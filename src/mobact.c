/* ************************************************************************
*   File: mobact.c                                        EmpireMUD 2.0b5 *
*  Usage: Functions for generating intelligent (?) behavior in mobiles    *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <math.h>

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "skills.h"
#include "dg_scripts.h"
#include "vnums.h"
#include "constants.h"

/**
* Contents:
*   Helpers
*   Generic NPCs
*   Mob Movement
*   Mob Activity
*   Mob Spawning
*   Mob Scaling
*/

// external vars
extern bool catch_up_mobs;
extern bool caught_up_mobs;

// external funcs
ACMD(do_exit);
ACMD(do_say);


// for validate_global_map_spawns, run_global_map_spawns
struct glb_map_spawn_bean {
	room_data *room;
	int x_coord, y_coord;
	bool in_city;
};


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Creates a new mob pursuit entry -- this will delete any old entry for the
* same target.
*
* @param char_data *ch The mob pursuing
* @param char_data *target The victim
*/
void add_pursuit(char_data *ch, char_data *target) {
	struct pursuit_data *purs;
	
	// only mobs pursue, and only pursue non-mobs
	if (!IS_NPC(ch) || IS_NPC(target) || IS_IMMORTAL(target)) {
		return;
	}
	
	end_pursuit(ch, target);
	
	// add new instance
	CREATE(purs, struct pursuit_data, 1);
	LL_PREPEND(MOB_PURSUIT(ch), purs);
	
	purs->idnum = GET_IDNUM(target);
	purs->last_seen = time(0);
	purs->location = GET_ROOM_VNUM(HOME_ROOM(IN_ROOM(ch)));
}


/**
* Cancels pursuit of a target.
*
* @param char_data *ch The mob pursuing
* @param char_data *target The victim
*/
void end_pursuit(char_data *ch, char_data *target) {
	struct pursuit_data *purs, *next_purs;
	
	// only mobs have this
	if (!IS_NPC(ch)) {
		return;
	}
	
	// remove any old instance
	for (purs = MOB_PURSUIT(ch); purs; purs = next_purs) {
		next_purs = purs->next;
		
		if (purs->idnum == GET_IDNUM(target)) {
			LL_DELETE(MOB_PURSUIT(ch), purs);
			free(purs);
		}
	}
}


/**
* Sends a mob back to where it was if it's not fighting and is standing.
*
* @param char_data *ch The mob to send back.
* @return bool TRUE if the mob moves back, FALSE otherwise.
*/
bool return_to_pursuit_location(char_data *ch) {
	struct pursuit_data *purs, *next_purs;
	room_data *loc;
	
	if (!ch || ch->desc || !IS_NPC(ch) || FIGHTING(ch) || GET_POS(ch) < POS_STANDING || AFF_FLAGGED(ch, AFF_ENTANGLED) || MOB_PURSUIT_LEASH_LOC(ch) == NOWHERE) {
		return FALSE;
	}
	
	loc = real_room(MOB_PURSUIT_LEASH_LOC(ch));
	
	if (!loc) {
		MOB_PURSUIT_LEASH_LOC(ch) = NOWHERE;
		return FALSE;
	}
	
	act("$n goes back to where $e was.", TRUE, ch, NULL, NULL, TO_ROOM);
	char_to_room(ch, loc);
	act("$n returns to what $e was doing.", TRUE, ch, NULL, NULL, TO_ROOM);
	MOB_PURSUIT_LEASH_LOC(ch) = NOWHERE;
	
	// reset pursue data to avoid loops
	LL_FOREACH_SAFE(MOB_PURSUIT(ch), purs, next_purs) {
		free(purs);
	}
	MOB_PURSUIT(ch) = NULL;
	
	return TRUE;
}


/**
* This generates a random amount of money the NPC will carry. If it's an
* empire's NPC, the money will come from the empire's vault.
*
* @param char_data *mob An NPC.
* @return int A random amount of money that mob carries.
*/
int mob_coins(char_data *mob) {
	empire_data *emp;
	int amt = 0;
	
	if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_HUMAN) && !MOB_FLAGGED(mob, MOB_PICKPOCKETED)) {
		amt = number(-20, get_approximate_level(mob));
		if (MOB_FLAGGED(mob, MOB_HARD)) {
			amt *= 3;
		}
		if (MOB_FLAGGED(mob, MOB_GROUP)) {
			amt *= 5;
		}
		amt = MAX(0, amt);
		
		if ((emp = GET_LOYALTY(mob))) {
			amt = MIN(amt, EMPIRE_COINS(emp));
			increase_empire_coins(emp, emp, -amt);
		}
	}
	
	return amt;
}


INTERACTION_FUNC(run_one_encounter) {
	char_data *aggr;
	int iter;
	bool any = FALSE;
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		aggr = read_mobile(interaction->vnum, TRUE);
		setup_generic_npc(aggr, NULL, NOTHING, NOTHING);
		if (COMPLEX_DATA(IN_ROOM(ch)) && COMPLEX_DATA(IN_ROOM(ch))->instance) {
			MOB_INSTANCE_ID(aggr) = INST_ID(COMPLEX_DATA(IN_ROOM(ch))->instance);
			if (MOB_INSTANCE_ID(aggr) != NOTHING) {
				add_instance_mob(real_instance(MOB_INSTANCE_ID(aggr)), GET_MOB_VNUM(aggr));
			}
		}
		char_to_room(aggr, IN_ROOM(ch));
		set_mob_flags(aggr, MOB_SPAWNED);
		act("$N appears!", FALSE, ch, 0, aggr, TO_CHAR | TO_ROOM);
		hit(aggr, ch, GET_EQ(aggr, WEAR_WIELD), TRUE);
		load_mtrigger(aggr);
		any = TRUE;
	}
	
	return any;
}


/**
* Offers a player a chance at a random encounter using INTERACT_ENCOUNTER on
* the room they are in.
*
* @param char_data *ch The unsuspecting fool.
*/
void random_encounter(char_data *ch) {
	if (!ch->desc || IS_NPC(ch) || !IN_ROOM(ch) || FIGHTING(ch) || IS_GOD(ch) || GET_INVIS_LEV(ch) > LVL_MORTAL || PRF_FLAGGED(ch, PRF_WIZHIDE) || NOHASSLE(ch) || ISLAND_FLAGGED(IN_ROOM(ch), ISLE_NO_AGGRO)) {
		return;
	}

	if (AFF_FLAGGED(ch, AFF_FLY | AFF_MAJESTY)) {
		return;
	}
	
	// water encounters don't trigger if the player is on a vehicle
	if ((ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_SHALLOW_WATER) || WATER_SECT(IN_ROOM(ch)) || ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_NEED_BOAT) || RMT_FLAGGED(IN_ROOM(ch), RMT_NEED_BOAT)) && (GET_SITTING_ON(ch) || EFFECTIVELY_FLYING(ch))) {
		return;
	}

	run_room_interactions(ch, IN_ROOM(ch), INTERACT_ENCOUNTER, NULL, NOTHING, run_one_encounter);
}


 //////////////////////////////////////////////////////////////////////////////
//// GENERIC NPCS ////////////////////////////////////////////////////////////


/**
* Finds the best name list available, given the choices. It falls back on
* other name lists as defaults.
*
* @param int name_set Any NAMES_x const.
* @param int sex Any SEX_x const.
* @return struct generic_name_data* The namelist object.
*/
struct generic_name_data *get_best_name_list(int name_set, int sex) {
	struct generic_name_data *iter, *set = get_generic_name_list(name_set, sex);
	
	// backup if no list found
	if (!set) {
		for (iter = generic_names; iter; iter = iter->next) {
			if (iter->sex == sex) {
				set = iter;
				break;
			}
		}
		
		// still no?
		if (!set) {
			set = generic_names;
		}
	}
	
	return set;
}


/**
* Fetch a generic name list by id/sex.
*
* @param int name_set Any NAMES_x const.
* @param int sex Any SEX_x const.
* @return struct generic_name_data* The generic name list, or NULL if no match.
*/
struct generic_name_data *get_generic_name_list(int name_set, int sex) {
	struct generic_name_data *iter;
	
