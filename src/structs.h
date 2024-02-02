/* ************************************************************************
*   File: structs.h                                       EmpireMUD 2.0b5 *
*  Usage: header file for central structures and contstants               *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/**
* Contents:
*   Main Configuration
*   Basic Types and Consts
*   #define Section
*     Miscellaneous Defines
*     Ability Defines
*     Adventure Defines
*     Archetype Defines
*     Augment Defines
*     Book Defines
*     Building Defines
*     Character Defines
*     Class Defines
*     Config Defines
*     Craft Defines
*     Crop Defines
*     Empire Defines
*     Event Defines
*     Event Defines (Timed Event System)
*     Faction Defines
*     Game Defines
*     Generic Defines
*     Mobile Defines
*     Moon Defines
*     Object Defines
*     Player Defines
*     Progress Defines
*     Quest Defines
*     Sector Defines
*     Shop Defines
*     Social Defines
*     Vehicle Defines
*     Weather and Season Defines
*     World Defines
*     Maxima and Limits
*   Structs Section
*     Miscellaneous Structs
*     Ability Structs
*     Adventure Structs
*     Archetype Structs
*     Attack Message Structs
*     Augment Structs
*     Book Structs
*     Building Structs
*     Class Structs
*     Map Structs
*     Mobile Structs
*     Player Structs
*     Character Structs
*     Craft Structs
*     Crop Structs
*     Data Structs
*     Empire Structs
*     Event Structs
*     Event Structs (Timed Event System)
*     Faction Structs
*     Fight Message Structs
*     Game Structs
*     Generic Structs
*     Object Structs
*     Progress Structs
*     Quest Structs
*     Sector Structs
*     Shop Structs
*     Social Structs
*     Trigger Structs
*     Vehicle Structs
*     Weather and Season Structs
*     World Structs
*/

// Note: You can usually find consts or other things related to a set of flags
// by searching for CONST_PREFIX_x (e.g. WEAR_x for wear flags).


#include <math.h>
#include "protocol.h" // needed by everything
#include "uthash.h"	// needed by everything
#include "utlist.h"	// needed by everything

 //////////////////////////////////////////////////////////////////////////////
//// MAIN CONFIGURATION //////////////////////////////////////////////////////

// These are configurations which need to be in a header file, and most of
// which can't be changed without corrupting your world or player files.

/*
 * EmpireMUD features an resizeable world, though you WILL have to
 * delete and recreate your map and zone files to change the size.  A
 * 500 x 500 creates a map of 250000 rooms.  If you don't have a lot of memory,
 * you might consider less.  It need not be square, you could use 200 x 400 or
 * 800 x 600.
 *
 * Your actual RAM usage will depend on how much land your game has. Ocean
 * tiles are removed from memory when not in use.
 */
#define MAP_WIDTH  1800
#define MAP_HEIGHT  1000
#define MAP_SIZE  (MAP_WIDTH * MAP_HEIGHT)

// for string formatting
#define X_PRECISION  (MAP_WIDTH <= 1000 ? 3 : 4)
#define Y_PRECISION  (MAP_HEIGHT <= 1000 ? 3 : 4)

#define WRAP_X  TRUE	// whether the mud wraps east-west
#define WRAP_Y  FALSE	// whether the mud wraps north-south


// What range of a 'real world' is covered by this map (-90 to 90 is the maximum range of latitudes; degrees south are represented as a negative number)
// Y_MAX_LATITUDE must always be the further north of the two:
#define Y_MIN_LATITUDE  -67	// 0 is the equator, -66.56 is the anarctic circle
#define Y_MAX_LATITUDE  67	// 66.56 is the arctic circle, 90 is the north pole

// -180 to 180 covers a full globe
#define X_MIN_LONGITUDE  -180	// min must always be the lower number
#define X_MAX_LONGITUDE  180

// these can safely be floating points
#define ARCTIC_LATITUDE  66.56	// based on the real world
#define TROPIC_LATITUDE  23.43	// based on the real world

// converts an X-coordinate to equivalent longitude based on X_MIN_LONGITUDE/X_MAX_LONGITUDE
#define X_TO_LONGITUDE(x_coord)  ((((double)x_coord) / MAP_WIDTH) * (X_MAX_LONGITUDE - X_MIN_LONGITUDE) + X_MIN_LONGITUDE)

// converts a Y-coordinate to the equivalent latitude, based on Y_MIN_LATITUDE/Y_MAX_LATITUDE
#define Y_TO_LATITUDE(y_coord)  ((((double)(y_coord) / MAP_HEIGHT) * ABSOLUTE(Y_MAX_LATITUDE - Y_MIN_LATITUDE)) + Y_MIN_LATITUDE)

// based on EmpireMUD's 360-day year (12 x 30-day months): code expects these 90 days apart
#define FIRST_EQUINOX_DOY  81	// march 21
#define NORTHERN_SOLSTICE_DOY  171	// june 21
#define LAST_EQUINOX_DOY  261	// sept 21
#define SOUTHERN_SOLSTICE_DOY  351	// dec 21


#define COIN_VALUE  0.1	// value of a coin as compared to 1 wealth (0.1 coin value = 10 coins per wealth)

// ***WARNING*** Change this before starting your playerfile
// but NEVER change it after, or you may not be able to log in to any character
// NOTE: You should use a salt starting with "$5$" and ending with "$" in order
// to get support for longer passwords and proper encryption, e.g. "$5$salt$"
// See http://linux.die.net/man/3/crypt for more info.
#define PASSWORD_SALT  "salt"

// features for determining whether or not a map tile needs to be in RAM as a room_data*
#define BASIC_OCEAN  6	// sector vnum used as the base for fully ignorable and blank rooms
#define TILE_KEEP_FLAGS  (SECTF_START_LOCATION)	// flags for rooms that MUST stay in RAM

// this determines if a room is "empty"/"blank" and doesn't need to stay in RAM
#define CAN_UNLOAD_MAP_ROOM(room)  ( \
	!COMPLEX_DATA(room) && \
	GET_ROOM_VNUM(room) < MAP_SIZE && \
	GET_EXITS_HERE(room) == 0 && \
	SECT(room) == world_map[FLAT_X_COORD(room)][FLAT_Y_COORD(room)].sector_type && \
	!ROOM_SECT_FLAGGED(room, TILE_KEEP_FLAGS) && \
	!ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE | ROOM_AFF_FAKE_INSTANCE) && \
	!ROOM_OWNER(room) && !ROOM_CONTENTS(room) && !ROOM_PEOPLE(room) && \
	!ROOM_VEHICLES(room) && \
	!ROOM_AFFECTS(room) && \
	!(room)->script \
)	// end CAN_UNLOAD_MAP_ROOM()

// defines what belongs in the land_map table
#define SECT_IS_LAND_MAP(sect)  (GET_SECT_VNUM(sect) != BASIC_OCEAN)

// for the in-game configs
#define CONFIG_HANDLER(name)	void (name)(char_data *ch, struct config_type *config, char *argument)


 //////////////////////////////////////////////////////////////////////////////
//// BASIC TYPES AND CONSTS //////////////////////////////////////////////////

#define BIT(bit)  (((bitvector_t)1) << ((bitvector_t)(bit)))
#define PROMO_APPLY(name)	void (name)(char_data *ch)


/*
 * Eventually we want to be able to redefine the below to any arbitrary
 * value.  This will allow us to use unsigned data types to get more
 * room and also remove the '< 0' checks all over that implicitly
 * assume these values. -gg 12/17/99
 */
#define NOWHERE		-1	/* nil reference for room-database	*/
#define NOTHING		-1	/* nil reference for objects		*/
#define NOBODY		-1	/* nil reference for mobiles		*/
#define NO_DIR		-1	// nil reference for directions

// These are generally just used to make a variable's "neutral" value
// easier to understand, for things that initialize to -1 or 0
#define ANY_ISLAND  -1	// for finding an island in certain functions
#define NO_ISLAND  -1	// error code for island lookups
#define UNAPPLIED_ISLAND  -2	// distinct from NO_ISLAND to prevent re-applying a vehicle/building to NO_ISLAND
#define NO_WEAR  -1	// bad wear location
#define END_RESOURCE_LIST  { NOTHING, -1 }
#define NOBITS  0	// lack of bitvector bits (for clarity in funky structs)
#define NO_ABIL  NO_SKILL	// things that don't require an ability
#define NO_FLAGS  0
#define NO_SKILL  -1	// things that don't require a skill
#define NO_TECH  NOTHING	// empire or player tech
#define NO_TOOL  NOBITS		// does not require a tool
#define NOT_REPEATABLE  -1	// quest's repeatable_after
#define OTHER_COIN  NOTHING	// use the NOTHING value to store the "other" coin type (which stores by empire id)
#define REAL_OTHER_COIN  NULL	// for when other-coin type is an empire pointer
#define UNLIMITED  -1	// unlimited duration/timer
#define WORKFORCE_UNLIMITED  -1	// no resource limit on workforce


// Various other special codes
#define MAGIC_NUMBER  (0x06)	// Arbitrary number that won't be in a string


// TYPE_x: for things that hold more than one type of thing:
#define TYPE_OBJ  0
#define TYPE_MOB  1
#define TYPE_ROOM  2
#define TYPE_MINE_DATA  3
#define TYPE_BLD  4
#define TYPE_VEH  5
#define TYPE_ABIL  6
#define TYPE_LIQUID  7


// basic types
typedef signed char byte;
typedef signed char sbyte;
typedef unsigned char ubyte;
typedef signed short int sh_int;
typedef unsigned short int ush_int;
#if !defined(__cplusplus)	/* Anyone know a portable method? */
	typedef char bool;
#endif


// 'unsigned long long' will give you at least 64 bits if you have GCC (or C99+)
// -- THIS IS REQUIRED because some bit sets already use it
typedef unsigned long long	bitvector_t;
typedef signed long long sbitvector_t;	// signed version needed for some uses


// vnums
typedef int any_vnum;
typedef any_vnum adv_vnum;	// for adventure zones
typedef any_vnum bld_vnum;	// for buildings
typedef any_vnum book_vnum;	// for the book_table
typedef any_vnum craft_vnum;	// craft recipe's vnum
typedef any_vnum crop_vnum;	// crop's vnum
typedef any_vnum empire_vnum;	// empire virtual number
typedef any_vnum mob_vnum;	/* A mob's vnum type */
typedef any_vnum obj_vnum;	/* An object's vnum type */
typedef any_vnum rmt_vnum;	// room_template
typedef any_vnum room_vnum;	/* A room's vnum type */
typedef any_vnum sector_vnum;	// sector's vnum
typedef any_vnum trig_vnum;	// for dg scripts
typedef any_vnum veh_vnum;	// for vehicles


// For simplicity...
typedef struct ability_data ability_data;
typedef struct account_data account_data;
typedef struct adventure_data adv_data;
typedef struct archetype_data archetype_data;
typedef struct attack_message_data attack_message_data;
typedef struct augment_data augment_data;
typedef struct bld_data bld_data;
typedef struct book_data book_data;
typedef struct char_data char_data;
typedef struct class_data class_data;
typedef struct craft_data craft_data;
typedef struct crop_data crop_data;
typedef struct descriptor_data descriptor_data;
typedef struct empire_data empire_data;
typedef struct event_data event_data;
typedef struct faction_data faction_data;
typedef struct generic_data generic_data;
typedef struct index_data index_data;
typedef struct morph_data morph_data;
typedef struct obj_data obj_data;
typedef struct player_index_data player_index_data;
typedef struct progress_data progress_data;
typedef struct quest_data quest_data;
typedef struct room_data room_data;
typedef struct room_template room_template;
typedef struct sector_data sector_data;
typedef struct shop_data shop_data;
typedef struct social_data social_data;
typedef struct skill_data skill_data;
typedef struct trig_data trig_data;
typedef struct vehicle_data vehicle_data;


  /////////////////////////////////////////////////////////////////////////////
 //// #DEFINE SECTION ////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS DEFINES ///////////////////////////////////////////////////

// APPLY_x: Modifier const used with obj affects ('A' fields), player affects, etc
#define APPLY_NONE  0	// No effect
#define APPLY_STRENGTH  1	// Apply to strength
#define APPLY_DEXTERITY  2	// Apply to dexterity
#define APPLY_HEALTH_REGEN  3	// +health regen
#define APPLY_CHARISMA  4	// Apply to charisma
#define APPLY_GREATNESS  5	// Apply to greatness
#define APPLY_MOVE_REGEN  6	// +move regen
#define APPLY_MANA_REGEN  7	// +mana regen
#define APPLY_INTELLIGENCE  8	// Apply to intelligence
#define APPLY_WITS  9	// Apply to wits
#define APPLY_AGE  10	// Apply to age
#define APPLY_MOVE  11	// Apply to max move points
#define APPLY_RESIST_PHYSICAL  12	// Apply to physical damage resistance
#define APPLY_BLOCK  13	// Apply to chance to block
#define APPLY_HEAL_OVER_TIME  14	// heals you every "real update"
#define APPLY_HEALTH  15	// Apply to max health
#define APPLY_MANA  16	// Apply to max mana
#define APPLY_TO_HIT  17	// +to-hit
#define APPLY_DODGE  18	// +dodge
#define APPLY_INVENTORY  19	// add inventory capacity
#define APPLY_BLOOD  20	// add to max blood
#define APPLY_BONUS_PHYSICAL  21	// add to bonus damage
#define APPLY_BONUS_MAGICAL  22	// add to magical damage
#define APPLY_BONUS_HEALING  23	// add to healing
#define APPLY_RESIST_MAGICAL  24	// Apply to magic damage resistance
#define APPLY_CRAFTING  25	// bonus craft levels
#define APPLY_BLOOD_UPKEEP  26	// vampire blood requirement
#define APPLY_NIGHT_VISION  27	// bonus to nighttime light radius
#define APPLY_NEARBY_RANGE  28	// larger "nearby"
#define APPLY_WHERE_RANGE  29	// larger "where"
#define APPLY_WARMTH  30	// protects against cold weather
#define APPLY_COOLING  31	// protects against warm weather


// AUTOMSG_x: automessage types
#define AUTOMSG_ONE_TIME  0
#define AUTOMSG_ON_LOGIN  1
#define AUTOMSG_REPEATING  2


// don't change these
#define BAN_NOT  0
#define BAN_NEW  1
#define BAN_SELECT  2
#define BAN_ALL  3


// CONF_FLAG_x: config system flags
#define CONF_FLAG_DEPRECATED  BIT(0)
// limit: 15, struct config_type


// EVOLVER_x: flags passed through to evolve.c
#define EVOLVER_OWNED  BIT(0)	// tile is owned


// for modify.c
#define FORMAT_INDENT	BIT(0)


// GLOBAL_x types for global_data
#define GLOBAL_MOB_INTERACTIONS  0
#define GLOBAL_MINE_DATA  1
#define GLOBAL_NEWBIE_GEAR  2
#define GLOBAL_MAP_SPAWNS  3
#define GLOBAL_OBJ_INTERACTIONS  4


// GLB_FLAG_x flags for global_data
#define GLB_FLAG_IN_DEVELOPMENT  BIT(0)	// not live
#define GLB_FLAG_ADVENTURE_ONLY  BIT(1)	// does not apply outside same-adventure
#define GLB_FLAG_CUMULATIVE_PERCENT  BIT(2)	// accumulates percent with other valid globals instead of its own percent
#define GLB_FLAG_CHOOSE_LAST  BIT(3)	// the first choose-last global that passes is saved for later, if nothing else is chosen
#define GLB_FLAG_RARE  BIT(4)	// a rare result (has various definitions by type)


// Group Defines
#define GROUP_ANON  BIT(0)	// Group is hidden/anonymous


// INTERACT_x: standard interactions
#define INTERACT_BUTCHER  0
#define INTERACT_SKIN  1
#define INTERACT_SHEAR  2
#define INTERACT_BARDE  3
#define INTERACT_LOOT  4
#define INTERACT_DIG  5
#define INTERACT_FORAGE  6
#define INTERACT_PICK  7	// formerly FIND-HERB
#define INTERACT_HARVEST  8
#define INTERACT_GATHER  9
#define INTERACT_ENCOUNTER  10
#define INTERACT_LIGHT  11
#define INTERACT_PICKPOCKET  12
#define INTERACT_MINE  13
#define INTERACT_COMBINE  14
#define INTERACT_SEPARATE  15
#define INTERACT_SCRAPE  16
#define INTERACT_SAW  17
#define INTERACT_TAN  18
#define INTERACT_CHIP  19
#define INTERACT_CHOP  20
#define INTERACT_FISH  21
#define INTERACT_PAN  22
#define INTERACT_QUARRY  23
#define INTERACT_TAME  24
#define INTERACT_SEED  25
#define INTERACT_DECAYS_TO  26
#define INTERACT_CONSUMES_TO  27
#define INTERACT_IDENTIFIES_TO  28
#define INTERACT_RUINS_TO_BLD  29
#define INTERACT_RUINS_TO_VEH  30
#define INTERACT_PRODUCTION  31
#define INTERACT_SKILLED_LABOR  32
#define INTERACT_LIQUID_CONJURE  33
#define INTERACT_OBJECT_CONJURE  34
#define INTERACT_VEHICLE_CONJURE  35
#define INTERACT_DISENCHANT  36
#define NUM_INTERACTS  37

// for do_gen_interact
#define GEN_INTERACT_SPECIAL(name)	void (name)(char_data *ch, room_data *room, const struct gen_interact_data_t *data)


// INTERACT_RESTRICT_x: types of interaction restrictions
#define INTERACT_RESTRICT_ABILITY  0	// player must have an ability
#define INTERACT_RESTRICT_PTECH  1	// player must have a ptech
#define INTERACT_RESTRICT_TECH  2	// empire must have a tech
#define INTERACT_RESTRICT_NORMAL  3	// only when mob/obj is not hard/group
#define INTERACT_RESTRICT_HARD  4	// only when mob/obj is 'hard' (but not group)
#define INTERACT_RESTRICT_GROUP  5	// only when mob/obj is 'group' (but not hard)
#define INTERACT_RESTRICT_BOSS  6	// only when mob/obj is 'hard' (hard + group)
#define INTERACT_RESTRICT_DEPLETION  7	// determines which depletion is checked/applied, if applicable
#define INTERACT_RESTRICT_TOOL  8	// tool required for the interaction


// for object saving
#define LOC_INVENTORY	0
#define MAX_BAG_ROWS	5


// REQ_x: requirement types (used for things like quest conditions and pre-reqs)
#define REQ_COMPLETED_QUEST  0
#define REQ_GET_COMPONENT  1
#define REQ_GET_OBJECT  2
#define REQ_KILL_MOB  3
#define REQ_KILL_MOB_FLAGGED  4
#define REQ_NOT_COMPLETED_QUEST  5
#define REQ_NOT_ON_QUEST  6
#define REQ_OWN_BUILDING  7
#define REQ_OWN_VEHICLE  8
#define REQ_SKILL_LEVEL_OVER  9
#define REQ_SKILL_LEVEL_UNDER  10
#define REQ_TRIGGERED  11	// completed by a script
#define REQ_VISIT_BUILDING  12
#define REQ_VISIT_ROOM_TEMPLATE  13
#define REQ_VISIT_SECTOR  14
#define REQ_HAVE_ABILITY  15
#define REQ_REP_OVER  16
#define REQ_REP_UNDER  17
#define REQ_WEARING  18
#define REQ_WEARING_OR_HAS  19
#define REQ_GET_CURRENCY  20
#define REQ_GET_COINS  21
#define REQ_CAN_GAIN_SKILL  22
#define REQ_CROP_VARIETY  23
#define REQ_OWN_HOMES  24
#define REQ_OWN_SECTOR  25
#define REQ_OWN_BUILDING_FUNCTION  26
#define REQ_OWN_VEHICLE_FLAGGED  27
#define REQ_EMPIRE_WEALTH  28
#define REQ_EMPIRE_FAME  29
#define REQ_EMPIRE_GREATNESS  30
#define REQ_DIPLOMACY  31
#define REQ_HAVE_CITY  32
#define REQ_EMPIRE_MILITARY  33
#define REQ_EMPIRE_PRODUCED_OBJECT  34
#define REQ_EMPIRE_PRODUCED_COMPONENT  35
#define REQ_EVENT_RUNNING  36
#define REQ_EVENT_NOT_RUNNING  37
#define REQ_LEVEL_UNDER  38
#define REQ_LEVEL_OVER  39
#define REQ_OWN_VEHICLE_FUNCTION  40
#define REQ_SPEAK_LANGUAGE  41
#define REQ_RECOGNIZE_LANGUAGE  42
#define REQ_COMPLETED_QUEST_EVER  43
#define REQ_DAYTIME  44
#define REQ_NIGHTTIME  45
#define REQ_DIPLOMACY_OVER  46


// REQ_AMT_x: How numbers displayed for different REQ_ types
#define REQ_AMT_NONE  0	// show as "completed"
#define REQ_AMT_NUMBER  1  // show as x/X
#define REQ_AMT_THRESHOLD  2	// needs a numeric arg but shows as a threshold
#define REQ_AMT_REPUTATION  3	// uses a faction reputation


// SHIPPING_x: for the shipping system
#define SHIPPING_QUEUED  0	// has not been processed yet
#define SHIPPING_EN_ROUTE  1	// waiting to deliver
#define SHIPPING_DELIVERED  2	// indicates the ship has been delivered and these can be offloaded to the destination
#define SHIPPING_WAITING_FOR_SHIP  3	// waiting for a ship


// SPAWN_x: mob spawn flags
#define SPAWN_NOCTURNAL  BIT(0)	// a. only spawns at night
#define SPAWN_DIURNAL  BIT(1)	// b. only spawns during day
#define SPAWN_CLAIMED  BIT(2)	// c. only claimed land
#define SPAWN_UNCLAIMED  BIT(3)	// d. only unclaimed land
#define SPAWN_CITY  BIT(4)	// e. only land in city (and claimed)
#define SPAWN_OUT_OF_CITY  BIT(5)	// f. only spawns outside of cities
#define SPAWN_NORTHERN  BIT(6)	// g. only spawns in the northern half of the map
#define SPAWN_SOUTHERN  BIT(7)	// h. only spawns in the southern half
#define SPAWN_EASTERN  BIT(8)	// i. only spawns in the east half
#define SPAWN_WESTERN  BIT(9)	// j. only spawns in the west half
#define SPAWN_CONTINENT_ONLY  BIT(10)	// k. only spawns on continents
#define SPAWN_NO_CONTINENT  BIT(11)	// l. won't spawn on continents
#define SPAWN_SPRING_ONLY  BIT(12)	// m. only during spring (or multiple seasons as listed)
#define SPAWN_SUMMER_ONLY  BIT(13)	// n. only during summer (or multiple seasons as listed)
#define SPAWN_AUTUMN_ONLY  BIT(14)	// o. only during autumn (or multiple seasons as listed)
#define SPAWN_WINTER_ONLY  BIT(15)	// p. only during winter (or multiple seasons as listed)


// trading post data state flags
#define TPD_FOR_SALE  BIT(0)	// main phase of sale
#define TPD_BOUGHT  BIT(1)	// ended with sale
#define TPD_EXPIRED  BIT(2)	// ended without sale
#define TPD_OBJ_PENDING  BIT(3)	// obj not received
#define TPD_COINS_PENDING  BIT(4)	// coins not received


 //////////////////////////////////////////////////////////////////////////////
//// ABILITY DEFINES /////////////////////////////////////////////////////////

// see skills.h


 //////////////////////////////////////////////////////////////////////////////
//// ADVENTURE DEFINES ///////////////////////////////////////////////////////

// ADV_x: adventure flags
#define ADV_IN_DEVELOPMENT  BIT(0)	// a. will not generate instances
#define ADV_LOCK_LEVEL_ON_ENTER  BIT(1)	// b. lock levels on entry
#define ADV_LOCK_LEVEL_ON_COMBAT  BIT(2)	// c. lock levels when combat starts
#define ADV_NO_NEARBY  BIT(3)	// d. hide from mortal nearby command
#define ADV_ROTATABLE  BIT(4)	// e. random rotation on instantiate
#define ADV_CONFUSING_RANDOMS  BIT(5)	// f. random exits do not need to match
#define ADV_NO_NEWBIE  BIT(6)	// g. prevents spawning on newbie islands
#define ADV_NEWBIE_ONLY  BIT(7)	// h. only spawns on newbie islands
#define ADV_NO_MOB_CLEANUP  BIT(8)	// i. won't despawn mobs that escaped the instance
#define ADV_EMPTY_RESET_ONLY  BIT(9)	// j. won't reset while players are inside
#define ADV_CAN_DELAY_LOAD  BIT(10)	// k. can save memory by not instantiating till a player appears
#define ADV_IGNORE_WORLD_SIZE  BIT(11)	// l. does not adjust the instance limit
#define ADV_IGNORE_ISLAND_LEVELS  BIT(12)	// m. does not skip islands with no players in the level range
#define ADV_CHECK_OUTSIDE_FIGHTS  BIT(13)	// n. looks for mobs in combat before despawning
#define ADV_GLOBAL_NEARBY  BIT(14)	// o. will show the closest one no matter how far away
#define ADV_DETECTABLE  BIT(15)	// p. can be found with the ability action "detect adventures around"


// ADV_LINK_x: adventure link rule types
#define ADV_LINK_BUILDING_EXISTING  0
#define ADV_LINK_BUILDING_NEW  1
#define ADV_LINK_PORTAL_WORLD  2
#define ADV_LINK_PORTAL_BUILDING_EXISTING  3
#define ADV_LINK_PORTAL_BUILDING_NEW  4
#define ADV_LINK_TIME_LIMIT  5
#define ADV_LINK_NOT_NEAR_SELF  6
#define ADV_LINK_PORTAL_CROP  7
#define ADV_LINK_EVENT_RUNNING  8
#define ADV_LINK_PORTAL_VEH_EXISTING  9
#define ADV_LINK_PORTAL_VEH_NEW_BUILDING_EXISTING  10
#define ADV_LINK_PORTAL_VEH_NEW_BUILDING_NEW  11
#define ADV_LINK_PORTAL_VEH_NEW_CROP  12
#define ADV_LINK_PORTAL_VEH_NEW_WORLD  13
#define ADV_LINK_IN_VEH_EXISTING  14
#define ADV_LINK_IN_VEH_NEW_BUILDING_EXISTING  15
#define ADV_LINK_IN_VEH_NEW_BUILDING_NEW  16
#define ADV_LINK_IN_VEH_NEW_CROP  17
#define ADV_LINK_IN_VEH_NEW_WORLD  18


// ADV_LINKF_x: adventure link rule flags
#define ADV_LINKF_CLAIMED_OK  BIT(0)	// can spawn on claimed territory
#define ADV_LINKF_CITY_ONLY  BIT(1)	// only spawns on claimed land in cities
#define ADV_LINKF_NO_CITY  BIT(2)	// won't spawn on claimed land in cities
#define ADV_LINKF_CLAIMED_ONLY  BIT(3)	// ONLY spawns on claimed tiles
#define ADV_LINKF_CONTINENT_ONLY  BIT(4)	// only spawns on continents
#define ADV_LINKF_NO_CONTINENT  BIT(5)	// does not spawn on continents


// ADV_SPAWN_x: adventure spawn types
#define ADV_SPAWN_MOB  0
#define ADV_SPAWN_OBJ  1
#define ADV_SPAWN_VEH  2


// INST_x: instance flags
#define INST_COMPLETED  BIT(0)	// instance is done and can be cleaned up
#define INST_NEEDS_LOAD  BIT(1)	// instance is not loaded yet


// RMT_X: room template flags
#define RMT_OUTDOOR  BIT(0)	// a. sun during day
#define RMT_DARK  BIT(1)	// b. requires a light at all times
#define RMT_LIGHT  BIT(2)	// c. never requires a light
#define RMT_NO_MOB  BIT(3)	// d. mobs won't enter
#define RMT_PEACEFUL  BIT(4)	// e. no attacking/saferoom
#define RMT_NEED_BOAT  BIT(5)	// f. requires a boat
#define RMT_NO_TELEPORT  BIT(6)	// g. cannot teleport in/out
#define RMT_LOOK_OUT  BIT(7)	// h. can see the map using "look out"
#define RMT_NO_LOCATION  BIT(8)	// i. don't show a location, disables where
	#define RMT_UNUSED1  BIT(9)
	#define RMT_UNUSED2  BIT(10)


 //////////////////////////////////////////////////////////////////////////////
//// ARCHETYPE DEFINES ///////////////////////////////////////////////////////

// ARCHT_x: archetype types
#define ARCHT_ORIGIN  0
#define ARCHT_HOBBY  1
#define NUM_ARCHETYPE_TYPES  2	// must be total number


// ARCH_x: archetype flags
#define ARCH_IN_DEVELOPMENT  BIT(0)	// a. not available to players
#define ARCH_BASIC  BIT(1)	// b. will show on the basic list
#define ARCH_LOCKED  BIT(2)	// c. requires the player to unlock it


 //////////////////////////////////////////////////////////////////////////////
//// AUGMENT DEFINES /////////////////////////////////////////////////////////

// AUGMENT_x: Augment types
#define AUGMENT_NONE  0
#define AUGMENT_ENCHANTMENT  1
#define AUGMENT_HONE  2


// AUG_x: Augment flags
#define AUG_IN_DEVELOPMENT  BIT(0)	// a. can't be used by players
#define AUG_SELF_ONLY  BIT(1)	// b. can only be used on own items / force-bind
#define AUG_ARMOR  BIT(2)	// c. targets ARMOR item type
#define AUG_SHIELD  BIT(3)	// d. only targets shields


 //////////////////////////////////////////////////////////////////////////////
//// BOOK DEFINES ////////////////////////////////////////////////////////////

// BOOK_x: book flags
// no flags defined or implemented (but a 'bits' field is saved to file)


// misc
#define DEFAULT_BOOK  0	// in case of errors
#define MAX_BOOK_TITLE  40
#define MAX_BOOK_BYLINE  40
#define MAX_BOOK_ITEM_NAME  20
#define MAX_BOOK_PARAGRAPH  4000


 //////////////////////////////////////////////////////////////////////////////
//// BUILDING DEFINES ////////////////////////////////////////////////////////

// BLD_x: building data flags
#define BLD_ROOM  BIT(0)	// a designatable interior room instead of a whole building
#define BLD_ALLOW_MOUNTS  BIT(1)	// can ride/mount in
#define BLD_TWO_ENTRANCES  BIT(2)	// has a rear door
#define BLD_OPEN  BIT(3)	// always-open
#define BLD_CLOSED  BIT(4)	// always-closed
#define BLD_INTERLINK  BIT(5)	// can be interlinked
#define BLD_HERD  BIT(6)	// can herd
#define BLD_DEDICATE  BIT(7)	// can be dedicated to a player
#define BLD_IS_RUINS  BIT(8)	// building counts as ruins
#define BLD_NO_NPC  BIT(9)	// mobs won't walk in
#define BLD_BARRIER  BIT(10)	// can only go back the direction you came
#define BLD_IN_CITY_ONLY  BIT(11)	// can only be used in-city
#define BLD_LARGE_CITY_RADIUS  BIT(12)	// counts as in-city further than normal
#define BLD_NO_PAINT  BIT(13)	// cannot be painted
#define BLD_ATTACH_ROAD  BIT(14)	// building connects to roads on the map
#define BLD_BURNABLE  BIT(15)	// fire! fire!
#define BLD_EXIT  BIT(16)	// for ROOM-flagged interiors, players can 'exit' here
#define BLD_OBSCURE_VISION  BIT(17)	// blocks tiles behind it
#define BLD_ROAD_ICON  BIT(18)	// replaces its icon with the generated road icons (dashes)
#define BLD_ROAD_ICON_WIDE  BIT(19)	// replaces its icon with wide road icons (equals signs)
#define BLD_ATTACH_BARRIER  BIT(20)	// icons with @u/@v will attach to this
#define BLD_NO_CUSTOMIZE  BIT(21)	// cannot be customized
#define BLD_NO_ABANDON_WHEN_RUINED  BIT(22)	// won't auto-abandon when it becomes ruins
// #define BLD_UNUSED11  BIT(23)
// #define BLD_UNUSED12  BIT(24)
// #define BLD_UNUSED13  BIT(25)
// #define BLD_UNUSED14  BIT(26)
// #define BLD_UNUSED15  BIT(27)
// #define BLD_UNUSED16  BIT(28)
#define BLD_SAIL  BIT(29)	// ships can pass through building
// #define BLD_UNUSED17  BIT(30)
// #define BLD_UNUSED18  BIT(31)
// #define BLD_UNUSED19  BIT(32)
#define BLD_ITEM_LIMIT  BIT(33)	// room does not hold unlimited items
#define BLD_LONG_AUTOSTORE  BIT(34)	// autostore takes a long time here
// #define BLD_UNUSED20  BIT(35)
// #define BLD_UNUSED21  BIT(36)
#define BLD_HIGH_DEPLETION  BIT(37)	// allows more resource farming here
// #define BLD_UNUSED22  BIT(38)
// #define BLD_UNUSED23  BIT(39)
#define BLD_NO_DELETE  BIT(40)	// will not be deleted for not having a homeroom
// #define BLD_UNUSED24  BIT(41)
#define BLD_NEED_BOAT  BIT(42)	// requires a boat to enter
#define BLD_LOOK_OUT  BIT(43)	// can see the map using "look out"
#define BLD_SECONDARY_TERRITORY  BIT(44)	// similar to a ship -- counts as territory off the map
// #define BLD_UNUSED25  BIT(45)
// #define BLD_UNUSED26  BIT(46)	// formerly UPGRADED (use a function flag now)
// #define BLD_UNUSED27  BIT(47)


// BLD_ON_x: Terrain flags for building crafts -- these match up with build_on flags for building crafts
#define BLD_ON_WATER  BIT(0)
#define BLD_ON_PLAINS  BIT(1)
#define BLD_ON_MOUNTAIN  BIT(2)
#define BLD_ON_FOREST  BIT(3)
#define BLD_ON_DESERT  BIT(4)
#define BLD_ON_RIVER  BIT(5)
#define BLD_ON_JUNGLE  BIT(6)
#define BLD_ON_NOT_PLAYER_MADE  BIT(7)
#define BLD_ON_OCEAN  BIT(8)
#define BLD_ON_OASIS  BIT(9)
#define BLD_FACING_CROP  BIT(10)	// use only for facing, not for on
#define BLD_ON_GROVE  BIT(11)
#define BLD_ON_SWAMP  BIT(12)
#define BLD_ANY_FOREST  BIT(13)
#define BLD_FACING_OPEN_BUILDING  BIT(14)
#define BLD_ON_FLAT_TERRAIN  BIT(15)	// for facing-only
#define BLD_ON_SHALLOW_SEA  BIT(16)
#define BLD_ON_COAST  BIT(17)
#define BLD_ON_RIVERBANK  BIT(18)
#define BLD_ON_ESTUARY  BIT(19)
#define BLD_ON_LAKE  BIT(20)
#define BLD_ON_BASE_TERRAIN_ALLOWED  BIT(21)	// for facing-only, allows the base sector to match
#define BLD_ON_GIANT_TREE  BIT(22)
#define BLD_ON_ROAD  BIT(23)	// use on vehicles but not buildings


