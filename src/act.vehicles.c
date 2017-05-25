/* ************************************************************************
*   File: act.vehicles.c                                  EmpireMUD 2.0b5 *
*  Usage: commands related to vehicles and vehicle movement               *
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
#include "dg_scripts.h"
#include "vnums.h"

/**
* Contents:
*   Helpers
*   Sub-Commands
*   Commands
*/

// local protos

// external consts
extern const char *dirs[];
extern const char *from_dir[];
extern const bool is_flat_dir[NUM_OF_DIRS];
extern const char *mob_move_types[];
extern const int rev_dir[];

// external funcs
extern int count_harnessed_animals(vehicle_data *veh);
extern room_data *dir_to_room(room_data *room, int dir, bool ignore_entrance);
extern struct vehicle_attached_mob *find_harnessed_mob_by_name(vehicle_data *veh, char *name);
extern room_data *get_vehicle_interior(vehicle_data *veh);
void harness_mob_to_vehicle(char_data *mob, vehicle_data *veh);
extern int perform_move(char_data *ch, int dir, int need_specials_check, byte mode);
void scale_item_to_level(obj_data *obj, int level);
void trigger_distrust_from_hostile(char_data *ch, empire_data *emp);	// fight.c
extern char_data *unharness_mob_from_vehicle(struct vehicle_attached_mob *vam, vehicle_data *veh);
extern bool validate_vehicle_move(char_data *ch, vehicle_data *veh, room_data *to_room);

// local data
struct {
	char *command;	// "drive"
	char *verb;	// "driving"
	int action;	// ACT_
	bitvector_t flag;	// required VEH_ flag
} drive_data[] = {
	{ "drive", "driving", ACT_DRIVING, VEH_DRIVING },	// SCMD_DRIVE
	{ "sail", "sailing", ACT_SAILING, VEH_SAILING },	// SCMD_SAIL
	{ "pilot", "piloting", ACT_PILOTING, VEH_FLYING },	// SCMD_PILOT
};


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Alerts people that the vehicle stops moving.
*
* @param char_data *ch The driver.
*/
void cancel_driving(char_data *ch) {
	char buf[MAX_STRING_LENGTH];
	vehicle_data *veh;
	
	if (!(veh = GET_DRIVING(ch))) {
		return;
	}
	
	snprintf(buf, sizeof(buf), "%s stops moving.\r\n", VEH_SHORT_DESC(veh));
	CAP(buf);
	msg_to_vehicle(veh, FALSE, buf);
	
	GET_DRIVING(ch) = NULL;
	VEH_DRIVER(veh) = NULL;
}


/**
* Finds a ship in ch's room or docked on ch's current island, which could be
* dispatched. Ships in the same room are preferred even if they aren't owned
* by ch; ships in other rooms are partially-validated for ownership and flags.
*
* @param char_data *ch The person looking to dispatch a ship.
* @param char *arg The typed-in arg for ship name.
* @return vehicle_data *veh The found ship, if any.
*/
vehicle_data *find_ship_to_dispatch(char_data *ch, char *arg) {
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	int island, found = 0, number = 1;
	vehicle_data *veh;
	
	// prefer ones here
	if ((veh = get_vehicle_in_room_vis(ch, arg))) {
		return veh;
	}
	
	// can't really find an island match if the character is not in an empire
	if (!GET_LOYALTY(ch)) {
		return NULL;
	}
	
	// can't find ships if character isn't on an island
	if ((island = GET_ISLAND_ID(IN_ROOM(ch))) == NO_ISLAND) {
		return NULL;
	}
	
	// 0.x does not target vehicles
	strcpy(tmp, arg);
	if ((number = get_number(&tmp)) == 0) {
		return NULL;
	}
	
	// otherwise look for ones that match
	LL_FOREACH(vehicle_list, veh) {
		if (VEH_OWNER(veh) != GET_LOYALTY(ch)) {
			continue;
		}
		if (!VEH_IS_COMPLETE(veh) || !VEH_FLAGGED(veh, VEH_SHIPPING)) {
			continue;
		}
		if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
			continue;
		}
		if (VEH_INTERIOR_HOME_ROOM(veh) && ROOM_AFF_FLAGGED(VEH_INTERIOR_HOME_ROOM(veh), ROOM_AFF_NO_WORK)) {
			continue;
		}
		if (VEH_SHIPPING_ID(veh) != -1) {
			continue;
		}
		
		// ensure in docks if we're finding it remotely
		if (!IN_ROOM(veh) || !room_has_function_and_city_ok(IN_ROOM(veh), FNC_DOCKS)) {
			continue;
		}
		if (GET_ISLAND_ID(IN_ROOM(veh)) != island) {
			continue;
		}
		
		// check name match
		if (!isname(tmp, VEH_KEYWORDS(veh))) {
			continue;
		}
		
		// found: check number
		if (++found == number) {
			return veh;
		}
	}
	
	return NULL;
}


/**
* Finds a valid target for a siege. This doesn't validate the target, just
* determines what the player is aiming at. This sends its own error messages.
*
* @param char_data *ch The person trying to lay siege.
* @param vehicle_data *veh The vehicle they're firing with. (Optional, it could actually be ch.)
* @param char *arg The typed-in target arg.
* @param room_data **room_targ A variable to bind the result to, if a room target.
* @param int *dir A variable to bind the direction to, for a room target.
* @param vehicle_data **veh_targ A variable to bind the result to, for a vehicle.
* @return bool TRUE if any target was found, FALSE if not.
*/
bool find_siege_target_for_vehicle(char_data *ch, vehicle_data *veh, char *arg, room_data **room_targ, int *dir, vehicle_data **veh_targ) {
	bool validate_siege_target_room(char_data *ch, vehicle_data *veh, room_data *to_room);
	bool validate_siege_target_vehicle(char_data *ch, vehicle_data *veh, vehicle_data *target);
	
	vehicle_data *tar;
	room_data *from_room = veh ? IN_ROOM(veh) : IN_ROOM(ch);
	room_data *room;
	int find_dir;
	
	// init
	*room_targ = NULL;
	*dir = NO_DIR;
	*veh_targ = NULL;
	
	// direction targeting
	if ((find_dir = parse_direction(ch, arg)) != NO_DIR) {
		if (!is_flat_dir[find_dir] || !(room = dir_to_room(from_room, find_dir, TRUE)) || room == from_room) {
			msg_to_char(ch, "You can't shoot that direction.\r\n");
		}
		else if (!validate_siege_target_room(ch, veh, room)) {
			// sends own message
		}
		else {
			// found!
			*dir = find_dir;
			*room_targ = room;
		}
	}

	// vehicle targeting
	else if ((tar = get_vehicle_in_target_room_vis(ch, from_room, arg))) {
		// validation
		if (!validate_siege_target_vehicle(ch, veh, tar)) {
			// sends own message
		}
		else {
			// found!
			*veh_targ = tar;
		}
	}
	else {
		msg_to_char(ch, "There's no %s to shoot at.\r\n", arg);
	}
	
	return (*room_targ || *veh_targ);
}


/**
* Attempt to move a vehicle. This may send an error message if it fails.
*
* @param char_data *ch The person trying to move the vehicle (optional: may be NULL).
* @param vehicle_data *veh The vehicle to move.
* @param int dir Which direction to move it.
* @param int subcmd A do_drive subcmd (0 for default).
* @return bool TRUE if it moved, FALSE if it was blocked.
*/
bool move_vehicle(char_data *ch, vehicle_data *veh, int dir, int subcmd) {
	void msdp_update_room(char_data *ch);
	
	room_data *to_room = NULL, *was_in;
	struct follow_type *fol, *next_fol;
	struct vehicle_room_list *vrl;
	char buf[MAX_STRING_LENGTH];
	char_data *ch_iter;
	
	// sanity
	if (!veh || dir == NO_DIR) {
		return FALSE;
	}
	
	if (ch && !can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
		act("You don't have permission to use $V.", FALSE, ch, NULL, veh, TO_CHAR);
		return FALSE;
	}

	// targeting
	if (!(to_room = dir_to_room(IN_ROOM(veh), dir, FALSE))) {
		if (ch) {
			snprintf(buf, sizeof(buf), "$V can't %s any further %s.", drive_data[subcmd].command, dirs[get_direction_for_char(ch, dir)]);
			act(buf, FALSE, ch, NULL, veh, TO_CHAR);
		}
		return FALSE;
	}
	if (!validate_vehicle_move(ch, veh, to_room)) {
		// sends own message
		return FALSE;
	}
	if (!IS_COMPLETE(to_room) && (!WATER_SECT(to_room) || !VEH_FLAGGED(veh, VEH_SAILING | VEH_FLYING))) {
		if (ch) {
			msg_to_char(ch, "You can't %s in there until it's complete.\r\n", drive_data[subcmd].command);
		}
		return FALSE;
	}
	if (ch && !ROOM_IS_CLOSED(IN_ROOM(veh)) && ROOM_IS_CLOSED(to_room) && !can_use_room(ch, to_room, GUESTS_ALLOWED) && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You don't have permission to %s there.\r\n", drive_data[subcmd].command);
		return FALSE;
	}
	
	// let's do this:
	
	// notify leaving
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(veh)), ch_iter, next_in_room) {
		if (ch_iter != VEH_SITTING_ON(veh) && ch_iter->desc) {
			if (VEH_SITTING_ON(veh)) {
				sprintf(buf, "$v %s %s with $N %s it.", mob_move_types[VEH_MOVE_TYPE(veh)], dirs[get_direction_for_char(ch_iter, dir)], IN_OR_ON(veh));
				act(buf, TRUE, ch_iter, veh, VEH_SITTING_ON(veh), TO_CHAR | ACT_VEHICLE_OBJ);
			}
			else {
				sprintf(buf, "$V %s %s.", mob_move_types[VEH_MOVE_TYPE(veh)], dirs[get_direction_for_char(ch_iter, dir)]);
				act(buf, TRUE, ch_iter, NULL, veh, TO_CHAR);
			}
		}
	}
	
	was_in = IN_ROOM(veh);
	vehicle_to_room(veh, to_room);
	
	if (!entry_vtrigger(veh)) {
		vehicle_to_room(veh, was_in);
		return FALSE;
	}
	
	// notify arrival
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(veh)), ch_iter, next_in_room) {
		if (ch_iter != VEH_SITTING_ON(veh) && ch_iter->desc) {
			if (VEH_SITTING_ON(veh)) {
				sprintf(buf, "$v %s in from %s with $N %s it.", mob_move_types[VEH_MOVE_TYPE(veh)], from_dir[get_direction_for_char(ch_iter, dir)], IN_OR_ON(veh));
				act(buf, TRUE, ch_iter, veh, VEH_SITTING_ON(veh), TO_CHAR | ACT_VEHICLE_OBJ);
			}
			else {
				sprintf(buf, "$V %s in from %s.", mob_move_types[VEH_MOVE_TYPE(veh)], from_dir[get_direction_for_char(ch_iter, dir)]);
				act(buf, TRUE, ch_iter, NULL, veh, TO_CHAR);
			}
		}
	}
	
	// message driver
	if (VEH_DRIVER(veh)) {
		if (has_ability(VEH_DRIVER(veh), ABIL_NAVIGATION)) {
			snprintf(buf, sizeof(buf), "You %s $V %s (%d, %d).", drive_data[subcmd].command, dirs[get_direction_for_char(VEH_DRIVER(veh), dir)], X_COORD(IN_ROOM(veh)), Y_COORD(IN_ROOM(veh)));
		}
		else {
			snprintf(buf, sizeof(buf), "You %s $V %s.", drive_data[subcmd].command, dirs[get_direction_for_char(VEH_DRIVER(veh), dir)]);
		}
		act(buf, FALSE, VEH_DRIVER(veh), NULL, veh, TO_CHAR);
		msdp_update_room(VEH_DRIVER(veh));
	}
	
	// move sitter
	if (VEH_SITTING_ON(veh)) {
		char_to_room(VEH_SITTING_ON(veh), to_room);
		qt_visit_room(VEH_SITTING_ON(veh), IN_ROOM(VEH_SITTING_ON(veh)));
		if (!IS_NPC(VEH_SITTING_ON(veh))) {
			GET_LAST_DIR(VEH_SITTING_ON(veh)) = dir;
		}
		
		if (VEH_SITTING_ON(veh) != VEH_DRIVER(veh)) {
			if (has_ability(VEH_SITTING_ON(veh), ABIL_NAVIGATION)) {
				snprintf(buf, sizeof(buf), "$V %s %s (%d, %d).", mob_move_types[VEH_MOVE_TYPE(veh)], dirs[get_direction_for_char(ch_iter, dir)], X_COORD(IN_ROOM(veh)), Y_COORD(IN_ROOM(veh)));
			}
			else {
				snprintf(buf, sizeof(buf), "$V %s %s.", mob_move_types[VEH_MOVE_TYPE(veh)], dirs[get_direction_for_char(ch_iter, dir)]);
			}
			act(buf, FALSE, VEH_SITTING_ON(veh), NULL, veh, TO_CHAR);
		}
		
		entry_memory_mtrigger(VEH_SITTING_ON(veh));
		greet_mtrigger(VEH_SITTING_ON(veh), dir);
		greet_memory_mtrigger(VEH_SITTING_ON(veh));
		greet_vtrigger(VEH_SITTING_ON(veh), NO_DIR);
		
		LL_FOREACH_SAFE(VEH_SITTING_ON(veh)->followers, fol, next_fol) {
			if ((IN_ROOM(fol->follower) == was_in) && (GET_POS(fol->follower) >= POS_STANDING)) {
				act("You follow $N.\r\n", FALSE, fol->follower, NULL, VEH_SITTING_ON(veh), TO_CHAR);
				perform_move(fol->follower, dir, TRUE, MOVE_FOLLOW);
			}
		}
	}
	
	// alert whole vessel
	if (VEH_ROOM_LIST(veh)) {
		LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
			LL_FOREACH2(ROOM_PEOPLE(vrl->room), ch_iter, next_in_room) {
				if (ch_iter->desc && ch_iter != VEH_DRIVER(veh)) {
					if (has_ability(ch_iter, ABIL_NAVIGATION)) {
						snprintf(buf, sizeof(buf), "$V %s %s (%d, %d).", mob_move_types[VEH_MOVE_TYPE(veh)], dirs[get_direction_for_char(ch_iter, dir)], X_COORD(IN_ROOM(veh)), Y_COORD(IN_ROOM(veh)));
					}
					else {
						snprintf(buf, sizeof(buf), "$V %s %s.", mob_move_types[VEH_MOVE_TYPE(veh)], dirs[get_direction_for_char(ch_iter, dir)]);
					}
					act(buf, FALSE, ch_iter, NULL, veh, TO_CHAR | TO_SPAMMY);
					msdp_update_room(ch_iter);
				}
			}
		}
	}
	
	return TRUE;
}


