/* ************************************************************************
*   File: act.other.c                                     EmpireMUD 2.0b1 *
*  Usage: Miscellaneous player-level commands                             *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __ACT_OTHER_C__

#include <math.h>

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
*   Helpers
*   Toggle Notifiers
*   Commands
*/

// external prototypes
extern char_data *has_familiar(char_data *ch);
void Objsave_char(char_data *ch, int rent_code);
void scale_item_to_level(obj_data *obj, int level);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* This quits out an old character and swaps the descriptor over to the new
* character. Caution: The new character may already be in-game, if it was
* linkdead.
*
* @param char_data *old The character who's currently playing.
* @param char_data *new The character to switch to (may or may not be playing).
*/
void perform_alternate(char_data *old, char_data *new) {
	void display_tip_to_char(char_data *ch);
	extern int enter_player_game(descriptor_data *d, int dolog, bool fresh);
	extern int has_mail(long recipient);
	void start_new_character(char_data *ch);
	extern char *START_MESSG;
	extern bool global_mute_slash_channel_joins;
	
	char sys[MAX_STRING_LENGTH], mort[MAX_STRING_LENGTH], temp[256];
	descriptor_data *desc, *next_d;
	bool show_start = FALSE;
	char_data *ch_iter;
	int invis_lev, last_tell;
	
	if (!old || !new || !old->desc || new->desc) {
		log("SYSERR: Attempting invalid peform_alternate with %s, %s, %s, %s", old ? "ok" : "no old", new ? "ok" : "no new", old->desc ? "ok" : "no old desc", new->desc ? "new desc" : "ok");
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
	
	// prepare logs
	snprintf(sys, sizeof(sys), "%s used alternate to switch to %s.", GET_NAME(old), GET_NAME(new));
	strcpy(temp, PERS(new, new, TRUE));
	snprintf(mort, sizeof(mort), "%s has switched to %s", PERS(old, old, TRUE), temp);
	
	// store last known level now
	GET_LAST_KNOWN_LEVEL(old) = GET_COMPUTED_LEVEL(old);
	
	// peace out
	if (!GET_INVIS_LEV(old)) {
		act("$n has left the game.", TRUE, old, NULL, NULL, TO_ROOM);
	}
	
	// save old char...
	Objsave_char(old, RENT_RENTED);
	SAVE_CHAR(old);
	
	// switch over replies
	last_tell = GET_LAST_TELL(old);
	if (invis_lev <= LVL_APPROVED) {
		for (ch_iter = character_list; ch_iter; ch_iter = ch_iter->next) {
			if (!IS_NPC(ch_iter) && GET_LAST_TELL(ch_iter) == GET_IDNUM(old)) {
				GET_LAST_TELL(ch_iter) = GET_IDNUM(new);
			}
		}
	}
	
	// move desc (do this AFTER saving)
	new->desc = old->desc;
	new->desc->character = new;
	old->desc = NULL;
	
	// remove old character
	extract_char(old);
	
	syslog(SYS_LOGIN, invis_lev, TRUE, "%s", sys);
	if (GET_INVIS_LEV(new) == 0 && !PLR_FLAGGED(new, PLR_INVSTART)) {
		mortlog("%s", mort);
	}
	
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
	
	if (AFF_FLAGGED(new, AFF_EARTHMELD)) {
		msg_to_char(new, "You are earthmelded.\r\n");
	}
	else {
		look_at_room(new);
	}
	
	msg_to_char(new, "\r\n");	// leading \r\n between the look and the tip
	display_tip_to_char(new);
	
	if (has_mail(GET_IDNUM(new))) {
		send_to_char("&rYou have mail waiting.&0\r\n", new);
	}
	
	if (show_start) {
		send_to_char(START_MESSG, new);
	}
	
	add_cooldown(new, COOLDOWN_ALTERNATE, SECS_PER_REAL_MIN);
	GET_LAST_TELL(new) = last_tell;
}


/**
* Shows a player's own group to him.
*
* @param char_data *ch The person to display to.
*/
static void print_group(char_data *ch) {
	extern char *get_room_name(room_data *room, bool color);
	extern const char *class_role[NUM_ROLES];
	extern const char *pool_abbrevs[];

	char status[256], class[256], loc[256], alerts[256];
	struct group_member_data *mem;
	int iter, ssize;
	char_data *k;

	if (GROUP(ch)) {
		msg_to_char(ch, "Your group consists of:\r\n");
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
			if (!IS_NPC(k) && GET_CLASS(k) != CLASS_NONE) {
				snprintf(class, sizeof(class), "/%s/%s", class_data[GET_CLASS(k)].name, class_role[(int) GET_CLASS_ROLE(k)]);
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
				snprintf(loc, sizeof(loc), " - %s (%d, %d)", get_room_name(IN_ROOM(k), FALSE), X_COORD(IN_ROOM(k)), Y_COORD(IN_ROOM(k)));
			}
			else {
				*loc = '\0';
			}
			
			// name: lvl class (spec) -- location (x, y)
			msg_to_char(ch, "%s%s [%d%s]%s%s%s\r\n", PERS(k, k, TRUE), (k == GROUP_LEADER(GROUP(ch))) ? " (L)" : "", get_approximate_level(k), class, status, alerts, loc);
		}
	}
	else {
		msg_to_char(ch, "You are not in a group.\r\n");
	}
}


INTERACTION_FUNC(shear_interact) {
	char buf[MAX_STRING_LENGTH];
	int iter, amt;
	obj_data *obj;
	
	add_cooldown(inter_mob, COOLDOWN_SHEAR, config_get_int("shear_growth_time") * SECS_PER_REAL_HOUR);
	WAIT_STATE(ch, 2 RL_SEC);
			
	amt = interaction->quantity;
	if (HAS_ABILITY(ch, ABIL_MASTER_FARMER)) {
		amt *= 2;
	}
	
	for (iter = 0; iter < amt; ++iter) {
		obj = read_object(interaction->vnum);
		obj_to_char_or_room(obj, ch);
		load_otrigger(obj);
	}
	
	// only show loot to the skinner
	if (amt == 1) {
		act("You skillfully shear $N and get $p.", FALSE, ch, obj, inter_mob, TO_CHAR);
		act("$n skillfully shears you and gets $p.", FALSE, ch, obj, inter_mob, TO_VICT);
		act("$n skillfully shears $N and gets $p.", FALSE, ch, obj, inter_mob, TO_NOTVICT);
	}
	else {
		sprintf(buf, "You skillfully shear $N and get $p (x%d).", amt);
		act(buf, FALSE, ch, obj, inter_mob, TO_CHAR);
		sprintf(buf, "$n skillfully shears you and gets $p (x%d).", amt);
		act(buf, FALSE, ch, obj, inter_mob, TO_VICT);
		sprintf(buf, "$n skillfully shears $N and gets $p (x%d).", amt);
		act(buf, FALSE, ch, obj, inter_mob, TO_NOTVICT);
	}
	
	return TRUE;
}


