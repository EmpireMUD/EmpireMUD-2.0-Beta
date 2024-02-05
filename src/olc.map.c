/* ************************************************************************
*   File: olc.map.c                                       EmpireMUD 2.0b5 *
*  Usage: OLC for the map and map-building rooms                          *
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
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "comm.h"
#include "olc.h"
#include "vnums.h"
#include "dg_scripts.h"
#include "constants.h"

/**
* Contents:
*   Displays
*   Edit Modules
*/

 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(mapedit_build) {
	char bld_arg[MAX_INPUT_LENGTH], dir_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	bld_data *bld;
	int dir = NO_DIR;
	
	half_chop(argument, bld_arg, dir_arg);
	
	if (IS_INSIDE(IN_ROOM(ch)) || IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
		msg_to_char(ch, "Leave the building or area first.\r\n");
	}
	else if (IS_CITY_CENTER(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't build over city centers this way.\r\n");
	}
	else if (!*bld_arg || !isdigit(*bld_arg)) {
		msg_to_char(ch, "Build which building vnum?\r\n");
	}
	else if (!(bld = building_proto(atoi(bld_arg)))) {
		msg_to_char(ch, "Unknown building vnum '%s'.\r\n", bld_arg);
	}
	else if (IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM)) {
		msg_to_char(ch, "You cannot build 'room' buildings this way.\r\n");
	}
	else if (!IS_SET(GET_BLD_FLAGS(bld), BLD_OPEN) && !*dir_arg) {
		msg_to_char(ch, "Build it facing which direction?\r\n");
	}
	else if (!IS_SET(GET_BLD_FLAGS(bld), BLD_OPEN) && (dir = parse_direction(ch, dir_arg)) == NO_DIR) {
		msg_to_char(ch, "Invalid direction.\r\n");
	}
	else if (dir != NO_DIR && dir >= NUM_2D_DIRS) {
		msg_to_char(ch, "Invalid direction.\r\n");
	}
	else if (dir != NO_DIR && (!SHIFT_DIR(IN_ROOM(ch), dir) || (IS_SET(GET_BLD_FLAGS(bld), BLD_TWO_ENTRANCES) && !SHIFT_DIR(IN_ROOM(ch), rev_dir[dir])))) {
		msg_to_char(ch, "You can't face it that direction.\r\n");
	}
	else {
		disassociate_building(IN_ROOM(ch));
		
		construct_building(IN_ROOM(ch), GET_BLD_VNUM(bld));
		
		if (dir != NO_DIR) {
			create_exit(IN_ROOM(ch), SHIFT_DIR(IN_ROOM(ch), dir), dir, FALSE);
			if (IS_SET(GET_BLD_FLAGS(bld), BLD_TWO_ENTRANCES)) {
				create_exit(IN_ROOM(ch), SHIFT_DIR(IN_ROOM(ch), rev_dir[dir]), rev_dir[dir], FALSE);
			}
			COMPLEX_DATA(IN_ROOM(ch))->entrance = rev_dir[dir];
			herd_animals_out(IN_ROOM(ch));
		}
		
		complete_building(IN_ROOM(ch));
		special_building_setup(ch, IN_ROOM(ch));

		msg_to_char(ch, "You create %s %s!\r\n", AN(GET_BLD_NAME(bld)), GET_BLD_NAME(bld));
		sprintf(buf, "$n creates %s %s!", AN(GET_BLD_NAME(bld)), GET_BLD_NAME(bld));
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		request_char_save_in_world(ch);
	}
}


