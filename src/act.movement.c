/* ************************************************************************
*   File: act.movement.c                                  EmpireMUD 2.0b5 *
*  Usage: movement commands, door handling, & sleep/rest/etc state        *
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
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Helpers
*   Move Validators
*   Move System
*   Commands
*/

// external vars
extern const struct action_data_struct action_data[];
extern const char *dirs[];
extern const int rev_dir[];
extern const char *from_dir[];
extern const char *mob_move_types[];

// external funcs
void do_unseat_from_vehicle(char_data *ch);

// local protos
bool can_enter_room(char_data *ch, room_data *room);
bool validate_vehicle_move(char_data *ch, vehicle_data *veh, room_data *to_room);


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


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* @param char_data *ch the person leaving tracks
* @param room_data *room the location of the tracks
* @param byte dir the direction the person went
*/
void add_tracks(char_data *ch, room_data *room, byte dir) {
	extern bool valid_no_trace(room_data *room);
	extern bool valid_unseen_passing(room_data *room);
	
	struct track_data *track;
	
	if (!IS_IMMORTAL(ch) && !ROOM_SECT_FLAGGED(room, SECTF_FRESH_WATER | SECTF_FRESH_WATER)) {
		if (!IS_NPC(ch) && has_ability(ch, ABIL_NO_TRACE) && valid_no_trace(room)) {
			gain_ability_exp(ch, ABIL_NO_TRACE, 5);
		}
		else if (!IS_NPC(ch) && has_ability(ch, ABIL_UNSEEN_PASSING) && valid_unseen_passing(room)) {
			gain_ability_exp(ch, ABIL_UNSEEN_PASSING, 5);
		}
		else {
			CREATE(track, struct track_data, 1);
		
			track->timestamp = time(0);
			track->dir = dir;
		
			if (IS_NPC(ch)) {
				track->mob_num = GET_MOB_VNUM(ch);
				track->player_id = NOTHING;
			}
			else {
				track->mob_num = NOTHING;
				track->player_id = GET_IDNUM(ch);
			}
			
			track->next = ROOM_TRACKS(room);
			ROOM_TRACKS(room) = track;
		}
	}
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
	extern bool can_infiltrate(char_data *ch, empire_data *emp);
	
	room_data *to_room;
	bool ok = FALSE;
	
	// easy checks
	if (!IS_APPROVED(ch) && config_get_bool("travel_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (AFF_FLAGGED(ch, AFF_ENTANGLED)) {
		msg_to_char(ch, "You are entangled and can't enter anything.\r\n");
	}
	else if (AFF_FLAGGED(ch, AFF_CHARM) && ch->master && IN_ROOM(ch) == IN_ROOM(ch->master)) {
		msg_to_char(ch, "The thought of leaving your master makes you weep.\r\n");
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
			act("$V can't be led through a portal.", FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_CHAR);
			return FALSE;
		}
		if (GET_ROOM_VEHICLE(to_room) && (VEH_FLAGGED(GET_LEADING_VEHICLE(ch), VEH_NO_LOAD_ONTO_VEHICLE) || !VEH_FLAGGED(GET_ROOM_VEHICLE(to_room), VEH_CARRY_VEHICLES))) {
			act("$V can't be led into $v.", FALSE, ch, GET_ROOM_VEHICLE(to_room), GET_LEADING_VEHICLE(ch), TO_CHAR | ACT_VEHICLE_OBJ);
			return FALSE;
		}
		if (!validate_vehicle_move(ch, GET_LEADING_VEHICLE(ch), to_room)) {
			// sends own message
			return FALSE;
		}
	}
	
	// permissions
	if (!skip_permissions && ROOM_OWNER(IN_ROOM(ch)) && !IS_IMMORTAL(ch) && !IS_NPC(ch) && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || !can_use_room(ch, to_room, GUESTS_ALLOWED))) {
		if (!allow_infiltrate || !has_ability(ch, ABIL_INFILTRATE)) {
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
	extern bool can_enter_instance(char_data *ch, struct instance_data *inst);
	extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom);
	
	struct instance_data *inst;
	
	// always
	if (IS_IMMORTAL(ch) || IS_NPC(ch)) {
		return TRUE;
	}
	
	// player limit
	if (IS_ADVENTURE_ROOM(room) && (inst = find_instance_by_room(room, FALSE))) {
		// only if not already in there
		if (!IS_ADVENTURE_ROOM(IN_ROOM(ch)) || find_instance_by_room(IN_ROOM(ch), FALSE) != inst) {
			if (!can_enter_instance(ch, inst)) {
				return FALSE;
			}
		}
	}
	
	return TRUE;
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
		act(lbuf, FALSE, ch, obj, obj ? NULL : ex->keyword, TO_ROOM);
	}

	/* Notify the other room */
	if (other_room && back) {
		sprintf(lbuf, "The %s is %s%s from the other side.", (back->keyword ? fname(back->keyword) : "door"), cmd_door[scmd], (scmd == SCMD_CLOSE) ? "d" : "ed");
		if (ROOM_PEOPLE(other_room)) {
			act(buf, FALSE, ROOM_PEOPLE(other_room), 0, 0, TO_ROOM);
			act(buf, FALSE, ROOM_PEOPLE(other_room), 0, 0, TO_CHAR);
		}
	}
}


/**
* Various ability exp gain based on any kind of move.
*
* @param char_data *ch The person moving.
* @param room_data *was_in The room they were in before (if applicable; may be NULL).
* @param int mode MOVE_NORMAL, etc.
*/
void gain_ability_exp_from_moves(char_data *ch, room_data *was_in, int mode) {
	if (IS_NPC(ch)) {
		return;
	}
	
	if (mode == MOVE_SWIM) {
		gain_ability_exp(ch, ABIL_SWIMMING, 1);
	}
	
	if (IS_RIDING(ch)) {
		gain_ability_exp(ch, ABIL_RIDE, 1);
		
		if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_ROUGH)) {
			gain_ability_exp(ch, ABIL_ALL_TERRAIN_RIDING, 5);
		}
	}
	else {	// not riding
		if (was_in && ROOM_SECT_FLAGGED(was_in, SECTF_ROUGH) && ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_ROUGH)) {
			gain_ability_exp(ch, ABIL_MOUNTAIN_CLIMBING, 10);
		}
		gain_ability_exp(ch, ABIL_NAVIGATION, 1);
		
		if (AFF_FLAGGED(ch, AFF_FLY)) {
			gain_ability_exp(ch, ABIL_FLY, 1);
		}
	}
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
	
	LL_FOREACH2(ROOM_CONTENTS(in_room), objiter, next_content) {
		if (GET_PORTAL_TARGET_VNUM(objiter) == GET_ROOM_VNUM(from_room)) {
			return objiter;
		}
	}
	
	// otherwise just use the original one
	return fallback;
}


