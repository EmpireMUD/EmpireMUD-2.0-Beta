/* ************************************************************************
*   File: dg_mobcmd.c                                     EmpireMUD 2.0b2 *
*  Usage: Script-related commands for mobs                                *
*                                                                         *
*  DG Scripts code came with the attributions in the next two blocks      *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */
/***************************************************************************
*  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
*  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
*                                                                         *
*  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
*  Chastain, Michael Quan, and Mitchell Tse.                              *
*                                                                         *
*  In order to use any part of this Merc Diku Mud, you must comply with   *
*  both the original Diku license in 'license.doc' as well the Merc       *
*  license in 'license.txt'.  In particular, you may not remove either of *
*  these copyright notices.                                               *
*                                                                         *
*  Much time and thought has gone into this software and you are          *
*  benefitting.  We hope that you share your changes too.  What goes      *
*  around, comes around.                                                  *
***************************************************************************/

/***************************************************************************
*  The MOBprograms have been contributed by N'Atas-ha.  Any support for   *
*  these routines should not be expected from Merc Industries.  However,  *
*  under no circumstances should the blame for bugs, etc be placed on     *
*  Merc Industries.  They are not guaranteed to work on all systems due   *
*  to their frequent use of strxxx functions.  They are also not the most *
*  efficient way to perform their tasks, but hopefully should be in the   *
*  easiest possible way to install and begin using. Documentation for     *
*  such installation can be found in INSTALL.  Enjoy........    N'Atas-Ha *
***************************************************************************/
// clean

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "dg_scripts.h"
#include "db.h"
#include "utils.h"
#include "handler.h"
#include "interpreter.h"
#include "comm.h"
#include "skills.h"
#include "vnums.h"

// external vars
extern int dg_owner_purged;
extern const char *damage_types[];
extern const char *dirs[];

// external funcs
void die(char_data *ch, char_data *killer);
extern struct instance_data *get_instance_by_mob(char_data *mob);
extern room_data *get_room(room_data *ref, char *name);
void instance_obj_setup(struct instance_data *inst, obj_data *obj);
extern struct instance_data *real_instance(any_vnum instance_id);
void send_char_pos(char_data *ch, int dam);
void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
void sub_write(char *arg, char_data *ch, byte find_invis, int targets);

/*
* Local functions.
*/

/**
* For scaled damage functions, allows mob flags to contribute.
*
* @param char_data *mob The mob.
* @param double modifier The existing modifier.
* @return double The new damage modifier;
*/
double scale_modifier_by_mob(char_data *mob, double modifier) {
	if (!mob || !IS_NPC(mob)) {
		return modifier;
	}

	if (MOB_FLAGGED(mob, MOB_DPS)) {
		modifier *= 1.25;
	}
	if (MOB_FLAGGED(mob, MOB_HARD)) {
		modifier *= 1.5;
	}
	if (MOB_FLAGGED(mob, MOB_GROUP)) {
		modifier *= 2.0;
	}
	
	return modifier;
}


/* attaches mob's name and vnum to msg and sends it to script_log */
void mob_log(char_data *mob, const char *format, ...) {
	va_list args;
	char output[MAX_STRING_LENGTH];

	snprintf(output, sizeof(output), "Mob (%s, VNum %d):: %s", GET_SHORT(mob), GET_MOB_VNUM(mob), format);

	va_start(args, format);
	script_vlog(output, args);
	va_end(args);
}

/*
** macro to determine if a mob is permitted to use these commands
*/
#define MOB_OR_IMPL(ch) (IS_NPC(ch) && (!(ch)->desc || GET_ACCESS_LEVEL((ch)->desc->original) >= LVL_CIMPL))

/* mob commands */

ACMD(do_madventurecomplete) {
	void mark_instance_completed(struct instance_data *inst);
	struct instance_data *inst;
	
	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED)) {
		return;
	}
	
	if ((inst = get_instance_by_mob(ch))) {
		mark_instance_completed(inst);
	}
}


/* prints the argument to all the rooms aroud the mobile */
ACMD(do_masound) {
	struct room_direction_data *ex;
	room_data *was_in_room;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (!*argument) {
		mob_log(ch, "masound called with no argument");
		return;
	}

	skip_spaces(&argument);

	was_in_room = IN_ROOM(ch);
	if (COMPLEX_DATA(IN_ROOM(ch))) {
		for (ex = COMPLEX_DATA(IN_ROOM(ch))->exits; ex; ex = ex->next) {
			if (ex->room_ptr && ex->room_ptr != was_in_room) {
				IN_ROOM(ch) = ex->room_ptr;
				sub_write(argument, ch, TRUE, TO_ROOM);
			}
		}
	}

	IN_ROOM(ch) = was_in_room;
}


