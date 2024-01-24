/* ************************************************************************
*   File: statistics.c                                    EmpireMUD 2.0b5 *
*  Usage: code related game stats                                         *
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
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "constants.h"
#include "skills.h"

/**
* Contents:
*   Stats Displays
*   Stats Getters
*   Player Stats
*   World Stats
*/

// config
#define RESCAN_WORLD_AFTER  (5 * SECS_PER_REAL_MIN)

// stats globals
struct stats_data_struct *global_sector_count = NULL;	// hash table of sector counts
int global_sector_size = 0;
struct stats_data_struct *global_crop_count = NULL;	// hash table count of crops
int global_crop_size = 0;
struct stats_data_struct *global_building_count = NULL;	// hash table of building counts
int global_building_size = 0;

time_t last_world_count = 0;	// timestamp of last sector/crop/building tally
time_t last_account_count = 0;	// timestamp of last time accounts were read

int total_accounts = 0;	// including inactive
int active_accounts = 0;	// not timed out (at least 1 char)
int active_accounts_week = 0;	// just this week (at least 1 char)
int max_players_this_uptime = 0;


 //////////////////////////////////////////////////////////////////////////////
//// STATS DISPLAYS //////////////////////////////////////////////////////////

/**
* Displays various game stats. This is shown on login/logout.
*
* @param char_data *ch The person to show stats to.
*/
void display_statistics_to_char(char_data *ch) {
	char populous_str[MAX_STRING_LENGTH], wealthiest_str[MAX_STRING_LENGTH], famous_str[MAX_STRING_LENGTH], greatest_str[MAX_STRING_LENGTH];
	int populous_empire = NOTHING, wealthiest_empire = NOTHING, famous_empire = NOTHING, greatest_empire = NOTHING;
	int num_populous = 0, num_wealthy = 0, num_famous = 0, num_great = 0;
	vehicle_data *veh;
	char_data *vict;
	obj_data *obj;
	empire_data *emp, *next_emp;
	int citizens, territory, members, military, count;

	char *tmstr;
	time_t mytime;
	int d, h, m;

	if (!ch || !ch->desc)
		return;

	msg_to_char(ch, "\r\nEmpireMUD Statistics:\r\n");

	mytime = boot_time;

	tmstr = (char *) asctime(localtime(&mytime));
	*(tmstr + strlen(tmstr) - 1) = '\0';

	mytime = time(0) - boot_time;
	d = mytime / 86400;
	h = (mytime / 3600) % 24;
	m = (mytime / 60) % 60;

	msg_to_char(ch, "Current uptime: Up since %s: %d day%s, %d:%02d\r\n", tmstr, d, ((d == 1) ? "" : "s"), h, m);

	/* Find best scores.. */
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_IMM_ONLY(emp) || EMPIRE_IS_TIMED_OUT(emp)) {
			continue;
		}
		
		if (EMPIRE_MEMBERS(emp) > populous_empire) {
			populous_empire = EMPIRE_MEMBERS(emp);
			num_populous = 1;
		}
		else if (EMPIRE_MEMBERS(emp) == populous_empire) {
			++num_populous;
		}
		if (GET_TOTAL_WEALTH(emp) > wealthiest_empire) {
			wealthiest_empire = GET_TOTAL_WEALTH(emp);
			num_wealthy = 1;
		}
		else if (GET_TOTAL_WEALTH(emp) == wealthiest_empire) {
			++num_wealthy;
		}
		if (EMPIRE_FAME(emp) > famous_empire) {
			famous_empire = EMPIRE_FAME(emp);
			num_famous = 1;
		}
		else if (EMPIRE_FAME(emp) == famous_empire) {
			++num_famous;
		}
		if (EMPIRE_GREATNESS(emp) > greatest_empire) {
			greatest_empire = EMPIRE_GREATNESS(emp);
			num_great = 1;
		}
		else if (EMPIRE_GREATNESS(emp) == greatest_empire) {
			++num_great;
		}
	}
	
	// init strings
	*populous_str = '\0';
	*wealthiest_str = '\0';
	*famous_str = '\0';
	*greatest_str = '\0';
	
	// build strings
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_IMM_ONLY(emp) || EMPIRE_IS_TIMED_OUT(emp)) {
			continue;
		}
		
		if (populous_empire != NOTHING && num_populous < 5 && EMPIRE_MEMBERS(emp) >= populous_empire) {
			snprintf(populous_str + strlen(populous_str), sizeof(populous_str) - strlen(populous_str), "%s%s%s&0", (*populous_str ? ", " : ""), EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		}
		if (wealthiest_empire != NOTHING && num_wealthy < 5 && GET_TOTAL_WEALTH(emp) >= wealthiest_empire) {
			snprintf(wealthiest_str + strlen(wealthiest_str), sizeof(wealthiest_str) - strlen(wealthiest_str), "%s%s%s&0", (*wealthiest_str ? ", " : ""), EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		}
		if (famous_empire != NOTHING && num_famous < 5 && EMPIRE_FAME(emp) >= famous_empire) {
			snprintf(famous_str + strlen(famous_str), sizeof(famous_str) - strlen(famous_str), "%s%s%s&0", (*famous_str ? ", " : ""), EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		}
		if (greatest_empire != NOTHING && num_great < 5 && EMPIRE_GREATNESS(emp) >= greatest_empire) {
			snprintf(greatest_str + strlen(greatest_str), sizeof(greatest_str) - strlen(greatest_str), "%s%s%s&0", (*greatest_str ? ", " : ""), EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		}
	}
	
	if (*greatest_str) {
		msg_to_char(ch, "Greatest empire%s: %s\r\n", PLURAL(num_great), greatest_str);
	}
	if (*populous_str) {
		msg_to_char(ch, "Most populous empire%s: %s\r\n", PLURAL(num_populous), populous_str);
	}
	if (*wealthiest_str) {
		msg_to_char(ch, "Most wealthy empire%s: %s\r\n", PLURAL(num_wealthy), wealthiest_str);
	}
	if (*famous_str) {
		msg_to_char(ch, "Most famous empire%s: %s\r\n", PLURAL(num_famous), famous_str);
	}

	// empire stats
	territory = members = citizens = military = 0;
	HASH_ITER(hh, empire_table, emp, next_emp) {
		territory += EMPIRE_TERRITORY(emp, TER_TOTAL);
		members += EMPIRE_MEMBERS(emp);
		citizens += EMPIRE_POPULATION(emp);
		military += EMPIRE_MILITARY(emp);
	}

	msg_to_char(ch, "Total Empires:    %5d     Claimed Area:       %d\r\n", HASH_COUNT(empire_table), territory);
	msg_to_char(ch, "Total Players:    %5d     Players in Empires: %d\r\n", HASH_CNT(idnum_hh, player_table_by_idnum), members);
	msg_to_char(ch, "Total Citizens:   %5d     Total Military:     %d\r\n", citizens, military);

	// creatures
	count = 0;
	DL_FOREACH(character_list, vict) {
		if (IS_NPC(vict)) {
			++count;
		}
	}
	msg_to_char(ch, "Unique Creatures: %5d     Total Mobs:         %d\r\n", HASH_COUNT(mobile_table), count);

	// objs
	DL_COUNT(object_list, obj, count);
	msg_to_char(ch, "Unique Objects:   %5d     Total Objects:      %d\r\n", HASH_COUNT(object_table), count);
	
	// vehicles
	DL_COUNT(vehicle_list, veh, count);
	msg_to_char(ch, "Unique Vehicles:  %5d     Total Vehicles:     %d\r\n", HASH_COUNT(vehicle_table), count);
}


/**
* do_mudstats: show configurations
*
* @param char_data *ch Person to show it to.
* @param char *argument In case some stats have sub-categories.
*/
void mudstats_configs(char_data *ch, char *argument) {
	char output[MAX_STRING_LENGTH * 2], part[256];
	
	// start output
	snprintf(output, sizeof(output), "Game configuration for %s:\r\n", config_get_string("mud_name"));
	
	// status/hiring
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "Status: %s", config_get_string("mud_status"));
	if (config_get_bool("hiring_builders") || config_get_bool("hiring_coders")) {
		snprintf(output + strlen(output), sizeof(output) - strlen(output), ", Hiring: %s", (config_get_bool("hiring_builders") && config_get_bool("hiring_coders")) ? "builders and coders" : (config_get_bool("hiring_builders") ? "builders" : "coders"));
	}
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "\r\n");
	
	// time
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "Game time: %.1f minutes per game hour,", SECS_PER_MUD_HOUR / (double)SECS_PER_REAL_MIN);
	snprintf(output + strlen(output), sizeof(output) - strlen(output), " %.1f hours per day,", SECS_PER_MUD_DAY / (double) SECS_PER_REAL_HOUR);
	snprintf(output + strlen(output), sizeof(output) - strlen(output), " %.1f days per month,", SECS_PER_MUD_MONTH / (double) SECS_PER_REAL_DAY);
	snprintf(output + strlen(output), sizeof(output) - strlen(output), " %.1f days per year\r\n", SECS_PER_MUD_YEAR / (double) SECS_PER_REAL_DAY);
	
	// skills
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "Skills: %d at %d, %d at %d, %d total\r\n", config_get_int("skills_at_max_level"), MAX_SKILL_CAP, config_get_int("skills_at_specialty_level"), SPECIALTY_SKILL_CAP, config_get_int("skills_per_char"));
	
	// environment
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "Environment: %s\r\n", config_get_bool("temperature_penalties") ? "temperature penalties" : "no penalties from temperature");
	
	// pk
	prettier_sprintbit(config_get_bitvector("pk_mode"), pk_modes, part);
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "Player-killing: %s\r\n", config_get_bitvector("pk_mode") ? part : "forbidden");
	
	// war
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "War: %d offense%s required%s\r\n", config_get_int("offense_min_to_war"), PLURAL(config_get_int("offense_min_to_war")), config_get_bool("mutual_war_only") ? ", wars must be mutual" : "");
	
	// city
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "Cities: %d minutes to establish, %d tiles apart (%d for allies)\r\n", config_get_int("minutes_to_full_city"), config_get_int("min_distance_between_cities"), config_get_int("min_distance_between_ally_cities"));
	
	// storage
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "Storage: %s\r\n", config_get_bool("decay_in_storage") ? "items decay" : "no decay");
	
	// workforce
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "Workforce: %d cap per member, %d minimum cap per island, %d tile range\r\n", config_get_int("max_chore_resource_per_member"), config_get_int("max_chore_resource_over_total"), config_get_int("chore_distance"));
	
	// newbie island
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "Newbie islands: %s", config_get_bool("cities_on_newbie_islands") ? "allow cities" : "no cities allowed");
	if (config_get_int("newbie_island_day_limit") > 0) {
		snprintf(output + strlen(output), sizeof(output) - strlen(output), ", %d day limit", config_get_int("newbie_island_day_limit"));
	}
	if (config_get_bool("naturalize_newbie_islands")) {
		snprintf(output + strlen(output), sizeof(output) - strlen(output), ", naturalized each year");
	}
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "\r\n");
	
	// approval
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "Approval: %s%s", config_get_bool("auto_approve") ? "automatic" : "required", config_get_bool("approve_per_character") ? " (per character)" : "");
	if (!config_get_bool("auto_approve")) {
		snprintf(output + strlen(output), sizeof(output) - strlen(output), ", needed for:");
		if (config_get_bool("build_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " building");
		}
		if (config_get_bool("chat_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " chatting");
		}
		if (config_get_bool("craft_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " crafting");
		}
		if (config_get_bool("event_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " events");
		}
		if (config_get_bool("gather_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " gathering");
		}
		if (config_get_bool("join_empire_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " empires");
		}
		if (config_get_bool("quest_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " quests");
		}
		if (config_get_bool("skill_gain_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " skills");
		}
		if (config_get_bool("tell_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " tells");
		}
		if (config_get_bool("terraform_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " terraforming");
		}
		if (config_get_bool("title_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " title");
		}
		if (config_get_bool("travel_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " travel");
		}
		if (config_get_bool("write_approval")) {
			snprintf(output + strlen(output), sizeof(output) - strlen(output), " writing");
		}
	}
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "\r\n");
	
	// timeouts
	snprintf(output + strlen(output), sizeof(output) - strlen(output), "Timeouts: %d days for newbies, up to %d days after %d hours of playtime\r\n", config_get_int("member_timeout_newbie"), config_get_int("member_timeout_full"), config_get_int("member_timeout_max_threshold"));
	
	if (ch->desc) {
		page_string(ch->desc, output, TRUE);
	}
}


/**
* do_mudstats: empire score averages
*
* @param char_data *ch Person to show it to.
* @param char *argument In case some stats have sub-categories.
*/
void mudstats_empires(char_data *ch, char *argument) {
	int iter;
	
	msg_to_char(ch, "Empire score medians:\r\n");
	
	for (iter = 0; iter < NUM_SCORES; ++iter) {
		msg_to_char(ch, " %s: %d\r\n", score_type[iter], (int) empire_score_average[iter]);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// STATS GETTERS ///////////////////////////////////////////////////////////

/**
* @param bld_data *bdg What building type.
* @return int The number of instances of that building in the world.
*/
int stats_get_building_count(bld_data *bdg) {
	struct stats_data_struct *data;
	any_vnum vnum;

	// sanity
	if (!bdg) {
		return 0;
	}
	
	if (HASH_COUNT(global_building_count) != global_building_size || last_world_count + RESCAN_WORLD_AFTER < time(0)) {
		update_world_count();
	}
	
	vnum = GET_BLD_VNUM(bdg);
	HASH_FIND_INT(global_building_count, &vnum, data);
	return data ? data->count : 0;
}


/**
* @param crop_data *cp What crop type.
* @return int The number of instances of that crop in the world.
*/
int stats_get_crop_count(crop_data *cp) {
	struct stats_data_struct *data;
	any_vnum vnum;

	if (!cp) {
		return 0;
	}
	
	if (HASH_COUNT(global_crop_count) != global_crop_size || last_world_count + RESCAN_WORLD_AFTER < time(0)) {
		update_world_count();
	}
	
	vnum = GET_CROP_VNUM(cp);
	HASH_FIND_INT(global_crop_count, &vnum, data);
	
	return data ? data->count : 0;
}


/**
* @param sector_data *sect What sector type.
* @return int The number of instances of that sector in the world.
*/
int stats_get_sector_count(sector_data *sect) {
	struct stats_data_struct *data;
	any_vnum vnum;
	
	if (!sect) {
		return 0;
	}
	
	if (HASH_COUNT(global_sector_count) != global_sector_size || last_world_count + RESCAN_WORLD_AFTER < time(0)) {
		update_world_count();
	}
	
	vnum = GET_SECT_VNUM(sect);
	HASH_FIND_INT(global_sector_count, &vnum, data);
	
	return data ? data->count : 0;
}


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER STATS ////////////////////////////////////////////////////////////

/**
* This function updates the following global stats:
*  total_accounts (all)
*  active_accounts (at least one character not timed out)
*  active_accounts_week (at least one character this week)
*/
void update_account_stats(void) {
	// helper type
	struct uniq_acct_t {
		int id;	// either account id (positive) or player idnum (negative)
		bool active_week;
		bool active_timeout;
		UT_hash_handle hh;
	};

	struct uniq_acct_t *acct, *next_acct, *acct_list = NULL;
	player_index_data *index, *next_index;
	int id;
	
	// rate-limit this, as it scans the whole playerfile
	if (last_account_count + RESCAN_WORLD_AFTER > time(0)) {
		return;
	}
	
	// build list
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
		id = index->account_id;
		HASH_FIND_INT(acct_list, &id, acct);
		if (!acct) {
			CREATE(acct, struct uniq_acct_t, 1);
			acct->id = id;
			acct->active_week = acct->active_timeout = FALSE;
			HASH_ADD_INT(acct_list, id, acct);
		}
		
		// update
		acct->active_week |= ((time(0) - index->last_logon) < SECS_PER_REAL_WEEK);
		if (!acct->active_timeout) {
			acct->active_timeout = !member_is_timed_out_index(index);
		}
	}

	// compute stats (and free temp data)
	total_accounts = 0;
	active_accounts = 0;
	active_accounts_week = 0;
	
	HASH_ITER(hh, acct_list, acct, next_acct) {
		++total_accounts;
		if (acct->active_week) {
			++active_accounts_week;
		}
		if (acct->active_timeout) {
			++active_accounts;
		}
		
		// and free
		HASH_DEL(acct_list, acct);
		free(acct);
	}
	
	// update timestamp
	last_account_count = time(0);
}


/**
* Updates the count of players online.
*/
void update_players_online_stats(void) {
	descriptor_data *d;
	int count, max;
	
	// determine current count
	count = 0;
	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) != CON_PLAYING || !d->character || IS_NPC(d->character)) {
			continue;
		}
		
		// morts only
		if (IS_IMMORTAL(d->character) || GET_INVIS_LEV(d->character) > LVL_MORTAL) {
			continue;
		}
		
		++count;
	}
	
	max = data_get_int(DATA_MAX_PLAYERS_TODAY);
	if (count > max) {
		data_set_int(DATA_MAX_PLAYERS_TODAY, count);
	}
	max_players_this_uptime = MAX(max_players_this_uptime, count);
}


 //////////////////////////////////////////////////////////////////////////////
//// WORLD STATS /////////////////////////////////////////////////////////////

/**
* Updates global_sector_count, global_crop_count, global_building_count.
*/
void update_world_count(void) {
	struct stats_data_struct *sect_inf = NULL, *crop_inf = NULL, *bld_inf = NULL, *data, *next_data;
	any_vnum vnum, last_bld_vnum = NOTHING, last_crop_vnum = NOTHING, last_sect_vnum = NOTHING;
	struct map_data *map;
	room_data *room;
	
	// free and recreate counts
	HASH_ITER(hh, global_sector_count, data, next_data) {
		HASH_DEL(global_sector_count, data);
		free(data);
	}
	HASH_ITER(hh, global_crop_count, data, next_data) {
		HASH_DEL(global_crop_count, data);
		free(data);
	}
	HASH_ITER(hh, global_building_count, data, next_data) {
		HASH_DEL(global_building_count, data);
		free(data);
	}
	
	// scan world
	for (map = land_map; map; map = map->next) {
		// sector
		vnum = GET_SECT_VNUM(map->sector_type);
		if (vnum != last_sect_vnum || !sect_inf) {
			HASH_FIND_INT(global_sector_count, &vnum, sect_inf);
			if (!sect_inf) {
				CREATE(sect_inf, struct stats_data_struct, 1);
				sect_inf->vnum = vnum;
				HASH_ADD_INT(global_sector_count, vnum, sect_inf);
			}
			last_sect_vnum = vnum;
		}
		++sect_inf->count;
		
		// crop
		if (map->crop_type) {
			vnum = GET_CROP_VNUM(map->crop_type);
			
			if (vnum != last_crop_vnum || !crop_inf) {
				HASH_FIND_INT(global_crop_count, &vnum, crop_inf);
				if (!crop_inf) {
					CREATE(crop_inf, struct stats_data_struct, 1);
					crop_inf->vnum = vnum;
					HASH_ADD_INT(global_crop_count, vnum, crop_inf);
				}
				last_crop_vnum = vnum;
			}
			
			++crop_inf->count;
		}
		
		// any further data?
		if (!(room = map->room)) {
			continue;
		}
		
		// building?
		vnum = BUILDING_VNUM(room);
		if (vnum != NOTHING) {
			if (vnum != last_bld_vnum) {
				HASH_FIND_INT(global_building_count, &vnum, bld_inf);
				if (!bld_inf) {
					CREATE(bld_inf, struct stats_data_struct, 1);
					bld_inf->vnum = vnum;
					HASH_ADD_INT(global_building_count, vnum, bld_inf);
				}
				last_bld_vnum = vnum;
			}
			
			++bld_inf->count;
		}
	}
	
	last_world_count = time(0);
	global_sector_size = HASH_COUNT(global_sector_count);
	global_crop_size = HASH_COUNT(global_crop_count);
	global_building_size = HASH_COUNT(global_building_count);
}
