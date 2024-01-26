/* ************************************************************************
*   File: updates.c                                       EmpireMUD 2.0b5 *
*  Usage: Handles versioning and auto-repair updates for live MUDs        *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "constants.h"
#include "dg_scripts.h"
#include "olc.h"
#include "skills.h"
#include "vnums.h"

#include <glob.h>

/**
* Contents:
*   Beta 2 Update Functions
*   Beta 3 Update Functions
*   Beta 4 Update Functions
*   Beta 5 Update Functions -- Write a function to be run once at startup
*     (Search for ADD HERE)
*   Update Data -- Add to the end of this array to activate the function
*     (search for ADD HERE)
*   Core Functions
*   Pre-b5.116 World Loading
*/


 //////////////////////////////////////////////////////////////////////////////
//// BETA 2 UPDATE FUNCTIONS /////////////////////////////////////////////////

void b2_5_workforce_update(void) {
	empire_data *emp, *next_emp;
	HASH_ITER(hh, empire_table, emp, next_emp) {
		// auto-balance was removed and the same id was used for dismantle-mines
		set_workforce_limit_all(emp, CHORE_DISMANTLE_MINES, 0);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}


// 2.8 removes the color preference
PLAYER_UPDATE_FUNC(b2_8_update_players) {
	REMOVE_BIT(PRF_FLAGS(ch), BIT(9));	// was PRF_COLOR
}


void b2_9_crop_update(void) {
	// this is actually a bug that occurred on EmpireMUDs that patched
	// b2.8 on a live copy; this will look for tiles that are in an
	// error state -- crops that were in the 'seeded' state during the
	// b2.8 reboot would have gotten bad original-sect data
	room_data *room, *next_room;
	HASH_ITER(hh, world_table, room, next_room) {
		if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && SECT_FLAGGED(BASE_SECT(room), SECTF_HAS_CROP_DATA) && !SECT_FLAGGED(BASE_SECT(room), SECTF_CROP)) {
			// normal case: crop with a 'Seeded' original sect
			// the fix is just to set the original sect to the current
			// sect so it will detect a new sect on-harvest instead of
			// setting it back to seeded
			change_base_sector(room, SECT(room));
		}
		/* as of b5.84, this is no longer active because climate_default_sector is gone
		else if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && !ROOM_SECT_FLAGGED(room, SECTF_CROP) && SECT(room) == BASE_SECT(room)) {
			// second error case: a Seeded crop with itself as its
			// original sect: detect a new original sect
			const sector_vnum climate_default_sector[NUM_CLIMATES];
			sector_data *sect;
			crop_data *cp;
			if ((cp = ROOM_CROP(room)) && (sect = sector_proto(climate_default_sector[GET_CROP_CLIMATE(cp)]))) {
				change_base_sector(room, sect);
			}
		}
		*/
	}
}


void b2_11_update(void) {
	struct empire_unique_storage *eus;
	struct trading_post_data *tpd;
	empire_data *emp, *next_emp;
	char_data *mob, *mobpr;
	obj_data *obj, *objpr;

	log(" - assigning mob triggers...");
	DL_FOREACH(character_list, mob) {
		if (IS_NPC(mob) && (mobpr = mob_proto(GET_MOB_VNUM(mob)))) {
			mob->proto_script = copy_trig_protos(mobpr->proto_script);
			assign_triggers(mob, MOB_TRIGGER);
		}
	}

	log(" - assigning triggers to object list...");
	DL_FOREACH(object_list, obj) {
		if ((objpr = obj_proto(GET_OBJ_VNUM(obj)))) {
			obj->proto_script = copy_trig_protos(objpr->proto_script);
			assign_triggers(obj, OBJ_TRIGGER);
		}
	}

	log(" - assigning triggers to warehouse objects...");
	HASH_ITER(hh, empire_table, emp, next_emp) {
		DL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
			if (eus->obj && (objpr = obj_proto(GET_OBJ_VNUM(eus->obj)))) {
				eus->obj->proto_script = copy_trig_protos(objpr->proto_script);
				assign_triggers(eus->obj, OBJ_TRIGGER);
			}
		}
	}

	log(" - assigning triggers to trading post objects...");
	DL_FOREACH(trading_list, tpd) {
		if (tpd->obj && (objpr = obj_proto(GET_OBJ_VNUM(tpd->obj)))) {
			tpd->obj->proto_script = copy_trig_protos(objpr->proto_script);
			assign_triggers(tpd->obj, OBJ_TRIGGER);
		}
	}

	// ensure everything gets saved this way since we won't do this again
	save_all_empires();
	save_trading_post();
}


// 2.11 loads inventories and attaches triggers
PLAYER_UPDATE_FUNC(b2_11_update_players) {
	obj_data *obj, *proto;
	int iter;

	// no work if in-game (covered by other parts of the update)
	if (!is_file) {
		return;
	}

	check_delayed_load(ch);

	// inventory
	DL_FOREACH2(ch->carrying, obj, next_content) {
		if ((proto = obj_proto(GET_OBJ_VNUM(obj)))) {
			obj->proto_script = copy_trig_protos(proto->proto_script);
			assign_triggers(obj, OBJ_TRIGGER);
		}
	}

	// eq
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter) && (proto = obj_proto(GET_OBJ_VNUM(GET_EQ(ch, iter))))) {
			GET_EQ(ch, iter)->proto_script = copy_trig_protos(proto->proto_script);
			assign_triggers(GET_EQ(ch, iter), OBJ_TRIGGER);
		}
	}
}


// this is a repeat of the b2.9 update, but should fix additional rooms
void b3_0_crop_update(void) {
	room_data *room, *next_room;
	HASH_ITER(hh, world_table, room, next_room) {
		if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && SECT_FLAGGED(BASE_SECT(room), SECTF_HAS_CROP_DATA) && !SECT_FLAGGED(BASE_SECT(room), SECTF_CROP)) {
			// normal case: crop with a 'Seeded' original sect
			// the fix is just to set the original sect to the current
			// sect so it will detect a new sect on-harvest instead of
			// setting it back to seeded
			change_base_sector(room, SECT(room));
		}
		/* as of b5.84, this is no longer active because climate_default_sector is gone
		else if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && !ROOM_SECT_FLAGGED(room, SECTF_CROP) && SECT_FLAGGED(BASE_SECT(room), SECTF_HAS_CROP_DATA) && !SECT_FLAGGED(BASE_SECT(room), SECTF_CROP)) {
			// second error case: a Seeded crop with a Seeded crop as
			// its original sect: detect a new original sect
			const sector_vnum climate_default_sector[NUM_CLIMATES];
			sector_data *sect;
			crop_data *cp;
			if ((cp = ROOM_CROP(room)) && (sect = sector_proto(climate_default_sector[GET_CROP_CLIMATE(cp)]))) {
				change_base_sector(room, sect);
			}
		}
		*/
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// BETA 3 UPDATE FUNCTIONS /////////////////////////////////////////////////

// updater for existing mines
void b3_1_mine_update(void) {
	room_data *room, *next_room;
	int type;

	HASH_ITER(hh, world_table, room, next_room) {
		if ((type = get_room_extra_data(room, 0)) <= 0) {	// 0 was ROOM_EXTRA_MINE_TYPE
			continue;
		}

		switch (type) {
			case 10: {	// iron
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 199);
				break;
			}
			case 11: {	// silver
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 161);
				break;
			}
			case 12: {	// gold
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 162);
				break;
			}
			case 13: {	// nocturnium
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 163);
				break;
			}
			case 14: {	// imperium
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 164);
				break;
			}
			case 15: {	// copper
				set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, 160);
				break;
			}
		}

		remove_room_extra_data(room, 0);	// ROOM_EXTRA_MINE_TYPE prior to b3.1
	}
}


PLAYER_UPDATE_FUNC(b3_2_player_gear_disenchant) {
	obj_data *obj, *next_obj, *new;
	int iter;

	check_delayed_load(ch);

	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if ((obj = GET_EQ(ch, iter)) && OBJ_FLAGGED(obj, OBJ_ENCHANTED) && obj_proto(GET_OBJ_VNUM(obj))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
	DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		if (OBJ_FLAGGED(obj, OBJ_ENCHANTED) && obj_proto(GET_OBJ_VNUM(obj))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
}


// removes the PLAYER-MADE flag from rooms and sets their "natural sect" instead
void b3_2_map_and_gear(void) {
	obj_data *obj, *next_obj, *new, *proto;
	struct empire_unique_storage *eus;
	struct trading_post_data *tpd;
	empire_data *emp, *next_emp;

	log("Applying b3.2 update...");

	/* as of b5.84, this is no longer active because climate_default_sector is gone
	log(" - updating the map...");
	const sector_vnum climate_default_sector[NUM_CLIMATES];
	room_data *room, *next_room;
	crop_vnum type;
	sector_vnum OASIS = 21, SANDY_TRENCH = 22;
	int ROOM_EXTRA_CROP_TYPE = 2;	// removed extra type
	bitvector_t ROOM_AFF_PLAYER_MADE = BIT(11);	// removed flag
	HASH_ITER(hh, world_table, room, next_room) {
		// player-made
		if (IS_SET(ROOM_AFF_FLAGS(room) | ROOM_BASE_FLAGS(room), ROOM_AFF_PLAYER_MADE)) {
			// remove the bits
			REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_PLAYER_MADE);
			affect_total_room(room);
		
			// update the natural sector
			if (GET_ROOM_VNUM(room) < MAP_SIZE) {
				world_map[FLAT_X_COORD(room)][FLAT_Y_COORD(room)].natural_sector = sector_proto((GET_SECT_VNUM(SECT(room)) == OASIS || GET_SECT_VNUM(SECT(room)) == SANDY_TRENCH) ? climate_default_sector[CLIMATE_ARID] : climate_default_sector[CLIMATE_TEMPERATE]);
			}
		}
		
		// crops
		if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && !ROOM_CROP(room)) {
			type = get_room_extra_data(room, ROOM_EXTRA_CROP_TYPE);
			set_crop_type(room, type > 0 ? crop_proto(type) : get_potential_crop_for_location(room, NOTHING));
			remove_room_extra_data(room, ROOM_EXTRA_CROP_TYPE);
		}
	}
	*/

	log(" - disenchanting the object list...");
	DL_FOREACH_SAFE(object_list, obj, next_obj) {
		if (OBJ_FLAGGED(obj, OBJ_ENCHANTED) && (proto = obj_proto(GET_OBJ_VNUM(obj))) && !OBJ_FLAGGED(proto, OBJ_ENCHANTED)) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}

	log(" - disenchanting warehouse objects...");
	HASH_ITER(hh, empire_table, emp, next_emp) {
		DL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
			if ((obj = eus->obj) && OBJ_FLAGGED(obj, OBJ_ENCHANTED) && (proto = obj_proto(GET_OBJ_VNUM(obj))) && !OBJ_FLAGGED(proto, OBJ_ENCHANTED)) {
				new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
				eus->obj = new;
				extract_obj(obj);
			}
		}
	}

	log(" - disenchanting trading post objects...");
	DL_FOREACH(trading_list, tpd) {
		if ((obj = tpd->obj) && OBJ_FLAGGED(obj, OBJ_ENCHANTED) && (proto = obj_proto(GET_OBJ_VNUM(obj))) && !OBJ_FLAGGED(proto, OBJ_ENCHANTED)) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			tpd->obj = new;
			extract_obj(obj);
		}
	}

	log(" - disenchanting player inventories...");
	update_all_players(NULL, b3_2_player_gear_disenchant);

	// ensure everything gets saved this way since we won't do this again
	save_all_empires();
	save_trading_post();
}


// fixes some guild-patterend cloth that was accidentally auto-weaved in a previous patch
// NOTE: the cloth is not storable, so any empire with it in normal storage must have had the bug
void b3_6_einv_fix(void) {
	struct empire_storage_data *store, *next_store;
	struct empire_island *isle, *next_isle;
	empire_data *emp, *next_emp;
	obj_data *proto;
	int total, amt;

	obj_vnum vnum = 2344;	// guild-patterned cloth
	obj_vnum cloth = 1359;
	obj_vnum silver = 161;

	proto = obj_proto(vnum);
	if (!proto || GET_OBJ_STORAGE(proto) || !obj_proto(cloth) || !obj_proto(silver)) {
		return;	// no work to do on this EmpireMUD
	}

	log("Fixing incorrectly auto-weaved guild-patterned cloth (2344)");

	HASH_ITER(hh, empire_table, emp, next_emp) {
		total = 0;
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
			HASH_ITER(hh, isle->store, store, next_store) {
				if (store->amount > 0 && store->vnum == vnum) {
					amt = store->amount;
					total += amt;
					add_to_empire_storage(emp, isle->island, cloth, 4 * amt, 0);
					add_to_empire_storage(emp, isle->island, silver, 2 * amt, 0);
					HASH_DEL(isle->store, store);
					free_empire_storage_data(store);
				}
			}
		}

		if (total > 0) {
			log(" - [%d] %s: %d un-woven", EMPIRE_VNUM(emp), EMPIRE_NAME(emp), total);
		}
	}

	save_all_empires();
}


// removes shipping ids that got stuck and are not in the holding pen
void b3_11_ship_fix(void) {
	vehicle_data *veh;

	DL_FOREACH(vehicle_list, veh) {
		if (IN_ROOM(veh) && VEH_SHIPPING_ID(veh) != -1 && (!GET_BUILDING(IN_ROOM(veh)) || GET_BLD_VNUM(GET_BUILDING(IN_ROOM(veh))) != RTYPE_SHIP_HOLDING_PEN)) {
			VEH_SHIPPING_ID(veh) = -1;
		}
	}
}


// removes AFF_SENSE_HIDDEN
PLAYER_UPDATE_FUNC(b3_12_update_players) {
	// only care if they have a permanent sense-hidden
	if (!AFF_FLAGGED(ch, AFF_SENSE_HIDDEN)) {
		return;
	}

	check_delayed_load(ch);
	REMOVE_BIT(AFF_FLAGS(ch), AFF_SENSE_HIDDEN);
	affect_total(ch);	// in case they are getting it from a real affect
}


// respawns wild crops and converts 5% of jungle to crops
void b3_15_crop_update(void) {
	struct map_data *map;
	crop_data *new_crop;
	room_data *room;
	
	/*
	const int SECT_JUNGLE = 28;	// convert jungles at random
	const int JUNGLE_PERCENT = 5;	// change to change jungle to crop
	const int SECT_JUNGLE_FIELD = 16;	// sect to use for crop
	*/

	LL_FOREACH(land_map, map) {
		room = NULL;

		if ((room = map->room)) {
			if (ROOM_OWNER(room)) {
				continue;	// skip owned tiles
			}
		}

		if (map->crop_type) {
			// update crop
			if (room || (room = real_room(map->vnum))) {
				new_crop = get_potential_crop_for_location(room, NOTHING);
				set_crop_type(room, new_crop ? new_crop : crop_table);
			}
		}
		/* removed in b5.165 because wild rops aren't needed
		else if (map->sector_type->vnum == SECT_JUNGLE && number(1, 100) <= JUNGLE_PERCENT) {
			// transform jungle
			if (room || (room = real_room(map->vnum))) {
				change_terrain(room, SECT_JUNGLE_FIELD, NOTHING);	// picks own crop
			}
		}*/
	}
}


// adds built-with resources to roads
void b3_17_road_update(void) {
	struct map_data *map;
	room_data *room;

	obj_vnum rock_obj = 100;

	LL_FOREACH(land_map, map) {
		if (!SECT_FLAGGED(map->sector_type, SECTF_IS_ROAD)) {
			continue;
		}
		if (!(room = real_room(map->vnum))) {
			continue;
		}

		// ensure complex data
		if (!COMPLEX_DATA(room)) {
			COMPLEX_DATA(room) = init_complex_data();
		}

		// add build cost in rocks if necessary
		if (!GET_BUILT_WITH(room)) {
			add_to_resource_list(&GET_BUILT_WITH(room), RES_OBJECT, rock_obj, 20, 0);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// BETA 4 UPDATE FUNCTIONS /////////////////////////////////////////////////

// adds approval
PLAYER_UPDATE_FUNC(b4_1_approve_players) {
	player_index_data *index;

	// fix some level glitches caused by this patch
	if (GET_IMMORTAL_LEVEL(ch) == -1) {
		GET_ACCESS_LEVEL(ch) = MIN(LVL_MORTAL, GET_ACCESS_LEVEL(ch));
	}
	else {
		GET_ACCESS_LEVEL(ch) = LVL_TOP - GET_IMMORTAL_LEVEL(ch);
		GET_ACCESS_LEVEL(ch) = MAX(GET_ACCESS_LEVEL(ch), LVL_GOD);
	}
	if (GET_ACCESS_LEVEL(ch) == LVL_GOD) {
		GET_ACCESS_LEVEL(ch) = LVL_START_IMM;
	}

	// if we should approve them (approve all imms now)
	if (IS_IMMORTAL(ch) || (GET_ACCESS_LEVEL(ch) >= LVL_MORTAL && config_get_bool("auto_approve"))) {
		if (config_get_bool("approve_per_character")) {
			SET_BIT(PLR_FLAGS(ch), PLR_APPROVED);
		}
		else {	// per-account (default)
			SET_BIT(GET_ACCOUNT(ch)->flags, ACCT_APPROVED);
			SAVE_ACCOUNT(GET_ACCOUNT(ch));
		}
	}

	// update the index in case any of this changed
	if ((index = find_player_index_by_idnum(GET_IDNUM(ch)))) {
		update_player_index(index, ch);
	}
}


// adds current mount to mounts list
PLAYER_UPDATE_FUNC(b4_2_mount_update) {
	check_delayed_load(ch);

	if (GET_MOUNT_VNUM(ch)) {
		add_mount(ch, GET_MOUNT_VNUM(ch), GET_MOUNT_FLAGS(ch) & ~MOUNT_RIDING);
	}
}


// sets default fight message flags
PLAYER_UPDATE_FUNC(b4_4_fight_messages) {
	SET_BIT(GET_FIGHT_MESSAGES(ch), DEFAULT_FIGHT_MESSAGES);
}


// convert data on unfinished buildings and disrepair
void b4_15_building_update(void) {
	struct resource_data *res, *disrepair_res;
	room_data *room, *next_room;

	HASH_ITER(hh, world_table, room, next_room) {
		// add INCOMPLETE aff
		if (BUILDING_RESOURCES(room) && !IS_DISMANTLING(room)) {
			SET_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_INCOMPLETE);
			affect_total_room(room);
		}

		// convert maintenance
		if (COMPLEX_DATA(room) && GET_BUILDING(room) && COMPLEX_DATA(room)->disrepair > 0) {
			// add maintenance
			if (GET_BLD_REGULAR_MAINTENANCE(GET_BUILDING(room))) {
				// basic stuff
				disrepair_res = copy_resource_list(GET_BLD_REGULAR_MAINTENANCE(GET_BUILDING(room)));

				// multiply by years of disrepair
				LL_FOREACH(disrepair_res, res) {
					res->amount *= COMPLEX_DATA(room)->disrepair;
				}

				// combine into existing resources
				if (BUILDING_RESOURCES(room)) {
					res = BUILDING_RESOURCES(room);
					GET_BUILDING_RESOURCES(room) = combine_resources(res, disrepair_res);
					free_resource_list(res);
				}
				else {
					GET_BUILDING_RESOURCES(room) = disrepair_res;
				}
			}

			// add damage (10% per year of disrepair)
			COMPLEX_DATA(room)->damage += COMPLEX_DATA(room)->disrepair * GET_BLD_MAX_DAMAGE(GET_BUILDING(room)) / 10;
		}

		// clear this
		if (COMPLEX_DATA(room)) {
			COMPLEX_DATA(room)->disrepair = 0;
		}
	}
}


// 4.19 removes the vampire flag
PLAYER_UPDATE_FUNC(b4_19_update_players) {
	bitvector_t PLR_VAMPIRE = BIT(14);
	REMOVE_BIT(PLR_FLAGS(ch), PLR_VAMPIRE);
}


// 4.32 moves 2 rmt flags to fnc flags
void b4_32_convert_rmts(void) {
	bitvector_t RMT_PIGEON_POST = BIT(9);	// j. can use mail here
	bitvector_t RMT_COOKING_FIRE = BIT(10);	// k. can cook here

	room_template *rmt, *next_rmt;

	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		if (IS_SET(GET_RMT_FLAGS(rmt), RMT_PIGEON_POST)) {
			REMOVE_BIT(GET_RMT_FLAGS(rmt), RMT_PIGEON_POST);
			SET_BIT(GET_RMT_FUNCTIONS(rmt), FNC_MAIL);
			save_library_file_for_vnum(DB_BOOT_RMT, GET_RMT_VNUM(rmt));
			log("- updated rmt %d: PIGEON-POST", GET_RMT_VNUM(rmt));
		}
		if (IS_SET(GET_RMT_FLAGS(rmt), RMT_COOKING_FIRE)) {
			REMOVE_BIT(GET_RMT_FLAGS(rmt), RMT_COOKING_FIRE);
			SET_BIT(GET_RMT_FUNCTIONS(rmt), FNC_COOKING_FIRE);
			save_library_file_for_vnum(DB_BOOT_RMT, GET_RMT_VNUM(rmt));
			log("- updated rmt %d: COOKING-FIRE", GET_RMT_VNUM(rmt));
		}
	}
}


// 4.36 needs triggers attached to studies
void b4_36_study_triggers(void) {
	const any_vnum bld_study = 5608, attach_trigger = 5609;
	struct trig_proto_list *tpl;
	room_data *room;

	DL_FOREACH2(interior_room_list, room, next_interior) {
		if (!GET_BUILDING(room) || GET_BLD_VNUM(GET_BUILDING(room)) != bld_study) {
			continue;
		}

		CREATE(tpl, struct trig_proto_list, 1);
		tpl->vnum = attach_trigger;
		LL_CONCAT(room->proto_script, tpl);

		assign_triggers(room, WLD_TRIGGER);
	}
}


// 4.38 needs triggers attached to towers
void b4_38_tower_triggers(void) {
	const any_vnum bld_tower = 5511, attach_trigger = 5511;
	struct trig_proto_list *tpl;
	room_data *room;

	DL_FOREACH2(interior_room_list, room, next_interior) {
		if (!GET_BUILDING(room) || GET_BLD_VNUM(GET_BUILDING(room)) != bld_tower) {
			continue;
		}

		CREATE(tpl, struct trig_proto_list, 1);
		tpl->vnum = attach_trigger;
		LL_CONCAT(room->proto_script, tpl);

		assign_triggers(room, WLD_TRIGGER);
	}
}


// b4.39 convert stored data from old files
void b4_39_data_conversion(void) {
	const char *EXP_FILE = LIB_ETC"exp_cycle";	// used prior to this patch
	const char *TIME_FILE = LIB_ETC"time";

	long l_in;
	int i_in;
	FILE *fl;

	// exp cycle file
	if ((fl = fopen(EXP_FILE, "r"))) {
		fscanf(fl, "%d\n", &i_in);
		data_set_long(DATA_DAILY_CYCLE, i_in);
		log(" - imported daily cycle %ld", data_get_long(DATA_DAILY_CYCLE));
		fclose(fl);
		unlink(EXP_FILE);
	}

	// time file
	if ((fl = fopen(TIME_FILE, "r"))) {
		fscanf(fl, "%ld\n", &l_in);
		data_set_long(DATA_WORLD_START, l_in);
		log(" - imported start time %ld", data_get_long(DATA_WORLD_START));
		fclose(fl);
		unlink(TIME_FILE);

		reset_time();
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// BETA 5 UPDATE FUNCTIONS /////////////////////////////////////////////////

// b5.1 changes the values of ATYPE_x consts and this updates existing affects
PLAYER_UPDATE_FUNC(b5_1_update_players) {
	struct trig_var_data *var, *next_var;
	struct over_time_effect_type *dot;
	struct affected_type *af;
	bool delete;
	
	// some variables from the stock game content
	const int jungletemple_tokens = 18500;
	const int roc_tokens_evil = 11003;
	const int roc_tokens_good = 11004;
	const int dragtoken_128 = 10900;
	const int permafrost_tokens_104 = 10550;
	const int adventureguild_chelonian_tokens = 18200;
	
	check_delayed_load(ch);
	
	LL_FOREACH(ch->affected, af) {
		if (af->type < 3000) {
			af->type += 3000;
		}
	}
	
	LL_FOREACH(ch->over_time_effects, dot) {
		if (dot->type < 3000) {
			dot->type += 3000;
		}
	}
	
	if (SCRIPT(ch) && SCRIPT(ch)->global_vars) {
		LL_FOREACH_SAFE(SCRIPT(ch)->global_vars, var, next_var) {
			// things just being deleted (now using cooldowns)
			if (!strncmp(var->name, "minipet", 7)) {
				delete = TRUE;
			}
			else if (!strcmp(var->name, "last_hestian_time")) {
				delete = TRUE;
			}
			else if (!strcmp(var->name, "last_conveyance_time")) {
				delete = TRUE;
			}
			else if (!strcmp(var->name, "last_skycleave_trinket_time")) {
				delete = TRUE;
			}
			else if (!strcmp(var->name, "last_tortoise_time")) {
				delete = TRUE;
			}
			else if (!strcmp(var->name, "tomb10770")) {
				delete = TRUE;
			}
			else if (!strcmp(var->name, "summon_10728")) {
				delete = TRUE;
			}
			
			// things converting to currencies
			else if (!strcmp(var->name, "jungletemple_tokens")) {
				add_currency(ch, jungletemple_tokens, atoi(var->value));
				delete = TRUE;
			}
			else if (!strcmp(var->name, "roc_tokens_evil")) {
				add_currency(ch, roc_tokens_evil, atoi(var->value));
				delete = TRUE;
			}
			else if (!strcmp(var->name, "roc_tokens_good")) {
				add_currency(ch, roc_tokens_good, atoi(var->value));
				delete = TRUE;
			}
			else if (!strcmp(var->name, "dragtoken_128")) {
				add_currency(ch, dragtoken_128, atoi(var->value));
				delete = TRUE;
			}
			else if (!strcmp(var->name, "permafrost_tokens_104")) {
				add_currency(ch, permafrost_tokens_104, atoi(var->value));
				delete = TRUE;
			}
			else if (!strcmp(var->name, "adventureguild_chelonian_tokens")) {
				add_currency(ch, adventureguild_chelonian_tokens, atoi(var->value));
				delete = TRUE;
			}
			else {
				delete = FALSE;
			}
			
			// if requested, delete it
			if (delete) {
				LL_DELETE(SCRIPT(ch)->global_vars, var);
				free_var_el(var);
			}
		}
	}
}


// b5.1 convert resource action vnums (all resource actions += 1000)
void b5_1_global_update(void) {
	struct instance_data *inst, *next_inst;
	craft_data *craft, *next_craft;
	adv_data *adv, *next_adv;
	bld_data *bld, *next_bld;
	vehicle_data *veh, *next_veh;
	room_data *room, *next_room;
	struct resource_data *res;
	
	// adventures
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		if (IS_SET(GET_ADV_FLAGS(adv), ADV_IN_DEVELOPMENT)) {
			continue;	// skip in-dev
		}
		if (!IS_SET(GET_ADV_FLAGS(adv), ADV_CAN_DELAY_LOAD)) {
			continue;	// only want delayables
		}
		
		// delete 'em
		DL_FOREACH_SAFE(instance_list, inst, next_inst) {
			if (INST_ADVENTURE(inst) == adv) {
				delete_instance(inst, FALSE);
			}
		}
	}
	
	// crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		LL_FOREACH(GET_CRAFT_RESOURCES(craft), res) {
			if (res->type == RES_ACTION && res->vnum < 100) {
				res->vnum += 1000;
				save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
			}
		}
	}
	
	// buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		LL_FOREACH(GET_BLD_REGULAR_MAINTENANCE(bld), res) {
			if (res->type == RES_ACTION && res->vnum < 100) {
				res->vnum += 1000;
				save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
			}
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		LL_FOREACH(VEH_REGULAR_MAINTENANCE(veh), res) {
			if (res->type == RES_ACTION && res->vnum < 100) {
				res->vnum += 1000;
				save_library_file_for_vnum(DB_BOOT_VEH, VEH_VNUM(veh));
			}
		}
	}
	
	// live rooms
	HASH_ITER(hh, world_table, room, next_room) {
		LL_FOREACH(BUILDING_RESOURCES(room), res) {
			if (res->type == RES_ACTION && res->vnum < 100) {
				res->vnum += 1000;
			}
		}
	}
	
	// live vehicles
	DL_FOREACH(vehicle_list, veh) {
		LL_FOREACH(VEH_NEEDS_RESOURCES(veh), res) {
			if (res->type == RES_ACTION && res->vnum < 100) {
				res->vnum += 1000;
			}
		}
	}
}


