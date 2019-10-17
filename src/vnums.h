/* ************************************************************************
*   File: vnums.h                                         EmpireMUD 2.0b5 *
*  Usage: stores commonly-used virtual numbers                            *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
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

// NOTE: many of these are used just for spawn data and if we could do that in
// file instead of code, these would not be needed

#define BUILDING_RUINS_OPEN  5006	// custom icons and db.world.c
#define BUILDING_RUINS_CLOSED  5007	// custom icons and db.world.c
#define BUILDING_TUNNEL  5008  // building.c
#define BUILDING_CITY_CENTER  5009
#define BUILDING_RUINS_FLOODED  5010	// water ruins

#define BUILDING_FENCE  5112
#define BUILDING_GATE  5113

#define BUILDING_BRIDGE  5133	// TODO: this is only used to indicate use-road-icon

#define BUILDING_STEPS  5140	// custom icons

#define BUILDING_SWAMPWALK  5155	// road icon

#define BUILDING_TRAPPERS_POST  5161	// workforce.c

#define BUILDING_GATEHOUSE  5173	// custom icons, rituals
#define BUILDING_WALL  5174

#define BUILDING_BOARDWALK  5206	// road icon

// Room building vnums
#define RTYPE_SHIP_HOLDING_PEN  5509	// for the shipping system's storage room
#define RTYPE_SORCERER_TOWER  5511
#define RTYPE_EXTRACTION_PIT  5523	// a place to keep chars when they are pending extraction

#define RTYPE_BEDROOM  5601
#define RTYPE_TUNNEL  5612	// for mine complex, tunnel


 //////////////////////////////////////////////////////////////////////////////
//// OBJECTS /////////////////////////////////////////////////////////////////

// these consts can be removed as soon as their hard-coded functions are gone!

/* Boards */
#define BOARD_MORT  1
#define BOARD_IMM  2

// Natural resources
#define o_ROCK  100	// TODO: rocks are special-cased for chipping
#define o_LIGHTNING_STONE  103
#define o_BLOODSTONE  104
#define o_WHEAT  141
#define o_HOPS  143
#define o_BARLEY  145
#define o_HANDAXE  181	// TODO: this is special-cased for mining

// brewing items
#define o_APPLES  3002
#define o_PEACHES  3004
#define o_CORN  3005

// clay
#define o_BRICKS  257	// TODO: create a workforce brickmaking ability/craft

// Wood crafts
#define o_STAKE  915	// could be a flag
#define o_BLANK_SIGN  918

// core objects
#define o_CORPSE  1000
#define o_HOME_CHEST  1010

// Skill tree items
#define o_PORTAL  1100  // could probably safely generate a NOTHING item
#define o_BLOODSWORD  1101
#define o_BLOODSPEAR  1102
#define o_BLOODSKEAN  1103
#define o_BLOODMACE  1104
#define o_FIREBALL  1105
#define o_IMPERIUM_SPIKE  1114
#define o_NEXUS_CRYSTAL  1115
#define o_BLOODSTAFF  1116
#define o_BLOODWHIP  1117
#define o_BLOODAXE  1118
#define o_NOCTURNIUM_SPIKE  1119
#define o_BLOODMAUL  1120
#define o_BLOODMATTOCK  1122

// herbs
#define o_IRIDESCENT_IRIS  1206

// misc stuff
#define o_GLOWING_SEASHELL  1300
#define o_NAILS  1306

// skins
#define o_SMALL_SKIN  1350	// TODO: trapper's post could use a room interaction like TRAPPING
#define o_LARGE_SKIN  1351

// for catapults/ships
#define o_HEAVY_SHOT  2135	// TODO: This could be a material or item type


 //////////////////////////////////////////////////////////////////////////////
//// MOBS ////////////////////////////////////////////////////////////////////

// Various mobs used by the code

#define CHICKEN  9010
#define DOG  9012
#define QUAIL  9020

#define HUMAN_MALE_1  9029
#define HUMAN_MALE_2  9030
#define HUMAN_FEMALE_1  9031
#define HUMAN_FEMALE_2  9032

// High Sorcery
#define SWIFT_DEINONYCHUS  400
#define SWIFT_LIGER  401
#define SWIFT_SERPENT  402
#define SWIFT_STAG  403
#define SWIFT_BEAR  404
#define MIRROR_IMAGE_MOB  450

