/* ************************************************************************
*   File: weather.c                                       EmpireMUD 2.0b5 *
*  Usage: functions handling time and the weather                         *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <math.h>

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "skills.h"
#include "constants.h"

/**
* Contents:
*   Unsorted Code
*   Time Handling
*   Moon System
*/

// local prototypes
void another_hour();
void weather_change();


void weather_and_time(void) {
	another_hour();
	weather_change();
}


/**
* This is called at startup and once per game day to update the seasons for the
* entire map.
*
* It uses a lot of math to determine 3 things:
*  1. Past the arctic line, always Winter.
*  2. Between the tropics, alternate Spring, Summer, Autumn (no Winter).
*  3. Everywhere else, seasons are based on time of year, and gradually move
*     North/South each day. There should always be a boundary between the
*     Summer and Winter regions (thus all the squirrelly numbers).
*
* A near-copy of this function exists in util/evolve.c for map evolutions.
*/
void determine_seasons(void) {
	double half_y, latitude;
	int ycoord, day_of_year;
	bool northern;
	
	// basic defintions and math
	#define north_y_arctic  ARCTIC_LATITUDE
	#define south_y_arctic  (90.0 - ARCTIC_LATITUDE)
	
	#define north_y_tropics  TROPIC_LATITUDE
	#define south_y_tropics  (90.0 - TROPIC_LATITUDE)
	
	// basic slope of the seasonal gradients
	double north_a_slope = ((north_y_arctic - 1) - (north_y_tropics + 1)) / 120.0;	// summer/winter
	double north_b_slope = ((north_y_arctic - 1) - (north_y_tropics + 1)) / 90.0;	// spring/autumn
	
	double south_a_slope = ((south_y_tropics - 1) - (south_y_arctic + 1)) / 120.0;	// same but for the south
	double south_b_slope = ((south_y_tropics - 1) - (south_y_arctic + 1)) / 90.0;
	
	#define pick_slope(north, south)  (northern ? north : south)
	
	// Day_of_year is the x-axis of the graph that determines the season at a
	// given y-coord. Month 0 is january; year is 0-359 days.
	day_of_year = main_time_info.month * 30 + main_time_info.day;
	
	for (ycoord = 0; ycoord < MAP_HEIGHT; ++ycoord) {
		latitude = Y_TO_LATITUDE(ycoord);
		northern = (latitude >= 0.0);
		
		// tropics? -- take half the tropic value, convert to percent, multiply by map height
		if (ABSOLUTE(latitude) < TROPIC_LATITUDE) {
			if (main_time_info.month < 2) {
				y_coord_to_season[ycoord] = TILESET_SPRING;
			}
			else if (main_time_info.month >= 10) {
				y_coord_to_season[ycoord] = TILESET_AUTUMN;
			}
			else {
				y_coord_to_season[ycoord] = TILESET_SUMMER;
			}
			continue;
		}
		
		// arctic? -- take half the arctic value, convert to percent, check map edges
		if (ABSOLUTE(latitude) > ARCTIC_LATITUDE) {
			y_coord_to_season[ycoord] = TILESET_WINTER;
			continue;
		}
		
		// all other regions: first split the map in half (we'll invert for the south)	
		if (northern) {
			half_y = latitude - north_y_tropics; // simplify by moving the y axis to match the tropics line
		}
		else {
			half_y = 90.0 - ABSOLUTE(latitude) - south_y_arctic;	// adjust to remove arctic
		}
	
		if (day_of_year < 6 * 30) {	// first half of year
			if (half_y >= (day_of_year + 1) * pick_slope(north_a_slope, south_a_slope)) {	// first winter line
				y_coord_to_season[ycoord] = northern ? TILESET_WINTER : TILESET_SUMMER;
			}
			else if (half_y >= (day_of_year - 89) * pick_slope(north_b_slope, south_b_slope)) {	// spring line
				y_coord_to_season[ycoord] = northern ? TILESET_SPRING : TILESET_AUTUMN;
			}
			else {
				y_coord_to_season[ycoord] = northern ? TILESET_SUMMER : TILESET_WINTER;
			}
			continue;
		}
		else {	// 2nd half of year
			if (half_y >= (day_of_year - 360) * -pick_slope(north_a_slope, south_a_slope)) {	// second winter line
				y_coord_to_season[ycoord] = northern ? TILESET_WINTER : TILESET_SUMMER;
			}
			else if (half_y >= (day_of_year - 268) * -pick_slope(north_b_slope, south_b_slope)) {	// autumn line
				y_coord_to_season[ycoord] = northern ? TILESET_AUTUMN : TILESET_SPRING;
			}
			else {
				y_coord_to_season[ycoord] = northern ? TILESET_SUMMER : TILESET_WINTER;
			}
			continue;
		}
	
		// fail? we should never reach this
		y_coord_to_season[ycoord] = TILESET_SUMMER;
	}
}


