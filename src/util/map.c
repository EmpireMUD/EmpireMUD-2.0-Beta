/* ************************************************************************
*   File: map.c                                           EmpireMUD 2.0b5 *
*  Usage: procedural map generator for EmpireMUD                          *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/**
* WARNING: Running this program will overwrite your world files. You should
* only do this when the mud is down, or shutting it down will overwrite the
* generated files.
*
* Steps:
*  1. go to lib/world/wld
*  2. run ./map
*  2a. you may have to "chmod u+x map" to run it
*  2b. you may need to raise the stack size limit: ulimit -s <limit>
*  2c. this will generate new .wld files and a new index -- you must to delete
*      your lib/world/base_map file or it will combine with your new .wld files
*  3. make sure the stats output looks good
*  4. you can use the map.txt data file with your map.php image generator to
*     see if the world looks good to you
*  5. when you're happy with it, start up the mud
*
*  NOTE: A new version of the map generator which creates the base_map file
*  itself is planned for EmpireMUD 2.0b5.
*
* Bonus feature: shift an existing map east/west
*  1. generate a new map (do NOT use this on a live game map)
*  2. run ./map shift <distance>
*  3. distance is number of map tiles to shift east (negative for west)
*  3. it will load the map, shift it, and then save it again
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "../conf.h"
#include "../sysdep.h"

#include "../structs.h"

/**
* Contents:
*  Config And Setup
*  Macros
*  Main Generator
*  Random Generator
*  Island Numbering
*  Helper Functions
*  Map Generator Functions
*  Bonus Features
*/

// helper function protos
inline void change_grid(int loc, int type);
void clear_pass(void);
struct island_data *closest_island(int x, int y);
inline int compute_distance(int x1, int y1, int x2, int y2);
void empire_srandom(unsigned long initial_seed);
int find_border(struct island_data *isle, int x_dir, int y_dir);
int get_line(FILE *fl, char *buf);
double get_percent_type(int type);
void init_grid(void);
int number(int from, int to);
void output_stats(void);
void print_map_graphic(void);
void print_map_to_files(void);
struct num_data_t *pop_ndt(void);
void push_ndt(int loc);
int sect_to_terrain(int sect_vnum);
inline int shift(int origin, int x_shift, int y_shift);
void shift_map_x(int amt);

// map generator protos
void add_latitude_terrain(int type, double y_start, double y_end);
void add_mountains(struct island_data *isle);
void add_pass(struct island_data *isle);
void add_river(struct island_data *isle);
void add_start_points(void);
void add_tundra(void);
void center_map(void);
void complete_map(void);
void create_islands(void);
void land_mass(struct island_data *isle, int min_radius, int max_radius);
void number_islands_and_fix_lakes();
void replace_near(int from, int to, int near, int dist);
void splotch(int room, int type, struct island_data *isle);

// bonus feature protos
void load_and_shift_map(int dist);


 //////////////////////////////////////////////////////////////////////////////
//// CONFIG AND SETUP ////////////////////////////////////////////////////////

// these may be the same size as the config in structs.h, or different
#define USE_WIDTH  MAP_WIDTH	// from structs.h
#define USE_HEIGHT  MAP_HEIGHT	// from structs.h

#define USE_WRAP_X  WRAP_X	// from structs.h
#define USE_WRAP_Y  WRAP_Y	// from structs.h


// master controls
#define TARGET_LAND_SIZE  333000	// how much ocean to convert to land -- THIS WILL DETERMINE HOW MUCH RAM YOUR MUD USES (333k = about 130 MB RAM)
#define NUM_START_POINTS  1
#define TUNDRA_HEIGHT  1	// tiles of tundra at top/bottom

// jungle replaces temperate terrain
#define JUNGLE_START_PRC  30	// % up from bottom of map where jungle starts
#define JUNGLE_END_PRC  70	// % up from bottom of map where jungle ends

// desert overrides jungle
#define DESERT_START_PRC  36.6	// % up from bottom where desert starts
#define DESERT_END_PRC  63.3	// % up from bottom where desert ends

// need these here
#ifndef NULL
	#define NULL (void *)0
#endif
#ifndef TRUE
	#define TRUE  1
#endif
#ifndef FALSE
	#define FALSE  !TRUE
#endif
#ifdef MAX
	#undef MAX
#endif
#define MAX(a, b)		((a) > (b) ? (a) : (b))
#ifdef MIN
	#undef MIN
#endif
#define MIN(a, b)		((a) < (b) ? (a) : (b))


// data for how to build clusters
struct {
	int min_radius, max_radius;
	int cluster_dist, cluster_size;
} island_types[] = {
	// It iterates repeatedly over this list until it's out of space:
	{ 30, 60, 45, 16 },	// big islands, cluster of 16 (continents)
	{ 10, 60, 45, 4 },	// medium size, cluster of 4
	{ 10, 30, 20, 8 },	// small size, cluster of 8
	{ 10, 30, 10, 8 },	// small size, tightly packed, cluster of 8
	{ 10, 30, 15, 3 },	// tiny cluster
	{ 10, 30, 15, 3 },	// tiny cluster
	
	{ -1, -1, -1, -1 }	// last
};


