/* ************************************************************************
*   File: act.movement.c                                  EmpireMUD 2.0b5 *
*  Usage: movement commands, door handling, & sleep/rest/etc state        *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"
#include "constants.h"

/**
* Contents:
*   Helpers
*   Move Validators
*   Move System
*   Commands
*/

// external funcs
ACMD(do_board);
ACMD(do_dismount);
ACMD(do_exits);

// local protos
bool can_enter_room(char_data *ch, room_data *room);
bool player_can_move(char_data *ch, int dir, room_data *to_room, bitvector_t flags);
void send_arrive_message(char_data *ch, room_data *from_room, room_data *to_room, int dir, bitvector_t flags);
void send_leave_message(char_data *ch, room_data *from_room, room_data *to_room, int dir, bitvector_t flags);


// door vars
#define NEED_OPEN	BIT(0)
#define NEED_CLOSED	BIT(1)

const char *cmd_door[] = {
	"open",
	"close",
};

const int flags_door[] = {
	NEED_CLOSED,
	NEED_OPEN,
};


// helper data type for listing portals
struct temp_portal_data {
	room_data *room;
	int distance;
	int in_city;
	bool is_own;
	struct temp_portal_data *next;
};


// simple sorter for portal data
int sort_temp_portal_data(struct temp_portal_data *a, struct temp_portal_data *b) {
	if (a->is_own != b->is_own) {
		return a->is_own ? -1 : 1;
	}
	else {
		return a->distance - b->distance;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* @param char_data *ch the person leaving tracks
* @param room_data *room the location of the tracks
* @param byte dir the direction the person went (if any)
* @param room_data *to_room for non-directional exits, helps the mob pursue
*/
void add_tracks(char_data *ch, room_data *room, byte dir, room_data *to_room) {
	struct track_data *track;
	int id;
	
	// check no-tracks flags first
	if (AFF_FLAGGED(ch, AFF_NO_TRACKS) || ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_TRACKS)) {
		// does not leave tracks
		return;
	}
	
	if (!IS_IMMORTAL(ch) && !ROOM_SECT_FLAGGED(room, SECTF_OCEAN | SECTF_FRESH_WATER) && SHARED_DATA(room) != &ocean_shared_data) {
		if (!IS_NPC(ch) && has_player_tech(ch, PTECH_NO_TRACK_WILD) && valid_no_trace(room)) {
			gain_player_tech_exp(ch, PTECH_NO_TRACK_WILD, 5);
		}
		else if (!IS_NPC(ch) && has_player_tech(ch, PTECH_NO_TRACK_CITY) && valid_unseen_passing(room)) {
			gain_player_tech_exp(ch, PTECH_NO_TRACK_CITY, 5);
		}
		else if ((id = GET_TRACK_ID(ch)) != 0) {
			// add tracks if they have any positive/negative track id
			HASH_FIND_INT(ROOM_TRACKS(room), &id, track);
			if (!track) {
				CREATE(track, struct track_data, 1);
				track->id = id;
				HASH_ADD_INT(ROOM_TRACKS(room), id, track);
			}
			
			// update/add track info
			track->timestamp = time(0);
			track->dir = dir;
			track->to_room = to_room ? GET_ROOM_VNUM(to_room) : NOWHERE;
			
			request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
		}
	}
}


/**
* Builds a temporary list of portals available to the character, based on a
* location. You MUST free this list after you're done with it.
*
* @param char_data *ch The person we're building the list for (their loyalty matters).
* @param room_data *origin The location to look for portals near.
* @param bool from_city If TRUE, the character is in a city.
* @param bool all_portals If TRUE, looking for all public portals, not just allies.
* @return struct temp_portal_data* The temporary list of portals.
*/
struct temp_portal_data *build_portal_list_near(char_data *ch, room_data *origin, bool from_city, bool all_portals) {
	struct temp_portal_data *port, *portal_list = NULL;
	bool there_in_city, wait_there;
	room_data *room, *next_room;
	int dist;
	
	int max_out_of_city_portal = config_get_int("max_out_of_city_portal");
	
	HASH_ITER(hh, world_table, room, next_room) {
		if (!all_portals && !ROOM_OWNER(room)) {
			continue;	// only show unowned on all
		}
		if (!IS_COMPLETE(room) || !room_has_function_and_city_ok(GET_LOYALTY(ch), room, FNC_PORTAL)) {
			continue;	// not a portal
		}
		if (!can_use_room(ch, room, all_portals ? GUESTS_ALLOWED : MEMBERS_AND_ALLIES)) {
			continue;	// not usable
		}
		
		// compute some distances
		dist = compute_distance(origin, room);
		there_in_city = ROOM_OWNER(room) ? is_in_city_for_empire(room, ROOM_OWNER(room), TRUE, &wait_there) : FALSE;
		
		if (all_portals && dist > max_out_of_city_portal && (!from_city || !there_in_city)) {
			continue;	// need city to reach it (all only)
		}
		
		// should be ok?
		CREATE(port, struct temp_portal_data, 1);
		port->room = room;
		port->distance = dist;
		port->in_city = there_in_city;
		port->is_own = (ROOM_OWNER(room) && ROOM_OWNER(room) == GET_LOYALTY(ch));
		
		LL_INSERT_INORDER(portal_list, port, sort_temp_portal_data);
	}
	
	return portal_list;
}


/**
* Determines if a character should be able to enter a portal. This will send
* its own error message.
*
* @param char_data *ch The character trying to enter a portal.
* @param obj_data *portal The portal.
* @param bool allow_infiltrate Whether or not to allow stealth entry.
* @param bool skip_permissions If TRUE (like when following), ignores permissions.
* @return bool TRUE if the character can enter the portal.
*/
bool can_enter_portal(char_data *ch, obj_data *portal, bool allow_infiltrate, bool skip_permissions) {
	room_data *to_room, *was_in = IN_ROOM(ch);
	bool ok = FALSE;
	
	// easy checks
	if (!IS_APPROVED(ch) && config_get_bool("travel_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (AFF_FLAGGED(ch, AFF_IMMOBILIZED)) {
		msg_to_char(ch, "You are immobilized and can't enter anything.\r\n");
	}
	else if (AFF_FLAGGED(ch, AFF_CHARM) && GET_LEADER(ch) && IN_ROOM(ch) == IN_ROOM(GET_LEADER(ch))) {
		msg_to_char(ch, "The thought of leaving your leader makes you weep.\r\n");
		act("$n bursts into tears.", FALSE, ch, NULL, NULL, TO_ROOM);
	}
	else if (!IS_PORTAL(portal) || !(to_room = real_room(GET_PORTAL_TARGET_VNUM(portal)))) {
		msg_to_char(ch, "You can't enter that!\r\n");
	}
	else if (!IS_IMMORTAL(ch) && !IS_NPC(ch) && IS_CARRYING_N(ch) > CAN_CARRY_N(ch)) {
		msg_to_char(ch, "You are overburdened and cannot move.\r\n");
	}
	else if (!can_enter_room(ch, to_room)) {
		msg_to_char(ch, "You can't seem to go there. Perhaps it's full.\r\n");
	}
	else if (!IS_IMMORTAL(ch) && GET_OBJ_VNUM(portal) == o_PORTAL && get_cooldown_time(ch, COOLDOWN_PORTAL_SICKNESS) > SECS_PER_REAL_MIN) {
		msg_to_char(ch, "You can't enter a portal until your portal sickness cooldown is under one minute.\r\n");
	}
	
	// leave trigger section (the was_in check is in case the player was teleported by the script)
	else if (!leave_mtrigger(ch, NO_DIR, "portal", "portal") || IN_ROOM(ch) != was_in) {
		// sends own message
	}
	else if (!leave_wtrigger(IN_ROOM(ch), ch, NO_DIR, "portal", "portal") || IN_ROOM(ch) != was_in) {
		// sends own message
	}
	else if (!leave_vtrigger(ch, NO_DIR, "portal", "portal") || IN_ROOM(ch) != was_in) {
		// sends own message
	}
	else if (!leave_otrigger(IN_ROOM(ch), ch, NO_DIR, "portal", "portal") || IN_ROOM(ch) != was_in) {
		// sends own message
	}
	else {
		ok = TRUE;
	}
	
	if (!ok) {
		return FALSE;
	}
	
	// complex checks:
	
	// vehicles
	if (GET_LEADING_VEHICLE(ch)) {
		if (!VEH_FLAGGED(GET_LEADING_VEHICLE(ch), VEH_CAN_PORTAL)) {
			act("$V can't be led through a portal.", FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_CHAR | ACT_VEH_VICT);
			return FALSE;
		}
		if (GET_ROOM_VEHICLE(to_room) && (VEH_FLAGGED(GET_LEADING_VEHICLE(ch), VEH_NO_LOAD_ONTO_VEHICLE) || !VEH_FLAGGED(GET_ROOM_VEHICLE(to_room), VEH_CARRY_VEHICLES))) {
			act("$V can't be led into $v.", FALSE, ch, GET_ROOM_VEHICLE(to_room), GET_LEADING_VEHICLE(ch), TO_CHAR | ACT_VEH_OBJ | ACT_VEH_VICT);
			return FALSE;
		}
		if (!validate_vehicle_move(ch, GET_LEADING_VEHICLE(ch), to_room)) {
			// sends own message
			return FALSE;
		}
	}
	
	// permissions
	if (!skip_permissions && ROOM_OWNER(IN_ROOM(ch)) && !IS_IMMORTAL(ch) && !IS_NPC(ch) && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || !can_use_room(ch, to_room, GUESTS_ALLOWED))) {
		if (!allow_infiltrate || !has_player_tech(ch, PTECH_INFILTRATE)) {
			msg_to_char(ch, "You don't have permission to enter that.\r\n");
			return FALSE;
		}
		if (!can_infiltrate(ch, ROOM_OWNER(IN_ROOM(ch)))) {
			// sends own message
			return FALSE;
		}
	}
	
	// SUCCESS
	return TRUE;
}


/**
* Determines if a character is allowed to enter a room via portal or walking.
*
* @param char_data *ch The player trying to enter.
* @parma room_data *room The target location.
*/
bool can_enter_room(char_data *ch, room_data *room) {
	struct instance_data *inst;
	room_data *outer_ch;
	
	// always
	if (IS_IMMORTAL(ch) || IS_NPC(ch)) {
		return TRUE;
	}
	
	// vehicle fix: find outer room for the person
	outer_ch = IN_ROOM(ch);
	while (GET_ROOM_VEHICLE(outer_ch) && IN_ROOM(GET_ROOM_VEHICLE(outer_ch))) {
		outer_ch = IN_ROOM(GET_ROOM_VEHICLE(outer_ch));
	}
	
	// player limit
	if (IS_ADVENTURE_ROOM(room) && (inst = find_instance_by_room(room, FALSE, FALSE))) {
		// only if not already in there
		if (!IS_ADVENTURE_ROOM(outer_ch) || find_instance_by_room(outer_ch, FALSE, FALSE) != inst) {
			if (!can_enter_instance(ch, inst)) {
				return FALSE;
			}
		}
	}
	
	return TRUE;
}


/**
* Tries to stop the character from flying. Only sends messages if it did stop
* flying (or at least stopped 1 type of flying).
*
* @param char_data *ch The person.
* @return bool TRUE if successful, FALSE if the character is still flying.
*/
bool check_stop_flying(char_data *ch) {
	if (affected_by_spell_and_apply(ch, ATYPE_MORPH, NOTHING, AFF_FLYING)) {
		// cannot override a morph
		return FALSE;
	}
	if (!IS_NPC(ch) && IS_RIDING(ch) && MOUNT_FLAGGED(ch, MOUNT_FLYING)) {
		do_dismount(ch, "", 0, 0);
	}
	if (AFF_FLAGGED(ch, AFF_FLYING)) {
		affects_from_char_by_aff_flag(ch, AFF_FLYING, FALSE);
		if (!AFF_FLAGGED(ch, AFF_FLYING)) {
			msg_to_char(ch, "You land.\r\n");
			act("$n lands.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
	}
	
	return EFFECTIVELY_FLYING(ch) ? FALSE : TRUE;
}


/**
* Clears a player's recent move times, which resets their shrinking map.
*
* @param char_data *ch The player.
*/
void clear_recent_moves(char_data *ch) {
	int iter;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	for (iter = 0; iter < TRACK_MOVE_TIMES; ++iter) {
		GET_MOVE_TIME(ch, iter) = 0;
	}
}


/**
* Returns the number of moves a player has made in the last 10 seconds.
*
* @param char_data *ch The player.
* @return int Number of times moved in 10 seconds.
*/
int count_recent_moves(char_data *ch) {
	time_t now = time(0);
	int iter, count = 0;
	
	if (IS_NPC(ch)) {
		return 0;
	}
	
	for (iter = 0; iter < TRACK_MOVE_TIMES; ++iter) {
		if (now - GET_MOVE_TIME(ch, iter) < 10) {
			++count;
		}
	}
	
	return count;
}


/**
* Determines the effective move type based on the movement. For example, moving
* from water to water triggers 'swim' unless flying.
*
* @param char_data *ch The person moving.
* @param room_data *to_room Optional: The target room (may be NULL).
* @return int A MOB_MOVE_ value to determine what type of movement to show.
*/
int determine_move_type(char_data *ch, room_data *to_room) {
	int type = IS_NPC(ch) ? MOB_MOVE_TYPE(ch) : MOB_MOVE_WALK;
	
	// in order of priority
	if (IS_MORPHED(ch)) {
		type = MORPH_MOVE_TYPE(GET_MORPH(ch));
	}
	else if (AFF_FLAGGED(ch, AFF_FLYING) && !IS_NPC(ch)) {
		// only switch to fly if not an npc
		type = MOB_MOVE_FLY;
	}
	else if (IS_RIDING(ch)) {
		type = MOB_MOVE_RIDE;
	}
	
	// overrides
	if (type == MOB_MOVE_WALK && to_room && (WATER_SECT(IN_ROOM(ch)) || WATER_SECT(to_room)) && type != MOB_MOVE_FLY && !HAS_WATERWALKING(ch)) {
		type = MOB_MOVE_SWIM;
	}
	else if (type == MOB_MOVE_WALK && (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_ROUGH) || ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH)) && type != MOB_MOVE_FLY) {
		type = MOB_MOVE_CLIMB;
	}
	
	return type;
}


void do_doorcmd(char_data *ch, obj_data *obj, int door, int scmd) {
	char lbuf[MAX_STRING_LENGTH];
	room_data *other_room = NULL;
	struct room_direction_data *ex = !obj ? find_exit(IN_ROOM(ch), door) : NULL, *back = NULL;
	
	#define TOGGLE_DOOR(ex, obj)	((obj) ? \
		(TOGGLE_BIT(GET_OBJ_VAL(obj, VAL_CONTAINER_FLAGS), CONT_CLOSED)) : \
		(TOGGLE_BIT((ex)->exit_info, EX_CLOSED)))
	#define OPEN_DOOR(ex, obj)	((obj) ? \
		(REMOVE_BIT(GET_OBJ_VAL(obj, VAL_CONTAINER_FLAGS), CONT_CLOSED)) : \
		(REMOVE_BIT((ex)->exit_info, EX_CLOSED)))
	#define CLOSE_DOOR(ex, obj)	((obj) ? \
		(SET_BIT(GET_OBJ_VAL(obj, VAL_CONTAINER_FLAGS), CONT_CLOSED)) : \
		(SET_BIT((ex)->exit_info, EX_CLOSED)))

	if (!door_mtrigger(ch, scmd, door)) {
		return;
	}
	if (!door_wtrigger(ch, scmd, door)) {
		return;
	}

	sprintf(lbuf, "$n %ss ", cmd_door[scmd]);
	
	// attempt to detect the reverse door, if there is one
	if (!obj && COMPLEX_DATA(IN_ROOM(ch)) && ex) {
		other_room = ex->room_ptr;
		if (other_room) {
			back = find_exit(other_room, rev_dir[door]);
			if (back != NULL && back->room_ptr != IN_ROOM(ch)) {
				back = NULL;
			}
		}
	}

	// this room/obj's door
	if (ex || obj) {
		TOGGLE_DOOR(ex, obj);
	}
	
	// toggling the door on the other side sometimes resulted in doors being permanently out-of-sync
	if (ex && back) {
		if (IS_SET(ex->exit_info, EX_CLOSED)) {
			CLOSE_DOOR(back, obj);
		}
		else {
			OPEN_DOOR(back, obj);
		}
	}
	
	send_config_msg(ch, "ok_string");

	/* Notify the room */
	sprintf(lbuf + strlen(lbuf), "%s%s.", ((obj) ? "" : "the "), (obj) ? "$p" : (ex->keyword ? "$F" : "door"));
	if (!obj || IN_ROOM(obj)) {
		act(lbuf, FALSE, ch, obj, obj ? NULL : ex->keyword, TO_ROOM | ACT_STR_VICT);
	}

	/* Notify the other room */
	if (other_room && back) {
		sprintf(lbuf, "The %s is %s%s from the other side.", (back->keyword ? fname(back->keyword) : "door"), cmd_door[scmd], (scmd == SCMD_CLOSE) ? "d" : "ed");
		if (ROOM_PEOPLE(other_room)) {
			act(buf, FALSE, ROOM_PEOPLE(other_room), 0, 0, TO_ROOM);
			act(buf, FALSE, ROOM_PEOPLE(other_room), 0, 0, TO_CHAR);
		}
		
		request_world_save(GET_ROOM_VNUM(other_room), WSAVE_ROOM);
	}
	
	// check for more saves
	if (obj) {
		request_obj_save_in_world(obj);
	}
	else {
		request_world_save(GET_ROOM_VNUM(IN_ROOM(ch)), WSAVE_ROOM);
	}
}


