/* ************************************************************************
*   File: act.other.c                                     EmpireMUD 2.0b5 *
*  Usage: Miscellaneous player-level commands                             *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __ACT_OTHER_C__

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
#include "constants.h"

/**
* Contents:
*   Helpers
*   Accept/Reject Helpers
*   Alt Import
*   Toggle Functions
*   Commands
*/

// external functions
ACMD(do_slash_channel);

// local prototypes
char_data *find_minipet(char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Dismisses all minipets controlled by the character, if any.
*
* @param char_data *ch The player whose pet(s) to purge.
* @return bool TRUE if it dismissed a minipet, FALSE if not.
*/
bool dismiss_any_minipet(char_data *ch) {
	bool any = FALSE;
	char_data *mob;
	
	while ((mob = find_minipet(ch))) {	// ensures there aren't more than 1
		if (!AFF_FLAGGED(mob, AFF_HIDDEN | AFF_NO_SEE_IN_ROOM)) {
			act("$n leaves.", TRUE, mob, NULL, NULL, TO_ROOM);
		}
		extract_char(mob);
		any = TRUE;
	}
	
	return any;
}


/**
* called by do_customize
*
* @param char_data *ch The player.
* @param char *argument Usage: name <new name | none>
*/
void do_customize_road(char_data *ch, char *argument) {
	char arg2[MAX_STRING_LENGTH], *ptr;
	empire_data *emp = GET_LOYALTY(ch);
	bool found;
	int iter;
	
	char *required_words[] = { "road", "avenue", "street", "way", "drive",
		"grove", "lane", "gardens", "place", "circus", "crescent", "close",
		"square", "hill", "mews", "vale", "rise", "row", "mead", "wharf",
		"alley", "bend", "boulevard", "circle", "court", "course", "crossing",
		"crossroad", "crossroads", "freeway", "lane", "loop", "pass",
		"parkway", "plaza", "trail", "route", "skyway", "terrace", "track",
		"trace", "tunnel", "turnpike", "bluff", "rd", "av", "ave", "st", "wy",
		"dr", "blf", "blvd", "cir", "pkwy", "pathway", "path", "walkway", "walk",
		"intersection", "fork", "corner", 
		"\n"
	};
	
	half_chop(argument, arg, arg2);
	
	if (!emp || ROOM_OWNER(IN_ROOM(ch)) != emp) {
		msg_to_char(ch, "You must own the tile to do this.\r\n");
	}
	else if (!IS_ROAD(IN_ROOM(ch))) {
		msg_to_char(ch, "This isn't a road (try customize building).\r\n");
	}
	else if (!has_permission(ch, PRIV_CUSTOMIZE, IN_ROOM(ch))) {
		msg_to_char(ch, "You are not allowed to customize.\r\n");
	}
	else if (is_abbrev(arg, "name")) {
		if (!*arg2) {
			msg_to_char(ch, "What would you like to name this road (or \"none\")?\r\n");
		}
		else if (!str_cmp(arg2, "none")) {
			REMOVE_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_HIDE_REAL_NAME);
			set_room_custom_name(IN_ROOM(ch), NULL);
			affect_total_room(IN_ROOM(ch));
			msg_to_char(ch, "This road no longer has a custom name.\r\n");
			command_lag(ch, WAIT_ABILITY);
		}
		else if (color_code_length(arg2) > 0) {
			msg_to_char(ch, "You cannot use color codes in custom road names.\r\n");
		}
		else if (strlen(arg2) > 60) {
			msg_to_char(ch, "That name is too long. Road names may not be over 60 characters.\r\n");
		}
		else {
			// looks good, but check that it has a required word
			found = FALSE;
			for (iter = 0; *required_words[iter] != '\n' && !found; ++iter) {
				if ((ptr = str_str(arg2, required_words[iter])) && ptr >= arg2 && (ptr == arg2 || *(ptr-1) == ' ')) {
					// found it at the start of a word
					found = TRUE;
				}
			}
			if (!found) {
				msg_to_char(ch, "The name must include a road name word like 'road' or 'drive'.\r\n");
				return;
			}
			
			set_room_custom_name(IN_ROOM(ch), arg2);
			SET_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_HIDE_REAL_NAME);
			affect_total_room(IN_ROOM(ch));
			msg_to_char(ch, "This road tile is now called \"%s\".\r\n", arg2);
			command_lag(ch, WAIT_ABILITY);
		}
	}
	else {
		msg_to_char(ch, "You can only customize the road's name.\r\n");
	}
}


