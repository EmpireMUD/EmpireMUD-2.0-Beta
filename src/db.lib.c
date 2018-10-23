/* ************************************************************************
*   File: db.lib.c                                        EmpireMUD 2.0b5 *
*  Usage: Primary read/write functions for game world data                *
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
#include "skills.h"
#include "olc.h"
#include "dg_scripts.h"
#include "dg_event.h"
#include "vnums.h"

/**
* Contents:
*   Adventure Lib
*   Automessages Lib
*   Building Lib
*   Craft Lib
*   Crop Lib
*   Empire Lib
*   Empire NPC Lib
*   Empire Offense Lib
*   Exit Lib
*   Extra Description Lib
*   Globals Lib
*   Icon Lib
*   Interaction Lib
*   Mobile Lib
*   Object Lib
*   Resource Lib
*   Room Lib
*   Room Template Lib
*   Sector Lib
*   Stored Event Lib
*   Theft Lib
*   Trigger Lib
*   Core Lib Functions
*   Index Saving
*   The Reals
*   Helpers
*   Sorters
*   Miscellaneous Lib
*/

// external variables
extern struct automessage *automessages;
extern struct db_boot_info_type db_boot_info[NUM_DB_BOOT_TYPES];
extern struct player_special_data dummy_mob;
extern struct empire_territory_data *global_next_territory_entry;
extern int max_automessage_id;
extern struct offense_info_type offense_info[NUM_OFFENSES];
extern bool world_is_sorted;

// external funcs
extern struct complex_room_data *init_complex_data();
void Crash_save_one_obj_to_file(FILE *fl, obj_data *obj, int location);
void free_archetype_gear(struct archetype_gear *list);
extern room_data *load_map_room(room_vnum vnum);
extern obj_data *Obj_load_from_file(FILE *fl, obj_vnum vnum, int *location, char_data *notify);
void sort_exits(struct room_direction_data **list);
void sort_world_table();

// locals
int check_object(obj_data *obj);
int count_hash_records(FILE *fl);
void delete_territory_npc(struct empire_territory_data *ter, struct empire_npc_data *npc);
empire_vnum find_free_empire_vnum(void);
void free_theft_logs(struct theft_log *list);
void parse_custom_message(FILE *fl, struct custom_message **list, char *error);
void parse_extra_desc(FILE *fl, struct extra_descr_data **list, char *error_part);
void parse_generic_name_file(FILE *fl, char *err_str);
void parse_icon(char *line, FILE *fl, struct icon_data **list, char *error_part);
void parse_interaction(char *line, struct interaction_item **list, char *error_part);
void parse_link_rule(FILE *fl, struct adventure_link_rule **list, char *error_part);
void parse_resource(FILE *fl, struct resource_data **list, char *error_str);
struct theft_log *reduce_theft_logs_and_get_recent(empire_data *emp);
PLAYER_UPDATE_FUNC(send_all_players_to_nowhere);
int sort_empires(empire_data *a, empire_data *b);
int sort_room_templates(room_template *a, room_template *b);
void write_custom_messages_to_file(FILE *fl, char letter, struct custom_message *list);
void write_extra_descs_to_file(FILE *fl, struct extra_descr_data *list);
void write_icons_to_file(FILE *fl, char file_tag, struct icon_data *list);
void write_interactions_to_file(FILE *fl, struct interaction_item *list);
void write_linking_rules_to_file(FILE *fl, char letter, struct adventure_link_rule *list);
void write_resources_to_file(FILE *fl, char letter, struct resource_data *list);
void write_shared_room_data(FILE *fl, struct shared_room_data *dat);
void write_trig_protos_to_file(FILE *fl, char letter, struct trig_proto_list *list);


 //////////////////////////////////////////////////////////////////////////////
//// ADVENTURE LIB ///////////////////////////////////////////////////////////

/**
* Puts an adventure in the hash table.
*
* @param adv_data *adv The adventure to add to the table.
*/
void add_adventure_to_table(adv_data *adv) {
	extern int sort_adventures(adv_data *a, adv_data *b);
	
	adv_data *find;
	adv_vnum vnum;
	
	if (adv) {
		vnum = GET_ADV_VNUM(adv);
		HASH_FIND_INT(adventure_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(adventure_table, vnum, adv);
			HASH_SORT(adventure_table, sort_adventures);
		}
	}
}

/**
* Removes an adventure from the hash table.
*
* @param adv_data *adv The adventure to remove from the table.
*/
void remove_adventure_from_table(adv_data *adv) {
	HASH_DEL(adventure_table, adv);
}


/**
* frees up memory for a adventure
*
* See also: olc_delete_adventure
*
* @param adv_data *adv The adventure to free.
*/
void free_adventure(adv_data *adv) {
	adv_data *proto = adventure_proto(GET_ADV_VNUM(adv));
	struct adventure_link_rule *link;
	
	if (GET_ADV_NAME(adv) && (!proto || GET_ADV_NAME(adv) != GET_ADV_NAME(proto))) {
		free(GET_ADV_NAME(adv));
	}
	if (GET_ADV_AUTHOR(adv) && (!proto || GET_ADV_AUTHOR(adv) != GET_ADV_AUTHOR(proto))) {
		free(GET_ADV_AUTHOR(adv));
	}
	if (GET_ADV_DESCRIPTION(adv) && (!proto || GET_ADV_DESCRIPTION(adv) != GET_ADV_DESCRIPTION(proto))) {
		free(GET_ADV_DESCRIPTION(adv));
	}
	if (GET_ADV_LINKING(adv) && (!proto || GET_ADV_LINKING(adv) != GET_ADV_LINKING(proto))) {
		while ((link = GET_ADV_LINKING(adv))) {
			GET_ADV_LINKING(adv) = link->next;
			free(link);
		}
	}
	if (GET_ADV_SCRIPTS(adv) && (!proto || GET_ADV_SCRIPTS(adv) != GET_ADV_SCRIPTS(proto))) {
		free_proto_scripts(&GET_ADV_SCRIPTS(adv));
	}
	
	free(adv);
}


/**
* Clears out the values of a adventure_data object.
*
* @param adv_data *adv The adventure to clear.
*/
void init_adventure(adv_data *adv) {
	memset((char *)adv, 0, sizeof(adv_data));
	
	GET_ADV_FLAGS(adv) = ADV_IN_DEVELOPMENT;
	GET_ADV_MAX_INSTANCES(adv) = 1;
	GET_ADV_RESET_TIME(adv) = 30;
}


/**
* Read one adventure from file.
*
* @param FILE *fl The open .adv file
* @param adv_vnum vnum The adventure vnum
*/
void parse_adventure(FILE *fl, adv_vnum vnum) {
	int int_in[4];
	char line[256], str_in[256];
	adv_data *adv, *find;

	CREATE(adv, adv_data, 1);
	init_adventure(adv);
	GET_ADV_VNUM(adv) = vnum;

	HASH_FIND_INT(adventure_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate adventure vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_adventure_to_table(adv);
		
	// for error messages
	sprintf(buf2, "adventure zone vnum %d", vnum);
	
	// lines 1-3
	GET_ADV_NAME(adv) = fread_string(fl, buf2);
	GET_ADV_AUTHOR(adv) = fread_string(fl, buf2);
	GET_ADV_DESCRIPTION(adv) = fread_string(fl, buf2);
	
	// line 4: start_vnum end_vnum min_level-max_level
	if (!get_line(fl, line) || sscanf(line, "%d %d %d-%d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
		log("SYSERR: Format error in line 4 of %s", buf2);
		exit(1);
	}
	
	GET_ADV_START_VNUM(adv) = int_in[0];
	GET_ADV_END_VNUM(adv) = int_in[1];
	GET_ADV_MIN_LEVEL(adv) = int_in[2];
	GET_ADV_MAX_LEVEL(adv) = int_in[3];
	
	// line 5: max_instances reset_time flags player_limit
	if (!get_line(fl, line) || sscanf(line, "%d %d %s %d", &int_in[0], &int_in[1], str_in, &int_in[2]) != 4) {
		log("SYSERR: Format error in line 5 of %s", buf2);
		exit(1);
	}
	
	GET_ADV_MAX_INSTANCES(adv) = int_in[0];
	GET_ADV_RESET_TIME(adv) = int_in[1];
	GET_ADV_FLAGS(adv) = asciiflag_conv(str_in);
	GET_ADV_PLAYER_LIMIT(adv) = int_in[2];
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
			exit(1);
		}
		switch (*line) {
			case 'L': {	// linking rule
				parse_link_rule(fl, &GET_ADV_LINKING(adv), buf2);
				break;
			}

			// end
			case 'S': {
				return;
			}
			
			case 'T': {	// trigger
				parse_trig_proto(line, &GET_ADV_SCRIPTS(adv), buf2);
				break;
			}
			
			default: {
				log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
				exit(1);
			}
		}
	}
}


/**
* Outputs one adventure zone in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param adv_data *adv The thing to save.
*/
void write_adventure_to_file(FILE *fl, adv_data *adv) {
	char temp[MAX_STRING_LENGTH];
	
	if (!fl || !adv) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_adventure_to_file called without %s", !fl ? "file" : "adventure");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_ADV_VNUM(adv));
	
	fprintf(fl, "%s~\n", NULLSAFE(GET_ADV_NAME(adv)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_ADV_AUTHOR(adv)));

	strcpy(temp, NULLSAFE(GET_ADV_DESCRIPTION(adv)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);

	fprintf(fl, "%d %d %d-%d\n", GET_ADV_START_VNUM(adv), GET_ADV_END_VNUM(adv), GET_ADV_MIN_LEVEL(adv), GET_ADV_MAX_LEVEL(adv));
	fprintf(fl, "%d %d %s %d\n", GET_ADV_MAX_INSTANCES(adv), GET_ADV_RESET_TIME(adv), bitv_to_alpha(GET_ADV_FLAGS(adv)), GET_ADV_PLAYER_LIMIT(adv));

	// L: linking rules
	write_linking_rules_to_file(fl, 'L', GET_ADV_LINKING(adv));
	
	// T: triggers
	write_trig_protos_to_file(fl, 'T', GET_ADV_SCRIPTS(adv));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// AUTOMESSAGES LIB ////////////////////////////////////////////////////////

/**
* Formats and sends an auto-message.
*/
void show_automessage_to_char(char_data *ch, struct automessage *msg) {
	if (msg->msg && *msg->msg) {
		msg_to_char(ch, "[%s%s&0] %s\r\n", config_get_string("automessage_color"), config_get_string("mud_name"), msg->msg);
	}
}


// quick-sorter by id
int sort_automessage_by_id(struct automessage *a, struct automessage *b) {
	return a->id - b->id;
}


// quick-sorter by type/time
int sort_automessage_by_data(struct automessage *a, struct automessage *b) {
	// sort one-time to the top
	if (a->timing != b->timing) {
		if (a->timing == AUTOMSG_ONE_TIME) {
			return -1;
		}
		if (b->timing == AUTOMSG_ONE_TIME) {
			return 1;
		}
	}
	
	// otherwise sort by timestamp
	return a->timestamp - b->timestamp;
}


/**
* Checks if anybody needs an automessage, and sends it.
*/
void display_automessages(void) {
	struct automessage *msg, *next_msg;
	struct player_automessage *pam;
	descriptor_data *desc;
	time_t now = time(0);
	char_data *ch;
	int id;
	
	HASH_ITER(hh, automessages, msg, next_msg) {
		id = msg->id;
		
		if (msg->timing == AUTOMSG_ON_LOGIN) {
			continue;	// we don't show these now
		}
		if (msg->timestamp > now - (2 * SECS_PER_REAL_MIN)) {
			continue;	// don't show messages newer than 2 minutes (grace period)
		}
		
		LL_FOREACH(descriptor_list, desc) {
			if (STATE(desc) != CON_PLAYING || !(ch = desc->character) || IS_NPC(ch)) {
				continue;	// not playing
			}
			
			// have/should they see/n it
			HASH_FIND_INT(GET_AUTOMESSAGES(ch), &id, pam);
			if (pam && msg->timing == AUTOMSG_ONE_TIME) {
				continue;	// seen
			}
			else if (pam && msg->timing == AUTOMSG_REPEATING && (now - pam->timestamp) < (msg->interval * SECS_PER_REAL_MIN)) {
				continue;	// too soon
			}
			
			// validated!
			show_automessage_to_char(ch, msg);
			
			// update or add player data
			if (!pam) {
				CREATE(pam, struct player_automessage, 1);
				pam->id = id;
				HASH_ADD_INT(GET_AUTOMESSAGES(ch), id, pam);
			}
			pam->timestamp = now;
		}
	}
}


/**
* Looks for login messages to send to a player who just logged in.
*
* @param char_data *ch The player.
*/
void display_automessages_on_login(char_data *ch) {
	struct automessage *msg, *next_msg;
	bool any = FALSE;
	
	HASH_ITER(hh, automessages, msg, next_msg) {
		if (msg->timing != AUTOMSG_ON_LOGIN) {
			continue;	// only showing 1 type
		}
		
		show_automessage_to_char(ch, msg);
		any = TRUE;
	}
		
	// trailing crlf
	if (any) {
		msg_to_char(ch, "\r\n");
	}
}


/**
* Frees up an automessage.
*
* @param struct automessage *msg The message to free.
*/
void free_automessage(struct automessage *msg) {
	if (msg->msg) {
		free(msg->msg);
	}
	free(msg);
}


void load_automessages(void) {
	char line[MAX_INPUT_LENGTH], error[MAX_STRING_LENGTH];
	int id, author, timing, interval, version;
	struct automessage *msg, *find;
	bool end = FALSE;
	char *tmp;
	FILE *fl;
	long ts;
	
	version = 1;	// default (makes for easier alternate parsing to have version numbers)
	if (version < 0) {
		// nothing to do here, but it prevents a compiler warning until version is used
	}
	
	if (!(fl = fopen(AUTOMESSAGE_FILE, "r"))) {
		return;	// there are no messages
	}
	
	snprintf(error, sizeof(error), "automessage file");
	
	while (!end) {
		if (!get_line(fl, line)) {
			log("SYSERR: Unexpected end of automessage file");
			break;
		}
		
		if (*line == '$') {
			end = TRUE;
		}
		else if (!strn_cmp(line, "Automessage:", 12)) {
			version = atoi(line+13);
		}
		else if (!strn_cmp(line, "Index:", 6)) {
			max_automessage_id = atoi(line+7);
		}
		else if (!strn_cmp(line, "Message:", 8)) {
			if (sscanf(line + 8, " %d %d %ld %d %d", &id, &author, &ts, &timing, &interval) != 5) {
				log("SYSERR: Bad format in automessage file. Skipping message.");
				tmp = fread_string(fl, error);
				if (tmp) {	// skip a line
					free(tmp);
				}
				continue;
			}
			
			CREATE(msg, struct automessage, 1);
			msg->id = id;
			msg->author = author;
			msg->timestamp = ts;
			msg->timing = timing;
			msg->interval = interval;
			msg->msg = fread_string(fl, error);
			
			HASH_FIND_INT(automessages, &id, find);
			if (find) {
				log("SYSERR: Duplicate automessage id %d found in file", id);
				HASH_DEL(automessages, find);
				free_automessage(find);
			}
			
			HASH_ADD_INT(automessages, id, msg);
		}
	}
	
	fclose(fl);
}


int new_automessage_id(void) {
	struct automessage *msg, *next_msg;
	int last = 0, found = 0;
	
	if (max_automessage_id < MAX_INT) {
		return ++max_automessage_id;
	}
	else {
		// whaaaa? find the first free id
		HASH_SORT(automessages, sort_automessage_by_id);
		HASH_ITER(hh, automessages, msg, next_msg) {
			if (msg->id > last + 1) {
				found = last + 1;
			}
			last = msg->id;
		}
		
		// sort back
		HASH_SORT(automessages, sort_automessage_by_data);
		if (found) {
			return found;
		}
	}
	
	// did we get here?
	log("SYSERR: No free automessage id (somehow)");
	return 1;
}


void save_automessages(void) {
	struct automessage *msg, *next_msg;
	FILE *fl;
	
	if (!(fl = fopen(AUTOMESSAGE_FILE TEMP_SUFFIX, "w"))) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Unable to write %s", AUTOMESSAGE_FILE TEMP_SUFFIX);
		return;
	}
	
	// header
	fprintf(fl, "Automessage: 1\n");	// version tag
	fprintf(fl, "Index: %d\n", max_automessage_id);
	
	HASH_ITER(hh, automessages, msg, next_msg) {
		fprintf(fl, "Message: %d %d %ld %d %d\n%s~\n", msg->id, msg->author, msg->timestamp, msg->timing, msg->interval, NULLSAFE(msg->msg));
	}
	
	fprintf(fl, "$~\n");
	fclose(fl);
	rename(AUTOMESSAGE_FILE TEMP_SUFFIX, AUTOMESSAGE_FILE);	
}


 //////////////////////////////////////////////////////////////////////////////
//// BUILDING LIB ////////////////////////////////////////////////////////////

/**
* Puts a building in the hash table.
*
* @param bld_data *bld The building to add to the table.
*/
void add_building_to_table(bld_data *bld) {
	extern int sort_buildings(bld_data *a, bld_data *b);
	
	bld_data *find;
	bld_vnum vnum;
	
	if (bld) {
		vnum = GET_BLD_VNUM(bld);
		HASH_FIND_INT(building_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(building_table, vnum, bld);
			HASH_SORT(building_table, sort_buildings);
		}
	}
}

/**
* Removes a building from the hash table.
*
* @param bld_data *bld The building to remove from the table.
*/
void remove_building_from_table(bld_data *bld) {
	HASH_DEL(building_table, bld);
}

/**
* frees up memory for a building
*
* See also: olc_delete_building
*
* @param bld_data *building The building to free.
*/
void free_building(bld_data *bdg) {
	void free_bld_relations(struct bld_relation *list);
	
	bld_data *proto = building_proto(GET_BLD_VNUM(bdg));
	struct interaction_item *interact;
	struct spawn_info *spawn;
	
	if (GET_BLD_NAME(bdg) && (!proto || GET_BLD_NAME(bdg) != GET_BLD_NAME(proto))) {
		free(GET_BLD_NAME(bdg));
	}
	if (GET_BLD_TITLE(bdg) && (!proto || GET_BLD_TITLE(bdg) != GET_BLD_TITLE(proto))) {
		free(GET_BLD_TITLE(bdg));
	}
	if (GET_BLD_ICON(bdg) && (!proto || GET_BLD_ICON(bdg) != GET_BLD_ICON(proto))) {
		free(GET_BLD_ICON(bdg));
	}
	if (GET_BLD_COMMANDS(bdg) && (!proto || GET_BLD_COMMANDS(bdg) != GET_BLD_COMMANDS(proto))) {
		free(GET_BLD_COMMANDS(bdg));
	}
	if (GET_BLD_DESC(bdg) && (!proto || GET_BLD_DESC(bdg) != GET_BLD_DESC(proto))) {
		free(GET_BLD_DESC(bdg));
	}
	
	if (GET_BLD_EX_DESCS(bdg) && (!proto || GET_BLD_EX_DESCS(bdg) != GET_BLD_EX_DESCS(proto))) {
		free_extra_descs(&GET_BLD_EX_DESCS(bdg));
	}

	if (GET_BLD_INTERACTIONS(bdg) && (!proto || GET_BLD_INTERACTIONS(bdg) != GET_BLD_INTERACTIONS(proto))) {
		while ((interact = GET_BLD_INTERACTIONS(bdg))) {
			GET_BLD_INTERACTIONS(bdg) = interact->next;
			free(interact);
		}
	}
	if (GET_BLD_SPAWNS(bdg) && (!proto || GET_BLD_SPAWNS(bdg) != GET_BLD_SPAWNS(proto))) {
		while ((spawn = GET_BLD_SPAWNS(bdg))) {
			GET_BLD_SPAWNS(bdg) = spawn->next;
			free(spawn);
		}
	}
	
	if (GET_BLD_SCRIPTS(bdg) && (!proto || GET_BLD_SCRIPTS(bdg) != GET_BLD_SCRIPTS(proto))) {
		free_proto_scripts(&GET_BLD_SCRIPTS(bdg));
	}
	
	if (GET_BLD_YEARLY_MAINTENANCE(bdg) && (!proto || GET_BLD_YEARLY_MAINTENANCE(bdg) != GET_BLD_YEARLY_MAINTENANCE(proto))) {
		free_resource_list(GET_BLD_YEARLY_MAINTENANCE(bdg));
	}
	
	if (GET_BLD_RELATIONS(bdg) && (!proto || GET_BLD_RELATIONS(bdg) != GET_BLD_RELATIONS(proto))) {
		free_bld_relations(GET_BLD_RELATIONS(bdg));
	}
	
	free(bdg);
}


/**
* Clears out the values of a building_data object.
*
* @param bld_data *building The building to clear.
*/
void init_building(bld_data *building) {
	memset((char *)building, 0, sizeof(bld_data));
	
	building->upgrades_to = NOTHING;
	building->artisan_vnum = NOTHING;
}


/**
* Read one building from file.
*
* @param FILE *fl The open .bld file
* @param bld_vnum vnum The building vnum
*/
void parse_building(FILE *fl, bld_vnum vnum) {
	char line[256], str_in[256], str_in2[256];
	struct spawn_info *spawn, *stemp;
	struct bld_relation *relat;
	bld_data *bld, *find;
	int int_in[4];
	double dbl_in;

	CREATE(bld, bld_data, 1);
	init_building(bld);	
	GET_BLD_VNUM(bld) = vnum;

	HASH_FIND_INT(building_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate building vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_building_to_table(bld);
		
	// for error messages
	sprintf(buf2, "building vnum %d", vnum);
	
	// lines 1-3
	GET_BLD_NAME(bld) = fread_string(fl, buf2);
	GET_BLD_TITLE(bld) = fread_string(fl, buf2);
	GET_BLD_ICON(bld) = fread_string(fl, buf2);
	
	// line 4: max_damage fame flags
	if (!get_line(fl, line) || sscanf(line, "%d %d %s %s", &int_in[0], &int_in[1], str_in, str_in2) != 4) {
		log("SYSERR: Format error in line 4 of %s", buf2);
		exit(1);
	}
	
	GET_BLD_MAX_DAMAGE(bld) = int_in[0];
	GET_BLD_FAME(bld) = int_in[1];
	GET_BLD_FLAGS(bld) = asciiflag_conv(str_in);
	GET_BLD_DESIGNATE_FLAGS(bld) = asciiflag_conv(str_in2);
		
	// line 5: extra_rooms citizens military artisan_vnum
	if (!get_line(fl, line) || sscanf(line, "%d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
		log("SYSERR: Format error in line 5 of %s", buf2);
		exit(1);
	}
	
	GET_BLD_EXTRA_ROOMS(bld) = int_in[0];
	GET_BLD_CITIZENS(bld) = int_in[1];
	GET_BLD_MILITARY(bld) = int_in[2];
	GET_BLD_ARTISAN(bld) = int_in[3];
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
			exit(1);
		}
		switch (*line) {
			// affects
			case 'A': {
				if (!get_line(fl, line)) {
					log("SYSERR: format error in A line of %s", buf2);
					exit(1);
				}
				
				GET_BLD_BASE_AFFECTS(bld) = asciiflag_conv(line);
				break;
			}
			
			// commands
			case 'C': {
				GET_BLD_COMMANDS(bld) = fread_string(fl, buf2);
				break;
			}
			
			// description
			case 'D': {
				GET_BLD_DESC(bld) = fread_string(fl, buf2);
				break;
			}

			// extra desc
			case 'E': {
				parse_extra_desc(fl, &GET_BLD_EX_DESCS(bld), buf2);
				break;
			}
			
			case 'F': {	// functions
				if (!get_line(fl, line)) {
					log("SYSERR: format error in F line of %s", buf2);
					exit(1);
				}
				
				GET_BLD_FUNCTIONS(bld) = asciiflag_conv(line);
				break;	
			}
			
			case 'I': {	// interaction item
				parse_interaction(line, &GET_BLD_INTERACTIONS(bld), buf2);
				break;
			}
			
			// mob spawn
			case 'M': {
				if (!get_line(fl, line) || sscanf(line, "%d %lf %s", &int_in[0], &dbl_in, str_in) != 3) {
					log("SYSERR: Format error in M line of %s", buf2);
					exit(1);
				}
				
				CREATE(spawn, struct spawn_info, 1);
				spawn->vnum = int_in[0];
				spawn->percent = dbl_in;
				spawn->flags = asciiflag_conv(str_in);
				spawn->next = NULL;
				
				// append at end
				if ((stemp = GET_BLD_SPAWNS(bld))) {
					while (stemp->next) {
						stemp = stemp->next;
					}
					stemp->next = spawn;
				}
				else {
					GET_BLD_SPAWNS(bld) = spawn;
				}
				break;
			}
			
			case 'R': {	// resources/yearly maintenance
				parse_resource(fl, &GET_BLD_YEARLY_MAINTENANCE(bld), buf2);
				break;
			}
			
			case 'T': {	// trigger
				parse_trig_proto(line, &GET_BLD_SCRIPTS(bld), buf2);
				break;
			}
			
			case 'U': {	// relation (formerly 'upgrades to')
				if (!get_line(fl, line)) {
					log("SYSERR: Missing U line of %s", buf2);
					exit(1);
				}
				if (sscanf(line, "%d %d", &int_in[0], &int_in[1]) != 2) {
					if (sscanf(line, "%d", &int_in[1]) == 1) {
						// backwards-compatible to when U was only upgrades-to
						int_in[0] = BLD_REL_UPGRADES_TO;
					}
					else {	// error
						log("SYSERR: Format error in U line of %s", buf2);
						exit(1);
					}
				}
				
				CREATE(relat, struct bld_relation, 1);
				relat->type = int_in[0];
				relat->vnum = int_in[1];
				
				LL_APPEND(GET_BLD_RELATIONS(bld), relat);
				break;
			}

			// end
			case 'S': {
				return;
			}
			
			default: {
				log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
				exit(1);
			}
		}
	}
}


/**
* Outputs one building in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param bld_data *building The thing to save.
*/
void write_building_to_file(FILE *fl, bld_data *bld) {
	char temp[MAX_STRING_LENGTH], temp2[MAX_STRING_LENGTH];
	struct bld_relation *relat;
	struct spawn_info *spawn;
	
	if (!fl || !bld) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_building_to_file called without %s", !fl ? "file" : "building");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_BLD_VNUM(bld));
	
	fprintf(fl, "%s~\n", NULLSAFE(GET_BLD_NAME(bld)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_BLD_TITLE(bld)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_BLD_ICON(bld)));

	strcpy(temp, bitv_to_alpha(GET_BLD_FLAGS(bld)));
	strcpy(temp2, bitv_to_alpha(GET_BLD_DESIGNATE_FLAGS(bld)));
	fprintf(fl, "%d %d %s %s\n", GET_BLD_MAX_DAMAGE(bld), GET_BLD_FAME(bld), temp, temp2);
	fprintf(fl, "%d %d %d %d\n", GET_BLD_EXTRA_ROOMS(bld), GET_BLD_CITIZENS(bld), GET_BLD_MILITARY(bld), GET_BLD_ARTISAN(bld));
	
	// A: base_affects
	if (GET_BLD_BASE_AFFECTS(bld) != NOBITS) {
		fprintf(fl, "A\n");
		fprintf(fl, "%s\n", bitv_to_alpha(GET_BLD_BASE_AFFECTS(bld)));
	}
	
	// C: commands list
	if (GET_BLD_COMMANDS(bld) && *GET_BLD_COMMANDS(bld)) {
		fprintf(fl, "C\n");
		fprintf(fl, "%s~\n", GET_BLD_COMMANDS(bld));
	}
	
	// D: description text
	if (GET_BLD_DESC(bld) && *GET_BLD_DESC(bld)) {
		fprintf(fl, "D\n");
		strcpy(temp, GET_BLD_DESC(bld));
		strip_crlf(temp);
		fprintf(fl, "%s~\n", temp);
	}
	
	// E: extra descriptions
	write_extra_descs_to_file(fl, GET_BLD_EX_DESCS(bld));
	
	// F: functions
	if (GET_BLD_FUNCTIONS(bld)) {
		fprintf(fl, "F\n");
		fprintf(fl, "%s\n", bitv_to_alpha(GET_BLD_FUNCTIONS(bld)));
	}
	
	// I: interactions
	write_interactions_to_file(fl, GET_BLD_INTERACTIONS(bld));
	
	// M: mob spawns
	for (spawn = GET_BLD_SPAWNS(bld); spawn; spawn = spawn->next) {
		fprintf(fl, "M\n");
		fprintf(fl, "%d %.2f %s\n", spawn->vnum, spawn->percent, bitv_to_alpha(spawn->flags));
	}
	
	// 'R': resources
	write_resources_to_file(fl, 'R', GET_BLD_YEARLY_MAINTENANCE(bld));
	
	// T: triggers
	write_trig_protos_to_file(fl, 'T', GET_BLD_SCRIPTS(bld));
	
	// U: relations (formerly upgrades_to)
	LL_FOREACH(GET_BLD_RELATIONS(bld), relat) {
		fprintf(fl, "U\n%d %d\n", relat->type, relat->vnum);
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// CRAFT LIB ///////////////////////////////////////////////////////////////

/**
* Puts a craft in the hash tables.
*
* @param craft_data *craft The craft to add to the table.
*/
void add_craft_to_table(craft_data *craft) {
	extern int sort_crafts_by_data(craft_data *a, craft_data *b);
	extern int sort_crafts_by_vnum(craft_data *a, craft_data *b);
	
	craft_data *find;
	craft_vnum vnum;
	
	if (craft) {
		vnum = GET_CRAFT_VNUM(craft);
		
		// main table
		HASH_FIND_INT(craft_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(craft_table, vnum, craft);
			HASH_SORT(craft_table, sort_crafts_by_vnum);
		}
		
		// sorted table
		HASH_FIND(sorted_hh, sorted_crafts, &vnum, sizeof(int), find);
		if (!find) {
			HASH_ADD(sorted_hh, sorted_crafts, vnum, sizeof(int), craft);
			HASH_SRT(sorted_hh, sorted_crafts, sort_crafts_by_data);
		}
	}
}

/**
* Removes a craft from the hash tables.
*
* @param craft_data *craft The craft to remove from the tables.
*/
void remove_craft_from_table(craft_data *craft) {
	HASH_DEL(craft_table, craft);
	HASH_DELETE(sorted_hh, sorted_crafts, craft);
}


/**
* frees up memory for a craft
*
* See also: olc_delete_craft
*
* @param craft_data *craft The craft to free.
*/
void free_craft(craft_data *craft) {
	craft_data *proto = craft_proto(craft->vnum);
	
	if (GET_CRAFT_NAME(craft) && (!proto || GET_CRAFT_NAME(craft) != GET_CRAFT_NAME(proto))) {
		free(GET_CRAFT_NAME(craft));
	}
	
	if (GET_CRAFT_RESOURCES(craft) && (!proto || GET_CRAFT_RESOURCES(craft) != GET_CRAFT_RESOURCES(proto))) {
		free_resource_list(GET_CRAFT_RESOURCES(craft));
	}
	
	free(craft);
}


/**
* Clears out the values of a craft recipe object.
*
* @param craft_data *craft The craft recipe to clear.
*/
void init_craft(craft_data *craft) {
	memset((char *)craft, 0, sizeof(craft_data));
	
	// clear/default some stuff
	GET_CRAFT_OBJECT(craft) = NOTHING;
	GET_CRAFT_REQUIRES_OBJ(craft) = NOTHING;
	GET_CRAFT_QUANTITY(craft) = 1;
	GET_CRAFT_TIME(craft) = 1;
}


/**
* Read one craft from file.
*
* @param FILE *fl The open .craft file
* @param craft_vnum vnum The craft vnum
*/
void parse_craft(FILE *fl, craft_vnum vnum) {
	char line[256], str_in[256], str_in2[256];
	craft_data *craft, *find;
	int int_in[4];
	
	CREATE(craft, craft_data, 1);
	init_craft(craft);
	GET_CRAFT_VNUM(craft) = vnum;

	HASH_FIND_INT(craft_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate craft vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_craft_to_table(craft);
		
	// for error messages
	sprintf(buf2, "craft vnum %d", vnum);
	
	// line 1: name~
	GET_CRAFT_NAME(craft) = fread_string(fl, buf2);
	
	// line 2: vnum quantity
	if (!get_line(fl, line) || sscanf(line, "%d %d", int_in, int_in + 1) != 2) {
		log("SYSERR: Format error in line 2 of craft recipe #%d", vnum);
		exit(1);
	}
	
	GET_CRAFT_OBJECT(craft) = int_in[0];
	GET_CRAFT_QUANTITY(craft) = int_in[1];
	
	// line 3: type ability flags time
	if (!get_line(fl, line) || sscanf(line, "%d %d %s %d %d", int_in, int_in + 1, str_in, int_in + 2, int_in + 3) != 5) {
		log("SYSERR: Format error in line 3 of craft recipe #%d", vnum);
		exit(1);
	}
	
	GET_CRAFT_TYPE(craft) = int_in[0];
	GET_CRAFT_ABILITY(craft) = int_in[1];
	GET_CRAFT_FLAGS(craft) = asciiflag_conv(str_in);
	GET_CRAFT_TIME(craft) = int_in[2];
	GET_CRAFT_REQUIRES_OBJ(craft) = int_in[3];
	
	// optionals

	// for error messages
	sprintf(buf2, "craft vnum %d, after numeric constants... expecting alphabetic flags", vnum);

	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s", buf2);
			exit(1);
		}
		switch (*line) {
			// building info
			case 'B': {
				if (!get_line(fl, line) || sscanf(line, "%d %s %s", &int_in[0], str_in, str_in2) != 3) {
					log("SYSERR: Format error in B section of craft recipe #%d", vnum);
					exit(1);
				}
				
				GET_CRAFT_BUILD_TYPE(craft) = int_in[0];
				GET_CRAFT_BUILD_ON(craft) = asciiflag_conv(str_in);
				GET_CRAFT_BUILD_FACING(craft) = asciiflag_conv(str_in2);
				break;
			}
			
			// L: Minimum crafting level
			case 'L': {
				if (!get_line(fl, line) || sscanf(line, "%d", &int_in[0]) != 1) {
					log("SYSERR: Format error in L section of craft recipe #%d", vnum);
					exit(1);
				}
				
				GET_CRAFT_MIN_LEVEL(craft) = int_in[0];
				break;
			}
			
			case 'R': {	// resources
				parse_resource(fl, &GET_CRAFT_RESOURCES(craft), buf2);
				break;
			}

			// end
			case 'S': {
				return;
			}
			
			default: {
				log("SYSERR: Format error in %s", buf2);
				exit(1);
			}
		}
	}
}


/**
* Outputs one craft recipe in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param craft_data *craft The thing to save.
*/
void write_craft_to_file(FILE *fl, craft_data *craft) {
	char temp1[256], temp2[256];
	
	if (!fl || !craft) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_craft_to_file called without %s", !fl ? "file" : "craft");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_CRAFT_VNUM(craft));
	fprintf(fl, "%s~\n", NULLSAFE(GET_CRAFT_NAME(craft)));
	
	fprintf(fl, "%d %d\n", GET_CRAFT_OBJECT(craft), GET_CRAFT_QUANTITY(craft));
	fprintf(fl, "%d %d %s %d %d\n", GET_CRAFT_TYPE(craft), GET_CRAFT_ABILITY(craft), bitv_to_alpha(GET_CRAFT_FLAGS(craft)), GET_CRAFT_TIME(craft), GET_CRAFT_REQUIRES_OBJ(craft));
	
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {
		strcpy(temp1, bitv_to_alpha(GET_CRAFT_BUILD_ON(craft)));
		strcpy(temp2, bitv_to_alpha(GET_CRAFT_BUILD_FACING(craft)));
		
		fprintf(fl, "B\n");
		fprintf(fl, "%d %s %s\n", GET_CRAFT_BUILD_TYPE(craft), temp1, temp2);
	}
	
	if (GET_CRAFT_MIN_LEVEL(craft) > 0) {
		fprintf(fl, "L\n");
		fprintf(fl, "%d\n", GET_CRAFT_MIN_LEVEL(craft));
	}
	
	// 'R': resources
	write_resources_to_file(fl, 'R', GET_CRAFT_RESOURCES(craft));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// CROP LIB ////////////////////////////////////////////////////////////////

/**
* Puts a crop in the hash table.
*
* @param crop_data *crop The crop to add to the table.
*/
void add_crop_to_table(crop_data *crop) {
	extern int sort_crops(crop_data *a, crop_data *b);
	
	crop_data *find;
	crop_vnum vnum;
	
	if (crop) {
		vnum = GET_CROP_VNUM(crop);
		HASH_FIND_INT(crop_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(crop_table, vnum, crop);
			HASH_SORT(crop_table, sort_crops);
		}
	}
}

/**
* Removes a crop from the hash table.
*
* @param crop_data *crop The crop to remove from the table.
*/
void remove_crop_from_table(crop_data *crop) {
	HASH_DEL(crop_table, crop);
}


/**
* frees up the memory for a crop item
*
* See also: olc_delete_crop
*
* @param crop_data *cp The crop to free.
*/
void free_crop(crop_data *cp) {
	crop_data *proto = crop_proto(cp->vnum);
	struct spawn_info *spawn;
	struct interaction_item *interact;
	
	if (GET_CROP_NAME(cp) && (!proto || GET_CROP_NAME(cp) != GET_CROP_NAME(proto))) {
		free(GET_CROP_NAME(cp));
	}
	if (GET_CROP_TITLE(cp) && (!proto || GET_CROP_TITLE(cp) != GET_CROP_TITLE(proto))) {
		free(GET_CROP_TITLE(cp));
	}
	
	if (GET_CROP_ICONS(cp) && (!proto || GET_CROP_ICONS(cp) != GET_CROP_ICONS(proto))) {
		free_icon_set(&GET_CROP_ICONS(cp));
	}
		
	if (GET_CROP_SPAWNS(cp) && (!proto || GET_CROP_SPAWNS(cp) != GET_CROP_SPAWNS(proto))) {
		while ((spawn = GET_CROP_SPAWNS(cp))) {
			GET_CROP_SPAWNS(cp) = spawn->next;
			free(spawn);
		}
	}

	if (GET_CROP_INTERACTIONS(cp) && (!proto || GET_CROP_INTERACTIONS(cp) != GET_CROP_INTERACTIONS(proto))) {
		while ((interact = GET_CROP_INTERACTIONS(cp))) {
			GET_CROP_INTERACTIONS(cp) = interact->next;
			free(interact);
		}
	}

	free(cp);
}


/**
* Clears out the data of a crop_data object.
*
* @param crop_data *cp The crop item to clear.
*/
void init_crop(crop_data *cp) {
	memset((char *)cp, 0, sizeof(crop_data));
}


/**
* Read one crop from file.
*
* @param FILE *fl The open .crop file
* @param crop_vnum vnum The crop vnum
*/
void parse_crop(FILE *fl, crop_vnum vnum) {
	crop_data *crop, *find;
	int int_in[4];
	double dbl_in;
	char line[256], str_in[256];
	struct spawn_info *spawn, *stemp;
	
	// create
	CREATE(crop, crop_data, 1);
	init_crop(crop);
	crop->vnum = vnum;

	HASH_FIND_INT(crop_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate crop vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_crop_to_table(crop);
		
	// for error messages
	sprintf(buf2, "crop vnum %d", vnum);
	
	// lines 1-2
	GET_CROP_NAME(crop) = fread_string(fl, buf2);
	GET_CROP_TITLE(crop) = fread_string(fl, buf2);
	
	// line 3: mapout, climate, flags
	if (!get_line(fl, line) || sscanf(line, "%d %d %s", &int_in[0], &int_in[1], str_in) != 3) {
		log("SYSERR: Format error in line 3 of %s", buf2);
		exit(1);
	}
	
	GET_CROP_MAPOUT(crop) = int_in[0];
	GET_CROP_CLIMATE(crop) = int_in[1];
	GET_CROP_FLAGS(crop) = asciiflag_conv(str_in);
			
	// line 4: x_min, x_max, y_min, y_max
	if (!get_line(fl, line) || sscanf(line, "%d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
		log("SYSERR: Format error in line 4 of %s", buf2);
		exit(1);
	}
	
	GET_CROP_X_MIN(crop) = int_in[0];
	GET_CROP_X_MAX(crop) = int_in[1];
	GET_CROP_Y_MIN(crop) = int_in[2];
	GET_CROP_Y_MAX(crop) = int_in[3];
		
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
			exit(1);
		}
		switch (*line) {
			// icons sets
			case 'D': {
				parse_icon(line, fl, &GET_CROP_ICONS(crop), buf2);
				break;
			}
			
			case 'I': {	// interaction item
				parse_interaction(line, &GET_CROP_INTERACTIONS(crop), buf2);
				break;
			}
			
			// mob spawn
			case 'M': {
				if (!get_line(fl, line) || sscanf(line, "%d %lf %s", &int_in[0], &dbl_in, str_in) != 3) {
					log("SYSERR: Format error in M line of %s", buf2);
					exit(1);
				}
				
				CREATE(spawn, struct spawn_info, 1);
				spawn->vnum = int_in[0];
				spawn->percent = dbl_in;
				spawn->flags = asciiflag_conv(str_in);
				spawn->next = NULL;
				
				// append at end
				if ((stemp = GET_CROP_SPAWNS(crop))) {
					while (stemp->next) {
						stemp = stemp->next;
					}
					stemp->next = spawn;
				}
				else {
					GET_CROP_SPAWNS(crop) = spawn;
				}
				break;
			}

			// end
			case 'S': {
				return;
			}
			
			default: {
				log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
				exit(1);
			}
		}
	}
}


/**
* Outputs one crop in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param crop_data *cp The thing to save.
*/
void write_crop_to_file(FILE *fl, crop_data *cp) {
	struct spawn_info *spawn;
	
	if (!fl || !cp) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_crop_to_file called without %s", !fl ? "file" : "crop");
		return;
	}
	
	fprintf(fl, "#%d\n", cp->vnum);
	
	fprintf(fl, "%s~\n", NULLSAFE(cp->name));
	fprintf(fl, "%s~\n", NULLSAFE(cp->title));

	fprintf(fl, "%d %d %s\n", GET_CROP_MAPOUT(cp), cp->climate, bitv_to_alpha(cp->flags));
	fprintf(fl, "%d %d %d %d\n", cp->x_min, cp->x_max, cp->y_min, cp->y_max);
	
	// D: icons
	write_icons_to_file(fl, 'D', cp->icons);
	
	// I: interactions
	write_interactions_to_file(fl, cp->interactions);
	
	// M: mob spawns
	for (spawn = cp->spawns; spawn; spawn = spawn->next) {
		fprintf(fl, "M\n");
		fprintf(fl, "%d %.2f %s\n", spawn->vnum, spawn->percent, bitv_to_alpha(spawn->flags));
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE LIB //////////////////////////////////////////////////////////////

/**
* Puts an empire in the hash table.
*
* @param empire_data *emp The empire to add to the table.
*/
void add_empire_to_table(empire_data *emp) {	
	empire_data *find;
	empire_vnum vnum;
	
	if (emp) {
		vnum = EMPIRE_VNUM(emp);
		HASH_FIND_INT(empire_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(empire_table, vnum, emp);
			HASH_SORT(empire_table, sort_empires);
		}
	}
}

/**
* Removes an empire from the hash table.
*
* @param empire_data *emp The empire to remove from the table.
*/
void remove_empire_from_table(empire_data *emp) {
	HASH_DEL(empire_table, emp);
}


/**
* This function detects when the mud boots up on a new map, and moves all
* empire territory to "no island" (to be moved back later). It also cleans up
* empire data and ensures players don't log in mid-ocean.
*/
void check_for_new_map(void) {
	void delete_instance(struct instance_data *inst, bool run_cleanup);	// instance.c
	void update_all_players(char_data *to_message, PLAYER_UPDATE_FUNC(*func));
	
	struct empire_storage_data *store, *next_store;
	struct empire_territory_data *ter, *next_ter;
	struct empire_trade_data *trade, *next_trade;
	struct empire_city_data *city, *next_city;
	struct shipping_data *shipd, *next_shipd;
	struct empire_island *isle, *next_isle;
	struct instance_data *inst, *next_inst;
	struct empire_unique_storage *eus;
	empire_data *emp, *next_emp;
	room_data *room;
	FILE *fl;
	
	if (!(fl = fopen(NEW_WORLD_HINT_FILE, "r"))) {
		return;	// no new world
	}
	fclose(fl);
	
	log("DETECT NEW WORLD MAP -- Clearing empire islands and player locations...");
	
	// ensure no instances in the instance list-- their locations SHOULD be all gone anyway
	LL_FOREACH_SAFE(instance_list, inst, next_inst) {
		if (inst->room) {
			free(inst->room);
		}
		if (INST_MOB_COUNTS(inst)) {
			free(INST_MOB_COUNTS(inst));
		}
		LL_DELETE(instance_list, inst);
		free(inst);
	}
	
	// update all empires
	HASH_ITER(hh, empire_table, emp, next_emp) {
		// move unique storage
		LL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
			eus->island = NO_ISLAND;	// simple move, for now
		}
		
		// free shipping (and put the items back)
		LL_FOREACH_SAFE(EMPIRE_SHIPPING_LIST(emp), shipd, next_shipd) {
			add_to_empire_storage(emp, NO_ISLAND, shipd->vnum, shipd->amount);
			free(shipd);	// no need to remove from list
		}
		EMPIRE_SHIPPING_LIST(emp) = NULL;	// all entries freed
		
		// free trade (no longer relevant)
		LL_FOREACH_SAFE(EMPIRE_TRADE(emp), trade, next_trade) {
			free(trade);
		}
		EMPIRE_TRADE(emp) = NULL;
		
		// free cities
		LL_FOREACH_SAFE(EMPIRE_CITY_LIST(emp), city, next_city) {
			if (city->name) {
				free(city->name);
			}
			room = city->location;
			if (room) {
				if (IS_CITY_CENTER(room)) {
					disassociate_building(room);
				}
				if (ROOM_OWNER(room)) {
					abandon_room(room);
				}
			}
			free(city);	// no need to remove from list
		}
		EMPIRE_CITY_LIST(emp) = NULL;	// all entries freed
		
		// free territory lists and clear numbers
		HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter, next_ter) {
			if (ter == global_next_territory_entry) {
				global_next_territory_entry = ter->hh.next;
			}
			
			// free npcs
			while (ter->npcs) {
				delete_territory_npc(ter, ter->npcs);
			}
			
			HASH_DEL(EMPIRE_TERRITORY_LIST(emp), ter);
			free(ter);
		}
		EMPIRE_TERRITORY_LIST(emp) = NULL;	// all entires freed
		
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
			if (isle->island == NO_ISLAND) {
				continue;	// we ONLY keep no-island data
			}
			
			// move all inventories to nowhere
			HASH_ITER(hh, isle->store, store, next_store) {
				add_to_empire_storage(emp, NO_ISLAND, store->vnum, store->amount);
				HASH_DEL(isle->store, store);
				free(store);
			}
			
			// remove island data
			if (isle->name) {
				free(isle->name);
			}
			HASH_DEL(EMPIRE_ISLANDS(emp), isle);
			free(isle);
		}
		
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
		EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	}
	
	// do this last in case of error
	save_all_empires();
	unlink(NEW_WORLD_HINT_FILE);
	
	// now move all player login locations to NOWHERE
	update_all_players(NULL, send_all_players_to_nowhere);
	
	// rescan all empires
	reread_empire_tech(NULL);
}


/**
* This function looks for any empire inventory that's in a "no island" location
* and moves it to the specified island. This includes unique storage.
*
* @param empire_data *emp The empire to move.
* @param int new_island The ID of the island to move to.
*/
void check_nowhere_einv(empire_data *emp, int new_island) {
	struct empire_storage_data *store, *next_store;
	struct empire_unique_storage *eus;
	struct empire_island *no_isle;
	bool any = FALSE;
	
	if (!emp || new_island == NO_ISLAND) {
		return;	// can't do
	}
	
	// move basic storage
	if ((no_isle = get_empire_island(emp, NO_ISLAND))) {
		HASH_ITER(hh, no_isle->store, store, next_store) {
			add_to_empire_storage(emp, new_island, store->vnum, store->amount);
			HASH_DEL(no_isle->store, store);
			free(store);
			any = TRUE;
		}
		no_isle->store = NULL;
	}
	
	// move unique storage
	LL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
		if (eus->island == NO_ISLAND) {
			eus->island = new_island;	// simple move, for now
			any = TRUE;
		}
	}
	
	if (any) {	// reporting
		EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
		log_to_empire(emp, ELOG_TERRITORY, "Empire inventory delivered to the new island");
	}
}