/**
* Various ability exp gain based on any kind of move.
*
* @param char_data *ch The person moving.
* @param room_data *was_in The room they were in before (if applicable; may be NULL).
* @param bitvector_t flags MOVE_ flags that might be passed in.
*/
void gain_ability_exp_from_moves(char_data *ch, room_data *was_in, bitvector_t flags) {
	if (IS_NPC(ch)) {
		return;
	}
	
	if (IS_SET(flags, MOVE_SWIM)) {
		gain_player_tech_exp(ch, PTECH_SWIMMING, 1);
	}
	
	if (IS_RIDING(ch)) {
		gain_player_tech_exp(ch, PTECH_RIDING, 1);
		
		if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_ROUGH)) {
			gain_player_tech_exp(ch, PTECH_RIDING_UPGRADE, 5);
		}
		if (EFFECTIVELY_FLYING(ch)) {
			gain_player_tech_exp(ch, PTECH_RIDING_FLYING, 5);
		}
	}
	else {	// not riding
		if (was_in && ROOM_SECT_FLAGGED(was_in, SECTF_ROUGH) && ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_ROUGH)) {
			gain_player_tech_exp(ch, PTECH_ROUGH_TERRAIN, 10);
		}
		gain_player_tech_exp(ch, PTECH_NAVIGATION, 1);
	}
	
	run_ability_gain_hooks(ch, NULL, AGH_MOVING);
}


int find_door(char_data *ch, const char *type, char *dir, const char *cmdname) {
	struct room_direction_data *ex;
	int door;

	if (*dir) {			/* a direction was specified */
		if ((door = parse_direction(ch, dir)) == NO_DIR) {	/* Partial Match */
			send_to_char("That's not a direction.\r\n", ch);
			return (-1);
		}
		if ((ex = find_exit(IN_ROOM(ch), door))) {	/* Braces added according to indent. -gg */
			if (ex->keyword) {
				if (isname((char *) type, ex->keyword))
					return (door);
				else {
					sprintf(buf2, "I see no %s there.\r\n", type);
					send_to_char(buf2, ch);
					return (NO_DIR);
				}
			}
			else
				return (door);
		}
		else {
			sprintf(buf2, "I really don't see how you can %s anything there.\r\n", cmdname);
			send_to_char(buf2, ch);
			return (NO_DIR);
		}
	}
	else {			/* try to locate the keyword */
		if (!*type) {
			sprintf(buf2, "What is it you want to %s?\r\n", cmdname);
			send_to_char(buf2, ch);
			return (NO_DIR);
		}
		if (COMPLEX_DATA(IN_ROOM(ch))) {
			for (ex = COMPLEX_DATA(IN_ROOM(ch))->exits; ex; ex = ex->next) {
				if (ex->keyword) {
					if (isname((char *) type, ex->keyword)) {
						return (ex->dir);
					}
				}
			}
		}

		sprintf(buf2, "There doesn't seem to be %s %s here.\r\n", AN(type), type);
		send_to_char(buf2, ch);
		return (NO_DIR);
	}
}


/**
* Finds a return portal to use for messaging.
*
* @param room_data *in_room What room to look for the portal in.
* @param room_data *from_room Find a portal leading where?
* @param obj_data *fallback Portal to use if none is found.
* @return obj_data* The portal to send the message from.
*/
obj_data *find_back_portal(room_data *in_room, room_data *from_room, obj_data *fallback) {
	obj_data *objiter;
	
	DL_FOREACH2(ROOM_CONTENTS(in_room), objiter, next_content) {
		if (GET_PORTAL_TARGET_VNUM(objiter) == GET_ROOM_VNUM(from_room)) {
			return objiter;
		}
	}
	
	// otherwise just use the original one
	return fallback;
}


/**
* Determines which room you exit to when you leave the building/vehicle you
* are currently in. This does not validate if you CAN exit from a room; it
* returns a room if possible anyway.
*
* Possible exits are:
* - if in a vehicle, the vehicle the room is in
* - if in a building, the exit of the home room
* - if in an adventure, must have look-out and not be no-teleport
*
* @param room_data *from_room The room to exit from.
* @return room_data* The room to exit to, if any.
*/
room_data *get_exit_room(room_data *from_room) {
	room_data *home;
	
	if (GET_ROOM_VEHICLE(from_room)) {
		// vehicles exit to the room they are in
		return IN_ROOM(GET_ROOM_VEHICLE(from_room));
	}
	else if ((home = HOME_ROOM(from_room)) && GET_BUILDING(home)) {
		if (ROOM_BLD_FLAGGED(home, BLD_OPEN)) {
			// open buildings exit to the same tile
			return home;
		}
		else if (BUILDING_ENTRANCE(home) != NO_DIR) {
			// regular building exits out the front
			return real_shift(home, shift_dir[rev_dir[BUILDING_ENTRANCE(home)]][0], shift_dir[rev_dir[BUILDING_ENTRANCE(home)]][1]);
		}
	}
	else if (IS_ADVENTURE_ROOM(from_room) && ROOM_INSTANCE(from_room) && RMT_FLAGGED(from_room, RMT_LOOK_OUT) && !RMT_FLAGGED(from_room, RMT_NO_TELEPORT) && !ROOM_AFF_FLAGGED(from_room, ROOM_AFF_NO_TELEPORT)) {
		// seems to be a valid exit
		home = HOME_ROOM(INST_FAKE_LOC(ROOM_INSTANCE(from_room)));
		if (ROOM_IS_CLOSED(home) && BUILDING_ENTRANCE(home) != NO_DIR) {
			// regular building exits out the front
			return real_shift(home, shift_dir[rev_dir[BUILDING_ENTRANCE(home)]][0], shift_dir[rev_dir[BUILDING_ENTRANCE(home)]][1]);
		}
		else {
			return home;	// close enough
		}
	}
	
	// all other cases
	return NULL;
}


// processes the character's north
int get_north_for_char(char_data *ch) {
	if (IS_NPC(ch) || (HAS_NAVIGATION(ch) && !IS_DRUNK(ch))) {
		return NORTH;
	}
	else {
		return (int)GET_CONFUSED_DIR(ch);
	}
}


/**
* Gives a player the appropriate amount of portal sickness.
*
* @param char_data *ch The player.
* @param obj_data *portal The portal they went through.
* @param room_data *from Origination room.
* @param room_data *to Destination room.
*/
void give_portal_sickness(char_data *ch, obj_data *portal, room_data *from, room_data *to) {
	bool junk;
	
	if (IS_NPC(ch) || IS_IMMORTAL(ch) || GET_OBJ_VNUM(portal) != o_PORTAL) {
		return;
	}

	if (get_cooldown_time(ch, COOLDOWN_PORTAL_SICKNESS) > 0) {
		if (is_in_city_for_empire(from, ROOM_OWNER(from), TRUE, &junk) && is_in_city_for_empire(to, ROOM_OWNER(to), TRUE, &junk)) {
			add_cooldown(ch, COOLDOWN_PORTAL_SICKNESS, 2 * SECS_PER_REAL_MIN);
		}
		else if (has_player_tech(ch, PTECH_PORTAL)) {
			add_cooldown(ch, COOLDOWN_PORTAL_SICKNESS, 4 * SECS_PER_REAL_MIN);
		}
		else {
			add_cooldown(ch, COOLDOWN_PORTAL_SICKNESS, 5 * SECS_PER_REAL_MIN);
		}
	}
	else {
		add_cooldown(ch, COOLDOWN_PORTAL_SICKNESS, SECS_PER_REAL_MIN);
	}
}


/**
* Marks that a player has moved, for tracking how often they move.
*
* @param char_data *ch The player moving.
*/
void mark_move_time(char_data *ch) {
	int iter;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	// slide them all up one
	for (iter = TRACK_MOVE_TIMES - 1; iter > 0; --iter) {
		GET_MOVE_TIME(ch, iter) = GET_MOVE_TIME(ch, iter-1);
	}
	
	GET_MOVE_TIME(ch, 0) = time(0);
}



/**
* Gets a movement method from a set of MOVE_ flags.
*
* @param bitvector_t flags The MOVE_ flags for this movement.
* @return char* The movement method, for triggers.
*/
inline char *move_flags_to_method(bitvector_t flags) {
	if (IS_SET(flags, MOVE_ENTER_VEH)) {
		return "enter";
	}
	else if (IS_SET(flags, MOVE_EXIT)) {
		return "exit";
	}
	else if (IS_SET(flags, MOVE_ENTER_PORTAL)) {
		return "portal";
	}
	else {
		return "move";
	}
}


/**
* This function attempts to find the next combination of a direction and
* distance ("40 west" or "east 15"). It will remove them from the string,
* leaving only the remainder of the string (if anything).
*
* @param char_data *ch The person to parse for.
* @param char *string The input string, which will be shortened if a direction is found and removed.
* @param int *dir A variable to bind the next dir to.
* @param int *dist A variable to bind the next distance to.
* @param bool send_error If TRUE, sends an error to the player.
* @return bool TRUE if a dir/dist was found, FALSE if it failed (errored).
*/
bool parse_next_dir_from_string(char_data *ch, char *string, int *dir, int *dist, bool send_error) {
	char copy[MAX_INPUT_LENGTH], word[MAX_INPUT_LENGTH], *tmp;
	int pos, found_dir = -1, found_dist = -1, start_word, end_word, one_dir;
	int mode;
	
	*dir = *dist = -1;	// default/dummy
	
	strcpy(copy, string);	// copy the string as we will overwrite it
	tmp = copy;	// will copy back over 'string' at the end
	skip_run_filler(&tmp);
	
	#define PNDFS_NO_MODE  0
	#define PNDFS_NUMBER  1
	#define PNDFS_WORD  2
	
	while (*tmp && (found_dir == -1 || found_dist == -1)) {
		for (pos = 0, mode = PNDFS_NO_MODE, start_word = -1, end_word = -1; pos < strlen(tmp) && end_word == -1; ++pos) {
			switch (mode) {
				case PNDFS_NO_MODE: {
					if (tmp[pos] == ',' || tmp[pos] == '-' || isspace(tmp[pos])) {
						continue;
					}
					
					if (isdigit(tmp[pos])) {
						mode = PNDFS_NUMBER;
						start_word = pos;
					}
					else if (isalpha(tmp[pos])) {
						mode = PNDFS_WORD;
						start_word = pos;
					}
					else {
						if (send_error) {
							msg_to_char(ch, "Unable to find a direction or distance at the start of '%s'.\r\n", tmp);
						}
						return FALSE;
					}
					break;
				}
				case PNDFS_NUMBER: {
					if (!isdigit(tmp[pos])) {
						end_word = pos;
					}
					break;
				}
				case PNDFS_WORD: {
					if (!isalpha(tmp[pos])) {
						end_word = pos;
					}
					break;
				}
			}
		}
	
		if (start_word == -1) {
			msg_to_char(ch, "You must specify a set of directions and distances.\r\n");
			return FALSE;
		}
	
		// pull out 'word'
		if (end_word != -1) {	// we have a terminated word
			strncpy(word, tmp + start_word, end_word - start_word);
			word[end_word] = '\0';	// add terminator
		
			// advance the string
			tmp += (end_word - start_word);
			skip_run_filler(&tmp);
		}
		else {	// word is at the end
			strcpy(word, tmp + start_word);
			*tmp = '\0';	// string is now empty
		}
	
		// determine if we found a distance or direction
		if (isdigit(*word)) {
			if (found_dist == -1) {
				found_dist = atoi(word);
			}
			else {
				if (send_error) {
					msg_to_char(ch, "Invalid movement string: there were two numbers in a row at the beginning.\r\n");
				}
				return FALSE;
			}
		}
		else if ((one_dir = parse_direction(ch, word)) == NO_DIR || one_dir == DIR_RANDOM) {
			if (send_error) {
				msg_to_char(ch, "Invalid movement string: unknown word '%s'.\r\n", word);
			}
			return FALSE;
		}
		else if (found_dir != -1) {
			if (send_error) {
				msg_to_char(ch, "Invalid movement string: there were two directions in a row at the beginning (you must specify a number of tiles to move in each direction).\r\n");
			}
			return FALSE;
		}
		else {
			found_dir = one_dir;
		}
	}
	
	// ok we seem to have both a direction and a distance
	strcpy(string, tmp);	// copy back to the original string
	*dir = found_dir;
	*dist = found_dist;
	return TRUE;
}