OLC_MODULE(mapedit_convert2newbie) {
	bool undo = FALSE, any;
	int count, iter, island_id = NO_ISLAND;
	sector_data *base;
	struct island_info *isle;
	struct map_data *map;
	room_data *room;

	// maps sects between normal temperate islands and newbie islands for automatic conversion
	struct {
		any_vnum normal_sect;	// e.g. [0] Plains
		any_vnum newbie_sect;	// e.g. [100] Plains (newbie)
		bool reversible;	// if FALSE, only converts normal->newbie, not newbie->normal
	} newbie_sector_map[] = {
		{ 0, 100, TRUE },	// Plains
		{ 1, 101, TRUE },	// Light Forest
		{ 2, 102, TRUE },	// Forest
		{ 3, 103, TRUE },	// Shady Forest
		{ 4, 104, TRUE },	// Overgrown Forest
		{ 7, 107, TRUE },	// Crop
		{ 13, 113, TRUE },	// Seeded Field
		{ 36, 136, TRUE },	// Stumps
		{ 37, 137, TRUE },	// Small Copse
		{ 38, 138, TRUE },	// Copse
		{ 39, 139, TRUE },	// Forest Edge
		{ 40, 140, TRUE },	// Riverbank
		{ 44, 144, TRUE },	// Light Riverbank Forest
		{ 45, 145, TRUE },	// Forested Riverbank
		{ 46, 146, TRUE },	// Stumped Riverbank
		{ 47, 147, TRUE },	// Riverside Copse
		{ 50, 150, TRUE },	// Shore
		{ 54, 154, TRUE },	// Shoreside Tree
		{ 56, 156, TRUE },	// Estuary Shore
		{ 58, 158, TRUE },	// Foothills
		{ 59, 159, TRUE },	// Seaside Stumps
		{ 60, 160, TRUE },	// Seaside Copse
		
		// convert
		{ 41, 140, FALSE },	// Floodplains
		{ 42, 144, FALSE }, // Flooded Woods
		{ 43, 145, FALSE },	// Flooded Forest
		{ 90, 104, FALSE },	// Overgrown Forest
		
		// remove
		{ 17, 100, FALSE },	// Trench
		{ 19, 100, FALSE },	// Canal
		
		{ NOTHING, NOTHING, FALSE }	// list end
	};
	
	// validate location
	if ((island_id = GET_ISLAND_ID(IN_ROOM(ch))) == NO_ISLAND) {
		msg_to_char(ch, "You can't do that while not on any island.\r\n");
		return;
	}
	
	// validate arg
	skip_spaces(&argument);
	if (!str_cmp(argument, "undo")) {
		undo = TRUE;
	}
	else if (str_cmp(argument, "confirm")) {
		msg_to_char(ch, "You must type: .map convert2newbie confirm\r\n");
		return;
	}
	
	count = 0;
	
	// check all land tiles
	LL_FOREACH(land_map, map) {
		if (map->shared->island_id != island_id) {
			continue;	// wrong island
		}
		
		room = map->room;	// may or may not exist
		base = map->base_sector;
		any = FALSE;
		
		if (room) {
			for (iter = 0; newbie_sector_map[iter].normal_sect != NOTHING; ++iter) {
				if (!undo) {
					// normal -> newbie
					if (GET_SECT_VNUM(map->natural_sector) == newbie_sector_map[iter].normal_sect) {
						set_natural_sector(map, sector_proto(newbie_sector_map[iter].newbie_sect));
						any = TRUE;
					}
					if (GET_SECT_VNUM(map->sector_type) == newbie_sector_map[iter].normal_sect) {
						change_terrain(room, newbie_sector_map[iter].newbie_sect, GET_SECT_VNUM(map->base_sector));
						any = TRUE;
					}
					if (GET_SECT_VNUM(base) == newbie_sector_map[iter].normal_sect) {
						change_base_sector(room, sector_proto(newbie_sector_map[iter].newbie_sect));
						any = TRUE;
					}
				}
				
				if (undo && newbie_sector_map[iter].reversible) {
					// newbie -> normal
					if (GET_SECT_VNUM(map->natural_sector) == newbie_sector_map[iter].newbie_sect) {
						set_natural_sector(map, sector_proto(newbie_sector_map[iter].normal_sect));
						any = TRUE;
					}
					if (GET_SECT_VNUM(map->sector_type) == newbie_sector_map[iter].newbie_sect) {
						change_terrain(room, newbie_sector_map[iter].normal_sect, GET_SECT_VNUM(map->base_sector));
						any = TRUE;
					}
					if (GET_SECT_VNUM(base) == newbie_sector_map[iter].newbie_sect) {
						change_base_sector(room, sector_proto(newbie_sector_map[iter].normal_sect));
						any = TRUE;
					}
				}
			}
			
			// no longer need this
			remove_room_extra_data(room, ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR);
		}
		else {	// map only, no room
			for (iter = 0; newbie_sector_map[iter].normal_sect != NOTHING; ++iter) {
				if (!undo) {
					// normal -> newbie
					if (GET_SECT_VNUM(map->natural_sector) == newbie_sector_map[iter].normal_sect) {
						set_natural_sector(map, sector_proto(newbie_sector_map[iter].newbie_sect));
						any = TRUE;
					}
					if (GET_SECT_VNUM(map->sector_type) == newbie_sector_map[iter].normal_sect) {
						perform_change_sect(NULL, map, sector_proto(newbie_sector_map[iter].newbie_sect));
						any = TRUE;
					}
					if (GET_SECT_VNUM(base) == newbie_sector_map[iter].normal_sect) {
						perform_change_base_sect(NULL, map, sector_proto(newbie_sector_map[iter].newbie_sect));
						any = TRUE;
					}
				}
				
				if (undo && newbie_sector_map[iter].reversible) {
					// newbie -> normal
					if (GET_SECT_VNUM(map->natural_sector) == newbie_sector_map[iter].newbie_sect) {
						set_natural_sector(map, sector_proto(newbie_sector_map[iter].normal_sect));
						any = TRUE;
					}
					if (GET_SECT_VNUM(map->sector_type) == newbie_sector_map[iter].newbie_sect) {
						perform_change_sect(NULL, map, sector_proto(newbie_sector_map[iter].normal_sect));
						any = TRUE;
					}
					if (GET_SECT_VNUM(base) == newbie_sector_map[iter].newbie_sect) {
						perform_change_base_sect(NULL, map, sector_proto(newbie_sector_map[iter].normal_sect));
						any = TRUE;
					}
				}
			}
			
			// no longer need this
			remove_extra_data(&map->shared->extra_data, ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR);
		}
		if (any) {
			request_world_save(map->vnum, WSAVE_ROOM | WSAVE_MAP);
			++count;
		}
	}
	
	if (count > 0) {
		isle = get_island(island_id, TRUE);
		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has %s on %d tile%s on island %d %s", GET_NAME(ch), (undo ? "undone convert2newbie" : "used convert2newbie"), count, PLURAL(count), island_id, isle->name);
		msg_to_char(ch, "You have %s on %d tile%s on this island.\r\n", (undo ? "undone convert2newbie" : "used convert2newbie"), count, PLURAL(count));
	}
	else {
		msg_to_char(ch, "Found no tiles to convert on this island.\r\n");
	}
}


