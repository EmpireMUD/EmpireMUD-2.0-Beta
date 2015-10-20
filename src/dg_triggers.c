/* ************************************************************************
*   File: dg_triggers.c                                   EmpireMUD 2.0b3 *
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


extern const char *cmd_door[];

/* external functions */
const char *skill_name(int num);
void add_var(struct trig_var_data **var_list, char *name, char *value, int id);
char *matching_quote(char *p);
char *str_str(char *cs, char *ct);

// external vars
extern const char *dirs[];
extern const int rev_dir[];


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

void random_mtrigger(char_data *ch) {
	trig_data *t;

	/*
	* This trigger is only called if a char is in the zone without nohassle.
	*/

	if (!SCRIPT_CHECK(ch, MTRIG_RANDOM) || AFF_FLAGGED(ch, AFF_CHARM))
		return;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (TRIGGER_CHECK(t, MTRIG_RANDOM) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			sdd.c = ch;
			if (script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW)) {
				break;
			}
		}
	}
}

void greet_memory_mtrigger(char_data *actor) {
	trig_data *t;
	char_data *ch;
	struct script_memory *mem;
	char buf[MAX_INPUT_LENGTH];
	int command_performed = 0;

	if (!valid_dg_target(actor, DG_ALLOW_GODS))
		return;

	for (ch = ROOM_PEOPLE(IN_ROOM(actor)); ch; ch = ch->next_in_room) {
		if (!SCRIPT_MEM(ch) || !AWAKE(ch) || FIGHTING(ch) || (ch == actor) || AFF_FLAGGED(ch, AFF_CHARM))
			continue;
		/* find memory line with command only */
		for (mem = SCRIPT_MEM(ch); mem && SCRIPT_MEM(ch); mem=mem->next) {
			if (GET_ID(actor)!=mem->id) continue;
			if (mem->cmd) {
				command_interpreter(ch, mem->cmd); /* no script */
				command_performed = 1;
				break;
			}
			/* if a command was not performed execute the memory script */
			if (mem && !command_performed) {
				for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
					if (IS_SET(GET_TRIG_TYPE(t), MTRIG_MEMORY) && CAN_SEE(ch, actor) && !GET_TRIG_DEPTH(t) && number(1, 100) <= GET_TRIG_NARG(t)) {
						union script_driver_data_u sdd;
						ADD_UID_VAR(buf, t, actor, "actor", 0);
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

	if (!valid_dg_target(actor, DG_ALLOW_GODS))
		return TRUE;

	for (ch = ROOM_PEOPLE(IN_ROOM(actor)); ch; ch = ch->next_in_room) {
		if (!SCRIPT_CHECK(ch, MTRIG_GREET | MTRIG_GREET_ALL) || (ch == actor) || AFF_FLAGGED(ch, AFF_CHARM)) {
			continue;
		}
		if (!SCRIPT_CHECK(ch, MTRIG_GREET_ALL) && (!AWAKE(ch) || FIGHTING(ch) || AFF_FLAGGED(actor, AFF_SNEAK))) {
			continue;
		}

		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
			if (((IS_SET(GET_TRIG_TYPE(t), MTRIG_GREET) && CAN_SEE(ch, actor)) || IS_SET(GET_TRIG_TYPE(t), MTRIG_GREET_ALL)) && !GET_TRIG_DEPTH(t) && (number(1, 100) <= GET_TRIG_NARG(t))) {
				union script_driver_data_u sdd;
				if (dir>=0 && dir < NUM_OF_DIRS)
					add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[rev_dir[dir]], 0);
				else
					add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
				ADD_UID_VAR(buf, t, actor, "actor", 0);
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

	if (!SCRIPT_MEM(ch) || AFF_FLAGGED(ch, AFF_CHARM))
		return;

	for (actor = ROOM_PEOPLE(IN_ROOM(ch)); actor && SCRIPT_MEM(ch); actor = actor->next_in_room) {
		if (actor!=ch && SCRIPT_MEM(ch)) {
			for (mem = SCRIPT_MEM(ch); mem && SCRIPT_MEM(ch); mem = mem->next) {
				if (GET_ID(actor)==mem->id) {
					struct script_memory *prev;
					if (mem->cmd) command_interpreter(ch, mem->cmd);
					else {
						for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
							if (TRIGGER_CHECK(t, MTRIG_MEMORY) && (number(1, 100) <= GET_TRIG_NARG(t))){
								union script_driver_data_u sdd;
								ADD_UID_VAR(buf, t, actor, "actor", 0);
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

	if (!SCRIPT_CHECK(ch, MTRIG_ENTRY) || AFF_FLAGGED(ch, AFF_CHARM))
		return 1;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
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

		if (SCRIPT_CHECK(ch, MTRIG_COMMAND) && !AFF_FLAGGED(ch, AFF_CHARM) && (actor!=ch)) {
			for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
				if (!TRIGGER_CHECK(t, MTRIG_COMMAND))
					continue;

				if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
					syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: Command Trigger #%d has no text argument!", GET_TRIG_VNUM(t));
					continue;
				}

				if (*GET_TRIG_ARG(t) == '*' || (mode == CMDTRG_EXACT && !str_cmp(cmd, GET_TRIG_ARG(t))) || (mode == CMDTRG_ABBREV && is_abbrev(cmd, GET_TRIG_ARG(t)))) {
					union script_driver_data_u sdd;
					ADD_UID_VAR(buf, t, actor, "actor", 0);
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

		if (SCRIPT_CHECK(ch, MTRIG_SPEECH) && AWAKE(ch) && !AFF_FLAGGED(ch, AFF_CHARM) && (actor!=ch)) {
			for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
				if (!TRIGGER_CHECK(t, MTRIG_SPEECH))
					continue;

				if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
					syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: Speech Trigger #%d has no text argument!", GET_TRIG_VNUM(t));
					continue;
				}

				if (*GET_TRIG_ARG(t) == '*' || ((GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) || (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str)))) {
					union script_driver_data_u sdd;
					ADD_UID_VAR(buf, t, actor, "actor", 0);
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

	if (SCRIPT_CHECK(ch, MTRIG_ACT) && !AFF_FLAGGED(ch, AFF_CHARM) && (actor!=ch)) {
		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next)  {
			if (!TRIGGER_CHECK(t, MTRIG_ACT))
				continue;

			if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
				syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: Act Trigger #%d has no text argument!", GET_TRIG_VNUM(t));
				continue;
			}

			if (((GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) || (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str)))) {
				union script_driver_data_u sdd;

				if (actor)
					ADD_UID_VAR(buf, t, actor, "actor", 0);
				if (victim)
					ADD_UID_VAR(buf, t, victim, "victim", 0);
				if (object)
					ADD_UID_VAR(buf, t, object, "object", 0);
				if (target)
					ADD_UID_VAR(buf, t, target, "target", 0);
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

	if (!SCRIPT_CHECK(ch, MTRIG_FIGHT | MTRIG_FIGHT_CHARMED) || !FIGHTING(ch))
		return;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_FIGHT_CHARMED)) {
			continue;
		}
		if (TRIGGER_CHECK(t, MTRIG_FIGHT | MTRIG_FIGHT_CHARMED) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;

			actor = FIGHTING(ch);
			if (actor)
				ADD_UID_VAR(buf, t, actor, "actor", 0);
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

	if (!SCRIPT_CHECK(ch, MTRIG_HITPRCNT) || !FIGHTING(ch) || AFF_FLAGGED(ch, AFF_CHARM))
		return;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (TRIGGER_CHECK(t, MTRIG_HITPRCNT) && GET_MAX_HEALTH(ch) && (((GET_HEALTH(ch) * 100) / MAX(1, GET_MAX_HEALTH(ch))) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;

			actor = FIGHTING(ch);
			ADD_UID_VAR(buf, t, actor, "actor", 0);
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

	if (!SCRIPT_CHECK(ch, MTRIG_RECEIVE) || AFF_FLAGGED(ch, AFF_CHARM))
		return 1;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (TRIGGER_CHECK(t, MTRIG_RECEIVE) && (number(1, 100) <= GET_TRIG_NARG(t))){
			union script_driver_data_u sdd;

			ADD_UID_VAR(buf, t, actor, "actor", 0);
			ADD_UID_VAR(buf, t, obj, "object", 0);
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

	if (!SCRIPT_CHECK(ch, MTRIG_DEATH) || AFF_FLAGGED(ch, AFF_CHARM))
	return 1;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (TRIGGER_CHECK(t, MTRIG_DEATH) && (number(1, 100) <= GET_TRIG_NARG(t))){
			union script_driver_data_u sdd;

			if (actor && actor != ch)
				ADD_UID_VAR(buf, t, actor, "actor", 0);
			sdd.c = ch;
			return script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}

void bribe_mtrigger(char_data *ch, char_data *actor, int amount) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (!SCRIPT_CHECK(ch, MTRIG_BRIBE) || AFF_FLAGGED(ch, AFF_CHARM))
		return;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (TRIGGER_CHECK(t, MTRIG_BRIBE) && (amount >= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			sdd.c = ch;
			snprintf(buf, sizeof(buf), "%d", amount);
			add_var(&GET_TRIG_VARS(t), "amount", buf, 0);
			ADD_UID_VAR(buf, t, actor, "actor", 0);
			script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
			break;
		}
	}
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

int ability_mtrigger(char_data *actor, char_data *ch, int abil) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (ch == NULL || abil == NO_ABIL)
		return 1;

	if (!SCRIPT_CHECK(ch, MTRIG_ABILITY) || AFF_FLAGGED(ch, AFF_CHARM))
		return 1;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (TRIGGER_CHECK(t, MTRIG_ABILITY) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;

			ADD_UID_VAR(buf, t, actor, "actor", 0);
			sprintf(buf, "%d", abil);
			add_var(&GET_TRIG_VARS(t), "ability", buf, 0);
			add_var(&GET_TRIG_VARS(t), "abilityname", ability_data[abil].name, 0);
			sdd.c = ch;
			return script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}

int leave_mtrigger(char_data *actor, int dir) {
	trig_data *t;
	char_data *ch;
	char buf[MAX_INPUT_LENGTH];

	for (ch = ROOM_PEOPLE(IN_ROOM(actor)); ch; ch = ch->next_in_room) {
		if (!SCRIPT_CHECK(ch, MTRIG_LEAVE | MTRIG_LEAVE_ALL) || (ch == actor) || AFF_FLAGGED(ch, AFF_CHARM)) {
			continue;
		}
		if (!SCRIPT_CHECK(ch, MTRIG_LEAVE_ALL) && (!AWAKE(ch) || FIGHTING(ch))) {
			continue;
		}

		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
			if (((IS_SET(GET_TRIG_TYPE(t), MTRIG_LEAVE) && CAN_SEE(ch, actor)) || IS_SET(GET_TRIG_TYPE(t), MTRIG_LEAVE_ALL)) && !GET_TRIG_DEPTH(t) && (number(1, 100) <= GET_TRIG_NARG(t))) {
				union script_driver_data_u sdd;
				if (dir>=0 && dir < NUM_OF_DIRS)
					add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[dir], 0);
				else
					add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
				ADD_UID_VAR(buf, t, actor, "actor", 0);
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
		if (!SCRIPT_CHECK(ch, MTRIG_DOOR) || !AWAKE(ch) || FIGHTING(ch) || (ch == actor) || AFF_FLAGGED(ch, AFF_CHARM))
			continue;

		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
			if (IS_SET(GET_TRIG_TYPE(t), MTRIG_DOOR) && CAN_SEE(ch, actor) && !GET_TRIG_DEPTH(t) && (number(1, 100) <= GET_TRIG_NARG(t))) {
				union script_driver_data_u sdd;
				add_var(&GET_TRIG_VARS(t), "cmd", (char *)cmd_door[subcmd], 0);
				if (dir>=0 && dir < NUM_OF_DIRS)
					add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[dir], 0);
				else
					add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
				ADD_UID_VAR(buf, t, actor, "actor", 0);
				sdd.c = ch;
				return script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW);
			}
		}
	}
	return 1;
}


/*
*  object triggers
*/

void random_otrigger(obj_data *obj) {
	trig_data *t;
	int val;

	if (!SCRIPT_CHECK(obj, OTRIG_RANDOM))
		return;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_RANDOM) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			sdd.o = obj;
			val = script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
			obj = sdd.o;
			if (val) {
				break;
			}
		}
	}
}


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
			ADD_UID_VAR(buf, t, actor, "actor", 0);
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

			if (*GET_TRIG_ARG(t) == '*' || (mode == CMDTRG_EXACT && !str_cmp(cmd, GET_TRIG_ARG(t))) || (mode == CMDTRG_ABBREV && is_abbrev(cmd, GET_TRIG_ARG(t)))) {
				ADD_UID_VAR(buf, t, actor, "actor", 0);
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
			ADD_UID_VAR(buf, t, actor, "actor", 0);
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
			ADD_UID_VAR(buf, t, actor, "actor", 0);
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


int drop_otrigger(obj_data *obj, char_data *actor) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int ret_val;

	if (!SCRIPT_CHECK(obj, OTRIG_DROP))
		return 1;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_DROP) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, actor, "actor", 0);
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
			ADD_UID_VAR(buf, t, actor, "actor", 0);
			ADD_UID_VAR(buf, t, victim, "victim", 0);
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

int ability_otrigger(char_data *actor, obj_data *obj, int abil) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (obj == NULL || abil == NO_ABIL)
		return 1;

	if (!SCRIPT_CHECK(obj, OTRIG_ABILITY))
		return 1;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_ABILITY) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;

			ADD_UID_VAR(buf, t, actor, "actor", 0);
			sprintf(buf, "%d", abil);
			add_var(&GET_TRIG_VARS(t), "ability", buf, 0);
			add_var(&GET_TRIG_VARS(t), "abilityname", ability_data[abil].name, 0);
			sdd.o = obj;
			return script_driver(&sdd, t, OBJ_TRIGGER, TRIG_NEW);
		}
	}

return 1;
}

int leave_otrigger(room_data *room, char_data *actor, int dir) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int temp, final = 1;
	obj_data *obj, *obj_next;

	for (obj = room->contents; obj; obj = obj_next) {
		obj_next = obj->next_content;
		if (!SCRIPT_CHECK(obj, OTRIG_LEAVE))
			continue;

		for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
			if (TRIGGER_CHECK(t, OTRIG_LEAVE) && (number(1, 100) <= GET_TRIG_NARG(t))) {
				union script_driver_data_u sdd;
				if (dir>=0 && dir < NUM_OF_DIRS)
					add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[dir], 0);
				else
					add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
				ADD_UID_VAR(buf, t, actor, "actor", 0);
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

int consume_otrigger(obj_data *obj, char_data *actor, int cmd) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];
	int ret_val;

	if (!SCRIPT_CHECK(obj, OTRIG_CONSUME))
		return 1;

	for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		if (TRIGGER_CHECK(t, OTRIG_CONSUME)) {
			union script_driver_data_u sdd;
			ADD_UID_VAR(buf, t, actor, "actor", 0);
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
			ADD_ROOM_UID_VAR(buf, t, room, "room", 0);
			sdd.r = room;
			if (!script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW)) {
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
			ADD_ROOM_UID_VAR(buf, t, room, "room", 0);
			sdd.r = room;
			script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
			break;
		}
	}
}

void random_wtrigger(room_data *room) {
	char buf[MAX_INPUT_LENGTH];
	trig_data *t;

	if (!SCRIPT_CHECK(room, WTRIG_RANDOM))
		return;

	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_RANDOM) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_ROOM_UID_VAR(buf, t, room, "room", 0);
			sdd.r = room;
			if (script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW))
				break;
		}
	}
}