// b5.3 looks for missile weapons that need updates
void b5_3_missile_update(void) {
	obj_data *obj, *next_obj;
	
	int TYPE_BOW = 29;	// bow type at the time of this patch
	
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (!IS_MISSILE_WEAPON(obj)) {
			continue;
		}
		
		// ALL bows are TYPE_BOW before this update
		if (GET_MISSILE_WEAPON_TYPE(obj) != TYPE_BOW) {
			log(" - updating %d %s from %d to %d (bow)", GET_OBJ_VNUM(obj), skip_filler(GET_OBJ_SHORT_DESC(obj)), GET_MISSILE_WEAPON_TYPE(obj), TYPE_BOW);
			set_obj_val(obj, VAL_MISSILE_WEAPON_TYPE, TYPE_BOW);
			save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
		}
	}
}


// b5.14 refreshes superior items
PLAYER_UPDATE_FUNC(b5_14_player_superiors) {
	obj_data *obj, *next_obj, *new;
	int iter;
	
	check_delayed_load(ch);
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if ((obj = GET_EQ(ch, iter)) && OBJ_FLAGGED(obj, OBJ_SUPERIOR) && obj_proto(GET_OBJ_VNUM(obj))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
	DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		if (OBJ_FLAGGED(obj, OBJ_SUPERIOR) && obj_proto(GET_OBJ_VNUM(obj))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
}


// removes the PLAYER-MADE flag from rooms and sets their "natural sect" instead
void b5_14_superior_items(void) {
	obj_data *obj, *next_obj, *new, *proto;
	struct empire_unique_storage *eus;
	struct trading_post_data *tpd;
	empire_data *emp, *next_emp;
	
	log(" - refreshing superiors in the object list...");
	DL_FOREACH_SAFE(object_list, obj, next_obj) {
		if (OBJ_FLAGGED(obj, OBJ_SUPERIOR) && (proto = obj_proto(GET_OBJ_VNUM(obj))) && !OBJ_FLAGGED(proto, OBJ_SUPERIOR)) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
	
	log(" - refreshing superiors in warehouse objects...");
	HASH_ITER(hh, empire_table, emp, next_emp) {
		DL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
			if ((obj = eus->obj) && OBJ_FLAGGED(obj, OBJ_SUPERIOR) && (proto = obj_proto(GET_OBJ_VNUM(obj))) && !OBJ_FLAGGED(proto, OBJ_SUPERIOR)) {
				new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
				eus->obj = new;
				extract_obj(obj);
			}
		}
	}
	
	log(" - refreshing superiors in trading post objects...");
	DL_FOREACH(trading_list, tpd) {
		if ((obj = tpd->obj) && OBJ_FLAGGED(obj, OBJ_SUPERIOR) && (proto = obj_proto(GET_OBJ_VNUM(obj))) && !OBJ_FLAGGED(proto, OBJ_SUPERIOR)) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			tpd->obj = new;
			extract_obj(obj);
		}
	}
}


// looks for stray flags in the world
void b5_19_world_fix(void) {
	struct map_data *map;
	bitvector_t flags;
	room_data *room;
	bld_data *bld;
	
	bitvector_t flags_to_wipe = ROOM_AFF_MAGIC_DARKNESS | ROOM_AFF_SILENT | ROOM_AFF_NO_WEATHER | ROOM_AFF_NO_TELEPORT;
	bitvector_t empire_only_flags = ROOM_AFF_PUBLIC | ROOM_AFF_NO_WORK | ROOM_AFF_NO_DISMANTLE | ROOM_AFF_NO_ABANDON;
	
	LL_FOREACH(land_map, map) {
		if (!map->shared->affects && !map->shared->base_affects) {
			continue;	// only looking for 'affected' tiles
		}
		
		// used several times below:
		room = map->room;
		
		// building affs fix
		if (!room || !(bld = GET_BUILDING(room))) {
			REMOVE_BIT(map->shared->base_affects, flags_to_wipe);
			REMOVE_BIT(map->shared->affects, flags_to_wipe);
		}
		else {	// has a building -- look for errors
			flags = flags_to_wipe;
			if (flags) {	// do not remove flags the building actually uses
				REMOVE_BIT(ROOM_BASE_FLAGS(room), flags);
				affect_total_room(room);
			}
			affect_total_room(room);
		}
		
		// empire flags (just check that these are not on unclaimed rooms)
		if (!room || !ROOM_OWNER(room)) {
			REMOVE_BIT(map->shared->base_affects, empire_only_flags);
			REMOVE_BIT(map->shared->affects, empire_only_flags);
			
			if (room) {
				affect_total_room(room);
			}
		}
	}
}


// finds old-style player-made rivers and converts them to canals
void b5_20_canal_fix(void) {
	struct map_data *map;
	sector_data *canl;
	bool change;
	
	int river_sect = 5;
	int canal_sect = 19;
	int estuary_sect = 53;
	
	if (!(canl = sector_proto(canal_sect))) {
		log("SYSERR: failed to do b5_20_canal_fix because canal sector is invalid.");
		exit(0);
	}
	
	LL_FOREACH(land_map, map) {
		// is a river but not a real river?
		change = GET_SECT_VNUM(map->base_sector) == river_sect && GET_SECT_VNUM(map->natural_sector) != river_sect;
		
		// is an estuary but not a real one or natural river?
		change |= GET_SECT_VNUM(map->base_sector) == estuary_sect && GET_SECT_VNUM(map->natural_sector) != estuary_sect && GET_SECT_VNUM(map->natural_sector) != river_sect;
		
		if (change) {
			// only change current sector if currently river
			if (map->sector_type == map->base_sector) {
				perform_change_sect(NULL, map, canl);
			}
			// always change base sector (it was required to hit this condition)
			perform_change_base_sect(NULL, map, canl);
			
			// for debugging:
			// log("- updated (%d, %d) from river to canal", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
		}
	}
}


// updates potions in player inventory
PLAYER_UPDATE_FUNC(b5_23_player_potion_update) {
	obj_data *obj, *next_obj, *new;
	int iter;
	
	check_delayed_load(ch);
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if ((obj = GET_EQ(ch, iter)) && IS_POTION(obj) && obj_proto(GET_OBJ_VNUM(obj))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
	DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		if (IS_POTION(obj) && obj_proto(GET_OBJ_VNUM(obj))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
}


// reloads and rescales all potions, and moves them from warehouse to einv if possible
void b5_23_potion_update(void) {
	struct empire_unique_storage *eus, *next_eus;
	obj_data *obj, *next_obj, *new, *proto;
	struct trading_post_data *tpd;
	empire_data *emp, *next_emp;
	
	log(" - updating the object list...");
	DL_FOREACH_SAFE(object_list, obj, next_obj) {
		if (IS_POTION(obj) && (proto = obj_proto(GET_OBJ_VNUM(obj)))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
	
	log(" - updating warehouse objects...");
	HASH_ITER(hh, empire_table, emp, next_emp) {
		DL_FOREACH_SAFE(EMPIRE_UNIQUE_STORAGE(emp), eus, next_eus) {
			if ((obj = eus->obj) && IS_POTION(obj) && (proto = obj_proto(GET_OBJ_VNUM(obj)))) {
				if (GET_OBJ_STORAGE(proto)) {
					// move to regular einv if now possible
					add_to_empire_storage(emp, eus->island, GET_OBJ_VNUM(proto), eus->amount, GET_OBJ_TIMER(proto));
					DL_DELETE(EMPIRE_UNIQUE_STORAGE(emp), eus);
					free(eus);
				}
				else {	// otherwise replace with a fresh copy
					new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
					eus->obj = new;
				}
				
				// either way the original is gone?
				extract_obj(obj);
			}
		}
	}
	
	log(" - updating trading post objects...");
	DL_FOREACH(trading_list, tpd) {
		if ((obj = tpd->obj) && IS_POTION(obj) && (proto = obj_proto(GET_OBJ_VNUM(obj)))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			tpd->obj = new;
			extract_obj(obj);
		}
	}
}


// updates poisons in player inventory
PLAYER_UPDATE_FUNC(b5_24_player_poison_update) {
	obj_data *obj, *next_obj, *new;
	int iter;
	
	check_delayed_load(ch);
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if ((obj = GET_EQ(ch, iter)) && IS_POISON(obj) && obj_proto(GET_OBJ_VNUM(obj))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
	DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		if (IS_POISON(obj) && obj_proto(GET_OBJ_VNUM(obj))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
}


// reloads and rescales all poisons, and moves them from warehouse to einv if possible
void b5_24_poison_update(void) {
	struct empire_unique_storage *eus, *next_eus;
	obj_data *obj, *next_obj, *new, *proto;
	struct trading_post_data *tpd;
	empire_data *emp, *next_emp;
	
	log(" - updating the object list...");
	DL_FOREACH_SAFE(object_list, obj, next_obj) {
		if (IS_POISON(obj) && (proto = obj_proto(GET_OBJ_VNUM(obj)))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
	
	log(" - updating warehouse objects...");
	HASH_ITER(hh, empire_table, emp, next_emp) {
		DL_FOREACH_SAFE(EMPIRE_UNIQUE_STORAGE(emp), eus, next_eus) {
			if ((obj = eus->obj) && IS_POISON(obj) && (proto = obj_proto(GET_OBJ_VNUM(obj)))) {
				if (GET_OBJ_STORAGE(proto)) {
					// move to regular einv if now possible
					add_to_empire_storage(emp, eus->island, GET_OBJ_VNUM(proto), eus->amount, GET_OBJ_TIMER(proto));
					DL_DELETE(EMPIRE_UNIQUE_STORAGE(emp), eus);
					free(eus);
				}
				else {	// otherwise replace with a fresh copy
					new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
					eus->obj = new;
				}
				
				// either way the original is gone?
				extract_obj(obj);
			}
		}
	}
	
	log(" - updating trading post objects...");
	DL_FOREACH(trading_list, tpd) {
		if ((obj = tpd->obj) && IS_POISON(obj) && (proto = obj_proto(GET_OBJ_VNUM(obj)))) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			tpd->obj = new;
			extract_obj(obj);
		}
	}
}


// sets dummy data on existing trenches/canals to prevent issues with new un-trenching code on existing games
void b5_25_trench_update(void) {
	struct map_data *map;
	int count = 0;
	
	any_vnum canal = 19;
	
	LL_FOREACH(land_map, map) {
		if (GET_SECT_VNUM(map->sector_type) == canal || GET_SECT_VNUM(map->base_sector) == canal || SECT_FLAGGED(map->sector_type, SECTF_IS_TRENCH) || SECT_FLAGGED(map->base_sector, SECTF_IS_TRENCH)) {
			// set this to NOTHING since we don't know the original type, and '0' would result in all trenches un-trenching to plains
			set_extra_data(&map->shared->extra_data, ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR, NOTHING);
			++count;
		}
	}
	
	log("- updated %d tile%s", count, PLURAL(count));
}


// fixes some empire data
void b5_30_empire_update(void) {
	struct empire_trade_data *trade, *next_trade;
	empire_data *emp, *next_emp;
	obj_data *proto;
	int iter;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		// fixes an issue where some numbers were defaulted higher -- these attributes are not even used prior to b5.30
		for (iter = 0; iter < NUM_EMPIRE_ATTRIBUTES; ++iter) {
			EMPIRE_ATTRIBUTE(emp, iter) = 0;
		}
		
		// fixes an older issue with trade data -- unstorable items
		LL_FOREACH_SAFE(EMPIRE_TRADE(emp), trade, next_trade) {
			if (!(proto = obj_proto(trade->vnum)) || !GET_OBJ_STORAGE(proto)) {
				LL_DELETE(EMPIRE_TRADE(emp), trade);
			}
		}
		
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
		EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	}
}


PLAYER_UPDATE_FUNC(b5_34_player_update) {
	struct player_skill_data *skill, *next_skill;
	struct player_ability_data *abil, *next_abil;
	int iter;
	
	check_delayed_load(ch);
	
	// check empire skill
	if (!IS_IMMORTAL(ch)) {
		HASH_ITER(hh, GET_SKILL_HASH(ch), skill, next_skill) {
			// delete empire skill entry
			if (skill->vnum == 1) {
				// gain bonus exp
				SAFE_ADD(GET_DAILY_BONUS_EXPERIENCE(ch), skill->level, 0, UCHAR_MAX, FALSE);
				HASH_DEL(GET_SKILL_HASH(ch), skill);
				free(skill);
			}
		}
	}
	
	// clear all ability purchases
	HASH_ITER(hh, GET_ABILITY_HASH(ch), abil, next_abil) {
		for (iter = 0; iter < NUM_SKILL_SETS; ++iter) {
			remove_ability_by_set(ch, find_ability_by_vnum(abil->vnum), iter, FALSE);
		}
	}
	
	// remove cooldowns
	while (ch->cooldowns) {
		remove_cooldown(ch, ch->cooldowns);
	}
	
	// remove all affects
	while (ch->affected) {
		affect_remove(ch, ch->affected);
	}
	while (ch->over_time_effects) {
		dot_remove(ch, ch->over_time_effects);
	}
	affect_total(ch);
}


// part of the HUGE progression update
void b5_34_mega_update(void) {
	struct instance_data *inst, *next_inst;
	struct empire_political_data *pol;
	empire_data *emp, *next_emp;
	char_data *mob, *next_mob;
	int iter;
	
	// remove Spirit of Progress mob
	DL_FOREACH_SAFE(character_list, mob, next_mob) {
		if (IS_NPC(mob) && GET_MOB_VNUM(mob) == 10856) {
			extract_char(mob);
		}
	}
	
	// remove all instances of adventure 50 (was shut off by this patch)
	DL_FOREACH_SAFE(instance_list, inst, next_inst) {
		if (INST_ADVENTURE(inst) && GET_ADV_VNUM(INST_ADVENTURE(inst)) == 50) {
			delete_instance(inst, TRUE);
		}
	}
	
	// update empires:
	HASH_ITER(hh, empire_table, emp, next_emp) {
		// remove all 'trade' relations
		LL_FOREACH(EMPIRE_DIPLOMACY(emp), pol) {
			REMOVE_BIT(pol->type, DIPL_TRADE);
			REMOVE_BIT(pol->offer, DIPL_TRADE);
		}
		
		// reset progression
		free_empire_goals(EMPIRE_GOALS(emp));
		EMPIRE_GOALS(emp) = NULL;
		free_empire_completed_goals(EMPIRE_COMPLETED_GOALS(emp));
		EMPIRE_COMPLETED_GOALS(emp) = NULL;
		
		// reset points
		for (iter = 0; iter < NUM_PROGRESS_TYPES; ++iter) {
			EMPIRE_PROGRESS_POINTS(emp, iter) = 0;
		}
		
		// reset attributes
		for (iter = 0; iter < NUM_EMPIRE_ATTRIBUTES; ++iter) {
			EMPIRE_ATTRIBUTE(emp, iter) = 0;
		}
		
		// reset techs
		for (iter = 0; iter < NUM_TECHS; ++iter) {
			EMPIRE_TECH(emp, iter) = 0;
			EMPIRE_BASE_TECH(emp, iter) = 0;
		}
		
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
	
	// rescan everyone
	reread_empire_tech(NULL);
}


// fixes some progression goals
void b5_35_progress_update(void) {
	struct empire_completed_goal *goal, *next_goal;
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		HASH_ITER(hh, EMPIRE_COMPLETED_GOALS(emp), goal, next_goal) {
			// Some goals were changed in this patch. Need to update anybody who completed them.
			switch (goal->vnum) {
				case 3030: {	// Vanguard: change category
					EMPIRE_PROGRESS_POINTS(emp, PROGRESS_INDUSTRY) -= 25;
					EMPIRE_PROGRESS_POINTS(emp, PROGRESS_DEFENSE) += 25;
					break;
				}
				case 2015: {	// Cache of Resources: remove workforce cap bonus
					EMPIRE_ATTRIBUTE(emp, EATT_WORKFORCE_CAP) -= 100;
					break;
				}
				case 2016: {	// Rare Surplus: add workforce cap bonus
					EMPIRE_ATTRIBUTE(emp, EATT_WORKFORCE_CAP) += 100;
					break;
				}
				case 4002: {	// World Famous: +25 terr
					EMPIRE_ATTRIBUTE(emp, EATT_BONUS_TERRITORY) += 25;
					break;
				}
				case 1902: {	// Stilt Buildings: +craft
					add_learned_craft_empire(emp, 5207);	// river fishery
					add_learned_craft_empire(emp, 5208);	// lake fishery
					break;
				}
				
				// things that give +5 territory now
				case 1001:	// Homestead
				case 1004:	// Expanding the Empire
				case 1005:	// Masonry
				case 2001:	// Clayworks
				case 2010:	// Storage
				case 2012:	// Artisans
				case 2018:	// Collecting Herbs
				case 2019:	// Herbal Empire
				case 2031:	// Harvest Time
				case 3001:	// Fortifications
				case 3002:	// Building the Barracks
				case 3031:	// Deterrence
				case 4011:	// Enchanted Forest
				case 4012:	// The Magic Grows
				{
					EMPIRE_ATTRIBUTE(emp, EATT_BONUS_TERRITORY) += 5;
					break;
				}
				
				// +50 territory
				case 4013:	// Magic to Make...
				case 2033: {	// Cornucopia
					EMPIRE_ATTRIBUTE(emp, EATT_BONUS_TERRITORY) += 50;
					break;
				}
			}
		}
		
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}


// fixes some progression goals
void b5_37_progress_update(void) {
	struct empire_completed_goal *goal, *next_goal;
	struct instance_data *inst, *next_inst;
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		HASH_ITER(hh, EMPIRE_COMPLETED_GOALS(emp), goal, next_goal) {
			// Some goals were changed in this patch. Need to update anybody who completed them.
			switch (goal->vnum) {
				case 2011: {	// Foundations: +craft
					add_learned_craft_empire(emp, 5209);	// depository
					break;
				}
			}
		}
		
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
	
	// remove all instances of adventure 12600 (force respawn to attach trigger)
	DL_FOREACH_SAFE(instance_list, inst, next_inst) {
		if (INST_ADVENTURE(inst) && GET_ADV_VNUM(INST_ADVENTURE(inst)) == 12600) {
			delete_instance(inst, TRUE);
		}
	}
}


// remove old Groves
void b5_38_grove_update(void) {
	struct instance_data *inst, *next_inst;
	
	// remove all instances of adventure 100 (it's now in-dev)
	DL_FOREACH_SAFE(instance_list, inst, next_inst) {
		if (INST_ADVENTURE(inst) && GET_ADV_VNUM(INST_ADVENTURE(inst)) == 100) {
			delete_instance(inst, TRUE);
		}
	}
}


// add new channels
PLAYER_UPDATE_FUNC(b5_40_update_players) {
	struct player_slash_channel *slash;
	struct slash_channel *chan;
	int iter;
	
	char *to_join[] = { "grats", "death", "progress", "\n" };
	
	for (iter = 0; *to_join[iter] != '\n'; ++iter) {
		if (!(chan = find_slash_channel_by_name(to_join[iter], TRUE))) {
			chan = create_slash_channel(to_join[iter]);
		}
		if (!find_on_slash_channel(ch, chan->id)) {
			CREATE(slash, struct player_slash_channel, 1);
			LL_PREPEND(GET_SLASH_CHANNELS(ch), slash);
			slash->id = chan->id;
		}
	}
}


// keep now has a variable number and old data must be converted
void b5_45_keep_update(void) {
	struct empire_storage_data *store, *next_store;
	struct empire_island *isle, *next_isle;
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
			HASH_ITER(hh, isle->store, store, next_store) {
				if (store->keep == 1) {
					// convert old 0/1 to 0/UNLIMITED
					store->keep = UNLIMITED;
					EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
				}
			}
		}
	}
}


// fix reputation
PLAYER_UPDATE_FUNC(b5_47_update_players) {
	struct player_faction_data *fct, *next;
	bool any = FALSE;
	
	check_delayed_load(ch);
	
	HASH_ITER(hh, GET_FACTIONS(ch), fct, next) {
		fct->value *= 10;	// this update raised the scale of faction rep by 10x
		any = TRUE;
	}
	
	if (any) {
		update_reputations(ch);
	}
}


// resets mountain tiles off-cycle because of the addition of tin
void b5_47_mine_update(void) {
	struct island_info *isle, *next_isle;
	struct map_data *tile;
	room_data *room;
	
	const char *detect_desc = "   The island has ";
	int len = strlen(detect_desc);
	
	// clear mines
	LL_FOREACH(land_map, tile) {
		if (!(room = tile->room) || !room_has_function_and_city_ok(ROOM_OWNER(room), room, FNC_MINE)) {
			remove_extra_data(&tile->shared->extra_data, ROOM_EXTRA_MINE_GLB_VNUM);
			remove_extra_data(&tile->shared->extra_data, ROOM_EXTRA_MINE_AMOUNT);
			remove_extra_data(&tile->shared->extra_data, ROOM_EXTRA_PROSPECT_EMPIRE);
		}
	}
	
	// island desc flags
	HASH_ITER(hh, island_table, isle, next_isle) {
		if (isle->desc && strncmp(isle->desc, detect_desc, len)) {
			SET_BIT(isle->flags, ISLE_HAS_CUSTOM_DESC);
		}
	}
}

// adds a rope vnum to mobs that are tied
void b5_48_rope_update(void) {
	char_data *mob;
	
	obj_vnum OLD_ROPE = 2035;	// leather rope
	
	DL_FOREACH(character_list, mob) {
		if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_TIED)) {
			GET_ROPE_VNUM(mob) = OLD_ROPE;
		}
	}
}


// tracks current empire einv as "gathered items" to retroactively set gather totals
void b5_58_gather_totals(void) {
	struct empire_island *isle, *next_isle;
	struct empire_storage_data *store, *next_store;
	empire_data *emp, *next_emp;
	
	// each empire
	HASH_ITER(hh, empire_table, emp, next_emp) {
		// each island
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
			// storage
			HASH_ITER(hh, isle->store, store, next_store) {
				if (store->amount > 0) {
					add_production_total(emp, store->vnum, store->amount);
				}
			}
		}
		
		EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	}
}


// add new channel
PLAYER_UPDATE_FUNC(b5_60_update_players) {
	struct player_slash_channel *slash;
	struct slash_channel *chan;
	int iter;
	
	char *to_join[] = { "events", "\n" };
	
	for (iter = 0; *to_join[iter] != '\n'; ++iter) {
		if (!(chan = find_slash_channel_by_name(to_join[iter], TRUE))) {
			chan = create_slash_channel(to_join[iter]);
		}
		if (!find_on_slash_channel(ch, chan->id)) {
			CREATE(slash, struct player_slash_channel, 1);
			LL_PREPEND(GET_SLASH_CHANNELS(ch), slash);
			slash->id = chan->id;
		}
	}
}


void b5_80_dailies_fix(void) {
	setup_daily_quest_cycles(10700);
	setup_daily_quest_cycles(16602);
	setup_daily_quest_cycles(16604);
	setup_daily_quest_cycles(16606);
	setup_daily_quest_cycles(16611);
	setup_daily_quest_cycles(16618);
}


// applies a new script to existing snowpersons
void b5_82_snowman_fix(void) {
	char_data *ch, *next_ch;
	trig_data *trig;
	bool has;
	
	int snowman_vnum = 16600;
	int new_script = 16634;
	
	DL_FOREACH_SAFE(character_list, ch, next_ch) {
		if (!IS_NPC(ch) || GET_MOB_VNUM(ch) != snowman_vnum) {
			continue;	// wrong mob
		}
		// check if it already has the new script
		has = FALSE;
		if (SCRIPT(ch)) {
			LL_FOREACH(TRIGGERS(SCRIPT(ch)), trig) {
				if (GET_TRIG_VNUM(trig) == new_script) {
					has = TRUE;
					break;
				}
			}
		}
		
		// attach if not
		if (!has && (trig = read_trigger(new_script))) {
			if (!SCRIPT(ch)) {
				create_script_data(ch, MOB_TRIGGER);
			}
			add_trigger(SCRIPT(ch), trig, -1);
			request_char_save_in_world(ch);
		}
	}
}


// update gear positions for new tool slot, which was inserted before the "shared" slot
// NOTE: it's generally not safe to insert slots in the middle due to both player gear sets
// and archetype gear sets
PLAYER_UPDATE_FUNC(b5_83_update_players) {
	obj_data *obj;
	
	check_delayed_load(ch);
	
	if ((obj = GET_EQ(ch, WEAR_TOOL))) {
		unequip_char(ch, WEAR_TOOL);
		equip_char(ch, obj, WEAR_SHARE);
	}
}


// climates changed from scalar ints to bitvectors
void b5_84_climate_update(void) {
	sector_data *sect, *next_sect;
	crop_data *crop, *next_crop;
	bool changed, any = FALSE;
	
	#define old_climate_temperate  1
	#define old_climate_arid  2
	#define old_climate_tropical  3
	
	HASH_ITER(hh, sector_table, sect, next_sect) {
		changed = FALSE;
		switch (GET_SECT_CLIMATE(sect)) {
			case old_climate_temperate: {
				GET_SECT_CLIMATE(sect) = CLIM_TEMPERATE;
				changed = TRUE;
				break;
			}
			case old_climate_arid: {
				GET_SECT_CLIMATE(sect) = CLIM_ARID;
				changed = TRUE;
				break;
			}
			case old_climate_tropical: {
				GET_SECT_CLIMATE(sect) = CLIM_TROPICAL;
				changed = TRUE;
				break;
			}
			// no default: no work if it's not one of those
		}
		if (changed) {
			any = TRUE;
			save_library_file_for_vnum(DB_BOOT_SECTOR, GET_SECT_VNUM(sect));
			log("- Sector [%d] %s", GET_SECT_VNUM(sect), GET_SECT_NAME(sect));
		}
	}
	
	HASH_ITER(hh, crop_table, crop, next_crop) {
		changed = FALSE;
		switch (GET_CROP_CLIMATE(crop)) {
			case old_climate_temperate: {
				GET_CROP_CLIMATE(crop) = CLIM_TEMPERATE;
				changed = TRUE;
				break;
			}
			case old_climate_arid: {
				GET_CROP_CLIMATE(crop) = CLIM_ARID;
				changed = TRUE;
				break;
			}
			case old_climate_tropical: {
				GET_CROP_CLIMATE(crop) = CLIM_TROPICAL;
				changed = TRUE;
				break;
			}
			// no default: no work if it's not one of those
		}
		if (changed) {
			any = TRUE;
			save_library_file_for_vnum(DB_BOOT_CROP, GET_CROP_VNUM(crop));
			log("- Crop [%d] %s", GET_CROP_VNUM(crop), GET_CROP_NAME(crop));
		}
	}
	
	if (!any) {
		log("- no old climates detected");
	}
}


// b5.86 refreshes missile weapons
PLAYER_UPDATE_FUNC(b5_86_player_missile_weapons) {
	obj_data *obj, *next_obj, *new;
	int iter;
	
	check_delayed_load(ch);
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if ((obj = GET_EQ(ch, iter)) && IS_MISSILE_WEAPON(obj)) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
	DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		if (IS_MISSILE_WEAPON(obj)) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
}


// removes 'crop' tiles from the 'natural sectors' of all tiles, but does not affect any current sectors
// also refreshes missile weapons
void b5_86_update(void) {
	obj_data *obj, *next_obj, *new;
	struct empire_unique_storage *eus;
	struct trading_post_data *tpd;
	empire_data *emp, *next_emp;
	struct map_data *map;
	int temp = 0, des = 0, jung = 0;
	
	any_vnum temperate_crop = 7;
	any_vnum desert_crop = 12;
	any_vnum jungle_crop = 16;
	
	sector_data *temperate_sect = sector_proto(4);	// overgrown forest
	sector_data *desert_sect = sector_proto(26);	// desert grove
	sector_data *jungle_sect = sector_proto(28);	// jungle
	
	// part 1:	
	log(" - refreshing the object list...");
	DL_FOREACH_SAFE(object_list, obj, next_obj) {
		if (IS_MISSILE_WEAPON(obj)) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
		}
	}
	
	log(" - refreshing warehouse objects...");
	HASH_ITER(hh, empire_table, emp, next_emp) {
		DL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
			if ((obj = eus->obj) && IS_MISSILE_WEAPON(obj)) {
				new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
				eus->obj = new;
				extract_obj(obj);
				EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
			}
		}
	}
	
	log(" - refreshing trading post objects...");
	DL_FOREACH(trading_list, tpd) {
		if ((obj = tpd->obj) && IS_MISSILE_WEAPON(obj)) {
			new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			tpd->obj = new;
			extract_obj(obj);
		}
	}
	
	log(" - refreshing player inventories...");
	
	// part 2:
	log("Removing crops from 'natural' sectors (but not current sectors)...");
	
	if (!temperate_sect) {
		log("- replacement temperate sector (4) is missing; canceling b5.86 update");
		return;
	}
	if (!desert_sect) {
		log("- replacement desert sector (26) is missing; canceling b5.86 update");
		return;
	}
	if (!jungle_sect) {
		log("- replacement jungle sector (28) is missing; canceling b5.86 update");
		return;
	}
	
	// update all map tiles
	LL_FOREACH(land_map, map) {
		if (!map->natural_sector || GET_SECT_VNUM(map->natural_sector) == temperate_crop) {
			set_natural_sector(map, temperate_sect);
			++temp;
		}
		else if (GET_SECT_VNUM(map->natural_sector) == desert_crop) {
			set_natural_sector(map, desert_sect);
			++des;
		}
		else if (GET_SECT_VNUM(map->natural_sector) == jungle_crop) {
			set_natural_sector(map, jungle_sect);
			++jung;
		}
	}
	
	log("- replaced natural sectors on %d temperate, %d desert, and %d jungle tile%s", temp, des, jung, PLURAL(temp+des+jung));
}


// removes 75% of unclaimed crops (previous patch removed wild crops in the map generator)
// also adds old-growth forests
void b5_87_crop_and_old_growth(void) {
	int removed_crop = 0, total_crop = 0, new_og = 0, total_forest = 0;
	struct empire_completed_goal *goal, *next_goal;
	empire_data *emp, *next_emp;
	struct map_data *map;
	room_data *room;
	bool has_og;
	
	any_vnum overgrown_forest = 4;	// sect to change
	any_vnum old_growth = 90;	// new sect
	
	has_og = (sector_proto(old_growth) ? TRUE : FALSE);
	
	LL_FOREACH(land_map, map) {
		if (SECT_FLAGGED(map->sector_type, SECTF_CROP) && map->crop_type && (room = real_room(map->vnum)) && !ROOM_OWNER(room)) {
			++total_crop;
			
			if (number(1,100) <= 75) {
				++removed_crop;
				// change to base sect
				uncrop_tile(room);
				
				// stop all possible chores here since the sector changed
				stop_room_action(room, ACT_HARVESTING);
				stop_room_action(room, ACT_CHOPPING);
				stop_room_action(room, ACT_PICKING);
				stop_room_action(room, ACT_GATHERING);
			}
		}
		else if (has_og && GET_SECT_VNUM(map->sector_type) == overgrown_forest && (room = real_room(map->vnum)) && !ROOM_OWNER(room)) {
			++total_forest;
			if (!number(0, 9)) {
				// 10% chance of becoming old-growth now
				++new_og;
				change_terrain(real_room(map->vnum), old_growth, NOTHING);
			}
			else {
				// otherwise, back-date their sector time 3-4 weeks
				set_extra_data(&map->shared->extra_data, ROOM_EXTRA_SECTOR_TIME, time(0) - number(1814400, 2419200));
			}
		}
	}
	
	log("- removed %d of %d unclaimed crop tiles", removed_crop, total_crop);
	log("- converted %d of %d Overgrown Forests to Old-Growth Forests", new_og, total_forest);
	
	log("- removing apiaries from the learned-list of empires with 2011 Foundations...");
	HASH_ITER(hh, empire_table, emp, next_emp) {
		HASH_ITER(hh, EMPIRE_COMPLETED_GOALS(emp), goal, next_goal) {
			// Some goals were changed in this patch. Need to update anybody who completed them.
			switch (goal->vnum) {
				case 2011: {	// Foundations: -craft
					remove_learned_craft_empire(emp, 5131, FALSE);	// apiary
					break;
				}
			}
		}
	}
}


/**
* This convertions the pre-b5.88 components to the new versions.
*
* @param int old_type The old component type, previously CMP_x.
* @param bitvector_t old_flags Any flags that were set on the old component, callled CMPF_x.
* @return any_vnum The new generic vnum component that matches it.
*/
any_vnum b5_88_old_component_to_new_component(int old_type, bitvector_t old_flags) {
	// first, we need the pre-b5.88 component types (ones not listed do not need conversion)
	const int cmp_BONE = 2, cmp_BLOCK = 3, cmp_CLAY = 4, cmp_FIBERS = 7,
		cmp_FLOUR = 8, cmp_FRUIT = 9, cmp_FUR = 10, cmp_GEM = 11,
		cmp_GRAIN = 12, cmp_HANDLE = 13, cmp_HERB = 14, cmp_LEATHER = 15,
		cmp_LUMBER = 16, cmp_MEAT = 17, cmp_METAL = 18, cmp_NAILS = 19,
		cmp_OIL = 20, cmp_PILLAR = 21, cmp_ROCK = 22, cmp_SEEDS = 23,
		cmp_SKIN = 24, cmp_SAPLING = 25, cmp_TEXTILE = 26, cmp_VEGETABLE = 27,
		cmp_ROPE = 28, cmp_PAINT = 29, cmp_WAX = 30, cmp_SWEETENER = 31,
		cmp_SAND = 32, cmp_GLASS = 33;
	// and old component flags
	const int cmpf_ANIMAL = BIT(0), cmpf_BUNCH = BIT(1), cmpf_DESERT = BIT(2),
		cmpf_FINE = BIT(3), cmpf_HARD = BIT(4), cmpf_LARGE = BIT(5),
		cmpf_MAGIC = BIT(6), cmpf_PLANT = BIT(8), cmpf_POOR = BIT(9),
		cmpf_RARE = BIT(10), cmpf_RAW = BIT(11), cmpf_REFINED = BIT(12),
		cmpf_SINGLE = BIT(13), cmpf_SMALL = BIT(14), cmpf_SOFT = BIT(15),
		cmpf_TEMPERATE = BIT(16), cmpf_TROPICAL = BIT(17),
		cmpf_COMMON = BIT(18), cmpf_AQUATIC = BIT(19);
	
	const struct {
		int type, flags;
		any_vnum new_comp;
	} b5_88_conversion[] = {
		// This will run in order from top to bottom, matching component +
		// exact flags, and returning the number on the right
		{ cmp_BONE, cmpf_LARGE | cmpf_MAGIC | cmpf_RARE, 6565 },	// dragon bone
		{ cmp_BONE, cmpf_LARGE | cmpf_RARE, 6563 },	// ivory
		{ cmp_BONE, cmpf_MAGIC, 6564 },	// magic bone
		{ cmp_BONE, NOBITS, 6560 },	// bone
		{ cmp_BLOCK, cmpf_BUNCH, 6076 },	// bricks
		{ cmp_BLOCK, cmpf_SINGLE, 6077 },	// large block
		{ cmp_BLOCK, NOBITS, 6075 },	// block
		{ cmp_CLAY, NOBITS, 6090 },	// clay (no variants)
		{ cmp_FIBERS, cmpf_HARD | cmpf_PLANT, 6422 },	// hard plant fibers
		{ cmp_FIBERS, cmpf_SOFT | cmpf_PLANT, 6421 },	// soft plant fibers
		{ cmp_FIBERS, cmpf_SOFT | cmpf_ANIMAL, 6441 },	// wool
		{ cmp_FIBERS, NOBITS, 6400 },	// basic fibers
		{ cmp_FLOUR, NOBITS, 6300 },	// basic flour
		{ cmp_FRUIT, cmpf_MAGIC | cmpf_SINGLE, 6121 },	// small magic fruit
		{ cmp_FRUIT, cmpf_MAGIC | cmpf_BUNCH, 6131 },	// large magic fruit
		{ cmp_FRUIT, cmpf_TEMPERATE | cmpf_SINGLE, 6122 },	// small temperate fruit
		{ cmp_FRUIT, cmpf_TEMPERATE | cmpf_BUNCH, 6132 },	// large temperate fruit
		{ cmp_FRUIT, cmpf_DESERT | cmpf_SINGLE, 6124 },	// small desert fruit
		{ cmp_FRUIT, cmpf_DESERT | cmpf_BUNCH, 6134 },	// large desert fruit
		{ cmp_FRUIT, cmpf_TROPICAL | cmpf_SINGLE, 6123 },	// small tropical fruit
		{ cmp_FRUIT, cmpf_TROPICAL | cmpf_BUNCH, 6133 },	// large tropical fruit
		{ cmp_FRUIT, cmpf_SINGLE, 6120 },	// small fruit
		{ cmp_FRUIT, cmpf_BUNCH, 6130 },	// large fruit
		{ cmp_FRUIT, NOBITS, 6120 },	// small fruit
		{ cmp_VEGETABLE, cmpf_MAGIC | cmpf_SINGLE, 6141 },	// small magic veg
		{ cmp_VEGETABLE, cmpf_MAGIC | cmpf_BUNCH, 6151 },	// large magic veg
		{ cmp_VEGETABLE, cmpf_TEMPERATE | cmpf_SINGLE, 6142 },	// small temperate veg
		{ cmp_VEGETABLE, cmpf_TEMPERATE | cmpf_BUNCH, 6152 },	// large temperate veg
		{ cmp_VEGETABLE, cmpf_DESERT | cmpf_SINGLE, 6144 },	// small desert veg
		{ cmp_VEGETABLE, cmpf_DESERT | cmpf_BUNCH, 6154 },	// large desert veg
		{ cmp_VEGETABLE, cmpf_TROPICAL | cmpf_SINGLE, 6143 },	// small tropical veg
		{ cmp_VEGETABLE, cmpf_TROPICAL | cmpf_BUNCH, 6153 },	// large tropical veg
		{ cmp_VEGETABLE, cmpf_SINGLE, 6140 },	// small veg
		{ cmp_VEGETABLE, cmpf_BUNCH, 6150 },	// large veg
		{ cmp_VEGETABLE, NOBITS, 6140 },	// small veg
		{ cmp_GRAIN, cmpf_SINGLE, 6160 },	// small grain
		{ cmp_GRAIN, cmpf_BUNCH, 6170 },	// large grain
		{ cmp_GRAIN, NOBITS, 6160 },	// small grain
		{ cmp_SEEDS, NOBITS, 6181 },	// edible seeds
		{ cmp_GEM, cmpf_REFINED, 6601 },	// powerful magic gem
		{ cmp_GEM, NOBITS, 6600 },	// magic gem
		{ cmp_HERB, cmpf_REFINED, 6651 },	// refined herb
		{ cmp_HERB, NOBITS, 6650 },	// herb
		{ cmp_SWEETENER, NOBITS, 6340 },	// basic sweetener
		{ cmp_OIL, NOBITS, 6320 },	// basic oil -- there are more types but no way to tell from flags
		{ cmp_GLASS, cmpf_RAW, 6831 },	// glass ingot
		{ cmp_GLASS, NOBITS, 6830 },	// glass
		{ cmp_SAND, NOBITS, 6085 },	// only 1 type of sand
		{ cmp_WAX, NOBITS, 6891 },	// basic wax
		{ cmp_PAINT, NOBITS, 6890 },	// basic paint
		{ cmp_ROPE, NOBITS, 6880 },	// basic rope
		{ cmp_HANDLE, cmpf_MAGIC, 6851 },	// magic handle
		{ cmp_HANDLE, NOBITS, 6850 },	// basic handle
		{ cmp_TEXTILE, cmpf_MAGIC, 6810 },	// magic cloth
		{ cmp_TEXTILE, NOBITS, 6800 },	// basic cloth
		{ cmp_NAILS, NOBITS, 6790 },	// only 1 type
		{ cmp_METAL, cmpf_POOR, 6710 },	// poor metal
		{ cmp_METAL, cmpf_COMMON, 6720 },	// common metal
		{ cmp_METAL, cmpf_HARD, 6740 },	// hardened metal
		{ cmp_METAL, cmpf_FINE, 6750 },	// precious metal 1
		{ cmp_METAL, cmpf_REFINED, 6750 },	// precious metal 2
		{ cmp_METAL, cmpf_MAGIC, 6760 },	// magic metal
		{ cmp_METAL, cmpf_RARE, 6730 },	// rare metal
		{ cmp_METAL, NOBITS, 6700 },	// basic metal
		{ cmp_ROCK, NOBITS, 6050 },	// basic rock
		{ cmp_SAPLING, cmpf_FINE, 6026 },	// fine sapling
		{ cmp_SAPLING, NOBITS, 6025 },	// basic sapling
		{ cmp_LUMBER, cmpf_MAGIC, 6001 },	// magic lumber
		{ cmp_LUMBER, cmpf_FINE, 6002 },	// fine lumber
		{ cmp_LUMBER, NOBITS, 6000 },	// basic lumber
		{ cmp_PILLAR, cmpf_LARGE, 6017 },	// large pillar
		{ cmp_PILLAR, cmpf_FINE, 6016 },	// fine pillar
		{ cmp_PILLAR, NOBITS, 6015 },	// basic pillar
		{ cmp_FUR, cmpf_LARGE, 6550 },	// large fur
		{ cmp_FUR, cmpf_SMALL, 6541 },	// small fur
		{ cmp_FUR, NOBITS, 6540 },	// basic fur
		{ cmp_SKIN, cmpf_MAGIC, 6511 },	// magic skin
		{ cmp_SKIN, cmpf_LARGE, 6510 },	// thick skin
		{ cmp_SKIN, cmpf_SMALL, 6501 },	// thin skin
		{ cmp_SKIN, NOBITS, 6500 },	// basic skin
		{ cmp_LEATHER, cmpf_MAGIC, 6531 },	// magic leather
		{ cmp_LEATHER, cmpf_LARGE, 6530 },	// thick leather
		{ cmp_LEATHER, cmpf_SMALL, 6521 },	// thin leather
		{ cmp_LEATHER, NOBITS, 6520 },	// basic leather
		{ cmp_MEAT, cmpf_AQUATIC, 6203 },	// raw fish
		{ cmp_MEAT, NOBITS, 6200 },	// raw meat
		
		{ -1, -1, -1 }	// LAST
	};
	
	int iter;
	
	for (iter = 0; b5_88_conversion[iter].type != -1; ++iter) {
		if (b5_88_conversion[iter].type == old_type && (b5_88_conversion[iter].flags == NOBITS || IS_SET_STRICT(old_flags, b5_88_conversion[iter].flags))) {
			return b5_88_conversion[iter].new_comp;
		}
	}
	
	// didn't find it? no valid component type
	return NOTHING;
}


// fixes tiles that fell prey to a now-fixed bug where the tile went: desert -> irrigated plains -> crop -> regular plains
void b5_88_irrigation_repair(void) {
	int fixed_current = 0, fixed_base = 0;
	struct map_data *map;
	
	// any tiles with current OR base sect in this range are trouble
		// plains and basic forests; jungle and swamp;
		// damaged jungle, marsh, stumps, copse, riverbank tiles;
		// shore, old-growth forest; shoreside/seaside tiles; jungle edge tiles;
		// enchanted forest; evergreen forests; goblin stumps; beaver flooding
	#define b588_TARGET_SECT(vnum)  ( \
		((vnum) >= 0 && (vnum) <= 4) || \
		((vnum) >= 27 && (vnum) <= 29) || \
		((vnum) >= 34 && (vnum) <= 47) || \
		(vnum) == 50 || (vnum) == 90 || \
		((vnum) >= 54 && (vnum) <= 56) || \
		((vnum) >= 59 && (vnum) <= 65) || \
		((vnum) >= 600 && (vnum) <= 604) || \
		((vnum) >= 10562 && (vnum) <= 10566) || \
		(vnum) == 18100 || \
		((vnum) >= 18451 && (vnum) <= 18452) \
	)
	// natural sect to look for (indicates the tile is a problem if it has this natural sect and one of the above current/base sect)
		// desert, oasis, grove
	#define b588_NATURAL_SECT(vnum)  ( \
		(vnum) == 20 || \
		(vnum) == 21 || \
		(vnum) == 26 \
	)
	
	LL_FOREACH(land_map, map) {
		if (!map->natural_sector || !b588_NATURAL_SECT(GET_SECT_VNUM(map->natural_sector))) {
			continue;	// doesn't have the natural sect we're looking for: skip
		}
		
		if (map->sector_type && b588_TARGET_SECT(GET_SECT_VNUM(map->sector_type))) {
			++fixed_current;
			change_terrain(real_room(map->vnum), GET_SECT_VNUM(map->natural_sector), NOTHING);
			// log(" - current: %d (%d, %d)", map->vnum, MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
		}
		else if (map->base_sector && b588_TARGET_SECT(GET_SECT_VNUM(map->base_sector))) {
			if (map->crop_type) {	// crop with bad base: remove crop
				++fixed_current;
				change_terrain(real_room(map->vnum), GET_SECT_VNUM(map->natural_sector), NOTHING);
				// log(" - current (crop): %d (%d, %d)", map->vnum, MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
			else {	// no crop -- only need to fix base
				++fixed_base;
				change_base_sector(real_room(map->vnum), map->natural_sector);
				// log(" - base: %d (%d, %d)", map->vnum, MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
		}
	}
	
	log("- repaired %d current and %d base sectors", fixed_current, fixed_base);
}


// updates 
void b5_88_resource_components_update(void) {
	any_vnum b5_88_old_component_to_new_component(int old_type, bitvector_t old_flags);
	
	craft_data *craft, *next_craft;
	progress_data *prg, *next_prg;
	vehicle_data *veh, *next_veh;
	augment_data *aug, *next_aug;
	social_data *soc, *next_soc;
	int this_block, last_block;
	quest_data *qst, *next_qst;
	struct resource_data *res;
	bld_data *bld, *next_bld;
	obj_data *obj, *next_obj;
	struct req_data *req;
	any_vnum vn;
	bool any;
	
	// augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		any = FALSE;
		LL_FOREACH(GET_AUG_RESOURCES(aug), res) {
			if (res->type == RES_COMPONENT && res->vnum < 100) {
				if ((vn = b5_88_old_component_to_new_component(res->vnum, res->misc)) != NOTHING) {
					log("- converting resource on augment [%d] %s from (%d %s) to [%d] %s", GET_AUG_VNUM(aug), GET_AUG_NAME(aug), res->vnum, bitv_to_alpha(res->misc), vn, get_generic_name_by_vnum(vn));
				}
				else {
					log("- unable to convert resource on augment [%d] %s from (%d %s)", GET_AUG_VNUM(aug), GET_AUG_NAME(aug), res->vnum, bitv_to_alpha(res->misc));
				}
				res->vnum = vn;
				res->misc = 0;
				any = TRUE;
			}
		}
		if (any) {
			save_library_file_for_vnum(DB_BOOT_AUG, GET_AUG_VNUM(aug));
		}
	}
	
	// buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		any = FALSE;
		LL_FOREACH(GET_BLD_REGULAR_MAINTENANCE(bld), res) {
			if (res->type == RES_COMPONENT && res->vnum < 100) {
				if ((vn = b5_88_old_component_to_new_component(res->vnum, res->misc)) != NOTHING) {
					log("- converting resource on building [%d] %s from (%d %s) to [%d] %s", GET_BLD_VNUM(bld), GET_BLD_NAME(bld), res->vnum, bitv_to_alpha(res->misc), vn, get_generic_name_by_vnum(vn));
				}
				else {
					log("- unable to convert resource on building [%d] %s from (%d %s)", GET_BLD_VNUM(bld), GET_BLD_NAME(bld), res->vnum, bitv_to_alpha(res->misc));
				}
				res->vnum = vn;
				res->misc = 0;
				any = TRUE;
			}
		}
		if (any) {
			save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
		}
	}
	
	// crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		any = FALSE;
		LL_FOREACH(GET_CRAFT_RESOURCES(craft), res) {
			if (res->type == RES_COMPONENT && res->vnum < 100) {
				if ((vn = b5_88_old_component_to_new_component(res->vnum, res->misc)) != NOTHING) {
					log("- converting resource on craft [%d] %s from (%d %s) to [%d] %s", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft), res->vnum, bitv_to_alpha(res->misc), vn, get_generic_name_by_vnum(vn));
				}
				else {
					log("- unable to convert resource on craft [%d] %s from (%d %s)", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft), res->vnum, bitv_to_alpha(res->misc));
				}
				res->vnum = vn;
				res->misc = 0;
				any = TRUE;
			}
		}
		if (any) {
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// progress goals
	HASH_ITER(hh, progress_table, prg, next_prg) {
		any = FALSE;
		LL_FOREACH(PRG_TASKS(prg), req) {
			if ((req->type == REQ_GET_COMPONENT || req->type == REQ_EMPIRE_PRODUCED_COMPONENT) && req->vnum < 100) {
				if ((vn = b5_88_old_component_to_new_component(req->vnum, req->misc)) != NOTHING) {
					log("- converting task on progress [%d] %s from (%d %s) to [%d] %s", PRG_VNUM(prg), PRG_NAME(prg), req->vnum, bitv_to_alpha(req->misc), vn, get_generic_name_by_vnum(vn));
				}
				else {
					log("- unable to convert task on progress [%d] %s from (%d %s)", PRG_VNUM(prg), PRG_NAME(prg), req->vnum, bitv_to_alpha(req->misc));
				}
				req->vnum = vn;
				req->misc = 0;
				any = TRUE;
			}
		}
		if (any) {
			PRG_VERSION(prg) += 1;	// triggers updates for empires
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
		}
	}
	
	// quests
	HASH_ITER(hh, quest_table, qst, next_qst) {
		any = FALSE;
		LL_FOREACH(QUEST_TASKS(qst), req) {
			if ((req->type == REQ_GET_COMPONENT || req->type == REQ_EMPIRE_PRODUCED_COMPONENT) && req->vnum < 100) {
				if ((vn = b5_88_old_component_to_new_component(req->vnum, req->misc)) != NOTHING) {
					log("- converting task on quest task [%d] %s from (%d %s) to [%d] %s", QUEST_VNUM(qst), QUEST_NAME(qst), req->vnum, bitv_to_alpha(req->misc), vn, get_generic_name_by_vnum(vn));
				}
				else {
					log("- unable to convert task on quest task [%d] %s from (%d %s)", QUEST_VNUM(qst), QUEST_NAME(qst), req->vnum, bitv_to_alpha(req->misc));
				}
				req->vnum = vn;
				req->misc = 0;
				any = TRUE;
			}
		}
		LL_FOREACH(QUEST_PREREQS(qst), req) {
			if ((req->type == REQ_GET_COMPONENT || req->type == REQ_EMPIRE_PRODUCED_COMPONENT) && req->vnum < 100) {
				if ((vn = b5_88_old_component_to_new_component(req->vnum, req->misc)) != NOTHING) {
					log("- converting task on quest prereq [%d] %s from (%d %s) to [%d] %s", QUEST_VNUM(qst), QUEST_NAME(qst), req->vnum, bitv_to_alpha(req->misc), vn, get_generic_name_by_vnum(vn));
				}
				else {
					log("- unable to convert task on quest prereq [%d] %s from (%d %s)", QUEST_VNUM(qst), QUEST_NAME(qst), req->vnum, bitv_to_alpha(req->misc));
				}
				req->vnum = vn;
				req->misc = 0;
				any = TRUE;
			}
		}
		if (any) {
			QUEST_VERSION(qst) += 1;	// triggers updates for players
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(qst));
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		any = FALSE;
		LL_FOREACH(SOC_REQUIREMENTS(soc), req) {
			if ((req->type == REQ_GET_COMPONENT || req->type == REQ_EMPIRE_PRODUCED_COMPONENT) && req->vnum < 100) {
				if ((vn = b5_88_old_component_to_new_component(req->vnum, req->misc)) != NOTHING) {
					log("- converting requirement on social [%d] %s from (%d %s) to [%d] %s", SOC_VNUM(soc), SOC_NAME(soc), req->vnum, bitv_to_alpha(req->misc), vn, get_generic_name_by_vnum(vn));
				}
				else {
					log("- unable to convert requirement on social [%d] %s from (%d %s)", SOC_VNUM(soc), SOC_NAME(soc), req->vnum, bitv_to_alpha(req->misc));
				}
				req->vnum = vn;
				req->misc = 0;
				any = TRUE;
			}
		}
		if (any) {
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		any = FALSE;
		LL_FOREACH(VEH_REGULAR_MAINTENANCE(veh), res) {
			if (res->type == RES_COMPONENT && res->vnum < 100) {
				if ((vn = b5_88_old_component_to_new_component(res->vnum, res->misc)) != NOTHING) {
					log("- converting resource on vehicle [%d] %s from (%d %s) to [%d] %s", VEH_VNUM(veh), VEH_SHORT_DESC(veh), res->vnum, bitv_to_alpha(res->misc), vn, get_generic_name_by_vnum(vn));
				}
				else {
					log("- unable to convert resource on vehicle [%d] %s from (%d %s)", VEH_VNUM(veh), VEH_SHORT_DESC(veh), res->vnum, bitv_to_alpha(res->misc));
				}
				res->vnum = vn;
				res->misc = 0;
				any = TRUE;
			}
		}
		if (any) {
			save_library_file_for_vnum(DB_BOOT_VEH, VEH_VNUM(veh));
		}
	}
	
	// lastly, force a full save of all objects -- because their component types were converted when they loaded
	last_block = -1;
	HASH_ITER(hh, object_table, obj, next_obj) {
		this_block = GET_OBJ_VNUM(obj) / 100;
		if (this_block != last_block) {
			last_block = this_block;
			save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
		}
	}
	
	need_progress_refresh = TRUE;
	check_progress_refresh();
}


// addendum to b5_88_resource_components_update to fix maintenance/repair lists
void b5_88_maintenance_fix(void) {
	any_vnum b5_88_old_component_to_new_component(int old_type, bitvector_t old_flags);
	
	int fixed_veh = 0, fixed_rm = 0;
	room_data *room, *next_room;
	struct resource_data *res;
	vehicle_data *veh;
	any_vnum vn;
	bool any;
	
	HASH_ITER(hh, world_table, room, next_room) {
		any = FALSE;
		LL_FOREACH(BUILDING_RESOURCES(room), res) {
			if (res->type == RES_COMPONENT && res->vnum < 100) {
				if ((vn = b5_88_old_component_to_new_component(res->vnum, res->misc)) != NOTHING) {
					res->vnum = vn;
				}
				else {
					res->vnum = COMP_NAILS;	// safe backup
				}
				res->misc = 0;
				any = TRUE;
			}
		}
		if (any) {
			++fixed_rm;
		}
	}
	
	DL_FOREACH(vehicle_list, veh) {
		any = FALSE;
		LL_FOREACH(VEH_NEEDS_RESOURCES(veh), res) {
			if (res->type == RES_COMPONENT && res->vnum < 100) {
				if ((vn = b5_88_old_component_to_new_component(res->vnum, res->misc)) != NOTHING) {
					res->vnum = vn;
				}
				else {
					res->vnum = COMP_NAILS;	// safe backup
				}
				res->misc = 0;
				any = TRUE;
			}
		}
		if (any) {
			++fixed_veh;
		}
	}
	
	log("- fixed %d vehicle%s and %d building%s", fixed_veh, PLURAL(fixed_veh), fixed_rm, PLURAL(fixed_rm));
}


void b5_94_terrain_heights(void) {
	int x, y, dir;
	bool any;
	
	struct temp_dat_t {
		struct map_data *loc;
		int height;
		struct temp_dat_t *prev, *next;	// doubly-linked list
	} *tdt, *big_queue = NULL, *river_queue = NULL, *mountain_queue = NULL;
	
	#define queue_tdt(queue, map, ht)  { struct temp_dat_t *temp; CREATE(temp, struct temp_dat_t, 1); temp->loc = &(map); temp->height = (ht); DL_APPEND(queue, temp); }
	
	// #define b594_is_water(sect)  (IS_SET(GET_SECT_CLIMATE(sect), CLIM_RIVER | CLIM_LAKE) || GET_SECT_VNUM(sect) == 32 || GET_SECT_VNUM(sect) == 33 || GET_SECT_VNUM(sect) == 5 || GET_SECT_VNUM(sect) == 10550)
	
	// make sure we need to do this
	any = FALSE;
	for (x = 0; x < MAP_WIDTH && !any; ++x) {
		for (y = 0; y < MAP_HEIGHT && !any; ++y) {
			if (world_map[x][y].shared->height > 0) {
				any = TRUE;
			}
		}
	}
	if (any) {
		log("- map already has height data; no work to do");
		return;
	}

	// queue all non-mountain/rivers to start with AND reset all heights to INT_MAX
	for (x = 0; x < MAP_WIDTH; ++x) {
		for (y = 0; y < MAP_HEIGHT; ++y) {
			world_map[x][y].shared->height = INT_MAX;
			if (!SECT_FLAGGED(world_map[x][y].base_sector, SECTF_NEEDS_HEIGHT)) {
				queue_tdt(big_queue, world_map[x][y], 0);
			}
		}
	}
	if (!big_queue) {
		log("- unable to find non-river/mountain tiles to start");
		return;
	}
	
	log("- processing flat land...");
	
	// use a queue for a breadth-first search
	while ((tdt = big_queue)) {
		if (tdt->loc->shared->height == INT_MAX) {
			// anything in the big queue is flat
			tdt->loc->shared->height = 0;
			
			// check neighbors
			for (dir = 0; dir < NUM_2D_DIRS; ++dir) {
				if (!get_coord_shift(MAP_X_COORD(tdt->loc->vnum), MAP_Y_COORD(tdt->loc->vnum), shift_dir[dir][0], shift_dir[dir][1], &x, &y)) {
					continue;	// no neighbor in this direction
				}
				if (world_map[x][y].shared->height != INT_MAX) {
					continue;	// already checked
				}
				if (!world_map[x][y].base_sector) {
					continue;	// don't think this is possible but best to be safe
				}
				
				// now queue based on type
				if (IS_SET(GET_SECT_CLIMATE(world_map[x][y].base_sector), CLIM_MOUNTAIN)) {
					// detect any mountain edge
					queue_tdt(mountain_queue, world_map[x][y], 1);
				}
				else if (IS_SET(GET_SECT_CLIMATE(tdt->loc->base_sector), CLIM_OCEAN) && SECT_FLAGGED(world_map[x][y].base_sector, SECTF_NEEDS_HEIGHT)) {
					// detect river touching ocean
					queue_tdt(river_queue, world_map[x][y], -1);
				}
				else {
					// neither mountain nor start-of-river -- ignore it; it's probably already in the queue
				}
			}
		}
		
		DL_DELETE(big_queue, tdt);
		free(tdt);
	}
	
	log("- detecting mountain heights...");
	
	// work on mountains from the outside in
	while ((tdt = mountain_queue)) {
		if (tdt->loc->shared->height == INT_MAX) {
			// assign height
			tdt->loc->shared->height = tdt->height;
			
			// check neighbors
			for (dir = 0; dir < NUM_2D_DIRS; ++dir) {
				if (!get_coord_shift(MAP_X_COORD(tdt->loc->vnum), MAP_Y_COORD(tdt->loc->vnum), shift_dir[dir][0], shift_dir[dir][1], &x, &y)) {
					continue;	// no neighbor in this direction
				}
				if (world_map[x][y].shared->height != INT_MAX) {
					continue;	// already checked
				}
				if (!world_map[x][y].base_sector) {
					continue;	// don't think this is possible but best to be safe
				}
				
				if (IS_SET(GET_SECT_CLIMATE(world_map[x][y].base_sector), CLIM_MOUNTAIN)) {
					// work up the mountain
					queue_tdt(mountain_queue, world_map[x][y], tdt->height + 1);
				}
			}
		}
		
		DL_DELETE(mountain_queue, tdt);
		free(tdt);
	}
	
	log("- detecting river heights...");
	
	// work on rivers upstream
	while ((tdt = river_queue)) {
		if (tdt->loc->shared->height == INT_MAX) {
			// assign height
			tdt->loc->shared->height = tdt->height;
			
			// check neighbors
			for (dir = 0; dir < NUM_2D_DIRS; ++dir) {
				if (!get_coord_shift(MAP_X_COORD(tdt->loc->vnum), MAP_Y_COORD(tdt->loc->vnum), shift_dir[dir][0], shift_dir[dir][1], &x, &y)) {
					continue;	// no neighbor in this direction
				}
				if (world_map[x][y].shared->height != INT_MAX) {
					continue;	// already checked
				}
				if (!world_map[x][y].base_sector) {
					continue;	// don't think this is possible but best to be safe
				}
				
				if (!IS_SET(GET_SECT_CLIMATE(world_map[x][y].base_sector), CLIM_MOUNTAIN) && SECT_FLAGGED(world_map[x][y].base_sector, SECTF_NEEDS_HEIGHT)) {
					// work up the river
					queue_tdt(river_queue, world_map[x][y], tdt->height - 1);
				}
			}
		}
		
		DL_DELETE(river_queue, tdt);
		free(tdt);
	}
	
	// tiles are 'missed' if they do not connect to the sea
	log("- checking for rivers/lakes with no height...");
	for (x = 0; x < MAP_WIDTH; ++x) {
		for (y = 0; y < MAP_HEIGHT; ++y) {
			if (world_map[x][y].shared->height == INT_MAX) {
				// log("  - (%d, %d) %s: not connected to ocean", x, y, GET_SECT_NAME(world_map[x][y].base_sector));
				world_map[x][y].shared->height = 0;
			}
		}
	}
}


// b5.99 replaces chant of magic/druids with a pair of triggers, which must be
// attached to all live buildings
void b5_99_henge_triggers(void) {
	const any_vnum main_trig = 5142, second_trig = 5143;
	struct trig_proto_list *tpl;
	room_data *room, *next_room;
	int count = 0;
	
	if (!real_trigger(main_trig) || !real_trigger(second_trig)) {
		log("- b5.99 update skipped because trigs %d and %d don't exist", main_trig, second_trig);
	}
	
	HASH_ITER(hh, world_table, room, next_room) {
		if (!GET_BUILDING(room)) {
			continue;
		}
		
		switch (GET_BLD_VNUM(GET_BUILDING(room))) {
			case 5142:	// regular henge
			case 12671: {	// wood henge
				CREATE(tpl, struct trig_proto_list, 1);
				tpl->vnum = main_trig;
				LL_APPEND(room->proto_script, tpl);
				
				CREATE(tpl, struct trig_proto_list, 1);
				tpl->vnum = second_trig;
				LL_APPEND(room->proto_script, tpl);
				
				assign_triggers(room, WLD_TRIGGER);
				++count;
				break;
			}
		}
	}
	
	log("- updated %d henge%s", count, PLURAL(count));
}


// b5.102: remove home chests and auto-store private homes
bool override_home_storage_cap = FALSE;	// this ensures nobody loses items during this patch

void b5_102_home_cleanup(void) {
	room_data *room, *next_room;
	obj_data *obj, *next_obj;
	
	obj_vnum o_HOME_CHEST = 1010;	// the item to remove
	
	override_home_storage_cap = TRUE;
	
	// dump out chests...
	DL_FOREACH_SAFE(object_list, obj, next_obj) {
		if (GET_OBJ_VNUM(obj) == o_HOME_CHEST) {
			empty_obj_before_extract(obj);
			extract_obj(obj);
		}
	}
	
	// autostore homes
	HASH_ITER(hh, world_table, room, next_room) {
		if (ROOM_PRIVATE_OWNER(HOME_ROOM(room)) != NOBODY) {
			DL_FOREACH_SAFE2(ROOM_CONTENTS(room), obj, next_obj, next_content) {
				perform_autostore(obj, ROOM_OWNER(room), NO_ISLAND);
			}
		}
	}
	
	override_home_storage_cap = FALSE;
}


// b5.103 removes the REPAIR-VEHICLES workforce chore
void b5_103_update(void) {
	empire_data *emp, *next_emp;
	
	const int CHORE_REPAIR_VEHICLES = 26;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		// this chore is gone
		set_workforce_limit_all(emp, CHORE_REPAIR_VEHICLES, 0);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}


// b5.104 removes the ROOM_EXTRA_RUINS_ICON data
void b5_104_update(void) {
	struct map_data *map;
	
	const int ROOM_EXTRA_RUINS_ICON = 7;
	
	LL_FOREACH(land_map, map) {
		remove_extra_data(&map->shared->extra_data, ROOM_EXTRA_RUINS_ICON);
	}
}


// b5.105 modifies workforce configs due to chore changes
void b5_105_update(void) {
	empire_data *emp, *next_emp;
	
	int CHORE_BRICKMAKING = 13;
	int CHORE_NEXUS_CRYSTALS = 24;
	int CHORE_GLASSMAKING = 32;
	int CHORE_TRAPPING = 17;
	int CHORE_BEEKEEPING = 31;
	int CHORE_HERB_GARDENING = 15;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		set_workforce_limit_all(emp, CHORE_BRICKMAKING, 0);
		set_workforce_limit_all(emp, CHORE_NEXUS_CRYSTALS, 0);
		set_workforce_limit_all(emp, CHORE_GLASSMAKING, 0);
		set_workforce_limit_all(emp, CHORE_TRAPPING, 0);
		set_workforce_limit_all(emp, CHORE_BEEKEEPING, 0);
		set_workforce_limit_all(emp, CHORE_HERB_GARDENING, 0);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}


// b5.106 (1/2) loads players and re-saves them to move equipment to the delay file
PLAYER_UPDATE_FUNC(b5_106_players) {
	check_delayed_load(ch);
}


// b5.106 (2/2) fixes some icon errors: a recent patch allowed icons to have too many backslashes
void b5_106_update(void) {
	struct map_data *map;
	int iter, diff;
	char *icon;
	
	LL_FOREACH(land_map, map) {
		if ((icon = map->shared->icon) && color_strlen(icon) > 4 && strstr(icon, "\\\\")) {
			// reduce double-slashes
			for (iter = diff = 0; iter <= strlen(icon); ++iter) {
				if (icon[iter] == '\\' && icon[iter+1] == '\\') {
					++diff;	// causes a skip
				}
				else if (iter > 0 && diff > 0) {	// copy backwards
					icon[iter-diff] = icon[iter];
				}
			}
		}
	}
}


// b5.107 copies players' personal lastname to current lastname (new split feature)
PLAYER_UPDATE_FUNC(b5_107_players) {
	if (GET_PERSONAL_LASTNAME(ch) && !GET_CURRENT_LASTNAME(ch)) {
		GET_CURRENT_LASTNAME(ch) = str_dup(GET_PERSONAL_LASTNAME(ch));
	}
}


// repairs bad dedicate ids on rooms
void b5_112_update(void) {
	struct map_data *map;
	room_data *room;
	
	LL_FOREACH(land_map, map) {
		if (!(room = map->room) || !ROOM_BLD_FLAGGED(room, BLD_DEDICATE)) {
			remove_extra_data(&map->shared->extra_data, ROOM_EXTRA_DEDICATE_ID);
		}
	}
	
	DL_FOREACH2(interior_room_list, room, next_interior) {
		if (!ROOM_BLD_FLAGGED(room, BLD_DEDICATE)) {
			remove_room_extra_data(room, ROOM_EXTRA_DEDICATE_ID);
		}
	}
}


// b5.117: clear last_christmas_gift_item
PLAYER_UPDATE_FUNC(b5_117_update_players) {
	struct trig_var_data *var, *next_var;
	
	check_delayed_load(ch);
	
	if (SCRIPT(ch) && SCRIPT(ch)->global_vars) {
		LL_FOREACH_SAFE(SCRIPT(ch)->global_vars, var, next_var) {
			if (!strcmp(var->name, "last_christmas_gift_item")) {
				LL_DELETE(SCRIPT(ch)->global_vars, var);
				free_var_el(var);
			}
		}
	}
}


// b5.119: repair bad terrain heights
void b5_119_repair_heights(void) {
	struct map_data *map;
	
	LL_FOREACH(land_map, map) {
		if (map->shared->height != 0 && SECT_FLAGGED(map->sector_type, SECTF_KEEPS_HEIGHT) && !SECT_FLAGGED(map->sector_type, SECTF_NEEDS_HEIGHT) && !SECT_FLAGGED(map->base_sector, SECTF_NEEDS_HEIGHT)) {
			// tile gained height through a pre-b5.119 bug and shouldn't have it
			if (map->room) {
				set_room_height(map->room, 0);
			}
			else {
				map->shared->height = 0;
				request_world_save(map->vnum, WSAVE_MAP);
			}
		}
	}
}


// b5.120: re-save whole world to fix junk ocean files
void b5_120_resave_world(void) {
	bool write_map_and_room_to_file(room_vnum vnum, bool force_obj_pack);
	
	int iter;
	
	// this actually ensures there are no stray files by calling writes on ones that shouldn't exist
	for (iter = 0; iter <= top_of_world_index; ++iter) {
		write_map_and_room_to_file(iter, FALSE);
	}
}


// b5.121: set default informative flags
PLAYER_UPDATE_FUNC(b5_121_update_players) {
	check_delayed_load(ch);
	SET_BIT(GET_INFORMATIVE_FLAGS(ch), DEFAULT_INFORMATIVE_BITS);
}


// this is needed for update_learned_recipes
int (*updatable_recipe_list)[][2] = NULL;


// warning: call this ONLY inside updatE_learned_recipes
PLAYER_UPDATE_FUNC(update_learned_recipes_player) {
	struct player_craft_data *pcd;
	int iter, vnum;
	
	if (!updatable_recipe_list) {
		return;
	}
	
	check_delayed_load(ch);
	
	for (iter = 0; (*updatable_recipe_list)[iter][0] != -1; ++iter) {
		vnum = (*updatable_recipe_list)[iter][0];
		HASH_FIND_INT(GET_LEARNED_CRAFTS(ch), &vnum, pcd);
		if (pcd) {
			HASH_DEL(GET_LEARNED_CRAFTS(ch), pcd);
			pcd->vnum = (*updatable_recipe_list)[iter][1];
			HASH_ADD_INT(GET_LEARNED_CRAFTS(ch), vnum, pcd);
		}
	}
}


/**
* Swaps one learned recipe for another, e.g. for building upgrades.
* This updates:
* - empire learned
* - player learned
*
* @param int (*list)[][2] an array of int pairs { from-vnum, to-vnum } which MUST end with { -1, -1 }
*/
void update_learned_recipes(int (*list)[][2]) {
	struct player_craft_data *pcd;
	empire_data *emp, *next_emp;
	int iter, vnum;
	
	if (!list || !*list) {
		return;
	}
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		for (iter = 0; (*list)[iter][0] != -1; ++iter) {
			vnum = (*list)[iter][0];
			HASH_FIND_INT(EMPIRE_LEARNED_CRAFTS(emp), &vnum, pcd);
			if (pcd) {
				HASH_DEL(EMPIRE_LEARNED_CRAFTS(emp), pcd);
				pcd->vnum = (*list)[iter][1];
				HASH_ADD_INT(EMPIRE_LEARNED_CRAFTS(emp), vnum, pcd);
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
			}
		}
	}
	
	// store to global
	updatable_recipe_list = list;
	update_all_players(NULL, update_learned_recipes_player);
}