	for (iter = generic_names; iter; iter = iter->next) {
		if (iter->name_set == name_set && iter->sex == sex) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* @param int name_set Any NAMES_x const.
* @param int sex Any SEX_x const.
* @return int a position in the name set for a random name of that sex
*/
int pick_generic_name(int name_set, int sex) {
	struct generic_name_data *set = get_best_name_list(name_set, sex);

	// grab max value from the set	
	return number(0, set->size-1);
}


/**
* This function substitutes #a, #n, and #e in a string.
*
* @param const char *str The base string
* @param const char *name Replaces #n
* @param const char *empire_name Replaces #e
* @param const char *empire_adjective Replaces #a
* @return char* The replaced string
*/
char *replace_npc_names(const char *str, const char *name, const char *empire_name, const char *empire_adjective) {
	static char buf[MAX_STRING_LENGTH];
	char *cp = buf, *ptr, *iter;

	for (iter = (char*)str; *iter; ++iter) {
		if (*iter == '#') {
			switch (*(++iter)) {
				case 'n':
					for (ptr = (char*)name; *ptr && (cp - buf) < (MAX_STRING_LENGTH - 1); *(cp++) = *(ptr++));
					break;
				case 'e':
					for (ptr = (char*)empire_name; *ptr && (cp - buf) < (MAX_STRING_LENGTH - 1); *(cp++) = *(ptr++));
					break;
				case 'a':
					for (ptr = (char*)empire_adjective; *ptr && (cp - buf) < (MAX_STRING_LENGTH - 1); *(cp++) = *(ptr++));
					break;
				default:
					if ((cp - buf) < (MAX_STRING_LENGTH - 1)) {
						*(cp++) = '#';
					}
					break;
			}
		}
		else if ((cp - buf) < (MAX_STRING_LENGTH - 1)) {
			*(cp++) = *iter;
		}
	}
	
	if ((cp - buf) < (MAX_STRING_LENGTH - 1)) {
		*cp = '\0';
	}
	else {
		buf[MAX_STRING_LENGTH-1] = '\0';
	}

	return (buf);
}


/**
* Re-strings a mob by replacing #n and #e with name and empire.
*
* @param char_data *mob
* @param empire_data *emp the empire (optional; NOTHING = none)
* @param int name Entry position in the name list -- NOTHING for auto-pick
* @param int sex Which sex it should be -- NOTHING for auto-pick
*/
void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex) {
	char *free_name = NULL, *free_short = NULL, *free_long = NULL, *free_look = NULL;
	struct generic_name_data *name_set;
	char_data *proto;
	
	// will work with the proto to make them re-stringable
	proto = mob_proto(GET_MOB_VNUM(mob));
	
	// short-circuit
	if (!strchr(GET_PC_NAME(proto ? proto : mob), '#') && !strchr(GET_SHORT_DESC(proto ? proto : mob), '#') && !strchr(GET_LONG_DESC(proto ? proto : mob), '#')) {
		// no # codes: no work to do
		return;
	}
	
	// safety
	if (sex == NOTHING) {
		// try the mob's natural sex first
		sex = GET_REAL_SEX(mob);
	}
	// pick a male/female sex if none given
	if (sex != SEX_MALE && sex != SEX_FEMALE) {
		sex = number(SEX_MALE, SEX_FEMALE);
	}
	
	// ensure empire
	if (!emp) {
		emp = GET_LOYALTY(mob);
	}
	
	// get name set
	name_set = get_best_name_list(MOB_NAME_SET(mob), sex);
		
	// pick a name if none given, or if invalid
	if (name == NOTHING || name >= name_set->size) {
		name = pick_generic_name(MOB_NAME_SET(mob), sex);
	}

	GET_LOYALTY(mob) = emp;
	
	// set up sex -- some mobs go both ways ;)
	GET_REAL_SEX(mob) = sex;
	MOB_DYNAMIC_SEX(mob) = sex;
	MOB_DYNAMIC_NAME(mob) = name;
	
	// mark strings for freeing later (this patches a small memory leak on mobs whose strings weren't prototypical)
	if (GET_PC_NAME(mob) && (!proto || GET_PC_NAME(mob) != GET_PC_NAME(proto))) {
		free_name = GET_PC_NAME(mob);
	}
	if (GET_SHORT_DESC(mob) && (!proto || GET_SHORT_DESC(mob) != GET_SHORT_DESC(proto))) {
		free_short = GET_SHORT_DESC(mob);
	}
	if (GET_LONG_DESC(mob) && (!proto || GET_LONG_DESC(mob) != GET_LONG_DESC(proto))) {
		free_long = GET_LONG_DESC(mob);
	}
	if (GET_LOOK_DESC(mob) && (!proto || GET_LOOK_DESC(mob) != GET_LOOK_DESC(proto))) {
		free_look = GET_LOOK_DESC(mob);
	}
	
	// restrings: uses "afar"/"lost" if there is no empire
	if (strchr(GET_PC_NAME(proto ? proto : mob), '#')) {
		GET_PC_NAME(mob) = str_dup(replace_npc_names(GET_PC_NAME(proto ? proto : mob), name_set->names[name], !emp ? "afar" : EMPIRE_NAME(emp), !emp ? "lost" : EMPIRE_ADJECTIVE(emp)));
		if (free_name) {
			free(free_name);
		}
	}
	if (strchr(GET_SHORT_DESC(proto ? proto : mob), '#')) {
		GET_SHORT_DESC(mob) = str_dup(replace_npc_names(GET_SHORT_DESC(proto ? proto : mob), name_set->names[name], !emp ? "afar" : EMPIRE_NAME(emp), !emp ? "lost" : EMPIRE_ADJECTIVE(emp)));
		if (free_short) {
			free(free_short);
		}
	}
	if (strchr(GET_LONG_DESC(proto ? proto : mob), '#')) {
		GET_LONG_DESC(mob) = str_dup(replace_npc_names(GET_LONG_DESC(proto ? proto : mob), name_set->names[name], !emp ? "afar" : EMPIRE_NAME(emp), !emp ? "lost" : EMPIRE_ADJECTIVE(emp)));
		if (free_long) {
			free(free_long);
		}
	}
	if (GET_LOOK_DESC(mob) && strchr(GET_LOOK_DESC(proto ? proto : mob), '#')) {
		GET_LOOK_DESC(mob) = str_dup(replace_npc_names(GET_LOOK_DESC(proto ? proto : mob), name_set->names[name], !emp ? "afar" : EMPIRE_NAME(emp), !emp ? "lost" : EMPIRE_ADJECTIVE(emp)));
		if (free_look) {
			free(free_look);
		}
	}
	
	request_mob_save_in_room(mob);
}


 //////////////////////////////////////////////////////////////////////////////
//// MOB MOVEMENT ////////////////////////////////////////////////////////////

/**
* Runs a mob's pursuit. 
*
* @param char_data *ch The mob who's pursuing someone.
* @return bool TRUE if we took action/moved, FALSE if not
*/
bool check_mob_pursuit(char_data *ch) {
	struct pursuit_data *purs, *next_purs;
	struct track_data *track, *next_track;
	room_vnum track_to_room = NOWHERE;
	char_data *vict;
	int dir = NO_DIR;
	bool found = FALSE, moved = FALSE;
	
	// break out early if not pursuing
	if (!MOB_PURSUIT(ch)) {
		return FALSE;
	}
	
	// store location to return to, if none is already set
	if (MOB_PURSUIT_LEASH_LOC(ch) == NOWHERE) {
		MOB_PURSUIT_LEASH_LOC(ch) = GET_ROOM_VNUM(IN_ROOM(ch));
	}
	
	LL_FOREACH_SAFE(MOB_PURSUIT(ch), purs, next_purs) {
		// check pursuit timeout and distance
		if (time(0) - purs->last_seen > config_get_int("mob_pursuit_timeout") * SECS_PER_REAL_MIN || compute_distance(IN_ROOM(ch), real_room(purs->location)) > config_get_int("mob_pursuit_distance")) {
			LL_DELETE(MOB_PURSUIT(ch), purs);
			free(purs);
			continue;
		}
		
		// now see if we can track from here
		found = FALSE;

		// look in this room
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
			if (!IS_NPC(vict) && GET_IDNUM(vict) == purs->idnum && CAN_SEE(ch, vict) && CAN_RECOGNIZE(ch, vict) && can_fight(ch, vict)) {
				found = TRUE;
				engage_combat(ch, vict, FALSE);
				
				// exit early: we're now in combat
				return TRUE;
			}
		}
		
		// track to next room
		HASH_ITER(hh, ROOM_TRACKS(IN_ROOM(ch)), track, next_track) {
			// don't bother checking track lifespan here -- just let mobs follow it till it gets removed
			if ((-1 * track->id) == purs->idnum) {
				found = TRUE;
				dir = track->dir;
				track_to_room = track->to_room;
				break;
			}
		}
		
		// try line-of-sight tracking
		if (!found && IS_OUTDOORS(ch)) {
			// first see if they're playing
			if (!(vict = is_playing(purs->idnum))) {
				LL_DELETE(MOB_PURSUIT(ch), purs);
				free(purs);
				continue;
			}
			
			// check distance: TODO this is magic-numbered to 7, similar to how far you can see players in mapview.c
			if (IS_OUTDOORS(vict) && compute_distance(IN_ROOM(ch), IN_ROOM(vict)) <= 7) {
				dir = get_direction_to(IN_ROOM(ch), IN_ROOM(vict));
				if (dir != NO_DIR) {
					// found one
					found = TRUE;
				}
			}
		}
		
		// break out if we found anything to do
		if (found) {
			break;
		}
	}	// end iteration
	
