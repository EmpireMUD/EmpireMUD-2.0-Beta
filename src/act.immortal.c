/* ************************************************************************
*   File: act.immortal.c                                  EmpireMUD 2.0b5 *
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
extern const char *affected_bits[];
extern const char *apply_types[];
extern const char *bld_on_flags[];
extern const char *bonus_bits[];
extern const char *climate_types[];
extern const char *component_flags[];
extern const char *component_types[];
extern const char *craft_types[];
extern const char *dirs[];
extern const char *extra_bits[];
extern const char *function_flags[];
extern const char *genders[];
extern const char *grant_bits[];
extern const char *instance_flags[];
extern const char *island_bits[];
extern const char *mapout_color_names[];
extern const char *olc_flag_bits[];
extern const char *progress_types[];
extern struct faction_reputation_type reputation_levels[];
extern const char *room_aff_bits[];
extern const char *sector_flags[];
extern const char *size_types[];
extern const char *spawn_flags[];
extern const char *spawn_flags_short[];
extern const char *syslog_types[];

// external functions
void adjust_vehicle_tech(vehicle_data *veh, bool add);
extern int adjusted_instance_limit(adv_data *adv);
void assign_class_abilities(char_data *ch, class_data *cls, int role);
extern struct instance_data *build_instance_loc(adv_data *adv, struct adventure_link_rule *rule, room_data *loc, int dir);	// instance.c
void check_autowiz(char_data *ch);
void check_delayed_load(char_data *ch);
void clear_char_abilities(char_data *ch, any_vnum skill);
void delete_instance(struct instance_data *inst, bool run_cleanup);	// instance.c
void deliver_shipment(empire_data *emp, struct shipping_data *shipd);	// act.item.c
void do_stat_vehicle(char_data *ch, vehicle_data *veh);
extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom, bool allow_fake_loc);
extern int get_highest_access_level(account_data *acct);
void get_icons_display(struct icon_data *list, char *save_buffer);
void get_interaction_display(struct interaction_item *list, char *save_buffer);
void get_player_skill_string(char_data *ch, char *buffer, bool abbrev);
void get_resource_display(struct resource_data *list, char *save_buffer);
void get_script_display(struct trig_proto_list *list, char *save_buffer);
extern char *get_room_name(room_data *room, bool color);
void replace_question_color(char *input, char *color, char *output);
void save_whole_world();
void scale_mob_to_level(char_data *mob, int level);
void scale_vehicle_to_level(vehicle_data *veh, int level);
extern char *show_color_codes(char *string);
extern int stats_get_crop_count(crop_data *cp);
extern int stats_get_sector_count(sector_data *sect);
void update_class(char_data *ch);
void update_world_count();

// locals
void instance_list_row(struct instance_data *inst, int number, char *save_buffer, size_t size);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Removes custom data from an island (as customized by empires).
*
* @param int island_id The island being decustomized.
*/
void decustomize_island(int island_id) {
	struct island_info *island = get_island(island_id, FALSE);
	
	struct empire_island *eisle;
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		HASH_FIND_INT(EMPIRE_ISLANDS(emp), &island_id, eisle);
		if (eisle && eisle->name) {
			log_to_empire(emp, ELOG_TERRITORY, "%s has lost its custom name and is now called %s", eisle->name, island ? island->name : "???");
			if (eisle->name) {
				free(eisle->name);
			}
			eisle->name = NULL;
			EMPIRE_NEEDS_SAVE(emp) = TRUE;
		}
	}
}


/**
* Autostores one item. Contents are also stored.
*
* @param obj_dtaa *obj The item to autostore.
* @param empire_data *emp The empire to store it to -- NOT CURRENTLY USED.
* @param int island The islands to store it to -- NOT CURRENTLY USED.
*/
static void perform_autostore(obj_data *obj, empire_data *emp, int island) {
	extern bool check_autostore(obj_data *obj, bool force);
	
	obj_data *temp, *next_temp;
	
	// store the inside first
	for (temp = obj->contains; temp; temp = next_temp) {
		next_temp = temp->next_content;
		perform_autostore(temp, emp, island);
	}
	
	
	check_autostore(obj, TRUE);
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
		if (REAL_NPC(t) || t == ch) {
			continue;
		}
		if (!CAN_SEE(t, ch) || !WIZHIDE_OK(t, ch)) {
			continue;
		}

		act(buf, TRUE, ch, 0, t, TO_VICT);
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
		if (REAL_NPC(t) || t == ch) {
			continue;
		}
		if (!CAN_SEE(t, ch) || !WIZHIDE_OK(t, ch)) {
			continue;
		}
		
		act(buf, TRUE, ch, 0, t, TO_VICT);
	}
	
	qt_visit_room(ch, IN_ROOM(ch));
	look_at_room(ch);
	enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
	msdp_update_room(ch);	// once we're sure we're staying
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
	if (GET_INVIS_LEV(ch) == 0 && !AFF_FLAGGED(ch, AFF_HIDE) && !PRF_FLAGGED(ch, PRF_WIZHIDE | PRF_INCOGNITO)) {
		send_to_char("You are already fully visible.\r\n", ch);
		return;
	}
	
	REMOVE_BIT(PRF_FLAGS(ch), PRF_WIZHIDE | PRF_INCOGNITO);
	GET_INVIS_LEV(ch) = 0;
	appear(ch);
	send_to_char("You are now fully visible.\r\n", ch);
}


// for show resources
struct show_res_t {
	long long amount;
	struct show_res_t *next;
};