// Natural Magic
#define FAMILIAR_CAT  500
#define FAMILIAR_SABERTOOTH  501
#define FAMILIAR_SPHINX  502
#define FAMILIAR_GRIFFIN  503
#define FAMILIAR_DIRE_WOLF  504
#define FAMILIAR_MOON_RABBIT  505
#define FAMILIAR_GIANT_TORTOISE  506
#define FAMILIAR_SPIRIT_WOLF  507
#define FAMILIAR_MANTICORE  508
#define FAMILIAR_PHOENIX  509
#define FAMILIAR_SCORPION_SHADOW  510
#define FAMILIAR_OWL_SHADOW  511
#define FAMILIAR_BASILISK  512
#define FAMILIAR_SALAMANDER  513
#define FAMILIAR_SKELETAL_HULK  514
#define FAMILIAR_BANSHEE  515


// empire mobs
#define CITIZEN_MALE  202
#define CITIZEN_FEMALE  203
#define GUARD  204
#define BODYGUARD  205
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
#define STONECUTTER  259
#define NAILMAKER  260
#define BRICKMAKER  261
#define GARDENER  262
#define FIRE_BRIGADE  265
#define TRAPPER  266
#define TANNER  267
#define SHEARER  268
#define COIN_MAKER  269
#define DOCKWORKER  270
#define APPRENTICE_EXARCH  271
#define MILL_WORKER  272
#define VEHICLE_REPAIRMAN  273
#define OVERSEER  274
#define PRESS_WORKER  276
#define MINE_SUPERVISOR  277
#define FISHERMAN  279


 //////////////////////////////////////////////////////////////////////////////
//// GENERICS ////////////////////////////////////////////////////////////////

// GENERIC_ACTION used by the siege system
#define RES_ACTION_REPAIR  1003


// GENERIC_AFFECTS (ATYPE_x) for affects/dots
#define ATYPE_FLY  3001
#define ATYPE_ENTRANCEMENT  3002
#define ATYPE_DARKNESS  3003
#define ATYPE_POISON  3004
#define ATYPE_BOOST  3005
#define ATYPE_CUT_DEEP  3006
#define ATYPE_SAP  3007
#define ATYPE_MANASHIELD  3008
#define ATYPE_EARTHMELD  3010
#define ATYPE_MUMMIFY  3011
#define ATYPE_BESTOW_VIGOR  3013	// NOTE: not used
#define ATYPE_COLORBURST  3015
#define ATYPE_HEARTSTOP  3016
#define ATYPE_PHOENIX_RITE  3017
#define ATYPE_DISARM  3018
#define ATYPE_SHOCKED  3019
#define ATYPE_SKYBRAND  3020
#define ATYPE_COUNTERSPELL  3021
#define ATYPE_REJUVENATE  3023
#define ATYPE_ENTANGLE  3024
#define ATYPE_BITE_PENALTY  3025	// called "biting" / ATYPE_BITING (search hint)
#define ATYPE_INSPIRE  3026
#define ATYPE_JABBED  3027
#define ATYPE_REGEN_POTION  3029
#define ATYPE_NATURE_POTION  3030
#define ATYPE_VIGOR  3031
#define ATYPE_ENERVATE  3032
#define ATYPE_ENERVATE_GAIN  3033
#define ATYPE_SIPHON  3034
#define ATYPE_SLOW  3035
#define ATYPE_SUNSHOCK  3036
#define ATYPE_TRIPPING  3037
#define ATYPE_SIPHON_DRAIN  3038
#define ATYPE_DG_AFFECT  3039
#define ATYPE_CLAWS  3040
#define ATYPE_DEATHSHROUD  3041
#define ATYPE_DEATH_PENALTY  3046
#define ATYPE_BASH  3047
#define ATYPE_STUNNING_BLOW  3049
#define ATYPE_STUN_IMMUNITY  3050
#define ATYPE_WAR_DELAY  3051
#define ATYPE_UNBURDENED  3052
#define ATYPE_SHADOW_KICK  3053
#define ATYPE_STAGGER_JAB  3054
#define ATYPE_SHADOWCAGE  3055
#define ATYPE_HOWL  3056
#define ATYPE_CRUCIAL_JAB  3057
#define ATYPE_DIVERSION  3058
#define ATYPE_SHADOW_JAB  3059
#define ATYPE_CONFER  3060
#define ATYPE_CONFERRED  3061
#define ATYPE_MORPH  3062
#define ATYPE_WHISPERSTRIDE  3063
#define ATYPE_WELL_FED  3064
#define ATYPE_ABLATE  3065
#define ATYPE_ACIDBLAST  3066
#define ATYPE_ASTRALCLAW  3067
#define ATYPE_CHRONOBLAST  3068
#define ATYPE_DISPIRIT  3069
#define ATYPE_BITE  3070
#define ATYPE_CANT_STOP  3071	// for vampires
#define ATYPE_SHADOWLASH_BLIND  3072
#define ATYPE_SHADOWLASH_DOT  3073
#define ATYPE_SOULCHAIN  3074
#define ATYPE_THORNLASH  3075
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


