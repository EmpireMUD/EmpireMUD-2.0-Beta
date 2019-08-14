/* ************************************************************************
*   File: dg_triggers.c                                   EmpireMUD 2.0b5 *
*  Usage: contains all the trigger functions for scripts.                 *
*                                                                         *
*  DG Scripts code by galion, 1996/08/05 23:32:08, revision 3.9           *
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
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "olc.h"
#include "dg_event.h"


extern const char *cmd_door[];

/* external functions */
const char *skill_name(int num);
void add_var(struct trig_var_data **var_list, char *name, char *value, int id);
char *matching_quote(char *p);
bool remove_live_script_by_vnum(struct script_data *script, trig_vnum vnum);
char *str_str(char *cs, char *ct);

// external vars
extern const char *dirs[];
extern const int rev_dir[];
extern struct instance_data *quest_instance_global;

// locals
int buy_vtrigger(char_data *actor, char_data *shopkeeper, obj_data *buying, int cost, any_vnum currency);
int kill_otrigger(obj_data *obj, char_data *dying, char_data *killer);


/*
*  General functions used by several triggers
*/


/*
* Copy first phrase into first_arg, returns rest of string
*/
char *one_phrase(char *arg, char *first_arg) {
	skip_spaces(&arg);

	if (!*arg)
	*first_arg = '\0';

	else if (*arg == '"') {
		char *p, c;

		p = matching_quote(arg);
		c = *p;
		*p = '\0';
		strcpy(first_arg, arg + 1);
		if (c == '\0')
			return p;
		else
			return p + 1;
	}
	else {
		char *s, *p;

		s = first_arg;
		p = arg;

		while (*p && !isspace(*p) && *p != '"')
			*s++ = *p++;

		*s = '\0';
		return p;
	}

	return arg;
}


int is_substring(char *sub, char *string) {
	char *s;

	if ((s = str_str(string, sub))) {
		int len = strlen(string);
		int sublen = strlen(sub);

		if ((s == string || isspace(*(s - 1)) || ispunct(*(s - 1))) && ((s + sublen == string + len) || isspace(s[sublen]) || ispunct(s[sublen])))
			return 1;
	}

	return 0;
}


/**
* @param char *input The player's command input (without args).
* @param char *match The text to match (one or more words).
* @param int mode CMDTRG_EXACT or CMDTRG_ABBREV.
* @return bool TRUE if the input matches, FALSE if not.
*/
bool match_command_trig(char *input, char *match, bool mode) {
	if (!input || !match) {	// missing input
		return FALSE;
	}
	
	skip_spaces(&match);
	
	if (*match == '*') {	// match anything
		return TRUE;
	}
	if (!strchr(match, ' ')) {	// no spaces? simple match
		if (mode == CMDTRG_EXACT) {
			return !str_cmp(input, match);
		}
		else if (mode == CMDTRG_ABBREV) {
			return (is_abbrev(input, match) && str_cmp(input, match));
		}
	}
	else {	// has spaces (multiple possible words)
		char buffer[MAX_INPUT_LENGTH], word[MAX_INPUT_LENGTH];
		strcpy(buffer, match);
		while (*buffer) {
			half_chop(buffer, word, buffer);
			if ((mode == CMDTRG_EXACT && !str_cmp(input, word)) || (mode == CMDTRG_ABBREV && is_abbrev(input, word) && str_cmp(input, word))) {
				return TRUE;
			}
		}
	}
	
	// no matches
	return FALSE;
}


/*
* return 1 if str contains a word or phrase from wordlist.
* phrases are in double quotes (").
* if wrdlist is NULL, then return 1, if str is NULL, return 0.
*/
int word_check(char *str, char *wordlist) {
	char words[MAX_INPUT_LENGTH], phrase[MAX_INPUT_LENGTH], *s;

	if (*wordlist=='*')
		return 1;

	strcpy(words, wordlist);

	for (s = one_phrase(words, phrase); *phrase; s = one_phrase(s, phrase))
		if (is_substring(phrase, str))
			return 1;

	return 0;
}



/*
*  mob triggers
*/


void greet_memory_mtrigger(char_data *actor) {
	trig_data *t;
	char_data *ch;
	struct script_memory *mem;
	char buf[MAX_INPUT_LENGTH];
	int command_performed = 0;

	if (IS_IMMORTAL(actor) && (GET_INVIS_LEV(actor) > LVL_MORTAL || PRF_FLAGGED(actor, PRF_WIZHIDE))) {
		return;
	}
	if (!valid_dg_target(actor, DG_ALLOW_GODS))
		return;

	for (ch = ROOM_PEOPLE(IN_ROOM(actor)); ch; ch = ch->next_in_room) {
		if (!SCRIPT_MEM(ch) || !AWAKE(ch) || FIGHTING(ch) || (ch == actor)) {
			continue;
		}
		/* find memory line with command only */
		for (mem = SCRIPT_MEM(ch); mem && SCRIPT_MEM(ch); mem=mem->next) {
			if (char_script_id(actor) != mem->id) {
				continue;
			}
			if (mem->cmd) {
				command_interpreter(ch, mem->cmd); /* no script */
				command_performed = 1;
				break;
			}
			/* if a command was not performed execute the memory script */
			if (mem && !command_performed) {
				for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
					if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
						continue;
					}
					if (IS_SET(GET_TRIG_TYPE(t), MTRIG_MEMORY) && CAN_SEE(ch, actor) && !GET_TRIG_DEPTH(t) && number(1, 100) <= GET_TRIG_NARG(t)) {
						union script_driver_data_u sdd;
						ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
						sdd.c = ch;
						script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
						break;
					}
				}
			}
			/* delete the memory */
			if (mem) {
				if (SCRIPT_MEM(ch)==mem) {
					SCRIPT_MEM(ch) = mem->next;
				}
				else {
					struct script_memory *prev;
					prev = SCRIPT_MEM(ch);
					while (prev->next != mem) prev = prev->next;
						prev->next = mem->next;
				}
				if (mem->cmd) free(mem->cmd);
					free(mem);
			}
		}
	}
}


int greet_mtrigger(char_data *actor, int dir) {
	trig_data *t;
	char_data *ch;
	char buf[MAX_INPUT_LENGTH];
	int intermediate, final=TRUE;

	if (IS_IMMORTAL(actor) && (GET_INVIS_LEV(actor) > LVL_MORTAL || PRF_FLAGGED(actor, PRF_WIZHIDE))) {
		return TRUE;
	}
	if (!valid_dg_target(actor, DG_ALLOW_GODS))
		return TRUE;

	for (ch = ROOM_PEOPLE(IN_ROOM(actor)); ch; ch = ch->next_in_room) {
		if (!SCRIPT_CHECK(ch, MTRIG_GREET | MTRIG_GREET_ALL) || (ch == actor)) {
			continue;
		}
		if (!SCRIPT_CHECK(ch, MTRIG_GREET_ALL) && (!AWAKE(ch) || FIGHTING(ch) || AFF_FLAGGED(actor, AFF_SNEAK))) {
			continue;
		}

		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
			if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
				continue;
			}
			if (((IS_SET(GET_TRIG_TYPE(t), MTRIG_GREET) && CAN_SEE(ch, actor)) || IS_SET(GET_TRIG_TYPE(t), MTRIG_GREET_ALL)) && !GET_TRIG_DEPTH(t) && (number(1, 100) <= GET_TRIG_NARG(t))) {
				union script_driver_data_u sdd;
				if (dir>=0 && dir < NUM_OF_DIRS)
					add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[rev_dir[dir]], 0);
				else
					add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				sdd.c = ch;
				intermediate = script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
				if (!intermediate)
					final = FALSE;
				continue;
			}
		}
	}
	return final;
}


void entry_memory_mtrigger(char_data *ch) {
	trig_data *t;
	char_data *actor;
	struct script_memory *mem;
	char buf[MAX_INPUT_LENGTH];

	if (!SCRIPT_MEM(ch))
		return;

	for (actor = ROOM_PEOPLE(IN_ROOM(ch)); actor && SCRIPT_MEM(ch); actor = actor->next_in_room) {
		if (IS_IMMORTAL(actor) && (GET_INVIS_LEV(actor) > LVL_MORTAL || PRF_FLAGGED(actor, PRF_WIZHIDE))) {
			continue;
		}
		if (actor!=ch && SCRIPT_MEM(ch)) {
			for (mem = SCRIPT_MEM(ch); mem && SCRIPT_MEM(ch); mem = mem->next) {
				if (char_script_id(actor) == mem->id) {
					struct script_memory *prev;
					if (mem->cmd) command_interpreter(ch, mem->cmd);
					else {
						for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
							if (TRIGGER_CHECK(t, MTRIG_MEMORY) && (number(1, 100) <= GET_TRIG_NARG(t))){
								union script_driver_data_u sdd;
								ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
								sdd.c = ch;
								script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
								break;
							}
						}
					}
					/* delete the memory */
					if (SCRIPT_MEM(ch)==mem) {
						SCRIPT_MEM(ch) = mem->next;
					}
					else {
						prev = SCRIPT_MEM(ch);
						while (prev->next != mem) prev = prev->next;
							prev->next = mem->next;
					}
					if (mem->cmd) free(mem->cmd);
						free(mem);
				}
			} /* for (mem =..... */
		}
	}
}


int entry_mtrigger(char_data *ch) {
	trig_data *t;

	if (!SCRIPT_CHECK(ch, MTRIG_ENTRY))
		return 1;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
			continue;
		}
		if (TRIGGER_CHECK(t, MTRIG_ENTRY) && (number(1, 100) <= GET_TRIG_NARG(t))){
			union script_driver_data_u sdd;
			sdd.c = ch;
			return script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
			break;
		}
	}

	return 1;
}


/**
* Buy trigger (mob): fires when someone is about to buy.
*
* @param char_data *actor The person trying to buy.
* @param char_data *shopkeeper The mob shopkeeper, if any (many shops have none).
* @param obj_data *buying The item being bought.
* @param int cost The amount to be charged.
* @param any_vnum currency The currency type (NOTHING for coins).
* @return int 0 if a trigger blocked the buy (stop); 1 if not (ok to continue).
*/
int buy_mtrigger(char_data *actor, char_data *shopkeeper, obj_data *buying, int cost, any_vnum currency) {
	union script_driver_data_u sdd;
	char buf[MAX_INPUT_LENGTH];
	char_data *ch, *ch_next;
	trig_data *t;
	
	// gods not affected
	if (!valid_dg_target(actor, DG_ALLOW_GODS)) {
		return 1;
	}
	
	LL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(actor)), ch, ch_next, next_in_room) {
		if (!SCRIPT_CHECK(ch, MTRIG_BUY)) {
			continue;
		}
		if (actor == ch) {
			continue;
		}
		
		LL_FOREACH(TRIGGERS(SCRIPT(ch)), t) {
			if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
				continue;
			}
			if (!TRIGGER_CHECK(t, MTRIG_BUY)) {
				continue;
			}
			
			// vars
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			ADD_UID_VAR(buf, t, obj_script_id(buying), "obj", 0);
			if (shopkeeper) {
				ADD_UID_VAR(buf, t, char_script_id(shopkeeper), "shopkeeper", 0);
			}
			else {
				add_var(&GET_TRIG_VARS(t), "shopkeeper", "", 0);
			}
			
			snprintf(buf, sizeof(buf), "%d", cost);
			add_var(&GET_TRIG_VARS(t), "cost", buf, 0);
			
			if (currency == NOTHING) {
				strcpy(buf, "coins");
			}
			else {
				snprintf(buf, sizeof(buf), "%d", currency);
			}
			add_var(&GET_TRIG_VARS(t), "currency", buf, 0);
			
			// run it:
			sdd.c = ch;
			if (!script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW)) {
				return 0;
			}
		}
	}

	return 1;
}


