/* ************************************************************************
*   File: dg_wldcmd.c                                     EmpireMUD 2.0b2 *
*  Usage: contains the command_interpreter for rooms, room commands.      *
*                                                                         *
*  DG Scripts code by galion, 1996/08/05 03:27:07, revision 3.12          *
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

// external functions
void send_char_pos(char_data *ch, int dam);
void die(char_data *ch, char_data *killer);
void sub_write(char *arg, char_data *ch, byte find_invis, int targets);
extern struct instance_data *find_instance_by_room(room_data *room);
char_data *get_char_by_room(room_data *room, char *name);
room_data *get_room(room_data *ref, char *name);
obj_data *get_obj_by_room(room_data *room, char *name);
char_data *get_char_in_room(room_data *room, char *name);
obj_data *get_obj_in_room(room_data *room, char *name);
void instance_obj_setup(struct instance_data *inst, obj_data *obj);
void scale_item_to_level(obj_data *obj, int level);
void scale_mob_to_level(char_data *mob, int level);
void wld_command_interpreter(room_data *room, char *argument);

// locals
#define WCMD(name)  void (name)(room_data *room, char *argument, int cmd, int subcmd)


struct wld_command_info {
	char *command;
	void (*command_pointer) (room_data *room, char *argument, int cmd, int subcmd);
	int subcmd;
};


/* do_wsend */
#define SCMD_WSEND        0
#define SCMD_WECHOAROUND  1