OLC_MODULE(mapedit_decay) {
	room_data *room = HOME_ROOM(IN_ROOM(ch));
	vehicle_data *veh;
	
	one_argument(argument, arg);
	
	if (*arg) {
		if ((veh = get_vehicle_in_room_vis(ch, arg, NULL))) {
			msg_to_char(ch, "Ok.\r\n");
			annual_update_vehicle(veh);
		}
		else {
			msg_to_char(ch, "You don't see %s %s here.\r\n", AN(arg), arg);
		}
	}
	else if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		msg_to_char(ch, "You can only decay map tiles.\r\n");
	}
	else {
		msg_to_char(ch, "Ok.\r\n");
		annual_update_map_tile(&(world_map[FLAT_X_COORD(room)][FLAT_Y_COORD(room)]));
		annual_update_depletions(&(SHARED_DATA(room)->depletion));
	}
}


OLC_MODULE(mapedit_terrain) {
	struct empire_city_data *city;
	empire_data *emp, *rescan_emp = NULL;
	int count;
	sector_data *sect = NULL, *next_sect, *old_sect = NULL;
	crop_data *crop, *next_crop;
	crop_data *cp = NULL;
	
	if (isdigit(*argument)) {
		sect = sector_proto(atoi(argument));
	}
	else {
		sect = get_sect_by_name(argument);
		cp = get_crop_by_name(argument);
	}

	if (IS_INSIDE(IN_ROOM(ch)) || IS_ADVENTURE_ROOM(IN_ROOM(ch)))
		msg_to_char(ch, "Leave the building or area first.\r\n");
	else if (!*argument || (!sect && !cp)) {
		msg_to_char(ch, "What type of terrain would you like to set?\r\nYour choices are:");
		
		count = 0;
		HASH_ITER(hh, sector_table, sect, next_sect) {
			msg_to_char(ch, "%s %s", (count++ > 0 ? "," : ""), GET_SECT_NAME(sect));
		}
		HASH_ITER(hh, crop_table, crop, next_crop) {
			msg_to_char(ch, ", %s", GET_CROP_NAME(crop));
		}
		
		msg_to_char(ch, "\r\n");
	}
	else if (sect && SECT_FLAGGED(sect, SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE)) {
		msg_to_char(ch, "That sector requires extra data and can't be set this way.\r\n");
	}
	else {
		old_sect = BASE_SECT(IN_ROOM(ch));
		emp = ROOM_OWNER(IN_ROOM(ch));

		// delete city center?
		if (IS_CITY_CENTER(IN_ROOM(ch)) && emp && (city = find_city_entry(emp, IN_ROOM(ch)))) {
			log_to_empire(emp, ELOG_TERRITORY, "%s was lost", city->name);
			LL_DELETE(EMPIRE_CITY_LIST(emp), city);
			if (city->name) {
				free(city->name);
			}
			free(city);
			rescan_emp = emp;
			EMPIRE_NEEDS_SAVE(emp) = TRUE;
			write_city_data_file();
		}
		
		if (sect) {
			msg_to_char(ch, "This room is now %s %s.\r\n", AN(GET_SECT_NAME(sect)), GET_SECT_NAME(sect));
			// TODO: special-cased to preserve sect for roads, consider a flag like SECTF_PRESERVE_BASE
			change_terrain(IN_ROOM(ch), GET_SECT_VNUM(sect), SECT_FLAGGED(sect, SECTF_IS_ROAD) ? GET_SECT_VNUM(old_sect) : NOTHING);
			if (ROOM_OWNER(IN_ROOM(ch))) {
				deactivate_workforce_room(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch));
			}
		}
		else if (cp) {
			if (!(sect = find_first_matching_sector(SECTF_CROP, NOBITS, get_climate(IN_ROOM(ch))))) {
				msg_to_char(ch, "No crop sector types are set up.\r\n");
				return;
			}
			else {
				msg_to_char(ch, "This room is now %s.\r\n", GET_CROP_NAME(cp));
				change_terrain(IN_ROOM(ch), GET_SECT_VNUM(sect), NOTHING);
				set_crop_type(IN_ROOM(ch), cp);
				if (ROOM_OWNER(IN_ROOM(ch))) {
					deactivate_workforce_room(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch));
				}
			}
		}
		
		if (sect && SECT_FLAGGED(sect, SECTF_IS_TRENCH)) {
			finish_trench(IN_ROOM(ch));	// fills it or schedules it
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR, GET_SECT_VNUM(old_sect));
		}
		else {
			remove_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR);
		}
		
		// rescan territory at the end
		if (rescan_emp) {
			read_empire_territory(rescan_emp, FALSE);
		}
	}
}


