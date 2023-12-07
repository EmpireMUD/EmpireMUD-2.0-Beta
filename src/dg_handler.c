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
	
	cancel_dg_owner_purged_tracker(trig);
	
	if (GET_TRIG_WAIT(trig)) {
		dg_event_cancel(GET_TRIG_WAIT(trig), cancel_wait_event);
	}
	
	if (trig->name && (!proto || trig->name != proto->name)) {
		free(trig->name);
	}
	if (trig->arglist && (!proto || trig->arglist != proto->arglist)) {
		free(trig->arglist);
	}
	if (trig->cmdlist && (!proto || trig->cmdlist != proto->cmdlist)) {
		LL_FOREACH_SAFE(trig->cmdlist, cmd, next_cmd) {
			if (cmd->cmd) {
				free(cmd->cmd);
			}
			free(cmd);
		}
	}
	
	free_varlist(trig->var_list);
	free(trig);
}


/**
* Adds the trigger to the trigger_list and random_triggers list, as needed.
*
* @param trig_data *trig The trigger to add.
*/
void add_trigger_to_global_lists(trig_data *trig) {
	if (trig && !trig->in_world_list) {
		DL_PREPEND2(trigger_list, trig, prev_in_world, next_in_world);
		trig->in_world_list = TRUE;
	}
	if (trig && !trig->in_random_list && TRIG_IS_RANDOM(trig)) {
		// add to end
		DL_APPEND2(random_triggers, trig, prev_in_random_triggers, next_in_random_triggers);
		trig->in_random_list = TRUE;
	}
}


/**
* Removes the trigger from the trigger_list and random_triggers, if present.
*
* @param trig_data *trig The trigger to remove.
* @param bool random_only If TRUE, only removes it from the random trigger list. It remains in the main list, if present.
*/
void remove_trigger_from_global_lists(trig_data *trig, bool random_only) {
	if (trig && trig->in_world_list && !random_only) {
		DL_DELETE2(trigger_list, trig, prev_in_world, next_in_world);
		trig->in_world_list = FALSE;
	}
	if (trig && trig->in_random_list) {
		DL_DELETE2(random_triggers, trig, prev_in_random_triggers, next_in_random_triggers);
		trig->in_random_list = FALSE;
	}
}