// show resources median sorter
int compare_show_res(struct show_res_t *a, struct show_res_t *b) {
	return a->amount - b->amount;
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

ADMIN_UTIL(util_approval);
ADMIN_UTIL(util_b318_buildings);
ADMIN_UTIL(util_clear_roles);
ADMIN_UTIL(util_diminish);
ADMIN_UTIL(util_evolve);
ADMIN_UTIL(util_islandsize);
ADMIN_UTIL(util_playerdump);
ADMIN_UTIL(util_randtest);
ADMIN_UTIL(util_redo_islands);
ADMIN_UTIL(util_rescan);
ADMIN_UTIL(util_resetbuildingtriggers);
ADMIN_UTIL(util_strlen);
ADMIN_UTIL(util_tool);
ADMIN_UTIL(util_wipeprogress);
ADMIN_UTIL(util_yearly);


struct {
	char *command;
	int level;
	void (*func)(char_data *ch, char *argument);
} admin_utils[] = {
	{ "approval", LVL_CIMPL, util_approval },
	{ "b318buildings", LVL_CIMPL, util_b318_buildings },
	{ "clearroles", LVL_CIMPL, util_clear_roles },
	{ "diminish", LVL_START_IMM, util_diminish },
	{ "evolve", LVL_CIMPL, util_evolve },
	{ "islandsize", LVL_START_IMM, util_islandsize },
	{ "playerdump", LVL_IMPL, util_playerdump },
	{ "randtest", LVL_CIMPL, util_randtest },
	{ "redoislands", LVL_CIMPL, util_redo_islands },
	{ "rescan", LVL_START_IMM, util_rescan },
	{ "resetbuildingtriggers", LVL_CIMPL, util_resetbuildingtriggers },
	{ "strlen", LVL_START_IMM, util_strlen },
	{ "tool", LVL_IMPL, util_tool },
	{ "wipeprogress", LVL_CIMPL, util_wipeprogress },
	{ "yearly", LVL_CIMPL, util_yearly },

	// last
	{ "\n", LVL_TOP+1, NULL }
};


// secret implementor-only util for quick changes -- util tool
ADMIN_UTIL(util_tool) {
	// msg_to_char(ch, "Ok.\r\n");
	trig_data *trig, *next_trig;
	
	HASH_ITER(hh, trigger_table, trig, next_trig) {
		if (trig->attach_type == WLD_TRIGGER || trig->attach_type == RMT_TRIGGER || trig->attach_type == BLD_TRIGGER || trig->attach_type == ADV_TRIGGER) {
			if (IS_SET(GET_TRIG_TYPE(trig), WTRIG_RANDOM)) {
				msg_to_char(ch, "[%5d] %s\r\n", GET_TRIG_VNUM(trig), GET_TRIG_NAME(trig));
			}
		}
	}
}


/**
* This utility converts player approval to/from "by account" or "by character".
*/
ADMIN_UTIL(util_approval) {
	bool to_acc = FALSE, to_char = FALSE, save_acct = FALSE, loaded = FALSE;
	account_data *acct, *next_acct;
	struct account_player *plr;
	char_data *pers;
	
	skip_spaces(&argument);
	
	// optional "by " account/character
	if (!strn_cmp(argument, "by ", 3)) {
		argument += 3;
		skip_spaces(&argument);
	}
	if (is_abbrev(argument, "account")) {
		to_acc = TRUE;
	}
	else if (is_abbrev(argument, "character")) {
		to_char = TRUE;
	}
	else {
		msg_to_char(ch, "Usage: util approval by <character | account>\r\n");
		msg_to_char(ch, "This utility will change all existing approvals to be either by-whole-account or by-character.\r\n");
		return;
	}
	
	// check all accounts
	HASH_ITER(hh, account_table, acct, next_acct) {
		save_acct = FALSE;
		
		// check players on the account
		for (plr = acct->players; plr; plr = plr->next) {
			if (plr->player) {
				if (to_acc && IS_SET(plr->player->plr_flags, PLR_APPROVED) && !IS_SET(acct->flags, ACCT_APPROVED)) {
					// upgrade approval to account
					SET_BIT(acct->flags, ACCT_APPROVED);
					save_acct = TRUE;
					break;	// converting to account: only requires 1 approved alt
				}
				else if (to_char && IS_SET(acct->flags, ACCT_APPROVED) && !IS_SET(plr->player->plr_flags, PLR_APPROVED)) {
					// distribute account approval to characters
					if ((pers = find_or_load_player(plr->name, &loaded))) {
						SET_BIT(PLR_FLAGS(pers), PLR_APPROVED);
						if (loaded) {
							store_loaded_char(pers);
						}
						else {
							SAVE_CHAR(pers);
						}
					}
					// no break in this one -- need to do all alts
				}
			}
		}
		
		// shut off account-wide approval now?
		if (to_char && IS_SET(acct->flags, ACCT_APPROVED)) {
			REMOVE_BIT(acct->flags, ACCT_APPROVED);
			save_acct = TRUE;
		}
		
		// and save
		if (save_acct) {
			save_library_file_for_vnum(DB_BOOT_ACCT, acct->id);
		}
	}
	
	syslog(SYS_VALID, GET_INVIS_LEV(ch), TRUE, "VALID: %s changed existing approvals to be %s", GET_NAME(ch), to_acc ? "account-wide" : "by character, not account");
	msg_to_char(ch, "All approvals are now %s.\r\n", to_acc ? "account-wide" : "by character, not account");
}


// looks up buildings with certain flags
ADMIN_UTIL(util_b318_buildings) {
	extern const char *bld_flags[];
	
	char buf[MAX_STRING_LENGTH];
	bld_data *bld, *next_bld;
	bool any = FALSE;
	
	// these are flags that were used prior to b3.18
	bitvector_t bad_flags = BIT(11) | BIT(13) | BIT(16) | BIT(18) | BIT(19) |
		BIT(20) | BIT(21) | BIT(22) | BIT(23) | BIT(24) | BIT(25) | BIT(26) |
		BIT(27) | BIT(28) | BIT(30) | BIT(31) | BIT(32) | BIT(35) | BIT(36) |
		BIT(38) | BIT(39) | BIT(41) | BIT(45) | BIT(47) | BIT(8) | BIT(17);
	
	HASH_ITER(hh, building_table, bld, next_bld) {
		if (IS_SET(GET_BLD_FLAGS(bld), bad_flags)) {
			sprintbit(GET_BLD_FLAGS(bld) & bad_flags, bld_flags, buf, TRUE);
			msg_to_char(ch, "[%5d] %s: %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld), buf);
			any = TRUE;
		}
	}
	
	if (!any) {
		msg_to_char(ch, "No buildings found with the deprecated b3.18 flags.\r\n");
	}
}


// for util_clear_roles
PLAYER_UPDATE_FUNC(update_clear_roles) {
	if (IS_IMMORTAL(ch)) {
		return;
	}
	
	GET_CLASS_ROLE(ch) = ROLE_NONE;
	assign_class_abilities(ch, NULL, NOTHING);
	
	if (!is_file) {
		msg_to_char(ch, "Your group role has been reset.\r\n");
	}
}


ADMIN_UTIL(util_clear_roles) {
	void update_all_players(char_data *to_message, PLAYER_UPDATE_FUNC(*func));
	update_all_players(ch, update_clear_roles);
	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has cleared all player roles", GET_NAME(ch));
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


ADMIN_UTIL(util_evolve) {
	void run_external_evolutions();
	extern bool manual_evolutions;
	
	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s used util evolve", GET_NAME(ch));
	send_config_msg(ch, "ok_string");
	manual_evolutions = TRUE;	// triggers a log
	run_external_evolutions();
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
	
	HASH_ITER(hh, world_table, room, next_room) {
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
	player_index_data *index, *next_index;
	char_data *plr;
	bool is_file;
	FILE *fl;
	
	if (!(fl = fopen("plrdump.txt", "w"))) {
		msg_to_char(ch, "Unable to open file for writing.\r\n");
		return;
	}
	
	fprintf(fl, "name\taccount\thours\thost\n");
	
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {		
		if (!(plr = find_or_load_player(index->name, &is_file))) {
			continue;
		}
		
		fprintf(fl, "%s\t%d\t%d\t%s\n", GET_NAME(plr), GET_ACCOUNT(plr)->id, (plr->player.time.played / SECS_PER_REAL_HOUR), plr->prev_host);
		
		// done
		if (is_file && plr) {
			free_char(plr);
			is_file = FALSE;
			plr = NULL;
		}
	}
	
	fclose(fl);
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
	void number_and_count_islands(bool reset);
	
	skip_spaces(&argument);
	
	if (!*argument || str_cmp(argument, "confirm")) {
		msg_to_char(ch, "WARNING: This command will re-number all islands and may make some einvs unreachable.\r\n");
		msg_to_char(ch, "Usage: util redoislands confirm\r\n");
	}
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has renumbered islands", GET_NAME(ch));
		number_and_count_islands(TRUE);
		msg_to_char(ch, "Islands renumbered. Caution: empire inventories may now be in the wrong place.\r\n");
	}
}


ADMIN_UTIL(util_rescan) {
	void refresh_empire_goals(empire_data *emp, any_vnum only_vnum);
	
	empire_data *emp;
	
	if (GET_ACCESS_LEVEL(ch) < LVL_CIMPL && !IS_GRANTED(ch, GRANT_EMPIRES)) {
		msg_to_char(ch, "You don't have permission to rescan empires.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Usage: rescan <empire | all>\r\n");
	}
	else if (!str_cmp(argument, "all")) {
		syslog(SYS_INFO, GET_INVIS_LEV(ch), TRUE, "Rescanning all empires");
		reread_empire_tech(NULL);
		send_config_msg(ch, "ok_string");
	}
	else if (!(emp = get_empire_by_name(argument))) {
		msg_to_char(ch, "Unknown empire.\r\n");
	}
	else {
		syslog(SYS_INFO, GET_INVIS_LEV(ch), TRUE, "Rescanning empire: %s", EMPIRE_NAME(emp));
		reread_empire_tech(emp);
		refresh_empire_goals(emp, NOTHING);
		send_config_msg(ch, "ok_string");
	}
}


ADMIN_UTIL(util_resetbuildingtriggers) {
	room_data *room, *next_room;
	bld_data *proto = NULL;
	bool all = FALSE;
	int count = 0;
	
	all = !str_cmp(argument, "all");
	
	if (!all && (!*argument || !isdigit(*argument))) {
		msg_to_char(ch, "Usage: resetbuildingtriggers <vnum | all>\r\n");
	}
	else if (!all && !(proto = building_proto(atoi(argument)))) {
		msg_to_char(ch, "Invalid building/room vnum '%s'.\r\n", argument);
	}
	else {
		HASH_ITER(hh, world_table, room, next_room) {
			if (!GET_BUILDING(room)) {
				continue;
			}
			if (!all && GET_BUILDING(room) != proto) {
				continue;
			}
			
			// remove old triggers
			remove_all_triggers(room, WLD_TRIGGER);
			free_proto_scripts(&room->proto_script);
			
			// add any triggers
			room->proto_script = copy_trig_protos(GET_BLD_SCRIPTS(GET_BUILDING(room)));
			assign_triggers(room, WLD_TRIGGER);
			
			++count;
		}
		
		msg_to_char(ch, "%d building%s/room%s updated.\r\n", count, PLURAL(count), PLURAL(count));
	}
}


ADMIN_UTIL(util_strlen) {
	msg_to_char(ch, "String: %s\r\n", argument);
	msg_to_char(ch, "Raw: %s\r\n", show_color_codes(argument));
	msg_to_char(ch, "strlen: %d\r\n", (int)strlen(argument));
	msg_to_char(ch, "color_strlen: %d\r\n", (int)color_strlen(argument));
	msg_to_char(ch, "color_code_length: %d\r\n", color_code_length(argument));
}


ADMIN_UTIL(util_wipeprogress) {
	void full_reset_empire_progress(empire_data *only_emp);
	
	empire_data *emp = NULL;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: util wipeprogress <empire | all>\r\n");
	}
	else if (str_cmp(argument, "all") && !(emp = get_empire_by_name(argument))) {
		msg_to_char(ch, "Unknown empire '%s'.\r\n", argument);
	}
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has wiped empire progress for %s", GET_REAL_NAME(ch), emp ? EMPIRE_NAME(emp) : "all empires");
		send_config_msg(ch, "ok_string");
		full_reset_empire_progress(emp);	// if NULL, does ALL
	}
}


ADMIN_UTIL(util_yearly) {
	void annual_world_update();
	
	if (!*argument || str_cmp(argument, "confirm")) {
		msg_to_char(ch, "You must type 'util yearly confirm' to do this. It will cause decay on the entire map.\r\n");
	}
	else {
		send_config_msg(ch, "ok_string");
		annual_world_update();
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
	extern room_data *find_location_for_rule(adv_data *adv, struct adventure_link_rule *rule, int *which_dir);
	extern const bool is_location_rule[];

	struct adventure_link_rule *rule, *rule_iter;
	int num_rules, tries, dir = NO_DIR;
	bool found = FALSE;
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
	
	// randomly choose one rule to attempt
	for (tries = 0; tries < 5 && !found; ++tries) {
		num_rules = 0;
		rule = NULL;
		for (rule_iter = GET_ADV_LINKING(adv); rule_iter; rule_iter = rule_iter->next) {
			if (is_location_rule[rule_iter->type]) {
				// choose one at random
				if (!number(0, num_rules++) || !rule) {
					rule = rule_iter;
				}
			}
		}
	
		if (rule) {
			if ((loc = find_location_for_rule(adv, rule, &dir))) {
				// make it so!
				if (build_instance_loc(adv, rule, loc, dir)) {
					found = TRUE;
				}
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
	struct instance_data *inst;
	room_data *loc;
	int num;
	
	if (!*argument || !isdigit(*argument) || (num = atoi(argument)) < 1) {
		msg_to_char(ch, "Invalid instance number '%s'.\r\n", *argument ? argument : "<blank>");
		return;
	}
	
	LL_FOREACH(instance_list, inst) {
		if (--num == 0) {
			if ((loc = INST_LOCATION(inst))) {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s deleted an instance of %s at %s", GET_REAL_NAME(ch), GET_ADV_NAME(INST_ADVENTURE(inst)), room_log_identifier(loc));
			}
			else {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s deleted an instance of %s at unknown location", GET_REAL_NAME(ch), GET_ADV_NAME(INST_ADVENTURE(inst)));
			}
			msg_to_char(ch, "Instance of %s deleted.\r\n", GET_ADV_NAME(INST_ADVENTURE(inst)));
			delete_instance(inst, TRUE);
			break;
		}
	}
	
	if (num > 0) {
		msg_to_char(ch, "Invalid instance number %d.\r\n", atoi(argument));
	}
}


void do_instance_delete_all(char_data *ch, char *argument) {
	struct instance_data *inst, *next_inst;
	descriptor_data *desc;
	adv_vnum vnum;
	adv_data *adv = NULL;
	int count = 0;
	bool all;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: instance deleteall <adventure vnum | all>\r\n");
		return;
	}
	
	all = !str_cmp(argument, "all");
	if (!all && (!isdigit(*argument) || (vnum = atoi(argument)) < 0 || !(adv = adventure_proto(vnum)))) {
		msg_to_char(ch, "Invalid adventure vnum '%s'.\r\n", argument);
		return;
	}
	
	// warn players of lag on 'all'
	LL_FOREACH(descriptor_list, desc) {
		if (STATE(desc) == CON_PLAYING && desc->character) {
			write_to_descriptor(desc->descriptor, "The game is performing a brief update... this will take a moment.\r\n");
			desc->has_prompt = FALSE;
		}
	}
	
	LL_FOREACH_SAFE(instance_list, inst, next_inst) {
		if (all || INST_ADVENTURE(inst) == adv) {
			++count;
			delete_instance(inst, TRUE);
		}
	}
	
	if (count > 0) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s deleted %d instance%s of %s", GET_REAL_NAME(ch), count, PLURAL(count), all ? "adventures" : GET_ADV_NAME(adv));
		msg_to_char(ch, "%d instance%s of '%s' deleted.\r\n", count, PLURAL(count), all ? "adventures" : GET_ADV_NAME(adv));
	}
	else {
		msg_to_char(ch, "No instances of %s found.\r\n", all ? "any adventures" : GET_ADV_NAME(adv));
	}
}


void do_instance_info(char_data *ch, char *argument) {
	struct instance_mob *mc, *next_mc;
	char buf[MAX_STRING_LENGTH];
	struct instance_data *inst;
	int num;
	
	if (!*argument || !isdigit(*argument) || (num = atoi(argument)) < 1) {
		msg_to_char(ch, "Invalid instance number '%s'.\r\n", *argument ? argument : "<blank>");
		return;
	}
	
	LL_FOREACH(instance_list, inst) {
		if (--num == 0) {
			msg_to_char(ch, "\tcInstance %d: [%d] %s\t0\r\n", atoi(argument), GET_ADV_VNUM(INST_ADVENTURE(inst)), GET_ADV_NAME(INST_ADVENTURE(inst)));
			
			if (INST_LOCATION(inst)) {
				if (ROOM_OWNER(INST_LOCATION(inst))) {
					sprintf(buf, " / %s%s\t0", EMPIRE_BANNER(ROOM_OWNER(INST_LOCATION(inst))), EMPIRE_NAME(ROOM_OWNER(INST_LOCATION(inst))));
				}
				else {
					*buf = '\0';
				}
				msg_to_char(ch, "Location: [%d] %s%s%s\r\n", GET_ROOM_VNUM(INST_LOCATION(inst)), get_room_name(INST_LOCATION(inst), FALSE), coord_display_room(ch, INST_LOCATION(inst), FALSE), buf);
			}
			if (INST_FAKE_LOC(inst) && INST_FAKE_LOC(inst) != INST_LOCATION(inst)) {
				if (ROOM_OWNER(INST_FAKE_LOC(inst))) {
					sprintf(buf, " / %s%s\t0", EMPIRE_BANNER(ROOM_OWNER(INST_FAKE_LOC(inst))), EMPIRE_NAME(ROOM_OWNER(INST_FAKE_LOC(inst))));
				}
				else {
					*buf = '\0';
				}
				msg_to_char(ch, "Fake Location: [%d] %s%s%s\r\n", GET_ROOM_VNUM(INST_FAKE_LOC(inst)), get_room_name(INST_FAKE_LOC(inst), FALSE), coord_display_room(ch, INST_FAKE_LOC(inst), FALSE), buf);
			}
			
			if (INST_START(inst)) {
				msg_to_char(ch, "First room: [%d] %s\r\n", GET_ROOM_VNUM(INST_START(inst)), get_room_name(INST_START(inst), FALSE));
			}
			
			if (INST_LEVEL(inst) > 0) {
				msg_to_char(ch, "Level: %d\r\n", INST_LEVEL(inst));
			}
			else {
				msg_to_char(ch, "Level: unscaled\r\n");
			}
			
			sprintbit(INST_FLAGS(inst), instance_flags, buf, TRUE);
			msg_to_char(ch, "Flags: %s\r\n", buf);
			
			msg_to_char(ch, "Created: %-24.24s\r\n", (char *) asctime(localtime(&INST_CREATED(inst))));
			if (INST_LAST_RESET(inst) != INST_CREATED(inst)) {
				msg_to_char(ch, "Last reset: %-24.24s\r\n", (char *) asctime(localtime(&INST_LAST_RESET(inst))));
			}
			
			if (INST_DIR(inst) != NO_DIR) {
				msg_to_char(ch, "Facing: %s\r\n", dirs[INST_DIR(inst)]);
			}
			if (INST_ROTATION(inst) != NO_DIR && IS_SET(GET_ADV_FLAGS(INST_ADVENTURE(inst)), ADV_ROTATABLE)) {
				msg_to_char(ch, "Rotation: %s\r\n", dirs[INST_ROTATION(inst)]);
			}
			
			if (INST_MOB_COUNTS(inst)) {
				msg_to_char(ch, "Mob counts:\r\n");
				HASH_ITER(hh, INST_MOB_COUNTS(inst), mc, next_mc) {
					msg_to_char(ch, "%3d %s\r\n", mc->count, skip_filler(get_mob_name_by_proto(mc->vnum)));
				}
			}
			
			break;	// only show 1
		}
	}
	
	if (num > 0) {
		msg_to_char(ch, "Invalid instance number %d.\r\n", atoi(argument));
	}
}


// shows by adventure
void do_instance_list_all(char_data *ch) {
	extern int count_instances(adv_data *adv);
	
	char buf[MAX_STRING_LENGTH];
	adv_data *adv, *next_adv;
	int count = 0;
	size_t size;
	
	size = snprintf(buf, sizeof(buf), "Instances by adventure:\r\n");
	
	// list by adventure
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		if (size >= sizeof(buf)) {
			break;
		}
		
		// skip in-dev adventures with no count
		count = count_instances(adv);
		if (ADVENTURE_FLAGGED(adv, ADV_IN_DEVELOPMENT) && !count) {
			continue;
		}
		
		size += snprintf(buf + size, sizeof(buf) - size, "[%5d] %s (%d/%d)\r\n", GET_ADV_VNUM(adv), GET_ADV_NAME(adv), count, adjusted_instance_limit(adv));
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


// list by name
void do_instance_list(char_data *ch, char *argument) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	struct instance_data *inst;
	adv_data *adv = NULL;
	adv_vnum vnum;
	int num = 0, count = 0;
	
	if (!ch->desc) {
		return;
	}
	
	// new in b3.20: no-arg shows a different list entirely
	if (!*argument) {
		do_instance_list_all(ch);
		return;
	}
	
	if (*argument && (!isdigit(*argument) || (vnum = atoi(argument)) < 0 || !(adv = adventure_proto(vnum)))) {
		msg_to_char(ch, "Invalid adventure vnum '%s'.\r\n", argument);
		return;
	}
	
	*buf = '\0';
	
	LL_FOREACH(instance_list, inst) {
		// num is out of the total instances, not just ones shown
		++num;
		
		if (!adv || adv == INST_ADVENTURE(inst)) {
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
	room_data *inst_loc, *loc;
	
	loc = GET_MAP_LOC(IN_ROOM(ch)) ? real_room(GET_MAP_LOC(IN_ROOM(ch))->vnum) : NULL;
	
	if (*argument && (!isdigit(*argument) || (distance = atoi(argument)) < 0)) {
		msg_to_char(ch, "Invalid distance '%s'.\r\n", argument);
		return;
	}
	
	size = snprintf(buf, sizeof(buf), "Instances within %d tiles:\r\n", distance);
	
	if (loc) {	// skip work if no map location found
		LL_FOREACH(instance_list, inst) {
			++num;
		
			inst_loc = INST_FAKE_LOC(inst);
			if (inst_loc && !INSTANCE_FLAGGED(inst, INST_COMPLETED) && compute_distance(loc, inst_loc) <= distance) {
				++count;
				instance_list_row(inst, num, line, sizeof(line));
				size += snprintf(buf + size, sizeof(buf) - size, "%s", line);
				if (size >= sizeof(buf)) {
					break;
				}
			}
		}
	}
	
	snprintf(buf + size, sizeof(buf) - size, "%d total instances shown\r\n", count);
	page_string(ch->desc, buf, TRUE);
}


void do_instance_reset(char_data *ch, char *argument) {
	void reset_instance(struct instance_data *inst);

	struct instance_data *inst;
	room_data *loc = NULL;
	int num;
	
	if (*argument) {
		if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid instance number '%s'.\r\n", *argument ? argument : "<blank>");
			return;
		}
	
		LL_FOREACH(instance_list, inst) {
			if (--num == 0) {
				loc = INST_FAKE_LOC(inst);
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
		if (!(inst = find_instance_by_room(IN_ROOM(ch), FALSE, TRUE))) {
			msg_to_char(ch, "You are not in or near an adventure zone instance.\r\n");
			return;
		}

		loc = INST_FAKE_LOC(inst);
		reset_instance(inst);
	}
	
	if (loc) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s forced a reset of an instance of %s at %s", GET_REAL_NAME(ch), GET_ADV_NAME(INST_ADVENTURE(inst)), room_log_identifier(loc));
	}
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s forced a reset of an instance of %s at unknown location", GET_REAL_NAME(ch), GET_ADV_NAME(INST_ADVENTURE(inst)));
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
	char flg[256], info[256], owner[MAX_STRING_LENGTH];

	if (INST_LEVEL(inst) > 0) {
		sprintf(info, " L%d", INST_LEVEL(inst));
	}
	else {
		*info = '\0';
	}
	
	if (INST_FAKE_LOC(inst) && ROOM_OWNER(INST_FAKE_LOC(inst))) {
		snprintf(owner, sizeof(owner), "(%s%s\t0)", EMPIRE_BANNER(ROOM_OWNER(INST_FAKE_LOC(inst))), EMPIRE_NAME(ROOM_OWNER(INST_FAKE_LOC(inst))));
	}
	else {
		*owner = '\0';
	}

	sprintbit(INST_FLAGS(inst), instance_flags, flg, TRUE);
	snprintf(save_buffer, size, "%3d. [%5d] %s [%d] (%d, %d)%s %s%s\r\n", number, GET_ADV_VNUM(INST_ADVENTURE(inst)), GET_ADV_NAME(INST_ADVENTURE(inst)), INST_FAKE_LOC(inst) ? GET_ROOM_VNUM(INST_FAKE_LOC(inst)) : NOWHERE, INST_FAKE_LOC(inst) ? X_COORD(INST_FAKE_LOC(inst)) : NOWHERE, INST_FAKE_LOC(inst) ? Y_COORD(INST_FAKE_LOC(inst)) : NOWHERE, info, INST_FLAGS(inst) != NOBITS ? flg : "", owner);
}


void do_instance_spawn(char_data *ch, char *argument) {
	void generate_adventure_instances();
	int num = 1;
	
	if (*argument && isdigit(*argument)) {
		num = atoi(argument);
	}
	if (num < 1 || num > 100) {
		msg_to_char(ch, "You may only run 1-100 spawn cycles.\r\n");
		return;
	}
	
	msg_to_char(ch, "Running %d instance spawn cycle%s...\r\n", num, PLURAL(num));
	while (num-- > 0) {
		generate_adventure_instances();
	}
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
		{ "nocustomize", LVL_START_IMM,	PC,		BINARY },

		{ "health",		LVL_START_IMM, 	BOTH, 	NUMBER },
		{ "move",		LVL_START_IMM, 	BOTH, 	NUMBER },
		{ "mana",		LVL_START_IMM, 	BOTH, 	NUMBER },
		{ "blood",		LVL_START_IMM, 	BOTH, 	NUMBER },

		{ "coins",		LVL_START_IMM,	PC,		MISC },
		{ "frozen",		LVL_START_IMM,	PC, 	BINARY },
		{ "drunk",		LVL_START_IMM, 	BOTH, 	MISC },
		{ "hunger",		LVL_START_IMM, 	BOTH, 	MISC },
		{ "thirst",		LVL_START_IMM, 	BOTH, 	MISC },
		{ "account",	LVL_START_IMM,	PC,		MISC },
		{ "access",		LVL_CIMPL, 	PC, 	NUMBER },
		{ "siteok",		LVL_START_IMM, 	PC, 	BINARY },
		{ "nowizlist", 	LVL_START_IMM, 	PC, 	BINARY },
		{ "loadroom", 	LVL_START_IMM, 	PC, 	MISC },
		{ "password",	LVL_CIMPL, 	PC, 	MISC },
		{ "nodelete", 	LVL_CIMPL, 	PC, 	BINARY },
		{ "noidleout",	LVL_START_IMM,	PC,		BINARY },
		{ "sex", 		LVL_START_IMM, 	BOTH, 	MISC },
		{ "age",		LVL_START_IMM,	BOTH,	NUMBER },
		{ "lastname",	LVL_START_IMM,	PC,		MISC },
		{ "muted",		LVL_START_IMM,	PC, 	BINARY },
		{ "name",		LVL_CIMPL,	PC,		MISC },
		{ "incognito",	LVL_START_IMM,	PC,		BINARY },
		{ "ipmask",		LVL_START_IMM,	PC,		BINARY },
		{ "multi-ip",	LVL_START_IMM,	PC,		BINARY },
		{ "multi-char",	LVL_START_IMM,	PC,		BINARY },	// deliberately after multi-ip, which is more common
		{ "vampire",	LVL_START_IMM,	PC, 	BINARY },
		{ "wizhide",	LVL_START_IMM,	PC,		BINARY },
		{ "bonustrait",	LVL_START_IMM,	PC,		MISC },
		{ "bonusexp", LVL_START_IMM, PC, NUMBER },
		{ "dailyquestscompleted", LVL_START_IMM, PC, NUMBER },
		{ "grants",		LVL_CIMPL,	PC,		MISC },
		{ "maxlevel", LVL_START_IMM, PC, NUMBER },
		{ "skill", LVL_START_IMM, PC, MISC },
		{ "faction", LVL_START_IMM, PC, MISC },
		{ "learned", LVL_START_IMM, PC, MISC },
		{ "minipet", LVL_START_IMM, PC, MISC },
		{ "mount", LVL_START_IMM, PC, MISC },
		{ "currency", LVL_START_IMM, PC, MISC },

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

	player_index_data *index, *next_index, *found_index;
	int i, iter, on = 0, off = 0, value = 0;
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
		sprintf(output, "%s's title is now: %s", GET_NAME(vict), NULLSAFE(GET_TITLE(vict)));
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
			TRIGGER_DELAYED_REFRESH(emp, DELAY_REFRESH_MEMBERS);
			et_change_greatness(emp);
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
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_FROZEN);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("muted") {
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_MUTED);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("nocustomize") {
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_NOCUSTOMIZE);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("notitle") {
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_NOTITLE);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("ipmask") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_IPMASK);
	}
	else if SET_CASE("incognito") {
		SET_OR_REMOVE(PRF_FLAGS(vict), PRF_INCOGNITO);
	}
	else if SET_CASE("wizhide") {
		SET_OR_REMOVE(PRF_FLAGS(vict), PRF_WIZHIDE);
	}
	else if SET_CASE("multi-ip") {
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_MULTI_IP);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("multi-char") {
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_MULTI_CHAR);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("vampire") {
		if (IS_VAMPIRE(vict)) {
			void un_vampire(char_data *ch);
			un_vampire(vict);
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
	else if SET_CASE("access") {
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
		if (!IS_NPC(vict)) {
			GET_IMMORTAL_LEVEL(vict) = (GET_ACCESS_LEVEL(vict) > LVL_MORTAL ? (LVL_TOP - GET_ACCESS_LEVEL(vict)) : -1);
		}
		SAVE_CHAR(vict);
		check_autowiz(ch);
	}
	else if SET_CASE("siteok") {
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_SITEOK);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
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
	else if SET_CASE("password") {
		if (GET_ACCESS_LEVEL(vict) >= LVL_IMPL) {
			send_to_char("You cannot change that.\r\n", ch);
			return (0);
		}
		if (GET_PASSWD(vict)) {
			free(GET_PASSWD(vict));
		}
		GET_PASSWD(vict) = str_dup(CRYPT(val_arg, PASSWORD_SALT));
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
		sprintf(output, "%s's sex is now %s.", GET_NAME(vict), genders[(int) GET_REAL_SEX(vict)]);
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
		else if (!strcmp(val_arg, "off") || !strcmp(val_arg, "none")) {
			if (GET_LASTNAME(vict) != NULL)
				free(GET_LASTNAME(vict));
			GET_LASTNAME(vict) = NULL;
    		sprintf(output, "%s no longer has a last name.", GET_NAME(vict));
		}
    	else {
			if (GET_LASTNAME(vict) != NULL)
				free(GET_LASTNAME(vict));
			GET_LASTNAME(vict) = str_dup(val_arg);
    		sprintf(output, "%s's last name is now: %s", GET_NAME(vict), GET_LASTNAME(vict));
		}
	}
	else if SET_CASE("bonustrait") {
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
	else if SET_CASE("bonusexp") {
		GET_DAILY_BONUS_EXPERIENCE(vict) = RANGE(0, 255);
	}
	else if SET_CASE("dailyquestscompleted") {
		GET_DAILY_QUESTS(vict) = RANGE(0, config_get_int("dailies_per_day"));
	}
	else if SET_CASE("maxlevel") {
		GET_HIGHEST_KNOWN_LEVEL(vict) = RANGE(0, SHRT_MAX);
	}
	else if SET_CASE("grants") {
		bitvector_t new, old = GET_GRANT_FLAGS(vict);
		new = GET_GRANT_FLAGS(vict) = olc_process_flag(ch, val_arg, "grants", NULL, grant_bits, GET_GRANT_FLAGS(vict));
		
		// this indicates a change
		if (new != old) {
			prettier_sprintbit(new, grant_bits, buf);
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
			sprintf(output, "%s now has %s.", GET_NAME(vict), money_amount(type, amt));
			
			// we're going to use increase_coins, so first see if they already had some
			if ((coin = find_coin_entry(GET_PLAYER_COINS(vict), type))) {
				amt -= coin->amount;
			}
			increase_coins(vict, type, amt);
		}
	}
	else if SET_CASE("faction") {
		void update_reputations(char_data *ch);
		
		int min_idx, min_rep, max_idx, max_rep, new_rep, new_val = 0;
		char fct_arg[MAX_INPUT_LENGTH], *fct_val;
		struct player_faction_data *pfd;
		bool del_rep = FALSE;
		faction_data *fct;
		
		// usage: set <player> faction <name/vnum> <rep/value>
		fct_val = any_one_word(val_arg, fct_arg);
		skip_spaces(&fct_val);
		
		if (!*fct_arg || !*fct_val) {
			msg_to_char(ch, "Usage: set <player> faction <name/vnum> <rep/value>\r\n");
			return 0;
		}
		if (!(fct = find_faction(fct_arg)) || FACTION_FLAGGED(fct, FCT_IN_DEVELOPMENT)) {
			msg_to_char(ch, "Unknown faction '%s'.\r\n", fct_arg);
			return 0;
		}
		
		// helper data
		min_idx = rep_const_to_index(FCT_MIN_REP(fct));
		min_rep = (min_idx != NOTHING) ? reputation_levels[min_idx].value : MIN_REPUTATION;
		max_idx = rep_const_to_index(FCT_MAX_REP(fct));
		max_rep = (max_idx != NOTHING) ? reputation_levels[max_idx].value : MAX_REPUTATION;
		
		// parse val arg
		if (!str_cmp(fct_val, "none")) {
			del_rep = TRUE;
		}
		else if (isdigit(*fct_val) || *fct_val == '-') {
			new_val = atoi(fct_val);
		}
		else if ((new_rep = get_reputation_by_name(fct_val)) == NOTHING) {
			msg_to_char(ch, "Invalid reputation level '%s'.\r\n", fct_val);
			return 0;
		}
		else {
			new_val = reputation_levels[rep_const_to_index(new_rep)].value;
		}
		
		// bounds check
		if (!del_rep && (new_val < min_rep || new_val > max_rep)) {
			msg_to_char(ch, "You can't set the reputation to that level. That faction has a range of %d-%d.\r\n", min_rep, max_rep);
			return 0;
		}
		
		// load data
		if (!(pfd = get_reputation(vict, FCT_VNUM(fct), TRUE))) {
			msg_to_char(ch, "Unable to set reputation for that player and faction.\r\n");
			return 0;
		}
		
		// and go
		if (del_rep) {
			HASH_DEL(GET_FACTIONS(vict), pfd);
			free(pfd);
			sprintf(output, "%s's reputation with %s deleted.", GET_NAME(vict), FCT_NAME(fct));
		}
		else {
			pfd->value = new_val;
			update_reputations(vict);
			pfd = get_reputation(vict, FCT_VNUM(fct), TRUE);
			new_rep = rep_const_to_index(pfd->rep);
			sprintf(output, "%s's reputation with %s set to %s / %d.", GET_NAME(vict), FCT_NAME(fct), reputation_levels[new_rep].name, pfd->value);
		}
	}
	else if SET_CASE("skill") {
		void check_ability_levels(char_data *ch, any_vnum skill);
		char skillname[MAX_INPUT_LENGTH], *skillval;
		int level = -1;
		skill_data *skill;
		
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
		else if (!(skill = find_skill(skillname))) {
			msg_to_char(ch, "Unknown skill '%s'.\r\n", skillname);
			return 0;
		}

		// victory
		set_skill(vict, SKILL_VNUM(skill), level);
		update_class(vict);
		check_ability_levels(vict, SKILL_VNUM(skill));
		assign_class_abilities(vict, NULL, NOTHING);
		sprintf(output, "%s's %s set to %d.", GET_NAME(vict), SKILL_NAME(skill), level);
	}
	else if SET_CASE("learned") {
		void add_learned_craft(char_data *ch, any_vnum vnum);
		void remove_learned_craft(char_data *ch, any_vnum vnum);
		char vnum_arg[MAX_INPUT_LENGTH], onoff_arg[MAX_INPUT_LENGTH];
		craft_data *cft;
		
		half_chop(val_arg, vnum_arg, onoff_arg);
		
		if (!*vnum_arg || !isdigit(*vnum_arg) || !*onoff_arg) {
			msg_to_char(ch, "Usage: set <name> learned <craft vnum> <on | off>\r\n");
			return 0;
		}
		if (!(cft = craft_proto(atoi(vnum_arg))) || !CRAFT_FLAGGED(cft, CRAFT_LEARNED)) {
			msg_to_char(ch, "Invalid craft (must be LEARNED and not IN-DEV).\r\n");
			return 0;
		}
		
		if (!str_cmp(onoff_arg, "on")) {
			if (CRAFT_FLAGGED(cft, CRAFT_IN_DEVELOPMENT) && !IS_IMMORTAL(vict)) {
				msg_to_char(ch, "Craft must not be IN-DEV to set it on a player.\r\n");
				return 0;
			}
			
			add_learned_craft(ch, GET_CRAFT_VNUM(cft));
			sprintf(output, "%s learned craft %d %s.", GET_NAME(vict), GET_CRAFT_VNUM(cft), GET_CRAFT_NAME(cft));
		}
		else if (!str_cmp(onoff_arg, "off")) {
			remove_learned_craft(ch, GET_CRAFT_VNUM(cft));
			sprintf(output, "%s un-learned craft %d %s.", GET_NAME(vict), GET_CRAFT_VNUM(cft), GET_CRAFT_NAME(cft));
		}
		else {
			msg_to_char(ch, "Do you want to turn it on or off?\r\n");
			return 0;
		}
	}
	else if SET_CASE("minipet") {
		void add_minipet(char_data *ch, any_vnum vnum);
		void remove_minipet(char_data *ch, any_vnum vnum);
		char vnum_arg[MAX_INPUT_LENGTH], onoff_arg[MAX_INPUT_LENGTH];
		char_data *pet;
		
		half_chop(val_arg, vnum_arg, onoff_arg);
		
		if (!*vnum_arg || !isdigit(*vnum_arg) || !*onoff_arg) {
			msg_to_char(ch, "Usage: set <name> minipet <mob vnum> <on | off>\r\n");
			return 0;
		}
		if (!(pet = mob_proto(atoi(vnum_arg)))) {
			msg_to_char(ch, "Invalid mob vnum.\r\n");
			return 0;
		}
		
		if (!str_cmp(onoff_arg, "on")) {
			add_minipet(vict, GET_MOB_VNUM(pet));
			sprintf(output, "%s: gained mini-pet %d %s.", GET_NAME(vict), GET_MOB_VNUM(pet), GET_SHORT_DESC(pet));
		}
		else if (!str_cmp(onoff_arg, "off")) {
			remove_minipet(vict, GET_MOB_VNUM(pet));
			sprintf(output, "%s: removed mini-pet %d %s.", GET_NAME(vict), GET_MOB_VNUM(pet), GET_SHORT_DESC(pet));
		}
		else {
			msg_to_char(ch, "Do you want to turn it on or off?\r\n");
			return 0;
		}
	}
	else if SET_CASE("mount") {
		char vnum_arg[MAX_INPUT_LENGTH], onoff_arg[MAX_INPUT_LENGTH];
		struct mount_data *mentry;
		char_data *mount;
		
		half_chop(val_arg, vnum_arg, onoff_arg);
		
		if (!*vnum_arg || !isdigit(*vnum_arg) || !*onoff_arg) {
			msg_to_char(ch, "Usage: set <name> mount <mob vnum> <on | off>\r\n");
			return 0;
		}
		if (!(mount = mob_proto(atoi(vnum_arg)))) {
			msg_to_char(ch, "Invalid mob vnum.\r\n");
			return 0;
		}
		
		if (!str_cmp(onoff_arg, "on")) {
			if (find_mount_data(vict, GET_MOB_VNUM(mount))) {
				msg_to_char(ch, "%s already has that mount.\r\n", GET_NAME(vict));
				return 0;
			}
			add_mount(vict, GET_MOB_VNUM(mount), get_mount_flags_by_mob(mount));
			sprintf(output, "%s: gained mount %d %s.", GET_NAME(vict), GET_MOB_VNUM(mount), GET_SHORT_DESC(mount));
		}
		else if (!str_cmp(onoff_arg, "off")) {
			if ((mentry = find_mount_data(vict, GET_MOB_VNUM(mount)))) {
				HASH_DEL(GET_MOUNT_LIST(vict), mentry);
				free(mentry);
				sprintf(output, "%s: removed mount %d %s.", GET_NAME(vict), GET_MOB_VNUM(mount), GET_SHORT_DESC(mount));
			}
			else {
				msg_to_char(ch, "%s does not have that mount.\r\n", GET_NAME(vict));
				return 0;
			}
		}
		else {
			msg_to_char(ch, "Do you want to turn it on or off?\r\n");
			return 0;
		}
	}
	else if SET_CASE("currency") {
		char vnum_arg[MAX_INPUT_LENGTH], amt_arg[MAX_INPUT_LENGTH];
		generic_data *gen;
		int amt;
		
		half_chop(val_arg, vnum_arg, amt_arg);
		
		if (!*vnum_arg || !isdigit(*vnum_arg) || !*amt_arg) {
			msg_to_char(ch, "Usage: set <name> currency <vnum> <amount>\r\n");
			return 0;
		}
		if (!(gen = find_generic(atoi(vnum_arg), GENERIC_CURRENCY))) {
			msg_to_char(ch, "Invalid currency vnum.\r\n");
			return 0;
		}
		if (!isdigit(*amt_arg) || (amt = atoi(amt_arg)) < 0) {
			msg_to_char(ch, "You must set it to zero or greater.\r\n");
			return 0;
		}
		
		amt = add_currency(vict, GEN_VNUM(gen), amt - get_currency(vict, GEN_VNUM(gen)));
		sprintf(output, "%s's %d %s set to %d.", GET_NAME(vict), GEN_VNUM(gen), GEN_NAME(gen), amt);
	}

	else if SET_CASE("account") {
		if (get_highest_access_level(GET_ACCOUNT(vict)) > GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "You can't edit accounts for people above your access level.\r\n");
			return 0;
		}
		if (!str_cmp(val_arg, "new")) {
			sprintf(output, "%s is now associated with a new account.", GET_NAME(vict));
			remove_player_from_account(vict);
			create_account_for_player(vict);
		}
		else {
			// load 2nd player
			if ((alt = find_or_load_player(val_arg, &file))) {
				if (get_highest_access_level(GET_ACCOUNT(alt)) > GET_ACCESS_LEVEL(ch)) {
					msg_to_char(ch, "You can't edit accounts for people above your access level.\r\n");
					if (file) {
						free_char(alt);
					}
					return 0;
				}
				
				sprintf(output, "%s is now associated with %s's account.", GET_NAME(vict), GET_NAME(alt));
				
				remove_player_from_account(vict);
				// does 2nd player have an account already? if not, make one
				if (!GET_ACCOUNT(alt)) {
					create_account_for_player(alt);
					add_player_to_account(vict, GET_ACCOUNT(alt));

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
					add_player_to_account(vict, GET_ACCOUNT(alt));
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
			TRIGGER_DELAYED_REFRESH(emp, DELAY_REFRESH_MEMBERS);
		}
	}

	else if SET_CASE("name") {
		void add_player_to_table(player_index_data *plr);
		void remove_player_from_table(player_index_data *plr);

		SAVE_CHAR(vict);
		one_argument(val_arg, buf);
		if (_parse_name(buf, newname) || fill_word(newname) || strlen(newname) > MAX_NAME_LENGTH || strlen(newname) < 2 || !Valid_Name(newname)) {
			msg_to_char(ch, "Invalid name.\r\n");
			return 0;
		}
		found_index = NULL;
		HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
			if (!str_cmp(index->name, newname)) {
				msg_to_char(ch, "That name is already taken.\r\n");
				return 0;
			}
			if (!str_cmp(index->name, GET_NAME(vict))) {
				if (found_index) {
					msg_to_char(ch, "WARNING: possible pfile corruption, more than one entry with that name.\r\n");
				}
				else {
					found_index = index;
				}
			}
		}
		if (!found_index) {
			msg_to_char(ch, "WARNING: That character does not have a record in the pfile.\r\nName not changed.\r\n");
			return 0;
		}
		strcpy(oldname, GET_PC_NAME(vict));
		if (GET_PC_NAME(vict)) {
			free(GET_PC_NAME(vict));
		}
		GET_PC_NAME(vict) = strdup(CAP(newname));
		
		// ensure we really have the right index
		if ((found_index = find_player_index_by_idnum(GET_IDNUM(vict)))) {
			remove_player_from_table(found_index);	// temporary remove
			
			if (found_index->name) {
				free(found_index->name);
			}
			found_index->name = str_dup(GET_PC_NAME(vict));
			strtolower(found_index->name);
			
			// now update the rest of the data
			update_player_index(found_index, vict);
			
			// now re-add to index
			add_player_to_table(found_index);
		}
		
		// rename the save file
		get_filename(oldname, buf1, PLR_FILE);
		get_filename(GET_NAME(vict), buf2, PLR_FILE);
		rename(buf1, buf2);
		get_filename(oldname, buf1, DELAYED_FILE);
		get_filename(GET_NAME(vict), buf2, DELAYED_FILE);
		rename(buf1, buf2);
		
		SAVE_CHAR(vict);
		save_library_file_for_vnum(DB_BOOT_ACCT, GET_ACCOUNT(vict)->id);
		sprintf(output, "%s's name changed to %s.", oldname, GET_NAME(vict));
	}
	
	else if SET_CASE("scale") {
		// adjust here to log the right number
		value = MAX(GET_MIN_SCALE_LEVEL(vict), value);
		value = MIN(GET_MAX_SCALE_LEVEL(vict), value);
		scale_mob_to_level(vict, value);
		SET_BIT(MOB_FLAGS(vict), MOB_NO_RESCALE);
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


SHOW(show_components) {
	char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	obj_data *obj, *next_obj;
	bitvector_t flags;
	size_t size;
	int type;
	
	argument = any_one_word(argument, arg);	// component type
	skip_spaces(&argument);	// optional flags
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show components <type> [flags]\r\n");
		msg_to_char(ch, "See: HELP COMPONENT TYPES, HELP COMPONENT FLAGS\r\n");
	}
	else if ((type = search_block(arg, component_types, FALSE)) == NOTHING) {
		msg_to_char(ch, "Unknown component type '%s' (see HELP COMPONENT TYPES).\r\n", arg);
	}
	else {
		flags = *argument ? olc_process_flag(ch, argument, "component", "flags", component_flags, NOBITS) : NOBITS;
		
		// preamble
		size = snprintf(buf, sizeof(buf), "Components for %s:\r\n", component_string(type, flags));
		
		HASH_ITER(hh, object_table, obj, next_obj) {
			if (size >= sizeof(buf)) {
				break;
			}
			if (GET_OBJ_CMP_TYPE(obj) != type) {
				continue;
			}
			if (flags && (flags & GET_OBJ_CMP_FLAGS(obj)) != flags) {
				continue;
			}
			
			if (GET_OBJ_CMP_FLAGS(obj)) {
				prettier_sprintbit(GET_OBJ_CMP_FLAGS(obj), component_flags, part);
			}
			else {
				*part = '\0';
			}
			size += snprintf(buf + size, sizeof(buf) - size, "[%5d] %s%s%s%s\r\n", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj), *part ? " (" : "", part, *part ? ")" : "");
		}
		
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}
}


// show data system
SHOW(show_data) {
	extern struct stored_data *data_table;
	extern struct stored_data_type stored_data_info[];
	
	char output[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH];
	struct stored_data *data, *next_data;
	size_t size;
	
	size = snprintf(output, sizeof(output), "Stored data:\r\n");
	
	HASH_ITER(hh, data_table, data, next_data) {
		// DATYPE_x:
		switch (data->keytype) {
			case DATYPE_INT: {
				snprintf(line, sizeof(line), " %s: %d\r\n", stored_data_info[data->key].name, data_get_int(data->key));
				break;
			}
			case DATYPE_LONG: {
				snprintf(line, sizeof(line), " %s: %ld\r\n", stored_data_info[data->key].name, data_get_long(data->key));
				break;
			}
			case DATYPE_DOUBLE: {
				snprintf(line, sizeof(line), " %s: %f\r\n", stored_data_info[data->key].name, data_get_double(data->key));
				break;
			}
			default: {
				snprintf(line, sizeof(line), " %s: UNKNOWN\r\n", stored_data_info[data->key].name);
				break;
			}
		}
		
		if ((size += strlen(line)) < sizeof(output)) {
			strcat(output, line);
		}
		else {
			break;
		}
	}
	
	if (ch->desc) {
		page_string(ch->desc, output, TRUE);
	}
}


SHOW(show_factions) {
	char name[MAX_INPUT_LENGTH], *arg2, buf[MAX_STRING_LENGTH];
	struct player_faction_data *pfd, *next_pfd;
	faction_data *fct;
	bool file = FALSE;
	char_data *vict;
	int count = 0;
	size_t size;
	int idx;
	
	arg2 = any_one_arg(argument, name);
	skip_spaces(&arg2);
	
	if (!(vict = find_or_load_player(name, &file))) {
		msg_to_char(ch, "No player by that name.\r\n");
	}
	else if (GET_ACCESS_LEVEL(vict) > GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else {
		check_delayed_load(vict);
		size = snprintf(buf, sizeof(buf), "%s's factions:\r\n", GET_NAME(vict));
		HASH_ITER(hh, GET_FACTIONS(vict), pfd, next_pfd) {
			if (size + 10 >= sizeof(buf)) {
				break;	// out of room
			}
			if (!(fct = find_faction_by_vnum(pfd->vnum))) {
				continue;
			}
			if (*arg2 && !multi_isname(arg2, FCT_NAME(fct))) {
				continue;	// filter
			}
			
			++count;
			idx = rep_const_to_index(pfd->rep);
			size += snprintf(buf + size, sizeof(buf) - size, "[%5d] %s %s(%s / %d)\t0\r\n", pfd->vnum, FCT_NAME(fct), reputation_levels[idx].color, reputation_levels[idx].name, pfd->value);
		}
		
		if (!count) {
			strcat(buf, " none\r\n");
		}
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}
	
	if (file) {
		free_char(vict);
	}
}


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
	struct empire_storage_data *store, *next_store;
	struct empire_island *eisle, *next_eisle;
	char arg[MAX_INPUT_LENGTH];
	struct island_info *isle;
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
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), eisle, next_eisle) {
			cur = find_or_make_show_island(eisle->island, &list);
			
			HASH_ITER(hh, eisle->store, store, next_store) {
				SAFE_ADD(cur->count, store->amount, 0, INT_MAX, FALSE);
			}
		}
		for (uniq = EMPIRE_UNIQUE_STORAGE(emp); uniq; uniq = uniq->next) {
			if (!cur || cur->island != uniq->island) {
				cur = find_or_make_show_island(uniq->island, &list);
			}
			SAFE_ADD(cur->count, uniq->amount, 0, INT_MAX, FALSE);
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
			
			isle = get_island(cur->island, TRUE);
			msg_to_char(ch, "%2d. %s: %d items\r\n", cur->island, get_island_name_for(isle->id, ch), cur->count);
			// pull it out of the list to prevent unlimited iteration
			REMOVE_FROM_LIST(cur, list, next);
			free(cur);
		}
	}
}


SHOW(show_piles) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	room_data *room, *next_room;
	int count, max = 100;
	obj_data *obj, *sub;
	vehicle_data *veh;
	size_t size;
	bool any;
	
	if (*argument && isdigit(*argument)) {
		max = atoi(argument);
	}
	
	size = snprintf(buf, sizeof(buf), "Piles of %d item%s or more:\r\n", max, PLURAL(max));
	
	any = FALSE;
	HASH_ITER(hh, world_table, room, next_room) {
		count = 0;
		LL_FOREACH2(ROOM_CONTENTS(room), obj, next_content) {
			++count;
			
			LL_FOREACH2(obj->contains, sub, next_content) {
				++count;
			}
		}
		LL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
			LL_FOREACH2(VEH_CONTAINS(veh), sub, next_content) {
				++count;
			}
		}
		
		if (count >= max) {
			snprintf(line, sizeof(line), "[%d] %s: %d item%s\r\n", GET_ROOM_VNUM(room), get_room_name(room, FALSE), count, PLURAL(count));
			any = TRUE;
			
			if (size + strlen(line) + 18 < sizeof(buf)) {
				strcat(buf, line);
				size += strlen(line);
			}
			else {
				size += snprintf(buf + size, sizeof(buf) - size, "*** OVERFLOW ***\r\n");
				break;
			}
		}
	}
	
	if (!any) {
		strcat(buf, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


SHOW(show_player) {
	char birth[80], lastlog[80];
	double days_played, avg_min_per_day;
	char_data *plr = NULL;
	bool file = FALSE;
	
	if (!*argument) {
		send_to_char("A name would help.\r\n", ch);
		return;
	}
	
	if (!(plr = find_or_load_player(argument, &file))) {
		send_to_char("There is no such player.\r\n", ch);
		return;
	}
	sprintf(buf, "Player: %-12s (%s) [%d]\r\n", GET_PC_NAME(plr), genders[(int) GET_REAL_SEX(plr)], GET_ACCESS_LEVEL(plr));
	strcpy(birth, ctime(&plr->player.time.birth));
	strcpy(lastlog, ctime(&plr->prev_logon));
	// Www Mmm dd hh:mm:ss yyyy
	sprintf(buf + strlen(buf), "Started: %-16.16s %4.4s   Last: %-16.16s %4.4s\r\n", birth, birth+20, lastlog, lastlog+20);
	
	if (GET_ACCESS_LEVEL(plr) <= GET_ACCESS_LEVEL(ch) && GET_ACCESS_LEVEL(ch) >= LVL_TO_SEE_ACCOUNTS) {
		sprintf(buf + strlen(buf), "Creation host: %s\r\n", NULLSAFE(GET_CREATION_HOST(plr)));
	}
	
	days_played = (double)(time(0) - plr->player.time.birth) / SECS_PER_REAL_DAY;
	avg_min_per_day = (((double) plr->player.time.played / SECS_PER_REAL_HOUR) / days_played) * SECS_PER_REAL_MIN;
	
	sprintf(buf + strlen(buf), "Played: %3dh %2dm (%d minutes per day)\r\n", (int) (plr->player.time.played / SECS_PER_REAL_HOUR), (int) (plr->player.time.played % SECS_PER_REAL_HOUR) / SECS_PER_REAL_MIN, (int)avg_min_per_day);
	
	send_to_char(buf, ch);
	
	if (file) {
		free_char(plr);
	}
}


SHOW(show_progress) {
	extern progress_data *find_progress_goal_by_name(char *name);
	
	int total = 0, active = 0, active_completed = 0, total_completed = 0;
	empire_data *emp, *next_emp;
	progress_data *prg;
	bool t_o;
	
	if (!*argument) {
		msg_to_char(ch, "Show completion of which progression goal?\r\n");
	}
	else if ((isdigit(*argument) && !(prg = real_progress(atoi(argument)))) || (!isdigit(*argument) && !(prg = find_progress_goal_by_name(argument)))) {
		msg_to_char(ch, "Unknown goal '%s'.\r\n", argument);
	}
	else {
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if (EMPIRE_IMM_ONLY(emp)) {
				continue;	// safe to skip these
			}
			
			++total;
			
			if (!(t_o = EMPIRE_IS_TIMED_OUT(emp))) {
				++active;
			}
			
			if (empire_has_completed_goal(emp, PRG_VNUM(prg))) {
				++total_completed;
				if (!t_o) {
					++active_completed;
				}
			}
		}
		
		if (total == 0) {
			msg_to_char(ch, "No player empires found; nobody has completed that goal.\r\n");
		}
		else {
			msg_to_char(ch, "%d completed that goal out of %d total empire%s (%d%%).\r\n", total_completed, total, PLURAL(total), (total_completed * 100 / total));
		}
		if (active == 0) {
			msg_to_char(ch, "No active empires found.\r\n");
		}
		else {
			msg_to_char(ch, "%d active empire%s completed that goal out of %d total active (%d%%).\r\n", active_completed, PLURAL(active_completed), active, (active_completed * 100 / active));
		}
	}
}


SHOW(show_progression) {
	int goals[NUM_PROGRESS_TYPES], rewards[NUM_PROGRESS_TYPES], cost[NUM_PROGRESS_TYPES], value[NUM_PROGRESS_TYPES];
	progress_data *prg, *next_prg;
	int iter, tot_v, tot_c;
	
	tot_v = tot_c = 0;
	for (iter = 0; iter < NUM_PROGRESS_TYPES; ++iter) {
		// init
		goals[iter] = rewards[iter] = cost[iter] = value[iter] = 0;
	}
	
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT)) {
			continue;
		}
		
		if (PRG_FLAGGED(prg, PRG_PURCHASABLE)) {
			++rewards[PRG_TYPE(prg)];
		}
		else {
			++goals[PRG_TYPE(prg)];
		}
		
		cost[PRG_TYPE(prg)] += PRG_COST(prg);
		tot_c += PRG_COST(prg);
		value[PRG_TYPE(prg)] += PRG_VALUE(prg);
		tot_v += PRG_VALUE(prg);
	}
	
	msg_to_char(ch, "Stats on active progression entries:\r\n");
	for (iter = 0; iter < NUM_PROGRESS_TYPES; ++iter) {
		msg_to_char(ch, "%s: %d goal%s (%d point%s), %d reward%s (%d total cost)\r\n", progress_types[iter], goals[iter], PLURAL(goals[iter]), value[iter], PLURAL(value[iter]), rewards[iter], PLURAL(rewards[iter]), cost[iter]);
	}
	msg_to_char(ch, "Total: %d point%s, %d total cost\r\n", tot_v, PLURAL(tot_v), tot_c);
}


