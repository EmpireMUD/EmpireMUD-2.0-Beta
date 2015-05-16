/* ************************************************************************
*   File: act.immortal.c                                  EmpireMUD 2.0b1 *
*  Usage: Player-level imm commands and other goodies                     *
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
#include "vnums.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "olc.h"
#include "dg_scripts.h"

/**
* Contents:
*   Helpers
*   Displays
*   Admin Util System
*   Instance Management
*   Set Data And Functions
*   Show Commands
*   Stat / Vstat
*   Vnum Searches
*   Commands
*/

// external vars
extern const char *action_bits[];
extern const char *affect_types[];
extern const char *affected_bits[];
extern const char *apply_types[];
extern const char *bld_on_flags[];
extern const char *bonus_bits[];
extern const char *climate_types[];
extern const char *dirs[];
extern const char *drinks[];
extern const char *genders[];
extern const char *grant_bits[];
extern const char *mapout_color_names[];
extern const char *room_aff_bits[];
extern const char *spawn_flags[];
extern const char *spawn_flags_short[];
extern const char *syslog_types[];

// external functions
extern struct instance_data *build_instance_loc(adv_data *adv, struct adventure_link_rule *rule, room_data *loc, int dir);	// instance.c
void check_autowiz(char_data *ch);
void clear_char_abilities(char_data *ch, int skill);
void delete_instance(struct instance_data *inst);	// instance.c
void get_icons_display(struct icon_data *list, char *save_buffer);
void get_interaction_display(struct interaction_item *list, char *save_buffer);
extern char *get_room_name(room_data *room, bool color);
void save_instances();
void save_whole_world();
void scale_mob_to_level(char_data *mob, int level);
void update_class(char_data *ch);

// locals
void instance_list_row(struct instance_data *inst, int number, char *save_buffer, size_t size);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Autostores one item. Contents are emptied out to where the object was.
*
* @param obj_dtaa *obj The item to autostore.
* @param empire_data *emp The empire to store it to.
* @param int island The islands to store it to.
*/
static void perform_autostore(obj_data *obj, empire_data *emp, int island) {	
	obj_data *temp, *next_temp;
	
	// try to store it
	if (emp) {
		if (OBJ_CAN_STORE(obj) || IS_COINS(obj)) {
			// try to store contents
			for (temp = obj->contains; temp; temp = next_temp) {
				next_temp = temp->next_content;
				perform_autostore(temp, emp, island);
			}
	
			// empty then just empty it
			empty_obj_before_extract(obj);

			if (IS_COINS(obj)) {
				increase_empire_coins(emp, real_empire(GET_COINS_EMPIRE_ID(obj)), GET_COINS_AMOUNT(obj));
			}
			else {
				add_to_empire_storage(emp, island, GET_OBJ_VNUM(obj), 1);
			}
			extract_obj(obj);
		}
	}
}


/**
* Sends a poofout/poofin message, moves a character, and looks at the room.
*
* @param char_data *ch The person.
* @param room_data *to_room The target location.
*/
static void perform_goto(char_data *ch, room_data *to_room) {
	char_data *t;
	
	if (!ch || !to_room) {
		return;
	}
	
	if (!IS_NPC(ch) && POOFOUT(ch)) {
		if (!strstr(POOFOUT(ch), "$n"))
			sprintf(buf, "$n %s", POOFOUT(ch));
		else
			strcpy(buf, POOFOUT(ch));
	}
	else {
		strcpy(buf, "$n disappears in a puff of smoke.");
	}

	for (t = ROOM_PEOPLE(IN_ROOM(ch)); t; t = t->next_in_room) {
		if (!REAL_NPC(t) && t != ch && CAN_SEE(t, ch)) {
			act(buf, TRUE, ch, 0, t, TO_VICT);
		}
	}

	char_from_room(ch);
	char_to_room(ch, to_room);
	GET_LAST_DIR(ch) = NO_DIR;

	if (!IS_NPC(ch) && POOFIN(ch)) {
		if (!strstr(POOFIN(ch), "$n"))
			sprintf(buf, "$n %s", POOFIN(ch));
		else
			strcpy(buf, POOFIN(ch));
	}
	else {
		strcpy(buf, "$n appears with an ear-splitting bang.");
	}

	for (t = ROOM_PEOPLE(IN_ROOM(ch)); t; t = t->next_in_room) {
		if (!REAL_NPC(t) && t != ch && CAN_SEE(t, ch)) {
			act(buf, TRUE, ch, 0, t, TO_VICT);
		}
	}
	
	look_at_room(ch);
	enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
}


/* Turns an immortal invisible */
void perform_immort_invis(char_data *ch, int level) {
	char_data *tch;

	if (IS_NPC(ch))
		return;

	for (tch = ROOM_PEOPLE(IN_ROOM(ch)); tch; tch = tch->next_in_room) {
		if (tch == ch)
			continue;
		if (GET_ACCESS_LEVEL(tch) >= GET_INVIS_LEV(ch) && GET_ACCESS_LEVEL(tch) < level)
			act("You blink and suddenly realize that $n is gone.", FALSE, ch, 0, tch, TO_VICT);
		if (GET_ACCESS_LEVEL(tch) < GET_INVIS_LEV(ch) && GET_ACCESS_LEVEL(tch) >= level)
			act("You suddenly realize that $n is standing beside you.", FALSE, ch, 0, tch, TO_VICT);
	}

	GET_INVIS_LEV(ch) = level;
	sprintf(buf, "Your invisibility level is %d.\r\n", level);
	send_to_char(buf, ch);
}


/* Immortal visible command, different than mortals' */
void perform_immort_vis(char_data *ch) {
	if (GET_INVIS_LEV(ch) == 0 && !AFF_FLAGGED(ch, AFF_HIDE)) {
		send_to_char("You are already fully visible.\r\n", ch);
		return;
	}

	GET_INVIS_LEV(ch) = 0;
	appear(ch);
	send_to_char("You are now fully visible.\r\n", ch);
}


/* Stop a person from snooping (cannot be used on the government) */
void stop_snooping(char_data *ch) {
	if (!ch->desc->snooping)
		send_to_char("You aren't snooping anyone.\r\n", ch);
	else {
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "GC: %s has stopped snooping %s", GET_REAL_NAME(ch), GET_REAL_NAME(ch->desc->snooping->character));
		send_to_char("You stop snooping.\r\n", ch);
		ch->desc->snooping->snoop_by = NULL;
		ch->desc->snooping = NULL;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

// returns TRUE if the user sees the target, FALSE if not
bool users_output(char_data *to, char_data *tch, descriptor_data *d, char *name_search, int low, int high, int rp) {
	extern const char *connected_types[];
	char_data *ch = tch;
	char levelname[20], *timeptr, idletime[10];
	char line[200], line2[220], state[30];
	const char *format = "%3d %s %-13s %-15s %-3s %-8s ";

	if (!ch && STATE(d) == CON_PLAYING) {
		if (d->original)
			ch = d->original;
		else if (!(ch = d->character))
			return FALSE;
	}

	if (ch) {
		if (*name_search && !is_abbrev(name_search, GET_NAME(ch)))
			return FALSE;
		if (!CAN_SEE_GLOBAL(to, ch) || GET_ACCESS_LEVEL(ch) < low || GET_ACCESS_LEVEL(ch) > high)
			return FALSE;
		if (rp && !PRF_FLAGGED(ch, PRF_RP))
			return FALSE;
		if (GET_INVIS_LEV(ch) > GET_ACCESS_LEVEL(to))
			return FALSE;

		sprintf(levelname, "%d", GET_ACCESS_LEVEL(ch));
	}
	else
		strcpy(levelname, "-");

	if (d) {
		timeptr = asctime(localtime(&d->login_time));
		timeptr += 11;
		*(timeptr + 8) = '\0';
	}
	else
		timeptr = str_dup("");

	if (ch && d && ch == d->original)
		strcpy(state, "Switched");
	else if (d)
		strcpy(state, connected_types[STATE(d)]);
	else
		strcpy(state, "Linkdead");

	if (ch)
		sprintf(idletime, "%3d", ch->char_specials.timer * SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN);
	else
		strcpy(idletime, "");

	sprintf(line, format, d ? d->desc_num : 0, levelname, (ch && GET_PC_NAME(ch)) ? GET_PC_NAME(ch) : (d && d->character && GET_PC_NAME(d->character)) ? GET_PC_NAME(d->character) : "UNDEFINED", state, idletime, timeptr);

	if (d && d->host && *d->host && (!ch || GET_ACCESS_LEVEL(ch) <= GET_ACCESS_LEVEL(to)))
		sprintf(line + strlen(line), "[%s]\r\n", d->host);
	else if (d)
		strcat(line, "[Hostname unknown]\r\n");
	else
		strcat(line, "\r\n");

	if (!d)
		sprintf(line2, "&c%s&0", line);
	else if (STATE(d) != CON_PLAYING)
		sprintf(line2, "&g%s&0", line);
	else
		strcpy(line2, line);
	strcpy(line, line2);

	send_to_char(line, to);
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// ADMIN UTIL SYSTEM ///////////////////////////////////////////////////////

#define ADMIN_UTIL(name)  void name(char_data *ch, char *argument)

ADMIN_UTIL(util_diminish);
ADMIN_UTIL(util_islandsize);
ADMIN_UTIL(util_playerdump);
ADMIN_UTIL(util_randtest);
ADMIN_UTIL(util_redo_islands);
ADMIN_UTIL(util_tool);
ADMIN_UTIL(util_fixresets);


struct {
	char *command;
	int level;
	void (*func)(char_data *ch, char *argument);
} admin_utils[] = {
	{ "diminish", LVL_START_IMM, util_diminish },
	{ "islandsize", LVL_START_IMM, util_islandsize },
	{ "playerdump", LVL_IMPL, util_playerdump },
	{ "randtest", LVL_CIMPL, util_randtest },
	{ "redoislands", LVL_CIMPL, util_redo_islands },
	{ "tool", LVL_IMPL, util_tool },
	{ "fixresets", LVL_CIMPL, util_fixresets },

	// last
	{ "\n", LVL_TOP+1, NULL }
};


ADMIN_UTIL(util_fixresets) {
	struct char_file_u chdata;
	char_data *vict;
	int pos, iter;
	bool is_file, save = FALSE;
	
	msg_to_char(ch, "Checking...\r\n");
	
	// ok, ready to roll
	for (pos = 0; pos <= top_of_p_table; ++pos) {
		// need chdata either way; check deleted here
		if (load_char(player_table[pos].name, &chdata) <= NOBODY || IS_SET(chdata.char_specials_saved.act, PLR_DELETED)) {
			continue;
		}
		
		if (!(vict = find_or_load_player(player_table[pos].name, &is_file))) {
			continue;
		}
		
		save = FALSE;
		
		for (iter = 0; iter < NUM_SKILLS; ++iter) {
			if (GET_FREE_SKILL_RESETS(vict, iter) > 5) {
				msg_to_char(ch, "%s had %d resets in %s.\r\n", GET_NAME(vict), GET_FREE_SKILL_RESETS(vict, iter), skill_data[iter].name);
				GET_FREE_SKILL_RESETS(vict, iter) = 0;
				save = TRUE;
			}
		}
		
		// save
		if (is_file) {
			if (save) {
				store_loaded_char(vict);
			}
			else {
				free_char(vict);
			}
			is_file = FALSE;
			vict = NULL;
		}
		else {
			SAVE_CHAR(vict);
		}
	}
	
	msg_to_char(ch, "Done.\r\n");
}


// secret implementor-only util for quick changes -- util tool
ADMIN_UTIL(util_tool) {
	void update_all_players(char_data *to_message);
	
	update_all_players(ch);	
	msg_to_char(ch, "Ok.\r\n");
}


ADMIN_UTIL(util_diminish) {
	double number, scale, result;
	
	half_chop(argument, buf1, buf2);
	
	if (!*buf1 || !*buf2) {
		msg_to_char(ch, "Usage: diminish <number> <scale>\r\n");
	}
	else if (!isdigit(*buf1) && *buf1 != '.') {
		msg_to_char(ch, "%s is not a valid floating point number.\r\n", buf1);
	}
	else if (!isdigit(*buf2) && *buf2 != '.') {
		msg_to_char(ch, "%s is not a valid floating point number.\r\n", buf2);
	}
	else {
		number = atof(buf1);
		scale = atof(buf2);
		result = diminishing_returns(number, scale);
		
		msg_to_char(ch, "Diminished value: %.2f\r\n", result);
	}
}


// util_islandsize: helper type
struct isf_type {
	int island;
	int count;
	UT_hash_handle hh;
};
int sort_isf_list(struct isf_type *a, struct isf_type *b) {
	return a->island - b->island;
}

ADMIN_UTIL(util_islandsize) {
	struct isf_type *isf, *next_isf, *list = NULL;
	char buf[MAX_STRING_LENGTH];
	room_data *room, *next_room;
	size_t size;
	int isle;
	
	HASH_ITER(world_hh, world_table, room, next_room) {
		if (GET_ROOM_VNUM(room) < MAP_SIZE) {
			isle = GET_ISLAND_ID(room);
			HASH_FIND_INT(list, &isle, isf);
			if (!isf) {
				CREATE(isf, struct isf_type, 1);
				isf->island = isle;
				isf->count = 0;
				HASH_ADD_INT(list, island, isf);
			}
			
			isf->count += 1;
		}
	}
	
	HASH_SORT(list, sort_isf_list);
	
	size = snprintf(buf, sizeof(buf), "Island sizes:\r\n");
	HASH_ITER(hh, list, isf, next_isf) {
		if (size < sizeof(buf)) {
			size += snprintf(buf + size, sizeof(buf) - size, "%2d: %d tile%s\r\n", isf->island, isf->count, PLURAL(isf->count));
		}
		
		// free as we go
		HASH_DEL(list, isf);
		free(isf);
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


ADMIN_UTIL(util_playerdump) {
	struct char_file_u chdata;
	char_data *plr;
	int pos;
	bool is_file;
	FILE *fl;
	
	if (!(fl = fopen("plrdump.txt", "w"))) {
		msg_to_char(ch, "Unable to open file for writing.\r\n");
		return;
	}
	
	fprintf(fl, "name\taccount\thours\thost\n");

	for (pos = 0; pos <= top_of_p_table; ++pos) {
		if (load_char(player_table[pos].name, &chdata) <= NOBODY || IS_SET(chdata.char_specials_saved.act, PLR_DELETED)) {
			continue;
		}
		
		if (!(plr = find_or_load_player(player_table[pos].name, &is_file))) {
			continue;
		}
		
		fprintf(fl, "%s\t%d\t%d\t%s\n", GET_NAME(plr), GET_ACCOUNT_ID(plr), (chdata.played / SECS_PER_REAL_HOUR), chdata.host);
		
		// done
		if (is_file && plr) {
			free_char(plr);
			is_file = FALSE;
			plr = NULL;
		}
	}

	msg_to_char(ch, "Ok.\r\n");
}


ADMIN_UTIL(util_randtest) {
	const int default_num = 1000, default_size = 100;
	
	int *values;
	int iter;
	int num, size, total, under, over;
	double avg;
	
	half_chop(argument, buf1, buf2);
	
	if (*buf1) {
		if (isdigit(*buf1)) {
			num = atoi(buf1);
		}
		else {
			msg_to_char(ch, "Usage: randtest [number of values] [size of values]\r\n");
			return;
		}
	}
	else {
		num = default_num;
	}
	
	if (*buf2) {
		if (isdigit(*buf2)) {
			size = atoi(buf2);
		}
		else {
			msg_to_char(ch, "Usage: randtest [number of values] [size of values]\r\n");
			return;
		}
	}
	else {
		size = default_size;
	}
	
	if (num > 10000 || num < 0 || size < 1) {
		msg_to_char(ch, "Invalid parameters.\r\n");
		return;
	}
	
	CREATE(values, int, num);
	total = 0;
	
	for (iter = 0; iter < num; ++iter) {
		values[iter] = number(1, size);
		total += values[iter];
	}
	
	avg = (double) total / num;
	
	under = over = 0;
	for (iter = 0; iter < num; ++iter) {
		if (values[iter] < (int) avg) {
			++under;
		}
		else {
			++over;
		}
	}
	
	msg_to_char(ch, "Generated %d numbers of size %d:\r\n", num, size);
	msg_to_char(ch, "Average result: %.2f\r\n", avg);
	msg_to_char(ch, "Results below average: %d\r\n", under);
	msg_to_char(ch, "Results above average: %d\r\n", over);
	
	free(values);
}


ADMIN_UTIL(util_redo_islands) {
	void number_the_islands(bool reset);
	
	skip_spaces(&argument);
	
	if (!*argument || str_cmp(argument, "confirm")) {
		msg_to_char(ch, "WARNING: This command will re-number all islands and may make some einvs unreachable.\r\n");
		msg_to_char(ch, "Usage: util redoislands confirm\r\n");
	}
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has renumbered islands", GET_NAME(ch));
		number_the_islands(TRUE);
		msg_to_char(ch, "Islands renumbered. Caution: empire inventories may now be in the wrong place.\r\n");
	}
}


// main interface for admin utils
ACMD(do_admin_util) {
	char util[MAX_INPUT_LENGTH], larg[MAX_INPUT_LENGTH];
	int iter, pos;
	bool found;
	
	half_chop(argument, util, larg);
	
	// find command?
	pos = NOTHING;
	for (iter = 0; admin_utils[iter].func != NULL && pos == NOTHING; ++iter) {
		if (GET_ACCESS_LEVEL(ch) >= admin_utils[iter].level && is_abbrev(util, admin_utils[iter].command)) {
			pos = iter;
		}
	}
	
	if (!*util) {
		msg_to_char(ch, "You have the following utilities: ");
		
		found = FALSE;
		for (iter = 0; admin_utils[iter].func != NULL; ++iter) {
			msg_to_char(ch, "%s%s", (found ? ", " : ""), admin_utils[iter].command);
			found = TRUE;
		}
		
		msg_to_char(ch, "\r\n");
	}
	else if (pos == NOTHING) {
		msg_to_char(ch, "Invalid utility.\r\n");
	}
	else {
		(admin_utils[pos].func)(ch, larg);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// INSTANCE MANAGEMENT /////////////////////////////////////////////////////


void do_instance_add(char_data *ch, char *argument) {
	extern bool can_instance(adv_data *adv);
	extern room_data *find_location_for_rule(struct adventure_link_rule *rule, int *which_dir);

	struct adventure_link_rule *rule;
	bool found = FALSE;
	int dir = NO_DIR;
	room_data *loc;
	adv_vnum vnum;
	adv_data *adv;
	
	if (!*argument || !isdigit(*argument) || (vnum = atoi(argument)) < 0 || !(adv = adventure_proto(vnum))) {
		msg_to_char(ch, "Invalid adventure vnum '%s'.\r\n", *argument ? argument : "<blank>");
		return;
	}
	
	if (!can_instance(adv)) {
		msg_to_char(ch, "Unable to add an instance of that adventure zone.\r\n");
		return;
	}
	
	for (rule = GET_ADV_LINKING(adv); rule; rule = rule->next) {
		if ((loc = find_location_for_rule(rule, &dir))) {
			// make it so!
			if (build_instance_loc(adv, rule, loc, dir)) {
				found = TRUE;
				save_instances();
				break;
			}
		}
	}
	
	if (found) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s created an instance of %s at %s", GET_REAL_NAME(ch), GET_ADV_NAME(adv), room_log_identifier(loc));
		msg_to_char(ch, "Instance added at %s.\r\n", room_log_identifier(loc));
	}
	else {
		msg_to_char(ch, "Unable to find a location to link that adventure.\r\n");
	}
}


void do_instance_delete(char_data *ch, char *argument) {
	void delete_instance(struct instance_data *inst);
	
	struct instance_data *inst;
	room_data *loc;
	int num;
	
	if (!*argument || !isdigit(*argument) || (num = atoi(argument)) < 1) {
		msg_to_char(ch, "Invalid instance number '%s'.\r\n", *argument ? argument : "<blank>");
		return;
	}
	
	for (inst = instance_list; inst; inst = inst->next) {
		if (--num == 0) {
			if ((loc = inst->location)) {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s deleted an instance of %s at %s", GET_REAL_NAME(ch), GET_ADV_NAME(inst->adventure), room_log_identifier(loc));
			}
			else {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s deleted an instance of %s at unknown location", GET_REAL_NAME(ch), GET_ADV_NAME(inst->adventure));
			}
			msg_to_char(ch, "Instance of %s deleted.\r\n", GET_ADV_NAME(inst->adventure));
			delete_instance(inst);
			save_instances();
			break;
		}
	}
	
	if (num > 0) {
		msg_to_char(ch, "Invalid instance number %d.\r\n", atoi(argument));
	}
}


void do_instance_delete_all(char_data *ch, char *argument) {
	struct instance_data *inst, *next_inst;
	adv_vnum vnum;
	adv_data *adv;
	int count = 0;
	
	if (!*argument || !isdigit(*argument) || (vnum = atoi(argument)) < 0 || !(adv = adventure_proto(vnum))) {
		msg_to_char(ch, "Invalid adventure vnum '%s'.\r\n", *argument ? argument : "<blank>");
		return;
	}
	
	for (inst = instance_list; inst; inst = next_inst) {
		next_inst = inst->next;
		
		if (inst->adventure == adv) {
			++count;
			delete_instance(inst);
		}
	}
	
	save_instances();
	
	if (count > 0) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s deleted %d instances of %s", GET_REAL_NAME(ch), count, GET_ADV_NAME(adv));
		msg_to_char(ch, "%d instances of '%s' deleted.\r\n", count, GET_ADV_NAME(adv));
	}
	else {
		msg_to_char(ch, "No instances of '%s' found.\r\n", GET_ADV_NAME(adv));
	}
}


void do_instance_list(char_data *ch, char *argument) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	struct instance_data *inst;
	adv_data *adv = NULL;
	adv_vnum vnum;
	int num = 0, count = 0;
	
	if (!ch->desc) {
		return;
	}
	
	if (*argument && (!isdigit(*argument) || (vnum = atoi(argument)) < 0 || !(adv = adventure_proto(vnum)))) {
		msg_to_char(ch, "Invalid adventure vnum '%s'.\r\n", argument);
		return;
	}
	
	*buf = '\0';
	
	for (inst = instance_list; inst; inst = inst->next) {
		// num is out of the total instances, not just ones shown
		++num;
		
		if (!adv || adv == inst->adventure) {
			++count;
			instance_list_row(inst, num, line, sizeof(line));
			
			if (snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s", line) < strlen(line)) {
				break;
			}
		}
	}
	
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%d total instances shown\r\n", count);
	page_string(ch->desc, buf, TRUE);
}


void do_instance_nearby(char_data *ch, char *argument) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	struct instance_data *inst;
	int num = 0, count = 0, distance = 50, size;
	room_data *inst_loc, *loc = get_map_location_for(IN_ROOM(ch));
	
	if (*argument && (!isdigit(*argument) || (distance = atoi(argument)) < 0)) {
		msg_to_char(ch, "Invalid distance '%s'.\r\n", argument);
		return;
	}
	
	size = snprintf(buf, sizeof(buf), "Instanes within %d tiles:\r\n", distance);
	
	for (inst = instance_list; inst; inst = inst->next) {
		++num;
		
		inst_loc = inst->location;
		if (inst_loc && !INSTANCE_FLAGGED(inst, INST_COMPLETED) && compute_distance(loc, inst_loc) <= distance) {
			++count;
			instance_list_row(inst, num, line, sizeof(line));
			size += snprintf(buf + size, sizeof(buf) - size, "%s", line);
			if (size >= sizeof(buf)) {
				break;
			}
		}
	}
	
	snprintf(buf + size, sizeof(buf) - size, "%d total instances shown\r\n", count);
	page_string(ch->desc, buf, TRUE);
}


void do_instance_reset(char_data *ch, char *argument) {
	extern struct instance_data *find_instance_by_room(room_data *room);
	void reset_instance(struct instance_data *inst);

	struct instance_data *inst;
	room_data *loc = NULL;
	int num;
	
	if (*argument) {
		if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid instance number '%s'.\r\n", *argument ? argument : "<blank>");
			return;
		}
	
		for (inst = instance_list; inst; inst = inst->next) {
			if (--num == 0) {
				loc = inst->location;
				reset_instance(inst);
				break;
			}
		}
		
		if (num > 0) {
			msg_to_char(ch, "Invalid instance number %d.\r\n", atoi(argument));
			return;
		}
	}
	else {
		// no argument
		if (!(inst = find_instance_by_room(IN_ROOM(ch)))) {
			msg_to_char(ch, "You are not in or near an adventure zone instance.\r\n");
			return;
		}

		loc = inst->location;
		reset_instance(inst);
	}
	
	if (loc) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s forced a reset of an instance of %s at %s", GET_REAL_NAME(ch), GET_ADV_NAME(inst->adventure), room_log_identifier(loc));
	}
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s forced a reset of an instance of %s at unknown location", GET_REAL_NAME(ch), GET_ADV_NAME(inst->adventure));
	}
	send_config_msg(ch, "ok_string");
}


/**
* Fetches a single row of instance data (usually for do_instance_list or
* do_instance_nearby).
*
* @param struct instance_data *inst The instance to list.
* @param int number The number to show it as (numbered list).
* @param char *save_buffer A buffer to save the output to.
* @param size_t size Size of the buffer.
*/
void instance_list_row(struct instance_data *inst, int number, char *save_buffer, size_t size) {
	extern const char *instance_flags[];
	
	char flg[256], info[256];

	if (inst->level > 0) {
		sprintf(info, " L%d", inst->level);
	}
	else {
		*info = '\0';
	}

	sprintbit(inst->flags, instance_flags, flg, TRUE);
	snprintf(save_buffer, size, "%3d. [%5d] %s [%d] (%d, %d)%s %s\r\n", number, GET_ADV_VNUM(inst->adventure), GET_ADV_NAME(inst->adventure), inst->location ? GET_ROOM_VNUM(inst->location) : NOWHERE, inst->location ? X_COORD(inst->location) : NOWHERE, inst->location ? Y_COORD(inst->location) : NOWHERE, info, inst->flags != NOBITS ? flg : "");
}