/**
* Actual transport between starting locations.
*
* @param char_data *ch The person to transport.
* @param room_data *to_room Where to send them.
*/
void perform_transport(char_data *ch, room_data *to_room) {
	room_data *was_in = IN_ROOM(ch);
	struct follow_type *k;

	msg_to_char(ch, "You transport to another starting location!\r\n");
	act("$n dematerializes and vanishes!", TRUE, ch, 0, 0, TO_ROOM);

	char_to_room(ch, to_room);
	qt_visit_room(ch, to_room);
	GET_LAST_DIR(ch) = NO_DIR;
	pre_greet_mtrigger(ch, IN_ROOM(ch), NO_DIR, "transport");	// cannot pre-greet for transport
	look_at_room(ch);

	act("$n materializes in front of you!", TRUE, ch, 0, 0, TO_ROOM);
	
	greet_triggers(ch, NO_DIR, "transport", FALSE);
	RESET_LAST_MESSAGED_TEMPERATURE(ch);
	msdp_update_room(ch);	// once we're sure we're staying

	for (k = ch->followers; k; k = k->next) {
		if ((IN_ROOM(k->follower) == was_in) && (GET_POS(k->follower) >= POS_STANDING)) {
			act("You follow $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
			perform_transport(k->follower, to_room);
		}
	}
}


/**
* Tick update for running action.
*
* @param char_data *ch The character doing the running.
*/
void process_running(char_data *ch) {
	int dir = GET_ACTION_VNUM(ch, 0), dist;
	bool done = FALSE;
	
	// translate 'dir' from the way the character THINKS he's going, to the actual way
	dir = confused_dirs[get_north_for_char(ch)][0][dir];
	
	// attempt to move
	if (!perform_move(ch, dir, NULL, MOVE_RUN)) {
		done = TRUE;
	}
	
	// limited distance?
	if (GET_ACTION_VNUM(ch, 1) > 0) {
		GET_ACTION_VNUM(ch, 1) -= 1;
		
		// finished this part of the run!
		if (GET_ACTION_VNUM(ch, 1) <= 0) {
			if (GET_ACTION_STRING(ch)) {
				if (parse_next_dir_from_string(ch, GET_ACTION_STRING(ch), &dir, &dist, FALSE) && dir != -1) {
					GET_ACTION_VNUM(ch, 0) = get_direction_for_char(ch, dir);
					GET_ACTION_VNUM(ch, 1) = dist;
				}
				else {	// count not get next dir/dist
					done = TRUE;
				}
			}
			else {	// no movement string
				done = TRUE;
			}
		}
	}
	
	if (done) {
		msg_to_char(ch, "Your run has ended.\r\n");
		cancel_action(ch);
		return;
	}
}


/*
 * Function to skip over junk in a string of run commands.
 */
void skip_run_filler(char **string) {
	const char *skippable = ",;-./\\";
	for (; **string && (isspace(**string) || strchr(skippable, **string)); (*string)++);
}


 //////////////////////////////////////////////////////////////////////////////
//// MOVE VALIDATORS /////////////////////////////////////////////////////////

/**
* This is called by both players and NPCs (unlike player_can_move) and
* determines if the character can move from their current room to the target.
*
* @param char_data *ch The character moving.
* @param int dir A real dir, not a confused dir (may be NO_DIR if portaling, etc).
* @param room_data *to_room The target room.
* @param bitvector_t flags MOVE_ flags passed along.
* @param bool triggers If TRUE, runs leave triggers.
* @return bool TRUE indicates can-move, FALSE means they were blocked (and received an error).
*/
bool char_can_move(char_data *ch, int dir, room_data *to_room, bitvector_t flags, bool triggers) {
	room_data *was_in = IN_ROOM(ch);
	
	// overrides all move restrictions
	if (PLR_FLAGGED(ch, PLR_UNRESTRICT)) {
		return TRUE;
	}
	
	if (IS_INJURED(ch, INJ_TIED) || GET_HEALTH(ch) < 1) {
		msg_to_char(ch, "You can't seem to move!\r\n");
		return FALSE;
	}
	if (AFF_FLAGGED(ch, AFF_IMMOBILIZED)) {
		msg_to_char(ch, "You are immobilized and can't move.\r\n");
		return FALSE;
	}
	if (MOB_FLAGGED(ch, MOB_TIED)) {
		msg_to_char(ch, "You are tied in place.\r\n");
		return FALSE;
	}
	if (AFF_FLAGGED(ch, AFF_CHARM) && GET_LEADER(ch) && IN_ROOM(ch) == IN_ROOM(GET_LEADER(ch))) {
		send_to_char("The thought of leaving your leader makes you weep.\r\n", ch);
		act("$n bursts into tears.", FALSE, ch, NULL, NULL, TO_ROOM);
		return FALSE;
	}
	if (GET_FEEDING_FROM(ch) || GET_FED_ON_BY(ch)) {
		msg_to_char(ch, "You can't seem to move!\r\n");
		return FALSE;
	}
	if (MOB_FLAGGED(ch, MOB_AQUATIC) && !WATER_SECT(to_room) && !EFFECTIVELY_FLYING(ch)) {
		msg_to_char(ch, "You can't go on land!\r\n");
		return FALSE;
	}
	if (!can_enter_room(ch, to_room)) {	// checks instance limits, etc
		msg_to_char(ch, "You can't seem to go there. Perhaps it's full.\r\n");
		return FALSE;
	}
	if (GET_LED_BY(ch) && IS_MAP_BUILDING(to_room) && !BLD_ALLOWS_MOUNTS(to_room)) {
		msg_to_char(ch, "You can't be led there.\r\n");
		return FALSE;
	}
	if (ROOM_BLD_FLAGGED(to_room, BLD_BARRIER) && !IS_SET(flags, MOVE_EARTHMELD) && (ROOM_OWNER(to_room) && GET_LOYALTY(ch) != ROOM_OWNER(to_room)) && IS_COMPLETE(to_room) && !EFFECTIVELY_FLYING(ch)) {
		msg_to_char(ch, "There is a barrier in your way.\r\n");
		return FALSE;
	}
	if (IS_INSIDE(was_in) && HOME_ROOM(to_room) != HOME_ROOM(was_in) && !IS_COMPLETE(HOME_ROOM(to_room))) {
		msg_to_char(ch, "You can't go there because the building is incomplete.\r\n");
		return FALSE;
	}
	if (IS_INSIDE(was_in) && HOME_ROOM(to_room) != HOME_ROOM(was_in) && GET_ROOM_VEHICLE(HOME_ROOM(to_room)) && !VEH_IS_COMPLETE(GET_ROOM_VEHICLE(HOME_ROOM(to_room)))) {
		msg_to_char(ch, "You can't go there because the %s is incomplete.\r\n", VEH_OR_BLD(GET_ROOM_VEHICLE(HOME_ROOM(to_room))));
		return FALSE;
	}
	
	// things that require a direction (i.e. player is not portaling)
	if (dir != NO_DIR) {
		int reverse = (GET_LAST_DIR(ch) == NO_DIR) ? NORTH : rev_dir[(int) GET_LAST_DIR(ch)];
		
		// on a barrier: can only go back last-direction
		if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_BARRIER) && IS_COMPLETE(IN_ROOM(ch)) && !IS_SET(flags, MOVE_EARTHMELD) && !EFFECTIVELY_FLYING(ch) && GET_LAST_DIR(ch) != NO_DIR && dir != reverse) {
			room_data *from_room = real_shift(IN_ROOM(ch), shift_dir[reverse][0], shift_dir[reverse][1]);
		
			// ok, we know we're on a wall and trying to go the wrong way. But does the next tile also have this issue?
			if (from_room && (!ROOM_BLD_FLAGGED(from_room, BLD_BARRIER) || !IS_COMPLETE(from_room) || !can_use_room(ch, from_room, MEMBERS_ONLY))) {
				msg_to_char(ch, "You are unable to pass. You can only go back %s.\r\n", dirs[get_direction_for_char(ch, reverse)]);
				return FALSE;
			}
		}
		
		// unfinished interior room
		if (ROOM_IS_CLOSED(IN_ROOM(ch)) && !IS_COMPLETE(IN_ROOM(ch)) && GET_LAST_DIR(ch) != NO_DIR && !IS_SET(flags, MOVE_EARTHMELD) && dir != rev_dir[(int) GET_LAST_DIR(ch)] && dir_to_room(IN_ROOM(ch), rev_dir[(int) GET_LAST_DIR(ch)], FALSE)) {
			msg_to_char(ch, "This room is incomplete. You can only go back %s.\r\n", dirs[get_direction_for_char(ch, rev_dir[(int) GET_LAST_DIR(ch)])]);
			return FALSE;
		}
	}
	
	// check player-can-move LAST before triggers
	if (!IS_NPC(ch) && !player_can_move(ch, dir, to_room, flags)) {
		return FALSE;
	}

	// check leave triggers (ideally this is last)
	if (triggers && (!leave_mtrigger(ch, dir, NULL, move_flags_to_method(flags)) || !leave_wtrigger(IN_ROOM(ch), ch, dir, NULL, move_flags_to_method(flags)) || !leave_vtrigger(ch, dir, NULL, move_flags_to_method(flags)) || !leave_otrigger(IN_ROOM(ch), ch, dir, NULL, move_flags_to_method(flags)))) {
		// script should have sent message
		return FALSE;
	}
	// check that they're still in the same room after leave trigs (prevent teleport crashes)
	if (IN_ROOM(ch) != was_in) {
		// script should have sent message
		return FALSE;
	}
	
	// made it this far: success
	return TRUE;
}


/**
* This determines if a PLAYER can move somewhere. Earthmeld overrides this.
*
* @param char_data *ch The player moving.
* @param int dir A real dir, not a confused dir (may be NO_DIR if portaling, etc).
* @param room_data *to_room The target room.
* @param bitvector_t flags MOVE_ flags passed along.
* @return bool TRUE indicates can-move, FALSE means they were blocked (and received an error).
*/
bool player_can_move(char_data *ch, int dir, room_data *to_room, bitvector_t flags) {
	struct affected_type *af;
	bool needs_help = FALSE;	// this will trigger a free fly effect if stuck
	
	// overrides all player move restrictions
	if (PLR_FLAGGED(ch, PLR_UNRESTRICT) || IS_SET(flags, MOVE_EARTHMELD)) {
		return TRUE;
	}
	
	// checks that only matter if the move is directional (e.g. not a portal)
	if (dir != NO_DIR) {
		// to-water checks
		if (WATER_SECT(to_room)) {
			if (!EFFECTIVELY_SWIMMING(ch)) {
				if (ROOM_IS_CLOSED(IN_ROOM(ch)) && !ROOM_IS_CLOSED(to_room)) {
					// player may be stuck
					needs_help = TRUE;
				}
				else {
					send_to_char("You don't know how to swim.\r\n", ch);
					return FALSE;
				}
			}
			// auto-swim ?
			if (!PRF_FLAGGED(ch, PRF_AUTOSWIM) && !WATER_SECT(IN_ROOM(ch)) && !IS_SET(flags, MOVE_SWIM | MOVE_IGNORE) && !EFFECTIVELY_FLYING(ch) && !HAS_WATERWALKING(ch) && !IS_INSIDE(IN_ROOM(ch)) && !IS_ADVENTURE_ROOM(IN_ROOM(ch)) && (!IS_RIDING(ch) || !MOUNT_FLAGGED(ch, MOUNT_AQUATIC | MOUNT_WATERWALKING))) {
				msg_to_char(ch, "You must type 'swim' to enter the water.\r\n");
				return FALSE;
			}
		}
	
		// water -> mountain
		if (WATER_SECT(IN_ROOM(ch)) && ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !EFFECTIVELY_FLYING(ch)) {
			send_to_char("You are unable to scale the cliff.\r\n", ch);
			return FALSE;
		}
		// mountain climbing
		if (!PRF_FLAGGED(ch, PRF_AUTOCLIMB) && !IS_SET(flags, MOVE_CLIMB | MOVE_IGNORE) && ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_ROUGH) && !EFFECTIVELY_FLYING(ch) && !IS_INSIDE(IN_ROOM(ch)) && !IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
			msg_to_char(ch, "You must type 'climb' to enter such rough terrain.\r\n");
			return FALSE;
		}
		// rough-to-rough without the ability
		if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_ROUGH) && ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && ROOM_HEIGHT(to_room) >= ROOM_HEIGHT(IN_ROOM(ch)) && (!IS_RIDING(ch) || !has_player_tech(ch, PTECH_RIDING_UPGRADE)) && !has_player_tech(ch, PTECH_ROUGH_TERRAIN) && !EFFECTIVELY_FLYING(ch)) {
			msg_to_char(ch, "You don't have the ability to cross such rough terrain.\r\n");
			return FALSE;
		}
		// entrance direction checks
		if (!ROOM_IS_CLOSED(IN_ROOM(ch)) && IS_MAP_BUILDING(to_room) && !IS_INSIDE(IN_ROOM(ch)) && !IS_ADVENTURE_ROOM(IN_ROOM(ch)) && BUILDING_ENTRANCE(to_room) != dir && ROOM_IS_CLOSED(to_room) && (!ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES) || BUILDING_ENTRANCE(to_room) != rev_dir[dir])) {
			if (ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES)) {
				msg_to_char(ch, "You can't enter it from this side. The entrances are from %s and %s.\r\n", from_dir[get_direction_for_char(ch, BUILDING_ENTRANCE(to_room))], from_dir[get_direction_for_char(ch, rev_dir[BUILDING_ENTRANCE(to_room)])]);
			}
			else {
				msg_to_char(ch, "You can't enter it from this side. The entrance is from %s.\r\n", from_dir[get_direction_for_char(ch, BUILDING_ENTRANCE(to_room))]);
			}
			return FALSE;
		}
	} // end of must-be-a-direction
	
	// too much inventory
	if (!IS_IMMORTAL(ch) && IS_CARRYING_N(ch) > CAN_CARRY_N(ch)) {
		msg_to_char(ch, "You are overburdened and cannot move.\r\n");
		return FALSE;
	}
	// permission check
	if (ROOM_IS_CLOSED(to_room) && !IS_IMMORTAL(ch) && !IS_INSIDE(IN_ROOM(ch)) && !ROOM_IS_CLOSED(IN_ROOM(ch)) && !IS_ADVENTURE_ROOM(IN_ROOM(ch)) && IS_ANY_BUILDING(to_room) && !can_use_room(ch, to_room, GUESTS_ALLOWED) && (!IS_SET(flags, MOVE_IGNORE) || (GET_LEADER(ch) && !IS_NPC(ch) && IS_NPC(GET_LEADER(ch))))) {
		msg_to_char(ch, "You can't enter a building without permission.\r\n");
		return FALSE;
	}
	// no-fly room
	if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_FLY) && EFFECTIVELY_FLYING(ch)) {
		msg_to_char(ch, "You can't fly there.\r\n");
		return FALSE;
	}
	// check if they can lead a vehicle there -- validate_vehicle_move sends own messages
	if (GET_LEADING_VEHICLE(ch) && IN_ROOM(GET_LEADING_VEHICLE(ch)) == IN_ROOM(ch) && !validate_vehicle_move(ch, GET_LEADING_VEHICLE(ch), to_room)) {
		return FALSE;
	}
	// this checks if a led MOB can move there
	if (GET_LEADING_MOB(ch) && !GET_LEADING_MOB(ch)->desc && IN_ROOM(GET_LEADING_MOB(ch)) == IN_ROOM(ch) && !char_can_move(GET_LEADING_MOB(ch), dir, to_room, flags | MOVE_LEAD, FALSE)) {
		act("You can't go there while leading $N.", FALSE, ch, NULL, GET_LEADING_MOB(ch), TO_CHAR);
		return FALSE;
	}
	
	// things that trigger auto-dismount:

	if (IS_RIDING(ch) && ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !has_player_tech(ch, PTECH_RIDING_UPGRADE) && !EFFECTIVELY_FLYING(ch)) {
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "You can't ride on such rough terrain.\r\n");
			return FALSE;
		}
	}
	if (IS_RIDING(ch) && DEEP_WATER_SECT(to_room) && !MOUNT_FLAGGED(ch, MOUNT_AQUATIC | MOUNT_WATERWALKING) && !EFFECTIVELY_FLYING(ch)) {
		// Riding-Upgrade does not help ocean
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "Your mount won't ride into the ocean.\r\n");
			return FALSE;
		}
	}
	if (IS_RIDING(ch) && !has_player_tech(ch, PTECH_RIDING_UPGRADE) && WATER_SECT(to_room) && !MOUNT_FLAGGED(ch, MOUNT_AQUATIC | MOUNT_WATERWALKING) && !EFFECTIVELY_FLYING(ch)) {
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "Your mount won't ride into the water.\r\n");
			return FALSE;
		}
	}
	if (IS_RIDING(ch) && MOUNT_FLAGGED(ch, MOUNT_AQUATIC) && !WATER_SECT(to_room) && !IS_WATER_BUILDING(to_room) && !EFFECTIVELY_FLYING(ch)) {
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "Your mount won't go onto the land.\r\n");
			return FALSE;
		}
	}
	
	// need a generic for this -- a nearly identical condition is used in do_mount
	if (IS_RIDING(ch) && IS_COMPLETE(to_room) && !BLD_ALLOWS_MOUNTS(to_room)) {
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "You can't ride indoors.\r\n");
			return FALSE;
		}
	}
	
	// were we stuck though?
	if (needs_help) {
		af = create_flag_aff(ATYPE_UNSTUCK, 1, AFF_FLYING, ch);
		affect_to_char(ch, af);
		free(af);
	}
	
	// success!
	return TRUE;
}


