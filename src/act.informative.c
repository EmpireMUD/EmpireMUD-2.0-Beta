/* ************************************************************************
*   File: act.informative.c                               EmpireMUD 2.0b5 *
*  Usage: Player-level commands of an informative nature                  *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "dg_scripts.h"
#include "vnums.h"
#include "constants.h"
#include "boards.h"
#include "olc.h"

/**
* Contents:
*   Helpers
*   Chart Functions
*   Look Assist Functions
*   Character Display Functions
*   Object Display Functions
*   Who List Parts
*   Commands
*/

// external protos
ACMD(do_weather);

void mudstats_configs(char_data *ch, char *argument);
void mudstats_empires(char_data *ch, char *argument);
void mudstats_time(char_data *ch, char *argument);

// local protos
ACMD(do_affects);
void list_one_char(char_data *i, char_data *ch, int num);
void look_at_char(char_data *i, char_data *ch, bool show_eq);
void look_in_obj(char_data *ch, char *arg, obj_data *obj, vehicle_data *veh);
void show_one_stored_item_to_char(char_data *ch, empire_data *emp, struct empire_storage_data *store, bool show_zero);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Shows a tip-of-the-day to a character.
*
* @param char_data *ch The person to show a tip to.
*/
void display_tip_to_char(char_data *ch) {
	int pos;

	if (IS_NPC(ch)) {
		pos = number(0, tips_of_the_day_size-1);
	}
	else {
		GET_LAST_TIP(ch) += 1;
		if (GET_LAST_TIP(ch) >= tips_of_the_day_size) {
			GET_LAST_TIP(ch) = 0;
		}
		pos = GET_LAST_TIP(ch);
	}

	if (tips_of_the_day_size > 0) {
		msg_to_char(ch, "&yTip: %s&0\r\n", tips_of_the_day[pos]);
	}
	else {
		msg_to_char(ch, "No tips are available.\r\n");
	}
}


/**
* Attempts to find a matching extra description in a list.
*
* @param char *word First word of argument.
* @param struct extra_descr_data *list Pointer to the start of an exdesc list.
* @param int *number Optional: For counting #.name syntax across lists.
* @return char* A pointer to the description to show, or NULL if no match.
*/
char *find_exdesc(char *word, struct extra_descr_data *list, int *number) {
	struct extra_descr_data *i;

	for (i = list; i; i = i->next) {
		if (isname(word, i->keyword) && (!number || --(*number) == 0)) {
			return (i->description);
		}
	}

	return (NULL);
}


/**
* Finds an extra description around 'ch' that matches the given word, if any.
* Also binds the thing that had the description, if any. If you pass the
* 'number' parameter, this will correctly decrement the number as it matches
* descriptions.
*
* @param char_data *ch The person looking.
* @param char *word The keyword given.
* @param int *number Optional: Pointer to the counter for 'look 3.sign' etc (may be NULL).
* @param obj_data **found_obj Optional: If the description is on an object, it will be passed back here (may be NULL).
* @param vehicle_data **found_veh Optional: If the description is on an vehicle, it will be passed back here (may be NULL).
* @param room_data **found_room Optional: If the description is on the room, it will be passed back here (may be NULL).
* @return char* The description to show, if one was found.
*/
char *find_exdesc_for_char(char_data *ch, char *word, int *number, obj_data **found_obj, vehicle_data **found_veh, room_data **found_room) {
	int pos, fnum;
	char *exdesc;
	obj_data *obj;
	vehicle_data *veh;
	
	if (found_obj) {
		*found_obj = NULL;
	}
	if (found_veh) {
		*found_veh = NULL;
	}
	if (found_room) {
		*found_room = NULL;
	}
	
	if (!ch || !word || !*word) {
		return NULL;
	}
	
	// did we get a number
	if (!number) {
		fnum = get_number(&word);
		number = &fnum;
	}
	
	// check inventory
	DL_FOREACH2(ch->carrying, obj, next_content) {
		if (CAN_SEE_OBJ(ch, obj) && (exdesc = find_exdesc(word, GET_OBJ_EX_DESCS(obj), number)) != NULL) {
			if (found_obj) {
				*found_obj = obj;
			}
			return exdesc;
		}
	}
	
	// check equipment
	for (pos = 0; pos < NUM_WEARS; ++pos) {
		if (GET_EQ(ch, pos) && CAN_SEE_OBJ(ch, GET_EQ(ch, pos)) && (exdesc = find_exdesc(word, GET_OBJ_EX_DESCS(GET_EQ(ch, pos)), number)) != NULL) {
			if (found_obj) {
				*found_obj = GET_EQ(ch, pos);
			}
			return exdesc;
		}
	}
	
	// check objects in the room
	DL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_content) {
		if (CAN_SEE_OBJ(ch, obj) && (exdesc = find_exdesc(word, GET_OBJ_EX_DESCS(obj), number)) != NULL) {
			if (found_obj) {
				*found_obj = obj;
			}
			return exdesc;
		}
	}
	
	// check vehicles
	DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
		if (!VEH_IS_EXTRACTED(veh) && CAN_SEE_VEHICLE(ch, veh) && (exdesc = find_exdesc(word, VEH_EX_DESCS(veh), number)) != NULL) {
			if (found_veh) {
				*found_veh = veh;
			}
			return exdesc;
		}
	}
	
	// does it match an extra desc of the room template?
	if (GET_ROOM_TEMPLATE(IN_ROOM(ch))) {
		if ((exdesc = find_exdesc(word, GET_RMT_EX_DESCS(GET_ROOM_TEMPLATE(IN_ROOM(ch))), number)) != NULL) {
			if (found_room) {
				*found_room = IN_ROOM(ch);
			}
			return exdesc;
		}
	}

	// does it match an extra desc of the building?
	if (GET_BUILDING(IN_ROOM(ch))) {
		if ((exdesc = find_exdesc(word, GET_BLD_EX_DESCS(GET_BUILDING(IN_ROOM(ch))), number)) != NULL) {
			if (found_room) {
				*found_room = IN_ROOM(ch);
			}
			return exdesc;
		}
	}
	
	// does it match an extra desc on the crop?
	if (ROOM_CROP(IN_ROOM(ch)) && ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_CROP)) {
		if ((exdesc = find_exdesc(word, GET_CROP_EX_DESCS(ROOM_CROP(IN_ROOM(ch))), number)) != NULL) {
			if (found_room) {
				*found_room = IN_ROOM(ch);
			}
			return exdesc;
		}
	}
	
	// does it match an extra desc on the sector?
	if ((exdesc = find_exdesc(word, GET_SECT_EX_DESCS(SECT(IN_ROOM(ch))), number)) != NULL) {
		if (found_room) {
			*found_room = IN_ROOM(ch);
		}
		return exdesc;
	}
	
	// no?
	return NULL;
}


/**
* Gets the list of a character's class-level skills, for use on who/whois.
*
* @param char_data *ch The player.
* @param char *buffer The string to save to.
* @param bool abbrev If TRUE, gets the short version.
*/
void get_player_skill_string(char_data *ch, char *buffer, bool abbrev) {
	struct player_skill_data *plsk, *next_plsk;
	
	*buffer = '\0';
	
	if (IS_IMMORTAL(ch)) {
		strcpy(buffer, abbrev ? "Imm" : "Immortal");
	}
	else if (!IS_NPC(ch)) {
		HASH_ITER(hh, GET_SKILL_HASH(ch), plsk, next_plsk) {
			if (plsk->level >= MAX_SKILL_CAP) {
				sprintf(buffer + strlen(buffer), "%s%s", (*buffer ? (abbrev ? "/" : ", ") : ""), abbrev ? SKILL_ABBREV(plsk->ptr) : SKILL_NAME(plsk->ptr));
			}
		}
	}
	
	if (!*buffer) {
		strcpy(buffer, abbrev ? "New" : "Newbie");
	}
}


/**
* Gets the instance's best-format level range in a string like "level 10" or
* "levels 50-75" -- this will change e.g. if the instance is scaled.
*
* @param struct instance_data *inst The instance to get level info for.
* @return char* The level text.
*/
char *instance_level_string(struct instance_data *inst) {
	static char output[256];
	
	*output = '\0';
	
	// sanitation
	if (!inst) {
		return output;
	}
	
	if (INST_LEVEL(inst)) {
		snprintf(output, sizeof(output), "level %d", INST_LEVEL(inst));
	}
	else if (GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)) > 0 && GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)) == 0) {
		snprintf(output, sizeof(output), "levels %d+", GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)));
	}
	else if (GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)) == 0 && GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)) > 0) {
		snprintf(output, sizeof(output), "levels 1-%d", GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)));
	}
	else if (GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)) == GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst))) {
		if (GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)) == 0) {
			snprintf(output, sizeof(output), "any level");
		}
		else {
			snprintf(output, sizeof(output), "level %d", GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)));
		}
	}
	else {
		snprintf(output, sizeof(output), "levels %d-%d", GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)), GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)));
	}
	
	return output;
}


// for nearby sorting
struct nearby_item_t {
	char *text;
	int distance;
	struct nearby_item_t *prev, *next;
};

// simple sorter for nearby
int nrb_sort_distance(struct nearby_item_t *a, struct nearby_item_t *b) {
	return a->distance - b->distance;
}


/**
* Picks a random custom long description from a player's equipment.
*
* @param char_data *ch The player.
* @return struct custom_message* The chosen message, or NULL.
*/
struct custom_message *pick_custom_longdesc(char_data *ch) {
	struct custom_message *ocm, *found = NULL;
	int iter, count = 0;
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (!GET_EQ(ch, iter) || !wear_data[iter].allow_custom_msgs) {
			continue;
		}
		
		LL_FOREACH(GET_OBJ_CUSTOM_MSGS(GET_EQ(ch, iter)), ocm) {
			if (ocm->type != OBJ_CUSTOM_LONGDESC && ocm->type != OBJ_CUSTOM_LONGDESC_FEMALE && ocm->type != OBJ_CUSTOM_LONGDESC_MALE) {
				continue;
			}
			if (ocm->type == OBJ_CUSTOM_LONGDESC_FEMALE && GET_SEX(ch) != SEX_FEMALE) {
				continue;
			}
			if (ocm->type == OBJ_CUSTOM_LONGDESC_MALE && GET_SEX(ch) != SEX_MALE && GET_SEX(ch) != SEX_NEUTRAL) {
				continue;
			}
			
			// matching
			if (!number(0, count++) || !found) {
				found = ocm;
			}
		}
	}
	
	return found;
}


// quick alpha sorter for the passives command
int sort_passives(struct affected_type *a, struct affected_type *b) {
	return strcmp(get_ability_name_by_vnum(a->type), get_ability_name_by_vnum(b->type));
}


 //////////////////////////////////////////////////////////////////////////////
//// CHART FUNCTIONS /////////////////////////////////////////////////////////

// temporary helper type for do_chart
struct chart_territory {
	any_vnum empire_id;	// for quick hashing
	empire_data *emp;	// which empire
	int tiles;	// count of territory
	struct empire_city_data *largest_city;	// only realy need 1 per empire
	
	UT_hash_handle hh;	// hashed by id
};


/**
* Marks a city for the empire (only the largest).
*
* @param struct chart_territory **hash Pointer to the hash to add to.
* @param empire_data *emp The empire with territory.
* @param struct empire_city_data *city The city to add (if largest).
*/
void chart_add_city(struct chart_territory **hash, empire_data *emp, struct empire_city_data *city) {
	any_vnum eid = EMPIRE_VNUM(emp);
	struct chart_territory *ct;
	
	HASH_FIND_INT(*hash, &eid, ct);
	if (!ct) {
		CREATE(ct, struct chart_territory, 1);
		ct->empire_id = eid;
		ct->emp = emp;
		
		HASH_ADD_INT(*hash, empire_id, ct);
	}
	
	// is it the largest city?
	if (!ct->largest_city || city_type[ct->largest_city->type].radius < city_type[city->type].radius) {
		ct->largest_city = city;
	}
}


/**
* Marks territory for the empire.
*
* @param struct chart_territory **hash Pointer to the hash to add to.
* @param empire_data *emp The empire with territory.
* @param int amount How much territory.
*/
void chart_add_territory(struct chart_territory **hash, empire_data *emp, int amount) {
	any_vnum eid = EMPIRE_VNUM(emp);
	struct chart_territory *ct;
	
	HASH_FIND_INT(*hash, &eid, ct);
	if (!ct) {
		CREATE(ct, struct chart_territory, 1);
		ct->empire_id = eid;
		ct->emp = emp;
		
		HASH_ADD_INT(*hash, empire_id, ct);
	}
	
	ct->tiles = MAX(ct->tiles, amount);
}


/**
* Free up the memory of the temporary chart data.
*
* @param struct chart_territory *hash The data to free.
*/
void free_chart_hash(struct chart_territory *hash) {
	struct chart_territory *ct, *next_ct;
	
	HASH_ITER(hh, hash, ct, next_ct) {
		HASH_DEL(hash, ct);
		free(ct);
	}
}


// Simple vnum sorter for do_chart
int sort_chart_hash(struct chart_territory *a, struct chart_territory *b) {
	return b->tiles - a->tiles;
}


 //////////////////////////////////////////////////////////////////////////////
//// LOOK ASSIST FUNCTIONS ///////////////////////////////////////////////////

