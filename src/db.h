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
#define LIB_CHANNELS  "channels/"


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
#define DATA_LAST_CONSTRUCTION_ID  5	// for vehicle construct/dismantle
#define DATA_START_PLAYTIME_TRACKING  6	// when we started tracking empire playtime (prevents accidental deletes)
#define NUM_DATAS  7


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
int file_to_string_alloc(const char *name, char **buf);
void boot_db(void);

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
extern struct time_info_data time_info;
extern byte y_coord_to_season[MAP_HEIGHT];


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

// applies
struct apply_data *copy_apply_list(struct apply_data *input);
struct obj_apply *copy_obj_apply_list(struct obj_apply *list);
void parse_apply(FILE *fl, struct apply_data **list, char *error_str);
void write_applies_to_file(FILE *fl, struct apply_data *list);

// archetypes
extern archetype_data *archetype_table;
extern archetype_data *sorted_archetypes;

extern archetype_data *archetype_proto(any_vnum vnum);
void free_archetype(archetype_data *arch);
void free_archetype_gear(struct archetype_gear *list);

// augments
extern augment_data *augment_table;
extern augment_data *sorted_augments;

augment_data *augment_proto(any_vnum vnum);
augment_data *find_augment_by_name(char_data *ch, char *name, int type);
void free_augment(augment_data *aug);

// automessage
extern struct automessage *automessages_table;

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

// buildings
extern bld_data *building_table;

void adjust_building_tech(empire_data *emp, room_data *room, bool add);
bld_data *building_proto(bld_vnum vnum);
void free_building(bld_data *building);

// cities
int city_points_available(empire_data *emp);
int count_city_points_used(empire_data *emp);
struct empire_city_data *create_city_entry(empire_data *emp, char *name, room_data *location, int type);

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

crop_data *crop_proto(crop_vnum vnum);
void free_crop(crop_data *cp);
void schedule_crop_growth(struct map_data *map);
void uncrop_tile(room_data *room);

// custom messages
void parse_custom_message(FILE *fl, struct custom_message **list, char *error);
void write_custom_messages_to_file(FILE *fl, char letter, struct custom_message *list);

// data system
double data_get_double(int key);
int data_get_int(int key);
long data_get_long(int key);
void load_data_table();
double data_set_double(int key, double value);
int data_set_int(int key, int value);
long data_set_long(int key, long value);

// descriptors
extern descriptor_data *descriptor_list;

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
struct empire_island *get_empire_island(empire_data *emp, int island_id);
int get_main_island(empire_data *emp);
empire_data *get_or_create_empire(char_data *ch);
void free_empire(empire_data *emp);
struct empire_homeless_citizen *make_citizen_homeless(empire_data *emp, struct empire_npc_data *npc);
bool member_is_timed_out_ch(char_data *ch);
void read_empire_members(empire_data *only_empire, bool read_techs);
void read_empire_territory(empire_data *emp, bool check_tech);
empire_data *real_empire(empire_vnum vnum);
void reread_empire_tech(empire_data *emp);
void save_empire(empire_data *e, bool save_all_parts);
void save_all_empires();
void sort_trade_data(struct empire_trade_data **list);
void update_empire_members_and_greatness(empire_data *emp);
void update_member_data(char_data *ch);

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

// empire misc
void record_theft_log(empire_data *emp, obj_vnum vnum, int amount);
char_data *spawn_empire_npc_to_room(empire_data *emp, struct empire_npc_data *npc, room_data *room, mob_vnum override_mob);

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
const char *get_generic_name_by_vnum(any_vnum vnum);
const char *get_generic_string_by_vnum(any_vnum vnum, int type, int pos);
extern int get_generic_value_by_vnum(any_vnum vnum, int type, int pos);

// globals
extern struct global_data *globals_table;
void free_global(struct global_data *glb);
extern struct global_data *global_proto(any_vnum vnum);

// help files
extern struct help_index_element *help_table;
extern int top_of_helpt;

void index_boot_help();

// instances
struct instance_data *get_instance_by_id(any_vnum instance_id);

// interactions
void free_interactions(struct interaction_item **list);

// islands
extern struct island_info *island_table;

struct island_info *get_island(int island_id, bool create_if_missing);
struct island_info *get_island_by_coords(char *coords);
struct island_info *get_island_by_name(char_data *ch, char *name);
char *get_island_name_for(int island_id, char_data *for_ch);
bool island_has_default_name(struct island_info *island);
void number_and_count_islands(bool reset);
void save_island_table();

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