void do_instance_test(char_data *ch, char *argument) {
	struct adventure_link_rule rule;
	bool found = FALSE;
	adv_vnum vnum;
	adv_data *adv;
	
	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "An instance is already linked here.\r\n");
		return;
	}
	
	if (!*argument || !isdigit(*argument) || (vnum = atoi(argument)) < 0 || !(adv = adventure_proto(vnum))) {
		msg_to_char(ch, "Invalid adventure vnum '%s'.\r\n", *argument ? argument : "<blank>");
		return;
	}
	
	// fake a link rule for this room
	rule.type = ADV_LINK_PORTAL_WORLD;
	rule.flags = NOBITS;
	rule.value = GET_SECT_VNUM(SECT(IN_ROOM(ch)));
	rule.portal_in = o_PORTAL;
	rule.portal_out = o_PORTAL;
	rule.dir = DIR_RANDOM;
	rule.bld_on = NOBITS;
	rule.bld_facing = NOBITS;
	rule.next = NULL;
	
	if (build_instance_loc(adv, &rule, IN_ROOM(ch), DIR_RANDOM)) {
		found = TRUE;
		save_instances();
	}
	
	if (found) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s created a test instance of %s at %s", GET_REAL_NAME(ch), GET_ADV_NAME(adv), room_log_identifier(IN_ROOM(ch)));
		msg_to_char(ch, "Test instance added at %s.\r\n", room_log_identifier(IN_ROOM(ch)));
	}
	else {
		msg_to_char(ch, "Unable to link that adventure here.\r\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// SET DATA AND FUNCTIONS //////////////////////////////////////////////////

#define PC   1
#define NPC  2
#define BOTH 3

#define MISC	0
#define BINARY	1
#define NUMBER	2

#define SET_OR_REMOVE(flagset, flags) { \
	if (on) SET_BIT(flagset, flags); \
	else if (off) REMOVE_BIT(flagset, flags); }

#define RANGE(low, high) (value = MAX((low), MIN((high), (value))))

#define SET_CASE(str)		(!str_cmp(str, set_fields[mode].cmd))


/* The set options available */
struct set_struct {
	const char *cmd;
	const char level;
	const char pcnpc;
	const char type;
	} set_fields[] = {
		{ "invstart", 	LVL_START_IMM, 	PC, 	BINARY },
		{ "title",		LVL_START_IMM, 	PC, 	MISC },
		{ "notitle",	LVL_START_IMM,	PC, 	BINARY },

		{ "health",		LVL_START_IMM, 	NPC, 	NUMBER },
		{ "move",		LVL_START_IMM, 	NPC, 	NUMBER },
		{ "mana",		LVL_START_IMM, 	NPC, 	NUMBER },
		{ "blood",		LVL_START_IMM, 	NPC, 	NUMBER },

		{ "coins",		LVL_START_IMM,	PC,		MISC },
		{ "frozen",		LVL_START_IMM,	PC, 	BINARY },
		{ "drunk",		LVL_START_IMM, 	BOTH, 	MISC },
		{ "hunger",		LVL_START_IMM, 	BOTH, 	MISC },
		{ "thirst",		LVL_START_IMM, 	BOTH, 	MISC },
		{ "level",		LVL_CIMPL, 	PC, 	NUMBER },
		{ "siteok",		LVL_START_IMM, 	PC, 	BINARY },
		{ "deleted", 	LVL_CIMPL, 	PC, 	BINARY },
		{ "nowizlist", 	LVL_START_IMM, 	PC, 	BINARY },
		{ "loadroom", 	LVL_START_IMM, 	PC, 	MISC },
		{ "color",		LVL_START_IMM, 	PC, 	BINARY },
		{ "password",	LVL_CIMPL, 	PC, 	MISC },
		{ "nodelete", 	LVL_CIMPL, 	PC, 	BINARY },
		{ "noidleout",	LVL_START_IMM,	PC,		BINARY },
		{ "sex", 		LVL_START_IMM, 	BOTH, 	MISC },
		{ "age",		LVL_START_IMM,	BOTH,	NUMBER },
		{ "lastname",	LVL_START_IMM,	PC,		MISC },
		{ "muted",		LVL_START_IMM,	PC, 	BINARY },
		{ "name",		LVL_CIMPL,	PC,		MISC },
		{ "ipmask",		LVL_START_IMM,	PC,		BINARY },
		{ "multiok",	LVL_START_IMM,	PC,		BINARY },
		{ "vampire",	LVL_START_IMM,	PC, 	BINARY },
		{ "account",	LVL_START_IMM,	PC,		MISC },
		{ "bonus",		LVL_START_IMM,	PC,		MISC },
		{ "grants",		LVL_CIMPL,	PC,		MISC },
		{ "skill", LVL_START_IMM, PC, MISC },

		{ "strength",	LVL_START_IMM,	BOTH,	NUMBER },
		{ "dexterity",	LVL_START_IMM,	BOTH,	NUMBER },
		{ "charisma",	LVL_START_IMM,	BOTH,	NUMBER },
		{ "greatness",	LVL_START_IMM,	BOTH,	NUMBER },
		{ "intelligence",LVL_START_IMM,	BOTH,	NUMBER },
		{ "wits",		LVL_START_IMM,	BOTH,	NUMBER },
		
		{ "scale", LVL_START_IMM, NPC, NUMBER },

		{ "\n", 0, BOTH, MISC }
};


/* All setting is done here, for simplicity */
int perform_set(char_data *ch, char_data *vict, int mode, char *val_arg) {
	/* Externs */
	extern int _parse_name(char *arg, char *name);
	extern int Valid_Name(char *newname);
	void make_vampire(char_data *ch, bool lore);
	int new_account_id();

	int i, iter, nr, on = 0, off = 0, value = 0;
	empire_data *emp;
	room_vnum rvnum;
	char output[MAX_STRING_LENGTH], oldname[MAX_INPUT_LENGTH], newname[MAX_INPUT_LENGTH];
	char_data *alt;
	bool file = FALSE;

	*output = '\0';

	/* Check to make sure all the levels are correct */
	if (GET_ACCESS_LEVEL(ch) != LVL_IMPL) {
		if (!IS_NPC(vict) && GET_ACCESS_LEVEL(ch) <= GET_ACCESS_LEVEL(vict) && vict != ch) {
			send_to_char("Maybe that's not such a great idea...\r\n", ch);
			return (0);
		}
	}
	if (GET_ACCESS_LEVEL(ch) < set_fields[mode].level) {
		send_to_char("You are not godly enough for that!\r\n", ch);
		return (0);
	}

	/* Make sure the PC/NPC is correct */
	if (IS_NPC(vict) && !(set_fields[mode].pcnpc & NPC)) {
		send_to_char("You can't do that to a beast!\r\n", ch);
		return (0);
	}
	else if (!IS_NPC(vict) && !(set_fields[mode].pcnpc & PC)) {
		send_to_char("That can only be done to a beast!\r\n", ch);
		return (0);
	}

	/* Find the value of the argument */
	if (set_fields[mode].type == BINARY) {
		if (!str_cmp(val_arg, "on") || !str_cmp(val_arg, "yes"))
			on = 1;
		else if (!str_cmp(val_arg, "off") || !str_cmp(val_arg, "no"))
			off = 1;
		if (!(on || off)) {
			send_to_char("Value must be 'on' or 'off'.\r\n", ch);
			return (0);
		}
		sprintf(output, "%s %s for %s.", set_fields[mode].cmd, ONOFF(on), GET_NAME(vict));
	}
	else if (set_fields[mode].type == NUMBER) {
		value = atoi(val_arg);
		sprintf(output, "%s's %s set to %d.", GET_NAME(vict), set_fields[mode].cmd, value);
	}
	else
		strcpy(output, "Okay.");


	if SET_CASE("invstart") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_INVSTART);
	}
	else if SET_CASE("title") {
		set_title(vict, val_arg);
		sprintf(output, "%s's title is now: %s", GET_NAME(vict), GET_TITLE(vict));
	}

	else if SET_CASE("health") {
		vict->points.current_pools[HEALTH] = RANGE(0, GET_MAX_HEALTH(vict));
		affect_total(vict);
	}
	else if SET_CASE("move") {
		vict->points.current_pools[MOVE] = RANGE(0, GET_MAX_MOVE(vict));
		affect_total(vict);
	}
	else if SET_CASE("mana") {
		vict->points.current_pools[MANA] = RANGE(0, GET_MAX_MANA(vict));
		affect_total(vict);
	}
	else if SET_CASE("blood") {
		vict->points.current_pools[BLOOD] = RANGE(0, GET_MAX_BLOOD(vict));
		affect_total(vict);
	}

	else if SET_CASE("strength") {
		vict->real_attributes[STRENGTH] = RANGE(1, att_max(vict));
		affect_total(vict);
	}
	else if SET_CASE("dexterity") {
		vict->real_attributes[DEXTERITY] = RANGE(1, att_max(vict));
		affect_total(vict);
	}
	else if SET_CASE("charisma") {
		vict->real_attributes[CHARISMA] = RANGE(1, att_max(vict));
		affect_total(vict);
	}
	else if SET_CASE("greatness") {
		// only update greatness if ch is in a room (playing)
		emp = GET_LOYALTY(vict);
		if (emp && IN_ROOM(vict)) {
			EMPIRE_GREATNESS(emp) -= vict->real_attributes[GREATNESS];
		}

		// set the actual greatness
		vict->real_attributes[GREATNESS] = RANGE(1, att_max(vict));
		
		// add greatness back
		if (emp && IN_ROOM(vict)) {
			EMPIRE_GREATNESS(emp) += vict->real_attributes[GREATNESS];
		}
		
		affect_total(vict);
		if (emp) {
			read_empire_members(emp, FALSE);
		}
	}
	else if SET_CASE("intelligence") {
		vict->real_attributes[INTELLIGENCE] = RANGE(1, att_max(vict));
		affect_total(vict);
	}
	else if SET_CASE("wits") {
		vict->real_attributes[WITS] = RANGE(1, att_max(vict));
		affect_total(vict);
	}

	else if SET_CASE("frozen") {
		if (ch == vict && on) {
			send_to_char("Better not -- could be a long winter!\r\n", ch);
			return (0);
		}
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_FROZEN);
	}
	else if SET_CASE("muted") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_MUTED);
	}
	else if SET_CASE("notitle") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NOTITLE);
	}
	else if SET_CASE("ipmask") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_IPMASK);
	}
	else if SET_CASE("multiok") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_MULTIOK);
	}
	else if SET_CASE("vampire") {
		if (IS_VAMPIRE(vict)) {
			GET_BLOOD(vict) = GET_MAX_BLOOD(vict);
			REMOVE_BIT(PLR_FLAGS(vict), PLR_VAMPIRE);
		}
		else {
			make_vampire(vict, TRUE);
		}
	}
	else if SET_CASE("hunger") {
		if (!str_cmp(val_arg, "off")) {
			GET_COND(vict, FULL) = UNLIMITED;
			sprintf(output, "%s's hunger now off.", GET_NAME(vict));
		}
		else if (is_number(val_arg)) {
			value = atoi(val_arg);
			RANGE(0, MAX_CONDITION);
			GET_COND(vict, FULL) = value;
			sprintf(output, "%s's hunger set to %d.", GET_NAME(vict), value);
		}
		else {
			send_to_char("Must be 'off' or a value from 0 to 24.\r\n", ch);
			return (0);
		}
	}
	else if SET_CASE("drunk") {
		if (!str_cmp(val_arg, "off")) {
			GET_COND(vict, DRUNK) = UNLIMITED;
			sprintf(output, "%s's drunk now off.", GET_NAME(vict));
		}
		else if (is_number(val_arg)) {
			value = atoi(val_arg);
			RANGE(0, MAX_CONDITION);
			GET_COND(vict, DRUNK) = value;
			sprintf(output, "%s's drunk set to %d.", GET_NAME(vict), value);
		}
		else {
			send_to_char("Must be 'off' or a value from 0 to 24.\r\n", ch);
			return (0);
		}
	}
	else if SET_CASE("thirst") {
		if (!str_cmp(val_arg, "off")) {
			GET_COND(vict, THIRST) = UNLIMITED;
			sprintf(output, "%s's thirst now off.", GET_NAME(vict));
		}
		else if (is_number(val_arg)) {
			value = atoi(val_arg);
			RANGE(0, MAX_CONDITION);
			GET_COND(vict, THIRST) = value;
			sprintf(output, "%s's thirst set to %d.", GET_NAME(vict), value);
		}
		else {
			send_to_char("Must be 'off' or a value from 0 to 24.\r\n", ch);
			return (0);
		}
	}
	else if SET_CASE("level") {
		if (value > GET_ACCESS_LEVEL(ch) || value > LVL_IMPL) {
			send_to_char("You can't do that.\r\n", ch);
			return (0);
		}
		
		if (!IS_NPC(vict) && GET_ACCESS_LEVEL(vict) < LVL_START_IMM && value >= LVL_START_IMM) {
			SET_BIT(PRF_FLAGS(vict), PRF_HOLYLIGHT | PRF_ROOMFLAGS | PRF_NOHASSLE);
		
			// turn on all syslogs
			for (iter = 0; *syslog_types[iter] != '\n'; ++iter) {
				SYSLOG_FLAGS(vict) |= BIT(iter);
			}
		}
		
		RANGE(0, LVL_IMPL);
		vict->player.access_level = (byte) value;
		if (!IS_NPC(vict))
			GET_IMMORTAL_LEVEL(vict) = IS_IMMORTAL(vict) ? LVL_TOP - GET_ACCESS_LEVEL(vict) : -1;
		SAVE_CHAR(vict);
		check_autowiz(ch);
	}
	else if SET_CASE("siteok") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_SITEOK);
	}
	else if SET_CASE("deleted") {
		void delete_player_character(char_data *ch);
		
		if (on || !PLR_FLAGGED(vict, PLR_DELETED)) {
			if (PLR_FLAGGED(vict, PLR_NODELETE)) {
				msg_to_char(ch, "You can't set that on a no-delete player.\r\n");
				return 0;
			}
			
			// DELETE THEM!
			SET_BIT(PLR_FLAGS(vict), PLR_DELETED);
			delete_player_character(vict);
		}
		else {
			REMOVE_BIT(PLR_FLAGS(vict), PLR_DELETED);
		}
	}
	else if SET_CASE("nowizlist") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NOWIZLIST);
	}
	else if SET_CASE("loadroom") {
		if (!str_cmp(val_arg, "off")) {
			REMOVE_BIT(PLR_FLAGS(vict), PLR_LOADROOM);
			sprintf(output, "Loadroom for %s off.", GET_NAME(vict));
		}
		else if (is_number(val_arg)) {
			rvnum = atoi(val_arg);
			if (real_room(rvnum)) {
				SET_BIT(PLR_FLAGS(vict), PLR_LOADROOM);
				GET_LOADROOM(vict) = rvnum;
				sprintf(output, "%s will enter at room #%d.", GET_NAME(vict), GET_LOADROOM(vict));
			}
			else {
				send_to_char("That room does not exist!\r\n", ch);
				return (0);
			}
		}
		else {
			send_to_char("Must be 'off' or a room's virtual number.\r\n", ch);
			return (0);
		}
	}
	else if SET_CASE("color") {
		SET_OR_REMOVE(PRF_FLAGS(vict), PRF_COLOR);
	}
	else if SET_CASE("password") {
		if (GET_ACCESS_LEVEL(vict) >= LVL_IMPL) {
			send_to_char("You cannot change that.\r\n", ch);
			return (0);
		}
		strncpy(GET_PASSWD(vict), CRYPT(val_arg, PASSWORD_SALT), MAX_PWD_LENGTH);
		*(GET_PASSWD(vict) + MAX_PWD_LENGTH) = '\0';
		sprintf(output, "Password for %s changed.", GET_NAME(vict));
	}
	else if SET_CASE("nodelete") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NODELETE);
	}
	else if SET_CASE("noidleout") {
		SET_OR_REMOVE(PRF_FLAGS(vict), PRF_NO_IDLE_OUT);
	}
	else if SET_CASE("sex") {
		if ((i = search_block(val_arg, genders, FALSE)) == NOTHING) {
			send_to_char("Must be 'male', 'female', or 'neutral'.\r\n", ch);
			return (0);
		}
		GET_REAL_SEX(vict) = i;
		sprintf(output, "%s's sex is now %s", GET_NAME(vict), genders[(int) GET_REAL_SEX(vict)]);
	}
	else if SET_CASE("age") {
		if (value < 2 || value > 95) {	/* Arbitrary limits. */
			send_to_char("Ages 2 to 95 accepted.\r\n", ch);
			return (0);
		}
		vict->player.time.birth = time(0) - ((value - 17) * SECS_PER_MUD_YEAR);
	}
	else if SET_CASE("lastname") {
		if (!*val_arg) {
			msg_to_char(ch, "Set the last name to what (or \"off\")?\r\n");
			return 0;
		}
		else if (!str_cmp(val_arg, "off")) {
			if (GET_LASTNAME(vict) != NULL)
				free(GET_LASTNAME(vict));
			GET_LASTNAME(vict) = NULL;
    		sprintf(output, "%s's no longer has a last name", GET_NAME(vict));
		}
    	else {
			if (GET_LASTNAME(vict) != NULL)
				free(GET_LASTNAME(vict));
			GET_LASTNAME(vict) = str_dup(val_arg);
    		sprintf(output, "%s's last name is now: %s", GET_NAME(vict), GET_LASTNAME(vict));
		}
	}
	else if SET_CASE("bonus") {
		void apply_bonus_trait(char_data *ch, bitvector_t trait, bool add);

		bitvector_t diff, new, old = GET_BONUS_TRAITS(vict);
		new = GET_BONUS_TRAITS(vict) = olc_process_flag(ch, val_arg, "bonus", NULL, bonus_bits, GET_BONUS_TRAITS(vict));
		
		// this indicates a change
		if (new != old) {
			sprintbit(new, bonus_bits, buf, TRUE);
			sprintf(output, "%s now has bonus traits: %s", GET_NAME(vict), buf);
			
			// apply them
			if ((diff = new & ~old)) {
				apply_bonus_trait(vict, diff, TRUE);
			}
			if ((diff = old & ~new)) {
				// removing?
				apply_bonus_trait(vict, diff, FALSE);
			}
		}
		else {
			// no change
			return 0;
		}
	}
	else if SET_CASE("grants") {
		bitvector_t new, old = GET_GRANT_FLAGS(vict);
		new = GET_GRANT_FLAGS(vict) = olc_process_flag(ch, val_arg, "grants", NULL, grant_bits, GET_GRANT_FLAGS(vict));
		
		// this indicates a change
		if (new != old) {
			sprintbit(new, grant_bits, buf, TRUE);
			sprintf(output, "%s now has grants: %s", GET_NAME(vict), buf);
		}
		else {
			// no change
			return 0;
		}
	}
	else if SET_CASE("coins") {
		char typearg[MAX_INPUT_LENGTH], *amtarg;
		struct coin_data *coin;
		empire_data *type;
		int amt;
		
		// set <name> coins <type> <amount>
		amtarg = any_one_arg(val_arg, typearg);
		skip_spaces(&amtarg);
		
		if (!*typearg || !*amtarg || !isdigit(*amtarg)) {
			msg_to_char(ch, "Usage: set <name> coins <type> <amount>\r\n");
			return 0;
		}
		else if (!(type = get_empire_by_name(typearg)) && !is_abbrev(typearg, "other") && !is_abbrev(typearg, "miscellaneous") && !is_abbrev(typearg, "simple")) {
			msg_to_char(ch, "Invalid coin type. Specify an empire name or 'miscellaneous'.\r\n");
			return 0;
		}
		else if ((amt = atoi(amtarg)) < 0) {
			msg_to_char(ch, "You can't set negative coins.\r\n");
		}
		else {
			// prepare message first (amt changes)
			sprintf(output, "%s now has %s", GET_NAME(vict), money_amount(type, amt));
			
			// we're going to use increase_coins, so first see if they already had some
			if ((coin = find_coin_entry(GET_PLAYER_COINS(vict), type))) {
				amt -= coin->amount;
			}
			increase_coins(vict, type, amt);
		}
	}
	else if SET_CASE("skill") {
		extern int find_skill_by_name(char *name);
	
		char skillname[MAX_INPUT_LENGTH], *skillval;
		int level = -1, old_level, skill;
		
		// set <name> skill "<name>" <level>
		skillval = any_one_word(val_arg, skillname);
		skip_spaces(&skillval);
		
		if (!*skillname || !*skillval) {
			msg_to_char(ch, "Usage: set <name> skill <skill> <level>\r\n");
			return 0;
		}
		if (!isdigit(*skillval) || (level = atoi(skillval)) < 0 || level > CLASS_SKILL_CAP) {
			msg_to_char(ch, "You must choose a level between 0 and %d.\r\n", CLASS_SKILL_CAP);
			return 0;
		}
		else if ((skill = find_skill_by_name(skillname)) == NO_SKILL) {
			msg_to_char(ch, "Unknown skill '%s'.\r\n", skillname);
			return 0;
		}

		// victory
		old_level = GET_SKILL(vict, skill);
		set_skill(vict, skill, level);
		if (old_level > level) {
			clear_char_abilities(vict, skill);
		}
		update_class(vict);
		sprintf(output, "%s's %s set to %d", GET_NAME(vict), skill_data[skill].name, level);
	}

	else if SET_CASE("account") {		
		if (!str_cmp(val_arg, "none")) {
			sprintf(output, "%s is %s associated with an account", GET_NAME(vict), (GET_ACCOUNT_ID(vict) > 0) ? "no longer" : "not");
			GET_ACCOUNT_ID(vict) = 0;
		}
		else {
			// load 2nd player
			if ((alt = find_or_load_player(val_arg, &file))) {
				sprintf(output, "%s is now associated with %s's account", GET_NAME(vict), GET_NAME(alt));
				
				// does 2nd player have an account already? if not, make one
				if (GET_ACCOUNT_ID(alt) == 0) {
					GET_ACCOUNT_ID(alt) = new_account_id();
					GET_ACCOUNT_ID(vict) = GET_ACCOUNT_ID(alt);

					if (file) {
						store_loaded_char(alt);
						file = FALSE;
						alt = NULL;
					}
					else {
						SAVE_CHAR(alt);
					}
				}
				else {
					// already has acct
					GET_ACCOUNT_ID(vict) = GET_ACCOUNT_ID(alt);
				}
				
				if (file && alt) {
					free_char(alt);
				}
			}
			else {
				msg_to_char(ch, "Unable to associate account: no such player '%s'.\r\n", val_arg);
				return 0;
			}
		}
		
		if ((emp = GET_LOYALTY(vict))) {
			read_empire_members(emp, FALSE);
		}
	}

	else if SET_CASE("name") {
		SAVE_CHAR(vict);
		one_argument(val_arg, buf);
		if (_parse_name(buf, newname) || fill_word(newname) || strlen(newname) > MAX_NAME_LENGTH || strlen(newname) < 2 || !Valid_Name(newname)) {
			msg_to_char(ch, "Invalid name.\r\n");
			return 0;
		}
		nr = NOBODY;
		for (i = 0; i <= top_of_p_table; i++) {
			if (!str_cmp(player_table[i].name, newname)) {
				msg_to_char(ch, "That name is already taken.\r\n");
				return 0;
			}
			if (!str_cmp(player_table[i].name, GET_NAME(vict))) {
				if (nr >= 0) {
					msg_to_char(ch, "WARNING: possible pfile corruption, more than one entry with that name.\r\n");
				}
				else {
					nr = i;
				}
			}
		}
		if (nr == NOBODY) {
			msg_to_char(ch, "WARNING: That character does not have a record in the pfile.\r\nName not changed.\r\n");
			return 0;
		}
		strcpy(oldname, GET_NAME(vict));
		if (player_table[nr].name)
			free(player_table[nr].name);
		CREATE(player_table[nr].name, char, strlen(newname) + 1);
		for (i = 0; (*(player_table[nr].name + i) = *(newname + i)); i++);

		if (GET_PC_NAME(vict))
			free(GET_PC_NAME(vict));
		GET_PC_NAME(vict) = strdup(CAP(newname));
		
		// TODO file renames could be moved somewhere useful
		// TODO also, this could be a lot cleaner and use remove() instead of system/rm
		get_filename(oldname, buf1, CRASH_FILE);
		get_filename(GET_NAME(vict), buf2, CRASH_FILE);
		sprintf(buf, "rm -f %s", buf2);
		system(buf);
		rename(buf1, buf2);
		get_filename(oldname, buf1, ALIAS_FILE);
		get_filename(GET_NAME(vict), buf2, ALIAS_FILE);
		sprintf(buf, "rm -f %s", buf2);
		system(buf);
		rename(buf1, buf2);
		get_filename(oldname, buf1, LORE_FILE);
		get_filename(GET_NAME(vict), buf2, LORE_FILE);
		sprintf(buf, "rm -f %s", buf2);
		system(buf);
		rename(buf1, buf2);
		
		SAVE_CHAR(vict);
		sprintf(output, "%s's name changed to %s.", oldname, GET_NAME(vict));
	}
	
	else if SET_CASE("scale") {
		// adjust here to log the right number
		value = MAX(GET_MIN_SCALE_LEVEL(vict), value);
		value = MIN(GET_MAX_SCALE_LEVEL(vict), value);
		scale_mob_to_level(vict, value);
	}
	
	// this goes last, after all SET_CASE statements
	else {
		send_to_char("Can't set that!\r\n", ch);
		return (0);
	}

	if (*output) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s used set: %s", GET_REAL_NAME(ch), output);
		strcat(output, "\r\n");
		send_to_char(CAP(output), ch);
	}
	return (1);
}


 //////////////////////////////////////////////////////////////////////////////
//// SHOW COMMANDS ///////////////////////////////////////////////////////////

// for the do_show command:
#define SHOW(name)	void (name)(char_data *ch, char *argument)


// for show_islands	
struct show_island_data {
	int island;
	int count;
	struct show_island_data *next;
};

// helper to get/create an island entry in a list
struct show_island_data *find_or_make_show_island(int island, struct show_island_data **list) {
	struct show_island_data *sid, *cur = NULL;
	
	// find an island entry
	for (sid = *list; sid && !cur; sid = sid->next) {
		if (sid->island == island) {
			cur = sid;
			break;
		}
	}

	// need to make an entry?
	if (!cur) {
		CREATE(cur, struct show_island_data, 1);
		cur->island = island;
		cur->count = 0;
		cur->next = *list;
		*list = cur;
	}
	
	return cur;
}