void weather_change(void) {
	int diff, change;
	if ((main_time_info.month >= 4) && (main_time_info.month <= 8))
		diff = (weather_info.pressure > 985 ? -2 : 2);
	else
		diff = (weather_info.pressure > 1015 ? -2 : 2);

	weather_info.change += (dice(1, 4) * diff + dice(2, 6) - dice(2, 6));

	weather_info.change = MIN(weather_info.change, 12);
	weather_info.change = MAX(weather_info.change, -12);

	weather_info.pressure += weather_info.change;

	weather_info.pressure = MIN(weather_info.pressure, 1040);
	weather_info.pressure = MAX(weather_info.pressure, 960);

	change = 0;

	switch (weather_info.sky) {
		case SKY_CLOUDLESS:
			if (weather_info.pressure < 990)
				change = 1;
			else if (weather_info.pressure < 1010)
				if (!number(0, 3))
					change = 1;
			break;
		case SKY_CLOUDY:
			if (weather_info.pressure < 970)
				change = 2;
			else if (weather_info.pressure < 990) {
				if (!number(0, 3))
					change = 2;
				else
					change = 0;
			}
			else if (weather_info.pressure > 1030)
				if (!number(0, 3))
					change = 3;
			break;
		case SKY_RAINING:
			if (weather_info.pressure < 970) {
				if (!number(0, 3))
					change = 4;
				else
					change = 0;
			}
			else if (weather_info.pressure > 1030)
				change = 5;
			else if (weather_info.pressure > 1010)
				if (!number(0, 3))
					change = 5;
			break;
		case SKY_LIGHTNING:
			if (weather_info.pressure > 1010)
				change = 6;
			else if (weather_info.pressure > 990)
				if (!number(0, 3))
					change = 6;
			break;
		default:
			change = 0;
			weather_info.sky = SKY_CLOUDLESS;
			break;
	}

	switch (change) {
		case 1:
			send_to_outdoor(TRUE, "The sky starts to get cloudy.\r\n");
			weather_info.sky = SKY_CLOUDY;
			break;
		case 2:
			send_to_outdoor(TRUE, "It starts to rain.\r\n");
			weather_info.sky = SKY_RAINING;
			break;
		case 3:
			send_to_outdoor(TRUE, "The clouds disappear.\r\n");
			weather_info.sky = SKY_CLOUDLESS;
			break;
		case 4:
			send_to_outdoor(TRUE, "Lightning starts to show in the sky.\r\n");
			weather_info.sky = SKY_LIGHTNING;
			break;
		case 5:
			send_to_outdoor(TRUE, "The rain stops.\r\n");
			weather_info.sky = SKY_CLOUDY;
			break;
		case 6:
			send_to_outdoor(TRUE, "The lightning stops.\r\n");
			weather_info.sky = SKY_RAINING;
			break;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// WEATHER HANDLING ////////////////////////////////////////////////////////


 //////////////////////////////////////////////////////////////////////////////
//// TIME HANDLING ///////////////////////////////////////////////////////////

/**
* Advances time by an hour (and cascades it to other time zones). Also triggers
* things which happen on specific hours.
*/
void another_hour(void) {
	descriptor_data *d;
	long lny;
	
	// update main time
	++main_time_info.hours;
	if (main_time_info.hours > 23) {
		// day change
		main_time_info.hours -= 24;
		++main_time_info.day;
		
		// seasons move daily
		determine_seasons();
		
		// month change
		if (main_time_info.day > 29) {
			main_time_info.day = 0;
			++main_time_info.month;
			
			// year change
			if (main_time_info.month > 11) {
				main_time_info.month = 0;
				++main_time_info.year;
				
				// run annual update
				annual_world_update();
			}
		}
		else {	// not day 30
			// check if we've missed a new year recently
			lny = data_get_long(DATA_LAST_NEW_YEAR);
			if (lny && lny + SECS_PER_MUD_YEAR < time(0)) {
				annual_world_update();
			}
		}
	}
	
	// cascade to other time zones
	cascade_time_info();
	
	// hour-based updates
	switch (main_time_info.hours) {
		case 0: {	// midnight
			run_external_evolutions();
			break;
		}
		case 1: {
			// 1am shipment
			process_shipping();
			break;
		}
		case 7: {
			for (d = descriptor_list; d; d = d->next) {
				if (STATE(d) == CON_PLAYING && !HAS_INFRA(d->character) && !PRF_FLAGGED(d->character, PRF_HOLYLIGHT) && AWAKE(d->character) && IS_OUTDOORS(d->character)) {
					look_at_room(d->character);
					msg_to_char(d->character, "\r\n");
				}
			}
			send_to_outdoor(FALSE, "The sun rises over the horizon.\r\n");
			
			// 7am shipment
			process_shipping();
			
			// world save
			save_whole_world();
			break;
		}
		case 8: {
			send_to_outdoor(FALSE, "The day has begun.\r\n");
			break;
		}
		case 12: {	// noon
			process_imports();
			break;
		}
		case 13: {
			// 1pm shipment
			process_shipping();
			break;
		}
		case 19: {
			send_to_outdoor(FALSE, "The sun slowly disappears beneath the horizon.\r\n");
			
			// 7pm shipment
			process_shipping();
			break;
		}
		case 20: {
			for (d = descriptor_list; d; d = d->next)
				if (STATE(d) == CON_PLAYING && !HAS_INFRA(d->character) && !PRF_FLAGGED(d->character, PRF_HOLYLIGHT) &&  AWAKE(d->character) && IS_OUTDOORS(d->character)) {
					look_at_room(d->character);
					msg_to_char(d->character, "\r\n");
				}
			send_to_outdoor(FALSE, "The night has begun.\r\n");
			break;
		}
	}
	
	// other hourly work
	compute_night_light_radius();
}


/**
* Cascades the main_time_info to the other time zones.
*/
void cascade_time_info(void) {
	struct time_info_data *ptr;
	int iter;
	
	for (iter = 0; iter < 23; ++iter) {
		ptr = &regional_time_info[23-iter];
		
		// start with same values
		ptr->year = main_time_info.year;
		ptr->month = main_time_info.month;
		ptr->day = main_time_info.day;
		
		// adjust hours
		ptr->hours = main_time_info.hours - iter;
		
		// adjust back days/months/years if needed
		if (ptr->hours < 0) {
			ptr->hours += 24;
			ptr->day -= 1;
			if (ptr->day < 0) {
				ptr->day += 30;
				ptr->month -= 1;
				if (ptr->month < 0) {
					ptr->month += 12;
					ptr->year -= 1;
				}
			}
		}
	}
}


/**
* @param room_data *room Any location.
* @return int One of SUN_RISE, SUN_LIGHT, SUN_SET, or SUN_DARK.
*/
int get_sun_status(room_data *room) {
	struct time_info_data *tinfo = local_time_info(room, NULL);
	if (tinfo->hours == 7) {
		return SUN_RISE;
	}
	else if (tinfo->hours == 19) {
		return SUN_SET;
	}
	else if (tinfo->hours > 7 && tinfo->hours < 19) {
		return SUN_LIGHT;
	}
	else {
		return SUN_DARK;
	}
}


/**
* Determines which of the 24 east-west time zones a location is in.
*
* @param room_data *room A room location.
* @param struct map_data *loc Or: a map location (pass one or the other).
* @return int One of the map's time zones (0..23).
*/
int get_time_zone(room_data *room, struct map_data *loc) {
	int x_coord, longitude, region;
	
	// determine location
	x_coord = (loc ? MAP_X_COORD(loc->vnum) : (room ? X_COORD(room) : -1));
	if (x_coord == -1) {
		return 23;	// 23 is the same as the main region
	}
	
	// determine longitude
	longitude = X_TO_LONGITUDE(x_coord);
	
	// determine grouping based on that
	longitude += 180;	// move from (-180 to 180) to (0 to 360)
	region = longitude / 24;	// split into 24
	region = MAX(0, MIN(23, region));	// safety: ensure it's within bounds}
	
	return region;
}


 //////////////////////////////////////////////////////////////////////////////
//// MOON SYSTEM /////////////////////////////////////////////////////////////

/**
* Determines the current phase of the moon based on how long its cycle is, and
* based on how long the mud has been alive.
*
* @param double cycle_days Number of days in the moon's cycle (Earth's moon is 29.53 days real time or 29.13 days game time).
* @return moon_phase_t One of the PHASE_ values.
*/
moon_phase_t get_moon_phase(double cycle_days) {
	double long_count_day, cycle_time;
	int phase;
	
	// exact number of days the mud has been running (all moons are 'new' at the time of DATA_START_WORLD)
	long_count_day = (double)(time(0) - data_get_long(DATA_WORLD_START)) / (double)SECS_PER_MUD_DAY;
	
	// determine how far into the current cycle we are
	if (cycle_days > 0.0) {
		cycle_time = fmod(long_count_day, cycle_days) / cycle_days;
	}
	else {	// div/0 safety
		cycle_time = 0.0;
	}
	
	phase = round((NUM_PHASES-1.0) * cycle_time);
	phase = MAX(0, MIN(NUM_PHASES-1, phase));
	return (moon_phase_t)phase;
}


/**
* Determines where the moon is in the sky. Phases rise roughly 3 hours apart.
*
* @param moon_phase_t phase Any moon phase.
* @param int hour Time of day from 0..23 hours.
*/
moon_pos_t get_moon_position(moon_phase_t phase, int hour) {
	int moonrise = (phase * 3) + 7, moonset = (phase * 3) + 19;
	double percent, pos;
	
	// check bounds/wraparound
	if (moonrise > 23) {
		moonrise -= 24;
	}
	if (moonset > 23) {
		moonset -= 24;
	}
	
	// shift moonset forward IF it's lower than moonrise (moon rises late)
	if (moonset < moonrise) {
		moonset += 24;	// tomorrow
	}
	
	// shift the incoming hour if it's before moonrise to see if it's in tomorrow's moonrise
	if (hour < moonrise) {
		hour += 24;
	}
	
	// now see how far inside or outside of this they are as a percentage
	percent = (double)(hour - moonrise) / (double)(moonset - moonrise);
	
	// outside of that range and the moon is not up
	if (percent <= 0.0 || percent >= 1.0) {
		return MOON_POS_DOWN;
	}
	
	pos = NUM_MOON_POS * percent;
	
	// determine which way to round to give some last-light
	if (percent < 0.5) {
		return (moon_pos_t) ceil(pos);
	}
	else {
		return (moon_pos_t) floor(pos);
	}
}


/**
* Computes the light radius at night based on any visible moon(s) in the sky.
* Fetch this from the night_light_radius global bool.
*
* @return int Number of tiles you can see at night.
*/
void compute_night_light_radius(void) {
	int dist, iter, best[24], second[24];
	generic_data *moon, *next_gen;
	moon_phase_t phase;
	
	int max_light_radius_base = config_get_int("max_light_radius_base");
	
	// init
	for (iter = 0; iter < 24; ++iter) {
		best[iter] = second[iter] = 0;
	}
	
	HASH_ITER(hh, generic_table, moon, next_gen) {
		if (GEN_TYPE(moon) != GENERIC_MOON || GET_MOON_CYCLE(moon) < 1 || GEN_FLAGGED(moon, GEN_IN_DEVELOPMENT)) {
			continue;	// not a moon or invalid cycle
		}
		phase = get_moon_phase(GET_MOON_CYCLE_DAYS(moon));
		
		for (iter = 0; iter < 24; ++iter) {
			if (get_moon_position(phase, regional_time_info[iter].hours) != MOON_POS_DOWN) {
				// moon is up: record it if better
				if (moon_phase_brightness[phase] > best[iter]) {
					second[iter] = best[iter];
					best[iter] = moon_phase_brightness[phase];
				}
				else if (moon_phase_brightness[phase] > second[iter]) {
					second[iter] = moon_phase_brightness[phase];
				}
			}
		}
	}
	
	// compute
	for (iter = 0; iter < 24; ++iter) {
		dist = best[iter] + second[iter]/2;
		dist = MAX(1, MIN(dist, max_light_radius_base));
	
		// save to the global
		night_light_radius[iter] = dist;
	}
}


/**
* Displays any visible moons to the player, one per line.
*
* @param char_data *ch The person to show the moons to.
*/
void show_visible_moons(char_data *ch) {
	struct time_info_data *tinfo;
	char buf[MAX_STRING_LENGTH];
	generic_data *moon, *next_gen;
	moon_phase_t phase;
	moon_pos_t pos;
	
	tinfo = local_time_info(IN_ROOM(ch), NULL);
	
	HASH_ITER(hh, generic_table, moon, next_gen) {
		if (GEN_TYPE(moon) != GENERIC_MOON || GET_MOON_CYCLE(moon) < 1 || GEN_FLAGGED(moon, GEN_IN_DEVELOPMENT)) {
			continue;	// not a moon or invalid cycle
		}
		
		// find moon in the sky
		phase = get_moon_phase(GET_MOON_CYCLE_DAYS(moon));
		pos = get_moon_position(phase, tinfo->hours);
		
		// qualify it some more
		if (pos == MOON_POS_DOWN) {
			continue;	// moon is down
		}
		if (phase == PHASE_NEW && get_sun_status(IN_ROOM(ch)) == SUN_LIGHT) {
			continue;	// new moon not visible in strong sunlight
		}
		
		// ok: show it
		snprintf(buf, sizeof(buf), "%s is %s, %s.\r\n", GEN_NAME(moon), moon_phases_long[phase], moon_positions[pos]);
		send_to_char(CAP(buf), ch);
	}
}