/**
* Given the argument "look at <target>", figure out what object or char
* matches the target.  First, see if there is another char in the room
* with the name.  Then check local objs for exdescs.
*
* Thanks to Angus Mezick <angus@EDGIL.CCMAIL.COMPUSERVE.COM> for the
* suggested fix to this problem.
*
* @param char_data *ch The looker.
* @param char *arg What the person typed to look at.
* @param char *more_args Additional args they passed.
* @param bool look_inside If TRUE, also shows contents (where applicable).
*/
void look_at_target(char_data *ch, char *arg, char *more_args, bool look_inside) {
	char targ_arg[MAX_INPUT_LENGTH];
	int found = FALSE, fnum;
	bitvector_t bits;
	char_data *found_char = NULL;
	obj_data *found_obj = NULL, *ex_obj = NULL;
	vehicle_data *found_veh = NULL, *ex_veh = NULL;
	bool inv_only = FALSE;
	char *exdesc;

	if (!ch->desc)
		return;
	
	// if first arg is 'inv', restrict to inventory
	if (strlen(arg) >= 3 && is_abbrev(arg, "inventory")) {
		inv_only = TRUE;
		one_argument(more_args, targ_arg);
		arg = targ_arg;
	}
	
	if (!*arg) {
		send_to_char("Look at what?\r\n", ch);
		return;
	}
	
	fnum = get_number(&arg);
	bits = generic_find(arg, &fnum, inv_only ? FIND_OBJ_INV : (FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP | FIND_CHAR_ROOM | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE), ch, &found_char, &found_obj, &found_veh);

	/* Is the target a character? */
	if (found_char != NULL) {
		look_at_char(found_char, ch, TRUE);
		if (ch != found_char) {
			if (CAN_SEE(found_char, ch)) {
				act("$n looks at you.", TRUE, ch, 0, found_char, TO_VICT);
			}
			act("$n looks at $N.", TRUE, ch, 0, found_char, TO_NOTVICT);
		}
		return;
	}
	
	// was the target a vehicle?
	if (found_veh != NULL) {
		look_at_vehicle(found_veh, ch);
		act("$n looks at $V.", TRUE, ch, NULL, found_veh, TO_ROOM | ACT_VEH_VICT);
		if (look_inside && VEH_FLAGGED(found_veh, VEH_CONTAINER)) {
			look_in_obj(ch, NULL, NULL, found_veh);
		}
		return;
	}

	// are we at 0.name?
	if (fnum <= 0 && !bits) {
		send_to_char("Look at what?\r\n", ch);
		return;
	}
	
	// does the argument match an extra description?
	if ((exdesc = find_exdesc_for_char(ch, arg, &fnum, &ex_obj, &ex_veh, NULL))) {
		if (ex_obj) {
			act("$n looks at $p.", TRUE, ch, ex_obj, NULL, TO_ROOM);
		}
		if (ex_veh) {
			act("$n looks at $V.", TRUE, ch, NULL, ex_veh, TO_ROOM | ACT_VEH_VICT);
		}
		send_to_char(exdesc, ch);
		found = TRUE;
	}
	
	// try moons
	if (!found) {
		found = look_at_moon(ch, arg, &fnum);
	}
	
	/* If an object was found back in generic_find */
	if (bits) {
		if (!found) {
			if (found_obj->worn_by) {
				act("You look at $p (worn):", FALSE, ch, found_obj, NULL, TO_CHAR);
			}
			else if (found_obj->carried_by) {
				act("You look at $p (inventory):", FALSE, ch, found_obj, NULL, TO_CHAR);
			}
			else if (IN_ROOM(found_obj)) {
				act("You look at $p (in room):", FALSE, ch, found_obj, NULL, TO_CHAR);
			}
			else {
				act("You look at $p:", FALSE, ch, found_obj, NULL, TO_CHAR);
			}
			if (ch->desc) {
				page_string(ch->desc, obj_desc_for_char(found_obj, ch, OBJ_DESC_LOOK_AT), TRUE);	/* Show no-description */
			}
			act("$n looks at $p.", TRUE, ch, found_obj, NULL, TO_ROOM);
			found = TRUE;
			if (look_inside && (IS_CONTAINER(found_obj) || IS_CORPSE(found_obj) || IS_DRINK_CONTAINER(found_obj))) {
				look_in_obj(ch, NULL, found_obj, NULL);
			}
		}
	}
	
	// try sky
	if (!found && !str_cmp(arg, "sky")) {
		found = TRUE;
		if (!IS_OUTDOORS(ch) && !CAN_LOOK_OUT(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't see the sky from here.\r\n");
		}
		else {
			do_weather(ch, "", 0, 0);
		}
	}
	
	// finally
	if (!found) {
		send_to_char("You do not see that here.\r\n", ch);
	}
}


/**
* Processes doing a "look in".
*
* @param char_data *ch The player doing the look-in.
* @param char *arg The typed argument (usually obj name) -- only if obj/veh are NULL.
* @param obj_data *obj Optional: A pre-validated object to look in (may be NULL).
* @param vehicle_data *veh Optional: A pre-validated vehicle to look in (may be NULL).
*/
void look_in_obj(char_data *ch, char *arg, obj_data *obj, vehicle_data *veh) {
	char buf[MAX_STRING_LENGTH];
	const char *gstr;
	char_data *dummy = NULL;
	int amt, number = 1;
	
	if (arg && *arg) {
		number = get_number(&arg);
	}
	
	if (!obj && !veh && (!arg || !*arg)) {
		send_to_char("Look in what?\r\n", ch);
	}
	else if (!obj && !veh && !(obj = get_obj_for_char_prefer_container(ch, arg, &number)) && !generic_find(arg, &number, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, &dummy, &obj, &veh)) {
		msg_to_char(ch, "There doesn't seem to be %s %s here.\r\n", AN(arg), arg);
	}
	else if (veh) {
		// vehicle section
		if (!VEH_FLAGGED(veh, VEH_CONTAINER)) {
			act("$V isn't a container.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
		else {
			sprintf(buf, "You look inside $V (%d/%d):", VEH_CARRYING_N(veh), VEH_CAPACITY(veh));
			act(buf, FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
			list_obj_to_char(VEH_CONTAINS(veh), ch, OBJ_DESC_CONTENTS, TRUE);
		}
	}
	// the rest is objects:
	else if (obj && (GET_OBJ_TYPE(obj) != ITEM_DRINKCON) && (GET_OBJ_TYPE(obj) != ITEM_CORPSE) && (GET_OBJ_TYPE(obj) != ITEM_CONTAINER)) {
		send_to_char("There's nothing inside that!\r\n", ch);
	}
	else if (obj) {
		if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER || GET_OBJ_TYPE(obj) == ITEM_CORPSE) {
			if (OBJVAL_FLAGGED(obj, CONT_CLOSED) && GET_OBJ_TYPE(obj) != ITEM_CORPSE)
				send_to_char("It is closed.\r\n", ch);
			else {
				msg_to_char(ch, "You look inside %s (%s):\r\n", get_obj_desc(obj, ch, OBJ_DESC_SHORT), (obj->worn_by ? "worn" : (obj->carried_by ? "inventory" : "in room")));
				list_obj_to_char(obj->contains, ch, OBJ_DESC_CONTENTS, TRUE);
			}
		}
		else if (IS_DRINK_CONTAINER(obj)) {		/* item must be a fountain or drink container */
			if (GET_DRINK_CONTAINER_CONTENTS(obj) <= 0)
				send_to_char("It is empty.\r\n", ch);
			else {
				if (GET_DRINK_CONTAINER_CAPACITY(obj) <= 0 || GET_DRINK_CONTAINER_CONTENTS(obj) > GET_DRINK_CONTAINER_CAPACITY(obj)) {
					// this normally indicates a bug
					sprintf(buf, "Its contents seem somewhat murky.\r\n");
				}
				else {
					amt = (GET_DRINK_CONTAINER_CONTENTS(obj) * 3) / GET_DRINK_CONTAINER_CAPACITY(obj);
					gstr = get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_COLOR);
					sprintf(buf, "It's %sfull of %s %s liquid.\r\n", fullness[amt], AN(gstr), gstr);
				}
				send_to_char(buf, ch);
			}
		}
		else {
			msg_to_char(ch, "You can't look in that!\r\n");
		}
	}
	else {
		// probably unreachable, but in case:
		msg_to_char(ch, "There's nothing in that.\r\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER DISPLAY FUNCTIONS /////////////////////////////////////////////

/**
* Show current health level of i to ch, as a string.
*
* @param char_data *i Person to diagnose.
* @param char_data *ch Person to show the output to.
*/
void diag_char_to_char(char_data *i, char_data *ch) {
	if (!ch || !i || IS_DEAD(i) || EXTRACTED(i) || !ch->desc) {
		return;
	}
	
	sprintf(buf, "$n is %s.", health_levels[(MAX(0, GET_HEALTH(i)) * 10 / MAX(1, GET_MAX_HEALTH(i)))]);
	act(buf, FALSE, i, 0, ch, TO_VICT);
}


/**
* Standard attributes display (used on score and stat).
*
* @param char_data *ch The person whose attributes to show.
* @param char_data *to The person to show them to.
*/
void display_attributes(char_data *ch, char_data *to) {
	char buf[MAX_STRING_LENGTH];
	int iter, pos;

	if (!ch || !to || !to->desc) {
		return;
	}
	
	msg_to_char(to, "        Physical                   Social                      Mental\r\n");
	
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		pos = attribute_display_order[iter];
		snprintf(buf, sizeof(buf), "%s  [%s%2d\t0]", attributes[pos].name, HAPPY_COLOR(GET_ATT(ch, pos), GET_REAL_ATT(ch, pos)), GET_ATT(ch, pos));
		msg_to_char(to, "  %-*.*s%s", 23 + color_code_length(buf), 23 + color_code_length(buf), buf, !((iter + 1) % 3) ? "\r\n" : "");
	}
	if (iter % 3) {
		msg_to_char(to, "\r\n");
	}
}


/**
* Character score sheet. This is targetable, so it doesn't send the data to
* ch himself.
*
* @param char_data *ch The person whose score to get.
* @param char_data *to The person to show it to.
*/
void display_score_to_char(char_data *ch, char_data *to) {
	char lbuf[MAX_STRING_LENGTH], lbuf2[MAX_STRING_LENGTH], lbuf3[MAX_STRING_LENGTH];
	struct player_skill_data *skdata, *next_skill;
	int i, j, count, pts, cols, val, temperature;
	empire_data *emp;
	struct time_info_data playing_time;


	msg_to_char(to, " +----------------------------- EmpireMUD 2.0b5 -----------------------------+\r\n");
	
	// row 1 col 1: name
	msg_to_char(to, "  Name: %-18.18s", PERS(ch, ch, 1));

	// row 1 col 2: class
	get_player_skill_string(ch, lbuf, TRUE);
	msg_to_char(to, " Skill: %-17.17s", lbuf);
	msg_to_char(to, " Level: %d (%d)\r\n", GET_COMPUTED_LEVEL(ch), GET_SKILL_LEVEL(ch));

	// row 1 col 3: levels

	// row 2 col 1
	if (GET_AGE_MODIFIER(ch) || (IS_VAMPIRE(ch) && GET_REAL_AGE(ch) != GET_AGE(ch))) {
		sprintf(buf, "%d/%d years", GET_AGE(ch), GET_REAL_AGE(ch));
	}
	else {
		sprintf(buf, "%d years", GET_AGE(ch));
	}
	playing_time = *real_time_passed((time(0) - ch->player.time.logon) + ch->player.time.played, 0);
	sprintf(buf1, "%dd, %dh", playing_time.day, playing_time.hours);
	msg_to_char(to, "  Age: %-19.19s Play Time: %-13.13s", buf, buf1);
	
	// row 2 col 3: rank
	if ((emp = GET_LOYALTY(ch)) && !IS_NPC(ch)) {
		msg_to_char(to, " Rank: %s&0\r\n", EMPIRE_RANK(emp, GET_RANK(ch)-1));
	}
	else {
		msg_to_char(to, "\r\n");
	}
	
	if (GET_BONUS_TRAITS(ch)) {
		prettier_sprintbit(GET_BONUS_TRAITS(ch), bonus_bit_descriptions, lbuf);
		msg_to_char(to, "  Bonus traits: %s\r\n", lbuf);
	}

	msg_to_char(to, " +-------------------------------- Condition --------------------------------+\r\n");
	
	// row 1 col 1: health, 6 color codes = 12 invisible characters
	sprintf(lbuf, "&g%d&0/&g%d&0 &g%+d&0/%ds", GET_HEALTH(ch), GET_MAX_HEALTH(ch), health_gain(ch, TRUE), SECS_PER_REAL_UPDATE);
	msg_to_char(to, "  Health: %-28.28s", lbuf);

	// row 1 col 2: move, 6 color codes = 12 invisible characters
	sprintf(lbuf, "&y%d&0/&y%d&0 &y%+d&0/%ds", GET_MOVE(ch), GET_MAX_MOVE(ch), move_gain(ch, TRUE), SECS_PER_REAL_UPDATE);
	msg_to_char(to, " Move: %-30.30s", lbuf);
	
	// row 1 col 3: mana, 6 color codes = 12 invisible characters
	sprintf(lbuf, "&c%d&0/&c%d&0 &c%+d&0/%ds", GET_MANA(ch), GET_MAX_MANA(ch), mana_gain(ch, TRUE), SECS_PER_REAL_UPDATE);
	msg_to_char(to, " Mana: %-30.30s\r\n", lbuf);
	
	// row 2 col 1: conditions
	*lbuf = '\0';
	if (IS_HUNGRY(ch)) {
		sprintf(lbuf + strlen(lbuf), "%s&yhungry&0", (strlen(lbuf) > 0 ? ", " : ""));
	}
	if (IS_THIRSTY(ch)) {
		sprintf(lbuf + strlen(lbuf), "%s&cthirsty&0", (strlen(lbuf) > 0 ? ", " : ""));
	}
	if (IS_DRUNK(ch)) {
		sprintf(lbuf + strlen(lbuf), "%s&mdrunk&0", (strlen(lbuf) > 0 ? ", " : ""));
	}
	if (IS_BLOOD_STARVED(ch)) {
		sprintf(lbuf + strlen(lbuf), "%s&rstarving&0", (strlen(lbuf) > 0 ? ", " : ""));
	}
	temperature = get_relative_temperature(ch);
	if (temperature <= -1 * config_get_int("temperature_discomfort")) {
		sprintf(lbuf + strlen(lbuf), "%s&c%s&0", (strlen(lbuf) > 0 ? ", " : ""), temperature_to_string(temperature));
	}
	if (temperature >= config_get_int("temperature_discomfort")) {
		sprintf(lbuf + strlen(lbuf), "%s&o%s&0", (strlen(lbuf) > 0 ? ", " : ""), temperature_to_string(temperature));
	}
	if (*lbuf == '\0') {
		strcpy(lbuf, "&gnone&0");
	}
	// gotta count the color codes to determine width
	count = 37 + color_code_length(lbuf);
	msg_to_char(to, "  Conditions: %-*.*s", count, count, lbuf);
	
	if (IS_VAMPIRE(ch)) {
		msg_to_char(to, " Blood: &r%d&0/&r%d&0-&r%d&0/hr\r\n", GET_BLOOD(ch), GET_MAX_BLOOD(ch), MAX(0, GET_BLOOD_UPKEEP(ch)));
	}
	else {
		msg_to_char(to, "\r\n");
	}
	
	msg_to_char(to, " +------------------------------- Attributes --------------------------------+\r\n");
	display_attributes(ch, to);

	// secondary attributes
	msg_to_char(to, " +---------------------------------------------------------------------------+\r\n");

	// row 1 (dex is removed from dodge to make the display easier to read)
	val = get_dodge_modifier(ch, NULL, FALSE) - (hit_per_dex * GET_DEXTERITY(ch));
	sprintf(lbuf, "Dodge  [%s%d&0]", HAPPY_COLOR(val, 0), val);
	
	val = get_block_rating(ch, FALSE);
	sprintf(lbuf2, "Block  [%s%d&0]", HAPPY_COLOR(val, 0), val);
	
	sprintf(lbuf3, "Resist  [%dp | %dm]", GET_RESIST_PHYSICAL(ch), GET_RESIST_MAGICAL(ch));
	msg_to_char(to, "  %-28.28s %-28.28s %-24.24s\r\n", lbuf, lbuf2, lbuf3);
	
	// row 2
	sprintf(lbuf, "Physical  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_PHYSICAL(ch), 0), GET_BONUS_PHYSICAL(ch));
	sprintf(lbuf2, "Magical  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_MAGICAL(ch), 0), GET_BONUS_MAGICAL(ch));
	sprintf(lbuf3, "Healing  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_HEALING(ch), 0), GET_BONUS_HEALING(ch));
	msg_to_char(to, "  %-28.28s %-28.28s %-28.28s\r\n", lbuf, lbuf2, lbuf3);
	
	// row 3 (dex is removed from to-hit to make the display easier to read)
	val = get_to_hit(ch, NULL, FALSE, FALSE) - (hit_per_dex * GET_DEXTERITY(ch));
	sprintf(lbuf, "To-hit  [%s%d&0]", HAPPY_COLOR(val, base_hit_chance), val);
	sprintf(lbuf2, "Speed  [%.2f]", get_combat_speed(ch, WEAR_WIELD));
	sprintf(lbuf3, "Crafting  [%s%d&0]", HAPPY_COLOR(get_crafting_level(ch), GET_SKILL_LEVEL(ch)), get_crafting_level(ch));
	// note: the "%-24.24s" for speed is lower because it contains no color codes
	msg_to_char(to, "  %-28.28s %-24.24s %-28.28s\r\n", lbuf, lbuf2, lbuf3);
		
	msg_to_char(to, " +--------------------------------- Skills ----------------------------------+\r\n ");

	count = 0;
	HASH_ITER(hh, GET_SKILL_HASH(ch), skdata, next_skill) {
		if (skdata->level > 0 && !SKILL_FLAGGED(skdata->ptr, SKILLF_IN_DEVELOPMENT)) {
			sprintf(lbuf, " %s: %s%d", SKILL_NAME(skdata->ptr), IS_ANY_SKILL_CAP(ch, skdata->vnum) ? "&g" : "&y", skdata->level);
			pts = get_ability_points_available_for_char(ch, skdata->vnum);
			if (pts > 0) {
				sprintf(lbuf + strlen(lbuf), " &g(%d)", pts);
			}
			
			cols = 25 + color_code_length(lbuf);
			msg_to_char(to, "%-*.*s&0", cols, cols, lbuf);
			
			if (++count == 3) {
				msg_to_char(to, "&0\r\n ");
				count = 0;
			}
		}
	}

	// trailing \r\n on skills
	if (count > 0) {
		msg_to_char(to, "&0\r\n ");
	}

	/* Gods and Immortals: */
	if (IS_GOD(ch) || IS_IMMORTAL(ch)) {
		strcpy(buf, " ");
		for (i = 0, j = 0; i < NUM_MATERIALS; i++)
			if (GET_RESOURCE(ch, i))
				sprintf(buf + strlen(buf), " %-14.14s %-6d %s", materials[i].name, GET_RESOURCE(ch, i), !(++j % 3) ? "\r\n " : "  ");
		if (j % 3)
			strcat(buf, "\r\n ");
		if (*buf)
			msg_to_char(to, "+------------------------------- Resources ---------------------------------+\r\n%s", buf);
	}

	// everything leaves a starting space so this next line does not need it
	msg_to_char(to, "+---------------------------------------------------------------------------+\r\n");
	
	// affects last
	if (ch == to) {
		// show full effects, which is only formatted for the character
		do_affects(to, "", 0, 0);
	}
	else {
		// show summary effects
		show_character_affects(ch, to);
	}
}


/**
* Shows a list of characters in the room.
*
* @param char_data *list Pointer to the start of the list of people in room.
* @param char_data *ch Person to send the output to.
*/
void list_char_to_char(char_data *list, char_data *ch) {
	char_data *i, *j;
	int c;
	
	bool use_mob_stacking = config_get_bool("use_mob_stacking");
	#define MOB_CAN_STACK(ch)  (use_mob_stacking && !GET_COMPANION(ch) && !GET_LED_BY(ch) && GET_POS(ch) != POS_FIGHTING && !MOB_FLAGGED((ch), MOB_EMPIRE | MOB_TIED | MOB_MOUNTABLE))
	
	// no work
	if (!list || !ch || !ch->desc) {
		return;
	}
	
	DL_FOREACH2(list, i, next_in_room) {
		c = 1;
		if (ch != i) {
			if (IS_NPC(i) && MOB_CAN_STACK(i)) {
				// check if already showed this mob...
				DL_FOREACH2(list, j, next_in_room) {
					if (j == i || (GET_MOB_VNUM(j) == GET_MOB_VNUM(i) && MOB_CAN_STACK(j) && CAN_SEE(ch, j) && GET_POS(j) == GET_POS(i))) {
						break;
					}
				}
			
				if (j != i) {
					continue;	// already showed this mob
				}
				
				// count duplicates
				DL_FOREACH2(i->next_in_room, j, next_in_room) {
					if (GET_MOB_VNUM(j) == GET_MOB_VNUM(i) && MOB_CAN_STACK(j) && CAN_SEE(ch, j) && GET_POS(j) == GET_POS(i)) {
						++c;
					}
				}
			}

			if (CAN_SEE(ch, i)) {
				list_one_char(i, ch, c);
			}
			else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE) && HAS_INFRA(i) && !MAGIC_DARKNESS(IN_ROOM(ch))) {
				if (c > 1) {
					msg_to_char(ch, "(%2d) ", c);
				}
				msg_to_char(ch, "You see a pair of glowing red eyes looking your way.\r\n");
			}
		}
	}
}


/**
* Show a player's lore entries.
*
* @param char_data *ch The person whose lore to show.
* @param char_data *to The person to show it to.
*/
void list_lore_to_char(char_data *ch, char_data *to) {
	struct lore_data *lore;
	char daystring[MAX_INPUT_LENGTH];
	struct time_info_data t;
	long beginning_of_time = data_get_long(DATA_WORLD_START);

	msg_to_char(to, "%s's lore:\r\n", PERS(ch, ch, 1));

	for (lore = GET_LORE(ch); lore; lore = lore->next) {
		if (has_player_tech(to, PTECH_CALENDAR)) {
			t = *mud_time_passed((time_t) lore->date, (time_t) beginning_of_time);

			strcpy(buf, month_name[(int) t.month]);
			if (!strn_cmp(buf, "the ", 4))
				strcpy(buf1, buf + 4);
			else
				strcpy(buf1, buf);

			snprintf(daystring, sizeof(daystring), "%d %s, Year %d", t.day + 1, buf1, t.year);
			msg_to_char(to, " %s on %s.\r\n", NULLSAFE(lore->text), daystring);
		}
		else {
			msg_to_char(to, " %s.\r\n", NULLSAFE(lore->text));
		}
	}
}


/**
* Shows one character as in-the-room.
*
* @param char_data *i The person to show.
* @param char_data *ch The person to send the output to.
* @param int num If mob-stacking is on, number of copies of this i to show.
*/
void list_one_char(char_data *i, char_data *ch, int num) {
	char buf[MAX_STRING_LENGTH], buf1[MAX_STRING_LENGTH], part[256];
	ability_data *abil;
	struct custom_message *ocm;
	struct affected_type *aff;
	generic_data *gen;
	struct over_time_effect_type *dot;
	struct string_hash *str_iter, *next_str, *str_hash = NULL;
	
	// POS_x
	const char *positions[] = {
		"is lying here, dead.",
		"is lying here, mortally wounded.",
		"is lying here, incapacitated.",
		"is lying here, stunned.",
		"is sleeping here.",
		"is resting here.",
		"is sitting here.",
		"!FIGHTING!",
		"is standing here."
	};
	
	// don't bother
	if (!ch || !i || !ch->desc) {
		return;
	}
	
	// able to see at all?
	if (AFF_FLAGGED(i, AFF_NO_SEE_IN_ROOM) && !PRF_FLAGGED(ch, PRF_HOLYLIGHT)) {
		return;
	}
	if (!WIZHIDE_OK(ch, i)) {
		return;
	}

	if (num > 1) {
		msg_to_char(ch, "(%2d) ", num);
	}

	if (GROUP(i) && in_same_group(ch, i)) {
		msg_to_char(ch, "(%s) ", GROUP_LEADER(GROUP(i)) == i ? "leader" : "group");
	}
	
	// empire prefixing
	if (!CHAR_MORPH_FLAGGED(i, MORPHF_ANIMAL)) {
		if (IS_DISGUISED(i)) {
			if (ROOM_OWNER(IN_ROOM(i))) {
				// disguised player shows loyalty of the area you're in -- players can't tell you're not an npc
				msg_to_char(ch, "<%s> ", EMPIRE_ADJECTIVE(ROOM_OWNER(IN_ROOM(i))));
			}
		}
		else if (GET_LOYALTY(i)) {
			if (IS_NPC(i)) {
				// npc shows empire adj
				msg_to_char(ch, "<%s> ", EMPIRE_ADJECTIVE(GET_LOYALTY(i)));
			}
			else {
				// player shows rank
				msg_to_char(ch, "<%s&0&y> ", EMPIRE_RANK(GET_LOYALTY(i), GET_RANK(i)-1));
			}
		}
	}
	
	if (PRF_FLAGGED(ch, PRF_ROOMFLAGS) && IS_NPC(i)) {
		msg_to_char(ch, "[%d] %s", GET_MOB_VNUM(i), HAS_TRIGGERS(i) ? "[TRIG] " : "");
	}
	
	if (IS_MORPHED(i) && GET_POS(i) == POS_STANDING) {
		if (AFF_FLAGGED(i, AFF_INVISIBLE)) {
			msg_to_char(ch, "*");
		}
		msg_to_char(ch, "%s\r\n", get_morph_desc(i, TRUE));
	}
	else if (IS_NPC(i) && mob_has_custom_message(i, MOB_CUSTOM_LONG_DESC) && GET_POS(i) == POS_STANDING) {
		if (AFF_FLAGGED(i, AFF_INVISIBLE)) {
			msg_to_char(ch, "*");
		}
		msg_to_char(ch, "%s\r\n", mob_get_custom_message(i, MOB_CUSTOM_LONG_DESC));
	}
	else if (IS_NPC(i) && GET_LONG_DESC(i) && GET_POS(i) == POS_STANDING) {
		if (AFF_FLAGGED(i, AFF_INVISIBLE)) {
			msg_to_char(ch, "*");
		}
		send_to_char(GET_LONG_DESC(i), ch);
	}
	else {
		*buf = '\0';
		
		if (GET_POS(i) != POS_FIGHTING) {
			if (GET_SITTING_ON(i)) {
				snprintf(part, sizeof(part), "%s", position_types[GET_POS(i)]);
				*part = LOWER(*part);
				sprintf(buf, "$n is %s %s %s%s%s.", part, IN_OR_ON(GET_SITTING_ON(i)), get_vehicle_short_desc(GET_SITTING_ON(i), ch), (VEH_ANIMALS(GET_SITTING_ON(i)) ? ", being pulled by " : ""), (VEH_ANIMALS(GET_SITTING_ON(i)) ? list_harnessed_mobs(GET_SITTING_ON(i)) : ""));
			}
			else if (!IS_NPC(i) && GET_ACTION(i) == ACT_GEN_CRAFT) {
				// show crafting
				craft_data *ctype = craft_proto(GET_ACTION_VNUM(i, 0));
				if (ctype && strstr(gen_craft_data[GET_CRAFT_TYPE(ctype)].strings[GCD_LONG_DESC], "%s")) {
					sprintf(buf1, "%s %s", AN(GET_CRAFT_NAME(ctype)), GET_CRAFT_NAME(ctype));
					sprintf(buf, gen_craft_data[GET_CRAFT_TYPE(ctype)].strings[GCD_LONG_DESC], buf1);
				}
				else if (ctype) {
					strcpy(buf, gen_craft_data[GET_CRAFT_TYPE(ctype)].strings[GCD_LONG_DESC]);
				}
				else {
					sprintf(buf, "$n %s", action_data[GET_ACTION(i)].long_desc);
				}
			}
			else if (!IS_NPC(i) && GET_ACTION(i) == ACT_OVER_TIME_ABILITY && (abil = ability_proto(GET_ACTION_VNUM(i, 0))) && abil_has_custom_message(abil, ABIL_CUSTOM_OVER_TIME_LONGDESC)) {
				// custom message
				strcpy(buf, abil_get_custom_message(abil, ABIL_CUSTOM_OVER_TIME_LONGDESC));
			}
			else if (!IS_NPC(i) && GET_ACTION(i) != ACT_NONE) {
				// show non-crafting action
				sprintf(buf, "$n %s", action_data[GET_ACTION(i)].long_desc);
			}
			else if (AFF_FLAGGED(i, AFF_DEATHSHROUDED) && GET_POS(i) <= POS_RESTING) {
				sprintf(buf, "$n %s", positions[POS_DEAD]);
			}
			else if (GET_POS(i) == POS_STANDING && AFF_FLAGGED(i, AFF_FLYING)) {
				strcpy(buf, "$n is flying here.");
			}
			else if (GET_POS(i) == POS_STANDING && !IS_DISGUISED(i) && (ocm = pick_custom_longdesc(i))) {
				strcpy(buf, ocm->msg);
			}
			else {	// normal positions
				sprintf(buf, "$n %s", positions[(int) GET_POS(i)]);
			}
		}
		else {
			if (FIGHTING(i)) {
				strcpy(buf, "$n is here, fighting ");
				if (FIGHTING(i) == ch)
					strcat(buf, "YOU!");
				else {
					if (IN_ROOM(i) == IN_ROOM(FIGHTING(i)))
						strcat(buf, PERS(FIGHTING(i), ch, 0));
					else
						strcat(buf, "someone who has already left");
					strcat(buf, "!");
					}
				}
			else			/* NIL fighting pointer */
				strcpy(buf, "$n is here struggling with thin air.");
		}
		
		if (AFF_FLAGGED(i, AFF_HIDDEN))
			strcat(buf, " (hidden)");
		if (!IS_NPC(i) && !i->desc)
			strcat(buf, " (linkless)");
		
		// send it
		if (AFF_FLAGGED(i, AFF_INVISIBLE)) {
			msg_to_char(ch, "*");
		}
		act(buf, FALSE, i, NULL, ch, TO_VICT);
	}
	
	if (can_get_quest_from_mob(ch, i, NULL)) {
		act("...$e has a quest for you!", FALSE, i, NULL, ch, TO_VICT);
	}
	if (can_turn_quest_in_to_mob(ch, i, NULL)) {
		act("...you can finish a quest here!", FALSE, i, NULL, ch, TO_VICT);
	}
	if (IS_RIDING(i)) {
		sprintf(buf, "...$E is %s upon %s.", (MOUNT_FLAGGED(i, MOUNT_FLYING) ? "flying" : "mounted"), get_mob_name_by_proto(GET_MOUNT_VNUM(i), TRUE));
		act(buf, FALSE, ch, 0, i, TO_CHAR);
	}
	if (GET_LED_BY(i)) {
		sprintf(buf, "...%s is being led by %s.", HSSH(i), GET_LED_BY(i) == ch ? "you" : "$N");
		act(buf, FALSE, ch, 0, GET_LED_BY(i), TO_CHAR);
	}
	if (MOB_FLAGGED(i, MOB_TIED))
		act("...$e is tied up here.", FALSE, i, 0, ch, TO_VICT);
	if (AFF_FLAGGED(i, AFF_HIDDEN)) {
		act("...$e has attempted to hide $mself.", FALSE, i, 0, ch, TO_VICT);
	}
	if (IS_INJURED(i, INJ_TIED))
		act("...$e is bound and gagged!", FALSE, i, 0, ch, TO_VICT);
	if (IS_INJURED(i, INJ_STAKED))
		act("...$e has a stake through $s heart!", FALSE, i, 0, ch, TO_VICT);
	
	// strings from affects
	LL_FOREACH(i->affected, aff) {
		if ((gen = real_generic(aff->type)) && GET_AFFECT_LOOK_AT_ROOM(gen)) {
			add_string_hash(&str_hash, GET_AFFECT_LOOK_AT_ROOM(gen), 1);
		}
	}
	LL_FOREACH(i->over_time_effects, dot) {
		if ((gen = real_generic(dot->type)) && GET_AFFECT_LOOK_AT_ROOM(gen)) {
			add_string_hash(&str_hash, GET_AFFECT_LOOK_AT_ROOM(gen), 1);
		}
	}
	HASH_ITER(hh, str_hash, str_iter, next_str) {
		act(str_iter->str, FALSE, i, NULL, ch, TO_VICT);
	}
	free_string_hash(&str_hash);
	
	if (GET_FED_ON_BY(i)) {
		sprintf(buf, "...$e is held tightly by %s!", PERS(GET_FED_ON_BY(i), ch, 0));
		act(buf, FALSE, i, 0, ch, TO_VICT);
	}
	if (GET_FEEDING_FROM(i)) {
		sprintf(buf, "...$e has $s teeth firmly implanted in %s!", PERS(GET_FEEDING_FROM(i), ch, 0));
		act(buf, FALSE, i, 0, ch, TO_VICT);
	}
	if (!IS_NPC(i) && GET_ACTION(i) == ACT_MORPHING) {
		act("...$e is undergoing a hideous transformation!", FALSE, i, FALSE, ch, TO_VICT);
	}
	if ((IS_MORPHED(i) || IS_DISGUISED(i)) && CAN_RECOGNIZE(ch, i) && !CHAR_MORPH_FLAGGED(i, MORPHF_HIDE_REAL_NAME)) {
		act("...this appears to be $o.", FALSE, i, FALSE, ch, TO_VICT);
	}
	
	// these 
	if ((AFF_FLAGGED(i, AFF_NO_SEE_IN_ROOM) || (IS_IMMORTAL(i) && PRF_FLAGGED(i, PRF_WIZHIDE))) && PRF_FLAGGED(ch, PRF_HOLYLIGHT)) {
		if (AFF_FLAGGED(i, AFF_EARTHMELDED)) {
			act("...$e is earthmelded.", FALSE, i, 0, ch, TO_VICT);
		}
		else {
			act("...$e is unable to be seen by players.", FALSE, i, 0, ch, TO_VICT);
		}
	}
}


/**
* Builds the text for one vehicle as shown in-the-room.
*
* @param vehicle_data *veh The vehicle to show.
* @param char_data *ch The person to send the output to.
* @param char* The constructed text for the vehicle's listing.
*/
char *list_one_vehicle_to_char(vehicle_data *veh, char_data *ch) {
	static char buf[MAX_STRING_LENGTH];
	char part[256];
	size_t size = 0, pos;
	
	// pre-description
	if (VEH_OWNER(veh) && (VEH_OWNER(veh) != ROOM_OWNER(IN_ROOM(veh)) || !VEH_CLAIMS_WITH_ROOM(veh))) {
		size += snprintf(buf + size, sizeof(buf) - size, "<%s> ", EMPIRE_ADJECTIVE(VEH_OWNER(veh)));
	}
	if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
		size += snprintf(buf + size, sizeof(buf) - size, "[%d] %s", VEH_VNUM(veh), HAS_TRIGGERS(veh) ? "[TRIG] " : "");
	}
	
	// main desc
	if (VEH_IS_COMPLETE(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "%s\r\n", VEH_LONG_DESC(veh));
	}
	else if (VEH_IS_DISMANTLING(veh)) {
		pos = size;
		size += snprintf(buf + size, sizeof(buf) - size, "%s is being dismantled.\r\n", get_vehicle_short_desc(veh, ch));
		*(buf + pos) = UPPER(*(buf + pos));
	}
	else {
		pos = size;
		size += snprintf(buf + size, sizeof(buf) - size, "%s is under construction.\r\n", get_vehicle_short_desc(veh, ch));
		*(buf + pos) = UPPER(*(buf + pos));
	}
	
	// additional descriptions like what's attached:
	if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		size += snprintf(buf + size, sizeof(buf) - size, "...it is ON FIRE!\r\n");
	}
	else if (VEH_IS_COMPLETE(veh) && (VEH_NEEDS_RESOURCES(veh) || VEH_HEALTH(veh) < VEH_MAX_HEALTH(veh))) {
		size += snprintf(buf + size, sizeof(buf) - size, "...it is in need of repair.\r\n");
	}
	
	if (VEH_SITTING_ON(veh) == ch) {
		snprintf(part, sizeof(part), "%s", position_types[GET_POS(VEH_SITTING_ON(veh))]);
		*part = LOWER(*part);
		size += snprintf(buf + size, sizeof(buf) - size, "...you are %s %s it.\r\n", part, IN_OR_ON(veh));
	}
	else if (VEH_SITTING_ON(veh)) {
		// only shown to players when it's a building
		snprintf(part, sizeof(part), "%s", position_types[GET_POS(VEH_SITTING_ON(veh))]);
		*part = LOWER(*part);
		size += snprintf(buf + size, sizeof(buf) - size, "...%s is %s %s it.\r\n", PERS(VEH_SITTING_ON(veh), ch, FALSE), part, IN_OR_ON(veh));
	}
	
	if (VEH_LED_BY(veh) == ch) {
		size += snprintf(buf + size, sizeof(buf) - size, "...you are leading it.\r\n");
	}
	else if (VEH_LED_BY(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "...it is being led by %s.\r\n", PERS(VEH_LED_BY(veh), ch, FALSE));
	}
	
	if (VEH_ANIMALS(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "...it is being pulled by %s.\r\n", list_harnessed_mobs(veh));
	}
	
	if (can_get_quest_from_vehicle(ch, veh, NULL)) {
		size += snprintf(buf + size, sizeof(buf) - size, "...it has a quest for you!\r\n");
	}
	if (can_turn_quest_in_to_vehicle(ch, veh, NULL)) {
		size += snprintf(buf + size, sizeof(buf) - size, "...you can finish a quest here!\r\n");
	}

	return buf;
}


/**
* Shows a list of vehicles in the room.
*
* @param vehicle_data *list Pointer to the start of the list of vehicles.
* @param vehicle_data *ch Person to send the output to.
* @param bool large_only If TRUE, skips small vehicles like furniture
* @param vehicle_data *exclude Optional: Don't show a specific vehicle (usually the one you're in); may be NULL.
*/
void list_vehicles_to_char(vehicle_data *list, char_data *ch, bool large_only, vehicle_data *exclude) {
	struct string_hash *str_iter, *next_str, *str_hash = NULL;
	vehicle_data *veh;
	
	bitvector_t large_veh_flags = VEH_BUILDING | VEH_NO_BUILDING | VEH_SIEGE_WEAPONS | VEH_ON_FIRE | VEH_VISIBLE_IN_DARK | VEH_OBSCURE_VISION;
	
	// no work
	if (!list || !ch || !ch->desc) {
		return;
	}
	
	DL_FOREACH2(list, veh, next_in_room) {
		// conditions to show
		if (veh == exclude) {	
			continue;
		}
		if (VEH_IS_EXTRACTED(veh) || !CAN_SEE_VEHICLE(ch, veh)) {
			continue;	// should we show a "something" ?
		}
		if (large_only && !VEH_FLAGGED(veh, large_veh_flags)) {
			continue;	// missing required flags
		}
		if (VEH_SITTING_ON(veh) && VEH_SITTING_ON(veh) != ch && !VEH_FLAGGED(veh, VEH_BUILDING)) {
			continue;	// don't show vehicles someone else is sitting on
		}
		
		add_string_hash(&str_hash, list_one_vehicle_to_char(veh, ch), 1);
	}
	
	HASH_ITER(hh, str_hash, str_iter, next_str) {
		if (str_iter->count == 1) {
			send_to_char(str_iter->str, ch);
		}
		else {
			msg_to_char(ch, "(%2d) %s", str_iter->count, str_iter->str);
		}
	}
	
	free_string_hash(&str_hash);
}


/**
* Perform a look-at-person.
*
* @param char_data *i The person being looked at.
* @param char_data *ch The person to show the output to.
* @param bool show_eq If TRUE, can also show inventory (if skilled).
*/
void look_at_char(char_data *i, char_data *ch, bool show_eq) {
	char buf[MAX_STRING_LENGTH];
	bool disguise, show_inv = show_eq, conceal_eq = FALSE, conceal_inv = FALSE;
	int j, found;
	struct affected_type *aff;
	generic_data *gen;
	struct over_time_effect_type *dot;
	struct string_hash *str_iter, *next_str, *str_hash = NULL;
	
	if (!i || !ch || !ch->desc)
		return;
	
	disguise = !PRF_FLAGGED(ch, PRF_HOLYLIGHT) && (IS_DISGUISED(i) || (IS_MORPHED(i) && CHAR_MORPH_FLAGGED(i, MORPHF_ANIMAL)));
	
	if (show_eq && ch != i && !IS_IMMORTAL(ch) && !IS_NPC(i)) {
		if (has_player_tech(i, PTECH_CONCEAL_EQUIPMENT)) {
			show_eq = FALSE;
			conceal_eq = TRUE;
		}
		if (has_player_tech(i, PTECH_CONCEAL_INVENTORY)) {
			show_inv = FALSE;
			conceal_inv = TRUE;
		}
	
	}

	if (ch != i) {
		act("You look at $N:", FALSE, ch, FALSE, i, TO_CHAR);
	}
	else {
		msg_to_char(ch, "You take a look at yourself:\r\n");
	}
	
	// look decs
	if (IS_MORPHED(i)) {
		msg_to_char(ch, "%s&0", NULLSAFE(MORPH_LOOK_DESC(GET_MORPH(i))));
	}
	else if (GET_LOOK_DESC(i)) {
		msg_to_char(ch, "%s&0", GET_LOOK_DESC(i));
	}
	
	// diagnose at the end
	if (GET_HEALTH(i) < GET_MAX_HEALTH(i)) {
		diag_char_to_char(i, ch);
	}
	
	// membership info
	if (GET_LOYALTY(i) && !disguise) {
		sprintf(buf, "$E is a member of %s%s\t0.", EMPIRE_BANNER(GET_LOYALTY(i)), EMPIRE_NAME(GET_LOYALTY(i)));
		act(buf, FALSE, ch, NULL, i, TO_CHAR);
	}
	if (ROOM_OWNER(IN_ROOM(i)) && disguise) {
		sprintf(buf, "$E is a member of %s%s\t0.", EMPIRE_BANNER(ROOM_OWNER(IN_ROOM(i))), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(i))));
		act(buf, FALSE, ch, NULL, i, TO_CHAR);
	}
	if (IS_NPC(i) && MOB_FACTION(i) && !FACTION_FLAGGED(MOB_FACTION(i), FCT_HIDE_ON_MOB)) {
		struct player_faction_data *pfd = get_reputation(ch, FCT_VNUM(MOB_FACTION(i)), FALSE);
		int idx = rep_const_to_index(pfd ? pfd->rep : FCT_STARTING_REP(MOB_FACTION(i)));
		sprintf(buf, "$E is a member of %s%s\t0.", (idx != NOTHING ? reputation_levels[idx].color : ""), FCT_NAME(MOB_FACTION(i)));
		act(buf, FALSE, ch, NULL, i, TO_CHAR);
	}
	
	if (size_data[GET_SIZE(i)].show_on_look && (IS_MORPHED(i) ? !MORPH_LOOK_DESC(GET_MORPH(i)) : !GET_LOOK_DESC(i))) {
		act(size_data[GET_SIZE(i)].show_on_look, FALSE, ch, NULL, i, TO_CHAR);
	}
	
	// generic affs -- npc or disguise do not affect this
	LL_FOREACH(i->affected, aff) {
		if ((gen = real_generic(aff->type)) && GET_AFFECT_LOOK_AT_CHAR(gen)) {
			add_string_hash(&str_hash, GET_AFFECT_LOOK_AT_CHAR(gen), 1);
		}
	}
	LL_FOREACH(i->over_time_effects, dot) {
		if ((gen = real_generic(dot->type)) && GET_AFFECT_LOOK_AT_CHAR(gen)) {
			add_string_hash(&str_hash, GET_AFFECT_LOOK_AT_CHAR(gen), 1);
		}
	}
	HASH_ITER(hh, str_hash, str_iter, next_str) {
		act(str_iter->str, FALSE, i, NULL, ch, TO_VICT);
	}
	free_string_hash(&str_hash);
	
	if (IS_NPC(i) && !disguise && MOB_FLAGGED(i, MOB_MOUNTABLE) && CAN_RIDE_MOUNT(ch, i)) {
		act("You can ride on $M.", FALSE, ch, NULL, i, TO_CHAR);
	}
	
	if (show_eq && !disguise) {
		// check if there's eq to see
		found = FALSE;
		for (j = 0; !found && j < NUM_WEARS; j++) {
			if (GET_EQ(i, j) && CAN_SEE_OBJ(ch, GET_EQ(i, j))) {
				found = TRUE;
			}
		}
		
		// show eq
		if (found) {
			msg_to_char(ch, "\r\n");	/* act() does capitalization. */
			act("$n is using:", FALSE, i, 0, ch, TO_VICT);
			for (j = 0; j < NUM_WEARS; j++) {
				if (GET_EQ(i, j) && CAN_SEE_OBJ(ch, GET_EQ(i, j))) {
					msg_to_char(ch, "%s%s", wear_data[j].eq_prompt, obj_desc_for_char(GET_EQ(i, j), ch, OBJ_DESC_EQUIPMENT));
				}
			}
		}
	}
	if (show_inv && !disguise) {
		// show inventory
		if (ch != i && has_player_tech(ch, PTECH_SEE_INVENTORY) && i->carrying && !run_ability_triggers_by_player_tech(ch, PTECH_SEE_INVENTORY, i, NULL, NULL)) {
			act("\r\nYou appraise $s inventory:", FALSE, i, 0, ch, TO_VICT);
			list_obj_to_char(i->carrying, ch, OBJ_DESC_INVENTORY, TRUE);

			if (ch != i && i->carrying) {
				if (can_gain_exp_from(ch, i)) {
					gain_player_tech_exp(ch, PTECH_SEE_INVENTORY, 5);
				}
				run_ability_hooks_by_player_tech(ch, PTECH_SEE_INVENTORY, i, NULL, NULL, NULL);
				GET_WAIT_STATE(ch) = MAX(GET_WAIT_STATE(ch), 0.5 RL_SEC);
			}
		}
	}
	
	// tech gains and hooks
	if (conceal_eq) {
		gain_player_tech_exp(i, PTECH_CONCEAL_EQUIPMENT, 5);
		run_ability_hooks_by_player_tech(i, PTECH_CONCEAL_EQUIPMENT, ch, NULL, NULL, NULL);
	}
	if (conceal_inv) {
		gain_player_tech_exp(i, PTECH_CONCEAL_INVENTORY, 5);
		run_ability_hooks_by_player_tech(i, PTECH_CONCEAL_INVENTORY, ch, NULL, NULL, NULL);
	}
}


/**
* Lists the affects on a character in a way that can be shown to players.
* 
* @param char_data *ch Whose effects
* @param char_data *to Who to send to
*/
void show_character_affects(char_data *ch, char_data *to) {
	struct over_time_effect_type *dot;
	struct affected_type *aff;
	bool beneficial;
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], lbuf[MAX_INPUT_LENGTH];
	int duration;

	/* Routine to show what spells a char is affected by */
	for (aff = ch->affected; aff; aff = aff->next) {
		*buf2 = '\0';
		beneficial = affect_is_beneficial(aff);

		// duration setup
		if (aff->expire_time == UNLIMITED) {
			strcpy(lbuf, "infinite");
		}
		else {
			duration = aff->expire_time - time(0);
			duration = MAX(duration, 0);
			strcpy(lbuf, colon_time(duration, FALSE, NULL));
		}
		
		// main entry
		sprintf(buf, "   \t%c%s\t0%s (%s) ", beneficial ? 'c' : 'r', get_generic_name_by_vnum(aff->type), (!beneficial && PRF_FLAGGED(to, PRF_SCREEN_READER)) ? " (debuff)" : "", lbuf);

		if (aff->modifier) {
			sprintf(buf2, "- %+d to %s", aff->modifier, apply_types[(int) aff->location]);
			strcat(buf, buf2);
		}
		if (aff->bitvector) {
			if (*buf2)
				strcat(buf, ", ");
			else
				strcat(buf, "- ");
			prettier_sprintbit(aff->bitvector, affected_bits, buf2);
			strcat(buf, buf2);
		}
		send_to_char(strcat(buf, "\r\n"), to);
	}
	
	// show DoT affects too
	for (dot = ch->over_time_effects; dot; dot = dot->next) {
		strcpy(lbuf, colon_time(dot->time_remaining, FALSE, NULL));
		
		// main body
		msg_to_char(to, "   \tr%s\t0 (%s) %d %s damage (%d/%d)\r\n", get_generic_name_by_vnum(dot->type), lbuf, dot->damage * dot->stack, damage_types[dot->damage_type], dot->stack, dot->max_stack);
	}
}