/**
* Checks all empires to ensure they have no "missing" einv in the no-island
* state. This is meant to be run at startup but can be re-run as needed.
*/
void check_nowhere_einv_all(void) {
	extern int get_main_island(empire_data *emp);
	
	empire_data *emp, *next_emp;
	int island;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if ((island = get_main_island(emp)) != NO_ISLAND) {
			check_nowhere_einv(emp, island);
		}
	}
}


/**
* Creates a new empire with default ranks and ch as leader. The default empire
* name is the player's name so that new players will see "This area is claimed
* by <your name>", which fits the concept that small empires are just land
* claimed by one person.
*
* @param char_data *ch The player who will lead the empire.
* @return empire_data* A pointer to the new empire (sometimes NULL
*/
empire_data *create_empire(char_data *ch) {
	void add_empire_to_table(empire_data *emp);
	extern bool check_unique_empire_name(empire_data *for_emp, char *name);
	void refresh_empire_goals(empire_data *emp, any_vnum only_vnum);
	void resort_empires(bool force);

	archetype_data *arch;
	char colorcode[10], name[MAX_STRING_LENGTH];
	empire_vnum vnum;
	empire_data *emp;
	int iter;
	
	// &r, etc
	char *colorlist = "rgbymcajloptvnRGBYMCAJLOPTV";
	int num_colors = 12;
	
	// this SHOULD always find a vnum
	vnum = find_free_empire_vnum();
	if (vnum == NOTHING || !ch || IS_NPC(ch) || GET_LOYALTY(ch)) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Unable to create an empire for %s", ch ? GET_NAME(ch) : "NULL");
		return NULL;
	}
	
	// determine a name
	sprintf(name, "%s", PERS(ch, ch, TRUE));	// 1st attempt
	if (!check_unique_empire_name(NULL, name)) {
		sprintf(name, "The %s Empire", PERS(ch, ch, TRUE));	// 2nd attempt
		if (!check_unique_empire_name(NULL, name)) {
			for (iter = 2; iter < 99; ++iter) {
				sprintf(name, "%s %d", PERS(ch, ch, TRUE), iter);	// 3rd attempt
				if (check_unique_empire_name(NULL, name)) {
					break;	// valid name
				}
			}
		}
	}
	
	// basic creation
	CREATE(emp, empire_data, 1);
	EMPIRE_VNUM(emp) = vnum;
	add_empire_to_table(emp);
	
	// starter data
	EMPIRE_NAME(emp) = str_dup(name);
	EMPIRE_ADJECTIVE(emp) = str_dup(name);
	sprintf(colorcode, "&%c", colorlist[number(0, num_colors-1)]);	// pick random color
	EMPIRE_BANNER(emp) = str_dup(colorcode);
	
	EMPIRE_CREATE_TIME(emp) = time(0);

	// member data
	EMPIRE_LEADER(emp) = GET_IDNUM(ch);
	EMPIRE_MEMBERS(emp) = 1;
	EMPIRE_GREATNESS(emp) = GET_GREATNESS(ch);
	if (GET_ACCESS_LEVEL(ch) >= LVL_GOD) {
		EMPIRE_IMM_ONLY(emp) = 1;
	}
	
	// rank setup
	arch = archetype_proto(CREATION_ARCHETYPE(ch, ARCHT_ORIGIN));
	if (!arch) {
		arch = archetype_proto(0);	// default to 0
	}
	EMPIRE_NUM_RANKS(emp) = 2;
	EMPIRE_RANK(emp, 0) = str_dup("Follower");
	EMPIRE_RANK(emp, 1) = str_dup(arch ? (GET_REAL_SEX(ch) == SEX_FEMALE ? GET_ARCH_FEMALE_RANK(arch) : GET_ARCH_MALE_RANK(arch)) : "Leader");
	for (iter = 0; iter < NUM_PRIVILEGES; ++iter) {
		EMPIRE_PRIV(emp, iter) = 2;
	}
	
	// this is necessary to save some parts at all
	emp->storage_loaded = TRUE;
	emp->logs_loaded = TRUE;

	// set the player up
	remove_lore(ch, LORE_PROMOTED);
	add_lore(ch, LORE_FOUND_EMPIRE, "Proudly founded %s%s&0", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
	GET_LOYALTY(ch) = emp;
	GET_RANK(ch) = 2;
	SAVE_CHAR(ch);

	save_empire(emp, TRUE);
	save_index(DB_BOOT_EMP);
	
	// this will set up all the data
	reread_empire_tech(emp);
	refresh_empire_goals(emp, NOTHING);

	// this is a good time to sort and rank
	resort_empires(FALSE);
	
	return emp;
}


/**
* Complex deletion of an empire: there's a lot of data to set free, including
* offline players.
*
* @param empire_data *emp The empire to delete.
*/
void delete_empire(empire_data *emp) {
	void remove_empire_from_table(empire_data *emp);

	struct empire_political_data *emp_pol, *next_pol, *temp;
	player_index_data *index, *next_index;
	struct vehicle_attached_mob *vam;
	empire_data *emp_iter, *next_emp;
	vehicle_data *veh, *next_veh;
	room_data *room, *next_room;
	char buf[MAX_STRING_LENGTH];
	obj_data *obj, *next_obj;
	char_data *ch, *next_ch;
	bool file = FALSE;
	empire_vnum vnum;

	if (!emp) {
		return;
	}
	
	// prepare
	log_to_empire(emp, ELOG_NONE, "This empire has been deleted");
	
	// remove from the table right away
	remove_empire_from_table(emp);

	// store this for later
	vnum = EMPIRE_VNUM(emp);

	// things to unset on other empires
	HASH_ITER(hh, empire_table, emp_iter, next_emp) {
		for (emp_pol = EMPIRE_DIPLOMACY(emp_iter); emp_pol; emp_pol = next_pol) {
			next_pol = emp_pol->next;
			if (emp_pol->id == vnum) {
				REMOVE_FROM_LIST(emp_pol, EMPIRE_DIPLOMACY(emp_iter), next);
				free(emp_pol);
				
				et_change_diplomacy(emp_iter);
				EMPIRE_NEEDS_SAVE(emp_iter) = TRUE;
			}
		}
	}
	
	// clean up players
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
		if (index->loyalty != emp) {
			continue;
		}
		if ((ch = is_playing(index->idnum)) || (ch = is_at_menu(index->idnum))) {
			if (IN_ROOM(ch)) {
				msg_to_char(ch, "Your empire has been destroyed.  You are no longer a member.\r\n");
			}
			GET_LOYALTY(ch) = NULL;
			GET_RANK(ch) = 0;
			update_player_index(index, ch);
		}
		else if ((ch = find_or_load_player(index->name, &file))) {
			GET_LOYALTY(ch) = NULL;
			GET_RANK(ch) = 0;
			update_player_index(index, ch);
			if (file) {
				store_loaded_char(ch);
			}
		}
	}
	
	// update all mobs
	for (ch = character_list; ch; ch = next_ch) {
		next_ch = ch->next;
		
		// this is "theoretically" just NPCs since we did players already
		if (GET_LOYALTY(ch) == emp) {
			GET_LOYALTY(ch) = NULL;
			
			if (IS_NPC(ch)) {
				SET_BIT(MOB_FLAGS(ch), MOB_SPAWNED);	// trigger a de-spawn
			}
		}
	}
	
	// update all objs
	for (obj = object_list; obj; obj = next_obj) {
		next_obj = obj->next;
		
		if (obj->last_empire_id == vnum) {
			obj->last_empire_id = NOTHING;
		}
	}
	
	// update all vehicles
	LL_FOREACH_SAFE2(vehicle_list, veh, next_veh, next) {
		if (VEH_OWNER(veh) == emp) {
			VEH_OWNER(veh) = NULL;
			VEH_SHIPPING_ID(veh) = -1;
		}
		LL_FOREACH(VEH_ANIMALS(veh), vam) {
			if (vam->empire == vnum) {
				vam->empire = NOTHING;
			}
		}
	}
	
	// remove all world ownership
	HASH_ITER(hh, world_table, room, next_room) {
		if (ROOM_OWNER(room) == emp) {
			perform_abandon_room(room);
		}
	}
	
	// save index first (before deleting files)
	save_index(DB_BOOT_EMP);
	
	sprintf(buf, "%s%d%s", LIB_EMPIRE, vnum, EMPIRE_SUFFIX);
	unlink(buf);
	sprintf(buf, "%s%d%s", STORAGE_PREFIX, vnum, EMPIRE_SUFFIX);
	unlink(buf);
	
	cleanup_all_coins();
	
	free_empire(emp);
}


/**
* Removes a territory NPC entry, and removes the citizen if it's spawned. This
* will free the "npc" argument after removing it from the territory npc list.
*
* @param struct empire_territory_data *ter The territory entry.
* @param struct empire_npc_data *npc The npc data.
*/
void delete_territory_npc(struct empire_territory_data *ter, struct empire_npc_data *npc) {
	struct empire_island *isle;
	empire_data *emp;
	
	if (!ter || !npc) {
		return;
	}
	
	// this MAY not exist anymore
	emp = ROOM_OWNER(HOME_ROOM(ter->room));
	
	// remove mob if any
	if (npc->mob) {
		GET_EMPIRE_NPC_DATA(npc->mob) = NULL;	// un-link this npc data from the mob, or extract will corrupt memory
		
		if (!EXTRACTED(npc->mob) && !IS_DEAD(npc->mob)) {
			act("$n leaves.", TRUE, npc->mob, NULL, NULL, TO_ROOM);
			extract_char(npc->mob);
		}
		npc->mob = NULL;
	}
	
	// reduce pop
	if (emp) {
		EMPIRE_POPULATION(emp) -= 1;
		if (!GET_ROOM_VEHICLE(ter->room) && (isle = get_empire_island(emp, GET_ISLAND_ID(ter->room)))) {
			isle->population -= 1;
		}
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
	
	LL_DELETE(ter->npcs, npc);
	free(npc);
}


/**
* Frees a set of workforce trackers.
*
* @param struct empire_workforce_tracker **tracker A pointer to the hash table of trackers.
*/
void ewt_free_tracker(struct empire_workforce_tracker **tracker) {
	struct empire_workforce_tracker *ewt, *next_ewt;
	struct empire_workforce_tracker_island *isle, *next_isle;
	
	HASH_ITER(hh, *tracker, ewt, next_ewt) {
		HASH_ITER(hh, ewt->islands, isle, next_isle) {
			HASH_DEL(ewt->islands, isle);
			free(isle);
		}
		
		HASH_DEL(*tracker, ewt);
		free(ewt);
	}
	*tracker = NULL;
}


/**
* Frees up strings and lists in the empire.
*
* @param empire_data *emp The empire to free
*/
void free_empire(empire_data *emp) {
	void free_empire_goals(struct empire_goal *hash);
	void free_empire_completed_goals(struct empire_completed_goal *hash);
	
	struct workforce_delay_chore *wdc, *next_wdc;
	struct workforce_delay *delay, *next_delay;
	struct empire_island *isle, *next_isle;
	struct empire_storage_data *store, *next_store;
	struct empire_unique_storage *eus;
	struct empire_territory_data *ter, *next_ter;
	struct player_craft_data *pcd, *next_pcd;
	struct empire_needs *needs, *next_needs;
	struct empire_city_data *city;
	struct empire_political_data *pol;
	struct empire_trade_data *trade;
	struct empire_log_data *elog;
	struct workforce_log *wf_log;
	struct shipping_data *shipd;
	room_data *room;
	int iter;
	
	// free islands
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		HASH_ITER(hh, isle->needs, needs, next_needs) {
			HASH_DEL(isle->needs, needs);
			free(needs);
		}
		HASH_ITER(hh, isle->store, store, next_store) {
			HASH_DEL(isle->store, store);
			free(store);
		}
		HASH_DEL(EMPIRE_ISLANDS(emp), isle);
		free(isle);
	}
	EMPIRE_ISLANDS(emp) = NULL;
	
	// free unique storage
	while ((eus = EMPIRE_UNIQUE_STORAGE(emp))) {
		EMPIRE_UNIQUE_STORAGE(emp) = eus->next;
		if (eus->obj) {
			extract_obj(eus->obj);
		}
	}
	EMPIRE_UNIQUE_STORAGE(emp) = NULL;
	
	// free shipping data
	while ((shipd = EMPIRE_SHIPPING_LIST(emp))) {
		EMPIRE_SHIPPING_LIST(emp) = shipd->next;
		free(shipd);
	}
	EMPIRE_SHIPPING_LIST(emp) = NULL;
	
	// free cities (while they last)
	while ((city = emp->city_list)) {
		if (city->name) {
			free(city->name);
		}
		room = city->location;
		if (room && IS_CITY_CENTER(room)) {
			disassociate_building(room);
		}
		
		emp->city_list = city->next;
		free(city);
	}
	
	// free learned crafts
	HASH_ITER(hh, EMPIRE_LEARNED_CRAFTS(emp), pcd, next_pcd) {
		HASH_DEL(EMPIRE_LEARNED_CRAFTS(emp), pcd);
		free(pcd);
	}
	
	// free trades
	while ((trade = emp->trade)) {
		emp->trade = trade->next;
		free(trade);
	}
	
	// free logs
	while ((elog = emp->logs)) {
		emp->logs = elog->next;
		if (elog->string) {
			free(elog->string);
		}
		free(elog);
	}
	
	// free territory
	HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter, next_ter) {
		if (ter == global_next_territory_entry) {
			global_next_territory_entry = ter->hh.next;
		}
		
		// free npcs
		while (ter->npcs) {
			delete_territory_npc(ter, ter->npcs);
		}
		
		HASH_DEL(EMPIRE_TERRITORY_LIST(emp), ter);
		free(ter);
	}
	
	// free diplomacy
	while ((pol = emp->diplomacy)) {
		emp->diplomacy = pol->next;
		free(pol);
	}
	
	// free offenses
	while (EMPIRE_OFFENSES(emp)) {
		remove_offense(emp, EMPIRE_OFFENSES(emp));
	}
	
	// free delays
	HASH_ITER(hh, EMPIRE_DELAYS(emp), delay, next_delay) {
		LL_FOREACH_SAFE(delay->chores, wdc, next_wdc) {
			free(wdc);
		}
		HASH_DEL(EMPIRE_DELAYS(emp), delay);
		free(delay);
	}
	
	// free workforce log
	while ((wf_log = EMPIRE_WORKFORCE_LOG(emp))) {
		EMPIRE_WORKFORCE_LOG(emp) = wf_log->next;
		free(wf_log);
	}
	
	free_theft_logs(EMPIRE_THEFT_LOGS(emp));
	
	// free goals
	free_empire_goals(EMPIRE_GOALS(emp));
	free_empire_completed_goals(EMPIRE_COMPLETED_GOALS(emp));
	
	// free strings
	if (emp->name) {
		free(emp->name);
	}
	if (emp->banner) {
		free(emp->banner);
	}
	if (EMPIRE_MOTD(emp)) {
		free(EMPIRE_MOTD(emp));
	}
	if (EMPIRE_DESCRIPTION(emp)) {
		free(EMPIRE_DESCRIPTION(emp));
	}
	for (iter = 0; iter < 20; iter++) {
		if (emp->rank[iter]) {
			free(emp->rank[iter]);
		}
	}
	ewt_free_tracker(&EMPIRE_WORKFORCE_TRACKER(emp));
	
	free(emp);
}


/**
* Delayed-load of logs for an empire.
*
* @param FILE *fl The open read file.
* @param empire_data *emp The empire to assign the logs to.
*/
void load_empire_logs_one(FILE *fl, empire_data *emp) {	
	char line[1024], str_in[256], buf[MAX_STRING_LENGTH];
	struct empire_log_data *elog, *last_log = NULL;
	struct offense_data *off, *last_off = NULL;
	long long_in;
	int t[10];
	
	if (!fl || !emp) {
		return;
	}
	
	// error for later
	sprintf(buf,"SYSERR: Format error in empire logs for #%d (expecting letter, got %s)", EMPIRE_VNUM(emp), line);

	for (;;) {
		if (!get_line(fl, line)) {
			log("%s", buf);
			exit(1);
		}
		switch (*line) {
			case 'L': {	// logs
				if (!get_line(fl, line) || sscanf(line, "%d %d", &t[0], &t[1]) != 2) {
					log("SYSERR: Format error in L line of log file for empire %d", EMPIRE_VNUM(emp));
					exit(1);
				}
				
				CREATE(elog, struct empire_log_data, 1);
				elog->type = t[0];
				elog->timestamp = (time_t) t[1];
				elog->string = fread_string(fl, buf2);
				elog->next = NULL;
				
				// append to end to preserve original order
				if (last_log) {
					last_log->next = elog;
				}
				else {
					emp->logs = elog;
				}
				last_log = elog;
				break;
			}
			case 'W': {	// offenses
				if (sscanf(line, "W %d %d %d %ld %d %d %s", &t[0], &t[1], &t[2], &long_in, &t[4], &t[5], str_in) != 7) {
					log("SYSERR: W line of empire %d does not scan (ignoring).\r\n", emp->vnum);
				}
				
				CREATE(off, struct offense_data, 1);
				off->type = t[0];
				off->empire = t[1];
				off->player_id = t[2];
				off->timestamp = long_in;
				off->x = t[4];
				off->y = t[5];
				off->flags = asciiflag_conv(str_in);
				
				if (last_off) {
					last_off->next = off;
				}
				else {
					EMPIRE_OFFENSES(emp) = off;
				}
				
				last_off = off;
				break;
			}

			case 'S': {	// fin
				return;
			}
			default: {
				log("%s", buf);
				exit(1);
			}
		}
	}
}


/**
* Delayed-load of storage for an empire.
*
* @param FILE *fl The open read file.
* @param empire_data *emp The empire to assign the storage to.
*/
void load_empire_storage_one(FILE *fl, empire_data *emp) {	
	int t[10], junk;
	long l_in;
	char line[1024], str_in[256], buf[MAX_STRING_LENGTH];
	struct empire_unique_storage *eus, *last_eus = NULL;
	struct shipping_data *shipd, *last_shipd = NULL;
	struct empire_storage_data *store;
	struct theft_log *tft;
	obj_data *obj, *proto;
	
	if (!fl || !emp) {
		return;
	}
	
	// error for later
	sprintf(buf,"SYSERR: Format error in empire storage for #%d (expecting letter, got %s)", EMPIRE_VNUM(emp), line);

	for (;;) {
		if (!get_line(fl, line)) {
			log("%s", buf);
			exit(1);
		}
		switch (*line) {
			case 'O': {	// storage
				if (!get_line(fl, line)) {
					log("SYSERR: Storage data for empire %d was incomplete", EMPIRE_VNUM(emp));
					exit(1);
				}
				if (sscanf(line, "%d %d %d %d", &t[0], &t[1], &t[2], &t[3]) != 4) {
					t[3] = 0;	// !keep
					if (sscanf(line, "%d %d %d", &t[0], &t[1], &t[2]) != 3) {
						log("SYSERR: Bad storage data for empire %d", EMPIRE_VNUM(emp));
						exit(1);
					}
				}
				
				// validate vnum
				proto = obj_proto(t[0]);
				if (proto && proto->storage) {
					add_to_empire_storage(emp, t[2], t[0], t[1]);
					
					// check keep
					if (t[3] && (store = find_stored_resource(emp, t[2], t[0]))) {
						store->keep = t[3];
					}
				}
				else if (proto && !proto->storage) {
					log("- removing %dx #%d from empire storage for %s: not storable", t[1], t[0], EMPIRE_NAME(emp));
				}
				else {
					log("- removing %dx #%d from empire storage for %s: no such object", t[1], t[0], EMPIRE_NAME(emp));
				}
				break;
			}
			case 'T': {	// theft logs
				if (sscanf(line, "T %d %d %ld", &t[0], &t[1], &l_in) != 3) {
					log("SYSERR: Invalid T line of empire %d: %s", EMPIRE_VNUM(emp), line);
					exit(0);
				}
				
				CREATE(tft, struct theft_log, 1);
				tft->vnum = t[0];
				tft->amount = t[1];
				tft->time_minutes = l_in;
				LL_APPEND(EMPIRE_THEFT_LOGS(emp), tft);
				break;
			}
			case 'U': {	// unique storage
				if (sscanf(line, "U %d %d %s", &t[0], &t[1], str_in) != 3) {
					log("SYSERR: Invalid U line of empire %d: %s", EMPIRE_VNUM(emp), line);
					exit(0);
				}
				
				if (!get_line(fl, line) || sscanf(line, "#%d", &t[2]) != 1) {
					log("SYSERR: Invalid U section of empire %d: no obj", EMPIRE_VNUM(emp));
					// possibly just not fatal, if next line give sno problems
					continue;
				}
				
				obj = Obj_load_from_file(fl, t[2], &junk, NULL);
				if (obj) {
					remove_from_object_list(obj);	// doesn't really go here right now
					
					CREATE(eus, struct empire_unique_storage, 1);
					eus->next = NULL;
					eus->island = t[0];
					eus->amount = t[1];
					eus->flags = asciiflag_conv(str_in);
					eus->obj = obj;
					
					if (last_eus) {
						last_eus->next = eus;
					}
					else {
						EMPIRE_UNIQUE_STORAGE(emp) = eus;
					}
					last_eus = eus;
				}
				
				break;
			}
			case 'V': {	// shipments
				if (sscanf(line, "V %d %d %d %d %d %ld %d %d", &t[0], &t[1], &t[2], &t[3], &t[4], &l_in, &t[5], &t[6]) != 8) {
					log("SYSERR: Invalid V line of empire %d: %s", EMPIRE_VNUM(emp), line);
					exit(0);
				}
				
				if (obj_proto(t[0])) {
					CREATE(shipd, struct shipping_data, 1);
					shipd->vnum = t[0];
					shipd->amount = t[1];
					shipd->from_island = t[2];
					shipd->to_island = t[3];
					shipd->status = t[4];
					shipd->status_time = l_in;
					shipd->shipping_id = t[5];
					shipd->ship_origin = t[6];
					shipd->next = NULL;
				
					EMPIRE_TOP_SHIPPING_ID(emp) = MAX(shipd->shipping_id, EMPIRE_TOP_SHIPPING_ID(emp));

					// append to end
					if (last_shipd) {
						last_shipd->next = shipd;
					}
					else {
						EMPIRE_SHIPPING_LIST(emp) = shipd;
					}
					last_shipd = shipd;
				}
				// else: don't bother warning, just drop it if the obj doesn't exist
				break;
			}

			case 'S': {	// fin
				return;
			}
			default: {
				log("%s", buf);
				exit(1);
			}
		}
	}
}