/* lets the mobile kill any player or mobile without murder*/
ACMD(do_mkill) {	
	char arg[MAX_INPUT_LENGTH];
	char_data *victim;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	one_argument(argument, arg);

	if (!*arg) {
		mob_log(ch, "mkill called with no argument");
		return;
	}

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "mkill: victim (%s) not found", arg);
			return;
		}
	}
	else if (!(victim = get_char_room_vis(ch, arg))) {
		mob_log(ch, "mkill: victim (%s) not found",arg);
		return;
	}

	if (victim == ch) {
		mob_log(ch, "mkill: victim is self");
		return;
	}

	if (!valid_dg_target(victim, DG_ALLOW_GODS)) {
		mob_log(ch, "mkill: target is invalid");
		return;
	}
	
	if (!can_fight(ch, victim)) {
		mob_log(ch, "mkill: !can_fight");
		return;
	}

	// start fight!
	hit(ch, victim, GET_EQ(ch, WEAR_WIELD), FALSE);
	
	// ensure hitting the right person
	if (victim && !EXTRACTED(victim) && !IS_DEAD(victim) && FIGHTING(ch) && FIGHTING(ch) != victim) {
		FIGHTING(ch) = victim;
	}
	
	return;
}


/*
* lets the mobile destroy an object in its inventory
* it can also destroy a worn object and it can destroy 
* items using all.xxxxx or just plain all of them
*/
ACMD(do_mjunk) {
	char arg[MAX_INPUT_LENGTH];
	int pos, junk_all = 0;
	obj_data *obj, *obj_next;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	one_argument(argument, arg);

	if (!*arg) {
		mob_log(ch, "mjunk called with no argument");
		return;
	}

	if (!str_cmp(arg, "all"))
		junk_all = 1;

	if ((find_all_dots(arg) == FIND_INDIV) && !junk_all) { 
		/* Thanks to Carlos Myers for fixing the line below */
		if ((pos = get_obj_pos_in_equip_vis(ch, arg, ch->equipment)) >= 0) {
			extract_obj(unequip_char(ch, pos));
			return;
		}
		if ((obj = get_obj_in_list_vis(ch, arg, ch->carrying)) != NULL)
			extract_obj(obj);
		return;
	}
	else {
		for (obj = ch->carrying; obj != NULL; obj = obj_next) {
			obj_next = obj->next_content;
			// "all" or "all.name ?
			if (arg[3] == '\0' || isname(arg+4, obj->name)) {
				extract_obj(obj);
			}
		}
		/* Thanks to Carlos Myers for fixing the line below */
		while ((pos = get_obj_pos_in_equip_vis(ch, arg, ch->equipment)) >= 0)
			extract_obj(unequip_char(ch, pos));
	}
	return;
}


/* prints the message to everyone in the room other than the mob and victim */
ACMD(do_mechoaround) {
	char arg[MAX_INPUT_LENGTH];
	char_data *victim;
	char *p;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	p = one_argument(argument, arg);
	skip_spaces(&p);

	if (!*arg) {
		mob_log(ch, "mechoaround called with no argument");
		return;
	}

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "mechoaround: victim (%s) does not exist",arg);
			return;
		}
	}
	else if (!(victim = get_char_room_vis(ch, arg))) {
		mob_log(ch, "mechoaround: victim (%s) does not exist",arg);
		return;
	}

	sub_write(p, victim, TRUE, TO_ROOM);
}


// prints the message to everyone except two targets
ACMD(do_mechoneither) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	char_data *vict1, *vict2, *iter;
	char *p;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	p = two_arguments(argument, arg1, arg2);
	skip_spaces(&p);

	if (!*arg1 || !*arg2 || !*p) {
		mob_log(ch, "mechoneither called with missing arguments");
		return;
	}

	if (*arg1 == UID_CHAR) {
		if (!(vict1 = get_char(arg1))) {
			mob_log(ch, "mechoneither: vict 1 (%s) does not exist", arg1);
			return;
		}
	}
	else if (!(vict1 = get_char_room_vis(ch, arg1))) {
		mob_log(ch, "mechoneither: vict 1 (%s) does not exist", arg1);
		return;
	}

	if (*arg2 == UID_CHAR) {
		if (!(vict2 = get_char(arg2))) {
			mob_log(ch, "mechoneither: vict 2 (%s) does not exist", arg2);
			return;
		}
	}
	else if (!(vict2 = get_char_room_vis(ch, arg2))) {
		mob_log(ch, "mechoneither: vict 2 (%s) does not exist", arg2);
		return;
	}

	for (iter = ROOM_PEOPLE(IN_ROOM(vict1)); iter; iter = iter->next_in_room) {
		if (iter->desc && iter != vict1 && iter != vict2) {
			sub_write(p, iter, TRUE, TO_CHAR);
		}
	}
}


/* sends the message to only the victim */
ACMD(do_msend) {
	char arg[MAX_INPUT_LENGTH];
	char_data *victim;
	char *p;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	p = one_argument(argument, arg);
	skip_spaces(&p);

	if (!*arg) {
		mob_log(ch, "msend called with no argument");
		return;
	}

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "msend: victim (%s) does not exist",arg);
			return;
		}
	}
	else if (!(victim = get_char_room_vis(ch, arg))) {
		mob_log(ch, "msend: victim (%s) does not exist",arg);
		return;
	}

	sub_write(p, victim, TRUE, TO_CHAR);
}


