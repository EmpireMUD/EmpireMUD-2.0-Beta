/* ************************************************************************
*   File: act.survival.c                                  EmpireMUD 2.0b3 *
*  Usage: code related to the Survival skill and its abilities            *
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
*   Commands
*/

// external vars

// external funcs
extern room_data *dir_to_room(room_data *room, int dir);
void scale_item_to_level(obj_data *obj, int level);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

INTERACTION_FUNC(butcher_interact) {
	obj_data *fillet = NULL;
	int num;
	
	if (!skill_check(ch, ABIL_BUTCHER, DIFF_EASY)) {
		return FALSE;
	}
	
	for (num = 0; num < interaction->quantity; ++num) {
		fillet = read_object(interaction->vnum, TRUE);
		scale_item_to_level(fillet, 1);	// minimum level
		obj_to_char(fillet, ch);
		load_otrigger(fillet);
	}
	
	if (fillet) {
		if (interaction->quantity != 1) {
			sprintf(buf, "You skillfully butcher $p (x%d) from the corpse!", interaction->quantity);
			act(buf, FALSE, ch, fillet, NULL, TO_CHAR);
			
			sprintf(buf, "$n butchers a corpse and gets $p (x%d).", interaction->quantity);
			act(buf, FALSE, ch, fillet, NULL, TO_ROOM);
		}
		else {
			act("You skillfully butcher $p from the corpse!", FALSE, ch, fillet, NULL, TO_CHAR);
			act("$n butchers a corpse and gets $p.", FALSE, ch, fillet, NULL, TO_ROOM);
		}
		return TRUE;
	}

	// nothing gained?
	return FALSE;
}


INTERACTION_FUNC(do_one_forage) {
	char lbuf[MAX_STRING_LENGTH];
	obj_data *obj = NULL;
	int iter, num;
	
	num = interaction->quantity;
	
	// more for skill wins
	num += skill_check(ch, ABIL_FORAGE, DIFF_EASY) ? 1 : 0;
	num += skill_check(ch, ABIL_FORAGE, DIFF_MEDIUM) ? 1 : 0;
	num += skill_check(ch, ABIL_FORAGE, DIFF_HARD) ? 1 : 0;
	
	for (iter = 0; iter < num; ++iter) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, 1);	// minimum level
		obj_to_char_or_room(obj, ch);
		add_depletion(inter_room, DPLTN_FORAGE, TRUE);
		load_otrigger(obj);
	}
	
	sprintf(lbuf, "You forage around and find $p (x%d)!", num);
	act(lbuf, FALSE, ch, obj, NULL, TO_CHAR);
	
	sprintf(lbuf, "$n forages around and finds $p (x%d).", num);
	act(lbuf, TRUE, ch, obj, NULL, TO_ROOM);
	
	return TRUE;
}


/**
* Finds the best saddle in a player's inventory.
*
* @param char_data *ch the person
* @return obj_data *the best saddle in inventory, or NULL if none
*/
obj_data *find_best_saddle(char_data *ch) {
	extern bool can_wear_item(char_data *ch, obj_data *item, bool send_messages);
	
	obj_data *obj, *best = NULL;
	double best_score = 0, this;
	
	for (obj = ch->carrying; obj; obj = obj->next_content) {
		if (CAN_WEAR(obj, ITEM_WEAR_SADDLE) && can_wear_item(ch, obj, FALSE)) {
			this = rate_item(obj);
			
			// give a slight bonus to items that are bound ONLY to this character
			if (OBJ_BOUND_TO(obj) && OBJ_BOUND_TO(obj)->next == NULL && bind_ok(obj, ch)) {
				this *= 1.1;
			}
			
			if (this >= best_score) {
				best = obj;
				best_score = this;
			}
		}
	}
	
	return best;
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_butcher) {
	extern obj_data *has_sharp_tool(char_data *ch);
	
	char_data *proto;
	obj_data *corpse;
	
	one_argument(argument, arg);

	if (!can_use_ability(ch, ABIL_BUTCHER, NOTHING, 0, NOTHING)) {
		return;
	}
	
	if (!*arg) {
		msg_to_char(ch, "What would you like to butcher?\r\n");
		return;
	}
	
	// find in inventory
	corpse = get_obj_in_list_vis(ch, arg, ch->carrying);
	
	// find in room
	if (!corpse) {
		corpse = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)));
	}
	
	if (!corpse) {
		msg_to_char(ch, "You don't see a %s here.\r\n", arg);
	}
	else if (!IS_CORPSE(corpse)) {
		msg_to_char(ch, "You can only butcher a corpse.\r\n");
	}
	else if (!bind_ok(corpse, ch)) {
		msg_to_char(ch, "You can't butcher a corpse that is bound to someone else.\r\n");
	}
	else if (GET_CORPSE_NPC_VNUM(corpse) == NOTHING || !(proto = mob_proto(GET_CORPSE_NPC_VNUM(corpse)))) {
		msg_to_char(ch, "You can't get any good meat out of that.\r\n");
	}
	else if (!has_sharp_tool(ch)) {
		msg_to_char(ch, "You need a sharp tool to butcher with.\r\n");
	}
	else if (ABILITY_TRIGGERS(ch, NULL, corpse, ABIL_BUTCHER)) {
		return;
	}
	else {
		if (run_interactions(ch, proto->interactions, INTERACT_BUTCHER, IN_ROOM(ch), NULL, corpse, butcher_interact)) {
			// success
		}
		else {
			act("You butcher $p but get no useful meat.", FALSE, ch, corpse, NULL, TO_CHAR);
			act("$n butchers $p but gets no useful meat.", FALSE, ch, corpse, NULL, TO_ROOM);
		}
		
		empty_obj_before_extract(corpse);
		extract_obj(corpse);
		charge_ability_cost(ch, NOTHING, 0, NOTHING, 0, WAIT_ABILITY);
		gain_ability_exp(ch, ABIL_BUTCHER, 15);
	}
}