/**
* Performs a douse on a torch/fire, if possible.
*
* @param char_data *ch The douser.
* @param obj_data *obj The object to douse.
* @param obj_data *cont Optional: The liquid container full of water (may be NULL).
*/
void do_douse_obj(char_data *ch, obj_data *obj, obj_data *cont) {
	if (!IS_LIGHT(obj)) {
		msg_to_char(ch, "You can't douse that -- it's not a light or fire.\r\n");
	}
	else if (!LIGHT_IS_LIT(obj)) {
		act("$p isn't even lit!", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else if (!LIGHT_FLAGGED(obj, LIGHT_FLAG_CAN_DOUSE)) {
		act("You can't seem to douse $p.", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else if (IN_ROOM(obj) && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) && (IS_NPC(ch) || LAST_OWNER_ID(obj) != GET_IDNUM(ch))) {
		msg_to_char(ch, "You can't douse anything here.\r\n");
	}
	else {
		if (cont) {
			act("You douse $p with $P.", FALSE, ch, obj, cont, TO_CHAR | ACT_OBJ_VICT);
			act("$n douses $p with $P.", FALSE, ch, obj, cont, TO_ROOM | ACT_OBJ_VICT);
			
			// use the water
			set_obj_val(cont, VAL_DRINK_CONTAINER_CONTENTS, 0);
			
			if (OBJ_FLAGGED(cont, OBJ_SINGLE_USE)) {
				// single-use: extract it
				run_interactions(ch, GET_OBJ_INTERACTIONS(cont), INTERACT_CONSUMES_TO, IN_ROOM(ch), NULL, cont, NULL, consumes_or_decays_interact);
				empty_obj_before_extract(cont);
				extract_obj(cont);
			}
			else {
				// if it wasn't single-use, reset its timer to UNLIMITED since the timer refers to the contents
				GET_OBJ_TIMER(cont) = UNLIMITED;
			}
		}
		else {
			// no container
			act("You douse $p.", FALSE, ch, obj, NULL, TO_CHAR);
			act("$n douses $p.", FALSE, ch, obj, NULL, TO_ROOM);
		}
		
		// douse it -- this may extract the item
		if (douse_light(obj) == FALSE) {
			msg_to_char(ch, "It's used up and you throw it away.\r\n");
		}
	}
}


/**
* Performs a douse on a room, with an optional water source pre-validated.
*
* @param char_data *ch The douser.
* @param room_data *room The room to douse.
* @param obj_data *cont Optional: The liquid container full of water (may be NULL).
*/
void do_douse_room(char_data *ch, room_data *room, obj_data *cont) {
	int amount = 0;
	
	// ensure this
	room = HOME_ROOM(room);
	
	if (cont && IS_DRINK_CONTAINER(cont)) {
		amount = GET_DRINK_CONTAINER_CONTENTS(cont);
		set_obj_val(cont, VAL_DRINK_CONTAINER_CONTENTS, 0);
		set_obj_val(cont, VAL_DRINK_CONTAINER_TYPE, 0);
		
		act("You throw some water from $p onto the flames!", FALSE, ch, cont, NULL, TO_CHAR);
		act("$n throws some water from $p onto the flames!", FALSE, ch, cont, NULL, TO_ROOM);
		
		if (OBJ_FLAGGED(cont, OBJ_SINGLE_USE)) {
			// single-use: extract it
			run_interactions(ch, GET_OBJ_INTERACTIONS(cont), INTERACT_CONSUMES_TO, IN_ROOM(ch), NULL, cont, NULL, consumes_or_decays_interact);
			empty_obj_before_extract(cont);
			extract_obj(cont);
		}
		else {
			// if it wasn't single-use, reset its timer to UNLIMITED since the timer refers to the contents
			GET_OBJ_TIMER(cont) = UNLIMITED;
		}
	}
	else {
		// water from room/immortal
		amount = config_get_int("fire_extinguish_value") / 4;
		amount = MAX(amount, 1);
		act("You throw some water onto the flames!", FALSE, ch, NULL, NULL, TO_CHAR);
		act("$n throws some water onto the flames!", FALSE, ch, NULL, NULL, TO_ROOM);
	}
	
	// and remove the fire...
	add_to_room_extra_data(room, ROOM_EXTRA_FIRE_REMAINING, -amount);

	if (get_room_extra_data(room, ROOM_EXTRA_FIRE_REMAINING) <= 0) {
		act("The flames have been extinguished!", FALSE, ch, NULL, NULL, TO_CHAR | TO_ROOM);
		if (room != IN_ROOM(ch) && ROOM_PEOPLE(room)) {
			act("The flames have been extinguished!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
		stop_burning(room);
	}
}


/**
* Finds a minipet belonging to the character, if any.
*
* @param char_data *ch The player whose pet to look for.
* @return char_data* The found minipet, if any.
*/
char_data *find_minipet(char_data *ch) {
	char_data *chiter, *found = NULL;
	
	// try the room first
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), chiter, next_in_room) {
		if (IS_MINIPET_OF(chiter, ch)) {
			found = chiter;
			break;
		}
	}
	if (!found) {
		DL_FOREACH(character_list, chiter) {
			if (IS_MINIPET_OF(chiter, ch)) {
				found = chiter;
				break;
			}
		}
	}
	
	return found;	// if any
}


/**
* This quits out an old character and swaps the descriptor over to the new
* character. Caution: The new character may already be in-game, if it was
* linkdead.
*
* @param char_data *old The character who's currently playing.
* @param char_data *new The character to switch to (may or may not be playing).
*/
void perform_alternate(char_data *old, char_data *new) {
	char sys[MAX_STRING_LENGTH], mort_in[MAX_STRING_LENGTH], mort_out[MAX_STRING_LENGTH], mort_alt[MAX_STRING_LENGTH], temp[256];
	const char *msg;
	descriptor_data *desc, *next_d;
	bool show_start = FALSE;
	int invis_lev, old_invis, last_tell;
	empire_data *old_emp;
	
	if (!old || !new || !old->desc || new->desc) {
		log("SYSERR: Attempting invalid perform_alternate with %s, %s, %s, %s", old ? "ok" : "no old", new ? "ok" : "no new", old->desc ? "ok" : "no old desc", new->desc ? "new desc" : "ok");
		return;
	}

	/*
	 * kill off all sockets connected to the same player as the one who is
	 * trying to quit.  Helps to maintain sanity as well as prevent duping.
	 */
	for (desc = descriptor_list; desc; desc = next_d) {
		next_d = desc->next;
		if (desc != old->desc && desc->character && (GET_IDNUM(desc->character) == GET_IDNUM(old))) {
			STATE(desc) = CON_DISCONNECT;
		}
	}
	
	invis_lev = MAX(GET_INVIS_LEV(new), (PLR_FLAGGED(new, PLR_INVSTART) ? GET_ACCESS_LEVEL(new) : 0));
	old_invis = GET_INVIS_LEV(old);
	old_emp = GET_LOYALTY(old);
	
	// prepare logs
	snprintf(sys, sizeof(sys), "%s used alternate at %s to switch to %s", GET_NAME(old), (IN_ROOM(old) ? room_log_identifier(IN_ROOM(old)) : "an unknown location"), GET_NAME(new));

	strcpy(temp, PERS(new, new, TRUE));
	snprintf(mort_alt, sizeof(mort_alt), "%s has switched to %s", PERS(old, old, TRUE), temp);
	snprintf(mort_in, sizeof(mort_in), "%s has entered the game", temp);
	snprintf(mort_out, sizeof(mort_in), "%s has left the game", PERS(old, old, TRUE));
	
	// peace out
	if (!GET_INVIS_LEV(old)) {
		act("$n has left the game.", TRUE, old, NULL, NULL, TO_ROOM);
	}
	
	// save old char...
	GET_LAST_KNOWN_LEVEL(old) = GET_COMPUTED_LEVEL(old);
	SAVE_CHAR(old);
	dismiss_any_minipet(old);
	despawn_companion(old, NOTHING);
	
	// save this to switch over replies
	last_tell = GET_LAST_TELL(old);
	
	// switch over replies for immortals and friends, too
	LL_FOREACH(descriptor_list, desc) {
		if (STATE(desc) != CON_PLAYING || !desc->character) {
			continue;	// not playing
		}
		if (GET_LAST_TELL(desc->character) != GET_IDNUM(old)) {
			continue;	// last tell wasn't old char
		}
		if (!IS_IMMORTAL(desc->character) && (PRF_FLAGGED(old, PRF_NO_FRIENDS) || PRF_FLAGGED(new, PRF_NO_FRIENDS) || account_friend_status(old, desc->character) != FRIEND_FRIENDSHIP)) {
			continue;	// not immortal or friends
		}
		
		// ok: update last tell
		GET_LAST_TELL(desc->character) = GET_IDNUM(new);
	}
	
	// move desc (do this AFTER saving)
	new->desc = old->desc;
	new->desc->character = new;
	old->desc = NULL;
	
	if (config_get_bool("public_logins")) {
		if (GET_INVIS_LEV(new) == 0 && !PLR_FLAGGED(new, PLR_INVSTART)) {
			mortlog("%s", mort_alt);
		}
		else if (old_invis == 0) {
			// only mortlog logout
			mortlog("%s", mort_out);
		}
	}
	else {	// not public logins -- use elogs
		if (GET_INVIS_LEV(new) == 0 && !PLR_FLAGGED(new, PLR_INVSTART) && !PRF_FLAGGED(new, PRF_NO_FRIENDS) && !PRF_FLAGGED(old, PRF_NO_FRIENDS)) {
			mortlog_friends(new, "%s", mort_alt);
		}
		else if (old_invis == 0) {
			// only mortlog logout
			mortlog_friends(old, "%s", mort_out);
		}
		
		if (!PRF_FLAGGED(new, PRF_NO_FRIENDS) && PRF_FLAGGED(old, PRF_NO_FRIENDS)) {
			mortlog_friends(new, "%s", mort_in);
		}
		
		if (old_emp && GET_LOYALTY(new) == old_emp && old_invis == 0) {
			// both in same empire
			log_to_empire(old_emp, ELOG_LOGINS, "%s", mort_alt);
		}
		else if (old_emp && old_invis == 0) {
			log_to_empire(old_emp, ELOG_LOGINS, "%s", mort_out);
		}
		
		if (GET_LOYALTY(new) && GET_LOYALTY(new) != old_emp && GET_INVIS_LEV(new) == 0 && !PLR_FLAGGED(new, PLR_INVSTART)) {
			log_to_empire(GET_LOYALTY(new), ELOG_LOGINS, "%s", mort_in);
		}
	}
	
	// remove old character
	extract_all_items(old);
	extract_char(old);
	
	// ensure character is gone right away
	extract_pending_chars();
	
	// if new is NOT already in-game
	if (!IN_ROOM(new)) {
		global_mute_slash_channel_joins = TRUE;
		enter_player_game(new->desc, FALSE, TRUE);
		global_mute_slash_channel_joins = FALSE;
		
		msg_to_char(new, "\r\n%s\r\n\r\n", config_get_string("welcome_message"));
		act("$n has entered the game.", TRUE, new, 0, 0, TO_ROOM);

		STATE(new->desc) = CON_PLAYING;
		
		// needs newbie setup (gear, etc?)
		if (PLR_FLAGGED(new, PLR_NEEDS_NEWBIE_SETUP)) {
			start_new_character(new);
			show_start = TRUE;
		}
	}
	
	// log late so we have a location
	sprintf(sys + strlen(sys), " at %s.", room_log_identifier(IN_ROOM(new)));
	syslog(SYS_LOGIN, invis_lev, TRUE, "%s", sys);
	
	if (AFF_FLAGGED(new, AFF_EARTHMELDED)) {
		msg_to_char(new, "You are earthmelded.\r\n");
	}
	else {
		look_at_room(new);
	}
	
	// leading \r\n between the look and the tip/info section
	msg_to_char(new, "\r\n");
	
	display_automessages_on_login(new);
	
	if (!PRF_FLAGGED(new, PRF_NO_TUTORIALS)) {
		display_tip_to_char(new);
	}
	if (GET_MAIL_PENDING(new)) {
		send_to_char("\trYou have mail waiting.\t0\r\n", new);
	}
	if (has_uncollected_event_rewards(new)) {
		msg_to_char(new, "\ttYou have uncollected event rewards. Type 'event collect' when you're in your own territory.\t0\r\n");
	}
	
	// reset daily cycle now
	check_daily_cycle_reset(new);
	
	if (!IS_APPROVED(new) && (msg = config_get_string("unapproved_greeting")) && *msg) {
		msg_to_char(new, "\r\n\to%s\t0", msg);
	}
	if (show_start && (msg = config_get_string("start_message")) && *msg) {
		msg_to_char(new, "\r\n\tY%s\t0", msg);
	}
	
	if (!IS_IMMORTAL(new)) {
		add_cooldown(new, COOLDOWN_ALTERNATE, SECS_PER_REAL_MIN);
	}
	GET_LAST_TELL(new) = last_tell;
	
	// send fresh MSDP
	if (new->desc) {
		send_initial_MSDP(new->desc);
	}
}


/**
* Final destination checks and work for 'herd'/.
* 
* @param char_data *ch The herder.
* @param char_data *mob The thing being herded.
* @param room_data *to_room The target room.
* @param int dir Optional: The direction, if applicable (doesn't apply in and out of vehicles; pass NO_DIR for this).
* @param vehicle_data *into_veh Optional: The vehicle being herded into (pass NULL if n/a).
*/
void perform_herd(char_data *ch, char_data *mob, room_data *to_room, int dir, vehicle_data *into_veh) {
	room_data *was_in;
	bool out;
	
	if (!mob || !to_room) {
		msg_to_char(ch, "Herd failed for unknown reason.\r\n");
	}
	
	// validation
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_FRESH_WATER | SECTF_OCEAN) && !MOB_FLAGGED(mob, MOB_AQUATIC) && !HAS_WATERWALKING(mob)) {
		msg_to_char(ch, "You can't herd it into the water.\r\n");
	}
	else if (ROOM_BLD_FLAGGED(to_room, BLD_BARRIER)) {
		msg_to_char(ch, "You find it impossible to herd anything there.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !MOB_FLAGGED(mob, MOB_MOUNTAINWALK)) {
		msg_to_char(ch, "You find it impossible to herd it into that terrain.\r\n");
	}
	else if (ROOM_IS_CLOSED(to_room) && !ROOM_BLD_FLAGGED(to_room, BLD_HERD))
		msg_to_char(ch, "You can't herd an animal in there.\r\n");
	else {
		was_in = IN_ROOM(ch);
		out = (dir == NO_DIR && !into_veh);
		
		// update spawn time: delay despawn due to interaction
		if (MOB_FLAGGED(mob, MOB_SPAWNED)) {
			set_mob_spawn_time(mob, time(0));
		}
		
		if (perform_move(mob, dir, to_room, MOVE_HERD | (out ? MOVE_EXIT : (into_veh ? (MOVE_ENTER_VEH | MOVE_NO_COST) : NOBITS)))) {
			if (into_veh) {
				act("You skillfully herd $N into $v.", FALSE, ch, into_veh, mob, TO_CHAR | ACT_VEH_OBJ);
				act("$n skillfully herds $N into $v.", FALSE, ch, into_veh, mob, TO_ROOM | ACT_VEH_OBJ);
			}
			else if (out) {
				act("You skillfully herd $N out.", FALSE, ch, NULL, mob, TO_CHAR);
				act("$n skillfully herds $N out.", FALSE, ch, NULL, mob, TO_ROOM);
			}
			else {
				act("You skillfully herd $N.", FALSE, ch, NULL, mob, TO_CHAR);
				act("$n skillfully herds $N.", FALSE, ch, NULL, mob, TO_ROOM);
			}
			
			// only attempt to move ch if they weren't moved already (e.g. by following)
			if (IN_ROOM(ch) == was_in && !perform_move(ch, dir, to_room, (out ? MOVE_EXIT : (into_veh ? (MOVE_ENTER_VEH | MOVE_NO_COST) : NOBITS)))) {
				char_to_room(mob, IN_ROOM(ch));
			}
			gain_player_tech_exp(ch, PTECH_HERD_COMMAND, 5);
			run_ability_hooks_by_player_tech(ch, PTECH_HERD_COMMAND, mob, NULL, NULL, NULL);
		}
		else {
			act("You try to herd $N, but $E refuses to move!", FALSE, ch, NULL, mob, TO_CHAR);
			act("$n tries to herd $N away, but $E refuses to move!", FALSE, ch, NULL, mob, TO_ROOM);
		}
	}
}


/**
* Shows a player's own group to him.
*
* @param char_data *ch The person to display to.
*/
static void print_group(char_data *ch) {
	char status[256], class[256], loc[256], alerts[256], skills[256];
	struct group_member_data *mem;
	int iter, ssize;
	char_data *k;

	if (GROUP(ch)) {
		msg_to_char(ch, "Your group:\r\n");
		for (mem = GROUP(ch)->members; mem; mem = mem->next) {
			k = mem->member;
			
			// build status section
			*status = '\0';
			ssize = 0;
			for (iter = 0; iter < NUM_POOLS; ++iter) {
				if (GET_CURRENT_POOL(k, iter) < GET_MAX_POOL(k, iter)) {
					ssize += snprintf(status + ssize, sizeof(status) - ssize, " %d/%d%s", GET_CURRENT_POOL(k, iter), GET_MAX_POOL(k, iter), pool_abbrevs[iter]);
				}
			}
			
			// show class section if they have one
			if (!IS_NPC(k)) {
				get_player_skill_string(k, skills, TRUE);
				snprintf(class, sizeof(class), "/%s", skills);
				
				// screenreader sees role here; otherwise the name is highlighted
				if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
					snprintf(class + strlen(class), sizeof(class) - strlen(class), "/%s", class_role[(int) GET_CLASS_ROLE(k)]);
				}
			}
			else {
				*class = '\0';
			}
			
			// warnings
			*alerts = '\0';
			if (IS_DEAD(k)) {
				snprintf(alerts, sizeof(alerts), " &r(dead)&0");
			}
			
			// show location if different
			if (IN_ROOM(k) != IN_ROOM(ch)) {
				if (HAS_NAVIGATION(ch) && (IS_NPC(k) || HAS_NAVIGATION(k))) {
					snprintf(loc, sizeof(loc), " - %s%s", get_room_name(IN_ROOM(k), FALSE), coord_display_room(ch, IN_ROOM(k), FALSE));
				}
				else {
					snprintf(loc, sizeof(loc), " - %s", get_room_name(IN_ROOM(k), FALSE));
				}
			}
			else {
				*loc = '\0';
			}
			
			// name: lvl class (spec) -- location (x, y)
			msg_to_char(ch, "%s%s%s\t0 [%d%s]%s%s%s\r\n", class_role_color[(int)GET_CLASS_ROLE(k)], PERS(k, k, TRUE), (k == GROUP_LEADER(GROUP(ch))) ? " (L)" : "", get_approximate_level(k), class, status, alerts, loc);
		}
	}
	else {
		msg_to_char(ch, "You are not in a group.\r\n");
	}
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(shear_interact) {
	char buf[MAX_STRING_LENGTH];
	char *cust;
	int iter, amt, obj_ok = 0;
	obj_data *obj = NULL;
	
	add_cooldown(inter_mob, COOLDOWN_SHEAR, config_get_int("shear_growth_time") * SECS_PER_REAL_HOUR);
	command_lag(ch, WAIT_OTHER);
			
	amt = interaction->quantity;
	if (has_player_tech(ch, PTECH_SHEAR_UPGRADE)) {
		amt *= 2;
	}
	
	for (iter = 0; iter < amt; ++iter) {
		obj = read_object(interaction->vnum, TRUE);
		obj_to_char(obj, ch);
		obj_ok = load_otrigger(obj);
		if (obj_ok) {
			get_otrigger(obj, ch, FALSE);
		}
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, amt);
	}
	
	// only show loot to the skinner
	if (!obj_ok || !obj) {
		act("You skillfully shear $N.", FALSE, ch, NULL, inter_mob, TO_CHAR);
		act("$n skillfully shears you.", FALSE, ch, NULL, inter_mob, TO_VICT);
		act("$n skillfully shears $N.", FALSE, ch, NULL, inter_mob, TO_NOTVICT);
	}
	else if (amt == 1) {
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		act(cust ? cust : "You skillfully shear $N and get $p.", FALSE, ch, obj, inter_mob, TO_CHAR);
		
		act("$n skillfully shears you and gets $p.", FALSE, ch, obj, inter_mob, TO_VICT);
		
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		act(cust ? cust : "$n skillfully shears $N and gets $p.", FALSE, ch, obj, inter_mob, TO_NOTVICT);
	}
	else {
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		sprintf(buf, "%s (x%d)", cust ? cust : "You skillfully shear $N and get $p.", amt);
		act(buf, FALSE, ch, obj, inter_mob, TO_CHAR);
		
		sprintf(buf, "$n skillfully shears you and gets $p (x%d).", amt);
		act(buf, FALSE, ch, obj, inter_mob, TO_VICT);
		
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		sprintf(buf, "%s (x%d)", cust ? cust : "$n skillfully shears $N and gets $p.", amt);
		act(buf, FALSE, ch, obj, inter_mob, TO_NOTVICT);
	}
	
	return TRUE;
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(skin_interact) {
	char buf[MAX_STRING_LENGTH];
	char *cust;
	obj_data *obj = NULL;
	int num, obj_ok = 0;
	
	if (!has_player_tech(ch, PTECH_SKINNING_UPGRADE) && number(1, 100) > 60) {
		return FALSE;	// 60% failure unskilled
	}
		
	for (num = 0; num < interaction->quantity; ++num) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, 1);	// min scale
		obj_to_char(obj, ch);
		obj_ok = load_otrigger(obj);
		if (obj_ok) {
			get_otrigger(obj, ch, FALSE);
		}
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	// only show loot to the skinner
	if (!obj_ok) {
		act("You carefully skin $P.", FALSE, ch, NULL, inter_item, TO_CHAR | ACT_OBJ_VICT);
		act("$n carefully skins $P.", FALSE, ch, NULL, inter_item, TO_ROOM | ACT_OBJ_VICT);
	}
	else if (interaction->quantity > 1) {
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		sprintf(buf, "%s (x%d)", cust ? cust : "You carefully skin $P and get $p.", interaction->quantity);
		act(buf, FALSE, ch, obj, inter_item, TO_CHAR | ACT_OBJ_VICT);
		
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		sprintf(buf, "%s (x%d)", cust ? cust : "$n carefully skins $P and gets $p.", interaction->quantity);
		act(buf, FALSE, ch, obj, inter_item, TO_ROOM | ACT_OBJ_VICT);
	}
	else {
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		act(cust ? cust : "You carefully skin $P and get $p.", FALSE, ch, obj, inter_item, TO_CHAR | ACT_OBJ_VICT);
		
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		act(cust ? cust : "$n carefully skins $P and gets $p.", FALSE, ch, obj, inter_item, TO_ROOM | ACT_OBJ_VICT);
	}
	
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// ACCEPT/REJECT HELPERS ///////////////////////////////////////////////////

// helper types and functions:

/**
* Validation function for offer accepts.
*
* @param char_data *ch The person trying to accept the offer.
* @param struct offer_data *offer The offer they are trying to accept.
* @return bool TRUE if it's okay to accept, FALSE to block it.
*/
#define OFFER_VALIDATE(name)  bool (name)(char_data *ch, struct offer_data *offer)

/**
* Function to perform an offer accept. The return value determines whether or
* not the offer should be deleted.
*
* @param char_data *ch The person accepting the offer.
* @param struct offer_data *offer The offer they are trying accepting.
* @return bool TRUE if we should delete the offer after, or FALSE if not (e.g. if it was deleted during the finish).
*/
#define OFFER_FINISH(name)  bool (name)(char_data *ch, struct offer_data *offer)


OFFER_VALIDATE(oval_quest) {
	struct instance_data *inst = find_matching_instance_for_shared_quest(ch, offer->data);
	quest_data *qst = quest_proto(offer->data);
	
	if (!qst) {
		msg_to_char(ch, "Unable to find a quest to accept.\r\n");
		return FALSE;
	}
	if (is_on_quest(ch, offer->data)) {
		msg_to_char(ch, "You are already on that quest.\r\n");
		return FALSE;
	}
	if (!char_meets_prereqs(ch, qst, inst)) {
		msg_to_char(ch, "You don't meet the prerequisites for that quest.\r\n");
		return FALSE;
	}
	
	return TRUE;
}


OFFER_FINISH(ofin_quest) {
	struct instance_data *inst = find_matching_instance_for_shared_quest(ch, offer->data);
	quest_data *qst = quest_proto(offer->data);
	
	if (qst) {
		start_quest(ch, qst, inst);
	}
	
	return TRUE;
}


OFFER_VALIDATE(oval_rez) {
	room_data *loc = real_room(offer->location);
	obj_data *corpse;
	
	if (!loc || (ROOM_INSTANCE(loc) && !can_enter_instance(ch, ROOM_INSTANCE(loc)))) {
		msg_to_char(ch, "You can't seem to resurrect there. Perhaps the adventure is full.\r\n");
		return FALSE;
	}
	
	// if already respawned, verify corpse location
	if (!IS_DEAD(ch)) {
		if (!(corpse = find_obj(GET_LAST_CORPSE_ID(ch))) || !IS_CORPSE(corpse)) {
			msg_to_char(ch, "You can't resurrect because your corpse is gone.\r\n");
			return FALSE;
		}
		if (obj_room(corpse) != loc) {
			msg_to_char(ch, "You can't resurrect because your corpse has moved from the resurrection location.\r\n");
			return FALSE;
		}
	}
	
	return TRUE;
}

OFFER_FINISH(ofin_rez) {
	room_data *loc = real_room(offer->location);	// pre-validated
	perform_resurrection(ch, is_playing(offer->from), loc, offer->data);
	return FALSE;	// prevent deletion because perform_res deletes the offer
}


OFFER_VALIDATE(oval_summon) {
	room_data *loc = real_room(offer->location);
	int type = offer->data;
	
	if (!IS_APPROVED(ch) && config_get_bool("travel_approval")) {
		send_config_msg(ch, "need_approval_string");
		return FALSE;
	}
	if (!loc) {
		msg_to_char(ch, "Summon location invalid.\r\n");
		return FALSE;
	}
	if (loc == IN_ROOM(ch)) {
		msg_to_char(ch, "You are already there!\r\n");
		return FALSE;
	}
	if ((ROOM_INSTANCE(loc) && !can_enter_instance(ch, ROOM_INSTANCE(loc)))) {
		msg_to_char(ch, "You can't be summoned there right now. Perhaps the adventure is full.\r\n");
		return FALSE;
	}
	if (type == SUMMON_ADVENTURE && IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't accept an adventure summon while you're already in an adventure.\r\n");
		return FALSE;
	}
	if (!can_teleport_to(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "You can't be teleported out of here.\r\n");
		return FALSE;
	}
	if (!can_teleport_to(ch, loc, FALSE)) {
		msg_to_char(ch, "You can't be teleported to the summon location.\r\n");
		return FALSE;
	}
	
	return TRUE;
}

OFFER_FINISH(ofin_summon) {
	room_data *loc = real_room(offer->location), *was_in;
	struct follow_type *fol, *next_fol;
	struct instance_data *inst;
	int type = offer->data;
	struct map_data *map;
	
	if (type == SUMMON_ADVENTURE) {
		SET_BIT(PLR_FLAGS(ch), PLR_ADVENTURE_SUMMONED);
		GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch) = GET_ROOM_VNUM(IN_ROOM(ch));
		GET_ADVENTURE_SUMMON_INSTANCE_ID(ch) = (inst = find_instance_by_room(loc, FALSE, FALSE)) ? INST_ID(inst) : NOTHING;
		map = GET_MAP_LOC(IN_ROOM(ch));
		GET_ADVENTURE_SUMMON_RETURN_MAP(ch) = map ? map->vnum : NOWHERE;
	}
	else {
		// if they accept a normal summon out of an adventure, cancel their group summon
		if (PLR_FLAGGED(ch, PLR_ADVENTURE_SUMMONED) && ROOM_INSTANCE(IN_ROOM(ch)) != ROOM_INSTANCE(loc)) {
			cancel_adventure_summon(ch);
		}
	}
	
	was_in = IN_ROOM(ch);
	
	act("$n vanishes in a swirl of light!", TRUE, ch, NULL, NULL, TO_ROOM);
	char_to_room(ch, loc);
	GET_LAST_DIR(ch) = NO_DIR;
	qt_visit_room(ch, IN_ROOM(ch));
	pre_greet_mtrigger(ch, IN_ROOM(ch), NO_DIR, "summon");	// cannot pre-greet for summon
	enter_triggers(ch, NO_DIR, "summon", FALSE);
	look_at_room(ch);
	act("$n appears in a swirl of light!", TRUE, ch, NULL, NULL, TO_ROOM);
	
	greet_triggers(ch, NO_DIR, "summon", FALSE);
	msdp_update_room(ch);	// once we're sure we're staying
	
	// followers?
	LL_FOREACH_SAFE(ch->followers, fol, next_fol) {
		if (IS_NPC(fol->follower) && AFF_FLAGGED(fol->follower, AFF_CHARM) && IN_ROOM(fol->follower) == was_in && !FIGHTING(fol->follower)) {
			act("$n vanishes in a swirl of light!", TRUE, fol->follower, NULL, NULL, TO_ROOM);
			char_to_room(fol->follower, IN_ROOM(ch));
			GET_LAST_DIR(fol->follower) = NO_DIR;
			pre_greet_mtrigger(fol->follower, IN_ROOM(fol->follower), NO_DIR, "summon");	// cannot pre-greet for summon
			enter_triggers(fol->follower, NO_DIR, "summon", FALSE);
			look_at_room(fol->follower);
			act("$n appears in a swirl of light!", TRUE, fol->follower, NULL, NULL, TO_ROOM);
			
			greet_triggers(fol->follower, NO_DIR, "summon", FALSE);
		}
	}
	
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// ALT IMPORT //////////////////////////////////////////////////////////////

/**
* @param char_data *ch Player to import to.
* @param char_data *alt Player to import from.
*/
void alt_import_aliases(char_data *ch, char_data *alt) {
	struct alias_data *al, *iter, *newl;
	bool found, imported = FALSE;
	
	// may have been loaded from file
	check_delayed_load(alt);
	
	LL_FOREACH(GET_ALIASES(alt), al) {
		// ensure not already aliased
		found = FALSE;
		LL_FOREACH(GET_ALIASES(ch), iter) {
			if (!str_cmp(al->alias, iter->alias)) {
				found = TRUE;
				break;
			}
		}
		if (found) {
			continue;	// already aliased
		}
		
		CREATE(newl, struct alias_data, 1);
		newl->alias = str_dup(al->alias);
		newl->replacement = str_dup(al->replacement);
		newl->type = al->type;
		
		LL_PREPEND(GET_ALIASES(ch), newl);
		
		msg_to_char(ch, "Imported alias '%s'.\r\n", newl->alias);
		imported = TRUE;
	}
	
	if (!imported) {
		msg_to_char(ch, "No aliases to import.\r\n");
	}
}


/**
* @param char_data *ch Player to import to.
* @param char_data *alt Player to import from.
*/
void alt_import_fmessages(char_data *ch, char_data *alt) {
	GET_FIGHT_MESSAGES(ch) = GET_FIGHT_MESSAGES(alt);
	msg_to_char(ch, "Imported fight message settings.\r\n");
}


/**
* @param char_data *ch Player to import to.
* @param char_data *alt Player to import from.
*/
void alt_import_fprompt(char_data *ch, char_data *alt) {
	if (GET_FIGHT_PROMPT(alt) && *GET_FIGHT_PROMPT(alt)) {
		if (GET_FIGHT_PROMPT(ch)) {
			free(GET_FIGHT_PROMPT(ch));
		}
		GET_FIGHT_PROMPT(ch) = str_dup(GET_FIGHT_PROMPT(alt));
		msg_to_char(ch, "Imported fprompt.\r\n");
	}
	else {
		msg_to_char(ch, "No fprompt to import.\r\n");
	}
}


/**
* @param char_data *ch Player to import to.
* @param char_data *alt Player to import from.
*/
void alt_import_ignores(char_data *ch, char_data *alt) {
	bool found, imported = FALSE, full = FALSE;
	int iter, sub;
	
	for (iter = 0; iter < MAX_IGNORES && !full; ++iter) {
		if (GET_IGNORE_LIST(alt, iter) <= 0) {
			continue;	// no ignore to copy
		}
		
		// ensure not already ignoring
		found = FALSE;
		for (sub = 0; sub < MAX_IGNORES; ++sub) {
			if (GET_IGNORE_LIST(ch, sub) == GET_IGNORE_LIST(alt, iter)) {
				found = TRUE;
				break;
			}
		}
		if (found) {
			continue;	// no need to copy
		}
		
		// attempt to copy
		found = FALSE;
		for (sub = 0; sub < MAX_IGNORES; ++sub) {
			if (GET_IGNORE_LIST(ch, sub) <= 0) {
				GET_IGNORE_LIST(ch, sub) = GET_IGNORE_LIST(alt, iter);
				imported = TRUE;
				found = TRUE;
				break;
			}
		}
		
		if (!found) {
			// nowhere to insert!
			full = TRUE;
			break;
		}
	}
	
	if (imported) {
		msg_to_char(ch, "Imported ignores.\r\n");
	}
	if (full) {
		msg_to_char(ch, "Your ignore list is too full to add more.\r\n");
	}
	if (!imported && !full) {
		msg_to_char(ch, "No ignores to import.\r\n");
	}
}


/**
* @param char_data *ch Player to import to.
* @param char_data *alt Player to import from.
*/
void alt_import_preferences(char_data *ch, char_data *alt) {
	bitvector_t set;
	
	// prf flags to import
	bitvector_t prfs = PRF_COMPACT | PRF_DEAF | PRF_NOTELL | 
					PRF_NOREPEAT | PRF_NOMAPCOL | 
					PRF_SCROLLING | PRF_NO_ROOM_DESCS | PRF_AUTORECALL | PRF_NOSPAM | 
					PRF_SCREEN_READER | PRF_AUTOKILL | PRF_AUTODISMOUNT | 
					PRF_NOEMPIRE | PRF_CLEARMETERS | PRF_NO_PAINT | 
					PRF_EXTRA_SPACING | PRF_AUTOCLIMB | 
					PRF_AUTOSWIM | PRF_ITEM_QUALITY | PRF_ITEM_DETAILS | 
					PRF_NO_EXITS | PRF_SHORT_EXITS;
	
	// add flags
	set = PRF_FLAGS(alt) & prfs;
	if (set) {
		SET_BIT(PRF_FLAGS(ch), set);
	}
	
	// remove any missing flags
	set = ~PRF_FLAGS(alt) & prfs;
	if (set) {
		REMOVE_BIT(PRF_FLAGS(ch), set);
	}
	
	// non-toggle prefs
	GET_MAPSIZE(ch) = GET_MAPSIZE(alt);
	
	msg_to_char(ch, "Imported preferences.\r\n");
}


/**
* @param char_data *ch Player to import to.
* @param char_data *alt Player to import from.
*/
void alt_import_prompt(char_data *ch, char_data *alt) {
	if (GET_PROMPT(alt) && *GET_PROMPT(alt)) {
		if (GET_PROMPT(ch)) {
			free(GET_PROMPT(ch));
		}
		GET_PROMPT(ch) = str_dup(GET_PROMPT(alt));
		msg_to_char(ch, "Imported prompt.\r\n");
	}
	else {
		msg_to_char(ch, "No prompt to import.\r\n");
	}
}


/**
* @param char_data *ch Player to import to.
* @param char_data *alt Player to import from.
*/
void alt_import_recolors(char_data *ch, char_data *alt) {
	int iter;
	
	for (iter = 0; iter < NUM_CUSTOM_COLORS; ++iter) {
		GET_CUSTOM_COLOR(ch, iter) = GET_CUSTOM_COLOR(alt, iter);
	}
	
	msg_to_char(ch, "Imported recolors.\r\n");
}


/**
* @param char_data *ch Player to import to.
* @param char_data *alt Player to import from.
*/
void alt_import_slash_channels(char_data *ch, char_data *alt) {
	struct slash_channel *chan, *load_slash;
	struct player_slash_channel *iter;
	char buf[MAX_STRING_LENGTH];
	bool imported = FALSE;
	
	// if not in the game, slash channels are here
	LL_FOREACH(LOAD_SLASH_CHANNELS(alt), load_slash) {
		if ((chan = find_slash_channel_by_name(load_slash->name, TRUE)) && !find_on_slash_channel(ch, chan->id)) {
			snprintf(buf, sizeof(buf), "join %s", chan->name);
			do_slash_channel(ch, buf, 0, 0);
			imported = TRUE;
		}
	}
	
	// if in-game, slash channels are here
	LL_FOREACH(GET_SLASH_CHANNELS(alt), iter) {
		if (!find_on_slash_channel(ch, iter->id) && (chan = find_slash_channel_by_id(iter->id))) {
			snprintf(buf, sizeof(buf), "join %s", chan->name);
			do_slash_channel(ch, buf, 0, 0);
			imported = TRUE;
		}
	}
	
	if (!imported) {
		msg_to_char(ch, "No channels to import.\r\n");
	}
}


/**
* @param char_data *ch Player to import to.
* @param char_data *alt Player to import from.
*/
void alt_import_smessages(char_data *ch, char_data *alt) {
	GET_STATUS_MESSAGES(ch) = GET_STATUS_MESSAGES(alt);
	msg_to_char(ch, "Imported status message settings.\r\n");
}


/**
* Sub-processor for "alt import".
*
* @param char_data *ch The player.
* @param char *argument Remaining args.
*/
void do_alt_import(char_data *ch, char *argument) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	char_data *alt = NULL;
	bool file = FALSE;
	
	static const char *valid_fields = "Valid fields: aliases, prompt, fprompt, preferences, fightmessages, recolors, slash-channels, ignores, all\r\n";
	
	two_arguments(argument, arg1, arg2);
	
	// validate
	if (!*arg1 || !*arg2) {
		msg_to_char(ch, "Usage: alt import <name> <field | all>\r\n%s", valid_fields);
	}
	else if (!(alt = find_or_load_player(arg1, &file))) {
		msg_to_char(ch, "Unknown alt '%s'.\r\n", arg1);
	}
	else if (GET_ACCOUNT(ch) != GET_ACCOUNT(alt)) {
		msg_to_char(ch, "That's not even your alt!\r\n");
	}
	
	// process import
	else if (is_abbrev(arg2, "aliases")) {
		alt_import_aliases(ch, alt);
	}
	else if (is_abbrev(arg2, "prompt")) {
		alt_import_prompt(ch, alt);
	}
	else if (is_abbrev(arg2, "fprompt")) {
		alt_import_fprompt(ch, alt);
	}
	else if (is_abbrev(arg2, "preferences")) {
		alt_import_preferences(ch, alt);
	}
	else if (is_abbrev(arg2, "fmessages") || is_abbrev(arg2, "fightmessages")) {
		alt_import_fmessages(ch, alt);
	}
	else if (is_abbrev(arg2, "recolors")) {
		alt_import_recolors(ch, alt);
	}
	else if (is_abbrev(arg2, "slash-channels")) {
		alt_import_slash_channels(ch, alt);
	}
	else if (is_abbrev(arg2, "smessages") || is_abbrev(arg2, "statusmessages")) {
		alt_import_smessages(ch, alt);
	}
	else if (is_abbrev(arg2, "ignores")) {
		alt_import_ignores(ch, alt);
	}
	
	// last
	else if (!str_cmp(arg2, "all")) {
		alt_import_aliases(ch, alt);
		alt_import_prompt(ch, alt);
		alt_import_fprompt(ch, alt);
		alt_import_preferences(ch, alt);
		alt_import_fmessages(ch, alt);
		alt_import_recolors(ch, alt);
		alt_import_ignores(ch, alt);
		alt_import_slash_channels(ch, alt);
		alt_import_smessages(ch, alt);
	}
	else {
		msg_to_char(ch, "Unknown field '%s'.\r\n%s", arg2, valid_fields);
	}

	// cleanup	
	if (alt && file) {
		free_char(alt);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// TOGGLE FUNCTIONS ////////////////////////////////////////////////////////

// for alphabetizing toggles for screenreader users
struct alpha_tog {
	char *name;
	int pos;
	int level;
	struct alpha_tog *next;
};

struct alpha_tog *alpha_tog_list = NULL;	// LL of alphabetized toggles


// alphabetizes the toggle list (after sorting by level)
int sort_alpha_toggles(struct alpha_tog *a, struct alpha_tog *b) {
	if (a->level != b->level) {
		return a->level - b->level;
	}
	return str_cmp(a->name, b->name);
}


// ensures there is an alphabetized list of toggles, for screenreader users
void alphabetize_toggles(void) {
	struct alpha_tog *el;
	int iter;
	
	if (alpha_tog_list) {
		return;	// no work (do 1 time only)
	}
	
	for (iter = 0; *toggle_data[iter].name != '\n'; ++iter) {
		CREATE(el, struct alpha_tog, 1);
		el->name = str_dup(toggle_data[iter].name);
		el->level = toggle_data[iter].level;
		el->pos = iter;
		LL_PREPEND(alpha_tog_list, el);
	}
	
	LL_SORT(alpha_tog_list, sort_alpha_toggles);
}


/**
* toggle notifier for "toggle afk"
*
* @param char_data *ch The player.
*/
void afk_notify(char_data *ch) {
	if (PRF_FLAGGED(ch, PRF_AFK)) {
		act("$n goes afk.", TRUE, ch, NULL, NULL, TO_ROOM);
	}
	else {
		act("$n is no longer afk.", TRUE, ch, NULL, NULL, TO_ROOM);
	}
}


/**
* Ensures political/!map-color are off when informative is on.
*
* @param char_data *ch The player.
*/
void tog_informative(char_data *ch) {
	char temp[256];
	
	// if on...
	if (PRF_FLAGGED(ch, PRF_INFORMATIVE)) {
		// shut off other color options
		REMOVE_BIT(PRF_FLAGS(ch), PRF_POLITICAL);
		if (!PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
			REMOVE_BIT(PRF_FLAGS(ch), PRF_NOMAPCOL);
		}
		
		// show current informative flags
		prettier_sprintbit(GET_INFORMATIVE_FLAGS(ch), informative_view_bits, temp);
		msg_to_char(ch, "Current informative options (type informative to change): %s\r\n", GET_INFORMATIVE_FLAGS(ch) ? temp : "none");
	}
}


/**
* Ensures political/informative are off when !map-color is on.
*
* @param char_data *ch The player.
*/
void tog_mapcolor(char_data *ch) {
	if (PRF_FLAGGED(ch, PRF_NOMAPCOL) && !PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
		REMOVE_BIT(PRF_FLAGS(ch), PRF_POLITICAL | PRF_INFORMATIVE);
	}
}


/**
* Ensures informative/!map-color are off when political is on.
*
* @param char_data *ch The player.
*/
void tog_political(char_data *ch) {
	if (PRF_FLAGGED(ch, PRF_POLITICAL)) {
		REMOVE_BIT(PRF_FLAGS(ch), PRF_INFORMATIVE);
		if (!PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
			REMOVE_BIT(PRF_FLAGS(ch), PRF_NOMAPCOL);
		}
	}
}


/**
* Sets the pvp cooldown if needed.
*
* @param char_data *ch The player.
*/
void tog_pvp(char_data *ch) {
	if (!PRF_FLAGGED(ch, PRF_ALLOW_PVP)) {
		add_cooldown(ch, COOLDOWN_PVP_FLAG, config_get_int("pvp_timer") * SECS_PER_REAL_MIN);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

// also reject / do_reject (search hint)
ACMD(do_accept) {
	char type_arg[MAX_INPUT_LENGTH], name_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	int max_duration = config_get_int("offer_time");
	struct offer_data *ofiter, *offer;
	bool delete = FALSE;
	int iter, type, ts;
	bool found, dupe;
	char_data *from;
	
	const char *mode[] = { "accept", "reject" };	// SCMD_ACCEPT, SCMD_REJECT
	
	// OFFER_x - Data for the offer system, for do_accept/reject
	struct {
		char *name;
		int min_pos;
		OFFER_VALIDATE(*validate_func);	// returns TRUE to allow, FALSE to prevent accept
		OFFER_FINISH(*finish_func);	// returns TRUE if it's ok to delete the offer, FALSE if not
	} offer_types[] = {
		{ "resurrection", POS_DEAD, oval_rez, ofin_rez },	// OFFER_RESURRECTION: uses offer->data for ability
		{ "summon", POS_STANDING, oval_summon, ofin_summon },	// OFFER_SUMMON: uses offer->data for SUMMON_x
		{ "quest", POS_RESTING, oval_quest, ofin_quest},	// OFFER_QUEST: uses offer->data for quest vnum
		
		// end
		{ "\n", POS_DEAD, NULL, NULL }
	};

	two_arguments(argument, type_arg, name_arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot %s offers.\r\n", mode[subcmd]);
		return;
	}
	
	if (!*type_arg) {
		msg_to_char(ch, "Usage: %s <type> [person]\r\n", mode[subcmd]);
		if (GET_OFFERS(ch)) {
			msg_to_char(ch, "You have the following offers available:\r\n");
			
			found = FALSE;
			for (offer = GET_OFFERS(ch); offer; offer = offer->next) {
				if (!(from = is_playing(offer->from))) {
					continue;
				}
				
				ts = time(0) - offer->time;
				msg_to_char(ch, " %s - %s (%d seconds left)\r\n", PERS(from, ch, TRUE), offer_types[offer->type].name, MAX(0, max_duration - ts));
				found = TRUE;
			}
			if (!found) {
				msg_to_char(ch, " none\r\n");
			}
		}
		else {
			msg_to_char(ch, "You have no offers available to %s.\r\n", mode[subcmd]);
		}
		return;
	}
	
	// determine type
	type = -1;
	for (iter = 0; *offer_types[iter].name != '\n'; ++iter) {
		if (is_abbrev(type_arg, offer_types[iter].name)) {
			type = iter;
			break;
		}
	}
	if (type == -1) {
		msg_to_char(ch, "Invalid type.\r\n");
		return;
	}
	if (!offer_types[type].validate_func || !offer_types[type].finish_func) {
		msg_to_char(ch, "That offer type is not currently implemented.\r\n");
		return;
	}
	
	// find entry
	offer = NULL;
	dupe = FALSE;
	for (ofiter = GET_OFFERS(ch); ofiter; ofiter = ofiter->next) {
		if (ofiter->type != type) {
			continue;
		}
		if (!(from = is_playing(ofiter->from))) {
			continue;
		}
		if (*name_arg && !is_abbrev(name_arg, GET_NAME(from))) {
			continue;
		}
		
		// validated
		if (!offer) {
			offer = ofiter;
			if (*name_arg) {
				break;	// end early if they specified the name
			}
		}
		else {
			dupe = TRUE;	// whoops, more than 1 valid match
			break;
		}
	}
	
	if (!offer) {
		msg_to_char(ch, "You have no offer like that to %s.\r\n", mode[subcmd]);
		return;
	}
	if (dupe) {
		msg_to_char(ch, "You have more than one offer for %s. Please specify a name too.\r\n", offer_types[type].name);
		return;
	}
	if (GET_POS(ch) < offer_types[type].min_pos) {
		send_low_pos_msg(ch);
		return;
	}
	
	if (subcmd == SCMD_ACCEPT) {
		// ok, we have validated offer so far..
		if (!(offer_types[type].validate_func)(ch, offer)) {
			return;
		}
	
		// and finally
		delete = (offer_types[type].finish_func)(ch, offer);
	}
	else {
		msg_to_char(ch, "You reject the offer for %s.\r\n", offer_types[type].name);
		if ((from = is_playing(offer->from))) {
			snprintf(buf, sizeof(buf), "$N has rejected your offer for %s.", offer_types[type].name);
			act(buf, FALSE, from, NULL, ch, TO_CHAR);
		}
		delete = TRUE;
	}
	
	// in either case
	if (delete) {
		LL_DELETE(GET_OFFERS(ch), offer);
		free(offer);
	}
}


ACMD(do_alternate) {
	char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	struct account_player *plr;
	player_index_data *index;
	char_data *newch, *alt;
	bool is_file = FALSE, timed_out;
	int days, hours;
	size_t size;
	
	argument = any_one_arg(argument, arg);
	
	if (!ch->desc) {
		msg_to_char(ch, "You can't do anything without a player controlling you. Who's even reading this?\r\n");
	}
	else if (IS_NPC(ch) || ch->desc->original) {
		msg_to_char(ch, "You can't do that while switched!\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "This command lets you switch which character you're logged in with.\r\n");
		msg_to_char(ch, "Usage: alternate <character name | list | import>\r\n");
	}
	else if (!str_cmp(arg, "list")) {
		size = snprintf(buf, sizeof(buf), "Account characters:\r\n");
		
		for (plr = GET_ACCOUNT(ch)->players; plr; plr = plr->next) {
			if (!plr->player) {
				continue;
			}
					
			// load alt
			alt = find_or_load_player(plr->name, &is_file);
			if (!alt) {
				continue;
			}
		
			// display:
			timed_out = member_is_timed_out_ch(alt);
			get_player_skill_string(alt, part, TRUE);
			if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
				size += snprintf(buf + size, sizeof(buf) - size, "[%d %s %s] %s%s&0", !is_file ? GET_COMPUTED_LEVEL(alt) : GET_LAST_KNOWN_LEVEL(alt), part, class_role[GET_CLASS_ROLE(alt)], (timed_out ? "&r" : ""), PERS(alt, alt, TRUE));
			}
			else {	// not screenreader
				size += snprintf(buf + size, sizeof(buf) - size, "[%d %s%s\t0] %s%s&0", !is_file ? GET_COMPUTED_LEVEL(alt) : GET_LAST_KNOWN_LEVEL(alt), class_role_color[GET_CLASS_ROLE(alt)], part, (timed_out ? "&r" : ""), PERS(alt, alt, TRUE));
			}
						
			// online/not
			if (!is_file) {
				size += snprintf(buf + size, sizeof(buf) - size, "  - &conline&0%s", IS_AFK(alt) ? " - &rafk&0" : "");
			}
			else if ((time(0) - alt->prev_logon) < SECS_PER_REAL_DAY) {
				hours = (time(0) - alt->prev_logon) / SECS_PER_REAL_HOUR;
				size += snprintf(buf + size, sizeof(buf) - size, "  - %d hour%s ago%s", hours, PLURAL(hours), (timed_out ? ", &rtimed-out&0" : ""));
			}
			else {	// more than a day
				days = (time(0) - alt->prev_logon) / SECS_PER_REAL_DAY;
				size += snprintf(buf + size, sizeof(buf) - size, "  - %d day%s ago%s", days, PLURAL(days), (timed_out ? ", &rtimed-out&0" : ""));
			}
		
			size += snprintf(buf + size, sizeof(buf) - size, "\r\n");
		
			if (alt && is_file) {
				free_char(alt);
			}
		}
		
		// prevent rapid-use
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
		command_lag(ch, WAIT_OTHER);
	}
	else if (!str_cmp(arg, "import")) {
		do_alt_import(ch, argument);
	}
	else if (IN_HOSTILE_TERRITORY(ch)) {
		msg_to_char(ch, "You can't alternate in hostile territory.\r\n");
	}
	else if (GET_POS(ch) < POS_RESTING) {
		msg_to_char(ch, "You can't alternate right now.\r\n");
	}
	else if (GET_POS(ch) == POS_FIGHTING || FIGHTING(ch)) {
		msg_to_char(ch, "You can't switch characters while fighting!\r\n");
	}
	else if (ch->desc->str) {
		msg_to_char(ch, "You can't alternate while editing text (use ,/save or ,/abort first).\r\n");
	}
	else if (ch->desc->snooping) {
		msg_to_char(ch, "You can't alternate while snooping.\r\n");
	}
	else if (GET_OLC_TYPE(ch->desc) != 0) {
		msg_to_char(ch, "You can't alternate with an editor open (use .save or .abort first).\r\n");
	}
	else if (get_cooldown_time(ch, COOLDOWN_PVP_QUIT_TIMER) > 0 && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You can't alternate so soon after fighting!\r\n");
	}
	else if (!(index = find_player_index_by_name(arg))) {
		msg_to_char(ch, "Unknown character.\r\n");
	}
	else if ((newch = is_playing(index->idnum)) || (newch = is_at_menu(index->idnum))) {
		// in-game?
		
		if (GET_ACCOUNT(newch) != GET_ACCOUNT(ch)) {
			msg_to_char(ch, "That character isn't on your account.\r\n");
			return;
		}
		if (newch == ch) {
			msg_to_char(ch, "You're already playing that character.\r\n");
			return;
		}
		if (get_cooldown_time(ch, COOLDOWN_ALTERNATE) > 0 && !IS_IMMORTAL(newch)) {
			msg_to_char(ch, "You can't alternate again so soon.\r\n");
			return;
		}
		if (newch->desc || !IN_ROOM(newch)) {
			msg_to_char(ch, "You can't switch to that character because someone is already playing it.\r\n");
			return;
		}
		if (IS_APPROVED(newch) && !IS_IMMORTAL(ch) && has_anonymous_host(ch->desc)) {
			msg_to_char(ch, "You can't switch to an approved character from an anonymous public host.\r\n");
			return;
		}
		
		// seems ok
		perform_alternate(ch, newch);
	}
	else {
		// prepare
		newch = load_player(index->name, TRUE);
		
		if (!newch) {
			msg_to_char(ch, "Unknown character.\r\n");
			return;
		}
		
		// in case
		REMOVE_BIT(PLR_FLAGS(newch), PLR_MAILING);
				
		// ensure legal switch
		if (GET_ACCOUNT(newch) != GET_ACCOUNT(ch)) {
			msg_to_char(ch, "That character isn't on your account.\r\n");
			free_char(newch);
			return;
		}
		if (get_cooldown_time(ch, COOLDOWN_ALTERNATE) > 0 && !IS_IMMORTAL(newch)) {
			msg_to_char(ch, "You can't alternate again so soon.\r\n");
			free_char(newch);
			return;
		}
		// select ban?
		if (isbanned(ch->desc->host) == BAN_SELECT && !ACCOUNT_FLAGGED(newch, ACCT_SITEOK)) {
			msg_to_char(ch, "Sorry, your account has not been cleared for login from your site!\r\n");
			free_char(newch);
			return;
		}
		
		// seems ok!
		perform_alternate(ch, newch);
	}
}


ACMD(do_beckon) {
	char arg[MAX_INPUT_LENGTH];
	char_data *vict;
	bool any;
	
	one_argument(argument, arg);
	if (!*arg || !str_cmp(arg, "all")) {
		any = FALSE;
		// beckon all
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
			if (vict == ch || is_ignoring(vict, ch) || GET_LEADER(vict)) {
				continue;	// nopes
			}
			
			if (!IS_NPC(vict)) {
				GET_BECKONED_BY(vict) = GET_IDNUM(REAL_CHAR(ch));	// real: may be switched imm
			}
			any = TRUE;
		}
		
		if (any) {
			msg_to_char(ch, "You beckon for everyone to follow you.\r\n");
			act("$n beckons for everyone to follow $m.", FALSE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			msg_to_char(ch, "There is nobody here to beckon.\r\n");
		}
	}
	else if (!(vict = get_char_room_vis(ch, arg, NULL))) {
		send_config_msg(ch, "no_person");
	}
	else if (!IS_NPC(vict) && GET_BECKONED_BY(vict) == GET_IDNUM(REAL_CHAR(ch))) {
		act("You already beckoned $M.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (!IS_NPC(vict) && is_ignoring(vict, ch)) {
		act("You can't beckon $M.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (GET_LEADER(vict) == ch) {
		act("$E is already following you.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (GET_LEADER(vict)) {
		act("$E is already following someone else.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else {
		if (!IS_NPC(vict)) {
			GET_BECKONED_BY(vict) = GET_IDNUM(REAL_CHAR(ch));	// real: may be switched imm
		}
		act("You beckon for $N to follow you.", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n beckons for you to follow $m.", FALSE, ch, NULL, vict, TO_VICT);
		act("$n beckons for $N to follow $m.", FALSE, ch, NULL, vict, TO_NOTVICT);
	}
}


ACMD(do_changepass) {
	char oldpass[MAX_INPUT_LENGTH], new1[MAX_INPUT_LENGTH], new2[MAX_INPUT_LENGTH];
	// changepass OLD NEW NEW
	
	argument = any_one_word(argument, oldpass);
	argument = any_one_word(argument, new1);
	argument = any_one_word(argument, new2);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!*oldpass || !*new1 || !*new2) {
		msg_to_char(ch, "Usage: changepass <old pw> <new pw> <new again>\r\n");
	}
	else if (strncmp(CRYPT(oldpass, PASSWORD_SALT), GET_PASSWD(ch), MAX_PWD_LENGTH)) {
		msg_to_char(ch, "Unable to change password: old password does not match.\r\n");
	}
	else if (strcmp(new1, new2)) {
		msg_to_char(ch, "Unable to change password: new passwords do not match.\r\n");
	}
	else {
		if (GET_PASSWD(ch)) {
			free(GET_PASSWD(ch));
		}
		GET_PASSWD(ch) = str_dup(CRYPT(new1, PASSWORD_SALT));
		SAVE_CHAR(ch);
		
		syslog(SYS_INFO, GET_INVIS_LEV(ch), TRUE, "%s has changed %s password using changepass", GET_NAME(ch), REAL_HSHR(ch));
		if (ch->desc && ch->desc->snoop_by) {
			syslog(SYS_INFO, MIN(LVL_TOP, MAX(GET_INVIS_LEV(ch), GET_ACCESS_LEVEL(ch) + 1)), TRUE, "WARNING: %s changed password while being snooped", GET_NAME(ch));
		}
		msg_to_char(ch, "You have successfully changed your password.\r\n");
	}
}


ACMD(do_companions) {
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH];
	struct companion_data *cd, *next_cd, *found_cd;
	char_data *mob, *proto = NULL;
	struct companion_mod *cmod;
	ability_data *abil;
	size_t size, lsize;
	bool found, full, low_in_skill = FALSE;
	int found_low_level = 0;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't use this command.\r\n");
		return;
	}
	
	setup_ability_companions(ch);
	
	if (!*argument) {
		size = snprintf(buf, sizeof(buf), "You can summon the following companions:\r\n");
		
		found = full = FALSE;
		HASH_ITER(hh, GET_COMPANIONS(ch), cd, next_cd) {
			if (cd->from_abil != NOTHING && !has_ability(ch, cd->from_abil)) {
				continue;	// missing ability: don't show
			}
			if (!(proto = mob_proto(cd->vnum))) {
				continue;	// mob missing
			}
			
			// build display
			cmod = get_companion_mod_by_type(cd, CMOD_SHORT_DESC);
			lsize = snprintf(line, sizeof(line), " %s", skip_filler(cmod ? cmod->str : get_mob_name_by_proto(cd->vnum, TRUE)));
			
			if (cd->from_abil != NOTHING && (abil = find_ability_by_vnum(cd->from_abil)) && ABIL_COST(abil) > 0) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " (%d %s)", ABIL_COST(abil), pool_types[ABIL_COST_TYPE(abil)]);
			}
			
			if (GET_MIN_SCALE_LEVEL(proto) > 0 && (cd->from_abil ? get_player_level_for_ability(ch, cd->from_abil) : get_approximate_level(ch)) < GET_MIN_SCALE_LEVEL(proto)) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " \trrequires level %d\t0", GET_MIN_SCALE_LEVEL(proto));
			}
			
			strcat(line, "\r\n");
			lsize += 2;
			found = TRUE;
			
			if (size + lsize < sizeof(buf)) {
				strcat(buf, line);
				size += lsize;
			}
			else {
				full = TRUE;
				break;
			}
			
			if (full) {
				break;
			}
		}
		
		if (!found) {
			strcat(buf, " none\r\n");	// always room for this if !found
		}
		if (full) {
			snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
		}
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
		return;
	}
	
	// lookup
	found_cd = NULL;
	HASH_ITER(hh, GET_COMPANIONS(ch), cd, next_cd) {
		if (cd->from_abil != NOTHING && !has_ability(ch, cd->from_abil)) {
			continue;	// missing ability
		}
		if (!(proto = mob_proto(cd->vnum))) {
			continue;	// mob missing
		}
		if ((cmod = get_companion_mod_by_type(cd, CMOD_KEYWORDS)) && !multi_isname(argument, cmod->str)) {
			continue;	// has custom keywords and doesn't match
		}
		else if (!cmod && !multi_isname(argument, GET_PC_NAME(proto))) {
			continue;	// no custom keywords and doesn't match mob's keywords
		}
		if ((cd->from_abil && GET_MIN_SCALE_LEVEL(proto) < MAX_SKILL_CAP) && get_player_level_for_ability(ch, cd->from_abil) < GET_MIN_SCALE_LEVEL(proto)) {
			if (found_low_level == 0 || found_low_level < GET_MIN_SCALE_LEVEL(proto)) {
				found_low_level = GET_MIN_SCALE_LEVEL(proto);
				low_in_skill = ABIL_ASSIGNED_SKILL(find_ability_by_vnum(cd->from_abil)) ? TRUE : FALSE;
			}
			continue;	// low skill
		}
		if ((!cd->from_abil || GET_MIN_SCALE_LEVEL(proto) > MAX_SKILL_CAP) && get_approximate_level(ch) < GET_MIN_SCALE_LEVEL(proto)) {
			if (found_low_level == 0 || found_low_level < GET_MIN_SCALE_LEVEL(proto)) {
				found_low_level = GET_MIN_SCALE_LEVEL(proto);
			}
			continue;	// low skill
		}
		
		// seems to be found!
		found_cd = cd;
		break;
	}
	
	// fetch ability if any
	abil = (found_cd && found_cd->from_abil != NOTHING) ? find_ability_by_vnum(found_cd->from_abil) : NULL;
	
	// validate
	if (GET_COMPANION(ch)) {
		msg_to_char(ch, "You already have a companion. Dismiss it first.\r\n");
		return;
	}
	if (!found_cd && found_low_level > 0) {
		msg_to_char(ch, "You must be level %d%s to summon that companion.\r\n", found_low_level, (low_in_skill ? " in the skill" : ""));
		return;
	}
	if (!found_cd || !proto) {
		msg_to_char(ch, "You don't have a companion called '%s'.\r\n", argument);
		return;
	}
	if (abil && ABIL_IS_SYNERGY(abil) && !check_solo_role(ch)) {
		msg_to_char(ch, "You must be alone to summon that companion in the solo role.\r\n");
		return;
	}
	if (abil && !char_can_act(ch, ABIL_MIN_POS(abil), !ABILITY_FLAGGED(abil, ABILF_NO_ANIMAL), !ABILITY_FLAGGED(abil, ABILF_NO_INVULNERABLE | ABILF_VIOLENT), FALSE)) {
		return;
	}
	if (!abil && GET_POS(ch) < POS_STANDING) {
		send_low_pos_msg(ch);
		return;
	}
	if (abil && !can_use_ability(ch, ABIL_VNUM(abil), ABIL_COST_TYPE(abil), ABIL_COST(abil), ABIL_COOLDOWN(abil))) {
		return;
	}
	if (abil && !ABILITY_FLAGGED(abil, ABILF_IGNORE_SUN) && ABIL_COST(abil) > 0 && ABIL_COST_TYPE(abil) == BLOOD && !check_vampire_sun(ch, TRUE)) {
		return;
	}
	if (abil && ABILITY_TRIGGERS(ch, NULL, NULL, NULL, ABIL_VNUM(abil))) {
		return;
	}
	
	// pre-message, if any
	send_pre_ability_messages(ch, NULL, NULL, abil, NULL);
	
	// proceed:
	charge_ability_cost(ch, abil ? ABIL_COST_TYPE(abil) : MOVE, abil ? ABIL_COST(abil) : 0, abil ? ABIL_COOLDOWN(abil) : NOTHING, abil ? ABIL_COOLDOWN_SECS(abil) : 0, abil ? ABIL_WAIT_TYPE(abil) : WAIT_OTHER);
	if (abil && !skill_check(ch, ABIL_VNUM(abil), ABIL_DIFFICULTY(abil))) {
		send_ability_fail_messages(ch, NULL, NULL, abil, NULL);
		return;
	}
	
	mob = load_companion_mob(ch, found_cd);
	
	// messaging to char
	if (abil && abil_has_custom_message(abil, ABIL_CUSTOM_TARGETED_TO_CHAR)) {
		act(abil_get_custom_message(abil, ABIL_CUSTOM_TARGETED_TO_CHAR), FALSE, ch, NULL, mob, TO_CHAR);
	}
	else {
		act("You summon $N.", FALSE, ch, NULL, mob, TO_CHAR);
	}
	
	// messaging to room
	if (abil && abil_has_custom_message(abil, ABIL_CUSTOM_TARGETED_TO_ROOM)) {
		act(abil_get_custom_message(abil, ABIL_CUSTOM_TARGETED_TO_ROOM), ABILITY_FLAGGED(abil, ABILF_INVISIBLE) ? TRUE : FALSE, ch, NULL, mob, TO_NOTVICT);
	}
	else {
		act("$n summons $N.", FALSE, ch, NULL, mob, TO_NOTVICT);
	}
	
	if (abil) {
		gain_ability_exp(ch, ABIL_VNUM(abil), 15);
		run_ability_hooks(ch, AHOOK_ABILITY, ABIL_VNUM(abil), 0, mob, NULL, NULL, NULL, NOBITS);
	}
}


ACMD(do_confirm) {
	if (IS_NPC(ch)) {
		return;
	}

	if (GET_POS(ch) == POS_FIGHTING) {
		msg_to_char(ch, "How can you be ready?  You're fighting for your life!\r\n");
		return;
	}

	if (reboot_control.time < 0 || reboot_control.time > config_get_int("reboot_warning_minutes") || reboot_control.type == REBOOT_NONE) {
		msg_to_char(ch, "There is no upcoming reboot to confirm for!\r\n");
		return;
	}

	if (REBOOT_CONF(ch)) {
		msg_to_char(ch, "You've already confirmed that you're ready for the %s.\r\n", reboot_types[reboot_control.type]);
	}
	else {
		REBOOT_CONF(ch) = TRUE;
		msg_to_char(ch, "You have confirmed that you're ready for the %s.\r\n", reboot_types[reboot_control.type]);
	}

	if (check_reboot_confirms() && reboot_control.time <= config_get_int("reboot_warning_minutes")) {
		reboot_control.immediate = TRUE;
	}
}


ACMD(do_customize) {
	char arg2[MAX_INPUT_LENGTH];
	
	half_chop(argument, arg, arg2);
	
	if (ACCOUNT_FLAGGED(ch, ACCT_NOCUSTOMIZE)) {
		msg_to_char(ch, "You are not allowed to customize anything anymore.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "What do you want to customize? (See HELP CUSTOMIZE)\r\n");
	}
	else if (is_abbrev(arg, "building") || is_abbrev(arg, "room")) {
		do_customize_room(ch, arg2);
	}
	else if (is_abbrev(arg, "island") || is_abbrev(arg, "isle")) {
		do_customize_island(ch, arg2);
	}
	else if (is_abbrev(arg, "vehicle") || is_abbrev(arg, "ship")) {
		do_customize_vehicle(ch, arg2);
	}
	else if (is_abbrev(arg, "road")) {
		do_customize_road(ch, arg2);
	}
	else {
		msg_to_char(ch, "What do you want to customize? (See HELP CUSTOMIZE)\r\n");
	}
}


ACMD(do_dismiss) {
	char_data *vict;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Dismiss whom?\r\n");
	}
	else if (!strn_cmp(arg, "comp", 4) && is_abbrev(arg, "companion")) {
		// requires abbrev of at least "comp"
		if (!despawn_companion(ch, NOTHING)) {
			msg_to_char(ch, "You do not have a companion to dismiss.\r\n");
		}
		else {
			send_config_msg(ch, "ok_string");
		}
	}
	else if (!strn_cmp(arg, "mini", 4) && (is_abbrev(arg, "minipet") || is_abbrev(arg, "mini-pet"))) {
		// requires abbrev of at least "famil"
		if (!dismiss_any_minipet(ch)) {
			msg_to_char(ch, "You do not have a minipet to dismiss.\r\n");
		}
		else {
			send_config_msg(ch, "ok_string");
		}
	}
	else if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (!IS_NPC(vict) || GET_LEADER(vict) != ch || (!GET_COMPANION(vict) && !IS_MINIPET_OF(vict, ch))) {
		msg_to_char(ch, "You can only dismiss a companion or minipet.\r\n");
	}
	else if (FIGHTING(vict) || GET_POS(vict) < POS_SLEEPING) {
		act("You can't dismiss $M right now.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else {
		act("$n leaves.", TRUE, vict, NULL, NULL, TO_ROOM);
		extract_char(vict);
	}
}


ACMD(do_douse) {
	room_data *room = HOME_ROOM(IN_ROOM(ch));
	obj_data *obj = NULL, *found_obj = NULL, *iter;
	char arg[MAX_INPUT_LENGTH];
	vehicle_data *veh;
	bool use_room;
	
	// prefer a room source
	use_room = (IS_IMMORTAL(ch) || WATER_SECT(IN_ROOM(ch)) || ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_DRINK) || room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_DRINK_WATER));
	
	if (!use_room) {
		// this loop finds a water container and sets obj
		DL_FOREACH2(ch->carrying, iter, next_content) {
			if (liquid_flagged(GET_DRINK_CONTAINER_TYPE(iter), LIQF_WATER) && GET_DRINK_CONTAINER_CONTENTS(iter) > 0) {
				obj = iter;
				break;
			}
		}
	}
	
	one_argument(argument, arg);
	
	if (!obj && !use_room) {
		msg_to_char(ch, "You have nothing to douse the fire with!\r\n");
	}
	else if (*arg && str_cmp(arg, "fire")) {
		if ((veh = get_vehicle_in_room_vis(ch, arg, NULL))) {
			do_douse_vehicle(ch, veh, obj);
		}
		else if (GET_ROOM_VEHICLE(IN_ROOM(ch)) && isname(arg, VEH_KEYWORDS(GET_ROOM_VEHICLE(IN_ROOM(ch))))) {
			do_douse_vehicle(ch, GET_ROOM_VEHICLE(IN_ROOM(ch)), obj);
		}
		else if (generic_find(arg, NULL, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, NULL, &found_obj, NULL)) {
			do_douse_obj(ch, found_obj, obj);
		}
		else {
			msg_to_char(ch, "You don't see %s %s to douse!\r\n", AN(arg), arg);
		}
	}
	else if (GET_ROOM_VEHICLE(IN_ROOM(ch)) && VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(ch)), VEH_ON_FIRE)) {
		do_douse_vehicle(ch, GET_ROOM_VEHICLE(IN_ROOM(ch)), obj);
	}
	else if (!IS_ANY_BUILDING(IN_ROOM(ch)) || !IS_BURNING(room)) {
		msg_to_char(ch, "There's no fire here!\r\n");
	}
	else {
		do_douse_room(ch, room, obj);
	}
}


