/* ************************************************************************
*   File: act.social.c                                    EmpireMUD 2.0b5 *
*  Usage: Functions to handle socials                                     *
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

/**
* Contents:
*   Social Core
*   Commands
*/

// external funcs
void add_to_channel_history(char_data *ch, int type, char *message);
void clear_last_act_message(descriptor_data *desc);
extern bool validate_social_requirements(char_data *ch, social_data *soc);

// locals
social_data *find_social(char_data *ch, char *name, bool exact);
void perform_social(char_data *ch, social_data *soc, char *argument);


 //////////////////////////////////////////////////////////////////////////////
//// SOCIAL CORE /////////////////////////////////////////////////////////////

/**
* This checks if "string" is a social, and performs it if so.
*
* @param char_data *ch The actor.
* @param char *string The command they typed.
* @param bool exact if TRUE, must be an exact match; FALSE can be abbrev
* @return bool TRUE if it was a social and we acted; FALSE if not
*/
bool check_social(char_data *ch, char *string, bool exact) {
	char arg1[MAX_STRING_LENGTH];
	social_data *soc;
	
	skip_spaces(&string);
	half_chop(string, arg, arg1);
	
	if (!*arg)
		return FALSE;
	
	if (!(soc = find_social(ch, arg, exact)))
		return FALSE;

	if (AFF_FLAGGED(ch, AFF_EARTHMELD | AFF_MUMMIFY | AFF_STUNNED) || IS_INJURED(ch, INJ_TIED | INJ_STAKED) || GET_FEEDING_FROM(ch) || GET_FED_ON_BY(ch)) {
		msg_to_char(ch, "You can't do that right now!\r\n");
		return TRUE;
	}
	
	perform_social(ch, soc, arg1);
	return TRUE;
}


