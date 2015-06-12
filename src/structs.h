/* ************************************************************************
*   File: structs.h                                       EmpireMUD 2.0b1 *
*  Usage: header file for central structures and contstants               *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/**
* Contents:
*   Master Configuration
*   Basic Types and Consts
*   #define Section
*     Miscellaneous Defines
*     Adventure Defines
*     Building Defines
*     Character Defines
*     Craft Defines
*     Crop Defines
*     Empire Defines
*     Game Defines
*     Mobile Defines
*     Object Defines
*     Player Defines
*     Sector Defines
*     World Defines
*     Maxima and Limits
*   Structs Section
*     Miscellaneous Structs
*     Adventure Structs
*     Building Structs
*     Mobile Structs
*     Player Structs
*     Character Structs
*     Craft Structs
*     Crop Structs
*     Data Structs
*     Empire Structs
*     Fight Structs
*     Game Structs
*     Object Structs
*     Sector Structs
*     Trigger Structs
*     Weather and Season Structs
*     World Structs
*/

// Note: You can usually find consts or other things related to a set of flags
// by searching for CONST_PREFIX_x (e.g. WEAR_x for wear flags).


#include "uthash.h"	// needed by everything

 //////////////////////////////////////////////////////////////////////////////
//// MASTER CONFIGURATION ////////////////////////////////////////////////////

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

#define COIN_VALUE  0.1	// value of a coin as compared to 1 wealth (0.1 coin value = 10 coins per wealth)

// ***WARNING*** Change this before starting your playerfile
// but NEVER change it after, or you may not be able to log in to any character
#define PASSWORD_SALT  "salt"

// this determines if a room is "empty ocean" and doesn't need to stay in RAM
#define BASIC_OCEAN  6	// sector to un-load and recreate as-needed
#define CAN_UNLOAD_MAP_ROOM(room)  ( \
	GET_SECT_VNUM(SECT(room)) == BASIC_OCEAN && !COMPLEX_DATA(room) && \
	GET_ROOM_VNUM(room) < MAP_SIZE && \
	!ROOM_OWNER(room) && !ROOM_CONTENTS(room) && !ROOM_PEOPLE(room) && \
	!ROOM_DEPLETION(room) && !ROOM_CUSTOM_NAME(room) && \
	!ROOM_CUSTOM_ICON(room) && !ROOM_CUSTOM_DESCRIPTION(room) && \
	!ROOM_AFFECTS(room) && ROOM_BASE_FLAGS(room) == NOBITS && \
	!(room)->extra_data && !(room)->script \
)	// end CAN_UNLOAD_MAP_ROOM()


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
#define NO_WEAR  -1	// bad wear location
#define END_RESOURCE_LIST  { NOTHING, -1 }
#define NOBITS  0	// lack of bitvector bits (for clarity in funky structs)
#define NO_ABIL  NO_SKILL	// things that don't require an ability
#define NO_FLAGS  0
#define NO_SKILL  -1	// things that don't require a skill
#define OTHER_COIN  NOTHING	// use the NOTHING value to store the "other" coin type (which stores by empire id)
#define REAL_OTHER_COIN  NULL	// for when other-coin type is an empire pointer
#define UNLIMITED  -1	// unlimited duration/timer


// Various other special codes
#define MAGIC_NUMBER  (0x06)	// Arbitrary number that won't be in a string


// for things that hold more than one type of thing:
#define TYPE_OBJ  0
#define TYPE_MOB  1
#define TYPE_ROOM  2


// basic types
typedef char byte;
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


// vnums
typedef int any_vnum;
typedef any_vnum adv_vnum;	// for adventure zones
typedef any_vnum bld_vnum;	// for building[].vnum
typedef any_vnum craft_vnum;	// craft recipe's vnum
typedef any_vnum crop_vnum;	// crop's vnum
typedef any_vnum empire_vnum;	// empire virtual number
typedef any_vnum mob_vnum;	/* A mob's vnum type */
typedef any_vnum obj_vnum;	/* An object's vnum type */
typedef any_vnum rmt_vnum;	// room_template
typedef any_vnum room_vnum;	/* A room's vnum type */
typedef any_vnum sector_vnum;	// sector's vnum
typedef any_vnum trig_vnum;	// for dg scripts


// For simplicity...
typedef struct adventure_data adv_data;
typedef struct bld_data bld_data;
typedef struct char_data char_data;
typedef struct craft_data craft_data;
typedef struct crop_data crop_data;
typedef struct descriptor_data descriptor_data;
typedef struct empire_data empire_data;
typedef struct index_data index_data;
typedef struct obj_data obj_data;
typedef struct room_data room_data;
typedef struct room_template room_template;
typedef struct sector_data sector_data;
typedef struct trig_data trig_data;


  /////////////////////////////////////////////////////////////////////////////
 //// #DEFINE SECTION ////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS DEFINES ///////////////////////////////////////////////////

// Modifier constants used with obj affects ('A' fields), player affect types, etc
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
#define APPLY_SOAK  12	// Apply to soak pool
#define APPLY_BLOCK  13	// Apply to chance to block
#define APPLY_HEAL_OVER_TIME  14	// heals you every 5
#define APPLY_HEALTH  15	// Apply to max health
#define APPLY_MANA  16	// Apply to max mana
#define APPLY_TO_HIT  17	// +to-hit
#define APPLY_DODGE  18	// +dodge
#define APPLY_INVENTORY  19	// add inventory capacity
#define APPLY_BLOOD  20	// add to max blood
#define APPLY_BONUS_PHYSICAL  21	// add to bonus damage
#define APPLY_BONUS_MAGICAL  22	// add to magical damage
#define APPLY_BONUS_HEALING  23	// add to healing


// don't change these
#define BAN_NOT  0
#define BAN_NEW  1
#define BAN_SELECT  2
#define BAN_ALL  3


// Group Defines
#define GROUP_ANON  BIT(0)	// Group is hidden/anonymous


// mob/room interactions
#define INTERACT_BUTCHER  0
#define INTERACT_SKIN  1
#define INTERACT_SHEAR  2
#define INTERACT_BARDE  3
#define INTERACT_LOOT  4
#define INTERACT_DIG  5
#define INTERACT_FORAGE  6
#define INTERACT_FIND_HERB  7
#define INTERACT_HARVEST  8
#define INTERACT_GATHER  9
#define INTERACT_ENCOUNTER  10
#define INTERACT_LIGHT  11
#define NUM_INTERACTS  12


// mob spawn flags
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


// trading post data state flags
#define TPD_FOR_SALE  BIT(0)	// main phase of sale
#define TPD_BOUGHT  BIT(1)	// ended with sale
#define TPD_EXPIRED  BIT(2)	// ended without sale
#define TPD_OBJ_PENDING  BIT(3)	// obj not received
#define TPD_COINS_PENDING  BIT(4)	// coins not received


 //////////////////////////////////////////////////////////////////////////////
//// ADVENTURE DEFINES ///////////////////////////////////////////////////////

// adventure flags
#define ADV_IN_DEVELOPMENT  BIT(0)	// will not generate instances
#define ADV_LOCK_LEVEL_ON_ENTER  BIT(1)	// lock levels on entry
#define ADV_LOCK_LEVEL_ON_COMBAT  BIT(2)	// lock levels when combat starts
#define ADV_NO_NEARBY  BIT(3)	// hide from mortal nearby command
#define ADV_ROTATABLE  BIT(4)	// random rotation on instantiate
#define ADV_CONFUSING_RANDOMS  BIT(5)	// random exits do not need to match


// adventure link rule types
#define ADV_LINK_BUILDING_EXISTING  0
#define ADV_LINK_BUILDING_NEW  1
#define ADV_LINK_PORTAL_WORLD  2
#define ADV_LINK_PORTAL_BUILDING_EXISTING  3
#define ADV_LINK_PORTAL_BUILDING_NEW  4
#define ADV_LINK_TIME_LIMIT  5
#define ADV_LINK_NOT_NEAR_SELF  6


// adventure link rule flags
#define ADV_LINKF_CLAIMED_OK  BIT(0)	// can spawn on claimed territory
#define ADV_LINKF_CITY_ONLY  BIT(1)	// only spawns on claimed land in cities
#define ADV_LINKF_NO_CITY  BIT(2)	// won't spawn on claimed land in cities


// adventure spawn types
#define ADV_SPAWN_MOB  0
#define ADV_SPAWN_OBJ  1


// instance flags
#define INST_COMPLETED  BIT(0)	// instance is done and can be cleaned up


// room template flags
#define RMT_OUTDOOR  BIT(0)	// a. sun during day
#define RMT_DARK  BIT(1)	// b. requires a light at all times
#define RMT_LIGHT  BIT(2)	// c. never requires a light
#define RMT_NO_MOB  BIT(3)	// d. mobs won't enter
#define RMT_PEACEFUL  BIT(4)	// e. no attacking/saferoom
#define RMT_NEED_BOAT  BIT(5)	// f. requires a boat
#define RMT_NO_TELEPORT  BIT(6)	// g. cannot teleport in/out


 //////////////////////////////////////////////////////////////////////////////
//// BUILDING DEFINES ////////////////////////////////////////////////////////

// building data flags
#define BLD_ROOM  BIT(0)	// a designatable interior room instead of a whole building
#define BLD_ALLOW_MOUNTS  BIT(1)	// can ride/mount in
#define BLD_TWO_ENTRANCES  BIT(2)	// has a rear door
#define BLD_OPEN  BIT(3)	// always-open
#define BLD_CLOSED  BIT(4)	// always-closed
#define BLD_INTERLINK  BIT(5)	// can be interlinked
#define BLD_HERD  BIT(6)	// can herd
#define BLD_DEDICATE  BIT(7)	// can be dedicated to a player
#define BLD_DRINK  BIT(8)	// can be used to drink/fill water
#define BLD_NO_NPC  BIT(9)	// mobs won't walk in
#define BLD_BARRIER  BIT(10)	// can only go back the direction you came
#define BLD_TAVERN  BIT(11)	// building functions as a tavern
#define BLD_COUNTS_AS_CITY  BIT(12)	// never counts toward outside territory
#define BLD_MINE  BIT(13)	// building is minable
#define BLD_ATTACH_ROAD  BIT(14)	// building connects to roads on the map
#define BLD_BURNABLE  BIT(15)	// fire! fire!
#define BLD_FORGE  BIT(16)	// can forge here
#define BLD_COOKING_FIRE  BIT(17)	// allows cook
#define BLD_ALCHEMIST  BIT(18)	// provides alchemy tools
#define BLD_STABLE  BIT(19)	// mobs don't despawn; can shear, barde, etc
#define BLD_LIBRARY  BIT(20)	// can use library and bookedit commands
#define BLD_APIARY  BIT(21)	// provides Apiaries tech
#define BLD_GLASSBLOWER  BIT(22)	// provides Glassblowing tech
#define BLD_DOCKS  BIT(23)	// provides Seaport tech
#define BLD_PIGEON_POST  BIT(24)	// can use mail
#define BLD_MILL  BIT(25)	// can use the mill command
#define BLD_POTTER  BIT(26)	// pottery crafts go faster
#define BLD_TAILOR  BIT(27)	// can use tailoring abilities here
#define BLD_BATHS  BIT(28)	// can bathe here
#define BLD_SAIL  BIT(29)	// ships can pass through building
#define BLD_TOMB  BIT(30)	// can set as a tomb
#define BLD_MINT  BIT(31)	// can mint coins
#define BLD_VAULT  BIT(32)	// for various vault functions
#define BLD_ITEM_LIMIT  BIT(33)	// room does not hold unlimited items
#define BLD_LONG_AUTOSTORE  BIT(34)	// autostore takes a long time here
#define BLD_WAREHOUSE  BIT(35)	// warehouse command
#define BLD_TRADE  BIT(36)	// trading post / trade command
#define BLD_HIGH_DEPLETION  BIT(37)	// allows more resource farming here
#define BLD_PORTAL  BIT(38)	// functions as a portal building
#define BLD_BEDROOM  BIT(39)	// boosts regen when sleeping