/**
* Command trigger (mob).
*
* @param char_data *actor The person typing a command.
* @param char *cmd The command as-typed (first word).
* @param char *argument Any arguments (remaining text).
* @param int mode CMDTRG_EXACT or CMDTRG_ABBREV.
* @return int 1 if a trigger ran (stop); 0 if not (ok to continue).
*/
int command_mtrigger(char_data *actor, char *cmd, char *argument, int mode) {
	char_data *ch, *ch_next;
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	/* prevent people we like from becoming trapped :P */
	if (!valid_dg_target(actor, 0))
		return 0;

	for (ch = ROOM_PEOPLE(IN_ROOM(actor)); ch; ch = ch_next) {
		ch_next = ch->next_in_room;

		if (SCRIPT_CHECK(ch, MTRIG_COMMAND) && (actor != ch || !AFF_FLAGGED(ch, AFF_ORDERED))) {
			for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
				if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
					continue;
				}
				if (!TRIGGER_CHECK(t, MTRIG_COMMAND))
					continue;

				if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
					syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: Command Trigger #%d has no text argument!", GET_TRIG_VNUM(t));
					continue;
				}
				
				if (match_command_trig(cmd, GET_TRIG_ARG(t), mode)) {
					union script_driver_data_u sdd;
					ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
					skip_spaces(&argument);
					add_var(&GET_TRIG_VARS(t), "arg", argument, 0);
					skip_spaces(&cmd);
					add_var(&GET_TRIG_VARS(t), "cmd", cmd, 0);
					sdd.c = ch;

					if (script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW))
						return 1;
				}
			}
		}
	}

	return 0;
}


void speech_mtrigger(char_data *actor, char *str) {
	char_data *ch, *ch_next;
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	for (ch = ROOM_PEOPLE(IN_ROOM(actor)); ch; ch = ch_next) {
		ch_next = ch->next_in_room;

		if (SCRIPT_CHECK(ch, MTRIG_SPEECH) && AWAKE(ch) && (actor!=ch)) {
			for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
				if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
					continue;
				}
				if (!TRIGGER_CHECK(t, MTRIG_SPEECH))
					continue;

				if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
					syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: Speech Trigger #%d has no text argument!", GET_TRIG_VNUM(t));
					continue;
				}

				if (*GET_TRIG_ARG(t) == '*' || ((GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) || (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str)))) {
					union script_driver_data_u sdd;
					ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
					add_var(&GET_TRIG_VARS(t), "speech", str, 0);
					sdd.c = ch;
					script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
					break;
				}
			}
		}
	}
}


void act_mtrigger(const char_data *ch, char *str, char_data *actor, char_data *victim, obj_data *object, obj_data *target, char *arg) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (SCRIPT_CHECK(ch, MTRIG_ACT) && (actor!=ch)) {
		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next)  {
			if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
				continue;
			}
			if (!TRIGGER_CHECK(t, MTRIG_ACT))
				continue;

			if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
				syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: Act Trigger #%d has no text argument!", GET_TRIG_VNUM(t));
				continue;
			}

			if (((GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) || (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str)))) {
				union script_driver_data_u sdd;

				if (actor)
					ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				if (victim)
					ADD_UID_VAR(buf, t, char_script_id(victim), "victim", 0);
				if (object)
					ADD_UID_VAR(buf, t, obj_script_id(object), "object", 0);
				if (target)
					ADD_UID_VAR(buf, t, obj_script_id(target), "target", 0);
				if (str) {
					/* we're guaranteed to have a string ending with \r\n\0 */
					char *nstr = strdup(str), *fstr = nstr, *p = strchr(nstr, '\r');
					skip_spaces(&nstr);
					*p = '\0';
					add_var(&GET_TRIG_VARS(t), "arg", nstr, 0);
					free(fstr);
				}	  
				sdd.c = (char_data*)ch;
				script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
				break;
			}	
		}
	}
}


void fight_mtrigger(char_data *ch) {
	char_data *actor;
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (!SCRIPT_CHECK(ch, MTRIG_FIGHT) || !FIGHTING(ch))
		return;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
			continue;
		}
		if (TRIGGER_CHECK(t, MTRIG_FIGHT) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;

			actor = FIGHTING(ch);
			if (actor)
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			else
				add_var(&GET_TRIG_VARS(t), "actor", "nobody", 0);

			sdd.c = ch;
			script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
			break;
		}
	}
}


void hitprcnt_mtrigger(char_data *ch) {
	char_data *actor;
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (!SCRIPT_CHECK(ch, MTRIG_HITPRCNT) || !FIGHTING(ch))
		return;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
			continue;
		}
		if (TRIGGER_CHECK(t, MTRIG_HITPRCNT) && GET_MAX_HEALTH(ch) && (((GET_HEALTH(ch) * 100) / MAX(1, GET_MAX_HEALTH(ch))) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;

			actor = FIGHTING(ch);
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.c = ch;
			script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
			break;
		}
	}
}


int receive_mtrigger(char_data *ch, char_data *actor, obj_data *obj) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int ret_val;

	if (!SCRIPT_CHECK(ch, MTRIG_RECEIVE))
		return 1;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
			continue;
		}
		if (TRIGGER_CHECK(t, MTRIG_RECEIVE) && (number(1, 100) <= GET_TRIG_NARG(t))){
			union script_driver_data_u sdd;

			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			ADD_UID_VAR(buf, t, obj_script_id(obj), "object", 0);
			sdd.c = ch;
			ret_val = script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
			if (EXTRACTED(actor) || EXTRACTED(ch) || IS_DEAD(actor) || IS_DEAD(ch) || obj->carried_by != actor)
				return 0;
			else
				return ret_val;
		}
	}

	return 1;
}


int death_mtrigger(char_data *ch, char_data *actor) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (!SCRIPT_CHECK(ch, MTRIG_DEATH))
	return 1;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
			continue;
		}
		if (TRIGGER_CHECK(t, MTRIG_DEATH) && (number(1, 100) <= GET_TRIG_NARG(t))){
			union script_driver_data_u sdd;

			if (actor && actor != ch)
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.c = ch;
			return script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}


// returns 0 to block the bribe, 1 to allow it
int bribe_mtrigger(char_data *ch, char_data *actor, int amount) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int ret_val;

	if (!SCRIPT_CHECK(ch, MTRIG_BRIBE))
		return 1;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
			continue;
		}
		if (TRIGGER_CHECK(t, MTRIG_BRIBE) && (amount >= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			sdd.c = ch;
			snprintf(buf, sizeof(buf), "%d", amount);
			add_var(&GET_TRIG_VARS(t), "amount", buf, 0);
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			ret_val = script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
			
			if (EXTRACTED(actor) || EXTRACTED(ch) || IS_DEAD(actor) || IS_DEAD(ch)) {
				return 0;
			}
			else {
				return ret_val;
			}
		}
	}
	
	return 1;
}


void load_mtrigger(char_data *ch) {
	trig_data *t;

	if (!SCRIPT_CHECK(ch, MTRIG_LOAD))
		return;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (TRIGGER_CHECK(t, MTRIG_LOAD) &&  (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			sdd.c = ch;
			script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
			break;
		}
	}
}


int ability_mtrigger(char_data *actor, char_data *ch, any_vnum abil) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	ability_data *ab;

	if (ch == NULL || !(ab = find_ability_by_vnum(abil))) {
		return 1;
	}

	if (!SCRIPT_CHECK(ch, MTRIG_ABILITY))
		return 1;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
			continue;
		}
		if (TRIGGER_CHECK(t, MTRIG_ABILITY) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;

			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sprintf(buf, "%d", abil);
			add_var(&GET_TRIG_VARS(t), "ability", buf, 0);
			add_var(&GET_TRIG_VARS(t), "abilityname", ABIL_NAME(ab), 0);
			sdd.c = ch;
			return script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}


/**
* @param char_data *actor The person trying to leave.
* @param int dir The direction they are trying to go (passed through to %direction%).
* @param char *custom_dir Optional: A different value for %direction% (may be NULL).
* @return int 0 = block the leave, 1 = pass
*/
int leave_mtrigger(char_data *actor, int dir, char *custom_dir) {
	trig_data *t;
	char_data *ch;
	char buf[MAX_INPUT_LENGTH];
	
	if (IS_IMMORTAL(actor) && (GET_INVIS_LEV(actor) > LVL_MORTAL || PRF_FLAGGED(actor, PRF_WIZHIDE))) {
		return 1;
	}
	
	for (ch = ROOM_PEOPLE(IN_ROOM(actor)); ch; ch = ch->next_in_room) {
		if (!SCRIPT_CHECK(ch, MTRIG_LEAVE | MTRIG_LEAVE_ALL) || (ch == actor)) {
			continue;
		}
		if (!SCRIPT_CHECK(ch, MTRIG_LEAVE_ALL) && (!AWAKE(ch) || FIGHTING(ch))) {
			continue;
		}

		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
			if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
				continue;
			}
			if (((IS_SET(GET_TRIG_TYPE(t), MTRIG_LEAVE) && CAN_SEE(ch, actor)) || IS_SET(GET_TRIG_TYPE(t), MTRIG_LEAVE_ALL)) && !GET_TRIG_DEPTH(t) && (number(1, 100) <= GET_TRIG_NARG(t))) {
				union script_driver_data_u sdd;
				if (dir>=0 && dir < NUM_OF_DIRS)
					add_var(&GET_TRIG_VARS(t), "direction", custom_dir ? custom_dir : (char *)dirs[dir], 0);
				else
					add_var(&GET_TRIG_VARS(t), "direction", custom_dir ? custom_dir : "none", 0);
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				sdd.c = ch;
				return script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
			}
		}
	}
	return 1;
}


int door_mtrigger(char_data *actor, int subcmd, int dir) {
	trig_data *t;
	char_data *ch;
	char buf[MAX_INPUT_LENGTH];

	for (ch = ROOM_PEOPLE(IN_ROOM(actor)); ch; ch = ch->next_in_room) {
		if (!SCRIPT_CHECK(ch, MTRIG_DOOR) || !AWAKE(ch) || FIGHTING(ch) || (ch == actor))
			continue;

		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
			if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
				continue;
			}
			if (IS_SET(GET_TRIG_TYPE(t), MTRIG_DOOR) && CAN_SEE(ch, actor) && !GET_TRIG_DEPTH(t) && (number(1, 100) <= GET_TRIG_NARG(t))) {
				union script_driver_data_u sdd;
				add_var(&GET_TRIG_VARS(t), "cmd", (char *)cmd_door[subcmd], 0);
				if (dir>=0 && dir < NUM_OF_DIRS)
					add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[dir], 0);
				else
					add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				sdd.c = ch;
				return script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
			}
		}
	}
	return 1;
}