/**
* Mortal view of other people's affects. This shows very little; it shows a
* little more if you have the right ptech.
* 
* @param char_data *ch Whose effects
* @param char_data *to Who to send to
*/
void show_character_affects_simple(char_data *ch, char_data *to) {
	bool good, is_ally, details;
	char line[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH], output[MAX_STRING_LENGTH];
	char *temp;
	int duration;
	size_t size;
	struct affected_type *aff;
	struct over_time_effect_type *dot;
	struct string_hash *str_iter, *next_str, *str_hash = NULL;
	
	if (IS_NPC(to) || !to->desc) {
		return;	// nobody to show to
	}
	
	is_ally = (is_fight_ally(to, ch) || GET_COMPANION(to) == ch);
	details = is_ally || (has_player_tech(to, PTECH_ENEMY_BUFF_DETAILS) && !AFF_FLAGGED(ch, AFF_SOULMASK));
		
	// build affects
	LL_FOREACH(ch->affected, aff) {
		good = affect_is_beneficial(aff);
		
		if (details) {
			// duration setup
			if (aff->expire_time == UNLIMITED) {
				strcpy(lbuf, "infinite");
			}
			else {
				duration = aff->expire_time - time(0);
				duration = MAX(duration, 0);
				strcpy(lbuf, colon_time(duration, FALSE, "infinite"));
			}
			
			// main entry
			snprintf(line, sizeof(line), "%s%s&0%s (%s)", (good ? "&c" : "&r"), get_generic_name_by_vnum(aff->type), (!good && PRF_FLAGGED(to, PRF_SCREEN_READER)) ? " (debuff)" : "", lbuf);
			
			if (aff->modifier) {
				snprintf(line + strlen(line), sizeof(line) - strlen(line), " - %+d to %s", aff->modifier, apply_types[(int) aff->location]);
			}
			if (aff->bitvector) {
				prettier_sprintbit(aff->bitvector, affected_bits, lbuf);
				snprintf(line + strlen(line), sizeof(line) - strlen(line), "%s %s", (aff->modifier ? "," : " -"), lbuf);
			}
			
			// caster?
			if (aff->cast_by == CAST_BY_ID(to)) {
				snprintf(line + strlen(line), sizeof(line) - strlen(line), " (you)");
			}
			
			// add to hash
			add_string_hash(&str_hash, line, 1);
		}
		else {
			// simple version
			snprintf(line, sizeof(line), "%s%s&0%s%s", (good ? "&c" : "&r"), get_generic_name_by_vnum(aff->type), (!good && PRF_FLAGGED(to, PRF_SCREEN_READER)) ? " (debuff)" : "", (aff->cast_by == CAST_BY_ID(to) ? " (you)" : ""));
			add_string_hash(&str_hash, line, 1);
		}
	}
	
	// build DoTs
	LL_FOREACH(ch->over_time_effects, dot) {
		if (dot->max_stack > 1) {
			snprintf(lbuf, sizeof(lbuf), " (%d/%d)", dot->stack, dot->max_stack);
		}
		else {
			*lbuf = '\0';
		}
		
		if (details) {
			snprintf(line, sizeof(line), "&r%s&0%s (%s)%s", get_generic_name_by_vnum(dot->type), (PRF_FLAGGED(to, PRF_SCREEN_READER) ? " (DoT)" : ""), colon_time(dot->time_remaining, FALSE, "infinite"), lbuf);
		}
		else {	// simple version
			snprintf(line, sizeof(line), "&r%s&0%s%s", get_generic_name_by_vnum(dot->type), (PRF_FLAGGED(to, PRF_SCREEN_READER) ? " (DoT)" : ""), lbuf);
		}
		
		// caster?
		if (aff->cast_by == CAST_BY_ID(to) ? " (you)" : "") {
			snprintf(line + strlen(line), sizeof(line) - strlen(line), " (you)");
		}
		
		add_string_hash(&str_hash, line, 1);
	}
	
	// extra info?
	if (details) {
		if (MOB_FLAGGED(ch, MOB_ANIMAL) || CHAR_MORPH_FLAGGED(ch, MORPHF_ANIMAL)) {
			add_string_hash(&str_hash, "&canimal&0", 1);
		}
		if (IS_MAGE(ch)) {
			add_string_hash(&str_hash, "&ccaster&0", 1);
		}
		if (IS_VAMPIRE(ch)) {
			add_string_hash(&str_hash, "&cvampire&0", 1);
		}
	}
	
	// build display
	size = snprintf(output, sizeof(output), "Affects on %s:%s%s", PERS(ch, to, FALSE), details ? "\r\n" : "", (str_hash ? "" : " none"));
	HASH_ITER(hh, str_hash, str_iter, next_str) {
		if (details) {
			if (size + strlen(str_iter->str) + 3 < sizeof(output)) {
				size += snprintf(output + size, sizeof(output) - size, " %s\r\n", str_iter->str);
			}
		}
		else {	// simple version
			if (size + strlen(str_iter->str) + 4 < sizeof(output)) {
				size += snprintf(output + size, sizeof(output) - size, "%s %s", (str_iter != str_hash ? "," : ""), str_iter->str);
			}
			else {
				// full
				break;
			}
		}
	}
	
	free_string_hash(&str_hash);
	
	if (details) {
		// just show it
		page_string(to->desc, output, TRUE);
	}
	else {
		// formatting for simple view:
		strcat(output, "\r\n");	// space reserved
		
		temp = strdup(output);
		format_text(&temp, 0, to->desc, MAX_STRING_LENGTH);
		page_string(to->desc, temp, TRUE);
		free(temp);
	}
	
	gain_player_tech_exp(to, PTECH_ENEMY_BUFF_DETAILS, 15);
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT DISPLAY FUNCTIONS ////////////////////////////////////////////////

/**
* Allows items to be colored by their loot quality.
*
* @param obj_data *obj The object to color.
* @param char_data *ch The player who will see it (for preference flags).
*/
char *obj_color_by_quality(obj_data *obj, char_data *ch) {
	// static char output[8];
	
	if (!ch || !obj || REAL_NPC(ch)) {
		return "";
	}
	else if (OBJ_FLAGGED(obj, OBJ_HARD_DROP) && OBJ_FLAGGED(obj, OBJ_GROUP_DROP)) {
		return "\tM";
	}
	else if (OBJ_FLAGGED(obj, OBJ_GROUP_DROP)) {
		return "\tC";
	}
	else if (OBJ_FLAGGED(obj, OBJ_HARD_DROP)) {
		return "\tG";
	}
	else if (OBJ_FLAGGED(obj, OBJ_SUPERIOR)) {
		return "\tY";
	}
	else if (OBJ_FLAGGED(obj, OBJ_JUNK)) {
		return "\tw";
	}
	else {	// no quality
		return "\tn";
	}
}


/**
* @param obj_data *obj
* @param char_data *ch person to show it to
* @param int mode OBJ_DESC_SHORT, OBJ_DESC_LONG
*/
char *get_obj_desc(obj_data *obj, char_data *ch, int mode) {
	static char output[MAX_STRING_LENGTH];
	char sdesc[MAX_STRING_LENGTH], liqname[256];
	bool color = FALSE;
	
	if (PRF_FLAGGED(ch, PRF_ITEM_QUALITY) && (mode == OBJ_DESC_INVENTORY || mode == OBJ_DESC_EQUIPMENT || mode == OBJ_DESC_CONTENTS || mode == OBJ_DESC_WAREHOUSE)) {
		strcpy(output, obj_color_by_quality(obj, ch));
		color = TRUE;
	}
	else {
		*output = '\0';
	}

	/* sdesc will be empty unless the short desc is modified */
	*sdesc = '\0';

	if (IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_CONTENTS(obj) > 0 && (mode == OBJ_DESC_CONTENTS || mode == OBJ_DESC_INVENTORY || mode == OBJ_DESC_WAREHOUSE)) {
		strcpy(liqname, get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_NAME));
		if (!strstr(GET_OBJ_SHORT_DESC(obj), liqname)) {
			// only if it's not already in the name
			sprintf(sdesc, "%s of %s", GET_OBJ_SHORT_DESC(obj), liqname);
		}
	}
	else if (IS_AMMO(obj)) {
		sprintf(sdesc, "%s (%d)", GET_OBJ_SHORT_DESC(obj), MAX(1, GET_AMMO_QUANTITY(obj)));
	}

	switch (mode) {
		case OBJ_DESC_LONG: {
			if (IN_ROOM(obj) && ROOM_SECT_FLAGGED(IN_ROOM(obj), SECTF_FRESH_WATER | SECTF_OCEAN)) {
				// floating!?
				
				strcpy(output, (*sdesc ? sdesc : GET_OBJ_SHORT_DESC(obj)));
				CAP(output);
				
				if (materials[GET_OBJ_MATERIAL(obj)].floats) {
					strcat(output, " is floating in the water.");
				}
				else {
					strcat(output, " is sinking fast.");
				}
			}
			else {	// NOT floating in water, or not in a room at all
				if (*sdesc) {
					// custom short desc
					sprintf(output, "%s is lying here.", CAP(sdesc));
				}
				else {
					// normal long desc
					strcpy(output, GET_OBJ_LONG_DESC(obj));
				}
			}
			break;
		}
		case OBJ_DESC_SHORT:
		default: {
			if (*sdesc)
				strcat(output, sdesc);
			else
				strcat(output, GET_OBJ_SHORT_DESC(obj));
			break;
		}
	}
	if (color) {
		strcat(output, "\tn");
	}
	return output;
}


