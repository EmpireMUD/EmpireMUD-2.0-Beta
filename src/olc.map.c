/* ************************************************************************
*   File: olc.map.c                                       EmpireMUD 2.0b5 *
*  Usage: OLC for the map and map-building rooms                          *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
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

/**
* Contents:
*   Displays
*   Edit Modules
*/

// external vars
extern bool world_map_needs_save;

// external funcs
void complete_building(room_data *room);
void deactivate_workforce_room(empire_data *emp, room_data *room);
extern crop_data *get_potential_crop_for_location(room_data *location);


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(mapedit_build) {
	void herd_animals_out(room_data *location);
	void special_building_setup(char_data *ch, room_data *room);
	extern const int rev_dir[];
	
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
		special_building_setup(ch, IN_ROOM(ch));
		complete_building(IN_ROOM(ch));
		
		if (dir != NO_DIR) {
			create_exit(IN_ROOM(ch), SHIFT_DIR(IN_ROOM(ch), dir), dir, FALSE);
			if (IS_SET(GET_BLD_FLAGS(bld), BLD_TWO_ENTRANCES)) {
				create_exit(IN_ROOM(ch), SHIFT_DIR(IN_ROOM(ch), rev_dir[dir]), rev_dir[dir], FALSE);
			}
			COMPLEX_DATA(IN_ROOM(ch))->entrance = rev_dir[dir];
			herd_animals_out(IN_ROOM(ch));
		}

		msg_to_char(ch, "You create %s %s!\r\n", AN(GET_BLD_NAME(bld)), GET_BLD_NAME(bld));
		sprintf(buf, "$n creates %s %s!", AN(GET_BLD_NAME(bld)), GET_BLD_NAME(bld));
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
	}
}


OLC_MODULE(mapedit_decay) {
	void annual_update_map_tile(room_data *room);	// db.world.c
	
	room_data *room = HOME_ROOM(IN_ROOM(ch));
	
	if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		msg_to_char(ch, "You can only decay map tiles.\r\n");
	}
	else {
		msg_to_char(ch, "Ok.\r\n");
		annual_update_map_tile(room);
	}
}


OLC_MODULE(mapedit_terrain) {
	extern crop_data *get_crop_by_name(char *name);
	extern sector_data *get_sect_by_name(char *name);
	
	struct empire_city_data *city, *temp;
	empire_data *emp;
	int count;
	sector_data *sect, *next_sect, *old_sect = NULL;
	crop_data *crop, *next_crop;
	crop_data *cp;
	
	sect = get_sect_by_name(argument);
	cp = get_crop_by_name(argument);

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
			REMOVE_FROM_LIST(city, EMPIRE_CITY_LIST(emp), next);
			if (city->name) {
				free(city->name);
			}
			free(city);
			save_empire(emp);
		}
		
		if (sect) {
			change_terrain(IN_ROOM(ch), GET_SECT_VNUM(sect));
			msg_to_char(ch, "This room is now %s %s.\r\n", AN(GET_SECT_NAME(sect)), GET_SECT_NAME(sect));
			if (ROOM_OWNER(IN_ROOM(ch))) {
				deactivate_workforce_room(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch));
			}
		}
		else if (cp) {
			if (!(sect = find_first_matching_sector(SECTF_CROP, NOBITS))) {
				msg_to_char(ch, "No crop sector types are set up.\r\n");
				return;
			}
			else {
				change_terrain(IN_ROOM(ch), GET_SECT_VNUM(sect));
				set_crop_type(IN_ROOM(ch), cp);
				msg_to_char(ch, "This room is now %s.\r\n", GET_CROP_NAME(cp));
				if (ROOM_OWNER(IN_ROOM(ch))) {
					deactivate_workforce_room(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch));
				}
			}
		}
				
		// preserve old original sect for roads -- TODO this is a special-case
		if (IS_ROAD(IN_ROOM(ch))) {
			change_base_sector(IN_ROOM(ch), old_sect);
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
	
	complete_building(IN_ROOM(ch));
	msg_to_char(ch, "Complete.\r\n");
}


