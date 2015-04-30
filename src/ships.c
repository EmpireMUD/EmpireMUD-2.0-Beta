/* ************************************************************************
*   File: ships.c                                         EmpireMUD 2.0b1 *
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
extern int last_action_rotation;

// external funcs
extern int count_objs_in_room(room_data *room);


// TODO The entire ship system needs a cleanup, overhaul, and features.

#define SHIP_PINNACE		0
#define SHIP_GALLEY			1
#define SHIP_BRIGANTINE		2
#define SHIP_ARGOSY			3
#define SHIP_GALLEON		4


struct ship_data_struct ship_data[] = {
	{ "pinnace", os_PINNACE, 0, ABIL_SHIPBUILDING, 60, 0 },
	{ "galley", os_GALLEY, 1, ABIL_SHIPBUILDING, 90, 0 },
	{ "brigantine", os_BRIGANTINE, 2, ABIL_SHIPBUILDING, 120, 0 },
	{ "argosy", os_ARGOSY, 3, ABIL_ADVANCED_SHIPS, 240, 1 },
	{ "galleon", os_GALLEON, 4, ABIL_ADVANCED_SHIPS, 480, 1 },

	{ "\n", NOTHING, 0, NO_ABIL, 0, 0 }
};

// how ships appear in the room
const char *boat_icon_ocean = "&0<&u44&0>";	// "&b~&0&u44&0&b~&0";
const char *boat_icon_river = "&0<&u44&0>";	// "&c~&0&u44&0&c~&0";

// currently only galleon can hold items that are !take
#define SHIP_CAN_HOLD_OBJ(ship, obj)  (IS_SHIP(ship) && (CAN_WEAR(obj, ITEM_WEAR_TAKE) || GET_SHIP_TYPE(ship) == SHIP_GALLEON))

#define CAN_SAIL_IN(room)  (ROOM_SECT_FLAGGED(room, SECTF_FRESH_WATER | SECTF_OCEAN) || IS_WATER_BUILDING(room))


 //////////////////////////////////////////////////////////////////////////////
//// SHIP FUNCTIONS //////////////////////////////////////////////////////////

/**
* This function looks for any ships in the room, then sets or un-sets the
* appropriate room flags.
*
* @param room_data *room Where to check
*/
void check_for_ships_present(room_data *room) {
	obj_data *obj;
	bool found = FALSE;
	
	for (obj = ROOM_CONTENTS(room); obj && !found; obj = obj->next_content) {
		if (IS_SHIP(obj)) {
			found = TRUE;
		}
	}
	
	if (found) {
		SET_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_SHIP_PRESENT);
		SET_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_SHIP_PRESENT);
	}
	else {
		REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_SHIP_PRESENT);
		REMOVE_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_SHIP_PRESENT);
	}
}