SHOW(show_islands) {
	struct empire_unique_storage *uniq;
	struct empire_storage_data *store;
	char arg[MAX_INPUT_LENGTH];
	empire_data *emp;
	int iter;
	
	struct show_island_data *list = NULL, *sid, *cur = NULL, *temp;
	
	one_word(argument, arg);
	if (!*arg) {
		msg_to_char(ch, "Show islands (storage) for which empire?\r\n");
	}
	else if (!(emp = get_empire_by_name(arg))) {
		msg_to_char(ch, "Unknown empire '%s'.\r\n", arg);
	}
	else {
		msg_to_char(ch, "Island storage counts for %s%s&0:\r\n", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		
		// collate storage info
		for (store = EMPIRE_STORAGE(emp); store; store = store->next) {
			if (!cur || cur->island != store->island) {
				cur = find_or_make_show_island(store->island, &list);
			}
			SAFE_ADD(cur->count, store->amount, INT_MIN, INT_MAX, TRUE);
		}
		for (uniq = EMPIRE_UNIQUE_STORAGE(emp); uniq; uniq = uniq->next) {
			if (!cur || cur->island != uniq->island) {
				cur = find_or_make_show_island(uniq->island, &list);
			}
			SAFE_ADD(cur->count, uniq->amount, INT_MIN, INT_MAX, TRUE);
		}
		
		if (!list) {
			msg_to_char(ch, " no storage\r\n");
		}
		
		// islands are sequentially numbered so this should be pretty safe -- iterate as long as anything remains in list; remove as we go
		for (iter = MIN(0, NO_ISLAND); list; ++iter) {
			// do we have this number in the list?
			cur = NULL;
			for (sid = list; sid && !cur; sid = sid->next) {
				if (sid->island == iter) {
					cur = sid;
					break;
				}
			}
			
			if (!cur) {
				continue;
			}
			
			msg_to_char(ch, "%2d. %d items\r\n", cur->island, cur->count);
			// pull it out of the list to prevent unlimited iteration
			REMOVE_FROM_LIST(cur, list, next);
			free(cur);
		}
	}
}


SHOW(show_player) {
	struct char_file_u vbuf;
	char birth[80], lastlog[80];
	double days_played, avg_min_per_day;
	
	if (!*argument) {
		send_to_char("A name would help.\r\n", ch);
		return;
	}

	if (load_char(argument, &vbuf) == NOTHING) {
		send_to_char("There is no such player.\r\n", ch);
		return;
	}
	sprintf(buf, "Player: %-12s (%s) [%d]\r\n", vbuf.name, genders[(int) vbuf.sex], vbuf.access_level);
	strcpy(birth, ctime(&vbuf.birth));
	strcpy(lastlog, ctime(&vbuf.last_logon));
	// Www Mmm dd hh:mm:ss yyyy
	sprintf(buf + strlen(buf), "Started: %-16.16s %4.4s   Last: %-16.16s %4.4s\r\n", birth, birth+20, lastlog, lastlog+20);
	
	if (vbuf.access_level <= GET_ACCESS_LEVEL(ch)) {
		sprintf(buf + strlen(buf), "Creation host: %s\r\n", vbuf.player_specials_saved.creation_host);
	}
	
	days_played = (double)(time(0) - vbuf.birth) / SECS_PER_REAL_DAY;
	avg_min_per_day = (((double) vbuf.played / SECS_PER_REAL_HOUR) / days_played) * SECS_PER_REAL_MIN;
	
	sprintf(buf + strlen(buf), "Played: %3dh %2dm (%d minutes per day)\r\n", (int) (vbuf.played / SECS_PER_REAL_HOUR), (int) (vbuf.played % SECS_PER_REAL_HOUR) / SECS_PER_REAL_MIN, (int)avg_min_per_day);
	
	send_to_char(buf, ch);
}


SHOW(show_rent) {
	void Crash_listrent(char_data *ch, char *name);
	
	if (!*argument) {
		send_to_char("A name would help.\r\n", ch);
		return;
	}

	Crash_listrent(ch, argument);
}


SHOW(show_stats) {
	extern int get_total_score(empire_data *emp);
	extern int buf_switches, buf_largecount, buf_overflows;
	
	int num_active_empires = 0, num_objs = 0, num_mobs = 0, num_players = 0, num_descs = 0, menu_count = 0;
	empire_data *emp, *next_emp;
	descriptor_data *desc;
	char_data *vict;
	obj_data *obj;
	
	// count descriptors at menus
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (STATE(desc) != CON_PLAYING) {
			++menu_count;
		}
	}
	
	// count connections, players, mobs
	for (vict = character_list; vict; vict = vict->next) {
		if (IS_NPC(vict)) {
			++num_mobs;
		}
		else if (CAN_SEE(ch, vict)) {
			++num_players;
			if (vict->desc) {
				++num_descs;
			}
		}
	}
	
	// count objs in world
	for (obj = object_list; obj; obj = obj->next) {
		++num_objs;
	}

	// count active empires
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (get_total_score(emp) > 0) {
			++num_active_empires;
		}
	}

	msg_to_char(ch, "Current stats:\r\n");
	msg_to_char(ch, "  %6d players in game  %6d connected\r\n", num_players, num_descs);
	msg_to_char(ch, "  %6d registered       %6d at menus\r\n", top_of_p_table + 1, menu_count);
	msg_to_char(ch, "  %6d empires          %6d active\r\n", HASH_COUNT(empire_table), num_active_empires);
	msg_to_char(ch, "  %6d mobiles          %6d prototypes\r\n", num_mobs, HASH_COUNT(mobile_table));
	msg_to_char(ch, "  %6d objects          %6d prototypes\r\n", num_objs, HASH_COUNT(object_table));
	msg_to_char(ch, "  %6d adventures       %6d total rooms\r\n", HASH_COUNT(adventure_table), HASH_CNT(world_hh, world_table));
	msg_to_char(ch, "  %6d buildings        %6d room templates\r\n", HASH_COUNT(building_table), HASH_COUNT(room_template_table));
	msg_to_char(ch, "  %6d sectors          %6d crops\r\n", HASH_COUNT(sector_table), HASH_COUNT(crop_table));
	msg_to_char(ch, "  %6d triggers         %6d craft recipes\r\n", HASH_COUNT(trigger_table), HASH_COUNT(craft_table));
	msg_to_char(ch, "  %6d large bufs       %6d buf switches\r\n", buf_largecount, buf_switches);
	msg_to_char(ch, "  %6d overflows\r\n", buf_overflows);
}


SHOW(show_site) {
	struct char_file_u vbuf;
	int j, k;
	
	if (!*argument) {
		msg_to_char(ch, "Locate players from what site?\r\n");
		return;
	}
	*buf = '\0';
	k = 0;
	for (j = 0; j <= top_of_p_table; j++) {
		load_char((player_table + j)->name, &vbuf);
		if (!IS_SET(vbuf.char_specials_saved.act, PLR_DELETED))
			if (str_str(vbuf.host, argument))
				sprintf(buf, "%s %-15.15s %s", buf, vbuf.name, ((++k % 3)) ? "|" : "\r\n");
	}
	msg_to_char(ch, "Players from site %s:\r\n", argument);
	if (*buf) {
		msg_to_char(ch, buf);
		
		// trailing crlf
		if (k % 3) {
			msg_to_char(ch, "\r\n");
		}
	}
	else {
		msg_to_char(ch, " none\r\n");
	}
}


SHOW(show_skills) {
	extern char *ability_color(char_data *ch, int abil);
	extern int get_ability_points_available_for_char(char_data *ch, int skill);
	
	char_data *vict;
	int sk_iter, ab_iter;
	bool found, is_file = FALSE;
	struct char_file_u tmp_store;
	
	argument = one_argument(argument, arg);
	vict = get_player_vis(ch, arg, FIND_CHAR_WORLD);
	
	if (!vict) {
		CREATE(vict, char_data, 1);
		clear_char(vict);
		if (load_char(arg, &tmp_store) > NOBODY) {
			store_to_char(&tmp_store, vict);
			SET_BIT(PLR_FLAGS(vict), PLR_KEEP_LAST_LOGIN_INFO);
		}
		else {
			// still no
			send_config_msg(ch, "no_person");
			free(vict);
			return;
		}
	}
	
	if (REAL_NPC(vict)) {
		msg_to_char(ch, "You can't show skills on an NPC.\r\n");
		if (is_file) {
			// unlikely to get here, but playing it safe
			free_char(vict);
		}
		return;
	}
	
	msg_to_char(ch, "Skills for %s:\r\n", PERS(vict, ch, TRUE));
	
	for (sk_iter = 0; sk_iter < NUM_SKILLS; ++sk_iter) {
		msg_to_char(ch, "&y%s&0 [%d, %.1f%%, %d]: ", skill_data[sk_iter].name, GET_SKILL(vict, sk_iter), GET_SKILL_EXP(vict, sk_iter), get_ability_points_available_for_char(vict, sk_iter));
		
		found = FALSE;
		for (ab_iter = 0; ab_iter < NUM_ABILITIES; ++ab_iter) {
			if (ability_data[ab_iter].parent_skill == sk_iter && HAS_ABILITY(vict, ab_iter)) {
				msg_to_char(ch, "%s%s%s&0", (found ? ", " : ""), ability_color(vict, ab_iter), ability_data[ab_iter].name);
				found = TRUE;
			}
		}
		
		msg_to_char(ch, "%s\r\n", (found ? "" : "none"));
	}
	
	msg_to_char(ch, "&yClass&0: &g");
	found = FALSE;
	for (ab_iter = 0; ab_iter < NUM_ABILITIES; ++ab_iter) {
		if (ability_data[ab_iter].parent_skill == NOTHING && HAS_ABILITY(vict, ab_iter)) {
			msg_to_char(ch, "%s%s", (found ? ", " : ""), ability_data[ab_iter].name);
			found = TRUE;
		}
	}
	msg_to_char(ch, "&0%s\r\n", (found ? "" : "none"));
	
	// available summary
	msg_to_char(ch, "Available daily bonus experience points: %d\r\n", GET_DAILY_BONUS_EXPERIENCE(vict));
	
	if (is_file) {
		free_char(vict);
	}
}


SHOW(show_commons) {
	descriptor_data *d, *nd;
	
	*buf = '\0';
	for (d = descriptor_list; d; d = d->next)
		for (nd = descriptor_list; nd; nd = nd->next)
			if (!str_cmp(d->host, nd->host) && d != nd && (!d->character || !PLR_FLAGGED(d->character, PLR_IPMASK))) {
				sprintf(buf + strlen(buf), "%s\r\n", d->character ? GET_NAME(d->character) : "<No Player>");
				break;
			}
	msg_to_char(ch, "Common sites:\r\n");
	if (*buf)
		msg_to_char(ch, buf);
	else
		msg_to_char(ch, "None.\r\n");
}


SHOW(show_players) {
	char_data *vict;
	descriptor_data *d;
	
	*buf = '\0';

	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) != CON_PLAYING || !(vict = d->character))
			continue;
		if (!CAN_SEE(ch, vict))
			continue;

		sprintf(buf + strlen(buf), " %-20.20s &g%4d&0/&g%4d&0 &y%4d&0/&y%4d&0 &c%4d&0/&c%4d&0", PERS(vict, vict, 1), GET_HEALTH(vict), GET_MAX_HEALTH(vict), GET_MOVE(vict), GET_MAX_MOVE(vict), GET_MANA(vict), GET_MAX_MANA(vict));
		if (IS_VAMPIRE(vict))
			sprintf(buf + strlen(buf), " &r%2d&0/&r%2d&0", GET_BLOOD(vict), GET_MAX_BLOOD(vict));

		strcat(buf, "\r\n");
	}

	msg_to_char(ch, "Players:\r\n %-20.20s %-9.9s %-9.9s %-9.9s %s\r\n %-20.20s %-9.9s %-9.9s %-9.9s %s\r\n%s", "Name", " Health", " Move", " Mana", "Blood", "----", "---------", "---------", "---------", "-----", buf);
}


SHOW(show_terrain) {
	extern int stats_get_crop_count(crop_data *cp);
	extern int stats_get_sector_count(sector_data *sect);
	void update_world_count();
	
	sector_data *sect, *next_sect;
	crop_data *crop, *next_crop;
	int count, total, this;
	
	// fresh numbers
	update_world_count();
	
	// output
	total = count = 0;
	
	HASH_ITER(hh, sector_table, sect, next_sect) {
		this = stats_get_sector_count(sect);
		msg_to_char(ch, " %6d %-20.20s %s", this, GET_SECT_NAME(sect), !((++count)%2) ? "\r\n" : " ");
		total += this;
	}
	
	HASH_ITER(hh, crop_table, crop, next_crop) {
		strcpy(buf, GET_CROP_NAME(crop));
		msg_to_char(ch, " %6d %-20.20s %s", stats_get_crop_count(crop), CAP(buf), !((++count)%2) ? "\r\n" : " ");
	}
	if (count % 2) {
		msg_to_char(ch, "\r\n");
	}
	
	msg_to_char(ch, " Total: %d\r\n", total);

}


SHOW(show_account) {
	struct char_file_u cbuf, vbuf;
	int iter;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: show account <player>\r\n");
	}
	else if (load_char(argument, &cbuf) == NOTHING) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else {
		msg_to_char(ch, "Account characters:\r\n");
		
		for (iter = 0; iter <= top_of_p_table; iter++) {
			load_char(player_table[iter].name, &vbuf);
			if (vbuf.char_specials_saved.idnum == cbuf.char_specials_saved.idnum || (vbuf.player_specials_saved.account_id != 0 && vbuf.player_specials_saved.account_id == cbuf.player_specials_saved.account_id)) {
				// same account
				if (is_playing(vbuf.char_specials_saved.idnum) || is_at_menu(vbuf.char_specials_saved.idnum)) {
					msg_to_char(ch, " &c[%d %s] %s (online)&0%s\r\n", vbuf.player_specials_saved.last_known_level, class_data[vbuf.player_specials_saved.character_class].name, vbuf.name, IS_SET(vbuf.char_specials_saved.act, PLR_DELETED) ? " (deleted)" : "");
				}
				else {
					// not playing but same account
					msg_to_char(ch, " [%d %s] %s%s\r\n", vbuf.player_specials_saved.last_known_level, class_data[vbuf.player_specials_saved.character_class].name, vbuf.name, IS_SET(vbuf.char_specials_saved.act, PLR_DELETED) ? " (deleted)" : "");
				}
			}
			else if (!IS_SET(vbuf.char_specials_saved.act, PLR_DELETED) && !strcmp(vbuf.host, cbuf.host)) {
				msg_to_char(ch, " &r[%d %s] %s (not on account)&0\r\n", vbuf.player_specials_saved.last_known_level, class_data[vbuf.player_specials_saved.character_class].name, vbuf.name);
			}
		}
	}
}


SHOW(show_notes) {
	struct char_file_u cbuf;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: show notes <player>\r\n");
	}
	else if (load_char(argument, &cbuf) == NOTHING) {
		msg_to_char(ch, "There is no such player.\r\n");
	}
	else if (cbuf.access_level >= GET_REAL_LEVEL(ch)) {
		msg_to_char(ch, "You can't show notes for players of that level.\r\n");
	}
	else if (!*cbuf.player_specials_saved.admin_notes) {
		msg_to_char(ch, "There are no notes for that player.\r\n");
	}
	else {
		msg_to_char(ch, "Admin notes for %s:\r\n%s", cbuf.name, cbuf.player_specials_saved.admin_notes);
	}
}


SHOW(show_arrowtypes) {
	obj_data *obj, *next_obj;
	int total;
	
	strcpy(buf, "You find the following arrow types:\r\n");
	
	total = 0;
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (IS_MISSILE_WEAPON(obj) || IS_ARROW(obj)) {
			sprintf(buf1, " %c: %s", 'A' + GET_OBJ_VAL(obj, IS_ARROW(obj) ? VAL_ARROW_TYPE : VAL_MISSILE_WEAPON_TYPE), GET_OBJ_SHORT_DESC(obj));
			sprintf(buf + strlen(buf), "%-32.32s%s", buf1, ((total++ % 2) ? "\r\n" : ""));
		}
	}
	
	if (total == 0) {
		msg_to_char(ch, "You find no objects with arrow types.\r\n");
	}
	else {
		if ((total % 2) != 0) {
			strcat(buf, "\r\n");
		}
		page_string(ch->desc, buf, TRUE);
	}
}


SHOW(show_ignoring) {
	char arg[MAX_INPUT_LENGTH];
	bool found, file = FALSE;
	char_data *vict = NULL;
	int iter;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Show ignores for whom?\r\n");
	}
	else if (!(vict = find_or_load_player(arg, &file))) {
		msg_to_char(ch, "There is no such player.\r\n");
	}
	else if (GET_ACCESS_LEVEL(vict) > GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else {
		// just list ignores
		msg_to_char(ch, "%s is ignoring: \r\n", GET_NAME(vict));
		
		found = FALSE;
		for (iter = 0; iter < MAX_IGNORES; ++iter) {
			if (GET_IGNORE_LIST(vict, iter) != 0 && get_name_by_id(GET_IGNORE_LIST(vict, iter))) {
				msg_to_char(ch, " %s\r\n", CAP(get_name_by_id(GET_IGNORE_LIST(vict, iter))));
				found = TRUE;
			}
		}
		
		if (!found) {
			msg_to_char(ch, " nobody\r\n");
		}
	
	}
	
	if (vict && file) {
		free_char(vict);
	}
}


SHOW(show_workforce) {
	void show_workforce_setup_to_char(empire_data *emp, char_data *ch);
	
	char arg[MAX_INPUT_LENGTH];
	empire_data *emp;
	
	one_word(argument, arg);
	if (!*arg) {
		msg_to_char(ch, "Show workforce for what empire?\r\n");
	}
	else if (!(emp = get_empire_by_name(arg))) {
		msg_to_char(ch, "Unknown empire '%s'.\r\n", arg);
	}
	else {
		show_workforce_setup_to_char(emp, ch);
	}
}