SHOW(show_quests) {
	void count_quest_tasks(struct req_data *list, int *complete, int *total);
	void show_quest_tracker(char_data *ch, struct player_quest *pq);
	
	char name[MAX_INPUT_LENGTH], *arg2, buf[MAX_STRING_LENGTH], when[256];
	struct player_completed_quest *pcq, *next_pcq;
	bool file = FALSE, found = FALSE;
	struct player_quest *pq;
	int count, total, diff;
	quest_data *qst;
	char_data *vict;
	size_t size;
	
	arg2 = any_one_arg(argument, name);
	skip_spaces(&arg2);
	
	if (!(vict = find_or_load_player(name, &file))) {
		msg_to_char(ch, "No player by that name.\r\n");
	}
	else if (GET_ACCESS_LEVEL(vict) > GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!*arg2 || is_abbrev(arg2, "active")) {
		// active quest list
		check_delayed_load(vict);
		if (IS_NPC(vict) || !GET_QUESTS(vict)) {
			msg_to_char(ch, "%s is not on any quests.\r\n", GET_NAME(vict));
			if (vict && file) {
				file = FALSE;
				free_char(vict);
			}
			return;
		}
		
		size = snprintf(buf, sizeof(buf), "%s's quests (%d/%d dailies):\r\n", GET_NAME(vict), GET_DAILY_QUESTS(vict), config_get_int("dailies_per_day"));
		LL_FOREACH(GET_QUESTS(vict), pq) {
			count_quest_tasks(pq->tracker, &count, &total);
			size += snprintf(buf + size, sizeof(buf) - size, "[%5d] %s (%d/%d tasks)\r\n", pq->vnum, get_quest_name_by_proto(pq->vnum), count, total);
		}
	
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}	// end "active"
	else if (is_abbrev(arg2, "completed")) {
		// completed quest list
		check_delayed_load(vict);
		if (IS_NPC(vict) || !GET_COMPLETED_QUESTS(vict)) {
			msg_to_char(ch, "%s has not completed any quests.\r\n", GET_NAME(vict));
			if (vict && file) {
				file = FALSE;
				free_char(vict);
			}
			return;
		}
		
		size = snprintf(buf, sizeof(buf), "%s's completed quests:\r\n", GET_NAME(vict));
		HASH_ITER(hh, GET_COMPLETED_QUESTS(vict), pcq, next_pcq) {
			if (size >= sizeof(buf)) {
				break;
			}
			
			if (time(0) - pcq->last_completed < SECS_PER_REAL_DAY) {
				diff = (time(0) - pcq->last_completed) / SECS_PER_REAL_HOUR;
				snprintf(when, sizeof(when), "(%d hour%s ago)", diff, PLURAL(diff));
			}
			else {
				diff = (time(0) - pcq->last_completed) / SECS_PER_REAL_DAY;
				snprintf(when, sizeof(when), "(%d day%s ago)", diff, PLURAL(diff));
			}
			
			size += snprintf(buf + size, sizeof(buf) - size, "[%5d] %s %s\r\n", pcq->vnum, get_quest_name_by_proto(pcq->vnum), when);
		}
	
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}
	else {
		// show one active quest's tracker
		check_delayed_load(vict);
		LL_FOREACH(GET_QUESTS(vict), pq) {
			if (!(qst = quest_proto(pq->vnum))) {
				continue;
			}
			
			if (multi_isname(arg2, QUEST_NAME(qst))) {
				msg_to_char(ch, "%s ", QUEST_NAME(qst));	// followed by "Quest Tracker:"
				show_quest_tracker(ch, pq);
				found = TRUE;
				break;	// show just one
			}
		}
		
		if (!found) {
			msg_to_char(ch, "%s is not on a quest called '%s'.\r\n", GET_NAME(vict), arg2);
		}
	}
	
	if (file) {
		free_char(vict);
	}
}


SHOW(show_rent) {
	void Crash_listrent(char_data *ch, char *name);
	
	if (!*argument) {
		send_to_char("A name would help.\r\n", ch);
		return;
	}

	Crash_listrent(ch, argument);
}


SHOW(show_resource) {
	obj_data *obj_iter, *next_obj, *proto = NULL;
	struct empire_island *eisle, *next_eisle;
	struct empire_storage_data *store;
	struct shipping_data *shipd;
	empire_data *emp, *next_emp;
	long long med_amt = 0;
	int median, pos;
	any_vnum vnum;
	
	// tracker data: uses long longs because empires can store max-int per island
	int total_emps = 0, active_emps = 0, emps_storing = 0, active_storing = 0;
	long long amt, total = 0, active_total = 0;	// track both total-total and active empires
	empire_data *highest_emp = NULL;	// empire with the most
	long long highest_amt = 0;	// how much they have
	
	// data storage for medians
	struct show_res_t *el, *next_el, *list = NULL;
	
	// attempt to figure out which resource
	if (!*argument) {
		msg_to_char(ch, "Usage: show resource <vnum | name>\r\n");
		return;
	}
	else if (isdigit(*argument)) {
		proto = obj_proto(atoi(argument));
		// checked later
	}
	else {	// look up by name
		HASH_ITER(hh, object_table, obj_iter, next_obj) {
			if (obj_iter->storage && multi_isname(argument, GET_OBJ_KEYWORDS(obj_iter))) {
				proto = obj_iter;
				break;
			}
		}
	}
	
	// verify
	if (!proto) {
		msg_to_char(ch, "Unknown storable object '%s'.\r\n", argument);
		return;
	}
	if (!proto->storage) {
		msg_to_char(ch, "You can only use 'show resource' on storable objects.\r\n");
		return;
	}
	vnum = GET_OBJ_VNUM(proto);
	
	// ok now build the data
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_IMM_ONLY(emp)) {
			continue;	// skip imms
		}
		
		amt = 0;
		
		// scan islands
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), eisle, next_eisle) {
			HASH_FIND_INT(eisle->store, &vnum, store);
			if (store) {
				SAFE_ADD(amt, store->amount, 0, LLONG_MAX, FALSE);
			}
		}
		// scan shipping
		LL_FOREACH(EMPIRE_SHIPPING_LIST(emp), shipd) {
			if (shipd->vnum == vnum) {
				SAFE_ADD(amt, shipd->amount, 0, LLONG_MAX, FALSE);
			}
		}
		
		// count it
		++total_emps;
		SAFE_ADD(total, amt, 0, LLONG_MAX, FALSE);
		
		if (amt > 0) {
			++emps_storing;
		}
		
		// active-only
		if (!EMPIRE_IS_TIMED_OUT(emp)) {
			++active_emps;
			SAFE_ADD(active_total, amt, 0, LLONG_MAX, FALSE);
			
			CREATE(el, struct show_res_t, 1);
			el->amount = amt;
			LL_INSERT_INORDER(list, el, compare_show_res);
			
			if (amt > highest_amt) {
				highest_emp = emp;
				highest_amt = amt;
			}
			
			if (amt > 0) {
				++active_storing;
			}
		}
	}
	
	// determine medians and free list
	median = active_emps / 2 + 1;
	median = MIN(active_emps, median);
	pos = 0;
	LL_FOREACH_SAFE(list, el, next_el) {
		if (pos++ == median) {
			med_amt = el->amount;
		}
		free(el);
	}
	
	// and output
	msg_to_char(ch, "Resource storage analysis for [%d] %s:\r\n", GET_OBJ_VNUM(proto), GET_OBJ_SHORT_DESC(proto));
	msg_to_char(ch, "%d active empire%s: %lld stored, %lld mean, %lld median, %d empires have any\r\n", active_emps, PLURAL(active_emps), active_total, (active_total / MAX(1, active_emps)), med_amt, active_storing);
	if (highest_emp) {
		msg_to_char(ch, "Highest active empire: %s (%lld stored)\r\n", EMPIRE_NAME(highest_emp), highest_amt);
	}
	msg_to_char(ch, "%d total empire%s: %lld stored, %lld mean, %d empires have any\r\n", total_emps, PLURAL(total_emps), total, (total / MAX(1, total_emps)), emps_storing);
}


SHOW(show_stats) {
	void update_account_stats();
	extern int buf_switches, buf_largecount, buf_overflows, top_of_helpt;
	extern int total_accounts, active_accounts, active_accounts_week;
	extern struct help_index_element *help_table;
	
	int num_active_empires = 0, num_objs = 0, num_mobs = 0, num_vehs = 0, num_players = 0, num_descs = 0, menu_count = 0;
	int num_trigs = 0, num_goals = 0, num_rewards = 0, num_mort_helps = 0, num_imm_helps = 0;
	progress_data *prg, *next_prg;
	empire_data *emp, *next_emp;
	descriptor_data *desc;
	vehicle_data *veh;
	char_data *vict;
	trig_data *trig;
	obj_data *obj;
	int iter;
	
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
	
	// other counts
	LL_COUNT(object_list, obj, num_objs);
	LL_COUNT(vehicle_list, veh, num_vehs);
	LL_COUNT2(trigger_list, trig, num_trigs, next_in_world);

	// count active empires
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_MEMBERS(emp) > 0) {
			++num_active_empires;
		}
	}
	
	// count goals
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT)) {
			continue;
		}
		
		if (PRG_FLAGGED(prg, PRG_PURCHASABLE)) {
			++num_rewards;
		}
		else {
			++num_goals;
		}
	}
	
	// count helps
	for (iter = 0; iter <= top_of_helpt; ++iter) {
		if (help_table[iter].duplicate) {
			continue;
		}
		
		if (help_table[iter].level > LVL_MORTAL) {
			++num_imm_helps;
		}
		else {
			++num_mort_helps;
		}
	}
	
	update_account_stats();

	msg_to_char(ch, "Current stats:\r\n");
	msg_to_char(ch, "  %6d players in game  %6d connected\r\n", num_players, num_descs);
	msg_to_char(ch, "  %6d registered       %6d at menus\r\n", HASH_CNT(idnum_hh, player_table_by_idnum), menu_count);
	msg_to_char(ch, "  %6d player accounts  %6d active accounts\r\n", total_accounts, active_accounts);
	msg_to_char(ch, "  %6d accounts logged in this week\r\n", active_accounts_week);
	msg_to_char(ch, "  %6d empires          %6d active\r\n", HASH_COUNT(empire_table), num_active_empires);
	msg_to_char(ch, "  %6d mobiles          %6d prototypes\r\n", num_mobs, HASH_COUNT(mobile_table));
	msg_to_char(ch, "  %6d objects          %6d prototypes\r\n", num_objs, HASH_COUNT(object_table));
	msg_to_char(ch, "  %6d vehicles         %6d prototypes\r\n", num_vehs, HASH_COUNT(vehicle_table));
	msg_to_char(ch, "  %6d adventures       %6d total rooms\r\n", HASH_COUNT(adventure_table), HASH_COUNT(world_table));
	msg_to_char(ch, "  %6d buildings        %6d room templates\r\n", HASH_COUNT(building_table), HASH_COUNT(room_template_table));
	msg_to_char(ch, "  %6d sectors          %6d crops\r\n", HASH_COUNT(sector_table), HASH_COUNT(crop_table));
	msg_to_char(ch, "  %6d triggers         %6d prototypes\r\n", num_trigs, HASH_COUNT(trigger_table));
	msg_to_char(ch, "  %6d craft recipes    %6d quests\r\n", HASH_COUNT(craft_table), HASH_COUNT(quest_table));
	msg_to_char(ch, "  %6d archetypes       %6d books\r\n", HASH_COUNT(archetype_table), HASH_COUNT(book_table));
	msg_to_char(ch, "  %6d classes          %6d skills\r\n", HASH_COUNT(class_table), HASH_COUNT(skill_table));
	msg_to_char(ch, "  %6d abilities        %6d factions\r\n", HASH_COUNT(ability_table), HASH_COUNT(faction_table));
	msg_to_char(ch, "  %6d globals          %6d morphs\r\n", HASH_COUNT(globals_table), HASH_COUNT(morph_table));
	msg_to_char(ch, "  %6d events           \r\n", HASH_COUNT(event_table));
	msg_to_char(ch, "  %6d socials          %6d generics\r\n", HASH_COUNT(social_table), HASH_COUNT(generic_table));
	msg_to_char(ch, "  %6d progress goals   %6d progress rewards\r\n", num_goals, num_rewards);
	msg_to_char(ch, "  %6d shops\r\n", HASH_COUNT(shop_table));
	msg_to_char(ch, "  %6d mortal helpfiles %6d immortal helpfiles\r\n", num_mort_helps, num_imm_helps);
	msg_to_char(ch, "  %6d large bufs       %6d buf switches\r\n", buf_largecount, buf_switches);
	msg_to_char(ch, "  %6d overflows\r\n", buf_overflows);
}


SHOW(show_site) {
	char buf[MAX_STRING_LENGTH], line[256];
	player_index_data *index, *next_index;
	size_t size;
	int k;
	
	if (!*argument) {
		msg_to_char(ch, "Locate players from what site?\r\n");
		return;
	}
	
	*buf = '\0';
	size = 0;
	k = 0;
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
		if (str_str(index->last_host, argument)) {
			snprintf(line, sizeof(line), " %-15.15s %s", index->name, ((++k % 3)) ? "|" : "\r\n");
			line[1] = UPPER(line[1]);	// cap name
			
			if (size + strlen(line) < sizeof(buf)) {
				size += snprintf(buf + size, sizeof(buf) - size, "%s", line);
			}
		}
	}
	msg_to_char(ch, "Players from site %s:\r\n", argument);
	if (*buf) {
		send_to_char(buf, ch);
		
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
	extern char *ability_color(char_data *ch, ability_data *abil);
	extern int get_ability_points_available_for_char(char_data *ch, any_vnum skill);
	
	struct player_ability_data *plab, *next_plab;
	struct player_skill_data *plsk, *next_plsk;
	ability_data *abil;
	skill_data *skill;
	char_data *vict;
	bool found, is_file = FALSE;
	int set;
	
	argument = one_argument(argument, arg);
	skip_spaces(&argument);
	
	if (!(vict = find_or_load_player(arg, &is_file))) {
		send_config_msg(ch, "no_person");
		return;
	}
		
	if (REAL_NPC(vict)) {
		msg_to_char(ch, "You can't show skills on an NPC.\r\n");
		if (is_file) {
			// unlikely to get here, but playing it safe
			free_char(vict);
		}
		return;
	}
	
	// detect "swap" arg
	set = GET_CURRENT_SKILL_SET(vict);
	if (*argument && is_abbrev(argument, "swap")) {
		// note: this ONLY supports 2 different sets
		set = (!set ? 1 : 0);
	}
	
	msg_to_char(ch, "Skills for %s:\r\n", PERS(vict, ch, TRUE));
	
	HASH_ITER(hh, GET_SKILL_HASH(vict), plsk, next_plsk) {
		skill = plsk->ptr;
		
		msg_to_char(ch, "&y%s&0 [%d, %.1f%%, %d%s]: ", SKILL_NAME(skill), get_skill_level(vict, SKILL_VNUM(skill)), get_skill_exp(vict, SKILL_VNUM(skill)), get_ability_points_available_for_char(vict, SKILL_VNUM(skill)), plsk->resets ? "*" : "");
		
		found = FALSE;
		HASH_ITER(hh, GET_ABILITY_HASH(vict), plab, next_plab) {
			abil = plab->ptr;
			
			if (!plab->purchased[set]) {
				continue;
			}
			if (ABIL_ASSIGNED_SKILL(abil) != skill) {
				continue;
			}

			msg_to_char(ch, "%s%s%s&0", (found ? ", " : ""), ability_color(vict, abil), ABIL_NAME(abil));
			found = TRUE;
		}
		
		msg_to_char(ch, "%s\r\n", (found ? "" : "none"));
	}
	
	msg_to_char(ch, "&yClass&0: &g");
	found = FALSE;
	HASH_ITER(hh, GET_ABILITY_HASH(vict), plab, next_plab) {
		abil = plab->ptr;
		
		// ALWAYS use current set for class abilities
		if (!plab->purchased[GET_CURRENT_SKILL_SET(vict)]) {
			continue;
		}
		if (ABIL_IS_PURCHASE(abil)) {
			continue;	// only looking for non-skill abilities
		}

		msg_to_char(ch, "%s%s", (found ? ", " : ""), ABIL_NAME(abil));
		found = TRUE;
	}
	msg_to_char(ch, "&0%s\r\n", (found ? "" : "none"));
	
	// available summary
	msg_to_char(ch, "Available daily bonus experience points: %d\r\n", GET_DAILY_BONUS_EXPERIENCE(vict));
	
	if (is_file) {
		free_char(vict);
	}
}


SHOW(show_buildings) {
	extern bld_data *get_building_by_name(char *name, bool room_only);
	extern int stats_get_building_count(bld_data *bdg);
	
	char buf[MAX_STRING_LENGTH * 2], line[256], part[256];
	struct sector_index_type *idx;
	bld_data *bld, *next_bld;
	int count, total, this;
	struct map_data *map;
	size_t size, l_size;
	sector_data *sect;
	room_data *room;
	bool any;
	
	// fresh numbers
	update_world_count();
	
	if (!*argument) {	// no-arg: show summary
		// output
		total = count = 0;
	
		HASH_ITER(hh, building_table, bld, next_bld) {
			if (IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM)) {
				continue;
			}
			
			this = stats_get_building_count(bld);
			strcpy(buf, GET_BLD_NAME(bld));
			msg_to_char(ch, " %6d %-20.20s %s", this, CAP(buf), !((++count)%2) ? "\r\n" : " ");
			total += this;
		}
		if (count % 2) {
			msg_to_char(ch, "\r\n");
		}
	
		msg_to_char(ch, " Total: %d\r\n", total);
	}
	// argument usage: show building <vnum | name>
	else if (!(isdigit(*argument) && (bld = building_proto(atoi(argument)))) && !(bld = get_building_by_name(argument, FALSE))) {
		msg_to_char(ch, "Unknown building '%s'.\r\n", argument);
	}
	else if (IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM)) {
		msg_to_char(ch, "This function only works on map buildings, not interior rooms.\r\n");
	}
	else if (!(sect = sector_proto(config_get_int("default_building_sect"))) || !(idx = find_sector_index(GET_SECT_VNUM(sect)))) {
		msg_to_char(ch, "Error looking up buildings: default sector not configured.\r\n");
	}
	else {
		size = snprintf(buf, sizeof(buf), "[%d] %s (%d in world):\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld), stats_get_building_count(bld));
		
		any = FALSE;
		LL_FOREACH2(idx->sect_rooms, map, next_in_sect) {
			// building rooms are always instantiated so it's safe to do a real_room without wasting RAM
			room = real_room(map->vnum);
			if (BUILDING_VNUM(room) != GET_BLD_VNUM(bld)) {
				continue;
			}
			
			// found
			if (ROOM_OWNER(room)) {
				snprintf(part, sizeof(part), " - %s%s\t0", EMPIRE_BANNER(ROOM_OWNER(room)), EMPIRE_ADJECTIVE(ROOM_OWNER(room)));
			}
			else {
				*part = '\0';
			}
			l_size = snprintf(line, sizeof(line), "(%*d, %*d) %s%s\r\n", X_PRECISION, MAP_X_COORD(map->vnum), Y_PRECISION, MAP_Y_COORD(map->vnum), get_room_name(room, FALSE), part);
			any = TRUE;
			
			if (size + l_size < sizeof(buf) + 40) {	// reserve a little extra space
				strcat(buf, line);
				size += l_size;
			}
			else {
				// hit the end, but we reserved space
				snprintf(buf + size, sizeof(buf) - size, "... and more\r\n");
				break;
			}
		}
		
		if (!any) {
			snprintf(buf + size, sizeof(buf) - size, " no matching tiles\r\n");
		}
		
		page_string(ch->desc, buf, TRUE);
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
		send_to_char(buf, ch);
	else
		msg_to_char(ch, "None.\r\n");
}


SHOW(show_crops) {
	extern crop_data *get_crop_by_name(char *name);
	
	char buf[MAX_STRING_LENGTH * 2], line[256], part[256];
	crop_data *crop, *next_crop;
	int count, total, this;
	struct map_data *map;
	size_t size, l_size;
	room_data *room;
	bool any;
	
	// fresh numbers
	update_world_count();
	
	if (!*argument) {	// no-arg: show summary
		// output
		total = count = 0;
	
		HASH_ITER(hh, crop_table, crop, next_crop) {
			this = stats_get_crop_count(crop);
			strcpy(buf, GET_CROP_NAME(crop));
			msg_to_char(ch, " %6d %-20.20s %s", this, CAP(buf), !((++count)%2) ? "\r\n" : " ");
			total += this;
		}
		if (count % 2) {
			msg_to_char(ch, "\r\n");
		}
	
		msg_to_char(ch, " Total: %d\r\n", total);
	}
	// argument usage: show building <vnum | name>
	else if (!(isdigit(*argument) && (crop = crop_proto(atoi(argument)))) && !(crop = get_crop_by_name(argument))) {
		msg_to_char(ch, "Unknown crop '%s'.\r\n", argument);
	}
	else {
		strcpy(part, GET_CROP_NAME(crop));
		size = snprintf(buf, sizeof(buf), "[%d] %s (%d in world):\r\n", GET_CROP_VNUM(crop), CAP(part), stats_get_crop_count(crop));
		
		any = FALSE;
		LL_FOREACH(land_map, map) {
			if (map->crop_type != crop) {
				continue;
			}
			
			// load room if possible (but not if it's not in RAM)
			room = real_real_room(map->vnum);
			
			// found
			if (room && ROOM_OWNER(room)) {
				snprintf(part, sizeof(part), " - %s%s\t0", EMPIRE_BANNER(ROOM_OWNER(room)), EMPIRE_ADJECTIVE(ROOM_OWNER(room)));
			}
			else {
				*part = '\0';
			}
			l_size = snprintf(line, sizeof(line), "(%*d, %*d) %s%s\r\n", X_PRECISION, MAP_X_COORD(map->vnum), Y_PRECISION, MAP_Y_COORD(map->vnum), room ? get_room_name(room, FALSE) : GET_CROP_TITLE(crop), part);
			any = TRUE;
			
			if (size + l_size < sizeof(buf) + 40) {	// reserve a little extra space
				strcat(buf, line);
				size += l_size;
			}
			else {
				// hit the end, but we reserved space
				snprintf(buf + size, sizeof(buf) - size, "... and more\r\n");
				break;
			}
		}
		
		if (!any) {
			snprintf(buf + size, sizeof(buf) - size, " no matching tiles\r\n");
		}
		
		page_string(ch->desc, buf, TRUE);
	}
}


SHOW(show_dailycycle) {
	char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	quest_data *qst, *next_qst;
	size_t size;
	int num;
	
	one_argument(argument, arg);
	
	if (!*arg || !isdigit(*arg)) {
		msg_to_char(ch, "Usage: show dailycycle <number>\r\n");
	}
	else if ((num = atoi(arg)) < 0) {
		msg_to_char(ch, "Invalid cycle number.\r\n");
	}
	else {
		size = snprintf(buf, sizeof(buf), "Daily quests with cycle id %d:\r\n", num);
		HASH_ITER(hh, quest_table, qst, next_qst) {
			if (!QUEST_FLAGGED(qst, QST_DAILY) || QUEST_DAILY_CYCLE(qst) != num) {
				continue;
			}
			
			size += snprintf(buf + size, sizeof(buf) - size, "[%5d] %s%s\r\n", QUEST_VNUM(qst), QUEST_NAME(qst), QUEST_DAILY_ACTIVE(qst) ? " (active)" : "");
		}
		
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}
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


SHOW(show_shops) {
	extern struct shop_temp_list *build_available_shop_list(char_data *ch);
	void free_shop_temp_list(struct shop_temp_list *list);

	struct shop_temp_list *stl, *shop_list = NULL;
	char buf[MAX_STRING_LENGTH];
	
	msg_to_char(ch, "Shops here:\r\n");
	
	shop_list = build_available_shop_list(ch);
	LL_FOREACH(shop_list, stl) {
		// determine shopkeeper
		if (stl->from_mob) {
			snprintf(buf, sizeof(buf), " (%s)", PERS(stl->from_mob, ch, FALSE));
		}
		else if (stl->from_obj) {
			snprintf(buf, sizeof(buf), " (%s)", GET_OBJ_SHORT_DESC(stl->from_obj));
		}
		else if (stl->from_veh) {
			snprintf(buf, sizeof(buf), " (%s)", VEH_SHORT_DESC(stl->from_veh));
		}
		else if (stl->from_room) {
			strcpy(buf, " (room)");
		}
		else {
			*buf = '\0';
		}
		
		msg_to_char(ch, "[%5d] %s%s%s\r\n", SHOP_VNUM(stl->shop), SHOP_NAME(stl->shop), buf, SHOP_FLAGGED(stl->shop, SHOP_IN_DEVELOPMENT) ? " (IN-DEV)" : "");
	}
	
	if (!shop_list) {
		msg_to_char(ch, " none\r\n");
	}
	
	free_shop_temp_list(shop_list);
}


SHOW(show_technology) {
	extern const char *player_tech_types[];
	
	struct player_tech *ptech;
	int last_tech, count = 0;
	char one[256], line[256];
	char_data *vict = NULL;
	bool is_file = FALSE;
	size_t lsize;
	
	if (!*argument) {
		msg_to_char(ch, "Show technology for which player?\r\n");
	}
	else if (!(vict = find_or_load_player(argument, &is_file))) {
		send_config_msg(ch, "no_person");
	}
	else {
		// techs (slightly complicated
		msg_to_char(ch, "Techs for %s:\r\n", GET_NAME(vict));
		
		last_tech = NOTHING;
		*line = '\0';
		lsize = 0;
		
		LL_FOREACH(GET_TECHS(vict), ptech) {
			if (ptech->id == last_tech) {
				continue;
			}
			
			snprintf(one, sizeof(one), "\t%c%s, ", (++count % 2) ? 'W' : 'w', player_tech_types[ptech->id]);
			
			if (color_strlen(one) + lsize >= 79) {
				// send line
				msg_to_char(ch, "%s\r\n", line);
				lsize = 0;
				*line = '\0';
			}
			
			strcat(line, one);
			lsize += color_strlen(one);
			last_tech = ptech->id;
		}
		
		if (*line) {
			msg_to_char(ch, "%s\r\n", line);
		}
		msg_to_char(ch, "\t0");
	}
	
	if (vict && is_file) {
		free_char(vict);
	}
}


SHOW(show_terrain) {
	extern sector_data *get_sect_by_name(char *name);
	
	char buf[MAX_STRING_LENGTH * 2], line[256], part[256];
	sector_data *sect, *next_sect;
	int count, total, this;
	struct map_data *map;
	size_t size, l_size;
	room_data *room;
	bool any;
	
	// fresh numbers
	update_world_count();
	
	if (!*argument) {	// no-arg: show summary
		// output
		total = count = 0;
	
		HASH_ITER(hh, sector_table, sect, next_sect) {
			this = stats_get_sector_count(sect);
			msg_to_char(ch, " %6d %-20.20s %s", this, GET_SECT_NAME(sect), !((++count)%2) ? "\r\n" : " ");
			total += this;
		}
	
		if (count % 2) {
			msg_to_char(ch, "\r\n");
		}
	
		msg_to_char(ch, " Total: %d\r\n", total);
	}
	// argument usage: show building <vnum | name>
	else if (!(isdigit(*argument) && (sect = sector_proto(atoi(argument)))) && !(sect = get_sect_by_name(argument))) {
		msg_to_char(ch, "Unknown sector '%s'.\r\n", argument);
	}
	else {
		strcpy(part, GET_SECT_NAME(sect));
		size = snprintf(buf, sizeof(buf), "[%d] %s (%d in world):\r\n", GET_SECT_VNUM(sect), CAP(part), stats_get_sector_count(sect));
		
		any = FALSE;
		LL_FOREACH(land_map, map) {
			if (map->sector_type != sect) {
				continue;
			}
			
			// load room if possible (but not if it's not in RAM)
			room = real_real_room(map->vnum);
			
			// found
			if (room && ROOM_OWNER(room)) {
				snprintf(part, sizeof(part), " - %s%s\t0", EMPIRE_BANNER(ROOM_OWNER(room)), EMPIRE_ADJECTIVE(ROOM_OWNER(room)));
			}
			else {
				*part = '\0';
			}
			l_size = snprintf(line, sizeof(line), "(%*d, %*d) %s%s\r\n", X_PRECISION, MAP_X_COORD(map->vnum), Y_PRECISION, MAP_Y_COORD(map->vnum), room ? get_room_name(room, FALSE) : GET_SECT_TITLE(sect), part);
			any = TRUE;
			
			if (size + l_size < sizeof(buf) + 40) {	// reserve a little extra space
				strcat(buf, line);
				size += l_size;
			}
			else {
				// hit the end, but we reserved space
				snprintf(buf + size, sizeof(buf) - size, "... and more\r\n");
				break;
			}
		}
		
		if (!any) {
			snprintf(buf + size, sizeof(buf) - size, " no matching tiles\r\n");
		}
		
		page_string(ch->desc, buf, TRUE);
	}
}


SHOW(show_unlearnable) {
	struct progress_perk *perk, *next_perk;
	craft_data *craft, *next_craft;
	progress_data *prg, *next_prg;
	obj_data *obj, *next_obj;
	bool any = FALSE, found;
	
	msg_to_char(ch, "Unlearnable recipes with the LEARNED flag:\r\n");
	
	HASH_ITER(hh, craft_table, craft, next_craft) {
		if (!CRAFT_FLAGGED(craft, CRAFT_LEARNED)) {
			continue;
		}
		
		found = FALSE;
		
		// try to find it in recipes
		HASH_ITER(hh, object_table, obj, next_obj) {
			if (IS_RECIPE(obj) && GET_RECIPE_VNUM(obj) == GET_CRAFT_VNUM(craft)) {
				found = TRUE;
				break;
			}
		}
		if (found) {	// was an item recipe
			continue;
		}
		
		// try to find it in progression
		HASH_ITER(hh, progress_table, prg, next_prg) {
			LL_FOREACH_SAFE(PRG_PERKS(prg), perk, next_perk) {
				if (perk->type == PRG_PERK_CRAFT && perk->value == GET_CRAFT_VNUM(craft)) {
					found = TRUE;
					break;
				}
			}
			if (found) {
				break;
			}
		}
		if (found) {	// was a progression reward
			continue;
		}
		
		// did we get this far?
		any = TRUE;
		msg_to_char(ch, "[%5d] %s (%s)\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft), GET_CRAFT_ABILITY(craft) == NO_ABIL ? "no ability" : get_ability_name_by_vnum(GET_CRAFT_ABILITY(craft)));
	}
	
	if (any) {
		msg_to_char(ch, "(remember, some of these may be added by scripts)\r\n");
	}
	else {
		msg_to_char(ch, " none\r\n");
	}
}


SHOW(show_uses) {
	extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);
	
	char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	craft_data *craft, *next_craft;
	quest_data *quest, *next_quest;
	augment_data *aug, *next_aug;
	vehicle_data *veh, *next_veh;
	social_data *soc, *next_soc;
	struct resource_data *res;
	bld_data *bld, *next_bld;
	bitvector_t flags;
	size_t size;
	int type;
	bool any;
	
	argument = any_one_word(argument, arg);	// component type
	skip_spaces(&argument);	// optional flags
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show uses <type> [flags]\r\n");
		msg_to_char(ch, "See: HELP COMPONENT TYPES, HELP COMPONENT FLAGS\r\n");
	}
	else if ((type = search_block(arg, component_types, FALSE)) == NOTHING) {
		msg_to_char(ch, "Unknown component type '%s' (see HELP COMPONENT TYPES).\r\n", arg);
	}
	else {
		flags = *argument ? olc_process_flag(ch, argument, "component", "flags", component_flags, NOBITS) : NOBITS;
		
		// preamble
		size = snprintf(buf, sizeof(buf), "Uses for %s:\r\n", component_string(type, flags));
		
		HASH_ITER(hh, augment_table, aug, next_aug) {
			if (size >= sizeof(buf)) {
				break;
			}
			
			LL_FOREACH(GET_AUG_RESOURCES(aug), res) {
				if (res->type != RES_COMPONENT || res->vnum != type) {
					continue;
				}
				if (flags && (res->misc & flags) != flags) {
					continue;
				}
				
				if (res->misc) {
					prettier_sprintbit(res->misc, component_flags, part);
				}
				else {
					*part = '\0';
				}
				size += snprintf(buf + size, sizeof(buf) - size, "AUG [%5d] %s%s%s%s\r\n", GET_AUG_VNUM(aug), GET_AUG_NAME(aug), *part ? " (" : "", part, *part ? ")" : "");
			}
		}
		
		HASH_ITER(hh, building_table, bld, next_bld) {
			if (size >= sizeof(buf)) {
				break;
			}
			
			LL_FOREACH(GET_BLD_YEARLY_MAINTENANCE(bld), res) {
				if (res->type != RES_COMPONENT || res->vnum != type) {
					continue;
				}
				if (flags && (res->misc & flags) != flags) {
					continue;
				}
				
				if (res->misc) {
					prettier_sprintbit(res->misc, component_flags, part);
				}
				else {
					*part = '\0';
				}
				size += snprintf(buf + size, sizeof(buf) - size, "BLD [%5d] %s%s%s%s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld), *part ? " (" : "", part, *part ? ")" : "");
			}
		}
		
		HASH_ITER(hh, craft_table, craft, next_craft) {
			if (size >= sizeof(buf)) {
				break;
			}
			
			LL_FOREACH(GET_CRAFT_RESOURCES(craft), res) {
				if (res->type != RES_COMPONENT || res->vnum != type) {
					continue;
				}
				if (flags && (res->misc & flags) != flags) {
					continue;
				}
				
				if (res->misc) {
					prettier_sprintbit(res->misc, component_flags, part);
				}
				else {
					*part = '\0';
				}
				size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s%s%s%s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft), *part ? " (" : "", part, *part ? ")" : "");
			}
		}
		
		HASH_ITER(hh, quest_table, quest, next_quest) {
			if (size >= sizeof(buf)) {
				break;
			}
			any = find_requirement_in_list(QUEST_TASKS(quest), REQ_GET_COMPONENT, type);
			any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_GET_COMPONENT, type);
		
			if (any) {
				size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
			}
		}
		
		HASH_ITER(hh, social_table, soc, next_soc) {
			if (size >= sizeof(buf)) {
				break;
			}
			any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_GET_COMPONENT, type);
		
			if (any) {
				size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
			}
		}
		
		HASH_ITER(hh, vehicle_table, veh, next_veh) {
			if (size >= sizeof(buf)) {
				break;
			}
			
			LL_FOREACH(VEH_YEARLY_MAINTENANCE(veh), res) {
				if (res->type != RES_COMPONENT || res->vnum != type) {
					continue;
				}
				if (flags && (res->misc & flags) != flags) {
					continue;
				}
				
				if (res->misc) {
					prettier_sprintbit(res->misc, component_flags, part);
				}
				else {
					*part = '\0';
				}
				size += snprintf(buf + size, sizeof(buf) - size, "VEH [%5d] %s%s%s%s\r\n", VEH_VNUM(veh), VEH_SHORT_DESC(veh), *part ? " (" : "", part, *part ? ")" : "");
			}
		}
		
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}
}


