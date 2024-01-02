/* ************************************************************************
*   File: dg_comm.c                                       EmpireMUD 2.0b5 *
*  Usage: string and messaging functions for DG Scripts                   *
*                                                                         *
*  DG Scripts code had no header info in this file                        *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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

/* same as any_one_arg except that it stops at punctuation */
char *any_one_name(char *argument, char *first_arg) {
	char *arg;
	bool has_uid = FALSE;

	/* Find first non blank */
	while (isspace(*argument)) {
		argument++;
	}

	/* Find length of first word */
	// update: if it finds a UID char, it now ends after the numeric portion, preventing it from eating the 'self' in *%mob%self
	for (arg = first_arg; *argument && !isspace(*argument) && (!has_uid || isdigit(*argument)) && (!ispunct(*argument) || *argument == UID_CHAR || *argument == '#' || *argument == '-'); arg++, argument++) {
		if (*argument == UID_CHAR) {
			has_uid = TRUE;
		}
		*arg = LOWER(*argument);
	}
	*arg = '\0';

	return argument;
}


void sub_write_to_char(char_data *ch, char *tokens[], void *otokens[], int token_type[], char type[], bool use_queue) {
	char sb[MAX_STRING_LENGTH];
	int i, iter;

	strcpy(sb, "");

	for (i = 0; tokens[i + 1]; i++) {
		strcat(sb,tokens[i]);

		switch (type[i]) {
			case '~': {
				if (!otokens[i] || token_type[i] != TYPE_MOB) {
					strcat(sb,"someone");
				}
				else if ((char_data*)otokens[i] == ch) {
					strcat(sb, "you");
				}
				else {
					strcat(sb,PERS((char_data*)otokens[i], ch, FALSE));
				}
				break;
			}
			case '|': {
				if (!otokens[i] || token_type[i] != TYPE_MOB) {
					strcat(sb, "someone's");
				}
				else if ((char_data*)otokens[i] == ch) {
					strcat(sb, "your");
				}
				else {
					strcat(sb,PERS((char_data*) otokens[i], ch, FALSE));
					strcat(sb,"'s");
				}
				break;
			}
			case '^': {
				if (!otokens[i] || token_type[i] != TYPE_MOB) {
					// formerly included: || !CAN_SEE(ch, (char_data*) otokens[i])
					// TODO if we had plural pronoun support, !see people should be "their"
					strcat(sb,"its");
				}
				else if ((char_data*)otokens[i] == ch) {
					strcat(sb,"your");
				}
				else {
					strcat(sb,HSHR((char_data*) otokens[i]));
				}
				break;
			}
			case '&': {
				if (!otokens[i] || token_type[i] != TYPE_MOB) {
					// formerly included: || !CAN_SEE(ch, (char_data*) otokens[i])
					// TODO if we had plural pronoun support, !see people should be "they"
					strcat(sb,"it");
				}
				else if ((char_data*)otokens[i] == ch) {
					strcat(sb,"you");
				}
				else {
					strcat(sb,HSSH((char_data*) otokens[i]));
				}
				break;
			}
			case '*': {
				if (!otokens[i] || token_type[i] != TYPE_MOB) {
					// formerly included: || !CAN_SEE(ch, (char_data*) otokens[i])
					// TODO if we had plural pronoun support, !see people should be "them"
					strcat(sb,"it");
				}
				else if ((char_data*)otokens[i] == ch) {
					strcat(sb,"you");
				}
				else {
					strcat(sb,HMHR((char_data*) otokens[i]));
				}
				break;
			}
			case '@': {
				if (!otokens[i]) {
					strcat(sb,"something");
				}
				else if (token_type[i] == TYPE_OBJ) {
					strcat(sb,OBJS(((obj_data*) otokens[i]), ch));
				}
				else if (token_type[i] == TYPE_VEH) {
					strcat(sb,get_vehicle_short_desc(((vehicle_data*) otokens[i]), ch));
				}
				else {
					strcat(sb,"something");
				}
				break;
			}
		}
	}

	strcat(sb,tokens[i]);
	strcat(sb, "&0\r\n");
	
	// find the first non-color-code and cap it
	for (iter = 0; iter < strlen(sb); ++iter) {
		if (sb[iter] == COLOUR_CHAR) {
			// skip
			++iter;
		}
		else {
			// found one!
			sb[iter] = UPPER(sb[iter]);
			break;
		}
	}
	
	if (use_queue && ch->desc) {
		stack_simple_msg_to_desc(ch->desc, sb);
	}
	else {
		send_to_char(sb, ch);
	}
}


void sub_write(char *arg, char_data *ch, byte find_invis, int targets) {
	char str[MAX_INPUT_LENGTH * 2];
	char type[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH];
	char *tokens[1024], *s, *p;
	void *otokens[1024];
	int token_type[1024];
	char_data *to;
	obj_data *obj;
	vehicle_data *veh;
	int i;
	int to_sleeping = 1, is_spammy = 0, is_animal_move = 0; /* mainly for windows compiles */

	if (!arg)
		return;

	tokens[0] = str;

	for (i = 0, p = arg, s = str; *p;) {
		switch (*p) {
			case '~':
			case '|':
			case '^':
			case '&':
			case '*': {
				if (*(p+1) != *p) {
					/* get char_data, move to next token */
					type[i] = *p;
					*s = '\0';
					p = any_one_name(++p, name);
					otokens[i] = find_invis ? get_char_in_room(IN_ROOM(ch), name) : get_char_room_vis(ch, name, NULL);
					token_type[i] = TYPE_MOB;
					tokens[++i] = ++s;
				}
				else {
					// double symbols are ignored
					++p;
					*s++ = *p++;
				}
				break;
			}

			case '@': {
				if (*(p+1) != *p) {
					/* get obj_data, move to next token */
					type[i] = *p;
					*s = '\0';
					p = any_one_name(++p, name);

					if (*name == UID_CHAR) {
						obj = get_obj(name);
						if (!obj) {
							veh = get_vehicle(name);
						}
					}
					else if (find_invis) {
						obj = get_obj_in_room(IN_ROOM(ch), name);
						if (!obj) {
							veh = get_vehicle_room(IN_ROOM(ch), name, NULL);
						}
					}
					else if ((obj = get_obj_in_list_vis(ch, name, NULL, ROOM_CONTENTS(IN_ROOM(ch))))) {
						// nothing
					}
					else if ((obj = get_obj_in_equip_vis(ch, name, NULL, ch->equipment, NULL))) {
						// nothing
					}
					else {
						obj = get_obj_in_list_vis(ch, name, NULL, ch->carrying);
						if (!obj) {
							veh = get_vehicle_in_room_vis(ch, name, NULL);
						}
					}

					otokens[i] = obj ? (void*)obj : (void*)veh;
					token_type[i] = obj ? TYPE_OBJ : (veh ? TYPE_VEH : TYPE_OBJ);
					tokens[++i] = ++s;
				}
				else {
					// double symbols are ignored
					++p;
					*s++ = *p++;
				}
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
		sub_write_to_char(ch, tokens, otokens, token_type, type, IS_SET(targets, TO_QUEUE) ? TRUE : FALSE);

	if (IS_SET(targets, TO_ROOM)) {
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), to, next_in_room) {
			if (to != ch && SENDOK(to) && (AWAKE(to) || IS_SET(targets, TO_SLEEP))) {
				sub_write_to_char(to, tokens, otokens, token_type, type, IS_SET(targets, TO_QUEUE) ? TRUE : FALSE);
			}
		}
	}
}
