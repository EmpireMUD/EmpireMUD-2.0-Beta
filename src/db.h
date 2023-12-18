/* ************************************************************************
*   File: db.h                                            EmpireMUD 2.0b5 *
*  Usage: header file for database handling                               *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

// versioning for the binary map file to allow updates and still be backwards-compatible
#define CURRENT_BINARY_MAP_VERSION  1	// used in write_fresh_binary_map_file()
typedef struct map_file_data_v1  map_file_data;	// the current version of the structure, for map_to_store()
#define store_to_map  store_to_map_v1	// used in load_binary_map_file()


// DB_BOOT_x: arbitrary constants used by index_boot() (must be unique) -- these correspond to an array in discrete_load() too
#define DB_BOOT_WLD  0	// not used as of b5.116, except to convert old copies
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
#define LIB_PLAYERS_DELETED  LIB_PLAYERS"deleted/"
#define LIB_OBJPACK  "packs/"
#define LIB_EMPIRE  "empires/"
#define LIB_CHANNELS  "channels/"


// file suffixes for common write files
#define SUF_PACK  ".pack"	// needs .
#define SUF_PLR  "plr"	// does not need .
#define SUF_DELAY  "delay"	// does not need .
#define SUF_MEMORY  "mem"	// does not need .


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
#define EMPIRE_HISTORY_PREFIX  LIB_EMPIRE"history/"	// for empire history
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

// TEXT_FILE_x: Text loaded from files for display in-game
#define TEXT_FILE_CREDITS  0
#define TEXT_FILE_GODLIST  1
#define TEXT_FILE_HANDBOOK  2
#define TEXT_FILE_HELP_SCREEN  3
#define TEXT_FILE_HELP_SCREEN_SCREENREADER  4
#define TEXT_FILE_IMOTD  5
#define TEXT_FILE_INFO  6
#define TEXT_FILE_MOTD  7
#define TEXT_FILE_NEWS  8
#define TEXT_FILE_POLICY  9
#define TEXT_FILE_SHORT_CREDITS  10
#define TEXT_FILE_WIZLIST  11
#define NUM_TEXT_FILE_STRINGS  12	// total

// misc files (user-modifiable libs)
#define CONFIG_FILE  LIB_MISC"game_configs"  // config.c system
#define DATA_FILE  LIB_MISC"game_data"	// data.c system
#define IDEA_FILE  LIB_MISC"ideas"	// for the 'idea'-command
#define TYPO_FILE  LIB_MISC"typos"	//         'typo'
#define BUG_FILE  LIB_MISC"bugs"	//         'bug'
#define ATTACK_MESSAGES_FILE  LIB_MISC"messages"	// damage messages
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

// additional files
#define BINARY_MAP_FILE  WLD_PREFIX"binary_map"	// all the map data
#define BINARY_WORLD_INDEX  WLD_PREFIX"binary_world_index"	// index of rooms
#define EVOLUTION_FILE  LIB_WORLD"evolutions"	// file generated by the 'evolve' utility

// used for many file reads:
#define READ_SIZE 256


// DATA_x: stored data system
#define DATA_DAILY_CYCLE  0	// timestamp of last day-reset (bonus exp, etc)
#define DATA_LAST_NEW_YEAR  1	// timestamp of last annual world update
#define DATA_WORLD_START  2	// timestamp of when the mud first started up
#define DATA_MAX_PLAYERS_TODAY  3	// players logged in today
#define DATA_LAST_ISLAND_DESCS  4	// last time island descs regenerated
#define DATA_LAST_CONSTRUCTION_ID  5	// for vehicle construct/dismantle
#define DATA_START_PLAYTIME_TRACKING  6	// when we started tracking empire playtime (prevents accidental deletes)
#define DATA_TOP_IDNUM  7	// highest player idnum to prevent re-use
#define NUM_DATAS  8


// DATYPE_x: types of stored data
#define DATYPE_INT  0
#define DATYPE_LONG  1
#define DATYPE_DOUBLE  2


// WSAVE_x: types of saves for world_save_requests
#define WSAVE_MAP  BIT(0)	// save flat map data like sector info (and base_sect)
#define WSAVE_ROOM  BIT(1)	// save room data
#define WSAVE_OBJS_AND_VEHS  BIT(2)	// save obj pack / veh pack


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


// delayed-saving system for world info
struct world_save_request_data {
	room_vnum vnum;
	int save_type;	// WSAVE_ bits
	UT_hash_handle hh;
};


// public procedures in db.*.c
void boot_db(void);
void discrete_load(FILE *fl, int mode, char *filename);
int file_to_string_alloc(const char *name, char **buf);
char *fread_string(FILE *fl, char *error);
void index_boot(int mode);

// global saves
void save_index(int type);
void save_library_file_for_vnum(int type, any_vnum vnum);

// world processors
void change_base_sector(room_data *room, sector_data *sect);
void change_terrain(room_data *room, sector_vnum sect, sector_vnum base_sect);
void construct_building(room_data *room, bld_vnum type);
void disassociate_building(room_data *room);
void set_crop_type(room_data *room, crop_data *cp);


// various externs
extern int Global_ignore_dark;
extern struct time_info_data main_time_info;
extern byte y_coord_to_season[MAP_HEIGHT];


// abilities
extern ability_data *ability_table;
extern ability_data *sorted_abilities;

ability_data *find_ability(char *argument);
ability_data *find_ability_by_name_exact(char *name, bool allow_abbrev);
#define find_ability_by_name(name)  find_ability_by_name_exact(name, TRUE)
ability_data *find_ability_by_vnum(any_vnum vnum);
#define ability_proto  find_ability_by_vnum	// why don't all the types have "type_proto()" functions?
void free_ability(ability_data *abil);
char *get_ability_name_by_vnum(any_vnum vnum);
void remove_ability_from_table(ability_data *abil);

// accounts
void add_player_to_account(char_data *ch, account_data *acct);
account_data *create_account_for_player(char_data *ch);
account_data *find_account(int id);
void free_account(account_data *acct);
void remove_player_from_account(char_data *ch);

// adventures
extern adv_data *adventure_table;

void add_adventure_to_table(adv_data *adv);
adv_data *adventure_proto(adv_vnum vnum);
void free_adventure(adv_data *adv);
void init_adventure(adv_data *adv);
void parse_link_rule(FILE *fl, struct adventure_link_rule **list, char *error_part);
void remove_adventure_from_table(adv_data *adv);
int sort_adventures(adv_data *a, adv_data *b);
void write_linking_rules_to_file(FILE *fl, char letter, struct adventure_link_rule *list);

// applies
struct apply_data *copy_apply_list(struct apply_data *input);
struct obj_apply *copy_obj_apply_list(struct obj_apply *list);
void parse_apply(FILE *fl, struct apply_data **list, char *error_str);
void write_applies_to_file(FILE *fl, struct apply_data *list);

// archetypes
extern archetype_data *archetype_table;
extern archetype_data *sorted_archetypes;

archetype_data *archetype_proto(any_vnum vnum);
void free_archetype(archetype_data *arch);
void free_archetype_gear(struct archetype_gear *list);
void parse_archetype_gear(FILE *fl, struct archetype_gear **list, char *error);
void remove_archetype_from_table(archetype_data *arch);
void write_archetype_gear_to_file(FILE *fl, struct archetype_gear *list);

// augments
extern augment_data *augment_table;
extern augment_data *sorted_augments;

augment_data *augment_proto(any_vnum vnum);
augment_data *find_augment_by_name(char_data *ch, char *name, int type);
void free_augment(augment_data *aug);
void remove_augment_from_table(augment_data *aug);

// automessage
extern struct automessage *automessages_table;

void display_automessages_on_login(char_data *ch);
void free_automessage(struct automessage *msg);
int new_automessage_id();
void save_automessages();
int sort_automessage_by_data(struct automessage *a, struct automessage *b);

// books
extern book_data *book_table;
extern struct author_data *author_table;
extern book_vnum top_book_vnum;

void add_book_to_table(book_data *book);
obj_data *create_book_obj(book_data *book);
void free_book(book_data *book);
void remove_book_from_table(book_data *book);

// buildings
extern bld_data *building_table;

void add_building_to_table(bld_data *bld);
void adjust_building_tech(empire_data *emp, room_data *room, bool add);
bld_data *building_proto(bld_vnum vnum);
void check_for_bad_buildings();
void free_bld_relations(struct bld_relation *list);
void free_building(bld_data *building);
void init_building(bld_data *building);
void remove_building_from_table(bld_data *bld);
int sort_buildings(bld_data *a, bld_data *b);

// cities
int city_points_available(empire_data *emp);
int count_cities(empire_data *emp);
int count_city_points_used(empire_data *emp);
struct empire_city_data *create_city_entry(empire_data *emp, char *name, room_data *location, int type);
void write_city_data_file();

// classes
extern class_data *class_table;
extern class_data *sorted_classes;

class_data *find_class(char *argument);
class_data *find_class_by_name(char *name);
class_data *find_class_by_vnum(any_vnum vnum);
void free_class(class_data *cls);
void remove_class_from_table(class_data *cls);

// communication
extern struct slash_channel *slash_channel_list;

// configs
extern struct config_type *config_table;

void free_config_type(struct config_type *cnf);
void save_config_system();

// crafts
extern craft_data *craft_table;
extern craft_data *sorted_crafts;

void add_craft_to_table(craft_data *craft);
craft_data *craft_proto(craft_vnum vnum);
void init_craft(craft_data *craft);
void free_craft(craft_data *craft);
void remove_craft_from_table(craft_data *craft);
int sort_crafts_by_data(craft_data *a, craft_data *b);
int sort_crafts_by_vnum(craft_data *a, craft_data *b);

// crops
extern crop_data *crop_table;

void add_crop_to_table(crop_data *crop);
crop_data *crop_proto(crop_vnum vnum);
void free_crop(crop_data *cp);
void init_crop(crop_data *cp);
void remove_crop_from_table(crop_data *crop);
void schedule_crop_growth(struct map_data *map);
int sort_crops(crop_data *a, crop_data *b);
void uncrop_tile(room_data *room);

// custom messages
void parse_custom_message(FILE *fl, struct custom_message **list, char *error);
void write_custom_messages_to_file(FILE *fl, char letter, struct custom_message *list);

// data system
extern struct stored_data *data_table;

double data_get_double(int key);
int data_get_int(int key);
long data_get_long(int key);
void load_data_table();
double data_set_double(int key, double value);
int data_set_int(int key, int value);
long data_set_long(int key, long value);

// descriptors
extern struct txt_block *bufpool;
extern descriptor_data *descriptor_list;

void free_descriptor(descriptor_data *desc);
bool has_anonymous_host(descriptor_data *desc);

// empires
extern empire_data *empire_table;
extern struct trading_post_data *trading_list;
extern bool check_empire_refresh;

void check_tavern_setup(room_data *room);
struct empire_territory_data *create_territory_entry(empire_data *emp, room_data *room);
void delete_empire(empire_data *emp);
void delete_member_data(char_data *ch, empire_data *from_emp);
void delete_territory_npc(struct empire_territory_data *ter, struct empire_npc_data *npc);
void delete_room_npcs(room_data *room, struct empire_territory_data *ter, bool make_homeless);
bool extract_tavern_resources(room_data *room);
void free_dropped_items(struct empire_dropped_item **list);
void free_empire(empire_data *emp);
void free_member_data(empire_data *emp);
struct empire_island *get_empire_island(empire_data *emp, int island_id);
int get_main_island(empire_data *emp);
empire_data *get_or_create_empire(char_data *ch);
struct empire_homeless_citizen *make_citizen_homeless(empire_data *emp, struct empire_npc_data *npc);
bool member_is_timed_out_ch(char_data *ch);
void read_empire_members(empire_data *only_empire, bool read_techs);
void read_empire_territory(empire_data *emp, bool check_tech);
empire_data *real_empire(empire_vnum vnum);
void reread_empire_tech(empire_data *emp);
void save_empire(empire_data *e, bool save_all_parts);
void save_all_empires();
void save_marked_empires();
int sort_empires(empire_data *a, empire_data *b);
int sort_trade_data(struct empire_trade_data *a, struct empire_trade_data *b);
void update_empire_members_and_greatness(empire_data *emp);
void update_member_data(char_data *ch);

// empire offenses
void add_offense(empire_data *emp, int type, char_data *offender, room_data *loc, bitvector_t flags);
int avenge_offenses_from_empire(empire_data *emp, empire_data *foe);
int avenge_solo_offenses_from_player(empire_data *emp, char_data *foe);
void clean_empire_offenses();
int get_total_offenses_from_empire(empire_data *emp, empire_data *foe);
int get_total_offenses_from_char(empire_data *emp, char_data *ch);
bool offense_was_seen(char_data *ch, empire_data *emp, room_data *from_room);
void remove_offense(empire_data *emp, struct offense_data *off);
void remove_recent_offenses(empire_data *emp, int type, char_data *offender);

// empire misc
void ewt_free_tracker(struct empire_workforce_tracker **tracker);
void record_theft_log(empire_data *emp, obj_vnum vnum, int amount);
char_data *spawn_empire_npc_to_room(empire_data *emp, struct empire_npc_data *npc, room_data *room, mob_vnum override_mob);

// empire territory
void delete_territory_entry(empire_data *emp, struct empire_territory_data *ter, bool make_npcs_homeless);
void populate_npc(room_data *room, struct empire_territory_data *ter, bool force);

// extra descs
void free_extra_descs(struct extra_descr_data **list);
void parse_extra_desc(FILE *fl, struct extra_descr_data **list, char *error_part);
void prune_extra_descs(struct extra_descr_data **list);
void write_extra_descs_to_file(FILE *fl, char key, struct extra_descr_data *list);

// events
extern event_data *event_table;
extern int top_event_id;
extern struct event_running_data *running_events;
extern bool events_need_save;

struct player_event_data *create_event_data(char_data *ch, int event_id, any_vnum event_vnum);
event_data *find_event_by_vnum(any_vnum vnum);
struct event_running_data *find_last_event_run_by_vnum(any_vnum event_vnum);
struct event_running_data *find_running_event_by_id(int id);
struct event_running_data *find_running_event_by_vnum(any_vnum event_vnum);
void free_event(event_data *event);
void free_event_leaderboard(struct event_leaderboard *hash);
char *get_event_name_by_proto(any_vnum vnum);
void remove_event_from_table(event_data *event);

// factions
extern faction_data *faction_table;
extern int MAX_REPUTATION;
extern int MIN_REPUTATION;
extern faction_data *sorted_factions;

faction_data *find_faction(char *argument);
faction_data *find_faction_by_name(char *name);
faction_data *find_faction_by_vnum(any_vnum vnum);
void free_faction(faction_data *fct);
void remove_faction_from_table(faction_data *fct);

// fight messages
extern attack_message_data *attack_message_table;

int get_attack_damage_type_by_vnum(any_vnum vnum);
const char *get_attack_first_person_by_vnum(any_vnum vnum);
const char *get_attack_name_by_vnum(any_vnum vnum);
const char *get_attack_noun_by_vnum(any_vnum vnum);
const char *get_attack_third_person_by_vnum(any_vnum vnum);
int get_attack_weapon_type_by_vnum(any_vnum vnum);
bool is_attack_flagged_by_vnum(any_vnum vnum, bitvector_t amdf_flags);

void add_attack_message(attack_message_data *add_to, struct attack_message_set *messages);
attack_message_data *create_attack_message(any_vnum vnum);
struct attack_message_set *create_attack_message_entry(bool duplicate_strings, char *die_to_attacker, char *die_to_victim, char *die_to_room, char *miss_to_attacker, char *miss_to_victim, char *miss_to_room, char *hit_to_attacker, char *hit_to_victim, char *hit_to_room, char *god_to_attacker, char *god_to_victim, char *god_to_room);
attack_message_data *find_attack_message(any_vnum a_type, bool create_if_missing);
attack_message_data *find_attack_message_by_name_or_vnum(char *name, bool exact);
void free_attack_message(attack_message_data *amd);
struct attack_message_set *get_one_attack_message(attack_message_data *amd, int num);
void load_fight_messages();
attack_message_data *real_attack_message(any_vnum vnum);

// generics
extern generic_data *generic_table;
extern generic_data *sorted_generics;

void free_generic(generic_data *gen);
generic_data *find_generic(any_vnum vnum, int type);
generic_data *find_generic_by_name(int type, char *name, bool exact);
generic_data *find_generic_no_spaces(int type, char *name);
generic_data *real_generic(any_vnum vnum);
const char *get_generic_name_by_vnum(any_vnum vnum);
const char *get_generic_string_by_vnum(any_vnum vnum, int type, int pos);
int get_generic_value_by_vnum(any_vnum vnum, int type, int pos);
void remove_generic_from_table(generic_data *gen);

// globals
extern struct global_data *globals_table;

void add_global_to_table(struct global_data *glb);
void clear_global(struct global_data *glb);
void free_global(struct global_data *glb);
struct global_data *global_proto(any_vnum vnum);
void remove_global_from_table(struct global_data *glb);
int sort_globals(struct global_data *a, struct global_data *b);

// help files
extern struct help_index_element *help_table;
extern int top_of_helpt;

int help_sort(const void *a, const void *b);
void index_boot_help();

// icons
void write_icons_to_file(FILE *fl, char file_tag, struct icon_data *list);

// instances
extern struct instance_data *instance_list;
extern bool instance_save_wait;
extern struct instance_data *quest_instance_global;
extern bool need_instance_save;

void free_instance(struct instance_data *inst);
struct instance_data *get_instance_by_id(any_vnum instance_id);

// interactions
void free_interactions(struct interaction_item **list);
void free_interaction_restrictions(struct interact_restriction **list);
void parse_interaction(char *line, struct interaction_item **list, char *error_part);
void write_interactions_to_file(FILE *fl, struct interaction_item *list);

// islands
extern struct island_info *island_table;

void check_island_levels(room_data *location, int level);
struct island_info *get_island(int island_id, bool create_if_missing);
struct island_info *get_island_by_coords(char *coords);
struct island_info *get_island_by_name(char_data *ch, char *name);
char *get_island_name_for_empire(int island_id, empire_data *for_emp);
#define get_island_name_for(island_id, for_ch)  get_island_name_for_empire((island_id), (for_ch ? GET_LOYALTY(for_ch) : NULL))
bool island_has_default_name(struct island_info *island);
void number_and_count_islands(bool reset);
void save_island_table();
void update_island_names();

// map memory
#define MAP_MEM_ICON  0
#define MAP_MEM_NAME  1

void add_player_map_memory(char_data *ch, room_vnum vnum, char *icon, char *name, time_t use_timestamp);
void delete_player_map_memory(struct player_map_memory *memory, char_data *ch);
const char *get_player_map_memory(char_data *ch, room_vnum vnum, int type);
void load_map_memory(char_data *ch);

// mobiles/chars
extern account_data *account_table;
extern char_data *character_list;
extern char_data *combat_list;
extern char_data *global_next_player;
extern char_data *next_combat_list;
extern char_data *next_combat_list_main;
extern char_data *mobile_table;
extern char_data *player_character_list;
extern player_index_data *player_table_by_idnum;
extern player_index_data *player_table_by_name;

int set_current_pool(char_data *ch, int type, int amount);
#define set_health(ch, amount)  set_current_pool(ch, HEALTH, amount)
#define set_move(ch, amount)  set_current_pool(ch, MOVE, amount)
#define set_mana(ch, amount)  set_current_pool(ch, MANA, amount)
#define set_blood(ch, amount)  set_current_pool(ch, BLOOD, amount)

void add_mobile_to_table(char_data *mob);
player_index_data *find_player_index_by_idnum(int idnum);
player_index_data *find_player_index_by_name(char *name);
void init_player(char_data *ch);
char_data *read_mobile(mob_vnum nr, bool with_triggers);
char_data *mob_proto(mob_vnum vnum);
void clear_char(char_data *ch);
void init_player_specials(char_data *ch);
int pick_generic_name(int name_set, int sex);
void remove_mobile_from_table(char_data *mob);
void reset_char(char_data *ch);
void free_char(char_data *ch);
void set_title(char_data *ch, char *title);
int sort_mobiles(char_data *a, char_data *b);

void save_char(char_data *ch, room_data *load_room);
#define SAVE_CHAR(ch)  save_char((ch), (IN_ROOM(ch) ? IN_ROOM(ch) : (GET_LOADROOM(ch) != NOWHERE ? real_room(GET_LOADROOM(ch)) : NULL)))

void clear_delayed_update(char_data *ch);
void queue_delayed_update(char_data *ch, bitvector_t type);
void update_player_index(player_index_data *index, char_data *ch);
char_data *find_or_load_player(char *name, bool *is_file);
void store_loaded_char(char_data *ch);
char_data *load_player(char *name, bool normal);

// morphs
extern morph_data *morph_table;
extern morph_data *sorted_morphs;

morph_data *morph_proto(any_vnum vnum);
void free_morph(morph_data *morph);
void remove_morph_from_table(morph_data *morph);

// objects
extern obj_data *object_list;
extern obj_data *object_table;
extern obj_data *purge_bound_items_next;
extern obj_data *global_next_obj;
extern bool suspend_autostore_updates;
extern bool add_chaos_to_obj_timers;

void add_object_to_table(obj_data *obj);
obj_data *create_obj(void);
struct obj_proto_data *create_obj_proto_data();
void clear_object(obj_data *obj);
void free_obj(obj_data *obj);
void free_obj_binding(struct obj_binding **list);
void free_obj_proto_data(struct obj_proto_data *data);
obj_data *obj_proto(obj_vnum vnum);
obj_data *read_object(obj_vnum nr, bool with_triggers);
void remove_object_from_table(obj_data *obj);
void set_obj_keywords(obj_data *obj, const char *str);
void set_obj_long_desc(obj_data *obj, const char *str);
void set_obj_look_desc(obj_data *obj, const char *str, bool format);
void set_obj_look_desc_append(obj_data *obj, const char *str, bool format);
void set_obj_short_desc(obj_data *obj, const char *str);
int sort_objects(obj_data *a, obj_data *b);

// objsave
void Crash_save(obj_data *obj, FILE *fp, int location);
void Crash_save_one_obj_to_file(FILE *fl, obj_data *obj, int location);
void loaded_obj_to_char(obj_data *obj, char_data *ch, int location, obj_data ***cont_row);
obj_data *Obj_load_from_file(FILE *fl, obj_vnum vnum, int *location, char_data *notify, char *error_str);
void objpack_load_room(room_data *room, bool use_pre_b5_116_dir);

// players
extern struct over_time_effect_type *free_dots_list;
extern struct channel_history_data *global_channel_history[NUM_GLOBAL_HISTORIES];
extern struct player_quest *global_next_player_quest, *global_next_player_quest_2;
extern const char *global_history_files[];
extern struct group_data *group_list;
extern struct int_hash *inherent_ptech_hash;
extern int max_inventory_size;
extern bool pause_affect_total;

void add_language(char_data *ch, any_vnum vnum, byte level);
void add_language_empire(empire_data *emp, any_vnum vnum, byte level);
void add_lastname(char_data *ch, char *name);
bool add_player_to_table(player_index_data *plr);
void check_autowiz(char_data *ch);
bool check_bonus_trait_reset(char_data *ch);
void check_delayed_load(char_data *ch);
void check_languages(char_data *ch);
void check_languages_all(void);
void check_languages_all_empires(void);
void check_languages_empire(empire_data *emp);
void convert_and_schedule_player_affects(char_data *ch);
void delete_player_character(char_data *ch);
void enter_player_game(descriptor_data *d, int dolog, bool fresh);
room_data *find_home(char_data *ch);
char_data *find_player_in_room_by_id(room_data *room, int id);
void free_alias(struct alias_data *a);
void free_companion(struct companion_data *cd);
void free_loaded_players();
void free_mail(struct mail_data *mail);
void free_player_completed_quests(struct player_completed_quest **hash);
void free_player_event_data(struct player_event_data *hash);
void free_player_index_data(player_index_data *index);
int get_highest_access_level(account_data *acct);
char_data *is_at_menu(int id);
char_data *is_playing(int id);
bool member_is_timed_out_index(player_index_data *index);
struct mail_data *parse_mail(FILE *fl, char *first_line);
void remove_lastname(char_data *ch, char *name);
void remove_player_from_table(player_index_data *plr);
void save_all_players(bool delay);
int speaks_language(char_data *ch, any_vnum vnum);
int speaks_language_empire(empire_data *emp, any_vnum vnum);
void start_new_character(char_data *ch);
int *summarize_weekly_playtime(empire_data *emp);
void write_mail_to_file(FILE *fl, char_data *ch);

// player equipment set
int add_eq_set_to_char(char_data *ch, int set_id, char *name);
void add_obj_to_eq_set(obj_data *obj, int set_id, int pos);
void clear_obj_eq_sets(obj_data *obj);
int count_eq_sets(char_data *ch);
void free_obj_eq_set(struct eq_set_obj *eq_set);
void free_player_eq_set(struct player_eq_set *eq_set);
struct player_eq_set *get_eq_set_by_id(char_data *ch, int id);
struct player_eq_set *get_eq_set_by_name(char_data *ch, char *name);
struct eq_set_obj *get_obj_eq_set_by_id(obj_data *obj, int id);
void remove_obj_from_eq_set(obj_data *obj, int set_id);

// player lastnames
void change_personal_lastname(char_data *ch, char *name);
bool has_lastname(char_data *ch, char *name);

// progress
extern progress_data *progress_table;
extern progress_data *sorted_progress;
extern bool need_progress_refresh;

void free_empire_completed_goals(struct empire_completed_goal *hash);
void free_empire_goals(struct empire_goal *hash);
void free_progress(progress_data *prg);
char *get_progress_name_by_proto(any_vnum vnum);
progress_data *real_progress(any_vnum vnum);
void remove_progress_from_table(progress_data *prg);

// quests
extern struct quest_data *quest_table;

struct quest_giver *copy_quest_givers(struct quest_giver *from);
void free_player_quests(struct player_quest *list);
void free_quest(quest_data *quest);
void free_quest_givers(struct quest_giver *list);
void free_quest_lookups(struct quest_lookup *list);
void free_quest_temp_list(struct quest_temp_list *list);
void parse_quest_giver(FILE *fl, struct quest_giver **list, char *error_str);
quest_data *quest_proto(any_vnum vnum);
void remove_quest_from_table(quest_data *quest);
void write_quest_givers_to_file(FILE *fl, char letter, struct quest_giver *list);

// requirements
void parse_requirement(FILE *fl, struct req_data **list, bool with_custom_text, char *error_str);
int sort_requirements_by_group(struct req_data *a, struct req_data *b);
void write_requirements_to_file(FILE *fl, char letter, struct req_data *list);

// resources
struct resource_data *copy_resource_list(struct resource_data *input);
void free_resource_list(struct resource_data *list);
void parse_resource(FILE *fl, struct resource_data **list, char *error_str);
void write_resources_to_file(FILE *fl, char letter, struct resource_data *list);

// room templates
extern room_template *room_template_table;

void add_room_template_to_table(room_template *rmt);
void free_room_template(room_template *rmt);
void init_room_template(room_template *rmt);
void remove_room_template_from_table(room_template *rmt);
room_template *room_template_proto(rmt_vnum vnum);
int sort_room_templates(room_template *a, room_template *b);
bool valid_room_template_vnum(rmt_vnum vnum);

// sectors
extern sector_data *sector_table;
extern struct sector_index_type *sector_index;

void add_sector_to_table(sector_data *sect);
struct sector_index_type *find_sector_index(sector_vnum vnum);
void free_sector(struct sector_data *st);
void init_sector(sector_data *st);
void perform_change_base_sect(room_data *loc, struct map_data *map, sector_data *sect);
void perform_change_sect(room_data *loc, struct map_data *map, sector_data *sect);
void remove_sector_from_table(sector_data *sect);
sector_data *sector_proto(sector_vnum vnum);
int sort_sectors(void *a, void *b);

// shops
extern shop_data *shop_table;

void free_shop(shop_data *shop);
void free_shop_lookups(struct shop_lookup *list);
shop_data *real_shop(any_vnum vnum);
void remove_shop_from_table(shop_data *shop);

// skills
extern skill_data *skill_table;
extern skill_data *sorted_skills;

skill_data *find_skill(char *argument);
skill_data *find_skill_by_name_exact(char *name, bool allow_abbrev);
#define find_skill_by_name(name)  find_skill_by_name_exact(name, TRUE)
skill_data *find_skill_by_vnum(any_vnum vnum);
void free_skill(skill_data *skill);
char *get_skill_abbrev_by_vnum(any_vnum vnum);
char *get_skill_name_by_vnum(any_vnum vnum);
void remove_skill_from_table(skill_data *skill);

// socials
extern social_data *social_table;
extern social_data *sorted_socials;

social_data *social_proto(any_vnum vnum);
void free_social(social_data *soc);
void remove_social_from_table(social_data *soc);

// starting locations / start locs
extern int highest_start_loc_index;
extern room_vnum *start_locs;

// statistics
extern double empire_score_average[NUM_SCORES];

// stored event libs
void add_stored_event(struct stored_event **list, int type, struct dg_event *event);
void cancel_all_stored_events(struct stored_event **list);
void cancel_stored_event(struct stored_event **list, int type);
void delete_stored_event(struct stored_event **list, int type);
struct stored_event *find_stored_event(struct stored_event *list, int type);

// stored event helpers
#define add_stored_event_room(room, type, ev)  add_stored_event(&SHARED_DATA(room)->events, type, ev)
#define cancel_all_stored_events_room(room)  cancel_all_stored_events(&SHARED_DATA(room)->events)
#define cancel_stored_event_room(room, type)  cancel_stored_event(&SHARED_DATA(room)->events, type)
#define delete_stored_event_room(room, type)  delete_stored_event(&SHARED_DATA(room)->events, type)
#define find_stored_event_room(room, type)  find_stored_event(SHARED_DATA(room)->events, type)

// time
void reset_time(void);

// trading post
void save_trading_post();

// triggers
extern trig_data *trigger_table;
extern trig_data *trigger_list;
extern trig_data *random_triggers;
extern trig_data *free_trigger_list;
extern trig_data *stc_next_random_trig;
extern struct dg_owner_purged_tracker_type *dg_owner_purged_tracker;
extern struct uid_lookup_table *master_uid_lookup_table;

void add_trigger_to_table(trig_data *trig);
void remove_trigger_from_table(trig_data *trig);
int sort_triggers(trig_data *a, trig_data *b);
void write_trig_protos_to_file(FILE *fl, char letter, struct trig_proto_list *list);

// vehicles
extern vehicle_data *vehicle_list;
extern vehicle_data *vehicle_table;
extern vehicle_data *global_next_vehicle;
extern vehicle_data *next_pending_vehicle;

void adjust_vehicle_tech(vehicle_data *veh, bool add);
void free_vehicle(vehicle_data *veh);
vehicle_data *read_vehicle(any_vnum vnum, bool with_triggers);
void remove_vehicle_from_table(vehicle_data *veh);
void set_vehicle_icon(vehicle_data *veh, const char *str);
void set_vehicle_keywords(vehicle_data *veh, const char *str);
void set_vehicle_long_desc(vehicle_data *veh, const char *str);
void set_vehicle_look_desc(vehicle_data *veh, const char *str, bool format);
void set_vehicle_short_desc(vehicle_data *veh, const char *str);
void set_vehicle_look_desc_append(vehicle_data *veh, const char *str, bool format);
vehicle_data *vehicle_proto(any_vnum vnum);

// wizlock system
extern int wizlock_level;
extern char *wizlock_message;

// world
extern room_data *world_table;
extern room_data *interior_room_list;
extern struct map_data world_map[MAP_WIDTH][MAP_HEIGHT];
extern struct map_data *land_map;
extern int size_of_world;
extern struct vnum_hash *mapout_update_requests;
extern struct world_save_request_data *world_save_requests;
extern struct vnum_hash *binary_world_index_updates;
extern char *world_index_data;
extern int top_of_world_index;
extern bool save_world_after_startup;
extern bool converted_to_b5_116;
extern bool block_world_save_requests;

void add_room_to_world_tables(room_data *room);
void add_trd_home_room(room_vnum vnum, room_vnum home_room);
void add_trd_owner(room_vnum vnum, empire_vnum owner);
void annual_update_depletions(struct depletion_data **list);
void annual_update_map_tile(struct map_data *tile);
void annual_world_update();
void cancel_world_save_request(room_vnum room, int only_save_type);
void change_chop_territory(room_data *room);
void set_natural_sector(struct map_data *map, sector_data *sect);
void set_private_owner(room_data *room, int idnum);
void set_room_custom_description(room_data *room, char *desc);
void set_room_custom_icon(room_data *room, char *icon);
void set_room_custom_name(room_data *room, char *name);
void set_room_height(room_data *room, int height);
void check_all_exits();
void check_terrain_height(room_data *room);
void clear_private_owner(int id);
struct room_direction_data *create_exit(room_data *from, room_data *to, int dir, bool back);
room_data *create_room(room_data *home);
void decustomize_room(room_data *room);
void decustomize_shared_data(struct shared_room_data *shared);
room_data *dir_to_room(room_data *room, int dir, bool ignore_entrance);
void delete_room(room_data *room, bool check_exits);
room_vnum find_free_vnum();
void finish_trench(room_data *room);
void free_complex_data(struct complex_room_data *data);
void free_shared_room_data(struct shared_room_data *data);
room_data *get_extraction_room();
crop_data *get_potential_crop_for_location(room_data *location, bool must_have_forage);
#define get_world_filename(str, vnum, suffix)  sprintf(str, "%s%02d/%d%s", WLD_PREFIX, ((vnum) % 100), (vnum), NULLSAFE(suffix))
struct complex_room_data *init_complex_data();
void init_mine(room_data *room, char_data *ch, empire_data *emp);
room_data *load_map_room(room_vnum vnum, bool schedule_unload);
FILE *open_world_file(int block);
void parse_other_shared_data(struct shared_room_data *shared, char *line, char *error_part);
void perform_burn_room(room_data *room, int evo_type);
room_data *real_real_room(room_vnum vnum);
room_data *real_room(room_vnum vnum);
void remove_room_from_world_tables(room_data *room);
void request_world_save(room_vnum vnum, int save_type);
void request_world_save_by_script(void *go, int type);
void ruin_one_building(room_data *room);
void run_external_evolutions();
void save_and_close_world_file(FILE *fl, int block);
void schedule_burn_down(room_data *room);
void schedule_check_unload(room_data *room, bool offset);
void schedule_trench_fill(struct map_data *map);
void set_burn_down_time(room_data *room, time_t when, bool schedule_burn);
void set_room_damage(room_data *room, double damage_amount);
void setup_start_locations();
int sort_exits(struct room_direction_data *a, struct room_direction_data *b);
void start_burning(room_data *room);
void stop_burning(room_data *room);
void update_world_index(room_vnum vnum, char value);
void untrench_room(room_data *room);
void write_all_wld_files();
void write_fresh_binary_map_file();
void write_one_tile_to_binary_map_file(struct map_data *map);
void write_whole_binary_world_index();

// binary map readers
void store_to_map_v1(struct map_file_data_v1 *store, struct map_data *map);


// misc
extern struct ban_list_element *ban_list;
extern time_t boot_time;
extern struct char_delayed_update *char_delayed_update_list;
extern char **detected_slow_ips;
extern int num_slow_ips;
extern struct generic_name_data *generic_names;
extern struct stats_data_struct *global_sector_count;
extern struct stats_data_struct *global_crop_count;
extern struct stats_data_struct *global_building_count;
extern char **intro_screens;
extern int num_intro_screens;
extern char **tips_of_the_day;
extern int tips_of_the_day_size;

void load_intro_screens();
void reload_text_string(int type);

// more frees
void free_apply_list(struct apply_data *list);
void free_obj_apply_list(struct obj_apply *list);
void free_icon_set(struct icon_data **set);
void free_exit_template(struct exit_template *ex);


// act.comm.c
extern bool global_mute_slash_channel_joins;

// olc.object.c
int set_obj_val(obj_data *obj, int pos, int value);

// statistics.c
extern int max_players_this_uptime;

// text file strings
extern char *text_file_strings[NUM_TEXT_FILE_STRINGS];

// workforce.c
extern struct empire_territory_data *global_next_territory_entry;


/* global buffering system */
#ifdef __DB_C__
	char buf[MAX_STRING_LENGTH];
	char buf1[MAX_STRING_LENGTH];
	char buf2[MAX_STRING_LENGTH];
	char arg[MAX_STRING_LENGTH];