// BLD_REL_x: relationships with other buildings/vehicles
#define BLD_REL_UPGRADES_TO_BLD  0	// upgrades to a building type
#define BLD_REL_STORES_LIKE_BLD  1	// acts like another building for storage locations
#define BLD_REL_STORES_LIKE_VEH  2	// acts like another vehicle for storage locations
#define BLD_REL_UPGRADES_TO_VEH  3	// upgrades to another vehicle type
#define BLD_REL_FORCE_UPGRADE_BLD  4	// automatically upgrades on-reboot
#define BLD_REL_FORCE_UPGRADE_VEH  5	// automatically upgrades on-reboot


// Designate flags -- DES_x -- can only designate if building and room both have a matching flag
#define DES_CRYPT			BIT(0)
#define DES_VAULT			BIT(1)
#define DES_FORGE			BIT(2)
#define DES_TUNNEL			BIT(3)
#define DES_HALL			BIT(4)
#define DES_SKYBRIDGE		BIT(5)
#define DES_THRONE			BIT(6)
#define DES_ARMORY			BIT(7)
#define DES_GREATHALL		BIT(8)
#define DES_BATHS			BIT(9)
#define DES_LABORATORY		BIT(10)
#define DES_TOP_OF_TOWER	BIT(11)
#define DES_HOUSEHOLD		BIT(12)
#define DES_HAVEN			BIT(13)
#define DES_SHIP_MAIN		BIT(14)
#define DES_SHIP_LARGE		BIT(15)
#define DES_SHIP_EXTRA		BIT(16)
#define DES_LAND_VEHICLE	BIT(17)


// FNC_x: function flags (for buildings)
#define FNC_ALCHEMIST  BIT(0)	// can brew and mix here
#define FNC_UPGRADED  BIT(1)	// used on upgraded buildings / advanced crafts
	#define FNC_UNUSED2  BIT(2)	// formerly BATHS; this now uses a script
#define FNC_BEDROOM  BIT(3)	// boosts regen while sleeping
#define FNC_CARPENTER  BIT(4)	// required by some crafts
	#define FNC_UNUSED5  BIT(5)	// formerly DIGGING; no longer needed for workforce
#define FNC_DOCKS  BIT(6)	// grants the seaport tech to the empire; counts as a dock fo
#define FNC_FORGE  BIT(7)	// can use the forge command here
#define FNC_GLASSBLOWER  BIT(8)	// grants the Glassblowing tech to the empire
#define FNC_GUARD_TOWER  BIT(9)	// hostile toward enemy players, at range
	#define FNC_UNUSED  BIT(10)	// formerly "HENGE" which now uses a script
#define FNC_LIBRARY  BIT(11)	// can write and store books here
#define FNC_MAIL  BIT(12)	// players can send mail here
#define FNC_MILL  BIT(13)	// can use the mill command here
#define FNC_MINE  BIT(14)	// can be mined for ore resources
#define FNC_MINT  BIT(15)	// functions as a mint
#define FNC_PORTAL  BIT(16)	// functions as a portal building
#define FNC_POTTER  BIT(17)	// pottery craft time is reduced here
#define FNC_PRESS  BIT(18)	// can use the 'press' craft
#define FNC_SAW  BIT(19)	//allows sawing here
#define FNC_SHIPYARD  BIT(20)	// used to build ships
#define FNC_SMELT  BIT(21)	// allows smelting here
#define FNC_STABLE  BIT(22)	// can shear, milk, and barde animals here; animals in this 
#define FNC_SUMMON_PLAYER  BIT(23)	// allows the summon player command
#define FNC_TAILOR  BIT(24)	// counts as tailor for crafts like weaving
#define FNC_TANNERY  BIT(25)	// allows tanning here
#define FNC_TAVERN  BIT(26)	// for workforce crafting
#define FNC_TOMB  BIT(27)	// players can re-spawn here after dying
#define FNC_TRADING_POST  BIT(28)	// access to global trade, e.g. a trading post
#define FNC_VAULT  BIT(29)	// stores coins, can use the warehouse command for privileged
#define FNC_WAREHOUSE  BIT(30)	// can use the warehouse command and store unique items
#define FNC_DRINK_WATER  BIT(31)	// can drink here
#define FNC_COOKING_FIRE  BIT(32)	// can cook here
#define FNC_LARGER_NEARBY  BIT(33)	// extends the radius of 'nearby'
#define FNC_FISHING  BIT(34)	// workforce can fish here
#define FNC_STORE_ALL BIT(35) // anything can be stored here (does not allow retrieval)
#define FNC_IN_CITY_ONLY  BIT(36)	// functions only work in-city
#define FNC_OVEN  BIT(37)	// for cooking
#define FNC_MAGIC_WORKFSHOP  BIT(38)	// no code purpose but can be used for workforce
#define FNC_APOTHECARY  BIT(39)	// for use in crafts

// These function flags don't work on movable vehicles (they require room data)
#define IMMOBILE_FNCS  (FNC_MINE | FNC_TOMB | FNC_LIBRARY)


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER DEFINES ///////////////////////////////////////////////////////

// energy pools for char_point_data
#define HEALTH  0
#define MOVE  1
#define MANA  2
#define BLOOD  3
#define NUM_POOLS  4


// core attributes for char_data
#define STRENGTH  0
#define DEXTERITY  1
#define CHARISMA  2
#define GREATNESS  3
#define INTELLIGENCE  4
#define WITS  5
#define NUM_ATTRIBUTES  6


// extra attributes -- ATT_x
#define ATT_BONUS_INVENTORY  0	// carry capacity
#define ATT_RESIST_PHYSICAL  1	// damage reduction
#define ATT_BLOCK  2	// bonus block chance
#define ATT_TO_HIT  3	// bonus to-hit chance
#define ATT_DODGE  4	// bonus dodge chance
#define ATT_EXTRA_BLOOD  5	// larger blood pool
#define ATT_BONUS_PHYSICAL  6	// extra physical damage
#define ATT_BONUS_MAGICAL  7	// extra magical damage
#define ATT_BONUS_HEALING  8	// extra healing
#define ATT_HEAL_OVER_TIME  9	// heal per "real update"
#define ATT_RESIST_MAGICAL  10	// damage reduction
#define ATT_CRAFTING_BONUS  11	// levels added to crafting
#define ATT_BLOOD_UPKEEP  12	// blood cost per hour
#define ATT_AGE_MODIFIER  13	// +/- age
#define ATT_NIGHT_VISION  14	// bonus light radius at night
#define ATT_NEARBY_RANGE  15	// larger "nearby"
#define ATT_WHERE_RANGE  16		// larger "where"
#define ATT_WARMTH  17	// from gear that keeps you warm in the cold
#define ATT_COOLING  18	// from gear that keeps you cool in the heat
#define NUM_EXTRA_ATTRIBUTES  19


// AFF_x: Affect bits
/* WARNING: In the world files, NEVER set the bits marked "R" ("Reserved") */
#define AFF_BLIND  BIT(0)	// a. (R) Char is blind
#define AFF_MAJESTY  BIT(1)	// b. majesty
#define AFF_INFRAVISION  BIT(2)	// c. Char can see full in dark
#define AFF_SNEAK  BIT(3)	// d. Char can move quietly
#define AFF_HIDDEN  BIT(4)	// e. Char is hidden
#define AFF_CHARM  BIT(5)	// f. Char is charmed
#define AFF_INVISIBLE  BIT(6)	// g. Char is invisible
#define AFF_IMMUNE_PHYSICAL_DEBUFFS  BIT(7)	// h. Immunity to 'physical' debuffs
#define AFF_SENSE_HIDDEN  BIT(8)	// i. See hidden people
#define AFF_IMMUNE_PHYSICAL  BIT(9)	// j. Immune to physical damage
#define AFF_NO_TARGET_IN_ROOM  BIT(10)	// k. no-target
#define AFF_NO_SEE_IN_ROOM  BIT(11)	// l. don't see on look
#define AFF_FLYING  BIT(12)	// m. person can fly
#define AFF_NO_ATTACK  BIT(13)	// n. can't be attacked
#define AFF_IMMUNE_MAGICAL_DEBUFFS  BIT(14)	// o. immune to 'magical' debuffs
#define AFF_DISARMED  BIT(15)	// p. disarmed
#define AFF_HASTE  BIT(16)	// q. haste: attacks faster
#define AFF_IMMOBILIZED  BIT(17)	// r. immobilized: can't move (entangled)
#define AFF_SLOW  BIT(18)	// s. slow (how great did that work out)
#define AFF_STUNNED  BIT(19)	// t. stunned/unable to act
#define AFF_STONED  BIT(20)	// u. trippy effects
#define AFF_CANT_SPEND_BLOOD  BIT(21)	// v. prevents most vampire powers
	#define AFF_UNUSED  BIT(22)	// w. formerly AFF_CLAWS/CLAWS which were reworked in b5.166
#define AFF_DEATHSHROUDED  BIT(23)	// x. deathshrouded
#define AFF_EARTHMELDED  BIT(24)	// y. interred in the earth
#define AFF_MUMMIFIED  BIT(25)	// z. mummified
#define AFF_SOULMASK  BIT(26)	// A. shows no details to others on 'affects'
#define AFF_NO_TRACKS  BIT(27)	// B. leaves no tracks
#define AFF_IMMUNE_POISON_DEBUFFS  BIT(28)	// C. Immune to any 'poison' debuffs
#define AFF_IMMUNE_MENTAL_DEBUFFS  BIT(29)	// D. Immune to any 'mental' debuffs
#define AFF_IMMUNE_STUN  BIT(30)	// E. Cannot be hit by stun effects
#define AFF_ORDERED  BIT(31)	// F. Has been issued an order from a player
#define AFF_NO_DRINK_BLOOD  BIT(32)	// G. Vampires can't bite or sire
#define AFF_DISTRACTED  BIT(33)	// H. Player cannot perform timed actions
#define AFF_HARD_STUNNED  BIT(34)	// I. Hard stuns are uncleansable and don't trigger stun-immunity
#define AFF_IMMUNE_DAMAGE  BIT(35)	// J. Cannot take damage
#define AFF_NO_WHERE  BIT(36)	// K. cannot be found using 'WHERE'
#define AFF_WATERWALKING  BIT(37)	// L. won't drown or be affected by water restrictions
#define AFF_LIGHT  BIT(38)	// M. has a light (lights up the room)
#define AFF_POOR_REGENS  BIT(39)	// N. dramatically lower regens
#define AFF_SLOWER_ACTIONS  BIT(40)	// O. timed actions are slower
#define AFF_HUNGRIER  BIT(41)	// P. character becomes hungry faster
#define AFF_THIRSTIER  BIT(42)	// Q. character becomes thirsty faster
#define AFF_IMMUNE_TEMPERATURE  BIT(43)	// R. character does not suffer effects of heat/cold
#define AFF_AUTO_RESURRECT  BIT(44)	// S. will self-resurrect on death
#define AFF_COUNTERSPELL  BIT(45)	// T. will block a counterspellable ability and then remove itself
#define AFF_NO_DISARM  BIT(46)	// U. player cannot be disarmed


// Injury flags -- IS_INJURED
#define INJ_TIED  BIT(0)	/* Bound and gagged						*/
#define INJ_STAKED  BIT(1)	/* Stake thru heart						*/


// sex
#define SEX_NEUTRAL  0
#define SEX_MALE  1
#define SEX_FEMALE  2
#define NUM_GENDERS  3	// total


// SIZE_x: character size (determines blood pool, corpse size, etc) -- note: these must go in order but you'll need to write an auto-updater if you want to add one in the middle
#define SIZE_NEGLIGIBLE  0	// has no size
#define SIZE_TINY  1	// mouse
#define SIZE_SMALL  2	// dog
#define SIZE_NORMAL  3	// human
#define SIZE_LARGE  4	// horse
#define SIZE_HUGE  5	// elephant
#define SIZE_ENORMOUS  6	// dragon
#define NUM_SIZES  7	// total


// positions
#define POS_DEAD  0	/* dead				*/
#define POS_MORTALLYW  1	/* mortally wounded	*/
#define POS_INCAP  2	/* incapacitated	*/
#define POS_STUNNED  3	/* stunned/bashed	*/
#define POS_SLEEPING  4	/* sleeping			*/
#define POS_RESTING  5	/* resting			*/
#define POS_SITTING  6	/* sitting			*/
#define POS_FIGHTING  7	/* fighting			*/
#define POS_STANDING  8	/* standing			*/


 //////////////////////////////////////////////////////////////////////////////
//// CLASS DEFINES ///////////////////////////////////////////////////////////

// see skills.h


 //////////////////////////////////////////////////////////////////////////////
//// CONFIG DEFINES //////////////////////////////////////////////////////////

// WHO_LIST_SORT_x: config game who_list_sort [type]
#define WHO_LIST_SORT_ROLE_LEVEL  0
#define WHO_LIST_SORT_LEVEL  1


 //////////////////////////////////////////////////////////////////////////////
//// CRAFT DEFINES ///////////////////////////////////////////////////////////

// CRAFT_TYPE_x: do_gen_craft (trade.c)
#define CRAFT_TYPE_ERROR  0
#define CRAFT_TYPE_FORGE  1
#define CRAFT_TYPE_CRAFT  2
#define CRAFT_TYPE_COOK  3
#define CRAFT_TYPE_SEW  4
#define CRAFT_TYPE_MILL  5
#define CRAFT_TYPE_BREW  6
#define CRAFT_TYPE_MIX  7
#define CRAFT_TYPE_BUILD  8
#define CRAFT_TYPE_WEAVE  9
#define CRAFT_TYPE_WORKFORCE  10
#define CRAFT_TYPE_MANUFACTURE  11
#define CRAFT_TYPE_SMELT  12
#define CRAFT_TYPE_PRESS  13
#define CRAFT_TYPE_BAKE  14
#define CRAFT_TYPE_MAKE  15
#define CRAFT_TYPE_PROCESS  16


// CRAFT_x: Craft Flags for do_gen_craft
#define CRAFT_POTTERY  BIT(0)  // bonus at pottery; requires fire
#define CRAFT_BUILDING  BIT(1)  // makes a building (on any craft type; BUILD type automatically counts as this)
#define CRAFT_SKILLED_LABOR  BIT(2)  // workforce can only produce this if the empire has skilled labor
#define CRAFT_SKIP_CONSUMES_TO  BIT(3)  // won't run the consumes-to interaction on components (e.g. keep the jar from paint when mixing it into a new paint)
#define CRAFT_DARK_OK  BIT(4)  // light not necessary
	#define CRAFT_UNUSED3  BIT(5)  // formerly alchemist (which ultimately was the same as FIRE)
	#define CRAFT_UNUSED  BIT(6)  // formerly sharp-tool/knife (now uses requires-tool)
#define CRAFT_FIRE  BIT(7)  // requires any fire source
#define CRAFT_SOUP  BIT(8)  // is a soup: requires a container of water, and the "object" property is a liquid id
#define CRAFT_IN_DEVELOPMENT  BIT(9)	// craft cannot be performed by mortals
#define CRAFT_UPGRADE  BIT(10)	// build: is an upgrade, not a new building
#define CRAFT_DISMANTLE_ONLY  BIT(11)	// build: building can be dismantled but not built
#define CRAFT_IN_CITY_ONLY  BIT(12)	// craft/building must be inside a city
#define CRAFT_VEHICLE  BIT(13)	// creates a vehicle instead of an object
	#define CRAFT_UNUSED5  BIT(14)	// formerly shipyard (now uses a function)
	#define CRAFT_UNUSED6  BIT(15)	// formerly bld-upgraded (now uses a function flag)
#define CRAFT_LEARNED  BIT(16)	// cannot use unless learned
#define CRAFT_BY_RIVER  BIT(17)	// must be within 1 tile of river
#define CRAFT_REMOVE_PRODUCTION  BIT(18)	// empire will un-produce the resources; used for things like 'smelt' where nothing new is really made
#define CRAFT_TAKE_REQUIRED_OBJ  BIT(19)	// causes the craft to take the 'required-obj' when created, if any (and may refund it on dismantle)
#define CRAFT_DISMANTLE_WITHOUT_ABILITY  BIT(20)	// buildings and vehicles can be dismantled without the ability
#define CRAFT_TOOL_OR_FUNCTION  BIT(21)	// with this flag, only requires the tool OR the function
#define CRAFT_UNDAMAGED_DISMANTLE_REFUND  BIT(22)	// refunds all materials when dismantled while undamaged (instead of 90%)
#define CRAFT_FULL_DISMANTLE_REFUND  BIT(23)	// refunds all materials when dismantled no matter what the health (instead of 20%-90%)


// For find_building_list_entry
#define FIND_BUILD_NORMAL  0
#define FIND_BUILD_UPGRADE  1

// for gen_craft_data[].strings
#define GCD_STRING_TO_CHAR  0
#define GCD_STRING_TO_ROOM  1
#define GCD_LONG_DESC  2


 //////////////////////////////////////////////////////////////////////////////
//// CROP DEFINES ////////////////////////////////////////////////////////////

// CROPF_x: crop flags
#define CROPF_REQUIRES_WATER  BIT(0)	// only plants near water
#define CROPF_IS_ORCHARD  BIT(1)	// follows orchard rules
#define CROPF_NOT_WILD  BIT(2)	// crop will never spawn in the wild
#define CROPF_NEWBIE_ONLY  BIT(3)	// only spawns on newbie islands
#define CROPF_NO_NEWBIE  BIT(4)	// never spawns on newbie islands
#define CROPF_ANY_LISTED_CLIMATE  BIT(5)	// climtes are "or" not "and"
#define CROPF_NO_GLOBAL_SPAWNS  BIT(6)	// won't use global spawn lists
#define CROPF_LOCK_ICON  BIT(7)	// similar to sectors; crop has a permanent random icon


// CROP_CUSTOM_x: custom messages
#define CROP_CUSTOM_MAGIC_GROWTH		0	// shown on magic-growth evolution


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE DEFINES //////////////////////////////////////////////////////////

// DELAY_REFRESH_x: flags indicating something on the empire needs a refresh
#define DELAY_REFRESH_CROP_VARIETY  BIT(0)	// refreshes specific progress goals
#define DELAY_REFRESH_GOAL_COMPLETE  BIT(1)	// checks for finished progress
#define DELAY_REFRESH_MEMBERS  BIT(2)	// re-reads empire member data
#define DELAY_REFRESH_GREATNESS  BIT(3)	// refreshes members-and-greatness
#define DELAY_REFRESH_MSDP_UPDATE_CLAIMS  BIT(4)	// empire members needs an MSDP update for claims
#define DELAY_REFRESH_MSDP_UPDATE_ALL  BIT(5)	// empire members needs an MSDP update for everything


// EADM_x: empire admin flags
#define EADM_NO_WAR  BIT(0)	// may not start a unilateral war
#define EADM_NO_STEAL  BIT(1)	// may not steal from other empires
#define EADM_CITY_CLAIMS_ONLY  BIT(2)	// may only claim in-city
#define EADM_NO_RENAME  BIT(3)	// cannot change name/adjective/description
#define EADM_DID_NEWBIE_MOVE  BIT(4)	// empire got its free move from the newbie island


// EATT_x: empire attributes
#define EATT_PROGRESS_POOL  0	// spendable progress points
#define EATT_BONUS_CITY_POINTS  1	// extra city points
#define EATT_MAX_CITY_SIZE  2	// how big the empire's cities can go (number of upgrades)
#define EATT_TERRITORY_PER_100_WEALTH  3	// number of tiles gained per 100 wealth
#define EATT_TERRITORY_PER_GREATNESS  4	// bonus to ter-per-grt
#define EATT_WORKFORCE_CAP  5	// workforce resource cap
#define EATT_BONUS_TERRITORY  6	// direct add to territory
#define EATT_DEFAULT_KEEP  7	// how many of an item to keep by default
#define NUM_EMPIRE_ATTRIBUTES  8	// total


// ETRAIT_x: empire trait flags
#define ETRAIT_DISTRUSTFUL  BIT(0)	// hostile behavior


// CHORE_x: workforce types
#define CHORE_BUILDING  0
#define CHORE_FARMING  1
#define CHORE_REPLANTING  2
#define CHORE_CHOPPING  3
#define CHORE_MAINTENANCE  4
#define CHORE_MINING  5
	#define CHORE_UNUSED6  6	// formerly digging; merged into productions
#define CHORE_SAWING  7
#define CHORE_SCRAPING  8
#define CHORE_SMELTING  9
#define CHORE_WEAVING  10
#define CHORE_PRODUCTION  11	// formerly digging, quarry, trapping, beekeeping, herb-gardening
#define CHORE_CRAFTING  12	// formerly nailmaking, brickmaking, glassmaking
	#define CHORE_UNUSED1  13
#define CHORE_ABANDON_DISMANTLED  14
	#define CHORE_UNUSED_C  15
#define CHORE_FIRE_BRIGADE  16
	#define CHORE_UNUSED_A  17
#define CHORE_TANNING  18
#define CHORE_SHEARING  19
#define CHORE_MINTING  20
#define CHORE_DISMANTLE_MINES  21
#define CHORE_ABANDON_CHOPPED  22
#define CHORE_ABANDON_FARMED  23
	#define CHORE_UNUSED2  24
#define CHORE_MILLING  25
	#define CHORE_UNUSED3  26	// formerly: CHORE_REPAIR_VEHICLES  26, merged with CHORE_MAINTENANCE
#define CHORE_OILMAKING  27
#define CHORE_GENERAL  28	// for reporting problems
#define CHORE_FISHING  29
#define CHORE_BURN_STUMPS  30
#define CHORE_PROSPECTING  31
	#define CHORE_UNUSED4  32
#define NUM_CHORES  33		// total


// DIPL_x: Diplomacy types
#define DIPL_PEACE  BIT(0)	// At peace
#define DIPL_WAR  BIT(1)	// At war
#define DIPL_THIEVERY  BIT(2)	// Can steal/thieve
#define DIPL_ALLIED  BIT(3)	// In an alliance
#define DIPL_NONAGGR  BIT(4)	// In a non-aggression pact
#define DIPL_TRADE  BIT(5)	// Open trading
#define DIPL_DISTRUST  BIT(6)	// Distrusting of one another
#define DIPL_TRUCE  BIT(7)	// end of war but not peace

// combo of all of them
#define ALL_DIPLS  (DIPL_PEACE | DIPL_WAR | DIPL_THIEVERY | DIPL_ALLIED | DIPL_NONAGGR | DIPL_TRADE | DIPL_DISTRUST | DIPL_TRUCE)
#define ALL_DIPLS_EXCEPT(flag)  (ALL_DIPLS & ~(flag))
#define CORE_DIPLS  ALL_DIPLS_EXCEPT(DIPL_TRADE)


// EINV_SORT_x: how sort empire inventory, or how it is currently sorted
#define EINV_UNSORTED			0	// something has unsorted it (must be 0)
#define EINV_SORT_AMOUNT		1	// sort by amount
#define EINV_SORT_PERISHABLE	2	// sort by decay timer


// ELOG_x: empire_log_data types
#define ELOG_NONE  0	// does not log to file
#define ELOG_ADMIN  1	// administrative changes
#define ELOG_DIPLOMACY  2	// all diplomacy commands
#define ELOG_HOSTILITY  3	// any hostile actions
#define ELOG_MEMBERS  4	// enroll, promote, demote, etc
#define ELOG_TERRITORY  5	// territory changes
#define ELOG_TRADE  6	// auto-trades
#define ELOG_LOGINS  7	// login/out/alt (does not save to file)
#define ELOG_SHIPPING  8	// shipments via do_ship
#define ELOG_WORKFORCE  9	// reporting related to workforce (does not echo, does not display unless requested)
#define ELOG_PROGRESS  10	// empire progression goals
#define ELOG_ALERT  11	// important messages


// ENEED_x: empire need types
#define ENEED_WORKFORCE  0	// food for workforce


// ENEED_STATUS_x: empire need statuses
#define ENEED_STATUS_UNSUPPLIED  BIT(0)	// a. empire failed to supply this type


// for empire_unique_storage->flags
#define EUS_VAULT  BIT(0)	// requires privilege


// OFFENSE_x: offense types
#define OFFENSE_STEALING  0
#define OFFENSE_ATTACKED_PLAYER  1
#define OFFENSE_GUARD_TOWER  2
#define OFFENSE_KILLED_PLAYER  3
#define OFFENSE_INFILTRATED  4
#define OFFENSE_ATTACKED_NPC  5
#define OFFENSE_SIEGED_BUILDING  6
#define OFFENSE_SIEGED_VEHICLE  7
#define OFFENSE_BURNED_BUILDING  8
#define OFFENSE_BURNED_VEHICLE  9
#define OFFENSE_PICKPOCKETED  10
#define OFFENSE_RECLAIMED  11
#define OFFENSE_BURNED_TILE  12
#define NUM_OFFENSES  13	// total


// OFF_x: offense flags
#define OFF_SEEN  BIT(0)	// someone saw this happen (npc here or player nearby)
#define OFF_WAR  BIT(1)	// happened at war (low/no hostile value)
#define OFF_AVENGED  BIT(2)	// player was killed or empire was warred for this (removes hostile value)


// PRIV_x: Empire Privilege Levels
#define PRIV_CLAIM  0	// Claim land
#define PRIV_BUILD  1	// Build structures
#define PRIV_HARVEST  2	// Harvest/plant things
#define PRIV_PROMOTE  3	// Promote/demote others
#define PRIV_CHOP  4	// Chop trees
#define PRIV_CEDE  5	// Cede land
#define PRIV_ENROLL  6	// Enroll new members
#define PRIV_WITHDRAW  7	// Can withdraw funds
#define PRIV_DIPLOMACY  8	// Can use diplomacy
#define PRIV_CUSTOMIZE  9	// custom room
#define PRIV_WORKFORCE  10	// set up chores
#define PRIV_STEALTH  11	// allow hostile stealth actions against other empires
#define PRIV_CITIES  12	// allows city management
#define PRIV_TRADE  13	// allows trade route management
#define PRIV_LOGS  14	// can view empire logs
#define PRIV_SHIPPING  15	// can use the ship command
#define PRIV_HOMES  16	// can set a home
#define PRIV_STORAGE  17	// can retrieve from storage
#define PRIV_WAREHOUSE  18	// can retrieve from warehouse
#define PRIV_PROGRESS  19	// can buy/manage progression goals
#define PRIV_DISMANTLE  20	// can dismantle things (not required for ones they built themselves)
#define NUM_PRIVILEGES  21	// total


// SCORE_x: for empire scores (e.g. sorting)
#define SCORE_COMMUNITY  0
#define SCORE_DEFENSE  1
#define SCORE_GREATNESS  2
#define SCORE_INDUSTRY  3
#define SCORE_INVENTORY  4
#define SCORE_MEMBERS  5
#define SCORE_PLAYTIME  6
#define SCORE_PRESTIGE  7
#define SCORE_TERRITORY  8
#define SCORE_WEALTH  9
#define NUM_SCORES  10	// total


// TECH_x: Technologies
	#define TECH_UNUSED0  0	// formerly glassblowing
#define TECH_CITY_LIGHTS  1
#define TECH_LOCKS  2
	#define TECH_UNUSED1  3	// formerly apiaries
#define TECH_SEAPORT  4
#define TECH_WORKFORCE  5
#define TECH_PROMINENCE  6
#define TECH_CITIZENS  7
#define TECH_PORTALS  8
#define TECH_MASTER_PORTALS  9
#define TECH_SKILLED_LABOR  10
#define TECH_TRADE_ROUTES  11
#define TECH_WORKFORCE_PROSPECTING  12
#define TECH_DEEP_MINES  13
#define TECH_RARE_METALS  14
#define TECH_BONUS_EXPERIENCE  15
#define TECH_TUNNELS  16
#define TECH_FAST_PROSPECT  17
#define TECH_FAST_EXCAVATE  18
#define TECH_HIDDEN_PROGRESS  19
#define NUM_TECHS  20


// TER_x: territory types for empire arrays
#define TER_TOTAL  0
#define TER_CITY  1
#define TER_OUTSKIRTS  2
#define TER_FRONTIER  3
#define NUM_TERRITORY_TYPES  4	// total


// for empire_trade_data
#define TRADE_EXPORT  0
#define TRADE_IMPORT  1


// for can_use_room
#define GUESTS_ALLOWED  0
#define MEMBERS_ONLY  1
#define MEMBERS_AND_ALLIES  2


// WF_PROB_x: Workforce problem logging
#define WF_PROB_NO_WORKERS  0	// no citizens available
#define WF_PROB_OVER_LIMIT  1	// too many items
#define WF_PROB_DEPLETED  2	// tile is out of resources
#define WF_PROB_NO_RESOURCES  3	// nothing to craft/build with
#define WF_PROB_ALREADY_SHEARED  4	// mob sheared too recently
#define WF_PROB_DELAYED  5	// delayed by a previous failure
#define WF_PROB_OUT_OF_CITY  6	// building requires in-city
#define WF_PROB_ADVENTURE_PRESENT  7	// blocked by adventure instance


// WPLOG_x: Types for the workforce production log
#define WPLOG_COINS  0
#define WPLOG_OBJECT  1
#define WPLOG_BUILDING_DONE  2
#define WPLOG_BUILDING_DISMANTLED  3
#define WPLOG_VEHICLE_DONE  4
#define WPLOG_VEHICLE_DISMANTLED  5
#define WPLOG_STUMPS_BURNED  6
#define WPLOG_FIRE_EXTINGUISHED  7
#define WPLOG_PROSPECTED  8
#define WPLOG_MAINTENANCE  9


// for tracking playtime
#define PLAYTIME_WEEKS_TO_TRACK  12	// playtime determined by past 12 weeks


 //////////////////////////////////////////////////////////////////////////////
//// EVENT DEFINES ///////////////////////////////////////////////////////////

// EVT_x: event types


// EVTF_x: event flags
#define EVTF_IN_DEVELOPMENT  BIT(0)	// a. quest is not live
#define EVTF_CONTINUES  BIT(1)	// b. event points do not reset when it runs again (it runs on the same id as last time)


// EVTS_x: event status
#define EVTS_NOT_STARTED  0	// default status
#define EVTS_RUNNING  1	// event is active
#define EVTS_COMPLETE  2	// event has ended
#define EVTS_COLLECTED  3	// rewards have been collected (used only on players)


 //////////////////////////////////////////////////////////////////////////////
//// EVENT DEFINES (TIMED EVENT SYSTEM) //////////////////////////////////////

// function types
#define EVENTFUNC(name) long (name)(struct dg_event *the_event, void *event_obj)
#define EVENT_CANCEL_FUNC(name) void (name)(void *event_obj)


// SEV_x: stored event types
#define SEV_TRENCH_FILL  0	// water fills over time
#define SEV_DESPAWN  1	// mob despawn
#define SEV_BURN_DOWN  2	// for buildings
#define SEV_GROW_CROP  3	// normal crop growth time
	#define SEV_UNUSED4  4	// was tavern; now unused
#define SEV_RESET_TRIGGER  5	// for scripting reset triggers
#define SEV_PURSUIT  6	// mob pursuing a target
#define SEV_MOVEMENT  7	// normal mob movement
#define SEV_AGGRO  8	// aggro or cityguard mobs
#define SEV_SCAVENGE  9	// scavenger mobs consume a corpse
#define SEV_VAMPIRE_FEEDING  10	// drinking blood
#define SEV_RESET_MOB  11	// periodic reset of damaged/tagged mobs
#define SEV_HEAL_OVER_TIME  12	// handles HOT applies
#define SEV_CHECK_LEADING  13	// called right after moving, in some cases
#define SEV_OBJ_TIMER  14	// various timer updates
#define SEV_OBJ_AUTOSTORE  15	// autostore check for objects


 //////////////////////////////////////////////////////////////////////////////
//// FACTION DEFINES /////////////////////////////////////////////////////////

// FCT_x: Faction flags
#define FCT_IN_DEVELOPMENT  BIT(0)	// a. not live
#define FCT_REP_FROM_KILLS  BIT(1)	// b. killing mobs affects faction rating
#define FCT_HIDE_IN_LIST  BIT(2)	// c. not shown in player's list
#define FCT_HIDE_ON_MOB  BIT(3)	// d. does not appear under mob's description


// FCTR_x: Relationship flags
#define FCTR_SHARED_GAINS  BIT(0)	// a. also gains rep when that one gains rep
#define FCTR_INVERSE_GAINS  BIT(1)	// b. loses rep when that one gains rep
#define FCTR_MUTUALLY_EXCLUSIVE  BIT(2)	// c. cannot gain if that one is positive
#define FCTR_UNLISTED  BIT(3)  // d. mortals won't see this relationship


// REP_x: Faction reputation levels
#define REP_NONE  0	// base / un-set
#define REP_NEUTRAL  1	// no opinion
#define REP_DESPISED  2	// lower cap
#define REP_HATED  3	// worst
#define REP_LOATHED  4	// bad
#define REP_DISLIKED  5	// not good
#define REP_LIKED  6	// decent
#define REP_ESTEEMED  7	// good
#define REP_VENERATED  8	// great!
#define REP_REVERED  9	// upper cap


 //////////////////////////////////////////////////////////////////////////////
//// GAME DEFINES ////////////////////////////////////////////////////////////

/* Access Level constants *****************************************************
 *
 * How to add immortal levels to your mud:
 *  - Change the level defines here, adding more levels between GOD and IMPL
 *  - Make sure that the levels for commands in interpreter.c are okay
 *  - Unless you have more than three immortal levels, the mud will auto-
 *    magically set the command levels for you.
 *  - Make sure LVL_TOP is no less than 3!
 *  - If you want more levels than those listed in the chart, you'll have to
 *    make necessary adjustments yourself:
 *    = Add a clause in db.player.c, Autowiz section: level_params[]
 *    = Add a clause in constants.c: level_names[][2]
 *    = Change interpreter.c command levels
 *
 *  = Future plans: An option to turn on mortal levels
 *
 * Quick Reference:
 *  Levels    You'll Have
 *  ------    -----------
 *    3           Mortal, God, Implementor
 *    4           Mortal, God, Immortal, Implementor
 *    5           Mortal, God, Immortal, Assistant, Implementor
 *    6           Mortal, God, Immortal, Assistant, Co-Implementor, Implementor
 */

#define LVL_TOP  6	// Highest possible access level
#define LVL_MORTAL  1	// Level when players can use commands -- don't set less than 1

// admin levels
#define LVL_IMPL  LVL_TOP
#define LVL_CIMPL  (LVL_ASST < LVL_TOP ? LVL_ASST + 1 : LVL_ASST)
#define LVL_ASST  (LVL_START_IMM < LVL_TOP ? LVL_START_IMM + 1 : LVL_START_IMM)
#define LVL_START_IMM  (LVL_GOD+1)
#define LVL_GOD  (LVL_MORTAL+1)

// global level configs
#define LVL_TO_SEE_ACCOUNTS  LVL_CIMPL


// PK_x: Player killing options for config_get_bitvector("pk_mode")
#define PK_OPEN					BIT(0)	// a. all pk all the time
#define PK_WAR					BIT(1)	// b. pk when at war
#define PK_TRESPASSERS			BIT(2)	// c. when someone is on your land (or an ally's)
#define PK_DISTRUST				BIT(3)	// d. when in a state of distrust
#define PK_EMPIRE_OFFENSES		BIT(4)	// e. empire has any offenses against you
#define PK_PERSONAL_OFFENSES	BIT(5)	// f. character has any offenses against you


