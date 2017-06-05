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
*   Version Checking
*   Miscellaneous Helpers
*   Miscellaneous Loaders
*   Miscellaneous Savers
*/

// external vars
extern const sector_vnum climate_default_sector[NUM_CLIMATES];

// external functions
void Crash_save_one_obj_to_file(FILE *fl, obj_data *obj, int location);
void discrete_load(FILE *fl, int mode, char *filename);
void free_complex_data(struct complex_room_data *data);
extern crop_data *get_potential_crop_for_location(room_data *location);
void index_boot(int mode);
extern obj_data *Obj_load_from_file(FILE *fl, obj_vnum vnum, int *location, char_data *notify);
void save_whole_world();
void update_all_players(char_data *to_message, PLAYER_UPDATE_FUNC(*func));

// local functions
int file_to_string_alloc(const char *name, char **buf);
void get_one_line(FILE *fl, char *buf);
void index_boot_help();
void reset_time();


 //////////////////////////////////////////////////////////////////////////////
//// GLOBAL DATA /////////////////////////////////////////////////////////////

// abilities
ability_data *ability_table = NULL;	// main hash (hh)
ability_data *sorted_abilities = NULL;	// alpha hash (sorted_hh)

// adventures
adv_data *adventure_table = NULL;	// adventure hash table

// archetype
archetype_data *archetype_table = NULL;	// master hash (hh)
archetype_data *sorted_archetypes = NULL;	// sorted hash (sorted_hh)

// augments
augment_data *augment_table = NULL;	// master augment hash table
augment_data *sorted_augments = NULL;	// alphabetic version // sorted_hh

// buildings
bld_data *building_table = NULL;	// building hash table

// classes
class_data *class_table = NULL;	// main hash (hh)
class_data *sorted_classes = NULL;	// alpha hash (sorted_hh)

// crafts
craft_data *craft_table = NULL;	// master crafting table
craft_data *sorted_crafts = NULL;	// sorted craft table

// crops
crop_data *crop_table = NULL;	// crop hash table

// empires
empire_data *empire_table = NULL;	// hash table of empires
double empire_score_average[NUM_SCORES];
struct trading_post_data *trading_list = NULL;	// global LL of trading post stuff

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
struct time_info_data time_info;	// the infomation about the time
struct weather_data weather_info;	// the infomation about the weather
int wizlock_level = 0;	// level of game restriction
char *wizlock_message = NULL;	// Message sent to people trying to connect

// global stuff
struct global_data *globals_table = NULL;	// hash table of global_data

// helps
struct help_index_element *help_table = 0;	// the help table
int top_of_helpt = 0;	// top of help index table

// map
room_vnum *start_locs = NULL;	// array of start locations
int num_of_start_locs = -1;	// maximum start locations
int top_island_num = -1;	// for number of islands

// mobs
char_data *mobile_table = NULL;	// hash table of mobs
struct player_special_data dummy_mob;	// dummy spec area for mobs
char_data *character_list = NULL;	// global linked list of chars (including players)
char_data *combat_list = NULL;	// head of l-list of fighting chars
char_data *next_combat_list = NULL;	// used for iteration of combat_list when more than 1 person can be removed from combat in 1 loop iteration
struct generic_name_data *generic_names = NULL;	// LL of generic name sets

// morphs
morph_data *morph_table = NULL;	// master morph hash table
morph_data *sorted_morphs = NULL;	// alphabetic version // sorted_hh

// objects
obj_data *object_list = NULL;	// global linked list of objs
obj_data *object_table = NULL;	// hash table of objs

// players
account_data *account_table = NULL;	// hash table of accounts
player_index_data *player_table_by_idnum = NULL;	// hash table by idnum
player_index_data *player_table_by_name = NULL;	// hash table by name
int top_idnum = 0;	// highest idnum in use
int top_account_id = 0;  // highest account number in use, determined during startup
struct group_data *group_list = NULL;	// global LL of groups

// quests
struct quest_data *quest_table = NULL;

// room templates
room_template *room_template_table = NULL;	// hash table of room templates

// sectors
sector_data *sector_table = NULL;	// sector hash table
struct sector_index_type *sector_index = NULL;	// index lists
struct map_data *last_evo_tile = NULL;	// for resuming map evolutions
sector_data *last_evo_sect = NULL;	// for resuming map evolutions
int evos_per_hour = 1;	// how many map tiles evolve per hour (for load-balancing)

// skills
skill_data *skill_table = NULL;	// main skills hash (hh)
skill_data *sorted_skills = NULL;	// alpha hash (sorted_hh)

// socials
social_data *social_table = NULL;	// master social hash table (hh)
social_data *sorted_socials = NULL;	// alphabetic version (sorted_hh)

// strings
char *credits = NULL;	// game credits
char *motd = NULL;	// message of the day - mortals
char *imotd = NULL;	// message of the day - immorts
char **intros = NULL;	// array of intro screens
int num_intros = 0;	// total number of intro screens
char *CREDIT_MESSG = NULL;	// short credits
char *help = NULL;	// help screen
char *info = NULL;	// info page
char *wizlist = NULL;	// list of higher gods
char *godlist = NULL;	// list of peon gods
char *handbook = NULL;	// handbook for new immortals
char *policies = NULL;	// policies page

// tips of the day system
char **tips_of_the_day = NULL;	// array of tips
int tips_of_the_day_size = 0;	// size of tip array

// triggers
trig_data *trigger_table = NULL;	// trigger prototype hash
trig_data *trigger_list = NULL;	// LL of all attached triggers
trig_data *random_triggers = NULL;	// DLL of live random triggers (next_in_random_triggers, prev_in_random_triggers)
int max_mob_id = MOB_ID_BASE;	// for unique mob ids
int max_obj_id = OBJ_ID_BASE;	// for unique obj ids
int max_vehicle_id = VEHICLE_ID_BASE;	// for unique vehicle ids
int dg_owner_purged = 0;	// For control of scripts
char_data *dg_owner_mob = NULL;	// for detecting self-purge
obj_data *dg_owner_obj = NULL;
vehicle_data *dg_owner_veh = NULL;
room_data *dg_owner_room = NULL;

// vehicles
vehicle_data *vehicle_table = NULL;	// master vehicle hash table
vehicle_data *vehicle_list = NULL;	// global linked list of vehicles (veh->next)

// world / rooms
room_data *world_table = NULL;	// hash table of the whole world
room_data *interior_room_list = NULL;	// linked list of interior rooms: room->next_interior
bool world_is_sorted = FALSE;	// to prevent unnecessary re-sorts
bool need_world_index = TRUE;	// used to trigger world index saving (always save at least once)
struct island_info *island_table = NULL; // hash table for all the islands
struct map_data world_map[MAP_WIDTH][MAP_HEIGHT];	// master world map
struct map_data *land_map = NULL;	// linked list of non-ocean
bool world_map_needs_save = TRUE;	// always do at least 1 save