OLC_MODULE(mapedit_complete_room) {	
	if (IS_DISMANTLING(IN_ROOM(ch))) {
		msg_to_char(ch, "Use '.map terrain' instead.\r\n");
		return;
	}
	if (!IS_INCOMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "It is already complete.\r\n");
		return;
	}
	
	act("The building knits itself together out of the ether.", FALSE, ch, NULL, NULL, TO_CHAR | TO_ROOM);
	complete_building(IN_ROOM(ch));
}


OLC_MODULE(mapedit_grow) {
	sector_data *preserve;
	struct evolution_data *evo;
	
	// percentage is checked in the evolution data
	if ((evo = get_evolution_by_type(SECT(IN_ROOM(ch)), EVO_MAGIC_GROWTH))) {
		preserve = (BASE_SECT(IN_ROOM(ch)) != SECT(IN_ROOM(ch))) ? BASE_SECT(IN_ROOM(ch)) : NULL;
		
		// messaging based on whether or not it's choppable
		msg_to_char(ch, "You cause the room to grow around you.\r\n");
		act("$n causes the room to grow around you.", FALSE, ch, NULL, NULL, TO_ROOM);
		
		change_terrain(IN_ROOM(ch), evo->becomes, preserve ? GET_SECT_VNUM(preserve) : NOTHING);
		
		remove_depletion(IN_ROOM(ch), DPLTN_PICK);
		remove_depletion(IN_ROOM(ch), DPLTN_FORAGE);
	}
	else {
		msg_to_char(ch, "There's nothing to grow here (or a random growth chance failed).\r\n");
	}
}


OLC_MODULE(mapedit_height) {
	if (IS_INSIDE(IN_ROOM(ch)) || IS_ADVENTURE_ROOM(IN_ROOM(ch)) || GET_ROOM_VNUM(ch) >= MAP_SIZE) {
		msg_to_char(ch, "You can only set heights on the map.\r\n");
	}
	else if (!*argument || !is_number(argument)) {
		msg_to_char(ch, "You must specify the height as a number.\r\n");
	}
	else {
		set_room_height(IN_ROOM(ch), atoi(argument));
		msg_to_char(ch, "This tile now has a height of %d.\r\n", ROOM_HEIGHT(IN_ROOM(ch)));
	}
}


OLC_MODULE(mapedit_maintain) {
	if (IS_DISMANTLING(IN_ROOM(ch)) || !IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only maintain completed buildings.\r\n");
		return;
	}
	
	finish_maintenance(ch, IN_ROOM(ch));
	msg_to_char(ch, "Done.\r\n");
}