// mud-life time
#define SECS_PER_MUD_HOUR  75
#define SECS_PER_MUD_DAY  (24 * SECS_PER_MUD_HOUR)
#define SECS_PER_MUD_MONTH  (30 * SECS_PER_MUD_DAY)
#define SECS_PER_MUD_YEAR  (12 * SECS_PER_MUD_MONTH)

/* real-life time (remember Real Life?) */
#define SECS_PER_REAL_MIN  60
#define SECS_PER_REAL_HOUR  (60 * SECS_PER_REAL_MIN)
#define SECS_PER_REAL_DAY  (24 * SECS_PER_REAL_HOUR)
#define SECS_PER_REAL_WEEK  (7 * SECS_PER_REAL_DAY)
#define SECS_PER_REAL_YEAR  (365 * SECS_PER_REAL_DAY)

// this is based on the number of 5-second ticks in a mud hour
#define SECS_PER_REAL_UPDATE  5
#define REAL_UPDATES_PER_MUD_HOUR  (SECS_PER_MUD_HOUR / SECS_PER_REAL_UPDATE)
// #define REAL_UPDATES_PER_MIN  (SECS_PER_REAL_MIN / SECS_PER_REAL_UPDATE)	// this is unused as of b5.152
// #define MUD_HOURS  *REAL_UPDATES_PER_MUD_HOUR	// this is unused as of b5.152


// misc game configs
#define ACTION_CYCLE_TIME  5	// seconds per action tick (before haste) -- TODO should this be a config?
#define ACTION_CYCLE_MULTIPLIER  10	// make action cycles longer so things can make them go faster
#define ACTION_CYCLE_SECOND  2	// how many action cycles is 1 second
#define ACTION_CYCLE_HALF_SEC  1	// how many action cycles is half a second
#define DOT_INTERVAL  5	// seconds per tick for damage-over-time
#define HISTORY_SIZE  5	// Keep last 5 commands.
#define MOB_RESTORE_INTERVAL  60	// seconds between when a mob loses health and when it starts checking to restore itself
#define WORKFORCE_CYCLE  75	// seconds between workforce chore updates
#define WORKFORCE_LOG_AND_NEEDS_CYCLE  (30 * SECS_PER_REAL_MIN)	// how often it will update workforce logs and needs


// System timing
#define OPT_USEC  100000	// 10 passes per second
#define PASSES_PER_SEC  (1000000 / OPT_USEC)
#define PASSES_PER_MUD_HOUR     (SECS_PER_MUD_HOUR*PASSES_PER_SEC)
#define RL_SEC  * PASSES_PER_SEC
#define SEC_MICRO  *1000000	// convert seconds to microseconds for microtime()


// Variables for the output buffering system
#define MAX_SOCK_BUF  (24 * 1024)	// Size of kernel's sock buf
#define MAX_PROMPT_LENGTH  275	// Max length of rendered prompt
#define GARBAGE_SPACE  32	// Space for **OVERFLOW** etc
#define SMALL_BUFSIZE  8192	// Static output buffer size
// Max amount of output that can be buffered
#define LARGE_BUFSIZE  (MAX_SOCK_BUF - GARBAGE_SPACE - MAX_PROMPT_LENGTH)


// REBOOT_x: reboot modes
#define REBOOT_NONE			0	// no reboot is set
#define REBOOT_REBOOT		1	// reboot mode
#define REBOOT_SHUTDOWN		2	// shutdown mode

#define REBOOT_IS_SET()		(reboot_control.type != REBOOT_NONE && (reboot_control.immediate || reboot_control.time >= 0))


// SHUTDOWN_x: shutdown types
#define SHUTDOWN_NORMAL  0	// comes up normally
#define SHUTDOWN_PAUSE  1	// writes a pause file which must be removed
#define SHUTDOWN_DIE  2	// kills the autorun
#define SHUTDOWN_COMPLETE  3	// like die but also frees all memory


 //////////////////////////////////////////////////////////////////////////////
//// GENERIC DEFINES /////////////////////////////////////////////////////////

// GENERIC_x: generic types
#define GENERIC_UNKNOWN  0	// dummy
#define GENERIC_LIQUID  1	// for drink containers
#define GENERIC_ACTION  2	// for resource actions
#define GENERIC_COOLDOWN  3	// for cooldowns (COOLDOWN_*)!
#define GENERIC_AFFECT  4	// for affects (ATYPE_*)
#define GENERIC_CURRENCY  5	// tokens, for shops
#define GENERIC_COMPONENT  6	// types of generic objects
#define GENERIC_MOON  7	// moon in the sky
#define GENERIC_LANGUAGE  8	// language a player can speak


// GEN_x: generic flags
#define GEN_BASIC  BIT(0)	// a. indicates it's basic (varies by type)
#define GEN_IN_DEVELOPMENT  BIT(1)	// b. disables SOME types -- see generic_types_uses_in_dev[]
#define GEN_SHOW_ADVENTURE  BIT(2)	// c. for some types, shows what adventure they came from


// how many strings a generic stores (can be safely raised with no updates)
#define NUM_GENERIC_STRINGS  10

// how many ints a generic stores (update write_generic_to_file if you change this)
#define NUM_GENERIC_VALUES  4


// LANG_x: how well someone speaks a language
#define LANG_UNKNOWN  0	// default: does not speak it, cannot recognize it
#define LANG_RECOGNIZE  1	// knows which language it is, but can't speak it
#define LANG_SPEAK  2	// full comprehension and speaking


// LIQF_x: flags for liquids
#define LIQF_WATER  BIT(0)	// counts as water for certain commands
#define LIQF_COOLING  BIT(1)	// cools down the player, if warm
#define LIQF_WARMING  BIT(2)	// warms up the player, if cold
#define LIQF_BLOOD  BIT(3)	// counts as blood for vampires
// BIT(31) limit: this is stored as an int (in the generic values)


 //////////////////////////////////////////////////////////////////////////////
//// MOBILE DEFINES //////////////////////////////////////////////////////////

// MOB_x: Mobile flags 
#define MOB_BRING_A_FRIEND  BIT(0)	// a. Mob auto-assists any other (!charmed) mob
#define MOB_SENTINEL  BIT(1)	// b. Mob should not move
#define MOB_AGGRESSIVE  BIT(2)	// c. Mob hits players in the room
#define MOB_ISNPC  BIT(3)	// d. (R) Auto-set on all mobs
#define MOB_MOUNTABLE  BIT(4)	// e. Can be ridden
#define MOB_MILKABLE  BIT(5)	// f. May be milked
#define MOB_SCAVENGER  BIT(6)	// g. Eats corpses
#define MOB_NO_CORPSE  BIT(7)	// h. Does not leave a corpse when killed (this flag was formerly called UNDEAD)
#define MOB_TIED  BIT(8)	// i. (R) Mob is tied up
#define MOB_ANIMAL  BIT(9)	// j. mob is an animal
#define MOB_MOUNTAINWALK  BIT(10)	// k. Walks on mountains
#define MOB_AQUATIC  BIT(11)	// l. Mob lives in the water only
#define MOB_PLURAL  BIT(12)	// m. Mob represents 2+ creatures
#define MOB_NO_ATTACK  BIT(13)	// n. the mob can be in combat, but will never hit
#define MOB_SPAWNED  BIT(14)	// o. Mob was spawned and should despawn if nobody is around
#define MOB_CHAMPION  BIT(15)	// p. Mob auto-rescues its leader
#define MOB_EMPIRE  BIT(16)	// q. empire NPC
	#define MOB_UNUSED  BIT(17)	// formerly 'familiar'
#define MOB_PICKPOCKETED  BIT(18)	// s. has already been pickpocketed
#define MOB_CITYGUARD  BIT(19)	// t. assists empiremates
#define MOB_PURSUE  BIT(20)	// u. mob remembers and pursues players
#define MOB_HUMAN  BIT(21)	// v. mob is a human
#define MOB_VAMPIRE  BIT(22)	// w. mob is a vampire
#define MOB_CASTER  BIT(23)	// x. mob has mana
#define MOB_TANK  BIT(24)	// y. mob has more health than normal
#define MOB_DPS  BIT(25)	// z. mob deals more damage than normal
#define MOB_HARD  BIT(26)	// A. more difficult than normal
#define MOB_GROUP  BIT(27)	// B. much more difficult than normals, requires group
#define MOB_EXTRACTED  BIT(28)	// C. for late-extraction
#define MOB_NO_LOOT  BIT(29)	// D. do not drop loot
#define MOB_NO_TELEPORT  BIT(30)  // E. cannot teleport to this mob
#define MOB_NO_EXPERIENCE  BIT(31)	// F. players get no exp against this mob
#define MOB_NO_RESCALE  BIT(32)	// G. mob won't rescale (after the first time), e.g. if specific traits were set
#define MOB_SILENT  BIT(33)	// H. will not set off custom strings
#define MOB_COINS  BIT(34)	// I. mob drops coins on death/pickpocket
#define MOB_NO_COMMAND  BIT(35)	// J. mob cannot be commanded/ordered
#define MOB_NO_UNCONSCIOUS  BIT(36)	// K. mob cannot be knocked out; it's always killed instead
#define MOB_IMPORTANT  BIT(37)	// L. won't be hit by no-arg "purge"; can be used by scripts


// MOB_CUSTOM_x: custom message types
#define MOB_CUSTOM_ECHO  0
#define MOB_CUSTOM_SAY  1
#define MOB_CUSTOM_SAY_DAY  2
#define MOB_CUSTOM_SAY_NIGHT  3
#define MOB_CUSTOM_ECHO_DAY  4
#define MOB_CUSTOM_ECHO_NIGHT  5
#define MOB_CUSTOM_LONG_DESC  6	// random long descs
#define MOB_CUSTOM_SCRIPT_1  7	// called by scripts
#define MOB_CUSTOM_SCRIPT_2  8	// called by scripts
#define MOB_CUSTOM_SCRIPT_3  9	// called by scripts
#define MOB_CUSTOM_SCRIPT_4  10	// called by scripts
#define MOB_CUSTOM_SCRIPT_5  11	// called by scripts
#define MOB_CUSTOM_SCAVENGE_CORPSE  12	// mob eats a corpse due to SCAVENGER flag


// MOB_MOVE_x: mob/vehicle movement types
#define MOB_MOVE_WALK  0
#define MOB_MOVE_CLIMB  1
#define MOB_MOVE_FLY  2
#define MOB_MOVE_PADDLE  3
#define MOB_MOVE_RIDE  4
#define MOB_MOVE_SLITHER  5
#define MOB_MOVE_SWIM  6
#define MOB_MOVE_SCURRIES  7
#define MOB_MOVE_SKITTERS  8
#define MOB_MOVE_CREEPS  9
#define MOB_MOVE_OOZES  10
#define MOB_MOVE_RUNS  11
#define MOB_MOVE_GALLOPS  12
#define MOB_MOVE_SHAMBLES  13
#define MOB_MOVE_TROTS  14
#define MOB_MOVE_HOPS  15
#define MOB_MOVE_WADDLES  16
#define MOB_MOVE_CRAWLS  17
#define MOB_MOVE_FLUTTERS  18
#define MOB_MOVE_DRIVES  19
#define MOB_MOVE_SAILS  20
#define MOB_MOVE_ROLLS  21
#define MOB_MOVE_RATTLES  22
#define MOB_MOVE_SKIS  23
#define MOB_MOVE_SLIDES  24
#define MOB_MOVE_SOARS  25
#define MOB_MOVE_LUMBERS  26
#define MOB_MOVE_FLOATS  27
#define MOB_MOVE_LOPES  28
#define MOB_MOVE_BLOWS  29
#define MOB_MOVE_DRIFTS  30
#define MOB_MOVE_BOUNCES  31
#define MOB_MOVE_FLOWS  32
#define MOB_MOVE_LEAVES  33
#define MOB_MOVE_SHUFFLES  34
#define MOB_MOVE_MARCHES  35
#define MOB_MOVE_SWEEPS  36
#define MOB_MOVE_BARGES  37
#define MOB_MOVE_BOLTS  38
#define MOB_MOVE_CHARGES  39
#define MOB_MOVE_CLAMBERS  40
#define MOB_MOVE_COASTS  41
#define MOB_MOVE_DARTS  42
#define MOB_MOVE_DASHES  43
#define MOB_MOVE_DRAWS  44
#define MOB_MOVE_FLITS  45
#define MOB_MOVE_GLIDES  46
#define MOB_MOVE_GOES  47
#define MOB_MOVE_HIKES  48
#define MOB_MOVE_HOBBLES  49
#define MOB_MOVE_HURRIES  50
#define MOB_MOVE_INCHES  51
#define MOB_MOVE_JOGS  52
#define MOB_MOVE_JOURNEYS  53
#define MOB_MOVE_JUMPS  54
#define MOB_MOVE_LEAPS  55
#define MOB_MOVE_LIMPS  56
#define MOB_MOVE_LURCHES  57
#define MOB_MOVE_MEANDERS  58
#define MOB_MOVE_MOSEYS  59
#define MOB_MOVE_PARADES  60
#define MOB_MOVE_PLODS  61
#define MOB_MOVE_PRANCES  62
#define MOB_MOVE_PROWLS  63
#define MOB_MOVE_RACES  64
#define MOB_MOVE_ROAMS  65
#define MOB_MOVE_ROMPS  66
#define MOB_MOVE_ROVES  67
#define MOB_MOVE_RUSHES  68
#define MOB_MOVE_SASHAYS  69
#define MOB_MOVE_SAUNTERS  70
#define MOB_MOVE_SCAMPERS  71
#define MOB_MOVE_SCOOTS  72
#define MOB_MOVE_SCRAMBLES  73
#define MOB_MOVE_SCUTTERS  74
#define MOB_MOVE_SIDLES  75
#define MOB_MOVE_SKIPS  76
#define MOB_MOVE_SKULKS  77
#define MOB_MOVE_SLEEPWALKS  78
#define MOB_MOVE_SLINKS  79
#define MOB_MOVE_SLOGS  80
#define MOB_MOVE_SNEAKS  81
#define MOB_MOVE_STAGGERS  82
#define MOB_MOVE_STOMPS  83
#define MOB_MOVE_STREAKS  84
#define MOB_MOVE_STRIDES  85
#define MOB_MOVE_STROLLS  86
#define MOB_MOVE_STRUTS  87
#define MOB_MOVE_STUMBLES  88
#define MOB_MOVE_SWIMS  89
#define MOB_MOVE_TACKS  90
#define MOB_MOVE_TEARS  91
#define MOB_MOVE_TIPTOES  92
#define MOB_MOVE_TODDLES  93
#define MOB_MOVE_TOTTERS  94
#define MOB_MOVE_TRAIPSES  95
#define MOB_MOVE_TRAMPS  96
#define MOB_MOVE_TRAVELS  97
#define MOB_MOVE_TREKS  98
#define MOB_MOVE_TRUDGES  99
#define MOB_MOVE_VAULTS  100
#define MOB_MOVE_WADES  101
#define MOB_MOVE_WANDERS  102
#define MOB_MOVE_WHIZZES  103
#define MOB_MOVE_ZIGZAGS  104
#define MOB_MOVE_ZOOMS  105


// NAMES_x: name sets: add matching files in lib/text/names/
#define NAMES_CITIZENS  0
#define NAMES_COUNTRYFOLK  1
#define NAMES_ROMAN  2
#define NAMES_NORTHERN  3
#define NAMES_PRIMITIVE_SHORT  4
#define NAMES_DESCRIPTIVE  5


 //////////////////////////////////////////////////////////////////////////////
//// MOON DEFINES ////////////////////////////////////////////////////////////

// PHASE_x: moon phases
typedef enum {
	PHASE_NEW,
	PHASE_WAXING_CRESCENT,
	PHASE_FIRST_QUARTER,
	PHASE_WAXING_GIBBOUS,
	PHASE_FULL,
	PHASE_WANING_GIBBOUS,
	PHASE_THIRD_QUARTER,
	PHASE_WANING_CRESCENT,
	
	NUM_PHASES	// this goes last
} moon_phase_t;


// MOON_POS_x: moon position in the sky
typedef enum {
	MOON_POS_DOWN,	// not visible
	MOON_POS_RISING,	// low in the east
	MOON_POS_EAST,	// high in the east
	MOON_POS_HIGH,	// overhead
	MOON_POS_WEST,	// high in the west
	MOON_POS_SETTING,	// low in the west
	
	NUM_MOON_POS	// this goes last
} moon_pos_t;


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT DEFINES //////////////////////////////////////////////////////////

// APPLY_TYPE_x: What type of obj apply it is
#define APPLY_TYPE_NATURAL  0	// built-in trait
#define APPLY_TYPE_ENCHANTMENT  1	// caused by enchant
#define APPLY_TYPE_HONED  2	// Trade ability
#define APPLY_TYPE_SUPERIOR  3	// only applies if the item is SUPERIOR when scaled
#define APPLY_TYPE_HARD_DROP  4	// only applies if the item is HARD-DROP when scaled
#define APPLY_TYPE_GROUP_DROP  5	// only applies if the item is GROUP-DROP when scaled
#define APPLY_TYPE_BOSS_DROP  6	// only applies if the item is GROUP- and HARD-DROP both when scaled


// Container flags -- limited to 31 because of int type in obj value
#define CONT_CLOSEABLE  BIT(0)	// Container can be closed
#define CONT_CLOSED  BIT(1)	// Container is closed


// CORPSE_x: Corpse flags -- limited to 31 because of int type in obj value
#define CORPSE_EATEN  BIT(0)	// The corpse is part-eaten
#define CORPSE_SKINNED  BIT(1)	// The corpse has been skinned
#define CORPSE_HUMAN  BIT(2)	// a person
#define CORPSE_NO_LOOT  BIT(3)	// cannot butcher/skin due to !LOOT flag


// ITEM_x: Item types
#define ITEM_UNDEFINED  0
#define ITEM_WEAPON  1	// item is a weapon
#define ITEM_WORN  2	// wearable equipment
#define ITEM_OTHER  3	// misc object
#define ITEM_CONTAINER  4	// item is a container
#define ITEM_DRINKCON  5	// item is a drink container
#define ITEM_FOOD  6	// item is food
#define ITEM_RECIPE  7	// can be learned for a craft
#define ITEM_PORTAL  8  // a portal
#define ITEM_BOARD  9	// message board
#define ITEM_CORPSE  10	// a corpse, pc or npc
#define ITEM_COINS  11	// stack of coins
#define ITEM_CURRENCY  12	// adventure currency item
#define ITEM_PAINT  13	// for painting buildings
#define ITEM_MAIL  14	// mail
#define ITEM_WEALTH  15	// item provides wealth
#define ITEM_CART  16	// This type is mostly DEPRECATED; use vehicles instead
#define ITEM_SHIP  17	// This type is mostly DEPRECATED; use vehicles instead
#define ITEM_LIGHTER  18	// can be used to light fires
#define ITEM_MINIPET  19	// grants a minipet when 'use'd
#define ITEM_MISSILE_WEAPON  20	// bow/crossbow/etc
#define ITEM_AMMO  21	// for missile weapons
#define ITEM_INSTRUMENT  22	// item is a musical instrument
#define ITEM_SHIELD  23	// item is a shield
#define ITEM_PACK  24	// increases inventory size
#define ITEM_POTION  25	// quaffable
#define ITEM_POISON  26	// poison vial
#define ITEM_ARMOR  27	// armor!
#define ITEM_BOOK  28	// tied to the book/library system
#define ITEM_LIGHT  29	// item is a light (torch, etc)


// ITEM_WEAR_x: Take/Wear flags -- where an item can be worn
// NOTE: Don't confuse these with the actual wear locations (WEAR_x)
#define ITEM_WEAR_TAKE  BIT(0)	// a. Item can be taken
#define ITEM_WEAR_FINGER  BIT(1)	// b. Can be worn on finger
#define ITEM_WEAR_NECK  BIT(2)	// c. Can be worn around neck
#define ITEM_WEAR_CLOTHES  BIT(3)	// d. Can be worn as clothes
#define ITEM_WEAR_HEAD  BIT(4)	// e. Can be worn on head
#define ITEM_WEAR_LEGS  BIT(5)	// f. Can be worn on legs
#define ITEM_WEAR_FEET  BIT(6)	// g. Can be worn on feet
#define ITEM_WEAR_HANDS  BIT(7)	// h. Can be worn on hands
#define ITEM_WEAR_ARMS  BIT(8)	// i. Can be worn on arms
#define ITEM_WEAR_WRISTS  BIT(9)	// j. Can be worn on wrists
#define ITEM_WEAR_ABOUT  BIT(10)	// k. Can be worn about body
#define ITEM_WEAR_WAIST  BIT(11)	// l. Can be worn around waist
#define ITEM_WEAR_EARS  BIT(12)	// m. Worn on ears
#define ITEM_WEAR_WIELD  BIT(13)	// n. Can be wielded
#define ITEM_WEAR_HOLD  BIT(14)	// o. Can be held
#define ITEM_WEAR_RANGED  BIT(15)	// p. Can be used as ranged weapon
#define ITEM_WEAR_ARMOR  BIT(16)	// q. Can be worn as armor
#define ITEM_WEAR_PACK  BIT(17)	// r. Can use as pack
#define ITEM_WEAR_SADDLE  BIT(18)	// s. Saddle


// LIGHT_FLAG_x (possibly also LIGHT_x as a search hint): flags for ITEM_LIGHT
#define LIGHT_FLAG_LIGHT_FIRE  BIT(0)	// It can be used in place of a lighter
#define LIGHT_FLAG_CAN_DOUSE  BIT(1)	// It can be put out
#define LIGHT_FLAG_JUNK_WHEN_EXPIRED  BIT(2)	// automatically removed
#define LIGHT_FLAG_COOKING_FIRE  BIT(3)	// allows cooking
#define LIGHT_FLAG_DESTROY_WHEN_DOUSED  BIT(4)	// always expires when doused


// Item materials
#define MAT_WOOD  0	// Made from wood
#define MAT_ROCK  1	// ...rock
#define MAT_IRON  2	// ...iron
#define MAT_SILVER  3	// ...sparkly stuff
#define MAT_GOLD  4	// ...gold
#define MAT_FLINT  5	// ...flint
#define MAT_CLAY  6	// ...clay
#define MAT_FLESH  7	// ...skin, corpse, etc
#define MAT_GLASS  8	// ...glass
#define MAT_WAX  9	// ...wax
#define MAT_MAGIC  10	// ...magic
#define MAT_CLOTH  11	// ...cloth
#define MAT_GEM  12	// ...gem
#define MAT_COPPER  13	// ...copper
#define MAT_BONE  14	// ...bone
#define MAT_HAIR  15	// ...hair
#define NUM_MATERIALS  16	// Total number of matierals


// MINT_FLAG_x: flags for ITEM_WEALTH
#define MINT_FLAG_AUTOMINT  BIT(0)	// Workforce will mint it
#define MINT_FLAG_NO_MINT  BIT(1)	// item cannot be minted


// OBJ_x: Extra object flags -- OBJ_FLAGGED(obj, f)
#define OBJ_UNIQUE  BIT(0)	// a. can only use 1 at a time
#define OBJ_PLANTABLE  BIT(1)	// b. Uses val 2 to set a crop type
#define OBJ_LIGHT  BIT(2)	// c. Generates light (prefer LIGHT item type tho)
#define OBJ_SUPERIOR  BIT(3)	// d. Item is of superior quality
#define OBJ_LARGE  BIT(4)	// e. Item can't be put in bags
#define OBJ_CREATED  BIT(5)	// f. Was created by a god
#define OBJ_SINGLE_USE 	BIT(6)	// g. decays when removed
#define OBJ_SLOW  BIT(7)	// h. weapon attacks slower
#define OBJ_FAST  BIT(8)	// i. weapon attacks faster
#define OBJ_ENCHANTED  BIT(9)	// j. magically enchanted
#define OBJ_JUNK  BIT(10)	// k. item is junk
#define OBJ_CREATABLE  BIT(11)	// l. item can be created using "create"
#define OBJ_SCALABLE  BIT(12)	// m. item an be auto-scaled
#define OBJ_TWO_HANDED  BIT(13)	// n. weapon requires both hands
#define OBJ_BIND_ON_EQUIP  BIT(14)	// o. binds when equipped
#define OBJ_BIND_ON_PICKUP  BIT(15)	// p. binds when acquired
//	#define OBJ_UNUSED1  BIT(16)	// q. formerly STAFF
#define OBJ_UNCOLLECTED_LOOT  BIT(17)	// r. will junk instead of autostore
#define OBJ_KEEP  BIT(18)	// s. obj will not be part of any "all" commands like "drop all"
//	#define OBJ_UNUSED2  BIT(19)	// t. formerly TOOL-PAN
//	#define OBJ_UNUSED3  BIT(20)	// u. formerly TOOL-SHOVEL
#define OBJ_NO_AUTOSTORE  BIT(21)	// v. keeps the game from cleaning it up
#define OBJ_HARD_DROP  BIT(22)	// w. dropped by a 'hard' mob
#define OBJ_GROUP_DROP  BIT(23)	// x. dropped by a 'group' mob
#define OBJ_GENERIC_DROP  BIT(24)	// y. blocks the hard/group drop flags
#define OBJ_NO_BASIC_STORAGE  BIT(25)	// z. cannot be stored in basic storage anymore
#define OBJ_SEEDED  BIT(26)	// A. has already been seeded
#define OBJ_IMPORTANT  BIT(27)	// B. prevents casual purging; can be used by scripts
#define OBJ_LONG_TIMER_IN_STORAGE  BIT(28)	// C. decays more slowly when stored
#define OBJ_NO_WAREHOUSE  BIT(29)	// D. cannot be stored in the home/warehouse anymore

#define OBJ_BIND_FLAGS  (OBJ_BIND_ON_EQUIP | OBJ_BIND_ON_PICKUP)	// all bind-on flags
#define OBJ_PRESERVE_FLAGS  (OBJ_HARD_DROP | OBJ_GROUP_DROP | OBJ_SUPERIOR | OBJ_KEEP | OBJ_NO_BASIC_STORAGE | OBJ_NO_WAREHOUSE | OBJ_SEEDED | OBJ_BIND_FLAGS | OBJ_IMPORTANT)	// flags that are preserved


// OBJ_CUSTOM_x: custom message types
#define OBJ_CUSTOM_BUILD_TO_CHAR  0
#define OBJ_CUSTOM_BUILD_TO_ROOM  1
#define OBJ_CUSTOM_INSTRUMENT_TO_CHAR  2
#define OBJ_CUSTOM_INSTRUMENT_TO_ROOM  3
#define OBJ_CUSTOM_CONSUME_TO_CHAR  4
#define OBJ_CUSTOM_CONSUME_TO_ROOM  5
#define OBJ_CUSTOM_CRAFT_TO_CHAR  6
#define OBJ_CUSTOM_CRAFT_TO_ROOM  7
#define OBJ_CUSTOM_WEAR_TO_CHAR  8
#define OBJ_CUSTOM_WEAR_TO_ROOM  9
#define OBJ_CUSTOM_REMOVE_TO_CHAR  10
#define OBJ_CUSTOM_REMOVE_TO_ROOM  11
#define OBJ_CUSTOM_LONGDESC  12
#define OBJ_CUSTOM_LONGDESC_FEMALE  13
#define OBJ_CUSTOM_LONGDESC_MALE  14
#define OBJ_CUSTOM_FISH_TO_CHAR  15
#define OBJ_CUSTOM_FISH_TO_ROOM  16
#define OBJ_CUSTOM_DECAYS_ON_CHAR  17	// worn/held
#define OBJ_CUSTOM_DECAYS_IN_ROOM  18	// everywhere else
#define OBJ_CUSTOM_RESOURCE_TO_CHAR  19  // when gained as a resource
#define OBJ_CUSTOM_RESOURCE_TO_ROOM  20  // when gained as a resource
#define OBJ_CUSTOM_SCRIPT_1  21	// called by scripts
#define OBJ_CUSTOM_SCRIPT_2  22	// called by scripts
#define OBJ_CUSTOM_SCRIPT_3  23	// called by scripts
#define OBJ_CUSTOM_SCRIPT_4  24	// called by scripts
#define OBJ_CUSTOM_SCRIPT_5  25	// called by scripts
#define OBJ_CUSTOM_MINE_TO_CHAR  26
#define OBJ_CUSTOM_MINE_TO_ROOM  27
#define OBJ_CUSTOM_CHOP_TO_CHAR  28
#define OBJ_CUSTOM_CHOP_TO_ROOM  29
#define OBJ_CUSTOM_ENTER_PORTAL_TO_CHAR  30
#define OBJ_CUSTOM_ENTER_PORTAL_TO_ROOM  31
#define OBJ_CUSTOM_EXIT_PORTAL_TO_ROOM  32


// RES_x: resource requirement types
#define RES_OBJECT  0	// specific obj (vnum= obj vnum, misc= scale level [refunds only])
#define RES_COMPONENT  1	// an obj of a given 'generic component' type (e.g. generic vnum 6000)
#define RES_LIQUID  2	// a volume of a given liquid (vnum= LIQ_ vnum)
#define RES_COINS  3	// an amount of coins (vnum= empire id of coins)
#define RES_POOL  4	// health, mana, etc (vnum= HEALTH, etc)
#define RES_ACTION  5	// flavorful action strings (take time but not resources)
#define RES_CURRENCY  6	// adventure currencies (generics)
#define RES_TOOL  7	// must have a tool of the given type (will be used up)


// storage flags (for obj storage locations)
#define STORAGE_WITHDRAW  BIT(0)	// requires withdraw privilege


// TOOL_x: tool flags for objects
#define TOOL_AXE  BIT(0)	// a. required to 'chop' trees
#define TOOL_FISHING  BIT(1)	// b. required for 'fish' command
#define TOOL_HAMMER  BIT(2)	// c. required for 'forge' command
#define TOOL_HARVESTING  BIT(3)	// d. required for 'harvest'
#define TOOL_KNAPPER  BIT(4)	// e. required to 'chip'
#define TOOL_KNIFE  BIT(5)	// f. required for the skin/butcher commands
#define TOOL_LOOM  BIT(6)	// g. allows the 'weave' command anywhere
#define TOOL_MINING  BIT(7)	// h. required for the 'mine' command
#define TOOL_PAN  BIT(8)	// i. required for 'pan' command
#define TOOL_GRINDING_STONE  BIT(9)	// j. mill without a mill building
#define TOOL_QUARRYING  BIT(10)	// k. required for the 'quarry' command
#define TOOL_SAW  BIT(11)	// l. used to 'saw' lumber
#define TOOL_SEWING_KIT  BIT(12)	// m. required for 'sew' command unless at a tailor
#define TOOL_SHEARS  BIT(13)	// n. required for shearing
#define TOOL_SHOVEL  BIT(14)	// o. speeds digging and is required to 'excavate'
#define TOOL_STAFF  BIT(15)	// p. counts as a staff (usually for magic)


// WEAR_x: Character equipment positions
/* NOTE: Don't confuse these constants with the ITEM_ bitvectors
   which control the valid places you can wear a piece of equipment */
#define WEAR_HEAD  0
#define WEAR_EARS  1
#define WEAR_NECK_1  2
#define WEAR_NECK_2  3
#define WEAR_CLOTHES  4
#define WEAR_ARMOR  5
#define WEAR_ABOUT  6
#define WEAR_ARMS  7
#define WEAR_WRISTS  8
#define WEAR_HANDS  9
#define WEAR_FINGER_R  10
#define WEAR_FINGER_L  11
#define WEAR_WAIST  12
#define WEAR_LEGS  13
#define WEAR_FEET  14
#define WEAR_PACK  15
#define WEAR_SADDLE  16
#define WEAR_SHEATH_1  17
#define WEAR_SHEATH_2  18
#define WEAR_WIELD  19
#define WEAR_RANGED  20
#define WEAR_HOLD  21
#define WEAR_TOOL  22
#define WEAR_SHARE  23
#define NUM_WEARS  24	/* This must be the # of eq positions!! */


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER DEFINES //////////////////////////////////////////////////////////

// ACCT_x: Account flags
#define ACCT_FROZEN  BIT(0)	// a. unable to play
#define ACCT_MUTED  BIT(1)	// b. unable to use public channels
#define ACCT_SITEOK  BIT(2)	// c. site-cleared for partial bans
#define ACCT_NOTITLE  BIT(3)	// d. cannot change own title
#define ACCT_MULTI_IP  BIT(4)	// e. can log in at the same time as other accounts on the same IP
#define ACCT_MULTI_CHAR  BIT(5)	// f. can log in more than one character on this account
#define ACCT_APPROVED  BIT(6)	// g. approved for full gameplay
#define ACCT_NOCUSTOMIZE  BIT(7)	// h. cannot use 'customize'

// list of account flags that are visible to low-level imms:
#define VISIBLE_ACCT_FLAGS  (ACCT_FROZEN | ACCT_MUTED | ACCT_NOTITLE | ACCT_APPROVED | ACCT_NOCUSTOMIZE)


// ACT_x: Periodic actions -- WARNING: changing the order of these will have tragic consequences with saved players
#define ACT_NONE			0
#define ACT_DIGGING			1
#define ACT_GATHERING		2
#define ACT_CHOPPING		3
#define ACT_BUILDING		4
#define ACT_DISMANTLING		5
#define ACT_HARVESTING		6
#define ACT_PLANTING		7
#define ACT_MINING			8
#define ACT_MINTING			9
#define ACT_FISHING			10
#define ACT_START_FILLIN	11
#define ACT_REPAIR_VEHICLE	12
#define ACT_CHIPPING		13
#define ACT_PANNING			14
#define ACT_MUSIC			15
#define ACT_EXCAVATING		16
#define ACT_SIRING			17
#define ACT_PICKING			18
#define ACT_MORPHING		19
#define ACT_SCRAPING		20
	#define ACT_BATHING			21	// deprecated as of b5.171
	#define ACT_CHANTING		22	// deprecated as of b5.166
#define ACT_PROSPECTING		23
#define ACT_FILLING_IN		24
#define ACT_RECLAIMING		25
	#define ACT_ESCAPING		26	// deprecated as of b5.166
#define ACT_RUNNING			27
	#define ACT_RITUAL			28	// deprecated as of b5.166
#define ACT_SAWING			29
#define ACT_QUARRYING		30
#define ACT_DRIVING			31
#define ACT_TANNING			32
#define ACT_READING			33
#define ACT_COPYING_BOOK	34
#define ACT_GEN_CRAFT		35
#define ACT_SAILING			36
#define ACT_PILOTING		37
#define ACT_SWAP_SKILL_SETS	38
#define ACT_MAINTENANCE		39
#define ACT_BURN_AREA		40
#define ACT_HUNTING			41
#define ACT_FORAGING		42
#define ACT_DISMANTLE_VEHICLE  43
#define ACT_OVER_TIME_ABILITY  44

// ACTF_x: act flags
#define ACTF_ANYWHERE  BIT(0)	// movement won't break it
#define ACTF_HASTE  BIT(1)	// haste increases speed
#define ACTF_FAST_CHORES  BIT(2)  // fast-chores increases speed
#define ACTF_SHOVEL  BIT(3)	// shovel increases speed
#define ACTF_FINDER  BIT(4)	// finder increases speed
#define ACTF_ALWAYS_FAST  BIT(5)	// this action is always faster
#define ACTF_SITTING  BIT(6)	// can be sitting
#define ACTF_FASTER_BONUS  BIT(7)	// speed boost from starting bonus
#define ACTF_FAST_PROSPECT  BIT(8)	// empire tech boosts speed
#define ACTF_FAST_EXCAVATE  BIT(9)	// empire tech boosts speed, when in-city
#define ACTF_VEHICLE_SPEEDS BIT(10)  // signals that this action accelerates based on vehicle speeds
#define ACTF_EVEN_FASTER  BIT(11)	// another speed boost for various actions
#define ACTF_IGNORE_COND  BIT(12)	// not slowed by hunger/thirst/slow
#define ACTF_FIGHTING  BIT(13)	// may be performed in combat
#define ACTF_RESTING  BIT(14)	// may be used while resting