SHOW(show_storage) {
	extern bld_data *get_building_by_name(char *name, bool room_only);
	
	struct obj_storage_type *store;
	bld_vnum building_type = NOTHING;
	obj_data *obj, *next_obj;
	bld_data *bld;
	int total;
	bool ok;

	if (*argument) {
		if ((bld = get_building_by_name(argument, FALSE))) {
			building_type = GET_BLD_VNUM(bld);
		}
	}
	else {
		building_type = BUILDING_VNUM(IN_ROOM(ch));
	}
	
	strcpy(buf, "Objects that can be stored here:\r\n");
	
	total = 0;
	if (building_type != NOTHING) {
		HASH_ITER(hh, object_table, obj, next_obj) {
			ok = FALSE;
		
			for (store = obj->storage; store && !ok; store = store->next) {
				if (store->building_type == building_type) {
					ok = TRUE;
				}
			}
		
			if (ok) {
				sprintf(buf + strlen(buf), " %2d. [%5d] %s\r\n", ++total, GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
			}
		}
	}
	
	if (total == 0) {
		strcat(buf, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


SHOW(show_startlocs) {
	room_data *iter, *next_iter;
	
	strcpy(buf, "Starting locations:\r\n");
	
	HASH_ITER(world_hh, world_table, iter, next_iter) {
		if (ROOM_SECT_FLAGGED(iter, SECTF_START_LOCATION)) {
			sprintf(buf + strlen(buf), "%s (%d, %d)&0\r\n", get_room_name(iter, TRUE), X_COORD(iter), Y_COORD(iter));
		}
	}
	
	page_string(ch->desc, buf, TRUE);
}


SHOW(show_spawns) {
	char buf[MAX_STRING_LENGTH];
	struct spawn_info *sp;
	sector_data *sect, *next_sect;
	crop_data *crop, *next_crop;
	bld_data *bld, *next_bld;
	char_data *mob;
	mob_vnum vnum;
	
	if (!*argument || !isdigit(*argument) || !(mob = mob_proto((vnum = atoi(argument))))) {
		msg_to_char(ch, "You must specify a valid mob vnum.\r\n");
		return;
	}
	
	*buf = '\0';

	// sectors:
	HASH_ITER(hh, sector_table, sect, next_sect) {
		for (sp = GET_SECT_SPAWNS(sect); sp; sp = sp->next) {
			if (sp->vnum == vnum) {
				sprintbit(sp->flags, spawn_flags, buf2, TRUE);
				sprintf(buf1, "%s: %.2f%% %s\r\n", GET_SECT_NAME(sect), sp->percent, buf2);
				if (strlen(buf) + strlen(buf1) < MAX_STRING_LENGTH) {
					strcat(buf, buf1);
				}
			}
		}
	}
	
	// crops:
	HASH_ITER(hh, crop_table, crop, next_crop) {
		for (sp = GET_CROP_SPAWNS(crop); sp; sp = sp->next) {
			if (sp->vnum == vnum) {
				sprintbit(sp->flags, spawn_flags, buf2, TRUE);
				sprintf(buf1, "%s: %.2f%% %s\r\n", GET_CROP_NAME(crop), sp->percent, buf2);
				CAP(buf1);	// crop names are lowercase
				if (strlen(buf) + strlen(buf1) < MAX_STRING_LENGTH) {
					strcat(buf, buf1);
				}
			}
		}
	}

	// buildings:
	HASH_ITER(hh, building_table, bld, next_bld) {
		for (sp = GET_BLD_SPAWNS(bld); sp; sp = sp->next) {
			if (sp->vnum == vnum) {
				sprintbit(sp->flags, spawn_flags, buf2, TRUE);
				sprintf(buf1, "%s: %.2f%% %s\r\n", GET_BLD_NAME(bld), sp->percent, buf2);
				if (strlen(buf) + strlen(buf1) < MAX_STRING_LENGTH) {
					strcat(buf, buf1);
				}
			}
		}
	}
	
	if (*buf) {
		page_string(ch->desc, buf, TRUE);
	}
	else {
		msg_to_char(ch, "No spawns found for mob %d.\r\n", vnum);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// STAT / VSTAT ////////////////////////////////////////////////////////////


/**
* Shows summarized spawn info for any spawn_info list to the character, only
* if there is something to show. No output is shown if there are no spawns.
*
* @param char_data *ch The person to show it to.
* @param struct spawn_info *list Any list of spawn_info.
*/
void show_spawn_summary_to_char(char_data *ch, struct spawn_info *list) {
	char output[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], entry[MAX_INPUT_LENGTH];
	char flg[MAX_INPUT_LENGTH];
	struct spawn_info *spawn;
	
	// spawns?
	*output = '\0';
	*line = '\0';
	for (spawn = list; spawn; spawn = spawn->next) {
		if (spawn->flags) {
			sprintbit(spawn->flags, spawn_flags_short, flg, TRUE);
			flg[strlen(flg) - 1] = '\0';	// removes the trailing space
			sprintf(entry, " %s (%d) %.2f%% %s", skip_filler(get_mob_name_by_proto(spawn->vnum)), spawn->vnum, spawn->percent, flg);
		}
		else {
			// no flags
			sprintf(entry, " %s (%d) %.2f%%", skip_filler(get_mob_name_by_proto(spawn->vnum)), spawn->vnum, spawn->percent);
		}
		
		if (*line || *output) {
			strcat(line, ",");
		}
		if (strlen(line) + strlen(entry) > 78) {
			strcat(line, "\r\n");
			strcat(output, line);
			*line = '\0';
		}
		strcat(line, entry);
	}
	if (*output || *line) {
		msg_to_char(ch, "Spawn info:\r\n%s%s%s", output, line, (*line ? "\r\n" : ""));
	}
}


/**
* @param char_data *ch The player requesting stats.
* @param adv_data *adv The adventure to display.
*/
void do_stat_adventure(char_data *ch, adv_data *adv) {
	extern int count_instances(adv_data *adv);
	void get_adventure_linking_display(struct adventure_link_rule *list, char *save_buffer);
	extern const char *adventure_flags[];
	
	char lbuf[MAX_STRING_LENGTH];
	int time;
	
	if (!adv) {
		return;
	}
	
	msg_to_char(ch, "VNum: [&c%d&0], Name: &c%s&0 (by &c%s&0)\r\n", GET_ADV_VNUM(adv), GET_ADV_NAME(adv), GET_ADV_AUTHOR(adv));
	msg_to_char(ch, "%s", NULLSAFE(GET_ADV_DESCRIPTION(adv)));
	
	msg_to_char(ch, "VNum range: [&c%d&0-&c%d&0], Level range: [&c%d&0-&c%d&0]\r\n", GET_ADV_START_VNUM(adv), GET_ADV_END_VNUM(adv), GET_ADV_MIN_LEVEL(adv), GET_ADV_MAX_LEVEL(adv));

	// reset time display helper
	if (GET_ADV_RESET_TIME(adv) == 0) {
		strcpy(lbuf, "never");
	}
	else if (GET_ADV_RESET_TIME(adv) > (60 * 24)) {
		time = GET_ADV_RESET_TIME(adv) - (GET_ADV_RESET_TIME(adv) / (60 * 24));
		sprintf(lbuf, "%d:%02d:%02d", (GET_ADV_RESET_TIME(adv) / (60 * 24)), (time / 60), (time % 60));
	}
	else if (GET_ADV_RESET_TIME(adv) > 60) {
		sprintf(lbuf, "%2d:%02d", (GET_ADV_RESET_TIME(adv) / 60), (GET_ADV_RESET_TIME(adv) % 60));
	}
	else {
		sprintf(lbuf, "%d min", GET_ADV_RESET_TIME(adv));
	}
	
	msg_to_char(ch, "Instance limit: [&c%d&0/&c%d&0], Player limit: [&c%d&0], Reset time: [&c%s&0]\r\n", count_instances(adventure_proto(GET_ADV_VNUM(adv))), GET_ADV_MAX_INSTANCES(adv), GET_ADV_PLAYER_LIMIT(adv), lbuf);
	
	sprintbit(GET_ADV_FLAGS(adv), adventure_flags, lbuf, TRUE);
	msg_to_char(ch, "Flags: &g%s&0\r\n", lbuf);
	
	get_adventure_linking_display(GET_ADV_LINKING(adv), lbuf);
	msg_to_char(ch, "Linking rules:\r\n%s", lbuf);
}


/**
* Show a character stats on a particular building.
*
* @param char_data *ch The player requesting stats.
* @param bld_data *bdg The building to stat.
*/
void do_stat_building(char_data *ch, bld_data *bdg) {
	extern const char *bld_flags[];
	extern const char *designate_flags[];
	
	struct obj_storage_type *store;
	char line[MAX_STRING_LENGTH];
	obj_data *obj, *next_obj;
	
	msg_to_char(ch, "Building VNum: [&c%d&0], Name: '&c%s&0'\r\n", GET_BLD_VNUM(bdg), GET_BLD_NAME(bdg));
	msg_to_char(ch, "Room Title: %s, Icon: %s&0\r\n", GET_BLD_TITLE(bdg), NULLSAFE(GET_BLD_ICON(bdg)));
	
	if (GET_BLD_DESC(bdg) && *GET_BLD_DESC(bdg)) {
		msg_to_char(ch, "Description:\r\n%s", GET_BLD_DESC(bdg));
	}
	
	if (GET_BLD_COMMANDS(bdg) && *GET_BLD_COMMANDS(bdg)) {
		msg_to_char(ch, "Command list: &c%s&0\r\n", GET_BLD_COMMANDS(bdg));
	}
	
	msg_to_char(ch, "Hitpoints: [&g%d&0], Fame: [&g%d&0], Extra Rooms: [&g%d&0]\r\n", GET_BLD_MAX_DAMAGE(bdg), GET_BLD_FAME(bdg), GET_BLD_EXTRA_ROOMS(bdg));
	
	// artisan?
	if (GET_BLD_ARTISAN(bdg) != NOTHING) {
		sprintf(buf, ", Artisan: &g%d&0 &c%s&0", GET_BLD_ARTISAN(bdg), get_mob_name_by_proto(GET_BLD_ARTISAN(bdg)));
	}
	else {
		*buf = '\0';
	}
	
	msg_to_char(ch, "Citizens: [&g%d&0], Military: [&g%d&0]%s\r\n", GET_BLD_CITIZENS(bdg), GET_BLD_MILITARY(bdg), buf);
	
	if (GET_BLD_UPGRADES_TO(bdg) != NOTHING) {
		msg_to_char(ch, "Upgrades to: &g%d&0 &c%s&0\r\n", GET_BLD_UPGRADES_TO(bdg), GET_BLD_NAME(building_proto(GET_BLD_UPGRADES_TO(bdg))));
	}
	
	sprintbit(GET_BLD_FLAGS(bdg), bld_flags, buf, TRUE);
	msg_to_char(ch, "Building flags: &g%s&0\r\n", buf);
	
	sprintbit(GET_BLD_DESIGNATE_FLAGS(bdg), designate_flags, buf, TRUE);
	msg_to_char(ch, "Designate flags: &c%s&0\r\n", buf);
	
	sprintbit(GET_BLD_BASE_AFFECTS(bdg), room_aff_bits, buf, TRUE);
	msg_to_char(ch, "Base affects: &g%s&0\r\n", buf);
	
	if (GET_BLD_EX_DESCS(bdg)) {
		struct extra_descr_data *desc;
		sprintf(buf, "Extra descs:&c");
		for (desc = GET_BLD_EX_DESCS(bdg); desc; desc = desc->next) {
			strcat(buf, " ");
			strcat(buf, desc->keyword);
		}
		msg_to_char(ch, "%s&0\r\n", buf);
	}
	
	if (GET_BLD_INTERACTIONS(bdg)) {
		send_to_char("Interactions:\r\n", ch);
		get_interaction_display(GET_BLD_INTERACTIONS(bdg), buf);
		send_to_char(buf, ch);
	}
	
	// storage? reverse-lookup
	*buf = '\0';
	*line = '\0';
	HASH_ITER(hh, object_table, obj, next_obj) {
		for (store = obj->storage; store; store = store->next) {
			if (store->building_type == GET_BLD_VNUM(bdg)) {
				sprintf(buf1, " %s", GET_OBJ_SHORT_DESC(obj));
				if (*line || *buf) {
					strcat(line, ",");
				}
				if (strlen(line) + strlen(buf1) > 78) {
					strcat(line, "\r\n");
					strcat(buf, line);
					*line = '\0';
				}
				strcat(line, buf1);
				break;
			}
		}
	}
	if (*buf || *line) {
		msg_to_char(ch, "Storable items:\r\n%s%s%s", buf, line, (*line ? "\r\n" : ""));
	}
	
	show_spawn_summary_to_char(ch, GET_BLD_SPAWNS(bdg));
}


/* Sends ch information on the character or animal k */
void do_stat_character(char_data *ch, char_data *k) {
	void find_uid_name(char *uid, char *name);
	extern double get_combat_speed(char_data *ch, int pos);
	extern int get_effective_block(char_data *ch);
	extern int get_effective_dodge(char_data *ch);
	extern int get_effective_soak(char_data *ch);
	extern int get_effective_to_hit(char_data *ch);
	extern int move_gain(char_data *ch);
	void display_attributes(char_data *ch, char_data *to);

	extern const char *class_role[NUM_ROLES];
	extern const char *cooldown_types[];
	extern const char *damage_types[];
	extern const char *player_bits[];
	extern const char *position_types[];
	extern const char *preference_bits[];
	extern const char *connected_types[];
	extern const char *olc_flag_bits[];
	extern struct promo_code_list promo_codes[];

	char lbuf[MAX_STRING_LENGTH], lbuf2[MAX_STRING_LENGTH], lbuf3[MAX_STRING_LENGTH];
	char uname[MAX_INPUT_LENGTH];
	struct script_memory *mem;
	struct trig_var_data *tv;
	struct cooldown_data *cool;
	int i, i2, diff, found = 0;
	obj_data *j;
	struct follow_type *fol;
	struct over_time_effect_type *dot;
	struct affected_type *aff;
	
	bool is_proto = (IS_NPC(k) && k == mob_proto(GET_MOB_VNUM(k)));

	sprinttype(GET_REAL_SEX(k), genders, buf);
	CAP(buf);
	sprintf(buf2, " %s '&y%s&0'  IDNum: [%5d], In room [%5d]\r\n", (!IS_NPC(k) ? "PC" : (!IS_MOB(k) ? "NPC" : "MOB")), GET_NAME(k), GET_IDNUM(k), IN_ROOM(k) ? GET_ROOM_VNUM(IN_ROOM(k)) : NOWHERE);
	send_to_char(strcat(buf, buf2), ch);

	if (IS_MOB(k)) {
		msg_to_char(ch, "Alias: &y%s&0, VNum: [&c%5d&0]\r\n", GET_PC_NAME(k), GET_MOB_VNUM(k));
		msg_to_char(ch, "L-Des: &y%s&0", (GET_LONG_DESC(k) ? GET_LONG_DESC(k) : "<None>\r\n"));
	}

	if (IS_NPC(k)) {
		msg_to_char(ch, "Scaled level: [&c%d&0|&c%d&0-&c%d&0]\r\n", GET_CURRENT_SCALE_LEVEL(k), GET_MIN_SCALE_LEVEL(k), GET_MAX_SCALE_LEVEL(k));
	}
	else {	// not NPC
		msg_to_char(ch, "Title: %s&0\r\n", (GET_TITLE(k) ? GET_TITLE(k) : "<None>"));
		
		if (*GET_REFERRED_BY(k)) {
			msg_to_char(ch, "Referred by: %s\r\n", GET_REFERRED_BY(k));
		}
		if (GET_PROMO_ID(k) > 0) {
			msg_to_char(ch, "Promo code: %s\r\n", promo_codes[(int)GET_PROMO_ID(k)].code);
		}

		msg_to_char(ch, "Access Level: [&c%d&0], Class: [&c%s&0/&c%s&0], Skill Level: [&c%d&0], Gear Level: [&c%d&0], Total: [&c%d&0]\r\n", GET_ACCESS_LEVEL(k), class_data[GET_CLASS(k)].name, class_role[(int) GET_CLASS_ROLE(k)], GET_SKILL_LEVEL(k), (int) GET_GEAR_LEVEL(k), IN_ROOM(k) ? GET_COMPUTED_LEVEL(k) : GET_LAST_KNOWN_LEVEL(k));
		
		coin_string(GET_PLAYER_COINS(k), buf);
		msg_to_char(ch, "Coins: %s\r\n", buf);

		strcpy(buf1, (char *) asctime(localtime(&(k->player.time.birth))));
		strcpy(buf2, buf1 + 20);
		buf1[10] = '\0';	// DoW Mon Day
		buf2[4] = '\0';	// get only year

		msg_to_char(ch, "Created: [%s, %s], Played [%dh %dm], Age [%d]\r\n", buf1, buf2, k->player.time.played / SECS_PER_REAL_HOUR, ((k->player.time.played % SECS_PER_REAL_HOUR) / SECS_PER_REAL_MIN), age(k)->year);
		if (GET_ACCESS_LEVEL(k) <= GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "Created from host: [%s]\r\n", GET_CREATION_HOST(k));
		}
		
		if (GET_ACCESS_LEVEL(k) >= LVL_BUILDER) {
			sprintbit(GET_OLC_FLAGS(k), olc_flag_bits, buf, TRUE);
			msg_to_char(ch, "OLC Vnums: [&c%d-%d&0], Flags: &g%s&0\r\n", GET_OLC_MIN_VNUM(k), GET_OLC_MAX_VNUM(k), buf);
		}
	}

	if (!IS_NPC(k) || GET_CURRENT_SCALE_LEVEL(k) > 0) {
		msg_to_char(ch, "Health: [&g%d&0/&g%d&0]  Move: [&g%d&0/&g%d&0]  Mana: [&g%d&0/&g%d&0]  Blood: [&g%d&0/&g%d&0]\r\n", GET_HEALTH(k), GET_MAX_HEALTH(k), GET_MOVE(k), GET_MAX_MOVE(k), GET_MANA(k), GET_MAX_MANA(k), GET_BLOOD(k), GET_MAX_BLOOD(k));

		display_attributes(k, ch);

		sprintf(lbuf, "Dodge  [%s%+d/%+d&0]", HAPPY_COLOR(get_effective_dodge(k), 0), GET_DODGE(k), get_effective_dodge(k));
		sprintf(lbuf2, "Block  [%s%+d/%+d&0]", HAPPY_COLOR(get_effective_block(k), 0), GET_BLOCK(k), get_effective_block(k));
		sprintf(lbuf3, "Soak  [%s%+d/%+d&0]", HAPPY_COLOR(get_effective_soak(k), 0), GET_SOAK(k), get_effective_soak(k));
		msg_to_char(ch, "  %-28.28s %-28.28s %-28.28s\r\n", lbuf, lbuf2, lbuf3);
	
		sprintf(lbuf, "Physical  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_PHYSICAL(k), 0), GET_BONUS_PHYSICAL(k));
		sprintf(lbuf2, "Magical  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_MAGICAL(k), 0), GET_BONUS_MAGICAL(k));
		sprintf(lbuf3, "Healing  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_HEALING(k), 0), GET_BONUS_HEALING(k));
		msg_to_char(ch, "  %-28.28s %-28.28s %-28.28s\r\n", lbuf, lbuf2, lbuf3);

		sprintf(lbuf, "To-hit  [%s%+d/%+d&0]", HAPPY_COLOR(get_effective_to_hit(k), 0), GET_TO_HIT(k), get_effective_to_hit(k));
		sprintf(lbuf2, "Speed  [%.2f]", get_combat_speed(k, WEAR_WIELD));
		msg_to_char(ch, "  %-28.28s %-28.28s\r\n", lbuf, lbuf2);

		if (IS_NPC(k)) {
			msg_to_char(ch, "NPC Bare Hand Dam: %d\r\n", MOB_DAMAGE(k));
		}
	}

	sprinttype(GET_POS(k), position_types, buf2);
	sprintf(buf, "Pos: %s, Fighting: %s", buf2, (FIGHTING(k) ? GET_NAME(FIGHTING(k)) : "Nobody"));

	if (IS_NPC(k)) {
		sprintf(buf + strlen(buf), ", Attack type: %s", attack_hit_info[MOB_ATTACK_TYPE(k)].name);
	}
	if (k->desc) {
		sprinttype(STATE(k->desc), connected_types, buf2);
		strcat(buf, ", Connected: ");
		strcat(buf, buf2);
	}
	send_to_char(strcat(buf, "\r\n"), ch);

	if (IS_NPC(k)) {
		sprintbit(MOB_FLAGS(k), action_bits, buf2, TRUE);
		msg_to_char(ch, "NPC flags: &c%s&0\r\n", buf2);
	}
	else {
		sprintf(buf, "Idle Timer (in tics) [%d]\r\n", k->char_specials.timer);
		send_to_char(buf, ch);
		sprintbit(PLR_FLAGS(k), player_bits, buf2, TRUE);
		msg_to_char(ch, "PLR: &c%s&0\r\n", buf2);
		sprintbit(PRF_FLAGS(k), preference_bits, buf2, TRUE);
		msg_to_char(ch, "PRF: &g%s&0\r\n", buf2);
		sprintbit(GET_BONUS_TRAITS(k), bonus_bits, buf2, TRUE);
		msg_to_char(ch, "BONUS: &c%s&0\r\n", buf2);
		sprintbit(GET_GRANT_FLAGS(k), grant_bits, buf2, TRUE);
		msg_to_char(ch, "GRANTS: &g%s&0\r\n", buf2);
		sprintbit(SYSLOG_FLAGS(k), syslog_types, buf2, TRUE);
		msg_to_char(ch, "SYSLOGS: &c%s&0\r\n", buf2);
	}

	if (!is_proto) {
		sprintf(buf, "Carried items: %d; ", IS_CARRYING_N(k));
		for (i = 0, j = k->carrying; j; j = j->next_content, i++);
		sprintf(buf + strlen(buf), "Items in: inventory: %d, ", i);

		for (i = 0, i2 = 0; i < NUM_WEARS; i++)
			if (GET_EQ(k, i))
				i2++;
		sprintf(buf2, "eq: %d\r\n", i2);
		send_to_char(strcat(buf, buf2), ch);
	}

	if (IS_NPC(k) && k->interactions) {
		send_to_char("Interactions:\r\n", ch);
		get_interaction_display(k->interactions, buf);
		send_to_char(buf, ch);
	}

	if (!IS_NPC(k)) {
		msg_to_char(ch, "Hunger: %d, Thirst: %d, Drunk: %d\r\n", GET_COND(k, FULL), GET_COND(k, THIRST), GET_COND(k, DRUNK));
		msg_to_char(ch, "Recent deaths: %d\r\n", GET_RECENT_DEATH_COUNT(k));
	}

	if (!is_proto) {
		sprintf(buf, "Master is: %s, Followers are:", ((k->master) ? GET_NAME(k->master) : "<none>"));
		for (fol = k->followers; fol; fol = fol->next) {
			sprintf(buf2, "%s %s", found++ ? "," : "", PERS(fol->follower, ch, 1));
			strcat(buf, buf2);
			if (strlen(buf) >= 62) {
				if (fol->next)
					send_to_char(strcat(buf, ",\r\n"), ch);
				else
					send_to_char(strcat(buf, "\r\n"), ch);
				*buf = found = 0;
			}
		}

		if (*buf) {
			send_to_char(strcat(buf, "\r\n"), ch);
		}
	}

	// cooldowns
	if (k->cooldowns) {
		found = FALSE;
		msg_to_char(ch, "Cooldowns: ");
		
		for (cool = k->cooldowns; cool; cool = cool->next) {
			diff = cool->expire_time - time(0);
			
			if (diff > 0) {
				sprinttype(cool->type, cooldown_types, buf);
				msg_to_char(ch, "%s&c%s&0 %d:%02d", (found ? ", ": ""), buf, (diff / 60), (diff % 60));
				
				found = TRUE;
			}
		}
		
		msg_to_char(ch, "%s\r\n", (found ? "" : "none"));
	}

	/* Showing the bitvector */
	sprintbit(AFF_FLAGS(k), affected_bits, buf2, TRUE);
	msg_to_char(ch, "AFF: &c%s&0\r\n", buf2);

	/* Routine to show what spells a char is affected by */
	if (k->affected) {
		for (aff = k->affected; aff; aff = aff->next) {
			*buf2 = '\0';
			
			// duration setup
			if (aff->duration == UNLIMITED) {
				strcpy(lbuf, "infinite");
			}
			else {
				sprintf(lbuf, "%.1fmin", ((double)(aff->duration + 1) * SECS_PER_REAL_UPDATE / 60.0));
			}

			sprintf(buf, "TYPE: (%s) &c%s&0 ", lbuf, affect_types[aff->type]);

			if (aff->modifier) {
				sprintf(buf2, "%+d to %s", aff->modifier, apply_types[(int) aff->location]);
				strcat(buf, buf2);
			}
			if (aff->bitvector) {
				if (*buf2)
					strcat(buf, ", sets ");
				else
					strcat(buf, "sets ");
				sprintbit(aff->bitvector, affected_bits, buf2, TRUE);
				strcat(buf, buf2);
			}
			send_to_char(strcat(buf, "\r\n"), ch);
		}
	}
	
	// dots
	for (dot = k->over_time_effects; dot; dot = dot->next) {
		if (dot->duration == UNLIMITED) {
			strcpy(lbuf, "unlimited");
		}
		else {
			sprintf(lbuf, "%.1fmin", ((double)(dot->duration + 1) * SECS_PER_REAL_UPDATE / 60.0));
		}
		
		msg_to_char(ch, "TYPE: (%s) &r%s&0 %d %s damage (%d/%d)\r\n", lbuf, affect_types[dot->type], dot->damage * dot->stack, damage_types[dot->damage_type], dot->stack, dot->max_stack);
	}

	/* check mobiles for a script */
	if (IS_NPC(k)) {
		do_sstat_character(ch, k);
		if (SCRIPT_MEM(k)) {
			mem = SCRIPT_MEM(k);
			msg_to_char(ch, "Script memory:\r\n  Remember             Command\r\n");
			while (mem) {
				char_data *mc = find_char(mem->id);
				if (!mc) {
					msg_to_char(ch, "  ** Corrupted!\r\n");
				}
				else {
					if (mem->cmd) {
						msg_to_char(ch, "  %-20.20s%s\r\n", GET_NAME(mc), mem->cmd);
					}
					else {
						msg_to_char(ch, "  %-20.20s <default>\r\n", GET_NAME(mc));
					}
				}
				
				mem = mem->next;
			}
		}
	}
	else {
		/* this is a PC, display their global variables */
		if (k->script && k->script->global_vars) {
			msg_to_char(ch, "Global Variables:\r\n");

			/* currently, variable context for players is always 0, so it is */
			/* not displayed here. in the future, this might change */
			for (tv = k->script->global_vars; tv; tv = tv->next) {
				if (*(tv->value) == UID_CHAR) {
					find_uid_name(tv->value, uname);
					msg_to_char(ch, "    %10s:  [UID]: %s\r\n", tv->name, uname);
				}
				else {
					msg_to_char(ch, "    %10s:  %s\r\n", tv->name, tv->value);
				}
			}
		}
	}
}


void do_stat_craft(char_data *ch, craft_data *craft) {
	extern const char *craft_flags[];
	extern const char *craft_types[];

	bld_data *bld;
	int count, iter, seconds;
	
	msg_to_char(ch, "Name: '&y%s&0', Vnum: [&g%d&0], Type: &c%s&0\r\n", GET_CRAFT_NAME(craft), GET_CRAFT_VNUM(craft), craft_types[GET_CRAFT_TYPE(craft)]);
	
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {
		bld = building_proto(GET_CRAFT_BUILD_TYPE(craft));
		msg_to_char(ch, "Builds: [&c%d&0] %s\r\n", GET_CRAFT_BUILD_TYPE(craft), bld ? GET_BLD_NAME(bld) : "UNKNOWN");
	}
	else if (CRAFT_FLAGGED(craft, CRAFT_SOUP)) {
		msg_to_char(ch, "Creates Volume: [&g%d drink%s&0], Liquid: [&g%d&0] %s\r\n", GET_CRAFT_QUANTITY(craft), PLURAL(GET_CRAFT_QUANTITY(craft)), GET_CRAFT_OBJECT(craft), (GET_CRAFT_OBJECT(craft) == NOTHING ? "NOTHING" : drinks[GET_CRAFT_OBJECT(craft)]));
	}
	else {
		msg_to_char(ch, "Creates Quantity: [&g%d&0], Item: [&c%d&0] %s\r\n", GET_CRAFT_QUANTITY(craft), GET_CRAFT_OBJECT(craft), get_obj_name_by_proto(GET_CRAFT_OBJECT(craft)));
	}
	
	sprintf(buf, "%s", (GET_CRAFT_ABILITY(craft) == NO_ABIL ? "none" : ability_data[GET_CRAFT_ABILITY(craft)].name));
	if (GET_CRAFT_ABILITY(craft) != NO_ABIL && ability_data[GET_CRAFT_ABILITY(craft)].parent_skill != NO_SKILL) {
		sprintf(buf + strlen(buf), " (%s %d)", skill_data[ability_data[GET_CRAFT_ABILITY(craft)].parent_skill].name, ability_data[GET_CRAFT_ABILITY(craft)].parent_skill_required);
	}
	seconds = GET_CRAFT_TIME(craft) * SECS_PER_REAL_UPDATE;
	msg_to_char(ch, "Ability: &y%s&0, Time: [&g%d action tick%s&0 | &g%d:%02d&0]\r\n", buf, GET_CRAFT_TIME(craft), PLURAL(GET_CRAFT_TIME(craft)), seconds / SECS_PER_REAL_MIN, seconds % SECS_PER_REAL_MIN);

	sprintbit(GET_CRAFT_FLAGS(craft), craft_flags, buf, TRUE);
	msg_to_char(ch, "Flags: &c%s&0\r\n", buf);
	
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {
		sprintbit(GET_CRAFT_BUILD_ON(craft), bld_on_flags, buf, TRUE);
		msg_to_char(ch, "Build on: &g%s&0\r\n", buf);
		sprintbit(GET_CRAFT_BUILD_FACING(craft), bld_on_flags, buf, TRUE);
		msg_to_char(ch, "Build facing: &c%s&0\r\n", buf);
	}
	
	if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING) {
		msg_to_char(ch, "Requires item: [%d] &g%s&0\r\n", GET_CRAFT_REQUIRES_OBJ(craft), skip_filler(get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(craft))));
	}

	// resources
	count = 0;
	msg_to_char(ch, "Resources required: ");
	for (iter = 0; iter < MAX_RESOURCES_REQUIRED && GET_CRAFT_RESOURCES(craft)[iter].vnum != NOTHING; ++iter) {
		msg_to_char(ch, "%s&y[%d] %s (x%d)&0", (count > 0 ? ", ": ""), GET_CRAFT_RESOURCES(craft)[iter].vnum, skip_filler(get_obj_name_by_proto(GET_CRAFT_RESOURCES(craft)[iter].vnum)), GET_CRAFT_RESOURCES(craft)[iter].amount);
		++count;
	}
	if (count == 0) {
		msg_to_char(ch, "none\r\n");
	}
	else {
		msg_to_char(ch, "\r\n");
	}
}


/**
* Show a character stats on a particular crop.
*
* @param char_data *ch The player requesting stats.
* @param crop_data *cp The crop to stat.
*/
void do_stat_crop(char_data *ch, crop_data *cp) {
	extern const char *crop_flags[];
	
	msg_to_char(ch, "Crop VNum: [&c%d&0], Name: '&c%s&0'\r\n", GET_CROP_VNUM(cp), GET_CROP_NAME(cp));
	msg_to_char(ch, "Room Title: %s, Mapout Color: %s\r\n", GET_CROP_TITLE(cp), mapout_color_names[GET_CROP_MAPOUT(cp)]);

	msg_to_char(ch, "Climate: &g%s&0\r\n", climate_types[GET_CROP_CLIMATE(cp)]);
	
	sprintbit(GET_CROP_FLAGS(cp), crop_flags, buf, TRUE);
	msg_to_char(ch, "Crop flags: &g%s&0\r\n", buf);
	
	if (GET_CROP_ICONS(cp)) {
		msg_to_char(ch, "Icons:\r\n");
		get_icons_display(GET_CROP_ICONS(cp), buf1);
		send_to_char(buf1, ch);
	}
	
	msg_to_char(ch, "Location: X-Min: [&g%d&0], X-Max: [&g%d&0], Y-Min: [&g%d&0], Y-Max: [&g%d&0]\r\n", GET_CROP_X_MIN(cp), GET_CROP_X_MAX(cp), GET_CROP_Y_MIN(cp), GET_CROP_Y_MAX(cp));
	
	if (GET_CROP_INTERACTIONS(cp)) {
		send_to_char("Interactions:\r\n", ch);
		get_interaction_display(GET_CROP_INTERACTIONS(cp), buf);
		send_to_char(buf, ch);
	}
	
	show_spawn_summary_to_char(ch, GET_CROP_SPAWNS(cp));
}


/* Gives detailed information on an object (j) to ch */
void do_stat_object(char_data *ch, obj_data *j) {
	extern const struct material_data materials[NUM_MATERIALS];
	extern const char *wear_bits[];
	extern const char *item_types[];
	extern const char *extra_bits[];
	extern const char *container_bits[];
	extern const char *obj_custom_types[];
	extern const char *storage_bits[];
	extern double get_base_dps(obj_data *weapon);
	extern double get_weapon_speed(obj_data *weapon);
	extern struct ship_data_struct ship_data[];
	extern const char *armor_types[NUM_ARMOR_TYPES+1];

	int i, found;
	room_data *room;
	obj_vnum vnum = GET_OBJ_VNUM(j);
	obj_data *j2;
	struct obj_storage_type *store;
	struct obj_custom_message *ocm;

	msg_to_char(ch, "Name: '&y%s&0', Aliases: %s\r\n", GET_OBJ_DESC(j, ch, OBJ_DESC_SHORT), GET_OBJ_KEYWORDS(j));
	
	if (OBJ_FLAGGED(j, OBJ_SCALABLE)) {
		sprintf(buf, " (%d-%d)", GET_OBJ_MIN_SCALE_LEVEL(j), GET_OBJ_MAX_SCALE_LEVEL(j));
	}
	else if (GET_OBJ_CURRENT_SCALE_LEVEL(j) > 0) {
		sprintf(buf, " (%d)", GET_OBJ_CURRENT_SCALE_LEVEL(j));
	}
	else {
		strcpy(buf, " (unscalable)");
	}
	
	msg_to_char(ch, "Gear level: [&g%.2f%s&0], VNum: [&g%5d&0], Type: &c%s&0\r\n", rate_item(j), buf, vnum, item_types[(int) GET_OBJ_TYPE(j)]);
	msg_to_char(ch, "L-Des: %s\r\n", GET_OBJ_DESC(j, ch, OBJ_DESC_LONG));

	*buf = 0;
	if (j->ex_description) {
		struct extra_descr_data *desc;
		sprintf(buf, "Extra descs:&c");
		for (desc = j->ex_description; desc; desc = desc->next) {
			strcat(buf, " ");
			strcat(buf, desc->keyword);
		}
		send_to_char(strcat(buf, "&0\r\n"), ch);
	}
	sprintbit(GET_OBJ_WEAR(j), wear_bits, buf, TRUE);
	msg_to_char(ch, "Can be worn on: &g%s&0\r\n", buf);

	sprintbit(GET_OBJ_AFF_FLAGS(j), affected_bits, buf, TRUE);
	msg_to_char(ch, "Set char bits : &y%s&0\r\n", buf);

	sprintbit(GET_OBJ_EXTRA(j), extra_bits, buf, TRUE);
	msg_to_char(ch, "Extra flags   : &g%s&0\r\n", buf);

	msg_to_char(ch, "Timer: %d, Material: %s\r\n", GET_OBJ_TIMER(j), materials[GET_OBJ_MATERIAL(j)].name);

	strcpy(buf, "In room: ");
	if (!IN_ROOM(j))
		strcat(buf, "Nowhere");
	else {
		sprintf(buf2, "%d", GET_ROOM_VNUM(IN_ROOM(j)));
		strcat(buf, buf2);
	}

	/*
	 * NOTE: In order to make it this far, we must already be able to see the
	 *       character holding the object. Therefore, we do not need CAN_SEE().
	 */
	strcat(buf, ", In object: ");
	strcat(buf, j->in_obj ? GET_OBJ_DESC(j->in_obj, ch, OBJ_DESC_SHORT) : "None");
	strcat(buf, ", Carried by: ");
	strcat(buf, j->carried_by ? GET_NAME(j->carried_by) : "Nobody");
	strcat(buf, ", Worn by: ");
	strcat(buf, j->worn_by ? GET_NAME(j->worn_by) : "Nobody");
	strcat(buf, "\r\n");
	send_to_char(buf, ch);
	
	// binding section
	if (OBJ_BOUND_TO(j)) {
		struct obj_binding *bind;
		
		msg_to_char(ch, "Bound to:");
		for (bind = OBJ_BOUND_TO(j); bind; bind = bind->next) {
			msg_to_char(ch, " %s", get_name_by_id(bind->idnum) ? CAP(get_name_by_id(bind->idnum)) : "<unknown>");
		}
		msg_to_char(ch, "\r\n");
	}

	switch (GET_OBJ_TYPE(j)) {
		case ITEM_POISON: {
			msg_to_char(ch, "Charges remaining: %d\r\n", GET_POISON_CHARGES(j));
			break;
		}
		case ITEM_WEAPON:
			msg_to_char(ch, "Speed: %.2f, Damage: %s", get_weapon_speed(j), (IS_MAGIC_ATTACK(GET_WEAPON_TYPE(j)) ? "Intelligence" : "Strength"));
			if (GET_WEAPON_DAMAGE_BONUS(j) != 0)
				msg_to_char(ch, "%+d", GET_WEAPON_DAMAGE_BONUS(j));
			
			msg_to_char(ch, " (%.2f base dps)\r\n", get_base_dps(j));

			msg_to_char(ch, "Damage type: %s\r\n", attack_hit_info[GET_WEAPON_TYPE(j)].name);
			break;
		case ITEM_ARMOR:
			msg_to_char(ch, "Armor type: %s\r\n", armor_types[GET_ARMOR_TYPE(j)]);
			break;
		case ITEM_CONTAINER:
			msg_to_char(ch, "Holds: %d items\r\n", GET_MAX_CONTAINER_CONTENTS(j));

			sprintbit(GET_CONTAINER_FLAGS(j), container_bits, buf, TRUE);
			msg_to_char(ch, "Flags: %s\r\n", buf);
			break;
		case ITEM_DRINKCON:
			msg_to_char(ch, "Contains: %d/%d drinks of %s\r\n", GET_DRINK_CONTAINER_CONTENTS(j), GET_DRINK_CONTAINER_CAPACITY(j), drinks[GET_DRINK_CONTAINER_TYPE(j)]);
			break;
		case ITEM_FOOD:
			msg_to_char(ch, "Fills for: %d hours\r\n", GET_FOOD_HOURS_OF_FULLNESS(j));
			if (IS_PLANTABLE_FOOD(j))
				msg_to_char(ch, "Plants: %s\r\n", GET_CROP_NAME(crop_proto(GET_FOOD_CROP_TYPE(j))));
			break;
		case ITEM_CORPSE:
			msg_to_char(ch, "Corpse of: ");

			if (IS_NPC_CORPSE(j)) {
				msg_to_char(ch, "%s\r\n", get_mob_name_by_proto(GET_CORPSE_NPC_VNUM(j)));
			}
			else if (IS_PC_CORPSE(j)) {
				msg_to_char(ch, "%s\r\n", get_name_by_id(GET_CORPSE_PC_ID(j)) ? CAP(get_name_by_id(GET_CORPSE_PC_ID(j))) : "a player");
			}
			else {
				msg_to_char(ch, "unknown\r\n");
			}
			break;
		case ITEM_COINS: {
			msg_to_char(ch, "Amount: %s\r\n", money_amount(real_empire(GET_COINS_EMPIRE_ID(j)), GET_COINS_AMOUNT(j)));
			break;
		}
		case ITEM_CART:
			msg_to_char(ch, "Holds: %d items\r\n", GET_MAX_CART_CONTENTS(j));
			msg_to_char(ch, "Animals required: %d\r\n", GET_CART_ANIMALS_REQUIRED(j));
			if (CART_CAN_FIRE(j)) {
				msg_to_char(ch, "Capable of firing.\r\n");
			}
			break;
		case ITEM_SHIP:
			msg_to_char(ch, "Ship type: %s\r\n", ship_data[GET_SHIP_TYPE(j)].name);
			if (GET_SHIP_RESOURCES_REMAINING(j) > 0)
				msg_to_char(ch, "Resources required to finish: %d\r\n", GET_SHIP_RESOURCES_REMAINING(j));
			msg_to_char(ch, "On deck: %d\r\n", GET_SHIP_MAIN_ROOM(j));
			break;
		case ITEM_MISSILE_WEAPON:
			msg_to_char(ch, "Speed: %.2f\r\n", missile_weapon_speed[GET_MISSILE_WEAPON_SPEED(j)]);
			msg_to_char(ch, "Damage: %d\r\n", GET_MISSILE_WEAPON_DAMAGE(j));
			msg_to_char(ch, "Arrow type: %c\r\n", 'A' + GET_MISSILE_WEAPON_TYPE(j));
			break;
		case ITEM_ARROW:
			if (GET_ARROW_QUANTITY(j) > 0) {
				msg_to_char(ch, "Quantity: %d\r\n", GET_ARROW_QUANTITY(j));
			}
			if (GET_ARROW_DAMAGE_BONUS(j) > 0) {
				msg_to_char(ch, "Damage: %+d\r\n", GET_ARROW_DAMAGE_BONUS(j));
			}
			msg_to_char(ch, "Arrow type: %c\r\n", 'A' + GET_ARROW_TYPE(j));
			break;
		case ITEM_PACK: {
			msg_to_char(ch, "Adds inventory space: %d\r\n", GET_PACK_CAPACITY(j));
			break;
		}
		case ITEM_PORTAL:
			if (j != obj_proto(GET_OBJ_VNUM(j))) {
				msg_to_char(ch, "Portal destination: ");
				room = real_room(GET_PORTAL_TARGET_VNUM(j));
				if (!room) {
					msg_to_char(ch, "None\r\n");
				}
				else {
					msg_to_char(ch, "%d - %s\r\n", GET_PORTAL_TARGET_VNUM(j), get_room_name(room, FALSE));
				}
			}
			else {
				// vstat -- just show target
				msg_to_char(ch, "%d", GET_PORTAL_TARGET_VNUM(j));
			}
			break;
		case ITEM_POTION: {
			extern const struct potion_data_type potion_data[];
			msg_to_char(ch, "Potion type: %s, Scale: %d\r\n", potion_data[GET_POTION_TYPE(j)].name, GET_POTION_SCALE(j));
			break;
		}
		case ITEM_WEALTH: {
			msg_to_char(ch, "Wealth value: %d\r\n", GET_WEALTH_VALUE(j));
			break;
		}
		default:
			msg_to_char(ch, "Values 0-2: [&g%d&0] [&g%d&0] [&g%d&0]\r\n", GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2));
			break;
	}

	/*
	 * I deleted the "equipment status" code from here because it seemed
	 * more or less useless and just takes up valuable screen space.
	 */

	if (j->contains) {
		sprintf(buf, "\r\nContents:&g");
		for (found = 0, j2 = j->contains; j2; j2 = j2->next_content) {
			sprintf(buf2, "%s %s", found++ ? "," : "", GET_OBJ_DESC(j2, ch, OBJ_DESC_SHORT));
			strcat(buf, buf2);
			if (strlen(buf) >= 62) {
				if (j2->next_content)
					send_to_char(strcat(buf, ",\r\n"), ch);
				else
					send_to_char(strcat(buf, "\r\n"), ch);
				*buf = found = 0;
			}
		}

		if (*buf)
     		send_to_char(strcat(buf, "\r\n"), ch);
		send_to_char("&0", ch);
	}
	found = 0;
	send_to_char("Applies:", ch);
	for (i = 0; i < MAX_OBJ_AFFECT; i++)
		if (j->affected[i].modifier)
			msg_to_char(ch, "%s %+d to %s", found++ ? "," : "", j->affected[i].modifier, apply_types[(int) j->affected[i].location]);
	if (!found)
		send_to_char(" None", ch);

	send_to_char("\r\n", ch);
	
	if (j->storage) {
		msg_to_char(ch, "Storage locations:");
		
		found = 0;
		for (store = j->storage; store; store = store->next) {			
			msg_to_char(ch, "%s%s", (found++ > 0 ? ", " : " "), GET_BLD_NAME(building_proto(store->building_type)));
			
			if (store->flags) {
				sprintbit(store->flags, storage_bits, buf2, TRUE);
				msg_to_char(ch, " ( %s)", buf2);
			}
		}
		
		msg_to_char(ch, "\r\n");
	}
	
	if (j->interactions) {
		send_to_char("Interactions:\r\n", ch);
		get_interaction_display(j->interactions, buf);
		send_to_char(buf, ch);
	}
	
	if (j->custom_msgs) {
		msg_to_char(ch, "Custom messages:\r\n");
		
		for (ocm = j->custom_msgs; ocm; ocm = ocm->next) {
			msg_to_char(ch, " %s: %s\r\n", obj_custom_types[ocm->type], ocm->msg);
		}
	}

	/* check the object for a script */
	do_sstat_object(ch, j);
}


/* Displays the vital statistics of IN_ROOM(ch) to ch */
void do_stat_room(char_data *ch) {
	extern char *get_mine_type_name(room_data *room);
	extern const char *exit_bits[];
	extern const char *depletion_type[NUM_DEPLETION_TYPES];
	extern const char *room_extra_types[];
	
	struct depletion_data *dep;
	struct empire_city_data *city;
	int found;
	bool comma;
	obj_data *j;
	char_data *k;
	crop_data *cp;
	empire_data *emp;
	struct affected_type *aff;
	struct room_extra_data *red;
	room_data *home = HOME_ROOM(IN_ROOM(ch));


	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_HAS_CROP_DATA) && (cp = crop_proto(get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CROP_TYPE)))) {
		strcpy(buf2, GET_CROP_NAME(cp));
		CAP(buf2);
	}
	else {
		strcpy(buf2, GET_SECT_NAME(SECT(IN_ROOM(ch))));
	}
	msg_to_char(ch, "(%d, %d) %s (&c%s&0/&c%s&0)\r\n", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)), get_room_name(IN_ROOM(ch), FALSE), buf2, GET_SECT_NAME(ROOM_ORIGINAL_SECT(IN_ROOM(ch))));
	msg_to_char(ch, "VNum: [&g%d&0], Island: [%d]\r\n", GET_ROOM_VNUM(IN_ROOM(ch)), GET_ISLAND_ID(home));
	
	if (home != IN_ROOM(ch)) {
		msg_to_char(ch, "Home room: &g%d&0 %s\r\n", GET_ROOM_VNUM(home), get_room_name(home, FALSE));
	}
	
	if (ROOM_CUSTOM_NAME(IN_ROOM(ch)) || ROOM_CUSTOM_ICON(IN_ROOM(ch)) || ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))) {
		msg_to_char(ch, "Custom:&y%s%s%s&0\r\n", ROOM_CUSTOM_NAME(IN_ROOM(ch)) ? " NAME" : "", ROOM_CUSTOM_ICON(IN_ROOM(ch)) ? " ICON" : "", ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch)) ? " DESCRIPTION" : "");
	}

	// ownership
	if ((emp = ROOM_OWNER(home))) {
		msg_to_char(ch, "Owner: %s%s&0", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		if ((city = find_city(emp, IN_ROOM(ch)))) {
			msg_to_char(ch, ", City: %s", city->name);
		}
		if (ROOM_PRIVATE_OWNER(home) != NOBODY) {
			msg_to_char(ch, ", Home: %s", get_name_by_id(ROOM_PRIVATE_OWNER(home)) ? CAP(get_name_by_id(ROOM_PRIVATE_OWNER(home))) : "someone");
		}
		
		send_to_char("\r\n", ch);
	}

	if (ROOM_AFF_FLAGS(IN_ROOM(ch))) {	
		sprintbit(ROOM_AFF_FLAGS(IN_ROOM(ch)), room_aff_bits, buf2, TRUE);
		msg_to_char(ch, "Status: &c%s&0\r\n", buf2);
	}
	
	if (COMPLEX_DATA(IN_ROOM(ch))) {
		if (GET_INSIDE_ROOMS(home) > 0) {
			msg_to_char(ch, "Designated rooms: %d\r\n", GET_INSIDE_ROOMS(home));
		}
		msg_to_char(ch, "Burning: %d, Damage: %d, Disrepair: %d year%s\r\n", BUILDING_BURNING(home), BUILDING_DAMAGE(home), BUILDING_DISREPAIR(home), BUILDING_DISREPAIR(home) != 1 ? "s" : "");
	}

	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_CAN_MINE) || ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_MINE)) {
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_TYPE) == MINE_NOT_SET) {
			msg_to_char(ch, "This area is unmined.\r\n");
		}
		else {
			msg_to_char(ch, "Mineral: %s, Amount remaining: %d\r\n", get_mine_type_name(IN_ROOM(ch)), get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT));
		}
	}

	sprintf(buf, "Chars present:&y");
	for (found = 0, k = ROOM_PEOPLE(IN_ROOM(ch)); k; k = k->next_in_room) {
		if (!CAN_SEE(ch, k))
			continue;
		sprintf(buf2, "%s %s(%s)", found++ ? "," : "", GET_NAME(k), (!IS_NPC(k) ? "PC" : (!IS_MOB(k) ? "NPC" : "MOB")));
		strcat(buf, buf2);
		if (strlen(buf) >= 62) {
			if (k->next_in_room)
				send_to_char(strcat(buf, ",\r\n"), ch);
			else
				send_to_char(strcat(buf, "\r\n"), ch);
			*buf = found = 0;
		}
	}
	msg_to_char(ch, "&0");

	if (*buf)
		send_to_char(strcat(buf, "\r\n&0"), ch);

	if (ROOM_CONTENTS(IN_ROOM(ch))) {
		sprintf(buf, "Contents:&g");
		for (found = 0, j = ROOM_CONTENTS(IN_ROOM(ch)); j; j = j->next_content) {
			if (!CAN_SEE_OBJ(ch, j))
				continue;
			sprintf(buf2, "%s %s", found++ ? "," : "", GET_OBJ_DESC(j, ch, OBJ_DESC_SHORT));
			strcat(buf, buf2);
			if (strlen(buf) >= 62) {
				if (j->next_content)
					send_to_char(strcat(buf, ",\r\n"), ch);
				else
					send_to_char(strcat(buf, "\r\n"), ch);
				*buf = found = 0;
			}
		}

		if (*buf)
			send_to_char(strcat(buf, "\r\n"), ch);
		send_to_char("&0", ch);
	}
	if (COMPLEX_DATA(IN_ROOM(ch))) {
		struct room_direction_data *ex;
		for (ex = COMPLEX_DATA(IN_ROOM(ch))->exits; ex; ex = ex->next) {
			if (ex->to_room == NOWHERE)
				sprintf(buf1, " &cNONE&0");
			else
				sprintf(buf1, "&c%5d&0", ex->to_room);
			sprintbit(ex->exit_info, exit_bits, buf2, TRUE);
			sprintf(buf, "Exit &c%-5s&0:  To: [%s], Keyword: %s, Type: %s\r\n", dirs[get_direction_for_char(ch, ex->dir)], buf1, ex->keyword ? ex->keyword : "None", buf2);
			send_to_char(buf, ch);
		}
	}
	
	if (ROOM_DEPLETION(IN_ROOM(ch))) {
		send_to_char("Depletion: ", ch);
		
		comma = FALSE;
		for (dep = ROOM_DEPLETION(IN_ROOM(ch)); dep; dep = dep->next) {
			if (dep->type < NUM_DEPLETION_TYPES) {
				msg_to_char(ch, "%s%s (%d)", comma ? ", " : "", depletion_type[dep->type], dep->count);
				comma = TRUE;
			}
		}
		send_to_char("\r\n", ch);
	}

	/* Routine to show what spells a room is affected by */
	if (ROOM_AFFECTS(IN_ROOM(ch))) {
		for (aff = ROOM_AFFECTS(IN_ROOM(ch)); aff; aff = aff->next) {
			*buf2 = '\0';

			sprintf(buf, "Affect: (%3dhr) &c%s&0 ", aff->duration + 1, affect_types[aff->type]);

			if (aff->modifier) {
				sprintf(buf2, "%+d to %s", aff->modifier, apply_types[(int) aff->location]);
				strcat(buf, buf2);
			}
			if (aff->bitvector) {
				if (*buf2)
					strcat(buf, ", sets ");
				else
					strcat(buf, "sets ");
				sprintbit(aff->bitvector, room_aff_bits, buf2, TRUE);
				strcat(buf, buf2);
			}
			send_to_char(strcat(buf, "\r\n"), ch);
		}
	}
	
	if (IN_ROOM(ch)->extra_data) {
		msg_to_char(ch, "Extra data:\r\n");
		
		for (red = IN_ROOM(ch)->extra_data; red; red = red->next) {
			sprinttype(red->type, room_extra_types, buf);
			msg_to_char(ch, " %s: %d\r\n", buf, red->value);
		}
	}

	/* check the room for a script */
	do_sstat_room(ch);
}


