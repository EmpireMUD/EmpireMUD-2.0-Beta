/* ************************************************************************
*   File: ships.c                                         EmpireMUD 2.0b3 *
*  Usage: code related to boating                                         *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
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
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Ship Functions
*   Ship Commands
*/

// external vars
extern const char *dirs[];
extern const int rev_dir[];

// external funcs
extern int count_objs_in_room(room_data *room);


// currently only galleon can hold items that are !take
#define SHIP_CAN_HOLD_OBJ(ship, obj)  (FALSE && (CAN_WEAR(obj, ITEM_WEAR_TAKE)))	// || GET_SHIP_TYPE(ship) == SHIP_GALLEON))


 //////////////////////////////////////////////////////////////////////////////
//// SHIP FUNCTIONS //////////////////////////////////////////////////////////

/* Returns TRUE if it loads the object */
bool load_one_obj_to_boat(char_data *ch, obj_data *obj, room_data *to_room, obj_data *ship) {		
	int size;
	
	if (ROOM_BLD_FLAGGED(to_room, BLD_ITEM_LIMIT)) {
		size = (OBJ_FLAGGED(obj, OBJ_LARGE) ? 2 : 1);
		if ((size + count_objs_in_room(to_room)) > config_get_int("room_item_limit")) {
			act("$p won't fit.", FALSE, ch, obj, NULL, TO_CHAR);
			return FALSE;
		}
	}
	
	obj_to_room(obj, to_room);
	act("You load $p into $P.", FALSE, ch, obj, ship, TO_CHAR);
	return TRUE;
}


/* Returns 1 if totally done, 0 if the room fills, and -1 if nothing could be loaded */
int load_all_objs_to_boat(char_data *ch, room_data *from, room_data *to, obj_data *ship) {
	obj_data *o, *next_o;
	bool done = TRUE, any = FALSE;
	int size;

	for (o = ROOM_CONTENTS(from); o; o = next_o) {
		next_o = o->next_content;

		/* We can only load a !take item onto a Galleon (catapult, etc) */
		if (!SHIP_CAN_HOLD_OBJ(ship, o)) {
			continue;
		}
		
		if (ROOM_BLD_FLAGGED(to, BLD_ITEM_LIMIT)) {
			size = (OBJ_FLAGGED(o, OBJ_LARGE) ? 2 : 1);
			if ((size + count_objs_in_room(to)) > config_get_int("room_item_limit")) {
				if (size > 1) {
					// MIGHT fit a smaller item
					continue;
				}
				else {
					// totally full
					break;
				}
			}
		}

		obj_to_room(o, to);
		any = TRUE;
	}

	/*
	 * done will still be TRUE if we never failed to load an item
	 * any will be TRUE if any item was loaded
	 * if not, this puppy is full!
	 */

	if (done)
		return 1;
	else if (any)
		return 0;
	else
		return -1;
}


/*
 * Returns:
 *  -2: nothing could be unloaded, docks are full
 *  -1: some stuff was unloaded, but docks were full
 *   0: room was empty
 *   1: room finished
 */
int perform_unload_boat(char_data *ch, room_data *from, room_data *to, obj_data *ship) {
	obj_data *o, *next_o;
	bool done = TRUE, any = FALSE;

	for (o = ROOM_CONTENTS(from); o; o = next_o) {
		next_o = o->next_content;

		/* It's not necessary to check for ITEM_WEAR_TAKE.. if it got here we'll take it out */

		obj_to_room(o, to);
		any = TRUE;
	}

	if (!done && !any)
		return -2;
	else if (!done && any)
		return -1;
	else if (done && !any)
		return 0;
	else
		return 1;
}


 //////////////////////////////////////////////////////////////////////////////
//// SHIP COMMANDS ///////////////////////////////////////////////////////////

