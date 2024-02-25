/* ************************************************************************
*   File: dg_scripts.c                                    EmpireMUD 2.0b5 *
*  Usage: contains general functions for using scripts.                   *
*                                                                         *
*  DG Scripts code by egreen, 1996/09/24 03:48:42, revision 3.25          *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#include "dg_event.h"
#include "db.h"
#include "olc.h"
#include "skills.h"
#include "vnums.h"
#include "constants.h"

// external vars
extern unsigned long main_game_pulse;

// local functions
int eval_lhs_op_rhs(char *expr, char *result, void *go, struct script_data *sc, trig_data *trig, int type);
struct cmdlist_element *find_case(trig_data *trig, struct cmdlist_element *cl, void *go, struct script_data *sc, int type, char *cond);
struct cmdlist_element *find_done(struct cmdlist_element *cl);
void process_eval(void *go, struct script_data *sc, trig_data *trig, int type, char *cmd);
void var_subst(void *go, struct script_data *sc, trig_data *trig, int type, char *line, char *buf);


/**
* Counts the number of people in a room for %people.vnum%
*
* @room_vnum vnum The room to check for people.
* @return int The number of characters in that room.
*/
int trgvar_in_room(room_vnum vnum) {
	room_data *room = real_room(vnum);
	int i = 0;
	char_data *ch;

	if (!room) {
		script_log("people.vnum: room does not exist");
		return (-1);
	}
	
	DL_COUNT2(ROOM_PEOPLE(room), ch, i, next_in_room);
	return i;
}


/**
* Find an object in a list by name without regards to visibility.
*
* @param char *name The keyword (singular).
* @param obj_data *list A list of objects to search.
* @return obj_data* The matching object in the list, if any, or else NULL.
*/
obj_data *get_obj_in_list(char *name, obj_data *list) {
	obj_data *i;
	int id;

	if (*name == UID_CHAR) {
		id = atoi(name + 1);
		
		if (id > 0) {
			DL_FOREACH2(list, i, next_content) {
				if (id == i->script_id) {
					return i;
				}
			}
		}
	}
	else {
		DL_FOREACH2(list, i, next_content) {
			if (isname(name, i->name)) {
				return i;
			}
		}
	}

	return NULL;
}


/**
* Finds an equipped object on a character by name, without regard to
* visibility.
*
* @param char_data *ch The person whose equipment to check.
* @param char *name The argument to match (singular).
* @return obj_data* The matching object if found; NULL if not.
*/
obj_data *get_object_in_equip(char_data *ch, char *name) {
	int j, n = 0, number;
	obj_data *obj;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname; 
	int id;

	if (*name == UID_CHAR) {
		id = atoi(name + 1);
		
		if (id > 0) {
			for (j = 0; j < NUM_WEARS; j++) {
				if ((obj = GET_EQ(ch, j))) {
					if (id == obj->script_id) {
						return (obj);
					}
				}
			}
		}
	}
	else if (is_number(name)) {
		obj_vnum ovnum = atoi(name);
		for (j = 0; j < NUM_WEARS; j++)
			if ((obj = GET_EQ(ch, j)))
				if (GET_OBJ_VNUM(obj) == ovnum)
					return (obj);
	}
	else {
		snprintf(tmpname, sizeof(tmpname), "%s", name);
		if (!(number = get_number(&tmp)))
			return NULL;

		for (j = 0; (j < NUM_WEARS) && (n <= number); j++)
			if ((obj = GET_EQ(ch, j)))
				if (isname(tmp, obj->name))
					if (++n == number)
						return (obj);
	}

	return NULL;
}


/**
* Find a WEAR_ pos by various names.
*
* Handles 'held', 'light' and 'wield' positions - Welcor
* After idea from Byron Ellacott - bje@apnic.net
*
* @param char *arg The position as typed in the script.
* @return int A WEAR_ const to match, or NO_WEAR for no match.
*/
int find_eq_pos_script(char *arg) {
	int i;
	struct eq_pos_list {
		const char *pos;
		int where;
	} eq_pos[] = {
		// WEAR_x:
		{ "head", WEAR_HEAD },
		{ "ears", WEAR_EARS },
		{ "neck1", WEAR_NECK_1 },
		{ "neck2", WEAR_NECK_2 },
		{ "clothes", WEAR_CLOTHES },
		{ "armor", WEAR_ARMOR },
		{ "about", WEAR_ABOUT },
		{ "arms", WEAR_ARMS },
		{ "wrists", WEAR_WRISTS },
		{ "hands", WEAR_HANDS },
		{ "rfinger", WEAR_FINGER_R },
		{ "lfinger", WEAR_FINGER_L },
		{ "waist", WEAR_WAIST },
		{ "legs", WEAR_LEGS },
		{ "feet", WEAR_FEET },
		{ "pack", WEAR_PACK },
		{ "saddle", WEAR_SADDLE },
		{ "sheath1", WEAR_SHEATH_1 },
		{ "sheath2", WEAR_SHEATH_2 },
		{ "wield", WEAR_WIELD },
		{ "ranged", WEAR_RANGED },
		{ "hold", WEAR_HOLD },
		{ "held", WEAR_HOLD },
		{ "tool", WEAR_TOOL },
		{ "shared", WEAR_SHARE },
		{ "none", NO_WEAR }
	};

	if (is_number(arg) && (i = atoi(arg)) >= 0 && i < NUM_WEARS)
		return i;

	for (i = 0;eq_pos[i].where != NO_WEAR;i++) {
		if (!str_cmp(eq_pos[i].pos, arg))
			return eq_pos[i].where;
	}
	return NO_WEAR;
}


/************************************************************
* generic searches based only on name
************************************************************/

/* search the entire world for a char, and return a pointer */
char_data *get_char(char *name) {
	char_data *i;

	if (*name == UID_CHAR) {
		i = find_char(atoi(name + 1));

		if (i && valid_dg_target(i, DG_ALLOW_GODS))
			return i;
	}
	else {
		DL_FOREACH(character_list, i) {
			if (isname(name, i->player.name) && valid_dg_target(i, DG_ALLOW_GODS)) {
				return i;
			}
		}
	}

	return NULL;
}

/*
* Finds a char in the same room as the object with the name 'name'
*/
char_data *get_char_near_obj(obj_data *obj, char *name) {
	char_data *ch;

	if (*name == UID_CHAR) {
		ch = find_char(atoi(name + 1));

		if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
			return ch;
	}
	else {
		room_data *num;
		if ((num = obj_room(obj))) {
			DL_FOREACH2(ROOM_PEOPLE(num), ch, next_in_room) {
				if (isname(name, ch->player.name) && valid_dg_target(ch, DG_ALLOW_GODS)) {
					return ch;
				}
			}
		}
	}

	return NULL;
}


/**
* Finds a char present with a vehicle.
*
* @param vehicle_data *veh The vehicle to search near.
* @param char *name The name to search for.
* @return char_data* The found character, or NULL.
*/
char_data *get_char_near_vehicle(vehicle_data *veh, char *name) {
	char_data *ch;

	if (*name == UID_CHAR) {
		ch = find_char(atoi(name + 1));

		if (ch && valid_dg_target(ch, DG_ALLOW_GODS)) {
			return ch;
		}
	}
	else {
		// prefer people attached to the vehicle
		if (VEH_DRIVER(veh) && isname(name, GET_PC_NAME(VEH_DRIVER(veh))) && valid_dg_target(VEH_DRIVER(veh), DG_ALLOW_GODS)) {
			return VEH_DRIVER(veh);
		}
		if (VEH_SITTING_ON(veh) && isname(name, GET_PC_NAME(VEH_SITTING_ON(veh))) && valid_dg_target(VEH_SITTING_ON(veh), DG_ALLOW_GODS)) {
			return VEH_SITTING_ON(veh);
		}
		if (VEH_LED_BY(veh) && isname(name, GET_PC_NAME(VEH_LED_BY(veh))) && valid_dg_target(VEH_LED_BY(veh), DG_ALLOW_GODS)) {
			return VEH_LED_BY(veh);
		}
		
		if (IN_ROOM(veh)) {
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(veh)), ch, next_in_room) {
				if (isname(name, GET_PC_NAME(ch)) && valid_dg_target(ch, DG_ALLOW_GODS)) {
					return ch;
				}
			}
		}
	}

	return NULL;
}


/*
* returns a pointer to the first character in world by name name,
* or NULL if none found.  Starts searching in room room first
*/
char_data *get_char_in_room(room_data *room, char *name) {    
	char_data *ch;

	if (*name == UID_CHAR) {
		ch = find_char(atoi(name + 1));

		if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
			return ch;
	}
	else {
		DL_FOREACH2(ROOM_PEOPLE(room), ch, next_in_room) {
			if (isname(name, ch->player.name) && valid_dg_target(ch, DG_ALLOW_GODS)) {
				return ch;
			}
		}
	}

	return NULL;
}


/**
* Look up an empire by UID (DG Scripts) or name/number. This is currently a
* pass-through to get_empire_by_name() but since it's used in scripting, it
* may get more restrictions in the future.
*
* @param char *name The typed-in argument.
* @return empire_data* The found empire, or NULL.
*/
empire_data *get_empire(char *name) {
	if (*name == UID_CHAR) {
		return find_empire_by_uid(atoi(name + 1));
	}
	else {
		return get_empire_by_name(name);
	}
}


/* searches the room with the object for an object with name 'name'*/

obj_data *get_obj_near_obj(obj_data *obj, char *name) {
	obj_data *i = NULL;  
	char_data *ch;
	room_data *rm;
	int id;

	if (!str_cmp(name, "self") || !str_cmp(name, "me"))
		return obj;

	/* is it inside ? */
	if (obj->contains && (i = get_obj_in_list(name, obj->contains)))
		return i;

	/* or outside ? */
	if (obj->in_obj) {
		if (*name == UID_CHAR) {
			id = atoi(name + 1);

			if (id > 0 && id == obj->in_obj->script_id) {
				return obj->in_obj;
			}
		}
		else if (isname(name, obj->in_obj->name)) {
			return obj->in_obj;
		}
	}   
	/* or worn ?*/
	else if (obj->worn_by && (i = get_object_in_equip(obj->worn_by, name)))
		return i;
	/* or carried ? */
	else if (obj->carried_by && (i = get_obj_in_list(name, obj->carried_by->carrying)))
		return i;
	else if ((rm = obj_room(obj))) {
		/* check the floor */
		if ((i = get_obj_in_list(name, ROOM_CONTENTS(rm))))
			return i;

		/* check peoples' inventory */
		DL_FOREACH2(ROOM_PEOPLE(rm), ch, next_in_room) {
			if ((i = get_object_in_equip(ch, name))) {
				return i;
			}
		}
	}
	return NULL;
}


/**
* Finds a matching object near the vehicle.
*
* @param vehicle_data *veh The vehicle that's looking for an object.
* @param char *name The name to look for.
* @return obj_data* The object found, or NULL if none.
*/
obj_data *get_obj_near_vehicle(vehicle_data *veh, char *name) {
	obj_data *i = NULL;
	char_data *ch;

	if (*name == UID_CHAR) {
		return find_obj(atoi(name + 1));
	}
	if (VEH_CONTAINS(veh) && (i = get_obj_in_list(name, VEH_CONTAINS(veh)))) {
		return i;
	}
	if (IN_ROOM(veh)) {
		if ((i = get_obj_in_list(name, ROOM_CONTENTS(IN_ROOM(veh))))) {
			return i;
		}
		
		// check inventories (because get_obj_near_obj does)
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(veh)), ch, next_in_room) {
			if ((i = get_object_in_equip(ch, name))) {
				return i;
			}
		}
	}
	
	return NULL;
}


/* returns the object in the world with name name, or NULL if not found */
obj_data *get_obj(char *name)  {
	obj_data *obj;

	if (*name == UID_CHAR)
		return find_obj(atoi(name + 1));
	else {
		DL_FOREACH(object_list, obj) {
			if (isname(name, obj->name)) {
				return obj;
			}
		}
	}

	return NULL;
}


/**
* finds room by id or vnum.
*
* @param room_data *ref A reference locataion, for instance lookup (may be NULL).
* @param char *name The room argument (id, vnum, etc).
* @return room_data* The found room, or NULL if none.
*/
room_data *get_room(room_data *ref, char *name) {
	struct instance_data *inst;
	room_data *nr;

	if (*name == UID_CHAR)
		return find_room(atoi(name + 1));
	else if (*name == 'i' && isdigit(*(name+1)) && ref && (inst = find_instance_by_room(ref, FALSE, TRUE))) {
		// instance lookup
		nr = find_room_template_in_instance(inst, atoi(name+1));
		if (nr) {
			return nr;
		}
		else {
			return NULL;
		}
	}
	else if (!(nr = find_target_room(NULL, name)))
		return NULL;
	else
		return nr;
}


/**
* Attempts to find a room, if possible, for a script that's running.
*
* @param int type Script type (WLD_TRIGGER, etc.)
* @param void *go The thing running the script (ambiguous).
* @return room_data* The room the script is running in (may be NULL if undetectable).
*/
room_data *get_room_by_script(int type, void *go) {
	room_data *room = NULL;
	
	switch (type) {
		case WLD_TRIGGER:
		case RMT_TRIGGER:
		case BLD_TRIGGER:
		case ADV_TRIGGER: {
			room = (room_data*)go;
			break;
		}
		case OBJ_TRIGGER: {
			room = obj_room((obj_data*)go);
			break;
		}
		case MOB_TRIGGER: {
			room = IN_ROOM((char_data*)go);
			break;
		}
		case VEH_TRIGGER: {
			room = IN_ROOM((vehicle_data*)go);
			break;
		}
		case EMP_TRIGGER: {
			room = NULL;	// cannot detect this way
			break;
		}
	}
	
	return room;
}


/**
* Find a vehicle in the world by name/uid.
*
* @param char *name The name to look up.
* @return vehicle_data* The found vehicle, or NULL if none.
*/
vehicle_data *get_vehicle(char *name) {
	vehicle_data *veh;
	
	if (*name == UID_CHAR) {
		return find_vehicle(atoi(name + 1));
	}
	else {
		DL_FOREACH(vehicle_list, veh) {
			if (isname(name, VEH_KEYWORDS(veh))) {
				return veh;
			}
		}
	}
	
	return NULL;
}


/*
* returns a pointer to the first character in world by name name,
* or NULL if none found.  Starts searching with the person owing the object
*/
char_data *get_char_by_obj(obj_data *obj, char *name) {
	char_data *ch;

	if (*name == UID_CHAR) {
		ch = find_char(atoi(name + 1));

		if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
			return ch;
	}
	else {
		if (obj->carried_by && isname(name, obj->carried_by->player.name) && valid_dg_target(obj->carried_by, DG_ALLOW_GODS))
			return obj->carried_by;

		if (obj->worn_by && isname(name, obj->worn_by->player.name) && valid_dg_target(obj->worn_by, DG_ALLOW_GODS))
			return obj->worn_by;
		
		DL_FOREACH(character_list, ch) {
			if (isname(name, ch->player.name) && valid_dg_target(ch, DG_ALLOW_GODS)) {
				return ch;
			}
		}
	}

	return NULL;
}


/**
* Looks up a character using a vehicle as a reference.
*
* @param vehicle_data *veh The vehicle that's looking for a character.
* @param char *name The argument.
* @return char_data* The found character, or NULL.
*/
char_data *get_char_by_vehicle(vehicle_data *veh, char *name) {
	char_data *ch;

	if (*name == UID_CHAR) {
		ch = find_char(atoi(name + 1));

		if (ch && valid_dg_target(ch, DG_ALLOW_GODS)) {
			return ch;
		}
	}
	else {
		// prefer people attached to the vehicle
		if (VEH_DRIVER(veh) && isname(name, GET_PC_NAME(VEH_DRIVER(veh))) && valid_dg_target(VEH_DRIVER(veh), DG_ALLOW_GODS)) {
			return VEH_DRIVER(veh);
		}
		if (VEH_SITTING_ON(veh) && isname(name, GET_PC_NAME(VEH_SITTING_ON(veh))) && valid_dg_target(VEH_SITTING_ON(veh), DG_ALLOW_GODS)) {
			return VEH_SITTING_ON(veh);
		}
		if (VEH_LED_BY(veh) && isname(name, GET_PC_NAME(VEH_LED_BY(veh))) && valid_dg_target(VEH_LED_BY(veh), DG_ALLOW_GODS)) {
			return VEH_LED_BY(veh);
		}
		
		// try people in the room with the vehicle
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(veh)), ch, next_in_room) {
			if (isname(name, GET_PC_NAME(ch)) && valid_dg_target(ch, DG_ALLOW_GODS)) {
				return ch;
			}
		}
		
		// try whole world
		DL_FOREACH(character_list, ch) {
			if (isname(name, GET_PC_NAME(ch)) && valid_dg_target(ch, DG_ALLOW_GODS)) {
				return ch;
			}
		}
	}

	return NULL;
}


/*
* returns a pointer to the first character in world by name name,
* or NULL if none found.  Starts searching in room room first
*/
char_data *get_char_by_room(room_data *room, char *name) {
	char_data *ch;

	if (*name == UID_CHAR) {
		ch = find_char(atoi(name + 1));

		if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
			return ch;
	}
	else {
		DL_FOREACH2(ROOM_PEOPLE(room), ch, next_in_room) {
			if (isname(name, ch->player.name) && valid_dg_target(ch, DG_ALLOW_GODS)) {
				return ch;
			}
		}
		
		DL_FOREACH(character_list, ch) {
			if (isname(name, ch->player.name) && valid_dg_target(ch, DG_ALLOW_GODS)) {
				return ch;
			}
		}
	}

	return NULL;
}


/*
* returns the object in the world with name name, or NULL if not found
* search based on obj
*/  
obj_data *get_obj_by_obj(obj_data *obj, char *name) {
	obj_data *i = NULL;
	room_data *rm;

	if (*name == UID_CHAR) 
		return find_obj(atoi(name + 1));

	if (!str_cmp(name, "self") || !str_cmp(name, "me"))
		return obj;

	if (obj->contains && (i = get_obj_in_list(name, obj->contains)))
		return i;

	if (obj->in_obj && isname(name, obj->in_obj->name))
		return obj->in_obj;

	if (obj->worn_by && (i = get_object_in_equip(obj->worn_by, name)))
		return i;

	if (obj->carried_by && (i = get_obj_in_list(name, obj->carried_by->carrying)))
		return i;

	if ((rm = obj_room(obj)) && (i = get_obj_in_list(name, ROOM_CONTENTS(rm))))
		return i;

	return get_obj(name);
}


/**
* Finds a matching object in relation to the vehicle.
*
* @param vehicle_data *veh The vehicle that's looking for an object.
* @param char *name The name to look for.
* @return obj_data* The object found, or NULL if none.
*/
obj_data *get_obj_by_vehicle(vehicle_data *veh, char *name) {
	obj_data *i = NULL;

	if (*name == UID_CHAR) {
		return find_obj(atoi(name + 1));
	}
	if (VEH_CONTAINS(veh) && (i = get_obj_in_list(name, VEH_CONTAINS(veh)))) {
		return i;
	}
	if (IN_ROOM(veh) && (i = get_obj_in_list(name, ROOM_CONTENTS(IN_ROOM(veh))))) {
		return i;
	}
	
	return get_obj(name);
}


/* only searches the room */
obj_data *get_obj_in_room(room_data *room, char *name) {
	obj_data *obj;
	int id;

	if (*name == UID_CHAR) {
		id = atoi(name + 1);
		if (id > 0) {
			DL_FOREACH2(ROOM_CONTENTS(room), obj, next_content) {
				if (id == obj->script_id) {
					return obj;
				}
			}
		}
	}
	else {
		DL_FOREACH2(ROOM_CONTENTS(room), obj, next_content) {
			if (isname(name, obj->name)) {
				return obj;
			}
		}
	}           

	return NULL;
}

/* returns obj with name - searches room, then world */
obj_data *get_obj_by_room(room_data *room, char *name) {
	obj_data *obj;

	if (*name == UID_CHAR) {
		return find_obj(atoi(name+1));
	}
	
	DL_FOREACH2(ROOM_CONTENTS(room), obj, next_content) {
		if (isname(name, obj->name)) {
			return obj;
		}
	}
	
	DL_FOREACH(object_list, obj) {
		if (isname(name, obj->name)) {
			return obj;
		}
	}

	return NULL;
}


/**
* Finds a matching vehicle in relation to an object.
*
* @param obj_data *obj The object that's looking for a vehicle.
* @param char *name The name to look for.
* @return vehicle_data* The found vehicle, or NULL.
*/
vehicle_data *get_vehicle_by_obj(obj_data *obj, char *name) {
	vehicle_data *iter;
	room_data *room;
	
	if (*name == UID_CHAR) {
		return find_vehicle(atoi(name + 1));
	}
	
	if ((room = obj_room(obj))) {
		DL_FOREACH2(ROOM_VEHICLES(room), iter, next_in_room) {
			if (isname(name, VEH_KEYWORDS(iter))) {
				return iter;
			}
		}
	}
	
	return get_vehicle(name);
}



/**
* Finds a matching vehicle in relation to a room.
*
* @param room_data *room The room that's looking for a vehicle.
* @param char *name The name to look for.
* @return vehicle_data* The found vehicle or NULL.
*/
vehicle_data *get_vehicle_by_room(room_data *room, char *name) {
	vehicle_data *iter;
	
	if (*name == UID_CHAR) {
		return find_vehicle(atoi(name + 1));
	}

	DL_FOREACH2(ROOM_VEHICLES(room), iter, next_in_room) {
		if (isname(name, VEH_KEYWORDS(iter))) {
			return iter;
		}
	}
	
	return get_vehicle(name);
}


/**
* Finds a matching vehicle in relation to another vehicle.
*
* @param vehicle_data *veh The vehicle that's looking for an object.
* @param char *name The name to look for.
* @return vehicle_data* The found vehicle or NULL.
*/
vehicle_data *get_vehicle_by_vehicle(vehicle_data *veh, char *name) {
	vehicle_data *iter;
	
	if (*name == UID_CHAR) {
		return find_vehicle(atoi(name + 1));
	}
	if (!str_cmp(name, "self") || !str_cmp(name, "me")) {
		return veh;
	}
	if (IN_ROOM(veh)) {
		DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(veh)), iter, next_in_room) {
			if (isname(name, VEH_KEYWORDS(iter))) {
				return iter;
			}
		}
	}
	
	return get_vehicle(name);
}


/**
* Finds a vehicle in the same room as the object.
*
* @param obj_data *obj The object to search near.
* @param char *name The name argument.
*/
vehicle_data *get_vehicle_near_obj(obj_data *obj, char *name) {
	room_data *orm;

	if (*name == UID_CHAR) {
		return find_vehicle(atoi(name + 1));
	}
	else if (obj->in_vehicle && isname(name, VEH_KEYWORDS(obj->in_vehicle))) {
		return obj->in_vehicle;
	}
	else if ((orm = obj_room(obj))) {
		if (GET_ROOM_VEHICLE(orm) && isname(name, VEH_KEYWORDS(GET_ROOM_VEHICLE(orm)))) {
			return GET_ROOM_VEHICLE(orm);
		}
		return get_vehicle_room(orm, name, NULL);
	}
	else {
		return NULL;
	}
}


/**
* Finds a matching vehicle near the vehicle.
*
* @param vehicle_data *veh The vehicle that's looking for another.
* @param char *name The name to look for.
* @return vehicle_data* The vehicle found, or NULL if none.
*/
vehicle_data *get_vehicle_near_vehicle(vehicle_data *veh, char *name) {
	vehicle_data *find;
	
	if (*name == UID_CHAR) {
		return find_vehicle(atoi(name + 1));
	}
	if ((find = get_vehicle_room(IN_ROOM(veh), name, NULL))) {
		return find;
	}
	if (GET_ROOM_VEHICLE(IN_ROOM(veh)) && isname(name, VEH_KEYWORDS(GET_ROOM_VEHICLE(IN_ROOM(veh))))) {
		return GET_ROOM_VEHICLE(IN_ROOM(veh));
	}
	
	return NULL;
}


/* search through all the persons items, including containers
and 0 if it doesnt exist, and greater then 0 if it does!
Jamie Nelson (mordecai@timespace.co.nz)
MUD -- 4dimensions.org:6000

Now also searches by vnum -- Welcor
*/
int item_in_list(char *item, obj_data *list) {
	obj_data *i;
	int id;
	int count = 0;    

	if (*item == UID_CHAR) {
		id = atoi(item + 1);
		
		if (id > 0) {
			DL_FOREACH2(list, i, next_content) {
				if (id == i->script_id) {
					++count;
					break;
				}
				else if (GET_OBJ_TYPE(i) == ITEM_CONTAINER) {
					count += item_in_list(item, i->contains);
				}
			}
		}
	}
	else if (is_number(item)) { /* check for vnum */
		obj_vnum ovnum = atoi(item);
		
		DL_FOREACH2(list, i, next_content) {
			if (GET_OBJ_VNUM(i) == ovnum) {
				count++;
				break;
			}
			else if (GET_OBJ_TYPE(i) == ITEM_CONTAINER) {
				count += item_in_list(item, i->contains);
			}
		}
	}
	else {
		DL_FOREACH2(list, i, next_content) {
			if (isname(item, i->name)) {
				count++;
				break;
			}
			else if (GET_OBJ_TYPE(i) == ITEM_CONTAINER) {
				count += item_in_list(item, i->contains);
			}
		}
	}
	return count;
}

/* BOOLEAN return, just check if a player or mob
has an item of any sort, searched for by name
or id. 
searching equipment as well as inventory,
and containers.
Jamie Nelson (mordecai@timespace.co.nz)
MUD -- 4dimensions.org:6000
*/

int char_has_item(char *item, char_data *ch) {
	/* If this works, no more searching needed */
	if (get_object_in_equip(ch, item) != NULL) 
		return 1; 

	if (item_in_list(item, ch->carrying) == 0)
		return 0;
	else
		return 1;
}


/* checks every PULSE_SCRIPT for random triggers */
void script_trigger_check(void) {
	room_data *room, *in_room = NULL;
	trig_data *trig;
	char buf[MAX_STRING_LENGTH];
	struct script_data *sc;
	vehicle_data *veh;
	// empire_data *emp;
	char_data *mob;
	obj_data *obj;
	bool fail;
	
	// iterate over global list of random triggers
	DL_FOREACH_SAFE2(random_triggers, trig, stc_next_random_trig, next_in_random_triggers) {
		if (GET_TRIG_DEPTH(trig)) {
			continue;	// trigger already running
		}
		if (!(sc = trig->attached_to)) {
			continue;	// can't get script data
		}
		if (!(sc->attached_to)) {
			continue;	// script somehow not attached to anything
		}
		if (number(1, 100) > GET_TRIG_NARG(trig)) {
			continue;	// failed % check (this is cheap so we do it first)
		}
		
		// clean vars
		mob = NULL;
		obj = NULL;
		veh = NULL;
		room = in_room = NULL;
		fail = FALSE;
		
		// x_TRIGGER: basic setup by type
		switch (sc->attached_type) {
			case MOB_TRIGGER: {
				mob = (char_data *)sc->attached_to;
				in_room = IN_ROOM(mob);
				if (GET_POS(mob) < POS_SLEEPING || IS_DEAD(mob) || EXTRACTED(mob) || AFF_FLAGGED(mob, AFF_STUNNED | AFF_HARD_STUNNED) || IS_INJURED(mob, INJ_TIED) || GET_FED_ON_BY(mob)) {
					fail = TRUE;
				}
				if (AFF_FLAGGED(mob, AFF_CHARM) && !TRIGGER_CHECK(trig, MTRIG_CHARMED)) {
					fail = TRUE;	// can't do while charmed
				}
				break;
			}
			case OBJ_TRIGGER: {
				obj = (obj_data *)sc->attached_to;
				in_room = obj_room(obj);
				break;
			}
			case VEH_TRIGGER: {
				veh = (vehicle_data *)sc->attached_to;
				in_room = IN_ROOM(veh);
				break;
			}
			case EMP_TRIGGER: {
				// WARNING: empires really do not support running scripts
				// emp = (empire_data *)sc->attached_to;
				in_room = NULL;
				break;
			}
			default: {	// all world trigger types
				room = in_room = (room_data*)sc->attached_to;
				break;
			}
		}
		
		if (fail) {
			continue; // type-based fail
		}
		if (TRIG_IS_LOCAL(trig) && (!in_room || !any_players_in_room(in_room))) {
			continue;	// need player present (is local)
		}
		else if (!TRIG_IS_GLOBAL(trig) && (!in_room || distance_to_nearest_player(in_room) > PLAYER_SCRIPT_RADIUS)) {
			continue;	// need players nearby (not global)
		}
		
		// x_TRIGGER: run the triggers, by type
		switch (sc->attached_type) {
			case MOB_TRIGGER: {
				union script_driver_data_u sdd;
				sdd.c = mob;
				script_driver(&sdd, trig, MOB_TRIGGER, TRIG_NEW);
				break;
			}
			case OBJ_TRIGGER: {
				union script_driver_data_u sdd;
				sdd.o = obj;
				script_driver(&sdd, trig, OBJ_TRIGGER, TRIG_NEW);
				break;
			}
			case VEH_TRIGGER: {
				union script_driver_data_u sdd;
				sdd.v = veh;
				script_driver(&sdd, trig, VEH_TRIGGER, TRIG_NEW);
				break;
			}
			case EMP_TRIGGER: {
				// type not supported: do nothing
				break;
			}
			default: {	// all world trigger types
				union script_driver_data_u sdd;
				ADD_UID_VAR(buf, trig, room_script_id(room), "room", 0);
				sdd.r = room;
				script_driver(&sdd, trig, WLD_TRIGGER, TRIG_NEW);
				break;
			}
		}
	}
}


// frees memory when a waited trigger is canceled
EVENT_CANCEL_FUNC(cancel_wait_event) {
	struct wait_event_data *wait_event_obj = (struct wait_event_data *)event_obj;
	free(wait_event_obj);
}


EVENTFUNC(trig_wait_event) {
	struct wait_event_data *wait_event_obj = (struct wait_event_data *)event_obj;
	trig_data *trig;
	void *go;
	int type;
	union script_driver_data_u sdd;
	bool debug = FALSE;

	trig = wait_event_obj->trigger;
	go = wait_event_obj->go;
	sdd.c = (char_data*)wait_event_obj->go;
	type = wait_event_obj->type;

	free(wait_event_obj);  
	GET_TRIG_WAIT(trig) = NULL;

	if (debug) {
		int found = FALSE;
		if (type == MOB_TRIGGER) {
			char_data *tch;
			DL_FOREACH(character_list, tch) {
				if (tch == (char_data*)go) {
					found = TRUE;
					break;
				}
			}
		}
		else if (type == OBJ_TRIGGER) {
			obj_data *obj;
			DL_FOREACH(object_list, obj) {
				if (obj == (obj_data*)go) {
					found = TRUE;
					break;
				}
			}
		}
		else if (type == VEH_TRIGGER) {
			vehicle_data *veh;
			DL_FOREACH(vehicle_list, veh) {
				if (veh == (vehicle_data*)go) {
					found = TRUE;
					break;
				}
			}
		}
		else if (type == EMP_TRIGGER) {
			// this type is really not supported
		}
		else {	// WLD_TRIGGER, RMT_TRIGGER, ADV_TRIGGER, BLD_TRIGGER
			room_data *i, *next_i;
			HASH_ITER(hh, world_table, i, next_i) {
				if (i == (room_data*)go) {
					found = TRUE;
					break;
				}
			}
		}
		if (!found) {
			log("Trigger restarted on unknown entity. Vnum: %d", GET_TRIG_VNUM(trig));
			log("Type: %s trigger", trig_attach_types[type]);
			script_log("Trigger restart attempt on unknown entity.");
			return 0;
		}
	}

	script_driver(&sdd, trig, type, TRIG_RESTART);

	/* Do not reenqueue*/
	return 0;
}