/**
* Determines the scale level for a room, with a character target as a backup
* for unscaled rooms.
*
* @param room_data *room The room to check.
* @param char_data *targ Optional: A character target.
* @return int A scale level.
*/
int get_room_scale_level(room_data *room, char_data *targ) {
	struct instance_data *inst;
	int level = 1;
	
	if (COMPLEX_DATA(room) && (inst = COMPLEX_DATA(room)->instance)) {
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
	// backup
	if (!level && targ) {
		level = get_approximate_level(targ);
	}
	
	return level;
}


/* attaches room vnum to msg and sends it to script_log */
void wld_log(room_data *room, const char *format, ...) {
	va_list args;
	char output[MAX_STRING_LENGTH];

	snprintf(output, sizeof(output), "Room %d :: %s", GET_ROOM_VNUM(room), format);

	va_start(args, format);
	script_vlog(output, args);
	va_end(args);
}

/* sends str to room */
void act_to_room(char *str, room_data *room) {
	/* no one is in the room */
	if (!room->people)
		return;

	/*
	* since you can't use act(..., TO_ROOM) for an room, send it
	* TO_ROOM and TO_CHAR for some char in the room.
	* (just dont use $n or you might get strange results)
	*/
	act(str, FALSE, room->people, 0, 0, TO_CHAR | TO_ROOM);
}


/* World commands */

WCMD(do_wadventurecomplete) {
	void mark_instance_completed(struct instance_data *inst);
	
	if (COMPLEX_DATA(room) && COMPLEX_DATA(room)->instance) {
		mark_instance_completed(COMPLEX_DATA(room)->instance);
	}
}


/* prints the argument to all the rooms aroud the room */
WCMD(do_wasound) {
	struct room_direction_data *newexit;
	int dir;
	room_data *to_room;
	bool map_echo = FALSE;

	skip_spaces(&argument);

	if (!*argument) {
		wld_log(room, "wasound called with no argument");
		return;
	}

	if (GET_ROOM_VNUM(room) < MAP_SIZE) {
		for (dir = 0; dir < NUM_2D_DIRS; ++dir) {
			if ((to_room = SHIFT_DIR(room, dir))) {
				act_to_room(argument, to_room);
			}
		}
		map_echo = TRUE;
	}

	if (COMPLEX_DATA(room)) {
		for (newexit = COMPLEX_DATA(room)->exits; newexit; newexit = newexit->next) {
			if ((to_room = newexit->room_ptr) && room != to_room) {
				// this skips rooms already hit by the direction shift
				if (!map_echo || GET_ROOM_VNUM(to_room) > MAP_SIZE) {
					act_to_room(argument, to_room);
				}
			}
		}
	}
}


WCMD(do_wecho) {
	skip_spaces(&argument);

	if (!*argument) 
		wld_log(room, "wecho called with no args");
	else 
		act_to_room(argument, room);
}


WCMD(do_wsend) {
	char buf[MAX_INPUT_LENGTH], *msg;
	char_data *ch;

	msg = any_one_arg(argument, buf);

	if (!*buf) {
		wld_log(room, "wsend called with no args");
		return;
	}

	skip_spaces(&msg);

	if (!*msg) {
		wld_log(room, "wsend called without a message");
		return;
	}

	if ((ch = get_char_by_room(room, buf))) {
		if (subcmd == SCMD_WSEND)
			sub_write(msg, ch, TRUE, TO_CHAR);
		else if (subcmd == SCMD_WECHOAROUND)
			sub_write(msg, ch, TRUE, TO_ROOM);
	}
	else
		wld_log(room, "no target found for wsend");
}

WCMD(do_wbuildingecho) {
	room_data *froom, *iter, *next_iter, *home;
	char room_num[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH], *msg;

	msg = any_one_word(argument, room_num);
	skip_spaces(&msg);

	if (!*room_num || !*msg)
		wld_log(room, "wbuildingecho called with too few args");
	else if (!(froom = get_room(room, room_num))) {
		wld_log(room, "wbuildingecho called with bad target");
	}
	else {
		home = HOME_ROOM(froom);
		sprintf(buf, "%s\r\n", msg);
		
		send_to_room(buf, home);
		HASH_ITER(interior_hh, interior_world_table, iter, next_iter) {
			if (HOME_ROOM(iter) == home && iter != home && ROOM_PEOPLE(iter)) {
				send_to_room(buf, iter);
			}
		}
	}
}


WCMD(do_wregionecho) {
	char room_number[MAX_INPUT_LENGTH], radius_arg[MAX_INPUT_LENGTH], *msg;
	descriptor_data *desc;
	room_data *center;
	char_data *targ;
	int radius;
	bool indoor_only = FALSE;

	argument = any_one_word(argument, room_number);
	msg = one_argument(argument, radius_arg);
	skip_spaces(&msg);

	if (!*room_number || !*radius_arg || !*msg) {
		wld_log(room, "wregionecho called with too few args");
	}
	else if (!isdigit(*radius_arg) && *radius_arg != '-') {
		wld_log(room, "wregionecho called with invalid radius");
	}
	else if (!(center = get_room(room, room_number))) {
		wld_log(room, "wregionecho called with invalid target");
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


WCMD(do_wdoor) {
	char target[MAX_INPUT_LENGTH], direction[MAX_INPUT_LENGTH];
	char field[MAX_INPUT_LENGTH], *value;
	room_data *rm, *to_room;
	struct room_direction_data *newexit, *temp;
	int dir, fd;

	const char *door_field[] = {
		"purge",
		"flags",
		"name",
		"room",
		"\n"
	};


	argument = one_word(argument, target);
	argument = one_argument(argument, direction);
	value = one_argument(argument, field);
	skip_spaces(&value);

	if (!*target || !*direction || !*field) {
		wld_log(room, "wdoor called with too few args");
		return;
	}

	if ((rm = get_room(room, target)) == NULL) {
		wld_log(room, "wdoor: invalid target");
		return;
	}

	if ((dir = search_block(direction, dirs, FALSE)) == NOTHING) {
		wld_log(room, "wdoor: invalid direction");
		return;
	}

	if ((fd = search_block(field, door_field, FALSE)) == NOTHING) {
		wld_log(room, "wdoor: invalid field");
		return;
	}
	
	if (!COMPLEX_DATA(rm)) {
		wld_log(room, "wdoor: called on room with no building data");
		return;
	}

	newexit = find_exit(rm, dir);

	/* purge exit */
	if (fd == 0) {
		if (newexit) {
			REMOVE_FROM_LIST(newexit, COMPLEX_DATA(rm)->exits, next);
			if (newexit->keyword)
				free(newexit->keyword);
			free(newexit);
		}
	}
	else {
		switch (fd) {
			case 1:  /* flags       */
				if (!newexit) {
					wld_log(room, "wdoor: invalid door");
					break;
				}
				newexit->exit_info = (sh_int)asciiflag_conv(value);
				break;
			case 2:  /* name        */
				if (!newexit) {
					wld_log(room, "wdoor: invalid door");
					break;
				}
				if (newexit->keyword)
					free(newexit->keyword);
				CREATE(newexit->keyword, char, strlen(value) + 1);
				strcpy(newexit->keyword, value);
				break;
			case 3:  /* room        */
				if ((to_room = real_room(atoi(value)))) {
					if (!newexit) {
						newexit = create_exit(rm, to_room, dir, FALSE);
					}
					else {
						newexit->to_room = GET_ROOM_VNUM(to_room);
						newexit->room_ptr = to_room;
					}
				}
				else
					wld_log(room, "wdoor: invalid door target");
				break;
		}
	}
}


WCMD(do_wteleport) {
	char_data *ch, *next_ch;
	room_data *target;
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct instance_data *inst;
	int iter;

	argument = one_argument(argument, arg1);
	argument = one_word(argument, arg2);

	if (!*arg1 || !*arg2) {
		wld_log(room, "wteleport called with too few args");
		return;
	}

	target = get_room(room, arg2);

	if (!target) {
		wld_log(room, "wteleport target is an invalid room");
		return;
	}

	if (!str_cmp(arg1, "all")) {
		if (target == room) {
			wld_log(room, "wteleport all target is itself");
			return;
		}

		for (ch = room->people; ch; ch = next_ch) {
			next_ch = ch->next_in_room;
			if (!valid_dg_target(ch, DG_ALLOW_GODS)) 
				continue;
			if (!IS_NPC(ch)) {
				GET_LAST_DIR(ch) = NO_DIR;
			}
			char_from_room(ch);
			char_to_room(ch, target);
			enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
		}
	}
	else if (!str_cmp(arg1, "adventure")) {
		// teleport all players in the adventure
		if (!(inst = ROOM_INSTANCE(room))) {
			wld_log(room, "wteleport: 'adventure' mode called outside any adventure");
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
		if ((ch = get_char_by_room(room, arg1))) {
			if (valid_dg_target(ch, DG_ALLOW_GODS)) {
				if (!IS_NPC(ch)) {
					GET_LAST_DIR(ch) = NO_DIR;
				}
				char_from_room(ch);
				char_to_room(ch, target);
				enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
			}
		}
		else
			wld_log(room, "wteleport: no target found");
	}
}


WCMD(do_wterracrop) {
	void do_dg_terracrop(room_data *target, crop_data *crop);

	char loc_arg[MAX_INPUT_LENGTH], crop_arg[MAX_INPUT_LENGTH];
	crop_data *crop;
	room_data *target;
	crop_vnum vnum;

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, crop_arg);
	
	// usage: %terracrop% [location] <crop vnum>
	if (!*loc_arg) {
		wld_log(room, "oterracrop: bad syntax");
		return;
	}
	
	// check number of args
	if (!*crop_arg) {
		// only arg is actually crop arg
		strcpy(crop_arg, loc_arg);
		target = room;
	}
	else {
		// two arguments
		target = get_room(room, loc_arg);
	}
	
	if (!target) {
		wld_log(room, "oterracrop: target is an invalid room");
		return;
	}
	
	// places you just can't terracrop -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*crop_arg) || (vnum = atoi(crop_arg)) < 0 || !(crop = crop_proto(vnum))) {
		wld_log(room, "oterracrop: invalid crop vnum");
		return;
	}

	// good to go
	do_dg_terracrop(target, crop);
}


WCMD(do_wterraform) {
	void do_dg_terraform(room_data *target, sector_data *sect);

	char loc_arg[MAX_INPUT_LENGTH], sect_arg[MAX_INPUT_LENGTH];
	sector_data *sect;
	room_data *target;
	sector_vnum vnum;

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, sect_arg);
	
	// usage: %terraform% [location] <sector vnum>
	if (!*loc_arg) {
		wld_log(room, "oterraform: bad syntax");
		return;
	}
	
	// check number of args
	if (!*sect_arg) {
		// only arg is actually sect arg
		strcpy(sect_arg, loc_arg);
		target = room;
	}
	else {
		// two arguments
		target = get_room(room, loc_arg);
	}
	
	if (!target) {
		wld_log(room, "oterraform: target is an invalid room");
		return;
	}
	
	// places you just can't terraform -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*sect_arg) || (vnum = atoi(sect_arg)) < 0 || !(sect = sector_proto(vnum))) {
		wld_log(room, "oterraform: invalid sector vnum");
		return;
	}
	
	// validate sect
	if (SECT_FLAGGED(sect, SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE)) {
		wld_log(room, "oterraform: sector requires data that can't be set this way");
		return;
	}

	// good to go
	do_dg_terraform(target, sect);
}