// processes the character's north
int get_north_for_char(char_data *ch) {
	if (IS_NPC(ch) || (has_ability(ch, ABIL_NAVIGATION) && !IS_DRUNK(ch))) {
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
		else if (has_ability(ch, ABIL_PORTAL_MAGIC)) {
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
	look_at_room(ch);
	if (!IS_NPC(ch)) {
		GET_LAST_DIR(ch) = NO_DIR;
	}

	act("$n materializes in front of you!", TRUE, ch, 0, 0, TO_ROOM);
	
	enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
	entry_memory_mtrigger(ch);
	greet_mtrigger(ch, NO_DIR);
	greet_memory_mtrigger(ch);
	greet_vtrigger(ch, NO_DIR);

	for (k = ch->followers; k; k = k->next) {
		if ((IN_ROOM(k->follower) == was_in) && (GET_POS(k->follower) >= POS_STANDING)) {
			act("You follow $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
			perform_transport(k->follower, to_room);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MOVE VALIDATORS /////////////////////////////////////////////////////////

/**
* This determines if a PLAYER can move somewhere. Mobs do not hit this check.
*
* @param char_data *ch The player moving.
* @param int dir A real dir, not a confused dir.
* @param room_data *to_room The target room.
* @param int need_specials_check If TRUE, the player is following someone.
* @return int 0 for fail, 1 for success
*/
int can_move(char_data *ch, int dir, room_data *to_room, int need_specials_check) {
	ACMD(do_dismount);
	
	if (WATER_SECT(to_room) && !EFFECTIVELY_SWIMMING(ch)) {
		send_to_char("You don't know how to swim.\r\n", ch);
		return 0;
	}
	// water->mountain
	if (!PLR_FLAGGED(ch, PLR_UNRESTRICT) && WATER_SECT(IN_ROOM(ch))) {
		if (ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !EFFECTIVELY_FLYING(ch)) {
			send_to_char("You are unable to scale the cliff.\r\n", ch);
			return 0;
		}
	}
	if (!PLR_FLAGGED(ch, PLR_UNRESTRICT) && IS_MAP_BUILDING(to_room) && !IS_INSIDE(IN_ROOM(ch)) && !IS_ADVENTURE_ROOM(IN_ROOM(ch)) && BUILDING_ENTRANCE(to_room) != dir && ROOM_IS_CLOSED(to_room) && (!ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES) || BUILDING_ENTRANCE(to_room) != rev_dir[dir])) {
		if (ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES)) {
			msg_to_char(ch, "You can't enter it from this side. The entrances are from %s and %s.\r\n", from_dir[get_direction_for_char(ch, BUILDING_ENTRANCE(to_room))], from_dir[get_direction_for_char(ch, rev_dir[BUILDING_ENTRANCE(to_room)])]);
		}
		else {
			msg_to_char(ch, "You can't enter it from this side. The entrance is from %s.\r\n", from_dir[get_direction_for_char(ch, BUILDING_ENTRANCE(to_room))]);
		}
		return 0;
	}
	if (!IS_IMMORTAL(ch) && !PLR_FLAGGED(ch, PLR_UNRESTRICT) && !IS_INSIDE(IN_ROOM(ch)) && !ROOM_IS_CLOSED(IN_ROOM(ch)) && !IS_ADVENTURE_ROOM(IN_ROOM(ch)) && IS_ANY_BUILDING(to_room) && !can_use_room(ch, to_room, GUESTS_ALLOWED) && ROOM_IS_CLOSED(to_room) && (!need_specials_check || (ch->master && !IS_NPC(ch) && IS_NPC(ch->master)))) {
		msg_to_char(ch, "You can't enter a building without permission.\r\n");
		return 0;
	}
	if (IS_RIDING(ch) && ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !has_ability(ch, ABIL_ALL_TERRAIN_RIDING) && !EFFECTIVELY_FLYING(ch)) {
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "You can't ride on such rough terrain.\r\n");
			return 0;
		}
	}
	if (IS_RIDING(ch) && DEEP_WATER_SECT(to_room) && !MOUNT_FLAGGED(ch, MOUNT_AQUATIC) && !EFFECTIVELY_FLYING(ch)) {
		// ATR does not help ocean
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "Your mount won't ride into the ocean.\r\n");
			return 0;
		}
	}
	if (IS_RIDING(ch) && !has_ability(ch, ABIL_ALL_TERRAIN_RIDING) && WATER_SECT(to_room) && !MOUNT_FLAGGED(ch, MOUNT_AQUATIC) && !EFFECTIVELY_FLYING(ch)) {
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "Your mount won't ride into the water.\r\n");
			return 0;
		}
	}
	if (IS_RIDING(ch) && MOUNT_FLAGGED(ch, MOUNT_AQUATIC) && !WATER_SECT(to_room) && !IS_WATER_BUILDING(to_room) && !EFFECTIVELY_FLYING(ch)) {
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "Your mount won't go onto the land.\r\n");
			return 0;
		}
	}
	
	// need a generic for this -- a nearly identical condition is used in do_mount
	if (IS_RIDING(ch) && IS_COMPLETE(to_room) && !BLD_ALLOWS_MOUNTS(to_room)) {
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "You can't ride indoors.\r\n");
			return 0;
		}
	}
	
	if (MOB_FLAGGED(ch, MOB_AQUATIC) && !WATER_SECT(to_room) && !EFFECTIVELY_FLYING(ch)) {
		msg_to_char(ch, "You can't go on land!\r\n");
		return 0;
	}
	
	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_ROUGH) && ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && (!IS_NPC(ch) || !MOB_FLAGGED(ch, MOB_MOUNTAINWALK)) && (IS_NPC(ch) || !IS_RIDING(ch) || !has_ability(ch, ABIL_ALL_TERRAIN_RIDING)) && !has_ability(ch, ABIL_MOUNTAIN_CLIMBING) && !EFFECTIVELY_FLYING(ch)) {
		msg_to_char(ch, "You must buy the Mountain Climbing ability to cross such rough terrain.\r\n");
		return 0;
	}
	
	if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_FLY) && EFFECTIVELY_FLYING(ch)) {
		msg_to_char(ch, "You can't fly there.\r\n");
		return 0;
	}
	
	if (GET_LEADING_VEHICLE(ch) && IN_ROOM(GET_LEADING_VEHICLE(ch)) == IN_ROOM(ch) && !validate_vehicle_move(ch, GET_LEADING_VEHICLE(ch), to_room)) {
		// sends own messages
		return 0;
	}
	if (GET_LEADING_MOB(ch) && !GET_LEADING_MOB(ch)->desc && IN_ROOM(GET_LEADING_MOB(ch)) == IN_ROOM(ch) && !can_move(GET_LEADING_MOB(ch), dir, to_room, TRUE)) {
		act("You can't go there while leading $N.", FALSE, ch, NULL, GET_LEADING_MOB(ch), TO_CHAR);
		return 0;
	}
	
	return 1;
}