/* prints the message to the room at large */
ACMD(do_mecho) {
	char *p;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (!*argument) {
		mob_log(ch, "mecho called with no arguments");
		return;
	}
	p = argument;
	skip_spaces(&p);

	sub_write(p, ch, TRUE, TO_ROOM);
}


ACMD(do_mbuildingecho) {
	room_data *home_room, *iter, *next_iter;
	char room_number[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH], *msg;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	msg = any_one_word(argument, room_number);
	skip_spaces(&msg);

	if (!*room_number || !*msg) {
		mob_log(ch, "mbuildingecho called with too few args");
	}
	else if (!(home_room = find_target_room(ch, room_number))) {
		mob_log(ch, "mbuildingecho called with invalid target");
	}
	else {
		home_room = HOME_ROOM(home_room);	// right?
		
		sprintf(buf, "%s\r\n", msg);
		
		send_to_room(buf, home_room);
		HASH_ITER(interior_hh, interior_world_table, iter, next_iter) {
			if (HOME_ROOM(iter) == home_room && iter != home_room && ROOM_PEOPLE(iter)) {
				send_to_room(buf, iter);
			}
		}
	}
}


ACMD(do_mregionecho) {
	char room_number[MAX_INPUT_LENGTH], radius_arg[MAX_INPUT_LENGTH], *msg;
	descriptor_data *desc;
	room_data *center;
	char_data *targ;
	int radius;
	bool indoor_only = FALSE;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	argument = any_one_word(argument, room_number);
	msg = one_argument(argument, radius_arg);
	skip_spaces(&msg);

	if (!*room_number || !*radius_arg || !*msg) {
		mob_log(ch, "mregionecho called with too few args");
	}
	else if (!isdigit(*radius_arg) && *radius_arg != '-') {
		mob_log(ch, "mregionecho called with invalid radius");
	}
	else if (!(center = find_target_room(ch, room_number))) {
		mob_log(ch, "mregionecho called with invalid target");
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


/*
* lets the mobile load an item or mobile.  All items
* are loaded into inventory, unless it is NO-TAKE. 
*/
ACMD(do_mload) {
	void scale_item_to_level(obj_data *obj, int level);
	void scale_mob_to_level(char_data *mob, int level);
	
	struct instance_data *inst = get_instance_by_mob(ch);
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	int number = 0;
	char_data *mob, *tch;
	obj_data *object, *cnt;
	char *target;
	int pos;
	bool ally = FALSE;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL) {
		return;
	}

	target = two_arguments(argument, arg1, arg2);
	skip_spaces(&target);

	if (!*arg1 || !*arg2 || !is_number(arg2) || ((number = atoi(arg2)) < 0)) {
		mob_log(ch, "mload: bad syntax");
		return;
	}

	if (is_abbrev(arg1, "mob")) {
		if ((mob = read_mobile(number, TRUE)) == NULL) {
			mob_log(ch, "mload: bad mob vnum");
			return;
		}
		MOB_INSTANCE_ID(mob) = MOB_INSTANCE_ID(ch);
		char_to_room(mob, IN_ROOM(ch));
		setup_generic_npc(mob, GET_LOYALTY(ch), NOTHING, NOTHING);
		
		// check ally request
		if (!strn_cmp(target, "ally", 4)) {
			ally = TRUE;
			target = one_argument(target, arg1);	// clear out the ally
			skip_spaces(&target);
		}
		
		if (*target && isdigit(*target)) {
			// scale to requested level
			scale_mob_to_level(mob, atoi(target));
		}
		else if (GET_CURRENT_SCALE_LEVEL(ch) > 0) {
			// only scale mob if self is scaled
			scale_mob_to_level(mob, GET_CURRENT_SCALE_LEVEL(ch));
		}
		
		load_mtrigger(mob);
		
		if (ally) {
			add_follower(mob, ch, FALSE);
		}
	}
	else if (is_abbrev(arg1, "obj")) {
		if ((object = read_object(number, TRUE)) == NULL) {
			mob_log(ch, "mload: bad object vnum");
			return;
		}
		
		if (inst) {
			instance_obj_setup(inst, object);
		}
		
		/* special handling to make objects able to load on a person/in a container/worn etc. */
		if (!target || !*target) {
			if (CAN_WEAR(object, ITEM_WEAR_TAKE)) {
				obj_to_char(object, ch);
			}
			else {
				obj_to_room(object, IN_ROOM(ch));
			}
		
			// must scale now
			scale_item_to_level(object, GET_CURRENT_SCALE_LEVEL(ch));
		
			load_otrigger(object);
			return;
		}
		
		target = two_arguments(target, arg1, arg2); /* recycling ... */
		skip_spaces(&target);
		
		if (*target && isdigit(*target)) {
			scale_item_to_level(object, atoi(target));
		}
		else {
			scale_item_to_level(object, GET_CURRENT_SCALE_LEVEL(ch));
		}
		
		tch = (*arg1 == UID_CHAR) ? get_char(arg1) : get_char_room_vis(ch, arg1);
		if (tch) {
			if (*arg2 && (pos = find_eq_pos_script(arg2)) >= 0 && !GET_EQ(tch, pos) && can_wear_on_pos(object, pos)) {
				equip_char(tch, object, pos);
				load_otrigger(object);
				return;
			}
			obj_to_char(object, tch);
			load_otrigger(object);
			return;
		}
		cnt = (*arg1 == UID_CHAR) ? get_obj(arg1) : get_obj_vis(ch, arg1);
		if (cnt && GET_OBJ_TYPE(cnt) == ITEM_CONTAINER) {
			obj_to_obj(object, cnt);
			load_otrigger(object);
			return;
		}

		/* neither char nor container found - just dump it in room */
		obj_to_room(object, IN_ROOM(ch)); 
		load_otrigger(object);
		return;
	}
	else
		mob_log(ch, "mload: bad type");
}


/*
* lets the mobile purge all objects and other npcs in the room,
* or purge a specified object or mob in the room.  It can purge
*  itself, but this will be the last command it does.
*/
ACMD(do_mpurge) {
	char arg[MAX_INPUT_LENGTH];
	char_data *victim;
	obj_data *obj;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	one_argument(argument, arg);

	if (!*arg) {
		/* 'purge' */
		char_data *vnext;
		obj_data *obj_next;

		for (victim = ROOM_PEOPLE(IN_ROOM(ch)); victim; victim = vnext) {
			vnext = victim->next_in_room;
			if (IS_NPC(victim) && victim != ch)
				extract_char(victim);
		}

		for (obj = ROOM_CONTENTS(IN_ROOM(ch)); obj; obj = obj_next) {
			obj_next = obj->next_content;
			extract_obj(obj);
		}

		return;
	}

	if (*arg == UID_CHAR)
		victim = get_char(arg);
	else
		victim = get_char_room_vis(ch, arg);

	if (victim == NULL) {
		if (*arg == UID_CHAR)
			obj = get_obj(arg);
		else 
			obj = get_obj_vis(ch, arg);

		if (obj) {
			extract_obj(obj);
			obj = NULL;
		}
		else 
			mob_log(ch, "mpurge: bad argument");

		return;
	}

	if (!IS_NPC(victim)) {
		mob_log(ch, "mpurge: purging a PC");
		return;
	}

	if (victim == ch)
		dg_owner_purged = 1;

	extract_char(victim);
}


/* lets the mobile goto any location it wishes that is not private */
ACMD(do_mgoto) {
	char arg[MAX_INPUT_LENGTH];
	room_data *location;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	one_word(argument, arg);

	if (!*arg) {
		mob_log(ch, "mgoto called with no argument");
		return;
	}

	if (!(location = find_target_room(ch, arg))) {
		mob_log(ch, "mgoto: invalid location");
		return;
	}

	if (FIGHTING(ch))
		stop_fighting(ch);

	char_from_room(ch);
	char_to_room(ch, location);
	enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
}


/* lets the mobile do a command at another location. Very useful */
ACMD(do_mat) {
	char arg[MAX_INPUT_LENGTH];
	room_data *location = NULL, *original;
	struct instance_data *inst;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	argument = one_word(argument, arg);

	if (!*arg || !*argument) {
		mob_log(ch, "mat: bad argument");
		return;
	}

	// special case for use of i### room-template targeting when mob is outside its instance
	if (arg[0] == 'i' && isdigit(arg[1]) && MOB_INSTANCE_ID(ch) != NOTHING && (inst = real_instance(MOB_INSTANCE_ID(ch))) && inst->start) {
		// I know that's a lot to check but we want i###-targeting to work when a mob wanders out -pc 4/13/2015
		location = get_room(inst->start, arg);
	}
	else {
		location = get_room(IN_ROOM(ch), arg);
	}

	if (!location) {
		mob_log(ch, "mat: invalid location");
		return;
	}

	original = IN_ROOM(ch);
	char_from_room(ch);
	char_to_room(ch, location);
	command_interpreter(ch, argument);

	/*
	* See if 'ch' still exists before continuing!
	* Handles 'at XXXX quit' case.
	*/
	if (IN_ROOM(ch) == location) {
		char_from_room(ch);
		char_to_room(ch, original);
	}
}


/*
* lets the mobile transfer people.  the all argument transfers
* everyone in the current room to the specified location
*/
ACMD(do_mteleport) {
	extern struct instance_data *find_instance_by_room(room_data *room);
	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	room_data *target;
	char_data *vict, *next_ch;
	struct instance_data *inst;
	int iter;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	argument = one_argument(argument, arg1);
	argument = one_word(argument, arg2);

	if (!*arg1 || !*arg2) {
		mob_log(ch, "mteleport: bad syntax");
		return;
	}

	target = find_target_room(ch, arg2);

	if (!target) {
		mob_log(ch, "mteleport target is an invalid room");
		return;
	}

	if (!str_cmp(arg1, "all")) {
		if (target == IN_ROOM(ch)) {
			mob_log(ch, "mteleport all target is itself");
			return;
		}

		for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = next_ch) {
			next_ch = vict->next_in_room;

			if (valid_dg_target(vict, DG_ALLOW_GODS)) {
				if (!IS_NPC(vict)) {
					GET_LAST_DIR(vict) = NO_DIR;
				}
				char_from_room(vict);
				char_to_room(vict, target);
				enter_wtrigger(IN_ROOM(vict), vict, NO_DIR);
			}
		}
	}
	else if (!str_cmp(arg1, "adventure")) {
		// teleport all players in the adventure
		if (!(inst = real_instance(MOB_INSTANCE_ID(ch)))) {
			mob_log(ch, "mteleport: 'adventure' mode called on non-adventure mob");
			return;
		}
		
		for (iter = 0; iter < inst->size; ++iter) {
			// only if it's not the target room, or we'd be here all day
			if (inst->room[iter] && inst->room[iter] != target) {
				for (vict = ROOM_PEOPLE(inst->room[iter]); vict; vict = next_ch) {
					next_ch = vict->next_in_room;
					
					if (!valid_dg_target(vict, DG_ALLOW_GODS)) {
						continue;
					}
					
					// teleport players and their followers
					if (!IS_NPC(vict) || (vict->master && !IS_NPC(vict->master))) {
						char_from_room(vict);
						char_to_room(vict, target);
						if (!IS_NPC(vict)) {
							GET_LAST_DIR(vict) = NO_DIR;
						}
						enter_wtrigger(IN_ROOM(vict), ch, NO_DIR);
					}
				}
			}
		}
	}
	else {
		if (*arg1 == UID_CHAR) {
			if (!(vict = get_char(arg1))) {
				mob_log(ch, "mteleport: victim (%s) does not exist",arg1);
				return;
			}
		}
		else if (!(vict = get_char_vis(ch, arg1, FIND_CHAR_WORLD))) {
			mob_log(ch, "mteleport: victim (%s) does not exist",arg1);
			return;
		}

		if (valid_dg_target(vict, DG_ALLOW_GODS)) {
			if (!IS_NPC(vict)) {
				GET_LAST_DIR(vict) = NO_DIR;
			}
			char_from_room(vict);
			char_to_room(vict, target);
			enter_wtrigger(IN_ROOM(vict), vict, NO_DIR);
		}
	}
}


