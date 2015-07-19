/* ************************************************************************
*   File: db.c                                            EmpireMUD 2.0b2 *
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
#include "mail.h"
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
void index_boot(int mode);
extern obj_data *Obj_load_from_file(FILE *fl, obj_vnum vnum, int *location, char_data *notify);

// local functions
int file_to_string_alloc(const char *name, char **buf);
void get_one_line(FILE *fl, char *buf);
void index_boot_help();
void save_daily_cycle();


 //////////////////////////////////////////////////////////////////////////////
//// GLOBAL DATA /////////////////////////////////////////////////////////////

// adventures
adv_data *adventure_table = NULL;	// adventure hash table

// buildings
bld_data *building_table = NULL;	// building hash table

// crafts
craft_data *craft_table = NULL;	// master crafting table
craft_data *sorted_crafts = NULL;	// sorted craft table

// crops
crop_data *crop_table = NULL;	// crop hash table

// empires
empire_data *empire_table = NULL;	// hash table of empires
double empire_score_average[NUM_SCORES];
struct trading_post_data *trading_list = NULL;	// global LL of trading post stuff

// fight system
struct message_list fight_messages[MAX_MESSAGES];	// fighting messages

// game config
time_t boot_time = 0;	// time of mud boot
int daily_cycle = 0;	// this is a timestamp for the last time skills/exp reset
int Global_ignore_dark = 0;	// For use in public channels
int no_mail = 0;	// mail disabled?
int no_rent_check = 0;	// skip rent check on boot?
struct time_info_data time_info;	// the infomation about the time
struct weather_data weather_info;	// the infomation about the weather
int wizlock_level = 0;	// level of game restriction
char *wizlock_message = NULL;	// Message sent to people trying to connect

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

// objects
obj_data *object_list = NULL;	// global linked list of objs
obj_data *object_table = NULL;	// hash table of objs

// players
struct player_index_element *player_table = NULL;	// index to plr file
FILE *player_fl = NULL;	// file desc of player file
int top_of_p_table = 0;	// ref to top of table
int top_of_p_file = 0;	// ref of size of p file
int top_idnum = 0;	// highest idnum in use
int top_account_id = 0;  // highest account number in use, determined during startup
struct group_data *group_list = NULL;	// global LL of groups

// room templates
room_template *room_template_table = NULL;	// hash table of room templates

// sectors
sector_data *sector_table = NULL;	// sector hash table

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
int max_mob_id = MOB_ID_BASE;	// for unique mob id's
int max_obj_id = OBJ_ID_BASE;	// for unique obj id's
int dg_owner_purged;	// For control of scripts

// world / rooms
room_data *world_table = NULL;	// hash table of the whole world
room_data *interior_world_table = NULL;	// hash table of the interior world
bool world_is_sorted = FALSE;	// to prevent unnecessary re-sorts
bool need_world_index = TRUE;	// used to trigger world index saving (always save at least once)
struct island_info *island_table = NULL; // hash table for all the islands


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
};


 //////////////////////////////////////////////////////////////////////////////
//// STARTUP /////////////////////////////////////////////////////////////////

/**
* Body of the booting system: this loads a lot of game data, although the
* world (objs, mobs, etc) has its own function.
*/
void boot_db(void) {
	void Read_Invalid_List();
	void boot_social_messages();
	void boot_world();
	void build_player_index();
	void check_ruined_cities();
	void delete_orphaned_rooms();
	void init_config_system();
	void init_skills();
	void load_banned();
	void load_daily_cycle();
	void load_intro_screens();
	void load_fight_messages();
	void load_player_data_at_startup();
	void load_tips_of_the_day();
	void load_trading_post();
	void reset_time();
	void sort_commands();
	void startup_room_reset();
	void update_obj_file();
	void update_ships();

	log("Boot db -- BEGIN.");
	
	log("Loading game config system.");
	init_config_system();

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
	
	// skills are needed for some world files (crafts?)
	log("Initializing skills.");
	init_skills();

	// Load the world!
	boot_world();

	log("Loading help entries.");
	index_boot_help();

	log("Generating player index.");
	build_player_index();
	
	log(" Calculating territory and members...");
	reread_empire_tech(NULL);
	
	log(" Checking for ruined cities...");
	check_ruined_cities();

	log("Loading fight messages.");
	load_fight_messages();

	log("Loading social messages.");
	boot_social_messages();
	
	log("Loading trading post.");
	load_trading_post();

	log("Sorting command list.");
	sort_commands();

	log("Booting mail system.");
	if (!scan_file()) {
		log("    Mail boot failed -- Mail system disabled");
		no_mail = 1;
	}
	
	// sends own log
	load_tips_of_the_day();
	
	log("Reading banned site and invalid-name list.");
	load_banned();
	Read_Invalid_List();

	if (!no_rent_check) {
		log("Deleting timed-out crash and rent files:");
		update_obj_file();
		log("   Done.");
	}
	
	// this loads objs and mobs back into the world
	log("Resetting all rooms.");
	startup_room_reset();

	log("Updating all ships.");
	update_ships();
	
	log("Checking for orphaned rooms...");
	delete_orphaned_rooms();

	load_daily_cycle();
	log("Beginning skill reset cycle at %d.", daily_cycle);

	log("Loading player data...");
	load_player_data_at_startup();

	boot_time = time(0);

	log("Boot db -- DONE.");
}