// this now also controls status messages using SCMD_FIGHT or SCMD_STATUS
// search hints: do_fmessages, do_smessages, do_statusmessages
ACMD(do_fightmessages) {
	bool screenreader = PRF_FLAGGED(ch, PRF_SCREEN_READER);
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	int iter, type = NOTHING, count;
	bool on = FALSE, off = FALSE, mode;
	
	// SCMD_FIGHT, SCMD_STATUS
	const char *message_type[] = { "fight", "status" };
	const char **type_strings = NULL;
	bitvector_t *flagset = NULL;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs do not have %s message toggles.\r\n", message_type[subcmd]);
		return;
	}
	
	// verify type
	if (subcmd == SCMD_FIGHT) {
		flagset = &GET_FIGHT_MESSAGES(ch);
		type_strings = combat_message_types;
	}
	else if (subcmd == SCMD_STATUS) {
		flagset = &GET_STATUS_MESSAGES(ch);
		type_strings = status_message_types;
	}
	else {
		msg_to_char(ch, "Command not implemented.\r\n");
		return;
	}
	
	// argument processing
	skip_spaces(&argument);
	chop_last_arg(argument, arg1, arg2);
	
	// detect optional on/off
	if (!str_cmp(arg2, "on")) {
		on = TRUE;
		argument = arg1;	// use only arg1
	}
	else if (!str_cmp(arg2, "off")) {
		off = TRUE;
		argument = arg1;	// use only arg1
	}
	// otherwise did not provide on/off arg
	
	// detect possible type from remaining arg
	if (*argument) {
		for (iter = 0; *type_strings[iter] != '\n'; ++iter) {
			if (is_multiword_abbrev(argument, type_strings[iter])) {
				type = iter;
				break;
			}
		}
	}
	
	if (!*argument) {
		snprintf(buf, sizeof(buf), "%s message toggles:\r\n", message_type[subcmd]);
		send_to_char(CAP(buf), ch);
		
		count = 0;
		for (iter = 0; *type_strings[iter] != '\n'; ++iter) {
			on = (IS_SET(*flagset, BIT(iter)) ? TRUE : FALSE);
			if (screenreader) {
				msg_to_char(ch, "%s: %s\r\n", type_strings[iter], on ? "on" : "off");
			}
			else {
				msg_to_char(ch, " [%s%3.3s\t0] %-25.25s%s", on ? "\tg" : "\tr", on ? "on" : "off", type_strings[iter], (!(++count % 2) ? "\r\n" : ""));
			}
		}
		
		if (count % 2 && !screenreader) {
			send_to_char("\r\n", ch);
		}
	}
	else if (!str_cmp(argument, "all")) {
		if (!on && !off) {
			msg_to_char(ch, "Did you want to turn them all on or off?\r\n");
			return;
		}
		// turn ON/OFF all bits that have names
		for (iter = 0; *type_strings[iter] != '\n'; ++iter) {
			if (on) {
				SET_BIT(*flagset, BIT(iter));
			}
			else {
				REMOVE_BIT(*flagset, BIT(iter));
			}
		}
		msg_to_char(ch, "You toggle all %s messages %s\t0.\r\n", message_type[subcmd], on ? "\tgon" : "\troff");
	}
	else if (type == NOTHING) {
		msg_to_char(ch, "Unknown %s message type '%s'.\r\n", message_type[subcmd], argument);
	}
	else {
		mode = (on ? TRUE : (off ? FALSE : (IS_SET(*flagset, BIT(type)) ? FALSE : TRUE)));
		if (mode) {
			SET_BIT(*flagset, BIT(type));
		}
		else {
			REMOVE_BIT(*flagset, BIT(type));
		}
		
		msg_to_char(ch, "You toggle '%s' %s\t0.\r\n", type_strings[type], mode ? "\tgon" : "\troff");
	}
}