/**
* @param char_data *ch The player requesting stats.
* @param room_template *rmt The room template to display.
*/
void do_stat_room_template(char_data *ch, room_template *rmt) {
	extern adv_data *get_adventure_for_vnum(rmt_vnum vnum);
	void get_exit_template_display(struct exit_template *list, char *save_buffer);
	void get_script_display(struct trig_proto_list *list, char *save_buffer);
	void get_template_spawns_display(struct adventure_spawn *list, char *save_buffer);
	extern const char *room_template_flags[];
	
	char lbuf[MAX_STRING_LENGTH];
	adv_data *adv;
	
	if (!rmt) {
		return;
	}
	
	msg_to_char(ch, "VNum: [&c%d&0], Title: %s\r\n", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt));
	msg_to_char(ch, "%s", NULLSAFE(GET_RMT_DESC(rmt)));

	adv = get_adventure_for_vnum(GET_RMT_VNUM(rmt));
	msg_to_char(ch, "Adventure: [&c%d %s&0]\r\n", !adv ? NOTHING : GET_ADV_VNUM(adv), !adv ? "NONE" : GET_ADV_NAME(adv));
	
	sprintbit(GET_RMT_FLAGS(rmt), room_template_flags, lbuf, TRUE);
	msg_to_char(ch, "Flags: &g%s&0\r\n", lbuf);

	sprintbit(GET_RMT_BASE_AFFECTS(rmt), room_aff_bits, lbuf, TRUE);
	msg_to_char(ch, "Affects: &y%s&0\r\n", lbuf);
	
	if (GET_RMT_EX_DESCS(rmt)) {
		struct extra_descr_data *desc;
		sprintf(buf, "Extra descs:&c");
		for (desc = GET_RMT_EX_DESCS(rmt); desc; desc = desc->next) {
			strcat(buf, " ");
			strcat(buf, desc->keyword);
		}
		msg_to_char(ch, "%s&0\r\n", buf);
	}

	get_exit_template_display(GET_RMT_EXITS(rmt), lbuf);
	msg_to_char(ch, "Exits:\r\n%s", lbuf);
	
	if (GET_RMT_INTERACTIONS(rmt)) {
		send_to_char("Interactions:\r\n", ch);
		get_interaction_display(GET_RMT_INTERACTIONS(rmt), buf);
		send_to_char(buf, ch);
	}
	
	get_template_spawns_display(GET_RMT_SPAWNS(rmt), lbuf);
	msg_to_char(ch, "Spawns:\r\n%s", lbuf);

	get_script_display(GET_RMT_SCRIPTS(rmt), lbuf);
	msg_to_char(ch, "Scripts:\r\n%s", lbuf);
}


/**
* Show a character stats on a particular sector.
*
* @param char_data *ch The player requesting stats.
* @param sector_data *st The sector to stat.
*/
void do_stat_sector(char_data *ch, sector_data *st) {
	void get_evolution_display(struct evolution_data *list, char *save_buffer);

	extern const char *sector_flags[];
	
	msg_to_char(ch, "Sector VNum: [&c%d&0], Name: '&c%s&0'\r\n", st->vnum, st->name);
	msg_to_char(ch, "Room Title: %s\r\n", st->title);
	
	if (st->commands && *st->commands) {
		msg_to_char(ch, "Command list: &c%s&0\r\n", st->commands);
	}
	
	msg_to_char(ch, "Movement cost: [&g%d&0]  Roadside Icon: %c  Mapout Color: %s\r\n", st->movement_loss, st->roadside_icon, mapout_color_names[GET_SECT_MAPOUT(st)]);

	msg_to_char(ch, "Climate: &g%s&0\r\n", climate_types[st->climate]);
	
	if (st->icons) {
		msg_to_char(ch, "Icons:\r\n");
		get_icons_display(st->icons, buf1);
		send_to_char(buf1, ch);
	}
	
	sprintbit(st->flags, sector_flags, buf, TRUE);
	msg_to_char(ch, "Sector flags: &g%s&0\r\n", buf);
	
	sprintbit(st->build_flags, bld_on_flags, buf, TRUE);
	msg_to_char(ch, "Build flags: &g%s&0\r\n", buf);
	
	if (st->evolution) {
		msg_to_char(ch, "Evolution information:\r\n");
		get_evolution_display(st->evolution, buf1);
		send_to_char(buf1, ch);
	}

	if (st->interactions) {
		send_to_char("Interactions:\r\n", ch);
		get_interaction_display(st->interactions, buf);
		send_to_char(buf, ch);
	}
	
	show_spawn_summary_to_char(ch, st->spawns);
}


 //////////////////////////////////////////////////////////////////////////////
//// VNUM SEARCHES ///////////////////////////////////////////////////////////

