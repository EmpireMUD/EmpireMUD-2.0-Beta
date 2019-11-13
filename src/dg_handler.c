/* ************************************************************************
*   File: dg_handler.c                                    EmpireMUD 2.0b5 *
*  Usage: handler.c-like functionality for DG Scripts                     *
*                                                                         *
*  DG Scripts code had no header or author info in this file              *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "dg_scripts.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "dg_event.h"

// external funcs
EVENT_CANCEL_FUNC(cancel_wait_event);

// locals
void actually_free_trigger(trig_data *trig);


/**
* Ensures the "attach_to" thing (also called "go" in some scripting code)
* has a SCRIPT() var, and sets up data that will be needed later.
*
* @param void *attach_to Any char/obj/room/vehicle that could have SCRIPT().
* @param int type _TRIGGER consts such as MOB_TRIGGER, corresponding to 'attach_to'.
* @return struct script_data* A pointer to the script data (which is also attached to 'attach_to'), or NULL if it failed.
*/
struct script_data *create_script_data(void *attach_to, int type) {
	struct script_data *scr = NULL;
	
	// x_TRIGGER: attach types
	switch (type) {
		case MOB_TRIGGER: {
			char_data *mob = (char_data*)attach_to;
			if (!(scr = SCRIPT(mob))) {
				CREATE(scr, struct script_data, 1);
				SCRIPT(mob) = scr;
			}
			break;
		}
		case OBJ_TRIGGER: {
			obj_data *obj = (obj_data*)attach_to;
			if (!(scr = SCRIPT(obj))) {
				CREATE(scr, struct script_data, 1);
				SCRIPT(obj) = scr;
			}
			break;
		}
		case WLD_TRIGGER:
		case RMT_TRIGGER:
		case ADV_TRIGGER:
		case BLD_TRIGGER: {
			room_data *room = (room_data*)attach_to;
			if (!(scr = SCRIPT(room))) {
				CREATE(scr, struct script_data, 1);
				SCRIPT(room) = scr;
			}
			type = WLD_TRIGGER;	// override other types
			break;
		}
		case VEH_TRIGGER: {
			vehicle_data *veh = (vehicle_data*)attach_to;
			if (!(scr = SCRIPT(veh))) {
				CREATE(scr, struct script_data, 1);
				SCRIPT(veh) = scr;
			}
			break;
		}
		case EMP_TRIGGER: {
			empire_data *emp = (empire_data*)attach_to;
			if (!(scr = SCRIPT(emp))) {
				CREATE(scr, struct script_data, 1);
				SCRIPT(emp) = scr;
			}
			break;
		}
		// no default: just won't set scr
	}
	
	// attach data
	if (scr) {
		scr->attached_to = attach_to;
		scr->attached_type = type;
	}
	
	return scr;	// or NULL
}


/**
* Triggers are temporarily stored in a list and then freed after everything has
* finished running, so that their data is still available if they e.g. detach
* themselves from something -- they can't free mid-execution. This fixes a bug
* from stock DG Scripts where a trigger could free itself mid-execution and
* still try to reference freed memory. -pc 5/29/2017
*/
void free_freeable_triggers(void) {
	trig_data *trig, *next_trig;
	
	LL_FOREACH_SAFE2(free_trigger_list, trig, next_trig, next_to_free) {
		actually_free_trigger(trig);
	}
	free_trigger_list = NULL;
}


/* release memory allocated for a variable list */
void free_varlist(struct trig_var_data *vd) {
	void free_var_el(struct trig_var_data *var);
	struct trig_var_data *var, *next_var;
	LL_FOREACH_SAFE(vd, var, next_var) {
		free_var_el(var);
	}
}


/**
* Prepares to free a trigger. The actual freeing is done on a short delay to
* ensure no script is running at the time.
*
* @param trig_data *trig The trigger to free.
*/
void free_trigger(trig_data *trig) {
	if (GET_TRIG_WAIT(trig)) {
		dg_event_cancel(GET_TRIG_WAIT(trig), cancel_wait_event);
		GET_TRIG_WAIT(trig) = NULL;
	}
	
	LL_PREPEND2(free_trigger_list, trig, next_to_free);
}