// b5.128: swap out learned recipes
void b5_128_learned_update(void) {
	// update list
	static int list[][2] = {
		{ 5161, 5300 },
		{ -1, -1 }
	};
	
	update_learned_recipes(&list);
}


obj_data *b5_130b_check_replace_obj(obj_data *obj) {
	obj_data *new_obj = NULL;
	trig_data *trig;
	bool found;
	int iter;
	
	// any vnum in this list triggers a new copy
	obj_vnum stop_list[] = { 10710, 256, 262, 10730, 10732, 10734, 10735, 10737, 10739, 10741, 10744, 10746, -1 };
	obj_vnum potion_fix = 2926;
	any_vnum stop_trig = 9806;
	
	if (GET_OBJ_VNUM(obj) == potion_fix) {
		found = FALSE;
		if (!SCRIPT(obj)) {
			create_script_data(obj, OBJ_TRIGGER);
		}
		LL_FOREACH(TRIGGERS(SCRIPT(obj)), trig) {
			if (GET_TRIG_VNUM(trig) == potion_fix) {
				found = TRUE;
			}
		}
		// add if needed
		if (!found && (trig = read_trigger(potion_fix))) {
			add_trigger(SCRIPT(obj), trig, -1);
		}
	}
	else {
		for (iter = 0; stop_list[iter] != -1; ++iter) {
			if (GET_OBJ_VNUM(obj) == stop_list[iter]) {
				// copy obj
				new_obj = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj), TRUE, TRUE);
			
				// check scripts
				found = FALSE;
				if (!SCRIPT(new_obj)) {
					create_script_data(new_obj, OBJ_TRIGGER);
				}
				LL_FOREACH(TRIGGERS(SCRIPT(new_obj)), trig) {
					if (GET_TRIG_VNUM(trig) == stop_trig) {
						found = TRUE;
					}
				}
				// add if needed
				if (!found && (trig = read_trigger(stop_trig))) {
					add_trigger(SCRIPT(new_obj), trig, -1);
				}
				
				// only need 1
				break;
			}
		}
	}
	
	return new_obj;	// if any
}