INTERACTION_FUNC(skin_interact) {
	char buf[MAX_STRING_LENGTH];
	obj_data *obj;
	int num;

	SET_BIT(GET_OBJ_VAL(inter_item, VAL_CORPSE_FLAGS), CORPSE_SKINNED);
	WAIT_STATE(ch, 2 RL_SEC);
		
	for (num = 0; num < interaction->quantity; ++num) {
		obj = read_object(interaction->vnum);
		if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			scale_item_to_level(obj, 1);	// min scale
		}
		obj_to_char_or_room(obj, ch);
		load_otrigger(obj);
	}
	
	// only show loot to the skinner
	if (interaction->quantity > 1) {
		sprintf(buf, "You carefully skin $P and get $p (x%d).", interaction->quantity);
		act(buf, FALSE, ch, obj, inter_item, TO_CHAR);
		sprintf(buf, "$n carefully skins $P and gets $p (x%d).", interaction->quantity);
		act(buf, FALSE, ch, obj, inter_item, TO_ROOM);
	}
	else {
		act("You carefully skin $P and get $p.", FALSE, ch, obj, inter_item, TO_CHAR);
		act("$n carefully skins $P and gets $p.", FALSE, ch, obj, inter_item, TO_ROOM);
	}
	
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// TOGGLE NOTIFIERS ////////////////////////////////////////////////////////

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


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_alternate) {
	extern int isbanned(char *hostname);

	char arg[MAX_INPUT_LENGTH];
	struct char_file_u tmp_store;
	char_data *newch;
	int player_i, idnum;
	
	any_one_arg(argument, arg);
	
	if (!ch->desc) {
		msg_to_char(ch, "You can't do anything without a player controlling you. Who's even reading this?\r\n");
	}
	else if (IS_NPC(ch) || ch->desc->original) {
		msg_to_char(ch, "You can't do that while switched!\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "This command lets you switch which character you're logged in with.\r\n");
		msg_to_char(ch, "Usage: alternate <character name>\r\n");
	}
	else if (!str_cmp(arg, "list")) {
		struct char_file_u vbuf;
		int iter;
	
		msg_to_char(ch, "Account characters:\r\n");
		
		for (iter = 0; iter <= top_of_p_table; iter++) {
			load_char(player_table[iter].name, &vbuf);
			if (IS_SET(vbuf.char_specials_saved.act, PLR_DELETED)) {
				continue;
			}
			
			if (vbuf.char_specials_saved.idnum == GET_IDNUM(ch)) {
				// self
				msg_to_char(ch, " &c%s&0\r\n", vbuf.name);
			}
			else if (vbuf.player_specials_saved.account_id != 0 && vbuf.player_specials_saved.account_id == GET_ACCOUNT_ID(ch)) {
				msg_to_char(ch, " %s\r\n", vbuf.name);
			}
		}
		
		// prevent rapid-use
		WAIT_STATE(ch, 1.5 RL_SEC);
	}
	else if (ROOM_OWNER(IN_ROOM(ch)) && empire_is_hostile(ROOM_OWNER(IN_ROOM(ch)), GET_LOYALTY(ch), IN_ROOM(ch))) {
		msg_to_char(ch, "You can't alternate in hostile territory.\r\n");
	}
	else if (get_cooldown_time(ch, COOLDOWN_ALTERNATE) > 0) {
		msg_to_char(ch, "You can't alternate again so soon.\r\n");
	}
	else if (get_cooldown_time(ch, COOLDOWN_PVP_QUIT_TIMER) > 0 && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You can't alternate so soon after fighting!\r\n");
	}
	else if (GET_POS(ch) == POS_FIGHTING || FIGHTING(ch)) {
		msg_to_char(ch, "You can't switch characters while fighting!\r\n");
	}
	else if ((idnum = get_id_by_name(arg)) == NOTHING) {
		msg_to_char(ch, "Unknown character.\r\n");
	}
	else if ((newch = is_playing(idnum)) || (newch = is_at_menu(idnum))) {
		// in-game?
		
		if (GET_ACCOUNT_ID(newch) != GET_ACCOUNT_ID(ch) || GET_ACCOUNT_ID(newch) == 0) {
			msg_to_char(ch, "That character isn't on your account.\r\n");
			return;
		}
		if (newch->desc || !IN_ROOM(newch)) {
			msg_to_char(ch, "You can't switch to that character because someone is already playing it.\r\n");
			return;
		}
		
		// seems ok
		perform_alternate(ch, newch);
	}
	else {
		// prepare
		CREATE(newch, char_data, 1);
		clear_char(newch);
		CREATE(newch->player_specials, struct player_special_data, 1);

		// load
		if ((player_i = load_char(arg, &tmp_store)) >= 0) {
			store_to_char(&tmp_store, newch);
			GET_PFILEPOS(newch) = player_i;

			if (PLR_FLAGGED(newch, PLR_DELETED)) {
				msg_to_char(ch, "Unknown character.\r\n");
				free_char(newch);
				return;
			}
			else {
				// undo it just in case they are set
				REMOVE_BIT(PLR_FLAGS(newch), PLR_WRITING | PLR_MAILING);
			}
		}
		else {
			msg_to_char(ch, "Unknown character.\r\n");
			free_char(newch);
			return;
		}
		
		// ensure legal switch
		if (GET_ACCOUNT_ID(newch) != GET_ACCOUNT_ID(ch) || GET_ACCOUNT_ID(newch) == 0) {
			msg_to_char(ch, "That character isn't on your account.\r\n");
			free_char(newch);
			return;
		}
		// select ban?
		if (isbanned(ch->desc->host) == BAN_SELECT && !PLR_FLAGGED(newch, PLR_SITEOK)) {
			msg_to_char(ch, "Sorry, that character has not been cleared for login from your site!\r\n");
			free_char(newch);
			return;
		}
		
		// seems ok!
		perform_alternate(ch, newch);
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
		strncpy(GET_PASSWD(ch), CRYPT(new1, PASSWORD_SALT), MAX_PWD_LENGTH);
		*(GET_PASSWD(ch) + MAX_PWD_LENGTH) = '\0';
		SAVE_CHAR(ch);
		
		syslog(SYS_INFO, GET_INVIS_LEV(ch), TRUE, "%s has changed %s password using changepass", GET_NAME(ch), HSHR(ch));
		if (ch->desc && ch->desc->snoop_by) {
			syslog(SYS_INFO, MIN(LVL_TOP, MAX(GET_INVIS_LEV(ch), GET_ACCESS_LEVEL(ch) + 1)), TRUE, "WARNING: %s changed password while being snooped", GET_NAME(ch));
		}
		msg_to_char(ch, "You have successfully changed your password.\r\n");
	}
}


ACMD(do_confirm) {
	bool check_reboot_confirms();
	void perform_reboot();
	extern struct reboot_control_data reboot_control;
	extern const char *reboot_type[];
	
	if (IS_NPC(ch)) {
		return;
	}

	if (GET_POS(ch) == POS_FIGHTING) {
		msg_to_char(ch, "How can you be ready?  You're fighting for your life!\r\n");
		return;
	}

	if (reboot_control.time == -1) {
		msg_to_char(ch, "No reboot has been set!\r\n");
		return;
	}

	if (REBOOT_CONF(ch)) {
		msg_to_char(ch, "You've already confirmed that you're ready for the %s.\r\n", reboot_type[reboot_control.type]);
	}
	else {
		REBOOT_CONF(ch) = TRUE;
		msg_to_char(ch, "You have confirmed that you're ready for the %s.\r\n", reboot_type[reboot_control.type]);
	}

	if (check_reboot_confirms() && reboot_control.time <= 15) {
		reboot_control.immediate = TRUE;
	}
}


ACMD(do_customize) {
	void do_customize_room(char_data *ch, char *argument);

	char arg2[MAX_INPUT_LENGTH];
	
	half_chop(argument, arg, arg2);
	
	if (!*arg) {
		msg_to_char(ch, "What do you want to customize? (See HELP CUSTOMIZE)\r\n");
	}
	else if (is_abbrev(arg, "building") || is_abbrev(arg, "room")) {
		do_customize_room(ch, arg2);
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
	else if (!strn_cmp(argument, "famil", 5) && is_abbrev(argument, "familiar")) {
		// requires abbrev of at least "famil"
		if (!(vict = has_familiar(ch))) {
			msg_to_char(ch, "You do not have a familiar to dismiss.\r\n");
		}
		else {
			if (IN_ROOM(ch) != IN_ROOM(vict)) {
				msg_to_char(ch, "You dismiss %s.\r\n", PERS(vict, vict, FALSE));
			}
			act("$n is dismissed and vanishes!", TRUE, vict, NULL, NULL, TO_ROOM);
			extract_char(vict);
		}
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (!IS_NPC(vict) || vict->master != ch || !MOB_FLAGGED(vict, MOB_FAMILIAR)) {
		msg_to_char(ch, "You can only dismiss a familiar.\r\n");
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
	obj_data *obj;
	byte amount;
	room_data *room = HOME_ROOM(IN_ROOM(ch));
	
	int fire_extinguish_value = config_get_int("fire_extinguish_value");

	// this loop finds a drink container and sets obj
	for (obj = ch->carrying; obj && !(GET_DRINK_CONTAINER_CONTENTS(obj) > 0); obj = obj->next_content);

	if (!IN_ROOM(ch))
		msg_to_char(ch, "Unexpected error in douse.\r\n");
	else if (!IS_ANY_BUILDING(IN_ROOM(ch)))
		msg_to_char(ch, "You can't really douse a fire here.\r\n");
	else if (!BUILDING_BURNING(room))
		msg_to_char(ch, "There's no fire here!\r\n");
	else if (!obj)
		msg_to_char(ch, "What do you plan to douse the fire with?\r\n");
	else if (GET_DRINK_CONTAINER_TYPE(obj) != LIQ_WATER)
		msg_to_char(ch, "Only water will douse a fire!\r\n");
	else {
		amount = GET_DRINK_CONTAINER_CONTENTS(obj);
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = 0;

		COMPLEX_DATA(room)->burning = MIN(fire_extinguish_value, BUILDING_BURNING(room) + amount);
		act("You throw some water from $p onto the flames!", FALSE, ch, obj, 0, TO_CHAR);
		act("$n throws some water from $p onto the flames!", FALSE, ch, obj, 0, TO_CHAR);

		if (BUILDING_BURNING(room) >= fire_extinguish_value) {
			act("The flames have been extinguished!", FALSE, ch, 0, 0, TO_CHAR | TO_ROOM);
			COMPLEX_DATA(room)->burning = 0;
		}
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
	syslog(SYS_INFO, GET_INVIS_LEV(ch), FALSE, "%s %s: %s", GET_NAME(ch), name, argument);

	if (stat(filename, &fbuf) < 0) {
		perror("SYSERR: Can't stat() file");
		return;
	}
	if (fbuf.st_size >= config_get_int("max_filesize")) {
		send_to_char("Sorry, the file is full right now.. try again later.\r\n", ch);
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
	else if (ROOM_CROP_TYPE(IN_ROOM(ch)) && (cp = crop_proto(ROOM_CROP_TYPE(IN_ROOM(ch))))) {
		snprintf(locpart, sizeof(locpart), " [CRP%d]", GET_CROP_VNUM(cp));
	}
	else {
		snprintf(locpart, sizeof(locpart), " [%d]", GET_ROOM_VNUM(IN_ROOM(ch)));
	}
	
	fprintf(fl, "%-8s (%6.6s)%s %s\n", GET_NAME(ch), (tmp + 4), locpart, argument);
	fclose(fl);
	send_to_char("Okay. Thanks!\r\n", ch);
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
		msg_to_char(ch, "Available group options: invite, join, kick, leave, leader\r\n");
		
		if (GROUP(ch)) {
			// should we replace this with the group summary? -pc
			print_group(ch);
		}
		else if ((vict = is_playing(GET_GROUP_INVITE(ch)))) {
			msg_to_char(ch, "You have been invited to a group by %s.\r\n", PERS(vict, vict, TRUE));
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
		if (!(vict = get_player_vis(ch, argument, FIND_CHAR_WORLD | FIND_NO_DARK))) {
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
		else if (GROUP(vict)) {
			msg_to_char(ch, "Your target is already in a group.\r\n");
			return;
		}
		else if (GROUP(ch) && GROUP_LEADER(GROUP(ch)) != ch) {
			msg_to_char(ch, "Only the group's leader can invite members.\r\n");
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

		msg_to_char(ch, "You have invited %s to the group.\r\n", GET_NAME(vict));
		
		if (!IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_GSAY)) {
			msg_to_char(vict, "&%c[group] %s has invited you to join a group.&0\r\n", GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_GSAY), GET_NAME(ch));
		}
		else {
			msg_to_char(vict, "[&Ggroup&0] %s has invited you to join a group.\r\n", GET_NAME(ch));
		}
		
		GET_GROUP_INVITE(vict) = GET_IDNUM(ch);
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
		GROUP(ch)->leader = vict;
		send_to_group(NULL, GROUP(ch), "%s is now group leader.", GET_NAME(vict));
		send_config_msg(ch, "ok_string");
	}
	else if (is_abbrev(buf, "option")) {
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
		msg_to_char(ch, "You must specify a group option, or type HELP GROUP for more info.\r\n");		
	}
}


ACMD(do_harness) {
	char_data *victim;
	obj_data *rope = NULL, *cart = NULL;

	/* subcmd 0 = harness, 1 = unharness */

	two_arguments(argument, arg, buf);

	if (!*arg || (!subcmd && !*buf)) {
		if (subcmd)
			msg_to_char(ch, "Remove whose harness?\r\n");
		else
			msg_to_char(ch, "Harness whom to what?\r\n");
	}
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (subcmd && !GET_PULLING(victim))
		act("$E isn't harnessed.", FALSE, ch, 0, victim, TO_CHAR);
	else if (subcmd) {
		obj_to_char((rope = read_object(o_ROPE)), ch);
		cart = GET_PULLING(victim);
		if (GET_PULLED_BY(cart, 0) == victim) {
			cart->pulled_by1 = NULL;
		}
		if (GET_PULLED_BY(cart, 1) == victim) {
			cart->pulled_by2 = NULL;
		}
		GET_PULLING(victim) = NULL;
		act("You unlatch $N from $p.", FALSE, ch, cart, victim, TO_CHAR);
		act("$n unlatches you from $p.", FALSE, ch, cart, victim, TO_VICT);
		act("$n unlatches $N from $p.", FALSE, ch, cart, victim, TO_NOTVICT);
		load_otrigger(rope);
	}
	else if (GET_PULLING(victim))
		act("$E is already harnessed!", FALSE, ch, 0, victim, TO_CHAR);
	else if (!IS_NPC(victim))
		msg_to_char(ch, "You can only harness animals.\r\n");
	else if (!(cart = get_obj_in_list_vis(ch, buf, ROOM_CONTENTS(IN_ROOM(ch)))))
		msg_to_char(ch, "You don't see a %s here.\r\n", buf);
	else if (GET_OBJ_TYPE(cart) != ITEM_CART || GET_CART_ANIMALS_REQUIRED(cart) < 1)
		msg_to_char(ch, "You can't harness anyone to that!\r\n");
	else if (GET_PULLED_BY(cart, 0) && (GET_CART_ANIMALS_REQUIRED(cart) <= 1 || GET_PULLED_BY(cart, 1)))
		msg_to_char(ch, "You can't harness any more animals to it.\r\n");
	else if (!(rope = get_obj_in_list_num(o_ROPE, ch->carrying)))
		msg_to_char(ch, "You need some rope to do that.\r\n");
	else if (!MOB_FLAGGED(victim, MOB_MOUNTABLE))
		act("You can't harness $N to that!", FALSE, ch, 0, victim, TO_CHAR);
	else {
		extract_obj(rope);
		if (GET_PULLED_BY(cart, 0)) {
			cart->pulled_by2 = victim;
		}
		else {
			cart->pulled_by1 = victim;
		}
		GET_PULLING(victim) = cart;
		act("You harness $N to $p.", FALSE, ch, cart, victim, TO_CHAR);
		act("$n harnesses you to $p.", FALSE, ch, cart, victim, TO_VICT);
		act("$n harnesses $N to $p.", FALSE, ch, cart, victim, TO_NOTVICT);
	}
}


ACMD(do_herd) {
	extern int perform_move(char_data *ch, int dir, int need_specials_check, byte mode);
	extern const int rev_dir[];

	struct room_direction_data *ex;
	char_data *victim;
	int dir;
	room_data *to_room;

	two_arguments(argument, arg, buf);

	if (IS_NPC(ch))
		return;
	else if (!*arg || !*buf)
		msg_to_char(ch, "Who do you want to herd, and which direction?\r\n");
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (victim == ch)
		msg_to_char(ch, "You can't herd yourself.\r\n");
	else if (!IS_NPC(victim) || victim->desc || !MOB_FLAGGED(victim, MOB_ANIMAL))
		msg_to_char(ch, "You can only herd animals.\r\n");
	else if (IS_NPC(victim) && MOB_FLAGGED(victim, MOB_HARD | MOB_GROUP | MOB_AGGRESSIVE)) {
		act("You can't herd $N!", FALSE, ch, NULL, victim, TO_CHAR);
	}
	else if ((dir = parse_direction(ch, buf)) == NO_DIR || !(to_room = real_shift(IN_ROOM(ch), shift_dir[dir][0], shift_dir[dir][1])))
		msg_to_char(ch, "That's not a direction!\r\n");
	else if (!ROOM_IS_CLOSED(IN_ROOM(ch)) && dir >= NUM_2D_DIRS) {
		msg_to_char(ch, "You can't herd anything in that direction.\r\n");
	}
	else if (COMPLEX_DATA(IN_ROOM(ch)) && ROOM_IS_CLOSED(IN_ROOM(ch)) && (!(ex = find_exit(IN_ROOM(ch), dir)) || !(to_room = ex->room_ptr) || !CAN_GO(ch, ex))) {
		msg_to_char(ch, "You can't herd anything in that direction.\r\n");
	}
	else if (ROOM_IS_CLOSED(IN_ROOM(ch)) && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED))
		msg_to_char(ch, "You don't have permission to herd here.\r\n");
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_ROUGH))
		msg_to_char(ch, "You find it difficult to do that here.\r\n");
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_ROUGH))
		msg_to_char(ch, "You find that difficult to do.\r\n");
	else if (GET_LED_BY(victim) && IN_ROOM(GET_LED_BY(victim)) == IN_ROOM(victim))
		msg_to_char(ch, "You can't herd someone who is being led by someone else.\r\n");
	else if (ROOM_IS_CLOSED(to_room) && !ROOM_BLD_FLAGGED(to_room, BLD_HERD))
		msg_to_char(ch, "You can't herd an animal into a building.\r\n");
	else if (ROOM_IS_CLOSED(to_room) && !IS_INSIDE(IN_ROOM(ch)) && BUILDING_ENTRANCE(to_room) != dir && (!ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES) || BUILDING_ENTRANCE(to_room) != rev_dir[dir])) {
		msg_to_char(ch, "You can only herd an animal through the entrance.\r\n");
	}
	else {
		if (perform_move(victim, dir, TRUE, 0)) {
			act("You skillfully herd $N.", FALSE, ch, 0, victim, TO_CHAR);
			act("$n skillfully herds $N.", FALSE, ch, 0, victim, TO_ROOM);
			if (!perform_move(ch, dir, FALSE, 0)) {
				char_to_room(victim, IN_ROOM(ch));
			}
		}
		else {
			act("You try to herd $N, but $E refuses to move!", FALSE, ch, 0, victim, TO_CHAR);
			act("$n tries to herd $N away, but $E refuses to move!", FALSE, ch, 0, victim, TO_ROOM);
		}
	}
}