// BONUS_x: bonus traits
#define BONUS_STRENGTH  BIT(0)	// "big boned"
#define BONUS_DEXTERITY  BIT(1)	// "double-jointed"
#define BONUS_CHARISMA  BIT(2)	// "friendly"
#define BONUS_GREATNESS  BIT(3)	// "upper class"
#define BONUS_INTELLIGENCE  BIT(4)	// "literate upbringing"
#define BONUS_WITS  BIT(5)	// "quick-witted"
#define BONUS_HEALTH  BIT(6)	// "thick skinned"
#define BONUS_MOVES  BIT(7)	// "traveller"
#define BONUS_MANA  BIT(8)	// "mana vessel"
#define BONUS_LIGHT_RADIUS  BIT(9)	// "twilight sight" - larger light radius
#define BONUS_MOVE_REGEN  BIT(10)	// "unusual stamina"
#define BONUS_MANA_REGEN  BIT(11)	// "channeler"
#define BONUS_FAST_CHORES  BIT(12)	// "work ethic"
#define BONUS_EXTRA_DAILY_SKILLS  BIT(13)	// "quick learner"
#define BONUS_INVENTORY  BIT(14)	// "pack mule" - bonus inventory
#define BONUS_FASTER  BIT(15)	// "nimble" - shorter movement penalty
#define BONUS_BLOOD  BIT(16)	// "rich veins" - larger blood pool
#define BONUS_CLOCK  BIT(17)	// "sense of time" - can tell time without a ptech
#define BONUS_NO_THIRST  BIT(18)	// "salt blooded" - never thirsty
#define BONUS_NO_HUNGER  BIT(19)	// "tenacious waif" - never hungry
#define BONUS_VIEW_HEIGHT  BIT(20)	// "surveyor" - +1 view height
#define BONUS_WARM_RESIST  BIT(21)	// "fireborn" - tolerant of warm climates
#define BONUS_COLD_RESIST  BIT(22)	// "frostborn" - tolerant of cold climates
#define NUM_BONUS_TRAITS  23

// bonus traits available to newbies (first 12h)
#define NEWBIE_BONUS_TRAITS  (BONUS_STRENGTH | BONUS_DEXTERITY | BONUS_CHARISMA | BONUS_GREATNESS | BONUS_INTELLIGENCE | BONUS_WITS | BONUS_HEALTH | BONUS_MOVES | BONUS_MANA | BONUS_MOVE_REGEN | BONUS_MANA_REGEN | BONUS_EXTRA_DAILY_SKILLS)

// bonus traits not available at all (not used on this mud)
#define FORBIDDEN_BONUS_TRAITS  (BONUS_CLOCK)


// CDU_x: delayed update types
#define CDU_PASSIVE_BUFFS  BIT(0)	// refresh passive buffs
#define CDU_SAVE  BIT(1)	// saves the character
#define CDU_MSDP_SEND_UPDATES  BIT(2)	// sends any pending MSDP updates
#define CDU_MSDP_AFFECTS  BIT(3)	// runs update_MSDP_affects()
#define CDU_MSDP_ATTRIBUTES  BIT(4)	// runs update_MSDP_attributes()
#define CDU_MSDP_COOLDOWNS  BIT(5)	// runs update_MSDP_cooldowns()
#define CDU_MSDP_DOTS  BIT(6)	// runs update_MSDP_dots()
#define CDU_MSDP_EMPIRE_ALL  BIT(7)	// runs update_MSDP_empire_data()
#define CDU_MSDP_EMPIRE_CLAIMS  BIT(8)	// runs update_MSDP_empire_claims()
#define CDU_MSDP_SKILLS  BIT(9)	// runs update_MSDP_skills()
#define CDU_TRAIT_HOOKS  BIT(10)	// runs check_trait_hooks()


// CHANNEL_HISTORY_x: types of channel histories -- act.comm.c
#define NO_HISTORY  -1	// reserved
#define CHANNEL_HISTORY_GOD  0	// no longer used: god channels are now global
#define CHANNEL_HISTORY_TELLS  1
#define CHANNEL_HISTORY_SAY  2
#define CHANNEL_HISTORY_EMPIRE  3	// no longer used: empire histories now saved to empire
#define CHANNEL_HISTORY_ROLL  4
#define NUM_CHANNEL_HISTORY_TYPES  5


// GLOBAL_HISTORY_x: types of global channel histories -- act.comm.c
#define GLOBAL_HISTORY_GOD  0		// god channel and wiznet
#define NUM_GLOBAL_HISTORIES  1		// must be the total number


// channels for announcements
#define DEATH_LOG_CHANNEL  "death"
#define EVENT_LOG_CHANNEL  "events"
#define PROGRESS_LOG_CHANNEL  "progress"
#define PLAYER_LOG_CHANNEL  "grats"


// CMOD_x: modifications to companions (struct companion_mod)
#define CMOD_SEX  0	// overrides mob's sex (num)
#define CMOD_KEYWORDS  1	// custom mob keywords (str)
#define CMOD_SHORT_DESC  2	// custom mob short-desc (str)
#define CMOD_LONG_DESC  3	// custom mob long-desc (str)
#define CMOD_LOOK_DESC  4	// custom mob look-desc (str)


// CON_x: Modes of connectedness
#define CON_PLAYING  0	// Playing - Nominal state
#define CON_CLOSE  1	// Disconnecting
#define CON_GET_NAME  2	// By what name ..?
#define CON_NAME_CNFRM  3	// Did I get that right, x?
#define CON_PASSWORD  4	// Password:
#define CON_NEWPASSWD  5	// Give me a password for x
#define CON_CNFPASSWD  6	// Please retype password:
#define CON_QSEX  7	// Sex?
	#define CON_UNUSED1  8
#define CON_RMOTD  9	// PRESS RETURN after MOTD
#define CON_DISCONNECT  10	// In-game disconnection
#define CON_REFERRAL  11	// referral
#define CON_Q_SCREEN_READER  12  // asks if the player is blind
#define CON_QLAST_NAME  13	// Do you have a last..
#define CON_SLAST_NAME  14  // And what is it?
#define CON_CLAST_NAME  15	// Did I get that right?
#define CON_ARCHETYPE_CNFRM  16	// Correct archetype?
#define CON_Q_HAS_ALT  17	// Do you have an alt?
#define CON_Q_ALT_NAME  18	// Type alt's name
#define CON_Q_ALT_PASSWORD  19	// Type alt's password
#define CON_FINISH_CREATION  20	// Done!
#define CON_Q_ARCHETYPE  21	// starting skills and spells
#define CON_GOODBYE  22	// Close on <enter>
#define CON_BONUS_RESET  23	// player will get new bonus traits (enter to continue)
#define CON_BONUS_TRAIT  24	// choose bonus trait (existing char)
#define CON_PROMO_CODE  25	// promo code?
#define CON_CONFIRM_PROMO_CODE  26	// promo confirmation


// CUSTOM_COLOR_x: custom color types
#define CUSTOM_COLOR_EMOTE  0
#define CUSTOM_COLOR_ESAY  1
#define CUSTOM_COLOR_GSAY  2
#define CUSTOM_COLOR_OOCSAY  3
#define CUSTOM_COLOR_SAY  4
#define CUSTOM_COLOR_SLASH  5
#define CUSTOM_COLOR_TELL  6
#define CUSTOM_COLOR_STATUS  7
#define CUSTOM_COLOR_SUN  8
#define CUSTOM_COLOR_TEMPERATURE  9
#define CUSTOM_COLOR_WEATHER  10
#define NUM_CUSTOM_COLORS  11	// total


// COND_x: Player conditions
#define DRUNK  0
#define FULL  1
#define THIRST  2
#define NUM_CONDS  3


// FM_x: combat messages
#define FM_MY_HITS  BIT(0)	// player's own hits
#define FM_MY_MISSES  BIT(1)	// player's misses
#define FM_HITS_AGAINST_ME  BIT(2)	// player is hit
#define FM_MISSES_AGAINST_ME  BIT(3)	// player is missed
#define FM_ALLY_HITS  BIT(4)	// any allied character hits
#define FM_ALLY_MISSES  BIT(5)	// any allied player misses
#define FM_HITS_AGAINST_ALLIES  BIT(6)	// any ally is hit
#define FM_MISSES_AGAINST_ALLIES  BIT(7)	// any ally is missed
#define FM_HITS_AGAINST_TARGET  BIT(8)	// anybody hits your target
#define FM_MISSES_AGAINST_TARGET  BIT(9)	// anybody misses your target
#define FM_HITS_AGAINST_TANK  BIT(10)	// anybody hits tank (target's focus)
#define FM_MISSES_AGAINST_TANK  BIT(11)	// anybody misses tank (target's focus)
#define FM_OTHER_HITS  BIT(12)	// hits not covered by other rules
#define FM_OTHER_MISSES  BIT(13)	// misses not covered by other rules
#define FM_AUTO_DIAGNOSE  BIT(14)	// does a diagnose after each hit
#define FM_MY_BUFFS_IN_COMBAT  BIT(15)	// buffs on me while fighting
#define FM_ALLY_BUFFS_IN_COMBAT  BIT(16)	// buffs on allies while fighting
#define FM_OTHER_BUFFS_IN_COMBAT  BIT(17)	// buffs on others while fighting
#define FM_DAMAGE_NUMBERS  BIT(18)	// shows numbers after damage strings
#define FM_MY_AFFECTS_IN_COMBAT  BIT(19)	// shows affects applying/wearing off me in combat
#define FM_ALLY_AFFECTS_IN_COMBAT  BIT(20)	// ally affects applying/wearing off when in combat
#define FM_OTHER_AFFECTS_IN_COMBAT  BIT(21)	// others' affects applying/wearing off when in combat
#define FM_MY_ABILITIES  BIT(22)	// abilities I used
#define FM_ALLY_ABILITIES  BIT(23)	// abilities allies used
#define FM_OTHER_ABILITIES  BIT(24)	// abilities others used
#define FM_ABILITIES_AGAINST_ME  BIT(25)	// abilities targeting me
#define FM_ABILITIES_AGAINST_ALLIES  BIT(26)	// abilities targeting allies
#define FM_ABILITIES_AGAINST_TARGET  BIT(27)	// abilities targeting my target
#define FM_ABILITIES_AGAINST_TANK  BIT(28)	// abilities targeting the tank
#define FM_MY_HEALS  BIT(29)	// heals I caused
#define FM_HEALS_ON_ME  BIT(30)	// heal effects that hit me
#define FM_HEALS_ON_ALLIES  BIT(31)	// heal effects that hit allies
#define FM_HEALS_ON_TARGET  BIT(32)	// heal effects that hit my target
#define FM_HEALS_ON_OTHER  BIT(33)	// heal effects that hit others

// flags set at character creation
#define DEFAULT_FIGHT_MESSAGES  (FM_MY_HITS | FM_MY_MISSES | FM_HITS_AGAINST_ME | FM_MISSES_AGAINST_ME | FM_ALLY_HITS | FM_ALLY_MISSES | FM_HITS_AGAINST_ALLIES | FM_MISSES_AGAINST_ALLIES | FM_HITS_AGAINST_TARGET | FM_MISSES_AGAINST_TARGET |FM_HITS_AGAINST_TANK | FM_MISSES_AGAINST_TANK | FM_OTHER_HITS | FM_OTHER_MISSES | FM_AUTO_DIAGNOSE | FM_MY_BUFFS_IN_COMBAT | FM_ALLY_BUFFS_IN_COMBAT | FM_OTHER_BUFFS_IN_COMBAT | FM_MY_AFFECTS_IN_COMBAT | FM_ALLY_AFFECTS_IN_COMBAT | FM_OTHER_AFFECTS_IN_COMBAT | FM_MY_ABILITIES | FM_ALLY_ABILITIES | FM_OTHER_ABILITIES | FM_ABILITIES_AGAINST_ME | FM_ABILITIES_AGAINST_ALLIES | FM_ABILITIES_AGAINST_TARGET | FM_ABILITIES_AGAINST_TANK | FM_MY_HEALS | FM_HEALS_ON_ME | FM_HEALS_ON_ALLIES | FM_HEALS_ON_TARGET | FM_HEALS_ON_OTHER)


// FRIEND_x: status on the friends list
#define FRIEND_NONE					0	// not friends
#define FRIEND_REQUEST_SENT			1	// sent a request to this player
#define FRIEND_REQUEST_RECEIVED		2	// received a request from that player
#define FRIEND_FRIENDSHIP			3	// true friendship


// GRANT_X: Grant flags allow players to use abilities below the required access level
#define GRANT_ADVANCE  BIT(0)
#define GRANT_BAN  BIT(1)
#define GRANT_CLEARABILITIES  BIT(2)
#define GRANT_DC  BIT(3)
#define GRANT_ECHO  BIT(4)
#define GRANT_EDITNOTES  BIT(5)
#define GRANT_EMPIRES  BIT(6)	// edelete, eedit, moveeinv
#define GRANT_FORCE  BIT(7)
#define GRANT_FREEZE  BIT(8)	// freeze/thaw
#define GRANT_GECHO  BIT(9)
#define GRANT_INSTANCE  BIT(10)
#define GRANT_LOAD  BIT(11)
#define GRANT_MUTE  BIT(12)
#define GRANT_OLC  BIT(13)	// includes vnum, vstat
#define GRANT_OLC_CONTROLS  BIT(14)	// .set*
#define GRANT_PAGE  BIT(15)
#define GRANT_PURGE  BIT(16)
#define GRANT_REBOOT  BIT(17)
#define GRANT_RELOAD  BIT(18)
#define GRANT_RESTORE  BIT(19)
#define GRANT_SEND  BIT(20)
#define GRANT_SET  BIT(21)
#define GRANT_SHUTDOWN  BIT(22)
#define GRANT_SNOOP  BIT(23)
#define GRANT_SWITCH  BIT(24)
#define GRANT_TEDIT  BIT(25)
#define GRANT_TRANSFER  BIT(26)
#define GRANT_UNBIND  BIT(27)
#define GRANT_USERS  BIT(28)
#define GRANT_WIZLOCK  BIT(29)
#define GRANT_RESCALE  BIT(30)
#define GRANT_APPROVE  BIT(31)
#define GRANT_FORGIVE  BIT(32)
#define GRANT_HOSTILE  BIT(33)
#define GRANT_SLAY  BIT(34)
#define GRANT_ISLAND  BIT(35)
#define GRANT_OSET  BIT(36)
#define GRANT_PLAYERDELETE  BIT(37)
#define GRANT_UNQUEST  BIT(38)
#define GRANT_AUTOMESSAGE  BIT(39)
#define GRANT_PEACE  BIT(40)
#define GRANT_UNPROGRESS  BIT(41)
#define GRANT_EVENTS  BIT(42)
#define GRANT_TRIGGERS  BIT(43)


// INFORMATIVE_x: For players' informative views
#define INFORMATIVE_BUILDING_STATUS  BIT(0)	// a. dismantling/unfinished
#define INFORMATIVE_DISREPAIR  BIT(1)	// b. major/minor disrepair
#define INFORMATIVE_MINE_STATUS  BIT(2)	// c. shows whether a mine has ore
#define INFORMATIVE_PUBLIC  BIT(3)	// g. tile is flagged as public
#define INFORMATIVE_NO_WORK  BIT(4)	// d. workforce is off
#define INFORMATIVE_NO_ABANDON  BIT(5)	// e. protection against abandon
#define INFORMATIVE_NO_DISMANTLE  BIT(6)	// f. workforce won't dismantle

// flags set at character creation
#define DEFAULT_INFORMATIVE_BITS  (INFORMATIVE_BUILDING_STATUS | INFORMATIVE_DISREPAIR | INFORMATIVE_MINE_STATUS | INFORMATIVE_PUBLIC | INFORMATIVE_NO_WORK | INFORMATIVE_NO_ABANDON | INFORMATIVE_NO_DISMANTLE)


// LASTNAME_x: config players lastname_mode: determines how players get last names
#define LASTNAME_SET_AT_CREATION  BIT(0)	// a. player chooses during creation menus
#define LASTNAME_CHANGE_ANY_TIME  BIT(1)	// b. player can change with 'lastname change'
#define LASTNAME_CHOOSE_FROM_LIST  BIT(2)	// c. player can choose from the namelist


// Lore types
#define LORE_JOIN_EMPIRE		0
#define LORE_DEFECT_EMPIRE		1
#define LORE_KICKED_EMPIRE		2
#define LORE_PLAYER_KILL		3
#define LORE_PLAYER_DEATH		4
#define LORE_TOWER_DEATH		5
#define LORE_FOUND_EMPIRE		6
#define LORE_START_VAMPIRE		7
#define LORE_SIRE_VAMPIRE		8
#define LORE_PURIFY				9
#define LORE_CREATED			10
#define LORE_MAKE_VAMPIRE		11
#define LORE_PROMOTED			12


// MORPHF_x: flags for morphs
#define MORPHF_IN_DEVELOPMENT  BIT(0)	// a. can't be used by players
#define MORPHF_SCRIPT_ONLY  BIT(1)	// b. can't be morphed manually
#define MORPHF_ANIMAL  BIT(2)	// c. treated like an npc animal (disguise)
#define MORPHF_VAMPIRE_ONLY  BIT(3)	// d. requires vampire status
#define MORPHF_TEMPERATE_AFFINITY  BIT(4)	// e. requires temperate
#define MORPHF_ARID_AFFINITY  BIT(5)	// f. requires arid
#define MORPHF_TROPICAL_AFFINITY  BIT(6)	// g. requires tropical
#define MORPHF_CHECK_SOLO  BIT(7)	// h. check for the solo role
#define MORPHF_NO_SLEEP  BIT(8)	// i. cannot sleep in this form
#define MORPHF_GENDER_NEUTRAL  BIT(9)	// j. causes an "it" instead of him/her
#define MORPHF_CONSUME_OBJ  BIT(10)	// k. uses up the requiresobj
#define MORPHF_NO_FASTMORPH  BIT(11)	// l. cannot fastmorph into this form
#define MORPHF_NO_MORPH_MESSAGE  BIT(12)	// m. does not inform of auto-unmorph
#define MORPHF_HIDE_REAL_NAME  BIT(13)	// n. won't show real name on 'look'


// MOUNT_x: mount flags -- MOUNT_FLAGGED(ch, flag)
#define MOUNT_RIDING  BIT(0)	// player is currently mounted
#define MOUNT_AQUATIC  BIT(1)	// mount can swim (but not go on land)
#define MOUNT_FLYING  BIT(2)	// mount can fly
#define MOUNT_WATERWALKING  BIT(3)	// mount can do land/water


// OFFER_x - types for the do_accept/offer_data system
#define OFFER_RESURRECTION  0
#define OFFER_SUMMON  1
#define OFFER_QUEST  2


// PLR_x: Player flags: used by char_data.char_specials.act
#define PLR_APPROVED	BIT(0)	// player is approved to play the full game
#define PLR_TRAITS_RESET	BIT(1)	// has already had traits reset
#define PLR_MAILING		BIT(2)	/* Player is writing mail				*/
#define PLR_DONTSET		BIT(3)	/* Don't EVER set (ISNPC bit)			*/
	#define PLR_UNUSED2		BIT(4)
	#define PLR_UNUSED3		BIT(5)	// these were flags that got moved to ACCT_x
	#define PLR_UNUSED4		BIT(6)
	#define PLR_UNUSED5		BIT(7)
#define PLR_LOADROOM	BIT(8)	/* Player uses nonstandard loadroom		*/
#define PLR_NOWIZLIST	BIT(9)	/* Player shouldn't be on wizlist		*/
#define PLR_NODELETE	BIT(10)	/* Player shouldn't be deleted			*/
#define PLR_INVSTART	BIT(11)	/* Player should enter game wizinvis	*/
#define PLR_IPMASK		BIT(12)	/* Player is IP-masked					*/
#define PLR_DISGUISED	BIT(13)	// Player is using a disguise
	#define PLR_UNUSEDV		BIT(14)
	#define PLR_UNUSED6		BIT(15)
#define PLR_NEEDS_NEWBIE_SETUP  BIT(16)  // player is created but needs gear and setup
#define PLR_UNRESTRICT	BIT(17)	/* !walls, !buildings					*/
#define PLR_KEEP_LAST_LOGIN_INFO  BIT(18)	// in case of players loaded into the game, does not overwrite their playtime or last login info if stored
#define PLR_EXTRACTED	BIT(19)	// for late-extraction
#define PLR_ADVENTURE_SUMMONED  BIT(20)	// marks the player for return to whence they came


// PRF_x: Preference flags
#define PRF_AFK  BIT(0)	// player is marked away
#define PRF_COMPACT  BIT(1)	// No extra CRLF pair before prompts
#define PRF_DEAF  BIT(2)	// Can't hear shouts
#define PRF_NOTELL  BIT(3)	// Can't receive tells
#define PRF_POLITICAL  BIT(4)	// Changes map to political colors
#define PRF_RP  BIT(5)	// RP-only
	#define PRF_UNUSED_1  BIT(6)	// was MORTLOG before b5.162
#define PRF_NOREPEAT  BIT(7)	// No repetition of comm commands
#define PRF_HOLYLIGHT  BIT(8)	// Immortal: Can see in dark
#define PRF_INCOGNITO  BIT(9)	// Immortal: Can't be seen on the who list
#define PRF_NOWIZ  BIT(10)	// Can't hear wizline
#define PRF_NOMAPCOL  BIT(11)	// Map is not colored
#define PRF_NOHASSLE  BIT(12)	// Ignored by mobs and triggers
#define PRF_NO_IDLE_OUT  BIT(13)	// Player won't idle out
#define PRF_ROOMFLAGS  BIT(14)	// Sees vnums and flags on look
	#define PRF_UNUSED_2  BIT(15)	// was !CHANNEL-JOINS before b5.162
#define PRF_AUTOKILL  BIT(16)	// Stops from knocking players out
#define PRF_SCROLLING  BIT(17)	// Turns off page_string
#define PRF_NO_ROOM_DESCS  BIT(18)	// Removes room descs; formerly 'brief'
#define PRF_BOTHERABLE  BIT(19)	// allows bite, purify, feed, etc
#define PRF_AUTORECALL  BIT(20)	// free recall when logged off too long
#define PRF_NOGODNET  BIT(21)	// Can't hear godnet
#define PRF_ALLOW_PVP  BIT(22)	// free pvp
#define PRF_INFORMATIVE  BIT(23)	// informative map display colors
#define PRF_NOSPAM  BIT(24)	// blocks periodic action messages
#define PRF_SCREEN_READER  BIT(25)	// player is visually impaired and using a screen reader that can't read the map
#define PRF_STEALTHABLE  BIT(26)	// player can steal (rather than be prevented from accidentally stealing)
#define PRF_WIZHIDE  BIT(27)	// player can't be seen in the room
#define PRF_AUTONOTES  BIT(28)	// Player login syslogs automatically show notes
#define PRF_AUTODISMOUNT  BIT(29)	// will dismount while moving instead of seeing an error
#define PRF_NOEMPIRE  BIT(30)	// the game will not automatically create an empire
#define PRF_CLEARMETERS  BIT(31)	// automatically clears the damage meters before a new fight
#define PRF_NO_TUTORIALS  BIT(32)	// shuts off new tutorial quests
#define PRF_NO_PAINT  BIT(33)	// unable to see custom paint colors
#define PRF_EXTRA_SPACING  BIT(34)	// causes an extra crlf before command interpreter
	#define PRF_UNUSED_3  BIT(35)	// was TRAVEL-LOOK before b5.162
#define PRF_AUTOCLIMB  BIT(36)	// will enter mountains without 'climb'
#define PRF_AUTOSWIM  BIT(37)	// will enter water without 'swim'
#define PRF_ITEM_QUALITY  BIT(38)	// shows loot quality color/tag in inv/eq
#define PRF_ITEM_DETAILS  BIT(39)	// shows additional item details on inv/eq
#define PRF_NO_EXITS  BIT(40)	// hides exits on look and auto-look
#define PRF_SHORT_EXITS  BIT(41)	// shows circlemud-style exits
#define PRF_NO_FRIENDS  BIT(42)	// this alt will not appear on friends lists
// note: if you add prefs, consider adding them to alt_import_preferences()


// PTECH_x: player techs
#define PTECH_RESERVED  0
#define PTECH_ARMOR_HEAVY  1	// can wear heavy armor
#define PTECH_ARMOR_LIGHT  2	// can wear light armor
#define PTECH_ARMOR_MAGE  3	// can wear mage armor
#define PTECH_ARMOR_MEDIUM  4	// can wear medium armor
#define PTECH_BLOCK  5	// can use shields/block
#define PTECH_BLOCK_RANGED  6	// can block arrows (requires block)
#define PTECH_BLOCK_MAGICAL  7	// can block magical attacks (requires block)
#define PTECH_BONUS_VS_ANIMALS  8	// extra damage against animals
#define PTECH_BUTCHER_UPGRADE  9	// butcher always succeeds
#define PTECH_CUSTOMIZE_BUILDING  10	// player can customize buildings
#define PTECH_DEEP_MINES  11	// increases mine size
#define PTECH_DUAL_WIELD  12	// can fight with offhand weapons
#define PTECH_FAST_WOOD_PROCESSING  13	// saw/scrape go faster
#define PTECH_FASTCASTING  14	// wits affects non-combat abilities instead of combat speed
#define PTECH_FAST_FIND  15	// digging, gathering, panning, picking
#define PTECH_FISH_COMMAND  16	// can use the 'fish' command/interaction
#define PTECH_FORAGE_COMMAND  17	// can use the 'forage' command/interaction
#define PTECH_HARVEST_UPGRADE  18	// more results from harvest
	#define PTECH_UNUSED  19	// deprecated: was HEALING_BOOST
#define PTECH_HIDE_UPGRADE  20	// improves hide and blocks search
#define PTECH_INFILTRATE  21	// can enter buildings without permission
#define PTECH_INFILTRATE_UPGRADE  22	// better infiltrates
#define PTECH_LARGER_LIGHT_RADIUS  23	// can see farther at night
#define PTECH_LIGHT_FIRE  24	// player can light torches/fires
#define PTECH_MAP_INVIS  25	// can't be seen on the map
#define PTECH_MILL_UPGRADE  26	// more results from milling
#define PTECH_NAVIGATION  27	// player sees correct directions
#define PTECH_NO_HUNGER  28	// player won't get hungry
#define PTECH_NO_POISON  29	// immune to poison effects
#define PTECH_NO_THIRST  30	// player won't get thirsty
#define PTECH_NO_TRACK_CITY  31	// blocks track/where in cities/indoors
#define PTECH_NO_TRACK_WILD  32	// blocks track/where in the wild
#define PTECH_PICKPOCKET  33	// can use the 'pickpocket' command/interaction
#define PTECH_POISON  34	// can use poisons in combat
#define PTECH_POISON_UPGRADE  35	// enhances all poisons
#define PTECH_PORTAL  36	// can open portals
#define PTECH_PORTAL_UPGRADE  37	// can open long-distance portals
#define PTECH_RANGED_COMBAT  38	// can use ranged weapons
#define PTECH_RIDING  39	// player can ride mounts
#define PTECH_RIDING_FLYING  40	// player can ride flying mounts (requires riding)
#define PTECH_RIDING_UPGRADE  41	// no terrain restrictions on riding (requires riding)
#define PTECH_ROUGH_TERRAIN  42	// player can cross rough-to-rough
#define PTECH_SEE_CHARS_IN_DARK  43	// can see people in dark rooms
#define PTECH_SEE_OBJS_IN_DARK  44	// includes vehicles
#define PTECH_SEE_INVENTORY  45	// can see players' inventories
#define PTECH_SHEAR_UPGRADE  46	// more results from shear
#define PTECH_STEAL_UPGRADE  47	// can steal from vaults
#define PTECH_SWIMMING  48	// player can enter water tiles
#define PTECH_SEARCH_COMMAND  49	// allows 'search' command
#define PTECH_TWO_HANDED_MASTERY  50	// bonus damage from 2H weapons
#define PTECH_WHERE_UPGRADE  51	// 'where' command embiggens
#define PTECH_DODGE_CAP  52	// improves your dodge cap
#define PTECH_SKINNING_UPGRADE  53	// skinning always succeeds
#define PTECH_BARDE  54	// can barde animals
#define PTECH_HERD_COMMAND  55	// can herd animals
#define PTECH_MILK_COMMAND  56	// can milk animals (at a stable)
#define PTECH_SHEAR_COMMAND  57	// can shear animals (at a stable)
#define PTECH_TAME_ANIMALS  58	// can tame animals
#define PTECH_BITE_MELEE_UPGRADE  59	// melee features of 'bite'
#define PTECH_BITE_TANK_UPGRADE  60	// tank features of 'bite'
#define PTECH_BITE_STEAL_BLOOD  61	// steals blood on each 'bite' attack
#define PTECH_SEE_IN_DARK_OUTDOORS  62  // can see in dark only if outside
#define PTECH_HUNT_ANIMALS  63	// can use the 'hunt' command on animals
#define PTECH_CLOCK  64	// can tell time
#define PTECH_CALENDAR  65	// can tell the date
#define PTECH_MINT_COMMAND  66	// can mint coins
#define PTECH_TAN_COMMAND  67	// can use the 'tan' command
#define PTECH_NO_PURIFY  68	// cannot be affected by the 'purify' spell
#define PTECH_VAMPIRE_SUN_IMMUNITY  69	// vampire not penalized in the sun
#define PTECH_GATHER_COMMAND  70	// can 'gather'
#define PTECH_CHOP_COMMAND  71	// can 'chop'
#define PTECH_DIG_COMMAND  72	// can 'dig'
#define PTECH_HARVEST_COMMAND  73	// can 'harvest'
#define PTECH_PICK_COMMAND  74	// can 'pick'
#define PTECH_QUARRY_COMMAND  75	// can 'quarry'
#define PTECH_DRINK_BLOOD_FASTER  76	// vampires drink more blood at a time
#define PTECH_SUMMON_MATERIALS  77	// can use the 'summon materials' command
#define PTECH_CUSTOMIZE_VEHICLE  78	// can use 'customize vehicle'
#define PTECH_PLANT_CROPS  79	// can 'plant'
#define PTECH_CHIP_COMMAND  80	// can 'chip'
#define PTECH_SAW_COMMAND  81	// can 'saw'
#define PTECH_SCRAPE_COMMAND  82	// can 'scrape'
#define PTECH_MAP_MEMORY  83	// remembers blocked/dark tiles on look/scan
#define PTECH_SEE_IN_MAGIC_DARKNESS  84	// sees through darkness room affect
#define PTECH_TRACK_COMMAND  85	// can use 'track'
#define PTECH_RESIST_POISON  86	// minor resistance
#define PTECH_VAMPIRE_BITE  87	// can use 'bite' for blood instead of the social
#define PTECH_ENEMY_BUFF_DETAILS  88	// can see details when using 'affects' on others
#define PTECH_CONCEAL_EQUIPMENT  89	// others can't see your equipment
#define PTECH_CONCEAL_INVENTORY  90	// others can't see your inventory
#define PTECH_FLEE_UPGRADE  91	// increased chance to flee
#define PTECH_FASTER_MELEE_COMBAT  92	// slight speed boost in melee
#define PTECH_FASTER_RANGED_COMBAT  93	// slight speed boost in ranged combat
#define PTECH_RIDING_SWAP_ANYWHERE  94	// can swap in any location
#define PTECH_RIDING_RELEASE_MOUNT  95	// can release your mounts
#define PTECH_REWORK_COMMAND  96	// allows refreshing and restringing gear
#define PTECH_BITE_REGENERATION  97	// rapid regeneration while drinking blood from humans
#define PTECH_STEAL_COMMAND  98	// can use 'steal'
#define PTECH_USE_HONED_GEAR  99	// can use honed gear
#define PTECH_REDIRECT_MAGICAL_DAMAGE_TO_MANA  100	// redirect part of magical damage to mana pool instead of health
#define PTECH_MORE_BLOOD_FROM_HUMANS  101	// vampires get more out of blood from humans


// SM_x: status messages
#define SM_ANIMAL_MOVEMENT  BIT(0)	// animals wandering on the map
#define SM_CHANNEL_JOINS  BIT(1)	// players joining/leaving slash-channels
#define SM_COOLDOWNS  BIT(2)	// message when a cooldown expires
#define SM_EMPIRE_LOGS   BIT(3)	// can turn off elog messages
#define SM_HUNGER  BIT(4)	// hunger messages
#define SM_THIRST  BIT(5)	// thirst messages
#define SM_LOW_BLOOD  BIT(6)	// vampire starvation messages
#define SM_MORTLOG  BIT(7)	// player creation logs, etc
#define SM_PROMPT  BIT(8)	// show or hide prompt
#define SM_SKILL_GAINS  BIT(9) // message when player gains skill points
#define SM_SUN  BIT(10)	// sunrise/sunset messages
#define SM_SUN_AUTO_LOOK  BIT(11)	// player looks when sun goes up or down
#define SM_TEMPERATURE  BIT(12)	// basic temperature change messages
#define SM_EXTREME_TEMPERATURE  BIT(13)	// warning messages for dangerous temperature
#define SM_TRAVEL_AUTO_LOOK  BIT(14)	// auto-look when running
#define SM_VEHICLE_MOVEMENT  BIT(15)	// messages shown to interior when vehicle moves
#define SM_WEATHER  BIT(16)	// weather change messages
#define SM_FIGHT_PROMPT  BIT(17)	// show or hide fprompt

// flags set at character creation
#define DEFAULT_STATUS_MESSAGES  (SM_ANIMAL_MOVEMENT | SM_CHANNEL_JOINS | SM_COOLDOWNS | SM_EMPIRE_LOGS | SM_HUNGER | SM_THIRST | SM_LOW_BLOOD | SM_MORTLOG | SM_PROMPT | SM_FIGHT_PROMPT | SM_SKILL_GAINS | SM_SUN | SM_SUN_AUTO_LOOK | SM_TEMPERATURE | SM_EXTREME_TEMPERATURE | SM_VEHICLE_MOVEMENT | SM_WEATHER)


// summon types for oval_summon, ofin_summon, and add_offer
#define SUMMON_PLAYER  0	// normal "summon player" command
#define SUMMON_ADVENTURE  1	// for adventure_summon()


// SYS_x: syslog types
#define SYS_CONFIG  BIT(0)	// configs
#define SYS_DEATH  BIT(1)	// death/respawn
#define SYS_ERROR  BIT(2)	// syserrs
#define SYS_GC  BIT(3)	// god commands
#define SYS_INFO  BIT(4)	// misc informative
#define SYS_LVL  BIT(5)	// level logs
#define SYS_LOGIN  BIT(6)	// logins/creates
#define SYS_OLC  BIT(7)	// olc logs
#define SYS_SCRIPT  BIT(8)	// script logs
#define SYS_SYSTEM  BIT(9)	// system stuff
#define SYS_VALID  BIT(10)	// validation logs
#define SYS_EMPIRE  BIT(11)	// empire-related logs
#define SYS_EVENT  BIT(12)	// event news and points