/**
* @param char_data *ch
* @param room_data *from Origin room
* @param room_data *to Destination room
* @param int dir Which direction is being moved
* @param int mode MOVE_NORMAL, etc
* @return int the move cost
*/
int move_cost(char_data *ch, room_data *from, room_data *to, int dir, int mode) {
	double need_movement, cost_from, cost_to;
	
	/* move points needed is avg. move loss for src and destination sect type */	
	
	// open buildings and incomplete non-closed buildings use average of current & original sect's move cost
	if (!ROOM_IS_CLOSED(from)) {
		cost_from = (GET_SECT_MOVE_LOSS(SECT(from)) + GET_SECT_MOVE_LOSS(BASE_SECT(from))) / 2.0;
	}
	else {
		cost_from = GET_SECT_MOVE_LOSS(SECT(from));
	}
	
	// cost for the space moving to
	if (!ROOM_IS_CLOSED(to)) {
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
	
	if (IS_RIDING(ch) || EFFECTIVELY_FLYING(ch) || mode == MOVE_EARTHMELD) {
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
	extern int count_harnessed_animals(vehicle_data *veh);

	char buf[MAX_STRING_LENGTH];
	
	if (!VEH_IS_COMPLETE(veh)) {
		if (ch) {
			act("$V can't move anywhere until it's complete!", FALSE, ch, NULL, veh, TO_CHAR);
		}
		return FALSE;
	}
	
	// required number of mounts
	if (count_harnessed_animals(veh) < VEH_ANIMALS_REQUIRED(veh)) {
		if (ch) {
			snprintf(buf, sizeof(buf), "You need to harness %d animal%s to $V before it can move.", VEH_ANIMALS_REQUIRED(veh), PLURAL(VEH_ANIMALS_REQUIRED(veh)));
			act(buf, FALSE, ch, NULL, veh, TO_CHAR);
		}
		return FALSE;
	}
	
	// closed building?
	if ((VEH_FLAGGED(veh, VEH_NO_BUILDING) || !BLD_ALLOWS_MOUNTS(to_room)) && !IS_INSIDE(IN_ROOM(veh)) && !ROOM_IS_CLOSED(IN_ROOM(veh)) && !IS_ADVENTURE_ROOM(IN_ROOM(veh)) && IS_ANY_BUILDING(to_room) && ROOM_IS_CLOSED(to_room)) {
		if (ch) {
			act("$V can't go in there.", FALSE, ch, NULL, veh, TO_CHAR);
		}
		return FALSE;
	}
	
	// barrier?
	if (ROOM_BLD_FLAGGED(to_room, BLD_BARRIER)) {
		if (ch) {
			act("$V can't move that close to the barrier.", FALSE, ch, NULL, veh, TO_CHAR);
		}
		return FALSE;
	}
	
	// terrain-based checks
	if (WATER_SECT(to_room) && !VEH_FLAGGED(veh, VEH_SAILING | VEH_FLYING)) {
		if (ch) {
			act("$V can't go onto the water.", FALSE, ch, NULL, veh, TO_CHAR);
		}
		return FALSE;
	}
	if (!WATER_SECT(to_room) && !IS_WATER_BUILDING(to_room) && !WATER_SECT(IN_ROOM(veh)) && !VEH_FLAGGED(veh, VEH_DRIVING | VEH_FLYING)) {
		if (ch) {
			act("$V can't go onto land.", FALSE, ch, NULL, veh, TO_CHAR);
		}
		return FALSE;
	}
	if (ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !VEH_FLAGGED(veh, VEH_ALLOW_ROUGH | VEH_FLYING)) {
		if (ch) {
			act("$V can't go into rough terrain.", FALSE, ch, NULL, veh, TO_CHAR);
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
	void trigger_distrust_from_stealth(char_data *ch, empire_data *emp);
	
	obj_data *use_portal;
	struct follow_type *fol, *next_fol;
	room_data *to_room = real_room(GET_PORTAL_TARGET_VNUM(portal));
	room_data *was_in = IN_ROOM(ch);
	
	// safety
	if (!to_room) {
		return;
	}
	
	// cancel some actions on movement
	if (!IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE && !IS_SET(action_data[GET_ACTION(ch)].flags, ACTF_ANYWHERE) && GET_ACTION_ROOM(ch) != GET_ROOM_VNUM(IN_ROOM(ch)) && GET_ACTION_ROOM(ch) != NOWHERE) {
		cancel_action(ch);
	}
	
	act("You enter $p...", FALSE, ch, portal, 0, TO_CHAR);
	act("$n steps into $p!", TRUE, ch, portal, 0, TO_ROOM);
	
	// ch first
	char_from_room(ch);
	char_to_room(ch, to_room);
	
	// move them first, then move them back if they aren't allowed to go.
	// see if an entry trigger disallows the move
	if (!entry_mtrigger(ch) || !enter_wtrigger(IN_ROOM(ch), ch, NO_DIR)) {
		char_from_room(ch);
		char_to_room(ch, was_in);
		return;
	}
	
	// update visit and last-dir
	qt_visit_room(ch, to_room);
	if (!IS_NPC(ch)) {
		GET_LAST_DIR(ch) = NO_DIR;
	}
	
	// see if there's a different portal on the other end
	use_portal = find_back_portal(to_room, was_in, portal);
	
	act("$n appears from $p!", TRUE, ch, use_portal, 0, TO_ROOM);
	look_at_room(ch);
	command_lag(ch, WAIT_MOVEMENT);
	give_portal_sickness(ch, portal, was_in, to_room);
	
	// leading vehicle (movement validated by can_move in do_simple_move)
	if (GET_LEADING_VEHICLE(ch) && IN_ROOM(GET_LEADING_VEHICLE(ch)) == was_in) {
		if (ROOM_PEOPLE(was_in)) {
			snprintf(buf, sizeof(buf), "$V %s through $p.", mob_move_types[VEH_MOVE_TYPE(GET_LEADING_VEHICLE(ch))]);
			act(buf, FALSE, ROOM_PEOPLE(was_in), portal, GET_LEADING_VEHICLE(ch), TO_CHAR | TO_ROOM);
		}
		vehicle_to_room(GET_LEADING_VEHICLE(ch), IN_ROOM(ch));
		snprintf(buf, sizeof(buf), "$V %s in through $p.", mob_move_types[VEH_MOVE_TYPE(GET_LEADING_VEHICLE(ch))]);
		act(buf, FALSE, ch, use_portal, GET_LEADING_VEHICLE(ch), TO_CHAR | TO_ROOM);
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
	}
	
	// trigger section
	entry_memory_mtrigger(ch);
	if (!greet_mtrigger(ch, NO_DIR) || !greet_vtrigger(ch, NO_DIR)) {
		char_from_room(ch);
		char_to_room(ch, was_in);
		look_at_room(ch);
	}
	else {
		greet_memory_mtrigger(ch);
	}
}


/* do_simple_move assumes
*    1. That there is no master and no followers.
*    2. That the direction exists.
*
* @return bool TRUE on success, FALSE on failure
*/
bool do_simple_move(char_data *ch, int dir, room_data *to_room, int need_specials_check, byte mode) {
	char lbuf[MAX_STRING_LENGTH];
	room_data *was_in = IN_ROOM(ch), *from_room;
	int need_movement, move_type, reverse = (IS_NPC(ch) || GET_LAST_DIR(ch) == NO_DIR) ? NORTH : rev_dir[(int) GET_LAST_DIR(ch)];
	char_data *animal = NULL, *vict;
	
	if (!IS_IMMORTAL(ch) && !IS_NPC(ch) && IS_CARRYING_N(ch) > CAN_CARRY_N(ch)) {
		msg_to_char(ch, "You are overburdened and cannot move.\r\n");
		return FALSE;
	}
	
	if (IS_INJURED(ch, INJ_TIED)) {
		msg_to_char(ch, "You can't seem to move!\r\n");
		return FALSE;
	}
	
	if (MOB_FLAGGED(ch, MOB_TIED)) {
		msg_to_char(ch, "You are tied in place.\r\n");
		return FALSE;
	}

	/* blocked by a leave trigger ? */
	if (!leave_mtrigger(ch, dir) || IN_ROOM(ch) != was_in) {
		/* prevent teleport crashes */
		return FALSE;
	}
	if (!leave_wtrigger(IN_ROOM(ch), ch, dir) || IN_ROOM(ch) != was_in) {
		/* prevent teleport crashes */
		return FALSE;
	}
	if (!leave_vtrigger(ch, dir) || IN_ROOM(ch) != was_in) {
		/* prevent teleport crashes */
		return FALSE;
	}
	if (!leave_otrigger(IN_ROOM(ch), ch, dir) || IN_ROOM(ch) != was_in) {
		/* prevent teleport crashes */
		return FALSE;
	}

	/* charmed? */
	if (AFF_FLAGGED(ch, AFF_CHARM) && ch->master && IN_ROOM(ch) == IN_ROOM(ch->master)) {
		send_to_char("The thought of leaving your master makes you weep.\r\n", ch);
		act("$n bursts into tears.", FALSE, ch, 0, 0, TO_ROOM);
		return FALSE;
	}

	if (GET_FEEDING_FROM(ch) || GET_FED_ON_BY(ch)) {
		msg_to_char(ch, "You can't seem to move!\r\n");
		return FALSE;
	}

	// if the room we're going to is water, check for ability to move
	if (WATER_SECT(to_room)) {
		if (!EFFECTIVELY_FLYING(ch) && !IS_RIDING(ch)) {
			if (has_ability(ch, ABIL_SWIMMING) && (mode == MOVE_NORMAL || mode == MOVE_FOLLOW)) {
				mode = MOVE_SWIM;
			}
			else if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_AQUATIC)) {
				mode = MOVE_SWIM;
			}
		}
	}

	// move into a barrier at all?
	if (mode != MOVE_EARTHMELD && !REAL_NPC(ch) && (ROOM_OWNER(to_room) && GET_LOYALTY(ch) != ROOM_OWNER(to_room)) && !PLR_FLAGGED(ch, PLR_UNRESTRICT) && ROOM_BLD_FLAGGED(to_room, BLD_BARRIER) && IS_COMPLETE(to_room) && !EFFECTIVELY_FLYING(ch)) {
		msg_to_char(ch, "There is a barrier in your way.\r\n");
		return FALSE;
	}

	// wall-block: can only go back last-direction
	if (mode != MOVE_EARTHMELD && !REAL_NPC(ch) && !PLR_FLAGGED(ch, PLR_UNRESTRICT) && !EFFECTIVELY_FLYING(ch)) {
		if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_BARRIER) && IS_COMPLETE(IN_ROOM(ch)) && GET_LAST_DIR(ch) != NO_DIR && dir != reverse) {
			from_room = real_shift(IN_ROOM(ch), shift_dir[reverse][0], shift_dir[reverse][1]);
			
			// ok, we know we're on a wall and trying to go the wrong way. But does the next tile also have this issue?
			if (from_room && (!ROOM_BLD_FLAGGED(from_room, BLD_BARRIER) || !IS_COMPLETE(from_room) || !can_use_room(ch, from_room, MEMBERS_ONLY))) {
				msg_to_char(ch, "You are unable to pass. You can only go back %s.\r\n", dirs[get_direction_for_char(ch, reverse)]);
				return FALSE;
			}
		}
	}
	
	// unfinished tunnel
	if ((BUILDING_VNUM(IN_ROOM(ch)) == BUILDING_TUNNEL || BUILDING_VNUM(IN_ROOM(ch)) == RTYPE_TUNNEL) && !IS_COMPLETE(IN_ROOM(ch)) && mode != MOVE_EARTHMELD && !REAL_NPC(ch) && !PLR_FLAGGED(ch, PLR_UNRESTRICT) && dir != rev_dir[(int) GET_LAST_DIR(ch)]) {
		msg_to_char(ch, "The tunnel is incomplete. You can only go back %s.\r\n", dirs[get_direction_for_char(ch, rev_dir[(int) GET_LAST_DIR(ch)])]);
		return FALSE;
	}
	
	if (!REAL_NPC(ch) && mode != MOVE_EARTHMELD && !can_move(ch, dir, to_room, need_specials_check))
		return FALSE;

	/* move points needed is avg. move loss for src and destination sect type */
	need_movement = move_cost(ch, IN_ROOM(ch), to_room, dir, mode);

	if (GET_MOVE(ch) < need_movement && !IS_IMMORTAL(ch) && !IS_NPC(ch)) {
		if (need_specials_check && ch->master)
			send_to_char("You are too exhausted to follow.\r\n", ch);
		else
			send_to_char("You are too exhausted.\r\n", ch);

		return FALSE;
	}

	if (GET_LED_BY(ch) && IS_MAP_BUILDING(to_room) && !BLD_ALLOWS_MOUNTS(to_room))
		return FALSE;
		
	if (AFF_FLAGGED(ch, AFF_ENTANGLED)) {
		msg_to_char(ch, "You are entangled and can't move.\r\n");
		return FALSE;
	}

	if (MOB_FLAGGED(ch, MOB_TIED)) {
		msg_to_char(ch, "You're tied up.\r\n");
		return FALSE;
	}
	
	// checks instance limits, etc
	if (!can_enter_room(ch, to_room)) {
		msg_to_char(ch, "You can't seem to go there. Perhaps it's full.\r\n");
		return FALSE;
	}

	/* Now we know we're allowed to go into the room. */
	if (!IS_IMMORTAL(ch) && !IS_NPC(ch))
		GET_MOVE(ch) -= need_movement;

	REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE);
	
	// determine real move type
	move_type = IS_NPC(ch) ? MOB_MOVE_TYPE(ch) : MOB_MOVE_WALK;
	if (IS_MORPHED(ch)) {
		move_type = MORPH_MOVE_TYPE(GET_MORPH(ch));
	}
	
	if (AFF_FLAGGED(ch, AFF_FLY) && !IS_MORPHED(ch) && !IS_NPC(ch)) {
		move_type = MOB_MOVE_FLY;
	}
	else if (IS_RIDING(ch)) {
		move_type = MOB_MOVE_RIDE;
	}
	else if (move_type == MOB_MOVE_WALK && ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN) && move_type != MOB_MOVE_FLY) {
		move_type = MOB_MOVE_SWIM;
	}
	else if (move_type == MOB_MOVE_WALK && ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_ROUGH) && move_type != MOB_MOVE_FLY) {
		move_type = MOB_MOVE_CLIMB;
	}

	// leaving message
	if (!AFF_FLAGGED(ch, AFF_SNEAK | AFF_NO_SEE_IN_ROOM)) {
		*buf2 = '\0';
		
		switch (mode) {
			case MOVE_LEAD:
				sprintf(buf2, "%s leads $n with %s.", HSSH(mode == MOVE_LEAD ? GET_LED_BY(ch) : ch->master), HMHR(mode == MOVE_LEAD ? GET_LED_BY(ch) : ch->master));
				break;
			case MOVE_FOLLOW:
				sprintf(buf2, "$n follows %s %%s.", HMHR(mode == MOVE_LEAD ? GET_LED_BY(ch) : ch->master));
				break;
			case MOVE_EARTHMELD:
				break;
			default: {
				// this leaves a %s for later
				sprintf(buf2, "$n %s %%s.", mob_move_types[move_type]);
				break;
			}
		}
		if (*buf2 && (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_SILENT) || number(0, 4))) {
			for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = vict->next_in_room) {
				if (vict == ch || !vict->desc) {
					continue;
				}
				if (!CAN_SEE(vict, ch) || !WIZHIDE_OK(vict, ch)) {
					continue;
				}

				// adjust direction
				if (strstr(buf2, "%s") != NULL) {
					sprintf(lbuf, buf2, dirs[get_direction_for_char(vict, dir)]);
				}
				else {
					strcpy(lbuf, buf2);
				}
				
				act(lbuf, TRUE, ch, NULL, vict, TO_VICT);
			}
		}
	}

	// mark it
	add_tracks(ch, IN_ROOM(ch), dir);
	mark_move_time(ch);

	char_from_room(ch);
	char_to_room(ch, to_room);

	/* move them first, then move them back if they aren't allowed to go. */
	/* see if an entry trigger disallows the move */
	if (!entry_mtrigger(ch) || !enter_wtrigger(IN_ROOM(ch), ch, dir)) {
		char_from_room(ch);
		char_to_room(ch, was_in);
		return FALSE;
	}

	if (!IS_NPC(ch)) {
		GET_LAST_DIR(ch) = dir;
	}

	// cancel some actions on movement
	if (!IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE && !IS_SET(action_data[GET_ACTION(ch)].flags, ACTF_ANYWHERE) && GET_ACTION_ROOM(ch) != GET_ROOM_VNUM(IN_ROOM(ch)) && GET_ACTION_ROOM(ch) != NOWHERE) {
		cancel_action(ch);
	}
	
	command_lag(ch, WAIT_MOVEMENT);

	if (animal) {
		char_from_room(animal);
		char_to_room(animal, to_room);
	}
	
	// walks-in messages
	if (!AFF_FLAGGED(ch, AFF_SNEAK | AFF_NO_SEE_IN_ROOM)) {
		switch (mode) {
			case MOVE_LEAD:
				act("$E leads $n behind $M.", TRUE, ch, 0, GET_LED_BY(ch), TO_NOTVICT);
				act("You lead $n behind you.", TRUE, ch, 0, GET_LED_BY(ch), TO_VICT);
				break;
			case MOVE_FOLLOW:
				act("$n follows $N.", TRUE, ch, NULL, ch->master, TO_NOTVICT);
				act("$n follows you.", TRUE, ch, 0, ch->master, TO_VICT);
				break;
			case MOVE_EARTHMELD:
				break;
			default: {
				// this leaves a %s for later
				sprintf(buf2, "$n %s %s from %%s.", mob_move_types[move_type], (ROOM_IS_CLOSED(IN_ROOM(ch)) ? "in" : "up"));

				for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = vict->next_in_room) {
					if (vict == ch || !vict->desc) {
						continue;
					}
					if (!CAN_SEE(vict, ch) || !WIZHIDE_OK(vict, ch)) {
						continue;
					}
					
					// adjust direction
					if (strstr(buf2, "%s")) {
						sprintf(lbuf, buf2, from_dir[get_direction_for_char(vict, dir)]);
					}
					else {
						strcpy(lbuf, buf);
					}

					act(lbuf, TRUE, ch, NULL, vict, TO_VICT);
				}
				break;
			}
		}
	}
	
	qt_visit_room(ch, IN_ROOM(ch));

	if (ch->desc != NULL) {
		look_at_room(ch);
	}
	if (animal && animal->desc != NULL) {
		look_at_room(animal);
	}

	gain_ability_exp_from_moves(ch, was_in, mode);
	
	// trigger section
	entry_memory_mtrigger(ch);
	if (!greet_mtrigger(ch, dir) || !greet_vtrigger(ch, dir)) {
		char_from_room(ch);
		char_to_room(ch, was_in);
		look_at_room(ch);
	}
	else {
		greet_memory_mtrigger(ch);
	}
	
	return TRUE;
}