/**
* Searches the adventure db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_adventure(char *searchname, char_data *ch) {
	adv_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, adventure_table, iter, next_iter) {
		if (multi_isname(searchname, GET_ADV_NAME(iter)) || multi_isname(searchname, GET_ADV_AUTHOR(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s (%d-%d)\r\n", ++found, GET_ADV_VNUM(iter), GET_ADV_NAME(iter), GET_ADV_START_VNUM(iter), GET_ADV_END_VNUM(iter));
		}
	}
	
	return found;
}


/**
* Searches the building db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_building(char *searchname, char_data *ch) {
	bld_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, building_table, iter, next_iter) {
		if (multi_isname(searchname, GET_BLD_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, GET_BLD_VNUM(iter), GET_BLD_NAME(iter));
		}
	}
	
	return found;
}


/**
* Searches the craft db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_craft(char *searchname, char_data *ch) {
	craft_data *iter, *next_iter;
	obj_data *or;
	int found = 0;
	
	HASH_ITER(hh, craft_table, iter, next_iter) {
		if (multi_isname(searchname, GET_CRAFT_NAME(iter)) || ((or = obj_proto(GET_CRAFT_OBJECT(iter))) && multi_isname(searchname, GET_OBJ_KEYWORDS(or)))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, GET_CRAFT_VNUM(iter), GET_CRAFT_NAME(iter));
		}
	}
	
	return found;
}


/**
* Searches the crop db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_crop(char *searchname, char_data *ch) {
	crop_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, crop_table, iter, next_iter) {
		if (multi_isname(searchname, GET_CROP_NAME(iter)) || multi_isname(searchname, GET_CROP_TITLE(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, GET_CROP_VNUM(iter), GET_CROP_NAME(iter));
		}
	}
	
	return found;
}


int vnum_mobile(char *searchname, char_data *ch) {
	char_data *mob, *next_mob;
	int found = 0;
	
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		if (multi_isname(searchname, mob->player.name)) {
			msg_to_char(ch, "%3d. [%5d] %s%s\r\n", ++found, mob->vnum, mob->proto_script ? "[TRIG] " : "", mob->player.short_descr);
		}
	}

	return (found);
}


int vnum_object(char *searchname, char_data *ch) {
	obj_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, object_table, iter, next_iter) {
		if (multi_isname(searchname, GET_OBJ_KEYWORDS(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s%s\r\n", ++found, GET_OBJ_VNUM(iter), iter->proto_script ? "[TRIG] " : "", GET_OBJ_SHORT_DESC(iter));
		}
	}
	return (found);
}


/**
* Searches the room template db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_room_template(char *searchname, char_data *ch) {
	room_template *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, room_template_table, iter, next_iter) {
		if (multi_isname(searchname, GET_RMT_TITLE(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s%s\r\n", ++found, GET_RMT_VNUM(iter), iter->proto_script ? "[TRIG] " : "", GET_RMT_TITLE(iter));
		}
	}
	
	return found;
}


/**
* Searches the sector db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_sector(char *searchname, char_data *ch) {
	sector_data *sect, *next_sect;
	int found = 0;
	
	HASH_ITER(hh, sector_table, sect, next_sect) {
		if (multi_isname(searchname, GET_SECT_NAME(sect)) || multi_isname(searchname, GET_SECT_TITLE(sect))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, GET_SECT_VNUM(sect), GET_SECT_NAME(sect));
		}
	}
	
	return found;
}


/**
* Searches the trigger db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_trigger(char *searchname, char_data *ch) {
	trig_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, trigger_table, iter, next_iter) {
		if (multi_isname(searchname, GET_TRIG_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, GET_TRIG_VNUM(iter), GET_TRIG_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////


ACMD(do_advance) {
	void start_new_character(char_data *ch);
	char_data *victim;
	char *name = arg, *level = buf2;
	int newlevel, oldlevel, iter;

	two_arguments(argument, name, level);

	if (*name) {
   		if (!(victim = get_char_vis(ch, name, FIND_CHAR_WORLD))) {
			send_to_char("That player is not here.\r\n", ch);
			return;
		}
	}
	else {
		send_to_char("Advance whom?\r\n", ch);
		return;
	}

	if (GET_ACCESS_LEVEL(ch) <= GET_ACCESS_LEVEL(victim)) {
		send_to_char("Maybe that's not such a great idea.\r\n", ch);
		return;
	}
	if (IS_NPC(victim)) {
		send_to_char("NO! Not on NPC's.\r\n", ch);
		return;
	}
	if (!*level || (newlevel = atoi(level)) <= 0) {
		send_to_char("That's not a level!\r\n", ch);
		return;
	}
	if (newlevel > LVL_IMPL) {
		sprintf(buf, "%d is the highest possible level.\r\n", LVL_IMPL);
		send_to_char(buf, ch);
		return;
	}
	if (newlevel > GET_ACCESS_LEVEL(ch)) {
		send_to_char("Yeah, right.\r\n", ch);
		return;
	}
	if (newlevel == GET_ACCESS_LEVEL(victim)) {
		send_to_char("They are already at that level.\r\n", ch);
		return;
	}
	oldlevel = GET_ACCESS_LEVEL(victim);
	if (newlevel < GET_ACCESS_LEVEL(victim)) {
		start_new_character(victim);
		GET_ACCESS_LEVEL(victim) = newlevel;
		GET_IMMORTAL_LEVEL(victim) = IS_IMMORTAL(victim) ? LVL_TOP - GET_ACCESS_LEVEL(victim) : -1;
		send_to_char("You are momentarily enveloped by darkness!\r\nYou feel somewhat diminished.\r\n", victim);
	}
	else {
		act("$n makes some strange gestures.\r\n"
			"A strange feeling comes upon you,\r\n"
			"Like a giant hand, light comes down\r\n"
			"from above, grabbing your body, that\r\n"
			"begins to pulse with colored lights\r\n"
			"from inside.\r\n\r\n"
			"Your head seems to be filled with demons\r\n"
			"from another plane as your body dissolves\r\n"
			"to the elements of time and space itself.\r\n"
			"Suddenly a silent explosion of light\r\n"
			"snaps you back to reality.\r\n\r\n"
			"You feel slightly different.", FALSE, ch, 0, victim, TO_VICT);
	}

	send_config_msg(ch, "ok_string");

	if (newlevel < oldlevel)
		syslog(SYS_LVL, GET_INVIS_LEV(ch), TRUE, "LVL: %s demoted %s from level %d to %d.", GET_NAME(ch), GET_NAME(victim), oldlevel, newlevel);
	else
		syslog(SYS_LVL, GET_INVIS_LEV(ch), TRUE, "LVL: %s has promoted %s to level %d (from %d)", GET_NAME(ch), GET_NAME(victim), newlevel, oldlevel);

	if (oldlevel < LVL_START_IMM && newlevel >= LVL_START_IMM) {
		SET_BIT(PRF_FLAGS(victim), PRF_HOLYLIGHT | PRF_ROOMFLAGS | PRF_NOHASSLE);
		
		// turn on all syslogs
		for (iter = 0; *syslog_types[iter] != '\n'; ++iter) {
			SYSLOG_FLAGS(victim) |= BIT(iter);
		}
	}

	GET_ACCESS_LEVEL(victim) = newlevel;
	GET_IMMORTAL_LEVEL(victim) = IS_IMMORTAL(victim) ? LVL_TOP - GET_ACCESS_LEVEL(victim) : -1;
	SAVE_CHAR(victim);
	check_autowiz(victim);
}


ACMD(do_at) {
	room_data *location, *original_loc;

	argument = one_word(argument, buf);

	if (!*buf) {
		send_to_char("You must supply a room number or a name.\r\n", ch);
		return;
	}

	if (!*argument) {
		send_to_char("What do you want to do there?\r\n", ch);
		return;
	}

	if (!(location = find_target_room(ch, buf)))
		return;

	/* a location has been found. */
	original_loc = IN_ROOM(ch);
	char_from_room(ch);
	char_to_room(ch, location);
	command_interpreter(ch, argument);

	/* check if the char is still there */
	if (IN_ROOM(ch) == location) {
		char_from_room(ch);
		char_to_room(ch, original_loc);
	}
}


ACMD(do_authorize) {
	void save_char_file_u(struct char_file_u *st);
	struct char_file_u chdata;
	char_data *vict;
	int id;

	one_argument(argument, arg);

	if (!*arg)
		msg_to_char(ch, "Usage: authorize <character>\r\n");
	else if (!(id = get_id_by_name(arg)))
		msg_to_char(ch, "Unable to find character '%s'?\r\n", arg);
	else {
		if ((vict = is_playing(id))) {
			if (GET_ACCESS_LEVEL(vict) < LVL_APPROVED) {
				GET_ACCESS_LEVEL(vict) = LVL_APPROVED;
			}
			SAVE_CHAR(vict);
			msg_to_char(vict, "Your character has been authorized.\r\n");
			mortlog("%s has been authorized!", PERS(vict, vict, 1));
		}
		else {
			if ((load_char(arg, &chdata)) > NOBODY) {
				if (chdata.char_specials_saved.idnum > 0) {
					if (chdata.access_level < LVL_APPROVED) {
						chdata.access_level = LVL_APPROVED;
					}
					save_char_file_u(&chdata);
				}
				else {
					msg_to_char(ch, "You can't authorize someone with idnum 0.\r\n");
					return;
				}
			}
			else {
				msg_to_char(ch, "No player by that name.\r\n");
				return;
			}
		}
		syslog(SYS_VALID, GET_INVIS_LEV(ch), TRUE, "VALID: %s has been authorized by %s", CAP(arg), GET_NAME(ch));
		msg_to_char(ch, "%s authorized.\r\n", CAP(arg));
	}
}


ACMD(do_autostore) {
	void read_vault(empire_data *emp);
	obj_data *obj, *next_obj;
	empire_data *emp = ROOM_OWNER(IN_ROOM(ch));

	one_argument(argument, arg);

	if (!emp) {
		msg_to_char(ch, "Nobody owns this spot. Use purge instead.\r\n");
	}
	else if (*arg) {
		if ((obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL) {
			act("$n auto-stores $p.", FALSE, ch, obj, 0, TO_ROOM);
			perform_autostore(obj, emp, GET_ISLAND_ID(IN_ROOM(ch)));
		}
		else {
			send_to_char("Nothing here by that name.\r\n", ch);
			return;
		}

		send_config_msg(ch, "ok_string");
	}
	else {			// no argument. clean out the room
		act("$n gestures...", FALSE, ch, 0, 0, TO_ROOM);
		send_to_room("The world seems a little cleaner.\r\n", IN_ROOM(ch));

		for (obj = ROOM_CONTENTS(IN_ROOM(ch)); obj; obj = next_obj) {
			next_obj = obj->next_content;
			perform_autostore(obj, emp, GET_ISLAND_ID(IN_ROOM(ch)));
		}
		
		read_vault(emp);
	}
}


ACMD(do_autowiz) {
	check_autowiz(ch);
}


ACMD(do_clearabilities) {
	extern int find_skill_by_name(char *name);
	
	char_data *vict;
	char typestr[MAX_STRING_LENGTH];
	int skill = NO_SKILL;
	bool all;

	argument = one_argument(argument, arg);
	skip_spaces(&argument);
	vict = get_player_vis(ch, arg, FIND_CHAR_WORLD);
	
	if (!vict || !*argument) {
		msg_to_char(ch, "Usage: clearabilities <name> <skill | all>\r\n");
		return;
	}
	if (!str_cmp(argument, "all")) {
		all = TRUE;
	}
	else {
		all = FALSE;
		if ((skill = find_skill_by_name(argument)) == NO_SKILL) {
			msg_to_char(ch, "Invalid skill '%s'.\r\n", argument);
			return;
		}
	}
	
	clear_char_abilities(vict, all ? NO_SKILL : skill);
	
	if (all) {
		*typestr = '\0';
	}
	else {
		sprintf(typestr, "%s ", skill_data[skill].name);
	}
	
	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has cleared %s's %sabilities", GET_REAL_NAME(ch), GET_REAL_NAME(vict), typestr);
	msg_to_char(vict, "Your %sabilities have been reset.\r\n", typestr);
	msg_to_char(ch, "You have cleared %s's %sabilities.\r\n", GET_REAL_NAME(vict), typestr);
}


ACMD(do_date) {
	extern time_t boot_time;
	char *tmstr;
	time_t mytime;
	int d, h, m;

	if (subcmd == SCMD_DATE)
		mytime = time(0);
	else
		mytime = boot_time;

	tmstr = (char *) asctime(localtime(&mytime));
	*(tmstr + strlen(tmstr) - 1) = '\0';

	if (subcmd == SCMD_DATE)
		sprintf(buf, "Current machine time: %s\r\n", tmstr);
	else {
		mytime = time(0) - boot_time;
		d = mytime / 86400;
		h = (mytime / 3600) % 24;
		m = (mytime / 60) % 60;

		sprintf(buf, "Up since %s: %d day%s, %d:%02d\r\n", tmstr, d, ((d == 1) ? "" : "s"), h, m);
	}

	send_to_char(buf, ch);
}


ACMD(do_dc) {
	descriptor_data *d;
	int num_to_dc;

	one_argument(argument, arg);
	if (!(num_to_dc = atoi(arg))) {
		send_to_char("Usage: DC <user number> (type USERS for a list)\r\n", ch);
		return;
	}
	for (d = descriptor_list; d && d->desc_num != num_to_dc; d = d->next);

	if (!d) {
		send_to_char("No such connection.\r\n", ch);
		return;
	}
	if (d->character && GET_ACCESS_LEVEL(d->character) >= GET_ACCESS_LEVEL(ch)) {
		if (!CAN_SEE(ch, d->character))
			send_to_char("No such connection.\r\n", ch);
		else
			send_to_char("Umm.. maybe that's not such a good idea...\r\n", ch);
		return;
	}

	/* We used to just close the socket here using close_socket(), but
	 * various people pointed out this could cause a crash if you're
	 * closing the person below you on the descriptor list.  Just setting
	 * to CON_CLOSE leaves things in a massively inconsistent state so I
	 * had to add this new flag to the descriptor. -je
	 *
	 * It is a much more logical extension for a CON_DISCONNECT to be used
	 * for in-game socket closes and CON_CLOSE for out of game closings.
	 * This will retain the stability of the close_me hack while being
	 * neater in appearance. -gg 12/1/97
	 *
	 * For those unlucky souls who actually manage to get disconnected
	 * by two different immortals in the same 1/10th of a second, we have
	 * the below 'if' check. -gg 12/17/99
	 */
	if (STATE(d) == CON_DISCONNECT || STATE(d) == CON_CLOSE)
		send_to_char("They're already being disconnected.\r\n", ch);
	else {
		/*
		 * Remember that we can disconnect people not in the game and
		 * that rather confuses the code when it expected there to be
		 * a character context.
		 */
		if (STATE(d) == CON_PLAYING)
			STATE(d) = CON_DISCONNECT;
		else
			STATE(d) = CON_CLOSE;

		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has disconnected %s", GET_REAL_NAME(ch), (d->character ? GET_REAL_NAME(d->character) : "an empty connection"));
		sprintf(buf, "Connection #%d closed.\r\n", num_to_dc);
		send_to_char(buf, ch);
		log("Connection closed by %s.", GET_NAME(ch));
	}
}


ACMD(do_distance) {
	char arg[MAX_INPUT_LENGTH];
	room_data *target;
	int dir;
	
	one_word(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Measure distance to where?\r\n");
	}
	else if (!(target = find_target_room(ch, arg))) {
		msg_to_char(ch, "Unknown target.\r\n");
	}
	else {	
		dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), target));
		msg_to_char(ch, "Distance to (%d, %d): %d tiles %s.\r\n", X_COORD(target), Y_COORD(target), compute_distance(IN_ROOM(ch), target), (dir == NO_DIR ? "away" : dirs[dir]));
	}
}


// this also handles emote
ACMD(do_echo) {
	void add_to_channel_history(descriptor_data *desc, int type, char *message);
	void clear_last_act_message(descriptor_data *desc);
	extern bool is_ignoring(char_data *ch, char_data *victim);
	
	char string[MAX_INPUT_LENGTH], lbuf[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH];
	char hbuf[MAX_INPUT_LENGTH];
	char *ptr, *end;
	char_data *vict = NULL, *tmp_char, *c;
	obj_data *obj = NULL;
	int len;
	bool needs_name = TRUE;

	skip_spaces(&argument);
	delete_doubledollar(argument);
	strcpy(string, argument);

	if (!*argument) {
		send_to_char("Yes.. but what?\r\n", ch);
		return;
	}

	// emote features like $n to move your name, @target, #item
	if (subcmd == SCMD_ECHO || strstr(string, "$n")) {
		needs_name = FALSE;
	}
	// find target
	if ((ptr = strchr(string, '@')) && *(ptr+1) != ' ') {
		one_argument(ptr+1, lbuf);
		if ((end = strchr(lbuf, '.')) || (end = strchr(lbuf, '!')) || (end = strchr(lbuf, ','))) {
			*end = '\0';
		}
		len = strlen(lbuf);
		vict = get_char_vis(ch, lbuf, FIND_CHAR_ROOM);

		if (vict) {		
			// replace with $N
			*ptr = '\0';
			strcpy(temp, string);
			strcat(temp, "$N");
			strcat(temp, ptr + len + 1);
			strcpy(string, temp);
		}
	}
	if ((ptr = strchr(string, '#')) && *(ptr+1) != ' ') {
		one_argument(ptr+1, lbuf);
		if ((end = strchr(lbuf, '.')) || (end = strchr(lbuf, '!')) || (end = strchr(lbuf, ','))) {
			*end = '\0';
		}
		len = strlen(lbuf);
		generic_find(lbuf, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &tmp_char, &obj);
		
		if (obj) {
			// replace with $p
			*ptr = '\0';
			strcpy(temp, string);
			strcat(temp, "$p");
			strcat(temp, ptr + len + 1);
			strcpy(string, temp);
		}
	}

	if (needs_name) {
		sprintf(lbuf, "$n %s", string);
	}
	else {
		strcpy(lbuf, string);
	}
	
	strcat(lbuf, "&0");
	
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		if (ch->desc) {
			clear_last_act_message(ch->desc);
		}
		
		if (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) {
			msg_to_char(ch, "&%c", GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE));
		}
		
		// send message
		act(lbuf, FALSE, ch, obj, vict, TO_CHAR | TO_IGNORE_BAD_CODE);
		
		// channel history
		if (ch->desc && ch->desc->last_act_message) {
			// the message was sent via act(), we can retrieve it from the desc			
			if (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) {
				sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE));
			}
			else {
				*hbuf = '\0';
			}
			strcat(hbuf, ch->desc->last_act_message);
			add_to_channel_history(ch->desc, CHANNEL_HISTORY_SAY, hbuf);
		}
	}

	if (vict) {
		// clear last act messages for everyone in the room
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
			if (c->desc && c != ch && c != vict) {
				clear_last_act_message(c->desc);
				
				if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					msg_to_char(c, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
				}
			}
		}
		
		act(lbuf, FALSE, ch, obj, vict, TO_NOTVICT | TO_IGNORE_BAD_CODE);

		// fetch and store channel history for the room
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
			if (c->desc && c != ch && c != vict && !is_ignoring(c, ch) && c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
				}
				else {
					*hbuf = '\0';
				}
				strcat(hbuf, c->desc->last_act_message);
				add_to_channel_history(c->desc, CHANNEL_HISTORY_SAY, hbuf);
			}
			else if (c->desc && c != ch && c != vict) {
				// just in case
				msg_to_char(c, "&0");
			}
		}
		
		// prepare to send to vict
		if ((ptr = strstr(lbuf, "$N"))) {
			*ptr = '\0';
			strcpy(temp, lbuf);
			strcat(temp, "you");
			strcat(temp, ptr+2);
			
			strcpy(lbuf, temp);
		}
		
		if (!is_ignoring(vict, ch)) {
			if (vict->desc) {
				clear_last_act_message(vict->desc);
			}
			
			if (!IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE)) {
				msg_to_char(vict, "&%c", GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE));
			}
		
			// send to vict
			act(lbuf, FALSE, ch, obj, vict, TO_VICT | TO_IGNORE_BAD_CODE);
		
			// channel history
			if (vict->desc && vict->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc			
				if (!IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE)) {
					sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE));
				}
				else {
					*hbuf = '\0';
				}
				strcat(hbuf, vict->desc->last_act_message);
				add_to_channel_history(vict->desc, CHANNEL_HISTORY_SAY, hbuf);
			}
		}
	}
	else {
		// clear last act messages for everyone in the room
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
			if (c->desc && c != ch) {
				clear_last_act_message(c->desc);
							
				if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					msg_to_char(c, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
				}
			}
		}
		
		// send to room
		act(lbuf, FALSE, ch, obj, vict, TO_ROOM | TO_NOT_IGNORING | TO_IGNORE_BAD_CODE);

		// fetch and store channel history for the room
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
			if (c->desc && c != ch && !is_ignoring(c, ch) && c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc			
				if (!IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
				}
				else {
					*hbuf = '\0';
				}
				strcat(hbuf, c->desc->last_act_message);
				add_to_channel_history(c->desc, CHANNEL_HISTORY_SAY, hbuf);
			}
			else if (c->desc && c != ch) {
				// just in case
				msg_to_char(c, "&0");
			}
		}
	}
}


ACMD(do_edelete) {
	empire_data *e;

	skip_spaces(&argument);
	if (!*argument)
		msg_to_char(ch, "What empire do you want to delete?\r\n");
	else if (!(e = get_empire_by_name(argument)))
		msg_to_char(ch, "Invalid empire.\r\n");
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has deleted empire %s", GET_NAME(ch), EMPIRE_NAME(e));
		delete_empire(e);
		msg_to_char(ch, "Empire deleted.\r\n");
	}
}


ACMD(do_editnotes) {
	char_data *vict = NULL;
	bool file = FALSE;
	char **write;
	
	one_argument(argument, arg);
	
	if (!ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (ch->desc->str) {
		msg_to_char(ch, "You're already editing something else.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Edit notes for whom?\r\n");
	}
	else if (!(vict = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else if (GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "You cannot edit notes for players of that level.\r\n");
	}
	else {
		// duplicate the str -- the victim will be un-loaded, so we can't edit it directly
		CREATE(write, char*, 1);
		*write = str_dup(GET_ADMIN_NOTES(vict));
		
		sprintf(buf, "notes for %s", GET_NAME(vict));
		start_string_editor(ch->desc, buf, write, MAX_ADMIN_NOTES_LENGTH-1);
		ch->desc->notes_id = GET_IDNUM(vict);
	
		act("$n begins editing some notes.", TRUE, ch, 0, 0, TO_ROOM);
	}
	
	if (vict && file) {
		free_char(vict);
	}
}


/*
 * do_file
 *  by Haddixx <haddixx@megamed.com>
 * A neat little utility that looks up the last X lines of a given file.
 * I've made some minor improvements to it.. I can't just leave well-enough
 * alone.  Anyway, it's a useful toy and I'm sure EmpireMUD users will love
 * it.
 *
 * see config.c for the list of loadable files
 */
ACMD(do_file) {
	extern struct file_lookup_struct file_lookup[];
	
	FILE *req_file;
 	int cur_line = 0, num_lines = 0, req_lines = 0, i, l;
	char field[MAX_INPUT_LENGTH], value[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH];

	skip_spaces(&argument);

	if (!*argument) {
		msg_to_char(ch, "USAGE: file <option> <num lines>\r\n\r\nFile options:\r\n");
		for (i = 1; file_lookup[i].level; i++)
			if (file_lookup[i].level <= GET_ACCESS_LEVEL(ch))
				msg_to_char(ch, "%-15s%s\r\n", file_lookup[i].cmd, file_lookup[i].file);
		return;
	}

	strcpy(arg, two_arguments(argument, field, value));

	for (l = 0; *file_lookup[l].cmd != '\n' && strn_cmp(field, file_lookup[l].cmd, strlen(field)); l++);

	if (*file_lookup[l].cmd == '\n') {
		msg_to_char(ch, "That is not a valid option!\r\n");
		return;
	}

	if (GET_ACCESS_LEVEL(ch) < file_lookup[l].level) {
		msg_to_char(ch, "You are not godly enough to view that file!\r\n");
		return;
	}

	req_lines = (*value ? atoi(value) : 15);

	/* open the requested file */
	if (!(req_file = fopen(file_lookup[l].file, "r"))) {
		syslog(SYS_ERROR, GET_INVIS_LEV(ch), TRUE, "SYSERR: Error opening file %s using 'file' command.", file_lookup[l].file);
		return;
	}

	/* count lines in requested file */
	get_line(req_file,buf);
	while (!feof(req_file)) {
		num_lines++;
		get_line(req_file,buf);
	}
	fclose(req_file);

	/* Limit # of lines printed to # requested or # of lines in file or 150 lines */
	req_lines = MIN(150, MIN(req_lines, num_lines));

	/* re-open */
	req_file = fopen(file_lookup[l].file, "r");
	*output = '\0';

	/* and print the requested lines */
	get_line(req_file, buf);
	while (!feof(req_file) && (strlen(output) + strlen(buf) + 2) < MAX_STRING_LENGTH) {
		cur_line++;
		if (cur_line > (num_lines - req_lines)) {
			strcat(output, buf);
			strcat(output, "\r\n");
		}
		get_line(req_file, buf);
	}
	page_string(ch->desc, output, 1);

	fclose(req_file);
}


ACMD(do_force) {
	descriptor_data *i, *next_desc;
	char_data *vict, *next_force;
	char to_force[MAX_INPUT_LENGTH + 2];

	half_chop(argument, arg, to_force);

	if (!*arg || !*to_force)
		send_to_char("Whom do you wish to force do what?\r\n", ch);
	else if ((GET_ACCESS_LEVEL(ch) < LVL_IMPL) || (str_cmp("all", arg) && str_cmp("room", arg))) {
		if (!(vict = get_char_vis(ch, arg, FIND_CHAR_WORLD)))
			send_config_msg(ch, "no_person");
		else if (!REAL_NPC(vict) && GET_REAL_LEVEL(ch) <= GET_REAL_LEVEL(vict))
			send_to_char("No, no, no!\r\n", ch);
		else {
			send_config_msg(ch, "ok_string");
			sprintf(buf1, "$n has forced you to '%s'.", to_force);
			act(buf1, TRUE, ch, NULL, vict, TO_VICT);
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s forced %s to %s", GET_NAME(ch), GET_NAME(vict), to_force);
			command_interpreter(vict, to_force);
		}
	}
	else if (!str_cmp("room", arg)) {
		send_config_msg(ch, "ok_string");
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s forced room %s to %s", GET_NAME(ch), room_log_identifier(IN_ROOM(ch)), to_force);

		for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = next_force) {
			next_force = vict->next_in_room;
			if (!REAL_NPC(vict) && GET_REAL_LEVEL(vict) >= GET_REAL_LEVEL(ch))
				continue;
			sprintf(buf1, "$n has forced you to '%s'.", to_force);
			act(buf1, TRUE, ch, NULL, vict, TO_VICT);
			command_interpreter(vict, to_force);
		}
	}
	else { /* force all */
		send_config_msg(ch, "ok_string");
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s forced all to %s", GET_NAME(ch), to_force);

		for (i = descriptor_list; i; i = next_desc) {
			next_desc = i->next;

			if (STATE(i) != CON_PLAYING || !(vict = i->character) || (!REAL_NPC(vict) && GET_REAL_LEVEL(vict) >= GET_REAL_LEVEL(ch)))
				continue;
			sprintf(buf1, "$n has forced you to '%s'.", to_force);
			act(buf1, TRUE, ch, NULL, vict, TO_VICT);
			command_interpreter(vict, to_force);
		}
	}
}


ACMD(do_fullsave) {
	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has triggered a full map save", GET_REAL_NAME(ch));
	syslog(SYS_INFO, 0, FALSE, "Updating zone files...");

	save_whole_world();
	send_config_msg(ch, "ok_string");
}


ACMD(do_gecho) {
	descriptor_data *pt;

	skip_spaces(&argument);
	delete_doubledollar(argument);

	if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_MUTED))
		msg_to_char(ch, "You can't use gecho while muted.\r\n");
	else if (!*argument)
		send_to_char("That must be a mistake...\r\n", ch);
	else {
		sprintf(buf, "%s\r\n", argument);
		for (pt = descriptor_list; pt; pt = pt->next) {
			if (STATE(pt) == CON_PLAYING && pt->character && pt->character != ch) {
				send_to_char(buf, pt->character);
			}
		}
		
		if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			send_to_char(buf, ch);
		}
	}
}


ACMD(do_goto) {
	room_data *location;

	one_word(argument, arg);
	
	if (!(location = find_target_room(ch, arg))) {
		return;
	}

	if (subcmd != SCMD_GOTO && !can_use_room(ch, location, GUESTS_ALLOWED)) {
		msg_to_char(ch, "You can't teleport there.\r\n");
		return;
	}

	perform_goto(ch, location);
}


ACMD(do_instance) {
	struct instance_data *inst;
	int count;
	
	argument = any_one_arg(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		count = 0;
		for (inst = instance_list; inst; inst = inst->next) {
			++count;
		}
		
		msg_to_char(ch, "Usage: instance <list | add | delete | deleteall | nearby | reset | test> [argument]\r\n");
		msg_to_char(ch, "There are %d live instances.\r\n", count);
	}
	else if (is_abbrev(arg, "list")) {
		do_instance_list(ch, argument);
	}
	else if (is_abbrev(arg, "add")) {
		do_instance_add(ch, argument);
	}
	else if (is_abbrev(arg, "delete")) {
		do_instance_delete(ch, argument);
	}
	else if (is_abbrev(arg, "deleteall")) {
		do_instance_delete_all(ch, argument);
	}
	else if (is_abbrev(arg, "nearby")) {
		do_instance_nearby(ch, argument);
	}
	else if (is_abbrev(arg, "reset")) {
		do_instance_reset(ch, argument);
	}
	else if (is_abbrev(arg, "test")) {
		do_instance_test(ch, argument);
	}
	else {
		msg_to_char(ch, "Invalid instance command.\r\n");
	}
}