// Terrain flags for do_build -- these match up with build_on flags for building crafts
#define BLD_ON_WATER  BIT(0)
#define BLD_ON_PLAINS  BIT(1)
#define BLD_ON_MOUNTAIN  BIT(2)
#define BLD_ON_FOREST  BIT(3)
#define BLD_ON_DESERT  BIT(4)
#define BLD_ON_RIVER  BIT(5)
#define BLD_ON_JUNGLE  BIT(6)
	#define BLD_ON_UNUSED2  BIT(7)
#define BLD_ON_OCEAN  BIT(8)
#define BLD_ON_OASIS  BIT(9)
#define BLD_FACING_CROP  BIT(10)	// use only for facing, not for on
#define BLD_ON_GROVE  BIT(11)
#define BLD_ON_SWAMP  BIT(12)
#define BLD_ANY_FOREST  BIT(13)
#define BLD_FACING_OPEN_BUILDING  BIT(14)


// tavern types
#define BREW_NONE 0
#define BREW_ALE  1
#define BREW_LAGER  2
#define BREW_WHEATBEER  3
#define BREW_CIDER  4
#define NUM_BREWS  5	// total


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


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER DEFINES ///////////////////////////////////////////////////////

// energy pools for char_point_data
#define HEALTH  0
#define MOVE  1
#define MANA  2
#define BLOOD  3
#define NUM_POOLS  4	// used in char_file_u -- **DO NOT CHANGE**


// core attributes for char_file_u and char_data
#define STRENGTH  0
#define DEXTERITY  1
#define CHARISMA  2
#define GREATNESS  3
#define INTELLIGENCE  4
#define WITS  5
#define NUM_ATTRIBUTES  6	// used in char_file_u -- **DO NOT CHANGE**


// extra attributes -- ATT_x
#define ATT_BONUS_INVENTORY  0	// carry capacity
#define ATT_SOAK  1	// damage absorption
#define ATT_BLOCK  2	// bonus block chance
#define ATT_TO_HIT  3	// bonus to-hit chance
#define ATT_DODGE  4	// bonus dodge chance
#define ATT_EXTRA_BLOOD  5	// larger blood pool
#define ATT_BONUS_PHYSICAL  6	// extra physical damage
#define ATT_BONUS_MAGICAL  7	// extra magical damage
#define ATT_BONUS_HEALING  8	// extra healing
#define ATT_HEAL_OVER_TIME  9	// heal per 5
#define NUM_EXTRA_ATTRIBUTES  10
#define TOTAL_EXTRA_ATTRIBUTES  20	// including spares, 10 spares remaining -- **DO NOT CHANGE**


/* Affect bits */
/* WARNING: In the world files, NEVER set the bits marked "R" ("Reserved") */
#define AFF_BLIND  BIT(0)	// a. (R) Char is blind
#define AFF_MAJESTY  BIT(1)	// b. majesty
#define AFF_INFRAVISION  BIT(2)	// c. Char can see full in dark
#define AFF_SNEAK  BIT(3)	// d. Char can move quietly
#define AFF_HIDE  BIT(4)	// e. Char is hidden
#define AFF_CHARM  BIT(5)	// f. Char is charmed
#define AFF_INVISIBLE  BIT(6)	// g. Char is invisible
#define AFF_IMMUNE_BATTLE  BIT(7)	// h. Immunity to Battle debuffs
#define AFF_SENSE_HIDE  BIT(8)	// i. See hidden people
#define AFF_IMMUNE_PHYSICAL  BIT(9)	// j. Immune to physical damage
#define AFF_NO_TARGET_IN_ROOM  BIT(10)	// k. no-target
#define AFF_NO_SEE_IN_ROOM  BIT(11)	// l. don't see on look
#define AFF_FLY  BIT(12)	// m. person can fly
#define AFF_NO_ATTACK  BIT(13)	// n. can't be attacked
#define AFF_IMMUNE_HIGH_SORCERY  BIT(14)	// o. immune to high sorcery debuffs
#define AFF_DISARM  BIT(15)	// p. disarmed
#define AFF_HASTE  BIT(16)	// q. haste: attacks faster
#define AFF_ENTANGLED  BIT(17)	// r. entangled: can't move
#define AFF_SLOW  BIT(18)	// s. slow (how great did that work out)
#define AFF_STUNNED  BIT(19)	// t. stunned/unable to act
#define AFF_STONED  BIT(20)	// u. trippy effects
#define AFF_CANT_SPEND_BLOOD  BIT(21)	// v. hinder vitae
#define AFF_CLAWS  BIT(22)	// w. claws
#define AFF_DEATHSHROUD  BIT(23)	// x. deathshroud
#define AFF_EARTHMELD  BIT(24)	// y. interred in the earth
#define AFF_MUMMIFY  BIT(25)	// z. mummified
#define AFF_SOULMASK  BIT(26)	// A. soulmask
#define AFF_IMMUNE_NATURAL_MAGIC  BIT(27)	// B. immune to natural magic debuffs
#define AFF_IMMUNE_STEALTH  BIT(28)	// C. Immune to stealth debuffs
#define AFF_IMMUNE_VAMPIRE  BIT(29)	// D. Immune to vampire debuffs
#define AFF_IMMUNE_STUN  BIT(30)	// E. Cannot be hit by stun effects


// Injury flags -- IS_INJURED
#define INJ_TIED  BIT(0)	/* Bound and gagged						*/
#define INJ_STAKED  BIT(1)	/* Stake thru heart						*/


// sex
#define SEX_NEUTRAL  0
#define SEX_MALE  1
#define SEX_FEMALE  2
#define NUM_GENDERS  3	// total


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
//// CRAFT DEFINES ///////////////////////////////////////////////////////////

// do_gen_craft (trade.c)
#define CRAFT_TYPE_ERROR  0
#define CRAFT_TYPE_FORGE  1
#define CRAFT_TYPE_CRAFT  2
#define CRAFT_TYPE_COOK  3
#define CRAFT_TYPE_SEW  4
#define CRAFT_TYPE_MILL  5
#define CRAFT_TYPE_BREW  6
#define CRAFT_TYPE_MIX  7
#define CRAFT_TYPE_BUILD  8


// Craft Flags for do_gen_craft
#define CRAFT_POTTERY  BIT(0)  // bonus at pottery; requires fire
#define CRAFT_APIARIES  BIT(1)  // requires apiary tech
#define CRAFT_GLASS  BIT(2)  // requires glassblowing tech
#define CRAFT_GLASSBLOWER  BIT(3)  // requires glassblower building
#define CRAFT_CARPENTER  BIT(4)  // requires carpenter building
#define CRAFT_ALCHEMY  BIT(5)  // requires access to glass/alchemist and fire
#define CRAFT_SHARP  BIT(6)  // requires sharp tool
#define CRAFT_FIRE  BIT(7)  // requires any fire source
#define CRAFT_SOUP  BIT(8)  // is a soup: requires a container of water, and the "object" property is a liquid id
#define CRAFT_IN_DEVELOPMENT  BIT(9)	// craft cannot be performed by mortals
#define CRAFT_UPGRADE  BIT(10)	// build: is an upgrade, not a new building
#define CRAFT_DISMANTLE_ONLY  BIT(11)	// build: building can be dismantled but not built
#define CRAFT_IN_CITY_ONLY  BIT(12)	// craft/building must be inside a city


// For find_building_list_entry
#define FIND_BUILD_NORMAL  0
#define FIND_BUILD_UPGRADE  1


 //////////////////////////////////////////////////////////////////////////////
//// CROP DEFINES ////////////////////////////////////////////////////////////

// crop flags
#define CROPF_REQUIRES_WATER  BIT(0)	// only plants near water
#define CROPF_IS_ORCHARD  BIT(1)	// follows orchard rules
#define CROPF_NOT_WILD  BIT(2)	// crop will never spawn in the wild


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE DEFINES //////////////////////////////////////////////////////////


// empire trait flags
#define ETRAIT_DISTRUSTFUL  BIT(0)	// hostile behavior


// workforce types
#define CHORE_BUILDING  0
#define CHORE_FARMING  1
#define CHORE_REPLANTING  2
#define CHORE_CHOPPING  3
#define CHORE_MAINTENANCE  4
#define CHORE_MINING  5
#define CHORE_DIGGING  6
#define CHORE_SAWING  7
#define CHORE_SCRAPING  8
#define CHORE_SMELTING  9
#define CHORE_WEAVING  10
#define CHORE_QUARRYING  11
#define CHORE_NAILMAKING  12
#define CHORE_BRICKMAKING  13
#define CHORE_AUTO_ABANDON  14
#define CHORE_HERB_GARDENING  15
#define CHORE_FIRE_BRIGADE  16
#define CHORE_TRAPPING  17
#define CHORE_TANNING  18
#define CHORE_SHEARING  19
#define CHORE_MINTING  20
#define CHORE_AUTO_BALANCE  21
#define NUM_CHORES  22		// total


/* Diplomacy types */
#define DIPL_PEACE  BIT(0)	// At peace
#define DIPL_WAR  BIT(1)	// At war
#define DIPL_ALLIED  BIT(3)	// In an alliance
#define DIPL_NONAGGR  BIT(4)	// In a non-aggression pact
#define DIPL_TRADE  BIT(5)	// Open trading
#define DIPL_DISTRUST  BIT(6)	// Distrusting of one another
#define DIPL_TRUCE  BIT(7)	// end of war but not peace


// empire_log_data types
#define ELOG_NONE  0	// does not log to file
#define ELOG_ADMIN  1	// administrative changes
#define ELOG_DIPLOMACY  2	// all diplomacy commands
#define ELOG_HOSTILITY  3	// any hostile actions
#define ELOG_MEMBERS  4	// enroll, promote, demote, etc
#define ELOG_TERRITORY  5	// territory changes
#define ELOG_TRADE  6	// auto-trades


// for empire_unique_storage->flags
#define EUS_VAULT  BIT(0)	// requires privilege


// Empire Privilege Levels
#define PRIV_CLAIM  0	// Claim land
#define PRIV_BUILD  1	// Build/Dismantle structures
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
#define NUM_PRIVILEGES  15	// total


// for empire scores (e.g. sorting)
#define SCORE_WEALTH  0
#define SCORE_TERRITORY  1
#define SCORE_MEMBERS  2
#define SCORE_TECHS  3
#define SCORE_EINV  4
#define SCORE_GREATNESS  5
#define SCORE_DIPLOMACY  6
#define SCORE_FAME  7
#define SCORE_MILITARY  8
#define SCORE_PLAYTIME  9
#define NUM_SCORES  10	// total


// Technologies
#define TECH_GLASSBLOWING  0
#define TECH_CITY_LIGHTS  1
#define TECH_LOCKS  2
#define TECH_APIARIES  3
#define TECH_SEAPORT  4
#define TECH_WORKFORCE  5
#define TECH_PROMINENCE  6
#define TECH_COMMERCE  7
#define TECH_PORTALS  8
#define TECH_MASTER_PORTALS  9
#define TECH_SKILLED_LABOR  10
#define TECH_TRADE_ROUTES  11
#define NUM_TECHS  12


// for empire_trade_data
#define TRADE_EXPORT  0
#define TRADE_IMPORT  1


// for can_use_room
#define GUESTS_ALLOWED  0
#define MEMBERS_ONLY  1
#define MEMBERS_AND_ALLIES  2


 //////////////////////////////////////////////////////////////////////////////
//// GAME DEFINES ////////////////////////////////////////////////////////////