/**
* @param obj_data *list the objects to show
* @param char_data *ch the person to show it to
* @param int mode OBJ_DESC_x
* @param int show if TRUE, shows " Nothing." if nothing
*/
void list_obj_to_char(obj_data *list, char_data *ch, int mode, int show) {
	obj_data *i, *j = NULL;
	bool found = FALSE;
	int num;
	
	DL_FOREACH2(list, i, next_content) {
		num = 0;
		
		if (OBJ_CAN_STACK(i)) {
			// look for a previous matching item
			
			DL_FOREACH2(list, j, next_content) {
				if (j == i) {
					break;
				}
				else if (OBJ_CAN_STACK(j) && OBJS_ARE_SAME(i, j)) {
					if (GET_OBJ_VNUM(j) == NOTHING) {
						break;
					}
					else if (GET_OBJ_VNUM(j) == GET_OBJ_VNUM(i)) {
						break;
					}
				}
			}
			if (j != i) {
				// found one -- just continue
				continue;
			}
		
			// determine number
			DL_FOREACH2(i, j, next_content) {
				if (OBJ_CAN_STACK(j) && OBJS_ARE_SAME(i, j)) {
					if (GET_OBJ_VNUM(j) == NOTHING) {
						num++;
					}
					else if (GET_OBJ_VNUM(j) == GET_OBJ_VNUM(i)) {
						num++;
					}
				}
			}
		}

		if (CAN_SEE_OBJ(ch, i)) {
			if (mode == OBJ_DESC_LONG) {
				send_to_char("&g", ch);
			}
			if (num > 1) {
				msg_to_char(ch, "(%2i) ", num);
			}
			
			send_to_char(obj_desc_for_char(i, ch, mode), ch);
			found = TRUE;
		}
	}
	if (!found && show)
		msg_to_char(ch, " Nothing.\r\n");
}


/*
 * This function screams bitvector... -gg 6/45/98
 */
char *obj_desc_for_char(obj_data *obj, char_data *ch, int mode) {
	static char buf[MAX_STRING_LENGTH];
	char tags[MAX_STRING_LENGTH], flags[256];
	bitvector_t show_flags;
	int board_type;
	room_data *room;
	
	// tags will be a comma-separated list of extra info, and PROBABLY starts with a space that can be skipped
	*tags = '\0';

	// initialize buf as the obj desc
	if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
		sprintf(buf, "[%d] %s%s", GET_OBJ_VNUM(obj), (HAS_TRIGGERS(obj) ? "[TRIG] " : ""), GET_OBJ_DESC(obj, ch, mode));
	}
	else {
		strcpy(buf, GET_OBJ_DESC(obj, ch, mode));
	}

	if (mode == OBJ_DESC_EQUIPMENT) {
		if (obj->worn_by && AFF_FLAGGED(obj->worn_by, AFF_DISARMED) && (obj->worn_on == WEAR_WIELD || obj->worn_on == WEAR_RANGED || obj->worn_on == WEAR_HOLD)) {
			sprintf(tags + strlen(tags), "%s disarmed", (*tags ? "," : ""));
		}
	}
	
	if (mode == OBJ_DESC_INVENTORY || mode == OBJ_DESC_EQUIPMENT || mode == OBJ_DESC_CONTENTS || mode == OBJ_DESC_LONG || mode == OBJ_DESC_WAREHOUSE) {
		// show level:
		if (PRF_FLAGGED(ch, PRF_ITEM_DETAILS) && GET_OBJ_CURRENT_SCALE_LEVEL(obj) > 1 && mode != OBJ_DESC_LONG && mode != OBJ_DESC_LOOK_AT) {
			sprintf(tags + strlen(tags), "%s L-%s%d\t0", (*tags ? "," : ""), (PRF_FLAGGED(ch, PRF_ITEM_DETAILS) ? color_by_difficulty(ch, GET_OBJ_CURRENT_SCALE_LEVEL(obj)) : ""), GET_OBJ_CURRENT_SCALE_LEVEL(obj));
		}
		
		// prepare flags:
		show_flags = GET_OBJ_EXTRA(obj);
		if (!PRF_FLAGGED(ch, PRF_ITEM_DETAILS)) {
			show_flags &= ~OBJ_ENCHANTED;
		}
		
		if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
			if (PRF_FLAGGED(ch, PRF_ITEM_QUALITY)) {
				// screenreader needs item quality as text
				if (OBJ_FLAGGED(obj, OBJ_HARD_DROP) && OBJ_FLAGGED(obj, OBJ_GROUP_DROP)) {
					sprintf(tags + strlen(tags), "%s boss", (*tags ? "," : ""));
				}
				else if (OBJ_FLAGGED(obj, OBJ_GROUP_DROP)) {
					sprintf(tags + strlen(tags), "%s group", (*tags ? "," : ""));
				}
				else if (OBJ_FLAGGED(obj, OBJ_HARD_DROP)) {
					sprintf(tags + strlen(tags), "%s hard", (*tags ? "," : ""));
				}
			}
			else {	// only suppress superior if quality is off
				show_flags &= ~OBJ_SUPERIOR;
			}
			
			prettier_sprintbit(show_flags, extra_bits_inv_flags, flags);
		}
		else {	// non-screenreader: suppress superior flag here
			prettier_sprintbit((show_flags & ~OBJ_SUPERIOR), extra_bits_inv_flags, flags);
		}
		
		// append flags
		if (*flags && strncmp(flags, "NOBITS", 6) && strncmp(flags, "none", 4)) {
			sprintf(tags + strlen(tags), "%s %s", (*tags ? "," : ""), flags);
		}
		
		if (LIGHT_IS_LIT(obj)) {
			sprintf(tags + strlen(tags), "%s light", (*tags ? "," : ""));
		}
		else if (IS_LIGHT(obj) && GET_LIGHT_HOURS_REMAINING(obj) == 0) {
			sprintf(tags + strlen(tags), "%s burnt out", (*tags ? "," : ""));
		}
		
		if (PRF_FLAGGED(ch, PRF_ITEM_DETAILS) && GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
			sprintf(tags + strlen(tags), "%s quest", (*tags ? "," : ""));
		}
		
		if (PRF_FLAGGED(ch, PRF_ITEM_DETAILS) && OBJ_FLAGGED(obj, OBJ_BIND_ON_EQUIP) && !OBJ_BOUND_TO(obj)) {
			sprintf(tags + strlen(tags), "%s unbound", (*tags ? "," : ""));
		}
		
		if (IS_STOLEN(obj)) {
			sprintf(tags + strlen(tags), "%s STOLEN", (*tags ? "," : ""));
		}
	}
	
	if (mode == OBJ_DESC_INVENTORY || (mode == OBJ_DESC_LONG && CAN_WEAR(obj, ITEM_WEAR_TAKE))) {
		if (can_get_quest_from_obj(ch, obj, NULL)) {
			sprintf(tags + strlen(tags), "%s quest available", (*tags ? "," : ""));
		}
		if (can_turn_quest_in_to_obj(ch, obj, NULL)) {
			sprintf(tags + strlen(tags), "%s finished quest", (*tags ? "," : ""));
		}
	}
	
	if (*tags) {
		// tags starts with a space so we use tags+1 here
		sprintf(buf + strlen(buf), " (%s)", tags+1);
	}
	
	if (mode == OBJ_DESC_LOOK_AT) {
		if (GET_OBJ_TYPE(obj) == ITEM_BOARD) {
			if (!board_loaded) {
				init_boards();
				board_loaded = 1;
				}
			if ((board_type = find_board(ch)) != -1)
				if (Board_show_board(board_type, ch, "board", obj))
					return "";
			strcpy(buf, "You see nothing special.");
		}
		else if (GET_OBJ_TYPE(obj) == ITEM_MAIL) {
			return GET_OBJ_ACTION_DESC(obj) ? GET_OBJ_ACTION_DESC(obj) : "It's blank.\r\n";
		}
		else if (IS_PORTAL(obj)) {
			room = real_room(GET_PORTAL_TARGET_VNUM(obj));
			if (room) {
				sprintf(buf, "%sYou peer into %s and see: %s%s\t0", NULLSAFE(GET_OBJ_ACTION_DESC(obj)), GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT), get_room_name(room, TRUE), coord_display_room(ch, room, FALSE));
			}
			else {
				sprintf(buf, "%sIt's a portal, but it doesn't seem to lead anywhere.", NULLSAFE(GET_OBJ_ACTION_DESC(obj)));
			}
		}
		else if (GET_OBJ_ACTION_DESC(obj) && *GET_OBJ_ACTION_DESC(obj)) {
			strcpy(buf, GET_OBJ_ACTION_DESC(obj));
		}
		else if (GET_OBJ_TYPE(obj) != ITEM_DRINKCON)
			strcpy(buf, "You see nothing special.");
		else {
			/* ITEM_TYPE == ITEM_DRINKCON */
			strcpy(buf, "It looks like a drink container.");
		}
	}
	
	// CRLF IS HERE (not for warehouse)
	if (buf[strlen(buf)-1] != '\n' && mode != OBJ_DESC_WAREHOUSE) {
		strcat(buf, "\r\n");
	}
	
	if (mode == OBJ_DESC_LOOK_AT || (mode == OBJ_DESC_LONG && !CAN_WEAR(obj, ITEM_WEAR_TAKE))) {
		if (can_get_quest_from_obj(ch, obj, NULL)) {
			strcat(buf, "...it has a quest for you!\r\n");
		}
		if (can_turn_quest_in_to_obj(ch, obj, NULL)) {
			strcat(buf, "...you can turn in a quest here!\r\n");
		}
	}
	
	return buf;
}


/**
* Shows empire inventory available in the room, based on building/vehicles
* present. This is the character's own empire UNLESS 'thief_mode' is set,
* in which case it shows any empire with storage present.
*
* @param char_data *ch The person looking at storage.
* @param room_data *room The location (will include vehicles in the room too).
* @param bool thief_mode If TRUE, indicates ownership and shows other empires' things.
* @return bool TRUE if any items were shown at all; otherwise FALSE.
*/
bool show_local_einv(char_data *ch, room_data *room, bool thief_mode) {
	struct vnum_hash *vhash = NULL, *vhash_iter, *vhash_next;
	empire_data *own_empire = GET_LOYALTY(ch), *emp;
	struct empire_storage_data *store, *next_store;
	struct empire_island *eisle;
	bool found_one, found_any;
	vehicle_data *veh;
	
	if (!thief_mode && !own_empire) {
		return FALSE;	// no empire - nothing to show
	}
	
	// show vault info first (own empire only; ignores thief mode)
	if (own_empire == ROOM_OWNER(room) && room_has_function_and_city_ok(GET_LOYALTY(ch), room, FNC_VAULT)) {
		msg_to_char(ch, "\r\nVault: %.1f coin%s, %d treasure (%d total)\r\n", EMPIRE_COINS(own_empire), (EMPIRE_COINS(own_empire) != 1.0 ? "s" : ""), EMPIRE_WEALTH(own_empire), (int) GET_TOTAL_WEALTH(own_empire));
	}
	
	// build a list of empires to check here
	if (thief_mode) {
		if (ROOM_OWNER(room)) {
			add_vnum_hash(&vhash, EMPIRE_VNUM(ROOM_OWNER(room)), 1);
		}
		DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
			if (!VEH_IS_EXTRACTED(veh) && VEH_OWNER(veh)) {
				add_vnum_hash(&vhash, EMPIRE_VNUM(VEH_OWNER(veh)), 1);
			}
		}
	}
	else {
		add_vnum_hash(&vhash, EMPIRE_VNUM(own_empire), 1);
	}
	
	// determines return val
	found_any = FALSE;
	
	// iterate over empire list (only if on an island)
	if (GET_ISLAND_ID(room) != NO_ISLAND) {
		HASH_ITER(hh, vhash, vhash_iter, vhash_next) {
			if (!(emp = real_empire(vhash_iter->vnum))) {
				continue;	// should be impossible
			}
		
			// look for storage here
			found_one = FALSE;
			eisle = get_empire_island(emp, GET_ISLAND_ID(room));
			HASH_ITER(hh, eisle->store, store, next_store) {
				if (store->amount > 0 && store->proto && obj_can_be_retrieved(store->proto, room, thief_mode ? NULL : own_empire)) {
					if (!found_one) {
						snprintf(buf, sizeof(buf), "%s inventory available here:\t0\r\n", EMPIRE_ADJECTIVE(emp));
						CAP(buf);
						msg_to_char(ch, "\r\n%s%s", EMPIRE_BANNER(emp), buf);
					}
			
					show_one_stored_item_to_char(ch, emp, store, FALSE);
					found_one = found_any = TRUE;
				}
			}
		}
	}
	
	free_vnum_hash(&vhash);
	return found_any;
}


/**
* display one stored item to ch
*
* @param char_data *ch Person to show it to.
* @param empire_data *emp The empire who owns it.
* @param struct empire_storage_data *store The storage entry to show.
* @param bool show_zero Forces an amount of 0, in case this is only being shown for the "total" reference and there are actually 0 here.
*/
void show_one_stored_item_to_char(char_data *ch, empire_data *emp, struct empire_storage_data *store, bool show_zero) {
	bool show_own_data = (GET_LOYALTY(ch) == emp || GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES));
	int total = get_total_stored_count(emp, store->vnum, TRUE);
	char lbuf[MAX_INPUT_LENGTH], keepstr[256];
	
	if (show_own_data && (total > store->amount || show_zero)) {
		sprintf(lbuf, " (%d total)", total);
	}
	else {
		*lbuf = '\0';
	}
	
	if (show_own_data && store->keep == UNLIMITED) {
		strcpy(keepstr, " (keep)");
	}
	else if (show_own_data && store->keep > 0) {
		snprintf(keepstr, sizeof(keepstr), " (keep %d)", store->keep);
	}
	else {
		*keepstr = '\0';
	}
	
	msg_to_char(ch, "(%4d) %s%s%s\r\n", (show_zero ? 0 : store->amount), store->proto ? GET_OBJ_SHORT_DESC(store->proto) : get_obj_name_by_proto(store->vnum), keepstr, lbuf);
}


 //////////////////////////////////////////////////////////////////////////////
//// WHO LIST PARTS //////////////////////////////////////////////////////////

// who types
#define WHO_MORTALS  0
#define WHO_GODS  1
#define WHO_IMMORTALS  2

#define WHO_SORTER(name)  int (name)(struct who_entry *a, struct who_entry *b)

// for sortable who list
struct who_entry {
	int access_level;
	int computed_level;
	int role;
	char *string;
	struct who_entry *next;
};


WHO_SORTER(sort_who_access_level) {
	if (a->access_level != b->access_level) {
		return a->access_level - b->access_level;
	}
	return 0;
}


WHO_SORTER(sort_who_level) {
	if (a->computed_level != b->computed_level) {
		return a->computed_level - b->computed_level;
	}
	return 0;
}


WHO_SORTER(sort_who_role_level) {
	if (a->role != b->role) {
		return a->role - b->role;
	}
	if (a->computed_level != b->computed_level) {
		return a->computed_level - b->computed_level;
	}
	return 0;
}


/**
* Get the "who" display for one person.
*
* @param char_data *ch The person to get WHO info for.
* @param bool shortlist If TRUE, only gets a short entry.
* @param bool screenreader If TRUE, shows slightly differently
* @return char* A pointer to the output.
*/
char *one_who_line(char_data *ch, bool shortlist, bool screenreader) {
	static char out[MAX_STRING_LENGTH];
	char buf1[MAX_STRING_LENGTH], show_role[24], part[MAX_STRING_LENGTH];
	int num, size = 0;
	
	*out = '\0';
	
	if (screenreader && GET_CLASS_ROLE(ch) != ROLE_NONE) {
		snprintf(show_role, sizeof(show_role), " %s", class_role[GET_CLASS_ROLE(ch)]);
	}
	else {
		*show_role = '\0';
	}
	
	// level/class info
	if (!IS_GOD(ch) && !IS_IMMORTAL(ch)) {
		if (shortlist) {
			size += snprintf(out + size, sizeof(out) - size, "[%s%3d%s] ", screenreader ? "" : class_role_color[GET_CLASS_ROLE(ch)], GET_COMPUTED_LEVEL(ch), screenreader ? "" : "\t0");
		}
		else {
			get_player_skill_string(ch, part, !screenreader);
			size += snprintf(out + size, sizeof(out) - size, "[%3d %s%s%s] ", GET_COMPUTED_LEVEL(ch), screenreader ? "" : class_role_color[GET_CLASS_ROLE(ch)], part, screenreader ? show_role : "\t0");
		}
	}
	
	// rank
	if (GET_LOYALTY(ch)) {
		size += snprintf(out + size, sizeof(out) - size, "<%s&0> ", EMPIRE_RANK(GET_LOYALTY(ch), GET_RANK(ch)-1));
	}

	// name
	size += snprintf(out + size, sizeof(out) - size, "%s", PERS(ch, ch, TRUE));
	
	// shortlist ends here
	if (shortlist) {
		// append invis even in short list
		if (GET_INVIS_LEV(ch)) {
			size += snprintf(out + size, sizeof(out) - size, " (i%d)", GET_INVIS_LEV(ch));
		}
		else if (IS_IMMORTAL(ch) && PRF_FLAGGED(ch, PRF_INCOGNITO)) {
			size += snprintf(out + size, sizeof(out) - size, " (inc)");
		}
		
		// determine length to show
		if (!screenreader) {
			num = color_code_length(out);
			strcpy(buf1, out);
			size = snprintf(out, sizeof(out), "%-*.*s", 35 + num, 35 + num, buf1);
		}
		return out;
	}
	
	// title
	size += snprintf(out + size, sizeof(out) - size, "%s&0", NULLSAFE(GET_TITLE(ch)));
	
	// tags
	if (IS_AFK(ch)) {
		size += snprintf(out + size, sizeof(out) - size, " &r(AFK)&0");
	}
	if ((GET_IDLE_SECONDS(ch) / SECS_PER_REAL_MIN) >= 5) {
		size += snprintf(out + size, sizeof(out) - size, " (idle: %d)", (GET_IDLE_SECONDS(ch) / SECS_PER_REAL_MIN));
	}
	if (IS_PVP_FLAGGED(ch)) {
		size += snprintf(out + size, sizeof(out) - size, " &R(PVP)&0");
	}
	if (PRF_FLAGGED(ch, PRF_RP)) {
		size += snprintf(out + size, sizeof(out) - size, " &m(RP)&0");
	}
	if (get_cooldown_time(ch, COOLDOWN_ROGUE_FLAG) > 0) {
		size += snprintf(out + size, sizeof(out) - size, " &M(rogue)&0");
	}

	if (GET_INVIS_LEV(ch)) {
		size += snprintf(out + size, sizeof(out) - size, " (i%d)", GET_INVIS_LEV(ch));
	}
	if (IS_IMMORTAL(ch) && PRF_FLAGGED(ch, PRF_INCOGNITO)) {
		size += snprintf(out + size, sizeof(out) - size, " (incog)");
	}
	else if (AFF_FLAGGED(ch, AFF_INVISIBLE)) {
		size += snprintf(out + size, sizeof(out) - size, " (invis)");
	}
	if (PRF_FLAGGED(ch, PRF_DEAF)) {
		size += snprintf(out + size, sizeof(out) - size, " (deaf)");
	}
	if (PRF_FLAGGED(ch, PRF_NOTELL)) {
		size += snprintf(out + size, sizeof(out) - size, " (notell)");
	}

	size += snprintf(out + size, sizeof(out) - size, "&0\r\n");
	return out;
}