	if (found && (dir != NO_DIR || track_to_room != NOWHERE) && !AFF_FLAGGED(ch, AFF_CHARM | AFF_ENTANGLED)) {
		if (track_to_room && find_portal_in_room_targetting(IN_ROOM(ch), track_to_room)) {
			perform_move(ch, NO_DIR, real_room(track_to_room), MOVE_ENTER_PORTAL | MOVE_WANDER);
			moved = TRUE;
		}
		else if (track_to_room != NOWHERE && find_vehicle_in_room_with_interior(IN_ROOM(ch), track_to_room)) {
			perform_move(ch, NO_DIR, real_room(track_to_room), MOVE_ENTER_VEH | MOVE_WANDER);
			moved = TRUE;
		}
		else if (GET_ROOM_VEHICLE(IN_ROOM(ch)) && IN_ROOM(GET_ROOM_VEHICLE(IN_ROOM(ch))) && GET_ROOM_VNUM(IN_ROOM(GET_ROOM_VEHICLE(IN_ROOM(ch)))) == track_to_room) {
			perform_move(ch, NO_DIR, real_room(track_to_room), MOVE_EXIT | MOVE_WANDER);
			moved = TRUE;
		}
		else if (dir != NO_DIR) {
			perform_move(ch, dir, NULL, MOVE_WANDER);
			moved = TRUE;
		}
	}
	
	// look for target again
	if (moved) {
		found = FALSE;

		for (purs = MOB_PURSUIT(ch); !found && purs; purs = next_purs) {
			next_purs = purs->next;
			
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
				if (!IS_NPC(vict) && GET_IDNUM(vict) == purs->idnum && can_fight(ch, vict)) {
					found = TRUE;
					engage_combat(ch, vict, FALSE);
					
					// exit now: we are in combat
					return TRUE;
				}
			}
		}
	}
	
	// try to go back if unable to move from pursuit
	if (!moved) {
		moved = return_to_pursuit_location(ch);
	}
	
	return moved;
}


/**
* This checks if a mob is willing to move onto a given sect.
*
* @param char_data *mob The mob.
* @param room_data *to_room The room to check movement into.
* @return bool TRUE if the mob will move there, FALSE if not
*/
bool mob_can_move_to_sect(char_data *mob, room_data *to_room) {
	sector_data *sect = SECT(to_room);
	int move_type = MOB_MOVE_TYPE(mob);
	bool ok = FALSE;
	
	// sect- and ability-based determinations
	if (ROOM_PRIVATE_OWNER(to_room) != NOBODY) {
		ok = FALSE;
	}
	else if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_REPEL_NPCS)) {
		ok = FALSE;
	}
	else if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_REPEL_ANIMALS) && MOB_FLAGGED(mob, MOB_ANIMAL)) {
		ok = FALSE;
	}
	else if (ROOM_BLD_FLAGGED(to_room, BLD_NO_NPC) && IS_COMPLETE(to_room)) {
		// nope
		ok = FALSE;
	}
	else if (SECT_FLAGGED(sect, SECTF_IS_ROAD) && !MOB_FLAGGED(mob, MOB_AQUATIC) && move_type != MOB_MOVE_SWIM) {
		ok = TRUE;
	}
	else if (AFF_FLAGGED(mob, AFF_FLY)) {
		ok = TRUE;
	}
	else if (SECT_FLAGGED(sect, SECTF_ROUGH)) {
		if (move_type == MOB_MOVE_CLIMB || MOB_FLAGGED(mob, MOB_MOUNTAINWALK)) {
			ok = TRUE;
		}
	}
	else if (SECT_FLAGGED(sect, SECTF_FRESH_WATER | SECTF_OCEAN)) {
		if (MOB_FLAGGED(mob, MOB_AQUATIC) || move_type == MOB_MOVE_SWIM || move_type == MOB_MOVE_PADDLE) {
			ok = TRUE;
		}
	}
	else {
		// not mountain/ocean/river/road ... just check for move types that indicate non-flat-ground
		if (move_type != MOB_MOVE_SWIM && move_type != MOB_MOVE_PADDLE) {
			ok = TRUE;
		}
	}
	
	// overrides: things that cancel previous oks
	
	// non-humans won't enter buildings (open buildings only count if finished)
	if (!MOB_FLAGGED(mob, MOB_HUMAN) && SECT_FLAGGED(sect, SECTF_MAP_BUILDING | SECTF_INSIDE) && (!ROOM_BLD_FLAGGED(to_room, BLD_OPEN) || IS_COMPLETE(to_room))) {
		ok = FALSE;
	}
	
	// done
	return ok;
}


/**
* This makes the final determination as to whether a mob can enter a specific
* building or room.
*
* @param char_data *ch The mobile
* @param int dir Which direction (NORTH, etc); may be NO_DIR
* @param room_data *to_room The destination
* @return TRUE if the move is valid, FALSE otherwise
*/
bool validate_mobile_move(char_data *ch, int dir, room_data *to_room) {
	empire_data *ch_emp = GET_LOYALTY(ch);
	empire_data *room_emp = ROOM_OWNER(to_room);
	bool valid = TRUE;
	
	// !mob room
	if (valid && (RMT_FLAGGED(to_room, RMT_NO_MOB) || (IS_COMPLETE(to_room) && ROOM_BLD_FLAGGED(to_room, BLD_NO_NPC)))) {
		valid = FALSE;
	}
	
	if (valid && ROOM_BLD_FLAGGED(to_room, BLD_BARRIER) && IS_COMPLETE(to_room)) {
		valid = FALSE;
	}
	
	// check building permissions (hostile empire locations only)
	if (valid && !IS_OUTDOOR_TILE(to_room) && room_emp && room_emp != ch_emp && (MOB_FLAGGED(ch, MOB_AGGRESSIVE) || empire_is_hostile(room_emp, ch_emp, to_room))) {
		if (MOB_FLAGGED(ch, MOB_AGGRESSIVE | MOB_CITYGUARD)) {
			// only locks blocks aggressive/cityguard
			if (EMPIRE_HAS_TECH(room_emp, TECH_LOCKS)) {
				valid = FALSE;
			}
		}
		else {
			// not aggressive: blocked by non-matching empire
			valid = FALSE;
		}
	}
	
	return valid;
}