int perform_move(char_data *ch, int dir, int need_specials_check, byte mode) {
	room_data *was_in, *to_room = IN_ROOM(ch);
	struct room_direction_data *ex;
	struct follow_type *k, *next;
	char buf[MAX_STRING_LENGTH];

	if (ch == NULL)
		return FALSE;

	if ((PLR_FLAGGED(ch, PLR_UNRESTRICT) && !IS_ADVENTURE_ROOM(IN_ROOM(ch)) && !IS_INSIDE(IN_ROOM(ch))) || !ROOM_IS_CLOSED(IN_ROOM(ch))) {
		if (dir >= NUM_2D_DIRS || dir < 0) {
			send_to_char("Alas, you cannot go that way...\r\n", ch);
			return FALSE;
		}
		// may produce a NULL
		to_room = real_shift(IN_ROOM(ch), shift_dir[dir][0], shift_dir[dir][1]);
	}
	else {
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
	
	// safety (and map bounds)
	if (!to_room) {
		msg_to_char(ch, "Alas, you cannot go that way...\r\n");
		return FALSE;
	}

	/* Store old room */
	was_in = IN_ROOM(ch);

	if (!do_simple_move(ch, dir, to_room, need_specials_check, mode))
		return FALSE;
	
	// leading vehicle (movement validated by can_move in do_simple_move)
	if (GET_LEADING_VEHICLE(ch) && IN_ROOM(GET_LEADING_VEHICLE(ch)) == was_in) {
		if (ROOM_PEOPLE(was_in)) {
			snprintf(buf, sizeof(buf), "$v %s behind $N.", mob_move_types[VEH_MOVE_TYPE(GET_LEADING_VEHICLE(ch))]);
			act(buf, FALSE, ROOM_PEOPLE(was_in), GET_LEADING_VEHICLE(ch), ch, TO_CHAR | TO_ROOM | ACT_VEHICLE_OBJ);
		}
		vehicle_to_room(GET_LEADING_VEHICLE(ch), IN_ROOM(ch));
		snprintf(buf, sizeof(buf), "$V %s in behind you.", mob_move_types[VEH_MOVE_TYPE(GET_LEADING_VEHICLE(ch))]);
		act(buf, FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_CHAR);
		snprintf(buf, sizeof(buf), "$V %s in behind $n.", mob_move_types[VEH_MOVE_TYPE(GET_LEADING_VEHICLE(ch))]);
		act(buf, FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_ROOM);
	}
	// leading mob (attempt move)
	if (GET_LEADING_MOB(ch) && IN_ROOM(GET_LEADING_MOB(ch)) == was_in) {
		perform_move(GET_LEADING_MOB(ch), dir, TRUE, MOVE_LEAD);
	}

	for (k = ch->followers; k; k = next) {
		next = k->next;
		if ((IN_ROOM(k->follower) == was_in) && (GET_POS(k->follower) >= POS_STANDING)) {
			act("You follow $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
			perform_move(k->follower, dir, TRUE, MOVE_FOLLOW);
		}
	}
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////


ACMD(do_avoid) {
	char_data *vict;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Who would you like to avoid?\r\n");
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (vict == ch) {
		send_to_char("You can't avoid yourself, no matter how hard you try.\r\n", ch);
	}
	else if (vict->master != ch) {
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
	need_movement = move_cost(ch, IN_ROOM(ch), found_room, dir, MOVE_NORMAL);
	
	if (GET_MOVE(ch) < need_movement && !IS_IMMORTAL(ch) && !IS_NPC(ch)) {
		msg_to_char(ch, "You're too tired to circle that way.\r\n");
		return;
	}
	
	// cancel some actions on movement
	if (!IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE && !IS_SET(action_data[GET_ACTION(ch)].flags, ACTF_ANYWHERE) && GET_ACTION_ROOM(ch) != GET_ROOM_VNUM(IN_ROOM(ch)) && GET_ACTION_ROOM(ch) != NOWHERE) {
		cancel_action(ch);
	}
	
	// message
	msg_to_char(ch, "You circle %s.\r\n", dirs[get_direction_for_char(ch, dir)]);
	if (!AFF_FLAGGED(ch, AFF_SNEAK | AFF_NO_SEE_IN_ROOM)) {
		for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = vict->next_in_room) {
			if (vict != ch && vict->desc && CAN_SEE(vict, ch)) {
				sprintf(buf, "$n circles %s.", dirs[get_direction_for_char(vict, dir)]);
				act(buf, TRUE, ch, NULL, vict, TO_VICT);
			}
		}
	}

	// work
	if (!IS_IMMORTAL(ch) && !IS_NPC(ch)) {
		GET_MOVE(ch) -= need_movement;
	}
	mark_move_time(ch);
	char_from_room(ch);
	char_to_room(ch, found_room);
	qt_visit_room(ch, IN_ROOM(ch));
	
	if (!IS_NPC(ch)) {
		GET_LAST_DIR(ch) = dir;
	}
	
	if (ch->desc) {
		look_at_room(ch);
	}
	
	command_lag(ch, WAIT_MOVEMENT);
	
	// triggers?
	if (!enter_wtrigger(IN_ROOM(ch), ch, dir) || !greet_mtrigger(ch, dir) || !greet_vtrigger(ch, dir)) {
		char_from_room(ch);
		char_to_room(ch, was_in);
		if (ch->desc) {
			look_at_room(ch);
		}
		return;
	}
	entry_memory_mtrigger(ch);
	greet_memory_mtrigger(ch);
	
	gain_ability_exp_from_moves(ch, was_in, MOVE_CIRCLE);
	
	// message
	if (!AFF_FLAGGED(ch, AFF_SNEAK | AFF_NO_SEE_IN_ROOM)) {
		for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = vict->next_in_room) {
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


// enters a portal
ACMD(do_enter) {
	ACMD(do_board);
	extern bool can_infiltrate(char_data *ch, empire_data *emp);
	
	vehicle_data *find_veh;
	char_data *tmp_char;
	obj_data *portal;
	room_data *room;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Enter what?\r\n");
		return;
	}

	generic_find(arg, FIND_OBJ_ROOM | FIND_VEHICLE_ROOM, ch, &tmp_char, &portal, &find_veh);
	
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


ACMD(do_follow) {
	bool circle_follow(char_data *ch, char_data *victim);
	char_data *leader;

	one_argument(argument, buf);

	if (*buf) {
		if (!(leader = get_char_vis(ch, buf, FIND_CHAR_ROOM))) {
			send_config_msg(ch, "no_person");
			return;
		}
	}
	else {
		send_to_char("Whom do you wish to follow?\r\n", ch);
		return;
	}

	if (ch->master == leader) {
		act("You are already following $M.", FALSE, ch, 0, leader, TO_CHAR);
		return;
	}
	if (AFF_FLAGGED(ch, AFF_CHARM) && (ch->master))
		act("But you only feel like following $N!", FALSE, ch, 0, ch->master, TO_CHAR);
	else {			/* Not Charmed follow person */
		if (leader == ch) {
			if (!ch->master) {
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
			if (ch->master) {
				stop_follower(ch);
			}

			add_follower(ch, leader, TRUE);
		}
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
	if (!generic_find(type, FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &victim, &obj, &tmp_veh))
		door = find_door(ch, type, dir, cmd_door[subcmd]);
	
	ex = find_exit(IN_ROOM(ch), door);

	if ((obj) || (ex)) {
		if (!(DOOR_IS_OPENABLE(ch, obj, ex)))
			act("You can't $F that!", FALSE, ch, 0, cmd_door[subcmd], TO_CHAR);
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
	if (!AFF_FLAGGED(ch, AFF_FLY)) {
		msg_to_char(ch, "You aren't flying.\r\n");
		return;
	}

	affects_from_char_by_aff_flag(ch, AFF_FLY, FALSE);
	
	if (!AFF_FLAGGED(ch, AFF_FLY)) {
		msg_to_char(ch, "You land.\r\n");
		act("$n lands.", TRUE, ch, NULL, NULL, TO_ROOM);
	}
	else {
		msg_to_char(ch, "You can't seem to land. Perhaps whatever is causing your flight can't be ended.\r\n");
	}
	
	command_lag(ch, WAIT_OTHER);
}


ACMD(do_move) {
	extern int get_north_for_char(char_data *ch);
	extern const int confused_dirs[NUM_2D_DIRS][2][NUM_OF_DIRS];
	
	// this blocks normal moves but not flee
	if (is_fighting(ch)) {
		msg_to_char(ch, "You can't move while fighting!\r\n");
		return;
	}
	
	perform_move(ch, confused_dirs[get_north_for_char(ch)][0][subcmd], FALSE, MOVE_NORMAL);
}


// mortals have to portal from a certain building, immortals can do it anywhere
ACMD(do_portal) {
	void empire_skillup(empire_data *emp, any_vnum ability, double amount);
	extern char *get_room_name(room_data *room, bool color);
	
	bool all_access = ((IS_IMMORTAL(ch) && (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_TRANSFER))) || (IS_NPC(ch) && !AFF_FLAGGED(ch, AFF_CHARM)));
	char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH];
	room_data *room, *next_room, *target = NULL;
	obj_data *portal, *end, *obj;
	int bsize, lsize, count, num, dist;
	bool all = FALSE, wait_here = FALSE, wait_there = FALSE, ch_in_city;
	bool there_in_city;
	
	int max_out_of_city_portal = config_get_int("max_out_of_city_portal");
	
	argument = any_one_word(argument, arg);
	
	if (!str_cmp(arg, "-a") || !str_cmp(arg, "-all")) {
		all = TRUE;
	}
	
	// just show portals
	if (all || !*arg) {
		if (IS_NPC(ch) || !ch->desc) {
			msg_to_char(ch, "You can't list portals right now.\r\n");
			return;
		}
		if (!GET_LOYALTY(ch)) {
			msg_to_char(ch, "You can't get a portal list if you're not in an empire.\r\n");
			return;
		}
		
		bsize = snprintf(buf, sizeof(buf), "Known portals:\r\n");
		
		count = 0;
		ch_in_city = (is_in_city_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), TRUE, &wait_here) || (!ROOM_OWNER(IN_ROOM(ch)) && is_in_city_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), TRUE, &wait_here)));
		HASH_ITER(hh, world_table, room, next_room) {
			// early exit
			if (bsize >= sizeof(buf) - 1) {
				break;
			}
			
			dist = compute_distance(IN_ROOM(ch), room);
			there_in_city = is_in_city_for_empire(room, ROOM_OWNER(room), TRUE, &wait_there);
			
			if (ROOM_OWNER(room) && room_has_function_and_city_ok(room, FNC_PORTAL) && IS_COMPLETE(room) && can_use_room(ch, room, all ? GUESTS_ALLOWED : MEMBERS_AND_ALLIES) && (!all || (dist <= max_out_of_city_portal || (ch_in_city && there_in_city)))) {
				// only shows owned portals the character can use
				++count;
				*line = '\0';
				lsize = 0;
				
				// sequential numbering: only numbers them if it's not a -all
				if (!all) {
					lsize += snprintf(line + lsize, sizeof(line) - lsize, "%2d. ", count);
				}
				
				// coords: navigation only
				if (has_ability(ch, ABIL_NAVIGATION)) {
					lsize += snprintf(line + lsize, sizeof(line) - lsize, "(%*d, %*d) ", X_PRECISION, X_COORD(room), Y_PRECISION, Y_COORD(room));
				}
				
				lsize += snprintf(line + lsize, sizeof(line) - lsize, "%s (%s%s&0)", get_room_name(room, FALSE), EMPIRE_BANNER(ROOM_OWNER(room)), EMPIRE_ADJECTIVE(ROOM_OWNER(room)));
				
				if ((dist > max_out_of_city_portal && (!ch_in_city || !there_in_city)) || (!has_ability(ch, ABIL_PORTAL_MASTER) && (!GET_LOYALTY(ch) || !EMPIRE_HAS_TECH(GET_LOYALTY(ch), TECH_MASTER_PORTALS)) && GET_ISLAND_ID(IN_ROOM(ch)) != GET_ISLAND_ID(target))) {
					lsize += snprintf(line + lsize, sizeof(line) - lsize, " &r(too far)&0");
				}
				
				bsize += snprintf(buf + bsize, sizeof(buf) - bsize, "%s\r\n", line);
			}
		}
		
		// page it in case it's long
		page_string(ch->desc, buf, TRUE);
		command_lag(ch, WAIT_OTHER);
		return;
	}
	
	// targeting: by list number (only targets member/ally portals
	if (is_number(arg) && (num = atoi(arg)) >= 1 && GET_LOYALTY(ch)) {
		HASH_ITER(hh, world_table, room, next_room) {
			if (ROOM_OWNER(room) && room_has_function_and_city_ok(room, FNC_PORTAL) && IS_COMPLETE(room) && can_use_room(ch, room, MEMBERS_AND_ALLIES)) {
				if (--num <= 0) {
					target = room;
					break;
				}
			}
		}
	}
	
	// targeting: if we didn't get a result yet, try standard targeting
	if (!target && !(target = find_target_room(ch, arg))) {
		// sends own message
		return;
	}

	// ok, we have a target...
	if (!all_access && !has_ability(ch, ABIL_PORTAL_MAGIC)  && (!GET_LOYALTY(ch) || !EMPIRE_HAS_TECH(GET_LOYALTY(ch), TECH_PORTALS))) {
		msg_to_char(ch, "You can only open portals if there is a portal mage in your empire.\r\n");
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
	if (!all_access && (!HAS_FUNCTION(IN_ROOM(ch), FNC_PORTAL) || !HAS_FUNCTION(target, FNC_PORTAL) || !IS_COMPLETE(target) || !IS_COMPLETE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You can only open portals between portal buildings.\r\n");
		return;
	}
	if (!all_access && (!check_in_city_requirement(IN_ROOM(ch), TRUE) || !check_in_city_requirement(target, TRUE))) {
		msg_to_char(ch, "You can't open that portal because of one the locations must be in a city, and isn't.\r\n");
		return;
	}
	if (!all_access && !can_use_room(ch, target, GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to open a portal to that location.\r\n");
		return;
	}
	if (!has_ability(ch, ABIL_PORTAL_MASTER) && (!GET_LOYALTY(ch) || !EMPIRE_HAS_TECH(GET_LOYALTY(ch), TECH_MASTER_PORTALS)) && GET_ISLAND_ID(IN_ROOM(ch)) != GET_ISLAND_ID(target)) {
		msg_to_char(ch, "You can't open a portal to another land without a portal master in your empire.\r\n");
		return;
	}

	// check there's not already a portal to there from here
	for (obj = ROOM_CONTENTS(IN_ROOM(ch)); obj; obj = obj->next_content) {
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
	GET_OBJ_VAL(portal, VAL_PORTAL_TARGET_VNUM) = GET_ROOM_VNUM(target);
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
	GET_OBJ_VAL(end, VAL_PORTAL_TARGET_VNUM) = GET_ROOM_VNUM(IN_ROOM(ch));
	GET_OBJ_TIMER(end) = 5;
	obj_to_room(end, target);
	if (GET_ROOM_VNUM(IN_ROOM(end))) {
		act("$p spins open!", TRUE, ROOM_PEOPLE(IN_ROOM(end)), end, 0, TO_ROOM | TO_CHAR);
	}
	load_otrigger(end);
	
	if (GET_LOYALTY(ch)) {
		empire_skillup(GET_LOYALTY(ch), ABIL_PORTAL_MAGIC, 15);
		empire_skillup(GET_LOYALTY(ch), ABIL_PORTAL_MASTER, 15);
	}
	
	command_lag(ch, WAIT_OTHER);
}


ACMD(do_rest) {
	switch (GET_POS(ch)) {
		case POS_STANDING:
			if (IS_RIDING(ch)) {
				msg_to_char(ch, "You climb down from your mount.\r\n");
				perform_dismount(ch);
			}
			send_to_char("You sit down and rest your tired bones.\r\n", ch);
			act("$n sits down and rests.", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_RESTING;
			break;
		case POS_SITTING:
			do_unseat_from_vehicle(ch);
			send_to_char("You rest your tired bones on the ground.\r\n", ch);
			act("$n rests on the ground.", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_RESTING;
			break;
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
}


ACMD(do_sit) {
	one_argument(argument, arg);

	switch (GET_POS(ch)) {
		case POS_STANDING: {
			if (IS_RIDING(ch)) {
				msg_to_char(ch, "You can't do any more sitting while mounted.\r\n");
			}
			else if (!*arg) {
				send_to_char("You sit down.\r\n", ch);
				act("$n sits down.", FALSE, ch, 0, 0, TO_ROOM);
				GET_POS(ch) = POS_SITTING;
				break;
			}
			else {
				void do_sit_on_vehicle(char_data *ch, char *argument);
				do_sit_on_vehicle(ch, arg);
			}
			break;
		}
		case POS_SITTING:
			send_to_char("You're sitting already.\r\n", ch);
			break;
		case POS_RESTING:
			if (*arg) {
				send_to_char("You need to stand up before you can sit on something.\r\n", ch);
				return;
			}
			send_to_char("You stop resting, and sit up.\r\n", ch);
			act("$n stops resting.", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_SITTING;
			break;
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
}


ACMD(do_sleep) {
	switch (GET_POS(ch)) {
		case POS_SITTING:
			do_unseat_from_vehicle(ch);
		case POS_STANDING:
		case POS_RESTING:
			if (IS_RIDING(ch)) {
				msg_to_char(ch, "You climb down from your mount.\r\n");
				perform_dismount(ch);
			}
			send_to_char("You lie down and go to sleep.\r\n", ch);
			act("$n lies down and falls asleep.", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_SLEEPING;
			break;
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
}


ACMD(do_stand) {
	switch (GET_POS(ch)) {
		case POS_STANDING:
			send_to_char("You are already standing.\r\n", ch);
			break;
		case POS_SITTING:
			do_unseat_from_vehicle(ch);
			send_to_char("You stand up.\r\n", ch);
			act("$n clambers to $s feet.", TRUE, ch, 0, 0, TO_ROOM);
			/* Will be sitting after a successful bash and may still be fighting. */
			GET_POS(ch) = FIGHTING(ch) ? POS_FIGHTING : POS_STANDING;
			break;
		case POS_RESTING:
			send_to_char("You stop resting, and stand up.\r\n", ch);
			act("$n stops resting, and clambers on $s feet.", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_STANDING;
			break;
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


ACMD(do_transport) {
	extern room_data *find_starting_location();
	
	char arg[MAX_INPUT_LENGTH];
	room_data *target = NULL;
	
	one_word(argument, arg);
	
	if (!IS_APPROVED(ch) && config_get_bool("travel_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_START_LOCATION)) {
		msg_to_char(ch, "You need to be at a starting location to transport!\r\n");
	}
	else if (*arg && !(target = find_target_room(ch, arg))) {
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
		perform_transport(ch, target ? target : find_starting_location());
	}
}


ACMD(do_wake) {
	char_data *vict;
	int self = 0;

	one_argument(argument, arg);
	if (*arg) {
		if (GET_POS(ch) == POS_SLEEPING)
			send_to_char("Maybe you should wake yourself up first.\r\n", ch);
		else if ((vict = get_char_vis(ch, arg, FIND_CHAR_ROOM)) == NULL)
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


ACMD(do_worm) {
	int dir;
	room_data *to_room;

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot worm.\r\n");
	}
	else if (!can_use_ability(ch, ABIL_WORM, NOTHING, 0, NOTHING)) {
		return;
	}
	else if (!AFF_FLAGGED(ch, AFF_EARTHMELD))
		msg_to_char(ch, "You aren't even earthmelded!\r\n");
	else if (IS_INSIDE(IN_ROOM(ch)) || IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
		// map only
		msg_to_char(ch, "You can't do that here.\r\n");
	}
	else if (!*arg)
		msg_to_char(ch, "Which way would you like to move?\r\n");
	else if (is_abbrev(arg, "look"))
		look_at_room(ch);
	else if ((dir = parse_direction(ch, arg)) == NO_DIR)
		msg_to_char(ch, "That's not a direction!\r\n");
	else if (dir >= NUM_2D_DIRS) {
		msg_to_char(ch, "You can't go that way from here!\r\n");
	}
	else if (!(to_room = real_shift(IN_ROOM(ch), shift_dir[dir][0], shift_dir[dir][1])))
		msg_to_char(ch, "You can't go that way!\r\n");
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_SHALLOW_WATER) || SECT_FLAGGED(BASE_SECT(to_room), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_SHALLOW_WATER))
		msg_to_char(ch, "You can't pass through the water!\r\n");
	else if (GET_MOVE(ch) < 1)
		msg_to_char(ch, "You don't have enough energy left to do that.\r\n");
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_WORM)) {
		return;
	}
	else {
		do_simple_move(ch, dir, to_room, TRUE, MOVE_EARTHMELD);
		gain_ability_exp(ch, ABIL_WORM, 1);
		
		// on top of any wait from the move itself
		GET_WAIT_STATE(ch) += 1 RL_SEC;
	}
}

