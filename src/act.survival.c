/* ************************************************************************
*   File: act.survival.c                                  EmpireMUD 2.0b5 *
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
*   Mount Commands
*   Commands
*/

// external vars

// external funcs
extern room_data *dir_to_room(room_data *room, int dir, bool ignore_entrance);
void scale_item_to_level(obj_data *obj, int level);

// local protos
ACMD(do_dismount);


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


/**
* Determines if a room qualifies for No Trace (outdoors/wilderness).
*
* @param room_data *room Where to check.
* @return bool TRUE if No Trace works here.
*/
bool valid_no_trace(room_data *room) {
	if (IS_ADVENTURE_ROOM(room)) {
		return FALSE;	// adventures do not trigger this ability
	}
	if (!IS_OUTDOOR_TILE(room) || IS_ROAD(room) || IS_ANY_BUILDING(room)) {
		return FALSE;	// not outdoors
	}
	
	// all other cases?
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// MOUNT COMMANDS //////////////////////////////////////////////////////////

// mount your current mount
void do_mount_current(char_data *ch) {
	obj_data *saddle;
	char_data *mob;
	
	if (IS_RIDING(ch)) {
		msg_to_char(ch, "You're already mounted.\r\n");
	}
	else if (GET_MOUNT_VNUM(ch) == NOTHING || !mob_proto(GET_MOUNT_VNUM(ch))) {
		msg_to_char(ch, "You don't have a current mount set.\r\n");
	}
	else if (IS_MORPHED(ch)) {
		msg_to_char(ch, "You can't ride anything in this form.\r\n");
	}
	else if (AFF_FLAGGED(ch, AFF_FLY)) {
		msg_to_char(ch, "You can't mount while flying.\r\n");
	}
	else if (AFF_FLAGGED(ch, AFF_SNEAK)) {
		msg_to_char(ch, "You can't mount while sneaking.\r\n");
	}
	else if (IS_COMPLETE(IN_ROOM(ch)) && !BLD_ALLOWS_MOUNTS(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't mount here.\r\n");
	}
	else if (GET_SITTING_ON(ch)) {
		msg_to_char(ch, "You're already sitting %s something.\r\n", IN_OR_ON(GET_SITTING_ON(ch)));
	}
	else if (MOUNT_FLAGGED(ch, MOUNT_FLYING) && !CAN_RIDE_FLYING_MOUNT(ch)) {
		msg_to_char(ch, "You don't have the correct ability to ride %s! (see HELP RIDE)\r\n", get_mob_name_by_proto(GET_MOUNT_VNUM(ch)));
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_RIDE)) {
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
		
		// load a copy of the mount mob, for messaging
		mob = read_mobile(GET_MOUNT_VNUM(ch), TRUE);
		char_to_room(mob, IN_ROOM(ch));
		
		// messaging
		if (saddle) {
			// prevents mounting "someone" in the dark
			sprintf(buf, "You throw on $p and clamber onto %s.", PERS(mob, mob, FALSE));
			act(buf, FALSE, ch, saddle, mob, TO_CHAR);
			
			act("$n throws on $p and clambers onto $N.", TRUE, ch, saddle, mob, TO_NOTVICT);
		}
		else {
			// prevents mounting "someone" in the dark
			sprintf(buf, "You clamber onto %s.", PERS(mob, mob, FALSE));
			act(buf, FALSE, ch, saddle, mob, TO_CHAR);
			
			act("$n clambers onto $N's back.", TRUE, ch, saddle, mob, TO_NOTVICT);
		}
		
		// clear any stale mob flags
		GET_MOUNT_FLAGS(ch) = 0;
		
		// hard work! this will un-load the mob
		perform_mount(ch, mob);
		command_lag(ch, WAIT_OTHER);
	}
}


// list/search mounts
void do_mount_list(char_data *ch, char *argument) {
	extern const char *mount_flags[];
	
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH], temp[MAX_STRING_LENGTH];
	struct mount_data *mount, *next_mount;
	bool any = FALSE, cur;
	char_data *proto;
	size_t size = 0;
	int count = 0;
	
	if (!GET_MOUNT_LIST(ch)) {
		msg_to_char(ch, "You have no mounts.\r\n");
		return;
	}
	
	// header
	if (!*argument) {
		size = snprintf(buf, sizeof(buf), "Your mounts:\r\n");
	}
	else {
		size = snprintf(buf, sizeof(buf), "Your mounts matching '%s':\r\n", argument);
	}
	
	HASH_ITER(hh, GET_MOUNT_LIST(ch), mount, next_mount) {
		if (size >= sizeof(buf)) {	// overflow
			break;
		}
		if (!(proto = mob_proto(mount->vnum))) {
			continue;
		}
		if (*argument && !multi_isname(argument, GET_PC_NAME(proto))) {
			continue;
		}
		
		// build line
		cur = (GET_MOUNT_VNUM(ch) == mount->vnum);
		if (mount->flags) {
			prettier_sprintbit(mount->flags, mount_flags, temp);
			snprintf(part, sizeof(part), "%s (%s)%s", skip_filler(GET_SHORT_DESC(proto)), temp, (cur && PRF_FLAGGED(ch, PRF_SCREEN_READER) ? " [current]" : ""));
		}
		else {
			snprintf(part, sizeof(part), "%s%s", skip_filler(GET_SHORT_DESC(proto)), (cur && PRF_FLAGGED(ch, PRF_SCREEN_READER) ? " [current]" : ""));
		}
		
		size += snprintf(buf + size, sizeof(buf) - size, " %s%-38s%s%s", (cur ? "&l" : ""), part, (cur ? "&0" : ""), PRF_FLAGGED(ch, PRF_SCREEN_READER) ? "\r\n" : (!(++count % 2) ? "\r\n" : " "));
		any = TRUE;
	}
	
	if (!PRF_FLAGGED(ch, PRF_SCREEN_READER) && !(++count % 2)) {
		size += snprintf(buf + size, sizeof(buf) - size, "\r\n");
	}
	
	if (!any) {
		size += snprintf(buf + size, sizeof(buf) - size, " no matches\r\n");
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


// attempt to add a mount
void do_mount_new(char_data *ch, char *argument) {
	struct mount_data *mount;
	char_data *mob, *proto;
	bool only;
	
	if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to mount anything here!\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "What did you want to mount?\r\n");
	}
	else if (!(mob = get_char_vis(ch, argument, FIND_CHAR_ROOM))) {
		// special case: mount/ride a vehicle
		if (get_vehicle_in_room_vis(ch, arg)) {
			void do_sit_on_vehicle(char_data *ch, char *argument);
			do_sit_on_vehicle(ch, arg);
		}
		else {
			send_config_msg(ch, "no_person");
		}
	}
	else if (ch == mob) {
		msg_to_char(ch, "You can't mount yourself.\r\n");
	}
	else if (!IS_NPC(mob)) {
		msg_to_char(ch, "You can't ride on other players.\r\n");
	}
	else if (find_mount_data(ch, GET_MOB_VNUM(mob))) {
		act("You already have $N in your stable.", FALSE, ch, NULL, mob, TO_CHAR);
	}
	else if (!MOB_FLAGGED(mob, MOB_MOUNTABLE) && !IS_IMMORTAL(ch)) {
		act("You can't ride $N!", FALSE, ch, 0, mob, TO_CHAR);
	}
	else if (AFF_FLAGGED(mob, AFF_FLY) && !CAN_RIDE_FLYING_MOUNT(ch)) {
		act("You don't have the correct ability to ride $N! (see HELP RIDE)", FALSE, ch, 0, mob, TO_CHAR);
	}
	else if (mob->desc || (GET_PC_NAME(mob) && (proto = mob_proto(GET_MOB_VNUM(mob))) && GET_PC_NAME(mob) != GET_PC_NAME(proto))) {
		act("You can't ride $N!", FALSE, ch, 0, mob, TO_CHAR);
	}
	else if (GET_LED_BY(mob)) {
		msg_to_char(ch, "You can't ride someone who's being led around.\r\n");
	}
	else if (GET_POS(mob) < POS_STANDING) {
		act("You can't mount $N right now.", FALSE, ch, NULL, mob, TO_CHAR);
	}
	else if (ABILITY_TRIGGERS(ch, mob, NULL, ABIL_RIDE)) {
		return;
	}
	else {
		// will immediately attempt to ride if they have no current mount
		only = (GET_MOUNT_VNUM(ch) == NOTHING);
		
		// add mob to pool
		add_mount(ch, GET_MOB_VNUM(mob), get_mount_flags_by_mob(mob));
		
		if (only && (mount = find_mount_data(ch, GET_MOB_VNUM(mob)))) {
			// NOTE: this deliberately has no carriage return (will get another message from do_mount_current)
			msg_to_char(ch, "You gain %s as a mount and attempt to ride %s: ", PERS(mob, mob, FALSE), HMHR(mob));
			act("$n gains $N as a mount.", FALSE, ch, NULL, mob, TO_NOTVICT);
			
			GET_MOUNT_VNUM(ch) = mount->vnum;
			GET_MOUNT_FLAGS(ch) = mount->flags;
			do_mount_current(ch);
		}
		else {	// has other mobs
			act("You gain $N as a mount and send $M back to your stable.", FALSE, ch, NULL, mob, TO_CHAR);
			act("$n gains $N as a mount and sends $M back to $s stable.", FALSE, ch, NULL, mob, TO_NOTVICT);
		}
		
		// remove mob
		extract_char(mob);
	}
}