static obj_data *create_ship(obj_vnum vnum, empire_data *owner, room_data *to_room) {
	extern room_data *create_room();
	extern room_vnum find_free_vnum(void);

	obj_data *ship;
	room_data *main_room, *spare[8];
	int type, i;


	for (i = 0; i < 8; i++)
		spare[i] = NULL;

	ship = read_object(vnum);
	type = GET_SHIP_TYPE(ship);

	/* The main room */
	main_room = create_room();
	attach_building_to_room(building_proto(RTYPE_B_ONDECK), main_room);
	GET_OBJ_VAL(ship, VAL_SHIP_MAIN_ROOM) = GET_ROOM_VNUM(main_room);
	ROOM_OWNER(main_room) = owner;
	COMPLEX_DATA(main_room)->home_room = NULL;
	COMPLEX_DATA(main_room)->boat = ship;

	switch (type) {
		case SHIP_PINNACE:
			/* Helm - 0 */
			spare[0] = create_room();
			attach_building_to_room(building_proto(RTYPE_B_HELM), spare[0]);
			ROOM_OWNER(spare[0]) = owner;
			COMPLEX_DATA(spare[0])->home_room = main_room;

			create_exit(main_room, spare[0], AFT, TRUE);
			break;
		case SHIP_GALLEY:
			/* Helm - 0 */
			spare[0] = create_room();
			attach_building_to_room(building_proto(RTYPE_B_HELM), spare[0]);
			ROOM_OWNER(spare[0]) = owner;
			COMPLEX_DATA(spare[0])->home_room = main_room;

			/* On Deck - 1 */
			spare[1] = create_room();
			attach_building_to_room(building_proto(RTYPE_B_ONDECK), spare[1]);
			ROOM_OWNER(spare[1]) = owner;
			COMPLEX_DATA(spare[1])->home_room = main_room;

			create_exit(main_room, spare[0], AFT, TRUE);
			create_exit(main_room, spare[1], FORE, TRUE);
			break;
		case SHIP_BRIGANTINE:
			/* Helm - 0 */
			spare[0] = create_room();
			attach_building_to_room(building_proto(RTYPE_B_HELM), spare[0]);
			ROOM_OWNER(spare[0]) = owner;
			COMPLEX_DATA(spare[0])->home_room = main_room;

			/* Storage - 1 */
			spare[1] = create_room();
			attach_building_to_room(building_proto(RTYPE_B_STORAGE), spare[1]);
			ROOM_OWNER(spare[1]) = owner;
			COMPLEX_DATA(spare[1])->home_room = main_room;

			create_exit(main_room, spare[0], AFT, TRUE);
			create_exit(main_room, spare[1], DOWN, TRUE);
			break;
		case SHIP_ARGOSY:
			/* Helm - 0 */
			spare[0] = create_room();
			attach_building_to_room(building_proto(RTYPE_B_HELM), spare[0]);
			ROOM_OWNER(spare[0]) = owner;
			COMPLEX_DATA(spare[0])->home_room = main_room;

			/* On Deck - 1 */
			spare[1] = create_room();
			attach_building_to_room(building_proto(RTYPE_B_ONDECK), spare[1]);
			ROOM_OWNER(spare[1]) = owner;
			COMPLEX_DATA(spare[1])->home_room = main_room;

			/* Storage - 2-3 */
			for (i = 2; i <= 3; i++) {
				spare[i] = create_room();
				attach_building_to_room(building_proto(RTYPE_B_STORAGE), spare[i]);
				ROOM_OWNER(spare[i]) = owner;
				COMPLEX_DATA(spare[i])->home_room = main_room;
			}

			create_exit(main_room, spare[0], AFT, TRUE);
			create_exit(main_room, spare[1], FORE, TRUE);
			create_exit(spare[1], spare[2], DOWN, TRUE);
			create_exit(main_room, spare[3], DOWN, TRUE);
			break;
		case SHIP_GALLEON:
			/* Helm - 0 */
			spare[0] = create_room();
			attach_building_to_room(building_proto(RTYPE_B_HELM), spare[0]);
			ROOM_OWNER(spare[0]) = owner;
			COMPLEX_DATA(spare[0])->home_room = main_room;

			/* On Deck - 1 */
			spare[1] = create_room();
			attach_building_to_room(building_proto(RTYPE_B_ONDECK), spare[1]);
			ROOM_OWNER(spare[1]) = owner;
			COMPLEX_DATA(spare[1])->home_room = main_room;

			/* Below Deck - 2-3 */
			for (i = 2; i <= 3; i++) {
				spare[i] = create_room();
				attach_building_to_room(building_proto(RTYPE_B_BELOWDECK), spare[i]);
				ROOM_OWNER(spare[i]) = owner;
				COMPLEX_DATA(spare[i])->home_room = main_room;
			}

			/* Storage - 4-7 */
			for (i = 4; i < 8; i++) {
				spare[i] = create_room();
				attach_building_to_room(building_proto(RTYPE_B_STORAGE), spare[i]);
				ROOM_OWNER(spare[i]) = owner;
				COMPLEX_DATA(spare[i])->home_room = main_room;
			}

			create_exit(main_room, spare[0], AFT, TRUE);
			create_exit(main_room, spare[1], FORE, TRUE);
			create_exit(spare[1], spare[2], DOWN, TRUE);
			create_exit(spare[2], spare[3], AFT, TRUE);
			create_exit(spare[2], spare[4], STARBOARD, TRUE);
			create_exit(spare[2], spare[5], PORT, TRUE);
			create_exit(spare[3], spare[6], STARBOARD, TRUE);
			create_exit(spare[3], spare[7], PORT, TRUE);
			break;
		default:
			extract_obj(ship);
			return NULL;
	}
	
	obj_to_room(ship, to_room);
	load_otrigger(ship);
	return (ship);
}


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

		/* We can never load a ship */
		if (GET_OBJ_TYPE(o) == ITEM_SHIP)
			continue;

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

		/* We can never load a ship */
		if (GET_OBJ_TYPE(o) == ITEM_SHIP)
			continue;

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