/**
* Load the storage and log files for all empires (called at startup).
*/
void load_empire_storage(void) {
	char fname[MAX_STRING_LENGTH];
	empire_data *emp, *next_emp;
	FILE *fl;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		emp->storage_loaded = TRUE;	// safe to save now (whether or not it loads one)
		emp->logs_loaded = TRUE;
		
		// storage file
		sprintf(fname, "%s%d%s", STORAGE_PREFIX, EMPIRE_VNUM(emp), EMPIRE_SUFFIX);
		if (!(fl = fopen(fname, "r"))) {
			// it's not considered critical to lack this file
			log("Unable to open einv file for empire %d %s", EMPIRE_VNUM(emp), EMPIRE_NAME(emp));
			continue;
		}
		load_empire_storage_one(fl, emp);
		fclose(fl);
		
		// logs file
		sprintf(fname, "%s%d%s", ELOG_PREFIX, EMPIRE_VNUM(emp), EMPIRE_SUFFIX);
		if ((fl = fopen(fname, "r"))) {
			// it's not considered critical to lack this file
			load_empire_logs_one(fl, emp);
			fclose(fl);
		}
	}
}


/**
* Parse one empire entry from file into the empire_table.
*
* @param FILE *fl The open file.
* @param empire_vnum vnum The vnum to process.
*/
void parse_empire(FILE *fl, empire_vnum vnum) {
	void assign_old_workforce_chore(empire_data *emp, int chore);
	extern struct empire_city_data *create_city_entry(empire_data *emp, char *name, room_data *location, int type);
	extern struct empire_npc_data *create_empire_npc(empire_data *emp, mob_vnum mob, int sex, int name, struct empire_territory_data *ter);
	extern struct empire_territory_data *create_territory_entry(empire_data *emp, room_data *room);
	
	empire_data *emp, *find;
	int t[6], j, iter;
	char line[1024], str_in[256];
	struct empire_political_data *emp_pol;
	struct empire_territory_data *ter;
	struct empire_trade_data *trade, *last_trade = NULL;
	struct empire_goal *egoal, *last_egoal = NULL;
	struct empire_completed_goal *ecg;
	struct player_craft_data *pcd;
	struct empire_log_data *elog;
	struct empire_needs *need;
	struct offense_data *off;
	struct empire_city_data *city;
	struct empire_island *isle;
	struct req_data *task;
	bitvector_t bit_in;
	room_data *room;
	double dbl_in;
	long long_in;
	char *tmp, c_in;
	
	sprintf(buf2, "empire #%d", vnum);
	
	// init
	CREATE(emp, empire_data, 1);
	emp->vnum = vnum;

	HASH_FIND_INT(empire_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate empire vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_empire_to_table(emp);
	
	emp->storage_loaded = FALSE;	// block accidental saves
	emp->logs_loaded = FALSE;
	
	emp->name = fread_string(fl, buf2);
	emp->adjective = fread_string(fl, buf2);
	emp->banner = fread_string(fl, buf2);
	EMPIRE_BANNER_HAS_UNDERLINE(emp) = (strstr(EMPIRE_BANNER(emp), "&u") ? TRUE : FALSE);
	
	if (!get_line(fl, line)) {
		log("SYSERR: Expecting ranks type of empire #%d but file ended!", vnum);
		exit(1);
	}

	if (sscanf(line, "%d %ld %d %s", &t[0], &long_in, &t[1], str_in) != 4) {
		// backwards-compatibility if no flags
		strcpy(str_in, "0");
		if (sscanf(line, "%d %ld %d", &t[0], &long_in, &t[1]) != 3) {
			log("SYSERR: Format error in data line of empire #%d", vnum);
			exit(1);
		}
	}

	emp->leader = t[0];
	emp->create_time = long_in;
	emp->num_ranks = t[1];
	emp->admin_flags = asciiflag_conv(str_in);
	
	emp->members = 0;
	emp->greatness = 0;
	for (iter = 0; iter < NUM_TECHS; ++iter) {
		emp->tech[iter] = 0;
	}
	emp->last_logon = 0;
	emp->population = 0;
	emp->military = 0;
	emp->territory_list = NULL;
	emp->city_list = NULL;
	
	// initialize safely
	for (iter = 0; iter < NUM_PRIVILEGES; ++iter) {
		emp->priv[iter] = emp->num_ranks;
	}

	sprintf(buf,"SYSERR: Format error in empire #%d (expecting letter, got %s)", vnum, line);

	for (;;) {
		if (!get_line(fl, line)) {
			log("%s", buf);
			exit(1);
		}

		switch (*line) {
			case 'A': {	// description
				EMPIRE_DESCRIPTION(emp) = fread_string(fl, buf2);
				break;
			}
			case 'B': {	// misc data
				if (sscanf(line, "B %d %ld", &t[0], &long_in) != 2) {
					log("SYSERR: Bad format in B tag of empire %d!", vnum);
					exit(1);
				}
				
				switch (t[0]) {
					case 1: {
						EMPIRE_CITY_OVERAGE_WARNING_TIME(emp) = long_in;
						break;
					}
					default: {
						log("SYSERR: Bad data type %d in B tag of empire %d!", t[0], vnum);
						exit(1);
					}
				}
				
				break;
			}
			case 'C': { // chore
				if (sscanf(line, "C %d %d %d", &t[0], &t[1], &t[2]) == 3) {
					if (t[1] >= 0 && t[1] < NUM_CHORES && (isle = get_empire_island(emp, t[0]))) {
						isle->workforce_limit[t[1]] = t[2];
					}
				}
				else if (sscanf(line, "C%d", &t[0]) == 1) {
					// old version
					assign_old_workforce_chore(emp, t[0]);
				}
				else {
					log("SYSERR: Bad chore data for empire %d", vnum);
					exit(1);
				}
				break;
			}
			case 'D': {	// diplomacy data
				if (!get_line(fl, line)) {
					log("SYSERR: Expecting political data for empire %d, but file ended!", vnum);
					exit(1);
				}
				sscanf(line, "%d %d %d %d", t, t + 1, t + 2, t + 3);
				CREATE(emp_pol, struct empire_political_data, 1);
				emp_pol->id = t[0];
				emp_pol->type = t[1];
				emp_pol->offer = t[2];
				emp_pol->start_time = t[3];
				LL_APPEND(emp->diplomacy, emp_pol);
				break;
			}
			case 'E': {	// extra data
				// frontier traits, coins
				if (!get_line(fl, line)) {
					log("SYSERR: Expected numerical data in E line of empire %d but file ended", vnum);
					break;
				}
				if (sscanf(line, "%s %lf", str_in, &dbl_in) != 2) {
					log("SYSERR: Expected 2 args in E line of empire %d", vnum);
					break;
				}
				
				emp->frontier_traits = asciiflag_conv(str_in);
				emp->coins = dbl_in;
				break;
			}
			case 'F': {	// base tech
				if (sscanf(line, "F %d %d", &t[0], &t[1]) != 2) {
					log("SYSERR: Bad format in F tag of empire %d!", vnum);
					exit(1);
				}
				
				if (t[0] >= 0 && t[0] < NUM_TECHS) {
					EMPIRE_BASE_TECH(emp, t[0]) = t[1];
				}
				break;
			}
			case 'G': {	// goals (sub-divided by a 2nd letter)
				switch (*(line + 1)) {
					case 'C': {	// GC: completed
						if (sscanf(line, "GC %d %ld", &t[0], &long_in) != 2) {
							log("SYSERR: Format error in GC line of empire %d", vnum);
							// non-fatal
							break;
						}
						
						HASH_FIND_INT(EMPIRE_COMPLETED_GOALS(emp), &t[0], ecg);
						if (!ecg) {
							CREATE(ecg, struct empire_completed_goal, 1);
							ecg->vnum = t[0];
							HASH_ADD_INT(EMPIRE_COMPLETED_GOALS(emp), vnum, ecg);
						}
						ecg->when = long_in;
						break;
					}
					case 'G': {	// GG: goal in progress
						if (sscanf(line, "GG %d %d %ld", &t[0], &t[1], &long_in) != 3) {
							long_in = time(0);	// backwards-compatible
							if (sscanf(line, "GG %d %d", &t[0], &t[1]) != 2) {
								log("SYSERR: Format error in GG line of empire %d", vnum);
								// fatal because it could mess up trackers
								exit(1);
							}
						}
						
						HASH_FIND_INT(EMPIRE_GOALS(emp), &t[0], egoal);
						if (!egoal) {
							CREATE(egoal, struct empire_goal, 1);
							egoal->vnum = t[0];
							HASH_ADD_INT(EMPIRE_GOALS(emp), vnum, egoal);
						}
						egoal->version = t[1];
						egoal->timestamp = long_in;
						
						last_egoal = egoal;
						break;
					}
					case 'L': {	// GL: learned crafts
						if (sscanf(line, "GL %d %d", &t[0], &t[1]) != 2) {
							log("SYSERR: Format error in GL line of empire %d", vnum);
							// not fatal
							break;
						}
						
						if (t[1] > 0) {
							HASH_FIND_INT(EMPIRE_LEARNED_CRAFTS(emp), &t[0], pcd);
							if (!pcd) {
								CREATE(pcd, struct player_craft_data, 1);
								pcd->vnum = t[0];
								pcd->count = t[1];
								HASH_ADD_INT(EMPIRE_LEARNED_CRAFTS(emp), vnum, pcd);
							}
						}
						break;
					}
					case 'P': {	// GP: goal points (progress points)
						if (sscanf(line, "GP %d %d", &t[0], &t[1]) != 2) {
							log("SYSERR: Format error in GP line of empire %d", vnum);
							// non-fatal
							break;
						}
						
						if (t[0] >= 0 && t[0] < NUM_PROGRESS_TYPES) {
							EMPIRE_PROGRESS_POINTS(emp, t[0]) = t[1];
						}
						break;
					}
					case 'T': {	// GT: tracker for last goal
						if (last_egoal && sscanf(line, "GT %d %d %lld %d %d %c", &t[0], &t[1], &bit_in, &t[2], &t[3], &c_in) == 6) {
							// found group
						}
						else if (last_egoal && sscanf(line, "GT %d %d %lld %d %d", &t[0], &t[1], &bit_in, &t[2], &t[3]) == 5) {
							c_in = 0;	// no group given
						}
						else {
							log("SYSERR: Format error in GT line of empire %d", vnum);
							// bad format but not fatal
							if (last_egoal) {
								--last_egoal->version;	// forces a refresh by putting the version out of date
							}
							break;
						}
					
						CREATE(task, struct req_data, 1);
						task->type = t[0];
						task->vnum = t[1];
						task->misc = bit_in;
						task->needed = t[2];
						task->current = t[3];
						task->group = c_in;
					
						LL_APPEND(last_egoal->tracker, task);
						break;
					}
					default: {
						log("SYSERR: Unknown G line in empire %d: %s", vnum, line);
						// safe to continue though
						break;
					}
				}
				break;
			}
			case 'I': {	// island name
				if ((isle = get_empire_island(emp, atoi(line+1)))) {
					isle->name = fread_string(fl, buf2);
				}
				break;
			}
			case 'J': {	// island data
				if (sscanf(line, "J %d %d %d %s", &t[0], &t[1], &t[2], str_in) != 4) {
					log("SYSERR: Expected 4 args in J line of empire %d", vnum);
					exit(0);
				}
				
				// find or add
				need = get_empire_needs(emp, t[0], t[1]);
				
				need->needed = t[2];
				need->status = asciiflag_conv(str_in);
				
				break;
			}
			case 'L': {	// NOTE: logs are here for backwards-compatibility -- they are now loaded by load_empire_logs_one
				if (!get_line(fl, line) || sscanf(line, "%d %d", &t[0], &t[1]) != 2) {
					log("SYSERR: Format error in L line of empire %d", vnum);
					exit(1);
				}
				
				CREATE(elog, struct empire_log_data, 1);
				elog->type = t[0];
				elog->timestamp = (time_t) t[1];
				elog->string = fread_string(fl, buf2);
				LL_APPEND(emp->logs, elog);
				break;
			}
			case 'M': {	// motd
				EMPIRE_MOTD(emp) = fread_string(fl, buf2);
				break;
			}
			case 'P': {	// privilege value
				j = atoi(line+1);
				if (!get_line(fl, line)) {
					log("SYSERR: Expecting privilege number for empire %d, but file ended!", vnum);
					exit(1);
					}
				sscanf(line, "%d", t);
				emp->priv[j] = MAX(1, MIN(emp->num_ranks, t[0]));
				break;
			}
			case 'R': {	// rank name
				j = atoi(line+1);
				if (j < 20)
					emp->rank[j] = fread_string(fl, buf2);
				else {
					log("Invalid rank %d in empire #%d", j, vnum);
					exit(1);
				}
				break;
			}
			case 'N': {	// npc
				if (sscanf(line, "N %d %d %d %d", t, t+1, t+2, t+3) == 4) {
					if ((room = real_room(t[3]))) {
						if (!(ter = find_territory_entry(emp, room))) {
							ter = create_territory_entry(emp, room);
						}
					
						// this attaches itself to the room
						create_empire_npc(emp, t[0], t[1], t[2], ter);
					}
				}
				else {
					log("SYSERR: NPC line of empire %d does not scan.\r\n", emp->vnum);
				}
				break;
			}
			case 'T': {	// territory
				if (sscanf(line, "T %d %d", t, t + 1) == 2) {
					if ((room = real_room(t[0]))) {
						if (!(ter = find_territory_entry(emp, room))) {
							ter = create_territory_entry(emp, room);
						}
						ter->population_timer = t[1];
					}
				}
				else {
					log("SYSERR: Territory line of empire %d does not scan.\r\n", emp->vnum);
				}
				break;
			}
			case 'W': {	// NOTE: offenses are here for backwards-compatibility -- they are now loaded by load_empire_logs_one
				if (sscanf(line, "W %d %d %d %ld %d %d %s", &t[0], &t[1], &t[2], &long_in, &t[4], &t[5], str_in) != 7) {
					log("SYSERR: W line of empire %d does not scan (ignoring).\r\n", emp->vnum);
				}
				
				CREATE(off, struct offense_data, 1);
				off->type = t[0];
				off->empire = t[1];
				off->player_id = t[2];
				off->timestamp = long_in;
				off->x = t[4];
				off->y = t[5];
				off->flags = asciiflag_conv(str_in);
				LL_APPEND(EMPIRE_OFFENSES(emp), off);
				break;
			}
			case 'X': { // trade
				if (sscanf(line, "X %d %d %d %lf", &t[0], &t[1], &t[2], &dbl_in) == 4) {
					CREATE(trade, struct empire_trade_data, 1);
					trade->type = t[0];
					trade->vnum = t[1];
					trade->limit = t[2];
					trade->cost = dbl_in;
					trade->next = NULL;
					
					// add to end
					if (last_trade) {
						last_trade->next = trade;
					}
					else {
						emp->trade = trade;
					}
					last_trade = trade;
				}
				else {
					log("SYSERR: X line of empire %d does not scan.\r\n", emp->vnum);
				}
				break;
			}
			case 'Y': { // city				
				if (sscanf(line, "Y %d %d %s", &t[0], &t[1], str_in) == 3) {
					tmp = fread_string(fl, buf2);
					
					if ((room = real_room(t[0])) && IS_CITY_CENTER(room)) {
						city = create_city_entry(emp, tmp, room, t[1]);
						city->traits = asciiflag_conv(str_in);
					}
					else if (room) {
						// no city center -- just lose the city
						log_to_empire(emp, ELOG_TERRITORY, "%s was lost", tmp);
					}
					
					free(tmp);	// was duplicated
				}
				else {
					log("SYSERR: Format error in city line of empire %d", emp->vnum);
				}
				
				break;
			}
			case 'Z': { // attribute
				if (sscanf(line, "Z%d %d", &t[0], &t[1]) != 2) {
					log("SYSERR: Format error in Z line of empire %d.\r\n", emp->vnum);
					exit(1);
				}
				
				if (t[0] >= 0 && t[0] < NUM_EMPIRE_ATTRIBUTES) {
					EMPIRE_ATTRIBUTE(emp, t[0]) = t[1];
				}
				break;
			}

			case 'S':			/* end of empire */
				return;
			default: {
				log("%s", buf);
				exit(1);
			}
		}
	}
}


// for check_for_new_map
PLAYER_UPDATE_FUNC(send_all_players_to_nowhere) {
	// clear these rooms
	GET_LAST_ROOM(ch) = NOWHERE;
	GET_LOADROOM(ch) = NOWHERE;
	GET_LOAD_ROOM_CHECK(ch) = NOWHERE;
	GET_MARK_LOCATION(ch) = NOWHERE;
	GET_TOMB_ROOM(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_RETURN_MAP(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_INSTANCE_ID(ch) = NOTHING;
	
	// remove loadroom flag (their loadroom is gone)
	REMOVE_BIT(PLR_FLAGS(ch), PLR_LOADROOM);
}


/**
* Writes one empire entry to file.
*
* @param FILE *fl The open write file.
* @param empire_data *emp The empire to save.
*/
void write_empire_to_file(FILE *fl, empire_data *emp) {
	struct empire_island *isle, *next_isle;
	struct empire_political_data *emp_pol;
	struct empire_territory_data *ter, *next_ter;
	struct empire_completed_goal *ecg, *next_ecg;
	struct player_craft_data *pcd, *next_pcd;
	struct empire_goal *egoal, *next_egoal;
	struct empire_needs *need, *next_need;
	struct empire_trade_data *trade;
	struct empire_city_data *city;
	struct empire_npc_data *npc;
	struct req_data *task;
	int iter;

	if (!emp) {
		return;
	}

	fprintf(fl, "#%d\n", EMPIRE_VNUM(emp));
	fprintf(fl, "%s~\n", NULLSAFE(EMPIRE_NAME(emp)));
	fprintf(fl, "%s~\n", NULLSAFE(EMPIRE_ADJECTIVE(emp)));
	fprintf(fl, "%s~\n", NULLSAFE(EMPIRE_BANNER(emp)));
	fprintf(fl, "%d %ld %d %s\n", EMPIRE_LEADER(emp), EMPIRE_CREATE_TIME(emp), EMPIRE_NUM_RANKS(emp), bitv_to_alpha(EMPIRE_ADMIN_FLAGS(emp)));
	
	// A: description
	if (EMPIRE_DESCRIPTION(emp) && *EMPIRE_DESCRIPTION(emp)) {
		char temp[MAX_EMPIRE_DESCRIPTION + 12];
		
		strcpy(temp, EMPIRE_DESCRIPTION(emp));
		strip_crlf(temp);
		fprintf(fl, "A\n%s~\n", temp);
	}
	
	// B: misc data
	if (EMPIRE_CITY_OVERAGE_WARNING_TIME(emp)) {
		fprintf(fl, "B 1 %ld\n", EMPIRE_CITY_OVERAGE_WARNING_TIME(emp));
	}

	// C: chores
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		for (iter = 0; iter < NUM_CHORES; ++iter) {
			if (isle->workforce_limit[iter] != 0) {
				fprintf(fl, "C %d %d %d\n", isle->island, iter, isle->workforce_limit[iter]);
			}
		}
	}
	
	// D: diplomacy
	for (emp_pol = EMPIRE_DIPLOMACY(emp); emp_pol; emp_pol = emp_pol->next) {
		fprintf(fl, "D\n%d %d %d %d\n", emp_pol->id, emp_pol->type, emp_pol->offer, (int) emp_pol->start_time);
	}
	
	// E: extra data
	fprintf(fl, "E\n%s %.1f\n", bitv_to_alpha(EMPIRE_FRONTIER_TRAITS(emp)), EMPIRE_COINS(emp));
	
	// F: base techs
	for (iter = 0; iter < NUM_TECHS; ++iter) {
		if (EMPIRE_BASE_TECH(emp, iter) > 0) {
			fprintf(fl, "F %d %d\n", iter, EMPIRE_BASE_TECH(emp, iter));
		}
	}
	
	// G: progression goals, tasks, and completed
	HASH_ITER(hh, EMPIRE_GOALS(emp), egoal, next_egoal) {
		// GG goal in progress
		fprintf(fl, "GG %d %d %ld\n", egoal->vnum, egoal->version, egoal->timestamp);
		
		// GT goal tracker
		LL_FOREACH(egoal->tracker, task) {
			fprintf(fl, "GT %d %d %lld %d %d %c\n", task->type, task->vnum, task->misc, task->needed, task->current, task->group);
		}
	}
	HASH_ITER(hh, EMPIRE_COMPLETED_GOALS(emp), ecg, next_ecg) {
		// GC completed goal
		fprintf(fl, "GC %d %ld\n", ecg->vnum, ecg->when);
	}
	HASH_ITER(hh, EMPIRE_LEARNED_CRAFTS(emp), pcd, next_pcd) {
		// GL learned crafts
		fprintf(fl, "GL %d %d\n", pcd->vnum, pcd->count);
	}
	for (iter = 0; iter < NUM_PROGRESS_TYPES; ++iter) {
		// GP goal points (progress points)
		if (EMPIRE_PROGRESS_POINTS(emp, iter)) {
			fprintf(fl, "GP %d %d\n", iter, EMPIRE_PROGRESS_POINTS(emp, iter));
		}
	}
	
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		// I: island names
		if (isle->name) {
			fprintf(fl, "I%d\n%s~\n", isle->island, isle->name);
		}
		
		// J: island data
		HASH_ITER(hh, isle->needs, need, next_need) {
			fprintf(fl, "J %d %d %d %s\n", isle->island, need->type, need->needed, bitv_to_alpha(need->status));
		}
	}
	
	// M: MOTD
	if (EMPIRE_MOTD(emp) && *EMPIRE_MOTD(emp)) {
		char temp[MAX_MOTD_LENGTH + 12];
		
		strcpy(temp, EMPIRE_MOTD(emp));
		strip_crlf(temp);
		fprintf(fl, "M\n%s~\n", temp);
	}
	
	// avoid L (used by empire logs)
	// avoid O (used by empire storage)

	// P: privs
	for (iter = 0; iter < NUM_PRIVILEGES; ++iter)
		fprintf(fl, "P%d\n%d\n", iter, EMPIRE_PRIV(emp, iter));
	
	// R: ranks
	for (iter = 0; iter < emp->num_ranks; ++iter)
		fprintf(fl, "R%d\n%s~\n", iter, EMPIRE_RANK(emp, iter));
	
	// T: territory buildings
	HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter, next_ter) {
		fprintf(fl, "T %d %d\n", ter->vnum, ter->population_timer);
	
		// npcs who live there
		for (npc = ter->npcs; npc; npc = npc->next) {
			fprintf(fl, "N %d %d %d %d\n", npc->vnum, npc->sex, npc->name, GET_ROOM_VNUM(npc->home));
		}
	}
	
	// avoid U (used by empire storage)
	// avoid V (used by empire storage)
	// avoid W (used by empire logs)
	
	// X: trade
	for (trade = EMPIRE_TRADE(emp); trade; trade = trade->next) {
		fprintf(fl, "X %d %d %d %.1f\n", trade->type, trade->vnum, trade->limit, trade->cost);
	}
	
	// Y: cities
	for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
		fprintf(fl, "Y %d %d %s\n%s~\n", GET_ROOM_VNUM(city->location), city->type, bitv_to_alpha(city->traits), city->name);
	}
	
	// Z: attributes
	for (iter = 0; iter < NUM_EMPIRE_ATTRIBUTES; ++iter) {
		fprintf(fl, "Z%d %d\n", iter, EMPIRE_ATTRIBUTE(emp, iter));
	}

	fprintf(fl, "S\n");
}


/**
* Writes one empire's log info to file. This is separate to speed up saving.
* This file includes:
* - elogs
* - offenses
*
* @param FILE *fl The open write file.
* @param empire_data *emp The empire whose logs to save.
*/
void write_empire_logs_to_file(FILE *fl, empire_data *emp) {
	struct empire_log_data *elog;
	struct offense_data *off;
	
	if (!emp) {
		return;
	}
	
	// L: logs
	for (elog = EMPIRE_LOGS(emp); elog; elog = elog->next) {
		fprintf(fl, "L\n");
		fprintf(fl, "%d %d\n", elog->type, (int) elog->timestamp);
		fprintf(fl, "%s~\n", elog->string);
	}
	
	// W: offenses
	LL_FOREACH(EMPIRE_OFFENSES(emp), off) {
		fprintf(fl, "W %d %d %d %ld %d %d %s\n", off->type, off->empire, off->player_id, off->timestamp, off->x, off->y, bitv_to_alpha(off->flags));
	}
	
	fprintf(fl, "S\n");
}


/**
* Writes one empire's storage info to file. This is separate because stoage
* needs to load AFTER objects.
*
* @param FILE *fl The open write file.
* @param empire_data *emp The empire whose storage to save.
*/
void write_empire_storage_to_file(FILE *fl, empire_data *emp) {	
	struct empire_storage_data *store, *next_store;
	struct empire_island *isle, *next_isle;
	struct empire_unique_storage *eus;
	struct shipping_data *shipd;
	struct theft_log *tft;

	if (!emp) {
		return;
	}
	
	// islands
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		// O: storage
		HASH_ITER(hh, isle->store, store, next_store) {
			fprintf(fl, "O\n%d %d %d %d\n", store->vnum, store->amount, isle->island, store->keep);
		}
	}
	
	// T: theft logs
	LL_FOREACH(EMPIRE_THEFT_LOGS(emp), tft) {
		fprintf(fl, "T %d %d %ld\n", tft->vnum, tft->amount, tft->time_minutes);
	}

	// U: unique storage
	for (eus = EMPIRE_UNIQUE_STORAGE(emp); eus; eus = eus->next) {
		// wut?
		if (eus->amount <= 0 || !eus->obj) {
			continue;
		}
		fprintf(fl, "U %d %d %s\n", eus->island, eus->amount, bitv_to_alpha(eus->flags));
		Crash_save_one_obj_to_file(fl, eus->obj, 0);
	}
	
	// V: shipments
	for (shipd = EMPIRE_SHIPPING_LIST(emp); shipd; shipd = shipd->next) {
		fprintf(fl, "V %d %d %d %d %d %ld %d %d\n", shipd->vnum, shipd->amount, shipd->from_island, shipd->to_island, shipd->status, shipd->status_time, shipd->shipping_id, shipd->ship_origin);
	}

	fprintf(fl, "S\n");
}


/**
* Writes the logs file for the empire.
*
* @param empire_data *emp The empire to save.
*/
void save_empire_logs(empire_data *emp) {
	char fname[30], tempname[64];
	FILE *fl;

	// empire logs: only if it's already been loaded (saves may trigger sooner)
	if (!emp || !emp->logs_loaded) {
		return;
	}
	
	sprintf(fname, "%s%d%s", ELOG_PREFIX, EMPIRE_VNUM(emp), EMPIRE_SUFFIX);
	strcpy(tempname, fname);
	strcat(tempname, TEMP_SUFFIX);
	if (!(fl = fopen(tempname, "w"))) {
		log("SYSERR: Unable to write %s", tempname);
		return;
	}
	write_empire_logs_to_file(fl, emp);
	fprintf(fl, "$~\n");
	fclose(fl);
	rename(tempname, fname);
	
	EMPIRE_NEEDS_LOGS_SAVE(emp) = FALSE;
}


/**
* Writes the storage file for the empire.
*
* @param empire_data *emp The empire to save.
*/
void save_empire_storage(empire_data *emp) {
	FILE *fl;
	char fname[30], tempname[64];

	// empire storage: only if it's already been loaded (saves may trigger sooner)
	if (!emp || !emp->storage_loaded) {
		return;
	}
	
	sprintf(fname, "%s%d%s", STORAGE_PREFIX, EMPIRE_VNUM(emp), EMPIRE_SUFFIX);
	strcpy(tempname, fname);
	strcat(tempname, TEMP_SUFFIX);
	if (!(fl = fopen(tempname, "w"))) {
		log("SYSERR: Unable to write %s", tempname);
		return;
	}
	write_empire_storage_to_file(fl, emp);
	fprintf(fl, "$~\n");
	fclose(fl);
	rename(tempname, fname);
	
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = FALSE;
}


/**
* Saves an empire to file.
*
* @param empire_data *emp The empire to save.
* @param bool save_all_parts If TRUE, also saves storage file, etc.
*/
void save_empire(empire_data *emp, bool save_all_parts) {
	FILE *fl;
	char fname[30], tempname[64];

	if (!emp) {
		return;
	}

	// main empire file
	sprintf(fname, "%s%d%s", LIB_EMPIRE, EMPIRE_VNUM(emp), EMPIRE_SUFFIX);
	strcpy(tempname, fname);
	strcat(tempname, TEMP_SUFFIX);
	if (!(fl = fopen(tempname, "w"))) {
		log("SYSERR: Unable to write %s", tempname);
		return;
	}
	write_empire_to_file(fl, emp);
	fprintf(fl, "$~\n");
	fclose(fl);
	rename(tempname, fname);
	
	EMPIRE_NEEDS_SAVE(emp) = FALSE;	// done
	
	if (save_all_parts) {
		save_empire_storage(emp);
		save_empire_logs(emp);
	}
}


/**
* Saves all empires.
*/
void save_all_empires(void) {
	empire_data *iter, *next_iter;

	HASH_ITER(hh, empire_table, iter, next_iter) {
		save_empire(iter, TRUE);
	}
}