ACMD(do_milk) {
	char_data *mob;
	obj_data *cont;
	int amount;

	two_arguments(argument, arg, buf);

	if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_STABLE))
		msg_to_char(ch, "You can't milk animals here!\r\n");
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to finish building the stable before you can milk anything.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED))
		msg_to_char(ch, "You don't have permission to milk these animals.\r\n");
	else if (!*arg || !*buf)
		msg_to_char(ch, "What would you like to milk, and into what?\r\n");
	else if (!(mob = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (!IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_MILKABLE))
		act("You can't milk $N!", FALSE, ch, 0, mob, TO_CHAR);
	else if (get_cooldown_time(mob, COOLDOWN_MILK) > 0)
		act("$E can't be milked again for a while.", FALSE, ch, 0, mob, TO_CHAR);
	else if (!(cont = get_obj_in_list_vis(ch, buf, ch->carrying)))
		msg_to_char(ch, "You don't seem to have a %s.\r\n", buf);
	else if (GET_OBJ_TYPE(cont) != ITEM_DRINKCON)
		act("You can't milk $N into $p!", FALSE, ch, cont, mob, TO_CHAR);
	else if (GET_DRINK_CONTAINER_CONTENTS(cont) > 0 && GET_DRINK_CONTAINER_TYPE(cont) != LIQ_MILK)
		msg_to_char(ch, "It's already full of something else.\r\n");
	else if (GET_DRINK_CONTAINER_CONTENTS(cont) >= GET_DRINK_CONTAINER_CAPACITY(cont))
		msg_to_char(ch, "It's already full.\r\n");
	else {
		act("You milk $N into $p.", FALSE, ch, cont, mob, TO_CHAR);
		act("$n milks $N into $p.", FALSE, ch, cont, mob, TO_ROOM);
		add_cooldown(mob, COOLDOWN_MILK, 2 * SECS_PER_MUD_DAY);
		amount = GET_DRINK_CONTAINER_CAPACITY(cont) - GET_DRINK_CONTAINER_CONTENTS(cont);
		GET_OBJ_VAL(cont, VAL_DRINK_CONTAINER_CONTENTS) += amount;
		GET_OBJ_VAL(cont, VAL_DRINK_CONTAINER_TYPE) = LIQ_MILK;
		GET_OBJ_TIMER(cont) = 72;	// mud hours
	}
}