/**
* Builds part of the WHO list.
*
* @param char_data *ch The person performing the who command.
* @param char *name_search If filtering names, the filter string.
* @param int low Minimum level to show.
* @param int high Maximum level to show.
* @param empire_data *empire_who If not null, only shows members of that empire.
* @param bool rp If TRUE, only shows RP players.
* @param bool shortlist If TRUE, gets the columnar short form.
* @param int type WHO_MORTALS, WHO_GODS, or WHO_IMMORTALS
* @return char* The who output for imms.
*/
char *partial_who(char_data *ch, char *name_search, int low, int high, empire_data *empire_who, bool rp, bool shortlist, int type) {
	static char who_output[MAX_STRING_LENGTH];
	struct who_entry *list = NULL, *entry, *next_entry;
	char whobuf[MAX_STRING_LENGTH], buf[MAX_STRING_LENGTH], online[MAX_STRING_LENGTH];
	descriptor_data *d;
	char_data *tch;
	int iter, count = 0, size, max_pl;
	
	// WHO_x
	const char *who_titles[] = { "Mortals", "Gods", "Immortals" };
	WHO_SORTER(*who_sorters[3]) = { NULL, sort_who_level, sort_who_access_level };
	
	// set sorters
	switch (config_get_type("who_list_sort")) {
		case WHO_LIST_SORT_LEVEL: {
			who_sorters[WHO_MORTALS] = sort_who_level;
			break;
		}
		case WHO_LIST_SORT_ROLE_LEVEL:
		default: {
			who_sorters[WHO_MORTALS] = sort_who_role_level;
			break;
		}
	}

	*whobuf = '\0';	// lines of chars
	size = 0;	// whobuf size

	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) != CON_PLAYING)
			continue;

		if (d->original)
			tch = d->original;
		else if (!(tch = d->character))
			continue;

		if (*name_search && !is_abbrev(name_search, PERS(tch, tch, 1)) && !strstr(NULLSAFE(GET_TITLE(tch)), name_search))
			continue;
		if (!CAN_SEE_GLOBAL(ch, tch)) {
			continue;
		}
		if (low != 0 && GET_COMPUTED_LEVEL(tch) < low) {
			continue;
		}
		if (high != 0 && GET_COMPUTED_LEVEL(tch) > high) {
			continue;
		}
		if (type == WHO_MORTALS && (IS_GOD(tch) || IS_IMMORTAL(tch)))
			continue;
		if (type == WHO_GODS && !IS_GOD(tch))
			continue;
		if (type == WHO_IMMORTALS && !IS_IMMORTAL(tch))
			continue;
		if (empire_who && GET_LOYALTY(tch) != empire_who)
			continue;
		if (rp && !PRF_FLAGGED(tch, PRF_RP))
			continue;
		if (!INCOGNITO_OK(ch, tch))
			continue;

		// show one char
		CREATE(entry, struct who_entry, 1);
		entry->access_level = GET_ACCESS_LEVEL(tch);
		entry->computed_level = GET_COMPUTED_LEVEL(tch);
		entry->role = GET_CLASS_ROLE(tch);
		entry->string = str_dup(one_who_line(tch, shortlist, PRF_FLAGGED(ch, PRF_SCREEN_READER)));
		LL_PREPEND(list, entry);
	}
	
	LL_SORT(list, who_sorters[type]);

	for (entry = list; entry; entry = next_entry) {
		next_entry = entry->next;
		
		++count;
		size += snprintf(whobuf + size, sizeof(whobuf) - size, "%s", entry->string);
		
		// columnar spacing
		if (shortlist) {
			size += snprintf(whobuf + size, sizeof(whobuf) - size, "%s", (!(count % 2) || PRF_FLAGGED(ch, PRF_SCREEN_READER)) ? "\r\n" : " ");
		}
		
		free(entry->string);
		free(entry);
	}
	list = NULL;

	if (*whobuf) {
		// repurposing size
		size = 0;
		
		if (type == WHO_MORTALS) {
			// update counts in case
			max_pl = data_get_int(DATA_MAX_PLAYERS_TODAY);
			if (count > max_pl) {
				max_pl = count;
				data_set_int(DATA_MAX_PLAYERS_TODAY, max_pl);
			}
			max_players_this_uptime = MAX(max_players_this_uptime, count);
			snprintf(online, sizeof(online), "%d online (max today %d, this uptime %d)", count, max_pl, max_players_this_uptime);
		}
		else {
			snprintf(online, sizeof(online), "%d online", count);
		}
		
		size += snprintf(who_output + size, sizeof(who_output) - size, "%s: %s", who_titles[type], online);

		// divider
		*buf = '\0';
		for (iter = 0; iter < strlen(who_output); ++iter) {
			buf[iter] = '-';
		}
		buf[iter] = '\0';
		
		size += snprintf(who_output + size, sizeof(who_output) - size, "\r\n%s\r\n%s", buf, whobuf);
		
		if (shortlist && (count % 2) && !PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
			size += snprintf(who_output + size, sizeof(who_output) - size, "\r\n");
		}
	}
	else {
		*who_output = '\0';
	}
	
	return who_output;
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_adventure) {
	char arg[MAX_STRING_LENGTH];
	struct instance_data *inst;
	
	if (*argument) {
		argument = any_one_arg(argument, arg);
		if (is_abbrev(arg, "summon")) {
			do_adventure_summon(ch, argument);
			return;
		}
		// otherwise fall through to the rest of the command
	}
	
	if (!(inst = find_instance_by_room(IN_ROOM(ch), FALSE, TRUE))) {
		msg_to_char(ch, "You are not in or near an adventure zone.\r\n");
		return;
	}

	msg_to_char(ch, "%s (%s)\r\n", NULLSAFE(GET_ADV_NAME(INST_ADVENTURE(inst))), instance_level_string(inst));
	
	if (GET_ADV_AUTHOR(INST_ADVENTURE(inst)) && *(GET_ADV_AUTHOR(INST_ADVENTURE(inst)))) {
		msg_to_char(ch, "by %s\r\n", GET_ADV_AUTHOR(INST_ADVENTURE(inst)));
	}
	msg_to_char(ch, "%s", NULLSAFE(GET_ADV_DESCRIPTION(INST_ADVENTURE(inst))));
	if (GET_ADV_PLAYER_LIMIT(INST_ADVENTURE(inst)) > 0) {
		msg_to_char(ch, "Player limit: %d\r\n", GET_ADV_PLAYER_LIMIT(INST_ADVENTURE(inst)));
	}
}


ACMD(do_affects) {
	char_data *vict;
	int i;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	// targeted version?
	one_argument(argument, arg);
	if (*arg) {
		if (GET_POS(ch) < POS_RESTING) {
			send_low_pos_msg(ch);
		}
		else if (!generic_find(arg, NULL, FIND_CHAR_ROOM | (IS_IMMORTAL(ch) ? FIND_CHAR_WORLD : NOBITS), ch, &vict, NULL, NULL)) {
			send_config_msg(ch, "no_person");
		}
		else if (IS_IMMORTAL(ch)) {
			msg_to_char(ch, "Affects on %s:\r\n", PERS(vict, ch, FALSE));
			show_character_affects(vict, ch);
		}
		else {
			show_character_affects_simple(vict, ch);
		}
		return;
	}

	msg_to_char(ch, "  Affects:\r\n");

	/* Conditions */
	// This reports conditions all on one line -- end each one with a comma and a space
	sprintf(buf1, "   You are ");
	if (IS_HUNGRY(ch)) {
		strcat(buf1, "hungry, ");
	}
	if (IS_THIRSTY(ch)) {
		strcat(buf1, "thirsty, ");
	}
	if (IS_DRUNK(ch)) {
		strcat(buf1, "inebriated, ");
	}
	if (IS_BLOOD_STARVED(ch)) {
		strcat(buf1, "starving, ");
	}

	if (strlen(buf1) > 13) {	/* We have a condition */
		buf1[strlen(buf1)-2] = '\0';	/* This removes the final ", " */
		for (i = strlen(buf1); i >= 0; i--)
			if (buf1[i] == ',') {	/* Looking for that last ',' */
				strcpy(buf2, buf1 + i + 1);
				sprintf(buf1 + i, " and%s", buf2);
				break;
			}
		msg_to_char(ch, "%s.\r\n", buf1);
	}
	
	// mount
	if (IS_RIDING(ch)) {
		msg_to_char(ch, "   You are riding %s.\r\n", get_mob_name_by_proto(GET_MOUNT_VNUM(ch), TRUE));
	}
	else if (has_player_tech(ch, PTECH_RIDING) && GET_MOUNT_VNUM(ch) != NOTHING && mob_proto(GET_MOUNT_VNUM(ch))) {
		msg_to_char(ch, "   You have %s. Type 'mount' to ride it.\r\n", get_mob_name_by_proto(GET_MOUNT_VNUM(ch), TRUE));
	}

	/* Morph */
	if (IS_MORPHED(ch)) {
		msg_to_char(ch, "   You are in the form of %s!\r\n", get_morph_desc(ch, FALSE));
	}
	else if (IS_DISGUISED(ch)) {
		msg_to_char(ch, "   You are disguised as %s!\r\n", PERS(ch, ch, 0));
	}

	show_character_affects(ch, ch);
}


ACMD(do_buffs) {
	char output[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	size_t out_size, line_size;
	bool any = FALSE, error, other, own;
	ability_data *abil;
	struct affected_type *aff;
	char_data *caster;
	struct group_member_data *mem;
	struct player_ability_data *plab, *next_plab;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs don't have abilities.\r\n");
		return;
	}
	
	// start string
	out_size = snprintf(output, sizeof(output), "Your buff abilities:\r\n");
	
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		if (!(abil = plab->ptr)) {
			continue;	// no abil somehow?
		}
		if (!plab->purchased[GET_CURRENT_SKILL_SET(ch)]) {
			continue;	// not purchased on current set
		}
		if (!IS_SET(ABIL_TYPES(abil), ABILT_BUFF) || ABILITY_FLAGGED(abil, ABILF_VIOLENT)) {
			continue;	// ability is not a buff, or is a debuff
		}
		if (ABIL_AFFECT_VNUM(abil) == NOTHING) {
			continue;	// no buff we can detect
		}
		if (!IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_ROOM | ATAR_SELF_ONLY)) {
			continue;	// not a targeted character buff
		}
		
		// build output
		any = TRUE;
		error = FALSE;
		line_size = snprintf(line, sizeof(line), " %s:", ABIL_NAME(abil));
		
		// check self?
		if (!IS_SET(ABIL_TARGETS(abil), ATAR_NOT_SELF)) {
			own = other = FALSE;
			caster = NULL;
			LL_FOREACH(ch->affected, aff) {
				if (aff->type == ABIL_AFFECT_VNUM(abil)) {
					if (aff->cast_by == GET_IDNUM(ch)) {
						own = TRUE;
					}
					else {
						other = TRUE;
						caster = is_playing(aff->cast_by);
					}
					// only need 1 match
					break;
				}
			}
			if (!own && other) {
				line_size += snprintf(line + line_size, sizeof(line) - line_size, " \tyon self from %s\t0", (caster ? GET_NAME(caster) : "other caster"));
				error = TRUE;
			}
			else if (!own) {
				line_size += snprintf(line + line_size, sizeof(line) - line_size, " \trmissing on self\t0");
				error = TRUE;
			}
		}
		
		// check companion?
		if (GET_COMPANION(ch) && !IS_SET(ABIL_TARGETS(abil), ATAR_SELF_ONLY)) {
			own = other = FALSE;
			caster = NULL;
			LL_FOREACH(GET_COMPANION(ch)->affected, aff) {
				if (aff->type == ABIL_AFFECT_VNUM(abil)) {
					if (aff->cast_by == GET_IDNUM(ch)) {
						own = TRUE;
					}
					else {
						other = TRUE;
						caster = is_playing(aff->cast_by);
					}
					// only need 1 match
					break;
				}
			}
			if (!own && other) {
				line_size += snprintf(line + line_size, sizeof(line) - line_size, "%s \tyon companion from %s\t0", (error ? "," : ""), (caster ? GET_NAME(caster) : "other caster"));
				error = TRUE;
			}
			else if (!own) {
				line_size += snprintf(line + line_size, sizeof(line) - line_size, "%s \trmissing on companion\t0", (error ? "," : ""));
				error = TRUE;
			}
		}
		
		// check party?
		if (GROUP(ch) && !IS_SET(ABIL_TARGETS(abil), ATAR_SELF_ONLY)) {
			LL_FOREACH(GROUP(ch)->members, mem) {
				if (mem->member == ch) {
					continue;
				}
				
				own = other = FALSE;
				caster = NULL;
				LL_FOREACH(mem->member->affected, aff) {
					if (aff->type == ABIL_AFFECT_VNUM(abil)) {
						if (aff->cast_by == GET_IDNUM(ch)) {
							own = TRUE;
						}
						else {
							other = TRUE;
							caster = is_playing(aff->cast_by);
						}
						// only need 1 match
						break;
					}
				}
				if (!own && other) {
					line_size += snprintf(line + line_size, sizeof(line) - line_size, "%s \tyon %s from %s\t0", (error ? "," : ""), GET_NAME(mem->member), (caster ? GET_NAME(caster) : "other caster"));
					error = TRUE;
				}
				else if (!own) {
					line_size += snprintf(line + line_size, sizeof(line) - line_size, "%s \trmissing on %s\t0", (error ? "," : ""), GET_NAME(mem->member));
					error = TRUE;
				}
			}
		}
		
		// ok?
		if (!error) {
			line_size += snprintf(line + line_size, sizeof(line) - line_size, " \tgok\t0");
		}
		
		// append?
		if (out_size + line_size + 20 < sizeof(output)) {
			out_size += snprintf(output + out_size, sizeof(output) - out_size, "%s\r\n", line);
		}
		else {
			// space is reserved for an OVERFLOW
			out_size += snprintf(output + out_size, sizeof(output) - out_size, "OVERFLOW\r\n");
			break;
		}
	}
	
	if (!any) {
		// always room left if none were found
		out_size += snprintf(output + out_size, sizeof(output) - out_size, " none\r\n");
	}
	
	if (ch->desc) {
		page_string(ch->desc, output, TRUE);
	}
}


ACMD(do_chart) {
	struct chart_territory *citer, *next_citer, *hash = NULL;
	struct empire_city_data *city;
	struct empire_island *e_isle, *eiter, *next_eiter;
	empire_data *emp, *next_emp;
	int iter, total_claims, num;
	struct island_info *isle, *isle_iter, *next_isle;
	bool any, city_prompt;
	char buf[MAX_STRING_LENGTH];
	
	skip_spaces(&argument);
	
	if (!*argument) {
		msg_to_char(ch, "Get chart information on which island?\r\n");
	}
	else if (!(isle = get_island_by_name(ch, argument)) || isle->id == NO_ISLAND) {
		msg_to_char(ch, "Unknown island.\r\n");
	}
	else if (IS_SET(isle->flags, ISLE_NO_CHART)) {
		msg_to_char(ch, "That island doesn't appear on any charts.\r\n");
	}
	else {
		msg_to_char(ch, "Chart information for %s:\r\n", get_island_name_for(isle->id, ch));
		
		// alternate names
		if (isle->id != NO_ISLAND) {
			if (GET_LOYALTY(ch) && (e_isle = get_empire_island(GET_LOYALTY(ch), isle->id)) && e_isle->name && strcmp(e_isle->name, isle->name)) {
				// show global name if different
				msg_to_char(ch, "Also known as: %s", isle->name);
				num = 1;
			}
			else {
				num = 0;	// not shown yet
			}
		
			// alternate names
			num = 0;
			HASH_ITER(hh, empire_table, emp, next_emp) {
				if (EMPIRE_IS_TIMED_OUT(emp) || emp == GET_LOYALTY(ch)) {
					continue;
				}
				if (!IS_IMMORTAL(ch) && !has_relationship(GET_LOYALTY(ch), emp, DIPL_NONAGGR | DIPL_ALLIED | DIPL_TRADE)) {
					continue;
				}
				if (!(e_isle = get_empire_island(emp, isle->id)) || !e_isle->name || !str_cmp(e_isle->name, isle->name)) {
					continue;
				}
			
				// definitely has a name
				if (num > 0) {
					msg_to_char(ch, ", %s (%s)", e_isle->name, EMPIRE_NAME(emp));
				}
				else {
					msg_to_char(ch, "Also known as: %s (%s)", e_isle->name, EMPIRE_NAME(emp));
				}
				++num;
			}
			if (num > 0) {
				msg_to_char(ch, "\r\n");
			}
		}
		
		// desc
		if (isle->desc) {
			send_to_char(isle->desc, ch);
		}
		
		if (HAS_NAVIGATION(ch) && isle->center != NOWHERE) {
			msg_to_char(ch, "Approximate center: (%d, %d)\r\n", MAP_X_COORD(isle->center), MAP_Y_COORD(isle->center));
			msg_to_char(ch, "Edges: ");
			any = FALSE;
			for (iter = 0; iter < NUM_SIMPLE_DIRS; ++iter) {
				if (isle->edge[iter] != NOWHERE) {
					msg_to_char(ch, "%s(%d, %d)", (any ? ", " : ""), MAP_X_COORD(isle->edge[iter]), MAP_Y_COORD(isle->edge[iter]));
					any = TRUE;
				}
			}
			msg_to_char(ch, "%s\r\n", any ? "" : "unknown");
		}
		
		// collect empire data on the island
		total_claims = 0;
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if (isle->id == NO_ISLAND || !(e_isle = get_empire_island(emp, isle->id))) {
				continue;
			}
			
			if (e_isle->territory[TER_TOTAL] > 0) {
				chart_add_territory(&hash, emp, e_isle->territory[TER_TOTAL]);
				SAFE_ADD(total_claims, e_isle->territory[TER_TOTAL], 0, INT_MAX, FALSE);
			}
			
			LL_FOREACH(EMPIRE_CITY_LIST(emp), city) {
				if (GET_ISLAND(city->location) != isle) {
					continue;
				}
				
				chart_add_city(&hash, emp, city);
			}
		}
		
		HASH_SORT(hash, sort_chart_hash);
		
		// claim info
		if (total_claims > 0 && isle->tile_size > 0) {
			msg_to_char(ch, "Percent claimed: %d%%, Largest empires: ", (total_claims * 100 / isle->tile_size));
			
			// only show some (largest, per sort_chart_hash)
			num = 0;
			HASH_ITER(hh, hash, citer, next_citer) {
				msg_to_char(ch, "%s%s%s\t0", (num > 0 ? ", " : ""), EMPIRE_BANNER(citer->emp), EMPIRE_NAME(citer->emp));
				if (++num >= 3) {
					break;
				}
			}
			msg_to_char(ch, "\r\n");
		}
		
		// cities
		city_prompt = FALSE;
		
		HASH_ITER(hh, hash, citer, next_citer) {
			if (!citer->largest_city || !city_type[citer->largest_city->type].show_to_others) {
				continue;
			}
			
			if (!city_prompt) {
				msg_to_char(ch, "Notable cities:\r\n");
				city_prompt = TRUE;
			}
			
			msg_to_char(ch, " The %s%s\t0 %s of %s%s\r\n", EMPIRE_BANNER(citer->emp), EMPIRE_ADJECTIVE(citer->emp), city_type[citer->largest_city->type].name, citer->largest_city->name, coord_display_room(ch, citer->largest_city->location, FALSE));
		}
		
		free_chart_hash(hash);
		
		// other islands with similar names
		*buf = '\0';
		HASH_ITER(hh, island_table, isle_iter, next_isle) {
			if (isle_iter == isle) {
				continue;	// same isle
			}
			if (!is_multiword_abbrev(argument, isle_iter->name)) {
				continue;	// no match
			}
			
			// found
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s%s", (*buf ? ", " : ""), isle_iter->name);
		}
		if (GET_LOYALTY(ch)) {
			HASH_ITER(hh, EMPIRE_ISLANDS(GET_LOYALTY(ch)), eiter, next_eiter) {
				if (eiter->island == isle->id) {
					continue;	// same isle
				}
				if (!eiter->name) {
					continue;	// no custom name
				}
				if (!is_multiword_abbrev(argument, eiter->name)) {
					continue;	// no match
				}
				
				// found
				snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s%s", (*buf ? ", " : ""), eiter->name);
			}
		}
		if (*buf) {
			msg_to_char(ch, "Islands with similar names: %s\r\n", buf);
		}
	}
}


// will show all currencies if the subcmd == TRUE
ACMD(do_coins) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], vstr[64], adv_part[128];
	struct player_currency *cur, *next_cur;
	bool any = FALSE;
	size_t size = 0;
	adv_data *adv;
	generic_data *gen;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs don't carry coins.\r\n");
		return;
	}
	
	skip_spaces(&argument);
	*buf = '\0';
	
	// basic coins -- only show if no-arg
	if (!*argument) {
		coin_string(GET_PLAYER_COINS(ch), line);
		size = snprintf(buf, sizeof(buf), "You have %s.\r\n", line);
	}
	
	if (GET_CURRENCIES(ch) && subcmd) {
		size += snprintf(buf + size, sizeof(buf) - size, "You%s have:\r\n", *argument ? "" : " also");
		
		HASH_ITER(hh, GET_CURRENCIES(ch), cur, next_cur) {
			if (*argument && !multi_isname(argument, get_generic_string_by_vnum(cur->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(cur->amount)))) {
				continue; // no keyword match
			}
			
			if ((gen = real_generic(cur->vnum)) && GEN_FLAGGED(gen, GEN_SHOW_ADVENTURE) && (adv = get_adventure_for_vnum(cur->vnum))) {
				snprintf(adv_part, sizeof(adv_part), " (%s)", GET_ADV_NAME(adv));
			}
			else {
				*adv_part = '\0';
			}
			
			if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
				sprintf(vstr, "[%5d] ", cur->vnum);
			}
			else {
				*vstr = '\0';
			}
			
			snprintf(line, sizeof(line), "%s%3d %s%s\r\n", vstr, cur->amount, get_generic_string_by_vnum(cur->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(cur->amount)), adv_part);
			any = TRUE;
			
			if (size + strlen(line) < sizeof(buf)) {
				strcat(buf, line);
				size += strlen(line);
			}
			else {
				break;
			}
		}
	}
	
	if (*argument && !any) {
		msg_to_char(ch, "You have no special currency called '%s'.\r\n", argument);
	}
	else if (ch->desc) {	// show currency
		page_string(ch->desc, buf, TRUE);
	}
}


ACMD(do_contents) {
	bool can_see_anything = FALSE;
	vehicle_data *veh;
	obj_data *obj;
	
	skip_spaces(&argument);
	if (*argument) {
		msg_to_char(ch, "This command only gets the contents of the room. To see the contents of an object, try 'look in <object>'.\r\n");
		return;
	}
	
	// verify we can see even 1 obj
	if (!can_see_anything) {
		DL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_content) {
			if (CAN_SEE_OBJ(ch, obj)) {
				can_see_anything = TRUE;
				break;	// only need 1
			}
		}
	}
	// verify we can see even 1 vehicle
	if (!can_see_anything) {
		DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
			if (!VEH_IS_EXTRACTED(veh) && CAN_SEE_VEHICLE(ch, veh)) {
				can_see_anything = TRUE;
				break;	// only need 1
			}
		}
	}
	
	// ok: show it
	if (can_see_anything) {
		send_to_char("\tw", ch);
		list_vehicles_to_char(ROOM_VEHICLES(IN_ROOM(ch)), ch, FALSE, NULL);
		send_to_char("\tg", ch);
		list_obj_to_char(ROOM_CONTENTS(IN_ROOM(ch)), ch, OBJ_DESC_LONG, FALSE);
		send_to_char("\t0", ch);
	}
	else {	// can see nothing
		msg_to_char(ch, "There are no contents here.\r\n");
	}
}


ACMD(do_cooldowns) {	
	struct cooldown_data *cool;
	int diff;
	bool found = FALSE;
	
	msg_to_char(ch, "Abilities on cooldown:\r\n");
	
	for (cool = ch->cooldowns; cool; cool = cool->next) {
		// only show if not expired (in case it wasn't cleaned up yet due to close timing)
		diff = cool->expire_time - time(0);
		if (diff >= 0) {
			msg_to_char(ch, " &c%s&0 %s\r\n", get_generic_name_by_vnum(cool->type), colon_time(diff, FALSE, NULL));
			found = TRUE;
		}
	}
	
	if (!found) {
		msg_to_char(ch, " none\r\n");
	}
}