// Define the terrains we're going to use
#define PLAINS			0
#define FOREST			1
#define RIVER			2
#define OCEAN			3
#define MOUNTAIN		4
#define TEMPERATE_CROP	5
#define DESERT			6
#define TOWER			7
#define JUNGLE			8
#define OASIS			9
#define GROVE			10
#define SWAMP			11
#define TUNDRA          12
#define LAKE			13
#define DESERT_CROP		14
#define JUNGLE_CROP		15
#define NUM_MAP_SECTS	16	/* Total */

// terrain data
struct {
	char *mapout_icon;	// 1-char icon for map data file output
	char *name;	// string name for stats list
	int sector_vnum;	// for .wld file output
	int is_land;	// for ocean or anything that doesn't connect islands
} terrains[NUM_MAP_SECTS] = {
	{ "b", "Plains", 0, TRUE },
	{ "f", "Forest", 4, TRUE },
	{ "i", "River", 5, TRUE },
	{ "k", "Ocean", 6, FALSE },
	{ "q", "Mountain", 8, TRUE },
	{ "b", "Tmp. Crop", 7, TRUE },
	{ "m", "Desert", 20, TRUE },
	{ "*", "Tower", 18, TRUE },
	{ "d", "Jungle", 28, TRUE },
	{ "j", "Oasis", 21, TRUE },
	{ "b", "Grove", 26, TRUE },
	{ "e", "Swamp", 29, TRUE },
	{ "h", "Tundra", 30, FALSE },
	{ "i", "Lake", 32, TRUE },
	{ "o", "Des. Crop", 12, TRUE },
	{ "3", "Jung. Crop", 16, TRUE },
};


 //////////////////////////////////////////////////////////////////////////////
//// MACROS //////////////////////////////////////////////////////////////////

// Directions
#define NORTH			0
#define EAST			1
#define SOUTH			2
#define WEST			3
#define NOWE			4
#define NOEA			5
#define SOWE			6
#define SOEA			7
#define NUM_DIRS  8

// how much to shift x/y to move in a direction
const int shift_dir[][2] = {
	{ 0, 1 },	// north
	{ 1, 0 },	// east
	{ 0, -1},	// south
	{-1, 0 },	// west
	{-1, 1 },	// nw
	{ 1, 1 },	// ne
	{-1, -1},	// sw
	{ 1, -1},	// se
};


// various computed sizes
#define USE_SIZE  (USE_WIDTH * USE_HEIGHT)
#define USE_BLOCK_SIZE  (USE_WIDTH * 5)
#define NUM_BLOCKS  (USE_SIZE / USE_BLOCK_SIZE)

// macros
#define MAP(x, y)  ((y) * USE_WIDTH + (x))
#define X_COORD(room)  ((room) % USE_WIDTH)
#define Y_COORD(room)  ((room) / USE_WIDTH)
#define IS_IN_Y_PRC_RANGE(ycoord, startprc, endprc)  (ycoord >= round(startprc / 100.0 * USE_HEIGHT) && ycoord <= round(endprc / 100.0 * USE_HEIGHT))


/* The master grid (USE_WIDTH, USE_HEIGHT) */
struct grid_type {
	int type;
	int pass;	// used to control fresh paint and prevent hideous conglomermountains
	int island_id;	// island data
};


// Island (nucleus) data structure
struct island_data {
	int loc;						/* Location of island in grid[]		*/
	int width[4];					/* Maximum width of island			*/

	struct island_data *next;
};


#define CREATE(result, type, number)  do {\
	if ((number) * sizeof(type) <= 0)	\
		perror("SYSERR: Zero bytes or less requested");	\
	if (!((result) = (type *) calloc ((number), sizeof(type))))	\
		{ perror("SYSERR: malloc failure"); abort(); } } while(0)


// locals
struct grid_type grid[USE_SIZE];	// main working grid
int total_ocean = 0;	// keep track of ocean to speed up counts
struct island_data *island_list = NULL;	// Master list


 //////////////////////////////////////////////////////////////////////////////
//// MAIN GENERATOR //////////////////////////////////////////////////////////

void create_map(void) {
	struct island_data *isle;

	init_grid();
	
	// make a ton of plains islands
	printf("Generating islands with %d total land target...\n", TARGET_LAND_SIZE);
	create_islands();

	printf("Adding mountains and rivers...\n");
	for (isle = island_list; isle; isle = isle->next) {
		// fillings based on location (it's not desert or jungle YET, so we check prcs
		if (IS_IN_Y_PRC_RANGE(Y_COORD(isle->loc), DESERT_START_PRC, DESERT_END_PRC)) {
			// desert
			add_mountains(isle);
			// rare chance of river
			if (!number(0, 2)) {
				add_river(isle);
			}
		}
		else if (IS_IN_Y_PRC_RANGE(Y_COORD(isle->loc), JUNGLE_START_PRC, JUNGLE_END_PRC)) {
			// jungle
			// chance of mountain
			if (!number(0, 1)) {
				add_mountains(isle);
			}
			// always get extra river
			add_river(isle);
			add_river(isle);
		}
		else {
			// not jungle or desert
			add_mountains(isle);
			add_river(isle);
			
			// chance to add additional river
			if (!number(0, 2)) {
				add_river(isle);
			}
		}

		// not currently adding passes
		// add_pass(isle);
	}
	
	// these really need to go in order, as they modify the map in passes
	printf("Adding desert...\n");
	add_latitude_terrain(DESERT, DESERT_START_PRC, DESERT_END_PRC);
	printf("Adding jungle...\n");
	add_latitude_terrain(JUNGLE, JUNGLE_START_PRC, JUNGLE_END_PRC);
	
	printf("Numbering islands and fixing lakes...\n");
	number_islands_and_fix_lakes();
	
	printf("Irrigating from rivers...\n");
	replace_near(DESERT, PLAINS, RIVER, 2);
	replace_near(DESERT, PLAINS, LAKE, 2);
	replace_near(JUNGLE, SWAMP, RIVER, 2);
	replace_near(JUNGLE, SWAMP, LAKE, 2);

	// tundra if no y-wrap
	if (!USE_WRAP_Y && TUNDRA_HEIGHT >= 1) {
		printf("Adding tundra...\n");
		add_tundra();
	}
	
	// center maps if they use only x-wrap
	if (USE_WRAP_X && !USE_WRAP_Y) {
		printf("Centering map horizontally...\r\n");
		center_map();
	}
	
	// finish up the map
	printf("Finishing map...\n");
	complete_map();
	add_start_points();
}


