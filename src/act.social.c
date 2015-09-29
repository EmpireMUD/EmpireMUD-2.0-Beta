/* ************************************************************************
*   File: act.social.c                                    EmpireMUD 2.0b2 *
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
*   Data
*   Social Core
*   Commands
*/

// locals
int find_action(char *name, bool exact);
char *fread_action(FILE * fl, int nr);
void perform_action(char_data *ch, int act_nr, char *argument);


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

#define MAX_SOCIALS		350

/* local globals */
static int list_top = -1;
int social_sort_info[MAX_SOCIALS];

// social data
struct social_messg {
	char *name;

	int hide;
	int min_char_position;		/* Position of the character */
	int min_victim_position;	/* Position of victim */

	/* No argument was supplied */
	char *char_no_arg;
	char *others_no_arg;

	/* An argument was there, and a victim was found */
	char *char_found;		/* if NULL, read no further, ignore args */
	char *others_found;
	char *vict_found;

	/* An argument was there, but no victim was found */
	char *not_found;

	/* The victim turned out to be the character */
	char *char_auto;
	char *others_auto;
} soc_mess_list[MAX_SOCIALS];


void boot_social_messages(void) {
	FILE *fl;
	int nr, hide, min_vpos, min_cpos, curr_soc = -1;
	char next_soc[100];

	/* open social file */
	if (!(fl = fopen(SOCMESS_FILE, "r"))) {
		sprintf(buf, "SYSERR: Can't open socials file '%s'", SOCMESS_FILE);
		perror(buf);
		exit(1);
	}

	/* now read 'em */
	for (nr = 0; nr < MAX_SOCIALS; nr++) {
		fscanf(fl, " %s ", next_soc);

		if (*next_soc == '*') {
			fscanf(fl, "%s\n", next_soc);
			continue;
		}
		if (*next_soc == '$')
			break;

		if (fscanf(fl, " %d %d %d \n", &hide, &min_cpos, &min_vpos) != 3) {
			log("SYSERR: Format error in social file near social '%s'", next_soc);
			exit(1);
		}
		/* read the stuff */
		curr_soc++;
		soc_mess_list[curr_soc].name = strdup(next_soc);
		soc_mess_list[curr_soc].hide = hide;
		soc_mess_list[curr_soc].min_char_position = min_cpos;
		soc_mess_list[curr_soc].min_victim_position = min_vpos;

		soc_mess_list[curr_soc].char_no_arg = fread_action(fl, nr);
		soc_mess_list[curr_soc].others_no_arg = fread_action(fl, nr);
		soc_mess_list[curr_soc].char_found = fread_action(fl, nr);

		/* if no char_found, the rest is to be ignored */
		if (!soc_mess_list[curr_soc].char_found)
			continue;

		soc_mess_list[curr_soc].others_found = fread_action(fl, nr);
		soc_mess_list[curr_soc].vict_found = fread_action(fl, nr);
		soc_mess_list[curr_soc].not_found = fread_action(fl, nr);
		soc_mess_list[curr_soc].char_auto = fread_action(fl, nr);
		soc_mess_list[curr_soc].others_auto = fread_action(fl, nr);
	}

	/* close file & set top */
	fclose(fl);
	list_top = curr_soc;
}


char *fread_action(FILE * fl, int nr) {
	char buf[MAX_STRING_LENGTH], *rslt;

	fgets(buf, MAX_STRING_LENGTH, fl);
	if (feof(fl)) {
		log("SYSERR: fread_action - unexpected EOF near action #%d", nr+1);
		exit(1);
		}
	if (*buf == '#')
		return (NULL);
	else {
		*(buf + strlen(buf) - 1) = '\0';
		CREATE(rslt, char, strlen(buf) + 1);
		strcpy(rslt, buf);
		return (rslt);
	}
}