ACMD(do_diagnose) {
	char_data *vict;

	one_argument(argument, buf);

	if (*buf) {
		if (!(vict = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM)))
			send_config_msg(ch, "no_person");
		else
			diag_char_to_char(vict, ch);
	}
	else {
		if (FIGHTING(ch))
			diag_char_to_char(FIGHTING(ch), ch);
		else
			send_to_char("Diagnose whom?\r\n", ch);
	}
}


ACMD(do_display) {
	skip_spaces(&argument);
	
	if (!*argument) {
		msg_to_char(ch, "Display what?\r\n");
	}
	else {
		delete_doubledollar(argument);
		msg_to_char(ch, "%s\r\n", replace_prompt_codes(ch, argument));
	}
}


ACMD(do_examine) {
	argument = one_argument(argument, arg);

	if (!*arg) {
		send_to_char("Examine what?\r\n", ch);
		return;
	}
	look_at_target(ch, arg, argument, TRUE);
}


ACMD(do_factions) {
	struct player_faction_data *pfd, *next_pfd;
	struct faction_relation *rel, *next_rel;
	char buf[MAX_STRING_LENGTH];
	faction_data *fct;
	int idx = NOTHING;
	int count = 0;
	size_t size;
	bool any;
	
	skip_spaces(&argument);
	
	if (*argument) {
		if (!(fct = find_faction_by_name(argument)) || FACTION_FLAGGED(fct, FCT_IN_DEVELOPMENT)) {
			msg_to_char(ch, "Unknown faction '%s'.\r\n", argument);
		}
		else if (!(pfd = get_reputation(ch, FCT_VNUM(fct), FALSE)) && !IS_IMMORTAL(ch)) {
			msg_to_char(ch, "You have not encountered that faction.\r\n");
		}
		else {
			idx = rep_const_to_index(pfd ? pfd->rep : FCT_STARTING_REP(fct));
			
			msg_to_char(ch, "%s%s\t0\r\n", (idx != NOTHING ? reputation_levels[idx].color : ""), FCT_NAME(fct));
			if (pfd && idx != NOTHING) {
				msg_to_char(ch, "Reputation: %s / %d\r\n", reputation_levels[idx].name, pfd->value);
			}
			else if (idx != NOTHING) {
				msg_to_char(ch, "Reputation: %s\r\n", reputation_levels[idx].name);
			}
			else {
				msg_to_char(ch, "Reputation: none\r\n");
			}
			msg_to_char(ch, "%s", NULLSAFE(FCT_DESCRIPTION(fct)));
			
			// relations?
			any = FALSE;
			HASH_ITER(hh, FCT_RELATIONS(fct), rel, next_rel) {
				if (IS_SET(rel->flags, FCTR_UNLISTED)) {
					continue;
				}
				
				// show it
				if (!any) {	// header
					any = TRUE;
					msg_to_char(ch, "Relationships:\r\n");
				}
				pfd = get_reputation(ch, rel->vnum, FALSE);
				idx = (pfd ? rep_const_to_index(pfd->rep) : NOTHING);
				prettier_sprintbit(rel->flags, relationship_descs, buf);
				msg_to_char(ch, " %s%s\t0 - %s\r\n", (idx != NOTHING ? reputation_levels[idx].color : ""), FCT_NAME(rel->ptr), buf);
			}
		}
	}
	else {	// no arg, show all
		size = snprintf(buf, sizeof(buf), "Your factions:\r\n");
		HASH_ITER(hh, GET_FACTIONS(ch), pfd, next_pfd) {
			if (size + 10 >= sizeof(buf)) {
				break;	// out of room
			}
			if (!(fct = find_faction_by_vnum(pfd->vnum))) {
				continue;
			}
			if (FACTION_FLAGGED(fct, FCT_HIDE_IN_LIST) && !IS_IMMORTAL(ch)) {
				continue;
			}
			
			++count;
			idx = rep_const_to_index(pfd->rep);
			size += snprintf(buf + size, sizeof(buf) - size, " %s %s(%s / %d)\t0%s\r\n", FCT_NAME(fct), reputation_levels[idx].color, reputation_levels[idx].name, pfd->value, (FACTION_FLAGGED(fct, FCT_HIDE_IN_LIST) ? " (hidden)" : ""));
		}
		
		if (!count) {
			strcat(buf, " none\r\n");
		}
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}
}


/* Generic page_string function for displaying text */
ACMD(do_gen_ps) {
	switch (subcmd) {
		case SCMD_CLEAR: {
			send_to_char("\033[H\033[J", ch);
			break;
		}
		case SCMD_VERSION: {
			msg_to_char(ch, "%s\r\n", version);
			msg_to_char(ch, "%s\r\n", DG_SCRIPT_VERSION);
			break;
		}
		default: {
			log("SYSERR: Unhandled case in do_gen_ps. (%d)", subcmd);
			return;
		}
	}
}


ACMD(do_gen_text_string) {
	if (subcmd < 0 || subcmd >= NUM_TEXT_FILE_STRINGS) {
		send_config_msg(ch, "huh_string");
	}
	else if (!text_file_strings[subcmd] || !*text_file_strings[subcmd]) {
		msg_to_char(ch, "This text is blank.\r\n");
	}
	else {
		page_string(ch->desc, text_file_strings[subcmd], FALSE);
	}
}


ACMD(do_help) {
	struct help_index_element *found;
	int level = GET_ACCESS_LEVEL(ch);

	if (!ch->desc)
		return;

	skip_spaces(&argument);
	
	// optional -m arg
	if (!strn_cmp(argument, "-m ", 3)) {
		level = LVL_MORTAL;
		argument += 3;
		skip_spaces(&argument);
	}
	
	// no-arg: basic help screen
	if (!*argument) {
		if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
			send_to_char(text_file_strings[TEXT_FILE_HELP_SCREEN_SCREENREADER], ch);
		}
		else {
			page_string(ch->desc, text_file_strings[TEXT_FILE_HELP_SCREEN], FALSE);
		}
		return;
	}
	
	// with arg: look up by keyword
	if (help_table) {
		found = find_help_entry(level, argument);
		
		if (found) {
			page_string(ch->desc, found->entry, FALSE);
		}
		else {
			send_to_char("There is no help on that word.\r\n", ch);
		}
	}
	else {
		send_to_char("No help available.\r\n", ch);
	}
}


ACMD(do_helpsearch) {	
	char output[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	char **words = NULL;
	int iter, wrd;
	bool found, fail;
	size_t size, word_size;
	
	if (!ch->desc) {
		// don't bother
		return;
	}
	
	delete_doubledollar(argument);
	word_size = 0;
	
	// put all the arguments into an array
	while (*argument) {
		argument = one_word(argument, arg);	// this removes leading filler words, which are going to show up in a lot of helps
		
		if (word_size > 0) {
			RECREATE(words, char*, word_size + 1);
		}
		else {
			CREATE(words, char*, word_size + 1);
		}
		
		words[word_size++] = str_dup(arg);
	}
	
	if (!word_size) {
		msg_to_char(ch, "Search for help about what?\r\n");
	}
	else if (!help_table) {
		msg_to_char(ch, "No help available.r\n");
	}
	else {
		size = snprintf(output, sizeof(output), "You find help on that in the following help entries:\r\n");
		found = FALSE;
		
		for (iter = 0; iter <= top_of_helpt; ++iter) {
			if (GET_ACCESS_LEVEL(ch) < help_table[iter].level) {
				continue;
			}
			if (help_table[iter].duplicate) {
				continue;
			}
			
			// see if it matches all words
			fail = FALSE;
			for (wrd = 0; wrd < word_size; ++wrd) {
				if (!str_str(help_table[iter].entry, words[wrd])) {
					fail = TRUE;
					break;
				}
			}
			if (fail) {
				continue;
			}
			
			// FOUND!
			found = TRUE;
			snprintf(line, sizeof(line), " %s\r\n", help_table[iter].keyword);
			
			if (size + strlen(line) < sizeof(output)) {
				strcat(output, line);
				size += strlen(line);
			}
			else {
				size += snprintf(output + size, sizeof(output) - size, "... and more\r\n");
				break;
			}
		}
		
		if (!found) {
			msg_to_char(ch, "%s none\r\n", output);
		}
		else {
			// send it out
			page_string(ch->desc, output, TRUE);
		}
	}
	
	// free data
	for (wrd = 0; wrd < word_size; ++wrd) {
		if (words[wrd]) {
			free(words[wrd]);
		}
	}
	if (words) {
		free(words);
	}
}


ACMD(do_informative) {
	int iter;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't change your informative settings right now.\r\n");
	}
	else if (!str_cmp(argument, "all")) {
		// turn on all informatives
		for (iter = 0; *informative_view_bits[iter] != '\n'; ++iter) {
			GET_INFORMATIVE_FLAGS(ch) |= BIT(iter);
		}
		msg_to_char(ch, "Your informative view now shows all types.\r\n");
	}
	else {
		GET_INFORMATIVE_FLAGS(ch) = olc_process_flag(ch, argument, "informative", "informative", informative_view_bits, GET_INFORMATIVE_FLAGS(ch));
	}
}


ACMD(do_inventory) {
	skip_spaces(&argument);
	
	if (!*argument) {	// no-arg: traditional inventory
		if (!IS_NPC(ch)) {
			do_coins(ch, "", 0, FALSE);
		}

		msg_to_char(ch, "You are carrying %d/%d items:\r\n", IS_CARRYING_N(ch), CAN_CARRY_N(ch));
		list_obj_to_char(ch->carrying, ch, OBJ_DESC_INVENTORY, TRUE);

		if (GET_LOYALTY(ch)) {
			show_local_einv(ch, IN_ROOM(ch), FALSE);
		}
	}
	else {	// advanced inventory
		char word[MAX_INPUT_LENGTH], heading[MAX_STRING_LENGTH], buf[MAX_STRING_LENGTH], temp[MAX_INPUT_LENGTH];
		int wear_type = NOTHING, type_type = NOTHING;
		bool kept = FALSE, not_kept = FALSE, bound = FALSE, unbound = FALSE;
		generic_data *cmp = NULL;
		obj_data *obj;
		size_t size;
		int count, to_show = -1;
		
		*heading = '\0';
		
		// parse flag if any
		while (*argument == '-') {
			switch (*(argument+1)) {
				case 'c': {
					argument += 2;	// skip past -c
					skip_spaces(&argument);
					strcpy(word, argument);
					*argument = '\0';	// no further args
					
					if (!*word) {
						msg_to_char(ch, "Show what component?\r\n");
						return;
					}
					if (!(cmp = find_generic_component(word))) {
						msg_to_char(ch, "Unknown component type '%s'.\r\n", word);
						return;
					}
					
					snprintf(heading + strlen(heading), sizeof(heading) - strlen(heading), "%s'%s':", (*heading ? ", " : ""), GEN_NAME(cmp));
					break;
				}
				case 'w': {
					half_chop(argument+2, word, temp);
					strcpy(argument, temp);
					if (!*word) {
						msg_to_char(ch, "Show items worn on what part?\r\n");
						return;
					}
					if ((wear_type = search_block(word, wear_bits, FALSE)) == NOTHING) {
						msg_to_char(ch, "Unknown wear location '%s'.\r\n", word);
						return;
					}
					
					snprintf(heading + strlen(heading), sizeof(heading) - strlen(heading), "%sworn on %s:", (*heading ? ", " : ""), wear_bits[wear_type]);
					break;
				}
				case 't': {
					half_chop(argument+2, word, temp);
					strcpy(argument, temp);
					if (!*word) {
						msg_to_char(ch, "Show items of what type?\r\n");
						return;
					}
					if ((type_type = search_block(word, item_types, FALSE)) == NOTHING) {
						msg_to_char(ch, "Unknown item type '%s'.\r\n", word);
						return;
					}
					
					snprintf(heading + strlen(heading), sizeof(heading) - strlen(heading), "%s%s", (*heading ? ", " : ""), item_types[type_type]);
					break;
				}
				case 'k': {
					strcpy(word, argument+2);
					strcpy(argument, word);
					kept = TRUE;
					snprintf(heading + strlen(heading), sizeof(heading) - strlen(heading), "%skept", (*heading ? ", " : ""));
					break;
				}
				case 'n': {
					strcpy(word, argument+2);
					strcpy(argument, word);
					not_kept = TRUE;
					snprintf(heading + strlen(heading), sizeof(heading) - strlen(heading), "%snot kept", (*heading ? ", " : ""));
					break;
				}
				case 'b': {
					strcpy(word, argument+2);
					strcpy(argument, word);
					bound = TRUE;
					snprintf(heading + strlen(heading), sizeof(heading) - strlen(heading), "%sbound", (*heading ? ", " : ""));
					break;
				}
				case 'u': {
					strcpy(word, argument+2);
					strcpy(argument, word);
					unbound = TRUE;
					snprintf(heading + strlen(heading), sizeof(heading) - strlen(heading), "%sunbound BoE", (*heading ? ", " : ""));
					break;
				}
				case 'i': {
					strcpy(word, argument+2);
					strcpy(argument, word);
					msg_to_char(ch, "Note: inventory -i is no longer supported. Use 'toggle item-details' instead.\r\n");
					break;
				}
				default: {
					if (isdigit(*(argument+1))) {
						// only show this many
						to_show = atoi(argument+1);
						to_show = MAX(1, to_show);
						
						snprintf(heading + strlen(heading), sizeof(heading) - strlen(heading), "%sfirst %d", (*heading ? ", " : ""), to_show);
						
						// peel off the first arg
						argument = one_argument(argument, arg);
					}
					else {
						// remove arg
						argument = one_argument(argument, arg);
					}
					break;
				}
			}
			
			// skip spaces and repeat
			skip_spaces(&argument);
		}
		
		// if we get this far, it's okay
		skip_spaces(&argument);
		count = 0;
		
		// start the string
		if (*argument && *heading) {
			size = snprintf(buf, sizeof(buf), "%s items matching '%s':\r\n", CAP(heading), argument);
		}
		else if (*argument) {
			size = snprintf(buf, sizeof(buf), "Items matching '%s':\r\n", argument);
		}
		else if (*heading) {
			size = snprintf(buf, sizeof(buf), "%s items:\r\n", CAP(heading));
		}
		else {
			size = snprintf(buf, sizeof(buf), "Items:\r\n");
		}
		
		DL_FOREACH2(ch->carrying, obj, next_content) {
			// break out early
			if (size + 80 > sizeof(buf)) {
				size += snprintf(buf + size, sizeof(buf) - size, "... and more\r\n");
				break;
			}
			
			// qualify it
			if (*argument && !multi_isname(argument, GET_OBJ_KEYWORDS(obj))) {
				continue;	// not matching keywords
			}
			if (cmp && !is_component(obj, cmp)) {
				continue;	// not matching component type
			}
			if (wear_type != NOTHING && !CAN_WEAR(obj, BIT(wear_type))) {
				continue;	// not matching wear pos
			}
			if (type_type != NOTHING && GET_OBJ_TYPE(obj) != type_type) {
				continue;	// not matching obj type
			}
			if (kept && !OBJ_FLAGGED(obj, OBJ_KEEP)) {
				continue;	// not matching keep flag
			}
			if (not_kept && OBJ_FLAGGED(obj, OBJ_KEEP)) {
				continue;	// not matching no-keep flag
			}
			if (bound && !OBJ_BOUND_TO(obj)) {
				continue;	// isn't bound
			}
			if (unbound && (OBJ_BOUND_TO(obj) || !OBJ_FLAGGED(obj, OBJ_BIND_FLAGS))) {
				continue;	// not an unbound bindable
			}
			
			// looks okay
			size += snprintf(buf + size, sizeof(buf) - size, "%2d. %s", ++count, obj_desc_for_char(obj, ch, OBJ_DESC_INVENTORY));
			
			// shown enough?
			if (to_show > 0 && count >= to_show) {
				break;
			}
		}
		
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}
}


ACMD(do_look) {
	char arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	char *exdesc;
	room_data *map;
	int look_type;

	if (!ch->desc)
		return;

	if (GET_POS(ch) < POS_SLEEPING)
		send_to_char("You can't see anything but stars!\r\n", ch);
	else if (AFF_FLAGGED(ch, AFF_BLIND))
		send_to_char("You can't see a damned thing, you're blind!\r\n", ch);
	
	/* prior to b5.31, this block gave an abbreviated version of look_at_room, which isn't necessary
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE) && ROOM_IS_CLOSED(IN_ROOM(ch))) {
		send_to_char("It is pitch black...\r\n", ch);
		list_char_to_char(ROOM_PEOPLE(IN_ROOM(ch)), ch);	// glowing red eyes
	}
	*/
	
	else {
		half_chop(argument, arg, arg2);

		if (!*arg) {			/* "look" alone, without an argument at all */
			clear_recent_moves(ch);
			look_at_room(ch);
		}
		else if (!str_cmp(arg, "out")) {
			if ((exdesc = find_exdesc_for_char(ch, arg, NULL, NULL, NULL, NULL))) {
				// extra desc takes priority
				send_to_char(exdesc, ch);
			}
			else if (!IS_IMMORTAL(ch) && !CAN_LOOK_OUT(IN_ROOM(ch))) {
				msg_to_char(ch, "You can't do that from here.\r\n");
			}
			else if (GET_ROOM_VEHICLE(IN_ROOM(ch))) {
				// look out from vehicle
				clear_recent_moves(ch);
				look_at_room_by_loc(ch, IN_ROOM(GET_ROOM_VEHICLE(IN_ROOM(ch))), LRR_LOOK_OUT_INSIDE);
			}
			else if (!(map = (GET_MAP_LOC(IN_ROOM(ch)) ? real_room(GET_MAP_LOC(IN_ROOM(ch))->vnum) : NULL))) {
				msg_to_char(ch, "You can't do that from here.\r\n");
			}
			else if (map == IN_ROOM(ch) && !ROOM_IS_CLOSED(IN_ROOM(ch))) {
				clear_recent_moves(ch);
				look_at_room_by_loc(ch, map, LRR_LOOK_OUT);
			}
			else {
				clear_recent_moves(ch);
				look_at_room_by_loc(ch, map, LRR_LOOK_OUT);
			}
		}
		else if (is_abbrev(arg, "in"))
			look_in_obj(ch, arg2, NULL, NULL);
		/* did the char type 'look <direction>?' */
		else if ((look_type = parse_direction(ch, arg)) != NO_DIR)
			look_in_direction(ch, look_type);
		else if (is_abbrev(arg, "at")) {
			half_chop(arg2, arg, arg3);
			look_at_target(ch, arg, arg3, FALSE);
		}
		else {
			look_at_target(ch, arg, arg2, FALSE);
		}
	}
}


ACMD(do_map) {
	int dist, mapsize;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't use this command.\r\n");
		return;
	}
	if (ROOM_IS_CLOSED(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only look at the map outdoors. (Try 'look out' instead.)\r\n");
		return;
	}
	
	dist = GET_MAPSIZE(ch);
	
	// check args
	skip_spaces(&argument);
	if (*argument) {
		if (isdigit(*argument)) {
			dist = atoi(argument);
		}
		else {
			msg_to_char(ch, "Usage: map [view distance]\r\n");
			return;
		}
	}
	
	// validate size
	if (*argument && dist > config_get_int("max_map_size")) {
		msg_to_char(ch, "The maximum view distance is %d.\r\n", config_get_int("max_map_size"));
		return;
	}
	else if (*argument && dist < 1) {
		msg_to_char(ch, "You must specify a distance of at least 1.\r\n");
		return;
	}
	
	mapsize = GET_MAPSIZE(ch);	// store for later
	GET_MAPSIZE(ch) = dist;	// requested distance
	
	clear_recent_moves(ch);	// prevents shrinkage
	look_at_room_by_loc(ch, IN_ROOM(ch), LRR_LOOK_OUT);
	
	GET_MAPSIZE(ch) = mapsize;
}


ACMD(do_mapsize) {
	#define MAPSIZE_RADIUS  1
	#define MAPSIZE_WIDTH  2
	
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH];
	int max_size, size, pos, mode;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	// check no-arg first
	skip_spaces(&argument);
	if (!*argument) {
		if (GET_MAPSIZE(ch) > 0) {
			msg_to_char(ch, "Current map size: radius %d (width %d)\r\n", GET_MAPSIZE(ch), GET_MAPSIZE(ch) * 2 + 1);
		}
		else {
			msg_to_char(ch, "Your map size is set to automatic.\r\n");
		}
		return;
	}
	
	// usage: mapsize <distance> [radius|width]
	// screenreader players default to radius but sighted players default to width
	
	half_chop(argument, num_arg, type_arg);
	if (*num_arg && !*type_arg && str_cmp(num_arg, "auto")) {	// look for type_arg attached to num_arg
		for (pos = 0; pos < strlen(num_arg); ++pos) {
			if (isdigit(num_arg[pos])) {
				continue;	// still in the number portion
			}
			else if (isspace(num_arg[pos])) {
				num_arg[pos] = '\0';	// terminate at the first space
				continue;
			}
			else if (!isdigit(num_arg[pos])) {
				// found the start of the real second arg (first non-digit char)
				strcpy(type_arg, num_arg + pos);
				num_arg[pos] = '\0';
				break;	// done parsing
			}
		}
	}
	
	// verify type argument, if any
	if (*type_arg && is_abbrev(type_arg, "width")) {
		mode = MAPSIZE_WIDTH;
	}
	else if (*type_arg && is_abbrev(type_arg, "radius")) {
		mode = MAPSIZE_RADIUS;
	}
	else if (*type_arg) {
		msg_to_char(ch, "Invalid type. Usage: mapsize <distance> [radius | width]\r\n");
		return;
	}
	else {
		// default mode is based on screenreader status
		mode = PRF_FLAGGED(ch, PRF_SCREEN_READER) ? MAPSIZE_RADIUS : MAPSIZE_WIDTH;
	}
	
	// determine max size
	max_size = (mode == MAPSIZE_RADIUS ? config_get_int("max_map_size") : (config_get_int("max_map_size") * 2 + 1));
	
	// and process
	if (!str_cmp(num_arg, "auto")) {
		GET_MAPSIZE(ch) = 0;
		msg_to_char(ch, "Your map size is now automatic.\r\n");
	
	}
	else if ((size = atoi(num_arg)) < 1) {
		msg_to_char(ch, "Invalid map size. Usage: mapsize <distance> [radius | width]\r\n");
	}
	else if (size > max_size) {
		msg_to_char(ch, "You must choose a size between %d and %d.\r\n", (mode == MAPSIZE_RADIUS ? 1 : 3), max_size);
	}
	else {
		GET_MAPSIZE(ch) = (mode == MAPSIZE_RADIUS ? size : MAX(1, size/2));
		msg_to_char(ch, "Your map size is now: radius %d (width %d)\r\n", GET_MAPSIZE(ch), GET_MAPSIZE(ch) * 2 + 1);
	}
}