/* remove a single trigger from a mob/obj/room */
void extract_trigger(trig_data *trig) {
	// prevents a bug when something has 2 random triggers and is purged by the first one -kh
	if (trig == stc_next_random_trig) {
		stc_next_random_trig = trig->next_in_random_triggers;
	}
	
	if (GET_TRIG_WAIT(trig)) {
		dg_event_cancel(GET_TRIG_WAIT(trig), cancel_wait_event);
		GET_TRIG_WAIT(trig) = NULL;
	}
	
	remove_trigger_from_global_lists(trig, FALSE);
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
			break;
		}
		default: {
			log("SYSERR: Invalid type called for extract_script()");
			return;
		}
	}

	#if 0 /* debugging */
	{
		char_data *i;
		obj_data *j;
		room_data *k, *next_k;
		vehicle_data *v;
		
		if (sc) {
			DL_FOREACH(character_list, i) {
				assert(sc != SCRIPT(i));
			}
			
			DL_FOREACH(object_list, j) {
				assert(sc != SCRIPT(j));
			}
			
			DL_FOREACH(vehicle_list, v) {
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
* This is only (and always) called when triggers are removed so, as a courtesy,
* it also triggers a world save even if it doesn't extract the script data,
* to save a large amount of code ATFO to save when triggers are removed. -pc
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
			request_char_save_in_world(mob);
			break;
		}
		case OBJ_TRIGGER: {
			obj_data *obj = (obj_data*)go;
			if (SCRIPT(obj) && !TRIGGERS(SCRIPT(obj)) && !SCRIPT(obj)->global_vars) {
				extract_script(obj, OBJ_TRIGGER);
			}
			request_obj_save_in_world(obj);
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
			request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
			break;
		}
		case VEH_TRIGGER: {
			vehicle_data *veh = (vehicle_data*)go;
			if (SCRIPT(veh) && !TRIGGERS(SCRIPT(veh)) && !SCRIPT(veh)->global_vars) {
				extract_script(veh, VEH_TRIGGER);
			}
			request_vehicle_save_in_world(veh);
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
	struct trig_proto_list *el, *iter, *list = NULL;
	
	LL_FOREACH(from, iter) {
		CREATE(el, struct trig_proto_list, 1);
		*el = *iter;
		LL_APPEND(list, el);
	}
	
	return list;
}


/**
* Updates SCRIPT_TYPES(sc) for all triggers attached to it.
*
* @param struct script_data *sc The script data to update.
*/
void update_script_types(struct script_data *sc) {
	trig_data *trig;
	
	if (sc) {
		SCRIPT_TYPES(sc) = NOBITS;
		LL_FOREACH(TRIGGERS(sc), trig) {
			SCRIPT_TYPES(sc) |= GET_TRIG_TYPE(trig);
		}
	}
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


 //////////////////////////////////////////////////////////////////////////////
//// UID LOOKUP TABLE ////////////////////////////////////////////////////////

// this system handles lookups by id for script objects
struct uid_lookup_table {
	int uid;
	int type;	// TYPE_ const for typesafety
	union {
		char_data *ch;	// TYPE_MOB (or player)
		obj_data *obj;	// TYPE_OBJ
		room_data *room;	// TYPE_ROOM
		vehicle_data *veh;	// TYPE_VEH
	} data;
	
	UT_hash_handle hh;
};

// main hash table
struct uid_lookup_table *master_uid_lookup_table = NULL;


/**
* Adds a person, place, or thing to the script lookup table.
*
* @param int uid The thing's id.
* @param void *ptr A pointer to the thing to add.
* @param int type TYPE_MOB, TYPE_VEH, etc. (matching ptr)
*/
void add_to_lookup_table(int uid, void *ptr, int type) {
	struct uid_lookup_table *find;
	
	HASH_FIND_INT(master_uid_lookup_table, &uid, find);
	if (find) {
		log ("Add_to_lookup failed. %d already there.", uid);
	}
	else {
		CREATE(find, struct uid_lookup_table, 1);
		find->uid = uid;
		find->type = type;
		HASH_ADD_INT(master_uid_lookup_table, uid, find);
		
		switch (type) {
			case TYPE_MOB: {
				find->data.ch = ptr;
				break;
			}
			case TYPE_OBJ: {
				find->data.obj = ptr;
				break;
			}
			case TYPE_ROOM: {
				find->data.room = ptr;
				break;
			}
			case TYPE_VEH: {
				find->data.veh = ptr;
				break;
			}
			default: {
				log ("Add_to_lookup called with invalid type %d for uid %d", type, uid);
				break;
			}
		}
	}
}


/**
* Find a character in the script lookup table.
*
* This incorporates former parts of find_char_by_uid_in_lookup_table()
*
* @param int uid The script id (or player idnum) to find in the lookup table.
* @return char_data* The found character, or NULL if not present.
*/
char_data *find_char(int uid) {
	struct uid_lookup_table *find;
	
	if (uid >= EMPIRE_ID_BASE && uid < OTHER_ID_BASE) {
		return NULL;	// shortcut: see note in dg_scripts.h
	}
	
	HASH_FIND_INT(master_uid_lookup_table, &uid, find);
	if (find && find->type == TYPE_MOB) {
		return find->data.ch;
	}
	
	#ifdef DEBUG_UID_LOOKUPS
		log("find_char : No character with number %d in lookup table", uid);
	#endif
	
	return NULL;	// all other cases
}


/**
* Looks up an empire by a DG Scripts UID. Does not use the lookup table because
* these can be found mathematically.
*
* @param int uid The UID.
* @return empire_data* The found empire, or NULL.
*/
empire_data *find_empire_by_uid(int uid) {
	if (uid < EMPIRE_ID_BASE || uid >= ROOM_ID_BASE) {
		// see note in dg_scripts.h
		return NULL;
	}
	
	return real_empire(uid - EMPIRE_ID_BASE);
}



/**
* Find an object in the script lookup table.
*
* This incorporates former parts of find_obj_by_uid_in_lookup_table()
*
* @param int uid The script id to find in the lookup table.
* @return obj_data* The found object, or NULL if not present.
*/
obj_data *find_obj(int uid) {
	struct uid_lookup_table *find;
	
	if (uid < OTHER_ID_BASE) {
		return NULL;	// shortcut: see note in dg_scripts.h
	}
	
	HASH_FIND_INT(master_uid_lookup_table, &uid, find);
	if (find && find->type == TYPE_OBJ) {
		return find->data.obj;
	}
	
	#ifdef DEBUG_UID_LOOKUPS
		log("find_obj : No object with number %d in lookup table", uid);
	#endif
	
	return NULL;	// all other cases
}


/**
* Finds a room from a script uid.
*
* @param int uid The script uid.
* @return room_data* The room, if any, or NULL if not.
*/
room_data *find_room(int uid) {
	uid -= ROOM_ID_BASE;
	if (uid >= 0)  {
    	return real_room(uid); 	// if any
    }
    
    return NULL;
}


/**
* Find a vehicle in the script lookup table.
*
* This incorporates former parts of find_vehicle_by_uid_in_lookup_table()
*
* @param int uid The script id to find in the lookup table.
* @return vehicle_data* The found vehicle, or NULL if not present.
*/
vehicle_data *find_vehicle(int uid) {
	struct uid_lookup_table *find;
	
	if (uid < OTHER_ID_BASE) {
		return NULL;	// shortcut: see note in dg_scripts.h
	}
	
	HASH_FIND_INT(master_uid_lookup_table, &uid, find);
	if (find && find->type == TYPE_VEH) {
		return find->data.veh;
	}
	
	#ifdef DEBUG_UID_LOOKUPS
		log("find_veh : No vehicle with number %d in lookup table", uid);
	#endif
	return NULL;	// all other cases
}


/**
* Removes a person, place, or thing from the script lookup table. This will
* log a warning if not found.
*
* @param int uid The id to remove.
*/
void remove_from_lookup_table(int uid) {
	struct uid_lookup_table *find;
	
	HASH_FIND_INT(master_uid_lookup_table, &uid, find);
	if (find) {
		HASH_DEL(master_uid_lookup_table, find);
		free(find);
	}
	else {
		log("remove_from_lookup. UID %d not found.", uid);
	}
}