ACMD(do_mydescription) {
	skip_spaces(&argument);
	
	if (IS_NPC(ch) || !ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Your current description (type 'mydescription set' to change):\r\n%s", GET_LONG_DESC(ch) ? GET_LONG_DESC(ch) : " not set\r\n");
	}
	else if (!str_cmp(argument, "set")) {
		if (ch->desc->str) {
			msg_to_char(ch, "You are already editing text.\r\n");
		}
		else {
			start_string_editor(ch->desc, "your description", &(GET_LONG_DESC(ch)), MAX_PLAYER_DESCRIPTION);
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
	char_data *vict;
	struct follow_type *k;

	half_chop(argument, name, message);

	if (!*name || !*message)
		send_to_char("Order who to do what?\r\n", ch);
	else if (!(vict = get_char_vis(ch, name, FIND_CHAR_ROOM)) && !is_abbrev(name, "followers"))
		send_to_char("That person isn't here.\r\n", ch);
	else if (ch == vict)
		send_to_char("You obviously suffer from schizophrenia.\r\n", ch);
	else {
		if (AFF_FLAGGED(ch, AFF_CHARM)) {
			send_to_char("Your superior would not aprove of you giving orders.\r\n", ch);
			return;
		}
		if (vict) {
			sprintf(buf, "$N orders you to '%s'", message);
			act(buf, FALSE, vict, 0, ch, TO_CHAR);
			act("$n gives $N an order.", FALSE, ch, 0, vict, TO_ROOM);

			if ((vict->master != ch) || !AFF_FLAGGED(vict, AFF_CHARM))
				act("$n has an indifferent look.", FALSE, vict, 0, 0, TO_ROOM);
			else {
				send_config_msg(ch, "ok_string");
				command_interpreter(vict, message);
			}
		}
		else {			/* This is order "followers" */
			sprintf(buf, "$n issues the order '%s'.", message);
			act(buf, FALSE, ch, 0, vict, TO_ROOM);

			org_room = IN_ROOM(ch);

			for (k = ch->followers; k; k = k->next) {
				if (org_room == IN_ROOM(k->follower))
					if (AFF_FLAGGED(k->follower, AFF_CHARM)) {
						found = TRUE;
						command_interpreter(k->follower, message);
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
	extern char *show_color_codes(char *string);
	
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
		sprintf(buf, "Your %s is currently: %s\r\n", types[subcmd], (*prompt ? show_color_codes(*prompt) : "n/a"));
		send_to_char(buf, ch);
		return;
	}

	delete_doubledollar(argument);

	// char_file_u only holds MAX_PROMPT_SIZE+1
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
	}
	else {
		if (*prompt) {
			free(*prompt);
		}
		*prompt = str_dup(argument);

		sprintf(buf, "Okay, set your %s to: %s\r\n", types[subcmd], show_color_codes(*prompt));
		send_to_char(buf, ch);
	}
}


ACMD(do_quit) {
	void display_statistics_to_char(char_data *ch);
	extern obj_data *player_death(char_data *ch);
	
	descriptor_data *d, *next_d;
	bool died = FALSE;

	if (IS_NPC(ch) || !ch->desc)
		return;

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
	else if (ROOM_OWNER(IN_ROOM(ch)) && empire_is_hostile(ROOM_OWNER(IN_ROOM(ch)), GET_LOYALTY(ch), IN_ROOM(ch))) {
		msg_to_char(ch, "You can't quit in hostile territory.\r\n");
	}
	else {
		if (GET_POS(ch) < POS_STUNNED) {
			msg_to_char(ch, "You shuffle off this mortal coil, and die...\r\n");
			act("$n shuffles off $s mortal coil and dies.", FALSE, ch, NULL, NULL, TO_ROOM);
			player_death(ch);
			died = TRUE;
		}
				
		// store last known level now
		GET_LAST_KNOWN_LEVEL(ch) = GET_COMPUTED_LEVEL(ch);
		
		if (!GET_INVIS_LEV(ch)) {
			act("$n has left the game.", TRUE, ch, 0, 0, TO_ROOM);
		}
		syslog(SYS_LOGIN, GET_INVIS_LEV(ch), TRUE, "%s has quit the game.", GET_NAME(ch));
		if (GET_INVIS_LEV(ch) == 0) {
			mortlog("%s has left the game", PERS(ch, ch, 1));
		}
		send_to_char("Goodbye, friend.. Come back soon!\r\n", ch);

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
		
		Objsave_char(ch, RENT_RENTED);
		save_char(ch, died ? NULL : IN_ROOM(ch));
		
		display_statistics_to_char(ch);
		
		// this will disconnect the descriptor
		extract_char(ch);
	}
}


ACMD(do_save) {
	void write_aliases(char_data *ch);

	if (!IS_NPC(ch) && ch->desc) {
		if (cmd) {
			msg_to_char(ch, "Saving %s.\r\n", GET_NAME(ch));
		}
				
		// store last known level now
		GET_LAST_KNOWN_LEVEL(ch) = GET_COMPUTED_LEVEL(ch);

		write_aliases(ch);
		SAVE_CHAR(ch);
		Objsave_char(ch, RENT_CRASH);
	}
}


ACMD(do_selfdelete) {
	void delete_player_character(char_data *ch);

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
		// actual delete (rent out equipment first)
		Objsave_char(ch, RENT_RENTED);
		delete_player_character(ch);
		
		// logs and messaging
		syslog(SYS_INFO, GET_INVIS_LEV(ch), TRUE, "DEL: %s (lev %d) has self-deleted.", GET_NAME(ch), GET_ACCESS_LEVEL(ch));
		if (!GET_INVIS_LEV(ch)) {
			act("$n has left the game.", TRUE, ch, 0, 0, TO_ROOM);
		}
		if (GET_INVIS_LEV(ch) == 0) {
			mortlog("%s has left the game", PERS(ch, ch, TRUE));
		}
		msg_to_char(ch, "You have deleted your character. Goodbye...\r\n");
		
		// bye now
		extract_char(ch);
	}
}


ACMD(do_shear) {
	char_data *mob;

	one_argument(argument, arg);

	if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_STABLE) || !IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to be in a stable to shear anything.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're a bit busy right now.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Which animal would you like to shear?\r\n");
	}
	else if (!(mob = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (!IS_NPC(mob)) {
		act("You can't shear $N!", FALSE, ch, NULL, mob, TO_CHAR);
	}
	else if (get_cooldown_time(mob, COOLDOWN_SHEAR) > 0) {
		act("$E is already shorn.", FALSE, ch, NULL, mob, TO_CHAR);
	}
	else {
		if (run_interactions(ch, mob->interactions, INTERACT_SHEAR, IN_ROOM(ch), mob, NULL, shear_interact)) {
			gain_ability_exp(ch, ABIL_MASTER_FARMER, 5);
		}
		else {
			act("You can't shear $N!", FALSE, ch, NULL, mob, TO_CHAR);
		}
	}
}


ACMD(do_skin) {
	extern obj_data *has_sharp_tool(char_data *ch);

	obj_data *obj;
	char_data *proto;

	one_argument(argument, arg);

	if (!*arg)
		msg_to_char(ch, "What would you like to skin.\r\n");
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)) && !(obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)))))
		msg_to_char(ch, "You don't seem to have anything like that.\r\n");
	else if (!IS_CORPSE(obj))
		msg_to_char(ch, "You can only skin corpses.\r\n");
	else if (GET_CORPSE_NPC_VNUM(obj) == NOTHING || !(proto = mob_proto(GET_CORPSE_NPC_VNUM(obj)))) {
		msg_to_char(ch, "You can't skin that.\r\n");
	}
	else if (IS_SET(GET_CORPSE_FLAGS(obj), CORPSE_EATEN))
		msg_to_char(ch, "It's too badly mangled to get any amount of usable skin.\r\n");
	else if (IS_SET(GET_CORPSE_FLAGS(obj), CORPSE_SKINNED))
		msg_to_char(ch, "It's already been skinned.\r\n");
	else if (!has_sharp_tool(ch))
		msg_to_char(ch, "You need to be wielding a sharp tool to skin a corpse.\r\n");
	else {
		// run it
		if (!run_interactions(ch, proto->interactions, INTERACT_SKIN, IN_ROOM(ch), NULL, obj, skin_interact)) {
			msg_to_char(ch, "There isn't anything you can skin from that corpse.\r\n");
		}
	}
}