OLC_MODULE(mapedit_unclaimable) {
	int hor, ver, radius;
	bool set = !ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE);
	bool any_land = FALSE;
	room_data *to;
	
	if (IS_INSIDE(IN_ROOM(ch)) || IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't do that inside.\r\n");
		return;
	}
	if (*argument && !isdigit(*argument)) {
		msg_to_char(ch, "Usage: unclaimable <radius>\r\n");
		return;
	}
	
	// this will be 0 if no argument (as desired)
	radius = atoi(argument);
		
	for (hor = -1 * radius; hor <= radius; ++hor) {
		for (ver = -1 * radius; ver <= radius; ++ver) {
			to = real_shift(IN_ROOM(ch), hor, ver);
			
			if (to && GET_ISLAND_ID(to) != NO_ISLAND) {
				any_land = TRUE;
				if (set) {
					SET_BIT(ROOM_BASE_FLAGS(to), ROOM_AFF_UNCLAIMABLE);
					affect_total_room(to);
					abandon_room(to);
				}
				else {
					REMOVE_BIT(ROOM_BASE_FLAGS(to), ROOM_AFF_UNCLAIMABLE);
					affect_total_room(to);
				}
			}
		}
	}
	
	if (!any_land) {
		msg_to_char(ch, "Only ocean found -- ocean cannot be set unclaimable.\r\n");
	}
	else if (radius == 0) {
		msg_to_char(ch, "You set this tile %s.\r\n", (set ? "unclaimable" : "no longer unclaimable"));
	}
	else {
		msg_to_char(ch, "You set the region within %d tiles %s.\r\n", radius, (set ? "unclaimable" : "no longer unclaimable"));
	}
}


OLC_MODULE(mapedit_pass_walls) {
	if (PLR_FLAGGED(ch, PLR_UNRESTRICT)) {
		REMOVE_BIT(PLR_FLAGS(ch), PLR_UNRESTRICT);
		msg_to_char(ch, "You deactivate pass-walls.\r\n");
		}
	else {
		SET_BIT(PLR_FLAGS(ch), PLR_UNRESTRICT);
		msg_to_char(ch, "You activate pass-walls.\r\n");
	}
}


OLC_MODULE(mapedit_decustomize) {
	decustomize_room(IN_ROOM(ch));
	msg_to_char(ch, "All customizations removed on this room/area.\r\n");
}


OLC_MODULE(mapedit_room_name) {
	if (!*argument)
		msg_to_char(ch, "What would you like to name this room/tile (or \"none\")?\r\n");
	else if (!str_cmp(argument, "none")) {
		set_room_custom_name(IN_ROOM(ch), NULL);
		msg_to_char(ch, "This room/tile no longer has a specialized name.\r\n");
	}
	else {
		set_room_custom_name(IN_ROOM(ch), argument);
		msg_to_char(ch, "This room/tile is now called \"%s\".\r\n", argument);
	}
}


OLC_MODULE(mapedit_icon) {
	delete_doubledollar(argument);

	if (IS_INSIDE(IN_ROOM(ch)) || IS_ADVENTURE_ROOM(IN_ROOM(ch)))
		msg_to_char(ch, "You may not do that here.\r\n");
	else if (!str_cmp(argument, "none")) {
		set_room_custom_icon(IN_ROOM(ch), NULL);
		msg_to_char(ch, "This area no longer has a specialized icon.\r\n");
	}
	else if (!*argument)
		msg_to_char(ch, "What would you like to set the icon to (or \"none\")?\r\n");
	else if (!validate_icon(argument, 4))
		msg_to_char(ch, "Room icons must be exactly four characters.\r\n");
	else if (argument[0] != COLOUR_CHAR)
		msg_to_char(ch, "Icons must begin with a color code.\r\n");
	else {
		set_room_custom_icon(IN_ROOM(ch), argument);
		msg_to_char(ch, "This area now has the icon \"%s&0\".\r\n", argument);
	}
}


OLC_MODULE(mapedit_delete_room) {
	char_data *c, *next_c;
	room_data *in_room, *home;

	if (str_cmp(argument, "ok"))
		msg_to_char(ch, "You MUST type \".map deleteroom ok\". This will delete the room you're in.\r\n");
	else if (!IS_INSIDE(IN_ROOM(ch)) || GET_ROOM_VNUM(IN_ROOM(ch)) < MAP_SIZE)
		msg_to_char(ch, "You may not delete this room.\r\n");
	else {
		in_room = IN_ROOM(ch);
		home = HOME_ROOM(in_room);
		
		DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_c, next_in_room) {
			char_to_room(c, home ? home : find_load_room(c));
			act("$n appears in front of you.", TRUE, c, NULL, NULL, TO_ROOM);
			if (c != ch) {
				msg_to_char(c, "Room deleted.\r\n");
			}
			look_at_room(c);
			msdp_update_room(c);
		}
		
		delete_room(in_room, TRUE);
		msg_to_char(ch, "Room deleted.\r\n");
	}
}


OLC_MODULE(mapedit_room_description) {
	if (!*argument) {
		msg_to_char(ch, "To set a description, use \".map description set\".\r\n");
		msg_to_char(ch, "To remove a description, use \".map description none\".\r\n");
	}
	else if (is_abbrev(argument, "none")) {
		set_room_custom_description(IN_ROOM(ch), NULL);
		msg_to_char(ch, "This area no longer has a specialized description.\r\n");
	}
	else if (is_abbrev(argument, "set")) {
		if (ch->desc->str) {
			msg_to_char(ch, "You are already editing a string.\r\n");
		}
		else {
			start_string_editor(ch->desc, "room description", &(ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))), MAX_ROOM_DESCRIPTION, TRUE);
			ch->desc->save_room_id = GET_ROOM_VNUM(IN_ROOM(ch));
			// warning: doesn't necessarily trigger a save
		}
	}
	else
		msg_to_char(ch, "You must specify whether you want to set or remove the description.\r\n");
}