ACMD(do_gen_write) {
	char locpart[256];
	const char *filename;
	struct stat fbuf;
	crop_data *cp;
	char *name;
	time_t ct;
	char *tmp;
	FILE *fl;

	switch (subcmd) {
		case SCMD_BUG: {
			filename = BUG_FILE;
			name = "bug";
			break;
		}
		case SCMD_TYPO: {
			filename = TYPO_FILE;
			name = "typo";
			break;
		}
		case SCMD_IDEA: {
			filename = IDEA_FILE;
			name = "idea";
			break;
		}
		default: {
			return;
		}
	}

	ct = time(0);
	tmp = asctime(localtime(&ct));

	if (IS_NPC(ch)) {
		send_to_char("Monsters can't have ideas - Go away.\r\n", ch);
		return;
	}

	skip_spaces(&argument);
	delete_doubledollar(argument);

	if (!*argument) {
		send_to_char("That must be a mistake...\r\n", ch);
		return;
	}
	syslog(SYS_INFO, GET_INVIS_LEV(ch), FALSE, "%s %s: %s", GET_NAME(ch), name, show_color_codes(argument));

	if (stat(filename, &fbuf) < 0) {
		perror("SYSERR: Can't stat() file");
		return;
	}
	if (fbuf.st_size >= config_get_int("max_filesize")) {
		send_to_char("Sorry, the file is full right now... try again later.\r\n", ch);
		syslog(SYS_INFO, GET_INVIS_LEV(ch), FALSE, "Warning: %s file is full", name);
		return;
	}
	if (!(fl = fopen(filename, "a"))) {
		perror("SYSERR: do_gen_write");
		send_to_char("Could not open the file. Sorry.\r\n", ch);
		return;
	}
	
	if (GET_ROOM_TEMPLATE(IN_ROOM(ch))) {
		snprintf(locpart, sizeof(locpart), " [RMT%d]", GET_RMT_VNUM(GET_ROOM_TEMPLATE(IN_ROOM(ch))));
	}
	else if (GET_BUILDING(IN_ROOM(ch))) {
		snprintf(locpart, sizeof(locpart), " [BLD%d]", GET_BLD_VNUM(GET_BUILDING(IN_ROOM(ch))));
	}
	else if ((cp = ROOM_CROP(IN_ROOM(ch)))) {
		snprintf(locpart, sizeof(locpart), " [CRP%d]", GET_CROP_VNUM(cp));
	}
	else {
		snprintf(locpart, sizeof(locpart), " [%d]", GET_ROOM_VNUM(IN_ROOM(ch)));
	}
	
	fprintf(fl, "%-8s (%6.6s)%s %s\n", GET_NAME(ch), (tmp + 4), locpart, argument);
	fclose(fl);
	
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_to_char("Okay. Thanks!\r\n", ch);
	}
	else {
		msg_to_char(ch, "You submit %s %s: %s\r\n", AN(name), name, show_color_codes(argument));
		msg_to_char(ch, "Thanks!\r\n");
	}
}