/**
* @param char *name The typed-in social?
* @param bool exact Must be an exact match if TRUE; may be an abbrev if FALSE.
* @return social_data* The social, or NULL if no match.
*/
social_data *find_social(char_data *ch, char *name, bool exact) {
	social_data *soc, *next_soc, *found = NULL;
	int num_found = 0;
	
	HASH_ITER(sorted_hh, sorted_socials, soc, next_soc) {
		if (LOWER(*SOC_COMMAND(soc)) < LOWER(*name)) {	// shortcut: check first letter
			continue;
		}
		if (LOWER(*SOC_COMMAND(soc)) > LOWER(*name)) {	// short exit: past the right letter
			break;
		}
		if (SOCIAL_FLAGGED(soc, SOC_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
			continue;
		}
		if (exact && str_cmp(name, SOC_COMMAND(soc))) {
			continue;
		}
		if (!exact && !is_abbrev(name, SOC_COMMAND(soc))) {
			continue;
		}
		if (!validate_social_requirements(ch, soc)) {
			continue;
		}
		
		// seems okay:
		
		if (!found) {	// if we don't have one yet, pick this one...
			found = soc;
			num_found = 1;
		}
		else if (found && SOC_REQUIREMENTS(soc) && !SOC_REQUIREMENTS(found)) {
			found = soc;	// replace the last one since this one has requirements
			// do not increment number
		}
		else if (found && !SOC_REQUIREMENTS(soc) && SOC_REQUIREMENTS(found)) {
			// skip this one entirely -- it has no requirements but we found
			// one that does already
		}
		else if (!number(0, num_found++)) {
			// equal weight, pick at random
			found = soc;
		}
	}
	
	return found;
}


/**
* Executes the a pre-validated social command.
*
* @param char_data *ch The person performing the social.
* @param social_data *soc The social to perform.
* @param char *argument The typed-in args, if any.
*/
void perform_social(char_data *ch, social_data *soc, char *argument) {
	void clear_last_act_message(descriptor_data *desc);
	
	char buf[MAX_INPUT_LENGTH], hbuf[MAX_INPUT_LENGTH];
	char_data *vict, *c;
	
	if (SOC_IS_TARGETABLE(soc)) {
		one_argument(argument, buf);
	}
	else {
		*buf = '\0';
	}
	
	if (GET_POS(ch) < SOC_MIN_CHAR_POS(soc)) {
		send_low_pos_msg(ch);
		return;
	}
	
	// clear last act messages for everyone in the room
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
		if (c->desc) {
			clear_last_act_message(c->desc);

			if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
				msg_to_char(c, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
			}
		}
	}

	if (!*buf) {
		sprintf(hbuf, "&%c%s&0\r\n", (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE) : '0', NULLSAFE(SOC_MESSAGE(soc, SOCM_NO_ARG_TO_CHAR)));
		msg_to_char(ch, hbuf);
		if (ch->desc) {
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, hbuf);
		}
		
		act(NULLSAFE(SOC_MESSAGE(soc, SOCM_NO_ARG_TO_OTHERS)), SOC_HIDDEN(soc), ch, FALSE, FALSE, TO_ROOM | TO_NOT_IGNORING);

		// fetch and store channel history for the room
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
			if (c == ch || !c->desc) {
				continue;
			}
			
			if (c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				sprintf(hbuf, "&%c%s", (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE) : '0', c->desc->last_act_message);
				add_to_channel_history(c, CHANNEL_HISTORY_SAY, hbuf);
			}
			if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
				// terminate color just in case
				msg_to_char(c, "&0");
			}
		}
		
		return;
	}
	if (!(vict = get_char_room_vis(ch, buf))) {
		sprintf(hbuf, "&%c%s&0\r\n", (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE) : '0', NULLSAFE(SOC_MESSAGE(soc, SOCM_TARGETED_NOT_FOUND)));
		msg_to_char(ch, hbuf);
		if (ch->desc) {
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, hbuf);
		}
	}
	else if (vict == ch) {
		// mo message?
		if (!SOC_MESSAGE(soc, SOCM_SELF_TO_CHAR) || !*SOC_MESSAGE(soc, SOCM_SELF_TO_CHAR)) {
			msg_to_char(ch, "You can't really do that.\r\n");
			return;
		}
		
		sprintf(hbuf, "&%c%s&0\r\n", (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE) : '0', NULLSAFE(SOC_MESSAGE(soc, SOCM_SELF_TO_CHAR)));
		msg_to_char(ch, hbuf);
		if (ch->desc) {
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, hbuf);
		}
		
		act(NULLSAFE(SOC_MESSAGE(soc, SOCM_SELF_TO_OTHERS)), SOC_HIDDEN(soc), ch, NULL, NULL, TO_ROOM | TO_NOT_IGNORING);

		// fetch and store channel history for the room
		LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
			if (c == ch || !c->desc) {
				continue;
			}
			
			if (c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				sprintf(hbuf, "&%c%s", (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE) : '0', c->desc->last_act_message);
				add_to_channel_history(c, CHANNEL_HISTORY_SAY, hbuf);
			}
			if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
				// terminate color just in case
				msg_to_char(c, "&0");
			}
		}
	}
	else {	// targeting someone
		if (GET_POS(vict) < SOC_MIN_VICT_POS(soc)) {
			switch (GET_POS(vict)) {
				case POS_DEAD:
					act("$E's dead and you can't do much with $M.", FALSE, ch, 0, vict, TO_CHAR);
					break;
				case POS_INCAP:
				case POS_MORTALLYW:
					act("$E's in pretty bad shape.", FALSE, ch, 0, vict, TO_CHAR);
					break;
				case POS_STUNNED:
					act("$E's stunned right now.", FALSE, ch, 0, vict, TO_CHAR);
					break;
				case POS_SLEEPING:
					act("$E's too busy sleeping to bother with you.", FALSE, ch, 0, vict, TO_CHAR);
					break;
				case POS_RESTING:
					act("$E's resting right now.", FALSE, ch, 0, vict, TO_CHAR);
					break;
				case POS_SITTING:
					act("$E needs to be standing for that action.", FALSE, ch, 0, vict, TO_CHAR);
					break;
				case POS_FIGHTING:
					act("$E's too busy fighting.", FALSE, ch, 0, vict, TO_CHAR);
					break;
			}
		}
		else {
			act(NULLSAFE(SOC_MESSAGE(soc, SOCM_TARGETED_TO_CHAR)), FALSE, ch, NULL, vict, TO_CHAR | TO_SLEEP);
			act(NULLSAFE(SOC_MESSAGE(soc, SOCM_TARGETED_TO_OTHERS)), SOC_HIDDEN(soc), ch, NULL, vict, TO_NOTVICT | TO_NOT_IGNORING);
			act(NULLSAFE(SOC_MESSAGE(soc, SOCM_TARGETED_TO_VICTIM)), SOC_HIDDEN(soc), ch, NULL, vict, TO_VICT | TO_NOT_IGNORING);

			// fetch and store channel history for the room
			LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
				if (!c->desc) {
					continue;
				}
				
				if (c->desc->last_act_message) {
					// the message was sent via act(), we can retrieve it from the desc
					sprintf(hbuf, "&%c%s", (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE) : '0', c->desc->last_act_message);
					add_to_channel_history(c, CHANNEL_HISTORY_SAY, hbuf);
				}
				if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					// terminate color just in case
					msg_to_char(c, "&0");
				}
			}
		}
	}
	
	// clear color codes for people we missed
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
		if (!IS_NPC(c) && c->desc && !c->desc->last_act_message && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
			send_to_char("\t0", c);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_insult) {
	char_data *victim;

	skip_spaces(&argument);
	one_argument(argument, arg);

	if (*arg) {
		if (!(victim = get_char_room_vis(ch, arg))) {
			send_config_msg(ch, "no_person");
		}
		else {
			if (victim != ch) {
				sprintf(buf, "You insult %s.\r\n", PERS(victim, ch, 0));
				send_to_char(buf, ch);

				switch (number(0, 2)) {
					case 0:
						if (GET_SEX(ch) == SEX_MALE) {
							if (GET_SEX(victim) == SEX_FEMALE)
								act("$n says that women can't fight.", FALSE, ch, 0, victim, TO_VICT | TO_NOT_IGNORING);
							else
								act("$n accuses you of fighting like a woman!", FALSE, ch, 0, victim, TO_VICT | TO_NOT_IGNORING);
						}
						else {
							if (GET_SEX(victim) != SEX_FEMALE)
								act("$n accuses you of having the smallest... (brain?)", FALSE, ch, 0, victim, TO_VICT | TO_NOT_IGNORING);
							else
								act("$n tells you that you'd lose a beauty contest against a troll.", FALSE, ch, 0, victim, TO_VICT | TO_NOT_IGNORING);
						}
						break;
					case 1:
						act("$n calls your mother a bitch!", FALSE, ch, 0, victim, TO_VICT | TO_NOT_IGNORING);
						break;
					default:
						act("$n tells you to get lost!", FALSE, ch, 0, victim, TO_VICT | TO_NOT_IGNORING);
						break;
				}
				act("$n insults $N.", TRUE, ch, 0, victim, TO_NOTVICT | TO_NOT_IGNORING);
			}
			else {
				send_to_char("You feel insulted.\r\n", ch);
			}
		}
	}
	else {
		send_to_char("I'm sure you don't want to insult *everybody*...\r\n", ch);
	}
}