// DB_BOOT_x
struct db_boot_info_type db_boot_info[NUM_DB_BOOT_TYPES] = {
	{ WLD_PREFIX, WLD_SUFFIX },	// DB_BOOT_WLD
	{ MOB_PREFIX, MOB_SUFFIX },	// DB_BOOT_MOB
	{ OBJ_PREFIX, OBJ_SUFFIX },	// DB_BOOT_OBJ
	{ NAMES_PREFIX, NULL },	// DB_BOOT_NAMES
	{ LIB_EMPIRE, EMPIRE_SUFFIX },	// DB_BOOT_EMP
	{ BOOK_PREFIX, BOOK_SUFFIX },	// DB_BOOT_BOOKS
	{ CRAFT_PREFIX, CRAFT_SUFFIX },	// DB_BOOT_CRAFT
	{ BLD_PREFIX, BLD_SUFFIX },	// DB_BOOT_BLD
	{ TRG_PREFIX, TRG_SUFFIX },	// DB_BOOT_TRG
	{ CROP_PREFIX, CROP_SUFFIX },	// DB_BOOT_CROP
	{ SECTOR_PREFIX, SECTOR_SUFFIX },	// DB_BOOT_SECTOR
	{ ADV_PREFIX, ADV_SUFFIX },	// DB_BOOT_ADV
	{ RMT_PREFIX, RMT_SUFFIX },	// DB_BOOT_RMT
	{ GLB_PREFIX, GLB_SUFFIX },	// DB_BOOT_GLB
	{ ACCT_PREFIX, ACCT_SUFFIX },	// DB_BOOT_ACCT
	{ AUG_PREFIX, AUG_SUFFIX },	// DB_BOOT_AUG
	{ ARCH_PREFIX, ARCH_SUFFIX },	// DB_BOOT_ARCH
	{ ABIL_PREFIX, ABIL_SUFFIX },	// DB_BOOT_ABIL
	{ CLASS_PREFIX, CLASS_SUFFIX },	// DB_BOOT_CLASS
	{ SKILL_PREFIX, SKILL_SUFFIX },	// DB_BOOT_SKILL
	{ VEH_PREFIX, VEH_SUFFIX },	// DB_BOOT_SKILL
	{ MORPH_PREFIX, MORPH_SUFFIX },	// DB_BOOT_MORPH
	{ QST_PREFIX, QST_SUFFIX },	// DB_BOOT_QST
	{ SOC_PREFIX, SOC_SUFFIX },	// DB_BOOT_SOC
	{ FCT_PREFIX, FCT_SUFFIX },	// DB_BOOT_FCT
};


 //////////////////////////////////////////////////////////////////////////////
//// STARTUP /////////////////////////////////////////////////////////////////

/**
* Body of the booting system: this loads a lot of game data, although the
* world (objs, mobs, etc) has its own function.
*/
void boot_db(void) {
	void Read_Invalid_List();
	void boot_world();
	void build_all_quest_lookups();
	void build_player_index();
	void check_ruined_cities();
	void check_version();
	void delete_old_players();
	void delete_orphaned_rooms();
	void detect_evos_per_hour();
	void init_config_system();
	void link_and_check_vehicles();
	void load_banned();
	void load_data_table();
	void load_intro_screens();
	void load_fight_messages();
	void load_tips_of_the_day();
	void load_trading_post();
	int run_convert_vehicle_list();
	void run_reboot_triggers();
	void sort_commands();
	void startup_room_reset();
	void verify_sectors();

	log("Boot db -- BEGIN.");
	
	log("Loading game config system.");
	init_config_system();
	
	log("Loading game data system.");
	load_data_table();
	
	log("Resetting the game time:");
	reset_time();

	log("Reading credits, help, bground, info & motds.");
	file_to_string_alloc(CREDITS_FILE, &credits);
	file_to_string_alloc(MOTD_FILE, &motd);
	file_to_string_alloc(IMOTD_FILE, &imotd);
	file_to_string_alloc(HELP_PAGE_FILE, &help);
	file_to_string_alloc(INFO_FILE, &info);
	file_to_string_alloc(WIZLIST_FILE, &wizlist);
	file_to_string_alloc(GODLIST_FILE, &godlist);
	file_to_string_alloc(POLICIES_FILE, &policies);
	file_to_string_alloc(HANDBOOK_FILE, &handbook);
	file_to_string_alloc(SCREDITS_FILE, &CREDIT_MESSG);
	load_intro_screens();

	// Load the world!
	boot_world();

	log("Loading help entries.");
	index_boot_help();
	
	log("Loading player accounts.");
	index_boot(DB_BOOT_ACCT);

	log("Generating player index.");
	build_player_index();
	
	log(" Checking for ruined cities...");
	check_ruined_cities();

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

	// NOTE: check_version() updates many things that change from version to
	// version. See the function itself for a list of one-time updates it runs
	// on the game. This should run as late in boot_db() as possible.
	log("Checking game version...");
	check_version();
	
	// Some things runs AFTER check_version() because they rely on all version
	// updates having been run on this EmpireMUD:
	
	log("Verifying world sectors.");
	verify_sectors();
	
	// convert vehicles -- this normally does nothing, but it may free a temporary list
	run_convert_vehicle_list();
	
	log("Checking for orphaned rooms...");
	delete_orphaned_rooms();
	
	log("Linking and checking vehicles.");
	link_and_check_vehicles();
	
	log("Building quest lookup hints.");
	build_all_quest_lookups();
	
	// figure out how often to evolve what (do this late)
	detect_evos_per_hour();
	
	// final things...
	log("Running reboot triggers.");
	run_reboot_triggers();
	
	log(" Calculating territory and members.");
	reread_empire_tech(NULL);
	
	// END
	log("Boot db -- DONE.");
	boot_time = time(0);
}


/**
* Load all of the game's world data during startup. NOTE: Order matters here,
* as some items rely on others. For example, sectors must be loaded before
* rooms (because rooms have sectors).
*/
void boot_world(void) {
	void build_land_map();
	void build_world_map();
	void check_abilities();
	void check_and_link_faction_relations();
	void check_archetypes();
	void check_classes();
	void check_skills();
	void check_for_bad_buildings();
	void check_for_bad_sectors();
	void check_newbie_islands();
	void check_triggers();
	void clean_empire_logs();
	void index_boot_world();
	void init_reputation();
	void load_daily_quest_file();
	void load_empire_storage();
	void load_instances();
	void load_islands();
	void load_world_map_from_file();
	void number_and_count_islands(bool reset);
	void read_ability_requirements();
	void renum_world();
	void setup_start_locations();
	extern int sort_abilities_by_data(ability_data *a, ability_data *b);
	extern int sort_archetypes_by_data(archetype_data *a, archetype_data *b);
	extern int sort_augments_by_data(augment_data *a, augment_data *b);
	extern int sort_classes_by_data(class_data *a, class_data *b);
	extern int sort_crafts_by_data(craft_data *a, craft_data *b);
	extern int sort_skills_by_data(skill_data *a, skill_data *b);
	extern int sort_socials_by_data(social_data *a, social_data *b);

	// DB_BOOT_x search: boot new types in this function
	
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
	load_world_map_from_file();	// get base data
	index_boot(DB_BOOT_WLD);	// override with live rooms
	build_world_map();	// ensure full world map
	build_land_map();	// determine which parts are land
	
	// requires rooms
	log("Loading empires.");
	index_boot(DB_BOOT_EMP);
	clean_empire_logs();
	
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
	
	log("Loading socials.");
	index_boot(DB_BOOT_SOC);
	
	log("Loading instances.");
	load_instances();
	
	log("Loading empire storage.");
	load_empire_storage();
	
	log("Loading daily quest cycles.");
	load_daily_quest_file();
	
	// check for bad data
	log("Verifying data.");
	check_abilities();
	check_and_link_faction_relations();
	check_archetypes();
	check_classes();
	check_skills();
	check_for_bad_buildings();
	check_for_bad_sectors();
	read_ability_requirements();
	check_triggers();
	
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
	extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom);
	void unlink_instance_entrance(room_data *room);

	struct obj_storage_type *store, *next_store, *temp;
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
		else if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) && !find_instance_by_room(room, TRUE)) {
			// room is marked as an instance entrance, but no instance is associated with it
			log(" unlinking instance entrance room %d for no association with an instance", GET_ROOM_VNUM(room));
			unlink_instance_entrance(room);
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
		if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD && !building_proto(GET_CRAFT_BUILD_TYPE(craft))) {
			GET_CRAFT_BUILD_TYPE(craft) = NOTHING;
			SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
			log(" disabling craft %d for bad building type", GET_CRAFT_VNUM(craft));
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// check craft recipes of people using olc: disable and warn
	for (dd = descriptor_list; dd; dd = dd->next) {
		if (GET_OLC_TYPE(dd) == OLC_CRAFT && GET_OLC_CRAFT(dd)) {
			if (GET_OLC_CRAFT(dd)->type == CRAFT_TYPE_BUILD && !building_proto(GET_OLC_CRAFT(dd)->build_type)) {
				GET_OLC_CRAFT(dd)->build_type = NOTHING;
				SET_BIT(GET_OLC_CRAFT(dd)->flags, CRAFT_IN_DEVELOPMENT);
				msg_to_desc(dd, "&RYou are editing a craft recipe whose building has been deleted. Building type removed.&0\r\n");
			}
		}
	}
	
	// check obj protos for storage locations that used a deleted building: delete
	HASH_ITER(hh, object_table, obj, next_obj) {
		for (store = obj->storage; store; store = next_store) {
			next_store = store->next;
			
			if (store->building_type != NOTHING && !building_proto(store->building_type)) {
				REMOVE_FROM_LIST(store, obj->storage, next);
				free(store);
				log(" removing storage for obj %d for bad building type", obj->vnum);
				save_library_file_for_vnum(DB_BOOT_OBJ, obj->vnum);
			}
		}
	}
	
	// check open obj editors for storage in deleted buildings: delete
	for (dd = descriptor_list; dd; dd = dd->next) {
		if (GET_OLC_TYPE(dd) == OLC_OBJECT && GET_OLC_OBJECT(dd)) {
			for (store = GET_OLC_OBJECT(dd)->storage; store; store = next_store) {
				next_store = store->next;
			
				if (store->building_type != NOTHING && !building_proto(store->building_type)) {
					REMOVE_FROM_LIST(store, GET_OLC_OBJECT(dd)->storage, next);
					free(store);
					msg_to_desc(dd, "&RYou are editing an object whose storage building has been deleted. Building type removed.&0\r\n");
				}
			}
		}
	}
	
	// check buildings for upgrades: unset
	HASH_ITER(hh, building_table, bld, next_bld) {
		if (GET_BLD_UPGRADES_TO(bld) != NOTHING && !building_proto(GET_BLD_UPGRADES_TO(bld))) {
			GET_BLD_UPGRADES_TO(bld) = NOTHING;
			log(" removing upgrade for building %d for bad building type", GET_BLD_VNUM(bld));
			save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
		}
	}
}


