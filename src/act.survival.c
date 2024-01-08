/* ************************************************************************
*   File: act.survival.c                                  EmpireMUD 2.0b5 *
*  Usage: code related to the Survival skill and its abilities            *
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
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "dg_scripts.h"
#include "vnums.h"
#include "constants.h"

/**
* Contents:
*   Helpers
*   Hunt Helpers
*   Mount Commands
*   Commands
*/

// local protos
ACMD(do_dismount);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(butcher_interact) {
	obj_data *fillet = NULL;
	char *cust;
	int num, obj_ok = 0;
	
	if (!has_player_tech(ch, PTECH_BUTCHER_UPGRADE) && number(1, 100) > 60) {
		return FALSE;	// 60% chance of failure without the ability
	}
	
	for (num = 0; num < interaction->quantity; ++num) {
		fillet = read_object(interaction->vnum, TRUE);
		scale_item_to_level(fillet, 1);	// minimum level
		obj_to_char(fillet, ch);
		obj_ok = load_otrigger(fillet);
		if (obj_ok) {
			get_otrigger(fillet, ch, FALSE);
		}
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	if (fillet) {
		if (!obj_ok) {
			// obj likely self-purged
			act("You skillfully butcher $P!", FALSE, ch, NULL, inter_item, TO_CHAR | ACT_OBJ_VICT);
			act("$n butchers $P.", FALSE, ch, NULL, inter_item, TO_ROOM | ACT_OBJ_VICT);
		}
		else if (interaction->quantity != 1) {
			cust = obj_get_custom_message(fillet, OBJ_CUSTOM_RESOURCE_TO_CHAR);
			sprintf(buf, "%s (x%d)", cust ? cust : "You skillfully butcher $p from $P!", interaction->quantity);
			act(buf, FALSE, ch, fillet, inter_item, TO_CHAR | ACT_OBJ_VICT);
			
			cust = obj_get_custom_message(fillet, OBJ_CUSTOM_RESOURCE_TO_ROOM);
			sprintf(buf, "%s (x%d)", cust ? cust : "$n butchers $P and gets $p.", interaction->quantity);
			act(buf, FALSE, ch, fillet, inter_item, TO_ROOM | ACT_OBJ_VICT);
		}
		else {
			cust = obj_get_custom_message(fillet, OBJ_CUSTOM_RESOURCE_TO_CHAR);
			act(cust ? cust : "You skillfully butcher $p from $P!", FALSE, ch, fillet, inter_item, TO_CHAR | ACT_OBJ_VICT);
			
			cust = obj_get_custom_message(fillet, OBJ_CUSTOM_RESOURCE_TO_ROOM);
			act(cust ? cust : "$n butchers $P and gets $p.", FALSE, ch, fillet, inter_item, TO_ROOM | ACT_OBJ_VICT);
		}
		return TRUE;
	}

	// nothing gained?
	return FALSE;
}


/**
* Finds the best saddle in a player's inventory.
*
* @param char_data *ch the person
* @return obj_data *the best saddle in inventory, or NULL if none
*/
obj_data *find_best_saddle(char_data *ch) {
	obj_data *obj, *best = NULL;
	double best_score = 0, this;
	
	DL_FOREACH2(ch->carrying, obj, next_content) {
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
* TODO: could rename this something more generic
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
//// HUNT HELPERS ////////////////////////////////////////////////////////////
	
// helper data: stores spawn lists for do_hunt
struct hunt_helper {
	struct spawn_info *list;
	struct hunt_helper *prev, *next;	// doubly-linked list
};

// for finding global spawn lists
struct hunt_global_bean {
	room_data *room;
	int x_coord, y_coord;
	struct hunt_helper **helpers;	// pointer to existing DLL of helpers
};


GLB_VALIDATOR(validate_global_hunt_for_map_spawns) {
	struct hunt_global_bean *data = (struct hunt_global_bean*)other_data;
	if (!other_data) {
		return FALSE;
	}
	return validate_spawn_location(data->room, GET_GLOBAL_SPARE_BITS(glb), data->x_coord, data->y_coord, FALSE);
}


GLB_FUNCTION(run_global_hunt_for_map_spawns) {
	struct hunt_global_bean *data = (struct hunt_global_bean*)other_data;
	struct hunt_helper *hlp;
	
	if (data && data->helpers && GET_GLOBAL_SPAWNS(glb)) {
		CREATE(hlp, struct hunt_helper, 1);
		hlp->list = GET_GLOBAL_SPAWNS(glb);
		DL_APPEND(*(data->helpers), hlp);
		return TRUE;
	}
	else {
		return FALSE;
	}
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
	else if (GET_MOUNT_VNUM(ch) == NOTHING || !find_mount_data(ch, GET_MOUNT_VNUM(ch)) || !mob_proto(GET_MOUNT_VNUM(ch))) {
		msg_to_char(ch, "You don't have a current mount set.\r\n");
	}
	else if (IS_MORPHED(ch)) {
		msg_to_char(ch, "You can't ride anything in this form.\r\n");
	}
	else if (AFF_FLAGGED(ch, AFF_FLYING)) {
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
		msg_to_char(ch, "You don't have the correct ability to ride %s! (see HELP RIDE)\r\n", get_mob_name_by_proto(GET_MOUNT_VNUM(ch), TRUE));
	}
	else if (MOUNT_FLAGGED(ch, MOUNT_WATERWALKING) && !CAN_RIDE_WATERWALK_MOUNT(ch)) {
		msg_to_char(ch, "You don't have the correct ability to ride %s! (see HELP RIDE)\r\n", get_mob_name_by_proto(GET_MOUNT_VNUM(ch), TRUE));
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_RIDING, NULL, NULL, NULL)) {
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
		
		++count;
		size += snprintf(buf + size, sizeof(buf) - size, " %s%-38s%s%s", (cur ? "&l" : ""), part, (cur ? "&0" : ""), PRF_FLAGGED(ch, PRF_SCREEN_READER) ? "\r\n" : (!(count % 2) ? "\r\n" : " "));
		any = TRUE;
	}
	
	if (!PRF_FLAGGED(ch, PRF_SCREEN_READER) && (count % 2)) {
		size += snprintf(buf + size, sizeof(buf) - size, "\r\n");
	}
	
	if (!any) {
		size += snprintf(buf + size, sizeof(buf) - size, " no matches\r\n");
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " (%d total mount%s)\r\n", count, PLURAL(count));
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
	else if (!(mob = get_char_vis(ch, argument, NULL, FIND_CHAR_ROOM))) {
		// special case: mount/ride a vehicle
		if (get_vehicle_in_room_vis(ch, arg, NULL)) {
			do_sit_on_vehicle(ch, arg, POS_SITTING);
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
	else if (!CAN_RIDE_MOUNT(ch, mob)) {
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
	else if (run_ability_triggers_by_player_tech(ch, PTECH_RIDING, mob, NULL, NULL)) {
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
	struct mount_data *mount;
	char_data *mob;
	
	if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to release mounts here (you wouldn't be able to re-mount it)!\r\n");
	}
	else if (!has_player_tech(ch, PTECH_RIDING_RELEASE_MOUNT)) {
		msg_to_char(ch, "You do not have the ability to release your mounts; they are only loyal to you.\r\n");
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
		SET_BIT(AFF_FLAGS(mob), AFF_NO_DRINK_BLOOD);
		set_mob_flags(mob, MOB_NO_EXPERIENCE);
		
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
		queue_delayed_update(ch, CDU_SAVE);	// prevent mob duplication
		
		load_mtrigger(mob);
		
		gain_player_tech_exp(ch, PTECH_RIDING_RELEASE_MOUNT, 30);
	}
}


// change to a stabled mount
void do_mount_swap(char_data *ch, char *argument) {
	struct mount_data *mount, *iter, *next_iter;
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	bool was_mounted = FALSE, stablemaster;
	char_data *proto;
	int number;
	
	stablemaster = has_player_tech(ch, PTECH_RIDING_SWAP_ANYWHERE);
	
	if (!stablemaster && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_STABLE)) {
		msg_to_char(ch, "You can only swap mounts in a stable.\r\n");
		return;
	}
	if (!stablemaster && !check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This stable must be in a city for you to swap mounts.\r\n");
		return;
	}
	if (!stablemaster && !IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You must complete the stable first.\r\n");
		return;
	}
	if (!stablemaster && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
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
	msg_to_char(ch, "You change your active mount to %s.\r\n", get_mob_name_by_proto(mount->vnum, TRUE));
	
	if (was_mounted) {
		do_mount_current(ch);
	}
	
	gain_player_tech_exp(ch, PTECH_RIDING_SWAP_ANYWHERE, 30);
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_butcher) {
	char_data *proto, *vict;
	obj_data *corpse;
	char *argptr;
	int number;
	
	one_argument(argument, arg);
	argptr = arg;
	number = get_number(&argptr);
	
	if (!*arg) {
		msg_to_char(ch, "What would you like to butcher?\r\n");
		return;
	}
	
	// find in inventory
	corpse = get_obj_in_list_vis(ch, argptr, &number, ch->carrying);
	
	// find in room
	if (!corpse) {
		corpse = get_obj_in_list_vis(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch)));
	}
	
	if (!corpse && (vict = get_char_room_vis(ch, argptr, &number))) {
		// no object found but matched a mob in the room
		act("You need to kill $M first.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (!corpse) {
		msg_to_char(ch, "You don't see %s %s here.\r\n", AN(arg), arg);
	}
	else if (!IS_CORPSE(corpse)) {
		msg_to_char(ch, "You can only butcher a corpse.\r\n");
	}
	else if (!bind_ok(corpse, ch)) {
		msg_to_char(ch, "You can't butcher a corpse that is bound to someone else.\r\n");
	}
	else if (GET_CORPSE_NPC_VNUM(corpse) == NOTHING || !(proto = mob_proto(GET_CORPSE_NPC_VNUM(corpse))) || !has_interaction(proto->interactions, INTERACT_BUTCHER)) {
		msg_to_char(ch, "You can't get any good meat out of that.\r\n");
	}
	else if (!has_tool(ch, TOOL_KNIFE)) {
		msg_to_char(ch, "You need to equip a good knife to butcher with.\r\n");
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_BUTCHER_UPGRADE, NULL, corpse, NULL)) {
		return;
	}
	else {
		if (!IS_SET(GET_CORPSE_FLAGS(corpse), CORPSE_NO_LOOT) && run_interactions(ch, proto->interactions, INTERACT_BUTCHER, IN_ROOM(ch), NULL, corpse, NULL, butcher_interact)) {
			// success
			gain_player_tech_exp(ch, PTECH_BUTCHER_UPGRADE, 15);
			run_ability_hooks_by_player_tech(ch, PTECH_BUTCHER_UPGRADE, NULL, NULL, NULL, NULL);
		}
		else {
			act("You butcher $p but get no useful meat.", FALSE, ch, corpse, NULL, TO_CHAR);
			act("$n butchers $p but gets no useful meat.", FALSE, ch, corpse, NULL, TO_ROOM);
		}
		
		empty_obj_before_extract(corpse);
		extract_obj(corpse);
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_dismount) {
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


ACMD(do_hunt) {
	struct hunt_helper *helpers = NULL, *item, *next_item;
	struct spawn_info *spawn, *found_spawn = NULL;
	char_data *mob, *found_proto = NULL;
	struct hunt_global_bean *data;
	vehicle_data *veh, *next_veh;
	bool junk, non_animal = FALSE;
	int count, x_coord, y_coord;
	
	double min_percent = 1.0;	// won't find things below 1% spawn
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
		return;
	}
	if (GET_ACTION(ch) == ACT_HUNTING && !*argument) {
		msg_to_char(ch, "You stop hunting.\r\n");
		cancel_action(ch);
		return;
	}
	if (!has_player_tech(ch, PTECH_HUNT_ANIMALS)) {
		msg_to_char(ch, "You don't have the right ability to hunt anything.\r\n");
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to hunt anything right now.\r\n");
		return;
	}
	if (!IS_OUTDOORS(ch)) {
		msg_to_char(ch, "You can only hunt while outdoors.\r\n");
		return;
	}
	if (ROOM_OWNER(IN_ROOM(ch)) && is_in_city_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), TRUE, &junk)) {
		// note: if you remove the in-city restriction, the validate_spawn_location() below will need in-city info
		msg_to_char(ch, "You can't hunt in cities.\r\n");
		return;
	}
	if (AFF_FLAGGED(ch, AFF_IMMOBILIZED)) {
		msg_to_char(ch, "You can't hunt anything while immobilized.\r\n");
		return;
	}
	if (GET_ACTION(ch) != ACT_NONE && GET_ACTION(ch) != ACT_HUNTING) {
		msg_to_char(ch, "You're too busy to hunt right now.\r\n");
		return;
	}
	if (!*argument) {
		msg_to_char(ch, "Hunt what?\r\n");
		return;
	}
	
	// count how many people are in the room and also check for a matching animal here
	count = 0;
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), mob, next_in_room) {
		++count;
		
		if (!IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_ANIMAL)) {
			continue;
		}
		
		if (multi_isname(argument, GET_PC_NAME(mob))) {
			act("You can see $N right here!", FALSE, ch, NULL, mob, TO_CHAR);
			return;
		}
	}
	
	if (count > 4) {
		msg_to_char(ch, "The area is too crowded to hunt for anything.\r\n");
		return;
	}
	if (run_ability_triggers_by_player_tech(ch, PTECH_HUNT_ANIMALS, NULL, NULL, NULL)) {
		return;
	}
	
	// build lists: vehicles
	DL_FOREACH_SAFE2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_veh, next_in_room) {
		if (VEH_IS_COMPLETE(veh) && VEH_SPAWNS(veh)) {
			CREATE(item, struct hunt_helper, 1);
			item->list = VEH_SPAWNS(veh);
			DL_PREPEND(helpers, item);
		}
	}
	// build lists: building
	if (GET_BUILDING(IN_ROOM(ch))) {
		// only find a spawn list here if the building is complete; otherwise no list = no spawn
		if (IS_COMPLETE(IN_ROOM(ch)) && GET_BLD_SPAWNS(GET_BUILDING(IN_ROOM(ch)))) {
			CREATE(item, struct hunt_helper, 1);
			item->list = GET_BLD_SPAWNS(GET_BUILDING(IN_ROOM(ch)));
			DL_PREPEND(helpers, item);
		}
	}
	// build lists: crop
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_CROP) && ROOM_CROP(IN_ROOM(ch))) {
		CREATE(item, struct hunt_helper, 1);
		item->list = GET_CROP_SPAWNS(ROOM_CROP(IN_ROOM(ch)));
		DL_PREPEND(helpers, item);
	}
	// build lists: sect
	else {
		CREATE(item, struct hunt_helper, 1);
		item->list = GET_SECT_SPAWNS(SECT(IN_ROOM(ch)));
		DL_PREPEND(helpers, item);
	}
	
	// prepare data for validation (calling these here prevents multiple function calls)
	x_coord = X_COORD(IN_ROOM(ch));
	y_coord = Y_COORD(IN_ROOM(ch));
	
	// build lists: global spawns
	CREATE(data, struct hunt_global_bean, 1);
	data->room = IN_ROOM(ch);
	data->x_coord = x_coord;
	data->y_coord = y_coord;
	data->helpers = &helpers;	// reference current list
	run_globals(GLOBAL_MAP_SPAWNS, run_global_hunt_for_map_spawns, TRUE, get_climate(IN_ROOM(ch)), NULL, NULL, 0, validate_global_hunt_for_map_spawns, data);
	free(data);
	
	// find the thing to hunt
	DL_FOREACH_SAFE(helpers, item, next_item) {
		LL_FOREACH(item->list, spawn) {
			if (spawn->percent < min_percent) {
				continue;	// too low
			}
			if (!validate_spawn_location(IN_ROOM(ch), spawn->flags, x_coord, y_coord, FALSE)) {
				continue;	// cannot spawn here
			}
			if (!(mob = mob_proto(spawn->vnum))) {
				continue;	// no proto
			}
			if (!multi_isname(argument, GET_PC_NAME(mob))) {
				continue;	// name mismatch
			}
			if (!MOB_FLAGGED(mob, MOB_ANIMAL)) {
				non_animal = TRUE;
				continue;	// only animals	-- check this last because it triggers an error message
			}
			
			// seems ok:
			found_proto = mob;
			found_spawn = spawn;	// records the percent etc
		}
		
		// and free the temporary data while we're here
		DL_DELETE(helpers, item);
		free(item);
	}
	
	// did we find anything?
	if (found_proto) {
		act("You see signs that $N has been here recently, and crouch low to stalk it.", FALSE, ch, NULL, found_proto, TO_CHAR);
		act("$n crouches low and begins to hunt.", TRUE, ch, NULL, NULL, TO_ROOM);
		
		start_action(ch, ACT_HUNTING, 0);
		GET_ACTION_VNUM(ch, 0) = GET_MOB_VNUM(found_proto);
		GET_ACTION_VNUM(ch, 1) = found_spawn->percent * 100;	// 10000 = 100.00%
	}
	else if (non_animal) {	// and also not success
		msg_to_char(ch, "You can only hunt animals.\r\n");
	}
	else {
		msg_to_char(ch, "You can't find a trail for anything like that here.\r\n");
	}
}