ACMD(do_point) {
	extern const char *dirs[];
	
	char buf[MAX_STRING_LENGTH];
	char_data *vict, *next_vict;
	char color;
	int dir;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Point at what?\r\n");
	}
	else if ((dir = parse_direction(ch, arg)) != NO_DIR && dir != DIR_RANDOM) {
		color = (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE) : '0';
		
		sprintf(buf, "\t%cYou point %s.\t0\r\n", color, dirs[get_direction_for_char(ch, dir)]);
		send_to_char(buf, ch);
		if (ch->desc) {
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, buf);
		}
		
		for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = next_vict) {
			next_vict = vict->next_in_room;
			
			if (vict != ch && CAN_SEE(vict, ch) && AWAKE(vict)) {
				if (vict->desc) {
					clear_last_act_message(vict->desc);
				}
				
				color = (!IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE) : '0';
				sprintf(buf, "\t%c$n points %s.\t0", color, dirs[get_direction_for_char(vict, dir)]);
				act(buf, FALSE, ch, NULL, vict, TO_VICT | TO_NOT_IGNORING);
				
				// channel history
				if (vict->desc && vict->desc->last_act_message) {
					add_to_channel_history(vict, CHANNEL_HISTORY_SAY, vict->desc->last_act_message);
				}
			}
		}
	}
	else {
		// normal point
		sprintf(buf, "point %s", argument);
		check_social(ch, buf, FALSE);
	}
}