/**
* Processes a get-item-from-vehicle.
*
* @param char_data *ch The person trying to get.
* @param obj_data *obj The item to get.
* @Param vehicle_data *veh The vehicle the item is in.
* @param int mode A find-obj mode like FIND_OBJ_INV.
* @return bool TRUE if successful, FALSE on fail.
*/
bool perform_get_from_vehicle(char_data *ch, obj_data *obj, vehicle_data *veh, int mode) {
	extern bool can_take_obj(char_data *ch, obj_data *obj);
	void get_check_money(char_data *ch, obj_data *obj);

	if (!bind_ok(obj, ch)) {
		act("$p: item is bound to someone else.", FALSE, ch, obj, NULL, TO_CHAR);
		return TRUE;	// don't break loop
	}
	if (!IS_NPC(ch) && !CAN_CARRY_OBJ(ch, obj)) {
		act("$p: you can't hold any more items.", FALSE, ch, obj, NULL, TO_CHAR);
		return FALSE;
	}
	
	if (mode == FIND_OBJ_INV || can_take_obj(ch, obj)) {
		if (get_otrigger(obj, ch)) {
			// last-minute scaling: scale to its minimum (adventures will override this on their own)
			if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) < 1) {
				scale_item_to_level(obj, GET_OBJ_MIN_SCALE_LEVEL(obj));
			}
			
			obj_to_char(obj, ch);
			act("You get $p from $V.", FALSE, ch, obj, veh, TO_CHAR);
			act("$n gets $p from $V.", TRUE, ch, obj, veh, TO_ROOM);
			get_check_money(ch, obj);
			return TRUE;
		}
	}
	return TRUE;	// return TRUE even though it failed -- don't break "get all" loops
}


/**
* Processes putting a single item into a vehicle.
*
* @param char_data *ch The player acting.
* @param obj_data *obj The item being put.
* @param vehicle_data *veh The vehicle to put it in.
* @return bool TRUE if successful, FALSE if not.
*/
bool perform_put_obj_in_vehicle(char_data *ch, obj_data *obj, vehicle_data *veh) {
	char_data *mort;
	
	if (!drop_otrigger(obj, ch)) {	// also takes care of obj purging self
		return FALSE;
	}
	
	if (VEH_CARRYING_N(veh) + obj_carry_size(obj) > VEH_CAPACITY(veh)) {
		act("$p won't fit in $V.", FALSE, ch, obj, veh, TO_CHAR);
		return FALSE;
	}
	
	obj_to_vehicle(obj, veh);
	act("$n puts $p in $V.", TRUE, ch, obj, veh, TO_ROOM);
	act("You put $p in $V.", FALSE, ch, obj, veh, TO_CHAR);
	
	if (IS_IMMORTAL(ch)) {
		if (VEH_OWNER(veh) && !EMPIRE_IMM_ONLY(VEH_OWNER(veh))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s puts %s into a vehicle owned by mortal empire (%s) at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), EMPIRE_NAME(VEH_OWNER(veh)), room_log_identifier(IN_ROOM(veh)));
		}
		else if (ROOM_OWNER(IN_ROOM(veh)) && !EMPIRE_IMM_ONLY(ROOM_OWNER(IN_ROOM(veh)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s puts %s into a vehicle in mortal empire (%s) at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))), room_log_identifier(IN_ROOM(veh)));
		}
		else if ((mort = find_mortal_in_room(IN_ROOM(ch)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s puts %s into a vehicle with mortal present (%s) at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), GET_NAME(mort), room_log_identifier(IN_ROOM(ch)));
		}
	}
	
	return TRUE;
}


/**
* Performs loading of a mob to a vehicle.
*
* @param char_data *ch The person doing the loading.
* @param char_data *mob The person to load.
* @param vehicle_data *cont The vehicle to load onto.
* @param room_data *to_room Where to put the item in cont.
*/
void perform_load_mob(char_data *ch, char_data *mob, vehicle_data *cont, room_data *to_room) {
	snprintf(buf, sizeof(buf), "You load $N %sto $v.", IN_OR_ON(cont));
	act(buf, FALSE, ch, cont, mob, TO_CHAR | ACT_VEHICLE_OBJ);
	snprintf(buf, sizeof(buf), "$n loads you %sto $v.", IN_OR_ON(cont));
	act(buf, FALSE, ch, cont, mob, TO_VICT | ACT_VEHICLE_OBJ);
	snprintf(buf, sizeof(buf), "$n loads $N %sto $v.", IN_OR_ON(cont));
	act(buf, FALSE, ch, cont, mob, TO_NOTVICT | ACT_VEHICLE_OBJ);
	
	char_to_room(mob, to_room);
	if (mob->desc) {
		look_at_room(mob);
	}
	
	snprintf(buf, sizeof(buf), "$n is loaded %sto $V.", IN_OR_ON(cont));
	act(buf, FALSE, mob, NULL, cont, TO_ROOM);
	
	enter_wtrigger(IN_ROOM(mob), mob, NO_DIR);
	entry_memory_mtrigger(mob);
	greet_mtrigger(mob, NO_DIR);
	greet_memory_mtrigger(mob);
	greet_vtrigger(mob, NO_DIR);
}


/**
* Performs loading of a vehicle into another vehicle.
*
* @param char_data *ch The person doing the loading.
* @param vehicle_data *veh The vehicle to load.
* @param vehicle_data *cont The vehicle to load into.
* @param room_data *to_room Where to put the item in cont.
*/
void perform_load_vehicle(char_data *ch, vehicle_data *veh, vehicle_data *cont, room_data *to_room) {
	snprintf(buf, sizeof(buf), "You load $V %sto $v.", IN_OR_ON(cont));
	act(buf, FALSE, ch, cont, veh, TO_CHAR | ACT_VEHICLE_OBJ);
	snprintf(buf, sizeof(buf), "$n loads $V %sto $v.", IN_OR_ON(cont));
	act(buf, FALSE, ch, cont, veh, TO_ROOM | ACT_VEHICLE_OBJ);
	
	vehicle_to_room(veh, to_room);
	
	snprintf(buf, sizeof(buf), "$V is loaded %sto $v.", IN_OR_ON(cont));
	if (ROOM_PEOPLE(to_room)) {
		act(buf, FALSE, ROOM_PEOPLE(to_room), cont, veh, TO_CHAR | TO_ROOM | ACT_VEHICLE_OBJ);
	}
}


/**
* Performs 1 unload of a mob from a vehicle.
*
* @param char_data *ch The person doing the unloading.
* @param char_data *mob The person to unload.
* @param vehicle_data *cont The containing vehicle.
*/
void perform_unload_mob(char_data *ch, char_data *mob, vehicle_data *cont) {
	act("You unload $N from $v.", FALSE, ch, cont, mob, TO_CHAR | ACT_VEHICLE_OBJ);
	act("$n unloads you from $v.", FALSE, ch, cont, mob, TO_VICT | ACT_VEHICLE_OBJ);
	act("$n unloads $N from $v.", FALSE, ch, cont, mob, TO_NOTVICT | ACT_VEHICLE_OBJ);
	
	char_to_room(mob, IN_ROOM(cont));
	if (mob->desc) {
		look_at_room(mob);
	}
	
	act("$n is unloaded from $V.", FALSE, mob, NULL, cont, TO_ROOM);
	
	enter_wtrigger(IN_ROOM(mob), mob, NO_DIR);
	entry_memory_mtrigger(mob);
	greet_mtrigger(mob, NO_DIR);
	greet_memory_mtrigger(mob);
	greet_vtrigger(mob, NO_DIR);
}


/**
* Performs 1 unload of a vehicle from another vehicle.
*
* @param char_data *ch The person doing the unloading.
* @param vehicle_data *veh The vehicle to unload.
* @param vehicle_data *cont The containing vehicle.
*/
void perform_unload_vehicle(char_data *ch, vehicle_data *veh, vehicle_data *cont) {
	act("You unload $V from $v.", FALSE, ch, cont, veh, TO_CHAR | ACT_VEHICLE_OBJ);
	act("$n unloads $V from $v.", FALSE, ch, cont, veh, TO_ROOM | ACT_VEHICLE_OBJ);
	
	vehicle_to_room(veh, IN_ROOM(cont));
	
	if (ROOM_PEOPLE(IN_ROOM(cont))) {
		act("$V is unloaded from $v.", FALSE, ROOM_PEOPLE(IN_ROOM(cont)), cont, veh, TO_CHAR | TO_ROOM | ACT_VEHICLE_OBJ);
	}
}