/* Access Level constants *****************************************************
 *
 * How to add immortal levels to your mud:
 *  - Change the level defines here, adding more levels between GOD and IMPL
 *  - Make sure that the levels for commands in interpreter.c are okay
 *  - Unless you have more than three immortal levels, the mud will auto-
 *    magically set the command levels for you.
 *  - Make sure LVL_TOP is no less than 4!
 *  - If you want more levels than those listed in the chart, you'll have to
 *    make necessary adjustments yourself:
 *    = Add a clause in utils/autowiz.c: level_params[]
 *    = Add a clause in constants.c: level_names[][2]
 *    = Change interpreter.c command levels
 *
 *  = Future plans: An option to turn on mortal levels
 *
 * Quick Reference:
 *  Levels    You'll Have
 *  ------    -----------
 *    4           Unapproved, Approved, God, Implementor
 *    5           Unapproved, Approved, God, Immortal, Implementor
 *    6           Unapproved, Approved, God, Immortal, Assistant, Implementor
 *    7           Unapproved, Approved, God, Immortal, Assistant, Co-Implementor, Implementor
 */

#define LVL_TOP  7	// Highest possible access level
#define LVL_APPROVED  2	// Level when players can use commands -- don't set less than 2

// admin levels
#define LVL_IMPL  LVL_TOP
#define LVL_CIMPL  (LVL_ASST < LVL_TOP ? LVL_ASST + 1 : LVL_ASST)
#define LVL_ASST  (LVL_START_IMM < LVL_TOP ? LVL_START_IMM + 1 : LVL_START_IMM)
#define LVL_START_IMM  (LVL_GOD+1)
#define LVL_GOD  (LVL_APPROVED+1)


// Player killing options (config.c)
#define PK_NONE  NOBITS
#define PK_WAR  BIT(0)	// pk when at war
#define PK_FULL  BIT(1)	// all pk all the time
#define PK_REVENGE  BIT(2)	// pk when someone has pk'd you


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
#define REAL_UPDATES_PER_MIN  (SECS_PER_REAL_MIN / SECS_PER_REAL_UPDATE)
#define MUD_HOURS  *REAL_UPDATES_PER_MUD_HOUR


// misc game configs
#define SKILLS_PER_CLASS  2	// number of skills that makes up a combo class
#define HISTORY_SIZE  5	// Keep last 5 commands.


// System timing
#define OPT_USEC  100000	// 10 passes per second
#define PASSES_PER_SEC  (1000000 / OPT_USEC)
#define RL_SEC  * PASSES_PER_SEC


// Variables for the output buffering system
#define MAX_SOCK_BUF  (12 * 1024)	// Size of kernel's sock buf
#define MAX_PROMPT_LENGTH  256	// Max length of rendered prompt
#define GARBAGE_SPACE  32	// Space for **OVERFLOW** etc
#define SMALL_BUFSIZE  4096	// Static output buffer size
// Max amount of output that can be buffered
#define LARGE_BUFSIZE  (MAX_SOCK_BUF - GARBAGE_SPACE - MAX_PROMPT_LENGTH)


// shutdown types
#define SHUTDOWN_NORMAL  0	// comes up normally
#define SHUTDOWN_PAUSE  1	// writes a pause file which must be removed
#define SHUTDOWN_DIE  2	// kills the autorun


 //////////////////////////////////////////////////////////////////////////////
//// MOBILE DEFINES //////////////////////////////////////////////////////////

// Mobile flags 
#define MOB_BRING_A_FRIEND  BIT(0)	// a. Mob auto-assists any other (!charmed) mob
#define MOB_SENTINEL  BIT(1)	// b. Mob should not move
#define MOB_AGGRESSIVE  BIT(2)	// c. Mob hits players in the room
#define MOB_ISNPC  BIT(3)	// d. (R) Auto-set on all mobs
#define MOB_MOUNTABLE  BIT(4)	// e. Can be ridden
#define MOB_MILKABLE  BIT(5)	// f. May be milked
#define MOB_SCAVENGER  BIT(6)	// g. Eats corpses
#define MOB_UNDEAD  BIT(7)	// h. Undead :)
#define MOB_TIED  BIT(8)	// i. (R) Mob is tied up
#define MOB_ANIMAL  BIT(9)	// j. mob is an animal
#define MOB_MOUNTAINWALK  BIT(10)	// k. Walks on mountains
#define MOB_AQUATIC  BIT(11)	// l. Mob lives in the water only
#define MOB_PLURAL  BIT(12)	// m. Mob represents 2+ creatures
	#define MOB_UNUSED1  BIT(13)	// no longer used
#define MOB_SPAWNED  BIT(14)	// o. Mob was spawned and should despawn if nobody is around
#define MOB_CHAMPION  BIT(15)	// p. Mob auto-rescues its master
#define MOB_EMPIRE  BIT(16)	// q. empire NPC
#define MOB_FAMILIAR  BIT(17)	// r. familiar: special rules apply
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


// mob movement types
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


// name sets: add matching files in lib/text/names/
#define NAMES_CITIZENS  0
#define NAMES_COUNTRYFOLK  1
#define NAMES_ROMAN  2
#define NAMES_NORTHERN  3


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT DEFINES //////////////////////////////////////////////////////////

// Container flags -- limited to 31 because of int type in obj value
#define CONT_CLOSEABLE  BIT(0)	// Container can be closed
#define CONT_CLOSED  BIT(1)	// Container is closed


// Corpse flags -- limited to 31 because of int type in obj value
#define CORPSE_EATEN  BIT(0)	// The corpse is part-eaten
#define CORPSE_SKINNED  BIT(1)	// The corpse has been skinned
#define CORPSE_HUMAN  BIT(2)	// a person

// Item types
#define ITEM_UNDEFINED  0
#define ITEM_WEAPON  1	// item is a weapon
#define ITEM_WORN  2	// wearable equipment
#define ITEM_OTHER  3	// misc object
#define ITEM_CONTAINER  4	// item is a container
#define ITEM_DRINKCON  5	// item is a drink container
#define ITEM_FOOD  6	// item is food
#define ITEM_BOAT  7	// a boat
#define ITEM_PORTAL  8  // a portal
#define ITEM_BOARD  9	// message board
#define ITEM_CORPSE  10	// a corpse, pc or npc
#define ITEM_COINS  11	// stack of coins
	#define ITEM_UNUSED2  12
	#define ITEM_UNUSED3  13
#define ITEM_MAIL  14	// mail
#define ITEM_WEALTH  15	// item provides wealth
#define ITEM_CART  16	// item is a cart/wagon
#define ITEM_SHIP  17	// large ship
	#define ITEM_UNUSED4  18
	#define ITEM_UNUSED5  19
#define ITEM_MISSILE_WEAPON  20	// bow/crossbow/etc
#define ITEM_ARROW  21	// for missile weapons
#define ITEM_INSTRUMENT  22	// item is a musical instrument
#define ITEM_SHIELD  23	// item is a shield
#define ITEM_PACK  24	// increases inventory size
#define ITEM_POTION  25	// quaffable
#define ITEM_POISON  26	// poison vial
#define ITEM_ARMOR  27	// armor!
#define ITEM_BOOK  28	// tied to the book/library system


// Take/Wear flags -- where an item can be worn
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


// Some different kind of liquids for use in values of drink containers
#define LIQ_WATER  0
#define LIQ_LAGER  1
#define LIQ_WHEATBEER  2
#define LIQ_ALE  3
#define LIQ_CIDER  4
#define LIQ_MILK  5
#define LIQ_BLOOD  6
#define LIQ_HONEY  7
#define LIQ_BEAN_SOUP  8
#define LIQ_COFFEE  9
#define LIQ_GREEN_TEA  10
#define LIQ_RED_WINE  11
#define LIQ_WHITE_WINE  12
#define NUM_LIQUIDS  13	// total


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
#define MAX_MATERIALS  20	// extra space to add more later -- **DO NOT CHANGE**


// Extra object flags -- OBJ_FLAGGED(obj, f)
#define OBJ_CHAIR  BIT(0)	// a. Item can be sat upon
#define OBJ_PLANTABLE  BIT(1)	// b. Uses val 2 to set a crop type
#define OBJ_LIGHT  BIT(2)	// c. Lights until timer pops
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
#define OBJ_STAFF  BIT(16)	// q. counts as a staff
#define OBJ_UNCOLLECTED_LOOT  BIT(17)	// r. will junk instead of autostore
#define OBJ_KEEP  BIT(18)	// s. obj will not be part of any "all" commands like "drop all"
#define OBJ_TOOL_PAN  BIT(19)	// t. counts as pan
#define OBJ_TOOL_SHOVEL  BIT(20)	// u. counts as shovel
#define OBJ_NO_AUTOSTORE  BIT(21)	// v. keeps the game from cleaning it up
#define OBJ_HARD_DROP  BIT(22)	// w. dropped by a 'hard' mob
#define OBJ_GROUP_DROP  BIT(23)	// x. dropped by a 'group' mob
#define OBJ_GENERIC_DROP  BIT(24)	// y. blocks the hard/group drop flags

#define OBJ_BIND_FLAGS  (OBJ_BIND_ON_EQUIP | OBJ_BIND_ON_PICKUP)	// all bind-on flags


// custom message types
#define OBJ_CUSTOM_BUILD_TO_CHAR  0
#define OBJ_CUSTOM_BUILD_TO_ROOM  1
#define OBJ_CUSTOM_INSTRUMENT_TO_CHAR  2
#define OBJ_CUSTOM_INSTRUMENT_TO_ROOM  3
#define OBJ_CUSTOM_EAT_TO_CHAR  4
#define OBJ_CUSTOM_EAT_TO_ROOM  5


// storage flags (for obj storage locations)
#define STORAGE_WITHDRAW  BIT(0)	// requires withdraw privilege


// Character equipment positions
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
#define NUM_WEARS  22	/* This must be the # of eq positions!! */


// for item scaling based on wear flags
#define WEAR_POS_MINOR  0
#define WEAR_POS_MAJOR  1


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER DEFINES //////////////////////////////////////////////////////////

// Periodic actions -- WARNING: changing the order of these will have tragic consequences with the player file
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
#define ACT_MELTING			11
#define ACT_MANUFACTURING	12
#define ACT_CHIPPING		13
#define ACT_PANNING			14
#define ACT_MUSIC			15
#define ACT_EXCAVATING		16
#define ACT_SIRING			17
#define ACT_PICKING			18
#define ACT_MORPHING		19
#define ACT_SCRAPING		20
#define ACT_BATHING			21
#define ACT_CHANTING		22
#define ACT_PROSPECTING		23
#define ACT_FILLING_IN		24
#define ACT_RECLAIMING		25
#define ACT_ESCAPING		26
#define ACT_STUDYING		27
#define ACT_RITUAL			28
#define ACT_SAWING			29
#define ACT_QUARRYING		30
#define ACT_WEAVING			31
#define ACT_TANNING			32
#define ACT_READING			33
#define ACT_COPYING_BOOK	34
#define ACT_GEN_CRAFT		35

// act flags
#define ACT_ANYWHERE	BIT(0)


// bonus traits
#define BONUS_STRENGTH  BIT(0)
#define BONUS_DEXTERITY  BIT(1)
#define BONUS_CHARISMA  BIT(2)
#define BONUS_GREATNESS  BIT(3)
#define BONUS_INTELLIGENCE  BIT(4)
#define BONUS_WITS  BIT(5)
#define BONUS_HEALTH  BIT(6)
#define BONUS_MOVES  BIT(7)
#define BONUS_MANA  BIT(8)
#define BONUS_HEALTH_REGEN  BIT(9)
#define BONUS_MOVE_REGEN  BIT(10)
#define BONUS_MANA_REGEN  BIT(11)
#define BONUS_FAST_CHORES  BIT(12)
#define BONUS_EXTRA_DAILY_SKILLS  BIT(13)
#define BONUS_INVENTORY  BIT(14)
#define BONUS_FASTER  BIT(15)
#define NUM_BONUS_TRAITS  16