ACMD(do_roll) {
	char numarg[MAX_INPUT_LENGTH], sizearg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	int num, size, total, iter;
	struct group_member_data *mem;
	char_data *vict;
	
	int max_num = 1000;
	
	// convert any "d" in the argument to a space, e.g. 2d6 dice
	for (iter = 0; iter < strlen(argument); ++iter) {
		if (LOWER(argument[iter]) == 'd') {
			argument[iter] = ' ';
		}
	}
	
	two_arguments(argument, numarg, sizearg);
	if (!*numarg && !*sizearg) {
		num = 1;
		size = 100;
	}
	else if (!*sizearg) {
		// this is actually size only
		num = 1;
		size = atoi(numarg);
	}
	else {
		num = atoi(numarg);
		size = atoi(sizearg);
	}
	
	if (num <= 0 || size <= 0) {
		msg_to_char(ch, "Usage: roll [number of dice] [size of dice]\r\n");
		return;
	}
	if (num > max_num) {
		msg_to_char(ch, "You can roll at most %d dice at one time.\r\n", max_num);
		num = MIN(num, max_num);
		// not fatal
	}
	
	total = 0;
	for (iter = 0; iter < num; ++iter) {
		total += number(1, size);
	}
	
	// clear room last-act
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
		if (vict->desc) {
			clear_last_act_message(vict->desc);
		}
	}
	
	if (num == 1) {
		snprintf(buf, sizeof(buf), "You roll a %d-sided die and get: %d\r\n", size, total);
		send_to_char(buf, ch);
		if (ch->desc) {
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, buf);
		}
		
		snprintf(buf, sizeof(buf), "$n rolls a %d-sided die and gets: %d", size, total);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (GROUP(ch)) {
			for (mem = GROUP(ch)->members; mem; mem = mem->next) {
				if (mem->member != ch && (IN_ROOM(mem->member) != IN_ROOM(ch) || !AWAKE(mem->member))) {
					if (mem->member->desc) {
						clear_last_act_message(mem->member->desc);
					}
					
					act(buf, FALSE, ch, NULL, mem->member, TO_VICT | TO_SLEEP);
					
					if (mem->member->desc && mem->member->desc->last_act_message) {
						add_to_channel_history(mem->member, CHANNEL_HISTORY_SAY, mem->member->desc->last_act_message);
					}
				}
			}
		}
	}
	else {
		msg_to_char(ch, "You roll %dd%d and get: %d\r\n", num, size, total);
		// local hist
		
		snprintf(buf, sizeof(buf), "$n rolls %dd%d and gets: %d", num, size, total);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (GROUP(ch)) {
			for (mem = GROUP(ch)->members; mem; mem = mem->next) {
				if (mem->member != ch && (IN_ROOM(mem->member) != IN_ROOM(ch) || !AWAKE(mem->member))) {
					if (mem->member->desc) {
						clear_last_act_message(mem->member->desc);
					}
					
					act(buf, FALSE, ch, NULL, mem->member, TO_VICT | TO_SLEEP);
					
					if (mem->member->desc && mem->member->desc->last_act_message) {
						add_to_channel_history(mem->member, CHANNEL_HISTORY_SAY, mem->member->desc->last_act_message);
					}
				}
			}
		}
	}
	
	// save room last-act
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
		if (vict != ch && vict->desc && vict->desc->last_act_message) {
			add_to_channel_history(vict, CHANNEL_HISTORY_SAY, vict->desc->last_act_message);
		}
	}
}


ACMD(do_socials) {
	char buf[MAX_STRING_LENGTH];
	social_data *soc, *next_soc, *last = NULL;
	int count = 0;
	size_t size;
	
	size = snprintf(buf, sizeof(buf), "The following social commands are available:\r\n");
	
	HASH_ITER(sorted_hh, sorted_socials, soc, next_soc) {
		if (size + 11 > sizeof(buf)) {	// early exit for full buffer
			break;
		}
		
		if (SOCIAL_FLAGGED(soc, SOC_IN_DEVELOPMENT)) {
			continue;
		}
		if (last && !str_cmp(SOC_COMMAND(soc), SOC_COMMAND(last))) {	// skip duplicate names
			continue;
		}
		if (!validate_social_requirements(ch, soc)) {
			continue;
		}
		
		last = soc;	// duplicate prevention
		size += snprintf(buf + size, sizeof(buf) - size, "%-11.11s%s", SOC_COMMAND(soc), (!(++count % 7)) ? "\r\n" : "");
	}
	
	// terminating crlf if possible
	if (count % 7 && (size + 2) < sizeof(buf)) {
		strcat(buf, "\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}
