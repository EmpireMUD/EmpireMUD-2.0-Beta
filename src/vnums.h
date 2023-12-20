/* ************************************************************************
*   File: vnums.h                                         EmpireMUD 2.0b5 *
*  Usage: stores commonly-used virtual numbers                            *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

// NOTE: These numbers match things created with the OLC editors and stored
// in data files. When you add a definition in this file, you must first find
// a free vnum in your EmpireMUD, and create the item/building/etc.

// Note on VNUMS:
// You do NOT need to be sequential with other numbers in this file. You should
// almost always add new content in the vnum range of your own adventures, or 
// pick a high starting vnum to avoid conflicting with stock content.


 //////////////////////////////////////////////////////////////////////////////
//// BUILDINGS ///////////////////////////////////////////////////////////////

#define BUILDING_TUNNEL  5008  // building.c
#define BUILDING_CITY_CENTER  5009

// Room building vnums
#define RTYPE_SHIP_HOLDING_PEN  5509	// for the shipping system's storage room
#define RTYPE_EXTRACTION_PIT  5523	// a place to keep chars when they are pending extraction

#define RTYPE_TUNNEL  5612	// for mine complex, tunnel


 //////////////////////////////////////////////////////////////////////////////
//// OBJECTS /////////////////////////////////////////////////////////////////

// these consts can be removed as soon as their hard-coded functions are gone!

/* Boards */
#define BOARD_MORT  1
#define BOARD_IMM  2

// Natural resources
#define o_LIGHTNING_STONE  103
#define o_BLOODSTONE  104
#define o_WHEAT  141
#define o_HOPS  143
#define o_BARLEY  145

// brewing items
#define o_APPLES  3002
#define o_PEACHES  3004
#define o_CORN  3005

// Wood crafts
#define o_STAKE  915	// could be a flag
#define o_BLANK_SIGN  918

// core objects
#define o_CORPSE  1000

// Skill tree items
#define o_PORTAL  1100  // could probably safely generate a NOTHING item

// herbs
#define o_IRIDESCENT_IRIS  1206

// misc stuff
#define o_GLOWING_SEASHELL  1300

// for catapults/ships
#define o_HEAVY_SHOT  2135	// TODO: Could apply the ammo system to vehicles


 //////////////////////////////////////////////////////////////////////////////
//// COMPONENTS //////////////////////////////////////////////////////////////

#define COMP_LUMBER  6000	// for 'tunnel'
#define COMP_PILLAR  6015	// for 'tunnel'
#define COMP_ROCK  6050	// for 'lay' (road)
#define COMP_CLAY  6090	// for workforce bricks
#define COMP_COMMON_METAL  6720	// for 'barde'
#define COMP_NAILS  6790	// for 'tunnel' and default maintenance
#define COMP_ROPE  6880	// for 'tie'


 //////////////////////////////////////////////////////////////////////////////
//// MOBS ////////////////////////////////////////////////////////////////////

// Various mobs used by the code

// High Sorcery
#define MIRROR_IMAGE_MOB  450


// empire mobs
#define CITIZEN_MALE  202
#define CITIZEN_FEMALE  203
#define GUARD  204
#define FARMER  206
#define FELLER  207
#define MINER  208
#define BUILDER  209
#define STEALTH_MASTER  221
#define DIGGER  224
#define STUMP_BURNER  226
#define SCRAPER  227
#define REPAIRMAN  229
#define THUG  230
#define CITY_GUARD  251
#define SAWYER  256
#define SMELTER  257
#define WEAVER  258
#define STONECUTTER  259	// unused now
#define NAILMAKER  260	// unused now
#define BRICKMAKER  261	// unused now
#define GARDENER  262	// unused now
#define FIRE_BRIGADE  265
#define TRAPPER  266	// unused now
#define TANNER  267
#define SHEARER  268
#define COIN_MAKER  269
#define DOCKWORKER  270	// unused?
#define APPRENTICE_EXARCH  271	// unused now
#define MILL_WORKER  272
#define VEHICLE_REPAIRMAN  273
#define OVERSEER  274
#define PRESS_WORKER  276
#define MINE_SUPERVISOR  277
#define FISHERMAN  279
#define BEEKEEPER  281	// unused now
#define GLASSMAKER  282	// unused now
#define CRAFTING_APPRENTICE  283
#define PRODUCTION_ASSISTANT  284
#define PROSPECTOR  285


 //////////////////////////////////////////////////////////////////////////////
//// GENERICS ////////////////////////////////////////////////////////////////

// GENERIC_ACTION used by the siege system
#define RES_ACTION_REPAIR  1003


