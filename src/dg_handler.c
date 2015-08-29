/* ************************************************************************
*   File: dg_handler.c                                    EmpireMUD 2.0b2 *
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


/* release memory allocated for a variable list */
void free_varlist(struct trig_var_data *vd) {
	struct trig_var_data *i, *j;

	for (i = vd; i;) {
		j = i;
		i = i->next;
		if (j->name)
			free(j->name);
		if (j->value)
			free(j->value);
		free(j);
	}
}


/* 
* Return memory used by a trigger
* The command list is free'd when changed and when
* shutting down.
*/
void free_trigger(trig_data *trig) {
	if (trig->name) {
		free(trig->name);
		trig->name = NULL;
	}
	
	// TODO should this free the cmd list? Not sure.

	if (trig->arglist) {
		free(trig->arglist);
		trig->arglist = NULL;
	}
	if (trig->var_list) {
		free_varlist(trig->var_list);
		trig->var_list = NULL;
	}
	if (GET_TRIG_WAIT(trig))
		event_cancel(GET_TRIG_WAIT(trig));

	free(trig);
}


/* remove a single trigger from a mob/obj/room */
void extract_trigger(trig_data *trig) {
	trig_data *temp;

	if (GET_TRIG_WAIT(trig)) {
		event_cancel(GET_TRIG_WAIT(trig));
		GET_TRIG_WAIT(trig) = NULL;
	}

	/* walk the trigger list and remove this one */
	REMOVE_FROM_LIST(trig, trigger_list, next_in_world);

	free_trigger(trig);
}


/* remove all triggers from a mob/obj/room */
void extract_script(void *thing, int type) {
	struct script_data *sc = NULL;
	trig_data *trig, *next_trig;
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
		
		if (sc) {
			for ( ; i ; i = i->next)
				assert(sc != SCRIPT(i));

			for ( ; j ; j = j->next)
				assert(sc != SCRIPT(j));
			
			HASH_ITER(world_hh, world_table, k, next_k) {
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


void free_proto_script(void *thing, int type) {
	struct trig_proto_list *proto = NULL, *fproto;
	char_data *mob;
	obj_data *obj;
	room_data *room;
	room_template *rmt;
	adv_data *adv;

	switch (type) {
		case MOB_TRIGGER:
			mob = (char_data*)thing;
			proto = mob->proto_script;
			mob->proto_script = NULL;
			break;
		case OBJ_TRIGGER:
			obj = (obj_data*)thing;
			proto = obj->proto_script;
			obj->proto_script = NULL;
			break;
		case WLD_TRIGGER:
			room = (room_data*)thing;
			proto = room->proto_script;
			room->proto_script = NULL;
			break;
		case RMT_TRIGGER: {
			rmt = (room_template*)thing;
			proto = GET_RMT_SCRIPTS(rmt);
			GET_RMT_SCRIPTS(rmt) = NULL;
			break;
		}
		case ADV_TRIGGER: {
			adv = (adv_data*)thing;
			proto = GET_ADV_SCRIPTS(adv);
			GET_ADV_SCRIPTS(adv) = NULL;
			break;
		}
	}
	#if 0 /* debugging */
	{
		char_data *i = character_list;
		obj_data *j = object_list;
		room_data *k, *next_k;
		
		if (proto) {
			for ( ; i ; i = i->next)
				assert(proto != i->proto_script);

			for ( ; j ; j = j->next)
				assert(proto != j->proto_script);

			HASH_ITER(world_hh, world_table, k, next_k) {
				assert(proto != k->proto_script);
			}
		}
	}
	#endif
	while (proto) {
		fproto = proto;
		proto = proto->next;
		free(fproto);
	}
}


void copy_proto_script(void *source, void *dest, int type) {
	struct trig_proto_list *tp_src = NULL, *tp_dst = NULL;

	switch (type) {
		case MOB_TRIGGER:
			tp_src = ((char_data*)source)->proto_script;
			break;
		case OBJ_TRIGGER:
			tp_src = ((obj_data*)source)->proto_script;
			break;
		case WLD_TRIGGER:
			tp_src = ((room_data*)source)->proto_script;
			break;
		case RMT_TRIGGER: {
			tp_src = ((room_template*)source)->proto_script;
			break;
		}
		case ADV_TRIGGER: {
			tp_src = ((adv_data*)source)->proto_script;
			break;
		}
	}

	if (tp_src) {
		CREATE(tp_dst, struct trig_proto_list, 1);
		switch (type) {
			case MOB_TRIGGER:
				((char_data*)dest)->proto_script = tp_dst;
				break;
			case OBJ_TRIGGER:
				((obj_data*)dest)->proto_script = tp_dst;
				break;
			case WLD_TRIGGER:
				((room_data*)dest)->proto_script = tp_dst;
				break;
			case RMT_TRIGGER: {
				((room_template*)dest)->proto_script = tp_dst;
				break;
			}
			case ADV_TRIGGER: {
				((adv_data*)dest)->proto_script = tp_dst;
				break;
			}
		}

		while (tp_src) {
			tp_dst->vnum = tp_src->vnum;
			tp_src = tp_src->next;
			if (tp_src)
				CREATE(tp_dst->next, struct trig_proto_list, 1);
			tp_dst = tp_dst->next;
		}
	}
}


void delete_variables(const char *charname) {
	char filename[PATH_MAX];

	if (!get_filename((char*)charname, filename, SCRIPT_VARS_FILE))
		return;

	if (remove(filename) < 0 && errno != ENOENT)
		log("SYSERR: deleting variable file %s: %s", filename, strerror(errno));
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