SHOW(show_account) {
	player_index_data *plr_index = NULL, *index, *next_index;
	bool file = FALSE, loaded_file = FALSE;
	char skills[MAX_STRING_LENGTH];
	char_data *plr = NULL, *loaded;
	int acc_id = NOTHING;
	time_t last_online = -1;	// -1 here will indicate no data, -2 will indicate online now
	
	#define ONLINE_NOW  -2
	
	if (!*argument) {
		msg_to_char(ch, "Usage: show account <player>\r\n");
		return;
	}
	
	// target by...
	if (isdigit(*argument)) {
		acc_id = atoi(argument);
	}
	else if (!(plr = find_or_load_player(argument, &file))) {
		send_to_char("There is no such player.\r\n", ch);
		return;
	}
	
	// look up index if applicable
	if (plr && !(plr_index = find_player_index_by_idnum(GET_IDNUM(plr)))) {
		msg_to_char(ch, "Unknown error: player not in index.\r\n");
		if (file) {
			free_char(plr);
		}
		return;
	}
	
	// look up id if needed
	if (acc_id == NOTHING && plr_index) {
		acc_id = plr_index->account_id;
	}
	
	// display:
	msg_to_char(ch, "Account characters:\r\n");
	
	HASH_ITER(name_hh, player_table_by_name, index, next_index) {
		if (index->account_id != acc_id && (!plr_index || strcmp(index->last_host, plr_index->last_host))) {
			continue;
		}
		if (!(loaded = find_or_load_player(index->name, &loaded_file))) {
			continue;
		}
		
		// skills name used by all 3
		get_player_skill_string(loaded, skills, TRUE);
		
		if (GET_ACCOUNT(loaded)->id == acc_id) {
			if (!loaded_file) {
				msg_to_char(ch, " &c[%d %s] %s (online)&0\r\n", GET_COMPUTED_LEVEL(loaded), skills, GET_PC_NAME(loaded));
				last_online = ONLINE_NOW;
			}
			else {
				// not playing but same account
				msg_to_char(ch, " [%d %s] %s\r\n", GET_LAST_KNOWN_LEVEL(loaded), skills, GET_PC_NAME(loaded));
				if (last_online != ONLINE_NOW) {
					last_online = MAX(last_online, loaded->prev_logon);
				}
			}
		}
		else {
			msg_to_char(ch, " &r[%d %s] %s (not on account)&0\r\n", loaded_file ? GET_LAST_KNOWN_LEVEL(loaded) : GET_COMPUTED_LEVEL(loaded), skills, GET_PC_NAME(loaded));
		}
		
		if (loaded_file) {
			free_char(loaded);
		}
	}
	
	if (last_online > 0) {
		msg_to_char(ch, " (last online: %-24.24s)\r\n", ctime(&last_online));
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_notes) {
	char buf[MAX_STRING_LENGTH];
	player_index_data *index = NULL;
	account_data *acct = NULL;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: show notes <player>\r\n");
		return;
	}
	
	// argument parsing
	if (isdigit(*argument)) {
		if (!(acct = find_account(atoi(argument)))) {
			msg_to_char(ch, "Unknown account '%s'.\r\n", argument);
			return;
		}
	}
	else {
		if (!(index = find_player_index_by_name(argument))) {
			msg_to_char(ch, "There is no such player.\r\n");
			return;
		}
		if (index->access_level >= GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "You can't show notes for players of that level.\r\n");
			return;
		}
		if (!(acct = find_account(index->account_id))) {
			msg_to_char(ch, "There are no notes for that player.\r\n");
			return;
		}
	}
	
	// final checks
	if (!acct) {	// in case somehow
		msg_to_char(ch, "Unknown account.\r\n");
	}
	else if (!acct->notes || !*acct->notes) {
		msg_to_char(ch, "There are no notes for that account.\r\n");
	}
	else {
		if (index) {
			strcpy(buf, index->fullname);
		}
		else {
			sprintf(buf, "account %d", acct->id);
		}
		msg_to_char(ch, "Admin notes for %s:\r\n%s", buf, acct->notes);
	}
}


SHOW(show_olc) {
	char buf[MAX_STRING_LENGTH];
	player_index_data *index, *next_index;
	any_vnum vnum = NOTHING;
	bool file, any = FALSE;
	char_data *pers;
	
	one_argument(argument, arg);
	if (*arg && (!isdigit(*arg) || (vnum = atoi(arg)) < 0)) {
		msg_to_char(ch, "Usage: show olc [optional vnum]\r\n");
		return;
	}
	
	// heading
	if (vnum != NOTHING) {
		msg_to_char(ch, "Players with OLC access for vnum %d:\r\n", vnum);
	}
	else {
		msg_to_char(ch, "Players with OLC access:\r\n");
	}
	
	// check all players
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
		if ((pers = find_or_load_player(index->name, &file))) {
			if (vnum != NOTHING ? ((GET_OLC_MIN_VNUM(pers) <= vnum && GET_OLC_MAX_VNUM(pers) >= vnum) || GET_ACCESS_LEVEL(pers) >= LVL_UNRESTRICTED_BUILDER || OLC_FLAGGED(pers, OLC_FLAG_ALL_VNUMS)) : (GET_ACCESS_LEVEL(pers) >= LVL_UNRESTRICTED_BUILDER || OLC_FLAGGED(pers, OLC_FLAG_ALL_VNUMS) || GET_OLC_MIN_VNUM(pers) > 0 || GET_OLC_MAX_VNUM(pers) > 0)) {
				sprintbit(GET_OLC_FLAGS(pers), olc_flag_bits, buf, TRUE);
				msg_to_char(ch, " %s [&c%d-%d&0] &g%s&0\r\n", GET_PC_NAME(pers), GET_OLC_MIN_VNUM(pers), GET_OLC_MAX_VNUM(pers), buf);
				any = TRUE;
			}
			
			if (file) {
				free_char(pers);
			}
		}
	}
	
	if (!any) {
		msg_to_char(ch, "none\r\n");
	}
}