ACMD(do_mterracrop) {
	void do_dg_terracrop(room_data *target, crop_data *cp);

	char loc_arg[MAX_INPUT_LENGTH], crop_arg[MAX_INPUT_LENGTH];
	crop_data *crop;
	room_data *target;
	crop_vnum vnum;

	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, crop_arg);
	
	// usage: %terracrop% [location] <crop vnum>
	if (!*loc_arg) {
		mob_log(ch, "mterracrop: bad syntax");
		return;
	}
	
	// check number of args
	if (!*crop_arg) {
		// only arg is actually crop arg
		strcpy(crop_arg, loc_arg);
		target = IN_ROOM(ch);
	}
	else {
		// two arguments
		target = find_target_room(ch, loc_arg);
	}
	
	if (!target) {
		mob_log(ch, "mterracrop: target is an invalid room");
		return;
	}
	
	// places you just can't terracrop -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*crop_arg) || (vnum = atoi(crop_arg)) < 0 || !(crop = crop_proto(vnum))) {
		mob_log(ch, "mterracrop: invalid crop vnum");
		return;
	}

	// good to go
	do_dg_terracrop(target, crop);
}


ACMD(do_mterraform) {
	void do_dg_terraform(room_data *target, sector_data *sect);

	char loc_arg[MAX_INPUT_LENGTH], sect_arg[MAX_INPUT_LENGTH];
	sector_data *sect;
	room_data *target;
	sector_vnum vnum;

	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, sect_arg);
	
	// usage: %terraform% [location] <sector vnum>
	if (!*loc_arg) {
		mob_log(ch, "mterraform: bad syntax");
		return;
	}
	
	// check number of args
	if (!*sect_arg) {
		// only arg is actually sect arg
		strcpy(sect_arg, loc_arg);
		target = IN_ROOM(ch);
	}
	else {
		// two arguments
		target = find_target_room(ch, loc_arg);
	}
	
	if (!target) {
		mob_log(ch, "mterraform: target is an invalid room");
		return;
	}
	
	// places you just can't terraform -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*sect_arg) || (vnum = atoi(sect_arg)) < 0 || !(sect = sector_proto(vnum))) {
		mob_log(ch, "mterraform: invalid sector vnum");
		return;
	}
	
	// validate sect
	if (SECT_FLAGGED(sect, SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE)) {
		mob_log(ch, "mterraform: sector requires data that can't be set this way");
		return;
	}

	// good to go
	do_dg_terraform(target, sect);
}