void do_stat_trigger(char_data *ch, trig_data *trig) {
	struct cmdlist_element *cmd_list;
	char buf[MAX_STRING_LENGTH];

	if (!trig) {
		log("SYSERR: NULL trigger passed to do_stat_trigger.");
		return;
	}

	build_page_display(ch, "Name: '&y%s&0',  VNum: [&g%5d&0]", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
	
	// x_TRIGGER
	switch (trig->attach_type) {
		case OBJ_TRIGGER: {
			build_page_display(ch, "Trigger Intended Assignment: Objects");
			sprintbit(GET_TRIG_TYPE(trig), otrig_types, buf, TRUE);
			break;
		}
		case WLD_TRIGGER:
		case RMT_TRIGGER:
		case BLD_TRIGGER:
		case ADV_TRIGGER: {
			build_page_display(ch, "Trigger Intended Assignment: Rooms");
			sprintbit(GET_TRIG_TYPE(trig), wtrig_types, buf, TRUE);
			break;
		}
		case MOB_TRIGGER: {
			build_page_display(ch, "Trigger Intended Assignment: Mobiles");
			sprintbit(GET_TRIG_TYPE(trig), trig_types, buf, TRUE);
			break;
		}
		case VEH_TRIGGER: {
			build_page_display(ch, "Trigger Intended Assignment: Vehicles");
			sprintbit(GET_TRIG_TYPE(trig), vtrig_types, buf, TRUE);
			break;
		}
		case EMP_TRIGGER: {
			build_page_display(ch, "Trigger Intended Assignment: Empires");
			sprintbit(GET_TRIG_TYPE(trig), wtrig_types, buf, TRUE);
			break;
		}
	}

	build_page_display(ch, "Trigger Type: %s, Numeric Arg: %d, Arg list: %s", buf, GET_TRIG_NARG(trig), ((GET_TRIG_ARG(trig) && *GET_TRIG_ARG(trig)) ? GET_TRIG_ARG(trig) : "None"));

	build_page_display(ch, "Commands:"); 
	
	LL_FOREACH(trig->cmdlist, cmd_list) {
		if (cmd_list->cmd) {
			build_page_display_str(ch, show_color_codes(cmd_list->cmd));
		}
	}

	send_page_display(ch);
}


/* find the name of what the uid points to */
void find_uid_name(char *uid, char *name, size_t nlen) {
	vehicle_data *veh;
	char_data *ch;
	obj_data *obj;

	if ((ch = get_char(uid)))
		snprintf(name, nlen, "%s", PERS(ch, ch, TRUE));
	else if ((obj = get_obj(uid)))
		snprintf(name, nlen, "%s", obj->name);
	else if ((veh = get_vehicle(uid))) {
		snprintf(name, nlen, "%s", VEH_SHORT_DESC(veh));
	}
	else
		snprintf(name, nlen, "uid = %s, (not found)", uid + 1);
}


/**
* general function to display stats on script sc
*
* @param char_data *to The person who will see the script_data (goes to their page_display).
* @param struct script_data *sc The script to stat.
*/
void script_stat(char_data *to, struct script_data *sc) {
	struct trig_var_data *tv;
	trig_data *t;
	char name[MAX_INPUT_LENGTH];
	char namebuf[512];
	char buf1[MAX_STRING_LENGTH];
	
	if (!to || !sc) {
		return;
	}

	build_page_display(to, "Global Variables: %s", sc->global_vars ? "" : "None");
	build_page_display(to, "Global context: %ld", sc->context);

	for (tv = sc->global_vars; tv; tv = tv->next) {
		snprintf(namebuf, sizeof(namebuf), "%s:%ld", tv->name, tv->context);
		if (*(tv->value) == UID_CHAR) {
			find_uid_name(tv->value, name, sizeof(name));
			build_page_display(to, "    %15s:  %s", tv->context?namebuf:tv->name, name);
		}
		else {
			build_page_display(to, "    %15s:  %s", tv->context?namebuf:tv->name, tv->value);
		}
	}

	for (t = TRIGGERS(sc); t; t = t->next) {
		build_page_display(to, "\r\n  Trigger: &y%s&0, VNum: [&g%5d&0]", GET_TRIG_NAME(t), GET_TRIG_VNUM(t));

		if (t->attach_type==OBJ_TRIGGER) {
			build_page_display(to, "  Trigger Intended Assignment: Objects");
			sprintbit(GET_TRIG_TYPE(t), otrig_types, buf1, TRUE);
		}
		else if (t->attach_type == WLD_TRIGGER || t->attach_type == RMT_TRIGGER || t->attach_type == BLD_TRIGGER || t->attach_type == ADV_TRIGGER) {
			build_page_display(to, "  Trigger Intended Assignment: Rooms");
			sprintbit(GET_TRIG_TYPE(t), wtrig_types, buf1, TRUE);
		}
		else if (t->attach_type == VEH_TRIGGER) {
			build_page_display(to, "  Trigger Intended Assignment: Vehicles");
			sprintbit(GET_TRIG_TYPE(t), vtrig_types, buf1, TRUE);
		}
		else if (t->attach_type == EMP_TRIGGER) {
			build_page_display(to, "  Trigger Intended Assignment: Empires");
			sprintbit(GET_TRIG_TYPE(t), wtrig_types, buf1, TRUE);
		}
		else {
			build_page_display(to, "  Trigger Intended Assignment: Mobiles");
			sprintbit(GET_TRIG_TYPE(t), trig_types, buf1, TRUE);
		}

		build_page_display(to, "  Trigger Type: %s, Numeric Arg: %d, Arg list: %s",  buf1, GET_TRIG_NARG(t), ((GET_TRIG_ARG(t) && *GET_TRIG_ARG(t)) ? GET_TRIG_ARG(t) : "None"));

		if (GET_TRIG_WAIT(t)) {
			build_page_display(to, "    Wait: %ld, Current line: %s", dg_event_time(GET_TRIG_WAIT(t)), t->curr_state ? t->curr_state->cmd : "End of Script");
			build_page_display(to, "  Variables: %s", GET_TRIG_VARS(t) ? "" : "None");

			for (tv = GET_TRIG_VARS(t); tv; tv = tv->next) {
				if (*(tv->value) == UID_CHAR) {
					find_uid_name(tv->value, name, sizeof(name));
					build_page_display(to, "    %15s:  %s", tv->name, name);
				}
				else {
					build_page_display(to, "    %15s:  %s", tv->name, tv->value);
				}
			}
		}
	}  
}


void do_sstat_room(char_data *to, char_data *ch) {
	room_data *rm = IN_ROOM(ch);

	build_page_display(to, "Script information (id %d):", room_script_id(rm));
	if (!SCRIPT(rm)) {
		build_page_display(to, "  None.");
		return;
	}

	script_stat(to, SCRIPT(rm));
}


void do_sstat_object(char_data *to, obj_data *j) {
	int id;
	
	if (OBJ_IS_IN_WORLD(j)) {
		id = SCRIPT(j) ? obj_script_id(j) : j->script_id;
	}
	else {
		id = j->script_id;
	}
	
	build_page_display(to, "Script information (id %d):", id);
	if (!SCRIPT(j)) {
		build_page_display(to, "  None.");
		return;
	}

	script_stat(to, SCRIPT(j));
}


void do_sstat_character(char_data *to, char_data *k) {
	build_page_display(to, "Script information (id %d):", (SCRIPT(k) && IN_ROOM(k)) ? char_script_id(k) : k->script_id);
	if (!SCRIPT(k)) {
		build_page_display(to, "  None.");
		return;
	}

	script_stat(to, SCRIPT(k));
}


/*
* adds the trigger t to script sc in in location loc.  loc = -1 means
* add to the end, loc = 0 means add before all other triggers.
*/
void add_trigger(struct script_data *sc, trig_data *t, int loc) {
	trig_data *i;
	int n;

	for (n = loc, i = TRIGGERS(sc); i && i->next && (n != 0); n--, i = i->next);

	if (!loc || !i) {
		LL_PREPEND(TRIGGERS(sc), t);
	}
	else {
		LL_APPEND_ELEM(TRIGGERS(sc), i, t);
	}

	SCRIPT_TYPES(sc) |= GET_TRIG_TYPE(t);
	t->attached_to = sc;
	
	add_trigger_to_global_lists(t);
	
	if (sc->attached_type == WLD_TRIGGER && sc->attached_to) {
		check_reset_trigger_event((room_data*)sc->attached_to, TRUE);
	}
}


ACMD(do_tattach) {
	vehicle_data *veh;
	char_data *victim;
	obj_data *object;
	room_data *room;
	trig_data *trig;
	char targ_name[MAX_INPUT_LENGTH], trig_name[MAX_INPUT_LENGTH];
	char loc_name[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];
	int loc, tn, num_arg;
	trig_data *proto;

	argument = two_arguments(argument, arg, trig_name);
	two_arguments(argument, targ_name, loc_name);

	if (!*arg || !*targ_name || !*trig_name) {
		msg_to_char(ch, "Usage: tattach <type> <trigger> <attach to target> [location]\r\n");
		return;
	}

	num_arg = atoi(targ_name);
	tn = atoi(trig_name);
	loc = (*loc_name) ? atoi(loc_name) : -1;

	if (is_abbrev(arg, "mobile") || is_abbrev(arg, "mtr")) {
		victim = (*targ_name == UID_CHAR) ? get_char(targ_name) : get_char_vis(ch, targ_name, NULL, FIND_CHAR_WORLD);
		if (!victim) { /* search room for one with this vnum */
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), victim, next_in_room) {
				if (GET_MOB_VNUM(victim) == num_arg) {
					break;
				}
			}

			if (!victim) {
				msg_to_char(ch, "That mob does not exist.\r\n");
				return;
			}
		}

		if (!IS_NPC(victim))  {
			msg_to_char(ch, "Players can't have scripts.\r\n");
			return;
		}
		if (!player_can_olc_edit(ch, OLC_MOBILE, GET_MOB_VNUM(victim))) {
			msg_to_char(ch, "You can't edit that vnum.\r\n");
			return;
		}
		/* have a valid mob, now get trigger */
		proto = real_trigger(tn);
		if (!proto || !(trig = read_trigger(tn))) {
			msg_to_char(ch, "That trigger does not exist.\r\n");
			return;
		}

		if (!SCRIPT(victim)) {
			create_script_data(victim, MOB_TRIGGER);
		}
		add_trigger(SCRIPT(victim), trig, loc);
		reread_companion_trigs(victim);
		request_char_save_in_world(victim);

		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Trigger %d (%s) attached to %s [%d] by %s", tn, GET_TRIG_NAME(trig), GET_SHORT_DESC(victim), GET_MOB_VNUM(victim), GET_NAME(ch));
		msg_to_char(ch, "Trigger %d (%s) attached to %s [%d].\r\n", tn, GET_TRIG_NAME(trig), GET_SHORT_DESC(victim), GET_MOB_VNUM(victim));
	}
	else if (is_abbrev(arg, "object") || is_abbrev(arg, "otr")) {
		object = (*targ_name == UID_CHAR) ? get_obj(targ_name) : get_obj_vis(ch, targ_name, NULL);
		if (!object) { /* search room for one with this vnum */
			DL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), object, next_content) {
				if (GET_OBJ_VNUM(object) == num_arg) {
					break;
				}
			}

			if (!object) { /* search inventory for one with this vnum */
				DL_FOREACH2(ch->carrying, object, next_content) {
					if (GET_OBJ_VNUM(object) == num_arg) {
						break;
					}
				}

				if (!object) {
					msg_to_char(ch, "That object does not exist.\r\n");
					return;
				}
			}
		}
		
		if (!player_can_olc_edit(ch, OLC_OBJECT, GET_OBJ_VNUM(object))) {
			msg_to_char(ch, "You can't edit that vnum.\r\n");
			return;
		}

		/* have a valid obj, now get trigger */
		proto = real_trigger(tn);
		if (!proto || !(trig = read_trigger(tn))) {
			msg_to_char(ch, "That trigger does not exist.\r\n");
			return;
		}

		if (!SCRIPT(object)) {
			create_script_data(object, OBJ_TRIGGER);
		}
		add_trigger(SCRIPT(object), trig, loc);
		request_obj_save_in_world(object);

		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Trigger %d (%s) attached to %s [%d] by %s", tn, GET_TRIG_NAME(trig), (GET_OBJ_SHORT_DESC(object) ? GET_OBJ_SHORT_DESC(object) : object->name), GET_OBJ_VNUM(object), GET_NAME(ch));
		msg_to_char(ch, "Trigger %d (%s) attached to %s [%d].\r\n", tn, GET_TRIG_NAME(trig), (GET_OBJ_SHORT_DESC(object) ? GET_OBJ_SHORT_DESC(object) : object->name), GET_OBJ_VNUM(object));
	}
	else if (is_abbrev(arg, "vehicle") || is_abbrev(arg, "vtr")) {
		veh = (*targ_name == UID_CHAR) ? get_vehicle(targ_name) : get_vehicle_vis(ch, targ_name, NULL);
		if (!veh) {
			// search room for vehicle with matching vnum
			DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
				if (VEH_VNUM(veh) == num_arg) {
					break;
				}
			}
			
			// no?
			if (!veh) {
				msg_to_char(ch, "That vehicle does not exist.\r\n");
				return;
			}
		}
		
		if (!player_can_olc_edit(ch, OLC_VEHICLE, VEH_VNUM(veh))) {
			msg_to_char(ch, "You can't edit that vnum.\r\n");
			return;
		}

		/* have a valid obj, now get trigger */
		proto = real_trigger(tn);
		if (!proto || !(trig = read_trigger(tn))) {
			msg_to_char(ch, "That trigger does not exist.\r\n");
			return;
		}

		if (!SCRIPT(veh)) {
			create_script_data(veh, VEH_TRIGGER);
		}
		add_trigger(SCRIPT(veh), trig, loc);
		request_vehicle_save_in_world(veh);

		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Trigger %d (%s) attached to %s [%d] by %s", tn, GET_TRIG_NAME(trig), VEH_SHORT_DESC(veh), VEH_VNUM(veh), GET_NAME(ch));
		msg_to_char(ch, "Trigger %d (%s) attached to %s [%d].\r\n", tn, GET_TRIG_NAME(trig), VEH_SHORT_DESC(veh), VEH_VNUM(veh));
	}
	else if (is_abbrev(arg, "room") || is_abbrev(arg, "wtr")) {
		if (strchr(targ_name, '.'))
			room = IN_ROOM(ch);
		else if (isdigit(*targ_name)) 
			room = find_target_room(ch, targ_name);
		else if (*targ_name == UID_CHAR) {
			room = find_room(atoi(targ_name + 1));
		}
		else
			room = NULL;

		if (!room) {
			msg_to_char(ch, "You need to supply a room number or . for current room.\r\n");
			return;
		}
		
		if (!player_can_olc_edit(ch, OLC_MAP, GET_ROOM_VNUM(room))) {
			msg_to_char(ch, "You can't edit that vnum.\r\n");
			return;
		}

		/* have a valid room, now get trigger */
		proto = real_trigger(tn);
		if (!proto || !(trig = read_trigger(tn))) {
			msg_to_char(ch, "That trigger does not exist.\r\n");
			return;
		}

		if (!SCRIPT(room)) {
			create_script_data(room, WLD_TRIGGER);
		}
		add_trigger(SCRIPT(room), trig, loc);
		request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);

		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Trigger %d (%s) attached to room %d by %s", tn, GET_TRIG_NAME(trig), GET_ROOM_VNUM(room), GET_NAME(ch));
		msg_to_char(ch, "Trigger %d (%s) attached to room %d.\r\n", tn, GET_TRIG_NAME(trig), GET_ROOM_VNUM(room));
	}
	else
		msg_to_char(ch, "Please specify 'mob', 'obj', or 'room'.\r\n");
}

/*
* Thanks to James Long for his assistance in plugging the memory leak 
* that used to be here.   -- Welcor
*/
/* adds a variable with given name and value to trigger */
void add_var(struct trig_var_data **var_list, char *name, char *value, int id) {
	struct trig_var_data *vd;

	if (strchr(name, '.')) {
		log("add_var() : Attempt to add illegal var: %s", name);
		return;
	}

	for (vd = *var_list; vd && str_cmp(vd->name, name); vd = vd->next);

	if (vd && (!vd->context || vd->context==id)) {
		free(vd->value);
		CREATE(vd->value, char, strlen(value) + 1);
	}
	else {
		CREATE(vd, struct trig_var_data, 1);

		CREATE(vd->name, char, strlen(name) + 1);
		strcpy(vd->name, name);                            /* strcpy: ok*/

		CREATE(vd->value, char, strlen(value) + 1);
		LL_PREPEND(*var_list, vd);
		vd->context = id;
	}

	strcpy(vd->value, value);                            /* strcpy: ok*/
}


/**
* Attempts to remove a trigger from the script. Will likely also free the
* script data if it removes the last one. It only removes the first matching
* trigger it finds.
*
* @param struct script_data *sc The script data to remove a trigger from.
* @param char *name The keyword (goblin, 2.goblin) or vnum (12345) of the script to remove.
* @return bool TRUE if it removed a trigger, FALSE if not.
*/
bool remove_trigger(struct script_data *sc, char *name) {
	bool vnum_only = FALSE, any = FALSE;
	trig_data *trig, *next_trig;
	char copy[256], *str;
	int num;
	
	if (is_number(name)) {
		// remove by number
		str = name;
		num = atoi(str);
		vnum_only = TRUE;
	}
	else {
		// remove by name
		strcpy(copy, name);
		str = copy;
		num = get_number(&str);
	}
	
	if (sc) {
		LL_FOREACH_SAFE(TRIGGERS(sc), trig, next_trig) {
			if (vnum_only && GET_TRIG_VNUM(trig) != num) {
				continue;
			}
			else if (!vnum_only && (!isname(str, GET_TRIG_NAME(trig)) || --num > 0)) {
				continue;
			}
			
			// this is the one to detach
			LL_DELETE(TRIGGERS(sc), trig);
			extract_trigger(trig);			
			update_script_types(sc);
			any = TRUE;
		}
	}
	
	return any;
}

ACMD(do_tdetach) {
	vehicle_data *veh = NULL;
	char_data *victim = NULL;
	obj_data *object = NULL;
	room_data *room;
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	char *trigger = 0;   
	int num_arg;

	argument = two_arguments(argument, arg1, arg2);
	one_argument(argument, arg3);

	if (!*arg1 || !*arg2) {
		msg_to_char(ch, "Usage:  tdetach [mob|obj|vehicle] <target name> <trigger name/vnum | all>\r\n");
		msg_to_char(ch, "        tdetach room <target vnum | .> <trigger name/vnum | all>\r\n");
		return;
	}

	/* vnum of mob/obj, if given */
	num_arg = atoi(arg2);
	
	if (!str_cmp(arg1, "room") || !str_cmp(arg1, "wtr")) {
		room = IN_ROOM(ch);
		if (!SCRIPT(room))
			msg_to_char(ch, "This room does not have any triggers.\r\n");
		else if (!player_can_olc_edit(ch, OLC_MAP, GET_ROOM_VNUM(IN_ROOM(ch)))) {
			msg_to_char(ch, "You can't edit this vnum.\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			remove_all_triggers(room, WLD_TRIGGER);
			if (!IS_NPC(ch)) {
				syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "All triggers removed from room %d by %s", GET_ROOM_VNUM(IN_ROOM(ch)), GET_NAME(ch));
			}
			msg_to_char(ch, "All triggers removed from room.\r\n");
		}
		else if (remove_trigger(SCRIPT(room), arg2)) {
			if (!IS_NPC(ch)) {
				syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "Trigger %s removed from room %d by %s", arg2, GET_ROOM_VNUM(IN_ROOM(ch)), GET_NAME(ch));
			}
			msg_to_char(ch, "Trigger removed.\r\n");
			check_extract_script(room, WLD_TRIGGER);
		}
		else {
			msg_to_char(ch, "That trigger was not found.\r\n");
		}
		
		// room always done now
		return;
	}
	else if (is_abbrev(arg1, "mobile") || !str_cmp(arg1, "mtr")) {
		victim = (*arg2 == UID_CHAR) ? get_char(arg2) : get_char_vis(ch, arg2, NULL, FIND_CHAR_WORLD);
		if (!victim) { /* search room for one with this vnum */
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), victim, next_in_room) {
				if (GET_MOB_VNUM(victim) == num_arg) {
					break;
				}
			}

			if (!victim) {
				msg_to_char(ch, "No such mobile around.\r\n");
				return;
			}
		}

		if (!player_can_olc_edit(ch, OLC_MOBILE, GET_MOB_VNUM(victim))) {
			msg_to_char(ch, "You can't edit that vnum.\r\n");
			return;
		}

		if (!*arg3)
			msg_to_char(ch, "You must specify a trigger to remove.\r\n");
		else
			trigger = arg3;
	}
	else if (is_abbrev(arg1, "object") || !str_cmp(arg1, "otr")) {
		object = (*arg2 == UID_CHAR) ? get_obj(arg2) : get_obj_vis(ch, arg2, NULL);
		if (!object) { /* search room for one with this vnum */
			DL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), object, next_content) {
				if (GET_OBJ_VNUM(object) == num_arg) {
					break;
				}
			}

			if (!object) { /* search inventory for one with this vnum */
				DL_FOREACH2(ch->carrying, object, next_content) {
					if (GET_OBJ_VNUM(object) == num_arg) {
						break;
					}
				}

				if (!object) { /* give up */
					msg_to_char(ch, "No such object around.\r\n");
					return;
				}
			}
		}

		if (!player_can_olc_edit(ch, OLC_OBJECT, GET_OBJ_VNUM(object))) {
			msg_to_char(ch, "You can't edit that vnum.\r\n");
			return;
		}

		if (!*arg3)
			msg_to_char(ch, "You must specify a trigger to remove.\r\n");
		else
			trigger = arg3;
	}
	else if (is_abbrev(arg1, "vehicle") || !str_cmp(arg1, "vtr")) {
		veh = (*arg2 == UID_CHAR) ? get_vehicle(arg2) : get_vehicle_vis(ch, arg2, NULL);
		if (!veh) {
			// search room for vehicle with matching vnum
			DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
				if (VEH_VNUM(veh) == num_arg) {
					break;
				}
			}
			
			// no?
			if (!veh) {
				msg_to_char(ch, "That vehicle does not exist.\r\n");
				return;
			}
		}
		
		if (!player_can_olc_edit(ch, OLC_VEHICLE, VEH_VNUM(veh))) {
			msg_to_char(ch, "You can't edit that vnum.\r\n");
			return;
		}

		if (!*arg3) {
			msg_to_char(ch, "You must specify a trigger to remove.\r\n");
		}
		else {
			trigger = arg3;
		}
	}
	else {
		if (*arg1 == UID_CHAR && ((victim = get_char(arg1)) || (object = get_obj(arg1)))) {
		}
		else if ((veh = get_vehicle_in_room_vis(ch, arg1, NULL))) {
		}
		else if ((object = get_obj_in_equip_vis(ch, arg1, NULL, ch->equipment, NULL))) {
			/* Thanks to Carlos Myers for fixing the line above */
		}
		else if ((object = get_obj_in_list_vis(ch, arg1, NULL, ch->carrying))) {
		}
		else if ((victim = get_char_room_vis(ch, arg1, NULL))) {
		}
		else if ((object = get_obj_in_list_vis(ch, arg1, NULL, ROOM_CONTENTS(IN_ROOM(ch))))) {
		}
		else if ((victim = get_char_vis(ch, arg1, NULL, FIND_CHAR_WORLD))) {
		}
		else if ((veh = get_vehicle_vis(ch, arg1, NULL))) {
		}
		else if ((object = get_obj_vis(ch, arg1, NULL))) {
		}
		else {
			msg_to_char(ch, "Nothing around by that name.\r\n");
		}

		if ((victim && !player_can_olc_edit(ch, OLC_MOBILE, GET_MOB_VNUM(victim))) || (object && !player_can_olc_edit(ch, OLC_OBJECT, GET_OBJ_VNUM(object))) || (veh && !player_can_olc_edit(ch, OLC_VEHICLE, VEH_VNUM(veh)))) {
			msg_to_char(ch, "You can't edit that vnum.\r\n");
			return;
		}

		trigger = arg2;
	}
	
	// do the actual detaching
	if (victim) {
		if (!IS_NPC(victim))
			msg_to_char(ch, "Players don't have triggers.\r\n");
		else if (!SCRIPT(victim))
			msg_to_char(ch, "That mob doesn't have any triggers.\r\n");
		else if (trigger && !str_cmp(trigger, "all")) {
			remove_all_triggers(victim, MOB_TRIGGER);
			if (!IS_NPC(ch)) {
				syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "All triggers removed from mob %s by %s", GET_SHORT_DESC(victim), GET_NAME(ch));
			}
			msg_to_char(ch, "All triggers removed from %s.\r\n", GET_SHORT_DESC(victim));
		}
		else if (trigger && remove_trigger(SCRIPT(victim), trigger)) {
			if (!IS_NPC(ch)) {
				syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "Trigger %s removed from mob %s by %s", trigger, GET_SHORT_DESC(victim), GET_NAME(ch));
			}
			msg_to_char(ch, "Trigger removed.\r\n");
			check_extract_script(victim, MOB_TRIGGER);
			reread_companion_trigs(victim);
		}
		else
			msg_to_char(ch, "That trigger was not found.\r\n");
	}
	else if (object) {
		if (!SCRIPT(object))
			msg_to_char(ch, "That object doesn't have any triggers.\r\n");
		else if (trigger && !str_cmp(trigger, "all")) {
			remove_all_triggers(object, OBJ_TRIGGER);
			if (!IS_NPC(ch)) {
				syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "All triggers removed from obj %s by %s", GET_OBJ_SHORT_DESC(object), GET_NAME(ch));
			}
			msg_to_char(ch, "All triggers removed from %s.\r\n", GET_OBJ_SHORT_DESC(object) ? GET_OBJ_SHORT_DESC(object) : object->name);
		}
		else if (remove_trigger(SCRIPT(object), trigger)) {
			if (!IS_NPC(ch)) {
				syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "Trigger %s removed from obj %s by %s", trigger, GET_OBJ_SHORT_DESC(object), GET_NAME(ch));
			}
			msg_to_char(ch, "Trigger removed.\r\n");
			check_extract_script(object, OBJ_TRIGGER);
		}
		else
			msg_to_char(ch, "That trigger was not found.\r\n");
	}
	else if (veh) {
		if (!SCRIPT(veh)) {
			msg_to_char(ch, "That vehicle has no triggers.\r\n");
		}
		else if (trigger && !str_cmp(trigger, "all")) {
			remove_all_triggers(veh, VEH_TRIGGER);
			if (!IS_NPC(ch)) {
				syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "All triggers removed from vehicle %s by %s", VEH_SHORT_DESC(veh), GET_NAME(ch));
			}
			msg_to_char(ch, "All triggers removed from %s.\r\n", VEH_SHORT_DESC(veh));
		}
		else if (remove_trigger(SCRIPT(veh), trigger)) {
			if (!IS_NPC(ch)) {
				syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "Trigger %s removed from vehicle %s by %s", trigger, VEH_SHORT_DESC(veh), GET_NAME(ch));
			}
			msg_to_char(ch, "Trigger removed.\r\n");
			check_extract_script(veh, VEH_TRIGGER);
		}
		else {
			msg_to_char(ch, "That trigger was not found.\r\n");
		}
	}
}


/* frees memory associated with var */
void free_var_el(struct trig_var_data *var) {
	free(var->name);
	free(var->value);
	free(var);
}


/*
* remove var name from var_list
*
* @param struct trig_var_data **var_list Pointer to the list of vars.
* @param char *name The name of the var to remove.
* @param int context The context to remove (will also remove context=0 no matter what)
* @return int 1 if found, else 0
*/
int remove_var(struct trig_var_data **var_list, char *name, int context) {
	struct trig_var_data *iter, *next_iter;
	bool any = FALSE;
	
	LL_FOREACH_SAFE(*var_list, iter, next_iter) {
		if (!str_cmp(name, iter->name) && (iter->context == context || iter->context == 0)) {
			LL_DELETE(*var_list, iter);
			free_var_el(iter);
			any = TRUE;
		}
	}
	
	return (any ? 1 : 0);
}


/*  
*  Logs any errors caused by scripts to the system log.
*  Will eventually allow on-line view of script errors.
*/
void script_vlog(const char *format, va_list args) {
	char temp[MAX_STRING_LENGTH], output[MAX_STRING_LENGTH];
	descriptor_data *i;

	snprintf(temp, sizeof(temp), "SCRIPT ERR: %s", format);
	vsnprintf(output, sizeof(output), temp, args);
	log("%s", output);

	for (i = descriptor_list; i; i = i->next) {
		if (STATE(i) != CON_PLAYING || IS_NPC(i->character)) /* switch */
			continue;
		if (GET_ACCESS_LEVEL(i->character) < LVL_BUILDER)
			continue;
		if (!IS_SET(SYSLOG_FLAGS(REAL_CHAR(i->character)), SYS_SCRIPT)) {
			continue;
		}

		msg_to_char(i->character, "&g[ %s ]\r\n&0", output);
	}
}


void script_log(const char *format, ...) {
	va_list args;

	va_start(args, format);
	script_vlog(format, args);
	va_end(args);
}


/**
* Does a script log by ambiguous type.
*
* @param int go_type Any _TRIGGER type.
* @param void *go The thing the trigger is attached to.
* @param const char *format... The sprintf format.
*/
void script_log_by_type(int go_type, void *go, const char *format, ...) {
	char output[MAX_STRING_LENGTH], name[MAX_STRING_LENGTH], type[MAX_STRING_LENGTH];
	any_vnum vnum;
	va_list args;
	
	// x_TRIGGER
	switch (go_type) {
		case MOB_TRIGGER: {
			strcpy(type, "Mob");
			strcpy(name, GET_SHORT_DESC((char_data*)go));
			vnum = GET_MOB_VNUM((char_data*)go);
			break;
		}
		case OBJ_TRIGGER: {
			strcpy(type, "Obj");
			strcpy(name, GET_OBJ_SHORT_DESC((obj_data*)go));
			vnum = GET_OBJ_VNUM((obj_data*)go);
			break;
		}
		case WLD_TRIGGER:
		case RMT_TRIGGER:
		case ADV_TRIGGER:
		case BLD_TRIGGER: {
			strcpy(type, "Wld");
			strcpy(name, "Room");
			vnum = GET_ROOM_VNUM((room_data*)go);
			break;
		}
		case VEH_TRIGGER: {
			strcpy(type, "Veh");
			strcpy(name, VEH_SHORT_DESC((vehicle_data*)go));
			vnum = VEH_VNUM((vehicle_data*)go);
			break;
		}
		case EMP_TRIGGER: {
			strcpy(type, "Empire");
			strcpy(name, EMPIRE_NAME((empire_data*)go));
			vnum = EMPIRE_VNUM((empire_data*)go);
			break;
		}
		default: {
			strcpy(type, "???");
			strcpy(name, "???");
			vnum = NOTHING;
			break;
		}
	}

	snprintf(output, sizeof(output), "%s (%s, VNum %d):: %s", type, name, vnum, format);

	va_start(args, format);
	script_vlog(output, args);
	va_end(args);
}


int text_processed(char *field, char *subfield, struct trig_var_data *vd, char *str, size_t slen, void *go, struct script_data *sc, trig_data *trig, int type) {
	char *p, *p2;
	char tmpvar[MAX_STRING_LENGTH];

	if (!str_cmp(field, "strlen")) {                     /* strlen    */
		snprintf(str, slen, "%d", (int)strlen(vd->value));
		return TRUE;
	}
	else if (!str_cmp(field, "trim")) {                /* trim      */
		/* trim whitespace from ends */
		snprintf(tmpvar, sizeof(tmpvar)-1 , "%s", vd->value); /* -1 to use later*/
		p = tmpvar;                       
		p2 = tmpvar + strlen(tmpvar) - 1; 
		while (*p && isspace(*p))
			p++;
		while ((p<=p2) && isspace(*p2))
			p2--; 
		if (p > p2) { /* nothing left */
			*str = '\0';
			return TRUE;
		}
		*(++p2) = '\0';                                         /* +1 ok (see above) */
		snprintf(str, slen, "%s", p);
		return TRUE;
	}
	else if (!str_cmp(field, "ana")) {                 /* ana or an/a       */
		snprintf(str, slen, "%s", AN(vd->value));
		return TRUE;
	}
	else if (!str_cmp(field, "cap")) {                 // capitalize first letter
		snprintf(str, slen, "%s", vd->value);
		for (p = str; *p; ++p) {
			if (isspace(*p)) {
				continue;
			}
			else if (*p == COLOUR_CHAR && *(p + 1)) {
				++p;	// skip one for & (color char)
			}
			else if (isdigit(*p) || isalpha(*p)) {
				// found an alpha/numeric char? cap it and break
				CAP(p);
				break;
			}
		}
		return TRUE;
	}
	else if (!str_cmp(field, "char")) {
		int pos = atoi(NULLSAFE(subfield));
		if (subfield && strlen(vd->value) > pos) {
			snprintf(str, slen, "%c", vd->value[pos]);
		}
		else {
			snprintf(str, slen, "0");
		}
	}
	else if (!str_cmp(field, "contains")) {            /* contains  */
		if (str_str(vd->value, subfield))
			snprintf(str, slen, "1");
		else 
			snprintf(str, slen, "0");
		return TRUE;
	}
	else if (!str_cmp(field, "car")) {                 /* car       */
		char *car = vd->value;
		char *strptr = str;
		while (*car && !isspace(*car) && (strptr - str) < (slen-1)) {
			*strptr++ = *car++;
		}
		*strptr = '\0';
		return TRUE;
	}
	else if (!str_cmp(field, "cdr")) {                 /* cdr       */
		char *cdr = vd->value;
		while (*cdr && !isspace(*cdr))
			cdr++; /* skip 1st field */
		while (*cdr && isspace(*cdr))
			cdr++;  /* skip to next */

		snprintf(str, slen, "%s", cdr);
		return TRUE;
	}
	else if (!str_cmp(field, "empty")) {
		if (!str_cmp(vd->value, "") || !str_cmp(vd->value, "0")) {
			snprintf(str, slen, "1");
		}
		else {
			snprintf(str, slen, "0");
		}
		return TRUE;
	}
	else if (!str_cmp(field, "index_of")) {
		char *find;
		if (subfield && *subfield && (find = strchr(vd->value, *subfield))) {
			snprintf(str, slen, "%d", (int)(find - vd->value));
		}
		else {
			snprintf(str, slen, "-1");
		}
	}
	else if (!str_cmp(field, "mudcommand")) {
		/* find the mud command returned from this text */
		int length, cmd;
		for (length = strlen(vd->value), cmd = 0; *cmd_info[cmd].command != '\n'; cmd++)
			if (!strn_cmp(cmd_info[cmd].command, vd->value, length))
				break;

		if (*cmd_info[cmd].command == '\n')
			strcpy(str,"");
		else
			snprintf(str, slen, "%s", cmd_info[cmd].command);
		return TRUE;
	}
	else if (!str_cmp(field, "process")) {
		// processes substitutions
		char temp[MAX_INPUT_LENGTH];
		var_subst(go, sc, trig, type, vd->value, temp);
		snprintf(str, slen, "%s", temp);
		return TRUE;
	}
	
	return FALSE;
}