void reboot_mtrigger(char_data *ch) {
	trig_data *t;
	int val;

	if (!SCRIPT_CHECK(ch, MTRIG_REBOOT)) {
		return;
	}

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (TRIGGER_CHECK(t, MTRIG_REBOOT)) {
			union script_driver_data_u sdd;
			sdd.c = ch;
			val = script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
			if (!val) {
				break;
			}
		}
	}
}


/*
*  object triggers
*/


// returns 0 if obj was purged
int timer_otrigger(obj_data *obj) {
	trig_data *t;
	int return_val;

	if (!SCRIPT_CHECK(obj, OTRIG_TIMER))
		return 1;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_TIMER)) {
			union script_driver_data_u sdd;
			sdd.o = obj;
			return_val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			/* don't allow a wear to take place, if
			* the object is purged.
			*/
			if (!obj || !return_val) {
				return 0;
			}
		}
	}  

	return 1;
}


int get_otrigger(obj_data *obj, char_data *actor) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int ret_val;
	if (!SCRIPT_CHECK(obj, OTRIG_GET))
		return 1;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_GET) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.o = obj;
			ret_val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			/* don't allow a get to take place, if
			* a) the actor is killed (the mud would choke on obj_to_char).
			* b) the object is purged.
			*/
			if (EXTRACTED(actor) || IS_DEAD(actor) || !obj)
				return 0;
			else
				return ret_val;
		}
	}

	return 1;
}


/**
* Buy trigger (obj) sub-processor.
*
* @param obj_data *obj The item to check.
* @param char_data *actor The person trying to buy.
* @param char_data *shopkeeper The mob shopkeeper, if any (many shops have none).
* @param obj_data *buying The item being bought.
* @param int cost The amount to be charged.
* @param any_vnum currency The currency type (NOTHING for coins).
* @param int type Location: OCMD_EQUIP, etc.
* @return int 0 if a trigger blocked the buy (stop); 1 if not (ok to continue).
*/
int buy_otrig(obj_data *obj, char_data *actor, char_data *shopkeeper, obj_data *buying, int cost, any_vnum currency, int type) {
	union script_driver_data_u sdd;
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (!obj || !SCRIPT_CHECK(obj, OTRIG_BUY)) {
		return 1;
	}
	
	LL_FOREACH(TRIGGERS(SCRIPT(obj)), t) {
		if (!TRIGGER_CHECK(t, OTRIG_BUY)) {
			continue;	// not a buy trigger
		}
		if (!IS_SET(GET_TRIG_NARG(t), type)) {
			continue;	// bad location
		}

		// vars
		ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
		ADD_UID_VAR(buf, t, obj_script_id(buying), "obj", 0);
		if (shopkeeper) {
			ADD_UID_VAR(buf, t, char_script_id(shopkeeper), "shopkeeper", 0);
		}
			else {
				add_var(&GET_TRIG_VARS(t), "shopkeeper", "", 0);
			}
		
		snprintf(buf, sizeof(buf), "%d", cost);
		add_var(&GET_TRIG_VARS(t), "cost", buf, 0);
		
		if (currency == NOTHING) {
			strcpy(buf, "coins");
		}
		else {
			snprintf(buf, sizeof(buf), "%d", currency);
		}
		add_var(&GET_TRIG_VARS(t), "currency", buf, 0);
		
		// run it
		sdd.o = obj;
		if (!script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW)) {
			return 0;
		}
	}

	return 1;
}


/**
* Buy trigger (obj): fires before someone buys something.
*
* @param char_data *actor The person trying to buy.
* @param char_data *shopkeeper The mob shopkeeper, if any (many shops have none).
* @param obj_data *buying The item being bought.
* @param int cost The amount to be charged.
* @param any_vnum currency The currency type (NOTHING for coins).
* @return int 0 if a trigger blocked the buy (stop); 1 if not (ok to continue).
*/
int buy_otrigger(char_data *actor, char_data *shopkeeper, obj_data *buying, int cost, any_vnum currency) {
	obj_data *obj;
	int iter;
	
	// gods not affected
	if (!valid_dg_target(actor, DG_ALLOW_GODS)) {
		return 1;
	}
	
	for (iter = 0; iter < NUM_WEARS; iter++) {
		if (!buy_otrig(GET_EQ(actor, iter), actor, shopkeeper, buying, cost, currency, OCMD_EQUIP)) {
			return 0;
		}
	}
	
	LL_FOREACH2(actor->carrying, obj, next_content) {
		if (!buy_otrig(obj, actor, shopkeeper, buying, cost, currency, OCMD_INVEN)) {
			return 0;
		}
	}

	LL_FOREACH2(ROOM_CONTENTS(IN_ROOM(actor)), obj, next_content) {
		if (!buy_otrig(obj, actor, shopkeeper, buying, cost, currency, OCMD_ROOM)) {
			return 0;
		}
	}

	return 1;
}


/**
* Command trigger (obj) sub-processor.
*
* @param obj_data *obj The item to check.
* @param char_data *actor The person typing a command.
* @param char *cmd The command as-typed (first word).
* @param char *argument Any arguments (remaining text).
* @param int type Location: OCMD_EQUIP, etc.
* @param int mode CMDTRG_EXACT or CMDTRG_ABBREV.
* @return int 1 if a trigger ran (stop); 0 if not (ok to continue).
*/
int cmd_otrig(obj_data *obj, char_data *actor, char *cmd, char *argument, int type, int mode) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (obj && SCRIPT_CHECK(obj, OTRIG_COMMAND)) {
		for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
			// not a command trigger
			if (!TRIGGER_CHECK(t, OTRIG_COMMAND))
				continue;
			
			// bad location
			if (!IS_SET(GET_TRIG_NARG(t), type)) {
				continue;
			}

			if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
				syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: O-Command Trigger #%d has no text argument!", GET_TRIG_VNUM(t));
				continue;
			}

			if (match_command_trig(cmd, GET_TRIG_ARG(t), mode)) {
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				skip_spaces(&argument);
				add_var(&GET_TRIG_VARS(t), "arg", argument, 0);
				skip_spaces(&cmd);
				add_var(&GET_TRIG_VARS(t), "cmd", cmd, 0);

				union script_driver_data_u sdd;
				sdd.o = obj;
				if (script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW))
					return 1;
			}
		}
	}

	return 0;
}


/**
* Command trigger (obj).
*
* @param char_data *actor The person typing a command.
* @param char *cmd The command as-typed (first word).
* @param char *argument Any arguments (remaining text).
* @param int mode CMDTRG_EXACT or CMDTRG_ABBREV.
* @return int 1 if a trigger ran (stop); 0 if not (ok to continue).
*/
int command_otrigger(char_data *actor, char *cmd, char *argument, int mode) {
	obj_data *obj;
	int i;

	/* prevent people we like from becoming trapped :P */
	if (!valid_dg_target(actor, 0))
		return 0;

	for (i = 0; i < NUM_WEARS; i++)
		if (cmd_otrig(GET_EQ(actor, i), actor, cmd, argument, OCMD_EQUIP, mode))
			return 1;

	for (obj = actor->carrying; obj; obj = obj->next_content)
		if (cmd_otrig(obj, actor, cmd, argument, OCMD_INVEN, mode))
			return 1;

	for (obj = ROOM_CONTENTS(IN_ROOM(actor)); obj; obj = obj->next_content)
		if (cmd_otrig(obj, actor, cmd, argument, OCMD_ROOM, mode))
			return 1;

	return 0;
}


int wear_otrigger(obj_data *obj, char_data *actor, int where) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int ret_val;

	if (!SCRIPT_CHECK(obj, OTRIG_WEAR))
		return 1;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_WEAR)) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.o = obj;
			ret_val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			/* don't allow a wear to take place, if
			* the object is purged.
			*/
			if (!obj)
				return 0;
			else
				return ret_val;
		}
	}

	return 1;
}


int remove_otrigger(obj_data *obj, char_data *actor) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int ret_val;

	if (!SCRIPT_CHECK(obj, OTRIG_REMOVE))
	return 1;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_REMOVE)) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.o = obj;
			ret_val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			/* don't allow a remove to take place, if
			* the object is purged.
			*/
			if (!obj)
				return 0;
			else
				return ret_val;
		}
	}

	return 1;
}


/**
* @param obj_data *obj The object being dropped/put/etc.
* @param char_data *actor The person doing.
* @param int mode Any DROP_TRIG_ type.
*/
int drop_otrigger(obj_data *obj, char_data *actor, int mode) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int ret_val;

	if (!SCRIPT_CHECK(obj, OTRIG_DROP))
		return 1;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_DROP) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			
			switch (mode) {
				case DROP_TRIG_DROP: {
					add_var(&GET_TRIG_VARS(t), "command", "drop", 0);
					break;
				}
				case DROP_TRIG_JUNK: {
					add_var(&GET_TRIG_VARS(t), "command", "junk", 0);
					break;
				}
				case DROP_TRIG_PUT: {
					add_var(&GET_TRIG_VARS(t), "command", "put", 0);
					break;
				}
				case DROP_TRIG_SACRIFICE: {
					add_var(&GET_TRIG_VARS(t), "command", "sacrifice", 0);
					break;
				}
				default: {
					add_var(&GET_TRIG_VARS(t), "command", "unknown", 0);
					break;
				}
			}
			
			sdd.o = obj;
			ret_val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			/* don't allow a drop to take place, if
			* the object is purged (nothing to drop).
			*/
			if (!obj)
				return 0;
			else
				return ret_val;
		}
	}

	return 1;
}


int give_otrigger(obj_data *obj, char_data *actor, char_data *victim) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int ret_val;

	if (!SCRIPT_CHECK(obj, OTRIG_GIVE))
	return 1;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_GIVE) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			ADD_UID_VAR(buf, t, char_script_id(victim), "victim", 0);
			sdd.o = obj;
			ret_val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			/* don't allow a give to take place, if
			* a) the object is purged.
			* b) the object is not carried by the giver.
			*/
			if (!obj || obj->carried_by != actor)
				return 0;
			else
				return ret_val;
		}
	}

	return 1;
}

void load_otrigger(obj_data *obj) {
	trig_data *t;

	if (!SCRIPT_CHECK(obj, OTRIG_LOAD))
		return;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_LOAD) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			sdd.o = obj;
			script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			break;
		}
	}
}