OLC_MODULE(mapedit_ruin) {
	room_data *room = HOME_ROOM(IN_ROOM(ch));
	vehicle_data *veh;
	
	one_argument(argument, arg);
	
	if (*arg && (veh = get_vehicle_in_room_vis(ch, arg, NULL))) {
		ruin_vehicle(veh, "$V is ruined.");
	}
	else if (*arg) {
		msg_to_char(ch, "You don't see that here.\r\n");
	}
	else if (GET_ROOM_VNUM(room) >= MAP_SIZE || !GET_BUILDING(room)) {
		msg_to_char(ch, "You can only ruin map buildings and vehicles.\r\n");
	}
	else {
		msg_to_char(ch, "Ok.\r\n");
		if (IS_CITY_CENTER(room)) {
			disassociate_building(room);
			write_city_data_file();
		}
		else {
			ruin_one_building(room);
		}
	}
}


OLC_MODULE(mapedit_exits) {
	int dir, rev;
	room_data *to_room = NULL;
	bool new = FALSE;

	two_arguments(argument, arg, buf);

	if (!str_cmp(buf, "new"))
		new = TRUE;
	else
		to_room = real_room(atoi(buf));

	if (!COMPLEX_DATA(IN_ROOM(ch)) || !IS_ANY_BUILDING(IN_ROOM(ch)))
		msg_to_char(ch, "You can't create exits here!\r\n");
	else if (!*arg || !*buf)
		msg_to_char(ch, "Usage: .map exit <direction> <virtual number/new>\r\n");
	else if ((dir = parse_direction(ch, arg)) == NO_DIR || dir == DIR_RANDOM || (rev = rev_dir[dir]) == NO_DIR)
		msg_to_char(ch, "Invalid direction.\r\n");
	else if (!new && !to_room)
		msg_to_char(ch, "Invalid destination.\r\n");
	else if (!new && (!COMPLEX_DATA(to_room) || !IS_ANY_BUILDING(to_room)))
		msg_to_char(ch, "You may only create exits to buildings.\r\n");
	else if (find_exit(IN_ROOM(ch), dir))
		msg_to_char(ch, "An exit already exists in that direction.\r\n");
	else if (!new && find_exit(to_room, rev))
		msg_to_char(ch, "An exit already exists in that direction in the target room.\r\n");
	else {
		if (new) {
			to_room = add_room_to_building(HOME_ROOM(IN_ROOM(ch)), NOTHING);
		}

		create_exit(IN_ROOM(ch), to_room, dir, TRUE);
		msg_to_char(ch, "You create an exit %s to %d.\r\n", dirs[get_direction_for_char(ch, dir)], GET_ROOM_VNUM(to_room));
	}
}