// WAIT_x: Wait types for the command_lag() function.
#define WAIT_NONE  0	// for functions that require a wait_type
#define WAIT_ABILITY  1	// general abilities
#define WAIT_COMBAT_ABILITY  2	// ability that does damage or affects combat
#define WAIT_COMBAT_SPELL  3	// spell that does damage or affects combat (except healing)
#define WAIT_MOVEMENT  4	// normal move lag
#define WAIT_SPELL  5	// general spells
#define WAIT_OTHER  6	// not covered by other categories
#define WAIT_LONG  7	// longer than the others
#define WAIT_VERY_LONG  8	// longest wait


 //////////////////////////////////////////////////////////////////////////////
//// PROGRESS DEFINES ////////////////////////////////////////////////////////

// PROGRESS_x: progress types
#define PROGRESS_UNDEFINED  0	// base/unset
#define PROGRESS_COMMUNITY  1
#define PROGRESS_INDUSTRY  2
#define PROGRESS_DEFENSE  3
#define PROGRESS_PRESTIGE  4
#define NUM_PROGRESS_TYPES  5	// total


// PRG_x: progress flags
#define PRG_IN_DEVELOPMENT  BIT(0)	// a. not available to players
#define PRG_PURCHASABLE  BIT(1)	// b. can buy it
#define PRG_NO_AUTOSTART  BIT(2)	// c. only started or added via script/quest
#define PRG_HIDDEN  BIT(3)	// d. progress does not show up
#define PRG_NO_ANNOUNCE  BIT(4)	// e. never announces when this goal is achieved
#define PRG_NO_PREVIEW  BIT(5)	// f. cannot view it until you're on it
#define PRG_NO_TRACKER  BIT(6)	// g. hide the tracker from the player


// PRG_PERK_x: progress perks
#define PRG_PERK_TECH  0	// grants a technology
#define PRG_PERK_CITY_POINTS  1	// grants more city points
#define PRG_PERK_CRAFT  2	// grants a recipe
#define PRG_PERK_MAX_CITY_SIZE  3	// increases max city size
#define PRG_PERK_TERRITORY_FROM_WEALTH  4	// increases territory from wealth
#define PRG_PERK_TERRITORY_PER_GREATNESS  5	// increases territory per greatness
#define PRG_PERK_WORKFORCE_CAP  6	// higher workforce caps
#define PRG_PERK_TERRITORY  7	// grants bonus territory (flat rate)
#define PRG_PERK_SPEAK_LANGUAGE  8	// whole empire may speak
#define PRG_PERK_RECOGNIZE_LANGUAGE  9	// whole empire may recognize


 //////////////////////////////////////////////////////////////////////////////
//// QUEST DEFINES ///////////////////////////////////////////////////////////

// QST_x: quest flags
#define QST_IN_DEVELOPMENT  BIT(0)	// not an active quest
#define QST_REPEAT_PER_INSTANCE  BIT(1)	// clears completion when instance closes
#define QST_EXPIRES_AFTER_INSTANCE  BIT(2)	// fails if instance closes
#define QST_EXTRACT_TASK_OBJECTS  BIT(3)	// takes away items required by the task
#define QST_DAILY  BIT(4)	// counts toward dailies; repeats each "day"
#define QST_EMPIRE_ONLY  BIT(5)	// only available if quest giver and player are in the same empire
#define QST_NO_GUESTS  BIT(6)	// quest start/finish use MEMBERS_ONLY
#define QST_TUTORIAL  BIT(7)	// quest can be blocked by 'toggle tutorial'
#define QST_GROUP_COMPLETION  BIT(8)	// group members auto-finish this quest, even if incomplete, if present when any member does
#define QST_EVENT  BIT(9)	// shows as an event quest; splits dailies into 2 pools
#define QST_IN_CITY_ONLY  BIT(10)	// requires that it's in a city


// QG_x: quest giver types
#define QG_BUILDING  0
#define QG_MOBILE  1
#define QG_OBJECT  2
#define QG_ROOM_TEMPLATE  3
#define QG_TRIGGER  4	// just to help lookups
#define QG_QUEST  5	// (e.g. as chain reward) just to help lookups
#define QG_VEHICLE  6


// QR_x: quest reward types
#define QR_BONUS_EXP  0
#define QR_COINS  1
#define QR_OBJECT  2
#define QR_SET_SKILL  3
#define QR_SKILL_EXP  4
#define QR_SKILL_LEVELS  5
#define QR_QUEST_CHAIN  6
#define QR_REPUTATION  7
#define QR_CURRENCY  8
#define QR_EVENT_POINTS  9
#define QR_SPEAK_LANGUAGE  10
#define QR_RECOGNIZE_LANGUAGE  11
#define QR_GRANT_PROGRESS  12
#define QR_START_PROGRESS  13
#define QR_UNLOCK_ARCHETYPE  14


// indicates empire (rather than misc) coins for a reward
#define REWARD_EMPIRE_COIN  0


 //////////////////////////////////////////////////////////////////////////////
//// SECTOR DEFINES //////////////////////////////////////////////////////////

// SECTF_x: sector flags -- see constants.world.c
#define SECTF_LOCK_ICON  BIT(0)	// random icon is chosen once and kept
#define SECTF_ADVENTURE  BIT(1)	// uses the adventure zone template
#define SECTF_NON_ISLAND  BIT(2)	// does not count as an island for island detection (e.g. ocean, tundra)
#define SECTF_CHORE  BIT(3)	// has an associated chore (goes in the territory list)
#define SECTF_NO_CLAIM  BIT(4)	// can never be claimed
#define SECTF_START_LOCATION  BIT(5)	// like a Tower of souls
#define SECTF_FRESH_WATER  BIT(6)	// e.g. river, requires boat
#define SECTF_OCEAN  BIT(7)	// e.g. ocean, requires boat
#define SECTF_DRINK  BIT(8)	// can drink from this tile (or adjacent one)
#define SECTF_HAS_CROP_DATA  BIT(9)	// e.g. seeded field, does not appear as crop
#define SECTF_CROP  BIT(10)	// shows crop tile instead of sect tile
#define SECTF_LAY_ROAD  BIT(11)	// can put a road on it
#define SECTF_IS_ROAD  BIT(12)	// can be removed with "lay" command, can roadsign
#define SECTF_CAN_MINE  BIT(13)	// mine data available (prospect)
#define SECTF_SHOW_ON_POLITICAL_MAPOUT  BIT(14)	// if unclaimed, still shows on the political graphical map
#define SECTF_MAP_BUILDING  BIT(15)	// shows a building instead of sect tile
#define SECTF_INSIDE  BIT(16)	// shows a room instead of sect tile; is interior of a building (off-map)
#define SECTF_LARGE_CITY_RADIUS  BIT(17)	// counts as in-city much further than normal
#define SECTF_OBSCURE_VISION  BIT(18)	// blocks mappc
#define SECTF_IS_TRENCH  BIT(19)	// excavate-related
#define SECTF_NO_GLOBAL_SPAWNS  BIT(20)	// won't use global spawn lists
#define SECTF_ROUGH  BIT(21)	// hard terrain, requires ATR; other mountain-like properties
#define SECTF_SHALLOW_WATER  BIT(22)	// can't earthmeld; other properties like swamp and oasis have
#define SECTF_NEEDS_HEIGHT  BIT(23)	// will automatically set its 'height' property under certain circumstances
#define SECTF_KEEPS_HEIGHT  BIT(24)	// retains its 'height' property but won't inherit a new one
#define SECTF_SEPARATE_NOT_ADJACENTS  BIT(25)	// runs every NOT-ADJACENT evolution separately instead of ensuring it's not adjacent to ANY of them
#define SECTF_SEPARATE_NOT_NEARS  BIT(26)	// runs every NOT-NEAR-SECTOR evolution separately instead of ensuring it's not near ANY of them
#define SECTF_INHERIT_BASE_CLIMATE  BIT(27)	// inherits the climate of the base sector in addition to its own (e.g. road, building, etc)
#define SECTF_IRRIGATES_AREA  BIT(28)	// tiles around this one trigger irrigation evolutions
// note: evolutions use these as flags in a SIGNED sbitvector_t; limit is BIT(62)


// SECT_CUSTOM_x: custom messages
#define SECT_CUSTOM_MAGIC_GROWTH		0	// shown on magic-growth evolution


 //////////////////////////////////////////////////////////////////////////////
//// SHOP DEFINES ////////////////////////////////////////////////////////////

// SHOP_x: shop flags
#define SHOP_IN_DEVELOPMENT  BIT(0)	// a. shop is not available to mortals


 //////////////////////////////////////////////////////////////////////////////
//// SOCIAL DEFINES //////////////////////////////////////////////////////////

// SOC_x: Social flags
#define SOC_IN_DEVELOPMENT  BIT(0)	// a. can't be used by players
#define SOC_HIDE_IF_INVIS  BIT(1)	// b. no "Someone" if player can't be seen


// SOCM_x: social message string
#define SOCM_NO_ARG_TO_CHAR  0
#define SOCM_NO_ARG_TO_OTHERS  1
#define SOCM_TARGETED_TO_CHAR  2
#define SOCM_TARGETED_TO_OTHERS  3
#define SOCM_TARGETED_TO_VICTIM  4
#define SOCM_TARGETED_NOT_FOUND  5
#define SOCM_SELF_TO_CHAR  6
#define SOCM_SELF_TO_OTHERS  7
#define NUM_SOCM_MESSAGES  8	// total


 //////////////////////////////////////////////////////////////////////////////
//// VEHICLE DEFINES //////////////////////////////////////////////////////////

// VEH_x: vehicle flags
#define VEH_INCOMPLETE  BIT(0)	// a. can't be used until finished
#define VEH_DRIVING  BIT(1)	// b. can move on land
#define VEH_SAILING  BIT(2)	// c. can move on water
#define VEH_FLYING  BIT(3)	// d. no terrain restrictions
#define VEH_ALLOW_ROUGH  BIT(4)	// e. can move on rough terrain
#define VEH_SIT  BIT(5)	// f. can sit on
#define VEH_IN  BIT(6)	// g. player sits IN rather than ON
#define VEH_BURNABLE  BIT(7)	// h. can burn
#define VEH_CONTAINER  BIT(8)	// i. can put items in
#define VEH_SHIPPING  BIT(9)	// j. used for the shipping system
#define VEH_CUSTOMIZABLE  BIT(10)	// k. players can name it
#define VEH_DRAGGABLE  BIT(11)	// l. player can drag it over land
#define VEH_NO_BUILDING  BIT(12)	// m. won't fit into a building
#define VEH_CAN_PORTAL  BIT(13)	// n. required for the vehicle to go through a portal
#define VEH_LEADABLE  BIT(14)	// o. can be lead around
#define VEH_CARRY_VEHICLES  BIT(15)	// p. can put vehicles on board
#define VEH_CARRY_MOBS  BIT(16)	// q. can put mobs on board
#define VEH_SIEGE_WEAPONS  BIT(17)	// r. can be used to besiege
#define VEH_ON_FIRE  BIT(18)	// s. currently on fire
#define VEH_NO_LOAD_ONTO_VEHICLE  BIT(19)	// t. cannot be loaded onto a vehicle
#define VEH_VISIBLE_IN_DARK  BIT(20)	// u. can be seen at night
#define VEH_NO_CLAIM  BIT(21)	// v. cannot be claimed
#define VEH_BUILDING  BIT(22)	// w. behaves more like a building
#define VEH_NEVER_DISMANTLE  BIT(23)	// x. vehicle cannot be dismantled
#define VEH_PLAYER_NO_DISMANTLE  BIT(24)	// y. player toggled no-dismantle on
#define VEH_DISMANTLING  BIT(25)	// z. is being dismantled
#define VEH_PLAYER_NO_WORK  BIT(26)	// A. player has marked it no-work
#define VEH_CHAMELEON  BIT(27)	// B. vehicle's icon isn't visible at a distance
#define VEH_INTERLINK  BIT(28)	// C. rooms can be interlinked with nearby interlink-flagged vehicles or rooms
#define VEH_IS_RUINS  BIT(29)	// D. counts as ruins for cities/decay
#define VEH_SLEEP  BIT(30)	// E. player can sleep/rest in/on it (like sit)
#define VEH_NO_PAINT  BIT(31)	// F. cannot be painted
#define VEH_BRIGHT_PAINT  BIT(32)	// G. brightly painted
#define VEH_DEDICATE  BIT(33)	// H. can be dedicated
#define VEH_EXTRACTED  BIT(34)	// I. vehicle is mid-extraction
#define VEH_RUIN_SLOWLY_FROM_CLIMATE  BIT(35)	// J. always ruins slowly no matter what climate
#define VEH_RUIN_QUICKLY_FROM_CLIMATE  BIT(36)	// K. always ruins quickly no matter what climate
#define VEH_OBSCURE_VISION  BIT(37)	// L. blocks tiles behind it on the map
#define VEH_HAS_INSTANCE  BIT(38)	// M. instance is attached to vehicle
#define VEH_TEMPORARY  BIT(39)	// N. vehicle will be removed when instance ends


// VEH_CUSTOM_x: custom message types
#define VEH_CUSTOM_RUINS_TO_ROOM  0	// sent when the building falls into ruin
#define VEH_CUSTOM_CLIMATE_CHANGE_TO_ROOM 1	// sent when the vehicle is destroyed by climate flags
#define VEH_CUSTOM_ENTER_TO_INSIDE  2	// sent inside a vehicle when a player enters
#define VEH_CUSTOM_ENTER_TO_OUTSIDE  3	// sent outside a vehicle when a player enters
#define VEH_CUSTOM_EXIT_TO_INSIDE  4	// sent inside a vehicle when a player exits
#define VEH_CUSTOM_EXIT_TO_OUTSIDE  5	// sent outside a vehicle when a player exits


// VSPEED_x: indicates the number of speed bonuses this vehicle gives to driving.
#define VSPEED_VERY_SLOW  0 // No speed bonuses.
#define VSPEED_SLOW       1 // One speed bonus.
#define VSPEED_NORMAL     2 // Two speed bonuses.
#define VSPEED_FAST       3 // Three speed bonuses.
#define VSPEED_VERY_FAST  4 // Four speed bonuses.

// The following vehicle flags are saved to file rather than read from the
// prototype. Flags which are NOT included in this list can be altered with
// OLC and affect live copies.
#define SAVABLE_VEH_FLAGS  (VEH_INCOMPLETE | VEH_ON_FIRE | VEH_PLAYER_NO_DISMANTLE | VEH_PLAYER_NO_WORK | VEH_DISMANTLING | VEH_CHAMELEON | VEH_BRIGHT_PAINT | VEH_HAS_INSTANCE | VEH_TEMPORARY)

// The following vehicle flags indicate a vehicle can move
#define MOVABLE_VEH_FLAGS  (VEH_DRIVING | VEH_SAILING | VEH_FLYING | VEH_DRAGGABLE | VEH_CAN_PORTAL | VEH_LEADABLE)


 //////////////////////////////////////////////////////////////////////////////
//// WEATHER AND SEASON DEFINES //////////////////////////////////////////////

// Sky conditions for weather_data
#define SKY_CLOUDLESS  0
#define SKY_CLOUDY  1
#define SKY_RAINING  2
#define SKY_LIGHTNING  3


// SUN_x: Sun state for get_sun_status()
#define SUN_DARK  0
#define SUN_RISE  1
#define SUN_LIGHT  2
#define SUN_SET  3


// tilesets for icons (this can be used for more than seasons; it just isn't)
#define TILESET_ANY  0	// applies to any type
#define TILESET_SPRING  1
#define TILESET_SUMMER  2
#define TILESET_AUTUMN  3
#define TILESET_WINTER  4	// winter must be the last season for reasons that appear if you search TILESET_WINTER
#define NUM_TILESETS  5	// total


 //////////////////////////////////////////////////////////////////////////////
//// WORLD DEFINES ///////////////////////////////////////////////////////////

// Direction-related stuff
#define NORTH  0
#define EAST  1
#define SOUTH  2
#define WEST  3
#define NUM_SIMPLE_DIRS  4	// set of basic dirs n,e,s,w

#define NORTHWEST  4
#define NORTHEAST  5
#define SOUTHWEST  6
#define SOUTHEAST  7
#define NUM_2D_DIRS  8	// set of dirs on the map/flat world

#define UP  8
#define DOWN  9
#define NUM_NATURAL_DIRS  10	// set of dirs

#define FORE  10
#define STARBOARD  11
#define PORT  12
#define AFT  13
#define DIR_RANDOM  14	// not a dir you can go, but used for room templates
#define NUM_OF_DIRS  15	// total number of directions


// CLIM_x: climate flags -- keywords specifically related to climate
// These were formally 4 separate 'types' rather than flags. It should be safe
// to bits a-d after b6, but in b5.84 they are used to detect sectors and crops
// that need to be updated
#define CLIM_UNUSED1  BIT(0)	// a. prior to b5.84, this was CLIMATE_NONE 0
#define CLIM_UNUSED2  BIT(1)	// b. prior to b5.84, this was CLIMATE_TEMPERATE 1
#define CLIM_UNUSED3  BIT(2)	// c. prior to b5.84, this was CLIMATE_ARID 2
#define CLIM_UNUSED4  BIT(3)	// d. prior to b5.84, this was CLIMATE_TROPICAL 3
#define CLIM_HOT  BIT(4)	// e. climate is warmer than normal
#define CLIM_COLD  BIT(5)	// f. climate is colder than normal
#define CLIM_HIGH  BIT(6)	// g. higher than normal (peaks)
#define CLIM_LOW  BIT(7)	// h. lower than normal (depression)
#define CLIM_MAGICAL  BIT(8)	// i. magical in some way
#define CLIM_TEMPERATE  BIT(9)	// j. temperate climate
#define CLIM_ARID  BIT(10)	// k. arid climate or desert
#define CLIM_TROPICAL  BIT(11)	// l. tropical climate
#define CLIM_MOUNTAIN  BIT(12)	// m. mountainous climate
#define CLIM_RIVER  BIT(13)	// n. moving (fresh) water
#define CLIM_FRESH_WATER  BIT(14)	// o. lake, pond; non-moving water
#define CLIM_SALT_WATER  BIT(15)	// p. ocean, sea; salt water
#define CLIM_FOREST  BIT(16)	// q. forested
#define CLIM_GRASSLAND  BIT(17)	// p. plains, savannah, steppe
#define CLIM_COASTAL  BIT(18)	// q. marks the edge on either the ocean or grassland side
#define CLIM_OCEAN  BIT(19)	// r. out to sea (compare to salt-water which could also be a lake)
#define CLIM_LAKE  BIT(20)	// s. either fresh or salt water
#define CLIM_WATERSIDE  BIT(21)	// t. adjacent to fresh water
#define CLIM_MILD  BIT(22)	// u. reduces temperature effects of other terrains
#define CLIM_HARSH  BIT(23)	// v. increases temperature effects of other terrains
#define CLIM_FROZEN_WATER  BIT(24)	// w. this would be a water sect but it's frozen


// DPLTN_x: depletion types
#define DPLTN_DIG  0
#define DPLTN_FORAGE  1
#define DPLTN_GATHER  2
#define DPLTN_PICK  3
#define DPLTN_FISH  4
#define DPLTN_QUARRY  5
#define DPLTN_PAN  6
#define DPLTN_TRAPPING  7
#define DPLTN_CHOP  8
#define DPLTN_HUNT  9
#define DPLTN_PRODUCTION  10	// for workforce
#define DPLTN_SECONDARY  11		// for workforce
#define DPLTN_TERTIARY  12		// for workforce
#define NUM_DEPLETION_TYPES  13	// total


// EVO_x: world evolutions
#define EVO_CHOPPED_DOWN  0	// sect it becomes when a tree is removed, [value=# of trees]: controls chopping
#define EVO_CROP_GROWS  1	// e.g. 'seeded field' crop-grows to 'crop' [no value]
#define EVO_ADJACENT_ONE  2	// called when adjacent to at least 1 of [value=sector]
#define EVO_ADJACENT_MANY  3	// called when adjacent to at least 6 of [value=sector]
#define EVO_RANDOM  4	// checked periodically with no trigger other than percent [no value]
#define EVO_TRENCH_START  5	// sector this becomes when someone starts an excavate [no value]
#define EVO_TRENCH_FULL  6	// sector this trench becomes when it fills with water [no value]
#define EVO_NEAR_SECTOR  7	// called when within 2 tiles of [value=sector]
#define EVO_PLANTS_TO  8	// set when someone plants [no value]
#define EVO_MAGIC_GROWTH  9	// called when Chant of Nature or similar is called
#define EVO_NOT_ADJACENT  10	// called when NOT adjacent to at least 1 of [value=sector]
#define EVO_NOT_NEAR_SECTOR  11 // called when NOT within 2 tiles of [value=sector]
#define EVO_SPRING  12	// triggers if it's spring
#define EVO_SUMMER  13	// triggers if it's summer
#define EVO_AUTUMN  14	// triggers if it's autumn
#define EVO_WINTER  15	// triggers if it's winter
#define EVO_BURNS_TO  16	// caused by a player burning it
#define EVO_SPREADS_TO  17	// reverse of adjacent-one
#define EVO_HARVEST_TO  18	// always harvests to a specific sector type (no matter what it was when planted)
#define EVO_DEFAULT_HARVEST_TO  19	// sect it becomes when harvested/cleared IF no data exists
#define EVO_TIMED  20	// evolves after a certain number of minutes
#define EVO_OWNED  21	// evolves if owned
#define EVO_UNOWNED  22	// evolves if un-owned
#define EVO_BURN_STUMPS  23	// uses the burn-stumps workforce to evolve
#define EVO_ADJACENT_SECTOR_FLAG  24	// evolves adjacent to a sector flag
#define EVO_NOT_ADJACENT_SECTOR_FLAG  25	// evolves when NOT adjacent to a sector flag
#define EVO_NEAR_SECTOR_FLAG  26	// evolves within 2 tiles of a sector flag
#define EVO_NOT_NEAR_SECTOR_FLAG  27	// evolves when NOT within 2 tiles of a sector flag
#define NUM_EVOS  28	// total

// EVO_VAL_x: evolution value types
#define EVO_VAL_NONE  0
#define EVO_VAL_SECTOR  1
#define EVO_VAL_NUMBER  2
#define EVO_VAL_SECTOR_FLAG  3


// Exit info (doors)
#define EX_ISDOOR  BIT(0)	// Exit is a door
#define EX_CLOSED  BIT(1)	// The door is closed


// ISLE_x: Island flags
#define ISLE_NEWBIE  BIT(0)	// a. Island follows newbie rules
#define ISLE_NO_AGGRO  BIT(1)	// b. Island will not fire aggro mobs or guard towers
#define ISLE_NO_CUSTOMIZE  BIT(2)	// c. cannot be renamed
#define ISLE_CONTINENT  BIT(3)	// d. island is a continent (usually large, affects spawns)
#define ISLE_HAS_CUSTOM_DESC  BIT(4)	// e. ** island has a custom desc -- internal use only (not having this flag will get the desc replaced)
#define ISLE_NO_CHART  BIT(5)	// f. island can't be targeted with the chart command
#define ISLE_NO_TEMPERATURE_PENALTIES  BIT(6)	// g. players are not penalized for heat/cold here
#define ISLE_ALWAYS_LIGHT  BIT(7)	// h. outdoor tiles are always light


// ROOM_AFF_x: Room affects -- these are similar to room flags, but if you want to set them
// in a "permanent" way, set the room's base_affects as well as its current
// affects.
#define ROOM_AFF_MAGIC_DARKNESS  BIT(0)	// a. Torches don't work; requires a ptech to see
#define ROOM_AFF_SILENT  BIT(1)	// b. Can't speak/hear
#define ROOM_AFF_HAS_INSTANCE  BIT(2)	// c. an instance is linked here
#define ROOM_AFF_CHAMELEON  BIT(3)	// d. Appears to be plains/desert
#define ROOM_AFF_TEMPORARY  BIT(4)	// e. Deleted when done (e.g. instances)
#define ROOM_AFF_NO_EVOLVE  BIT(5)	// f. Trees/etc won't grow here
#define ROOM_AFF_UNCLAIMABLE  BIT(6)	// g. Cannot be claimed
#define ROOM_AFF_PUBLIC  BIT(7)	// h. Empire allows public use
#define ROOM_AFF_DISMANTLING  BIT(8)	// i. Being dismantled
#define ROOM_AFF_NO_FLY  BIT(9)	// j. Can't fly there
#define ROOM_AFF_NO_WEATHER  BIT(10)	// k. Blocks normal weather messages
#define ROOM_AFF_IN_VEHICLE  BIT(11)	// l. Part of a vehicle
#define ROOM_AFF_NO_WORK  BIT(12)	// m. workforce ignores this room
#define ROOM_AFF_NO_DISREPAIR  BIT(13)	// n. will not fall into disrepair
#define ROOM_AFF_NO_DISMANTLE  BIT(14)	// o. blocks normal dismantle until turned off
#define ROOM_AFF_INCOMPLETE  BIT(15)	// p. building is incomplete
#define ROOM_AFF_NO_TELEPORT  BIT(16)	// q. cannot teleport
#define ROOM_AFF_BRIGHT_PAINT  BIT(17)	// r. paint is bright color
#define ROOM_AFF_FAKE_INSTANCE  BIT(18)	// s. room is a fake_loc for an instance
#define ROOM_AFF_NO_ABANDON  BIT(19)	// t. cannot abandon by accident
#define ROOM_AFF_REPEL_NPCS  BIT(20)	// u. all npcs are prevented from wandering in
#define ROOM_AFF_REPEL_ANIMALS  BIT(21)	// v. animals are prevented from wandering in
#define ROOM_AFF_NO_WORKFORCE_EVOS  BIT(22)	// w. workforce chores that would evolve the tile don't run
#define ROOM_AFF_HIDE_REAL_NAME  BIT(23)	// x. won't show the real name after a custom name, like Ruins of a House (Ruins)
#define ROOM_AFF_MAPOUT_BUILDING  BIT(24)	// y. shows as a building on the mapout (set automatically)
#define ROOM_AFF_NO_TRACKS  BIT(25)		// z. nobody leaves tracks and you cannot track
#define ROOM_AFF_PERMANENT_PAINT  BIT(26)	// A. paint displays different and cannot be repainted; removed automatically on dismantle
// NOTE: limit BIT(31) -- This is currently an unsigned int, to save space since there are a lot of rooms in the world


// ROOM_EXTRA_x: For various misc numbers attached to rooms
// WARNING: Make sure you have a place these are set, a place they are read,
// and *especially* a place they are removed. -pc
#define ROOM_EXTRA_PROSPECT_EMPIRE  0
#define ROOM_EXTRA_MINE_AMOUNT  1
#define ROOM_EXTRA_FIRE_REMAINING  2
#define ROOM_EXTRA_SEED_TIME  3
	#define ROOM_EXTRA_UNUSED4  4	// formerly tavern type
	#define ROOM_EXTRA_UNUSED5  5	// formerly tavern brewing time
	#define ROOM_EXTRA_UNUSED6  6	// formerly tavern available time
	#define ROOM_EXTRA_UNUSED  7	// formerly ruins-icon
#define ROOM_EXTRA_CHOP_PROGRESS  8
#define ROOM_EXTRA_TRENCH_PROGRESS  9
#define ROOM_EXTRA_HARVEST_PROGRESS  10
#define ROOM_EXTRA_PAINT_COLOR  11
#define ROOM_EXTRA_DEDICATE_ID  12
#define ROOM_EXTRA_BUILD_RECIPE  13
#define ROOM_EXTRA_FOUND_TIME  14
#define ROOM_EXTRA_REDESIGNATE_TIME  15
#define ROOM_EXTRA_CEDED  16	// used to mark that a room was ceded to someone and never used by the empire, to prevent cede+steal
#define ROOM_EXTRA_MINE_GLB_VNUM  17
#define ROOM_EXTRA_TRENCH_FILL_TIME  18  // when the trench will be filled
#define ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR  19	// for un-trenching correctly
#define ROOM_EXTRA_ORIGINAL_BUILDER  20	// person who started the building
#define ROOM_EXTRA_SECTOR_TIME  21	// when it became this sector type (for types with a timed evo)
#define ROOM_EXTRA_WORKFORCE_PROSPECT  22	// progress tracker for workforce


// SAVE_INFO_x: flags related to saving (this is a ubyte: only holds 8 bits)
#define SAVE_INFO_PACK_SAVED  BIT(0)	// indicates an obj/veh pack is already saved


// TEMPERATURE_x: Temperature flags for adventures, buildings, and room templates
#define TEMPERATURE_USE_CLIMATE  0	// inherit from local tile
#define TEMPERATURE_ALWAYS_COMFORTABLE  1	// no penalties from temperature
#define TEMPERATURE_MILDER  2	// milder than local tile
#define TEMPERATURE_HARSHER  3	// harsher than local tile
#define TEMPERATURE_FREEZING  4	// always freezing
#define TEMPERATURE_COLD  5	// always cold
#define TEMPERATURE_COOL  6	// always cool (no penalty)
#define TEMPERATURE_COOLER  7	// cooler than local tile
#define TEMPERATURE_COOLER_WHEN_HOT  8	// cooler than local tile when > 0
#define TEMPERATURE_NEUTRAL  9	// temperature is always 0 (neutral)
#define TEMPERATURE_WARM  10	// always warm (no penalty)
#define TEMPERATURE_WARMER  11	// warmer than local tile
#define TEMPERATURE_WARMER_WHEN_COLD  12	// warmer than local tile when < 0
#define TEMPERATURE_HOT  13	// always hot
#define TEMPERATURE_SWELTERING  14	// always sweltering


 //////////////////////////////////////////////////////////////////////////////
//// MAXIMA AND LIMITS ///////////////////////////////////////////////////////

// misc game limits
#define BANNED_SITE_LENGTH    50	// how long ban hosts can be
#define COLREDUC_SIZE  80	// how many characters long a color_reducer string can be
#define MAX_ACTION_STRING  245	// any longer and it cannot be read from file
#define MAX_ADMIN_NOTES_LENGTH  2400
#define MAX_AFFECT  32
#define MAX_CMD_LENGTH  (MAX_STRING_LENGTH-80)	// can't go bigger than this, altho DG Scripts wanted 16k
#define MAX_COIN  2140000000	// 2.14b (< MAX_INT)
#define MAX_COIN_TYPES  10	// don't store more than this many different coin types
#define MAX_CONDITION  (REAL_UPDATES_PER_MUD_HOUR * 24 * 2)	// FULL, etc: 2 days of hunger/thirst
#define MAX_CONFIG_TEXT  4000	// long-string configs
#define MAX_EMPIRE_DESCRIPTION  2000
#define MAX_FACTION_DESCRIPTION  4000
#define MAX_GROUP_SIZE  4	// how many members a group allows
#define MAX_IGNORES  15	// total number of players you can ignore
#define MAX_INPUT_LENGTH  1024	// Max length per *line* of input
#define MAX_INT  2147483647	// useful for bounds checking
#define MAX_INVALID_NAMES  200	// ban.c
#define MAX_ISLAND_NAME  40	// island name length -- seems more than reasonable
#define MAX_ITEM_DESCRIPTION  8000
#define MAX_MAIL_SIZE  4096	// arbitrary
#define MAX_MOTD_LENGTH  4000	// eedit.c, configs
#define MAX_NAME_LENGTH  20
#define MAX_NOTES  8000
#define MAX_PLAYER_DESCRIPTION  4000
#define MAX_POOFIN_LENGTH  80
#define MAX_POOF_LENGTH  80
#define MAX_PROMPT_SIZE  120
#define MAX_PWD_LENGTH  32
#define MAX_RANKS  20	// Max levels in an empire
#define MAX_RANK_LENGTH  20	// length limit
#define MAX_RAW_INPUT_LENGTH  1536  // Max size of *raw* input
#define MAX_RECENT_CHANNELS  20		// number of messages to show in each history
#define KEEP_RECENT_CHANNELS  60	// total number of messages to keep just in case some are hidden
#define MAX_REFERRED_BY_LENGTH  160
#define MAX_ROOM_DESCRIPTION  4000
#define MAX_SKILL_RESETS  10	// number of skill resets you can save up
#define MAX_SLASH_CHANNEL_NAME_LENGTH  16
#define MAX_STORAGE  1000000	// empire storage cap, must be < MAX_INT
#define MAX_STRING_LENGTH  8192
#define MAX_TITLE_LENGTH  100
#define MAX_TITLE_LENGTH_NO_COLOR  80	// title limit without color codes (less than MAX_TITLE_LENGTH)
#define NUM_ACTION_VNUMS  3	// action vnums 0, 1, 2
#define NUM_OBJ_VAL_POSITIONS  3	// GET_OBJ_VAL(obj, X) -- caution: changing this will require you to change the .obj file format
#define NUM_GLB_VAL_POSITIONS  3	// GET_GLOBAL_VAL(glb, X) -- caution: changing this will require you to change the .glb file format
#define NUM_SKILL_SETS  2	// number of different sets a player has
#define TRACK_MOVE_TIMES  20	// player's last X move timestamps


/*
 * A MAX_PWD_LENGTH of 10 will cause BSD-derived systems with MD5 passwords
 * and GNU libc 2 passwords to be truncated.  On BSD this will enable anyone
 * with a name longer than 5 character to log in with any password.  If you
 * have such a system, it is suggested you change the limit to 20.
 *
 * Please note that this will erase your player files.  If you are not
 * prepared to do so, simply erase these lines but heed the above warning.
 */
#if defined(HAVE_UNSAFE_CRYPT) && MAX_PWD_LENGTH < 20
#error You need to increase MAX_PWD_LENGTH to at least 20.
#error See the comment near these errors for more explanation.
#endif


  /////////////////////////////////////////////////////////////////////////////
 //// STRUCTS SECTION ////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS STRUCTS ///////////////////////////////////////////////////

// for character affect expiration
struct affect_expire_event_data {
	char_data *character;
	struct affected_type *affect;
};


// apply types for augments and morphs
struct apply_data {
	int location;	// APPLY_
	int weight;	// what percent of points go to this
	struct apply_data *next;	// linked list
};


// Simple affect structure
struct affected_type {
	any_vnum type;	// The type of spell that caused this
	int cast_by;	// player ID (positive) or mob vnum (negative)
	long expire_time;	// timestamp when the affect will expire -- note: -1 when unlimited; these save as "number of seconds remaining" in player files
	int modifier;	// This is added to apropriate ability
	byte location;	// Tells which ability to change - APPLY_
	bitvector_t bitvector;	// Tells which bits to set - AFF_
	
	struct dg_event *expire_event;	// expiry is handled by events; if scheuled, one is linked here
	
	struct affected_type *next;
};


// global messages
struct automessage {
	int id;
	int author;	// player id
	long timestamp;
	
	char *msg;	// the text
	
	int timing;	// AUTOMSG_ types
	int interval;	// minutes, for repeating
	
	UT_hash_handle hh;	// hash handle (automessages_table, by id)
};


// for sitebans
struct ban_list_element {
	char site[BANNED_SITE_LENGTH+1];
	int type;
	time_t date;
	char name[MAX_NAME_LENGTH+1];
	