/**
* Look for places that store a sector and might have bad ones. It does not
* save the changes to file on its own; it will re-fix them on every boot.
*/
void check_for_bad_sectors(void) {
	struct evolution_data *evo, *next_evo, *temp;
	sector_data *sect, *next_sect;

	HASH_ITER(hh, sector_table, sect, next_sect) {
		// look for evolutions with bad targets
		for (evo = GET_SECT_EVOS(sect); evo; evo = next_evo) {
			next_evo = evo->next;
			
			// oops: remove bad evo
			if (!sector_proto(evo->becomes)) {
				REMOVE_FROM_LIST(evo, GET_SECT_EVOS(sect), next);
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
	for (room = interior_room_list; room; room = next_room) {
		next_room = room->next_interior;
		
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
	room_data *room, *next_room;
	
	// ensure we have a backup sect
	use_sect = sector_proto(climate_default_sector[CLIMATE_TEMPERATE]);
	if (!use_sect) {
		// just pull the first one
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
	HASH_ITER(hh, world_table, room, next_room) {
		if (!SECT(room)) {
			// can't use change_terrain() here
			perform_change_sect(room, NULL, use_sect);
		}
		if (!BASE_SECT(room)) {
			change_base_sector(room, use_sect);
		}
		
		// also check for missing crop data
		if (SECT_FLAGGED(SECT(room), SECTF_HAS_CROP_DATA | SECTF_CROP) && !ROOM_CROP(room)) {
			crop_data *new_crop = get_potential_crop_for_location(room);
			if (new_crop) {
				set_crop_type(room, new_crop);
			}
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
			if (trd->home_room != NOWHERE && COMPLEX_DATA(room) && (home = real_room(trd->home_room))) {
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


/* resolve all vnums in the world */
void renum_world(void) {
	room_data *room, *next_room, *home;
	struct room_direction_data *ex, *next_ex, *temp;
	
	process_temporary_room_data();

	// exits
	HASH_ITER(hh, world_table, room, next_room) {
		// exits
		if (COMPLEX_DATA(room)) {
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
					REMOVE_FROM_LIST(ex, COMPLEX_DATA(room)->exits, next);
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
			}
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
	extern int file_to_string(const char *name, char *buf);
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
	register char *point;
	int done = 0, length = 0, templength;

	*buf = '\0';

	do {
		if (!fgets(tmp, 512, fl)) {
			log("SYSERR: fread_string: format error at or near %s", error);
			exit(1);
		}
				
		// upgrade to allow ~ when not at the end
		point = strchr(tmp, '\0');
		
		// backtrack past any \r\n or trailing space or tab
		for (--point; *point == '\r' || *point == '\n' || *point == ' ' || *point == '\t'; --point);
		
		// look for a trailing ~
		if (*point == '~') {
			*point = '\0';
			done = 1;
		}
		else {
			strcpy(point+1, "\r\n");
		}

		templength = strlen(tmp);

		if (length + templength >= MAX_STRING_LENGTH) {
			log("SYSERR: fread_string: string too large (db.c)");
			log(error);
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

		if (*line == '#' && *(line + 1))
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
	extern int help_sort(const void *a, const void *b);
	
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
	island->next = island_stack;
	island_stack = island;
}


/**
* Checks the newbie islands and applies their rules (abandons land).
*/
void check_newbie_islands(void) {
	struct island_info *isle = NULL, *ii, *next_ii;
	room_data *room, *next_room;
	int num_newbie_isles, last_isle = -1;
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
		if (GET_ROOM_VNUM(room) >= MAP_SIZE || GET_ISLAND_ID(room) == NO_ISLAND) {
			continue;
		}
		
		// ensure ownership and that the empire is "not new"
		if (!(emp = ROOM_OWNER(room)) || (EMPIRE_CREATE_TIME(emp) + (config_get_int("newbie_island_day_limit") * SECS_PER_REAL_DAY)) > time(0)) {
			continue;
		}
		
		// usually see many of the same island in a row, so this reduces lookups
		if (last_isle == -1 || GET_ISLAND_ID(room) != last_isle) {
			isle = get_island(GET_ISLAND_ID(room), TRUE);
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
*/
void number_island(struct map_data *map, int island) {
	int x, y, new_x, new_y;
	struct map_data *tile;
	room_data *room;
	
	map->island = island;
	
	// if there's a real room
	if ((room = real_real_room(map->vnum))) {
		SET_ISLAND_ID(room, island);
	}
	
	// check neighboring tiles
	for (x = -1; x <= 1; ++x) {
		for (y = -1; y <= 1; ++y) {
			// same tile
			if (x == 0 && y == 0) {
				continue;
			}
			
			if (get_coord_shift(MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum), x, y, &new_x, &new_y)) {
				tile = &(world_map[new_x][new_y]);
				
				if (!SECT_FLAGGED(tile->sector_type, SECTF_NON_ISLAND) && tile->island <= 0) {
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
	struct island_info *isle;
	struct map_data *map;
	room_data *room;
	int iter, use_id;
	
	// find top island id (and reset if requested)
	top_island_num = -1;
	for (map = land_map; map; map = map->next) {
		if (reset || SECT_FLAGGED(map->sector_type, SECTF_NON_ISLAND)) {
			map->island = NO_ISLAND;
		}
		else {
			top_island_num = MAX(top_island_num, map->island);
		}
	}
	
	// now update the whole world -- if it's not a reset, this is done in two stages:
	
	// 1. expand EXISTING islands
	if (!reset) {
		for (map = land_map; map; map = map->next) {
			if (map->island == NO_ISLAND) {
				continue;
			}
			
			use_id = map->island;
			push_island(map);
			
			while ((item = pop_island())) {
				number_island(item->loc, use_id);
				free(item);
			}
		}
	}
	
	// 2. look for places that have no island id but need one -- and also measure islands while we're here
	for (map = land_map; map; map = map->next) {
		if (map->island == NO_ISLAND && !SECT_FLAGGED(map->sector_type, SECTF_NON_ISLAND)) {
			use_id = ++top_island_num;
			push_island(map);
			
			while ((item = pop_island())) {
				number_island(item->loc, use_id);
				free(item);
			}
		}
		else {
			use_id = map->island;
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
	MOB_INSTANCE_ID(ch) = NOTHING;
	MOB_DYNAMIC_SEX(ch) = NOTHING;
	MOB_DYNAMIC_NAME(ch) = NOTHING;
	MOB_PURSUIT_LEASH_LOC(ch) = NOWHERE;
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
	
	GET_LAST_ROOM(ch) = NOWHERE;
	GET_LOADROOM(ch) = NOWHERE;
	GET_LOAD_ROOM_CHECK(ch) = NOWHERE;
	GET_MARK_LOCATION(ch) = NOWHERE;
	GET_MOUNT_VNUM(ch) = NOTHING;
	GET_PLEDGE(ch) = NOTHING;
	GET_TOMB_ROOM(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_RETURN_MAP(ch) = NOWHERE;
	GET_LAST_TELL(ch) = NOBODY;
	GET_TEMPORARY_ACCOUNT_ID(ch) = NOTHING;
	GET_IMMORTAL_LEVEL(ch) = -1;	// Not an immortal
	
	for (iter = 0; iter < MAX_REWARDS_PER_DAY; ++iter) {
		GET_REWARDED_TODAY(ch, iter) = -1;
	}
	
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
	mob->next = character_list;
	character_list = mob;
	
	// safe minimums
	if (GET_MAX_HEALTH(mob) < 1) {
		GET_MAX_HEALTH(mob) = 1;
	}
	if (GET_MAX_MOVE(mob) < 1) {
		GET_MAX_MOVE(mob) = 1;
	}
	
	for (iter = 0; iter < NUM_POOLS; ++iter) {
		mob->points.current_pools[iter] = mob->points.max_pools[iter];
	}

	mob->player.time.birth = time(0);
	mob->player.time.played = 0;
	mob->player.time.logon = time(0);

	MOB_PURSUIT(mob) = NULL;

	// GET_MAX_BLOOD is a function
	GET_BLOOD(mob) = GET_MAX_BLOOD(mob);
	
	mob->script_id = 0;	// will detect when needed
	
	if (with_triggers) {
		mob->proto_script = copy_trig_protos(proto->proto_script);
		assign_triggers(mob, MOB_TRIGGER);
	}
	else {
		mob->proto_script = NULL;
	}
	
	// note this may lead to slight over-spawning after reboots -pc 5/20/16
	MOB_SPAWN_TIME(mob) = time(0);

	return (mob);
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT LOADING //////////////////////////////////////////////////////////


void clear_object(obj_data *obj) {
	memset((char *) obj, 0, sizeof(obj_data));

	obj->vnum = NOTHING;
	IN_ROOM(obj) = NULL;
	obj->worn_on = NO_WEAR;
	
	GET_OBJ_REQUIRES_QUEST(obj) = NOTHING;
	
	obj->last_owner_id = NOBODY;
	obj->last_empire_id = NOTHING;
	obj->stolen_from = NOTHING;
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

	*obj = *proto;
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
//// VERSION CHECKING ////////////////////////////////////////////////////////

// add versions in ascending order: this is used by check_version()
const char *versions_list[] = {
	// this system was added in b2.5
	"b2.5",
	"b2.7",
	"b2.8",
	"b2.9",
	"b2.11",
	"b3.0",
	"b3.1",
	"b3.2",
	"b3.6",
	"b3.8",
	"b3.11",
	"b3.12",
	"b3.15",
	"b3.17",
	"b4.1",
	"b4.2",
	"b4.4",
	"b4.15",
	"b4.19",
	"b4.32",
	"b4.36",
	"b4.38",
	"b4.39",
	"\n"	// be sure the list terminates with \n
};


/**
* Find the version of the last successful boot.
*
* @return int a position in versions_list[] or NOTHING
*/
int get_last_boot_version(void) {
	char str[256];
	FILE *fl;
	
	if (!(fl = fopen(VERSION_FILE, "r"))) {
		return -1;
	}
	
	sprintf(buf, "version file");
	get_line(fl, str);
	fclose(fl);
	
	return search_block(str, versions_list, TRUE);
}

/**
* Writes the version of the last good boot.
*
* @param int version Which version id to write (pos in versions_list).
*/
// version is pos in versions_list[]
void write_last_boot_version(int version) {
	FILE *fl;
	
	if (version == NOTHING) {
		return;
	}
	
	if (!(fl = fopen(VERSION_FILE, "w"))) {
		log("Unable to write version file. This would cause version updates to be applied repeatedly.");
		exit(1);
	}
	
	fprintf(fl, "%s\n", versions_list[version]);
	fclose(fl);
}


// 2.8 removes the color preference
PLAYER_UPDATE_FUNC(b2_8_update_players) {
	REMOVE_BIT(PRF_FLAGS(ch), BIT(9));	// was PRF_COLOR
}


// 2.11 loads inventories and attaches triggers
PLAYER_UPDATE_FUNC(b2_11_update_players) {
	void check_delayed_load(char_data *ch);

	obj_data *obj, *proto;
	int iter;
	
	// no work if in-game (covered by other parts of the update)
	if (!is_file) {
		return;
	}
	
	check_delayed_load(ch);
	
	// inventory
	for (obj = ch->carrying; obj; obj = obj->next_content) {
		if ((proto = obj_proto(GET_OBJ_VNUM(obj)))) {
			obj->proto_script = copy_trig_protos(proto->proto_script);
			assign_triggers(obj, OBJ_TRIGGER);
		}
	}
	
	// eq
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter) && (proto = obj_proto(GET_OBJ_VNUM(GET_EQ(ch, iter))))) {
			GET_EQ(ch, iter)->proto_script = copy_trig_protos(proto->proto_script);
			assign_triggers(GET_EQ(ch, iter), OBJ_TRIGGER);
		}
	}
}


// updater for existing mines
void b3_1_mine_update(void) {
	room_data *room, *next_room;
	int type;
	
	HASH_ITER(hh, world_table, room, next_room) {
		if ((type = get_room_extra_data(room, 0)) <= 0) {	// 0 was ROOM_EXTRA_MINE_TYPE
			continue;
		}

		switch (type) {
			case 10: {	// iron
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 199);
				break;
			}
			case 11: {	// silver
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 161);
				break;
			}
			case 12: {	// gold
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 162);
				break;
			}
			case 13: {	// nocturnium
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 163);
				break;
			}
			case 14: {	// imperium
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 164);
				break;
			}
			case 15: {	// copper
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 160);
				break;
			}
		}

		remove_room_extra_data(room, 0);	// ROOM_EXTRA_MINE_TYPE prior to b3.1
	}
}


PLAYER_UPDATE_FUNC(b3_2_player_gear_disenchant) {
	void check_delayed_load(char_data *ch);
	obj_data *obj, *next_obj, *new;
	int iter;
	
	check_delayed_load(ch);
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if ((obj = GET_EQ(ch, iter)) && OBJ_FLAGGED(obj, OBJ_ENCHANTED) && obj_proto(GET_OBJ_VNUM(obj))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
	for (obj = ch->carrying; obj; obj = next_obj) {
		next_obj = obj->next_content;
		if (OBJ_FLAGGED(obj, OBJ_ENCHANTED) && obj_proto(GET_OBJ_VNUM(obj))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
}


// removes the PLAYER-MADE flag from rooms and sets their "natural sect" instead
void b3_2_map_and_gear(void) {
	extern const sector_vnum climate_default_sector[NUM_CLIMATES];
	void save_trading_post();

	obj_data *obj, *next_obj, *new, *proto;
	struct empire_unique_storage *eus;
	struct trading_post_data *tpd;
	room_data *room, *next_room;
	empire_data *emp, *next_emp;
	crop_vnum type;
	
	int ROOM_EXTRA_CROP_TYPE = 2;	// removed extra type
	bitvector_t ROOM_AFF_PLAYER_MADE = BIT(11);	// removed flag
	sector_vnum OASIS = 21, SANDY_TRENCH = 22;
	
	log("Applying b3.2 update...");
	
	log(" - updating the map...");
	HASH_ITER(hh, world_table, room, next_room) {
		// player-made
		if (IS_SET(ROOM_AFF_FLAGS(room) | ROOM_BASE_FLAGS(room), ROOM_AFF_PLAYER_MADE)) {
			// remove the bits
			REMOVE_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_PLAYER_MADE);
			REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_PLAYER_MADE);
		
			// update the natural sector
			if (GET_ROOM_VNUM(room) < MAP_SIZE) {
				world_map[FLAT_X_COORD(room)][FLAT_Y_COORD(room)].natural_sector = sector_proto((GET_SECT_VNUM(SECT(room)) == OASIS || GET_SECT_VNUM(SECT(room)) == SANDY_TRENCH) ? climate_default_sector[CLIMATE_ARID] : climate_default_sector[CLIMATE_TEMPERATE]);
				world_map_needs_save = TRUE;
			}
		}
		
		// crops
		if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && !ROOM_CROP(room)) {
			type = get_room_extra_data(room, ROOM_EXTRA_CROP_TYPE);
			set_crop_type(room, type > 0 ? crop_proto(type) : get_potential_crop_for_location(room));
			remove_room_extra_data(room, ROOM_EXTRA_CROP_TYPE);
		}
	}
	
	log(" - disenchanting the object list...");
	for (obj = object_list; obj; obj = next_obj) {
		next_obj = obj->next;
		if (OBJ_FLAGGED(obj, OBJ_ENCHANTED) && (proto = obj_proto(GET_OBJ_VNUM(obj))) && !OBJ_FLAGGED(proto, OBJ_ENCHANTED)) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
	
	log(" - disenchanting warehouse objects...");
	HASH_ITER(hh, empire_table, emp, next_emp) {
		for (eus = EMPIRE_UNIQUE_STORAGE(emp); eus; eus = eus->next) {
			if ((obj = eus->obj) && OBJ_FLAGGED(obj, OBJ_ENCHANTED) && (proto = obj_proto(GET_OBJ_VNUM(obj))) && !OBJ_FLAGGED(proto, OBJ_ENCHANTED)) {
				new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
				eus->obj = new;
				extract_obj(obj);
			}
		}
	}
	
	log(" - disenchanting trading post objects...");
	for (tpd = trading_list; tpd; tpd = tpd->next) {
		if ((obj = tpd->obj) && OBJ_FLAGGED(obj, OBJ_ENCHANTED) && (proto = obj_proto(GET_OBJ_VNUM(obj))) && !OBJ_FLAGGED(proto, OBJ_ENCHANTED)) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
			tpd->obj = new;
			extract_obj(obj);
		}
	}
	
	log(" - disenchanting player inventories...");
	update_all_players(NULL, b3_2_player_gear_disenchant);
	
	// ensure everything gets saved this way since we won't do this again
	save_all_empires();
	save_trading_post();
	save_whole_world();
}


// fixes some guild-patterend cloth that was accidentally auto-weaved in a previous patch
// NOTE: the cloth is not storable, so any empire with it in normal storage must have had the bug
void b3_6_einv_fix(void) {
	struct empire_storage_data *store, *next_store;
	empire_data *emp, *next_emp;
	obj_data *proto;
	int total, amt;
	
	obj_vnum vnum = 2344;	// guild-patterned cloth
	obj_vnum cloth = 1359;
	obj_vnum silver = 161;
	
	proto = obj_proto(vnum);
	if (!proto || proto->storage || !obj_proto(cloth) || !obj_proto(silver)) {
		return;	// no work to do on this EmpireMUD
	}
	
	log("Fixing incorrectly auto-weaved guild-patterned cloth (2344)");
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		total = 0;
		LL_FOREACH_SAFE(EMPIRE_STORAGE(emp), store, next_store) {
			if (store->vnum == vnum) {
				amt = store->amount;
				total += amt;
				add_to_empire_storage(emp, store->island, cloth, 4 * amt);
				add_to_empire_storage(emp, store->island, silver, 2 * amt);
				LL_DELETE(EMPIRE_STORAGE(emp), store);
				free(store);
			}
		}
		
		if (total > 0) {
			log(" - [%d] %s: %d un-woven", EMPIRE_VNUM(emp), EMPIRE_NAME(emp), total);
		}
	}
	
	save_all_empires();
}


// removes shipping ids that got stuck and are not in the holding pen
void b3_11_ship_fix(void) {
	vehicle_data *veh;
	
	LL_FOREACH(vehicle_list, veh) {
		if (IN_ROOM(veh) && VEH_SHIPPING_ID(veh) != -1 && (!GET_BUILDING(IN_ROOM(veh)) || GET_BLD_VNUM(GET_BUILDING(IN_ROOM(veh))) != RTYPE_SHIP_HOLDING_PEN)) {
			VEH_SHIPPING_ID(veh) = -1;
		}
	}
}


// removes AFF_SENSE_HIDE
PLAYER_UPDATE_FUNC(b3_12_update_players) {
	void check_delayed_load(char_data *ch);
	
	// only care if they have a permanent sense-hide
	if (!AFF_FLAGGED(ch, AFF_SENSE_HIDE)) {
		return;
	}
	
	check_delayed_load(ch);
	REMOVE_BIT(AFF_FLAGS(ch), AFF_SENSE_HIDE);
	affect_total(ch);	// in case they are getting it from a real affect
}


// respawns wild crops and converts 5% of jungle to crops
void b3_15_crop_update(void) {
	extern crop_data *get_potential_crop_for_location(room_data *location);
	
	struct map_data *map;
	room_data *room;
	
	const int SECT_JUNGLE = 28;	// convert jungles at random
	const int JUNGLE_PERCENT = 5;	// change to change jungle to crop
	const int SECT_JUNGLE_FIELD = 16;	// sect to use for crop
	
	LL_FOREACH(land_map, map) {
		room = NULL;
		
		if ((room = real_real_room(map->vnum))) {
			if (ROOM_OWNER(room)) {
				continue;	// skip owned tiles
			}
		}
		
		if (map->crop_type) {
			// update crop
			if (room || (room = real_room(map->vnum))) {
				set_crop_type(room, get_potential_crop_for_location(room));
			}
		}
		else if (map->sector_type->vnum == SECT_JUNGLE && number(1, 100) <= JUNGLE_PERCENT) {
			// transform jungle
			if (room || (room = real_room(map->vnum))) {
				change_terrain(room, SECT_JUNGLE_FIELD);	// picks own crop
			}
		}
	}
	
	save_whole_world();
}


// adds built-with resources to roads
void b3_17_road_update(void) {
	extern struct complex_room_data *init_complex_data();
	
	struct map_data *map;
	room_data *room;
	
	obj_vnum rock_obj = 100;
	
	LL_FOREACH(land_map, map) {
		if (!SECT_FLAGGED(map->sector_type, SECTF_IS_ROAD)) {
			continue;
		}
		if (!(room = real_room(map->vnum))) {
			continue;
		}
		
		// ensure complex data
		if (!COMPLEX_DATA(room)) {
			COMPLEX_DATA(room) = init_complex_data();
		}
		
		// add build cost in rocks if necessary
		if (!GET_BUILT_WITH(room)) {
			add_to_resource_list(&GET_BUILT_WITH(room), RES_OBJECT, rock_obj, 20, 0);
		}
	}
	
	save_whole_world();
}


// adds approval
PLAYER_UPDATE_FUNC(b4_1_approve_players) {
	player_index_data *index;
	
	// fix some level glitches caused by this patch
	if (GET_IMMORTAL_LEVEL(ch) == -1) {
		GET_ACCESS_LEVEL(ch) = MIN(LVL_MORTAL, GET_ACCESS_LEVEL(ch));
	}
	else {
		GET_ACCESS_LEVEL(ch) = LVL_TOP - GET_IMMORTAL_LEVEL(ch);
		GET_ACCESS_LEVEL(ch) = MAX(GET_ACCESS_LEVEL(ch), LVL_GOD);
	}
	if (GET_ACCESS_LEVEL(ch) == LVL_GOD) {
		GET_ACCESS_LEVEL(ch) = LVL_START_IMM;
	}
	
	// if we should approve them (approve all imms now)
	if (IS_IMMORTAL(ch) || (GET_ACCESS_LEVEL(ch) >= LVL_MORTAL && config_get_bool("auto_approve"))) {
		if (config_get_bool("approve_per_character")) {
			SET_BIT(PLR_FLAGS(ch), PLR_APPROVED);
		}
		else {	// per-account (default)
			SET_BIT(GET_ACCOUNT(ch)->flags, ACCT_APPROVED);
			SAVE_ACCOUNT(GET_ACCOUNT(ch));
		}
	}
	
	// update the index in case any of this changed
	if ((index = find_player_index_by_idnum(GET_IDNUM(ch)))) {
		update_player_index(index, ch);
	}
}


// adds current mount to mounts list
PLAYER_UPDATE_FUNC(b4_2_mount_update) {
	void check_delayed_load(char_data *ch);
	
	check_delayed_load(ch);
	
	if (GET_MOUNT_VNUM(ch)) {
		add_mount(ch, GET_MOUNT_VNUM(ch), GET_MOUNT_FLAGS(ch) & ~MOUNT_RIDING);
	}
}


// sets default fight message flags
PLAYER_UPDATE_FUNC(b4_4_fight_messages) {
	SET_BIT(GET_FIGHT_MESSAGES(ch), DEFAULT_FIGHT_MESSAGES);
}


// convert data on unfinished buildings and disrepair
void b4_15_building_update(void) {
	extern struct resource_data *combine_resources(struct resource_data *combine_a, struct resource_data *combine_b);
	extern struct resource_data *copy_resource_list(struct resource_data *input);
	
	struct resource_data *res, *disrepair_res;
	room_data *room, *next_room;
	
	HASH_ITER(hh, world_table, room, next_room) {
		// add INCOMPLETE aff
		if (BUILDING_RESOURCES(room) && !IS_DISMANTLING(room)) {
			SET_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_INCOMPLETE);
			SET_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_INCOMPLETE);
		}
		
		// convert maintenance
		if (COMPLEX_DATA(room) && GET_BUILDING(room) && COMPLEX_DATA(room)->disrepair > 0) {
			// add maintenance
			if (GET_BLD_YEARLY_MAINTENANCE(GET_BUILDING(room))) {
				// basic stuff
				disrepair_res = copy_resource_list(GET_BLD_YEARLY_MAINTENANCE(GET_BUILDING(room)));
				
				// multiply by years of disrepair
				LL_FOREACH(disrepair_res, res) {
					res->amount *= COMPLEX_DATA(room)->disrepair;
				}
				
				// combine into existing resources
				if (BUILDING_RESOURCES(room)) {
					res = BUILDING_RESOURCES(room);
					GET_BUILDING_RESOURCES(room) = combine_resources(res, disrepair_res);
					free_resource_list(res);
				}
				else {
					GET_BUILDING_RESOURCES(room) = disrepair_res;
				}
			}
			
			// add damage (10% per year of disrepair)
			COMPLEX_DATA(room)->damage += COMPLEX_DATA(room)->disrepair * GET_BLD_MAX_DAMAGE(GET_BUILDING(room)) / 10;
		}
		
		// clear this
		if (COMPLEX_DATA(room)) {
			COMPLEX_DATA(room)->disrepair = 0;
		}
	}
	
	save_whole_world();
}


// 4.19 removes the vampire flag
PLAYER_UPDATE_FUNC(b4_19_update_players) {
	bitvector_t PLR_VAMPIRE = BIT(14);
	REMOVE_BIT(PLR_FLAGS(ch), PLR_VAMPIRE);
}


// 4.32 moves 2 rmt flags to fnc flags
void b4_32_convert_rmts(void) {
	bitvector_t RMT_PIGEON_POST = BIT(9);	// j. can use mail here
	bitvector_t RMT_COOKING_FIRE = BIT(10);	// k. can cook here

	room_template *rmt, *next_rmt;
	
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		if (IS_SET(GET_RMT_FLAGS(rmt), RMT_PIGEON_POST)) {
			REMOVE_BIT(GET_RMT_FLAGS(rmt), RMT_PIGEON_POST);
			SET_BIT(GET_RMT_FUNCTIONS(rmt), FNC_MAIL);
			save_library_file_for_vnum(DB_BOOT_RMT, GET_RMT_VNUM(rmt));
			log("- updated rmt %d: PIGEON-POST", GET_RMT_VNUM(rmt));
		}
		if (IS_SET(GET_RMT_FLAGS(rmt), RMT_COOKING_FIRE)) {
			REMOVE_BIT(GET_RMT_FLAGS(rmt), RMT_COOKING_FIRE);
			SET_BIT(GET_RMT_FUNCTIONS(rmt), FNC_COOKING_FIRE);
			save_library_file_for_vnum(DB_BOOT_RMT, GET_RMT_VNUM(rmt));
			log("- updated rmt %d: COOKING-FIRE", GET_RMT_VNUM(rmt));
		}
	}
}


// 4.36 needs triggers attached to studies
void b4_36_study_triggers(void) {
	const any_vnum bld_study = 5608, attach_trigger = 5609;
	struct trig_proto_list *tpl;
	room_data *room;
	
	LL_FOREACH2(interior_room_list, room, next_interior) {
		if (!GET_BUILDING(room) || GET_BLD_VNUM(GET_BUILDING(room)) != bld_study) {
			continue;
		}
		
		CREATE(tpl, struct trig_proto_list, 1);
		tpl->vnum = attach_trigger;
		LL_CONCAT(room->proto_script, tpl);
		
		assign_triggers(room, WLD_TRIGGER);
	}
}


// 4.38 needs triggers attached to towers
void b4_38_tower_triggers(void) {
	const any_vnum bld_tower = 5511, attach_trigger = 5511;
	struct trig_proto_list *tpl;
	room_data *room;
	
	LL_FOREACH2(interior_room_list, room, next_interior) {
		if (!GET_BUILDING(room) || GET_BLD_VNUM(GET_BUILDING(room)) != bld_tower) {
			continue;
		}
		
		CREATE(tpl, struct trig_proto_list, 1);
		tpl->vnum = attach_trigger;
		LL_CONCAT(room->proto_script, tpl);
		
		assign_triggers(room, WLD_TRIGGER);
	}
}


// b4.39 convert stored data from old files
void b4_39_data_conversion(void) {
	const char *EXP_FILE = LIB_ETC"exp_cycle";	// used prior to this patch
	const char *TIME_FILE = LIB_ETC"time";
	
	long l_in;
	int i_in;
	FILE *fl;
	
	// exp cycle file
	if ((fl = fopen(EXP_FILE, "r"))) {
		fscanf(fl, "%d\n", &i_in);
		data_set_long(DATA_DAILY_CYCLE, i_in);
		log(" - imported daily cycle %ld", data_get_long(DATA_DAILY_CYCLE));
		fclose(fl);
		unlink(EXP_FILE);
	}
	
	// time file
	if ((fl = fopen(TIME_FILE, "r"))) {
		fscanf(fl, "%ld\n", &l_in);
		data_set_long(DATA_WORLD_START, l_in);
		log(" - imported start time %ld", data_get_long(DATA_WORLD_START));
		fclose(fl);
		unlink(TIME_FILE);
		
		reset_time();
	}
}


/**
* Performs some auto-updates when the mud detects a new version.
*/
void check_version(void) {
	void resort_empires(bool force);
	
	int last, iter, current = NOTHING;
	
	#define MATCH_VERSION(name)  (!str_cmp(versions_list[iter], name))
	
	last = get_last_boot_version();
	
	// updates for every version since the last boot
	for (iter = 0; *versions_list[iter] != '\n'; ++iter) {
		current = iter;
		
		// skip versions below last-boot
		if (last != NOTHING && iter <= last) {
			continue;
		}
		
		// version-specific updates
		if (MATCH_VERSION("b2.5")) {
			void set_workforce_limit_all(empire_data *emp, int chore, int limit);
			
			log("Applying b2.5 update to empires...");
			empire_data *emp, *next_emp;
			HASH_ITER(hh, empire_table, emp, next_emp) {
				// auto-balance was removed and the same id was used for dismantle-mines
				set_workforce_limit_all(emp, CHORE_DISMANTLE_MINES, 0);
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
			}
		}
		if (MATCH_VERSION("b2.8")) {
			log("Applying b2.8 update to players...");
			update_all_players(NULL, b2_8_update_players);
		}
		if (MATCH_VERSION("b2.9")) {
			log("Applying b2.9 update to crops...");
			// this is actually a bug that occurred on EmpireMUDs that patched
			// b2.8 on a live copy; this will look for tiles that are in an
			// error state -- crops that were in the 'seeded' state during the
			// b2.8 reboot would have gotten bad original-sect data
			room_data *room, *next_room;
			HASH_ITER(hh, world_table, room, next_room) {
				if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && SECT_FLAGGED(BASE_SECT(room), SECTF_HAS_CROP_DATA) && !SECT_FLAGGED(BASE_SECT(room), SECTF_CROP)) {
					// normal case: crop with a 'Seeded' original sect
					// the fix is just to set the original sect to the current
					// sect so it will detect a new sect on-harvest instead of
					// setting it back to seeded
					change_base_sector(room, SECT(room));
				}
				else if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && !ROOM_SECT_FLAGGED(room, SECTF_CROP) && SECT(room) == BASE_SECT(room)) {
					// second error case: a Seeded crop with itself as its
					// original sect: detect a new original sect
					extern const sector_vnum climate_default_sector[NUM_CLIMATES];
					sector_data *sect;
					crop_data *cp;
					if ((cp = ROOM_CROP(room)) && (sect = sector_proto(climate_default_sector[GET_CROP_CLIMATE(cp)]))) {
						change_base_sector(room, sect);
					}
				}
			}
		}
		if (MATCH_VERSION("b2.11")) {
			void save_trading_post();
			
			struct empire_unique_storage *eus;
			struct trading_post_data *tpd;
			empire_data *emp, *next_emp;
			char_data *mob, *mobpr;
			obj_data *obj, *objpr;
			
			log("Applying b2.11 update:");
			log(" - assigning mob triggers...");
			for (mob = character_list; mob; mob = mob->next) {
				if (IS_NPC(mob) && (mobpr = mob_proto(GET_MOB_VNUM(mob)))) {
					mob->proto_script = copy_trig_protos(mobpr->proto_script);
					assign_triggers(mob, MOB_TRIGGER);
				}
			}
			
			log(" - assigning triggers to object list...");
			for (obj = object_list; obj; obj = obj->next) {
				if ((objpr = obj_proto(GET_OBJ_VNUM(obj)))) {
					obj->proto_script = copy_trig_protos(objpr->proto_script);
					assign_triggers(obj, OBJ_TRIGGER);
				}
			}
			
			log(" - assigning triggers to warehouse objects...");
			HASH_ITER(hh, empire_table, emp, next_emp) {
				for (eus = EMPIRE_UNIQUE_STORAGE(emp); eus; eus = eus->next) {
					if (eus->obj && (objpr = obj_proto(GET_OBJ_VNUM(eus->obj)))) {
						eus->obj->proto_script = copy_trig_protos(objpr->proto_script);
						assign_triggers(eus->obj, OBJ_TRIGGER);
					}
				}
			}
			
			log(" - assigning triggers to trading post objects...");
			for (tpd = trading_list; tpd; tpd = tpd->next) {
				if (tpd->obj && (objpr = obj_proto(GET_OBJ_VNUM(tpd->obj)))) {
					tpd->obj->proto_script = copy_trig_protos(objpr->proto_script);
					assign_triggers(tpd->obj, OBJ_TRIGGER);
				}
			}
			
			log(" - assigning triggers to player inventories...");
			update_all_players(NULL, b2_11_update_players);
			
			// ensure everything gets saved this way since we won't do this again
			save_all_empires();
			save_trading_post();
			save_whole_world();
		}
		if (MATCH_VERSION("b3.0")) {
			log("Applying b3.0 update to crops...");
			// this is a repeat of the b2.9 update, but should fix additional
			// rooms
			room_data *room, *next_room;
			HASH_ITER(hh, world_table, room, next_room) {
				if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && SECT_FLAGGED(BASE_SECT(room), SECTF_HAS_CROP_DATA) && !SECT_FLAGGED(BASE_SECT(room), SECTF_CROP)) {
					// normal case: crop with a 'Seeded' original sect
					// the fix is just to set the original sect to the current
					// sect so it will detect a new sect on-harvest instead of
					// setting it back to seeded
					change_base_sector(room, SECT(room));
				}
				else if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && !ROOM_SECT_FLAGGED(room, SECTF_CROP) && SECT_FLAGGED(BASE_SECT(room), SECTF_HAS_CROP_DATA) && !SECT_FLAGGED(BASE_SECT(room), SECTF_CROP)) {
					// second error case: a Seeded crop with a Seeded crop as
					// its original sect: detect a new original sect
					extern const sector_vnum climate_default_sector[NUM_CLIMATES];
					sector_data *sect;
					crop_data *cp;
					if ((cp = ROOM_CROP(room)) && (sect = sector_proto(climate_default_sector[GET_CROP_CLIMATE(cp)]))) {
						change_base_sector(room, sect);
					}
				}
			}
		}
		if (MATCH_VERSION("b3.1")) {
			log("Applying b3.1 update to mines...");
			b3_1_mine_update();
		}
		if (MATCH_VERSION("b3.2")) {
			b3_2_map_and_gear();
		}
		if (MATCH_VERSION("b3.6")) {
			b3_6_einv_fix();
		}
		if (MATCH_VERSION("b3.8")) {
			void b3_8_ship_update(void);	// vehicles.c
			log("Applying b3.8 update to vehicles...");
			b3_8_ship_update();
		}
		if (MATCH_VERSION("b3.11")) {
			log("Applying b3.11 fix to ships...");
			b3_11_ship_fix();
		}
		if (MATCH_VERSION("b3.12")) {
			log("Applying b3.12 removal of stray affect flag...");
			update_all_players(NULL, b3_12_update_players);
		}
		if (MATCH_VERSION("b3.15")) {
			log("Spawning b3.15 crops...");
			b3_15_crop_update();
		}
		if (MATCH_VERSION("b3.17")) {
			log("Adding b3.17 road data...");
			b3_17_road_update();
		}
		if (MATCH_VERSION("b4.1")) {
			log("Adding b4.1 approval data...");
			update_all_players(NULL, b4_1_approve_players);
			reread_empire_tech(NULL);
			resort_empires(TRUE);
		}
		if (MATCH_VERSION("b4.2")) {
			log("Adding b4.2 mount data...");
			update_all_players(NULL, b4_2_mount_update);
		}
		if (MATCH_VERSION("b4.4")) {
			log("Adding b4.4 fight messages...");
			update_all_players(NULL, b4_4_fight_messages);
		}
		if (MATCH_VERSION("b4.15")) {
			log("Converting b4.15 building data...");
			b4_15_building_update();
		}
		if (MATCH_VERSION("b4.19")) {
			log("Applying b4.19 update to players...");
			update_all_players(NULL, b4_19_update_players);
		}
		if (MATCH_VERSION("b4.32")) {
			log("Applying b4.32 update to rmts...");
			b4_32_convert_rmts();
		}
		if (MATCH_VERSION("b4.36")) {
			log("Applying b4.36 update to studies...");
			b4_36_study_triggers();
		}
		if (MATCH_VERSION("b4.38")) {
			log("Applying b4.38 update to towers...");
			b4_38_tower_triggers();
		}
		if (MATCH_VERSION("b4.39")) {
			log("Converting datat to b4.39 format...");
			b4_39_data_conversion();
		}
	}
	
	write_last_boot_version(current);
}


 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS HELPERS ///////////////////////////////////////////////////

/**
* This adapts a saved empire's chore data from the pre-2.0b3.6 format, to the
* newer version that is done per-island. It uses the temporary_room_data that
* exists during startup to find a list of islands the empire controls. No other
* ownership data is available during startup.
* 
* @param empire_data *emp The empire to add chore data for.
* @param int chore Which CHORE_ to turn on.
*/
void assign_old_workforce_chore(empire_data *emp, int chore) {
	void set_workforce_limit(empire_data *emp, int island_id, int chore, int limit);
	
	struct trd_type *trd, *next_trd;
	struct map_data *map;
	int last_isle = -1;
	
	if (chore < 0 || chore >= NUM_CHORES) {
		return;
	}
	
	HASH_ITER(hh, temporary_room_data, trd, next_trd) {
		if (trd->owner != EMPIRE_VNUM(emp)) {	// ensure is this empire
			continue;
		}
		if (trd->vnum >= MAP_SIZE) {	// ensure is on map
			continue;
		}
		
		// only bother if different from the last island found
		map = &(world_map[MAP_X_COORD(trd->vnum)][MAP_Y_COORD(trd->vnum)]);
		if (map->island != NO_ISLAND && last_isle != map->island) {
			set_workforce_limit(emp, map->island, chore, WORKFORCE_UNLIMITED);
			last_isle = map->island;
		}
	}
}


/* reset the time in the game from file */
void reset_time(void) {
	long beginning_of_time = data_get_long(DATA_WORLD_START);
	
	// a whole new world!
	if (!beginning_of_time) {
		beginning_of_time = time(0);
		data_set_long(DATA_WORLD_START, beginning_of_time);
	}

	time_info = *mud_time_passed(time(0), beginning_of_time);

	if (time_info.hours <= 6)
		weather_info.sunlight = SUN_DARK;
	else if (time_info.hours == 7)
		weather_info.sunlight = SUN_RISE;
	else if (time_info.hours <= 18)
		weather_info.sunlight = SUN_LIGHT;
	else if (time_info.hours == 19)
		weather_info.sunlight = SUN_SET;
	else
		weather_info.sunlight = SUN_DARK;

	log("   Current Gametime: %dH %dD %dM %dY.", time_info.hours, time_info.day, time_info.month, time_info.year);

	weather_info.pressure = 960;
	if ((time_info.month >= 5) && (time_info.month <= 8))
		weather_info.pressure += number(1, 50);
	else
		weather_info.pressure += number(1, 80);

	weather_info.change = 0;

	if (weather_info.pressure <= 980)
		weather_info.sky = SKY_LIGHTNING;
	else if (weather_info.pressure <= 1000)
		weather_info.sky = SKY_RAINING;
	else if (weather_info.pressure <= 1020)
		weather_info.sky = SKY_CLOUDY;
	else
		weather_info.sky = SKY_CLOUDLESS;
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
	num_of_start_locs = count;
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
		messages->next = fight_messages[i].msg;
		fight_messages[i].msg = messages;

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
	if (num_intros > 0 && intros) {
		for (iter = 0; iter < num_intros; ++iter) {
			free(intros[iter]);
		}
		free(intros);
		intros = NULL;
	}
	
	num_intros = 0;

	sprintf(lbuf, "%s%s", INTROS_PREFIX, INDEX_FILE);
	if (!(index = fopen(lbuf, "r"))) {
		log("SYSERR: opening index file '%s': %s", lbuf, strerror(errno));
		exit(1);
	}

	// count records in the index
	fscanf(index, "%s\n", lbuf2);
	while (*lbuf2 != '$') {
		++num_intros;
		fscanf(index, "%s\n", lbuf2);
	}

	if (num_intros <= 0) {
		log("SYSERR: boot error - 0 intro screens counted in %s.", lbuf);
		exit(1);
	}

	rewind(index);
	CREATE(intros, char*, num_intros);
	iter = 0;
	
	fscanf(index, "%s\n", lbuf2);
	while (*lbuf2 != '$') {
		sprintf(lbuf, "%s%s", INTROS_PREFIX, lbuf2);
		if (file_to_string_alloc(lbuf, intros + iter) == 0) {
			prune_crlf(intros[iter]);
		}
		++iter;
		
		fscanf(index, "%s\n", lbuf2);
	}
	
	log("Loaded %d intro screens.", num_intros);
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
	struct trading_post_data *tpd = NULL, *last_tpd = NULL;
	char line[256], str_in[256];
	obj_data *obj;
	int int_in[5];
	long long_in;
	FILE *fl;
	
	if (!(fl = fopen(TRADING_POST_FILE, "r"))) {
		// not a problem (no trading post exists)
		return;
	}
	
	// if somehow the trading list already exists
	if ((last_tpd = trading_list)) {
		while (last_tpd->next) {
			last_tpd = last_tpd->next;
		}
	}
	
	while (get_line(fl, line)) {
		switch (*line) {
			case 'T': {	// begin new trade item
				CREATE(tpd, struct trading_post_data, 1);
				tpd->next = NULL;
				
				// put at end of list
				if (last_tpd) {
					last_tpd->next = tpd;
				}
				else {
					trading_list = tpd;
				}
				last_tpd = tpd;
				
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
				obj = Obj_load_from_file(fl, atoi(line+1), &int_in[0], NULL);	// last val is junk
				
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
	
	if (!(fl = fopen(TRADING_POST_FILE, "w"))) {
		log("SYSERR: Unable to open file %s for writing.", TRADING_POST_FILE);
		return;
	}
	
	for (tpd = trading_list; tpd; tpd = tpd->next) {
		fprintf(fl, "T %d %s %ld %d %d %d %d\n", tpd->player, bitv_to_alpha(tpd->state), tpd->start, tpd->for_hours, tpd->buy_cost, tpd->post_cost, tpd->coin_type);
		if (tpd->obj) {
			Crash_save_one_obj_to_file(fl, tpd->obj, 0);
		}
	}
	
	fprintf(fl, "$~\n");
	fclose(fl);
}