ACMD(do_invis) {
	int level;

	if (IS_NPC(ch)) {
		send_to_char("You can't do that!\r\n", ch);
		return;
	}

	one_argument(argument, arg);
	if (!*arg) {
		if (GET_INVIS_LEV(ch) > 0)
			perform_immort_vis(ch);
		else
			perform_immort_invis(ch, GET_ACCESS_LEVEL(ch));
	}
	else {
		level = atoi(arg);
		if (level > GET_ACCESS_LEVEL(ch))
			send_to_char("You can't go invisible above your own level.\r\n", ch);
		else if (level < 1)
			perform_immort_vis(ch);
		else
			perform_immort_invis(ch, level);
	}
}


ACMD(do_last) {
	extern const char *level_names[][2];

	struct char_file_u chdata;
	char status[10];

	one_argument(argument, arg);

	if (!*arg)
		send_to_char("For whom do you wish to search?\r\n", ch);
	else if (load_char(arg, &chdata) == NOTHING)
		send_to_char("There is no such player.\r\n", ch);
	else if ((chdata.access_level > GET_ACCESS_LEVEL(ch)) && (GET_ACCESS_LEVEL(ch) < LVL_IMPL))
		send_to_char("You are not sufficiently godly for that!\r\n", ch);
	else {
		strcpy(status, level_names[(int) chdata.access_level][0]);
		// crlf built into ctime
		msg_to_char(ch, "[%5d] [%s] %-12s : %-18s : %-20s", chdata.char_specials_saved.idnum, status, chdata.name, chdata.host, ctime(&chdata.last_logon));
	}
}


ACMD(do_load) {
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	char_data *mob;
	obj_data *obj;
	any_vnum number;

	two_arguments(argument, buf, buf2);

	if (!*buf || !*buf2 || !isdigit(*buf2)) {
		send_to_char("Usage: load { obj | mob } <number>\r\n", ch);
		return;
	}
	if ((number = atoi(buf2)) < 0) {
		send_to_char("A NEGATIVE number??\r\n", ch);
		return;
	}
	if (is_abbrev(buf, "mob")) {
		if (!mob_proto(number)) {
			send_to_char("There is no monster with that number.\r\n", ch);
			return;
		}
		mob = read_mobile(number);
		setup_generic_npc(mob, NULL, NOTHING, NOTHING);
		char_to_room(mob, IN_ROOM(ch));

		act("$n makes a quaint, magical gesture with one hand.", TRUE, ch, 0, 0, TO_ROOM);
		act("$n has created $N!", FALSE, ch, 0, mob, TO_ROOM);
		act("You create $N.", FALSE, ch, 0, mob, TO_CHAR);
		load_mtrigger(mob);
	}
	else if (is_abbrev(buf, "obj")) {
		if (!obj_proto(number)) {
			send_to_char("There is no object with that number.\r\n", ch);
			return;
		}
		obj = read_object(number);
		if (CAN_WEAR(obj, ITEM_WEAR_TAKE))
			obj_to_char(obj, ch);
		else
			obj_to_room(obj, IN_ROOM(ch));
		act("$n makes a strange magical gesture.", TRUE, ch, 0, 0, TO_ROOM);
		act("$n has created $p!", FALSE, ch, obj, 0, TO_ROOM);
		act("You create $p.", FALSE, ch, obj, 0, TO_CHAR);
		load_otrigger(obj);
	}
	else {
		send_to_char("That'll have to be either 'obj' or 'mob'.\r\n", ch);
	}
}


ACMD(do_mapout) {
	void output_map_to_file(void);

	msg_to_char(ch, "Writing map output file...\r\n");
	output_map_to_file();
	msg_to_char(ch, "Done.\r\n");
}


ACMD(do_moveeinv) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	struct empire_unique_storage *unique;
	struct empire_storage_data *store, *next_store, *temp;
	int island_from, island_to, count;
	empire_data *emp;
	
	argument = any_one_word(argument, arg1);	// empire
	two_arguments(argument, arg2, arg3);	// island_from, island_to
	
	if (!*arg1 || !*arg2 || !*arg3) {
		msg_to_char(ch, "Usage: moveeinv \"<empire>\" <from island> <to island>\r\n");
	}
	else if (!(emp = get_empire_by_name(arg1))) {
		msg_to_char(ch, "Invalid empire '%s'.\r\n", arg1);
	}
	else if (!isdigit(*arg2) || (island_from = atoi(arg2)) < 0) {
		msg_to_char(ch, "Invalid from-island '%s'.\r\n", arg2);
	}
	else if (!isdigit(*arg3) || (island_to = atoi(arg3)) < 0) {
		msg_to_char(ch, "Invalid to-island '%s'.\r\n", arg2);
	}
	else {
		count = 0;
		for (store = EMPIRE_STORAGE(emp); store; store = next_store) {
			next_store = store->next;
			
			if (store->island == island_from) {
				add_to_empire_storage(emp, island_to, store->vnum, store->amount);
				store->island = island_to;
				count += store->amount;
				
				REMOVE_FROM_LIST(store, EMPIRE_STORAGE(emp), next);
				free(store);
			}
		}
		for (unique = EMPIRE_UNIQUE_STORAGE(emp); unique; unique = unique->next) {
			if (unique->island == island_from) {
				unique->island = island_to;
				count += unique->amount;
				
				// does not consolidate
			}
		}
		
		if (count != 0) {
			save_empire(emp);
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has moved %d einv items for %s from island %d to island %d", GET_REAL_NAME(ch), count, EMPIRE_NAME(emp), island_from, island_to);
			msg_to_char(ch, "Moved %d items for %s%s&0 from island %d to island %d.\r\n", count, EMPIRE_BANNER(emp), EMPIRE_NAME(emp), island_from, island_to);
		}
		else {
			msg_to_char(ch, "No items to move.\r\n");
		}
	}
}


ACMD(do_poofset) {
	char **msg;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that now.\r\n");
		return;
	}

	switch (subcmd) {
		case SCMD_POOFIN:    msg = &(POOFIN(ch));    break;
		case SCMD_POOFOUT:   msg = &(POOFOUT(ch));   break;
		default:    return;
	}

	skip_spaces(&argument);
	delete_doubledollar(argument);
	
	if (!*argument) {
		msg_to_char(ch, "Current %s: %s\r\n", subcmd == SCMD_POOFIN ? "poofin" : "poofout", *msg ? *msg : "none");
		if (*msg) {
			msg_to_char(ch, "Use '%s none' to remove it.\r\n", subcmd == SCMD_POOFIN ? "poofin" : "poofout");
		}
		return;
	}

	// char_file_u only holds MAX_POOFIN_LENGTH+1
	if (strlen(argument) > MAX_POOFIN_LENGTH) {
		msg_to_char(ch, "You can't set that to anything longer than %d characters.\r\n", MAX_POOF_LENGTH);
		return;
	}

	if (*msg) {
		free(*msg);
	}

	if (!str_cmp(argument, "none")) {
		*msg = NULL;
	}
	else {
		*msg = str_dup(argument);
	}

	send_config_msg(ch, "ok_string");
}


ACMD(do_purge) {
	char_data *vict, *next_v;
	obj_data *obj;

	one_argument(argument, buf);

	if (*buf) {			/* argument supplied. destroy single object or char */
		if ((vict = get_char_vis(ch, buf, FIND_CHAR_ROOM)) != NULL) {
			if (!REAL_NPC(vict) && (GET_REAL_LEVEL(ch) <= GET_REAL_LEVEL(vict))) {
				send_to_char("Fuuuuuuuuu!\r\n", ch);
				return;
			}
			act("$n disintegrates $N.", FALSE, ch, 0, vict, TO_NOTVICT);

			if (!REAL_NPC(vict)) {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has purged %s.", GET_NAME(ch), GET_NAME(vict));
				if (vict->desc) {
					SAVE_CHAR(vict);
					STATE(vict->desc) = CON_CLOSE;
					vict->desc->character = NULL;
					vict->desc = NULL;
				}
			}
			extract_char(vict);
		}
		else if ((obj = get_obj_in_list_vis(ch, buf, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL) {
			act("$n destroys $p.", FALSE, ch, obj, 0, TO_ROOM);
			extract_obj(obj);
		}
		else {
			send_to_char("Nothing here by that name.\r\n", ch);
			return;
		}

		send_config_msg(ch, "ok_string");
	}
	else {			/* no argument. clean out the room */
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has purged room %s", GET_REAL_NAME(ch), room_log_identifier(IN_ROOM(ch)));
		act("$n gestures... You are surrounded by scorching flames!", FALSE, ch, 0, 0, TO_ROOM);
		send_to_room("The world seems a little cleaner.\r\n", IN_ROOM(ch));

		for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = next_v) {
			next_v = vict->next_in_room;
			if (REAL_NPC(vict))
				extract_char(vict);
		}

		while (ROOM_CONTENTS(IN_ROOM(ch)))
			extract_obj(ROOM_CONTENTS(IN_ROOM(ch)));
	}
}


ACMD(do_random) {
	int tries;
	room_vnum roll;
	room_data *loc;
	
	// looks for a random non-ocean location
	for (tries = 0; tries < 100; ++tries) {
		roll = number(0, MAP_SIZE - 1);
		loc = real_real_room(roll);	// use real_real_room to skip BASIC_OCEAN
		
		if (loc && !ROOM_IS_CLOSED(loc) && !ROOM_SECT_FLAGGED(loc, SECTF_OCEAN)) {
			perform_goto(ch, loc);
			return;
		}
	}
	
	msg_to_char(ch, "Failed to find a good random spot to send you.\r\n");
}


ACMD(do_reboot) {
	void perform_reboot();
	void update_reboot();
	extern struct reboot_control_data reboot_control;
	extern const char *shutdown_types[];
	extern const char *reboot_type[];
	
	char arg[MAX_INPUT_LENGTH];
	descriptor_data *desc;
	int type, time = 0;
	bool no_arg = FALSE;

	// any number of args in any order
	argument = any_one_arg(argument, arg);
	no_arg = !*arg;
	while (*arg) {
		if (is_number(arg)) {
			if ((time = atoi(arg)) == 0) {
				msg_to_char(ch, "Invalid amount of time!\r\n");
				return;
			}
		}
		else if (!str_cmp(arg, "now")) {
			reboot_control.immediate = TRUE;
		}
		else if ((type = search_block(arg, shutdown_types, FALSE)) != NOTHING) {
			reboot_control.level = type;
		}
		else {
			msg_to_char(ch, "Unknown reboot option '%s'.\r\n", arg);
			return;
		}
		
		argument = any_one_arg(argument, arg);
	}
	
	// All good to set up the reboot:
	if (!no_arg) {
		reboot_control.time = time + 1;	// minutes
		reboot_control.type = subcmd;
	}

	if (reboot_control.immediate) {
		msg_to_char(ch, "Rebooting momentarily...\r\n");
	}
	else {
		msg_to_char(ch, "The %s is set for %d minutes (%s).\r\n", reboot_type[reboot_control.type], no_arg ? reboot_control.time : time, shutdown_types[reboot_control.level]);
		if (no_arg) {
			msg_to_char(ch, "Players blocking the %s:\r\n", reboot_type[reboot_control.type]);
			for (desc = descriptor_list; desc; desc = desc->next) {
				if (STATE(desc) != CON_PLAYING || !desc->character) {
					msg_to_char(ch, " %s at menu\r\n", (desc->character ? GET_NAME(desc->character) : "New connection"));
				}
				else if (!REBOOT_CONF(desc->character)) {
					msg_to_char(ch, " %s not confirmed\r\n", GET_NAME(desc->character));
				}
			}
		}
		else {
			update_reboot();
		}
	}
}


/*
 * Too bad it doesn't check the return values to let the user
 * know about -1 values.  This will result in an 'Okay.' to a
 * 'reload' command even when the string was not replaced.
 * To fix later, if desired. -gg 6/24/99
 *
 * Could also replace the control here with a structure... and it
 * would be nice to have a syslog. -paul 12/8/2014
 */
ACMD(do_reload) {
	extern int file_to_string_alloc(const char *name, char **buf);
	void index_boot_help();
	void load_intro_screens();
	extern char *credits;
	extern char *motd;
	extern char *imotd;
	extern char *CREDIT_MESSG;
	extern char *help;
	extern char *info;
	extern char *handbook;
	extern char *policies;
	extern char *wizlist;
	extern char *godlist;
	extern struct help_index_element *help_table;
	extern int top_of_helpt;
	
	int i;

	one_argument(argument, arg);

	if (!str_cmp(arg, "all") || *arg == '*') {
		load_intro_screens();
		
		file_to_string_alloc(SCREDITS_FILE, &CREDIT_MESSG);
		file_to_string_alloc(WIZLIST_FILE, &wizlist);
		file_to_string_alloc(GODLIST_FILE, &godlist);
		file_to_string_alloc(CREDITS_FILE, &credits);
		file_to_string_alloc(MOTD_FILE, &motd);
		file_to_string_alloc(IMOTD_FILE, &imotd);
		file_to_string_alloc(HELP_PAGE_FILE, &help);
		file_to_string_alloc(INFO_FILE, &info);
		file_to_string_alloc(POLICIES_FILE, &policies);
		file_to_string_alloc(HANDBOOK_FILE, &handbook);
	}
	else if (!str_cmp(arg, "wizlist"))
		file_to_string_alloc(WIZLIST_FILE, &wizlist);
	else if (!str_cmp(arg, "godlist"))
		file_to_string_alloc(GODLIST_FILE, &godlist);
	else if (!str_cmp(arg, "credits"))
		file_to_string_alloc(CREDITS_FILE, &credits);
	else if (!str_cmp(arg, "motd"))
		file_to_string_alloc(MOTD_FILE, &motd);
	else if (!str_cmp(arg, "imotd"))
		file_to_string_alloc(IMOTD_FILE, &imotd);
	else if (!str_cmp(arg, "help"))
		file_to_string_alloc(HELP_PAGE_FILE, &help);
	else if (!str_cmp(arg, "info"))
		file_to_string_alloc(INFO_FILE, &info);
	else if (!str_cmp(arg, "policy"))
		file_to_string_alloc(POLICIES_FILE, &policies);
	else if (!str_cmp(arg, "handbook"))
		file_to_string_alloc(HANDBOOK_FILE, &handbook);
	else if (!str_cmp(arg, "shortcredits"))
		file_to_string_alloc(SCREDITS_FILE, &CREDIT_MESSG);
	else if (!str_cmp(arg, "intros")) {
		load_intro_screens();
	}
	else if (!str_cmp(arg, "xhelp")) {
		if (help_table) {
			for (i = 0; i <= top_of_helpt; i++) {
				if (help_table[i].keyword)
					free(help_table[i].keyword);
				if (help_table[i].entry && !help_table[i].duplicate)
					free(help_table[i].entry);
			}
			free(help_table);
		}
		top_of_helpt = 0;
		index_boot_help();
	}
	else {
		send_to_char("Unknown reload option.\r\n", ch);
		return;
	}

	send_config_msg(ch, "ok_string");
}


ACMD(do_rescale) {
	void scale_item_to_level(obj_data *obj, int level);

	obj_data *obj, *new, *proto = NULL;
	char_data *vict;
	int level;
	
	bitvector_t preserve_flags = OBJ_SUPERIOR;	// flags to copy over if obj is reloaded

	argument = one_argument(argument, arg);
	skip_spaces(&argument);
	level = atoi(argument);
	
	if (!*argument || !*arg || !isdigit(*argument)) {
		msg_to_char(ch, "Usage: rescale <item> <level>\r\n");
	}
	else if (level < 0) {
		msg_to_char(ch, "Invalid level.\r\n");
	}
	else if ((vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		// victim mode
		if (!IS_NPC(vict)) {
			msg_to_char(ch, "You can only rescale NPCs.\r\n");
		}
		else {
			scale_mob_to_level(vict, level);
			
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has rescaled mob %s to level %d at %s", GET_NAME(ch), PERS(vict, vict, FALSE), GET_CURRENT_SCALE_LEVEL(vict), room_log_identifier(IN_ROOM(vict)));
			
			sprintf(buf, "You rescale $N to level %d.", GET_CURRENT_SCALE_LEVEL(vict));
			act("$n rescales $N.", FALSE, ch, NULL, vict, TO_NOTVICT);
			act(buf, FALSE, ch, NULL, vict, TO_CHAR);
			if (vict->desc) {
				sprintf(buf, "$n rescales you to level %d.", GET_CURRENT_SCALE_LEVEL(vict));
				act(buf, FALSE, ch, NULL, vict, TO_VICT);
			}
		}
	}
	else if ((obj = get_obj_in_list_vis(ch, arg, ch->carrying)) || (obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		// item mode
		if (!OBJ_FLAGGED(obj, OBJ_SCALABLE) && !(proto = obj_proto(GET_OBJ_VNUM(obj)))) {
			msg_to_char(ch, "That object cannot be rescaled.\r\n");
		}
		else if (proto && !OBJ_FLAGGED(proto, OBJ_SCALABLE)) {
			act("$p is not scalable.", FALSE, ch, obj, NULL, TO_CHAR);
		}
		else {
			if (!OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
				new = read_object(GET_OBJ_VNUM(obj));
				GET_OBJ_EXTRA(new) |= GET_OBJ_EXTRA(obj) & preserve_flags;
				
				// swap binds
				OBJ_BOUND_TO(new) = OBJ_BOUND_TO(obj);
				OBJ_BOUND_TO(obj) = NULL;
				
				swap_obj_for_obj(obj, new);
				extract_obj(obj);
				obj = new;
			}
			
			scale_item_to_level(obj, level);
			
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has rescaled obj %s to level %d at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), GET_OBJ_CURRENT_SCALE_LEVEL(obj), room_log_identifier(IN_ROOM(ch)));
			sprintf(buf, "You rescale $p to level %d.", GET_OBJ_CURRENT_SCALE_LEVEL(obj));
			act(buf, FALSE, ch, obj, NULL, TO_CHAR);
			act("$n rescales $p.", FALSE, ch, obj, NULL, TO_ROOM);
		}
	}
	else {
		msg_to_char(ch, "You don't see %s %s here.\r\n", AN(arg), arg);
	}
}


ACMD(do_restore) {
	struct cooldown_data *cool;
	empire_data *emp;
	char_data *vict;
	int i, iter;

	one_argument(argument, buf);
	if (!*buf)
		send_to_char("Whom do you wish to restore?\r\n", ch);
	else if (!(vict = get_char_vis(ch, buf, FIND_CHAR_WORLD)))
		send_config_msg(ch, "no_person");
	else {
		GET_HEALTH(vict) = GET_MAX_HEALTH(vict);
		GET_MOVE(vict) = GET_MAX_MOVE(vict);
		GET_MANA(vict) = GET_MAX_MANA(vict);
		GET_BLOOD(vict) = GET_MAX_BLOOD(vict);

		if (GET_POS(vict) < POS_SLEEPING) {
			GET_POS(vict) = POS_STANDING;
		}
		
		// remove all cooldowns
		while ((cool = vict->cooldowns)) {
			vict->cooldowns = cool->next;
			free(cool);
		}

		if (!IS_NPC(vict) && (GET_ACCESS_LEVEL(ch) >= LVL_GOD) && (GET_ACCESS_LEVEL(vict) >= LVL_GOD)) {
			for (i = 0; i < NUM_CONDS; i++)
				GET_COND(vict, i) = UNLIMITED;

			for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
				vict->real_attributes[iter] = att_max(vict);
			}
			
			for (i = 0; i < NUM_SKILLS; ++i) {
				set_skill(vict, i, 100);
			}
			update_class(vict);
			
			// temporarily remove empire abilities
			emp = GET_LOYALTY(vict);
			if (emp) {
				adjust_abilities_to_empire(vict, emp, FALSE);
			}
			
			for (i = 0; i < NUM_ABILITIES; ++i) {
				vict->player_specials->saved.abilities[i].purchased = TRUE;
				vict->player_specials->saved.abilities[i].levels_gained = 0;
			}

			affect_total(vict);
			
			// re-add abilities
			if (emp) {
				adjust_abilities_to_empire(vict, emp, TRUE);
			}
		}
		update_pos(vict);
		if (ch != vict) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has restored %s", GET_REAL_NAME(ch), GET_REAL_NAME(vict));
		}
		send_config_msg(ch, "ok_string");
		act("You have been fully healed by $N!", FALSE, vict, 0, ch, TO_CHAR);
	}
}


ACMD(do_return) {
	if (ch->desc && ch->desc->original) {
		syslog(SYS_GC, GET_INVIS_LEV(ch->desc->original), TRUE, "GC: %s has returned to %s original body", GET_REAL_NAME(ch->desc->original), HSHR(ch->desc->original));
		send_to_char("You return to your original body.\r\n", ch);

		/*
		 * If someone switched into your original body, disconnect them.
		 *   - JE 2/22/95
		 *
		 * Zmey: here we put someone switched in our body to disconnect state
		 * but we must also NULL his pointer to our character, otherwise
		 * close_socket() will damage our character's pointer to our descriptor
		 * (which is assigned below in this function). 12/17/99
		 */

		if (ch->desc->original->desc) {
			ch->desc->original->desc->character = NULL;
			STATE(ch->desc->original->desc) = CON_DISCONNECT;
		}

		/* Now our descriptor points to our original body. */
		ch->desc->character = ch->desc->original;
		ch->desc->original = NULL;

		/* And our body's pointer to descriptor now points to our descriptor. */
		ch->desc->character->desc = ch->desc;
		ch->desc = NULL;
	}
}


ACMD(do_send) {
	char_data *vict;

	half_chop(argument, arg, buf);

	if (!*arg) {
		send_to_char("Send what to who?\r\n", ch);
		return;
	}
	if (!(vict = get_char_vis(ch, arg, FIND_CHAR_WORLD))) {
		send_config_msg(ch, "no_person");
		return;
	}
	send_to_char(buf, vict);
	send_to_char("\r\n", vict);
	if (PRF_FLAGGED(ch, PRF_NOREPEAT))
		send_to_char("Sent.\r\n", ch);
	else {
		sprintf(buf2, "You send '%s' to %s.\r\n", buf, GET_NAME(vict));
		send_to_char(buf2, ch);
	}
}


ACMD(do_set) {
	char_data *vict = NULL, *cbuf = NULL;
	struct char_file_u tmp_store;
	char field[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	int mode, len, player_i = 0, retval;
	char is_file = 0, is_player = 0;

	half_chop(argument, name, buf);

	if (!str_cmp(name, "file")) {
		is_file = 1;
		half_chop(buf, name, buf);
	}
	else if (!str_cmp(name, "player")) {
		is_player = 1;
		half_chop(buf, name, buf);
	}
	else if (!str_cmp(name, "mob"))
		half_chop(buf, name, buf);

	half_chop(buf, field, buf);
	strcpy(val_arg, buf);

	if (!*name || !*field) {
		send_to_char("Usage: set <victim> <field> <value>\r\n", ch);
		return;
	}

	/* find the target */
	if (!is_file) {
		if (is_player) {
			if (!(vict = get_player_vis(ch, name, FIND_CHAR_WORLD))) {
				send_to_char("There is no such player.\r\n", ch);
				return;
			}
		}
		else { /* is_mob */
			if (!(vict = get_char_vis(ch, name, FIND_CHAR_WORLD))) {
				send_to_char("There is no such creature.\r\n", ch);
				return;
			}
		}
		if (!IS_NPC(vict) && ch != vict && GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch)) {
			send_to_char("Sorry, you can't do that.\r\n", ch);
			return;
		}
	}
	else if (is_file) {
		/* try to load the player off disk */
		CREATE(cbuf, char_data, 1);
		clear_char(cbuf);
		if ((player_i = load_char(name, &tmp_store)) > NOBODY) {
			store_to_char(&tmp_store, cbuf);
			if (GET_ACCESS_LEVEL(cbuf) >= GET_ACCESS_LEVEL(ch)) {
				free_char(cbuf);
				send_to_char("Sorry, you can't do that.\r\n", ch);
				return;
			}
			vict = cbuf;
			SET_BIT(PLR_FLAGS(vict), PLR_KEEP_LAST_LOGIN_INFO);
		}
		else {
			free(cbuf);
			send_to_char("There is no such player.\r\n", ch);
			return;
		}
	}

	/* find the command in the list */
	len = strlen(field);
	for (mode = 0; *(set_fields[mode].cmd) != '\n'; mode++)
		if (!strn_cmp(field, set_fields[mode].cmd, len))
			break;

	/* perform the set */
	retval = perform_set(ch, vict, mode, val_arg);

	/* save the character if a change was made */
	if (retval) {
		if (!is_file && !IS_NPC(vict))
			SAVE_CHAR(vict);
		if (is_file) {
			store_loaded_char(vict);
			send_to_char("Saved in file.\r\n", ch);
		}
	}
	else if (is_file) {
		/* free the memory if we allocated it earlier */
		free_char(cbuf);
	}
}


ACMD(do_show) {
	int count, pos, iter;
	char field[MAX_INPUT_LENGTH], value[MAX_INPUT_LENGTH];

	struct show_struct {
		const char *cmd;
		const char level;
		SHOW(*func);
	} fields[] = {
		{ "nothing", 0, NULL },	// this is skipped
		
		{ "player", LVL_START_IMM, show_player },
		{ "rent", LVL_START_IMM, show_rent },
		{ "stats", LVL_GOD, show_stats },
		{ "site", LVL_ASST, show_site },
		{ "commons", LVL_ASST, show_commons },
		{ "players", LVL_START_IMM, show_players },
		{ "terrain", LVL_START_IMM, show_terrain },
		{ "account", LVL_CIMPL, show_account },
		{ "notes", LVL_START_IMM, show_notes },
		{ "arrowtypes", LVL_START_IMM, show_arrowtypes },
		{ "skills", LVL_START_IMM, show_skills },
		{ "storage", LVL_START_IMM, show_storage },
		{ "startlocs", LVL_START_IMM, show_startlocs },
		{ "spawns", LVL_START_IMM, show_spawns },
		{ "ignoring", LVL_START_IMM, show_ignoring },
		{ "workforce", LVL_START_IMM, show_workforce },
		{ "islands", LVL_START_IMM, show_islands },

		// last
		{ "\n", 0, NULL }
	};
	
	// don't bother -- nobody to show it to
	if (!ch->desc) {
		return;
	}

	skip_spaces(&argument);

	if (!*argument) {
		strcpy(buf, "Show options:\r\n");
		for (count = 0, iter = 1; fields[iter].level; ++iter) {
			if (fields[iter].level <= GET_ACCESS_LEVEL(ch)) {
				sprintf(buf + strlen(buf), "%-15s%s", fields[iter].cmd, (!(++count % 5) ? "\r\n" : ""));
			}
		}
		strcat(buf, "\r\n");
		send_to_char(buf, ch);
		return;
	}

	strcpy(arg, two_arguments(argument, field, value));

	pos = NOTHING;
	for (iter = 0; *(fields[iter].cmd) != '\n'; ++iter) {
		if (!strn_cmp(field, fields[iter].cmd, strlen(field))) {
			pos = iter;
			break;
		}
	}
	
	if (pos == NOTHING) {
		msg_to_char(ch, "Invalid option.\r\n");
	}
	else if (GET_ACCESS_LEVEL(ch) < fields[pos].level) {
		send_to_char("You are not godly enough for that!\r\n", ch);
	}
	else if (!fields[pos].func) {
		msg_to_char(ch, "That function is not implemented yet.\r\n");
	}
	else {
		// SUCCESS -- pass to next function
		(fields[pos].func)(ch, value);
	}
}