ACMD(do_mark) {
	int dist;
	const char *dir_str;
	room_data *mark, *here;
	
	skip_spaces(&argument);
	here = get_map_location_for(IN_ROOM(ch));
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't mark the map.\r\n");
	}
	else if (*argument && (!str_cmp(argument, "clear") || !str_cmp(argument, "rem") || !str_cmp(argument, "remove") || !str_cmp(argument, "unset"))) {
		GET_MARK_LOCATION(ch) = NOWHERE;
		msg_to_char(ch, "You clear your marked location.\r\n");
	}
	else if (GET_ROOM_VNUM(here) >= MAP_SIZE || RMT_FLAGGED(IN_ROOM(ch), RMT_NO_LOCATION)) {
		msg_to_char(ch, "You may only mark distances on the map.\r\n");
	}
	else {
		if (*argument && !str_cmp(argument, "set")) {
			GET_MARK_LOCATION(ch) = GET_ROOM_VNUM(here);
			msg_to_char(ch, "You have marked %s. Use 'mark' again to see the distance to it from any other map location.\r\n", (IN_ROOM(ch) == here ? "this location" : "this map location"));
		}
		else if (*argument) {
			msg_to_char(ch, "Usage: mark [set | clear]\r\n");
		}
		else {
			// report
			if (GET_MARK_LOCATION(ch) == NOWHERE || !(mark = real_room(GET_MARK_LOCATION(ch)))) {
				msg_to_char(ch, "You have not marked a location to measure from. Use 'mark set' to do so.\r\n");
			}
			else {
				dist = compute_distance(mark, IN_ROOM(ch));
				dir_str = get_partial_direction_to(ch, IN_ROOM(ch), mark, FALSE);
				
				if (HAS_NAVIGATION(ch)) {
					msg_to_char(ch, "Your mark at (%d, %d) is %d map tile%s %s.\r\n", X_COORD(mark), Y_COORD(mark), dist, (dist == 1 ? "" : "s"), (*dir_str ? dir_str : "away"));
				}
				else {
					msg_to_char(ch, "Your mark is %d map tile%s %s.\r\n", dist, (dist == 1 ? "" : "s"), (*dir_str ? dir_str : "away"));
				}
			}
		}
	}
}


ACMD(do_messages) {
	struct automessage *msg, *next_msg;
	char buf[MAX_STRING_LENGTH * 2];
	struct player_automessage *pam;
	time_t now = time(0);
	int id, count = 0;
	size_t size;
	
	if (IS_NPC(ch) || !ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
		return;
	}
	
	size = snprintf(buf, sizeof(buf), "Recent messages:\r\n");
	
	HASH_ITER(hh, automessages_table, msg, next_msg) {
		if (!msg->msg) {
			continue;	// somehow
		}
		
		// look up player message for time detection
		id = msg->id;
		HASH_FIND_INT(GET_AUTOMESSAGES(ch), &id, pam);
	
		if (msg->timing == AUTOMSG_ON_LOGIN || msg->timestamp > (now - (24 * SECS_PER_REAL_HOUR)) || (pam && pam->timestamp > (now - 24 * SECS_PER_REAL_HOUR))) {
			if (size + strlen(msg->msg) + 2 < sizeof(buf)) {
				++count;
				strcat(buf, msg->msg);
				strcat(buf, "\r\n");
				size += strlen(msg->msg) + 2;
			}
			else {
				size += snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
				break;
			}
			
			// mark seen
			if (msg->timing != AUTOMSG_ON_LOGIN) {
				if (!pam) {
					CREATE(pam, struct player_automessage, 1);
					pam->id = id;
					HASH_ADD_INT(GET_AUTOMESSAGES(ch), id, pam);
				}
				pam->timestamp = now;
			}
		}
	}
	
	if (!count) {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


ACMD(do_mudstats) {
	int iter, pos;
	
	const struct {
		char *name;
		void (*func)(char_data *ch, char *argument);
	} stat_list[] = {
		{ "configs", mudstats_configs },
		{ "empires", mudstats_empires },
		{ "time", mudstats_time },
		
		// last
		{ "\n", NULL }
	};
	
	argument = any_one_arg(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "See HELP MUDSTATS for more information.\r\n");
		return;
	}
	
	pos = NOTHING;
	for (iter = 0; *stat_list[iter].name != '\n'; ++iter) {
		if (is_abbrev(arg, stat_list[iter].name)) {
			pos = iter;
			break;
		}
	}
	
	if (pos == NOTHING) {
		msg_to_char(ch, "Invalid mudstats option.\r\n");
		return;
	}
	
	if (stat_list[iter].func != NULL) {
		// pass control
		(stat_list[iter].func)(ch, argument);
	}
	else {
		msg_to_char(ch, "That option is not implemented.\r\n");
	}
}


ACMD(do_nearby) {
	bool cities = TRUE, adventures = TRUE, starts = TRUE, check_arg = FALSE;
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH], adv_color[256], dist_buf[256], trait_buf[256];
	const char *dir_str;
	struct instance_data *inst;
	struct empire_city_data *city, *in_city;
	empire_data *emp, *next_emp;
	int iter, dist, size, max_dist;
	bool found = FALSE, newbie_only;
	room_data *loc;
	any_vnum vnum;
	struct nearby_item_t *nrb_list = NULL, *nrb_item, *next_item;
	
	bitvector_t show_city_traits = ETRAIT_DISTRUSTFUL;
	
	// for global nearby
	struct glb_nrb {
		any_vnum vnum;	// adventure vnum
		int dist;	// distance
		room_data *loc;	// location
		char *str; // part shown
		UT_hash_handle hh;
	} *hash = NULL, *glb, *next_glb;
	
	if (!ch->desc) {
		return;
	}
	
	if (NO_LOCATION(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't use nearby from here.\r\n");
		return;
	}
	
	// detect distance
	max_dist = room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_LARGER_NEARBY) ? 150 : 50;
	max_dist += GET_EXTRA_ATT(ch, ATT_NEARBY_RANGE);
	max_dist = MAX(0, max_dist);
	
	// newbie islands only show newbie island things
	newbie_only = (GET_ISLAND(IN_ROOM(ch)) && IS_SET(GET_ISLAND(IN_ROOM(ch))->flags, ISLE_NEWBIE)) ? TRUE : FALSE;
	
	// argument-parsing
	skip_spaces(&argument);
	while (*argument == '-') {
		++argument;
	}
	
	if ((is_abbrev(argument, "city") || is_abbrev(argument, "cities")) && strlen(argument) >= 3) {
		adventures = FALSE;
		starts = FALSE;
	}
	else if (is_abbrev(argument, "adventures") && strlen(argument) >= 3) {
		cities = FALSE;
		starts = FALSE;
	}
	else if ((is_abbrev(argument, "starting locations") || is_abbrev(argument, "starts")) && strlen(argument) >= 5) {
		cities = FALSE;
		adventures = FALSE;
	}
	else if (*argument) {
		check_arg = TRUE;
	}
	
	// displaying:
	size = snprintf(buf, sizeof(buf), "You find nearby (within %d tile%s):\r\n", max_dist, PLURAL(max_dist));
	#define NEARBY_DIR  get_partial_direction_to(ch, IN_ROOM(ch), loc, (PRF_FLAGGED(ch, PRF_SCREEN_READER) ? FALSE : TRUE))
			// was: (dir == NO_DIR ? "away" : (PRF_FLAGGED(ch, PRF_SCREEN_READER) ? dirs[dir] : alt_dirs[dir]))

	// check starting locations
	if (starts) {
		for (iter = 0; iter <= highest_start_loc_index; ++iter) {
			loc = real_room(start_locs[iter]);
			dist = compute_distance(IN_ROOM(ch), loc);
		
			if (dist <= max_dist && (!check_arg || multi_isname(argument, get_room_name(loc, FALSE)))) {
				found = TRUE;

				// dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), loc));
				dir_str = NEARBY_DIR;
				snprintf(dist_buf, sizeof(dist_buf), "%d %3s", dist, (dir_str && *dir_str) ? dir_str : "away");
				snprintf(line, sizeof(line), "%8s: %s%s\r\n", dist_buf, get_room_name(loc, FALSE), coord_display_room(ch, loc, FALSE));
				
				CREATE(nrb_item, struct nearby_item_t, 1);
				nrb_item->text = str_dup(line);
				nrb_item->distance = dist;
				DL_APPEND(nrb_list, nrb_item);
			}
		}
	}
	
	// check cities
	if (cities) {
		// will always try to show a city we're in
		if (ROOM_OWNER(IN_ROOM(ch)) && get_territory_type_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), FALSE, NULL, NULL) != TER_FRONTIER) {
			in_city = find_closest_city(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch));
	    }
	    else {
	    	in_city = NULL;
	    }
		
		HASH_ITER(hh, empire_table, emp, next_emp) {
			for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
				if (newbie_only && GET_ISLAND(city->location) && !IS_SET(GET_ISLAND(city->location)->flags, ISLE_NEWBIE)) {
					continue;	// skip non-newbie
				}
				
				loc = city->location;
				dist = compute_distance(IN_ROOM(ch), loc);

				if ((dist <= max_dist || city == in_city) && (!check_arg || multi_isname(argument, city->name))) {
					found = TRUE;
				
					// dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), loc));
					dir_str = NEARBY_DIR;
					snprintf(dist_buf, sizeof(dist_buf), "%d %3s", dist, (dir_str && *dir_str) ? dir_str : "away");
					
					if (city->traits & show_city_traits) {
						prettier_sprintbit(city->traits & show_city_traits, empire_trait_types, part);
						snprintf(trait_buf, sizeof(trait_buf), " (%s)", part);
					}
					else {
						*trait_buf = '\0';
					}
					
					snprintf(line, sizeof(line), "%8s: The %s of %s%s / %s%s&0%s\r\n", dist_buf, city_type[city->type].name, city->name, coord_display_room(ch, loc, FALSE), EMPIRE_BANNER(emp), EMPIRE_NAME(emp), trait_buf);
					
					CREATE(nrb_item, struct nearby_item_t, 1);
					nrb_item->text = str_dup(line);
					nrb_item->distance = dist;
					DL_APPEND(nrb_list, nrb_item);
				}
			}
		}
	}
	
	// check instances
	if (adventures) {
		DL_FOREACH(instance_list, inst) {
			glb = NULL;	// init this now -- used later
			
			if (!INST_FAKE_LOC(inst) || INSTANCE_FLAGGED(inst, INST_COMPLETED)) {
				continue;	// not a valid location or was completed
			}
		
			if (ADVENTURE_FLAGGED(INST_ADVENTURE(inst), ADV_NO_NEARBY)) {
				continue;	// does not show on nearby
			}
			if (newbie_only && GET_ISLAND(INST_FAKE_LOC(inst)) && !IS_SET(GET_ISLAND(INST_FAKE_LOC(inst))->flags, ISLE_NEWBIE)) {
				continue;	// only showing newbie now
			}
		
			loc = INST_FAKE_LOC(inst);
			if (!loc) {
				continue;	// no location
			}
			
			// distance check based on global-nearby flag
			dist = compute_distance(IN_ROOM(ch), loc);
			if (ADVENTURE_FLAGGED(INST_ADVENTURE(inst), ADV_GLOBAL_NEARBY)) {
				// check global
				vnum = GET_ADV_VNUM(INST_ADVENTURE(inst));
				HASH_FIND_INT(hash, &vnum, glb);
				if (!glb) {	// create entry
					CREATE(glb, struct glb_nrb, 1);
					glb->vnum = vnum;
					glb->dist = dist;
					glb->loc = loc;
					HASH_ADD_INT(hash, vnum, glb);
				}
				
				if (dist <= glb->dist) {
					// update entry
					glb->dist = dist;
					glb->loc = loc;
				}
				else {	// not closer
					continue;
				}
			}
			else if (dist > max_dist) {	// not global
				continue;	// too far
			}
			
			// check arg?
			if (check_arg && !multi_isname(argument, GET_ADV_NAME(INST_ADVENTURE(inst)))) {
				continue;
			}
			
			// owner part
			if (ROOM_OWNER(loc)) {
				snprintf(part, sizeof(part), " / %s%s&0", EMPIRE_BANNER(ROOM_OWNER(loc)), EMPIRE_NAME(ROOM_OWNER(loc)));
			}
			else {
				*part = '\0';
			}
		
			// show instance
			found = TRUE;
			// dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), loc));
			dir_str = NEARBY_DIR;
			strcpy(adv_color, color_by_difficulty((ch), pick_level_from_range((INST_LEVEL(inst) > 0 ? INST_LEVEL(inst) : get_approximate_level(ch)), GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)), GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)))));
			snprintf(dist_buf, sizeof(dist_buf), "%d %3s", dist, (dir_str && *dir_str) ? dir_str : "away");
			snprintf(line, sizeof(line), "%8s: %s%s\t0%s / %s%s\r\n", dist_buf, adv_color, GET_ADV_NAME(INST_ADVENTURE(inst)), coord_display_room(ch, loc, FALSE), instance_level_string(inst), part);
			
			if (glb) {	// just add it to the global list
				if (glb->str) {
					free(glb->str);
				}
				glb->str = str_dup(line);
			}
			else {
				CREATE(nrb_item, struct nearby_item_t, 1);
				nrb_item->text = str_dup(line);
				nrb_item->distance = dist;
				DL_APPEND(nrb_list, nrb_item);
			}
		}
	}
	
	// add globals to buf and free global data
	HASH_ITER(hh, hash, glb, next_glb) {
		HASH_DEL(hash, glb);
		if (glb->str) {
			CREATE(nrb_item, struct nearby_item_t, 1);
			nrb_item->text = glb->str;	// move instead of freeing
			nrb_item->distance = glb->dist;
			DL_APPEND(nrb_list, nrb_item);
		}
		free(glb);
	}
	
	DL_SORT(nrb_list, nrb_sort_distance);
	DL_FOREACH_SAFE(nrb_list, nrb_item, next_item) {
		if (nrb_item->text) {
			if (size + strlen(nrb_item->text) < sizeof(buf)) {
				strcat(buf, nrb_item->text);
				size += strlen(nrb_item->text);
			}
			free(nrb_item->text);
		}
		free(nrb_item);
	}
	
	if (!found) {
		size += snprintf(buf + size, sizeof(buf) - size, " nothing\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


ACMD(do_no_cmd) {
	switch (subcmd) {
		case NOCMD_CAST: {
			// this one is no longer used because we have the 'cast' command again as of b5.166
			msg_to_char(ch, "EmpireMUD doesn't use the 'cast' command. You use most abilities by typing their name.\r\n");
			break;
		}
		case NOCMD_GOSSIP: {
			msg_to_char(ch, "EmpireMUD doesn't have a gossip channel. Try the /ooc channel, or type /list to see which global channels you're on.\r\n");
			break;
		}
		case NOCMD_LEVELS: {
			msg_to_char(ch, "EmpireMUD uses skills and gear to determine your level, not experience points. See HELP LEVELS for more info.\r\n");
			break;
		}
		case NOCMD_PRACTICE: {
			msg_to_char(ch, "EmpireMUD doesn't use 'practices' for skill gain. Type 'skills' or check out HELP SKILLS for more info.\r\n");
			break;
		}
		case NOCMD_RENT: {
			msg_to_char(ch, "EmpireMUD doesn't require you to rent or save your character anywhere. You usually log back in right where you quit.\r\n");
			break;
		}
		case NOCMD_REPORT: {
			msg_to_char(ch, "EmpireMUD doesn't have a 'report' command.\r\n");
			break;
		}
		case NOCMD_UNGROUP: {
			msg_to_char(ch, "EmpireMUD doesn't have an 'ungroup' command. Use 'group leave' or 'group kick'.\r\n");
			break;
		}
		case NOCMD_WIMPY: {
			msg_to_char(ch, "EmpireMUD doesn't have a 'wimpy' command.\r\n");
			break;
		}
		case NOCMD_TOGGLE: {
			msg_to_char(ch, "EmpireMUD doesn't have that command by itself. Use 'toggle' instead.\r\n");
			break;
		}
		default: {
			send_config_msg(ch, "huh_string");
			break;
		}
	}
}


ACMD(do_passives) {
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
	bool found = FALSE, is_file = FALSE;
	struct affected_type *aff;
	char_data *vict = ch;
	
	one_argument(argument, arg);
	
	// only immortals can target this command
	if (IS_IMMORTAL(ch) && *arg) {
		if (!(vict = find_or_load_player(arg, &is_file))) {
			send_config_msg(ch, "no_person");
			return;
		}
		if (IS_NPC(vict)) {
			msg_to_char(ch, "You can't view passive buffs on an NPC.\r\n");
			if (is_file) {
				// unlikely to get here, but playing it safe
				free_char(vict);
			}
			return;
		}
	}
	else if (IS_NPC(vict)) {	// when viewing passives on yourself
		msg_to_char(ch, "You don't have passive buffs.\r\n");
	}
	
	// these aren't normally loaded when offline
	if (is_file) {
		check_delayed_load(vict);
		affect_total(vict);
	}
	
	// header
	if (ch == vict) {
		msg_to_char(ch, "Passive buffs:\r\n");
	}
	else {
		msg_to_char(ch, "Passive buffs for %s:\r\n", PERS(vict, ch, TRUE));
	}
	
	LL_SORT(GET_PASSIVE_BUFFS(vict), sort_passives);
	LL_FOREACH(GET_PASSIVE_BUFFS(vict), aff) {
		*buf2 = '\0';
		
		sprintf(buf, " &c%s&0 ", get_ability_name_by_vnum(aff->type));

		if (aff->modifier) {
			sprintf(buf2, "%+d to %s", aff->modifier, apply_types[(int) aff->location]);
			strcat(buf, buf2);
		}
		if (aff->bitvector) {
			if (*buf2) {
				strcat(buf, ", sets ");
			}
			else {
				strcat(buf, "sets ");
			}
			sprintbit(aff->bitvector, affected_bits, buf2, TRUE);
			strcat(buf, buf2);
		}
		send_to_char(strcat(buf, "\r\n"), ch);
		found = TRUE;
	}
	
	if (!found) {
		msg_to_char(ch, " none\r\n");
	}
	
	if (is_file) {
		free_char(vict);
	}
}


ACMD(do_score) {
	char_data *victim;

	if (REAL_NPC(ch))
		return;

	one_argument(argument, arg);

	if (IS_IMMORTAL(ch) && *arg) {
		if (!(victim = get_player_vis(ch, arg, FIND_CHAR_WORLD)))
			send_config_msg(ch, "no_person");
		else if (IS_NPC(victim))
			msg_to_char(ch, "You can't get a score sheet for an NPC.\r\n");
		else if (GET_REAL_LEVEL(victim) > GET_REAL_LEVEL(ch))
			msg_to_char(ch, "You can't get a score sheet for someone of a higher level!\r\n");
		else
			display_score_to_char(victim, ch);
		return;
	}

	display_score_to_char(ch, ch);
}


ACMD(do_survey) {
	char line[1024];
	char *temp;
	struct empire_city_data *city;
	struct empire_island *eisle;
	struct island_info *island;
	int max, prc, ter_type, base_height, mod_height;
	bool junk, large_radius;
	struct depletion_data *dep;
	
	msg_to_char(ch, "You survey the area:\r\n");
	
	if ((island = GET_ISLAND(IN_ROOM(ch)))) {
		// find out if it has a local name
		if (GET_LOYALTY(ch) && island->id != NO_ISLAND && (eisle = get_empire_island(GET_LOYALTY(ch), island->id)) && eisle->name && str_cmp(eisle->name, island->name)) {
			msg_to_char(ch, "Location: %s (%s)%s%s\r\n", get_island_name_for(island->id, ch), island->name, IS_SET(island->flags, ISLE_NEWBIE) ? " (newbie island)" : "", IS_SET(island->flags, ISLE_CONTINENT) ? " (continent)" : "");
		}
		else {
			msg_to_char(ch, "Location: %s%s%s\r\n", get_island_name_for(island->id, ch), IS_SET(island->flags, ISLE_NEWBIE) ? " (newbie island)" : "", IS_SET(island->flags, ISLE_CONTINENT) ? " (continent)" : "");
		}
	}
	
	if (get_climate(IN_ROOM(ch)) != NOBITS) {
		ordered_sprintbit(get_climate(IN_ROOM(ch)), climate_flags, climate_flags_order, FALSE, buf);
		msg_to_char(ch, "Climate: %s\r\n", buf);
	}
	msg_to_char(ch, "Temperature: %s\r\n", temperature_to_string(get_room_temperature(IN_ROOM(ch))));
	
	base_height = ROOM_HEIGHT(HOME_ROOM(IN_ROOM(ch)));
	mod_height = get_room_blocking_height(IN_ROOM(ch), NULL);
	if (base_height > 0 && mod_height > 0) {
		if (mod_height != base_height) {
			msg_to_char(ch, "Elevation: %d (with structures: %d)\r\n", base_height, mod_height);
		}
		else {
			msg_to_char(ch, "Elevation: %d\r\n", base_height);
		}
	}
	
	// empire
	if (ROOM_OWNER(IN_ROOM(ch))) {
		if ((ter_type = get_territory_type_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), FALSE, &junk, &large_radius)) == TER_CITY && (city = find_closest_city(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch)))) {
			msg_to_char(ch, "This is the %s%s&0 %s of %s%s.\r\n", EMPIRE_BANNER(ROOM_OWNER(IN_ROOM(ch))), EMPIRE_ADJECTIVE(ROOM_OWNER(IN_ROOM(ch))), city_type[city->type].name, city->name, large_radius ? " (extended radius)" : "");
		}
		else if (ter_type == TER_OUTSKIRTS) {
			msg_to_char(ch, "This is the outskirts of %s%s&0.\r\n", EMPIRE_BANNER(ROOM_OWNER(IN_ROOM(ch))), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))));
		}
		else {
			msg_to_char(ch, "This area is claimed by %s%s&0.\r\n", EMPIRE_BANNER(ROOM_OWNER(IN_ROOM(ch))), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))));
		}
	}
	else if (GET_LOYALTY(ch)) {
		ter_type = get_territory_type_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), FALSE, &junk, &large_radius);
		msg_to_char(ch, "This location would be %s for your empire%s.\r\n", (ter_type == TER_CITY ? "in a city" : (ter_type == TER_OUTSKIRTS ? "on the outskirts of a city" : "on the frontier")), large_radius ? " (extended radius)" : "");
	}
	
	// building info
	if (COMPLEX_DATA(IN_ROOM(ch))) {
		if (BUILDING_DAMAGE(IN_ROOM(ch)) > 0 || (IS_COMPLETE(IN_ROOM(ch)) && BUILDING_RESOURCES(IN_ROOM(ch)))) {
			msg_to_char(ch, "It's in need of maintenance and repair.\r\n");
		}
		if (IS_BURNING(IN_ROOM(ch))) {
			msg_to_char(ch, "It's on fire!\r\n");
		}
	}
	
	// depletion
	*buf = '\0';
	LL_FOREACH(ROOM_DEPLETION(IN_ROOM(ch)), dep) {
		if (dep->count > 0 && *depletion_strings[dep->type] && (max = get_depletion_max(IN_ROOM(ch), dep->type)) > 0) {
			strcpy(line, depletion_strings[dep->type]);
			prc = dep->count * 100 / max;
			prc = MIN(100, MAX(1, prc)) / 25;
			temp = str_replace("$$", depletion_levels[prc], line);
			
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s%s", *buf ? ", " : "", temp);
			free(temp);
		}
	}
	if (*buf) {
		// add an "and"
		if ((temp = strrchr(buf, ',')) && temp != strchr(buf, ',')) {
			msg_to_char(ch, "It looks like someone has %-*.*s and%s.\r\n", (int)(temp-buf+1), (int)(temp-buf+1), buf, temp + 1);
		}
		else if (temp) {
			msg_to_char(ch, "It looks like someone has %-*.*s and%s.\r\n", (int)(temp-buf), (int)(temp-buf), buf, temp + 1);
		}
		else {	// no comma
			msg_to_char(ch, "It looks like someone has %s.\r\n", buf);
		}
	}
	
	// adventure info
	if (find_instance_by_room(IN_ROOM(ch), FALSE, TRUE)) {
		do_adventure(ch, "", 0, 0);
	}
	
	// TO ADD:
	//	- room name
	//	- building info (name)
	
}