#else
	extern struct shared_room_data ocean_shared_data;
	extern char buf[MAX_STRING_LENGTH];
	extern char buf1[MAX_STRING_LENGTH];
	extern char buf2[MAX_STRING_LENGTH];
	extern char arg[MAX_STRING_LENGTH];
#endif


// quick setter functions

// triggers a player or room save (savable character traits changed)
#define request_char_save_in_world(ch)  do {	\
	if (IN_ROOM(ch)) {	\
		if (MOB_SAVES_TO_ROOM(ch)) {	\
			request_world_save(GET_ROOM_VNUM(IN_ROOM(ch)), WSAVE_ROOM);	\
		}	\
		else if (!IS_NPC(ch)) {	\
			queue_delayed_update(ch, CDU_SAVE);	\
		}	\
	}	\
} while(0)

// triggers a room-pack save (savable obj traits changed)
#define request_obj_save_in_world(obj)  do {	\
	room_data *_room;	\
	if ((_room = find_room_obj_saves_in(obj))) {	\
		request_world_save(GET_ROOM_VNUM(_room), WSAVE_OBJS_AND_VEHS);	\
	}	\
	if ((obj)->carried_by && !IS_NPC((obj)->carried_by)) {	\
		queue_delayed_update((obj)->carried_by, CDU_SAVE);	\
	}	\
	else if ((obj)->worn_by && !IS_NPC((obj)->worn_by)) {	\
		queue_delayed_update((obj)->worn_by, CDU_SAVE);	\
	}	\
} while(0)

