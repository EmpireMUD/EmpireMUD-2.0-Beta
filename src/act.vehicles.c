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
extern struct vehicle_attached_mob *find_harnessed_mob_by_name(vehicle_data *veh, char *name);
void harness_mob_to_vehicle(char_data *mob, vehicle_data *veh);
extern char_data *unharness_mob_from_vehicle(struct vehicle_attached_mob *vam, vehicle_data *veh);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

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
	else if (VEH_SITTING_ON(veh) != ch) {
		msg_to_char(ch, "Someone else is already sitting %s it.\r\n", VEH_FLAGGED(veh, VEH_IN) ? "in" : "on");
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
	if (VEH_OWNER(veh) && VEH_OWNER(veh) != GET_LOYALTY(ch)) {
		msg_to_char(ch, "You can't put items into vehicles owned by other empires.\r\n");
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
		else if (VEH_OWNER(veh) && VEH_OWNER(veh) != GET_LOYALTY(ch)) {
			msg_to_char(ch, "You can't lead something owned by another empire.\r\n");
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