ACMD(do_temperature) {
	int ch_temp, room_temp, temp_limit;
	char imm_part[256], change_part[256], dire_part[256];
	
	room_temp = get_room_temperature(IN_ROOM(ch));
	
	// imm part for room
	if (IS_IMMORTAL(ch)) {
		snprintf(imm_part, sizeof(imm_part), " (%d)", room_temp);
	}
	else {
		*imm_part = '\0';
	}
	
	msg_to_char(ch, "It's %s %s%s.\r\n", temperature_to_string(room_temp), IS_OUTDOORS(ch) ? "out" : "in here", imm_part);
	
	ch_temp = get_relative_temperature(ch);
	temp_limit = config_get_int("temperature_discomfort");
	
	// imm part for character
	*imm_part = '\0';
	if (IS_IMMORTAL(ch)) {
		snprintf(imm_part, sizeof(imm_part), " (%d)", ch_temp);
	}
	
	// change part?
	*change_part = '\0';
	*dire_part = '\0';
	if (GET_TEMPERATURE(ch) != room_temp && get_temperature_type(IN_ROOM(ch)) != TEMPERATURE_ALWAYS_COMFORTABLE) {
		if (GET_TEMPERATURE(ch) > room_temp) {
			if (room_temp > 0) {
				snprintf(change_part, sizeof(change_part), " and cooling down");
			}
			else {
				snprintf(change_part, sizeof(change_part), " %s getting colder", (ch_temp > (-1 * temp_limit) ? "but" : "and"));
			}
		
			// dire part? (warning that it won't get better
			if (room_temp >= temp_limit) {
				snprintf(dire_part, sizeof(dire_part), ", but it's still too %s", (room_temp >= config_get_int("temperature_extreme") ? "hot" : "warm"));
			}
		}
		else {
			if (room_temp < 0) {
				snprintf(change_part, sizeof(change_part), " and warming up");
			}
			else if (GET_TEMPERATURE(ch) < temp_limit) {
				snprintf(change_part, sizeof(change_part), " %s getting warmer", (ch_temp > (-1 * temp_limit) ? "but" : "and"));
			}
			else {
				snprintf(change_part, sizeof(change_part), " and getting hotter");
			}
		
			// dire part? (warning that it won't get better
			if (room_temp <= -1 * temp_limit) {
				snprintf(dire_part, sizeof(dire_part), ", but it's still %stoo cold", (room_temp <= -1 * config_get_int("temperature_extreme") ? "way " : ""));
			}
		}
	}
	
	if (ch_temp < temp_limit && ch_temp > -1 * temp_limit) {
		msg_to_char(ch, "You're comfortable right now%s%s%s.\r\n", imm_part, change_part, dire_part);
	}
	else {
		msg_to_char(ch, "You are %s%s%s%s%s\r\n", temperature_to_string(ch_temp), imm_part, change_part, dire_part, (ABSOLUTE(ch_temp) >= temp_limit * 2) ? "!" : ".");
	}
}


ACMD(do_time) {
	char time_of_day[MAX_STRING_LENGTH];
	struct time_info_data tinfo;
	const char *suf;
	int weekday, day, latitude, y_coord, sun;
	
	tinfo = get_local_time(IN_ROOM(ch));
	y_coord = Y_COORD(IN_ROOM(ch));
	latitude = (y_coord != -1) ? Y_TO_LATITUDE(y_coord) : 0;
	
	if (HAS_CLOCK(ch)) {
		sprintf(buf, "It is %d o'clock %s", ((tinfo.hours % 12 == 0) ? 12 : ((tinfo.hours) % 12)), ((tinfo.hours >= 12) ? "pm" : "am"));
		
		if (has_player_tech(ch, PTECH_CALENDAR)) {
			strcat(buf, ", on ");
			
			/* 30 days in a month */
			weekday = ((30 * tinfo.month) + tinfo.day + 1) % 7;
			strcat(buf, weekdays[weekday]);
		}
		
		if (IS_IMMORTAL(ch)) {
			sprintf(buf + strlen(buf), " (%d%s global time)", ((main_time_info.hours % 12 == 0) ? 12 : ((main_time_info.hours) % 12)), ((main_time_info.hours >= 12) ? "pm" : "am"));
		}
		
		strcat(buf, ".\r\n");
		send_to_char(buf, ch);
	
		gain_player_tech_exp(ch, PTECH_CLOCK, 1);
	}
	
	// prepare the start of a string for time-of-day (used below)
	sun = get_sun_status(IN_ROOM(ch));
	if (sun == SUN_DARK) {
		snprintf(time_of_day, sizeof(time_of_day), "It's nighttime");
	}
	else if (sun == SUN_RISE) {
		snprintf(time_of_day, sizeof(time_of_day), "It's almost dawn");
	}
	else if (sun == SUN_SET) {
		snprintf(time_of_day, sizeof(time_of_day), "It's sunset");
	}
	else if (HAS_CLOCK(ch)) {
		snprintf(time_of_day, sizeof(time_of_day), "It's daytime");
	}
	// all other time options are only shown without clocks:
	else if (tinfo.hours == 12) {
		snprintf(time_of_day, sizeof(time_of_day), "It's midday");
	}
	else if (tinfo.hours < 12) {
		snprintf(time_of_day, sizeof(time_of_day), "It's %smorning", tinfo.hours <= 8 ? "early " : "");
	}
	else {	// afternoon is all that's left
		snprintf(time_of_day, sizeof(time_of_day), "It's %safternoon", tinfo.hours >= 17 ? "late " : "");
	}

	if (has_player_tech(ch, PTECH_CALENDAR)) {
		day = tinfo.day + 1;	/* day in [1..30] */

		/* 11, 12, and 13 are the infernal exceptions to the rule */
		if ((day % 10) == 1 && day != 11)
			suf = "st";
		else if ((day % 10) == 2 && day != 12)
			suf = "nd";
		else if ((day % 10) == 3 && day != 13)
			suf = "rd";
		else
			suf = "th";

		msg_to_char(ch, "%s on the %d%s day of the month of %s, year %d.\r\n", time_of_day, day, suf, month_name[(int) tinfo.month], tinfo.year);
		
		gain_player_tech_exp(ch, PTECH_CALENDAR, 1);
	}
	else {
		// no calendar -- just show time of day
		msg_to_char(ch, "%s.\r\n", time_of_day);
	}
	
	// check season
	if (IS_OUTDOORS(ch) && !NO_LOCATION(IN_ROOM(ch))) {
		msg_to_char(ch, "It's %s", seasons[GET_SEASON(IN_ROOM(ch))]);
		
		if (get_temperature_type(IN_ROOM(ch)) != TEMPERATURE_ALWAYS_COMFORTABLE) {
			msg_to_char(ch, " and %s out.\r\n", temperature_to_string(get_room_temperature(IN_ROOM(ch))));
		}
		else {
			send_to_char(".\r\n", ch);
		}
	}
	
	// check solstices
	if (DAY_OF_YEAR(tinfo) == FIRST_EQUINOX_DOY) {
		msg_to_char(ch, "Today is the %s equinox.\r\n", latitude >= 0 ? "vernal" : "autumnal");
	}
	else if (DAY_OF_YEAR(tinfo) == NORTHERN_SOLSTICE_DOY) {
		msg_to_char(ch, "Today is the %s solstice.\r\n", latitude >= 0 ? "summer" : "winter");
	}
	else if (DAY_OF_YEAR(tinfo) == LAST_EQUINOX_DOY) {
		msg_to_char(ch, "Today is the %s equinox.\r\n", latitude >= 0 ? "autumnal" : "vernal");
	}
	else if (DAY_OF_YEAR(tinfo) == SOUTHERN_SOLSTICE_DOY) {
		msg_to_char(ch, "Today is the %s solstice.\r\n", latitude >= 0 ? "winter" : "summer");
	}
	
	// check zenith
	if (is_zenith_day(IN_ROOM(ch))) {
		if (tinfo.hours < 12) {
			msg_to_char(ch, "Today will be the sun's zenith passage.\r\n");
		}
		else if (tinfo.hours > 12) {
			msg_to_char(ch, "Today was the sun's zenith passage.\r\n");
		}
		else {
			msg_to_char(ch, "Today is the sun's zenith passage.%s\r\n", IS_OUTDOORS(ch) ? " It's directly overhead!" : "");
		}
	}
}


ACMD(do_tip) {
	display_tip_to_char(ch);
}


ACMD(do_weather) {
	char *sky_text, *change_text;
	bool showed_temp = FALSE;
	
	// weather, if available
	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_WEATHER)) {
		msg_to_char(ch, "There's nothing interesting about the weather.\r\n");
	}
	else if (IS_OUTDOORS(ch)) {
		// these may be overridden below
		if (weather_info.change >= 0) {
			change_text = "and you feel a warm wind from the south";
		}
		else {
			change_text = "but your foot tells you bad weather is due";
		}
		
		// main weather messages
		switch (weather_info.sky) {
			case SKY_CLOUDLESS: {
				sky_text = "cloudless";
				break;
			}
			case SKY_CLOUDY: {
				sky_text = "cloudy";
				break;
			}
			case SKY_RAINING: {
				if (get_room_temperature(IN_ROOM(ch)) <= -1 * config_get_int("temperature_discomfort")) {
					sky_text = "snowy";
					if (weather_info.change >= 0) {
						change_text = "but better weather is due any day now";
					}
					else {
						change_text = "and the chill in your bones tells you it's getting worse";
					}
				}
				else {
					sky_text = "rainy";
				}
				break;
			}
			case SKY_LIGHTNING: {
				if (get_room_temperature(IN_ROOM(ch)) <= -1 * config_get_int("temperature_discomfort")) {
					sky_text = "white with snow";
					if (weather_info.change >= 0) {
						change_text = "but it seems to be clearing up";
					}
					else {
						change_text = "and it's getting worse";
					}
				}
				else {
					sky_text = "lit by flashes of lightning";
				}
				break;
			}
			default: {
				sky_text = "clear";
				// change-text already set
				break;
			}
		}
		
		msg_to_char(ch, "The sky is %s %s.\r\n", sky_text, change_text);
	}
	
	// show season unless in a no-location room
	if (!NO_LOCATION(IN_ROOM(ch))) {
		msg_to_char(ch, "It's %s", seasons[GET_SEASON(IN_ROOM(ch))]);
		
		if (get_temperature_type(IN_ROOM(ch)) != TEMPERATURE_ALWAYS_COMFORTABLE) {
			msg_to_char(ch, " and %s %s.\r\n", temperature_to_string(get_room_temperature(IN_ROOM(ch))), IS_OUTDOORS(ch) ? "out" : "in here");
			showed_temp = TRUE;
		}
		else {
			send_to_char(".\r\n", ch);
		}
	}
	
	// show sun/daytime
	if (IS_OUTDOORS(ch) || CAN_LOOK_OUT(IN_ROOM(ch))) {
		switch (get_sun_status(IN_ROOM(ch))) {
			case SUN_DARK: {
				msg_to_char(ch, "It's nighttime.\r\n");
				break;
			}
			case SUN_RISE: {
				msg_to_char(ch, "The sun is rising.\r\n");
				break;
			}
			case SUN_SET: {
				msg_to_char(ch, "The sun is setting.\r\n");
				break;
			}
			case SUN_LIGHT: {
				msg_to_char(ch, "The sun is up.\r\n");
				break;
			}
		}
	}
	else if (get_sun_status(IN_ROOM(ch)) == SUN_DARK) {
		msg_to_char(ch, "It's nighttime.\r\n");
	}
	else {
		msg_to_char(ch, "It's daytime.\r\n");
	}
	
	// show moons
	if (IS_OUTDOORS(ch) || CAN_LOOK_OUT(IN_ROOM(ch))) {
		show_visible_moons(ch);
	}
	
	// final message if not outdoors
	if (!IS_OUTDOORS(ch) && !CAN_LOOK_OUT(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't see the sky from here.\r\n");
	}
	
	// temperature
	if (!showed_temp && get_temperature_type(IN_ROOM(ch)) != TEMPERATURE_ALWAYS_COMFORTABLE) {
		msg_to_char(ch, "It's %s %s.\r\n", temperature_to_string(get_room_temperature(IN_ROOM(ch))), IS_OUTDOORS(ch) ? "out" : "in here");
		showed_temp = TRUE;
	}
}


ACMD(do_whereami) {
	struct map_data *map;
	adv_data *adv;
	
	msg_to_char(ch, "You are at: %s%s\r\n", get_room_name(IN_ROOM(ch), FALSE), coord_display_room(ch, IN_ROOM(ch), FALSE));
	
	// extra info:
	if (IS_IMMORTAL(ch)) {
		msg_to_char(ch, "VNum: [%d]\r\n", GET_ROOM_VNUM(IN_ROOM(ch)));
		// sector (basically the same as stat room)
		msg_to_char(ch, "Sector: [%d] %s", GET_SECT_VNUM(SECT(IN_ROOM(ch))), GET_SECT_NAME(SECT(IN_ROOM(ch))));
		msg_to_char(ch, ", Base: [%d] %s", GET_SECT_VNUM(BASE_SECT(IN_ROOM(ch))), GET_SECT_NAME(BASE_SECT(IN_ROOM(ch))));
		if (GET_ROOM_VNUM(IN_ROOM(ch)) < MAP_SIZE && (map = &world_map[X_COORD(IN_ROOM(ch))][Y_COORD(IN_ROOM(ch))])) {
			msg_to_char(ch, ", Natural: [%d] %s", GET_SECT_VNUM(map->natural_sector), GET_SECT_NAME(map->natural_sector));
		}
		msg_to_char(ch, "\r\n");
		
		if (ROOM_CROP(IN_ROOM(ch))) {
			msg_to_char(ch, "Crop: [%d] %s\r\n", GET_CROP_VNUM(ROOM_CROP(IN_ROOM(ch))), GET_CROP_NAME(ROOM_CROP(IN_ROOM(ch))));
		}
		if (GET_ROOM_TEMPLATE(IN_ROOM(ch))) {
			msg_to_char(ch, "Room template: [%d] %s\r\n", GET_RMT_VNUM(GET_ROOM_TEMPLATE(IN_ROOM(ch))), GET_RMT_TITLE(GET_ROOM_TEMPLATE(IN_ROOM(ch))));
			if ((adv = get_adventure_for_vnum(GET_RMT_VNUM(GET_ROOM_TEMPLATE(IN_ROOM(ch)))))) {
				msg_to_char(ch, "Adventure: [%d] %s\r\n", GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
			}
		}
		if (GET_BUILDING(IN_ROOM(ch))) {
			msg_to_char(ch, "Building: [%d] %s\r\n", GET_BLD_VNUM(GET_BUILDING(IN_ROOM(ch))), GET_BLD_NAME(GET_BUILDING(IN_ROOM(ch))));
		}
	}
}


ACMD(do_who) {
	char name_search[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH*2], empname[MAX_INPUT_LENGTH];
	char mode, *part, *ptr;
	int outsize = 0;
	int low = 0, high = 0;
	bool rp = FALSE;
	bool shortlist = FALSE;
	empire_data *show_emp = NULL;

	skip_spaces(&argument);
	strcpy(buf, argument);
	name_search[0] = '\0';

	while (*buf) {
		half_chop(buf, arg, buf1);
		if (isdigit(*arg)) {
			sscanf(arg, "%d-%d", &low, &high);
			strcpy(buf, buf1);
		}
		else if (*arg == '-') {
			mode = *(arg + 1);       /* just in case; we destroy arg in the switch */
			switch (mode) {
				case 'r':
					rp = TRUE;
					strcpy(buf, buf1);
					break;
				case 'l':
					half_chop(buf1, arg, buf);
					sscanf(arg, "%d-%d", &low, &high);
					break;
				case 'n':
					half_chop(buf1, name_search, buf);
					break;
				case 'e':
					ptr = any_one_word(buf1, empname);
					
					// was this actually an empire name or just a "who -e"?
					if (!*empname || *empname == '-') {
						show_emp = GET_LOYALTY(ch);
						// just skip this arg
						strcpy(buf, buf1);
						break;
					}
					
					// otherwise assume it was an empire name
					skip_spaces(&ptr);
					strcpy(buf, ptr);
					
					show_emp = get_empire_by_name(empname);
					if (!show_emp) {
						msg_to_char(ch, "Unknown empire '%s'.\r\n", empname);
						return;
					}
					break;
				case 's':
					shortlist = TRUE;
					strcpy(buf, buf1);
					break;
				default: {
					send_to_char("format: who [minlev[-maxlev]] [-n name] [-s] [-e [empire]] [-r]\r\n", ch);
					return;
				}
			}				/* end of switch */
		}
		else {			/* endif */
			send_to_char("format: who [minlev[-maxlev]] [-n name] [-s] [-e [empire]] [-r]\r\n", ch);
			return;
		}
	}
	
	*output = '\0';

	/* Immortals first */
	part = partial_who(ch, name_search, low, high, show_emp, rp, shortlist, WHO_IMMORTALS);
	if (*part) {
		outsize += snprintf(output + outsize, sizeof(output) - outsize, "%s%s", (*output ? "\r\n" : ""), part);
	}

	/* Gods second */
	part = partial_who(ch, name_search, low, high, show_emp, rp, shortlist, WHO_GODS);
	if (*part) {
		outsize += snprintf(output + outsize, sizeof(output) - outsize, "%s%s", (*output ? "\r\n" : ""), part);
	}

	/* Then mortals */
	part = partial_who(ch, name_search, low, high, show_emp, rp, shortlist, WHO_MORTALS);
	if (*part) {
		outsize += snprintf(output + outsize, sizeof(output) - outsize, "%s%s", (*output ? "\r\n" : ""), part);
	}

	if (*output) {
		page_string(ch->desc, output, TRUE);
	}
	else {
		/* Didn't find a match to set parameters */
		msg_to_char(ch, "You don't see anyone like that.\r\n");
	}
}


ACMD(do_whoami) {
	char real[256], curr[256];
	
	strcpy(curr, PERS(ch, ch, FALSE));
	strcpy(real, PERS(ch, ch, TRUE));
	
	if (strcmp(curr, real)) {	// different
		msg_to_char(ch, "%s (%s)\r\n", curr, real);
	}
	else {
		msg_to_char(ch, "%s\r\n", real);
	}
}


ACMD(do_whois) {
	char part[MAX_STRING_LENGTH], friend_part[256];
	char_data *victim = NULL;
	bool file = FALSE;
	int level, diff, math;
	struct friend_data *friend;

	skip_spaces(&argument);

	if (!*argument) {
		send_to_char("Who is whom?\r\n", ch);
		return;
	}
	if (!(victim = find_or_load_player(argument, &file))) {
		send_to_char("There is no such player.\r\n", ch);
		return;
	}
	
	// load remaining data
	check_delayed_load(victim);
	
	// friend portion
	if (!PRF_FLAGGED(victim, PRF_NO_FRIENDS) && (friend = find_account_friend(GET_ACCOUNT(ch), GET_ACCOUNT_ID(victim))) && friend->status == FRIEND_FRIENDSHIP) {
		if (strcmp(friend->original_name, GET_NAME(victim))) {
			snprintf(friend_part, sizeof(friend_part), " (friends, alt of %s)", friend->original_name);
		}
		else {
			snprintf(friend_part, sizeof(friend_part), " (friends)");
		}
	}
	else {
		*friend_part = '\0';
	}
	
	// basic info
	msg_to_char(ch, "%s%s&0\r\n", PERS(victim, victim, TRUE), NULLSAFE(GET_TITLE(victim)));
	sprinttype(GET_REAL_SEX(victim), genders, part, sizeof(part), "UNDEFINED");
	msg_to_char(ch, "Status: %s %s%s\r\n", CAP(part), level_names[(int) GET_ACCESS_LEVEL(victim)][1], friend_part);

	// show class (but don't bother for immortals, as they generally have all skills
	if (!IS_GOD(victim) && !IS_IMMORTAL(victim)) {
		level = file ? GET_LAST_KNOWN_LEVEL(victim) : GET_COMPUTED_LEVEL(victim);
		
		get_player_skill_string(victim, part, FALSE);
		msg_to_char(ch, "Level: %d\r\n", level);
		msg_to_char(ch, "Skills: %s (%s%s\t0)\r\n", part, class_role_color[GET_CLASS_ROLE(victim)], class_role[GET_CLASS_ROLE(victim)]);
	}

	if (GET_LOYALTY(victim)) {
		msg_to_char(ch, "%s&0 of %s%s&0\r\n", EMPIRE_RANK(GET_LOYALTY(victim), GET_RANK(victim)-1), EMPIRE_BANNER(GET_LOYALTY(victim)), EMPIRE_NAME(GET_LOYALTY(victim)));
		msg_to_char(ch, "Territory: %d, Members: %d, Greatness: %d\r\n", EMPIRE_TERRITORY(GET_LOYALTY(victim), TER_TOTAL), EMPIRE_MEMBERS(GET_LOYALTY(victim)), EMPIRE_GREATNESS(GET_LOYALTY(victim)));
	}
	
	// last login info
	if (!IS_IMMORTAL(victim) && !IN_ROOM(victim)) {
		diff = time(0) - victim->prev_logon;
		
		if (diff > SECS_PER_REAL_YEAR) {
			math = diff / SECS_PER_REAL_YEAR;
			msg_to_char(ch, "Last online: over %d year%s ago\r\n", math, PLURAL(math));
		}
		else if (diff > (SECS_PER_REAL_DAY * 30.5)) {
			math = diff / (SECS_PER_REAL_DAY * 30.5);
			msg_to_char(ch, "Last online: over %d month%s ago\r\n", math, PLURAL(math));
		}
		else if (diff > SECS_PER_REAL_WEEK) {
			msg_to_char(ch, "Last online: more than a week ago\r\n");
		}
		else if (diff > SECS_PER_REAL_DAY) {
			msg_to_char(ch, "Last online: more than a day ago\r\n");
		}
		else {
			msg_to_char(ch, "Last online: today\r\n");
		}
	}

	if (GET_LORE(victim)) {
		list_lore_to_char(victim, ch);
	}

	// cleanup	
	if (victim && file) {
		free_char(victim);
	}
}
