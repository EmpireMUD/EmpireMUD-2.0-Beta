/* ************************************************************************
*   File: read_map.c                                      EmpireMUD 2.0b5 *
*  Usage: ./read_map binary_map [vnum]                                    *
*  Usage: ./read_map binary_map <x> <y>                                   *
*                                                                         *
*  View stats on the binary map file, or read one of its room entries.    *
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

/**
* Table of contents:
*  Data
*  Display Functions
*  Map Loader
*  main()
*/


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

map_file_data *loaded_map = NULL;	// load via load_map()
int map_width = 0;	// detected in load_map()
int map_height = 0;	// detected in load_map()

// this allows the inclusion of utils.h
void basic_mud_log(const char *format, ...) { }


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAY FUNCTIONS ///////////////////////////////////////////////////////

/**
* Shows all the data for a single map tile.
*
* @param int vnum The map tile vnum.
*/
void show_tile(int vnum) {
	fprintf(stdout, "island_id: %d\n", loaded_map[vnum].island_id);
	
	fprintf(stdout, "sector_type: %d\n", loaded_map[vnum].sector_type);
	fprintf(stdout, "base_sector: %d\n", loaded_map[vnum].base_sector);
	fprintf(stdout, "natural_sector: %d\n", loaded_map[vnum].natural_sector);
	fprintf(stdout, "crop_type: %d\n", loaded_map[vnum].crop_type);
	
	fprintf(stdout, "affects: %lld\n", loaded_map[vnum].affects);
	fprintf(stdout, "base_affects: %lld\n", loaded_map[vnum].base_affects);
	
	fprintf(stdout, "height: %d\n", loaded_map[vnum].height);
	fprintf(stdout, "sector_time: %ld\n", loaded_map[vnum].sector_time);
	
	fprintf(stdout, "misc: %d\n", loaded_map[vnum].misc);
}


/**
* Shows general map stats (not currently used).
*/
void show_map_stats(void) {
	fprintf(stdout, "No stats to show with this utility. Request a vnum or x y instead.\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// MAP LOADER //////////////////////////////////////////////////////////////

/**
* Loads binary map file by name.
*
* @param const char *filename The path to the binary map file.
*/
void load_map(const char *filename) {
	struct map_file_header header;
	FILE *fl;
	size_t read;
	
	// in case it's called twice
	if (loaded_map) {
		free(loaded_map);
	}
	
	if (!(fl = fopen(filename, "r+b"))) {
		fprintf(stdout, "Unable to load binary map file '%s'\n", filename);
		exit(1);
	}
	
	read = fread(&header, sizeof(struct map_file_header), 1, fl);
	if (read < 1) {
		fprintf(stdout, "Failure reading map header.\n");
		exit(1);
	}
	fprintf(stdout, "Reading map file '%s': version %d, size %dx%d\n", filename, header.version, header.width, header.height);
	map_width = header.width;
	map_height = header.height;
	
	if (header.version != CURRENT_BINARY_MAP_VERSION) {
		fprintf(stdout, "Map file version is too old to load with this utility.\n");
		exit(1);
	}
	
	CREATE(loaded_map, map_file_data, header.width * header.height);
	read = fread(loaded_map, sizeof(map_file_data), header.width * header.height, fl);
	
	if (read < header.width * header.height) {
		fprintf(stdout, "Failure reading map: %zu bytes read; expecting %d.\n", read, header.width * header.height);
		exit(1);
	}
	
	fclose(fl);
}


 //////////////////////////////////////////////////////////////////////////////
//// MAIN() //////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
	int vnum = -1, x = -1, y = -1;
	
	if (argc < 2 || argc > 4) {
		fprintf(stdout, "Usage: ./%s <map file> [vnum]\n", argv[0]);
		fprintf(stdout, "Usage: ./%s <map file> <x> <y>\n", argv[0]);
		exit(1);
	}
	
	load_map(argv[1]);
	
	if (argc == 3) {
		vnum = atoi(argv[2]);
		fprintf(stdout, "Data for map tile vnum %d:\n", vnum);
		show_tile(vnum);
	}
	else if (argc == 4) {
		x = atoi(argv[2]);
		y = atoi(argv[3]);
		fprintf(stdout, "Data for map tile (%d, %d):\n", x, y);
		show_tile(y * map_width + x);
	}
	else {
		show_map_stats();
	}
	
	return 0;
}