/* Vatiken's Group System: Version 1.1, updated for EmpireMUD by Paul Clarke */
ACMD(do_group) {
	char buf[MAX_STRING_LENGTH];
	char_data *vict;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use group commands.\r\n");
		return;
	}

	argument = one_argument(argument, buf);

	// no-arg
	if (!*buf) {
		if (!PRF_FLAGGED(ch, PRF_NO_TUTORIALS)) {
			msg_to_char(ch, "Available group options: new, invite, join, kick, leave, leader, options\r\n");
		}
		
		if (GROUP(ch)) {
			// should we replace this with the group summary? -pc
			print_group(ch);
		}
		else if ((vict = is_playing(GET_GROUP_INVITE(ch)))) {
			msg_to_char(ch, "You have been invited to a group by %s.\r\n", PERS(vict, vict, TRUE));
		}
		else {
			msg_to_char(ch, "You are not in a group.\r\n");
		}
		return;
	}

	if (is_abbrev(buf, "new")) {
		if (GROUP(ch)) {
			msg_to_char(ch, "You are already in a group.\r\n");
		}
		else {
			create_group(ch);
		}
	}
	else if (is_abbrev(buf, "invite")) {
		skip_spaces(&argument);
		if (GROUP(ch) && GROUP_LEADER(GROUP(ch)) != ch) {
			msg_to_char(ch, "Only the group's leader can invite members.\r\n");
			return;
		}
		else if (!(vict = get_player_vis(ch, argument, FIND_CHAR_WORLD | FIND_NO_DARK))) {
			msg_to_char(ch, "Invite whom?\r\n");
			return;
		}
		else if (vict == ch) {
			msg_to_char(ch, "You can't invite yourself.\r\n");
			return;
		}
		else if (IS_NPC(vict)) {
			msg_to_char(ch, "You can't invite NPCs to a group.\r\n");
			return;
		}
		else if (is_ignoring(vict, ch)) {
			msg_to_char(ch, "You can't invite someone who is ignoring you to a group.\r\n");
			return;
		}
		else if (GROUP(vict)) {
			msg_to_char(ch, "Your target is already in a group.\r\n");
			return;
		}
		else if (GROUP(ch) && count_group_members(GROUP(ch)) >= MAX_GROUP_SIZE) {
			msg_to_char(ch, "The group is already full.\r\n");
			return;
		}
		
		// make one if needed
		if (!GROUP(ch)) {
			create_group(ch);
		}

		msg_to_char(ch, "You have invited %s to the group.\r\n", PERS(vict, vict, FALSE));
		
		if (!IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_GSAY)) {
			msg_to_char(vict, "&%c[group] %s has invited you to join a group.&0\r\n", GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_GSAY), GET_NAME(ch));
		}
		else {
			msg_to_char(vict, "[&Ggroup&0] %s has invited you to join a group.\r\n", GET_NAME(ch));
		}
		
		GET_GROUP_INVITE(vict) = GET_IDNUM(ch);
		if (!GET_LEADER(vict)) {
			// auto-beckon
			GET_BECKONED_BY(vict) = GET_IDNUM(REAL_CHAR(ch));
		}
	}
	else if (is_abbrev(buf, "join")) {
		if (!(vict = is_playing(GET_GROUP_INVITE(ch)))) {
			msg_to_char(ch, "You don't have an open group invitation.\r\n");
			return;
		}
		else if (GROUP(ch)) {
			msg_to_char(ch, "You are already in a group.\r\n");
			return;
		}
		else if (!GROUP(vict)) {
			msg_to_char(ch, "Your group invitation is no longer valid.\r\n");
			return;
		}
		else if (!GROUP(vict)) {
			msg_to_char(ch, "They are not a part of a group!\r\n");
			return;
		}
		else if (count_group_members(GROUP(vict)) >= MAX_GROUP_SIZE) {
			msg_to_char(ch, "That group is full now.\r\n");
			return;
		}

		GET_GROUP_INVITE(ch) = 0;
		join_group(ch, GROUP(vict)); 
	}
	else if (is_abbrev(buf, "kick")) {
		skip_spaces(&argument);
		if (!(vict = get_player_vis(ch, argument, FIND_CHAR_WORLD | FIND_NO_DARK))) {
			msg_to_char(ch, "Kick out whom?\r\n");
			return;
		}
		else if (vict == ch) {
			msg_to_char(ch, "There are easier ways to leave the group.\r\n");
			return;
		}
		else if (!GROUP(ch) ) {
			msg_to_char(ch, "But you are not part of a group.\r\n");
			return;
		}
		else if (GROUP_LEADER(GROUP(ch)) != ch ) {
			msg_to_char(ch, "Only the group's leader can kick members out.\r\n");
			return;
		}
		else if (GROUP(vict) != GROUP(ch)) {
			msg_to_char(ch, "They are not a member of your group!\r\n");
			return;
		} 
		msg_to_char(ch, "You have kicked %s out of the group.\r\n", GET_NAME(vict));
		msg_to_char(vict, "You have been kicked out of the group.\r\n"); 
		leave_group(vict);
	}
	else if (is_abbrev(buf, "leave")) {
		if (!GROUP(ch)) {
			msg_to_char(ch, "But you aren't a part of a group!\r\n");
			return;
		}

		leave_group(ch);
	}
	else if (is_abbrev(buf, "leader")) {
		skip_spaces(&argument);
		if (!(vict = get_player_vis(ch, argument, FIND_CHAR_WORLD | FIND_NO_DARK))) {
			msg_to_char(ch, "Set the leader to whom?\r\n");
			return;
		}
		else if (!GROUP(ch)) {
			msg_to_char(ch, "But you are not part of a group.\r\n");
			return;
		}
		else if (GROUP_LEADER(GROUP(ch)) != ch) {
			msg_to_char(ch, "Only the group's leader can change who the leader is.\r\n");
			return;
		}
		else if (vict == ch) {
			msg_to_char(ch, "Aren't you already the leader.\r\n");
			return;
		}
		else if (GROUP(vict) != GROUP(ch)) {
			msg_to_char(ch, "They are not a member of your group!\r\n");
			return;
		}
		GET_LEADER(GROUP(ch)) = vict;
		send_to_group(NULL, GROUP(ch), "%s is now group leader.", GET_NAME(vict));
		send_config_msg(ch, "ok_string");
	}
	else if (is_abbrev(buf, "options")) {
		skip_spaces(&argument);
		if (!GROUP(ch)) {
			msg_to_char(ch, "But you aren't part of a group!\r\n");
			return;
		}
		else if (GROUP_LEADER(GROUP(ch)) != ch) {
			msg_to_char(ch, "Only the group leader can adjust the group options.\r\n");
			return;
		}
		else if (is_abbrev(argument, "anonymous")) {
			TOGGLE_BIT(GROUP_FLAGS(GROUP(ch)), GROUP_ANON);
			msg_to_char(ch, "The group location is now %s to other players.\r\n", IS_SET(GROUP_FLAGS(GROUP(ch)), GROUP_ANON) ? "invisible" : "visible");
		}
		else 
			msg_to_char(ch, "The flag options are: anonymous\r\n");
	}
	else {
		msg_to_char(ch, "Invalid group option. See HELP GROUP for more info.\r\n");		
	}
}