// b5.130b: fix a small number of items that have scripts they need attached
void b5_130b_item_refresh(void) {
	obj_data *obj, *next_obj, *new_obj;
	struct empire_unique_storage *eus;
	struct trading_post_data *tpd;
	empire_data *emp, *next_emp;
	
	// part 1:	
	log(" - refreshing the object list...");
	DL_FOREACH_SAFE(object_list, obj, next_obj) {
		if ((new_obj = b5_130b_check_replace_obj(obj))) {
			swap_obj_for_obj(obj, new_obj);
			extract_obj(obj);
		}
	}
	
	log(" - refreshing warehouse objects...");
	HASH_ITER(hh, empire_table, emp, next_emp) {
		DL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
			if ((obj = eus->obj) && (new_obj = b5_130b_check_replace_obj(obj))) {
				eus->obj = new_obj;
				extract_obj(obj);
				EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
			}
		}
	}
	
	log(" - refreshing trading post objects...");
	DL_FOREACH(trading_list, tpd) {
		if ((obj = tpd->obj) && (new_obj = b5_130b_check_replace_obj(obj))) {
			tpd->obj = new_obj;
			extract_obj(obj);
		}
	}
}


// b5.130b: fix a small number of items that have scripts they need attached
PLAYER_UPDATE_FUNC(b5_130b_player_refresh) {
	obj_data *obj, *next_obj, *new_obj;
	int pos;
	
	check_delayed_load(ch);
	
	for (pos = 0; pos < NUM_WEARS; ++pos) {
		if ((obj = GET_EQ(ch, pos)) && (new_obj = b5_130b_check_replace_obj(obj))) {
			swap_obj_for_obj(obj, new_obj);
			extract_obj(obj);
		}
	}
	DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		if ((new_obj = b5_130b_check_replace_obj(obj))) {
			swap_obj_for_obj(obj, new_obj);
			extract_obj(obj);
		}
	}
}


