/* ************************************************************************
*   File: data.c                                          EmpireMUD 2.0b5 *
*  Usage: Related to saving/loading/using the global game data system     *
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
#include "db.h"


/**
* Contents:
*   Helpers
*   Getters
*   Setters
*   File I/O
*/

// external vars

// locals
struct stored_data *data_table = NULL;	// key:value for global data
bool data_table_needs_save = FALSE;	// triggers a save


// DATA_x: stored data (in order of const)
struct stored_data_type stored_data_info[] = {
	// note: names MUST be all-one-word
	{ "daily_cycle", DATYPE_LONG },	// DATA_DAILY_CYCLE
	{ "last_new_year", DATYPE_LONG },	// DATA_LAST_NEW_YEAR
	{ "world_start", DATYPE_LONG },	// DATA_WORLD_START
	{ "max_players_today", DATYPE_INT },	// DATA_MAX_PLAYERS_TODAY
	
	{ "\n", NOTHING }	// last
};


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////


 //////////////////////////////////////////////////////////////////////////////
//// GETTERS /////////////////////////////////////////////////////////////////

/**
* @param int key Any DATA_ const that has a "double" type.
* @return double The value of that key, or 0.0 if no data found.
*/
double data_get_double(int key) {
	struct stored_data *data;
	HASH_FIND_INT(data_table, &key, data);
	
	if (!data) {
		return 0.0;	// no known data
	}
	if (data->keytype != DATYPE_DOUBLE) {
		log("SYSERR: Data key %s (%d) incorrectly requested as double", stored_data_info[data->key].name, data->key);
		return 0.0;
	}
	
	return data->value.double_val;
}


/**
* @param int key Any DATA_ const that has a "int" type.
* @return int The value of that key, or 0 if no data found.
*/
int data_get_int(int key) {
	struct stored_data *data;
	HASH_FIND_INT(data_table, &key, data);
	
	if (!data) {
		return 0;	// no known data
	}
	if (data->keytype != DATYPE_INT) {
		log("SYSERR: Data key %s (%d) incorrectly requested as int", stored_data_info[data->key].name, data->key);
		return 0;
	}
	
	return data->value.int_val;
}


/**
* @param int key Any DATA_ const that has a "long" type.
* @return long The value of that key, or 0 if no data found.
*/
long data_get_long(int key) {
	struct stored_data *data;
	HASH_FIND_INT(data_table, &key, data);
	
	if (!data) {
		return 0;
	}
	if (data->keytype != DATYPE_LONG) {
		log("SYSERR: Data key %s (%d) incorrectly requested as long", stored_data_info[data->key].name, data->key);
		return 0;
	}
	
	return data->value.long_val;
}


 //////////////////////////////////////////////////////////////////////////////
//// SETTERS /////////////////////////////////////////////////////////////////

/**
* @param int key Any DATA_ const that has a "double" type.
* @param double value The value to set it to.
* @return double The value it was set to.
*/
double data_set_double(int key, double value) {
	struct stored_data *data;
	
	if (key < 0 || key >= NUM_DATAS) {
		log("SYSERR: Attempting to set out-of-range 'double' data key %d", key);
		return 0.0;
	}
	if (stored_data_info[key].type != DATYPE_DOUBLE) {
		log("SYSERR: Attempting to set 'double' data on non-double key %s", stored_data_info[key].name);
		return 0;
	}
	
	HASH_FIND_INT(data_table, &key, data);
	if (!data) {
		// add it
		CREATE(data, struct stored_data, 1);
		data->key = key;
		data->keytype = stored_data_info[data->key].type;
		
		HASH_ADD_INT(data_table, key, data);
	}
	
	data->value.double_val = value;
	data_table_needs_save = TRUE;
	
	return data->value.double_val;
}


/**
* @param int key Any DATA_ const that has an "int" type.
* @param int value The value to set it to.
* @return int The value it was set to.
*/
int data_set_int(int key, int value) {
	struct stored_data *data;
	
	if (key < 0 || key >= NUM_DATAS) {
		log("SYSERR: Attempting to set out-of-range 'int' data key %d", key);
		return 0;
	}
	if (stored_data_info[key].type != DATYPE_INT) {
		log("SYSERR: Attempting to set 'int' data on non-int key %s", stored_data_info[key].name);
		return 0;
	}
	
	HASH_FIND_INT(data_table, &key, data);
	if (!data) {
		// add it
		CREATE(data, struct stored_data, 1);
		data->key = key;
		data->keytype = stored_data_info[data->key].type;
		
		HASH_ADD_INT(data_table, key, data);
	}
	
	data->value.int_val = value;
	data_table_needs_save = TRUE;
	
	return data->value.int_val;
}