/* 
* Performs the actual freeing of a trigger, which is always done on a delay so
* that they can't be extracted while their own script is running. -pc 5/29/2017
*/
void actually_free_trigger(trig_data *trig) {
	trig_data *proto = real_trigger(GET_TRIG_VNUM(trig));
	struct cmdlist_element *cmd, *next_cmd;
	
	if (GET_TRIG_WAIT(trig)) {
		dg_event_cancel(GET_TRIG_WAIT(trig), cancel_wait_event);
	}
	
	if (trig->name && (!proto || trig->name != proto->name)) {
		free(trig->name);
	}
	if (trig->arglist && (!proto || trig->arglist != proto->arglist)) {
		free(trig->arglist);
	}
	if (trig->var_list && (!proto || trig->var_list != proto->var_list)) {
		free_varlist(trig->var_list);
	}
	if (trig->cmdlist && (!proto || trig->cmdlist != proto->cmdlist)) {
		LL_FOREACH_SAFE(trig->cmdlist, cmd, next_cmd) {
			if (cmd->cmd) {
				free(cmd->cmd);
			}
			free(cmd);
		}
	}
	
	free(trig);
}


/* remove a single trigger from a mob/obj/room */
void extract_trigger(trig_data *trig) {
	if (GET_TRIG_WAIT(trig)) {
		dg_event_cancel(GET_TRIG_WAIT(trig), cancel_wait_event);
		GET_TRIG_WAIT(trig) = NULL;
	}

	/* walk the trigger list and remove this one */
	LL_DELETE2(trigger_list, trig, next_in_world);
	
	// global trig?
	if (TRIG_IS_RANDOM(trig)) {
		DL_DELETE2(random_triggers, trig, prev_in_random_triggers, next_in_random_triggers);
	}

	free_trigger(trig);
}


/* remove all triggers from a mob/obj/room */
void extract_script(void *thing, int type) {
	struct script_data *sc = NULL;
	trig_data *trig, *next_trig;
	vehicle_data *veh;
	char_data *mob;
	obj_data *obj;
	room_data *room;

	switch (type) {
		case MOB_TRIGGER:
			mob = (char_data*)thing;
			sc = SCRIPT(mob);
			SCRIPT(mob) = NULL;
			break;
		case OBJ_TRIGGER:
			obj = (obj_data*)thing;
			sc = SCRIPT(obj);
			SCRIPT(obj) = NULL;
			break;
		case WLD_TRIGGER:
			room = (room_data*)thing;
			sc = SCRIPT(room);
			SCRIPT(room) = NULL;
			break;
		case VEH_TRIGGER: {
			veh = (vehicle_data*)thing;
			sc = SCRIPT(veh);
			SCRIPT(veh) = NULL;
			break;
		}
		case EMP_TRIGGER: {
			empire_data *emp = (empire_data*)thing;
			sc = SCRIPT(emp);
			SCRIPT(emp) = NULL;
		}
		default: {
			log("SYSERR: Invalid type called for extract_script()");
			return;
		}
	}

	#if 0 /* debugging */
	{
		char_data *i = character_list;
		obj_data *j = object_list;
		room_data *k, *next_k;
		vehicle_data *v;
		
		if (sc) {
			for ( ; i ; i = i->next)
				assert(sc != SCRIPT(i));

			for ( ; j ; j = j->next)
				assert(sc != SCRIPT(j));
			
			LL_FOREACH(vehicle_list, v) {
				assert(sc != SCRIPT(v));
			}
			
			HASH_ITER(hh, world_table, k, next_k) {
				assert(sc != SCRIPT(k));
			}
		}
	}
	#endif
	for (trig = TRIGGERS(sc); trig; trig = next_trig) {
		next_trig = trig->next;
		extract_trigger(trig);
	}
	TRIGGERS(sc) = NULL;

	/* Thanks to James Long for tracking down this memory leak */
	free_varlist(sc->global_vars);

	free(sc);
}


/* erase the script memory of a mob */
void extract_script_mem(struct script_memory *sc) {
	struct script_memory *next;
	while (sc) {
		next = sc->next;
		if (sc->cmd)
			free(sc->cmd);
		free(sc);
		sc = next;
	}
}