/**
* Attempts to move the mobile more or less at random, as part of normal movement.
*
* @param char_data *ch The mobile
* @return bool TRUE if the mobile moved successfully
*/
bool try_mobile_movement(char_data *ch) {
	int dir, count;
	room_data *to_room, *temp_room, *was_in = IN_ROOM(ch);
	struct room_direction_data *ex;
	vehicle_data *veh;
	
	// animals don't move indoors
	if (MOB_FLAGGED(ch, MOB_ANIMAL) && ROOM_IS_CLOSED(IN_ROOM(ch)) && !IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
		return FALSE;
	}
	
	// 40% random chance to attempt a move indoors or 20% outdoors
	if (number(1, 100) > 20 * (COMPLEX_DATA(IN_ROOM(ch)) ? 2 : 1)) {
		return FALSE;
	}
	
	// pick a random direction
	dir = number(-1, NUM_2D_DIRS-1);
	
	// -1 will attempt to enter/exit a vehicle instead
	if (dir == -1 && ROOM_CAN_EXIT(IN_ROOM(ch)) && (!GET_ROOM_VEHICLE(IN_ROOM(ch)) || VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(ch)), VEH_BUILDING))) {
		do_exit(ch, "", 0, 0);
	}
	else if (dir == -1) {	// look for a vehicle to enter
		to_room = NULL;
		count = 0;
		DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
			if (!VEH_IS_COMPLETE(veh)) {
				continue; // incomplete
			}
			if (!VEH_FLAGGED(veh, VEH_BUILDING)) {
				continue;	// buildings only (not vehicles)
			}
			if (!(temp_room = get_vehicle_interior(veh))) {
				continue; // no interior
			}
			if (!mob_can_move_to_sect(ch, temp_room) || !validate_mobile_move(ch, NO_DIR, temp_room)) {
				continue;	// won't go there
			}
			
			// seems ok: pick one at random
			if (!number(0, count++)) {
				to_room = temp_room;
			}
		}
		
		// did we find one?
		if (to_room) {
			perform_move(ch, NO_DIR, to_room, MOVE_ENTER_VEH);
		}
	}	// end attempt enter/exit
	else if (IS_OUTDOORS(ch) && !IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
		// map movement:
		to_room = real_shift(IN_ROOM(ch), shift_dir[dir][0], shift_dir[dir][1]);
		
		if (to_room && mob_can_move_to_sect(ch, to_room)) {
			// check building and entrances
			if ((!ROOM_BLD_FLAGGED(to_room, BLD_OPEN) && !IS_COMPLETE(to_room)) || (!IS_ADVENTURE_ROOM(IN_ROOM(ch)) && !IS_INSIDE(IN_ROOM(ch)) && ROOM_IS_CLOSED(to_room) && BUILDING_ENTRANCE(to_room) != dir && (!ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES) || BUILDING_ENTRANCE(to_room) != rev_dir[dir]))) {
				// can't go that way
			}
			else if (validate_mobile_move(ch, dir, to_room)) {
				perform_move(ch, dir, to_room, MOVE_WANDER);
			}
		}
	}
	else if (COMPLEX_DATA(IN_ROOM(ch)) && (ex = find_exit(IN_ROOM(ch), dir)) && CAN_GO(ch, ex)) {
		// indoor movement
		to_room = ex->room_ptr;
		
		if (to_room && mob_can_move_to_sect(ch, to_room) && validate_mobile_move(ch, dir, to_room)) {
			perform_move(ch, dir, to_room, MOVE_WANDER);
		}
	}

	// TRUE if moved
	return (was_in != IN_ROOM(ch));
}


 //////////////////////////////////////////////////////////////////////////////
//// MOB ACTIVITY ////////////////////////////////////////////////////////////

/**
* Main cycle of mob activity: runs on 1 mob at a time as of b5.115. Previously,
* this was mobile_activity() and iterated over the character list. Now it runs
* inside of real_update().
*
* @param char_data *ch The mob.
*/
void run_mobile_activity(char_data *ch) {
	register char_data *ally, *vict, *targ;
	bool acted = FALSE, moved = FALSE;
	obj_data *obj;

	#define CAN_AGGRO(mob, vict)  (!IS_DEAD(vict) && !NOHASSLE(vict) && (IS_NPC(vict) || !PRF_FLAGGED(vict, PRF_WIZHIDE)) && !IS_GOD(vict) && vict != mob->master && !AFF_FLAGGED(vict, AFF_IMMUNE_PHYSICAL | AFF_NO_TARGET_IN_ROOM | AFF_NO_SEE_IN_ROOM | AFF_NO_ATTACK) && CAN_SEE(mob, vict) && can_fight((mob), (vict)))
	
	// prevent running multiple mob moves during a catch-up cycle
	if (!catch_up_mobs) {
		return;
	}
	caught_up_mobs = TRUE;	// indicate we have run at least 1 mob
	
	// things that prevent activity:
	if (!IS_MOB(ch) || GET_FED_ON_BY(ch) || EXTRACTED(ch) || IS_DEAD(ch) || AFF_FLAGGED(ch, AFF_STUNNED | AFF_HARD_STUNNED)) {
		return;	// not a mob or is disabled
	}
	if (FIGHTING(ch) || !AWAKE(ch) || AFF_FLAGGED(ch, AFF_CHARM) || MOB_FLAGGED(ch, MOB_TIED) || IS_INJURED(ch, INJ_TIED) || GET_LED_BY(ch)) {
		return;	// already busy
	}
	
	// 1. pursuit
	if (!moved && MOB_FLAGGED(ch, MOB_PURSUE) && MOB_PURSUIT(ch) && !moved) {
		moved = check_mob_pursuit(ch);
	}
	
	// 2. try a basic move
	if (!moved && !MOB_FLAGGED(ch, MOB_SENTINEL | MOB_TIED) && !AFF_FLAGGED(ch, AFF_CHARM | AFF_ENTANGLED) && GET_POS(ch) == POS_STANDING && (!ch->master || IN_ROOM(ch) != IN_ROOM(ch->master)) && (!MOB_FLAGGED(ch, MOB_PURSUE) || !MOB_PURSUIT(ch))) {
		moved = try_mobile_movement(ch);
	}

	// 3. Aggressive Mobs (even if we moved)
	if (!acted && MOB_FLAGGED(ch, MOB_AGGRESSIVE) && (IS_ADVENTURE_ROOM(IN_ROOM(ch)) || !ISLAND_FLAGGED(IN_ROOM(ch), ISLE_NO_AGGRO))) {
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
			if (vict == ch) {
				continue;	// ignore: self
			}
			if (!IS_NPC(vict) && !vict->desc) {
				continue;	// ignore: linkdead player
			}
			if (IS_NPC(vict) && (IS_ADVENTURE_ROOM(IN_ROOM(vict)) || MOB_FLAGGED(vict, MOB_AGGRESSIVE) || !MOB_FLAGGED(vict, MOB_HUMAN))) {
				continue;	// they will attack humans but not aggro-humans, and not inadventures
			}
			if (MOB_FACTION(ch) && has_reputation(vict, FCT_VNUM(MOB_FACTION(ch)), REP_LIKED)) {
				continue;	// ignore: don't aggro people we like
			}
			if (!CAN_AGGRO(ch, vict)) {
				continue;	// ignore: invalid target
			}
			
			// ok good to go
			hit(ch, vict, GET_EQ(ch, WEAR_WIELD), TRUE);
			acted = TRUE;
			break;
		}
	}	// end aggro
	
	// 4. Cityguards
	if (!acted && MOB_FLAGGED(ch, MOB_CITYGUARD) && GET_LOYALTY(ch)) {
		// 4.1: look for people to assist
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), ally, next_in_room) {
			if (ally == ch || GET_LOYALTY(ally) != GET_LOYALTY(ch)) {
				continue;	// ignore: is self or is wrong empire
			}
			if (!(targ = FIGHTING(ally)) || targ == ch || targ == ch->master) {
				continue;	// ignore: fighting us!
			}
			if (!IS_NPC(targ) && GET_LOYALTY(targ) == GET_LOYALTY(ch)) {
				continue;	// ignore: player in own empire
			}
			if (!IS_NPC(targ) && !IS_NPC(ally) && IS_PVP_FLAGGED(ally)) {
				continue;	// ignore: pvp fight
			}
			if (!CAN_AGGRO(ch, targ)) {
				continue;	// invalid target
			}
					
			// assist ally
			engage_combat(ch, targ, FALSE);
			acted = TRUE;
			break;
		}	// end cityguard assist
		
		// 4.2: look for intruders to aggro
		if (!acted && !ISLAND_FLAGGED(IN_ROOM(ch), ISLE_NO_AGGRO)) {
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
				if (GET_LOYALTY(vict) == GET_LOYALTY(ch)) {
					continue;	// ignore: self or same empire
				}
				if (!IS_NPC(vict) && !vict->desc) {
					continue;	// ignore: linkdead player
				}
				if (GET_LOYALTY(vict) && empire_is_friendly(GET_LOYALTY(ch), GET_LOYALTY(vict))) {
					continue;	// ignore: friendly empire
				}
				if (vict->master && !IS_NPC(vict->master) && in_same_group(vict, vict->master) && GET_LOYALTY(vict->master) == GET_LOYALTY(ch)) {
					continue;	// ignore: grouped leader is in my empire
				}
				if (!CAN_AGGRO(ch, vict)) {
					continue;	// ignore: invalid target
				}
				
				// ok it's someone we CAN hit: now see why we should
				if (MOB_FLAGGED(vict, MOB_AGGRESSIVE)) {
					// attack: aggro mobs
					hit(ch, vict, GET_EQ(ch, WEAR_WIELD), TRUE);
					acted = TRUE;
					break;
				}
				else if (IS_HOSTILE(vict) || (!IS_DISGUISED(vict) && !IS_NPC(vict) && empire_is_hostile(GET_LOYALTY(ch), GET_LOYALTY(vict), IN_ROOM(ch)))) {
					// attack: hostile to empire
					hit(ch, vict, GET_EQ(ch, WEAR_WIELD), TRUE);
					acted = TRUE;
					break;
				}
				else if (IS_NPC(vict) && GET_LOYALTY(vict) && empire_is_hostile(GET_LOYALTY(ch), GET_LOYALTY(vict), IN_ROOM(ch))) {
					// attack: hostility against empire mobs
					hit(ch, vict, GET_EQ(ch, WEAR_WIELD), TRUE);
					acted = TRUE;
					break;
				}
			}
		}	// end cityguard aggro
	}	// end cityguard
	
	// 5. scavenging
	if (!acted && MOB_FLAGGED(ch, MOB_SCAVENGER)) {
		DL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_content) {
			if (GET_OBJ_TYPE(obj) == ITEM_CORPSE && GET_CORPSE_SIZE(obj) <= GET_SIZE(ch) && !number(0, 10)) {
				act("$n eats $p.", FALSE, ch, obj, NULL, TO_ROOM);
				empty_obj_before_extract(obj);
				extract_obj(obj);
				break;
			}
		}
	}

	/* Add new mobile actions here */
}


