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

// external vars
extern unsigned long pulse;

// local prototypes
void another_hour();
void send_hourly_sun_messages();
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
	day_of_year = DAY_OF_YEAR(main_time_info);
	
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


/**
* Reset weather data on startup (or request).
*/
void reset_weather(void) {
	weather_info.pressure = 960;
	if ((main_time_info.month >= 5) && (main_time_info.month <= 8)) {
		weather_info.pressure += number(1, 50);
	}
	else {
		weather_info.pressure += number(1, 80);
	}

	weather_info.change = 0;

	if (weather_info.pressure <= 980) {
		weather_info.sky = SKY_LIGHTNING;
	}
	else if (weather_info.pressure <= 1000) {
		weather_info.sky = SKY_RAINING;
	}
	else if (weather_info.pressure <= 1020) {
		weather_info.sky = SKY_CLOUDY;
	}
	else {
		weather_info.sky = SKY_CLOUDLESS;
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
* Advances time by an hour and triggers things which happen on specific hours.
*/
void another_hour(void) {
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
	
	// hour-based updates
	switch (main_time_info.hours) {
		case 0: {	// midnight
			run_external_evolutions();
			break;
		}
		case 1: {	// 1am shipment
			process_shipping();
			break;
		}
		case 7: {	// 7am shipment and world save
			process_shipping();
			save_whole_world();
			break;
		}
		case 12: {	// noon
			process_imports();
			break;
		}
		case 13: {	// 1pm shipment
			process_shipping();
			break;
		}
		case 19: {	// 7pm shipment
			process_shipping();
			break;
		}
	}
	
	// and announce it to the players
	send_hourly_sun_messages();
}


/**
* Determines the number of hours of sunlight in the room today, based on
* latitude and time of year.
*
* @param room_data *room The location.
* @return double The number of hours of sunlight today.
*/
double get_hours_of_sun(room_data *room, bool debug) {
	double latitude, days_percent, max_hours;
	struct time_info_data tinfo;
	int y_coord, doy;
	double hours = 12.0;
	
	// this takes latitude in degrees but uses it in radians for the tangent...
	// it fits the graph we need though
	#define HOURS_SUN_AT_SOLSTICE(latitude, flip)  (((flip) ? -2.5 : 2.5) * tan((latitude) / 48) + 12)
	
	if ((y_coord = Y_COORD(room)) == -1) {
		return hours;	// no location: default
	}
	latitude = Y_TO_LATITUDE(y_coord);
	tinfo = get_local_time(room);
	doy = DAY_OF_YEAR(tinfo);
	
	// bound it to -67..67 because anything beyond that is in the arctic circle
	latitude = MAX(-67.0, MIN(67.0, latitude));
	
	// set max_hours to the number of hours at the solstice
	// and set days_percent to percent of the way to that solstice from the equinox
	if (doy >= FIRST_EQUINOX_DOY && doy < NORTHERN_SOLSTICE_DOY) {
		// march-june: days before the solstice
		max_hours = HOURS_SUN_AT_SOLSTICE(latitude, (latitude < 0));
		days_percent = (doy - FIRST_EQUINOX_DOY) / 90.0;
	}
	else if (doy >= NORTHERN_SOLSTICE_DOY && doy < LAST_EQUINOX_DOY) {
		// june-september: days after the solstice
		max_hours = HOURS_SUN_AT_SOLSTICE(latitude, (latitude < 0));
		days_percent = 1.0 - ((doy - NORTHERN_SOLSTICE_DOY) / 90.0);
	}
	else if (doy >= LAST_EQUINOX_DOY && doy < SOUTHERN_SOLSTICE_DOY) {
		// september-december: days before the solstice
		max_hours = HOURS_SUN_AT_SOLSTICE(latitude, (latitude > 0));
		days_percent = (doy - LAST_EQUINOX_DOY) / 90.0;
	}
	else {
		// december-march: days after the solstice
		if (doy < SOUTHERN_SOLSTICE_DOY) {
			doy += 360;	// to make it "days after the solstice"
		}
		max_hours = HOURS_SUN_AT_SOLSTICE(latitude, (latitude < 0));
		days_percent = 1.0 - ((doy - SOUTHERN_SOLSTICE_DOY) / 90.0);
	}
	
	if (max_hours > 12.0) {
		hours = days_percent * (max_hours - 12.0) + 12.0;
	}
	else if (max_hours < 12.0) {
		hours = 12.0 - (days_percent * (12.0 - max_hours));
	}
	
	if (debug) {
		log("lat=%.2f, doy=%d, perc=%.2f, max_hours=%.2f, hours=%.2f", latitude, doy, days_percent, max_hours, hours);
	}
	
	// bound it to 0-24 hours of daylight
	return MAX(0.0, MIN(24.0, hours));
}


/**
* Determines exact local time based on east/west position.
*
* @param room_data *room The location.
* @return struct time_info_data The local time data.
*/
struct time_info_data get_local_time(room_data *room) {
	double longitude, percent, minutes_dec;
	struct time_info_data tinfo;
	int x_coord;
	
	// ensure we're using local time & determine location
	if (!config_get_bool("use_local_time") || (x_coord = (room ? X_COORD(room) : -1)) == -1) {
		return main_time_info;
	}
	
	// determine longitude
	longitude = X_TO_LONGITUDE(x_coord) + 180.0;	// longitude from 0-360 instead of -/+180
	percent = 1.0 - (longitude / 360.0);	// percentage of the way west
	
	tinfo = main_time_info;	// copy
	
	// adjust hours backward for distance from east end
	minutes_dec = ((pulse / PASSES_PER_SEC) % SECS_PER_MUD_HOUR) / (double)SECS_PER_MUD_HOUR;
	tinfo.hours -= ceil(24.0 * percent - minutes_dec);
	
	// adjust back days/months/years if needed
	if (tinfo.hours < 0) {
		tinfo.hours += 24;
		if (--tinfo.day < 0) {
			tinfo.day += 30;
			if (--tinfo.month < 0) {
				tinfo.month += 12;
				--tinfo.year;
			}
		}
	}
	
	return tinfo;
}


/**
* @param room_data *room Any location.
* @return int One of SUN_RISE, SUN_LIGHT, SUN_SET, or SUN_DARK.
*/
int get_sun_status(room_data *room) {
	// struct time_info_data tinfo = get_local_time(room);
	double hours, sun_mod, longitude, percent;
	int x_coord;
	
	if ((x_coord = X_COORD(room)) == -1) {
		// no x-coord (not in a mappable spot)
		hours = main_time_info.hours + (((pulse / PASSES_PER_SEC) % SECS_PER_MUD_HOUR) / (double)SECS_PER_MUD_HOUR);
	}
	else {
		// determine exact time
		longitude = X_TO_LONGITUDE(x_coord) + 180.0;	// longitude from 0-360 instead of -/+180
		percent = 1.0 - (longitude / 360.0);	// percentage of the way west
		hours = main_time_info.hours - (24.0 * percent - ((pulse / PASSES_PER_SEC) % SECS_PER_MUD_HOUR) / (double)SECS_PER_MUD_HOUR);
		if (hours < 0.0) {
			hours += 24.0;
		}
	}
	
	
	// sun_mod is subtracted in the morning and added in the evening
	sun_mod = (get_hours_of_sun(room, FALSE) - 12.0) / 2.0;
	// hours = tinfo.hours + (((pulse / PASSES_PER_SEC) % SECS_PER_MUD_HOUR) / (double)SECS_PER_MUD_HOUR);
	
	if (ABSOLUTE(hours - (7.0 - sun_mod)) < 0.5) {
		return SUN_RISE;
	}
	else if (ABSOLUTE(hours - (19.0 + sun_mod)) < 0.5) {
		return SUN_SET;
	}
	else if (hours > 7.0 - sun_mod && hours < 19.0 + sun_mod) {
		return SUN_LIGHT;
	}
	else {
		return SUN_DARK;
	}
}


/**
* Gets the numbers of days from the solstice a given room will experience
* zenith passage -- the day(s) of the year where the sun passes directly over-
* head at noon. This only occurs in the tropics; everywhere else returns -1.
*
* @param room_data *room The location.
* @return int Number of days before/after the solstice that the zenith occurs (or -1 if no zenith).
*/
int get_zenith_days_from_solstice(room_data *room) {
	double latitude, percent_from_solstice;
	int y_coord;
	
	if ((y_coord = Y_COORD(room)) == -1) {
		return -1;	// exit early if not on the map
	}
	
	latitude = Y_TO_LATITUDE(y_coord);
	if (latitude > TROPIC_LATITUDE || latitude < -TROPIC_LATITUDE) {
		return -1;	// not in the tropics
	}
	
	// percent of days from the solstice to equinox that the zenith happens
	percent_from_solstice = 1.0 - ABSOLUTE(latitude / TROPIC_LATITUDE);
	return (int)round(percent_from_solstice * 90);	// 90 days in 1/4 year
}


/**
* Determines whether or not today is the day of zenith passage -- the day(s) of
* the year where the sun passes directly overhead at noon. This only occurs in
* the tropics; everywhere else returns FALSE all year.
*
* @param room_data *room The location to check.
* @return bool TRUE if the current day (in that room) is the day of zenith passage, or FALSE if not.
*/
bool is_zenith_day(room_data *room) {
	int zenith_days, y_coord, doy;
	struct time_info_data tinfo;
	
	if ((y_coord = Y_COORD(room)) == -1 || (zenith_days = get_zenith_days_from_solstice(room)) < 0) {
		return FALSE;	// exit early if not on the map or no zenith
	}
	
	// get day of year
	tinfo = get_local_time(room);
	
	if (Y_TO_LATITUDE(y_coord) > 0) {
		// northern hemisphere: no need to wrap here (unlike the southern solstice)
		return (ABSOLUTE(NORTHERN_SOLSTICE_DOY - DAY_OF_YEAR(tinfo)) == zenith_days);
	}
	else {
		// southern hemisphere
		doy = DAY_OF_YEAR(tinfo);
		if (doy < 90) {
			// wrap it around to put it in range of the december solstice
			doy += 360;
		}
		return (ABSOLUTE(SOUTHERN_SOLSTICE_DOY - doy) == zenith_days);
	}
}


/**
* To be called at the end of the hourly update to show players sunrise/sunset.
*/
void send_hourly_sun_messages(void) {
	struct time_info_data tinfo;
	descriptor_data *desc;
	
	LL_FOREACH(descriptor_list, desc) {
		if (STATE(desc) != CON_PLAYING || desc->character == NULL) {
			continue;
		}
		if (!AWAKE(desc->character) || !IS_OUTDOORS(desc->character)) {
			continue;
		}
		
		// get local time
		tinfo = get_local_time(IN_ROOM(desc->character));
		
		switch (tinfo.hours) {
			case 7: {	// sunrise
				// show map if needed
				if (!HAS_INFRA(desc->character) && !PRF_FLAGGED(desc->character, PRF_HOLYLIGHT) && get_sun_status(IN_ROOM(desc->character)) != GET_LAST_LOOK_SUN(desc->character)) {
					look_at_room(desc->character);
					msg_to_char(desc->character, "\r\n");
				}
				msg_to_char(desc->character, "The sun rises over the horizon.\r\n");
				break;
			}
			case 8: {	// day start
				msg_to_char(desc->character, "The day has begun.\r\n");
				break;
			}
			case 12: {	// noon
				if (is_zenith_day(IN_ROOM(desc->character))) {
					msg_to_char(desc->character, "You watch as the sun passes directly overhead -- today is the zenith passage!\r\n");
				}
				break;
			}
			case 19: {	// sunset
				msg_to_char(desc->character, "The sun slowly disappears beneath the horizon.\r\n");
				break;
			}
			case 20: {	// dark
				// show map if needed
				if (!HAS_INFRA(desc->character) && !PRF_FLAGGED(desc->character, PRF_HOLYLIGHT) && get_sun_status(IN_ROOM(desc->character)) != GET_LAST_LOOK_SUN(desc->character)) {
					look_at_room(desc->character);
					msg_to_char(desc->character, "\r\n");
				}
				msg_to_char(desc->character, "The night has begun.\r\n");
				break;
			}
		}
	}
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
*
* @param room_data *room A location.
* @return int Number of tiles you can see at night in that room.
*/
int compute_night_light_radius(room_data *room) {
	generic_data *moon, *next_gen;
	struct time_info_data tinfo;
	int dist, best = 0, second = 0;
	moon_phase_t phase;
	
	int max_light_radius_base = config_get_int("max_light_radius_base");
	
	tinfo = get_local_time(room);
	
	HASH_ITER(hh, generic_table, moon, next_gen) {
		if (GEN_TYPE(moon) != GENERIC_MOON || GET_MOON_CYCLE(moon) < 1 || GEN_FLAGGED(moon, GEN_IN_DEVELOPMENT)) {
			continue;	// not a moon or invalid cycle
		}
		phase = get_moon_phase(GET_MOON_CYCLE_DAYS(moon));
		if (get_moon_position(phase, tinfo.hours) != MOON_POS_DOWN) {
			// moon is up: record it if better
			if (moon_phase_brightness[phase] > best) {
				second = best;
				best = moon_phase_brightness[phase];
			}
			else if (moon_phase_brightness[phase] > second) {
				second = moon_phase_brightness[phase];
			}
		}
	}
	
	// compute
	dist = best + second/2;
	dist = MAX(1, MIN(dist, max_light_radius_base));
	return dist;
}


/**
* Lets a player look at a moon by name.
*
* @param char_data *ch The player.
* @param char *name The argument typed by the player after 'look [at]'.
* @param int *number Optional: For multi-list number targeting (look 4.moon; may be NULL)
*/
bool look_at_moon(char_data *ch, char *name, int *number) {
	char buf[MAX_STRING_LENGTH], copy[MAX_INPUT_LENGTH], *tmp = copy;
	struct time_info_data tinfo;
	generic_data *moon, *next_gen;
	moon_phase_t phase;
	moon_pos_t pos;
	int num;
	
	if (!IS_OUTDOORS(ch)) {
		return FALSE;
	}
	
	skip_spaces(&name);
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {	// can't target 0.moon
		return FALSE;
	}
	
	tinfo = get_local_time(IN_ROOM(ch));
	
	HASH_ITER(hh, generic_table, moon, next_gen) {
		if (GEN_TYPE(moon) != GENERIC_MOON || GET_MOON_CYCLE(moon) < 1 || GEN_FLAGGED(moon, GEN_IN_DEVELOPMENT)) {
			continue;	// not a moon or invalid cycle
		}
		if (!isname(tmp, GEN_NAME(moon))) {
			continue;	// not a name match
		}
		
		// find moon in the sky
		phase = get_moon_phase(GET_MOON_CYCLE_DAYS(moon));
		pos = get_moon_position(phase, tinfo.hours);
		
		// qualify it some more -- allow new moon in direct sunlight (unlike show-visible-moons)
		if (pos == MOON_POS_DOWN) {
			continue;	// moon is down
		}
		if (--(*number) > 0) {
			continue;	// number-dot syntax
		}
		
		// ok: show it
		snprintf(buf, sizeof(buf), "%s is %s, %s.\r\n", GEN_NAME(moon), moon_phases_long[phase], moon_positions[pos]);
		send_to_char(CAP(buf), ch);
		act("$n looks at $t.", TRUE, ch, GEN_NAME(moon), NULL, TO_ROOM);
		return TRUE;
	}
	
	return FALSE;	// nope
}


/**
* Displays any visible moons to the player, one per line.
*
* @param char_data *ch The person to show the moons to.
*/
void show_visible_moons(char_data *ch) {
	struct time_info_data tinfo;
	char buf[MAX_STRING_LENGTH];
	generic_data *moon, *next_gen;
	moon_phase_t phase;
	moon_pos_t pos;
	
	tinfo = get_local_time(IN_ROOM(ch));
	
	HASH_ITER(hh, generic_table, moon, next_gen) {
		if (GEN_TYPE(moon) != GENERIC_MOON || GET_MOON_CYCLE(moon) < 1 || GEN_FLAGGED(moon, GEN_IN_DEVELOPMENT)) {
			continue;	// not a moon or invalid cycle
		}
		
		// find moon in the sky
		phase = get_moon_phase(GET_MOON_CYCLE_DAYS(moon));
		pos = get_moon_position(phase, tinfo.hours);
		
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