// b5.134: clear map memory for screenreader users
PLAYER_UPDATE_FUNC(b5_134_update_players) {
	if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
		load_map_memory(ch);
		while (GET_MAP_MEMORY(ch)) {
			delete_player_map_memory(GET_MAP_MEMORY(ch), ch);
		}
		// mark these loaded in order to ensure a save (it can be skipped here if they are skill-swapped out of Cartography)
		GET_MAP_MEMORY_LOADED(ch) = TRUE;
		GET_MAP_MEMORY_NEEDS_SAVE(ch) = TRUE;
	}
}


// b5.151: apply hide-real-name flag to customized roads
void b5_151_road_fix(void) {
	struct map_data *map;
	room_data *room;
	
	LL_FOREACH(land_map, map) {
		if (SECT_FLAGGED(map->sector_type, SECTF_IS_ROAD) && map->shared->name) {
			room = map->room ? map->room : real_room(map->vnum);
			if (room) {
				SET_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_HIDE_REAL_NAME);
				affect_total_room(room);
				request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
			}
		}
	}
}


// b5.151 part 2: Repair sectors changed by adventures 104 and 18450, and update new terrains
void b5_151_terrain_fix(void) {
	room_data *room;
	struct map_data *map;
	int changed_sect = 0, changed_base = 0, changed_bld = 0;
	sector_vnum to_sect, to_base, trench_original;
	bool remove_bld;
	
	// sector vnums in use at the time of this patch
	#define b5151_PLAINS  0
	#define b5151_FOREST_1  1
	#define b5151_FOREST_2  2
	#define b5151_FOREST_3  3
	#define b5151_FOREST_4  4
	#define b5151_RIVER  5
	#define b5151_CROP  7
	#define b5151_MOUNTAIN  8
	#define b5151_ROAD  9
	#define b5151_BUILDING  10
	#define b5151_DESERT_CROP  12
	#define b5151_SEEDED_FIELD  13
	#define b5151_SEEDED_DESERT  14
	#define b5151_TRENCH  17
	#define b5151_CANAL  19
	#define b5151_DESERT  20
	#define b5151_OASIS  21
	#define b5151_SANDY_TRENCH  22
	#define b5151_DESERT_STUMPS  23
	#define b5151_DESERT_COPSE  24
	#define b5151_DESERT_SHRUB  25
	#define b5151_GROVE  26
	#define b5151_STUMPS  36
	#define b5151_COPSE_1  37
	#define b5151_COPSE_2  38
	#define b5151_FOREST_EDGE  39
	#define b5151_RIVERBANK  40
	#define b5151_FLOODPLAINS  41
	#define b5151_FLOODED_WOODS  42
	#define b5151_FLOODED_FOREST  43
	#define b5151_LIGHT_RIVERBANK_FOREST  44
	#define b5151_FORESTED_RIVERBANK  45
	#define b5151_STUMPED_RIVERBANK  46
	#define b5151_RIVERSIDE_COPSE  47
	#define b5151_SHORE  50
	#define b5151_BEACH  51
	#define b5151_CLIFFS  52
	#define b5151_ESTUARY  53
	#define b5151_SHORESIDE_TREE  54
	#define b5151_SEASIDE_STUMPS  59
	#define b5151_IRRIGATED_FIELD  70
	#define b5151_IRRIGATED_FOREST  71
	#define b5151_IRRIGATED_JUNGLE  72
	#define b5151_IRRIGATED_STUMPS  73
	#define b5151_IRRIGATED_COPSE  74
	#define b5151_IRRIGATED_JUNGLE_STUMPS  75
	#define b5151_IRRIGATED_JUNGLE_COPSE  76
	#define b5151_IRRIGATED_PLANTED_FIELD  77
	#define b5151_IRRIGATED_CROP  78
	#define b5151_DRY_OASIS  82
	#define b5151_PLANTED_OASIS  83
	#define b5151_OASIS_CROP  84
	#define b5151_IRRIGATION_CANAL  85
	#define b5151_DAMP_TRENCH  86
	#define b5151_VERDANT_CANAL  87
	#define b5151_IRRIGATED_OASIS  88
	#define b5151_OLD_GROWTH  90
	#define b5151_PLANTED_DRY_OASIS  91
	
	#define b5151_WEIRDWOOD_0  610
	#define b5151_WEIRDWOOD_1  611
	#define b5151_WEIRDWOOD_2  612
	#define b5151_WEIRDWOOD_3  613
	#define b5151_WEIRDWOOD_4  614
	#define b5151_WEIRDWOOD_5  615
	#define b5151_ENCHANTED_OASIS  616
	
	#define b5151_PERMA_RIVER  10550
	#define b5151_PERMA_ESTUARY  10551
	#define b5151_PERMA_CANAL  10552
	#define b5151_PERMA_LAKE  10553
	#define b5151_PERMA_IRRIGATION_CANAL  10554
	#define b5151_PERMA_VERDANT_CANAL  10555

	#define b5151_BEAVER_PLAINS  18451
	#define b5151_BEAVER_WOODS  18452
	#define b5151_BEAVER_DESERT  18453
	#define b5151_BEAVER_END_DESERT  18456
	#define b5151_BEAVER_OASIS  18457
	#define b5151_WITHERED_TEMPERATE  10775
	#define b5151_WITHERED_DESERT  10776
	#define b5151_GOBLIN_STUMPS  18100
	
	#define b5151_is_TEMPERATE_SCORCH(vnum)  ((vnum) == 10300 || (vnum) == 10302 || (vnum) == 10303)
	#define b5151_is_DESERT_SCORCH(vnum)  ((vnum) == 10301 || (vnum) == 10304 || (vnum) == 10305)
	
	// helpers
	#define b5151_no_sect_change(vnum)  ((vnum) == b5151_ROAD || (vnum) == b5151_BUILDING)
	#define b5151_is_DESERT(vnum)  ((vnum) == b5151_ROAD || (vnum) == b5151_BUILDING || (vnum) == b5151_DESERT || (vnum) == b5151_GROVE || (vnum) == b5151_DESERT_STUMPS || (vnum) == b5151_DESERT_COPSE || (vnum) == b5151_DESERT_SHRUB || (vnum) == b5151_BEACH || (vnum) == b5151_DESERT_CROP || b5151_is_DESERT_SCORCH(vnum) || (vnum) == b5151_WITHERED_DESERT)
	#define b5151_is_IRRIGATED(vnum)  ((vnum) == b5151_IRRIGATED_FIELD || (vnum) == b5151_IRRIGATED_FOREST || (vnum) == b5151_IRRIGATED_JUNGLE || (vnum) == b5151_IRRIGATED_STUMPS || (vnum) == b5151_IRRIGATED_COPSE || (vnum) == b5151_IRRIGATED_JUNGLE_STUMPS || (vnum) == b5151_IRRIGATED_JUNGLE_COPSE || (vnum) == b5151_IRRIGATED_PLANTED_FIELD || (vnum) == b5151_IRRIGATED_CROP || (vnum) == b5151_IRRIGATED_OASIS)
	#define b5151_is_TEMPERATE(vnum)  ((vnum) == b5151_PLAINS || (vnum) == b5151_FOREST_1 || (vnum) == b5151_FOREST_2 || (vnum) == b5151_FOREST_3 || (vnum) == b5151_FOREST_4 || (vnum) == b5151_STUMPS || (vnum) == b5151_COPSE_1 || (vnum) == b5151_COPSE_2 || (vnum) == b5151_SHORE || (vnum) == b5151_SHORESIDE_TREE || (vnum) == b5151_SEASIDE_STUMPS || (vnum) == b5151_FOREST_EDGE || (vnum) == b5151_RIVERBANK || (vnum) == b5151_FLOODPLAINS || (vnum) == b5151_FLOODED_WOODS || (vnum) == b5151_FLOODED_FOREST || (vnum) == b5151_LIGHT_RIVERBANK_FOREST || (vnum) == b5151_FORESTED_RIVERBANK || (vnum) == b5151_STUMPED_RIVERBANK || (vnum) == b5151_RIVERSIDE_COPSE || (vnum) == b5151_OLD_GROWTH || b5151_is_TEMPERATE_SCORCH(vnum) || ((vnum) >= 10562 && (vnum) <= 10566 /* evergreens */) || (vnum) == b5151_WITHERED_TEMPERATE || ((vnum) >= 11988 && (vnum) <= 11992 /* calamander */) || ((vnum) >= 16697 && (vnum) <= 16699 /* nordlys */) || (vnum) == b5151_GOBLIN_STUMPS || ((vnum) == 18293 || (vnum) == 18294 /* dragon tree */) || (vnum) == b5151_PERMA_RIVER || (vnum) == b5151_PERMA_ESTUARY)
	#define b5151_is_MOUNTAIN(vnum)  ((vnum) == b5151_MOUNTAIN || (vnum) == b5151_CLIFFS || ((vnum) >= 10190 && (vnum) <= 10192 /* volcano */))
	#define b5151_is_WEIRDWOOD(vnum)  ((vnum) == b5151_WEIRDWOOD_0 || (vnum) == b5151_WEIRDWOOD_1 || (vnum) == b5151_WEIRDWOOD_2 || (vnum) == b5151_WEIRDWOOD_3 || (vnum) == b5151_WEIRDWOOD_4 || (vnum) == b5151_WEIRDWOOD_5)
	
	
	LL_FOREACH(land_map, map) {
		to_sect = to_base = NOTHING;
		remove_bld = FALSE;
		
		// just skip ocean
		if (map->shared == &ocean_shared_data) {
			continue;
		}
		
		trench_original = get_extra_data(map->shared->extra_data, ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR);
		
		// chain of things to check
		if (b5151_is_MOUNTAIN(GET_SECT_VNUM(map->base_sector))) {
			// skip: mountains are added only by the Volcano
		}
		else if ((GET_SECT_VNUM(map->base_sector) == b5151_RIVER && GET_SECT_VNUM(map->natural_sector) != b5151_RIVER) || (GET_SECT_VNUM(map->base_sector) == b5151_ESTUARY && GET_SECT_VNUM(map->natural_sector) != b5151_ESTUARY)) {
			if (GET_SECT_VNUM(map->natural_sector) == b5151_RIVER || GET_SECT_VNUM(map->natural_sector) == b5151_ESTUARY) {
				// log("- (%d, %d) Probably fine (River or Estuary became River or Estuary)", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
			else if (GET_SECT_VNUM(map->natural_sector) == b5151_OASIS) {
				// log("- (%d, %d) River to Verdant Canal", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_VERDANT_CANAL;
				trench_original = b5151_OASIS;
				remove_bld = TRUE;
			}
			else if (b5151_is_DESERT(GET_SECT_VNUM(map->natural_sector))) {
				// log("- (%d, %d) River to Irrigation Canal", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_IRRIGATION_CANAL;
				trench_original = b5151_DESERT;
				remove_bld = TRUE;
			}
			else if (b5151_is_TEMPERATE(GET_SECT_VNUM(map->natural_sector))) {
				// log("- (%d, %d) River to Canal", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_CANAL;
				trench_original = b5151_PLAINS;
				remove_bld = TRUE;
			}
			else {
				log("- (%d, %d) Warning: No available fix (river/estuary out of place)", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
		} // end fake-river
		else if (GET_SECT_VNUM(map->natural_sector) == b5151_OASIS && (GET_SECT_VNUM(map->base_sector) != b5151_OASIS || GET_SECT_VNUM(map->sector_type) != b5151_OASIS)) {
			// natural oasis but not base/currently oasis
			if (b5151_no_sect_change(GET_SECT_VNUM(map->sector_type)) && (GET_SECT_VNUM(map->base_sector) == b5151_OASIS || GET_SECT_VNUM(map->base_sector) == b5151_ENCHANTED_OASIS)) {
				// log("- (%d, %d) Probably fine (Bld/Road on Oasis/Oasis)", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
			else if (GET_SECT_VNUM(map->sector_type) == b5151_ENCHANTED_OASIS && GET_SECT_VNUM(map->base_sector) == b5151_ENCHANTED_OASIS) {
				// log("- (%d, %d) Probably fine (Enchanted Oasis on Oasis)", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
			else if (GET_SECT_VNUM(map->sector_type) == b5151_BUILDING && b5151_is_DESERT(GET_SECT_VNUM(map->base_sector))) {
				// log("- (%d, %d) Building on removed Oasis -> Dry Oasis", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_base = b5151_DRY_OASIS;
				// do NOT remove the building
			}
			else if (b5151_is_TEMPERATE_SCORCH(GET_SECT_VNUM(map->base_sector)) || b5151_is_DESERT_SCORCH(GET_SECT_VNUM(map->base_sector))) {
				// log("- (%d, %d) Scorch tile back to Oasis", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_OASIS;
				remove_bld = TRUE;
			}
			else if (GET_SECT_VNUM(map->base_sector) == b5151_TRENCH || GET_SECT_VNUM(map->base_sector) == b5151_SANDY_TRENCH) {
				// log("- (%d, %d) Trench to Damp Trench", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_DAMP_TRENCH;
				trench_original = b5151_OASIS;
			}
			else if (GET_SECT_VNUM(map->base_sector) == b5151_CANAL || GET_SECT_VNUM(map->base_sector) == b5151_IRRIGATION_CANAL) {
				// log("- (%d, %d) Canal to Verdant Canal", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_VERDANT_CANAL;
				trench_original = b5151_OASIS;
			}
			else if (GET_SECT_VNUM(map->sector_type) == b5151_SEEDED_FIELD || GET_SECT_VNUM(map->sector_type) == b5151_IRRIGATED_PLANTED_FIELD) {
				// log("- (%d, %d) Irrigated seed to Planted Oasis", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = b5151_PLANTED_OASIS;
				to_base = b5151_IRRIGATED_OASIS;
			}
			else if (GET_SECT_VNUM(map->sector_type) == b5151_SEEDED_DESERT) {
				// log("- (%d, %d) Seeded Desert to Planted Dry Oasis", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = b5151_PLANTED_DRY_OASIS;
				to_base = b5151_DRY_OASIS;
			}
			else if (GET_SECT_VNUM(map->sector_type) == b5151_CROP || GET_SECT_VNUM(map->sector_type) == b5151_IRRIGATED_CROP) {
				// log("- (%d, %d) Irrigated crop to Oasis Crop", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = b5151_OASIS_CROP;
				to_base = b5151_IRRIGATED_OASIS;
			}
			else if (GET_SECT_VNUM(map->sector_type) == b5151_DESERT_CROP) {
				// log("- (%d, %d) Desert crop to Oasis Crop", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = b5151_OASIS_CROP;
				to_base = b5151_OASIS;
			}
			else if (b5151_is_TEMPERATE(GET_SECT_VNUM(map->base_sector))) {
				// log("- (%d, %d) Temperate tile back to Oasis", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_OASIS;
				remove_bld = TRUE;
			}
			else if (b5151_is_IRRIGATED(GET_SECT_VNUM(map->base_sector))) {
				// put this AFTER crops as those are irrigated too
				// log("- (%d, %d) Irrigated tile to Irrigated Oasis", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_IRRIGATED_OASIS;
				remove_bld = TRUE;
			}
			else if (GET_SECT_VNUM(map->base_sector) >= b5151_BEAVER_PLAINS && GET_SECT_VNUM(map->base_sector) <= b5151_BEAVER_END_DESERT) {
				// log("- (%d, %d) Flooded Tile to Flooded Oasis", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_BEAVER_OASIS;
			}
			else if (GET_SECT_VNUM(map->base_sector) >= b5151_WEIRDWOOD_0 && GET_SECT_VNUM(map->base_sector) <= b5151_WEIRDWOOD_4) {
				// log("- (%d, %d) Weirdwood to Enchanted Oasis", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_ENCHANTED_OASIS;
				remove_bld = TRUE;
			}
			else if (b5151_is_DESERT(GET_SECT_VNUM(map->sector_type)) || b5151_is_DESERT(GET_SECT_VNUM(map->base_sector))) {
				// log("- (%d, %d) Desert to Dry Oasis", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_DRY_OASIS;
				remove_bld = TRUE;
			}
			else {
				log("- (%d, %d) Warning: No available fix (natural oasis has bad sector)", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
		}	// end natural oasis
		else if (GET_SECT_VNUM(map->sector_type) == b5151_CROP && b5151_is_DESERT(GET_SECT_VNUM(map->natural_sector))) {
			// regular crop on a natural desert tile
			// log("- (%d, %d) Old irrigated crop to new Irrigated Crop", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			to_sect = b5151_IRRIGATED_CROP;
			to_base = b5151_IRRIGATED_FIELD;
		}
		else if (GET_SECT_VNUM(map->sector_type) == b5151_SEEDED_FIELD && b5151_is_DESERT(GET_SECT_VNUM(map->natural_sector))) {
			// regular crop on a natural desert tile
			// log("- (%d, %d) Old irrigated seed to new Planted Irrigated", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			to_sect = b5151_IRRIGATED_PLANTED_FIELD;
			to_base = b5151_IRRIGATED_FIELD;
		}
		else if (b5151_is_DESERT(GET_SECT_VNUM(map->natural_sector)) && !b5151_is_DESERT(GET_SECT_VNUM(map->base_sector))) {
			// things that started out desert but aren't now
			if (b5151_is_IRRIGATED(GET_SECT_VNUM(map->base_sector))) {
				// log("- (%d, %d) Probably fine (Irrigated on Desert)", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
			else if (b5151_is_WEIRDWOOD(GET_SECT_VNUM(map->base_sector))) {
				// log("- (%d, %d) Probably fine (Weirdwood on Desert)", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
			else if (GET_SECT_VNUM(map->base_sector) == b5151_SANDY_TRENCH) {
				// log("- (%d, %d) Probably fine (Sandy Trench on Desert)", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
			else if (b5151_is_DESERT_SCORCH(GET_SECT_VNUM(map->base_sector))) {
				// log("- (%d, %d) Probably fine (Desert Scorch on Desert)", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
			else if (GET_SECT_VNUM(map->base_sector) >= b5151_BEAVER_DESERT && GET_SECT_VNUM(map->base_sector) <= b5151_BEAVER_END_DESERT) {
				// log("- (%d, %d) Probably fine (Beaver flooding on desert)", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
			else if (b5151_is_TEMPERATE_SCORCH(GET_SECT_VNUM(map->base_sector))) {
				// log("- (%d, %d) Temperate Scorch on Desert tile", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_DESERT;
			}
			else if (GET_SECT_VNUM(map->base_sector) == b5151_TRENCH) {
				// log("- (%d, %d) Temperate Trench on Desert tile", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_SANDY_TRENCH;
				trench_original = b5151_DESERT;
			}
			else if (GET_SECT_VNUM(map->base_sector) == b5151_CANAL) {
				// log("- (%d, %d) Canal on Desert tile", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_IRRIGATION_CANAL;
				trench_original = b5151_DESERT;
			}
			else if (GET_SECT_VNUM(map->base_sector) == b5151_PERMA_RIVER || GET_SECT_VNUM(map->base_sector) == b5151_PERMA_ESTUARY || GET_SECT_VNUM(map->base_sector) == b5151_PERMA_CANAL || GET_SECT_VNUM(map->base_sector) == b5151_PERMA_LAKE) {
				// log("- (%d, %d) Permafrost tile on Desert tile", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_IRRIGATION_CANAL;
				trench_original = b5151_DESERT;
			}
			else if (GET_SECT_VNUM(map->base_sector) == b5151_OASIS) {
				// log("- (%d, %d) Oasis on Desert tile: canal", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_IRRIGATION_CANAL;
				trench_original = b5151_DESERT;
				remove_bld = TRUE;
			}
			else if (GET_SECT_VNUM(map->base_sector) == b5151_ENCHANTED_OASIS) {
				// log("- (%d, %d) Enchanted Oasis on non-Oasis tile", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_WEIRDWOOD_1;
				remove_bld = TRUE;
			}
			else if (b5151_is_TEMPERATE(GET_SECT_VNUM(map->base_sector))) {
				// log("- (%d, %d) Temperate tile on Desert", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_IRRIGATED_FIELD;
			}
			else if (GET_SECT_VNUM(map->base_sector) == b5151_BEAVER_PLAINS || GET_SECT_VNUM(map->base_sector) == b5151_BEAVER_WOODS) {
				// log("- (%d, %d) Flooded Tile to Flooded Oasis", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				to_sect = to_base = b5151_BEAVER_DESERT;
			}
			else {
				log("- (%d, %d) Warning: No available fix (base desert with something else on top)", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
			}
		}
		
		
		// LAST: do the work
		if ((to_sect != NOTHING || to_base != NOTHING)) { // && (room = map->room ? map->room : real_room(map->vnum))) {
			if (to_sect && !b5151_no_sect_change(GET_SECT_VNUM(map->sector_type))) {
				perform_change_sect(NULL, map, sector_proto(to_sect));
				++changed_sect;
			}
			if (to_base != NOTHING) {
				perform_change_base_sect(NULL, map, sector_proto(to_base));
				++changed_base;
			}
			if (trench_original > 0) {
				set_extra_data(&map->shared->extra_data, ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR, trench_original);
			}
			if (remove_bld && (room = (map->room ? map->room : real_room(map->vnum))) && GET_BUILDING(room) && !IS_CITY_CENTER(room)) {
				// log("- (%d, %d) Removing building", MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum));
				++changed_bld;
				disassociate_building(room);
			}
		}
	}
	
	log("- total: %d sector%s, %d base sector%s, removed %d building%s", changed_sect, PLURAL(changed_sect), changed_base, PLURAL(changed_base), changed_bld, PLURAL(changed_bld));
}


// b5.152 (1/4): Updates a "light" object with new data.
void b5_152_light_update(obj_data *obj) {
	obj_data *proto;
	bool was_lit;
	
	if ((OBJ_FLAGGED(obj, OBJ_LIGHT) || IS_LIGHT(obj)) && (proto = obj_proto(GET_OBJ_VNUM(obj)))) {
		was_lit = LIGHT_IS_LIT(obj);
		
		// type is always loaded from file -- no need to update
		
		// light flag likely removed
		if (!OBJ_FLAGGED(proto, OBJ_LIGHT)) {
			REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_LIGHT);
		}
		
		// timer likely changed, too: remove it if applicable
		if (GET_OBJ_TIMER(proto) <= 0) {
			GET_OBJ_TIMER(obj) = GET_OBJ_TIMER(proto);
		}
		
		// match all 3 vals if it became a light type
		if (IS_LIGHT(obj)) {
			GET_OBJ_VAL(obj, 0) = GET_OBJ_VAL(proto, 0);
			GET_OBJ_VAL(obj, 1) = GET_OBJ_VAL(proto, 1);
			GET_OBJ_VAL(obj, 2) = GET_OBJ_VAL(proto, 2);
		}
		
		// and is it really in the world? (not on a loaded character)
		if (OBJ_IS_IN_WORLD(obj)) {
			request_obj_save_in_world(obj);
			schedule_obj_timer_update(obj, FALSE);
			
			// and, did this shut the light off?
			if (was_lit != LIGHT_IS_LIT(obj)) {
				if (was_lit) {
					apply_obj_light(obj, FALSE);
				}
				else {
					apply_obj_light(obj, TRUE);
				}
			}
		}
	}
}


// b5.152 (2/4): Updates contents of a container -- only when carried by a player
void b5_152_container_update(obj_data *obj) {
	if (obj->next_content) {
		b5_152_container_update(obj->next_content);
	}
	if (obj->contains) {
		b5_152_container_update(obj->contains);
	}
	
	b5_152_light_update(obj);
}


// b5.152 (3/4): replace lights and fix durations on world and mob affs
void b5_152_world_update(void) {
	room_data *room, *next_room;
	char_data *mob;
	obj_data *obj, *next_obj;
	struct affected_type *af;
	struct empire_unique_storage *eus;
	struct trading_post_data *tpd;
	empire_data *emp, *next_emp;
	
	// rooms
	HASH_ITER(hh, world_table, room, next_room) {
		LL_FOREACH(ROOM_AFFECTS(room), af) {
			// these were saved as timestamps before, but should now be seconds
			if (af->expire_time != UNLIMITED) {
				// log("%d: %d %ld %ld", GET_ROOM_VNUM(room), af->type, af->expire_time, af->expire_time - time(0));
				af->expire_time -= time(0);
				schedule_room_affect_expire(room, af);
				request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
			}
		}
	}
	
	// mobs
	DL_FOREACH(character_list, mob) {
		LL_FOREACH(mob->affected, af) {
			// these were saved in 5-second updates and are now in 1-second intervals instead
			if (af->expire_time != UNLIMITED) {
				//  log("%s: %d %ld %ld", GET_SHORT_DESC(mob), af->type, af->expire_time, (time(0) + 5 * (af->expire_time - time(0))));
				af->expire_time = time(0) + 5 * (af->expire_time - time(0));
				schedule_affect_expire(mob, af);
				request_char_save_in_world(mob);
			}
		}
	}
	
	// lights in the world
	DL_FOREACH(object_list, obj) {
		b5_152_light_update(obj);
	}
	
	// lights in empire storage
	HASH_ITER(hh, empire_table, emp, next_emp) {
		DL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
			if ((obj = eus->obj)) {
				b5_152_light_update(obj);
			}
		}
		EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	}
	
	// trading post
	DL_FOREACH(trading_list, tpd) {
		if ((obj = tpd->obj)) {
			b5_152_light_update(obj);
		}
	}
	save_trading_post();
	
	// and report on any objects that might be in the DB and unchanged
	HASH_ITER(hh, object_table, obj, next_obj) {
		// this ignores only 2 vnums that keep LIGHT flags in the stock distribution
		if (OBJ_FLAGGED(obj, OBJ_LIGHT) && GET_OBJ_VNUM(obj) != 10303 && GET_OBJ_VNUM(obj) != 10502) {
			log("Warning: Object [%d] %s has LIGHT flag not converted by b5.152 patch (HELP LIGHT ITEM)", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
		}
	}
}


// b5.152 (4/4): replace lights and fix durations on player affs
PLAYER_UPDATE_FUNC(b5_152_player_update) {
	struct affected_type *af;
	struct empire_unique_storage *eus;
	obj_data *obj;
	int pos;
	
	check_delayed_load(ch);
	
	// equipment
	for (pos = 0; pos < NUM_WEARS; ++pos) {
		if ((obj = GET_EQ(ch, pos))) {
			b5_152_light_update(obj);
			if (obj->contains) {
				b5_152_container_update(obj->contains);
			}
		}
	}
	
	// inventory
	DL_FOREACH2(ch->carrying, obj, next_content) {
		b5_152_light_update(obj);
		if (obj->contains) {
			b5_152_container_update(obj->contains);
		}
	}
	
	// home storage
	DL_FOREACH(GET_HOME_STORAGE(ch), eus) {
		if ((obj = eus->obj)) {
			b5_152_light_update(obj);
		}
	}
	
	// affect durations
	LL_FOREACH(ch->affected, af) {
		// these were saved in 5-second updates and are now in 1-second intervals instead
		// log("%s: %d %ld", GET_PC_NAME(ch), af->type, af->expire_time);
		if (af->expire_time != UNLIMITED) {
			// note they are in SECONDS not TIMESTAMPS at this point
			af->expire_time *= 5;
		}
	}
}


// b5.153: Fix players with invalid hunger/thirst values due to a bug in b5.152
PLAYER_UPDATE_FUNC(b5_153_player_repair) {
	int iter;
	
	for (iter = 0; iter < NUM_CONDS; ++iter) {
		// repair weird negatives
		if (GET_COND(ch, iter) < -1) {
			GET_COND(ch, iter) = UNLIMITED;
		}
		
		// check for mortals with unlimited values they shouldn't have
		if (!IS_IMMORTAL(ch) && !IS_GOD(ch) && GET_COND(ch, iter) == UNLIMITED) {
			GET_COND(ch, iter) = 0;
		}
	}
}


// b5.162: Players now have status message preferences.
PLAYER_UPDATE_FUNC(b5_162_status_messages) {
	// bits to migrate
	bitvector_t PRF_MORTLOG = BIT(6);
	bitvector_t PRF_NO_CHANNEL_JOINS = BIT(15);
	bitvector_t PRF_TRAVEL_LOOK = BIT(35);

	GET_STATUS_MESSAGES(ch) = DEFAULT_STATUS_MESSAGES;
	GET_FIGHT_MESSAGES(ch) |= FM_MY_BUFFS_IN_COMBAT | FM_ALLY_BUFFS_IN_COMBAT | FM_OTHER_BUFFS_IN_COMBAT | FM_MY_AFFECTS_IN_COMBAT | FM_ALLY_AFFECTS_IN_COMBAT | FM_OTHER_AFFECTS_IN_COMBAT | FM_MY_ABILITIES | FM_ALLY_ABILITIES | FM_OTHER_ABILITIES | FM_ABILITIES_AGAINST_ME | FM_ABILITIES_AGAINST_ALLIES | FM_ABILITIES_AGAINST_TARGET | FM_ABILITIES_AGAINST_TANK;
	
	// mortlog
	if (PRF_FLAGGED(ch, PRF_MORTLOG)) {
		SET_BIT(GET_STATUS_MESSAGES(ch), SM_MORTLOG);
		REMOVE_BIT(PRF_FLAGS(ch), PRF_MORTLOG);
	}
	else {
		REMOVE_BIT(GET_STATUS_MESSAGES(ch), SM_MORTLOG);
	}
	
	// !channel-joins
	if (PRF_FLAGGED(ch, PRF_NO_CHANNEL_JOINS)) {
		REMOVE_BIT(GET_STATUS_MESSAGES(ch), SM_CHANNEL_JOINS);
		REMOVE_BIT(PRF_FLAGS(ch), PRF_NO_CHANNEL_JOINS);
	}
	else {
		SET_BIT(GET_STATUS_MESSAGES(ch), SM_CHANNEL_JOINS);
	}
	
	// travel-look
	if (PRF_FLAGGED(ch, PRF_TRAVEL_LOOK)) {
		SET_BIT(GET_STATUS_MESSAGES(ch), SM_TRAVEL_AUTO_LOOK);
		REMOVE_BIT(PRF_FLAGS(ch), PRF_TRAVEL_LOOK);
	}
	else {
		REMOVE_BIT(GET_STATUS_MESSAGES(ch), SM_TRAVEL_AUTO_LOOK);
	}
}


// b5.165: Add some new fmessages to players
PLAYER_UPDATE_FUNC(b5_165_fight_messages) {
	GET_FIGHT_MESSAGES(ch) |= FM_MY_HEALS | FM_HEALS_ON_ME | FM_HEALS_ON_ALLIES | FM_HEALS_ON_TARGET | FM_HEALS_ON_OTHER;
}


// updates with the new required affect on counterspell and phoenix rite plus heal friend
PLAYER_UPDATE_FUNC(b5_166_player_update) {
	struct affected_type *hjp;
	struct player_skill_data *plsk, *next_plsk;
	
	any_vnum COUNTERSPELL = 3021;
	any_vnum PHOENIX_RITE = 3017;
	
	any_vnum HEAL_FRIEND = 110;
	any_vnum NATURAL_MAGIC = 3;
	
	any_vnum BASH = 97;
	any_vnum BATTLE = 0;
	
	any_vnum GIANT_TORTOISE = 506;
	
	check_delayed_load(ch);
	
	// new affect flags
	LL_FOREACH(ch->affected, hjp) {
		if (hjp->type == COUNTERSPELL && hjp->bitvector == NOBITS) {
			hjp->bitvector = AFF_COUNTERSPELL;
		}
	
		if (hjp->type == PHOENIX_RITE && hjp->bitvector == NOBITS) {
			hjp->bitvector = AFF_AUTO_RESURRECT;
		}
	}
	
	// heal friend increased in level
	if (has_ability(ch, HEAL_FRIEND) && get_skill_level(ch, NATURAL_MAGIC) < 55) {
		remove_ability(ch, ability_proto(HEAL_FRIEND), FALSE);
	}
	
	// bash increased in level
	if (has_ability(ch, BASH) && get_skill_level(ch, BATTLE) < 80) {
		remove_ability(ch, ability_proto(BASH), FALSE);
	}
	
	// remove giant tortoise companion: it now comes from a different ability
	remove_companion(ch, GIANT_TORTOISE);
	
	// grant free skill resets
	HASH_ITER(hh, GET_SKILL_HASH(ch), plsk, next_plsk) {
		if (plsk->level > 0) {
			plsk->resets = MIN(plsk->resets + 1, MAX_SKILL_RESETS);
		}
	}
	
	// add new status message
	GET_STATUS_MESSAGES(ch) |= SM_FIGHT_PROMPT;
}

// applies traits now required for enchanted walls
void b5_166_barrier_magentafication(void) {
	room_data *room, *next_room;
	
	HASH_ITER(hh, world_table, room, next_room) {
		if (ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_FLY) && ROOM_BLD_FLAGGED(room, BLD_BARRIER) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_PERMANENT_PAINT)) {
			SET_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_PERMANENT_PAINT);
			set_room_extra_data(room, ROOM_EXTRA_PAINT_COLOR, 11);
			affect_total_room(room);
		}
	}
}


// clears removed DIGGING flag
void b5_168_updates(void) {
	bld_data *bld, *next_bld;
	room_template *rmt, *next_rmt;
	vehicle_data *veh, *next_veh;
	
	bitvector_t FNC_DIGGING = BIT(5);
	
	// ensure no use of the DIGGING function flag
	HASH_ITER(hh, building_table, bld, next_bld) {
		if (IS_SET(GET_BLD_FUNCTIONS(bld), FNC_DIGGING)) {
			REMOVE_BIT(GET_BLD_FUNCTIONS(bld), FNC_DIGGING);
			log("Warning: Deprecated function DIGGING removed from building %d %s", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
			save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
		}
	}
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		if (IS_SET(GET_RMT_FUNCTIONS(rmt), FNC_DIGGING)) {
			REMOVE_BIT(GET_RMT_FUNCTIONS(rmt), FNC_DIGGING);
			log("Warning: Deprecated function DIGGING removed from room template %d %s", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt));
			save_library_file_for_vnum(DB_BOOT_RMT, GET_RMT_VNUM(rmt));
		}
	}
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		if (IS_SET(VEH_FUNCTIONS(veh), FNC_DIGGING)) {
			REMOVE_BIT(VEH_FUNCTIONS(veh), FNC_DIGGING);
			log("Warning: Deprecated function DIGGING removed from vehicle %d %s", VEH_VNUM(veh), VEH_SHORT_DESC(veh));
			save_library_file_for_vnum(DB_BOOT_VEH, VEH_VNUM(veh));
		}
	}
}


// ensures everyone has a highest greatness shown
PLAYER_UPDATE_FUNC(b5_188_greatness) {
	int great;
	
	check_delayed_load(ch);
	
	great = GET_GREATNESS(ch);
	great = MIN(great, att_max(ch));
	
	if (GET_HIGHEST_KNOWN_GREATNESS(ch) < great) {
		GET_HIGHEST_KNOWN_GREATNESS(ch) = great;
	}
}


// moves player books to a new vnum range to fix an old problem
void b5_169_book_move(void) {
	any_vnum new_vnum, old_vnum;
	bool found, is_file;
	int iter;
	book_data *book, *next_book, *copied;
	char_data *ch;
	empire_data *emp, *next_emp;
	obj_data *obj;
	room_data *room;
	player_index_data *index, *next_index;
	struct author_data *author, *next_author;
	struct empire_unique_storage *eus;
	struct library_data *libr, *next_libr;
	struct trading_post_data *tpd;
	
	// list to keep as-is
	int core_book_list[] = { 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 24, 5511, 11862, -1 };	// terminated by -1
	
	// helpers for updating existing books
	struct book_repair_t {
		any_vnum old;
		any_vnum new;
		UT_hash_handle hh;
	} *book_hash = NULL, *bh, *next_bh;
	
	// update all books
	HASH_ITER(hh, book_table, book, next_book) {
		if (BOOK_VNUM(book) >= START_PLAYER_BOOKS) {
			continue;	// skip any already moved/legal
		}
		
		// is it a core book?
		for (iter = 0, found = FALSE; core_book_list[iter] != -1 && !found; ++iter) {
			if (BOOK_VNUM(book) == core_book_list[iter]) {
				found = TRUE;
			}
		}
		if (found) {
			// skip books on the core list
			continue;
		}
		
		// ok, going to move the book: determine new vnum
		old_vnum = BOOK_VNUM(book);
		new_vnum = ++top_book_vnum;
		if (top_book_vnum < START_PLAYER_BOOKS) {
			// if it's the first player book, ensure it's not too low in vnum
			top_book_vnum = START_PLAYER_BOOKS;
			new_vnum = START_PLAYER_BOOKS;
		}
		
		// create hash entry (or find?)
		HASH_FIND_INT(book_hash, &old_vnum, bh);
		if (!bh) {
			CREATE(bh, struct book_repair_t, 1);
			bh->old = old_vnum;
			HASH_ADD_INT(book_hash, old, bh);
		}
		bh->new = new_vnum;
		
		// copy book to that location
		copied = setup_olc_book(book);
		BOOK_VNUM(copied) = new_vnum;
		add_book_to_table(copied);
		
		// swap in-library data; this will be correct soon
		BOOK_IN_LIBRARIES(copied) = BOOK_IN_LIBRARIES(book);
		BOOK_IN_LIBRARIES(book) = NULL;
		
		// delete old book
		remove_book_from_table(book);
		free_book(book);
	}
	
	// update existing books: world
	DL_FOREACH(object_list, obj) {
		if (IS_BOOK(obj)) {
			old_vnum = GET_BOOK_ID(obj);
			HASH_FIND_INT(book_hash, &old_vnum, bh);
			if (bh) {
				set_obj_val(obj, VAL_BOOK_ID, bh->new);
			}
		}
	}
	
	// update existing books: warehouse
	HASH_ITER(hh, empire_table, emp, next_emp) {
		DL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
			if ((obj = eus->obj) && IS_BOOK(obj)) {
				old_vnum = GET_BOOK_ID(obj);
				HASH_FIND_INT(book_hash, &old_vnum, bh);
				if (bh) {
					set_obj_val(obj, VAL_BOOK_ID, bh->new);
					EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
				}
			}
		}
	}
	
	// update existing books: trading post
	DL_FOREACH(trading_list, tpd) {
		if ((obj = tpd->obj) && IS_BOOK(obj)) {
			old_vnum = GET_BOOK_ID(obj);
			HASH_FIND_INT(book_hash, &old_vnum, bh);
			if (bh) {
				set_obj_val(obj, VAL_BOOK_ID, bh->new);
			}
		}
	}
	save_trading_post();
	
	// update existing books: players
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
		if (!(ch = find_or_load_player(index->name, &is_file))) {
			continue;
		}
		
		// skip eq: books are not equippable
		
		// inventory
		DL_FOREACH2(ch->carrying, obj, next_content) {
			if (IS_BOOK(obj)) {
				old_vnum = GET_BOOK_ID(obj);
				HASH_FIND_INT(book_hash, &old_vnum, bh);
				if (bh) {
					set_obj_val(obj, VAL_BOOK_ID, bh->new);
				}
			}
		}
		
		// home storage
		DL_FOREACH(GET_HOME_STORAGE(ch), eus) {
			if ((obj = eus->obj) && IS_BOOK(obj)) {
				old_vnum = GET_BOOK_ID(obj);
				HASH_FIND_INT(book_hash, &old_vnum, bh);
				if (bh) {
					set_obj_val(obj, VAL_BOOK_ID, bh->new);
				}
			}
		}
		
		// save
		if (is_file) {
			store_loaded_char(ch);
			is_file = FALSE;
			ch = NULL;
		}
		else {
			queue_delayed_update(ch, CDU_SAVE);
		}
	}
	
	// and lastly, audit remaining books for invalid library locations
	HASH_ITER(hh, book_table, book, next_book) {
		HASH_ITER(hh, BOOK_IN_LIBRARIES(book), libr, next_libr) {
			if (!(room = real_room(libr->location)) || !HAS_FUNCTION(room, FNC_LIBRARY)) {
				// invalid location
				HASH_DEL(BOOK_IN_LIBRARIES(book), libr);
				free(libr);
			}
		}
	}
	
	// save all books:
	save_author_index();
	HASH_ITER(hh, author_table, author, next_author) {
		save_author_books(author->idnum);
	}
	
	// free data
	HASH_ITER(hh, book_hash, bh, next_bh) {
		HASH_DEL(book_hash, bh);
		free(bh);
	}
}


// b5.169 part 2: rename city centers
void b5_169_city_centers(void) {
	char buf[256];
	empire_data *emp, *next_emp;
	struct empire_city_data *city;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		LL_FOREACH(EMPIRE_CITY_LIST(emp), city) {
			if (city->location) {
				snprintf(buf, sizeof(buf), "The Center of %s", city->name);
				set_room_custom_name(city->location, buf);
				SET_BIT(ROOM_BASE_FLAGS(city->location), ROOM_AFF_HIDE_REAL_NAME);
				affect_total_room(city->location);
			}
		}
	}
}


// b5.170 applies timers to items in storage
void b5_170_timer_updates(void) {
	int amount;
	empire_data *emp, *next_emp;
	obj_data *proto;
	struct empire_island *isle, *next_isle;
	struct empire_storage_data *store, *next_store;
	struct shipping_data *shipd, *next_shipd;
	
	// Note: there is no need to update warehouse or home storage: these did
	// not allow items with timers prior to this patch.
	
	// all empires
	HASH_ITER(hh, empire_table, emp, next_emp) {
		// each island
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
			// each storage on the island
			HASH_ITER(hh, isle->store, store, next_store) {
				EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
				
				if (store->timers) {
					// remove and replace existing timers
					free_storage_timers(&store->timers);
				}
				
				if ((proto = store->proto) && GET_OBJ_TIMER(proto)) {
					// initialize to several different times
					amount = store->amount;
					if (amount > 0) {
						add_storage_timer(&store->timers, GET_OBJ_TIMER(proto), amount/2);
						amount -= (amount / 2);
					}
					if (amount > 0) {
						add_storage_timer(&store->timers, GET_OBJ_TIMER(proto) * 3/4, amount/2);
						amount -= (amount / 2);
					}
					if (amount > 0) {
						add_storage_timer(&store->timers, GET_OBJ_TIMER(proto) * 2/3, amount);
					}
				}
			}
		}
		
		// shipping
		DL_FOREACH_SAFE(EMPIRE_SHIPPING_LIST(emp), shipd, next_shipd) {
			EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
			
			if (shipd->timers) {
				// remove and replace existing timers
				free_storage_timers(&shipd->timers);
			}
			
			if ((proto = obj_proto(shipd->vnum)) && GET_OBJ_TIMER(proto)) {
				add_storage_timer(&shipd->timers, GET_OBJ_TIMER(proto), shipd->amount);
			}
		}
	}
}


// b5.170 adds data on the character for their home location; need to update it now
void b5_170_home_assignments(void) {
	bool is_file;
	char_data *ch;
	room_data *iter, *next_iter;
	
	HASH_ITER(hh, world_table, iter, next_iter) {
		if (ROOM_PRIVATE_OWNER(iter) != NOBODY) {
			// found private owner
			if ((ch = find_or_load_player_by_idnum(ROOM_PRIVATE_OWNER(iter), &is_file))) {
				// save new data
				GET_HOME_LOCATION(ch) = GET_ROOM_VNUM(iter);
				
				if (is_file) {
					store_loaded_char(ch);
					is_file = FALSE;
				}
				else {
					queue_delayed_update(ch, CDU_SAVE);
				}
			}
			else {
				// bad data: no player
				set_private_owner(iter, NOBODY);
			}
		}
	}
}


// b5.171 replaces bathing with a pair of triggers, which must be attached to all live buildings
void b5_171_bath_triggers(void) {
	struct trig_proto_list *tpl;
	room_data *room, *next_room;
	int count = 0;
	
	// some vnums
	any_vnum BATH_BUILDING = 5162;
	any_vnum BATH_ROOM = 5614;
	
	any_vnum BATH_TRIG_1 = 5162;
	any_vnum BATH_TRIG_2 = 9808;
	
	if (!real_trigger(BATH_TRIG_1) || !real_trigger(BATH_TRIG_2)) {
		log("- bath update skipped because trigs %d and %d don't exist", BATH_TRIG_1, BATH_TRIG_2);
		return;
	}
	
	HASH_ITER(hh, world_table, room, next_room) {
		if (!GET_BUILDING(room)) {
			continue;
		}
		
		if (GET_BLD_VNUM(GET_BUILDING(room)) == BATH_BUILDING || GET_BLD_VNUM(GET_BUILDING(room)) == BATH_ROOM) {
			CREATE(tpl, struct trig_proto_list, 1);
			tpl->vnum = BATH_TRIG_1;
			LL_APPEND(room->proto_script, tpl);
		
			CREATE(tpl, struct trig_proto_list, 1);
			tpl->vnum = BATH_TRIG_2;
			LL_APPEND(room->proto_script, tpl);
		
			assign_triggers(room, WLD_TRIGGER);
			++count;
		}
	}
	
	log("- updated %d bath%s", count, PLURAL(count));
}


// b5.173: remove tavern data, remove tavern from learned list on empires without the new goal
void b5_173_tavern_update(void) {
	empire_data *emp, *next_emp;
	room_data *room, *next_room;
	
	// new progress goal for tavern
	const any_vnum TAVERN_PROG = 2916;
	const any_vnum TAVERN_CRAFT = 5138;
	const any_vnum BEEKEEPING_PROG = 2912;
	const any_vnum MEAD_CRAFT = 236;
	
	// former data values
	const int ROOM_EXTRA_TAVERN_TYPE = 4;
	const int ROOM_EXTRA_TAVERN_BREWING_TIME = 5;
	const int ROOM_EXTRA_TAVERN_AVAILABLE_TIME = 6;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		// remove Tavern from empires
		if (!empire_has_completed_goal(emp, TAVERN_PROG) && empire_has_learned_craft(emp, TAVERN_CRAFT)) {
			remove_learned_craft_empire(emp, TAVERN_CRAFT, TRUE);
		}
		// add mead?
		if (empire_has_completed_goal(emp, BEEKEEPING_PROG)) {
			add_learned_craft_empire(emp, MEAD_CRAFT);
		}
	}
	
	// remove old data from rooms
	HASH_ITER(hh, world_table, room, next_room) {
		if (get_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE)) {
			remove_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE);
		}
		if (get_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME)) {
			remove_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME);
		}
		if (get_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME)) {
			remove_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME);
		}
	}
}