ACMD(do_herd) {
	char mob_arg[MAX_INPUT_LENGTH], dir_arg[MAX_INPUT_LENGTH];
	vehicle_data *into_veh;
	room_data *to_room;
	char_data *victim;
	int dir;

	two_arguments(argument, mob_arg, dir_arg);

	if (IS_NPC(ch))
		return;
	else if (!has_player_tech(ch, PTECH_HERD_COMMAND)) {
		msg_to_char(ch, "You don't have the correct ability to herd animals.\r\n");
	}
	else if (!*mob_arg || !*dir_arg) {
		msg_to_char(ch, "Who do you want to herd, and which direction?\r\n");
	}
	
	// location checks
	else if (ROOM_IS_CLOSED(IN_ROOM(ch)) && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to herd here.\r\n");
	}
	else if (IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't herd anything here.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN)) {
		msg_to_char(ch, "You find it difficult to do that here.\r\n");
	}
	
	// determine/validate target
	else if (!(victim = get_char_vis(ch, mob_arg, NULL, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (victim == ch)
		msg_to_char(ch, "You can't herd yourself.\r\n");
	else if (!IS_NPC(victim) || victim->desc || !MOB_FLAGGED(victim, MOB_ANIMAL))
		msg_to_char(ch, "You can only herd animals.\r\n");
	else if (IS_NPC(victim) && MOB_FLAGGED(victim, MOB_HARD | MOB_GROUP | MOB_AGGRESSIVE)) {
		act("You can't herd $N!", FALSE, ch, NULL, victim, TO_CHAR);
	}
	else if (GET_POS(victim) < POS_STANDING || MOB_FLAGGED(victim, MOB_TIED)) {
		act("You can't herd $M right now.", FALSE, ch, NULL, victim, TO_CHAR);
	}
	else if (GET_LED_BY(victim) && IN_ROOM(GET_LED_BY(victim)) == IN_ROOM(victim)) {
		msg_to_char(ch, "You can't herd someone who is being led by someone else.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_ROUGH) && !MOB_FLAGGED(victim, MOB_MOUNTAINWALK)) {
		msg_to_char(ch, "You find it difficult to do that here.\r\n");
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_HERD_COMMAND, victim, NULL, NULL)) {
		// triggered
	}
	
	// herd out of vehicle:
	else if (!str_cmp(dir_arg, "out") || !str_cmp(dir_arg, "exit") || !str_cmp(dir_arg, "outside")) {
		if (IS_OUTDOORS(ch) || !ROOM_CAN_EXIT(IN_ROOM(ch)) || !(to_room = get_exit_room(IN_ROOM(ch)))) {
			msg_to_char(ch, "There's no exit here to herd anybody out of.\r\n");
		}
		else {
			perform_herd(ch, victim, to_room, NO_DIR, NULL);
		}
	}
	
	// herd in a direction:
	else if ((dir = parse_direction(ch, dir_arg)) != NO_DIR) {
		if (!(to_room = dir_to_room(IN_ROOM(ch), dir, FALSE))) {
			msg_to_char(ch, "You can't herd anything in that direction.\r\n");
		}
		else {
			perform_herd(ch, victim, to_room, dir, NULL);
		}
	}
	
	// herd into vehicle
	else if ((into_veh = get_vehicle_in_room_vis(ch, dir_arg, NULL))) {
		if (VEH_INTERIOR_ROOM_VNUM(into_veh) != NOTHING && !VEH_IS_COMPLETE(into_veh)) {
		    msg_to_char(ch, "You can't herd anything into a %s that isn't complete.\r\n", VEH_OR_BLD(into_veh));
		}
		else if (!VEH_FLAGGED(into_veh, VEH_CARRY_MOBS) || !(to_room = get_vehicle_interior(into_veh))) {
			msg_to_char(ch, "You can't herd anything into that.\r\n");
		}
		else if (!VEH_IS_COMPLETE(into_veh)) {
			// this is almost certainly already checked, but it doesn't hurt to check
		    msg_to_char(ch, "You can't herd anything into a %s that isn't complete.\r\n", VEH_OR_BLD(into_veh));
		}
		else {
			perform_herd(ch, victim, to_room, NO_DIR, into_veh);
		}
	}
	
	// no valid target
	else {
		msg_to_char(ch, "Usage: herd <animal> <direction | vehicle | building | out>\r\n");
	}
}


ACMD(do_lastname) {
	char arg1[MAX_INPUT_LENGTH], new_name[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	char *arg2, *best, *exact;
	struct player_lastname *lastn;
	size_t size;
	int count;
	
	// we assume 'lastname <change | set | list> <name>' but also keep 'argument' whole too
	arg2 = any_one_arg(argument, arg1);
	skip_spaces(&argument);
	skip_spaces(&arg2);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot do this.\r\n");
	}
	else if (!IS_SET(config_get_bitvector("lastname_mode"), LASTNAME_CHANGE_ANY_TIME | LASTNAME_CHOOSE_FROM_LIST)) {
		// not allowed to pick
		if (GET_CURRENT_LASTNAME(ch)) {
			msg_to_char(ch, "Your lastname is: %s\r\n", GET_CURRENT_LASTNAME(ch));
		}
		else {
			msg_to_char(ch, "You cannot set your own lastname.\r\n");
		}
	}
	else if (!*argument) {	// no arg
		// usage?
		if (IS_SET(config_get_bitvector("lastname_mode"), LASTNAME_CHANGE_ANY_TIME) && IS_SET(config_get_bitvector("lastname_mode"), LASTNAME_CHOOSE_FROM_LIST)) {
			msg_to_char(ch, "Usage: lastname [change | list] [name | none]\r\n");
		}
		else if (IS_SET(config_get_bitvector("lastname_mode"), LASTNAME_CHANGE_ANY_TIME)) {
			msg_to_char(ch, "Usage: lastname [change] [name | none]\r\n");
		}
		else if (IS_SET(config_get_bitvector("lastname_mode"), LASTNAME_CHOOSE_FROM_LIST)) {
			msg_to_char(ch, "Usage: lastname [list] [name | none]\r\n");
		}
		
		// and show current
		if (GET_CURRENT_LASTNAME(ch)) {
			msg_to_char(ch, "Your lastname is: %s\r\n", GET_CURRENT_LASTNAME(ch));
		}
		else {
			msg_to_char(ch, "You don't have a lastname.\r\n");
		}
	}
	else if (is_abbrev(arg1, "change")) {
		// player wants to change their self-chosen lastname
		if (!IS_SET(config_get_bitvector("lastname_mode"), LASTNAME_CHANGE_ANY_TIME)) {
			msg_to_char(ch, "You cannot %s your own lastname.\r\n", GET_PERSONAL_LASTNAME(ch) ? "change" : "choose");
		}
		else if (!*arg2) {
			msg_to_char(ch, "Change your lastname to what?\r\n");
		}
		else if (!str_cmp(arg2, "none")) {
			change_personal_lastname(ch, NULL);
			msg_to_char(ch, "You no longer have a personal lastname.\r\n");
			syslog(SYS_INFO, GET_INVIS_LEV(ch), TRUE, "%s has changed personal lastname to: none", GET_NAME(ch));
		}
		else if (_parse_name(arg2, new_name, ch->desc, FALSE)) {
			// sends own error message if it return 1
		}
		else if (!Valid_Name(new_name)) {
			msg_to_char(ch, "You cannot be called '%s'.\r\n", new_name);
		}
		else {
			// ok!
			change_personal_lastname(ch, new_name);
			if (GET_CURRENT_LASTNAME(ch)) {
				free(GET_CURRENT_LASTNAME(ch));
			}
			GET_CURRENT_LASTNAME(ch) = str_dup(GET_PERSONAL_LASTNAME(ch));
			msg_to_char(ch, "Your personal lastname is now: %s\r\n", GET_PERSONAL_LASTNAME(ch));
			syslog(SYS_INFO, GET_INVIS_LEV(ch), TRUE, "%s has changed personal lastname to: %s", GET_NAME(ch), GET_PERSONAL_LASTNAME(ch));
		}
	}
	else if (is_abbrev(arg1, "list") && IS_SET(config_get_bitvector("lastname_mode"), LASTNAME_CHOOSE_FROM_LIST)) {
		if (!GET_PERSONAL_LASTNAME(ch) && !GET_LASTNAME_LIST(ch)) {
			msg_to_char(ch, "You don't have any lastnames to list.\r\n");
		}
		else {	// list them all
			count = 0;
			if (*arg2) {
				size = snprintf(output, sizeof(output), "Lastnames matching '%s':\r\n", arg2);
			}
			else {
				size = snprintf(output, sizeof(output), "Your lastnames:\r\n");
			}
		
			if (GET_PERSONAL_LASTNAME(ch)) {
				++count;
				size += snprintf(output + size, sizeof(output) - size, " %s (personal)\r\n", GET_PERSONAL_LASTNAME(ch));
			}
		
			LL_FOREACH(GET_LASTNAME_LIST(ch), lastn) {
				if (*arg2 && !multi_isname(arg2, lastn->name)) {
					continue;	// searched
				}
		
				// show it
				++count;
				snprintf(line, sizeof(line), " %s\r\n", NULLSAFE(lastn->name));
				if (size + strlen(line) < sizeof(output)) {
					strcat(output, line);
					size += strlen(line);
				}
				else {
					if (size + 10 < sizeof(output)) {
						strcat(output, "OVERFLOW\r\n");
					}
					break;
				}
			}
	
			if (!count) {
				strcat(output, " none\r\n");	// space reserved for this for sure
			}
	
			if (ch->desc) {
				page_string(ch->desc, output, TRUE);
			}
		}
	}
	else if (IS_SET(config_get_bitvector("lastname_mode"), LASTNAME_CHOOSE_FROM_LIST)) {
		if (!str_cmp(argument, "none")) {
			// just shutting it off?
			if (GET_CURRENT_LASTNAME(ch)) {
				free(GET_CURRENT_LASTNAME(ch));
			}
			GET_CURRENT_LASTNAME(ch) = NULL;
			queue_delayed_update(ch, CDU_SAVE);
			msg_to_char(ch, "You stop using a lastname.\r\n");
			return;
		}
		
		// set current lastname (using whole arg)
		best = exact = NULL;
		
		// check personal first
		if (GET_PERSONAL_LASTNAME(ch)) {
			if (!str_cmp(argument, GET_PERSONAL_LASTNAME(ch))) {
				exact = GET_PERSONAL_LASTNAME(ch);
			}
			else if (multi_isname(argument, GET_PERSONAL_LASTNAME(ch))) {
				best = GET_PERSONAL_LASTNAME(ch);
			}
		}
		
		// no exact? check the list
		if (!exact) {
			LL_FOREACH(GET_LASTNAME_LIST(ch), lastn) {
				if (lastn->name && !str_cmp(argument, lastn->name)) {
					exact = lastn->name;
					break;
				}
				else if (lastn->name && !best && multi_isname(argument, lastn->name)) {
					best = lastn->name;
				}
			}
		}
		
		// found any?
		if (!exact && !best) {
			msg_to_char(ch, "You don't have a lastname called '%s'.\r\n", argument);
		}
		else {	// change it
			if (GET_CURRENT_LASTNAME(ch)) {
				free(GET_CURRENT_LASTNAME(ch));
			}
			GET_CURRENT_LASTNAME(ch) = str_dup(exact ? exact : best);
			queue_delayed_update(ch, CDU_SAVE);
			msg_to_char(ch, "Your lastname is now: %s\r\n", GET_CURRENT_LASTNAME(ch));
		}
	}
	else {
		msg_to_char(ch, "Invalid lastname command.\r\n");
	}
}


ACMD(do_milk) {
	char_data *mob;
	obj_data *cont;
	int amount;

	two_arguments(argument, arg, buf);

	if (!has_player_tech(ch, PTECH_MILK_COMMAND)) {
		msg_to_char(ch, "You don't have the correct ability to milk animals.\r\n");
	}
	else if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_STABLE))
		msg_to_char(ch, "You can't milk animals here!\r\n");
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to finish building the stable before you can milk anything.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED))
		msg_to_char(ch, "You don't have permission to milk these animals.\r\n");
	else if (!*arg || !*buf)
		msg_to_char(ch, "What would you like to milk, and into what?\r\n");
	else if (!(mob = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (!IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_MILKABLE))
		act("You can't milk $N!", FALSE, ch, 0, mob, TO_CHAR);
	else if (get_cooldown_time(mob, COOLDOWN_MILK) > 0)
		act("$E can't be milked again for a while.", FALSE, ch, 0, mob, TO_CHAR);
	else if (!(cont = get_obj_in_list_vis(ch, buf, NULL, ch->carrying)))
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(buf), buf);
	else if (GET_OBJ_TYPE(cont) != ITEM_DRINKCON)
		act("You can't milk $N into $p!", FALSE, ch, cont, mob, TO_CHAR);
	else if (GET_DRINK_CONTAINER_CONTENTS(cont) > 0 && GET_DRINK_CONTAINER_TYPE(cont) != LIQ_MILK)
		msg_to_char(ch, "It's already full of something else.\r\n");
	else if (GET_DRINK_CONTAINER_CONTENTS(cont) >= GET_DRINK_CONTAINER_CAPACITY(cont))
		msg_to_char(ch, "It's already full.\r\n");
	else if (run_ability_triggers_by_player_tech(ch, PTECH_MILK_COMMAND, mob, cont, NULL)) {
		// triggered
	}
	else {
		act("You milk $N into $p.", FALSE, ch, cont, mob, TO_CHAR);
		act("$n milks $N into $p.", FALSE, ch, cont, mob, TO_ROOM);
		add_cooldown(mob, COOLDOWN_MILK, 2 * SECS_PER_MUD_DAY);
		amount = GET_DRINK_CONTAINER_CAPACITY(cont) - GET_DRINK_CONTAINER_CONTENTS(cont);
		set_obj_val(cont, VAL_DRINK_CONTAINER_CONTENTS, GET_DRINK_CONTAINER_CONTENTS(cont) + amount);
		set_obj_val(cont, VAL_DRINK_CONTAINER_TYPE, LIQ_MILK);
		GET_OBJ_TIMER(cont) = 72;	// mud hours
		schedule_obj_timer_update(cont, FALSE);
		gain_player_tech_exp(ch, PTECH_MILK_COMMAND, 33);
		run_ability_hooks_by_player_tech(ch, PTECH_MILK_COMMAND, mob, cont, NULL, NULL);
		request_obj_save_in_world(cont);
	}
}