OLC_MODULE(mapedit_delete_exit) {
	struct room_direction_data *ex;
	int dir;

	one_argument(argument, arg);

	if (!*arg)
		msg_to_char(ch, "Delete the exit in which direction?\r\n");
	else if ((dir = parse_direction(ch, arg)) == NO_DIR)
		msg_to_char(ch, "Invalid direction.\r\n");
	else if (!COMPLEX_DATA(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't delete exits here.\r\n");
	}
	else {
		if ((ex = find_exit(IN_ROOM(ch), dir))) {
			if (ex->room_ptr) {
				--GET_EXITS_HERE(ex->room_ptr);
			}
			if (ex->keyword)
				free(ex->keyword);
			LL_DELETE(COMPLEX_DATA(IN_ROOM(ch))->exits, ex);
			free(ex);
		}
		msg_to_char(ch, "Exit deleted. Target room not deleted.\r\n");
	}
}


OLC_MODULE(mapedit_naturalize) {
	bool island = FALSE, world = FALSE;
	int count, island_id = NO_ISLAND;
	struct island_info *isle;
	struct map_data *map;
	crop_data *new_crop;
	room_data *room;
	
	bool do_unclaim = config_get_bool("naturalize_unclaimable");
	
	// parse argument
	skip_spaces(&argument);
	if (*argument && is_abbrev(argument, "island")) {
		island = TRUE;
	}
	else if (*argument && !str_cmp(argument, "world")) {
		world = TRUE;
	}
	else if (*argument) {
		msg_to_char(ch, "Usage: .map naturalize [island | world]\r\n");
		return;
	}
	
	if (!island && !world && (IS_INSIDE(IN_ROOM(ch)) || IS_ADVENTURE_ROOM(IN_ROOM(ch)) || GET_ROOM_VNUM(IN_ROOM(ch)) >= MAP_SIZE)) {
		msg_to_char(ch, "Leave the building or area first.\r\n");
	}
	else if (island && (island_id = GET_ISLAND_ID(IN_ROOM(ch))) == NO_ISLAND) {
		msg_to_char(ch, "You can't do that while not on any island.\r\n");
	}
	else if (island || world) {	// normal processing for whole island/world
		count = 0;
		
		// check all land tiles
		LL_FOREACH(land_map, map) {
			room = map->room;	// may or may not exist
			
			if (island && map->shared->island_id != island_id) {
				continue;
			}
			if (room && ROOM_OWNER(room)) {
				continue;
			}
			if (room && ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE | ROOM_AFF_NO_EVOLVE)) {
				continue;
			}
			if (room && ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE) && !do_unclaim) {
				continue;
			}
			if (map->sector_type == map->natural_sector) {
				// already same -- but refresh crop type if applicable
				if (SECT_FLAGGED(map->sector_type, SECTF_HAS_CROP_DATA)) {
					if (room || (room = real_room(map->vnum))) {
						new_crop = get_potential_crop_for_location(room, NOTHING);
						set_crop_type(room, new_crop ? new_crop : crop_table);
					}
				}
				continue;	// no need to change terrain
			}
			
			// looks good: naturalize it
			if (room) {
				if (ROOM_PEOPLE(room)) {
					act("The area is naturalized!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
				}
				decustomize_room(room);
				change_terrain(room, GET_SECT_VNUM(map->natural_sector), NOTHING);
				if (ROOM_OWNER(room)) {
					deactivate_workforce_room(ROOM_OWNER(room), room);
				}
				
				// no longer need this
				remove_room_extra_data(room, ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR);
			}
			else {
				decustomize_shared_data(map->shared);
				perform_change_sect(NULL, map, map->natural_sector);
				perform_change_base_sect(NULL, map, map->natural_sector);
				
				if (SECT_FLAGGED(map->natural_sector, SECTF_HAS_CROP_DATA)) {
					room = real_room(map->vnum);	// need it loaded after all
					new_crop = get_potential_crop_for_location(room, NOTHING);
					set_crop_type(room, new_crop ? new_crop : crop_table);
				}
				else {
					map->crop_type = NULL;
				}
		
				// no longer need this
				remove_extra_data(&map->shared->extra_data, ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR);
			}
			request_world_save(map->vnum, WSAVE_ROOM | WSAVE_MAP);
			++count;
		}
		
		if (count > 0) {
			if (island) {
				isle = get_island(island_id, TRUE);
				syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has naturalized %d tile%s on island %d %s", GET_NAME(ch), count, PLURAL(count), island_id, isle->name);
			}
			else if (world) {
				syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has naturalized %d tile%s for the whole world!", GET_NAME(ch), count, PLURAL(count));
			}
		}
		msg_to_char(ch, "You have naturalized the sectors for %d tile%s%s.\r\n", count, PLURAL(count), island ? " on this island" : "");
	}
	else {	// normal processing for 1 room
		map = &(world_map[FLAT_X_COORD(IN_ROOM(ch))][FLAT_Y_COORD(IN_ROOM(ch))]);
		decustomize_room(IN_ROOM(ch));
		change_terrain(IN_ROOM(ch), GET_SECT_VNUM(map->natural_sector), NOTHING);
		if (ROOM_OWNER(IN_ROOM(ch))) {
			deactivate_workforce_room(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch));
		}
		
		// no longer need this
		remove_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR);
		
		// reset crop?
		if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_HAS_CROP_DATA)) {
			new_crop = get_potential_crop_for_location(IN_ROOM(ch), NOTHING);
			set_crop_type(IN_ROOM(ch), new_crop ? new_crop : crop_table);
		}
		
		// probably no need to log 1-tile naturalizes
		// syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has naturalized the sector at %s", GET_NAME(ch), room_log_identifier(IN_ROOM(ch)));
		msg_to_char(ch, "You have naturalized the sector for this tile.\r\n");
		act("$n has naturalized the area!", FALSE, ch, NULL, NULL, TO_ROOM);
		
		request_world_save(GET_ROOM_VNUM(IN_ROOM(ch)), WSAVE_MAP | WSAVE_ROOM);
	}
}