// b5.174: copy old config to new config
void b5_174_config_change(void) {
	struct config_type *get_config_by_key(char *key, bool exact);
	struct config_type *config;
	
	if (strcmp(config_get_string("newyear_message"), "The ground under you shakes violently!")) {
		// had a custom message -- copy it over
		if ((config = get_config_by_key("world_reset_message", TRUE))) {
			log("- copying message");
			if (config->data.string_val) {
				free(config->data.string_val);
			}
			config->data.string_val = str_dup(config_get_string("newyear_message"));
			save_config_system();
		}
	}
}


// ADD HERE, above: more beta 5 update functions


 //////////////////////////////////////////////////////////////////////////////
//// UPDATE DATA /////////////////////////////////////////////////////////////

const struct {
	char *version;	// should be in a format like "1.0" or "b5.1"; must be unique
	void (*update_func)(void);
	PLAYER_UPDATE_FUNC(*player_update_func);
	char *log_str;	// for the syslog; may be NULL
} version_update_data[] = {
	// 1. this list MUST be in ascending order -- oldest updaters to newest
	// 2. do not insert in the middle, only at the end
	// 3. never remove the last entry that was applied to the mud (you'd risk it re-running updaters or losing its place)
	
	// beta 2
	{ "b2.5", b2_5_workforce_update, NULL, "Update to empires" },
	{ "b2.7", NULL, NULL, "No update" },
	{ "b2.8", NULL, b2_8_update_players, "Update to players" },
	{ "b2.9", b2_9_crop_update, NULL, "Update to crops" },
	{ "b2.11", b2_11_update, b2_11_update_players, "Trigger updates" },
	
	// beta 3
	{ "b3.0", b3_0_crop_update, NULL, "Update to crops" },
	{ "b3.1", b3_1_mine_update, NULL, "Update to mines" },
	{ "b3.2", b3_2_map_and_gear, NULL, "Fixing map and gear" },
	{ "b3.6", b3_6_einv_fix, NULL, "Fixing einventory" },
	{ "b3.8", NULL, NULL, "Update to vehicles - canceled in b5.110" },
	{ "b3.11", b3_11_ship_fix, NULL, "Fix to ships" },
	{ "b3.12", NULL, b3_12_update_players, "Removal of stray affect flag" },
	{ "b3.15", b3_15_crop_update, NULL, "Spawning crops" },
	{ "b3.17", b3_17_road_update, NULL, "Adding road data" },
	
	// beta 4
	{ "b4.1", NULL, b4_1_approve_players, "Adding approval data" },
	{ "b4.2", NULL, b4_2_mount_update, "Adding mount data" },
	{ "b4.4", NULL, b4_4_fight_messages, "Adding fight messages" },
	{ "b4.15", b4_15_building_update, NULL, "Converting building data" },
	{ "b4.19", NULL, b4_19_update_players, "Update to players" },
	{ "b4.32", b4_32_convert_rmts, NULL, "Update to rmts" },
	{ "b4.36", b4_36_study_triggers, NULL, "Update to studies" },
	{ "b4.38", b4_38_tower_triggers, NULL, "Update to towers" },
	{ "b4.39", b4_39_data_conversion, NULL, "Converting data to new format" },
	
	// beta 5
	{ "b5.1", b5_1_global_update, b5_1_update_players, NULL },
	{ "b5.3", b5_3_missile_update, NULL, NULL },
	{ "b5.14", b5_14_superior_items, b5_14_player_superiors, NULL },
	{ "b5.17", save_all_empires, NULL, NULL },
	{ "b5.19", b5_19_world_fix, NULL, NULL },
	{ "b5.20", b5_20_canal_fix, NULL, NULL },
	{ "b5.23", b5_23_potion_update, b5_23_player_potion_update, "Updating potions" },
	{ "b5.24", b5_24_poison_update, b5_24_player_poison_update, "Updating poisons" },
	{ "b5.25", b5_25_trench_update, NULL, "Updating trenches" },
	{ "b5.30", b5_30_empire_update, NULL, "Updating empires" },
	{ "b5.34", b5_34_mega_update, b5_34_player_update, "Progression update" },
	{ "b5.35", b5_35_progress_update, NULL, "Progression update" },
	{ "b5.37", b5_37_progress_update, NULL, NULL },
	{ "b5.38", b5_38_grove_update, NULL, "Grove update" },
	{ "b5.40", NULL, b5_40_update_players, "Channel update for players" },
	{ "b5.45", b5_45_keep_update, NULL, "Applying 'keep' update to empires" },
	{ "b5.47", b5_47_mine_update, b5_47_update_players, "Clear mine data, update island flags, and fix player reputation" },
	{ "b5.48", b5_48_rope_update, NULL, "Update to add ropes to tied mobs" },
	{ "b5.58", b5_58_gather_totals, NULL, "Update to empire gather totals" },
	{ "b5.60", NULL, b5_60_update_players, "Channel update for players" },
	{ "b5.80", b5_80_dailies_fix, NULL, "Update daily quests" },
	{ "b5.82", b5_82_snowman_fix, NULL, "Snowman fix" },
	{ "b5.83", NULL, b5_83_update_players, "Tool update to players" },
	{ "b5.84", b5_84_climate_update, NULL, "Updating sectors and crops" },
	{ "b5.86", b5_86_update, b5_86_player_missile_weapons, "Update to missile weapons" },
	{ "b5.86a", NULL, NULL, NULL },
	{ "b5.87", b5_87_crop_and_old_growth, NULL, "Removing 75% of unclaimed crops and adding old-growth forests" },
	{ "b5.88", b5_88_irrigation_repair, NULL, "Repairing tiles affected by previous irrigation bug" },
	{ "b5.88a", b5_88_resource_components_update, NULL, "Updating components" },
	{ "b5.88b", b5_88_maintenance_fix, NULL, "Repairing build/maintenance lists" },
	{ "b5.94", b5_94_terrain_heights, NULL, "Attempting to detect map heights" },
	{ "b5.99", b5_99_henge_triggers, NULL, "Update to henges" },
	{ "b5.102", b5_102_home_cleanup, NULL, "Remove home chests and store home items" },
	{ "b5.103", b5_103_update, NULL, "Remove the repair-vehicles workforce chore" },
	{ "b5.104", b5_104_update, NULL, "Remove old data" },
	{ "b5.105", b5_105_update, NULL, "Shut off old workforce chores" },
	{ "b5.106", b5_106_update, b5_106_players, "Fix ruins icons and re-save all players with equipment in the main file" },
	{ "b5.107", NULL, b5_107_players, "Move lastname data" },
	{ "b5.112", b5_112_update, NULL, "Fix bad dedicate data" },
	{ "b5.117", NULL, b5_117_update_players, "Clear last Christmas gift data" },
	{ "b5.119", b5_119_repair_heights, NULL, "Repairing tiles that shouldn't have heights" },
	{ "b5.120", b5_120_resave_world, NULL, "Resaving whole world to clear junk files" },
	{ "b5.121", NULL, b5_121_update_players, "Adding default informative flags" },
	{ "b5.128", b5_128_learned_update, NULL, "Updated learned crafts for trappers posts" },
	{ "b5.130b", b5_130b_item_refresh, b5_130b_player_refresh, "Updated items that need script attachments" },
	{ "b5.134", NULL, b5_134_update_players, "Wiped map memory for screenreader users to clear bad data" },
	{ "b5.151", b5_151_road_fix, NULL, "Applying hide-real-name flag to customized roads" },
	{ "b5.151.1", b5_151_terrain_fix, NULL, "Repairing bad terrains and updating with new oases and irrigated terrains" },
	{ "b5.152", b5_152_world_update, b5_152_player_update, "Updating lights and expire times on player, mob, and world affects" },
	{ "b5.153", NULL, b5_153_player_repair, "Repairing hunger/thirst on players" },
	{ "b5.162", NULL, b5_162_status_messages, "Applying default status messages to players" },
	{ "b5.165", NULL, b5_165_fight_messages, "Adding new fight messages to players" },
	{ "b5.166", b5_166_barrier_magentafication, b5_166_player_update, "Updating enchanted walls, abilities, and affects" },
	{ "b5.168", b5_168_updates, b5_188_greatness, "Removing old DIGGING function flag and updating character greatness" },
	{ "b5.169", b5_169_book_move, NULL, "Renumbering books written by players to resolve vnum conflicts" },
	{ "b5.169.0.1", b5_169_city_centers, NULL, "Applying names to city centers" },
	{ "b5.170", b5_170_timer_updates, NULL, "Applying timers to stored items" },
	{ "b5.170a", b5_170_home_assignments, NULL, "Setting new data for player homes" },
	{ "b5.171", b5_171_bath_triggers, NULL, "Assigning new triggers to baths" },
	{ "b5.172", b5_169_city_centers, NULL, "Re-applying names to city centers to fix hide-real-name" },
	{ "b5.172a", b5_173_tavern_update, NULL, "Applying the tavern update (moved to new progress reward)" },
	{ "b5.174", b5_174_config_change, NULL, "Copying newyear_message to world_reset_message if needed" },
	
	// ADD HERE, above: more beta 5 update lines
	
	{ "\n", NULL, NULL, "\n" }	// must be last
};


 //////////////////////////////////////////////////////////////////////////////