// release your current mount
void do_mount_release(char_data *ch, char *argument) {
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	struct mount_data *mount;
	char_data *mob;
	
	if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to release mounts here (you wouldn't be able to re-mount it)!\r\n");
	}
	else if (!has_ability(ch, ABIL_STABLEMASTER)) {
		msg_to_char(ch, "You need the Stablemaster ability to release a mount.\r\n");
	}
	else if (*argument) {
		msg_to_char(ch, "You can only release your active mount (you get this error if you type a name).\r\n");
	}
	else if (GET_MOUNT_VNUM(ch) == NOTHING || !mob_proto(GET_MOUNT_VNUM(ch))) {
		msg_to_char(ch, "You have no active mount to release.\r\n");
	}
	else {
		if (IS_RIDING(ch)) {
			do_dismount(ch, "", 0, 0);
		}
		
		mob = read_mobile(GET_MOUNT_VNUM(ch), TRUE);
		char_to_room(mob, IN_ROOM(ch));
		setup_generic_npc(mob, GET_LOYALTY(ch), NOTHING, NOTHING);
		
		act("You drop $N's lead and release $M.", FALSE, ch, NULL, mob, TO_CHAR);
		act("$n drops $N's lead and releases $M.", TRUE, ch, NULL, mob, TO_NOTVICT);
		
		// remove data
		if ((mount = find_mount_data(ch, GET_MOB_VNUM(mob)))) {
			HASH_DEL(GET_MOUNT_LIST(ch), mount);
			free(mount);
		}
		
		// unset current-mount
		GET_MOUNT_VNUM(ch) = NOTHING;
		GET_MOUNT_FLAGS(ch) = NOBITS;
		SAVE_CHAR(ch);	// prevent mob duplication
		
		load_mtrigger(mob);
	}
}