// types of channel histories -- act.comm.c
#define NO_HISTORY  -1	// reserved
#define CHANNEL_HISTORY_GOD  0
#define CHANNEL_HISTORY_TELLS  1
#define CHANNEL_HISTORY_SAY  2
#define CHANNEL_HISTORY_EMPIRE  3
#define NUM_CHANNEL_HISTORY_TYPES  4


// Modes of connectedness
#define CON_PLAYING  0	// Playing - Nominal state
#define CON_CLOSE  1	// Disconnecting
#define CON_GET_NAME  2	// By what name ..?
#define CON_NAME_CNFRM  3	// Did I get that right, x?
#define CON_PASSWORD  4	// Password:
#define CON_NEWPASSWD  5	// Give me a password for x
#define CON_CNFPASSWD  6	// Please retype password:
#define CON_QSEX  7	// Sex?
#define CON_QCOLOR  8	// Color, anyone?
#define CON_RMOTD  9	// PRESS RETURN after MOTD
#define CON_DISCONNECT  10	// In-game disconnection
#define CON_REFERRAL  11	// referral
#define CON_Q_SCREEN_READER  12  // asks if the player is blind
#define CON_QLAST_NAME  13	// Do you have a last..
#define CON_SLAST_NAME  14  // And what is it?
#define CON_CLAST_NAME  15	// Did I get that right?
#define CON_BOOKEDIT  16	// authorship
#define CON_Q_HAS_ALT  17	// Do you have an alt?
#define CON_Q_ALT_NAME  18	// Type alt's name
#define CON_Q_ALT_PASSWORD  19	// Type alt's password
#define CON_FINISH_CREATION  20	// Done!
#define CON_Q_ARCHETYPE  21	// starting skills and spells
#define CON_GOODBYE  22	// Close on <enter>
#define CON_BONUS_CREATION  23	// choose bonus trait (new character)
#define CON_BONUS_EXISTING  24	// choose bonus trait (existing char)
#define CON_PROMO_CODE  25	// promo code?
#define CON_CONFIRM_PROMO_CODE  26	// promo confirmation


// custom color types
#define CUSTOM_COLOR_EMOTE  0
#define CUSTOM_COLOR_ESAY  1
#define CUSTOM_COLOR_GSAY  2
#define CUSTOM_COLOR_OOCSAY  3
#define CUSTOM_COLOR_SAY  4
#define CUSTOM_COLOR_SLASH  5
#define CUSTOM_COLOR_TELL  6
#define CUSTOM_COLOR_STATUS  7
// up to MAX_CUSTOM_COLORS (10) - 1


// Player conditions
#define DRUNK  0
#define FULL  1
#define THIRST  2
#define NUM_CONDS  3


// Grant flags allow players to use abilities below the required access level
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
#define GRANT_AUTHORIZE  BIT(31)
#define GRANT_FORGIVE  BIT(32)
#define GRANT_HOSTILE  BIT(33)


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
#define LORE_DEATH				10
#define LORE_MAKE_VAMPIRE		11


// Morph forms
#define MORPH_NONE  0
#define MORPH_BAT  1
#define MORPH_WOLF  2
#define MORPH_HORRID_FORM  3
#define MORPH_DREAD_BLOOD  4
#define MORPH_MIST  5
#define MORPH_SAVAGE_WEREWOLF  6
#define MORPH_TOWERING_WEREWOLF  7
#define MORPH_SAGE_WEREWOLF  8
#define MORPH_DEER  9
#define MORPH_OSTRICH  10
#define MORPH_TAPIR  11
#define NUM_MORPHS  12

// flags for morphs
#define MORPH_FLAG_NO_CLAWS  BIT(0)	// can't use claws
#define MORPH_FLAG_ANIMAL  BIT(1)	// treated like an npc animal (disguise)
#define MORPH_FLAG_VAMPIRE_ONLY  BIT(2)	// requires vampire status
#define MORPH_FLAG_TEMPERATE_AFFINITY  BIT(3)	// requires temperate
#define MORPH_FLAG_ARID_AFFINITY  BIT(4)	// requires arid
#define MORPH_FLAG_TROPICAL_AFFINITY  BIT(5)	// requires tropical

// for morph data
#define MORPH_STRING_NAME  0
#define MORPH_STRING_DESC  1


// mount flags -- MOUNT_FLAGGED(ch, flag)
#define MOUNT_RIDING  BIT(0)	// player is currently mounted
#define MOUNT_AQUATIC  BIT(1)	// mount can swim
#define MOUNT_FLYING  BIT(2)	// mount can fly


/* Player flags: used by char_data.char_specials.act */
#define PLR_FROZEN		BIT(0)	/* Player is frozen						*/
#define PLR_WRITING		BIT(1)	/* Player writing (board/mail)			*/
#define PLR_MAILING		BIT(2)	/* Player is writing mail				*/
#define PLR_DONTSET		BIT(3)	/* Don't EVER set (ISNPC bit)			*/
#define PLR_SITEOK		BIT(4)	/* Player has been site-cleared			*/
#define PLR_MUTED		BIT(5)	/* Player not allowed to use pub_comm	*/
#define PLR_NOTITLE		BIT(6)	/* Player not allowed to set title		*/
#define PLR_DELETED		BIT(7)	/* Player deleted - space reusable		*/
#define PLR_LOADROOM	BIT(8)	/* Player uses nonstandard loadroom		*/
#define PLR_NOWIZLIST	BIT(9)	/* Player shouldn't be on wizlist		*/
#define PLR_NODELETE	BIT(10)	/* Player shouldn't be deleted			*/
#define PLR_INVSTART	BIT(11)	/* Player should enter game wizinvis	*/
#define PLR_IPMASK		BIT(12)	/* Player is IP-masked					*/
#define PLR_DISGUISED	BIT(13)	// Player is using a disguise
#define PLR_VAMPIRE		BIT(14)	/* Player is a vampire					*/
#define PLR_MULTIOK		BIT(15)	/* Player can multiplay					*/
#define PLR_NEEDS_NEWBIE_SETUP  BIT(16)  // player is created but needs gear and setup
#define PLR_UNRESTRICT	BIT(17)	/* !walls, !buildings					*/
#define PLR_KEEP_LAST_LOGIN_INFO  BIT(18)	// in case of players loaded into the game, does not overwrite their playtime or last login info if stored
#define PLR_EXTRACTED	BIT(19)	// for late-extraction


// Preference flags
#define PRF_AFK  BIT(0)	// player is marked away
#define PRF_COMPACT  BIT(1)	// No extra CRLF pair before prompts
#define PRF_DEAF  BIT(2)	// Can't hear shouts
#define PRF_NOTELL  BIT(3)	// Can't receive tells
#define PRF_POLITICAL  BIT(4)	// Changes map to political colors
#define PRF_RP  BIT(5)	// RP-only
#define PRF_MORTLOG  BIT(6)	// Views mortlogs, default: ON
#define PRF_NOREPEAT  BIT(7)	// No repetition of comm commands
#define PRF_HOLYLIGHT  BIT(8)	// Immortal: Can see in dark
#define PRF_COLOR  BIT(9)	// Color
#define PRF_NOWIZ  BIT(10)	// Can't hear wizline
#define PRF_NOMAPCOL  BIT(11)	// Map is not colored
#define PRF_NOHASSLE  BIT(12)	// Ignored by mobs and triggers
#define PRF_NO_IDLE_OUT  BIT(13)	// Player won't idle out
#define PRF_ROOMFLAGS  BIT(14)	// Sees vnums and flags on look
#define PRF_NO_CHANNEL_JOINS  BIT(15)	// Won't wee channel joins
#define PRF_AUTOKILL  BIT(16)	// Stops from knocking players out
#define PRF_SCROLLING  BIT(17)	// Turns off page_string
#define PRF_BRIEF  BIT(18)	// Cuts map size, removes room descs
#define PRF_BOTHERABLE  BIT(19)	// allows bite, purify, feed, etc
#define PRF_AUTORECALL  BIT(20)	// free recall when logged off too long
#define PRF_NOGODNET  BIT(21)	// Can't hear godnet
#define PRF_ALLOW_PVP  BIT(22)	// free pvp
#define PRF_INFORMATIVE  BIT(23)	// informative map display colors
#define PRF_NOSPAM  BIT(24)	// blocks periodic action messages
#define PRF_SCREEN_READER  BIT(25)	// player is visually impaired and using a screen reader that can't read the map
#define PRF_STEALTHABLE  BIT(26)	// player can steal (rather than be prevented from accidentally stealing)


// Rent codes
#define RENT_UNDEF  0
#define RENT_CRASH  1
#define RENT_RENTED  2


// group roles for classes
#define ROLE_NONE  0	// placeholder
#define ROLE_TANK  1
#define ROLE_MELEE  2
#define ROLE_CASTER  3
#define ROLE_HEALER  4
#define NUM_ROLES  5


// syslog types
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


 //////////////////////////////////////////////////////////////////////////////
//// SECTOR DEFINES //////////////////////////////////////////////////////////

// sector flags -- see constants.world.c
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
#define SECTF_COUNTS_AS_CITY  BIT(17)	// does not count as "outside" territory
#define SECTF_OBSCURE_VISION  BIT(18)	// blocks catapult, mappc
#define SECTF_IS_TRENCH  BIT(19)	// excavate-related
	#define SECTF_UNUSED1  BIT(20)
#define SECTF_ROUGH  BIT(21)	// hard terrain, requires ATR; other mountain-like properties
#define SECTF_SHALLOW_WATER  BIT(22)	// can't earthmeld; other properties like swamp and oasis have


 //////////////////////////////////////////////////////////////////////////////
//// WEATHER AND SEASON DEFINES //////////////////////////////////////////////

// Sky conditions for weather_data
#define SKY_CLOUDLESS  0
#define SKY_CLOUDY  1
#define SKY_RAINING  2
#define SKY_LIGHTNING  3


// Sun state for weather_data
#define SUN_DARK  0
#define SUN_RISE  1
#define SUN_LIGHT  2
#define SUN_SET  3


// tilesets for icons (this can be used for more than seasons; it just isn't)
#define TILESET_ANY  0	// applies to any type
#define TILESET_SPRING  1
#define TILESET_SUMMER  2
#define TILESET_AUTUMN  3
#define TILESET_WINTER  4
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
#define FORE  10
#define STARBOARD  11
#define PORT  12
#define AFT  13
#define DIR_RANDOM  14	// not a dir you can go, but used for room templates
#define NUM_OF_DIRS  15	// total number of directions


// data for climate types
#define CLIMATE_NONE  0
#define CLIMATE_TEMPERATE  1
#define CLIMATE_ARID  2
#define CLIMATE_TROPICAL  3
#define NUM_CLIMATES  4 // total


// depletion types
#define DPLTN_DIG  0
#define DPLTN_FORAGE  1
#define DPLTN_GATHER  2
#define DPLTN_PICK  3
#define DPLTN_FISH  4
#define DPLTN_QUARRY  5
#define DPLTN_PAN  6
#define DPLTN_TRAPPING  7
#define NUM_DEPLETION_TYPES  8	// total


// world evolutions
#define EVO_CHOPPED_DOWN  0	// sect it becomes when a tree is removed, [value=# of trees]: controls chopping, chant of nature, etc
#define EVO_CROP_GROWS  1	// e.g. 'seeded field' crop-grows to 'crop' [no value]
#define EVO_ADJACENT_ONE  2	// called when adjacent to at least 1 of [value=sector]
#define EVO_ADJACENT_MANY  3	// called when adjacent to at least 6 of [value=sector]
#define EVO_RANDOM  4	// checked periodically with no trigger other than percent [no value]
#define EVO_TRENCH_START  5	// sector this becomes when someone starts an excavate [no value]
#define EVO_TRENCH_FULL  6	// sector this trench becomes when it fills with water [no value]
#define EVO_NEAR_SECTOR  7	// called when within 2 tiles of [value=sector]
#define EVO_PLANTS_TO  8	// set when someone plants [no value]
#define EVO_MAGIC_GROWTH  9	// called when Chant of Nature or similar is called
#define NUM_EVOS  10	// total

