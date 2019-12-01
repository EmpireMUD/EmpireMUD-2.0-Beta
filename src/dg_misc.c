/* ************************************************************************
*   File: dg_misc.c                                       EmpireMUD 2.0b5 *
*  Usage: contains general functions for script usage.                    *
*                                                                         *
*  DG Scripts code had no header or author info in this file              *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <math.h>

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "dg_scripts.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "dg_event.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"

// external funcs
void combat_meter_damage_dealt(char_data *ch, int amt);
void combat_meter_damage_taken(char_data *ch, int amt);
void combat_meter_heal_dealt(char_data *ch, int amt);
void combat_meter_heal_taken(char_data *ch, int amt);
extern obj_data *die(char_data *ch, char_data *killer);
extern room_data *get_room(room_data *ref, char *name);

/* external vars */
extern const char *apply_types[];
extern const char *affected_bits[];


/**
* Creates a room and adds it to the current ship/building.
*
* WARNING: This cannot currently be used for adventure rooms. That would take
* an upgrade.
*
* @param room_data *from The room we're attaching a new one to.
* @param int dir Which direction to add it.
* @param bld_data *bld A building prototype to attach, if desired (technically optional here but you do need to attach one at some point).
* @return room_data* The created room.
*/
room_data *do_dg_add_room_dir(room_data *from, int dir, bld_data *bld) {
	void add_room_to_vehicle(room_data *room, vehicle_data *veh);
	extern struct empire_territory_data *create_territory_entry(empire_data *emp, room_data *room);
	extern room_data *create_room(room_data *home);
	void sort_world_table();
	
	room_data *home = HOME_ROOM(from), *new;
	
	// create the new room
	new = create_room(home);
	create_exit(from, new, dir, TRUE);
	if (bld) {
		attach_building_to_room(bld, new, TRUE);
	}

	COMPLEX_DATA(home)->inside_rooms++;
	
	if (GET_ROOM_VEHICLE(from)) {
		++VEH_INSIDE_ROOMS(GET_ROOM_VEHICLE(from));
		COMPLEX_DATA(new)->vehicle = GET_ROOM_VEHICLE(from);
		add_room_to_vehicle(new, GET_ROOM_VEHICLE(from));
		SET_BIT(ROOM_AFF_FLAGS(new), ROOM_AFF_IN_VEHICLE);
		SET_BIT(ROOM_BASE_FLAGS(new), ROOM_AFF_IN_VEHICLE);
	}
	
	if (ROOM_OWNER(home)) {
		perform_claim_room(new, ROOM_OWNER(home));
	}
	
	// sort now just in case
	sort_world_table();
	
	return new;
}