ACMD(do_minipets) {
	char output[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH];
	struct minipet_data *mini, *next_mini;
	char_data *mob, *to_summon;
	size_t size;
	int count, number;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "Mobs don't get minipets.\r\n");
		return;
	}
	if (!ch->desc) {
		return;	// no point, nothing to show
	}
	
	if (!*argument) {	// just list minipets
		size = snprintf(output, sizeof(output), "Minipets in your collection:\r\n");
		count = 0;
	
		HASH_ITER(hh, GET_MINIPETS(ch), mini, next_mini) {
			if (!(mob = mob_proto(mini->vnum))) {
				continue;	// no mob
			}
		
			// ok:
			++count;
		
			if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
				snprintf(line, sizeof(line), "%s\r\n", skip_filler(GET_SHORT_DESC(mob)));
			}
			else {	// non-screenreader
				snprintf(line, sizeof(line), " %-36.36s%s", skip_filler(GET_SHORT_DESC(mob)), !(count % 2) ? "\r\n" : "");
			}
		
			if (size + strlen(line) + 14 < sizeof(output)) {
				strcat(output, line);
				size += strlen(line);
			}
			else {
				strcat(output, "OVERFLOW\r\n");	// 10 characters always reserved
				break;
			}
		}
	
		if (count == 0) {
			strcat(output, " none\r\n");	// space always reserved for this
		}
		else if (!PRF_FLAGGED(ch, PRF_SCREEN_READER) && (count % 2)) {
			strcat(output, "\r\n");	// space always reserved for this
		}
		
		if (count) {
			snprintf(line, sizeof(line), " (%d total)\r\n", count);
			if (size + strlen(line) < sizeof(output)) {
				strcat(output, line);
				size += strlen(line);
			}
		}
	
		if (ch->desc) {
			page_string(ch->desc, output, TRUE);
		}
	}	// end no-arg
	else if (GET_POS(ch) < POS_STANDING) {
		send_low_pos_msg(ch);	// must be standing to do the rest
	}
	else if (is_abbrev(argument, "dismiss")) {
		if (!(mob = find_minipet(ch))) {
			msg_to_char(ch, "You don't seem to have a minipet to dismiss.\r\n");
		}
		else {
			msg_to_char(ch, "You dismiss your minipet.\r\n");
			dismiss_any_minipet(ch);
		}
	}
	else {
		number = get_number(&argument);
		
		to_summon = NULL;	// to find
		HASH_ITER(hh, GET_MINIPETS(ch), mini, next_mini) {
			if (!(mob = mob_proto(mini->vnum))) {
				continue;	// no mob
			}
			else if (!multi_isname(argument, GET_PC_NAME(mob))) {
				continue;	// no match
			}
			else if (--number == 0) {
				to_summon = mob;
				break;	// FOUND!
			}
		}
		
		if (!to_summon) {
			msg_to_char(ch, "You don't have a minipet called '%s'.\r\n", argument);
		}
		else if ((mob = find_minipet(ch)) && GET_MOB_VNUM(mob) == GET_MOB_VNUM(to_summon)) {
			msg_to_char(ch, "You already have that minipet out.\r\n");
		}
		else {
			dismiss_any_minipet(ch);	// out with the old...
			
			mob = read_mobile(GET_MOB_VNUM(to_summon), TRUE);
			set_mob_flags(mob, DEFAULT_MINIPET_FLAGS);
			SET_BIT(AFF_FLAGS(mob), DEFAULT_MINIPET_AFFS);	// will this work?
			
			// try to scale mob to the summoner (most minipets have level caps of 1 tho)
			scale_mob_as_companion(mob, ch, 0);
			
			char_to_room(mob, IN_ROOM(ch));
			act("You whistle and $N appears!", FALSE, ch, NULL, mob, TO_CHAR);
			act("$n whistles and $N appears!", FALSE, ch, NULL, mob, TO_NOTVICT);
			
			add_follower(mob, ch, TRUE);
			load_mtrigger(mob);
			
			command_lag(ch, WAIT_OTHER);
		}
	}
}


ACMD(do_morph) {
	char buf[MAX_STRING_LENGTH], line[256];
	morph_data *morph, *next_morph;
	bool full = FALSE;
	double multiplier;
	obj_data *obj;
	bool normal, fast;
	size_t size, lsize;
	int count;
	char *tmp;
	
	skip_spaces(&argument);
	fast = (subcmd == SCMD_FASTMORPH || FIGHTING(ch) || GET_POS(ch) == POS_FIGHTING);
	normal = (!str_cmp(argument, "normal") || !str_cmp(argument, "norm"));
	
	// safety first: mobs must use %morph%
	if (IS_NPC(ch) && (!fast || !normal)) {
		msg_to_char(ch, "You can't morph.\r\n");
		return;
	}
	
	if (!*argument) {
		count = 1;	// counting 'normal'
		size = snprintf(buf, sizeof(buf), "You know the following morphs:\r\n %-*.*s%s", (!GET_MORPH(ch) ? 42 : 38), (!GET_MORPH(ch) ? 42 : 38), (!GET_MORPH(ch) ? "\tgnormal (current)\t0" : "normal"), PRF_FLAGGED(ch, PRF_SCREEN_READER) ? "\r\n" : "");
		
		HASH_ITER(hh, morph_table, morph, next_morph) {
			if (MORPH_FLAGGED(morph, MORPHF_IN_DEVELOPMENT | MORPHF_SCRIPT_ONLY)) {
				continue;
			}
			if (MORPH_ABILITY(morph) != NO_ABIL && !has_ability(ch, MORPH_ABILITY(morph))) {
				continue;
			}
			if (MORPH_REQUIRES_OBJ(morph) != NOTHING && !has_required_object(ch, MORPH_REQUIRES_OBJ(morph))) {
				continue;
			}
			
			// build info line
			++count;
			if (strstr(MORPH_SHORT_DESC(morph), "#n")) {
				tmp = str_replace("#n", PERS(ch, ch, TRUE), MORPH_SHORT_DESC(morph));
				lsize = snprintf(line, sizeof(line), "%s", tmp);
				free(tmp);	// allocated by str_replace
			}
			else { // no #n
				lsize = snprintf(line, sizeof(line), "%s", skip_filler(MORPH_SHORT_DESC(morph)));
			}
			
			// current?
			if (GET_MORPH(ch) == morph) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " (current)");
			}
			
			// append
			if (PRF_FLAGGED(ch, PRF_SCREEN_READER) || !(count % 2) || lsize > 38) {
				if (size + lsize + 3 < sizeof(buf)) {
					size += snprintf(buf + size, sizeof(buf) - size, " %s%s\t0\r\n", (GET_MORPH(ch) == morph ? "\tg" : ""), line);
				}
				else {
					full = TRUE;
				}
			}
			else {
				if (size + 39 < sizeof(buf)) {
					size += snprintf(buf + size, sizeof(buf) - size, " %s%-38.38s\t0", (GET_MORPH(ch) == morph ? "\tg" : ""), line);
				}
				else {
					full = TRUE;
				}
			}
		}
		
		if (full) {
			snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
		}
		else if (count % 2 && size + 2 < sizeof(buf) && !PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
			strcat(buf, "\r\n");
		}
		
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
		return;
	} // end no-argument
	
	// initialize
	morph = NULL;
	multiplier = fast ? 3.0 : 1.0;
	
	if (normal && !IS_MORPHED(ch)) {
		msg_to_char(ch, "You aren't morphed.\r\n");
	}
	else if (!IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're too busy to morph now!\r\n");
	}
	else if (!normal && !(morph = find_morph_by_name(ch, argument))) {
		msg_to_char(ch, "You don't know such a morph.\r\n");
	}
	else if (morph && MORPH_FLAGGED(morph, MORPHF_VAMPIRE_ONLY) && !check_vampire_sun(ch, TRUE)) {
		// sends own sun message
	}
	else if (morph && morph == GET_MORPH(ch)) {
		msg_to_char(ch, "You are already in that form.\r\n");
	}
	else if (!morph_affinity_ok(IN_ROOM(ch), morph)) {
		msg_to_char(ch, "You can't morph into that here.\r\n");
	}
	else if (morph && !can_use_ability(ch, MORPH_ABILITY(morph), MORPH_COST_TYPE(morph), MORPH_COST(morph) * multiplier, NOTHING)) {
		// sends own message
	}
	else if (morph && MORPH_COST_TYPE(morph) == BLOOD && MORPH_COST(morph) > 0 && !CAN_SPEND_BLOOD(ch)) {
		msg_to_char(ch, "Your blood is inert, you can't do that!\r\n");
	}
	else if (morph && MORPH_ABILITY(morph) != NO_ABIL && ABILITY_TRIGGERS(ch, NULL, NULL, NULL, MORPH_ABILITY(morph))) {
		return;
	}
	else if (morph && MORPH_FLAGGED(morph, MORPHF_VAMPIRE_ONLY) && !IS_VAMPIRE(ch)) {
		msg_to_char(ch, "You must be a vampire to do that.\r\n");
	}
	else if (morph && fast && MORPH_FLAGGED(morph, MORPHF_NO_FASTMORPH)) {
		msg_to_char(ch, "You cannot fastmorph into that form.\r\n");
	}
	else {
		// charge costs
		if (morph) {
			charge_ability_cost(ch, MORPH_COST_TYPE(morph), MORPH_COST(morph) * multiplier, NOTHING, 0, WAIT_ABILITY);
		}
		else {
			command_lag(ch, WAIT_ABILITY);
		}
		
		// required obj?
		if (morph && MORPH_REQUIRES_OBJ(morph) != NOTHING && (obj = has_required_object(ch, MORPH_REQUIRES_OBJ(morph)))) {
			if (!IS_IMMORTAL(ch) && OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {	// bind when used
				bind_obj_to_player(obj, ch);
				reduce_obj_binding(obj, ch);
			}
			
			if (MORPH_FLAGGED(morph, MORPHF_CONSUME_OBJ)) {	// take the obj
				extract_obj(obj);
			}
		}
		
		if (IS_NPC(ch) || fast) {
			// insta-morph!
			finish_morphing(ch, morph);
			command_lag(ch, WAIT_OTHER);
		}
		else {
			start_action(ch, ACT_MORPHING, config_get_int("morph_timer"));
			GET_ACTION_VNUM(ch, 0) = morph ? MORPH_VNUM(morph) : NOTHING;
			msg_to_char(ch, "You begin to transform!\r\n");
		}
	}
}


ACMD(do_mydescription) {
	skip_spaces(&argument);
	
	if (IS_NPC(ch) || !ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Your current description (type 'mydescription set' to change):\r\n%s", GET_LOOK_DESC(ch) ? GET_LOOK_DESC(ch) : " not set\r\n");
	}
	else if (!str_cmp(argument, "set")) {
		if (ch->desc->str) {
			msg_to_char(ch, "You are already editing text.\r\n");
		}
		else {
			start_string_editor(ch->desc, "your description", &(GET_LOOK_DESC(ch)), MAX_PLAYER_DESCRIPTION, TRUE);
		}
	}
	else {
		msg_to_char(ch, "Type 'mydescription set' to set your description.\r\n");
	}
}


ACMD(do_order) {
	char name[MAX_INPUT_LENGTH], message[MAX_INPUT_LENGTH];
	bool found = FALSE;
	room_data *org_room;
	char_data *vict = NULL;
	struct follow_type *k;

	half_chop(argument, name, message);

	if (!*name || !*message) {
		send_to_char("Order whom to do what?\r\n", ch);
	}
	else if (!vict && is_abbrev(name, "companion") && !(vict = GET_COMPANION(ch))) {
		msg_to_char(ch, "You don't have a companion to order.\r\n");
	}
	else if (!vict && !(vict = get_char_vis(ch, name, NULL, FIND_CHAR_ROOM)) && !is_abbrev(name, "followers")) {
		send_to_char("That person isn't here.\r\n", ch);
	}
	else if (ch == vict) {
		send_to_char("You can order yourself around all you want.\r\n", ch);
	}
	else if (vict && MOB_FLAGGED(vict, MOB_NO_COMMAND)) {
		act("You can't command $M.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (vict && IN_ROOM(ch) != IN_ROOM(vict)) {
		msg_to_char(ch, "You can't order someone who's not here.\r\n");
	}
	else {
		if (AFF_FLAGGED(ch, AFF_CHARM)) {
			send_to_char("Your superior would not approve of you giving orders.\r\n", ch);
			return;
		}
		if (vict) {
			sprintf(buf, "$N orders you to '%s'", message);
			act(buf, FALSE, vict, 0, ch, TO_CHAR);
			act("$n gives $N an order.", FALSE, ch, 0, vict, TO_ROOM);

			if ((GET_LEADER(vict) != ch) || !AFF_FLAGGED(vict, AFF_CHARM))
				act("$n has an indifferent look.", FALSE, vict, 0, 0, TO_ROOM);
			else {
				send_config_msg(ch, "ok_string");
				SET_BIT(AFF_FLAGS(vict), AFF_ORDERED);
				command_interpreter(vict, message);
				REMOVE_BIT(AFF_FLAGS(vict), AFF_ORDERED);
			}
		}
		else {			/* This is order "followers" */
			sprintf(buf, "$n issues the order '%s'.", message);
			act(buf, FALSE, ch, 0, vict, TO_ROOM);

			org_room = IN_ROOM(ch);

			for (k = ch->followers; k; k = k->next) {
				if (org_room == IN_ROOM(k->follower))
					if (AFF_FLAGGED(k->follower, AFF_CHARM) && !MOB_FLAGGED(k->follower, MOB_NO_COMMAND)) {
						found = TRUE;
						SET_BIT(AFF_FLAGS(k->follower), AFF_ORDERED);
						command_interpreter(k->follower, message);
						REMOVE_BIT(AFF_FLAGS(k->follower), AFF_ORDERED);
					}
			}
			if (found)
				send_config_msg(ch, "ok_string");
			else
				send_to_char("Nobody here is a loyal subject of yours!\r\n", ch);
		}
	}
}


// Either displays current prompt, or sets one; takes SCMD_PROMPT or SCMD_FPROMPT
ACMD(do_prompt) {	
	char *types[] = { "prompt", "fprompt" };
	char **prompt;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs get no %s.\r\n", types[subcmd]);
		return;
	}

	switch (subcmd) {
		case SCMD_PROMPT: {
			prompt = &GET_PROMPT(ch);
			break;
		}
		case SCMD_FPROMPT: {
			prompt = &GET_FIGHT_PROMPT(ch);
			break;
		}
		default: {
			msg_to_char(ch, "That command is not implemented.\r\n");
			return;
		}
	}

	if (!*argument) {
		msg_to_char(ch, "Your %s is currently: %s\r\n", types[subcmd], (*prompt ? show_color_codes(*prompt) : "n/a"));
		if (!SHOW_STATUS_MESSAGES(ch, SM_PROMPT)) {
			msg_to_char(ch, "(Use 'smessages prompt' to turn your prompt back on.)\r\n");
		}
		return;
	}

	delete_doubledollar(argument);
	
	if (strlen(argument) > MAX_PROMPT_SIZE) {
		argument[MAX_PROMPT_SIZE] = '\0';
	}

	if (!str_cmp(argument, "off") || !str_cmp(argument, "none")) {
		if (*prompt) {
			free(*prompt);
		}
		*prompt = str_dup("&0");	// empty prompt
		send_config_msg(ch, "ok_string");
	}
	else if (!str_cmp(argument, "default")) {
		if (*prompt) {
			free(*prompt);
		}
		*prompt = NULL;	// restores default prompt
		send_config_msg(ch, "ok_string");
		if (!SHOW_STATUS_MESSAGES(ch, SM_PROMPT)) {
			msg_to_char(ch, "(Use 'smessages prompt' to turn your prompt back on.)\r\n");
		}
	}
	else {
		if (*prompt) {
			free(*prompt);
		}
		*prompt = str_dup(argument);

		msg_to_char(ch, "Okay, set your %s to: %s\r\n", types[subcmd], show_color_codes(*prompt));
		if (!SHOW_STATUS_MESSAGES(ch, SM_PROMPT)) {
			msg_to_char(ch, "(Use 'smessages prompt' to turn your prompt back on.)\r\n");
		}
	}
}