WCMD(do_wforce) {
	char_data *ch, *next_ch;
	char arg1[MAX_INPUT_LENGTH], *line;

	line = one_argument(argument, arg1);

	if (!*arg1 || !*line) {
		wld_log(room, "wforce called with too few args");
		return;
	}

	if (!str_cmp(arg1, "all")) {
		for (ch = room->people; ch; ch = next_ch) {
			next_ch = ch->next_in_room;

			if (valid_dg_target(ch, 0)) {
				command_interpreter(ch, line);
			}
		}
	}

	else {
		if ((ch = get_char_by_room(room, arg1))) {
			if (valid_dg_target(ch, 0)) {
				command_interpreter(ch, line);
			}
		}

		else
			wld_log(room, "wforce: no target found");
	}
}


/* purge all objects an npcs in room, or specified object or mob */
WCMD(do_wpurge) {
	char arg[MAX_INPUT_LENGTH];
	char_data *ch, *next_ch;
	obj_data *obj, *next_obj;

	one_argument(argument, arg);

	if (!*arg) {
		/* purge all */
		for (ch = room->people; ch; ch = next_ch ) {
			next_ch = ch->next_in_room;
			if (IS_NPC(ch))
				extract_char(ch);
		}

		for (obj = room->contents; obj; obj = next_obj ) {
			next_obj = obj->next_content;
			extract_obj(obj);
		}

		return;
	}

	if (*arg == UID_CHAR)
		ch = get_char(arg);
	else 
		ch = get_char_in_room(room, arg);

	if (!ch) {
		if (*arg == UID_CHAR)
			obj = get_obj(arg);
		else 
			obj = get_obj_in_room(room, arg);

		if (obj) {
			extract_obj(obj);
		}
		else 
			wld_log(room, "wpurge: bad argument");

		return;
	}

	if (!IS_NPC(ch)) {
		wld_log(room, "wpurge: purging a PC");
		return;
	}

	extract_char(ch);
}