// main game execution
int main(int argc, char **argv) {
	empire_srandom(time(0));
	
	if (argc > 1 && !strcmp(argv[1], "shift")) {
		if (argc <= 2) {
			printf("Usage: %s shift <distance>\n", argv[0]);
			exit(1);
		}
		
		load_and_shift_map(atoi(argv[2]));
		return 0;
	}
	else if (argc > 1) {
		printf("Unknown mode: %s\n", argv[1]);
	}

	create_map();

	print_map_graphic();
	print_map_to_files();

	printf("Done.\n");
	output_stats();
	return (0);
}


 //////////////////////////////////////////////////////////////////////////////
//// RANDOM GENERATOR ////////////////////////////////////////////////////////

// borrowed from the EmpireMUD/CircleMUD base,
/* This program is public domain and was written by William S. England (Oct 1988) */

#define	mM  (unsigned long)2147483647
#define	qQ  (unsigned long)127773
#define	aA (unsigned int)16807
#define	rR (unsigned int)2836
static unsigned long seed;

void empire_srandom(unsigned long initial_seed) {
    seed = initial_seed;
}

unsigned long empire_random(void) {
	register int lo, hi, test;

	hi = seed/qQ;
	lo = seed%qQ;

	test = aA*lo - rR*hi;

	if (test > 0)
		seed = test;
	else
		seed = test+ mM;

	return (seed);
}

// core number generator
int number(int from, int to) {
	// shortcut -paul 12/9/2014
	if (from == to) {
		return from;
	}
	
	/* error checking in case people call number() incorrectly */
	if (from > to) {
		int tmp = from;
		from = to;
		to = tmp;
	}

	return ((empire_random() % (to - from + 1)) + from);
}


 //////////////////////////////////////////////////////////////////////////////
//// ISLAND NUMBERING ////////////////////////////////////////////////////////

struct num_data_t {
	int loc;
	struct num_data_t *next;	// LL
};

struct num_data_t *ndt_stack = NULL;	// global list


// pops an item from the stack and returns it, or NULL if empty
struct num_data_t *pop_ndt(void) {
	struct num_data_t *temp = NULL;
	
	if (ndt_stack) {
		temp = ndt_stack;
		ndt_stack = temp->next;
	}
	
	return temp;
}