int enter_wtrigger(room_data *room, char_data *actor, int dir) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (!SCRIPT_CHECK(room, WTRIG_ENTER))
		return 1;

	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_ENTER) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_ROOM_UID_VAR(buf, t, room, "room", 0);
			if (dir>=0 && dir < NUM_OF_DIRS)
				add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[rev_dir[dir]], 0);
			else
				add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
			ADD_UID_VAR(buf, t, actor, "actor", 0);
			sdd.r = room;
			return script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
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

		if (*GET_TRIG_ARG(t) == '*' || (mode == CMDTRG_EXACT && !str_cmp(cmd, GET_TRIG_ARG(t))) || (mode == CMDTRG_ABBREV && is_abbrev(cmd, GET_TRIG_ARG(t)))) {
			union script_driver_data_u sdd;
			ADD_ROOM_UID_VAR(buf, t, room, "room", 0);
			ADD_UID_VAR(buf, t, actor, "actor", 0);
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
			ADD_ROOM_UID_VAR(buf, t, room, "room", 0);
			ADD_UID_VAR(buf, t, actor, "actor", 0);
			add_var(&GET_TRIG_VARS(t), "speech", str, 0);
			sdd.r = room;
			script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
			break;
		}
	}
}

int drop_wtrigger(obj_data *obj, char_data *actor) {
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

			ADD_ROOM_UID_VAR(buf, t, room, "room", 0);
			ADD_UID_VAR(buf, t, actor, "actor", 0);
			ADD_UID_VAR(buf, t, obj, "object", 0);
			sdd.r = room;
			ret_val = script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
			if (obj->carried_by != actor)
				return 0;
			else
				return ret_val;
			break;
		}

	return 1;
}

