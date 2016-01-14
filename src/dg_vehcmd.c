/* ************************************************************************
*   File: dg_vehcmd.c                                     EmpireMUD 2.0b3 *
*  Usage: contains the command_interpreter for vehicles, vehicle commands *
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

/**
* Contents:
*   Helpers
*   Commands
*/

// external vars
extern const char *damage_types[];
extern const char *dirs[];
extern int dg_owner_purged;

// external functions
void die(char_data *ch, char_data *killer);
extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom);
extern char_data *get_char_by_vehicle(vehicle_data *veh, char *name);
extern obj_data *get_obj_by_vehicle(vehicle_data *veh, char *name);
extern room_data *get_room(room_data *ref, char *name);
extern vehicle_data *get_vehicle_by_vehicle(vehicle_data *veh, char *name);
void instance_obj_setup(struct instance_data *inst, obj_data *obj);
void scale_item_to_level(obj_data *obj, int level);
void scale_mob_to_level(char_data *mob, int level);
void scale_vehicle_to_level(vehicle_data *veh, int level);
void sub_write(char *arg, char_data *ch, byte find_invis, int targets);
void vehicle_command_interpreter(vehicle_data *veh, char *argument);


// local stuff
#define VCMD(name)  void (name)(vehicle_data *veh, char *argument, int cmd, int subcmd)

struct vehicle_command_info {
	char *command;
	void (*command_pointer)(vehicle_data *veh, char *argument, int cmd, int subcmd);
	int subcmd;
};