/* modify an affection on the target. affections can be of the AFF_  */
/* variety or APPLY_ type. APPLY_'s have an integer value for them  */
/* while AFF_'s have boolean values. In any case, the duration MUST  */
/* be non-zero.                                                       */
/* usage:  apply <target> <property> <value> <duration seconds>       */
#define APPLY_TYPE	1
#define AFFECT_TYPE	2
void do_dg_affect(void *go, struct script_data *sc, trig_data *trig, int script_type, char *cmd) {
	char_data *ch = NULL;
	int value = 0, duration = 0;
	char junk[MAX_INPUT_LENGTH]; /* will be set to "dg_affect" */
	char charname[MAX_INPUT_LENGTH], property[MAX_INPUT_LENGTH];
	char value_p[MAX_INPUT_LENGTH], duration_p[MAX_INPUT_LENGTH];
	any_vnum atype = ATYPE_DG_AFFECT;
	bitvector_t i = 0, type = 0;
	struct affected_type af;
	bool all_off = FALSE;

	half_chop(cmd, junk, cmd);
	half_chop(cmd, charname, cmd);
	// sometimes charname is an affect vnum
	if (*charname == '#') {
		atype = atoi(charname+1);
		half_chop(cmd, charname, cmd);
		if (!find_generic(atype, GENERIC_AFFECT)) {
			script_log("Trigger: %s, VNum %d. dg_affect: Missing requested generic affect vnum %d", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), atype);
			atype = ATYPE_DG_AFFECT;
		}
	}
	
	half_chop(cmd, property, cmd);
	half_chop(cmd, value_p, duration_p);

	/* make sure all parameters are present */
	if (!*charname || !*property) {
		script_log("Trigger: %s, VNum %d. dg_affect usage: [#affect vnum] <target> <property> <value> <duration>", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
		return;
	}
	
	if (!str_cmp(property, "off")) {
		all_off = TRUE;
		// this is good -- no mor args
	}
	else {
		if (!*value_p) {
			script_log("Trigger: %s, VNum %d. dg_affect usage: [#affect vnum] <target> <property> <value> <duration>", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
			return;
		}
		else if (str_cmp(value_p, "off") && !*duration_p) {
			script_log("Trigger: %s, VNum %d. dg_affect missing duration", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
			return;
		}

		value = atoi(value_p);
		duration = atoi(duration_p);
		if ((duration == 0 || duration < -1) && str_cmp(value_p, "off")) {
			script_log("Trigger: %s, VNum %d. dg_affect: need positive duration!", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
			script_log("Line was: dg_affect %s %s %s %s (%d)", charname, property, value_p, duration_p, duration);
			return;
		}

		/* find the property -- first search apply_types */
		if ((i = search_block(property, apply_types, TRUE)) != NOTHING) {
			type = APPLY_TYPE;
		}

		if (!type) { /* search affect_bits now */
			if ((i = search_block(property, affected_bits, TRUE)) != NOTHING) {
				type = AFFECT_TYPE;
			}
		}

		if (!type) { /* property not found */
			script_log("Trigger: %s, VNum %d. dg_affect: unknown property '%s'!", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), property);
			return;
		}
	}

	/* locate the target */
	ch = get_char(charname);
	if (!ch) {
		script_log("Trigger: %s, VNum %d. dg_affect: cannot locate target!", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
		return;
	}
	if (IS_DEAD(ch) || EXTRACTED(ch)) {
		// no affects on the dead
		return;
	}
	
	if (duration == -1 && !IS_NPC(ch) && str_cmp(value_p, "off")) {
		script_log("Trigger: %s, VNum %d. dg_affect: cannot use infinite duration on player target", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
		return;
	}
	
	// are we just removing the whole thing?
	if (all_off) {
		affect_from_char(ch, atype, TRUE);
		return;
	}
	
	// removing one type?
	if (!str_cmp(value_p, "off")) {
		if (type == APPLY_TYPE) {
			affect_from_char_by_apply(ch, atype, i, TRUE);
		}
		else {
			affect_from_char_by_bitvector(ch, atype, BIT(i), TRUE);
		}
		return;
	}

	/* add the affect */
	af.type = atype;
	af.cast_by = (script_type == MOB_TRIGGER ? CAST_BY_ID((char_data*)go) : 0);
	af.duration = (duration == -1 ? UNLIMITED : ceil((double)duration / SECS_PER_REAL_UPDATE));
	af.modifier = value;

	if (type == APPLY_TYPE) {
		af.location = i;
		af.bitvector = NOBITS;
	}
	else {
		af.location = APPLY_NONE;
		af.bitvector = BIT(i);
	}

	affect_to_char(ch, &af);
}


/**
* add/remove an affect on a room
*/
void do_dg_affect_room(void *go, struct script_data *sc, trig_data *trig, int script_type, char *cmd) {
	extern const char *room_aff_bits[];
	
	char junk[MAX_INPUT_LENGTH]; /* will be set to "dg_affect_room" */
	char roomname[MAX_INPUT_LENGTH], property[MAX_INPUT_LENGTH];
	char value_p[MAX_INPUT_LENGTH], duration_p[MAX_INPUT_LENGTH];
	bitvector_t i = 0, type = 0;
	int atype = ATYPE_DG_AFFECT;
	struct affected_type af;
	room_data *room = NULL;
	bool all_off = FALSE;
	int duration = 0;

	half_chop(cmd, junk, cmd);
	half_chop(cmd, roomname, cmd);
	// sometimes roomname is an affect vnum
	if (*roomname == '#') {
		atype = atoi(roomname+1);
		half_chop(cmd, roomname, cmd);
		if (!find_generic(atype, GENERIC_AFFECT)) {
			atype = ATYPE_DG_AFFECT;
		}
	}
	half_chop(cmd, property, cmd);
	half_chop(cmd, value_p, duration_p);

	/* make sure all parameters are present */
	if (!*roomname || !*property) {
		script_log("Trigger: %s, VNum %d. dg_affect_room usage: [#affect vnum] <room> <property> <on|off> <duration>", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
		return;
	}
	
	if (!str_cmp(property, "off")) {
		all_off = TRUE;
		// simple mode
	}
	else {	// niot all-off
		// duration:
		if (str_cmp(value_p, "off")) {	// not "off"
			if (!*value_p || !*duration_p) {
				script_log("Trigger: %s, VNum %d. dg_affect_room usage: [#affect vnum] <room> <property> <on|off> <duration>", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
				return;
			}
	
			duration = atoi(duration_p);
			if (duration == 0 || duration < -1) {
				script_log("Trigger: %s, VNum %d. dg_affect_room: need positive duration!", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
				script_log("Line was: dg_affect_room %s %s %s %s (%d)", roomname, property, value_p, duration_p, duration);
				return;
			}
		}

		// find the property -- search room_aff_bits
		if ((i = search_block(property, room_aff_bits, TRUE)) != NOTHING) {
			type = AFFECT_TYPE;
		}

		if (!type) { // property not found
			script_log("Trigger: %s, VNum %d. dg_affect_room: unknown property '%s'!", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), property);
			return;
		}
	}

	/* locate the target */
	room = get_room((script_type == WLD_TRIGGER ? (room_data*)go : NULL), roomname);
	if (!room) {
		script_log("Trigger: %s, VNum %d. dg_affect_room: cannot locate target: %s", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), roomname);
		return;
	}
	
	if (duration == -1 && IS_OUTDOOR_TILE(room) && GET_ROOM_VNUM(room) < MAP_SIZE) {
		script_log("Trigger: %s, VNum %d. dg_affect_room: cannot use infinite duration on map tile target", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
		return;
	}
	
	if (all_off) {	// removing all matching types
		affect_from_room(room, atype);
		return;
	}

	if (!str_cmp(value_p, "off")) {	// remove by bit
		affect_from_room_by_bitvector(room, atype, BIT(i), FALSE);
		return;
	}

	/* add the affect */
	af.type = atype;
	af.cast_by = (script_type == MOB_TRIGGER ? CAST_BY_ID((char_data*)go) : 0);
	af.duration = time(0) + duration;	// duration is actually expire time on room affs (TODO: change the name)
	af.modifier = 0;
	af.location = APPLY_NONE;
	af.bitvector = BIT(i);

	affect_to_room(room, &af);
}


/**
* Do the actual work for the %build% commands, once everything has been
* validated except the last arg.
*
* @param room_data *target The room to change.
* @param char *argument <vnum [dir] | ruin | demolish>
*/
void do_dg_build(room_data *target, char *argument) {
	void complete_building(room_data *room);
	void ruin_one_building(room_data *room);	// db.world.c
	void special_building_setup(char_data *ch, room_data *room);
	extern const int rev_dir[];
	
	char vnum_arg[MAX_INPUT_LENGTH], dir_arg[MAX_INPUT_LENGTH];
	bool ruin = FALSE, demolish = FALSE;
	any_vnum vnum = NOTHING;
	bld_data *bld = NULL;
	int dir = NORTH;
	
	skip_spaces(&argument);
	
	if (!target || !*argument) {
		return;
	}
	
	// parse arg
	if (is_abbrev(argument, "ruin")) {
		ruin = TRUE;
	}
	else if (is_abbrev(argument, "demolish")) {
		demolish = TRUE;
	}
	else if (isdigit(*argument)) {
		argument = any_one_arg(argument, vnum_arg);
		argument = any_one_arg(argument, dir_arg);
		
		if ((vnum = atoi(vnum_arg)) < 0 || !(bld = building_proto(vnum)) || IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM)) {
			script_log("do_dg_build: invalid building: %d", vnum);
			return;
		}
		else if (*dir_arg && ((dir = parse_direction(NULL, dir_arg)) == NO_DIR || dir >= NUM_2D_DIRS)) {
			script_log("do_dg_build: invalid direction: %s", dir_arg);
			return;
		}
		else if (dir != NO_DIR && (!SHIFT_DIR(target, dir) || (IS_SET(GET_BLD_FLAGS(bld), BLD_TWO_ENTRANCES) && !SHIFT_DIR(target, rev_dir[dir])))) {
			script_log("do_dg_build: invalid direction from room %d: %s", GET_ROOM_VNUM(target), dir_arg);
			return;
		}
		
		// bld validated
	}
	else {
		script_log("do_dg_build: invalid argument: %s", argument);
		return;
	}
	
	
	if (ruin) {
		room_data *home = HOME_ROOM(target);
		if (GET_ROOM_VNUM(home) < MAP_SIZE && GET_BUILDING(home)) {
			ruin_one_building(home);
		}
	}
	else if (demolish) {
		disassociate_building(target);
	}
	else if (bld) {
		disassociate_building(target);
	
		construct_building(target, GET_BLD_VNUM(bld));
		complete_building(target);
		special_building_setup(NULL, target);
	
		if (dir != NO_DIR) {
			create_exit(target, SHIFT_DIR(target, dir), dir, FALSE);
			if (IS_SET(GET_BLD_FLAGS(bld), BLD_TWO_ENTRANCES)) {
				create_exit(target, SHIFT_DIR(target, rev_dir[dir]), rev_dir[dir], FALSE);
			}
			COMPLEX_DATA(target)->entrance = rev_dir[dir];
		}
	}
}


/**
* Performs a script-caused ownership change on one (or more) things.
*
* @param empire_data *emp The empire to change ownership to (may be NULL for none).
* @param char_data *vict Optional: A mob whose ownership to change.
* @param obj_data *obj Optional: An object whose ownership to change.
* @param room_data *room Optional: A room whose ownership to change.
* @param vehicle_data *veh Optional: A vehicle whose ownership to change.
*/
void do_dg_own(empire_data *emp, char_data *vict, obj_data *obj, room_data *room, vehicle_data *veh) {
	void adjust_vehicle_tech(vehicle_data *veh, bool add);
	void kill_empire_npc(char_data *ch);
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	empire_data *owner;
	
	if (vict && IS_NPC(vict)) {
		if (GET_LOYALTY(vict) && emp != GET_LOYALTY(vict) && GET_EMPIRE_NPC_DATA(vict)) {
			// resets the population timer on their house
			kill_empire_npc(vict);
		}
		GET_LOYALTY(vict) = emp;
		setup_generic_npc(vict, emp, MOB_DYNAMIC_NAME(vict), MOB_DYNAMIC_SEX(vict));
	}
	if (obj) {
		obj->last_empire_id = emp ? EMPIRE_VNUM(emp) : NOTHING;
	}
	if (veh) {
		if ((owner = VEH_OWNER(veh)) && emp != owner) {
			VEH_SHIPPING_ID(veh) = -1;
			if (VEH_INTERIOR_HOME_ROOM(veh)) {
				abandon_room(VEH_INTERIOR_HOME_ROOM(veh));
			}
			adjust_vehicle_tech(veh, FALSE);
			
			qt_empire_players(owner, qt_lose_vehicle, VEH_VNUM(veh));
			et_lose_vehicle(owner, VEH_VNUM(veh));
		}
		VEH_OWNER(veh) = emp;
		if (emp && VEH_INTERIOR_HOME_ROOM(veh)) {
			claim_room(VEH_INTERIOR_HOME_ROOM(veh), emp);
		}
		if (emp) {
			adjust_vehicle_tech(veh, TRUE);
			qt_empire_players(emp, qt_gain_vehicle, VEH_VNUM(veh));
			et_gain_vehicle(emp, VEH_VNUM(veh));
		}
	}
	if (room) {
		if (ROOM_OWNER(room) && emp != ROOM_OWNER(room)) {
			abandon_room(room);
		}
		if (emp) {
			claim_room(room, emp);
		}
		if (GET_ROOM_VEHICLE(room)) {
			VEH_OWNER(GET_ROOM_VEHICLE(room)) = emp;
		}
	}
}


/**
* Purges all matching vnums in an instance. This is called from:
* %purge% instance <arguments>
*
* Currently it supports:
* - mob <vnum> [message]
*
* @param void *owner The owner of the script, to check if it purges itself.
* @param struct instance_data *inst The instance whose contents are being purged.
* @param char *argument The arguments passed to "%purge% instance".
*/
void dg_purge_instance(void *owner, struct instance_data *inst, char *argument) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	char_data *mob, *next_mob;
	obj_data *obj, *next_obj;
	any_vnum vnum;
	int iter;
	
	if (!inst) {
		return;
	}
	
	argument = any_one_arg(argument, arg1);	// type
	argument = any_one_arg(argument, arg2);	// vnum
	skip_spaces(&argument);	// message
	vnum = atoi(arg2);
	
	if (!*arg1 || !*arg2) {
		script_log("dg_purge_instance called with invalid arguments: %s %s %s", arg1, arg2, argument);
	}
	else if (is_abbrev(arg1, "mobile")) {
		LL_FOREACH_SAFE(character_list, mob, next_mob) {
			if (!IS_NPC(mob) || GET_MOB_VNUM(mob) != vnum || EXTRACTED(mob) || MOB_INSTANCE_ID(mob) != INST_ID(inst)) {
				continue;
			}
			
			// found
			if (*argument) {
				act(argument, TRUE, mob, NULL, NULL, TO_ROOM);
			}
			
			if (mob == owner) {
				dg_owner_purged = 1;
			}
			extract_char(mob);
		}
	}
	else if (is_abbrev(arg1, "object")) {
		for (iter = 0; iter < INST_SIZE(inst); ++iter) {
			if (!INST_ROOM(inst, iter)) {
				continue;
			}
			
			LL_FOREACH_SAFE2(ROOM_CONTENTS(INST_ROOM(inst, iter)), obj, next_obj, next_content) {
				if (GET_OBJ_VNUM(obj) != vnum) {
					continue;
				}
				
				// found!
				if (*argument && ROOM_PEOPLE(INST_ROOM(inst, iter))) {
					act(argument, FALSE, ROOM_PEOPLE(INST_ROOM(inst, iter)), NULL, NULL, TO_CHAR | TO_ROOM);
				}
			
				if (obj == owner) {
					dg_owner_purged = 1;
				}
				extract_obj(obj);
			}
		}
	}
	else {
		script_log("dg_purge_instance called with invalid type: %s", arg1);
	}
}


/**
* Processes the %quest% command.
*
* @param int go_type _TRIGGER type for 'go'
* @param void *go A pointer to the thing the trigger is running on.
* @param char *argument The typed-in arg.
*/
void do_dg_quest(int go_type, void *go, char *argument) {
	extern struct instance_data *get_instance_by_id(any_vnum instance_id);
	extern struct player_quest *is_on_quest(char_data *ch, any_vnum quest);
	
	char vict_arg[MAX_INPUT_LENGTH], cmd_arg[MAX_INPUT_LENGTH], vnum_arg[MAX_INPUT_LENGTH];
	struct instance_data *inst = NULL;
	struct player_quest *pq;
	empire_data *emp = NULL;
	room_data *room = NULL;
	char_data *vict = NULL;
	quest_data *quest;
	any_vnum vnum;
	
	argument = any_one_arg(argument, vict_arg);
	argument = any_one_arg(argument, cmd_arg);
	argument = any_one_arg(argument, vnum_arg);
	
	if (!*vict_arg || !*cmd_arg || !*vnum_arg) {
		script_log_by_type(go_type, go, "dg_quest: too few args");
		return;
	}
	if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0 || !(quest = quest_proto(vnum))) {
		script_log_by_type(go_type, go, "dg_quest: invalid vnum '%s'", vnum_arg);
		return;
	}
	if (QUEST_FLAGGED(quest, QST_IN_DEVELOPMENT)) {
		script_log_by_type(go_type, go, "dg_quest: quest [%d] %s is set IN-DEVELOPMENT", QUEST_VNUM(quest), QUEST_NAME(quest));
		return;
	}
	
	// find vict by uid?
	if (*vict_arg == UID_CHAR) {
		vict = get_char(vict_arg);
	}
	
	// x_TRIGGER: set up basics
	switch (go_type) {
		case MOB_TRIGGER: {
			char_data *mob = (char_data*)go;
			room = IN_ROOM(mob);
			emp = GET_LOYALTY(mob);
			inst = get_instance_by_id(MOB_INSTANCE_ID(mob));
			if (!vict) {
				vict = get_char_room_vis(mob, vict_arg);
			}
			break;
		}
		case OBJ_TRIGGER: {
			extern room_data *obj_room(obj_data *obj);
			
			obj_data *obj = (obj_data*)go;
			room = obj_room(obj);
			if (!vict) {
				vict = get_char_near_obj(obj, vict_arg);
			}
			emp = (obj->carried_by ? GET_LOYALTY(obj->carried_by) : ((!CAN_WEAR(obj, ITEM_WEAR_TAKE) || !vict) ? ROOM_OWNER(room) : (vict ? GET_LOYALTY(vict) : NULL)));
			break;
		}
		case WLD_TRIGGER:
		case RMT_TRIGGER:
		case ADV_TRIGGER:
		case BLD_TRIGGER: {
			extern char_data *get_char_in_room(room_data *room, char *name);
			
			room = (room_data*)go;
			emp = ROOM_OWNER(room);
			if (!vict) {
				vict = get_char_in_room(room, vict_arg);
			}
			break;
		}
		case VEH_TRIGGER: {
			vehicle_data *veh = (vehicle_data*)go;
			room = IN_ROOM(veh);
			emp = VEH_OWNER(veh);
			if (!vict) {
				vict = get_char_near_vehicle(veh, vict_arg);
			}
			break;
		}
		case EMP_TRIGGER: {
			script_log_by_type(go_type, go, "dg_quest: empire triggers are not supported");
			break;
		}
		default: {
			script_log_by_type(go_type, go, "dg_quest: unknown type %d", go_type);
			return;
		}
	}
	
	// should have detected a victim
	if (!vict) {
		script_log_by_type(go_type, go, "dg_quest: unable to find target '%s'", vict_arg);
		return;
	}
	
	// ready for commands
	if (is_abbrev(cmd_arg, "drop")) {
		if ((pq = is_on_quest(vict, QUEST_VNUM(quest)))) {
			void drop_quest(char_data *ch, struct player_quest *pq);
			drop_quest(vict, pq);
		}
	}
	else if (is_abbrev(cmd_arg, "finish")) {
		if ((pq = is_on_quest(vict, QUEST_VNUM(quest)))) {
			void complete_quest(char_data *ch, struct player_quest *pq, empire_data *giver_emp);
			complete_quest(vict, pq, emp);
		}
	}
	else if (is_abbrev(cmd_arg, "start")) {
		if (!is_on_quest(vict, QUEST_VNUM(quest))) {
			void start_quest(char_data *ch, quest_data *qst, struct instance_data *inst);
			if (!inst && room && COMPLEX_DATA(room)) {
				inst = COMPLEX_DATA(room)->instance;
			}
			start_quest(vict, quest, inst);
		}
	}
	else if (is_abbrev(cmd_arg, "trigger")) {
		qt_triggered_task(vict, QUEST_VNUM(quest));
	}
	else if (is_abbrev(cmd_arg, "untrigger")) {
		qt_untrigger_task(vict, QUEST_VNUM(quest));
	}
	else {
		script_log_by_type(go_type, go, "dg_quest: invalid command '%s'", cmd_arg);
	}
}


/**
* Do the actual work for the %terracrop% commands, once everything has been
* validated.
*
* @param room_data *target The room to change.
* @param crop_data *cp The crop to change it to.
*/
void do_dg_terracrop(room_data *target, crop_data *cp) {
	sector_data *sect;
	
	if (!target || !cp) {
		return;
	}
	
	if (!(sect = find_first_matching_sector(SECTF_CROP, NOBITS))) {
		// no crop sects?
		return;
	}
	else {
		change_terrain(target, GET_SECT_VNUM(sect));
		set_crop_type(target, cp);
		
		remove_depletion(target, DPLTN_PICK);
		remove_depletion(target, DPLTN_FORAGE);
		remove_depletion(target, DPLTN_DIG);
		remove_depletion(target, DPLTN_GATHER);
		remove_depletion(target, DPLTN_FISH);
		remove_depletion(target, DPLTN_QUARRY);
		remove_depletion(target, DPLTN_PAN);
		remove_depletion(target, DPLTN_TRAPPING);
		remove_depletion(target, DPLTN_CHOP);
		
		if (ROOM_OWNER(target)) {
			void deactivate_workforce_room(empire_data *emp, room_data *room);
			deactivate_workforce_room(ROOM_OWNER(target), target);
		}
	}
}


/**
* Do the actual work for the %terraform% commands, once everything has been
* validated.
*
* @param room_data *target The room to change.
* @param sector_data *sect The sector to change it to.
*/
void do_dg_terraform(room_data *target, sector_data *sect) {
	void finish_trench(room_data *room);
	
	sector_data *old_sect;
	
	if (!target || !sect) {
		return;
	}
	
	old_sect = BASE_SECT(target);
	
	change_terrain(target, GET_SECT_VNUM(sect));
	
	// preserve old original sect for roads -- TODO this is a special-case
	if (IS_ROAD(target)) {
		change_base_sector(target, old_sect);
	}
	
	remove_depletion(target, DPLTN_PICK);
	remove_depletion(target, DPLTN_FORAGE);
	remove_depletion(target, DPLTN_DIG);
	remove_depletion(target, DPLTN_GATHER);
	remove_depletion(target, DPLTN_FISH);
	remove_depletion(target, DPLTN_QUARRY);
	remove_depletion(target, DPLTN_PAN);
	remove_depletion(target, DPLTN_TRAPPING);
	remove_depletion(target, DPLTN_CHOP);
	
	if (ROOM_OWNER(target)) {
		void deactivate_workforce_room(empire_data *emp, room_data *room);
		deactivate_workforce_room(ROOM_OWNER(target), target);
	}
	
	if (SECT_FLAGGED(sect, SECTF_IS_TRENCH)) {
		finish_trench(target);
	}
}


/**
* Just checks if a trigger is attached to a given SCRIPT(*).
*
* @param struct script_data *sc The SCRIPT() from a char/obj/etc.
* @param any_vnum vnum Which trigger to check for.
* @return bool TRUE if the trigger is attached, FALSE if not.
*/
bool has_trigger(struct script_data *sc, any_vnum vnum) {
	trig_data *trig;
	
	if (!sc) {	// no script data?
		return FALSE;
	}
	LL_FOREACH(TRIGGERS(sc), trig) {
		if (GET_TRIG_VNUM(trig) == vnum) {
			return TRUE;	// found any 1 copy of it
		}
	}
	
	// not found
	return FALSE;
}


/**
* For the has_component and charge_component script functions. Both are
* expected to look like:
*   %actor.has_component(type, amount, <optional comma-separated flags>)%
*
* Examples: %actor.has_component(fibers, 3, plant, hard)%
*           %actor.charge_component(sapling, 1)%
*
* @param char *argument The text inside the (parens).
* @param int *cmp_type An int pointer to pass back the component type.
* @param int *number An int pointer to pass back the number requested.
* @param bitvector_t *cmp_flags A pointer to pass back and component flags.
* @return bool TRUE if the string parsed correctly; FALSE if this was not a valid string.
*/
bool parse_script_component_args(char *argument, int *cmp_type, int *number, bitvector_t *cmp_flags) {
	extern const char *component_flags[];
	extern const char *component_types[];
	
	char str_arg[256], num_arg[256], arg[256], temp[256];
	bitvector_t flag;
	
	*cmp_type = 0;
	*number = 0;
	*cmp_flags = NOBITS;
	
	comma_args(argument, str_arg, temp);	// type?
	strcpy(arg, temp);
	comma_args(arg, num_arg, temp);	/// number
	strcpy(arg, temp);
	
	// basic args
	if (!*str_arg || !*num_arg) {
		return FALSE;	// first 2 args are required
	}
	if ((*cmp_type = search_block(str_arg, component_types, FALSE)) == NOTHING) {
		return FALSE;	// not a valid type
	}
	if (!isdigit(*num_arg) || (*number = atoi(num_arg)) < 0) {
		return FALSE;	// invalid number
	}
	
	// any remaining args? these will be bits
	while (*arg) {
		comma_args(arg, str_arg, temp);
		strcpy(arg, temp);
		
		if ((flag = search_block(str_arg, component_flags, FALSE)) != NOTHING) {
			*cmp_flags |= BIT(flag);
		}
		else {
			return FALSE;	// not a valid component flag
		}
	}
	
	// if we survived to here, we got a full match (and already set the pointers)
	return TRUE;
}


void send_char_pos(char_data *ch, int dam) {
	switch (GET_POS(ch)) {
		case POS_MORTALLYW:
			act("$n is mortally wounded, and will die soon, if not aided.", TRUE, ch, 0, 0, TO_ROOM);
			msg_to_char(ch, "You are mortally wounded, and will die soon, if not aided.\r\n");
			break;
		case POS_INCAP:
			act("$n is incapacitated and will slowly die, if not aided.", TRUE, ch, 0, 0, TO_ROOM);
			msg_to_char(ch, "You are incapacitated and will slowly die, if not aided.\r\n");
			break;
		case POS_STUNNED:
			act("$n is stunned, but will probably regain consciousness again.", TRUE, ch, 0, 0, TO_ROOM);
			msg_to_char(ch, "You're stunned, but will probably regain consciousness again.\r\n");
			break;
		case POS_DEAD:
			act("$n is dead!  R.I.P.", FALSE, ch, 0, 0, TO_ROOM);
			send_to_char("You are dead!  Sorry...\r\n", ch);
			break;
		default:                        /* >= POSITION SLEEPING */
			if (dam > (GET_MAX_HEALTH(ch) / 4))
				act("That really did HURT!", FALSE, ch, 0, 0, TO_CHAR);
			if (GET_HEALTH(ch) < (GET_MAX_HEALTH(ch) / 4))
				msg_to_char(ch, "&rYou wish that your wounds would stop BLEEDING so much!&0\r\n");
			break;
	}
}


/* Used throughout the xxxcmds.c files for checking if a char
* can be targetted 
* - allow_gods is false when called by %force%, for instance,
* while true for %teleport%.  -- Welcor 
*/
int valid_dg_target(char_data *ch, int bitvector) {
	if (IS_NPC(ch))
		return TRUE;  /* all npcs are allowed as targets */
	else if (GET_ACCESS_LEVEL(ch) < LVL_START_IMM || !NOHASSLE(ch))
		return TRUE;  /* as well as all mortals */
	else if (!IS_SET(bitvector, DG_ALLOW_GODS) && GET_ACCESS_LEVEL(ch) >= LVL_CIMPL) 
		return FALSE; /* but not always the highest gods */
	else if (GET_INVIS_LEV(ch) >= LVL_START_IMM) {
		// skip invisible immortals
		return FALSE;
	}
	else {
		// lower-level gods who are visible
		return TRUE;
	}
}


/**
* Runs triggers to update and prepare the mud at startup.
*/
void run_reboot_triggers(void) {
	void reboot_mtrigger(char_data *ch);
	void reboot_otrigger(obj_data *obj);
	void reboot_vtrigger(vehicle_data *veh);
	void reboot_wtrigger(room_data *room);
	
	vehicle_data *veh, *next_veh;
	room_data *room, *next_room;
	char_data *mob, *next_mob;
	obj_data *obj, *next_obj;
	
	HASH_ITER(hh, world_table, room, next_room) {
		reboot_wtrigger(room);
	}
	LL_FOREACH_SAFE(character_list, mob, next_mob) {
		reboot_mtrigger(mob);
	}
	LL_FOREACH_SAFE(object_list, obj, next_obj) {
		reboot_otrigger(obj);
	}
	LL_FOREACH_SAFE(vehicle_list, veh, next_veh) {
		reboot_vtrigger(veh);
	}
}


/**
* Deals damage to a character based on scaled level and modifier.
*
* @param char_data *vict The poor sod who's taking damage.
* @param char_data *killer Optional: The person to credit with the kill.
* @param int level The level to scale damage to.
* @param int dam_type The DAM_x type of damage.
* @param double modifier Percent to multiply scaled damage by (to make it lower or higher).
*/
void script_damage(char_data *vict, char_data *killer, int level, int dam_type, double modifier) {
	void death_log(char_data *ch, char_data *killer, int type);
	extern char *get_room_name(room_data *room, bool color);
	extern int reduce_damage_from_skills(int dam, char_data *victim, char_data *attacker, int damtype);
	void scale_mob_for_character(char_data *mob, char_data *ch);
	void scale_mob_to_level(char_data *mob, int level);
	
	double dam;
	
	// no point damaging the dead
	if (IS_DEAD(vict)) {
		return;
	}
	if (AFF_FLAGGED(vict, AFF_NO_ATTACK)) {
		return;	// can't be attacked
	}
	
	if (IS_IMMORTAL(vict) && (modifier > 0)) {
		msg_to_char(vict, "Being the cool immortal you are, you sidestep a trap, obviously placed to kill you.\r\n");
		return;
	}
	
	// check scaling
	if (killer && IS_NPC(killer) && GET_CURRENT_SCALE_LEVEL(killer) == 0) {
		scale_mob_for_character(killer, vict);
	}
	if (IS_NPC(vict) && GET_CURRENT_SCALE_LEVEL(vict) == 0) {
		if (killer) {
			scale_mob_for_character(vict, killer);
		}
		else {
			scale_mob_to_level(vict, level);
		}
	}
	
	dam = level / 7.0;
	if (level > 100) {
		dam *= 1.0 + ((level - 100) / 40.0);
	}
	dam *= modifier;
	
	// full immunity
	if (AFF_FLAGGED(vict, AFF_IMMUNE_DAMAGE)) {
		dam = 0;
	}
	
	// guarantee at least 1
	if (modifier > 0) {
		dam = reduce_damage_from_skills(dam, vict, killer, dam_type);	// resistance, etc
		dam = MAX(1, dam);
		
		if (killer && vict != killer) {
			combat_meter_damage_dealt(killer, dam);
		}
		combat_meter_damage_taken(vict, dam);
	}
	else if (modifier < 0) {
		// healing
		dam = MIN(-1, dam);
		
		if (killer) {
			combat_meter_heal_dealt(killer, -dam);
		}
		combat_meter_heal_taken(vict, -dam);
	}
	
	GET_HEALTH(vict) -= dam;
	GET_HEALTH(vict) = MIN(GET_HEALTH(vict), GET_MAX_HEALTH(vict));

	update_pos(vict);
	send_char_pos(vict, dam);

	if (GET_POS(vict) == POS_DEAD) {
		if (!IS_NPC(vict)) {
			syslog(SYS_DEATH, 0, TRUE, "%s killed by script at %s", GET_NAME(vict), get_room_name(IN_ROOM(vict), FALSE));
			death_log(vict, vict, TYPE_SUFFERING);
		}
		die(vict, killer ? killer : vict);
	}
	else if (modifier < 0 && GET_HEALTH(vict) > 0 && GET_POS(vict) < POS_SLEEPING) {
		msg_to_char(vict, "You recover and wake up.\r\n");
		GET_POS(vict) = IS_NPC(vict) ? POS_STANDING : POS_SITTING;
	}
}


/**
* This variant of script_damage adds a scaled damage-over-time effect to the
* victim.
*
* @param char_data *vict The person receiving the DoT.
* @param any_vnum atype The ATYPE_ const or vnum for the affect.
* @param int level The level to scale damage to.
* @param int dam_type A DAM_x type.
* @param double modifier An amount to modify the damage by (1.0 = full damage).
* @param int dur_seconds The requested duration, in seconds.
* @param int max_stacks Number of times this DoT can stack (minimum/default 1).
* @param char_data *cast_by The caster, if any, for tracking on the effect (may be NULL).
*/
void script_damage_over_time(char_data *vict, any_vnum atype, int level, int dam_type, double modifier, int dur_seconds, int max_stacks, char_data *cast_by) {
	double dam;
	
	if (modifier <= 0 || dur_seconds <= 0) {
		return;
	}
	
	if (IS_IMMORTAL(vict) && (modifier > 0)) {
		msg_to_char(vict, "Being the cool immortal you are, you sidestep a trap, obviously placed to kill you.\r\n");
		return;
	}
	
	dam = level / 17.5;
	dam *= modifier;
	
	// guarantee at least 1
	if (modifier > 0) {
		dam = MAX(1, dam);
	}

	// add the affect
	apply_dot_effect(vict, atype, ceil((double)dur_seconds / SECS_PER_REAL_UPDATE), dam_type, (int) dam, MAX(1, max_stacks), cast_by);
}


/**
* Central processor for %heal% -- this is used to restore scaled health/move/
* mana, and (optionally) to remove debuffs and dots.
*
* Expected parameters in the string: <target> <what to heal> [scale modifier]
* Example: %self% mana 100
*
* @param void *thing The thing calling the script (mob, room, obj, veh)
* @param int type What type "thing" is (MOB_TRIGGER, etc)
* @param char *argument The text passed to the command.
*/
void script_heal(void *thing, int type, char *argument) {
	extern char_data *get_char_by_room(room_data *room, char *name);
	extern char_data *get_char_by_vehicle(vehicle_data *veh, char *name);
	extern int get_room_scale_level(room_data *room, char_data *targ);
	extern const double apply_values[];
	extern const bool aff_is_bad[];
	
	char targ_arg[MAX_INPUT_LENGTH], what_arg[MAX_INPUT_LENGTH], *scale_arg, log_root[MAX_STRING_LENGTH];
	struct affected_type *aff, *next_aff;
	int pos, amount, level = -1;
	char_data *victim = NULL;
	double scale = 100.0;
	bool done_aff;
	bitvector_t bitv;
	
	// 3 args: target, what, scale
	scale_arg = any_one_arg(argument, targ_arg);
	scale_arg = any_one_arg(scale_arg, what_arg);
	skip_spaces(&scale_arg);
	if (*targ_arg == UID_CHAR) {
		victim = get_char(targ_arg);
	}	// otherwise we'll determine victim later
	
	// determine how to log errors
	switch (type) {
		case MOB_TRIGGER: {
			level = get_approximate_level((char_data*)thing);
			if (!victim) {
				victim = get_char_room_vis((char_data*)thing, targ_arg);
			}
			
			snprintf(log_root, sizeof(log_root), "Mob (%s, VNum %d)::", GET_SHORT((char_data*)thing), GET_MOB_VNUM((char_data*)thing));
			break;
		}
		case OBJ_TRIGGER: {
			level = GET_OBJ_CURRENT_SCALE_LEVEL((obj_data*)thing);
			if (!victim) {
				victim = get_char_by_obj((obj_data*)thing, targ_arg);
			}
			
			snprintf(log_root, sizeof(log_root), "Obj (%s, VNum %d)::", GET_OBJ_SHORT_DESC((obj_data*)thing), GET_OBJ_VNUM((obj_data*)thing));
			break;
		}
		case VEH_TRIGGER: {
			level = VEH_SCALE_LEVEL((vehicle_data*)thing);
			if (!victim) {
				victim = get_char_by_vehicle((vehicle_data*)thing, targ_arg);
			}
			
			snprintf(log_root, sizeof(log_root), "Veh (%s, VNum %d)::", VEH_SHORT_DESC((vehicle_data*)thing), VEH_VNUM((vehicle_data*)thing));
			break;
		}
		case EMP_TRIGGER: {
			script_log("%s script_heal: Empire scripts are not supported", log_root);
			break;
		}
		case WLD_TRIGGER:
		default: {
			level = get_room_scale_level((room_data*)thing, NULL);
			if (!victim) {
				victim = get_char_by_room((room_data*)thing, targ_arg);
			}
			
			snprintf(log_root, sizeof(log_root), "Room %d ::", GET_ROOM_VNUM((room_data*)thing));
			break;
		}
	}
	
	if (!*targ_arg || !*what_arg) {
		script_log("%s script_heal: Invalid arguments: %s", log_root, argument);
		return;
	}
	if (level < 0) {
		script_log("%s script_heal: Unable to detect level", log_root);
		return;
	}
	if (!victim) {
		script_log("%s script_heal: Unable to find target: %s", log_root, targ_arg);
		return;
	}
	if (IS_DEAD(victim)) {
		return;	// fail silently on dead people
	}
	
	// process scale arg (optional)
	if (*scale_arg && (scale = atof(scale_arg)) < 1) {
		script_log("%s script_heal: Invalid scale argument: %s", log_root, scale_arg);
		return;
	}
	scale /= 100.0;	// convert to percent
	
	// now the real work
	if (is_abbrev(what_arg, "health") || is_abbrev(what_arg, "hitpoints")) {
		amount = (394 * level / 55.0 - 5580 / 11.0) * scale;
		amount = MAX(30, amount);
		GET_HEALTH(victim) = MIN(GET_MAX_HEALTH(victim), GET_HEALTH(victim) + amount);
		
		if (GET_POS(victim) < POS_SLEEPING) {
			GET_POS(victim) = POS_STANDING;
		}
	}
	else if (is_abbrev(what_arg, "mana")) {
		amount = (292 * level / 55.0 - 3940 / 11.0) * scale;
		amount = MAX(40, amount);
		GET_MANA(victim) = MIN(GET_MAX_MANA(victim), GET_MANA(victim) + amount);
	}
	else if (is_abbrev(what_arg, "moves")) {
		amount = (37 * level / 11.0 - 1950 / 11.0) * scale;
		amount = MAX(75, amount);
		GET_MOVE(victim) = MIN(GET_MAX_MOVE(victim), GET_MOVE(victim) + amount);
	}
	else if (is_abbrev(what_arg, "dots")) {
		while (victim->over_time_effects) {
			dot_remove(victim, victim->over_time_effects);
		}
	}
	else if (is_abbrev(what_arg, "debuffs")) {
	
		LL_FOREACH_SAFE(victim->affected, aff, next_aff) {
			// can't cleanse penalties (things cast by self)
			if (aff->cast_by == CAST_BY_ID(victim)) {
				continue;
			}
			
			done_aff = FALSE;
			if (aff->location != APPLY_NONE && (apply_values[(int) aff->location] == 0.0 || aff->modifier < 0)) {
				affect_remove(victim, aff);
				done_aff = TRUE;
			}
			if (!done_aff && (bitv = aff->bitvector) != NOBITS) {
				// check each bit
				for (pos = 0; bitv && !done_aff; ++pos, bitv >>= 1) {
					if (IS_SET(bitv, BIT(0)) && aff_is_bad[pos]) {
						affect_remove(victim, aff);
						done_aff = TRUE;
					}
				}
			}
		}
	}
	else {
		script_log("%s script_heal: Invalid thing to heal: %s", log_root, what_arg);
	}
}


/**
* %mod% <variable> <field> <value>
*
* This function allows scripts to modify a mob/object/room/vehicle.
*
* @param char *argument Expected to be: <variable> <field> <value>
*/
void script_modify(char *argument) {
	void format_text(char **ptr_string, int mode, descriptor_data *d, unsigned int maxlen);
	extern char *get_room_description(room_data *room);
	extern vehicle_data *get_vehicle(char *name);
	extern bool world_map_needs_save;
	
	char targ_arg[MAX_INPUT_LENGTH], field_arg[MAX_INPUT_LENGTH], value[MAX_INPUT_LENGTH], temp[MAX_STRING_LENGTH];
	vehicle_data *veh = NULL, *v_proto;
	char_data *mob = NULL, *m_proto;
	obj_data *obj = NULL, *o_proto;
	room_data *room = NULL;
	bool clear;
	
	half_chop(argument, targ_arg, temp);
	half_chop(temp, field_arg, value);
	
	if (!*targ_arg || !*field_arg || !*value) {
		script_log("%%mod%% called without %s arguments", (!*targ_arg ? "any" : (!*field_arg ? "field and value" : "value")));
		return;
	}
	if (*targ_arg != UID_CHAR) {
		script_log("%%mod%% requires a variable for the target, got '%s' instead", targ_arg);
		return;
	}
	
	// this indicates we're clearing a field and setting it back to the prototype
	clear = !str_cmp(value, "-") || !str_cmp(value, "--");
	
	// CHARACTER MODE
	if ((mob = get_char(targ_arg))) {
		m_proto = IS_NPC(mob) ? mob_proto(GET_MOB_VNUM(mob)) : NULL;
		mob->customized = TRUE;	// triggers string saving
		
		if (!IS_NPC(mob)) {
			script_log("%%mod%% cannot target a player");
		}
		else if (is_abbrev(field_arg, "keywords")) {
			if (GET_PC_NAME(mob) && (!m_proto || GET_PC_NAME(mob) != GET_PC_NAME(m_proto))) {
				free(GET_PC_NAME(mob));
			}
			GET_PC_NAME(mob) = clear ? (m_proto ? GET_PC_NAME(m_proto) : str_dup("ERROR")) : str_dup(value);
		}
		else if (is_abbrev(field_arg, "longdescription")) {
			if (GET_LONG_DESC(mob) && (!m_proto || GET_LONG_DESC(mob) != GET_LONG_DESC(m_proto))) {
				free(GET_LONG_DESC(mob));
			}
			strcat(value, "\r\n");	// required by long descs
			GET_LONG_DESC(mob) = clear ? (m_proto ? GET_LONG_DESC(m_proto) : str_dup("ERROR")) : str_dup(value);
		}
		else if (is_abbrev(field_arg, "shortdescription")) {
			if (GET_SHORT_DESC(mob) && (!m_proto || GET_SHORT_DESC(mob) != GET_SHORT_DESC(m_proto))) {
				free(GET_SHORT_DESC(mob));
			}
			GET_SHORT_DESC(mob) = clear ? (m_proto ? GET_SHORT_DESC(m_proto) : str_dup("ERROR")) : str_dup(value);
		}
		else {
			script_log("%%mod%% called with invalid mob field '%s'", field_arg);
		}
	}
	// OBJECT MODE
	else if ((obj = get_obj(targ_arg))) {
		o_proto = obj_proto(GET_OBJ_VNUM(obj));
		
		if (is_abbrev(field_arg, "keywords")) {
			if (GET_OBJ_KEYWORDS(obj) && (!o_proto || GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(o_proto))) {
				free(GET_OBJ_KEYWORDS(obj));
			}
			GET_OBJ_KEYWORDS(obj) = clear ? (o_proto ? GET_OBJ_KEYWORDS(o_proto) : str_dup("ERROR")) : str_dup(value);
		}
		else if (is_abbrev(field_arg, "longdescription")) {
			if (GET_OBJ_LONG_DESC(obj) && (!o_proto || GET_OBJ_LONG_DESC(obj) != GET_OBJ_LONG_DESC(o_proto))) {
				free(GET_OBJ_LONG_DESC(obj));
			}
			GET_OBJ_LONG_DESC(obj) = clear ? (o_proto ? GET_OBJ_LONG_DESC(o_proto) : str_dup("ERROR")) : str_dup(value);
		}
		else if (is_abbrev(field_arg, "lookdescription")) {	// SETS the lookdescription
			if (GET_OBJ_ACTION_DESC(obj) && (!o_proto || GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(o_proto))) {
				free(GET_OBJ_ACTION_DESC(obj));
			}
			strcat(value, "\r\n");
			GET_OBJ_ACTION_DESC(obj) = clear ? (o_proto ? GET_OBJ_ACTION_DESC(o_proto) : str_dup("")) : str_dup(value);
			if (GET_OBJ_ACTION_DESC(obj) && (!o_proto || GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(o_proto))) {
				format_text(&GET_OBJ_ACTION_DESC(obj), (strlen(GET_OBJ_ACTION_DESC(obj)) > 80 ? FORMAT_INDENT : 0), NULL, MAX_STRING_LENGTH);
			}
		}
		else if (is_abbrev(field_arg, "append-lookdescription")) {	// ADDS TO THE END OF the lookdescription
			if (strlen(NULLSAFE(GET_OBJ_ACTION_DESC(obj))) + strlen(value) + 2 > MAX_ITEM_DESCRIPTION) {
				script_log("%%mod%% append-description: obj lookdescription length is too long (%d max)", MAX_ITEM_DESCRIPTION);
			}
			else {
				snprintf(temp, sizeof(temp), "%s%s\r\n", NULLSAFE(GET_OBJ_ACTION_DESC(obj)), value);
				if (GET_OBJ_ACTION_DESC(obj) && (!o_proto || GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(o_proto))) {
					free(GET_OBJ_ACTION_DESC(obj));
				}
				GET_OBJ_ACTION_DESC(obj) = str_dup(temp);
				if (GET_OBJ_ACTION_DESC(obj) && (!o_proto || GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(o_proto))) {
					format_text(&GET_OBJ_ACTION_DESC(obj), (strlen(GET_OBJ_ACTION_DESC(obj)) > 80 ? FORMAT_INDENT : 0), NULL, MAX_STRING_LENGTH);
				}
			}
		}
		else if (is_abbrev(field_arg, "append-lookdesc-noformat")) {	// ADDS TO THE END OF the lookdescription without formatting
			if (strlen(NULLSAFE(GET_OBJ_ACTION_DESC(obj))) + strlen(value) + 2 > MAX_ITEM_DESCRIPTION) {
				script_log("%%mod%% append-description: obj lookdescription length is too long (%d max)", MAX_ITEM_DESCRIPTION);
			}
			else {
				snprintf(temp, sizeof(temp), "%s%s\r\n", NULLSAFE(GET_OBJ_ACTION_DESC(obj)), value);
				if (GET_OBJ_ACTION_DESC(obj) && (!o_proto || GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(o_proto))) {
					free(GET_OBJ_ACTION_DESC(obj));
				}
				GET_OBJ_ACTION_DESC(obj) = str_dup(temp);
			}
		}
		else if (is_abbrev(field_arg, "shortdescription")) {
			if (GET_OBJ_SHORT_DESC(obj) && (!o_proto || GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(o_proto))) {
				free(GET_OBJ_SHORT_DESC(obj));
			}
			GET_OBJ_SHORT_DESC(obj) = clear ? (o_proto ? GET_OBJ_SHORT_DESC(o_proto) : str_dup("ERROR")) : str_dup(value);
		}
		else {
			script_log("%%mod%% called with invalid obj field '%s'", field_arg);
		}
	}
	// ROOM MODE
	else if ((room = get_room(NULL, targ_arg))) {
		if (GET_ROOM_VNUM(room) < MAP_SIZE) {
			world_map_needs_save = TRUE;
		}
		
		if (SHARED_DATA(room) == &ocean_shared_data) {
			script_log("%%mod%% cannot be used on Ocean rooms");
		}
		else if (is_abbrev(field_arg, "name") || is_abbrev(field_arg, "title")) {
			if (ROOM_CUSTOM_NAME(room)) {
				free(ROOM_CUSTOM_NAME(room));
			}
			ROOM_CUSTOM_NAME(room) = clear ? NULL : str_dup(value);
		}
		else if (is_abbrev(field_arg, "description")) {	// SETS the description
			if (ROOM_CUSTOM_DESCRIPTION(room)) {
				free(ROOM_CUSTOM_DESCRIPTION(room));
			}
			strcat(value, "\r\n");
			ROOM_CUSTOM_DESCRIPTION(room) = clear ? NULL : str_dup(value);
			if (ROOM_CUSTOM_DESCRIPTION(room)) {
				format_text(&ROOM_CUSTOM_DESCRIPTION(room), (strlen(ROOM_CUSTOM_DESCRIPTION(room)) > 80 ? FORMAT_INDENT : 0), NULL, MAX_STRING_LENGTH);
			}
		}
		else if (is_abbrev(field_arg, "append-description")) {	// ADDS TO THE END OF the description
			if (strlen(NULLSAFE(ROOM_CUSTOM_DESCRIPTION(room))) + strlen(value) + 2 > MAX_ROOM_DESCRIPTION) {
				script_log("%%mod%% append-description: description length is too long (%d max)", MAX_ROOM_DESCRIPTION);
			}
			else {
				snprintf(temp, sizeof(temp), "%s%s\r\n", ROOM_CUSTOM_DESCRIPTION(room) ? ROOM_CUSTOM_DESCRIPTION(room) : get_room_description(room), value);
				if (ROOM_CUSTOM_DESCRIPTION(room)) {
					free(ROOM_CUSTOM_DESCRIPTION(room));
				}
				ROOM_CUSTOM_DESCRIPTION(room) = str_dup(temp);
				format_text(&ROOM_CUSTOM_DESCRIPTION(room), (strlen(ROOM_CUSTOM_DESCRIPTION(room)) > 80 ? FORMAT_INDENT : 0), NULL, MAX_STRING_LENGTH);
			}
		}
		else if (is_abbrev(field_arg, "append-desc-noformat")) {	// ADDS TO THE END OF the description
			if (strlen(NULLSAFE(ROOM_CUSTOM_DESCRIPTION(room))) + strlen(value) + 2 > MAX_ROOM_DESCRIPTION) {
				script_log("%%mod%% append-description: description length is too long (%d max)", MAX_ROOM_DESCRIPTION);
			}
			else {
				snprintf(temp, sizeof(temp), "%s%s\r\n", ROOM_CUSTOM_DESCRIPTION(room) ? ROOM_CUSTOM_DESCRIPTION(room) : get_room_description(room), value);
				if (ROOM_CUSTOM_DESCRIPTION(room)) {
					free(ROOM_CUSTOM_DESCRIPTION(room));
				}
				ROOM_CUSTOM_DESCRIPTION(room) = str_dup(temp);
			}
		}
		else {
			script_log("%%mod%% called with invalid room field '%s'", field_arg);
		}
	}
	// VEHICLE MODE
	else if ((veh = get_vehicle(targ_arg))) {
		v_proto = vehicle_proto(VEH_VNUM(veh));
		
		if (is_abbrev(field_arg, "keywords")) {
			if (VEH_KEYWORDS(veh) && (!v_proto || VEH_KEYWORDS(veh) != VEH_KEYWORDS(v_proto))) {
				free(VEH_KEYWORDS(veh));
			}
			VEH_KEYWORDS(veh) = clear ? (v_proto ? VEH_KEYWORDS(v_proto) : str_dup("ERROR")) : str_dup(value);
		}
		else if (is_abbrev(field_arg, "longdescription")) {
			if (VEH_LONG_DESC(veh) && (!v_proto || VEH_LONG_DESC(veh) != VEH_LONG_DESC(v_proto))) {
				free(VEH_LONG_DESC(veh));
			}
			VEH_LONG_DESC(veh) = clear ? (v_proto ? VEH_LONG_DESC(v_proto) : str_dup("ERROR")) : str_dup(value);
		}
		else if (is_abbrev(field_arg, "lookdescription")) {	// SETS the lookdescription
			if (VEH_LOOK_DESC(veh) && (!v_proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(v_proto))) {
				free(VEH_LOOK_DESC(veh));
			}
			strcat(value, "\r\n");
			VEH_LOOK_DESC(veh) = clear ? (v_proto ? VEH_LOOK_DESC(v_proto) : str_dup("")) : str_dup(value);
			if (VEH_LOOK_DESC(veh) && (!v_proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(v_proto))) {
				format_text(&VEH_LOOK_DESC(veh), (strlen(VEH_LOOK_DESC(veh)) > 80 ? FORMAT_INDENT : 0), NULL, MAX_STRING_LENGTH);
			}
		}
		else if (is_abbrev(field_arg, "append-lookdescription")) {	// ADDS TO THE END OF the lookdescription
			if (strlen(NULLSAFE(VEH_LOOK_DESC(veh))) + strlen(value) + 2 > MAX_ITEM_DESCRIPTION) {
				script_log("%%mod%% append-description: vehicle lookdescription length is too long (%d max)", MAX_ITEM_DESCRIPTION);
			}
			else {
				snprintf(temp, sizeof(temp), "%s%s\r\n", NULLSAFE(VEH_LOOK_DESC(veh)), value);
				if (VEH_LOOK_DESC(veh) && (!v_proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(v_proto))) {
					free(VEH_LOOK_DESC(veh));
				}
				VEH_LOOK_DESC(veh) = str_dup(temp);
				if (VEH_LOOK_DESC(veh) && (!v_proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(v_proto))) {
					format_text(&VEH_LOOK_DESC(veh), (strlen(VEH_LOOK_DESC(veh)) > 80 ? FORMAT_INDENT : 0), NULL, MAX_STRING_LENGTH);
				}
			}
		}
		else if (is_abbrev(field_arg, "append-lookdesc-noformat")) {	// ADDS TO THE END OF the lookdescription
			if (strlen(NULLSAFE(VEH_LOOK_DESC(veh))) + strlen(value) + 2 > MAX_ITEM_DESCRIPTION) {
				script_log("%%mod%% append-description: vehicle lookdescription length is too long (%d max)", MAX_ITEM_DESCRIPTION);
			}
			else {
				snprintf(temp, sizeof(temp), "%s%s\r\n", NULLSAFE(VEH_LOOK_DESC(veh)), value);
				if (VEH_LOOK_DESC(veh) && (!v_proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(v_proto))) {
					free(VEH_LOOK_DESC(veh));
				}
				VEH_LOOK_DESC(veh) = str_dup(temp);
			}
		}
		else if (is_abbrev(field_arg, "shortdescription")) {
			if (VEH_SHORT_DESC(veh) && (!v_proto || VEH_SHORT_DESC(veh) != VEH_SHORT_DESC(v_proto))) {
				free(VEH_SHORT_DESC(veh));
			}
			VEH_SHORT_DESC(veh) = clear ? (v_proto ? VEH_SHORT_DESC(v_proto) : str_dup("ERROR")) : str_dup(value);
		}
		else {
			script_log("%%mod%% called with invalid vehicle field '%s'", field_arg);
		}
	}
	else {	// no target?
		script_log("%%mod%% called with invalid target");
		return;
	}
}