int ability_otrigger(char_data *actor, obj_data *obj, any_vnum abil) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	ability_data *ab;

	if (obj == NULL || !(ab = find_ability_by_vnum(abil)))
		return 1;

	if (!SCRIPT_CHECK(obj, OTRIG_ABILITY))
		return 1;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_ABILITY) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;

			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sprintf(buf, "%d", abil);
			add_var(&GET_TRIG_VARS(t), "ability", buf, 0);
			add_var(&GET_TRIG_VARS(t), "abilityname", ABIL_NAME(ab), 0);
			sdd.o = obj;
			return script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
		}
	}

return 1;
}


/**
* @param room_data *room The room the person is trying to leave.
* @param char_data *actor The person trying to leave.
* @param int dir The direction they are trying to go (passed through to %direction%).
* @param char *custom_dir Optional: A different value for %direction% (may be NULL).
* @return int 0 = block the leave, 1 = pass
*/
int leave_otrigger(room_data *room, char_data *actor, int dir, char *custom_dir) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int temp, final = 1;
	obj_data *obj, *obj_next;
	
	if (IS_IMMORTAL(actor) && (GET_INVIS_LEV(actor) > LVL_MORTAL || PRF_FLAGGED(actor, PRF_WIZHIDE))) {
		return 1;
	}
	
	for (obj = room->contents; obj; obj = obj_next) {
		obj_next = obj->next_content;
		if (!SCRIPT_CHECK(obj, OTRIG_LEAVE))
			continue;

		for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
			if (TRIGGER_CHECK(t, OTRIG_LEAVE) && (number(1, 100) <= GET_TRIG_NARG(t))) {
				union script_driver_data_u sdd;
				if (dir>=0 && dir < NUM_OF_DIRS)
					add_var(&GET_TRIG_VARS(t), "direction", custom_dir ? custom_dir : (char *)dirs[dir], 0);
				else
					add_var(&GET_TRIG_VARS(t), "direction", custom_dir ? custom_dir : "none", 0);
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				sdd.o = obj;
				temp = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
				obj = sdd.o;
				if (temp == 0)
					final = 0;
			}
		}
	}

	return final;
}


/**
* A trigger that fires when a character tries to 'consume' an object. Most
* consume triggers can block the action by returning 0, but a few (poisons and
* shooting) cannot block.
*
* @param obj_data *obj The item to test for triggers.
* @param char_data *actor The player consuming the object.
* @param int cmd The command that's consuming the item (OCMD_*).
* @param char_data *target Optional: If the consume is targeted (e.g. poisons), the target (may be NULL).
* @return int 0 to block consume, 1 to continue.
*/
int consume_otrigger(obj_data *obj, char_data *actor, int cmd, char_data *target) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int ret_val;

	if (!SCRIPT_CHECK(obj, OTRIG_CONSUME))
		return 1;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_CONSUME)) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			if (target) {
				ADD_UID_VAR(buf, t, char_script_id(target), "target", 0);
			}
			else {
				add_var(&GET_TRIG_VARS(t), "target", "", 0);
			}
			
			switch (cmd) {
				case OCMD_EAT:
					add_var(&GET_TRIG_VARS(t), "command", "eat", 0);
					break;
				case OCMD_DRINK:
					add_var(&GET_TRIG_VARS(t), "command", "drink", 0);
					break;
				case OCMD_QUAFF:
					add_var(&GET_TRIG_VARS(t), "command", "quaff", 0);
					break;
				case OCMD_READ: {
					add_var(&GET_TRIG_VARS(t), "command", "read", 0);
					break;
				}
				case OCMD_BUILD: {
					add_var(&GET_TRIG_VARS(t), "command", "build", 0);
					break;
				}
				case OCMD_CRAFT: {
					add_var(&GET_TRIG_VARS(t), "command", "craft", 0);
					break;
				}
				case OCMD_SHOOT: {
					add_var(&GET_TRIG_VARS(t), "command", "shoot", 0);
					break;
				}
				case OCMD_POISON: {
					add_var(&GET_TRIG_VARS(t), "command", "poison", 0);
					break;
				}
				case OCMD_PAINT: {
					add_var(&GET_TRIG_VARS(t), "command", "paint", 0);
					break;
				}
				case OCMD_LIGHT: {
					add_var(&GET_TRIG_VARS(t), "command", "light", 0);
					break;
				}
			}
			sdd.o = obj;
			ret_val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			/* don't allow a wear to take place, if
			* the object is purged.
			*/
			if (!obj)
				return 0;
			else
				return ret_val;
		}
	}

	return 1;
}


/**
* Called when a character is finished with an object, e.g. reading a book.
*
* @param obj_data *obj The object being finished.
* @param char_data *actor The person finishing it.
* @return int 1 to proceed, 0 if a script returned 0 or the obj was purged.
*/
int finish_otrigger(obj_data *obj, char_data *actor) {
	char buf[MAX_INPUT_LENGTH];
	int val = TRUE;
	trig_data *t;

	if (!SCRIPT_CHECK(obj, OTRIG_FINISH)) {
		return 1;
	}

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_FINISH) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			sdd.o = obj;
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			if (!val || !obj) {
				break;
			}
		}
	}
	
	return (val && obj) ? 1 : 0;
}


void reboot_otrigger(obj_data *obj) {
	trig_data *t;
	int val;

	if (!SCRIPT_CHECK(obj, OTRIG_REBOOT)) {
		return;
	}

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_REBOOT)) {
			union script_driver_data_u sdd;
			sdd.o = obj;
			val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			if (!val) {
				break;
			}
		}
	}
}


/*
*  world triggers
*/

void adventure_cleanup_wtrigger(room_data *room) {
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (!SCRIPT_CHECK(room, WTRIG_ADVENTURE_CLEANUP))
		return;

	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_ADVENTURE_CLEANUP) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			sdd.r = room;
			if (!script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW)) {
				break;
			}
		}
	}
}


/**
* Called when a building is completed.
*
* @param room_data *room The room.
*/
void complete_wtrigger(room_data *room) {
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (!SCRIPT_CHECK(room, WTRIG_COMPLETE))
		return;

	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_COMPLETE) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			sdd.r = room;
			if (script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW)) {
				break;
			}
		}
	}
}


/**
* Called when a player attempts to dismantle or redesignate a room. Returning
* a 0 from the script will prevent the dismantle if possible.
*
* NOT preventable if a player is dismantling a building, but this trigger is
* firing on some interior room (e.g. a study).
*
* It's also possible for there to be NO actor if a building is being destroyed
* by something other than a player.
*
* @param room_data *room The room attempting to dismantle.
* @param char_data *actor Optional: The player attempting the dismantle (may be NULL).
* @param bool preventable If TRUE, returning 0 will prevent the dismantle.
* @return int The exit code from the script.
*/
int dismantle_wtrigger(room_data *room, char_data *actor, bool preventable) {
	char buf[MAX_INPUT_LENGTH];
	trig_data *trig;
	int one, value = 1;
	
	if (!SCRIPT_CHECK(room, WTRIG_DISMANTLE))
		return 1;
	
	LL_FOREACH(TRIGGERS(SCRIPT(room)), trig) {
		if (TRIGGER_CHECK(trig, WTRIG_DISMANTLE)) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, trig, room_script_id(room), "room", 0);
			add_var(&GET_TRIG_VARS(trig), "preventable", preventable ? "1" : "0", 0);
			if (actor) {
				ADD_UID_VAR(buf, trig, char_script_id(actor), "actor", 0);
			}
			else {
				add_var(&GET_TRIG_VARS(trig), "actor", "", 0);
			}
			sdd.r = room;
			one = script_driver(&sdd, trig, WLD_TRIGGER, TRIG_NEW);
			value = MIN(value, one);	// can be set to 0
		}
	}

	return value;
}


/**
* Called when a building first loads.
*
* @param room_data *room The room.
*/
void load_wtrigger(room_data *room) {
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (!SCRIPT_CHECK(room, WTRIG_LOAD))
		return;

	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_LOAD) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			sdd.r = room;
			if (script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW)) {
				break;
			}
		}
	}
}


void reset_wtrigger(room_data *room) {
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (!SCRIPT_CHECK(room, WTRIG_RESET))
		return;

	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_RESET) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			sdd.r = room;
			script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
			break;
		}
	}
}


int enter_wtrigger(room_data *room, char_data *actor, int dir) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	
	if (IS_IMMORTAL(actor) && (GET_INVIS_LEV(actor) > LVL_MORTAL || PRF_FLAGGED(actor, PRF_WIZHIDE))) {
		return TRUE;
	}

	if (!SCRIPT_CHECK(room, WTRIG_ENTER))
		return 1;

	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_ENTER) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			if (dir>=0 && dir < NUM_OF_DIRS)
				add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[rev_dir[dir]], 0);
			else
				add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.r = room;
			return script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}


/**
* Buy trigger (room): fires when someone tries to buy
*
* @param char_data *actor The person trying to buy.
* @param char_data *shopkeeper The mob shopkeeper, if any (many shops have none).
* @param obj_data *buying The item being bought.
* @param int cost The amount to be charged.
* @param any_vnum currency The currency type (NOTHING for coins).
* @return int 0 if a trigger blocked the buy (stop); 1 if not (ok to continue).
*/
int buy_wtrigger(char_data *actor, char_data *shopkeeper, obj_data *buying, int cost, any_vnum currency) {
	room_data *room = IN_ROOM(actor);
	union script_driver_data_u sdd;
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (!SCRIPT_CHECK(IN_ROOM(actor), WTRIG_BUY)) {
		return 1;
	}
	if (!valid_dg_target(actor, DG_ALLOW_GODS)) {
		return 1;	// gods not affected
	}
	
	LL_FOREACH(TRIGGERS(SCRIPT(room)), t) {
		if (!TRIGGER_CHECK(t, WTRIG_BUY)) {
			continue;
		}
		
		// vars
		ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
		ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
		ADD_UID_VAR(buf, t, obj_script_id(buying), "obj", 0);
		if (shopkeeper) {
			ADD_UID_VAR(buf, t, char_script_id(shopkeeper), "shopkeeper", 0);
		}
			else {
				add_var(&GET_TRIG_VARS(t), "shopkeeper", "", 0);
			}
		
		snprintf(buf, sizeof(buf), "%d", cost);
		add_var(&GET_TRIG_VARS(t), "cost", buf, 0);
		
		if (currency == NOTHING) {
			strcpy(buf, "coins");
		}
		else {
			snprintf(buf, sizeof(buf), "%d", currency);
		}
		add_var(&GET_TRIG_VARS(t), "currency", buf, 0);
		
		// run it
		sdd.r = room;
		if (!script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW)) {
			return 0;
		}
	}
	
	return 1;
}