/**
* @param char_data *ch
* @param room_data *from Origin room
* @param room_data *to Destination room
* @param int dir Which direction is being moved
* @param bitvector_t flags MOVE_ flags passed through.
* @return int the move cost
*/
int move_cost(char_data *ch, room_data *from, room_data *to, int dir, bitvector_t flags) {
	double need_movement, cost_from, cost_to;
	
	// shortcut: is it free?
	if (IS_SET(flags, MOVE_NO_COST)) {
		return 0;
	}
	
	/* move points needed is avg. move loss for src and destination sect type */	
	if (IS_INCOMPLETE(from)) {
		// incomplte: full base cost
		cost_from = GET_SECT_MOVE_LOSS(BASE_SECT(from));
	}
	else if (!ROOM_IS_CLOSED(from)) {
		// open buildings: average of base/current
		cost_from = (GET_SECT_MOVE_LOSS(SECT(from)) + GET_SECT_MOVE_LOSS(BASE_SECT(from))) / 2.0;
	}
	else {
		// current sect cost
		cost_from = GET_SECT_MOVE_LOSS(SECT(from));
	}
	
	// cost for the space moving to
	if (IS_INCOMPLETE(to)) {
		cost_to = GET_SECT_MOVE_LOSS(BASE_SECT(to));
	}
	else if (!ROOM_IS_CLOSED(to)) {
		cost_to = (GET_SECT_MOVE_LOSS(SECT(to)) + GET_SECT_MOVE_LOSS(BASE_SECT(to))) / 2.0;
	}
	else {
		cost_to = GET_SECT_MOVE_LOSS(SECT(to));
	}
	
	// calculate movement needed
	need_movement = (cost_from + cost_to) / 2.0;
	
	// higher diagonal cost
	if (dir >= NUM_SIMPLE_DIRS && dir < NUM_2D_DIRS) {
		need_movement *= 1.414;	// sqrt(2)
	}
	
	// cheaper cost if either room is a road
	if (IS_ROAD(from) || IS_ROAD(to)) {
		need_movement /= 2.0;
	}
	
	if (IS_RIDING(ch) || EFFECTIVELY_FLYING(ch) || IS_SET(flags, MOVE_EARTHMELD) || IS_INSIDE(from) != IS_INSIDE(to)) {
		need_movement /= 4.0;
	}
	
	// no free moves
	return MAX(1, (int)need_movement);
}


