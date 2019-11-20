/* ************************************************************************
*   File: dg_objcmd.c                                     EmpireMUD 2.0b5 *
*  Usage: contains the command_interpreter for objects, object commands.  *
*                                                                         *
*  DG Scripts code by galion, 1996/08/04 23:10:16, revision 3.8           *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "dg_scripts.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"

// external vars
extern const char *damage_types[];
extern const char *dirs[];
extern const char *alt_dirs[];
extern struct instance_data *quest_instance_global;

// external functions
void adjust_vehicle_tech(vehicle_data *veh, bool add);
void obj_command_interpreter(obj_data *obj, char *argument);
void send_char_pos(char_data *ch, int dam);
extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom, bool allow_fake_loc);
char_data *get_char_by_obj(obj_data *obj, char *name);
obj_data *get_obj_by_obj(obj_data *obj, char *name);
room_data *get_room(room_data *ref, char *name);
extern vehicle_data *get_vehicle(char *name);
vehicle_data *get_vehicle_by_obj(obj_data *obj, char *name);
vehicle_data *get_vehicle_near_obj(obj_data *obj, char *name);
void instance_obj_setup(struct instance_data *inst, obj_data *obj);
void sub_write(char *arg, char_data *ch, byte find_invis, int targets);
void die(char_data *ch, char_data *killer);
void scale_item_to_level(obj_data *obj, int level);
void scale_mob_to_level(char_data *mob, int level);
void scale_vehicle_to_level(vehicle_data *veh, int level);

// locals
room_data *obj_room(obj_data *obj);


#define OCMD(name)  void (name)(obj_data *obj, char *argument, int cmd, int subcmd)


struct obj_command_info {
	char *command;
	void (*command_pointer)(obj_data *obj, char *argument, int cmd, int subcmd);
	int subcmd;
};


/* do_osend */
#define SCMD_OSEND         0
#define SCMD_OECHOAROUND   1


/**
* Determines the scale level for a obj, with a character target as a backup
* for unscaled objects.
*
* @param obj_data *obj The object to check.
* @param char_data *targ Optional: A character target.
* @return int A scale level.
*/
int get_obj_scale_level(obj_data *obj, char_data *targ) {
	struct instance_data *inst;
	int level = 1;
	room_data *orm = obj_room(obj);
	
	if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) > 0) {
		level = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
	}
	else if (orm && COMPLEX_DATA(orm) && (inst = COMPLEX_DATA(orm)->instance)) {
		if (INST_LEVEL(inst)) {
			level = INST_LEVEL(inst);
		}
		else if (GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)) > 0) {
			level = GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst));
		}
		else if (GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)) > 0) {
			level = GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)) / 2; // average?
		}
	}
	
	if (!level && targ) {
		// backup
		level = get_approximate_level(targ);
	}
	
	return level;
}


/* attaches object name and vnum to msg and sends it to script_log */
void obj_log(obj_data *obj, const char *format, ...) {
	va_list args;
	char output[MAX_STRING_LENGTH];

	snprintf(output, sizeof(output), "Obj (%s, VNum %d):: %s", GET_OBJ_SHORT_DESC(obj), GET_OBJ_VNUM(obj), format);

	va_start(args, format);
	script_vlog(output, args);
	va_end(args);
}

/**
* @param obj_data *obj The item.
* @return room_data* the real room that the object or object's carrier is in
*/
room_data *obj_room(obj_data *obj) {
	if (IN_ROOM(obj))
		return IN_ROOM(obj);
	else if (obj->in_vehicle) {
		return IN_ROOM(obj->in_vehicle);
	}
	else if (obj->carried_by)
		return IN_ROOM(obj->carried_by);
	else if (obj->worn_by)
		return IN_ROOM(obj->worn_by);
	else if (obj->in_obj)
		return obj_room(obj->in_obj);
	else
		return NULL;
}


/* obj_data *commands */

OCMD(do_oadventurecomplete) {
	void mark_instance_completed(struct instance_data *inst);
	
	struct instance_data *inst;
	room_data *room = obj_room(obj);
	
	inst = quest_instance_global;
	if (!inst) {
		inst = room ? find_instance_by_room(room, FALSE, TRUE) : NULL;
	}
	
	if (inst) {
		mark_instance_completed(inst);
	}
}


OCMD(do_obuild) {
	void do_dg_build(room_data *target, char *argument);

	char loc_arg[MAX_INPUT_LENGTH], bld_arg[MAX_INPUT_LENGTH], *tmp;
	room_data *orm = obj_room(obj), *target;
	
	tmp = any_one_word(argument, loc_arg);
	strcpy(bld_arg, tmp);
	
	// usage: %build% [location] <vnum [dir] | ruin | demolish>
	if (!*loc_arg) {
		obj_log(obj, "obuild: bad syntax");
		return;
	}
	
	// check number of args
	if (!*bld_arg) {
		// only arg is actually building arg
		strcpy(bld_arg, argument);
		target = orm;
	}
	else {
		// two arguments
		target = get_room(orm, loc_arg);
	}
	
	if (!target) {
		obj_log(obj, "obuild: target is an invalid room");
		return;
	}
	
	// places you just can't build -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	// good to go
	do_dg_build(target, bld_arg);
}


OCMD(do_oecho) {
	room_data *room;

	skip_spaces(&argument);

	if (!*argument) 
		obj_log(obj, "oecho called with no args");
	else if ((room = obj_room(obj))) {
		if (ROOM_PEOPLE(room))
			sub_write(argument, ROOM_PEOPLE(room), TRUE, TO_ROOM | TO_CHAR);
	}
	else
		obj_log(obj, "oecho called by object in no location");
}


OCMD(do_oforce) {
	char_data *ch, *next_ch;
	room_data *room;
	char arg1[MAX_INPUT_LENGTH], *line;

	line = one_argument(argument, arg1);

	if (!*arg1 || !*line) {
		obj_log(obj, "oforce called with too few args");
		return;
	}

	if (!str_cmp(arg1, "all")) {
		if (!(room = obj_room(obj))) 
			obj_log(obj, "oforce called by object in no location");
		else {
			for (ch = ROOM_PEOPLE(room); ch; ch = next_ch) {
				next_ch = ch->next_in_room;
				if (valid_dg_target(ch, 0)) {
					command_interpreter(ch, line);
				}
			}
		}      
	}
	else {
		if ((ch = get_char_by_obj(obj, arg1))) {
			if (valid_dg_target(ch, 0)) {
				command_interpreter(ch, line);
			}
		}
		else {
			obj_log(obj, "oforce: no target found");
		}
	}
}


OCMD(do_oheal) {
	script_heal(obj, OBJ_TRIGGER, argument);
}