/**
* Command trigger (room).
*
* @param char_data *actor The person typing a command.
* @param char *cmd The command as-typed (first word).
* @param char *argument Any arguments (remaining text).
* @param int mode CMDTRG_EXACT or CMDTRG_ABBREV.
* @return int 1 if a trigger ran (stop); 0 if not (ok to continue).
*/
int command_wtrigger(char_data *actor, char *cmd, char *argument, int mode) {
	room_data *room;
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (!actor || !SCRIPT_CHECK(IN_ROOM(actor), WTRIG_COMMAND))
		return 0;

	/* prevent people we like from becoming trapped :P */
	if (!valid_dg_target(actor, 0))
		return 0;

	room = IN_ROOM(actor);
	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (!TRIGGER_CHECK(t, WTRIG_COMMAND))
			continue;

		if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
			syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: W-Command Trigger #%d has no text argument!", GET_TRIG_VNUM(t));
			continue;
		}

		if (match_command_trig(cmd, GET_TRIG_ARG(t), mode)) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			skip_spaces(&argument);
			add_var(&GET_TRIG_VARS(t), "arg", argument, 0);
			skip_spaces(&cmd);
			add_var(&GET_TRIG_VARS(t), "cmd", cmd, 0);

			sdd.r = room;
			return script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
		}
	}

	return 0;
}


void speech_wtrigger(char_data *actor, char *str) {
	room_data *room;
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (!actor || !SCRIPT_CHECK(IN_ROOM(actor), WTRIG_SPEECH))
		return;

	room = IN_ROOM(actor);
	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (!TRIGGER_CHECK(t, WTRIG_SPEECH))
			continue;

		if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
			syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: W-Speech Trigger #%d has no text argument!", GET_TRIG_VNUM(t));
			continue;
		}

		if (*GET_TRIG_ARG(t)=='*' || (GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) || (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			add_var(&GET_TRIG_VARS(t), "speech", str, 0);
			sdd.r = room;
			script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
			break;
		}
	}
}


/**
* @param obj_data *obj The object being dropped/put/etc.
* @param char_data *actor The person doing.
* @param int mode Any DROP_TRIG_ type.
*/
int drop_wtrigger(obj_data *obj, char_data *actor, int mode) {
	room_data *room;
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int ret_val;

	if (!actor || !SCRIPT_CHECK(IN_ROOM(actor), WTRIG_DROP))
		return 1;

	room = IN_ROOM(actor);
	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) 
		if (TRIGGER_CHECK(t, WTRIG_DROP) && (number(1, 100) <= GET_TRIG_NARG(t))) {	
			union script_driver_data_u sdd;

			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			ADD_UID_VAR(buf, t, obj_script_id(obj), "object", 0);
			
			switch (mode) {
				case DROP_TRIG_DROP: {
					add_var(&GET_TRIG_VARS(t), "command", "drop", 0);
					break;
				}
				case DROP_TRIG_JUNK: {
					add_var(&GET_TRIG_VARS(t), "command", "junk", 0);
					break;
				}
				case DROP_TRIG_PUT: {
					add_var(&GET_TRIG_VARS(t), "command", "put", 0);
					break;
				}
				case DROP_TRIG_SACRIFICE: {
					add_var(&GET_TRIG_VARS(t), "command", "sacrifice", 0);
					break;
				}
				default: {
					add_var(&GET_TRIG_VARS(t), "command", "unknown", 0);
					break;
				}
			}
			
			sdd.r = room;
			ret_val = script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
			if (ret_val && obj->carried_by != actor)
				return 0;
			else
				return ret_val;
			break;
		}

	return 1;
}

int ability_wtrigger(char_data *actor, char_data *vict, obj_data *obj, any_vnum abil) {
	room_data *room;
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	ability_data *ab;

	if (!actor || !(ab = find_ability_by_vnum(abil)) || !SCRIPT_CHECK(IN_ROOM(actor), WTRIG_ABILITY))
		return 1;

	room = IN_ROOM(actor);
	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_ABILITY) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;

			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			if (vict)
				ADD_UID_VAR(buf, t, char_script_id(vict), "victim", 0);
			if (obj)
				ADD_UID_VAR(buf, t, obj_script_id(obj), "object", 0);
			sprintf(buf, "%d", abil);
			add_var(&GET_TRIG_VARS(t), "ability", buf, 0);
			add_var(&GET_TRIG_VARS(t), "abilityname", ABIL_NAME(ab), 0);
			sdd.r = room;
			return script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}


/**
* @param room_data *room The room the person is trying to leave.
* @param char_data *actor The person trying to leave.
* @param int dir The direction they are trying to go (passed through to %direction%).
* @param char *custom_dir Optional: A different value for %direction% (may be NULL).
* @return int 0 = block the leave, 1 = pass
*/
int leave_wtrigger(room_data *room, char_data *actor, int dir, char *custom_dir) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (!SCRIPT_CHECK(room, WTRIG_LEAVE))
		return 1;
	if (IS_IMMORTAL(actor) && (GET_INVIS_LEV(actor) > LVL_MORTAL || PRF_FLAGGED(actor, PRF_WIZHIDE))) {
		return 1;
	}
	
	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_LEAVE) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			if (dir>=0 && dir < NUM_OF_DIRS)
				add_var(&GET_TRIG_VARS(t), "direction", custom_dir ? custom_dir : (char *)dirs[dir], 0);
			else
				add_var(&GET_TRIG_VARS(t), "direction", custom_dir ? custom_dir : "none", 0);
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.r = room;
			return script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}

int door_wtrigger(char_data *actor, int subcmd, int dir) {
	room_data *room;
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (!actor || !SCRIPT_CHECK(IN_ROOM(actor), WTRIG_DOOR))
		return 1;

	room = IN_ROOM(actor);
	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_DOOR) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			add_var(&GET_TRIG_VARS(t), "cmd", (char *)cmd_door[subcmd], 0);
			if (dir>=0 && dir < NUM_OF_DIRS)
				add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[dir], 0);
			else
				add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.r = room;
			return script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}


void reboot_wtrigger(room_data *room) {
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;
	int val;

	if (!SCRIPT_CHECK(room, WTRIG_REBOOT)) {
		return;
	}

	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_REBOOT)) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			sdd.r = room;
			val = script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
			if (!val) {
				break;
			}
		}
	}
}


/**
* Checks all triggers for something that would prevent a 'buy'.
*
* @param char_data *actor The person trying to buy.
* @param char_data *shopkeeper The mob shopkeeper, if any (many shops have none).
* @param obj_data *buying The item being bought.
* @param int cost The amount to be charged.
* @param any_vnum currency The currency type (NOTHING for coins).
* @return bool FALSE means hit-trigger/stop; TRUE means continue buying.
*/
bool check_buy_trigger(char_data *actor, char_data *shopkeeper, obj_data *buying, int cost, any_vnum currency) {
	int cont = 1;
	
	cont = buy_wtrigger(actor, shopkeeper, buying, cost, currency);	// world trigs
	if (cont) {
		cont = buy_mtrigger(actor, shopkeeper, buying, cost, currency);	// mob trigs
	}
	if (cont) {
		cont = buy_otrigger(actor, shopkeeper, buying, cost, currency);	// obj trigs
	}
	if (cont) {
		cont = buy_vtrigger(actor, shopkeeper, buying, cost, currency);	// vehicles
	}
	return cont;
}


/**
* Checks all triggers for a command match.
*
* @param char_data *actor The person typing a command.
* @param char *cmd The command as-typed (first word).
* @param char *argument Any arguments (remaining text).
* @param int mode CMDTRG_EXACT or CMDTRG_ABBREV.
* @return bool TRUE means hit-trigger/stop; FALSE means continue execution
*/
bool check_command_trigger(char_data *actor, char *cmd, char *argument, int mode) {
	int cont = 0;
	
	if (!IS_NPC(actor) && ACCOUNT_FLAGGED(actor, ACCT_FROZEN)) {
		return cont;
	}
	
	// never override the toggle command for immortals
	if (IS_IMMORTAL(actor) && is_abbrev(cmd, "toggles")) {
		return cont;
	}

	cont = command_wtrigger(actor, cmd, argument, mode);	// world trigs
	if (!cont) {
		cont = command_mtrigger(actor, cmd, argument, mode);	// mob trigs
	}
	if (!cont) {
		cont = command_otrigger(actor, cmd, argument, mode);	// obj trigs
	}
	if (!cont) {
		cont = command_vtrigger(actor, cmd, argument, mode);	// vehicles
	}
	return cont;
}


 //////////////////////////////////////////////////////////////////////////////
//// VEHICLE TRIGGERS ////////////////////////////////////////////////////////

/**
* Buy trigger (vehicle): fires when someone tries to buy
*
* @param char_data *actor The person trying to buy.
* @param char_data *shopkeeper The mob shopkeeper, if any (many shops have none).
* @param obj_data *buying The item being bought.
* @param int cost The amount to be charged.
* @param any_vnum currency The currency type (NOTHING for coins).
* @return int 0 if a trigger blocked the buy (stop); 1 if not (ok to continue).
*/
int buy_vtrigger(char_data *actor, char_data *shopkeeper, obj_data *buying, int cost, any_vnum currency) {
	union script_driver_data_u sdd;
	vehicle_data *veh, *next_veh;
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	// gods not affected
	if (!valid_dg_target(actor, DG_ALLOW_GODS)) {
		return 1;
	}
	
	LL_FOREACH_SAFE2(ROOM_VEHICLES(IN_ROOM(actor)), veh, next_veh, next_in_room) {
		if (!SCRIPT_CHECK(veh, VTRIG_BUY)) {
			continue;
		}
		
		LL_FOREACH(TRIGGERS(SCRIPT(veh)), t) {
			if (!TRIGGER_CHECK(t, VTRIG_BUY)) {
				continue;
			}
			
			// vars
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			ADD_UID_VAR(buf, t, obj_script_id(buying), "obj", 0);
			if (shopkeeper) {
				ADD_UID_VAR(buf, t, char_script_id(shopkeeper), "shopkeeper", 0);
			}
			else {
				add_var(&GET_TRIG_VARS(t), "shopkeeper", "", 0);
			}
			
			snprintf(buf, sizeof(buf), "%d", cost);
			add_var(&GET_TRIG_VARS(t), "cost", buf, 0);
			
			if (currency == NOTHING) {
				strcpy(buf, "coins");
			}
			else {
				snprintf(buf, sizeof(buf), "%d", currency);
			}
			add_var(&GET_TRIG_VARS(t), "currency", buf, 0);
			
			// run it
			sdd.v = veh;
			if (!script_driver(&sdd, t, VEH_TRIGGER, TRIG_NEW)) {
				return 0;
			}
		}
	}

	return 1;
}


/**
* Command trigger (vehicle).
*
* @param char_data *actor The person typing a command.
* @param char *cmd The command as-typed (first word).
* @param char *argument Any arguments (remaining text).
* @param int mode CMDTRG_EXACT or CMDTRG_ABBREV.
* @return int 1 if a trigger ran (stop); 0 if not (ok to continue).
*/
int command_vtrigger(char_data *actor, char *cmd, char *argument, int mode) {
	vehicle_data *veh, *next_veh;
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	/* prevent people we like from becoming trapped :P */
	if (!valid_dg_target(actor, 0)) {
		return 0;
	}
	
	LL_FOREACH_SAFE2(ROOM_VEHICLES(IN_ROOM(actor)), veh, next_veh, next_in_room) {
		if (SCRIPT_CHECK(veh, VTRIG_COMMAND)) {
			for (t = TRIGGERS(SCRIPT(veh)); t; t = t->next) {
				if (!TRIGGER_CHECK(t, VTRIG_COMMAND)) {
					continue;
				}

				if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
					syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: Command Trigger #%d has no text argument!", GET_TRIG_VNUM(t));
					continue;
				}

				if (match_command_trig(cmd, GET_TRIG_ARG(t), mode)) {
					union script_driver_data_u sdd;
					ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
					skip_spaces(&argument);
					add_var(&GET_TRIG_VARS(t), "arg", argument, 0);
					skip_spaces(&cmd);
					add_var(&GET_TRIG_VARS(t), "cmd", cmd, 0);
					sdd.v = veh;

					if (script_driver(&sdd, t, VEH_TRIGGER, TRIG_NEW)) {
						return 1;
					}
				}
			}
		}
	}

	return 0;
}