ACMD(do_slay) {
	extern obj_data *die(char_data *ch, char_data *killer);
	
	char_data *vict;

	one_argument(argument, arg);

	if (!*arg)
		send_to_char("Slay whom?\r\n", ch);
	else {
		if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
			send_to_char("They aren't here.\r\n", ch);
		else if (ch == vict)
			send_to_char("Your mother would be so sad.. :(\r\n", ch);
		else if (!IS_NPC(vict) && GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch)) {
			act("Surely you don't expect $N to let you slay $M, do you?", FALSE, ch, NULL, vict, TO_CHAR);
		}
		else {
			if (!IS_NPC(vict) && !affected_by_spell(ch, ATYPE_PHOENIX_RITE)) {
				syslog(SYS_GC | SYS_DEATH, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has slain %s at %s", GET_REAL_NAME(ch), GET_REAL_NAME(vict), room_log_identifier(IN_ROOM(vict)));
				mortlog("%s has been slain at (%d, %d)", PERS(vict, vict, TRUE), X_COORD(IN_ROOM(vict)), Y_COORD(IN_ROOM(vict)));
			}
			
			act("You chop $M to pieces! Ah! The blood!", FALSE, ch, 0, vict, TO_CHAR);
			act("$N chops you to pieces!", FALSE, vict, 0, ch, TO_CHAR);
			act("$n brutally slays $N!", FALSE, ch, 0, vict, TO_NOTVICT);
			tag_mob(vict, ch);	// ensures loot binding if applicable
			die(vict, ch);
		}
	}
}


ACMD(do_snoop) {
	char_data *victim, *tch;

	if (!ch->desc)
		return;

	one_argument(argument, arg);

	if (!*arg)
		stop_snooping(ch);
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_WORLD)))
		send_to_char("No such person around.\r\n", ch);
	else if (!victim->desc)
		send_to_char("There's no link.. nothing to snoop.\r\n", ch);
	else if (victim == ch)
		stop_snooping(ch);
	else if (victim->desc->snoop_by)
		send_to_char("Busy already. \r\n", ch);
	else if (victim->desc->snooping == ch->desc)
		send_to_char("Don't be stupid.\r\n", ch);
	else {
		if (victim->desc->original)
			tch = victim->desc->original;
		else
			tch = victim;

		if (GET_ACCESS_LEVEL(tch) >= GET_ACCESS_LEVEL(ch)) {
			send_to_char("You can't.\r\n", ch);
			return;
		}
		
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "GC: %s is now snooping %s", GET_REAL_NAME(ch), GET_REAL_NAME(victim));
		send_config_msg(ch, "ok_string");

		if (ch->desc->snooping)
			ch->desc->snooping->snoop_by = NULL;

		ch->desc->snooping = victim->desc;
		victim->desc->snoop_by = ch->desc;
	}
}


ACMD(do_stat) {
	void read_saved_vars(char_data *ch);
	
	char_data *victim;
	crop_data *cp;
	obj_data *obj;
	struct char_file_u tmp_store;
	int tmp;

	half_chop(argument, buf1, buf2);

	if (!*buf1) {
		send_to_char("Stats on who or what?\r\n", ch);
		return;
	}
	else if (is_abbrev(buf1, "room")) {
		do_stat_room(ch);
	}
	else if (!strn_cmp(buf1, "adventure", 3) && is_abbrev(buf1, "adventure")) {
		if (COMPLEX_DATA(IN_ROOM(ch)) && COMPLEX_DATA(IN_ROOM(ch))->instance) {
			do_stat_adventure(ch, COMPLEX_DATA(IN_ROOM(ch))->instance->adventure);
		}
		else {
			msg_to_char(ch, "You are not in an adventure zone.\r\n");
		}
	}
	else if (!strn_cmp(buf1, "template", 4) && is_abbrev(buf1, "template")) {
		if (GET_ROOM_TEMPLATE(IN_ROOM(ch))) {
			do_stat_room_template(ch, GET_ROOM_TEMPLATE(IN_ROOM(ch)));
		}
		else {
			msg_to_char(ch, "This is not a templated room.\r\n");
		}
	}
	else if (!str_cmp(buf1, "building")) {
		if (GET_BUILDING(IN_ROOM(ch))) {
			do_stat_building(ch, GET_BUILDING(IN_ROOM(ch)));
		}
		else {
			msg_to_char(ch, "You are not in a building.\r\n");
		}
	}
	else if (!str_cmp(buf1, "crop")) {
		if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_HAS_CROP_DATA) && (cp = crop_proto(ROOM_CROP_TYPE(IN_ROOM(ch))))) {
			do_stat_crop(ch, cp);
		}
		else {
			msg_to_char(ch, "You are not on a crop tile.\r\n");
		}
	}
	else if (!strn_cmp(buf1, "sect", 4) && is_abbrev(buf1, "sector")) {
		do_stat_sector(ch, SECT(IN_ROOM(ch)));
	}
	else if (is_abbrev(buf1, "mob")) {
		if (!*buf2)
			send_to_char("Stats on which mobile?\r\n", ch);
		else {
			if ((victim = get_char_vis(ch, buf2, FIND_CHAR_WORLD)) != NULL)
				do_stat_character(ch, victim);
			else
				send_to_char("No such mobile around.\r\n", ch);
		}
	}
	else if (is_abbrev(buf1, "player")) {
		if (!*buf2) {
			send_to_char("Stats on which player?\r\n", ch);
		}
		else {
			if ((victim = get_player_vis(ch, buf2, FIND_CHAR_WORLD)) != NULL)
				do_stat_character(ch, victim);
			else
				send_to_char("No such player around.\r\n", ch);
		}
	}
	else if (!str_cmp(buf1, "file")) {
		if (!*buf2) {
			send_to_char("Stats on which player?\r\n", ch);
		}
		else {
			CREATE(victim, char_data, 1);
			clear_char(victim);
			if (load_char(buf2, &tmp_store) > NOBODY) {
				store_to_char(&tmp_store, victim);
				SET_BIT(PLR_FLAGS(victim), PLR_KEEP_LAST_LOGIN_INFO);
				// put somewhere extractable
				char_to_room(victim, world_table);
				if (GET_ACCESS_LEVEL(victim) > GET_ACCESS_LEVEL(ch)) {
					send_to_char("Sorry, you can't do that.\r\n", ch);
				}
				else {
					read_saved_vars(victim);
					do_stat_character(ch, victim);
				}
				extract_char_final(victim);
			}
			else {
				send_to_char("There is no such player.\r\n", ch);
				free(victim);
			}
		}
	}
	else if (is_abbrev(buf1, "object")) {
		if (!*buf2)
			send_to_char("Stats on which object?\r\n", ch);
		else {
			if ((obj = get_obj_vis(ch, buf2)) != NULL)
				do_stat_object(ch, obj);
			else
				send_to_char("No such object around.\r\n", ch);
		}
	}
	else {
		if ((obj = get_object_in_equip_vis(ch, buf1, ch->equipment, &tmp)) != NULL)
			do_stat_object(ch, obj);
		else if ((obj = get_obj_in_list_vis(ch, buf1, ch->carrying)) != NULL)
			do_stat_object(ch, obj);
		else if ((victim = get_char_vis(ch, buf1, FIND_CHAR_ROOM)) != NULL)
			do_stat_character(ch, victim);
		else if ((obj = get_obj_in_list_vis(ch, buf1, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL)
			do_stat_object(ch, obj);
		else if ((victim = get_char_vis(ch, buf1, FIND_CHAR_WORLD)) != NULL)
			do_stat_character(ch, victim);
		else if ((obj = get_obj_vis(ch, buf1)) != NULL)
			do_stat_object(ch, obj);
		else
			send_to_char("Nothing around by that name.\r\n", ch);
	}
}


ACMD(do_switch) {
	char_data *victim;

	one_argument(argument, arg);

	if (ch->desc->original)
		send_to_char("You're already switched.\r\n", ch);
	else if (!*arg)
		send_to_char("Switch with who?\r\n", ch);
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_WORLD)))
		send_to_char("No such character.\r\n", ch);
	else if (ch == victim)
		send_to_char("Hee hee... we are jolly funny today, eh?\r\n", ch);
	else if (victim->desc)
		send_to_char("You can't do that, the body is already in use!\r\n", ch);
	else if ((GET_ACCESS_LEVEL(ch) < LVL_IMPL) && !IS_NPC(victim))
		send_to_char("You aren't holy enough to use a mortal's body.\r\n", ch);
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has switched into %s at %s", GET_REAL_NAME(ch), GET_REAL_NAME(victim), room_log_identifier(IN_ROOM(victim)));
		send_config_msg(ch, "ok_string");

		ch->desc->character = victim;
		ch->desc->original = ch;

		victim->desc = ch->desc;
		ch->desc = NULL;
	}
}


ACMD(do_syslog) {
	int iter;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't change your syslog settings right now.\r\n");
	}
	else if (!str_cmp(argument, "all")) {
		// turn on all syslogs
		for (iter = 0; *syslog_types[iter] != '\n'; ++iter) {
			SYSLOG_FLAGS(ch) |= BIT(iter);
		}
		msg_to_char(ch, "Your syslogs are now all on.\r\n");
	}
	else {
		SYSLOG_FLAGS(ch) = olc_process_flag(ch, argument, "syslog", "syslog", syslog_types, SYSLOG_FLAGS(ch));
	}
}


ACMD(do_tedit) {
	extern struct tedit_struct tedit_option[];
	
	int l, i;
	char field[MAX_INPUT_LENGTH];


	if (ch->desc == NULL) {
		send_to_char("Get outta here you linkdead head!\r\n", ch);
		return;
	}
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing something else. Type ,/h for help.\r\n");
		return;
	}

	half_chop(argument, field, buf);

	if (!*field) {
		strcpy(buf, "Files available to be edited:\r\n");
		i = 1;
		for (l = 0; *tedit_option[l].cmd != '\n'; l++) {
			if (GET_ACCESS_LEVEL(ch) >= tedit_option[l].level) {
				sprintf(buf, "%s%-11.11s", buf, tedit_option[l].cmd);
				if (!(i % 7))
					strcat(buf, "\r\n");
				i++;
			}
		}

		if (--i % 7)
			strcat(buf, "\r\n");

		if (i == 0)
			strcat(buf, "None.\r\n");

		send_to_char(buf, ch);
		return;
	}
	for (l = 0; *(tedit_option[l].cmd) != '\n'; l++)
		if (!strn_cmp(field, tedit_option[l].cmd, strlen(field)))
			break;

	if (*tedit_option[l].cmd == '\n') {
		send_to_char("Invalid text editor option.\r\n", ch);
		return;
	}

	if (GET_ACCESS_LEVEL(ch) < tedit_option[l].level) {
		send_to_char("You are not godly enough for that!\r\n", ch);
		return;
	}

	/* set up editor stats */
	start_string_editor(ch->desc, "file", tedit_option[l].buffer, tedit_option[l].size);
	ch->desc->file_storage = str_dup(tedit_option[l].filename);
	act("$n begins editing a file.", TRUE, ch, 0, 0, TO_ROOM);
}


// do_transfer <- search alias because why is it called do_trans? -pc
ACMD(do_trans) {
	descriptor_data *i;
	char_data *victim;
	room_data *to_room = IN_ROOM(ch);

	argument = one_argument(argument, buf);
	one_word(argument, arg);

	if (*arg) {
		if (!(to_room = find_target_room(ch, arg))) {
			// sent own error message
			return;
		}
	}

	if (!*buf) {
		send_to_char("Whom do you wish to transfer (and where)?\r\n", ch);
	}
	else if (str_cmp("all", buf) != 0) {
		if (!(victim = get_char_vis(ch, buf, FIND_CHAR_WORLD)))
			send_config_msg(ch, "no_person");
		else if (victim == ch)
			send_to_char("That doesn't make much sense, does it?\r\n", ch);
		else {
			if ((GET_REAL_LEVEL(ch) < GET_REAL_LEVEL(victim)) && !REAL_NPC(victim)) {
				send_to_char("Go transfer someone your own size.\r\n", ch);
				return;
			}
			
			if (!IS_NPC(victim)) {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has transferred %s to %s", GET_REAL_NAME(ch), GET_REAL_NAME(victim), room_log_identifier(to_room));
			}

			act("$n disappears in a mushroom cloud.", FALSE, victim, 0, 0, TO_ROOM);
			char_from_room(victim);
			char_to_room(victim, to_room);
			if (!IS_NPC(victim)) {
				GET_LAST_DIR(victim) = NO_DIR;
			}
			act("$n arrives from a puff of smoke.", FALSE, victim, 0, 0, TO_ROOM);
			act("$n has transferred you!", FALSE, ch, 0, victim, TO_VICT);
			look_at_room(victim);
			enter_wtrigger(IN_ROOM(victim), victim, NO_DIR);
			send_config_msg(ch, "ok_string");
		}
	}
	else {			/* Trans All */
		if (GET_ACCESS_LEVEL(ch) < LVL_IMPL) {
			send_to_char("I think not.\r\n", ch);
			return;
		}

		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has transferred all players to %s", GET_REAL_NAME(ch), room_log_identifier(to_room));

		for (i = descriptor_list; i; i = i->next) {
			if (STATE(i) == CON_PLAYING && i->character && i->character != ch) {
				victim = i->character;
				if (GET_ACCESS_LEVEL(victim) >= GET_ACCESS_LEVEL(ch))
					continue;
				act("$n disappears in a mushroom cloud.", FALSE, victim, 0, 0, TO_ROOM);
				char_from_room(victim);
				char_to_room(victim, to_room);
				if (!IS_NPC(victim)) {
					GET_LAST_DIR(victim) = NO_DIR;
				}
				act("$n arrives from a puff of smoke.", FALSE, victim, 0, 0, TO_ROOM);
				act("$n has transferred you!", FALSE, ch, 0, victim, TO_VICT);
				look_at_room(victim);
				enter_wtrigger(IN_ROOM(victim), victim, NO_DIR);
			}
		}
		
		send_config_msg(ch, "ok_string");
	}
}


ACMD(do_unbind) {
	void free_obj_binding(struct obj_binding **list);
	
	obj_data *obj;
	
	one_argument(argument, arg);

	if (!*arg) {
		msg_to_char(ch, "Unbind which object?\r\n");
	}
	else if (!(obj = get_obj_vis(ch, arg))) {
		msg_to_char(ch, "Unable to find '%s'.\r\n", argument);
	}
	else if (!OBJ_BOUND_TO(obj)) {
		act("$p isn't bound to anybody.", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else {
		free_obj_binding(&OBJ_BOUND_TO(obj));
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s used unbind on %s", GET_REAL_NAME(ch), GET_OBJ_SHORT_DESC(obj));
		act("You unbind $p.", FALSE, ch, obj, NULL, TO_CHAR);
	}
}


ACMD(do_users) {
	char mode;
	char name_search[MAX_INPUT_LENGTH], host_search[MAX_INPUT_LENGTH];
	char_data *tch;
	descriptor_data *d;
	int low = 0, high = LVL_IMPL, num_can_see = 0;
	int rp = 0, playing = 0, deadweight = 0;
	bool result;

	host_search[0] = name_search[0] = '\0';

	strcpy(buf, argument);
	while (*buf) {
		half_chop(buf, arg, buf1);
		if (*arg == '-') {
			mode = *(arg + 1);  /* just in case; we destroy arg in the switch */
			switch (mode) {
				case 'r':
					rp = 1;
					playing = 1;
					strcpy(buf, buf1);
					break;
				case 'p':
					playing = 1;
					strcpy(buf, buf1);
					break;
				case 'd':
					deadweight = 1;
					strcpy(buf, buf1);
					break;
				case 'l':
					playing = 1;
					half_chop(buf1, arg, buf);
					sscanf(arg, "%d-%d", &low, &high);
					break;
				case 'n':
					playing = 1;
					half_chop(buf1, name_search, buf);
					break;
				case 'h':
					playing = 1;
					half_chop(buf1, host_search, buf);
					break;
				default:
					send_to_char("format: users [-l minlevel[-maxlevel]] [-n name] [-h host] [-r] [-p]\r\n", ch);
					return;
			}				/* end of switch */

		}
		else {			/* endif */
			send_to_char("format: users [-l minlevel[-maxlevel]] [-n name] [-h host] [-r] [-p]\r\n", ch);
			return;
		}
	}				/* end while (parser) */
	msg_to_char(ch, "Num L Name          State           Idl Login@   Site\r\n");
	msg_to_char(ch, "--- - ------------- --------------- --- -------- ------------------------\r\n");

	one_argument(argument, arg);

	if (!*host_search) {
		for (tch = character_list; tch; tch = tch->next) {
			if (IS_NPC(tch) || tch->desc)
				continue;
			result = users_output(ch, tch, NULL, name_search, low, high, rp);
			
			if (result) {
				num_can_see++;
			}
		}
	}

	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) != CON_PLAYING && playing)
			continue;
		if (STATE(d) == CON_PLAYING && deadweight)
			continue;
		if (*host_search && !strstr(d->host, host_search))
			continue;
		
		result =users_output(ch, NULL, d, name_search, low, high, rp);
		
		if (result) {
			++num_can_see;
		}
	}

	msg_to_char(ch, "\r\n%d visible sockets connected.\r\n", num_can_see);
}


ACMD(do_vnum) {
	half_chop(argument, buf, buf2);

	if (!*buf || !*buf2) {
		send_to_char("Usage: vnum <type> <name>\r\n", ch);
	}
	else if (is_abbrev(buf, "mob")) {
		if (!vnum_mobile(buf2, ch)) {
			send_to_char("No mobiles by that name.\r\n", ch);
		}
	}
	else if (is_abbrev(buf, "obj")) {
		if (!vnum_object(buf2, ch)) {
			send_to_char("No objects by that name.\r\n", ch);
		}
	}
	else if (is_abbrev(buf, "craft")) {
		if (!vnum_craft(buf2, ch)) {
			msg_to_char(ch, "No crafts by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "adventure")) {
		if (!vnum_adventure(buf2, ch)) {
			msg_to_char(ch, "No adventures by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "building")) {
		if (!vnum_building(buf2, ch)) {
			msg_to_char(ch, "No buildings by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "crop")) {
		if (!vnum_crop(buf2, ch)) {
			msg_to_char(ch, "No crops by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "roomtemplate")) {
		if (!vnum_room_template(buf2, ch)) {
			msg_to_char(ch, "No room templates by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "sector")) {
		if (!vnum_sector(buf2, ch)) {
			msg_to_char(ch, "No sectors by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "trigger")) {
		if (!vnum_trigger(buf2, ch)) {
			msg_to_char(ch, "No triggers by that name.\r\n");
		}
	}
	else {
		send_to_char("Usage: vnum <type> <name>\r\n", ch);
	}
}


ACMD(do_vstat) {
	char_data *mob;
	obj_data *obj;
	any_vnum number;

	two_arguments(argument, buf, buf2);

	if (!*buf || !*buf2 || !isdigit(*buf2)) {
		send_to_char("Usage: vstat <type> <vnum>\r\n", ch);
		return;
	}
	if ((number = atoi(buf2)) < 0) {
		send_to_char("A NEGATIVE number??\r\n", ch);
		return;
	}
	
	if (is_abbrev(buf, "adventure")) {
		adv_data *adv = adventure_proto(number);
		if (!adv) {
			msg_to_char(ch, "There is no adventure zone with that number.\r\n");
			return;
		}
		do_stat_adventure(ch, adv);
	}
	else if (is_abbrev(buf, "building")) {
		bld_data *bld = building_proto(number);
		if (!bld) {
			msg_to_char(ch, "There is no building with that number.\r\n");
			return;
		}
		do_stat_building(ch, bld);
	}
	else if (is_abbrev(buf, "craft")) {
		craft_data *craft = craft_proto(number);
		if (!craft) {
			msg_to_char(ch, "There is no craft with that number.\r\n");
			return;
		}
		do_stat_craft(ch, craft);
	}
	else if (is_abbrev(buf, "crop")) {
		crop_data *crop = crop_proto(number);
		if (!crop) {
			msg_to_char(ch, "There is no crop with that number.\r\n");
			return;
		}
		do_stat_crop(ch, crop);
	}
	else if (is_abbrev(buf, "mobile")) {
		if (!mob_proto(number)) {
			send_to_char("There is no monster with that number.\r\n", ch);
			return;
		}
		mob = read_mobile(number);
		// put it somewhere, briefly
		char_to_room(mob, world_table);
		do_stat_character(ch, mob);
		extract_char(mob);
	}
	else if (is_abbrev(buf, "object")) {
		if (!obj_proto(number)) {
			send_to_char("There is no object with that number.\r\n", ch);
			return;
		}
		obj = read_object(number);
		do_stat_object(ch, obj);
		extract_obj(obj);
	}
	else if (is_abbrev(buf, "roomtemplate")) {
		room_template *rmt = room_template_proto(number);
		if (!rmt) {
			msg_to_char(ch, "There is no room template with that number.\r\n");
			return;
		}
		do_stat_room_template(ch, rmt);
	}
	else if (is_abbrev(buf, "sector")) {
		sector_data *sect = sector_proto(number);
		if (!sect) {
			msg_to_char(ch, "There is no sector with that number.\r\n");
			return;
		}
		do_stat_sector(ch, sect);
	}
	else if (is_abbrev(buf, "trigger")) {
		trig_data *trig = real_trigger(number);
		if (!trig) {
			msg_to_char(ch, "That vnum does not exist.\r\n");
			return;
		}

		do_stat_trigger(ch, trig);
	}
	else
		send_to_char("Invalid type.\r\n", ch);
}


ACMD(do_wizlock) {
	extern int wizlock_level;
	extern char *wizlock_message;
	int value;
	const char *when;

	argument = one_argument(argument, arg);
	if (*arg) {
		value = atoi(arg);
		if (value < 0 || value > GET_ACCESS_LEVEL(ch)) {
			send_to_char("Invalid wizlock value.\r\n", ch);
			return;
		}
		if (wizlock_message) {
			free(wizlock_message);
			wizlock_message = NULL;
		}
		skip_spaces(&argument);
		if (*argument && value != 0) {	// do not copy message on unlock
			wizlock_message = str_dup(strcat(argument, "\r\n"));
		}
		wizlock_level = value;
		when = "now";
		
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has %swizlocked the game at level %d", GET_NAME(ch), (wizlock_level < 1 ? "un" : ""), wizlock_level);
		if (wizlock_message) {
			syslog(SYS_GC, LVL_START_IMM, TRUE, "GC: Wizlock: %s", wizlock_message);
		}
	}
	else {
		when = "currently";
	}

	// messaging
	switch (wizlock_level) {
		case 0:
			msg_to_char(ch, "The game is %s completely open.\r\n", when);
			break;
		case 1:
			msg_to_char(ch, "The game is %s closed to new players.\r\n", when);
			break;
		default:
			msg_to_char(ch, "Only level %d and above may enter the game %s.\r\n", wizlock_level, when);
			break;
	}
	
	if (wizlock_message) {
		msg_to_char(ch, "%s\r\n", wizlock_message);
	}
}


// General fn for wizcommands of the sort: cmd <player>
ACMD(do_wizutil) {
	char_data *vict;
	int result;

	one_argument(argument, arg);

	if (!*arg)
		send_to_char("Yes, but for whom?!?\r\n", ch);
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_WORLD)))
		send_to_char("There is no such player.\r\n", ch);
	else if (IS_NPC(vict))
		send_to_char("You can't do that to a mob!\r\n", ch);
	else if (GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch))
		send_to_char("Hmmm... you'd better not.\r\n", ch);
	else {
		switch (subcmd) {
			case SCMD_NOTITLE:
				result = PLR_TOG_CHK(vict, PLR_NOTITLE);
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: Notitle %s for %s by %s.", ONOFF(result), GET_NAME(vict), GET_NAME(ch));
				msg_to_char(ch, "Notitle %s for %s.\r\n", ONOFF(result), GET_NAME(vict));
				break;
			case SCMD_MUTE:
				result = PLR_TOG_CHK(vict, PLR_MUTED);
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: Mute %s for %s by %s.", ONOFF(result), GET_NAME(vict), GET_NAME(ch));
				msg_to_char(ch, "Mute %s for %s.\r\n", ONOFF(result), GET_NAME(vict));
				break;
			case SCMD_FREEZE:
				if (ch == vict) {
					send_to_char("Oh, yeah, THAT'S real smart...\r\n", ch);
					return;
				}
				if (PLR_FLAGGED(vict, PLR_FROZEN)) {
					send_to_char("Your victim is already pretty cold.\r\n", ch);
					return;
				}
				SET_BIT(PLR_FLAGS(vict), PLR_FROZEN);
				send_to_char("A bitter wind suddenly rises and drains every erg of heat from your body!\r\nYou feel frozen!\r\n", vict);
				send_to_char("Frozen.\r\n", ch);
				act("A sudden cold wind conjured from nowhere freezes $n!", FALSE, vict, 0, 0, TO_ROOM);
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s frozen by %s.", GET_NAME(vict), GET_NAME(ch));
				break;
			case SCMD_THAW:
				if (!PLR_FLAGGED(vict, PLR_FROZEN)) {
					send_to_char("Sorry, your victim is not morbidly encased in ice at the moment.\r\n", ch);
					return;
				}
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s un-frozen by %s.", GET_NAME(vict), GET_NAME(ch));
				REMOVE_BIT(PLR_FLAGS(vict), PLR_FROZEN);
				send_to_char("A fireball suddenly explodes in front of you, melting the ice!\r\nYou feel thawed.\r\n", vict);
				send_to_char("Thawed.\r\n", ch);
				act("A sudden fireball conjured from nowhere thaws $n!", FALSE, vict, 0, 0, TO_ROOM);
				break;
			default:
				log("SYSERR: Unknown subcmd %d passed to do_wizutil (%s)", subcmd, __FILE__);
				break;
		}
		SAVE_CHAR(vict);
	}
}