int ability_wtrigger(char_data *actor, char_data *vict, obj_data *obj, int abil) {
	room_data *room;
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (!actor || abil == NO_ABIL || !SCRIPT_CHECK(IN_ROOM(actor), WTRIG_ABILITY))
		return 1;

	room = IN_ROOM(actor);
	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_ABILITY) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;

			ADD_ROOM_UID_VAR(buf, t, room, "room", 0);
			ADD_UID_VAR(buf, t, actor, "actor", 0);
			if (vict)
				ADD_UID_VAR(buf, t, vict, "victim", 0);
			if (obj)
				ADD_UID_VAR(buf, t, obj, "object", 0);
			sprintf(buf, "%d", abil);
			add_var(&GET_TRIG_VARS(t), "ability", buf, 0);
			add_var(&GET_TRIG_VARS(t), "abilityname", ability_data[abil].name, 0);
			sdd.r = room;
			return script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}


int leave_wtrigger(room_data *room, char_data *actor, int dir) {
	trig_data *t;
	char buf[MAX_INPUT_LENGTH];

	if (!SCRIPT_CHECK(room, WTRIG_LEAVE))
		return 1;

	for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
		if (TRIGGER_CHECK(t, WTRIG_LEAVE) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			ADD_ROOM_UID_VAR(buf, t, room, "room", 0);
			if (dir>=0 && dir < NUM_OF_DIRS)
				add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[dir], 0);
			else
				add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
			ADD_UID_VAR(buf, t, actor, "actor", 0);
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
			ADD_ROOM_UID_VAR(buf, t, room, "room", 0);
			add_var(&GET_TRIG_VARS(t), "cmd", (char *)cmd_door[subcmd], 0);
			if (dir>=0 && dir < NUM_OF_DIRS)
				add_var(&GET_TRIG_VARS(t), "direction", (char *)dirs[dir], 0);
			else
				add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
			ADD_UID_VAR(buf, t, actor, "actor", 0);
			sdd.r = room;
			return script_driver(&sdd, t, WLD_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
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

	cont = command_wtrigger(actor, cmd, argument, mode);	// world trigs
	if (!cont) {
		cont = command_mtrigger(actor, cmd, argument, mode);	// mob trigs
	}
	if (!cont) {
		cont = command_otrigger(actor, cmd, argument, mode);	// obj trigs
	}
	return cont;
}