// do_vsend
#define SCMD_VSEND         0
#define SCMD_VECHOAROUND   1


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Determines the scale level for a vehicle, with a character target as a backup
* for unscaled vehicles.
*
* @param vehicle_data *veh The vehicle to check.
* @param char_data *targ Optional: A character target.
* @return int A scale level.
*/
int get_vehicle_scale_level(vehicle_data *veh, char_data *targ) {
	struct instance_data *inst;
	int level = 1;
	room_data *orm = IN_ROOM(veh);
	
	if (VEH_SCALE_LEVEL(veh) > 0) {
		level = VEH_SCALE_LEVEL(veh);
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


/**
* Attaches vehicle name and vnum to msg and sends it to script_log
*
* @param vehicle_data *veh The vehicle that's logging.
* @param const char *format... The log.
*/
void veh_log(vehicle_data *veh, const char *format, ...) {
	va_list args;
	char output[MAX_STRING_LENGTH];

	snprintf(output, sizeof(output), "Veh (%s, VNum %d):: %s", VEH_SHORT_DESC(veh), VEH_VNUM(veh), format);

	va_start(args, format);
	script_vlog(output, args);
	va_end(args);
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

VCMD(do_vadventurecomplete) {
	void mark_instance_completed(struct instance_data *inst);
	
	room_data *room = IN_ROOM(veh);
	
	if (room && COMPLEX_DATA(room) && COMPLEX_DATA(room)->instance) {
		mark_instance_completed(COMPLEX_DATA(room)->instance);
	}
}


VCMD(do_vecho) {
	skip_spaces(&argument);

	if (!*argument)  {
		veh_log(veh, "vecho called with no args");
	}
	else if (!IN_ROOM(veh)) {
		veh_log(veh, "vecho called by vehicle in no location");
	}
	else {
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			sub_write(argument, ROOM_PEOPLE(IN_ROOM(veh)), TRUE, TO_ROOM | TO_CHAR);
		}
	}
}


VCMD(do_vforce) {
	char arg1[MAX_INPUT_LENGTH], *line;
	char_data *ch, *next_ch;
	room_data *room;

	line = one_argument(argument, arg1);

	if (!*arg1 || !*line) {
		veh_log(veh, "vforce called with too few args");
		return;
	}

	if (!str_cmp(arg1, "all")) {
		if (!(room = IN_ROOM(veh))) {
			veh_log(veh, "vforce called by vehicle in no location");
		}
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
		if ((ch = get_char_by_vehicle(veh, arg1))) {
			if (valid_dg_target(ch, 0)) {
				command_interpreter(ch, line);
			}
		}
		else {
			veh_log(veh, "vforce: no target found");
		}
	}
}


VCMD(do_vbuildingecho) {
	char room_number[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH], *msg;
	room_data *froom, *home_room, *iter;
	room_data *orm = IN_ROOM(veh);

	msg = any_one_word(argument, room_number);
	skip_spaces(&msg);

	if (!*room_number || !*msg) {
		veh_log(veh, "vbuildingecho called with too few args");
	}
	else if (!(froom = get_room(orm, arg))) {
		veh_log(veh, "vbuildingecho called with invalid target");
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


VCMD(do_vregionecho) {
	char room_number[MAX_INPUT_LENGTH], radius_arg[MAX_INPUT_LENGTH], *msg;
	room_data *center, *orm = IN_ROOM(veh);
	bool indoor_only = FALSE;
	descriptor_data *desc;
	char_data *targ;
	int radius;

	argument = any_one_word(argument, room_number);
	msg = one_argument(argument, radius_arg);
	skip_spaces(&msg);

	if (!*room_number || !*radius_arg || !*msg) {
		veh_log(veh, "vregionecho called with too few args");
	}
	else if (!isdigit(*radius_arg) && *radius_arg != '-') {
		veh_log(veh, "vregionecho called with invalid radius");
	}
	else if (!(center = get_room(orm, arg))) {
		veh_log(veh, "vregionecho called with invalid target");
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


// prints the message to everyone except two targets
VCMD(do_vechoneither) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	char_data *vict1, *vict2, *iter;
	char *p;

	p = two_arguments(argument, arg1, arg2);
	skip_spaces(&p);

	if (!*arg1 || !*arg2 || !*p) {
		veh_log(veh, "vechoneither called with missing arguments");
		return;
	}
	
	if (!(vict1 = get_char_by_vehicle(veh, arg1))) {
		veh_log(veh, "vechoneither: vict 1 (%s) does not exist", arg1);
		return;
	}
	if (!(vict2 = get_char_by_vehicle(veh, arg2))) {
		veh_log(veh, "vechoneither: vict 2 (%s) does not exist", arg2);
		return;
	}
	
	for (iter = ROOM_PEOPLE(IN_ROOM(vict1)); iter; iter = iter->next_in_room) {
		if (iter->desc && iter != vict1 && iter != vict2) {
			sub_write(p, iter, TRUE, TO_CHAR);
		}
	}
}


VCMD(do_vsend) {
	char buf[MAX_INPUT_LENGTH], *msg;
	char_data *ch;

	msg = any_one_arg(argument, buf);

	if (!*buf) {
		veh_log(veh, "vsend called with no args");
		return;
	}

	skip_spaces(&msg);

	if (!*msg) {
		veh_log(veh, "vsend called without a message");
		return;
	}

	if ((ch = get_char_by_vehicle(veh, buf))) {
		if (subcmd == SCMD_VSEND)
			sub_write(msg, ch, TRUE, TO_CHAR);
		else if (subcmd == SCMD_VECHOAROUND)
			sub_write(msg, ch, TRUE, TO_ROOM);
	}
	else {
		veh_log(veh, "no target found for vsend");
	}
}


// incoming subcmd is direction
VCMD(do_vmove) {
	extern bool move_vehicle(char_data *ch, vehicle_data *veh, int dir, int subcmd);
	move_vehicle(NULL, veh, subcmd, 0);
}


// purge all objects and npcs in room, or specified object/mob/vehicle
VCMD(do_vpurge) {
	char arg[MAX_INPUT_LENGTH];
	char_data *ch, *next_ch;
	obj_data *o, *next_obj;
	vehicle_data *v;
	room_data *rm;

	one_argument(argument, arg);

	if (!*arg) {
		// purge all
		if ((rm = IN_ROOM(veh))) {
			for (ch = ROOM_PEOPLE(rm); ch; ch = next_ch ) {
				next_ch = ch->next_in_room;
				if (IS_NPC(ch)) {
					extract_char(ch);
				}
			}

			for (o = ROOM_CONTENTS(rm); o; o = next_obj ) {
				next_obj = o->next_content;
				extract_obj(o);
			}
		}

		return;
	}
	
	if ((ch = get_char_by_vehicle(veh, arg))) {
		if (!IS_NPC(ch)) {
			veh_log(veh, "vpurge: purging a PC");
			return;
		}

		extract_char(ch);
	}
	else if ((o = get_obj_by_vehicle(veh, arg))) {
		extract_obj(o);
	}
	else if ((v = get_vehicle_by_vehicle(veh, arg))) {
		if (v == veh) {
			dg_owner_purged = 1;
		}
		extract_vehicle(v);
	}
	else {
		veh_log(veh, "vpurge: bad argument");
	}
}


VCMD(do_vteleport) {	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	room_data *target, *orm = IN_ROOM(veh);
	struct instance_data *inst;
	char_data *ch, *next_ch;
	int iter;

	two_arguments(argument, arg1, arg2);

	if (!*arg1 || !*arg2) {
		veh_log(veh, "vteleport called with too few args");
		return;
	}

	target = get_room(orm, arg2);

	if (!target) {
		veh_log(veh, "vteleport target is an invalid room");
		return;
	}
	
	if (!str_cmp(arg1, "all")) {
		if (target == orm) {
			veh_log(veh, "vteleport target is itself");
			return;
		}

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
			veh_log(veh, "vteleport: 'adventure' mode called outside any adventure");
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
		if ((ch = get_char_by_vehicle(veh, arg1))) {
			if (valid_dg_target(ch, DG_ALLOW_GODS)) {
				char_from_room(ch);
				char_to_room(ch, target);
				if (!IS_NPC(ch)) {
					GET_LAST_DIR(ch) = NO_DIR;
				}
				enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
			}
		}
		else {
			veh_log(veh, "vteleport: no target found");
		}
	}
}


VCMD(do_vterracrop) {
	void do_dg_terracrop(room_data *target, crop_data *crop);

	char loc_arg[MAX_INPUT_LENGTH], crop_arg[MAX_INPUT_LENGTH];
	room_data *orm = IN_ROOM(veh), *target;
	crop_data *crop;
	crop_vnum vnum;

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, crop_arg);
	
	// usage: %terracrop% [location] <crop vnum>
	if (!*loc_arg) {
		veh_log(veh, "vterracrop: bad syntax");
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
		veh_log(veh, "vterracrop: target is an invalid room");
		return;
	}
	
	// places you just can't terracrop -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*crop_arg) || (vnum = atoi(crop_arg)) < 0 || !(crop = crop_proto(vnum))) {
		veh_log(veh, "vterracrop: invalid crop vnum");
		return;
	}
	

	// good to go
	do_dg_terracrop(target, crop);
}


VCMD(do_vterraform) {
	void do_dg_terraform(room_data *target, sector_data *sect);

	char loc_arg[MAX_INPUT_LENGTH], sect_arg[MAX_INPUT_LENGTH];
	room_data *orm = IN_ROOM(veh), *target;
	sector_data *sect;
	sector_vnum vnum;

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, sect_arg);
	
	// usage: %terraform% [location] <sector vnum>
	if (!*loc_arg) {
		veh_log(veh, "vterraform: bad syntax");
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
		veh_log(veh, "vterraform: target is an invalid room");
		return;
	}
	
	// places you just can't terraform -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*sect_arg) || (vnum = atoi(sect_arg)) < 0 || !(sect = sector_proto(vnum))) {
		veh_log(veh, "vterraform: invalid sector vnum");
		return;
	}
	
	// validate sect
	if (SECT_FLAGGED(sect, SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE)) {
		veh_log(veh, "vterraform: sector requires data that can't be set this way");
		return;
	}

	// good to go
	do_dg_terraform(target, sect);
}


VCMD(do_dgvload) {
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct instance_data *inst = NULL;
	int number = 0;
	room_data *room;
	char_data *mob, *tch;
	obj_data *object, *cnt;
	vehicle_data *vehicle;
	char *target;
	int pos;

	target = two_arguments(argument, arg1, arg2);
	skip_spaces(&target);

	if (!*arg1 || !*arg2 || !is_number(arg2) || ((number = atoi(arg2)) < 0)) {
		veh_log(veh, "vload: bad syntax");
		return;
	}

	if (!(room = IN_ROOM(veh))) {
		veh_log(veh, "vload: vehicle in no location trying to load");
		return;
	}
	
	inst = find_instance_by_room(room, FALSE);
	
	if (is_abbrev(arg1, "mobile")) {
		if (!mob_proto(number)) {
			veh_log(veh, "vload: bad mob vnum");
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
			veh_log(veh, "vload: bad object vnum");
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
			scale_item_to_level(object, VEH_SCALE_LEVEL(veh));

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
			scale_item_to_level(object, VEH_SCALE_LEVEL(veh));
		}
		
		tch = get_char_near_vehicle(veh, arg1);
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
		cnt = get_obj_near_vehicle(veh, arg1);
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
			veh_log(veh, "oload: bad vehicle vnum");
			return;
		}
		vehicle = read_vehicle(number, TRUE);
		vehicle_to_room(vehicle, room);
		
		if (target && *target && isdigit(*target)) {
			// target is scale level
			scale_vehicle_to_level(vehicle, atoi(target));
		}
		else if (VEH_SCALE_LEVEL(veh) > 0) {
			scale_vehicle_to_level(vehicle, VEH_SCALE_LEVEL(veh));
		}
		else {
			// hope to inherit
			scale_vehicle_to_level(veh, 0);
		}
		
		load_vtrigger(veh);
	}
	else {
		veh_log(veh, "vload: bad type");
	}
}