ACMD(do_summon) {
	bool check_scaling(char_data *mob, char_data *attacker);
	extern bool check_vampire_sun(char_data *ch, bool message);
	void summon_materials(char_data *ch, char *argument);
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	char_data *mob;
	int vnum = NOTHING, ability = NO_ABIL, iter, max = 1, cost = 0;
	empire_data *emp = NULL;
	bool follow = FALSE, familiar = FALSE, charm = FALSE;
	int count, cooldown = NOTHING, cooldown_time = 0, cost_type = MOVE, gain = 20;
	
	const int animal_vnums[] = { DOG, CHICKEN, QUAIL };
	const int num_animal_vnums = 3;
	
	const int human_vnums[] = { HUMAN_MALE_1, HUMAN_MALE_2, HUMAN_FEMALE_1, HUMAN_FEMALE_2 };
	const int num_human_vnums = 4;
	
	argument = one_argument(argument, arg);
	
	// types of summon
	if (is_abbrev(arg, "humans")) {
		if (!check_vampire_sun(ch, TRUE)) {
			return;
		}
		ability = ABIL_SUMMON_HUMANS;
		cooldown = COOLDOWN_SUMMON_HUMANS;
		cooldown_time = SECS_PER_REAL_MIN;
	}
	else if (is_abbrev(arg, "animals")) {
		ability = ABIL_SUMMON_ANIMALS;
		cooldown = COOLDOWN_SUMMON_ANIMALS;
		cooldown_time = 30;
	}
	else if (is_abbrev(arg, "guards")) {
		ability = ABIL_SUMMON_GUARDS;
		cooldown = COOLDOWN_SUMMON_GUARDS;
		cooldown_time = 3 * SECS_PER_REAL_MIN;
	}
	else if (is_abbrev(arg, "bodyguard")) {
		ability = ABIL_SUMMON_BODYGUARD;
		cooldown = COOLDOWN_SUMMON_BODYGUARD;
		cooldown_time = 5 * SECS_PER_REAL_MIN;
	}
	else if (is_abbrev(arg, "thugs")) {
		ability = ABIL_SUMMON_THUGS;
		cooldown = COOLDOWN_SUMMON_THUGS;
		cooldown_time = 3 * SECS_PER_REAL_MIN;
	}
	else if (is_abbrev(arg, "swift")) {
		ability = ABIL_SUMMON_SWIFT;
		cooldown = COOLDOWN_SUMMON_SWIFT;
		cooldown_time = SECS_PER_REAL_MIN;
	}
	else if (!IS_NPC(ch) && is_abbrev(arg, "materials")) {
		ability = ABIL_SUMMON_MATERIALS;
		summon_materials(ch, argument);
		return;
	}
	else {
		msg_to_char(ch, "What do you want to summon?\r\n");
		return;
	}
	
	if (!can_use_ability(ch, ability, NOTHING, 0, cooldown)) {
		return;
	}
	
	for (mob = ROOM_PEOPLE(IN_ROOM(ch)), count = 0; mob; mob = mob->next_in_room) {
		if (IS_NPC(mob)) {
			++count;
		}
	}
	
	if (count > config_get_int("summon_npc_limit")) {
		msg_to_char(ch, "There are too many npcs here to summon more.\r\n");
		return;
	}

	// types of summon	
	switch (ability) {
		case ABIL_SUMMON_HUMANS: {
			if (!IS_VAMPIRE(ch)) {
				send_config_msg(ch, "must_be_vampire");
				return;
			}
			
			cost = 10;
			cost_type = BLOOD;
			if (!can_use_ability(ch, ability, cost_type, cost, cooldown)) {
				return;
			}
			
			vnum = human_vnums[number(0, num_human_vnums-1)];
			max = ceil(GET_CHARISMA(ch) / 3.0);
			break;
		}
		case ABIL_SUMMON_ANIMALS: {
			cost = 10;
			cost_type = MANA;
			if (!can_use_ability(ch, ability, cost_type, cost, cooldown)) {
				return;
			}

			charm = TRUE;
			vnum = animal_vnums[number(0, num_animal_vnums-1)];
			max = ceil(GET_WITS(ch) / 3.0);
			break;
		}
		case ABIL_SUMMON_SWIFT: {
			cost = 10;
			cost_type = MANA;
			
			// argument
			skip_spaces(&argument);
			
			if (is_abbrev(argument, "stag")) {
				vnum = SWIFT_STAG;
			}
			else if (is_abbrev(argument, "deinonychus") && IS_SPECIALTY_SKILL(ch, SKILL_HIGH_SORCERY)) {
				vnum = SWIFT_DEINONYCHUS;
			}
			else if (is_abbrev(argument, "serpent") && IS_SPECIALTY_SKILL(ch, SKILL_HIGH_SORCERY)) {
				vnum = SWIFT_SERPENT;
			}
			else if (is_abbrev(argument, "liger") && IS_CLASS_SKILL(ch, SKILL_HIGH_SORCERY)) {
				vnum = SWIFT_LIGER;
			}
			else if (is_abbrev(argument, "bear") && IS_CLASS_SKILL(ch, SKILL_HIGH_SORCERY)) {
				vnum = SWIFT_BEAR;
			}
			else {
				send_to_char("What kind of swift would you like to summon: stag", ch);
				if (IS_SPECIALTY_SKILL(ch, SKILL_HIGH_SORCERY)) { 
					send_to_char(", deinonychus, serpent", ch);
				}
				if (IS_CLASS_SKILL(ch, SKILL_HIGH_SORCERY)) { 
					send_to_char(", liger, bear", ch);
				}
				
				send_to_char("\r\n", ch);
				return;
			}
			
			if (!can_use_ability(ch, ability, cost_type, cost, cooldown)) {
				return;
			}

			max = 1;
			break;
		}
		case ABIL_SUMMON_THUGS: {
			cost = 10;
			cost_type = MOVE;
			
			if (!can_use_ability(ch, ability, cost_type, cost, cooldown)) {
				return;
			}
			
			vnum = THUG;
			max = ceil(GET_CHARISMA(ch) / 3.0);
			follow = TRUE;
			break;
		}
		case ABIL_SUMMON_GUARDS: {
			cost = 25;
			cost_type = MOVE;
			
			if (!(emp = GET_LOYALTY(ch))) {
				msg_to_char(ch, "You must be in an empire to summon guards.\r\n");
				return;
			}
			if (GET_LOYALTY(ch) != ROOM_OWNER(IN_ROOM(ch))) {
				msg_to_char(ch, "You can only summon guards in your empire's territory.\r\n");
				return;
			}

			if (!can_use_ability(ch, ability, cost_type, cost, cooldown)) {
				return;
			}
			
			vnum = GUARD;
			max = MAX(1, GET_CHARISMA(ch) / 2);
			break;
		}
		case ABIL_SUMMON_BODYGUARD: {
			cost = 25;
			cost_type = MOVE;
			
			if (!(emp = GET_LOYALTY(ch))) {
				msg_to_char(ch, "You must be in an empire to summon a bodyguard.\r\n");
				return;
			}
			if (has_familiar(ch)) {
				msg_to_char(ch, "You can't summon a bodyguard while you have a familiar or charmed follower.\r\n");
				return;
			}
			if (!can_use_ability(ch, ability, cost_type, cost, cooldown)) {
				return;
			}
			
			vnum = BODYGUARD;
			follow = TRUE;
			familiar = TRUE;
			break;
		}
	}

	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_START_LOCATION)) {
		msg_to_char(ch, "You can't summon anyone here.\r\n");
		return;
	}
	
	if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_BARRIER)) {
		msg_to_char(ch, "You can't summon anyone here.\r\n");
		return;
	}

	if (vnum == NOTHING) {
		msg_to_char(ch, "This ability doesn't seem to work properly.\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ability)) {
		return;
	}
	
	charge_ability_cost(ch, cost_type, cost, cooldown, cooldown_time);

	msg_to_char(ch, "You whistle loudly...\r\n");
	act("$n whistles loudly!", FALSE, ch, 0, 0, TO_ROOM);

	for (iter = 0; iter < max; ++iter) {
		if (skill_check(ch, ability, DIFF_MEDIUM)) {
			mob = read_mobile(vnum);
			if (IS_NPC(ch)) {
				MOB_INSTANCE_ID(mob) = MOB_INSTANCE_ID(ch);
			}
			
			if (emp) {
				// guarantee empire flag
				SET_BIT(MOB_FLAGS(mob), MOB_EMPIRE);
			}
			setup_generic_npc(mob, emp, NOTHING, NOTHING);

			// try to scale mob to the summoner
			check_scaling(mob, ch);
			
			// spawn data
			SET_BIT(MOB_FLAGS(mob), MOB_SPAWNED | MOB_NO_LOOT);
			MOB_SPAWN_TIME(mob) = time(0);
			
			char_to_room(mob, IN_ROOM(ch));
			act("$n approaches!", FALSE, mob, 0, 0, TO_ROOM);
			
			if (familiar) {				
				SET_BIT(AFF_FLAGS(mob), AFF_CHARM);
				SET_BIT(MOB_FLAGS(mob), MOB_FAMILIAR);
				add_follower(mob, ch, TRUE);
			}
			else if (charm) {
				SET_BIT(AFF_FLAGS(mob), AFF_CHARM);
				add_follower(mob, ch, TRUE);
			}
			else if (follow) {
				add_follower(mob, ch, TRUE);
				SET_BIT(MOB_FLAGS(mob), MOB_SENTINEL);
			}
			
			load_mtrigger(mob);
		}
	}
	
	if (ability != NO_ABIL) {
		gain_ability_exp(ch, ability, gain);
	}
}