/**
* Tick update for driving action.
*
* @param char_data *ch The character doing the driving.
*/
void process_driving(char_data *ch) {
	extern int get_north_for_char(char_data *ch);
	extern const int confused_dirs[NUM_2D_DIRS][2][NUM_OF_DIRS];
	
	int dir = GET_ACTION_VNUM(ch, 0), subcmd = GET_ACTION_VNUM(ch, 2);
	vehicle_data *veh;
	
	// translate 'dir' from the way the character THINKS he's going, to the actual way
	dir = confused_dirs[get_north_for_char(ch)][0][dir];
	
	// not got a vehicle?
	if ((!(veh = GET_SITTING_ON(ch)) && !(veh = GET_ROOM_VEHICLE(IN_ROOM(ch)))) || !VEH_FLAGGED(veh, drive_data[subcmd].flag)) {
		cancel_action(ch);
		return;
	}
	// on wrong vehicle?
	if (veh != GET_DRIVING(ch)) {
		cancel_action(ch);
		return;
	}
	
	if (count_harnessed_animals(veh) < VEH_ANIMALS_REQUIRED(veh)) {
		cancel_action(ch);
		return;
	}
	
	if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		cancel_action(ch);
		return;
	}
	
	// attempt to move the vehicle
	if (!move_vehicle(ch, veh, dir, subcmd)) {
		look_at_room(ch);	// show them where they stopped
		msg_to_char(ch, "\r\n");	// extra linebreak between look and "vehicle stops"
		cancel_action(ch);
		return;
	}
	
	// update location to prevent auto-cancel
	GET_ACTION_ROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));
	
	// limited distance?
	if (GET_ACTION_VNUM(ch, 1) > 0) {
		GET_ACTION_VNUM(ch, 1) -= 1;
		
		// arrived!
		if (GET_ACTION_VNUM(ch, 1) <= 0) {
			look_at_room(ch);	// show them where they stopped
			msg_to_char(ch, "\r\n");	// extra linebreak between look and "vehicle stops"
			cancel_action(ch);
			return;
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// SUB-COMMANDS ////////////////////////////////////////////////////////////

/**
* Sub-processor for do_customize: customize vehicle <target> <name | description> <value>
*
* @param char_data *ch The person trying to customize the vehicle.
* @param char *argument The argument.
*/
void do_customize_vehicle(char_data *ch, char *argument) {
	char tar_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	vehicle_data *veh, *proto;
	
	argument = two_arguments(argument, tar_arg, type_arg);
	skip_spaces(&argument);
	
	if (!ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!*tar_arg || !*type_arg || !*argument) {
		msg_to_char(ch, "Usage: customize vehicle <target> <name | longdesc | description> <value | set | none>\r\n");
	}
	// vehicle validation
	else if (!(veh = get_vehicle_in_room_vis(ch, tar_arg)) && (!(veh = GET_ROOM_VEHICLE(IN_ROOM(ch))) || !isname(tar_arg, VEH_KEYWORDS(veh)))) {
		msg_to_char(ch, "You don't see that vehicle here.\r\n");
	}
	else if (!VEH_FLAGGED(veh, VEH_CUSTOMIZABLE)) {
		msg_to_char(ch, "You can't customize that!\r\n");
	}
	else if (!can_use_vehicle(ch, veh, MEMBERS_ONLY) || !has_permission(ch, PRIV_CUSTOMIZE)) {
		msg_to_char(ch, "You don't have permission to customize that.\r\n");
	}
	
	// types:
	else if (is_abbrev(type_arg, "name")) {
		if (!*argument) {
			msg_to_char(ch, "What would you like to name this vehicle (or \"none\")?\r\n");
		}
		else if (!str_cmp(argument, "none")) {
			proto = vehicle_proto(VEH_VNUM(veh));
			
			if (proto && VEH_SHORT_DESC(veh) != VEH_SHORT_DESC(proto)) {
				free(VEH_SHORT_DESC(veh));
				VEH_SHORT_DESC(veh) = VEH_SHORT_DESC(proto);
			}
			if (proto && VEH_KEYWORDS(veh) != VEH_KEYWORDS(proto)) {
				free(VEH_KEYWORDS(veh));
				VEH_KEYWORDS(veh) = VEH_KEYWORDS(proto);
			}
			if (proto && VEH_LONG_DESC(veh) != VEH_LONG_DESC(proto) && strstr(VEH_LONG_DESC(veh), VEH_LONG_DESC(proto))) {
				free(VEH_LONG_DESC(veh));
				VEH_LONG_DESC(veh) = VEH_LONG_DESC(proto);
			}
			
			msg_to_char(ch, "The vehicle no longer has a custom name.\r\n");
		}
		else if (color_code_length(argument) > 0) {
			msg_to_char(ch, "You cannot use color codes in custom names.\r\n");
		}
		else if (strlen(argument) > 24) {
			msg_to_char(ch, "That name is too long. Vehicle names may not be over 24 characters.\r\n");
		}
		else {
			proto = vehicle_proto(VEH_VNUM(veh));
			
			// change short desc
			if (!proto || VEH_SHORT_DESC(veh) != VEH_SHORT_DESC(proto)) {
				free(VEH_SHORT_DESC(veh));
			}
			VEH_SHORT_DESC(veh) = str_dup(argument);
			
			// change keywords
			sprintf(buf, "%s %s", fname(VEH_KEYWORDS(veh)), skip_filler(argument));
			if (!proto || VEH_KEYWORDS(veh) != VEH_KEYWORDS(proto)) {
				free(VEH_KEYWORDS(veh));
			}
			VEH_KEYWORDS(veh) = str_dup(buf);
			
			// optionally, change the longdesc too
			if (proto && (VEH_LONG_DESC(veh) == VEH_LONG_DESC(proto) || strstr(VEH_LONG_DESC(veh), VEH_LONG_DESC(proto)))) {
				if (VEH_LONG_DESC(veh) != VEH_LONG_DESC(proto)) {
					free(VEH_LONG_DESC(veh));
				}
				sprintf(buf, "%s (%s)", VEH_LONG_DESC(proto), VEH_SHORT_DESC(veh));
				VEH_LONG_DESC(veh) = str_dup(buf);
			}
			
			msg_to_char(ch, "It is now called \"%s\".\r\n", argument);
		}
	}
	else if (is_abbrev(type_arg, "longdesc")) {
		if (!*argument) {
			msg_to_char(ch, "What would you like to make the long description of this vehicle (or \"none\")?\r\n");
		}
		else if (!str_cmp(argument, "none")) {
			proto = vehicle_proto(VEH_VNUM(veh));
			
			if (!proto || VEH_LONG_DESC(veh) != VEH_LONG_DESC(proto)) {
				free(VEH_LONG_DESC(veh));
				VEH_LONG_DESC(veh) = VEH_LONG_DESC(proto);
			}
			
			msg_to_char(ch, "It no longer has a custom long description.\r\n");
		}
		else if (color_code_length(argument) > 0) {
			msg_to_char(ch, "You cannot use color codes in custom long descriptions.\r\n");
		}
		else if (strlen(argument) > 72) {
			msg_to_char(ch, "That description is too long. Long descriptions may not be over 72 characters.\r\n");
		}
		else {
			proto = vehicle_proto(VEH_VNUM(veh));
			
			if (!proto || VEH_LONG_DESC(veh) != VEH_LONG_DESC(proto)) {
				free(VEH_LONG_DESC(veh));
			}
			
			VEH_LONG_DESC(veh) = str_dup(argument);
			
			msg_to_char(ch, "It now has the long description:\r\n%s\r\n", argument);
		}
	}
	else if (is_abbrev(type_arg, "description")) {
		if (!*argument) {
			msg_to_char(ch, "To set a description, use \"description set\".\r\n");
			msg_to_char(ch, "To remove a description, use \"description none\".\r\n");
		}
		else if (ch->desc->str) {
			msg_to_char(ch, "You are already editing something else.\r\n");
		}
		else if (is_abbrev(argument, "none")) {
			proto = vehicle_proto(VEH_VNUM(veh));
			
			if (!proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(proto)) {
				if (VEH_LOOK_DESC(veh)) {
					free(VEH_LOOK_DESC(veh));
				}
				VEH_LOOK_DESC(veh) = VEH_LOOK_DESC(proto);
			}
			msg_to_char(ch, "It no longer has a custom description.\r\n");
		}
		else if (is_abbrev(argument, "set")) {
			proto = vehicle_proto(VEH_VNUM(veh));
			
			if (!proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(proto)) {
				// differs from proto
				start_string_editor(ch->desc, "vehicle description", &VEH_LOOK_DESC(veh), MAX_ITEM_DESCRIPTION, TRUE);
			}
			else {
				// has proto's desc
				VEH_LOOK_DESC(veh) = VEH_LOOK_DESC(proto) ? str_dup(VEH_LOOK_DESC(proto)) : NULL;
				start_string_editor(ch->desc, "vehicle description", &VEH_LOOK_DESC(veh), MAX_ITEM_DESCRIPTION, TRUE);
				ch->desc->str_on_abort = VEH_LOOK_DESC(proto);
			}
			
			act("$n begins editing a vehicle description.", TRUE, ch, 0, 0, TO_ROOM);
		}
		else {
			msg_to_char(ch, "You must specify whether you want to set or remove the description.\r\n");
		}
	}
	else {
		msg_to_char(ch, "You can customize name, longdesc, or description.\r\n");
	}

}

/**
* Performs a douse on a vehicle.
*
* @param char_data *ch The douser.
* @param vehicle_data *veh The burning vehicle.
* @param obj_data *cont The liquid container full of water.
*/
void do_douse_vehicle(char_data *ch, vehicle_data *veh, obj_data *cont) {
	if (!VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		msg_to_char(ch, "It's not even on fire!\r\n");
	}
	else {
		GET_OBJ_VAL(cont, VAL_DRINK_CONTAINER_CONTENTS) = 0;
		REMOVE_BIT(VEH_FLAGS(veh), VEH_ON_FIRE);
		
		act("You put out the fire on $V with $p!", FALSE, ch, cont, veh, TO_CHAR);
		act("$n puts out the fire on $V with $p!", FALSE, ch, cont, veh, TO_ROOM);
		msg_to_vehicle(veh, FALSE, "The flames have been extinguished!\r\n");
	}
}


/**
* Get helper for getting from a vehicle.
*
* @param char_data *ch Person trying to get from the container.
* @param vehicle_data *veh The vehicle to get from.
* @param char *arg The typed argument.
* @param int mode Passed through to perform_get_from_vehicle.
* @param int howmany Number to get.
*/
void do_get_from_vehicle(char_data *ch, vehicle_data *veh, char *arg, int mode, int howmany) {
	obj_data *obj, *next_obj;
	bool found = FALSE;
	int obj_dotmode;
	
	// basic checks
	if (!VEH_FLAGGED(veh, VEH_CONTAINER)) {
		act("$V is not a container.", FALSE, ch, NULL, veh, TO_CHAR);
		return;
	}
	if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to get anything from that.\r\n");
		return;
	}

	obj_dotmode = find_all_dots(arg);
	
	if (obj_dotmode == FIND_INDIV) {
		if (!(obj = get_obj_in_list_vis(ch, arg, VEH_CONTAINS(veh)))) {
			sprintf(buf, "There doesn't seem to be %s %s in $V.", AN(arg), arg);
			act(buf, FALSE, ch, NULL, veh, TO_CHAR);
		}
		else {
			while(obj && howmany--) {
				next_obj = obj->next_content;
				if (!perform_get_from_vehicle(ch, obj, veh, mode)) {
					break;
				}
				obj = get_obj_in_list_vis(ch, arg, next_obj);
			}
		}
	}
	else {
		if (obj_dotmode == FIND_ALLDOT && !*arg) {
			msg_to_char(ch, "Get all of what?\r\n");
			return;
		}
		LL_FOREACH_SAFE2(VEH_CONTAINS(veh), obj, next_obj, next_content) {
			if (CAN_SEE_OBJ(ch, obj) && (obj_dotmode == FIND_ALL || isname(arg, GET_OBJ_KEYWORDS(obj)))) {
				found = TRUE;
				if (!perform_get_from_vehicle(ch, obj, veh, mode)) {
					break;
				}
			}
		}
		if (!found) {
			if (obj_dotmode == FIND_ALL) {
				act("$V seems to be empty.", FALSE, ch, NULL, veh, TO_CHAR);
			}
			else {
				sprintf(buf, "You can't seem to find any %ss in $V.", arg);
				act(buf, FALSE, ch, NULL, veh, TO_CHAR);
			}
		}
	}
}


/**
* command sub-processor for burning a vehicle
*
* @param char_data *ch The person trying to burn a vehicle.
* @param vehicle_data *veh The vehicle to burn.
* @param obj_data *flint The flint.
*/
void do_light_vehicle(char_data *ch, vehicle_data *veh, obj_data *flint) {
	void start_vehicle_burning(vehicle_data *veh);
	
	char buf[MAX_STRING_LENGTH];
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "Mobs can't light vehicles on fire.\r\n");
	}
	else if (!VEH_FLAGGED(veh, VEH_BURNABLE)) {
		msg_to_char(ch, "You can't seem to get it to burn.\r\n");
	}
	else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		msg_to_char(ch, "It is already on fire!\r\n");
	}
	else if (VEH_OWNER(veh) && GET_LOYALTY(ch) != VEH_OWNER(veh) && !has_relationship(GET_LOYALTY(ch), VEH_OWNER(veh), DIPL_WAR)) {
		msg_to_char(ch, "You can't burn %s vehicles unless you're at war.\r\n", EMPIRE_ADJECTIVE(VEH_OWNER(veh)));
	}
	else {
		snprintf(buf, sizeof(buf), "You %s $V on fire!", (flint ? "strike $p and light" : "light"));
		act(buf, FALSE, ch, flint, veh, TO_CHAR);
		snprintf(buf, sizeof(buf), "$n %s $V on fire!", (flint ? "strikes $p and lights" : "lights"));
		act(buf, FALSE, ch, flint, veh, TO_ROOM);
		start_vehicle_burning(veh);
	}
}


/**
* Command processing for a character who is trying to sit in/on a vehicle.
*
* @param char_data *ch The person trying to sit.
* @param char *argument The targeting arg.
*/
void do_sit_on_vehicle(char_data *ch, char *argument) {
	char buf[MAX_STRING_LENGTH];
	vehicle_data *veh;
	
	skip_spaces(&argument);	// usually done ahead of time, but in case
	
	if (!IS_APPROVED(ch) && config_get_bool("travel_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_POS(ch) == POS_FIGHTING) {
		msg_to_char(ch, "You can't really do that right now!\r\n");
	}
	else if (GET_POS(ch) < POS_STANDING || GET_SITTING_ON(ch)) {
		msg_to_char(ch, "You need to stand up before you can do that.\r\n");
	}
	else if (IS_RIDING(ch)) {
		msg_to_char(ch, "You can't do that while mounted.\r\n");
	}
	else if (!(veh = get_vehicle_in_room_vis(ch, argument))) {
		msg_to_char(ch, "You don't see anything like that here.\r\n");
	}
	else if (!VEH_FLAGGED(veh, VEH_SIT)) {
		msg_to_char(ch, "You can't sit on that!\r\n");
	}
	else if (!VEH_IS_COMPLETE(veh)) {
		msg_to_char(ch, "You can't sit %s it until it's finished.\r\n", IN_OR_ON(veh));
	}
	else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		msg_to_char(ch, "You can't sit on it while it's on fire!\r\n");
	}
	else if (VEH_SITTING_ON(veh)) {
		msg_to_char(ch, "%s already sitting %s it.\r\n", (VEH_SITTING_ON(veh) != ch ? "Someone else is" : "You are"), IN_OR_ON(veh));
	}
	else if (VEH_LED_BY(veh)) {
		msg_to_char(ch, "You can't sit %s it while %s leading it around.\r\n", IN_OR_ON(veh), (VEH_LED_BY(veh) == ch) ? "you are" : "someone else is");
	}
	else if (VEH_DRIVER(veh)) {
		msg_to_char(ch, "You can't lead it while someone else is controlling it.\r\n");
	}
	else if (!can_use_vehicle(ch, veh, MEMBERS_AND_ALLIES)) {
		msg_to_char(ch, "You don't have permission to sit %s that.\r\n", IN_OR_ON(veh));
	}
	else if (GET_LEADING_VEHICLE(ch) || GET_LEADING_MOB(ch)) {
		msg_to_char(ch, "You can't sit %s it while you're leading something.\r\n", IN_OR_ON(veh));
	}
	else {
		snprintf(buf, sizeof(buf), "You sit %s $V.", IN_OR_ON(veh));
		act(buf, FALSE, ch, NULL, veh, TO_CHAR);
		
		snprintf(buf, sizeof(buf), "$n sits %s $V.", IN_OR_ON(veh));
		act(buf, FALSE, ch, NULL, veh, TO_ROOM);
		
		sit_on_vehicle(ch, veh);
		GET_POS(ch) = POS_SITTING;
	}
}