void process_manufacturing(char_data *ch) {
	obj_data *ship;
	char_data *c;
	Resource iron[] = { {o_NAILS, 2}, END_RESOURCE_LIST };
	Resource logs[] = { {o_LUMBER, 1}, END_RESOURCE_LIST };
	int type = NOTHING, iter, total, count;
	
	// this finds the ship being worked on
	for (ship = ROOM_CONTENTS(IN_ROOM(ch)); ship; ship = ship->next_content) {
		if (IS_SHIP(ship) && GET_SHIP_RESOURCES_REMAINING(ship) > 0) {
			break;
		}
	}
	
	// this determines type
	for (iter = 0; *ship_data[iter].name != '\n'; ++iter) {
		if (GET_OBJ_VNUM(ship) == ship_data[iter].vnum) {
			type = iter;
			break;
		}
	}

	if (!ship) {
		msg_to_char(ch, "You try to work on a ship, but there isn't one here!\r\n");
		GET_ACTION(ch) = ACT_NONE;
		return;
	}
	
	total = 1 + (AFF_FLAGGED(ch, AFF_HASTE) ? 1 : 0) + (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES) ? 1 : 0);
	for (count = 0; count < total && GET_ACTION(ch) == ACT_MANUFACTURING && ship; ++count) {
		if ((GET_SHIP_RESOURCES_REMAINING(ship) % 2) != 0) {
			if (!has_resources(ch, iron, TRUE, TRUE))
				GET_ACTION(ch) = ACT_NONE;
			else {
				extract_resources(ch, iron, TRUE);
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "You nail the wood onto the structure.\r\n");
				}
				act("$n nails the wood onto the structure.", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
				GET_OBJ_VAL(ship, VAL_SHIP_RESOURCES_REMAINING) -= 1;
			}
		}
		else {
			if (!has_resources(ch, logs, TRUE, TRUE))
				GET_ACTION(ch) = ACT_NONE;
			else {
				extract_resources(ch, logs, TRUE);
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "You position some wood on the structure.\r\n");
				}
				act("$n positions the wood on the structure.", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
				GET_OBJ_VAL(ship, VAL_SHIP_RESOURCES_REMAINING) -= 1;
			}
		}
		if (GET_SHIP_RESOURCES_REMAINING(ship) == 0) {
			msg_to_char(ch, "You finish the construction!\r\n");
			act("$n finishes the construction!", FALSE, ch, 0, 0, TO_ROOM);
			for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
				if (!IS_NPC(c) && GET_ACTION(c) == ACT_MANUFACTURING) {
					GET_ACTION(c) = ACT_NONE;
				
					if (type != NOTHING && ship_data[type].ability != NO_ABIL) {
						gain_ability_exp(c, ship_data[type].ability, 100);
					}
				}
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// SHIP COMMANDS ///////////////////////////////////////////////////////////


ACMD(do_board) {
	struct follow_type *k;
	obj_data *ship;
	char_data *leading = NULL;
	room_data *was_in = IN_ROOM(ch), *ship_room;

	one_argument(argument, arg);

	if (IS_NPC(ch))
		return;
	else if (!*arg)
		msg_to_char(ch, "Board what?\r\n");
	else if (!(ship = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)))))
		msg_to_char(ch, "You don't see a %s here.\r\n", arg);
	else if (GET_OBJ_TYPE(ship) != ITEM_SHIP)
		msg_to_char(ch, "You can't board that!\r\n");
	else if (!(ship_room = real_room(GET_SHIP_MAIN_ROOM(ship)))) {
		msg_to_char(ch, "You can't board that ship!\r\n");
	}
	else if (!can_use_room(ch, ship_room, GUESTS_ALLOWED) && !IS_IMMORTAL(ch))
		msg_to_char(ch, "You don't have permission to board it.\r\n");
	else if (IS_RIDING(ch))
		msg_to_char(ch, "You can't board while riding.\r\n");
	else if (GET_LEADING(ch) && IN_ROOM(GET_LEADING(ch)) == IN_ROOM(ch) && (leading = GET_LEADING(ch)) && BUILDING_VNUM(IN_ROOM(ch)) != BUILDING_DOCKS)
		msg_to_char(ch, "You can't lead an animal on board from here.\r\n");
	else if (GET_SHIP_RESOURCES_REMAINING(ship) > 0)
		msg_to_char(ch, "You can't board the ship until it's finished!\r\n");
	else {
		act("$n boards $p.", TRUE, ch, ship, 0, TO_ROOM);
		msg_to_char(ch, "You board it.\r\n");
		if (leading)
			act("$n is lead on-board.", TRUE, leading, 0, ch, TO_NOTVICT);
		char_to_room(ch, ship_room);
		look_at_room(ch);
		
		if (!IS_NPC(ch)) {
			GET_LAST_DIR(ch) = NO_DIR;
		}
			
		enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
		entry_memory_mtrigger(ch);
		greet_mtrigger(ch, NO_DIR);
		greet_memory_mtrigger(ch);
		
		act("$n boards the ship.", TRUE, ch, 0, 0, TO_ROOM);
		if (leading) {
			char_to_room(leading, ship_room);
			act("$n boards the ship.", TRUE, leading, 0, 0, TO_ROOM);
			look_at_room(leading);
			
			if (!IS_NPC(leading)) {
				GET_LAST_DIR(leading) = NO_DIR;
			}
			enter_wtrigger(IN_ROOM(leading), leading, NO_DIR);
			entry_memory_mtrigger(leading);
			greet_mtrigger(leading, NO_DIR);
			greet_memory_mtrigger(leading);
		}

		for (k = ch->followers; k; k = k->next)
			if ((IN_ROOM(k->follower) == was_in) && (GET_POS(k->follower) >= POS_STANDING)) {
				act("You follow $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
				act("$n boards $p.", TRUE, k->follower, ship, 0, TO_ROOM);
				char_to_room(k->follower, ship_room);
				act("$n boards the ship.", TRUE, k->follower, 0, 0, TO_ROOM);
				look_at_room(k->follower);
				
				if (!IS_NPC(k->follower)) {
					GET_LAST_DIR(k->follower) = NO_DIR;
				}
				enter_wtrigger(IN_ROOM(k->follower), k->follower, NO_DIR);
				entry_memory_mtrigger(k->follower);
				greet_mtrigger(k->follower, NO_DIR);
				greet_memory_mtrigger(k->follower);
			}
		if (BUILDING_VNUM(was_in) != BUILDING_DOCKS)
			WAIT_STATE(ch, 2 RL_SEC);
	}
}