OCMD(do_obuildingecho) {
	room_data *froom, *home_room, *iter;
	room_data *orm = obj_room(obj);
	char room_number[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH], *msg;

	msg = any_one_word(argument, room_number);
	skip_spaces(&msg);

	if (!*room_number || !*msg) {
		obj_log(obj, "obuildingecho called with too few args");
	}
	else if (!(froom = get_room(orm, room_number))) {
		obj_log(obj, "obuildingecho called with invalid target");
	}
	else {
		home_room = HOME_ROOM(froom);	// right?
		
		sprintf(buf, "%s\r\n", msg);
		
		send_to_room(buf, home_room);
		for (iter = interior_room_list; iter; iter = iter->next_interior) {
			if (HOME_ROOM(iter) == home_room && iter != home_room && ROOM_PEOPLE(iter)) {
				send_to_room(buf, iter);
			}
		}
	}
}


OCMD(do_oregionecho) {
	char room_number[MAX_INPUT_LENGTH], radius_arg[MAX_INPUT_LENGTH], *msg;
	descriptor_data *desc;
	room_data *center, *orm = obj_room(obj);
	char_data *targ;
	int radius;
	bool indoor_only = FALSE;

	argument = any_one_word(argument, room_number);
	msg = one_argument(argument, radius_arg);
	skip_spaces(&msg);

	if (!*room_number || !*radius_arg || !*msg) {
		obj_log(obj, "oregionecho called with too few args");
	}
	else if (!isdigit(*radius_arg) && *radius_arg != '-') {
		obj_log(obj, "oregionecho called with invalid radius");
	}
	else if (!(center = get_room(orm, room_number))) {
		obj_log(obj, "oregionecho called with invalid target");
	}
	else {
		center = GET_MAP_LOC(center) ? real_room(GET_MAP_LOC(center)->vnum) : NULL;
		radius = atoi(radius_arg);
		if (radius < 0) {
			radius = -radius;
			indoor_only = TRUE;
		}
		
		if (center) {			
			for (desc = descriptor_list; desc; desc = desc->next) {
				if (STATE(desc) != CON_PLAYING || !(targ = desc->character)) {
					continue;
				}
				if (NO_LOCATION(IN_ROOM(targ))) {
					continue;
				}
				if (compute_distance(center, IN_ROOM(targ)) > radius) {
					continue;
				}
				
				if (!indoor_only || IS_OUTDOORS(targ)) {
					msg_to_desc(desc, "%s\r\n", CAP(msg));
				}
			}
		}
	}
}


OCMD(do_ovehicleecho) {
	char targ[MAX_INPUT_LENGTH], *msg;
	vehicle_data *veh;

	msg = any_one_word(argument, targ);
	skip_spaces(&msg);

	if (!*targ || !*msg) {
		obj_log(obj, "ovehicleecho called with too few args");
	}
	else if (!(veh = get_vehicle_near_obj(obj, targ))) {
		obj_log(obj, "ovehicleecho called with invalid target");
	}
	else {
		msg_to_vehicle(veh, FALSE, "%s\r\n", msg);
	}
}


// prints the message to everyone except two targets
OCMD(do_oechoneither) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	char_data *vict1, *vict2, *iter;
	char *p;

	p = two_arguments(argument, arg1, arg2);
	skip_spaces(&p);

	if (!*arg1 || !*arg2 || !*p) {
		obj_log(obj, "oechoneither called with missing arguments");
		return;
	}
	
	if (!(vict1 = get_char_by_obj(obj, arg1))) {
		obj_log(obj, "oechoneither: vict 1 (%s) does not exist", arg1);
		return;
	}
	if (!(vict2 = get_char_by_obj(obj, arg2))) {
		obj_log(obj, "oechoneither: vict 2 (%s) does not exist", arg2);
		return;
	}
	
	for (iter = ROOM_PEOPLE(IN_ROOM(vict1)); iter; iter = iter->next_in_room) {
		if (iter->desc && iter != vict1 && iter != vict2) {
			sub_write(p, iter, TRUE, TO_CHAR);
		}
	}
}


OCMD(do_orestore) {
	extern const bool aff_is_bad[];
	extern const double apply_values[];
	
	struct affected_type *aff, *next_aff;
	char arg[MAX_INPUT_LENGTH];
	vehicle_data *veh = NULL;
	char_data *victim = NULL;
	obj_data *otarg = NULL;
	room_data *room = NULL, *orm;
	bitvector_t bitv;
	bool done_aff;
	int pos;
	
	orm = obj_room(obj);
	one_argument(argument, arg);
	
	// find a target
	if (!*arg) {
		otarg = obj;
	}
	else if (!str_cmp(arg, "room") || !str_cmp(arg, "building")) {
		room = orm;
	}
	else if ((*arg == UID_CHAR && (victim = get_char(arg))) || (victim = get_char_by_obj(obj, arg))) {
		// found victim
	}
	else if ((*arg == UID_CHAR && (veh = get_vehicle(arg))) || (veh = get_vehicle_near_obj(obj, arg))) {
		// found vehicle
		if (!VEH_IS_COMPLETE(veh)) {
			obj_log(obj, "orestore: used on unfinished vehicle");
			return;
		}
	}
	else if ((*arg == UID_CHAR && (otarg = get_obj(arg))) || (otarg = get_obj_by_obj(obj, arg))) {
		// found obj
	}
	else if ((room = get_room(orm, arg))) {
		// found room
	}
	else {
		// bad arg
		obj_log(obj, "orestore: bad argument");
		return;
	}
	
	if (room) {
		room = HOME_ROOM(room);
		if (!IS_COMPLETE(room)) {
			obj_log(obj, "orestore: used on unfinished building");
			return;
		}
	}
	
	// perform the restoration
	if (victim) {
		while (victim->over_time_effects) {
			dot_remove(victim, victim->over_time_effects);
		}
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
		if (GET_POS(victim) < POS_SLEEPING) {
			GET_POS(victim) = POS_STANDING;
		}
		GET_HEALTH(victim) = GET_MAX_HEALTH(victim);
		GET_MOVE(victim) = GET_MAX_MOVE(victim);
		GET_MANA(victim) = GET_MAX_MANA(victim);
		GET_BLOOD(victim) = GET_MAX_BLOOD(victim);
	}
	if (otarg) {
		// not sure what to do for objs
	}
	if (veh) {
		free_resource_list(VEH_NEEDS_RESOURCES(veh));
		VEH_NEEDS_RESOURCES(veh) = NULL;
		VEH_HEALTH(veh) = VEH_MAX_HEALTH(veh);
		REMOVE_BIT(VEH_FLAGS(veh), VEH_ON_FIRE);
	}
	if (room) {
		if (COMPLEX_DATA(room)) {
			free_resource_list(GET_BUILDING_RESOURCES(room));
			GET_BUILDING_RESOURCES(room) = NULL;
			COMPLEX_DATA(room)->damage = 0;
			COMPLEX_DATA(room)->burn_down_time = 0;
		}
	}
}