int destroy_vtrigger(vehicle_data *veh) {
	trig_data *t;

	if (!SCRIPT_CHECK(veh, VTRIG_DESTROY)) {
		return 1;
	}

	for (t = TRIGGERS(SCRIPT(veh)); t; t = t->next) {
		if (TRIGGER_CHECK(t, VTRIG_DESTROY) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			sdd.v = veh;
			return script_driver(&sdd, t, VEH_TRIGGER, TRIG_NEW);
		}
	}

	return dg_owner_purged ? 0 : 1;
}


int entry_vtrigger(vehicle_data *veh) {
	trig_data *t;

	if (!SCRIPT_CHECK(veh, VTRIG_ENTRY)) {
		return 1;
	}

	for (t = TRIGGERS(SCRIPT(veh)); t; t = t->next) {
		if (TRIGGER_CHECK(t, VTRIG_ENTRY) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			sdd.v = veh;
			return script_driver(&sdd, t, VEH_TRIGGER, TRIG_NEW);
			break;
		}
	}

	return 1;
}


int greet_vtrigger(char_data *actor, int dir) {
	int intermediate, final = TRUE;
	vehicle_data *veh, *next_veh;
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (IS_IMMORTAL(actor) && (GET_INVIS_LEV(actor) > LVL_MORTAL || PRF_FLAGGED(actor, PRF_WIZHIDE))) {
		return 1;
	}
	if (!valid_dg_target(actor, DG_ALLOW_GODS)) {
		return TRUE;
	}
	
	LL_FOREACH_SAFE2(ROOM_VEHICLES(IN_ROOM(actor)), veh, next_veh, next_in_room) {
		if (!SCRIPT_CHECK(veh, VTRIG_GREET)) {
			continue;
		}

		for (t = TRIGGERS(SCRIPT(veh)); t; t = t->next) {
			if (IS_SET(GET_TRIG_TYPE(t), VTRIG_GREET) && !GET_TRIG_DEPTH(t) && (number(1, 100) <= GET_TRIG_NARG(t))) {
				union script_driver_data_u sdd;
				if (dir >= 0 && dir < NUM_OF_DIRS) {
					add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[rev_dir[dir]], 0);
				}
				else {
					add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
				}
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				sdd.v = veh;
				intermediate = script_driver(&sdd, t, VEH_TRIGGER, TRIG_NEW);
				if (!intermediate) {
					final = FALSE;
				}
				continue;
			}
		}
	}
	return final;
}


/**
* @param char_data *actor The person trying to leave.
* @param int dir The direction they are trying to go (passed through to %direction%).
* @param char *custom_dir Optional: A different value for %direction% (may be NULL).
* @return int 0 = block the leave, 1 = pass
*/
int leave_vtrigger(char_data *actor, int dir, char *custom_dir) {
	vehicle_data *veh, *next_veh;
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;
	
	if (IS_IMMORTAL(actor) && (GET_INVIS_LEV(actor) > LVL_MORTAL || PRF_FLAGGED(actor, PRF_WIZHIDE))) {
		return 1;
	}
	
	LL_FOREACH_SAFE2(ROOM_VEHICLES(IN_ROOM(actor)), veh, next_veh, next_in_room) {
		if (!SCRIPT_CHECK(veh, VTRIG_LEAVE)) {
			continue;
		}

		for (t = TRIGGERS(SCRIPT(veh)); t; t = t->next) {
			if (IS_SET(GET_TRIG_TYPE(t), VTRIG_LEAVE) && !GET_TRIG_DEPTH(t) && (number(1, 100) <= GET_TRIG_NARG(t))) {
				union script_driver_data_u sdd;
				if ( dir >= 0 && dir < NUM_OF_DIRS) {
					add_var(&GET_TRIG_VARS(t), "direction", custom_dir ? custom_dir : (char *)dirs[dir], 0);
				}
				else {
					add_var(&GET_TRIG_VARS(t), "direction", custom_dir ? custom_dir : "none", 0);
				}
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				sdd.v = veh;
				return script_driver(&sdd, t, VEH_TRIGGER, TRIG_NEW);
			}
		}
	}
	return 1;
}


void load_vtrigger(vehicle_data *veh) {
	trig_data *t;

	if (!SCRIPT_CHECK(veh, VTRIG_LOAD)) {
		return;
	}

	for (t = TRIGGERS(SCRIPT(veh)); t; t = t->next) {
		if (TRIGGER_CHECK(t, VTRIG_LOAD) &&  (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			sdd.v = veh;
			script_driver(&sdd, t, VEH_TRIGGER, TRIG_NEW);
			break;
		}
	}
}


/**
* This trigger is only called if a char is in the area without nohassle.
*
* @param vehicle_data *veh The vehicle triggering.
*/


void reboot_vtrigger(vehicle_data *veh) {
	trig_data *t;
	int val;

	if (!SCRIPT_CHECK(veh, VTRIG_REBOOT)) {
		return;
	}

	for (t = TRIGGERS(SCRIPT(veh)); t; t = t->next) {
		if (TRIGGER_CHECK(t, VTRIG_REBOOT)) {
			union script_driver_data_u sdd;
			sdd.v = veh;
			val = script_driver(&sdd, t, VEH_TRIGGER, TRIG_NEW);
			if (!val) {
				break;
			}
		}
	}
}


void speech_vtrigger(char_data *actor, char *str) {
	vehicle_data *veh, *next_veh;
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;
	
	LL_FOREACH_SAFE2(ROOM_VEHICLES(IN_ROOM(actor)), veh, next_veh, next_in_room) {
		if (SCRIPT_CHECK(veh, VTRIG_SPEECH)) {
			for (t = TRIGGERS(SCRIPT(veh)); t; t = t->next) {
				if (!TRIGGER_CHECK(t, VTRIG_SPEECH)) {
					continue;
				}

				if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
					syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: Speech Trigger #%d has no text argument!", GET_TRIG_VNUM(t));
					continue;
				}

				if (*GET_TRIG_ARG(t) == '*' || ((GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) || (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str)))) {
					union script_driver_data_u sdd;
					ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
					add_var(&GET_TRIG_VARS(t), "speech", str, 0);
					sdd.v = veh;
					script_driver(&sdd, t, VEH_TRIGGER, TRIG_NEW);
					break;
				}
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// START QUEST TRIGGERS ////////////////////////////////////////////////////

// 1 = continue; 0 = cancel
int start_quest_mtrigger(char_data *actor, quest_data *quest, struct instance_data *inst) {
	char buf[MAX_INPUT_LENGTH];
	char_data *ch;
	trig_data *t;
	
	if (!valid_dg_target(actor, DG_ALLOW_GODS)) {
		return TRUE;
	}
	
	// store instance globally to allow %instance.xxx% in scripts
	quest_instance_global = inst;
	
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(actor)), ch, next_in_room) {
		if (!SCRIPT_CHECK(ch, MTRIG_START_QUEST) || (ch == actor)) {
			continue;
		}
		
		LL_FOREACH(TRIGGERS(SCRIPT(ch)), t) {
			if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
				continue;
			}
			if (IS_SET(GET_TRIG_TYPE(t), MTRIG_START_QUEST)) {
				union script_driver_data_u sdd;
				if (quest) {
					snprintf(buf, sizeof(buf), "%d", QUEST_VNUM(quest));
					add_var(&GET_TRIG_VARS(t), "questvnum", buf, 0);
					add_var(&GET_TRIG_VARS(t), "questname", QUEST_NAME(quest), 0);
				}
				else {	// no quest?
					add_var(&GET_TRIG_VARS(t), "questvnum", "0", 0);
					add_var(&GET_TRIG_VARS(t), "questname", "Unknown", 0);
				}
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				sdd.c = ch;
				if (!script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW)) {
					quest_instance_global = NULL;	// un-set this
					return FALSE;
				}
			}
		}
	}
	quest_instance_global = NULL;	// un-set this
	return TRUE;
}


// 1 = continue; 0 = cancel
int start_quest_otrigger_one(obj_data *obj, char_data *actor, quest_data *quest, struct instance_data *inst) {
	char buf[MAX_INPUT_LENGTH];
	int ret_val = TRUE;
	trig_data *t;
	
	if (!SCRIPT_CHECK(obj, OTRIG_START_QUEST)) {
		return TRUE;
	}
	
	// store instance globally to allow %instance.xxx% in scripts
	quest_instance_global = inst;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_START_QUEST)) {
			union script_driver_data_u sdd;
			if (quest) {
				snprintf(buf, sizeof(buf), "%d", QUEST_VNUM(quest));
				add_var(&GET_TRIG_VARS(t), "questvnum", buf, 0);
				add_var(&GET_TRIG_VARS(t), "questname", QUEST_NAME(quest), 0);
			}
			else {	// no quest?
				add_var(&GET_TRIG_VARS(t), "questvnum", "0", 0);
				add_var(&GET_TRIG_VARS(t), "questname", "Unknown", 0);
			}
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.o = obj;
			ret_val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			if (!ret_val) {
				break;
			}
		}
	}
	
	quest_instance_global = NULL;	// un-set this
	return ret_val;
}


// 1 = continue, 0 = stop
int start_quest_vtrigger(char_data *actor, quest_data *quest, struct instance_data *inst) {
	vehicle_data *veh, *next_veh;
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (!valid_dg_target(actor, DG_ALLOW_GODS)) {
		return TRUE;
	}
	
	// store instance globally to allow %instance.xxx% in scripts
	quest_instance_global = inst;
	
	LL_FOREACH_SAFE2(ROOM_VEHICLES(IN_ROOM(actor)), veh, next_veh, next_in_room) {
		if (!SCRIPT_CHECK(veh, VTRIG_START_QUEST)) {
			continue;
		}

		for (t = TRIGGERS(SCRIPT(veh)); t; t = t->next) {
			if (IS_SET(GET_TRIG_TYPE(t), VTRIG_START_QUEST)) {
				union script_driver_data_u sdd;
				if (quest) {
					snprintf(buf, sizeof(buf), "%d", QUEST_VNUM(quest));
					add_var(&GET_TRIG_VARS(t), "questvnum", buf, 0);
					add_var(&GET_TRIG_VARS(t), "questname", QUEST_NAME(quest), 0);
				}
				else {	// no quest?
					add_var(&GET_TRIG_VARS(t), "questvnum", "0", 0);
					add_var(&GET_TRIG_VARS(t), "questname", "Unknown", 0);
				}
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				sdd.v = veh;
				if (!script_driver(&sdd, t, VEH_TRIGGER, TRIG_NEW)) {
					quest_instance_global = NULL;	// un-set this
					return FALSE;
				}
			}
		}
	}
	
	quest_instance_global = NULL;	// un-set this
	return TRUE;
}