SHOW(show_ammotypes) {
	obj_data *obj, *next_obj;
	int total;
	
	strcpy(buf, "You find the following ammo types:\r\n");
	
	total = 0;
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (IS_MISSILE_WEAPON(obj) || IS_AMMO(obj)) {
			sprintf(buf1, " %c: %s", 'A' + GET_OBJ_VAL(obj, IS_AMMO(obj) ? VAL_AMMO_TYPE : VAL_MISSILE_WEAPON_AMMO_TYPE), GET_OBJ_SHORT_DESC(obj));
			sprintf(buf + strlen(buf), "%-32.32s%s", buf1, ((total++ % 2) ? "\r\n" : ""));
		}
	}
	
	if (total == 0) {
		msg_to_char(ch, "You find no objects with ammo types.\r\n");
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
	player_index_data *index;
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
			if (GET_IGNORE_LIST(vict, iter) != 0 && (index = find_player_index_by_idnum(GET_IGNORE_LIST(vict, iter)))) {
				msg_to_char(ch, " %s\r\n", index->fullname);
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


SHOW(show_currency) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	struct player_currency *cur, *next_cur;
	char_data *plr = NULL;
	bool file = FALSE;
	size_t size;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show currency <player>\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else {
	
		coin_string(GET_PLAYER_COINS(plr), line);
		size = snprintf(buf, sizeof(buf), "%s has %s.\r\n", GET_NAME(plr), line);
	
		if (GET_CURRENCIES(plr)) {
			size += snprintf(buf + size, sizeof(buf) - size, "Currencies:\r\n");
		
			HASH_ITER(hh, GET_CURRENCIES(plr), cur, next_cur) {
				snprintf(line, sizeof(line), "%3d %s\r\n", cur->amount, get_generic_string_by_vnum(cur->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(cur->amount)));
			
				if (size + strlen(line) < sizeof(buf)) {
					strcat(buf, line);
					size += strlen(line);
				}
				else {
					break;
				}
			}
		}
	
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_produced) {
	extern int sort_empire_production_totals(struct empire_production_total *a, struct empire_production_total *b);
	
	char arg[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	struct empire_production_total *egt, *next_egt;
	empire_data *emp = NULL;
	obj_vnum vnum = NOTHING;
	size_t size, count;
	obj_data *obj;
	
	argument = any_one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show produced <empire>\r\n");
	}
	else if (!(emp = get_empire_by_name(arg))) {
		send_to_char("There is no such empire.\r\n", ch);
	}
	else {
		if (*argument) {
			size = snprintf(output, sizeof(output), "Produced items for matching '%s' for %s%s\t0:\r\n", argument, EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		}
		else {
			size = snprintf(output, sizeof(output), "Produced items for %s%s\t0:\r\n", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		}
		
		// check if argument is a vnum
		if (isdigit(*argument)) {
			vnum = atoi(argument);
		}
		
		count = 0;
		HASH_SORT(EMPIRE_PRODUCTION_TOTALS(emp), sort_empire_production_totals);
		HASH_ITER(hh, EMPIRE_PRODUCTION_TOTALS(emp), egt, next_egt) {
			if (!(obj = egt->proto)) {
				continue;	// no obj?
			}
			if (*argument && vnum != egt->vnum && !multi_isname(argument, GET_OBJ_KEYWORDS(obj))) {
				continue;	// searched
			}
		
			// show it
			if (egt->imported || egt->exported) {
				snprintf(line, sizeof(line), " [%5d] %s: %d (%d/%d)\r\n", GET_OBJ_VNUM(obj), skip_filler(GET_OBJ_SHORT_DESC(obj)), egt->amount, egt->imported, egt->exported);
			}
			else {
				snprintf(line, sizeof(line), " [%5d] %s: %d\r\n", GET_OBJ_VNUM(obj), skip_filler(GET_OBJ_SHORT_DESC(obj)), egt->amount);
			}
			
			if (size + strlen(line) < sizeof(output)) {
				strcat(output, line);
				size += strlen(line);
				++count;
			}
			else {
				if (size + 10 < sizeof(output)) {
					strcat(output, "OVERFLOW\r\n");
				}
				break;
			}
		}
	
		if (!count) {
			strcat(output, "  none\r\n");	// space reserved for this for sure
		}
	
		if (ch->desc) {
			page_string(ch->desc, output, TRUE);
		}
	}
}


SHOW(show_learned) {
	char arg[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	struct player_craft_data *pcd, *next_pcd;
	empire_data *emp = NULL;
	char_data *plr = NULL;
	size_t size, count;
	craft_data *craft;
	bool file = FALSE;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show learned <player | empire>\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file)) && !(emp = get_empire_by_name(arg))) {
		send_to_char("There is no such player or empire.\r\n", ch);
	}
	else {	// plr OR emp is guaranteed
		if (*argument) {
			size = snprintf(output, sizeof(output), "Learned recipes matching '%s' for %s:\r\n", argument, plr ? GET_NAME(plr) : EMPIRE_NAME(emp));
		}
		else {
			size = snprintf(output, sizeof(output), "Learned recipes for %s:\r\n", plr ? GET_NAME(plr) : EMPIRE_NAME(emp));
		}
		
		count = 0;
		HASH_ITER(hh, (plr ? GET_LEARNED_CRAFTS(plr) : EMPIRE_LEARNED_CRAFTS(emp)), pcd, next_pcd) {
			if (!(craft = craft_proto(pcd->vnum))) {
				continue;	// no craft?
			}
			if (CRAFT_FLAGGED(craft, CRAFT_IN_DEVELOPMENT)) {
				continue;	// in-dev
			}
			if (*argument && !multi_isname(argument, GET_CRAFT_NAME(craft))) {
				continue;	// searched
			}
		
			// show it
			snprintf(line, sizeof(line), " [%5d] %s (%s)\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft), craft_types[GET_CRAFT_TYPE(craft)]);
			if (size + strlen(line) < sizeof(output)) {
				strcat(output, line);
				size += strlen(line);
				++count;
			}
			else {
				if (size + 10 < sizeof(output)) {
					strcat(output, "OVERFLOW\r\n");
				}
				break;
			}
		}
	
		if (!count) {
			strcat(output, "  none\r\n");	// space reserved for this for sure
		}
	
		if (ch->desc) {
			page_string(ch->desc, output, TRUE);
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_minipets) {
	char arg[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	struct minipet_data *mini, *next_mini;
	char_data *mob, *plr = NULL;
	size_t size, count;
	bool file = FALSE;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show minipets <player>\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else {
		if (*argument) {
			size = snprintf(output, sizeof(output), "Mini-pets matching '%s' for %s:\r\n", argument, GET_NAME(plr));
		}
		else {
			size = snprintf(output, sizeof(output), "Mini-pets for %s:\r\n", GET_NAME(plr));
		}
		
		count = 0;
		HASH_ITER(hh, GET_MINIPETS(plr), mini, next_mini) {
			if (!(mob = mob_proto(mini->vnum))) {
				continue;	// no mob?
			}
			if (*argument && !multi_isname(argument, GET_PC_NAME(mob))) {
				continue;	// searched
			}
		
			// show it
			snprintf(line, sizeof(line), " [%5d] %s\r\n", GET_MOB_VNUM(mob), skip_filler(GET_SHORT_DESC(mob)));
			if (size + strlen(line) < sizeof(output)) {
				strcat(output, line);
				size += strlen(line);
				++count;
			}
			else {
				if (size + 10 < sizeof(output)) {
					strcat(output, "OVERFLOW\r\n");
				}
				break;
			}
		}
	
		if (!count) {
			strcat(output, "  none\r\n");	// space reserved for this for sure
		}
	
		if (ch->desc) {
			page_string(ch->desc, output, TRUE);
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_mounts) {
	char arg[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	struct mount_data *mount, *next_mount;
	char_data *mob, *plr = NULL;
	size_t size, count;
	bool file = FALSE;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show mounts <player>\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else {
		if (*argument) {
			size = snprintf(output, sizeof(output), "Mounts matching '%s' for %s:\r\n", argument, GET_NAME(plr));
		}
		else {
			size = snprintf(output, sizeof(output), "Mounts for %s:\r\n", GET_NAME(plr));
		}
		
		count = 0;
		HASH_ITER(hh, GET_MOUNT_LIST(plr), mount, next_mount) {
			if (!(mob = mob_proto(mount->vnum))) {
				continue;	// no mob?
			}
			if (*argument && !multi_isname(argument, GET_PC_NAME(mob))) {
				continue;	// searched
			}
		
			// show it
			snprintf(line, sizeof(line), " [%5d] %s\r\n", GET_MOB_VNUM(mob), skip_filler(GET_SHORT_DESC(mob)));
			if (size + strlen(line) < sizeof(output)) {
				strcat(output, line);
				size += strlen(line);
				++count;
			}
			else {
				if (size + 10 < sizeof(output)) {
					strcat(output, "OVERFLOW\r\n");
				}
				break;
			}
		}
	
		if (!count) {
			strcat(output, "  none\r\n");	// space reserved for this for sure
		}
	
		if (ch->desc) {
			page_string(ch->desc, output, TRUE);
		}
	}
	
	if (plr && file) {
		free_char(plr);
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
	
	HASH_ITER(hh, world_table, iter, next_iter) {
		if (ROOM_SECT_FLAGGED(iter, SECTF_START_LOCATION)) {
			sprintf(buf + strlen(buf), "%s (%d, %d)&0\r\n", get_room_name(iter, TRUE), X_COORD(iter), Y_COORD(iter));
		}
	}
	
	page_string(ch->desc, buf, TRUE);
}


SHOW(show_spawns) {
	char buf[MAX_STRING_LENGTH];
	struct spawn_info *sp;
	vehicle_data *veh, *next_veh;
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
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		LL_FOREACH(VEH_SPAWNS(veh), sp) {
			if (sp->vnum == vnum) {
				sprintbit(sp->flags, spawn_flags, buf2, TRUE);
				sprintf(buf1, "%s: %.2f%% %s\r\n", VEH_SHORT_DESC(veh), sp->percent, buf2);
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


SHOW(show_variables) {
	void find_uid_name(char *uid, char *name);
	
	char uname[MAX_INPUT_LENGTH];
	struct trig_var_data *tv;
	char_data *plr = NULL;
	bool file = FALSE;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: show variables <player>\r\n");
	}
	else if (!(plr = find_or_load_player(argument, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else {
		msg_to_char(ch, "Global Variables:\r\n");
		check_delayed_load(plr);
		
		if (plr->script && plr->script->global_vars) {
			/* currently, variable context for players is always 0, so it is */
			/* not displayed here. in the future, this might change */
			for (tv = plr->script->global_vars; tv; tv = tv->next) {
				if (*(tv->value) == UID_CHAR) {
					find_uid_name(tv->value, uname);
					msg_to_char(ch, " %10s:  [UID]: %s\r\n", tv->name, uname);
				}
				else {
					msg_to_char(ch, " %10s:  %s\r\n", tv->name, tv->value);
				}
			}
		}
	}
	
	if (plr && file) {
		free_char(plr);
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
	
	msg_to_char(ch, "Instance limit: [&c%d&0/&c%d&0 (&c%d&0)], Player limit: [&c%d&0], Reset time: [&c%s&0]\r\n", count_instances(adventure_proto(GET_ADV_VNUM(adv))), adjusted_instance_limit(adv), GET_ADV_MAX_INSTANCES(adv), GET_ADV_PLAYER_LIMIT(adv), lbuf);
	
	sprintbit(GET_ADV_FLAGS(adv), adventure_flags, lbuf, TRUE);
	msg_to_char(ch, "Flags: &g%s&0\r\n", lbuf);
	
	get_adventure_linking_display(GET_ADV_LINKING(adv), lbuf);
	msg_to_char(ch, "Linking rules:\r\n%s", lbuf);
	
	if (GET_ADV_SCRIPTS(adv)) {
		get_script_display(GET_ADV_SCRIPTS(adv), lbuf);
		msg_to_char(ch, "Scripts:\r\n%s", lbuf);
	}
}


/**
* Show the stats on a book.
*
* @param char_data *ch The player requesting stats.
* @param book_data *book The book to stat.
*/
void do_stat_book(char_data *ch, book_data *book) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	struct paragraph_data *para;
	player_index_data *index;
	size_t size = 0;
	int count, len, num;
	char *ptr, *txt;
	
	size += snprintf(buf + size, sizeof(buf) - size, "Book VNum: [\tc%d\t0], Author: \ty%s\t0 (\tc%d\t0)\r\n", book->vnum, (index = find_player_index_by_idnum(book->author)) ? index->fullname : "nobody", book->author);
	size += snprintf(buf + size, sizeof(buf) - size, "Title: %s\t0\r\n", book->title);
	size += snprintf(buf + size, sizeof(buf) - size, "Byline: %s\t0\r\n", book->byline);
	size += snprintf(buf + size, sizeof(buf) - size, "Item: [%s]\r\n", book->item_name);
	size += snprintf(buf + size, sizeof(buf) - size, "%s", book->item_description);	// desc has its own crlf
	
	// precompute number of paragraphs
	num = 0;
	for (para = book->paragraphs; para; para = para->next) {
		++num;
	}
	
	for (para = book->paragraphs, count = 1; para; para = para->next, ++count) {
		txt = para->text;
		skip_spaces(&txt);
		len = strlen(txt);
		len = MIN(len, 62);	// aiming for full page width then a ...
		snprintf(line, sizeof(line), "Paragraph %*d: %-*.*s...", (num >= 10 ? 2 : 1), count, len, len, txt);
		if ((ptr = strstr(line, "\r\n"))) {	// line ended early?
			sprintf(ptr, "...");	// overwrite the crlf
		}
		
		// too big?
		if (size + strlen(line) + 2 >= sizeof(buf)) {
			break;
		}
		
		size += snprintf(buf + size, sizeof(buf) - size, "%s\r\n", line);
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


/**
* Show a character stats on a particular building.
*
* @param char_data *ch The player requesting stats.
* @param bld_data *bdg The building to stat.
*/
void do_stat_building(char_data *ch, bld_data *bdg) {
	void get_bld_relations_display(struct bld_relation *list, char *save_buffer);
	extern const char *bld_flags[];
	extern const char *designate_flags[];
	
	char line[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	struct obj_storage_type *store;
	obj_data *obj, *next_obj;
	
	msg_to_char(ch, "Building VNum: [&c%d&0], Name: '&c%s&0'\r\n", GET_BLD_VNUM(bdg), GET_BLD_NAME(bdg));
	
	replace_question_color(NULLSAFE(GET_BLD_ICON(bdg)), "&0", lbuf);
	msg_to_char(ch, "Room Title: %s, Icon: %s&0 %s&0\r\n", GET_BLD_TITLE(bdg), lbuf, show_color_codes(NULLSAFE(GET_BLD_ICON(bdg))));
	
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
	
	if (GET_BLD_RELATIONS(bdg)) {
		get_bld_relations_display(GET_BLD_RELATIONS(bdg), lbuf);
		msg_to_char(ch, "Relations:\r\n%s", lbuf);
	}
	
	sprintbit(GET_BLD_FLAGS(bdg), bld_flags, buf, TRUE);
	msg_to_char(ch, "Building flags: &c%s&0\r\n", buf);
	
	sprintbit(GET_BLD_FUNCTIONS(bdg), function_flags, buf, TRUE);
	msg_to_char(ch, "Functions: &g%s&0\r\n", buf);
	
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
	
	if (GET_BLD_YEARLY_MAINTENANCE(bdg)) {
		get_resource_display(GET_BLD_YEARLY_MAINTENANCE(bdg), buf);
		msg_to_char(ch, "Yearly maintenance:\r\n%s", buf);
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
	
	get_script_display(GET_BLD_SCRIPTS(bdg), line);
	msg_to_char(ch, "Scripts:\r\n%s", line);
	
	show_spawn_summary_to_char(ch, GET_BLD_SPAWNS(bdg));
}


/* Sends ch information on the character or animal k */
void do_stat_character(char_data *ch, char_data *k) {
	extern double get_combat_speed(char_data *ch, int pos);
	extern int get_crafting_level(char_data *ch);
	extern int get_block_rating(char_data *ch, bool can_gain_skill);
	extern int total_bonus_healing(char_data *ch);
	extern int get_dodge_modifier(char_data *ch, char_data *attacker, bool can_gain_skill);
	extern int get_to_hit(char_data *ch, char_data *victim, bool off_hand, bool can_gain_skill);
	extern int move_gain(char_data *ch);
	void display_attributes(char_data *ch, char_data *to);

	extern const char *account_flags[];
	extern const char *class_role[];
	extern const char *damage_types[];
	extern const double hit_per_dex;
	extern const char *mob_custom_types[];
	extern const char *mob_move_types[];
	extern const char *player_bits[];
	extern const char *position_types[];
	extern const char *preference_bits[];
	extern const char *connected_types[];
	extern const int base_hit_chance;
	extern struct promo_code_list promo_codes[];

	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH], lbuf2[MAX_STRING_LENGTH], lbuf3[MAX_STRING_LENGTH];
	struct script_memory *mem;
	struct cooldown_data *cool;
	int count, i, i2, iter, diff, found = 0, val;
	obj_data *j;
	struct follow_type *fol;
	struct over_time_effect_type *dot;
	struct affected_type *aff;
	archetype_data *arch;
	
	bool is_proto = (IS_NPC(k) && k == mob_proto(GET_MOB_VNUM(k)));
	
	// ensure fully loaded
	check_delayed_load(k);

	sprinttype(GET_REAL_SEX(k), genders, buf);
	CAP(buf);
	sprintf(buf2, " %s '&y%s&0'  IDNum: [%5d], In room [%5d]\r\n", (!IS_NPC(k) ? "PC" : (!IS_MOB(k) ? "NPC" : "MOB")), GET_NAME(k), GET_IDNUM(k), IN_ROOM(k) ? GET_ROOM_VNUM(IN_ROOM(k)) : NOWHERE);
	send_to_char(strcat(buf, buf2), ch);
	
	if (!IS_NPC(k) && GET_ACCOUNT(k)) {
		if (GET_ACCESS_LEVEL(ch) >= LVL_TO_SEE_ACCOUNTS) {
			sprintbit(GET_ACCOUNT(k)->flags, account_flags, buf, TRUE);
			msg_to_char(ch, "Account: [%d], Flags: &g%s&0\r\n", GET_ACCOUNT(k)->id, buf);
		}
		else {	// low-level imms only see certain account flags
			sprintbit(GET_ACCOUNT(k)->flags & VISIBLE_ACCT_FLAGS, account_flags, buf, TRUE);
			msg_to_char(ch, "Account: &g%s&0\r\n", buf);
		}
	}
	
	if (IS_MOB(k)) {
		msg_to_char(ch, "Alias: &y%s&0, VNum: [&c%5d&0]\r\n", GET_PC_NAME(k), GET_MOB_VNUM(k));
		msg_to_char(ch, "L-Des: &y%s&0%s", (GET_LONG_DESC(k) ? GET_LONG_DESC(k) : "<None>\r\n"), NULLSAFE(GET_LOOK_DESC(k)));
	}

	if (IS_NPC(k)) {
		msg_to_char(ch, "Scaled level: [&c%d&0|&c%d&0-&c%d&0], Faction: [\tt%s\t0]\r\n", GET_CURRENT_SCALE_LEVEL(k), GET_MIN_SCALE_LEVEL(k), GET_MAX_SCALE_LEVEL(k), MOB_FACTION(k) ? FCT_NAME(MOB_FACTION(k)) : "none");
	}
	else {	// not NPC
		msg_to_char(ch, "Title: %s&0\r\n", (GET_TITLE(k) ? GET_TITLE(k) : "<None>"));
		
		if (GET_REFERRED_BY(k) && *GET_REFERRED_BY(k)) {
			msg_to_char(ch, "Referred by: %s\r\n", GET_REFERRED_BY(k));
		}
		if (GET_PROMO_ID(k) > 0) {
			msg_to_char(ch, "Promo code: %s\r\n", promo_codes[GET_PROMO_ID(k)].code);
		}

		get_player_skill_string(k, lbuf, TRUE);
		msg_to_char(ch, "Access Level: [&c%d&0], Class: [%s/&c%s&0], Skill Level: [&c%d&0], Gear Level: [&c%d&0], Total: [&c%d&0/&c%d&0]\r\n", GET_ACCESS_LEVEL(k), lbuf, class_role[(int) GET_CLASS_ROLE(k)], GET_SKILL_LEVEL(k), GET_GEAR_LEVEL(k), IN_ROOM(k) ? GET_COMPUTED_LEVEL(k) : GET_LAST_KNOWN_LEVEL(k), GET_HIGHEST_KNOWN_LEVEL(k));
		
		msg_to_char(ch, "Archetypes:");
		for (iter = 0, count = 0; iter < NUM_ARCHETYPE_TYPES; ++iter) {
			if ((arch = archetype_proto(CREATION_ARCHETYPE(k, iter)))) {
				msg_to_char(ch, "%s%s", (count++ > 0) ? ", " : " ", GET_ARCH_NAME(arch));
			}
		}
		msg_to_char(ch, "%s\r\n", count ? "" : " none");
		
		coin_string(GET_PLAYER_COINS(k), buf);
		msg_to_char(ch, "Coins: %s\r\n", buf);

		strcpy(buf1, (char *) asctime(localtime(&(k->player.time.birth))));
		strcpy(buf2, buf1 + 20);
		buf1[10] = '\0';	// DoW Mon Day
		buf2[4] = '\0';	// get only year

		msg_to_char(ch, "Created: [%s, %s], Played [%dh %dm], Age [%d]\r\n", buf1, buf2, k->player.time.played / SECS_PER_REAL_HOUR, ((k->player.time.played % SECS_PER_REAL_HOUR) / SECS_PER_REAL_MIN), age(k)->year);
		if (GET_ACCESS_LEVEL(k) <= GET_ACCESS_LEVEL(ch) && GET_ACCESS_LEVEL(ch) >= LVL_TO_SEE_ACCOUNTS) {
			msg_to_char(ch, "Created from host: [%s]\r\n", NULLSAFE(GET_CREATION_HOST(k)));
		}
		
		if (GET_ACCESS_LEVEL(k) >= LVL_BUILDER) {
			sprintbit(GET_OLC_FLAGS(k), olc_flag_bits, buf, TRUE);
			msg_to_char(ch, "OLC Vnums: [&c%d-%d&0], Flags: &g%s&0\r\n", GET_OLC_MIN_VNUM(k), GET_OLC_MAX_VNUM(k), buf);
		}
	}

	if (!IS_NPC(k) || GET_CURRENT_SCALE_LEVEL(k) > 0) {
		msg_to_char(ch, "Health: [&g%d&0/&g%d&0]  Move: [&g%d&0/&g%d&0]  Mana: [&g%d&0/&g%d&0]  Blood: [&g%d&0/&g%d&0]\r\n", GET_HEALTH(k), GET_MAX_HEALTH(k), GET_MOVE(k), GET_MAX_MOVE(k), GET_MANA(k), GET_MAX_MANA(k), GET_BLOOD(k), GET_MAX_BLOOD(k));

		display_attributes(k, ch);

		// dex is removed from dodge to make it easier to compare to caps
		val = get_dodge_modifier(k, NULL, FALSE) - (hit_per_dex * GET_DEXTERITY(k));;
		sprintf(lbuf, "Dodge  [%s%d&0]", HAPPY_COLOR(val, 0), val);
		
		val = get_block_rating(k, FALSE);
		sprintf(lbuf2, "Block  [%s%d&0]", HAPPY_COLOR(val, 0), val);
		
		sprintf(lbuf3, "Resist  [%d|%d]", GET_RESIST_PHYSICAL(k), GET_RESIST_MAGICAL(k));
		msg_to_char(ch, "  %-28.28s %-28.28s %-28.28s\r\n", lbuf, lbuf2, lbuf3);
	
		sprintf(lbuf, "Physical  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_PHYSICAL(k), 0), GET_BONUS_PHYSICAL(k));
		sprintf(lbuf2, "Magical  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_MAGICAL(k), 0), GET_BONUS_MAGICAL(k));
		sprintf(lbuf3, "Healing  [%s%+d&0]", HAPPY_COLOR(total_bonus_healing(k), 0), total_bonus_healing(k));
		msg_to_char(ch, "  %-28.28s %-28.28s %-28.28s\r\n", lbuf, lbuf2, lbuf3);

		// dex is removed from to-hit to make it easier to compare to caps
		val = get_to_hit(k, NULL, FALSE, FALSE) - (hit_per_dex * GET_DEXTERITY(k));;
		sprintf(lbuf, "To-hit  [%s%d&0]", HAPPY_COLOR(val, base_hit_chance), val);
		sprintf(lbuf2, "Speed  [&0%.2f&0]", get_combat_speed(k, WEAR_WIELD));
		sprintf(lbuf3, "Crafting  [%s%d&0]", HAPPY_COLOR(get_crafting_level(k), IS_NPC(k) ? get_approximate_level(k) : GET_SKILL_LEVEL(k)), get_crafting_level(k));
		msg_to_char(ch, "  %-28.28s %-28.28s %-28.28s\r\n", lbuf, lbuf2, lbuf3);
		
		if (IS_NPC(k)) {
			sprintf(lbuf, "Mob-dmg  [%d]", MOB_DAMAGE(k));
			sprintf(lbuf2, "Mob-hit  [%d]", MOB_TO_HIT(k));
			sprintf(lbuf3, "Mob-dodge  [%d]", MOB_TO_DODGE(k));
			msg_to_char(ch, "  %-24.24s %-24.24s %-24.24s\r\n", lbuf, lbuf2, lbuf3);
			
			msg_to_char(ch, "NPC Bare Hand Dam: %d\r\n", MOB_DAMAGE(k));
		}
	}

	sprinttype(GET_POS(k), position_types, buf2);
	sprintf(buf, "Pos: %s, Fighting: %s", buf2, (FIGHTING(k) ? GET_NAME(FIGHTING(k)) : "Nobody"));

	if (IS_NPC(k)) {
		sprintf(buf + strlen(buf), ", Attack: %s, Move: %s, Size: %s", attack_hit_info[MOB_ATTACK_TYPE(k)].name, mob_move_types[(int)MOB_MOVE_TYPE(k)], size_types[GET_SIZE(k)]);
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
		prettier_sprintbit(GET_GRANT_FLAGS(k), grant_bits, buf2);
		msg_to_char(ch, "GRANTS: &g%s&0\r\n", buf2);
		sprintbit(SYSLOG_FLAGS(k), syslog_types, buf2, TRUE);
		msg_to_char(ch, "SYSLOGS: &c%s&0\r\n", buf2);
	}

	if (!is_proto) {
		sprintf(buf, "Carried items: %d/%d; ", IS_CARRYING_N(k), CAN_CARRY_N(k));
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
	
	if (MOB_CUSTOM_MSGS(k)) {
		struct custom_message *mcm;
		
		msg_to_char(ch, "Custom messages:\r\n");
		LL_FOREACH(MOB_CUSTOM_MSGS(k), mcm) {
			msg_to_char(ch, " %s: %s\r\n", mob_custom_types[mcm->type], mcm->msg);
		}
	}

	if (!IS_NPC(k)) {
		msg_to_char(ch, "Hunger: %d, Thirst: %d, Drunk: %d\r\n", GET_COND(k, FULL), GET_COND(k, THIRST), GET_COND(k, DRUNK));
		msg_to_char(ch, "Recent deaths: %d\r\n", GET_RECENT_DEATH_COUNT(k));
	}
	
	if (IS_MORPHED(k)) {
		msg_to_char(ch, "Morphed into: %d - %s\r\n", MORPH_VNUM(GET_MORPH(k)), get_morph_desc(k, FALSE));
	}
	if (IS_DISGUISED(k)) {
		msg_to_char(ch, "Disguised as: %s\r\n", GET_DISGUISED_NAME(k));
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
				msg_to_char(ch, "%s&c%s&0 %d:%02d", (found ? ", ": ""), get_generic_name_by_vnum(cool->type), (diff / 60), (diff % 60));
				
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

			sprintf(buf, "TYPE: (%s) &c%s&0 ", lbuf, get_generic_name_by_vnum(aff->type));

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
		
		msg_to_char(ch, "TYPE: (%s) &r%s&0 %d %s damage (%d/%d)\r\n", lbuf, get_generic_name_by_vnum(dot->type), dot->damage * dot->stack, damage_types[dot->damage_type], dot->stack, dot->max_stack);
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
						msg_to_char(ch, "  %-20.20s%s\r\n", PERS(mc, mc, TRUE), mem->cmd);
					}
					else {
						msg_to_char(ch, "  %-20.20s <default>\r\n", PERS(mc, mc, TRUE));
					}
				}
				
				mem = mem->next;
			}
		}
	}
}


void do_stat_craft(char_data *ch, craft_data *craft) {
	extern const char *craft_flags[];
	
	ability_data *abil;
	bld_data *bld;
	int seconds;
	
	msg_to_char(ch, "Name: '&y%s&0', Vnum: [&g%d&0], Type: &c%s&0\r\n", GET_CRAFT_NAME(craft), GET_CRAFT_VNUM(craft), craft_types[GET_CRAFT_TYPE(craft)]);
	
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {
		bld = building_proto(GET_CRAFT_BUILD_TYPE(craft));
		msg_to_char(ch, "Builds: [&c%d&0] %s\r\n", GET_CRAFT_BUILD_TYPE(craft), bld ? GET_BLD_NAME(bld) : "UNKNOWN");
	}
	else if (CRAFT_FLAGGED(craft, CRAFT_VEHICLE)) {
		msg_to_char(ch, "Creates Vehicle: [&c%d&0] %s\r\n", GET_CRAFT_OBJECT(craft), (GET_CRAFT_OBJECT(craft) == NOTHING ? "NOTHING" : get_vehicle_name_by_proto(GET_CRAFT_OBJECT(craft))));
	}
	else if (CRAFT_FLAGGED(craft, CRAFT_SOUP)) {
		msg_to_char(ch, "Creates Volume: [&g%d drink%s&0], Liquid: [&g%d&0] %s\r\n", GET_CRAFT_QUANTITY(craft), PLURAL(GET_CRAFT_QUANTITY(craft)), GET_CRAFT_OBJECT(craft), get_generic_string_by_vnum(GET_CRAFT_OBJECT(craft), GENERIC_LIQUID, GSTR_LIQUID_NAME));
	}
	else {
		msg_to_char(ch, "Creates Quantity: [&g%d&0], Item: [&c%d&0] %s\r\n", GET_CRAFT_QUANTITY(craft), GET_CRAFT_OBJECT(craft), get_obj_name_by_proto(GET_CRAFT_OBJECT(craft)));
	}
	
	sprintf(buf, "%s", (GET_CRAFT_ABILITY(craft) == NO_ABIL ? "none" : get_ability_name_by_vnum(GET_CRAFT_ABILITY(craft))));
	if ((abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft))) && ABIL_ASSIGNED_SKILL(abil) != NULL) {
		sprintf(buf + strlen(buf), " (%s %d)", SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
	}
	msg_to_char(ch, "Ability: &y%s&0, Level: &g%d&0", buf, GET_CRAFT_MIN_LEVEL(craft));
	
	if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_BUILD && !CRAFT_FLAGGED(craft, CRAFT_VEHICLE)) {
		seconds = GET_CRAFT_TIME(craft) * ACTION_CYCLE_TIME;
		msg_to_char(ch, ", Time: [&g%d action tick%s&0 | &g%d:%02d&0]\r\n", GET_CRAFT_TIME(craft), PLURAL(GET_CRAFT_TIME(craft)), seconds / SECS_PER_REAL_MIN, seconds % SECS_PER_REAL_MIN);
	}
	else {
		msg_to_char(ch, "\r\n");
	}

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
	msg_to_char(ch, "Resources required:\r\n");
	get_resource_display(GET_CRAFT_RESOURCES(craft), buf);
	send_to_char(buf, ch);
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


/**
* Shows immortal stats on an empire.
*
* @param char_data *ch The person viewing the stats.
* @param empire_data *emp The empire.
*/
void do_stat_empire(char_data *ch, empire_data *emp) {
	extern int get_total_score(empire_data *emp);
	void script_stat (char_data *ch, struct script_data *sc);
	
	extern const char *empire_admin_flags[];
	extern const char *empire_attributes[];
	extern const char *empire_trait_types[];
	extern const char *progress_types[];
	extern const char *techs[];
	
	empire_data *emp_iter, *next_emp;
	int iter, found_rank, total, len;
	player_index_data *index;
	char line[256];
	bool any;
	
	// determine rank by iterating over the sorted empire list
	found_rank = 0;
	HASH_ITER(hh, empire_table, emp_iter, next_emp) {
		++found_rank;
		if (emp == emp_iter) {
			break;
		}
	}
	
	msg_to_char(ch, "%s%s\t0, Adjective: [%s%s\t0], VNum: [\tc%5d\t0]\r\n", EMPIRE_BANNER(emp), EMPIRE_NAME(emp), EMPIRE_BANNER(emp), EMPIRE_ADJECTIVE(emp), EMPIRE_VNUM(emp));
	msg_to_char(ch, "Leader: [\ty%s\t0], Created: [\ty%-24.24s\t0], Score/rank: [\tc%d #%d\t0]\r\n", (index = find_player_index_by_idnum(EMPIRE_LEADER(emp))) ? index->fullname : "UNKNOWN", ctime(&EMPIRE_CREATE_TIME(emp)), get_total_score(emp), found_rank);
	
	sprintbit(EMPIRE_ADMIN_FLAGS(emp), empire_admin_flags, line, TRUE);
	msg_to_char(ch, "Admin flags: \tg%s\t0\r\n", line);
	
	sprintbit(EMPIRE_FRONTIER_TRAITS(emp), empire_trait_types, line, TRUE);
	msg_to_char(ch, "Frontier traits: \tc%s\t0\r\n", line);

	msg_to_char(ch, "Technology: \tg");
	for (iter = 0, len = 0, any = FALSE; iter < NUM_TECHS; ++iter) {
		if (EMPIRE_HAS_TECH(emp, iter)) {
			any = TRUE;
			if (len > 0 && len + strlen(techs[iter]) + 3 >= 80) {	// new line
				msg_to_char(ch, ",\r\n%s", techs[iter]);
				len = strlen(techs[iter]);
			}
			else {	// same line
				msg_to_char(ch, "%s%s", (len > 0) ? ", " : "", techs[iter]);
				len += strlen(techs[iter]) + 2;
			}
		}
	}
	if (!any) {
		msg_to_char(ch, "none");
	}
	msg_to_char(ch, "\t0\r\n");	// end tech
	
	msg_to_char(ch, "Members: [\tc%d\t0/\tc%d\t0], Citizens: [\tc%d\t0], Military: [\tc%d\t0]\r\n", EMPIRE_MEMBERS(emp), EMPIRE_TOTAL_MEMBER_COUNT(emp), EMPIRE_POPULATION(emp), EMPIRE_MILITARY(emp));
	msg_to_char(ch, "Territory: [\tc%d\t0/\tc%d\t0], In-City: [\tc%d\t0], Outskirts: [\tc%d\t0/\tc%d\t0], Frontier: [\tc%d\t0/\tc%d\t0]\r\n", EMPIRE_TERRITORY(emp, TER_TOTAL), land_can_claim(emp, TER_TOTAL), EMPIRE_TERRITORY(emp, TER_CITY), EMPIRE_TERRITORY(emp, TER_OUTSKIRTS), land_can_claim(emp, TER_OUTSKIRTS), EMPIRE_TERRITORY(emp, TER_FRONTIER), land_can_claim(emp, TER_FRONTIER));

	msg_to_char(ch, "Wealth: [\ty%d\t0], Treasure: [\ty%d\t0], Coins: [\ty%.1f\t0]\r\n", (int) GET_TOTAL_WEALTH(emp), EMPIRE_WEALTH(emp), EMPIRE_COINS(emp));
	msg_to_char(ch, "Greatness: [\tc%d\t0], Fame: [\tc%d\t0]\r\n", EMPIRE_GREATNESS(emp), EMPIRE_FAME(emp));
	
	// progress points by category
	total = 0;
	for (iter = 1; iter < NUM_PROGRESS_TYPES; ++iter) {
		total += EMPIRE_PROGRESS_POINTS(emp, iter);
		msg_to_char(ch, "%s: [\ty%d\t0], ", progress_types[iter], EMPIRE_PROGRESS_POINTS(emp, iter));
	}
	msg_to_char(ch, "Total: [\ty%d\t0]\r\n", total);
	
	// attributes
	for (iter = 0, len = 0; iter < NUM_EMPIRE_ATTRIBUTES; ++iter) {
		sprintf(line, "%s: [\tc%+d\t0]", empire_attributes[iter], EMPIRE_ATTRIBUTE(emp, iter));
		
		if (len > 0 && len + strlen(line) + 2 >= 80) {	// start new line
			msg_to_char(ch, "\r\n%s", line);
			len = strlen(line);
		}
		else {	// same line
			msg_to_char(ch, "%s%s", (len > 0) ? ", " : "", line);
			len += strlen(line) + 2;
		}
	}
	if (len > 0) {
		msg_to_char(ch, "\r\n");
	}
	
	msg_to_char(ch, "Script information:\r\n");
	if (SCRIPT(emp)) {
		script_stat(ch, SCRIPT(emp));
	}
	else {
		msg_to_char(ch, "  None.\r\n");
	}
}


/**
* Show a character stats on a particular global.
*
* @param char_data *ch The player requesting stats.
* @param struct global_data *glb The global to stat.
*/
void do_stat_global(char_data *ch, struct global_data *glb) {
	extern const char *global_flags[];
	extern const char *global_types[];
	
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
	ability_data *abil;
	
	msg_to_char(ch, "Global VNum: [&c%d&0], Type: [&c%s&0], Name: '&c%s&0'\r\n", GET_GLOBAL_VNUM(glb), global_types[GET_GLOBAL_TYPE(glb)], GET_GLOBAL_NAME(glb));

	sprintf(buf, "%s", (GET_GLOBAL_ABILITY(glb) == NO_ABIL ? "none" : get_ability_name_by_vnum(GET_GLOBAL_ABILITY(glb))));
	if ((abil = find_ability_by_vnum(GET_GLOBAL_ABILITY(glb))) && ABIL_ASSIGNED_SKILL(abil) != NULL) {
		sprintf(buf + strlen(buf), " (%s %d)", SKILL_ABBREV(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
	}
	msg_to_char(ch, "Requires ability: [&y%s&0], Percent: [&g%.2f&0]\r\n", buf, GET_GLOBAL_PERCENT(glb));
	
	sprintbit(GET_GLOBAL_FLAGS(glb), global_flags, buf, TRUE);
	msg_to_char(ch, "Flags: &g%s&0\r\n", buf);
	
	// GLOBAL_x
	switch (GET_GLOBAL_TYPE(glb)) {
		case GLOBAL_MOB_INTERACTIONS: {
			sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), action_bits, buf, TRUE);
			sprintbit(GET_GLOBAL_TYPE_EXCLUDE(glb), action_bits, buf2, TRUE);
			msg_to_char(ch, "Levels: [&g%s&0], Mob Flags: &c%s&0, Exclude: &c%s&0\r\n", level_range_string(GET_GLOBAL_MIN_LEVEL(glb), GET_GLOBAL_MAX_LEVEL(glb), 0), buf, buf2);
			break;
		}
		case GLOBAL_MINE_DATA: {
			sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), sector_flags, buf, TRUE);
			sprintbit(GET_GLOBAL_TYPE_EXCLUDE(glb), sector_flags, buf2, TRUE);
			msg_to_char(ch, "Capacity: [&g%d-%d normal, %d-%d deep&0], Sector Flags: &c%s&0, Exclude: &c%s&0\r\n", GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE)/2, GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE), (int)(GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE) / 2.0 * 1.5), (int)(GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE) * 1.5), buf, buf2);
			break;
		}
		case GLOBAL_NEWBIE_GEAR: {
			void get_archetype_gear_display(struct archetype_gear *list, char *save_buffer);
			get_archetype_gear_display(GET_GLOBAL_GEAR(glb), buf2);
			msg_to_char(ch, "Gear:\r\n%s", buf2);
			break;
		}
	}
	
	if (GET_GLOBAL_INTERACTIONS(glb)) {
		send_to_char("Interactions:\r\n", ch);
		get_interaction_display(GET_GLOBAL_INTERACTIONS(glb), buf);
		send_to_char(buf, ch);
	}
}


/* Gives detailed information on an object (j) to ch */
void do_stat_object(char_data *ch, obj_data *j) {
	extern const char *apply_type_names[];
	extern const struct material_data materials[NUM_MATERIALS];
	extern const char *wear_bits[];
	extern const char *item_types[];
	extern const char *container_bits[];
	extern const char *obj_custom_types[];
	extern const char *storage_bits[];
	extern double get_base_dps(obj_data *weapon);
	extern double get_weapon_speed(obj_data *weapon);
	extern const char *armor_types[NUM_ARMOR_TYPES+1];
	
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	int found;
	struct obj_apply *apply;
	room_data *room;
	obj_vnum vnum = GET_OBJ_VNUM(j);
	obj_data *j2;
	struct obj_storage_type *store;
	struct custom_message *ocm;
	player_index_data *index;
	crop_data *cp;
	bool any;

	msg_to_char(ch, "Name: '&y%s&0', Aliases: %s\r\n", GET_OBJ_DESC(j, ch, OBJ_DESC_SHORT), GET_OBJ_KEYWORDS(j));

	if (GET_OBJ_CURRENT_SCALE_LEVEL(j) > 0) {
		sprintf(buf, " (%d)", GET_OBJ_CURRENT_SCALE_LEVEL(j));
	}
	else if (GET_OBJ_MIN_SCALE_LEVEL(j) > 0 || GET_OBJ_MAX_SCALE_LEVEL(j) > 0) {
		sprintf(buf, " (%d-%d)", GET_OBJ_MIN_SCALE_LEVEL(j), GET_OBJ_MAX_SCALE_LEVEL(j));
	}
	else {
		strcpy(buf, " (no scale limit)");
	}
	
	msg_to_char(ch, "Gear rating: [&g%.2f%s&0], VNum: [&g%5d&0], Type: &c%s&0\r\n", rate_item(j), buf, vnum, item_types[(int) GET_OBJ_TYPE(j)]);
	msg_to_char(ch, "L-Des: %s\r\n", GET_OBJ_DESC(j, ch, OBJ_DESC_LONG));
	
	if (GET_OBJ_ACTION_DESC(j)) {
		msg_to_char(ch, "%s", GET_OBJ_ACTION_DESC(j));
	}

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
	
	msg_to_char(ch, "Timer: &y%d&0, Material: &y%s&0, Component type: &y%s&0\r\n", GET_OBJ_TIMER(j), materials[GET_OBJ_MATERIAL(j)].name, component_string(GET_OBJ_CMP_TYPE(j), GET_OBJ_CMP_FLAGS(j)));
	
	if (GET_OBJ_REQUIRES_QUEST(j) != NOTHING) {
		msg_to_char(ch, "Requires quest: [%d] &c%s&0\r\n", GET_OBJ_REQUIRES_QUEST(j), get_quest_name_by_proto(GET_OBJ_REQUIRES_QUEST(j)));
	}
	
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
	strcat(buf, ", In vehicle: ");
	strcat(buf, j->in_vehicle ? VEH_SHORT_DESC(j->in_vehicle) : "None");
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
		any = FALSE;
		for (bind = OBJ_BOUND_TO(j); bind; bind = bind->next) {
			msg_to_char(ch, "%s %s", (any ? "," : ""), (index = find_player_index_by_idnum(bind->idnum)) ? index->fullname : "<unknown>");
			any = TRUE;
		}
		msg_to_char(ch, "\r\n");
	}
	
	// ITEM_X: stat obj
	switch (GET_OBJ_TYPE(j)) {
		case ITEM_BOOK: {
			book_data *book = book_proto(GET_BOOK_ID(j));
			msg_to_char(ch, "Book: %d - %s\r\n", GET_BOOK_ID(j), (book ? book->title : "unknown"));
			break;
		}
		case ITEM_POISON: {
			msg_to_char(ch, "Poison affect type: [%d] %s\r\n", GET_POISON_AFFECT(j), GET_POISON_AFFECT(j) != NOTHING ? get_generic_name_by_vnum(GET_POISON_AFFECT(j)) : "not custom");
			msg_to_char(ch, "Charges remaining: %d\r\n", GET_POISON_CHARGES(j));
			break;
		}
		case ITEM_RECIPE: {
			craft_data *cft = craft_proto(GET_RECIPE_VNUM(j));
			msg_to_char(ch, "Teaches craft: %d %s (%s)\r\n", GET_RECIPE_VNUM(j), cft ? GET_CRAFT_NAME(cft) : "UNKNOWN", cft ? craft_types[GET_CRAFT_TYPE(cft)] : "?");
			break;
		}
		case ITEM_WEAPON:
			msg_to_char(ch, "Speed: %.2f, Damage: %d (%s+%.2f base dps)\r\n", get_weapon_speed(j), GET_WEAPON_DAMAGE_BONUS(j), (IS_MAGIC_ATTACK(GET_WEAPON_TYPE(j)) ? "Intelligence" : "Strength"), get_base_dps(j));
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
			msg_to_char(ch, "Contains: %d/%d drinks of %s\r\n", GET_DRINK_CONTAINER_CONTENTS(j), GET_DRINK_CONTAINER_CAPACITY(j), get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(j), GENERIC_LIQUID, GSTR_LIQUID_NAME));
			break;
		case ITEM_FOOD:
			msg_to_char(ch, "Fills for: %d hour%s\r\n", GET_FOOD_HOURS_OF_FULLNESS(j), PLURAL(GET_FOOD_HOURS_OF_FULLNESS(j)));
			break;
		case ITEM_CORPSE:
			msg_to_char(ch, "Corpse of: ");

			if (IS_NPC_CORPSE(j)) {
				msg_to_char(ch, "%s\r\n", get_mob_name_by_proto(GET_CORPSE_NPC_VNUM(j)));
			}
			else if (IS_PC_CORPSE(j)) {
				msg_to_char(ch, "%s\r\n", (index = find_player_index_by_idnum(GET_CORPSE_PC_ID(j))) ? index->fullname : "a player");
			}
			else {
				msg_to_char(ch, "unknown\r\n");
			}
			
			msg_to_char(ch, "Corpse size: %s\r\n", size_types[GET_CORPSE_SIZE(j)]);
			break;
		case ITEM_COINS: {
			msg_to_char(ch, "Amount: %s\r\n", money_amount(real_empire(GET_COINS_EMPIRE_ID(j)), GET_COINS_AMOUNT(j)));
			break;
		}
		case ITEM_CURRENCY: {
			msg_to_char(ch, "Amount: %d %s\r\n", GET_CURRENCY_AMOUNT(j), get_generic_string_by_vnum(GET_CURRENCY_VNUM(j), GENERIC_CURRENCY, WHICH_CURRENCY(GET_CURRENCY_AMOUNT(j))));
			break;
		}
		case ITEM_MISSILE_WEAPON:
			msg_to_char(ch, "Speed: %.2f, Damage: %d (%s+%.2f base dps)\r\n", get_weapon_speed(j), GET_MISSILE_WEAPON_DAMAGE(j), (IS_MAGIC_ATTACK(GET_MISSILE_WEAPON_TYPE(j)) ? "Intelligence" : "Strength"), get_base_dps(j));
			msg_to_char(ch, "Damage type: %s\r\n", attack_hit_info[GET_MISSILE_WEAPON_TYPE(j)].name);
			msg_to_char(ch, "Ammo type: %c\r\n", 'A' + GET_MISSILE_WEAPON_AMMO_TYPE(j));
			break;
		case ITEM_AMMO:
			if (GET_AMMO_QUANTITY(j) > 0) {
				msg_to_char(ch, "Quantity: %d\r\n", GET_AMMO_QUANTITY(j));
			}
			if (GET_AMMO_DAMAGE_BONUS(j) > 0) {
				msg_to_char(ch, "Damage: %+d\r\n", GET_AMMO_DAMAGE_BONUS(j));
			}
			msg_to_char(ch, "Ammo type: %c\r\n", 'A' + GET_AMMO_TYPE(j));
			if (GET_OBJ_AFF_FLAGS(j) || GET_OBJ_APPLIES(j)) {
				generic_data *aftype = find_generic(GET_OBJ_VNUM(j), GENERIC_AFFECT);
				msg_to_char(ch, "Debuff name: %s\r\n", aftype ? GEN_NAME(aftype) : get_generic_name_by_vnum(ATYPE_RANGED_WEAPON));
			}
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
		case ITEM_PAINT: {
			extern const char *paint_colors[];
			extern const char *paint_names[];
			msg_to_char(ch, "Paint color: %s%s\t0\r\n", paint_colors[GET_PAINT_COLOR(j)], paint_names[GET_PAINT_COLOR(j)]);
			break;
		}
		case ITEM_POTION: {
			msg_to_char(ch, "Potion affect type: [%d] %s\r\n", GET_POTION_AFFECT(j), GET_POTION_AFFECT(j) != NOTHING ? get_generic_name_by_vnum(GET_POTION_AFFECT(j)) : "not custom");
			msg_to_char(ch, "Potion cooldown: [%d] %s, %d second%s\r\n", GET_POTION_COOLDOWN_TYPE(j), GET_POTION_COOLDOWN_TYPE(j) != NOTHING ? get_generic_name_by_vnum(GET_POTION_COOLDOWN_TYPE(j)) : "no cooldown", GET_POTION_COOLDOWN_TIME(j), PLURAL(GET_POTION_COOLDOWN_TIME(j)));
			break;
		}
		case ITEM_WEALTH: {
			msg_to_char(ch, "Wealth value: %d\r\n", GET_WEALTH_VALUE(j));
			msg_to_char(ch, "Automatically minted by workforce: %s\r\n", GET_WEALTH_AUTOMINT(j) ? "yes" : "no");
			break;
		}
		case ITEM_LIGHTER: {
			if (GET_LIGHTER_USES(j) == UNLIMITED) {
				msg_to_char(ch, "Lighter uses: unlimited\r\n");
			}
			else {
				msg_to_char(ch, "Lighter uses: %d\r\n", GET_LIGHTER_USES(j));
			}
			break;
		}
		case ITEM_MINIPET: {
			msg_to_char(ch, "Mini-pet: [%d] %s\r\n", GET_MINIPET_VNUM(j), get_mob_name_by_proto(GET_MINIPET_VNUM(j)));
			break;
		}
		default:
			msg_to_char(ch, "Values 0-2: [&g%d&0] [&g%d&0] [&g%d&0]\r\n", GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2));
			break;
	}
	
	// data that isn't type-based:
	if (OBJ_FLAGGED(j, OBJ_PLANTABLE) && (cp = crop_proto(GET_OBJ_VAL(j, VAL_FOOD_CROP_TYPE)))) {
		msg_to_char(ch, "Plants %s (%s).\r\n", GET_CROP_NAME(cp), climate_types[GET_CROP_CLIMATE(cp)]);
	}

	/*
	 * I deleted the "equipment status" code from here because it seemed
	 * more or less useless and just takes up valuable screen space.
	 */

	if (j->contains) {
		sprintf(buf, "Contents:&g");
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
	for (apply = GET_OBJ_APPLIES(j); apply; apply = apply->next) {
		if (apply->apply_type != APPLY_TYPE_NATURAL) {
			sprintf(part, " (%s)", apply_type_names[(int)apply->apply_type]);
		}
		else {
			*part = '\0';
		}
		msg_to_char(ch, "%s %+d to %s%s", found++ ? "," : "", apply->modifier, apply_types[(int) apply->location], part);
	}
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
	extern struct generic_name_data *get_best_name_list(int name_set, int sex);
	extern const char *exit_bits[];
	extern const char *depletion_type[NUM_DEPLETION_TYPES];
	extern const char *instance_flags[];
	extern const char *room_extra_types[];
	
	char buf1[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], buf3[MAX_STRING_LENGTH], *nstr;
	struct depletion_data *dep;
	struct empire_city_data *city;
	int found, num;
	bool comma;
	obj_data *j;
	char_data *k;
	crop_data *cp;
	empire_data *emp;
	struct affected_type *aff;
	struct room_extra_data *red, *next_red;
	struct generic_name_data *name_set;
	struct empire_territory_data *ter;
	struct empire_npc_data *npc;
	player_index_data *index;
	struct global_data *glb;
	room_data *home = HOME_ROOM(IN_ROOM(ch));
	struct instance_data *inst, *inst_iter;
	vehicle_data *veh;
	
	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_HAS_CROP_DATA) && (cp = ROOM_CROP(IN_ROOM(ch)))) {
		strcpy(buf2, GET_CROP_NAME(cp));
		CAP(buf2);
	}
	else {
		strcpy(buf2, GET_SECT_NAME(SECT(IN_ROOM(ch))));
	}
	
	// check for natural sect
	if (GET_ROOM_VNUM(IN_ROOM(ch)) < MAP_SIZE) {
		sprintf(buf3, "/&c%s&0", GET_SECT_NAME(world_map[X_COORD(IN_ROOM(ch))][Y_COORD(IN_ROOM(ch))].natural_sector));
	}
	else {
		*buf3 = '\0';
	}
	
	msg_to_char(ch, "(%d, %d) %s (&c%s&0/&c%s&0%s)\r\n", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)), get_room_name(IN_ROOM(ch), FALSE), buf2, GET_SECT_NAME(BASE_SECT(IN_ROOM(ch))), buf3);
	msg_to_char(ch, "VNum: [&g%d&0], Island: [%d] %s\r\n", GET_ROOM_VNUM(IN_ROOM(ch)), GET_ISLAND_ID(IN_ROOM(ch)), GET_ISLAND(IN_ROOM(ch)) ? GET_ISLAND(IN_ROOM(ch))->name : "no island");
	
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
			msg_to_char(ch, ", Home: %s", (index = find_player_index_by_idnum(ROOM_PRIVATE_OWNER(home))) ? index->fullname : "someone");
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
		
		if (IS_BURNING(home)) {
			sprintf(buf2, "Burns down in: %ld seconds", BUILDING_BURN_DOWN_TIME(home) - time(0));
		}
		else {
			strcpy(buf2, "Not on fire");
		}
		msg_to_char(ch, "%s, Damage: %d/%d\r\n", buf2, (int) BUILDING_DAMAGE(home), GET_BUILDING(home) ? GET_BLD_MAX_DAMAGE(GET_BUILDING(home)) : 0);
	}

	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_CAN_MINE) || room_has_function_and_city_ok(IN_ROOM(ch), FNC_MINE)) {
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_GLB_VNUM) <= 0 || !(glb = global_proto(get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_GLB_VNUM))) || GET_GLOBAL_TYPE(glb) != GLOBAL_MINE_DATA) {
			msg_to_char(ch, "This area is unmined.\r\n");
		}
		else {
			msg_to_char(ch, "Mine type: %s, Amount remaining: %d\r\n", GET_GLOBAL_NAME(glb), get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT));
		}
	}
	
	if ((inst = find_instance_by_room(IN_ROOM(ch), FALSE, TRUE))) {
		num = 0;
		LL_FOREACH(instance_list, inst_iter) {
			++num;
			if (inst_iter == inst) {
				break;
			}
		}
		sprintbit(INST_FLAGS(inst), instance_flags, buf2, TRUE);
		msg_to_char(ch, "Instance \tc%d\t0: [\tg%d\t0] \ty%s\t0, Main Room: [\tg%d\t0], Flags: \tc%s\t0\r\n", num, GET_ADV_VNUM(INST_ADVENTURE(inst)), GET_ADV_NAME(INST_ADVENTURE(inst)), (INST_START(inst) ? GET_ROOM_VNUM(INST_START(inst)) : NOWHERE), buf2);
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
	
	if (ROOM_VEHICLES(IN_ROOM(ch))) {
		sprintf(buf, "Vehicles:&w");
		found = 0;
		LL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
			if (!CAN_SEE_VEHICLE(ch, veh)) {
				continue;
			}
			sprintf(buf2, "%s %s", found++ ? "," : "", VEH_SHORT_DESC(veh));
			strcat(buf, buf2);
			if (strlen(buf) >= 62) {
				if (veh->next_in_room) {
					send_to_char(strcat(buf, ",\r\n"), ch);
				}
				else {
					send_to_char(strcat(buf, "\r\n"), ch);
				}
				*buf = found = 0;
			}
		}

		if (*buf) {
			send_to_char(strcat(buf, "\r\n"), ch);
		}
		send_to_char("&0", ch);
	}
	
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
	
	// empire citizens
	if (ROOM_OWNER(IN_ROOM(ch)) && (ter = find_territory_entry(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch))) && ter->npcs) {
		*buf = '\0';
		LL_FOREACH(ter->npcs, npc) {
			if (npc->mob) {
				sprintf(buf + strlen(buf), "%s[%d] \ty%s\t0", *buf ? ", " : "", GET_MOB_VNUM(npc->mob), GET_SHORT_DESC(npc->mob));
			}
			else if ((k = mob_proto(npc->vnum))) {
				name_set = get_best_name_list(MOB_NAME_SET(k), npc->sex);
				nstr = str_replace("#n", name_set->names[npc->name], GET_SHORT_DESC(k));
				sprintf(buf + strlen(buf), "%s[%d] \tc%s\t0", *buf ? ", " : "", npc->vnum, nstr);
				free(nstr);
			}
			else {
				sprintf(buf + strlen(buf), "%s[%d] \trUNKNOWN\t0", *buf ? ", " : "", npc->vnum);
			}
		}
		
		if (*buf) {
			msg_to_char(ch, "Citizens: %s\r\n", buf);
		}
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

			sprintf(buf, "Affect: (%3ldsec) &c%s&0 ", (aff->duration - time(0)), get_generic_name_by_vnum(aff->type));

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
	
	if (ROOM_EXTRA_DATA(IN_ROOM(ch))) {
		msg_to_char(ch, "Extra data:\r\n");
		
		HASH_ITER(hh, ROOM_EXTRA_DATA(IN_ROOM(ch)), red, next_red) {
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
	
	sprintbit(GET_RMT_FUNCTIONS(rmt), function_flags, lbuf, TRUE);
	msg_to_char(ch, "Functions: &y%s&0\r\n", lbuf);

	sprintbit(GET_RMT_BASE_AFFECTS(rmt), room_aff_bits, lbuf, TRUE);
	msg_to_char(ch, "Affects: &g%s&0\r\n", lbuf);
	
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
	
	struct sector_index_type *idx = find_sector_index(GET_SECT_VNUM(st));
	
	msg_to_char(ch, "Sector VNum: [&c%d&0], Name: '&c%s&0', Live Count [&c%d&0/&c%d&0]\r\n", st->vnum, st->name, idx->sect_count, idx->base_count);
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
int vnum_book(char *searchname, char_data *ch) {
	book_data *book, *next_book;
	int found = 0;
	
	HASH_ITER(hh, book_table, book, next_book) {
		if (multi_isname(searchname, book->title) || multi_isname(searchname, book->byline)) {
			msg_to_char(ch, "%3d. [%5d] %s\t0 (%s\t0)\r\n", ++found, book->vnum, book->title, book->byline);
		}
	}

	return (found);
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
	int found = 0;
	
	HASH_ITER(hh, craft_table, iter, next_iter) {
		if (multi_isname(searchname, GET_CRAFT_NAME(iter))) {
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


/**
* Searches the global db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_global(char *searchname, char_data *ch) {
	extern const char *global_types[];
	
	struct global_data *iter, *next_iter;
	char flags[MAX_STRING_LENGTH];
	int found = 0;
	
	HASH_ITER(hh, globals_table, iter, next_iter) {
		if (multi_isname(searchname, GET_GLOBAL_NAME(iter))) {
			// GLOBAL_x
			switch (GET_GLOBAL_TYPE(iter)) {
				case GLOBAL_MOB_INTERACTIONS: {
					sprintbit(GET_GLOBAL_TYPE_FLAGS(iter), action_bits, flags, TRUE);
					msg_to_char(ch, "%3d. [%5d] %s (%s) %s (%s)\r\n", ++found, GET_GLOBAL_VNUM(iter), GET_GLOBAL_NAME(iter), level_range_string(GET_GLOBAL_MIN_LEVEL(iter), GET_GLOBAL_MAX_LEVEL(iter), 0), flags, global_types[GET_GLOBAL_TYPE(iter)]);
					break;
				}
				case GLOBAL_MINE_DATA: {
					sprintbit(GET_GLOBAL_TYPE_FLAGS(iter), sector_flags, flags, TRUE);
					msg_to_char(ch, "%3d. [%5d] %s - %s (%s)\r\n", ++found, GET_GLOBAL_VNUM(iter), GET_GLOBAL_NAME(iter), flags, global_types[GET_GLOBAL_TYPE(iter)]);
					break;
				}
				default: {
					msg_to_char(ch, "%3d. [%5d] %s (%s)\r\n", ++found, GET_GLOBAL_VNUM(iter), GET_GLOBAL_NAME(iter), global_types[GET_GLOBAL_TYPE(iter)]);
					break;
				}
			}
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

ACMD(do_addnotes) {
	char notes[MAX_ADMIN_NOTES_LENGTH + 1], buf[MAX_STRING_LENGTH];
	player_index_data *index = NULL;
	account_data *acct = NULL;
	
	argument = one_argument(argument, arg);	// target
	skip_spaces(&argument);	// text to add
	
	if (!*arg || !*argument) {
		msg_to_char(ch, "Usage: addnotes <name> <text>\r\n");
		return;
	}
	
	// argument processing
	if (isdigit(*arg)) {
		if (!(acct = find_account(atoi(arg)))) {
			msg_to_char(ch, "Unknown account '%s'.\r\n", arg);
			return;
		}
	}
	else {
		if (!(index = find_player_index_by_name(arg))) {
			send_to_char("There is no such player.\r\n", ch);
			return;
		}
		if (index->access_level >= GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "You cannot add notes for players of that level.\r\n");
			return;
		}
		if (!(acct = find_account(index->account_id))) {
			msg_to_char(ch, "Unable to find the account for that player.\r\n");
			return;
		}
	}
	
	// final checks
	if (!acct) {
		msg_to_char(ch, "Unknown account.\r\n");
	}
	if (strlen(NULLSAFE(acct->notes)) + strlen(argument) + 2 > MAX_ADMIN_NOTES_LENGTH) {
		msg_to_char(ch, "Notes too long, unable to add text. Use editnotes instead.\r\n");
	}
	else {
		snprintf(notes, sizeof(notes), "%s%s\r\n", NULLSAFE(acct->notes), argument);
		if (acct->notes) {
			free(acct->notes);
		}
		acct->notes = str_dup(notes);
		SAVE_ACCOUNT(acct);
		
		if (index) {
			strcpy(buf, index->fullname);
		}
		else {
			sprintf(buf, "account %d", acct->id);
		}
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has added notes for %s", GET_NAME(ch), buf);
		msg_to_char(ch, "Notes added to %s.\r\n", buf);
	}
}


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
		GET_IMMORTAL_LEVEL(victim) = GET_ACCESS_LEVEL(victim) > LVL_MORTAL ? (LVL_TOP - GET_ACCESS_LEVEL(victim)) : -1;
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
		syslog(SYS_LVL, GET_INVIS_LEV(ch), TRUE, "LVL: %s demoted %s from level %d to %d", GET_NAME(ch), GET_NAME(victim), oldlevel, newlevel);
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
	GET_IMMORTAL_LEVEL(victim) = GET_ACCESS_LEVEL(victim) > LVL_MORTAL ? (LVL_TOP - GET_ACCESS_LEVEL(victim)) : -1;
	SAVE_CHAR(victim);
	check_autowiz(victim);
}


// also do_unapprove
ACMD(do_approve) {
	char_data *vict = NULL;
	bool file = FALSE;

	one_argument(argument, arg);

	if (!*arg)
		msg_to_char(ch, "Usage: %sapprove <character>\r\n", (subcmd == SCMD_UNAPPROVE ? "un" : ""));
	else if (!(vict = find_or_load_player(arg, &file))) {
		msg_to_char(ch, "Unable to find character '%s'.\r\n", arg);
	}
	else if (GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch) && ch != vict) {
		msg_to_char(ch, "You can only %sapprove players below your access level.\r\n", (subcmd == SCMD_UNAPPROVE ? "un" : ""));
	}
	else if (subcmd == SCMD_APPROVE && IS_APPROVED(vict)) {
		msg_to_char(ch, "That player is already approved.\r\n");
	}
	else if (subcmd == SCMD_UNAPPROVE && !IS_APPROVED(vict)) {
		msg_to_char(ch, "That player is not even approved.\r\n");
	}
	else {
		if (subcmd == SCMD_APPROVE) {
			if (config_get_bool("approve_per_character")) {
				SET_BIT(PLR_FLAGS(vict), PLR_APPROVED);
			}
			else {	// per-account (default)
				SET_BIT(GET_ACCOUNT(vict)->flags, ACCT_APPROVED);
				SAVE_ACCOUNT(GET_ACCOUNT(vict));
			}
			
			if (GET_ACCESS_LEVEL(vict) < LVL_MORTAL) {
				GET_ACCESS_LEVEL(vict) = LVL_MORTAL;
			}
		}
		else {
			REMOVE_BIT(GET_ACCOUNT(vict)->flags, ACCT_APPROVED);
			SAVE_ACCOUNT(GET_ACCOUNT(vict));
			REMOVE_BIT(PLR_FLAGS(vict), PLR_APPROVED);
		}
		
		if (!file) {
			msg_to_char(vict, "Your character %s approved.\r\n", (subcmd == SCMD_APPROVE ? "has been" : "is no longer"));
		}
		syslog(SYS_VALID, GET_INVIS_LEV(ch), TRUE, "VALID: %s has been %sapproved by %s", GET_PC_NAME(vict), (subcmd == SCMD_UNAPPROVE ? "un" : ""), GET_NAME(ch));
		msg_to_char(ch, "%s %sapproved.\r\n", GET_PC_NAME(vict), (subcmd == SCMD_UNAPPROVE ? "un" : ""));

		if (file) {
			store_loaded_char(vict);
			vict = NULL;
			file = FALSE;
		}
		
	}
	
	if (vict && file) {
		free_char(vict);
	}
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
	
	msdp_update_room(ch);	// in case it changed
}


ACMD(do_automessage) {
	extern int new_automessage_id();
	void free_automessage(struct automessage *msg);
	void save_automessages(void);
	extern int sort_automessage_by_data(struct automessage *a, struct automessage *b);
	extern const char *automessage_types[];
	extern struct automessage *automessages;
	
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	char cmd_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], id_arg[MAX_STRING_LENGTH];
	struct automessage *msg, *next_msg;
	int id, iter, type, interval = 5;
	player_index_data *plr;
	size_t size;
	
	argument = any_one_arg(argument, cmd_arg);
	
	if (is_abbrev(cmd_arg, "list")) {
		size = snprintf(buf, sizeof(buf), "Automessages:\r\n");
		
		HASH_ITER(hh, automessages, msg, next_msg) {
			switch (msg->timing) {
				case AUTOMSG_REPEATING: {
					snprintf(part, sizeof(part), "%s (%dm)", automessage_types[msg->timing], msg->interval);
					break;
				}
				default: {
					strcpy(part, automessage_types[msg->timing]);
					break;
				}
			}
			
			plr = find_player_index_by_idnum(msg->author);
			snprintf(line, sizeof(line), "%d. %s (%s): %s\r\n", msg->id, part, plr->fullname, msg->msg);
			
			if (size + strlen(line) < sizeof(buf)) {
				strcat(buf, line);
				size += strlen(line);
			}
			else {
				size += snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
				break;
			}
		}
		
		if (!automessages) {
			size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
		}
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}
	else if (is_abbrev(cmd_arg, "add")) {
		argument = any_one_arg(argument, type_arg);
		
		if (!*type_arg) {
			msg_to_char(ch, "Usage: automessage add <type> [interval] <msg>\r\n");
			return;
		}
		if ((type = search_block(type_arg, automessage_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid message type '%s'.\r\n", type_arg);
			return;
		}
		
		// special handling for repeats
		if (type == AUTOMSG_REPEATING) {
			argument = any_one_arg(argument, type_arg);
			if (!*type_arg) {
				msg_to_char(ch, "Usage: automessage add <type> [interval] <msg>\r\n");
				return;
			}
			if (!isdigit(*type_arg) || (interval = atoi(type_arg)) < 1) {
				msg_to_char(ch, "Invalid repeating interval '%s'.\r\n", type_arg);
				return;
			}
		}
		
		skip_spaces(&argument);
		if (!*argument) {
			msg_to_char(ch, "Usage: automessage add <type> [interval] <msg>\r\n");
			return;
		}
		
		// ready!
		CREATE(msg, struct automessage, 1);
		msg->id = new_automessage_id();
		msg->timestamp = time(0);
		msg->author = GET_IDNUM(ch);
		msg->timing = type;
		msg->interval = interval;
		msg->msg = str_dup(argument);
		
		HASH_ADD_INT(automessages, id, msg);
		
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s added automessage %d: %s", GET_NAME(ch), msg->id, msg->msg);
		msg_to_char(ch, "You create a new message %d: %s\r\n", msg->id, NULLSAFE(msg->msg));
		
		HASH_SORT(automessages, sort_automessage_by_data);
		save_automessages();
	}
	else if (is_abbrev(cmd_arg, "change")) {
		argument = any_one_arg(argument, id_arg);
		
		if (!*id_arg || !isdigit(*id_arg) || (id = atoi(id_arg)) < 0) {
			msg_to_char(ch, "Usage: automessage change <id> <property> <value>\r\n");
			return;
		}
		
		HASH_FIND_INT(automessages, &id, msg);
		if (!msg) {
			msg_to_char(ch, "Invalid message id %d.\r\n", id);
			return;
		}
		
		argument = any_one_arg(argument, type_arg);
		skip_spaces(&argument);
		
		if (is_abbrev(type_arg, "type")) {
			if (!*argument) {
				msg_to_char(ch, "Change the type to what?\r\n");
				return;
			}
			if ((type = search_block(argument, automessage_types, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid message type '%s'.\r\n", type_arg);
				return;
			}
			
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s changed automessage %d's type to %s", GET_NAME(ch), msg->id, automessage_types[type]);
			msg_to_char(ch, "You change automessage %d's type to %s\r\n", msg->id, automessage_types[type]);
			if (type == AUTOMSG_REPEATING && type != msg->timing) {
				msg->interval = 5;	// safe default
			}
			msg->timing = type;
			HASH_SORT(automessages, sort_automessage_by_data);
			save_automessages();
		}
		else if (is_abbrev(type_arg, "interval")) {
			if (msg->timing != AUTOMSG_REPEATING) {
				msg_to_char(ch, "You can only change that on a repeating message.\r\n");
				return;
			}
			if (!*argument || !isdigit(*argument) || (interval = atoi(argument)) < 1) {
				msg_to_char(ch, "Invalid interval.\r\n");
				return;
			}
			
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s changed automessage %d's interval to %d minute%s", GET_NAME(ch), msg->id, interval, PLURAL(interval));
			msg_to_char(ch, "You change automessage %d's interval to %d minute%s\r\n", msg->id, interval, PLURAL(interval));
			msg->interval = interval;
			save_automessages();
		}
		else if (is_abbrev(type_arg, "message")) {
			if (!*argument) {
				msg_to_char(ch, "Change the message to what?\r\n");
				return;
			}
			
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s changed automessage %d to: %s", GET_NAME(ch), msg->id, argument);
			msg_to_char(ch, "You change automessage %d from: %s\r\nTo: %s\r\n", msg->id, NULLSAFE(msg->msg), argument);
			
			if (msg->msg) {
				free(msg->msg);
			}
			msg->msg = str_dup(argument);
			save_automessages();
		}
		else {
			msg_to_char(ch, "You can change the type, interval, or message.\r\n");
			return;
		}
	}
	else if (is_abbrev(cmd_arg, "delete")) {
		skip_spaces(&argument);
		if (!*argument) {
			msg_to_char(ch, "Delete which automessage (id)?\r\n");
			return;
		}
		
		id = atoi(argument);
		HASH_FIND_INT(automessages, &id, msg);
		if (!msg) {
			msg_to_char(ch, "No such message id to delete.\r\n");
			return;
		}
		
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s deleted automessage %d: %s", GET_NAME(ch), msg->id, msg->msg);
		msg_to_char(ch, "You delete message %d: %s\r\n", msg->id, NULLSAFE(msg->msg));
		HASH_DEL(automessages, msg);
		free_automessage(msg);
		save_automessages();
	}
	else {
		msg_to_char(ch, "Usage: automessage list\r\n");
		msg_to_char(ch, "       automessage add <type> [interval] <msg>\r\n");
		msg_to_char(ch, "       automessage change <id> <property> <new val>\r\n");
		msg_to_char(ch, "       automessage delete <id>\r\n");
		msg_to_char(ch, "Types: ");
		for (iter = 0; *automessage_types[iter] != '\n'; ++iter) {
			msg_to_char(ch, "%s%s", (iter > 0 ? ", " : ""), automessage_types[iter]);
		}
		msg_to_char(ch, "\r\n");
	}
}


ACMD(do_autostore) {
	void read_vault(empire_data *emp);
	obj_data *obj, *next_obj;
	vehicle_data *veh;
	empire_data *emp = ROOM_OWNER(IN_ROOM(ch));

	one_argument(argument, arg);

	if (!emp && !*arg) {
		msg_to_char(ch, "Nobody owns this spot. Use purge instead.\r\n");
	}
	else if (*arg) {
		if ((obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL) {
			act("$n auto-stores $p.", FALSE, ch, obj, NULL, TO_ROOM);
			perform_autostore(obj, emp, GET_ISLAND_ID(IN_ROOM(ch)));
		}
		else if ((veh = get_vehicle_in_room_vis(ch, arg))) {
			act("$n auto-stores items in $V.", FALSE, ch, NULL, veh, TO_ROOM);
			
			LL_FOREACH_SAFE2(VEH_CONTAINS(veh), obj, next_obj, next_content) {
				perform_autostore(obj, VEH_OWNER(veh), GET_ISLAND_ID(IN_ROOM(ch)));
			}
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
		
		LL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
			LL_FOREACH_SAFE2(VEH_CONTAINS(veh), obj, next_obj, next_content) {
				perform_autostore(obj, VEH_OWNER(veh), GET_ISLAND_ID(IN_ROOM(ch)));
			}
		}
		
		read_vault(emp);
	}
}


ACMD(do_autowiz) {
	check_autowiz(ch);
}


ACMD(do_breakreply) {
	char_data *iter;
	
	LL_FOREACH(character_list, iter) {
		if (IS_NPC(iter) || GET_ACCESS_LEVEL(iter) >= GET_ACCESS_LEVEL(ch)) {
			continue;
		}
		if (GET_LAST_TELL(iter) != GET_IDNUM(ch)) {
			continue;
		}
		
		GET_LAST_TELL(iter) = NOBODY;
	}
	
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		msg_to_char(ch, "Players currently in-game can no longer reply to you (unless you send them another tell).\r\n");
	}
}


ACMD(do_clearabilities) {
	char_data *vict;
	char typestr[MAX_STRING_LENGTH];
	skill_data *skill = NULL;
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
		if (!(skill = find_skill(argument))) {
			msg_to_char(ch, "Invalid skill '%s'.\r\n", argument);
			return;
		}
	}
	
	clear_char_abilities(vict, all ? NO_SKILL : SKILL_VNUM(skill));
	
	if (all) {
		*typestr = '\0';
	}
	else {
		sprintf(typestr, "%s ", SKILL_NAME(skill));
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
			send_to_char("Umm... maybe that's not such a good idea...\r\n", ch);
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


// do_directions
ACMD(do_distance) {
	char arg[MAX_INPUT_LENGTH];
	room_data *target;
	int dir, dist;
	
	one_word(argument, arg);
	
	if (!IS_IMMORTAL(ch) && !IS_NPC(ch) && !HAS_NAVIGATION(ch)) {
		msg_to_char(ch, "You don't know how to navigate.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Get the direction and distance to where?\r\n");
	}
	else if (!IS_IMMORTAL(ch) && (!isdigit(*arg) || !strchr(arg, ','))) {
		msg_to_char(ch, "You can only find distances to coordinates.\r\n");
	}
	else if (!(target = find_target_room(ch, arg))) {
		msg_to_char(ch, "Unknown target.\r\n");
	}
	else {	
		dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), target));
		dist = compute_distance(IN_ROOM(ch), target);
		msg_to_char(ch, "(%d, %d) is %d tile%s %s.\r\n", X_COORD(target), Y_COORD(target), dist, PLURAL(dist), (dir == NO_DIR ? "away" : dirs[dir]));
	}
}


// this also handles emote
ACMD(do_echo) {
	void add_to_channel_history(char_data *ch, int type, char *message);
	void clear_last_act_message(descriptor_data *desc);
	extern bool is_ignoring(char_data *ch, char_data *victim);
	
	char string[MAX_INPUT_LENGTH], lbuf[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH];
	char hbuf[MAX_INPUT_LENGTH];
	char *ptr, *end;
	char_data *vict = NULL, *tmp_char, *c;
	vehicle_data *tmp_veh = NULL;
	obj_data *obj = NULL;
	int len;
	bool needs_name = TRUE;

	skip_spaces(&argument);
	delete_doubledollar(argument);
	strcpy(string, argument);

	if (!*argument) {
		send_to_char("Yes... but what?\r\n", ch);
		return;
	}
	if (subcmd == SCMD_EMOTE && strstr(string, "$O")) {
		msg_to_char(ch, "You can't use the $O symbol in an emote.\r\n");
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
		generic_find(lbuf, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &tmp_char, &obj, &tmp_veh);
		
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
		
		if (subcmd == SCMD_EMOTE && !IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) {
			msg_to_char(ch, "&%c", GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE));
		}
		
		// send message
		act(lbuf, FALSE, ch, obj, vict, TO_CHAR | TO_IGNORE_BAD_CODE);
		
		// channel history
		if (ch->desc && ch->desc->last_act_message) {
			// the message was sent via act(), we can retrieve it from the desc			
			if (subcmd == SCMD_EMOTE && !IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) {
				sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE));
			}
			else {
				*hbuf = '\0';
			}
			strcat(hbuf, ch->desc->last_act_message);
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, hbuf);
		}
	}

	if (vict) {
		// clear last act messages for everyone in the room
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
			if (c->desc && c != ch && c != vict) {
				clear_last_act_message(c->desc);
				
				if (subcmd == SCMD_EMOTE && !IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					msg_to_char(c, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
				}
			}
		}
		
		act(lbuf, FALSE, ch, obj, vict, TO_NOTVICT | TO_IGNORE_BAD_CODE);

		// fetch and store channel history for the room
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
			if (c->desc && c != ch && c != vict && !is_ignoring(c, ch) && c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				if (subcmd == SCMD_EMOTE && !IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
				}
				else {
					*hbuf = '\0';
				}
				strcat(hbuf, c->desc->last_act_message);
				add_to_channel_history(c, CHANNEL_HISTORY_SAY, hbuf);
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
			
			if (subcmd == SCMD_EMOTE && !IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE)) {
				msg_to_char(vict, "&%c", GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE));
			}
		
			// send to vict
			if (vict != ch) {
				act(lbuf, FALSE, ch, obj, vict, TO_VICT | TO_IGNORE_BAD_CODE);
			}
		
			// channel history
			if (vict->desc && vict->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc			
				if (subcmd == SCMD_EMOTE && !IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE)) {
					sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE));
				}
				else {
					*hbuf = '\0';
				}
				strcat(hbuf, vict->desc->last_act_message);
				add_to_channel_history(vict, CHANNEL_HISTORY_SAY, hbuf);
			}
		}
	}
	else {
		// clear last act messages for everyone in the room
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
			if (c->desc && c != ch) {
				clear_last_act_message(c->desc);
							
				if (subcmd == SCMD_EMOTE && !IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
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
				if (subcmd == SCMD_EMOTE && !IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
				}
				else {
					*hbuf = '\0';
				}
				strcat(hbuf, c->desc->last_act_message);
				add_to_channel_history(c, CHANNEL_HISTORY_SAY, hbuf);
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
	player_index_data *index = NULL;
	char buf[MAX_STRING_LENGTH];
	account_data *acct = NULL;
	descriptor_data *desc;
	
	one_argument(argument, arg);
	
	// preliminaries
	if (!ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
		return;
	}
	if (ch->desc->str) {
		msg_to_char(ch, "You're already editing something else.\r\n");
		return;
	}
	if (!*arg) {
		msg_to_char(ch, "Edit notes for whom?\r\n");
		return;
	}
	
	// argument processing
	if (isdigit(*arg)) {
		if (!(acct = find_account(atoi(arg)))) {
			msg_to_char(ch, "Unknown account '%s'.\r\n", arg);
			return;
		}
	}
	else {
		if (!(index = find_player_index_by_name(arg))) {
			send_to_char("There is no such player.\r\n", ch);
			return;
		}
		if (index->access_level >= GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "You cannot edit notes for players of that level.\r\n");
			return;
		}
		if (!(acct = find_account(index->account_id))) {
			msg_to_char(ch, "Unable to find account for that player.\r\n");
			return;
		}
	}
	
	if (!acct) {
		msg_to_char(ch, "Unable to find account.\r\n");
		return;
	}
	
	// ensure nobody else is writing notes for the same person
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (desc != ch->desc && desc->str == &(acct->notes)) {
			msg_to_char(ch, "Someone is already editing notes for that account.\r\n");
			return;
		}
	}
	
	// good to go
	if (index) {
		sprintf(buf, "notes for %s", index->fullname);
	}
	else {
		sprintf(buf, "notes for account %d", acct->id);
	}
	start_string_editor(ch->desc, buf, &(acct->notes), MAX_ADMIN_NOTES_LENGTH-1, TRUE);
	ch->desc->notes_id = acct->id;

	act("$n begins editing some notes.", TRUE, ch, FALSE, FALSE, TO_ROOM);
}


ACMD(do_endwar) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	empire_data *iter, *next_iter, *other, *first = NULL, *second = NULL;
	struct empire_political_data *pol, *rev;
	bool any, line;
	
	argument = any_one_word(argument, arg1);
	argument = any_one_word(argument, arg2);
	
	if (!*arg1 && !*arg2) {	// no args: list active wars
		msg_to_char(ch, "Active wars:\r\n");
		
		any = FALSE;
		HASH_ITER(hh, empire_table, iter, next_iter) {
			line = FALSE;
			
			LL_FOREACH(EMPIRE_DIPLOMACY(iter), pol) {
				if (IS_SET(pol->type, DIPL_WAR) && (other = real_empire(pol->id))) {
					if (!line) {
						msg_to_char(ch, " %s%s\t0 vs ", EMPIRE_BANNER(iter), EMPIRE_NAME(iter));
					}
					
					msg_to_char(ch, "%s%s%s\t0", (line ? ", " : ""), EMPIRE_BANNER(other), EMPIRE_NAME(other));
					any = line = TRUE;
				}
			}
			
			if (line) {
				msg_to_char(ch, "\r\n");
			}
		}
		
		if (!any) {
			msg_to_char(ch, " none\r\n");
		}
	}	// end no-arg
	else if (!(first = get_empire(arg1))) {
		msg_to_char(ch, "Invalid empire '%s'.\r\n", arg1);
	}
	else if (*arg2 && !(second = get_empire(arg2))) {
		msg_to_char(ch, "Invalid empire '%s'.\r\n", arg2);
	}
	else {	// ok now ends wars
		any = FALSE;
		
		// we are guaranteed a "first" empire but not a "second"
		LL_FOREACH(EMPIRE_DIPLOMACY(first), pol) {
			if (second && pol->id != EMPIRE_VNUM(second)) {
				continue;	// doing 1? or all?
			}
			if (!IS_SET(pol->type, DIPL_WAR)) {
				continue;	// not war
			}
			
			other = (second ? second : real_empire(pol->id));
			
			// remove war, set distrust
			REMOVE_BIT(pol->type, DIPL_WAR);
			SET_BIT(pol->type, DIPL_DISTRUST);
			pol->start_time = time(0);
			
			// and back
			if ((rev = find_relation(other, first))) {
				REMOVE_BIT(rev->type, DIPL_WAR);
				SET_BIT(rev->type, DIPL_DISTRUST);
				rev->start_time = time(0);
			}
			
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "ABUSE: DIPL: %s has ended the war between %s and %s", GET_NAME(ch), EMPIRE_NAME(first), EMPIRE_NAME(other));
			log_to_empire(first, ELOG_DIPLOMACY, "The war with %s is over", EMPIRE_NAME(other));
			log_to_empire(other, ELOG_DIPLOMACY, "The war with %s is over", EMPIRE_NAME(first));
			any = TRUE;
		}
		
		if (!any && second) {
			msg_to_char(ch, "You didn't find a war to end between %s and %s.\r\n", EMPIRE_NAME(first), EMPIRE_NAME(second));
		}
		else if (!any) {
			msg_to_char(ch, "%s is not at war with anybody.\r\n", EMPIRE_NAME(first));
		}
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
		syslog(SYS_ERROR, GET_INVIS_LEV(ch), TRUE, "SYSERR: Error opening file %s using 'file' command", file_lookup[l].file);
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
	page_string(ch->desc, show_color_codes(output), TRUE);

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


ACMD(do_forgive) {
	char_data *vict;
	bool any;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Forgive whom?r\n");
	}
	else if (!(vict = get_player_vis(ch, arg, FIND_CHAR_WORLD | FIND_NO_DARK))) {
		send_config_msg(ch, "no_person");
	}
	else if (IS_NPC(vict)) {
		msg_to_char(ch, "You can't forgive an NPC for anything.\r\n");
	}
	else {
		any = FALSE;
		
		if (get_cooldown_time(vict, COOLDOWN_HOSTILE_FLAG) > 0) {
			remove_cooldown_by_type(vict, COOLDOWN_HOSTILE_FLAG);
			msg_to_char(ch, "Hostile flag forgiven.\r\n");
			if (ch != vict) {
				act("$n has forgiven your hostile flag.", FALSE, ch, NULL, vict, TO_VICT);
			}
			any = TRUE;
		}
		
		if (get_cooldown_time(vict, COOLDOWN_ROGUE_FLAG) > 0) {
			remove_cooldown_by_type(vict, COOLDOWN_ROGUE_FLAG);
			msg_to_char(ch, "Rogue flag forgiven.\r\n");
			if (ch != vict) {
				act("$n has forgiven your rogue flag.", FALSE, ch, NULL, vict, TO_VICT);
			}
			any = TRUE;
		}
		
		if (get_cooldown_time(vict, COOLDOWN_LEFT_EMPIRE) > 0) {
			remove_cooldown_by_type(vict, COOLDOWN_LEFT_EMPIRE);
			msg_to_char(ch, "Defect timer forgiven.\r\n");
			if (ch != vict) {
				act("$n has forgiven your empire defect timer.", FALSE, ch, NULL, vict, TO_VICT);
			}
			any = TRUE;
		}
		
		if (get_cooldown_time(vict, COOLDOWN_PVP_FLAG) > 0) {
			remove_cooldown_by_type(vict, COOLDOWN_PVP_FLAG);
			msg_to_char(ch, "PVP cooldown forgiven.\r\n");
			if (ch != vict) {
				act("$n has forgiven your PVP cooldown.", FALSE, ch, NULL, vict, TO_VICT);
			}
			any = TRUE;
		}
		
		if (any) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has forgiven %s", GET_NAME(ch), GET_NAME(vict));
		}
		else {
			act("There's nothing you can forgive $N for.", FALSE, ch, NULL, vict, TO_CHAR);
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

	if (!IS_NPC(ch) && ACCOUNT_FLAGGED(ch, ACCT_MUTED))
		msg_to_char(ch, "You can't use gecho while muted.\r\n");
	else if (!*argument)
		send_to_char("That must be a mistake...\r\n", ch);
	else {
		sprintf(buf, "%s\r\n", argument);
		for (pt = descriptor_list; pt; pt = pt->next) {
			if (STATE(pt) == CON_PLAYING && pt->character && pt->character != ch) {
				send_to_char(buf, pt->character);
				
				if (GET_ACCESS_LEVEL(pt->character) >= GET_ACCESS_LEVEL(ch)) {
					msg_to_char(pt->character, "(gecho by %s)\r\n", GET_REAL_NAME(ch));
				}
			}
		}
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			send_to_char(buf, ch);
		}
	}
}


ACMD(do_goto) {
	room_data *location;
	char_data *iter;

	one_word(argument, arg);
	
	if (!(location = find_target_room(ch, arg))) {
		return;
	}

	if (subcmd != SCMD_GOTO && !can_use_room(ch, location, GUESTS_ALLOWED)) {
		msg_to_char(ch, "You can't teleport there.\r\n");
		return;
	}
	
	// wizhide safety
	if (!PRF_FLAGGED(ch, PRF_WIZHIDE) && GET_INVIS_LEV(ch) < LVL_START_IMM) {
		LL_FOREACH2(ROOM_PEOPLE(location), iter, next_in_room) {
			if (ch == iter || !IS_IMMORTAL(iter)) {
				continue;
			}
			if (GET_INVIS_LEV(iter) > GET_ACCESS_LEVEL(ch)) {
				continue;	// ignore -- ch can't see them at all
			}
			if (PRF_FLAGGED(iter, PRF_WIZHIDE)) {
				msg_to_char(ch, "You can't teleport there because someone there is using wizhide, and you aren't.\r\n");
				return;
			}
		}
	}
	
	perform_goto(ch, location);
}


ACMD(do_hostile) {
	char_data *vict;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Mark whom hostile?\r\n");
	}
	else if (!(vict = get_player_vis(ch, arg, FIND_CHAR_WORLD | FIND_NO_DARK))) {
		send_config_msg(ch, "no_person");
	}
	else if (IS_NPC(vict)) {
		msg_to_char(ch, "You can't mark an NPC hostile.\r\n");
	}
	else if (GET_ACCESS_LEVEL(vict) > GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else {
		add_cooldown(vict, COOLDOWN_HOSTILE_FLAG, config_get_int("hostile_flag_time") * SECS_PER_REAL_MIN);
		msg_to_char(vict, "You are now hostile!\r\n");
		if (ch != vict) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has marked %s as hostile", GET_NAME(ch), GET_NAME(vict));
			act("$N is now hostile.", FALSE, ch, NULL, vict, TO_CHAR);
		}
	}
}


ACMD(do_instance) {
	struct instance_data *inst;
	int count;
	
	argument = any_one_arg(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		count = 0;
		LL_FOREACH(instance_list, inst) {
			++count;
		}
		
		msg_to_char(ch, "Usage: instance <list | add | delete | deleteall | info | nearby | reset | spawn | test> [argument]\r\n");
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
	else if (is_abbrev(arg, "information")) {
		do_instance_info(ch, argument);
	}
	else if (is_abbrev(arg, "nearby")) {
		do_instance_nearby(ch, argument);
	}
	else if (is_abbrev(arg, "reset")) {
		do_instance_reset(ch, argument);
	}
	else if (is_abbrev(arg, "spawn")) {
		do_instance_spawn(ch, argument);
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


ACMD(do_island) {
	void save_island_table();

	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH * 2], line[256], flags[256];
	struct island_info *isle, *next_isle;
	room_data *center;
	bitvector_t old;
	size_t outsize;
	
	argument = any_one_arg(argument, arg1);	// command
	skip_spaces(&argument);	// remainder
	
	if (!ch->desc) {
		// don't bother
		return;
	}
	else if (is_abbrev(arg1, "list")) {
		outsize = snprintf(output, sizeof(output), "Islands:\r\n");
		
		HASH_ITER(hh, island_table, isle, next_isle) {
			if (*argument && !multi_isname(argument, isle->name)) {
				continue;
			}
			
			center = real_room(isle->center);
			
			snprintf(line, sizeof(line), "%2d. %s (%d, %d), size %d, levels %d-%d", isle->id, isle->name, (center ? FLAT_X_COORD(center) : -1), (center ? FLAT_Y_COORD(center) : -1), isle->tile_size, isle->min_level, isle->max_level);
			if (isle->flags) {
				sprintbit(isle->flags, island_bits, flags, TRUE);
				snprintf(line + strlen(line), sizeof(line) - strlen(line), ", %s", flags);
			}
			
			if (strlen(line) + outsize < sizeof(output)) {
				outsize += snprintf(output + outsize, sizeof(output) - outsize, "%s\r\n", line);
			}
			else {
				outsize += snprintf(output + outsize, sizeof(output) - outsize, "OVERFLOW\r\n");
				break;
			}
		}
		
		page_string(ch->desc, output, TRUE);
	}
	else if (is_abbrev(arg1, "rename")) {
		argument = one_argument(argument, arg2);
		skip_spaces(&argument);
		
		if (!*arg2 || !*argument || (!isdigit(*arg2) && strcmp(arg2, "-1"))) {
			msg_to_char(ch, "Usage: island rename <id> <name>\r\n");
		}
		else if (!(isle = get_island(atoi(arg2), FALSE))) {
			msg_to_char(ch, "Unknown island id '%s'.\r\n", arg2);
		}
		else if (strlen(argument) > MAX_ISLAND_NAME) {
			msg_to_char(ch, "Island names may not be longer than %d characters.\r\n", MAX_ISLAND_NAME);
		}
		else if (strchr(argument, '"')) {
			msg_to_char(ch, "Island names may not contain quotation marks.\r\n");
		}
		else {
			send_config_msg(ch, "ok_string");
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has renamed island %d (%s) to %s", GET_NAME(ch), isle->id, NULLSAFE(isle->name), argument);
			
			if (isle->name) {
				free(isle->name);
			}
			isle->name = str_dup(argument);
			
			save_island_table();
		}
	}
	else if (is_abbrev(arg1, "description")) {
		if (!*argument) {
			msg_to_char(ch, "Usage: island description <id>\r\n");
		}
		else if (!ch->desc) {
			msg_to_char(ch, "You can't edit text right now.\r\n");
		}
		else if (ch->desc->str) {
			msg_to_char(ch, "You are already editing something else right now.\r\n");
		}
		else if (isdigit(*argument) && !(isle = get_island(atoi(argument), FALSE))) {
			msg_to_char(ch, "Unknown island id '%s'.\r\n", argument);
		}
		else if (!isdigit(*argument) && !(isle = get_island_by_name(ch, argument))) {
			msg_to_char(ch, "Unknown island '%s'.\r\n", argument);
		}
		else {
			start_string_editor(ch->desc, "island description", &isle->desc, MAX_STRING_LENGTH, TRUE);
			ch->desc->island_desc_id = isle->id;
		}
	}
	else if (is_abbrev(arg1, "flags")) {
		argument = one_argument(argument, arg2);
		skip_spaces(&argument);
		
		if (!*arg2 || !isdigit(*arg2)) {
			msg_to_char(ch, "Usage: island flags <id> [add | remove] [flags]\r\n");
		}
		else if (!(isle = get_island(atoi(arg2), FALSE))) {
			msg_to_char(ch, "Unknown island id '%s'.\r\n", arg2);
		}
		else {
			old = isle->flags;
			isle->flags = olc_process_flag(ch, argument, "island", "island flags", island_bits, isle->flags);
			
			if (isle->flags != old) {
				sprintbit(isle->flags, island_bits, flags, TRUE);
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has set island %d (%s) flags to: %s", GET_NAME(ch), isle->id, NULLSAFE(isle->name), flags);
				save_island_table();
				
				if (IS_SET(isle->flags, ISLE_NO_CUSTOMIZE) && !IS_SET(old, ISLE_NO_CUSTOMIZE)) {
					decustomize_island(isle->id);
				}
			}
		}
	}
	else {
		msg_to_char(ch, "Usage: island list [keywords]\r\n");
		msg_to_char(ch, "       island rename <id> <name>\r\n");
		msg_to_char(ch, "       island description <id>\r\n");
		msg_to_char(ch, "       island flags <id> [add | remove] [flags]\r\n");
	}
}


ACMD(do_last) {
	extern const char *level_names[][2];
	
	char_data *plr = NULL;
	bool file = FALSE;
	char status[10];

	one_argument(argument, arg);

	if (!*arg) {
		send_to_char("For whom do you wish to search?\r\n", ch);
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else if ((GET_ACCESS_LEVEL(plr) > GET_ACCESS_LEVEL(ch)) && (GET_ACCESS_LEVEL(ch) < LVL_IMPL)) {
		send_to_char("You are not sufficiently godly for that!\r\n", ch);
	}
	else {
		strcpy(status, level_names[(int) GET_ACCESS_LEVEL(plr)][0]);
		// crlf built into ctime
		msg_to_char(ch, "[%5d] [%s] %-12s : %-18s : %-20s", GET_IDNUM(plr), status, GET_PC_NAME(plr), plr->desc ? plr->desc->host : plr->prev_host, file ? ctime(&plr->prev_logon) : ctime(&plr->player.time.logon));
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


ACMD(do_load) {
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	vehicle_data *veh;
	char_data *mob, *mort;
	obj_data *obj;
	any_vnum number;

	two_arguments(argument, buf, buf2);

	if (!*buf || !*buf2 || !isdigit(*buf2)) {
		send_to_char("Usage: load { obj | mob | vehicle } <number>\r\n", ch);
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
		mob = read_mobile(number, TRUE);
		setup_generic_npc(mob, NULL, NOTHING, NOTHING);
		char_to_room(mob, IN_ROOM(ch));

		act("$n makes a quaint, magical gesture with one hand.", TRUE, ch, 0, 0, TO_ROOM);
		act("$n has created $N!", FALSE, ch, 0, mob, TO_ROOM);
		act("You create $N.", FALSE, ch, 0, mob, TO_CHAR);
		load_mtrigger(mob);
		
		if ((mort = find_mortal_in_room(IN_ROOM(ch)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s loaded mob %s with mortal present (%s) at %s", GET_NAME(ch), GET_NAME(mob), GET_NAME(mort), room_log_identifier(IN_ROOM(ch)));
		}
		else if (ROOM_OWNER(IN_ROOM(ch)) && !EMPIRE_IMM_ONLY(ROOM_OWNER(IN_ROOM(ch)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s loaded mob %s in mortal empire (%s) at %s", GET_NAME(ch), GET_NAME(mob), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))), room_log_identifier(IN_ROOM(ch)));
		}
	}
	else if (is_abbrev(buf, "obj")) {
		if (!obj_proto(number)) {
			send_to_char("There is no object with that number.\r\n", ch);
			return;
		}
		obj = read_object(number, TRUE);
		if (CAN_WEAR(obj, ITEM_WEAR_TAKE))
			obj_to_char(obj, ch);
		else
			obj_to_room(obj, IN_ROOM(ch));
		act("$n makes a strange magical gesture.", TRUE, ch, 0, 0, TO_ROOM);
		act("$n has created $p!", FALSE, ch, obj, 0, TO_ROOM);
		act("You create $p.", FALSE, ch, obj, 0, TO_CHAR);
		load_otrigger(obj);
	}
	else if (is_abbrev(buf, "vehicle")) {
		if (!vehicle_proto(number)) {
			msg_to_char(ch, "There is no vehicle with that number.\r\n");
			return;
		}
		veh = read_vehicle(number, TRUE);
		vehicle_to_room(veh, IN_ROOM(ch));
		scale_vehicle_to_level(veh, 0);	// attempt auto-detect of level
		act("$n makes an odd magical gesture.", TRUE, ch, NULL, NULL, TO_ROOM);
		act("$n has created $V!", FALSE, ch, NULL, veh, TO_ROOM);
		act("You create $V.", FALSE, ch, NULL, veh, TO_CHAR);
		load_vtrigger(veh);
		
		if ((mort = find_mortal_in_room(IN_ROOM(ch)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s loaded vehicle %s with mortal present (%s) at %s", GET_NAME(ch), VEH_SHORT_DESC(veh), GET_NAME(mort), room_log_identifier(IN_ROOM(ch)));
		}
		else if (ROOM_OWNER(IN_ROOM(ch)) && !EMPIRE_IMM_ONLY(ROOM_OWNER(IN_ROOM(ch)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s loaded vehicle %s in mortal empire (%s) at %s", GET_NAME(ch), VEH_SHORT_DESC(veh), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))), room_log_identifier(IN_ROOM(ch)));
		}
	}
	else {
		send_to_char("That'll have to be either 'obj', 'mob', or 'vehicle'.\r\n", ch);
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
	struct empire_storage_data *store, *next_store;
	struct empire_island *eisle;
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
	else if (!is_number(arg2) || (island_from = atoi(arg2)) < -1) {
		msg_to_char(ch, "Invalid from-island '%s'.\r\n", arg2);
	}
	else if (!is_number(arg3) || (island_to = atoi(arg3)) < -1) {
		msg_to_char(ch, "Invalid to-island '%s'.\r\n", arg2);
	}
	else if (island_to == island_from) {
		msg_to_char(ch, "Those are the same island.\r\n");
	}
	else {
		count = 0;
		eisle = get_empire_island(emp, island_from);
		HASH_ITER(hh, eisle->store, store, next_store) {
			count += store->amount;
			add_to_empire_storage(emp, island_to, store->vnum, store->amount);
			HASH_DEL(eisle->store, store);
			free(store);
		}
		for (unique = EMPIRE_UNIQUE_STORAGE(emp); unique; unique = unique->next) {
			if (unique->island == island_from) {
				unique->island = island_to;
				count += unique->amount;
				
				// does not consolidate
			}
		}
		
		if (count != 0) {
			EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has moved %d einv items for %s from island %d to island %d", GET_REAL_NAME(ch), count, EMPIRE_NAME(emp), island_from, island_to);
			msg_to_char(ch, "Moved %d items for %s%s&0 from island %d to island %d.\r\n", count, EMPIRE_BANNER(emp), EMPIRE_NAME(emp), island_from, island_to);
		}
		else {
			msg_to_char(ch, "No items to move.\r\n");
		}
	}
}


ACMD(do_oset) {
	char obj_arg[MAX_INPUT_LENGTH], field_arg[MAX_INPUT_LENGTH];
	obj_data *obj, *proto;
	
	argument = one_argument(argument, obj_arg);
	argument = any_one_arg(argument, field_arg);
	skip_spaces(&argument);	// remainder
	
	if (!*obj_arg || !*field_arg) {
		msg_to_char(ch, "Usage: oset <object> <field> <value>\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, obj_arg, ch->carrying)) && !(obj = get_obj_in_list_vis(ch, obj_arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(obj_arg), obj_arg);
	}
	else if (is_abbrev(field_arg, "flags")) {
		GET_OBJ_EXTRA(obj) = olc_process_flag(ch, argument, "extra", "oset name flags", extra_bits, GET_OBJ_EXTRA(obj));
	}
	else if (is_abbrev(field_arg, "keywords") || is_abbrev(field_arg, "aliases")) {
		if (!*argument) {
			msg_to_char(ch, "Set the keywords to what?\r\n");
		}
		else {
			proto = obj_proto(GET_OBJ_VNUM(obj));
			
			if (GET_OBJ_KEYWORDS(obj) && (!proto || GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(proto))) {
				free(GET_OBJ_KEYWORDS(obj));
			}
			if (proto && !str_cmp(argument, "none")) {
				GET_OBJ_KEYWORDS(obj) = GET_OBJ_KEYWORDS(proto);
				msg_to_char(ch, "You restore its original keywords.\r\n");
			}
			else {
				GET_OBJ_KEYWORDS(obj) = str_dup(argument);
				msg_to_char(ch, "You change its keywords to '%s'.\r\n", GET_OBJ_KEYWORDS(obj));
			}
		}
	}
	else if (is_abbrev(field_arg, "longdescription")) {
		if (!*argument) {
			msg_to_char(ch, "Set the long description to what?\r\n");
		}
		else {
			proto = obj_proto(GET_OBJ_VNUM(obj));
			
			if (GET_OBJ_LONG_DESC(obj) && (!proto || GET_OBJ_LONG_DESC(obj) != GET_OBJ_LONG_DESC(proto))) {
				free(GET_OBJ_LONG_DESC(obj));
			}
			if (proto && !str_cmp(argument, "none")) {
				GET_OBJ_LONG_DESC(obj) = GET_OBJ_LONG_DESC(proto);
				msg_to_char(ch, "You restore its original long description.\r\n");
			}
			else {
				GET_OBJ_LONG_DESC(obj) = str_dup(argument);
				msg_to_char(ch, "You change its long description to '%s'.\r\n", GET_OBJ_LONG_DESC(obj));
			}
		}
	}
	else if (is_abbrev(field_arg, "shortdescription")) {
		if (!*argument) {
			msg_to_char(ch, "Set the short description to what?\r\n");
		}
		else {
			proto = obj_proto(GET_OBJ_VNUM(obj));
			
			if (GET_OBJ_SHORT_DESC(obj) && (!proto || GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(proto))) {
				free(GET_OBJ_SHORT_DESC(obj));
			}
			if (proto && !str_cmp(argument, "none")) {
				GET_OBJ_SHORT_DESC(obj) = GET_OBJ_SHORT_DESC(proto);
				msg_to_char(ch, "You restore its original short description.\r\n");
			}
			else {
				GET_OBJ_SHORT_DESC(obj) = str_dup(argument);
				msg_to_char(ch, "You change its short description to '%s'.\r\n", GET_OBJ_SHORT_DESC(obj));
			}
		}
	}
	else if (is_abbrev(field_arg, "timer")) {
		if (!*argument || (!isdigit(*argument) && *argument != '-')) {
			msg_to_char(ch, "Set the timer to what?\r\n");
		}
		else {
			GET_OBJ_TIMER(obj) = atoi(argument);
			msg_to_char(ch, "You change its timer to %d.\r\n", GET_OBJ_TIMER(obj));
		}
	}
	else {
		msg_to_char(ch, "Invalid field.\r\n");
	}
}


ACMD(do_peace) {
	struct txt_block *inq, *next_inq;
	char_data *iter, *next_iter;
	
	LL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_iter, next_in_room) {
		if (FIGHTING(iter) || GET_POS(iter) == POS_FIGHTING) {
			stop_fighting(iter);
		}
		
		if (iter != ch && iter->desc) {
			LL_FOREACH_SAFE(iter->desc->input.head, inq, next_inq) {
				LL_DELETE(iter->desc->input.head, inq);
				free(inq->text);
				free(inq);
			}
		}
	}
	
	if (find_mortal_in_room(IN_ROOM(ch))) {
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s used peace with mortal present at %s", GET_NAME(ch), room_log_identifier(IN_ROOM(ch)));
	}
	
	act("You raise your hands and a feeling of peace sweeps over the room.", FALSE, ch, NULL, NULL, TO_CHAR);
	act("$n raises $s hands and a feeling of peace enters your heart.", FALSE, ch, NULL, NULL, TO_ROOM);
}


ACMD(do_playerdelete) {
	void delete_player_character(char_data *ch);
	
	descriptor_data *desc, *next_desc;
	char name[MAX_INPUT_LENGTH];
	char_data *victim = NULL;
	bool file = FALSE;
	
	argument = any_one_arg(argument, name);
	skip_spaces(&argument);
	
	if (!*argument || !*name) {
		msg_to_char(ch, "Usage: playerdelete <name> CONFIRM\r\n");
	}
	else if (!(victim = find_or_load_player(name, &file))) {
		msg_to_char(ch, "Unknown player '%s'.\r\n", name);
	}
	else if (GET_ACCESS_LEVEL(victim) >= GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "No, no, no, no, no, no, no.\r\n");
	}
	else if (PLR_FLAGGED(victim, PLR_NODELETE)) {
		msg_to_char(ch, "You cannot delete that player because of a !DEL player flag.\r\n");
	}
	else if (strcmp(argument, "CONFIRM")) {
		msg_to_char(ch, "You must type the word CONFIRM, in all caps, after the target name.\r\n");
		msg_to_char(ch, "WARNING: This will permanently delete the character.\r\n");
	}
	else {
		// logs and messaging
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has deleted player %s", GET_NAME(ch), GET_NAME(victim));
		if (!file) {
			if (!GET_INVIS_LEV(victim)) {
				act("$n has left the game.", TRUE, victim, FALSE, FALSE, TO_ROOM);
			}
			if (GET_INVIS_LEV(victim) == 0) {
				if (config_get_bool("public_logins")) {
					mortlog("%s has left the game", PERS(victim, victim, TRUE));
				}
				else if (GET_LOYALTY(victim)) {
					log_to_empire(GET_LOYALTY(victim), ELOG_LOGINS, "%s has left the game", PERS(victim, victim, TRUE));
				}
			}
			msg_to_char(victim, "Your character has been deleted. Goodbye...\r\n");
		}
		
		// look for this character at a menu, in case
		for (desc = descriptor_list; desc; desc = next_desc) {
			next_desc = desc->next;
			
			if (desc->character && desc->character != victim && GET_IDNUM(desc->character) == GET_IDNUM(victim)) {
				msg_to_desc(desc, "Your character has been deleted. Goodbye...\r\n");
				if (STATE(desc) == CON_PLAYING) {
					STATE(desc) = CON_DISCONNECT;
				}
				else {
					STATE(desc) = CON_CLOSE;
				}
			}
		}
		
		// actual delete (remove items first)
		extract_all_items(victim);
		delete_player_character(victim);
		extract_char(victim);
		victim = NULL;	// prevent cleanup
	}
	
	// cleanup
	if (victim && file) {
		free_char(victim);
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
	void deliver_shipment(empire_data *emp, struct shipping_data *shipd);
	
	struct shipping_data *shipd, *next_shipd;
	char_data *vict, *next_v;
	vehicle_data *veh;
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
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has purged %s", GET_NAME(ch), GET_NAME(vict));
				SAVE_CHAR(vict);
				if (vict->desc) {
					STATE(vict->desc) = CON_CLOSE;
					vict->desc->character = NULL;
					vict->desc = NULL;
				}
				
				extract_all_items(vict);	// otherwise it duplicates items
			}
			extract_char(vict);
		}
		else if ((obj = get_obj_in_list_vis(ch, buf, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL) {
			act("$n destroys $p.", FALSE, ch, obj, 0, TO_ROOM);
			extract_obj(obj);
		}
		else if ((veh = get_vehicle_in_room_vis(ch, buf))) {
			// finish the shipment before transferring or purging a vehicle
			if (VEH_OWNER(veh) && VEH_SHIPPING_ID(veh) != -1) {
				LL_FOREACH_SAFE(EMPIRE_SHIPPING_LIST(VEH_OWNER(veh)), shipd, next_shipd) {
					if (shipd->shipping_id == VEH_SHIPPING_ID(veh)) {
						deliver_shipment(VEH_OWNER(veh), shipd);
					}
				}
				VEH_SHIPPING_ID(veh) = -1;
			}
			
			act("$n destroys $V.", FALSE, ch, NULL, veh, TO_ROOM);
			if (IN_ROOM(veh) != IN_ROOM(ch) && ROOM_PEOPLE(IN_ROOM(veh))) {
				act("$V is destroyed!", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM);
			}
			extract_vehicle(veh);
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

		while (ROOM_CONTENTS(IN_ROOM(ch))) {
			extract_obj(ROOM_CONTENTS(IN_ROOM(ch)));
		}
	}
}


ACMD(do_random) {
	struct map_data *map;
	int tries;
	room_vnum roll;
	room_data *loc;
	
	// looks for a random non-ocean location
	for (tries = 0; tries < 100; ++tries) {
		roll = number(0, MAP_SIZE - 1);
		map = &world_map[MAP_X_COORD(roll)][MAP_Y_COORD(roll)];
		
		if (GET_SECT_VNUM(map->sector_type) == BASIC_OCEAN) {
			continue;
		}
		
		if ((loc = real_room(roll)) && !ROOM_IS_CLOSED(loc)) {
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
	int type, time = 0, var;
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
	
	if (subcmd == SCMD_REBOOT) {
		reboot_control.level = SHUTDOWN_NORMAL;	// prevent a reboot from using a shutdown type
	}

	if (reboot_control.immediate) {
		msg_to_char(ch, "Preparing for imminent %s...\r\n", reboot_type[reboot_control.type]);
	}
	else {
		var = (no_arg ? reboot_control.time : time);
		msg_to_char(ch, "The %s is set for %d minute%s (%s).\r\n", reboot_type[reboot_control.type], var, PLURAL(var), shutdown_types[reboot_control.level]);
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
	void load_data_table();
	void load_intro_screens();
	extern char *credits;
	extern char *motd;
	extern char *news;
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
		file_to_string_alloc(NEWS_FILE, &news);
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
	else if (!str_cmp(arg, "news"))
		file_to_string_alloc(NEWS_FILE, &news);
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
	else if (!str_cmp(arg, "data")) {
		load_data_table();
	}
	else {
		send_to_char("Unknown reload option.\r\n", ch);
		return;
	}

	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has reloaded: %s", GET_NAME(ch), arg);
	send_config_msg(ch, "ok_string");
}


ACMD(do_rescale) {
	void scale_item_to_level(obj_data *obj, int level);
	
	vehicle_data *veh;
	obj_data *obj, *new;
	char_data *vict;
	int level;

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
			SET_BIT(MOB_FLAGS(vict), MOB_NO_RESCALE);
			
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
	else if ((veh = get_vehicle_in_room_vis(ch, arg))) {
		scale_vehicle_to_level(veh, level);
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has rescaled vehicle %s to level %d at %s", GET_NAME(ch), VEH_SHORT_DESC(veh), VEH_SCALE_LEVEL(veh), room_log_identifier(IN_ROOM(ch)));
		sprintf(buf, "You rescale $V to level %d.", VEH_SCALE_LEVEL(veh));
		act(buf, FALSE, ch, NULL, veh, TO_CHAR);
		act("$n rescales $V.", FALSE, ch, NULL, veh, TO_ROOM);
	}
	else if ((obj = get_obj_in_list_vis(ch, arg, ch->carrying)) || (obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		// item mode
		if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			scale_item_to_level(obj, level);
		}
		else {
			new = fresh_copy_obj(obj, level);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
			obj = new;
		}
		
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has rescaled obj %s to level %d at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), GET_OBJ_CURRENT_SCALE_LEVEL(obj), room_log_identifier(IN_ROOM(ch)));
		sprintf(buf, "You rescale $p to level %d.", GET_OBJ_CURRENT_SCALE_LEVEL(obj));
		act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		act("$n rescales $p.", FALSE, ch, obj, NULL, TO_ROOM);
	}
	else {
		msg_to_char(ch, "You don't see %s %s here.\r\n", AN(arg), arg);
	}
}


ACMD(do_restore) {
	void add_ability_by_set(char_data *ch, ability_data *abil, int skill_set, bool reset_levels);
	
	char name_arg[MAX_INPUT_LENGTH], *type_args, arg[MAX_INPUT_LENGTH], msg[MAX_STRING_LENGTH], types[MAX_STRING_LENGTH];
	ability_data *abil, *next_abil;
	skill_data *skill, *next_skill;
	struct cooldown_data *cool;
	empire_data *emp;
	char_data *vict;
	int i, iter;
	
	// modes
	bool all = FALSE, blood = FALSE, cds = FALSE, dots = FALSE, drunk = FALSE, health = FALSE, hunger = FALSE, mana = FALSE, moves = FALSE, thirst = FALSE;

	type_args = one_argument(argument, name_arg);
	skip_spaces(&type_args);
	
	if (!*name_arg) {
		send_to_char("Whom do you wish to restore?\r\n", ch);
		return;
	}
	else if (!(vict = get_char_vis(ch, name_arg, FIND_CHAR_WORLD))) {
		send_config_msg(ch, "no_person");
		return;
	}
	
	// parse type args
	if (!*type_args) {
		all = TRUE;
	}
	else {
		all = FALSE;
		while (*type_args) {
			type_args = any_one_arg(type_args, arg);
			skip_spaces(&type_args);
			
			if (is_abbrev(arg, "all") || is_abbrev(arg, "full")) {
				all = TRUE;
			}
			else if (is_abbrev(arg, "blood")) {
				blood = TRUE;
			}
			else if (is_abbrev(arg, "cooldowns")) {
				cds = TRUE;
			}
			else if (is_abbrev(arg, "dots")) {
				dots = TRUE;
			}
			else if (is_abbrev(arg, "drunkenness")) {
				drunk = TRUE;
			}
			else if (is_abbrev(arg, "health") || is_abbrev(arg, "hitpoints")) {
				health = TRUE;
			}
			else if (is_abbrev(arg, "hunger")) {
				hunger = TRUE;
			}
			else if (is_abbrev(arg, "mana")) {
				mana = TRUE;
			}
			else if (is_abbrev(arg, "moves") || is_abbrev(arg, "movement") || is_abbrev(arg, "vitality")) {
				// "vitality" catches "v" on "restore <name> h m v"
				moves = TRUE;
			}
			else if (is_abbrev(arg, "thirsty")) {
				thirst = TRUE;
			}
			else {
				msg_to_char(ch, "Unknown restore type '%s'.\r\n", arg);
				return;
			}
		}
	}
	
	// OK: setup default messages
	*types = '\0';
	if (ch == vict) {
		if (all) {
			strcpy(msg, "You have fully restored yourself!");
		}
		else {
			strcpy(msg, "You have restored your");
		}
	}
	else {
		if (all) {
			strcpy(msg, "You have been fully restored by $N!");
		}
		else {
			strcpy(msg, "$N has restored your");
		}
	}
	
	// OK: do the work
	
	// fill pools
	if (all || health) {
		GET_HEALTH(vict) = GET_MAX_HEALTH(vict);
		
		if (GET_POS(vict) < POS_SLEEPING) {
			GET_POS(vict) = POS_STANDING;
		}
		
		update_pos(vict);
		sprintf(types + strlen(types), "%s health", *types ? "," : "");
	}
	if (all || mana) {
		GET_MANA(vict) = GET_MAX_MANA(vict);
		sprintf(types + strlen(types), "%s mana", *types ? "," : "");
	}
	if (all || moves) {
		GET_MOVE(vict) = GET_MAX_MOVE(vict);
		sprintf(types + strlen(types), "%s moves", *types ? "," : "");
	}
	if (all || blood) {
		GET_BLOOD(vict) = GET_MAX_BLOOD(vict);
		sprintf(types + strlen(types), "%s blood", *types ? "," : "");
	}
	
	// remove DoTs
	if (all || dots) {
		while (vict->over_time_effects) {
			dot_remove(vict, vict->over_time_effects);
		}
		
		sprintf(types + strlen(types), "%s DoTs", *types ? "," : "");
	}
	
	// remove all cooldowns
	if (all || cds) {
		while ((cool = vict->cooldowns)) {
			vict->cooldowns = cool->next;
			free(cool);
		}
		
		sprintf(types + strlen(types), "%s cooldowns", *types ? "," : "");
	}
	
	// conditions
	if (all || hunger) {
		if (!IS_NPC(vict) && GET_COND(vict, FULL) != UNLIMITED) {
			GET_COND(vict, FULL) = 0;
		}
		sprintf(types + strlen(types), "%s hunger", *types ? "," : "");
	}
	if (all || thirst) {
		if (!IS_NPC(vict) && GET_COND(vict, THIRST) != UNLIMITED) {
			GET_COND(vict, THIRST) = 0;
		}
		sprintf(types + strlen(types), "%s thirst", *types ? "," : "");
	}
	if (all || drunk) {
		if (!IS_NPC(vict) && GET_COND(vict, DRUNK) != UNLIMITED) {
			GET_COND(vict, DRUNK) = 0;
		}
		sprintf(types + strlen(types), "%s drunkenness", *types ? "," : "");
	}

	if (all && !IS_NPC(vict) && (GET_ACCESS_LEVEL(ch) >= LVL_GOD) && (GET_ACCESS_LEVEL(vict) >= LVL_GOD)) {
		for (i = 0; i < NUM_CONDS; i++)
			GET_COND(vict, i) = UNLIMITED;

		for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
			vict->real_attributes[iter] = att_max(vict);
		}
		
		HASH_ITER(hh, skill_table, skill, next_skill) {
			set_skill(vict, SKILL_VNUM(skill), SKILL_MAX_LEVEL(skill));
		}
		update_class(vict);
		assign_class_abilities(vict, NULL, NOTHING);
		
		// temporarily remove empire abilities
		emp = GET_LOYALTY(vict);
		if (emp) {
			adjust_abilities_to_empire(vict, emp, FALSE);
		}
		
		HASH_ITER(hh, ability_table, abil, next_abil) {
			// add abilities to set 0
			add_ability_by_set(vict, abil, 0, TRUE);
		}

		affect_total(vict);
		
		// re-add abilities
		if (emp) {
			adjust_abilities_to_empire(vict, emp, TRUE);
		}
	}
	
	if (ch != vict) {
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has restored %s:%s", GET_REAL_NAME(ch), GET_REAL_NAME(vict), all ? " full restore" : types);
	}
	
	send_config_msg(ch, "ok_string");
	
	if (!all) {
		sprintf(msg + strlen(msg), "%s!", types);
	}
	act(msg, FALSE, vict, NULL, ch, TO_CHAR);
}


ACMD(do_return) {
	if (ch->desc && ch->desc->original) {
		syslog(SYS_GC, GET_INVIS_LEV(ch->desc->original), TRUE, "GC: %s has returned to %s original body", GET_REAL_NAME(ch->desc->original), REAL_HSHR(ch->desc->original));
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
		send_to_char("Send what to whom?\r\n", ch);
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
	char field[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	bool is_file = FALSE, load_from_file = FALSE;
	int mode, len, retval;
	char_data *vict = NULL;
	char is_player = 0;

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
		if (!(vict = find_or_load_player(name, &load_from_file))) {
			send_to_char("There is no such player.\r\n", ch);
			return;
		}
		if (GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch)) {
			send_to_char("Sorry, you can't do that.\r\n", ch);
			if (load_from_file) {
				free_char(vict);
			}
			return;
		}
		
		// some sets need the delay file
		check_delayed_load(vict);
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
		if (load_from_file) {
			store_loaded_char(vict);
			send_to_char("Saved in file.\r\n", ch);
		}
		else if (!IS_NPC(vict)) {
			SAVE_CHAR(vict);
		}
	}
	else if (load_from_file) {
		free_char(vict);
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
		{ "buildings", LVL_START_IMM, show_buildings },
		{ "commons", LVL_ASST, show_commons },
		{ "crops", LVL_START_IMM, show_crops },
		{ "players", LVL_START_IMM, show_players },
		{ "terrain", LVL_START_IMM, show_terrain },
		{ "account", LVL_TO_SEE_ACCOUNTS, show_account },
		{ "notes", LVL_START_IMM, show_notes },
		{ "ammotypes", LVL_START_IMM, show_ammotypes },
		{ "skills", LVL_START_IMM, show_skills },
		{ "storage", LVL_START_IMM, show_storage },
		{ "startlocs", LVL_START_IMM, show_startlocs },
		{ "spawns", LVL_START_IMM, show_spawns },
		{ "ignoring", LVL_START_IMM, show_ignoring },
		{ "workforce", LVL_START_IMM, show_workforce },
		{ "islands", LVL_START_IMM, show_islands },
		{ "variables", LVL_START_IMM, show_variables },
		{ "components", LVL_START_IMM, show_components },
		{ "quests", LVL_START_IMM, show_quests },
		{ "unlearnable", LVL_START_IMM, show_unlearnable },
		{ "uses", LVL_START_IMM, show_uses },
		{ "factions", LVL_START_IMM, show_factions },
		{ "dailycycle", LVL_START_IMM, show_dailycycle },
		{ "data", LVL_CIMPL, show_data },
		{ "minipets", LVL_START_IMM, show_minipets },
		{ "mounts", LVL_START_IMM, show_mounts },
		{ "learned", LVL_START_IMM, show_learned },
		{ "currency", LVL_START_IMM, show_currency },
		{ "technology", LVL_START_IMM, show_technology },
		{ "shops", LVL_START_IMM, show_shops },
		{ "piles", LVL_CIMPL, show_piles },
		{ "progress", LVL_START_IMM, show_progress },
		{ "progression", LVL_START_IMM, show_progression },
		{ "produced", LVL_START_IMM, show_produced },
		{ "resource", LVL_START_IMM, show_resource },
		{ "olc", LVL_START_IMM, show_olc },

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

	half_chop(argument, field, value);

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
	extern bool check_scaling(char_data *mob, char_data *attacker);
	extern obj_data *die(char_data *ch, char_data *killer);
	
	char_data *vict;

	one_argument(argument, arg);

	if (!*arg)
		send_to_char("Slay whom?\r\n", ch);
	else {
		if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
			send_to_char("They aren't here.\r\n", ch);
		else if (ch == vict)
			send_to_char("Your mother would be so sad... :(\r\n", ch);
		else if (!IS_NPC(vict) && GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch)) {
			act("Surely you don't expect $N to let you slay $M, do you?", FALSE, ch, NULL, vict, TO_CHAR);
		}
		else {
			if (!IS_NPC(vict) && !affected_by_spell(ch, ATYPE_PHOENIX_RITE)) {
				syslog(SYS_GC | SYS_DEATH, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has slain %s at %s", GET_REAL_NAME(ch), GET_REAL_NAME(vict), room_log_identifier(IN_ROOM(vict)));
				log_to_slash_channel_by_name(DEATH_LOG_CHANNEL, NULL, "%s has been slain at (%d, %d)", PERS(vict, vict, TRUE), X_COORD(IN_ROOM(vict)), Y_COORD(IN_ROOM(vict)));
			}
			
			act("You chop $M to pieces! Ah! The blood!", FALSE, ch, 0, vict, TO_CHAR);
			act("$N chops you to pieces!", FALSE, vict, 0, ch, TO_CHAR);
			act("$n brutally slays $N!", FALSE, ch, 0, vict, TO_NOTVICT);

			check_scaling(vict, ch);	// ensure scaling
			tag_mob(vict, ch);	// ensures loot binding if applicable

			// this would prevent the death
			if (affected_by_spell(vict, ATYPE_PHOENIX_RITE)) {
				affect_from_char(vict, ATYPE_PHOENIX_RITE, FALSE);
			}

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
		send_to_char("There's no link... nothing to snoop.\r\n", ch);
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
	char_data *victim = NULL;
	vehicle_data *veh;
	empire_data *emp;
	crop_data *cp;
	obj_data *obj;
	bool file = FALSE;
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
			do_stat_adventure(ch, INST_ADVENTURE(COMPLEX_DATA(IN_ROOM(ch))->instance));
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
		if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_HAS_CROP_DATA) && (cp = ROOM_CROP(IN_ROOM(ch)))) {
			do_stat_crop(ch, cp);
		}
		else {
			msg_to_char(ch, "You are not on a crop tile.\r\n");
		}
	}
	else if (!strn_cmp(buf1, "emp", 3) && is_abbrev(buf1, "empire")) {
		if ((emp = get_empire_by_name(buf2))) {
			do_stat_empire(ch, emp);
		}
		else if (!*buf2) {
			msg_to_char(ch, "Get stats on which empire?\r\n");
		}
		else {
			msg_to_char(ch, "Unknown empire.\r\n");
		}
	}
	else if (!strn_cmp(buf1, "sect", 4) && is_abbrev(buf1, "sector")) {
		do_stat_sector(ch, SECT(IN_ROOM(ch)));
	}
	else if (is_abbrev(buf1, "mob")) {
		if (!*buf2)
			send_to_char("Stats on which mobile?\r\n", ch);
		else {
			if ((victim = get_char_vis(ch, buf2, FIND_CHAR_WORLD | FIND_NPC_ONLY)) != NULL)
				do_stat_character(ch, victim);
			else
				send_to_char("No such mobile around.\r\n", ch);
		}
	}
	else if (is_abbrev(buf1, "vehicle")) {
		if (!*buf2)
			send_to_char("Stats on which vehicle?\r\n", ch);
		else {
			if ((veh = get_vehicle_vis(ch, buf2)) != NULL) {
				do_stat_vehicle(ch, veh);
			}
			else {
				send_to_char("No such vehicle around.\r\n", ch);
			}
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
		else if (!(victim = find_or_load_player(buf2, &file))) {
			send_to_char("There is no such player.\r\n", ch);
		}
		else if (GET_ACCESS_LEVEL(victim) > GET_ACCESS_LEVEL(ch)) {
			send_to_char("Sorry, you can't do that.\r\n", ch);
		}
		else {
			do_stat_character(ch, victim);
		}
		
		if (victim && file) {
			free_char(victim);
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
		else if ((veh = get_vehicle_in_room_vis(ch, buf1)) || (veh = get_vehicle_vis(ch, buf1))) {
			do_stat_vehicle(ch, veh);
		}
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
		send_to_char("Switch with whom?\r\n", ch);
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
	start_string_editor(ch->desc, "file", tedit_option[l].buffer, tedit_option[l].size, FALSE);
	ch->desc->file_storage = str_dup(tedit_option[l].filename);
	act("$n begins editing a file.", TRUE, ch, 0, 0, TO_ROOM);
}


// do_transfer <- search alias because why is it called do_trans? -pc
ACMD(do_trans) {
	struct shipping_data *shipd, *next_shipd;
	descriptor_data *i;
	char_data *victim;
	room_data *to_room = IN_ROOM(ch);
	vehicle_data *veh;

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
	else if (!str_cmp(buf, "all")) {			/* Trans All */
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
				qt_visit_room(victim, IN_ROOM(victim));
				look_at_room(victim);
				enter_wtrigger(IN_ROOM(victim), victim, NO_DIR);
				msdp_update_room(victim);	// once we're sure we're staying
			}
		}
		
		send_config_msg(ch, "ok_string");
	}
	else if ((victim = get_char_vis(ch, buf, FIND_CHAR_WORLD))) {
		if (victim == ch)
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
			qt_visit_room(victim, IN_ROOM(victim));
			look_at_room(victim);
			enter_wtrigger(IN_ROOM(victim), victim, NO_DIR);
			msdp_update_room(victim);	// once we're sure we're staying
			send_config_msg(ch, "ok_string");
		}
	}
	else if ((veh = get_vehicle_vis(ch, buf))) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has transferred %s to %s", GET_REAL_NAME(ch), VEH_SHORT_DESC(veh), room_log_identifier(to_room));
	
		// finish the shipment before transferring
		if (VEH_OWNER(veh) && VEH_SHIPPING_ID(veh) != -1) {
			LL_FOREACH_SAFE(EMPIRE_SHIPPING_LIST(VEH_OWNER(veh)), shipd, next_shipd) {
				if (shipd->shipping_id == VEH_SHIPPING_ID(veh)) {
					deliver_shipment(VEH_OWNER(veh), shipd);
				}
			}
			VEH_SHIPPING_ID(veh) = -1;
		}
		
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			act("$V disappears in a mushroom cloud.", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM);
		}
		
		adjust_vehicle_tech(veh, FALSE);
		vehicle_from_room(veh);
		vehicle_to_room(veh, to_room);
		adjust_vehicle_tech(veh, TRUE);
		
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			act("$V arrives from a puff of smoke.", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM);
		}
		send_config_msg(ch, "ok_string");
	}
	else {
		send_config_msg(ch, "no_person");
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


ACMD(do_unprogress) {
	void refresh_empire_goals(empire_data *emp, any_vnum only_vnum);
	void remove_completed_goal(empire_data *emp, any_vnum vnum);
	
	char emp_arg[MAX_INPUT_LENGTH], prg_arg[MAX_INPUT_LENGTH];
	empire_data *emp, *next_emp, *only = NULL;
	struct empire_goal *goal;
	progress_data *prg;
	int count;
	bool all;
	
	argument = any_one_word(argument, emp_arg);
	argument = any_one_arg(argument, prg_arg);
	all = !str_cmp(emp_arg, "all");
	
	if (!*emp_arg && !*prg_arg) {
		msg_to_char(ch, "Usage: unprogress <empire | all> <goal vnum>\r\n");
	}
	else if (!all && !(only = get_empire_by_name(emp_arg))) {
		msg_to_char(ch, "Invalid empire '%s'.\r\n", emp_arg);
	}
	else if (!isdigit(*prg_arg) || !(prg = real_progress(atoi(prg_arg)))) {
		msg_to_char(ch, "Invalid progression vnum '%s'.\r\n", prg_arg);
	}
	else {
		count = 0;
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if (!all && emp != only) {
				continue;	// only doing one?
			}
			
			// found current goal?
			if ((goal = get_current_goal(emp, PRG_VNUM(prg)))) {
				++count;
				cancel_empire_goal(emp, goal);
			}
			
			// found completed goal?
			if (empire_has_completed_goal(emp, PRG_VNUM(prg))) {
				remove_completed_goal(emp, PRG_VNUM(prg));
				++count;
			}
			
			refresh_empire_goals(emp, NOTHING);
		}
		
		if (all) {
			if (count < 1) {
				msg_to_char(ch, "No empires have that goal.\r\n");
			}
			else {
				msg_to_char(ch, "Goal removed from %d empire%s.\r\n", count, PLURAL(count));
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has removed progress goal %d from %d empire%s", GET_REAL_NAME(ch), PRG_VNUM(prg), count, PLURAL(count));
			}
		}
		else if (count < 1) {
			msg_to_char(ch, "%s does not have that goal.\r\n", only ? EMPIRE_NAME(only) : "UNKNOWN");
		}
		else {
			msg_to_char(ch, "Goal removed from %s.\r\n", only ? EMPIRE_NAME(only) : "UNKNOWN");
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has removed progress goal %d from %s", GET_REAL_NAME(ch), PRG_VNUM(prg), only ? EMPIRE_NAME(only) : "UNKNOWN");
		}
	}
}


