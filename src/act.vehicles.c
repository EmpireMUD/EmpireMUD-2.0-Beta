/* ************************************************************************
*   File: act.vehicles.c                                  EmpireMUD 2.0b3 *
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

/**
* Contents:
*   Helpers
*   Sub-Commands
*   Commands
*/

// local protos

// external consts

// external funcs
extern int count_harnessed_animals(vehicle_data *veh);
extern room_data *dir_to_room(room_data *room, int dir);
extern struct vehicle_attached_mob *find_harnessed_mob_by_name(vehicle_data *veh, char *name);
extern room_data *get_vehicle_interior(vehicle_data *veh);
void harness_mob_to_vehicle(char_data *mob, vehicle_data *veh);
extern int perform_move(char_data *ch, int dir, int need_specials_check, byte mode);
void scale_item_to_level(obj_data *obj, int level);
extern char_data *unharness_mob_from_vehicle(struct vehicle_attached_mob *vam, vehicle_data *veh);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

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
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// SUB-COMMANDS ////////////////////////////////////////////////////////////

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

	room_data *was_in;
	obj_data *portal;
	
	if (!*arg || !(portal = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "Which direction is that?\r\n");
	}
	else if (!IS_PORTAL(portal)) {
		msg_to_char(ch, "You can't drag it into that!\r\n");
	}
	else if (!can_enter_portal(ch, portal, FALSE, FALSE)) {
		// sends own message
	}
	else if (!VEH_FLAGGED(veh, VEH_CAN_PORTAL)) {
		act("$V can't be dragged through portals.", FALSE, ch, NULL, veh, TO_CHAR);
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
	vehicle_data *veh;
	
	skip_spaces(&argument);	// usually done ahead of time, but in case
	
	if (GET_POS(ch) == POS_FIGHTING) {
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
		msg_to_char(ch, "You can't sit %s it until it's finished.\r\n", VEH_FLAGGED(veh, VEH_IN) ? "in" : "on");
	}
	else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		msg_to_char(ch, "You can't sit on it while it's on fire!\r\n");
	}
	else if (VEH_SITTING_ON(veh)) {
		msg_to_char(ch, "%s already sitting %s it.\r\n", (VEH_SITTING_ON(veh) != ch ? "Someone else is" : "You are"), VEH_FLAGGED(veh, VEH_IN) ? "in" : "on");
	}
	else if (VEH_LED_BY(veh)) {
		msg_to_char(ch, "You can't sit %s it while %s leading it around.\r\n", VEH_FLAGGED(veh, VEH_IN) ? "in" : "on", (VEH_LED_BY(veh) == ch) ? "you are" : "someone else is");
	}
	else if (GET_LEADING_VEHICLE(ch) || GET_LEADING_MOB(ch)) {
		msg_to_char(ch, "You can't sit %s it while you're leading something.\r\n", VEH_FLAGGED(veh, VEH_IN) ? "in" : "on");
	}
	else {
		act("You sit on $V.", FALSE, ch, NULL, veh, TO_CHAR);
		act("$n sits on $V.", FALSE, ch, NULL, veh, TO_ROOM);
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
	else if (GET_LEADING_VEHICLE(ch) && VEH_FLAGGED(GET_LEADING_VEHICLE(ch), VEH_NO_BUILDING)) {
		act("You can't lead $V in there.", FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_CHAR);
	}
	else {
		// move ch: out-message
		snprintf(buf, sizeof(buf), "You %s $V.", command);
		act(buf, FALSE, ch, NULL, veh, TO_CHAR);
		snprintf(buf, sizeof(buf), "$n %ss $V.", command);
		act(buf, TRUE, ch, NULL, veh, TO_ROOM);
		
		// move ch
		char_to_room(ch, to_room);
		if (!IS_NPC(ch)) {
			GET_LAST_DIR(ch) = NO_DIR;
		}
		look_at_room(ch);
		
		// move ch: in-message
		snprintf(buf, sizeof(buf), "$n %ss.", command);
		act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
		
		// move ch: triggers
		enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
		entry_memory_mtrigger(ch);
		greet_mtrigger(ch, NO_DIR);
		greet_memory_mtrigger(ch);
		
		// leading-mob
		if (GET_LEADING_MOB(ch) && IN_ROOM(GET_LEADING_MOB(ch)) == was_in) {
			act("$n follows $M.", TRUE, GET_LEADING_MOB(ch), NULL, ch, TO_NOTVICT);
			
			char_to_room(GET_LEADING_MOB(ch), to_room);
			if (!IS_NPC(GET_LEADING_MOB(ch))) {
				GET_LAST_DIR(GET_LEADING_MOB(ch)) = NO_DIR;
			}
			look_at_room(GET_LEADING_MOB(ch));
			
			snprintf(buf, sizeof(buf), "$n %ss.", command);
			act(buf, TRUE, GET_LEADING_MOB(ch), NULL, NULL, TO_ROOM);
			
			enter_wtrigger(IN_ROOM(GET_LEADING_MOB(ch)), GET_LEADING_MOB(ch), NO_DIR);
			entry_memory_mtrigger(GET_LEADING_MOB(ch));
			greet_mtrigger(GET_LEADING_MOB(ch), NO_DIR);
			greet_memory_mtrigger(GET_LEADING_MOB(ch));
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
		}
		
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_disembark) {
	vehicle_data *veh = GET_ROOM_VEHICLE(IN_ROOM(ch));
	room_data *was_in = IN_ROOM(ch), *to_room;
	struct follow_type *k;

	if (!veh || !(to_room = IN_ROOM(veh))) {
		msg_to_char(ch, "You can't disembark from here!\r\n");
	}
	else if (!IS_IMMORTAL(ch) && !IS_NPC(ch) && IS_CARRYING_N(ch) > CAN_CARRY_N(ch)) {
		msg_to_char(ch, "You are overburdened and cannot move.\r\n");
	}
	else if (IS_RIDING(ch) && !ROOM_BLD_FLAGGED(to_room, BLD_ALLOW_MOUNTS)) {
		msg_to_char(ch, "You can't disembark here while riding.\r\n");
	}
	else {
		act("$n disembarks from $V.", TRUE, ch, NULL, veh, TO_ROOM);
		msg_to_char(ch, "You disembark.\r\n");
		
		char_to_room(ch, to_room);
		if (!IS_NPC(ch)) {
			GET_LAST_DIR(ch) = NO_DIR;
		}
		look_at_room(ch);
		
		act("$n disembarks from $V.", TRUE, ch, NULL, veh, TO_ROOM);
		
		enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
		entry_memory_mtrigger(ch);
		greet_mtrigger(ch, NO_DIR);
		greet_memory_mtrigger(ch);
		
		if (GET_LEADING_MOB(ch) && IN_ROOM(GET_LEADING_MOB(ch)) == was_in) {
			act("$n is led off.", TRUE, GET_LEADING_MOB(ch), NULL, NULL, TO_ROOM);
			
			char_to_room(GET_LEADING_MOB(ch), to_room);
			if (!IS_NPC(GET_LEADING_MOB(ch))) {
				GET_LAST_DIR(GET_LEADING_MOB(ch)) = NO_DIR;
			}
			look_at_room(GET_LEADING_MOB(ch));
			
			act("$n disembarks from $V.", TRUE, GET_LEADING_MOB(ch), NULL, veh, TO_ROOM);
			
			enter_wtrigger(IN_ROOM(GET_LEADING_MOB(ch)), GET_LEADING_MOB(ch), NO_DIR);
			entry_memory_mtrigger(GET_LEADING_MOB(ch));
			greet_mtrigger(GET_LEADING_MOB(ch), NO_DIR);
			greet_memory_mtrigger(GET_LEADING_MOB(ch));
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
			act("$n disembarks from $V.", TRUE, k->follower, NULL, veh, TO_ROOM);

			char_to_room(k->follower, to_room);
			if (!IS_NPC(k->follower)) {
				GET_LAST_DIR(k->follower) = NO_DIR;
			}
			look_at_room(k->follower);
			
			act("$n disembarks from $p.", TRUE, k->follower, NULL, veh, TO_ROOM);
			
			enter_wtrigger(IN_ROOM(k->follower), k->follower, NO_DIR);
			entry_memory_mtrigger(k->follower);
			greet_mtrigger(k->follower, NO_DIR);
			greet_memory_mtrigger(k->follower);
		}
		
		command_lag(ch, WAIT_OTHER);
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
	else if (IS_WATER_SECT(SECT(IN_ROOM(ch)))) {
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
	else if (!(to_room = dir_to_room(IN_ROOM(ch), dir))) {
		msg_to_char(ch, "You can't drag anything in that direction.\r\n");
	}
	else if (IS_WATER_SECT(SECT(to_room))) {
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
		act("$V is dragged along with $M.", FALSE, ch, NULL, veh, TO_ROOM);
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
		msg_to_char(ch, "You can't lead anything while you're sitting %s something.\r\n", VEH_FLAGGED(GET_SITTING_ON(ch), VEH_IN) ? "in" : "on");
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
	else {
		start_action(ch, ACT_REPAIRING, -1);
		GET_ACTION_VNUM(ch, 0) = GET_ID(veh);
		act("You begin to repair $V.", FALSE, ch, NULL, veh, TO_CHAR);
		act("$n beings to repair $V.", FALSE, ch, NULL, veh, TO_ROOM);
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