/**
* Delayed empire saves -- things marked as needing saves.
*/
void save_marked_empires(void) {
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_NEEDS_SAVE(emp)) {
			save_empire(emp, FALSE);
		}
		if (EMPIRE_NEEDS_STORAGE_SAVE(emp)) {
			save_empire_storage(emp);
		}
		if (EMPIRE_NEEDS_LOGS_SAVE(emp)) {
			save_empire_logs(emp);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE NPC LIB //////////////////////////////////////////////////////////

/**
* Creates an npc entry and associates it to the room
*
* @param empire_data *emp the empire
* @param mob_vnum mobv The vnum of the mob
* @param int sex SEX_x
* @param int name pos in the name list
* @param struct empire_territory_data *ter Where the mob lives
* @return struct empire_npc_data* pointer to the npc obj
*/
struct empire_npc_data *create_empire_npc(empire_data *emp, mob_vnum mobv, int sex, int name, struct empire_territory_data *ter) {
	struct empire_npc_data *npc;
	
	CREATE(npc, struct empire_npc_data, 1);
	npc->vnum = mobv;
	npc->sex = sex;
	npc->name = name;
	npc->home = ter->room;
	
	npc->mob = NULL;
	
	npc->empire_id = EMPIRE_VNUM(emp);
	
	npc->next = ter->npcs;
	ter->npcs = npc;
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
	
	return npc;
}


/**
* frees up the data for a room that may have npcs.
*
* You only need to provide one argument or the other.
*
* @param room_data *room The room to clean up (optional)
* @param struct empire_territory_data *ter the territory to clean up (optional)
*/
void delete_room_npcs(room_data *room, struct empire_territory_data *ter) {
	struct empire_territory_data *tt = ter;
	room_data *loc = (room ? room : ter->room);
	empire_data *emp;

	if (!loc || !ROOM_OWNER(loc)) {
		return;
	}

	emp = ROOM_OWNER(loc);
	
	// not passed in as ter?
	if (!tt && emp) {
		tt = find_territory_entry(emp, loc);
	}
	
	if (tt) {
		while (tt->npcs) {
			delete_territory_npc(tt, tt->npcs);
		}
	}
}


/**
* Causes a room to populate (NPC moves in), if possible.
*
* @param room_data *room The location to populate.
* @param struct empire_territory_data *ter The territory entry, if you already have it (will attempt to detect otherwise).
*/
void populate_npc(room_data *room, struct empire_territory_data *ter) {
	extern int pick_generic_name(int name_set, int sex);
	extern char_data *spawn_empire_npc_to_room(empire_data *emp, struct empire_npc_data *npc, room_data *room, mob_vnum override_mob);
	
	struct empire_npc_data *npc;
	struct empire_island *isle;
	bool found_artisan = FALSE;
	int count, max, sex;
	empire_data *emp;
	char_data *proto;
	mob_vnum artisan;
	
	if (!room || !(emp = ROOM_OWNER(room)) || !EMPIRE_HAS_TECH(emp, TECH_CITIZENS) || (!ter && !(ter = find_territory_entry(emp, room)))) {
		return;	// no work
	}
	if (!IS_COMPLETE(room) || ROOM_PRIVATE_OWNER(HOME_ROOM(room)) != NOBODY) {
		return;	// nobody populates here
	}
	
	// reset timer
	ter->population_timer = config_get_int("building_population_timer");
	
	// detect max npcs
	if (GET_BUILDING(room)) {
		max = GET_BLD_CITIZENS(GET_BUILDING(room));
		artisan = GET_BLD_ARTISAN(GET_BUILDING(room));
	}
	else {
		max = 0;
		artisan = NOTHING;
	}
	
	// check npcs living here
	for (count = 0, npc = ter->npcs; npc; npc = npc->next) {
		++count;
		if (npc->vnum == artisan) {
			found_artisan = TRUE;
		}
	}
	
	// further processing only if we're short npcs here
	if (artisan != NOTHING && !found_artisan) {
		sex = number(SEX_MALE, SEX_FEMALE);
		proto = mob_proto(artisan);
		npc = create_empire_npc(emp, artisan, sex, pick_generic_name(proto ? MOB_NAME_SET(proto) : 0, sex), ter);
		EMPIRE_POPULATION(emp) += 1;
		if (!GET_ROOM_VEHICLE(room) && (isle = get_empire_island(emp, GET_ISLAND_ID(room)))) {
			isle->population += 1;
		}
		
		// spawn it right away if anybody is in the room
		if (ROOM_PEOPLE(room)) {
			spawn_empire_npc_to_room(emp, npc, ter->room, NOTHING);
		}
		
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
	else if (count < max) {
		sex = number(SEX_MALE, SEX_FEMALE);
		proto = mob_proto(sex == SEX_MALE ? CITIZEN_MALE : CITIZEN_FEMALE);
		npc = create_empire_npc(emp, proto ? GET_MOB_VNUM(proto) : 0, sex, pick_generic_name(MOB_NAME_SET(proto), sex), ter);
		EMPIRE_POPULATION(emp) += 1;
		if (!GET_ROOM_VEHICLE(room) && (isle = get_empire_island(emp, GET_ISLAND_ID(room)))) {
			isle->population += 1;
		}

		// spawn it right away if anybody is in the room
		if (ROOM_PEOPLE(room)) {
			spawn_empire_npc_to_room(emp, npc, room, NOTHING);
		}
		
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}


/**
* This updates buildings and assigns new NPCs to them as-needed.
*/
void update_empire_npc_data(void) {	
	struct empire_territory_data *ter, *next_ter;
	empire_data *emp, *next_emp;
	
	int time_to_empire_emptiness = config_get_int("time_to_empire_emptiness") * SECS_PER_REAL_WEEK;
	
	// each empire
	HASH_ITER(hh, empire_table, emp, next_emp) {
		// skip idle empires: TODO could macro this
		if (EMPIRE_LAST_LOGON(emp) + time_to_empire_emptiness < time(0)) {
			continue;
		}
		
		// each territory spot
		HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter, next_ter) {
			if (--ter->population_timer <= 0) {
				populate_npc(ter->room, ter);
			}
		}
	}
}


/**
* Loads an npc, spawns them to the room, and returns the mob
*
* @param empire_data *emp The empire it belongs to
* @param struct empire_npc_data *npc The npc data
* @param room_data *room The room to spawn to
* @param mob_vnum override_mob Overrides the default vnum with this, for workers -- NOTHING to not override
* @return char_data *the mob
*/
char_data *spawn_empire_npc_to_room(empire_data *emp, struct empire_npc_data *npc, room_data *room, mob_vnum override_mob) {
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	char_data *mob;
	
	mob = read_mobile((override_mob == NOTHING) ? npc->vnum : override_mob, TRUE);
	
	GET_EMPIRE_NPC_DATA(mob) = npc;
	npc->mob = mob;

	// guarantee empire flag
	setup_generic_npc(mob, emp, npc->name, npc->sex);
	
	// spawn data
	SET_BIT(MOB_FLAGS(mob), MOB_SPAWNED | MOB_EMPIRE);
	GET_LOYALTY(mob) = emp;
	
	// update spawn time
	ROOM_LAST_SPAWN_TIME(room) = time(0);

	// put in the room
	char_to_room(mob, room);
	act("$n arrives.", TRUE, mob, NULL, NULL, TO_ROOM);
	load_mtrigger(mob);
	
	return mob;
}


/**
* This deletes an empire npc from the empire data on death.
*
* @param char_data *ch The creature who is dying
*/
void kill_empire_npc(char_data *ch) {
	struct empire_territory_data *ter, *ter_next;
	struct empire_npc_data *npc, *npc_next;
	empire_data *emp;
	bool found = FALSE;
	
	int building_population_timer = config_get_int("building_population_timer");

	// check if this was an empire npc
	if (!IS_NPC(ch) || !GET_EMPIRE_NPC_DATA(ch) || !(emp = real_empire(GET_EMPIRE_NPC_DATA(ch)->empire_id))) {
		return;
	}
	
	// find and remove the entry
	HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter, ter_next) {
		for (npc = ter->npcs; npc && !found; npc = npc_next) {
			npc_next = npc->next;
			
			if (npc == GET_EMPIRE_NPC_DATA(ch)) {
				delete_territory_npc(ter, npc);
				
				// reset the population timer
				ter->population_timer = building_population_timer;
				found = TRUE;
			}
		}
		
		if (found) {
			break;
		}
	}
	
	GET_EMPIRE_NPC_DATA(ch) = NULL;
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE OFFENSE LIB //////////////////////////////////////////////////////

/**
* Adds a new offense against an empire. Note: the OFF_WAR flag is added auto-
* matically.
*
* @param empire_data *emp The empire being offended.
* @param int type Any OFFENSE_ type.
* @param char_data *offender The person committing the offense (will use any player master of an npc).
* @param room_data *loc Where it happened.
* @param bitvector_t flags Any OFF_ flags that apply.
*/
void add_offense(empire_data *emp, int type, char_data *offender, room_data *loc, bitvector_t flags) {
	void log_offense_to_empire(empire_data *emp, struct offense_data *off, char_data *offender);
	
	struct offense_data *off;
	
	// try to find a player
	while (IS_NPC(offender) && offender->master) {
		offender = offender->master;
	}
	if (IS_NPC(offender) || GET_LOYALTY(offender) == emp) {
		return;	// no offense
	}
	
	CREATE(off, struct offense_data, 1);
	off->type = type;
	off->empire = GET_LOYALTY(offender) ? EMPIRE_VNUM(GET_LOYALTY(offender)) : NOTHING;
	off->player_id = GET_IDNUM(offender);
	off->timestamp = time(0);
	off->x = X_COORD(IN_ROOM(offender));
	off->y = Y_COORD(IN_ROOM(offender));
	off->flags = flags;
	
	// check for war
	if (!IS_SET(flags, OFF_WAR) && GET_LOYALTY(offender) && has_relationship(emp, GET_LOYALTY(offender), DIPL_WAR)) {
		off->flags |= OFF_WAR;
	}
	
	LL_PREPEND(EMPIRE_OFFENSES(emp), off);
	EMPIRE_NEEDS_LOGS_SAVE(emp) = TRUE;
	
	log_offense_to_empire(emp, off, offender);
}


/**
* Marks all offenses from an empire 'avenged'.
*
* @param empire_data *emp The empire to mark (the Avenger).
* @param empire_data *foe The empire whose offenses have been avenged.
* @return int The number of offenses avenged by this.
*/
int avenge_offenses_from_empire(empire_data *emp, empire_data *foe) {
	struct offense_data *off;
	int count = 0;
	
	if (!emp || !foe) {
		return 0;	// no work
	}
	
	LL_FOREACH(EMPIRE_OFFENSES(emp), off) {
		if (off->empire == EMPIRE_VNUM(foe)) {
			SET_BIT(off->flags, OFF_AVENGED);
			++count;
		}
	}
	
	if (count) {
		EMPIRE_NEEDS_LOGS_SAVE(emp) = TRUE;
	}
	
	return count;
}


/**
* Marks all non-empire offenses from a player 'avenged'. These are ONLY solo
* offenses, not ones committed while in an empire.
*
* @param empire_data *emp The empire to mark (the Avenger).
* @param char_data *foe The person whose offenses were avenged.
* @return int The number of offenses avenged by this.
*/
int avenge_solo_offenses_from_player(empire_data *emp, char_data *foe) {
	struct offense_data *off;
	int count = 0;
	
	if (!emp || !foe || IS_NPC(foe)) {
		return 0;	// no work
	}
	
	LL_FOREACH(EMPIRE_OFFENSES(emp), off) {
		if (off->empire == NOTHING && off->player_id == GET_IDNUM(foe)) {
			SET_BIT(off->flags, OFF_AVENGED);
			++count;
		}
	}
	
	if (count) {
		EMPIRE_NEEDS_LOGS_SAVE(emp) = TRUE;
	}
	
	return count;
}


/**
* Clears out old offenses for all empires.
*/
void clean_empire_offenses(void) {
	time_t clear_older = time(0) - SECS_PER_REAL_WEEK, clear_faster = time(0) - SECS_PER_REAL_DAY;
	struct offense_data *off, *next_off;
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		LL_FOREACH_SAFE(EMPIRE_OFFENSES(emp), off, next_off) {
			if (off->timestamp < clear_older) {
				remove_offense(emp, off);
			}
			else if (!OFFENSE_HAS_WEIGHT(off) && off->timestamp < clear_faster) {
				remove_offense(emp, off);
			}
		}
		
		// don't really need to trigger a save. They'll either save on their
		// own or this will get re-pruned next reboot
	}
}


/**
* Gets the total 'weight' value of all current offenses by an empire.
*
* @param empire_data *emp The empire to check (who was offended).
* @param empire_data *foe Which enemy did the offending.
* @return int The total weight of all offenses.
*/
int get_total_offenses_from_empire(empire_data *emp, empire_data *foe) {
	struct offense_data *off;
	int total = 0;
	
	if (!emp || !foe) {
		return 0;	// shortcut
	}
	
	LL_FOREACH(EMPIRE_OFFENSES(emp), off) {
		if (!OFFENSE_HAS_WEIGHT(off)) {
			continue;	// some don't count
		}
		if (off->empire != EMPIRE_VNUM(foe)) {
			continue;	// wrong empire
		}
		
		// ok
		total += offense_info[off->type].weight;
	}
	
	return total;
}


/**
* Gets the total 'weight' value of all current offenses by a player.
*
* @param empire_data *emp The empire to check (who was offended).
* @param char_data *ch The player who did the offending.
* @return int The total weight of all offenses.
*/
int get_total_offenses_from_char(empire_data *emp, char_data *ch) {
	struct offense_data *off;
	int total = 0;
	
	if (!emp || !ch || IS_NPC(ch)) {
		return 0;	// shortcut
	}
	
	LL_FOREACH(EMPIRE_OFFENSES(emp), off) {
		if (!OFFENSE_HAS_WEIGHT(off)) {
			continue;	// some don't count
		}
		if (off->player_id != GET_IDNUM(ch)) {
			continue;	// wrong person
		}
		
		// ok
		total += offense_info[off->type].weight;
	}
	
	return total;
}


/**
* If offenses should elog, this function builds and logs that.
*
* @param empire_data *emp The empire that was offended.
* @param struct offense_data *off The offense to log.
* @param char_data *offender The person who committed the offense.
*/
void log_offense_to_empire(empire_data *emp, struct offense_data *off, char_data *offender) {
	char buf[MAX_STRING_LENGTH];
	*buf = '\0';
	
	// OFFENSE_x: offenses that must be logged
	switch (off->type) {
		// TODO
	}
	
	if (*buf) {
		log_to_empire(emp, ELOG_HOSTILITY, "%s", buf);
	}
}


/**
* Determines if someone 'saw' an offense take place. Call this after the
* offense takes place.
*
* @param char_data *ch The offender.
* @param empire_data *emp The empire being offended.
* @param room_data *from_room Optional: If the character started out in another spot (infiltrate).
*/
bool offense_was_seen(char_data *ch, empire_data *emp, room_data *from_room) {
	descriptor_data *desc;
	char_data *iter;
	
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_in_room) {
		if (ch != iter && GET_LOYALTY(iter) == emp && CAN_SEE(iter, ch)) {
			return TRUE;	// someone here saw it
		}
	}
	
	if (from_room) {
		LL_FOREACH2(ROOM_PEOPLE(from_room), iter, next_in_room) {
			if (ch != iter && GET_LOYALTY(iter) == emp && CAN_SEE(iter, ch)) {
				return TRUE;	// someone here saw it
			}
		}
	}
	
	// check nearby players
	LL_FOREACH(descriptor_list, desc) {
		if (STATE(desc) != CON_PLAYING || !desc->character || desc->character == ch) {
			continue;	// not a player
		}
		if (IS_IMMORTAL(desc->character) || !AWAKE(desc->character) || !CAN_SEE(desc->character, ch)) {
			continue;	// can't even see them
		}
		if (GET_LOYALTY(desc->character) != emp && (!GET_LOYALTY(desc->character) || !has_relationship(GET_LOYALTY(desc->character), emp, DIPL_ALLIED))) {
			continue;	// must be a member of the empire, or an ally
		}
		if (compute_distance(IN_ROOM(ch), IN_ROOM(desc->character)) > 7) {
			continue;	// too far away
		}
		
		return TRUE;	// player saw it
	}
	
	return FALSE;	// nobody saw it
}


/**
* Removes and frees an offense.
*
* @param empire_data *emp The empire to remove from.
* @param struct offense_data *off The offense to remove.
*/
void remove_offense(empire_data *emp, struct offense_data *off) {
	LL_DELETE(EMPIRE_OFFENSES(emp), off);
	free(off);
}


