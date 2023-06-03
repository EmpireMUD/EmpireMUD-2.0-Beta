/* ************************************************************************
*   File: db.c                                            EmpireMUD 2.0b5 *
*  Usage: Loading/saving chars, booting/resetting world, internal funcs   *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __DB_C__

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "skills.h"
#include "olc.h"
#include "vnums.h"
#include "dg_scripts.h"
#include "constants.h"

/**
* Contents:
*   Global Data
*   Startup
*   Temporary Data System
*   Data Integrity
*   Post-Processing
*   I/O Helpers
*   Help I/O
*   Island Setup
*   Mobile Loading
*   Object Loading
*   Miscellaneous Helpers
*   Miscellaneous Loaders
*   Miscellaneous Savers
*/

// functions called during startup
void Read_Invalid_List();
void abandon_lost_vehicles();
void boot_world();
void build_all_quest_lookups();
void build_all_shop_lookups();
void build_land_map();
void build_player_index();
void build_world_map();
void check_abilities();
void check_and_link_faction_relations();
void check_archetypes();
void check_classes();
void check_for_bad_sectors();
void check_for_new_map();
void check_learned_empire_crafts();
void check_newbie_islands();
void check_nowhere_einv_all();
void check_sector_times(any_vnum only_sect);
void check_skills();
void check_triggers();
void check_version();
void chore_update();
void clean_empire_logs();
void compute_generic_relations();
void delete_old_players();
void delete_orphaned_rooms();
void delete_pre_b5_116_world_files();
void expire_old_politics();
void generate_island_descriptions();
void index_boot_world();
void init_config_system();
void init_inherent_player_techs();
void init_reputation();
void init_text_file_strings();
void link_and_check_vehicles();
void load_automessages();
void load_banned();
void load_binary_map_file();
void load_daily_quest_file();
void load_empire_storage();
void load_fight_messages();
void load_instances();
void load_islands();
void load_running_events_file();
void load_slash_channels();
void load_tips_of_the_day();
void load_trading_post();
void load_world_from_binary_index();
void perform_requested_world_saves();
void renum_world();
void run_reboot_triggers();
void schedule_map_unloads();
void setup_island_levels();
void sort_commands();
void startup_room_reset();
void update_instance_world_size();
void verify_empire_goals();
void verify_running_events();
void verify_sectors();
void write_binary_world_index_updates();
void write_whole_mapout();
int sort_abilities_by_data(ability_data *a, ability_data *b);
int sort_archetypes_by_data(archetype_data *a, archetype_data *b);
int sort_augments_by_data(augment_data *a, augment_data *b);
int sort_classes_by_data(class_data *a, class_data *b);
int sort_skills_by_data(skill_data *a, skill_data *b);
int sort_socials_by_data(social_data *a, social_data *b);

// local functions
void get_one_line(FILE *fl, char *buf);


 //////////////////////////////////////////////////////////////////////////////
//// GLOBAL DATA /////////////////////////////////////////////////////////////

// abilities
ability_data *ability_table = NULL;	// main hash (hh)
ability_data *sorted_abilities = NULL;	// alpha hash (sorted_hh)

// adventures
adv_data *adventure_table = NULL;	// adventure hash table

// archetype
archetype_data *archetype_table = NULL;	// main hash (hh)
archetype_data *sorted_archetypes = NULL;	// sorted hash (sorted_hh)

// augments
augment_data *augment_table = NULL;	// main augment hash table
augment_data *sorted_augments = NULL;	// alphabetic version // sorted_hh

// automessage system
struct automessage *automessages_table = NULL;	// hash table (hh, by id)
int max_automessage_id = 0;	// read from file, permanent max id

// buildings
bld_data *building_table = NULL;	// building hash table

// classes
class_data *class_table = NULL;	// main hash (hh)
class_data *sorted_classes = NULL;	// alpha hash (sorted_hh)

// crafts
craft_data *craft_table = NULL;	// main crafting table
craft_data *sorted_crafts = NULL;	// sorted craft table

// crops
crop_data *crop_table = NULL;	// crop hash table

// empires
empire_data *empire_table = NULL;	// hash table of empires
double empire_score_average[NUM_SCORES];
struct trading_post_data *trading_list = NULL;	// global DLL of trading post stuff
bool check_empire_refresh = FALSE;	// triggers empire refreshes
struct empire_territory_data *global_next_territory_entry = NULL;	// for territory iteration

// events
event_data *event_table = NULL;	// global hash table (hh)
int top_event_id = 0;	// highest unique id used
struct event_running_data *running_events = NULL;	// list of active events
bool events_need_save = FALSE;	// triggers a save on running_events

// factions
faction_data *faction_table = NULL;	// main hash (hh)
faction_data *sorted_factions = NULL;	// alpha hash (sorted_hh)
int MAX_REPUTATION = 0;	// highest possible rep value, auto-detected at startup
int MIN_REPUTATION = 0;	// lowest possible rep value, auto-detected at startup

// fight system
struct message_list fight_messages[MAX_MESSAGES];	// fighting messages

// game config
time_t boot_time = 0;	// time of mud boot
int Global_ignore_dark = 0;	// For use in public channels
int no_auto_deletes = 0;	// skip player deletes on boot?
int wizlock_level = 0;	// level of game restriction
char *wizlock_message = NULL;	// Message sent to people trying to connect

// generics
generic_data *generic_table = NULL;	// hash table (hh)
generic_data *sorted_generics = NULL;	// hash table (sorted_hh)

// global stuff
struct global_data *globals_table = NULL;	// hash table of global_data

// helps
struct help_index_element *help_table = 0;	// the help table -- NOT a hash table
int top_of_helpt = 0;	// top of help index table

// instances
struct instance_data *instance_list = NULL;	// doubly-linked global instance list
bool instance_save_wait = FALSE;	// prevents repeated instance saving
struct instance_data *quest_instance_global = NULL;	// passes instances through to some quest triggers
bool need_instance_save = FALSE;	// triggers full instance saves

// map
room_vnum *start_locs = NULL;	// array of start locations
int highest_start_loc_index = -1;	// maximum start locations
int top_island_num = -1;	// for number of islands

// mobs
char_data *mobile_table = NULL;	// hash table of mobs
struct player_special_data dummy_mob;	// dummy spec area for mobs
char_data *character_list = NULL;	// global doubly-linked list of chars (including players)
char_data *combat_list = NULL;	// head of l-list of fighting chars
char_data *next_combat_list = NULL;	// used for iteration of combat_list when more than 1 person can be removed from combat in 1 loop iteration
char_data *next_combat_list_main = NULL;	// used for iteration of combat_list in frequent_combat()
struct generic_name_data *generic_names = NULL;	// LL of generic name sets
char_data *global_next_player = NULL;	// used in limits.c for iterating
char_data *player_character_list = NULL;	// global doubly-linked list of players-only (no NPCs)

// morphs
morph_data *morph_table = NULL;	// main morph hash table
morph_data *sorted_morphs = NULL;	// alphabetic version // sorted_hh

// objects
obj_data *object_list = NULL;	// global doubly-linked list of objs
obj_data *object_table = NULL;	// hash table of objs
bool suspend_autostore_updates = FALSE;	// prevents rescheduling an event twice in a row

// safe obj iterators
obj_data *purge_bound_items_next = NULL;	// used in purge_bound_items()
obj_data *global_next_obj = NULL;	// used in limits.c

// players
account_data *account_table = NULL;	// hash table of accounts
player_index_data *player_table_by_idnum = NULL;	// hash table by idnum
player_index_data *player_table_by_name = NULL;	// hash table by name
int top_idnum = 0;	// highest idnum in use
int top_account_id = 0;  // highest account number in use, determined during startup
struct group_data *group_list = NULL;	// global LL of groups
bool pause_affect_total = FALSE;	// helps prevent unnecessary calls to affect_total
int max_inventory_size = 25;	// records how high inventories go right now (for script safety)
struct int_hash *inherent_ptech_hash = NULL;	// hash of PTECH_ that are automatic
struct player_quest *global_next_player_quest = NULL;	// for safely iterating
struct player_quest *global_next_player_quest_2 = NULL;	// it may be possible for 2 iterators at once on this
struct over_time_effect_type *free_dots_list = NULL;	// global LL of DOTs that have expired and must be free'd late to prevent issues

// progress
progress_data *progress_table = NULL;	// hashed by vnum, sorted by vnum
progress_data *sorted_progress = NULL;	// hashed by vnum, sorted by type/data
bool need_progress_refresh = FALSE;	// triggers an update of all empires' trackers

// quests
struct quest_data *quest_table = NULL;

// room templates
room_template *room_template_table = NULL;	// hash table of room templates

// sectors
sector_data *sector_table = NULL;	// sector hash table
struct sector_index_type *sector_index = NULL;	// index lists

// shops
shop_data *shop_table = NULL;	// hash table of shops (hh)

// skills
skill_data *skill_table = NULL;	// main skills hash (hh)
skill_data *sorted_skills = NULL;	// alpha hash (sorted_hh)

// socials
social_data *social_table = NULL;	// main social hash table (hh)
social_data *sorted_socials = NULL;	// alphabetic version (sorted_hh)

// strings
char *text_file_strings[NUM_TEXT_FILE_STRINGS];	// array of basic text file strings
char **intro_screens = NULL;	// array of intro screens
int num_intro_screens = 0;	// total number of intro screens

// time, seasons, and weather
struct time_info_data main_time_info;	// central time (corresponds to the latest time zone)
struct weather_data weather_info;	// the infomation about the weather
byte y_coord_to_season[MAP_HEIGHT];	// what season a given y-coord is in, as set by determine_seasons()