OCMD(do_osend) {
	char buf[MAX_INPUT_LENGTH], *msg;
	char_data *ch;

	msg = any_one_arg(argument, buf);

	if (!*buf) {
		obj_log(obj, "osend called with no args");
		return;
	}

	skip_spaces(&msg);

	if (!*msg) {
		obj_log(obj, "osend called without a message");
		return;
	}

	if ((ch = get_char_by_obj(obj, buf))) {
		if (subcmd == SCMD_OSEND)
			sub_write(msg, ch, TRUE, TO_CHAR);
		else if (subcmd == SCMD_OECHOAROUND)
			sub_write(msg, ch, TRUE, TO_ROOM);
	}
	else
		obj_log(obj, "no target found for osend");
}


/* set the object's timer value */
OCMD(do_otimer) {
	char arg[MAX_INPUT_LENGTH];

	one_argument(argument, arg);

	if (!*arg)
		obj_log(obj, "otimer: missing argument");
	else if (!isdigit(*arg)) 
		obj_log(obj, "otimer: bad argument");
	else
		GET_OBJ_TIMER(obj) = atoi(arg);
}


/* transform into a different object */
/* note: this shouldn't be used with containers unless both objects */
/* are containers! */
OCMD(do_otransform) {
	char arg[MAX_INPUT_LENGTH];
	obj_data *o;
	obj_data tmpobj;
	char_data *wearer = NULL;
	int pos = 0;

	one_argument(argument, arg);

	if (TRUE) {
		obj_log(obj, "otransform is currently disabled");
	}
	else if (!*arg)
		obj_log(obj, "otransform: missing argument");
	else if (!isdigit(*arg)) 
		obj_log(obj, "otransform: bad argument");
	else {
		o = read_object(atoi(arg), TRUE);
		if (o == NULL) {
			obj_log(obj, "otransform: bad object vnum");
			return;
		}

		if (obj->worn_by) {
			pos = obj->worn_on;
			wearer = obj->worn_by;
			unequip_char(obj->worn_by, pos);
		}

		/* move new obj info over to old object and delete new obj */
		memcpy(&tmpobj, o, sizeof(*o));
		tmpobj.in_room = IN_ROOM(obj);
		tmpobj.last_empire_id = obj->last_empire_id;
		tmpobj.last_owner_id = obj->last_owner_id;
		tmpobj.stolen_timer = obj->stolen_timer;
		tmpobj.stolen_from = obj->stolen_from;
		tmpobj.autostore_timer = obj->autostore_timer;
		tmpobj.carried_by = obj->carried_by;
		tmpobj.in_vehicle = obj->in_vehicle;
		tmpobj.worn_by = obj->worn_by;
		tmpobj.worn_on = obj->worn_on;
		tmpobj.in_obj = obj->in_obj;
		tmpobj.contains = obj->contains;
		tmpobj.script_id = obj->script_id;
		tmpobj.proto_script = obj->proto_script;
		tmpobj.script = obj->script;
		tmpobj.next_content = obj->next_content;
		tmpobj.next = obj->next;
		memcpy(obj, &tmpobj, sizeof(*obj));

		if (wearer) {
			equip_char(wearer, obj, pos);
		}

		extract_obj(o);
	}
}


OCMD(do_omod) {
	void script_modify(char *argument);
	script_modify(argument);
}


OCMD(do_omorph) {
	char tar_arg[MAX_INPUT_LENGTH], num_arg[MAX_INPUT_LENGTH];
	morph_data *morph = NULL;
	char_data *vict;
	bool normal;
	
	two_arguments(argument, tar_arg, num_arg);
	normal = !str_cmp(num_arg, "normal");
	
	if (!*tar_arg || !*num_arg) {
		obj_log(obj, "omorph: missing argument(s)");
	}
	else if (!(vict = get_char_by_obj(obj, tar_arg))) {
		obj_log(obj, "omorph: invalid target '%s'", tar_arg);
	}
	else if (!normal && (!isdigit(*num_arg) || !(morph = morph_proto(atoi(num_arg))))) {
		obj_log(obj, "omorph: invalid morph '%s'", num_arg);
	}
	else if (morph && MORPH_FLAGGED(morph, MORPHF_IN_DEVELOPMENT)) {
		obj_log(obj, "omorph: morph %d set in-development", MORPH_VNUM(morph));
	}
	else {
		perform_morph(vict, morph);
	}
}