/**
* Sometimes one offense overrides another. This removes any recent offense
* for a character/type combo.
*
* @param empire_data *emp The empire that was offended.
* @param int type The OFFENSE_ type.
* @param char_data *offender The person who offended.
*/
void remove_recent_offenses(empire_data *emp, int type, char_data *offender) {
	struct offense_data *off, *next_off;
	long cutoff = time(0) - (10 * SECS_PER_REAL_MIN);
	
	while (offender && IS_NPC(offender) && offender->master) {
		offender = offender->master;	// climb the food chain
	}
	
	if (!emp || !offender || IS_NPC(offender)) {
		return;	// sanitation check
	}
	
	LL_FOREACH_SAFE(EMPIRE_OFFENSES(emp), off, next_off) {
		if (off->timestamp < cutoff) {
			break;	// offenses are sorted reverse-chronologically and we have gone too far this time
		}
		if (off->type != type) {
			continue;	// wrong type
		}
		if (off->player_id != GET_IDNUM(offender)) {
			continue;	// wrong player;
		}
		
		// ok:
		remove_offense(emp, off);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EXIT LIB ////////////////////////////////////////////////////////////////

/**
* frees up memory for a exit_template
*
* @param struct exit_template *ex The exit template to free.
*/
void free_exit_template(struct exit_template *ex) {
	if (ex->keyword) {
		free(ex->keyword);
	}
	free(ex);
}


/**
* Reads direction data from a file and builds an exit for it.
*
* @param FILE *fl The file to read from.
* @param room_data *room Which room to add an exit to.
* @param int dir The direction to add the exit.
*/
void setup_dir(FILE *fl, room_data *room, int dir) {
	struct room_direction_data *ex;
	int t[5];
	char line[256], str_in[256], *tmp;
	
	// ensure we have a building -- although we SHOULD
	if (!COMPLEX_DATA(room)) {
		COMPLEX_DATA(room) = init_complex_data();
	}

	sprintf(buf2, "room #%d, direction D%d", GET_ROOM_VNUM(room), dir);
	
	// keyword
	tmp = fread_string(fl, buf2);
	
	if (!get_line(fl, line)) {
		log("SYSERR: Format error, %s", buf2);
		exit(1);
	}
	if (sscanf(line, " %s %d ", str_in, &t[0]) != 2) {
		log("SYSERR: Format error, %s", buf2);
		exit(1);
	}

	// see if there's already an exit before making one
	if (!(ex = find_exit(room, dir))) {
		CREATE(ex, struct room_direction_data, 1);
		ex->dir = dir;
		
		ex->next = COMPLEX_DATA(room)->exits;
		COMPLEX_DATA(room)->exits = ex;
	}

	// exit data
	ex->keyword = tmp;
	ex->to_room = t[0];	// this is a vnum
	ex->room_ptr = NULL;	// will be set in renum_world
	ex->exit_info = asciiflag_conv(str_in);

	// sort last
	sort_exits(&(COMPLEX_DATA(room)->exits));
}


 //////////////////////////////////////////////////////////////////////////////
//// EXTRA DESCRIPTION LIB ///////////////////////////////////////////////////

/**
* Frees a list of extra descriptions and sets it to null.
*
* @param struct extra_descr_data **list The list to free.
*/
void free_extra_descs(struct extra_descr_data **list) {
	struct extra_descr_data *ex;
	
	if (!list) {
		return;
	}
	
	while ((ex = *list)) {
		*list = ex->next;
		
		if (ex->keyword) {
			free(ex->keyword);
		}
		if (ex->description) {
			free(ex->description);
		}
		free(ex);
	}
	*list = NULL;
}


/**
* Removes/frees any empty extra descs from a list -- prunes down to just the
* important ones.
*
* @param struct extra_descr_data **list The list to clean up.
*/
void prune_extra_descs(struct extra_descr_data **list) {
	struct extra_descr_data *ex, *next_ex, *temp;
	
	if (!list) {
		return;
	}

	for (ex = *list; ex; ex = next_ex) {
		next_ex = ex->next;
		
		if (!ex->description || !*ex->description || !ex->keyword || !*ex->keyword) {
			REMOVE_FROM_LIST(ex, *list, next);

			if (ex->keyword) {
				free(ex->keyword);
			}
			if (ex->description) {
				free(ex->description);
			}
			free(ex);
		}
	}
}


/**
* Parse one extra desc and assign it to the end of the given list.
*
* @param FILE *fl The file open for reading (just after the E line).
* @param struct extra_descr_data **list A reference to the start of the list to assign to.
* @param char *error_part A string containing e.g. "mob #1234", describing where the error occurred, if one happens.
*/
void parse_extra_desc(FILE *fl, struct extra_descr_data **list, char *error_part) {
	struct extra_descr_data *new_descr, *ex_iter;
				
	CREATE(new_descr, struct extra_descr_data, 1);
	new_descr->keyword = fread_string(fl, buf2);
	new_descr->description = fread_string(fl, buf2);
	new_descr->next = NULL;
		
	// add to end
	if ((ex_iter = *list) != NULL) {
		while (ex_iter->next) {
			ex_iter = ex_iter->next;
		}
		ex_iter->next = new_descr;
	}
	else {
		*list = new_descr;
	}
}


/**
* Output extra descriptions with the E tag to a library file.
*
* @param FILE *fl The file open for writing.
* @param struct extra_descr_data *list The list to save.
*/
void write_extra_descs_to_file(FILE *fl, struct extra_descr_data *list) {
	char temp[MAX_STRING_LENGTH];
	struct extra_descr_data *ex;
	
	for (ex = list; ex; ex = ex->next) {
		fprintf(fl, "E\n");
		fprintf(fl, "%s~\n", NULLSAFE(ex->keyword));
		strcpy(temp, NULLSAFE(ex->description));
		strip_crlf(temp);
		fprintf(fl, "%s~\n", temp);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// GLOBALS LIB /////////////////////////////////////////////////////////////

/**
* Puts a global into the hash table.
*
* @param struct global_data *glb The global data to add to the table.
*/
void add_global_to_table(struct global_data *glb) {
	extern int sort_globals(struct global_data *a, struct global_data *b);
	
	struct global_data *find;
	any_vnum vnum;
	
	if (glb) {
		vnum = GET_GLOBAL_VNUM(glb);
		HASH_FIND_INT(globals_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(globals_table, vnum, glb);
			HASH_SORT(globals_table, sort_globals);
		}
	}
}

/**
* Removes a global from the hash table.
*
* @param struct global_data *glb The global data to remove from the table.
*/
void remove_global_from_table(struct global_data *glb) {
	HASH_DEL(globals_table, glb);
}


/**
* Initializes a new global. This clears all memory for it, so set the vnum
* AFTER.
*
* @param struct global_data *glb The global to initialize.
*/
void clear_global(struct global_data *glb) {
	memset((char *) glb, 0, sizeof(struct global_data));
	
	GET_GLOBAL_VNUM(glb) = NOTHING;
	GET_GLOBAL_ABILITY(glb) = NO_ABIL;
	GET_GLOBAL_PERCENT(glb) = 100.0;
}


/**
* frees up memory for a global data item.
*
* See also: olc_delete_global
*
* @param struct global_data *glb The global data to free.
*/
void free_global(struct global_data *glb) {
	struct global_data *proto = global_proto(GET_GLOBAL_VNUM(glb));
	struct interaction_item *interact;
	
	if (GET_GLOBAL_NAME(glb) && (!proto || GET_GLOBAL_NAME(glb) != GET_GLOBAL_NAME(proto))) {
		free(GET_GLOBAL_NAME(glb));
	}
	
	if (GET_GLOBAL_INTERACTIONS(glb) && (!proto || GET_GLOBAL_INTERACTIONS(glb) != GET_GLOBAL_INTERACTIONS(proto))) {
		while ((interact = GET_GLOBAL_INTERACTIONS(glb))) {
			GET_GLOBAL_INTERACTIONS(glb) = interact->next;
			free(interact);
		}
	}
	
	if (GET_GLOBAL_GEAR(glb) && (!proto || GET_GLOBAL_GEAR(glb) != GET_GLOBAL_GEAR(proto))) {
		free_archetype_gear(GET_GLOBAL_GEAR(glb));
	}
	
	free(glb);
}


/**
* Read one global from file.
*
* @param FILE *fl The open .glb file
* @param any_vnum vnum The global vnum
*/
void parse_global(FILE *fl, any_vnum vnum) {
	void parse_archetype_gear(FILE *fl, struct archetype_gear **list, char *error);

	struct global_data *glb, *find;
	char line[256], str_in[256], str_in2[256], str_in3[256];
	int int_in[4];
	double dbl_in;

	CREATE(glb, struct global_data, 1);
	clear_global(glb);
	GET_GLOBAL_VNUM(glb) = vnum;

	HASH_FIND_INT(globals_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate global vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_global_to_table(glb);
		
	// for error messages
	sprintf(buf2, "global vnum %d", vnum);
	
	// line 1
	GET_GLOBAL_NAME(glb) = fread_string(fl, buf2);
	
	// line 2: type flags typeflags typeexclude min_level-max_level
	if (!get_line(fl, line) || sscanf(line, "%d %s %s %s %d-%d", &int_in[0], str_in, str_in2, str_in3, &int_in[1], &int_in[2]) != 6) {
		log("SYSERR: Format error in line 2 of %s", buf2);
		exit(1);
	}
	
	GET_GLOBAL_TYPE(glb) = int_in[0];
	GET_GLOBAL_FLAGS(glb) = asciiflag_conv(str_in);
	GET_GLOBAL_TYPE_FLAGS(glb) = asciiflag_conv(str_in2);
	GET_GLOBAL_TYPE_EXCLUDE(glb) = asciiflag_conv(str_in3);
	GET_GLOBAL_MIN_LEVEL(glb) = int_in[1];
	GET_GLOBAL_MAX_LEVEL(glb) = int_in[2];
		
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
			exit(1);
		}
		switch (*line) {
			case 'E': {	// extra data
				// line 1: abil perc
				if (!get_line(fl, line) || sscanf(line, "%d %lf", &int_in[0], &dbl_in) != 2) {
					log("SYSERR: Format error E line 1 of %s", buf2);
					exit(1);
				}
				GET_GLOBAL_ABILITY(glb) = int_in[0];
				GET_GLOBAL_PERCENT(glb) = dbl_in;
				
				// line 2: val0 val1 val25
				if (!get_line(fl, line) || sscanf(line, "%d %d %d", &int_in[0], &int_in[1], &int_in[2]) != 3) {
					log("SYSERR: Format error E line 2 of %s", buf2);
					exit(1);
				}
				GET_GLOBAL_VAL(glb, 0) = int_in[0];
				GET_GLOBAL_VAL(glb, 1) = int_in[1];
				GET_GLOBAL_VAL(glb, 2) = int_in[2];
				break;
			}
			case 'G': {	// gear: loc vnum
				parse_archetype_gear(fl, &GET_GLOBAL_GEAR(glb), buf2);
				break;
			}
			case 'I': {	// interaction item
				parse_interaction(line, &GET_GLOBAL_INTERACTIONS(glb), buf2);
				break;
			}

			// end
			case 'S': {
				return;
			}
			
			default: {
				log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
				exit(1);
			}
		}
	}
}


/**
* Outputs one global item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param struct global_data *glb The thing to save.
*/
void write_global_to_file(FILE *fl, struct global_data *glb) {
	void write_archetype_gear_to_file(FILE *fl, struct archetype_gear *list);
	
	char temp[MAX_STRING_LENGTH], temp2[MAX_STRING_LENGTH], temp3[MAX_STRING_LENGTH];
	
	if (!fl || !glb) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_global_to_file called without %s", !fl ? "file" : "global");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_GLOBAL_VNUM(glb));
	
	fprintf(fl, "%s~\n", NULLSAFE(GET_GLOBAL_NAME(glb)));

	strcpy(temp, bitv_to_alpha(GET_GLOBAL_FLAGS(glb)));
	strcpy(temp2, bitv_to_alpha(GET_GLOBAL_TYPE_FLAGS(glb)));
	strcpy(temp3, bitv_to_alpha(GET_GLOBAL_TYPE_EXCLUDE(glb)));
	fprintf(fl, "%d %s %s %s %d-%d\n", GET_GLOBAL_TYPE(glb), temp, temp2, temp3, GET_GLOBAL_MIN_LEVEL(glb), GET_GLOBAL_MAX_LEVEL(glb));
	
	// E: extra data
	fprintf(fl, "E\n");
	fprintf(fl, "%d %.2f\n", GET_GLOBAL_ABILITY(glb), GET_GLOBAL_PERCENT(glb));
	fprintf(fl, "%d %d %d\n", GET_GLOBAL_VAL(glb, 0), GET_GLOBAL_VAL(glb, 1), GET_GLOBAL_VAL(glb, 2));
	
	// G: gear
	write_archetype_gear_to_file(fl, GET_GLOBAL_GEAR(glb));
	
	// I: interactions
	write_interactions_to_file(fl, GET_GLOBAL_INTERACTIONS(glb));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// ICON LIB ////////////////////////////////////////////////////////////////

/**
* Frees a whole set of icons safely.
*
* @param struct icon_data **set The set to free (or a pointer to one to free).
*/
void free_icon_set(struct icon_data **set) {
	struct icon_data *icon;
	
	while ((icon = *set)) {
		if (icon->icon) {
			free(icon->icon);
		}
		if (icon->color) {
			free(icon->color);
		}
		*set = icon->next;
		free(icon);
	}
	*set = NULL;
}


/**
* Parse one icon data item and assign it to the end of the given list.
* This function will trigger an exit(1) if it errors.
*
* @param char *line The line from a db file, to process (the one that had the initial icon command).
* @param FILE *fl The file to load data from.
* @param struct icon_data **list A reference to the start of the list to assign to.
* @param char *error_part A string containing e.g. "mob #1234", describing where the error occurred, if one happens.
*/
void parse_icon(char *line, FILE *fl, struct icon_data **list, char *error_part) {
	struct icon_data *icon, *icon_iter;
	int type;

	// First line was like D7, where D is the file code and the number is a TILESET_x
	if (!isdigit(*(line+1)) || ((type = atoi(line+1)) < 0 || type >= NUM_TILESETS)) {
		log("SYSERR: Bad type in icon line of %s", error_part);
		exit(1);
	}
	
	CREATE(icon, struct icon_data, 1);
	icon->type = type;
	icon->icon = fread_string(fl, error_part);
	icon->color = fread_string(fl, error_part);
	icon->next = NULL;
	
	// append to end
	if ((icon_iter = *list) != NULL) {
		while (icon_iter->next) {
			icon_iter = icon_iter->next;
		}
		icon_iter->next = icon;
	}
	else {
		*list = icon;
	}
}


/**
* Writes out a set of icons to a file.
*
* @param FILE *fl A file open for writing.
* @param char file_tag The file tag (letter) to write data under.
* @param struct icon_data *list The head of the list of icons to write.
*/
void write_icons_to_file(FILE *fl, char file_tag, struct icon_data *list) {
	struct icon_data *icon;
	
	for (icon = list; icon; icon = icon->next) {
		fprintf(fl, "%c%d\n", file_tag, icon->type);
		fprintf(fl, "%s~\n", NULLSAFE(icon->icon));
		fprintf(fl, "%s~\n", NULLSAFE(icon->color));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// INTERACTION LIB /////////////////////////////////////////////////////////

/**
* Parse one interaction item and assign it to the end of the given list.
* This function will trigger an exit(1) if it errors.
*
* @param char *line The line from a db file, to process.
* @param struct interaction_item **list A reference to the start of the list to assign to.
* @param char *error_part A string containing e.g. "mob #1234", describing where the error occurred, if one happens.
*/
void parse_interaction(char *line, struct interaction_item **list, char *error_part) {
	struct interaction_item *interact, *inter_iter;
	int int_in[3];
	double dbl_in;
	char char_in, excl = 0;

	// interaction item: I type vnum percent quantity X
	if (sscanf(line, "I %d %d %lf %d %c", &int_in[0], &int_in[1], &dbl_in, &int_in[2], &char_in) == 5) {	// with exclusion code
		excl = isalpha(char_in) ? char_in : 0;
	}
	else if (sscanf(line, "I %d %d %lf %d", &int_in[0], &int_in[1], &dbl_in, &int_in[2]) == 4) {	// no exclusion code
		excl = 0;
	}
	else {
		log("SYSERR: Format error in 'I' field, %s\n", error_part);
		exit(1);
	}
	
	CREATE(interact, struct interaction_item, 1);
	interact->type = int_in[0];
	interact->vnum = int_in[1];
	interact->percent = dbl_in;
	interact->quantity = int_in[2];
	interact->exclusion_code = excl;
	interact->next = NULL;
	
	// gotta add to the end of the list exclusion order is important
	if ((inter_iter = *list) != NULL) {
		while (inter_iter->next) {
			inter_iter = inter_iter->next;
		}
		inter_iter->next = interact;
	}
	else {
		*list = interact;
	}
}


/**
* Saves interaction data ('I' tag) to a db file.
*
* @param FILE *fl The open file to write to.
* @param struct interaction_item *list The interaction list to write.
*/
void write_interactions_to_file(FILE *fl, struct interaction_item *list) {
	extern const char *interact_types[];
	extern const byte interact_vnum_types[NUM_INTERACTS];
	
	struct interaction_item *interact;
	
	for (interact = list; interact; interact = interact->next) {
		fprintf(fl, "I %d %d %.2f %d", interact->type, interact->vnum, interact->percent, interact->quantity);
		
		if (isalpha(interact->exclusion_code)) {
			fprintf(fl, " %c", interact->exclusion_code);
		}
		
		fprintf(fl, "  # %s: %s\n", interact_types[interact->type], (interact_vnum_types[interact->type] == TYPE_MOB) ? get_mob_name_by_proto(interact->vnum) : get_obj_name_by_proto(interact->vnum));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ISLAND LIB //////////////////////////////////////////////////////////////

/**
* Fetches the island data for a given room. Optionally, you can have this
* function guarantee the existence of the island data (e.g. if you're fetching
* based on a room).
*
* @param int island_id The island to look up.
* @param bool create_if_missing If TRUE, will guarantee a result by adding one.
* @return struct island_info* A pointer to the island data, or NULL if there is none (and you didn't specify create).
*/
struct island_info *get_island(int island_id, bool create_if_missing) {
	extern int sort_island_table(struct island_info *a, struct island_info *b);
	
	struct island_info *isle = NULL;
	char buf[MAX_STRING_LENGTH];
	int iter;
	
	HASH_FIND_INT(island_table, &island_id, isle);
	
	if (!isle && create_if_missing) {
		CREATE(isle, struct island_info, 1);
		// ensure good data
		isle->id = island_id;
		
		if (island_id != NO_ISLAND) {
			sprintf(buf, "Unexplored Island %d", island_id);
		}
		else {
			strcpy(buf, "The Ocean");
		}
		
		isle->name = str_dup(buf);
		isle->flags = NOBITS;
		isle->tile_size = 0;
		isle->center = NOWHERE;
		for (iter = 0; iter < NUM_SIMPLE_DIRS; ++iter) {
			isle->edge[iter] = NOWHERE;
		}
		
		HASH_ADD_INT(island_table, id, isle);
		HASH_SORT(island_table, sort_island_table);
	}

	return isle;
}


/**
* Finds island data using coordinates.
*
* @param char_data *coords The incoming coordinates.
* @return struct island_info* The island data, or NULL if no match.
*/
struct island_info *get_island_by_coords(char *coords) {
	char str[MAX_INPUT_LENGTH];
	room_data *room;
	
	skip_spaces(&coords);
	if (*coords == '(') {
		any_one_word(coords, str);
	}
	else {
		strcpy(str, coords);
	}
	
	// must be coords
	if (!*str || !isdigit(*str) || !strchr(str, ',')) {
		return NULL;
	}
	
	// find room by coords
	if (!(room = find_target_room(NULL, str))) {
		return NULL;
	}
	
	return GET_ISLAND(room);
}


/**
* This finds an island by name. It prefers exact matches over abbrevs. If a
* player is given, it will check the player's name for the island too.
* 
* @param char_data *ch Optional: A player who's looking (may be NULL).
* @param char *name The name of an island.
* @return struct island_info* Returns an island if any matched, or NULL.
*/
struct island_info *get_island_by_name(char_data *ch, char *name) {
	struct empire_island *eisle, *next_eisle, *e_abbrev;
	struct island_info *isle, *next_isle, *abbrev;
	
	// check custom names first
	if (ch && GET_LOYALTY(ch)) {
		e_abbrev = NULL;
		
		HASH_ITER(hh, EMPIRE_ISLANDS(GET_LOYALTY(ch)), eisle, next_eisle) {
			if (eisle->name && !str_cmp(name, eisle->name)) {
				return get_island(eisle->island, TRUE);
			}
			else if (eisle->name && !e_abbrev && is_multiword_abbrev(name, eisle->name)) {
				e_abbrev = eisle;
			}
		}
		
		if (e_abbrev) {
			return get_island(e_abbrev->island, TRUE);
		}
	}
	
	abbrev = NULL;
	HASH_ITER(hh, island_table, isle, next_isle) {
		if (!str_cmp(name, isle->name)) {
			return isle;
		}
		else if (!abbrev && is_multiword_abbrev(name, isle->name)) {
			abbrev = isle;
		}
	}
	
	// didn't find an exact match, so:
	return abbrev;
}


/**
* Gets the name that a player sees for an island.
*
* @param int island_id Which island.
* @param char_data *for_ch The player.
*/
char *get_island_name_for(int island_id, char_data *for_ch) {
	struct empire_island *eisle;
	struct island_info *island;
	
	if (island_id == NO_ISLAND || !(island = get_island(island_id, TRUE))) {
		return "No Island";
	}
	if (!GET_LOYALTY(for_ch)) {
		return island->name;
	}
	if (!(eisle = get_empire_island(GET_LOYALTY(for_ch), island_id))) {
		return island->name;
	}
	
	return eisle->name ? eisle->name : island->name;
}


/**
* Determines if an island has a default name or not.
*
* @param struct island_info *island The island to check.
* @return bool TRUE if the island has its default name.
*/
bool island_has_default_name(struct island_info *island) {
	char buf[MAX_STRING_LENGTH];
	
	sprintf(buf, "Unexplored Island %d", island->id);
	return !str_cmp(island->name, buf);
}


/**
* Load one island from file.
*
* @param FILE *fl The open read file.
* @param int id The island id.
* @return struct island_info* The island.
*/
static struct island_info *load_one_island(FILE *fl, int id) {	
	char errstr[MAX_STRING_LENGTH], line[256], str_in[256];
	struct island_info *isle;
	
	snprintf(errstr, sizeof(errstr), "island %d", id);
	
	// this does all the adding to tables and whatnot too!
	isle = get_island(id, TRUE);
	
	// line 1: name
	if (isle->name) {
		free(isle->name);
	}
	isle->name = fread_string(fl, errstr);
	
	// line 2: center, flags
	if (!get_line(fl, line) || sscanf(line, "%s", str_in) != 1) {
		log("SYSERR: Format error in line 2 of %s", errstr);
		exit(1);
	}
	
	isle->flags = asciiflag_conv(str_in);
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in instance, expecting alphabetic flags, got: %s", line);
			exit(1);
		}
		switch (*line) {
			case 'D': {	// description
				isle->desc = fread_string(fl, errstr);
				break;
			}
			case 'S': {
				// done
				return isle;
			}
		}
	}
	
	return isle;
}


/**
* Loads island data, if there is any.
*/
void load_islands(void) {
	char line[256];
	FILE *fl;
	
	if (!(fl = fopen(ISLAND_FILE, "r"))) {
		// log("SYSERR: Unable to load islands from file.");
		// there just isn't a file -- that's ok, we'll generate new data
		return;
	}
	
	while (get_line(fl, line)) {
		if (*line == '#') {
			load_one_island(fl, atoi(line+1));
		}
		else if (*line == '$') {
			// done;
			break;
		}
		else {
			// junk data?
		}
	}
	
	fclose(fl);
}


/**
* Write island data to file.
*/
void save_island_table(void) {
	struct island_info *isle, *next_isle;
	char temp[MAX_STRING_LENGTH];
	FILE *fl;
	
	if (!(fl = fopen(ISLAND_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to write %s", ISLAND_FILE TEMP_SUFFIX);
		return;
	}

	HASH_ITER(hh, island_table, isle, next_isle) {
		fprintf(fl, "#%d\n", isle->id);
		fprintf(fl, "%s~\n", NULLSAFE(isle->name));
		fprintf(fl, "%s\n", bitv_to_alpha(isle->flags));
		
		if (isle->desc && *isle->desc) {
			strcpy(temp, isle->desc);
			strip_crlf(temp);
			fprintf(fl, "D\n%s~\n", temp);
		}
		
		fprintf(fl, "S\n");
	}

	fprintf(fl, "$\n");
	fclose(fl);
	rename(ISLAND_FILE TEMP_SUFFIX, ISLAND_FILE);
}


/**
* Simple sorter for the island_table hash
*
* @param struct island_info *a One element
* @param struct island_info *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_island_table(struct island_info *a, struct island_info *b) {
	return a->id - b->id;
}



 //////////////////////////////////////////////////////////////////////////////
//// MOBILE LIB //////////////////////////////////////////////////////////////

/**
* Puts a mob in the hash table.
*
* @param char_data *mob The mob to add to the table.
*/
void add_mobile_to_table(char_data *mob) {
	extern int sort_mobiles(char_data *a, char_data *b);
	
	char_data *find;
	mob_vnum vnum;
	
	if (mob) {
		vnum = mob->vnum;	// can't use GET_MOB_VNUM macro here
		HASH_FIND_INT(mobile_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(mobile_table, vnum, mob);
			HASH_SORT(mobile_table, sort_mobiles);
		}
	}
}

/**
* Removes a mob from the hash table.
*
* @param char_data *mob The mob to remove from the table.
*/
void remove_mobile_from_table(char_data *mob) {
	HASH_DEL(mobile_table, mob);
}


/**
* Reads in one mob from a file and sets up the prototype and index.
*
* @param FILE *mob_f The file to load from.
* @param int nr The vnum of the mob to set up.
*/
void parse_mobile(FILE *mob_f, int nr) {
	int j, t[10], iter;
	char line[256], *tmpptr;
	char f1[128], f2[128];
	char_data *mob, *find;
	
	// create!
	CREATE(mob, char_data, 1);
	clear_char(mob);
	mob->vnum = nr;

	HASH_FIND_INT(mobile_table, &nr, find);
	if (find) {
		log("WARNING: Duplicate mobile vnum #%d", nr);
		// but have to load it anyway to advance the file
	}
	add_mobile_to_table(mob);

	/*
	 * Mobiles should NEVER use anything in the 'player_specials' structure.
	 * The only reason we have every mob in the game share this copy of the
	 * structure is to save newbie coders from themselves. -gg 2/25/98
	 */
	mob->player_specials = &dummy_mob;
	sprintf(buf2, "mob vnum %d", nr);

	/***** String data *****/
	mob->player.name = fread_string(mob_f, buf2);
	tmpptr = mob->player.short_descr = fread_string(mob_f, buf2);
	if (tmpptr && *tmpptr)
		if (!str_cmp(fname(tmpptr), "a") || !str_cmp(fname(tmpptr), "an") || !str_cmp(fname(tmpptr), "the"))
			*tmpptr = LOWER(*tmpptr);
	mob->player.long_descr = fread_string(mob_f, buf2);
	
	// 1. min_scale_level max_scale_level mobflags affs [size]
	if (!get_line(mob_f, line) || sscanf(line, "%d %d %s %s %d", &t[0], &t[1], f1, f2, &t[2]) != 5) {
		t[2] = SIZE_NORMAL;	// backwards-compatible
		
		if (sscanf(line, "%d %d %s %s", &t[0], &t[1], f1, f2) != 4) {
			log("SYSERR: Format error in first numeric section of mob #%d\n...expecting line of form '# # # # #'", nr);
			exit(1);
		}
	}
	
	GET_MIN_SCALE_LEVEL(mob) = t[0];
	GET_MAX_SCALE_LEVEL(mob) = t[1];
	MOB_FLAGS(mob) = asciiflag_conv(f1);
	AFF_FLAGS(mob) = asciiflag_conv(f2);
	SET_SIZE(mob) = t[2];

	SET_BIT(MOB_FLAGS(mob), MOB_ISNPC);	// sanity
	REMOVE_BIT(MOB_FLAGS(mob), MOB_EXTRACTED);	// sanity

	// 2. sex name-list move-type attack-type
	if (!get_line(mob_f, line) || sscanf(line, "%d %d %d %d", &t[0], &t[1], &t[2], &t[3]) != 4) {
		log("SYSERR: Format in second numeric section of mob #%d\n...expecting line of form '# # # #'", nr);
		exit(1);
	}

	mob->player.sex = t[0];
	MOB_NAME_SET(mob) = t[1];
	mob->mob_specials.move_type = t[2];
	mob->mob_specials.attack_type = t[3];

	// basic setup
	mob->points.max_pools[HEALTH] = 10;
	mob->points.max_pools[MOVE] = 10;
	mob->points.max_pools[MANA] = 0;
	mob->points.max_pools[BLOOD] = 10;
	mob->char_specials.position = POS_STANDING;
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		mob->aff_attributes[iter] = mob->real_attributes[iter];
	}
	for (j = 0; j < NUM_WEARS; j++) {
		mob->equipment[j] = NULL;
	}
	mob->desc = NULL;
	mob->mob_specials.spawn_time = 0;

	sprintf(buf2, "mob %d, after numeric constants\n...expecting alphabetic flags", nr);
	j = 0;

	for (;;) {
		if (!get_line(mob_f, line)) {
			log("SYSERR: Format error in %s", buf2);
			exit(1);
		}
		switch (*line) {
			case 'D': { // look desc
				mob->player.look_descr = fread_string(mob_f, buf2);
				break;
			}
			case 'F': {	// faction
				if (strlen(line) > 2) {
					MOB_FACTION(mob) = find_faction_by_vnum(atoi(line + 2));
				}
				break;
			}
			case 'I': {	// interaction item
				parse_interaction(line, &mob->interactions, buf2);
				break;
			}
			
			case 'M': {	// custom messages
				parse_custom_message(mob_f, &MOB_CUSTOM_MSGS(mob), buf2);
				break;
			}
			
			case 'T': {	// trigger
				parse_trig_proto(line, &(mob->proto_script), buf2);
				break;
			}

			case 'S': {
				return;
			}
			default: {
				log("SYSERR: Format error in %s", buf2);
				exit(1);
			}
		}
	}
}


/**
* Outputs one mob in the db file format, starting with a #VNUM and ending
* in an S.
*
* @param FILE *fl The file to write it to.
* @param char_data *mob The mobile to save.
*/
void write_mob_to_file(FILE *fl, char_data *mob) {
	char temp[MAX_STRING_LENGTH], temp2[MAX_STRING_LENGTH];
	
	if (!fl || !mob) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_mob_to_file called without %s", !fl ? "file" : "mob");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_MOB_VNUM(mob));
	fprintf(fl, "%s~\n", NULLSAFE(GET_PC_NAME(mob)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_SHORT_DESC(mob)));

	strcpy(temp, NULLSAFE(GET_LONG_DESC(mob)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);

	// min_scale_level max_scale_level mobflags affs size
	strcpy(temp, bitv_to_alpha(MOB_FLAGS(mob)));
	strcpy(temp2, bitv_to_alpha(AFF_FLAGS(mob)));
	fprintf(fl, "%d %d %s %s %d\n", GET_MIN_SCALE_LEVEL(mob), GET_MAX_SCALE_LEVEL(mob), temp, temp2, SET_SIZE(mob));
	
	// sex name-list move-type attack-type
	fprintf(fl, "%d %d %d %d\n", GET_SEX(mob), MOB_NAME_SET(mob), MOB_MOVE_TYPE(mob), MOB_ATTACK_TYPE(mob));

	// optionals:
	
	// D: look desc
	if (GET_LOOK_DESC(mob) && *GET_LOOK_DESC(mob)) {
		strcpy(temp, GET_LOOK_DESC(mob));
		strip_crlf(temp);
		fprintf(fl, "D\n%s~\n", temp);
	}
	
	// F: faction
	if (MOB_FACTION(mob)) {
		fprintf(fl, "F %d\n", FCT_VNUM(MOB_FACTION(mob)));
	}
	
	// I: interactions
	write_interactions_to_file(fl, mob->interactions);
	
	// M: custom message
	write_custom_messages_to_file(fl, 'M', MOB_CUSTOM_MSGS(mob));
	
	// T, V: triggers
	write_trig_protos_to_file(fl, 'T', mob->proto_script);
		
	// END
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT LIB //////////////////////////////////////////////////////////////

/**
* Puts an object in the hash table.
*
* @param obj_data *obj The object to add to the table.
*/
void add_object_to_table(obj_data *obj) {
	extern int sort_objects(obj_data *a, obj_data *b);
	
	obj_data *find;
	obj_vnum vnum;
	
	if (obj) {
		vnum = GET_OBJ_VNUM(obj);
		HASH_FIND_INT(object_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(object_table, vnum, obj);
			HASH_SORT(object_table, sort_objects);
		}
	}
}

/**
* Removes an object from the hash table.
*
* @param obj_data *obj The object to remove from the table.
*/
void remove_object_from_table(obj_data *obj) {
	HASH_DEL(object_table, obj);
}


/**
* @param struct obj_binding **list The list of obj bindings to free.
*/
void free_obj_binding(struct obj_binding **list) {
	struct obj_binding *bind;
	
	while (list && (bind = *list)) {
		*list = bind->next;
		free(bind);
	}
}


/* release memory allocated for an obj struct */
void free_obj(obj_data *obj) {
	struct interaction_item *interact;
	struct obj_storage_type *store;
	obj_data *proto;
	
	proto = obj_proto(GET_OBJ_VNUM(obj));

	if (GET_OBJ_KEYWORDS(obj) && (!proto || GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(proto))) {
		free(GET_OBJ_KEYWORDS(obj));
	}
	if (GET_OBJ_LONG_DESC(obj) && (!proto || GET_OBJ_LONG_DESC(obj) != GET_OBJ_LONG_DESC(proto))) {
		free(GET_OBJ_LONG_DESC(obj));
	}
	if (GET_OBJ_SHORT_DESC(obj) && (!proto || GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(proto))) {
		free(GET_OBJ_SHORT_DESC(obj));
	}
	if (GET_OBJ_ACTION_DESC(obj) && (!proto || GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(proto))) {
		free(GET_OBJ_ACTION_DESC(obj));
	}
	if (obj->ex_description && (!proto || obj->ex_description != proto->ex_description)) {
		free_extra_descs(&obj->ex_description);
	}
	if (obj->proto_script && (!proto || obj->proto_script != proto->proto_script)) {
		free_proto_scripts(&obj->proto_script);
	}

	if (obj->interactions && (!proto || obj->interactions != proto->interactions)) {
		while ((interact = obj->interactions)) {
			obj->interactions = interact->next;
			free(interact);
		}
	}
	if (obj->storage && (!proto || obj->storage != proto->storage)) {
		while ((store = obj->storage)) {
			obj->storage = store->next;
			free(store);
		}
	}
	
	if (obj->custom_msgs && (!proto || obj->custom_msgs != proto->custom_msgs)) {
		free_custom_messages(obj->custom_msgs);
	}
	
	free_obj_binding(&OBJ_BOUND_TO(obj));
	
	// applies are ALWAYS a copy
	free_obj_apply_list(GET_OBJ_APPLIES(obj));
	
	/* free any assigned scripts */
	if (SCRIPT(obj)) {
		extract_script(obj, OBJ_TRIGGER);
	}

	/* find_obj helper */
	if (obj->script_id > 0) {
		remove_from_lookup_table(obj->script_id);
	}

	free(obj);
}


/**
* Reads in one object from a file and sets up the index/prototype.
*
* @param FILE *obj_f The file to read from.
* @param int nr The vnum of the object.
*/
void parse_object(FILE *obj_f, int nr) {
	static char line[256];
	int t[10], retval;
	char *tmpptr;
	char f1[256], f2[256];
	struct obj_storage_type *store, *last_store = NULL;
	struct obj_apply *apply, *last_apply = NULL;
	obj_data *obj, *find;
	
	// create
	CREATE(obj, obj_data, 1);
	clear_object(obj);
	obj->vnum = nr;

	HASH_FIND_INT(object_table, &nr, find);
	if (find) {
		log("WARNING: Duplicate object vnum #%d", nr);
		// but have to load it anyway to advance the file
	}
	add_object_to_table(obj);

	sprintf(buf2, "object #%d", nr);

	/* *** string data *** */
	if ((obj->name = fread_string(obj_f, buf2)) == NULL) {
		log("SYSERR: Null obj name or format error at or near %s", buf2);
		exit(1);
	}
	tmpptr = obj->short_description = fread_string(obj_f, buf2);
	if (tmpptr && *tmpptr)
		if (!str_cmp(fname(tmpptr), "a") || !str_cmp(fname(tmpptr), "an") || !str_cmp(fname(tmpptr), "the"))
			*tmpptr = LOWER(*tmpptr);

	tmpptr = obj->description = fread_string(obj_f, buf2);
	if (tmpptr && *tmpptr)
		CAP(tmpptr);
	obj->action_description = fread_string(obj_f, buf2);

	/* *** numeric data *** */
	if (!get_line(obj_f, line)) {
		log("SYSERR: Expecting first numeric line of %s, but file ended!", buf2);
		exit(1);
	}
	if ((retval = sscanf(line, " %d %s %s %d", &t[0], f1, f2, &t[1])) != 4) {
		// older version of the file: missing version into
		t[1] = 1;
		if ((retval = sscanf(line, " %d %s %s", t, f1, f2)) != 3) {
			log("SYSERR: Format error in first numeric line (expecting 3 args, got %d), %s", retval, buf2);
			exit(1);
		}
	}
	obj->obj_flags.type_flag = t[0];
	obj->obj_flags.extra_flags = asciiflag_conv(f1);
	obj->obj_flags.wear_flags = asciiflag_conv(f2);
	OBJ_VERSION(obj) = t[1];

	if (!get_line(obj_f, line)) {
		log("SYSERR: Expecting second numeric line of %s, but file ended!", buf2);
		exit(1);
	}
	if ((retval = sscanf(line, "%d %d %d", t, t + 1, t + 2)) != 3) {
		log("SYSERR: Format error in second numeric line (expecting 3 args, got %d), %s", retval, buf2);
		exit(1);
	}
	obj->obj_flags.value[0] = t[0];
	obj->obj_flags.value[1] = t[1];
	obj->obj_flags.value[2] = t[2];

	/* Initialized to zero here, but modified later */
	obj->obj_flags.bitvector = 0;

	if (!get_line(obj_f, line)) {
		log("SYSERR: Expecting third numeric line of %s, but file ended!", buf2);
		exit(1);
	}
	if ((retval = sscanf(line, "%d %d", &t[0], &t[1])) != 2) {
		log("SYSERR: Format error in third numeric line (expecting 2 args, got %d), %s", retval, buf2);
		exit(1);
	}

	obj->obj_flags.material = t[0];
	obj->obj_flags.timer = t[1];

	/* *** extra descriptions and affect fields *** */
	strcat(buf2, ", after numeric constants\n...expecting alphabetic flags");

	for (;;) {
		if (!get_line(obj_f, line)) {
			log("SYSERR: Format error in %s", buf2);
			exit(1);
		}
		switch (*line) {
			case 'A':
				if (!get_line(obj_f, line)) {
					log("SYSERR: Format error in 'A' field, %s\n...expecting 3 numeric constants but file ended!", buf2);
					exit(1);
				}
				
				if (sscanf(line, " %d %d %d ", &t[0], &t[1], &t[2]) != 3) {
					// backwards-compatible
					t[2] = APPLY_TYPE_NATURAL;
					if ((retval = sscanf(line, " %d %d ", &t[0], &t[1])) != 2) {
						log("SYSERR: Format error in 'A' field, %s\n...expecting 3 numeric arguments, got %d\n...offending line: '%s'", buf2, retval, line);
						exit(1);
					}
				}
				CREATE(apply, struct obj_apply, 1);
				apply->location = t[0];
				apply->modifier = t[1];
				apply->apply_type = t[2];
				
				// append to end
				if (last_apply) {
					last_apply->next = apply;
				}
				else {
					GET_OBJ_APPLIES(obj) = apply;
				}
				last_apply = apply;
				break;

			case 'B':
				if (!get_line(obj_f, line)) {
					log("SYSERR: Format error in 'B' field, %s\n...expecting bitvectors but file ended!", buf2);
					exit(1);
				}
				if (sscanf(line, " %s ", f1) != 1) {
					log("SYSERR: Unable to read 'B' value, %s", buf2);
					exit(1);
				}
				obj->obj_flags.bitvector |= asciiflag_conv(f1);
				break;
			
			case 'C': {	// scalable
				if (!get_line(obj_f, line) || sscanf(line, "%d %d", &t[0], &t[1]) != 2) {
					log("SYSERR: Format error in 'C' Field, %s", buf2);
					exit(1);
				}
				
				GET_OBJ_MIN_SCALE_LEVEL(obj) = t[0];
				GET_OBJ_MAX_SCALE_LEVEL(obj) = t[1];
				break;
			}

			case 'E': {	// extra desc
				parse_extra_desc(obj_f, &(obj->ex_description), buf2);
				break;
			}

			case 'I': {	// interaction item
				parse_interaction(line, &obj->interactions, buf2);
				break;
			}
						
			case 'M': {	// custom messages
				parse_custom_message(obj_f, &obj->custom_msgs, buf2);
				break;
			}
			
			case 'O': {	// component info
				if (!get_line(obj_f, line) || sscanf(line, "%d %s", t, f1) != 2) {
					log("SYSERR: Format error in 'O' Field, %s", buf2);
					exit(1);
				}
				
				GET_OBJ_CMP_TYPE(obj) = t[0];
				GET_OBJ_CMP_FLAGS(obj) = asciiflag_conv(f1);
				break;
			}
			
			case 'Q': {	// requires quest
				if (sscanf(line, "Q %d", &t[0]) == 1) {
					GET_OBJ_REQUIRES_QUEST(obj) = t[0];
				}
				break;
			}
			
			case 'R': {
				if (!get_line(obj_f, line) || sscanf(line, "%d %s", t, f1) != 2) {
					log("SYSERR: Format error in 'R' Field, %s", buf2);
					exit(1);
				}
				
				CREATE(store, struct obj_storage_type, 1);
				store->building_type = t[0];
				store->flags = asciiflag_conv(f1);
				store->next = NULL;
				
				if (last_store) {
					last_store->next = store;
				}
				else {
					obj->storage = store;
				}
				last_store = store;
				break;
			}
			
			case 'T': {	// trigger
				dg_obj_trigger(line, obj);
				break;
			}
			
			case 'S':
				check_object(obj);
				return;

			default:
				log("SYSERR: Format error in %s", buf2);
				exit(1);
		}
	}
}


/**
* Outputs one object in the db file format, starting with a #VNUM and ending
* in an S.
*
* @param FILE *fl The file to write it to.
* @param obj_data *obj The object to save.
*/
void write_obj_to_file(FILE *fl, obj_data *obj) {
	char temp[MAX_STRING_LENGTH], temp2[MAX_STRING_LENGTH];
	struct obj_storage_type *store;
	struct obj_apply *apply;
	
	if (!fl || !obj) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_obj_to_file called without %s", !fl ? "file" : "object");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_OBJ_VNUM(obj));
	fprintf(fl, "%s~\n", NULLSAFE(GET_OBJ_KEYWORDS(obj)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_OBJ_SHORT_DESC(obj)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_OBJ_LONG_DESC(obj)));

	strcpy(temp, NULLSAFE(GET_OBJ_ACTION_DESC(obj)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// I imagine calling this function twice in one fprintf would not have the desired effect.
	strcpy(temp, bitv_to_alpha(GET_OBJ_EXTRA(obj)));
	strcpy(temp2, bitv_to_alpha(GET_OBJ_WEAR(obj)));
	fprintf(fl, "%d %s %s %d\n", GET_OBJ_TYPE(obj), temp, temp2, OBJ_VERSION(obj));

	fprintf(fl, "%d %d %d\n", GET_OBJ_VAL(obj, 0), GET_OBJ_VAL(obj, 1), GET_OBJ_VAL(obj, 2));
	
	// timer is converted from -1 to 0 here for historical reasons
	fprintf(fl, "%d %d\n", GET_OBJ_MATERIAL(obj), GET_OBJ_TIMER(obj) == UNLIMITED ? 0 : MAX(0, GET_OBJ_TIMER(obj)));

	// now optional parts:
	
	// A: applies
	for (apply = GET_OBJ_APPLIES(obj); apply; apply = apply->next) {
		fprintf(fl, "A\n");
		fprintf(fl, "%d %d %d\n", apply->location, apply->modifier, apply->apply_type);
	}
	
	// B: sets affect bitvector
	if (GET_OBJ_AFF_FLAGS(obj)) {
		fprintf(fl, "B\n");
		fprintf(fl, "%s\n", bitv_to_alpha(GET_OBJ_AFF_FLAGS(obj)));
	}
	
	// C: scaling
	if (GET_OBJ_MIN_SCALE_LEVEL(obj) > 0 || GET_OBJ_MAX_SCALE_LEVEL(obj) > 0) {
		fprintf(fl, "C\n");
		fprintf(fl, "%d %d\n", GET_OBJ_MIN_SCALE_LEVEL(obj), GET_OBJ_MAX_SCALE_LEVEL(obj));
	}
	
	// E: extra descriptions
	write_extra_descs_to_file(fl, obj->ex_description);
	
	// I: interactions
	write_interactions_to_file(fl, obj->interactions);
	
	// M: custom message
	write_custom_messages_to_file(fl, 'M', obj->custom_msgs);
	
	// O: component data
	if (GET_OBJ_CMP_TYPE(obj) != CMP_NONE) {
		fprintf(fl, "O\n");
		fprintf(fl, "%d %s\n", GET_OBJ_CMP_TYPE(obj), bitv_to_alpha(GET_OBJ_CMP_FLAGS(obj)));
	}
	
	// Q: requires quest
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
		fprintf(fl, "Q %d\n", GET_OBJ_REQUIRES_QUEST(obj));
	}
	
	// R: storage
	for (store = obj->storage; store; store = store->next) {
		fprintf(fl, "R\n");
		fprintf(fl, "%d %s\n", store->building_type, bitv_to_alpha(store->flags));
	}
	
	// T, V: triggers
	write_trig_protos_to_file(fl, 'T', obj->proto_script);

	// END
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// RESOURCE LIB ////////////////////////////////////////////////////////////

/**
* Copies a resource list.
*
* @param struct resource_data *input The list to copy.
* @return struct resource_data* The copied list.
*/
struct resource_data *copy_resource_list(struct resource_data *input) {
	struct resource_data *new, *last, *list, *iter;
	
	last = list = NULL;
	for (iter = input; iter; iter = iter->next) {
		CREATE(new, struct resource_data, 1);
		*new = *iter;
		new->next = NULL;
		
		if (last) {
			last->next = new;
		}
		else {
			list = new;
		}
		last = new;
	}
	
	return list;
}


/**
* Frees a list of resource_data items.
*
* @param struct resource_data *list The start of the list to free.
*/
void free_resource_list(struct resource_data *list) {
	struct resource_data *res;
	
	while ((res = list)) {
		list = res->next;
		free(res);
	}
}


/**
* Parses a resource tag (usually 'R', written by write_resources_to_file().
* This file should have just read the 'R' line, and the next line to read is its data.
*
* @param FILE *fl The file to read from.
* @param struct resource_data **list The resource list to append to.
* @param char *error_str An identifier to log if there is a problem.
*/
void parse_resource(FILE *fl, struct resource_data **list, char *error_str) {
	int vnum, amount, type, misc;
	struct resource_data *res;
	char line[256];
	
	if (!fl || !list || !get_line(fl, line)) {
		log("SYSERR: data error in resource line of %s", error_str ? error_str : "unknown resource");
		exit(1);
	}
	
	if (sscanf(line, "%d %d %d %d", &vnum, &amount, &type, &misc) != 4) {
		// backwards-compatibility
		if (sscanf(line, "%d %d", &vnum, &amount) == 2) {
			type = RES_OBJECT;
			misc = 0;
		}
		else {
			log("SYSERR: format error in resource line of %s", error_str ? error_str : "unknown resource");
			exit(1);
		}
	}
	
	CREATE(res, struct resource_data, 1);
	res->type = type;
	res->vnum = vnum;
	res->amount = amount;
	res->misc = misc;
	
	LL_APPEND(*list, res);
}


/**
* Writes new-style resource_data to data file.
*
* @param FILE *fl The file, open for writing.
* @param char letter The letter to write for the tag, usually 'R'.
* @param struct resource_data *list The list of resources to write.
*/
void write_resources_to_file(FILE *fl, char letter, struct resource_data *list) {
	struct resource_data *res;
	
	if (!fl || !list) {
		return;
	}
	
	for (res = list; res; res = res->next) {
		// for historical reasons (pre-2.0b16 compatibility), vnum and amount come first
		fprintf(fl, "%c\n%d %d %d %d\n", letter, res->vnum, res->amount, res->type, res->misc);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ROOM LIB ////////////////////////////////////////////////////////////////


/**
* Adds a room to any applicable world hash tables.
*
* @param room_data *room The room to add.
*/
void add_room_to_world_tables(room_data *room) {	
	HASH_ADD_INT(world_table, vnum, room);
	
	// interior linked list
	if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		room->next_interior = interior_room_list;
		interior_room_list = room;
	}
	
	world_is_sorted = FALSE;
}


/**
* Remove a room from the correct world tables.
*
* @param room_data *room The room to remove.
*/
void remove_room_from_world_tables(room_data *room) {
	room_data *temp;
	
	HASH_DEL(world_table, room);
	
	if (room->vnum >= MAP_SIZE) {
		REMOVE_FROM_LIST(room, interior_room_list, next_interior);
	}
}


/**
* Load one room from file.
*
* @param FILE *fl The open file (having just read the # line).
* @param room_vnum vnum The vnum to read in.
*/
void parse_room(FILE *fl, room_vnum vnum) {
	void add_trd_home_room(room_vnum vnum, room_vnum home_room);
	void add_trd_owner(room_vnum vnum, empire_vnum owner);

	char line[256], line2[256], error_buf[256], error_log[MAX_STRING_LENGTH], str1[256], str2[256];
	double dbl_in;
	long l_in;
	int t[10];
	struct depletion_data *dep;
	struct reset_com *reset, *last_reset = NULL;
	struct affected_type *af;
	struct track_data *track;
	room_data *room, *find;
	bool error = FALSE;
	char *ptr;

	sprintf(error_buf, "room #%d", vnum);
	
	CREATE(room, room_data, 1);
	
	// basic setup: things that don't default to 0/NULL
	room->vnum = vnum;
	
	// attach/create shared data
	if (vnum < MAP_SIZE) {
		GET_MAP_LOC(room) = &(world_map[MAP_X_COORD(vnum)][MAP_Y_COORD(vnum)]);
		SHARED_DATA(room) = GET_MAP_LOC(room)->shared;
	}
	else {
		CREATE(SHARED_DATA(room), struct shared_room_data, 1);
	}
	
	HASH_FIND_INT(world_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate room vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}

	// put it in the hash table -- do not need to set need_world_index as this is a startup load
	add_room_to_world_tables(room);

	// FIRST LINE: sector original subtype progress
	if (!get_line(fl, line)) {
		log("SYSERR: Expecting sector line of room #%d but file ended!", vnum);
		exit(1);
	}

	if (sscanf(line, "%d %d %d", &t[0], &t[1], &t[2]) != 3) {
		log("SYSERR: Format error in sector line of room #%d", vnum);
		exit(1);
	}
	
	// need to unlink shared data, if present, if this is not an ocean
	if (t[1] != BASIC_OCEAN && SHARED_DATA(room) == &ocean_shared_data) {
		// converting from ocean to non-ocean
		SHARED_DATA(room) = NULL;	// unlink ocean share
		CREATE(SHARED_DATA(room), struct shared_room_data, 1);
		
		if (GET_MAP_LOC(room)) {	// and pass this shared data back up to the world
			GET_MAP_LOC(room)->shared = SHARED_DATA(room);
		}
	}
	
	GET_ISLAND_ID(room) = t[0];
	SHARED_DATA(room)->island_ptr = (t[0] == NO_ISLAND ? NULL : get_island(t[0], TRUE));
	room->sector_type = sector_proto(t[1]);
	room->base_sector = sector_proto(t[2]);
	
	if (GET_MAP_LOC(room)) {
		GET_MAP_LOC(room)->sector_type = room->sector_type;
		GET_MAP_LOC(room)->base_sector = room->base_sector;
	}

	// set up building data?
	if (IS_ANY_BUILDING(room) || IS_ADVENTURE_ROOM(room)) {
		COMPLEX_DATA(room) = init_complex_data();
	}

	// extra data
	sprintf(error_log, "SYSERR: Format error in room #%d (expecting D/E/S)", vnum);

	for (;;) {
		if (!get_line(fl, line)) {
			log("%s", error_log);
			exit(1);
		}
		switch (*line) {
			case 'A': {	// affects
				if (!get_line(fl, line2) || sscanf(line2, "%d %d %ld %d %d %s", &t[0], &t[1], &l_in, &t[3], &t[4], str1) != 6) {
					log("SYSERR: Format error in A line of room #%d", vnum);
					exit(1);
				}
				
				CREATE(af, struct affected_type, 1);
				af->type = t[0];
				af->cast_by = t[1];
				af->duration = l_in;
				af->modifier = t[3];
				af->location = t[4];
				af->bitvector = asciiflag_conv(str1);
				
				LL_PREPEND(ROOM_AFFECTS(room), af);
				break;
			}
			case 'B': {	// building data
				if (!get_line(fl, line2) || sscanf(line2, "%d %d %d %d %ld %lf %d %d", &t[0], &t[1], &t[2], &t[3], &l_in, &dbl_in, &t[6], &t[7]) != 8) {
					log("SYSERR: Format error in B line of room #%d", vnum);
					exit(1);
				}
				
				// ensure has building
				if (!COMPLEX_DATA(room)) {
					COMPLEX_DATA(room) = init_complex_data();
				}
				
				if (t[0] != NOTHING) {
					attach_building_to_room(building_proto(t[0]), room, FALSE);
				}
				if (t[1] != NOTHING) {
					attach_template_to_room(room_template_proto(t[1]), room);
				}
				
				COMPLEX_DATA(room)->entrance = t[2];
				COMPLEX_DATA(room)->patron = t[3];
				COMPLEX_DATA(room)->burn_down_time = l_in;
				COMPLEX_DATA(room)->damage = dbl_in;	// formerly t[5], which is now unused
				COMPLEX_DATA(room)->private_owner = t[6];
				COMPLEX_DATA(room)->paint_color = t[7];
				
				break;
			}
			case 'C': { // reset command
				if (!*(line + 1) || !*(line + 2) || *(line + 2) == '*') {
					// skip
					break;
				}
			
				CREATE(reset, struct reset_com, 1);
				reset->next = NULL;
				
				// add to end
				if (last_reset) {
					last_reset->next = reset;
				}
				else {
					room->reset_commands = reset;
				}
				last_reset = reset;
				
				// load data
				reset->command = *(line + 2);
				ptr = line + 3;
				
				if (reset->command == 'V') { // V: script variable reset
					// trigger_type misc? room_vnum sarg1 sarg2
					if (sscanf(line + 3, " %d %d %d %79s %79[^\f\n\r\t\v]", &reset->arg1, &reset->arg2, &reset->arg3, str1, str2) != 5) {
						error = TRUE;
					}
					else {
						reset->sarg1 = strdup(str1);
						reset->sarg2 = strdup(str2);
					}
				}
				else if (strchr("Y", reset->command) != NULL) {	// generic-mob: 3-arg command
					// generic-sex generic-name empire-id
					if (sscanf(ptr, " %d %d %d ", &reset->arg1, &reset->arg2, &reset->arg3) != 3) {
						error = TRUE;
					}
				}
				else if (strchr("CDT", reset->command) != NULL) {	/* C, D, Trigger: a 2-arg command */
					// trigger_type trigger_vnum
					if (sscanf(ptr, " %d %d ", &reset->arg1, &reset->arg2) != 2) {
						error = TRUE;
					}
				}
				else if (strchr("M", reset->command) != NULL) {	// Mob: 3 args
					if (sscanf(ptr, " %d %s %d ", &reset->arg1, str1, &reset->arg2) != 3) {
						error = TRUE;
					}
					else {
						reset->sarg1 = strdup(str1);
					}
				}
				else if (strchr("I", reset->command) != NULL) {	/* a 1-arg command */
					if (sscanf(ptr, " %d ", &reset->arg1) != 1) {
						error = TRUE;
					}
				}
				else if (strchr("O", reset->command) != NULL) {
					// 0-arg: nothing to do
				}
				else {	// all other types (1-arg?)
					if (sscanf(ptr, " %d ", &reset->arg1) != 1) {
						error = TRUE;
					}
				}

				if (error) {
					log("SYSERR: Format error in room %d, zone command: '%s'", vnum, line);
					exit(1);
				}
				
				break;
			}
			case 'D': {	// door
				setup_dir(fl, room, atoi(line + 1));
				break;
			}
			case 'E': {	// affects
				if (!get_line(fl, line2)) {
					log("SYSERR: Unable to get E line for room #%d", vnum);
					break;
				}

				ROOM_BASE_FLAGS(room) = asciiflag_conv(line2);
				ROOM_AFF_FLAGS(room) = ROOM_BASE_FLAGS(room);
				break;
			}
			case 'H': {	// home_room
				// ensure it has building data
				if (!COMPLEX_DATA(room)) {
					COMPLEX_DATA(room) = init_complex_data();
				}

				// actual home room will be set later
				add_trd_home_room(vnum, atoi(line + 1));
				break;
			}
			case 'I': {	// icon
				if (ROOM_CUSTOM_ICON(room)) {
					free(ROOM_CUSTOM_ICON(room));
				}
				ROOM_CUSTOM_ICON(room) = fread_string(fl, error_buf);
				break;
			}
			case 'M': {	// description
				if (ROOM_CUSTOM_DESCRIPTION(room)) {
					free(ROOM_CUSTOM_DESCRIPTION(room));
				}
				ROOM_CUSTOM_DESCRIPTION(room) = fread_string(fl, error_buf);
				break;
			}
			case 'N': {	// name
				if (ROOM_CUSTOM_NAME(room)) {
					free(ROOM_CUSTOM_NAME(room));
				}
				ROOM_CUSTOM_NAME(room) = fread_string(fl, error_buf);
				break;
			}
			case 'O': {	// owner (empire_vnum)
				add_trd_owner(vnum, atoi(line+1));
				break;
			}
			case 'P': { // crop (plants)
				set_crop_type(room, crop_proto(atoi(line+1)));
				break;
			}
			case 'R': {	/* resources */
				if (!COMPLEX_DATA(room)) {	// ensure complex data
					COMPLEX_DATA(room) = init_complex_data();
				}
				
				parse_resource(fl, &GET_BUILDING_RESOURCES(room), error_log);
				break;
			}
			
			case 'T': {	// trigger (deprecated)
				// NOTE: prior to b2.11, trigger prototypes were saved and read
				// this way they are no longer saved this way at all, but this
				// must be left in to be backwards-compatbile. If your mud has
				// been up since b2.11, you can safely remove this block.
				parse_trig_proto(line, &(room->proto_script), error_log);
				break;
			}
			
			case 'X': {	// resource depletion
				if (!get_line(fl, line2)) {
					log("SYSERR: Unable to read depletion line of room #%d", vnum);
					exit(1);
				}
				if ((sscanf(line2, "%d %d", t, t+1)) != 2) {
					log("SYSERR: Format in depletion line of room #%d", vnum);
					exit(1);
				}
				
				CREATE(dep, struct depletion_data, 1);
				dep->type = t[0];
				dep->count = t[1];
				dep->next = ROOM_DEPLETION(room);
				ROOM_DEPLETION(room) = dep;
				
				break;
			}
			
			case 'W': {	// built-with resources
				if (!COMPLEX_DATA(room)) {	// ensure complex data
					COMPLEX_DATA(room) = init_complex_data();
				}
				
				parse_resource(fl, &GET_BUILT_WITH(room), error_log);
				break;
			}
			
			case 'Y': {	// tracks
				if (!get_line(fl, line2) || sscanf(line2, "%d %d %ld %d", &t[0], &t[1], &l_in, &t[2]) != 4) {
					log("SYSERR: Bad formatting in Y section of room #%d", vnum);
					exit(1);
				}
				
				CREATE(track, struct track_data, 1);
				track->player_id = t[0];
				track->mob_num = t[1];
				track->timestamp = l_in;
				track->dir = t[2];
				
				LL_PREPEND(ROOM_TRACKS(room), track);
				break;
			}
			
			case 'Z': {	// extra data
				if (!get_line(fl, line2) || sscanf(line2, "%d %d", &t[0], &t[1]) != 2) {
					log("SYSERR: Bad formatting in Z section of room #%d", vnum);
					exit(1);
				}
				
				set_room_extra_data(room, t[0], t[1]);
				break;
			}

			case 'S': {	// end of room
				return;
			}
			default: {
				log("%s", error_log);
				exit(1);
			}
		}
	}
}


/**
* Outputs one room in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param room_data *room The thing to save.
*/
void write_room_to_file(FILE *fl, room_data *room) {
	extern bool objpack_save_room(room_data *room);
	
	struct cooldown_data *cool;
	struct trig_var_data *tvd;
	struct affected_type *af;
	trig_data *trig;
	char_data *mob;
	
	if (!fl || !room) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_room_to_file called without %s", !fl ? "file" : "room");
		return;
	}
	
	// save this room
	fprintf(fl, "#%d\n", GET_ROOM_VNUM(room));
	
	// both sector and original-sector must save vnums
	fprintf(fl, "%d %d %d\n", GET_ISLAND_ID(room), GET_SECT_VNUM(SECT(room)), GET_SECT_VNUM(BASE_SECT(room)));
	
	// A affects
	LL_FOREACH(ROOM_AFFECTS(room), af) {
		fprintf(fl, "A\n%d %d %ld %d %d %s\n", af->type, af->cast_by, af->duration, af->modifier, af->location, bitv_to_alpha(af->bitvector));
	}
	
	// B building data
	if (COMPLEX_DATA(room)) {
		fprintf(fl, "B\n%d %d %d %d %ld %.2f %d %d\n", BUILDING_VNUM(room), ROOM_TEMPLATE_VNUM(room), COMPLEX_DATA(room)->entrance, COMPLEX_DATA(room)->patron, COMPLEX_DATA(room)->burn_down_time, COMPLEX_DATA(room)->damage, COMPLEX_DATA(room)->private_owner, COMPLEX_DATA(room)->paint_color);
	}
	
	// C: load commands
	{
		// must save obj pack instruction BEFORE mob instruction
		if (objpack_save_room(room)) {
			// C O
			fprintf(fl, "C O\n");
		}
		
		if (SCRIPT(room)) {
			// triggers: C T type vnum
			for (trig = TRIGGERS(SCRIPT(room)); trig; trig = trig->next) {
				fprintf(fl, "C T %d %d\n", WLD_TRIGGER, GET_TRIG_VNUM(trig));
			}
			
			// triggers: C V
			LL_FOREACH(SCRIPT(room)->global_vars, tvd) {
				if (*tvd->name == '-' || !*tvd->value) { // don't save if it begins with - or is empty
					continue;
				}
				
				// trig-type 0 context name value
				fprintf(fl, "C V %d %d %d %79s %79s\n", WLD_TRIGGER, 0, (int)tvd->context, tvd->name, tvd->value);
			}
		}
		if (ROOM_PEOPLE(room)) {
			for (mob = ROOM_PEOPLE(room); mob; mob = mob->next_in_room) {
				if (mob && IS_NPC(mob) && !MOB_FLAGGED(mob, MOB_EMPIRE | MOB_FAMILIAR)) {
					// C M vnum flags rope-vnum
					fprintf(fl, "C M %d %s %d\n", GET_MOB_VNUM(mob), bitv_to_alpha(MOB_FLAGS(mob)), GET_ROPE_VNUM(mob));
					
					// C I instance_id
					if (MOB_INSTANCE_ID(mob) != NOTHING) {
						fprintf(fl, "C I %d\n", MOB_INSTANCE_ID(mob));
					}
				
					// C Y sex name 
					if (MOB_DYNAMIC_SEX(mob) != NOTHING || MOB_DYNAMIC_NAME(mob) != NOTHING || GET_LOYALTY(mob)) {
						fprintf(fl, "C Y %d %d %d\n", MOB_DYNAMIC_SEX(mob), MOB_DYNAMIC_NAME(mob), GET_LOYALTY(mob) ? EMPIRE_VNUM(GET_LOYALTY(mob)) : NOTHING);
					}
				
					// C C type time
					for (cool = mob->cooldowns; cool; cool = cool->next) {
						fprintf(fl, "C C %d %d\n", cool->type, (int)(cool->expire_time - time(0)));
					}
					
					if (SCRIPT(mob)) {
						// triggers: C T type vnum
						for (trig = TRIGGERS(SCRIPT(mob)); trig; trig = trig->next) {
							fprintf(fl, "C T %d %d\n", MOB_TRIGGER, GET_TRIG_VNUM(trig));
						}
						
						// triggers: C V type context name~
						//           value~
						LL_FOREACH(SCRIPT(mob)->global_vars, tvd) {
							if (*tvd->name == '-' || !*tvd->value) { // don't save if it begins with - or is empty
								continue;
							}
							
							// trig-type 0 context name value
							fprintf(fl, "C V %d %d %d %79s %79s\n", MOB_TRIGGER, 0, (int)tvd->context, tvd->name, tvd->value);
						}
					}
				}
			}
		}
	}
	
	// H home room
	if (HOME_ROOM(room) != room) {
		fprintf(fl, "H%d\n", GET_ROOM_VNUM(HOME_ROOM(room)));
	}
	
	// O owner
	if (ROOM_OWNER(room)) {
		fprintf(fl, "O%d\n", EMPIRE_VNUM(ROOM_OWNER(room)));
	}
	
	// P crop (plant)
	if (ROOM_CROP(room)) {
		fprintf(fl, "P%d\n", GET_CROP_VNUM(ROOM_CROP(room)));
	}
	
	// R resource
	if (COMPLEX_DATA(room) && BUILDING_RESOURCES(room)) {
		write_resources_to_file(fl, 'R', BUILDING_RESOURCES(room));
	}

	// D door
	if (COMPLEX_DATA(room)) {
		struct room_direction_data *ex;
		
		for (ex = COMPLEX_DATA(room)->exits; ex; ex = ex->next) {
			if (ex->keyword) {
				strcpy(buf2, ex->keyword);
			}
			else {
				*buf2 = 0;
			}

			fprintf(fl, "D%d\n%s~\n%s %d\n", ex->dir, buf2, bitv_to_alpha(ex->exit_info), ex->to_room);
		}
	}

	// NOTE: Prior to b2.11, this saved T as prototype triggers, but this is
	// no longer used: write_trig_protos_to_file(fl, 'T', room->proto_scripts);
	
	// W: built with
	if (COMPLEX_DATA(room) && GET_BUILT_WITH(room)) {
		write_resources_to_file(fl, 'W', GET_BUILT_WITH(room));
	}
	
	// Things that we prefer to save to the base map
	if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		write_shared_room_data(fl, SHARED_DATA(room));
	}
	
	// end
	fprintf(fl, "S\n");
}


/**
* Opens the world file for a specific world block, for writing.
*
* @param int block Which world block to open.
* @return FILE* An open write file. (Game exits on failure.)
*/
FILE *open_world_file(int block) {
	char filename[64];
	FILE *fl;
	
	sprintf(filename, "%s%d%s%s", WLD_PREFIX, block, WLD_SUFFIX, TEMP_SUFFIX);
	
	if (!(fl = fopen(filename, "w"))) {
		log("Unable to open: %s", filename);
		exit(1);
	}
	
	return fl;
}


/**
* Terminates, closes, and renames the world file for a given block. This file
* should have been opened using open_world_file().
*
* @param FILE *fl A file that was opened with open_world_file().
* @param int block The world block id passed to that function.
*/
void save_and_close_world_file(FILE *fl, int block) {
	char filename[64], tempname[64];
	
	if (!fl) {
		log("SYSERR: No file passed to save_and_close_world_file()");
		return;
	}
	
	sprintf(filename, "%s%d%s", WLD_PREFIX, block, WLD_SUFFIX);
	strcpy(tempname, filename);
	strcat(tempname, TEMP_SUFFIX);
	
	fprintf(fl, "$~\n");
	fclose(fl);
	rename(tempname, filename);
}


 //////////////////////////////////////////////////////////////////////////////
//// ROOM TEMPLATE LIB ///////////////////////////////////////////////////////

/**
* Puts a room template in the hash table.
*
* @param room_template *rmt The room template to add to the table.
*/
void add_room_template_to_table(room_template *rmt) {
	room_template *find;
	rmt_vnum vnum;
	
	if (rmt) {
		vnum = GET_RMT_VNUM(rmt);
		HASH_FIND_INT(room_template_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(room_template_table, vnum, rmt);
			HASH_SORT(room_template_table, sort_room_templates);
		}
	}
}

/**
* Removes a room template from the hash table.
*
* @param room_template *rmt The room template to remove from the table.
*/
void remove_room_template_from_table(room_template *rmt) {
	HASH_DEL(room_template_table, rmt);
}


/**
* frees up memory for a room template
*
* See also: olc_delete_room_template
*
* @param room_template *rmt The room template to free.
*/
void free_room_template(room_template *rmt) {
	room_template *proto = room_template_proto(GET_RMT_VNUM(rmt));
	struct interaction_item *interact;
	struct adventure_spawn *spawn;
	struct exit_template *ex;
	
	if (GET_RMT_TITLE(rmt) && (!proto || GET_RMT_TITLE(rmt) != GET_RMT_TITLE(proto))) {
		free(GET_RMT_TITLE(rmt));
	}
	if (GET_RMT_DESC(rmt) && (!proto || GET_RMT_DESC(rmt) != GET_RMT_DESC(proto))) {
		free(GET_RMT_DESC(rmt));
	}

	if (GET_RMT_SPAWNS(rmt) && (!proto || GET_RMT_SPAWNS(rmt) != GET_RMT_SPAWNS(proto))) {
		while ((spawn = GET_RMT_SPAWNS(rmt))) {
			GET_RMT_SPAWNS(rmt) = spawn->next;
			free(spawn);
		}
	}
	
	if (GET_RMT_EX_DESCS(rmt) && (!proto || GET_RMT_EX_DESCS(rmt) != GET_RMT_EX_DESCS(proto))) {
		free_extra_descs(&GET_RMT_EX_DESCS(rmt));
	}
	
	if (GET_RMT_EXITS(rmt) && (!proto || GET_RMT_EXITS(rmt) != GET_RMT_EXITS(proto))) {
		while ((ex = GET_RMT_EXITS(rmt))) {
			GET_RMT_EXITS(rmt) = ex->next;
			free_exit_template(ex);
		}
	}
	if (GET_RMT_INTERACTIONS(rmt) && (!proto || GET_RMT_INTERACTIONS(rmt) != GET_RMT_INTERACTIONS(proto))) {
		while ((interact = GET_RMT_INTERACTIONS(rmt))) {
			GET_RMT_INTERACTIONS(rmt) = interact->next;
			free(interact);
		}
	}
	
	if (GET_RMT_SCRIPTS(rmt) && (!proto || GET_RMT_SCRIPTS(rmt) != GET_RMT_SCRIPTS(proto))) {
		free_proto_scripts(&GET_RMT_SCRIPTS(rmt));
	}
	
	free(rmt);
}


/**
* Clears out the values of a room_template object.
*
* @param room_template *rmt The room template to clear.
*/
void init_room_template(room_template *rmt) {
	memset((char *)rmt, 0, sizeof(room_template));
}


/**
* Read one room template from file.
*
* @param FILE *fl The open .rmt file
* @param rmt_vnum vnum The room template vnum
*/
void parse_room_template(FILE *fl, rmt_vnum vnum) {
	int int_in[4];
	double dbl_in;
	char line[256], str_in[256], str_in2[256], str_in3[256];
	struct adventure_spawn *spawn, *last_spawn = NULL;
	struct exit_template *ex, *last_ex = NULL;
	room_template *rmt, *find;

	CREATE(rmt, room_template, 1);
	init_room_template(rmt);
	GET_RMT_VNUM(rmt) = vnum;

	HASH_FIND_INT(room_template_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate room template vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_room_template_to_table(rmt);
		
	// for error messages
	sprintf(buf2, "room template vnum %d", vnum);

	// lines 1-2
	GET_RMT_TITLE(rmt) = fread_string(fl, buf2);
	GET_RMT_DESC(rmt) = fread_string(fl, buf2);
	
	// line 3: flags base_affects [functions]
	if (!get_line(fl, line)) {
		log("SYSERR: Missing line 3 of %s", buf2);
		exit(1);
	}
	if (sscanf(line, "%s %s %s", str_in, str_in2, str_in3) == 3) {
		GET_RMT_FLAGS(rmt) = asciiflag_conv(str_in);
		GET_RMT_BASE_AFFECTS(rmt) = asciiflag_conv(str_in2);
		GET_RMT_FUNCTIONS(rmt) = asciiflag_conv(str_in3);
	}
	else if (sscanf(line, "%s %s", str_in, str_in2) == 2) {
		GET_RMT_FLAGS(rmt) = asciiflag_conv(str_in);
		GET_RMT_BASE_AFFECTS(rmt) = asciiflag_conv(str_in2);
	}
	else {
		log("SYSERR: Format error in line 3 of %s", buf2);
		exit(1);
	}
		
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
			exit(1);
		}
		switch (*line) {
			case 'D': {	// exit
				CREATE(ex, struct exit_template, 1);
				ex->next = NULL;
				if (last_ex) {
					last_ex->next = ex;
				}
				else {
					GET_RMT_EXITS(rmt) = ex;
				}
				last_ex = ex;
				
				ex->dir = atoi(line + 1);
				
				// line 1: keyword
				ex->keyword = fread_string(fl, buf2);
				
				// line 2: exit_info target_room
				if (!get_line(fl, line) || sscanf(line, "%s %d", str_in, &int_in[0]) != 2) {
					log("SYSERR: Format error in E line 1 of %s", buf2);
					exit(1);
				}
				
				ex->exit_info = asciiflag_conv(str_in);
				ex->target_room = int_in[0];
				break;
			}
			case 'E': {	// extra desc
				parse_extra_desc(fl, &GET_RMT_EX_DESCS(rmt), buf2);
				break;
			}
			case 'I': {	// interaction item
				parse_interaction(line, &GET_RMT_INTERACTIONS(rmt), buf2);
				break;
			}
			case 'M': {	// spawn
				CREATE(spawn, struct adventure_spawn, 1);
				spawn->next = NULL;
				if (last_spawn) {
					last_spawn->next = spawn;
				}
				else {
					GET_RMT_SPAWNS(rmt) = spawn;
				}
				last_spawn = spawn;

				// same line (don't read another): type vnum percent limit
				if (sscanf(line, "M %d %d %lf %d", &int_in[0], &int_in[1], &dbl_in, &int_in[2]) != 4) {
					log("SYSERR: Format error in M line of %s", buf2);
					exit(1);
				}
				
				spawn->type = int_in[0];
				spawn->vnum = int_in[1];
				spawn->percent = dbl_in;
				spawn->limit = int_in[2];
				break;
			}
			case 'T': {	// trigger
				parse_trig_proto(line, &GET_RMT_SCRIPTS(rmt), buf2);
				break;
			}

			// end
			case 'S': {
				return;
			}
			
			default: {
				log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
				exit(1);
			}
		}
	}
}


/**
* Outputs one room template in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param room_template *rmt The thing to save.
*/
void write_room_template_to_file(FILE *fl, room_template *rmt) {
	char temp[MAX_STRING_LENGTH], temp2[MAX_STRING_LENGTH], temp3[MAX_STRING_LENGTH];
	struct adventure_spawn *sp;
	struct exit_template *ex;
	
	if (!fl || !rmt) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_room_template_to_file called without %s", !fl ? "file" : "room template");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_RMT_VNUM(rmt));
	
	fprintf(fl, "%s~\n", NULLSAFE(GET_RMT_TITLE(rmt)));

	strcpy(temp, NULLSAFE(GET_RMT_DESC(rmt)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);

	strcpy(temp, bitv_to_alpha(GET_RMT_FLAGS(rmt)));
	strcpy(temp2, bitv_to_alpha(GET_RMT_BASE_AFFECTS(rmt)));
	strcpy(temp3, bitv_to_alpha(GET_RMT_FUNCTIONS(rmt)));
	fprintf(fl, "%s %s %s\n", temp, temp2, temp3);

	// D: exits
	for (ex = GET_RMT_EXITS(rmt); ex; ex = ex->next) {
		fprintf(fl, "D%d\n", ex->dir);
		fprintf(fl, "%s~\n", NULLSAFE(ex->keyword));
		fprintf(fl, "%s %d\n", bitv_to_alpha(ex->exit_info), ex->target_room);
	}
	
	// E: extra descriptions
	write_extra_descs_to_file(fl, GET_RMT_EX_DESCS(rmt));
	
	// I: interactions
	write_interactions_to_file(fl, GET_RMT_INTERACTIONS(rmt));
	
	// M: spawns
	for (sp = GET_RMT_SPAWNS(rmt); sp; sp = sp->next) {
		fprintf(fl, "M %d %d %.2f %d\n", sp->type, sp->vnum, sp->percent, sp->limit);
	}
	
	// T: triggers
	write_trig_protos_to_file(fl, 'T', GET_RMT_SCRIPTS(rmt));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// SECTOR LIB //////////////////////////////////////////////////////////////

/**
* Puts a sector in the hash table.
*
* @param sector_data *sect The sect to add to the table.
*/
void add_sector_to_table(sector_data *sect) {
	extern int sort_sectors(void *a, void *b);
	
	sector_data *find;
	sector_vnum vnum;
	
	if (sect) {
		vnum = GET_SECT_VNUM(sect);
		HASH_FIND_INT(sector_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(sector_table, vnum, sect);
			HASH_SORT(sector_table, sort_sectors);
		}
	}
}

/**
* Removes a sector from the hash table.
*
* @param sector_data *sect The sect to remove from the table.
*/
void remove_sector_from_table(sector_data *sect) {
	HASH_DEL(sector_table, sect);
}


/**
* frees up the memory for a sector item
*
* See also: olc_delete_sector
*
* @param sector_data *st The sector to free.
*/
void free_sector(sector_data *st) {
	struct interaction_item *interact;
	struct evolution_data *evo;
	struct spawn_info *spawn;
	sector_data *proto;
	
	proto = sector_proto(GET_SECT_VNUM(st));
	
	if (GET_SECT_NAME(st) && (!proto || GET_SECT_NAME(st) != GET_SECT_NAME(proto))) {
		free(GET_SECT_NAME(st));
	}
	if (GET_SECT_TITLE(st) && (!proto || GET_SECT_TITLE(st) != GET_SECT_TITLE(proto))) {
		free(GET_SECT_TITLE(st));
	}
	if (GET_SECT_COMMANDS(st) && (!proto || GET_SECT_COMMANDS(st) != GET_SECT_COMMANDS(proto))) {
		free(GET_SECT_COMMANDS(st));
	}
	
	if (GET_SECT_ICONS(st) && (!proto || GET_SECT_ICONS(st) != GET_SECT_ICONS(proto))) {
		free_icon_set(&GET_SECT_ICONS(st));
	}
	
	if (GET_SECT_SPAWNS(st) && (!proto || GET_SECT_SPAWNS(st) != GET_SECT_SPAWNS(proto))) {
		while ((spawn = GET_SECT_SPAWNS(st))) {
			GET_SECT_SPAWNS(st) = spawn->next;
			free(spawn);
		}
	}
	
	if (GET_SECT_EVOS(st) && (!proto || GET_SECT_EVOS(st) != GET_SECT_EVOS(proto))) {
		while ((evo = GET_SECT_EVOS(st))) {
			GET_SECT_EVOS(st) = evo->next;
			free(evo);
		}
	}

	if (GET_SECT_INTERACTIONS(st) && (!proto || GET_SECT_INTERACTIONS(st) != GET_SECT_INTERACTIONS(proto))) {
		while ((interact = GET_SECT_INTERACTIONS(st))) {
			GET_SECT_INTERACTIONS(st) = interact->next;
			free(interact);
		}
	}

	free(st);
}


/**
* Clears out the data of a sector_data object.
*
* @param sector_data *st The sector item to clear.
*/
void init_sector(sector_data *st) {
	memset((char *)st, 0, sizeof(sector_data));
}


/**
* Read one sector from file.
*
* @param FILE *fl The open .bld file
* @param sector_vnum vnum The sector vnum
*/
void parse_sector(FILE *fl, sector_vnum vnum) {
	char line[256], str_in[256], str_in2[256], char_in[2];
	struct evolution_data *evo, *last_evo = NULL;
	struct spawn_info *spawn, *stemp;
	sector_data *sect, *find;
	double dbl_in;
	int int_in[4];
		
	// for error messages
	sprintf(buf2, "sector vnum %d", vnum);
	
	// create
	CREATE(sect, sector_data, 1);
	init_sector(sect);
	sect->vnum = vnum;

	HASH_FIND_INT(sector_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate sector vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_sector_to_table(sect);
	
	// lines 1-2
	GET_SECT_NAME(sect) = fread_string(fl, buf2);
	GET_SECT_TITLE(sect) = fread_string(fl, buf2);
	
	// line 3: roadside, mapout, climate, movement, flags, build flags
	if (!get_line(fl, line) || sscanf(line, "'%c' %d %d %d %s %s", &char_in[0], &int_in[0], &int_in[1], &int_in[2], str_in, str_in2) != 6) {
		log("SYSERR: Format error in line 3 of %s", buf2);
		exit(1);
	}
	
	GET_SECT_ROADSIDE_ICON(sect) = char_in[0];
	GET_SECT_MAPOUT(sect) = int_in[0];
	GET_SECT_CLIMATE(sect) = int_in[1];
	GET_SECT_MOVE_LOSS(sect) = int_in[2];
	GET_SECT_FLAGS(sect) = asciiflag_conv(str_in);
	GET_SECT_BUILD_FLAGS(sect) = asciiflag_conv(str_in2);
		
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
			exit(1);
		}
		switch (*line) {
			// commands
			case 'C': {
				GET_SECT_COMMANDS(sect) = fread_string(fl, buf2);
				break;
			}
			
			// tile sets
			case 'D': {
				parse_icon(line, fl, &GET_SECT_ICONS(sect), buf2);
				break;
			}
			
			// evolution
			case 'E': {
				if (!get_line(fl, line) || sscanf(line, "%d %d %lf %d", &int_in[0], &int_in[1], &dbl_in, &int_in[2]) != 4) {
					log("SYSERR: Bad data in E line of %s", buf2);
					exit(1);
				}
				
				CREATE(evo, struct evolution_data, 1);
				evo->type = int_in[0];
				evo->value = int_in[1];
				evo->percent = dbl_in;
				evo->becomes = int_in[2];
				evo->next = NULL;
				
				if (last_evo) {
					last_evo->next = evo;
				}
				else {
					GET_SECT_EVOS(sect) = evo;
				}
				last_evo = evo;
				break;
			}
	
			case 'I': {	// interaction item
				parse_interaction(line, &GET_SECT_INTERACTIONS(sect), buf2);
				break;
			}
			
			// mob spawn
			case 'M': {
				if (!get_line(fl, line) || sscanf(line, "%d %lf %s", &int_in[0], &dbl_in, str_in) != 3) {
					log("SYSERR: Format error in M line of %s", buf2);
					exit(1);
				}
				
				CREATE(spawn, struct spawn_info, 1);
				spawn->vnum = int_in[0];
				spawn->percent = dbl_in;
				spawn->flags = asciiflag_conv(str_in);
				spawn->next = NULL;
				
				// append at end
				if ((stemp = GET_SECT_SPAWNS(sect))) {
					while (stemp->next) {
						stemp = stemp->next;
					}
					stemp->next = spawn;
				}
				else {
					GET_SECT_SPAWNS(sect) = spawn;
				}
				break;
			}

			// end
			case 'S': {
				return;
			}
			
			default: {
				log("SYSERR: Format error in %s, expecting alphabetic flags", buf2);
				exit(1);
			}
		}
	}
}


/**
* Outputs one sector in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param sector_data *st The thing to save.
*/
void write_sector_to_file(FILE *fl, sector_data *st) {
	char temp[64], temp2[64];
	struct evolution_data *evo;
	struct spawn_info *spawn;
	
	if (!fl || !st) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_sector_to_file called without %s", !fl ? "file" : "sector");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_SECT_VNUM(st));
	
	fprintf(fl, "%s~\n", NULLSAFE(GET_SECT_NAME(st)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_SECT_TITLE(st)));

	strcpy(temp, bitv_to_alpha(GET_SECT_FLAGS(st)));
	strcpy(temp2, bitv_to_alpha(GET_SECT_BUILD_FLAGS(st)));
	fprintf(fl, "'%c' %d %d %d %s %s\n", GET_SECT_ROADSIDE_ICON(st), GET_SECT_MAPOUT(st), GET_SECT_CLIMATE(st), GET_SECT_MOVE_LOSS(st), temp, temp2);
	
	// C: commands list
	if (GET_SECT_COMMANDS(st) && *GET_SECT_COMMANDS(st)) {
		fprintf(fl, "C\n");
		fprintf(fl, "%s~\n", GET_SECT_COMMANDS(st));
	}
	
	// D: tile sets
	write_icons_to_file(fl, 'D', GET_SECT_ICONS(st));
	
	// E: evolution
	for (evo = GET_SECT_EVOS(st); evo; evo = evo->next) {
		fprintf(fl, "E\n");
		fprintf(fl, "%d %d %.2f %d\n", evo->type, evo->value, evo->percent, evo->becomes);
	}
	
	// I: interactions
	write_interactions_to_file(fl, GET_SECT_INTERACTIONS(st));
	
	// M: mob spawns
	for (spawn = GET_SECT_SPAWNS(st); spawn; spawn = spawn->next) {
		fprintf(fl, "M\n");
		fprintf(fl, "%d %.2f %s\n", spawn->vnum, spawn->percent, bitv_to_alpha(spawn->flags));
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// STORED EVENT LIB ////////////////////////////////////////////////////////

// cancel func externs
EVENT_CANCEL_FUNC(cancel_burn_event);
EVENT_CANCEL_FUNC(cancel_map_event);
EVENT_CANCEL_FUNC(cancel_room_event);


// SEV_x: list of cancel functions
struct stored_event_info_t stored_event_info[] = {
	{ cancel_map_event },	// SEV_TRENCH_FILL
		{ NULL },	// unused
	{ cancel_burn_event },	// SEV_BURN_DOWN
	{ cancel_map_event },	// SEV_GROW_CROP
	{ cancel_room_event },	// SEV_TAVERN
	{ cancel_room_event },	// SEV_RESET_TRIGGER
};


/**
* Adds a stored event to a list. If there's already one of the same type in
* the list, it will be replaced.
*
* @param struct stored_event **list The list to add to.
* @param int type The SEV_ type to add the event as.
* @param struct event *event The event to store.
*/
void add_stored_event(struct stored_event **list, int type, struct event *event) {
	struct stored_event *sev;
	
	if (!event) {
		return;
	}
	
	// ensure no duplicate entries
	sev = find_stored_event(*list, type);
	if (!sev) {
		CREATE(sev, struct stored_event, 1);
		sev->type = type;
		HASH_ADD_INT(*list, type, sev);
	}
	
	sev->ev = event;
}


/**
* Cancels an event that was stored, and deletes the entry.
*
* @param struct stored_event **list The list to cancel out of.
* @param int type The SEV_ type to cancel.
*/
void cancel_stored_event(struct stored_event **list, int type) {
	struct stored_event *sev = find_stored_event(*list, type);
	
	if (sev && sev->ev) {
		event_cancel(sev->ev, stored_event_info[type].cancel);
		sev->ev = NULL;
	}
	
	if (sev) {
		HASH_DEL(*list, sev);
		free(sev);
	}
}


/**
* Deletes a stored event without canceling it.
*
* @param struct stored_event **list The list to delete from.
* @param int type The SEV_ type to cancel.
*/
void delete_stored_event(struct stored_event **list, int type) {
	struct stored_event *sev = find_stored_event(*list, type);
	if (sev) {
		HASH_DEL(*list, sev);
		free(sev);
	}
}


/**
* Finds a stored event in a list of them.
*
* @param struct stored_event *list The list to search in.
* @param int type Any SEV_ const.
* @return struct stored_event* The found event if any, or NULL.
*/
struct stored_event *find_stored_event(struct stored_event *list, int type) {
	struct stored_event *find;
	
	HASH_FIND_INT(list, &type, find);
	return find;
}


// frees memory when a map event is canceled
EVENT_CANCEL_FUNC(cancel_map_event) {
	struct map_event_data *data = (struct map_event_data *)event_obj;
	free(data);
}


// generic canceller for room events) {
EVENT_CANCEL_FUNC(cancel_room_event) {
	struct room_event_data *data = (struct room_event_data *)event_obj;
	free(data);
}


 //////////////////////////////////////////////////////////////////////////////
//// THEFT LIB ///////////////////////////////////////////////////////////////

/**
* Frees up a list of theft logs.
*
* struct theft_log *list The list to free.
*/
void free_theft_logs(struct theft_log *list) {
	struct theft_log *iter;
	
	while ((iter = list)) {
		list = list->next;
		free(iter);
	}
}


/**
* Turns theft logs into consolidated empire theft reports.
*/
void process_theft_logs(void) {
	char buf[MAX_STRING_LENGTH], temp[MAX_STRING_LENGTH];
	struct theft_log *list, *iter;
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (!EMPIRE_THEFT_LOGS(emp)) {
			continue;	// short-circuit
		}
		
		list = reduce_theft_logs_and_get_recent(emp);
		*buf = '\0';
		LL_FOREACH(list, iter) {	// build the buffer to report
			if (iter->vnum == NOTHING) {
				sprintf(temp, "coin%s x%d", PLURAL(iter->amount), iter->amount);
			}
			else {
				sprintf(temp, "%s x%d", skip_filler(get_obj_name_by_proto(iter->vnum)), iter->amount);
			}
			
			if (!*buf || strlen(buf) + strlen(temp) + 2 < 150) {
				// just add to buffer
				sprintf(buf + strlen(buf), "%s%s", (*buf ? ", " : "Recently stolen: "), temp);
			}
			else {
				// overflow: report current then start new buffer
				log_to_empire(emp, ELOG_HOSTILITY, "%s", buf);
				sprintf(buf, "Recently stolen: %s", temp);
			}
		}
		
		// anything left to report?
		if (*buf) {
			log_to_empire(emp, ELOG_HOSTILITY, "%s", buf);
		}
		
		free_theft_logs(list);
	}
}


/**
* Records a theft to an empire for delayed logging.
*
* @param empire_data *emp Which empire.
* @param obj_vnum vnum What item was stolen (use NOTHING for coins).
* @param int amount How much/many was/were taken.
*/
void record_theft_log(empire_data *emp, obj_vnum vnum, int amount) {
	long time_minutes = time(0) / 60;
	struct theft_log *tft, *iter;
	bool found = FALSE;
	
	if (!emp || amount < 1) {
		return;	// wut?
	}
	
	LL_FOREACH(EMPIRE_THEFT_LOGS(emp), iter) {
		if (iter->time_minutes < time_minutes) {
			// always stored in descending order of time for fast adds
			break;
		}
		if (iter->vnum == vnum) {
			iter->amount += amount;
			EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
			found = TRUE;
			break;
		}
	}
	
	if (!found) {
		CREATE(tft, struct theft_log, 1);
		tft->vnum = vnum;
		tft->amount = amount;
		tft->time_minutes = time_minutes;
		LL_PREPEND(EMPIRE_THEFT_LOGS(emp), tft);
		EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	}
}


/**
* Filters recent thefts from an empire's theft log, EXCLUDING the last ~5
* minutes, which are not reported to give some anonymity. This generates a NEW
* list (which must be freed with free_theft_logs) that is consolidated so that
* each vnum only appears once, with the total stolen.
*
* @param empire_data *emp Which empire to get theft logs for.
* @return struct theft_log* The new list of recent thefts to report.
*/
struct theft_log *reduce_theft_logs_and_get_recent(empire_data *emp) {
	struct theft_log *iter, *next_iter, *tft, *recent = NULL;
	long threshold = (time(0) / 60) - 4;	// more than 4 minutes ago
	
	LL_FOREACH_SAFE(EMPIRE_THEFT_LOGS(emp), iter, next_iter) {
		if (iter->time_minutes > threshold) {
			continue;	// skip recent
		}
		
		// ok, found, so we're gonna use this one
		LL_DELETE(EMPIRE_THEFT_LOGS(emp), iter);
		LL_SEARCH_SCALAR(EMPIRE_THEFT_LOGS(emp), tft, vnum, iter->vnum);
		if (!tft) {
			CREATE(tft, struct theft_log, 1);
			tft->vnum = iter->vnum;
			// prepend will essentially flip the order so older thefts are earlier
			LL_PREPEND(recent, tft);
		}
		tft->amount += iter->amount;
		free(iter);
	}
	
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	return recent;
}


 //////////////////////////////////////////////////////////////////////////////
//// TRIGGER LIB /////////////////////////////////////////////////////////////

/**
* Puts a trigger in the hash table.
*
* @param trig_data *trig The trigger to add to the table.
*/
void add_trigger_to_table(trig_data *trig) {
	extern int sort_triggers(trig_data *a, trig_data *b);
	
	trig_data *find;
	trig_vnum vnum;
	
	if (trig) {
		vnum = GET_TRIG_VNUM(trig);
		HASH_FIND_INT(trigger_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(trigger_table, vnum, trig);
			HASH_SORT(trigger_table, sort_triggers);
		}
	}
}

/**
* Removes a trigger from the hash table.
*
* @param trig_data *trig The trigger to remove from the table.
*/
void remove_trigger_from_table(trig_data *trig) {
	HASH_DEL(trigger_table, trig);
}


/**
* Writes a trigger proto list to a data file.
*
* @param FILE *fl The file open for writing.
* @param char letter The file tag (almost always 'T' for triggers).
* @param struct trig_proto_list *list The list to write to the file.
*/
void write_trig_protos_to_file(FILE *fl, char letter, struct trig_proto_list *list) {
	struct trig_proto_list *iter;
	
	LL_FOREACH(list, iter) {
		fprintf(fl, "%c %d\n", letter, iter->vnum);
	}
}


/**
* Outputs one trigger in the db file format, starting with a #VNUM and
* ending with a ~.
*
* @param FILE *fl The file to write it to.
* @param trig_data *trig The thing to save.
*/
void write_trigger_to_file(FILE *fl, trig_data *trig) {
	struct cmdlist_element *cmd;
	char lbuf[MAX_CMD_LENGTH+1];
	
	if (!fl || !trig) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_trigger_to_file called without %s", !fl ? "file" : "trigger");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_TRIG_VNUM(trig));
	
	fprintf(fl, "%s~\n", NULLSAFE(GET_TRIG_NAME(trig)));
	fprintf(fl, "%d %s %d\n", trig->attach_type, bitv_to_alpha(GET_TRIG_TYPE(trig)), GET_TRIG_NARG(trig));
	fprintf(fl, "%s~\n", NULLSAFE(GET_TRIG_ARG(trig)));

	// Build the text for the script
	strcpy(lbuf, ""); /* strcpy OK for MAX_CMD_LENGTH > 0*/
	for (cmd = trig->cmdlist; cmd; cmd = cmd->next) {
		strcat(lbuf, cmd->cmd);
		strcat(lbuf, "\n");
	}

	if (!*lbuf) {
		strcpy(lbuf, "* Empty script");
	}

	fprintf(fl, "%s~\n", lbuf);

	// triggers do NOT have the trailing 'S'
}


 //////////////////////////////////////////////////////////////////////////////
//// CORE I/O FUNCTIONS //////////////////////////////////////////////////////

/**
* Discrete load processes a data file, finds #VNUM records in it, and sends
* them off to various parse functions.
*
* @param FILE *fl The file to read.
* @param int mode Any DB_BOOT_ const.
* @param char *filename The name of the file, for error reporting.
*/
void discrete_load(FILE *fl, int mode, char *filename) {
	void parse_ability(FILE *fl, any_vnum vnum);
	void parse_account(FILE *fl, int nr);
	void parse_archetype(FILE *fl, any_vnum vnum);
	void parse_augment(FILE *fl, any_vnum vnum);
	void parse_book(FILE *fl, int book_id);
	void parse_class(FILE *fl, any_vnum vnum);
	void parse_morph(FILE *fl, any_vnum vnum);
	void parse_progress(FILE *fl, any_vnum vnum);
	void parse_quest(FILE *fl, any_vnum vnum);
	void parse_skill(FILE *fl, any_vnum vnum);
	void parse_social(FILE *fl, any_vnum vnum);
	void parse_vehicle(FILE *fl, any_vnum vnum);
	
	any_vnum nr = -1, last;
	char line[256];

	/* modes positions correspond to DB_BOOT_x in db.h */
	const char *modes[] = {"world", "mob", "obj", "zone", "empire", "book", "craft", "trg", "crop", "sector", "adventure", "room template", "global", "account", "augment", "archetype", "ability", "class", "skill", "vehicle", "morph", "quest", "social", "faction", "generic", "shop", "progress" };

	for (;;) {
		if (!get_line(fl, line)) {
			if (nr == -1)
				log("SYSERR: %s file %s is empty!", modes[mode], filename);
			else {
				log("SYSERR: Format error in %s after %s #%d\n...expecting a new %s, but file ended!\n(maybe the file is not terminated with '$'?)", filename, modes[mode], nr, modes[mode]);
			}
			exit(1);
		}

		if (*line == '$')
			return;

		if (*line == '#') {
			last = nr;
			if (sscanf(line, "#%d", &nr) != 1) {
				log("SYSERR: Format error after %s #%d", modes[mode], last);
				exit(1);
			}
			// DB_BOOT_x
			switch (mode) {
				case DB_BOOT_ABIL: {
					parse_ability(fl, nr);
					break;
				}
				case DB_BOOT_ACCT: {
					parse_account(fl, nr);
					break;
				}
				case DB_BOOT_ADV: {
					parse_adventure(fl, nr);
					break;
				}
				case DB_BOOT_ARCH: {
					parse_archetype(fl, nr);
					break;
				}
				case DB_BOOT_AUG: {
					parse_augment(fl, nr);
					break;
				}
				case DB_BOOT_BLD: {
					parse_building(fl, nr);
					break;
				}
				case DB_BOOT_CLASS: {
					parse_class(fl, nr);
					break;
				}
				case DB_BOOT_CRAFT: {
					parse_craft(fl, nr);
					break;
				}
				case DB_BOOT_CROP: {
					parse_crop(fl, nr);
					break;
				}
				case DB_BOOT_FCT: {
					void parse_faction(FILE *fl, int nr);
					parse_faction(fl, nr);
					break;
				}
				case DB_BOOT_GEN: {
					void parse_generic(FILE *fl, int nr);
					parse_generic(fl, nr);
					break;
				}
				case DB_BOOT_GLB: {
					parse_global(fl, nr);
					break;
				}
				case DB_BOOT_WLD:
					parse_room(fl, nr);
					break;
				case DB_BOOT_MOB:
					parse_mobile(fl, nr);
					break;
				case DB_BOOT_MORPH: {
					parse_morph(fl, nr);
					break;
				}
				case DB_BOOT_PRG: {
					parse_progress(fl, nr);
					break;
				}
				case DB_BOOT_EMP:
					parse_empire(fl, nr);
					break;
				case DB_BOOT_OBJ:
					parse_object(fl, nr);
					break;
				case DB_BOOT_BOOKS: {
					parse_book(fl, nr);
					break;
				}
				case DB_BOOT_QST: {
					parse_quest(fl, nr);
					break;
				}
				case DB_BOOT_RMT: {
					parse_room_template(fl, nr);
					break;
				}
				case DB_BOOT_SECTOR: {
					parse_sector(fl, nr);
					break;
				}
				case DB_BOOT_SHOP: {
					void parse_shop(FILE *fl, int nr);
					parse_shop(fl, nr);
					break;
				}
				case DB_BOOT_SKILL: {
					parse_skill(fl, nr);
					break;
				}
				case DB_BOOT_SOC: {
					parse_social(fl, nr);
					break;
				}
				case DB_BOOT_TRG: {
					parse_trigger(fl, nr);
					break;
				}
				case DB_BOOT_VEH: {
					parse_vehicle(fl, nr);
					break;
				}
				default: {
					log("SYSERR: discrete_load not implemented for mode %d", mode);
					exit(1);
				}
			}
		}
		else {
			log("SYSERR: Format error in %s file %s near %s #%d", modes[mode], filename, modes[mode], nr);
			log("SYSERR: ... offending line: '%s'", line);
			exit(1);
		}
	}
}


/**
* index_boot: Loads an index file for a given type, and loads each entry from
* the index using discrete_load.
*
* @param int mode Any DB_BOOT_ const.
*/
void index_boot(int mode) {
	char buf1[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
	const char *index_filename, *prefix = NULL;
	FILE *index, *db_file;
	int rec_count = 0, size[2];

	if (mode >= 0 && mode < NUM_DB_BOOT_TYPES && db_boot_info[mode].prefix) {
		prefix = db_boot_info[mode].prefix;
	}
	else {
		log("SYSERR: Unknown subcommand %d to index_boot: prefix not defined!", mode);
		exit(1);
	}

    index_filename = INDEX_FILE;

	sprintf(buf2, "%s%s", prefix, index_filename);

	if (!(index = fopen(buf2, "r"))) {
		log("SYSERR: opening index file '%s': %s", buf2, strerror(errno));
		exit(1);
	}

	/* first, count the number of records in the file so we can malloc */
	fscanf(index, "%s\n", buf1);
	while (*buf1 != '$') {
		sprintf(buf2, "%s%s", prefix, buf1);
		if (!(db_file = fopen(buf2, "r"))) {
			log("SYSERR: File '%s' listed in '%s%s': %s", buf2, prefix, index_filename, strerror(errno));
			fscanf(index, "%s\n", buf1);
			continue;
		}
		else {
			rec_count += count_hash_records(db_file);
		}

		fclose(db_file);
		fscanf(index, "%s\n", buf1);
	}

	if (!rec_count) {
		if (db_boot_info[mode].allow_zero) {
			// types that don't require any entries and exit early if none
			return;
		}
		else if (mode == DB_BOOT_NAMES) {
			// types that continue even if no #
		}
		else {
			// types that do require entries
			log("SYSERR: boot error - 0 records counted in %s%s.", prefix, index_filename);
			exit(1);
		}
	}
	
	/*
	 * DB_BOOT_x:
	 * NOTE: "bytes" does _not_ include strings or other later malloc'd things.
	 */
	switch (mode) {
		case DB_BOOT_ABIL: {
			size[0] = sizeof(ability_data) * rec_count;
			log("  %d abilities, %d bytes in db", rec_count, size[0]);
			break;
		}
		case DB_BOOT_ARCH: {
			size[0] = sizeof(archetype_data) * rec_count;
			log("  %d archetypes, %d bytes in db", rec_count, size[0]);
			break;
		}
		case DB_BOOT_AUG: {
			size[0] = sizeof(augment_data) * rec_count;
			log("  %d augments, %d bytes in db", rec_count, size[0]);
			break;
		}
		case DB_BOOT_BOOKS: {
			log("  %d books", rec_count);
			break;
		}
		case DB_BOOT_MOB:
   			size[0] = sizeof(char_data) * rec_count;
			log("   %d mobs, %d bytes in prototypes.", rec_count, size[0]);
			break;
		case DB_BOOT_ACCT: {
			size[0] = sizeof(account_data) * rec_count;
			log("   %d accounts, %d bytes in index.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_ADV: {
			size[0] = sizeof(adv_data) * rec_count;
			log("   %d adventure zones, %d bytes in adventure table.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_BLD: {
			size[0] = sizeof(bld_data) * rec_count;
			log("   %d buildings, %d bytes in building table.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_CLASS: {
			size[0] = sizeof(class_data) * rec_count;
			log("  %d classes, %d bytes in db", rec_count, size[0]);
			break;
		}
		case DB_BOOT_CRAFT: {
			size[0] = sizeof(craft_data) * rec_count;
			log("   %d crafts, %d bytes in craft_table.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_CROP: {
			size[0] = sizeof(crop_data) * rec_count;
			log("   %d crops, %d bytes in crop table.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_FCT: {
			size[0] = sizeof(faction_data) * rec_count;
			log("   %d factions, %d bytes in factions table.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_GEN: {
			size[0] = sizeof(generic_data) * rec_count;
			log("   %d generics, %d bytes in generics table.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_GLB: {
			size[0] = sizeof(struct global_data) * rec_count;
			log("   %d globals, %d bytes in globals table.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_MORPH: {
   			size[0] = sizeof(morph_data) * rec_count;
			log("   %d morphs, %d bytes in prototypes.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_NAMES: {
			log("   %d name lists.", rec_count);
			break;
		}
		case DB_BOOT_PRG: {
   			size[0] = sizeof(progress_data) * rec_count;
			log("   %d progression goals, %d bytes in prototypes.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_OBJ:
			size[0] = sizeof(obj_data) * rec_count;
			log("   %d objs, %d bytes in prototypes.", rec_count, size[0]);
			break;
		case DB_BOOT_QST: {
   			size[0] = sizeof(quest_data) * rec_count;
			log("   %d quests, %d bytes in prototypes.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_EMP: {
			size[0] = sizeof(empire_data) * rec_count;
			log("   %d empires, %d bytes.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_RMT: {
			size[0] = sizeof(room_template) * rec_count;
			log("   %d room templates, %d bytes in room_template table.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_SECTOR: {
			size[0] = sizeof(sector_data) * rec_count;
			log("   %d sectors, %d bytes in sector.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_SHOP: {
			size[0] = sizeof(shop_data) * rec_count;
			log("   %d shops, %d bytes in shops table.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_SKILL: {
			size[0] = sizeof(skill_data) * rec_count;
			log("  %d skills, %d bytes in db", rec_count, size[0]);
			break;
		}
		case DB_BOOT_SOC: {
			size[0] = sizeof(social_data) * rec_count;
			log("  %d socials, %d bytes in db", rec_count, size[0]);
			break;
		}
		case DB_BOOT_TRG: {
			size[0] = sizeof(trig_data) * rec_count;
			log("   %d triggers, %d bytes in triggers.", rec_count, size[0]);
			break;
		}
		case DB_BOOT_VEH: {
			size[0] = sizeof(vehicle_data) * rec_count;
			log("   %d vehicles, %d bytes in db.", rec_count, size[0]);
			break;
		}
	}
	rewind(index);
	fscanf(index, "%s\n", buf1);
	while (*buf1 != '$') {
		sprintf(buf2, "%s%s", prefix, buf1);
		if (!(db_file = fopen(buf2, "r"))) {
			log("SYSERR: %s: %s", buf2, strerror(errno));
			exit(1);
		}
		// DB_BOOT_x
		switch (mode) {
			case DB_BOOT_ABIL:
			case DB_BOOT_ACCT:
			case DB_BOOT_ADV:
			case DB_BOOT_ARCH:
			case DB_BOOT_AUG:
			case DB_BOOT_BLD:
			case DB_BOOT_CLASS:
			case DB_BOOT_CRAFT:
			case DB_BOOT_CROP:
			case DB_BOOT_FCT:
			case DB_BOOT_GEN:
			case DB_BOOT_GLB:
			case DB_BOOT_OBJ:
			case DB_BOOT_MOB:
			case DB_BOOT_MORPH:
			case DB_BOOT_PRG:
			case DB_BOOT_EMP:
			case DB_BOOT_BOOKS:
			case DB_BOOT_QST:
			case DB_BOOT_RMT:
			case DB_BOOT_SECTOR:
			case DB_BOOT_SHOP:
			case DB_BOOT_SKILL:
			case DB_BOOT_SOC:
			case DB_BOOT_TRG:
			case DB_BOOT_VEH:
			case DB_BOOT_WLD: {
				discrete_load(db_file, mode, buf2);
				break;
			}
			case DB_BOOT_NAMES: {
				parse_generic_name_file(db_file, buf2);
				break;
			}
		}

		fclose(db_file);
		fscanf(index, "%s\n", buf1);
	}
	fclose(index);
}


/**
* This writes the full data file for any library type (building, sector, etc).
* You don't have to call it with a real vnum; it just uses the vnum to
* determine which "zone" (block of 100) items to save to file.
*
* @param int type Any DB_BOOT_ const.
* @param any_vnum vnum A vnum in the set to save (it saved the whole 100-block).
*/
void save_library_file_for_vnum(int type, any_vnum vnum) {
	char filename[64], tempname[64], *prefix = NULL, *suffix = NULL;
	int zone = vnum / 100;
	FILE *fl;
	
	// setup
	if (type >= 0 && type < NUM_DB_BOOT_TYPES && db_boot_info[type].prefix && db_boot_info[type].suffix) {
		prefix = db_boot_info[type].prefix;
		suffix = db_boot_info[type].suffix;
	}
	else {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Unable to write library file (1) for invalid type %d", type);
		// early exit -- nothing to do
		return;
	}
	
	sprintf(filename, "%s%d%s", prefix, zone, suffix);
	strcpy(tempname, filename);
	strcat(tempname, TEMP_SUFFIX);
	
	if (!(fl = fopen(tempname, "w"))) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Unable to write db file '%s': %s", filename, strerror(errno));
		return;
	}
	
	// log("Saving library file %s", filename);
	
	// DB_BOOT_x
	switch (type) {
		case DB_BOOT_ABIL: {
			void write_ability_to_file(FILE *fl, ability_data *abil);
			ability_data *abil, *next_abil;
			HASH_ITER(hh, ability_table, abil, next_abil) {
				if (ABIL_VNUM(abil) >= (zone * 100) && ABIL_VNUM(abil) <= (zone * 100 + 99)) {
					write_ability_to_file(fl, abil);
				}
			}
			break;
		}
		case DB_BOOT_ACCT: {
			void write_account_to_file(FILE *fl, account_data *acct);
			account_data *acct, *next_acct;
			HASH_ITER(hh, account_table, acct, next_acct) {
				if (acct->id >= (zone * 100) && acct->id <= (zone * 100 + 99)) {
					write_account_to_file(fl, acct);
				}
			}
			break;
		}
		case DB_BOOT_ADV: {
			adv_data *adv, *next_adv;
			HASH_ITER(hh, adventure_table, adv, next_adv) {
				if (GET_ADV_VNUM(adv) >= (zone * 100) && GET_ADV_VNUM(adv) <= (zone * 100 + 99)) {
					write_adventure_to_file(fl, adv);
				}
			}
			break;
		}
		case DB_BOOT_ARCH: {
			void write_archetype_to_file(FILE *fl, archetype_data *arch);
			archetype_data *arch, *next_arch;
			HASH_ITER(hh, archetype_table, arch, next_arch) {
				if (GET_ARCH_VNUM(arch) >= (zone * 100) && GET_ARCH_VNUM(arch) <= (zone * 100 + 99)) {
					write_archetype_to_file(fl, arch);
				}
			}
			break;
		}
		case DB_BOOT_AUG: {
			void write_augment_to_file(FILE *fl, augment_data *aug);
			augment_data *aug, *next_aug;
			HASH_ITER(hh, augment_table, aug, next_aug) {
				if (GET_AUG_VNUM(aug) >= (zone * 100) && GET_AUG_VNUM(aug) <= (zone * 100 + 99)) {
					write_augment_to_file(fl, aug);
				}
			}
			break;
		}
		case DB_BOOT_BLD: {
			bld_data *bld, *next_bld;
			HASH_ITER(hh, building_table, bld, next_bld) {
				if (GET_BLD_VNUM(bld) >= (zone * 100) && GET_BLD_VNUM(bld) <= (zone * 100 + 99)) {
					write_building_to_file(fl, bld);
				}
			}
			break;
		}
		case DB_BOOT_CLASS: {
			void write_class_to_file(FILE *fl, class_data *cls);
			class_data *cls, *next_cls;
			HASH_ITER(hh, class_table, cls, next_cls) {
				if (CLASS_VNUM(cls) >= (zone * 100) && CLASS_VNUM(cls) <= (zone * 100 + 99)) {
					write_class_to_file(fl, cls);
				}
			}
			break;
		}
		case DB_BOOT_CRAFT: {
			craft_data *craft, *next_craft;
			HASH_ITER(hh, craft_table, craft, next_craft) {
				if (GET_CRAFT_VNUM(craft) >= (zone * 100) && GET_CRAFT_VNUM(craft) <= (zone * 100 + 99)) {
					write_craft_to_file(fl, craft);
				}
			}
			break;
		}
		case DB_BOOT_CROP: {
			crop_data *crop, *next_crop;
			HASH_ITER(hh, crop_table, crop, next_crop) {
				if (GET_CROP_VNUM(crop) >= (zone * 100) && GET_CROP_VNUM(crop) <= (zone * 100 + 99)) {
					write_crop_to_file(fl, crop);
				}
			}
			break;
		}
		case DB_BOOT_FCT: {
			void write_faction_to_file(FILE *fl, faction_data *fct);
			faction_data *fct, *next_fct;
			HASH_ITER(hh, faction_table, fct, next_fct) {
				if (FCT_VNUM(fct) >= (zone * 100) && FCT_VNUM(fct) <= (zone * 100 + 99)) {
					write_faction_to_file(fl, fct);
				}
			}
			break;
		}
		case DB_BOOT_GEN: {
			void write_generic_to_file(FILE *fl, generic_data *gen);
			generic_data *gen, *next_gen;
			HASH_ITER(hh, generic_table, gen, next_gen) {
				if (GEN_VNUM(gen) >= (zone * 100) && GEN_VNUM(gen) <= (zone * 100 + 99)) {
					write_generic_to_file(fl, gen);
				}
			}
			break;
		}
		case DB_BOOT_GLB: {
			struct global_data *glb, *next_glb;
			HASH_ITER(hh, globals_table, glb, next_glb) {
				if (GET_GLOBAL_VNUM(glb) >= (zone * 100) && GET_GLOBAL_VNUM(glb) <= (zone * 100 + 99)) {
					write_global_to_file(fl, glb);
				}
			}
			break;
		}
		case DB_BOOT_MOB: {
			char_data *mob, *next_mob;
			HASH_ITER(hh, mobile_table, mob, next_mob) {
				if (GET_MOB_VNUM(mob) >= (zone * 100) && GET_MOB_VNUM(mob) <= (zone * 100 + 99)) {
					write_mob_to_file(fl, mob);
				}
			}
			break;
		}
		case DB_BOOT_MORPH: {
			void write_morph_to_file(FILE *fl, morph_data *morph);
			morph_data *morph, *next_morph;
			HASH_ITER(hh, morph_table, morph, next_morph) {
				if (MORPH_VNUM(morph) >= (zone * 100) && MORPH_VNUM(morph) <= (zone * 100 + 99)) {
					write_morph_to_file(fl, morph);
				}
			}
			break;
		}
		case DB_BOOT_OBJ: {
			obj_data *obj, *next_obj;
			HASH_ITER(hh, object_table, obj, next_obj) {
				if (GET_OBJ_VNUM(obj) >= (zone * 100) && GET_OBJ_VNUM(obj) <= (zone * 100 + 99)) {
					write_obj_to_file(fl, obj);
				}
			}
			break;
		}
		case DB_BOOT_PRG: {
			void write_progress_to_file(FILE *fl, progress_data *prg);
			progress_data *prg, *next_prg;
			HASH_ITER(hh, progress_table, prg, next_prg) {
				if (PRG_VNUM(prg) >= (zone * 100) && PRG_VNUM(prg) <= (zone * 100 + 99)) {
					write_progress_to_file(fl, prg);
				}
			}
			break;
		}
		case DB_BOOT_QST: {
			void write_quest_to_file(FILE *fl, quest_data *quest);
			quest_data *qst, *next_qst;
			HASH_ITER(hh, quest_table, qst, next_qst) {
				if (QUEST_VNUM(qst) >= (zone * 100) && QUEST_VNUM(qst) <= (zone * 100 + 99)) {
					write_quest_to_file(fl, qst);
				}
			}
			break;
		}
		case DB_BOOT_RMT: {
			room_template *rmt, *next_rmt;
			HASH_ITER(hh, room_template_table, rmt, next_rmt) {
				if (GET_RMT_VNUM(rmt) >= (zone * 100) && GET_RMT_VNUM(rmt) <= (zone * 100 + 99)) {
					write_room_template_to_file(fl, rmt);
				}
			}
			break;
		}
		case DB_BOOT_SECTOR: {
			sector_data *sect, *next_sect;
			HASH_ITER(hh, sector_table, sect, next_sect) {
				if (GET_SECT_VNUM(sect) >= (zone * 100) && GET_SECT_VNUM(sect) <= (zone * 100 + 99)) {
					write_sector_to_file(fl, sect);
				}
			}
			break;
		}
		case DB_BOOT_SHOP: {
			void write_shop_to_file(FILE *fl, shop_data *shop);
			shop_data *shop, *next_shop;
			HASH_ITER(hh, shop_table, shop, next_shop) {
				if (SHOP_VNUM(shop) >= (zone * 100) && SHOP_VNUM(shop) <= (zone * 100 + 99)) {
					write_shop_to_file(fl, shop);
				}
			}
			break;
		}
		case DB_BOOT_SKILL: {
			void write_skill_to_file(FILE *fl, skill_data *skill);
			skill_data *skill, *next_skill;
			HASH_ITER(hh, skill_table, skill, next_skill) {
				if (SKILL_VNUM(skill) >= (zone * 100) && SKILL_VNUM(skill) <= (zone * 100 + 99)) {
					write_skill_to_file(fl, skill);
				}
			}
			break;
		}
		case DB_BOOT_SOC: {
			void write_social_to_file(FILE *fl, social_data *soc);
			social_data *soc, *next_soc;
			HASH_ITER(hh, social_table, soc, next_soc) {
				if (SOC_VNUM(soc) >= (zone * 100) && SOC_VNUM(soc) <= (zone * 100 + 99)) {
					write_social_to_file(fl, soc);
				}
			}
			break;
		}
		case DB_BOOT_TRG: {
			trig_data *trig, *next_trig;
			HASH_ITER(hh, trigger_table, trig, next_trig) {
				if (GET_TRIG_VNUM(trig) >= (zone * 100) && GET_TRIG_VNUM(trig) <= (zone * 100 + 99)) {
					write_trigger_to_file(fl, trig);
				}
			}
			break;
		}
		case DB_BOOT_VEH: {
			void write_vehicle_to_file(FILE *fl, vehicle_data *veh);
			vehicle_data *veh, *next_veh;
			HASH_ITER(hh, vehicle_table, veh, next_veh) {
				if (VEH_VNUM(veh) >= (zone * 100) && VEH_VNUM(veh) <= (zone * 100 + 99)) {
					write_vehicle_to_file(fl, veh);
				}
			}
			break;
		}
		default: {
			syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Unable to write library file (2) for invalid type %d", type);
			// early exit -- nothing to do
			return;
		}
	}
	
	// terminate
	fprintf(fl, "$\n");
	fclose(fl);
	
	// and done
	rename(tempname, filename);
}


 //////////////////////////////////////////////////////////////////////////////
//// INDEX SAVING ////////////////////////////////////////////////////////////

// writes entries in the adventure index
void write_adventure_index(FILE *fl) {
	adv_data *adv, *next_adv;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		// determine "zone number" by vnum
		this = (int)(GET_ADV_VNUM(adv) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, ADV_SUFFIX);
			last = this;
		}
	}
}


// writes entries in the building index
void write_building_index(FILE *fl) {
	bld_data *bld, *next_bld;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, building_table, bld, next_bld) {
		// determine "zone number" by vnum
		this = (int)(GET_BLD_VNUM(bld) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, BLD_SUFFIX);
			last = this;
		}
	}
}


// writes entries in the craft index
void write_craft_index(FILE *fl) {
	craft_data *craft, *next_craft;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, craft_table, craft, next_craft) {
		// determine "zone number" by vnum
		this = (int)(GET_CRAFT_VNUM(craft) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, CRAFT_SUFFIX);
			last = this;
		}
	}
}


// writes entries in the crop index
void write_crop_index(FILE *fl) {
	crop_data *crop, *next_crop;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, crop_table, crop, next_crop) {
		// determine "zone number" by vnum
		this = (int)(GET_CROP_VNUM(crop) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, CROP_SUFFIX);
			last = this;
		}
	}
}


// writes entries in the globals index
void write_globals_index(FILE *fl) {
	struct global_data *glb, *next_glb;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, globals_table, glb, next_glb) {
		// determine "zone number" by vnum
		this = (int)(GET_GLOBAL_VNUM(glb) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, GLB_SUFFIX);
			last = this;
		}
	}
}


// writes entries in the mob index
void write_mobile_index(FILE *fl) {
	char_data *mob, *next_mob;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		// determine "zone number" by vnum -- don't use GET_MOB_VNUM because it's not extremely accurate
		this = (int)(mob->vnum / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, MOB_SUFFIX);
			last = this;
		}
	}
}


// writes entries in the object index
void write_object_index(FILE *fl) {
	obj_data *obj, *next_obj;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, object_table, obj, next_obj) {
		// determine "zone number" by vnum
		this = (int)(GET_OBJ_VNUM(obj) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, OBJ_SUFFIX);
			last = this;
		}
	}
}


// writes entries in the room template index
void write_room_template_index(FILE *fl) {
	room_template *rmt, *next_rmt;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		// determine "zone number" by vnum
		this = (int)(GET_RMT_VNUM(rmt) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, RMT_SUFFIX);
			last = this;
		}
	}
}


// writes entries in the sector index
void write_sector_index(FILE *fl) {
	sector_data *sect, *next_sect;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, sector_table, sect, next_sect) {
		// determine "zone number" by vnum
		this = (int)(GET_SECT_VNUM(sect) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, SECTOR_SUFFIX);
			last = this;
		}
	}
}


// writes entries in the trigger index
void write_trigger_index(FILE *fl) {
	trig_data *trig, *next_trig;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, trigger_table, trig, next_trig) {
		// determine "zone number" by vnum
		this = (int)(GET_TRIG_VNUM(trig) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, TRG_SUFFIX);
			last = this;
		}
	}
}


/**
* Writes the index file for any database type.
*
* @param int type Any DB_BOOT_ type.
*/
void save_index(int type) {
	char filename[64], tempfile[64], *prefix = NULL, *suffix = NULL;
	FILE *fl;
	
	// setup
	if (type >= 0 && type < NUM_DB_BOOT_TYPES && db_boot_info[type].prefix && db_boot_info[type].suffix) {
		prefix = db_boot_info[type].prefix;
		suffix = db_boot_info[type].suffix;
	}
	else {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Unable to write index file for invalid type %d", type);
		// early exit -- nothing to do
		return;
	}
	
	sprintf(filename, "%s%s", prefix, INDEX_FILE);
	strcpy(tempfile, filename);
	strcat(tempfile, TEMP_SUFFIX);
	
	if (!(fl = fopen(tempfile, "w"))) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Unable to write index file '%s': %s", filename, strerror(errno));
		return;
	}
	
	// DB_BOOT_x
	switch (type) {
		case DB_BOOT_ABIL: {
			void write_ability_index(FILE *fl);
			write_ability_index(fl);
			break;
		}
		case DB_BOOT_ACCT: {
			void write_account_index(FILE *fl);
			write_account_index(fl);
			break;
		}
		case DB_BOOT_ADV: {
			write_adventure_index(fl);
			break;
		}
		case DB_BOOT_ARCH: {
			void write_archetype_index(FILE *fl);
			write_archetype_index(fl);
			break;
		}
		case DB_BOOT_AUG: {
			void write_augments_index(FILE *fl);
			write_augments_index(fl);
			break;
		}
		case DB_BOOT_BLD: {
			write_building_index(fl);
			break;
		}
		case DB_BOOT_CLASS: {
			void write_class_index(FILE *fl);
			write_class_index(fl);
			break;
		}
		case DB_BOOT_CRAFT: {
			write_craft_index(fl);
			break;
		}
		case DB_BOOT_CROP: {
			write_crop_index(fl);
			break;
		}
		case DB_BOOT_FCT: {
			void write_faction_index(FILE *fl);
			write_faction_index(fl);
			break;
		}
		case DB_BOOT_GEN: {
			void write_generic_index(FILE *fl);
			write_generic_index(fl);
			break;
		}
		case DB_BOOT_GLB: {
			write_globals_index(fl);
			break;
		}
		case DB_BOOT_MOB: {
			write_mobile_index(fl);
			break;
		}
		case DB_BOOT_MORPH: {
			void write_morphs_index(FILE *fl);
			write_morphs_index(fl);
			break;
		}
		case DB_BOOT_OBJ: {
			write_object_index(fl);
			break;
		}
		case DB_BOOT_PRG: {
			void write_progress_index(FILE *fl);
			write_progress_index(fl);
			break;
		}
		case DB_BOOT_QST: {
			void write_quest_index(FILE *fl);
			write_quest_index(fl);
			break;
		}
		case DB_BOOT_RMT: {
			write_room_template_index(fl);
			break;
		}
		case DB_BOOT_SECTOR: {
			write_sector_index(fl);
			break;
		}
		case DB_BOOT_SHOP: {
			void write_shop_index(FILE *fl);
			write_shop_index(fl);
			break;
		}
		case DB_BOOT_SKILL: {
			void write_skill_index(FILE *fl);
			write_skill_index(fl);
			break;
		}
		case DB_BOOT_SOC: {
			void write_socials_index(FILE *fl);
			write_socials_index(fl);
			break;
		}
		case DB_BOOT_TRG: {
			write_trigger_index(fl);
			break;
		}
		case DB_BOOT_VEH: {
			void write_vehicle_index(FILE *fl);
			write_vehicle_index(fl);
			break;
		}
		case DB_BOOT_EMP: {
			empire_data *emp, *next_emp;
			
			HASH_ITER(hh, empire_table, emp, next_emp) {
				fprintf(fl, "%d%s\n", EMPIRE_VNUM(emp), suffix);
			}
			break;
		}
		default: {
			log("SYSERR: Unable to write index file %s: not implemented for type %d", filename, type);
			fprintf(fl, "$\n");
			fclose(fl);
			return;
		}
	}
	
	fprintf(fl, "$\n");
	fclose(fl);
	
	// and move the temp file over
	rename(tempfile, filename);
}


 //////////////////////////////////////////////////////////////////////////////
//// THE REALS ///////////////////////////////////////////////////////////////

// some of these were formerly named real_thing() but thing_proto() is clearer

/**
* @param adv_vnum vnum Any adventure vnum
* @return adv_data* The adventure, or NULL if it doesn't exist
*/
adv_data *adventure_proto(adv_vnum vnum) {
	adv_data *adv;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(adventure_table, &vnum, adv);
	return adv;
}


/**
* @param bld_vnum vnum Any building vnum
* @return bld_data* The building prototype, or NULL if it doesn't exist
*/
bld_data *building_proto(bld_vnum vnum) {
	bld_data *bld;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(building_table, &vnum, bld);
	return bld;
}


/**
* @param craft_vnum vnum Any craft recipe vnum
* @return craft_data* The craft, or NULL if it doesn't exist
*/
craft_data *craft_proto(craft_vnum vnum) {
	craft_data *craft;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(craft_table, &vnum, craft);
	return craft;
}


/**
* @param crop_vnum vnum Any crop vnum
* @return crop_data* The crop, or NULL if it doesn't exist
*/
crop_data *crop_proto(crop_vnum vnum) {
	crop_data *crop;
	
	// sanity
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(crop_table, &vnum, crop);
	return crop;
}


/**
* @param any_vnum vnum Any global vnum
* @return struct global_data* The global, or NULL if it doesn't exist
*/
struct global_data *global_proto(any_vnum vnum) {
	struct global_data *glb;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(globals_table, &vnum, glb);
	return glb;
}


/**
* @param mob_vnum vnum Any mob vnum.
* @return char_data* The mob prototype for that vnum, or NULL.
*/
char_data *mob_proto(mob_vnum vnum) {
	char_data *mob;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(mobile_table, &vnum, mob);
	return mob;
}


/**
* @param obj_vnum vnum Any obj vnum.
* @return obj_data* The obj prototype for that vnum, or NULL.
*/
obj_data *obj_proto(obj_vnum vnum) {
	obj_data *obj;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(object_table, &vnum, obj);
	return obj;
}


/**
* @param empire_vnum id The empire vnum.
* @return empire_date* An empire from the hash table, or NULL if no match.
*/
empire_data *real_empire(empire_vnum vnum) {
	empire_data *emp;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(empire_table, &vnum, emp);
	return emp;
}


/**
* Fetches a room by vnum; will NOT fill in a map space if it doesn't exist.
* Use real_room() if you need a guaranteed map room.
*
* @param room_vnum vnum The vnum to look up.
* @return room_data* A pointer to the room, or NULL.
*/
room_data *real_real_room(room_vnum vnum) {
	room_data *room = NULL;
	
	// sheer sanity
	if (vnum < 0 || vnum == NOWHERE) {
		return NULL;
	}
	
	// whole world
	HASH_FIND_INT(world_table, &vnum, room);
	
	return room;
}


/**
* Fetches a room by vnum. It is guaranteed to return a map room, even if it
* has to create an ocean to do it.
*
* @param room_vnum vnum The vnum to look up.
* @return room_data* A pointer to the room, or NULL.
*/
room_data *real_room(room_vnum vnum) {
	room_data *room = NULL;
	
	// sheer sanity
	if (vnum < 0 || vnum == NOWHERE) {
		return NULL;
	}
	
	// first see if it exists already
	room = real_real_room(vnum);
	
	// we guarantee map rooms exist
	if (!room && vnum < MAP_SIZE) {
		room = load_map_room(vnum);
	}
	
	return room;
}


/**
* @param rmt_vnum vnum Any room template vnum
* @return room_template* The template, or NULL if it doesn't exist
*/
room_template *room_template_proto(rmt_vnum vnum) {
	room_template *rmt;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(room_template_table, &vnum, rmt);
	return rmt;
}


/**
* @param sector_vnum vnum Any sector vnum
* @return sector_data* The found sector, or NULL.
*/
sector_data *sector_proto(sector_vnum vnum) {
	sector_data *sect;
	
	// sanity
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(sector_table, &vnum, sect);
	return sect;
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Checks that an object's wear/extra/aff/etc are correct, to verify data
* integrity.
*
* @param obj_data *obj The object to check.
* @return int Returns FALSE if the object is ok, or TRUE if it's bad.
*/
int check_object(obj_data *obj) {
	extern const char *affected_bits[];
	extern const char *wear_bits[];
	extern const char *extra_bits[];
	int error = FALSE;

	sprintbit(GET_OBJ_WEAR(obj), wear_bits, buf, TRUE);
	if (strstr(buf, "UNDEFINED")) {
		error = TRUE;
		log("SYSERR: Object #%d (%s) has unknown wear flags.", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
	}

	sprintbit(GET_OBJ_EXTRA(obj), extra_bits, buf, TRUE);
	if (strstr(buf, "UNDEFINED")) {
		error = TRUE;
		log("SYSERR: Object #%d (%s) has unknown extra flags.", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
	}

	sprintbit(GET_OBJ_AFF_FLAGS(obj), affected_bits, buf, TRUE);
	if (strstr(buf, "UNDEFINED")) {
		error = TRUE;
		log("SYSERR: Object #%d (%s) has unknown affection flags.", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
	}

	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_DRINKCON: {
			if (GET_DRINK_CONTAINER_CONTENTS(obj) > GET_DRINK_CONTAINER_CAPACITY(obj)) {
				error = TRUE;
				log("SYSERR: Object #%d (%s) contains (%d) more than maximum (%d).", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj), GET_DRINK_CONTAINER_CONTENTS(obj), GET_DRINK_CONTAINER_CAPACITY(obj));
			}
			break;
		}
	}

	return (error);
}


/**
* Cleans empire logs past the expiration date.
*/
void clean_empire_logs(void) {
	int empire_log_ttl = config_get_int("empire_log_ttl") * SECS_PER_REAL_DAY;
	
	struct empire_log_data *elog, *next_elog, *temp;
	empire_data *iter, *next_iter;
	time_t now = time(0);
	
	log("Cleaning stale empire logs...");

	HASH_ITER(hh, empire_table, iter, next_iter) {
		for (elog = EMPIRE_LOGS(iter); elog; elog = next_elog) {
			next_elog = elog->next;
			
			// stale?
			if (elog->timestamp + empire_log_ttl < now) {
				REMOVE_FROM_LIST(elog, EMPIRE_LOGS(iter), next);
				if (elog->string) {
					free(elog->string);
				}
				free(elog);
				EMPIRE_NEEDS_LOGS_SAVE(iter) = TRUE;
			}
		}
	}
}


/**
* function to count how many hash-mark delimited records exist in a file
*
* @param FILE *fl A file.
* @return int The number of lines in the file that start with #
*/
int count_hash_records(FILE *fl) {
	char buf[128];
	int count = 0;

	while (fgets(buf, 128, fl))
		if (*buf == '#')
			count++;

	return (count);
}


/**
* This function is called mainly when someone is trying to claim land. It
* will create an empire if the player isn't in one.
*
* @param char_data *ch The player.
* @return empire_data *The empire to use to claim land, etc.
*/
empire_data *get_or_create_empire(char_data *ch) {
	empire_data *emp;
	
	if (IS_NPC(ch) || PRF_FLAGGED(ch, PRF_NOEMPIRE)) {
		return NULL;
	}
	if ((emp = GET_LOYALTY(ch))) {
		return emp;
	}
	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		return NULL;	// do not create
	}
	return create_empire(ch);
}


/**
* @return empire_vnum A free empire vnum, or NOTHING if there aren't any somehow.
*/
empire_vnum find_free_empire_vnum(void) {
	empire_data *emp, *next_emp;
	empire_vnum iter, max = 0;

	// easy way (no empires)
	if (!empire_table) {
		return 1;
	}
	
	// almost easy way (top empire + 1)
	HASH_ITER(hh, empire_table, emp, next_emp) {
		max = MAX(max, EMPIRE_VNUM(emp));
	}
	if (max < MAX_INT - 1) {
		return max + 1;
	}
	
	// hard way
	for (iter = 1; iter < MAX_INT; ++iter) {
		if (!real_empire(iter)) {
			return iter;
		}
	}
	
	// there's no fffing way there are more than MAX_INT-1 empires -pc
	return NOTHING;
}


/**
* Reads in a custom message, after the 'M' tag (or whichever letter) has been
* read.
*
* @param FILE *fl The file already open for reading.
* @param struct custom_message **list The list to append the message to.
* @param char *error The partial error message, in case of emergencies.
*/
void parse_custom_message(FILE *fl, struct custom_message **list, char *error) {
	char line[MAX_STRING_LENGTH];
	struct custom_message *mes;
	int num;
	
	if (!get_line(fl, line) || sscanf(line, "%d", &num) != 1) {
		log("SYSERR: Unable to read message type for custom message line of %s", error);
		exit(1);
	}
	
	CREATE(mes, struct custom_message, 1);
	mes->type = num;
	mes->msg = fread_string(fl, error);
	
	LL_APPEND(*list, mes);
}


/**
* Writes out a set of custom messages to a mob/obj file.
*
* @param FILE *fl The file, open for writing.
* @param char letter The key letter (usually 'M').
* @param struct custom_message *list The list to write.
*/
void write_custom_messages_to_file(FILE *fl, char letter, struct custom_message *list) {
	struct custom_message *iter;
	
	// M: custom message
	LL_FOREACH(list, iter) {
		fprintf(fl, "%c\n", letter);
		fprintf(fl, "%d\n", iter->type);
		fprintf(fl, "%s~\n", iter->msg);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// SORTERS /////////////////////////////////////////////////////////////////

/**
* Sorter for help_index_element.
*/
int help_sort(const void *a, const void *b) {
	const struct help_index_element *a1, *b1;
	int cmp;

	a1 = (const struct help_index_element *) a;
	b1 = (const struct help_index_element *) b;

	cmp = (str_cmp(a1->keyword, b1->keyword));
	
	if (cmp) {
		return cmp;
	}
	else {
		return (b1->level - a1->level);
	}
}


/**
* Simple sorter for the adventure hash
*
* @param adv_data *a One element
* @param adv_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_adventures(adv_data *a, adv_data *b) {
	return GET_ADV_VNUM(a) - GET_ADV_VNUM(b);
}


/**
* Simple sorter for the buildings hash
*
* @param bld_data *a One element
* @param bld_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_buildings(bld_data *a, bld_data *b) {
	return GET_BLD_VNUM(a) - GET_BLD_VNUM(b);
}


/**
* For sorted crafts, as displayed to players
*
* @param craft_data *a One element
* @param craft_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_crafts_by_data(craft_data *a, craft_data *b) {
	ability_data *a_abil, *b_abil;
	int a_level, b_level;
	
	if (GET_CRAFT_TYPE(a) != GET_CRAFT_TYPE(b)) {
		return GET_CRAFT_TYPE(a) - GET_CRAFT_TYPE(b);
	}
	
	a_abil = find_ability_by_vnum(GET_CRAFT_ABILITY(a));
	b_abil = find_ability_by_vnum(GET_CRAFT_ABILITY(b));
	a_level = a_abil ? ABIL_SKILL_LEVEL(a_abil) : 0;
	b_level = b_abil ? ABIL_SKILL_LEVEL(b_abil) : 0;
	
	// reverse level sort
	if (a_level != b_level) {
		return b_level - a_level;
	}
	
	return strcmp(NULLSAFE(GET_CRAFT_NAME(a)), NULLSAFE(GET_CRAFT_NAME(b)));
}


/**
* Simple sorter for the crafts hash
*
* @param craft_data *a One element
* @param craft_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_crafts_by_vnum(craft_data *a, craft_data *b) {
	return GET_CRAFT_VNUM(a) - GET_CRAFT_VNUM(b);
}


/**
* Simple sorter for the crops hash
*
* @param crop_data *a One element
* @param crop_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_crops(crop_data *a, crop_data *b) {
	return GET_CROP_VNUM(a) - GET_CROP_VNUM(b);
}


/**
* Simple sorter for the globals hash
*
* @param struct global_data *a One element
* @param struct global_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_globals(struct global_data *a, struct global_data *b) {
	return GET_GLOBAL_VNUM(a) - GET_GLOBAL_VNUM(b);
}


/**
* Compare two empires for sorting.
*
* @param empire_data *a First empire
* @param empire_data *b Second empire
* @return int <0, 0, or >0, depending on comparison
*/
int sort_empires(empire_data *a, empire_data *b) {
	extern int get_total_score(empire_data *emp);
	
	bool a_timeout = EMPIRE_IS_TIMED_OUT(a);
	bool b_timeout = EMPIRE_IS_TIMED_OUT(b);

	int a_score = get_total_score(a), b_score = get_total_score(b);

	if (EMPIRE_MEMBERS(a) == 0 || EMPIRE_MEMBERS(b) == 0) {
		// special case -- push 0-member empires to the end
		return EMPIRE_MEMBERS(b) - EMPIRE_MEMBERS(a);
	}
	else if (a_timeout != b_timeout) {
		return a_timeout ? 1 : -1;
	}
	else if (EMPIRE_IMM_ONLY(a) != EMPIRE_IMM_ONLY(b)) {
		return EMPIRE_IMM_ONLY(a) - EMPIRE_IMM_ONLY(b);
	}
	/*
	else if (EMPIRE_HAS_TECH(a, TECH_PROMINENCE) && !EMPIRE_HAS_TECH(b, TECH_PROMINENCE)) {
		return -1;
	}
	else if (!EMPIRE_HAS_TECH(a, TECH_PROMINENCE) && EMPIRE_HAS_TECH(b, TECH_PROMINENCE)) {
		return 1;
	}
	*/
	else if (a_score != b_score) {
		return b_score - a_score;
	}
	else if (EMPIRE_SORT_VALUE(a) != EMPIRE_SORT_VALUE(b)) {
		// these should never be the same even if the scores were
		return EMPIRE_SORT_VALUE(b) - EMPIRE_SORT_VALUE(a);
	}
	else {
		// fallback?
		return str_cmp(EMPIRE_NAME(a), EMPIRE_NAME(b));
	}
}


/**
* Sorts a list of exits by direction.
*
* @param struct room_direction_data **list The list of exits to sort.
*/
void sort_exits(struct room_direction_data **list) {
	struct room_direction_data *a, *b, *a_next, *b_next;
	struct room_direction_data temp;
	bool changed = TRUE;
	
	if (*list && (*list)->next) {
		while (changed) {
			changed = FALSE;

			a = *list;
			while ((b = a->next)) {
				if (a->dir > b->dir) {
					// preserve next-pointers
					a_next = a->next;
					b_next = b->next;
					
					// swap positions by swapping data
					temp = *a;
					*a = *b;
					*b = temp;
					
					// restore next pointers
					a->next = a_next;
					b->next = b_next;
					
					changed = TRUE;
				}
				
				a = a->next;
			}
		}
	}
}


/**
* This sorts icons by type, for easier visual analysis in both the editor and
* the db file. It should not change the relative order of any entries within
* a type.
*
* @param struct icon_data **list The icon set to sort.
*/
void sort_icon_set(struct icon_data **list) {
	struct icon_data *a, *b, *a_next, *b_next;
	struct icon_data temp;
	bool changed = TRUE;
	
	if (*list && (*list)->next) {
		while (changed) {
			changed = FALSE;

			a = *list;
			while ((b = a->next)) {
				if (a->type > b->type) {
					// preserve next-pointers
					a_next = a->next;
					b_next = b->next;
					
					// swap positions by swapping data
					temp = *a;
					*a = *b;
					*b = temp;
					
					// restore next pointers
					a->next = a_next;
					b->next = b_next;
					
					changed = TRUE;
				}
				
				a = a->next;
			}
		}
	}
}


/**
* This sorts interactions by type, for easier visual analysis in both
* the editor and the db file. It should not change the relative order of any
* entries within a type.
*
* @param struct interaction_item **list The interactions list to sort.
*/
void sort_interactions(struct interaction_item **list) {
	struct interaction_item *a, *b, *a_next, *b_next;
	struct interaction_item temp;
	bool changed = TRUE;
	
	if (*list && (*list)->next) {
		while (changed) {
			changed = FALSE;

			a = *list;
			while ((b = a->next)) {
				if (a->type > b->type || (a->type == b->type && a->exclusion_code > b->exclusion_code)) {
					// preserve next-pointers
					a_next = a->next;
					b_next = b->next;
					
					// swap positions by swapping data
					temp = *a;
					*a = *b;
					*b = temp;
					
					// restore next pointers
					a->next = a_next;
					b->next = b_next;
					
					changed = TRUE;
				}
				
				a = a->next;
			}
		}
	}
}


/**
* Simple sorter for the mob hash
*
* @param char_data *a One element
* @param char_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_mobiles(char_data *a, char_data *b) {
	// using GET_MOB_VNUM() macro here fails because of IS_NPC check
	return a->vnum - b->vnum;
}


/**
* Simple sorter for the object hash
*
* @param obj_data *a One element
* @param obj_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_objects(obj_data *a, obj_data *b) {
	return GET_OBJ_VNUM(a) - GET_OBJ_VNUM(b);
}


/**
* Simple sorter for the room templates hash
*
* @param room_template *a One element
* @param room_template *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_room_templates(room_template *a, room_template *b) {
	return GET_RMT_VNUM(a) - GET_RMT_VNUM(b);
}


/**
* Simple sorter for the sector_table hash
*
* @param void *a One element
* @param void *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_sectors(void *a, void *b) {
	return GET_SECT_VNUM((sector_data*)a) - GET_SECT_VNUM((sector_data*)b);
}


/**
* This sorts trade data by type and cost, for easier visual analysis in both
* the editor and the db file. It should not change the relative order of any
* entries within a type.
*
* @param struct empire_trade_data **list The trade list to sort.
*/
void sort_trade_data(struct empire_trade_data **list) {
	struct empire_trade_data *a, *b, *a_next, *b_next;
	struct empire_trade_data temp;
	bool changed = TRUE;
	
	if (*list && (*list)->next) {
		while (changed) {
			changed = FALSE;

			a = *list;
			while ((b = a->next)) {
				if (a->type > b->type || (a->type == b->type && a->cost > b->cost)) {
					// preserve next-pointers
					a_next = a->next;
					b_next = b->next;
					
					// swap positions by swapping data
					temp = *a;
					*a = *b;
					*b = temp;
					
					// restore next pointers
					a->next = a_next;
					b->next = b_next;
					
					changed = TRUE;
				}
				
				a = a->next;
			}
		}
	}
}


/**
* Simple sorter for the triggers hash
*
* @param trig_data *a One element
* @param trig_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_triggers(trig_data *a, trig_data *b) {
	return GET_TRIG_VNUM(a) - GET_TRIG_VNUM(b);
}


/**
* Simple sorter for the world_table hash
*
* @param void *a One element
* @param void *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_world_table_func(void *a, void *b) {
	return ((room_data*)a)->vnum - ((room_data*)b)->vnum;
}


/**
* Sorts the world table -- only necessary for things like saving.
*/
void sort_world_table(void) {
	if (!world_is_sorted) {
		HASH_SORT(world_table, sort_world_table_func);
	}
	world_is_sorted = TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS LIB ///////////////////////////////////////////////////////

/**
* Copies a list of augment applies.
*
* @param struct apply_data *input The list to copy.
* @return struct apply_data* The copied list.
*/
struct apply_data *copy_apply_list(struct apply_data *input) {
	struct apply_data *new, *last, *list, *iter;
	
	last = NULL;
	list = NULL;
	for (iter = input; iter; iter = iter->next) {
		CREATE(new, struct apply_data, 1);
		*new = *iter;
		new->next = NULL;
		
		if (last) {
			last->next = new;
		}
		else {
			list = new;
		}
		last = new;
	}
	
	return list;
}


/**
* Creates a copy of a list of applies.
*
* @param struct obj_apply *list The list to copy.
* @return struct obj_apply* A pointer to the copy list.
*/
struct obj_apply *copy_obj_apply_list(struct obj_apply *list) {
	struct obj_apply *new_list, *last, *iter, *new;
	
	new_list = last = NULL;
	for (iter = list; iter; iter = iter->next) {
		CREATE(new, struct obj_apply, 1);
		*new = *iter;
		new->next = NULL;
		
		if (last) {
			last->next = new;
		}
		else {
			new_list = new;
		}
		last = new;
	}
	
	return new_list;
}


/**
* Frees a list of augment applies.
*
* @param struct apply_data *list The start of the list to free.
*/
void free_apply_list(struct apply_data *list) {
	struct apply_data *app;
	while ((app = list)) {
		list = app->next;
		free(app);
	}
}


/**
* Frees the memory for a list of object applies.
*
* @param struct obj_apply *list The start of the list to free.
*/
void free_obj_apply_list(struct obj_apply *list) {
	struct obj_apply *app;
	
	while ((app = list)) {
		list = app->next;
		free(app);
	}
}


/**
* Creates and initialies a complex room data pointer.
*
* @return complex_room_data* Clean building data.
*/
struct complex_room_data *init_complex_data() {
	struct complex_room_data *data;
	
	CREATE(data, struct complex_room_data, 1);
	
	// building list
	data->to_build = NULL;
	
	data->entrance = NO_DIR;
	data->patron = 0;
	data->inside_rooms = 0;
	data->home_room = NULL;
	data->private_owner = NOBODY;
	
	data->burn_down_time = 0;	// not-burning
	data->damage = 0;	// no damage
	
	return data;
}


/**
* @param struct complex_room_data *data The building data to delete.
*/
void free_complex_data(struct complex_room_data *data) {
	struct room_direction_data *ex;
	
	while ((ex = data->exits)) {
		data->exits = ex->next;
		if (ex->room_ptr) {
			--GET_EXITS_HERE(ex->room_ptr);
		}
		if (ex->keyword) {
			free(ex->keyword);
		}
		free(ex);
	}
	
	free_resource_list(data->to_build);
	free_resource_list(data->built_with);
	
	free(data);
}


/**
* Frees the memory for shared room data.
*
* @param struct shared_room_data *data The data to free.
*/
void free_shared_room_data(struct shared_room_data *data) {
	struct room_extra_data *room_ex, *next_room_ex;
	struct stored_event *ev, *next_ev;
	struct depletion_data *dep;
	struct track_data *track;
	
	if (data->name) {
		free(data->name);
	}
	if (data->icon) {
		free(data->icon);
	}
	if (data->description) {
		free(data->description);
	}
	
	while ((dep = data->depletion)) {
		data->depletion = dep->next;
		free(dep);
	}
	HASH_ITER(hh, data->extra_data, room_ex, next_room_ex) {
		HASH_DEL(data->extra_data, room_ex);
		free(room_ex);
	}
	while ((track = data->tracks)) {
		data->tracks = track->next;
		free(track);
	}
	
	HASH_ITER(hh, data->events, ev, next_ev) {
		cancel_stored_event(&data->events, ev->type);
	}
	
	free(data);
}


/**
* Parses an 'A' apply tag, written by write_applies_to_file(). This file
* should have just read the 'A' line, and the next line to read is its data.
*
* @param FILE *fl The file to read from.
* @param struct apply_data **list The apply list to append to.
* @param char *error_str An identifier to log if there is a problem.
*/
void parse_apply(FILE *fl, struct apply_data **list, char *error_str) {
	struct apply_data *new;
	char line[256];
	int int_in[2];
	
	if (!fl || !list || !get_line(fl, line) || sscanf(line, "%d %d", &int_in[0], &int_in[1]) != 2) {
		log("SYSERR: format error in A line of %s", error_str ? error_str : "apply");
		exit(1);
	}
	
	CREATE(new, struct apply_data, 1);
	new->location = int_in[0];
	new->weight = int_in[1];
	
	LL_APPEND(*list, new);
}


/**
* Loads a generic name file into memory.
*
* @param FILE *fl File open for reading.
* @param char *err_str The name of the file, for error reporting.
*/
void parse_generic_name_file(FILE *fl, char *err_str) {
	extern struct generic_name_data *get_generic_name_list(int name_set, int sex);
	extern const char *genders[];
	extern struct generic_name_data *generic_names;
	extern int search_block(char *arg, const char **list, int exact);
	
	struct generic_name_data *data;
	char line[256], str_in[256];
	int count, pos, int_in, sex;
	
	if (!fl) {
		log("SYSERR: parse_generic_name_file called with no file");
		return;
	}
	
	count = 0;
	while (get_line(fl, line)) {
		if (*line == '$') {
			break;
		}
		else {
			++count;
		}
	}
	
	// first line is meta-data
	--count;
	
	if (count <= 0) {
		// no work
		return;
	}
	
	rewind(fl);
	
	if (!get_line(fl, line) || sscanf(line, "%d %s", &int_in, str_in) != 2) {
		log("SYSERR: Invalid format in meta-data line of %s", err_str);
		exit(1);
	}
	
	if ((sex = search_block(str_in, genders, FALSE)) == NOTHING) {
		log("SYSERR: Invalid gender '%s' in %s", str_in, err_str);
		exit(1);
	}
	
	if (get_generic_name_list(int_in, sex)) {
		log("SYSERR: Duplicate name list '%d %s'", int_in, genders[sex]);
		exit(1);
	}
	
	// setup
	CREATE(data, struct generic_name_data, 1);
	data->name_set = int_in;
	data->sex = sex;
	
	// add to LL
	data->next = generic_names;
	generic_names = data;
	
	// name data
	CREATE(data->names, char*, count);
	
	// read in
	pos = 0;
	while (pos < count && get_line(fl, line)) {
		// done
		if (*line == '$') {
			break;
		}
		else if (*line) {
			data->names[pos++] = str_dup(line);
		}
	}
	
	// store actual final size
	data->size = pos;
}


/**
* Parse one linking rule and assign it to the end of the given list.
* This function will trigger an exit(1) if it errors.
*
* @param FILE *fl The file, open for reading, right after the letter tag (usually L).
* @param struct adventure_link_rule **list A reference to the start of the list to assign to.
* @param char *error_part A string containing e.g. "mob #1234", describing where the error occurred, if one happens.
*/
void parse_link_rule(FILE *fl, struct adventure_link_rule **list, char *error_part) {
	char line[MAX_INPUT_LENGTH], str_in[MAX_INPUT_LENGTH], str_in2[MAX_INPUT_LENGTH];
	struct adventure_link_rule *link;
	int int_in[3];
	
	CREATE(link, struct adventure_link_rule, 1);
	LL_APPEND(*list, link);
	
	// line 1: type flags
	if (!get_line(fl, line) || sscanf(line, "%d %s", &int_in[0], str_in) != 2) {
		log("SYSERR: Format error in link rule line 1 of %s", buf2);
		exit(1);
	}
	
	link->type = int_in[0];
	link->flags = asciiflag_conv(str_in);
	
	// line 2: value portal_in portal_out
	if (!get_line(fl, line) || sscanf(line, "%d %d %d", &int_in[0], &int_in[1], &int_in[2]) != 3) {
		log("SYSERR: Format error in link rule line 2 of %s", buf2);
		exit(1);
	}
	
	link->value = int_in[0];
	link->portal_in = int_in[1];
	link->portal_out = int_in[2];
	
	// line 3: dir bld_on bld_facing
	if (!get_line(fl, line) || sscanf(line, "%d %s %s", &int_in[0], str_in, str_in2) != 3) {
		log("SYSERR: Format error in link rule line 3 of %s", buf2);
		exit(1);
	}
	
	link->dir = int_in[0];
	link->bld_on = asciiflag_conv(str_in);
	link->bld_facing = asciiflag_conv(str_in2);
}


/**
* Writes the A tag for any apply_data list.
*
* @param FILE *fl The file to write to.
* @param struct apply_data *list The list of applies to write.
*/
void write_applies_to_file(FILE *fl, struct apply_data *list) {
	struct apply_data *app;
	
	LL_FOREACH(list, app) {
		fprintf(fl, "A\n%d %d\n", app->location, app->weight);
	}
}


/**
* Sorts requirements by what group they are in.
*
* @param struct req_data *a Comparable #1.
* @param struct req_data *a Comparable #2.
* @return int sort instruction (-, 0, +)
*/
int sort_requirements_by_group(struct req_data *a, struct req_data *b) {
	return (a->group - b->group);
}


/**
* Parses a requirement, saved as:
*
* A
* 1 123 123456 10
*
* @param FILE *fl The file, having just read the letter tag.
* @param struct req_data **list The list to append to.
* @param char *error_str How to report if there is an error.
*/
void parse_requirement(FILE *fl, struct req_data **list, char *error_str) {
	struct req_data *req;
	int type, needed;
	bitvector_t misc;
	char line[256];
	any_vnum vnum;
	char group;
	
	if (!fl || !list || !get_line(fl, line)) {
		log("SYSERR: data error in requirement line of %s", error_str ? error_str : "UNKNOWN");
		exit(1);
	}
	
	if (sscanf(line, "%d %d %llu %d %c", &type, &vnum, &misc, &needed, &group) == 5) {
		group = isalpha(group) ? group : 0;
	}
	else if (sscanf(line, "%d %d %llu %d", &type, &vnum, &misc, &needed) == 4) {
		group = 0;
	}
	else {
		log("SYSERR: format error in requirement line of %s", error_str ? error_str : "UNKNOWN");
		exit(1);
	}
	
	CREATE(req, struct req_data, 1);
	req->type = type;
	req->vnum = vnum;
	req->misc = misc;
	req->group = group;
	req->needed = needed;
	req->current = 0;
	
	LL_APPEND(*list, req);
	LL_SORT(*list, sort_requirements_by_group);
}


/**
* Saves linking rule data (usually 'L' tag) to a db file.
*
* @param FILE *fl The open file to write to.
* @param char letter The letter to tag in the file.
* @param struct adventure_link_rule *list The list to write.
*/
void write_linking_rules_to_file(FILE *fl, char letter, struct adventure_link_rule *list) {
	struct adventure_link_rule *link;
	char temp[256], temp2[256];
	
	LL_FOREACH(list, link) {
		fprintf(fl, "%c\n", letter);
		fprintf(fl, "%d %s\n", link->type, bitv_to_alpha(link->flags));
		fprintf(fl, "%d %d %d\n", link->value, link->portal_in, link->portal_out);
		
		strcpy(temp, bitv_to_alpha(link->bld_on));
		strcpy(temp2, bitv_to_alpha(link->bld_facing));
		fprintf(fl, "%d %s %s\n", link->dir, temp, temp2);
	}
}


/**
* Writes a list of 'req_data' to a data file.
*
* @param FILE *fl The file, open for writing.
* @param char letter The tag letter.
* @param struct req_data *list The list to write.
*/
void write_requirements_to_file(FILE *fl, char letter, struct req_data *list) {
	struct req_data *iter;
	LL_FOREACH(list, iter) {
		// NOTE: iter->current is NOT written to file (is only used for live data)
		if (iter->group) {
			fprintf(fl, "%c\n%d %d %llu %d %c\n", letter, iter->type, iter->vnum, iter->misc, iter->needed, iter->group);
		}
		else {
			fprintf(fl, "%c\n%d %d %llu %d\n", letter, iter->type, iter->vnum, iter->misc, iter->needed);
		}
	}
}


/**
* Saves the 'shared' portion of a room or map tile to file.
*
* @param FILE *fl The file already open for writing.
* @param struct shared_room_data *dat The shared data to write.
*/
void write_shared_room_data(FILE *fl, struct shared_room_data *dat) {
	struct room_extra_data *red, *next_red;
	char temp[MAX_STRING_LENGTH];
	struct depletion_data *dep;
	struct track_data *track, *next_track;
	time_t now = time(0);
	
	// E affects
	if (dat->base_affects) {
		fprintf(fl, "E\n%llu\n", dat->base_affects);
	}

	// I icon
	if (dat->icon) {
		fprintf(fl, "I\n%s~\n", dat->icon);
	}

	// M description
	if (dat->description) {
		strcpy(temp, dat->description);
		strip_crlf(temp);
		fprintf(fl, "M\n%s~\n", temp);
	}

	// N name
	if (dat->name) {
		fprintf(fl, "N\n%s~\n", dat->name);
	}
	
	// X depletion
	LL_FOREACH(dat->depletion, dep) {
		fprintf(fl, "X\n%d %d\n", dep->type, dep->count);
	}
	
	// Y tracks
	LL_FOREACH_SAFE(dat->tracks, track, next_track) {
		if (now - track->timestamp > SECS_PER_REAL_HOUR) {
			LL_DELETE(dat->tracks, track);
			free(track);
		}
		else {
			fprintf(fl, "Y\n%d %d %ld %d\n", track->player_id, track->mob_num, track->timestamp, track->dir);
		}
	}

	// Z: extra data
	HASH_ITER(hh, dat->extra_data, red, next_red) {
		fprintf(fl, "Z\n%d %d\n", red->type, red->value);
	}
}
