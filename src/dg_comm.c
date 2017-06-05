/* ************************************************************************
*   File: dg_comm.c                                       EmpireMUD 2.0b5 *
*  Usage: string and messaging functions for DG Scripts                   *
*                                                                         *
*  DG Scripts code had no header info in this file                        *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "dg_scripts.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "skills.h"

/* external functions */
extern char_data *get_char_in_room(room_data *room, char *name);
extern obj_data *get_obj_in_room(room_data *room, char *name);

/* same as any_one_arg except that it stops at punctuation */
char *any_one_name(char *argument, char *first_arg) {
	char *arg;

	/* Find first non blank */
	while (isspace(*argument)) {
		argument++;
	}

	/* Find length of first word */
	for (arg = first_arg; *argument && !isspace(*argument) && (!ispunct(*argument) || *argument == '#' || *argument == '-'); arg++, argument++) {
		*arg = LOWER(*argument);
	}
	*arg = '\0';

	return argument;
}


void sub_write_to_char(char_data *ch, char *tokens[], void *otokens[], char type[]) {
	char sb[MAX_STRING_LENGTH];
	int i, iter;

	strcpy(sb, "");

	for (i = 0; tokens[i + 1]; i++) {
		strcat(sb,tokens[i]);

		switch (type[i]) {
			case '~': {
				if (!otokens[i])
					strcat(sb,"someone");
				else if ((char_data*)otokens[i] == ch)
					strcat(sb, "you");
				else
					strcat(sb,PERS((char_data*)otokens[i], ch, FALSE));
				break;
			}
			case '|': {
				if (!otokens[i])
					strcat(sb, "someone's");
				else if ((char_data*)otokens[i] == ch)
					strcat(sb, "your");
				else {
					strcat(sb,PERS((char_data*) otokens[i], ch, FALSE));
					strcat(sb,"'s");
				}
				break;
			}
			case '^': {
				if (!otokens[i] || !CAN_SEE(ch, (char_data*) otokens[i]))
					strcat(sb,"its");
				else if ((char_data*)otokens[i] == ch)
					strcat(sb,"your");
				else
					strcat(sb,HSHR((char_data*) otokens[i]));
				break;
			}
			/*
			case '&': {
				if (!otokens[i] || !CAN_SEE(ch, (char_data*) otokens[i]))
					strcat(sb,"it");
				else if ((char_data*)otokens[i] == ch)
					strcat(sb,"you");
				else
					strcat(sb,HSSH((char_data*) otokens[i]));
				break;
			}
			*/
			case '*': {
				if (!otokens[i] || !CAN_SEE(ch, (char_data*) otokens[i]))
					strcat(sb,"it");
				else if ((char_data*)otokens[i] == ch)
					strcat(sb,"you");
				else
					strcat(sb,HMHR((char_data*) otokens[i]));
				break;
			}
			case '@': {
				if (!otokens[i])
					strcat(sb,"something");
				else
					strcat(sb,OBJS(((obj_data*) otokens[i]), ch));
				break;
			}
		}
	}

	strcat(sb,tokens[i]);
	strcat(sb, "&0\r\n");
	
	// find the first non-color-code and cap it
	for (iter = 0; iter < strlen(sb); ++iter) {
		if (sb[iter] == '&') {
			// skip
			++iter;
		}
		else {
			// found one!
			sb[iter] = UPPER(sb[iter]);
			break;
		}
	}
	
	send_to_char(sb, ch);
}


void sub_write(char *arg, char_data *ch, byte find_invis, int targets) {
	char str[MAX_INPUT_LENGTH * 2];
	char type[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH];
	char *tokens[MAX_INPUT_LENGTH], *s, *p;
	void *otokens[MAX_INPUT_LENGTH];
	char_data *to;
	obj_data *obj;
	int i;
	int to_sleeping = 1, is_spammy = 0; /* mainly for windows compiles */

	if (!arg)
		return;

	tokens[0] = str;

	for (i = 0, p = arg, s = str; *p;) {
		switch (*p) {
			case '~':
			case '|':
			case '^':
			// case '&':	// removed this because it conflicts with color codes
			case '*': {
				/* get char_data, move to next token */
				type[i] = *p;
				*s = '\0';
				p = any_one_name(++p, name);
				otokens[i] = find_invis ? get_char_in_room(IN_ROOM(ch), name) : get_char_room_vis(ch, name);
				tokens[++i] = ++s;
				break;
			}

			case '@': {
				/* get obj_data, move to next token */
				type[i] = *p;
				*s = '\0';
				p = any_one_name(++p, name);

				if (find_invis)
					obj = get_obj_in_room(IN_ROOM(ch), name);
				else if (!(obj = get_obj_in_list_vis(ch, name, ROOM_CONTENTS(IN_ROOM(ch))))) {
					// nothing
				}
				else if (!(obj = get_obj_in_equip_vis(ch, name, ch->equipment))) {
					// nothing
				}
				else {
					obj = get_obj_in_list_vis(ch, name, ch->carrying);
				}

				otokens[i] = obj;
				tokens[++i] = ++s;
				break;
			}

			case '\\': {
				p++;
				*s++ = *p++;
				break;
			}

			default: {
				*s++ = *p++;
			}
		}
	}

	*s = '\0';
	tokens[++i] = NULL;

	if (IS_SET(targets, TO_CHAR) && SENDOK(ch) && (AWAKE(ch) || IS_SET(targets, TO_SLEEP)))
		sub_write_to_char(ch, tokens, otokens, type);

	if (IS_SET(targets, TO_ROOM)) {
		for (to = ROOM_PEOPLE(IN_ROOM(ch)); to; to = to->next_in_room) {
			if (to != ch && SENDOK(to) && (AWAKE(to) || IS_SET(targets, TO_SLEEP))) {
				sub_write_to_char(to, tokens, otokens, type);
			}
		}
	}
}