// push a location onto the stack
void push_ndt(int loc) {
	struct num_data_t *ndt;
	CREATE(ndt, struct num_data_t, 1);
	ndt->loc = loc;
	ndt->next = ndt_stack;
	ndt_stack = ndt;
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPER FUNCTIONS ////////////////////////////////////////////////////////

/**
* Changes the grid safely.
*
* @param int loc Where to change.
* @param int type What type to change to (e.g. PLAINS).
*/
inline void change_grid(int loc, int type) {
	// sanity
	if (loc == -1 || loc >= USE_SIZE) {
		return;
	}
	
	if (grid[loc].type == OCEAN) {
		--total_ocean;
	}
	
	grid[loc].type = type;
	
	if (type == OCEAN) {
		++total_ocean;
	}
}


// clears all pass data
void clear_pass(void) {
	int iter;
	
	for (iter = 0; iter < USE_SIZE; ++iter) {
		grid[iter].pass = 0;
	}
}


// Find an island entry
struct island_data *closest_island(int x, int y) {
	struct island_data *isle, *best = NULL;
	int b = USE_SIZE;

	for (isle = island_list; isle; isle = isle->next)
		if (compute_distance(X_COORD(isle->loc), Y_COORD(isle->loc), x, y) < b) {
			best = isle;
			b = compute_distance(X_COORD(isle->loc), Y_COORD(isle->loc), x, y);
		}

	return best;
}


// quick distance computation
inline int compute_distance(int x1, int y1, int x2, int y2) {
	int dist;
	
	dist = ((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
	dist = (int) sqrt(dist);
	
	return dist;
}


/* Returns a room on the island/ocean border in a given dir */
int find_border(struct island_data *isle, int x_dir, int y_dir) {
	int i, loc;

	/* Track to the border of the island */
	for (i = 0; (loc = shift(isle->loc, i*x_dir, i*y_dir)) != -1 && terrains[grid[loc].type].is_land; i++);

	/* We went 1 too far */
	i--;

	return (shift(isle->loc, i*x_dir, i*y_dir));
}


// reads a line from a file
int get_line(FILE *fl, char *buf) {
	char temp[256];
	int lines = 0;

	do {
		fgets(temp, 256, fl);
		if (feof(fl)) {
			return (0);
		}
		lines++;
	} while (*temp == '*' || *temp == '\n');

	temp[strlen(temp) - 1] = '\0';
	strcpy(buf, temp);
	return (lines);
}


// fetch what percent of the world is a given type
double get_percent_type(int type) {
	int iter, count;
	
	for (iter = 0, count = 0; iter < USE_SIZE; ++iter) {
		if (grid[iter].type == type) {
			++count;
		}
	}
	
	return (count * 100.0 / USE_SIZE);
}


// Set the whole world to one GIANT ocean
void init_grid(void) {
	int i;

	for (i = 0; i < USE_SIZE; i++) {
		grid[i].type = OCEAN;
		grid[i].island_id = -1;
		grid[i].pass = 0;
	}
	total_ocean = USE_SIZE;
}


/* Display the usage to the user */
void output_stats(void) {
	int total[NUM_MAP_SECTS+1], max_island = 0;
	int i;

	for (i = 0; i < NUM_MAP_SECTS + 1; i++) {
		total[i] = 0;
	}

	for (i = 0; i < USE_SIZE; i++) {
		total[(int) grid[i].type + 1]++;
		max_island = MAX(max_island, grid[i].island_id);
	}

	for (i = 0; i < NUM_MAP_SECTS + 1; i++)
		printf("%-10.10s: %7d %4.2f%% %s", i == 0 ? "ERROR" : terrains[i-1].name, total[i], ((double)total[i] * 100 / (double)USE_SIZE), !((i+1)%3) ? "\n" : " ");
	printf("\n");
	printf("Number of islands: %d\n", max_island);
}


// creates the data file for the graphic map
void print_map_graphic(void) {
	FILE *out;
	int i;

	out = fopen("map.txt", "w");

	fprintf(out, "%dx%d\n", USE_WIDTH, USE_HEIGHT);
	for (i = 0; i < USE_SIZE; i++) {
		fprintf(out, "%s", terrains[grid[i].type].mapout_icon);
		if (!((i+1) % USE_WIDTH))
			fprintf(out, "\n");
		}

	fclose(out);

	return;
}


// outputs the .wld data files
void print_map_to_files(void) {
	FILE *out, *index_fl;
	int i, j, pos;
	char fname[256];
	
	if (!(index_fl = fopen("index", "w"))) {
		printf("Unable to write index file!\n");
		exit(0);
	}

	for (i = 0; i < NUM_BLOCKS; i++) {
		sprintf(fname, "%d.wld", i);
		if (!(out = fopen(fname, "w"))) {
			printf("Unable to write file %s!\n", fname);
			exit(0);
		}
		
		// add to index
		fprintf(index_fl, "%s\n", fname);

		for (j = 0; j < USE_BLOCK_SIZE; j++) {
			pos = i * USE_BLOCK_SIZE + j;
			
			if (grid[pos].type != OCEAN) {
				fprintf(out, "#%d\n", pos);
				fprintf(out, "%d %d %d\n", grid[pos].island_id, terrains[grid[pos].type].sector_vnum, terrains[grid[pos].type].sector_vnum);
				fprintf(out, "S\n");
			}
		}
		fprintf(out, "$~\n");
		fclose(out);
	}
	
	// end index file
	fprintf(index_fl, "$\n");
	fclose(index_fl);

	return;
}


// This takes an initial point 'origin' and translates it by x,y
// may return -1 if no valid location
inline int shift(int origin, int x_shift, int y_shift) {
	int loc = origin, y_coord, x_coord;
	
	// sanity
	if (loc < 0 || loc >= USE_SIZE) {
		return -1;
	}
	
	x_coord = X_COORD(loc);
	y_coord = Y_COORD(loc);

	// handle X
	if (x_coord + x_shift < 0) {
		if (USE_WRAP_X) {
			loc += x_shift + USE_WIDTH;
		}
		else {
			// off the left side
			return -1;
		}
	}
	else if (x_coord + x_shift >= USE_WIDTH) {
		if (USE_WRAP_X) {
			loc += x_shift - USE_WIDTH;
		}
		else {
			// off the right side
			return -1;
		}
	}
	else {
		loc += x_shift;
	}
	
	// handle Y
	if (y_coord + y_shift < 0) {
		if (USE_WRAP_Y) {
			loc += (y_shift * USE_WIDTH) + USE_SIZE;
		}
		else {
			// off the bottom
			return -1;
		}
	}
	else if (y_coord + y_shift >= USE_HEIGHT) {
		if (USE_WRAP_Y) {
			loc += (y_shift * USE_WIDTH) - USE_SIZE;
		}
		else {
			// off the top
			return -1;
		}
	}
	else {
		loc += (y_shift * USE_WIDTH);
	}

	// again, we can ONLY return map locations
	if (loc >= 0 && loc < USE_SIZE) {
		return loc;
	}
	else {
		return -1;
	}
}


// converts a sector vnum back to terrain id (terrains[id].sector_vnum)
int sect_to_terrain(int sect_vnum) {
	int iter;
	
	for (iter = 0; iter < NUM_MAP_SECTS; ++iter) {
		if (terrains[iter].sector_vnum == sect_vnum) {
			return iter;
		}
	}
	
	printf("Unable to identify sector vnum %d\n", sect_vnum);
	exit(1);
}


// pans the whole map to the right/left a given amount, for eliminating east/west edges
void shift_map_x(int amt) {
	struct grid_type *temp;
	int iter, loc;
	
	CREATE(temp, struct grid_type, USE_SIZE);
	
	// make a copy shifted horizontally
	for (iter = 0; iter < USE_SIZE; ++iter) {
		loc = shift(iter, amt, 0);
		
		if (loc < 0 || loc >= USE_SIZE) {
			printf("ERROR: shift_map_x somehow found invalid location: %d (%d, %d, 0)\n", loc, iter, amt);
			exit(1);
		}
		
		temp[loc] = grid[iter];
	}
	
	// then copy back over
	for (iter = 0; iter < USE_SIZE; ++iter) {
		grid[iter] = temp[iter];
	}
	
	free(temp);
}


 //////////////////////////////////////////////////////////////////////////////
//// MAP GENERATOR FUNCTIONS /////////////////////////////////////////////////

// changes plains to <type> in a pattern roughly between y-start and y-end
// y_start and y_end are PERCENTAGES of the map height
void add_latitude_terrain(int type, double y_start, double y_end) {
	int x, y, to;
	
	for (x = 0; x < USE_WIDTH; ++x) {
		for (y = 0; y < USE_HEIGHT; ++y) {
			to = MAP(x, y);
			
			if (grid[to].type == PLAINS) {
				if (IS_IN_Y_PRC_RANGE(y + number(-1, 1), y_start, y_end)) {
					change_grid(to, type);
				}
			}
		}
	}
}


/* Add a mountain range to an island */
void add_mountains(struct island_data *isle) {
	int x, y, room;
	int hor, ver, to;
	int found = 0;
	
	clear_pass();

	do {
		x = number(-1, 1);
		y = number(-1, 1);
	} while (x == 0 && y == 0);

	room = find_border(isle, x, y);

	/* invert directions */
	x *= -1;
	y *= -1;

	while (room != -1) {
		if (!terrains[grid[room].type].is_land)
			return;
		if (grid[room].type == MOUNTAIN && grid[room].pass == 0) {
			return;
		}
		
		if (grid[room].type == PLAINS) {
			change_grid(room, MOUNTAIN);
		}
		grid[room].pass = 1;

		for (hor = -2; hor <= 2; ++hor) {
			for (ver = -2; ver  <= 2; ++ver) {
				// skip the corners to make a "rounded brush"
				if (((hor == ver) || (hor == -1 * ver)) && (hor == -2 || hor == 2)) {
					continue;
				}
				
				to = shift(room, hor, ver);
				if (to != -1 && number(0, 10) && grid[to].type == PLAINS) {
					// if we find a mountain that already exists, we stop AFTER this round
					if (grid[to].type == MOUNTAIN && grid[to].pass == 0) {
						found = 1;
					}
					change_grid(to, MOUNTAIN);
					grid[to].pass = 1;
				}
			}
		}
		
		// break out
		if (found) {
			return;
		}

		/* Alter course */
		if (!number(0, 4)) {
			if (x == 0)
				x += number(-1, 1);
			else if (y == 0)
				y += number(-1, 1);
			else if (x > 0 && y > 0) {
				if ((y -= number(0, 1)))
					x -= number(0, 1);
			}
			else if (x < 0 && y < 0) {
				if ((y += number(0, 1)))
					x += number(0, 1);
			}
			else if (x < 0 && y > 0) {
				if ((y -= number(0, 1)))
					x += number(0, 1);
			}
			else if (y < 0 && x > 0) {
				if ((y += number(0, 1)))
					x -= number(0, 1);
			}
		}

		room = shift(room, x, y);
	}
}


/* Add a pass through a mountain (at random) */
void add_pass(struct island_data *isle) {
	int x, y, room, loc;
	int sect = PLAINS;

	do {
		x = number(-1, 1);
		y = number(-1, 1);
	} while (x == 0 && y == 0);

	room = find_border(isle, x, y);

	/* invert directions */
	x *= -1;
	y *= -1;

	while (room != -1) {
		if (!terrains[grid[room].type].is_land) {
			return;
		}
		if (grid[room].type == PLAINS) {
			sect = PLAINS;
		}
		if (grid[room].type == DESERT) {
			sect = DESERT;
		}
		if (grid[room].type == MOUNTAIN) {
			change_grid(room, sect);
		}

		if ((loc = shift(room, 0, 1)) != -1 && grid[loc].type == MOUNTAIN) {
			change_grid(loc, sect);
		}
		if ((loc = shift(room, 1, 0)) != -1 && grid[loc].type == MOUNTAIN) {
			change_grid(loc, sect);
		}

		room = shift(room, x, y);
	}
}


/* Add a river to the island */
void add_river(struct island_data *isle) {
	int x, y, room;
	int hor, ver, to;
	int found = 0;

	clear_pass();

	do {
		x = number(-1, 1);
		y = number(-1, 1);
	} while (x == 0 && y == 0);

	room = find_border(isle, x, y);

	/* invert directions */
	x *= -1;
	y *= -1;

	while (room != -1) {
		if (!terrains[grid[room].type].is_land)
			return;
		
		change_grid(room, RIVER);
		grid[room].pass = 1;
		
		for (hor = number(-1, 0); hor <= 1; ++hor) {
			for (ver = number(-1, 0); ver <= 1; ++ver) {
				if (hor == 0 && ver == 0) {
					// safe to skip self
					continue;
				}
				
				to = shift(room, hor, ver);
				if (to != -1) {
					// if we hit another river, stop AFTER this sect
					if (grid[to].type == RIVER && grid[to].pass == 0) {
						found = 1;
					}
				
					if (terrains[grid[to].type].is_land) {
						change_grid(to, RIVER);
						grid[to].pass = 1;
					}
				}
			}
		}

		if (found) {
			return;
		}

		/* Alter course */
		if (!number(0, 2)) {
			if (x == 0)
				x += number(-1, 1);
			else if (y == 0)
				y += number(-1, 1);
			else if (x > 0 && y > 0) {
				if ((y -= number(0, 1)))
					x -= number(0, 1);
			}
			else if (x < 0 && y < 0) {
				if ((y += number(0, 1)))
					x += number(0, 1);
			}
			else if (x < 0 && y > 0) {
				if ((y -= number(0, 1)))
					x += number(0, 1);
			}
			else if (y < 0 && x > 0) {
				if ((y += number(0, 1)))
					x -= number(0, 1);
			}
			
			// verify bounds to prevent leaps
			x = MAX(-1, MIN(x, 1));
			y = MAX(-1, MIN(y, 1));
		}

		room = shift(room, x, y);
	}
}


// adds a start location (tower)
void add_start_points(void) {
	struct island_data *isle;
	int count = 0;

	for (isle = island_list; isle && count < NUM_START_POINTS; isle = isle->next) {
		if (!number(0, 2) && (grid[isle->loc].type == PLAINS || grid[isle->loc].type == FOREST || grid[isle->loc].type == DESERT)) {
			change_grid(isle->loc, TOWER);
			count++;
		}
	}

	// repeat until success
	if (count == 0) {
		add_start_points();
	}
}


// overwrites the top and bottom edges of the map with tundra
void add_tundra(void) {
	int iter;
	
	// find edge tiles
	for (iter = 0; iter < USE_SIZE; ++iter) {
		if (Y_COORD(iter) < TUNDRA_HEIGHT + (!number(0, 2) ? 1 : 0)) {
			change_grid(iter, TUNDRA);
		}
		else if (Y_COORD(iter) >= (USE_HEIGHT - TUNDRA_HEIGHT - (!number(0, 2) ? 1 : 0))) {
			change_grid(iter, TUNDRA);
		}
	}
}


// attempts to put the emptiest spot on the east/west edge
void center_map(void) {
	int lrg_size, lrg_start, this_size, this_start, min, first_gap_size;
	int vsize[USE_WIDTH];
	int x, y, shift_val, loc;
	int first_gap_done;
	
	// init
	for (x = 0; x < USE_WIDTH; ++x) {
		vsize[x] = 0;
	}
	
	// count how many land tiles there are for each X position
	for (x = 0; x < USE_WIDTH; ++x) {
		for (y = 0; y < USE_HEIGHT; ++y) {
			loc = MAP(x, y);
			
			if (loc < 0 || loc >= USE_SIZE) {
				// somehow
				printf("ERROR: center_map got bad loc %d (%d, %d)\n", loc, x, y);
			}
			
			if (terrains[grid[loc].type].is_land) {
				vsize[x] += 1;
			}
		}
	}
	
	// detect minimum vsize
	min = USE_HEIGHT;
	for (x = 0; x < USE_WIDTH; ++x) {
		if (vsize[x] < min) {
			min = vsize[x];
		}
	}
	
	// find the largest gap of size=min
	first_gap_done = FALSE;
	first_gap_size = 0;
	lrg_size = 0;
	lrg_start = -1;
	this_size = 0;
	this_start = -1;
	
	// this goes to + 1 because we need to trigger the end condition
	for (x = 0; x < USE_WIDTH + 1; ++x) {
		// store first gap for later
		if (x < USE_WIDTH && !first_gap_done) {
			if (vsize[x] == min) {
				first_gap_size += 1;
			}
			else {
				first_gap_done = TRUE;
			}
		}
		
		// found an end
		if (x == USE_WIDTH || (vsize[x] != min && this_start != -1)) {
			// new largest?
			if (this_size > lrg_size) {
				lrg_size = this_size;
				lrg_start = this_start;
			}
			
			if (x == USE_WIDTH) {
				break;	// do not reset!
			}
			
			// reset
			this_size = 0;
			this_start = -1;
		}
		else if (vsize[x] == min) {
			if (this_start == -1) { // starting?
				this_start = x;
				this_size = 1;
			}
			else {	// continuing
				this_size += 1;
			}
		}
	}
	
	// special case: last gap connects to first gap -- nothing to center if this is largest
	if (this_size + first_gap_size > lrg_size) {
		printf("- not centering: largest gap is on the edge\n");
		return;
	}
	
	// did we find a place at all?
	if (lrg_start == -1) {
		printf("- unable to center map: no gap detected\n");
		return;
	}
	
	// we now want to shift it left far enough to center the largest gap
	shift_val = lrg_start + (lrg_size / 2);
	while (shift_val >= USE_WIDTH) {
		shift_val -= USE_WIDTH;
	}
	
	printf("- shifting left by %d\n", shift_val);
	shift_map_x(-shift_val);
}


// turns all plains/desert into new things
void complete_map(void) {
	int iter;
	
	for (iter = 0; iter < USE_SIZE; ++iter) {
		switch (grid[iter].type) {
			case PLAINS: {
				if (number(0, 3) == 0) {
					change_grid(iter, TEMPERATE_CROP);
				}
				else {
					change_grid(iter, FOREST);
				}
				
				// nothing stays plains
				break;
			}
			case DESERT: {
				if (number(0, 74) == 0) {
					change_grid(iter, OASIS);
				}
				else if (number(0, 24) == 0) {
					change_grid(iter, DESERT_CROP);
				}
				else if (number(0, 3) == 0) {
					change_grid(iter, GROVE);
				}
				
				// most spaces remain desert
				break;
			}
			case JUNGLE: {
				if (number(1, 100) <= 5) {
					change_grid(iter, JUNGLE_CROP);
				}
				
				// most spaces remain jungle
				break;
			}
		}
	}
}


/* Build all of the islands in memory */
void create_islands(void) {
	struct island_data *isle;
	int ii, iter, loc, dir, last_loc, attempts;

	while ((USE_SIZE - total_ocean) < TARGET_LAND_SIZE) {
		for (ii = 0; island_types[ii].min_radius != -1 && (USE_SIZE - total_ocean) < TARGET_LAND_SIZE; ++ii) {
			last_loc = -1;
			for (iter = 0; iter < island_types[ii].cluster_size; ++iter) {
				if (last_loc == -1) {
					loc = number(0, USE_SIZE - 1);
				}
				else {
					dir = number(0, NUM_DIRS-1);
					attempts = 0;
					do {
						loc = shift(last_loc, shift_dir[dir][0] * number(island_types[ii].cluster_dist/2, island_types[ii].cluster_dist), shift_dir[dir][1] * number(island_types[ii].cluster_dist/2, island_types[ii].cluster_dist));
					} while (loc == -1 && ++attempts < 50);
					if (loc == -1) {
						break;
					}
				}
				
				// save for later
				last_loc = loc;
				
				CREATE(isle, struct island_data, 1);
				isle->loc = loc;

				isle->next = island_list;
				island_list = isle;

				land_mass(isle, island_types[ii].min_radius, island_types[ii].max_radius);
			}
		}
	}
}


/* Add land to an island */
void land_mass(struct island_data *isle, int min_radius, int max_radius) {
	int i, j, last_s, last_n, a, b, loc;
	int sect = PLAINS;

	/* Island's heart */
	change_grid(isle->loc, sect);

	isle->width[EAST] = number(min_radius, max_radius);
	isle->width[WEST] = number(min_radius, max_radius);

	a = last_n = isle->width[NORTH] = number(min_radius, MIN(isle->width[EAST], isle->width[WEST]));
	b = last_s = isle->width[SOUTH] = number(min_radius, MIN(isle->width[EAST], isle->width[WEST]));

	for (j = 0; j <= isle->width[EAST]; j++) {
		for (i = 0; i <= last_n; i++) {
			if ((loc = shift(isle->loc, j, i)) != -1) {
				change_grid(loc, sect);
			}
		}
		for (i = 0; i <= last_s; i++) {
			if ((loc = shift(isle->loc, j, -i)) != -1) {
				change_grid(loc, sect);
			}
		}

		last_n += number(last_n <= 0 ? 0 : -2, ((isle->width[EAST] - j) < last_n) ? -2 : 2);
		last_s += number(last_s <= 0 ? 0 : -2, ((isle->width[EAST] - j) < last_s) ? -2 : 2);
		}

	last_n = a;
	last_s = b;

	for (j = 0; j <= isle->width[WEST]; j++) {
		for (i = 0; i <= last_n; i++) {
			if ((loc = shift(isle->loc, -j, i)) != -1) {
				change_grid(loc, sect);
			}
		}
		for (i = 0; i <= last_s; i++) {
			if ((loc = shift(isle->loc, -j, -i)) != -1) {
				change_grid(loc, sect);
			}
		}

		last_n += number(last_n <= 0 ? 0 : -2, ((isle->width[WEST] - j) < last_n) ? -2 : 2);
		last_s += number(last_s <= 0 ? 0 : -2, ((isle->width[WEST] - j) < last_s) ? -2 : 2);
	}
}


// sets up island numbers and converts trapped ocean to lake
void number_islands_and_fix_lakes(void) {
	int first_ocean_done = FALSE, changed = FALSE;
	struct num_data_t *ndt;
	int old, x, y, pos;
	int iter, use_id, use_land;
	int top_id = 0;
	
	// initialize
	for (iter = 0; iter < USE_SIZE; ++iter) {
		grid[iter].island_id = 0;	// anything that stays zero will be made a lake
	}
	
	// find and create basic stack
	for (iter = 0; iter < USE_SIZE; ++iter) {
		if (grid[iter].island_id == 0) {
			if (terrains[grid[iter].type].is_land) {
				use_id = ++top_id;
				use_land = TRUE;
			}
			else if (!first_ocean_done) {	// non-land
				use_id = -1;
				use_land = FALSE;
				first_ocean_done = TRUE;
			}
			else {
				continue;	// skip
			}
			
			old = grid[iter].island_id;
			
			push_ndt(iter);
			while ((ndt = pop_ndt())) {
				grid[ndt->loc].island_id = use_id;
	
				for (x = -1; x <= 1; ++x) {
					for (y = -1; y <= 1; ++y) {
						if (x != 0 || y != 0) {
							pos = shift(ndt->loc, x, y);
							if (pos != -1 && grid[pos].island_id == old && terrains[grid[pos].type].is_land == use_land) {
								push_ndt(pos);
							}
						}
					}
				}
				free(ndt);
			}
		}
	}
	
	// now find unreachable land and turn to lake
	for (iter = 0; iter < USE_SIZE; ++iter) {
		if (grid[iter].island_id == 0) {
			change_grid(iter, LAKE);
			changed = TRUE;
		}
	}
	
	// should only take 1 second time
	if (changed) {
		number_islands_and_fix_lakes();
	}
}


/**
* Convert terrain from one thing to another near other terrain.
* 
* @param int from Terrain to convert from.
* @param int to Terrain to convert to.
* @param int near Terrain it must be near.
* @param int dist Distance it must be within.
*/
void replace_near(int from, int to, int near, int dist) {
	int x, y, hor, ver, at, loc;
	int found;
	
	for (x = 0; x < USE_WIDTH; ++x) {
		for (y = 0; y < USE_HEIGHT; ++y) {
			at = MAP(x, y);
			
			if (grid[at].type == from) {
				found = 0;
				for (hor = -dist; hor <= dist && !found; ++hor) {
					for (ver = -dist; ver <= dist && !found; ++ver) {
						loc = shift(at, hor, ver);
						
						if (loc != -1 && grid[loc].type == near && compute_distance(X_COORD(at), Y_COORD(at), X_COORD(loc), Y_COORD(loc)) <= dist) {
							change_grid(at, to);
							found = 1;
						}
					}
				}
			}
		}
	}
}


/* Add a splotch of a given sect in a nice splotchy shape	*/
void splotch(int room, int type, struct island_data *isle) {
	int i, j, last_s, last_n, a, b, width_e, width_w, width_n, width_s;
	int loc, sect = PLAINS;

	if (sect == DESERT) {
		width_e = number(1, 3);
		width_w = number(1, 3);
	}
	else {
		width_e = number(2, 8);
		width_w = number(2, 8);
	}

	a = last_n = width_n = number((sect == DESERT ? 1 : 2), width_e);
	b = last_s = width_s = number((sect == DESERT ? 1 : 2), width_e);

	for (j = 0; j <= width_e; j++) {
		for (i = 0; i <= last_n; i++) {
			if ((loc = shift(room, j, i)) != -1 && grid[loc].type == sect) {
				change_grid(loc, type);
			}
		}
		for (i = 0; i <= last_s; i++) {
			if ((loc = shift(room, j, -i)) != -1 && grid[loc].type == sect) {
				change_grid(loc, type);
			}
		}

		last_n += number(last_n <= 0 ? 0 : -2, ((width_e - j) < last_n) ? -2 : 2);
		last_s += number(last_s <= 0 ? 0 : -2, ((width_e - j) < last_s) ? -2 : 2);
		}

	last_n = a;
	last_s = b;

	for (j = 0; j <= width_w; j++) {
		for (i = 0; i <= last_n; i++) {
			if ((loc = shift(room, -j, i)) != -1 && grid[loc].type == sect) {
				change_grid(loc, type);
			}
		}
		for (i = 0; i <= last_s; i++) {
			if ((loc = shift(room, -j, -i)) != -1 && grid[loc].type == sect) {
				change_grid(loc, type);
			}
		}

		last_n += number(last_n <= 0 ? 0 : -2, ((width_w - j) < last_n) ? -2 : 2);
		last_s += number(last_s <= 0 ? 0 : -2, ((width_w - j) < last_s) ? -2 : 2);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// BONUS FEATURES //////////////////////////////////////////////////////////

void load_and_shift_map(int dist) {
	char fname[256], line[1024];
	FILE *index, *wld;
	int block, vnum, island, type, junk;
	
	// nowork
	if (dist == 0) {
		printf("Shift by distance 0: no work to do.\n");
		return;
	}
	
	init_grid();
	printf("Loaded existing map...\n");
	
	// load in existing map
	if (!(index = fopen("index", "r"))) {
		printf("ERROR: Unable to load index file.\n");
		exit(1);
	}
	
	while (fscanf(index, "%d.wld\n", &block) == 1) {
		sprintf(fname, "%d.wld", block);
		printf("Loading file %s ...\n", fname);
		if (!(wld = fopen(fname, "r"))) {
			printf("ERROR: Unable to open world file %s.\n", fname);
			exit(1);
		}
		
		while (get_line(wld, line)) {
			if (*line == '$') {
				break;
			}
			else if (*line == '#') {
				// found entry
				vnum = atoi(line + 1);
				
				if (!get_line(wld, line) || sscanf(line, "%d %d %d", &island, &type, &junk) != 3) {
					printf("Error reading room #%d\n", vnum);
					exit(1);
				}
				
				change_grid(vnum, sect_to_terrain(type));
				grid[vnum].island_id = island;
				
				// trailing 'S' line
				get_line(wld, line);
			}
			else {
				printf("Unknown line in %s: %s\n", fname, line);
				exit(1);
			}
		}
	}
	
	shift_map_x(dist);
	
	print_map_graphic();
	print_map_to_files();

	printf("Map shifted %d on X-axis.\n", dist);
	output_stats();	
}