	struct ban_list_element *next;
};


// for queuing up data that needs to be updated
struct char_delayed_update {
	int id;	// unique id; use char_script_id()
	char_data *ch;	// person to update
	bitvector_t type;	// CDU_ update type
	UT_hash_handle hh;	// hash handle (by id)
};


// data storage for config system
union config_data_union {
	bitvector_t bitvector_val;
	bool bool_val;
	double double_val;
	int int_val;
	int *int_array;
	char *string_val;
};


// for the global config system
struct config_type {
	int set;	// CONFIG_
	char *key;	// string key
	int type;	// CONFTYPE_x how to access the data
	char *description;	// long desc for contextual help
	
	union config_data_union data;	// whatever type of data is stored here (based on type)
	int data_size;	// for array data
	
	// for types with their own handlers
	CONFIG_HANDLER(*show_func);
	CONFIG_HANDLER(*edit_func);
	void *custom_data;
	sh_int config_flags;	// set in init_config_system
	
	UT_hash_handle hh;	// config_table hash
};


// used for the external 'evolve' tool and its imports
struct evo_import_data {
	room_vnum vnum;
	sector_vnum old_sect;
	sector_vnum new_sect;
};


// Extra descriptions
struct extra_descr_data {
	char *keyword;	// Keyword in look/examine
	char *description;	// What to see
	
	struct extra_descr_data *next;
};


// for do_file, act.immortal.c
struct file_lookup_struct {
	char *cmd;
	int level;
	char *file;
};


// for do_findmaintenance and do_territory
struct find_territory_node {
	room_data *loc;
	char *details;	// optional string with vehicles, etc
	int count;
	bool is_vehicle;	// notes if the marked territory was from a vehicle
	
	struct find_territory_node *prev, *next;	// doubly-linked list
};


// for do_gen_interact_room, act.actions.c
struct gen_interact_data_t {
	int interact;	// INTERACT_ type
	int action;	// ACT_ type
	char *command;	// 'quarry'
	char *verb;	// 'quarrying'
	char *timer_config;	// optional: config to get the timer (NULL to not use)
	int timer;	// number of action ticks to use if a config is not set
	int ptech;	// required ptech (may be NO_TECH)
	char *approval_config;	// a 'bool' key for the config system like "gather_approval" (may be null)
	bitvector_t tool;	// any TOOL_ needed
	GEN_INTERACT_SPECIAL(*spec_proc);	// optional spec to run during the process such as gen_proc_nature_burn
	bitvector_t flags;	// GI_ flags
	struct {	// for all strings, index 0 is to-char and index 1 is to-room
		char *start[2];	// shown at start-action
		char *pre_finish;	// shown before attempting the resource interaction
		char *finish[2];	// shown when resource is gained (unless there's a custom message), may contain $p
		char *empty;	// char-only, shown at the end if there's nothing they can get
		int random_frequency;	// 1-100% chance to see a message per tick
		char *random_tick[10][2];
	} msg;
	int custom_tool_message_to_char;	// optional OBJ_CUSTOM_ type; may be NOTHING
	int custom_tool_message_to_room;	// optional OBJ_CUSTOM_ type; may be NOTHING
};


// generic name system
struct generic_name_data {
	int name_set;	// NAMES_x const
	int sex;	// male/female/both (each list id has multiple gender lists)
	char **names;
	int size;	// size of names array
	
	struct generic_name_data *next;	// LL
};


// for global tables
struct global_data {
	any_vnum vnum;
	char *name;	// descriptive text
	int type;	// GLOBAL_
	bitvector_t flags;	// GLB_FLAG_ flags
	int value[NUM_GLB_VAL_POSITIONS];	// misc vals
	
	// constraints
	any_vnum ability;	// must have this to trigger, unless NO_ABIL
	double percent;	// chance to trigger
	bitvector_t type_flags;	// type-dependent flags
	bitvector_t type_exclude;	// type-dependent flags
	bitvector_t spare_bits;	// more flags that can be used by various globals
	int min_level;
	int max_level;
	
	// data
	struct interaction_item *interactions;	// GLOBAL_MINE_DATA, GLOBAL_MOB_INTERAXTIONS
	struct archetype_gear *gear;	// GLOBAL_NEWBIE_GEAR
	struct spawn_info *spawns;	// GLOBAL_MAP_SPAWNS
	
	UT_hash_handle hh;
};


// for GLB_VALIDATOR
struct glb_emp_bean {
	empire_data *empire;
};


// for GLB_VALIDATOR
struct glb_room_emp_bean {
	room_data *room;
	empire_data *empire;
};


// group member list
struct group_member_data {
	char_data *member;
	struct group_member_data *next;	// linked list
};


// Group Data Struct
struct group_data {
	char_data *leader;
	struct group_member_data *members;	// LL
	bitvector_t group_flags;	// GROUP_x flags
	
	struct group_data *next;	// global group LL
};


// for help files
struct help_index_element {
	char *keyword;
	char *entry;
	int level;
	int duplicate;
};


// icon data for things that appear on the map (sects, crops)
struct icon_data {
	int type;	// TILESET_x
	char *icon;  // ^^^^
	char *color;  // e.g. &g

	struct icon_data *next;
};


// element in mob/obj/trig indexes
struct index_data {
	int vnum;	// virtual number of this
	int number;	// number of existing units of this mob/obj

	char *farg;	// string argument for special function
	trig_data *proto;	// for triggers... the trigger
};


// for various hashes that just need an int
struct int_hash {
	int num;	// the int
	UT_hash_handle hh;	// hash handle
};


// for exclusive interaction processing
struct interact_exclusion_data {
	int code;	// a-z, A-Z, or empty (stored as an int for easy-int funcs)
	int rolled_value;	// 1-10000
	int cumulative_value;	// value so far
	bool done;	// TRUE if one already passed
	UT_hash_handle hh;
};


// restricts interactions to certain players
struct interact_restriction {
	int type;	// INTERACT_RESTRICT_ type
	signed long long vnum;	// based on type; support most bitvectors too
	struct interact_restriction *next;
};


// for the "interactions" system (like butcher, dig, etc)
struct interaction_item {
	int type;	// INTERACT_
	obj_vnum vnum;	// the skin object
	double percent;	// how often to do it 0.01 - 100.00
	int quantity;	// how many to give
	char exclusion_code;	// creates mutually-exclusive sets
	
	struct interact_restriction *restrictions;	// linked list
	
	struct interaction_item *next;
};


// see morph.c
struct morph_data {
	any_vnum vnum;
	char *keywords;
	char *short_desc;	// short description (a bat)
	char *long_desc;	// long description (seen in room)
	char *look_desc;	// desc shown on look-at
	
	bitvector_t flags;	// MORPHF_ flags
	bitvector_t affects;	// AFF_ flags added
	int attack_type;	// TYPE_ const
	int move_type;	// MOVE_TYPE_ const
	int size;	// SIZE_ const for this form
	struct apply_data *applies;	// how it modifies players
	
	int cost_type;	// any pool (NUM_POOLS)
	int cost;	// amount it costs in that pool
	any_vnum ability;	// required ability or NO_ABIL
	obj_vnum requires_obj;	// required item or NOTHING
	int max_scale;	// highest possible level
	
	UT_hash_handle hh;	// morph_table hash
	UT_hash_handle sorted_hh;	// sorted_morphs hash
};


// for items -- this replaces CircleMUD's obj_affected_type
struct obj_apply {
	byte apply_type;	// APPLY_TYPE_x
	byte location;	// Which ability to change (APPLY_)
	sh_int modifier;	// How much it changes by
	
	struct obj_apply *next;	// linked list
};


// for do_accept/reject
struct offer_data {
	int from;	// player id
	int type;	// OFFER_x
	room_vnum location;	// where the player was
	time_t time;	// when the offer happened
	int data;	// misc data
	struct offer_data *next;
};


// simple structure for passing around a hash of number pairs { id, value }
struct pair_hash {
	int id;
	int value;
	UT_hash_handle hh;
};


// pathfind.c: one step in the path
struct pathfind_node {
	room_data *inside_room;	// if it's an interior, this is the current room
	struct map_data *map_loc;	// if it's on the map, this is the current loc
	struct pathfind_node *parent;	// last node in the path
	
	double steps;	// total steps taken (diagonals count more)
	int cur_dir;	// last direction moved
	int cur_dist;	// number of times it was moved
	
	double estimate;	// estimated distance, for prioritizing
	
	struct pathfind_node *prev, *next;	// doubly-linked list
};


// pathfind.c: handles the data for pathfinding
struct pathfind_controller {
	room_data *start;	// start pos
	room_data *end;	// destination
	int end_x, end_y;	// target coordinates (prevent repeat lookups)
	
	int key;	// initialized with get_pathfind_key()
	
	struct pathfind_node *nodes;	// doubly-linked list of nodes to check
	struct pathfind_node *free_nodes;	// doubly-linked list of nodes that have been checked
};


// for ad tracking and special promotions
struct promo_code_list {
	char *code;
	bool expired;
	PROMO_APPLY(*apply_func);
};


// a pre-requisite or requirement for a quest
struct req_data {
	int type;	// REQ_ type
	any_vnum vnum;
	bitvector_t misc;	// stores flags for some types
	char group;	// for "AND" grouping
	
	int needed;	// how many the player needs
	int current;	// how many the player has (for places where this data is tracked
	
	char *custom;	// custom display text, may be NULL
	
	struct req_data *next;
};


// for act.highsorcery.c ritual strings
struct ritual_strings {
	char *to_char;
	char *to_room;
};


// for the shipping system
struct shipping_data {
	obj_vnum vnum;
	int amount;
	int from_island;	// island of origin
	int to_island;	// destination island
	room_vnum to_room;	// optional: exact destination
	int status;	// SHIPPING_
	long status_time;	// when it gained that status
	room_vnum ship_origin;	// where the ship is coming from (in case we have to send it back)
	int shipping_id;	// VEH_IDNUM() of ship, if any
	struct storage_timer *timers;	// for items that decay
	
	struct shipping_data *prev, *next;	// DL: EMPIRE_SHIPPING_LIST()
};


// skills.c
struct skill_data {
	any_vnum vnum;
	char *name;
	char *abbrev;
	char *desc;
	
	bitvector_t flags;	// SKILLF_ flags
	int max_level;	// skill's maximum level (default 100)
	int min_drop_level;	// how low the skill can be dropped manually (default 0)
	struct skill_ability *abilities;	// assigned abilities
	struct synergy_ability *synergies;	// LL of abilities gained from paired skills
	
	UT_hash_handle hh;	// skill_table hash handle
	UT_hash_handle sorted_hh;	// sorted_skills hash handle
};


// for attaching abilities to skills
struct skill_ability {
	any_vnum vnum;	// ability vnum
	any_vnum prerequisite;	// required ability vnum
	int level;	// skill level to get this ability
	
	struct skill_ability *next;	// linked list
};


// for abilities you gain when you have this skill at its max and another skill at <level>
struct synergy_ability {
	int role;	// ROLE_ const
	any_vnum skill;	// skill required
	int level;	// level required in that skill
	any_vnum ability;	// ability to gain
	
	int unused;	// for future expansion
	
	struct synergy_ability *next;	// LL
};


// for act.comm.c
struct slash_channel {
	int id;
	char *name;
	char *lc_name;	// lowercase name (for speed)
	char color;
	struct channel_history_data *history;
	
	struct slash_channel *next;
};


// for mob spawning in sects, buildings, etc
struct spawn_info {
	mob_vnum vnum;
	double percent;	// .01 - 100.0
	bitvector_t flags;	// SPAWN_
	
	struct spawn_info *next;
};


// stats data
struct stats_data_struct {
	any_vnum vnum;
	int count;
	
	UT_hash_handle hh;	// hashable
};


// simple structure for passing around a hash of unique strings
struct string_hash {
	char *str;
	int count;
	UT_hash_handle hh;
};


// for text file loading (and do_tedit in act.immortal.c)
struct text_file_data_type {
	char *name;
	char *filename;
	bool can_edit;
	int level;	// to edit
	int size;	// when editing
};


/* This structure is purely intended to be an easy way to transfer */
/* and return information about time (real or mudwise).            */
struct time_info_data {
	int hours, day, month;
	sh_int year;
};


// for trading_post_list
struct trading_post_data {
	int player;	// player idnum who posted the auction
	bitvector_t state;	// TPD_x (state flags)
	
	long start;	// when the auction began
	int for_hours;	// how long the auction should run
	
	obj_data *obj;	// the item up for auction
	int buy_cost;	// # of coins to buy it
	int post_cost;	// # of coins paid to post it
	empire_vnum coin_type;	// empire vnum or OTHER_COIN for buy/post coins
	
	struct trading_post_data *prev, *next;	// DLL
};


// simple structure for passing around a list of vnums
struct vnum_hash {
	any_vnum vnum;
	int count;
	UT_hash_handle hh;
};


 //////////////////////////////////////////////////////////////////////////////
//// ABILITY STRUCTS /////////////////////////////////////////////////////////

// abilities.c
struct ability_data {
	any_vnum vnum;
	char *name;
	
	// properties
	bitvector_t flags;	// ABILF_ flags
	any_vnum mastery_abil;	// used for crafting abilities
	struct ability_type *type_list;	// types with properties
	double scale;	// effectiveness scale (1.0 = 100%)
	bitvector_t immunities;	// AFF_ flags that block this ability
	bitvector_t gain_hooks;	// AGH_ flags
	bitvector_t requires_tool;	// TOOL_ flags required to use it
	
	// command-related data
	char *command;	// if ability has a command
	byte min_position;	// to use the command
	bitvector_t targets;	// ATAR_ flags
	int cost_type;	// HEALTH, MANA, etc.
	int cost;	// amount of h/v/m
	double cost_per_amount;		// cost modifier for healing/damage
	double cost_per_scale_point;	// cost modifier when scaled
	double cost_per_target;		// cost modifier with multiple targets
	struct resource_data *resource_cost;	// additional costs
	any_vnum cooldown;	// generic cooldown if any
	int cooldown_secs;	// how long to cooldown, if any
	int wait_type;	// WAIT_ flag
	int linked_trait;	// APPLY_ type that this scales with
	int difficulty;	// DIFF_ type, if any
	struct custom_message *custom_msgs;	// any custom messages
	
	// type-specific data
	any_vnum affect_vnum;	// affects
	int short_duration;	// affects
	int long_duration;	// affects
	bitvector_t affects;	// affects
	struct apply_data *applies;	// affects
	int attack_type;	// damage
	int damage_type;	// damage
	int pool_type;	// restore
	int max_stacks;	// dot
	int move_type;	// for move abilities
	struct ability_data_list *data;	// LL of additional data
	struct interaction_item *interactions;	// LL of regular interactions
	struct ability_hook *hooks;	// LL of hooks for this ability
	
	// live cached (not saved) data:
	skill_data *assigned_skill;	// skill for reverse-lookup
	int skill_level;	// level of that skill required
	bitvector_t types;	// summary of ABILT_ flags
	bitvector_t hook_flags;	// quick reference for hook types
	bool is_class;	// assignment comes from a class
	bool is_synergy;	// assignemnt comes from a synergy
	
	UT_hash_handle hh;	// ability_table hash handle
	UT_hash_handle sorted_hh;	// sorted_abilities hash handle
};


// for abilities with misc data
struct ability_data_list {
	int type;	// ADL_ type of data list
	any_vnum vnum;	// number of the list entry thing
	signed long long misc;	// used by some types
	struct ability_data_list *next;
};


// for characters, tells an ability when to gain (other than when activated)
struct ability_gain_hook {
	any_vnum ability;
	bitvector_t triggers;	// AGH_ flags
	UT_hash_handle hh;	// GET_ABILITY_GAIN_HOOKS(ch)
};


// Ability hooks (things that cause an ability to run itself)
struct ability_hook {
	bitvector_t type;	// AHOOK_ const
	any_vnum value;	// depends on type
	double percent;	// chance to execute
	int misc;	// for expansion
	
	struct ability_hook *next;	// LL
};


// used to detect changes in attributes
struct ability_trait_hook {
	any_vnum ability;	// which ability to update
	int linked_trait;	// which trait to watch
	int last_value;		// last known value of the trait
	
	UT_hash_handle hh;	// GET_ABILITY_TRAIT_HOOKS(ch)
};


// determines the weight for each type, to affect scaling
struct ability_type {
	bitvector_t type;	// a single ABILT_ flag
	int weight;	// how much weight to give that type
	struct ability_type *next;
};


// passes data throughout an ability call
struct ability_exec {
	ability_data *abil;	// which ability (pointer)
	bool stop;	// indicates no further types should process
	bool success;	// indicates the player should be charged
	bool sent_any_msg;	// if TRUE, it sent at least one message to the actor
	bool sent_fail_msg;	// if TRUE, it sent at least one fail
	bool no_msg;	// indicates you shouldn't send messages
	bool engage_anyway;	// we engage combat if !stop || engage_anyway
	
	// costs
	int cost;	// for types that raise the cost later
	bool should_charge_cost;	// if TRUE, will charge regardless of success
	int total_amount;	// for cost-per-amount
	int total_targets;	// for cost-per-target
	double max_scale;	// for cost-per-scale-point
	
	// core scaling data
	bool has_mastery;	// TRUE if the player has the mastery ability
	bool matching_role;	// if FALSE, has penalties from no matching role
	double trait_modifier;	// 0.0 to 1.0 rating based on the required trait
	
	// data passed by types
	int conjure_liquid_max;
	int move_dir;
	room_data *move_room;
	any_vnum ready_weapon_val;
	int restore_pool;
	int restore_amount;
	any_vnum summon_vnum;
	
	struct ability_exec_type *types;	// LL of type data
};


// preliminary data from ability setup
struct ability_exec_type {
	bitvector_t type;
	double scale_points;
	struct ability_exec_type *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// ADVENTURE STRUCTS ///////////////////////////////////////////////////////

// adventure zones
struct adventure_data {
	adv_vnum vnum;
	
	// strings
	char *name;
	char *author;
	char *description;
	
	// numeric data
	rmt_vnum start_vnum, end_vnum;	// room template vnum range (inclusive)
	int min_level, max_level;	// level range
	int max_instances;	// total number possible in world
	int reset_time;	// how often to reset things (minutes)
	bitvector_t flags;	// ADV_ flags
	int player_limit;	// maximum number of players at a time (if over 0)
	int temperature_type;	// TEMPERATURE_ const
	
	// lists
	struct adventure_link_rule *linking;
	
	// for cleanup triggers
	struct trig_proto_list *proto_script;	// list of default triggers
	
	UT_hash_handle hh;	// adventure_table hash
};


// how to link adventure zones
struct adventure_link_rule {
	int type;	// ADV_LINK_
	bitvector_t flags;	// ADV_LINKF_
	
	int value;	// e.g. building vnum, sector vnum to link from (by type)
	obj_vnum portal_in, portal_out;	// some types use portals
	int dir;	// some attachments take a preferred direction
	bitvector_t bld_on, bld_facing;	// for applying to the map
	any_vnum vehicle_vnum;	// for rules that use a vehicle
	
	struct adventure_link_rule *next;
};


// spawn info for adventure zones
struct adventure_spawn {
	int type;	// obj, mob, etc
	any_vnum vnum;	// thing to spawn
	double percent;	// .01 - 100.0
	int limit;	// maximum number that may spawn in 1 instance
	
	struct adventure_spawn *next;
};



// exit information for room templates
struct exit_template {
	int dir;	// which way
	char *keyword;	// name if it will be closable
	sh_int exit_info;	// EX_x
	rmt_vnum target_room;	// room template to link to
	
	// unsaved:
	bool done;	// used while instantiating rooms
	
	struct exit_template *next;
};


// instance info
struct instance_data {
	any_vnum id;	// instance id
	adv_data *adventure;	// which adventure this is an instance of
	
	bitvector_t flags;	// INST_ flags
	room_data *location;	// map room linked from
	room_data *fake_loc;	// for "roaming instances", a location set by script
	room_data *start;	// starting interior room (first room of zone)
	int level;	// locked, scaled level
	time_t created;	// when instantiated
	time_t last_reset;	// for reset timers
	
	// data stored ONLY for delayed load
	int dir;	// any (or no) direction the adventure might face
	int rotation;	// any direction the adventure is rotated, if applicable
	struct adventure_link_rule *rule;
	
	// unstored data
	int size;	// size of room arrays
	room_data **room;	// array of rooms (some == NULL)
	struct instance_mob *mob_counts;	// hash table (hh)
	bool cleanup;	// TRUE if the instance is expired and mid-cleanup
	
	struct instance_data *prev;
	struct instance_data *next;
};


// tracks the mobs in an instance
struct instance_mob {
	mob_vnum vnum;
	int count;
	UT_hash_handle hh;	// instance->mob_counts
};


// adventure zone room templates
struct room_template {
	rmt_vnum vnum;
	
	// strings
	char *title;
	char *description;
	
	// numeric data
	bitvector_t flags;	// RMT_
	bitvector_t base_affects;	// ROOM_AFF_
	bitvector_t functions;	// FNC_
	rmt_vnum subzone;	// for subdividing where/shout/etc
	int temperature_type;	// TEMPERATURE_ const
	
	// lists
	struct adventure_spawn *spawns;	// list of objs/mobs
	struct extra_descr_data *ex_description;	// extra descriptions
	struct exit_template *exits;	// list of exits
	struct interaction_item *interactions;	// interaction items
	struct trig_proto_list *proto_script;	// list of default triggers
	
	// live data (not saved, not freed)
	struct quest_lookup *quest_lookups;
	struct shop_lookup *shop_lookups;
	
	UT_hash_handle hh;	// room_template_table hash
};


 //////////////////////////////////////////////////////////////////////////////
//// ARCHETYPE STRUCTS ///////////////////////////////////////////////////////

struct archetype_data {
	// basic data
	any_vnum vnum;
	char *name;
	char *description;
	char *lore;	// optional lore entry
	int type;	// ARCHT_
	bitvector_t flags;	// ARCH_
	
	// default starting ranks
	char *male_rank;
	char *female_rank;
	
	generic_data *language;	// optional starting language (generic)
	struct archetype_skill *skills;	// linked list
	struct archetype_gear *gear;	// linked list
	int attributes[NUM_ATTRIBUTES];	// starting attributes (default 1)
	
	UT_hash_handle hh;	// archetype_table hash handle
	UT_hash_handle sorted_hh;	// sorted_archetypes hash handle
};


struct archetype_skill {
	any_vnum skill;	// SKILL_ type
	int level;	// starting level
	
	struct archetype_skill *next;
};


struct archetype_gear {
	int wear;	// WEAR_, -1 == inventory
	obj_vnum vnum;
	struct archetype_gear *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// ATTACK MESSAGE STRUCTS //////////////////////////////////////////////////

// AMDF_x: Attack message flags
#define AMDF_WEAPON			BIT(0)	// a. allowed on weapons
#define AMDF_MOBILE			BIT(1)	// b. allowed on mobiles
#define AMDF_DISARMABLE		BIT(2)	// c. can be disarmed
#define AMDF_APPLY_POISON	BIT(3)	// d. can trigger poison
#define AMDF_IGNORE_MISSING	BIT(4)	// e. auditor won't warn on missing messages

#define AMDF_FLAGS_REQUIRE_EXTENDED_DATA  (AMDF_WEAPON | AMDF_MOBILE)


// MSG_x: each fight message has these 4 types
#define MSG_DIE		0	// messages when death
#define MSG_MISS	1	// messages when miss
#define MSG_HIT		2	// messages when hit
#define MSG_GOD		3	// messages when hit on god
#define NUM_MSG_TYPES  4	// total


// SPD_x: speeds for attacks
#define SPD_FAST	0
#define SPD_NORMAL	1
#define SPD_SLOW	2
#define NUM_ATTACK_SPEEDS  3


// WEAPON_x: Weapon types
#define WEAPON_BLUNT	0
#define WEAPON_SHARP	1	// like wolverine
#define WEAPON_MAGIC	2


// fight message list
struct attack_message_data {
	any_vnum vnum;		// Attack vnum (usually an ATTACK_ or TYPE_ const)
	char *name;	// for display purposes
	char *death_log;	// optional message for how death is displayed
	bitvector_t flags;	// AMDF_ flags
	any_vnum counts_as;	// if it counts as another attack type, for ability requirements etc
	
	// used by attack types:
	char *first_pers;	// You "slash"
	char *third_pers;	// $n "slashes"
	char *noun;		// ... with your "swing"
	double speed[NUM_ATTACK_SPEEDS];	// SPD_ { fast, normal, slow }
	int weapon_type;	// WEAPON_ type
	int damage_type;	// DAM_ type
	
	struct attack_message_set *msg_list;	// Linked list of messages
	int num_msgs;	// How many attack messages are in the list
	
	UT_hash_handle hh;	// attack_message_table hash (by vnum)
};


// individual trio of fight messages
struct attack_message_type {
	char *attacker_msg;	// message to attacker
	char *victim_msg;	// message to victim
	char *room_msg;	// message to room
};


// part of fight messages
struct attack_message_set {
	struct attack_message_type msg[NUM_MSG_TYPES];	// the 4 message types
	struct attack_message_set *next;	// to next messages of this kind
};


 //////////////////////////////////////////////////////////////////////////////
//// AUGMENT STRUCTS /////////////////////////////////////////////////////////

struct augment_data {
	any_vnum vnum;
	char *name;	// descriptive text
	int type;	// AUGMENT_x
	bitvector_t flags;	// AUG_x flags
	bitvector_t wear_flags;	// ITEM_WEAR_ where this augment applies
	
	any_vnum ability;	// required ability or NO_ABIL
	obj_vnum requires_obj;	// required item or NOTHING
	struct apply_data *applies;	// how it modifies items
	struct resource_data *resources;	// resources required
	
	UT_hash_handle hh;	// augment_table hash
	UT_hash_handle sorted_hh;	// sorted_augments hash
};


// new resource-required data (for augments, crafts, etc)
struct resource_data {
	int type;	// RES_ type
	obj_vnum vnum;	// which item
	int amount;	// how many
	int misc;	// various uses
	
	struct resource_data *next;	// linked list
};


 //////////////////////////////////////////////////////////////////////////////
//// BOOK STRUCTS ////////////////////////////////////////////////////////////

// Just helps keep a unique list of authors
struct author_data {
	int idnum;
	UT_hash_handle hh;	// author_table
};


// data for a book
struct book_data {
	book_vnum vnum;
	
	int author;
	bitvector_t flags;
	char *title;
	char *byline;
	char *item_name;
	char *item_description;
	
	struct paragraph_data *paragraphs;	// linked list
	
	UT_hash_handle hh;	// book_table
};


// single book in a library
struct library_book {
	any_vnum vnum;		// book vnum
	UT_hash_handle hh;	// hashed by vnum in the library
};


// tracks where books are stored
struct library_info {
	room_vnum room;	// location (hash key)
	struct library_book *books;	// hash of books (by vnum)
	
	UT_hash_handle hh;
};


 //////////////////////////////////////////////////////////////////////////////
//// BUILDING STRUCTS ////////////////////////////////////////////////////////

// building_table
struct bld_data {
	bld_vnum vnum;	// virtual number
	
	char *name;	// short name
	char *title;	// shown on look
	char *icon;	// map icon
	char *commands;	// listed under the room desc/map
	char *description;	// for non-map buildings
	
	int max_damage;
	int fame;	// how much is added to empire fame
	bitvector_t flags;	// BLD_
	bitvector_t functions;	// FNC_
	bld_vnum upgrades_to;	// the vnum of any building
	int height;	// 0+ addition to terrain height (map buildings only)
	int temperature_type;	// TEMPERATURE_ const
	
	int extra_rooms;	// how many rooms it can have
	bitvector_t designate_flags;	// DES_x
	bitvector_t base_affects;	// ROOM_AFF_
	
	int citizens;	// how many people live here
	int military;	// how much it adds to the military pool
	mob_vnum artisan_vnum;	// vnum of an artisan to load
	
	struct extra_descr_data *ex_description;	// extra descriptions
	struct spawn_info *spawns;	// linked list of spawn data
	struct interaction_item *interactions;	// interaction items
	struct trig_proto_list *proto_script;	// list of default triggers
	struct resource_data *regular_maintenance;	// needed each reset cycle
	struct bld_relation *relations;	// links to buildings/vehicles
	
	// live data (not saved, not freed)
	struct quest_lookup *quest_lookups;
	struct shop_lookup *shop_lookups;
	
	UT_hash_handle hh;	// building_table hash handle
};


// for relationships between buildings/vehicles
struct bld_relation {
	int type;	// BLD_REL_
	any_vnum vnum;	// relevant vnum
	
	struct bld_relation *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// CLASS STRUCTS ///////////////////////////////////////////////////////////

// class.c
struct class_data {
	any_vnum vnum;
	char *name;
	char *abbrev;	// short form (4 letters)
	
	bitvector_t flags;	// CLASSF_ flags
	int pools[NUM_POOLS];	// health/etc values at 100
	struct class_skill_req *skill_requirements;	// linked list
	struct class_ability *abilities;	// linked list
	
	UT_hash_handle hh;	// ability_table hash handle
	UT_hash_handle sorted_hh;	// sorted_abilities hash handle
};


// for skill requirements on a class
struct class_skill_req {
	any_vnum vnum;	// skill vnum
	int level;	// required skill level (needs to match exactly: 0, 50, 75, 100)
	struct class_skill_req *next;	// linked list
};


// for abilities assigned to a class
struct class_ability {
	any_vnum vnum;	// ability vnum
	int role;	// ROLE_ type
	struct class_ability *next;	// linked list
};


 //////////////////////////////////////////////////////////////////////////////
//// MAP STRUCTS /////////////////////////////////////////////////////////////

// header data for the binary map file
// WARNING: you cannot change the size of this struct without corrupting the binary_map file
struct map_file_header {
	int version;	// for versioning the map file to support changes to structure
	int width;	// tiles wide
	int height;	// tiles tall
};


// fixed-sized version of the map data for writing to binary files
// WARNING: you cannot change the size of this struct without corrupting the binary_map file
// to add to this struct, add a new version and leave this one so your game can still load your existing world file
struct map_file_data_v1 {
	int island_id;	// from shared data
		
	any_vnum sector_type;	// from map data
	any_vnum base_sector;
	any_vnum natural_sector;
	any_vnum crop_type;
	
	bitvector_t affects;	// from shared data
	bitvector_t base_affects;
	int height;
	long sector_time;
	
	int misc;	// used by the evolver
};


 //////////////////////////////////////////////////////////////////////////////
//// MOBILE STRUCTS //////////////////////////////////////////////////////////


// Specials used by NPCs, not PCs
struct mob_special_data {
	int current_scale_level;	// level the mob was scaled to, or -1 for not scaled
	int min_scale_level;	// minimum level this mob may be scaled to
	int max_scale_level;	// maximum level this mob may be scaled to
	
	int name_set;	// NAMES_x
	any_vnum language;	// default language (NOTHING to use global default instead)
	struct custom_message *custom_msgs;	// any custom messages
	faction_data *faction;	// if any
	
	int to_hit;	// Mob's attack % bonus
	int to_dodge;	// Mob's dodge % bonus
	int damage;	// Raw damage
	int	attack_type;	// weapon type
	
	byte move_type;	// how the mob moves
	
	struct pursuit_data *pursuit;	// mob pursuit
	room_vnum pursuit_leash_loc;	// where to return to
	struct mob_tag *tagged_by;	// mob tagged by

	time_t spawn_time;	// used for despawning
	any_vnum instance_id;	// instance the mob belongs to, or NOTHING if none
	
	// for #n-named mobs
	int dynamic_sex;
	int dynamic_name;
};


// for mobs/objs/abils custom action messages
struct custom_message {
	int type;	// OBJ_CUSTOM_ or MOB_CUSTOM_
	char *msg;
	
	struct custom_message *next;
};


// to prevent loot-/kill-stealing
struct mob_tag {
	int idnum;	// player id
	struct mob_tag *next;	// linked list
};


// for mob pursuit (mobact.c)
struct pursuit_data {
	int idnum;	// player id
	time_t last_seen;	// time of last spotting
	room_vnum location;	// prevents tracking forever
	
	// possible complications:
	char *disguise;
	any_vnum morph;
	
	struct pursuit_data *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER STRUCTS //////////////////////////////////////////////////////////

// player accounts
struct account_data {
	int id;	// corresponds to player_index_data account_id and player's saved account id
	struct account_player *players;	// linked list of players
	time_t last_logon;	// timestamp of the last login on the account
	bitvector_t flags;	// ACCT_
	char *notes;	// account notes
	
	// lists/hashes
	struct friend_data *friends;	// hash table of friends
	struct pk_data *killed_by;	// LL of players who killed this player recently
	struct unlocked_archetype *unlocked_archetypes;	// hash (vnum)
	
	UT_hash_handle hh;	// account_table
};


// list of players in account_data
struct account_player {
	char *name;	// stored on load in order to build index
	player_index_data *player;
	
	struct account_player *next;
};


// used in descriptor for personal channel histories -- act.comm.c
struct channel_history_data {
	int idnum;	// id of the author
	int invis_level;	// if it's an invisible immortal
	int access_level;	// not shown below this access level at all
	int rank;	// for empire chats, required rank
	any_vnum language;	// which language it was in, for some channels
	bool is_disguised;	// indicates we need to show a real name to immortals
	char *message;
	long timestamp;
	struct channel_history_data *prev, *next;	// doubly-linked list
};


// player money
struct coin_data {
	int amount;	// raw value
	empire_vnum empire_id;	// which empire (or NOTHING) for misc
	time_t last_acquired;	// helps cleanup
	
	struct coin_data *next;
};


// companion characters (familiars, bodyguards, and other combat pets)
struct companion_data {
	any_vnum vnum;	// mob vnum (unique)
	any_vnum from_abil;	// if not NOTHING, player can only use/list this companion while they have the abil
	bool instantiated;	// TRUE if it has ever been loaded as a mob
	
	struct companion_mod *mods;	// modifications to the companion
	struct trig_proto_list *scripts;	// list of triggers to attach
	struct trig_var_data *vars;	// global script variables on the mob
	
	UT_hash_handle hh;	// hashed by mob vnum
};


// changes to companions
struct companion_mod {
	byte type;	// CMOD_ type
	int num;	// numeric data (used by some types)
	char *str;	// string data (used by some types)
	struct companion_mod *next;
};


// account-wide friends list
struct friend_data {
	int account_id;			// id of the friend's account
	int status;				// FRIEND_ status
	char *name;				// last name seen
	char *original_name;	// name we added with
	UT_hash_handle hh;		// hash handle for account->friends
};


// players have a collection of lastnames
struct player_lastname {
	char *name;
	struct player_lastname *next;	// linked list: GET_LASTNAME_LIST(ch)
};


// track who/when a player has been killed by another player
struct pk_data {
	int killed_alt;	// id of which alt died
	int player_id;	// id of player who killed them
	any_vnum empire;	// which empire the killer belonged to
	long last_time;	// when the last kill was
	struct pk_data *next;
};


// player table
struct player_index_data {
	// these stats are all copied from the player on startup (and save_char)
	
	int idnum;	// player id
	char *name;	// character's login name
	char *fullname;	// character's display name
	int account_id;	// player's account id
	time_t last_logon;	// timestamp of last play time
	time_t birth;	// creation time
	int played;	// time played
	int access_level;	// player's access level
	bitvector_t plr_flags;	// PLR_: a copy of the last-saved player flags
	empire_data *loyalty;	// empire, if any
	int rank;	// empire rank
	char *last_host;	// last known host
	sh_int highest_known_level;	// player's highest-ever level
	