// 0 = stop, 1 = continue
int start_quest_wtrigger(room_data *room, char_data *actor, quest_data *quest, struct instance_data *inst) {
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (!SCRIPT_CHECK(room, WTRIG_START_QUEST))
		return 1;
	
	// store instance globally to allow %instance.xxx% in scripts
	quest_instance_global = inst;
	
	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_START_QUEST)) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			if (quest) {
				snprintf(buf, sizeof(buf), "%d", QUEST_VNUM(quest));
				add_var(&GET_TRIG_VARS(t), "questvnum", buf, 0);
				add_var(&GET_TRIG_VARS(t), "questname", QUEST_NAME(quest), 0);
			}
			else {	// no quest?
				add_var(&GET_TRIG_VARS(t), "questvnum", "0", 0);
				add_var(&GET_TRIG_VARS(t), "questname", "Unknown", 0);
			}
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.r = room;
			if (!script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW)) {
				quest_instance_global = NULL;	// un-set this
				return FALSE;
			}
		}
	}
	
	quest_instance_global = NULL;	// un-set this
	return TRUE;
}


/**
* Start quest trigger (obj).
*
* @param char_data *actor The person trying to get a quest.
* @param quest_data *quest The quest to try to start
* @param struct instance_data *inst The instance associated with the quest, if any.
* @return int 0/FALSE to stop the quest, 1/TRUE to allow it to continue.
*/
int start_quest_otrigger(char_data *actor, quest_data *quest, struct instance_data *inst) {
	obj_data *obj;
	int i;

	/* prevent people we like from becoming trapped :P */
	if (!valid_dg_target(actor, DG_ALLOW_GODS))
		return 1;

	for (i = 0; i < NUM_WEARS; i++)
		if (GET_EQ(actor, i) && !start_quest_otrigger_one(GET_EQ(actor, i), actor, quest, inst))
			return 0;

	for (obj = actor->carrying; obj; obj = obj->next_content)
		if (!start_quest_otrigger_one(obj, actor, quest, inst))
			return 0;

	for (obj = ROOM_CONTENTS(IN_ROOM(actor)); obj; obj = obj->next_content)
		if (!start_quest_otrigger_one(obj, actor, quest, inst))
			return 0;

	return 1;
}


/**
* Checks all quest triggers
*
* @param char_data *actor The person trying to start a quest.
* @param quest_data *quest The quest.
* @param struct instance_data *inst An associated instance, if there is one.
* @return int 0 means stop execution (block quest), 1 means continue
*/
int check_start_quest_trigger(char_data *actor, quest_data *quest, struct instance_data *inst) {
	room_data *room = IN_ROOM(actor);
	struct trig_proto_list *tpl;
	trig_data *proto, *trig;
	int val = 1;

	if (val) {
		val = start_quest_mtrigger(actor, quest, inst);	// mob trigs
	}
	if (val) {
		val = start_quest_otrigger(actor, quest, inst);	// obj trigs
	}
	if (val) {
		val = start_quest_vtrigger(actor, quest, inst);	// vehicles
	}
	if (val) {
		// still here? world triggers require additional work because we add
		// the trigs from the quest itself
		LL_FOREACH(QUEST_SCRIPTS(quest), tpl) {
			if (!(proto = real_trigger(tpl->vnum))) {
				continue;
			}
			if (!IS_SET(GET_TRIG_TYPE(proto), WTRIG_START_QUEST)) {
				continue;
			}
			
			// attach this trigger
			if (!(trig = read_trigger(tpl->vnum))) {
				continue;
			}
			if (!SCRIPT(room)) {
				create_script_data(room, WLD_TRIGGER);
			}
			add_trigger(SCRIPT(room), trig, -1);
		}
		
		val = start_quest_wtrigger(room, actor, quest, inst);	// world trigs
		
		// now remove those triggers again
		LL_FOREACH(QUEST_SCRIPTS(quest), tpl) {
			if (!(proto = real_trigger(tpl->vnum))) {
				continue;
			}
			if (!IS_SET(GET_TRIG_TYPE(proto), WTRIG_START_QUEST)) {
				continue;
			}
			
			// find and remove
			if (SCRIPT(room)) {
				remove_live_script_by_vnum(SCRIPT(room), tpl->vnum);
			}
		}
	}
	return val;
}


 //////////////////////////////////////////////////////////////////////////////
//// FINISH QUEST TRIGGERS ///////////////////////////////////////////////////

// 1 = continue; 0 = cancel
int finish_quest_mtrigger(char_data *actor, quest_data *quest, struct instance_data *inst) {
	char buf[MAX_INPUT_LENGTH];
	char_data *ch;
	trig_data *t;
	
	if (!valid_dg_target(actor, DG_ALLOW_GODS)) {
		return TRUE;
	}
	
	// store instance globally to allow %instance.xxx% in scripts
	quest_instance_global = inst;
	
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(actor)), ch, next_in_room) {
		if (!SCRIPT_CHECK(ch, MTRIG_FINISH_QUEST) || (ch == actor)) {
			continue;
		}
		
		LL_FOREACH(TRIGGERS(SCRIPT(ch)), t) {
			if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
				continue;
			}
			if (IS_SET(GET_TRIG_TYPE(t), MTRIG_FINISH_QUEST)) {
				union script_driver_data_u sdd;
				if (quest) {
					snprintf(buf, sizeof(buf), "%d", QUEST_VNUM(quest));
					add_var(&GET_TRIG_VARS(t), "questvnum", buf, 0);
					add_var(&GET_TRIG_VARS(t), "questname", QUEST_NAME(quest), 0);
				}
				else {	// no quest?
					add_var(&GET_TRIG_VARS(t), "questvnum", "0", 0);
					add_var(&GET_TRIG_VARS(t), "questname", "Unknown", 0);
				}
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				sdd.c = ch;
				if (!script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW)) {
					quest_instance_global = NULL;	// un-set this
					return FALSE;
				}
			}
		}
	}
	
	quest_instance_global = NULL;	// un-set this
	return TRUE;
}


// 1 = continue; 0 = cancel
int finish_quest_otrigger_one(obj_data *obj, char_data *actor, quest_data *quest, struct instance_data *inst) {
	char buf[MAX_INPUT_LENGTH];
	int ret_val = TRUE;
	trig_data *t;
	
	if (!SCRIPT_CHECK(obj, OTRIG_FINISH_QUEST)) {
		return TRUE;
	}
	
	// store instance globally to allow %instance.xxx% in scripts
	quest_instance_global = inst;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_FINISH_QUEST)) {
			union script_driver_data_u sdd;
			if (quest) {
				snprintf(buf, sizeof(buf), "%d", QUEST_VNUM(quest));
				add_var(&GET_TRIG_VARS(t), "questvnum", buf, 0);
				add_var(&GET_TRIG_VARS(t), "questname", QUEST_NAME(quest), 0);
			}
			else {	// no quest?
				add_var(&GET_TRIG_VARS(t), "questvnum", "0", 0);
				add_var(&GET_TRIG_VARS(t), "questname", "Unknown", 0);
			}
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.o = obj;
			ret_val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			if (!ret_val) {
				break;
			}
		}
	}
	
	quest_instance_global = NULL;	// un-set this
	return ret_val;
}


// 1 = continue, 0 = stop
int finish_quest_vtrigger(char_data *actor, quest_data *quest, struct instance_data *inst) {
	vehicle_data *veh, *next_veh;
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (!valid_dg_target(actor, DG_ALLOW_GODS)) {
		return TRUE;
	}
	
	// store instance globally to allow %instance.xxx% in scripts
	quest_instance_global = inst;
	
	LL_FOREACH_SAFE2(ROOM_VEHICLES(IN_ROOM(actor)), veh, next_veh, next_in_room) {
		if (!SCRIPT_CHECK(veh, VTRIG_FINISH_QUEST)) {
			continue;
		}

		for (t = TRIGGERS(SCRIPT(veh)); t; t = t->next) {
			if (IS_SET(GET_TRIG_TYPE(t), VTRIG_FINISH_QUEST)) {
				union script_driver_data_u sdd;
				if (quest) {
					snprintf(buf, sizeof(buf), "%d", QUEST_VNUM(quest));
					add_var(&GET_TRIG_VARS(t), "questvnum", buf, 0);
					add_var(&GET_TRIG_VARS(t), "questname", QUEST_NAME(quest), 0);
				}
				else {	// no quest?
					add_var(&GET_TRIG_VARS(t), "questvnum", "0", 0);
					add_var(&GET_TRIG_VARS(t), "questname", "Unknown", 0);
				}
				ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
				sdd.v = veh;
				if (!script_driver(&sdd, t, VEH_TRIGGER, TRIG_NEW)) {
					quest_instance_global = NULL;	// un-set this
					return FALSE;
				}
			}
		}
	}
	
	quest_instance_global = NULL;	// un-set this
	return TRUE;
}


// 0 = stop, 1 = continue
int finish_quest_wtrigger(room_data *room, char_data *actor, quest_data *quest, struct instance_data *inst) {
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (!SCRIPT_CHECK(room, WTRIG_FINISH_QUEST))
		return 1;
	
	// store instance globally to allow %instance.xxx% in scripts
	quest_instance_global = inst;

	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_FINISH_QUEST)) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, room_script_id(room), "room", 0);
			if (quest) {
				snprintf(buf, sizeof(buf), "%d", QUEST_VNUM(quest));
				add_var(&GET_TRIG_VARS(t), "questvnum", buf, 0);
				add_var(&GET_TRIG_VARS(t), "questname", QUEST_NAME(quest), 0);
			}
			else {	// no quest?
				add_var(&GET_TRIG_VARS(t), "questvnum", "0", 0);
				add_var(&GET_TRIG_VARS(t), "questname", "Unknown", 0);
			}
			ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
			sdd.r = room;
			if (!script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW)) {
				quest_instance_global = NULL;	// un-set this
				return FALSE;
			}
		}
	}
	
	quest_instance_global = NULL;	// un-set this
	return TRUE;
}


/**
* Finish quest trigger (obj).
*
* @param char_data *actor The person trying to get a quest.
* @param quest_data *quest The quest to try to finish
* @param struct instance_data *inst The associated instance, if any.
* @return int 0/FALSE to stop the quest, 1/TRUE to allow it to continue.
*/
int finish_quest_otrigger(char_data *actor, quest_data *quest, struct instance_data *inst) {
	obj_data *obj;
	int i;

	/* prevent people we like from becoming trapped :P */
	if (!valid_dg_target(actor, DG_ALLOW_GODS))
		return 1;

	for (i = 0; i < NUM_WEARS; i++)
		if (GET_EQ(actor, i) && !finish_quest_otrigger_one(GET_EQ(actor, i), actor, quest, inst))
			return 0;

	for (obj = actor->carrying; obj; obj = obj->next_content)
		if (!finish_quest_otrigger_one(obj, actor, quest, inst))
			return 0;

	for (obj = ROOM_CONTENTS(IN_ROOM(actor)); obj; obj = obj->next_content)
		if (!finish_quest_otrigger_one(obj, actor, quest, inst))
			return 0;

	return 1;
}