// tips of the day system
char **tips_of_the_day = NULL;	// array of tips
int tips_of_the_day_size = 0;	// size of tip array

// triggers
trig_data *trigger_table = NULL;	// trigger prototype hash
trig_data *trigger_list = NULL;	// DLL of all attached triggers
trig_data *random_triggers = NULL;	// DLL of live random triggers (next_in_random_triggers, prev_in_random_triggers)
trig_data *stc_next_random_trig = NULL;	// helps with trigger iteration when multiple random triggers are deleted at once
trig_data *free_trigger_list = NULL;	// LL of triggers to free (next_to_free)
int top_script_uid = OTHER_ID_BASE;	// for unique mobs/objs/vehicles in the DG Scripts system

// vehicles
vehicle_data *vehicle_table = NULL;	// main vehicle hash table
vehicle_data *vehicle_list = NULL;	// global doubly-linked list of vehicles (veh->next)

// safe vehicle iterators
vehicle_data *global_next_vehicle = NULL;	// used in limits.c
vehicle_data *next_pending_vehicle = NULL;	// used in handler.c

// world / rooms
room_data *world_table = NULL;	// hash table of the whole world
room_data *interior_room_list = NULL;	// doubly-linked list of interior rooms: room->prev_interior, room->next_interior
struct island_info *island_table = NULL; // hash table for all the islands
struct map_data world_map[MAP_WIDTH][MAP_HEIGHT];	// main world map
struct map_data *land_map = NULL;	// linked list of non-ocean
int size_of_world = 1;	// used by the instancer to adjust instance counts
struct shared_room_data ocean_shared_data;	// for BASIC_OCEAN tiles
struct vnum_hash *mapout_update_requests = NULL;	// hash table of requests for mapout updates, by room vnum
struct world_save_request_data *world_save_requests = NULL;	// hash table of save requests
struct vnum_hash *binary_world_index_updates = NULL;	// hash of updates to write to the binary world index
FILE *binary_map_fl = NULL;	// call ensure_binary_map_file_is_open() before using this, and leave it open
char *world_index_data = NULL;	// for managing the binary world file
int top_of_world_index = -1;	// current max entry index
bool save_world_after_startup = FALSE;	// if TRUE, will trigger a world save at the end of startup
bool converted_to_b5_116 = FALSE;	// triggers old world file deletes, only if it converted at startup
bool block_world_save_requests = FALSE;	// used during startup to prevent unnecessary writes


// DB_BOOT_x
struct db_boot_info_type db_boot_info[NUM_DB_BOOT_TYPES] = {
	// prefix, suffix, allow-zero-of-it
	{ WLD_PREFIX, WLD_SUFFIX, TRUE },	// DB_BOOT_WLD	-- this is no longer used as of b5.116, other than in the converter
	{ MOB_PREFIX, MOB_SUFFIX, FALSE },	// DB_BOOT_MOB
	{ OBJ_PREFIX, OBJ_SUFFIX, FALSE },	// DB_BOOT_OBJ
	{ NAMES_PREFIX, NULL, FALSE },	// DB_BOOT_NAMES
	{ LIB_EMPIRE, EMPIRE_SUFFIX, TRUE },	// DB_BOOT_EMP
	{ BOOK_PREFIX, BOOK_SUFFIX, TRUE },	// DB_BOOT_BOOKS
	{ CRAFT_PREFIX, CRAFT_SUFFIX, TRUE },	// DB_BOOT_CRAFT
	{ BLD_PREFIX, BLD_SUFFIX, TRUE },	// DB_BOOT_BLD
	{ TRG_PREFIX, TRG_SUFFIX, TRUE },	// DB_BOOT_TRG
	{ CROP_PREFIX, CROP_SUFFIX, FALSE },	// DB_BOOT_CROP
	{ SECTOR_PREFIX, SECTOR_SUFFIX, FALSE },	// DB_BOOT_SECTOR
	{ ADV_PREFIX, ADV_SUFFIX, TRUE },	// DB_BOOT_ADV
	{ RMT_PREFIX, RMT_SUFFIX, TRUE },	// DB_BOOT_RMT
	{ GLB_PREFIX, GLB_SUFFIX, TRUE },	// DB_BOOT_GLB
	{ ACCT_PREFIX, ACCT_SUFFIX, TRUE },	// DB_BOOT_ACCT
	{ AUG_PREFIX, AUG_SUFFIX, TRUE },	// DB_BOOT_AUG
	{ ARCH_PREFIX, ARCH_SUFFIX, TRUE },	// DB_BOOT_ARCH
	{ ABIL_PREFIX, ABIL_SUFFIX, TRUE },	// DB_BOOT_ABIL
	{ CLASS_PREFIX, CLASS_SUFFIX, TRUE },	// DB_BOOT_CLASS
	{ SKILL_PREFIX, SKILL_SUFFIX, TRUE },	// DB_BOOT_SKILL
	{ VEH_PREFIX, VEH_SUFFIX, TRUE },	// DB_BOOT_SKILL
	{ MORPH_PREFIX, MORPH_SUFFIX, TRUE },	// DB_BOOT_MORPH
	{ QST_PREFIX, QST_SUFFIX, TRUE },	// DB_BOOT_QST
	{ SOC_PREFIX, SOC_SUFFIX, TRUE },	// DB_BOOT_SOC
	{ FCT_PREFIX, FCT_SUFFIX, TRUE },	// DB_BOOT_FCT
	{ GEN_PREFIX, GEN_SUFFIX, TRUE },	// DB_BOOT_GEN
	{ SHOP_PREFIX, SHOP_SUFFIX, TRUE },	// DB_BOOT_SHOP
	{ PRG_PREFIX, PRG_SUFFIX, TRUE },	// DB_BOOT_PRG
	{ EVT_PREFIX, EVT_SUFFIX, TRUE },	// DB_BOOT_EVT
};


 //////////////////////////////////////////////////////////////////////////////
//// STARTUP /////////////////////////////////////////////////////////////////

/**
* Body of the booting system: this loads a lot of game data, although the
* world (objs, mobs, etc) has its own function.
*/
void boot_db(void) {
	log("Boot db -- BEGIN.");
	
	log("Loading game config system.");
	init_config_system();
	init_inherent_player_techs();
	
	log("Loading game data system.");
	load_data_table();
	
	log("Resetting the game time:");
	reset_time();

	log("Reading credits, help, info, motds, etc.");
	init_text_file_strings();
	load_intro_screens();

	// Load the world!
	boot_world();
	
	log("Loading automessages.");
	load_automessages();

	log("Loading help entries.");
	index_boot_help();
	
	// logs its own message
	load_slash_channels();
	
	log("Loading player accounts.");
	index_boot(DB_BOOT_ACCT);

	log("Generating player index.");
	build_player_index();

	log("Loading fight messages.");
	load_fight_messages();
	
	log("Loading trading post.");
	load_trading_post();

	log("Sorting command list.");
	sort_commands();
	
	// sends own log
	load_tips_of_the_day();
	
	log("Reading banned site and invalid-name list.");
	load_banned();
	Read_Invalid_List();

	if (!no_auto_deletes) {
		delete_old_players();
	}
	
	// this loads objs and mobs back into the world
	log("Resetting all rooms.");
	startup_room_reset();
	
	log("Linking and checking vehicles.");
	link_and_check_vehicles();

	// NOTE: check_version() updates many things that change from version to
	// version. See the function itself for a list of one-time updates it runs
	// on the game. This should run as late in boot_db() as possible.
	log("Checking game version...");
	check_version();
	
	// Some things runs AFTER check_version() because they rely on all version
	// updates having been run on this EmpireMUD:
	
	log("Verifying world sectors.");
	verify_sectors();
	check_sector_times(NOTHING);
	
	log("Checking for orphaned rooms...");
	delete_orphaned_rooms();
	
	log("Building quest lookup hints.");
	build_all_quest_lookups();
	
	log("Building shop lookup hints.");
	build_all_shop_lookups();
	
	log("Updating island descriptions.");
	generate_island_descriptions();
	
	// final things...
	log("Running reboot triggers.");
	run_reboot_triggers();
	
	log("Calculating empire data.");
	reread_empire_tech(NULL);
	check_for_new_map();
	setup_island_levels();
	expire_old_politics();
	check_learned_empire_crafts();
	check_nowhere_einv_all();
	check_languages_all_empires();
	verify_empire_goals();
	need_progress_refresh = TRUE;
	
	log("Checking for ruined cities...");
	check_ruined_cities();
	
	log("Abandoning lost vehicles...");
	abandon_lost_vehicles();
	
	log("Managing world memory.");
	schedule_map_unloads();
	update_instance_world_size();
	
	log("Activating workforce.");
	chore_update();
	
	log("Final startup...");
	write_whole_mapout();
	if (save_world_after_startup) {
		log(" writing fresh binary map file...");
		write_fresh_binary_map_file();
		log(" writing fresh wld files...");
		write_all_wld_files();
		log(" writing fresh binary world index...");
		write_whole_binary_world_index();
	}
	else {	// not doing a whole save but it's good to do a partial
		log(" writing pending saves...");
		perform_requested_world_saves();
		write_binary_world_index_updates();
	}
	if (converted_to_b5_116) {
		log(" deleting pre-b5.116 world files...");
		delete_pre_b5_116_world_files();
	}
	// put things here
	
	// END
	block_world_save_requests = FALSE;	// in case
	log("Boot db -- DONE.");
	boot_time = time(0);
}