void sort_socials(void) {
	int a, b, tmp;

	/* initialize array */
	for (a = 0; a <= list_top; a++)
		social_sort_info[a] = a;

	for (a = 0; a <= list_top - 1; a++) {
		for (b = a + 1; b <= list_top; b++) {
			if (strcmp(soc_mess_list[social_sort_info[a]].name, soc_mess_list[social_sort_info[b]].name) > 0) {
				tmp = social_sort_info[a];
				social_sort_info[a] = social_sort_info[b];
				social_sort_info[b] = tmp;
			}
		}
	}
}


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
	int action;
	
	skip_spaces(&string);
	half_chop(string, arg, arg1);
	
	if (!*arg)
		return FALSE;
	
	if ((action = find_action(arg, exact)) == NOTHING)
		return FALSE;

	if (AFF_FLAGGED(ch, AFF_EARTHMELD | AFF_MUMMIFY | AFF_STUNNED) || IS_INJURED(ch, INJ_TIED | INJ_STAKED)) {
		msg_to_char(ch, "You can't do that right now!\r\n");
		return TRUE;
	}
	
	perform_action(ch, action, arg1);
	return TRUE;
}


/**
* @param char *name The typed-in social?
* @param bool exact Must be an exact match if TRUE; may be an abbrev if FALSE.
* @return int The social ID, or NOTHING if no match.
*/
int find_action(char *name, bool exact) {
	int i;

	if (list_top < 0)
		return NOTHING;

	for (i = 0; i <= list_top; i++) {
		if (!str_cmp(name, soc_mess_list[i].name) || (!exact && is_abbrev(name, soc_mess_list[i].name))) {
			return (i);
		}
	}

	return NOTHING;
}


void perform_action(char_data *ch, int act_nr, char *argument) {
	void clear_last_act_message(descriptor_data *desc);
	void add_to_channel_history(descriptor_data *desc, int type, char *message);
	
	char hbuf[MAX_INPUT_LENGTH];
	struct social_messg *action;
	char_data *vict, *c;

	action = &soc_mess_list[act_nr];

	if (action->char_found)
		one_argument(argument, buf);
	else
		*buf = '\0';

	if (GET_POS(ch) < action->min_char_position) {
		switch (GET_POS(ch)) {
			case POS_DEAD:
				send_to_char("Lie still; you are DEAD!!! :-(\r\n", ch);
				break;
			case POS_INCAP:
			case POS_MORTALLYW:
				send_to_char("You are in a pretty bad shape, unable to do anything!\r\n", ch);
				break;
			case POS_STUNNED:
				send_to_char("All you can do right now is think about the stars!\r\n", ch);
				break;
			case POS_SLEEPING:
				send_to_char("In your dreams, or what?\r\n", ch);
				break;
			case POS_RESTING:
				send_to_char("Nah... You feel too relaxed to do that.\r\n", ch);
				break;
			case POS_SITTING:
				send_to_char("Maybe you should get on your feet first?\r\n", ch);
				break;
			case POS_FIGHTING:
				send_to_char("No way! You're fighting for your life!\r\n", ch);
				break;
		}
		return;
	}

	// clear last act messages for everyone in the room
	for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
		if (c->desc) {
			clear_last_act_message(c->desc);

			if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
				msg_to_char(c, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
			}
		}
	}

	if (!*buf) {
		sprintf(hbuf, "&%c%s&0\r\n", (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE) : '0', action->char_no_arg);
		msg_to_char(ch, hbuf);
		if (ch->desc) {
			add_to_channel_history(ch->desc, CHANNEL_HISTORY_SAY, hbuf);
		}
		
		act(action->others_no_arg, action->hide, ch, 0, 0, TO_ROOM | TO_NOT_IGNORING);

		// fetch and store channel history for the room
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
			if (c == ch || !c->desc) {
				continue;
			}
			
			if (c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				sprintf(hbuf, "&%c%s", (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE) : '0', c->desc->last_act_message);
				add_to_channel_history(c->desc, CHANNEL_HISTORY_SAY, hbuf);
			}
			if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
				// terminate color just in case
				msg_to_char(c, "&0");
			}
		}
		
		return;
	}
	if (!(vict = get_char_room_vis(ch, buf))) {
		sprintf(hbuf, "&%c%s&0\r\n", (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE) : '0', action->not_found);
		msg_to_char(ch, hbuf);
		if (ch->desc) {
			add_to_channel_history(ch->desc, CHANNEL_HISTORY_SAY, hbuf);
		}
	}
	else if (vict == ch) {
		// mo message?
		if (!action->char_auto || !*(action->char_auto)) {
			msg_to_char(ch, "You can't really do that.\r\n");
			return;
		}
		
		sprintf(hbuf, "&%c%s&0\r\n", (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE) : '0', action->char_auto);
		msg_to_char(ch, hbuf);
		if (ch->desc) {
			add_to_channel_history(ch->desc, CHANNEL_HISTORY_SAY, hbuf);
		}
		
		act(action->others_auto, action->hide, ch, 0, 0, TO_ROOM | TO_NOT_IGNORING);

		// fetch and store channel history for the room
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
			if (c == ch || !c->desc) {
				continue;
			}
			
			if (c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				sprintf(hbuf, "&%c%s", (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE) : '0', c->desc->last_act_message);
				add_to_channel_history(c->desc, CHANNEL_HISTORY_SAY, hbuf);
			}
			if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
				// terminate color just in case
				msg_to_char(c, "&0");
			}
		}
	}
	else {
		if (GET_POS(vict) < action->min_victim_position) {
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
			act(action->char_found, 0, ch, 0, vict, TO_CHAR | TO_SLEEP);
			act(action->others_found, action->hide, ch, 0, vict, TO_NOTVICT | TO_NOT_IGNORING);
			act(action->vict_found, action->hide, ch, 0, vict, TO_VICT | TO_NOT_IGNORING);

			// fetch and store channel history for the room
			for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
				if (!c->desc) {
					continue;
				}
				
				if (c->desc->last_act_message) {
					// the message was sent via act(), we can retrieve it from the desc
					sprintf(hbuf, "&%c%s", (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) ? GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE) : '0', c->desc->last_act_message);
					add_to_channel_history(c->desc, CHANNEL_HISTORY_SAY, hbuf);
				}
				if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					// terminate color just in case
					msg_to_char(c, "&0");
				}
			}
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
		if (!(victim = get_char_room_vis(ch, arg)))
			send_config_msg(ch, "no_person");
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
	else
		send_to_char("I'm sure you don't want to insult *everybody*...\r\n", ch);
}