/**
* Processor for "put [number] <obj(s)> <vehicle>"
*
* @param char_data *ch The player.
* @param vehicle_data *veh Which vehicle.
* @param int dotmode Detected FIND_ dotmode ("all.obj").
* @param char *arg The object name typed by the player.
* @param int howmany Number to put (1+).
*/
void do_put_obj_in_vehicle(char_data *ch, vehicle_data *veh, int dotmode, char *arg, int howmany) {
	obj_data *obj, *next_obj;
	bool multi = (howmany > 1);
	bool found = FALSE;
	
	if (!VEH_FLAGGED(veh, VEH_CONTAINER)) {
		act("$V is not a container.", FALSE, ch, NULL, veh, TO_CHAR);
		return;
	}
	if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to put anything in there, and you wouldn't be able to get it out again.\r\n");
		return;
	}
	
	if (dotmode == FIND_INDIV) {	// put <obj> <vehicle>
		if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
			msg_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg), arg);
		}
		else if (multi && OBJ_FLAGGED(obj, OBJ_KEEP)) {
			msg_to_char(ch, "You marked that 'keep' and can't put it in anything unless you unkeep it.\r\n");
		}
		else {
			while (obj && howmany--) {
				next_obj = obj->next_content;
				
				if (multi && OBJ_FLAGGED(obj, OBJ_KEEP)) {
					continue;
				}
				
				if (!perform_put_obj_in_vehicle(ch, obj, veh)) {
					break;
				}
				
				obj = get_obj_in_list_vis(ch, arg, next_obj);
				found = TRUE;
			}
			
			if (!found) {
				msg_to_char(ch, "You didn't seem to have any that aren't marked 'keep'.\r\n");
			}
		}
	}
	else {
		LL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
			if (CAN_SEE_OBJ(ch, obj) && (dotmode == FIND_ALL || isname(arg, GET_OBJ_KEYWORDS(obj)))) {
				if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
					continue;
				}
				found = TRUE;
				if (!perform_put_obj_in_vehicle(ch, obj, veh)) {
					break;
				}
			}
		}
		if (!found) {
			if (dotmode == FIND_ALL)
				msg_to_char(ch, "You don't seem to have any non-keep items to put in it.\r\n");
			else {
				msg_to_char(ch, "You don't seem to have any %ss that aren't marked 'keep'.\r\n", arg);
			}
		}
	}
}