/**
* Load all of the game's world data during startup. NOTE: Order matters here,
* as some items rely on others. For example, sectors must be loaded before
* rooms (because rooms have sectors).
*/
void boot_world(void) {
	// DB_BOOT_x search: boot new types in this function
	// TODO: could load them sequentially fromm the array (need to order the array)
	
	log("Loading generics.");
	index_boot(DB_BOOT_GEN);
	
	log("Loading abilities.");
	index_boot(DB_BOOT_ABIL);
	
	log("Loading classes.");
	index_boot(DB_BOOT_CLASS);
	
	log("Loading skills.");
	index_boot(DB_BOOT_SKILL);
	
	log("Loading name lists.");
	index_boot(DB_BOOT_NAMES);

	log("Loading triggers and generating index.");
	index_boot(DB_BOOT_TRG);
	
	log("Loading room templates.");
	index_boot(DB_BOOT_RMT);
	
	log("Loading buildings.");
	index_boot(DB_BOOT_BLD);
	
	log("Loading sectors.");
	index_boot(DB_BOOT_SECTOR);
	
	log("Loading crops.");
	index_boot(DB_BOOT_CROP);
	
	log("Loading vehicles.");
	index_boot(DB_BOOT_VEH);
	
	log("Loading factions.");
	index_boot(DB_BOOT_FCT);
	init_reputation();
	
	// requires sectors, buildings, and room templates -- order matters here
	log("Loading the world.");
	load_binary_map_file();	// get base data
	load_world_from_binary_index();	// override with live rooms
	build_world_map();	// ensure full world map
	build_land_map();	// determine which parts are land
	
	// requires rooms
	log("Loading empires.");
	index_boot(DB_BOOT_EMP);
	clean_empire_offenses();
	
	// requires empires
	log("  Renumbering rooms.");
	renum_world();
	
	log("  Finding islands.");
	load_islands();
	number_and_count_islands(FALSE);

	log("  Initializing start locations.");
	setup_start_locations();

	log("Loading mobs and generating index.");
	index_boot(DB_BOOT_MOB);

	log("Loading objs and generating index.");
	index_boot(DB_BOOT_OBJ);
	
	log("Loading global tables.");
	index_boot(DB_BOOT_GLB);
	
	log("Loading craft recipes.");
	index_boot(DB_BOOT_CRAFT);
	
	log("Loading shops.");
	index_boot(DB_BOOT_SHOP);
	
	log("Loading quests.");
	index_boot(DB_BOOT_QST);
	
	log("Loading books into libraries.");
	index_boot(DB_BOOT_BOOKS);
	
	log("Loading adventures.");
	index_boot(DB_BOOT_ADV);
	
	log("Loading archetypes.");
	index_boot(DB_BOOT_ARCH);
	
	log("Loading augments.");
	index_boot(DB_BOOT_AUG);
	
	log("Loading morphs.");
	index_boot(DB_BOOT_MORPH);
	
	log("Loading empire progression.");
	index_boot(DB_BOOT_PRG);
	
	log("Loading events.");
	index_boot(DB_BOOT_EVT);
	
	log("Loading socials.");
	index_boot(DB_BOOT_SOC);
	
	log("Loading instances.");
	load_instances();
	
	log("Loading empire storage and logs.");
	load_empire_storage();
	clean_empire_logs();
	
	log("Loading daily quest cycles.");
	load_daily_quest_file();
	
	log("Loading active events.");
	load_running_events_file();
	
	// check for bad data
	log("Verifying data.");
	check_abilities();
	check_and_link_faction_relations();
	check_archetypes();
	check_classes();
	check_skills();
	check_for_bad_buildings();
	check_for_bad_sectors();
	perform_force_upgrades();
	verify_running_events();
	read_ability_requirements();
	check_triggers();
	compute_generic_relations();
	
	log("Sorting data.");
	HASH_SRT(sorted_hh, sorted_abilities, sort_abilities_by_data);
	HASH_SRT(sorted_hh, sorted_archetypes, sort_archetypes_by_data);
	HASH_SRT(sorted_hh, sorted_augments, sort_augments_by_data);
	HASH_SRT(sorted_hh, sorted_classes, sort_classes_by_data);
	HASH_SRT(sorted_hh, sorted_crafts, sort_crafts_by_data);
	HASH_SRT(sorted_hh, sorted_skills, sort_skills_by_data);
	HASH_SRT(sorted_hh, sorted_socials, sort_socials_by_data);
	
	log("Checking newbie islands.");
	check_newbie_islands();
	
	log("Assigning dummy mob traits.");
	dummy_mob.rank = 1;	// prevents random crashes when IS_NPC isn't checked correctly in new code
}


 //////////////////////////////////////////////////////////////////////////////
//// TEMPORARY DATA SYSTEM ///////////////////////////////////////////////////

// used to store data temporarily during startup, before a vnum
// can be converted to a room pointer
struct trd_type {
	room_vnum vnum;
	room_vnum home_room;
	empire_vnum owner;

	UT_hash_handle hh;
};

// holds vnums until renum_world()
struct trd_type *temporary_room_data = NULL;


/**
* Creates/gets a temporary_room_data entry.
*
* @param room_vnum vnum The room to store data for.
*/
static struct trd_type *get_trd(room_vnum vnum) {
	struct trd_type *trd;
	
	HASH_FIND_INT(temporary_room_data, &vnum, trd);
	if (!trd) {
		CREATE(trd, struct trd_type, 1);
		trd->vnum = vnum;
		HASH_ADD_INT(temporary_room_data, vnum, trd);
		
		trd->home_room = NOWHERE;
		trd->owner = NOTHING;
	}
	
	return trd;
}

/**
* Store home room vnum temporarily.
*
* @param room_vnum vnum The room to store data for.
* @param room_vnum home_room The vnum of that room's home_room.
*/
void add_trd_home_room(room_vnum vnum, room_vnum home_room) {
	struct trd_type *trd = get_trd(vnum);
	trd->home_room = home_room;
}


/**
* Store owner vnum temporarily.
*
* @param room_vnum vnum The room to store data for.
* @param empire_vnum owner The empire who owns it.
*/
void add_trd_owner(room_vnum vnum, empire_vnum owner) {
	struct trd_type *trd = get_trd(vnum);
	trd->owner = owner;
}


 //////////////////////////////////////////////////////////////////////////////
//// DATA INTEGRITY //////////////////////////////////////////////////////////