OLC_MODULE(mapedit_maintain) {
	void finish_maintenance(char_data *ch, room_data *room);
	
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
			
			if (to) {
				if (set) {
					SET_BIT(ROOM_AFF_FLAGS(to), ROOM_AFF_UNCLAIMABLE);
					SET_BIT(ROOM_BASE_FLAGS(to), ROOM_AFF_UNCLAIMABLE);
					abandon_room(to);
				}
				else {
					REMOVE_BIT(ROOM_AFF_FLAGS(to), ROOM_AFF_UNCLAIMABLE);
					REMOVE_BIT(ROOM_BASE_FLAGS(to), ROOM_AFF_UNCLAIMABLE);
				}
			}
		}
	}
	
	if (radius == 0) {
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
	void decustomize_room(room_data *room);
	
	decustomize_room(IN_ROOM(ch));
	msg_to_char(ch, "All customizations removed on this room/acre.\r\n");
}


OLC_MODULE(mapedit_room_name) {
	if (!*argument)
		msg_to_char(ch, "What would you like to name this room/tile (or \"none\")?\r\n");
	else if (!str_cmp(argument, "none")) {
		if (ROOM_CUSTOM_NAME(IN_ROOM(ch))) {
			free(ROOM_CUSTOM_NAME(IN_ROOM(ch)));
			ROOM_CUSTOM_NAME(IN_ROOM(ch)) = NULL;
		}
		msg_to_char(ch, "This room/tile no longer has a specialized name.\r\n");
	}
	else {
		if (ROOM_CUSTOM_NAME(IN_ROOM(ch))) {
			free(ROOM_CUSTOM_NAME(IN_ROOM(ch)));
		}
		ROOM_CUSTOM_NAME(IN_ROOM(ch)) = str_dup(argument);
		msg_to_char(ch, "This room/tile is now called \"%s\".\r\n", argument);
	}
}


OLC_MODULE(mapedit_icon) {
	extern bool validate_icon(char *icon);

	delete_doubledollar(argument);

	if (IS_INSIDE(IN_ROOM(ch)) || IS_ADVENTURE_ROOM(IN_ROOM(ch)))
		msg_to_char(ch, "You may not do that here.\r\n");
	else if (!str_cmp(argument, "none")) {
		if (ROOM_CUSTOM_ICON(IN_ROOM(ch))) {
			free(ROOM_CUSTOM_ICON(IN_ROOM(ch)));
			ROOM_CUSTOM_ICON(IN_ROOM(ch)) = NULL;
			}
		msg_to_char(ch, "This area no longer has a specialized icon.\r\n");
		}
	else if (!*argument)
		msg_to_char(ch, "What would you like to set the icon to (or \"none\")?\r\n");
	else if (!validate_icon(argument))
		msg_to_char(ch, "Room icons must be exactly four characters.\r\n");
	else if (argument[0] != '&')
		msg_to_char(ch, "Icons must begin with a color code.\r\n");
	else {
		if (ROOM_CUSTOM_ICON(IN_ROOM(ch))) {
			free(ROOM_CUSTOM_ICON(IN_ROOM(ch)));
		}
		ROOM_CUSTOM_ICON(IN_ROOM(ch)) = str_dup(argument);
		msg_to_char(ch, "This area now has the icon \"%s&0\".\r\n", argument);
	}
}