/**
* Checks if 'ch' can move 'veh' from the room they are in, to 'to_room'. This
* sends its own error message.
* 
* @param char_data *ch The player trying to move. (OPTIONAL)
* @param vehicle_data *veh The vehicle trying to move, too.
* @return bool TRUE if the player's vehicle can move there, FALSE if not.
*/
bool validate_vehicle_move(char_data *ch, vehicle_data *veh, room_data *to_room) {
	char buf[MAX_STRING_LENGTH];
	bool veh_allows_veh, veh_allows_veh_home, veh_can_go_in;
	
	if (!VEH_IS_COMPLETE(veh)) {
		if (ch) {
			act("$V can't move anywhere until it's complete!", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		return FALSE;
	}
	if (VEH_HEALTH(veh) < 1) {
		if (ch) {
			act("$V can't move anywhere until it's repaired!", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		return FALSE;
	}
	
	// required number of mounts
	if (count_harnessed_animals(veh) < VEH_ANIMALS_REQUIRED(veh)) {
		if (ch) {
			snprintf(buf, sizeof(buf), "You need to harness %d animal%s to $V before it can move.", VEH_ANIMALS_REQUIRED(veh), PLURAL(VEH_ANIMALS_REQUIRED(veh)));
			act(buf, FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		return FALSE;
	}
	
	// check size limits
	if (VEH_SIZE(veh) > 0 && total_vehicle_size_in_room(to_room, GET_LOYALTY(ch)) + VEH_SIZE(veh) > config_get_int("vehicle_size_per_tile")) {
		if (ch) {
			act("There is already too much there for $V to go there.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		return FALSE;
	}
	else if (VEH_SIZE(veh) == 0 && total_small_vehicles_in_room(to_room, GET_LOYALTY(ch)) >= config_get_int("vehicle_max_per_tile")) {
		if (ch) {
			act("$V cannot go there because it's too full already.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		return FALSE;
	}
	
	// closed building checks
	if (!IS_ADVENTURE_ROOM(IN_ROOM(veh)) && IS_ANY_BUILDING(to_room) && ROOM_IS_CLOSED(to_room)) {
		// vehicle allows a vehicle in if flagged for it; buildings require ALLOW-MOUNTS instead
		veh_allows_veh = (GET_ROOM_VEHICLE(to_room) ? VEH_FLAGGED(GET_ROOM_VEHICLE(to_room), VEH_CARRY_VEHICLES) : BLD_ALLOWS_MOUNTS(to_room)) ? TRUE : FALSE;
		veh_allows_veh_home = (GET_ROOM_VEHICLE(HOME_ROOM(to_room)) ? VEH_FLAGGED(GET_ROOM_VEHICLE(HOME_ROOM(to_room)), VEH_CARRY_VEHICLES) : BLD_ALLOWS_MOUNTS(HOME_ROOM(to_room))) ? TRUE : FALSE;
		// based on where we're going, compares veh's own !BUILDING or !LOAD-IN-VEHICLE flags
		veh_can_go_in = ((GET_ROOM_VEHICLE(to_room) && !VEH_FLAGGED(GET_ROOM_VEHICLE(to_room), VEH_BUILDING)) ? !VEH_FLAGGED(veh, VEH_NO_LOAD_ONTO_VEHICLE) : !VEH_FLAGGED(veh, VEH_NO_BUILDING)) ? TRUE : FALSE;
		
		// prevent entering from outside if mounts are not allowed
		if ((!veh_can_go_in || !veh_allows_veh) && !IS_INSIDE(IN_ROOM(veh)) && !ROOM_IS_CLOSED(IN_ROOM(veh))) {
			if (ch) {
				act("$V can't go in there.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
			}
			return FALSE;
		}
		// prevent moving from an allowed building to a disallowed building (usually due to interlink)
		if (HOME_ROOM(to_room) != to_room && HOME_ROOM(to_room) != HOME_ROOM(IN_ROOM(veh)) && !veh_allows_veh && !veh_allows_veh_home) {
			if (ch) {
				act("$V can't go in there.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
			}
			return FALSE;
		}
		// prevent moving deeper into a building if part of it does not allow vehicles and you're in the entrance room
		if (IS_MAP_BUILDING(IN_ROOM(veh)) && (VEH_FLAGGED(veh, VEH_NO_BUILDING) || !BLD_ALLOWS_MOUNTS(to_room))) {
			if (ch) {
				act("$V can't go in there.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
			}
			return FALSE;
		}
	}
	
	// climate checks
	if (!ROOM_IS_CLOSED(to_room) && !vehicle_allows_climate(veh, to_room, NULL)) {
		act("$V can't go there.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		return FALSE;
	}
	
	// barrier?
	if (ROOM_BLD_FLAGGED(to_room, BLD_BARRIER)) {
		if (ch) {
			act("$V can't move that close to the barrier.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		return FALSE;
	}
	
	// terrain-based checks
	if (WATER_SECT(to_room) && !VEH_FLAGGED(veh, VEH_SAILING | VEH_FLYING)) {
		if (ch) {
			act("$V can't go onto the water.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		return FALSE;
	}
	if (!WATER_SECT(to_room) && !IS_WATER_BUILDING(to_room) && (!WATER_SECT(IN_ROOM(veh)) || !VEH_FLAGGED(veh, VEH_DRAGGABLE)) && !VEH_FLAGGED(veh, VEH_DRIVING | VEH_FLYING) && (!VEH_FLAGGED(veh, VEH_LEADABLE) || VEH_FLAGGED(veh, VEH_SAILING))) {
		if (ch) {
			act("$V can't go onto land.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		return FALSE;
	}
	if (ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !VEH_FLAGGED(veh, VEH_ALLOW_ROUGH | VEH_FLYING)) {
		if (ch) {
			act("$V can't go into rough terrain.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		return FALSE;
	}
	
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// MOVE SYSTEM /////////////////////////////////////////////////////////////

/**
* process sending ch through portal
*
* @param char_data *ch The person to send through.
* @param obj_data *portal The portal object.
* @param bool following TRUE only if this person followed someone else through.
*/
void char_through_portal(char_data *ch, obj_data *portal, bool following) {
	char *msg;
	obj_data *use_portal;
	struct follow_type *fol, *next_fol;
	room_data *to_room = real_room(GET_PORTAL_TARGET_VNUM(portal));
	room_data *was_in = IN_ROOM(ch);
	
	// safety
	if (!to_room) {
		return;
	}
	
	// cancel some actions on movement
	if (!IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE && !IS_SET(action_data[GET_ACTION(ch)].flags, ACTF_ANYWHERE) && GET_ACTION_ROOM(ch) != GET_ROOM_VNUM(to_room) && GET_ACTION_ROOM(ch) != NOWHERE) {
		cancel_action(ch);
	}
	
	// to-char entry message
	if (!(msg = obj_get_custom_message(portal, OBJ_CUSTOM_ENTER_PORTAL_TO_CHAR))) {
		msg = "You enter $p...";
	}
	act(msg, FALSE, ch, portal, NULL, TO_CHAR);
	
	// to-room entry message
	if (!(msg = obj_get_custom_message(portal, OBJ_CUSTOM_ENTER_PORTAL_TO_ROOM))) {
		msg = "$n steps into $p!";
	}
	act(msg, TRUE, ch, portal, NULL, TO_ROOM);
	
	// ch first
	char_from_room(ch);
	char_to_room(ch, to_room);
	
	// move them first, then move them back if they aren't allowed to go.
	// see if an entry trigger disallows the move
	if (!greet_triggers(ch, NO_DIR, "portal", TRUE)) {
		char_from_room(ch);
		char_to_room(ch, was_in);
		return;
	}
	
	// update visit and last-dir
	qt_visit_room(ch, to_room);
	GET_LAST_DIR(ch) = NO_DIR;
	add_tracks(ch, was_in, NO_DIR, IN_ROOM(ch));
	
	// see if there's a different portal on the other end
	use_portal = find_back_portal(to_room, was_in, portal);
	
	// to-room exit message
	if (!(msg = obj_get_custom_message(portal, OBJ_CUSTOM_EXIT_PORTAL_TO_ROOM))) {
		msg = "$n appears from $p!";
	}
	act(msg, TRUE, ch, use_portal, NULL, TO_ROOM);
	
	look_at_room(ch);
	command_lag(ch, WAIT_MOVEMENT);
	give_portal_sickness(ch, portal, was_in, to_room);
	
	// leading vehicle (movement validated by player_can_move in do_simple_move)
	if (GET_LEADING_VEHICLE(ch) && IN_ROOM(GET_LEADING_VEHICLE(ch)) == was_in) {
		if (ROOM_PEOPLE(was_in)) {
			snprintf(buf, sizeof(buf), "$V %s through $p.", mob_move_types[VEH_MOVE_TYPE(GET_LEADING_VEHICLE(ch))]);
			act(buf, FALSE, ROOM_PEOPLE(was_in), portal, GET_LEADING_VEHICLE(ch), TO_CHAR | TO_ROOM | ACT_VEH_VICT);
		}
		
		vehicle_from_room(GET_LEADING_VEHICLE(ch));
		vehicle_to_room(GET_LEADING_VEHICLE(ch), IN_ROOM(ch));
		
		snprintf(buf, sizeof(buf), "$V %s in through $p.", mob_move_types[VEH_MOVE_TYPE(GET_LEADING_VEHICLE(ch))]);
		act(buf, FALSE, ch, use_portal, GET_LEADING_VEHICLE(ch), TO_CHAR | TO_ROOM | ACT_VEH_VICT);
	}
	
	// now followers
	for (fol = ch->followers; fol; fol = next_fol) {
		next_fol = fol->next;
		if ((IN_ROOM(fol->follower) == was_in) && (GET_POS(fol->follower) >= POS_STANDING) && can_enter_room(fol->follower, to_room)) {
			if (!can_enter_portal(fol->follower, portal, TRUE, TRUE)) {
				// sent its own message
				msg_to_char(ch, "You are unable to follow.\r\n");
			}
			else {
				act("You follow $N.\r\n", FALSE, fol->follower, 0, ch, TO_CHAR);
				char_through_portal(fol->follower, portal, TRUE);
			}
		}
	}
	
	// trigger distrust?
	if (!following && ROOM_OWNER(was_in) && !IS_IMMORTAL(ch) && !IS_NPC(ch) && !can_use_room(ch, was_in, GUESTS_ALLOWED)) {
		trigger_distrust_from_stealth(ch, ROOM_OWNER(was_in));
		add_offense(ROOM_OWNER(was_in), OFFENSE_INFILTRATED, ch, was_in, offense_was_seen(ch, ROOM_OWNER(was_in), was_in) ? OFF_SEEN : NOBITS);
	}
	
	// trigger section
	if (!pre_greet_mtrigger(ch, IN_ROOM(ch), NO_DIR, "portal") || !greet_triggers(ch, NO_DIR, "portal", TRUE)) {
		char_from_room(ch);
		char_to_room(ch, was_in);
		look_at_room(ch);
	}
	
	RESET_LAST_MESSAGED_TEMPERATURE(ch);
	msdp_update_room(ch);	// once we're sure we're staying
}


/* do_simple_move assumes
*    1. That there is no leader and no followers.
*    2. That the direction exists.
*
* @param char_data *ch The person moving.
* @param int dir The direction to move.
* @param room_data *to_room The destination.
* @param bitvector_t flags MOVE_ flags that might have been passed here.
* @return bool TRUE on success, FALSE on failure
*/
bool do_simple_move(char_data *ch, int dir, room_data *to_room, bitvector_t flags) {
	room_data *was_in = IN_ROOM(ch);
	
	// basic move checks -- these send their own messages
	if (!char_can_move(ch, dir, to_room, flags, TRUE)) {
		return FALSE;
	}
	
	// update move types AFTER char_can_move: detect swim and set movement type
	if (WATER_SECT(to_room) && !EFFECTIVELY_FLYING(ch) && !IS_RIDING(ch) && !IS_SET(flags, MOVE_EARTHMELD)) {
		if (has_player_tech(ch, PTECH_SWIMMING) && !HAS_WATERWALKING(ch)) {
			flags |= MOVE_SWIM;
		}
		else if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_AQUATIC)) {
			flags |= MOVE_SWIM;
		}
	}
	
	// movement costs: do not apply to immortals/npcs; also skip entirely if set no-cost (even negative moves can't block this)
	if (!IS_IMMORTAL(ch) && !IS_NPC(ch) && !IS_SET(flags, MOVE_NO_COST)) {
		// move points needed is avg. move loss for src and destination sect type
		int need_movement = move_cost(ch, IN_ROOM(ch), to_room, dir, flags);
		
		if (GET_MOVE(ch) >= need_movement) {
			set_move(ch, GET_MOVE(ch) - need_movement);
		}
		else {
			msg_to_char(ch, "You are too exhausted%s.\r\n", (IS_SET(flags, MOVE_FOLLOW) && GET_LEADER(ch)) ? " to follow" : "");
			return FALSE;
		}
	}
	
	// cancel any 'hide' affects
	affects_from_char_by_aff_flag(ch, AFF_HIDDEN, TRUE);
	REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDDEN);
	
	// lastly, check pre-greet trigs
	if (!pre_greet_mtrigger(ch, to_room, dir, move_flags_to_method(flags))) {
		return FALSE;
	}
	
	// ACTUAL MOVEMENT
	char_from_room(ch);
	char_to_room(ch, to_room);
	qt_visit_room(ch, to_room);

	/* move them first, then move them back if they aren't allowed to go. */
	/* see if an entry trigger disallows the move */
	if (!greet_triggers(ch, dir, move_flags_to_method(flags), TRUE)) {
		char_from_room(ch);
		char_to_room(ch, was_in);
		return FALSE;
	}
	
	// MESSAGING: if we made it this far, send the messages
	send_leave_message(ch, was_in, to_room, dir, flags);
	send_arrive_message(ch, was_in, to_room, dir, flags);

	// auto-look
	if (ch->desc != NULL) {
		if (IS_SET(flags, MOVE_RUN) && !SHOW_STATUS_MESSAGES(ch, SM_TRAVEL_AUTO_LOOK)) {
			msg_to_char(ch, "You run %s to %s%s.\r\n", dirs[get_direction_for_char(ch, dir)], get_room_name(IN_ROOM(ch), FALSE), coord_display_room(ch, IN_ROOM(ch), FALSE));
		}
		else {	// normal look
			look_at_room(ch);
		}
	}
	
	// greet trigger section: this can send the player back
	if (!greet_triggers(ch, dir, move_flags_to_method(flags), TRUE)) {
		char_from_room(ch);
		char_to_room(ch, was_in);
		look_at_room(ch);
		return FALSE;
	}
	
	// mark the move, once we're sure we're staying
	GET_LAST_DIR(ch) = dir;
	mark_move_time(ch);
	command_lag(ch, WAIT_MOVEMENT);
	add_tracks(ch, was_in, dir, IN_ROOM(ch));
	gain_ability_exp_from_moves(ch, was_in, flags);
	
	RESET_LAST_MESSAGED_TEMPERATURE(ch);
	msdp_update_room(ch);
	
	// cancel some actions on movement
	if (!IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE && !IS_SET(action_data[GET_ACTION(ch)].flags, ACTF_ANYWHERE) && GET_ACTION_ROOM(ch) != GET_ROOM_VNUM(IN_ROOM(ch)) && GET_ACTION_ROOM(ch) != NOWHERE) {
		cancel_action(ch);
	}
	if (!IS_NPC(ch) && GET_ACTION(ch) == ACT_RUNNING && (!IS_SET(flags, MOVE_RUN) || IN_ROOM(ch) != to_room)) {
		cancel_action(ch);
	}
	
	return TRUE;
}


/**
* This is the main interface for a normal move.
* 
* New move params as of b5.21 -- converted need_specials_check/mode to bits
*
* @param char_data *ch The person trying to move.
* @param int dir Which dir to go (may be NO_DIR).
* @param room_data *to_room Optional: The room the player is moving to. (Pass NULL to auto-detect from 'dir'.)
* @param bitvector_t flags Various MOVE_ flags (interpreter.h)
* @return int TRUE if the move succeeded, FALSE if not.
*/
int perform_move(char_data *ch, int dir, room_data *to_room, bitvector_t flags) {
	room_data *was_in;
	struct room_direction_data *ex;
	struct follow_type *k, *next;
	char buf[MAX_STRING_LENGTH];

	if (!ch) {
		return FALSE;	// somehow
	}
	
	// optional: determine the target room: this is slightly different from room_data dir_to_room()
	if (dir != NO_DIR && !to_room) {
		if ((PLR_FLAGGED(ch, PLR_UNRESTRICT) && !IS_ADVENTURE_ROOM(IN_ROOM(ch)) && !IS_INSIDE(IN_ROOM(ch))) || !ROOM_IS_CLOSED(IN_ROOM(ch))) {
			// map movement
			if (dir >= NUM_2D_DIRS || dir < 0) {
				send_to_char("Alas, you cannot go that way...\r\n", ch);
				return FALSE;
			}
			// may produce a NULL
			to_room = real_shift(IN_ROOM(ch), shift_dir[dir][0], shift_dir[dir][1]);
		}
		else {	// non-map movement
			if (!(ex = find_exit(IN_ROOM(ch), dir)) || !ex->room_ptr) {
				msg_to_char(ch, "Alas, you cannot go that way...\r\n");
				return FALSE;
			}
			if (EXIT_FLAGGED(ex, EX_CLOSED) && ex->keyword) {
				msg_to_char(ch, "The %s is closed.\r\n", fname(ex->keyword));
				return FALSE;
			}
			to_room = ex->room_ptr;
		}
	}	// end detect-to_room
	
	// safety (and map bounds)
	if (!to_room) {
		msg_to_char(ch, "Alas, you cannot go that way...\r\n");
		return FALSE;
	}

	/* Store old room */
	was_in = IN_ROOM(ch);

	if (!do_simple_move(ch, dir, to_room, flags))
		return FALSE;
	
	// leading vehicle (movement validated by player_can_move in do_simple_move)
	if (GET_LEADING_VEHICLE(ch) && IN_ROOM(GET_LEADING_VEHICLE(ch)) == was_in) {
		if (ROOM_PEOPLE(was_in)) {
			snprintf(buf, sizeof(buf), "$v %s behind $N.", mob_move_types[VEH_MOVE_TYPE(GET_LEADING_VEHICLE(ch))]);
			act(buf, FALSE, ROOM_PEOPLE(was_in), GET_LEADING_VEHICLE(ch), ch, TO_CHAR | TO_ROOM | ACT_VEH_OBJ);
		}
		
		vehicle_from_room(GET_LEADING_VEHICLE(ch));
		vehicle_to_room(GET_LEADING_VEHICLE(ch), IN_ROOM(ch));
		
		snprintf(buf, sizeof(buf), "$V %s in behind you.", mob_move_types[VEH_MOVE_TYPE(GET_LEADING_VEHICLE(ch))]);
		act(buf, FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_CHAR | ACT_VEH_VICT);
		snprintf(buf, sizeof(buf), "$V %s in behind $n.", mob_move_types[VEH_MOVE_TYPE(GET_LEADING_VEHICLE(ch))]);
		act(buf, FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_ROOM | ACT_VEH_VICT);
	}
	// leading mob (attempt move)
	if (GET_LEADING_MOB(ch) && IN_ROOM(GET_LEADING_MOB(ch)) == was_in) {
		perform_move(GET_LEADING_MOB(ch), dir, to_room, MOVE_LEAD);
	}

	for (k = ch->followers; k; k = next) {
		next = k->next;
		if ((IN_ROOM(k->follower) == was_in) && (GET_POS(k->follower) >= POS_STANDING)) {
			act("You follow $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
			perform_move(k->follower, dir, to_room, MOVE_FOLLOW);
		}
	}
	return TRUE;
}


/**
* Sends the messages relating to arriving in a room. It will temporarily move
* the person to the 'to_room' if they aren't there, then move them back.
*
* @param char_data *ch The person moving.
* @param room_data *from_room The room they moved from (pass their current IN_ROOM if you don't have one).
* @param room_data *to_room The room they are moving to.
* @param int dir Which dir they moved, if applicable (may be NO_DIR).
* @param bitvector_t flags Any MOVE_ flags passed along.
*/
void send_arrive_message(char_data *ch, room_data *from_room, room_data *to_room, int dir, bitvector_t flags) {
	room_data *cur_room = IN_ROOM(ch);
	char msg[256], temp[256];
	char_data *targ;
	int move_type;
	bitvector_t is_animal_move = NOBITS;
	
	// no work if they can't be seen moving
	if (AFF_FLAGGED(ch, AFF_SNEAK | AFF_NO_SEE_IN_ROOM)) {
		return;
	}
	
	// move them if needed
	if (cur_room != to_room) {
		char_from_room(ch);
		char_to_room(ch, to_room);
	}
	
	// check if it can be ignored by SM_ANIMAL_MOVEMENT
	if ((MOB_FLAGGED(ch, MOB_ANIMAL) || CHAR_MORPH_FLAGGED(ch, MORPHF_ANIMAL)) && IS_OUTDOORS(ch) && !IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
		is_animal_move = ACT_ANIMAL_MOVE;
	}
	
	// prepare empty room message
	*msg = '\0';
	move_type = determine_move_type(ch, to_room);
	
	// MOVE_x: prepare message: Leave a %s (%%s) in the message if you want a direction.
	if (IS_SET(flags, MOVE_EARTHMELD)) {
		*msg = '\0';	// earthmeld hides all move msgs
	}
	else if (IS_SET(flags, MOVE_LEAD) && GET_LED_BY(ch)) {
		act("$E leads $n behind $M.", TRUE, ch, NULL, GET_LED_BY(ch), TO_NOTVICT);
		act("You lead $n behind you.", TRUE, ch, NULL, GET_LED_BY(ch), TO_VICT);
	}
	else if (IS_SET(flags, MOVE_FOLLOW) && GET_LEADER(ch)) {
		act("$n follows $N.", TRUE, ch, NULL, GET_LEADER(ch), TO_NOTVICT);
		if (CAN_SEE(GET_LEADER(ch), ch) && WIZHIDE_OK(GET_LEADER(ch), ch)) {
			act("$n follows you.", TRUE, ch, NULL, GET_LEADER(ch), TO_VICT);
		}
	}
	else if (IS_SET(flags, MOVE_ENTER_PORTAL)) {
		obj_data *portal = find_portal_in_room_targetting(from_room, GET_ROOM_VNUM(to_room));
		obj_data *use_portal = find_back_portal(to_room, from_room, portal);
		if (use_portal && obj_has_custom_message(use_portal, OBJ_CUSTOM_EXIT_PORTAL_TO_ROOM)) {
			act(obj_get_custom_message(use_portal, OBJ_CUSTOM_EXIT_PORTAL_TO_ROOM), TRUE, ch, use_portal, NULL, TO_ROOM);
		}
		else if (use_portal) {
			act("$n appears from $p!", TRUE, ch, use_portal, NULL, TO_ROOM);
		}
		else {
			act("$n appears from a portal.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
	}
	else if (IS_SET(flags, MOVE_ENTER_VEH)) {
		vehicle_data *veh = GET_ROOM_VEHICLE(to_room);
		bool is_bld = (!veh || VEH_FLAGGED(veh, VEH_BUILDING));
		if (veh && veh_has_custom_message(veh, VEH_CUSTOM_ENTER_TO_INSIDE)) {
			act(veh_get_custom_message(veh, VEH_CUSTOM_ENTER_TO_INSIDE), TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (veh && is_bld) {
			act("$n enters $V.", TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (veh && !is_bld) {
			act("$n boards $V.", TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (GET_BUILDING(HOME_ROOM(to_room))) {
			snprintf(msg, sizeof(msg), "$n enters the %s.", GET_BLD_NAME(GET_BUILDING(HOME_ROOM(to_room))));
			is_animal_move = NOBITS;	// prevent this flag
		}
		else {
			act("$n enters the building.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
	}
	else if (IS_SET(flags, MOVE_EXIT)) {
		vehicle_data *veh = GET_ROOM_VEHICLE(from_room);
		bool is_bld = (!veh || VEH_FLAGGED(veh, VEH_BUILDING));
		if (veh && veh_has_custom_message(veh, VEH_CUSTOM_EXIT_TO_OUTSIDE)) {
			act(veh_get_custom_message(veh, VEH_CUSTOM_EXIT_TO_OUTSIDE), TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (veh && is_bld) {
			act("$n exits $V.", TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (veh && !is_bld) {
			act("$n disembarks from $V.", TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (GET_BUILDING(HOME_ROOM(from_room))) {
			snprintf(msg, sizeof(msg), "$n exits the %s.", GET_BLD_NAME(GET_BUILDING(HOME_ROOM(from_room))));
			is_animal_move = NOBITS;	// prevent this flag
		}
		else {
			act("$n exits the building.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
	}
	else if ((dir == UP || dir == DOWN) && move_type == MOB_MOVE_WALK) {	// override for walking in from up/down
		snprintf(msg, sizeof(msg), "$n comes in from %%s.");
	}
	else if (dir != NO_DIR) {	// normal move message
		snprintf(msg, sizeof(msg), "$n %s %s from %%s.", mob_move_types[move_type], (ROOM_IS_CLOSED(IN_ROOM(ch)) ? "in" : "up"));
	}
	else {	// move message with no direction?
		snprintf(msg, sizeof(msg), "$n %s %s.", mob_move_types[move_type], (ROOM_IS_CLOSED(IN_ROOM(ch)) ? "in" : "up"));
	}
	
	// process/send the message, if one was set
	if (*msg) {
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), targ, next_in_room) {
			if (targ == ch || !targ->desc || !CAN_SEE(targ, ch) || !WIZHIDE_OK(targ, ch)) {
				continue;
			}
			
			if (strstr(msg, "%s") != NULL) {
				// needs direction
				snprintf(temp, sizeof(temp), msg, from_dir[get_direction_for_char(targ, dir)]);
				act(temp, TRUE, ch, NULL, targ, TO_VICT | is_animal_move);
			}
			else {
				act(msg, TRUE, ch, NULL, targ, TO_VICT | is_animal_move);
			}
		}
	}
	
	// move them back if we moved them
	if (IN_ROOM(ch) != cur_room) {
		char_from_room(ch);
		char_to_room(ch, cur_room);
	}
}


/**
* Sends the messages relating to leaving the room. This can safely be done
* AFTER scripts have run, when you're sure they can safely move. It will
* temporarily move the person back to 'from_room' if necessary.
*
* Messages are sent to the 'from_room'.
*
* @param char_data *ch The person moving.
* @param room_data *from_room The room they moved from (pass their current IN_ROOM if you don't have one).
* @param room_data *to_room The room they are moving to.
* @param int dir Which dir they moved, if applicable (may be NO_DIR).
* @param bitvector_t flags Any MOVE_ flags passed along.
*/
void send_leave_message(char_data *ch, room_data *from_room, room_data *to_room, int dir, bitvector_t flags) {
	room_data *cur_room = IN_ROOM(ch);
	char msg[256], temp[256];
	char_data *targ;
	int move_type;
	bitvector_t is_animal_move = NOBITS;
	
	// no work if they can't be seen moving
	if (AFF_FLAGGED(ch, AFF_SNEAK | AFF_NO_SEE_IN_ROOM)) {
		return;
	}
	
	// move them if needed
	if (cur_room != from_room) {
		char_from_room(ch);
		char_to_room(ch, from_room);
	}
	
	// check if it can be ignored by SM_ANIMAL_MOVEMENT
	if ((MOB_FLAGGED(ch, MOB_ANIMAL) || CHAR_MORPH_FLAGGED(ch, MORPHF_ANIMAL)) && IS_OUTDOORS(ch) && !IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
		is_animal_move = ACT_ANIMAL_MOVE;
	}
	
	// prepare empty room message
	*msg = '\0';
	move_type = determine_move_type(ch, to_room);
	
	// MOVE_x: prepare message: Leave a %s (%%s) in the message if you want a direction.
	if (IS_SET(flags, MOVE_EARTHMELD)) {
		*msg = '\0';	// earthmeld hides all move msgs
	}
	else if (IS_SET(flags, MOVE_LEAD) && GET_LED_BY(ch)) {
		snprintf(msg, sizeof(msg), "%s leads $n with %s.", HSSH(GET_LED_BY(ch)), HMHR(GET_LED_BY(ch)));
	}
	else if (IS_SET(flags, MOVE_FOLLOW) && GET_LEADER(ch) && dir != NO_DIR) {
		snprintf(msg, sizeof(msg), "$n follows %s %%s.", HMHR(GET_LEADER(ch)));
	}
	else if (IS_SET(flags, MOVE_FOLLOW) && GET_LEADER(ch)) {
		act("$n follows $M.", TRUE, ch, NULL, GET_LEADER(ch), TO_NOTVICT);
	}
	else if (IS_SET(flags, MOVE_ENTER_PORTAL)) {
		obj_data *portal = find_portal_in_room_targetting(from_room, GET_ROOM_VNUM(to_room));
		if (portal && obj_has_custom_message(portal, OBJ_CUSTOM_ENTER_PORTAL_TO_ROOM)) {
			act(obj_get_custom_message(portal, OBJ_CUSTOM_ENTER_PORTAL_TO_ROOM), TRUE, ch, portal, NULL, TO_ROOM);
		}
		else if (portal) {
			act("$n steps into $p.", TRUE, ch, portal, NULL, TO_ROOM);
		}
		else {
			act("$n steps into a portal.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
	}
	else if (IS_SET(flags, MOVE_ENTER_VEH)) {
		vehicle_data *veh = GET_ROOM_VEHICLE(to_room);
		bool is_bld = (!veh || VEH_FLAGGED(veh, VEH_BUILDING));
		if (veh && veh_has_custom_message(veh, VEH_CUSTOM_ENTER_TO_OUTSIDE)) {
			act(veh_get_custom_message(veh, VEH_CUSTOM_ENTER_TO_OUTSIDE), TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (veh && is_bld) {
			act("$n enters $V.", TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (veh && !is_bld) {
			act("$n boards $V.", TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (GET_BUILDING(HOME_ROOM(to_room))) {
			snprintf(msg, sizeof(msg), "$n enters the %s.", GET_BLD_NAME(GET_BUILDING(HOME_ROOM(to_room))));
			is_animal_move = NOBITS;	// prevent this flag
		}
		else {
			act("$n enters the building.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
	}
	else if (IS_SET(flags, MOVE_EXIT)) {
		vehicle_data *veh = GET_ROOM_VEHICLE(from_room);
		bool is_bld = (!veh || VEH_FLAGGED(veh, VEH_BUILDING));
		if (veh && veh_has_custom_message(veh, VEH_CUSTOM_EXIT_TO_INSIDE)) {
			act(veh_get_custom_message(veh, VEH_CUSTOM_EXIT_TO_INSIDE), TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (veh && is_bld) {
			act("$n exits $V.", TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (veh && !is_bld) {
			act("$n disembarks from $V.", TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
		else if (GET_BUILDING(HOME_ROOM(from_room))) {
			snprintf(msg, sizeof(msg), "$n exits the %s.", GET_BLD_NAME(GET_BUILDING(HOME_ROOM(from_room))));
			is_animal_move = NOBITS;	// prevent this flag
		}
		else {
			act("$n exits a building.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
	}
	else if ((dir == UP || dir == DOWN) && move_type == MOB_MOVE_WALK) {	// override for up/down walk
		snprintf(msg, sizeof(msg), "$n goes %%s.");
	}
	else if (dir != NO_DIR) {	// normal move message
		snprintf(msg, sizeof(msg), "$n %s %%s.", mob_move_types[move_type]);
	}
	else {	// move message with no direction?
		snprintf(msg, sizeof(msg), "$n %s away.", mob_move_types[move_type]);
	}
	
	// process/send the message
	if (*msg) {
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), targ, next_in_room) {
			if (targ == ch || !targ->desc || !CAN_SEE(targ, ch) || !WIZHIDE_OK(targ, ch)) {
				continue;
			}
			
			if (strstr(msg, "%s") != NULL) {
				// needs direction
				snprintf(temp, sizeof(temp), msg, dirs[get_direction_for_char(targ, dir)]);
				act(temp, TRUE, ch, NULL, targ, TO_VICT | is_animal_move);
			}
			else {
				act(msg, TRUE, ch, NULL, targ, TO_VICT | is_animal_move);
			}
		}
	}
	
	// move them back if we moved them
	if (IN_ROOM(ch) != cur_room) {
		char_from_room(ch);
		char_to_room(ch, cur_room);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////


ACMD(do_avoid) {
	char_data *vict;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Who would you like to avoid?\r\n");
	}
	else if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (vict == ch) {
		send_to_char("You can't avoid yourself, no matter how hard you try.\r\n", ch);
	}
	else if (GET_LEADER(vict) != ch) {
		act("$E isn't even following you.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (IS_NPC(vict)) {
		msg_to_char(ch, "You can only use 'avoid' on player characters.\r\n");
	}
	else {
		act("You manage to avoid $N.", FALSE, ch, NULL, vict, TO_CHAR);
		act("You were following $n, but $e has managed to avoid you.", FALSE, ch, NULL, vict, TO_VICT);
		act("$n manages to avoid $N, who was following $m.", TRUE, ch, NULL, vict, TO_NOTVICT);
		
		stop_follower(vict);
		GET_WAIT_STATE(vict) = 4 RL_SEC;
	}
}


ACMD(do_circle) {
	// directions to check for blocking walls for 
	const int check_circle_dirs[NUM_SIMPLE_DIRS][2] = {
		{ EAST, WEST },	// n
		{ NORTH, SOUTH }, // e
		{ EAST, WEST }, // s
		{ NORTH, SOUTH } // w
	};
	
	char_data *vict;
	int dir, iter, need_movement, side, blocked[NUM_SIMPLE_DIRS];
	room_data *to_room, *found_room = NULL, *side_room, *was_in = IN_ROOM(ch);
	struct follow_type *fol, *next_fol;
	bool ok = TRUE, found_side_clear;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Circle which direction?\r\n");
		return;
	}
	
	if ((dir = parse_direction(ch, arg)) == NO_DIR) {
		msg_to_char(ch, "Invalid direction.\r\n");
		return;
	}
	
	if (ROOM_IS_CLOSED(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to be outdoors to circle buildings.\r\n");
		return;
	}
	
	if (dir >= NUM_2D_DIRS) {
		msg_to_char(ch, "You can't circle that way.\r\n");
		return;
	}
	
	if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_BARRIER) && IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't circle from here.\r\n");
		return;
	}
	
	if (!IS_IMMORTAL(ch) && !IS_NPC(ch) && IS_CARRYING_N(ch) > CAN_CARRY_N(ch)) {
		msg_to_char(ch, "You are overburdened and cannot move.\r\n");
		return;
	}
	
	// start here
	to_room = IN_ROOM(ch);
	
	// attempt to go up to 4 rooms
	for (iter = 0; iter < 4; ++iter) {
		to_room = real_shift(to_room, shift_dir[dir][0], shift_dir[dir][1]);
		
		// map boundary
		if (!to_room) {
			msg_to_char(ch, "You can't circle that way.\r\n");
			ok = FALSE;
			break;
		}
		
		// check wall directly in the way
		if (ROOM_BLD_FLAGGED(to_room, BLD_BARRIER) && IS_COMPLETE(to_room)) {
			msg_to_char(ch, "There is a barrier in the way.\r\n");
			ok = FALSE;
			break;
		}
		
		// check invalid terrain in the way
		if (ROOM_SECT_FLAGGED(to_room, SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_ROUGH)) {
			msg_to_char(ch, "You can't circle that way.\r\n");
			ok = FALSE;
			break;
		}
		
		// check first room is blocked at all...
		if (iter == 0 && !ROOM_IS_CLOSED(to_room)) {
			msg_to_char(ch, "There's nothing to circle.\r\n");
			ok = FALSE;
			break;
		}
		
		if (iter > 0 && !ROOM_IS_CLOSED(to_room)) {
			found_room = to_room;
			break;
		}
		
		// detect blocked array
		for (side = 0; side < NUM_SIMPLE_DIRS; ++side) {
			side_room = real_shift(to_room, shift_dir[side][0], shift_dir[side][1]);
			
			if (!side_room || !ROOM_BLD_FLAGGED(side_room, BLD_BARRIER) || !IS_COMPLETE(side_room)) {
				blocked[side] = FALSE;
			}
			else {
				blocked[side] = TRUE;
			}
		}
		
		// check if the correct dirs are blocked
		found_side_clear = FALSE;
		
		if (dir < NUM_SIMPLE_DIRS) {
			for (side = 0; side < 2; ++side) {
				if (!blocked[check_circle_dirs[dir][side]]) {
					found_side_clear = TRUE;
				}
			}
		}
		else {
			// if circling at an angle, rules are slightly different
			// first, if N/S or E/W pairs are blocked, any angle is blocked
			if (!(blocked[NORTH] && blocked[SOUTH]) && !(blocked[EAST] && blocked[WEST])) {
				// check L-shaped walls
				if (dir == NORTHEAST || dir == SOUTHWEST) {
					if (!(blocked[NORTH] && blocked[EAST]) && !(blocked[SOUTH] && blocked[WEST])) {
						found_side_clear = TRUE;
					}
				}
				else if (dir == NORTHWEST || dir == SOUTHEAST) {
					if (!(blocked[NORTH] && blocked[WEST]) && !(blocked[SOUTH] && blocked[EAST])) {
						found_side_clear = TRUE;
					}
				}
				else if ((!blocked[NORTH] || !blocked[SOUTH]) && (!blocked[EAST] || !blocked[WEST])) {
					// weird misc case
					found_side_clear = TRUE;
				}
			}
		}
		
		if (!found_side_clear) {
			msg_to_char(ch, "There is a barrier in the way.\r\n");
			ok = FALSE;
			break;
		}
	}
	
	if (!ok) {
		return;
	}
	
	if (!found_room) {
		msg_to_char(ch, "You can't circle that way.\r\n");
		return;
	}
	
	// check costs
	need_movement = move_cost(ch, IN_ROOM(ch), found_room, dir, MOVE_CIRCLE);
	
	if (GET_MOVE(ch) < need_movement && !IS_IMMORTAL(ch) && !IS_NPC(ch)) {
		msg_to_char(ch, "You're too tired to circle that way.\r\n");
		return;
	}
	
	// lastly, check pre-greet trigs
	if (!pre_greet_mtrigger(ch, found_room, dir, "move")) {
		return;
	}
	
	// cancel some actions on movement
	if (!IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE && !IS_SET(action_data[GET_ACTION(ch)].flags, ACTF_ANYWHERE) && GET_ACTION_ROOM(ch) != GET_ROOM_VNUM(found_room) && GET_ACTION_ROOM(ch) != NOWHERE) {
		cancel_action(ch);
	}
	
	// message
	msg_to_char(ch, "You circle %s.\r\n", dirs[get_direction_for_char(ch, dir)]);
	if (!AFF_FLAGGED(ch, AFF_SNEAK | AFF_NO_SEE_IN_ROOM)) {
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
			if (vict != ch && vict->desc && CAN_SEE(vict, ch)) {
				sprintf(buf, "$n circles %s.", dirs[get_direction_for_char(vict, dir)]);
				act(buf, TRUE, ch, NULL, vict, TO_VICT);
			}
		}
	}

	// work
	if (!IS_IMMORTAL(ch) && !IS_NPC(ch)) {
		set_move(ch, GET_MOVE(ch) - need_movement);
	}
	mark_move_time(ch);
	char_from_room(ch);
	char_to_room(ch, found_room);
	qt_visit_room(ch, IN_ROOM(ch));
	
	GET_LAST_DIR(ch) = dir;
	
	if (ch->desc) {
		look_at_room(ch);
	}
	
	command_lag(ch, WAIT_MOVEMENT);
	
	// triggers?
	if (!greet_triggers(ch, dir, "move", TRUE)) {
		char_from_room(ch);
		char_to_room(ch, was_in);
		if (ch->desc) {
			look_at_room(ch);
		}
		return;
	}
	
	RESET_LAST_MESSAGED_TEMPERATURE(ch);
	msdp_update_room(ch);	// once we're sure we're staying
	
	gain_ability_exp_from_moves(ch, was_in, MOVE_CIRCLE);
	
	// message
	if (!AFF_FLAGGED(ch, AFF_SNEAK | AFF_NO_SEE_IN_ROOM)) {
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
			if (vict != ch && vict->desc && CAN_SEE(vict, ch)) {
				sprintf(buf, "$n circles in from %s.", from_dir[get_direction_for_char(vict, dir)]);
				act(buf, TRUE, ch, NULL, vict, TO_VICT);
			}
		}
	}

	// followers
	for (fol = ch->followers; fol; fol = next_fol) {
		next_fol = fol->next;
		if ((IN_ROOM(fol->follower) == was_in) && (GET_POS(fol->follower) >= POS_STANDING)) {
			sprintf(buf, "%s", dirs[get_direction_for_char(fol->follower, dir)]);
			do_circle(fol->follower, buf, 0, 0);
		}
	}
}


ACMD(do_climb) {
	room_data *to_room;
	int dir;

	one_argument(argument, arg);

	if (!*arg) {
		msg_to_char(ch, "Which way would you like to climb?\r\n");
	}
	else if (IS_INSIDE(IN_ROOM(ch)) || IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't do that here.\r\n");	// map only
	}
	else if ((dir = parse_direction(ch, arg)) == NO_DIR) {
		msg_to_char(ch, "That's not a direction!\r\n");
	}
	else if (dir >= NUM_2D_DIRS) {
		msg_to_char(ch, "You can't climb that way!\r\n");
	}
	else if (!(to_room = dir_to_room(IN_ROOM(ch), dir, FALSE))) {
		msg_to_char(ch, "You can't go that way!\r\n");
	}
	else if (!ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_ROUGH)) {
		msg_to_char(ch, "You can only climb onto rough terrain.\r\n");
	}
	else {
		perform_move(ch, dir, NULL, MOVE_CLIMB);
	}
}


// enters a portal
ACMD(do_enter) {
	vehicle_data *find_veh;
	char_data *tmp_char;
	obj_data *portal;
	room_data *room;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Enter what?\r\n");
		return;
	}

	generic_find(arg, NULL, FIND_OBJ_ROOM | FIND_VEHICLE_ROOM, ch, &tmp_char, &portal, &find_veh);
	
	if (find_veh) {
		// overload by passing to board
		do_board(ch, argument, 0, SCMD_ENTER);
		return;
	}
	
	if (!portal) {
		msg_to_char(ch, "You don't see %s %s here.\r\n", AN(arg), arg);
		return;
	}
	
	room = real_room(GET_PORTAL_TARGET_VNUM(portal));
	if (!room) {
		act("$p doesn't seem to lead anywhere.", FALSE, ch, portal, 0, TO_CHAR);
		return;
	}
	
	if (!can_enter_portal(ch, portal, TRUE, FALSE)) {
		// sends own message
		return;
	}
	
	char_through_portal(ch, portal, FALSE);
}


// this command tries to 'exit' the current vehicle/building, or falls through to 'exits'
ACMD(do_exit) {
	room_data *to_room;
	
	if (IS_OUTDOORS(ch) || !ROOM_CAN_EXIT(IN_ROOM(ch))) {
		// if you can't exit here, passes through to 'exits'
		do_exits(ch, "", 0, -1);
	}
	else if (!(to_room = get_exit_room(IN_ROOM(ch))) || to_room == IN_ROOM(ch)) {
		// no valid target room?
		msg_to_char(ch, "You can't exit from here.\r\n");
	}
	else if (GET_POS(ch) < POS_STANDING) {
		// if we got here but aren't standing, alert
		send_low_pos_msg(ch);
	}
	else {
		perform_move(ch, NO_DIR, to_room, MOVE_EXIT);
	}
}


ACMD(do_follow) {
	char_data *leader, *chiter;

	one_argument(argument, buf);

	if (*buf) {
		if (!(leader = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM))) {
			send_config_msg(ch, "no_person");
			return;
		}
	}
	else {
		// check for beckon
		if (!IS_NPC(ch) && GET_BECKONED_BY(ch) > 0) {
			leader = NULL;
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), chiter, next_in_room) {
				if (!REAL_NPC(chiter) && GET_IDNUM(REAL_CHAR(chiter)) == GET_BECKONED_BY(ch)) {
					leader = chiter;
					break;
				}
			}
			
			if (!leader) {
				msg_to_char(ch, "Follow whom? The person who beckoned you isn't here.\r\n");
				return;
			}
			
			// found leader if we got here
		}
		else {	// not beckoned
			send_to_char("Whom do you wish to follow?\r\n", ch);
			return;
		}
	}

	if (GET_LEADER(ch) == leader) {
		act("You are already following $M.", FALSE, ch, 0, leader, TO_CHAR);
		return;
	}
	if (AFF_FLAGGED(ch, AFF_CHARM) && (GET_LEADER(ch)))
		act("But you only feel like following $N!", FALSE, ch, 0, GET_LEADER(ch), TO_CHAR);
	else {			/* Not Charmed follow person */
		if (leader == ch) {
			if (!GET_LEADER(ch)) {
				send_to_char("You are already following yourself.\r\n", ch);
				return;
			}
			stop_follower(ch);
		}
		else {
			if (circle_follow(ch, leader)) {
				send_to_char("Sorry, but following in loops is not allowed.\r\n", ch);
				return;
			}
			if (GET_LEADER(ch)) {
				stop_follower(ch);
			}

			add_follower(ch, leader, TRUE);
			if (!IS_NPC(ch)) {
				GET_BECKONED_BY(ch) = 0;
			}
		}
		
		// short delay prevents abuse in pvp (maybe)
		command_lag(ch, WAIT_ABILITY);
	}
}


ACMD(do_gen_door) {
	int door = NO_DIR;
	char type[MAX_INPUT_LENGTH], dir[MAX_INPUT_LENGTH];
	vehicle_data *tmp_veh = NULL;
	obj_data *obj = NULL;
	char_data *victim = NULL;
	struct room_direction_data *ex;
	
	#define DOOR_IS_OPENABLE(ch, obj, ex)	((obj) ? \
			((GET_OBJ_TYPE(obj) == ITEM_CONTAINER) && \
			OBJVAL_FLAGGED(obj, CONT_CLOSEABLE)) : \
			(EXIT_FLAGGED((ex), EX_ISDOOR)))
	#define DOOR_IS_OPEN(ch, obj, ex)	((obj) ? \
			(!OBJVAL_FLAGGED(obj, CONT_CLOSED)) : \
			(!EXIT_FLAGGED((ex), EX_CLOSED)))

	#define DOOR_IS_CLOSED(ch, obj, ex)	(!(DOOR_IS_OPEN(ch, obj, ex)))

	skip_spaces(&argument);
	if (!*argument) {
		sprintf(buf, "%s what?\r\n", cmd_door[subcmd]);
		send_to_char(CAP(buf), ch);
		return;
	}
	two_arguments(argument, type, dir);
	if (!generic_find(type, NULL, FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &victim, &obj, &tmp_veh))
		door = find_door(ch, type, dir, cmd_door[subcmd]);
	
	ex = find_exit(IN_ROOM(ch), door);

	if ((obj) || (ex)) {
		if (!(DOOR_IS_OPENABLE(ch, obj, ex)))
			act("You can't $F that!", FALSE, ch, 0, cmd_door[subcmd], TO_CHAR | ACT_STR_VICT);
		else if (!DOOR_IS_OPEN(ch, obj, ex) && IS_SET(flags_door[subcmd], NEED_OPEN))
			send_to_char("But it's already closed!\r\n", ch);
		else if (!DOOR_IS_CLOSED(ch, obj, ex) && IS_SET(flags_door[subcmd], NEED_CLOSED))
			send_to_char("But it's currently open!\r\n", ch);
		else
			do_doorcmd(ch, obj, door, subcmd);
	}
	return;
}


ACMD(do_land) {
	if (!AFF_FLAGGED(ch, AFF_FLYING)) {
		msg_to_char(ch, "You aren't flying.\r\n");
		return;
	}
	
	// ensure morph isn't the cause
	if (affected_by_spell_and_apply(ch, ATYPE_MORPH, NOTHING, AFF_FLYING)) {
		msg_to_char(ch, "You can't land in this form.\r\n");
		return;
	}
	
	affects_from_char_by_aff_flag(ch, AFF_FLYING, FALSE);
	
	if (!AFF_FLAGGED(ch, AFF_FLYING)) {
		msg_to_char(ch, "You land.\r\n");
		act("$n lands.", TRUE, ch, NULL, NULL, TO_ROOM);
	}
	else {
		msg_to_char(ch, "You can't seem to land. Perhaps whatever is causing your flight can't be ended.\r\n");
	}
	
	command_lag(ch, WAIT_OTHER);
}


ACMD(do_move) {
	// this blocks normal moves but not flee
	if (is_fighting(ch)) {
		msg_to_char(ch, "You can't move while fighting!\r\n");
		return;
	}
	
	perform_move(ch, confused_dirs[get_north_for_char(ch)][0][subcmd], NULL, NOBITS);
}


// mortals have to portal from a certain building, immortals can do it anywhere
ACMD(do_portal) {
	bool all_access = ((IS_IMMORTAL(ch) && (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_TRANSFER))) || (IS_NPC(ch) && !AFF_FLAGGED(ch, AFF_CHARM)));
	char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH];
	const char *dir_str;
	struct temp_portal_data *port, *next_port, *portal_list = NULL;
	room_data *near = NULL, *target = NULL;
	obj_data *portal, *end, *obj;
	int bsize, lsize, count, num, dist;
	bool all = FALSE, wait_here = FALSE, wait_there = FALSE, ch_in_city;
	
	int max_out_of_city_portal = config_get_int("max_out_of_city_portal");
	
	ch_in_city = (is_in_city_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), TRUE, &wait_here) || (!ROOM_OWNER(IN_ROOM(ch)) && is_in_city_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), TRUE, &wait_here)));
	skip_spaces(&argument);
	
	// grab the first word off the argument
	if (*argument == '(' || *argument == '"') {	// if it starts with a ( or ", strip them
		argument = any_one_word(argument, arg);
		strcpy(argument, arg);
	}
	else {	// grab the first word but don't remove it
		skip_spaces(&argument);
		any_one_word(argument, arg);
	}
	
	if (!str_cmp(arg, "-a") || !str_cmp(arg, "-all")) {
		all = TRUE;
	}
	else if (!str_cmp(arg, "-n") || !str_cmp(arg, "-near")) {
		// portal near <coords>
		argument = any_one_arg(argument, arg);	// strip off the 'near'
		argument = any_one_word(argument, arg);	// grab coords into 'arg'
		
		if (!HAS_NAVIGATION(ch)) {
			msg_to_char(ch, "You don't have the right ability to use the '-near' feature (e.g. Navigation).\r\n");
			return;
		}
		if (!*arg) {
			msg_to_char(ch, "See portals near which coordinates?\r\n");
			return;
		}
		if (!strchr(arg, ',')) {
			msg_to_char(ch, "You can only use 'portal near' with coordinates.\r\n");
			return;
		}
		
		if (!(near = parse_room_from_coords(argument)) && !(near = find_target_room(ch, arg))) {
			msg_to_char(ch, "Invalid location.\r\n");
			return;
		}
	}
	
	// just show portals
	if (all || near || !*argument) {
		if (IS_NPC(ch) || !ch->desc) {
			msg_to_char(ch, "You can't list portals right now.\r\n");
			return;
		}
		if (!GET_LOYALTY(ch)) {
			msg_to_char(ch, "You can't get a portal list if you're not in an empire.\r\n");
			return;
		}
		
		portal_list = build_portal_list_near(ch, near ? near : IN_ROOM(ch), ch_in_city, all);
		
		// ready to show it
		if (near) {
			bsize = snprintf(buf, sizeof(buf), "Known portals near (%d, %d):\r\n", X_COORD(near), Y_COORD(near));
		}
		else {
			bsize = snprintf(buf, sizeof(buf), "Known portals:\r\n");
		}
		
		count = 0;
		LL_FOREACH_SAFE(portal_list, port, next_port) {
			// early exit
			if (bsize >= sizeof(buf) - 1) {
				break;
			}
			
			++count;
			*line = '\0';
			lsize = 0;
			
			// sequential numbering: only numbers them if it's not a -all
			if (!all) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, "%2d.", count);
			}
			
			dir_str = get_partial_direction_to(ch, IN_ROOM(ch), port->room, PRF_FLAGGED(ch, PRF_SCREEN_READER) ? FALSE : TRUE);
			
			lsize += snprintf(line + lsize, sizeof(line) - lsize, "%s %s (%s%s&0) - %d %s", coord_display_room(ch, port->room, TRUE), get_room_name(port->room, FALSE), ROOM_OWNER(port->room) ? EMPIRE_BANNER(ROOM_OWNER(port->room)) : "\t0", ROOM_OWNER(port->room) ? EMPIRE_ADJECTIVE(ROOM_OWNER(port->room)) : "not claimed", port->distance, (*dir_str ? dir_str : "away"));
			
			if ((port->distance > max_out_of_city_portal && (!ch_in_city || !port->in_city)) || (!has_player_tech(ch, PTECH_PORTAL_UPGRADE) && (!GET_LOYALTY(ch) || !EMPIRE_HAS_TECH(GET_LOYALTY(ch), TECH_MASTER_PORTALS)) && GET_ISLAND(IN_ROOM(ch)) != GET_ISLAND(port->room))) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " &r(too far)&0");
			}
			
			bsize += snprintf(buf + bsize, sizeof(buf) - bsize, "%s\r\n", line);
			
			// free RAM!
			free(port);
		}
		
		// page it in case it's long
		page_string(ch->desc, buf, TRUE);
		command_lag(ch, WAIT_OTHER);
		return;
	}
	
	// targeting: by list number (only targets member/ally portals)
	if (!target && !strchr(argument, ',') && is_number(argument) && (num = atoi(argument)) >= 1 && GET_LOYALTY(ch)) {
		portal_list = build_portal_list_near(ch, IN_ROOM(ch), ch_in_city, FALSE);
		LL_FOREACH_SAFE(portal_list, port, next_port) {
			if (!target && --num <= 0) {
				target = port->room;
			}
			free(port);	// free RAM!
		}
	}
	
	// targeting: by keywords? (only targets member/ally portals; only if not coords)
	if (!target && *argument && !strchr(argument, ',')) {
		portal_list = build_portal_list_near(ch, IN_ROOM(ch), ch_in_city, FALSE);
		LL_FOREACH_SAFE(portal_list, port, next_port) {
			if (!target && multi_isname(argument, get_room_name(port->room, FALSE))) {
				target = port->room;
			}
			free(port);	// free RAM!
		}
	}
	
	// targeting: if we didn't get a result yet, try standard targeting
	if (!target && !(target = parse_room_from_coords(argument)) && !(target = find_target_room(ch, argument))) {
		// sends own message
		return;
	}

	// ok, we have a target...
	if (!all_access && !has_player_tech(ch, PTECH_PORTAL) && (!GET_LOYALTY(ch) || !EMPIRE_HAS_TECH(GET_LOYALTY(ch), TECH_PORTALS))) {
		msg_to_char(ch, "You don't have the ability to open portals, and you aren't in an empire with that ability either.\r\n");
		return;
	}
	if (target == IN_ROOM(ch)) {
		msg_to_char(ch, "There's no point in portaling to the same place you're standing.\r\n");
		return;
	}
	if (!all_access && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to open portals here.\r\n");
		return;
	}
	if (!all_access && !can_use_room(ch, target, GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to open a portal to that location.\r\n");
		return;
	}
	if (!all_access && (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_PORTAL) || !room_has_function_and_city_ok(GET_LOYALTY(ch), target, FNC_PORTAL))) {
		msg_to_char(ch, "You can only open portals between portal buildings.\r\n");
		return;
	}
	if (!all_access && !IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "Complete the building first.\r\n");
		return;
	}
	if (!all_access && !IS_COMPLETE(target)) {
		msg_to_char(ch, "The target portal is not complete.\r\n");
		return;
	}
	if (!all_access && (!check_in_city_requirement(IN_ROOM(ch), TRUE) || !check_in_city_requirement(target, TRUE))) {
		msg_to_char(ch, "You can't open that portal because of one the locations must be in a city, and isn't.\r\n");
		return;
	}
	if (!has_player_tech(ch, PTECH_PORTAL_UPGRADE) && (!GET_LOYALTY(ch) || !EMPIRE_HAS_TECH(GET_LOYALTY(ch), TECH_MASTER_PORTALS)) && GET_ISLAND(IN_ROOM(ch)) != GET_ISLAND(target)) {
		msg_to_char(ch, "You can't open a portal to such a distant land.\r\n");
		return;
	}

	// check there's not already a portal to there from here
	DL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_content) {
		if (GET_PORTAL_TARGET_VNUM(obj) == GET_ROOM_VNUM(target)) {
			msg_to_char(ch, "There is already a portal to that location open here.\r\n");
			return;
		}
	}
	
	// check distance
	if (!all_access) {
		if (!is_in_city_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), TRUE, &wait_here) || !is_in_city_for_empire(target, ROOM_OWNER(target), TRUE, &wait_there)) {
			dist = compute_distance(IN_ROOM(ch), target);
			
			if (dist > max_out_of_city_portal) {
				msg_to_char(ch, "You can't open a portal further away than %d tile%s unless both ends are in a city%s.\r\n", max_out_of_city_portal, PLURAL(max_out_of_city_portal), wait_here ? " (this city was founded too recently)" : (wait_there ? " (that city was founded too recently)" : ""));
				return;
			}
		}
	}
	
	// portal this side
	portal = read_object(o_PORTAL, TRUE);
	set_obj_val(portal, VAL_PORTAL_TARGET_VNUM, GET_ROOM_VNUM(target));
	GET_OBJ_TIMER(portal) = 5;
	obj_to_room(portal, IN_ROOM(ch));
	
	if (all_access) {
		act("You wave your hand and create $p!", FALSE, ch, portal, 0, TO_CHAR);
		act("$n waves $s hand and creates $p!", FALSE, ch, portal, 0, TO_ROOM);
	}
	else {
		act("You chant a few mystic words and $p whirls open!", FALSE, ch, portal, 0, TO_CHAR);
		act("$n chants a few mystic words and $p whirls open!", FALSE, ch, portal, 0, TO_ROOM);
	}
		
	load_otrigger(portal);
	
	// portal other side
	end = read_object(o_PORTAL, TRUE);
	set_obj_val(end, VAL_PORTAL_TARGET_VNUM, GET_ROOM_VNUM(IN_ROOM(ch)));
	GET_OBJ_TIMER(end) = 5;
	obj_to_room(end, target);
	
	if (GET_ROOM_VNUM(IN_ROOM(end))) {
		act("$p spins open!", TRUE, ROOM_PEOPLE(IN_ROOM(end)), end, 0, TO_ROOM | TO_CHAR);
	}
	load_otrigger(end);
	
	if (GET_LOYALTY(ch)) {
		empire_player_tech_skillup(GET_LOYALTY(ch), PTECH_PORTAL, 15);
		empire_player_tech_skillup(GET_LOYALTY(ch), PTECH_PORTAL_UPGRADE, 15);
	}
	else {
		gain_player_tech_exp(ch, PTECH_PORTAL, 15);
		gain_player_tech_exp(ch, PTECH_PORTAL_UPGRADE, 15);
		run_ability_hooks_by_player_tech(ch, PTECH_PORTAL, NULL, NULL, NULL, NULL);
	}
	
	command_lag(ch, WAIT_OTHER);
}


ACMD(do_rest) {
	one_argument(argument, arg);
	
	switch (GET_POS(ch)) {
		case POS_STANDING: {
			if (*arg) {
				do_sit_on_vehicle(ch, arg, POS_RESTING);
			}
			else if (WATER_SECT(IN_ROOM(ch))) {
				msg_to_char(ch, "You can't rest in the water.\r\n");
			}
			else if (!check_stop_flying(ch)) {
				msg_to_char(ch, "You can't do that because you're flying.\r\n");
			}
			else {
				if (IS_RIDING(ch)) {
					msg_to_char(ch, "You climb down from your mount.\r\n");
					perform_dismount(ch);
				}
				send_to_char("You sit down and rest your tired bones.\r\n", ch);
				act("$n sits down and rests.", TRUE, ch, 0, 0, TO_ROOM);
				GET_POS(ch) = POS_RESTING;
			}
			break;
		}
		case POS_SITTING: {
			if (*arg) {
				do_sit_on_vehicle(ch, arg, POS_RESTING);
			}
			else if (GET_SITTING_ON(ch) && validate_sit_on_vehicle(ch, GET_SITTING_ON(ch), POS_RESTING, FALSE)) {
				send_to_char("You rest your tired bones.\r\n", ch);
				act("$n leans back and rests.", TRUE, ch, NULL, NULL, TO_ROOM);
				GET_POS(ch) = POS_RESTING;
			}
			else if (WATER_SECT(IN_ROOM(ch))) {
				msg_to_char(ch, "You can't rest in the water.\r\n");
			}
			else if (!check_stop_flying(ch)) {
				msg_to_char(ch, "You can't do that because you're flying.\r\n");
			}
			else {
				do_unseat_from_vehicle(ch);
				send_to_char("You rest your tired bones on the ground.\r\n", ch);
				act("$n rests on the ground.", TRUE, ch, 0, 0, TO_ROOM);
				GET_POS(ch) = POS_RESTING;
			}
			break;
		}
		case POS_RESTING:
			send_to_char("You are already resting.\r\n", ch);
			break;
		case POS_SLEEPING:
			send_to_char("You have to wake up first.\r\n", ch);
			break;
		case POS_FIGHTING:
			send_to_char("Rest while fighting?  Are you MAD?\r\n", ch);
			break;
		default:
			send_to_char("You stop floating around, and stop to rest your tired bones.\r\n", ch);
			act("$n stops floating around, and rests.", FALSE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_RESTING;
			break;
	}
	
	if (GET_POS(ch) == POS_SITTING && !IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE && !action_flagged(ch, ACTF_RESTING)) {
		cancel_action(ch);
	}
}


ACMD(do_run) {
	char buf[MAX_STRING_LENGTH];
	long long time_check = -1;
	room_data *path_to_room;
	char *found_path = NULL;
	int dir, dist = -1;
	bool dir_only;

	skip_run_filler(&argument);
	dir_only = !strchr(argument, ' ') && (parse_direction(ch, argument) != NO_DIR);	// only 1 word
	path_to_room = (*argument && HAS_NAVIGATION(ch)) ? parse_room_from_coords(argument) : NULL;	// maybe
	
	// basics
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!*argument && GET_ACTION(ch) == ACT_RUNNING) {
		msg_to_char(ch, "You are currently running %d tile%s %s.\r\n", GET_ACTION_VNUM(ch, 1), PLURAL(GET_ACTION_VNUM(ch, 1)), dirs[confused_dirs[get_north_for_char(ch)][0][GET_ACTION_VNUM(ch, 0)]]);
		if (GET_ACTION_STRING(ch)) {
			msg_to_char(ch, "Your remaining path is: %s\r\n", GET_ACTION_STRING(ch));
		}
	}
	else if (GET_ACTION(ch) != ACT_NONE && GET_ACTION(ch) != ACT_RUNNING) {
		msg_to_char(ch, "You're too busy doing something else.\r\n");
	}
	
	// initial parsing
	else if (!*argument) {
		msg_to_char(ch, "You must specify the path to run using directions and distances.\r\n");
	}
	else if (!dir_only && !path_to_room && !parse_next_dir_from_string(ch, argument, &dir, &dist, TRUE)) {
		// sent its own error message
	}
	else if (!dir_only && !path_to_room && (dir == -1 || dir == DIR_RANDOM)) {
		msg_to_char(ch, "Invalid path string.\r\n");
	}
	
	// optional direction-only parsing
	else if (dir_only && !path_to_room && ((dir = parse_direction(ch, argument)) == NO_DIR || dir == DIR_RANDOM)) {
		msg_to_char(ch, "Invalid direction '%s'.\r\n", argument);
	}
	
	// did they request a path?
	else if (path_to_room && path_to_room == IN_ROOM(ch)) {
		msg_to_char(ch, "You're already there!\r\n");
	}
	else if (path_to_room && get_cooldown_time(ch, COOLDOWN_PATHFINDING) > 0) {
		msg_to_char(ch, "You must wait another %d second%s before you can run-to-coordinates again.\r\n", get_cooldown_time(ch, COOLDOWN_PATHFINDING), PLURAL(get_cooldown_time(ch, COOLDOWN_PATHFINDING)));
	}
	else if (path_to_room && (time_check = microtime()) && !(found_path = get_pathfind_string(IN_ROOM(ch), path_to_room, ch, NULL, pathfind_road))) {
		msg_to_char(ch, "Unable to find a route to that location (it may be too far or there may not be a road to it).\r\n");
		// if pathfinding took longer than 0.1 seconds, set a cooldown
		if (time_check > 0 && microtime() - time_check > 100000) {
			add_cooldown(ch, COOLDOWN_PATHFINDING, 30);
			strcpy(buf, coord_display(NULL, X_COORD(path_to_room), Y_COORD(path_to_room), FALSE));
			log("Pathfinding: %s failed to find run path in time:%s to%s", GET_NAME(ch), coord_display(NULL, X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)), FALSE), buf);
		}
	}
	else if (found_path && !parse_next_dir_from_string(ch, found_path, &dir, &dist, FALSE)) {
		msg_to_char(ch, "Unable to find a route to that location (it may be too far or there may not be a road to it).\r\n");
	}
	
	else {
		// 'dir' is the way we are ACTUALLY going, but we store the direction the character thinks it is
		
		// if pathfinding took longer than 0.1 seconds, set a cooldown
		if (time_check > 0 && microtime() - time_check > 100000) {
			strcpy(buf, coord_display(NULL, X_COORD(path_to_room), Y_COORD(path_to_room), FALSE));
			log("Pathfinding: %s got slow run path (%d microseconds):%s to%s", GET_NAME(ch), (int)(microtime() - time_check), coord_display(NULL, X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)), FALSE), buf);
			add_cooldown(ch, COOLDOWN_PATHFINDING, 30);
		}
		
		GET_ACTION(ch) = ACT_NONE;	// prevents a stops-moving message
		start_action(ch, ACT_RUNNING, 0);
		GET_ACTION_VNUM(ch, 0) = get_direction_for_char(ch, dir);
		GET_ACTION_VNUM(ch, 1) = dist;	// may be -1 for continuous
		
		if (GET_ACTION_STRING(ch)) {
			free(GET_ACTION_STRING(ch));
		}
		GET_ACTION_STRING(ch) = found_path ? str_dup(found_path) : (dir_only ? NULL : str_dup(argument));
		
		msg_to_char(ch, "You start running %s...\r\n", dirs[get_direction_for_char(ch, dir)]);
	}
}


ACMD(do_sit) {
	one_argument(argument, arg);

	switch (GET_POS(ch)) {
		case POS_STANDING: {
			if (IS_RIDING(ch) && !PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
				msg_to_char(ch, "You can't do any more sitting while mounted.\r\n");
			}
			else if (*arg) {
				do_sit_on_vehicle(ch, arg, POS_SITTING);
			}
			else if (WATER_SECT(IN_ROOM(ch))) {
				msg_to_char(ch, "You can't sit in the water.\r\n");
			}
			else if (!check_stop_flying(ch)) {
				msg_to_char(ch, "You can't do that because you're flying.\r\n");
			}
			else {
				if (IS_RIDING(ch)) {
					do_dismount(ch, "", 0, 0);
				}
				send_to_char("You sit down.\r\n", ch);
				act("$n sits down.", FALSE, ch, 0, 0, TO_ROOM);
				GET_POS(ch) = POS_SITTING;
				break;
			}
			break;
		}
		case POS_SITTING:
			send_to_char("You're sitting already.\r\n", ch);
			break;
		case POS_RESTING: {
			if (*arg) {
				do_sit_on_vehicle(ch, arg, POS_SITTING);
			}
			else {
				if (GET_SITTING_ON(ch) && !validate_sit_on_vehicle(ch, GET_SITTING_ON(ch), POS_SITTING, FALSE)) {
					do_unseat_from_vehicle(ch);
				}
				
				send_to_char("You stop resting, and sit up.\r\n", ch);
				act("$n stops resting.", TRUE, ch, 0, 0, TO_ROOM);
				GET_POS(ch) = POS_SITTING;
			}
			break;
		}
		case POS_SLEEPING:
			send_to_char("You have to wake up first.\r\n", ch);
			break;
		case POS_FIGHTING:
			send_to_char("Sit down while fighting? Are you MAD?\r\n", ch);
			break;
		default:
			send_to_char("You stop floating around, and sit down.\r\n", ch);
			act("$n stops floating around, and sits down.", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_SITTING;
			break;
	}
	
	if (GET_POS(ch) == POS_SITTING && !IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE && !action_flagged(ch, ACTF_SITTING)) {
		cancel_action(ch);
	}
}


ACMD(do_sleep) {
	one_argument(argument, arg);
	
	switch (GET_POS(ch)) {
		case POS_SITTING:
		case POS_STANDING:
		case POS_RESTING: {
			if (*arg) {
				do_sit_on_vehicle(ch, arg, POS_SLEEPING);
			}
			else {
				if (GET_SITTING_ON(ch) && !validate_sit_on_vehicle(ch, GET_SITTING_ON(ch), POS_SLEEPING, FALSE)) {
					do_unseat_from_vehicle(ch);
				}
				if (WATER_SECT(IN_ROOM(ch)) && !GET_SITTING_ON(ch)) {
					// only if they were unseated
					msg_to_char(ch, "You can't sleep in the water.\r\n");
					return;
				}
				if (!check_stop_flying(ch)) {
					msg_to_char(ch, "You can't do that because you're flying.\r\n");
					return;
				}
				if (IS_RIDING(ch)) {
					msg_to_char(ch, "You climb down from your mount.\r\n");
					perform_dismount(ch);
				}
				send_to_char("You lie down and go to sleep.\r\n", ch);
				act("$n lies down and falls asleep.", TRUE, ch, 0, 0, TO_ROOM);
				GET_POS(ch) = POS_SLEEPING;
			}
			break;
		}
		case POS_SLEEPING:
			send_to_char("You are already sound asleep.\r\n", ch);
			break;
		case POS_FIGHTING:
			send_to_char("Sleep while fighting?  Are you MAD?\r\n", ch);
			break;
		default:
			send_to_char("You stop floating around, and lie down to sleep.\r\n", ch);
			act("$n stops floating around, and lie down to sleep.", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_SLEEPING;
			break;
	}
	
	if (GET_POS(ch) == POS_SLEEPING && !IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE) {
		cancel_action(ch);
	}
}


ACMD(do_stand) {
	switch (GET_POS(ch)) {
		case POS_STANDING:
			send_to_char("You are already standing.\r\n", ch);
			break;
		case POS_SITTING: {
			do_unseat_from_vehicle(ch);
			send_to_char("You stand up.\r\n", ch);
			act("$n clambers to $s feet.", TRUE, ch, 0, 0, TO_ROOM);
			/* Will be sitting after a successful bash and may still be fighting. */
			GET_POS(ch) = FIGHTING(ch) ? POS_FIGHTING : POS_STANDING;
			break;
		}
		case POS_RESTING: {
			do_unseat_from_vehicle(ch);
			send_to_char("You stop resting, and stand up.\r\n", ch);
			act("$n stops resting, and clambers on $s feet.", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_STANDING;
			break;
		}
		case POS_SLEEPING:
			send_to_char("You have to wake up first!\r\n", ch);
			break;
		case POS_FIGHTING:
			send_to_char("Do you not consider fighting as standing?\r\n", ch);
			break;
		default:
			send_to_char("You stop floating around, and put your feet on the ground.\r\n", ch);
			act("$n stops floating around, and puts $s feet on the ground.", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_STANDING;
			break;
	}
}


ACMD(do_swim) {
	room_data *to_room;
	int dir;

	one_argument(argument, arg);

	if (!*arg) {
		msg_to_char(ch, "Which way would you like to swim?\r\n");
	}
	else if (IS_INSIDE(IN_ROOM(ch)) || IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't do that here.\r\n");	// map only
	}
	else if ((dir = parse_direction(ch, arg)) == NO_DIR) {
		msg_to_char(ch, "That's not a direction!\r\n");
	}
	else if (dir >= NUM_2D_DIRS) {
		msg_to_char(ch, "You can't swim that way!\r\n");
	}
	else if (!(to_room = dir_to_room(IN_ROOM(ch), dir, FALSE))) {
		msg_to_char(ch, "You can't go that way!\r\n");
	}
	else if (!WATER_SECT(to_room) && !WATER_SECT(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only swim in the water.\r\n");
	}
	else {
		perform_move(ch, dir, NULL, MOVE_SWIM);
	}
}


ACMD(do_transport) {
	char arg[MAX_INPUT_LENGTH];
	room_data *target = NULL;
	
	one_word(argument, arg);
	
	if (!IS_APPROVED(ch) && config_get_bool("travel_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_START_LOCATION)) {
		msg_to_char(ch, "You need to be at a starting location to transport!\r\n");
	}
	else if (*arg && !(target = parse_room_from_coords(argument)) && !(target = find_target_room(ch, arg))) {
		skip_spaces(&argument);
		msg_to_char(ch, "You can't transport to '%s'\r\n", argument);
	}
	else if (target == IN_ROOM(ch)) {
		msg_to_char(ch, "It seems you've arrived before you even left!\r\n");
	}
	else if (target && !ROOM_SECT_FLAGGED(target, SECTF_START_LOCATION)) {
		msg_to_char(ch, "You can only transport to other starting locations.\r\n");
	}
	else if (target && !can_use_room(ch, target, GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to transport there.\r\n");
	}
	else {
		perform_transport(ch, target ? target : find_other_starting_location(IN_ROOM(ch)));
	}
}


ACMD(do_wake) {
	char_data *vict;
	int self = 0;

	one_argument(argument, arg);
	if (*arg) {
		if (GET_POS(ch) == POS_SLEEPING)
			send_to_char("Maybe you should wake yourself up first.\r\n", ch);
		else if ((vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)) == NULL)
			send_config_msg(ch, "no_person");
		else if (vict == ch)
			self = 1;
		else if (AWAKE(vict))
			act("$E is already awake.", FALSE, ch, 0, vict, TO_CHAR);
		else if (GET_POS(vict) < POS_SLEEPING)
			act("$E's in pretty bad shape!", FALSE, ch, 0, vict, TO_CHAR);
		else {
			act("You wake $M up.", FALSE, ch, 0, vict, TO_CHAR);
			act("You are awakened by $n.", FALSE, ch, 0, vict, TO_VICT | TO_SLEEP);
			GET_POS(vict) = POS_SITTING;
		}
		if (!self)
			return;
	}
	if (GET_POS(ch) > POS_SLEEPING)
		send_to_char("You are already awake...\r\n", ch);
	else {
		send_to_char("You awaken, and sit up.\r\n", ch);
		act("$n awakens.", TRUE, ch, 0, 0, TO_ROOM);
		GET_POS(ch) = POS_SITTING;
	}
}