OCMD(do_oown) {
	void do_dg_own(empire_data *emp, char_data *vict, obj_data *obj, room_data *room, vehicle_data *veh);
	
	char type_arg[MAX_INPUT_LENGTH], targ_arg[MAX_INPUT_LENGTH], emp_arg[MAX_INPUT_LENGTH];
	room_data *orm = obj_room(obj);
	vehicle_data *vtarg = NULL;
	empire_data *emp = NULL;
	char_data *vict = NULL;
	room_data *rtarg = NULL;
	obj_data *otarg = NULL;
	
	*emp_arg = '\0';	// just in case
	
	if (!orm) {
		obj_log(obj, "oown: Object nowhere when trying to oown");
		return;
	}
	
	// first arg -- possibly a type
	argument = one_argument(argument, type_arg);
	if (is_abbrev(type_arg, "room")) {
		argument = one_argument(argument, targ_arg);
		skip_spaces(&argument);
		
		if (!*targ_arg) {
			obj_log(obj, "oown: Too few arguments (oown room)");
			return;
		}
		else if (!*argument) {
			// this was the last arg
			strcpy(emp_arg, targ_arg);
		}
		else if (!(rtarg = get_room(orm, targ_arg))) {
			obj_log(obj, "oown: Invalid room target");
			return;
		}
		else {
			// room is set; remaining arg is owner
			strcpy(emp_arg, argument);
		}
	}
	else if (is_abbrev(type_arg, "mobile")) {
		argument = one_argument(argument, targ_arg);
		skip_spaces(&argument);
		strcpy(emp_arg, argument);	// always
		
		if (!*targ_arg || !*emp_arg) {
			obj_log(obj, "oown: Too few arguments (oown mob)");
			return;
		}
		else if (!(vict = ((*targ_arg == UID_CHAR) ? get_char(targ_arg) : get_char_near_obj(obj, targ_arg)))) {
			obj_log(obj, "oown: Invalid mob target");
			return;
		}
	}
	else if (is_abbrev(type_arg, "vehicle")) {
		argument = one_argument(argument, targ_arg);
		skip_spaces(&argument);
		strcpy(emp_arg, argument);	// always
		
		if (!*targ_arg || !*emp_arg) {
			obj_log(obj, "oown: Too few arguments (oown vehicle)");
			return;
		}
		else if (!(vtarg = ((*targ_arg == UID_CHAR) ? get_vehicle(targ_arg) : get_vehicle_near_obj(obj, targ_arg)))) {
			obj_log(obj, "oown: Invalid vehicle target");
			return;
		}
	}
	else if (is_abbrev(type_arg, "object")) {
		argument = one_argument(argument, targ_arg);
		skip_spaces(&argument);
		strcpy(emp_arg, argument);	// always
		
		if (!*targ_arg || !*emp_arg) {
			obj_log(obj, "oown: Too few arguments (oown obj)");
			return;
		}
		else if (!(otarg = ((*targ_arg == UID_CHAR) ? get_obj(targ_arg) : get_obj_near_obj(obj, targ_arg)))) {
			obj_log(obj, "oown: Invalid obj target");
			return;
		}
	}
	else {	// attempt to find a target
		strcpy(targ_arg, type_arg);	// there was no type
		skip_spaces(&argument);
		strcpy(emp_arg, argument);
		
		if (!*targ_arg) {
			obj_log(obj, "oown: Too few arguments");
			return;
		}
		else if (*targ_arg == UID_CHAR && ((vict = get_char(targ_arg)) || (vtarg = get_vehicle(targ_arg)) || (otarg = get_obj(targ_arg)) || (rtarg = get_room(orm, targ_arg)))) {
			// found by uid
		}
		else if ((vict = get_char_near_obj(obj, targ_arg)) || (vtarg = get_vehicle_near_obj(obj, targ_arg)) || (otarg = get_obj_near_obj(obj, targ_arg))) {
			// found by name
		}
		else {
			obj_log(obj, "oown: Invalid target");
			return;
		}
	}
	
	// only change owner on the home room
	if (rtarg) {
		rtarg = HOME_ROOM(rtarg);
	}
	
	// check that we got a target
	if (!vict && !vtarg && !rtarg && !otarg) {
		obj_log(obj, "oown: Unable to find a target");
		return;
	}
	
	// process the empire
	if (!*emp_arg) {
		obj_log(obj, "oown: No empire argument given");
		return;
	}
	else if (!str_cmp(emp_arg, "none")) {
		emp = NULL;	// set owner to none
	}
	else if (!(emp = get_empire(emp_arg))) {
		obj_log(obj, "oown: Invalid empire");
		return;
	}
	
	// final target checks
	if (vict && !IS_NPC(vict)) {
		obj_log(obj, "oown: Attempting to change the empire of a player");
		return;
	}
	if (rtarg && IS_ADVENTURE_ROOM(rtarg)) {
		obj_log(obj, "oown: Attempting to change ownership of an adventure room");
		return;
	}
	
	// do the ownership change
	do_dg_own(emp, vict, otarg, rtarg, vtarg);
}


/* purge all objects an npcs in room, or specified object or mob */
OCMD(do_opurge) {
	char arg[MAX_INPUT_LENGTH];
	char_data *ch, *next_ch;
	obj_data *o, *next_obj;
	vehicle_data *veh;
	room_data *rm;

	argument = one_argument(argument, arg);
	skip_spaces(&argument);

	if (!*arg) {
		/* purge all */
		if ((rm = obj_room(obj))) {
			for (ch = ROOM_PEOPLE(rm); ch; ch = next_ch ) {
				next_ch = ch->next_in_room;
				if (IS_NPC(ch))
					extract_char(ch);
			}

			for (o = ROOM_CONTENTS(rm); o; o = next_obj ) {
				next_obj = o->next_content;
				if (o != obj)
					extract_obj(o);
			}
		}

		return;
	} /* no arg */
	
	
	// purge all mobs/objs in an instance
	if (!str_cmp(arg, "instance")) {
		room_data *room = obj_room(obj);
		struct instance_data *inst = quest_instance_global;
		if (!inst) {
			inst = room ? find_instance_by_room(room, FALSE, TRUE) : NULL;
		}
		
		if (!inst) {
			obj_log(obj, "opurge: obj using purge instance outside an instance");
			return;
		}
		dg_purge_instance(obj, inst, argument);
	}
	// purge char
	else if ((ch = get_char_by_obj(obj, arg))) {
		if (!IS_NPC(ch)) {
			obj_log(obj, "opurge: purging a PC");
			return;
		}

		if (*argument) {
			act(argument, TRUE, ch, NULL, NULL, TO_ROOM);
		}
		extract_char(ch);
	}
	// purge vehicle
	else if ((veh = get_vehicle_by_obj(obj, arg))) {
		if (*argument) {
			act(argument, TRUE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM);
		}
		extract_vehicle(veh);
	}
	// purge obj
	else if ((o = get_obj_by_obj(obj, arg))) {
		if (o == obj) {
			dg_owner_purged = 1;
		}
		if (*argument) {
			room_data *room = obj_room(o);
			act(argument, TRUE, room ? ROOM_PEOPLE(room) : NULL, o, NULL, TO_CHAR | TO_ROOM);
		}
		extract_obj(o);
	}
	else {
		obj_log(obj, "opurge: bad argument");
	}
}


// quest commands
OCMD(do_oquest) {
	void do_dg_quest(int go_type, void *go, char *argument);	
	do_dg_quest(OBJ_TRIGGER, obj, argument);
}


