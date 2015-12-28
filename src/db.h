/* ************************************************************************
*   File: db.h                                            EmpireMUD 2.0b3 *
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
#define NUM_DB_BOOT_TYPES  21	// total


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
#define GLB_PREFIX  LIB_WORLD"glb/"	// global templates
#define WLD_PREFIX  LIB_WORLD"wld/"	// room definitions
#define MOB_PREFIX  LIB_WORLD"mob/"	// monster prototypes
#define NAMES_PREFIX  LIB_TEXT"names/"	// mob namelists
#define OBJ_PREFIX  LIB_WORLD"obj/"	// object prototypes
#define RMT_PREFIX  LIB_WORLD"rmt/"	// room templates
#define SECTOR_PREFIX  LIB_WORLD"sect/"	// sect definitions
#define SKILL_PREFIX  LIB_WORLD"skill/"	// player skills
#define TRG_PREFIX  LIB_WORLD"trg/"	// trigger files
#define VEH_PREFIX  LIB_WORLD"veh/"	// vehicle files
#define HLP_PREFIX  LIB_TEXT"help/"	// for HELP <keyword>
#define INTROS_PREFIX  LIB_TEXT"intros/"	// for intro screens
#define BOOK_PREFIX  "books/"	// for books.c
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
#define GLB_SUFFIX  ".glb"	// global suffix
#define MOB_SUFFIX  ".mob"	// mob suffix for file saves
#define OBJ_SUFFIX  ".obj"	// obj suffix for file saves
#define RMT_SUFFIX  ".rmt"	// room template suffix
#define SECTOR_SUFFIX  ".sect"	// sector file suffix
#define SKILL_SUFFIX  ".skill"	// player skills
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

// misc files (user-modifiable libs)
#define CONFIG_FILE  LIB_MISC"game_configs"  // config.c system
#define IDEA_FILE  LIB_MISC"ideas"	// for the 'idea'-command
#define TYPO_FILE  LIB_MISC"typos"	//         'typo'
#define BUG_FILE  LIB_MISC"bugs"	//         'bug'
#define MESS_FILE  LIB_MISC"messages"	// damage messages
#define SOCMESS_FILE  LIB_MISC"socials"	// messgs for social acts
#define TIPS_OF_THE_DAY_FILE  LIB_MISC"tips"	// one-line tips shown on login
#define XNAME_FILE  LIB_MISC"xnames"	// invalid name substrings

// etc files (non-user-modifiable libs)
#define BAN_FILE  LIB_ETC"badsites"	// for the siteban system
#define TIME_FILE  LIB_ETC"time"	// for recording the big bang
#define EXP_FILE  LIB_ETC"exp_cycle"	// for experience cycling
#define INSTANCE_FILE  LIB_ETC"instances"	// instanced adventures
#define ISLAND_FILE  LIB_ETC"islands"	// island info
#define TRADING_POST_FILE  LIB_ETC"trading_post"	// for global trade
#define VERSION_FILE  LIB_ETC"version"	// for version tracking

// map output data
#define DATA_DIR  "../data/"
#define GEOGRAPHIC_MAP_FILE  DATA_DIR"map.txt"	// for map output
#define POLITICAL_MAP_FILE  DATA_DIR"map-political.txt"	// for political map
#define CITY_DATA_FILE  DATA_DIR"map-cities.txt"	// for cities on the website

// world blocks: the world is split into chunks for saving and updating
#define WORLD_BLOCK_SIZE  (MAP_WIDTH * 5)	// number of rooms per .wld file
#define NUM_WORLD_BLOCK_UPDATES  15	// world is divided into this many updates, and one fires per 30 seconds
#define GET_WORLD_BLOCK(roomvnum)  (roomvnum == NOWHERE ? NOWHERE : (int)(roomvnum / WORLD_BLOCK_SIZE))

// additional files
#define WORLD_MAP_FILE  LIB_WORLD"base_map"	// storage for the game's base map

// used for many file reads:
#define READ_SIZE 256


// for DB_BOOT_ configs
struct db_boot_info_type {
	char *prefix;
	char *suffix;
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

// descriptors
extern descriptor_data *descriptor_list;

// empires
extern empire_data *empire_table;
extern struct trading_post_data *trading_list;
void delete_empire(empire_data *emp);
extern struct empire_island *get_empire_island(empire_data *emp, int island_id);
extern empire_data *get_or_create_empire(char_data *ch);
void free_empire(empire_data *emp);
void read_empire_members(empire_data *only_empire, bool read_techs);
void read_empire_territory(empire_data *emp);
extern empire_data *real_empire(empire_vnum vnum);
void reread_empire_tech(empire_data *emp);
void save_empire(empire_data *e);
void save_all_empires();

// extra descs
void free_extra_descs(struct extra_descr_data **list);

// globals
extern struct global_data *globals_table;
void free_global(struct global_data *glb);
extern struct global_data *global_proto(any_vnum vnum);

// islands
extern struct island_info *island_table;
extern struct island_info *get_island(int island_id, bool create_if_missing);
extern struct island_info *get_island_by_coords(char *coords);
extern struct island_info *get_island_by_name(char *name);

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
void reset_char(char_data *ch);
void free_char(char_data *ch);
void set_title(char_data *ch, char *title);

void save_char(char_data *ch, room_data *load_room);
#define SAVE_CHAR(ch)  save_char((ch), (IN_ROOM(ch) ? IN_ROOM(ch) : (GET_LOADROOM(ch) != NOWHERE ? real_room(GET_LOADROOM(ch)) : NULL)))

void update_player_index(player_index_data *index, char_data *ch);
extern char_data *find_or_load_player(char *name, bool *is_file);
void store_loaded_char(char_data *ch);
char_data *load_player(char *name, bool normal);

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
extern char_data *is_at_menu(int id);
extern char_data *is_playing(int id);

// room templates
extern room_template *room_template_table;
void free_room_template(room_template *rmt);
extern room_template *room_template_proto(rmt_vnum vnum);

// sectors
extern sector_data *sector_table;
void free_sector(struct sector_data *st);
extern sector_data *sector_proto(sector_vnum vnum);

// skills
extern skill_data *skill_table;
extern skill_data *sorted_skills;
extern skill_data *find_skill(char *argument);
extern skill_data *find_skill_by_name(char *name);
extern skill_data *find_skill_by_vnum(any_vnum vnum);
void free_skill(skill_data *skill);
extern char *get_skill_abbrev_by_vnum(any_vnum vnum);
extern char *get_skill_name_by_vnum(any_vnum vnum);

// triggers
extern trig_data *trigger_table;
extern trig_data *trigger_list;

// vehicles
extern vehicle_data *vehicle_list;
extern vehicle_data *vehicle_table;
extern vehicle_data *vehicle_proto(any_vnum vnum);
void free_vehicle(vehicle_data *veh);

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
extern struct obj_apply *copy_apply_list(struct obj_apply *list);
extern struct resource_data *create_resource_list(int first_vnum, int first_amount, ...);
void free_resource_list(struct resource_data *list);

// more frees
void free_apply_list(struct obj_apply *list);
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
	extern char buf[MAX_STRING_LENGTH];
	extern char buf1[MAX_STRING_LENGTH];
	extern char buf2[MAX_STRING_LENGTH];
	extern char arg[MAX_STRING_LENGTH];
#endif
