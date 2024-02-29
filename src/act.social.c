/* ************************************************************************
*   File: act.social.c                                    EmpireMUD 2.0b5 *
*  Usage: Functions to handle socials                                     *
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
#include "constants.h"
#include "dg_scripts.h"

/**
* Contents:
*   Social Core
*   Commands
*/

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
	char str[MAX_INPUT_LENGTH], arg1[MAX_INPUT_LENGTH];
	social_data *soc;
	
	skip_spaces(&string);
	half_chop(string, str, arg1);
	
	if (!*str) {
		return FALSE;
	}
	
	if (!(soc = find_social(ch, str, exact))) {
		return FALSE;	// no match to any social
	}
	
	// going to process the social: remove hide first
	if (AFF_FLAGGED(ch, AFF_HIDDEN)) {
		REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDDEN);
		affects_from_char_by_aff_flag(ch, AFF_HIDDEN, FALSE);
	}
	
	// does a command trigger override this social?
	if (strlen(string) < strlen(SOC_COMMAND(soc)) && check_command_trigger(ch, SOC_COMMAND(soc), arg1, CMDTRG_EXACT)) {
		return TRUE;
	}
	
	// earthmeld doesn't hit the correct error in char_can_act -- just block all socials in earthmeld
	if (AFF_FLAGGED(ch, AFF_EARTHMELDED)) {
		msg_to_char(ch, "You can't do that while earthmelded.\r\n");
		return TRUE;
	}
	
	// this passes POS_DEAD because social pos is checked in perform_social
	if (!char_can_act(ch, POS_DEAD, TRUE, TRUE, FALSE)) {
		return TRUE;	// sent its own error message
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
	if (GET_FEEDING_FROM(ch) && SOC_MIN_CHAR_POS(soc) >= POS_SLEEPING) {
		msg_to_char(ch, "You can't do that right now!\r\n");
		return;
	}
	if (GET_FED_ON_BY(ch) && SOC_MIN_CHAR_POS(soc) >= POS_SLEEPING) {
		msg_to_char(ch, "The fangs in your flesh prevent you from even moving.\r\n");
		return;
	}
	
	// clear last act messages for everyone in the room
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
		if (c->desc) {
			clear_last_act_message(c->desc);

			if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
				msg_to_char(c, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
			}
		}
	}

	if (!*buf) {
		sprintf(hbuf, "&%c%s&0\r\n", CUSTOM_COLOR_CHAR(ch, CUSTOM_COLOR_EMOTE), NULLSAFE(SOC_MESSAGE(soc, SOCM_NO_ARG_TO_CHAR)));
		send_to_char(hbuf, ch);
		if (ch->desc) {
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, ch, hbuf, FALSE, 0, NOTHING);
		}
		
		act(NULLSAFE(SOC_MESSAGE(soc, SOCM_NO_ARG_TO_OTHERS)), SOC_HIDDEN(soc), ch, FALSE, FALSE, TO_ROOM | TO_NOT_IGNORING);

		// fetch and store channel history for the room
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
			if (c == ch || !c->desc) {
				continue;
			}
			
			if (c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				sprintf(hbuf, "&%c%s", CUSTOM_COLOR_CHAR(c, CUSTOM_COLOR_EMOTE), c->desc->last_act_message);
				add_to_channel_history(c, CHANNEL_HISTORY_SAY, ch, hbuf, (IS_MORPHED(ch) || IS_DISGUISED(ch)), 0, NOTHING);
			}
			if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
				// terminate color just in case
				msg_to_char(c, "&0");
			}
		}
		
		return;
	}
	if (!(vict = get_char_room_vis(ch, buf, NULL))) {
		sprintf(hbuf, "&%c%s&0\r\n", CUSTOM_COLOR_CHAR(ch, CUSTOM_COLOR_EMOTE), NULLSAFE(SOC_MESSAGE(soc, SOCM_TARGETED_NOT_FOUND)));
		send_to_char(hbuf, ch);
		if (ch->desc) {
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, ch, hbuf, FALSE, 0, NOTHING);
		}
	}
	else if (vict == ch) {
		// mo message?
		if (!SOC_MESSAGE(soc, SOCM_SELF_TO_CHAR) || !*SOC_MESSAGE(soc, SOCM_SELF_TO_CHAR)) {
			msg_to_char(ch, "You can't really do that.\r\n");
			return;
		}
		
		sprintf(hbuf, "&%c%s&0\r\n", CUSTOM_COLOR_CHAR(ch, CUSTOM_COLOR_EMOTE), NULLSAFE(SOC_MESSAGE(soc, SOCM_SELF_TO_CHAR)));
		send_to_char(hbuf, ch);
		if (ch->desc) {
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, ch, hbuf, FALSE, 0, NOTHING);
		}
		
		act(NULLSAFE(SOC_MESSAGE(soc, SOCM_SELF_TO_OTHERS)), SOC_HIDDEN(soc), ch, NULL, NULL, TO_ROOM | TO_NOT_IGNORING);

		// fetch and store channel history for the room
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
			if (c == ch || !c->desc) {
				continue;
			}
			
			if (c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				sprintf(hbuf, "&%c%s", CUSTOM_COLOR_CHAR(c, CUSTOM_COLOR_EMOTE), c->desc->last_act_message);
				add_to_channel_history(c, CHANNEL_HISTORY_SAY, ch, hbuf, (IS_MORPHED(ch) || IS_DISGUISED(ch)), 0, NOTHING);
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
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
				if (!c->desc) {
					continue;
				}
				
				if (c->desc->last_act_message) {
					// the message was sent via act(), we can retrieve it from the desc
					sprintf(hbuf, "&%c%s", CUSTOM_COLOR_CHAR(c, CUSTOM_COLOR_EMOTE), c->desc->last_act_message);
					add_to_channel_history(c, CHANNEL_HISTORY_SAY, ch, hbuf, (IS_MORPHED(ch) || IS_DISGUISED(ch)), 0, NOTHING);
				}
				if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					// terminate color just in case
					msg_to_char(c, "&0");
				}
			}
		}
	}
	
	// clear color codes for people we missed
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
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
		if (!(victim = get_char_room_vis(ch, arg, NULL))) {
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
	char buf[MAX_STRING_LENGTH], to_char[MAX_STRING_LENGTH], *to_room;
	char_data *vict, *pers, *next_pers;
	obj_data *obj;
	vehicle_data *veh;
	char color;
	int dir;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Point at what?\r\n");
	}
	else if ((dir = parse_direction(ch, arg)) != NO_DIR && dir != DIR_RANDOM) {
		color = CUSTOM_COLOR_CHAR(ch, CUSTOM_COLOR_EMOTE);
		
		sprintf(buf, "\t%cYou point %s.\t0\r\n", color, dirs[get_direction_for_char(ch, dir)]);
		send_to_char(buf, ch);
		if (ch->desc) {
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, ch, buf, FALSE, 0, NOTHING);
		}
		
		DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), pers, next_pers, next_in_room) {
			if (pers != ch && CAN_SEE(pers, ch) && AWAKE(pers)) {
				if (pers->desc) {
					clear_last_act_message(pers->desc);
				}
				
				color = CUSTOM_COLOR_CHAR(pers, CUSTOM_COLOR_EMOTE);
				sprintf(buf, "\t%c$n points %s.\t0", color, dirs[get_direction_for_char(pers, dir)]);
				act(buf, FALSE, ch, NULL, pers, TO_VICT | TO_NOT_IGNORING);
				
				// channel history
				if (pers->desc && pers->desc->last_act_message) {
					add_to_channel_history(pers, CHANNEL_HISTORY_SAY, ch, pers->desc->last_act_message, (IS_MORPHED(ch) || IS_DISGUISED(ch)), 0, NOTHING);
				}
			}
		}
	}
	else if (!generic_find(arg, NULL, FIND_CHAR_ROOM | FIND_OBJ_ROOM | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, &vict, &obj, &veh)) {
		msg_to_char(ch, "You must have a VERY long index finger because you don't see %s %s here.\r\n", AN(arg), arg);
	}
	else if (vict) {
		// normal point social
		sprintf(buf, "point %s", argument);
		check_social(ch, buf, FALSE);
	}
	else if (obj || veh) {
		// combine these as they both do the same loop
		color = CUSTOM_COLOR_CHAR(ch, CUSTOM_COLOR_EMOTE);
		
		if (obj) {
			safe_snprintf(to_char, sizeof(to_char), "\t%cYou point at $p.\t0", color);
			to_room = "$n points at $p.\t0";
		}
		else if (veh) {
			safe_snprintf(to_char, sizeof(to_char), "\t%cYou point at $v.\t0", color);
			to_room = "$n points at $v.";
		}
		else {
			// ??
			msg_to_char(ch, "You try to point.\r\n");
			return;
		}
		
		// send to ch
		if (ch->desc) {
			clear_last_act_message(ch->desc);
		}
		act(to_char, FALSE, ch, obj ? (const void*)obj : (const void*)veh, NULL, TO_CHAR | (obj ? NOBITS : ACT_VEH_OBJ));
		if (ch->desc && ch->desc->last_act_message) {
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, ch, ch->desc->last_act_message, FALSE, 0, NOTHING);
		}
		
		DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), pers, next_pers, next_in_room) {
			if (pers != ch && CAN_SEE(pers, ch) && AWAKE(pers)) {
				if (pers->desc) {
					clear_last_act_message(pers->desc);
				}
				
				color = CUSTOM_COLOR_CHAR(pers, CUSTOM_COLOR_EMOTE);
				sprintf(buf, "\t%c%s", color, to_room);
				act(buf, FALSE, ch, obj ? (const void*)obj : (const void*)veh, pers, TO_VICT | TO_NOT_IGNORING | (obj ? NOBITS : ACT_VEH_OBJ));
				
				// channel history
				if (pers->desc && pers->desc->last_act_message) {
					add_to_channel_history(pers, CHANNEL_HISTORY_SAY, ch, pers->desc->last_act_message, (IS_MORPHED(ch) || IS_DISGUISED(ch)), 0, NOTHING);
				}
			}
		}
	}
	else {
		msg_to_char(ch, "You must have a VERY long index finger because you don't see %s %s here.\r\n", AN(arg), arg);
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
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
		if (vict->desc) {
			clear_last_act_message(vict->desc);
		}
	}
	
	if (num == 1) {
		safe_snprintf(buf, sizeof(buf), "You roll a %d-sided die and get: %d\r\n", size, total);
		send_to_char(buf, ch);
		if (ch->desc) {
			add_to_channel_history(ch, CHANNEL_HISTORY_ROLL, ch, buf, FALSE, 0, NOTHING);
		}
		
		safe_snprintf(buf, sizeof(buf), "$n rolls a %d-sided die and gets: %d", size, total);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (GROUP(ch)) {
			for (mem = GROUP(ch)->members; mem; mem = mem->next) {
				if (mem->member != ch && (IN_ROOM(mem->member) != IN_ROOM(ch) || !AWAKE(mem->member))) {
					if (mem->member->desc) {
						clear_last_act_message(mem->member->desc);
					}
					
					act(buf, FALSE, ch, NULL, mem->member, TO_VICT | TO_SLEEP);
					
					if (mem->member->desc && mem->member->desc->last_act_message) {
						add_to_channel_history(mem->member, CHANNEL_HISTORY_ROLL, ch, mem->member->desc->last_act_message, (IS_MORPHED(ch) || IS_DISGUISED(ch)), 0, NOTHING);
					}
				}
			}
		}
	}
	else {
		safe_snprintf(buf, sizeof(buf), "You roll %dd%d and get: %d\r\n", num, size, total);
		send_to_char(buf, ch);
		if (ch->desc) {
			add_to_channel_history(ch, CHANNEL_HISTORY_ROLL, ch, buf, FALSE, 0, NOTHING);
		}
		
		safe_snprintf(buf, sizeof(buf), "$n rolls %dd%d and gets: %d", num, size, total);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (GROUP(ch)) {
			for (mem = GROUP(ch)->members; mem; mem = mem->next) {
				if (mem->member != ch && (IN_ROOM(mem->member) != IN_ROOM(ch) || !AWAKE(mem->member))) {
					if (mem->member->desc) {
						clear_last_act_message(mem->member->desc);
					}
					
					act(buf, FALSE, ch, NULL, mem->member, TO_VICT | TO_SLEEP);
					
					if (mem->member->desc && mem->member->desc->last_act_message) {
						add_to_channel_history(mem->member, CHANNEL_HISTORY_ROLL, ch, mem->member->desc->last_act_message, (IS_MORPHED(ch) || IS_DISGUISED(ch)), 0, NOTHING);
					}
				}
			}
		}
	}
	
	// save room last-act
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
		if (vict != ch && vict->desc && vict->desc->last_act_message) {
			add_to_channel_history(vict, CHANNEL_HISTORY_ROLL, ch, vict->desc->last_act_message, (IS_MORPHED(ch) || IS_DISGUISED(ch)), 0, NOTHING);
		}
	}
}


ACMD(do_socials) {
	social_data *soc, *next_soc, *last = NULL;
	
	build_page_display(ch, "The following social commands are available:");
	
	HASH_ITER(sorted_hh, sorted_socials, soc, next_soc) {
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
		build_page_display_col_str(ch, 7, FALSE, SOC_COMMAND(soc));
	}
	
	send_page_display(ch);
}