OCMD(do_osiege) {
	void besiege_room(char_data *attacker, room_data *to_room, int damage, vehicle_data *by_vehicle);
	extern bool besiege_vehicle(char_data *attacker, vehicle_data *veh, int damage, int siege_type, vehicle_data *by_vehicle);
	extern room_data *dir_to_room(room_data *room, int dir, bool ignore_entrance);
	extern bool find_siege_target_for_vehicle(char_data *ch, vehicle_data *veh, char *arg, room_data **room_targ, int *dir, vehicle_data **veh_targ);
	extern bool validate_siege_target_room(char_data *ch, vehicle_data *veh, room_data *to_room);
	
	char scale_arg[MAX_INPUT_LENGTH], tar_arg[MAX_INPUT_LENGTH];
	vehicle_data *veh_targ = NULL;
	room_data *orm, *room_targ = NULL;
	int dam, dir, scale = -1;
	
	two_arguments(argument, tar_arg, scale_arg);
	
	if (!*tar_arg) {
		obj_log(obj, "osiege called with no args");
		return;
	}
	// determine scale level if provided
	if (*scale_arg && (!isdigit(*scale_arg) || (scale = atoi(scale_arg)) < 0)) {
		obj_log(obj, "osiege called with invalid scale level '%s'", scale_arg);
		return;
	}
	
	// find a target
	if (!veh_targ && !room_targ && *tar_arg == UID_CHAR) {
		room_targ = find_room(atoi(tar_arg+1));
	}
	if (!veh_targ && !room_targ && *tar_arg == UID_CHAR) {
		veh_targ = get_vehicle(tar_arg);
	}
	if (!veh_targ && !room_targ && (orm = obj_room(obj))) {
		if ((dir = search_block(tar_arg, dirs, FALSE)) != NOTHING || (dir = search_block(tar_arg, alt_dirs, FALSE)) != NOTHING) {
			room_targ = dir_to_room(orm, dir, FALSE);
		}
	}
	if (!veh_targ && !room_targ) {
		veh_targ = get_vehicle_near_obj(obj, tar_arg);
	}
	
	// seems ok
	if (scale == -1) {
		scale = get_obj_scale_level(obj, NULL);
	}
	
	dam = scale * 8 / 100;	// 8 damage per 100 levels
	dam = MAX(1, dam);	// minimum 1
	
	if (room_targ) {
		if (validate_siege_target_room(NULL, NULL, room_targ)) {
			besiege_room(NULL, room_targ, dam, NULL);
		}
	}
	else if (veh_targ) {
		besiege_vehicle(NULL, veh_targ, dam, SIEGE_PHYSICAL, NULL);
	}
	else {
		obj_log(obj, "osiege: invalid target");
	}
}


// kills the target
OCMD(do_oslay) {
	char name[MAX_INPUT_LENGTH];
	char_data *vict;
	
	argument = one_argument(argument, name);

	if (!*name) {
		obj_log(obj, "oslay: no target");
		return;
	}
	
	if (*name == UID_CHAR) {
		if (!(vict = get_char(name))) {
			obj_log(obj, "oslay: victim (%s) does not exist", name);
			return;
		}
	}
	else if (!(vict = get_char_by_obj(obj, name))) {
		obj_log(obj, "oslay: victim (%s) does not exist", name);
		return;
	}
	
	if (IS_IMMORTAL(vict)) {
		msg_to_char(vict, "Being the cool immortal you are, you sidestep a trap, obviously placed to kill you.\r\n");
	}
	else {
		die(vict, vict);
	}
}


OCMD(do_oteleport) {	
	char_data *ch, *next_ch;
	room_data *target, *orm = obj_room(obj);
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct instance_data *inst;
	vehicle_data *veh;
	obj_data *tobj;
	int iter;

	two_arguments(argument, arg1, arg2);

	if (!*arg1 || !*arg2) {
		obj_log(obj, "oteleport called with too few args");
		return;
	}

	target = get_room(orm, arg2);

	if (!target) {
		obj_log(obj, "oteleport target is an invalid room");
		return;
	}
	
	if (!str_cmp(arg1, "all")) {
		if (target == orm)
			obj_log(obj, "oteleport target is itself");

		for (ch = ROOM_PEOPLE(orm); ch; ch = next_ch) {
			next_ch = ch->next_in_room;
			if (!valid_dg_target(ch, DG_ALLOW_GODS)) 
				continue;
			char_from_room(ch);
			char_to_room(ch, target);
			if (!IS_NPC(ch)) {
				GET_LAST_DIR(ch) = NO_DIR;
			}
			enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
			qt_visit_room(ch, IN_ROOM(ch));
			msdp_update_room(ch);
		}
	}
	else if (!str_cmp(arg1, "adventure")) {
		// teleport all players in the adventure
		if (!orm || !(inst = find_instance_by_room(orm, FALSE, TRUE))) {
			obj_log(obj, "oteleport: 'adventure' mode called outside any adventure");
			return;
		}
		
		for (iter = 0; iter < INST_SIZE(inst); ++iter) {
			// only if it's not the target room, or we'd be here all day
			if (INST_ROOM(inst, iter) && INST_ROOM(inst, iter) != target) {
				for (ch = ROOM_PEOPLE(INST_ROOM(inst, iter)); ch; ch = next_ch) {
					next_ch = ch->next_in_room;
					
					if (!valid_dg_target(ch, DG_ALLOW_GODS)) {
						continue;
					}
					
					// teleport players and their followers
					if (!IS_NPC(ch) || (ch->master && !IS_NPC(ch->master))) {
						char_from_room(ch);
						char_to_room(ch, target);
						if (!IS_NPC(ch)) {
							GET_LAST_DIR(ch) = NO_DIR;
						}
						enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
						qt_visit_room(ch, IN_ROOM(ch));
						msdp_update_room(ch);
					}
				}
			}
		}
	}
	else {
		if ((ch = get_char_by_obj(obj, arg1))) {
			if (valid_dg_target(ch, DG_ALLOW_GODS)) {
				char_from_room(ch);
				char_to_room(ch, target);
				if (!IS_NPC(ch)) {
					GET_LAST_DIR(ch) = NO_DIR;
				}
				enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
				qt_visit_room(ch, IN_ROOM(ch));
				msdp_update_room(ch);
			}
		}
		else if ((veh = get_vehicle_near_obj(obj, arg1))) {
			adjust_vehicle_tech(veh, FALSE);
			vehicle_from_room(veh);
			vehicle_to_room(veh, target);
			adjust_vehicle_tech(veh, TRUE);
			entry_vtrigger(veh);
		}
		else if ((tobj = get_obj_by_obj(obj, arg1))) {
			obj_to_room(tobj, target);
		}
		else {
			obj_log(obj, "oteleport: no target found");
		}
	}
}


OCMD(do_oterracrop) {
	void do_dg_terracrop(room_data *target, crop_data *crop);

	char loc_arg[MAX_INPUT_LENGTH], crop_arg[MAX_INPUT_LENGTH];
	crop_data *crop;
	room_data *orm = obj_room(obj), *target;
	crop_vnum vnum;

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, crop_arg);
	
	// usage: %terracrop% [location] <crop vnum>
	if (!*loc_arg) {
		obj_log(obj, "oterracrop: bad syntax");
		return;
	}
	
	// check number of args
	if (!*crop_arg) {
		// only arg is actually crop arg
		strcpy(crop_arg, loc_arg);
		target = orm;
	}
	else {
		// two arguments
		target = get_room(orm, loc_arg);
	}
	
	if (!target) {
		obj_log(obj, "oterracrop: target is an invalid room");
		return;
	}
	
	// places you just can't terracrop -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*crop_arg) || (vnum = atoi(crop_arg)) < 0 || !(crop = crop_proto(vnum))) {
		obj_log(obj, "oterracrop: invalid crop vnum");
		return;
	}
	

	// good to go
	do_dg_terracrop(target, crop);
}