ACMD(do_mdamage) {
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH];
	double modifier = 1.0;
	char_data *vict;
	int type;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	argument = two_arguments(argument, name, modarg);
	argument = one_argument(argument, typearg);	// optional

	/* who cares if it's a number ? if not it'll just be 0 */
	if (!*name) {
		mob_log(ch, "mdamage: bad syntax");
		return;
	}
	
	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	modifier *= scale_modifier_by_mob(ch, modifier);

	if (*name == UID_CHAR) {
		if (!(vict = get_char(name))) {
			mob_log(ch, "mdamage: victim (%s) does not exist", name);
			return;
		}
	}
	else if (!(vict = get_char_room_vis(ch, name))) {
		mob_log(ch, "mdamage: victim (%s) does not exist", name);
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			mob_log(ch, "mdamage: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	script_damage(vict, ch, get_approximate_level(ch), type, modifier);
}


ACMD(do_maoe) {
	extern bool is_fight_enemy(char_data *ch, char_data *frenemy);	// fight.c

	char modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH];
	double modifier = 1.0;
	int level, type;
	char_data *vict, *next_vict;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	two_arguments(argument, modarg, typearg);
	
	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			mob_log(ch, "maoe: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	modifier *= scale_modifier_by_mob(ch, modifier);
	
	level = get_approximate_level(ch);
	for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = next_vict) {
		next_vict = vict->next_in_room;
		
		if (ch != vict && is_fight_enemy(ch, vict)) {
			script_damage(vict, ch, level, type, modifier);
		}
	}
}