VCMD(do_vdamage) {
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH];
	double modifier = 1.0;
	char_data *ch;
	int type;

	argument = two_arguments(argument, name, modarg);
	argument = one_argument(argument, typearg);	// optional

	/* who cares if it's a number ? if not it'll just be 0 */
	if (!*name) {
		veh_log(veh, "vdamage: bad syntax");
		return;
	}

	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}

	ch = get_char_by_vehicle(veh, name);

	if (!ch) {
		veh_log(veh, "vdamage: target not found");        
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			veh_log(veh, "vdamage: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	script_damage(ch, NULL, get_vehicle_scale_level(veh, ch), type, modifier);
}


VCMD(do_vaoe) {
	char modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH];
	double modifier = 1.0;
	int level, type;
	room_data *orm = IN_ROOM(veh);
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
			veh_log(veh, "vaoe: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}

	level = get_vehicle_scale_level(veh, NULL);
	for (vict = ROOM_PEOPLE(orm); vict; vict = next_vict) {
		next_vict = vict->next_in_room;
		
		// harder to tell friend from foe: hit PCs or people following PCs
		if (!IS_NPC(vict) || (vict->master && !IS_NPC(vict->master))) {
			script_damage(vict, NULL, level, type, modifier);
		}
	}
}


VCMD(do_vdot) {
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
		veh_log(veh, "vdot: bad syntax");
		return;
	}

	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}

	ch = get_char_by_vehicle(veh, name);

	if (!ch) {
		veh_log(veh, "vdot: target not found");        
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			veh_log(veh, "vdot: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	max_stacks = (*stackarg ? atoi(stackarg) : 1);
	script_damage_over_time(ch, get_vehicle_scale_level(veh, ch), type, modifier, atoi(durarg), max_stacks, NULL);
}


VCMD(do_vdoor) {
	char target[MAX_INPUT_LENGTH], direction[MAX_INPUT_LENGTH];
	char field[MAX_INPUT_LENGTH], *value;
	room_data *rm, *troom, *orm = IN_ROOM(veh);
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
		veh_log(veh, "vdoor called with too few args");
		return;
	}

	if (!(rm = get_room(orm, target))) {
		veh_log(veh, "vdoor: invalid target");
		return;
	}

	if ((dir = search_block(direction, dirs, FALSE)) == NOTHING) {
		veh_log(veh, "vdoor: invalid direction");
		return;
	}

	if ((fd = search_block(field, door_field, FALSE)) == NOTHING) {
		veh_log(veh, "vdoor: invalid field");
		return;
	}
	
	if (!COMPLEX_DATA(rm)) {
		veh_log(veh, "vdoor: called on room with no building data");
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
				veh_log(veh, "vdoor: invalid door");
				break;
			}
			newexit->exit_info = (sh_int)asciiflag_conv(value);
			break;
		case 2:  /* name        */
			if (!newexit) {
				veh_log(veh, "vdoor: invalid door");
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
				veh_log(veh, "vdoor: invalid door target");
			break;
	}
}