/**
* This is called every few seconds to animate mobs in the room with players.
*/
void run_mob_echoes(void) {
	struct custom_message *mcm, *found_mcm;
	char_data *ch, *mob, *found_mob, *chiter;
	descriptor_data *desc;
	int count, sun;
	bool oops;
	
	LL_FOREACH(descriptor_list, desc) {
		// validate ch
		if (STATE(desc) != CON_PLAYING || !(ch = desc->character) || !AWAKE(ch)) {
			continue;
		}
		
		// only the first connected player in the room counts, so multiple players don't lead to spam
		oops = FALSE;
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), chiter, next_in_room) {
			if (chiter->desc) {
				if (chiter != ch) {
					oops = TRUE;	// not first
				}
				break;	// found any descriptor
			}
		}
		if (oops) {
			continue;
		}
		
		// 25% random chance per player to happen at all...
		if (number(0, 3)) {
			continue;
		}
		
		// initialize vars
		sun = get_sun_status(IN_ROOM(ch));
		found_mob = NULL;
		found_mcm = NULL;
		count = 0;
		
		// now find a mob with a valid message
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), mob, next_in_room) {
			// things that disqualify the mob
			if (mob->desc || !IS_NPC(mob) || IS_DEAD(mob) || EXTRACTED(mob) || FIGHTING(mob) || !AWAKE(mob) || MOB_FLAGGED(mob, MOB_TIED | MOB_SILENT) || IS_INJURED(mob, INJ_TIED) || GET_LED_BY(mob)) {
				continue;
			}
			
			// ok now find a random message to show?
			LL_FOREACH(MOB_CUSTOM_MSGS(mob), mcm) {
				// MOB_CUSTOM_x: types we use here
				if (mcm->type == MOB_CUSTOM_ECHO || mcm->type == MOB_CUSTOM_SAY) {
					// ok = true
				}
				else if ((mcm->type == MOB_CUSTOM_SAY_DAY || mcm->type == MOB_CUSTOM_ECHO_DAY) && (sun == SUN_LIGHT || sun == SUN_RISE)) {
					// day ok
				}
				else if ((mcm->type == MOB_CUSTOM_SAY_NIGHT || mcm->type == MOB_CUSTOM_ECHO_NIGHT) && (sun == SUN_DARK || sun == SUN_SET)) {
					// night ok
				}
				else {
					// NOT ok
					continue;
				}
				
				// looks good! pick one at random
				if (!number(0, count++) || !found_mcm) {
					found_mob = mob;
					found_mcm = mcm;
				}
			}
		}
		
		// did we find something to show?
		if (found_mcm) {
			// MOB_CUSTOM_x
			switch (found_mcm->type) {
				case MOB_CUSTOM_ECHO:
				case MOB_CUSTOM_ECHO_DAY:
				case MOB_CUSTOM_ECHO_NIGHT: {
					act(found_mcm->msg, FALSE, found_mob, NULL, NULL, TO_ROOM);
					break;
				}
				case MOB_CUSTOM_SAY:
				case MOB_CUSTOM_SAY_DAY:
				case MOB_CUSTOM_SAY_NIGHT: {
					do_say(found_mob, found_mcm->msg, 0, 0);
					break;
				}
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MOB SPAWNING ////////////////////////////////////////////////////////////

/**
* Despawns a spawned mob.
*
* @param char_data *ch the mob
*/
void despawn_mob(char_data *ch) {
	obj_data *obj, *next_obj;
	int iter;
	
	if (!IS_NPC(ch)) {
		return;	// safety check
	}
	
	// empty inventory and equipment
	DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		extract_obj(obj);
	}
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter)) {
			extract_obj(GET_EQ(ch, iter));
		}
	}

	extract_char(ch);
}


/**
* Runs a single spawn list on a room. The room is assumed to be pre-validated
* and ready to receive spawns.
*
* @param room_data *room The room to spawn in.
* @param struct spawn_info *list The spawn list to run (spawns based on flags and percentages).
* @return int The number of mobs spawned, if any.
*/
static int spawn_one_list(room_data *room, struct spawn_info *list) {
	int count, x_coord, y_coord;
	struct spawn_info *spawn;
	bool in_city, junk;
	room_data *home;
	char_data *mob, *proto;
	
	if (!room || !list) {
		return 0;	// safety first
	}
	
	home = HOME_ROOM(room);
	
	// set up data for faster checking
	x_coord = X_COORD(room);
	y_coord = Y_COORD(room);
	count = 0;
	in_city = (ROOM_OWNER(home) && is_in_city_for_empire(room, ROOM_OWNER(home), TRUE, &junk)) ? TRUE : FALSE;
	
	LL_FOREACH(list, spawn) {
		// validate flags
		if (!validate_spawn_location(room, spawn->flags, x_coord, y_coord, in_city)) {
			continue;
		}
		
		// check percent
		if (number(1, 10000) > (int)(100 * spawn->percent)) {
			continue;
		}
		
		// check repel-animals
		if (ROOM_AFF_FLAGGED(room, ROOM_AFF_REPEL_ANIMALS) && (!(proto = mob_proto(spawn->vnum)) || MOB_FLAGGED(proto, MOB_ANIMAL))) {
			continue;	// repelled by this flag
		}
		
		// passed! let's spawn
		mob = read_mobile(spawn->vnum, TRUE);
		if (!mob) {
			continue;	// in case of problem
		}
		
		// ensure loyalty
		if (ROOM_OWNER(home) && MOB_FLAGGED(mob, MOB_HUMAN | MOB_EMPIRE | MOB_CITYGUARD)) {
			GET_LOYALTY(mob) = ROOM_OWNER(home);
		}
		
		// in case of generic names
		setup_generic_npc(mob, ROOM_OWNER(home), NOTHING, NOTHING);
		
		// enforce spawn data
		set_mob_flags(mob, MOB_SPAWNED);
		
		// put in the room
		char_to_room(mob, room);
		load_mtrigger(mob);
		++count;
	}
	
	return count;
}