// triggers a room-pack save (savable vehicle traits changed)
#define request_vehicle_save_in_world(veh)  do {	\
	if (IN_ROOM(veh)) {	\
		request_world_save(GET_ROOM_VNUM(IN_ROOM(veh)), WSAVE_OBJS_AND_VEHS);	\
	}	\
} while(0)

// combine setting mob flags with saving
// note: adding SPAWNED will always reset spawn time and schedule the despawn here
#define set_mob_flags(mob, to_set)  do { 	\
	if (IS_NPC(mob)) {						\
		SET_BIT(MOB_FLAGS(mob), (to_set));	\
		check_scheduled_events_mob(mob);	\
		request_char_save_in_world(mob);	\
		if (IS_SET((to_set), MOB_SPAWNED)) {	\
			set_mob_spawn_time((mob), time(0));	\
		}									\
	}										\
} while (0)

// combine removing these with saving
// note: removing TIED will always reset spawn time and schedule the despawn here
#define remove_mob_flags(mob, to_set)  do { \
	if (IS_NPC(mob)) {						\
		REMOVE_BIT(MOB_FLAGS(mob), (to_set));	\
		check_scheduled_events_mob(mob);	\
		request_char_save_in_world(mob);	\
		if (IS_SET((to_set), MOB_TIED)) {	\
			set_mob_spawn_time((mob), time(0));	\
		}									\
	}										\
} while (0)


// combine setting these with saving
#define set_vehicle_flags(veh, to_set)  do { \
	SET_BIT(VEH_FLAGS(veh), (to_set));	\
	request_vehicle_save_in_world(veh);	\
} while (0)

// combine removing these with saving
#define remove_vehicle_flags(veh, to_set)  do { \
	REMOVE_BIT(VEH_FLAGS(veh), (to_set));	\
	request_vehicle_save_in_world(veh);	\
} while (0)