OCMD(do_oterraform) {
	void do_dg_terraform(room_data *target, sector_data *sect);

	char loc_arg[MAX_INPUT_LENGTH], sect_arg[MAX_INPUT_LENGTH];
	sector_data *sect;
	room_data *orm = obj_room(obj), *target;
	sector_vnum vnum;

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, sect_arg);
	
	// usage: %terraform% [location] <sector vnum>
	if (!*loc_arg) {
		obj_log(obj, "oterraform: bad syntax");
		return;
	}
	
	// check number of args
	if (!*sect_arg) {
		// only arg is actually sect arg
		strcpy(sect_arg, loc_arg);
		target = orm;
	}
	else {
		// two arguments
		target = get_room(orm, loc_arg);
	}
	
	if (!target) {
		obj_log(obj, "oterraform: target is an invalid room");
		return;
	}
	
	// places you just can't terraform -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*sect_arg) || (vnum = atoi(sect_arg)) < 0 || !(sect = sector_proto(vnum))) {
		obj_log(obj, "oterraform: invalid sector vnum");
		return;
	}
	
	// validate sect
	if (SECT_FLAGGED(sect, SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE)) {
		obj_log(obj, "oterraform: sector requires data that can't be set this way");
		return;
	}

	// good to go
	do_dg_terraform(target, sect);
}