GLB_VALIDATOR(validate_global_map_spawns) {
	struct glb_map_spawn_bean *data = (struct glb_map_spawn_bean*)other_data;
	if (!other_data) {
		return FALSE;
	}
	return validate_spawn_location(data->room, GET_GLOBAL_SPARE_BITS(glb), data->x_coord, data->y_coord, data->in_city);
}


GLB_FUNCTION(run_global_map_spawns) {
	struct glb_map_spawn_bean *data = (struct glb_map_spawn_bean*)other_data;
	if (data) {
		spawn_one_list(data->room, GET_GLOBAL_SPAWNS(glb));
	}
}


/**
* This code processes the spawn data for one room and spawns accordingly.
* Notably, it won't spawn if more NPCs are in the room than the config const
* 'spawn_limit_per_room', and it won't spawn claimed tiles if the empire is
* timed out.
*
* This will update the last-spawned-time for the room.
*
* @param room_data *room The location to spawn.
* @param bool only_artisans If TRUE, the room has respawned too recently and will only spawn artisans
*/
static void spawn_one_room(room_data *room, bool only_artisans) {
	room_data *iter, *next_iter, *home;
	struct empire_territory_data *ter;
	struct vehicle_room_list *vrl;
	vehicle_data *veh, *next_veh;
	struct empire_npc_data *npc;
	char_data *ch_iter;
	time_t now = time(0);
	crop_data *cp;
	int count;
	mob_vnum artisan = NOTHING;
	
	int time_to_empire_emptiness = config_get_int("time_to_empire_emptiness") * SECS_PER_REAL_WEEK;
	
	// safety first
	if (!room) {
		return;
	}
	
	// start recursively inside vehicles
	DL_FOREACH_SAFE2(ROOM_VEHICLES(room), veh, next_veh, next_in_room) {
		if (!VEH_IS_EXTRACTED(veh)) {
			LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
				spawn_one_room(vrl->room, only_artisans);
			}
		}
	}
	
	artisan = (GET_BUILDING(room) ? GET_BLD_ARTISAN(GET_BUILDING(room)) : NOTHING);
	
	home = HOME_ROOM(room);
	
	if (!only_artisans) {
		// update this now, no matter what happens
		ROOM_LAST_SPAWN_TIME(room) = now;
	}
	
	// never spawn idle empires at all
	if (ROOM_OWNER(home) && EMPIRE_LAST_LOGON(ROOM_OWNER(home)) + time_to_empire_emptiness < now) {
		return;
	}
	
	
	if (!ROOM_AFF_FLAGGED(room, ROOM_AFF_REPEL_NPCS)) {
		// spawns not blocked by repel-npcs
	
		// spawn empire npcs who live here first
		if (ROOM_OWNER(home) && (ter = find_territory_entry(ROOM_OWNER(home), room))) {
			for (npc = ter->npcs; npc; npc = npc->next) {
				if (npc->mob) {
					continue;	// check if the mob is already spawned
				}
				if (only_artisans && npc->vnum != artisan) {
					continue;	// not spawning this now
				}
			
				spawn_empire_npc_to_room(ROOM_OWNER(home), npc, room, NOTHING);
			}
		}
	
		// count creatures in the room; don't spawn rooms that are already populous
		count = 0;
		DL_FOREACH2(ROOM_PEOPLE(room), ch_iter, next_in_room) {
			if (IS_NPC(ch_iter)) {
				++count;
			}
		}
	
		// normal spawn list
		if (!only_artisans && count < config_get_int("spawn_limit_per_room")) {
			// spawn based on vehicles?
			DL_FOREACH_SAFE2(ROOM_VEHICLES(room), veh, next_veh, next_in_room) {
				if (!VEH_IS_EXTRACTED(veh) && VEH_SPAWNS(veh)) {
					count += spawn_one_list(room, VEH_SPAWNS(veh));
				}
			}
		
			// spawn based on tile type -- if we're not now over the count
			if (count < config_get_int("spawn_limit_per_room")) {
				if (GET_BUILDING(room)) {
					// only find a spawn list here if the building is complete; otherwise no list = no spawn
					if (IS_COMPLETE(room)) {
						count += spawn_one_list(room, GET_BLD_SPAWNS(GET_BUILDING(room)));
					}
				}
				else if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && (cp = ROOM_CROP(room))) {
					count += spawn_one_list(room, GET_CROP_SPAWNS(cp));
				}
				else {
					count += spawn_one_list(room, GET_SECT_SPAWNS(SECT(room)));
				}
			}
		
			// global spawn lists
			if (IS_OUTDOOR_TILE(room) && !ROOM_SECT_FLAGGED(room, SECTF_NO_GLOBAL_SPAWNS) && !ROOM_CROP_FLAGGED(room, CROPF_NO_GLOBAL_SPAWNS) && count < config_get_int("spawn_limit_per_room")) {
				struct glb_map_spawn_bean *data;
				bool junk;
			
				CREATE(data, struct glb_map_spawn_bean, 1);
				data->room = room;
				data->x_coord = X_COORD(room);
				data->y_coord = Y_COORD(room);
				data->in_city = (ROOM_OWNER(home) && is_in_city_for_empire(room, ROOM_OWNER(home), TRUE, &junk)) ? TRUE : FALSE;
				run_globals(GLOBAL_MAP_SPAWNS, run_global_map_spawns, TRUE, GET_SECT_CLIMATE(BASE_SECT(room)), NULL, NULL, 0, validate_global_map_spawns, data);
				free(data);
			}
		}
	}	// end repel-npcs block
	
	// spawn interior rooms: recursively
	if (GET_INSIDE_ROOMS(room) > 0) {
		DL_FOREACH_SAFE2(interior_room_list, iter, next_iter, next_interior) {
			if (HOME_ROOM(iter) == room && iter != room) {
				spawn_one_room(iter, only_artisans);
			}
		}
	}
}


/**
* Spawns the mobs in the rooms around the center.
*
* @param room_data *center The origin location.
*/
void spawn_mobs_from_center(room_data *center) {
	int y_iter, x_iter;
	room_data *to_room;
	time_t now = time(0);
	
	int mob_spawn_interval = config_get_int("mob_spawn_interval") * SECS_PER_REAL_MIN;
	int mob_spawn_radius = config_get_int("mob_spawn_radius");
	
	// always start on the map
	center = (GET_MAP_LOC(center) ? real_room(GET_MAP_LOC(center)->vnum) : NULL);
	
	// skip if we didn't find a map location
	if (!center || GET_ROOM_VNUM(center) >= MAP_SIZE) {
		return;
	}
	
	// only bother at all if the center needs to be spawned
	if (ROOM_LAST_SPAWN_TIME(center) >= (now - mob_spawn_interval)) {
		spawn_one_room(center, TRUE);	// run an only-artisans spawn just in case
		return;
	}
	
	for (y_iter = -1 * mob_spawn_radius; y_iter <= mob_spawn_radius; ++y_iter) {
		for (x_iter = -1 * mob_spawn_radius; x_iter <= mob_spawn_radius; ++x_iter) {
			to_room = real_shift(center, x_iter, y_iter);

			if (to_room && !ROOM_AFF_FLAGGED(to_room, ROOM_AFF_UNCLAIMABLE)) {
				spawn_one_room(to_room, ROOM_LAST_SPAWN_TIME(to_room) >= (now - mob_spawn_interval));
			}
		}
	}
}