/* loads a mobile or object into the room */
WCMD(do_wload) {
	void scale_mob_to_level(char_data *mob, int level);
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct instance_data *inst = find_instance_by_room(room);
	int number = 0;
	char_data *mob, *tch;
	obj_data *object, *cnt;
	char *target;
	int pos;

	target = two_arguments(argument, arg1, arg2);
	skip_spaces(&target);

	if (!*arg1 || !*arg2 || !is_number(arg2) || ((number = atoi(arg2)) < 0)) {
		wld_log(room, "wload: bad syntax");
		return;
	}

	if (is_abbrev(arg1, "mob")) {
		if ((mob = read_mobile(number)) == NULL) {
			wld_log(room, "wload: bad mob vnum");
			return;
		}
		
		// store instance id
		if (COMPLEX_DATA(room) && COMPLEX_DATA(room)->instance) {
			MOB_INSTANCE_ID(mob) = COMPLEX_DATA(room)->instance->id;
		}
		
		if (*target && isdigit(*target)) {
			scale_mob_to_level(mob, atoi(target));
		}
		else if (COMPLEX_DATA(room) && COMPLEX_DATA(room)->instance && COMPLEX_DATA(room)->instance->level > 0) {
			// instance level-locked
			scale_mob_to_level(mob, COMPLEX_DATA(room)->instance->level);
		}
		
		char_to_room(mob, room);
		setup_generic_npc(mob, NULL, NOTHING, NOTHING);
		load_mtrigger(mob);
	}

	else if (is_abbrev(arg1, "obj")) {
		if ((object = read_object(number)) == NULL) {
			wld_log(room, "wload: bad object vnum");
			return;
		}
		
		if (inst) {
			instance_obj_setup(inst, object);
		}
		
		/* special handling to make objects able to load on a person/in a container/worn etc. */
		if (!target || !*target) {
			obj_to_room(object, room);

			// adventure is level-locked?		
			if (inst && inst->level > 0) {
				scale_item_to_level(object, inst->level);
			}
		
			load_otrigger(object);
			return;
		}

		target = two_arguments(target, arg1, arg2); /* recycling ... */
		skip_spaces(&target);
		
		// scaling on request
		if (*target && isdigit(*target)) {
			scale_item_to_level(object, atoi(target));
		}
		else if ((inst = find_instance_by_room(room)) && inst->level > 0) {
			// scaling by locked adventure
			scale_item_to_level(object, inst->level);
		}
		
		tch = get_char_in_room(room, arg1);
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
		cnt = get_obj_in_room(room, arg1);
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

	else
		wld_log(room, "wload: bad type");
}


WCMD(do_wdamage) {
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH];
	double modifier = 1.0;
	char_data *ch;
	int type;

	argument = two_arguments(argument, name, modarg);
	argument = one_argument(argument, typearg);	// optional

	if (!*name) {
		wld_log(room, "wdamage: bad syntax");
		return;
	}

	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	
	ch = get_char_by_room(room, name);

	if (!ch) {
		wld_log(room, "wdamage: target not found");      
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			wld_log(room, "wdamage: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}

	script_damage(ch, NULL, get_room_scale_level(room, ch), type, modifier);
}


WCMD(do_waoe) {
	char modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH];
	char_data *vict, *next_vict;
	double modifier = 1.0;
	int level, type;

	two_arguments(argument, modarg, typearg);
	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			wld_log(room, "waoe: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	level = get_room_scale_level(room, NULL);
	for (vict = room->people; vict; vict = next_vict) {
		next_vict = vict->next_in_room;
		
		// harder to tell friend from foe: hit PCs or people following PCs
		if (!IS_NPC(vict) || (vict->master && !IS_NPC(vict->master))) {
			script_damage(vict, NULL, level, type, modifier);
		}
	}
}


WCMD(do_wdot) {
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], durarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH], stackarg[MAX_INPUT_LENGTH];
	double modifier = 1.0;
	char_data *ch;
	int type, max_stacks;

	argument = one_argument(argument, name);
	argument = one_argument(argument, modarg);
	argument = one_argument(argument, durarg);
	argument = one_argument(argument, typearg);	// optional, defualt: physical
	argument = one_argument(argument, stackarg);	// optional, defualt: 1

	if (!*name || !*modarg || !*durarg) {
		wld_log(room, "dot: bad syntax");
		return;
	}

	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	
	ch = get_char_by_room(room, name);

	if (!ch) {
		wld_log(room, "dot: target not found");      
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			wld_log(room, "wdamage: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}

	max_stacks = (*stackarg ? atoi(stackarg) : 1);
	script_damage_over_time(ch, get_room_scale_level(room, ch), type, modifier, atoi(durarg), max_stacks);
}


WCMD(do_wat) {
	char location[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	room_data *r2;

	half_chop(argument, location, arg2);

	if (!*location || !*arg2) {
		wld_log(room, "wat: bad syntax");
		return;
	}
	r2 = get_room(room, location);
	if (!r2) {
		wld_log(room, "wat: location not found");
		return;
	}

	wld_command_interpreter(r2, arg2);
}


WCMD(do_wscale) {
	char arg[MAX_INPUT_LENGTH], lvl_arg[MAX_INPUT_LENGTH];
	struct instance_data *inst;
	char_data *victim;
	obj_data *obj, *fresh, *proto;
	int level;

	two_arguments(argument, arg, lvl_arg);

	if (!*arg) {
		wld_log(room, "wscale: No target provided");
		return;
	}
	
	if (*lvl_arg && !isdigit(*lvl_arg)) {
		wld_log(room, "wscale: invalid level '%s'", lvl_arg);
		return;
	}
	else if (*lvl_arg) {
		level = atoi(lvl_arg);
	}
	else if ((inst = find_instance_by_room(room))) {
		level = inst->level;
	}
	else {
		level = 0;
	}
	
	if (level <= 0) {
		wld_log(room, "wscale: no valid level to scale to");
		return;
	}

	if (*arg == UID_CHAR) {
		victim = get_char(arg);
	}
	else {
		victim = get_char_in_room(room, arg);
	}

	if (!victim) {
		if (*arg == UID_CHAR)
			obj = get_obj(arg);
		else 
			obj = get_obj_in_room(room, arg);

		if (obj) {
			if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
				scale_item_to_level(obj, level);
			}
			else if ((proto = obj_proto(GET_OBJ_VNUM(obj))) && OBJ_FLAGGED(proto, OBJ_SCALABLE)) {
				fresh = read_object(GET_OBJ_VNUM(obj));
				scale_item_to_level(fresh, level);
				swap_obj_for_obj(obj, fresh);
				extract_obj(obj);
			}
			else {
				// attempt to scale anyway
				scale_item_to_level(obj, level);
			}
		}
		else 
			wld_log(room, "wscale: bad argument");

		return;
	}

	if (!IS_NPC(victim)) {
		wld_log(room, "wscale: unable to scale a PC");
		return;
	}

	scale_mob_to_level(victim, level);
}


const struct wld_command_info wld_cmd_info[] = {
	{ "RESERVED", NULL, NO_SCMD },/* this must be first -- for specprocs */

	{ "wadventurecomplete", do_wadventurecomplete, NO_SCMD },
	{ "wasound", do_wasound, NO_SCMD },
	{ "wdoor", do_wdoor, NO_SCMD },
	{ "wecho", do_wecho, NO_SCMD },
	{ "wechoaround", do_wsend, SCMD_WECHOAROUND },
	{ "wforce", do_wforce, NO_SCMD },
	{ "wload", do_wload, NO_SCMD },
	{ "wpurge", do_wpurge, NO_SCMD },
	{ "wscale", do_wscale, NO_SCMD },
	{ "wsend", do_wsend, SCMD_WSEND },
	{ "wteleport", do_wteleport, NO_SCMD },
	{ "wterracrop", do_wterracrop, NO_SCMD },
	{ "wterraform", do_wterraform, NO_SCMD },
	{ "wbuildingecho", do_wbuildingecho, NO_SCMD },
	{ "wregionecho", do_wregionecho, NO_SCMD },
	{ "wdamage", do_wdamage, NO_SCMD },
	{ "waoe", do_waoe, NO_SCMD },
	{ "wdot", do_wdot, NO_SCMD },
	{ "wat", do_wat, NO_SCMD },

	{ "\n", NULL, NO_SCMD }        /* this must be last */
};


/*
*  This is the command interpreter used by rooms, called by script_driver.
*/
void wld_command_interpreter(room_data *room, char *argument) {
	int cmd, length;
	char line[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];

	skip_spaces(&argument);

	/* just drop to next line for hitting CR */
	if (!*argument)
		return;

	half_chop(argument, arg, line);

	/* find the command */
	for (length = strlen(arg), cmd = 0; *wld_cmd_info[cmd].command != '\n'; cmd++)
		if (!strn_cmp(wld_cmd_info[cmd].command, arg, length))
			break;

	if (*wld_cmd_info[cmd].command == '\n')
		wld_log(room, "Unknown world cmd: '%s'", argument);
	else
		((*wld_cmd_info[cmd].command_pointer)(room, line, cmd, wld_cmd_info[cmd].subcmd));
}