OCMD(do_dgoload) {
	struct obj_binding *copy_obj_bindings(struct obj_binding *from);
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct instance_data *inst = NULL;
	int number = 0;
	room_data *room, *in_room;
	char_data *mob, *tch;
	obj_data *object, *cnt;
	vehicle_data *veh;
	char *target;
	int pos;

	target = two_arguments(argument, arg1, arg2);
	skip_spaces(&target);

	if (!*arg1 || !*arg2 || !is_number(arg2) || ((number = atoi(arg2)) < 0)) {
		obj_log(obj, "oload: bad syntax");
		return;
	}

	if (!(room = obj_room(obj))) {
		obj_log(obj, "oload: object in no location trying to load");
		return;
	}
	
	if (obj_room(obj)) {
		inst = find_instance_by_room(obj_room(obj), FALSE, TRUE);
	}

	if (is_abbrev(arg1, "mobile")) {
		if (!mob_proto(number)) {
			obj_log(obj, "oload: bad mob vnum");
			return;
		}
		mob = read_mobile(number, TRUE);
		if (COMPLEX_DATA(room) && COMPLEX_DATA(room)->instance) {
			MOB_INSTANCE_ID(mob) = INST_ID(COMPLEX_DATA(room)->instance);
			if (MOB_INSTANCE_ID(mob) != NOTHING) {
				add_instance_mob(real_instance(MOB_INSTANCE_ID(mob)), GET_MOB_VNUM(mob));
			}
		}
		char_to_room(mob, room);
		setup_generic_npc(mob, NULL, NOTHING, NOTHING);
		
		if (target && *target && isdigit(*target)) {
			// target is scale level
			scale_mob_to_level(mob, atoi(target));
			SET_BIT(MOB_FLAGS(mob), MOB_NO_RESCALE);
		}
		
		load_mtrigger(mob);
	}
	else if (is_abbrev(arg1, "object")) {
		if (!obj_proto(number)) {
			obj_log(obj, "oload: bad object vnum");
			return;
		}
		object = read_object(number, TRUE);
		
		if (inst) {
			instance_obj_setup(inst, object);
		}

		/* special handling to make objects able to load on a person/in a container/worn etc. */
		if (!target || !*target) {
			obj_to_room(object, room);
		
			// must scale now if possible
			scale_item_to_level(object, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
			
			// copy existing bindings
			if (OBJ_FLAGGED(object, OBJ_BIND_ON_PICKUP) && OBJ_BOUND_TO(obj)) {
				OBJ_BOUND_TO(object) = copy_obj_bindings(OBJ_BOUND_TO(obj));
			}

			load_otrigger(object);
			return;
		}
		target = two_arguments(target, arg1, arg2); /* recycling ... */
		skip_spaces(&target);
		
		// if they're picking a room, move arg2 down a slot to "target" level
		in_room = NULL;
		if (!str_cmp(arg1, "room")) {
			in_room = room;
			target = arg2;
		}
		else if (*arg1 == UID_CHAR) {
			if ((in_room = get_room(room, arg1))) {
				target = arg2;
			}
		}
		else {	// not targeting a room
			in_room = NULL;
		}
		
		// if there is still a target, we scale on that number, otherwise default scale
		if (*target && isdigit(*target)) {
			scale_item_to_level(object, atoi(target));
		}
		else {
			// default
			scale_item_to_level(object, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
		}
		
		// copy existing bindings
		if (OBJ_FLAGGED(object, OBJ_BIND_ON_PICKUP) && OBJ_BOUND_TO(obj)) {
			OBJ_BOUND_TO(object) = copy_obj_bindings(OBJ_BOUND_TO(obj));
		}
		
		if (in_room) {	// load in the room
			obj_to_room(object, in_room); 
			load_otrigger(object);
			return;
		}
		
		tch = get_char_near_obj(obj, arg1);
		if (tch) {
			// mark as "gathered" like a resource
			if (!IS_NPC(tch) && GET_LOYALTY(tch)) {
				add_production_total(GET_LOYALTY(tch), GET_OBJ_VNUM(object), 1);
			}
			
			if (*arg2 && (pos = find_eq_pos_script(arg2)) >= 0 && !GET_EQ(tch, pos) && can_wear_on_pos(object, pos)) {
				equip_char(tch, object, pos);
				load_otrigger(object);
				determine_gear_level(tch);
				return;
			}
			obj_to_char(object, tch);
			load_otrigger(object);
			return;
		}
		cnt = get_obj_near_obj(obj, arg1);
		if (cnt && GET_OBJ_TYPE(cnt) == ITEM_CONTAINER) {
			obj_to_obj(object, cnt);
			load_otrigger(object);
			return;
		}
		/* neither char nor container found - just dump it in room */
		obj_to_room(object, room); 
		load_otrigger(object);
		return;
	}
	else if (is_abbrev(arg1, "vehicle")) {
		if (!vehicle_proto(number)) {
			obj_log(obj, "oload: bad vehicle vnum");
			return;
		}
		veh = read_vehicle(number, TRUE);
		vehicle_to_room(veh, room);
		
		if (target && *target && isdigit(*target)) {
			// target is scale level
			scale_vehicle_to_level(veh, atoi(target));
		}
		else if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) > 0) {
			scale_vehicle_to_level(veh, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
		}
		else {
			// hope to inherit
			scale_vehicle_to_level(veh, 0);
		}
		
		load_vtrigger(veh);
	}
	else {
		obj_log(obj, "oload: bad type");
	}
}


OCMD(do_odamage) {
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	double modifier = 1.0;
	char_data *ch;
	int type;

	argument = two_arguments(argument, name, modarg);
	argument = one_argument(argument, typearg);	// optional

	/* who cares if it's a number ? if not it'll just be 0 */
	if (!*name) {
		obj_log(obj, "odamage: bad syntax");
		return;
	}

	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	
	// send negatives to %heal% instead
	if (modifier < 0) {
		sprintf(buf, "%s health %.2f", name, -atof(modarg));
		script_heal(obj, OBJ_TRIGGER, buf);
		return;
	}

	ch = get_char_by_obj(obj, name);

	if (!ch) {
		obj_log(obj, "odamage: target not found");        
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			obj_log(obj, "odamage: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	script_damage(ch, NULL, get_obj_scale_level(obj, ch), type, modifier);
}


OCMD(do_oaoe) {
	char modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH];
	double modifier = 1.0;
	int level, type;
	room_data *orm = obj_room(obj);
	char_data *vict, *next_vict;
	
	// no room == no work
	if (!orm) {
		return;
	}

	two_arguments(argument, modarg, typearg);
	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			obj_log(obj, "oaoe: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}

	level = get_obj_scale_level(obj, NULL);
	for (vict = ROOM_PEOPLE(orm); vict; vict = next_vict) {
		next_vict = vict->next_in_room;
		
		// harder to tell friend from foe: hit PCs or people following PCs
		if (!IS_NPC(vict) || (vict->master && !IS_NPC(vict->master))) {
			script_damage(vict, NULL, level, type, modifier);
		}
	}
}


OCMD(do_odot) {
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], durarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH], stackarg[MAX_INPUT_LENGTH];
	any_vnum atype = ATYPE_DG_AFFECT;
	double modifier = 1.0;
	char_data *ch;
	int type, max_stacks;

	argument = one_argument(argument, name);
	// sometimes name is an affect vnum
	if (*name == '#') {
		atype = atoi(name+1);
		argument = one_argument(argument, name);
		if (!find_generic(atype, GENERIC_AFFECT)) {
			atype = ATYPE_DG_AFFECT;
		}
	}
	argument = one_argument(argument, modarg);
	argument = one_argument(argument, durarg);
	argument = one_argument(argument, typearg);	// optional, default: physical
	argument = one_argument(argument, stackarg);	// optional, default: 1

	/* who cares if it's a number ? if not it'll just be 0 */
	if (!*name || !*modarg || !*durarg) {
		obj_log(obj, "odot: bad syntax");
		return;
	}

	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}

	ch = get_char_by_obj(obj, name);

	if (!ch) {
		obj_log(obj, "odot: target not found");        
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			obj_log(obj, "odot: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	max_stacks = (*stackarg ? atoi(stackarg) : 1);
	script_damage_over_time(ch, atype, get_obj_scale_level(obj, ch), type, modifier, atoi(durarg), max_stacks, NULL);
}


OCMD(do_odoor) {
	char target[MAX_INPUT_LENGTH], direction[MAX_INPUT_LENGTH];
	char field[MAX_INPUT_LENGTH], *value;
	room_data *rm, *troom, *orm = obj_room(obj);
	struct room_direction_data *newexit, *temp;
	int dir, fd;

	const char *door_field[] = {
		"purge",	// 0
		"flags",	// 1
		"name",	// 2
		"room",	// 3
		"add",	// 4
		"\n"
	};


	argument = one_word(argument, target);
	argument = one_argument(argument, direction);
	value = one_argument(argument, field);
	skip_spaces(&value);

	if (!*target || !*direction || !*field) {
		obj_log(obj, "odoor called with too few args");
		return;
	}

	if (!(rm = get_room(orm, target))) {
		obj_log(obj, "odoor: invalid target");
		return;
	}

	if ((dir = search_block(direction, dirs, FALSE)) == NO_DIR && (dir = search_block(direction, alt_dirs, FALSE)) == NO_DIR) {
		obj_log(obj, "odoor: invalid direction");
		return;
	}

	if ((fd = search_block(field, door_field, FALSE)) == NOTHING) {
		obj_log(obj, "odoor: invalid field");
		return;
	}
	
	if (!COMPLEX_DATA(rm)) {
		obj_log(obj, "odoor: called on room with no building data");
		return;
	}

	newexit = find_exit(rm, dir);

	/* purge exit */
	if (fd == 0) {
		if (newexit) {
			REMOVE_FROM_LIST(newexit, COMPLEX_DATA(rm)->exits, next);
			if (newexit->room_ptr) {
				--GET_EXITS_HERE(newexit->room_ptr);
			}
			if (newexit->keyword)
				free(newexit->keyword);
			free(newexit);
		}
	}

	switch (fd) {
		case 1:  /* flags       */
			if (!newexit) {
				obj_log(obj, "odoor: invalid door");
				break;
			}
			newexit->exit_info = (sh_int)asciiflag_conv(value);
			break;
		case 2:  /* name        */
			if (!newexit) {
				obj_log(obj, "odoor: invalid door");
				break;
			}
			if (newexit->keyword)
				free(newexit->keyword);
			CREATE(newexit->keyword, char, strlen(value) + 1);
			strcpy(newexit->keyword, value);
			break;
		case 3:  /* room        */
			if ((troom = get_room(orm, value))) {
				if (!newexit) {
					newexit = create_exit(rm, troom, dir, FALSE);
				}
				else {
					if (newexit->room_ptr) {
						// lower old one
						--GET_EXITS_HERE(newexit->room_ptr);
					}
					newexit->to_room = GET_ROOM_VNUM(troom);
					newexit->room_ptr = troom;
					++GET_EXITS_HERE(troom);
				}
			}
			else
				obj_log(obj, "odoor: invalid door target");
			break;
		case 4: {	// create room
			bld_data *bld;
			
			if (IS_ADVENTURE_ROOM(rm) || !ROOM_IS_CLOSED(rm) || !COMPLEX_DATA(rm)) {
				obj_log(obj, "odoor: attempting to add a room in invalid location %d", GET_ROOM_VNUM(rm));
			}
			else if (!*value || !isdigit(*value) || !(bld = building_proto(atoi(value))) || !IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM)) {
				obj_log(obj, "odoor: attempting to add invalid room '%s'", value);
			}
			else {
				do_dg_add_room_dir(rm, dir, bld);
			}
			break;
		}
	}
}


OCMD(do_osetval) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	int position, new_value;

	two_arguments(argument, arg1, arg2);
	if (!*arg1 || !*arg2 || !is_number(arg1) || !is_number(arg2)) {
		obj_log(obj, "osetval: bad syntax");
		return;
	}

	position = atoi(arg1);
	new_value = atoi(arg2);
	if (position >= 0 && position < NUM_OBJ_VAL_POSITIONS)
		GET_OBJ_VAL(obj, position) = new_value;
	else
		obj_log(obj, "osetval: position out of bounds!");
}

