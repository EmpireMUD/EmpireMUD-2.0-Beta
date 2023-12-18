/* ************************************************************************
*   File: act.god.c                                       EmpireMUD 2.0b5 *
*  Usage: Player-level god commands                                       *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "vnums.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "dg_scripts.h"

/**
* Contents:
*   Helpers
*   Commands
*/

 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Actual sacrifice of an item to a god.
*
* @param char_data *ch The person sacrificing something.
* @param char_data *god The got to sacrifice to.
* @param obj_data *obj The item to sacrifice.
* @param bool message If TRUE, sends messages.
* @return int The number of items sacrificed.
*/
static int perform_sacrifice(char_data *ch, char_data *god, obj_data *obj, bool message) {
	bool any_patron = FALSE;
	double bonus = 1.0;
	vehicle_data *veh;
	int num = 1;
	
	if (IS_STOLEN(obj)) {
		act("$p: You can't sacrifice stolen items!", FALSE, ch, obj, NULL, TO_CHAR);
		return 0;
	}
	if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
		act("$p: You can't sacrifice items with (keep).", FALSE, ch, obj, NULL, TO_CHAR);
		return 0;
	}
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !IS_NPC(ch) && !IS_IMMORTAL(ch)) {
		act("$p: You can't sacrifice quest items.", FALSE, ch, obj, NULL, TO_CHAR);
		return 0;
	}
	if (!drop_otrigger(obj, ch, DROP_TRIG_SACRIFICE) || !drop_wtrigger(obj, ch, DROP_TRIG_SACRIFICE)) {
		return 0;
	}
	
	/* Determine monument bonus */
	if (ROOM_PATRON(IN_ROOM(ch)) == GET_IDNUM(god)) {
		any_patron = TRUE;
	}
	else {	// look for a vehicle
		DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
			if (VEH_IS_COMPLETE(veh) && VEH_HEALTH(veh) > 0 && VEH_PATRON(veh) == GET_IDNUM(god)) {
				any_patron = TRUE;
				break;
			}
		}
	}
	
	if (any_patron) {
		bonus = 1.50;
	}

	// Oops, it was created -- no bonus, or else it could be used to increase one's own resources
	if (OBJ_FLAGGED(obj, OBJ_CREATED))
		bonus = 1;

	GET_RESOURCE(god, GET_OBJ_MATERIAL(obj)) += (int)((1 + rate_item(obj)) * bonus);

	if (message) {
		*buf2 = '\0';
		if (ch == god) {
			sprintf(buf, "You sacrifice $p to yourself!");
			sprintf(buf1, "$n sacrifices $p to $mself!");
		}
		else {
			sprintf(buf, "You sacrifice $p to %s!", PERS(god, god, 1));
			sprintf(buf1, "$n sacrifices $p to %s!", PERS(god, god, 1));
			sprintf(buf2, "$n sacrifices $p to you!");
		}
		act(buf, TRUE, ch, obj, god, TO_CHAR | TO_QUEUE);
		act(buf1, TRUE, ch, obj, god, TO_NOTVICT | TO_QUEUE);
		if (IN_ROOM(ch) == IN_ROOM(god) && ch != god)
			act(buf2, TRUE, ch, obj, god, TO_VICT | TO_QUEUE);
	}
	while (obj->contains) {
		num += perform_sacrifice(ch, god, obj->contains, FALSE);
	}
	extract_obj(obj);

	return num;
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////


ACMD(do_create) {
	char *name;
	obj_data *proto, *obj = NULL;
	obj_data *obj_iter, *next_obj;
	int cost, iter, mat, num = 1, count = 0, obj_ok = 0;
	bool imm_access = (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_LOAD));
	bool low_res = FALSE;

	// create [number] <name list>
	name = one_argument(argument, arg);

	if (isdigit(*arg)) {
		num = atoi(arg);
		if (num < 1) {
			msg_to_char(ch, "You need to create at LEAST one.\r\n");
			return;
		}
	}
	else {
		// no number; use full argument
		name = argument;
	}

	// process name?
	skip_spaces(&name);
	if (!*name) {
		msg_to_char(ch, "What would you like to create?\r\n");
		return;
	}

	// look it up
	proto = NULL;
	HASH_ITER(hh, object_table, obj_iter, next_obj) {
		if (OBJ_FLAGGED(obj_iter, OBJ_CREATABLE) && multi_isname(name, GET_OBJ_KEYWORDS(obj_iter))) {
			proto = obj_iter;
			break;
		}
	}

	if (!proto) {
		msg_to_char(ch, "You can't create that.\r\n");
		return;
	}
	
	// safety limit
	if (num > 1000) {
		msg_to_char(ch, "You can't create more than 1000 at once.\r\n");
		return;
	}
	
	// setup
	count = 0;
	cost = rate_item(proto) + 1;
	mat = GET_OBJ_MATERIAL(proto);

	// make them!
	for (iter = 0; iter < num && !low_res; ++iter) {
		// imms have no need to have the resources (just gods)
		if (GET_RESOURCE(ch, mat) >= cost || imm_access) {
			++count;
			GET_RESOURCE(ch, mat) = MAX(0, GET_RESOURCE(ch, mat) - cost);
			obj = read_object(GET_OBJ_VNUM(proto), TRUE);
			SET_BIT(GET_OBJ_EXTRA(obj), OBJ_CREATED);
			
			if (!CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
				obj_to_room(obj, IN_ROOM(ch));
			}
			else {
				obj_to_char(obj, ch);
			}
			
			scale_item_to_level(obj, 1);	// minimum level
			obj_ok = load_otrigger(obj);
			if (obj_ok && obj->carried_by) {
				get_otrigger(obj, obj->carried_by, FALSE);
			}
		}
		else {
			low_res = TRUE;
		}
	}

	if (count == 0) {
		msg_to_char(ch, "You fail to create it.\r\n");
		if (obj_ok && obj) {
			extract_obj(obj);
		}
	}
	else if (!obj_ok) {
		// probably self-purged?
		msg_to_char(ch, "You create it.\r\n");
	}
	else if (count == 1) {
		act("You create $p!", FALSE, ch, obj, 0, TO_CHAR);
		act("$n creates $p from thin air!", TRUE, ch, obj, 0, TO_ROOM);
	}
	else {
		sprintf(buf, "You create $p (x%d)!", count);
		sprintf(buf1, "$n creates $p (x%d) from thin air!", count);
		act(buf, FALSE, ch, obj, 0, TO_CHAR);
		act(buf1, TRUE, ch, obj, 0, TO_ROOM);
	}

	if (low_res) {
		msg_to_char(ch, "You have insufficient resources to create more.\r\n");
	}
}