	UT_hash_handle idnum_hh;	// player_table_by_idnum
	UT_hash_handle name_hh;	// player_table_by_name
};


// for stack_msg_to_desc(); descriptor_data
struct stack_msg {
	char *string;	// text
	int count;	// (x2)
	struct stack_msg *prev, *next;	// doubly-linked list for speed
};


// for descriptor_data
struct txt_block {
	char *text;
	int aliased;
	
	struct txt_block *next;
};


// for descriptor_data
struct txt_q {
	struct txt_block *head;
	struct txt_block *tail;
};


// for descriptor_data, reducing the number of color codes sent to a client
struct color_reducer {
	char last_fg[COLREDUC_SIZE];	// last sent foreground
	char last_bg[COLREDUC_SIZE];	//   " background
	bool is_underline;	// TRUE if an underline was sent
	bool is_clean;	// TRUE if a clean code was sent
	char want_fg[COLREDUC_SIZE];	// last requested (not yet sent) foreground
	char want_bg[COLREDUC_SIZE];	//   " background
	bool want_clean;	// TRUE if an &0 or &n color-terminator was requested
	bool want_underline;	// TRUE if an underline was requested
};


// descriptors -- the connection to the game
struct descriptor_data {
	socket_t descriptor;	// file descriptor for socket

	char *host;	// hostname
	byte bad_pws;	// number of bad pw attemps this login
	byte idle_tics;	// tics idle at game menus prompt
	int connected;	// STATE()
	int submenu;	// SUBMENU() -- use varies by menu
	int desc_num;	// unique num assigned to desc
	time_t login_time;	// when the person connected

	char *showstr_head;	// for keeping track of an internal str
	char **showstr_vector;	// for paging through texts
	int showstr_count;	// number of pages to page through
	int showstr_page;	// which page are we currently showing?
	struct stack_msg *stack_msg_list;	// queued stackable messages
	
	protocol_t *pProtocol; // see protocol.c
	struct color_reducer color;
	bool no_nanny;	// skips interpreting player input if only a telnet negotiation sequence was sent
	int wait;	// triggers a short wait at some menus to prevent out-of-order interactions
	
	char **str;	// for the modify-str system
	char *backstr;	// for the modify-str aborts
	char *str_on_abort;	// a pointer to restore if the player aborts the editor
	char *file_storage;	// name of where to save a file
	bool straight_to_editor;	// if true, commands go straight to the string editor
	size_t max_str;	// max length of editor
	int mail_to;	// name for mail system
	int notes_id;	// idnum of player for notes-editing
	int island_desc_id;	// editing an island desc
	room_vnum save_room_id;	// editing a room desc
	room_vnum save_room_pack_id;	// editing a vehicle desc in the room
	any_vnum save_empire;	// for the text editor to know which empire to save
	struct config_type *save_config;	// saves the config file when done editing text
	bool allow_null;	// string editor can be empty/null
	
	int has_prompt;	// is the user at a prompt?
	char inbuf[MAX_RAW_INPUT_LENGTH];	// buffer for raw input
	char last_input[MAX_INPUT_LENGTH];	// the last input
	char small_outbuf[SMALL_BUFSIZE];	// standard output buffer
	char *output;	// ptr to the current output buffer
	char **history;	// History of commands, for ! mostly.
	int history_pos;	// Circular array position.
	int bufptr;	// ptr to end of current output
	int bufspace;	// space left in the output buffer
	bool data_left_to_write;	// indicates there is more data to write, to prevent an extra crlf
	struct txt_block *large_outbuf;	// ptr to large buffer, if we need it
	struct txt_q input;	// q of unprocessed input

	char_data *character;	// linked to char
	char_data *original;	// original char if switched
	char_data *temp_char;	// for char creation
	descriptor_data *snooping;	// Who is this char snooping
	descriptor_data *snoop_by;	// And who is snooping this char

	char *last_act_message;	// stores the last thing act() sent to this desc
	
	// olc
	int olc_type;	// OLC_OBJECT, etc -- only when an editor is open
	char *olc_storage;	// a character buffer created and used by some olc modes
	any_vnum olc_vnum;	// vnum being edited
	bool olc_show_tree, olc_show_synergies;	// for skill editors
	
	// OLC_x: olc types
	ability_data *olc_ability;	// abil being edited
	adv_data *olc_adventure;	// adv being edited
	archetype_data *olc_archetype;	// arch being edited
	attack_message_data *olc_attack;	// attack message being edited
	int olc_attack_num;		// which message in the attack set is being edited
	augment_data *olc_augment;	// aug being edited
	book_data *olc_book;	// book being edited
	class_data *olc_class;	// class being edited
	obj_data *olc_object;	// item being edited
	char_data *olc_mobile;	// mobile being edited
	morph_data *olc_morph;	// morph being edited
	craft_data *olc_craft;	// craft recipe being edited
	bld_data *olc_building;	// building being edited
	crop_data *olc_crop;	// crop being edited
	event_data *olc_event;	// event being edited
	faction_data *olc_faction;	// faction being edited
	generic_data *olc_generic;	// generic being edited
	struct global_data *olc_global;	// global being edited
	progress_data *olc_progress;	// prg being edited
	quest_data *olc_quest;	// quest being edited
	room_template *olc_room_template;	// rmt being edited
	struct sector_data *olc_sector;	// sector being edited
	shop_data *olc_shop;	// shop being edited
	social_data *olc_social;	// social being edited
	skill_data *olc_skill;	// skill being edited
	trig_data *olc_trigger;	// trigger being edited
	vehicle_data *olc_vehicle;	// vehicle being edited
	
	// linked list
	descriptor_data *next;
};


// used in player specials to track combat
struct combat_meters {
	int hits, misses;	// my hit %
	int hits_taken, dodges, blocks;	// my dodge %
	int damage_dealt, damage_taken, pet_damage;
	int heals_dealt, heals_taken;
	time_t start, end;	// times
	bool over;
};


// basic paragraphs for book_data
struct paragraph_data {
	char *text;
	struct paragraph_data *next;
};


// player mail
struct mail_data {
	int from;	// player idnum
	time_t timestamp;	// when the mail was sent
	char *body;	// mail body
	
	struct mail_data *next;	// linked list
};


// for permanently learning minipets
struct minipet_data {
	any_vnum vnum;	// vnum of the mob
	UT_hash_handle hh;	// player's minipets hash
};


// for player mount collections
struct mount_data {
	mob_vnum vnum;	// mob that's mounted, for name
	bitvector_t flags;	// stored MOUNT_ flags
	
	UT_hash_handle hh;	// hash handle for GET_MOUNT_LIST(ch)
};


// records when a player saw an automessage
struct player_automessage {
	int id;
	long timestamp;
	UT_hash_handle hh;	// GET_AUTOMESSAGES(ch)
};


// for permanently learning crafts (also used by empires)
struct player_craft_data {
	any_vnum vnum;	// vnum of the learned craft
	int count;	// for empires only, number of things giving this craft
	UT_hash_handle hh;	// player's learned_crafts hash
};


// adventure currencies
struct player_currency {
	any_vnum vnum;	// generic vnum
	int amount;
	UT_hash_handle hh;	// GET_PLAYER_CURRENCY()
};


// equipment sets
struct player_eq_set {
	int id;	// unique set id
	char *name;	// keyword to set the set
	struct player_eq_set *next;	// LL
};


// used in player_special_data
struct player_ability_data {
	any_vnum vnum;	// ABIL_ or ability vnum
	bool purchased[NUM_SKILL_SETS];	// whether or not the player bought it
	byte levels_gained;	// tracks for the cap on how many times one ability grants skillups
	ability_data *ptr;	// live abil pointer

	UT_hash_handle hh;	// player's ability hash
};


// languages a player knows -- also used for empires
struct player_language {
	any_vnum vnum;	// vnum of the language (generic)
	byte level;	// LANG_ constant for how well they speak it
	UT_hash_handle hh;	// player's language hash
};


// remembers a tile a player has seen before
struct player_map_memory {
	room_vnum vnum;	// map loc
	time_t timestamp;	// when last set
	char *icon;	// icon, if any
	char *name;	// name, if any
	UT_hash_handle hh;	// GET_MAP_MEMORY(ch)
};


// used for a hash of skills per player in player_special_data
struct player_skill_data {
	any_vnum vnum;	// SKILL_ or skill vnum
	int level;	// current skill level (0-100)
	double exp;	// experience gained in skill (0-100%)
	int resets;	// resets available
	bool noskill;	// if TRUE, do not gain
	skill_data *ptr;	// live skill pointer
	
	UT_hash_handle hh;	// player's skill hash
};


// channels a player is on
struct player_slash_channel {
	int id;
	struct player_slash_channel *next;
};


// player techs (from abilities)
struct player_tech {
	int id;	// which PTECH_
	any_vnum abil;	// which ability it came from
	bool check_solo;	// if TRUE, need to check solo to allow it
	struct player_tech *next;	// LL: GET_TECHS(ch)
};


/*
 * Specials needed only by PCs, not NPCs.  Space for this structure is
 * not allocated in memory for NPCs, but it is for PCs.
 */
struct player_special_data {
	// SAVED PORTION //
	// add new items to write_player_to_file() and read_player_primary_data()
	
	// account and player stuff
	account_data *account;	// pointer to account_table entry
	int temporary_account_id;	// used during creation
	char *creation_host;	// host they created from
	char *referred_by;	// as entered in character creation
	byte invis_level;	// level of invisibility
	byte immortal_level;	// stored so that if level numbers are changed, imms stay at the correct level
	bitvector_t grants;	// grant imm abilities
	bitvector_t syslogs;	// which syslogs people want to see
	bitvector_t bonus_traits;	// BONUS_
	bitvector_t fight_messages;	// FM_ flags
	bitvector_t status_messages;	// SM_ flags
	bitvector_t informative_flags;	// INFORMATIVE_ flags
	ubyte bad_pws;	// number of bad password attemps
	struct mail_data *mail_pending;	// uncollected letters
	int create_alt_id;	// used in CON_Q_ALT_NAME and CON_Q_ALT_PASSWORD
	int promo_id;	// entry in the promo_codes table
	int ignore_list[MAX_IGNORES];	// players who can't message you
	int last_tell;	// idnum of last tell from
	time_t last_offense_seen;	// timestamp of last time the player checked offenses
	any_vnum last_companion;	// if the player has a companion out, this triggers a re-summon
	
	// character strings
	char *personal_lastname;	// Lastname written by the player themselves
	char *current_lastname;		// Lastname the player is currently using
	char *title;	// shown on 'who'/'whois'
	char *prompt;	// custom prompt
	char *fight_prompt;	// fight prompt
	char *poofin;	// shown when immortal appears
	char *poofout;	// shown when immortal disappears
	
	// preferences
	bitvector_t pref;	// preference flags for PCs.
	int last_tip;	// for display_tip_to_character
	byte mapsize;	// how big the player likes the map
	char custom_colors[NUM_CUSTOM_COLORS];	// for custom channel coloring, storing the letter part of the & code ('r' for &r)
	
	// quests and progression
	struct player_quest *quests;	// quests the player is on (player_quest->next)
	struct player_completed_quest *completed_quests;	// hash table (hh)
	time_t last_goal_check;	// last time the player looked for new empire goals
	
	// empire
	empire_vnum pledge;	// Empire he's applying to
	byte rank;	// Rank in the empire
	sh_int highest_known_greatness;	// maximum greatness achieved (prevents empire greatness dropping)
	
	// misc player attributes
	ubyte apparent_age;	// for vampires	
	int conditions[NUM_CONDS];	// Drunk, full, thirsty
	int resources[NUM_MATERIALS];	// God resources
	int temperature;	// how warm/cold the player currently is
	
	// various lists
	struct coin_data *coins;	// linked list of coin data
	struct companion_data *companions;	// player's companions (familiars, bodyguards, hirelings)
	struct player_currency *currencies;	// hash table of adventure currencies
	struct alias_data *aliases;	// Character's aliases
	struct player_eq_set *eq_sets;	// player's saved equipment sets
	struct player_lastname *lastname_list;	// linked list of lastnames
	struct offer_data *offers;	// various offers for do_accept/reject
	struct player_slash_channel *slash_channels;	// channels the player is on
	struct slash_channel *load_slash_channels;	// temporary storage between load and join
	struct player_faction_data *factions;	// hash table of factions
	struct channel_history_data *channel_history[NUM_CHANNEL_HISTORY_TYPES];	// DLs: histories
	struct player_automessage *automessages;	// hash of seen messages
	struct player_event_data *event_data;	// hash of event scores and results
	struct affected_type *passive_buffs;	// from PASSIVE-BUFF abilities
	struct player_map_memory *map_memory;	// hash of memories of map locations

	// some daily stuff
	int daily_cycle;	// Last update cycle registered
	ubyte daily_bonus_experience;	// boosted skill gain points
	int daily_quests;	// number of daily quests completed today
	int event_daily_quests;	// number of daily event quests completed today

	// action info
	int action;	// ACT_
	double action_cycle;	// time left before an action tick
	int action_timer;	// ticks to completion (use varies)
	room_vnum action_room;	// player location
	int action_vnum[NUM_ACTION_VNUMS];	// slots for storing action data (use varies)
	struct resource_data *action_resources;	// temporary list for resources stored during actions
	char *action_string;	// for run, abilities, etc
	char_data *action_targ_char;	// action targets this char (saved as id)
	int temporary_char_targ;	// helps when loading action_targ_char
	bitvector_t action_targ_multi;	// action targets group/etc
	obj_data *action_targ_obj;	// unsaved: action targets this object
	vehicle_data *action_targ_veh;	// action targets this vehicle
	int temporary_veh_targ;	// helps when loading action_targ_veh
	room_vnum action_targ_room;	// action targets this room
	
	// locations and movement
	room_vnum load_room;	// Which room to place char in
	room_vnum load_room_check;	// this is the home room of the player's loadroom, used to check that they're still in the right place
	room_vnum last_room;	// useful when dead
	room_vnum home_location;	// player's private home, if any -- not called "home_room" to avoid confusion with the room_data version of a "home room"
	room_vnum tomb_room;	// location of player's chosen tomb
	int recent_death_count;	// for death penalty
	long last_death_time;	// for death counts
	int last_corpse_id;	// DG Scripts obj id of last corpse
	int adventure_summon_instance_id;	// instance summoned to
	room_vnum adventure_summon_return_location;	// where to send a player back to if they're outside an adventure
	room_vnum adventure_summon_return_map;	// map check location for the return loc
	room_vnum marked_location;	// for map marking
	any_vnum last_vehicle;	// if the player quit in a vehicle
	
	// olc data
	any_vnum olc_min_vnum;	// low range of olc permissions
	any_vnum olc_max_vnum;	// high range of olc permissions
	bitvector_t olc_flags;	// olc permissions
	
	// skill/ability data
	any_vnum creation_archetype[NUM_ARCHETYPE_TYPES];	// array of creation choices
	struct player_skill_data *skill_hash;
	struct player_ability_data *ability_hash;
	int current_skill_set;	// which skill set a player is currently in
	bool can_gain_new_skills;	// not required to keep skills at zero
	sh_int skill_level;  // levels computed based on skills above level 75
	sh_int highest_known_level;	// maximum level ever achieved (used for gear restrictions)
	sh_int last_known_level;	// set on save/quit/alt
	ubyte class_progression;	// % of the way from SPECIALTY_SKILL_CAP to MAX_SKILL_CAP
	ubyte class_role;	// ROLE_ chosen by the player
	class_data *character_class;  // character's class as determined by top skills
	any_vnum speaking;	// current language
	struct player_language *languages;	// languages the player speaks/recognizes
	struct player_craft_data *learned_crafts;	// crafts learned from patterns
	struct minipet_data *minipets;	// collection of summonable pets
	struct player_tech *techs;	// techs from abilities
	struct empire_unique_storage *home_storage;	// DLL: items stored in the home
	time_t last_home_set_time;	// how long ago the player used home-set (blocks home retrieve)
	
	// tracking for specific skills
	byte confused_dir;  // people without Navigation think this dir is north
	char *disguised_name;	// verbatim copy of name -- grabs custom mob names and empire names
	byte disguised_sex;	// sex of the mob you're disguised as
	any_vnum using_poison;	// poison preference for Stealth
	any_vnum using_ammo;	// preferred ranged ammo

	// mount info
	struct mount_data *mount_list;	// list of stored mounts
	mob_vnum mount_vnum;	// current mount vnum
	bitvector_t mount_flags;	// current mount flags
	
	// other
	int largest_inventory;	// highest inventory size a player has ever had
	
	// UNSAVED PORTION //
	
	int idle_seconds;	// how long they have been idle (updated every 5 seconds or so)
	int gear_level;	// computed gear level -- determine_gear_level()
	byte reboot_conf;	// Reboot confirmation
	byte create_points;	// Used in character creation
	int group_invite_by;	// idnum of the last player to invite this one
	time_t move_time[TRACK_MOVE_TIMES];	// timestamp of last X moves
	int beckoned_by;	// idnum of player who beckoned (for follow)
	int last_aff_wear_off_id;	// helps prevent duplicate wear-off messages
	int last_aff_wear_off_vnum;	// helps prevent duplicate wear-off messages
	time_t last_aff_wear_off_time;	// helps prevent duplicate wear-off messages
	time_t last_cond_message_time[NUM_CONDS];	// last time we sent a message for drunk, full, thirsty
	time_t last_cold_time;	// last time a player was told they're getting colder
	time_t last_warm_time;	// last time a player was told they're getting warmer
	int last_messaged_temperature;	// last temperature a player was warned about
	int last_look_sun;	// used to determine if the player needs to 'look' at sunrise/set
	bool map_memory_needs_save;	// whether or not to save the map memory file
	bool map_memory_loaded;	// whether or not it has been loaded yet
	int map_memory_count;	// how many tiles are currently remembered
	
	struct ability_gain_hook *gain_hooks;	// hash table of when to gain ability xp
	struct ability_trait_hook *trait_hooks;	// hash table to watch for trait changes
	struct combat_meters meters;	// combat meter data
	
	bool affects_converted;	// if FALSE, player's affs have seconds-of-duration instead of expire-timestamp
	bool needs_delayed_load;	// whether or not the player still needs delayed data
	bool dont_save_delay;	// marked when a player is partially unloaded, to prevent accidentally saving a delay file with no gear
	bool restore_on_login;	// mark the player to trigger a free reset when they enter the game
	bool reread_empire_tech_on_login;	// mark the player to trigger empire tech re-read on entering the game
};


// unlockable account perks
struct unlocked_archetype {
	any_vnum vnum;	// which archetype
	UT_hash_handle hh;	// hashed in account_data->unlocked_archetypes by vnum
};


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER STRUCTS ///////////////////////////////////////////////////////

// These data contain information about a players time data
struct time_data {
	time_t birth;	// This represents the characters age
	time_t logon;	// Time of the last logon (used to calculate played)
	int played;	// This is the total accumulated time played in secs
};


// general player-related info, usually PCs and NPCs
struct char_player_data {
	char *passwd;	// character's password
	char *name;	// PC name / NPC keyword (kill ... )
	char *short_descr;	// for NPC string-building
	char *long_descr;	// for 'look' on mobs
	char *look_descr;	// look-at text
	byte sex;	// PC / NPC sex
	byte access_level;	// PC access level -- LVL_x -- not to be confused with GET_COMPUTED_LEVEL() or get_approximate_level()
	struct time_data time;	// PC's age