/**
* Checks if all script data is empty and it's safe to free the script data
* from the 'go'.
*
* @param void *go The mob, obj, etc to check.
* @param int type The corresponding *_TRIGGER type for 'go' (e.g. MOB_TRIGGER).
*/
void check_extract_script(void *go, int type) {
	// x_TRIGGER: attach types
	switch (type) {
		case MOB_TRIGGER: {
			char_data *mob = (char_data*)go;
			if (SCRIPT(mob) && !TRIGGERS(SCRIPT(mob)) && !SCRIPT(mob)->global_vars) {
				extract_script(mob, MOB_TRIGGER);
			}
			break;
		}
		case OBJ_TRIGGER: {
			obj_data *obj = (obj_data*)go;
			if (SCRIPT(obj) && !TRIGGERS(SCRIPT(obj)) && !SCRIPT(obj)->global_vars) {
				extract_script(obj, OBJ_TRIGGER);
			}
			break;
		}
		case WLD_TRIGGER:
		case RMT_TRIGGER:
		case ADV_TRIGGER:
		case BLD_TRIGGER: {
			room_data *room = (room_data*)go;
			if (SCRIPT(room) && !TRIGGERS(SCRIPT(room)) && !SCRIPT(room)->global_vars) {
				extract_script(room, WLD_TRIGGER);
			}
			type = WLD_TRIGGER;	// override other types
			break;
		}
		case VEH_TRIGGER: {
			vehicle_data *veh = (vehicle_data*)go;
			if (SCRIPT(veh) && !TRIGGERS(SCRIPT(veh)) && !SCRIPT(veh)->global_vars) {
				extract_script(veh, VEH_TRIGGER);
			}
			break;
		}
		case EMP_TRIGGER: {
			empire_data *emp = (empire_data*)go;
			if (SCRIPT(emp) && !TRIGGERS(SCRIPT(emp)) && !SCRIPT(emp)->global_vars) {
				extract_script(emp, EMP_TRIGGER);
			}
			break;
		}
	}
}


/**
* Removes all the triggers from a given thing. This will also call
* check_extract_script() to see if more memory can be freed too.
*
* @param void *thing The thing to remove from (mob, obj, room, veh).
* @param int type The *_TRIGGER type that corresponds to that thing.
*/
void remove_all_triggers(void *thing, int type) {
	struct script_data *sc = NULL;
	trig_data *trig, *next_trig;
	
	// x_TRIGGER: attach types
	switch (type) {
		case MOB_TRIGGER: {
			char_data *mob = (char_data*)thing;
			sc = SCRIPT(mob);
			break;
		}
		case OBJ_TRIGGER: {
			obj_data *obj = (obj_data*)thing;
			sc = SCRIPT(obj);
			break;
		}
		case WLD_TRIGGER: {
			room_data *room = (room_data*)thing;
			sc = SCRIPT(room);
			break;
		}
		case VEH_TRIGGER: {
			vehicle_data *veh = (vehicle_data*)thing;
			sc = SCRIPT(veh);
			break;
		}
		case EMP_TRIGGER: {
			empire_data *emp = (empire_data*)thing;
			sc = SCRIPT(emp);
			break;
		}
		default: {
			log("SYSERR: Invalid type called for remove_all_triggers()");
			return;
		}
	}
	
	if (sc) {
		LL_FOREACH_SAFE(TRIGGERS(sc), trig, next_trig) {
			extract_trigger(trig);
		}
		TRIGGERS(sc) = NULL;
	
		// checks if there's more we can free
		check_extract_script(thing, type);
	}
}


/**
* Frees a whole list of proto scripts.
*
* @param struct trig_proto_list **list A pointer to list to free.
*/
void free_proto_scripts(struct trig_proto_list **list) {
	struct trig_proto_list *iter, *next_iter;
	LL_FOREACH_SAFE(*list, iter, next_iter) {
		free(iter);
	}
	*list = NULL;
}


/**
* Duplicates a list of proto scripts.
*
* @param struct trig_proto_list *from The list to copy.
* @return struct trig_proto_list* The copied list.
*/
struct trig_proto_list *copy_trig_protos(struct trig_proto_list *from) {
	struct trig_proto_list *el, *iter, *list = NULL, *end = NULL;
	
	LL_FOREACH(from, iter) {
		CREATE(el, struct trig_proto_list, 1);
		*el = *iter;
		el->next = NULL;
		
		if (end) {
			end->next = el;
		}
		else {
			list = el;
		}
		end = el;
	}
	
	return list;
}


// I don't believe this is used any more... it was required before the world was a hash table, when we had to copy the world
void update_wait_events(room_data *to, room_data *from) {
	trig_data *trig;

	if (!SCRIPT(from))
		return;

	for (trig = TRIGGERS(SCRIPT(from)); trig; trig = trig->next) {
		if (!GET_TRIG_WAIT(trig))
			continue;

		((struct wait_event_data *)GET_TRIG_WAIT(trig)->event_obj)->go = to;
	}
}
