/* ************************************************************************
*   File: dg_objcmd.c                                     EmpireMUD 2.0b3 *
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
extern int dg_owner_purged;

// external functions
void obj_command_interpreter(obj_data *obj, char *argument);
void send_char_pos(char_data *ch, int dam);
extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom);
char_data *get_char_by_obj(obj_data *obj, char *name);
obj_data *get_obj_by_obj(obj_data *obj, char *name);
room_data *get_room(room_data *ref, char *name);
vehicle_data *get_vehicle_by_obj(obj_data *obj, char *name);
vehicle_data *get_vehicle_near_obj(obj_data *obj, char *name);
void instance_obj_setup(struct instance_data *inst, obj_data *obj);
void sub_write(char *arg, char_data *ch, byte find_invis, int targets);
void die(char_data *ch, char_data *killer);
void scale_item_to_level(obj_data *obj, int level);
void scale_mob_to_level(char_data *mob, int level);

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
		if (inst->level) {
			level = inst->level;
		}
		else if (GET_ADV_MIN_LEVEL(inst->adventure) > 0) {
			level = GET_ADV_MIN_LEVEL(inst->adventure);
		}
		else if (GET_ADV_MAX_LEVEL(inst->adventure) > 0) {
			level = GET_ADV_MAX_LEVEL(inst->adventure) / 2; // average?
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
	
	room_data *room = obj_room(obj);
	
	if (room && COMPLEX_DATA(room) && COMPLEX_DATA(room)->instance) {
		mark_instance_completed(COMPLEX_DATA(room)->instance);
	}
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


OCMD(do_obuildingecho) {
	room_data *froom, *home_room, *iter;
	room_data *orm = obj_room(obj);
	char room_number[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH], *msg;

	msg = any_one_word(argument, room_number);
	skip_spaces(&msg);

	if (!*room_number || !*msg) {
		obj_log(obj, "obuildingecho called with too few args");
	}
	else if (!(froom = get_room(orm, arg))) {
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
	else if (!(center = get_room(orm, arg))) {
		obj_log(obj, "oregionecho called with invalid target");
	}
	else {
		center = get_map_location_for(center);
		radius = atoi(radius_arg);
		if (radius < 0) {
			radius = -radius;
			indoor_only = TRUE;
		}
		
		if (center) {			
			for (desc = descriptor_list; desc; desc = desc->next) {
				if (STATE(desc) != CON_PLAYING || !(targ = desc->character) || PLR_FLAGGED(targ, PLR_WRITING)) {
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
		tmpobj.autostore_timer = obj->autostore_timer;
		tmpobj.carried_by = obj->carried_by;
		tmpobj.in_vehicle = obj->in_vehicle;
		tmpobj.worn_by = obj->worn_by;
		tmpobj.worn_on = obj->worn_on;
		tmpobj.in_obj = obj->in_obj;
		tmpobj.contains = obj->contains;
		tmpobj.id = obj->id;
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


/* purge all objects an npcs in room, or specified object or mob */
OCMD(do_opurge) {
	char arg[MAX_INPUT_LENGTH];
	char_data *ch, *next_ch;
	obj_data *o, *next_obj;
	vehicle_data *veh;
	room_data *rm;

	one_argument(argument, arg);

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
	
	// purge char
	if ((ch = get_char_by_obj(obj, arg))) {
		if (!IS_NPC(ch)) {
			obj_log(obj, "opurge: purging a PC");
			return;
		}

		extract_char(ch);
	}
	// purge vehicle
	else if ((veh = get_vehicle_by_obj(obj, arg))) {
		extract_vehicle(veh);
	}
	// purge obj
	else if ((o = get_obj_by_obj(obj, arg))) {
		if (o == obj) {
			dg_owner_purged = 1;
		}
		extract_obj(o);
	}
	else {
		obj_log(obj, "opurge: bad argument");
	}
}