// evolution value types
#define EVO_VAL_NONE  0
#define EVO_VAL_SECTOR  1
#define EVO_VAL_NUMBER  2


// Exit info (doors)
#define EX_ISDOOR  BIT(0)	// Exit is a door
#define EX_CLOSED  BIT(1)	// The door is closed


// mine types
#define MINE_NOT_SET  0	// do not change this -- 0 can't be used, as it represents an uninitialized mine. The rest are arbitrary
#define MINE_IRON  10
#define MINE_SILVER  11
#define MINE_GOLD  12
#define MINE_NOCTURNIUM  13
#define MINE_IMPERIUM  14
#define MINE_COPPER  15


// Room affects -- these are similar to room flags, but if you want to set them
// in a "permanent" way, set the room's base_affects as well as its current
// affects.
// TODO this system could be cleaned up by adding an affect_total_room that
// applies permanent affects to the room's current affects on its own.
#define ROOM_AFF_DARK  BIT(0)	// a. Torches don't work
#define ROOM_AFF_SILENT  BIT(1)	// b. Can't speak/hear
#define ROOM_AFF_HAS_INSTANCE  BIT(2)	// c. an instance is linked here
#define ROOM_AFF_CHAMELEON  BIT(3)	// d. Appears to be plains/desert
#define ROOM_AFF_TEMPORARY  BIT(4)	// e. Deleted when done (e.g. instances)
#define ROOM_AFF_NO_EVOLVE  BIT(5)	// f. Trees/etc won't grow here
#define ROOM_AFF_UNCLAIMABLE  BIT(6)	// g. Cannot be claimed
#define ROOM_AFF_PUBLIC  BIT(7)	// h. Empire allows public use
#define ROOM_AFF_DISMANTLING  BIT(8)	// i. Being dismantled
#define ROOM_AFF_NO_FLY  BIT(9)	// j. Can't fly there
#define ROOM_AFF_SHIP_PRESENT  BIT(10)	// k. A ship is present
#define ROOM_AFF_PLAYER_MADE  BIT(11)	// l. A river (or whatever) that a player created
#define ROOM_AFF_NO_WORK  BIT(12)	// m. workforce ignores this room
#define ROOM_AFF_NO_DISREPAIR  BIT(13)	// n. will not fall into disrepair
#define ROOM_AFF_NO_DISMANTLE  BIT(14)


// For various misc numbers attached to rooms
// WARNING: Make sure you have a place these are set, a place they are read,
// and *especially* a place they are removed. -pc
#define ROOM_EXTRA_MINE_TYPE  0
#define ROOM_EXTRA_MINE_AMOUNT  1
#define ROOM_EXTRA_CROP_TYPE  2
#define ROOM_EXTRA_SEED_TIME  3
#define ROOM_EXTRA_TAVERN_TYPE  4
#define ROOM_EXTRA_TAVERN_BREWING_TIME  5
#define ROOM_EXTRA_TAVERN_AVAILABLE_TIME  6
#define ROOM_EXTRA_RUINS_ICON  7
#define ROOM_EXTRA_CHOP_PROGRESS  8
#define ROOM_EXTRA_TRENCH_PROGRESS  9
#define ROOM_EXTRA_HARVEST_PROGRESS  10
#define ROOM_EXTRA_GARDEN_WORKFORCE_PROGRESS  11
#define ROOM_EXTRA_QUARRY_WORKFORCE_PROGRESS  12
#define ROOM_EXTRA_BUILD_RECIPE  13


// number of different appearances
#define NUM_RUINS_ICONS  7
#define NUM_SWAMP_DISPLAYS  4


 //////////////////////////////////////////////////////////////////////////////
//// MAXIMA AND LIMITS ///////////////////////////////////////////////////////

// misc game limits
#define BANNED_SITE_LENGTH    50	// how long ban hosts can be
#define MAX_CLASS_ABILITIES  15	// maximum abilities per class
#define MAX_CMD_LENGTH  16384	// 16k -- for trigger lengths
#define MAX_COIN  2140000000	// 2.14b (< MAX_INT)
#define MAX_COIN_TYPES  10	// don't store more than this many different coin types
#define MAX_CONDITION  750	// FULL, etc
#define MAX_EMPIRE_DESCRIPTION  2000
#define MAX_INPUT_LENGTH  1024	// Max length per *line* of input
#define MAX_INT  2147483647	// useful for bounds checking
#define MAX_INVALID_NAMES  200	// ban.c
#define MAX_ITEM_DESCRIPTION  4000
#define MAX_MESSAGES  60	// fight.c
#define MAX_MOTD_LENGTH  4000	// eedit.c, configs
#define MAX_OBJ_AFFECT  6
#define NUM_OBJ_VAL_POSITIONS  3	// GET_OBJ_VAL(obj, X) -- caution: changing this will require you to change the .obj file format
#define MAX_GROUP_SIZE  4	// how many members a group allows
#define MAX_PLAYER_DESCRIPTION  4000
#define MAX_POOF_LENGTH  80
#define MAX_RANK_LENGTH  20	// length limit
#define MAX_RANKS  20	// Max levels in an empire
#define MAX_RAW_INPUT_LENGTH  1536  // Max size of *raw* input
#define MAX_RESOURCES_REQUIRED  10	// how many resources a recipe can need
#define MAX_ROOM_DESCRIPTION  4000
#define MAX_SKILL_RESETS  10	// number of skill resets you can save up
#define MAX_STORAGE  1000000	// empire storage cap, must be < MAX_INT
#define MAX_STRING_LENGTH  8192


// limits used in char_file_u -- **DO NOT CHANGE** (without a pconvert)
#define MAX_ABILITIES  400	// just a shy ton
#define NUM_ACTION_VNUMS  3	// action vnums 0, 1, 2
#define MAX_ADMIN_NOTES_LENGTH  240
#define MAX_AFFECT  32
#define MAX_COOLDOWNS  32
#define MAX_CUSTOM_COLORS  10	// total storable custom colors
#define MAX_DISGUISED_NAME_LENGTH  32
#define MAX_HOST_LENGTH  45
#define MAX_IGNORES  15	// total number of players you can ignore
#define MAX_NAME_LENGTH  20
#define MAX_POOFIN_LENGTH  80
#define MAX_PROMPT_SIZE  120
#define MAX_PWD_LENGTH  32
#define MAX_REFERRED_BY_LENGTH  32
#define MAX_REWARDS_PER_DAY  5	//  number of times a player can be rewarded
#define MAX_SKILLS  24
#define MAX_SLASH_CHANNEL_NAME_LENGTH  16
#define MAX_SLASH_CHANNELS  20
#define MAX_TITLE_LENGTH  100
#define MAX_TITLE_LENGTH_NO_COLOR  80	// title limit without color codes (less than MAX_TITLE_LENGTH)


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

// Simple affect structure -- Used in char_file_u **DO NOT CHANGE**
struct affected_type {
	sh_int type;	// The type of spell that caused this
	sh_int duration;	// For how long its effects will last
	int modifier;	// This is added to apropriate ability
	byte location;	// Tells which ability to change - APPLY_x
	bitvector_t bitvector;	// Tells which bits to set - AFF_x

	struct affected_type *next;
};


// for sitebans
struct ban_list_element {
	char site[BANNED_SITE_LENGTH+1];
	int type;
	time_t date;
	char name[MAX_NAME_LENGTH+1];
	
	struct ban_list_element *next;
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


// generic name system
struct generic_name_data {
	int name_set;	// NAMES_x const
	int sex;	// male/female/both (each list id has multiple gender lists)
	char **names;
	int size;	// size of names array
	
	struct generic_name_data *next;	// LL
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
	struct trig_data *proto;	// for triggers... the trigger
};


// for exclusive interaction processing
struct interact_exclusion_data {
	int code;	// a-z, A-Z, or empty (stored as an int for easy-int funcs)
	int rolled_value;	// 1-10000
	int cumulative_value;	// value so far
	bool done;	// TRUE if one already passed
	UT_hash_handle hh;
};


// for the "interactions" system (like butcher, dig, etc) -- INTERACT_x
struct interaction_item {
	int type;	// INTERACT_x
	obj_vnum vnum;	// the skin object
	double percent;	// how often to do it 0.01 - 100.00
	int quantity;	// how many to give
	char exclusion_code;	// creates mutually-exclusive sets
	
	struct interaction_item *next;
};


// obj applies
struct obj_affected_type {
	byte location;	// Which ability to change (APPLY_XXX)
	sh_int modifier;	// How much it changes by
};


// for ad tracking and special promotions
struct promo_code_list {
	char *code;
	bool expired;
	PROMO_APPLY(*apply_func);
};


/* resource data */
typedef struct resource_data_struct {
	int vnum;
	int amount;
} Resource;


// for act.highsorcery.c ritual strings
struct ritual_strings {
	char *to_char;
	char *to_room;
};


// for mob spawning in sects, buildings, etc
struct spawn_info {
	mob_vnum vnum;
	double percent;	// .01 - 100.0
	bitvector_t flags;	// SPAWN_x
	
	struct spawn_info *next;
};


// for do_tedit in act.immortal.c
struct tedit_struct {
	char *cmd;
	int level;
	char **buffer;
	int size;
	char *filename;
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
	
	struct trading_post_data *next;	// LL
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
	bitvector_t flags;	// ADV_x
	int player_limit;	// maximum number of players at a time (if over 0)
	
	// lists
	struct adventure_link_rule *linking;
	
	UT_hash_handle hh;	// adventure_table hash
};


// how to link adventure zones
struct adventure_link_rule {
	int type;	// ADV_LINK_x
	bitvector_t flags;	// ADV_LINKF_x
	
	int value;	// e.g. building vnum, sector vnum to link from (by type)
	obj_vnum portal_in, portal_out;	// some types use portals
	int dir;	// some attachments take a preferred direction
	bitvector_t bld_on, bld_facing;	// for applying to the map
	
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
	
	struct exit_template *next;
};


// instance info
struct instance_data {
	any_vnum id;	// instance id
	adv_data *adventure;	// which adventure this is an instance of
	
	bitvector_t flags;	// INST_x
	room_data *location;	// map room linked from
	room_data *start;	// starting interior room (first room of zone)
	int level;	// locked, scaled level
	time_t created;	// when instantiated
	time_t last_reset;	// for reset timers
	
	// unstored data
	int size;	// size of room arrays
	room_data **room;	// array of rooms (some == NULL)
	
	struct instance_data *next;
};


// adventure zone room templates
struct room_template {
	rmt_vnum vnum;
	
	// strings
	char *title;
	char *description;
	
	// numeric data
	bitvector_t flags;	// RMT_x
	bitvector_t base_affects;	// ROOM_AFF_x
	
	// lists
	struct adventure_spawn *spawns;	// list of objs/mobs
	struct extra_descr_data *ex_description;	// extra descriptions
	struct exit_template *exits;	// list of exits
	struct interaction_item *interactions;	// interaction items
	struct trig_proto_list *proto_script;	// list of default triggers
	
	UT_hash_handle hh;	// room_template_table hash
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
	bitvector_t flags;	// BLD_x
	bld_vnum upgrades_to;	// the vnum of any building
	
	int extra_rooms;	// how many rooms it can have
	bitvector_t designate_flags;	// DES_x
	bitvector_t base_affects;	// ROOM_AFF_x
	
	int citizens;	// how many people live here
	int military;	// how much it adds to the military pool
	mob_vnum artisan_vnum;	// vnum of an artisan to load
	
	struct extra_descr_data *ex_description;	// extra descriptions
	struct spawn_info *spawns;	// linked list of spawn data
	struct interaction_item *interactions;	// interaction items
	