// GENERIC_AFFECTS (ATYPE_x) for affects/dots
#define ATYPE_POISON  3004
#define ATYPE_BOOST  3005
#define ATYPE_SAP  3007
#define ATYPE_MANASHIELD  3008
#define ATYPE_EARTHMELD  3010
#define ATYPE_MUMMIFY  3011
#define ATYPE_HEARTSTOP  3016
#define ATYPE_REJUVENATE  3023
#define ATYPE_BITE_PENALTY  3025	// called "biting" / ATYPE_BITING (search hint)
#define ATYPE_INSPIRE  3026
#define ATYPE_JABBED  3027
#define ATYPE_VIGOR  3031
#define ATYPE_ENERVATE  3032
#define ATYPE_ENERVATE_GAIN  3033
#define ATYPE_SIPHON  3034
#define ATYPE_SLOW  3035
#define ATYPE_SIPHON_DRAIN  3038
#define ATYPE_DG_AFFECT  3039
#define ATYPE_CLAWS  3040
#define ATYPE_DEATHSHROUD  3041
#define ATYPE_DEATH_PENALTY  3046
#define ATYPE_STUN_IMMUNITY  3050
#define ATYPE_WAR_DELAY  3051
#define ATYPE_SHADOWCAGE  3055
#define ATYPE_HOWL  3056
#define ATYPE_DIVERSION  3058
#define ATYPE_CONFER  3060
#define ATYPE_CONFERRED  3061
#define ATYPE_MORPH  3062
#define ATYPE_WELL_FED  3064
#define ATYPE_BITE  3070
#define ATYPE_CANT_STOP  3071	// for vampires
#define ATYPE_SOULCHAIN  3074
#define ATYPE_ARROW_TO_THE_KNEE  3076
#define ATYPE_HOSTILE_DELAY  3077
#define ATYPE_NATURE_BURN  3078
#define ATYPE_RANGED_WEAPON  3079
#define ATYPE_CHARGE  3080
#define ATYPE_TRICK_SHOT  3081
#define ATYPE_BUFF  3082
#define ATYPE_DOT  3085
#define ATYPE_UNSTUCK  3101
#define ATYPE_POTION  3102
#define ATYPE_HUNTED  3103
#define ATYPE_WORKING  3104
#define ATYPE_ARROW_DISTRACTION  3105
#define ATYPE_COLD_PENALTY  3106	// "freezing"
#define ATYPE_HOT_PENALTY  3107	// "sweltering"
#define ATYPE_COOL_PENALTY  3108	// "chilly"
#define ATYPE_WARM_PENALTY  3109	// "warm"
#define ATYPE_BRIEF_RESPITE  3110	// after death


// GENERIC_COOLDOWN entires used by the code
#define COOLDOWN_DEATH_RESPAWN  2001
#define COOLDOWN_LEFT_EMPIRE  2002
#define COOLDOWN_HOSTILE_FLAG  2003
#define COOLDOWN_PVP_FLAG  2004
#define COOLDOWN_PVP_QUIT_TIMER  2005
#define COOLDOWN_MILK  2006
#define COOLDOWN_SHEAR  2007
#define COOLDOWN_OUTRAGE  2009
#define COOLDOWN_RESCUE  2010
#define COOLDOWN_ENERVATE  2014
#define COOLDOWN_SIPHON  2016
#define COOLDOWN_MIRRORIMAGE  2017
#define COOLDOWN_REJUVENATE  2021
#define COOLDOWN_CLEANSE  2022
#define COOLDOWN_ENTANGLE  2025
#define COOLDOWN_HEARTSTOP  2026
#define COOLDOWN_TAME  2033
#define COOLDOWN_SEARCH  2034
#define COOLDOWN_BACKSTAB  2038
#define COOLDOWN_JAB  2039
#define COOLDOWN_BLIND  2040
#define COOLDOWN_SAP  2041
#define COOLDOWN_PRICK  2042
#define COOLDOWN_WEAKEN  2043
#define COOLDOWN_MOONRISE  2044
#define COOLDOWN_ALTERNATE  2045
#define COOLDOWN_DISPEL  2046
#define COOLDOWN_BLOODSWEAT  2047
#define COOLDOWN_EARTHMELD  2048
#define COOLDOWN_SHADOWCAGE  2049
#define COOLDOWN_HOWL  2050
#define COOLDOWN_DIVERSION  2051
#define COOLDOWN_ROGUE_FLAG  2052
#define COOLDOWN_PORTAL_SICKNESS  2053
#define COOLDOWN_BITE  2062
#define COOLDOWN_KITE  2068
#define COOLDOWN_CHARGE  2069
#define COOLDOWN_PLEDGE  2087
#define COOLDOWN_PATHFINDING  2088


// LIQ_x: Some different kind of liquids (vnums of GENERIC_LIQUID)
#define LIQ_WATER  0
#define LIQ_LAGER  1
#define LIQ_WHEATBEER  2
#define LIQ_ALE  3
#define LIQ_CIDER  4
#define LIQ_MILK  5
#define LIQ_BLOOD  6