/**
* @param int key Any DATA_ const that has a "long" type.
* @param long value The value to set it to.
* @return long The value it was set to.
*/
long data_set_long(int key, long value) {
	struct stored_data *data;
	
	if (key < 0 || key >= NUM_DATAS) {
		log("SYSERR: Attempting to set out-of-range 'long' data key %d", key);
		return 0;
	}
	if (stored_data_info[key].type != DATYPE_LONG) {
		log("SYSERR: Attempting to set 'long' data on non-long key %s", stored_data_info[key].name);
		return 0;
	}
	
	HASH_FIND_INT(data_table, &key, data);
	if (!data) {
		// add it
		CREATE(data, struct stored_data, 1);
		data->key = key;
		data->keytype = stored_data_info[data->key].type;
		
		HASH_ADD_INT(data_table, key, data);
	}
	
	data->value.long_val = value;
	data_table_needs_save = TRUE;
	
	return data->value.long_val;
}


 //////////////////////////////////////////////////////////////////////////////
//// FILE I/O ////////////////////////////////////////////////////////////////

/**
* Saves the data_table info to file.
*
* @param bool force If TRUE, saves even if it doesn't think it needs it.
*/
void save_data_table(bool force) {
	struct stored_data *data, *next_data;
	FILE *fl;
	
	if (!force && !data_table_needs_save) {
		return;
	}
	
	if (!(fl = fopen(DATA_FILE TEMP_SUFFIX, "w"))) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Unable to write %s", DATA_FILE TEMP_SUFFIX);
		return;
	}
	
	fprintf(fl, "* EmpireMUD Game Data\n");
	
	HASH_ITER(hh, data_table, data, next_data) {
		// DATYPE_x
		switch (data->keytype) {
			case DATYPE_DOUBLE: {
				fprintf(fl, "%s %f\n", stored_data_info[data->key].name, data->value.double_val);
				break;
			}
			case DATYPE_INT: {
				fprintf(fl, "%s %d\n", stored_data_info[data->key].name, data->value.int_val);
				break;
			}
			case DATYPE_LONG: {
				fprintf(fl, "%s %ld\n", stored_data_info[data->key].name, data->value.long_val);
				break;
			}
			default: {
				syslog(SYS_ERROR, LVL_START_IMM, TRUE, "Unable to save data system: unsavable data type %d", data->keytype);
				fclose(fl);
				return;
			}
		}
	}

	fprintf(fl, "$~\n");
	fclose(fl);
	rename(DATA_FILE TEMP_SUFFIX, DATA_FILE);	
	
	data_table_needs_save = FALSE;
}


/**
* Loads the data_table from file at startup, or on request.
*/
void load_data_table(void) {
	char line[MAX_INPUT_LENGTH], key[MAX_INPUT_LENGTH], *arg;
	struct stored_data *data;
	int iter, id;
	FILE *fl;
	
	// aaand read from file	
	if (!(fl = fopen(DATA_FILE, "r"))) {
		// no config exists
		return;
	}
	
	// iterate over whole file
	while (TRUE) {
		// get line, or unnatural end
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in data system file");
			break;
		}
		
		// natural end!
		if (*line == '$') {
			break;
		}
		
		// comment / skip
		if (*line == '*') {
			continue;
		}
		
		arg = any_one_arg(line, key);
		skip_spaces(&arg);
		
		// junk line?
		if (!*key) {
			continue;
		}
		
		// look it up
		id = NOTHING;
		for (iter = 0; iter < NUM_DATAS; ++iter) {
			if (!str_cmp(key, stored_data_info[iter].name)) {
				id = iter;
				break;
			}
		}
		
		// quick sanity
		if (id == NOTHING) {
			log("SYSERR: Unknown data key: %s", key);
			continue;
		}
		
		// find or add the data for it
		HASH_FIND_INT(data_table, &id, data);
		if (!data) {
			CREATE(data, struct stored_data, 1);
			data->key = id;
			data->keytype = stored_data_info[iter].type;
			HASH_ADD_INT(data_table, key, data);
		}
		
		// DATYPE_x: process argument by type
		switch (data->keytype) {
			case DATYPE_DOUBLE: {
				data->value.double_val = atof(arg);
				break;
			}
			case DATYPE_INT: {
				data->value.int_val = atoi(arg);
				break;
			}
			case DATYPE_LONG: {
				data->value.long_val = atol(arg);
				break;
			}
			default: {
				log("Unable to load data system: %s: unloadable data type %d", stored_data_info[data->key].name, data->keytype);
				break;
			}
		}
	}
	
	fclose(fl);
}