/**
* Processes a request to remove a character from a chair/vehicle and sends
* a message. Exits early if the character is not actually in/on a vehicle.
*
* @param char_data *ch The person getting up.
*/
void do_unseat_from_vehicle(char_data *ch) {
	char buf[MAX_STRING_LENGTH];
	
	if (!GET_SITTING_ON(ch)) {
		return;
	}
	
	snprintf(buf, sizeof(buf), "You get up %s of $V.", VEH_FLAGGED(GET_SITTING_ON(ch), VEH_IN) ? "out" : "off");
	act(buf, FALSE, ch, NULL, GET_SITTING_ON(ch), TO_CHAR);

	snprintf(buf, sizeof(buf), "$n gets up %s of $V.", VEH_FLAGGED(GET_SITTING_ON(ch), VEH_IN) ? "out" : "off");
	act(buf, TRUE, ch, NULL, GET_SITTING_ON(ch), TO_ROOM);

	unseat_char_from_vehicle(ch);
	if (GET_POS(ch) == POS_SITTING) {
		GET_POS(ch) = FIGHTING(ch) ? POS_FIGHTING : POS_STANDING;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_board) {
	char *command = (subcmd == SCMD_ENTER ? "enter" : "board");
	room_data *was_in = IN_ROOM(ch), *to_room;
	char buf[MAX_STRING_LENGTH];
	struct vehicle_data *veh;
	struct follow_type *k;

	one_argument(argument, arg);

	if (IS_NPC(ch) && AFF_FLAGGED(ch, AFF_ORDERED)) {
		return;
	}
	else if (!IS_APPROVED(ch) && config_get_bool("travel_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!IS_IMMORTAL(ch) && !IS_NPC(ch) && IS_CARRYING_N(ch) > CAN_CARRY_N(ch)) {
		msg_to_char(ch, "You are overburdened and cannot move.\r\n");
	}
	else if (!*arg) {
		snprintf(buf, sizeof(buf), "%s what?\r\n", command);
		send_to_char(CAP(buf), ch);
	}
	else if (!(veh = get_vehicle_in_room_vis(ch, arg))) {
		msg_to_char(ch, "You don't see %s %s here.\r\n", AN(arg), arg);
	}
	else if (!VEH_INTERIOR_HOME_ROOM(veh) && VEH_INTERIOR_ROOM_VNUM(veh) == NOTHING) {
		// this is a pre-check
		msg_to_char(ch, "You can't %s that!\r\n", command);
	}
	else if (!VEH_IS_COMPLETE(veh)) {
		msg_to_char(ch, "You can't %s it until it's finished.\r\n", command);
	}
	else if (!(to_room = get_vehicle_interior(veh))) {
		msg_to_char(ch, "You can't seem to %s it.\r\n", command);
	}
	else if (!can_use_vehicle(ch, veh, MEMBERS_AND_ALLIES)) {
		msg_to_char(ch, "You don't have permission to %s it.\r\n", command);
	}
	else if (IS_RIDING(ch) && !ROOM_BLD_FLAGGED(to_room, BLD_ALLOW_MOUNTS)) {
		msg_to_char(ch, "You can't %s that while riding.\r\n", command);
	}
	else if (GET_LEADING_MOB(ch) && IN_ROOM(GET_LEADING_MOB(ch)) == IN_ROOM(ch) && !VEH_FLAGGED(veh, VEH_CARRY_MOBS)) {
		msg_to_char(ch, "You can't %s it while leading an animal.\r\n", command);
	}
	else if (GET_LEADING_VEHICLE(ch) && IN_ROOM(GET_LEADING_VEHICLE(ch)) == IN_ROOM(ch) && !VEH_FLAGGED(veh, VEH_CARRY_VEHICLES)) {
		msg_to_char(ch, "You can't %s it while leading another vehicle.\r\n", command);
	}
	else if (GET_LEADING_VEHICLE(ch) && (VEH_FLAGGED(GET_LEADING_VEHICLE(ch), VEH_NO_LOAD_ONTO_VEHICLE) || !VEH_FLAGGED(veh, VEH_CARRY_VEHICLES))) {
		act("You can't lead $V in there.", FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_CHAR);
	}
	else {
		// move ch: out-message
		snprintf(buf, sizeof(buf), "You %s $V.", command);
		act(buf, FALSE, ch, NULL, veh, TO_CHAR);
		if (!AFF_FLAGGED(ch, AFF_SNEAK)) {
			snprintf(buf, sizeof(buf), "$n %ss $V.", command);
			act(buf, TRUE, ch, NULL, veh, TO_ROOM);
		}
		
		// move ch
		char_to_room(ch, to_room);
		qt_visit_room(ch, IN_ROOM(ch));
		if (!IS_NPC(ch)) {
			GET_LAST_DIR(ch) = NO_DIR;
		}
		look_at_room(ch);
		
		// move ch: in-message
		if (!AFF_FLAGGED(ch, AFF_SNEAK)) {
			snprintf(buf, sizeof(buf), "$n %ss.", command);
			act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
		}
		
		// move ch: triggers
		enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
		entry_memory_mtrigger(ch);
		greet_mtrigger(ch, NO_DIR);
		greet_memory_mtrigger(ch);
		greet_vtrigger(ch, NO_DIR);
		
		// leading-mob
		if (GET_LEADING_MOB(ch) && IN_ROOM(GET_LEADING_MOB(ch)) == was_in) {
			if (!AFF_FLAGGED(GET_LEADING_MOB(ch), AFF_SNEAK)) {
				act("$n follows $N.", TRUE, GET_LEADING_MOB(ch), NULL, ch, TO_NOTVICT);
			}
			
			char_to_room(GET_LEADING_MOB(ch), to_room);
			if (!IS_NPC(GET_LEADING_MOB(ch))) {
				GET_LAST_DIR(GET_LEADING_MOB(ch)) = NO_DIR;
			}
			look_at_room(GET_LEADING_MOB(ch));
			
			if (!AFF_FLAGGED(GET_LEADING_MOB(ch), AFF_SNEAK)) {
				snprintf(buf, sizeof(buf), "$n %ss.", command);
				act(buf, TRUE, GET_LEADING_MOB(ch), NULL, NULL, TO_ROOM);
			}
			
			enter_wtrigger(IN_ROOM(GET_LEADING_MOB(ch)), GET_LEADING_MOB(ch), NO_DIR);
			entry_memory_mtrigger(GET_LEADING_MOB(ch));
			greet_mtrigger(GET_LEADING_MOB(ch), NO_DIR);
			greet_memory_mtrigger(GET_LEADING_MOB(ch));
			greet_vtrigger(GET_LEADING_MOB(ch), NO_DIR);
		}
		
		// leading-vehicle
		if (GET_LEADING_VEHICLE(ch) && IN_ROOM(GET_LEADING_VEHICLE(ch)) == was_in) {
			if (ROOM_PEOPLE(was_in)) {
				act("$v is led behind $M.", TRUE, ROOM_PEOPLE(was_in), GET_LEADING_VEHICLE(ch), ch, TO_CHAR | TO_NOTVICT | ACT_VEHICLE_OBJ);
			}
			
			vehicle_to_room(GET_LEADING_VEHICLE(ch), to_room);
			act("$V is led in.", TRUE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_CHAR | TO_ROOM | ACT_VEHICLE_OBJ);
		}
		
		// followers?
		for (k = ch->followers; k; k = k->next) {
			if (IN_ROOM(k->follower) != was_in) {
				continue;
			}
			if (GET_POS(k->follower) < POS_STANDING) {
				continue;
			}
			if (!IS_IMMORTAL(k->follower) && !IS_NPC(k->follower) && IS_CARRYING_N(k->follower) > CAN_CARRY_N(k->follower)) {
				continue;
			}
		
			act("You follow $N.\r\n", FALSE, k->follower, NULL, ch, TO_CHAR);
			snprintf(buf, sizeof(buf), "$n %ss $V.", command);
			act(buf, TRUE, k->follower, NULL, veh, TO_ROOM);

			char_to_room(k->follower, to_room);
			qt_visit_room(k->follower, IN_ROOM(k->follower));
			if (!IS_NPC(k->follower)) {
				GET_LAST_DIR(k->follower) = NO_DIR;
			}
			look_at_room(k->follower);
			
			snprintf(buf, sizeof(buf), "$n %ss.", command);
			act(buf, TRUE, k->follower, NULL, NULL, TO_ROOM);
			
			enter_wtrigger(IN_ROOM(k->follower), k->follower, NO_DIR);
			entry_memory_mtrigger(k->follower);
			greet_mtrigger(k->follower, NO_DIR);
			greet_memory_mtrigger(k->follower);
			greet_vtrigger(k->follower, NO_DIR);
		}
		
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_disembark) {
	vehicle_data *veh = GET_ROOM_VEHICLE(IN_ROOM(ch));
	room_data *was_in = IN_ROOM(ch), *to_room;
	struct follow_type *k;

	if (!IS_APPROVED(ch) && config_get_bool("travel_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!veh || !(to_room = IN_ROOM(veh))) {
		msg_to_char(ch, "You can't disembark from here!\r\n");
	}
	else if (!IS_IMMORTAL(ch) && !IS_NPC(ch) && IS_CARRYING_N(ch) > CAN_CARRY_N(ch)) {
		msg_to_char(ch, "You are overburdened and cannot move.\r\n");
	}
	else if (IS_RIDING(ch) && !ROOM_BLD_FLAGGED(to_room, BLD_ALLOW_MOUNTS)) {
		msg_to_char(ch, "You can't disembark here while riding.\r\n");
	}
	else {
		if (!AFF_FLAGGED(ch, AFF_SNEAK)) {
			act("$n disembarks from $V.", TRUE, ch, NULL, veh, TO_ROOM);
		}
		msg_to_char(ch, "You disembark.\r\n");
		
		char_to_room(ch, to_room);
		qt_visit_room(ch, IN_ROOM(ch));
		if (!IS_NPC(ch)) {
			GET_LAST_DIR(ch) = NO_DIR;
		}
		look_at_room(ch);
		
		if (!AFF_FLAGGED(ch, AFF_SNEAK)) {
			act("$n disembarks from $V.", TRUE, ch, NULL, veh, TO_ROOM);
		}
		
		enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
		entry_memory_mtrigger(ch);
		greet_mtrigger(ch, NO_DIR);
		greet_memory_mtrigger(ch);
		greet_vtrigger(ch, NO_DIR);
		
		if (GET_LEADING_MOB(ch) && IN_ROOM(GET_LEADING_MOB(ch)) == was_in) {
			act("$n is led off.", TRUE, GET_LEADING_MOB(ch), NULL, NULL, TO_ROOM);
			
			char_to_room(GET_LEADING_MOB(ch), to_room);
			if (!IS_NPC(GET_LEADING_MOB(ch))) {
				GET_LAST_DIR(GET_LEADING_MOB(ch)) = NO_DIR;
			}
			look_at_room(GET_LEADING_MOB(ch));
			
			if (!AFF_FLAGGED(GET_LEADING_MOB(ch), AFF_SNEAK)) {
				act("$n disembarks from $V.", TRUE, GET_LEADING_MOB(ch), NULL, veh, TO_ROOM);
			}
			
			enter_wtrigger(IN_ROOM(GET_LEADING_MOB(ch)), GET_LEADING_MOB(ch), NO_DIR);
			entry_memory_mtrigger(GET_LEADING_MOB(ch));
			greet_mtrigger(GET_LEADING_MOB(ch), NO_DIR);
			greet_memory_mtrigger(GET_LEADING_MOB(ch));
			greet_vtrigger(GET_LEADING_MOB(ch), NO_DIR);
		}
		if (GET_LEADING_VEHICLE(ch) && IN_ROOM(GET_LEADING_VEHICLE(ch)) == was_in) {
			if (ROOM_PEOPLE(was_in)) {
				act("$v is led behind $M.", TRUE, ROOM_PEOPLE(was_in), GET_LEADING_VEHICLE(ch), ch, TO_CHAR | TO_NOTVICT | ACT_VEHICLE_OBJ);
			}
			vehicle_to_room(GET_LEADING_VEHICLE(ch), to_room);
			act("$V is led off.", TRUE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_CHAR | TO_ROOM | ACT_VEHICLE_OBJ);
		}

		for (k = ch->followers; k; k = k->next) {
			if (IN_ROOM(k->follower) != was_in) {
				continue;
			}
			if (GET_POS(k->follower) < POS_STANDING) {
				continue;
			}
			if (!IS_IMMORTAL(k->follower) && !IS_NPC(k->follower) && IS_CARRYING_N(k->follower) > CAN_CARRY_N(k->follower)) {
				continue;
			}
		
			act("You follow $N.\r\n", FALSE, k->follower, NULL, ch, TO_CHAR);
			if (!AFF_FLAGGED(k->follower, AFF_SNEAK)) {
				act("$n disembarks from $V.", TRUE, k->follower, NULL, veh, TO_ROOM);
			}

			char_to_room(k->follower, to_room);
			qt_visit_room(k->follower, IN_ROOM(k->follower));
			if (!IS_NPC(k->follower)) {
				GET_LAST_DIR(k->follower) = NO_DIR;
			}
			look_at_room(k->follower);
			
			if (!AFF_FLAGGED(k->follower, AFF_SNEAK)) {
				act("$n disembarks from $V.", TRUE, k->follower, NULL, veh, TO_ROOM);
			}
			
			enter_wtrigger(IN_ROOM(k->follower), k->follower, NO_DIR);
			entry_memory_mtrigger(k->follower);
			greet_mtrigger(k->follower, NO_DIR);
			greet_memory_mtrigger(k->follower);
			greet_vtrigger(k->follower, NO_DIR);
		}
		
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_dispatch) {
	extern char_data *find_chore_worker_in_room(room_data *room, mob_vnum vnum);
	extern room_data *find_docks(empire_data *emp, int island_id);
	extern struct empire_npc_data *find_free_npc_for_chore(empire_data *emp, room_data *loc);
	extern int find_free_shipping_id(empire_data *emp);
	void sail_shipment(empire_data *emp, vehicle_data *boat);
	extern bool ship_is_empty(vehicle_data *ship);
	extern char_data *spawn_empire_npc_to_room(empire_data *emp, struct empire_npc_data *npc, room_data *room, mob_vnum override_mob);

	struct island_info *to_isle;
	char targ[MAX_INPUT_LENGTH];
	struct empire_npc_data *npc;
	struct shipping_data *shipd;
	char_data *worker;
	vehicle_data *veh;
	
	argument = one_argument(argument, targ);
	skip_spaces(&argument);
	
	if (IS_NPC(ch) || !GET_LOYALTY(ch)) {
		msg_to_char(ch, "You can't dispatch any ships.\r\n");
	}
	else if (GET_RANK(ch) < EMPIRE_PRIV(GET_LOYALTY(ch), PRIV_SHIPPING)) {
		msg_to_char(ch, "You don't have permission to dispatch ships.\r\n");
	}
	else if (!*targ || !*argument) {
		msg_to_char(ch, "Dispatch which ship to which island?\r\n");
	}
	
	// vehicle validation
	else if (!(veh = find_ship_to_dispatch(ch, targ))) {
		msg_to_char(ch, "You can't find any ship like that to dispatch on this island.\r\n");
	}
	else if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to dispatch that.\r\n");
	}
	else if (!VEH_FLAGGED(veh, VEH_SHIPPING)) {
		msg_to_char(ch, "You can only dispatch shipping vessels.\r\n");
	}
	else if (!VEH_IS_COMPLETE(veh)) {
		msg_to_char(ch, "It isn't even built yet.\r\n");
	}
	else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		msg_to_char(ch, "You can't dispatch it while it's on fire!\r\n");
	}
	else if (VEH_SHIPPING_ID(veh) != -1) {
		msg_to_char(ch, "It's already being used for shipping.\r\n");
	}
	else if (!ship_is_empty(veh)) {
		msg_to_char(ch, "You can't dispatch a ship that has people inside it.\r\n");
	}
	else if (!WATER_SECT(IN_ROOM(veh)) && !IS_WATER_BUILDING(IN_ROOM(veh))) {
		msg_to_char(ch, "You can only dispatch ships that are on the water or in docks.\r\n");
	}
	
	// destination validation
	else if (GET_ISLAND_ID(IN_ROOM(veh)) == NO_ISLAND) {
		msg_to_char(ch, "You can't automatically dispatch ships that are out at sea.\r\n");
	}
	else if (!(to_isle = get_island_by_name(ch, argument)) && !(to_isle = get_island_by_coords(argument))) {
		msg_to_char(ch, "Unknown target island \"%s\".\r\n", argument);
	}
	else if (to_isle->id == GET_ISLAND_ID(IN_ROOM(veh)) && HAS_FUNCTION(IN_ROOM(veh), FNC_DOCKS)) {
		msg_to_char(ch, "It is already docked on that island.\r\n");
	}
	else if (!find_docks(GET_LOYALTY(ch), to_isle->id)) {
		msg_to_char(ch, "%s has no docks (docks must not be set no-work).\r\n", to_isle->name);
	}
	
	// ready ready go
	else {
		if (!(worker = find_chore_worker_in_room(IN_ROOM(veh), OVERSEER))) {
			if ((npc = find_free_npc_for_chore(GET_LOYALTY(ch), IN_ROOM(veh)))) {
				worker = spawn_empire_npc_to_room(GET_LOYALTY(ch), npc, IN_ROOM(veh), OVERSEER);
			}
		}
		
		// still no worker?
		if (!worker) {
			msg_to_char(ch, "There are no free workers to oversee the dispatch.\r\n");
			return;
		}
		
		// ensure a despawn
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
		
		// create shipment
		CREATE(shipd, struct shipping_data, 1);
		shipd->vnum = NOTHING;
		shipd->from_island = GET_ISLAND_ID(IN_ROOM(veh));
		shipd->to_island = to_isle->id;
		shipd->status = SHIPPING_QUEUED;
		shipd->status_time = time(0);
		shipd->ship_origin = GET_ROOM_VNUM(IN_ROOM(veh));
		
		VEH_SHIPPING_ID(veh) = find_free_shipping_id(GET_LOYALTY(ch));
		shipd->shipping_id = VEH_SHIPPING_ID(veh);
		
		LL_APPEND(EMPIRE_SHIPPING_LIST(GET_LOYALTY(ch)), shipd);
		sail_shipment(GET_LOYALTY(ch), veh);
		send_config_msg(ch, "ok_string");
	}
}


/**
* Attempts to drag an object through a portal. This is a sub-function of
* do_drag. It is called when no direction matched the drag command.
*
* @param char_data *ch The player.
* @param vehicle_data *veh The vehicle being dragged.
* @param char *arg The typed-in arg.
*/
void do_drag_portal(char_data *ch, vehicle_data *veh, char *arg) {
	extern bool can_enter_portal(char_data *ch, obj_data *portal, bool allow_infiltrate, bool skip_permissions);
	void char_through_portal(char_data *ch, obj_data *portal, bool following);

	room_data *was_in, *to_room;
	obj_data *portal;
	
	if (!*arg || !(portal = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "Which direction is that?\r\n");
	}
	else if (!IS_PORTAL(portal) || !(to_room = real_room(GET_PORTAL_TARGET_VNUM(portal)))) {
		msg_to_char(ch, "You can't drag it into that!\r\n");
	}
	else if (!can_enter_portal(ch, portal, FALSE, FALSE)) {
		// sends own message
	}
	else if (!VEH_FLAGGED(veh, VEH_CAN_PORTAL)) {
		act("$V can't be dragged through portals.", FALSE, ch, NULL, veh, TO_CHAR);
	}
	else if (WATER_SECT(to_room)) {
		msg_to_char(ch, "You can't drag it through there because it can't be dragged into water.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !VEH_FLAGGED(veh, VEH_ALLOW_ROUGH)) {
		msg_to_char(ch, "You can't drag it through there because of rough terrain on the other side.\r\n");
	}
	else if (ROOM_IS_CLOSED(to_room) && VEH_FLAGGED(veh, VEH_NO_BUILDING)) {
		msg_to_char(ch, "You can't drag it in there.\r\n");
	}
	else if (GET_ROOM_VEHICLE(to_room) && (VEH_FLAGGED(veh, VEH_NO_LOAD_ONTO_VEHICLE) || !VEH_FLAGGED(GET_ROOM_VEHICLE(to_room), VEH_CARRY_VEHICLES))) {
		msg_to_char(ch, "You can't drag it in there.\r\n");
	}
	else {
		was_in = IN_ROOM(ch);
		char_through_portal(ch, portal, FALSE);
		
		if (IN_ROOM(ch) == was_in) {
			// failure here would have sent its own message
			return;
		}
		
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			act("$V is dragged into $p.", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), portal, veh, TO_CHAR | TO_ROOM);
		}
		
		vehicle_to_room(veh, IN_ROOM(ch));
		act("$V is dragged in with you.", FALSE, ch, NULL, veh, TO_CHAR);
		act("$V is dragged in with $m.", FALSE, ch, NULL, veh, TO_ROOM);
	}
}


ACMD(do_drag) {
	char what[MAX_INPUT_LENGTH], where[MAX_INPUT_LENGTH];
	room_data *to_room, *was_in;
	vehicle_data *veh;
	int dir;
	
	two_arguments(argument, what, where);
	
	if (GET_LEADING_MOB(ch) || GET_LEADING_VEHICLE(ch)) {
		msg_to_char(ch, "You can't drag anything while you're leading something.\r\n");
	}
	else if (GET_SITTING_ON(ch)) {
		msg_to_char(ch, "You can't drag anything while you're sitting on something.\r\n");
	}
	else if (WATER_SECT(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't drag anything in the water.\r\n");
	}
	else if (AFF_FLAGGED(ch, AFF_CHARM) && ch->master && IN_ROOM(ch) == IN_ROOM(ch->master)) {
		msg_to_char(ch, "The thought of leaving your master makes you weep.\r\n");
		act("$n bursts into tears.", FALSE, ch, NULL, NULL, TO_ROOM);
	}
	else if (!*what || !*where) {
		msg_to_char(ch, "Drag what, in which direction?\r\n");
	}
	// vehicle validation
	else if (!(veh = get_vehicle_in_room_vis(ch, what))) {
		msg_to_char(ch, "You don't see %s %s here.\r\n", AN(what), what);
	}
	else if (!VEH_FLAGGED(veh, VEH_DRAGGABLE)) {
		msg_to_char(ch, "You can't drag that!\r\n");
	}
	else if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to drag that.\r\n");
	}
	else if (!VEH_IS_COMPLETE(veh)) {
		msg_to_char(ch, "You can't drag that around until it's finished.\r\n");
	}
	else if (count_harnessed_animals(veh) > 0) {
		msg_to_char(ch, "You can't drag that while animals are harnessed to it.\r\n");
	}
	else if (VEH_SITTING_ON(veh)) {
		msg_to_char(ch, "You can't drag it while someone is sitting on it.\r\n");
	}
	else if (VEH_LED_BY(veh)) {
		msg_to_char(ch, "You can't drag it while someone is leading it.\r\n");
	}
	// direction validation
	else if ((dir = parse_direction(ch, where)) == NO_DIR) {
		do_drag_portal(ch, veh, where);
	}
	else if (!(to_room = dir_to_room(IN_ROOM(ch), dir, FALSE))) {
		msg_to_char(ch, "You can't drag anything in that direction.\r\n");
	}
	else if (WATER_SECT(to_room)) {
		msg_to_char(ch, "You can't drag anything into the water.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !VEH_FLAGGED(veh, VEH_ALLOW_ROUGH)) {
		msg_to_char(ch, "You can't drag it on such rough terrain.\r\n");
	}
	else if (ROOM_IS_CLOSED(to_room) && VEH_FLAGGED(veh, VEH_NO_BUILDING)) {
		msg_to_char(ch, "You can't drag it in there.\r\n");
	}
	else {
		// seems okay enough -- try movement
		was_in = IN_ROOM(ch);
		if (!perform_move(ch, dir, FALSE, 0) || IN_ROOM(ch) == was_in) {
			// failure here would have sent its own message
			return;
		}
		
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			act("$V is dragged along.", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM);
		}
		
		vehicle_to_room(veh, IN_ROOM(ch));
		act("$V is dragged along with you.", FALSE, ch, NULL, veh, TO_CHAR);
		act("$V is dragged along with $m.", FALSE, ch, NULL, veh, TO_ROOM);
	}
}


/**
* Attempts to drive/sail a vehicle through a portal.
*
* @param char_data *ch The driver.
* @param vehicle_data *veh The vehicle.
* @param obj_data *portal The portal.
* @param int subcmd The original subcommand passed to do_drive.
*/
void do_drive_through_portal(char_data *ch, vehicle_data *veh, obj_data *portal, int subcmd) {
	extern bool can_enter_portal(char_data *ch, obj_data *portal, bool allow_infiltrate, bool skip_permissions);
	extern obj_data *find_back_portal(room_data *in_room, room_data *from_room, obj_data *fallback);
	void give_portal_sickness(char_data *ch, obj_data *portal, room_data *from, room_data *to);
	
	room_data *to_room, *was_in = IN_ROOM(veh);
	struct vehicle_room_list *vrl;
	char_data *ch_iter, *next_ch;
	char buf[MAX_STRING_LENGTH];
	
	if (!IS_PORTAL(portal) || !(to_room = real_room(GET_PORTAL_TARGET_VNUM(portal)))) {
		act("You can't seem to go through $p.", FALSE, ch, NULL, NULL, TO_CHAR);
	}
	else if (!VEH_FLAGGED(veh, VEH_CAN_PORTAL)) {
		act("$V can't go through portals.", FALSE, ch, NULL, veh, TO_CHAR);
	}
	else if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
		act("You don't have permission to use $V.", FALSE, ch, NULL, veh, TO_CHAR);
	}
	else if (IN_ROOM(veh) != IN_ROOM(portal)) {
		snprintf(buf, sizeof(buf), "You can't %s through $p because it's not in the same room as $V.", drive_data[subcmd].command);
		act(buf, FALSE, ch, portal, veh, TO_CHAR);
	}
	else if (!can_enter_portal(ch, portal, FALSE, FALSE)) {
		// sends own message
	}
	else if (!validate_vehicle_move(ch, veh, to_room)) {
		// sends own message
	}
	else {
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			snprintf(buf, sizeof(buf), "$V %s through $p.", mob_move_types[VEH_MOVE_TYPE(veh)]);
			act(buf, FALSE, ROOM_PEOPLE(IN_ROOM(veh)), portal, veh, TO_CHAR | TO_ROOM);
		}
		
		vehicle_to_room(veh, to_room);
		
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			snprintf(buf, sizeof(buf), "$V %s out of $p.", mob_move_types[VEH_MOVE_TYPE(veh)]);
			act(buf, FALSE, ROOM_PEOPLE(IN_ROOM(veh)), find_back_portal(to_room, was_in, portal), veh, TO_CHAR | TO_ROOM);
		}
		
		if (VEH_SITTING_ON(veh)) {
			char_to_room(VEH_SITTING_ON(veh), to_room);
			look_at_room(VEH_SITTING_ON(veh));
		}
		
		// stop driving after
		if (GET_ACTION(ch) == drive_data[subcmd].action) {
			cancel_action(ch);
		}
		
		// give portal sickness to everyone
		if (VEH_SITTING_ON(veh)) {
			give_portal_sickness(VEH_SITTING_ON(veh), portal, was_in, to_room);
		}
		if (VEH_ROOM_LIST(veh)) {
			snprintf(buf, sizeof(buf), "$V %s through $p.", mob_move_types[VEH_MOVE_TYPE(veh)]);
			LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
				LL_FOREACH_SAFE2(ROOM_PEOPLE(vrl->room), ch_iter, next_ch, next_in_room) {
					give_portal_sickness(ch_iter, portal, was_in, to_room);
					act(buf, FALSE, ch_iter, portal, veh, TO_CHAR | TO_SPAMMY);
				}
			}
		}
	}
}