ACMD(do_mount) {
	char arg[MAX_INPUT_LENGTH];
	
	argument = one_argument(argument, arg);
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't ride anything!\r\n");
	}
	else if (!has_player_tech(ch, PTECH_RIDING)) {
		msg_to_char(ch, "You don't have the ability to ride anything.\r\n");
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


ACMD(do_track) {
	char buf[MAX_STRING_LENGTH];
	room_vnum track_to_room = NOWHERE;
	char_data *vict, *proto;
	struct track_data *track, *next_track;
	bool found = FALSE;
	vehicle_data *veh;
	byte dir = NO_DIR;
	obj_data *portal;
	
	int tracks_lifespan = config_get_int("tracks_lifespan");
	
	one_argument(argument, arg);
	
	if (!has_player_tech(ch, PTECH_TRACK_COMMAND)) {
		msg_to_char(ch, "You don't know how to follow tracks.\r\n");
		return;
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to track for anything here.\r\n");
		return;
	}
	else if (!*arg) {
		msg_to_char(ch, "Track whom? Or what?\r\n");
		return;
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_TRACK_COMMAND, NULL, NULL, NULL)) {
		return;
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_TRACKS)) {
		msg_to_char(ch, "You can't seem to find a trail.\r\n");
		return;
	}
	
	HASH_ITER(hh, ROOM_TRACKS(IN_ROOM(ch)), track, next_track) {
		// skip already-expired tracks
		if (time(0) - track->timestamp > tracks_lifespan * SECS_PER_REAL_MIN) {
			continue;
		}
		
		if (track->id < 0 && (vict = is_playing(-1 * track->id))) {
			// TODO: this is pretty similar to the MATCH macro in handler.c and could be converted to use it
			if (isname(arg, GET_PC_NAME(vict)) || isname(arg, PERS(vict, vict, 0)) || isname(arg, PERS(vict, vict, 1)) || (!IS_NPC(vict) && GET_CURRENT_LASTNAME(vict) && isname(arg, GET_CURRENT_LASTNAME(vict)))) {
				found = TRUE;
				dir = track->dir;
				track_to_room = track->to_room;
				break;
			}
		}
		else if (track->id >= 0 && (proto = mob_proto(track->id)) && isname(arg, GET_PC_NAME(proto))) {
			found = TRUE;
			dir = track->dir;
			track_to_room = track->to_room;
			break;
		}
	}
	
	if (found) {
		if (dir != NO_DIR) {
			msg_to_char(ch, "You find a trail heading %s!\r\n", dirs[get_direction_for_char(ch, dir)]);
		}
		else if ((portal = find_portal_in_room_targetting(IN_ROOM(ch), track_to_room))) {
			act("You find a trail heading into $p!", FALSE, ch, portal, NULL, TO_CHAR);
		}
		else if ((veh = find_vehicle_in_room_with_interior(IN_ROOM(ch), track_to_room))) {
			snprintf(buf, sizeof(buf), "You find a trail heading %sto $V!", IN_OR_ON(veh));
			act(buf, FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		else if ((veh = GET_ROOM_VEHICLE(IN_ROOM(ch))) && IN_ROOM(veh) && GET_ROOM_VNUM(IN_ROOM(veh)) == track_to_room) {
			snprintf(buf, sizeof(buf), "You find a trail heading %s of $V!", VEH_FLAGGED(veh, VEH_IN) ? "out" : "off");
			act(buf, FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		
		gain_player_tech_exp(ch, PTECH_TRACK_COMMAND, 20);
		run_ability_hooks_by_player_tech(ch, PTECH_TRACK_COMMAND, NULL, NULL, NULL, IN_ROOM(ch));
	}
	else {
		msg_to_char(ch, "You can't seem to find a trail.\r\n");
	}
	
	command_lag(ch, WAIT_ABILITY);
}