// change to a stabled mount
void do_mount_swap(char_data *ch, char *argument) {
	struct mount_data *mount, *iter, *next_iter;
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	bool was_mounted = FALSE;
	char_data *proto;
	int number;
	
	if (!has_ability(ch, ABIL_STABLEMASTER) && (!HAS_FUNCTION(IN_ROOM(ch), FNC_STABLE) || !IS_COMPLETE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You can only swap mounts in a stable unless you have the Stablemaster ability.\r\n");
		return;
	}
	if (!has_ability(ch, ABIL_STABLEMASTER) && !check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to swap mounts here.\r\n");
		return;
	}
	if (!has_ability(ch, ABIL_STABLEMASTER) && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to mount anything here!\r\n");
		return;
	}
	if (!*argument) {
		msg_to_char(ch, "Swap to which mount?\r\n");
		return;
	}
	
	// look up mount by argument
	mount = NULL;
	strcpy(tmp, argument);
	number = get_number(&tmp);
	HASH_ITER(hh, GET_MOUNT_LIST(ch), iter, next_iter) {
		if (!(proto = mob_proto(iter->vnum))) {
			continue;
		}
		if (!multi_isname(tmp, GET_PC_NAME(proto))) {
			continue;
		}
		
		// match (barring #.x)
		if (--number != 0) {
			continue;
		}
		
		// match!
		mount = iter;
		break;
	}
	
	// did we find one?
	if (!mount) {
		msg_to_char(ch, "You don't seem to have a mount called '%s'.\r\n", argument);
		return;
	}
	if (mount->vnum == GET_MOUNT_VNUM(ch)) {
		msg_to_char(ch, "You're already using that mount.\r\n");
		return;
	}
	
	// Ok go:
	if (IS_RIDING(ch)) {
		do_dismount(ch, "", 0, 0);
		was_mounted = TRUE;
	}
	
	// change current mount to that
	GET_MOUNT_VNUM(ch) = mount->vnum;
	GET_MOUNT_FLAGS(ch) = mount->flags;
	msg_to_char(ch, "You change your active mount to %s.\r\n", get_mob_name_by_proto(mount->vnum));
	
	if (was_mounted) {
		do_mount_current(ch);
	}
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
	extern const char *dirs[];
	
	room_data *room = IN_ROOM(ch);
	char buf[MAX_STRING_LENGTH];
	int dir = NO_DIR;
	
	any_one_arg(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't fish.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_FISHING && !*arg) {
		msg_to_char(ch, "You stop fishing.\r\n");
		act("$n stops fishing.", TRUE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (FIGHTING(ch) && GET_POS(ch) == POS_FIGHTING) {
		msg_to_char(ch, "You can't do that now!\r\n");
	}
	else if (!can_use_ability(ch, ABIL_FISH, NOTHING, 0, NOTHING)) {
		// own messages
	}
	else if (GET_ACTION(ch) != ACT_NONE && GET_ACTION(ch) != ACT_FISHING) {
		msg_to_char(ch, "You're really too busy to do that.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to fish for anything here.\r\n");
	}
	else if (*arg && (dir = parse_direction(ch, arg)) == NO_DIR) {
		msg_to_char(ch, "Fish in what direction?\r\n");
	}
	else if (dir != NO_DIR && !(room = dir_to_room(IN_ROOM(ch), dir, FALSE))) {
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
		if (dir != NO_DIR) {
			sprintf(buf, " to the %s", dirs[get_direction_for_char(ch, dir)]);
		}
		else {
			*buf = '\0';
		}
		
		msg_to_char(ch, "You begin looking for fish%s...\r\n", buf);
		act("$n begins looking for fish.", TRUE, ch, NULL, NULL, TO_ROOM);
		
		start_action(ch, ACT_FISHING, config_get_int("fishing_timer") / (skill_check(ch, ABIL_FISH, DIFF_EASY) ? 2 : 1));
		GET_ACTION_VNUM(ch, 0) = dir;
	}
}


ACMD(do_forage) {
	int cost = 1;
	
	int short_depletion = config_get_int("short_depletion");
	
	if (!can_use_ability(ch, ABIL_FORAGE, MOVE, cost, NOTHING)) {
		return;
	}
	
	if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're too busy to forage right now.\r\n");
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
	
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to forage for anything here.\r\n");
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
	char arg[MAX_INPUT_LENGTH];
	
	argument = one_argument(argument, arg);
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't ride anything!\r\n");
	}
	else if (!can_use_ability(ch, ABIL_RIDE, NOTHING, 0, NOTHING)) {
		// sends own msgs
	}
	
	// list requires no position
	else if (!str_cmp(arg, "list") || !str_cmp(arg, "search")) {
		do_mount_list(ch, argument);
	}
	
	else if (GET_POS(ch) < POS_STANDING) {
		msg_to_char(ch, "You can't do that right now.\r\n");
	}
	
	// other sub-commands require standing
	else if (!*arg) {
		do_mount_current(ch);
	}
	else if (!str_cmp(arg, "swap") || !str_cmp(arg, "change")) {
		do_mount_swap(ch, argument);
	}
	else if (!str_cmp(arg, "release")) {
		do_mount_release(ch, argument);
	}
	else {
		// arg provided
		do_mount_new(ch, arg);
	}
}


ACMD(do_nightsight) {	
	struct affected_type *af;
	
	if (affected_by_spell(ch, ATYPE_NIGHTSIGHT)) {
		msg_to_char(ch, "You end your nightsight.\r\n");
		act("The glow in $n's eyes fades.", TRUE, ch, NULL, NULL, TO_ROOM);
		affect_from_char(ch, ATYPE_NIGHTSIGHT, FALSE);
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
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to track for anything here.\r\n");
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