OLC_MODULE(mapedit_populate) {
	char_data *current = ROOM_PEOPLE(IN_ROOM(ch));
	vehicle_data *veh;
	
	if (*argument && (veh = get_vehicle_in_room_vis(ch, argument, NULL))) {
		populate_vehicle_npc(veh, NULL, TRUE);
		
		if (current == ROOM_PEOPLE(IN_ROOM(ch))) {
			msg_to_char(ch, "Okay. But there didn't seem to be anything to populate.\r\n");
		}
	}
	else if (*argument && !is_abbrev(argument, "room") && !is_abbrev(argument, "building")) {
		msg_to_char(ch, "You don't see that here.\r\n");
	}
	else if (!GET_BUILDING(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only populate buildings.\r\n");
	}
	else if (!ROOM_OWNER(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only populate owned buildings.\r\n");
	}
	else {
		// should send an "arrives" message if successful
		populate_npc(IN_ROOM(ch), NULL, TRUE);
		
		if (current == ROOM_PEOPLE(IN_ROOM(ch))) {
			msg_to_char(ch, "Okay. But there didn't seem to be anything to populate.\r\n");
		}
	}
}


OLC_MODULE(mapedit_remember) {
	int count, island_id = NO_ISLAND;
	struct island_info *isle;
	struct map_data *map;
	bool island = FALSE;
	
	// parse argument
	skip_spaces(&argument);
	if (*argument && is_abbrev(argument, "island")) {
		island = TRUE;
	}
	else if (*argument) {
		msg_to_char(ch, "Usage: .map remember [island]\r\n");
		return;
	}
	
	if (!island && (IS_INSIDE(IN_ROOM(ch)) || IS_ADVENTURE_ROOM(IN_ROOM(ch)) || GET_ROOM_VNUM(IN_ROOM(ch)) >= MAP_SIZE)) {
		msg_to_char(ch, "Leave the building or area first.\r\n");
	}
	else if (island && (island_id = GET_ISLAND_ID(IN_ROOM(ch))) == NO_ISLAND) {
		msg_to_char(ch, "You can't do that while not on any island.\r\n");
	}
	else if (!island && ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE)) {
		msg_to_char(ch, "You can't make it remember a sector which requires extra data like this one.\r\n");
	}
	else if (island) {	// normal processing for whole island
		count = 0;
		
		// check all land tiles
		LL_FOREACH(land_map, map) {
			if (map->shared->island_id != island_id) {
				continue;
			}
			if (SECT_FLAGGED(map->sector_type, SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE)) {
				continue;
			}
			if (map->natural_sector == map->sector_type) {
				continue;	// already same
			}
			
			// looks good
			set_natural_sector(map, map->sector_type);
			++count;
		}
		
		if (count > 0) {
			isle = get_island(island_id, TRUE);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has set 'remember' for %d tile%s on island %d %s", GET_NAME(ch), count, PLURAL(count), island_id, isle->name);
		}
		msg_to_char(ch, "You have set the map to remember sectors for %d tile%s on this island.\r\n", count, PLURAL(count));
	}
	else {	// normal processing for 1 room
		map = &(world_map[FLAT_X_COORD(IN_ROOM(ch))][FLAT_Y_COORD(IN_ROOM(ch))]);
		set_natural_sector(map, map->sector_type);
		
		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has set 'remember' for %s", GET_NAME(ch), room_log_identifier(IN_ROOM(ch)));
		msg_to_char(ch, "You have set the map to remember the sector for this tile.\r\n");
	}
}


OLC_MODULE(mapedit_roomtype) {
	bld_data *id = NULL;

	if (!IS_INSIDE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to be in one of the interior rooms of a building first.\r\n");
		return;
	}
	if (!*argument) {
		msg_to_char(ch, "What type of room would you like to set (use 'vnum b <name>' to search)?\r\n");
		return;
	}
	
	// parse arg
	if (isdigit(*argument) && !(id = building_proto(atoi(argument)))) {
		msg_to_char(ch, "Invalid building (room) vnum '%s'.\r\n", argument);
		return;
	}
	else if (!id && !(id = get_building_by_name(argument, TRUE))) {
		msg_to_char(ch, "Invalid building (room) type name '%s'.\r\n", argument);
		return;
	}
	
	// final handling
	if (!id || !IS_SET(GET_BLD_FLAGS(id), BLD_ROOM)) {
		msg_to_char(ch, "Invalid room type. You must specify a building with the ROOM flag.\r\n");
	}
	else {
		dismantle_wtrigger(IN_ROOM(ch), ch, FALSE);
		detach_building_from_room(IN_ROOM(ch));
		attach_building_to_room(id, IN_ROOM(ch), TRUE);
		msg_to_char(ch, "This room is now %s %s.\r\n", AN(GET_BLD_NAME(id)), GET_BLD_NAME(id));
		complete_wtrigger(IN_ROOM(ch));
	}
}