// GENERIC_COOLDOWN entires used by the code
#define COOLDOWN_DEATH_RESPAWN  2001
#define COOLDOWN_LEFT_EMPIRE  2002
#define COOLDOWN_HOSTILE_FLAG  2003
#define COOLDOWN_PVP_FLAG  2004
#define COOLDOWN_PVP_QUIT_TIMER  2005
#define COOLDOWN_MILK  2006
#define COOLDOWN_SHEAR  2007
#define COOLDOWN_DISARM  2008
#define COOLDOWN_OUTRAGE  2009
#define COOLDOWN_RESCUE  2010
#define COOLDOWN_KICK  2011
#define COOLDOWN_BASH  2012
#define COOLDOWN_COLORBURST  2013
#define COOLDOWN_ENERVATE  2014
#define COOLDOWN_SLOW  2015
#define COOLDOWN_SIPHON  2016
#define COOLDOWN_MIRRORIMAGE  2017
#define COOLDOWN_SUNSHOCK  2018
#define COOLDOWN_TELEPORT_HOME  2019
#define COOLDOWN_TELEPORT_CITY  2020
#define COOLDOWN_REJUVENATE  2021
#define COOLDOWN_CLEANSE  2022
#define COOLDOWN_LIGHTNINGBOLT  2023
#define COOLDOWN_SKYBRAND  2024
#define COOLDOWN_ENTANGLE  2025
#define COOLDOWN_HEARTSTOP  2026
#define COOLDOWN_SUMMON_HUMANS  2027
#define COOLDOWN_SUMMON_ANIMALS  2028
#define COOLDOWN_SUMMON_GUARDS  2029
#define COOLDOWN_SUMMON_BODYGUARD  2030
#define COOLDOWN_SUMMON_THUG  2031
#define COOLDOWN_SUMMON_SWIFT  2032 
#define COOLDOWN_TAME  2033
#define COOLDOWN_SEARCH  2034
#define COOLDOWN_TERRIFY  2035
#define COOLDOWN_DARKNESS  2036
#define COOLDOWN_SHADOWSTEP  2037
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
#define COOLDOWN_WHISPERSTRIDE  2054
#define COOLDOWN_ABLATE  2055
#define COOLDOWN_ACIDBLAST  2056
#define COOLDOWN_ARCLIGHT  2057
#define COOLDOWN_ASTRALCLAW  2058
#define COOLDOWN_CHRONOBLAST  2059
#define COOLDOWN_DEATHTOUCH  2060
#define COOLDOWN_DISPIRIT  2061
#define COOLDOWN_BITE  2062
#define COOLDOWN_SHADOWLASH  2064
#define COOLDOWN_SOULCHAIN  2065
#define COOLDOWN_STARSTRIKE  2066
#define COOLDOWN_THORNLASH  2067
#define COOLDOWN_KITE  2068
#define COOLDOWN_CHARGE  2069
#define COOLDOWN_PLEDGE  2087


// LIQ_x: Some different kind of liquids (vnums of GENERIC_LIQUID)
#define LIQ_WATER  0
#define LIQ_LAGER  1
#define LIQ_WHEATBEER  2
#define LIQ_ALE  3
#define LIQ_CIDER  4
#define LIQ_MILK  5
#define LIQ_BLOOD  6