// do_sail, do_pilot (search hints)
ACMD(do_drive) {
	char dir_arg[MAX_INPUT_LENGTH], dist_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	struct vehicle_room_list *vrl;
	bool was_driving, same_dir;
	char_data *ch_iter;
	vehicle_data *veh;
	obj_data *portal;
	int dir, dist = -1;

	// 2nd arg (dist) is optional
	two_arguments(argument, dir_arg, dist_arg);
	
	// basics
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!*dir_arg && GET_ACTION(ch) == drive_data[subcmd].action) {
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE && GET_ACTION(ch) != drive_data[subcmd].action) {
		msg_to_char(ch, "You're too busy doing something else.\r\n");
	}
	
	// vehicle verification
	else if (!(veh = GET_SITTING_ON(ch)) && !(veh = GET_ROOM_VEHICLE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You can't %s anything here.\r\n", drive_data[subcmd].command);
	}
	else if (!VEH_FLAGGED(veh, drive_data[subcmd].flag)) {
		snprintf(buf, sizeof(buf), "You can't %s $V!", drive_data[subcmd].command);
		act(buf, FALSE, ch, NULL, veh, TO_CHAR);
	}
	else if (!VEH_IS_COMPLETE(veh)) {
		act("$V isn't going anywhere until it's finished.", FALSE, ch, NULL, veh, TO_CHAR);
	}
	else if (count_harnessed_animals(veh) < VEH_ANIMALS_REQUIRED(veh)) {
		msg_to_char(ch, "You must harness %d more animal%s to it first.\r\n", (VEH_ANIMALS_REQUIRED(veh) - count_harnessed_animals(veh)), PLURAL(VEH_ANIMALS_REQUIRED(veh) - count_harnessed_animals(veh)));
	}
	else if (veh == GET_ROOM_VEHICLE(IN_ROOM(ch)) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_LOOK_OUT)) {
		msg_to_char(ch, "You can't %s here because you can't see outside.\r\n", drive_data[subcmd].command);
	}
	else if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to %s this.\r\n", drive_data[subcmd].command);
	}
	else if (VEH_DRIVER(veh) && VEH_DRIVER(veh) != ch) {
		msg_to_char(ch, "Someone else is %s it right now.\r\n", drive_data[subcmd].verb);
	}
	else if (VEH_SITTING_ON(veh) && VEH_SITTING_ON(veh) != ch) {
		msg_to_char(ch, "You can't %s it while someone else is sitting %s it.\r\n", drive_data[subcmd].command, IN_OR_ON(veh));
	}
	else if (VEH_LED_BY(veh) && VEH_LED_BY(veh) != ch) {
		msg_to_char(ch, "You can't %s it while someone else is leading it.\r\n", drive_data[subcmd].command);
	}
	else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		msg_to_char(ch, "You can't %s it while it's on fire!\r\n", drive_data[subcmd].command);
	}
	
	// target arg
	else if (!*dir_arg) {
		msg_to_char(ch, "Which direction would you like to %s?\r\n", drive_data[subcmd].command);
	}
	else if ((dir = parse_direction(ch, dir_arg)) == NO_DIR) {
		if ((portal = get_obj_in_list_vis(ch, dir_arg, ROOM_CONTENTS(IN_ROOM(veh)))) && IS_PORTAL(portal)) {
			do_drive_through_portal(ch, veh, portal, subcmd);
		}
		else {
			msg_to_char(ch, "'%s' isn't a direction you can %s.\r\n", dir_arg, drive_data[subcmd].command);
		}
	}
	else if (dir == DIR_RANDOM || !dir_to_room(IN_ROOM(veh), dir, FALSE) || (subcmd != SCMD_PILOT && !is_flat_dir[dir])) {
		msg_to_char(ch, "You can't %s that direction.\r\n", drive_data[subcmd].command);
	}
	else if (GET_ACTION(ch) == drive_data[subcmd].action && GET_ACTION_VNUM(ch, 0) == dir && !*dist_arg) {
		msg_to_char(ch, "You are already %s that way.\r\n", drive_data[subcmd].verb);
	}
	else if (*dist_arg && (!isdigit(*dist_arg) || (dist = atoi(dist_arg)) < 1)) {
		snprintf(buf, sizeof(buf), "%s how far!?\r\n", drive_data[subcmd].command);
		CAP(buf);
		send_to_char(buf, ch);
	}
	else {
		// 'dir' is the way we are ACTUALLY going, but we store the direction the character thinks it is
		
		was_driving = (GET_ACTION(ch) == drive_data[subcmd].action);
		same_dir = (was_driving && (get_direction_for_char(ch, dir) == GET_ACTION_VNUM(ch, 0)));
		GET_ACTION(ch) = ACT_NONE;	// prevents a stops-moving message
		start_action(ch, drive_data[subcmd].action, 0);
		GET_ACTION_VNUM(ch, 0) = get_direction_for_char(ch, dir);
		GET_ACTION_VNUM(ch, 1) = dist;	// may be -1 for continuous
		GET_ACTION_VNUM(ch, 2) = subcmd;
		
		GET_DRIVING(ch) = veh;
		VEH_DRIVER(veh) = ch;
		
		if (same_dir) {
			msg_to_char(ch, "You will now stop after %d tiles.\r\n", dist);
		}
		else if (was_driving) {
			msg_to_char(ch, "You turn %s.\r\n", dirs[get_direction_for_char(ch, dir)]);
		}
		else {
			msg_to_char(ch, "You start %s %s.\r\n", drive_data[subcmd].verb, dirs[get_direction_for_char(ch, dir)]);
		}
		
		// alert whole vehicle
		if (!same_dir && VEH_ROOM_LIST(veh)) {
			LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
				LL_FOREACH2(ROOM_PEOPLE(vrl->room), ch_iter, next_in_room) {
					if (ch_iter != ch && ch_iter->desc) {
						snprintf(buf, sizeof(buf), "$V %s %s.", (was_driving ? "turns to the" : "begins to move"), dirs[get_direction_for_char(ch_iter, dir)]);
						act(buf, FALSE, ch_iter, NULL, veh, TO_CHAR);
					}
				}
			}
		}
	}
}


ACMD(do_fire) {
	void besiege_room(room_data *to_room, int damage);
	bool besiege_vehicle(vehicle_data *veh, int damage, int siege_type);
	
	char veh_arg[MAX_INPUT_LENGTH], tar_arg[MAX_INPUT_LENGTH];
	vehicle_data *veh, *veh_targ;
	sector_data *secttype;
	room_data *room_targ;
	int dam, diff, dir;
	char_data *vict;
	
	static struct resource_data *ammo = NULL;
	
	if (!ammo) {
		add_to_resource_list(&ammo, RES_OBJECT, o_HEAVY_SHOT, 1, 0);
	}
	
	two_arguments(argument, veh_arg, tar_arg);
	
	// basic checks
	if (GET_POS(ch) == POS_FIGHTING) {
		// only POS_SITTING is required so people can fire while sitting on a vehicle
		msg_to_char(ch, "You're too busy fighting!\r\n");
	}
	else if (!*veh_arg || !*tar_arg) {
		msg_to_char(ch, "Usage: fire <vehicle> <direction | target vehicle>\r\n");
	}
	else if (!has_resources(ch, ammo, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE)) {
		// sends own error message
	}
	
	// find what to fire with
	else if (!(veh = get_vehicle_in_room_vis(ch, veh_arg)) && (!(veh = GET_ROOM_VEHICLE(IN_ROOM(ch))) || !isname(veh_arg, VEH_KEYWORDS(veh)))) {
		msg_to_char(ch, "You don't see %s %s here to fire.\r\n", AN(veh_arg), veh_arg);
	}
	else if (!VEH_FLAGGED(veh, VEH_SIEGE_WEAPONS)) {
		act("$V has no siege weapons.", FALSE, ch, NULL, veh, TO_CHAR);
	}
	else if (!can_use_vehicle(ch, veh, MEMBERS_AND_ALLIES)) {
		msg_to_char(ch, "You don't have permission to fire that.\r\n");
	}
	else if (time(0) - VEH_LAST_FIRE_TIME(veh) < config_get_int("vehicle_siege_time")) {
		diff = config_get_int("vehicle_siege_time") - (time(0) - VEH_LAST_FIRE_TIME(veh));
		msg_to_char(ch, "You must wait another %d second%s to fire it.\r\n", diff, PLURAL(diff));
	}
	
	// find a target
	else if (!find_siege_target_for_vehicle(ch, veh, tar_arg, &room_targ, &dir, &veh_targ)) {
		// sends own messages
	}
	
	// seems ok
	else {
		if (SHOULD_APPEAR(ch)) {
			appear(ch);
		}
		
		extract_resources(ch, ammo, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), NULL);
		dam = VEH_SCALE_LEVEL(veh) * 8 / 100;	// 8 damage per 100 levels
		dam = MAX(1, dam);	// minimum 1
		
		if (room_targ) {
			sprintf(buf, "You fire $V %s!", dirs[get_direction_for_char(ch, dir)]);
			act(buf, FALSE, ch, NULL, veh, TO_CHAR);
			
			// message to ch's room
			LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
				if (vict != ch && vict->desc) {
					sprintf(buf, "$V fires %s!", dirs[get_direction_for_char(vict, dir)]);
					act(buf, FALSE, vict, NULL, veh, TO_CHAR);
				}
			}
			
			// message to veh's room
			if (IN_ROOM(veh) != IN_ROOM(ch)) {
				LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(veh)), vict, next_in_room) {
					if (vict != ch && vict->desc) {
						sprintf(buf, "$V fires %s!", dirs[get_direction_for_char(vict, dir)]);
						act(buf, FALSE, vict, NULL, veh, TO_CHAR);
					}
				}
			}
			
			if (ROOM_OWNER(room_targ) && GET_LOYALTY(ch) != ROOM_OWNER(room_targ)) {
				trigger_distrust_from_hostile(ch, ROOM_OWNER(room_targ));
			}
			
			secttype = SECT(room_targ);
			besiege_room(room_targ, dam);
			
			if (SECT(room_targ) != secttype) {
				msg_to_char(ch, "It is destroyed!\r\n");
			}
		}
		else if (veh_targ) {
			act("You fire $v at $V!", FALSE, ch, veh, veh_targ, TO_CHAR | ACT_VEHICLE_OBJ);
			act("$n fires $v at $V!", FALSE, ch, veh, veh_targ, TO_ROOM | ACT_VEHICLE_OBJ);
			
			// message to veh's room
			if (IN_ROOM(veh) != IN_ROOM(ch) && ROOM_PEOPLE(IN_ROOM(veh))) {
				act("$v fires at $V!", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), veh, veh_targ, TO_CHAR | TO_ROOM | ACT_VEHICLE_OBJ);
			}
			
			if (VEH_OWNER(veh_targ) && GET_LOYALTY(ch) != VEH_OWNER(veh_targ)) {
				trigger_distrust_from_hostile(ch, VEH_OWNER(veh_targ));
			}
			
			besiege_vehicle(veh_targ, dam, SIEGE_PHYSICAL);
		}
		
		// delays
		VEH_LAST_FIRE_TIME(veh) = time(0);
		GET_WAIT_STATE(ch) = 5 RL_SEC;
	}
}