	struct lore_data *lore;	// player achievements
};


// Char's points.
// everything in here has a default value that it's always reset to, and only
// gear and affects can raise them above it; character can't gain them
// permanently
struct char_point_data {
	int current_pools[NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	int max_pools[NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	int deficit[NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	
	int extra_attributes[NUM_EXTRA_ATTRIBUTES];	// ATT_ (dodge, etc)
};


// for fighting
struct fight_data {
	char_data *victim;	// Actual victim
	byte mode;	// Fight mode, FMODE_x
	byte wait;	// Time to intercept
	unsigned long long last_swing_mainhand;	// last attack time (microseconds)
	unsigned long long last_swing_offhand;	// last attack time (microseconds)
};


// Special playing constants shared by PCs and NPCs
struct char_special_data {
	// SAVED SECTION //
	// add new items to write_player_to_file() and read_player_primary_data()
	
	int idnum;	// player's idnum; -1 for mobiles
	sbyte size;	// character's SIZE_ const
	bitvector_t act;	// mob flag for NPCs; player flag for PCs
	bitvector_t injuries;	// Bitvectors including damage to the player
	bitvector_t affected_by;	// Bitvector for spells/skills affected by
	morph_data *morph;	// for morphed people
	obj_vnum rope_vnum;	// for tied-up people
	byte last_direction;	// used for walls and movement

	// UNSAVED SECTION //
	
	struct fight_data fighting;	// Opponent
	char_data *hunting;	// Char hunted by this char

	char_data *feeding_from;	// Who person is biting
	char_data *fed_on_by;	// Who is biting person
	
	char_data *companion;	// for PC this is the NPC companion; for NPC it's the player they are the companion OF
	
	char_data *led_by;	// A person may lead a mob
	char_data *leading_mob;	// A mob may lead a person
	vehicle_data *leading_vehicle;	// A person may lead a vehicle
	vehicle_data *sitting_on;	// Vehicle the person is on
	vehicle_data *driving;	// Vehicle the person is driving
	
	struct empire_npc_data *empire_npc;	// if this is an empire spawn
	
	byte position;	// Standing, fighting, sleeping, etc.
	
	int health_regen;	// Hit regeneration add
	int move_regen;	// Move regeneration add
	int mana_regen;	// mana regen add
	
	int carry_items;	// Number of items carried
	
	struct ability_exec *running_ability_data;	// while running an ability, its data is here for reference anywhere
	struct vnum_hash **running_ability_limiter;	// while running a chain of abilities, stores the limiter here
	struct stored_event *stored_events;	// linked list of stored dg events
};


// main character data for PC/NPC in-game
struct char_data {
	mob_vnum vnum;	// mob's vnum
	room_data *in_room;	// location (real room number)
	int wait;	// wait for how many loops

	empire_data *loyalty;	// live loyalty pointer
	struct char_player_data player;	// normal data
	sh_int real_attributes[NUM_ATTRIBUTES];	// str, etc (natural)
	sh_int aff_attributes[NUM_ATTRIBUTES];	// str, etc (modified)
	struct char_point_data points;	// points
	struct char_special_data char_specials;	// PC/NPC specials
	struct player_special_data *player_specials;	// PC specials
	struct mob_special_data mob_specials;	// NPC specials
	struct interaction_item *interactions;	// mob interaction items
	struct cooldown_data *cooldowns;	// ability cooldowns
	
	struct affected_type *affected;	// affected by what spells
	struct over_time_effect_type *over_time_effects;	// damage-over-time effects
	obj_data *equipment[NUM_WEARS];	// Equipment array

	obj_data *carrying;	// head of list
	descriptor_data *desc;	// NULL for mobiles

	int script_id;	// used by DG triggers - unique id
	bool in_lookup_table;	// if ch is in the script lookup table or not
	struct trig_proto_list *proto_script;	// list of default triggers
	struct script_data *script;	// script info for the object
	struct script_memory *memory;	// for mob memory triggers

	char_data *prev_in_room, *next_in_room;	// For room->people - doubly-linked list
	char_data *prev, *next;	// For character_list (doubly-linked)
	char_data *prev_plr, *next_plr;	// For player_character_list (doubly-linked)
	char_data *next_fighting;	// For fighting list
	bool in_combat_list;	// helps with removing from combat list
	
	struct follow_type *followers;	// List of chars followers
	char_data *leader;	// Who is char following?
	struct group_data *group;	// Character's Group

	char *prev_host;	// Previous host (they're Trills)
	time_t prev_logon;	// Time (in secs) of prev logon
	
	// live data (not saved, not freed)
	struct quest_lookup *quest_lookups;
	struct shop_lookup *shop_lookups;
	bool customized;	// mob strings need saving if TRUE
	sh_int lights;	// number of lights on the character
	
	UT_hash_handle hh;	// mobile_table
};


// for cooldown expiration
struct cooldown_expire_event_data {
	char_data *character;
	struct cooldown_data *cooldown;
};


// cooldown info (cooldowns are defined by generics)
struct cooldown_data {
	any_vnum type;	// any COOLDOWN_ const or vnum
	time_t expire_time;	// time at which the cooldown has expired
	struct dg_event *expire_event;	// scheduled DG event, if any
	
	struct cooldown_data *next;	// linked list
};


// for damage-over-time (dot) updates and expiry
struct dot_event_data {
	char_data *ch;
	struct over_time_effect_type *dot;
};


// for damage-over-time (DoTs)
struct over_time_effect_type {
	any_vnum type;	// ATYPE_
	int cast_by;	// player ID (positive) or mob vnum (negative)
	int time_remaining;	// time in SECONDS
	sh_int damage_type;	// DAM_x type
	sh_int damage;	// amount
	sh_int stack;	// damage is multiplied by this
	sh_int max_stack;	// how high it's allowed to stack
	
	struct dg_event *update_event;	// for updating every 5 seconds

	struct over_time_effect_type *next;
};


// Structure used for chars following other chars
struct follow_type {
	char_data *follower;
	struct follow_type *next;
};


// For a person's lore
struct lore_data {
	int type;	// LORE_x
	long date;	// when it was acquired (timestamp)
	char *text;

	struct lore_data *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// CRAFT STRUCTS ///////////////////////////////////////////////////////////

// main struct for the crafting code
struct craft_data {
	craft_vnum vnum;	// vnum of this recipe
	
	int type;	// CRAFT_TYPE_
	any_vnum ability;	// NO_ABIL for none, otherwise ABIL_ type
	
	char *name;
	any_vnum object;	// vnum of the object it makes, or liquid id if CRAFT_SOUP, or vehicle if CRAFT_VEHICLE
	int quantity;	// makes X
	
	int min_level;	// required level to craft it using get_crafting_level()
	bitvector_t flags;	// CRAFT_
	int time;	// how many action ticks it takes
	
	// for buildings:
	any_vnum build_type;	// a building vnum (maybe something else too?)
	bitvector_t build_on;	// BLD_ON_ flags for the tile it's built upon
	bitvector_t build_facing;	// BLD_ON_ flags for the tile it's facing
	
	bitvector_t requires_tool;	// any TOOL_ flags required to make this
	obj_vnum requires_obj;	// only shows up if you have the item
	bitvector_t requires_function;	// FNC_
	struct resource_data *resources;	// linked list
	
	UT_hash_handle hh;	// craft_table hash
	UT_hash_handle sorted_hh;	// sorted_crafts hash
};


// act.trade.c: For 
struct gen_craft_data_t {
	char *command;	// "forge"
	char *verb;	// "forging"
	bitvector_t actf_flags;	// additional ACTF_ flags
	char *strings[3];	// periodic message { to char, to room }
};


 //////////////////////////////////////////////////////////////////////////////
//// CROP STRUCTS ////////////////////////////////////////////////////////////

// data for online-editable crop data
struct crop_data {
	crop_vnum vnum;	// virtual number
	
	char *name;	// for stat/lookups/mapreader
	char *title;	// room title
	struct icon_data *icons;	// linked list of available icons
	int mapout;	// position in mapout_color_names, mapout_color_tokens
	
	bitvector_t climate;	// CLIM_ flags
	bitvector_t flags;	// CROPF_ flags
	
	// only spawns where:
	int x_min;	// x >= this
	int x_max;	// x <= this
	int y_min;	// y >= this
	int y_max;	// y <= this
	
	struct custom_message *custom_msgs;	// any custom messages
	struct spawn_info *spawns;	// mob spawn data
	struct interaction_item *interactions;	// interaction items
	struct extra_descr_data *ex_description;	// extra descriptions
	
	UT_hash_handle hh;	// crop_table hash
};


 //////////////////////////////////////////////////////////////////////////////
//// DATA STRUCTS ////////////////////////////////////////////////////////////

// for action messages
struct action_data_struct {
	char *name;	// shown e.g. in sentences or prompt ("digging")
	char *long_desc;	// shown in room (action description)
	bitvector_t flags;	// ACTF_ flags
	void (*process_function)(char_data *ch);	// called on ticks (may be NULL)
	void (*cancel_function)(char_data *ch);	// called when the action is cancelled early (may be NULL)
};


// chore type data -- CHORE_
struct empire_chore_type {
	char *name;
	mob_vnum mob;
	bool hidden;	// won't show in the main chores list
	int requires_tech;	// if any -- or NOTHING
};


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE STRUCTS //////////////////////////////////////////////////////////


// matches up to city_type
struct empire_city_data {
	char *name;
	int type;
	
	room_data *location;
	bitvector_t traits;	// ETRAIT_x
	
	struct empire_city_data *next;
};


// hash of goals completed by the empire
struct empire_completed_goal {
	any_vnum vnum;	// which progress goal
	time_t when;
	
	UT_hash_handle hh;	// stored in empire's hash table
};


// hash of items dropped around the empire (blocks workforce cheating)
struct empire_dropped_item {
	obj_vnum vnum;
	int count;
	UT_hash_handle hh;	// EMPIRE_DROPPED_ITEMS()
};


// tracks how much an empire is actually playing
struct empire_playtime_tracker {
	int cycle;	// hash key: using DAILY_CYCLE_DAY
	int playtime_secs;	// total playtime in seconds
	UT_hash_handle hh;
};


// permanent counts of totals of items accumulated by the empire, for use in progress goals/quests
struct empire_production_total {
	obj_vnum vnum;	// which item
	obj_data *proto;	// pointer to the obj proto
	
	int amount;	// how much
	int imported;	// how many have been imported (used to prevent abuse)
	int exported;	// how many have been exported
	
	UT_hash_handle hh;	// empire->gathered_totals hash (by vnum)
};


// current progress goals
struct empire_goal {
	any_vnum vnum;	// which progress goal
	ush_int version;	// for auto-updating
	struct req_data *tracker;	// tasks to track
	time_t timestamp;	// when the goal was started
	
	UT_hash_handle hh;	// hashed by vnum
};


// used to track homeless npcs
struct empire_homeless_citizen {
	mob_vnum vnum;	// npc type
	int sex;	// SEX_x
	int name;	// position in the name list
	struct map_data *loc;	// where it became homeless
	time_t when;	// time when it became homeless
	
	// empire_vnum empire_id;	// empire vnum -- probably not needed
	// char_data *mob;	// can't currently spawn from this
	struct empire_homeless_citizen *next;	// linked list
};


// per-island data for the empire
struct empire_island {
	int island;	// which island id
	
	// saved portion
	int workforce_limit[NUM_CHORES];	// workforce settings
	char *name;	// empire's local name for the island
	struct empire_storage_data *store;	// hash table of storage here
	bool store_is_sorted;	// EINV_SORT_ id of how it's currently sorted, or EINV_UNSORTED if not
	struct empire_needs *needs;	// hash of stuff needed
	
	// unsaved portion
	int tech[NUM_TECHS];	// TECH_ present on that island
	int population;	// citizens
	unsigned int territory[NUM_TERRITORY_TYPES];	// territory counts on this island
	
	UT_hash_handle hh;	// EMPIRE_ISLANDS(emp) hash handle
};


struct empire_log_data {
	int type;	// ELOG_
	time_t timestamp;
	char *string;
	
	struct empire_log_data *prev, *next;
};


// non-saving shortcut of member data
struct empire_member_account {
	int id;	// player account id
	struct empire_member_data *list;
	UT_hash_handle hh;	// EMPIRE_MEMBER_ACCOUNTS(), hashed by id
};


// non-saving shortcut of member data
struct empire_member_data {
	int id;	// player character idnum
	int greatness;	// greatness of character
	time_t timeout_at;	// timestamp when they did/will time out
	UT_hash_handle hh;	// empire_member_account->list, hashed by id
};


// data related to what an empire needs (on an island)
struct empire_needs {
	int type;	// ENEED_ const
	int needed;	// how much currently needed
	bitvector_t status;	// ENEED_STATUS_ const
	UT_hash_handle hh;	// hashed by type
};


// data for keeping npcs' names and data for empires
struct empire_npc_data {
	mob_vnum vnum;	// npc type
	int sex;	// SEX_x
	int name;	// position in the name list
	room_data *home;	// where it lives (if a room)
	vehicle_data *home_vehicle;	// where it lives (if a vehicle)
	
	empire_vnum empire_id;	// empire vnum
	char_data *mob;
	struct empire_npc_data *next;	// linked list
};


// The political structure for the empires
struct empire_political_data {
	empire_vnum id;	// vnum of the other empire
	int type;	// The type of relationship between them
	int offer;	// Suggested states of relation
	
	time_t start_time;	// used mainly for war

	struct empire_political_data *next;
};


// The storage structure for empire islands
struct empire_storage_data {
	obj_vnum vnum;	// what's stored
	obj_data *proto;	// pointer to the obj proto
	int amount;	// how much
	int keep;	// how much workforce should ignore/keep (UNLIMITED/-1 or >0)
	struct storage_timer *timers;	// doubly-linked list for expiration
	UT_hash_handle hh;	// empire_island->store hash (by vnum)
};


// list of rooms and buildings owned
struct empire_territory_data {
	room_vnum vnum;	// vnum of the room, for hashing
	room_data *room;	// pointer to territory location
	
	int population_timer;	// time to re-populate
	struct empire_npc_data *npcs;	// list of empire mobs that live here
	
	bool marked;	// for checking that rooms still exist
	
	UT_hash_handle hh;	// emp->territory_list hash (by room vnum)
};


// similar to territory: a hash of owned vehicles and their npcs
struct empire_vehicle_data {
	int idnum;	// vehicle's unique id, for hashing
	vehicle_data *veh;	// pointer to the vehicle
	
	int population_timer;	// time to re-populate
	struct empire_npc_data *npcs;	// list of empire mobs that live on the outside of the vehicle
	
	bool marked;	// for checking that entries still exist
	
	UT_hash_handle hh;	// emp->vehicle_list hash (by idnum)
};


// for empire trades
struct empire_trade_data {
	int type;	// TRADE_x
	obj_vnum vnum;	// item type
	int limit;	// min (export), max (import)
	double cost;	// min (export), max (import)
	
	struct empire_trade_data *next;
};


// for storing scaled, enchanted, or superior items
struct empire_unique_storage {
	obj_data *obj;	// actual live object
	int amount;	// stacking
	struct storage_timer *timers;	// for expiring items
	sh_int flags;	// up to 15 flags, EUS_
	int island;	// split by islands
	
	struct empire_unique_storage *prev, *next;
};


// for the ewt system: island entry
struct empire_workforce_tracker_island {
	int id;
	int amount;
	int workers;
	UT_hash_handle hh;
};


// for the ewt system: resource tracker
struct empire_workforce_tracker {
	obj_vnum vnum;	// type of resource
	int total_workers;	// people working this resource
	int total_amount;	// amount of resource, total
	struct empire_workforce_tracker_island *islands;	// amount per island
	UT_hash_handle hh;
};


// linked list of chores in the 'delay' state
struct workforce_delay_chore {
	int chore;
	int time;
	int problem;
	struct workforce_delay_chore *next;
};


// allows workforce chores to be skipped
struct workforce_delay {
	room_vnum location;
	struct workforce_delay_chore *chores;
	UT_hash_handle hh;
};


// lets players prevent production of a certain item
struct workforce_production_limit {
	obj_vnum vnum;	// obj vnum
	int limit;	// 0 = make none (no entry = make all)
	UT_hash_handle hh;	// EMPIRE_PRODUCTION_LIMITS() hash
};


// to support daily workforce elogs
struct workforce_production_log {
	int type;	// WPLOG_ type
	any_vnum vnum;	// object vnum etc
	int amount;	// quantity
	
	struct workforce_production_log *next;	// LL
};


// for offenses committed against an empire
struct offense_data {
	int type;	// OFFENSE_ constant
	any_vnum empire;	// which empire caused it (or NOTHING)
	int player_id;	// which player caused it
	time_t timestamp;	// when
	int x, y;	// approximate location
	bitvector_t flags;	// OFF_ for anonymous offenses, whether or not there was an observer
	
	struct offense_data *prev, *next;	// doubly-linked list
};


// for item decay timers in storage
struct storage_timer {
	int timer;
	int amount;
	struct storage_timer *next, *prev;	// stored as a doubly linked list in ascending order by timer
};


// records recent thefts in the empire
struct theft_log {
	obj_vnum vnum;
	int amount;
	long time_minutes;	// timestamp to the nearest minute
						// NOTE: Theft logs are always stored in descending time
	
	struct theft_log *next;	// linked list
};


// temporarily logs any errors that come up during workforce
struct workforce_log {
	any_vnum loc;	// don't store room itself -- may not be in memory later
	int chore;	// CHORE_ const
	int problem;	// WF_PROB_ const
	int count;	// how many times this apepars in the same spot
	bool delayed;	// whether this is a delay-repeat or not
	struct workforce_log *next;
};


// structure that records individual mobs who worked in the last cycle
struct workforce_where_log {
	char_data *mob;	// may be NULL if purged
	int chore;	// CHORE_ const
	room_vnum loc;	// where it happened
	struct workforce_where_log *prev, *next;
};


// The main data structure for the empires
struct empire_data {
	empire_vnum vnum;	// empire's virtual number
	
	int leader;	// ID number of the leader, also used in pfiles
	char *name;	// Name of the empire, defaults to leader's name
	char *adjective;	// the adjective form of the name
	char *banner;	// Color codes (maximum of 3) of the banner
	char *description;	// Empire desc
	char *motd;	// Empire MOTD
	
	long create_time;	// when it was founded
	long city_overage_warning_time;	// if the empire has been warned

	byte num_ranks;	// Total number of levels (maximum 20)
	char *rank[MAX_RANKS];	// Name of each rank
	
	int attributes[NUM_EMPIRE_ATTRIBUTES];	// misc attributes
	int progress_points[NUM_PROGRESS_TYPES];	// empire's points in each category
	bitvector_t admin_flags;	// EADM_
	bitvector_t frontier_traits;	// ETRAIT_
	double coins;	// total coins (always in local currency)

	byte priv[NUM_PRIVILEGES];	// The rank at which you can use a command
	int base_tech[NUM_TECHS];	// TECH_ from rewards (not added by buildings or players)

	// linked lists, hashes, etc
	struct empire_political_data *diplomacy;
	struct shipping_data *shipping_list;	// DL of shipping orders
	struct empire_unique_storage *unique_store;	// DLL: eus->next
	struct empire_trade_data *trade;
	struct empire_log_data *logs;
	struct offense_data *offenses;	// doubly-linked list
	struct empire_goal *goals;	// current goal trackers (hash by vnum)
	struct empire_completed_goal *completed_goals;	// actually a hash (vnum)
	struct player_language *languages;	// languages available to the whole empire
	struct player_craft_data *learned_crafts;	// crafts available to the whole empire
	struct theft_log *theft_logs;	// recently stolen items
	struct empire_production_total *production_totals;	// totals of items produced by the empire (hash by vnum)
	struct empire_homeless_citizen *homeless;	// list of homeless npcs
	struct script_data *script;	// for storing variables
	struct workforce_production_limit *production_limits;	// limits on what workforce can make
	struct workforce_production_log *production_logs;	// LL of things produced
	struct empire_playtime_tracker *playtime_tracker;	// tracks real gameplay
	struct channel_history_data *chat_history;
	
	// other saved data
	time_t wf_log_and_needs_time;	// last time we updated wf logs and needs (saved to storage file)
	
	// unsaved data
	struct empire_territory_data *territory_list;	// hash table by vnum
	struct empire_vehicle_data *vehicle_list;	// hash table by idnum
	struct empire_city_data *city_list;	// linked list of cities
	struct empire_workforce_tracker *ewt_tracker;	// workforce tracker
	struct workforce_delay *delays;	// speeds up chore processing
	
	/* Unsaved data */
	unsigned int territory[NUM_TERRITORY_TYPES];	// territory counts on this island
	int wealth;	// computed by read_vault
	int population;	// npc population who lives here
	int military;	// number of soldiers
	int greatness;	// total greatness of members
	int tech[NUM_TECHS];	// TECH_, detected from buildings and abilities
	struct empire_island *islands;	// empire island data hash
	int members;	// Number of members, calculated at boot time
	int total_member_count;	// Total number of members including timeouts and dupes
	int total_playtime;	// total playtime among all accounts, in hours
	bool imm_only;	// Don't allow imms/morts to be together
	int fame;	// Empire's fame rating
	time_t last_logon;	// time of last member's last logon
	int scores[NUM_SCORES];	// empire score in each category
	int sort_value;	// for score ties
	bool banner_has_underline;	// helper
	struct workforce_log *wf_log;	// errors with workforce
	struct workforce_where_log *wf_where_log;	// list of people working
	time_t next_timeout;	// for triggering rescans
	int min_level;	// minimum level in the empire
	int max_level;	// maximum level in the empire
	bitvector_t delayed_refresh;	// things that are requesting an update
	struct empire_member_account *member_accounts;	// tracks greatness/etc
	struct empire_dropped_item *dropped_items;	// hash (by vnum) of items dropped in the empire
	char mapout_token;	// displayed for this empire on the graphical political map
	
	bool history_loaded;	// record whether or not chat history has been loaded
	bool storage_loaded;	// record whether or not storage has been loaded, to prevent saving over it
	bool logs_loaded;	// record whether or not logs have been loaded, to prevent saving over them
	
	bool needs_save;	// for things that delay-save
	bool needs_logs_save;	// for logs/offenses that delay-save
	bool needs_storage_save;	// for storage delay-save
	
	UT_hash_handle hh;	// empire_table hash handle
};


 //////////////////////////////////////////////////////////////////////////////
//// EVENT STRUCTS ///////////////////////////////////////////////////////////

// global events: main data
struct event_data {
	any_vnum vnum;
	ush_int version;	// for auto-updating
	
	char *name;	// short name for strings
	char *description;	// long desc shown to players
	char *complete_msg;	// sent to participants when over
	char *notes;	// admin notes
	
	int type;	// EVT_ type
	bitvector_t flags;	// EVTF_ flags
	struct event_reward *rank_rewards;	// rewards given for final position
	struct event_reward *threshold_rewards;	// rewards given for points progress
	
	// constraints
	int min_level;	// or 0 for no min
	int max_level;	// or 0 for no max
	int duration;	// minutes in length
	int repeats_after;	// minutes to auto-repeat; 0/NOT_REPEATABLE for none
	int max_points;	// fixed point cap, if >0
	
	UT_hash_handle hh;	// hash handle for event_table
};


// for 'event' start/end events
struct event_event_data {
	struct event_running_data *running;
};


// global events: rewards
struct event_reward {
	int min;	// minimum rank that gets this, OR minimum event points for threshold
	int max;	// maximum rank that gets this (optional: if 0, all players over 'min' get it)
	int type;	// QR_ type
	
	any_vnum vnum;	// thing to give
	int amount;	// how much/many to give
	
	struct event_reward *next;	// linked list
};


// for the 'running_events' linked list, saved to the events file
struct event_running_data {
	int id;	// permanent unique id (based on top_event_id)
	event_data *event;	// pointer to the event proto
	
	time_t start_time;	// when it began
	int status;	// EVTS_ state
	
	// leaderboards (these are summaries and, in general, the game relies on the player file for scores)
	struct event_leaderboard *player_leaderboard;
	// struct event_leaderboard *empire_leaderboard;
	
	struct dg_event *next_dg_event;	// handles timing for ending the event
	
	struct event_running_data *next;	// linked list: running_events
};


// summary of player/empire points (copied from any points the players gain)
struct event_leaderboard {
	int id;	// player or empire id
	int points;	// last-recorded points
	bool approved;	// in case we can't count unapproved chars
	bool ignore;	// for imms or people who are disqualified, won't count toward rank
	
	UT_hash_handle hh;	// hash handle for running_event->player_leaderboard or running_event->empire_leaderboard
};


// for tracking players' points and status in the current event as well as past ones
struct player_event_data {
	int id;	// event id
	event_data *event;	// which event it was
	
	time_t timestamp;	// when the event happened
	int points;	// total accumulated points
	int collected_points;	// the highest threshold reward collected by the player
	int rank;	// last recorded rank
	int status;	// what state the event is in
	int level;	// best-recorded player level during the event
	
	UT_hash_handle hh;	// hash handle for GET_EVENT_DATA(ch)
};


 //////////////////////////////////////////////////////////////////////////////
//// EVENT STRUCTS (TIMED EVENT SYSTEM) //////////////////////////////////////

// for character-driven events that can be PC or NPC
struct char_event_data {
	char_data *character;	// the person acting
};


// for map events
struct map_event_data {
	struct map_data *map;
};


// data for various timed mob events
struct mob_event_data {
	char_data *mob;		// which mob
};


// data for various timed object events
struct obj_event_data {
	obj_data *obj;		// which object
};


// data for the event when a building is burning
struct room_event_data {
	room_data *room;
};


// for room affect expiration
struct room_expire_event_data {
	room_data *room;
	struct affected_type *affect;
};


// for lists of stored events on things
struct stored_event {
	struct dg_event *ev;
	int type;	// SEV_ type
	
	struct stored_event *next;	// linked list
	// UT_hash_handle hh;	// FORMERLY hashed by type; these are short lists though
};


// data for SEV_ consts
struct stored_event_info_t {
	EVENT_CANCEL_FUNC(*cancel);	// which function cancels it
};


 //////////////////////////////////////////////////////////////////////////////
//// FACTION STRUCTS /////////////////////////////////////////////////////////

struct faction_data {
	any_vnum vnum;
	
	char *name;	// basic name
	char *description;	// full text desc
	
	bitvector_t flags;	// FCT_ flags
	int max_rep;	// REP_ they cap at, positive
	int min_rep;	// REP_ they cap at, negative
	int starting_rep;	// REP_ initial reputation for players
	struct faction_relation *relations;	// hash table
	
	// lists
	UT_hash_handle hh;	// faction_table hash handle
	UT_hash_handle sorted_hh;	// sorted_factions hash handle
};


// how factions view each other
struct faction_relation {
	any_vnum vnum;	// who it's with
	bitvector_t flags;	// FCTR_ flags
	faction_data *ptr;	// quick reference
	
	UT_hash_handle hh;	// fct->relations hash handle
};


// for hash table of player faction levels
struct player_faction_data {
	any_vnum vnum;	// faction vnum
	int value;	// reputation points
	int rep;	// REP_ const
	UT_hash_handle hh;	// GET_FACTIONS(ch) hash
};


 //////////////////////////////////////////////////////////////////////////////
//// GAME STRUCTS ////////////////////////////////////////////////////////////

// For reboots/shutdowns
struct reboot_control_data {
	int type;	// REBOOT_NONE, REBOOT_REBOOT, REBOOT_SHUTDOWN
	int time;	// minutes
	int level;	// SHUTDOWN_NORMAL, SHUTDOWN_PAUSE, SHUTDOWN_DIE, SHUTDOWN_COMPLETE
	int immediate;	// TRUE/FALSE for instant reboot
};


 //////////////////////////////////////////////////////////////////////////////
//// GENERIC STRUCTS /////////////////////////////////////////////////////////

// generic data for currency, liquids, etc
struct generic_data {
	any_vnum vnum;
	
	char *name;	// for internal labeling
	int type;
	bitvector_t flags;	// GEN_ flags
	
	// data depends on type
	int value[NUM_GENERIC_VALUES];
	char *string[NUM_GENERIC_STRINGS];	// this can be expanded
	
	// connected to other generics
	struct generic_relation *relations;	// set in OLC
	struct generic_relation *computed_relations;	// determined at runtime (expanded list)
	
	UT_hash_handle hh;	// generic_table hash
	UT_hash_handle sorted_hh;	// sorted_generics hash
};


// this indicates a one-way "is a" relationship between one generic and another
struct generic_relation {
	any_vnum vnum;	// another generic
	UT_hash_handle hh;	// hashed by vnum of the other generic
};


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT STRUCTS //////////////////////////////////////////////////////////

// used for binding objects to players
struct obj_binding {
	int idnum;	// player id
	struct obj_binding *next;	// linked list
};


// object numeric data -- part of obj_data; often unique to each obj instance
struct obj_flag_data {
	int value[NUM_OBJ_VAL_POSITIONS];	// Values of the item (see list)
	bitvector_t wear_flags;	// Where you can wear it
	bitvector_t extra_flags;	// If it hums, glows, etc.
	int carrying_n;	// number of items inside
	int timer;	// Timer for object
	bitvector_t bitvector;	// To set chars bits (AFF_ flags)
	int current_scale_level;	// level the obj was scaled to, or -1 for not scaled
};


// object properties which never vary from the prototype
struct obj_proto_data {
	byte type_flag;	// Type of item
	int material;	// material this is made out of
	any_vnum component;	// matching generic component vnum
	
	bitvector_t tool_flags;	// any TOOL_ uses it provides when equipped
	
	int min_scale_level;	// minimum level this obj may be scaled to
	int max_scale_level;	// maximum level this obj may be scaled to
	
	any_vnum requires_quest;	// can only have obj whilst on quest
	bitvector_t requires_tool;	// tool required when crafting/building
	
	// lists
	struct extra_descr_data *ex_description;	// extra descriptions
	struct custom_message *custom_msgs;	// any custom messages
	struct interaction_item *interactions;	// interaction items
	struct obj_storage_type *storage;	// linked list of where an obj can be stored
	
	// lookup tables
	struct quest_lookup *quest_lookups;
	struct shop_lookup *shop_lookups;
};


// a game item
struct obj_data {
	obj_vnum vnum;	// object's virtual number
	ush_int version;	// for auto-updating objects
	struct obj_proto_data *proto_data;	// data which never changes on instances of objects
	
	room_data *in_room;	// In what room -- NULL when container/carried

	struct obj_flag_data obj_flags;	// Object information
	struct obj_apply *applies;	// APPLY_ list

	char *name;	// Title of object: get, etc.
	char *description;	// When in room (long desc)
	char *short_description;	// sentence-building; when worn/carry/in cont.
	char *action_description;	// What to write when looked at
	vehicle_data *in_vehicle;	// in vehicle or NULL
	char_data *carried_by;	// Carried by NULL in room/container
	char_data *worn_by;	// Worn by?
	sh_int worn_on;	// Worn where?
	
	empire_vnum last_empire_id;	// id of the last empire to have this
	int last_owner_id;	// last person to have the item
	time_t stolen_timer;	// when the object was last stolen
	empire_vnum stolen_from;	// empire who owned it
	
	struct stored_event *stored_events;	// linked list of stored dg events
	time_t autostore_timer;	// how long an object has been where it be
	
	struct obj_binding *bound_to;	// LL of who it's bound to
	struct eq_set_obj *eq_sets;	// LL of what eq sets it's part of

	obj_data *in_obj;	// In what object NULL when none
	obj_data *contains;	// Contains objects

	int script_id;	// used by DG triggers - unique id
	struct trig_proto_list *proto_script;	// list of default triggers
	struct script_data *script;	// script info for the object

	obj_data *prev_content, *next_content;	// For 'contains' doubly-linked lists
	obj_data *prev, *next;	// For the object double-linked list
	
	bool search_mark;	// for things that iterate over inventory/lists repeatedly
	
	UT_hash_handle hh;	// object_table hash
};


// for player equipment sets
struct eq_set_obj {
	int id;	// which set (for the current owner)
	int pos;	// wear location
	struct eq_set_obj *next;	// LL
};


// for where an item can be stored
struct obj_storage_type {
	int type;	// TYPE_BLD or TYPE_VEH
	any_vnum vnum;	// building/vehicle vnum
	int flags;	// STORAGE_x
	
	struct obj_storage_type *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// PROGRESS STRUCTS ////////////////////////////////////////////////////////

// progression goal
struct progress_data {
	any_vnum vnum;
	
	char *name;
	char *description;
	
	int version;	// for auto-updating
	int type;	// PROGRESS_ const
	bitvector_t flags;	// PRG_ flags
	int value;	// points
	int cost;	// in points
	
	// lists
	struct progress_list *prereqs;	// linked list of requires progress
	struct req_data *tasks;	// linked list of tasks to complete
	struct progress_perk *perks;	// linked list of perks granted
	
	UT_hash_handle hh;	// progress_table
	UT_hash_handle sorted_hh;	// sorted_progress
};


// basic list of progressions
struct progress_list {
	any_vnum vnum;
	struct progress_list *next;	// linked list
};


// for a linked list of things you get from a progression goal
struct progress_perk {
	int type;	// PRG_PERK_ const
	int value;
	struct progress_perk *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// QUEST STRUCTS ///////////////////////////////////////////////////////////

struct quest_data {
	any_vnum vnum;
	ush_int version;	// for auto-updating
	
	char *name;
	char *description;	// is this also the start-quest message?
	char *complete_msg;	// text shown on completion
	
	bitvector_t flags;	// QST_ flags
	struct quest_giver *starts_at;	// people/things that start the quest
	struct quest_giver *ends_at;	// people/things where you can turn it in
	struct req_data *tasks;	// list of objectives
	struct quest_reward *rewards;	// linked list
	
	// constraints
	int min_level;	// or 0 for no min
	int max_level;	// or 0 for no max
	struct req_data *prereqs;	// linked list of prerequisites
	int repeatable_after;	// minutes to repeat; NOT_REPEATABLE for none
	int daily_cycle;	// for dailies that rotate with others
	
	// misc data
	bool daily_active;	// if FALSE, quest is not available today
	
	struct trig_proto_list *proto_script;	// quest triggers
	
	UT_hash_handle hh;	// hash handle for quest_table
};


// for start, finish
struct quest_giver {
	int type;	// mob, obj, etc
	any_vnum vnum;	// what mob
	
	struct quest_giver *next;	// may have more than one
};


// reverse-lookups for quest givers
struct quest_lookup {
	quest_data *quest;
	struct quest_lookup *next;
};


// the spoils
struct quest_reward {
	int type;	// QR_ type
	any_vnum vnum;	// which one of type
	int amount;	// how many items, how much exp
	
	struct quest_reward *next;
};


// used for building a linked list of available quests
struct quest_temp_list {
	quest_data *quest;
	struct instance_data *instance;
	struct quest_temp_list *next;
};


// for tracking player quest completion
struct player_completed_quest {
	any_vnum vnum;	// which quest
	time_t last_completed;	// when
	
	any_vnum last_instance_id;	// where last completed one was acquired
	any_vnum last_adventure;	// which adventure it was acquired in
	
	UT_hash_handle hh;	// stored in player's hash table
};


// quests the player is on
struct player_quest {
	any_vnum vnum;	// which quest
	ush_int version;	// for auto-updating
	time_t start_time;	// when started
	
	struct req_data *tracker;	// quest tasks to track
	
	any_vnum instance_id;	// where it was acquired (if anywhere)
	any_vnum adventure;	// which adventure it was acquired in
	
	struct player_quest *next;	// linked list
};


 //////////////////////////////////////////////////////////////////////////////
//// SECTOR STRUCTS //////////////////////////////////////////////////////////

// data for online-editable sector data
struct sector_data {
	sector_vnum vnum;	// virtual number
	
	char *name;	// for stat/lookups/mapreader
	char *title;	// room title
	char *commands;	// commands shown below the map

	struct icon_data *icons;	// linked list of available icons
	char roadside_icon;	// single-character icon for showing on composite tiles
	int mapout;	// position in mapout_color_names, mapout_color_tokens
	
	int movement_loss;	// move point cost
	bitvector_t climate;	// CLIM_ flags
	bitvector_t flags;	// SECTF_ flags: warning: evolutions use these as flags in a SIGNED sbitvector_t
	bitvector_t build_flags;	// matches up with craft_data.build_on and .build_facing
	int temperature_type;	// TEMPERATURE_ const modifies the sector
	
	struct custom_message *custom_msgs;	// any custom messages
	struct spawn_info *spawns;	// mob spawn data
	struct evolution_data *evolution;	// change over time
	struct interaction_item *interactions;	// interaction items
	struct extra_descr_data *ex_description;	// extra descriptions
	
	char *notes;	// misc notes shown only to imms
	
	UT_hash_handle hh;	// sector_table hash
};


// for sector_data, to describe how a tile changes over time
struct evolution_data {
	int type;	// EVO_
	sbitvector_t value;	// used by some types, e.g. # of adjacent forests; sector flags; etc
	double percent;	// chance of happening per zone update
	sector_vnum becomes;	// sector to transform to
	
	struct evolution_data *next;
};


// for iteration of map locations by sector
struct sector_index_type {
	sector_vnum vnum;	// which sect
	
	struct map_data *sect_rooms;	// LL of rooms
	int sect_count;	// how many rooms in the list
	
	struct map_data *base_rooms;	// LL of rooms
	int base_count;	// number of rooms with it as the base sect
	
	UT_hash_handle hh;	// sector_index hash handle
};


 //////////////////////////////////////////////////////////////////////////////
//// SHOP STRUCTS ////////////////////////////////////////////////////////////

struct shop_data {
	any_vnum vnum;
	char *name;	// internal use
	
	bitvector_t flags;	// SHOP_ flags
	faction_data *allegiance;	// faction, if any
	int open_time;	// 0-23, if any
	int close_time;
	
	struct quest_giver *locations;	// shop locs
	struct shop_item *items;	// for sale
	
	UT_hash_handle hh;	// shop_table hash handle
};


// individual item in a shop
struct shop_item {
	obj_vnum vnum;	// the item
	int cost;
	any_vnum currency;	// generic vnum or NOTHING for coins
	int min_rep;	// reputation requirement if any (if a faction shop)
	struct shop_item *next;	// LL
};


// reverse-lookups for shops
struct shop_lookup {
	shop_data *shop;
	struct shop_lookup *next;
};


// used for building a linked list of local shops
struct shop_temp_list {
	shop_data *shop;
	
	char_data *from_mob;	// may
	obj_data *from_obj;	// be any
	room_data *from_room;	// of these
	vehicle_data *from_veh;	// things
	
	struct shop_temp_list *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// SOCIAL STRUCTS //////////////////////////////////////////////////////////

struct social_data {
	any_vnum vnum;
	
	char *name;	// for internal labeling
	char *command;	// as seen/typed by the player
	
	bitvector_t flags;	// SOC_ flags
	int min_char_position;	// POS_ of the character
	int min_victim_position;	// POS_ of victim
	struct req_data *requirements;	// linked list of requirements
	
	char *message[NUM_SOCM_MESSAGES];	// strings
	
	UT_hash_handle hh;	// social_table hash
	UT_hash_handle sorted_hh;	// sorted_socials hash
};


 //////////////////////////////////////////////////////////////////////////////
//// TRIGGER STRUCTS /////////////////////////////////////////////////////////

// linked list for mob/object prototype trigger lists
struct trig_proto_list {
	trig_vnum vnum;	/* vnum of the trigger   */
	struct trig_proto_list *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// VEHICLE STRUCTS /////////////////////////////////////////////////////////

struct vehicle_data {
	any_vnum vnum;
	int idnum;		// unique id for the vehicle
	
	char *keywords;	// targeting terms
	char *short_desc;	// the sentence-building name ("a canoe")
	char *long_desc;	// As described in the room.
	char *look_desc;	// Description when looked at
	char *icon;	// Optional: Shown on map if not null
	
	struct vehicle_attribute_data *attributes;	// non-instanced data
	bitvector_t flags;	// VEH_ flags
	
	// instance data
	empire_data *owner;	// which empire owns it, if any
	int scale_level;	// determines amount of damage, etc
	double health;	// current health
	obj_data *contains;	// contains objects
	int carrying_n;	// size of contents
	struct vehicle_attached_mob *animals;	// linked list of mobs attached
	struct depletion_data *depletion;	// resource depletions tied to the vehicle
	struct resource_data *needs_resources;	// resources until finished/maintained
	struct resource_data *built_with;	// resources used to build it
	room_data *interior_home_room;	// the vehicle's main room
	struct vehicle_room_list *room_list;	// all interior rooms
	int inside_rooms;	// how many rooms are inside
	time_t last_fire_time;	// for vehicles with siege weapons
	time_t last_move_time;	// for autostore
	room_data *in_room;	// where it is
	char_data *led_by;	// person leading it
	char_data *sitting_on;	// person sitting on it
	char_data *driver;	// person driving it
	int construction_id;	// temporary id used to resume construction/dismantle
	struct room_extra_data *extra_data;	// hash of misc storage
	any_vnum instance_id;	// adventure instance the vehicle belongs to, or NOTHING if none
	bitvector_t room_affects;	// ROOM_AFF_ flags applied to the room while veh is here
	
	// scripting
	int script_id;	// used by DG triggers - unique id (only set if a script has referenced it)
	struct trig_proto_list *proto_script;	// list of default triggers
	struct script_data *script;	// script info for the object
	
	// this helps with updating vehicles when they change rooms/islands
	room_data *applied_to_room;	// room its traits were applied to, if any
	int applied_to_island;	// island its traits were applied to (by id), if any
	
	// lists
	struct vehicle_data *prev, *next;	// vehicle_list (global) doubly-linked list
	struct vehicle_data *prev_in_room, *next_in_room;	// ROOM_VEHICLES(room) doubly-linked list
	struct quest_lookup *quest_lookups;
	struct shop_lookup *shop_lookups;
	UT_hash_handle hh;	// vehicle_table hash handle
};


// data that only prototypes need
struct vehicle_attribute_data {
	int maxhealth;	// total hitpoints
	int min_scale_level;	// minimum level
	int max_scale_level;	// maximum level
	int capacity;	// holds X items
	int animals_required;	// number of animals to move it
	int move_type;	// MOB_MOVE_ type
	bld_vnum interior_room_vnum;	// Any ROOM-flagged bld to use as an interior
	int max_rooms;	// 1 = can enter; >1 allows designate
	bitvector_t designate_flags;	// DES_ flags
	struct resource_data *regular_maintenance;	// needed each reset cycle
	int veh_move_speed;  // VSPEED_ for driving action speed
	int size;	// vehicle size
	struct extra_descr_data *ex_description;	// extra descriptions
	struct interaction_item *interactions;	// interaction items
	struct spawn_info *spawns;	// linked list of spawn data
	bitvector_t functions;	// FNC_ flags offered to the room the vehicle is in
	bitvector_t requires_climate;	// CLIM_ flags required for this vehicle to enter a room
	bitvector_t forbid_climate;	// CLIM_ flags that block this vehicle from entering
	mob_vnum artisan_vnum;	// vnum of an artisan to load
	int citizens;	// how many citizens can live here (outside)
	int fame;	// how much fame it adds to the empire
	int military;	// how much it adds to the military pool
	struct custom_message *custom_msgs;	// any custom messages
	struct bld_relation *relations;	// links to buildings/vehicles
	int height;	// 0+ addition to terrain height
};


// data for a mob that's attached to the vehicle and extracted
struct vehicle_attached_mob {
	mob_vnum mob;	// which mob
	int scale_level;	// what level it was
	bitvector_t flags;	// mob flags
	empire_vnum empire;	// empire that owned it
	struct vehicle_attached_mob *next;	// linked list
};


// list of rooms inside a vehicle
struct vehicle_room_list {
	room_data *room;
	struct vehicle_room_list *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// WEATHER AND SEASON STRUCTS //////////////////////////////////////////////

// TILESET_x, used in sector_data
struct tileset_data {
	char *icon;  // ^^^^
	char *color;  // e.g. &g
};


// global weather data
// TODO - I dream of making this regional or per-island -pc
struct weather_data {
	int pressure;	// How is the pressure ( Mb )
	int change;	// How fast and what way does it change
	int sky;	// How is the sky
	// formerly sunlight -- now use get_sun_status(room)
};


 //////////////////////////////////////////////////////////////////////////////
//// WORLD STRUCTS ///////////////////////////////////////////////////////////


// init_room(), delete_room(), disassociate_building()
struct room_data {
	room_vnum vnum; // room number (vnum)
	
	empire_data *owner;  // who owns this territory
	
	sector_data *sector_type;  // terrain type -- saved in file as vnum
	sector_data *base_sector;  // for when built-over -- ^
	crop_data *crop_type;	// if this room has a crop, this is it
	
	struct map_data *map_loc;	// map location if any
	struct complex_room_data *complex; // for rooms that are buildings, inside, adventures, etc
	struct shared_room_data *shared;	// data that could be local OR from the map tile
	sh_int light;  // number of light sources
	int exits_here;	// number of rooms that have complex->exits to this one
	
	struct affected_type *af;  // room affects
	
	time_t last_spawn_time;  // used to spawn npcs
	ubyte pathfind_key;	// for the pathfidning system
	
	struct trig_proto_list *proto_script;	/* list of default triggers  */
	struct script_data *script;	/* script info for the room           */

	obj_data *contents;  // start of doubly-linked item list (obj->next_content)
	char_data *people;  // start of people doubly-linked list (ch->next_in_room, prev_in_room)
	vehicle_data *vehicles;	// start of doubly-linked vehicle list (veh->next_in_room, prev_in_room)
	
	struct reset_com *reset_commands;	// doubly-linked list used only during startup
	struct dg_event *unload_event;	// used for un-loading of live rooms
	ubyte save_info;	// SAVE_INFO_ flags
	
	UT_hash_handle hh;	// hash handle for world_table
	room_data *prev_interior, *next_interior;	// doubly linked list: interior_room_list
};


// data used only for rooms that are map buildings, interiors, or adventures
struct complex_room_data {
	struct room_direction_data *exits;  // directions
	
	struct resource_data *to_build;  // for incomplete/dismantled buildings
	struct resource_data *built_with;	// actual list of things used to build it
	
	// pointers to the room's building or room template data, IF ANY
	bld_data *bld_ptr;	// point to building_table proto
	room_template *rmt_ptr;	// points to room_template_table proto
	
	byte entrance;  // direction of entrance
	// int patron;  // for shrine gods -- removed in b5.108, stored as extra data
	byte inside_rooms;  // count of designated rooms inside
	room_data *home_room;  // for interior rooms (and boats and instances), means this is actually part of another room; is saved as vnum to file but is room_data* in real life
	
	int disrepair;	// UNUSED: as of b4.15 this is 0 for all buildings and not used (but is saved to file and coule be repurposed)
	
	vehicle_data *vehicle;  // the associated vehicle (usually only on the home room)
	struct instance_data *instance;	// if part of an instantiated adventure
	
	// int paint_color;	// for the 'paint' command -- as of b5.108 this is stored as extra data
	int private_owner;	// for privately-owned houses
	
	time_t burn_down_time;	// if >0, the timestamp when this building will burn down
	
	double damage;  // for catapulting
	
	// additional unsaved data
	int applied_to_island;	// tracks which island its techs were applied to
};


// data that could be from the map tile (for map rooms) or local (non-map rooms)
// NOTE: if you add something here where the default is not 0, add to init_map() too
// NOTE: if you add anything here, it MUST also be added to:
//			- if it's flat data (int, etc), create a new map_to_store function
//				- and update the version stuff at the top of db.h
//				- and update write_fresh_binary_map_file()
//			- if it's variable data (lists, strings), add to these 3 places:
//				- HAS_SHARED_DATA_TO_SAVE() macro
//				- write_map_and_room_to_file() for saving it (in the shared section)
//				- load_one_room_from_wld_file() for loading it
struct shared_room_data {
	int island_id;	// the island id (may be NO_ISLAND)
	struct island_info *island_ptr;	// pointer to the island (may be NULL)
	int height;	// mountain height, or river distance-from-ocean
	
	// custom data
	char *name;  // room name may be set
	char *description;  // so may a description
	char *icon;  // same with map icon
	
	bitvector_t affects;  // affect bitvector (modified)
	bitvector_t base_affects;  // base affects
	
	// lists
	struct depletion_data *depletion;	// resource depletion
	struct room_extra_data *extra_data;	// hash of misc storage
	struct track_data *tracks;	// hash: for tracking
	
	// events
	struct stored_event *events;	// linked list of stored events
};


// for resource depletion
struct depletion_data {
	int type;
	int count;
	
	struct depletion_data *next;
};


// data for the island_table
struct island_info {
	any_vnum id;	// game-assigned, permanent id
	char *name;	// global name for the island
	char *desc;	// description of the island
	bitvector_t flags;	// ISLE_ flags
	
	// computed data
	int min_level;	// of players on the island
	int max_level;	// determined at startup and on-move
	int tile_size;
	room_vnum center;
	room_vnum edge[NUM_SIMPLE_DIRS];	// edges
	
	UT_hash_handle hh;	// island_table hash
};


// structure for the reset commands
struct reset_com {
	char command;	// current command

	long long arg1;
	long long arg2;	// Arguments to the command
	long long arg3;
	long long arg4;
	long long arg5;

	char *sarg1;	// string argument
	char *sarg2;	// string argument
	
	struct reset_com *prev, *next;	// DLL
};


// exits
struct room_direction_data {
	int dir;	// which direction (e.g. NORTH)
	char *keyword;	// for open/close

	sh_int exit_info;	// EX_x
	room_vnum to_room;	// Where direction leads (or NOWHERE)
	room_data *room_ptr;	// direct link to a room (set in renum_world)
	
	struct room_direction_data *next;
};


// for storing misc vals to the room (or vehicle)
struct room_extra_data {
	int type;	// ROOM_EXTRA_
	int value;
	
	UT_hash_handle hh;	// room->shared->extra_data hash
};


// for tracking
struct track_data {
	int id;	// positive = mob vnum, negative = player idnum
	
	time_t timestamp;	// when
	byte dir;	// which way (may be NO_DIR)
	room_vnum to_room;	// for tracks that enter portals/vehicles
	
	UT_hash_handle hh;	// tracks are hashed by positive vnum or negative player id
};


// data for the world map (world_map, land_map)
struct map_data {
	room_vnum vnum;	// corresponding room vnum (coordinates can be derived from here)
	room_data *room;	// pointer is set IF it exists in the world_table right now (otherwise load with real_room)
	
	// three basic sector types
	sector_data *sector_type;	// current sector
	sector_data *base_sector;	// underlying current sector (e.g. plains under building)
	sector_data *natural_sector;	// sector at time of map generation
	
	struct shared_room_data *shared;	// for map tiles' room_data*, they point to this
	crop_data *crop_type;	// possible crop type
	
	ubyte pathfind_key;	// for the pathfinding system
	
	// lists
	struct map_data *next_in_sect;	// LL of all map locations of a given sect
	struct map_data *next_in_base_sect;	// LL for base sect
	struct map_data *next;	// linked list of non-ocean tiles, for iterating
};