ACMD(do_disembark) {
	obj_data *ship = GET_BOAT(IN_ROOM(ch));
	char_data *leading = NULL;
	room_data *was_in = IN_ROOM(ch);
	struct follow_type *k;

	if (!ship)
		msg_to_char(ch, "You can't disembark from here!\r\n");
	else if (IS_RIDING(ch))
		msg_to_char(ch, "You can't disembark while riding.\r\n");
	else if (GET_LEADING(ch) && IN_ROOM(GET_LEADING(ch)) == IN_ROOM(ch) && (leading = GET_LEADING(ch)) && BUILDING_VNUM(IN_ROOM(ship)) != BUILDING_DOCKS)
		msg_to_char(ch, "You can't lead an animal off the ship unless it's at a dock.\r\n");
	else {
		act("$n disembarks from $p.", TRUE, ch, ship, 0, TO_ROOM);
		msg_to_char(ch, "You disembark.\r\n");
		if (leading)
			act("$n is lead off.", TRUE, leading, 0, ch, TO_NOTVICT);
		char_to_room(ch, IN_ROOM(ship));
		act("$n disembarks from $p.", TRUE, ch, ship, 0, TO_ROOM);
		look_at_room(ch);
		
		if (!IS_NPC(ch)) {
			GET_LAST_DIR(ch) = NO_DIR;
		}
		enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
		entry_memory_mtrigger(ch);
		greet_mtrigger(ch, NO_DIR);
		greet_memory_mtrigger(ch);
		
		
		if (leading) {
			char_to_room(leading, IN_ROOM(ship));
			act("$n disembarks from $p.", TRUE, leading, ship, 0, TO_ROOM);
			look_at_room(leading);
			
			if (!IS_NPC(leading)) {
				GET_LAST_DIR(leading) = NO_DIR;
			}
			enter_wtrigger(IN_ROOM(leading), leading, NO_DIR);
			entry_memory_mtrigger(leading);
			greet_mtrigger(leading, NO_DIR);
			greet_memory_mtrigger(leading);
		}

		for (k = ch->followers; k; k = k->next)
			if ((IN_ROOM(k->follower) == was_in) && (GET_POS(k->follower) >= POS_STANDING)) {
				act("You follow $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
				act("$n disembarks from $p.", TRUE, k->follower, ship, 0, TO_ROOM);
				char_to_room(k->follower, IN_ROOM(ship));
				act("$n disembarks from $p.", TRUE, k->follower, ship, 0, TO_ROOM);
				look_at_room(k->follower);
				
				if (!IS_NPC(k->follower)) {
					GET_LAST_DIR(k->follower) = NO_DIR;
				}
				enter_wtrigger(IN_ROOM(k->follower), k->follower, NO_DIR);
				entry_memory_mtrigger(k->follower);
				greet_mtrigger(k->follower, NO_DIR);
				greet_memory_mtrigger(k->follower);
			}
		if (BUILDING_VNUM(IN_ROOM(ch)) != BUILDING_DOCKS)
			WAIT_STATE(ch, 2 RL_SEC);
	}
}


ACMD(do_load_boat) {
	obj_data *ship, *to_load = NULL;
	room_data *room, *next_room, *ship_room;
	int val;
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
		HASH_ITER(interior_hh, interior_world_table, room, next_room) {
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
				/* else nothing was loaded at all to this room */
			}
		}
		if (!to_load && !done) {
			act("$p is full.", FALSE, ch, ship, 0, TO_CHAR);
		}
	}
}