ACMD(do_harness) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	char_data *animal;
	vehicle_data *veh;
	
	// usage: harness <animal> <vehicle>
	two_arguments(argument, arg1, arg2);
	
	if (!*arg1 || !*arg2) {
		msg_to_char(ch, "Harness whom to what?\r\n");
	}
	else if (!(animal = get_char_vis(ch, arg1, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (!(veh = get_vehicle_in_room_vis(ch, arg2))) {
		msg_to_char(ch, "You don't see a %s here.\r\n", arg2);
	}
	else if (count_harnessed_animals(veh) >= VEH_ANIMALS_REQUIRED(veh)) {
		msg_to_char(ch, "You can't harness %s animals to it.\r\n", count_harnessed_animals(veh) == 0 ? "any" : "any more");
	}
	else if (!VEH_IS_COMPLETE(veh)) {
		act("You must finish constructing $V before you can harness anything to it.", FALSE, ch, NULL, veh, TO_CHAR);
	}
	else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		msg_to_char(ch, "You can't harness anything to it while it's on fire.\r\n");
	}
	else if (!IS_NPC(animal)) {
		msg_to_char(ch, "You can only harness animals.\r\n");
	}
	else if (!MOB_FLAGGED(animal, MOB_MOUNTABLE)) {
		act("You can't harness $N to anything!", FALSE, ch, NULL, animal, TO_CHAR);
	}
	else if (GET_LED_BY(animal) && GET_LED_BY(animal) != ch) {
		act("$N is being led by someone else.", FALSE, ch, NULL, animal, TO_CHAR);
	}
	else if (GET_LOYALTY(animal) && GET_LOYALTY(animal) != GET_LOYALTY(ch)) {
		msg_to_char(ch, "You can't harness animals that belong to other empires.\r\n");
	}
	else {
		if (GET_LED_BY(animal)) {
			act("You stop leading $N.", FALSE, GET_LED_BY(animal), NULL, animal, TO_CHAR);
		}
		
		act("You harness $N to $v.", FALSE, ch, veh, animal, TO_CHAR | ACT_VEHICLE_OBJ);
		act("$n harnesses you to $v.", FALSE, ch, veh, animal, TO_VICT | ACT_VEHICLE_OBJ);
		act("$n harnesses $N to $v.", FALSE, ch, veh, animal, TO_NOTVICT | ACT_VEHICLE_OBJ);
		harness_mob_to_vehicle(animal, veh);
	}
}


ACMD(do_lead) {
	vehicle_data *veh;
	char_data *mob;
	
	one_argument(argument, arg);
	
	if (GET_LEADING_MOB(ch)) {
		act("You stop leading $N.", FALSE, ch, NULL, GET_LEADING_MOB(ch), TO_CHAR);
		act("$n stops leading $N.", FALSE, ch, NULL, GET_LEADING_MOB(ch), TO_ROOM);
		GET_LED_BY(GET_LEADING_MOB(ch)) = NULL;
		GET_LEADING_MOB(ch) = NULL;
	}
	else if (GET_LEADING_VEHICLE(ch)) {
		act("You stop leading $V.", FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_CHAR);
		act("$n stops leading $V.", FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_ROOM);
		VEH_LED_BY(GET_LEADING_VEHICLE(ch)) = NULL;
		GET_LEADING_VEHICLE(ch) = NULL;
	}
	else if (IS_NPC(ch)) {
		msg_to_char(ch, "Npcs can't lead anything.\r\n");
	}
	else if (GET_SITTING_ON(ch)) {
		msg_to_char(ch, "You can't lead anything while you're sitting %s something.\r\n", IN_OR_ON(GET_SITTING_ON(ch)));
	}
	else if (!*arg) {
		msg_to_char(ch, "Lead whom (or what)?\r\n");
	}
	else if ((mob = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		// lead mob (we already made sure they aren't leading anything)
		if (ch == mob) {
			msg_to_char(ch, "You can't lead yourself.\r\n");
		}
		else if (!IS_NPC(mob)) {
			msg_to_char(ch, "You can't lead other players around.\r\n");
		}
		else if (!MOB_FLAGGED(mob, MOB_MOUNTABLE)) {
			act("You can't lead $N!", FALSE, ch, 0, mob, TO_CHAR);
		}
		else if (GET_LED_BY(mob)) {
			act("Someone is already leading $M.", FALSE, ch, 0, mob, TO_CHAR);
		}
		else if (mob->desc) {
			act("You can't lead $N!", FALSE, ch, 0, mob, TO_CHAR);
		}
		else if (GET_LOYALTY(mob) && GET_LOYALTY(mob) != GET_LOYALTY(ch)) {
			msg_to_char(ch, "You can't lead animals owned by other empires.\r\n");
		}
		else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
			msg_to_char(ch, "You can't lead animals out of other people's territory.\r\n");
		}
		else {
			act("You begin to lead $N.", FALSE, ch, NULL, mob, TO_CHAR);
			act("$n begins to lead $N.", TRUE, ch, NULL, mob, TO_ROOM);
			GET_LEADING_MOB(ch) = mob;
			GET_LED_BY(mob) = ch;
		}
	}
	else if ((veh = get_vehicle_in_room_vis(ch, arg))) {
		// lead vehicle (we already made sure they aren't leading anything)
		if (!VEH_FLAGGED(veh, VEH_LEADABLE)) {
			act("You can't lead $V!", FALSE, ch, NULL, veh, TO_CHAR);
		}
		else if (!VEH_IS_COMPLETE(veh)) {
			act("You must finish constructing $V before you can lead it.", FALSE, ch, NULL, veh, TO_CHAR);
		}
		else if (VEH_LED_BY(veh)) {
			act("$N is already leading it.", FALSE, ch, NULL, VEH_LED_BY(veh), TO_CHAR);
		}
		else if (count_harnessed_animals(veh) < VEH_ANIMALS_REQUIRED(veh)) {
			msg_to_char(ch, "You need to harness %d animal%s to it before you can lead it.\r\n", VEH_ANIMALS_REQUIRED(veh), PLURAL(VEH_ANIMALS_REQUIRED(veh)));
		}
		else if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
			msg_to_char(ch, "You don't have permission to lead that.\r\n");
		}
		else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
			msg_to_char(ch, "You can't lead it while it's on fire!\r\n");
		}
		else if (VEH_SITTING_ON(veh)) {
			msg_to_char(ch, "You can't lead it while %s sitting on it.\r\n", (VEH_SITTING_ON(veh) == ch) ? "you are" : "someone else is");
		}
		else if (VEH_DRIVER(veh)) {
			msg_to_char(ch, "You can't lead it while someone else is controlling it.\r\n");
		}
		else {
			act("You begin to lead $V.", FALSE, ch, NULL, veh, TO_CHAR);
			act("$n begins to lead $V.", TRUE, ch, NULL, veh, TO_ROOM);
			GET_LEADING_VEHICLE(ch) = veh;
			VEH_LED_BY(veh) = ch;
		}
	}
	else {
		msg_to_char(ch, "You don't see any %s to lead here.\r\n", arg);
	}
}


ACMD(do_load_vehicle) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	vehicle_data *cont, *veh, *next_veh;
	char_data *mob, *next_mob;
	room_data *to_room;
	bool found;
	
	two_arguments(argument, arg1, arg2);
	
	if (!*arg1 || !*arg2) {
		msg_to_char(ch, "Usage: load <mob | vehicle | all> <onto vehicle>\r\n");
	}
	else if (!(cont = get_vehicle_in_room_vis(ch, arg2))) {
		msg_to_char(ch, "You don't see %s %s here.\r\n", arg2, AN(arg2));
	}
	else if (!VEH_IS_COMPLETE(cont)) {
		msg_to_char(ch, "You must finish constructing it before anything can be loaded %sto it.\r\n", IN_OR_ON(cont));
	}
	else if (VEH_FLAGGED(cont, VEH_ON_FIRE)) {
		msg_to_char(ch, "You can't load anything %sto it while it's on fire!\r\n", IN_OR_ON(cont));
	}
	else if (!(to_room = get_vehicle_interior(cont))) {
		msg_to_char(ch, "You can't load anything %sto that!\r\n", IN_OR_ON(cont));
	}
	else if (!can_use_vehicle(ch, cont, MEMBERS_AND_ALLIES)) {
		act("You don't have permission to use $V.", FALSE, ch, NULL, cont, TO_CHAR);
	}
	else if (!str_cmp(arg1, "all")) {
		// LOAD ALL
		found = FALSE;
		
		if (VEH_FLAGGED(cont, VEH_CARRY_MOBS)) {
			LL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), mob, next_mob, next_in_room) {
				if (mob == ch || !IS_NPC(mob)) {
					continue;
				}
				if (!MOB_FLAGGED(mob, MOB_ANIMAL | MOB_MOUNTABLE) || GET_LED_BY(mob)) {
					continue;
				}
				if (GET_POS(mob) < POS_STANDING || FIGHTING(mob) || AFF_FLAGGED(mob, AFF_ENTANGLED) || GET_FED_ON_BY(mob)) {
					continue;
				}
			
				perform_load_mob(ch, mob, cont, to_room);
				found = TRUE;
			}
		}
		
		if (VEH_FLAGGED(cont, VEH_CARRY_VEHICLES)) {
			LL_FOREACH_SAFE2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_veh, next_in_room) {
				if (veh == cont || !VEH_IS_COMPLETE(veh) || VEH_FLAGGED(veh, VEH_ON_FIRE | VEH_NO_LOAD_ONTO_VEHICLE)) {
					continue;
				}
				if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
					continue;
				}
				if (VEH_SITTING_ON(veh) || VEH_LED_BY(veh) || VEH_DRIVER(veh)) {
					continue;
				}
			
				perform_load_vehicle(ch, veh, cont, to_room);
				found = TRUE;
			}
		}
		
		if (!found) {
			msg_to_char(ch, "There was nothing you could load here.\r\n");
		}
	}
	else if ((mob = get_char_room_vis(ch, arg1))) {
		// LOAD MOB
		if (ch == mob) {
			msg_to_char(ch, "Just board it.\r\n");
		}
		else if (!IS_NPC(mob) || mob->master) {
			act("You can't load $N.", FALSE, ch, NULL, mob, TO_CHAR);
		}
		else if (!MOB_FLAGGED(mob, MOB_ANIMAL | MOB_MOUNTABLE)) {
			msg_to_char(ch, "You can only load animals and mounts.\r\n");
		}
		else if (!VEH_FLAGGED(cont, VEH_CARRY_MOBS)) {
			act("$v won't carry mobs.", FALSE, ch, cont, NULL, TO_CHAR | ACT_VEHICLE_OBJ);
		}
		else if (GET_POS(mob) < POS_STANDING || FIGHTING(mob) || AFF_FLAGGED(mob, AFF_ENTANGLED) || GET_FED_ON_BY(mob)) {
			act("You can't load $M right now.", FALSE, ch, NULL, mob, TO_CHAR);
		}
		else if (GET_LED_BY(mob)) {
			snprintf(buf, sizeof(buf), "You can't load $N while %s leading $M.", GET_LED_BY(mob) == ch ? "you're" : "someone else is");
			act(buf, FALSE, ch, NULL, mob, TO_CHAR);
		}
		else {
			perform_load_mob(ch, mob, cont, to_room);
		}
	}
	else if ((veh = get_vehicle_in_room_vis(ch, arg1))) {
		// LOAD VEHICLE
		if (veh == cont) {
			msg_to_char(ch, "You can't load it into itself.\r\n");
		}
		else if (!VEH_FLAGGED(cont, VEH_CARRY_VEHICLES)) {
			act("$v won't carry vehicles.", FALSE, ch, cont, NULL, TO_CHAR | ACT_VEHICLE_OBJ);
		}
		else if (VEH_FLAGGED(veh, VEH_NO_LOAD_ONTO_VEHICLE)) {
			snprintf(buf, sizeof(buf), "You can't load $v %sto anything.", IN_OR_ON(cont));
			act(buf, FALSE, ch, veh, NULL, TO_CHAR | ACT_VEHICLE_OBJ);
		}
		else if (VEH_FLAGGED(veh, VEH_NO_LOAD_ONTO_VEHICLE)) {
			msg_to_char(ch, "You can't load that %sto vehicles.\r\n", IN_OR_ON(cont));
		}
		else if (!VEH_IS_COMPLETE(veh)) {
			msg_to_char(ch, "You must finish constructing it before it can be loaded %sto anything.\r\n", IN_OR_ON(cont));
		}
		else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
			msg_to_char(ch, "You can't load that -- it's on fire!\r\n");
		}
		else if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
			act("You don't have permission to load $V.", FALSE, ch, NULL, veh, TO_CHAR);
		}
		else if (VEH_DRIVER(veh)) {
			msg_to_char(ch, "You can't load %s while %s driving it.\r\n", VEH_SHORT_DESC(veh), VEH_DRIVER(veh) == ch ? "you're" : "someone else is");
		}
		else if (VEH_LED_BY(veh)) {
			msg_to_char(ch, "You can't load %s while %s leading it.\r\n", VEH_SHORT_DESC(veh), VEH_LED_BY(veh) == ch ? "you're" : "someone else is");
		}
		else if (VEH_SITTING_ON(veh)) {
			msg_to_char(ch, "You can't load %s while %s sitting %s it.\r\n", VEH_SHORT_DESC(veh), VEH_SITTING_ON(veh) == ch ? "you're" : "someone else is", IN_OR_ON(veh));
		}
		else {
			perform_load_vehicle(ch, veh, cont, to_room);
		}
	}
	else {
		msg_to_char(ch, "You don't see %s %s here.\r\n", arg1, AN(arg1));
	}
}