ACMD(do_quit) {
	descriptor_data *d, *next_d;
	bool confirm = FALSE, died = FALSE;
	any_vnum vnum = NOTHING;

	if (IS_NPC(ch) || !ch->desc)
		return;
	
	one_argument(argument, arg);
	confirm = !str_cmp(arg, "confirm");

	if (subcmd != SCMD_QUIT)
		msg_to_char(ch, "You have to type quit--no less, to quit!\r\n");
	else if (GET_POS(ch) == POS_FIGHTING || FIGHTING(ch))
		msg_to_char(ch, "No way! You're fighting for your life!\r\n");
	else if (get_cooldown_time(ch, COOLDOWN_PVP_QUIT_TIMER) > 0 && !IS_IMMORTAL(ch))
		msg_to_char(ch, "You can't quit so soon after fighting!\r\n");
	else if (GET_FED_ON_BY(ch))
		msg_to_char(ch, "You can't quit with fangs in your neck!\r\n");
	else if (GET_FEEDING_FROM(ch))
		msg_to_char(ch, "You can't quit while drinking blood!\r\n");
	else if (IN_HOSTILE_TERRITORY(ch)) {
		msg_to_char(ch, "You can't quit in hostile territory.\r\n");
	}
	else if (GET_OLC_TYPE(ch->desc) != 0 && !confirm) {
		msg_to_char(ch, "You can't quit with an OLC editor open (use .save or .abort first, or 'quit confirm').\r\n");
	}
	else if (ch->desc->str && !confirm) {
		msg_to_char(ch, "You can't quit with a text editor open (use /save or /abort first, or 'quit confirm').\r\n");
	}
	else if (GET_POS(ch) < POS_STUNNED && !confirm) {
		msg_to_char(ch, "Quitting now will kill your character. Type 'quit confirm' to proceed.\r\n");
	}
	else {
		if (GET_POS(ch) < POS_STUNNED) {
			msg_to_char(ch, "You shuffle off this mortal coil, and die...\r\n");
			act("$n shuffles off $s mortal coil and dies.", FALSE, ch, NULL, NULL, TO_ROOM);
			player_death(ch);
			died = TRUE;
		}
		else if (GET_COMPANION(ch)) {
			// preserve companion
			vnum = GET_MOB_VNUM(GET_COMPANION(ch));
		}
		
		if (!GET_INVIS_LEV(ch)) {
			act("$n has left the game.", TRUE, ch, 0, 0, TO_ROOM);
		}
		syslog(SYS_LOGIN, GET_INVIS_LEV(ch), TRUE, "%s has quit the game at %s", GET_NAME(ch), IN_ROOM(ch) ? room_log_identifier(IN_ROOM(ch)) : "an unknown location");
		if (GET_INVIS_LEV(ch) == 0) {
			if (config_get_bool("public_logins")) {
				mortlog("%s has left the game", PERS(ch, ch, 1));
			}
			else {
				mortlog_friends(ch, "%s has left the game", PERS(ch, ch, 1));
				if (GET_LOYALTY(ch)) {
					log_to_empire(GET_LOYALTY(ch), ELOG_LOGINS, "%s has left the game", PERS(ch, ch, TRUE));
				}
			}
		}
		send_to_char("Goodbye, friend... Come back soon!\r\n", ch);
		dismiss_any_minipet(ch);
		despawn_companion(ch, NOTHING);

		/*
		 * kill off all sockets connected to the same player as the one who is
		 * trying to quit.  Helps to maintain sanity as well as prevent duping.
		 */
		for (d = descriptor_list; d; d = next_d) {
			next_d = d->next;
			if (d != ch->desc && d->character && (GET_IDNUM(d->character) == GET_IDNUM(ch))) {
				STATE(d) = CON_DISCONNECT;
			}
		}
		
		GET_LAST_KNOWN_LEVEL(ch) = GET_COMPUTED_LEVEL(ch);
		GET_LAST_COMPANION(ch) = vnum;
		save_char(ch, died ? NULL : IN_ROOM(ch));
		
		display_statistics_to_char(ch);
		
		pause_affect_total = TRUE;	// prevent unneeded affect_totals
		extract_all_items(ch);
		extract_char(ch);	// this will disconnect the descriptor
		pause_affect_total = FALSE;
		
		// ensure quit characters are gone right away
		extract_pending_chars();
	}
}


ACMD(do_save) {
	if (!IS_NPC(ch) && ch->desc) {
		if (cmd) {
			msg_to_char(ch, "Saving %s.\r\n", GET_NAME(ch));
		}
		
		GET_LAST_KNOWN_LEVEL(ch) = GET_COMPUTED_LEVEL(ch);
		SAVE_CHAR(ch);
	}
}


ACMD(do_selfdelete) {
	char passwd[MAX_INPUT_LENGTH];
	
	argument = any_one_arg(argument, passwd);
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (PLR_FLAGGED(ch, PLR_NODELETE)) {
		msg_to_char(ch, "You cannot self-delete because of your !DEL player flag.\r\n");
	}
	else if (GET_ACCESS_LEVEL(ch) == LVL_TOP) {
		msg_to_char(ch, "Top-level players may not self-delete.\r\n");
	}
	else if (!*passwd || !*argument) {
		msg_to_char(ch, "WARNING: This will delete your character.\r\n");
		msg_to_char(ch, "Usage: selfdelete <password> CONFIRM\r\n");
	}
	else if (strncmp(CRYPT(passwd, PASSWORD_SALT), GET_PASSWD(ch), MAX_PWD_LENGTH)) {
		msg_to_char(ch, "Invalid password.\r\n");
		msg_to_char(ch, "Usage: selfdelete <password> CONFIRM\r\n");
	}
	else if (strcmp(argument, "CONFIRM")) {
		msg_to_char(ch, "You must type the word CONFIRM, in all caps, after your password.\r\n");
		msg_to_char(ch, "WARNING: This will delete your character.\r\n");
	}
	else {
		
		// logs and messaging
		syslog(SYS_LOGIN, GET_INVIS_LEV(ch), TRUE, "DEL: %s (lev %d/%d) has self-deleted [%s]", GET_NAME(ch), GET_COMPUTED_LEVEL(ch), GET_ACCESS_LEVEL(ch), ch->desc ? ch->desc->host : "no host");
		if (!GET_INVIS_LEV(ch)) {
			act("$n has left the game.", TRUE, ch, 0, 0, TO_ROOM);
		}
		if (GET_INVIS_LEV(ch) == 0) {
			if (config_get_bool("public_logins")) {
				mortlog("%s has left the game", PERS(ch, ch, 1));
			}
			else {
				mortlog_friends(ch, "%s has left the game", PERS(ch, ch, 1));
				if (GET_LOYALTY(ch)) {
					log_to_empire(GET_LOYALTY(ch), ELOG_LOGINS, "%s has left the game", PERS(ch, ch, TRUE));
				}
			}
		}
		msg_to_char(ch, "You have deleted your character. Goodbye...\r\n");
		
		// actual delete (remove equipment first)
		extract_all_items(ch);
		delete_player_character(ch);
		extract_char(ch);
	}
}


ACMD(do_shear) {
	char_data *mob;
	bool any;

	one_argument(argument, arg);

	if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!has_player_tech(ch, PTECH_SHEAR_COMMAND)) {
		msg_to_char(ch, "You don't have the correct ability to shear animals.\r\n");
	}
	else if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_STABLE)) {
		msg_to_char(ch, "You need to be in a stable to shear anything.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This stable must be in a city for you to shear anything.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "Complete the building first.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're a bit busy right now.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Which animal would you like to shear?\r\n");
	}
	else if (!(mob = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (!IS_NPC(mob) || !has_interaction(mob->interactions, INTERACT_SHEAR)) {
		act("You can't shear $N!", FALSE, ch, NULL, mob, TO_CHAR);
	}
	else if (get_cooldown_time(mob, COOLDOWN_SHEAR) > 0) {
		act("$E is already shorn.", FALSE, ch, NULL, mob, TO_CHAR);
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_SHEAR_COMMAND, mob, NULL, NULL)) {
		// triggered
	}
	else {
		check_scaling(mob, ch);	// ensure mob is scaled -- this matters for global interactions
		
		any = run_interactions(ch, mob->interactions, INTERACT_SHEAR, IN_ROOM(ch), mob, NULL, NULL, shear_interact);
		any |= run_global_mob_interactions(ch, mob, INTERACT_SHEAR, shear_interact);
		
		if (any) {
			gain_player_tech_exp(ch, PTECH_SHEAR_COMMAND, 33);
			gain_player_tech_exp(ch, PTECH_SHEAR_UPGRADE, 33);
			run_ability_hooks_by_player_tech(ch, PTECH_SHEAR_COMMAND, mob, NULL, NULL, NULL);
		}
		else {
			act("You can't shear $N!", FALSE, ch, NULL, mob, TO_CHAR);
		}
	}
}


ACMD(do_skin) {
	obj_data *obj;
	char_data *proto, *vict;
	char *argptr;
	int number;

	one_argument(argument, arg);
	argptr = arg;
	number = get_number(&argptr);

	if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!*argptr)
		msg_to_char(ch, "What would you like to skin?\r\n");
	else if (!(obj = get_obj_in_list_vis_prefer_type(ch, argptr, &number, ch->carrying, ITEM_CORPSE)) && !(obj = get_obj_in_list_vis_prefer_type(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch)), ITEM_CORPSE))) {
		// no object found: try mobs in the room
		if ((vict = get_char_room_vis(ch, arg, &number))) {
			act("You need to kill $M first.", FALSE, ch, NULL, vict, TO_CHAR);
		}
		else {
			msg_to_char(ch, "You don't seem to have anything like that.\r\n");
		}
	}
	else if (!IS_CORPSE(obj))
		msg_to_char(ch, "You can only skin corpses.\r\n");
	else if (GET_CORPSE_NPC_VNUM(obj) == NOTHING || !(proto = mob_proto(GET_CORPSE_NPC_VNUM(obj))) || !has_interaction(proto->interactions, INTERACT_SKIN)) {
		msg_to_char(ch, "You can't skin that.\r\n");
	}
	else if (!bind_ok(obj, ch)) {
		msg_to_char(ch, "You can't skin a corpse that is bound to someone else.\r\n");
	}
	else if (IS_SET(GET_CORPSE_FLAGS(obj), CORPSE_EATEN))
		msg_to_char(ch, "It's too badly mangled to get any amount of usable skin.\r\n");
	else if (IS_SET(GET_CORPSE_FLAGS(obj), CORPSE_SKINNED))
		msg_to_char(ch, "It's already been skinned.\r\n");
	else if (!has_tool(ch, TOOL_KNIFE))
		msg_to_char(ch, "You need to be using a good knife to skin a corpse.\r\n");
	else {
		// run it
		if (IS_SET(GET_CORPSE_FLAGS(obj), CORPSE_NO_LOOT) || !run_interactions(ch, proto->interactions, INTERACT_SKIN, IN_ROOM(ch), NULL, obj, NULL, skin_interact)) {
			act("You try to skin $p but get nothing useful.", FALSE, ch, obj, NULL, TO_CHAR);
		}
		else {
			gain_player_tech_exp(ch, PTECH_SKINNING_UPGRADE, 15);
			run_ability_hooks_by_player_tech(ch, PTECH_SKINNING_UPGRADE, NULL, obj, NULL, NULL);
		}
		
		set_obj_val(obj, VAL_CORPSE_FLAGS, GET_CORPSE_FLAGS(obj) | CORPSE_SKINNED);
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_title) {
	skip_spaces(&argument);
	delete_doubledollar(argument);

	if (IS_NPC(ch))
		send_to_char("Your title is fine... go away.\r\n", ch);
	else if (!IS_APPROVED(ch) && config_get_bool("title_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (ACCOUNT_FLAGGED(ch, ACCT_NOTITLE)) {
		send_to_char("You can't title yourself -- you shouldn't have abused it!\r\n", ch);
	}
	else if (strstr(argument, "(") || strstr(argument, ")") || strstr(argument, "%"))
		send_to_char("Titles can't contain the (, ), or % characters.\r\n", ch);
	else if (strlen(argument) > MAX_TITLE_LENGTH-1 || color_strlen(argument) > MAX_TITLE_LENGTH_NO_COLOR) {
		// the -1 reserves an extra spot for an extra space
		msg_to_char(ch, "Sorry, titles can't be longer than %d characters.\r\n", MAX_TITLE_LENGTH_NO_COLOR);
	}
	else {
		set_title(ch, argument);
		msg_to_char(ch, "Okay, you're now %s%s.\r\n", PERS(ch, ch, 1), NULLSAFE(GET_TITLE(ch)));
	}
}


ACMD(do_toggle) {
	const char *togcols[NUM_TOG_TYPES][2] = { { "\tr", "\tg" }, { "\tg", "\tr" } };
	const char *tognames[NUM_TOG_TYPES][2] = { { "off", "on" }, { "on", "off" } };
	const char *imm_color = "\tc";
	const char *clear_color = "\t0";

	int iter, type = NOTHING, count, on, pos;
	char arg[MAX_INPUT_LENGTH];
	struct alpha_tog *altog;
	bool imm;
	bool screenreader = PRF_FLAGGED(ch, PRF_SCREEN_READER);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs do not have toggles.\r\n");
		return;
	}
	
	argument = any_one_arg(argument, arg);
	skip_spaces(&argument);
	
	for (iter = 0; *toggle_data[iter].name != '\n' && type == NOTHING; ++iter) {
		if (toggle_data[iter].level <= GET_ACCESS_LEVEL(ch) && is_abbrev(arg, toggle_data[iter].name)) {
			type = iter;
		}
	}
	
	if (!*arg) {
		msg_to_char(ch, "Toggles:\r\n");
		alphabetize_toggles();	// in case
		
		iter = count = 0;
		LL_FOREACH(alpha_tog_list, altog) {
			pos = screenreader ? altog->pos : iter++;	// shows in alpha order for SR; normal order for rest
			if (*toggle_data[pos].name == '\n') {
				break;	// this should not be possible, but just in case
			}
			
			if (toggle_data[pos].level <= GET_ACCESS_LEVEL(ch)) {
				on = (PRF_FLAGGED(ch, toggle_data[pos].bit) ? 1 : 0);
				imm = (toggle_data[pos].level >= LVL_START_IMM);
				if (screenreader) {
					msg_to_char(ch, "%s: %s%s\r\n", toggle_data[pos].name, tognames[toggle_data[pos].type][on], imm ? " (immortal)" : "");
				}
				else {
					msg_to_char(ch, " %s[%s%3.3s%s] %-15.15s%s%s", imm ? imm_color : "", togcols[toggle_data[pos].type][on], tognames[toggle_data[pos].type][on], imm ? imm_color : clear_color, toggle_data[pos].name, clear_color, (!(++count % 3) ? "\r\n" : ""));
				}
			}
		}
		
		if (count % 3 && !screenreader) {
			send_to_char("\r\n", ch);
		}
	}
	else if (type == NOTHING) {
		msg_to_char(ch, "Unknown toggle '%s'.\r\n", arg);
	}
	else {
		// check for optional on/off arg
		if (!str_cmp(argument, "on")) {
			if ((toggle_data[type].type == TOG_ONOFF && IS_SET(PRF_FLAGS(ch), toggle_data[type].bit)) || (toggle_data[type].type == TOG_OFFON && !IS_SET(PRF_FLAGS(ch), toggle_data[type].bit))) {
				msg_to_char(ch, "That toggle is already on.\r\n");
				return;
			}
			
			if (toggle_data[type].type == TOG_ONOFF) {
				SET_BIT(PRF_FLAGS(ch), toggle_data[type].bit);
			}
			else {
				REMOVE_BIT(PRF_FLAGS(ch), toggle_data[type].bit);
			}
		}
		else if (!str_cmp(argument, "off")) {
			if ((toggle_data[type].type == TOG_ONOFF && !IS_SET(PRF_FLAGS(ch), toggle_data[type].bit)) || (toggle_data[type].type == TOG_OFFON && IS_SET(PRF_FLAGS(ch), toggle_data[type].bit))) {
				msg_to_char(ch, "That toggle is already off.\r\n");
				return;
			}
			
			if (toggle_data[type].type == TOG_ONOFF) {
				REMOVE_BIT(PRF_FLAGS(ch), toggle_data[type].bit);
			}
			else {
				SET_BIT(PRF_FLAGS(ch), toggle_data[type].bit);
			}
		}
		else {	// neither on nor off specified
			on = PRF_TOG_CHK(ch, toggle_data[type].bit);
		}
		
		on = PRF_FLAGGED(ch, toggle_data[type].bit) ? 1 : 0;
		
		msg_to_char(ch, "You toggle %s %s%s&0.\r\n", toggle_data[type].name, togcols[toggle_data[type].type][on], tognames[toggle_data[type].type][on]);
		
		// callback can notify or make additional changes
		if (toggle_data[type].callback_func != NULL) {
			(toggle_data[type].callback_func)(ch);
		}
	}
}


ACMD(do_visible) {
	if (GET_ACCESS_LEVEL(ch) >= LVL_GOD) {
		perform_immort_vis(ch);
		return;
	}

	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	else {
		send_to_char("You are already visible.\r\n", ch);
	}
}
