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

 //////////////////////////////////////////////////////////////////////////////
//// BUILDINGS ///////////////////////////////////////////////////////////////

// NOTE: many of these are used just for spawn data and if we could do that in
// file instead of code, these would not be needed

// TODO: guard towers could use a flag plus a damage config of some kind

#define BUILDING_RUINS_OPEN  5006	// custom icons and db.world.c
#define BUILDING_RUINS_CLOSED  5007	// custom icons and db.world.c
#define BUILDING_TUNNEL  5008  // building.c
#define BUILDING_CITY_CENTER  5009
#define BUILDING_RUINS_FLOODED  5010	// water ruins

#define BUILDING_FENCE  5112
#define BUILDING_GATE  5113

#define BUILDING_BRIDGE  5133	// TODO: this is only used to indicate use-road-icon

#define BUILDING_STEPS  5140	// custom icons

#define BUILDING_SWAMPWALK  5155

#define BUILDING_TRAPPERS_POST  5161	// workforce.c

#define BUILDING_GATEHOUSE  5173	// custom icons, rituals
#define BUILDING_WALL  5174


// Room building vnums
#define RTYPE_SHIP_HOLDING_PEN  5509	// for the shipping system's storage room
#define RTYPE_SORCERER_TOWER  5511

#define RTYPE_BEDROOM  5601
#define RTYPE_TUNNEL  5612	// for mine complex, tunnel


// special resource action used by the siege system
#define RES_ACTION_REPAIR  3


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
#define o_FLOWER  123	// TODO: find-herbs could be a global type and "flower" is the only one that requires no skill
#define o_WHEAT  141
#define o_HOPS  143
#define o_BARLEY  145
#define o_HANDAXE  181	// TODO: this is special-cased for mining
#define o_FLINT_SET  183	// TODO: this is special-cased for lighting fires

// brewing items
#define o_APPLES  3002
#define o_PEACHES  3004
#define o_CORN  3005

// clay
#define o_BRICKS  257	// TODO: create a workforce brickmaking ability/craft

// Sewn items
#define o_ROPE  2035

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
#define o_NOCTURNIUM_SPIKE  1119

// herbs
#define o_WHITEGRASS  1200
#define o_FIVELEAF  1201
#define o_REDTHORN  1202
#define o_MAGEWHISPER  1203
#define o_DAGGERBITE  1204
#define o_BILEBERRIES  1205
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