VCMD(do_vat)  {
	char location[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	vehicle_data *vehicle;
	room_data *room = NULL, *was_in;

	half_chop(argument, location, arg2);

	if (!*location || !*arg2) {
		veh_log(veh, "vat: bad syntax : %s", argument);  
		return;
	}

	if ((was_in = IN_ROOM(veh))) {
		room = get_room(was_in, location);
	}
	else if (isdigit(*location)) {
		// backup plan
		room = real_room(atoi(location));
	}

	if (!room) {
		veh_log(veh, "vat: location not found");
		return;
	}

	vehicle = read_vehicle(VEH_VNUM(veh), TRUE);
	if (!vehicle) {
		return;
	}

	vehicle_to_room(vehicle, room);
	vehicle_command_interpreter(vehicle, arg2);

	if (IN_ROOM(vehicle) == room) {
		extract_vehicle(vehicle);
	}
}


VCMD(do_vscale) {
	char arg[MAX_INPUT_LENGTH], lvl_arg[MAX_INPUT_LENGTH];
	obj_data *otarg, *fresh, *proto;
	vehicle_data *vehicle;
	char_data *victim;
	int level;

	two_arguments(argument, arg, lvl_arg);

	if (!*arg) {
		veh_log(veh, "vscale: No target provided");
		return;
	}
	
	if (*lvl_arg && !isdigit(*lvl_arg)) {
		veh_log(veh, "vscale: invalid level '%s'", lvl_arg);
		return;
	}
	else if (*lvl_arg) {
		level = atoi(lvl_arg);
	}
	else {
		level = VEH_SCALE_LEVEL(veh);
	}
	
	if (level <= 0) {
		veh_log(veh, "vscale: no valid level to scale to");
		return;
	}

	if ((victim = get_char_by_vehicle(veh, arg))) {
		if (!IS_NPC(victim)) {
			veh_log(veh, "vscale: unable to scale a PC");
			return;
		}

		scale_mob_to_level(victim, level);
	}
	else if ((vehicle = get_vehicle_by_vehicle(veh, arg))) {
		scale_vehicle_to_level(vehicle, level);
	}
	else if ((otarg = get_obj_by_vehicle(veh, arg))) {
		if (OBJ_FLAGGED(otarg, OBJ_SCALABLE)) {
			scale_item_to_level(otarg, level);
		}
		else if ((proto = obj_proto(GET_OBJ_VNUM(otarg))) && OBJ_FLAGGED(proto, OBJ_SCALABLE)) {
			fresh = read_object(GET_OBJ_VNUM(otarg), TRUE);
			scale_item_to_level(fresh, level);
			swap_obj_for_obj(otarg, fresh);
			extract_obj(otarg);
		}
		else {
			// attempt to scale anyway
			scale_item_to_level(otarg, level);
		}
	}
	
	else {
		veh_log(veh, "vscale: bad argument");
	}
}


const struct vehicle_command_info veh_cmd_info[] = {
	{ "RESERVED", 0, 0 },/* this must be first -- for specprocs */
	
	// dirs
	{ "north", do_vmove, NORTH },
	{ "east", do_vmove, EAST },
	{ "south", do_vmove, SOUTH },
	{ "west", do_vmove, WEST },
	{ "northwest", do_vmove, NORTHWEST },
	{ "northeast", do_vmove, NORTHEAST },
	{ "southwest", do_vmove, SOUTHWEST },
	{ "southeast", do_vmove, SOUTHEAST },
	{ "nw", do_vmove, NORTHWEST },
	{ "ne", do_vmove, NORTHEAST },
	{ "sw", do_vmove, SOUTHWEST },
	{ "se", do_vmove, SOUTHEAST },
	{ "up", do_vmove, UP },
	{ "down", do_vmove, DOWN },
	{ "fore", do_vmove, FORE },
	{ "starboard", do_vmove, STARBOARD },
	{ "port", do_vmove, PORT },
	{ "aft", do_vmove, AFT },

	{ "vadventurecomplete", do_vadventurecomplete, NO_SCMD },
	{ "vat", do_vat, NO_SCMD },
	{ "vdoor", do_vdoor, NO_SCMD },
	{ "vdamage", do_vdamage,   NO_SCMD },
	{ "vaoe", do_vaoe,   NO_SCMD },
	{ "vdot", do_vdot,   NO_SCMD },
	{ "vecho", do_vecho, NO_SCMD },
	{ "vechoaround", do_vsend, SCMD_VECHOAROUND },
	{ "vechoneither", do_vechoneither, NO_SCMD },
	{ "vforce", do_vforce, NO_SCMD },
	{ "vload", do_dgvload, NO_SCMD },
	{ "vpurge", do_vpurge, NO_SCMD },
	{ "vscale", do_vscale, NO_SCMD },
	{ "vsend", do_vsend, SCMD_VSEND },
	{ "vteleport", do_vteleport, NO_SCMD },
	{ "vterracrop", do_vterracrop, NO_SCMD },
	{ "vterraform", do_vterraform, NO_SCMD },
	{ "vbuildingecho", do_vbuildingecho, NO_SCMD },
	{ "vregionecho", do_vregionecho, NO_SCMD },

	{ "\n", 0, 0 }        /* this must be last */
};



/**
* This is the command interpreter used by vehicless, called by script_driver.
*
* @param vehicle_data *veh The vehicle that's acting.
* @param char *argument The typed-in arg.
*/
void vehicle_command_interpreter(vehicle_data *veh, char *argument) {
	char line[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];
	int cmd, length;
	
	skip_spaces(&argument);
	
	/* just drop to next line for hitting CR */
	if (!*argument) {
		return;
	}
	
	half_chop(argument, arg, line);
	
	/* find the command */
	for (length = strlen(arg), cmd = 0; *veh_cmd_info[cmd].command != '\n'; cmd++) {
		if (!strn_cmp(veh_cmd_info[cmd].command, arg, length)) {
			break;
		}
	}

	if (*veh_cmd_info[cmd].command == '\n') {
		veh_log(veh, "Unknown vehicle cmd: '%s'", argument);
	}
	else {
		((*veh_cmd_info[cmd].command_pointer) (veh, line, cmd, veh_cmd_info[cmd].subcmd));
	}
}