ACMD(do_manufacture) {
	obj_data *ship;
	int i = 0;
	bool comma = FALSE;
	empire_data *emp;

	for (ship = ROOM_CONTENTS(IN_ROOM(ch)); ship; ship = ship->next_content) {
		if (GET_OBJ_TYPE(ship) == ITEM_SHIP && GET_SHIP_RESOURCES_REMAINING(ship) > 0) {
			break;
		}
	}

	skip_spaces(&argument);

	if (*argument)
		for (i = 0; str_cmp(ship_data[i].name, "\n") && (!is_abbrev(argument, ship_data[i].name) || (ship_data[i].ability != NO_ABIL && !HAS_ABILITY(ch, ship_data[i].ability))); i++);

	if ((BUILDING_VNUM(IN_ROOM(ch)) != BUILDING_SHIPYARD && BUILDING_VNUM(IN_ROOM(ch)) != BUILDING_SHIPYARD2) || !IS_COMPLETE(IN_ROOM(ch)))
		msg_to_char(ch, "You can't do that here.\r\n");
	else if (GET_ACTION(ch) == ACT_MANUFACTURING) {
		msg_to_char(ch, "You stop working on the ship.\r\n");
		GET_ACTION(ch) = ACT_NONE;
		}
	else if (GET_ACTION(ch) != ACT_NONE)
		msg_to_char(ch, "You're busy doing something else right now.\r\n");
	else if (ship && *argument)
		msg_to_char(ch, "A shipyard may only build one ship at a time.\r\n");
	else if (!ship && (!*argument || !str_cmp(ship_data[i].name, "\n"))) {
		msg_to_char(ch, "What type of ship would you like to build:");
		for (i = 0; str_cmp(ship_data[i].name, "\n"); i++)
			if (ship_data[i].ability == NO_ABIL || HAS_ABILITY(ch, ship_data[i].ability)) {
				msg_to_char(ch, "%s%s", (comma ? ", " : " "), ship_data[i].name);
				comma = TRUE;
			}
		msg_to_char(ch, "\r\n");
	}
	else if (ship) {
		act("You begin working on $p.", FALSE, ch, ship, 0, TO_CHAR);
		act("$n begins working on $p.", FALSE, ch, ship, 0, TO_ROOM);
		start_action(ch, ACT_MANUFACTURING, 1, NOBITS);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED))
		msg_to_char(ch, "You don't have permission to manufacture ships here.\r\n");
	else if (ship_data[i].advanced && BUILDING_VNUM(IN_ROOM(ch)) != BUILDING_SHIPYARD2)
		msg_to_char(ch, "You can't build that without an advanced shipyard.\r\n");
	else {
		emp = get_or_create_empire(ch);
		ship = create_ship(ship_data[i].vnum, emp, IN_ROOM(ch));
		msg_to_char(ch, "You begin working on a %s.\r\n", ship_data[i].name);
		act("$n begins to build a ship.", FALSE, ch, 0, 0, TO_ROOM);
		start_action(ch, ACT_MANUFACTURING, 1, NOBITS);
		GET_OBJ_VAL(ship, VAL_SHIP_RESOURCES_REMAINING) = ship_data[i].resources;
	}
}