ACMD(do_sacrifice) {
	obj_data *obj, *next_obj;
	char_data *god = NULL;
	int dotmode, number;
	bool file = FALSE, any = FALSE;

	two_arguments(argument, arg, buf);

	if (!*arg || !*buf) {
		msg_to_char(ch, "What do you wish to sacrifice, and to whom?\r\n");
		return;
	}
	
	if (!(god = find_or_load_player(buf, &file))) {
		send_to_char("There is no such god.\r\n", ch);
		return;
	}
	
	if (!IS_GOD(god) && !IS_IMMORTAL(god)) {
		msg_to_char(ch, "You can only sacrifice to gods.\r\n");
		if (file) {
			free_char(god);
		}
		return;
	}

	dotmode = find_all_dots(arg);

	if (dotmode == FIND_ALL) {
		msg_to_char(ch, "You can't just sacrifice everything, that's how accidents happen.\r\n");
		/*
		if (!ch->carrying && (!ROOM_CONTENTS(IN_ROOM(ch)) || !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)))
			msg_to_char(ch, "You don't seem to have anything to sacrifice.\r\n");
		else {
			DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
				if (OBJ_FLAGGED(obj, OBJ_KEEP) || !bind_ok(obj, ch)) {
					continue;
				}
				
				perform_sacrifice(ch, god, obj, TRUE);
			}
			DL_FOREACH_SAFE2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_obj, next_content) {
				if (OBJ_FLAGGED(obj, OBJ_KEEP) || !bind_ok(obj, ch)) {
					continue;
				}

				if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED))
					continue;

				if (!CAN_WEAR(obj, ITEM_WEAR_TAKE))
					continue;

				perform_sacrifice(ch, god, obj, TRUE);
			}
		}
		*/
	}
	else if (dotmode == FIND_ALLDOT) {
		if (!*arg) {
			msg_to_char(ch, "What do you want to sacrifice all of?\r\n");
			if (god && file) {
				free_char(god);
			}
			return;
		}
		if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
			if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || !(obj = get_obj_in_list_vis(ch, arg, NULL, ROOM_CONTENTS(IN_ROOM(ch))))) {
				msg_to_char(ch, "You don't seem to have any %ss to sacrifice.\r\n", arg);
				if (god && file) {
					free_char(god);
				}
				return;
			}
		}

		while (obj) {
			next_obj = get_obj_in_list_vis(ch, arg, NULL, obj->next_content);
			if (!next_obj && can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) && !IN_ROOM(obj))
				next_obj = get_obj_in_list_vis(ch, arg, NULL, ROOM_CONTENTS(IN_ROOM(ch)));
			
			if (CAN_WEAR(obj, ITEM_WEAR_TAKE) && !OBJ_FLAGGED(obj, OBJ_KEEP) && bind_ok(obj, ch)) {
				perform_sacrifice(ch, god, obj, TRUE);
				any = TRUE;
			}
			
			obj = next_obj;
		}
		
		if (!any) {
			msg_to_char(ch, "You can't sacrifice anything like that.\r\n");
		}
	}
	else {
		char *argptr = arg;
		number = get_number(&argptr);
		
		if (!(obj = get_obj_in_list_vis(ch, argptr, &number, ch->carrying)) && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || !(obj = get_obj_in_list_vis(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch))))))
			msg_to_char(ch, "You don't seem to have any %ss to sacrifice.\r\n", arg);
		else if (!CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
			msg_to_char(ch, "You can't sacrifice an item that you can't pick up!\r\n");
		}
		else if (!bind_ok(obj, ch)) {
			msg_to_char(ch, "You can't sacrifice an item that's bound to someone else.\r\n");
		}
		else {
			perform_sacrifice(ch, god, obj, TRUE);
		}
	}
	
	if (god && file) {
		store_loaded_char(god);
		file = FALSE;
	}
}
