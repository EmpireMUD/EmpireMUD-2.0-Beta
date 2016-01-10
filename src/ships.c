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

 //////////////////////////////////////////////////////////////////////////////
//// SHIP FUNCTIONS //////////////////////////////////////////////////////////

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
