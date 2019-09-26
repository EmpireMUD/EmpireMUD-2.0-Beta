/* ************************************************************************
*   File: db.h                                            EmpireMUD 2.0b5 *
*  Usage: header file for database handling                               *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

// DB_BOOT_x: arbitrary constants used by index_boot() (must be unique) -- these correspond to an array in discrete_load() too
#define DB_BOOT_WLD  0
#define DB_BOOT_MOB  1
#define DB_BOOT_OBJ  2
#define DB_BOOT_NAMES  3
#define DB_BOOT_EMP  4
#define DB_BOOT_BOOKS  5
#define DB_BOOT_CRAFT  6
#define DB_BOOT_BLD  7
#define DB_BOOT_TRG  8
#define DB_BOOT_CROP  9
#define DB_BOOT_SECTOR  10
#define DB_BOOT_ADV  11
#define DB_BOOT_RMT  12
#define DB_BOOT_GLB  13
#define DB_BOOT_ACCT  14
#define DB_BOOT_AUG  15
#define DB_BOOT_ARCH  16
#define DB_BOOT_ABIL  17
#define DB_BOOT_CLASS  18
#define DB_BOOT_SKILL  19
#define DB_BOOT_VEH  20
#define DB_BOOT_MORPH  21
#define DB_BOOT_QST  22
#define DB_BOOT_SOC  23
#define DB_BOOT_FCT  24
#define DB_BOOT_GEN  25
#define DB_BOOT_SHOP  26
#define DB_BOOT_PRG  27
#define DB_BOOT_EVT  28
#define NUM_DB_BOOT_TYPES  29	// total


// library sub-dirs
#define LIB_ACCTS  LIB_PLAYERS"accounts/"
#define LIB_WORLD  "world/"
#define LIB_TEXT  "text/"
#define LIB_TEXT_HELP  "text/help/"
#define LIB_MISC  "misc/"
#define LIB_ETC  "etc/"
#define LIB_BOARD  "boards/"
#define LIB_PLAYERS  "players/"
#define LIB_OBJPACK  "packs/"
#define LIB_EMPIRE  "empires/"


// file suffixes for common write files
#define SUF_PACK  "pack"
#define SUF_PLR  "plr"
#define SUF_DELAY  "delay"


// files used to block startup
#define KILLSCRIPT_FILE  "../.killscript"	// autorun: shut mud down
#define PAUSE_FILE  "../pause"	// autorun: don't restart mud


// for file writes to prevent outages by writing temp files first
#define TEMP_SUFFIX  ".temp"	// for safe filewrites


// names of various files and directories
#define INDEX_FILE  "index"	// index of world files
#define ABIL_PREFIX  LIB_WORLD"abil/"	// player abilities
#define ACCT_PREFIX  LIB_ACCTS	// account files
#define ADV_PREFIX  LIB_WORLD"adv/"	// adventure zones
#define ARCH_PREFIX  LIB_WORLD"arch/"	// archetypes
#define AUG_PREFIX  LIB_WORLD"aug/"	// augments
#define BLD_PREFIX  LIB_WORLD"bld/"	// building definitions
#define CLASS_PREFIX  LIB_WORLD"class/"	// player classes
#define CRAFT_PREFIX  LIB_WORLD"craft/"	// craft recipes
#define CROP_PREFIX  LIB_WORLD"crop/"	// crop definitions
#define EVT_PREFIX  LIB_WORLD"evt/"	// events
#define FCT_PREFIX  LIB_WORLD"fct/"	// factions
#define GEN_PREFIX  LIB_WORLD"gen/"	// generics
#define GLB_PREFIX  LIB_WORLD"glb/"	// global templates
#define WLD_PREFIX  LIB_WORLD"wld/"	// room definitions
#define MOB_PREFIX  LIB_WORLD"mob/"	// monster prototypes
#define MORPH_PREFIX  LIB_WORLD"morph/"	// morphs
#define NAMES_PREFIX  LIB_TEXT"names/"	// mob namelists
#define OBJ_PREFIX  LIB_WORLD"obj/"	// object prototypes
#define PRG_PREFIX  LIB_WORLD"prg/"	// empire progress
#define QST_PREFIX  LIB_WORLD"qst/"	// quests
#define RMT_PREFIX  LIB_WORLD"rmt/"	// room templates
#define SECTOR_PREFIX  LIB_WORLD"sect/"	// sect definitions
#define SHOP_PREFIX  LIB_WORLD"shop/"	// shops
#define SKILL_PREFIX  LIB_WORLD"skill/"	// player skills
#define SOC_PREFIX  LIB_WORLD"soc/"	// socials
#define TRG_PREFIX  LIB_WORLD"trg/"	// trigger files
#define VEH_PREFIX  LIB_WORLD"veh/"	// vehicle files
#define HLP_PREFIX  LIB_TEXT"help/"	// for HELP <keyword>
#define INTROS_PREFIX  LIB_TEXT"intros/"	// for intro screens
#define BOOK_PREFIX  "books/"	// for books.c
#define ELOG_PREFIX  LIB_EMPIRE"logs/"	// for empire logs
#define STORAGE_PREFIX  LIB_EMPIRE"storage/"	// for empire storage

// library file suffixes
#define ABIL_SUFFIX  ".abil"	// player abilities
#define ACCT_SUFFIX  ".acct"	// account file suffix
#define ADV_SUFFIX  ".adv"	// adventure file suffix
#define ARCH_SUFFIX  ".arch"	// archetype file suffix
#define AUG_SUFFIX  ".aug"	// augment file suffix
#define BLD_SUFFIX  ".bld"	// building file suffix
#define BOOK_SUFFIX  ".book"	// book file suffix
#define CLASS_SUFFIX  ".class"	// player classes
#define CRAFT_SUFFIX  ".craft"	// craft file suffix
#define CROP_SUFFIX  ".crop"	// crop file suffix
#define EMPIRE_SUFFIX  ".empire"	// empire file suffix
#define EVT_SUFFIX  ".evt"	// events
#define FCT_SUFFIX  ".fct"	// factions
#define GEN_SUFFIX  ".gen"	// generics
#define GLB_SUFFIX  ".glb"	// global suffix
#define MOB_SUFFIX  ".mob"	// mob suffix for file saves
#define MORPH_SUFFIX  ".morph"	// morph file suffix
#define OBJ_SUFFIX  ".obj"	// obj suffix for file saves
#define PRG_SUFFIX  ".prg"	// empire progress suffix
#define QST_SUFFIX  ".qst"	// quest file
#define RMT_SUFFIX  ".rmt"	// room template suffix
#define SECTOR_SUFFIX  ".sect"	// sector file suffix
#define SHOP_SUFFIX  ".shop"	// shop file suffix
#define SKILL_SUFFIX  ".skill"	// player skills
#define SOC_SUFFIX  ".soc"	// social file suffix
#define TRG_SUFFIX  ".trg"	// trigger file suffix
#define VEH_SUFFIX  ".veh"	// vehicle file suffix
#define WLD_SUFFIX  ".wld"	// suffix for rooms

// various text files
#define CREDITS_FILE  LIB_TEXT"credits"	// for the 'credits' command
#define SCREDITS_FILE  LIB_TEXT"credits.short"	// short version of credits
#define MOTD_FILE  LIB_TEXT"motd"	// messages of the day / mortal
#define IMOTD_FILE  LIB_TEXT"imotd"	// messages of the day / immort
#define HELP_PAGE_FILE  LIB_TEXT_HELP"screen"	// for HELP <CR>
#define INFO_FILE  LIB_TEXT"info"	// for INFO
#define WIZLIST_FILE  LIB_TEXT"wizlist"	// for WIZLIST
#define GODLIST_FILE  LIB_TEXT"godlist"	// for GODLIST
#define POLICIES_FILE  LIB_TEXT"policies"	// player policies/rules
#define HANDBOOK_FILE  LIB_TEXT"handbook"	// handbook for new immorts
#define NEWS_FILE  LIB_TEXT"news"	// shown when players type 'news'

// misc files (user-modifiable libs)
#define CONFIG_FILE  LIB_MISC"game_configs"  // config.c system
#define DATA_FILE  LIB_MISC"game_data"	// data.c system
#define IDEA_FILE  LIB_MISC"ideas"	// for the 'idea'-command
#define TYPO_FILE  LIB_MISC"typos"	//         'typo'
#define BUG_FILE  LIB_MISC"bugs"	//         'bug'
#define MESS_FILE  LIB_MISC"messages"	// damage messages
#define TIPS_OF_THE_DAY_FILE  LIB_MISC"tips"	// one-line tips shown on login
#define XNAME_FILE  LIB_MISC"xnames"	// invalid name substrings

// etc files (non-user-modifiable libs)
#define AUTOMESSAGE_FILE  LIB_ETC"automessages"	// the automessage system
#define BAN_FILE  LIB_ETC"badsites"	// for the siteban system
#define DAILY_QUEST_FILE  LIB_ETC"daily_quests"	// which quests are on/off
#define INSTANCE_FILE  LIB_ETC"instances"	// instanced adventures
#define ISLAND_FILE  LIB_ETC"islands"	// island info
#define NEW_WORLD_HINT_FILE  LIB_ETC"new_world"	// if present, moves einv on startup
#define RUNNING_EVENTS_FILE  LIB_ETC"events"	// data for events that are running
#define TRADING_POST_FILE  LIB_ETC"trading_post"	// for global trade
#define VERSION_FILE  LIB_ETC"version"	// for version tracking

// map output data
#define DATA_DIR  "../data/"
#define GEOGRAPHIC_MAP_FILE  DATA_DIR"map.txt"	// for map output
#define POLITICAL_MAP_FILE  DATA_DIR"map-political.txt"	// for political map
#define CITY_DATA_FILE  DATA_DIR"map-cities.txt"	// for cities on the website

// world blocks: the world is split into chunks for saving
#define WORLD_BLOCK_SIZE  (MAP_WIDTH * 5)	// number of rooms per .wld file
#define GET_WORLD_BLOCK(roomvnum)  (roomvnum == NOWHERE ? NOWHERE : (int)(roomvnum / WORLD_BLOCK_SIZE))

// additional files
#define WORLD_MAP_FILE  LIB_WORLD"base_map"	// storage for the game's base map
#define EVOLUTION_FILE  LIB_WORLD"evolutions"	// file generated by the 'evolve' utility

// used for many file reads:
#define READ_SIZE 256


// DATA_x: stored data system
#define DATA_DAILY_CYCLE  0	// timestamp of last day-reset (bonus exp, etc)
#define DATA_LAST_NEW_YEAR  1	// timestamp of last annual world update
#define DATA_WORLD_START  2	// timestamp of when the mud first started up
#define DATA_MAX_PLAYERS_TODAY  3	// players logged in today
#define DATA_LAST_ISLAND_DESCS  4	// last time island descs regenerated
#define NUM_DATAS  5


// DATYPE_x: types of stored data
#define DATYPE_INT  0
#define DATYPE_LONG  1
#define DATYPE_DOUBLE  2


// for DB_BOOT_ configs
struct db_boot_info_type {
	char *prefix;
	char *suffix;
	bool allow_zero;
};


// for DATA_ configuration
struct stored_data_type {
	char *name;	// how it's stored in the file
	int type;	// DATYPE_ const
};


// for storing data between reboots
struct stored_data {
	int key;	// DATA_ const
	int keytype;	// DATYPE_ const
	union {
		int int_val;
		long long_val;
		double double_val;
	} value;
	
	UT_hash_handle hh;
};


// public procedures in db.c
char *fread_string(FILE *fl, char *error);
void boot_db(void);

// global saves
void save_index(int type);
void save_library_file_for_vnum(int type, any_vnum vnum);

// world processors
void change_base_sector(room_data *room, sector_data *sect);
void change_terrain(room_data *room, sector_vnum sect);
void construct_building(room_data *room, bld_vnum type);
void disassociate_building(room_data *room);
void set_crop_type(room_data *room, crop_data *cp);


// various externs
extern int Global_ignore_dark;
extern struct time_info_data time_info;


// abilities
extern ability_data *ability_table;
extern ability_data *sorted_abilities;
extern ability_data *find_ability(char *argument);
extern ability_data *find_ability_by_name(char *name);
extern ability_data *find_ability_by_vnum(any_vnum vnum);
void free_ability(ability_data *abil);
extern char *get_ability_name_by_vnum(any_vnum vnum);

// accounts
void add_player_to_account(char_data *ch, account_data *acct);
extern account_data *create_account_for_player(char_data *ch);
extern account_data *find_account(int id);
void remove_player_from_account(char_data *ch);

// adventures
extern adv_data *adventure_table;
extern struct instance_data *instance_list;
void free_adventure(adv_data *adv);
extern adv_data *adventure_proto(adv_vnum vnum);

// archetypes
extern archetype_data *archetype_table;
extern archetype_data *sorted_archetypes;
extern archetype_data *archetype_proto(any_vnum vnum);
void free_archetype(archetype_data *arch);

// augments
extern augment_data *augment_table;
extern augment_data *sorted_augments;
extern augment_data *augment_proto(any_vnum vnum);
void free_augment(augment_data *aug);

// books
extern book_data *book_table;
extern struct author_data *author_table;
void free_book(book_data *book);

// buildings
extern bld_data *building_table;
void free_building(bld_data *building);
extern bld_data *building_proto(bld_vnum vnum);

// classes
extern class_data *class_table;
extern class_data *sorted_classes;
extern class_data *find_class(char *argument);
extern class_data *find_class_by_name(char *name);
extern class_data *find_class_by_vnum(any_vnum vnum);
void free_class(class_data *cls);

// crafts
extern craft_data *craft_table;
extern craft_data *sorted_crafts;
void free_craft(craft_data *craft);
extern craft_data *craft_proto(craft_vnum vnum);

// crops
extern crop_data *crop_table;
void free_crop(crop_data *cp);
extern crop_data *crop_proto(crop_vnum vnum);

// data system getters
extern double data_get_double(int key);
extern int data_get_int(int key);
extern long data_get_long(int key);

// data system setters
extern double data_set_double(int key, double value);
extern int data_set_int(int key, int value);
extern long data_set_long(int key, long value);

// descriptors
extern descriptor_data *descriptor_list;

// empires
extern empire_data *empire_table;
extern struct trading_post_data *trading_list;
extern bool check_delayed_refresh;
void delete_empire(empire_data *emp);
extern struct empire_island *get_empire_island(empire_data *emp, int island_id);
extern empire_data *get_or_create_empire(char_data *ch);
void free_empire(empire_data *emp);
void read_empire_members(empire_data *only_empire, bool read_techs);
void read_empire_territory(empire_data *emp, bool check_tech);
extern empire_data *real_empire(empire_vnum vnum);
void reread_empire_tech(empire_data *emp);
void save_empire(empire_data *e, bool save_all_parts);
void save_all_empires();

// empire offenses
void add_offense(empire_data *emp, int type, char_data *offender, room_data *loc, bitvector_t flags);
extern int avenge_offenses_from_empire(empire_data *emp, empire_data *foe);
extern int avenge_solo_offenses_from_player(empire_data *emp, char_data *foe);
void clean_empire_offenses();
extern int get_total_offenses_from_empire(empire_data *emp, empire_data *foe);
extern int get_total_offenses_from_char(empire_data *emp, char_data *ch);
extern bool offense_was_seen(char_data *ch, empire_data *emp, room_data *from_room);
void remove_offense(empire_data *emp, struct offense_data *off);
void remove_recent_offenses(empire_data *emp, int type, char_data *offender);

// extra descs
void free_extra_descs(struct extra_descr_data **list);

// events
extern event_data *event_table;
extern int top_event_id;
extern struct event_running_data *running_events;
extern bool events_need_save;
extern event_data *find_event_by_vnum(any_vnum vnum);
extern struct event_running_data *find_last_event_run_by_vnum(any_vnum event_vnum);
extern struct event_running_data *find_running_event_by_id(int id);
extern struct event_running_data *find_running_event_by_vnum(any_vnum event_vnum);
void free_event(event_data *event);
extern char *get_event_name_by_proto(any_vnum vnum);

// factions
extern faction_data *faction_table;
extern int MAX_REPUTATION;
extern int MIN_REPUTATION;
extern faction_data *sorted_factions;
extern faction_data *find_faction(char *argument);
extern faction_data *find_faction_by_name(char *name);
extern faction_data *find_faction_by_vnum(any_vnum vnum);
void free_faction(faction_data *fct);

// generics
extern generic_data *generic_table;
extern generic_data *sorted_generics;
void free_generic(generic_data *gen);
extern generic_data *find_generic(any_vnum vnum, int type);
extern generic_data *find_generic_by_name(int type, char *name, bool exact);
extern generic_data *real_generic(any_vnum vnum);
extern const char *get_generic_name_by_vnum(any_vnum vnum);
extern const char *get_generic_string_by_vnum(any_vnum vnum, int type, int pos);
extern int get_generic_value_by_vnum(any_vnum vnum, int type, int pos);

// globals
extern struct global_data *globals_table;
void free_global(struct global_data *glb);
extern struct global_data *global_proto(any_vnum vnum);

// interactions
void free_interactions(struct interaction_item **list);

// islands
extern struct island_info *island_table;
extern struct island_info *get_island(int island_id, bool create_if_missing);
extern struct island_info *get_island_by_coords(char *coords);
extern struct island_info *get_island_by_name(char_data *ch, char *name);
extern char *get_island_name_for(int island_id, char_data *for_ch);

// mobiles/chars
extern account_data *account_table;
extern char_data *character_list;
extern char_data *combat_list;
extern char_data *next_combat_list;
extern char_data *mobile_table;
extern player_index_data *player_table_by_idnum;
extern player_index_data *player_table_by_name;
extern player_index_data *find_player_index_by_idnum(int idnum);
extern player_index_data *find_player_index_by_name(char *name);
void init_player(char_data *ch);
extern char_data *read_mobile(mob_vnum nr, bool with_triggers);
extern char_data *mob_proto(mob_vnum vnum);
void clear_char(char_data *ch);
void init_player_specials(char_data *ch);
void reset_char(char_data *ch);
void free_char(char_data *ch);
void set_title(char_data *ch, char *title);

void save_char(char_data *ch, room_data *load_room);
#define SAVE_CHAR(ch)  save_char((ch), (IN_ROOM(ch) ? IN_ROOM(ch) : (GET_LOADROOM(ch) != NOWHERE ? real_room(GET_LOADROOM(ch)) : NULL)))

void update_player_index(player_index_data *index, char_data *ch);
extern char_data *find_or_load_player(char *name, bool *is_file);
void store_loaded_char(char_data *ch);
char_data *load_player(char *name, bool normal);

// morphs
extern morph_data *morph_table;
extern morph_data *sorted_morphs;
extern morph_data *morph_proto(any_vnum vnum);
void free_morph(morph_data *morph);

// objects
extern obj_data *object_list;
extern obj_data *object_table;
obj_data *create_obj(void);
void clear_object(obj_data *obj);
void free_obj(obj_data *obj);
obj_data *obj_proto(obj_vnum vnum);
obj_data *read_object(obj_vnum nr, bool with_triggers);

// players
extern struct group_data *group_list;
extern bool pause_affect_total;
extern char_data *find_player_in_room_by_id(room_data *room, int id);
extern char_data *is_at_menu(int id);
extern char_data *is_playing(int id);

// progress
extern progress_data *progress_table;
extern progress_data *sorted_progress;
extern bool need_progress_refresh;
extern char *get_progress_name_by_proto(any_vnum vnum);
extern progress_data *real_progress(any_vnum vnum);
void free_progress(progress_data *prg);

// quests
extern struct quest_data *quest_table;
extern quest_data *quest_proto(any_vnum vnum);
void free_quest(quest_data *quest);

// room templates
extern room_template *room_template_table;
void free_room_template(room_template *rmt);
extern room_template *room_template_proto(rmt_vnum vnum);

// sectors
extern sector_data *sector_table;
extern struct sector_index_type *sector_index;
extern struct sector_index_type *find_sector_index(sector_vnum vnum);
void free_sector(struct sector_data *st);
void perform_change_base_sect(room_data *loc, struct map_data *map, sector_data *sect);
void perform_change_sect(room_data *loc, struct map_data *map, sector_data *sect);
extern sector_data *sector_proto(sector_vnum vnum);

// shops
extern shop_data *shop_table;
void free_shop(shop_data *shop);
extern shop_data *real_shop(any_vnum vnum);

// skills
extern skill_data *skill_table;
extern skill_data *sorted_skills;
extern skill_data *find_skill(char *argument);
extern skill_data *find_skill_by_name(char *name);
extern skill_data *find_skill_by_vnum(any_vnum vnum);
void free_skill(skill_data *skill);
extern char *get_skill_abbrev_by_vnum(any_vnum vnum);
extern char *get_skill_name_by_vnum(any_vnum vnum);

// socials
extern social_data *social_table;
extern social_data *sorted_socials;
extern social_data *social_proto(any_vnum vnum);
void free_social(social_data *soc);

// stored event libs
void add_stored_event(struct stored_event **list, int type, struct dg_event *event);
void cancel_stored_event(struct stored_event **list, int type);
void delete_stored_event(struct stored_event **list, int type);
extern struct stored_event *find_stored_event(struct stored_event *list, int type);

// stored event helpers
#define add_stored_event_room(room, type, ev)  add_stored_event(&SHARED_DATA(room)->events, type, ev)
#define cancel_stored_event_room(room, type)  cancel_stored_event(&SHARED_DATA(room)->events, type)
#define delete_stored_event_room(room, type)  delete_stored_event(&SHARED_DATA(room)->events, type)
#define find_stored_event_room(room, type)  find_stored_event(SHARED_DATA(room)->events, type)

// triggers
extern trig_data *trigger_table;
extern trig_data *trigger_list;
extern trig_data *random_triggers;
extern trig_data *free_trigger_list;

// vehicles
extern vehicle_data *vehicle_list;
extern vehicle_data *vehicle_table;
extern vehicle_data *vehicle_proto(any_vnum vnum);
void free_vehicle(vehicle_data *veh);
extern vehicle_data *read_vehicle(any_vnum vnum, bool with_triggers);

// world
void check_all_exits();
extern struct room_direction_data *create_exit(room_data *from, room_data *to, int dir, bool back);
void delete_room(room_data *room, bool check_exits);
extern room_data *world_table;
extern room_data *interior_room_list;
extern struct map_data world_map[MAP_WIDTH][MAP_HEIGHT];
extern struct map_data *land_map;
room_data *real_real_room(room_vnum vnum);
room_data *real_room(room_vnum vnum);

// misc
extern struct obj_apply *copy_obj_apply_list(struct obj_apply *list);
void free_resource_list(struct resource_data *list);

// more frees
void free_apply_list(struct apply_data *list);
void free_obj_apply_list(struct obj_apply *list);
void free_icon_set(struct icon_data **set);
void free_exit_template(struct exit_template *ex);


/* global buffering system */
#ifdef __DB_C__
	char buf[MAX_STRING_LENGTH];
	char buf1[MAX_STRING_LENGTH];
	char buf2[MAX_STRING_LENGTH];
	char arg[MAX_STRING_LENGTH];
#else
	extern struct player_special_data dummy_mob;
	extern struct shared_room_data ocean_shared_data;
	extern char buf[MAX_STRING_LENGTH];
	extern char buf1[MAX_STRING_LENGTH];
	extern char buf2[MAX_STRING_LENGTH];
	extern char arg[MAX_STRING_LENGTH];
#endif