ACMD(do_sail) {
	extern const char *from_dir[];

	struct room_direction_data *ex;
	char_data *vict;
	obj_data *ship;
	int dir;
	room_data *to_room, *was_in = NULL;

	skip_spaces(&argument);
	
	// basics
	if (!(ship = GET_BOAT(HOME_ROOM(IN_ROOM(ch))))) {
		msg_to_char(ch, "You can only sail on a ship.\r\n");
		return;
	}
	if (!can_use_room(ch, HOME_ROOM(IN_ROOM(ch)), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to sail this ship.\r\n");
		return;
	}
	if (!*argument) {
		msg_to_char(ch, "Which direction would you like to sail?\r\n");
		return;
	}
	if ((dir = parse_direction(ch, argument)) == NO_DIR || dir >= NUM_2D_DIRS) {
		msg_to_char(ch, "You can't sail that direction.\r\n");
		return;
	}
	
	// targeting
	if (ROOM_IS_CLOSED(IN_ROOM(ship))) {
		if (!(ex = find_exit(IN_ROOM(ship), dir))) {
			msg_to_char(ch, "You can't sail that direction!\r\n");
			return;
		}

		to_room = ex->room_ptr;
	}
	else {
		to_room = real_shift(IN_ROOM(ship), shift_dir[dir][0], shift_dir[dir][1]);
	}
	
	// checks based on target
	if (!to_room || !CAN_SAIL_IN(to_room)) {
		msg_to_char(ch, "You can't sail that direction.\r\n");
		return;
	}
	if (!IS_COMPLETE(to_room)) {
		msg_to_char(ch, "You can't sail in until it's complete.\r\n");
		return;
	}
	if (!ROOM_IS_CLOSED(IN_ROOM(ship)) && ROOM_IS_CLOSED(to_room) && !can_use_room(ch, to_room, GUESTS_ALLOWED) && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You don't have permission to sail there.\r\n");
		return;
	}

	// let's do this
	was_in = IN_ROOM(ship);
	
	// notify leaving
	for (vict = ROOM_PEOPLE(IN_ROOM(ship)); vict; vict = vict->next_in_room) {
		if (vict->desc) {
			sprintf(buf, "$p sails %s.", dirs[get_direction_for_char(vict, dir)]);
			act(buf, TRUE, vict, ship, 0, TO_CHAR);
		}
	}
			
	obj_to_room(ship, to_room);
	check_for_ships_present(was_in);	// update old room
	check_for_ships_present(to_room);	// update new room
	
	for (vict = ROOM_PEOPLE(IN_ROOM(ship)); vict; vict = vict->next_in_room) {
		if (vict->desc) {
			sprintf(buf, "$p sails in %s.", from_dir[get_direction_for_char(vict, dir)]);
			act(buf, TRUE, vict, ship, 0, TO_CHAR | TO_SPAMMY);
		}
	}
	
	// looking at the room you're in while on a ship shows the surrounding area
	look_at_room_by_loc(ch, IN_ROOM(ch), NOBITS);
	WAIT_STATE(ch, 1 RL_SEC);
}


ACMD(do_unload_boat) {
	obj_data *ship;
	room_data *room, *next_room, *ship_room;
	int val;
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
		HASH_ITER(interior_hh, interior_world_table, room, next_room) {
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
	}
}