/**
* Process script variable for a %room.north(...)%, etc.
*
* @param room_data *room The room variable's room.
* @param char *subfield Contents of the parens in %room.north(subfield)%
* @param char *str String to write output to.
* @param size_t slen Max length of the str.
*/
void direction_vars(room_data *room, int dir, char *subfield, char *str, size_t slen) {
	struct room_direction_data *ex;
	
	// sanity check
	if (!room || dir == NO_DIR || !str) {
		return;
	}

	if ((ex = find_exit(room, dir)) && (!subfield || str_cmp(subfield, "map"))) {	// normal exit
		if (subfield && *subfield) {
			if (!str_cmp(subfield, "vnum")) {
				snprintf(str, slen, "%d", ex->to_room);
			}
			else if (!str_cmp(subfield, "bits")) {
				sprintbit(ex->exit_info, exit_bits, str, TRUE);
			}
			else if (!str_cmp(subfield, "room")) {
				if (ex->to_room != NOWHERE) {
					snprintf(str, slen, "%c%d", UID_CHAR, ex->to_room + ROOM_ID_BASE);
				}
				else {
					*str = '\0';
				}
			}
		}
		else { /* no subfield - default to bits */
			sprintbit(ex->exit_info ,exit_bits, str, TRUE);
		}
	}
	else if (dir < NUM_2D_DIRS && (!ROOM_IS_CLOSED(room) || !str_cmp(NULLSAFE(subfield), "map"))) {	// map dirs
		room_data *to_room = SHIFT_DIR(room, dir);
		if (to_room && subfield && *subfield) {
			if (!str_cmp(subfield, "vnum")) {
				snprintf(str, slen, "%d", GET_ROOM_VNUM(to_room));
			}
			else if (!str_cmp(subfield, "room") || !str_cmp(subfield, "map")) {
				snprintf(str, slen, "%c%d", UID_CHAR, GET_ROOM_VNUM(to_room) + ROOM_ID_BASE);
			}
		}
		else {	// default to empty
			*str = '\0';
		}
	}
	else {
		*str = '\0';
	}
}					