	UT_hash_handle hh;	// building_table hash handle
};


 //////////////////////////////////////////////////////////////////////////////
//// MOBILE STRUCTS //////////////////////////////////////////////////////////


// Specials used by NPCs, not PCs
struct mob_special_data {
	int current_scale_level;	// level the mob was scaled to, or -1 for not scaled
	int min_scale_level;	// minimum level this mob may be scaled to
	int max_scale_level;	// maximum level this mob may be scaled to
	
	int name_set;	// NAMES_x

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
	
	struct pursuit_data *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER STRUCTS //////////////////////////////////////////////////////////

// for book editing
struct bookedit_data {
	int id;
	int mode;
	bool changed;
	
	unsigned int bits;
	char *title;
	char *byline;
	char *item_name;
	char *item_description;
	
	struct paragraph_data *paragraphs;	// linked list
};


// used in descriptor for personal channel histories -- act.comm.c
struct channel_history_data {
	char *message;
	struct channel_history_data *next;
};


// player money
struct coin_data {
	int amount;	// raw value
	empire_vnum empire_id;	// which empire (or NOTHING) for misc
	time_t last_acquired;	// helps cleanup
	
	struct coin_data *next;
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


// descriptors -- the connection to the game
struct descriptor_data {
	socket_t descriptor;	// file descriptor for socket

	char host[MAX_HOST_LENGTH+1];	// hostname
	byte bad_pws;	// number of bad pw attemps this login
	byte idle_tics;	// tics idle at password prompt
	int connected;	// STATE()
	int desc_num;	// unique num assigned to desc
	time_t login_time;	// when the person connected

	char *showstr_head;	// for keeping track of an internal str
	char **showstr_vector;	// for paging through texts
	int showstr_count;	// number of pages to page through
	int showstr_page;	// which page are we currently showing?

	char **str;	// for the modify-str system
	char *backstr;	// for the modify-str aborts
	char *file_storage;	// name of where to save a file
	bool straight_to_editor;	// if true, commands go straight to the string editor
	size_t max_str;	// max length of editor
	int mail_to;	// name for mail system
	int notes_id;	// idnum of player for notes-editing
	
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
	char last_color_sent;  // the last color code that actually transmitted

	char_data *character;	// linked to char
	char_data *original;	// original char if switched
	char_data *temp_char;	// for char creation
	descriptor_data *snooping;	// Who is this char snooping
	descriptor_data *snoop_by;	// And who is snooping this char

	char *last_act_message;	// stores the last thing act() sent to this desc
	struct channel_history_data *channel_history[NUM_CHANNEL_HISTORY_TYPES];	// histories
	
	struct bookedit_data *bookedit;	// for book editing
	
	// olc
	int olc_type;	// OLC_OBJECT, etc -- only when an editor is open
	char *olc_storage;	// a character buffer created and used by some olc modes
	any_vnum olc_vnum;	// vnum being edited
	
	adv_data *olc_adventure;	// adv being edited
	obj_data *olc_object;	// item being edited
	char_data *olc_mobile;	// mobile being edited
	craft_data *olc_craft;	// craft recipe being edited
	bld_data *olc_building;	// building being edited
	crop_data *olc_crop;	// crop being edited
	room_template *olc_room_template;	// rmt being edited
	struct sector_data *olc_sector;	// sector being edited
	struct trig_data *olc_trigger;	// trigger being edited
	
	// linked list
	descriptor_data *next;
};


// basic paragraphs for book_data
struct paragraph_data {
	char *text;
	struct paragraph_data *next;
};


// used in player_special_data_saved **DO NOT CHANGE** or you could corrupt the playerfile
struct player_ability_data {
	bool purchased;	// whether or not the player bought it
	byte levels_gained;	// tracks for the cap on how many times one ability grants skillups
};


// for the player index
struct player_index_element {
	char *name;
	int id;
};


// used in player_special_data_saved **DO NOT CHANGE** or you could corrupt the playerfile
struct player_skill_data {
	byte level;	// current skill level (0-100)
	double exp;	// experience gained in skill (0-100)
	byte resets;	// resets available
	bool noskill;	// if TRUE, do not gain
};


// channels a player is on
struct player_slash_channel {
	int id;
	struct channel_history_data *history;
	struct player_slash_channel *next;
};


/*
 *  **DO NOT CHANGE**
 *  If you want to add new values to the playerfile, do it here.  DO NOT
 * ADD, DELETE OR MOVE ANY OF THE VARIABLES - doing so will change the
 * size of the structure and ruin the playerfile.  However, you can change
 * the names of the spares to something more meaningful, and then use them
 * in your new code.  They will automatically be transferred from the
 * playerfile into memory when players log in.
 */
struct player_special_data_saved {
	// account and player stuff
	int account_id;	// all characters with the same account_id belong to the same player
	char creation_host[MAX_HOST_LENGTH+1];	// host they created from
	char referred_by[MAX_REFERRED_BY_LENGTH];	// as entered in character creation
	char admin_notes[MAX_ADMIN_NOTES_LENGTH];	// for admin to leave notes about a player
	byte invis_level;	// level of invisibility
	byte immortal_level;	// stored so that if level numbers are changed, imms stay at the correct level
	bitvector_t grants;	// grant imm abilities
	bitvector_t syslogs;	// which syslogs people want to see
	bitvector_t bonus_traits;	// BONUS_x
	ubyte bad_pws;	// number of bad password attemps

	// preferences
	bitvector_t pref;	// preference flags for PCs.
	int last_tip;	// for display_tip_to_character
	byte mapsize;	// how big the player likes the map

	// empire
	empire_vnum pledge;	// Empire he's applying to
	empire_vnum empire;	// The empire this player follows (when stored to file; live players use ch->loyalty)
	byte rank;	// Rank in the empire
	
	// misc player attributes
	ubyte apparent_age;	// for vampires	
	sh_int conditions[NUM_CONDS];	// Drunk, full, thirsty
	int resources[MAX_MATERIALS];	// God resources
	int ignore_list[MAX_IGNORES];	// players who can't message you
	char slash_channels[MAX_SLASH_CHANNELS][MAX_SLASH_CHANNEL_NAME_LENGTH];	// for storing /channels
	char custom_colors[MAX_CUSTOM_COLORS];	// for custom channel coloring, storing the letter part of the & code ('r' for &r)

	// some daily stuff
	int daily_cycle;	// Last update cycle registered
	ubyte daily_bonus_experience;	// boosted skill gain points
	int rewarded_today[MAX_REWARDS_PER_DAY];	// idnums, for ABIL_REWARD

	// action info
	int action;	// ACT_x
	int action_rotation;	// used to keep all player actions from happening on the same tick
	int action_timer;	// ticks to completion (use varies)
	room_vnum action_room;	// player location
	int action_vnum[NUM_ACTION_VNUMS];	// slots for storing action data (use varies)
	
	// locations and movement
	room_vnum load_room;	// Which room to place char in
	room_vnum load_room_check;	// this is the home room of the player's loadroom, used to check that they're still in the right place
	room_vnum last_room;	// useful when dead
	room_vnum tomb_room;	// location of player's chosen tomb
	byte last_direction;	// used for walls and movement
	int recent_death_count;	// for death penalty
	long last_death_time;	// for death counts
	int last_corpse_id;	// DG Scripts obj id of last corpse
	
	// olc data
	any_vnum olc_min_vnum;	// low range of olc permissions
	any_vnum olc_max_vnum;	// high range of olc permissions
	bitvector_t olc_flags;	// olc permissions
	
	// skill/ability data
	byte creation_archetype;	// this is now stored permanently so later decisions can be made based on it
	struct player_skill_data skills[MAX_SKILLS];
	struct player_ability_data abilities[MAX_ABILITIES];
	bool can_gain_new_skills;	// not required to keep skills at zero
	bool can_get_bonus_skills;	// can buy extra 75's
	sh_int skill_level;  // levels computed based on class skills
	sh_int last_known_level;	// for getting level of offline characters
	ubyte class_progression;	// % of the way from SPECIALTY_SKILL_CAP to CLASS_SKILL_CAP
	ubyte class_role;	// ROLE_x chosen by the player
	sh_int character_class;  // character's class as determined by top skills
	
	// tracking for specific skills
	byte confused_dir;  // people without Navigation think this dir is north
	char disguised_name[MAX_DISGUISED_NAME_LENGTH];	// verbatim copy of name -- grabs custom mob names and empire names
	byte disguised_sex;	// sex of the mob you're disguised as
	sh_int morph;	// MORPH_x form
	bitvector_t mount_flags;	// flags for the stored mount
	mob_vnum mount_vnum;	// stored mount
	byte using_poison;	// poison preference for Stealth

	/* spares below for future expansion.  You can change the names from
	   'sparen' to something meaningful, but don't change the order.  */

	byte spare0;
	byte spare1;
	byte spare2;
	byte spare3;
	byte promo_id;	// entry in the promo_codes table -- TODO move up to an int in beta2, and remove int casts in various places
	
	ubyte spare5;
	ubyte spare6;
	ubyte spare7;
	ubyte spare8;
	ubyte spare9;
	
	sh_int spare10;
	sh_int spare11;
	sh_int spare12;
	sh_int spare13;
	sh_int spare14;
	
	int spare15;
	int spare16;
	int health_deficit;	// TODO in beta2, move these to an array with the pools
	int move_deficit;
	int mana_deficit;
	
	double spare20;
	double spare21;
	double spare22;
	double spare23;
	double spare24;
	
	bitvector_t spare25; 
	bitvector_t spare26;
	bitvector_t spare27;
	bitvector_t spare28;
	bitvector_t spare29;
	
	// these are initialized to NOTHING/-1
	any_vnum spare30;
	any_vnum spare31;
	any_vnum spare32;
	any_vnum spare33;
	any_vnum spare34;
};


/*
 * Specials needed only by PCs, not NPCs.  Space for this structure is
 * not allocated in memory for NPCs, but it is for PCs and the portion
 * of it labelled 'saved' is saved in the playerfile.  This structure can
 * be changed freely; beware, though, that changing the contents of
 * player_special_data_saved will corrupt the playerfile.
 */
struct player_special_data {
	struct player_special_data_saved saved;
	
	double gear_level;	// total of rate_item() on equipment
	struct coin_data *coins;	// linked list of coin data
	
	char *lastname;	// Last name
	char *title;	// shown on 'who'/'whois'
	char *prompt;	// custom prompt
	char *fight_prompt;	// fight prompt
	char *poofin;	// shown when immortal appears
	char *poofout;	// shown when immortal disappears

	struct alias_data *aliases;	// Character's aliases
	int last_tell;	// idnum of last tell from

	struct player_slash_channel *slash_channels;	// channels the player is on
	byte reboot_conf;	// Reboot confirmation

	byte create_points;	// Used in character creation
	int create_alt_id;	// used in CON_Q_ALT_NAME and CON_Q_ALT_PASSWORD
	
	bool restore_on_login;	// mark the player to trigger a free reset when they enter the game
	bool reread_empire_tech_on_login;	// mark the player to trigger empire tech re-read on entering the game
	
	room_vnum marked_location;	// for map marking
	
	int group_invite_by;	// idnum of the last player to invite this one
	
	// for late-accept resurrection
	room_vnum resurrect_location;	// where the player is being resurrected (vnum so we don't have to look for it on room delete)
	int resurrect_by;	// idnum of player who cast it, for skillups
	int resurrect_ability;	// which ability was used
	time_t resurrect_time;	// time of last rez offer
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
	char passwd[MAX_PWD_LENGTH+1];	// character's password
	char *name;	// PC name / NPC keyword (kill ... )
	char *short_descr;	// for NPC string-building
	char *long_descr;	// for 'look' on mobs or 'look at' on players
	byte sex;	// PC / NPC sex
	byte access_level;	// PC access level -- LVL_x -- not to be confused with GET_COMPUTED_LEVEL() or get_approximate_level()
	struct time_data time;	// PC's age