/**
* Validates 1 location for a spawn_info entry, based on the flags (including
* time-of-day flags). This does not check spawn percentage or validate the mob.
* This function takes in-city and coords as parameters because normal uses of
* the function will call it many many times.
*
* @param room_data *room The room to validate.
* @param bitvector_t spawn_flags The flags to validate for the room.
* @param int x_coord The x-coordinate of the room (prevents multiple function calls).
* @param int y_coord The y-coordinate of the room (prevents multiple function calls).
* @param bool in_city Whether or not the room is in-city (prevents multiple function calls).
* @return bool TRUE if the spawn matches; FALSE if not.
*/
bool validate_spawn_location(room_data *room, bitvector_t spawn_flags, int x_coord, int y_coord, bool in_city) {
	room_data *home;
	int season, sun = SUN_LIGHT;
	
	if (!room) {
		return FALSE;	// shortcut
	}
	
	// SPAWN_x: this code validates spawn flags
	
	// load sun info if needed
	if (IS_SET(spawn_flags, SPAWN_NOCTURNAL | SPAWN_DIURNAL)) {
		sun = get_sun_status(room);
	}
	
	// easy checks
	if (IS_SET(spawn_flags, SPAWN_NOCTURNAL) && sun != SUN_DARK && sun != SUN_SET) {
		return FALSE;
	}
	if (IS_SET(spawn_flags, SPAWN_DIURNAL) && sun != SUN_LIGHT && sun != SUN_RISE) {
		return FALSE;
	}
	
	// home room checks
	home = HOME_ROOM(room);
	if (IS_SET(spawn_flags, SPAWN_CLAIMED) && !ROOM_OWNER(home)) {
		return FALSE;
	}
	if (IS_SET(spawn_flags, SPAWN_UNCLAIMED) && ROOM_OWNER(home)) {
		return FALSE;
	}
	
	// in-city checks
	if (IS_SET(spawn_flags, SPAWN_CITY) && !in_city) {
		return FALSE;
	}
	if (IS_SET(spawn_flags, SPAWN_OUT_OF_CITY) && in_city) {
		return FALSE;
	}
	
	// validate coords
	if (IS_SET(spawn_flags, SPAWN_NORTHERN) && (y_coord == -1 || y_coord < (MAP_HEIGHT / 2))) {
		return FALSE;
	}
	if (IS_SET(spawn_flags, SPAWN_SOUTHERN) && (y_coord == -1 || y_coord >= (MAP_HEIGHT / 2))) {
		return FALSE;
	}
	if (IS_SET(spawn_flags, SPAWN_EASTERN) && (x_coord == -1 || x_coord < (MAP_WIDTH / 2))) {
		return FALSE;
	}
	if (IS_SET(spawn_flags, SPAWN_WESTERN) && (x_coord == -1 || x_coord >= (MAP_WIDTH / 2))) {
		return FALSE;
	}
	
	// validate continent/not
	if (IS_SET(spawn_flags, SPAWN_CONTINENT_ONLY)) {
		if (!GET_ISLAND(room) || !IS_SET(GET_ISLAND(room)->flags, ISLE_CONTINENT)) {
			return FALSE;	// not a continent
		}
	}
	else if (IS_SET(spawn_flags, SPAWN_NO_CONTINENT)) {
		if (GET_ISLAND(room) && IS_SET(GET_ISLAND(room)->flags, ISLE_CONTINENT)) {
			return FALSE;	// oops is a continent
		}
	}
	
	// validate seasons -- if any are set
	if (IS_SET(spawn_flags, SPAWN_SPRING_ONLY | SPAWN_SUMMER_ONLY | SPAWN_AUTUMN_ONLY | SPAWN_WINTER_ONLY)) {
		season = GET_SEASON(room);
		if (!((IS_SET(spawn_flags, SPAWN_SPRING_ONLY) && season == TILESET_SPRING) || (IS_SET(spawn_flags, SPAWN_SUMMER_ONLY) && season == TILESET_SUMMER) || (IS_SET(spawn_flags, SPAWN_AUTUMN_ONLY) && season == TILESET_AUTUMN) || (IS_SET(spawn_flags, SPAWN_WINTER_ONLY) && season == TILESET_WINTER))) {
			// none of the seasons match (only 1 must match)
			return FALSE;
		}
	}
	
	// ok?
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// MOB SCALING /////////////////////////////////////////////////////////////

/**
* This ensures that mobs are scaled correctly, and can be called any time
* two Creatures are attacking each other. It will scale both of them, if they
* are NPCs.
*
* @param char_data *mob One of the characters.
* @param char_data *attacker The other character.
* @return bool TRUE -- always
*/
bool check_scaling(char_data *mob, char_data *attacker) {
	// ensure scaling
	if (attacker != mob && IS_NPC(mob) && GET_CURRENT_SCALE_LEVEL(mob) == 0) {
		scale_mob_for_character(mob, attacker);
	}
	if (attacker != mob && IS_NPC(attacker) && GET_CURRENT_SCALE_LEVEL(attacker) == 0) {
		scale_mob_for_character(attacker, mob);
	}
	
	return TRUE;
}


/**
* This function figures out what level a mob would be scaled to if it were
* fighting ch. This is largely based on master/party.
*
* @param char_data *ch The person we'd scale based on.
* @param bool check_group if TRUE, considers group members' levels too
* @return int The ideal scale level.
*/
int determine_best_scale_level(char_data *ch, bool check_group) {
	struct group_member_data *mem;
	char_data *scale_to = ch;
	int iter, level = 0;
	
	// level caps for sub-100 scaling -- TODO this is very similar to what's done in can_wear_item()
	int level_ranges[] = { BASIC_SKILL_CAP, SPECIALTY_SKILL_CAP, CLASS_SKILL_CAP, -1 };	// terminate with -1
	
	// determine who we're really scaling to
	while (IS_NPC(scale_to) && scale_to->master) {
		scale_to = scale_to->master;
	}
	
	// now determine the ideal level based on scale_to
	if (IS_NPC(scale_to)) {
		if (GET_CURRENT_SCALE_LEVEL(scale_to) > 0) {
			level = GET_CURRENT_SCALE_LEVEL(scale_to);
		}
		else {
			// aim low -- don't scale mobs too high based on mobs alone
			level = MAX(1, GET_MIN_SCALE_LEVEL(scale_to));
		}
	}
	else {
		level = GET_COMPUTED_LEVEL(scale_to);
		
		// for people under the class cap, also cap scaling on their level range
		if (GET_SKILL_LEVEL(scale_to) < CLASS_SKILL_CAP) {
			for (iter = 0; level_ranges[iter] != -1; ++iter) {
				if (GET_SKILL_LEVEL(scale_to) <= level_ranges[iter]) {
					level = MIN(level, level_ranges[iter]);
				}
			}
		}
		
		if (check_group && GROUP(scale_to)) {
			for (mem = GROUP(scale_to)->members; mem; mem = mem->next) {
				if (mem->member != scale_to) {
					level = MAX(level, determine_best_scale_level(mem->member, FALSE));
				}
			}
		}
	}

	return level;	
}


/**
* Scales a mob below the master's level like a companion. Companions generally
* scale 25 levels below the character's level (or the use_level, if you pass
* one) if the level is over 100. That is, companions scale with the character
* up to level 100.
*
* @param char_data *mob The mob to scale.
* @param char_data *master The person to base it on.
* @param int use_level If you are using something other than character's level, e.g. the level of a skill, pass it here (or 0 to detect level here).
*/
void scale_mob_as_companion(char_data *mob, char_data *master, int use_level) {
	int scale_level;
	
	if (use_level > 0) {
		scale_level = use_level;
	}
	else {
		scale_level = get_approximate_level(master);
	}
	
	if (scale_level > CLASS_SKILL_CAP) {
		// 25 levels lower if over 100
		scale_level = MAX(CLASS_SKILL_CAP, scale_level - 25);
	}
	scale_mob_to_level(mob, scale_level);
	set_mob_flags(mob, MOB_NO_RESCALE);	// ensure it doesn't rescale itself
}


/**
* This scales one NPC to the level of a player or NPC, or as closely as
* allowed.
*
* @param char_data *mob The NPC to scale.
* @param char_data *ch The creature to scale based on.
*/
void scale_mob_for_character(char_data *mob, char_data *ch) {
	scale_mob_to_level(mob, determine_best_scale_level(ch, TRUE));
}


/**
* This scales a mob to a given level (or as close as possible given the mob's
* scale limits.
*
* @param char_data *mob The mob to scale.
* @param int level The level to scale it to.
*/
void scale_mob_to_level(char_data *mob, int level) {
	double value, target;
	int low_level, mid_level, high_level, over_level;
	int room_lev = 0, room_min = 0, room_max = 0;
	int pools_down[NUM_POOLS];
	int iter;
	
	// sanity
	if (!IS_NPC(mob)) {
		return;
	}
	
	// check location constraints
	get_scale_constraints(NULL, mob, &room_lev, &room_min, &room_max);
	
	// does the room have a forced scale?
	if (room_lev > 0) {
		level = room_lev;
	}
	
	// adjust based on mob's own settings (fall back to room's settings)
	if (GET_MIN_SCALE_LEVEL(mob) > 0) {
		level = MAX(GET_MIN_SCALE_LEVEL(mob), level);
	}
	else if (room_min > 0 && !GET_MAX_SCALE_LEVEL(mob)) {
		level = MAX(room_min, level);
	}
	
	if (GET_MAX_SCALE_LEVEL(mob) > 0) {
		level = MIN(GET_MAX_SCALE_LEVEL(mob), level);
	}
	else if (room_max > 0 && !GET_MIN_SCALE_LEVEL(mob)) {
		level = MIN(room_max, level);
	}
	
	// insanity!
	if (level <= 0) {
		return;
	}
	
	// store how far down they are on pools
	for (iter = 0; iter < NUM_POOLS; ++iter) {
		pools_down[iter] = mob->points.max_pools[iter] - mob->points.current_pools[iter];
	}
	
	// set up: determine how many levels the mob gets in each level range
	low_level = MAX(0, MIN(level, BASIC_SKILL_CAP));
	mid_level = MAX(0, MIN(level, SPECIALTY_SKILL_CAP) - BASIC_SKILL_CAP);
	high_level = MAX(0, MIN(level, CLASS_SKILL_CAP) - SPECIALTY_SKILL_CAP);
	over_level = MAX(0, level - CLASS_SKILL_CAP);
	
	GET_CURRENT_SCALE_LEVEL(mob) = level;

	// health
	value = (1.5 * low_level) + (3.25 * mid_level) + (5.0 * high_level) + (12 * over_level);
	target = 45.0;	// max multiplier of health flags
	if (MOB_FLAGGED(mob, MOB_GROUP) && MOB_FLAGGED(mob, MOB_HARD)) {	// boss
		value *= MOB_FLAGGED(mob, MOB_TANK) ? (1.0 * target) : (0.75 * target);
	}
	else if (MOB_FLAGGED(mob, MOB_GROUP)) {
		value *= MOB_FLAGGED(mob, MOB_TANK) ? (0.65 * target) : (0.5 * target);
	}
	else if (MOB_FLAGGED(mob, MOB_HARD)) {
		value *= MOB_FLAGGED(mob, MOB_TANK) ? (0.25 * target) : (0.1 * target);
	}
	else if (MOB_FLAGGED(mob, MOB_TANK)) {
		value *= (0.05 * target);
	}
	mob->points.max_pools[HEALTH] = MAX(1, (int) ceil(value));
	
	// move
	value = 100 + (10 * high_level) + (15 * over_level);
	value *= MOB_FLAGGED(mob, MOB_DPS) ? 2.0 : 1.0;
	value *= MOB_FLAGGED(mob, MOB_HARD) ? 2.0 : 1.0;
	value *= MOB_FLAGGED(mob, MOB_GROUP) ? 3.0 : 1.0;
	mob->points.max_pools[MOVE] = MAX(100, (int) ceil(value));
	
	// mana: only if caster
	value = (5 * low_level) + (8 * mid_level) + (10 * high_level) + (15 * over_level);
	value *= MOB_FLAGGED(mob, MOB_CASTER) ? 1.0 : 0;
	value *= MOB_FLAGGED(mob, MOB_HARD) ? 2.0 : 1.0;
	value *= MOB_FLAGGED(mob, MOB_GROUP) ? 3.0 : 1.0;
	mob->points.max_pools[MANA] = MAX(0, (int) ceil(value));
	
	// blood*
	value = size_data[GET_SIZE(mob)].max_blood;
	value += MOB_FLAGGED(mob, MOB_VAMPIRE) ? ((10 * high_level) + (20 * over_level)) : 0;
	value *= MOB_FLAGGED(mob, MOB_ANIMAL) ? 0.5 : 1.0;
	mob->points.max_pools[BLOOD] = MAX(1, (int) ceil(value));
	
	// strength
	value = level / 20.0;
	value *= MOB_FLAGGED(mob, MOB_DPS) ? 1.5 : 1.0;
	value += MOB_FLAGGED(mob, MOB_HARD) ? 1.5 : 0;
	value += MOB_FLAGGED(mob, MOB_GROUP) ? 2.0 : 0;
	mob->real_attributes[STRENGTH] = MAX(1, (int) ceil(value));
	
	// dexterity
	value = level / 20.0;
	value *= MOB_FLAGGED(mob, MOB_TANK) ? 1.25 : 1.0;
	value += MOB_FLAGGED(mob, MOB_HARD) ? 1.25 : 0;
	value += MOB_FLAGGED(mob, MOB_GROUP) ? 2.0 : 0;
	mob->real_attributes[DEXTERITY] = MAX(1, (int) ceil(value));
	
	// charisma
	value = level / 20.0;
	value *= MOB_FLAGGED(mob, MOB_HUMAN) ? 1.25 : 1.0;
	mob->real_attributes[CHARISMA] = MAX(1, (int) ceil(value));
	
	// greatness
	value = level / 20.0;
	value *= MOB_FLAGGED(mob, MOB_HUMAN) ? 1.25 : 1.0;
	mob->real_attributes[GREATNESS] = MAX(1, (int) ceil(value));
	
	// intelligence
	value = level / 20.0;
	value *= MOB_FLAGGED(mob, MOB_CASTER) ? 1.5 : 1.0;
	value += MOB_FLAGGED(mob, MOB_HARD) ? 1.5 : 0;
	value += MOB_FLAGGED(mob, MOB_GROUP) ? 2.0 : 0;
	mob->real_attributes[INTELLIGENCE] = MAX(1, (int) ceil(value));
	
	// wits
	value = level / 20.0;
	value *= MOB_FLAGGED(mob, MOB_DPS) ? 1.5 : 1.0;
	value *= MOB_FLAGGED(mob, MOB_TANK) ? 0.5 : 1.0;
	mob->real_attributes[WITS] = MAX(1, (int) ceil(value));
	
	// damage
	target = (low_level / 20.0) + (mid_level / 17.5) + (high_level / 15.0) + (over_level / 9.5);
	value = target * attack_hit_info[MOB_ATTACK_TYPE(mob)].speed[SPD_NORMAL];
	value *= MOB_FLAGGED(mob, MOB_DPS) ? 2.5 : 1.0;
	value *= MOB_FLAGGED(mob, MOB_HARD) ? 2.5 : 1.0;
	value *= MOB_FLAGGED(mob, MOB_GROUP) ? 3.5 : 1.0;
	if (!attack_hit_info[MOB_ATTACK_TYPE(mob)].disarmable) {
		value *= 0.7;	// disarm would cut damage in half; this brings it closer together
	}
	if (MOB_FLAGGED(mob, MOB_HARD | MOB_GROUP)) {
		value *= 1.15;	// 15% more damage from non-Normal mobs
	}
	mob->mob_specials.damage = MAX(1, (int) ceil(value));
	
	// to-hit
	value = MAX(0, level - 50) + (level * 0.1);
	value *= MOB_FLAGGED(mob, MOB_HARD) ? 1.1 : 1.0;
	value *= MOB_FLAGGED(mob, MOB_GROUP) ? 1.3 : 1.0;
	value += 50;	// to hit-cap them
	mob->mob_specials.to_hit = MAX(0, (int) round(value));
	
	// to-dodge
	value = MAX(0, level - 50) + (level * 0.1);
	value *= MOB_FLAGGED(mob, MOB_HARD) ? 1.1 : 1.0;
	value *= MOB_FLAGGED(mob, MOB_GROUP) ? 1.3 : 1.0;
	mob->mob_specials.to_dodge = MAX(0, (int) round(value));
	
	// cleanup
	for (iter = 0; iter < NUM_POOLS; ++iter) {
		mob->points.current_pools[iter] = mob->points.max_pools[iter] - pools_down[iter];
		// ensure minimum of 1 after scaling:
		mob->points.current_pools[iter] = MAX(1, MIN(mob->points.max_pools[iter], mob->points.current_pools[iter]));
	}
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		mob->aff_attributes[iter] = mob->real_attributes[iter];
	}
	affect_total(mob);
	update_pos(mob);
}