void queue_delayed_update(char_data *ch, bitvector_t type);
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
void free_obj_binding(struct obj_binding **list);
obj_data *obj_proto(obj_vnum vnum);
obj_data *read_object(obj_vnum nr, bool with_triggers);

// players
extern struct group_data *group_list;
extern bool pause_affect_total;

void check_autowiz(char_data *ch);
void check_delayed_load(char_data *ch);
void delete_player_character(char_data *ch);
void enter_player_game(descriptor_data *d, int dolog, bool fresh);
int get_highest_access_level(account_data *acct);
char_data *find_player_in_room_by_id(room_data *room, int id);
char_data *is_at_menu(int id);
char_data *is_playing(int id);
void start_new_character(char_data *ch);
int *summarize_weekly_playtime(empire_data *emp);

// player equipment set
int add_eq_set_to_char(char_data *ch, int set_id, char *name);
void add_obj_to_eq_set(obj_data *obj, int set_id, int pos);
int count_eq_sets(char_data *ch);
void free_player_eq_set(struct player_eq_set *eq_set);
struct player_eq_set *get_eq_set_by_id(char_data *ch, int id);
struct player_eq_set *get_eq_set_by_name(char_data *ch, char *name);
struct eq_set_obj *get_obj_eq_set_by_id(obj_data *obj, int id);
void remove_obj_from_eq_set(obj_data *obj, int set_id);

// player lastnames
void change_personal_lastname(char_data *ch, char *name);

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
void free_player_quests(struct player_quest *list);
void free_quest(quest_data *quest);
void free_quest_temp_list(struct quest_temp_list *list);

// resources
struct resource_data *copy_resource_list(struct resource_data *input);
void free_resource_list(struct resource_data *list);
void parse_resource(FILE *fl, struct resource_data **list, char *error_str);
void write_resources_to_file(FILE *fl, char letter, struct resource_data *list);

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

// starting locations / start locs
extern int highest_start_loc_index;
extern room_vnum *start_locs;

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

// trading post
void save_trading_post();

// triggers
extern trig_data *trigger_table;
extern trig_data *trigger_list;
extern trig_data *random_triggers;
extern trig_data *free_trigger_list;

// vehicles
extern vehicle_data *vehicle_list;
extern vehicle_data *vehicle_table;

void adjust_vehicle_tech(vehicle_data *veh, bool add);
void free_vehicle(vehicle_data *veh);
extern vehicle_data *read_vehicle(any_vnum vnum, bool with_triggers);
extern vehicle_data *vehicle_proto(any_vnum vnum);

// wizlock system
extern int wizlock_level;
extern char *wizlock_message;

// world
extern room_data *world_table;
extern room_data *interior_room_list;
extern struct map_data world_map[MAP_WIDTH][MAP_HEIGHT];
extern struct map_data *land_map;

void annual_world_update();
void change_chop_territory(room_data *room);
void check_all_exits();
void check_terrain_height(room_data *room);
void clear_private_owner(int id);
struct room_direction_data *create_exit(room_data *from, room_data *to, int dir, bool back);
room_data *create_room(room_data *home);
void decustomize_room(room_data *room);
room_data *dir_to_room(room_data *room, int dir, bool ignore_entrance);
void delete_room(room_data *room, bool check_exits);
void finish_trench(room_data *room);
void free_complex_data(struct complex_room_data *data);
crop_data *get_potential_crop_for_location(room_data *location, bool must_have_forage);
struct complex_room_data *init_complex_data();
void init_mine(room_data *room, char_data *ch, empire_data *emp);
void output_map_to_file();
void perform_burn_room(room_data *room);
room_data *real_real_room(room_vnum vnum);
room_data *real_room(room_vnum vnum);
void run_external_evolutions();
void save_whole_world();
void sort_world_table();
void stop_burning(room_data *room);
void untrench_room(room_data *room);

// misc
extern char **tips_of_the_day;
extern int tips_of_the_day_size;

void load_intro_screens();

// more frees
void free_apply_list(struct apply_data *list);
void free_obj_apply_list(struct obj_apply *list);
void free_icon_set(struct icon_data **set);
void free_exit_template(struct exit_template *ex);


// act.comm.c
extern bool global_mute_slash_channel_joins;

// statistics.c
extern int max_players_this_uptime;


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