OCMD(do_oteleport) {	
	char_data *ch, *next_ch;
	room_data *target, *orm = obj_room(obj);
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct instance_data *inst;
	vehicle_data *veh;
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
		}
	}
	else if (!str_cmp(arg1, "adventure")) {
		// teleport all players in the adventure
		if (!orm || !(inst = find_instance_by_room(orm, FALSE))) {
			obj_log(obj, "oteleport: 'adventure' mode called outside any adventure");
			return;
		}
		
		for (iter = 0; iter < inst->size; ++iter) {
			// only if it's not the target room, or we'd be here all day
			if (inst->room[iter] && inst->room[iter] != target) {
				for (ch = ROOM_PEOPLE(inst->room[iter]); ch; ch = next_ch) {
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
			}
		}
		else if ((veh = get_vehicle_near_obj(obj, arg1))) {
			vehicle_from_room(veh);
			vehicle_to_room(veh, target);
			entry_vtrigger(veh);
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
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	void scale_vehicle_to_level(vehicle_data *veh, int level);
	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct instance_data *inst = NULL;
	int number = 0;
	room_data *room;
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
		inst = find_instance_by_room(obj_room(obj), FALSE);
	}

	if (is_abbrev(arg1, "mobile")) {
		if (!mob_proto(number)) {
			obj_log(obj, "oload: bad mob vnum");
			return;
		}
		mob = read_mobile(number, TRUE);
		if (COMPLEX_DATA(room) && COMPLEX_DATA(room)->instance) {
			MOB_INSTANCE_ID(mob) = COMPLEX_DATA(room)->instance->id;
		}
		char_to_room(mob, room);
		setup_generic_npc(mob, NULL, NOTHING, NOTHING);
		
		if (target && *target && isdigit(*target)) {
			// target is scale level
			scale_mob_to_level(mob, atoi(target));
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

			load_otrigger(object);
			return;
		}
		target = two_arguments(target, arg1, arg2); /* recycling ... */
		skip_spaces(&target);
		
		// if there is still a target, we scale on that number, otherwise default scale
		if (*target && isdigit(*target)) {
			scale_item_to_level(object, atoi(target));
		}
		else {
			// default
			scale_item_to_level(object, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
		}
		
		tch = get_char_near_obj(obj, arg1);
		if (tch) {
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
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH];
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
	double modifier = 1.0;
	char_data *ch;
	int type, max_stacks;

	argument = one_argument(argument, name);
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
	script_damage_over_time(ch, get_obj_scale_level(obj, ch), type, modifier, atoi(durarg), max_stacks, NULL);
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

	if ((dir = search_block(direction, dirs, FALSE)) == NOTHING) {
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

	victim = get_char_by_obj(obj, arg);
	if (!victim) {
		otarg = get_obj_by_obj(obj, arg);
		if (otarg) {
			if (OBJ_FLAGGED(otarg, OBJ_SCALABLE)) {
				scale_item_to_level(otarg, level);
			}
			else if ((proto = obj_proto(GET_OBJ_VNUM(otarg))) && OBJ_FLAGGED(proto, OBJ_SCALABLE)) {
				fresh = read_object(GET_OBJ_VNUM(otarg), TRUE);
				scale_item_to_level(fresh, level);
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

		return;
	}

	if (!IS_NPC(victim)) {
		obj_log(obj, "oscale: unable to scale a PC");
		return;
	}

	scale_mob_to_level(victim, level);
}


const struct obj_command_info obj_cmd_info[] = {
	{ "RESERVED", 0, 0 },/* this must be first -- for specprocs */

	{ "oadventurecomplete", do_oadventurecomplete, NO_SCMD },
	{ "oat", do_oat, NO_SCMD },
	{ "odoor", do_odoor, NO_SCMD },
	{ "odamage", do_odamage,   NO_SCMD },
	{ "oaoe", do_oaoe,   NO_SCMD },
	{ "odot", do_odot,   NO_SCMD },
	{ "oecho", do_oecho, NO_SCMD },
	{ "oechoaround", do_osend, SCMD_OECHOAROUND },
	{ "oechoneither", do_oechoneither, NO_SCMD },
	{ "oforce", do_oforce, NO_SCMD },
	{ "oload", do_dgoload, NO_SCMD },
	{ "opurge", do_opurge, NO_SCMD },
	{ "oscale", do_oscale, NO_SCMD },
	{ "osend", do_osend, SCMD_OSEND },
	{ "osetval", do_osetval, NO_SCMD },
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