/* sets str to be the value of var.field */
void find_replacement(void *go, struct script_data *sc, trig_data *trig, int type, char *var, char *field, char *subfield, char *str, size_t slen) {
	char buf[MAX_STRING_LENGTH];
	struct trig_var_data *vd=NULL;
	char_data *ch = NULL, *c = NULL, *rndm;
	vehicle_data *veh = NULL, *v = NULL;
	obj_data *obj = NULL, *o = NULL;
	room_data *room, *r = NULL;
	empire_data *e = NULL;
	char *name;
	int num, count;

	// x_TRIGGER: mob, obj, world, rmt, adv, vehicle, bld, emp
	const char cmd_prefix[] = { 'm', 'o', 'w', 'w', 'w', 'v', 'w', 'e' };
	
	// commands that work with '%command%' syntax (will be replaced with 'mcommand', 'ocommand', etc)
	const char *script_commands[] = {
		"adventurecomplete",
		"aggro",
		"aoe",
		"asound",
		"at",
		"build",
		"buildingecho",
		"damage",
		"door",
		"dot",
		"echo",
		"echoaround",
		"echoneither",
		"force",
		"heal",
		"load",
		"mod",
		"morph",
		"own",
		"purge",
		"quest",
		"regionecho",
		"restore",
		"scale",
		"send",
		"siege",
		"slay",
		"subecho",
		"teleport",
		"terracrop",
		"terraform",
		"transform",
		"vehicleecho",
		"\n"	// must be last
	};
	
	/* X.global() will have a NULL trig */
	if (trig) {
		for (vd = GET_TRIG_VARS(trig); vd; vd = vd->next) {
			if (!str_cmp(vd->name, var)) {
				break;
			}
		}
	}

	/* some evil waitstates could crash the mud if sent here with sc==NULL*/
	if (!vd && sc) {
		for (vd = sc->global_vars; vd; vd = vd->next) {
			if (!str_cmp(vd->name, var) && (vd->context==0 || vd->context==sc->context)) {
				break;
			}
		}
	}

	if (!field || !*field) {
		if (vd) {
			snprintf(str, slen, "%s", vd->value);
		}
		else {
			if (!str_cmp(var, "self"))
				switch (type) {
					case MOB_TRIGGER:
						snprintf(str, slen, "%c%d", UID_CHAR, char_script_id((char_data*) go));
						break;
					case OBJ_TRIGGER:
						snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id((obj_data*) go));
						break;
					case WLD_TRIGGER:
					case RMT_TRIGGER:
					case BLD_TRIGGER:
					case ADV_TRIGGER:
						snprintf(str, slen, "%c%d", UID_CHAR, GET_ROOM_VNUM((room_data*)go) + ROOM_ID_BASE);
						break;
					case VEH_TRIGGER: {
						snprintf(str, slen, "%c%d", UID_CHAR, veh_script_id((vehicle_data*) go));
						break;
					}
					case EMP_TRIGGER: {
						snprintf(str, slen, "%c%d", UID_CHAR, EMPIRE_VNUM((empire_data*) go) + EMPIRE_ID_BASE);
						break;
					}
				}
			//        snprintf(str, slen, "self");
			else if ((num = search_block(var, script_commands, TRUE)) != NOTHING) {
				snprintf(str, slen, "%c%s", cmd_prefix[type], script_commands[num]);
			}
			else if (!str_cmp(var, "dailycycle")) {
				snprintf(str, slen, "%ld", DAILY_CYCLE_DAY);
				return;
			}
			else if (!str_cmp(var, "event")) {
				// %event% with no field
				script_log("Trigger: %s, VNum %d, %%event%% called with no field", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
				strcpy(str, "0");
			}
			else if (!str_cmp(var, "instance")) {
				// %instance% with no field
				struct instance_data *inst = get_instance_for_script(type, go);
				snprintf(str, slen, "%d", inst ? INST_ID(inst) : 0);
			}
			else if (!str_cmp(var, "timestamp")) {
				snprintf(str, slen, "%ld", time(0));
				return;
			}
			else if (!str_cmp(var, "startloc")) {
				room_data *sloc = find_starting_location(NULL);
				snprintf(str, slen, "%c%d", UID_CHAR, GET_ROOM_VNUM(sloc) + ROOM_ID_BASE);
				return;
			}
			else if (!str_cmp(var, "weather")) {
				snprintf(str, slen, "%s", weather_types[weather_info.sky]);
				return;
			}
			else
				*str = '\0';
		}

		return;
	}
	else {
		if (vd) {
			name = vd->value;

			switch (type) {
				case MOB_TRIGGER:
					ch = (char_data*) go;

					if (*name != UID_CHAR && (o = get_object_in_equip(ch, name))) {
						// just setting
					}
					else if (*name != UID_CHAR && (o = get_obj_in_list(name, ch->carrying))) {
						// just setting
					}
					else if (*name != UID_CHAR && (c = get_char_room(name, IN_ROOM(ch)))) {
						// just setting
					}
					else if (*name != UID_CHAR && (v = get_vehicle_room(IN_ROOM(ch), name, NULL))) {
						// just setting
					}
					else if (*name != UID_CHAR && GET_ROOM_VEHICLE(IN_ROOM(ch)) && isname(name, VEH_KEYWORDS(GET_ROOM_VEHICLE(IN_ROOM(ch))))) {
						v = GET_ROOM_VEHICLE(IN_ROOM(ch));
					}
					else if (*name != UID_CHAR && (o = get_obj_in_list(name, ROOM_CONTENTS(IN_ROOM(ch))))) {
						// just setting
					}
					else if ((c = get_char(name))) {
						// just setting
					}
					else if ((v = get_vehicle(name))) {
						// just setting
					}
					else if ((o = get_obj(name))) {
						// just setting
					}
					else if ((r = get_room(IN_ROOM(ch), name))) {
						// just setting
					}
					else if ((e = get_empire(name))) {
						// just setting
					}

					break;
				case OBJ_TRIGGER:
					obj = (obj_data*) go;

					if ((c = get_char_by_obj(obj, name))) {
						// just setting
					}
					else if ((v = get_vehicle_by_obj(obj, name))) {
						// just setting
					}
					else if ((o = get_obj_by_obj(obj, name))) {
						// just setting
					}
					else if ((r = get_room(obj_room(obj), name))) {
						// just setting
					}
					else if ((e = get_empire(name))) {
						// just setting
					}
					else {
						o = obj;
					}

					break;
				case WLD_TRIGGER:
				case RMT_TRIGGER:
				case BLD_TRIGGER:
				case ADV_TRIGGER:
					room = (room_data*) go;

					if ((c = get_char_by_room(room, name))) {
						// just setting
					}
					else if ((v = get_vehicle_by_room(room, name))) {
						// just setting
					}
					else if ((o = get_obj_by_room(room, name))) {
						// just setting
					}
					else if ((r = get_room(room, name))) {
						// just setting
					}
					else if ((e = get_empire(name))) {
						// just setting
					}

					break;
				case VEH_TRIGGER: {
					veh = (vehicle_data*)go;
					
					if ((c = get_char_by_vehicle(veh, name)) || (v = get_vehicle_by_vehicle(veh, name)) || (o = get_obj_by_vehicle(veh, name)) || (r = get_room(IN_ROOM(veh), name)) || (e = get_empire(name))) {
						// just setting vars
					}
					break;
				}
				case EMP_TRIGGER: {
					// emp = (empire_data*)go;
					
					if ((c = get_char(name))) {
						// just setting
					}
					else if ((v = get_vehicle(name))) {
						// just setting
					}
					else if ((o = get_obj(name))) {
						// just setting
					}
					else if ((r = get_room(NULL, name))) {
						// just setting
					}
					else if ((e = get_empire(name))) {
						// just setting
					}
					break;
				}
			}
		}
		else {
			if (!str_cmp(var, "self")) {
				switch (type) {
					case MOB_TRIGGER:
						c = (char_data*) go;
						r = NULL;
						v = NULL;
						o = NULL;  /* NULL assignments added to avoid self to always be    */ 
						e = NULL;
						break;     /* the room.  - Welcor        */
					case OBJ_TRIGGER:
						o = (obj_data*) go;
						c = NULL;
						r = NULL;
						v = NULL;
						e = NULL;
						break;
					case WLD_TRIGGER:
					case RMT_TRIGGER:
					case BLD_TRIGGER:
					case ADV_TRIGGER:
						r = (room_data*) go;
						c = NULL;
						o = NULL;
						v = NULL;
						e = NULL;
						break;
					case VEH_TRIGGER: {
						v = (vehicle_data*)go;
						c = NULL;
						o = NULL;
						r = NULL;
						e = NULL;
						break;
					}
					case EMP_TRIGGER: {
						e = (empire_data*)go;
						c = NULL;
						v = NULL;
						o = NULL;
						r = NULL;
						break;
					}
				}
			}
			else if (!str_cmp(var, "people")) {
				snprintf(str, slen, "%d",((num = atoi(field)) > 0) ? trgvar_in_room(num) : 0);        
				return;
			}
			else if (!str_cmp(var, "time")) {
				room_data *where = get_room_by_script(type, go);
				struct time_info_data tinfo = get_local_time(where);
				
				if (!str_cmp(field, "hour"))
					snprintf(str, slen, "%d", tinfo.hours);
				else if (!str_cmp(field, "day"))
					snprintf(str, slen, "%d", tinfo.day + 1);
				else if (!str_cmp(field, "month"))
					snprintf(str, slen, "%d", tinfo.month + 1);
				else if (!str_cmp(field, "year"))
					snprintf(str, slen, "%d", tinfo.year);
				else if (!str_cmp(field, "day_of_year")) {
					snprintf(str, slen, "%d", DAY_OF_YEAR(tinfo));
				}
				else
					*str = '\0';
				return;
			}
			else if (!str_cmp(var, "event")) {	// %event.*%
				if (!str_cmp(field, "name")) {
					if (subfield && *subfield && isdigit(*subfield)) {
						snprintf(str, slen, "%s", get_event_name_by_proto(atoi(subfield)));
					}
					else {
						script_log("Trigger: %s, VNum %d, event.name invalid event vnum: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), NULLSAFE(subfield));
						strcpy(str, "");
					}
				}
				else if (!str_cmp(field, "running")) {
					if (subfield && *subfield && isdigit(*subfield)) {
						snprintf(str, slen, "%d", find_running_event_by_vnum(atoi(subfield)) ? 1 : 0);
					}
					else {
						script_log("Trigger: %s, VNum %d, event.running invalid event vnum: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), NULLSAFE(subfield));
						strcpy(str, "0");
					}
				}
				else if (!str_cmp(field, "time")) {
					if (subfield && *subfield && isdigit(*subfield)) {
						struct event_running_data *ev = find_running_event_by_vnum(atoi(subfield));
						if (ev && ev->event && ev->status == EVTS_RUNNING) {
							int remains = (time(0) - (ev->start_time + (EVT_DURATION(ev->event) * SECS_PER_REAL_MIN))) / SECS_PER_REAL_MIN;
							snprintf(str, slen, "%d", MAX(1, remains));
						}
						else {
							strcpy(str, "0");	// not running = 0 time left
						}
					}
					else {
						script_log("Trigger: %s, VNum %d, event.time invalid event vnum: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), NULLSAFE(subfield));
						strcpy(str, "0");
					}
				}
				else {
					// bad field
					script_log("Trigger: %s, VNum %d, unknown event field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), field);
				}
				return;
			}
			else if (!str_cmp(var, "instance")) {
				struct instance_data *inst = get_instance_for_script(type, go);
				
				// does not require an instance
				if (!str_cmp(field, "nearest_adventure")) {
					room_data *find;
					any_vnum vnum;
					if (subfield && isdigit(*subfield) && ((vnum = atoi(subfield)) != NOTHING) && (find = find_nearest_adventure(get_room_by_script(type, go), vnum))) {
						snprintf(str, slen, "%c%d", UID_CHAR, GET_ROOM_VNUM(find) + ROOM_ID_BASE);
					}
					else {
						snprintf(str, slen, "0");
					}
				}
				else if (!str_cmp(field, "nearest_rmt")) {
					room_data *find;
					any_vnum vnum;
					if (subfield && isdigit(*subfield) && ((vnum = atoi(subfield)) != NOTHING) && (find = find_nearest_rmt(get_room_by_script(type, go), vnum))) {
						snprintf(str, slen, "%c%d", UID_CHAR, GET_ROOM_VNUM(find) + ROOM_ID_BASE);
					}
					else {
						snprintf(str, slen, "0");
					}
				}
				
				// everything else requires inst exists
				else if (!inst) {
					// safety
					snprintf(str, slen, "0");
				}
				else if (!str_cmp(field, "id")) {
					snprintf(str, slen, "%d", INST_ID(inst));
				}
				else if (!str_cmp(field, "load")) {
					check_instance_is_loaded(inst);
					strcpy(str, "1");
				}
				else if (!str_cmp(field, "loaded")) {
					snprintf(str, slen, "%d", IS_SET(INST_FLAGS(inst), INST_NEEDS_LOAD) ? 0 : 1);
				}
				else if (!str_cmp(field, "level")) {
					if (subfield && *subfield && INST_START(inst) && atoi(subfield) > 0) {
						lock_instance_level(INST_START(inst), atoi(subfield));
					}
					snprintf(str, slen, "%d", INST_LEVEL(inst));
				}
				else if (!str_cmp(field, "location")) {
					if (INST_FAKE_LOC(inst)) {
						snprintf(str, slen, "%c%d", UID_CHAR, GET_ROOM_VNUM(INST_FAKE_LOC(inst)) + ROOM_ID_BASE);
					}
					else {
						snprintf(str, slen, "0");
					}
				}
				else if (!str_cmp(field, "mob")) {	// search for mob in the instance
					if (subfield && *subfield && isdigit(*subfield)) {
						char_data *miter, *found_mob = NULL;
						mob_vnum vnum = atoi(subfield);
						DL_FOREACH(character_list, miter) {
							if (!EXTRACTED(miter) && GET_MOB_VNUM(miter) == vnum) {
								if (MOB_INSTANCE_ID(miter) == INST_ID(inst) || ROOM_INSTANCE(IN_ROOM(miter)) == inst) {
									found_mob = miter;
									break;
								}
							}
						}
						
						if (found_mob) {
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(found_mob));
						}
						else {
							*str = '\0';
						}
					}
					else {
						*str = '\0';
					}
				}
				else if (!str_cmp(field, "name")) {
					snprintf(str, slen, "%s", GET_ADV_NAME(INST_ADVENTURE(inst)));
				}
				else if (!str_cmp(field, "players_present")) {
					int count = 0;
					char_data *ch_iter;
					DL_FOREACH2(player_character_list, ch_iter, next_plr) {
						if (ROOM_INSTANCE(IN_ROOM(ch_iter)) == inst) {
							++count;
						}
					}
					snprintf(str, slen, "%d", count);
				}
				else if (!str_cmp(field, "real_location")) {
					if (INST_LOCATION(inst)) {
						snprintf(str, slen, "%c%d", UID_CHAR, GET_ROOM_VNUM(INST_LOCATION(inst)) + ROOM_ID_BASE);
					}
					else {
						snprintf(str, slen, "0");
					}
				}
				else if (!str_cmp(field, "set_location")) {
					room_data *targ_room;
					
					if (subfield && *subfield && (targ_room = get_room(get_room_by_script(type, go), subfield))) {
						if (SHARED_DATA(targ_room) && SHARED_DATA(targ_room) == &ocean_shared_data) {
							script_log("Trigger: %s, VNum %d, instance.set_location: unable to set location to '%d %s': location is in the ocean or is sharing data", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), GET_ROOM_VNUM(targ_room), get_room_name(targ_room, FALSE));
						}
						else {
							set_instance_fake_loc(inst, targ_room);
						}
					}
					else {
						script_log("Trigger: %s, VNum %d, bad instance.set_location: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), NULLSAFE(subfield));
					}
					
					strcpy(str, "0");
				}
				else if (!str_cmp(field, "start")) {
					if (INST_START(inst)) {
						snprintf(str, slen, "%c%d", UID_CHAR, GET_ROOM_VNUM(INST_START(inst)) + ROOM_ID_BASE);
					}
					else {
						snprintf(str, slen, "0");
					}
				}
				else {
					// bad field
					script_log("Trigger: %s, VNum %d, unknown instance field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), field);
				}
				return;
			}
			else if (!str_cmp(var, "cooldown")) {
				if (field && *field && isdigit(*field)) {
					generic_data *gen;
					if ((gen = find_generic(atoi(field), GENERIC_COOLDOWN))) {
						snprintf(str, slen, "%s", GEN_NAME(gen));
					}
					else {
						strcpy(str, "UNKNOWN");
					}
				}
				else {
					strcpy(str, "UNKNOWN");
				}
			}
			else if (!str_cmp(var, "currency")) {
				// %currency.<vnum>(<amt>)% gets the name for that currency
				if (field && *field && isdigit(*field)) {
					generic_data *gen;
					int amt = subfield ? atoi(subfield) : 1;
					
					if ((gen = find_generic(atoi(field), GENERIC_CURRENCY))) {
						snprintf(str, slen, "%s", GEN_STRING(gen, WHICH_CURRENCY(amt)));
					}
					else {
						strcpy(str, "UNKNOWN");
					}
				}
				else {
					strcpy(str, "UNKNOWN");
				}
			}
			else if (!str_cmp(var, "language")) {
				generic_data *lang = NULL;
				
				// look up language
				if (subfield && *subfield) {
					if (isdigit(*subfield)) {
						lang = find_generic(atoi(subfield), GENERIC_LANGUAGE);
					}
					if (!lang) {
						lang = find_generic_no_spaces(GENERIC_LANGUAGE, subfield);
					}
				}
				
				if (!str_cmp(field, "name")) {
					snprintf(str, slen, "%s", lang ? GEN_NAME(lang) : "");
				}
				else if (!str_cmp(field, "short")) {
					char temp[1024];
					int pos;
					
					strcpy(temp, lang ? GEN_NAME(lang) : "");
					for (pos = 0; pos < strlen(temp); ++pos) {
						if (temp[pos] == ' ') {
							temp[pos] = '-';
						}
						else {
							temp[pos] = LOWER(temp[pos]);
						}
					}
					snprintf(str, slen, "%s", temp);
				}
				else if (!str_cmp(field, "vnum")) {
					snprintf(str, slen, "%d", lang ? GEN_VNUM(lang) : -1);
				}
				else {
					snprintf(str, slen, "UNKNOWN");
				}
			}
			else if (!str_cmp(var, "random")) {
				if (!str_cmp(field, "char") || !str_cmp(field, "ally") || !str_cmp(field, "enemy")) {
					bool ally = (!str_cmp(field, "ally") ? TRUE : FALSE);
					bool enemy = (!str_cmp(field, "enemy") ? TRUE : FALSE);
					rndm = NULL;
					count = 0;

					if (type == MOB_TRIGGER) {
						ch = (char_data*) go;
						DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
							if (c == ch || !CAN_SEE(ch, c) || !valid_dg_target(c, DG_ALLOW_GODS)) {
								continue;
							}
							if (enemy && (AFF_FLAGGED(c, AFF_NO_ATTACK) || !is_fight_enemy(ch, c))) {
								continue;
							}
							if (ally && (AFF_FLAGGED(c, AFF_NO_ATTACK) || !is_fight_ally(ch, c))) {
								continue;
							}

							// it's all good
							if (!number(0, count))
								rndm = c;
							count++;
						}
					}
					else if (type == OBJ_TRIGGER) {
						DL_FOREACH2(ROOM_PEOPLE(obj_room((obj_data*) go)), c, next_in_room) {
							if (valid_dg_target(c, DG_ALLOW_GODS)) {
								if (!number(0, count))
									rndm = c;
								count++;
							}
						}
					}
					else if (type == WLD_TRIGGER || type == RMT_TRIGGER || type == BLD_TRIGGER || type == ADV_TRIGGER) {
						DL_FOREACH2(ROOM_PEOPLE((room_data*) go), c, next_in_room) {
							if (valid_dg_target(c, DG_ALLOW_GODS)) {
								if (!number(0, count)) {
									rndm = c;
								}
								count++;
							}
						}
					}
					else if (type == VEH_TRIGGER) {
						DL_FOREACH2(ROOM_PEOPLE(IN_ROOM((vehicle_data*)go)), c, next_in_room) {
							if (valid_dg_target(c, DG_ALLOW_GODS)) {
								if (!number(0, count++)) {
									rndm = c;
								}
							}
						}
					}
					else if (type == EMP_TRIGGER) {
						// type not supported here
					}

					if (rndm)
						snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(rndm));
					else
						*str = '\0';
				}
				else
					snprintf(str, slen, "%d", ((num = atoi(field)) > 0) ? number(1, num) : 0);

				return;
			}
			else if (!str_cmp(var, "skill")) {
				if (!str_cmp(field, "name")) {
					if (subfield && *subfield) {
						skill_data *sk = find_skill(subfield);
						snprintf(str, slen, "%s", sk ? SKILL_NAME(sk) : "");
					}
					else {
						*str = '\0';
					}
				}
				else if (!str_cmp(field, "validate")) {
					if (subfield && *subfield) {
						skill_data *sk = find_skill(subfield);
						snprintf(str, slen, "%d", sk ? 1 : 0);
					}
					else {
						snprintf(str, slen, "0");
					}
				}
			}
			else if (!str_cmp(var, "temperature")) {
				if (field && (*field == '-' || *field == '+' || isdigit(*field))) {
					snprintf(str, slen, "%s", temperature_to_string(atoi(field)));
				}
				else {
					strcpy(str, "");
				}
			}
			// no else
		}

		if (c) {
			if (text_processed(field, subfield, vd, str, slen, go, sc, trig, type))
				return;

			else if (!str_cmp(field, "global")) { /* get global of something else */
				if (IS_NPC(c) && c->script) {
					find_replacement(go, c->script, NULL, MOB_TRIGGER, subfield, NULL, NULL, str, slen);
				}
			}
			/* set str to some 'non-text' first */
			*str = '\x1';

			switch (LOWER(*field)) {
				case 'a': {	// char.a*
					if (!str_cmp(field, "ability")) {
						if (subfield && *subfield) {
							ability_data *ab = find_ability(subfield);
							if (ab) {
								snprintf(str, slen, (IS_NPC(c) || has_ability(c, ABIL_VNUM(ab))) ? "1" : "0");
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "action")) {
						if (GET_FEEDING_FROM(c)) {
							snprintf(str, slen, "feeding");
						}
						else if (IS_NPC(c) || GET_ACTION(c) == ACT_NONE) {
							strcpy(str, "");	// none
						}
						else if (GET_ACTION(c) == ACT_GEN_CRAFT) {
							craft_data *ctype = craft_proto(GET_ACTION_VNUM(c, 0));
							snprintf(str, slen, "%s", gen_craft_data[GET_CRAFT_TYPE(ctype)].verb);
						}
						else {
							snprintf(str, slen, "%s", action_data[GET_ACTION(c)].name);
						}
					}
					
					else if (!str_cmp(field, "add_companion")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield)) {
							char_data *pet = mob_proto(atoi(subfield));
							if (pet) {
								add_companion(c, GET_MOB_VNUM(pet), NO_ABIL);
							}
							else {
								script_log("Trigger: %s, VNum %d, attempting to add invalid companion: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
							}
						}
						
						strcpy(str, "0");
					}
					else if (!str_cmp(field, "add_lastname")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							add_lastname(c, subfield);
						}
						strcpy(str, "0");
					}
					else if (!str_cmp(field, "add_learned")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							craft_data *cft = craft_proto(atoi(subfield));
							if (cft && CRAFT_FLAGGED(cft, CRAFT_LEARNED) && !CRAFT_FLAGGED(cft, CRAFT_IN_DEVELOPMENT)) {
								add_learned_craft(c, GET_CRAFT_VNUM(cft));
							}
							else {
								script_log("Trigger: %s, VNum %d, attempting to add invalid learned craft: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
							}
						}
						
						strcpy(str, "0");
					}
					
					else if (!str_cmp(field, "add_minipet")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield)) {
							char_data *pet = mob_proto(atoi(subfield));
							if (pet) {
								add_minipet(c, GET_MOB_VNUM(pet));
							}
							else {
								script_log("Trigger: %s, VNum %d, attempting to add invalid minipet: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
							}
						}
						
						strcpy(str, "0");
					}
					
					else if (!str_cmp(field, "add_mob_flag")) {
						if (subfield && *subfield && IS_NPC(c)) {
							bitvector_t pos = search_block(subfield, action_bits, FALSE);
							if (pos != NOTHING) {
								set_mob_flags(c, BIT(pos));
							}
							else {
								script_log("Trigger: %s, VNum %d, unknown mob flag: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
							}
						}
						snprintf(str, slen, "0");
					}
					
					else if (!str_cmp(field, "add_mount")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield)) {
							char_data *mount = mob_proto(atoi(subfield));
							if (mount) {
								add_mount(c, GET_MOB_VNUM(mount), get_mount_flags_by_mob(mount));
							}
							else {
								script_log("Trigger: %s, VNum %d, attempting to add invalid mount: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
							}
						}
						
						strcpy(str, "0");
					}
					
					else if (!str_cmp(field, "add_resources")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							// nop %actor.add_resources(vnum, number)%
							char arg1[256], arg2[256];
							struct resource_data *res = NULL;
							obj_vnum vnum;
							int amt;
						
							comma_args(subfield, arg1, arg2);
							if (*arg1 && *arg2 && (vnum = atoi(arg1)) > 0 && (amt = atoi(arg2)) != 0 && obj_proto(vnum)) {
								add_to_resource_list(&res, RES_OBJECT, vnum, ABSOLUTE(amt), 0);
								
								// this sends an error message to c on failure
								if (amt > 0) {
									give_resources(c, res, FALSE);
								}
								else {
									extract_resources(c, res, can_use_room(c, IN_ROOM(c), GUESTS_ALLOWED), NULL);
								}
								
								free_resource_list(res);
							}
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "add_unlocked_archetype")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield)) {
							archetype_data *arch = archetype_proto(atoi(subfield));
							if (arch && ARCHETYPE_FLAGGED(arch, ARCH_LOCKED)) {
								add_unlocked_archetype(c, GET_ARCH_VNUM(arch));
							}
							else {
								script_log("Trigger: %s, VNum %d, attempting to add invalid archetype: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
							}
						}
						
						strcpy(str, "0");
					}
					else if (!str_cmp(field, "adventure_summoned_from")) {
						room_data *find;
						if (!IS_NPC(c) && PLR_FLAGGED(c, PLR_ADVENTURE_SUMMONED) && (find = real_room(GET_ADVENTURE_SUMMON_RETURN_LOCATION(c)))) {
							snprintf(str, slen, "%c%d", UID_CHAR, room_script_id(find));
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "aff_flagged")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, affected_bits, FALSE);
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", AFF_FLAGGED(c, BIT(pos)) ? 1 : 0);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							sprintbit(AFF_FLAGS(c), affected_bits, buf, TRUE);
							snprintf(str, slen, "%s", buf);
						}
					}
					
					else if (!str_cmp(field, "alias"))
						snprintf(str, slen, "%s", GET_PC_NAME(c));
					else if (!str_cmp(field, "allegiance")) {
						if (IS_NPC(c)) {
							faction_data *fct;
							if (subfield && *subfield) {
								if (!str_cmp(subfield, "none")) {
									MOB_FACTION(c) = NULL;
								}
								else if ((fct = find_faction(subfield))) {
									MOB_FACTION(c) = fct;
								}
								else {
									script_log("Trigger: %s, VNum %d, unknown faction: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
								}
							}
							
							snprintf(str, slen, "%d", MOB_FACTION(c) ? FCT_VNUM(MOB_FACTION(c)) : -1);
						}
						else {
							snprintf(str, slen, "-1");
						}
					}
					/*
					else if (!str_cmp(field, "align")) {
						if (subfield && *subfield) {
							int addition = atoi(subfield);
							GET_ALIGNMENT(c) = MAX(-1000, MIN(addition, 1000));
						} 
						snprintf(str, slen, "%d", GET_ALIGNMENT(c));
					}
					*/
					else if (!str_cmp(field, "affect")) {
						if (subfield && *subfield) {
							generic_data *gen;
							
							if (isdigit(*subfield) && (gen = find_generic(atoi(subfield), GENERIC_AFFECT))) {
								snprintf(str, slen, "%d", affected_by_spell(c, GEN_VNUM(gen)) ? 1 : 0);
							}
							else if ((gen = find_generic_by_name(GENERIC_AFFECT, subfield, TRUE))) {
								snprintf(str, slen, "%d", affected_by_spell(c, GEN_VNUM(gen)) ? 1 : 0);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					break;
				}
				case 'b': {	// char.b*
					if (!str_cmp(field, "biting")) {
						if (GET_FEEDING_FROM(c)) {
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(GET_FEEDING_FROM(c)));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "bitten_by")) {
						if (GET_FED_ON_BY(c)) {
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(GET_FED_ON_BY(c)));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "block")) {
						snprintf(str, slen, "%d", get_block_rating(c, FALSE));
					}
					else if (!str_cmp(field, "blood")) {
						if (subfield && *subfield) {
							int amt = atoi(subfield);
							if (amt != 0) {
								set_blood(c, GET_BLOOD(c) + amt);
								if (GET_BLOOD(c) <= 0) {
									out_of_blood(c);
								}
							}
							
							// return current remainder
							snprintf(str, slen, "%d", GET_BLOOD(c));
						}
						else {
							// current blood
							snprintf(str, slen, "%d", GET_BLOOD(c));
						}
					}
					else if (!str_cmp(field, "bonus_exp")) {
						if (!IS_NPC(c)) {
							int amt;
							if (subfield && *subfield && (amt = atoi(subfield)) != 0 && !IS_NPC(c)) {
								SAFE_ADD(GET_DAILY_BONUS_EXPERIENCE(c), amt, 0, UCHAR_MAX, FALSE);
								update_MSDP_bonus_exp(c, UPDATE_SOON);
							}
							snprintf(str, slen, "%d", GET_DAILY_BONUS_EXPERIENCE(c));
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "bonus_healing")) {
						snprintf(str, slen, "%d", GET_BONUS_HEALING(c));
					}
					else if (!str_cmp(field, "bonus_magical")) {
						snprintf(str, slen, "%d", GET_BONUS_MAGICAL(c));
					}
					else if (!str_cmp(field, "bonus_physical")) {
						snprintf(str, slen, "%d", GET_BONUS_PHYSICAL(c));
					}
					break;
				}
				case 'c': {	// char.c*
					/*
					else if (!str_cmp(field, "coins")) {
						// TODO possibly a way to specify coin type or do it by actor's loyalty?
						// nop %c.coins(10)%
						if (subfield && *subfield) {
							int addition = atoi(subfield);
							increase_coins(c, OTHER_COIN, addition);
						}
						// snprintf(str, slen, "%d", GET_GOLD(c));
					}
					*/
					
					if (!str_cmp(field, "can_act")) {
						if (!AFF_FLAGGED(c, AFF_DISTRACTED) && GET_POS(c) == POS_STANDING && !FIGHTING(c) && (IS_NPC(c) || GET_ACTION(c) == ACT_NONE) && !EXTRACTED(c) && !GET_FEEDING_FROM(c) && !GET_FED_ON_BY(c) && !IS_DEAD(c) && !AFF_FLAGGED(c, AFF_STUNNED | AFF_HARD_STUNNED)) {
							strcpy(str, "1");
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "can_afford")) {
						if (subfield && isdigit(*subfield)) {
							if (can_afford_coins(c, (type == MOB_TRIGGER) ? GET_LOYALTY((char_data*)go) : REAL_OTHER_COIN, atoi(subfield))) {
								snprintf(str, slen, "1");
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "can_gain_new_skills")) {
						snprintf(str, slen, "%d", (!IS_NPC(c) && CAN_GAIN_NEW_SKILLS(c)) ? 1 : 0);
					}
					else if (!str_cmp(field, "cancel_adventure_summon")) {
						if (!IS_NPC(c)) {
							cancel_adventure_summon(c);
						}
						*str = '\0';
					}
					else if (!str_cmp(field, "can_recognize_lang")) {
						// arg1: language name/vnum
						// arg2: optional on/off to toggle recognize
						char arg1[256], arg2[256];
						int mode;
						generic_data *lang = NULL;
						
						comma_args(subfield, arg1, arg2);
						
						// load language
						if (*arg1) {
							if (isdigit(*arg1)) {
								lang = find_generic(atoi(arg1), GENERIC_LANGUAGE);
							}
							if (!lang) {
								lang = find_generic_no_spaces(GENERIC_LANGUAGE, arg1);
							}
						}
						
						// optional toggle on/off
						if (!IS_NPC(c) && *arg2 && lang) {
							if (!str_cmp(arg2, "1") || !str_cmp(arg2, "on")) {
								if (speaks_language(c, GEN_VNUM(lang)) != LANG_SPEAK) {
									add_language(c, GEN_VNUM(lang), LANG_RECOGNIZE);
								}
							}
							else if (!str_cmp(arg2, "0") || !str_cmp(arg2, "off")) {
								if (speaks_language(c, GEN_VNUM(lang)) != LANG_SPEAK) {
									add_language(c, GEN_VNUM(lang), LANG_UNKNOWN);
								}
							}
							check_languages(c);
						}
						
						// and show recognizability
						mode = lang ? speaks_language(c, GEN_VNUM(lang)) : LANG_UNKNOWN;
						snprintf(str, slen, "%d", (mode == LANG_RECOGNIZE || mode == LANG_SPEAK) ? 1 : 0);
					}
					else if (!str_cmp(field, "can_speak")) {
						// arg1: language name/vnum
						// arg2: optional on/off to toggle speaking
						char arg1[256], arg2[256];
						generic_data *lang = NULL;
						
						comma_args(subfield, arg1, arg2);
						
						// load language
						if (*arg1) {
							if (isdigit(*arg1)) {
								lang = find_generic(atoi(arg1), GENERIC_LANGUAGE);
							}
							if (!lang) {
								lang = find_generic_no_spaces(GENERIC_LANGUAGE, arg1);
							}
						}
						
						// optional toggle on/off
						if (!IS_NPC(c) && *arg2 && lang) {
							if (!str_cmp(arg2, "1") || !str_cmp(arg2, "on")) {
								add_language(c, GEN_VNUM(lang), LANG_SPEAK);
							}
							else if (!str_cmp(arg2, "0") || !str_cmp(arg2, "off")) {
								add_language(c, GEN_VNUM(lang), LANG_UNKNOWN);
							}
							check_languages(c);
						}
						
						// and show speakability
						snprintf(str, slen, "%d", (lang && speaks_language(c, GEN_VNUM(lang)) == LANG_SPEAK) ? 1 : 0);
					}
					else if (!str_cmp(field, "can_start_quest")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							any_vnum vnum = atoi(subfield);
							quest_data *qst = quest_proto(vnum);
							if (IS_NPC(c) || !qst || is_on_quest(c, vnum)) {
								// cannot start
								strcpy(str, "0");
							}
							else {
								// maybe
								struct instance_data *inst = get_instance_for_script(type, go);
								if (char_meets_prereqs(c, qst, inst)) {
									strcpy(str, "1");
								}
								else {
									strcpy(str, "0");
								}
							}
						}
					}
					else if (!str_cmp(field, "char_target")) {
						char_data *targ;
						*str = '\0';	// default to no-target
						if (subfield && *subfield) {
							if ((targ = get_char_room_vis(c, subfield, NULL))) {
								snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(targ));
							}
						}
					}
					else if (!str_cmp(field, "charge_coins")) {
						if (subfield && isdigit(*subfield)) {
							// will report output to str
							charge_coins(c, (type == MOB_TRIGGER) ? GET_LOYALTY((char_data*)go) : REAL_OTHER_COIN, atoi(subfield), NULL, str);
						}
					}
					else if (!str_cmp(field, "charge_component")) {
						// args: (type, number) -- type may be vnum or name
						char arg1[256], arg2[256];
						struct resource_data *resources = NULL;
						generic_data *cmp;
						
						comma_args(subfield, arg1, arg2);
						
						if (!subfield || !isdigit(*arg2) || !(cmp = find_generic_component(arg1))) {
							script_log("Trigger: %s, VNum %d, bad args to charge_component(%s)", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), NULLSAFE(subfield));
							*str = '\0';
						}
						else {
							add_to_resource_list(&resources, RES_COMPONENT, GEN_VNUM(cmp), atoi(arg2), 0);
							extract_resources(c, resources, FALSE, NULL);
							free_resource_list(resources);
							strcpy(str, "1");
						}
					}
					else if (!str_cmp(field, "charge_currency")) {
						if (subfield && isdigit(*subfield)) {
							char arg1[256], arg2[256];
							comma_args(subfield, arg1, arg2);
							if (*arg1 && *arg2) {
								add_currency(c, atoi(arg1), -atoi(arg2));
							}
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "canbeseen")) {
						if ((type == MOB_TRIGGER) && !CAN_SEE(((char_data*)go), c))
							snprintf(str, slen, "0");
						else
							snprintf(str, slen, "1");
					}
					else if (!str_cmp(field, "canuseroom_ally")) {
						room_data *troom = (subfield && *subfield) ? get_room(IN_ROOM(c), subfield) : IN_ROOM(c);
						snprintf(str, slen, "%d", (troom && can_use_room(c, troom, MEMBERS_AND_ALLIES)) ? 1 : 0);
					}
					else if (!str_cmp(field, "canuseroom_guest")) {
						room_data *troom = (subfield && *subfield) ? get_room(IN_ROOM(c), subfield) : IN_ROOM(c);
						snprintf(str, slen, "%d", (troom && can_use_room(c, troom, GUESTS_ALLOWED)) ? 1 : 0);
					}
					else if (!str_cmp(field, "canuseroom_member")) {
						room_data *troom = (subfield && *subfield) ? get_room(IN_ROOM(c), subfield) : IN_ROOM(c);
						snprintf(str, slen, "%d", (troom && can_use_room(c, troom, MEMBERS_ONLY)) ? 1 : 0);
					}
					else if (!str_cmp(field, "can_enter_instance")) {
						room_data *troom = (subfield && *subfield) ? get_room(IN_ROOM(c), subfield) : IN_ROOM(c);
						struct instance_data *inst;
						
						if (troom && IS_ADVENTURE_ROOM(troom) && (inst = find_instance_by_room(troom, FALSE, FALSE))) {
							// only if not already in there
							if (!IS_ADVENTURE_ROOM(IN_ROOM(c)) || find_instance_by_room(IN_ROOM(c), FALSE, FALSE) != inst) {
								if (!can_enter_instance(c, inst)) {
									snprintf(str, slen, "0");
								}
							}
						}
						if (*str != '0') {	// all other cases
							snprintf(str, slen, "1");
						}
					}
					else if (!str_cmp(field, "can_see")) {
						vehicle_data *vtarg;
						char_data *ctarg;
						obj_data *otarg;
						
						if (subfield && (ctarg = get_char(subfield))) {
							snprintf(str, slen, "%d", CAN_SEE(c, ctarg) ? 1 : 0);
						}
						else if (subfield && (otarg = get_obj(subfield))) {
							snprintf(str, slen, "%d", CAN_SEE_OBJ(c, otarg) ? 1 : 0);
						}
						else if (subfield && (vtarg = get_vehicle(subfield))) {
							snprintf(str, slen, "%d", CAN_SEE_VEHICLE(c, vtarg) ? 1 : 0);
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "can_teleport_room")) {
						room_data *troom = (subfield && *subfield) ? get_room(IN_ROOM(c), subfield) : IN_ROOM(c);
						snprintf(str, slen, "%d", (troom && can_teleport_to(c, troom, TRUE)) ? 1 : 0);
					}
					else if (!str_cmp(field, "carrying")) {
						snprintf(str, slen, "%d", IS_CARRYING_N(c));
					}
					else if (!str_cmp(field, "class")) {
						if (IS_NPC(c) || !GET_CLASS(c)) {
							*str = '\0';
						}
						else {
							snprintf(str, slen, "%s", SHOW_CLASS_NAME(c));
						}
					}
					else if (!str_cmp(field, "cha") || !str_cmp(field, "charisma")) {
						snprintf(str, slen, "%d", GET_CHARISMA(c));
					}
					else if (!str_cmp(field, "companion")) {
						if (GET_COMPANION(c)) {
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(GET_COMPANION(c)));
						}
						else {
							*str = '\0';
						}
					}
					
					else if (!str_cmp(field, "coins")) {
						empire_data *coin_emp;
						struct coin_data *coin;
						int total;
						
						if (IS_NPC(c)) {
							// no coins
							snprintf(str, slen, "0");
						}
						else if (subfield && (isdigit(*subfield) || *subfield == '-')) {
							// numeric subfield
							if (atoi(subfield) > 0) {
								if ((coin_emp = real_empire(atoi(subfield))) && (coin = find_coin_entry(GET_PLAYER_COINS(c), coin_emp))) {
									snprintf(str, slen, "%d", coin->amount);
								}
								else {
									// empire not found, thus coins not found
									snprintf(str, slen, "0");
								}
							}
							else if (!strcmp(subfield, "-1") || (isdigit(*subfield) && atoi(subfield) == 0)) {
								// misc coins
								coin = find_coin_entry(GET_PLAYER_COINS(c), REAL_OTHER_COIN);
								snprintf(str, slen, "%d", coin ? coin->amount : 0);
							}
						}
						else if (subfield && *subfield) {
							// non-numeric subfield
							if ((coin_emp = get_empire(subfield)) && (coin = find_coin_entry(GET_PLAYER_COINS(c), coin_emp))) {
								snprintf(str, slen, "%d", coin->amount);
							}
							else {
								// empire not found, thus coins not found
								snprintf(str, slen, "0");
							}
						}
						else {	// empty subfield: total coins
							total = 0;
							LL_FOREACH(GET_PLAYER_COINS(c), coin) {
								SAFE_ADD(total, coin->amount, 0, INT_MAX, FALSE);
							}
							snprintf(str, slen, "%d", total);
						}
					}
					
					else if (!str_cmp(field, "command_lag")) {
						// true/false if the character is in a "wait", OR sets a wait
						int wait_type;
						if (subfield && *subfield && (wait_type = search_block(subfield, wait_types, FALSE)) != NOTHING) {
							command_lag(c, wait_type);
						}
						snprintf(str, slen, "%d", (GET_WAIT_STATE(c) > 0 ? 1 : 0));
					}
					else if (!str_cmp(field, "completed_quest")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							any_vnum vnum = atoi(subfield);
							if (!IS_NPC(c) && has_completed_quest_any(c, vnum)) {
								strcpy(str, "1");
							}
							else {
								strcpy(str, "0");
							}
						}
					}
					else if (!str_cmp(field, "completed_quest_instance")) {
						struct instance_data *inst = get_instance_for_script(type, go);
						
						if (subfield && *subfield && isdigit(*subfield) && inst) {
							any_vnum vnum = atoi(subfield);
							if (!IS_NPC(c) && has_completed_quest(c, vnum, INST_ID(inst))) {
								strcpy(str, "1");
							}
							else {
								strcpy(str, "0");
							}
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "cooldown")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							snprintf(str, slen, "%d", get_cooldown_time(c, atoi(subfield)));
						}
					}
					else if (!str_cmp(field, "crafting_level")) {
						snprintf(str, slen, "%d", get_crafting_level(c));
					}
					else if (!str_cmp(field, "currency")) {
						if (subfield && *subfield) {
							snprintf(str, slen, "%d", get_currency(c, atoi(subfield)));
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "custom")) {
						// default to empty
						*str = '\0';
						if (subfield && *subfield && IS_NPC(c)) {
							char arg1[256], arg2[256];
							char *msg;
							int ctype, pos = -1;
							
							comma_args(subfield, arg1, arg2);
							if (*arg1 && (ctype = search_block(arg1, mob_custom_types, FALSE)) != NOTHING) {
								if (*arg2) {	// optional message pos
									pos = atoi(arg2);
									msg = get_custom_message_pos(MOB_CUSTOM_MSGS(c), ctype, pos);
								}
								else {
									msg = get_custom_message(MOB_CUSTOM_MSGS(c), ctype);
								}
								
								// did we find one
								if (msg) {
									snprintf(str, slen, "%s", NULLSAFE(msg));
								}
							}
						}
					}

					break;
				}
				case 'd': {	// char.d*
					if (!str_cmp(field, "dead")) {
						snprintf(str, slen, "%d", IS_DEAD(c) ? 1 : 0);
					}
					else if (!str_cmp(field, "dex") || !str_cmp(field, "dexterity")) {
						snprintf(str, slen, "%d", GET_DEXTERITY(c));
					}
					else if (!str_cmp(field, "dir")) {
						if (subfield && *subfield) {
							int dir;
							if ((dir = search_block(subfield, dirs, FALSE)) != NOTHING || (dir = search_block(subfield, alt_dirs, FALSE)) != NOTHING) {
								if (IS_NPC(c) || HAS_NAVIGATION(c)) {
									snprintf(str, slen, "%s", dirs[dir]);	// real dir
								}
								else {
									snprintf(str, slen, "%s", dirs[get_direction_for_char(c, dir)]);	// fake dir
								}
							}
							else {
								// bad direction -- just give them their arg back
								snprintf(str, slen, "%s", subfield);
							}
						}
						else {	// missing arg
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "disabled")) {
						// things which would keep a character from acting
						if (EXTRACTED(c) || GET_FEEDING_FROM(c) || GET_FED_ON_BY(c) || IS_DEAD(c) || GET_POS(c) < POS_SLEEPING || AFF_FLAGGED(c, AFF_STUNNED | AFF_HARD_STUNNED)) {
							snprintf(str, slen, "1");
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "dodge")) {
						snprintf(str, slen, "%d", get_dodge_modifier(c, NULL, FALSE));
					}
					else if (!str_cmp(field, "drunk")) {
						if (subfield && *subfield) {
							gain_condition(c, DRUNK, atoi(subfield) * REAL_UPDATES_PER_MUD_HOUR);
						}
						snprintf(str, slen, "%d", GET_COND(c, DRUNK) / REAL_UPDATES_PER_MUD_HOUR);
					}
					break;
				}
				case 'e': {	// char.e*
					/*
					if (!str_cmp(field, "exp")) {
						if (subfield && *subfield) {
							int addition = MIN(atoi(subfield), 1000);

							gain_exp(c, addition);
						}
						snprintf(str, slen, "%d", GET_EXP(c));
					}
					*/

					if (!str_cmp(field, "empire")) {
						if (GET_LOYALTY(c)) {
							snprintf(str, slen, "%c%d", UID_CHAR, EMPIRE_VNUM(GET_LOYALTY(c)) + EMPIRE_ID_BASE);
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "end_adventure_summon")) {
						*str = '\0';
						if (IS_NPC(c) && PLR_FLAGGED(c, PLR_ADVENTURE_SUMMONED)) {
							adventure_unsummon(c);
						}
					}
					else if (!str_cmp(field, "eq")) {
						int pos;
						if (!subfield || !*subfield)
							strcpy(str,"");
						else if (*subfield == '*') {
							int i, j = 0;
							for (i = 0; i < NUM_WEARS; i++)
								if (GET_EQ(c, i)) { 
									j++;
									break;
								}
							if (j > 0)
								strcpy(str,"1");
							else
								strcpy(str,"");
						}
						else if ((pos = find_eq_pos_script(subfield)) < 0 || !GET_EQ(c, pos))
							strcpy(str,"");
						else
							snprintf(str, slen, "%c%d",UID_CHAR, obj_script_id(GET_EQ(c, pos)));
					}
					else if (!str_cmp(field, "event_points")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							struct event_running_data *running = find_running_event_by_vnum(atoi(subfield));
							struct player_event_data *ped;
							if (running && (ped = get_event_data(c, running->id))) {
								snprintf(str, slen, "%d", ped->points);
							}
							else {
								strcpy(str, "0");
							}
						}
						else {
							strcpy(str, "0");
						}
					}
					break;
				}
				case 'f': {	// char.f*
					if (!str_cmp(field, "faction_name")) {
						if (IS_NPC(c) && MOB_FACTION(c)) {
							snprintf(str, slen, "%s", FCT_NAME(MOB_FACTION(c)));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "fighting")) {
						if (FIGHTING(c))
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(FIGHTING(c)));
						else 
							*str = '\0';
					}
					else if (!str_cmp(field, "find_lighter")) {
						bool junk;
						obj_data *lighter = find_lighter_in_list(c->carrying, &junk);
						if (lighter) {
							snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id(lighter));
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "firstname")) {
						if (IS_NPC(c)) {
							char temp[MAX_STRING_LENGTH];
							strcpy(temp, fname(GET_PC_NAME(c)));
							snprintf(str, slen, "%s", CAP(temp));
						}
						else {
							snprintf(str, slen, "%s", GET_PC_NAME(c));
						}
					}
					else if (!str_cmp(field, "flush_queue")) {
						if (c->desc) {
							send_stacked_msgs(c->desc);
						}
						*str = '\0';
					}
					else if (!str_cmp(field, "follower")) {
						struct follow_type *fol;
						bool fnd = FALSE;
						
						if (!c->followers || !c->followers->follower) {
							// no followers
							*str = '\0';
						}
						else if (subfield && *subfield) {
							LL_FOREACH(c->followers, fol) {
								if (fol->follower && multi_isname(subfield, GET_PC_NAME(fol->follower))) {
									// found follower
									snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(fol->follower));
									fnd = TRUE;
									break;
								}
							}
							if (!fnd) {
								*str = '\0';	// no match
							}
						}
						else {
							// simple 1st-follower
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(c->followers->follower));
						}
					}
					break;
				}
				case 'g': {	// char.g*
					if (!str_cmp(field, "grt") || !str_cmp(field, "greatness")) {
						snprintf(str, slen, "%d", GET_GREATNESS(c));
					}
					else if (!str_cmp(field, "gain_event_points")) {
						// %actor.gain_event_points(vnum,amt)%
						
						if (subfield && *subfield && !IS_NPC(c)) {
							char arg1[256], arg2[256];
							comma_args(subfield, arg1, arg2);
							
							if (*arg1 && *arg2 && isdigit(*arg1)) {
								int pts = gain_event_points(c, atoi(arg1), atoi(arg2));
								snprintf(str, slen, "%d", pts);
							}
							else {
								strcpy(str, "0");
							}
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "gain_reputation")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							char arg1[256], arg2[256];
							faction_data *fct;
							int amount = 0;
							
							comma_args(subfield, arg1, arg2);
							if (*arg1 && *arg2 && (fct = find_faction(arg1)) && (amount = atoi(arg2)) != 0) {
								gain_reputation(c, FCT_VNUM(fct), amount, FALSE, TRUE);
								snprintf(str, slen, "1");
							}
						}
						// all other cases...
						if (*str != '1') {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "gain_skill")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							// %actor.gain_skill(skill, amount)%
							char arg1[256], arg2[256];
							skill_data *sk;
							int amount = 0;
							
							comma_args(subfield, arg1, arg2);
							if (*arg1 && *arg2 && (sk = find_skill(arg1)) && (amount = atoi(arg2)) != 0 && noskill_ok(c, SKILL_VNUM(sk))) {
								gain_skill(c, sk, amount, NULL);
								snprintf(str, slen, "1");
							}
						}
						// all other cases...
						if (*str != '1') {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "give_coins")) {
						if (subfield && isdigit(*subfield)) {
							increase_coins(c, (type == MOB_TRIGGER) ? GET_LOYALTY((char_data*)go) : REAL_OTHER_COIN, atoi(subfield));
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "give_currency")) {
						if (subfield && isdigit(*subfield)) {
							char arg1[256], arg2[256];
							comma_args(subfield, arg1, arg2);
							if (*arg1 && *arg2) {
								add_currency(c, atoi(arg1), atoi(arg2));
							}
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "give_skill_reset")) {
						skill_data *sk = find_skill(subfield);
						struct player_skill_data *skdata;
						
						if (sk && (skdata = get_skill_data(c, SKILL_VNUM(sk), TRUE))) {
							skdata->resets = MIN(skdata->resets + 1, MAX_SKILL_RESETS);
							queue_delayed_update(c, CDU_MSDP_SKILLS);
						}
						*str = '\0';
					}
					break;
				}
				case 'h': {	// char.h*
					if (!str_cmp(field, "has_companion")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield) && has_companion(c, atoi(subfield)) != NULL) {
							strcpy(str, "1");
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "has_component")) {
						// args: (type, number) -- type may be vnum or name
						char arg1[256], arg2[256];
						struct resource_data *resources = NULL;
						generic_data *cmp;
						
						comma_args(subfield, arg1, arg2);
						
						if (!subfield || !isdigit(*arg2) || !(cmp = find_generic_component(arg1))) {
							script_log("Trigger: %s, VNum %d, bad args to has_component(%s)", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), NULLSAFE(subfield));
							*str = '\0';
						}
						else {
							add_to_resource_list(&resources, RES_COMPONENT, GEN_VNUM(cmp), atoi(arg2), 0);
							snprintf(str, slen, "%d", has_resources(c, resources, FALSE, FALSE, NULL) ? 1 : 0);
							free_resource_list(resources);
						}
					}
					else if (!str_cmp(field, "has_item")) {
						if (!(subfield && *subfield))
							*str = '\0';
						else
							snprintf(str, slen, "%d", char_has_item(subfield, c));
					}
					else if (!str_cmp(field, "has_lastname")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							snprintf(str, slen, "%d", has_lastname(c, subfield) ? 1 : 0);
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "has_minipet")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield) && has_minipet(c, atoi(subfield))) {
							strcpy(str, "1");
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "has_mount")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield)) {
							snprintf(str, slen, "%d", find_mount_data(c, atoi(subfield)) ? 1 : 0);
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "has_reputation")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							// %actor.has_reputation(vnum, level)%
							char arg1[256], arg2[256];
							faction_data *fct;
							int rep;
							
							comma_args(subfield, arg1, arg2);
							if (*arg1 && *arg2 && (fct = find_faction(arg1)) && (rep = get_reputation_by_name(arg2)) != NOTHING) {
								snprintf(str, slen, "%d", has_reputation(c, FCT_VNUM(fct), rep) ? 1 : 0);
							}
						}
						// all other cases...
						if (*str != '1') {
							snprintf(str, slen, "0");
						}
					}
					
					else if (!str_cmp(field, "has_resources")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							// %actor.has_resources(vnum, number)%
							char arg1[256], arg2[256];
							struct resource_data *res = NULL;
							obj_vnum vnum;
							int amt;
						
							comma_args(subfield, arg1, arg2);
							if (*arg1 && *arg2 && (vnum = atoi(arg1)) > 0 && (amt = atoi(arg2)) > 0) {
								add_to_resource_list(&res, RES_OBJECT, vnum, amt, 0);
								
								if (has_resources(c, res, can_use_room(c, IN_ROOM(c), GUESTS_ALLOWED), FALSE, NULL)) {
									snprintf(str, slen, "1");
								}
								
								free_resource_list(res);
							}
						}
						// all other cases...
						if (*str != '1') {							
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "has_tech")) {
						if (subfield && *subfield) {
							int pos;
							
							if ((pos = search_block(subfield, player_tech_types, FALSE)) != NOTHING) {
								snprintf(str, slen, "%d", has_player_tech(c, pos) ? 1 : 0);
							}
							else {
								*str = '\0';
							}
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "has_trigger")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							snprintf(str, slen, "%d", has_trigger(SCRIPT(c), atoi(subfield)));
						}
						else {
							snprintf(str, slen, "0");	// no vnum provided
						}
					}
					else if (!str_cmp(field, "has_unlocked_archetype")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield) && has_unlocked_archetype(c, atoi(subfield))) {
							strcpy(str, "1");
						}
						else {
							strcpy(str, "0");
						}
					}
					
					else if (!str_cmp(field, "hisher"))
						snprintf(str, slen, "%s", HSHR(c));

					else if (!str_cmp(field, "heshe"))
						snprintf(str, slen, "%s", HSSH(c));

					else if (!str_cmp(field, "highest_level")) {
						if (IS_NPC(c)) {
							int lev = get_approximate_level(c);
							snprintf(str, slen, "%d", MAX(lev, GET_MAX_SCALE_LEVEL(c)));
						}
						else {
							snprintf(str, slen, "%d", GET_HIGHEST_KNOWN_LEVEL(c));
						}
					}

					else if (!str_cmp(field, "himher"))
						snprintf(str, slen, "%s", HMHR(c));

					else if (!str_cmp(field, "hitp") || !str_cmp(field, "health"))
						snprintf(str, slen, "%d", GET_HEALTH(c));
						
					else if (!str_cmp(field, "home")) {
						room_data *home_loc;
						
						if ((home_loc = find_home(c))) {
							snprintf(str, slen, "%c%d", UID_CHAR, GET_ROOM_VNUM(home_loc) + ROOM_ID_BASE);
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "hunger")) {
						if (subfield && *subfield) {
							gain_condition(c, FULL, atoi(subfield) * REAL_UPDATES_PER_MUD_HOUR);
						}
						snprintf(str, slen, "%d", GET_COND(c, FULL) / REAL_UPDATES_PER_MUD_HOUR);
					}
						
					break;
				}
				case 'i': {	// char.i*
					if (!str_cmp(field, "id")) {
						snprintf(str, slen, "%d", char_script_id(c));
					}
					else if (!str_cmp(field, "is_name")) {
						if (subfield && *subfield && match_char_name(NULL, c, subfield, NOBITS, NULL)) {
							snprintf(str, slen, "1");
						}
						else {
							snprintf(str, slen, "0");
						}
					}

					/* new check for pc/npc status */
					else if (!str_cmp(field, "is_pc")) {
						snprintf(str, slen, IS_NPC(c) ? "0" : "1");
					}
					else if (!str_cmp(field, "is_npc")) {
						snprintf(str, slen, IS_NPC(c) ? "1" : "0");
					}

					else if (!str_cmp(field, "inventory")) {
						if(subfield && *subfield) {
							DL_FOREACH2(c->carrying, obj, next_content) {
								if(GET_OBJ_VNUM(obj)==atoi(subfield)) {
									snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id(obj)); /* arg given, found */
									return;
								}
							}
							if (!obj)
								strcpy(str, ""); /* arg given, not found */
						}
						else { /* no arg given */
							if (c->carrying) {
								snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id(c->carrying));
							}
							else {
								strcpy(str, "");
							}
						}
					}
					
					else if (!str_cmp(field, "is_ally")) {
						if (subfield && *subfield) {
							char_data *targ = (*subfield == UID_CHAR) ? get_char(subfield) : get_char_vis(c, subfield, NULL, FIND_CHAR_WORLD);
							snprintf(str, slen, "%d", (targ && is_fight_ally(c, targ)) ? 1 : 0);
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "is_enemy")) {
						if (subfield && *subfield) {
							char_data *targ = (*subfield == UID_CHAR) ? get_char(subfield) : get_char_vis(c, subfield, NULL, FIND_CHAR_WORLD);
							snprintf(str, slen, "%d", (targ && is_fight_enemy(c, targ)) ? 1 : 0);
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					
					else if (!str_cmp(field, "is_flying")) {
						snprintf(str, slen, EFFECTIVELY_FLYING(c) ? "1" : "0");
					}
					
					else if (!str_cmp(field, "is_god")) {
						snprintf(str, slen, "%d", IS_GOD(c) ? 1 : 0);
					}

					else if (!str_cmp(field, "is_hostile")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							if (!str_cmp("on", subfield) || *subfield == '1') {
								add_cooldown(c, COOLDOWN_HOSTILE_FLAG, config_get_int("hostile_flag_time") * SECS_PER_REAL_MIN);
							}
							else if (!str_cmp("off", subfield) || *subfield == '0') {
								remove_cooldown_by_type(c, COOLDOWN_HOSTILE_FLAG);
							}
						}
						if (IS_HOSTILE(c))
							snprintf(str, slen, "1");
						else
							snprintf(str, slen, "0");
					}
					
					else if (!str_cmp(field, "is_ignoring")) {
						char_data *targ;
						if (subfield && *subfield && ((targ = get_char(subfield))) && is_ignoring(c, targ)) {
							strcpy(str, "1");
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "is_immortal")) {
						snprintf(str, slen, "%d", IS_IMMORTAL(c) ? 1 : 0);
					}
					else if (!str_cmp(field, "is_tagged_by")) {
						char_data *targ;
						if (subfield && *subfield && (targ = get_char(subfield))) {
							if (IS_NPC(targ) && GET_LEADER(targ)) {
								targ = GET_LEADER(targ);
							}
							snprintf(str, slen, "%d", IS_TAGGED_BY(c, targ) ? 1 : 0);
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "is_scaled")) {
						snprintf(str, slen, "%d", (IS_NPC(c) && GET_CURRENT_SCALE_LEVEL(c) > 0) ? 1 : 0);
					}
					else if (!str_cmp(field, "is_waterwalking")) {
						snprintf(str, slen, HAS_WATERWALKING(c) ? "1" : "0");
					}

					else if (!str_cmp(field, "int") || !str_cmp(field, "intelligence")) {
						snprintf(str, slen, "%d", GET_INTELLIGENCE(c));
					}
					break;
				}
				case 'l': {	// char.l*
					if (!str_cmp(field, "lastname")) {
						snprintf(str, slen, "%s", IS_NPC(c) ? "" : GET_CURRENT_LASTNAME(c)); 
					}
					else if (!str_cmp(field, "last_move_dir")) {
						snprintf(str, slen, "%s", GET_LAST_DIR(c) != NO_DIR ? dirs[GET_LAST_DIR(c)] : "");
					}
					else if (!str_cmp(field, "leader")) {
						if (!GET_LEADER(c))
							strcpy(str, "");
						else
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(GET_LEADER(c)));
					}
					else if (!str_cmp(field, "learned")) {
						if (subfield && *subfield && isdigit(*subfield) && has_learned_craft(c, atoi(subfield))) {
							strcpy(str, "1");
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "level")) {
						snprintf(str, slen, "%d", get_approximate_level(c)); 
					}
					else if (!str_cmp(field, "link_instance")) {
						struct instance_data *inst;
						if (IS_NPC(c)) {
							if (MOB_INSTANCE_ID(c) != NOTHING) {
								// remove from old instance?
								subtract_instance_mob(real_instance(MOB_INSTANCE_ID(c)), GET_MOB_VNUM(c));
								MOB_INSTANCE_ID(c) = NOTHING;
							}
							if ((inst = find_instance_by_room(IN_ROOM(c), FALSE, TRUE))) {
								MOB_INSTANCE_ID(c) = inst->id;
								add_instance_mob(inst, GET_MOB_VNUM(c));
							}
							
							request_char_save_in_world(c);
						}
						*str = '\0';
					}
					else if (!str_cmp(field, "link_adventure_summon")) {
						*str = '\0';
						if (!IS_NPC(c) && PLR_FLAGGED(c, PLR_ADVENTURE_SUMMONED)) {
							struct instance_data *inst = find_instance_by_room(IN_ROOM(c), FALSE, FALSE);
							GET_ADVENTURE_SUMMON_INSTANCE_ID(c) = inst ? INST_ID(inst) : NOTHING;
						}
					}
					else if (!str_cmp(field, "linked_to_instance")) {
						struct instance_data *inst;
						room_data *froom = (subfield && *subfield) ? get_room(IN_ROOM(c), subfield) : IN_ROOM(c);
						if (IS_NPC(c) && froom && (inst = find_instance_by_room(froom, FALSE, TRUE)) && MOB_INSTANCE_ID(c) == inst->id) {
							snprintf(str, slen, "1");
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "longdesc")) {
						snprintf(str, slen, "%s", GET_LONG_DESC(c));
						// trim trailing CRLFs
						while (strchr("\r\n", str[strlen(str)-1])) {
							str[strlen(str)-1] = '\0';
						}
					}
					break;
				}
				case 'm': {	// char.m*
					if (!str_cmp(field, "mark_adventure_summoned_from")) {
						*str = '\0';
						if (!IS_NPC(c)) {
							struct map_data *map;
							if (!PLR_FLAGGED(c, PLR_ADVENTURE_SUMMONED)) {
								// ensure this is not set
								GET_ADVENTURE_SUMMON_INSTANCE_ID(c) = NOTHING;
							}
							SET_BIT(PLR_FLAGS(c), PLR_ADVENTURE_SUMMONED);
							GET_ADVENTURE_SUMMON_RETURN_LOCATION(c) = GET_ROOM_VNUM(IN_ROOM(c));
							map = GET_MAP_LOC(IN_ROOM(c));
							GET_ADVENTURE_SUMMON_RETURN_MAP(c) = map ? map->vnum : NOWHERE;
						}
					}
					else if (!str_cmp(field, "maxcarrying")) {
						snprintf(str, slen, "%d", CAN_CARRY_N(c));
					}
					else if (!str_cmp(field, "maxhitp") || !str_cmp(field, "maxhealth"))
						snprintf(str, slen, "%d", GET_MAX_HEALTH(c));

					else if (!str_cmp(field, "maxblood"))
						snprintf(str, slen, "%d", GET_MAX_BLOOD(c));

					else if (!str_cmp(field, "mana")) {
						int amt;
						if (subfield && *subfield && (amt = atoi(subfield))) {
							set_mana(c, GET_MANA(c) + amt);
						}
						snprintf(str, slen, "%d", GET_MANA(c));
					}

					else if (!str_cmp(field, "maxmana"))
						snprintf(str, slen, "%d", GET_MAX_MANA(c));

					else if (!str_cmp(field, "move")) {
						int amt;
						if (subfield && *subfield && (amt = atoi(subfield))) {
							set_move(c, GET_MOVE(c) + amt);
						}
						snprintf(str, slen, "%d", GET_MOVE(c));
					}

					else if (!str_cmp(field, "maxmove"))
						snprintf(str, slen, "%d", GET_MAX_MOVE(c));

					else if (!str_cmp(field, "master")) {
						// for historical reasons .master is here -- but please use .leader
						if (!GET_LEADER(c))
							strcpy(str, "");
						else
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(GET_LEADER(c)));
					}
					else if (!str_cmp(field, "mob_flagged")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, action_bits, FALSE);
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", MOB_FLAGGED(c, BIT(pos)) ? 1 : 0);
							}
							else {
								sprintbit(IS_NPC(c) ? MOB_FLAGS(c) : NOBITS, action_bits, buf, TRUE);
								snprintf(str, slen, "%s", buf);
							}
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "morph")) {
						snprintf(str, slen, "%d", IS_MORPHED(c) ? MORPH_VNUM(GET_MORPH(c)) : 0);
					}
					else if (!str_cmp(field, "movetype")) {
						snprintf(str, slen, "%s", IS_NPC(c) ? mob_move_types[(int) MOB_MOVE_TYPE(c)] : (EFFECTIVELY_FLYING(c) ? "flies" : "walks"));
					}
				
					break;
				}
				case 'n': {	// char.n*
					if (!str_cmp(field, "name")) {
						snprintf(str, slen, "%s", PERS(c, c, FALSE));
					}
					else if (!str_cmp(field, "namelist")) {
						if (!IS_NPC(c)) {
							snprintf(str, slen, "%d", NOTHING);
						}
						else if (!subfield || !*subfield || !isdigit(*subfield)) {
							snprintf(str, slen, "%d", MOB_DYNAMIC_NAME(c));
						}
						else {
							MOB_DYNAMIC_NAME(c) = atoi(subfield);
							setup_generic_npc(c, GET_LOYALTY(c), MOB_DYNAMIC_NAME(c), MOB_DYNAMIC_SEX(c));
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "next_follower")) {
						struct follow_type *fol;
						bool fnd = FALSE, success = FALSE;
						
						if (GET_LEADER(c)) {
							LL_FOREACH(GET_LEADER(c)->followers, fol) {
								if (fnd && fol->follower) {	// already found c...
									snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(fol->follower));
									success = TRUE;
								}
								else if (fol->follower == c) {
									fnd = TRUE;	// found my position in the list
								}
							}
							if (!success) {
								*str = '\0';	// no next follower
							}
						}
						else {
							*str = '\0';	// no leader == no next follower
						}
					}
					else if (!str_cmp(field, "next_in_room")) {
						char_data *temp_ch = NULL;
						
						// attempt to prevent extracted people from showing in lists
						DL_FOREACH2(c->next_in_room, temp_ch, next_in_room) {
							if (!SCRIPT_SHOULD_SKIP_CHAR(temp_ch)) {
								break;
							}
						}
						
						if (temp_ch) {
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(temp_ch));
						}
						else {
							strcpy(str, "");
						}
					}
					else if (!str_cmp(field, "nohassle")) {
						snprintf(str, slen,"%d", NOHASSLE(c) ? 1 : 0);
					}
					else if (!str_cmp(field, "noskill")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							skill_data *sk;							
							if ((sk = find_skill(subfield)) && !noskill_ok(c, SKILL_VNUM(sk))) {
								snprintf(str, slen, "1");
							}
						}
						// all other cases...
						if (*str != '1') {							
							snprintf(str, slen, "0");
						}
					}
					break;
				}
				case 'o': {	// char.o*
					if (!str_cmp(field, "obj_target")) {
						obj_data *targ;
						if (subfield && generic_find(subfield, NULL, FIND_OBJ_INV | FIND_OBJ_ROOM, c, NULL, &targ, NULL)) {
							snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id(targ));
						}
						else {
							strcpy(str, "");
						}
					}
					else if (!str_cmp(field, "obj_target_inv")) {
						obj_data *targ;
						*str = '\0';	// default to no-target
						if (subfield && *subfield) {
							if ((targ = get_obj_in_list_vis(c, subfield, NULL, c->carrying))) {
								snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id(targ));
							}
						}
					}
					else if (!str_cmp(field, "on_quest")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							any_vnum vnum = atoi(subfield);
							if (!IS_NPC(c) && is_on_quest(c, vnum)) {
								strcpy(str, "1");
							}
							else {
								strcpy(str, "0");
							}
						}
					}
					break;
				}
				case 'p': {	// char.p*
					if (!str_cmp(field, "parse_dir")) {
						if (subfield && *subfield) {
							int dir;
							if ((dir = parse_direction(c, subfield)) != NO_DIR) {
								snprintf(str, slen, "%s", dirs[dir]);	// real dir
							}
							else {
								// bad direction -- empty result
								*str = '\0';
							}
						}
						else {	// missing arg
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "pc_name")) {
						snprintf(str, slen, "%s", GET_PC_NAME(c));
					}
					else if (!str_cmp(field, "plr_flagged")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, player_bits, FALSE);
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", PLR_FLAGGED(c, BIT(pos)) ? 1 : 0);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							sprintbit(!IS_NPC(c) ? PLR_FLAGS(c) : NOBITS, player_bits, buf, TRUE);
							snprintf(str, slen, "%s", buf);
						}
					}
					else if (!str_cmp(field, "poison_immunity")) {
						if (has_player_tech(c, PTECH_NO_POISON)) {
							snprintf(str, slen, "1");
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "position")) {
						snprintf(str, slen, "%s", position_types[(int) GET_POS(c)]);
					}
					
					break;
				}
				case 'q': {	// char.q*
					if (!str_cmp(field, "quest_finished")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							any_vnum vnum = atoi(subfield);
							struct player_quest *pq;
							int complete, total;
							if (!IS_NPC(c) && (pq = is_on_quest(c, vnum))) {
								count_quest_tasks(pq->tracker, &complete, &total);
								if (complete >= total) {
									strcpy(str, "1");
								}
								else {
									strcpy(str, "0");
								}
							}
							else {
								strcpy(str, "0");
							}
						}
					}
					else if (!str_cmp(field, "quest_triggered")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							any_vnum vnum = atoi(subfield);
							struct player_quest *pq;
							struct req_data *task;
							int count = 0;
							
							if (!IS_NPC(c) && (pq = is_on_quest(c, vnum))) {
								LL_FOREACH(pq->tracker, task) {
									if (task->type == REQ_TRIGGERED) {
										count += task->current;
									}
								}
								snprintf(str, slen, "%d", count);
							}
							else {
								strcpy(str, "0");
							}
						}
					}
					break;
				}
				case 'r': {	// char.r*
					if (!str_cmp(field, "real_name")) {
						snprintf(str, slen, "%s", PERS(c, c, TRUE));
					}
					else if (!str_cmp(field, "remove_companion")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield)) {
							remove_companion(c, atoi(subfield));
						}
						
						strcpy(str, "0");
					}
					else if (!str_cmp(field, "remove_lastname")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							remove_lastname(c, subfield);
						}
						strcpy(str, "0");
					}
					else if (!str_cmp(field, "remove_learned")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							remove_learned_craft(c, atoi(subfield));
						}
						
						strcpy(str, "0");
					}
					else if (!str_cmp(field, "remove_minipet")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield)) {
							remove_minipet(c, atoi(subfield));
						}
						
						strcpy(str, "0");
					}
					else if (!str_cmp(field, "remove_mount")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield)) {
							struct mount_data *mount;
							
							// unmount first?
							if (GET_MOUNT_VNUM(c) == atoi(subfield)) {
								if (IS_RIDING(c)) {
									ACMD(do_dismount);
									do_dismount(c, "", 0, 0);
								}
								GET_MOUNT_VNUM(c) = NOTHING;
							}
							
							// remove data
							if ((mount = find_mount_data(c, atoi(subfield)))) {
								HASH_DEL(GET_MOUNT_LIST(c), mount);
								free(mount);
							}
						}
						
						strcpy(str, "0");
					}
					else if (!str_cmp(field, "remove_mob_flag")) {
						if (subfield && *subfield && IS_NPC(c)) {
							bitvector_t pos = search_block(subfield, action_bits, FALSE);
							if (pos != NOTHING) {
								remove_mob_flags(c, BIT(pos));
							}
							else {
								script_log("Trigger: %s, VNum %d, unknown mob flag: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
							}
						}
						snprintf(str, slen, "0");
					}
					else if (!str_cmp(field, "remove_unlocked_archetype")) {
						if (!IS_NPC(c) && subfield && *subfield && isdigit(*subfield)) {
							remove_unlocked_archetype(c, atoi(subfield));
						}
						strcpy(str, "0");
					}
					else if (!str_cmp(field, "resist_magical")) {
						snprintf(str, slen, "%d", GET_RESIST_MAGICAL(c));
					}
					else if (!str_cmp(field, "resist_physical")) {
						snprintf(str, slen, "%d", GET_RESIST_PHYSICAL(c));
					}
					else if (!str_cmp(field, "resist_poison")) {
						if (has_player_tech(c, PTECH_RESIST_POISON)) {
							snprintf(str, slen, "1");
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "room")) {
						snprintf(str, slen, "%c%d",UID_CHAR, GET_ROOM_VNUM(IN_ROOM(c)) + ROOM_ID_BASE);
					}

					else if (!str_cmp(field, "riding")) {
						if (IS_RIDING(c))
							snprintf(str, slen, "1");
						else
							snprintf(str, slen, "0");
					}
					break;
				}
				case 's': {	// char.s*
					if (!str_cmp(field, "set_cooldown")) {
						if (subfield && *subfield) {
							char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
							generic_data *gen;
							int val;
							
							comma_args(subfield, arg1, arg2);
							
							if (!*arg1 || !*arg2 || !isdigit(*arg1) || !isdigit(*arg2) || !(gen = find_generic(atoi(arg1), GENERIC_COOLDOWN))) {
								script_log("Trigger: %s, VNum %d. bad arguments to set_cooldown(%s, %s)", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), arg1, arg2);
								strcpy(str, "0");
							}
							else {
								val = atoi(arg2);
								if (val > 0) {
									add_cooldown(c, GEN_VNUM(gen), val);
								}
								else {
									remove_cooldown_by_type(c, GEN_VNUM(gen));
								}
								
								// success
								strcpy(str, "1");
							}
						}
					}
					else if (!str_cmp(field, "set_reputation")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							// %actor.has_reputation(vnum, level)%
							char arg1[256], arg2[256];
							faction_data *fct;
							int rep;
							
							comma_args(subfield, arg1, arg2);
							if (*arg1 && *arg2 && (fct = find_faction(arg1)) && (rep = get_reputation_by_name(arg2)) != NOTHING) {
								set_reputation(c, FCT_VNUM(fct), rep);
							}
						}
						strcpy(str, "1");
					}
					else if (!str_cmp(field, "sex"))
						snprintf(str, slen, "%s", genders[(int)GET_SEX(c)]);

					else if (!str_cmp(field, "str") || !str_cmp(field, "strength"))
						snprintf(str, slen, "%d", GET_STRENGTH(c));

					else if (!str_cmp(field, "skill")) {
						skill_data *sk = find_skill(subfield);
						if (sk && !IS_NPC(c)) {
							snprintf(str, slen, "%d", get_skill_level(c, SKILL_VNUM(sk)));
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					
					else if (!str_cmp(field, "set_skill")) {
						if (subfield && *subfield && !IS_NPC(c)) {
							// %actor.set_skill(skill, number)%
							char arg1[256], arg2[256];
							skill_data *sk;
							int sk_lev = 0;
							
							comma_args(subfield, arg1, arg2);
							if (*arg1 && *arg2 && (sk = find_skill(arg1)) && (sk_lev = atoi(arg2)) >= 0 && sk_lev <= MAX_SKILL_CAP && (sk_lev < get_skill_level(c, SKILL_VNUM(sk)) || noskill_ok(c, SKILL_VNUM(sk)))) {
								// TODO skill cap checking! need a f() like can_set_skill_to(c, sk, lev)
								set_skill(c, SKILL_VNUM(sk), sk_lev);
								snprintf(str, slen, "1");
							}
						}
						// all other cases...
						if (*str != '1') {							
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "speaking")) {
						snprintf(str, slen, "%d", IS_NPC(c) ? (MOB_LANGUAGE(c) != NOTHING ? MOB_LANGUAGE(c) : config_get_int("default_language_vnum")) : GET_SPEAKING(c));
					}
					
					break;
				}
				case 't': {	// char.t*
					if (!str_cmp(field, "temperature")) {
						if (subfield && *subfield) {
							if (!str_cmp(subfield, "base")) {
								snprintf(str, slen, "%d", IS_NPC(c) ? 0 : GET_TEMPERATURE(c));
							}
							else if (isdigit(*subfield) || *subfield == '+' || *subfield == '-') {
								int new_temp = atoi(subfield);
								new_temp = MAX(-100, MIN(100, new_temp));
								if (IS_NPC(c)) {
									GET_TEMPERATURE(c) = new_temp;
								}
								// and report new relative temp
								snprintf(str, slen, "%d", get_relative_temperature(c));
							}
							else {
								snprintf(str, slen, "%d", get_relative_temperature(c));
							}
						}
						else {
							snprintf(str, slen, "%d", get_relative_temperature(c));
						}
					}
					else if (!str_cmp(field, "thirst")) {
						if (subfield && *subfield) {
							gain_condition(c, THIRST, atoi(subfield) * REAL_UPDATES_PER_MUD_HOUR);
						}
						snprintf(str, slen, "%d", GET_COND(c, THIRST) / REAL_UPDATES_PER_MUD_HOUR);
					}
					else if (!str_cmp(field, "tohit")) {
						snprintf(str, slen, "%d", get_to_hit(c, NULL, FALSE, FALSE));
					}
					else if (!str_cmp(field, "trigger_counterspell")) {
						// optional target
						char_data *trig_by = NULL;
						if (subfield && *subfield) {
							trig_by = (*subfield == UID_CHAR) ? get_char(subfield) : get_char_vis(c, subfield, NULL, FIND_CHAR_WORLD);
						}
						
						// trigger it with or without a valid target; that arg can be NULL
						if (trigger_counterspell(c, trig_by)) {
							snprintf(str, slen, "1");
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "tool")) {
						obj_data *tool;
						int tool_type;
						if (subfield && *subfield && (tool_type = search_block(subfield, tool_flags, FALSE)) != NOTHING && (tool = has_tool(c, BIT(tool_type)))) {
							snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id(tool));
						}
						else {
							strcpy(str, "");	// tool not found
						}
					}
					break;
				}
				case 'u': {	// char.u*
					if (!str_cmp(field, "unlink_instance")) {
						if (IS_NPC(c) && MOB_INSTANCE_ID(c) != NOTHING) {
							subtract_instance_mob(real_instance(MOB_INSTANCE_ID(c)), GET_MOB_VNUM(c));
							MOB_INSTANCE_ID(c) = NOTHING;
							request_char_save_in_world(c);
						}
						*str = '\0';
					}
					else if (!str_cmp(field, "unscale_and_reset")) {
						if (IS_NPC(c)) {
							check_reset_mob(c, TRUE);
						}
						* str = '\0';
					}
					break;
				}
				case 'v': {	// char.v*
					if (!str_cmp(field, "vampire")) {
						// subfield can change vampire status
						if (subfield && *subfield) {
							if ((!str_cmp("on", subfield) || *subfield == '1') && !IS_VAMPIRE(c)) {
								if (!IS_NPC(c)) {
									make_vampire(c, TRUE, NOTHING);
								}
								else {
									set_mob_flags(c, MOB_VAMPIRE);
								}
							}
							else if ((!str_cmp("off", subfield) || *subfield == '0') && IS_VAMPIRE(c)) {
								if (!IS_NPC(c)) {
									check_un_vampire(c, TRUE);
								}
								else {
									remove_mob_flags(c, MOB_VAMPIRE);
								}
							}
						}
						// echo whether or not they are a vampire
						snprintf(str, slen, "%d", IS_VAMPIRE(c) ? 1 : 0);
					}
					else if (!str_cmp(field, "veh_target")) {
						vehicle_data *targ;
						*str = '\0';	// default to no-target
						if (subfield && *subfield) {
							if ((targ = get_vehicle_in_room_vis(c, subfield, NULL))) {
								snprintf(str, slen, "%c%d", UID_CHAR, veh_script_id(targ));
							}
						}
					}
					else if (!str_cmp(field, "view_height")) {
						snprintf(str, slen, "%d", get_view_height(c, IN_ROOM(c)));
					}
					else if (!str_cmp(field, "vnum")) {
						if (IS_NPC(c))
							snprintf(str, slen, "%d", GET_MOB_VNUM(c));
						else
						/* 
						* for compatibility with unsigned indexes 
						* - this is deprecated - use %actor.is_pc% to check 
						* instead of %actor.vnum% == -1  --Welcor 09/03
						*/
							strcpy(str, "-1");
					}    
					
					else if (!str_cmp(field, "var")) {
						struct trig_var_data *remote_vd;
						char arg1[256], arg2[256];
						bool found = FALSE;

						comma_args(subfield, arg1, arg2);
						*str = '\0';

						if (*arg1 && SCRIPT(c)) {
							LL_FOREACH(SCRIPT(c)->global_vars, remote_vd) {
								if (!str_cmp(remote_vd->name, arg1)) {
									snprintf(str, slen, "%s", remote_vd->value);
									found = TRUE;
									break;
								}
							}
						}
						if (!found && *arg2) {
							snprintf(str, slen, "%s", arg2);
						}
					}
					else if (!str_cmp(field, "varexists")) {
						struct trig_var_data *remote_vd;
						snprintf(str, slen, "0");
						if (SCRIPT(c)) {
							for (remote_vd = SCRIPT(c)->global_vars; remote_vd; remote_vd = remote_vd->next) {
								if (!str_cmp(remote_vd->name, subfield)) break;
							}
							if (remote_vd)
								snprintf(str, slen, "1");
						}
					}

					break;
				}
				case 'w': {	// char.w*
					if (!str_cmp(field, "wit") || !str_cmp(field, "wits")) {
						snprintf(str, slen, "%d", GET_WITS(c));
					}
					break;
				}
			} /* switch *field */	

			if (*str == '\x1') { /* no match found in switch */
				if (SCRIPT(c)) {
					for (vd = (SCRIPT(c))->global_vars; vd; vd = vd->next)
						if (!str_cmp(vd->name, field))
							break;
					if (vd)
						snprintf(str, slen, "%s", vd->value);
					else {
						*str = '\0';
						script_log("Trigger: %s, VNum %d. unknown char field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), field);
					}
				}
				else {
					*str = '\0';
					script_log("Trigger: %s, VNum %d. unknown char field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), field);
				}
			}
		} /* if (c) ...*/

		else if (o) {
			if (text_processed(field, subfield, vd, str, slen, go, sc, trig, type))
				return;

			*str = '\x1';
			switch (LOWER(*field)) {
				case 'a': {
					if (!str_cmp(field, "add_wear")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, wear_bits, FALSE);
							if (pos != NOTHING) {
								SET_BIT(GET_OBJ_WEAR(o), BIT(pos));
								request_obj_save_in_world(o);
							}
							else {
								script_log("Trigger: %s, VNum %d, unknown wear flag: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
							}
						}
						snprintf(str, slen, "0");
					}
					else if (!str_cmp(field, "attack")) {
						if (IS_WEAPON(o) || IS_MISSILE_WEAPON(o)) {
							int type = IS_WEAPON(o) ? GET_WEAPON_TYPE(o) : GET_MISSILE_WEAPON_TYPE(o);
							attack_message_data *amd = real_attack_message(type);
							if (!subfield || !*subfield || !str_cmp(subfield, "0") || is_abbrev(subfield, "name")) {
								snprintf(str, slen, "%s", amd ? ATTACK_NAME(amd) : "");
							}
							else if (!str_cmp(subfield, "1") || is_abbrev(subfield, "first-person")) {
								snprintf(str, slen, "%s", amd ? ATTACK_FIRST_PERSON(amd) : "");
							}
							else if (!str_cmp(subfield, "3") || is_abbrev(subfield, "third-person")) {
								snprintf(str, slen, "%s", amd ? ATTACK_THIRD_PERSON(amd) : "");
							}
							else if (is_abbrev(subfield, "noun")) {
								snprintf(str, slen, "%s", amd ? ATTACK_NOUN(amd) : "");
							}
							else if (is_abbrev(subfield, "sharp")) {
								snprintf(str, slen, "%d", (amd && ATTACK_WEAPON_TYPE(amd) == WEAPON_SHARP) ? 1 : 0);
							}
							else if (is_abbrev(subfield, "blunt")) {
								snprintf(str, slen, "%d", (amd && ATTACK_WEAPON_TYPE(amd) == WEAPON_BLUNT) ? 1 : 0);
							}
							else if (is_abbrev(subfield, "magic")) {
								snprintf(str, slen, "%d", (amd && ATTACK_WEAPON_TYPE(amd) == WEAPON_MAGIC) ? 1 : 0);
							}
							else if (is_abbrev(subfield, "damage")) {
								snprintf(str, slen, "%s", damage_types[amd ? ATTACK_DAMAGE_TYPE(amd) : 0]);
							}
							else if (is_abbrev(subfield, "disarmable")) {
								snprintf(str, slen, "%d", (amd && ATTACK_FLAGGED(amd, AMDF_DISARMABLE)) ? 1 : 0);
							}
						}
						else {	// not a weapon
							*str = '\0';
						}
					}
					break;
				}
				case 'b': {	// obj.b*
					if (!str_cmp(field, "bind")) {
						if (subfield && *subfield) {
							if (!str_cmp(subfield, "none") || !str_cmp(subfield, "nobody")) {
								free_obj_binding(&OBJ_BOUND_TO(o));
								request_obj_save_in_world(o);
							}
							else {	// attempt to bind it
								char_data *targ = (*subfield == UID_CHAR) ? get_char(subfield) : get_char_by_obj(o, subfield);
								if (targ && !IS_NPC(targ) && OBJ_FLAGGED(o, OBJ_BIND_FLAGS)) {
									// unbind first
									free_obj_binding(&OBJ_BOUND_TO(o));
									bind_obj_to_player(o, targ);
									reduce_obj_binding(o, targ);
									request_obj_save_in_world(o);
								}
								else {	// wasn't targeting a person, try an obj
									obj_data *oarg = (*subfield == UID_CHAR) ? get_obj(subfield) : get_obj_by_obj(o, subfield);
									if (oarg) {
										free_obj_binding(&OBJ_BOUND_TO(o));	// unbind first
										OBJ_BOUND_TO(o) = copy_obj_bindings(OBJ_BOUND_TO(oarg));
										request_obj_save_in_world(o);
									}
								}
							}
						}
						
						// no result
						*str = '\0';
					}
					else if (!str_cmp(field, "bound_to")) {
						if (OBJ_BOUND_TO(o)) {
							player_index_data *index;
							struct obj_binding *bind;
							bool any = FALSE;
							
							*str = '\0';
							LL_FOREACH(OBJ_BOUND_TO(o), bind) {
								snprintf(str + strlen(str), slen - strlen(str), "%s%s", (any ? ", " : ""), (index = find_player_index_by_idnum(bind->idnum)) ? index->fullname : "<unknown>");
								any = TRUE;
							}
						}
						else {
							strcpy(str, "");
						}
					}
					break;
				}
				case 'c': {	// obj.c*
					if (!str_cmp(field, "can_wear")) {
						int pos;
						if (subfield && *subfield && (pos = search_block(subfield, wear_bits, FALSE)) != NOTHING) {
							snprintf(str, slen, "%d", CAN_WEAR(o, BIT(pos)) ? 1 : 0);
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "carried_by")) {
						if (o->carried_by)
							snprintf(str, slen,"%c%d",UID_CHAR, char_script_id(o->carried_by));
						else
							strcpy(str,"");
					}
					else if (!str_cmp(field, "component_type")) {
						snprintf(str, slen, "%s", GET_OBJ_COMPONENT(o) != NOTHING ? get_generic_name_by_vnum(GET_OBJ_COMPONENT(o)) : "");
					}
					else if (!str_cmp(field, "contents")) {
						if (o->contains)
							snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id(o->contains));
						else
							strcpy(str, "");
					}
					/* thanks to Jamie Nelson (Mordecai of 4 Dimensions MUD) */
					else if (!str_cmp(field, "count")) {
						int count = 0; 
						if (GET_OBJ_TYPE(o) == ITEM_CONTAINER) {
							obj_vnum snum = (subfield && is_number(subfield)) ? atoi(subfield) : NOTHING;
							obj_data *item;
							DL_FOREACH2(o->contains, item, next_content) {
								if (snum != NOTHING) {
									if (snum == GET_OBJ_VNUM(item))
										++count;
								}
								else {
									if (isname(subfield, item->name))
										++count;
								}
							}
						}
						snprintf(str, slen, "%d", count);
					}
					else if (!str_cmp(field, "custom")) {
						// default to empty
						*str = '\0';
						if (subfield && *subfield) {
							char arg1[256], arg2[256];
							char *msg;
							int ctype, pos = -1;
							
							comma_args(subfield, arg1, arg2);
							if (*arg1 && (ctype = search_block(arg1, obj_custom_types, FALSE)) != NOTHING) {
								if (*arg2) {	// optional message pos
									pos = atoi(arg2);
									msg = get_custom_message_pos(GET_OBJ_CUSTOM_MSGS(o), ctype, pos);
								}
								else {
									msg = get_custom_message(GET_OBJ_CUSTOM_MSGS(o), ctype);
								}
								
								// did we find one
								if (msg) {
									snprintf(str, slen, "%s", NULLSAFE(msg));
								}
							}
						}
					}
					break;
				}
				case 'e': {	// obj.e*
					if (!str_cmp(field, "empire")) {
						if (o->last_empire_id != NOTHING) {
							snprintf(str, slen, "%c%d", UID_CHAR, o->last_empire_id + EMPIRE_ID_BASE);
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "empty")) {
						*str = '\0';
						if (IS_CORPSE(o) || IS_CONTAINER(o)) {
							empty_obj_before_extract(o);
						}
						else if (IS_DRINK_CONTAINER(o)) {
							set_obj_val(o, VAL_DRINK_CONTAINER_CONTENTS, 0);
							set_obj_val(o, VAL_DRINK_CONTAINER_TYPE, 0);

							if (OBJ_FLAGGED(o, OBJ_SINGLE_USE)) {
								// single-use: extract it
								run_interactions(o->carried_by ? o->carried_by : o->worn_by, GET_OBJ_INTERACTIONS(o), INTERACT_CONSUMES_TO, IN_ROOM(o), NULL, o, NULL, consumes_or_decays_interact);
								empty_obj_before_extract(o);
								extract_obj(o);
							}
							else {
								// if it wasn't single-use, reset its timer to UNLIMITED since the timer refers to the contents
								GET_OBJ_TIMER(o) = UNLIMITED;
							}
						}
					}
					break;
				}
				case 'f': {	// obj.f*
					if (!str_cmp(field, "flag")) {
						if (subfield && *subfield) {
							int fl = search_block(subfield, extra_bits, FALSE);
							if (fl != NOTHING) {
								bool was_lit = LIGHT_IS_LIT(o);
								
								TOGGLE_BIT(GET_OBJ_EXTRA(o), BIT(fl));
								snprintf(str, slen, (OBJ_FLAGGED(o, BIT(fl)) ? "1" : "0"));
								
								if (was_lit != LIGHT_IS_LIT(o)) {
									if (was_lit) {
										apply_obj_light(o, FALSE);
									}
									else {
										apply_obj_light(o, TRUE);
									}
								}
								
								request_obj_save_in_world(o);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					break;
				}
				case 'h': {	// obj.h*
					/* thanks to Jamie Nelson (Mordecai of 4 Dimensions MUD) */
					if (!str_cmp(field, "has_in")) { 
						bool found = FALSE;
						if (GET_OBJ_TYPE(o) == ITEM_CONTAINER) {
							obj_data *item;
							obj_vnum snum = (subfield && is_number(subfield)) ? atoi(subfield) : NOTHING;
							
							DL_FOREACH2(o->contains, item, next_content) {
								if (snum != NOTHING) {
									if (snum == GET_OBJ_VNUM(item)) {
										found = TRUE;
										break;
									}
								}
								else {
									if (isname(subfield, item->name)) {
										found = TRUE;
										break;
									}
								}
							}
						}
						if (found)
							snprintf(str, slen, "1");
						else
							snprintf(str, slen, "0");
					}
					else if (!str_cmp(field, "has_trigger")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							snprintf(str, slen, "%d", has_trigger(SCRIPT(o), atoi(subfield)));
						}
						else {
							snprintf(str, slen, "0");	// no vnum provided
						}
					}
					break;
				}
				case 'i': {	// obj.i*
					if (!str_cmp(field, "id")) {
						snprintf(str, slen, "%d", obj_script_id(o));
					}
					else if (!str_cmp(field, "in_obj")) {
						if (o->in_obj) {
							snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id(o->in_obj));
						}
						else {
							strcpy(str, "");
						}
					}

					else if (!str_cmp(field, "is_flagged")) {
						if (subfield && *subfield) {
							int fl = search_block(subfield, extra_bits, FALSE);
							if (fl != NOTHING) {
								snprintf(str, slen, (OBJ_FLAGGED(o, BIT(fl)) ? "1" : "0"));
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							sprintbit(GET_OBJ_EXTRA(o), extra_bits, buf, TRUE);
							snprintf(str, slen, "%s", buf);
						}
					}
					
					else if (!str_cmp(field, "is_component")) {
						char arg[256], temp[256], remain[256];
						generic_data *cmp = NULL;
						bool any = FALSE;
						
						if (subfield && *subfield) {
							strcpy(remain, subfield);
							while (*remain) {
								comma_args(remain, arg, temp);
								strcpy(remain, temp);
								if ((cmp = find_generic_component(arg)) && is_component(o, cmp)) {
									any = TRUE;	// only need 1
									break;
								}
							}
							
							snprintf(str, slen, "%d", any ? 1 : 0);
						}
						else {	// no subfield: is a component at all?
							snprintf(str, slen, "%d", GET_OBJ_COMPONENT(o) != NOTHING ? 1 : 0);
						}
					}

					else if (!str_cmp(field, "is_inroom")) {
						if (IN_ROOM(o))
							snprintf(str, slen,"%c%d",UID_CHAR, GET_ROOM_VNUM(IN_ROOM(o)) + ROOM_ID_BASE); 
						else
							strcpy(str, "");
					}
					
					else if (!str_cmp(field, "is_name")) {
						if (subfield && *subfield && MATCH_ITEM_NAME(subfield, o)) {
							snprintf(str, slen, "1");
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					break;
				}
				case 'k': {	// obj.k*
					if (!str_cmp(field, "keywords")) {
						snprintf(str, slen, "%s", GET_OBJ_KEYWORDS(o));
					}
					break;
				}
				case 'l': {	// obj.l*
					if (!str_cmp(field, "level")) {
						snprintf(str, slen, "%d", GET_OBJ_CURRENT_SCALE_LEVEL(o));
					}
					else if (!str_cmp(field, "longdesc")) {
						snprintf(str, slen, "%s", GET_OBJ_LONG_DESC(o));
					}
					break;
				}
				case 'm': {	// obj.m*
					if (!str_cmp(field, "material")) {
						snprintf(str, slen, "%s", materials[GET_OBJ_MATERIAL(o)].name);
					}
					break;
				}
				case 'n': {	// obj.n*
					if (!str_cmp(field, "name"))
						snprintf(str, slen, "%s",  o->name);

					else if (!str_cmp(field, "next_in_list")) {
						if (o->next_content)
							snprintf(str, slen,"%c%d",UID_CHAR, obj_script_id(o->next_content));
						else
							strcpy(str,"");
					}
					break;
				}
				case 'q': {	// obj.q*
					if (!str_cmp(field, "quest")) {
						snprintf(str, slen, "%d", GET_OBJ_REQUIRES_QUEST(o) > 0 ? GET_OBJ_REQUIRES_QUEST(o) : 0);
					}
					break;
				}
				case 'r': {	// obj.r*
					if (!str_cmp(field, "remove_wear")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, wear_bits, FALSE);
							if (pos != NOTHING) {
								REMOVE_BIT(GET_OBJ_WEAR(o), BIT(pos));
								request_obj_save_in_world(o);
							}
							else {
								script_log("Trigger: %s, VNum %d, unknown wear flag: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
							}
						}
						snprintf(str, slen, "0");
					}
					else if (!str_cmp(field, "room")) {
						if (obj_room(o))
							snprintf(str, slen,"%c%d",UID_CHAR, GET_ROOM_VNUM(obj_room(o)) + ROOM_ID_BASE);
						else
							strcpy(str, "");
					}
					break;
				}
				case 's': {	// obj.s*
					if (!str_cmp(field, "shortdesc")) {
						snprintf(str, slen, "%s",  GET_OBJ_SHORT_DESC(o));
					}
					else if (!str_cmp(field, "size")) {
						snprintf(str, slen, "%d", obj_carry_size(o));
					}
					break;
				}
				case 't': {	// obj.t*
					if (!str_cmp(field, "type")) {
						sprinttype(GET_OBJ_TYPE(o), item_types, str, slen, "OTHER");
					}
					else if (!str_cmp(field, "timer"))
						snprintf(str, slen, "%d", GET_OBJ_TIMER(o));
					else if (!str_cmp(field, "tool")) {
						if (GET_OBJ_TOOL_FLAGS(o)) {
							sprintbit(GET_OBJ_TOOL_FLAGS(o), tool_flags, str, TRUE);
						}
						else {	// not a tool
							strcpy(str, "");
						}
					}
					break;
				}
				case 'u': {	// obj.u*
					if (!str_cmp(field, "used_lighter")) {
						char_data *person = NULL;
						
						if (subfield && *subfield) {	// optional person
							person = get_char_by_obj(o, subfield);
						}
						
						// returns 1 if obj is purged
						snprintf(str, slen, "%d", used_lighter(person, o) ? 1 : 0);
					}
					break;
				}
				case 'v': {	// obj.v*
						if (!str_cmp(field, "var")) {
							struct trig_var_data *remote_vd;
							char arg1[256], arg2[256];
							bool found = FALSE;

							comma_args(subfield, arg1, arg2);
							*str = '\0';

							if (*arg1 && SCRIPT(o)) {
								LL_FOREACH(SCRIPT(o)->global_vars, remote_vd) {
									if (!str_cmp(remote_vd->name, arg1)) {
										snprintf(str, slen, "%s", remote_vd->value);
										found = TRUE;
										break;
									}
								}
							}
							if (!found && *arg2) {
								snprintf(str, slen, "%s", arg2);
							}
						}
					else if (!str_cmp(field, "varexists")) {
						struct trig_var_data *remote_vd;
						snprintf(str, slen, "0");
						if (SCRIPT(o)) {
							for (remote_vd = SCRIPT(o)->global_vars; remote_vd; remote_vd = remote_vd->next) {
								if (!str_cmp(remote_vd->name, subfield)) {
									break;
								}
							}
							if (remote_vd) {
								snprintf(str, slen, "1");
							}
						}
					}
					else if (!str_cmp(field, "vnum"))
						snprintf(str, slen, "%d", GET_OBJ_VNUM(o));
					else if (!str_cmp(field, "val0")) {
						if (subfield && is_number(subfield)) {
							// can change the lights
							bool was_lit = LIGHT_IS_LIT(o);
							
							set_obj_val(o, 0, atoi(subfield));
							
							// check lights
							if (was_lit != LIGHT_IS_LIT(o)) {
								if (was_lit) {
									apply_obj_light(o, FALSE);
								}
								else {
									apply_obj_light(o, TRUE);
								}
							}
						}
						snprintf(str, slen, "%d", GET_OBJ_VAL(o, 0));
					}
					else if (!str_cmp(field, "val1")) {
						if (subfield && is_number(subfield)) {
							// can change the lights
							bool was_lit = LIGHT_IS_LIT(o);
							
							set_obj_val(o, 1, atoi(subfield));
							
							// check lights
							if (was_lit != LIGHT_IS_LIT(o)) {
								if (was_lit) {
									apply_obj_light(o, FALSE);
								}
								else {
									apply_obj_light(o, TRUE);
								}
							}
							if (IS_LIGHT(o)) {
								// in case
								schedule_obj_timer_update(o, FALSE);
							}
						}
						snprintf(str, slen, "%d", GET_OBJ_VAL(o, 1));
					}
					else if (!str_cmp(field, "val2")) {
						if (subfield && is_number(subfield)) {
							int old_val = GET_OBJ_VAL(o, 2);
							
							set_obj_val(o, 2, atoi(subfield));
							
							// things that might need to update:
							if (GET_OBJ_VAL(o, 2) != old_val) {
								// ... none so far
							}
						}
						snprintf(str, slen, "%d", GET_OBJ_VAL(o, 2));
					}
					break;
				}
				case 'w': {	// obj.w*
					if (!str_cmp(field, "wearable")) {
						snprintf(str, slen, "%d", (GET_OBJ_WEAR(o) & ~ITEM_WEAR_TAKE) ? 1 : 0);
					}
					else if (!str_cmp(field, "worn_by")) {
						if (o->worn_by)
							snprintf(str, slen,"%c%d",UID_CHAR, char_script_id(o->worn_by));
						else
							strcpy(str,"");
					}
					break;
				}
			} /* switch *field */


			if (*str == '\x1') { /* no match in switch */
				if (SCRIPT(o)) { /* check for global var */
					for (vd = (SCRIPT(o))->global_vars; vd; vd = vd->next)
						if (!str_cmp(vd->name, field))
							break;
					if (vd)
						snprintf(str, slen, "%s", vd->value);
					else {
						*str = '\0';
						script_log("Trigger: %s, VNum %d, type: %d. unknown object field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
					}
				}
				else {
					*str = '\0';
					script_log("Trigger: %s, VNum %d, type: %d. unknown object field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
				}
			}
		} /* if (o) ... */

		else if (r) {
			if (text_processed(field, subfield, vd, str, slen, go, sc, trig, type))
				return;
				
			*str = '\x1';
			switch (LOWER(*field)) {
				case 'a': {	// room.a*
					if (!str_cmp(field, "aff_flagged")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, room_aff_bits, FALSE);
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", ROOM_AFF_FLAGGED(r, BIT(pos)) ? 1 : 0);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							sprintbit(ROOM_AFF_FLAGS(r), room_aff_bits, buf, TRUE);
							snprintf(str, slen, "%s", buf);
						}
					}
					else if (!str_cmp(field, "aft")) {
						direction_vars(r, AFT, subfield, str, slen);
					}
					break;
				}
				case 'b': {	// room.b*
					if (!str_cmp(field, "bld_dir")) {
						int dir;
						if (subfield && *subfield && ((dir = search_block(subfield, dirs, FALSE)) != NO_DIR || (dir = search_block(subfield, alt_dirs, FALSE)) != NO_DIR)) {
							room_data *home = HOME_ROOM(r);
							if (GET_BUILDING(home) && BUILDING_ENTRANCE(home) != NO_DIR) {
								// adjust for dir
								dir = confused_dirs[rev_dir[BUILDING_ENTRANCE(home)]][0][dir];
							}
							snprintf(str, slen, "%s", dirs[dir]);
						}
						else {
							// no dir or not a valid dir
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "bld_flagged")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, bld_flags, FALSE);
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", ROOM_BLD_FLAGGED(r, BIT(pos)) ? 1 : 0);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							sprintbit(GET_BUILDING(r) ? GET_BLD_FLAGS(GET_BUILDING(r)) : NOBITS, bld_flags, buf, TRUE);
							snprintf(str, slen, "%s", buf);
						}
					}
					else if (!str_cmp(field, "building")) {
						if (GET_BUILDING(r)) {
							snprintf(str, slen, "%s", GET_BLD_NAME(GET_BUILDING(r)));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "building_vnum")) {
						if (GET_BUILDING(r)) {
							snprintf(str, slen, "%d", GET_BLD_VNUM(GET_BUILDING(r)));
						}
						else {
							*str = '\0';
						}
					}
					break;
				}
				case 'c': {	// room.c*
					if (!str_cmp(field, "can_build")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, bld_on_flags, FALSE);
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", IS_SET(GET_SECT_BUILD_FLAGS(SECT(r)), BIT(pos)) ? 1 : 0);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "complete")) {
						snprintf(str, slen, "%d", IS_COMPLETE(r) ? 1 : 0);
					}
					else if (!str_cmp(field, "contents")) {
						if (subfield && *subfield) {
							DL_FOREACH2(ROOM_CONTENTS(r), obj, next_content) {
								if (GET_OBJ_VNUM(obj) == atoi(subfield)) {
									/* arg given, found */
									snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id(obj)); 
									return;
								}
							}
							if (!obj)
								strcpy(str, ""); /* arg given, not found */
						}
						else { /* no arg given */
							if (ROOM_CONTENTS(r)) {
								snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id(ROOM_CONTENTS(r)));
							}
							else {
								strcpy(str, "");
							}
						}
					}
					else if (!str_cmp(field, "coords")) {
						if (GET_MAP_LOC(r) && !NO_LOCATION(r)) {
							snprintf(str, slen, "(%d, %d)", MAP_X_COORD(GET_MAP_LOC(r)->vnum), MAP_Y_COORD(GET_MAP_LOC(r)->vnum));
						}
						else {
							snprintf(str, slen, "(%s, %s)", "???", "???");
						}
					}
					else if (!str_cmp(field, "crop")) {
						crop_data *cp;
						if ((cp = ROOM_CROP(r))) {
							snprintf(str, slen, "%s", GET_CROP_NAME(cp));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "crop_flagged")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, crop_flags, FALSE);
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", ROOM_CROP_FLAGGED(r, BIT(pos)) ? 1 : 0);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							sprintbit((ROOM_SECT_FLAGGED(r, SECTF_HAS_CROP_DATA) && ROOM_CROP(r)) ? GET_CROP_FLAGS(ROOM_CROP(r)) : NOBITS, crop_flags, buf, TRUE);
							snprintf(str, slen, "%s", buf);
						}
					}
					else if (!str_cmp(field, "crop_vnum")) {
						crop_data *cp;
						if ((cp = ROOM_CROP(r))) {
							snprintf(str, slen, "%d", GET_CROP_VNUM(cp));
						}
						else {
							*str = '\0';
						}
					}
					break;
				}
				case 'd': {	// room.d*
					if (!str_cmp(field, "dedicated_to")) {
						player_index_data *index;
						if (ROOM_PATRON(r) && (index = find_player_index_by_idnum(ROOM_PATRON(r)))) {
							snprintf(str, slen, "%s", index->fullname);
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "depletion")) {
						// usage: %room.depletion(TYPE)% to get
						//        %room.depletion(TYPE,AMOUNT)% to set
						char arg1[256], arg2[256];
						int type;
						comma_args(subfield, arg1, arg2);
						
						if (!*arg1 || (type = search_block(arg1, depletion_types, FALSE)) == NOTHING) {
							// missing type?
							*str = '\0';
						}
						else if (!*arg2 || !isdigit(*arg2)) {
							// just getting
							snprintf(str, slen, "%d", get_depletion(r, type));
						}
						else {
							// setting
							set_room_depletion(r, type, atoi(arg2));
							snprintf(str, slen, "%d", get_depletion(r, type));
						}
					}
					else if (!str_cmp(field, "direction")) {
						room_data *targ;
						int dir;
				
						if (subfield && *subfield && (targ = get_room(r, subfield)) && (dir = get_direction_to(r, targ)) != NO_DIR) {
							snprintf(str, slen, "%s", dirs[dir]);
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "distance")) {
						room_data *targ;
						if (subfield && *subfield && (targ = get_room(r, subfield))) {
							snprintf(str, slen, "%d", compute_distance(r, targ));
						}
						else {
							snprintf(str, slen, "%d", MAP_SIZE);
						}
					}
					else if (!str_cmp(field, "down")) {
						direction_vars(r, DOWN, subfield, str, slen);
					}
					break; 
				}
				case 'e': {	// room.e*
					if (!str_cmp(field, "east")) {
						direction_vars(r, EAST, subfield, str, slen);
					}
					else if (!str_cmp(field, "empire_adjective")) {
						if (ROOM_OWNER(r)) {
							snprintf(str, slen, "%s", EMPIRE_ADJECTIVE(ROOM_OWNER(r)));
						}
						else {
							strcpy(str, "");
						}
					}
					else if (!str_cmp(field, "empire")) {
						if (ROOM_OWNER(r)) {
							snprintf(str, slen, "%c%d", UID_CHAR, EMPIRE_VNUM(ROOM_OWNER(r)) + EMPIRE_ID_BASE);
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "empire_id")) {
						if (ROOM_OWNER(r)) {
							snprintf(str, slen, "%d", EMPIRE_VNUM(ROOM_OWNER(r)));
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "empire_name")) {
						if (ROOM_OWNER(r)) {
							snprintf(str, slen, "%s", EMPIRE_NAME(ROOM_OWNER(r)));
						}
						else {
							strcpy(str, "");
						}
					}
					else if (!str_cmp(field, "enter_dir")) {
						if (BUILDING_ENTRANCE(r) != NO_DIR) {
							sprinttype(BUILDING_ENTRANCE(r), dirs, str, slen, "north");
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "exit_dir")) {
						if (BUILDING_ENTRANCE(r) != NO_DIR && rev_dir[BUILDING_ENTRANCE(r)] != NO_DIR) {
							sprinttype(rev_dir[BUILDING_ENTRANCE(r)], dirs, str, slen, "south");
						}
						else {
							*str = '\0';
						}
					}
					break;
				}
				case 'f': {	// room.f*
					if (!str_cmp(field, "fore")) {
						direction_vars(r, FORE, subfield, str, slen);
					}
					else if (!str_cmp(field, "function")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, function_flags, FALSE);
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", room_has_function_and_city_ok(NULL, r, BIT(pos)) ? 1 : 0);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					break;
				}
				case 'h': {	// room.h*
					if (!str_cmp(field, "has_evolution")) {
						int type;
						if (subfield && *subfield && (type = search_block(subfield, evo_types, FALSE)) != NOTHING) {
							snprintf(str, slen, "%d", (SECT(r) && has_evolution_type(SECT(r), type)) ? 1 : 0);
						}
						else {
							snprintf(str, slen, "0");	// no type (or invalid type) provided
						}
					}
					else if (!str_cmp(field, "has_trigger")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							snprintf(str, slen, "%d", has_trigger(SCRIPT(r), atoi(subfield)));
						}
						else {
							snprintf(str, slen, "0");	// no vnum provided
						}
					}
					else if (!str_cmp(field, "height")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							set_room_height(r, atoi(subfield));
						}
						snprintf(str, slen, "%d", ROOM_HEIGHT(r));
					}
					break;
				}
				case 'i': {	// room.i*
					if (!str_cmp(field, "id")) {
						if (r)
							snprintf(str, slen, "%d", GET_ROOM_VNUM(r) + ROOM_ID_BASE);
						else
							*str = '\0';
					}
					else if (!str_cmp(field, "in_city")) {
						bool junk;
						if (ROOM_OWNER(r) && is_in_city_for_empire(r, ROOM_OWNER(r), (*subfield && (atoi(subfield) || !str_cmp(subfield, "true"))), &junk)) {
							strcpy(str, "1");
						}
						else {
							strcpy(str, "0");
						}
					}
					else if (!str_cmp(field, "in_vehicle")) {
						if (GET_ROOM_VEHICLE(r)) {
							snprintf(str, slen, "%c%d", UID_CHAR, veh_script_id(GET_ROOM_VEHICLE(r)));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "is_on_map")) {
						snprintf(str, slen, "%d", (GET_ROOM_VNUM(r) < MAP_SIZE) ? 1 : 0);
					}
					else if (!str_cmp(field, "is_outdoors")) {
						snprintf(str, slen, "%d", IS_OUTDOOR_TILE(r) ? 1 : 0);
					}
					else if (!str_cmp(field, "is_zenith_day")) {
						snprintf(str, slen, "%d", is_zenith_day(r) ? 1 : 0);
					}
					break;
				}
				case 'm': {	// room.m*
					if (!str_cmp(field, "max_citizens")) {
						snprintf(str, slen, "%d", GET_BUILDING(HOME_ROOM(r)) ? GET_BLD_CITIZENS(GET_BUILDING(HOME_ROOM(r))) : 0);
					}
					else if (!str_cmp(field, "mine_type")) {
						if (get_room_extra_data(r, ROOM_EXTRA_MINE_GLB_VNUM) > 0) {
							snprintf(str, slen, "%s", get_mine_type_name(r));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "moon_phase")) {
						char arg1[256], arg2[256];
						generic_data *moon;
						comma_args(subfield, arg1, arg2);	// optional 2nd arg for long phase names
						
						if (!subfield || !*arg1) {
							strcpy(str, "");
						}
						else if ((isdigit(*arg1) && (moon = real_generic(atoi(arg1))) && GEN_TYPE(moon) == GENERIC_MOON) || (moon = find_generic_by_name(GENERIC_MOON, arg1, FALSE))) {
							if (*arg2 && (!str_cmp(arg2, "l") || !str_cmp(arg2, "long"))) {
								snprintf(str, slen, "%s", moon_phases_long[get_moon_phase(GET_MOON_CYCLE_DAYS(moon))]);
							}
							else {
								snprintf(str, slen, "%s", moon_phases[get_moon_phase(GET_MOON_CYCLE_DAYS(moon))]);
							}
						}
						else {	// unknown moon
							strcpy(str, "");
						}
					}
					else if (!str_cmp(field, "moon_pos")) {
						struct time_info_data tinfo;
						generic_data *moon;
						if (!subfield || !*subfield) {
							strcpy(str, "");
						}
						else if ((isdigit(*subfield) && (moon = real_generic(atoi(subfield))) && GEN_TYPE(moon) == GENERIC_MOON) || (moon = find_generic_by_name(GENERIC_MOON, subfield, FALSE))) {
							tinfo = get_local_time(r);
							snprintf(str, slen, "%s", moon_positions[get_moon_position(get_moon_phase(GET_MOON_CYCLE_DAYS(moon)), tinfo.hours)]);
						}
						else {	// unknown moon
							strcpy(str, "");
						}
					}
					break;
				}
				case 'n': {	// room.n*
					if (!str_cmp(field, "name")) {
						snprintf(str, slen, "%s",  get_room_name(r, FALSE));
					}
					else if (!str_cmp(field, "north")) {
						direction_vars(r, NORTH, subfield, str, slen);
					}
					else if (!str_cmp(field, "northeast") || !str_cmp(field, "ne")) {
						direction_vars(r, NORTHEAST, subfield, str, slen);
					}
					else if (!str_cmp(field, "northwest") || !str_cmp(field, "nw")) {
						direction_vars(r, NORTHWEST, subfield, str, slen);
					}
					break;
				}
				case 'p': {	// room.p*
					if (!str_cmp(field, "painted")) {
						if (ROOM_PAINT_COLOR(r)) {
							char color_name[80];
							sprinttype(ROOM_PAINT_COLOR(r), paint_names, color_name, sizeof(color_name), "");
							*color_name = LOWER(*color_name);
							snprintf(str, slen, "%s%s", (ROOM_AFF_FLAGGED(r, ROOM_AFF_BRIGHT_PAINT) ? "bright " : ""), color_name);
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "players_present")) {
						char_data *chit;
						int num = 0;
						DL_FOREACH2(ROOM_PEOPLE(r), chit, next_in_room) {
							if (!IS_NPC(chit)) {
								++num;
							}
						}
						snprintf(str, slen, "%d", num);
					}
					else if (!str_cmp(field, "people")) {
						char_data *temp_ch = NULL;
						mob_vnum find_vnum = NOTHING;
						
						// optional vnum search
						if (subfield && *subfield && (*subfield == '-' || isdigit(*subfield))) {
							find_vnum = atoi(subfield);
						}
				
						// attempt to prevent extracted people from showing in lists
						DL_FOREACH2(ROOM_PEOPLE(r), temp_ch, next_in_room) {
							if (EXTRACTED(temp_ch)) {
								continue;	// always skip
							}
							if (find_vnum != NOTHING && GET_MOB_VNUM(temp_ch) != find_vnum) {
								continue;	// vnum mismatch
							}
							if (find_vnum == NOTHING && SCRIPT_SHOULD_SKIP_CHAR(temp_ch)) {
								continue;	// skippable character (if not searching by vnum)
							}
							
							// if we got to here, this is valid character; break out
							break;
						}
				
						if (temp_ch) {
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(temp_ch));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "port")) {
						direction_vars(r, PORT, subfield, str, slen);
					}
					break;
				}
				case 'r': {	// room.r*
					if (!str_cmp(field, "rmt_flagged")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, room_template_flags, FALSE);
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", RMT_FLAGGED(r, BIT(pos)) ? 1 : 0);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							sprintbit(GET_ROOM_TEMPLATE(r) ? GET_RMT_FLAGS(GET_ROOM_TEMPLATE(r)) : NOBITS, room_template_flags, buf, TRUE);
							snprintf(str, slen, "%s", buf);
						}
					}
					break;
				}
				case 's': {	// room.s*
					if (!str_cmp(field, "season")) {
						snprintf(str, slen, "%s", icon_types[GET_SEASON(r)]);
						*str = LOWER(*str);
					}
					else if (!str_cmp(field, "sector")) {
						snprintf(str, slen, "%s", GET_SECT_NAME(SECT(r)));
					}
					else if (!str_cmp(field, "sector_flagged")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, sector_flags, FALSE);
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", ROOM_SECT_FLAGGED(r, BIT(pos)) ? 1 : 0);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							sprintbit(GET_SECT_FLAGS(SECT(r)), sector_flags, buf, TRUE);
							snprintf(str, slen, "%s", buf);
						}
					}
					else if (!str_cmp(field, "sector_vnum")) {
						snprintf(str, slen, "%d", GET_SECT_VNUM(SECT(r)));
					}
					else if (!str_cmp(field, "south")) {
						direction_vars(r, SOUTH, subfield, str, slen);
					}
					else if (!str_cmp(field, "southeast") || !str_cmp(field, "se")) {
						direction_vars(r, SOUTHEAST, subfield, str, slen);
					}
					else if (!str_cmp(field, "southwest") || !str_cmp(field, "sw")) {
						direction_vars(r, SOUTHWEST, subfield, str, slen);
					}
					else if (!str_cmp(field, "starboard")) {
						direction_vars(r, STARBOARD, subfield, str, slen);
					}
					else if (!str_cmp(field, "subzone")) {
						if (GET_ROOM_TEMPLATE(r) && GET_RMT_SUBZONE(GET_ROOM_TEMPLATE(r)) != NOWHERE) {
							snprintf(str, slen, "%d", GET_RMT_SUBZONE(GET_ROOM_TEMPLATE(r)));
						}
						else {
							// no valid subzone: show a -1
							snprintf(str, slen, "-1");
						}
					}
					else if (!str_cmp(field, "sun")) {
						snprintf(str, slen, "%s", sun_types[get_sun_status(r)]);
					}
					break;
				}
				case 't': {	// room.t*
					if (!str_cmp(field, "temperature")) {
						snprintf(str, slen, "%d", get_room_temperature(r));
					}
					else if (!str_cmp(field, "template")) {
						if (r && GET_ROOM_TEMPLATE(r)) {
							snprintf(str, slen, "%d", GET_RMT_VNUM(GET_ROOM_TEMPLATE(r))); 
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "time")) {
						struct time_info_data tinfo = get_local_time(r);
						
						if (!subfield || !str_cmp(subfield, "hour")) {
							// defaults to hour
							snprintf(str, slen, "%d", tinfo.hours);
						}
						else if (!str_cmp(subfield, "day")) {
							snprintf(str, slen, "%d", tinfo.day + 1);
						}
						else if (!str_cmp(subfield, "month")) {
							snprintf(str, slen, "%d", tinfo.month + 1);
						}
						else if (!str_cmp(subfield, "year")) {
							snprintf(str, slen, "%d", tinfo.year);
						}
						else if (!str_cmp(subfield, "day_of_year")) {
							snprintf(str, slen, "%d", DAY_OF_YEAR(tinfo));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "title")) {
						snprintf(str, slen, "%s", get_room_name(r, FALSE));
					}
					break;
				}
				case 'u': {	// room.u*
					if (!str_cmp(field, "up")) {
						direction_vars(r, UP, subfield, str, slen);
					}
					break;
				}
				case 'v': {	// room.v*
					if (!str_cmp(field, "var")) {
						struct trig_var_data *remote_vd;
						char arg1[256], arg2[256];
						bool found = FALSE;

						comma_args(subfield, arg1, arg2);
						*str = '\0';

						if (*arg1 && SCRIPT(r)) {
							LL_FOREACH(SCRIPT(r)->global_vars, remote_vd) {
								if (!str_cmp(remote_vd->name, arg1)) {
									snprintf(str, slen, "%s", remote_vd->value);
									found = TRUE;
									break;
								}
							}
						}
						if (!found && *arg2) {
							snprintf(str, slen, "%s", arg2);
						}
					}
					else if (!str_cmp(field, "varexists")) {
						struct trig_var_data *remote_vd;
						snprintf(str, slen, "0");
						if (SCRIPT(r)) {
							for (remote_vd = SCRIPT(r)->global_vars; remote_vd; remote_vd = remote_vd->next) {
								if (!str_cmp(remote_vd->name, subfield)) {
									break;
								}
							}
							if (remote_vd) {
								snprintf(str, slen, "1");
							}
						}
					}
					else if (!str_cmp(field, "vehicles")) {
						vehicle_data *temp_veh = NULL;
						
						// find first non-extracted vehicle
						DL_FOREACH2(ROOM_VEHICLES(r), temp_veh, next_in_room) {
							if (!VEH_IS_EXTRACTED(temp_veh)) {
								break;
							}
						}
						
						if (temp_veh) {
							snprintf(str, slen, "%c%d", UID_CHAR, veh_script_id(temp_veh));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "view_height")) {
						snprintf(str, slen, "%d", get_room_blocking_height(r, NULL));
					}
					else if (!str_cmp(field, "vnum")) {
						snprintf(str, slen, "%d", GET_ROOM_VNUM(r)); 
					}
					break;
				}
				case 'w': {	// room.w*
					if (!str_cmp(field, "weather")) {
						if (!ROOM_IS_CLOSED(r))
							snprintf(str, slen, "%s", weather_types[weather_info.sky]);
						else
							*str = '\0';
					}
					else if (!str_cmp(field, "west")) {
						direction_vars(r, WEST, subfield, str, slen);
					}
					break;
				}
			}
			
			if (*str == '\x1') { /* no match in switch */
				if (SCRIPT(r)) { /* check for global var */
					for (vd = (SCRIPT(r))->global_vars; vd; vd = vd->next)
						if (!str_cmp(vd->name, field))
							break;
					if (vd)
						snprintf(str, slen, "%s", vd->value);
					else {
						*str = '\0';
						script_log("Trigger: %s, VNum %d, type: %d. unknown room field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
					}
				}
				else {
					*str = '\0';
					script_log("Trigger: %s, VNum %d, type: %d. unknown room field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
				}
			}
		}	// if (r) ...
		

		else if (v) {	// vehicle variable
			if (text_processed(field, subfield, vd, str, slen, go, sc, trig, type))
				return;
				
			*str = '\x1';
			switch (LOWER(*field)) {
				case 'a': {	// veh.a*
					if (!str_cmp(field, "animals_harnessed")) {
						snprintf(str, slen, "%d", count_harnessed_animals(v));
					}
					else if (!str_cmp(field, "animals_required")) {
						snprintf(str, slen, "%d", VEH_ANIMALS_REQUIRED(v));
					}
					break;
				}
				case 'b': {	// veh.b*
					if (!str_cmp(field, "burn")) {
						if (VEH_FLAGGED(v, VEH_BURNABLE) && !VEH_FLAGGED(v, VEH_ON_FIRE)) {
							start_vehicle_burning(v);
						}
						*str = '\0';
					}
					break;
				}
				case 'c': {	// veh.c*
					if (!str_cmp(field, "can_fire")) {
						if (VEH_FLAGGED(v, VEH_SIEGE_WEAPONS) && (time(0) - VEH_LAST_FIRE_TIME(v)) >= config_get_int("vehicle_siege_time")) {
							snprintf(str, slen, "1");
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "complete")) {
						snprintf(str, slen, "%d", VEH_IS_COMPLETE(v) ? 1 : 0);
					}
					else if (!str_cmp(field, "contents")) {
						if (VEH_FLAGGED(v, VEH_CONTAINER) && VEH_CONTAINS(v)) {
							snprintf(str, slen, "%c%d", UID_CHAR, obj_script_id(VEH_CONTAINS(v)));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "count")) {
						int count = 0; 
						if (VEH_FLAGGED(v, VEH_CONTAINER)) {
							obj_vnum snum = (subfield && is_number(subfield)) ? atoi(subfield) : NOTHING;
							obj_data *item;
							DL_FOREACH2(VEH_CONTAINS(v), item, next_content) {
								if (snum != NOTHING) {
									if (snum == GET_OBJ_VNUM(item)) {
										++count;
									}
								}
								else {
									if (isname(subfield, item->name)) {
										++count;
									}
								}
							}
						}
						snprintf(str, slen, "%d", count);
					}
					break;
				}
				case 'd': {	// veh.d*
					if (!str_cmp(field, "dedicated_to")) {
						player_index_data *index;
						if (VEH_PATRON(v) && (index = find_player_index_by_idnum(VEH_PATRON(v)))) {
							snprintf(str, slen, "%s", index->fullname);
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "depletion")) {
						// usage: %veh.depletion(TYPE)% to get
						//        %veh.depletion(TYPE,AMOUNT)% to set
						char arg1[256], arg2[256];
						int type;
						comma_args(subfield, arg1, arg2);
						
						if (!*arg1 || (type = search_block(arg1, depletion_types, FALSE)) == NOTHING) {
							// missing type?
							*str = '\0';
						}
						else if (!*arg2 || !isdigit(*arg2)) {
							// just getting
							snprintf(str, slen, "%d", get_vehicle_depletion(v, type));
						}
						else {
							// setting
							set_vehicle_depletion(v, type, atoi(arg2));
							snprintf(str, slen, "%d", get_vehicle_depletion(v, type));
						}
					}
					else if (!str_cmp(field, "driver")) {
						if (VEH_DRIVER(v)) {
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(VEH_DRIVER(v)));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "dump")) {
						fully_empty_vehicle(v, IN_ROOM(v));
						*str = '\0';
					}
					else if (!str_cmp(field, "dump_objects")) {
						empty_vehicle(v, IN_ROOM(v));
						*str = '\0';
					}
					break;
				}
				case 'e': {	// veh.e*
					if (!str_cmp(field, "empire")) {
						if (VEH_OWNER(v)) {
							snprintf(str, slen, "%c%d", UID_CHAR, EMPIRE_VNUM(VEH_OWNER(v)) + EMPIRE_ID_BASE);
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "empire_id")) {
						if (VEH_OWNER(v)) {
							snprintf(str, slen, "%d", EMPIRE_VNUM(VEH_OWNER(v)));
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "empire_name")) {
						if (VEH_OWNER(v)) {
							snprintf(str, slen, "%s", EMPIRE_NAME(VEH_OWNER(v)));
						}
						else {
							strcpy(str, "");
						}
					}
					else if (!str_cmp(field, "extinguish")) {
						if (VEH_FLAGGED(v, VEH_ON_FIRE)) {
							remove_vehicle_flags(v, VEH_ON_FIRE);
						}
						*str = '\0';
					}
					break;
				}
				case 'f': {	// veh.f*
					if (!str_cmp(field, "function")) {
						if (subfield && *subfield) {
							bitvector_t pos = search_block(subfield, function_flags, FALSE);
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", vehicle_has_function_and_city_ok(v, BIT(pos)) ? 1 : 0);
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					break;
				}
				case 'h': {	// veh.h*
					if (!str_cmp(field, "harness")) {
						if (subfield && *subfield && VEH_ANIMALS_REQUIRED(v) < count_harnessed_animals(v)) {
							char_data *mob = NULL;
							
							// find or load mob
							if (*subfield == UID_CHAR) {
								mob = get_char(subfield);
							}
							else if (is_number(subfield)) {
								mob = read_mobile(atoi(subfield), FALSE);	// don't bother attaching triggers
							}
							
							if (mob && IS_NPC(mob)) {
								harness_mob_to_vehicle(mob, v);
							}
						}
						*str = '\0';
					}
					else if (!str_cmp(field, "has_in")) {
						bool found = FALSE;
						if (VEH_FLAGGED(v, VEH_CONTAINER)) {
							obj_data *item;
							obj_vnum snum = (subfield && is_number(subfield)) ? atoi(subfield) : NOTHING;
							
							DL_FOREACH2(VEH_CONTAINS(v), item, next_content) {
								if (snum != NOTHING) {
									if (snum == GET_OBJ_VNUM(item)) {
										found = TRUE;
										break;
									}
								}
								else {
									if (isname(subfield, item->name)) {
										found = TRUE;
										break;
									}
								}
							}
						}
						
						snprintf(str, slen, "%d", found ? 1 : 0);
					}
					else if (!str_cmp(field, "has_trigger")) {
						if (subfield && *subfield && isdigit(*subfield)) {
							snprintf(str, slen, "%d", has_trigger(SCRIPT(v), atoi(subfield)));
						}
						else {
							snprintf(str, slen, "0");	// no vnum provided
						}
					}
					else if (!str_cmp(field, "health") || !str_cmp(field, "hitp")) {
						snprintf(str, slen, "%d", (int) VEH_HEALTH(v));
					}
					break;
				}
				case 'i': {	// veh.i*
					if (!str_cmp(field, "id")) {
						snprintf(str, slen, "%d", veh_script_id(v));
					}
					else if (!str_cmp(field, "in_on")) {
						snprintf(str, slen, "%s", IN_OR_ON(v));
					}
					else if (!str_cmp(field, "interior")) {
						room_data *interior = get_vehicle_interior(v);
						if (interior) {
							snprintf(str, slen, "%c%d", UID_CHAR, GET_ROOM_VNUM(interior) + ROOM_ID_BASE);
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "is_flagged")) {
						if (subfield && *subfield) {
							int fl = search_block(subfield, vehicle_flags, FALSE);
							if (fl != NOTHING) {
								snprintf(str, slen, (VEH_FLAGGED(v, BIT(fl)) ? "1" : "0"));
							}
							else {
								snprintf(str, slen, "0");
							}
						}
						else {
							sprintbit(VEH_FLAGS(v), vehicle_flags, buf, TRUE);
							snprintf(str, slen, "%s", buf);
						}
					}
					break;
				}
				case 'k': {	// veh.k*
					if (!str_cmp(field, "keywords")) {
						snprintf(str, slen, "%s", VEH_KEYWORDS(v));
					}
					break;
				}
				case 'l': {	// veh.l*
					if (!str_cmp(field, "led_by")) {
						if (VEH_LED_BY(v)) {
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(VEH_LED_BY(v)));
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "level")) {
						snprintf(str, slen, "%d", VEH_SCALE_LEVEL(v));
					}
					else if (!str_cmp(field, "link_instance")) {
						struct instance_data *inst;
						if (VEH_INSTANCE_ID(v) != NOTHING) {
							VEH_INSTANCE_ID(v) = NOTHING;
						}
						if ((inst = find_instance_by_room(IN_ROOM(v), FALSE, TRUE))) {
							VEH_INSTANCE_ID(v) = inst->id;
						}
						request_vehicle_save_in_world(v);
						*str = '\0';
					}
					else if (!str_cmp(field, "longdesc")) {
						snprintf(str, slen, "%s", VEH_LONG_DESC(v));
						strip_crlf(str);	// may have trailing \r\n
					}
					break;
				}
				case 'm': {	// veh.m*
					if (!str_cmp(field, "maxhealth") || !str_cmp(field, "maxhitp")) {
						snprintf(str, slen, "%d", VEH_MAX_HEALTH(v));
					}
					else if (!str_cmp(field, "maxrooms")) {
						snprintf(str, slen, "%d", VEH_MAX_ROOMS(v));
					}
					else if (!str_cmp(field, "movetype")) {
						snprintf(str, slen, "%s", mob_move_types[VEH_MOVE_TYPE(v)]);
					}
					break;
				}
				case 'n': {	// veh.n*
					if (!str_cmp(field, "needsrepair")) {
						if (VEH_IS_COMPLETE(v) && (VEH_NEEDS_RESOURCES(v) || VEH_HEALTH(v) < VEH_MAX_HEALTH(v))) {
							snprintf(str, slen, "1");
						}
						else {
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "next_in_room")) {
						vehicle_data *temp_veh = NULL;
						
						DL_FOREACH2(v->next_in_room, temp_veh, next_in_room) {
							if (!VEH_IS_EXTRACTED(temp_veh)) {
								break;
							}
						}
						
						if (temp_veh) {
							snprintf(str, slen, "%c%d", UID_CHAR, veh_script_id(temp_veh));
						}
						else {
							strcpy(str, "");
						}
					}
					break;
				}
				case 'p': {	// veh.p*
					if (!str_cmp(field, "painted")) {
						if (VEH_PAINT_COLOR(v)) {
							char color_name[80];
							sprinttype(VEH_PAINT_COLOR(v), paint_names, color_name, sizeof(color_name), "");
							*color_name = LOWER(*color_name);
							snprintf(str, slen, "%s%s", (VEH_FLAGGED(v, VEH_BRIGHT_PAINT) ? "bright " : ""), color_name);
						}
						else {
							*str = '\0';
						}
					}
					break;
				}
				case 'r': {	// veh.r*
					if (!str_cmp(field, "room")) {
						snprintf(str, slen, "%c%d",UID_CHAR, GET_ROOM_VNUM(IN_ROOM(v)) + ROOM_ID_BASE); 
					}
					else if (!str_cmp(field, "rooms")) {
						snprintf(str, slen, "%d", VEH_INSIDE_ROOMS(v));
					}
					break;
				}
				case 's': {	// veh.s*
					if (!str_cmp(field, "shortdesc")) {
						snprintf(str, slen, "%s", VEH_SHORT_DESC(v));
					}
					else if (!str_cmp(field, "sitting_on") || !str_cmp(field, "sitting_in")) {
						if (VEH_SITTING_ON(v)) {
							snprintf(str, slen, "%c%d", UID_CHAR, char_script_id(VEH_SITTING_ON(v)));
						}
						else {
							*str = '\0';
						}
					}
					break;
				}
				case 'u': {	// veh.u*
					if (!str_cmp(field, "unharness")) {
						while (VEH_ANIMALS(v)) {
							unharness_mob_from_vehicle(VEH_ANIMALS(v), v);
						}
						*str = '\0';
					}
					else if (!str_cmp(field, "unlink_instance")) {
						VEH_INSTANCE_ID(v) = NOTHING;
						request_vehicle_save_in_world(v);
						*str = '\0';
					}
					break;
				}
				case 'v': {	// veh.v*
					if (!str_cmp(field, "var")) {
						struct trig_var_data *remote_vd;
						char arg1[256], arg2[256];
						bool found = FALSE;

						comma_args(subfield, arg1, arg2);
						*str = '\0';

						if (*arg1 && SCRIPT(v)) {
							LL_FOREACH(SCRIPT(v)->global_vars, remote_vd) {
								if (!str_cmp(remote_vd->name, arg1)) {
									snprintf(str, slen, "%s", remote_vd->value);
									found = TRUE;
									break;
								}
							}
						}
						if (!found && *arg2) {
							snprintf(str, slen, "%s", arg2);
						}
					}
					else if (!str_cmp(field, "varexists")) {
						struct trig_var_data *remote_vd;
						snprintf(str, slen, "0");
						if (SCRIPT(v)) {
							for (remote_vd = SCRIPT(v)->global_vars; remote_vd; remote_vd = remote_vd->next) {
								if (!str_cmp(remote_vd->name, subfield)) {
									break;
								}
							}
							if (remote_vd) {
								snprintf(str, slen, "1");
							}
						}
					}
					else if (!str_cmp(field, "vnum")) {
						snprintf(str, slen, "%d", VEH_VNUM(v));
					}
					break;
				}
			}	// end switch
			
			if (*str == '\x1') { /* no match in switch */
				if (SCRIPT(v)) { /* check for global var */
					for (vd = (SCRIPT(v))->global_vars; vd; vd = vd->next) {
						if (!str_cmp(vd->name, field)) {
							break;
						}
					}
					if (vd) {
						snprintf(str, slen, "%s", vd->value);
					}
					else {
						*str = '\0';
						script_log("Trigger: %s, VNum %d, type: %d. unknown vehicle field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
					}
				}
				else {
					*str = '\0';
					script_log("Trigger: %s, VNum %d, type: %d. unknown vehicle field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
				}
			}
		}	// if (v) ...
		

		else if (e) {	// empire variable
			if (text_processed(field, subfield, vd, str, slen, go, sc, trig, type)) {
				return;
			}
				
			*str = '\x1';
			switch (LOWER(*field)) {
				case 'a': {	// emp.a*
					if (!str_cmp(field, "add_progress")) {
						if (subfield && *subfield) {
							progress_data *prg;
							any_vnum vnum;
							
							if (isdigit(*subfield) && (vnum = atoi(subfield)) != NOTHING && (prg = real_progress(vnum))) {
								if (!empire_has_completed_goal(e, vnum)) {
									script_reward_goal(e, prg);
									check_for_eligible_goals(e);
								}
								strcpy(str, "1");
							}
							else {
								script_log("Trigger: %s, VNum %d. add_progress called with invalid vnum '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
								strcpy(str, "0");
							}
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "adjective")) {
						snprintf(str, slen, "%s", EMPIRE_ADJECTIVE(e));
					}
					break;
				}
				case 'b': {	// emp.b*
					if (!str_cmp(field, "banner")) {
						snprintf(str, slen, "%s", EMPIRE_BANNER(e));
					}
					break;
				}
				case 'c': {	// emp.c*
					if (!str_cmp(field, "can_recognize_lang")) {
						// arg1: language name/vnum
						// arg2: optional on/off to toggle recognize
						char arg1[256], arg2[256];
						int mode;
						generic_data *lang = NULL;
						
						comma_args(subfield, arg1, arg2);
						
						// load language
						if (*arg1) {
							if (isdigit(*arg1)) {
								lang = find_generic(atoi(arg1), GENERIC_LANGUAGE);
							}
							if (!lang) {
								lang = find_generic_no_spaces(GENERIC_LANGUAGE, arg1);
							}
						}
						
						// optional toggle on/off
						if (*arg2 && lang) {
							if (!str_cmp(arg2, "1") || !str_cmp(arg2, "on")) {
								if (speaks_language_empire(e, GEN_VNUM(lang)) != LANG_SPEAK) {
									add_language_empire(e, GEN_VNUM(lang), LANG_RECOGNIZE);
								}
							}
							else if (!str_cmp(arg2, "0") || !str_cmp(arg2, "off")) {
								if (speaks_language_empire(e, GEN_VNUM(lang)) != LANG_SPEAK) {
									add_language_empire(e, GEN_VNUM(lang), LANG_UNKNOWN);
								}
							}
							check_languages_empire(e);
						}
						
						// and show recognizability
						mode = lang ? speaks_language_empire(e, GEN_VNUM(lang)) : LANG_UNKNOWN;
						snprintf(str, slen, "%d", (mode == LANG_RECOGNIZE || mode == LANG_SPEAK) ? 1 : 0);
					}
					else if (!str_cmp(field, "can_speak")) {
						// arg1: language name/vnum
						// arg2: optional on/off to toggle speaking
						char arg1[256], arg2[256];
						generic_data *lang = NULL;
						
						comma_args(subfield, arg1, arg2);
						
						// load language
						if (*arg1) {
							if (isdigit(*arg1)) {
								lang = find_generic(atoi(arg1), GENERIC_LANGUAGE);
							}
							if (!lang) {
								lang = find_generic_no_spaces(GENERIC_LANGUAGE, arg1);
							}
						}
						
						// optional toggle on/off
						if (*arg2 && lang) {
							if (!str_cmp(arg2, "1") || !str_cmp(arg2, "on")) {
								add_language_empire(e, GEN_VNUM(lang), LANG_SPEAK);
							}
							else if (!str_cmp(arg2, "0") || !str_cmp(arg2, "off")) {
								add_language_empire(e, GEN_VNUM(lang), LANG_UNKNOWN);
							}
							check_languages_empire(e);
						}
						
						// and show speakability
						snprintf(str, slen, "%d", (lang && speaks_language_empire(e, GEN_VNUM(lang)) == LANG_SPEAK) ? 1 : 0);
					}
					else if (!str_cmp(field, "charge_coins")) {
						if (subfield && isdigit(*subfield)) {
							decrease_empire_coins(e, e, atoi(subfield));
						}
						*str = '\0';
					}
					else if (!str_cmp(field, "coins")) {
						snprintf(str, slen, "%d", (int) EMPIRE_COINS(e));
					}
					break;
				}
				case 'd': {	// emp.d*
					if (!str_cmp(field, "description")) {
						snprintf(str, slen, "%s", EMPIRE_DESCRIPTION(e));
					}
					break;
				}
				case 'f': {	// emp.f*
					if (!str_cmp(field, "fame")) {
						snprintf(str, slen, "%d", EMPIRE_FAME(e));
					}
					break;
				}
				case 'g': {	// emp.g*
					if (!str_cmp(field, "give_coins")) {
						if (subfield && isdigit(*subfield)) {
							increase_empire_coins(e, e, atoi(subfield));
						}
						*str = '\0';
					}
					else if (!str_cmp(field, "grt") || !str_cmp(field, "greatness")) {
						snprintf(str, slen, "%d", EMPIRE_GREATNESS(e));
					}
					break;
				}
				case 'h': {	// emp.h*
					if (!str_cmp(field, "has_progress")) {
						if (subfield && *subfield) {
							any_vnum vnum;
							
							if (isdigit(*subfield) && (vnum = atoi(subfield)) != NOTHING) {
								snprintf(str, slen, "%d", empire_has_completed_goal(e, vnum) ? 1 : 0);
							}
							else {
								strcpy(str, "0");
							}
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "has_storage")) {
						if (subfield && *subfield) {
							// arg1: obj vnum
							// arg2: room location (storage is per-island
							any_vnum vnum;
							char arg1[256], arg2[256];
							room_data *where;
							struct empire_storage_data *store;
							
							comma_args(subfield, arg1, arg2);
							if (!*arg1 || !*arg2 || !isdigit(*arg1) || (vnum = atoi(arg1)) < 0) {
								// bad input
								snprintf(str, slen, "0");
							}
							else if (!(where = get_room(NULL, arg2))) {
								// bad loccation
								snprintf(str, slen, "0");
							}
							else if (!(store = find_stored_resource(e, GET_ISLAND_ID(where), vnum))) {
								// none stored
								snprintf(str, slen, "0");
							}
							else {
								// found
								snprintf(str, slen, "%d", store->amount);
							}
						}
						else {
							// no input
							snprintf(str, slen, "0");
						}
					}
					else if (!str_cmp(field, "has_tech")) {
						if (subfield && *subfield) {
							int pos;
							
							if ((pos = search_block(subfield, empire_tech_types, FALSE)) != NOTHING) {
								snprintf(str, slen, "%d", EMPIRE_HAS_TECH(e, pos) ? 1 : 0);
							}
							else {
								*str = '\0';
							}
						}
						else {
							*str = '\0';
						}
					}
					break;
				}
				case 'i': {	// emp.i*
					if (!str_cmp(field, "id")) {
						// this is scripting id
						snprintf(str, slen, "%d", EMPIRE_VNUM(e) + EMPIRE_ID_BASE);
					}
					else if (!str_cmp(field, "is_on_progress")) {
						if (subfield && *subfield) {
							any_vnum vnum;
							
							if (isdigit(*subfield) && (vnum = atoi(subfield)) != NOTHING) {
								snprintf(str, slen, "%d", get_current_goal(e, vnum) ? 1 : 0);
							}
							else {
								strcpy(str, "0");
							}
						}
						else {
							*str = '\0';
						}
					}
					break;
				}
				case 'l': {	// emp.l*
					if (!str_cmp(field, "leader_id")) {
						snprintf(str, slen, "%d", EMPIRE_LEADER(e));
					}
					break;
				}
				case 'm': {	// emp.m*
					if (!str_cmp(field, "max_territory")) {
						snprintf(str, slen, "%d", land_can_claim(e, TER_TOTAL));
					}
					else if (!str_cmp(field, "members")) {
						snprintf(str, slen, "%d", EMPIRE_MEMBERS(e));
					}
					else if (!str_cmp(field, "military")) {
						snprintf(str, slen, "%d", EMPIRE_MILITARY(e));
					}
					break;
				}
				case 'n': {	// emp.n*
					if (!str_cmp(field, "name")) {
						snprintf(str, slen, "%s", EMPIRE_NAME(e));
					}
					break;
				}
				case 'p': {	// emp.p*
					if (!str_cmp(field, "population")) {
						snprintf(str, slen, "%d", EMPIRE_POPULATION(e));
					}
					else if (!str_cmp(field, "priv")) {
						if (subfield && *subfield) {
							int pos = search_block(subfield, priv, FALSE);
							
							if (pos != NOTHING) {
								snprintf(str, slen, "%d", EMPIRE_PRIV(e, pos));
							}
							// let any other result error out.
						}
					}
					break;
				}
				case 'q': {	// emp.q*
					if (!str_cmp(field, "qualifies_for_progress")) {
						if (subfield && *subfield) {
							progress_data *prg;
							any_vnum vnum;
							
							if (isdigit(*subfield) && (vnum = atoi(subfield)) != NOTHING && (prg = real_progress(vnum))) {
								snprintf(str, slen, "%d", (!empire_has_completed_goal(e, vnum) && empire_meets_goal_prereqs(e, prg)) ? 1 : 0);
							}
							else {
								strcpy(str, "0");
							}
						}
						else {
							*str = '\0';
						}
					}
					break;
				}
				case 'r': {	// emp.r*
					if (!str_cmp(field, "rank")) {
						if (subfield && *subfield) {
							int num = atoi(subfield);
							if (num > 0 && num <= EMPIRE_NUM_RANKS(e)) {
								snprintf(str, slen, "%s", EMPIRE_RANK(e, num-1));
							}
							else {
								*str = '\0';
							}
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "remove_progress")) {
						if (subfield && *subfield) {
							struct empire_goal *goal;
							progress_data *prg;
							any_vnum vnum;
							
							if (isdigit(*subfield) && (vnum = atoi(subfield)) != NOTHING && (prg = real_progress(vnum))) {
								if (empire_has_completed_goal(e, vnum)) {
									remove_completed_goal(e, vnum);
								}
								if ((goal = get_current_goal(e, vnum))) {
									cancel_empire_goal(e, goal);
								}
								strcpy(str, "1");
								check_for_eligible_goals(e);
							}
							else {
								strcpy(str, "0");
							}
						}
						else {
							*str = '\0';
						}
					}
					else if (!str_cmp(field, "ranks")) {
						snprintf(str, slen, "%d", EMPIRE_NUM_RANKS(e));
					}
					break;
				}
				case 's': {	// emp.s*
					if (!str_cmp(field, "start_progress")) {
						if (subfield && *subfield) {
							progress_data *prg;
							any_vnum vnum;
							
							if (isdigit(*subfield) && (vnum = atoi(subfield)) != NOTHING && (prg = real_progress(vnum))) {
								if (!empire_has_completed_goal(e, vnum) && !get_current_goal(e, vnum) && empire_meets_goal_prereqs(e, prg)) {
									struct empire_goal *goal = start_empire_goal(e, prg);
									int complete, total;
									
									if (goal) {
										refresh_one_goal_tracker(e, goal);
									}
									// check if complete
									count_quest_tasks(goal->tracker, &complete, &total);
									if (complete == total) {
										complete_goal(e, goal);
									}
								}
								strcpy(str, "1");
							}
							else {
								script_log("Trigger: %s, VNum %d. start_progress called with invalid vnum '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), subfield);
								strcpy(str, "0");
							}
						}
						else {
							*str = '\0';
						}
					}
					break;
				}
				
				case 't': {	// emp.t*
					if (!str_cmp(field, "territory")) {
						snprintf(str, slen, "%d", EMPIRE_TERRITORY(e, TER_TOTAL));
					}
					break;
				}
				case 'v': {	// emp.v*
					if (!str_cmp(field, "var")) {
						struct trig_var_data *remote_vd;
						char arg1[256], arg2[256];
						bool found = FALSE;

						comma_args(subfield, arg1, arg2);
						*str = '\0';

						if (*arg1 && SCRIPT(e)) {
							LL_FOREACH(SCRIPT(e)->global_vars, remote_vd) {
								if (!str_cmp(remote_vd->name, arg1)) {
									snprintf(str, slen, "%s", remote_vd->value);
									found = TRUE;
									break;
								}
							}
						}
						if (!found && *arg2) {
							snprintf(str, slen, "%s", arg2);
						}
					}
					else if (!str_cmp(field, "varexists")) {
						struct trig_var_data *remote_vd;
						snprintf(str, slen, "0");
						if (SCRIPT(e)) {
							for (remote_vd = SCRIPT(e)->global_vars; remote_vd; remote_vd = remote_vd->next) {
								if (!str_cmp(remote_vd->name, subfield)) {
									break;
								}
							}
							if (remote_vd) {
								snprintf(str, slen, "1");
							}
						}
					}
					else if (!str_cmp(field, "vnum")) {
						snprintf(str, slen, "%d", EMPIRE_VNUM(e));
					}
					break;
				}
				case 'w': {	// emp.w*
					if (!str_cmp(field, "wealth")) {
						snprintf(str, slen, "%d", (int) GET_TOTAL_WEALTH(e));
					}
					break;
				}
			}	// end switch
			
			if (*str == '\x1') { /* no match in switch */
				if (SCRIPT(e)) { /* check for global var */
					for (vd = (SCRIPT(e))->global_vars; vd; vd = vd->next) {
						if (!str_cmp(vd->name, field)) {
							break;
						}
					}
					if (vd) {
						snprintf(str, slen, "%s", vd->value);
					}
					else {
						*str = '\0';
						script_log("Trigger: %s, VNum %d, type: %d. unknown empire field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
					}
				}
				else {
					*str = '\0';
					script_log("Trigger: %s, VNum %d, type: %d. unknown empire field: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
				}
			}
		}	// if (e) ...
		
		else {
			if (vd && text_processed(field, subfield, vd, str, slen, go, sc, trig, type))
				return;
		}
	}
}

/* 
* Now automatically checks if the variable has more then one field
* in it. And if the field returns a name or a script UID or the like
* it can recurse.
* If you supply a value like, %actor.int.str% it wont blow up on you
* either.
* - Jamie Nelson 31st Oct 2003 01:03
*
* Now also lets subfields have variables parsed inside of them
* so that:
* %echo% %actor.gold(%actor.gold%)%
* will double the actors gold every time its called.  etc...
* - Jamie Nelson 31st Oct 2003 01:24
*/

/* substitutes any variables into line and returns it as buf */
void var_subst(void *go, struct script_data *sc, trig_data *trig, int type, char *line, char *buf) {
	char tmp[MAX_INPUT_LENGTH], repl_str[MAX_INPUT_LENGTH];
	char *var = NULL, *field = NULL, *p = NULL, *p_end = NULL;;
	char tmp2[MAX_INPUT_LENGTH];
	char *subfield_p, subfield[MAX_INPUT_LENGTH];
	int left, len;
	int paren_count = 0;
	int dots = 0;

	/* skip out if no %'s */
	if (!strchr(line, '%')) {
		strcpy(buf, line);
		return;
	}
	/*lets just empty these to start with*/
	*repl_str = *tmp = *tmp2 = '\0';

	p = strcpy(tmp, line);
	p_end = strchr(p, '\0');
	subfield_p = subfield;

	left = MAX_INPUT_LENGTH - 1;

	while (p < p_end && *p && (left > 0)) {


		/* copy until we find the first % */
		while (p < p_end && *p && (*p != '%') && (left > 0)) {
			*(buf++) = *(p++);
			left--;
		}

		*buf = '\0';

		/* double % */
		if (p < p_end && *p && (*(++p) == '%') && (left > 0)) {
			*(buf++) = *(p++);
			*buf = '\0';
			left--;
			continue;
		}

		/* so it wasn't double %'s */
		else if (p < p_end && *p && (left > 0)) {

			/* search until end of var or beginning of field */      
			for (var = p; p < p_end && *p && (*p != '%') && (*p != '.'); p++);

			field = p;
			if (p < p_end && *p == '.') {
				*(p++) = '\0';
				dots = 0;
				for (field = p; p < p_end && *p && ((*p != '%')||(paren_count > 0) || (dots)); ++p) {
					if (dots > 0) {
						*subfield_p = '\0';
						find_replacement(go, sc, trig, type, var, field, subfield, repl_str, sizeof(repl_str));
						
						// reset subfield -- this fixes a dg scripts error where subfields would continue concatenating
						subfield_p = subfield;
						
						if (*repl_str) {   
							snprintf(tmp2, sizeof(tmp2), "eval tmpvr %s", repl_str); //temp var
							process_eval(go, sc, trig, type, tmp2);
							strcpy(var, "tmpvr");
							field = p;
							dots = 0;
							continue;
						}
						dots = 0;
					}
					else if (*p=='(') {
						*p = '\0';
						paren_count++;
					}
					else if (*p==')') {
						*p = '\0';
						paren_count--;
					}
					else if (paren_count > 0) {
						*subfield_p++ = *p;
					}
					else if (*p=='.') {
						*p = '\0';
						dots++;
					} 
				} /* for (field.. */
			} /* if *p == '.' */

			if (p < p_end) {
				*(p++) = '\0';
			}
			*subfield_p = '\0';

			if (*subfield) {
				var_subst(go, sc, trig, type, subfield, tmp2);
				strcpy(subfield, tmp2);
			}

			find_replacement(go, sc, trig, type, var, field, subfield, repl_str, sizeof(repl_str));

			strncat(buf, repl_str, left);
			len = strlen(repl_str);
			buf += len;
			left -= len;
			
			// reset subfield -- this fixes a dg scripts error where subfields would continue concatenating
			subfield_p = subfield;
		} /* else if *p .. */
	} /* while *p .. */ 
}

/* returns 1 if string is all digits, else 0 */
int is_num(char *num) {
	if (*num == '-')
		num++;
	
	while (*num && isdigit(*num))
		num++;

	if (!*num || isspace(*num))
		return 1;
	else
		return 0;
}


/* evaluates 'lhs op rhs', and copies to result */
void eval_op(char *op, char *lhs, char *rhs, char *result, void *go, struct script_data *sc, trig_data *trig) {
	char *p;
	int n;

	/* strip off extra spaces at begin and end */
	while (*lhs && isspace(*lhs)) 
		++lhs;
	while (*rhs && isspace(*rhs))
		++rhs;

	for (p = lhs; *p; ++p);
	if (p > lhs) {
		for (--p; (p > lhs) && isspace(*p); *p-- = '\0');
	}
	for (p = rhs; *p; ++p);
	if (p > rhs) {
		for (--p; (p > rhs) && isspace(*p); *p-- = '\0');  
	}

	/* find the op, and figure out the value */
	if (!strcmp("||", op)) {
		if ((!*lhs || (*lhs == '0')) && (!*rhs || (*rhs == '0')))
			strcpy(result, "0");
		else
			strcpy(result, "1");
	}

	else if (!strcmp("&&", op)) {
		if (!*lhs || (*lhs == '0') || !*rhs || (*rhs == '0'))
			strcpy (result, "0");
		else
			strcpy (result, "1");
	}

	else if (!strcmp("==", op)) {
		if (is_num(lhs) && is_num(rhs))
			sprintf(result, "%d", atoi(lhs) == atoi(rhs));
		else
			sprintf(result, "%d", !str_cmp(lhs, rhs));
	}   

	else if (!strcmp("!=", op)) {
		if (is_num(lhs) && is_num(rhs))
			sprintf(result, "%d", atoi(lhs) != atoi(rhs));
		else
			sprintf(result, "%d", str_cmp(lhs, rhs));
	}   

	else if (!strcmp("<=", op)) {
		if (is_num(lhs) && is_num(rhs))
			sprintf(result, "%d", atoi(lhs) <= atoi(rhs));
		else
			sprintf(result, "%d", str_cmp(lhs, rhs) <= 0);
	}

	else if (!strcmp(">=", op)) {
		if (is_num(lhs) && is_num(rhs))
			sprintf(result, "%d", atoi(lhs) >= atoi(rhs));
		else
			sprintf(result, "%d", str_cmp(lhs, rhs) <= 0);
	}

	else if (!strcmp("<", op)) {
		if (is_num(lhs) && is_num(rhs))
			sprintf(result, "%d", atoi(lhs) < atoi(rhs));
		else
			sprintf(result, "%d", str_cmp(lhs, rhs) < 0);
	}

	else if (!strcmp(">", op)) {
		if (is_num(lhs) && is_num(rhs))
			sprintf(result, "%d", atoi(lhs) > atoi(rhs));
		else
			sprintf(result, "%d", str_cmp(lhs, rhs) > 0);
	}

	else if (!strcmp("/=", op))
		sprintf(result, "%c", is_abbrev(rhs, lhs) ? '1' : '0');
	
	else if (!strcmp("~=", op))
		sprintf(result, "%c", is_substring(rhs, lhs) ? '1' : '0');

	else if (!strcmp("*", op))
		sprintf(result, "%d", atoi(lhs) * atoi(rhs));

	else if (!strcmp("//", op))
		sprintf(result, "%d", (n = atoi(rhs)) ? (atoi(lhs) % n) : 0);

	else if (!strcmp("/", op))
		sprintf(result, "%d", (n = atoi(rhs)) ? (atoi(lhs) / n) : 0);

	else if (!strcmp("+", op)) 
		sprintf(result, "%d", atoi(lhs) + atoi(rhs));

	else if (!strcmp("-", op))
		sprintf(result, "%d", atoi(lhs) - atoi(rhs));

	else if (!strcmp("!", op)) {
		if (is_num(rhs))
			sprintf(result, "%d", !atoi(rhs));
		else
			sprintf(result, "%d", !*rhs);
	}
}


/*
* p points to the first quote, returns the matching
* end quote, or the last non-null char in p.
*/
char *matching_quote(char *p) {
	for (p++; *p && (*p != '"'); p++) {
		if (*p == '\\')
			p++;
	}

	if (!*p)
		p--;

	return p;
}

/*
* p points to the first paren.  returns a pointer to the
* matching closing paren, or the last non-null char in p.
*/
char *matching_paren(char *p) {
	int i;

	for (p++, i = 1; *p && i; p++) {
		if (*p == '(')
			i++;
		else if (*p == ')')
			i--;
		else if (*p == '"')
			p = matching_quote(p);
	}

	return --p;
}


/* evaluates line, and returns answer in result */
void eval_expr(char *line, char *result, void *go, struct script_data *sc, trig_data *trig, int type) {
	char expr[MAX_INPUT_LENGTH], *p;

	while (*line && isspace(*line))
		line++;

	if (eval_lhs_op_rhs(line, result, go, sc, trig, type));

	else if (*line == '(') {
		p = strcpy(expr, line);
		p = matching_paren(expr);
		*p = '\0';
		eval_expr(expr + 1, result, go, sc, trig, type);
	}

	else
		var_subst(go, sc, trig, type, line, result);
}


/*
* evaluates expr if it is in the form lhs op rhs, and copies
* answer in result.  returns 1 if expr is evaluated, else 0
*/
int eval_lhs_op_rhs(char *expr, char *result, void *go, struct script_data *sc, trig_data *trig, int type) {
	char *p, *tokens[MAX_INPUT_LENGTH];
	char line[MAX_INPUT_LENGTH], lhr[MAX_INPUT_LENGTH], rhr[MAX_INPUT_LENGTH];
	int i, j, oplist, tsize;
	char *found;

	/*
	* valid operands, in order of priority
	* each must also be defined in eval_op()
	*/
	#define num_op_lists  8
	static char *ops[num_op_lists][5] = {
		// higher in this table = higher priority
		{ "!", "\n" },
		{ "//", "*", "/", "\n" },	// things on same line have same precedence
		{ "+", "-", "\n" },
		{ "<=", "<", ">=", ">", "\n" },
		{ "/=", "~=", "\n" },
		{ "==", "!=", "\n" },
		{ "&&", "\n" },
		{ "||", "\n" }	// each list must end with "\n"
	};
	
	// symbols used in operators
	const char *opsymbols = "!/*+-<>=~&|";
	
	p = strcpy(line, expr);

	/*
	* initialize tokens, an array of pointers to locations
	* in line where the ops could possibly occur.
	*/
	for (j = 0; *p; j++) {
		tokens[j] = p;
		if (*p == '(')
			p = matching_paren(p) + 1;
		else if (*p == '"')
			p = matching_quote(p) + 1;
		else if (isalnum(*p))
			for (p++; *p && (isalnum(*p) || isspace(*p)); p++);
		else if (strchr(opsymbols, *p)) {
			for (p++; *p && strchr(opsymbols, *p); ++p);
		}
		else
			p++;
	}
	tokens[j] = NULL;
	tsize = j;
	
	for (oplist = num_op_lists - 1; oplist >= 0; --oplist) {
		for (j = tsize - 1; j >= 0; --j) {
			// try to find this token in this oplist
			found = NULL;
			for (i = 0; !found && *ops[oplist][i] != '\n'; ++i) {
				if (!strn_cmp(ops[oplist][i], tokens[j], strlen(ops[oplist][i]))) {
					found = ops[oplist][i];
					// -> need to find the LAST token that's in THIS list
				}
			}
			// [IMMORTAL Khufu]: if it finds a matching operator, it needs to check the rest of the array for another operator with the same precedence...
			// why is it not working to check tokens in reverse order?
	
			if (found) {
				*tokens[j] = '\0';
				p = tokens[j] + strlen(found);

				eval_expr(line, lhr, go, sc, trig, type);
				eval_expr(p, rhr, go, sc, trig, type);
				eval_op(found, lhr, rhr, result, go, sc, trig);

				return 1;
			}
		}
	}

	return 0;
}



/* returns 1 if cond is true, else 0 */
int process_if(char *cond, void *go, struct script_data *sc, trig_data *trig, int type) {
	char result[MAX_INPUT_LENGTH], *p;

	eval_expr(cond, result, go, sc, trig, type);

	p = result;
	skip_spaces(&p);

	if (!*p || *p == '0')
		return 0;
	else
		return 1;
}


/*
* scans for end of if-block.
* returns the line containg 'end', or the last
* line of the trigger if not found.
*/
struct cmdlist_element *find_end(struct cmdlist_element *cl) {
	struct cmdlist_element *c;
	char *p;

	if (!(cl->next))
		return cl;

	for (c = cl->next; c && c->next; c = c->next) {
		for (p = c->cmd; *p && isspace(*p); p++);

		if (!strn_cmp("if ", p, 3))
			c = find_end(c); /* may return NULL ? */
		else if (!strn_cmp("end", p, 3))
			return c;
	}

	return c;
}


/*
* searches for valid elseif, else, or end to continue execution at.
* returns line of elseif, else, or end if found, or last line of trigger.
*/
struct cmdlist_element *find_else_end(trig_data *trig, struct cmdlist_element *cl, void *go, struct script_data *sc, int type) {
	struct cmdlist_element *c;
	char *p;

	if (!(cl->next))
		return cl;

	for (c = cl->next; c && c->next; c = c ? c->next : NULL) {
		for (p = c->cmd; *p && isspace(*p); p++); /* skip spaces */

		if (!strn_cmp("if ", p, 3))
			c = find_end(c);

		else if (!strn_cmp("elseif ", p, 7)) {
			if (process_if(p + 7, go, sc, trig, type)) {
				GET_TRIG_DEPTH(trig)++;
				return c;
			}
		}

		else if (!strn_cmp("else", p, 4)) {
			GET_TRIG_DEPTH(trig)++;
			return c;
		}

		else if (!strn_cmp("end", p, 3))
			return c;
	}

	return c;
}


/* processes any 'wait' commands in a trigger */
void process_wait(void *go, trig_data *trig, int type, char *cmd, struct cmdlist_element *cl) {
	char buf[MAX_INPUT_LENGTH], *arg;
	struct wait_event_data *wait_event_obj;
	long when, hr, min, ntime;
	char c;

	arg = any_one_arg(cmd, buf);
	skip_spaces(&arg);

	if (!*arg) {
		script_log("Trigger: %s, VNum %d. wait w/o an arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cl->cmd);
		return;
	}

	if (!strn_cmp(arg, "until ", 6)) {
		room_data *where = get_room_by_script(type, go);
		struct time_info_data tinfo = get_local_time(where);

		/* valid forms of time are 14:30 and 1430 */
		if (sscanf(arg, "until %ld:%ld", &hr, &min) == 2)
			min += (hr * 60);
		else
			min = (hr % 100) + ((hr / 100) * 60);

		/* calculate the pulse of the day of "until" time */
		ntime = (min * SECS_PER_MUD_HOUR * PASSES_PER_SEC) / 60;

		/* calculate pulse of day of current time */
		when = (main_game_pulse % (SECS_PER_MUD_HOUR * PASSES_PER_SEC)) + (tinfo.hours * SECS_PER_MUD_HOUR * PASSES_PER_SEC);

		if (when >= ntime) /* adjust for next day */
			when = (SECS_PER_MUD_DAY * PASSES_PER_SEC) - when + ntime;
		else
			when = ntime - when;
	}

	else {
		if (sscanf(arg, "%ld %c", &when, &c) == 2) {
			if (c == 't')
				when *= PASSES_PER_MUD_HOUR;
			else if (c == 's')
				when *= PASSES_PER_SEC;
		}
	}

	CREATE(wait_event_obj, struct wait_event_data, 1);
	wait_event_obj->trigger = trig;
	wait_event_obj->go = go;
	wait_event_obj->type = type;

	GET_TRIG_WAIT(trig) = dg_event_create(trig_wait_event, wait_event_obj, when);
	trig->curr_state = cl->next;
}


/* processes a script set command */
void process_set(struct script_data *sc, trig_data *trig, char *cmd) {
	char arg[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH], *value;
	
	value = any_one_arg(cmd, arg);
	value = any_one_arg(value, name);

	skip_spaces(&value);

	if (!*name) {
		script_log("Trigger: %s, VNum %d. set w/o an arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	add_var(&GET_TRIG_VARS(trig), name, value, sc ? sc->context : 0);

}

/* processes a script eval command */
void process_eval(void *go, struct script_data *sc, trig_data *trig, int type, char *cmd) {
	char arg[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH];
	char result[MAX_INPUT_LENGTH], *expr;

	expr = any_one_arg(cmd, arg); /* cut off 'eval' */
	expr = any_one_arg(expr, name); /* cut off name */

	skip_spaces(&expr);

	if (!*name) {
		script_log("Trigger: %s, VNum %d. eval w/o an arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	eval_expr(expr, result, go, sc, trig, type);
	add_var(&GET_TRIG_VARS(trig), name, result, sc ? sc->context : 0);
}


/* script attaching a trigger to something */
void process_attach(void *go, struct script_data *sc, trig_data *trig, int type, char *cmd) {
	char arg[MAX_INPUT_LENGTH], trignum_s[MAX_INPUT_LENGTH];
	char result[MAX_INPUT_LENGTH], *id_p;
	trig_data *newtrig, *proto;
	vehicle_data *v = NULL;
	char_data *c=NULL;
	obj_data *o=NULL;
	room_data *r=NULL;
	int id;

	id_p = two_arguments(cmd, arg, trignum_s);
	skip_spaces(&id_p);

	if (!*trignum_s) {
		script_log("Trigger: %s, VNum %d. attach w/o an arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	if (!id_p || !*id_p || atoi(id_p)==0) {
		script_log("Trigger: %s, VNum %d. attach invalid id arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	/* parse and locate the id specified */
	eval_expr(id_p, result, go, sc, trig, type);
	if (!(id = atoi(result))) {
		script_log("Trigger: %s, VNum %d. attach invalid id arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}
	
	if (!(c = find_char(id)) && !(v = find_vehicle(id)) && !(o = find_obj(id)) && !(r = find_room(id))) {
		script_log("Trigger: %s, VNum %d. attach invalid id arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	/* locate and load the trigger specified */
	proto = real_trigger(atoi(trignum_s));
	if (!proto || !(newtrig=read_trigger(proto->vnum))) {
		script_log("Trigger: %s, VNum %d. attach invalid trigger: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), trignum_s);
		return;
	}

	if (c) {
		if (!IS_NPC(c)) {
			script_log("Trigger: %s, VNum %d. attach invalid target: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), GET_NAME(c));
			return;
		}
		if (!SCRIPT(c)) {
			create_script_data(c, MOB_TRIGGER);
		}
		add_trigger(SCRIPT(c), newtrig, -1);
		reread_companion_trigs(c);
		request_char_save_in_world(c);
		return;
	}

	if (v) {
		if (!SCRIPT(v)) {
			create_script_data(v, VEH_TRIGGER);
		}
		add_trigger(SCRIPT(v), newtrig, -1);
		request_vehicle_save_in_world(v);
		return;
	}

	if (o) {
		if (!SCRIPT(o)) {
			create_script_data(o, OBJ_TRIGGER);
		}
		add_trigger(SCRIPT(o), newtrig, -1);
		request_obj_save_in_world(o);
		return;
	}

	if (r) {
		if (!SCRIPT(r)) {
			create_script_data(r, WLD_TRIGGER);
		}
		add_trigger(SCRIPT(r), newtrig, -1);
		request_world_save(GET_ROOM_VNUM(r), WSAVE_ROOM);
		return;
	}
}


/* script detaching a trigger from something */
void process_detach(void *go, struct script_data *sc, trig_data *trig, int type, char *cmd) {
	char arg[MAX_INPUT_LENGTH], trignum_s[MAX_INPUT_LENGTH];
	char result[MAX_INPUT_LENGTH], *id_p;
	vehicle_data *v = NULL;
	char_data *c=NULL;
	obj_data *o=NULL;
	room_data *r=NULL;
	int id;

	id_p = two_arguments(cmd, arg, trignum_s);
	skip_spaces(&id_p);

	if (!*trignum_s) {
		script_log("Trigger: %s, VNum %d. detach w/o an arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	if (!id_p || !*id_p || atoi(id_p)==0) {
		script_log("Trigger: %s, VNum %d. detach invalid id arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	/* parse and locate the id specified */
	eval_expr(id_p, result, go, sc, trig, type);
	if (!(id = atoi(result))) {
		script_log("Trigger: %s, VNum %d. detach invalid id arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}
	
	// find first good match
	if (!(c = find_char(id)) && !(v = find_vehicle(id)) && !(o = find_obj(id)) && !(r = find_room(id))) {
		script_log("Trigger: %s, VNum %d. detach invalid id arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	if (c && SCRIPT(c)) {
		if (!str_cmp(trignum_s, "all")) {
			remove_all_triggers(c, MOB_TRIGGER);
			return;
		}
		if (remove_trigger(SCRIPT(c), trignum_s)) {
			check_extract_script(c, MOB_TRIGGER);
		}
		reread_companion_trigs(c);
		return;
	}

	if (v && SCRIPT(v)) {
		if (!str_cmp(trignum_s, "all")) {
			remove_all_triggers(v, VEH_TRIGGER);
			return;
		}
		if (remove_trigger(SCRIPT(v), trignum_s)) {
			check_extract_script(v, VEH_TRIGGER);
		}
		return;
	}

	if (o && SCRIPT(o)) {
		if (!str_cmp(trignum_s, "all")) {
			remove_all_triggers(o, OBJ_TRIGGER);
			return;
		}
		if (remove_trigger(SCRIPT(o), trignum_s)) {
			check_extract_script(o, OBJ_TRIGGER);
		}
		return;
	}

	if (r && SCRIPT(r)) {
		if (!str_cmp(trignum_s, "all")) {
			remove_all_triggers(r, WLD_TRIGGER);
			return;
		}
		if (remove_trigger(SCRIPT(r), trignum_s)) {
			check_extract_script(r, WLD_TRIGGER);
		}
		return;
	}

}

room_data *dg_room_of_obj(obj_data *obj) {
	if (IN_ROOM(obj))
		return IN_ROOM(obj);
	if (obj->in_vehicle) {
		return IN_ROOM(obj->in_vehicle);
	}
	if (obj->carried_by)
		return IN_ROOM(obj->carried_by);
	if (obj->worn_by)
		return IN_ROOM(obj->worn_by);
	if (obj->in_obj)
		return (dg_room_of_obj(obj->in_obj));
	return NULL;
}


/* create a UID variable from the id number */
void makeuid_var(void *go, struct script_data *sc, trig_data *trig, int type, char *cmd) {
	char junk[MAX_INPUT_LENGTH], varname[MAX_INPUT_LENGTH];
	char arg[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH];
	char uid[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH];
	struct instance_data *inst;
	vehicle_data *veh;
	char_data *mob;

	*uid = '\0';
	half_chop(cmd, junk, temp);    /* makeuid */
	half_chop(temp, varname, cmd); /* variable name */
	half_chop(cmd, arg, temp);     /* numerical id or 'obj' 'mob' or 'room' */
	half_chop(temp, name, cmd);    /* if the above was obj, mob or room, this is the name */

	if (!*varname) {
		script_log("Trigger: %s, VNum %d. makeuid w/o an arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	if (!*arg) {
		script_log("Trigger: %s, VNum %d. makeuid invalid id arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	if (atoi(arg)!=0) { /* easy, if you pass an id number */
		char result[MAX_INPUT_LENGTH];

		eval_expr(arg, result, go, sc, trig, type);
		snprintf(uid, sizeof(uid), "%c%s", UID_CHAR, result);
	}
	else { /* a lot more work without it */
		if (!*name) {
			script_log("Trigger: %s, VNum %d. makeuid needs name: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
			return;
		}
		
		if (is_abbrev(arg, "empire")) {
			empire_data *emp = get_empire_by_name(name);
			if (emp) {
				snprintf(uid, sizeof(uid), "%c%d", UID_CHAR, EMPIRE_VNUM(emp) + EMPIRE_ID_BASE);
			}
		}
		else if (is_abbrev(arg, "mob")) {
			char_data *c = NULL;
			switch (type) {
				case WLD_TRIGGER:
				case RMT_TRIGGER:
				case BLD_TRIGGER:
				case ADV_TRIGGER:
					c = get_char_in_room((room_data*)go, name);
					break;
				case OBJ_TRIGGER:
					c = get_char_near_obj((obj_data*)go, name);
					break;
				case MOB_TRIGGER:
					c = get_char_room_vis((char_data*)go, name, NULL);
					break;
				case VEH_TRIGGER: {
					c = get_char_near_vehicle((vehicle_data*)go, name);
					break;
				}
				case EMP_TRIGGER: {
					c = get_char(name);
					break;
				}
			}
			if (c) 
				snprintf(uid, sizeof(uid), "%c%d", UID_CHAR, char_script_id(c));
		}
		else if (is_abbrev(arg, "obj")) {
			obj_data *o = NULL;
			switch (type) {
				case WLD_TRIGGER:
				case RMT_TRIGGER:
				case BLD_TRIGGER:
				case ADV_TRIGGER:
					o = get_obj_in_room((room_data*)go, name);
					break;
				case OBJ_TRIGGER:
					o = get_obj_near_obj((obj_data*)go, name);
					break;
				case MOB_TRIGGER:
					if ((o = get_obj_in_list_vis((char_data*)go, name, NULL, ((char_data*)go)->carrying)) == NULL)
						o = get_obj_in_list_vis((char_data*)go, name, NULL, ROOM_CONTENTS(IN_ROOM((char_data*)go)));
					break;
				case VEH_TRIGGER: {
					o = get_obj_near_vehicle((vehicle_data*)go, name);
					break;
				}
				case EMP_TRIGGER: {
					o = get_obj(name);
					break;
				}
			}
			if (o)
				snprintf(uid, sizeof(uid), "%c%d", UID_CHAR, obj_script_id(o));
		}
		else if (is_abbrev(arg, "room")) {
			room_data *r = NULL;
			switch (type) {
				case WLD_TRIGGER:
				case RMT_TRIGGER:
				case BLD_TRIGGER:
				case ADV_TRIGGER:
					r = (room_data*)go;
					if (*name == 'i' && isdigit(*(name+1)) && (inst = find_instance_by_room(r, FALSE, TRUE))) {
						// instance lookup
						r = find_room_template_in_instance(inst, atoi(name+1));
					}
					else if (*name) {
						r = get_room(r, name);
					}
					break;
				case OBJ_TRIGGER:
					r = obj_room((obj_data*)go);
					if (*name == 'i' && isdigit(*(name+1)) && r && (inst = find_instance_by_room(r, FALSE, TRUE))) {
						// instance lookup
						r = find_room_template_in_instance(inst, atoi(name+1));
					}
					else if (*name) {
						r = get_room(r, name);
					}
					break;
				case MOB_TRIGGER:
					mob = (char_data *)go;
					r = IN_ROOM(mob);
					if (*name == 'i' && isdigit(*(name+1)) && MOB_INSTANCE_ID(mob) != NOTHING && (inst = get_instance_by_id(MOB_INSTANCE_ID(mob)))) {
						// instance lookup
						r = find_room_template_in_instance(inst, atoi(name+1));
					}
					else if (*name) {
						r = get_room(r, name);
					}
					break;
				case VEH_TRIGGER: {
					veh = (vehicle_data*)go;
					if (*name == 'i' && isdigit(*(name+1)) && VEH_INSTANCE_ID(veh) != NOTHING && (inst = get_instance_by_id(VEH_INSTANCE_ID(veh)))) {
						// instance lookup
						r = find_room_template_in_instance(inst, atoi(name+1));
					}
					else if (*name) {
						r = get_room(r, name);
					}
					break;
				}
				case EMP_TRIGGER: {
					r = get_room(NULL, name);
					break;
				}
			}
			if (r) {
				snprintf(uid, sizeof(uid), "%c%d", UID_CHAR, GET_ROOM_VNUM(r) + ROOM_ID_BASE);
			}
		}
		else if (is_abbrev(arg, "vehicle")) {
			vehicle_data *v = NULL;
			switch (type) {
				case WLD_TRIGGER:
				case RMT_TRIGGER:
				case BLD_TRIGGER:
				case ADV_TRIGGER: {
					v = get_vehicle_room((room_data*)go, name, NULL);
					break;
				}
				case OBJ_TRIGGER: {
					v = get_vehicle_near_obj((obj_data*)go, name);
					break;
				}
				case MOB_TRIGGER: {
					v = get_vehicle_in_room_vis((char_data*)go, name, NULL);
					break;
				}
				case VEH_TRIGGER: {
					v = get_vehicle_near_vehicle((vehicle_data*)go, name);
					break;
				}
				case EMP_TRIGGER: {
					v = get_vehicle(name);
					break;
				}
			}
			if (v) {
				snprintf(uid, sizeof(uid), "%c%d", UID_CHAR, veh_script_id(v));
			}
		}
		else {
			script_log("Trigger: %s, VNum %d. makeuid syntax error: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
			return;
			}
		}
	if (*uid)
		add_var(&GET_TRIG_VARS(trig), varname, uid, sc ? sc->context : 0);
}

/*
* processes a script return command.
* returns the new value for the script to return.
*/
int process_return(trig_data *trig, char *cmd) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

	two_arguments(cmd, arg1, arg2);

	if (!*arg2) {
		script_log("Trigger: %s, VNum %d. return w/o an arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return 1;
	}

	return atoi(arg2);
}


/*
* removes a variable from the global vars of sc,
* or the local vars of trig if not found in global list.
*/
void process_unset(struct script_data *sc, trig_data *trig, char *cmd) {
	char arg[MAX_INPUT_LENGTH], *var;

	var = any_one_arg(cmd, arg);

	skip_spaces(&var);

	if (!*var) {
		script_log("Trigger: %s, VNum %d. unset w/o an arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	if (!remove_var(&(sc->global_vars), var, sc->context))
		remove_var(&GET_TRIG_VARS(trig), var, sc->context);
}


/*
* copy a locally owned variable to the globals of another script
*     'remote <variable_name> <uid>'
*/
void process_remote(struct script_data *sc, trig_data *trig, char *cmd) {
	struct trig_var_data *vd;
	struct script_data *sc_remote=NULL;
	char *line, *var, *uid_p;
	char arg[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
	int uid, context;
	vehicle_data *veh;
	empire_data *emp;
	room_data *room;
	char_data *mob;
	obj_data *obj;

	line = any_one_arg(cmd, arg);
	line = any_one_arg(line, buf);
	line = any_one_arg(line, buf2);
	var = buf;
	uid_p = buf2;
	skip_spaces(&var);
	skip_spaces(&uid_p);


	if (!*buf || !*buf2) {
		script_log("Trigger: %s, VNum %d. remote: invalid arguments '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	/* find the locally owned variable */
	for (vd = GET_TRIG_VARS(trig); vd; vd = vd->next)
		if (!str_cmp(vd->name, buf))
			break;

	if (!vd)
		for (vd = sc->global_vars; vd; vd = vd->next)
			if (!str_cmp(vd->name, var) && (vd->context==0 || vd->context==sc->context))
				break; 

	if (!vd) {
		script_log("Trigger: %s, VNum %d. local var '%s' not found in remote call", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), buf);
		return;
	}    

	/* find the target script from the uid number */
	uid = atoi(buf2);
	if (uid<=0) {
		script_log("Trigger: %s, VNum %d. remote: illegal uid '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), buf2);
		return;
	}

	/* for all but PC's, context comes from the existing context. */
	/* for PC's, context is 0 (global) */
	context = vd->context;

	if ((room = find_room(uid))) {
		sc_remote = SCRIPT(room) ? SCRIPT(room) : create_script_data(room, WLD_TRIGGER);
		request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	}
	else if ((mob = find_char(uid))) {
		sc_remote = SCRIPT(mob) ? SCRIPT(mob) : create_script_data(mob, MOB_TRIGGER);
		if (!IS_NPC(mob))
			context = 0;
		add_companion_var(mob, vd->name, vd->value, context);
		request_char_save_in_world(mob);
	}
	else if ((obj = find_obj(uid))) {
		sc_remote = SCRIPT(obj) ? SCRIPT(obj) : create_script_data(obj, OBJ_TRIGGER);
		request_obj_save_in_world(obj);
	}
	else if ((veh = find_vehicle(uid))) {
		sc_remote = SCRIPT(veh) ? SCRIPT(veh) : create_script_data(veh, VEH_TRIGGER);
		request_vehicle_save_in_world(veh);
	}
	else if ((emp = find_empire_by_uid(uid))) {
		sc_remote = SCRIPT(emp) ? SCRIPT(emp) : create_script_data(emp, EMP_TRIGGER);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
	else {
		script_log("Trigger: %s, VNum %d. remote: uid '%d' invalid", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), uid);
		return;
	}

	if (!sc_remote) {
		return; // nothing to assign to
	}

	add_var(&(sc_remote->global_vars), vd->name, vd->value, context);
}


/*
* command-line interface to rdelete
* named vdelete so people didn't think it was to delete rooms
*/
ACMD(do_vdelete) {
	struct trig_var_data *vd;
	struct script_data *sc_remote=NULL;
	char *var, *uid_p;
	char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
	int uid;
	// long context;	// unused
	vehicle_data *veh;
	empire_data *emp;
	room_data *room;
	char_data *mob;
	obj_data *obj;

	argument = any_one_arg(argument, buf);
	argument = any_one_arg(argument, buf2);
	var = buf;
	uid_p = buf2;
	skip_spaces(&var);
	skip_spaces(&uid_p);


	if (!*buf || !*buf2) {
		msg_to_char(ch, "Usage: vdelete <variablename> <id>\r\n");
		return;
	}

	/* find the target script from the uid number */
	uid = atoi(buf2);
	if (uid<=0) {
		msg_to_char(ch, "vdelete: illegal id specified.\r\n");
		return;
	}

	if ((room = find_room(uid))) {
		sc_remote = SCRIPT(room);
	}
	else if ((mob = find_char(uid))) {
		sc_remote = SCRIPT(mob);
		remove_companion_var(mob, var, 0);
		/*
		// this was set but never used...
		if (!IS_NPC(mob))
			context = 0;
		*/
	}
	else if ((obj = find_obj(uid))) {
		sc_remote = SCRIPT(obj);
	}
	else if ((veh = find_vehicle(uid))) {
		sc_remote = SCRIPT(veh);
	}
	else if ((emp = find_empire_by_uid(uid))) {
		sc_remote = SCRIPT(emp);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
	else {
		msg_to_char(ch, "vdelete: cannot resolve specified id.\r\n");
		return;
	}

	if (sc_remote==NULL) {
		msg_to_char(ch, "That id represents no global variables.(1)\r\n");
		return;
	}

	if (sc_remote->global_vars==NULL) {
		msg_to_char(ch, "That id represents no global variables.(2)\r\n");
		return;
	}

	/* find the global */
	for (vd = sc_remote->global_vars; vd; vd = vd->next)
		if (!str_cmp(vd->name, var))
			break;

	if (!vd) {
		msg_to_char(ch, "That variable cannot be located.\r\n");
		return;
	}

	/* ok, delete the variable */
	LL_DELETE(sc_remote->global_vars, vd);

	/* and free up the space */
	free(vd->value);
	free(vd->name);
	free(vd);
	
	request_world_save_by_script(sc_remote->attached_to, sc_remote->attached_type);

	msg_to_char(ch, "Deleted.\r\n");
}

/*
* delete a variable from the globals of another script
*     'rdelete <variable_name> <uid>'
*/
void process_rdelete(struct script_data *sc, trig_data *trig, char *cmd) {
	struct trig_var_data *vd;
	struct script_data *sc_remote=NULL;
	char *line, *var, *uid_p;
	char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
	int uid;
	// long context;	// unused
	vehicle_data *veh;
	empire_data *emp;
	room_data *room;
	char_data *mob;
	obj_data *obj;

	line = any_one_arg(cmd, arg);
	line = any_one_arg(line, buf);
	line = any_one_arg(line, buf2);
	var = buf;
	uid_p = buf2;
	skip_spaces(&var);
	skip_spaces(&uid_p);


	if (!*buf || !*buf2) {
		script_log("Trigger: %s, VNum %d. rdelete: invalid arguments '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	/* find the target script from the uid number */
	uid = atoi(buf2);
	if (uid<=0) {
		script_log("Trigger: %s, VNum %d. rdelete: illegal uid '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), buf2);
		return;
	}

	if ((room = find_room(uid))) {
		sc_remote = SCRIPT(room);
		request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	}
	else if ((mob = find_char(uid))) {
		sc_remote = SCRIPT(mob);
		remove_companion_var(mob, var, sc->context);
		request_char_save_in_world(mob);
		/*
		// this was set but never used
		if (!IS_NPC(mob))
			context = 0;
		*/
	}
	else if ((obj = find_obj(uid))) {
		sc_remote = SCRIPT(obj);
		request_obj_save_in_world(obj);
	}
	else if ((veh = find_vehicle(uid))) {
		sc_remote = SCRIPT(veh);
		request_vehicle_save_in_world(veh);
	}
	else if ((emp = find_empire_by_uid(uid))) {
		sc_remote = SCRIPT(emp);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
	else {
		script_log("Trigger: %s, VNum %d. remote: uid '%d' invalid", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), uid);
		return;
	}

	if (sc_remote==NULL)
		return; /* no script to delete a trigger from */
	if (sc_remote->global_vars==NULL)
		return; /* no script globals */

	/* find the global */
	for (vd = sc_remote->global_vars; vd; vd = vd->next)
		if (!str_cmp(vd->name, var) && (vd->context==0 || vd->context==sc->context))
			break;

	if (!vd)
		return; /* the variable doesn't exist, or is the wrong context */

	/* ok, delete the variable */
	LL_DELETE(sc_remote->global_vars, vd);

	/* and free up the space */
	free(vd->value);
	free(vd->name);
	free(vd);
}


/*
* makes a local variable into a global variable
*/
void process_global(struct script_data *sc, trig_data *trig, char *cmd, int id) {
	struct trig_var_data *vd;
	char arg[MAX_INPUT_LENGTH], *var;

	var = any_one_arg(cmd, arg);

	skip_spaces(&var);

	if (!*var) {
		script_log("Trigger: %s, VNum %d. global w/o an arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	for (vd = GET_TRIG_VARS(trig); vd; vd = vd->next)
		if (!str_cmp(vd->name, var))
			break;

	if (!vd) {
		script_log("Trigger: %s, VNum %d. local var '%s' not found in global call", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), var);
		return;
	}    

	add_var(&(sc->global_vars), vd->name, vd->value, id);
	
	// formerly: remove_var(&GET_TRIG_VARS(trig), vd->name, id);
	LL_DELETE(GET_TRIG_VARS(trig), vd);
	free_var_el(vd);
}


/* set the current context for a script */
void process_context(struct script_data *sc, trig_data *trig, char *cmd) {
	char arg[MAX_INPUT_LENGTH], *var;

	var = any_one_arg(cmd, arg);

	skip_spaces(&var);

	if (!*var) {
		script_log("Trigger: %s, VNum %d. context w/o an arg: '%s'", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), cmd);
		return;
	}

	sc->context = atol(var);
}

void extract_value(struct script_data *sc, trig_data *trig, char *cmd) {
	char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH];
	char *buf3;
	char to[128];
	int num;

	buf3 = any_one_arg(cmd, buf);
	half_chop(buf3, buf2, buf);
	strcpy(to, buf2);

	num = atoi(buf);
	if (num < 1) {
		script_log("extract number < 1!");
		return;
	}

	half_chop(buf, buf3, buf2);

	while (num>0) {
		half_chop(buf2, buf, temp);
		strcpy(buf2, temp);
		num--;
	}

	add_var(&GET_TRIG_VARS(trig), to, buf, sc ? sc->context : 0);
}

/*  This is the core driver for scripts. */
//int script_driver(void **go_adress, trig_data *trig, int type, int mode)
int script_driver(union script_driver_data_u *sdd, trig_data *trig, int type, int mode) {
	static int depth = 0;
	int ret_val = 1;
	struct cmdlist_element *cl;
	char cmd[MAX_INPUT_LENGTH], *p;
	struct script_data *sc = 0;
	struct cmdlist_element *temp;
	// unsigned long loops = 0;
	void *go = NULL;

	if (depth > MAX_SCRIPT_DEPTH) {
		script_log("Triggers recursed beyond maximum allowed depth.");
		return ret_val;
	}

	depth++;

	switch (type) {
		case MOB_TRIGGER:
			go = sdd->c;
			sc = SCRIPT((char_data*) go);
			create_dg_owner_purged_tracker(trig, (char_data*) go, NULL, NULL, NULL);
			break;
		case OBJ_TRIGGER:
			go = sdd->o;
			sc = SCRIPT((obj_data*) go);
			create_dg_owner_purged_tracker(trig, NULL, (obj_data*) go, NULL, NULL);
			break;
		case WLD_TRIGGER:
		case RMT_TRIGGER:
		case BLD_TRIGGER:
		case ADV_TRIGGER:
			go = sdd->r;
			sc = SCRIPT((room_data*) go);
			create_dg_owner_purged_tracker(trig, NULL, NULL, (room_data*) go, NULL);
			break;
		case VEH_TRIGGER: {
			go = sdd->v;
			sc = SCRIPT((vehicle_data*) go);
			create_dg_owner_purged_tracker(trig, NULL, NULL, NULL, (vehicle_data*) go);
			break;
		}
		case EMP_TRIGGER: {
			script_log("Attempting to run unsupported script on empire (trig: %d)", GET_TRIG_VNUM(trig));
			break;
		}
	}
	
	if (!sc) {
		script_log("Attempting to run script without script data (trig: %d)", GET_TRIG_VNUM(trig));
		return ret_val;
	}

	if (mode == TRIG_NEW) {
		GET_TRIG_DEPTH(trig) = 1;
		GET_TRIG_LOOPS(trig) = 0;
		sc->context = 0;
	}

	for (cl = (mode == TRIG_NEW) ? trig->cmdlist : trig->curr_state; cl && GET_TRIG_DEPTH(trig); cl = cl ? cl->next : NULL) {
		for (p = cl->cmd; *p && isspace(*p); p++);

		if (*p == '*') /* comment */
			continue;

		else if (!strn_cmp(p, "if ", 3)) {
			if (process_if(p + 3, go, sc, trig, type))
				GET_TRIG_DEPTH(trig)++;
			else
				cl = find_else_end(trig, cl, go, sc, type);
		}

		else if (!strn_cmp("elseif ", p, 7) || !strn_cmp("else", p, 4)) {
			/*
			* if not in an if-block, ignore the extra 'else[if]' and warn about it
			*/
			if (GET_TRIG_DEPTH(trig) == 1) { 
				script_log("Trigger VNum %d has 'else' without 'if'.", 
				GET_TRIG_VNUM(trig));
				continue; 
			}
			cl = find_end(cl);
			GET_TRIG_DEPTH(trig)--;
		}
		else if (!strn_cmp("while ", p, 6)) {
			temp = find_done(cl);  
			if (!temp) {
				script_log("Trigger VNum %d has 'while' without 'done'.", GET_TRIG_VNUM(trig));
				return ret_val;
			}
			if (process_if(p + 6, go, sc, trig, type)) {
				temp->original = cl;
			}
			else {
				cl = temp;
				// loops = 0;
			}
		}
		else if (!strn_cmp("switch ", p, 7)) {
			cl = find_case(trig, cl, go, sc, type, p + 7);
		}
		else if (!strn_cmp("end", p, 3)) {   
			/*
			* if not in an if-block, ignore the extra 'end' and warn about it.
			*/
			if (GET_TRIG_DEPTH(trig) == 1) { 
				script_log("Trigger VNum %d has 'end' without 'if'.", GET_TRIG_VNUM(trig));
				continue; 
			}
			GET_TRIG_DEPTH(trig)--;
		}
		else if (!strn_cmp("done", p, 4)) {
			/* if in a while loop, cl->original is non-NULL */
			if (cl->original) {
				char *orig_cmd = cl->original->cmd;
				while (*orig_cmd && isspace(*orig_cmd))
					orig_cmd++;
				if (cl->original && process_if(orig_cmd + 6, go, sc, trig, type)) {
					cl = cl->original;
					// loops++;   
					GET_TRIG_LOOPS(trig)++;
					/* This actually breaks multiple types of triggers -kh
					if (loops == 30) {
						process_wait(go, trig, type, "wait 1", cl);
						depth--;
						return ret_val;
					}
					*/
					if (GET_TRIG_LOOPS(trig) > MAX(300, max_inventory_size)) {
						script_log("Trigger VNum %d has looped %d times!!!", GET_TRIG_VNUM(trig), MAX(300, max_inventory_size));
						break;
					}
				}
				else {
				/* if we're falling through a switch statement, this ends it. */
				}
			}
		}
		else if (!strn_cmp("break", p, 5)) {
			cl = find_done(cl);
		}
		else if (!strn_cmp("case", p, 4)) { 
			/* Do nothing, this allows multiple cases to a single instance */
		}

		else {
			var_subst(go, sc, trig, type, p, cmd);

			if (!strn_cmp(cmd, "eval ", 5))
				process_eval(go, sc, trig, type, cmd);

			else if (!strn_cmp(cmd, "nop ", 4)); /* nop: do nothing */

			else if (!strn_cmp(cmd, "extract ", 8))
				extract_value(sc, trig, cmd);

			else if (!strn_cmp(cmd, "makeuid ", 8))
				makeuid_var(go, sc, trig, type, cmd);

			else if (!strn_cmp(cmd, "halt", 4))
				break;

			else if (!strn_cmp(cmd, "dg_affect ", 10))
				do_dg_affect(go, sc, trig, type, cmd);
			else if (!strn_cmp(cmd, "dg_affect_room ", 15))
				do_dg_affect_room(go, sc, trig, type, cmd);

			else if (!strn_cmp(cmd, "global ", 7)) {
				process_global(sc, trig, cmd, sc->context);
				request_world_save_by_script(go, type);
			}

			else if (!strn_cmp(cmd, "context ", 8))
				process_context(sc, trig, cmd);

			else if (!strn_cmp(cmd, "remote ", 7))
				process_remote(sc, trig, cmd);

			else if (!strn_cmp(cmd, "rdelete ", 8))
				process_rdelete(sc, trig, cmd);

			else if (!strn_cmp(cmd, "return ", 7))
				ret_val = process_return(trig, cmd);

			else if (!strn_cmp(cmd, "set ", 4))
				process_set(sc, trig, cmd);

			else if (!strn_cmp(cmd, "unset ", 6)) {
				process_unset(sc, trig, cmd);
				request_world_save_by_script(go, type);
			}

			else if (!strn_cmp(cmd, "wait ", 5)) {
				process_wait(go, trig, type, cmd, cl);
				depth--;
				return ret_val;
			}

			else if (!strn_cmp(cmd, "attach ", 7))
				process_attach(go, sc, trig, type, cmd);

			else if (!strn_cmp(cmd, "detach ", 7))
				process_detach(go, sc, trig, type, cmd);

			else if (!strn_cmp(cmd, "version", 7))
				syslog(SYS_OLC, LVL_GOD, TRUE, "%s", DG_SCRIPT_VERSION);

			else {
				switch (type) {
					case MOB_TRIGGER:
						command_interpreter((char_data*) go, cmd);
						break;
					case OBJ_TRIGGER:
						obj_command_interpreter((obj_data*) go, cmd);
						break;
					case WLD_TRIGGER:
					case RMT_TRIGGER:
					case BLD_TRIGGER:
					case ADV_TRIGGER:
						wld_command_interpreter((room_data*) go, cmd);
						break;
					case VEH_TRIGGER: {
						vehicle_command_interpreter((vehicle_data*) go, cmd);
						break;
					}
					case EMP_TRIGGER: {
						// unsupported type
						break;
					}
				}
			}
		}
		
		// escape when owner purged
		if (trig->purge_tracker && trig->purge_tracker->purged) {
			--depth;
			if (type == OBJ_TRIGGER) {
				sdd->o = NULL;
			}
			return ret_val;
		}
	}

	switch (type) { /* the script may have been detached */
		case MOB_TRIGGER:
			sc = SCRIPT((char_data*) go);
			break;
		case OBJ_TRIGGER:
			sc = SCRIPT((obj_data*) go);
			break;
		case WLD_TRIGGER:
		case RMT_TRIGGER:
		case BLD_TRIGGER:
		case ADV_TRIGGER:
			sc = SCRIPT((room_data*) go);
			break;
		case VEH_TRIGGER: {
			sc = SCRIPT((vehicle_data*) go);
			break;
		}
		case EMP_TRIGGER: {
			sc = SCRIPT((empire_data*) go);
			break;
		}
	}
	
	// this formerly only called free_varlist "if (sc)" but that seems like a memory leak
	free_varlist(GET_TRIG_VARS(trig));
	GET_TRIG_VARS(trig) = NULL;
	GET_TRIG_DEPTH(trig) = 0;
	cancel_dg_owner_purged_tracker(trig);

	depth--;
	return ret_val;
}

/* returns the trigger prototype with given virtual number */
trig_data *real_trigger(trig_vnum vnum) {
	trig_data *trig;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(trigger_table, &vnum, trig);
	return trig;
}


/**
* Quick way to turn a vnum into a name, safely.
*
* @param trig_vnum vnum The vnum to look up.
* @return char* A name for the vnum, or "UNKNOWN".
*/
char *get_trigger_name_by_proto(trig_vnum vnum) {
	trig_data *proto = real_trigger(vnum);
	char *unk = "UNKNOWN";
	
	return proto ? GET_TRIG_NAME(proto) : unk;
}


/*
* scans for a case/default instance
* returns the line containg the correct case instance, or the last
* line of the trigger if not found.
*/
struct cmdlist_element *find_case(trig_data *trig, struct cmdlist_element *cl, void *go, struct script_data *sc, int type, char *cond) {
	char result[MAX_INPUT_LENGTH];
	struct cmdlist_element *c;
	char *p, *buf;

	eval_expr(cond, result, go, sc, trig, type);

	if (!(cl->next))
		return cl;  

	for (c = cl->next; c->next; c = c->next) {
		for (p = c->cmd; *p && isspace(*p); p++);

		if (!strn_cmp("while ", p, 6) || !strn_cmp("switch", p, 6))
			c = find_done(c);
		else if (!strn_cmp("case ", p, 5)) {
			buf = (char*)malloc(MAX_STRING_LENGTH);
			eval_op("==", result, p + 5, buf, go, sc, trig);
			if (*buf && *buf!='0') {
				free(buf);
				return c;
			}
			free(buf);
		}
		else if (!strn_cmp("default", p, 7))
			return c;
		else if (!strn_cmp("done", p, 3))   
			return c;
	}
	return c;
}        

/*
* scans for end of while/switch-blocks.   
* returns the line containg 'end', or the last
* line of the trigger if not found.     
* Malformed scripts may cause NULL to be returned.
*/
struct cmdlist_element *find_done(struct cmdlist_element *cl) {
	struct cmdlist_element *c;
	char *p;

	if (!cl || !(cl->next))
		return cl;

	for (c = cl->next; c && c->next; c = c->next) {
		for (p = c->cmd; *p && isspace(*p); p++);

		if (!strn_cmp("while ", p, 6) || !strn_cmp("switch ", p, 7))
			c = find_done(c);
		else if (!strn_cmp("done", p, 3))
			return c;
	}

	return c;
}