/**
* Load all of the game's world data during startup. NOTE: Order matters here,
* as some items rely on others. For example, sectors must be loaded before
* rooms (because rooms have sectors).
*/
void boot_world(void) {
	void check_for_bad_buildings();
	void check_for_bad_sectors();
	void check_newbie_islands();
	void clean_empire_logs();
	void index_boot_world();
	void load_empire_storage();
	void load_instances();
	void load_islands();
	void number_and_count_islands(bool reset);
	void renum_world();
	void setup_start_locations();
	void verify_sectors();

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
	
	// requires sectors, buildings, and room templates
	log("Loading rooms.");
	index_boot(DB_BOOT_WLD);
	
	// requires rooms
	log("Loading empires.");
	index_boot(DB_BOOT_EMP);
	clean_empire_logs();
		
	log("  Verifying world sectors.");
	verify_sectors();
	
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
	
	log("Loading craft recipes.");
	index_boot(DB_BOOT_CRAFT);
	
	log("Loading books into libraries.");
	index_boot(DB_BOOT_BOOKS);
	
	log("Loading adventures.");
	index_boot(DB_BOOT_ADV);
	
	log("Loading instances.");
	load_instances();
	
	log("Loading empire storage.");
	load_empire_storage();
	
	// check for bad data
	log("Verifying data.");
	check_for_bad_buildings();
	check_for_bad_sectors();
	
	log("Checking newbie islands.");
	check_newbie_islands();
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
	extern struct instance_data *find_instance_by_room(room_data *room);
	void unlink_instance_entrance(room_data *room);

	struct obj_storage_type *store, *next_store, *temp;
	bld_data *bld, *next_bld;
	room_data *room, *next_room;
	craft_data *craft, *next_craft;
	obj_data *obj, *next_obj;
	descriptor_data *dd;
	bool deleted = FALSE;
	
	// check buildings and adventures
	HASH_ITER(world_hh, world_table, room, next_room) {
		if (ROOM_SECT_FLAGGED(room, SECTF_MAP_BUILDING) && !GET_BUILDING(room)) {
			// map building
			log(" removing building at %d for bad building type", GET_ROOM_VNUM(room));
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
		else if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) && !find_instance_by_room(room)) {
			// room is marked as an instance entrance, but no instance is associated with it
			log(" unlinking instance entrance room %d for no association with an instance", GET_ROOM_VNUM(room));
			unlink_instance_entrance(room);
		}
		else if (COMPLEX_DATA(room) && !GET_BUILDING(room) && !GET_ROOM_TEMPLATE(room)) {
			log(" removing complex data for no building, no template data");
			free_complex_data(COMPLEX_DATA(room));
			COMPLEX_DATA(room) = NULL;
		}
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
	HASH_ITER(interior_hh, interior_world_table, room, next_room) {
		// boats are checked separately
		if (BUILDING_VNUM(room) == RTYPE_B_ONDECK && HOME_ROOM(room) == room) {
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
* Sets the boat pointers on all rooms associated with ships, sets the ship-
* present flag on map rooms, and deletes boat interiors that have no exterior.
*
* This is meant to be called at startup.
*/
void update_ships(void) {
	void check_for_ships_present(room_data *room);
	
	obj_data *o, *next_o;
	room_data *rl, *room, *next_room;
	bool found = FALSE;

	for (o = object_list; o; o = next_o) {
		next_o = o->next;

		if (GET_OBJ_TYPE(o) == ITEM_SHIP) {
			if ((rl = real_room(GET_SHIP_MAIN_ROOM(o)))) {
				COMPLEX_DATA(rl)->boat = o;
			}
			else {
				extract_obj(o);
			}
		}
	}

	HASH_ITER(world_hh, world_table, room, next_room) {
		if (BUILDING_VNUM(room) == RTYPE_B_ONDECK && HOME_ROOM(room) == room && !GET_BOAT(room)) {
			delete_room(room, FALSE);	// must check_all_exits
			found = TRUE;
		}
		else if (GET_ROOM_VNUM(room) < MAP_SIZE) {
			// regular map room? check for ships present
			check_for_ships_present(room);
		}
	}
	
	// only bother this if we deleted anything
	if (found) {
		check_all_exits();
		read_empire_territory(NULL);
	}
}


/**
* This ensures that every room has a valid sector.
*/
void verify_sectors(void) {
	extern crop_data *get_potential_crop_for_location(room_data *location);
	
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
	HASH_ITER(world_hh, world_table, room, next_room) {
		if (!SECT(room)) {
			SECT(room) = use_sect;
		}
		if (!ROOM_ORIGINAL_SECT(room)) {
			ROOM_ORIGINAL_SECT(room) = use_sect;
		}
		
		// also check for missing crop data
		if (SECT_FLAGGED(SECT(room), SECTF_HAS_CROP_DATA | SECTF_CROP) && !find_room_extra_data(room, ROOM_EXTRA_CROP_TYPE)) {
			crop_data *new_crop = get_potential_crop_for_location(room);
			if (new_crop) {
				set_room_extra_data(room, ROOM_EXTRA_CROP_TYPE, GET_CROP_VNUM(new_crop));
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
	HASH_ITER(world_hh, world_table, room, next_room) {
		// exits
		if (COMPLEX_DATA(room)) {
			// exit targets
			for (ex = COMPLEX_DATA(room)->exits; ex; ex = next_ex) {
				next_ex = ex->next;
				
				// validify exit
				ex->room_ptr = real_room(ex->to_room);
				if (!ex->room_ptr) {
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
				case 'f': {
					el.level = LVL_APPROVED;
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
			log("SYSERR: File '%s' listed in '%s/%s': %s", buf2, prefix, index_filename, strerror(errno));
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

/**
* Checks the newbie islands and applies their rules (abandons land).
*/
void check_newbie_islands(void) {
	struct island_info *isle = NULL;
	room_data *room, *next_room;
	int last_isle = -1;
	empire_data *emp;
	
	HASH_ITER(world_hh, world_table, room, next_room) {
		if (GET_ROOM_VNUM(room) >= MAP_SIZE || GET_ISLAND_ID(room) == NO_ISLAND) {
			continue;
		}
		
		// usually see many of the same island in a row, so this reduces lookups
		if (last_isle == -1 || GET_ISLAND_ID(room) != last_isle) {
			isle = get_island(GET_ISLAND_ID(room), TRUE);
		}
		
		// apply newbie rules?
		if (IS_SET(isle->flags, ISLE_NEWBIE)) {
			if ((emp = ROOM_OWNER(room)) && (EMPIRE_CREATE_TIME(emp) + (config_get_int("newbie_island_day_limit") * SECS_PER_REAL_DAY)) < time(0)) {
				log_to_empire(emp, ELOG_TERRITORY, "(%d, %d) abandoned on newbie island", FLAT_X_COORD(room), FLAT_Y_COORD(room));
				abandon_room(room);
			}
		}
	}
}


// adds a room to an island, then scans all adjacent rooms
static void recursive_island_scan(room_data *room, int island) {
	int dir;
	room_data *to_room;
	
	SET_ISLAND_ID(room, MAX(0, island));	// ensure it's never < 0, as that would go poorly
	
	for (dir = 0; dir < NUM_2D_DIRS; ++dir) {
		// TODO disabling ocean lookups on this real_shift would save RAM at startup
		to_room = real_shift(room, shift_dir[dir][0], shift_dir[dir][1]);
		
		// TODO <= 0 here -- isn't 0 a valid island num?
		if (to_room && !ROOM_SECT_FLAGGED(to_room, SECTF_NON_ISLAND) && GET_ISLAND_ID(to_room) <= 0) {
			recursive_island_scan(to_room, island);
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
	room_data *room, *next_room;
	struct island_info *isle;
	int id, iter;
	
	// find top island id (and reset if requested)
	top_island_num = -1;
	HASH_ITER(world_hh, world_table, room, next_room) {
		// map only
		if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
			continue;
		}
		
		if (reset || ROOM_SECT_FLAGGED(room, SECTF_NON_ISLAND)) {
			SET_ISLAND_ID(room, NO_ISLAND);
		}
		else {
			top_island_num = MAX(top_island_num, GET_ISLAND_ID(room));
		}
	}
	
	// now update the whole world -- if it's not a reset, this is done in two stages:
	
	// 1. expand EXISTING islands
	if (!reset) {
		HASH_ITER(world_hh, world_table, room, next_room) {
			// map only
			if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
				continue;
			}
			
			if (GET_ISLAND_ID(room) != NO_ISLAND) {
				recursive_island_scan(room, GET_ISLAND_ID(room));
			}
		}
	}
	
	// 2. look for places that have no island id but need one -- and also measure islands while we're here
	HASH_ITER(world_hh, world_table, room, next_room) {
		// map only!
		if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
			continue;
		}
		
		if (GET_ISLAND_ID(room) == NO_ISLAND && !ROOM_SECT_FLAGGED(room, SECTF_NON_ISLAND)) {
			recursive_island_scan(room, ++top_island_num);
		}
		
		// find helper entry
		id = GET_ISLAND_ID(room);
		if (id == NO_ISLAND) {
			// just an ocean
			continue;
		}
		
		HASH_FIND_INT(list, &id, data);
		if (!data) {	// or create one
			CREATE(data, struct island_read_data, 1);
			data->id = id;
			for (iter = 0; iter < NUM_SIMPLE_DIRS; ++iter) {
				data->edge[iter] = NOWHERE;
				data->edge_val[iter] = NOWHERE;
			}
			HASH_ADD_INT(list, id, data);
		}

		// update helper data
		data->size += 1;
		data->sum_x += FLAT_X_COORD(room);
		data->sum_y += FLAT_Y_COORD(room);
	
		// detect edges
		if (data->edge[NORTH] == NOWHERE || FLAT_Y_COORD(room) > data->edge_val[NORTH]) {
			data->edge[NORTH] = GET_ROOM_VNUM(room);
			data->edge_val[NORTH] = FLAT_Y_COORD(room);
		}
		if (data->edge[SOUTH] == NOWHERE || FLAT_Y_COORD(room) < data->edge_val[SOUTH]) {
			data->edge[SOUTH] = GET_ROOM_VNUM(room);
			data->edge_val[SOUTH] = FLAT_Y_COORD(room);
		}
		if (data->edge[EAST] == NOWHERE || FLAT_X_COORD(room) > data->edge_val[EAST]) {
			data->edge[EAST] = GET_ROOM_VNUM(room);
			data->edge_val[EAST] = FLAT_X_COORD(room);
		}
		if (data->edge[WEST] == NOWHERE || FLAT_X_COORD(room) < data->edge_val[WEST]) {
			data->edge[WEST] = GET_ROOM_VNUM(room);
			data->edge_val[WEST] = FLAT_X_COORD(room);
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

	GET_PFILEPOS(ch) = NOTHING;
	ch->vnum = NOBODY;
	GET_POS(ch) = POS_STANDING;
	MOB_INSTANCE_ID(ch) = NOTHING;
	MOB_DYNAMIC_SEX(ch) = NOTHING;
	MOB_DYNAMIC_NAME(ch) = NOTHING;
	MOB_PURSUIT_LEASH_LOC(ch) = NOWHERE;
}


/**
* Create a new mobile from a prototype.
*
* @param mob_vnum nr The number to load.
* @return char_data* The instantiated mob.
*/
char_data *read_mobile(mob_vnum nr) {
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

	GET_ID(mob) = max_mob_id++;
	/* find_char helper */
	add_to_lookup_table(GET_ID(mob), (void *)mob);

	copy_proto_script(proto, mob, MOB_TRIGGER);
	assign_triggers(mob, MOB_TRIGGER);

	return (mob);
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT LOADING //////////////////////////////////////////////////////////


void clear_object(obj_data *obj) {
	memset((char *) obj, 0, sizeof(obj_data));

	obj->vnum = NOTHING;
	IN_ROOM(obj) = NULL;
	obj->worn_on = NO_WEAR;
	
	obj->last_owner_id = NOBODY;
	obj->last_empire_id = NOTHING;
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

	GET_ID(obj) = max_obj_id++;
	/* find_obj helper */
	add_to_lookup_table(GET_ID(obj), (void *)obj);

	return (obj);
}


/**
* Create a new object from a prototype..
*
* @param obj_vnum nr The object vnum to load.
* @return obj_data* The instantiated item.
*/
obj_data *read_object(obj_vnum nr) {
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

	if (obj->obj_flags.timer == 0)
		obj->obj_flags.timer = UNLIMITED;
	
	GET_ID(obj) = max_obj_id++;
	/* find_obj helper */
	add_to_lookup_table(GET_ID(obj), (void *)obj);
	
	copy_proto_script(proto, obj, OBJ_TRIGGER);
	assign_triggers(obj, OBJ_TRIGGER);

	return (obj);
}


 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS HELPERS ///////////////////////////////////////////////////

/* this is necessary for the autowiz system */
void reload_wizlists(void) {
	file_to_string_alloc(WIZLIST_FILE, &wizlist);
	file_to_string_alloc(GODLIST_FILE, &godlist);
}


/* reset the time in the game from file */
void reset_time(void) {
	long load_time();	// db.c

	long beginning_of_time = load_time();

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

	HASH_ITER(world_hh, world_table, room, next_room) {
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
* Loads the timestamp for the daily skill gain cycle.
*/
void load_daily_cycle(void) {
	FILE *fl;

	if (!(fl = fopen(EXP_FILE, "r"))) {
		daily_cycle = time(0);
		save_daily_cycle();	// save now so we get a reset in 24 hours
		return;
	}

	fscanf(fl, "%d\n", &daily_cycle);
	fclose(fl);
}


/**
* Loads the "messages" file, which contains damage messages for various attack
* types.
*/
void load_fight_messages(void) {
	extern char *fread_action(FILE * fl, int nr);
	
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
* Loads (and sometimes creates) the timestamp file that we use to compute
* when the game "began".
*/
long load_time(void) {
	long t;
	FILE *fp;

	if (!(fp = fopen(TIME_FILE, "r"))) {
		if (!(fp = fopen(TIME_FILE, "w"))) {
			log("SYSERR: Unable to create a beginning_of_time file!");
			return 650336715;
		}
		fprintf(fp, "%ld\n", time(0));
		fclose(fp);
		return time(0);
	}
	fscanf(fp, "%ld\n", &t);
	fclose(fp);
	return t;
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
* Saves the timestamp file for the daily skill cycle.
*/
void save_daily_cycle(void) {
	FILE *fl;

	if (!(fl = fopen(EXP_FILE TEMP_SUFFIX, "w"))) {
		log("Error opening daily cycle file!");
		return;
	}

	fprintf(fl, "%d\n", daily_cycle);
	fclose(fl);
	
	rename(EXP_FILE TEMP_SUFFIX, EXP_FILE);
}


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