ACMD(do_mdot) {
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], durarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH], stackarg[MAX_INPUT_LENGTH];
	double modifier = 1.0;
	char_data *vict;
	int type, max_stacks;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	argument = one_argument(argument, name);
	argument = one_argument(argument, modarg);
	argument = one_argument(argument, durarg);
	argument = one_argument(argument, typearg);	// optional, default physical
	argument = one_argument(argument, stackarg);	// optional, default 1

	if (!*name || !*modarg || !*durarg) {
		mob_log(ch, "mdot: bad syntax");
		return;
	}
	
	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	modifier *= scale_modifier_by_mob(ch, modifier);

	if (*name == UID_CHAR) {
		if (!(vict = get_char(name))) {
			mob_log(ch, "mdot: victim (%s) does not exist", name);
			return;
		}
	}
	else if (!(vict = get_char_room_vis(ch, name))) {
		mob_log(ch, "mdot: victim (%s) does not exist", name);
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			mob_log(ch, "mdot: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	max_stacks = (*stackarg ? atoi(stackarg) : 1);
	script_damage_over_time(vict, get_approximate_level(ch), type, modifier, atoi(durarg), max_stacks, ch);
}


/*
* lets the mobile force someone to do something.  must be mortal level
* and the all argument only affects those in the room with the mobile
*/
ACMD(do_mforce) {
	char arg[MAX_INPUT_LENGTH];

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	argument = one_argument(argument, arg);

	if (!*arg || !*argument) {
		mob_log(ch, "mforce: bad syntax");
		return;
	}

	if (!str_cmp(arg, "all")) {
		descriptor_data *i;
		char_data *vch;

		for (i = descriptor_list; i ; i = i->next) {
			if ((i->character != ch) && !i->connected && (IN_ROOM(i->character) == IN_ROOM(ch)) && STATE(i) == CON_PLAYING) {
				vch = i->character;
				if (GET_ACCESS_LEVEL(vch) < GET_ACCESS_LEVEL(ch) && CAN_SEE(ch, vch) && valid_dg_target(vch, 0)) {
					command_interpreter(vch, argument);
				}
			}
		}
	}
	else {
		char_data *victim;

		if (*arg == UID_CHAR) {
			if (!(victim = get_char(arg))) {
				mob_log(ch, "mforce: victim (%s) does not exist",arg);
				return;
			}
		}
		else if ((victim = get_char_room_vis(ch, arg)) == NULL) {
			mob_log(ch, "mforce: no such victim");
			return;
		}

		if (victim == ch) {
			mob_log(ch, "mforce: forcing self");
			return;
		}

		if (valid_dg_target(victim, 0))
			command_interpreter(victim, argument);
	}
}

/* hunt for someone */
ACMD(do_mhunt) {
	void add_pursuit(char_data *ch, char_data *target);
	
	char_data *victim;
	char arg[MAX_INPUT_LENGTH];

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (!IS_NPC(ch)) {
		msg_to_char(ch, "Only mobs can use this.\r\n");
		return;
	}

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	one_argument(argument, arg);

	if (!*arg) {
		mob_log(ch, "mhunt called with no argument");
		return;
	}

	if (FIGHTING(ch))
		return;

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "mhunt: victim (%s) does not exist", arg);
			return;
		}
	}
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_WORLD))) {
		mob_log(ch, "mhunt: victim (%s) does not exist", arg);
		return;
	}
	
	add_pursuit(ch, victim);
	SET_BIT(MOB_FLAGS(ch), MOB_PURSUE);
}


/* place someone into the mob's memory list */
ACMD(do_mremember) {
	char_data *victim;
	struct script_memory *mem;
	char arg[MAX_INPUT_LENGTH];

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	argument = one_argument(argument, arg);

	if (!*arg) {
		mob_log(ch, "mremember: bad syntax");
		return;
	}

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "mremember: victim (%s) does not exist", arg);
			return;
		}
	}
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_WORLD))) {
		mob_log(ch,"mremember: victim (%s) does not exist", arg);
		return;
	}

	/* create a structure and add it to the list */
	CREATE(mem, struct script_memory, 1);
	if (!SCRIPT_MEM(ch))
		SCRIPT_MEM(ch) = mem;
	else {
		struct script_memory *tmpmem = SCRIPT_MEM(ch);
		while (tmpmem->next)
			tmpmem = tmpmem->next;
		tmpmem->next = mem;
	}

	/* fill in the structure */
	mem->id = GET_ID(victim);
	if (argument && *argument) {
		mem->cmd = strdup(argument);
	}
}


