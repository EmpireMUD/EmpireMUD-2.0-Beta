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

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "skills.h"

// local prototypes
void determine_seasons();


void weather_and_time(int mode) {
	void another_hour(int mode);
	void weather_change();

	another_hour(mode);
	if (mode)
		weather_change();
}


void another_hour(int mode) {
	void process_shipping();

	descriptor_data *d;
	long lny;

	time_info.hours++;

	if (mode) {
		switch (time_info.hours) {
			case 1: {
				// 1am shipment
				process_shipping();
				break;
			}
			case 7:
				weather_info.sunlight = SUN_RISE;
				for (d = descriptor_list; d; d = d->next) {
					if (STATE(d) == CON_PLAYING && !HAS_INFRA(d->character) && !PRF_FLAGGED(d->character, PRF_HOLYLIGHT) && AWAKE(d->character) && IS_OUTDOORS(d->character)) {
						look_at_room(d->character);
						msg_to_char(d->character, "\r\n");
					}
				}
				send_to_outdoor(FALSE, "The sun rises over the horizon.\r\n");
				
				// 7am shipment
				process_shipping();
				break;
			case 8:
				weather_info.sunlight = SUN_LIGHT;
				send_to_outdoor(FALSE, "The day has begun.\r\n");
				break;
			case 12:
				// noon
				break;
			case 13: {
				// 1pm shipment
				process_shipping();
				break;
			}
			case 19:
				weather_info.sunlight = SUN_SET;
				send_to_outdoor(FALSE, "The sun slowly disappears beneath the horizon.\r\n");
				
				// 7pm shipment
				process_shipping();
				break;
			case 20:
				weather_info.sunlight = SUN_DARK;
				for (d = descriptor_list; d; d = d->next)
					if (STATE(d) == CON_PLAYING && !HAS_INFRA(d->character) && !PRF_FLAGGED(d->character, PRF_HOLYLIGHT) &&  AWAKE(d->character) && IS_OUTDOORS(d->character)) {
						look_at_room(d->character);
						msg_to_char(d->character, "\r\n");
					}
				send_to_outdoor(FALSE, "The night has begun.\r\n");
				break;
		}
	}
	if (time_info.hours > 23) {	/* Changed by HHS due to bug ??? */
		time_info.hours -= 24;
		time_info.day++;
		
		determine_seasons();
		
		if (time_info.day > 29) {
			time_info.day = 0;
			time_info.month++;

			if (time_info.month > 11) {
				time_info.month = 0;
				time_info.year++;
				
				annual_world_update();
			}
		}
		else {	// not day 30
			// check if we've missed a new year
			lny = data_get_long(DATA_LAST_NEW_YEAR);
			if (lny && lny + SECS_PER_MUD_YEAR < time(0)) {
				annual_world_update();
			}
		}
	}
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
	day_of_year = time_info.month * 30 + time_info.day;
	
	for (ycoord = 0; ycoord < MAP_HEIGHT; ++ycoord) {
		latitude = Y_TO_LATITUDE(ycoord);
		northern = (latitude >= 0.0);
		
		// tropics? -- take half the tropic value, convert to percent, multiply by map height
		if (ABSOLUTE(latitude) < TROPIC_LATITUDE) {
			if (time_info.month < 2) {
				y_coord_to_season[ycoord] = TILESET_SPRING;
			}
			else if (time_info.month >= 10) {
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
	if ((time_info.month >= 4) && (time_info.month <= 8))
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


/*
 *  Empire Moons 1.0
 *   by Paul Clarke, 10/19/2k
 *
 *  To add a moon: increase NUM_OF_MOONS by 1 and add a line in moons[]
 *  to correspond with your new moon.  Reboot and it's all done for you.
 */

#define NUM_OF_MOONS		1

#define PHASE_NEW				0	/*								*/
#define PHASE_WAXING			1	/*								*/
#define PHASE_FIRST_QUARTER		2	/*  Phases of the moon			*/
#define PHASE_FULL				3	/*								*/
#define PHASE_LAST_QUARTER		4	/*								*/
#define PHASE_WANING			5	/*								*/

struct moon_data {
	char *name;
	byte cycle;			/* Days between full moons */
} moons[NUM_OF_MOONS] = {
	{ "The Moon", 28 }
};


/* Retrieve the current phase of a specific moon */
byte get_phase(int M) {
	int total_time = time_info.day;
	int diff;

	total_time += time_info.month * 30;		/* 30 days in a month	*/
	total_time += time_info.year * 12 * 30;	/* 12 months in a year	*/

	total_time %= moons[M].cycle;

	diff = 100 * total_time / moons[M].cycle;		/* diff is a percentage of the rotation */

	if (diff <= 2 || diff >= 98)
		return PHASE_NEW;
	if (diff >= 23 && diff <= 27)
		return PHASE_FIRST_QUARTER;
	if (diff >= 73 && diff <= 77)
		return PHASE_LAST_QUARTER;
	if (diff >= 48 && diff <= 52)
		return PHASE_FULL;
	if (diff < 50)
		return PHASE_WAXING;
	else
		return PHASE_WANING;
}


void list_moons_to_char(char_data *ch) {
	int M, i;
	char moon_str[112];

	if (NUM_OF_MOONS == 0)
		return;

	*buf = '\0';

	for (M = 0; M < NUM_OF_MOONS; M++) {
		*moon_str = '\0';
		switch(get_phase(M)) {
			case PHASE_WAXING:			sprintf(moon_str, "waxing");				break;
			case PHASE_FIRST_QUARTER:	sprintf(moon_str, "in its first quarter");	break;
			case PHASE_FULL:			sprintf(moon_str, "full");					break;
			case PHASE_LAST_QUARTER:	sprintf(moon_str, "in its last quarter");	break;
			case PHASE_WANING:			sprintf(moon_str, "waning");				break;
		}
		if (*buf)
			strcat(buf, ", ");
		if (*moon_str)
			sprintf(buf+strlen(buf), "%s is %s", moons[M].name, moon_str);
	}

	if (!*buf)
		sprintf(buf, "You can't see the moon%s right now", NUM_OF_MOONS > 1 ? "s" : "");

	strcat(buf, ".\r\n");

	/* This will find the last comma and replace it with an "and" */
	for (i = strlen(buf)-1; i > 0; i--)
		if (buf[i] == ',') {
			sprintf(buf1, "%s", buf + i+1);
			buf[i] = '\0';
			strcat(buf, " and");
			strcat(buf, buf1);
			break;
		}
	send_to_char(buf, ch);
}


byte distance_can_see(char_data *ch) {
	int M, p, a = 0, b = 0, c = 0;

	for (M = 0; M < NUM_OF_MOONS; M++) {
		switch (get_phase(M)) {
			case PHASE_FULL:			c = 4;		break;
			case PHASE_FIRST_QUARTER:
			case PHASE_LAST_QUARTER:	c = 3;		break;
			case PHASE_WAXING:
			case PHASE_WANING:			c = 2;		break;
			default:					c = 1;		break;
		}
		if (c > a)
			a = c;
		else if (c > b)
			b = c;
	}
	p = a + b;
	p = MIN(5, p);

	if (has_player_tech(ch, PTECH_LARGER_LIGHT_RADIUS)) {
		p += 2;
	}

	if (IS_LIGHT(IN_ROOM(ch)))
		p++;

	return p;
}