ACMD(do_load_boat) {
	/*
	obj_data *ship, *to_load = NULL;
	room_data *room, *next_room, *ship_room;
	int val = -1;
	bool done = FALSE;

	two_arguments(argument, arg, buf);

	if (BUILDING_VNUM(IN_ROOM(ch)) != BUILDING_DOCKS)
		msg_to_char(ch, "You can only load a ship at the dock.\r\n");
	else if (!IS_COMPLETE(IN_ROOM(ch)))
		msg_to_char(ch, "The docks are incomplete.\r\n");
	else if (!*arg)
		msg_to_char(ch, "What ship would you like to load?\r\n");
	else if (!(ship = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)))))
		msg_to_char(ch, "There's no ship by that name here.\r\n");
	else if (GET_OBJ_TYPE(ship) != ITEM_SHIP)
		msg_to_char(ch, "That's not a ship!\r\n");
	else if (*buf && !(to_load = get_obj_in_list_vis(ch, buf, ch->carrying)) && !(to_load = get_obj_in_list_vis(ch, buf, ROOM_CONTENTS(IN_ROOM(ch)))))
		msg_to_char(ch, "You don't seem to have a %s to load.\r\n", buf);
	else if (to_load && GET_OBJ_TYPE(to_load) == ITEM_SHIP)
		msg_to_char(ch, "You can't load a ship onto a ship!\r\n");
	else if (to_load && !SHIP_CAN_HOLD_OBJ(ship, to_load))
		msg_to_char(ch, "You can't load that onto this ship!\r\n");
	else if (!(ship_room = real_room(GET_SHIP_MAIN_ROOM(ship)))) {
		msg_to_char(ch, "You can't seem to load this ship.\r\n");
	}
	else {
		for (room = interior_room_list; room; room = next_room) {
			next_room = room->next_interior;
			
			if (done) {
				break;
			}

			if (HOME_ROOM(room) != ship_room)
				continue;
			if (BUILDING_VNUM(room) != RTYPE_B_STORAGE && BUILDING_VNUM(room) != RTYPE_B_ONDECK)
				continue;

			if (to_load) {
				if (load_one_obj_to_boat(ch, to_load, room, ship))
					return;
			}
			else {
				if ((val = load_all_objs_to_boat(ch, IN_ROOM(ch), room, ship)) == 1)
					done = TRUE;
				if (val != -1) {
					act("You load some cargo into $p.", FALSE, ch, ship, 0, TO_CHAR);
					act("$n loads some cargo into $p.", FALSE, ch, ship, 0, TO_ROOM);
				}
				// else nothing was loaded at all to this room
			}
		}
		if (val == -1 && !done) {
			msg_to_char(ch, "There was nothing to load.\r\n");
		}
		else if (!to_load && !done) {
			act("$p is full.", FALSE, ch, ship, 0, TO_CHAR);
		}
	}
	*/
}


ACMD(do_unload_boat) {
	/*
	obj_data *ship;
	room_data *room, *next_room, *ship_room;
	int val = 0;
	bool more = FALSE;

	one_argument(argument, arg);

	if (BUILDING_VNUM(IN_ROOM(ch)) != BUILDING_DOCKS)
		msg_to_char(ch, "You can only unload a ship at the dock.\r\n");
	else if (!IS_COMPLETE(IN_ROOM(ch)))
		msg_to_char(ch, "The docks are incomplete.\r\n");
	else if (!*arg)
		msg_to_char(ch, "What ship would you like to load?\r\n");
	else if (!(ship = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)))))
		msg_to_char(ch, "There's no ship by that name here.\r\n");
	else if (GET_OBJ_TYPE(ship) != ITEM_SHIP)
		msg_to_char(ch, "That's not a ship!\r\n");
	else if (!(ship_room = real_room(GET_SHIP_MAIN_ROOM(ship)))) {
		msg_to_char(ch, "You can't seem to unload the ship.\r\n");
	}
	else {
		for (room = interior_room_list; room; room = next_room) {
			next_room = room->next_interior;
			
			if (HOME_ROOM(room) != ship_room)
				continue;
			if (BUILDING_VNUM(room) != RTYPE_B_STORAGE && BUILDING_VNUM(room) != RTYPE_B_ONDECK)
				continue;

			if ((val = perform_unload_boat(ch, room, IN_ROOM(ch), ship)) < 0)
				more = TRUE;
			if (val != 0 && val != -2) {
				act("You unload some cargo from $p.", FALSE, ch, ship, 0, TO_CHAR);
				act("$n unloads some cargo from $p.", FALSE, ch, ship, 0, TO_ROOM);
			}
		}
		if (more) {
			msg_to_char(ch, "You can't unload anything else, the docks are full!\r\n");
		}
		else if (val == 0) {
			msg_to_char(ch, "There was nothing to unload from that ship.\r\n");
		}
	}
	*/
}