/* remove someone from the list */
ACMD(do_mforget) {
	char_data *victim;
	struct script_memory *mem, *prev;
	char arg[MAX_INPUT_LENGTH];

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	one_argument(argument, arg);

	if (!*arg) {
		mob_log(ch, "mforget: bad syntax");
		return;
	}

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "mforget: victim (%s) does not exist", arg);
			return;
		}
	}
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_WORLD))) {
		mob_log(ch, "mforget: victim (%s) does not exist", arg);
		return;
	}

	mem = SCRIPT_MEM(ch);
	prev = NULL;
	while (mem) {
		if (mem->id == GET_ID(victim)) {
			if (mem->cmd)
				free(mem->cmd);
			if (prev==NULL) {
				SCRIPT_MEM(ch) = mem->next;
				free(mem);
				mem = SCRIPT_MEM(ch);
			}
			else {
				prev->next = mem->next;
				free(mem);
				mem = prev->next;
			}
		}
		else {
			prev = mem;
			mem = mem->next;
		}
	}
}


/* transform into a different mobile */
ACMD(do_mtransform) {
	char arg[MAX_INPUT_LENGTH];
	char_data *m, tmpmob;
	obj_data *obj[NUM_WEARS];
	mob_vnum this_vnum = GET_MOB_VNUM(ch);
	bool keep_attr = TRUE; // new mob keeps the old mob's h/v/m/b
	int pos, iter;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc || !IS_NPC(ch)) {
		msg_to_char(ch, "You've got no VNUM to return to, dummy! try 'switch'\r\n");
		return;
	}

	one_argument(argument, arg);

	if (TRUE) {
		mob_log(ch, "mtransform is currently disabled");
	}
	else if (!*arg)
		mob_log(ch, "mtransform: missing argument");
	else if (!isdigit(*arg) && *arg != '-')
		mob_log(ch, "mtransform: bad argument");
	else {
		if (isdigit(*arg))
			m = read_mobile(atoi(arg), TRUE);
		else {
			keep_attr = 0;
			m = read_mobile(atoi(arg+1), TRUE);
		}
		if (m == NULL) {
			mob_log(ch, "mtransform: bad mobile vnum");
			return;
		}
		
		MOB_INSTANCE_ID(m) = MOB_INSTANCE_ID(ch);
		
		setup_generic_npc(m, GET_LOYALTY(ch), MOB_DYNAMIC_NAME(ch), MOB_DYNAMIC_SEX(ch));

		/* move new obj info over to old object and delete new obj */

		for (pos = 0; pos < NUM_WEARS; pos++) {
			if (GET_EQ(ch, pos))
				obj[pos] = unequip_char(ch, pos);
			else
				obj[pos] = NULL;
		}

		/* put the mob in the same room as ch so extract will work */
		char_to_room(m, IN_ROOM(ch));

		memcpy(&tmpmob, m, sizeof(*m));
		tmpmob.id = ch->id;
		tmpmob.affected = ch->affected;
		tmpmob.carrying = ch->carrying;
		tmpmob.proto_script = ch->proto_script;
		tmpmob.script = ch->script;
		tmpmob.memory = ch->memory;
		tmpmob.next_in_room = ch->next_in_room;
		tmpmob.next = ch->next;
		tmpmob.next_fighting = ch->next_fighting;
		tmpmob.followers = ch->followers;
		tmpmob.master = ch->master;
		tmpmob.cooldowns = ch->cooldowns;
	
		if (keep_attr) {
			for (iter = 0; iter < NUM_POOLS; ++iter) {
				GET_CURRENT_POOL(&tmpmob, iter) = GET_CURRENT_POOL(ch, iter);
				GET_MAX_POOL(&tmpmob, iter) = GET_MAX_POOL(ch, iter);
			}
		}
		GET_POS(&tmpmob) = GET_POS(ch);
		IS_CARRYING_N(&tmpmob) = IS_CARRYING_N(ch);
		FIGHTING(&tmpmob) = FIGHTING(ch);
		HUNTING(&tmpmob) = HUNTING(ch);
		memcpy(ch, &tmpmob, sizeof(*ch));

		for (pos = 0; pos < NUM_WEARS; pos++) {
			if (obj[pos])
				equip_char(ch, obj[pos], pos);
		}

		ch->vnum = this_vnum;
		extract_char(m);
	}
}