	struct lore_data *lore;	// player achievements
};


// Char's points.  Used in char_file_u -- **DO NOT CHANGE**
// everything in here has a default value that it's always reset to, and only
// gear and affects can raise them above it; character can't gain them
// permanently
struct char_point_data {
	int current_pools[NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	int max_pools[NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	
	int extra_attributes[TOTAL_EXTRA_ATTRIBUTES];	// ATT_x (dodge, etc)
};


/* 
 * char_special_data_saved: specials which both a PC and an NPC have in
 * common, but which must be saved to the playerfile for PC's.
 *
 * ***WARNING***  Do not change this structure.  Doing so will ruin the
 * playerfile.  If you want to add to the playerfile, use the spares
 * in player_special_data.
 */
struct char_special_data_saved {
	int idnum;	// player's idnum; -1 for mobiles
	bitvector_t act;	// mob flag for NPCs; player flag for PCs

	bitvector_t injuries;	// Bitvectors including damage to the player

	bitvector_t affected_by;	// Bitvector for spells/skills affected by
};


// for fighting
struct fight_data {
	char_data *victim;	// Actual victim
	byte mode;	// Fight mode, FMODE_x
	byte wait;	// Time to intercept
};


// Special playing constants shared by PCs and NPCs which aren't in pfile
struct char_special_data {
	obj_data *chair;	// Object that this char's sitting in

	struct fight_data fighting;	// Opponent
	char_data *hunting;	// Char hunted by this char

	char_data *feeding_from;	// Who person is biting
	char_data *fed_on_by;	// Who is biting person

	char_data *led_by;	// A person may lead a mob
	char_data *leading;	// A mob may lead a person

	obj_data *pulling;	// The mob may be pulling something
	
	struct empire_npc_data *empire_npc;	// if this is an empire spawn

	byte position;	// Standing, fighting, sleeping, etc.
	
	int health_regen;	// Hit regeneration add
	int move_regen;	// Move regeneration add
	int mana_regen;	// mana regen add

	int carry_items;	// Number of items carried
	int	timer;	// Timer for update

	struct char_special_data_saved saved;	// constants saved in plrfile
};


// main character data for PC/NPC in-game
struct char_data {
	int pfilepos;	// playerfile pos
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

	int id;	// used by DG triggers - unique id
	struct trig_proto_list *proto_script;	// list of default triggers
	struct script_data *script;	// script info for the object
	struct script_memory *memory;	// for mob memory triggers

	char_data *next_in_room;	// For room->people - list
	char_data *next;	// For either monster or ppl-list
	char_data *next_fighting;	// For fighting list
	
	struct follow_type *followers;	// List of chars followers
	char_data *master;	// Who is char following?
	struct group_data *group;	// Character's Group

	char prev_host[MAX_HOST_LENGTH+1];	// Previous host (they're Trills)
	time_t prev_logon;	// Time (in secs) of prev logon
	
	UT_hash_handle hh;	// mobile_table
};


// used in char_file_u **DO NOT CHANGE**
struct cooldown_data {
	sh_int type;	// any COOLDOWN_x const
	time_t expire_time;	// time at which the cooldown has expired
	
	struct cooldown_data *next;	// linked list
};


// for damage-over-time (DoTs) Used in char_file_u **DO NOT CHANGE**
struct over_time_effect_type {
	sh_int type;	// ATYPE_x
	sh_int duration;	// time in 5-second real-updates
	sh_int damage_type;	// DAM_x type
	sh_int damage;	// amount
	sh_int stack;	// damage is multiplied by this
	sh_int max_stack;	// how high it's allowed to stack

	struct over_time_effect_type *next;
};


// ***BEWARE*** Changing it will ruin the playerfile
struct char_file_u {
	char name[MAX_NAME_LENGTH+1];	// first name
	char lastname[MAX_NAME_LENGTH+1];	// last name
	char title[MAX_TITLE_LENGTH+1];	// character's title
	char description[MAX_PLAYER_DESCRIPTION];	// player's manual description
	char prompt[MAX_PROMPT_SIZE+1];	// character's prompt
	char fight_prompt[MAX_PROMPT_SIZE+1];	// character's prompt for combat
	char poofin[MAX_POOFIN_LENGTH+1];	// message when appearing
	char poofout[MAX_POOFIN_LENGTH+1];	// message when disappearing
	byte sex;	// sex (yes, please)
	byte access_level;	// character's level
	time_t birth;	// time of birth of character
	int	played;	// number of secs played in total

	char pwd[MAX_PWD_LENGTH+1];	// character's password

	struct char_special_data_saved char_specials_saved;
	struct player_special_data_saved player_specials_saved;
	sh_int attributes[NUM_ATTRIBUTES];	// str, etc (natural)
	struct char_point_data points;
	struct affected_type affected[MAX_AFFECT];
	struct over_time_effect_type over_time_effects[MAX_AFFECT];
	struct cooldown_data cooldowns[MAX_COOLDOWNS];

	time_t last_logon;	// time (in secs) of last logon
	char host[MAX_HOST_LENGTH+1];	// host of last logon
};


// Structure used for chars following other chars
struct follow_type {
	char_data *follower;
	struct follow_type *next;
};


// For a person's lore
struct lore_data {
	int type;	// LORE_x
	int value;	// empire id or other id type
	long date;	// when it was acquired (timestamp)

	struct lore_data *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// CRAFT STRUCTS ///////////////////////////////////////////////////////////

// main struct for the crafting code
struct craft_data {
	craft_vnum vnum;	// vnum of this recipe
	
	int type;	// CRAFT_TYPE_x
	int ability;	// NO_ABIL for none, otherwise ABIL_x
	
	char *name;
	obj_vnum object;	// vnum of the object it makes, or liquid id if CRAFT_SOUP
	int quantity;	// makes X
	
	bitvector_t flags;	// CRAFT_x
	int time;	// how many action ticks it takes
	
	// for buildings:
	any_vnum build_type;	// a building vnum (maybe something else too?)
	bitvector_t build_on;	// BLD_ON_x flags for the tile it's built upon
	bitvector_t build_facing;	// BLD_ON_x flags for the tile it's facing
	
	obj_vnum requires_obj;	// only shows up if you have the item, e.g. o_TENT
	Resource *resources;
	
	UT_hash_handle hh;	// craft_table hash
	UT_hash_handle sorted_hh;	// sorted_crafts hash
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
	
	int climate;	// CLIMATE_x
	bitvector_t flags;	// CROPF_x flags
	
	// only spawns where:
	int x_min;	// x >= this
	int x_max;	// x <= this
	int y_min;	// y >= this
	int y_max;	// y <= this
	
	struct spawn_info *spawns;	// mob spawn data
	struct interaction_item *interactions;	// interaction items
	
	UT_hash_handle hh;	// crop_table hash
};


 //////////////////////////////////////////////////////////////////////////////
//// DATA STRUCTS ////////////////////////////////////////////////////////////

// for action messages
struct action_data_struct {
	char *name;	// shown e.g. in sentences or prompt ("digging")
	char *long_desc;	// shown in room (action description)
	void (*process_function)(char_data *ch);	// called on ticks (may be NULL)
	void (*cancel_function)(char_data *ch);	// called when the action is cancelled early (may be NULL)
};


// For new players -- starting selections
struct archetype_type {
	char *name;
	char *description;
	
	char *starting_rank[NUM_GENDERS];	// rank names for default empires
	
	int primary_skill;	// SKILL_x or NO_SKILL
	int primary_skill_level;	// 0-100
	
	int secondary_skill;	// SKILL_x or NO_SKILL
	int secondary_skill_level;	// 0-100

	int attributes[NUM_ATTRIBUTES];	// starting attributes
	
	struct {
		int wear;	// WEAR_x location for newbie gear
		obj_vnum vnum;	// vnum of object
	} gear[10];	// newbie gear -- MUST terminate with a vnum=NOTHING
};


// STRENGTH, NUM_ATTRIBUTES, etc
struct attribute_data_type {
	char *name;
	char *creation_description;	// shown if players need help during creation
};


// used for city metadata
struct city_metadata_type {
	char *name;
	char *icon;
	int radius;
	int max_population;
};


// ROLE_x
struct class_role_data_type {
	int role;	// ROLE_x const
	int ability[MAX_CLASS_ABILITIES];	// list of abilities for this role
};


// CLASS_x
struct class_data_type {
	char *name;
	char *abbrev;	// 4-letter
	int skills[SKILLS_PER_CLASS];	// skills which make up this class
	int max_pools[NUM_POOLS];	// health, move, mana, blood (blood is not used here)
	struct class_role_data_type role[NUM_ROLES];	// various class roles
};


// chore type data -- CHORE_x
struct empire_chore_type {
	char *name;
	mob_vnum mob;
};


// for do_fish
struct fishing_data_type {
	int vnum;
	double chance;
};


// MAT_x
struct material_data {
	char *name;
	bool floats;
};


// mining data
struct mine_data_type {
	int type;
	char *name;
	obj_vnum vnum;
	int min_amount;	// initial mine value
	int max_amount;	//		min-max
	double chance;	// chance a mine is this, out of 100.00; -1 is special chance
	int ability;	// if != NO_ABIL, requires this ability to find
};


// see act.stealth.c
struct poison_data_type {
	char *name;
	int ability;
	
	int atype;	// ATYPE_x
	int apply;	// APPLY_x
	int mod;	// +/- value
	bitvector_t aff;
	
	// dot affect
	int dot_type;	// ATYPE_x, -1 for none
	int dot_duration;	// time for the dot
	int dot_damage_type;	// DAM_x for the dot
	int dot_damage;	// damage for the dot
	int dot_max_stacks;	// how high the dot can stack
	
	int special;	// special poison procedure
	bool allow_stack;	// whether or not it can stack
};


// see act.naturalmagic.c
struct potion_data_type {
	char *name;	// name for olc, etc
	int atype;	// ATYPE_x
	int apply;	// APPLY_x
	bitvector_t aff;
	int spec;	// POTION_SPEC_x
};


// ships.c ship_data[]
struct ship_data_struct {
	char *name;
	int vnum;
	int type;
	int ability;	// NO_ABIL or ABIL_x
	int resources;
	int advanced;
};


// skills.c
struct smelt_data_type {
	obj_vnum from;
	int from_amt;
	
	obj_vnum to;
	int to_amt;
	
	bool workforce;
};


// tanning data
struct tanning_data_type {
	obj_vnum from;
	obj_vnum to;
};


// for BREW_x
struct tavern_data_type {
	char *name;
	int liquid;
	int ingredients[3];
};


// for do_toggle
struct toggle_data_type {
	char *name;	// toggle display and subcommand
	int type;	// TOG_ONOFF, TOG_OFFON
	bitvector_t bit;	// PRF_x
	int level;	// required level to see/use toggle
	void (*notify_func)(char_data *ch);	// optional function to alert changes
};


// WEAR_x data for each equipment slot
struct wear_data_type {
	char *eq_prompt;	// shown on 'eq' list
	bitvector_t item_wear;	// matching ITEM_WEAR_x
	bool count_stats;	// FALSE means it's a slot like in-sheath, and adds nothing to the character
	int cascade_pos;	// for ring 1 -> ring 2; use NO_WEAR if it doesn't cascade
	char *already_wearing;	// error message when slot is full
	char *wear_msg_to_room;	// msg act()'d to room on wear
	char *wear_msg_to_char;	// msg act()'d to char on wear
};


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE STRUCTS //////////////////////////////////////////////////////////


// matches up to city_type
struct empire_city_data {
	char *name;
	int type;
	int population;
	int military;
	
	room_data *location;
	bitvector_t traits;	// ETRAIT_x
	
	struct empire_city_data *next;
};


struct empire_log_data {
	int type;	// ELOG_x
	time_t timestamp;
	char *string;
	
	struct empire_log_data *next;
};


// data for keeping npcs' names and data for empires
struct empire_npc_data {
	mob_vnum vnum;	// npc type
	int sex;	// SEX_x
	int name;	// position in the name list
	room_data *home;	// where it lives
	
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


/* The storage structure for empires */
struct empire_storage_data {
	obj_vnum vnum;	// what's stored
	int amount;	// how much
	int island;	// which island it's stored on

	struct empire_storage_data *next;
};


// list of rooms and buildings owned
struct empire_territory_data {
	room_data *room;	// pointer to territory location
	int population_timer;	// time to re-populate
	
	struct empire_npc_data *npcs;	// list of empire mobs that live here
	
	bool marked;	// for checking that rooms still exist
	
	struct empire_territory_data *next;	// linked list
};


// for empire trades
struct empire_trade_data {
	int type;	// TRADE_x
	obj_vnum vnum;	// item type
	int limit;	// min (export), max (import)
	int cost;	// min (export), max (import)
	
	struct empire_trade_data *next;
};


// for storing scaled, enchanted, or superior items
struct empire_unique_storage {
	obj_data *obj;	// actual live object
	int amount;	// stacking
	sh_int flags;	// up to 15 flags, EUS_x
	int island;	// split by islands
	
	struct empire_unique_storage *next;
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

	byte num_ranks;	// Total number of levels (maximum 20)
	char *rank[MAX_RANKS];	// Name of each rank
	
	bitvector_t frontier_traits;	// ETRAIT_x
	int coins;	// total coins (always in local currency)

	byte priv[NUM_PRIVILEGES];	// The rank at which you can use a command

	// linked lists
	struct empire_political_data *diplomacy;
	struct empire_storage_data *store;
	struct empire_unique_storage *unique_store;	// LL: eus->next
	struct empire_trade_data *trade;
	struct empire_log_data *logs;
	
	// unsaved data
	struct empire_territory_data *territory_list;	// linked list of buildings/rooms
	struct empire_city_data *city_list;	// linked list of cities
	
	// unsaved data
	int city_terr;	// total territory IN cities
	int outside_terr;	// total territory OUTSIDE cities

	/* Unsaved data */
	int wealth;	// computed by read_vault
	int population;	// npc population who lives here
	int military;	// number of soldiers
	int greatness;	// total greatness of members
	int tech[NUM_TECHS];	// TECH_x, detected from buildings and abilities
	int **island_tech;	// array of TECH_x by island
	int size_island_tech;	// size of the island_tech array last time it was created
	int members;	// Number of members, calculated at boot time
	int total_member_count;	// Total number of members including timeouts and dupes
	int total_playtime;	// total playtime among all accounts, in hours
	bool imm_only;	// Don't allow imms/morts to be together
	int fame;	// Empire's fame rating
	time_t last_logon;	// time of last member's last logon
	bool chore_active[NUM_CHORES];	// which chores the empire should be doing
	int scores[NUM_SCORES];	// empire score in each category
	int sort_value;	// for score ties
	bool storage_loaded;	// record whether or not storage has been loaded, to prevent saving over it
	
	UT_hash_handle hh;	// empire_table hash handle
};


 //////////////////////////////////////////////////////////////////////////////
//// FIGHT STRUCTS ///////////////////////////////////////////////////////////

// part of fight messages
struct msg_type {
	char *attacker_msg;	// message to attacker
	char *victim_msg;	// message to victim
	char *room_msg;	// message to room
};


// part of fight messages
struct message_type {
	struct msg_type die_msg;	// messages when death
	struct msg_type miss_msg;	// messages when miss
	struct msg_type hit_msg;	// messages when hit
	struct msg_type god_msg;	// messages when hit on god
	struct message_type *next;	// to next messages of this kind
};


// fight message list
struct message_list {
	int a_type;	// Attack type
	int number_of_attacks;	// How many attack messages to chose from
	struct message_type *msg;	// List of messages
};


 //////////////////////////////////////////////////////////////////////////////
//// GAME STRUCTS ////////////////////////////////////////////////////////////

// For reboots/shutdowns
struct reboot_control_data {
	int type;	// SCMD_REBOOT, SCMD_SHUTDOWN
	int time;	// minutes
	int level;	// REBOOT_NORMAL, PAUSE, DIE
	int immediate;	// TRUE/FALSE for instant reboot
};


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT STRUCTS //////////////////////////////////////////////////////////

// used for binding objects to players
struct obj_binding {
	int idnum;	// player id
	struct obj_binding *next;	// linked list
};


// object numeric data -- part of obj_data
struct obj_flag_data {
	int value[NUM_OBJ_VAL_POSITIONS];	// Values of the item (see list)
	byte type_flag;	// Type of item
	bitvector_t wear_flags;	// Where you can wear it
	bitvector_t extra_flags;	// If it hums, glows, etc.
	int carrying_n;	// number of items inside
	int cost;	// Value when sold (gp.)
	int timer;	// Timer for object
	bitvector_t bitvector;	// To set chars bits
	int material;	// Material this is made out of
	
	int current_scale_level;	// level the obj was scaled to, or -1 for not scaled
	int min_scale_level;	// minimum level this obj may be scaled to
	int max_scale_level;	// maximum level this obj may be scaled to
};


// a game item
struct obj_data {
	obj_vnum vnum;	// object's virtual number
	room_data *in_room;	// In what room -- NULL when container/carried

	struct obj_flag_data obj_flags;	// Object information
	struct obj_affected_type affected[MAX_OBJ_AFFECT];	// affects

	char *name;	// Title of object: get, etc.
	char *description;	// When in room (long desc)
	char *short_description;	// sentence-building; when worn/carry/in cont.
	char *action_description;	// What to write when looked at
	struct extra_descr_data *ex_description;	// extra descriptions
	struct obj_custom_message *custom_msgs;	// any custom messages
	char_data *carried_by;	// Carried by NULL in room/container
	char_data *worn_by;	// Worn by?
	sh_int worn_on;	// Worn where?
	
	empire_vnum last_empire_id;	// id of the last empire to have this
	int last_owner_id;	// last person to have the item
	time_t stolen_timer;	// when the object was last stolen
	
	struct interaction_item *interactions;	// interaction items
	struct obj_storage_type *storage;	// linked list of where an obj can be stored
	time_t autostore_timer;	// how long an object has been where it be

	char_data *sitting;	// Person in the chair
	char_data *pulled_by1;	// Animal pulling this obj
	char_data *pulled_by2;	// Up to two animals may be needed
	struct obj_binding *bound_to;	// LL of who it's bound to

	obj_data *in_obj;	// In what object NULL when none
	obj_data *contains;	// Contains objects

	int id;	// used by DG triggers - unique id
	struct trig_proto_list *proto_script;	// list of default triggers
	struct script_data *script;	// script info for the object

	obj_data *next_content;	// For 'contains' lists
	obj_data *next;	// For the object list
	
	UT_hash_handle hh;	// object_table hash
};


// for object custom action messages
struct obj_custom_message {
	int type;	// OBJ_CUSTOM_x
	char *msg;
	
	struct obj_custom_message *next;
};


// for where an item can be stored
struct obj_storage_type {
	bld_vnum building_type;	// building vnum
	int flags;	// STORAGE_x
	
	struct obj_storage_type *next;
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
	int climate;	// CLIMATE_x
	bitvector_t flags;	// SECTF_x flags
	bitvector_t build_flags;	// matches up with craft_data.build_on and .build_facing
	
	struct spawn_info *spawns;	// mob spawn data
	struct evolution_data *evolution;	// change over time
	struct interaction_item *interactions;	// interaction items
	
	UT_hash_handle hh;	// sector_table hash
};


// for sector_data, to describe how a tile changes over time
struct evolution_data {
	int type;	// EVO_x
	int value;	// used by some types, e.g. # of adjacent forests
	double percent;	// chance of happening per zone update
	sector_vnum becomes;	// sector to transform to
	
	struct evolution_data *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// TRIGGER STRUCTS /////////////////////////////////////////////////////////

// linked list for mob/object prototype trigger lists
struct trig_proto_list {
	trig_vnum vnum;	/* vnum of the trigger   */
	struct trig_proto_list *next;
};


 //////////////////////////////////////////////////////////////////////////////
//// WEATHER AND SEASON STRUCTS //////////////////////////////////////////////

// TILESET_x, used in sector_data
struct tileset_data {
	char *icon;  // ^^^^
	char *color;  // e.g. &g
};


struct weather_data {
	int pressure;	// How is the pressure ( Mb )
	int change;	// How fast and what way does it change
	int sky;	// How is the sky
	int sunlight;	// And how much sun
};


 //////////////////////////////////////////////////////////////////////////////
//// WORLD STRUCTS ///////////////////////////////////////////////////////////


// init_room(), delete_room(), disassociate_building()
struct room_data {
	room_vnum vnum; // room number (vnum)
	
	empire_data *owner;  // who owns this territory
	
	sector_data *sector_type;  // terrain type -- saved in file as vnum
	sector_data *original_sector;  // for when built-over -- ^
	int island;	// island numbers
	
	struct complex_room_data *complex; // for rooms that are buildings, inside, adventures, etc
	byte light;  // number of light sources
	
	struct depletion_data *depletion;	// resource depletion

	// custom data
	char *name;  // room name may be set
	char *description;  // so may a description
	char *icon;  // same with map icon

	struct affected_type *af;  // room affects
	sh_int affects;  // affect bitvector (modified)
	sh_int base_affects;  // base affects
	
	struct track_data *tracks;	// for tracking
	
	time_t last_spawn_time;  // used to spawn npcs
	
	struct room_extra_data *extra_data;	// misc storage

	struct trig_proto_list *proto_script;	/* list of default triggers  */
	struct script_data *script;	/* script info for the room           */

	obj_data *contents;  // start of item list
	char_data *people;  // start of people list
	
	struct reset_com *reset_commands;	// used only during startup
	
	UT_hash_handle world_hh;	// hash handle for world_table
	UT_hash_handle interior_hh;	// hash handle for interior_world_table
};


// resources left to build/dismantle a room (in complex_room_data)
struct building_resource_type {
	obj_vnum vnum;
	int amount;
	
	struct building_resource_type *next;
};


// data used only for rooms that are map buildings, interiors, or adventures
struct complex_room_data {
	struct room_direction_data *exits;  // directions
	
	struct building_resource_type *to_build;  // for incomplete/dismantled buildings
	
	// pointers to the room's building or room template data, IF ANY
	bld_data *bld_ptr;	// point to building_table proto
	room_template *rmt_ptr;	// points to room_template_table proto
	
	byte entrance;  // direction of entrance
	int patron;  // for shrine gods
	byte inside_rooms;  // count of designated rooms inside
	room_data *home_room;  // for interior rooms (and boats and instances), means this is actually part of another room; is saved as vnum to file but is room_data* in real life
	
	int disrepair;	// decay over time
	
	obj_data *boat;  // if in a boat, this is the boat obj
	struct instance_data *instance;	// if part of an instantiated adventure
	
	int private_owner;	// for privately-owned houses
	
	byte burning;  // if burning, the burn value
	byte damage;  // for catapulting
};


// for resource depletion
struct depletion_data {
	int type;
	int count;
	
	struct depletion_data *next;
};


// structure for the reset commands
struct reset_com {
	char command;	// current command

	int arg1;
	int arg2;	// Arguments to the command
	int arg3;

	char *sarg1;	// string argument
	char *sarg2;	// string argument
	
	struct reset_com *next;
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


// for storing misc vals to the room
struct room_extra_data {
	int type;	// ROOM_EXTRA_x
	int value;
	
	struct room_extra_data *next;
};


// for tracking
struct track_data {
	int player_id;	// player or NOTHING
	mob_vnum mob_num;	// mob or NOTHING
	
	time_t timestamp;	// when
	byte dir;	// which way
	
	struct track_data *next;
};