OLC_MODULE(mapedit_delete_room) {
	extern room_data *find_load_room(char_data *ch);

	char_data *c, *next_c;
	room_data *in_room, *home;

	if (str_cmp(argument, "ok"))
		msg_to_char(ch, "You MUST type \".map deleteroom ok\". This will delete the room you're in.\r\n");
	else if (!IS_INSIDE(IN_ROOM(ch)) || GET_ROOM_VNUM(IN_ROOM(ch)) < MAP_SIZE)
		msg_to_char(ch, "You may not delete this room.\r\n");
	else {
		in_room = IN_ROOM(ch);
		home = HOME_ROOM(in_room);
		
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = next_c) {
			next_c = c->next_in_room;
			char_to_room(c, home ? home : find_load_room(c));
			act("$n appears in front of you.", TRUE, c, NULL, NULL, TO_ROOM);
			if (c != ch) {
				msg_to_char(c, "Room deleted.\r\n");
			}
			look_at_room(c);
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
		if (ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))) {
			free(ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch)));
			ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch)) = NULL;
			}
		msg_to_char(ch, "This area no longer has a specialized description.\r\n");
	}
	else if (is_abbrev(argument, "set")) {
		if (ch->desc->str) {
			msg_to_char(ch, "You are already editing a string.\r\n");
		}
		else {
			start_string_editor(ch->desc, "room description", &(ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))), MAX_ROOM_DESCRIPTION, TRUE);
		}
	}
	else
		msg_to_char(ch, "You must specify whether you want to set or remove the description.\r\n");
}


OLC_MODULE(mapedit_ruin) {
	void ruin_one_building(room_data *room);	// db.world.c

	room_data *room = HOME_ROOM(IN_ROOM(ch));
	
	if (GET_ROOM_VNUM(room) >= MAP_SIZE || !GET_BUILDING(room)) {
		msg_to_char(ch, "You can only ruin map buildings.\r\n");
	}
	else {
		msg_to_char(ch, "Ok.\r\n");
		ruin_one_building(room);
	}
}


OLC_MODULE(mapedit_exits) {
	void add_room_to_vehicle(room_data *room, vehicle_data *veh);
	extern room_data *create_room();
	extern const char *dirs[];
	extern room_vnum find_free_vnum();
	extern const int rev_dir[];

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
			to_room = create_room();
			attach_building_to_room(building_proto(config_get_int("default_interior")), to_room, TRUE);
			
			// TODO this is done in several different things that add rooms, and could be moved to a function -paul 7/14/2016
			if (GET_ROOM_VEHICLE(IN_ROOM(ch))) {
				++VEH_INSIDE_ROOMS(GET_ROOM_VEHICLE(IN_ROOM(ch)));
				COMPLEX_DATA(to_room)->vehicle = GET_ROOM_VEHICLE(IN_ROOM(ch));
				add_room_to_vehicle(to_room, GET_ROOM_VEHICLE(IN_ROOM(ch)));
			}
			COMPLEX_DATA(HOME_ROOM(IN_ROOM(ch)))->inside_rooms++;
			
			COMPLEX_DATA(to_room)->home_room = HOME_ROOM(IN_ROOM(ch));
			
			if (ROOM_OWNER(HOME_ROOM(IN_ROOM(ch)))) {
				perform_claim_room(to_room, ROOM_OWNER(HOME_ROOM(IN_ROOM(ch))));
			}
			complete_wtrigger(to_room);
		}

		create_exit(IN_ROOM(ch), to_room, dir, TRUE);
		msg_to_char(ch, "You create an exit %s to %d.\r\n", dirs[get_direction_for_char(ch, dir)], GET_ROOM_VNUM(to_room));
	}
}


