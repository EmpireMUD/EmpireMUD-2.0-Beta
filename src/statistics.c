/* ************************************************************************
*   File: statistics.c                                    EmpireMUD 2.0b1 *
*  Usage: code related game stats                                         *
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
#include "db.h"
#include "comm.h"
#include "handler.h"

/**
* Contents:
*   Stats Displays
*   Stats Getters
*   World Stats
*/

// config
int rescan_world_after = 5 * SECS_PER_REAL_MIN;


// stats data
struct stats_data_struct {
	any_vnum vnum;
	int count;
	
	UT_hash_handle hh;	// hashable
};

// stats globals
struct stats_data_struct *global_sector_count = NULL;	// hash table of sector counts
struct stats_data_struct *global_crop_count = NULL;	// hash table count of crops
struct stats_data_struct *global_building_count = NULL;	// hash table of building counts

time_t last_world_count = 0;	// timestamp of last sector/crop/building tally


// external consts


// external funcs


// locals
void update_world_count();


 //////////////////////////////////////////////////////////////////////////////
//// STATS DISPLAYS //////////////////////////////////////////////////////////

/**
* Displays various game stats. This is shown on login/logout.
*
* @param char_data *ch The person to show stats to.
*/
void display_statistics_to_char(char_data *ch) {
	extern int top_of_p_table;
	extern time_t boot_time;

	char_data *vict;
	obj_data *obj;
	empire_data *emp, *next_emp;
	int citizens, territory, members, military, count;
	int populous_empire = NOTHING, wealthiest_empire = NOTHING, famous_empire = NOTHING, greatest_empire = NOTHING;
	bool found;

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
		if (!EMPIRE_IMM_ONLY(emp)) {
			if (EMPIRE_MEMBERS(emp) > populous_empire) {
				populous_empire = EMPIRE_MEMBERS(emp);
			}
			if (GET_TOTAL_WEALTH(emp) > wealthiest_empire) {
				wealthiest_empire = GET_TOTAL_WEALTH(emp);
			}
			if (EMPIRE_FAME(emp) > famous_empire) {
				famous_empire = EMPIRE_FAME(emp);
			}
			if (EMPIRE_GREATNESS(emp) > greatest_empire) {
				greatest_empire = EMPIRE_GREATNESS(emp);
			}
		}
	}

	if (greatest_empire != NOTHING) {
		found = FALSE;
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if (EMPIRE_GREATNESS(emp) >= greatest_empire && !EMPIRE_IMM_ONLY(emp)) {
				msg_to_char(ch, "%s %s%s&0", found ? "," : "Greatest empire:", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
				found = TRUE;
			}
		}
		msg_to_char(ch, "\r\n");
	}
	if (populous_empire != NOTHING) {
		found = FALSE;
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if (EMPIRE_MEMBERS(emp) >= populous_empire && !EMPIRE_IMM_ONLY(emp)) {
				msg_to_char(ch, "%s %s%s&0", found ? "," : "Most populous empire:", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
				found = TRUE;
			}
		}
		msg_to_char(ch, "\r\n");
	}
	if (wealthiest_empire != NOTHING) {
		found = FALSE;
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if (GET_TOTAL_WEALTH(emp) >= wealthiest_empire && !EMPIRE_IMM_ONLY(emp)) {
				msg_to_char(ch, "%s %s%s&0", found ? "," : "Most wealthy empire:", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
				found = TRUE;
			}
		}
		msg_to_char(ch, "\r\n");
	}
	if (famous_empire != NOTHING) {
		found = FALSE;
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if (EMPIRE_FAME(emp) >= famous_empire && !EMPIRE_IMM_ONLY(emp)) {
				msg_to_char(ch, "%s %s%s&0", found ? "," : "Most famous empire:", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
				found = TRUE;
			}
		}
		msg_to_char(ch, "\r\n");
	}

	// empire stats
	territory = members = citizens = military = 0;
	HASH_ITER(hh, empire_table, emp, next_emp) {
		territory += EMPIRE_CITY_TERRITORY(emp) + EMPIRE_OUTSIDE_TERRITORY(emp);
		members += EMPIRE_MEMBERS(emp);
		citizens += EMPIRE_POPULATION(emp);
		military += EMPIRE_MILITARY(emp);
	}

	msg_to_char(ch, "Total Empires:	    %3d     Claimed Area:       %d\r\n", HASH_COUNT(empire_table), territory);
	msg_to_char(ch, "Total Players:    %5d     Players in Empires: %d\r\n", top_of_p_table + 1, members);
	msg_to_char(ch, "Total Citizens:   %5d     Total Military:     %d\r\n", citizens, military);

	// creatures
	count = 0;
	for (vict = character_list; vict; vict = vict->next) {
		if (IS_NPC(vict)) {
			++count;
		}
	}
	msg_to_char(ch, "Unique Creatures:   %3d     Total Mobs:         %d\r\n", HASH_COUNT(mobile_table), count);

	// objs
	count = 0;
	for (obj = object_list; obj; obj = obj->next) {
		++count;
	}
	msg_to_char(ch, "Unique Objects:     %3d     Total Objects:      %d\r\n", HASH_COUNT(object_table), count);
}