/**
* Checks all quest triggers
*
* @param char_data *actor The person trying to finish a quest.
* @param quest_data *quest The quest.
* @param struct instance_data *inst The associated instance, if any.
* @return int 0 means stop execution (block quest), 1 means continue
*/
int check_finish_quest_trigger(char_data *actor, quest_data *quest, struct instance_data *inst) {
	room_data *room = IN_ROOM(actor);
	struct trig_proto_list *tpl;
	trig_data *proto, *trig;
	int val = 1;

	if (val) {
		val = finish_quest_mtrigger(actor, quest, inst);	// mob trigs
	}
	if (val) {
		val = finish_quest_otrigger(actor, quest, inst);	// obj trigs
	}
	if (val) {
		val = finish_quest_vtrigger(actor, quest, inst);	// vehicles
	}
	if (val) {
		// still here? world triggers require additional work because we add
		// the trigs from the quest itself
		LL_FOREACH(QUEST_SCRIPTS(quest), tpl) {
			if (!(proto = real_trigger(tpl->vnum))) {
				continue;
			}
			if (!IS_SET(GET_TRIG_TYPE(proto), WTRIG_FINISH_QUEST)) {
				continue;
			}
			
			// attach this trigger
			if (!(trig = read_trigger(tpl->vnum))) {
				continue;
			}
			if (!SCRIPT(room)) {
				create_script_data(room, WLD_TRIGGER);
			}
			add_trigger(SCRIPT(room), trig, -1);
		}
		
		val = finish_quest_wtrigger(room, actor, quest, inst);	// world trigs
		
		// now remove those triggers again
		LL_FOREACH(QUEST_SCRIPTS(quest), tpl) {
			if (!(proto = real_trigger(tpl->vnum))) {
				continue;
			}
			if (!IS_SET(GET_TRIG_TYPE(proto), WTRIG_FINISH_QUEST)) {
				continue;
			}
			
			// find and remove
			if (SCRIPT(room)) {
				remove_live_script_by_vnum(SCRIPT(room), tpl->vnum);
			}
		}
	}
	return val;
}


 //////////////////////////////////////////////////////////////////////////////
//// RESET TRIGGER HELPER ////////////////////////////////////////////////////

// runs room reset triggers on a loop
EVENTFUNC(run_reset_triggers) {
	struct room_event_data *data = (struct room_event_data *)event_obj;
	room_data *room;
	
	// grab data but don't free (we usually reschedule this)
	room = data->room;
	
	// still have any?
	if (IS_ADVENTURE_ROOM(room) || !SCRIPT_CHECK(room, WTRIG_RESET)) {
		delete_stored_event_room(room, SEV_RESET_TRIGGER);
		free(data);
		return 0;
	}
	
	reset_wtrigger(room);
	return (7.5 * 60) RL_SEC;	// reenqueue for the original time
}


/**
* Checks if a building is a tavern and can run. If so, sets up the data for
* it and schedules the event. If not, it clears that data.
*
* @param room_data *room The room to check for tavernness.
* @param bool random_offest If TRUE, throws in some random in the 1st timer.
*/
void check_reset_trigger_event(room_data *room, bool random_offset) {
	struct room_event_data *data;
	struct dg_event *ev;
	int mins;
	
	if (!IS_ADVENTURE_ROOM(room) && SCRIPT_CHECK(room, WTRIG_RESET)) {
		if (!find_stored_event_room(room, SEV_RESET_TRIGGER)) {
			CREATE(data, struct room_event_data, 1);
			data->room = room;
			
			// schedule every 7.5 minutes
			mins = 7.5 - (random_offset ? number(0,6) : 0);
			ev = dg_event_create(run_reset_triggers, (void*)data, (mins * 60) RL_SEC);
			add_stored_event_room(room, SEV_RESET_TRIGGER, ev);
		}
	}
	else {
		cancel_stored_event_room(room, SEV_RESET_TRIGGER);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// KILL TRIGGER FUNCS //////////////////////////////////////////////////////

/**
* Runs kill triggers for everyone involved in the kill -- the killer, their
* allies, and all items possessed by those people. Additionally, vehicles also
* fire kill triggers if they cause the kill, even if they are not in the same
* room.
*
* Kill triggers will not fire if a person kills himself.
*
* @param char_data *dying The person who has died.
* @param char_data *killer Optional: Person who killed them.
* @param vehicle_data *veh_killer Optional: Vehicle who killed them.
* @return int The return value of a script (1 is normal, 0 suppresses the death cry).
*/
int run_kill_triggers(char_data *dying, char_data *killer, vehicle_data *veh_killer) {
	extern bool is_fight_ally(char_data *ch, char_data *frenemy);	// fight.c
	
	union script_driver_data_u sdd;
	char_data *ch_iter, *next_ch;
	trig_data *trig, *next_trig;
	char buf[MAX_INPUT_LENGTH];
	room_data *room;
	int pos;
	
	int val = 1;	// default return value
	
	if (!dying) {
		return val;	// somehow
	}
	
	// store this, in case it changes during any script
	room = IN_ROOM(dying);
	
	if (killer && killer != dying) {
		// check characters first:
		LL_FOREACH_SAFE2(ROOM_PEOPLE(room), ch_iter, next_ch, next_in_room) {
			if (EXTRACTED(ch_iter) || IS_DEAD(ch_iter) || !SCRIPT_CHECK(ch_iter, MTRIG_KILL)) {
				continue;
			}
			if (ch_iter == dying) {
				continue;	// cannot fire if killing self
			}
			if (ch_iter != killer && !is_fight_ally(ch_iter, killer)) {
				continue;	// is not on the killing team
			}
			LL_FOREACH_SAFE(TRIGGERS(SCRIPT(ch_iter)), trig, next_trig) {
				if (AFF_FLAGGED(ch_iter, AFF_CHARM) && !TRIGGER_CHECK(trig, MTRIG_CHARMED)) {
					continue;	// cannot do while charmed
				}
				if (!TRIGGER_CHECK(trig, MTRIG_KILL) || (number(1, 100) > GET_TRIG_NARG(trig))) {
					continue;	// wrong trig or failed random percent
				}
			
				// ok:
				memset((char *) &sdd, 0, sizeof(union script_driver_data_u));
				ADD_UID_VAR(buf, trig, char_script_id(dying), "actor", 0);
				if (killer) {
					ADD_UID_VAR(buf, trig, char_script_id(killer), "killer", 0);
				}
				else {
					add_var(&GET_TRIG_VARS(trig), "killer", "", 0);
				}
				sdd.c = ch_iter;
			
				// run it -- any script returning 0 guarantees we will return 0
				val &= script_driver(&sdd, trig, MOB_TRIGGER, TRIG_NEW);
			}
		}
		
		// check gear on characters present, IF they are on the killing team:
		LL_FOREACH_SAFE2(ROOM_PEOPLE(room), ch_iter, next_ch, next_in_room) {
			if (EXTRACTED(ch_iter) || IS_DEAD(ch_iter)) {
				continue;	// cannot benefit if dead
			}
			if (ch_iter != killer && !is_fight_ally(ch_iter, killer)) {
				continue;	// is not on the killing team
			}
			
			// equipped
			for (pos = 0; pos < NUM_WEARS; ++pos) {
				if (!GET_EQ(ch_iter, pos)) {
					continue;	// no item
				}
				
				// ok:
				val &= kill_otrigger(GET_EQ(ch_iter, pos), dying, killer);
			}
			
			// inventory:
			val &= kill_otrigger(ch_iter->carrying, dying, killer);
		}
	}
	
	// and the vehicle
	if (veh_killer && SCRIPT_CHECK(veh_killer, VTRIG_KILL)) {
		LL_FOREACH_SAFE(TRIGGERS(SCRIPT(veh_killer)), trig, next_trig) {
			if (!TRIGGER_CHECK(trig, VTRIG_KILL) || (number(1, 100) > GET_TRIG_NARG(trig))) {
				continue;	// wrong trig or failed random percent
			}
			
			// ok:
			memset((char *) &sdd, 0, sizeof(union script_driver_data_u));
			ADD_UID_VAR(buf, trig, char_script_id(dying), "actor", 0);
			ADD_UID_VAR(buf, trig, veh_script_id(veh_killer), "killer", 0);
			sdd.v = veh_killer;
		
			// run it -- any script returning 0 guarantees we will return 0
			val &= script_driver(&sdd, trig, VEH_TRIGGER, TRIG_NEW);
		}
	}
	
	return val;
}


/**
* Runs kill triggers on a single object, then on its next contents
* (recursively). This function is called by run_kill_triggers, which has
* already validated that the owner of the object qualifies as either the killer
* or an ally of the killer.
*
* @param obj_data *obj The object possibly running triggers.
* @param char_data *dying The person who has died.
* @param char_data *killer Optional: Person who killed them.
* @return int The return value of a script (1 is normal, 0 suppresses the death cry).
*/
int kill_otrigger(obj_data *obj, char_data *dying, char_data *killer) {
	obj_data *next_contains, *next_inside;
	union script_driver_data_u sdd;
	trig_data *trig, *next_trig;
	int val = 1;	// default value
	
	if (!obj) {
		return val;	// often called with no obj
	}
	
	// save for later
	next_contains = obj->contains;
	next_inside = obj->next_content;
	
	// run script if possible...
	if (SCRIPT_CHECK(obj, OTRIG_KILL)) {
		LL_FOREACH_SAFE(TRIGGERS(SCRIPT(obj)), trig, next_trig) {
			if (!TRIGGER_CHECK(trig, OTRIG_KILL) || (number(1, 100) > GET_TRIG_NARG(trig))) {
				continue;	// wrong trig or failed random percent
			}
			
			// ok:
			memset((char *) &sdd, 0, sizeof(union script_driver_data_u));
			ADD_UID_VAR(buf, trig, char_script_id(dying), "actor", 0);
			if (killer) {
				ADD_UID_VAR(buf, trig, char_script_id(killer), "killer", 0);
			}
			else {
				add_var(&GET_TRIG_VARS(trig), "killer", "", 0);
			}
			sdd.o = obj;
			
			// run it -- any script returning 0 guarantees we will return 0
			val &= script_driver(&sdd, trig, OBJ_TRIGGER, TRIG_NEW);
			
			// ensure obj is safe
			obj = sdd.o;
			if (!obj) {
				break;	// cannot run more triggers -- obj is gone
			}
		}
	}
	
	// run recursively
	if (next_contains) {
		val &= kill_otrigger(next_contains, dying, killer);
	}
	if (next_inside) {
		val &= kill_otrigger(next_inside, dying, killer);
	}
	
	return val;
}