OLC_MODULE(mapedit_delete_exit) {
	struct room_direction_data *ex, *temp;
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
			REMOVE_FROM_LIST(ex, COMPLEX_DATA(IN_ROOM(ch))->exits, next);
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
			room = real_real_room(map->vnum);	// may or may not exist
			
			if (island && map->island != island_id) {
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
				continue;	// already same
			}
			
			// looks good: naturalize it
			if (room) {
				change_terrain(room, GET_SECT_VNUM(map->natural_sector));
				if (ROOM_PEOPLE(room)) {
					act("The area is naturalized!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
				}
				if (ROOM_OWNER(room)) {
					deactivate_workforce_room(ROOM_OWNER(room), room);
				}
			}
			else {
				perform_change_sect(NULL, map, map->natural_sector);
				perform_change_base_sect(NULL, map, map->natural_sector);
				
				if (SECT_FLAGGED(map->natural_sector, SECTF_HAS_CROP_DATA)) {
					room = real_room(map->vnum);	// need it loaded after all
					set_crop_type(room, get_potential_crop_for_location(room));
				}
				else {
					map->crop_type = NULL;
				}
			}
			++count;
		}
		
		if (count > 0) {
			world_map_needs_save = TRUE;
			
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
		change_terrain(IN_ROOM(ch), GET_SECT_VNUM(map->natural_sector));
		if (ROOM_OWNER(IN_ROOM(ch))) {
			deactivate_workforce_room(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch));
		}
		
		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has set naturalized the sector at %s", GET_NAME(ch), room_log_identifier(IN_ROOM(ch)));
		msg_to_char(ch, "You have naturalized the sector for this tile.\r\n");
		act("$n has naturalized the area!", FALSE, ch, NULL, NULL, TO_ROOM);
		world_map_needs_save = TRUE;
	}
}


OLC_MODULE(mapedit_populate) {
	void populate_npc(room_data *room, struct empire_territory_data *ter);
	char_data *current = ROOM_PEOPLE(IN_ROOM(ch));
	
	if (!GET_BUILDING(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only populate buildings.\r\n");
	}
	else if (!ROOM_OWNER(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only populate owned buildings.\r\n");
	}
	else {
		// should send an "arrives" message if successful
		populate_npc(IN_ROOM(ch), NULL);
		
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
			if (map->island != island_id) {
				continue;
			}
			if (SECT_FLAGGED(map->sector_type, SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE)) {
				continue;
			}
			if (map->natural_sector == map->sector_type) {
				continue;	// already same
			}
			
			// looks good
			map->natural_sector = map->sector_type;
			++count;
		}
		
		if (count > 0) {
			isle = get_island(island_id, TRUE);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has set 'remember' for %d tile%s on island %d %s", GET_NAME(ch), count, PLURAL(count), island_id, isle->name);
			world_map_needs_save = TRUE;
		}
		msg_to_char(ch, "You have set the map to remember sectors for %d tile%s on this island.\r\n", count, PLURAL(count));
	}
	else {	// normal processing for 1 room
		map = &(world_map[FLAT_X_COORD(IN_ROOM(ch))][FLAT_Y_COORD(IN_ROOM(ch))]);
		map->natural_sector = map->sector_type;
		
		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has set 'remember' for %s", GET_NAME(ch), room_log_identifier(IN_ROOM(ch)));
		msg_to_char(ch, "You have set the map to remember the sector for this tile.\r\n");
		world_map_needs_save = TRUE;
	}
}


OLC_MODULE(mapedit_roomtype) {
	extern bld_data *get_building_by_name(char *name, bool room_only);
	
	bld_data *id;

	if (!IS_INSIDE(IN_ROOM(ch)))
		msg_to_char(ch, "You need to be in one of the interior rooms of a building first.\r\n");
	else if (!*argument || !(id = get_building_by_name(argument, TRUE))) {
		msg_to_char(ch, "What type of room would you like to set (use 'vnum b <name>' to search)?\r\n");
	}
	else {
		dismantle_wtrigger(IN_ROOM(ch), ch, FALSE);
		detach_building_from_room(IN_ROOM(ch));
		attach_building_to_room(id, IN_ROOM(ch), TRUE);
		msg_to_char(ch, "This room is now %s %s.\r\n", AN(GET_BLD_NAME(id)), GET_BLD_NAME(id));
		complete_wtrigger(IN_ROOM(ch));
	}
}