/* submitted by PurpleOnyx - tkhasi@shadowglen.com*/
OCMD(do_oat)  {
	char location[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	obj_data *object;
	room_data *room = NULL, *objr;

	half_chop(argument, location, arg2);

	if (!*location || !*arg2) {
		obj_log(obj, "oat: bad syntax : %s", argument);  
		return;
	}

	if ((objr = obj_room(obj))) {
		room = get_room(objr, location);
	}
	else if (isdigit(*location)) {
		// backup plan
		room = real_room(atoi(location));
	}

	if (!room) {
		obj_log(obj, "oat: location not found");
		return;
	}

	object = read_object(GET_OBJ_VNUM(obj), TRUE);
	if (!object)
		return;

	obj_to_room(object, room);
	obj_command_interpreter(object, arg2);

	if (IN_ROOM(object) == room)
		extract_obj(object);
}


OCMD(do_oscale) {
	char arg[MAX_INPUT_LENGTH], lvl_arg[MAX_INPUT_LENGTH];
	char_data *victim;
	obj_data *otarg, *fresh, *proto;
	vehicle_data *veh;
	int level;

	two_arguments(argument, arg, lvl_arg);

	if (!*arg) {
		obj_log(obj, "oscale: No target provided");
		return;
	}
	
	if (*lvl_arg && !isdigit(*lvl_arg)) {
		obj_log(obj, "oscale: invalid level '%s'", lvl_arg);
		return;
	}
	else if (*lvl_arg) {
		level = atoi(lvl_arg);
	}
	else {
		level = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
	}
	
	if (level <= 0) {
		obj_log(obj, "oscale: no valid level to scale to");
		return;
	}

	// scale adventure
	if (!str_cmp(arg, "instance")) {
		void scale_instance_to_level(struct instance_data *inst, int level);
		room_data *orm = obj_room(obj);
		struct instance_data *inst;
		if ((inst = find_instance_by_room(orm, FALSE, TRUE))) {
			scale_instance_to_level(inst, level);
		}
	}
	// scale char
	else if ((victim = get_char_by_obj(obj, arg))) {
		if (!IS_NPC(victim)) {
			obj_log(obj, "oscale: unable to scale a PC");
			return;
		}

		scale_mob_to_level(victim, level);
		SET_BIT(MOB_FLAGS(victim), MOB_NO_RESCALE);
	}
	// scale vehicle
	else if ((veh = get_vehicle_by_obj(obj, arg))) {
		scale_vehicle_to_level(veh, level);
	}
	// scale obj
	else if ((otarg = get_obj_by_obj(obj, arg))) {
		if (OBJ_FLAGGED(otarg, OBJ_SCALABLE)) {
			scale_item_to_level(otarg, level);
		}
		else if ((proto = obj_proto(GET_OBJ_VNUM(otarg))) && OBJ_FLAGGED(proto, OBJ_SCALABLE)) {
			fresh = fresh_copy_obj(otarg, level);
			swap_obj_for_obj(otarg, fresh);
			if (otarg == obj) {
				dg_owner_purged = 1;
			}
			extract_obj(otarg);
		}
		else {
			// attempt to scale anyway
			scale_item_to_level(otarg, level);
		}
	}
	else {
		obj_log(obj, "oscale: bad argument");
	}
}


const struct obj_command_info obj_cmd_info[] = {
	{ "RESERVED", 0, 0 },/* this must be first -- for specprocs */

	{ "oadventurecomplete", do_oadventurecomplete, NO_SCMD },
	{ "oat", do_oat, NO_SCMD },
	{ "obuild", do_obuild, NO_SCMD },
	{ "odoor", do_odoor, NO_SCMD },
	{ "odamage", do_odamage,   NO_SCMD },
	{ "oaoe", do_oaoe,   NO_SCMD },
	{ "odot", do_odot,   NO_SCMD },
	{ "oecho", do_oecho, NO_SCMD },
	{ "oechoaround", do_osend, SCMD_OECHOAROUND },
	{ "oechoneither", do_oechoneither, NO_SCMD },
	{ "oforce", do_oforce, NO_SCMD },
	{ "oheal", do_oheal, NO_SCMD },
	{ "oload", do_dgoload, NO_SCMD },
	{ "omod", do_omod, NO_SCMD },
	{ "omorph", do_omorph, NO_SCMD },
	{ "oown", do_oown, NO_SCMD },
	{ "opurge", do_opurge, NO_SCMD },
	{ "oquest", do_oquest, NO_SCMD },
	{ "orestore", do_orestore, NO_SCMD },
	{ "oscale", do_oscale, NO_SCMD },
	{ "osend", do_osend, SCMD_OSEND },
	{ "osetval", do_osetval, NO_SCMD },
	{ "osiege", do_osiege, NO_SCMD },
	{ "oslay", do_oslay, NO_SCMD },
	{ "oteleport", do_oteleport, NO_SCMD },
	{ "oterracrop", do_oterracrop, NO_SCMD },
	{ "oterraform", do_oterraform, NO_SCMD },
	{ "otimer", do_otimer, NO_SCMD },
	{ "otransform", do_otransform, NO_SCMD },
	{ "obuildingecho", do_obuildingecho, NO_SCMD }, /* fix by Rumble */
	{ "oregionecho", do_oregionecho, NO_SCMD },
	{ "ovehicleecho", do_ovehicleecho, NO_SCMD },

	{ "\n", 0, 0 }        /* this must be last */
};



/*
*  This is the command interpreter used by objects, called by script_driver.
*/
void obj_command_interpreter(obj_data *obj, char *argument) {
	int cmd, length;
	char line[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];

	skip_spaces(&argument);

	/* just drop to next line for hitting CR */
	if (!*argument)
		return;

	half_chop(argument, arg, line);

	/* find the command */
	for (length = strlen(arg), cmd = 0; *obj_cmd_info[cmd].command != '\n'; cmd++)
		if (!strn_cmp(obj_cmd_info[cmd].command, arg, length))
			break;

	if (*obj_cmd_info[cmd].command == '\n')
		obj_log(obj, "Unknown object cmd: '%s'", argument);
	else
		((*obj_cmd_info[cmd].command_pointer) (obj, line, cmd, obj_cmd_info[cmd].subcmd));
}