ACMD(do_dismount) {
	void do_unseat_from_vehicle(char_data *ch);
	
	char_data *mount;
	
	if (IS_RIDING(ch)) {
		mount = mob_proto(GET_MOUNT_VNUM(ch));
		
		msg_to_char(ch, "You jump down off of %s.\r\n", mount ? GET_SHORT_DESC(mount) : "your mount");
		
		sprintf(buf, "$n jumps down off of %s.", mount ? GET_SHORT_DESC(mount) : "$s mount");
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		perform_dismount(ch);
	}
	else if (GET_SITTING_ON(ch)) {
		do_unseat_from_vehicle(ch);
	}
	else {
		msg_to_char(ch, "You're not riding anything right now.\r\n");
	}
}


ACMD(do_fish) {
	room_data *room = IN_ROOM(ch);
	int dir = NO_DIR;
	
	any_one_arg(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't fish.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_FISHING) {
		msg_to_char(ch, "You stop fishing.\r\n");
		act("$n stops fishing.", TRUE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (!can_use_ability(ch, ABIL_FISH, NOTHING, 0, NOTHING)) {
		// own messages
	}
	else if (GET_ACTION(ch)) {
		msg_to_char(ch, "You're really too busy to do that.\r\n");
	}
	else if (*arg && (dir = parse_direction(ch, arg)) == NO_DIR) {
		msg_to_char(ch, "Fish in what direction?\r\n");
	}
	else if (dir != NO_DIR && !(room = dir_to_room(IN_ROOM(ch), dir))) {
		msg_to_char(ch, "You can't fish in that direction.\r\n");
	}
	else if (!CAN_INTERACT_ROOM(room, INTERACT_FISH)) {
		msg_to_char(ch, "You can't fish for anything %s!\r\n", (room == IN_ROOM(ch)) ? "here" : "there");
	}
	else if (!can_use_room(ch, room, MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to fish %s.\r\n", (room == IN_ROOM(ch)) ? "here" : "there");
	}
	else if (!GET_EQ(ch, WEAR_WIELD) || GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) != ITEM_WEAPON || GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_JAB) {
		msg_to_char(ch, "You'll need a spear to fish.\r\n");
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_FISH)) {
		return;
	}
	else {
		start_action(ch, ACT_FISHING, config_get_int("fishing_timer") / (skill_check(ch, ABIL_FISH, DIFF_EASY) ? 2 : 1));
		GET_ACTION_VNUM(ch, 0) = dir;
		
		msg_to_char(ch, "You begin looking for fish...\r\n");
		act("$n begins looking for fish.", TRUE, ch, NULL, NULL, TO_ROOM);
	}
}


ACMD(do_forage) {
	int cost = 1;
	
	int short_depletion = config_get_int("short_depletion");
	
	if (!can_use_ability(ch, ABIL_FORAGE, MOVE, cost, NOTHING)) {
		return;
	}
	
	if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to forage here.\r\n");
		return;
	}
	
	if (!CAN_INTERACT_ROOM(IN_ROOM(ch), INTERACT_FORAGE)) {
		msg_to_char(ch, "There's nothing you can forage for here.\r\n");
		return;
	}

	if (get_depletion(IN_ROOM(ch), DPLTN_FORAGE) >= short_depletion) {
		msg_to_char(ch, "You can't seem to find anything to forage for.\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_FORAGE)) {
		return;
	}

	if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_FORAGE, do_one_forage)) {
		// success
		charge_ability_cost(ch, MOVE, cost, NOTHING, 0, WAIT_ABILITY);
		gain_ability_exp(ch, ABIL_FORAGE, 15);
	}
	else {
		msg_to_char(ch, "You don't seem to be able to find anything to forage for.\r\n");
	}
}