/**
* do_mudstats: empire score averages
*
* @param char_data *ch Person to show it to.
* @param char *argument In case some stats have sub-categories.
*/
void mudstats_empires(char_data *ch, char *argument) {
	extern double empire_score_average[NUM_SCORES];
	extern const char *score_type[];
	
	int iter;
	
	msg_to_char(ch, "Empire score averages:\r\n");
	
	for (iter = 0; iter < NUM_SCORES; ++iter) {
		msg_to_char(ch, " %s: %.3f\r\n", score_type[iter], empire_score_average[iter]);
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
	
	if (HASH_COUNT(global_building_count) != HASH_COUNT(building_table) || last_world_count + rescan_world_after < time(0)) {
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
	
	if (HASH_COUNT(global_crop_count) != HASH_COUNT(crop_table) || last_world_count + rescan_world_after < time(0)) {
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
	
	if (HASH_COUNT(global_sector_count) != HASH_COUNT(sector_table) || last_world_count + rescan_world_after < time(0)) {
		update_world_count();
	}
	
	vnum = GET_SECT_VNUM(sect);
	HASH_FIND_INT(global_sector_count, &vnum, data);
	
	return data ? data->count : 0;
}


 //////////////////////////////////////////////////////////////////////////////
//// WORLD STATS /////////////////////////////////////////////////////////////

/**
* Updates global_sector_count, global_crop_count, global_building_count.
*/
void update_world_count(void) {
	struct stats_data_struct *sect_inf = NULL, *crop_inf = NULL, *bld_inf = NULL, *data, *next_data;
	room_data *iter, *next_iter;
	any_vnum vnum, last_bld_vnum = NOTHING, last_crop_vnum = NOTHING, last_sect_vnum = NOTHING;
	
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
	HASH_ITER(world_hh, world_table, iter, next_iter) {
		// sector
		vnum = GET_SECT_VNUM(SECT(iter));
		if (vnum != last_sect_vnum) {
			HASH_FIND_INT(global_sector_count, &vnum, sect_inf);
			if (!sect_inf) {
				CREATE(sect_inf, struct stats_data_struct, 1);
				sect_inf->vnum = vnum;
				HASH_ADD_INT(global_sector_count, vnum, sect_inf);
			}
			last_sect_vnum = vnum;
		}
		++sect_inf->count;
		
		// crop?
		vnum = ROOM_CROP_TYPE(iter);
		if (vnum != NOTHING) {
			if (vnum != last_crop_vnum) {
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
		
		// building?
		vnum = BUILDING_VNUM(iter);
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
}