ACMD(do_mdoor) {
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


	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	argument = one_word(argument, target);
	argument = one_argument(argument, direction);
	value = one_argument(argument, field);
	skip_spaces(&value);

	if (!*target || !*direction || !*field) {
		mob_log(ch, "mdoor called with too few args");
		return;
	}

	if (!(rm = find_target_room(ch, target))) {
		mob_log(ch, "mdoor: invalid target");
		return;
	}

	if ((dir = search_block(direction, dirs, FALSE)) == NO_DIR) {
		mob_log(ch, "mdoor: invalid direction");
		return;
	}

	if ((fd = search_block(field, door_field, FALSE)) == NOTHING) {
		mob_log(ch, "mdoor: invalid field");
		return;
	}
	
	if (!COMPLEX_DATA(rm)) {
		mob_log(ch, "mdoor called on room with no building data");
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
					mob_log(ch, "mdoor: invalid door");
					break;
				}
				newexit->exit_info = (sh_int)asciiflag_conv(value);
				break;
			case 2:  /* name        */
				if (!newexit) {
					mob_log(ch, "mdoor: invalid door");
					break;
				}
				if (newexit->keyword)
					free(newexit->keyword);
				newexit->keyword = str_dup(value);
				break;
			case 3:  /* room        */
				if ((to_room = find_target_room(ch, value))) {
					if (!newexit) {
						newexit = create_exit(rm, to_room, dir, FALSE);
					}
					else {
						newexit->to_room = GET_ROOM_VNUM(to_room);
						newexit->room_ptr = to_room;
					}
				}
				else
					mob_log(ch, "mdoor: invalid door target");
				break;
		}
	}
}


ACMD(do_mfollow) {
	extern bool circle_follow(char_data *ch, char_data *victim);
	
	char buf[MAX_INPUT_LENGTH];
	char_data *leader;
	struct follow_type *j, *k;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	one_argument(argument, buf);

	if (!*buf) {
		mob_log(ch, "mfollow: bad syntax");
		return;
	}

	if (*buf == UID_CHAR) {
		if (!(leader = get_char(buf))) {
			mob_log(ch, "mfollow: victim (%s) does not exist", buf);
			return;
		}
	}
	else if (!(leader = get_char_vis(ch, buf, FIND_CHAR_ROOM))) {
		mob_log(ch,"mfollow: victim (%s) not found", buf);
		return;
	}

	if (ch->master == leader) /* already following */
		return;

	if (AFF_FLAGGED(ch, AFF_CHARM) && (ch->master))  /* can't override charm */
		return;

	/* stop following someone else first */
	if (ch->master) {
		if (ch->master->followers->follower == ch) {	/* Head of follower-list? */
			k = ch->master->followers;
			ch->master->followers = k->next;
			free(k);
		}
		else {			/* locate follower who is not head of list */
			for (k = ch->master->followers; k->next->follower != ch; k = k->next);

			j = k->next;
			k->next = j->next;
			free(j);
		}
		ch->master = NULL;
	}

	if (ch == leader) 
		return;

	if (circle_follow(ch, leader)) {
		mob_log(ch, "mfollow: Following in circles.");
		return;
	}

	ch->master = leader;

	CREATE(k, struct follow_type, 1);

	k->follower = ch;
	k->next = leader->followers;
	leader->followers = k;
}


ACMD(do_mscale) {
	void scale_item_to_level(obj_data *obj, int level);
	void scale_mob_to_level(char_data *mob, int level);

	char arg[MAX_INPUT_LENGTH], lvl_arg[MAX_INPUT_LENGTH];
	char_data *victim;
	obj_data *obj, *fresh, *proto;
	int level;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	two_arguments(argument, arg, lvl_arg);

	if (!*arg) {
		mob_log(ch, "mscale: No target provided");
		return;
	}
	
	if (*lvl_arg && !isdigit(*lvl_arg)) {
		mob_log(ch, "mscale: invalid level '%s'", lvl_arg);
		return;
	}
	else if (*lvl_arg) {
		level = atoi(lvl_arg);
	}
	else {
		level = IS_NPC(ch) ? GET_CURRENT_SCALE_LEVEL(ch) : 0;
	}
	
	if (level <= 0) {
		mob_log(ch, "mscale: no valid level to scale to");
		return;
	}

	if (*arg == UID_CHAR) {
		victim = get_char(arg);
	}
	else {
		victim = get_char_room_vis(ch, arg);
	}

	if (victim == NULL) {
		if (*arg == UID_CHAR) {
			obj = get_obj(arg);
		}
		else {
			obj = get_obj_vis(ch, arg);
		}

		if (obj) {
			if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
				scale_item_to_level(obj, level);
			}
			else if ((proto = obj_proto(GET_OBJ_VNUM(obj))) && OBJ_FLAGGED(proto, OBJ_SCALABLE)) {
				fresh = read_object(GET_OBJ_VNUM(obj), TRUE);
				scale_item_to_level(fresh, level);
				swap_obj_for_obj(obj, fresh);
				extract_obj(obj);
			}
			else {
				// attempt to scale anyway
				scale_item_to_level(obj, level);
			}
		}
		else {
			mob_log(ch, "mscale: bad argument");
		}

		return;
	}

	if (!IS_NPC(victim)) {
		mob_log(ch, "mscale: unable to scale a PC");
		return;
	}

	scale_mob_to_level(victim, level);
}