ACMD(do_point) {
	extern const char *dirs[];
	
	char_data *vict, *next_vict;
	int dir;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Point at what?\r\n");
	}
	else if ((dir = parse_direction(ch, arg)) != NO_DIR && dir != DIR_RANDOM) {
		msg_to_char(ch, "You point %s.\r\n", dirs[get_direction_for_char(ch, dir)]);
		
		for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = next_vict) {
			next_vict = vict->next_in_room;
			
			if (vict != ch && CAN_SEE(vict, ch) && AWAKE(vict)) {
				sprintf(buf, "$n points %s.", dirs[get_direction_for_char(vict, dir)]);
				act(buf, FALSE, ch, NULL, vict, TO_VICT | TO_NOT_IGNORING);
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
	
	if (num == 1) {
		msg_to_char(ch, "You roll a %d-sided die and get: %d\r\n", size, total);
		snprintf(buf, sizeof(buf), "$n rolls a %d-sided die and gets: %d", size, total);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (GROUP(ch)) {
			for (mem = GROUP(ch)->members; mem; mem = mem->next) {
				if (mem->member != ch && (IN_ROOM(mem->member) != IN_ROOM(ch) || !AWAKE(mem->member))) {
					act(buf, FALSE, ch, NULL, mem->member, TO_VICT | TO_SLEEP);
				}
			}
		}
	}
	else {
		msg_to_char(ch, "You roll %dd%d and get: %d\r\n", num, size, total);
		snprintf(buf, sizeof(buf), "$n rolls %dd%d and gets: %d", num, size, total);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (GROUP(ch)) {
			for (mem = GROUP(ch)->members; mem; mem = mem->next) {
				if (mem->member != ch && (IN_ROOM(mem->member) != IN_ROOM(ch) || !AWAKE(mem->member))) {
					act(buf, FALSE, ch, NULL, mem->member, TO_VICT | TO_SLEEP);
				}
			}
		}
	}
}


ACMD(do_socials) {
	int sortpos, j = 0;

	/* Ugh, don't forget to sort... */
	sort_socials();

	sprintf(buf, "The following social commands are available:\r\n");

	for (sortpos = 0; sortpos <= list_top; sortpos++)
		sprintf(buf+strlen(buf), "%-11.11s%s", soc_mess_list[social_sort_info[sortpos]].name, (!(++j % 7)) ? "\r\n" : "");
	
	if (j % 7) {
		strcat(buf, "\r\n");
	}
	page_string(ch->desc, buf, 1);
}