ACMD(do_title) {
	skip_spaces(&argument);
	delete_doubledollar(argument);

	if (IS_NPC(ch))
		send_to_char("Your title is fine... go away.\r\n", ch);
	else if (PLR_FLAGGED(ch, PLR_NOTITLE))
		send_to_char("You can't title yourself -- you shouldn't have abused it!\r\n", ch);
	else if (strstr(argument, "(") || strstr(argument, ")") || strstr(argument, "%"))
		send_to_char("Titles can't contain the (, ), or % characters.\r\n", ch);
	else if (strlen(argument) > MAX_TITLE_LENGTH-1 || (strlen(argument) - (2 * count_color_codes(argument))) > MAX_TITLE_LENGTH_NO_COLOR) {
		// the -1 reserves an extra spot for an extra space
		msg_to_char(ch, "Sorry, titles can't be longer than %d characters.\r\n", MAX_TITLE_LENGTH_NO_COLOR);
	}
	else {
		set_title(ch, argument);
		msg_to_char(ch, "Okay, you're now %s%s.\r\n", PERS(ch, ch, 1), GET_TITLE(ch));
	}
}


ACMD(do_toggle) {
	extern const struct toggle_data_type toggle_data[];	// constants.c
	
	const char *togcols[NUM_TOG_TYPES][2] = { { "&r", "&g" }, { "&g", "&r" } };
	const char *tognames[NUM_TOG_TYPES][2] = { { "off", "on" }, { "on", "off" } };

	int iter, type = NOTHING, count, on;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs do not have toggles.\r\n");
		return;
	}

	skip_spaces(&argument);
	
	for (iter = 0; *toggle_data[iter].name != '\n' && type == NOTHING; ++iter) {
		if (toggle_data[iter].level <= GET_ACCESS_LEVEL(ch) && is_abbrev(argument, toggle_data[iter].name)) {
			type = iter;
		}
	}
	
	if (!*argument || type == NOTHING) {
		msg_to_char(ch, "Toggles:\r\n");
		
		for (iter = count = 0; *toggle_data[iter].name != '\n'; ++iter) {
			if (toggle_data[iter].level <= GET_ACCESS_LEVEL(ch)) {
				on = PRF_FLAGGED(ch, toggle_data[iter].bit) ? 1 : 0;
				msg_to_char(ch, " [%s%3.3s&0] %-15.15s%s", togcols[toggle_data[iter].type][on], tognames[toggle_data[iter].type][on], toggle_data[iter].name, (!(++count % 3) ? "\r\n" : ""));
			}
		}
		
		if (count % 3) {
			send_to_char("\r\n", ch);
		}
	}
	else if (type != NOTHING) {
		on = PRF_TOG_CHK(ch, toggle_data[type].bit);
		on = PRF_FLAGGED(ch, toggle_data[type].bit) ? 1 : 0;
		
		// special case for pvp toggle
		if (toggle_data[type].bit == PRF_ALLOW_PVP) {
			add_cooldown(ch, COOLDOWN_PVP_FLAG, config_get_int("pvp_timer") * SECS_PER_REAL_MIN);
		}
		
		msg_to_char(ch, "You toggle %s %s%s&0.\r\n", toggle_data[type].name, togcols[toggle_data[type].type][on], tognames[toggle_data[type].type][on]);
		
		// maybe notify
		if (toggle_data[type].notify_func != NULL) {
			(toggle_data[type].notify_func)(ch);
		}
	}
}


ACMD(do_visible) {
	void perform_immort_vis(char_data *ch);

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