//// CORE FUNCTIONS //////////////////////////////////////////////////////////

/**
* Find the version of the last successful boot.
*
* @return int a position in version_update_data[] or NOTHING if not present
*/
int get_last_boot_version(void) {
	char str[256];
	FILE *fl;
	int iter, pos;
	
	if (!(fl = fopen(VERSION_FILE, "r"))) {
		// if no file, do not run auto-updaters -- skip them
		for (iter = 0; *version_update_data[iter].version != '\n'; ++iter) {
			// just looking for last entry
		}
		return iter - 1;
	}
	
	sprintf(buf, "version file");
	get_line(fl, str);
	fclose(fl);
	
	// find last version
	pos = NOTHING;
	for (iter = 0; *version_update_data[iter].version != '\n'; ++iter) {
		if (!str_cmp(str, version_update_data[iter].version)) {
			pos = iter;
			break;
		}
	}
	
	if (pos == NOTHING) {
		// failed to find the version? find the last version instead
		for (iter = 0; *version_update_data[iter].version != '\n'; ++iter) {
			// just looking for last entry
		}
		pos = iter - 1;
		log("SYSERR: get_last_boot_version got unknown version '%s' from version file '%s' -- skipping auto-updaters and assuming version '%s'", str, VERSION_FILE, version_update_data[pos].version);
	}
	
	return pos;
}


/**
* Writes the version of the last good boot.
*
* @param int version Which version id to write (pos in version_update_data).
*/
void write_last_boot_version(int version) {
	FILE *fl;
	
	if (block_all_saves_due_to_shutdown) {
		return;
	}
	
	if (version == NOTHING) {
		return;
	}
	
	if (!(fl = fopen(VERSION_FILE, "w"))) {
		log("Unable to write version file. This would cause version updates to be applied repeatedly.");
		exit(1);
	}
	
	fprintf(fl, "%s\n", version_update_data[version].version);
	fclose(fl);
}