ACMD(do_repair) {
	char arg[MAX_INPUT_LENGTH];
	vehicle_data *veh;
	
	one_argument(argument, arg);
	
	if (GET_ACTION(ch) == ACT_REPAIRING) {
		act("You stop repairing.", FALSE, ch, NULL, NULL, TO_CHAR);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy right now.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Repair what?\r\n");
	}
	else if (!(veh = get_vehicle_in_room_vis(ch, arg))) {
		msg_to_char(ch, "You don't see anything like that here.\r\n");
	}
	else if (!can_use_vehicle(ch, veh, MEMBERS_AND_ALLIES)) {
		msg_to_char(ch, "You can't repair something that belongs to someone else.\r\n");
	}
	else if (!VEH_IS_COMPLETE(veh)) {
		msg_to_char(ch, "You can only repair vehicles that are finished.\r\n");
	}
	else if (!VEH_NEEDS_RESOURCES(veh) && VEH_HEALTH(veh) >= VEH_MAX_HEALTH(veh)) {
		msg_to_char(ch, "It doesn't need repair.\r\n");
	}
	else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		msg_to_char(ch, "You can't repair it while it's on fire!\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to repair anything here.\r\n");
	}
	else {
		start_action(ch, ACT_REPAIRING, -1);
		GET_ACTION_VNUM(ch, 0) = veh_script_id(veh);
		act("You begin to repair $V.", FALSE, ch, NULL, veh, TO_CHAR);
		act("$n begins to repair $V.", FALSE, ch, NULL, veh, TO_ROOM);
	}
}


ACMD(do_scrap) {
	char arg[MAX_INPUT_LENGTH];
	vehicle_data *veh;
	craft_data *craft;
	char_data *iter;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Scrap what?\r\n");
	}
	else if (!(veh = get_vehicle_in_room_vis(ch, arg))) {
		msg_to_char(ch, "You don't see anything like that here.\r\n");
	}
	else if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
		msg_to_char(ch, "You can't scrap that! It's not even yours.\r\n");
	}
	else if (VEH_IS_COMPLETE(veh)) {
		msg_to_char(ch, "You can only scrap incomplete vehicles.\r\n");
	}
	else {
		// seems good... but make sure nobody is working on it
		LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_in_room) {
			if (IS_NPC(iter) || GET_ACTION(iter) != ACT_GEN_CRAFT) {
				continue;
			}
			if (!(craft = craft_proto(GET_ACTION_VNUM(iter, 0)))) {
				continue;
			}
			if (!CRAFT_FLAGGED(craft, CRAFT_VEHICLE) || GET_CRAFT_OBJECT(craft) != VEH_VNUM(veh)) {
				continue;
			}
			
			// someone is working on the vehicle
			msg_to_char(ch, "You can't scrap it while %s working on it.\r\n", (ch == iter ? "you're" : "someone is"));
			return;
		}
		
		act("You scrap $V.", FALSE, ch, NULL, veh, TO_CHAR);
		act("$n scraps $V.", FALSE, ch, NULL, veh, TO_ROOM);
		extract_vehicle(veh);
	}
}


ACMD(do_unharness) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct vehicle_attached_mob *animal = NULL, *iter, *next_iter;
	vehicle_data *veh;
	char_data *mob;
	bool any;
	
	// usage: unharness [animal] <vehicle>
	two_arguments(argument, arg1, arg2);
	if (*arg1 && !*arg2) {
		strcpy(arg2, arg1);
		*arg1 = '\0';
	}
	
	// main logic tree
	if (!*arg2) {
		msg_to_char(ch, "Unharness which animal from which vehicle?\r\n");
	}
	else if (!(veh = get_vehicle_in_room_vis(ch, arg2))) {
		msg_to_char(ch, "You don't see a %s here.\r\n", arg2);
	}
	else if (*arg1 && !(animal = find_harnessed_mob_by_name(veh, arg1))) {
		msg_to_char(ch, "There isn't a %s harnessed to it.", arg1);
	}
	else if (count_harnessed_animals(veh) == 0 && !animal) {
		act("There isn't anything harnessed to $V.", FALSE, ch, NULL, veh, TO_CHAR);
	}
	else {
		any = FALSE;
		LL_FOREACH_SAFE(VEH_ANIMALS(veh), iter, next_iter) {
			if (animal && iter != animal) {
				continue;
			}
			
			mob = unharness_mob_from_vehicle(iter, veh);
			
			if (mob) {
				any = TRUE;
				act("You unlatch $N from $v.", FALSE, ch, veh, mob, TO_CHAR);
				act("$n unlatches $N from $v.", FALSE, ch, veh, mob, TO_NOTVICT);
			}
		}
		
		// no messaging? possibly animals no longer exist
		if (!any) {
			send_config_msg(ch, "ok_string");
		}
	}
}


ACMD(do_unload_vehicle) {
	char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	vehicle_data *cont, *veh, *next_veh;
	char_data *mob, *next_mob;
	bool found;
	
	one_argument(argument, arg);
	
	if (!(cont = GET_ROOM_VEHICLE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You must be inside a vehicle to unload anything.\r\n");
	}
	else if (!can_use_vehicle(ch, cont, MEMBERS_AND_ALLIES)) {
		act("You don't have permission to unload $V.", FALSE, ch, NULL, cont, TO_CHAR);
	}
	else if (!VEH_IS_COMPLETE(cont)) {
		act("You must finish constructing $V before anything can be unloaded.", FALSE, ch, NULL, cont, TO_CHAR);
	}
	else if (VEH_FLAGGED(cont, VEH_ON_FIRE)) {
		msg_to_char(ch, "You can't load anything while there's a fire!\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Unload what?\r\n");
	}
	else if (!str_cmp(arg, "all")) {
		// UNLOAD ALL
		found = FALSE;
		
		if (!GET_ROOM_VEHICLE(IN_ROOM(cont)) || VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(cont)), VEH_CARRY_MOBS)) {
			LL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), mob, next_mob, next_in_room) {
				if (mob == ch || !IS_NPC(mob)) {
					continue;
				}
				if (!MOB_FLAGGED(mob, MOB_ANIMAL | MOB_MOUNTABLE) || GET_LED_BY(mob)) {
					continue;
				}
				if (GET_POS(mob) < POS_STANDING || FIGHTING(mob) || AFF_FLAGGED(mob, AFF_ENTANGLED) || GET_FED_ON_BY(mob)) {
					continue;
				}
			
				perform_unload_mob(ch, mob, cont);
				found = TRUE;
			}
		}
		
		if (!GET_ROOM_VEHICLE(IN_ROOM(cont)) || VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(cont)), VEH_CARRY_VEHICLES)) {
			LL_FOREACH_SAFE2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_veh, next_in_room) {
				if (veh == cont || !VEH_IS_COMPLETE(veh) || VEH_FLAGGED(veh, VEH_ON_FIRE | VEH_NO_LOAD_ONTO_VEHICLE)) {
					continue;
				}
				if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
					continue;
				}
				if (VEH_SITTING_ON(veh) || VEH_LED_BY(veh) || VEH_DRIVER(veh)) {
					continue;
				}
			
				perform_unload_vehicle(ch, veh, cont);
				found = TRUE;
			}
		}
		
		if (!found) {
			msg_to_char(ch, "There was nothing you could unload here.\r\n");
		}
	}
	else if ((mob = get_char_room_vis(ch, arg))) {
		// UNLOAD MOB
		if (ch == mob) {
			msg_to_char(ch, "You can't unload yourself.\r\n");
		}
		else if (!IS_NPC(mob) || mob->master) {
			act("You can't unload $N.", FALSE, ch, NULL, mob, TO_CHAR);
		}
		else if (!MOB_FLAGGED(mob, MOB_ANIMAL | MOB_MOUNTABLE)) {
			msg_to_char(ch, "You can only unload animals and mounts.\r\n");
		}
		else if (GET_ROOM_VEHICLE(IN_ROOM(cont)) && !VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(cont)), VEH_CARRY_MOBS)) {
			msg_to_char(ch, "You can't unload mobs here.\r\n");
		}
		else if (GET_POS(mob) < POS_STANDING || FIGHTING(mob) || AFF_FLAGGED(mob, AFF_ENTANGLED) || GET_FED_ON_BY(mob)) {
			act("You can't unload $M right now.", FALSE, ch, NULL, mob, TO_CHAR);
		}
		else if (GET_LED_BY(mob)) {
			snprintf(buf, sizeof(buf), "You can't unload $N while %s leading $M.", GET_LED_BY(mob) == ch ? "you're" : "someone else is");
			act(buf, FALSE, ch, NULL, mob, TO_CHAR);
		}
		else {
			perform_unload_mob(ch, mob, cont);
		}
	}
	else if ((veh = get_vehicle_in_room_vis(ch, arg))) {
		// UNLOAD VEHICLE
		if (veh == cont) {
			msg_to_char(ch, "You can't unload it from itself.\r\n");
		}
		else if (GET_ROOM_VEHICLE(IN_ROOM(cont)) && !VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(cont)), VEH_CARRY_VEHICLES)) {
			msg_to_char(ch, "You can't unload vehicles here.\r\n");
		}
		else if (VEH_FLAGGED(veh, VEH_NO_LOAD_ONTO_VEHICLE)) {
			msg_to_char(ch, "That cannot be unloaded from vehicles.\r\n");
		}
		else if (!VEH_IS_COMPLETE(veh)) {
			msg_to_char(ch, "You must finish constructing it before it can be unloaded from anything.\r\n");
		}
		else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
			msg_to_char(ch, "You can't unload that -- it's on fire!\r\n");
		}
		else if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
			act("You don't have permission to unload $V.", FALSE, ch, NULL, veh, TO_CHAR);
		}
		else if (VEH_DRIVER(veh)) {
			msg_to_char(ch, "You can't unload %s while %s driving it.\r\n", VEH_SHORT_DESC(veh), VEH_DRIVER(veh) == ch ? "you're" : "someone else is");
		}
		else if (VEH_LED_BY(veh)) {
			msg_to_char(ch, "You can't unload %s while %s leading it.\r\n", VEH_SHORT_DESC(veh), VEH_LED_BY(veh) == ch ? "you're" : "someone else is");
		}
		else if (VEH_SITTING_ON(veh)) {
			msg_to_char(ch, "You can't unload %s while %s sitting %s it.\r\n", VEH_SHORT_DESC(veh), VEH_SITTING_ON(veh) == ch ? "you're" : "someone else is", IN_OR_ON(veh));
		}
		else {
			perform_unload_vehicle(ch, veh, cont);
		}
	}
	else {
		msg_to_char(ch, "You don't see %s %s here.\r\n", arg, AN(arg));
	}
}