/**
* This function checks the world, craft recipes, object storage, and open
* .olc editors for deleted buildings, and removes them. It is called on
* startup and should also be called any time a building is deleted.
*/
void check_for_bad_buildings(void) {
	struct obj_storage_type *store, *next_store;
	struct bld_relation *relat, *next_relat;
	bld_data *bld, *next_bld;
	room_data *room, *next_room;
	craft_data *craft, *next_craft;
	obj_data *obj, *next_obj;
	descriptor_data *dd;
	bool deleted = FALSE;
	
	// check buildings and adventures
	HASH_ITER(hh, world_table, room, next_room) {
		if (ROOM_SECT_FLAGGED(room, SECTF_MAP_BUILDING) && !GET_BUILDING(room)) {
			// map building
			log(" removing building at %d for bad building type", GET_ROOM_VNUM(room));
			disassociate_building(room);
		}
		else if (IS_CITY_CENTER(room) && (!ROOM_OWNER(room) || !find_city_entry(ROOM_OWNER(room), room))) {
			// city center with no matching city
			log(" removing city center at %d for lack of city entry", GET_ROOM_VNUM(room));
			disassociate_building(room);
		}
		else if (GET_ROOM_VNUM(room) >= MAP_SIZE && ROOM_SECT_FLAGGED(room, SECTF_INSIDE) && !GET_BUILDING(room)) {
			// designated room
			log(" deleting room %d for bad building type", GET_ROOM_VNUM(room));
			delete_room(room, FALSE);	// must check_all_exits
			deleted = TRUE;
		}
		else if (GET_ROOM_VNUM(room) >= MAP_SIZE && ROOM_SECT_FLAGGED(room, SECTF_ADVENTURE) && !GET_ROOM_TEMPLATE(room)) {
			// adventure room
			log(" deleting room %d for bad template type", GET_ROOM_VNUM(room));
			delete_room(room, FALSE);	// must check_all_exits
			deleted = TRUE;
		}
		else if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) && !find_instance_by_room(room, TRUE, FALSE)) {
			// room is marked as an instance entrance, but no instance is associated with it
			log(" unlinking instance entrance room %d for no association with an instance", GET_ROOM_VNUM(room));
			unlink_instance_entrance(room, NULL, FALSE);
			prune_instances();	// cleans up rooms too
		}
		/* This probably isn't necessary and having it here will cause roads to be pulled up as of b3.17 -paul
		else if (COMPLEX_DATA(room) && !GET_BUILDING(room) && !GET_ROOM_TEMPLATE(room)) {
			log(" removing complex data from %d for no building, no template data", GET_ROOM_VNUM(room));
			disassociate_building(room);
		}
		*/
	}
	if (deleted) {
		check_all_exits();
	}
	
	// check craft "build" recipes: disable
	HASH_ITER(hh, craft_table, craft, next_craft) {
		if (CRAFT_IS_BUILDING(craft) && !building_proto(GET_CRAFT_BUILD_TYPE(craft))) {
			GET_CRAFT_BUILD_TYPE(craft) = NOTHING;
			SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
			log(" disabling craft %d for bad building type", GET_CRAFT_VNUM(craft));
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// check craft recipes of people using olc: disable and warn
	for (dd = descriptor_list; dd; dd = dd->next) {
		if (GET_OLC_TYPE(dd) == OLC_CRAFT && GET_OLC_CRAFT(dd)) {
			if (CRAFT_IS_BUILDING(GET_OLC_CRAFT(dd)) && !building_proto(GET_OLC_CRAFT(dd)->build_type)) {
				GET_OLC_CRAFT(dd)->build_type = NOTHING;
				SET_BIT(GET_OLC_CRAFT(dd)->flags, CRAFT_IN_DEVELOPMENT);
				msg_to_desc(dd, "&RYou are editing a craft recipe whose building has been deleted. Building type removed.&0\r\n");
			}
		}
	}
	
	// check obj protos for storage locations that used a deleted building: delete
	HASH_ITER(hh, object_table, obj, next_obj) {
		for (store = GET_OBJ_STORAGE(obj); store; store = next_store) {
			next_store = store->next;
			
			if (store->type == TYPE_BLD && store->vnum != NOTHING && !building_proto(store->vnum)) {
				LL_DELETE(obj->proto_data->storage, store);
				free(store);
				log(" removing storage for obj %d for bad building type", obj->vnum);
				save_library_file_for_vnum(DB_BOOT_OBJ, obj->vnum);
			}
		}
	}
	
	// check open obj editors for storage in deleted buildings: delete
	for (dd = descriptor_list; dd; dd = dd->next) {
		if (GET_OLC_TYPE(dd) == OLC_OBJECT && GET_OLC_OBJECT(dd)) {
			for (store = GET_OBJ_STORAGE(GET_OLC_OBJECT(dd)); store; store = next_store) {
				next_store = store->next;
			
				if (store->type == TYPE_BLD && store->vnum != NOTHING && !building_proto(store->vnum)) {
					LL_DELETE(GET_OLC_OBJECT(dd)->proto_data->storage, store);
					free(store);
					msg_to_desc(dd, "&RYou are editing an object whose storage building has been deleted. Building type removed.&0\r\n");
				}
			}
		}
	}
	
	// check buildings for bad relations: unset
	HASH_ITER(hh, building_table, bld, next_bld) {
		LL_FOREACH_SAFE(GET_BLD_RELATIONS(bld), relat, next_relat) {
			if (bld_relationship_vnum_types[relat->type] == TYPE_BLD && (relat->vnum == NOTHING || !building_proto(relat->vnum))) {
				log(" removing %s relationship for building %d for bad building type", bld_relationship_types[relat->type], GET_BLD_VNUM(bld));
				LL_DELETE(GET_BLD_RELATIONS(bld), relat);
				free(relat);
				save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
			}
			if (bld_relationship_vnum_types[relat->type] == TYPE_VEH && (relat->vnum == NOTHING || !vehicle_proto(relat->vnum))) {
				log(" removing %s relationship for building %d for bad vehicle type", bld_relationship_types[relat->type], GET_BLD_VNUM(bld));
				LL_DELETE(GET_BLD_RELATIONS(bld), relat);
				free(relat);
				save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
			}
		}
	}
}


/**
* Look for places that store a sector and might have bad ones. It does not
* save the changes to file on its own; it will re-fix them on every boot.
*/
void check_for_bad_sectors(void) {
	struct evolution_data *evo, *next_evo;
	sector_data *sect, *next_sect;

	HASH_ITER(hh, sector_table, sect, next_sect) {
		// look for evolutions with bad targets
		for (evo = GET_SECT_EVOS(sect); evo; evo = next_evo) {
			next_evo = evo->next;
			
			// oops: remove bad evo
			if (!sector_proto(evo->becomes)) {
				LL_DELETE(GET_SECT_EVOS(sect), evo);
				free(evo);
			}
		}
	}
}


/**
* This function deletes rooms which are orphaned -- have no home room or map
* association left. It is only called at startup. It is safe to remove a non-
* map-room's "home_room" and let the next reboot clean up the room itself.
*/
void delete_orphaned_rooms(void) {
	room_data *room, *next_room;
	bool deleted = FALSE;
	
	// start at the end of the map!
	DL_FOREACH_SAFE2(interior_room_list, room, next_room, next_interior) {
		// vehicles are checked separately
		if (ROOM_AFF_FLAGGED(room, ROOM_AFF_IN_VEHICLE) && HOME_ROOM(room) == room) {
			continue;
		}
		
		// adventure zones: cleans up orphans on its own
		if (IS_ADVENTURE_ROOM(room)) {
			continue;
		}
		
		if (!COMPLEX_DATA(room) || !COMPLEX_DATA(room)->home_room || !COMPLEX_DATA(HOME_ROOM(room))) {
			if (!ROOM_BLD_FLAGGED(room, BLD_NO_DELETE)) {
				log("Deleting room %d due to missing homeroom.", GET_ROOM_VNUM(room));
				delete_room(room, FALSE);	// must check_all_exits
				deleted = TRUE;
			}
		}
	}
	
	if (deleted) {
		check_all_exits();
	}
}


/**
* This ensures that every room has a valid sector.
*/
void verify_sectors(void) {	
	sector_data *use_sect, *sect, *next_sect;
	struct map_data *map, *next_map;
	crop_data *new_crop;
	room_data *room;
	
	// ensure we have a backup sect
	use_sect = sector_proto(config_get_int("default_land_sect"));
	if (!use_sect) {	// backup: try to find one
		use_sect = find_first_matching_sector(NOBITS, SECTF_HAS_CROP_DATA | SECTF_CROP | SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE, NOBITS);
	}
	if (!use_sect) {	// backup: just pull the first one in the list
		HASH_ITER(hh, sector_table, sect, next_sect) {
			use_sect = sect;
			break;
		}
	}
	
	// we MUST have at least 1 by now, but just in case...
	if (!use_sect) {
		log("SYSERR: No valid sector for verify_sectors");
		exit(1);
	}
	
	// check whole world
	LL_FOREACH_SAFE(land_map, map, next_map) {
		room = map->room;	// if loaded
		
		if (!map->sector_type) {
			// can't use change_terrain() here -- note check_terrain_height() does not run here either
			perform_change_sect(NULL, map, use_sect);
		}
		if (!map->base_sector) {
			if (!room) {
				room = real_room(map->vnum);	// load it into memory
			}
			change_base_sector(room, use_sect);
		}
		
		// also check for missing crop data
		if (SECT_FLAGGED(map->sector_type, SECTF_HAS_CROP_DATA | SECTF_CROP) && !map->crop_type) {
			if (!room) {
				room = real_room(map->vnum);	// load it into memory
			}
			new_crop = get_potential_crop_for_location(room, FALSE);
			set_crop_type(room, new_crop ? new_crop : crop_table);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// POST-PROCESSING /////////////////////////////////////////////////////////

/**
* This is used after rooms are loaded to assign home rooms.
*/
void process_temporary_room_data(void) {
	room_data *room, *home;
	struct trd_type *trd, *next_trd;

	// do temporary room data (e.g. home room) -- this frees temporary room data as it goes
	HASH_ITER(hh, temporary_room_data, trd, next_trd) {
		if ((room = real_room(trd->vnum))) {
			// home room
			if (trd->home_room != NOWHERE && (home = real_room(trd->home_room))) {
				if (!COMPLEX_DATA(room)) {
					COMPLEX_DATA(room) = init_complex_data();
				}
				COMPLEX_DATA(room)->home_room = home;
			}
			// owner
			if (trd->owner != NOTHING) {
				ROOM_OWNER(room) = real_empire(trd->owner);
			}
		}
		HASH_DEL(temporary_room_data, trd);
		free(trd);
	}
}


/**
* resolve all vnums in the world and schedules some events that can't be
* scheduled until this point.
*/
void renum_world(void) {
	room_data *room, *next_room, *home;
	struct room_direction_data *ex, *next_ex;
	struct affected_type *af;
	struct map_data *map;
	
	process_temporary_room_data();
	
	HASH_ITER(hh, world_table, room, next_room) {
		// schedule affects
		LL_FOREACH(ROOM_AFFECTS(room), af) {
			schedule_room_affect_expire(room, af);
		}
		
		if (COMPLEX_DATA(room)) {
			// events
			if (COMPLEX_DATA(room)->burn_down_time > 0 && !find_stored_event_room(room, SEV_BURN_DOWN)) {
				schedule_burn_down(room);
			}
			
			// exit targets
			for (ex = COMPLEX_DATA(room)->exits; ex; ex = next_ex) {
				next_ex = ex->next;
				
				// validify exit
				ex->room_ptr = real_room(ex->to_room);
				if (ex->room_ptr) {
					++GET_EXITS_HERE(ex->room_ptr);
				}
				else {
					if (ex->keyword) {
						free(ex->keyword);
					}
					LL_DELETE(COMPLEX_DATA(room)->exits, ex);
					free(ex);
				}
			}
			
			// count interior rooms -- only for non-map locations
			if (GET_ROOM_VNUM(room) > MAP_SIZE && (home = HOME_ROOM(room)) != room) {
				if (COMPLEX_DATA(home)) {
					COMPLEX_DATA(home)->inside_rooms++;
				}
				if (GET_ROOM_VEHICLE(home)) {
					++VEH_INSIDE_ROOMS(GET_ROOM_VEHICLE(home));
				}
				
				GET_ISLAND_ID(room) = GET_ISLAND_ID(home);
				GET_ISLAND(room) = GET_ISLAND(home);
				GET_MAP_LOC(room) = GET_MAP_LOC(home);
			}
		}
		
		// other room setup
		check_tavern_setup(room);
		
		// ensure affects
		affect_total_room(room);
	}
	
	// schedule map events
	LL_FOREACH(land_map, map) {
		if (get_extra_data(map->shared->extra_data, ROOM_EXTRA_TRENCH_FILL_TIME) > 0) {
			schedule_trench_fill(map);
		}
		if (get_extra_data(map->shared->extra_data, ROOM_EXTRA_SEED_TIME)) {
			schedule_crop_growth(map);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// I/O HELPERS /////////////////////////////////////////////////////////////

/*
 * Thanks to Andrey (andrey@alex-ua.com) for this bit of code, although I
 * did add the 'goto' and changed some "while()" into "do { } while()".
 *	-gg 6/24/98 (technically 6/25/98, but I care not.)
 */
int count_alias_records(FILE *fl) {
	char key[READ_SIZE], next_key[READ_SIZE];
	char line[READ_SIZE], *scan;
	int total_keywords = 0;

	/* get the first keyword line */
	get_one_line(fl, key);

	while (*key != '$') {
		/* skip the text */
		do {
			get_one_line(fl, line);
			if (feof(fl))
				goto ackeof;
		} while (*line != '#');

		/* now count keywords */
		scan = key;
		do {
			scan = any_one_word(scan, next_key);
			if (*next_key)
				++total_keywords;
		} while (*next_key);

		/* get next keyword line (or $) */
		get_one_line(fl, key);

		if (feof(fl))
			goto ackeof;
	}

	return (total_keywords);

	/* No, they are not evil. -gg 6/24/98 */
	ackeof:	
		log("SYSERR: Unexpected end of help file.");
		exit(1);	/* Some day we hope to handle these things better... */
}


/* read contents of a text file, and place in buf */
int file_to_string(const char *name, char *buf) {
	FILE *fl;
	char tmp[READ_SIZE+3];

	*buf = '\0';

	if (!(fl = fopen(name, "r"))) {
		log("SYSERR: reading %s: %s", name, strerror(errno));
		return (-1);
	}
	do {
		fgets(tmp, READ_SIZE, fl);
		tmp[strlen(tmp) - 1] = '\0'; /* take off the trailing \n */
		strcat(tmp, "\r\n");

		if (!feof(fl)) {
			if (strlen(buf) + strlen(tmp) + 1 > MAX_STRING_LENGTH) {
				log("SYSERR: %s: string too big (%d max)", name, MAX_STRING_LENGTH);
				*buf = '\0';
				return (-1);
			}
			strcat(buf, tmp);
		}
	} while (!feof(fl));

	fclose(fl);

	return (0);
}


/*
 * Steps:
 *   1: Make sure no one is using the pointer in paging.
 *   2: Read contents of a text file.
 *   3: Allocate space.
 *   4: Point 'buf' to it.
 *
 * We don't want to free() the string that someone may be
 * viewing in the pager.  page_string() keeps the internal
 * str_dup()'d copy on ->showstr_head and it won't care
 * if we delete the original.  Otherwise, strings are kept
 * on ->showstr_vector but we'll only match if the pointer
 * is to the string we're interested in and not a copy.
 */
int file_to_string_alloc(const char *name, char **buf) {
	char temp[MAX_STRING_LENGTH];
	descriptor_data *in_use;

	for (in_use = descriptor_list; in_use; in_use = in_use->next)
		if (in_use->showstr_vector && *in_use->showstr_vector == *buf)
			return (-1);

	/* Lets not free() what used to be there unless we succeeded. */
	if (file_to_string(name, temp) < 0)
		return (-1);

	if (*buf)
		free(*buf);

	*buf = str_dup(temp);
	return (0);
}


// reads in one action line
char *fread_action(FILE * fl, int nr) {
	char buf[MAX_STRING_LENGTH], *rslt;

	fgets(buf, MAX_STRING_LENGTH, fl);
	if (feof(fl)) {
		log("SYSERR: fread_action - unexpected EOF near action #%d", nr+1);
		exit(1);
		}
	if (*buf == '#')
		return (NULL);
	else {
		*(buf + strlen(buf) - 1) = '\0';
		CREATE(rslt, char, strlen(buf) + 1);
		strcpy(rslt, buf);
		return (rslt);
	}
}


/* read and allocate space for a '~'-terminated string from a given file */
char *fread_string(FILE * fl, char *error) {
	char buf[MAX_STRING_LENGTH], tmp[MAX_INPUT_LENGTH], *rslt;
	int done = 0, length = 0, templength;
	register int pos;

	*buf = '\0';

	do {
		if (!fgets(tmp, 512, fl)) {
			log("SYSERR: fread_string: format error at or near %s", error);
			exit(1);
		}
				
		// upgrade to allow ~ when not at the end
		pos = strlen(tmp);
		
		// backtrack past any \r\n or trailing space or tab
		for (--pos; pos > 0 && (tmp[pos] == '\r' || tmp[pos] == '\n' || tmp[pos] == ' ' || tmp[pos] == '\t'); --pos);
		
		// look for a trailing ~
		if (tmp[pos] == '~') {
			tmp[pos] = '\0';
			done = 1;
		}
		else {
			strcpy(tmp + pos + (pos > 0 ? 1 : 0), "\r\n");
		}

		templength = strlen(tmp);

		if (length + templength >= MAX_STRING_LENGTH) {
			log("SYSERR: fread_string: string too large (db.c)");
			log("%s", error);
			exit(1);
		}
		else {
			strcat(buf + length, tmp);
			length += templength;
		}
	} while (!done);

	/* allocate space for the new string and copy it */
	if (strlen(buf) > 0) {
		CREATE(rslt, char, length + 1);
		strcpy(rslt, buf);
	}
	else {
		rslt = NULL;
	}

	return (rslt);
}


/**
* Loads one line from a file into buf.
*
* @param FILE *fl The file to read from.
* @param char *buf Where to store the string.
*/
void get_one_line(FILE *fl, char *buf) {
	if (fgets(buf, READ_SIZE, fl) == NULL) {
		log("SYSERR: error reading help file: not terminated with $?");
		exit(1);
	}

	buf[strlen(buf) - 1] = '\0'; /* take off the trailing \n */
}


/**
* Initializes all the text_file_strings[] and reads them, at startup ONLY.
*/
void init_text_file_strings(void) {
	int iter;
	
	for (iter = 0; iter < NUM_TEXT_FILE_STRINGS; ++iter) {
		text_file_strings[iter] = NULL;
		if (text_file_data[iter].filename && *text_file_data[iter].filename) {
			file_to_string_alloc(text_file_data[iter].filename, &text_file_strings[iter]);
		}
	}
}


/**
* Reloads 1 text file, by type.
*
* @param int type Any TEXT_FILE_ type.
*/
void reload_text_string(int type) {
	if (type >= 0 && type < NUM_TEXT_FILE_STRINGS && text_file_data[type].filename && *text_file_data[type].filename) {
		if (text_file_strings[type]) {
			free(text_file_strings[type]);
			text_file_strings[type] = NULL;
		}
		file_to_string_alloc(text_file_data[type].filename, &text_file_strings[type]);
	}
}



 //////////////////////////////////////////////////////////////////////////////
//// HELP I/O ////////////////////////////////////////////////////////////////

/**
* Loads one help file.
*
* @param FILE *fl The input file.
*/
void load_help(FILE *fl) {
	char key[READ_SIZE+1], next_key[READ_SIZE+1], entry[32384];
	char line[READ_SIZE+1], *scan;
	struct help_index_element el;
	int iter;

	/* get the first keyword line */
	get_one_line(fl, key);
	while (*key != '$') {
		/* read in the corresponding help entry */
		strcpy(entry, strcat(key, "\r\n"));
		get_one_line(fl, line);
		while (*line != '#') {
			strcat(entry, strcat(line, "\r\n"));
			get_one_line(fl, line);
		}

		el.level = 0;

		if (*line == '#' && *(line + 1)) {
			// this uses switch, not alpha-math, because some of these levels
			// may be the same as others, but the letters still need to work
			// consistently -paul
			switch (*(line + 1)) {
				case 'a': {
					el.level = LVL_IMPL;
					break;
				}
				case 'b': {
					el.level = LVL_CIMPL;
					break;
				}
				case 'c': {
					el.level = LVL_ASST;
					break;
				}
				case 'd': {
					el.level = LVL_START_IMM;
					break;
				}
				case 'e': {
					el.level = LVL_GOD;
					break;
				}
				case 'f': {	// everyone can see this anyway
					el.level = LVL_MORTAL;
					break;
				}
			}
		}
		
		// convert ampersand codes like &0 to tab codes like \t0 -- string length won't change
		for (iter = 0; iter < strlen(entry); ++iter) {
			if (entry[iter] == '&') {
				entry[iter] = '\t';
				// skip next letter as part of the code:
				++iter;
			}
		}
		
		/* now, add the entry to the index with each keyword on the keyword line */
		el.duplicate = 0;
		el.entry = str_dup(entry);
		scan = any_one_word(key, next_key);
		while (*next_key) {
			el.keyword = str_dup(next_key);
			help_table[top_of_helpt++] = el;
			el.duplicate++;
			scan = any_one_word(scan, next_key);
		}

		/* get next keyword line (or $) */
		get_one_line(fl, key);
	}
}


/**
* Loads the help index and every help file therein.
*/
void index_boot_help(void) {
	const char *index_filename, *prefix = NULL;	/* NULL or egcs 1.1 complains */
	FILE *index, *db_file;
	int rec_count = 0, size[2];

	prefix = HLP_PREFIX;

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
		else
			rec_count += count_alias_records(db_file);

		fclose(db_file);
		fscanf(index, "%s\n", buf1);
	}

	/* Exit if 0 records, unless this is shops */
	if (!rec_count) {
		log("SYSERR: boot error - 0 records counted in %s/%s.", prefix, index_filename);
		exit(1);
	}

	/* Any idea why you put this here Jeremy? */
	rec_count++;

	/*
	 * NOTE: "bytes" does _not_ include strings or other later malloc'd things.
	 */
	CREATE(help_table, struct help_index_element, rec_count);
	size[0] = sizeof(struct help_index_element) * rec_count;
	log("   %d entries, %d bytes.", rec_count, size[0]);

	rewind(index);
	fscanf(index, "%s\n", buf1);
	while (*buf1 != '$') {
		sprintf(buf2, "%s%s", prefix, buf1);
		if (!(db_file = fopen(buf2, "r"))) {
			log("SYSERR: %s: %s", buf2, strerror(errno));
			exit(1);
		}

		/*
		 * If you think about it, we have a race here.  Although, this is the
		 * "point-the-gun-at-your-own-foot" type of race.
		 */
		load_help(db_file);

		fclose(db_file);
		fscanf(index, "%s\n", buf1);
	}
	fclose(index);

	qsort(help_table, top_of_helpt, sizeof(struct help_index_element), help_sort);
	top_of_helpt--;
}


 //////////////////////////////////////////////////////////////////////////////
//// ISLAND SETUP ////////////////////////////////////////////////////////////

// this helps find places to number
struct island_num_data_t {
	struct map_data *loc;
	struct island_num_data_t *next;	// LL
};

struct island_num_data_t *island_stack = NULL;	// global stack


// pops an item from the stack and returns it, or NULL if empty
struct island_num_data_t *pop_island(void) {
	struct island_num_data_t *temp = NULL;
	
	if (island_stack) {
		temp = island_stack;
		island_stack = temp->next;
	}
	
	return temp;
}


// push a location onto the stack
void push_island(struct map_data *loc) {
	struct island_num_data_t *island;
	CREATE(island, struct island_num_data_t, 1);
	island->loc = loc;
	LL_PREPEND(island_stack, island);
}


/**
* Checks the newbie islands and applies their rules (abandons land).
*/
void check_newbie_islands(void) {
	struct island_info *isle = NULL, *ii, *next_ii;
	room_data *room, *next_room;
	int num_newbie_isles;
	empire_data *emp;
	empire_vnum vnum;
	
	// for limit-tracking
	struct cni_track {
		empire_vnum vnum;	// which empire
		int count;	// how many
		UT_hash_handle hh;
	};
	struct cni_track *cni, *next_cni, *list = NULL;
	
	// count islands
	num_newbie_isles = 0;
	HASH_ITER(hh, island_table, ii, next_ii) {
		if (IS_SET(ii->flags, ISLE_NEWBIE)) {
			++num_newbie_isles;
		}
	}
	
	HASH_ITER(hh, world_table, room, next_room) {
		if (GET_ROOM_VNUM(room) >= MAP_SIZE || !(isle = GET_ISLAND(room))) {
			continue;
		}
		
		// ensure ownership and that the empire is "not new"
		if (!(emp = ROOM_OWNER(room)) || (EMPIRE_CREATE_TIME(emp) + (config_get_int("newbie_island_day_limit") * SECS_PER_REAL_DAY)) > time(0)) {
			continue;
		}
		
		// apply newbie rules?
		if (IS_SET(isle->flags, ISLE_NEWBIE)) {
			
			// find/make tracker
			vnum = EMPIRE_VNUM(emp);
			HASH_FIND_INT(list, &vnum, cni);
			if (!cni) {
				CREATE(cni, struct cni_track, 1);
				cni->vnum = vnum;
				HASH_ADD_INT(list, vnum, cni);
			}
			
			cni->count += 1;
			
			if (cni->count > num_newbie_isles) {
				log_to_empire(emp, ELOG_TERRITORY, "(%d, %d) abandoned on newbie island", FLAT_X_COORD(room), FLAT_Y_COORD(room));
				abandon_room(room);
			}
		}
	}
	
	// clean up
	HASH_ITER(hh, list, cni, next_cni) {
		HASH_DEL(list, cni);
		free(cni);
	}
}


/**
* Numbers an island and pushes its neighbors onto the stack if they need island
* ids.
*
* @param struct map_data *map A map location.
* @param int island The island id.
* @param struct island_info *ptr Optional: Prevent having to look up the island pointer if it's already available. (NULL to detect here)
* @param ubyte pathfind_key Uses this to prevent re-working the same tile.
*/
void number_island(struct map_data *map, int island, struct island_info *isle_ptr, ubyte pathfind_key) {
	int x, y, new_x, new_y;
	struct map_data *tile;
	
	if (map->pathfind_key == pathfind_key) {
		return;	// no work
	}
	
	map->pathfind_key = pathfind_key;
	map->shared->island_id = island;
	map->shared->island_ptr = isle_ptr ? isle_ptr : get_island(island, TRUE);
	
	// check neighboring tiles
	for (x = -1; x <= 1; ++x) {
		for (y = -1; y <= 1; ++y) {
			// same tile
			if (x == 0 && y == 0) {
				continue;
			}
			
			if (get_coord_shift(MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum), x, y, &new_x, &new_y)) {
				tile = &(world_map[new_x][new_y]);
				
				if (tile->pathfind_key != pathfind_key && !SECT_FLAGGED(tile->sector_type, SECTF_NON_ISLAND) && tile->shared->island_id <= 0) {
					// add to stack
					push_island(tile);
				}
			}
		}
	}
}


/**
* This function is normally run at startup to make sure all islands are
* correctly numbered. It prefers to expand existing islands. You can also
* call it with reset=TRUE to totally reset all island data, but you may not
* like what this does to empire inventories on existing games.
*
* @param bool reset If TRUE, clears all existing island IDs and renumbers.
*/
void number_and_count_islands(bool reset) {
	// helper type
	struct island_read_data {
		int id;
		int size;
		int sum_x, sum_y;	// for averaging center
		room_vnum edge[NUM_SIMPLE_DIRS];	// detected edges
		int edge_val[NUM_SIMPLE_DIRS];	// for detecting edges
		UT_hash_handle hh;
	};
	
	struct island_read_data *data, *next_data, *list = NULL;
	bool re_empire = (top_island_num != -1);
	struct island_num_data_t *item;
	struct island_info *isle, *use_isle;
	room_data *room, *next_room, *maploc;
	struct map_data *map;
	int iter, use_id;
	ubyte pathfind_key;
	
	// find top island id (and reset if requested)
	top_island_num = 0;	// this ensures any new island ID has a minimum of 1
	for (map = land_map; map; map = map->next) {
		if (reset || SECT_FLAGGED(map->sector_type, SECTF_NON_ISLAND)) {
			map->shared->island_id = NO_ISLAND;
		}
		else {
			top_island_num = MAX(top_island_num, map->shared->island_id);
		}
	}
	
	// now update the whole world -- if it's not a reset, this is done in two stages:
	
	// 1. expand EXISTING islands
	if (!reset) {
		pathfind_key = get_pathfind_key();
		for (map = land_map; map; map = map->next) {
			if (map->shared->island_id == NO_ISLAND || map->shared->island_id == 0) {
				continue;
			}
			
			use_id = map->shared->island_id;
			use_isle = map->shared->island_ptr;
			push_island(map);
			
			while ((item = pop_island())) {
				number_island(item->loc, use_id, use_isle, pathfind_key);
				free(item);
			}
		}
	}
	
	// 2. look for places that have no island id but need one -- and also measure islands while we're here
	pathfind_key = get_pathfind_key();
	for (map = land_map; map; map = map->next) {
		if (map->shared->island_id == NO_ISLAND && !SECT_FLAGGED(map->sector_type, SECTF_NON_ISLAND)) {
			use_id = ++top_island_num;
			use_isle = get_island(use_id, TRUE);
			push_island(map);
			
			while ((item = pop_island())) {
				number_island(item->loc, use_id, use_isle, pathfind_key);
				free(item);
			}
		}
		else {
			use_id = map->shared->island_id;
		}
		
		HASH_FIND_INT(list, &use_id, data);
		if (!data) {	// or create one
			CREATE(data, struct island_read_data, 1);
			data->id = use_id;
			for (iter = 0; iter < NUM_SIMPLE_DIRS; ++iter) {
				data->edge[iter] = NOWHERE;
				data->edge_val[iter] = NOWHERE;
			}
			HASH_ADD_INT(list, id, data);
		}

		// update helper data
		data->size += 1;
		data->sum_x += MAP_X_COORD(map->vnum);
		data->sum_y += MAP_Y_COORD(map->vnum);
	
		// detect edges
		if (data->edge[NORTH] == NOWHERE || MAP_Y_COORD(map->vnum) > data->edge_val[NORTH]) {
			data->edge[NORTH] = map->vnum;
			data->edge_val[NORTH] = MAP_Y_COORD(map->vnum);
		}
		if (data->edge[SOUTH] == NOWHERE || MAP_Y_COORD(map->vnum) < data->edge_val[SOUTH]) {
			data->edge[SOUTH] = map->vnum;
			data->edge_val[SOUTH] = MAP_Y_COORD(map->vnum);
		}
		if (data->edge[EAST] == NOWHERE || MAP_X_COORD(map->vnum) > data->edge_val[EAST]) {
			data->edge[EAST] = map->vnum;
			data->edge_val[EAST] = MAP_X_COORD(map->vnum);
		}
		if (data->edge[WEST] == NOWHERE || MAP_X_COORD(map->vnum) < data->edge_val[WEST]) {
			data->edge[WEST] = map->vnum;
			data->edge_val[WEST] = MAP_X_COORD(map->vnum);
		}
	}
	
	// process and free the island helpers
	HASH_ITER(hh, list, data, next_data) {
		if ((isle = get_island(data->id, TRUE))) {
			isle->tile_size = data->size;

			// detect center
			room = real_room(((data->sum_y / data->size) * MAP_WIDTH) + (data->sum_x / data->size));
			isle->center = room ? GET_ROOM_VNUM(room) : NOWHERE;
			
			// store edges
			for (iter = 0; iter < NUM_SIMPLE_DIRS; ++iter) {
				isle->edge[iter] = data->edge[iter];
			}
		}
		
		// and free
		HASH_DEL(list, data);
		free(data);
	}
	
	// update all island pointers
	LL_FOREACH(land_map, map) {
		map->shared->island_ptr = (map->shared->island_id != NO_ISLAND) ? get_island(map->shared->island_id, TRUE) : NULL;
	}
	
	// update all interior rooms
	HASH_ITER(hh, world_table, room, next_room) {
		if (!(maploc = get_map_location_for(room)) || (maploc == room)) {
			continue;
		}
		
		GET_ISLAND_ID(room) = GET_ISLAND_ID(maploc);
		GET_ISLAND(room) = GET_ISLAND(maploc);
		GET_MAP_LOC(room) = (GET_ROOM_VNUM(maploc) < MAP_SIZE ? &(world_map[FLAT_X_COORD(maploc)][FLAT_Y_COORD(maploc)]) : NULL);
	}
	
	// lastly
	if (re_empire) {
		reread_empire_tech(NULL);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MOBILE LOADING //////////////////////////////////////////////////////////

/* clear ALL the working variables of a char; do NOT free any space alloc'ed */
void clear_char(char_data *ch) {
	memset((char *) ch, 0, sizeof(char_data));

	ch->vnum = NOBODY;
	GET_POS(ch) = POS_STANDING;
	SET_SIZE(ch) = SIZE_NORMAL;
	MOB_INSTANCE_ID(ch) = NOTHING;
	MOB_DYNAMIC_SEX(ch) = NOTHING;
	MOB_DYNAMIC_NAME(ch) = NOTHING;
	MOB_LANGUAGE(ch) = NOTHING;
	MOB_PURSUIT_LEASH_LOC(ch) = NOWHERE;
	GET_ROPE_VNUM(ch) = NOTHING;
	
	ch->customized = FALSE;
}


/**
* This is called during creation, and before loading a player from file. It
* initializes things that should be -1/NOTHINGs.
*
* @param char_data *ch A player.
*/
void init_player_specials(char_data *ch) {
	int iter;
	
	if (IS_NPC(ch)) {
		syslog(SYS_ERROR, 0, TRUE, "SYSERR: init_player_specials called on an NPC");
		return;
	}
	
	// ensures they have unique player_specials
	if (!(ch->player_specials) || ch->player_specials == &dummy_mob) {
		CREATE(ch->player_specials, struct player_special_data, 1);
	}
	
	GET_LAST_COMPANION(ch) = NOTHING;
	GET_LAST_ROOM(ch) = NOWHERE;
	GET_LOADROOM(ch) = NOWHERE;
	GET_LOAD_ROOM_CHECK(ch) = NOWHERE;
	GET_MARK_LOCATION(ch) = NOWHERE;
	GET_MOUNT_VNUM(ch) = NOTHING;
	GET_PLEDGE(ch) = NOTHING;
	GET_TOMB_ROOM(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_RETURN_MAP(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_INSTANCE_ID(ch) = NOTHING;
	GET_LAST_TELL(ch) = NOBODY;
	GET_TEMPORARY_ACCOUNT_ID(ch) = NOTHING;
	GET_IMMORTAL_LEVEL(ch) = -1;	// Not an immortal
	USING_POISON(ch) = NOTHING;
	GET_MAP_MEMORY_LOADED(ch) = FALSE;
	
	for (iter = 0; iter < NUM_ARCHETYPE_TYPES; ++iter) {
		CREATION_ARCHETYPE(ch, iter) = NOTHING;
	}
}


/**
* Create a new mobile from a prototype. You should almost always call this with
* with_triggers = TRUE.
*
* @param mob_vnum nr The number to load.
* @param bool with_triggers If TRUE, attaches all triggers.
* @return char_data* The instantiated mob.
*/
char_data *read_mobile(mob_vnum nr, bool with_triggers) {
	char_data *mob, *proto;
	int iter;

	if (!(proto = mob_proto(nr))) {
		log("WARNING: Mobile vnum %d does not exist in database.", nr);
		// grab first one (bug)
		proto = mobile_table;
	}

	CREATE(mob, char_data, 1);
	clear_char(mob);
	*mob = *proto;
	DL_PREPEND(character_list, mob);
	
	// safe minimums
	if (GET_MAX_HEALTH(mob) < 1) {
		GET_MAX_HEALTH(mob) = 1;
	}
	if (GET_MAX_MOVE(mob) < 1) {
		GET_MAX_MOVE(mob) = 1;
	}
	
	// fix pools
	for (iter = 0; iter < NUM_POOLS; ++iter) {
		mob->points.current_pools[iter] = mob->points.max_pools[iter];
	}

	// GET_MAX_BLOOD is a function
	GET_BLOOD(mob) = GET_MAX_BLOOD(mob);	// set ok: mob being loaded

	mob->player.time.birth = time(0);
	mob->player.time.played = 0;
	mob->player.time.logon = time(0);

	MOB_PURSUIT(mob) = NULL;
	
	mob->script_id = 0;	// will detect when needed
	
	if (with_triggers) {
		mob->proto_script = copy_trig_protos(proto->proto_script);
		assign_triggers(mob, MOB_TRIGGER);
	}
	else {
		mob->proto_script = NULL;
	}
	
	// note this may lead to slight over-spawning after reboots -pc 5/20/16
	set_mob_spawn_time(mob, time(0));
	
	// special handling for mobs with LIGHT flags on the prototype
	if (AFF_FLAGGED(mob, AFF_LIGHT)) {
		++GET_LIGHTS(mob);
	}

	return (mob);
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT LOADING //////////////////////////////////////////////////////////

/**
* Clears basic data about the object. You should generally not call this
* directly -- prefer read_object() for prototyped items or create_obj() for
* non-prototyped ones. You must assign proto_data afterwards, either from a
* prototype or by setting it to create_obj_proto_data().
*
* @param obj_data *obj The object to clear.
*/
void clear_object(obj_data *obj) {
	memset((char *) obj, 0, sizeof(obj_data));

	obj->vnum = NOTHING;
	IN_ROOM(obj) = NULL;
	obj->worn_on = NO_WEAR;
	
	obj->last_owner_id = NOBODY;
	obj->last_empire_id = NOTHING;
	obj->stolen_from = NOTHING;
	
	// this does NOT create proto_data -- that must be done separately
}


/**
* Creates and initializes the proto_data, to be used on an object.
*
* @return struct obj_proto_data* The new data.
*/
struct obj_proto_data *create_obj_proto_data(void) {
	struct obj_proto_data *data;
	
	CREATE(data, struct obj_proto_data, 1);
	
	data->component = NOTHING;
	data->requires_quest = NOTHING;
	
	return data;
}


/**
* Create an object, and add it to the object list -- this is used to make
* anonymous objects, without a prototype.
*
* @return obj_data* The new item.
*/
obj_data *create_obj(void) {
	obj_data *obj;

	CREATE(obj, obj_data, 1);
	clear_object(obj);
	obj->proto_data = create_obj_proto_data();
	add_to_object_list(obj);
	
	// ensure it doesn't decay unless asked
	GET_OBJ_TIMER(obj) = UNLIMITED;
	
	obj->script_id = 0;	// will detect when needed
	
	return (obj);
}


/**
* Create a new object from a prototype. You should almost always call this with
* with_triggers = TRUE.
*
* @param obj_vnum nr The object vnum to load.
* @param bool with_triggers If TRUE, attaches all triggers.
* @return obj_data* The instantiated item.
*/
obj_data *read_object(obj_vnum nr, bool with_triggers) {
	obj_data *obj, *proto;
	
	if (!(proto = obj_proto(nr))) {
		log("Object vnum %d does not exist in database.", nr);
		// grab first one (bug)
		proto = object_table;
	}

	CREATE(obj, obj_data, 1);
	clear_object(obj);

	*obj = *proto;	// copies the proto_data pointer here too
	add_to_object_list(obj);
	
	// applies are ALWAYS a copy
	GET_OBJ_APPLIES(obj) = copy_obj_apply_list(GET_OBJ_APPLIES(proto));
	
	if (obj->obj_flags.timer == 0)
		obj->obj_flags.timer = UNLIMITED;
	
	obj->script_id = 0;	// will detect when needed
	
	if (with_triggers) {
		obj->proto_script = copy_trig_protos(proto->proto_script);
		assign_triggers(obj, OBJ_TRIGGER);
	}
	else {
		obj->proto_script = NULL;
	}

	return (obj);
}


 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS HELPERS ///////////////////////////////////////////////////

/* reset the time in the game from file */
void reset_time(void) {
	long beginning_of_time = data_get_long(DATA_WORLD_START);
	
	// a whole new world!
	if (!beginning_of_time) {
		beginning_of_time = time(0);
		data_set_long(DATA_WORLD_START, beginning_of_time);
	}

	main_time_info = *mud_time_passed(time(0), beginning_of_time);
	reset_weather();
	determine_seasons();

	log("   Current Gametime: %dH %dD %dM %dY.", main_time_info.hours, main_time_info.day, main_time_info.month, main_time_info.year);
}


/**
* Sets a player tech as 'inherent' -- all players automatically have it. This
* allows some variation from EmpireMUD to EmpireMUD, as some games require
* players to earn more techs.
*
* @param int ptech The PTECH_ type to set as 'inherent'.
*/
void set_inherent_ptech(int ptech) {
	struct int_hash *entry;
	
	HASH_FIND_INT(inherent_ptech_hash, &ptech, entry);
	
	if (!entry) {
		CREATE(entry, struct int_hash, 1);
		entry->num = ptech;
		HASH_ADD_INT(inherent_ptech_hash, num, entry);
	}
}


/**
* Scans the world for SECTF_START_LOCATION and creates the array of valid
* start locations.
*/
void setup_start_locations(void) {
	room_data *room, *next_room;
	room_vnum *new_start_locs = NULL;
	int count = -1;

	HASH_ITER(hh, world_table, room, next_room) {
		if (ROOM_SECT_FLAGGED(room, SECTF_START_LOCATION)) {
			++count;
			
			if (new_start_locs) {
				RECREATE(new_start_locs, room_vnum, count + 1);
			}
			else {
				CREATE(new_start_locs, room_vnum, count + 1);
			}
			
			new_start_locs[count] = GET_ROOM_VNUM(room);
		}
	}

	if (start_locs) {
		free(start_locs);
	}

	start_locs = new_start_locs;
	highest_start_loc_index = count;
}


 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS LOADERS ///////////////////////////////////////////////////

/**
* Loads the "messages" file, which contains damage messages for various attack
* types.
*/
void load_fight_messages(void) {
	FILE *fl;
	int i, type;
	struct message_type *messages;
	char chk[128];

	if (!(fl = fopen(MESS_FILE, "r"))) {
		log("SYSERR: Error reading combat message file %s: %s", MESS_FILE, strerror(errno));
		exit(1);
	}
	for (i = 0; i < MAX_MESSAGES; i++) {
		fight_messages[i].a_type = NOTHING;
		fight_messages[i].number_of_attacks = 0;
		fight_messages[i].msg = 0;
	}

	fgets(chk, 128, fl);
	while (!feof(fl) && (*chk == '\n' || *chk == '*'))
		fgets(chk, 128, fl);

	while (*chk == 'M') {
		fgets(chk, 128, fl);
		sscanf(chk, " %d\n", &type);
		for (i = 0; (i < MAX_MESSAGES) && (fight_messages[i].a_type != type) && (fight_messages[i].a_type != NOTHING); i++);
		if (i >= MAX_MESSAGES) {
			log("SYSERR: Too many combat messages. Increase MAX_MESSAGES and recompile.");
			exit(1);
		}
		CREATE(messages, struct message_type, 1);
		fight_messages[i].number_of_attacks++;
		fight_messages[i].a_type = type;
		
		LL_PREPEND(fight_messages[i].msg, messages);

		messages->die_msg.attacker_msg = fread_action(fl, i);
		messages->die_msg.victim_msg = fread_action(fl, i);
		messages->die_msg.room_msg = fread_action(fl, i);
		messages->miss_msg.attacker_msg = fread_action(fl, i);
		messages->miss_msg.victim_msg = fread_action(fl, i);
		messages->miss_msg.room_msg = fread_action(fl, i);
		messages->hit_msg.attacker_msg = fread_action(fl, i);
		messages->hit_msg.victim_msg = fread_action(fl, i);
		messages->hit_msg.room_msg = fread_action(fl, i);
		messages->god_msg.attacker_msg = fread_action(fl, i);
		messages->god_msg.victim_msg = fread_action(fl, i);
		messages->god_msg.room_msg = fread_action(fl, i);
		fgets(chk, 128, fl);
		while (!feof(fl) && (*chk == '\n' || *chk == '*'))
			fgets(chk, 128, fl);
	}

	fclose(fl);
}


/**
* Loads the game's various connect screens.
*/
void load_intro_screens(void) {
	char lbuf[MAX_STRING_LENGTH], lbuf2[MAX_STRING_LENGTH];
	FILE *index;
	int iter;
	
	// any old intros?
	if (num_intro_screens > 0 && intro_screens) {
		for (iter = 0; iter < num_intro_screens; ++iter) {
			free(intro_screens[iter]);
		}
		free(intro_screens);
		intro_screens = NULL;
	}
	
	num_intro_screens = 0;

	sprintf(lbuf, "%s%s", INTROS_PREFIX, INDEX_FILE);
	if (!(index = fopen(lbuf, "r"))) {
		log("SYSERR: opening index file '%s': %s", lbuf, strerror(errno));
		exit(1);
	}

	// count records in the index
	fscanf(index, "%s\n", lbuf2);
	while (*lbuf2 != '$') {
		++num_intro_screens;
		fscanf(index, "%s\n", lbuf2);
	}

	if (num_intro_screens <= 0) {
		log("SYSERR: boot error - 0 intro screens counted in %s.", lbuf);
		exit(1);
	}

	rewind(index);
	CREATE(intro_screens, char*, num_intro_screens);
	iter = 0;
	
	fscanf(index, "%s\n", lbuf2);
	while (*lbuf2 != '$') {
		sprintf(lbuf, "%s%s", INTROS_PREFIX, lbuf2);
		if (file_to_string_alloc(lbuf, intro_screens + iter) == 0) {
			prune_crlf(intro_screens[iter]);
		}
		++iter;
		
		fscanf(index, "%s\n", lbuf2);
	}
	
	log("Loaded %d intro screens.", num_intro_screens);
}


/**
* Load in the tips file. (Safe to reload at any point.)
*/
void load_tips_of_the_day(void) {
	char line[256];
	int count, pos;
	FILE *fl;
	
	const char *default_tip = "There are no useful tips in the system. See lib/misc/tips.";
	
	// free old one if exists
	if (tips_of_the_day != NULL) {
		for (pos = 0; pos < tips_of_the_day_size; ++pos) {
			if (tips_of_the_day[pos]) {
				free(tips_of_the_day[pos]);
			}
		}
		free(tips_of_the_day);
		tips_of_the_day = NULL;
		tips_of_the_day_size = 0;
	}
	
	// no file?
	if (!(fl = fopen(TIPS_OF_THE_DAY_FILE, "r"))) {
		CREATE(tips_of_the_day, char*, 1);
		tips_of_the_day[0] = str_dup(default_tip);
		tips_of_the_day_size = 1;
		return;
	}
	
	// count size
	count = 0;
	while (get_line(fl, line)) {
		if (*line == '$') {
			break;
		}
		else if (*line && *line != '*') {
			++count;
		}
	}
	
	rewind(fl);
	CREATE(tips_of_the_day, char*, count);
	
	pos = 0;
	while (get_line(fl, line)) {
		// done?
		if (*line == '$') {
			break;
		}
		
		if (*line && *line != '*') {
			tips_of_the_day[pos++] = str_dup(line);
		}
	}
	tips_of_the_day_size = pos;
	log("Loaded %d tips of the day.", pos);
}


/**
* Loads the trading post data from file at startup, into trading_list.
*/
void load_trading_post(void) {
	struct trading_post_data *tpd = NULL;
	char line[256], str_in[256];
	obj_data *obj;
	int int_in[5];
	long long_in;
	FILE *fl;
	
	if (!(fl = fopen(TRADING_POST_FILE, "r"))) {
		// not a problem (no trading post exists)
		return;
	}
	
	while (get_line(fl, line)) {
		switch (*line) {
			case 'T': {	// begin new trade item
				CREATE(tpd, struct trading_post_data, 1);
				DL_APPEND(trading_list, tpd);
				
				if (sscanf(line, "T %d %s %ld %d %d %d %d", &int_in[0], str_in, &long_in, &int_in[1], &int_in[2], &int_in[3], &int_in[4]) != 7) {
					log("SYSERR: Invalid T line in trading post file");
					exit(1);
				}
				
				tpd->player = int_in[0];
				tpd->state = asciiflag_conv(str_in);
				tpd->start = long_in;
				tpd->for_hours = int_in[1];
				tpd->buy_cost = int_in[2];
				tpd->post_cost = int_in[3];
				tpd->coin_type = int_in[4];
				break;
			}
			case '#': { // load object into last T
				obj = Obj_load_from_file(fl, atoi(line+1), &int_in[0], NULL, "the trading post");
				
				if (obj && tpd) {
					remove_from_object_list(obj);	// doesn't really go here right now
					tpd->obj = obj;
				}
				else if (obj) {
					// obj but no tpd
					extract_obj(obj);
				}
				// ignore if neither
				break;
			}
			case '$': {	// done!
				break;
			}
		}
	}
	
	fclose(fl);
}


 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS SAVERS ////////////////////////////////////////////////////

/**
* Save the trading post file (done after any changes).
*/
void save_trading_post(void) {
	struct trading_post_data *tpd;
	FILE *fl;
	
	if (block_all_saves_due_to_shutdown) {
		return;
	}
	
	if (!(fl = fopen(TRADING_POST_FILE, "w"))) {
		log("SYSERR: Unable to open file %s for writing.", TRADING_POST_FILE);
		return;
	}
	
	DL_FOREACH(trading_list, tpd) {
		fprintf(fl, "T %d %s %ld %d %d %d %d\n", tpd->player, bitv_to_alpha(tpd->state), tpd->start, tpd->for_hours, tpd->buy_cost, tpd->post_cost, tpd->coin_type);
		if (tpd->obj) {
			Crash_save_one_obj_to_file(fl, tpd->obj, 0);
		}
	}
	
	fprintf(fl, "$~\n");
	fclose(fl);
}