/**
* Performs some auto-updates when the mud detects a new version.
*/
void check_version(void) {
	int last, iter, current = NOTHING;
	bool any = FALSE;
	
	last = get_last_boot_version();
	
	// updates for every version since the last boot
	for (iter = 0; *version_update_data[iter].version != '\n'; ++iter) {
		current = iter;
		
		// skip versions below last-boot
		if (last != NOTHING && iter <= last) {
			continue;
		}
		
		// ready:
		log("Applying %s update: %s", version_update_data[current].version, NULLSAFE(version_update_data[current].log_str));
		
		// run main updater
		if (version_update_data[iter].update_func) {
			(version_update_data[iter].update_func)();
		}
		
		// run player updater
		if (version_update_data[iter].player_update_func) {
			update_all_players(NULL, version_update_data[iter].player_update_func);
		}
		
		any = TRUE;
	}
	
	// save for next time
	write_last_boot_version(current);
	
	// ensure everything is saved
	if (any) {
		save_island_table();
		save_trading_post();
		run_delayed_refresh();
		free_loaded_players();
		save_all_empires();
		save_world_after_startup = TRUE;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// PRE-B5.116 WORLD LOADING ////////////////////////////////////////////////

#define PRE_B5_116_WORLD_MAP_FILE  LIB_WORLD"base_map"	// storage for the game's base map


/**
* After successfully starting up and saving the new version, this will clean
* up old files including: base_map, world index, world files, obj packs.
*/
void delete_pre_b5_116_world_files(void) {
	int wld_count = 0, pack_count = 0;
	char fname[256];
	glob_t globbuf;
	int iter;
	FILE *fl;
	
	if (access(LIB_WORLD "base_map", F_OK) == 0) {
		log("Deleting: %s", LIB_WORLD "base_map");
		unlink(LIB_WORLD "base_map");
	}
	sprintf(fname, "%s%s", WLD_PREFIX, INDEX_FILE);
	if ((fl = fopen(fname, "w"))) {
		log("Overwriting: %s", fname);
		fprintf(fl, "$\n");
		fclose(fl);
	}
	
	// old world files:
	sprintf(fname, "%s*%s", WLD_PREFIX, WLD_SUFFIX);
	glob(fname, 0, NULL, &globbuf);
	for (iter = 0; iter < globbuf.gl_pathc; ++iter) {
		++wld_count;
		unlink(globbuf.gl_pathv[iter]);
	}
	if (wld_count > 0) {
		log("Deleting: %d world file%s", wld_count, PLURAL(wld_count));
	}
	globfree(&globbuf);
	
	// old pack files:
	sprintf(fname, "%s*%s", LIB_OBJPACK, SUF_PACK);
	glob(fname, 0, NULL, &globbuf);
	for (iter = 0; iter < globbuf.gl_pathc; ++iter) {
		++pack_count;
		unlink(globbuf.gl_pathv[iter]);
	}
	if (pack_count > 0) {
		log("Deleting: %d pack file%s", pack_count, PLURAL(pack_count));
	}
	globfree(&globbuf);
}


/**
* This function helps upgrade EmpireMUDs to b5.116+:
*
* This loads the world_map array from file. This is optional, and this data
* can be overwritten by the actual rooms from the .wld files. This should be
* run after sectors are loaded, and before the .wld files are read in.
*
* @return bool TRUE if we did successfully load an old base_map; FALSE if not.
*/
bool load_pre_b5_116_world_map_from_file(void) {
	void init_map();
	
	char line[256], line2[256], str1[256], str2[256], error_buf[MAX_STRING_LENGTH];
	struct map_data *map, *last = NULL;
	struct depletion_data *dep;
	struct track_data *track;
	int var[8];
	long l_in;
	FILE *fl;
	
	if (!(fl = fopen(PRE_B5_116_WORLD_MAP_FILE, "r"))) {
		// no longer log this: this was a backup option anyway
		// log(" - no %s file, booting without one", PRE_B5_116_WORLD_MAP_FILE);
		return FALSE;
	}
	
	strcpy(error_buf, "base_map file");
	
	// optionals
	while (get_line(fl, line)) {
		if (*line == '$') {
			break;
		}
		
		// new room
		if (isdigit(*line)) {
			// x y island sect base natural crop misc
			if (sscanf(line, "%d %d %d %d %d %d %d %d", &var[0], &var[1], &var[2], &var[3], &var[4], &var[5], &var[6], &var[7]) != 8) {
				var[7] = 0;	// backwards-compatible on the misc
				if (sscanf(line, "%d %d %d %d %d %d %d", &var[0], &var[1], &var[2], &var[3], &var[4], &var[5], &var[6]) != 7) {
					log("Encountered bad line in world map file: %s", line);
					continue;
				}
			}
			if (var[0] < 0 || var[0] >= MAP_WIDTH || var[1] < 0 || var[1] >= MAP_HEIGHT) {
				log("Encountered bad location in world map file: (%d, %d)", var[0], var[1]);
				continue;
			}
		
			map = &(world_map[var[0]][var[1]]);
			sprintf(error_buf, "base_map tile %d", map->vnum);
			
			if (var[3] != BASIC_OCEAN && map->shared == &ocean_shared_data) {
				map->shared = NULL;	// unlink basic ocean
				CREATE(map->shared, struct shared_room_data, 1);
			}
			
			if (map->shared->island_id != var[2]) {
				map->shared->island_id = var[2];
				map->shared->island_ptr = (var[2] == NO_ISLAND ? NULL : get_island(var[2], TRUE));
			}
			
			// these will be validated later
			map->sector_type = sector_proto(var[3]);
			map->base_sector = sector_proto(var[4]);
			map->natural_sector = sector_proto(var[5]);
			map->crop_type = crop_proto(var[6]);
			// var[7] is ignored -- this is misc keys for the evolve.c utility
			
			last = map;	// store in case of more data
		}
		else if (last) {
			switch (*line) {
				case 'E': {	// affects
					if (!get_line(fl, line2)) {
						log("SYSERR: Unable to get E line for map tile #%d", last->vnum);
						break;
					}
					if (sscanf(line2, "%s %s", str1, str2) != 2) {
						if (sscanf(line2, "%s", str1) != 1) {
							log("SYSERR: Invalid E line for map tile #%d", last->vnum);
							break;
						}
						// otherwise backwards-compatible:
						strcpy(str2, str1);
					}

					last->shared->base_affects = asciiflag_conv(str1);
					last->shared->affects = asciiflag_conv(str2);
					break;
				}
				case 'I': {	// icon
					if (last->shared->icon) {
						free(last->shared->icon);
					}
					last->shared->icon = fread_string(fl, error_buf);
					break;
				}
				case 'M': {	// description
					if (last->shared->description) {
						free(last->shared->description);
					}
					last->shared->description = fread_string(fl, error_buf);
					break;
				}
				case 'N': {	// name
					if (last->shared->name) {
						free(last->shared->name);
					}
					last->shared->name = fread_string(fl, error_buf);
					break;
				}
				case 'U': {	// other data (height etc)
					parse_other_shared_data(last->shared, line, error_buf);
					break;
				}
				case 'X': {	// resource depletion
					if (!get_line(fl, line2)) {
						log("SYSERR: Unable to read depletion line of map tile #%d", last->vnum);
						exit(1);
					}
					if ((sscanf(line2, "%d %d", &var[0], &var[1])) != 2) {
						log("SYSERR: Format in depletion line of map tile #%d", last->vnum);
						exit(1);
					}
				
					CREATE(dep, struct depletion_data, 1);
					dep->type = var[0];
					dep->count = var[1];
					LL_PREPEND(last->shared->depletion, dep);
					break;
				}
				case 'Y': {	// tracks
					if (!get_line(fl, line2)) {
						log("SYSERR: Missing Y section of map tile #%d", last->vnum);
						exit(1);
					}
					if (sscanf(line2, "%d %d %ld %d %d", &var[0], &var[1], &l_in, &var[2], &var[3]) != 5) {
						var[3] = NOWHERE;	// to_room: backwards-compatible with old version
						if (sscanf(line2, "%d %d %ld %d", &var[0], &var[1], &l_in, &var[2]) != 4) {
							log("SYSERR: Bad formatting in Y section of map tile #%d", last->vnum);
							exit(1);
						}
					}
					
					// note: var[0] is no longer used (formerly player id)
					HASH_FIND_INT(last->shared->tracks, &var[1], track);
					if (!track) {
						CREATE(track, struct track_data, 1);
						track->id = var[1];
						HASH_ADD_INT(last->shared->tracks, id, track);
					}
					track->timestamp = l_in;
					track->dir = var[2];
					track->to_room = var[3];
					break;
				}
				case 'Z': {	// extra data
					if (!get_line(fl, line2) || sscanf(line2, "%d %d", &var[0], &var[1]) != 2) {
						log("SYSERR: Bad formatting in Z section of map tile #%d", last->vnum);
						exit(1);
					}
					
					set_extra_data(&last->shared->extra_data, var[0], var[1]);
					break;
				}
			}
		}
		else {
			log("Junk data found in base_map file: %s", line);
			exit(0);
		}
	}
	
	fclose(fl);
	converted_to_b5_116 = TRUE;
	return TRUE;
}


/**
* This function helps upgrade EmpireMUDs to b5.116+:
*
* Reads direction data from a file and builds an exit for it.
*
* @param FILE *fl The file to read from.
* @param room_data *room Which room to add an exit to.
* @param int dir The direction to add the exit.
*/
void setup_pre_b5_116_dir(FILE *fl, room_data *room, int dir) {
	struct room_direction_data *ex;
	int t[5];
	char line[256], str_in[256], *tmp;
	
	// ensure we have a building -- although we SHOULD
	if (!COMPLEX_DATA(room)) {
		COMPLEX_DATA(room) = init_complex_data();
	}

	sprintf(buf2, "room #%d, direction D%d", GET_ROOM_VNUM(room), dir);
	
	// keyword
	tmp = fread_string(fl, buf2);
	
	if (!get_line(fl, line)) {
		log("SYSERR: Format error, %s", buf2);
		exit(1);
	}
	if (sscanf(line, " %s %d ", str_in, &t[0]) != 2) {
		log("SYSERR: Format error, %s", buf2);
		exit(1);
	}

	// see if there's already an exit before making one
	if (!(ex = find_exit(room, dir))) {
		CREATE(ex, struct room_direction_data, 1);
		ex->dir = dir;
		LL_PREPEND(COMPLEX_DATA(room)->exits, ex);
	}

	// exit data
	ex->keyword = tmp;
	ex->to_room = t[0];	// this is a vnum
	ex->room_ptr = NULL;	// will be set in renum_world
	ex->exit_info = asciiflag_conv(str_in);

	// sort last
	LL_SORT(COMPLEX_DATA(room)->exits, sort_exits);
}


/**
* This function helps upgrade EmpireMUDs to b5.116+:
*
* Load one room from file.
*
* @param FILE *fl The open file (having just read the # line).
* @param room_vnum vnum The vnum to read in.
*/
void parse_pre_b5_116_room(FILE *fl, room_vnum vnum) {
	char line[256], line2[256], error_buf[256], error_log[MAX_STRING_LENGTH], str1[256], str2[256];
	double dbl_in;
	long l_in;
	int t[10];
	struct depletion_data *dep;
	struct reset_com *reset;
	struct affected_type *af;
	struct track_data *track;
	room_data *room, *find;
	bool error = FALSE;
	char *ptr;

	sprintf(error_buf, "room #%d", vnum);
	
	CREATE(room, room_data, 1);
	
	// basic setup: things that don't default to 0/NULL
	room->vnum = vnum;
	
	// attach/create shared data
	if (vnum < MAP_SIZE) {
		GET_MAP_LOC(room) = &(world_map[MAP_X_COORD(vnum)][MAP_Y_COORD(vnum)]);
		SHARED_DATA(room) = GET_MAP_LOC(room)->shared;
		GET_MAP_LOC(room)->room = room;
	}
	else {
		CREATE(SHARED_DATA(room), struct shared_room_data, 1);
	}
	
	HASH_FIND_INT(world_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate room vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	
	// put it in the hash table
	add_room_to_world_tables(room);
	
	// FIRST LINE: sector original subtype progress
	if (!get_line(fl, line)) {
		log("SYSERR: Expecting sector line of room #%d but file ended!", vnum);
		exit(1);
	}

	if (sscanf(line, "%d %d %d", &t[0], &t[1], &t[2]) != 3) {
		log("SYSERR: Format error in sector line of room #%d", vnum);
		exit(1);
	}
	
	// need to unlink shared data, if present, if this is not an ocean
	if (t[1] != BASIC_OCEAN && SHARED_DATA(room) == &ocean_shared_data) {
		// converting from ocean to non-ocean
		SHARED_DATA(room) = NULL;	// unlink ocean share
		CREATE(SHARED_DATA(room), struct shared_room_data, 1);
		
		if (GET_MAP_LOC(room)) {	// and pass this shared data back up to the world
			GET_MAP_LOC(room)->shared = SHARED_DATA(room);
		}
	}
	
	GET_ISLAND_ID(room) = t[0];
	SHARED_DATA(room)->island_ptr = (t[0] == NO_ISLAND ? NULL : get_island(t[0], TRUE));
	room->sector_type = sector_proto(t[1]);
	room->base_sector = sector_proto(t[2]);
	
	if (GET_MAP_LOC(room)) {
		GET_MAP_LOC(room)->sector_type = room->sector_type;
		GET_MAP_LOC(room)->base_sector = room->base_sector;
	}

	// set up building data?
	if (IS_ANY_BUILDING(room) || IS_ADVENTURE_ROOM(room)) {
		COMPLEX_DATA(room) = init_complex_data();
	}

	// extra data
	sprintf(error_log, "SYSERR: Format error in room #%d (expecting D/E/S)", vnum);

	for (;;) {
		if (!get_line(fl, line)) {
			log("%s", error_log);
			exit(1);
		}
		switch (*line) {
			case 'A': {	// affects
				if (!get_line(fl, line2) || sscanf(line2, "%d %d %ld %d %d %s", &t[0], &t[1], &l_in, &t[3], &t[4], str1) != 6) {
					log("SYSERR: Format error in A line of room #%d", vnum);
					exit(1);
				}
				
				CREATE(af, struct affected_type, 1);
				af->type = t[0];
				af->cast_by = t[1];
				af->expire_time = l_in;
				af->modifier = t[3];
				af->location = t[4];
				af->bitvector = asciiflag_conv(str1);
				
				LL_PREPEND(ROOM_AFFECTS(room), af);
				break;
			}
			case 'B': {	// building data
				if (!get_line(fl, line2) || sscanf(line2, "%d %d %d %d %ld %lf %d %d", &t[0], &t[1], &t[2], &t[3], &l_in, &dbl_in, &t[6], &t[7]) != 8) {
					log("SYSERR: Format error in B line of room #%d", vnum);
					exit(1);
				}
				
				// ensure has building
				if (!COMPLEX_DATA(room)) {
					COMPLEX_DATA(room) = init_complex_data();
				}
				
				if (t[0] != NOTHING) {
					attach_building_to_room(building_proto(t[0]), room, FALSE);
				}
				if (t[1] != NOTHING) {
					attach_template_to_room(room_template_proto(t[1]), room);
				}
				
				COMPLEX_DATA(room)->entrance = t[2];
				// see below for t[3]
				COMPLEX_DATA(room)->burn_down_time = l_in;
				COMPLEX_DATA(room)->damage = dbl_in;	// formerly t[5], which is now unused
				COMPLEX_DATA(room)->private_owner = t[6];
				
				// b5.108: now converts the dedicate id / paint color if it sees it here; it's now saved as extra data
				if (t[3] != 0) {
					set_room_extra_data(room, ROOM_EXTRA_DEDICATE_ID, t[3]);
				}
				if (t[7] != 0) {
					set_room_extra_data(room, ROOM_EXTRA_PAINT_COLOR, t[7]);
				}
				break;
			}
			case 'C': { // reset command
				if (!*(line + 1) || !*(line + 2) || *(line + 2) == '*') {
					// skip
					break;
				}
			
				CREATE(reset, struct reset_com, 1);
				DL_APPEND(room->reset_commands, reset);
				
				// load data
				reset->command = *(line + 2);
				ptr = line + 3;
				
				if (reset->command == 'V') { // V: script variable reset
					// trigger_type misc? room_vnum sarg1 sarg2
					if (sscanf(line + 3, " %lld %lld %lld %79s %79[^\f\n\r\t\v]", &reset->arg1, &reset->arg2, &reset->arg3, str1, str2) != 5) {
						error = TRUE;
					}
					else {
						reset->sarg1 = strdup(str1);
						reset->sarg2 = strdup(str2);
					}
				}
				else if (strchr("Y", reset->command) != NULL) {	// generic-mob: 3-arg command
					// generic-sex generic-name empire-id
					if (sscanf(ptr, " %lld %lld %lld ", &reset->arg1, &reset->arg2, &reset->arg3) != 3) {
						error = TRUE;
					}
				}
				else if (strchr("CDT", reset->command) != NULL) {	/* C, D, Trigger: a 2-arg command */
					// trigger_type trigger_vnum
					if (sscanf(ptr, " %lld %lld ", &reset->arg1, &reset->arg2) != 2) {
						error = TRUE;
					}
				}
				else if (strchr("M", reset->command) != NULL) {	// Mob: 3 args
					if (sscanf(ptr, " %lld %s %lld ", &reset->arg1, str1, &reset->arg3) != 3) {
						error = TRUE;
					}
					else {
						// this was formerly saved as ascii flags
						reset->arg2 = asciiflag_conv(str1);
					}
				}
				else if (strchr("I", reset->command) != NULL) {	/* a 1-arg command */
					if (sscanf(ptr, " %lld ", &reset->arg1) != 1) {
						error = TRUE;
					}
				}
				else if (strchr("O", reset->command) != NULL) {
					// 0-arg: nothing to do
				}
				else if (strchr("S", reset->command) != NULL) {	// mob custom strings: <string type> <value>
					skip_spaces(&ptr);
					reset->sarg1 = strdup(ptr);
				}
				else {	// all other types (1-arg?)
					if (sscanf(ptr, " %lld ", &reset->arg1) != 1) {
						error = TRUE;
					}
				}

				if (error) {
					log("SYSERR: Format error in room %d, zone command: '%s'", vnum, line);
					exit(1);
				}
				
				break;
			}
			case 'D': {	// door
				setup_pre_b5_116_dir(fl, room, atoi(line + 1));
				break;
			}
			case 'E': {	// affects
				if (!get_line(fl, line2)) {
					log("SYSERR: Unable to get E line for room #%d", vnum);
					break;
				}
				if (sscanf(line2, "%s %s", str1, str2) != 2) {
					if (sscanf(line2, "%s", str1) != 1) {
						log("SYSERR: Invalid E line for room #%d", vnum);
						break;
					}
					// otherwise backwards-compatible:
					strcpy(str2, str1);
				}

				ROOM_BASE_FLAGS(room) = asciiflag_conv(str1);
				ROOM_AFF_FLAGS(room) = asciiflag_conv(str2);
				break;
			}
			case 'H': {	// home_room
				// ensure it has building data
				if (!COMPLEX_DATA(room)) {
					COMPLEX_DATA(room) = init_complex_data();
				}

				// actual home room will be set later
				add_trd_home_room(vnum, atoi(line + 1));
				break;
			}
			case 'I': {	// icon
				if (ROOM_CUSTOM_ICON(room)) {
					free(ROOM_CUSTOM_ICON(room));
				}
				ROOM_CUSTOM_ICON(room) = fread_string(fl, error_buf);
				break;
			}
			case 'M': {	// description
				if (ROOM_CUSTOM_DESCRIPTION(room)) {
					free(ROOM_CUSTOM_DESCRIPTION(room));
				}
				ROOM_CUSTOM_DESCRIPTION(room) = fread_string(fl, error_buf);
				break;
			}
			case 'N': {	// name
				if (ROOM_CUSTOM_NAME(room)) {
					free(ROOM_CUSTOM_NAME(room));
				}
				ROOM_CUSTOM_NAME(room) = fread_string(fl, error_buf);
				break;
			}
			case 'O': {	// owner (empire_vnum)
				add_trd_owner(vnum, atoi(line+1));
				break;
			}
			case 'P': { // crop (plants)
				set_crop_type(room, crop_proto(atoi(line+1)));
				break;
			}
			case 'R': {	/* resources */
				if (!COMPLEX_DATA(room)) {	// ensure complex data
					COMPLEX_DATA(room) = init_complex_data();
				}
				
				parse_resource(fl, &GET_BUILDING_RESOURCES(room), error_log);
				break;
			}
			
			case 'T': {	// trigger (deprecated)
				// NOTE: prior to b2.11, trigger prototypes were saved and read
				// this way they are no longer saved this way at all, but this
				// must be left in to be backwards-compatbile. If your mud has
				// been up since b2.11, you can safely remove this block.
				parse_trig_proto(line, &(room->proto_script), error_log);
				break;
			}
			
			case 'U': {	// other data (height etc)
				parse_other_shared_data(SHARED_DATA(room), line, error_buf);
				break;
			}
			
			case 'X': {	// resource depletion
				if (!get_line(fl, line2)) {
					log("SYSERR: Unable to read depletion line of room #%d", vnum);
					exit(1);
				}
				if ((sscanf(line2, "%d %d", t, t+1)) != 2) {
					log("SYSERR: Format in depletion line of room #%d", vnum);
					exit(1);
				}
				
				CREATE(dep, struct depletion_data, 1);
				dep->type = t[0];
				dep->count = t[1];
				LL_PREPEND(ROOM_DEPLETION(room), dep);
				
				break;
			}
			
			case 'W': {	// built-with resources
				if (!COMPLEX_DATA(room)) {	// ensure complex data
					COMPLEX_DATA(room) = init_complex_data();
				}
				
				parse_resource(fl, &GET_BUILT_WITH(room), error_log);
				break;
			}
			
			case 'Y': {	// tracks
				if (!get_line(fl, line2)) {
					log("SYSERR: Missing Y section of room #%d", vnum);
					exit(1);
				}
				if (sscanf(line2, "%d %d %ld %d %d", &t[0], &t[1], &l_in, &t[2], &t[3]) != 5) {
					t[3] = NOWHERE;	// to_room: backwards-compatible with old version
					if (sscanf(line2, "%d %d %ld %d", &t[0], &t[1], &l_in, &t[2]) != 4) {
						log("SYSERR: Bad formatting in Y section of room #%d", vnum);
						exit(1);
					}
				}
				
				// t[0] is no longer used at all (formerly player id)
				HASH_FIND_INT(ROOM_TRACKS(room), &t[1], track);
				if (!track) {
					CREATE(track, struct track_data, 1);
					track->id = t[1];
					HASH_ADD_INT(ROOM_TRACKS(room), id, track);
				}
				track->timestamp = l_in;
				track->dir = t[2];
				track->to_room = t[3];
				break;
			}
			
			case 'Z': {	// extra data
				if (!get_line(fl, line2) || sscanf(line2, "%d %d", &t[0], &t[1]) != 2) {
					log("SYSERR: Bad formatting in Z section of room #%d", vnum);
					exit(1);
				}
				
				set_room_extra_data(room, t[0], t[1]);
				break;
			}

			case 'S': {	// end of room
				return;
			}
			default: {
				log("%s", error_log);
				exit(1);
			}
		}
	}
}