ACMD(do_mount) {
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	char_data *mob = NULL, *temp, *proto;
	obj_data *saddle = NULL;

	one_argument(argument, arg);

	if (IS_NPC(ch))
		msg_to_char(ch, "You can't ride anything!\r\n");
	else if (!can_use_ability(ch, ABIL_RIDE, NOTHING, 0, NOTHING)) {
		// sends own msgs
	}
	else if (IS_MORPHED(ch)) {
		msg_to_char(ch, "You can't ride anything in this form.\r\n");
	}
	else if (AFF_FLAGGED(ch, AFF_FLY)) {
		msg_to_char(ch, "You can't mount while flying.\r\n");
	}
	else if (IS_RIDING(ch)) {
		msg_to_char(ch, "You're already mounted.\r\n");
	}
	else if (!*arg && (GET_MOUNT_VNUM(ch) == NOTHING || !mob_proto(GET_MOUNT_VNUM(ch)))) {
		msg_to_char(ch, "What did you want to mount?\r\n");
	}
	else if (*arg && !(mob = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		// special case: mount/ride a vehicle
		if (get_vehicle_in_room_vis(ch, arg)) {
			void do_sit_on_vehicle(char_data *ch, char *argument);
			do_sit_on_vehicle(ch, arg);
		}
		else {
			send_config_msg(ch, "no_person");
		}
	}
	else if (AFF_FLAGGED(ch, AFF_SNEAK)) {
		msg_to_char(ch, "You can't mount while sneaking.\r\n");
	}
	else if (!mob && IS_COMPLETE(IN_ROOM(ch)) && !BLD_ALLOWS_MOUNTS(IN_ROOM(ch))) {
		// only check this if they didn't target a mob -- still need to be able to pick up new mounts indoors
		msg_to_char(ch, "You can't mount here.\r\n");
	}
	else if (GET_SITTING_ON(ch)) {
		msg_to_char(ch, "You're already sitting %s something.\r\n", IN_OR_ON(GET_SITTING_ON(ch)));
	}
	else if (mob && ch == mob) {
		msg_to_char(ch, "You can't mount yourself.\r\n");
	}
	else if (mob && !IS_NPC(mob)) {
		msg_to_char(ch, "You can't ride on other players.\r\n");
	}
	else if (mob && !MOB_FLAGGED(mob, MOB_MOUNTABLE) && !IS_IMMORTAL(ch)) {
		act("You can't ride $N!", FALSE, ch, 0, mob, TO_CHAR);
	}
	else if (((mob && AFF_FLAGGED(mob, AFF_FLY)) || (!mob && MOUNT_FLAGGED(ch, MOUNT_FLYING))) && !CAN_RIDE_FLYING_MOUNT(ch)) {
		if (mob) {
			act("You don't have the correct ability to ride $N! (see HELP RIDE)", FALSE, ch, 0, mob, TO_CHAR);
		}
		else {
			msg_to_char(ch, "You don't have the correct ability to ride %s! (see HELP RIDE)\r\n", get_mob_name_by_proto(GET_MOUNT_VNUM(ch)));
		}
	}
	else if (mob && (mob->desc || (GET_PC_NAME(mob) && (proto = mob_proto(GET_MOB_VNUM(mob))) && GET_PC_NAME(mob) != GET_PC_NAME(proto)))) {
		act("You can't ride $N!", FALSE, ch, 0, mob, TO_CHAR);
	}
	else if (mob && GET_LED_BY(mob)) {
		msg_to_char(ch, "You can't ride someone's who's being led around.\r\n");
	}
	else if (mob && GET_POS(mob) < POS_STANDING) {
		act("You can't mount $N right now.", FALSE, ch, NULL, mob, TO_CHAR);
	}
	else if (ABILITY_TRIGGERS(ch, mob, NULL, ABIL_RIDE)) {
		return;
	}
	else {
		// attempt to use a saddle
		if (!(saddle = GET_EQ(ch, WEAR_SADDLE))) {
			saddle = find_best_saddle(ch);
			if (saddle) {
				equip_char(ch, saddle, WEAR_SADDLE);
				determine_gear_level(ch);
			}
		}
		
		// check for abandoning old mount
		if (mob && GET_MOUNT_VNUM(ch) != NOTHING && mob_proto(GET_MOUNT_VNUM(ch))) {
			temp = read_mobile(GET_MOUNT_VNUM(ch), TRUE);
			char_to_room(temp, IN_ROOM(ch));
			setup_generic_npc(temp, GET_LOYALTY(ch), NOTHING, NOTHING);
			
			act("You drop $N's lead.", FALSE, ch, NULL, temp, TO_CHAR);
			act("$n drops $N's lead.", TRUE, ch, NULL, temp, TO_ROOM);
			
			GET_MOUNT_VNUM(ch) = NOTHING;
			load_mtrigger(temp);
		}

		// load a copy of the mount mob, if there wasn't one (i.e. re-mounting the stored mount)
		if (!mob) {
			mob = read_mobile(GET_MOUNT_VNUM(ch), TRUE);
			char_to_room(mob, IN_ROOM(ch));
		}

		// messaging
		if (saddle) {
			// prevents mounting "someone" in the dark
			sprintf(buf, "You throw on $p and clamber onto %s's back.", PERS(mob, mob, FALSE));
			act(buf, FALSE, ch, saddle, mob, TO_CHAR);
			
			act("$n throws on $p and clambers onto $N's back.", TRUE, ch, saddle, mob, TO_ROOM);
		}
		else {
			// prevents mounting "someone" in the dark
			sprintf(buf, "You clamber onto %s's back.", PERS(mob, mob, FALSE));
			act(buf, FALSE, ch, saddle, mob, TO_CHAR);
			
			act("$n clambers onto $N's back.", TRUE, ch, saddle, mob, TO_ROOM);
		}
		
		// clear any stale mob flags
		GET_MOUNT_FLAGS(ch) = 0;
		
		// hard work! this will un-load the mob
		perform_mount(ch, mob);
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_nightsight) {	
	struct affected_type *af;
	
	if (affected_by_spell(ch, ATYPE_NIGHTSIGHT)) {
		msg_to_char(ch, "You end your nightsight.\r\n");
		act("The glow in $n's eyes fades.", TRUE, ch, NULL, NULL, TO_ROOM);
		affect_from_char(ch, ATYPE_NIGHTSIGHT);
	}
	else if (!can_use_ability(ch, ABIL_NIGHTSIGHT, NOTHING, 0, NOTHING)) {
		return;
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_NIGHTSIGHT)) {
		return;
	}
	else {
		msg_to_char(ch, "You activate nightsight.\r\n");
		act("$n's eyes flash and take on a pale red glow.", TRUE, ch, NULL, NULL, TO_ROOM);
		af = create_flag_aff(ATYPE_NIGHTSIGHT, UNLIMITED, AFF_INFRAVISION, ch);
		affect_join(ch, af, 0);
	}

	command_lag(ch, WAIT_ABILITY);
}


ACMD(do_track) {
	extern const char *dirs[];
	
	char_data *vict, *proto;
	struct track_data *track;
	bool found = FALSE;
	byte dir = NO_DIR;
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_TRACK, NOTHING, 0, NOTHING)) {
		return;
	}
	else if (!*arg) {
		msg_to_char(ch, "Track whom? Or what?\r\n");
		return;
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_TRACK)) {
		return;
	}

	for (track = ROOM_TRACKS(IN_ROOM(ch)); !found && track; track = track->next) {
		if (track->player_id != NOTHING && (vict = is_playing(track->player_id))) {
			// TODO: this is pretty similar to the MATCH macro in handler.c and could be converted to use it
			if (isname(arg, GET_PC_NAME(vict)) || isname(arg, PERS(vict, vict, 0)) || isname(arg, PERS(vict, vict, 1)) || (!IS_NPC(vict) && GET_LASTNAME(vict) && isname(arg, GET_LASTNAME(vict)))) {
				found = TRUE;
				dir = track->dir;
			}
		}
		else if (track->mob_num != NOTHING && (proto = mob_proto(track->mob_num)) && isname(arg, GET_PC_NAME(proto))) {
			found = TRUE;
			dir = track->dir;
		}
	}
	
	if (found) {
		msg_to_char(ch, "You find a trail heading %s!\r\n", dirs[get_direction_for_char(ch, dir)]);
		gain_ability_exp(ch, ABIL_TRACK, 20);
	}
	else {
		msg_to_char(ch, "You can't seem to find a trail.\r\n");
	}
	
	command_lag(ch, WAIT_ABILITY);
}