ACMD(do_unquest) {
	void drop_quest(char_data *ch, struct player_quest *pq);
	
	struct player_completed_quest *pcq, *next_pcq;
	struct player_quest *pq, *next_pq;
	char arg[MAX_INPUT_LENGTH];
	quest_data *quest;
	char_data *vict;
	bool found;
	
	argument = one_argument(argument, arg);
	skip_spaces(&argument);	// vnum
	
	if (!*arg || !*argument || !isdigit(*argument)) {
		msg_to_char(ch, "Usage: unquest <target> <quest vnum>\r\n");
	}
	else if (!(vict = get_player_vis(ch, arg, FIND_CHAR_WORLD)) || IS_NPC(vict)) {
		send_config_msg(ch, "no_person");
	}
	else if (GET_ACCESS_LEVEL(vict) > GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "You simply can't do that.\r\n");
	}
	else if (!(quest = quest_proto(atoi(argument)))) {
		msg_to_char(ch, "Invalid quest vnum.\r\n");
	}
	else {
		found = FALSE;
		
		// remove from active quests
		LL_FOREACH_SAFE(GET_QUESTS(vict), pq, next_pq) {
			if (pq->vnum == QUEST_VNUM(quest)) {
				drop_quest(vict, pq);
				found = TRUE;
			}
		}
		
		// remove from completed quests
		HASH_ITER(hh, GET_COMPLETED_QUESTS(vict), pcq, next_pcq) {
			if (pcq->vnum == QUEST_VNUM(quest)) {
				HASH_DEL(GET_COMPLETED_QUESTS(vict), pcq);
				free(pcq);
				found = TRUE;
			}
		}
		
		if (ch == vict) {
			if (found) {
				// no need to syslog for self
				msg_to_char(ch, "You remove [%d] %s from your quest lists.\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
			}
			else {
				msg_to_char(ch, "You are not on that quest.\r\n");
			}
		}
		else {	// ch != vict
			if (found) {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has removed [%d] %s from %s's quest lists", GET_NAME(ch), QUEST_VNUM(quest), QUEST_NAME(quest), GET_NAME(vict));
				msg_to_char(ch, "You remove [%d] %s from %s's quest lists.\r\n", QUEST_VNUM(quest), QUEST_NAME(quest), PERS(vict, ch, TRUE));
			}
			else {
				msg_to_char(ch, "%s is not on that quest.\r\n", PERS(vict, ch, TRUE));
			}
		}
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
	char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
	
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
	else if (is_abbrev(buf, "adventure")) {	// name precedence for "vnum a"
		if (!vnum_adventure(buf2, ch)) {
			msg_to_char(ch, "No adventures by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "ability")) {
		extern int vnum_ability(char *searchname, char_data *ch);
		if (!vnum_ability(buf2, ch)) {
			msg_to_char(ch, "No abilities by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "archetype")) {
		extern int vnum_archetype(char *searchname, char_data *ch);
		if (!vnum_archetype(buf2, ch)) {
			msg_to_char(ch, "No archetypes by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "augment")) {
		extern int vnum_augment(char *searchname, char_data *ch);
		if (!vnum_augment(buf2, ch)) {
			msg_to_char(ch, "No augments by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "building")) {
		if (!vnum_building(buf2, ch)) {
			msg_to_char(ch, "No buildings by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "book")) {
		if (!vnum_book(buf2, ch)) {
			send_to_char("No books by that name.\r\n", ch);
		}
	}
	else if (is_abbrev(buf, "class")) {
		extern int vnum_class(char *searchname, char_data *ch);
		if (!vnum_class(buf2, ch)) {
			msg_to_char(ch, "No classes by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "crop")) {
		if (!vnum_crop(buf2, ch)) {
			msg_to_char(ch, "No crops by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "event")) {
		extern int vnum_event(char *searchname, char_data *ch);
		if (!vnum_event(buf2, ch)) {
			msg_to_char(ch, "No events by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "faction")) {
		extern int vnum_faction(char *searchname, char_data *ch);
		if (!vnum_faction(buf2, ch)) {
			msg_to_char(ch, "No factions by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "global")) {	// takes precedence on 'g'
		if (!vnum_global(buf2, ch)) {
			msg_to_char(ch, "No globals by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "generic")) {
		extern int vnum_generic(char *searchname, char_data *ch);
		if (!vnum_generic(buf2, ch)) {
			msg_to_char(ch, "No generics by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "morph")) {
		extern int vnum_morph(char *searchname, char_data *ch);
		if (!vnum_morph(buf2, ch)) {
			msg_to_char(ch, "No morphs by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "progression")) {
		extern int vnum_progress(char *searchname, char_data *ch);
		if (!vnum_progress(buf2, ch)) {
			msg_to_char(ch, "No progression goals by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "quest")) {
		extern int vnum_quest(char *searchname, char_data *ch);
		if (!vnum_quest(buf2, ch)) {
			msg_to_char(ch, "No quests by that name.\r\n");
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
	else if (is_abbrev(buf, "shop")) {
		extern int vnum_shop(char *searchname, char_data *ch);
		if (!vnum_shop(buf2, ch)) {
			msg_to_char(ch, "No shops by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "skill")) {
		extern int vnum_skill(char *searchname, char_data *ch);
		if (!vnum_skill(buf2, ch)) {
			msg_to_char(ch, "No skills by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "social")) {
		extern int vnum_social(char *searchname, char_data *ch);
		if (!vnum_social(buf2, ch)) {
			msg_to_char(ch, "No socials by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "trigger")) {
		if (!vnum_trigger(buf2, ch)) {
			msg_to_char(ch, "No triggers by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "vehicle")) {
		extern int vnum_vehicle(char *searchname, char_data *ch);
		if (!vnum_vehicle(buf2, ch)) {
			msg_to_char(ch, "No vehicles by that name.\r\n");
		}
	}
	else {
		send_to_char("Usage: vnum <type> <name>\r\n", ch);
	}
}


ACMD(do_vstat) {
	empire_data *emp;
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
	
	if (is_abbrev(buf, "adventure")) {	// alphabetic precedence for "vstat a"
		adv_data *adv = adventure_proto(number);
		if (!adv) {
			msg_to_char(ch, "There is no adventure zone with that number.\r\n");
			return;
		}
		do_stat_adventure(ch, adv);
	}
	else if (is_abbrev(buf, "ability")) {
		void do_stat_ability(char_data *ch, ability_data *abil);
		ability_data *abil = find_ability_by_vnum(number);
		if (!abil) {
			msg_to_char(ch, "There is no ability with that number.\r\n");
			return;
		}
		do_stat_ability(ch, abil);
	}
	else if (is_abbrev(buf, "archetype")) {
		void do_stat_archetype(char_data *ch, archetype_data *arch);
		archetype_data *arch = archetype_proto(number);
		if (!arch) {
			msg_to_char(ch, "There is no archetype with that number.\r\n");
			return;
		}
		do_stat_archetype(ch, arch);
	}
	else if (is_abbrev(buf, "augment")) {
		void do_stat_augment(char_data *ch, augment_data *aug);
		augment_data *aug = augment_proto(number);
		if (!aug) {
			msg_to_char(ch, "There is no augment with that number.\r\n");
			return;
		}
		do_stat_augment(ch, aug);
	}
	else if (is_abbrev(buf, "building")) {	// alphabetic precedence for "vstat b"
		bld_data *bld = building_proto(number);
		if (!bld) {
			msg_to_char(ch, "There is no building with that number.\r\n");
			return;
		}
		do_stat_building(ch, bld);
	}
	else if (is_abbrev(buf, "book")) {
		book_data *book = book_proto(number);
		if (!book) {
			msg_to_char(ch, "There is no book with that number.\r\n");
			return;
		}
		do_stat_book(ch, book);
	}
	else if (is_abbrev(buf, "craft")) {	// alphabetic precedence for "vstat c"
		craft_data *craft = craft_proto(number);
		if (!craft) {
			msg_to_char(ch, "There is no craft with that number.\r\n");
			return;
		}
		do_stat_craft(ch, craft);
	}
	else if (is_abbrev(buf, "class")) {
		void do_stat_class(char_data *ch, class_data *cls);
		class_data *cls = find_class_by_vnum(number);
		if (!cls) {
			msg_to_char(ch, "There is no class with that number.\r\n");
			return;
		}
		do_stat_class(ch, cls);
	}
	else if (is_abbrev(buf, "crop")) {
		crop_data *crop = crop_proto(number);
		if (!crop) {
			msg_to_char(ch, "There is no crop with that number.\r\n");
			return;
		}
		do_stat_crop(ch, crop);
	}
	else if (!strn_cmp(buf, "emp", 3) && is_abbrev(buf, "empire")) {
		if ((emp = get_empire_by_name(buf2))) {
			do_stat_empire(ch, emp);
		}
		else if (!*buf2) {
			msg_to_char(ch, "Get stats on which empire?\r\n");
		}
		else {
			msg_to_char(ch, "Unknown empire.\r\n");
		}
	}
	else if (is_abbrev(buf, "event")) {
		void do_stat_event(char_data *ch, event_data *event);
		event_data *event = find_event_by_vnum(number);
		if (!event) {
			msg_to_char(ch, "There is no event with that number.\r\n");
			return;
		}
		do_stat_event(ch, event);
	}
	else if (is_abbrev(buf, "faction")) {
		void do_stat_faction(char_data *ch, faction_data *fct);
		faction_data *fct = find_faction_by_vnum(number);
		if (!fct) {
			msg_to_char(ch, "There is no faction with that number.\r\n");
			return;
		}
		do_stat_faction(ch, fct);
	}
	else if (is_abbrev(buf, "global")) {	// precedence on 'g'
		struct global_data *glb = global_proto(number);
		if (!glb) {
			msg_to_char(ch, "There is no global with that number.\r\n");
			return;
		}
		do_stat_global(ch, glb);
	}
	else if (is_abbrev(buf, "generic")) {
		void do_stat_generic(char_data *ch, generic_data *gen);
		generic_data *gen = real_generic(number);
		if (!gen) {
			msg_to_char(ch, "There is no generic with that number.\r\n");
			return;
		}
		do_stat_generic(ch, gen);
	}
	else if (is_abbrev(buf, "mobile")) {
		if (!mob_proto(number)) {
			send_to_char("There is no monster with that number.\r\n", ch);
			return;
		}
		mob = read_mobile(number, TRUE);
		// put it somewhere, briefly
		char_to_room(mob, world_table);
		do_stat_character(ch, mob);
		extract_char(mob);
	}
	else if (is_abbrev(buf, "morph")) {
		void do_stat_morph(char_data *ch, morph_data *morph);
		morph_data *morph = morph_proto(number);
		if (!morph) {
			msg_to_char(ch, "There is no morph with that number.\r\n");
			return;
		}
		do_stat_morph(ch, morph);
	}
	else if (is_abbrev(buf, "object")) {
		if (!obj_proto(number)) {
			send_to_char("There is no object with that number.\r\n", ch);
			return;
		}
		obj = read_object(number, TRUE);
		do_stat_object(ch, obj);
		extract_obj(obj);
	}
	else if (is_abbrev(buf, "progression")) {
		void do_stat_progress(char_data *ch, progress_data *prg);
		progress_data *prg = real_progress(number);
		if (!prg) {
			msg_to_char(ch, "There is no progression goal with that number.\r\n");
			return;
		}
		do_stat_progress(ch, prg);
	}
	else if (is_abbrev(buf, "quest")) {
		void do_stat_quest(char_data *ch, quest_data *quest);
		quest_data *quest = quest_proto(number);
		if (!quest) {
			msg_to_char(ch, "There is no quest with that number.\r\n");
			return;
		}
		do_stat_quest(ch, quest);
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
	else if (is_abbrev(buf, "shop")) {
		void do_stat_shop(char_data *ch, shop_data *shop);
		shop_data *shop = real_shop(number);
		if (!shop) {
			msg_to_char(ch, "There is no shop with that number.\r\n");
			return;
		}
		do_stat_shop(ch, shop);
	}
	else if (is_abbrev(buf, "skill")) {
		void do_stat_skill(char_data *ch, skill_data *skill);
		skill_data *skill = find_skill_by_vnum(number);
		if (!skill) {
			msg_to_char(ch, "There is no skill with that number.\r\n");
			return;
		}
		do_stat_skill(ch, skill);
	}
	else if (is_abbrev(buf, "social")) {
		void do_stat_social(char_data *ch, social_data *soc);
		social_data *soc = social_proto(number);
		if (!soc) {
			msg_to_char(ch, "There is no social with that number.\r\n");
			return;
		}
		do_stat_social(ch, soc);
	}
	else if (is_abbrev(buf, "trigger")) {
		trig_data *trig = real_trigger(number);
		if (!trig) {
			msg_to_char(ch, "That vnum does not exist.\r\n");
			return;
		}

		do_stat_trigger(ch, trig);
	}
	else if (is_abbrev(buf, "vehicle")) {
		vehicle_data *veh;
		if (!vehicle_proto(number)) {
			msg_to_char(ch, "There is no vehicle with that vnum.\r\n");
		}
		else {
			veh = read_vehicle(number, TRUE);
			do_stat_vehicle(ch, veh);
			extract_vehicle(veh);
		}
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
			wizlock_message = str_dup(argument);
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
				result = ((TOGGLE_BIT(GET_ACCOUNT(vict)->flags, ACCT_NOTITLE)) & ACCT_NOTITLE);
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: Notitle %s for %s by %s", ONOFF(result), GET_NAME(vict), GET_NAME(ch));
				msg_to_char(ch, "Notitle %s for %s.\r\n", ONOFF(result), GET_NAME(vict));
				break;
			case SCMD_MUTE:
				result = ((TOGGLE_BIT(GET_ACCOUNT(vict)->flags, ACCT_MUTED)) & ACCT_MUTED);
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: Mute %s for %s by %s", ONOFF(result), GET_NAME(vict), GET_NAME(ch));
				msg_to_char(ch, "Mute %s for %s.\r\n", ONOFF(result), GET_NAME(vict));
				break;
			case SCMD_FREEZE:
				if (ch == vict) {
					send_to_char("Oh, yeah, THAT'S real smart...\r\n", ch);
					return;
				}
				if (ACCOUNT_FLAGGED(vict, ACCT_FROZEN)) {
					send_to_char("Your victim is already pretty cold.\r\n", ch);
					return;
				}
				SET_BIT(GET_ACCOUNT(vict)->flags, ACCT_FROZEN);
				send_to_char("A bitter wind suddenly rises and drains every erg of heat from your body!\r\nYou feel frozen!\r\n", vict);
				send_to_char("Frozen.\r\n", ch);
				act("A sudden cold wind conjured from nowhere freezes $n!", FALSE, vict, 0, 0, TO_ROOM);
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s frozen by %s", GET_NAME(vict), GET_NAME(ch));
				break;
			case SCMD_THAW:
				if (!ACCOUNT_FLAGGED(vict, ACCT_FROZEN)) {
					send_to_char("Sorry, your victim is not morbidly encased in ice at the moment.\r\n", ch);
					return;
				}
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s un-frozen by %s", GET_NAME(vict), GET_NAME(ch));
				REMOVE_BIT(GET_ACCOUNT(vict)->flags, ACCT_FROZEN);
				send_to_char("A fireball suddenly explodes in front of you, melting the ice!\r\nYou feel thawed.\r\n", vict);
				send_to_char("Thawed.\r\n", ch);
				act("A sudden fireball conjured from nowhere thaws $n!", FALSE, vict, 0, 0, TO_ROOM);
				break;
			default:
				log("SYSERR: Unknown subcmd %d passed to do_wizutil (%s)", subcmd, __FILE__);
				break;
		}
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
		SAVE_CHAR(vict);
	}
}
