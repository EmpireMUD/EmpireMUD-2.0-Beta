/* ************************************************************************
*   File: act.informative.c                               EmpireMUD 2.0b5 *
*  Usage: Player-level commands of an informative nature                  *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
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
#include "utils.h"
#include "skills.h"
#include "dg_scripts.h"
#include "vnums.h"

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

// extern variables
extern struct city_metadata_type city_type[];
extern const char *class_role[];
extern const char *class_role_color[];
extern const char *dirs[];
extern struct help_index_element *help_table;
extern const char *item_types[];
extern int top_of_helpt;
extern const char *month_name[];
extern struct faction_reputation_type reputation_levels[];
extern const char *wear_bits[];
extern const struct wear_data_type wear_data[NUM_WEARS];

// external functions
void clear_recent_moves(char_data *ch);
extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom, bool allow_fake_loc);
extern char *get_room_name(room_data *room, bool color);
extern char *list_harnessed_mobs(vehicle_data *veh);
void look_at_vehicle(vehicle_data *veh, char_data *ch);

// local protos
ACMD(do_affects);
void list_obj_to_char(obj_data *list, char_data *ch, int mode, int show);
void list_one_char(char_data *i, char_data *ch, int num);
void look_at_char(char_data *i, char_data *ch, bool show_eq);
void show_obj_to_char(obj_data *obj, char_data *ch, int mode);
void show_one_stored_item_to_char(char_data *ch, empire_data *emp, struct empire_storage_data *store, bool show_zero);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Shows a tip-of-the-day to a character.
*
* @param char_data *ch The person to show a tip to.
*/
void display_tip_to_char(char_data *ch) {
	extern char **tips_of_the_day;
	extern int tips_of_the_day_size;
	
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
* @return char* A pointer to the description to show, or NULL if no match.
*/
char *find_exdesc(char *word, struct extra_descr_data *list) {
	struct extra_descr_data *i;

	for (i = list; i; i = i->next)
		if (isname(word, i->keyword))
			return (i->description);

	return (NULL);
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
			if (plsk->level >= CLASS_SKILL_CAP) {
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
		
		LL_FOREACH(GET_EQ(ch, iter)->custom_msgs, ocm) {
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

/*
 * Given the argument "look at <target>", figure out what object or char
 * matches the target.  First, see if there is another char in the room
 * with the name.  Then check local objs for exdescs.
 *
 * Thanks to Angus Mezick <angus@EDGIL.CCMAIL.COMPUSERVE.COM> for the
 * suggested fix to this problem.
 *
 * @param char_data *ch The looker.
 * @param char *arg What the person typed to look at.
 */
void look_at_target(char_data *ch, char *arg) {
	int bits, found = FALSE, j, fnum, i = 0;
	char_data *found_char = NULL;
	obj_data *obj, *found_obj = NULL;
	vehicle_data *veh, *found_veh = NULL;
	char *desc;

	if (!ch->desc)
		return;

	if (!*arg) {
		send_to_char("Look at what?\r\n", ch);
		return;
		}

	bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP | FIND_CHAR_ROOM | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, &found_char, &found_obj, &found_veh);

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
		act("$n looks at $V.", TRUE, ch, NULL, found_veh, TO_ROOM);
		return;
	}

	/* Strip off "number." from 2.foo and friends. */
	if (!(fnum = get_number(&arg))) {
		send_to_char("Look at what?\r\n", ch);
		return;
	}

	/* Does the argument match an extra desc in the char's inventory? */
	for (obj = ch->carrying; obj && !found; obj = obj->next_content) {
		if (CAN_SEE_OBJ(ch, obj))
			if ((desc = find_exdesc(arg, obj->ex_description)) != NULL && ++i == fnum) {
				send_to_char(desc, ch);
				act("$n looks at $p.", TRUE, ch, obj, NULL, TO_ROOM);
				found = TRUE;
			}
	}

	/* Does the argument match an extra desc in the char's equipment? */
	for (j = 0; j < NUM_WEARS && !found; j++)
		if (GET_EQ(ch, j) && CAN_SEE_OBJ(ch, GET_EQ(ch, j)))
			if ((desc = find_exdesc(arg, GET_EQ(ch, j)->ex_description)) != NULL && ++i == fnum) {
				send_to_char(desc, ch);
				act("$n looks at $p.", TRUE, ch, GET_EQ(ch, j), NULL, TO_ROOM);
				found = TRUE;
			}

	/* Does the argument match an extra desc of an object in the room? */
	for (obj = ROOM_CONTENTS(IN_ROOM(ch)); obj && !found; obj = obj->next_content)
		if (CAN_SEE_OBJ(ch, obj))
			if ((desc = find_exdesc(arg, obj->ex_description)) != NULL && ++i == fnum) {
				send_to_char(desc, ch);
				act("$n looks at $p.", TRUE, ch, obj, NULL, TO_ROOM);
				found = TRUE;
			}

	// does it match an extra desc of a vehicle here?
	if (!found && ROOM_VEHICLES(IN_ROOM(ch))) {
		LL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
			if (CAN_SEE_VEHICLE(ch, veh) && (desc = find_exdesc(arg, VEH_EX_DESCS(veh))) != NULL && ++i == fnum) {
				send_to_char(desc, ch);
				act("$n looks at $V.", TRUE, ch, NULL, veh, TO_ROOM);
				found = TRUE;
				break;	// only 1
			}
		}
	}

	// does it match an extra desc of the room template?
	if (!found && GET_ROOM_TEMPLATE(IN_ROOM(ch))) {
		if ((desc = find_exdesc(arg, GET_RMT_EX_DESCS(GET_ROOM_TEMPLATE(IN_ROOM(ch))))) != NULL && ++i == fnum) {
			send_to_char(desc, ch);
			found = TRUE;
		}
	}

	// does it match an extra desc of the building?
	if (!found && GET_BUILDING(IN_ROOM(ch))) {
		if ((desc = find_exdesc(arg, GET_BLD_EX_DESCS(GET_BUILDING(IN_ROOM(ch))))) != NULL && ++i == fnum) {
			send_to_char(desc, ch);
			found = TRUE;
		}
	}
	
	/* If an object was found back in generic_find */
	if (bits) {
		if (!found) {
			show_obj_to_char(found_obj, ch, OBJ_DESC_LOOK_AT);	/* Show no-description */
			act("$n looks at $p.", TRUE, ch, found_obj, NULL, TO_ROOM);
		}
	}
	else if (!found) {
		send_to_char("You do not see that here.\r\n", ch);
	}
}


/**
* Processes doing a "look in".
*
* @param char_data *ch The player doing the look-in.
* @param char *arg The typed argument (usually obj name).
*/
void look_in_obj(char_data *ch, char *arg) {
	extern const char *fullness[];
	vehicle_data *veh = NULL;
	obj_data *obj = NULL;
	char_data *dummy = NULL;
	int amt, bits;

	if (!*arg)
		send_to_char("Look in what?\r\n", ch);
	else if (!(bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, &dummy, &obj, &veh))) {
		sprintf(buf, "There doesn't seem to be %s %s here.\r\n", AN(arg), arg);
		send_to_char(buf, ch);
	}
	else if (veh) {
		// vehicle section
		if (!VEH_FLAGGED(veh, VEH_CONTAINER)) {
			act("$V isn't a container.", FALSE, ch, NULL, veh, TO_CHAR);
		}
		else {
			sprintf(buf, "$V (%d/%d):", VEH_CARRYING_N(veh), VEH_CAPACITY(veh));
			act(buf, FALSE, ch, NULL, veh, TO_CHAR);
			list_obj_to_char(VEH_CONTAINS(veh), ch, OBJ_DESC_CONTENTS, TRUE);
		}
	}
	// the rest is objects:
	else if ((GET_OBJ_TYPE(obj) != ITEM_DRINKCON) && (GET_OBJ_TYPE(obj) != ITEM_CORPSE) && (GET_OBJ_TYPE(obj) != ITEM_CONTAINER))
		send_to_char("There's nothing inside that!\r\n", ch);
	else {
		if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER || GET_OBJ_TYPE(obj) == ITEM_CORPSE) {
			if (OBJVAL_FLAGGED(obj, CONT_CLOSED) && GET_OBJ_TYPE(obj) != ITEM_CORPSE)
				send_to_char("It is closed.\r\n", ch);
			else {
				send_to_char(fname(GET_OBJ_KEYWORDS(obj)), ch);
				switch (bits) {
					case FIND_OBJ_INV:
						send_to_char(" (carried): \r\n", ch);
						break;
					case FIND_OBJ_ROOM:
						send_to_char(" (here): \r\n", ch);
						break;
					case FIND_OBJ_EQUIP:
						send_to_char(" (used): \r\n", ch);
						break;
				}

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
					sprintf(buf, "It's %sfull of a %s liquid.\r\n", fullness[amt], get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_COLOR));
				}
				send_to_char(buf, ch);
			}
		}
		else {
			msg_to_char(ch, "You can't look in that!\r\n");
		}
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
	extern const char *health_levels[];
	
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
	extern struct attribute_data_type attributes[NUM_ATTRIBUTES];
	extern int attribute_display_order[NUM_ATTRIBUTES];
	
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
	void show_character_affects(char_data *ch, char_data *to);
	extern double get_combat_speed(char_data *ch, int pos);
	extern int get_block_rating(char_data *ch, bool can_gain_skill);
	extern int get_crafting_level(char_data *ch);
	extern int total_bonus_healing(char_data *ch);
	extern int get_dodge_modifier(char_data *ch, char_data *attacker, bool can_gain_skill);
	extern int get_to_hit(char_data *ch, char_data *victim, bool off_hand, bool can_gain_skill);
	extern int health_gain(char_data *ch, bool info_only);
	extern int move_gain(char_data *ch, bool info_only);
	extern int mana_gain(char_data *ch, bool info_only);
	extern int get_ability_points_available_for_char(char_data *ch, any_vnum skill);
	extern const char *bonus_bit_descriptions[];
	extern const struct material_data materials[NUM_MATERIALS];
	extern const int base_hit_chance;
	extern const double hit_per_dex;

	char lbuf[MAX_STRING_LENGTH], lbuf2[MAX_STRING_LENGTH], lbuf3[MAX_STRING_LENGTH];
	struct player_skill_data *skdata, *next_skill;
	int i, j, count, pts, cols, val;
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
	sprintf(lbuf, "&g%d&0/&g%d&0+&g%d&0/5s", GET_HEALTH(ch), GET_MAX_HEALTH(ch), health_gain(ch, TRUE));
	msg_to_char(to, "  Health: %-28.28s", lbuf);

	// row 1 col 2: move, 6 color codes = 12 invisible characters
	sprintf(lbuf, "&y%d&0/&y%d&0+&y%d&0/5s", GET_MOVE(ch), GET_MAX_MOVE(ch), move_gain(ch, TRUE));
	msg_to_char(to, " Move: %-30.30s", lbuf);
	
	// row 1 col 3: mana, 6 color codes = 12 invisible characters
	sprintf(lbuf, "&c%d&0/&c%d&0+&c%d&0/5s", GET_MANA(ch), GET_MAX_MANA(ch), mana_gain(ch, TRUE));
	msg_to_char(to, " Mana: %-30.30s\r\n", lbuf);
	
	// row 2 col 1: conditions
	*lbuf = '\0';
	if (IS_HUNGRY(ch)) {
		sprintf(lbuf + strlen(lbuf), "%s%s", (strlen(lbuf) > 0 ? ", " : ""), "&yhungry&0");
	}
	if (IS_THIRSTY(ch)) {
		sprintf(lbuf + strlen(lbuf), "%s%s", (strlen(lbuf) > 0 ? ", " : ""), "&cthirsty&0");
	}
	if (IS_DRUNK(ch)) {
		sprintf(lbuf + strlen(lbuf), "%s%s", (strlen(lbuf) > 0 ? ", " : ""), "&mdrunk&0");
	}
	if (IS_BLOOD_STARVED(ch)) {
		sprintf(lbuf + strlen(lbuf), "%s%s", (strlen(lbuf) > 0 ? ", " : ""), "&rstarving&0");
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
	
	sprintf(lbuf3, "Resist  [%d|%d]", GET_RESIST_PHYSICAL(ch), GET_RESIST_MAGICAL(ch));
	msg_to_char(to, "  %-28.28s %-28.28s %-28.28s\r\n", lbuf, lbuf2, lbuf3);
	
	// row 2
	sprintf(lbuf, "Physical  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_PHYSICAL(ch), 0), GET_BONUS_PHYSICAL(ch));
	sprintf(lbuf2, "Magical  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_MAGICAL(ch), 0), GET_BONUS_MAGICAL(ch));
	sprintf(lbuf3, "Healing  [%s%+d&0]", HAPPY_COLOR(total_bonus_healing(ch), 0), total_bonus_healing(ch));
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
		if (skdata->level > 0) {
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
	int c = 1;
	
	bool use_mob_stacking = config_get_bool("use_mob_stacking");
	#define MOB_CAN_STACK(ch)  (use_mob_stacking && !GET_LED_BY(ch) && GET_POS(ch) != POS_FIGHTING && !MOB_FLAGGED((ch), MOB_EMPIRE | MOB_TIED | MOB_MOUNTABLE | MOB_FAMILIAR))
	
	// no work
	if (!list || !ch || !ch->desc) {
		return;
	}

	for (i = list; i; i = i->next_in_room, c = 1) {
		if (ch != i) {
			if (IS_NPC(i) && MOB_CAN_STACK(i)) {
				for (j = list; j != i; j = j->next_in_room) {
					if (GET_MOB_VNUM(j) == GET_MOB_VNUM(i) && MOB_CAN_STACK(j) && CAN_SEE(ch, j) && GET_POS(j) == GET_POS(i)) {
						break;
					}
				}
			
				if (j != i) {
					continue;
				}
			
				for (j = i->next_in_room; j; j = j->next_in_room) {
					if (GET_MOB_VNUM(j) == GET_MOB_VNUM(i) && MOB_CAN_STACK(j) && CAN_SEE(ch, j) && GET_POS(j) == GET_POS(i)) {
						c++;
					}
				}
			}

			if (CAN_SEE(ch, i)) {
				list_one_char(i, ch, c);
			}
			else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch)) && HAS_INFRA(i) && !MAGIC_DARKNESS(IN_ROOM(ch))) {
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
		t = *mud_time_passed((time_t) lore->date, (time_t) beginning_of_time);

		strcpy(buf, month_name[(int) t.month]);
		if (!strn_cmp(buf, "the ", 4))
			strcpy(buf1, buf + 4);
		else
			strcpy(buf1, buf);

		snprintf(daystring, sizeof(daystring), "%d %s, Year %d", t.day + 1, buf1, t.year);
		msg_to_char(to, " %s on %s.\r\n", NULLSAFE(lore->text), daystring);
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
	extern bool can_get_quest_from_mob(char_data *ch, char_data *mob, struct quest_temp_list **build_list);
	extern bool can_turn_quest_in_to_mob(char_data *ch, char_data *mob, struct quest_temp_list **build_list);
	extern char *get_vehicle_short_desc(vehicle_data *veh, char_data *to);
	extern struct action_data_struct action_data[];
	
	struct custom_message *ocm;
	
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
				sprintf(buf, "$n is sitting %s %s%s%s.", IN_OR_ON(GET_SITTING_ON(i)), get_vehicle_short_desc(GET_SITTING_ON(i), ch), (VEH_ANIMALS(GET_SITTING_ON(i)) ? ", being pulled by " : ""), (VEH_ANIMALS(GET_SITTING_ON(i)) ? list_harnessed_mobs(GET_SITTING_ON(i)) : ""));
			}
			else if (!IS_NPC(i) && GET_ACTION(i) != ACT_NONE) {
				sprintf(buf, "$n %s", action_data[GET_ACTION(i)].long_desc);
			}
			else if (AFF_FLAGGED(i, AFF_DEATHSHROUD) && GET_POS(i) <= POS_RESTING) {
				sprintf(buf, "$n %s", positions[POS_DEAD]);
			}
			else if (GET_POS(i) == POS_STANDING && AFF_FLAGGED(i, AFF_FLY)) {
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
		
		if (AFF_FLAGGED(i, AFF_HIDE))
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
	if (affected_by_spell(i, ATYPE_FLY)) {
		act("...$e is flying with gossamer mana wings!", FALSE, i, 0, ch, TO_VICT);
	}
	if (IS_RIDING(i)) {
		sprintf(buf, "...$E is %s upon %s.", (MOUNT_FLAGGED(i, MOUNT_FLYING) ? "flying" : "mounted"), get_mob_name_by_proto(GET_MOUNT_VNUM(i)));
		act(buf, FALSE, ch, 0, i, TO_CHAR);
	}
	if (GET_LED_BY(i)) {
		sprintf(buf, "...%s is being led by %s.", HSSH(i), GET_LED_BY(i) == ch ? "you" : "$N");
		act(buf, FALSE, ch, 0, GET_LED_BY(i), TO_CHAR);
	}
	if (MOB_FLAGGED(i, MOB_TIED))
		act("...$e is tied up here.", FALSE, i, 0, ch, TO_VICT);
	if (AFF_FLAGGED(i, AFF_STUNNED | AFF_HARD_STUNNED)) {
		act("...$e is stunned!", FALSE, i, 0, ch, TO_VICT);
	}
	if (AFF_FLAGGED(i, AFF_HIDE)) {
		act("...$e has attempted to hide $mself.", FALSE, i, 0, ch, TO_VICT);
	}
	if (AFF_FLAGGED(i, AFF_BLIND))
		act("...$e is groping around blindly!", FALSE, i, 0, ch, TO_VICT);
	if (IS_INJURED(i, INJ_TIED))
		act("...$e is bound and gagged!", FALSE, i, 0, ch, TO_VICT);
	if (IS_INJURED(i, INJ_STAKED))
		act("...$e has a stake through $s heart!", FALSE, i, 0, ch, TO_VICT);
	if (AFF_FLAGGED(i, AFF_MUMMIFY))
		act("...$e is mummified in a hard outer covering!", FALSE, i, 0, ch, TO_VICT);
	if (AFF_FLAGGED(i, AFF_CLAWS))
		act("...$s fingers are mutated into giant, hideous claws!", FALSE, i, 0, ch, TO_VICT);
	if (AFF_FLAGGED(i, AFF_MAJESTY))
		act("...$e has a majestic aura about $m!", FALSE, i, 0, ch, TO_VICT);
	if (GET_FED_ON_BY(i)) {
		sprintf(buf, "...$e is held tightly by %s!", PERS(GET_FED_ON_BY(i), ch, 0));
		act(buf, FALSE, i, 0, ch, TO_VICT);
		}
	if (GET_FEEDING_FROM(i)) {
		sprintf(buf, "...$e has $s teeth firmly implanted in %s!", PERS(GET_FEEDING_FROM(i), ch, 0));
		act(buf, FALSE, i, 0, ch, TO_VICT);
		}
	if (!IS_NPC(i) && GET_ACTION(i) == ACT_MORPHING)
		act("...$e is undergoing a hideous transformation!", FALSE, i, 0, ch, TO_VICT);
	if ((IS_MORPHED(i) || IS_DISGUISED(i)) && (IS_IMMORTAL(ch) || CAN_RECOGNIZE(ch, i))) {
		act("...this appears to be $o.", FALSE, i, 0, ch, TO_VICT);
	}
	
	// these 
	if ((AFF_FLAGGED(i, AFF_NO_SEE_IN_ROOM) || (IS_IMMORTAL(i) && PRF_FLAGGED(i, PRF_WIZHIDE))) && PRF_FLAGGED(ch, PRF_HOLYLIGHT)) {
		if (AFF_FLAGGED(i, AFF_EARTHMELD)) {
			act("...$e is earthmelded.", FALSE, i, 0, ch, TO_VICT);
		}
		else {
			act("...$e is unable to be seen by players.", FALSE, i, 0, ch, TO_VICT);
		}
	}
}


/**
* Shows one vehicle as in-the-room.
*
* @param vehicle_data *veh The vehicle to show.
* @param char_data *ch The person to send the output to.
*/
void list_one_vehicle_to_char(vehicle_data *veh, char_data *ch) {
	extern bool can_get_quest_from_vehicle(char_data *ch, vehicle_data *veh, struct quest_temp_list **build_list);
	extern bool can_turn_quest_in_to_vehicle(char_data *ch, vehicle_data *veh, struct quest_temp_list **build_list);
	
	char buf[MAX_STRING_LENGTH];
	size_t size = 0;
	
	// pre-description
	if (VEH_OWNER(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "<%s> ", EMPIRE_ADJECTIVE(VEH_OWNER(veh)));
	}
	if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
		size += snprintf(buf + size, sizeof(buf) - size, "[%d] %s", VEH_VNUM(veh), HAS_TRIGGERS(veh) ? "[TRIG] " : "");
	}
	
	// main desc
	if (VEH_IS_COMPLETE(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "%s\r\n", VEH_LONG_DESC(veh));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, "%s is under construction.\r\n", VEH_SHORT_DESC(veh));
	}
	
	// additional descriptions like what's attached:
	if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		size += snprintf(buf + size, sizeof(buf) - size, "...it is ON FIRE!\r\n");
	}
	/* this is now indicated instead of the long desc
	if (!VEH_IS_COMPLETE(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "...it is unfinished.\r\n");
	}
	*/
	else if (VEH_NEEDS_RESOURCES(veh) || VEH_HEALTH(veh) < VEH_MAX_HEALTH(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "...it is in need of repair.\r\n");
	}
	
	if (VEH_SITTING_ON(veh) == ch) {
		size += snprintf(buf + size, sizeof(buf) - size, "...you are sitting %s it.\r\n", IN_OR_ON(veh));
	}
	else if (VEH_SITTING_ON(veh)) {
		// this is PROBABLY not shown to players
		size += snprintf(buf + size, sizeof(buf) - size, "...%s is sitting %s it.\r\n", PERS(VEH_SITTING_ON(veh), ch, FALSE), IN_OR_ON(veh));
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

	send_to_char(buf, ch);
}


/**
* Shows a list of vehicles in the room.
*
* @param vehicle_data *list Pointer to the start of the list of vehicles.
* @param vehicle_data *ch Person to send the output to.
*/
void list_vehicles_to_char(vehicle_data *list, char_data *ch) {
	vehicle_data *veh;
	
	// no work
	if (!list || !ch || !ch->desc) {
		return;
	}
	
	LL_FOREACH2(list, veh, next_in_room) {
		// conditions to show
		if (!CAN_SEE_VEHICLE(ch, veh)) {
			continue;	// should we show a "something" ?
		}
		if (VEH_SITTING_ON(veh) && VEH_SITTING_ON(veh) != ch) {
			continue;	// don't show vehicles someone else is sitting on
		}
		
		list_one_vehicle_to_char(veh, ch);
	}
}


/**
* Perform a look-at-person.
*
* @param char_data *i The person being looked at.
* @param char_data *ch The person to show the output to.
* @param bool show_eq If TRUE, can also show inventory (if skilled).
*/
void look_at_char(char_data *i, char_data *ch, bool show_eq) {
	extern struct character_size_data size_data[];
	
	char buf[MAX_STRING_LENGTH];
	bool disguise;
	int j, found;
	
	if (!i || !ch || !ch->desc)
		return;
	
	disguise = !IS_IMMORTAL(ch) && (IS_DISGUISED(i) || (IS_MORPHED(i) && CHAR_MORPH_FLAGGED(i, MORPHF_ANIMAL)));
	
	if (show_eq && ch != i && !IS_IMMORTAL(ch) && !IS_NPC(i) && has_ability(i, ABIL_CONCEALMENT)) {
		show_eq = FALSE;
		gain_ability_exp(i, ABIL_CONCEALMENT, 5);
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
	diag_char_to_char(i, ch);
	
	// membership info
	if (GET_LOYALTY(i) && !disguise) {
		sprintf(buf, "$E is a member of %s%s\t0.", EMPIRE_BANNER(GET_LOYALTY(i)), EMPIRE_NAME(GET_LOYALTY(i)));
		act(buf, FALSE, ch, NULL, i, TO_CHAR);
	}
	if (ROOM_OWNER(IN_ROOM(i)) && disguise) {
		sprintf(buf, "$E is a member of %s%s\t0.", EMPIRE_BANNER(ROOM_OWNER(IN_ROOM(i))), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(i))));
		act(buf, FALSE, ch, NULL, i, TO_CHAR);
	}
	if (IS_NPC(i) && MOB_FACTION(i)) {
		struct player_faction_data *pfd = get_reputation(ch, FCT_VNUM(MOB_FACTION(i)), FALSE);
		int idx = rep_const_to_index(pfd ? pfd->rep : NOTHING);
		sprintf(buf, "$E is a member of %s%s\t0.", (idx != NOTHING ? reputation_levels[idx].color : ""), FCT_NAME(MOB_FACTION(i)));
		act(buf, FALSE, ch, NULL, i, TO_CHAR);
	}
	
	if (size_data[GET_SIZE(i)].show_on_look) {
		act(size_data[GET_SIZE(i)].show_on_look, FALSE, ch, NULL, i, TO_CHAR);
	}
	
	// non-npc, non-disguised info (disguise 'counts' as NPC)
	if (!IS_NPC(i) && !disguise) {
		if (HAS_INFRA(i)) {
			act("You notice a distinct, red glint in $S eyes.", FALSE, ch, NULL, i, TO_CHAR);
		}
		if (AFF_FLAGGED(i, AFF_CLAWS)) {
			act("$N's hands are huge, distorted, and very sharp!", FALSE, ch, NULL, i, TO_CHAR);
		}
		if (AFF_FLAGGED(i, AFF_MAJESTY)) {
			act("$N has an aura of majesty about $M.", FALSE, ch, NULL, i, TO_CHAR);
		}
		if (AFF_FLAGGED(i, AFF_MUMMIFY)) {
			act("$E is mummified in a hard, dark substance!", FALSE, ch, NULL, i, TO_CHAR);
		}
	}
	
	if (IS_NPC(i) && !disguise && MOB_FLAGGED(i, MOB_MOUNTABLE) && has_player_tech(ch, PTECH_RIDING) && (!AFF_FLAGGED(i, AFF_FLY) || CAN_RIDE_FLYING_MOUNT(ch))) {
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
					send_to_char(wear_data[j].eq_prompt, ch);
					show_obj_to_char(GET_EQ(i, j), ch, OBJ_DESC_EQUIPMENT);
				}
			}
		}
	
		// show inventory
		if (ch != i && has_player_tech(ch, PTECH_SEE_INVENTORY)) {
			act("\r\nYou appraise $s inventory:", FALSE, i, 0, ch, TO_VICT);
			list_obj_to_char(i->carrying, ch, OBJ_DESC_INVENTORY, TRUE);

			if (ch != i && i->carrying) {
				if (can_gain_exp_from(ch, i)) {
					gain_player_tech_exp(ch, PTECH_SEE_INVENTORY, 5);
				}
				GET_WAIT_STATE(ch) = MAX(GET_WAIT_STATE(ch), 0.5 RL_SEC);
			}
		}
	}
}


/**
* Lists the affects on a character in a way that can be shown to players.
* 
* @param char_data *ch Whose effects
* @param char_data *to Who to send to
*/
void show_character_affects(char_data *ch, char_data *to) {
	extern const char *apply_types[];
	extern const char *affected_bits[];
	extern const char *damage_types[];

	struct over_time_effect_type *dot;
	struct affected_type *aff;
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], lbuf[MAX_INPUT_LENGTH];

	/* Routine to show what spells a char is affected by */
	for (aff = ch->affected; aff; aff = aff->next) {
		*buf2 = '\0';

		// duration setup
		if (aff->duration == UNLIMITED) {
			strcpy(lbuf, "infinite");
		}
		else {
			sprintf(lbuf, "%.1fmin", ((double)(aff->duration + 1) * SECS_PER_REAL_UPDATE / 60.0));
		}
		
		// main entry
		sprintf(buf, "   &c%s&0 (%s) ", get_generic_name_by_vnum(aff->type), lbuf);

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
		if (dot->duration == UNLIMITED) {
			strcpy(lbuf, "infinite");
		}
		else {
			snprintf(lbuf, sizeof(lbuf), "%.1fmin", ((double)(dot->duration + 1) * SECS_PER_REAL_UPDATE / 60.0));
		}
		
		// main body
		msg_to_char(to, "   &r%s&0 (%s) %d %s damage (%d/%d)\r\n", get_generic_name_by_vnum(dot->type), lbuf, dot->damage * dot->stack, damage_types[dot->damage_type], dot->stack, dot->max_stack);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT DISPLAY FUNCTIONS ////////////////////////////////////////////////

/**
* @param obj_data *obj
* @param char_data *ch person to show it to
* @param int mode OBJ_DESC_SHORT, OBJ_DESC_LONG
*/
char *get_obj_desc(obj_data *obj, char_data *ch, int mode) {
	extern const struct material_data materials[NUM_MATERIALS];

	static char output[MAX_STRING_LENGTH];
	char sdesc[MAX_STRING_LENGTH];

	*output = '\0';

	/* sdesc will be empty unless the short desc is modified */
	*sdesc = '\0';

	if (IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_CONTENTS(obj) > 0) {
		sprintf(sdesc, "%s of %s", GET_OBJ_SHORT_DESC(obj), get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_NAME));
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
				strcpy(output, sdesc);
			else
				strcpy(output, GET_OBJ_SHORT_DESC(obj));
			break;
		}
	}
	return output;
}


/**
* This is always called by do_inventory to show what's stored here. If nothing
* is stored, no message is sent.
*
* @param char_data *ch The person checking inventory.
* @param room_data *room The room they are checking.
* @param empire_data *emp An empire.
* @return bool TRUE if any items were shown at all, otherwise FALSE
*/
bool inventory_store_building(char_data *ch, room_data *room, empire_data *emp) {
	char buf[MAX_STRING_LENGTH];
	bool found = FALSE;
	struct empire_storage_data *store, *next_store;
	struct empire_island *eisle;
	obj_data *proto;

	/* Must be in an empire */
	if (!emp) {
		return found;
	}
	
	eisle = get_empire_island(emp, GET_ISLAND_ID(IN_ROOM(ch)));
	
	if (room_has_function_and_city_ok(IN_ROOM(ch), FNC_VAULT)) {
		msg_to_char(ch, "\r\nVault: %.1f coin%s, %d treasure (%d total)\r\n", EMPIRE_COINS(emp), (EMPIRE_COINS(emp) != 1.0 ? "s" : ""), EMPIRE_WEALTH(emp), (int) GET_TOTAL_WEALTH(emp));
	}
	
	HASH_ITER(hh, eisle->store, store, next_store) {
		if ((proto = store->proto)) {
			if (obj_can_be_retrieved(proto, room)) {
				if (!found) {
					snprintf(buf, sizeof(buf), "\r\n%s inventory available here:\r\n", EMPIRE_ADJECTIVE(emp));
					CAP(buf + 2);
					msg_to_char(ch, "%s", buf);
				}
				
				show_one_stored_item_to_char(ch, emp, store, FALSE);
				found = TRUE;
			}
		}
	}
	
	return found;
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

	for (i = list; i; i = i->next_content) {
		num = 0;
		
		if (OBJ_CAN_STACK(i)) {
			// look for a previous matching item
			
			for (j = list; j != i; j = j->next_content) {
				if (OBJ_CAN_STACK(j) && OBJS_ARE_SAME(i, j)) {
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
			for (j = i; j; j = j->next_content) {
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
			
			show_obj_to_char(i, ch, mode);
			found = TRUE;
		}
	}
	if (!found && show)
		msg_to_char(ch, " Nothing.\r\n");
}


/*
 * This function screams bitvector... -gg 6/45/98
 */
void show_obj_to_char(obj_data *obj, char_data *ch, int mode) {
	extern int Board_show_board(int board_type, char_data *ch, char *arg, obj_data *board);
	extern int board_loaded;
	extern bool can_get_quest_from_obj(char_data *ch, obj_data *obj, struct quest_temp_list **build_list);
	extern bool can_turn_quest_in_to_obj(char_data *ch, obj_data *obj, struct quest_temp_list **build_list);
	void init_boards(void);
	extern int find_board(char_data *ch);
	extern const char *extra_bits_inv_flags[];

	char flags[256];
	int board_type;
	room_data *room;

	// initialize buf as the obj desc
	if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
		sprintf(buf, "[%d] %s%s", GET_OBJ_VNUM(obj), (HAS_TRIGGERS(obj) ? "[TRIG] " : ""), GET_OBJ_DESC(obj, ch, mode));
	}
	else {
		strcpy(buf, GET_OBJ_DESC(obj, ch, mode));
	}

	if (mode == OBJ_DESC_EQUIPMENT) {
		if (obj->worn_by && AFF_FLAGGED(obj->worn_by, AFF_DISARM) && (obj->worn_on == WEAR_WIELD || obj->worn_on == WEAR_RANGED || obj->worn_on == WEAR_HOLD)) {
			strcat(buf, " (disarmed)");
		}
	}
	
	if (mode == OBJ_DESC_INVENTORY || mode == OBJ_DESC_EQUIPMENT || mode == OBJ_DESC_CONTENTS || mode == OBJ_DESC_LONG) {
		sprintbit(GET_OBJ_EXTRA(obj), extra_bits_inv_flags, flags, TRUE);
		if (strncmp(flags, "NOBITS", 6)) {
			sprintf(buf + strlen(buf), " %.*s", ((int)strlen(flags)-1), flags);	// remove trailing space
		}
		
		if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
			strcat(buf, " (quest)");
		}
		
		if (IS_STOLEN(obj)) {
			strcat(buf, " (STOLEN)");
		}
	}
	
	if (mode == OBJ_DESC_INVENTORY || (mode == OBJ_DESC_LONG && CAN_WEAR(obj, ITEM_WEAR_TAKE))) {
		if (can_get_quest_from_obj(ch, obj, NULL)) {
			strcat(buf, " (quest available)");
		}
		if (can_turn_quest_in_to_obj(ch, obj, NULL)) {
			strcat(buf, " (finished quest)");
		}
	}
	
	if (mode == OBJ_DESC_LOOK_AT) {
		if (GET_OBJ_TYPE(obj) == ITEM_BOARD) {
			if (!board_loaded) {
				init_boards();
				board_loaded = 1;
				}
			if ((board_type = find_board(ch)) != -1)
				if (Board_show_board(board_type, ch, "board", obj))
					return;
			strcpy(buf, "You see nothing special.");
		}
		else if (GET_OBJ_TYPE(obj) == ITEM_MAIL) {
			page_string(ch->desc, GET_OBJ_ACTION_DESC(obj) ? GET_OBJ_ACTION_DESC(obj) : "It's blank.\r\n", 1);
			return;
		}
		else if (IS_PORTAL(obj)) {
			room = real_room(GET_PORTAL_TARGET_VNUM(obj));
			if (room) {
				sprintf(buf, "%sYou peer into %s and see: %s\t0", NULLSAFE(GET_OBJ_ACTION_DESC(obj)), GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT), get_room_name(room, TRUE));
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
	
	// CRLF IS HERE
	if (buf[strlen(buf)-1] != '\n') {
		strcat(buf, "\r\n");
	}
	
	if (mode == OBJ_DESC_LONG && !CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
		if (can_get_quest_from_obj(ch, obj, NULL)) {
			strcat(buf, "...it has a quest for you!\r\n");
		}
		if (can_turn_quest_in_to_obj(ch, obj, NULL)) {
			strcat(buf, "...you can turn in a quest here!\r\n");
		}
	}
	
	page_string(ch->desc, buf, TRUE);
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
	int total = get_total_stored_count(emp, store->vnum, TRUE);
	char lbuf[MAX_INPUT_LENGTH], keepstr[256];
	
	if (total > store->amount || show_zero) {
		sprintf(lbuf, " (%d total)", total);
	}
	else {
		*lbuf = '\0';
	}
	
	if (store->keep == UNLIMITED) {
		strcpy(keepstr, " (keep)");
	}
	else if (store->keep > 0) {
		snprintf(keepstr, sizeof(keepstr), " (keep %d)", store->keep);
	}
	else {
		*keepstr = '\0';
	}
	
	msg_to_char(ch, "(%4d) %s%s%s\r\n", (show_zero ? 0 : store->amount), get_obj_name_by_proto(store->vnum), keepstr, lbuf);
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


// quick-switch of linked list positions
inline struct who_entry *switch_who_pos(struct who_entry *l1, struct who_entry *l2) {
    l1->next = l2->next;
    l2->next = l1;
    return l2;
}


/**
* Sorts a who list using a sort function.
*
* @param struct who_entry **node_list A pointer to the linked list to sort.
* @param WHO_SORTER(*compare_func) A sorter function.
*/
void sort_who_entries(struct who_entry **node_list, WHO_SORTER(*compare_func)) {
	struct who_entry *start, *p, *q, *top;
    bool changed = TRUE;
        
    // safety first
    if (!node_list || !compare_func) {
    	return;
    }
    
    start = *node_list;

	CREATE(top, struct who_entry, 1);

    top->next = start;
    if (start && start->next) {
    	// q is always one item behind p

        while (changed) {
            changed = FALSE;
            q = top;
            p = top->next;
            while (p->next != NULL) {
            	if ((compare_func)(p, p->next) > 0) {
					q->next = switch_who_pos(p, p->next);
					changed = TRUE;
				}
				
                q = p;
                if (p->next) {
                    p = p->next;
                }
            }
        }
    }
    
    *node_list = top->next;
    free(top);
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
	char buf[MAX_STRING_LENGTH], buf1[MAX_STRING_LENGTH], show_role[24], part[MAX_STRING_LENGTH];
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
			get_player_skill_string(ch, part, TRUE);
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
		num = color_code_length(out);
		sprintf(buf, "%%-%d.%ds", 35 + num, 35 + num);
		strcpy(buf1, out);
		
		size = snprintf(out, sizeof(out), buf, buf1);
		return out;
	}
	
	// title
	size += snprintf(out + size, sizeof(out) - size, "%s&0", NULLSAFE(GET_TITLE(ch)));
	
	// tags
	if (IS_AFK(ch)) {
		size += snprintf(out + size, sizeof(out) - size, " &r(AFK)&0");
	}
	if ((ch->char_specials.timer * SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN) >= 5) {
		size += snprintf(out + size, sizeof(out) - size, " (idle: %d)", (ch->char_specials.timer * SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN));
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
	extern int max_players_this_uptime;
	
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
	switch (config_get_int("who_list_sort")) {
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
		entry->next = list;
		list = entry;
	}
	
	sort_who_entries(&list, who_sorters[type]);

	for (entry = list; entry; entry = next_entry) {
		next_entry = entry->next;
		
		++count;
		size += snprintf(whobuf + size, sizeof(whobuf) - size, "%s", entry->string);
		
		// columnar spacing
		if (shortlist) {
			size += snprintf(whobuf + size, sizeof(whobuf) - size, "%s", !(count % 2) ? "\r\n" : " ");
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
		
		if (shortlist && (count % 2)) {
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
	void adventure_summon(char_data *ch, char *argument);

	char arg[MAX_STRING_LENGTH];
	struct instance_data *inst;
	
	if (*argument) {
		argument = any_one_arg(argument, arg);
		if (is_abbrev(arg, "summon")) {
			adventure_summon(ch, argument);
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
	int i;
	
	if (IS_NPC(ch))
		return;

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
		msg_to_char(ch, "   You are riding %s.\r\n", get_mob_name_by_proto(GET_MOUNT_VNUM(ch)));
	}
	else if (has_player_tech(ch, PTECH_RIDING) && GET_MOUNT_VNUM(ch) != NOTHING && mob_proto(GET_MOUNT_VNUM(ch))) {
		msg_to_char(ch, "   You have %s. Type 'mount' to ride it.\r\n", get_mob_name_by_proto(GET_MOUNT_VNUM(ch)));
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


ACMD(do_chart) {
	struct chart_territory *citer, *next_citer, *hash = NULL;
	struct empire_city_data *city;
	struct empire_island *e_isle;
	empire_data *emp, *next_emp;
	int iter, total_claims, num;
	struct island_info *isle;
	bool any, city_prompt;
	
	skip_spaces(&argument);
	
	if (!*argument) {
		msg_to_char(ch, "Get chart information on which island?\r\n");
	}
	else if (!(isle = get_island_by_name(ch, argument)) || isle->id == NO_ISLAND) {
		msg_to_char(ch, "Unknown island.\r\n");
	}
	else {
		msg_to_char(ch, "Chart information for %s:\r\n", isle->name);
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
			if (!(e_isle = get_empire_island(emp, isle->id))) {
				continue;
			}
			
			if (e_isle->territory[TER_TOTAL] > 0) {
				chart_add_territory(&hash, emp, e_isle->territory[TER_TOTAL]);
				total_claims += e_isle->territory[TER_TOTAL];
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
	}
}


// will show all currencies if the subcmd == TRUE
ACMD(do_coins) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], vstr[64];
	struct player_currency *cur, *next_cur;
	bool any = FALSE;
	size_t size = 0;
	
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
			
			if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
				sprintf(vstr, "[%5d] ", cur->vnum);
			}
			else {
				*vstr = '\0';
			}
			
			snprintf(line, sizeof(line), "%s%3d %s\r\n", vstr, cur->amount, get_generic_string_by_vnum(cur->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(cur->amount)));
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
		LL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_content) {
			if (CAN_SEE_OBJ(ch, obj)) {
				can_see_anything = TRUE;
				break;	// only need 1
			}
		}
	}
	// verify we can see even 1 vehicle
	if (!can_see_anything) {
		LL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
			if (CAN_SEE_VEHICLE(ch, veh)) {
				can_see_anything = TRUE;
				break;	// only need 1
			}
		}
	}
	
	// ok: show it
	if (can_see_anything) {
		send_to_char("&g", ch);
		list_obj_to_char(ROOM_CONTENTS(IN_ROOM(ch)), ch, OBJ_DESC_LONG, FALSE);
		send_to_char("&w", ch);
		list_vehicles_to_char(ROOM_VEHICLES(IN_ROOM(ch)), ch);
		send_to_char("&0", ch);
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
		if (diff > 0) {
			msg_to_char(ch, " &c%s&0 %d:%02d\r\n", get_generic_name_by_vnum(cool->type), (diff / 60), (diff % 60));

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
		if (!(vict = get_char_vis(ch, buf, FIND_CHAR_ROOM)))
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
	extern char *replace_prompt_codes(char_data *ch, char *str);
	
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
	vehicle_data *tmp_veh = NULL;
	char_data *tmp_char;
	obj_data *tmp_object;

	one_argument(argument, arg);

	if (!*arg) {
		send_to_char("Examine what?\r\n", ch);
		return;
	}
	look_at_target(ch, arg);

	generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_CHAR_ROOM | FIND_OBJ_EQUIP | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, &tmp_char, &tmp_object, &tmp_veh);

	if (tmp_object) {
		if ((GET_OBJ_TYPE(tmp_object) == ITEM_DRINKCON) || (GET_OBJ_TYPE(tmp_object) == ITEM_CONTAINER) || (GET_OBJ_TYPE(tmp_object) == ITEM_CORPSE)) {
			send_to_char("When you look inside, you see:\r\n", ch);
			look_in_obj(ch, arg);
		}
	}
	if (tmp_veh && VEH_FLAGGED(tmp_veh, VEH_CONTAINER)) {
		msg_to_char(ch, "When you look inside, you see:\r\n");
		look_in_obj(ch, arg);
	}
}


ACMD(do_factions) {
	extern const char *relationship_descs[];
	
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
		else {
			if ((pfd = get_reputation(ch, FCT_VNUM(fct), FALSE))) {
				idx = rep_const_to_index(pfd->rep);
			}
			
			msg_to_char(ch, "%s%s\t0\r\n", (idx != NOTHING ? reputation_levels[idx].color : ""), FCT_NAME(fct));
			if (pfd && idx != NOTHING) {
				msg_to_char(ch, "Reputation: %s / %d\r\n", reputation_levels[idx].name, pfd->value);
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
			
			++count;
			idx = rep_const_to_index(pfd->rep);
			size += snprintf(buf + size, sizeof(buf) - size, " %s %s(%s / %d)\t0\r\n", FCT_NAME(fct), reputation_levels[idx].color, reputation_levels[idx].name, pfd->value);
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
		case SCMD_CREDITS: {
			extern char *credits;
			page_string(ch->desc, credits, 0);
			break;
		}
		case SCMD_INFO: {
			extern char *info;
			page_string(ch->desc, info, 0);
			break;
		}
		case SCMD_WIZLIST: {
			extern char *wizlist;
			page_string(ch->desc, wizlist, 0);
			break;
		}
		case SCMD_GODLIST: {
			extern char *godlist;
			page_string(ch->desc, godlist, 0);
			break;
		}
		case SCMD_HANDBOOK: {
			extern char *handbook;
			page_string(ch->desc, handbook, 0);
			break;
		}
		case SCMD_POLICIES: {
			extern char *policies;
			page_string(ch->desc, policies, 0);
			break;
		}
		case SCMD_MOTD: {
			extern char *motd;
			page_string(ch->desc, motd, 0);
			break;
		}
		case SCMD_IMOTD: {
			extern char *imotd;
			page_string(ch->desc, imotd, 0);
			break;
		}
		case SCMD_NEWS: {
			extern char *news;
			page_string(ch->desc, news, 0);
			break;
		}
		case SCMD_CLEAR: {
			send_to_char("\033[H\033[J", ch);
			break;
		}
		case SCMD_VERSION: {
			extern const char *version;
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


ACMD(do_help) {
	extern struct help_index_element *find_help_entry(int level, const char *word);
	extern char *help;
	struct help_index_element *found;

	if (!ch->desc)
		return;

	skip_spaces(&argument);

	if (!*argument) {
		page_string(ch->desc, help, 0);
		return;
	}
	if (help_table) {
		found = find_help_entry(GET_ACCESS_LEVEL(ch), argument);
		
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


ACMD(do_inventory) {
	skip_spaces(&argument);
	
	if (!*argument) {	// no-arg: traditional inventory
		empire_data *ch_emp, *room_emp;
		
		if (!IS_NPC(ch)) {
			do_coins(ch, "", 0, FALSE);
		}

		msg_to_char(ch, "You are carrying %d/%d items:\r\n", IS_CARRYING_N(ch), CAN_CARRY_N(ch));
		list_obj_to_char(ch->carrying, ch, OBJ_DESC_INVENTORY, TRUE);

		if (IS_COMPLETE(IN_ROOM(ch)) && GET_LOYALTY(ch) && can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
			ch_emp = GET_LOYALTY(ch);
			room_emp = ROOM_OWNER(IN_ROOM(ch));
			
			if (!room_emp || ch_emp == room_emp || has_relationship(ch_emp, room_emp, DIPL_TRADE)) {
				inventory_store_building(ch, IN_ROOM(ch), ch_emp);
			}
		}
	}
	else {	// advanced inventory
		char word[MAX_INPUT_LENGTH], heading[MAX_STRING_LENGTH], buf[MAX_STRING_LENGTH];
		int cmp_type = NOTHING, wear_type = NOTHING, type_type = NOTHING;
		bool kept = FALSE, not_kept = FALSE, identify = FALSE;
		bitvector_t cmpf_type = NOBITS;
		obj_data *obj;
		size_t size;
		int count;
		
		*heading = '\0';
		
		// parse flag if any
		if (*argument == '-') {
			switch (*(argument+1)) {
				case 'c': {
					strcpy(word, argument+2);
					*argument = '\0';	// no further args
					if (!*word) {
						msg_to_char(ch, "Show what components?\r\n");
						return;
					}
					if (!parse_component(word, &cmp_type, &cmpf_type)) {
						msg_to_char(ch, "Unknown component type '%s'.\r\n", word);
						return;
					}
					
					snprintf(heading, sizeof(heading), "Items of type '%s':", component_string(cmp_type, cmpf_type));
					break;
				}
				case 'w': {
					half_chop(argument+2, word, argument);
					if (!*word) {
						msg_to_char(ch, "Show items worn on what part?\r\n");
						return;
					}
					if ((wear_type = search_block(word, wear_bits, FALSE)) == NOTHING) {
						msg_to_char(ch, "Unknown wear location '%s'.\r\n", word);
						return;
					}
					
					snprintf(heading, sizeof(heading), "Items worn on %s:", wear_bits[wear_type]);
					break;
				}
				case 't': {
					half_chop(argument+2, word, argument);
					if (!*word) {
						msg_to_char(ch, "Show items of what type?\r\n");
						return;
					}
					if ((type_type = search_block(word, item_types, FALSE)) == NOTHING) {
						msg_to_char(ch, "Unknown item type '%s'.\r\n", word);
						return;
					}
					
					snprintf(heading, sizeof(heading), "%s items:", item_types[type_type]);
					break;
				}
				case 'k': {
					strcpy(word, argument+2);
					strcpy(argument, word);
					kept = TRUE;
					snprintf(heading, sizeof(heading), "Items marked (keep):");
					break;
				}
				case 'n': {
					strcpy(word, argument+2);
					strcpy(argument, word);
					not_kept = TRUE;
					snprintf(heading, sizeof(heading), "Items not marked (keep):");
					break;
				}
				case 'i': {
					strcpy(word, argument+2);
					strcpy(argument, word);
					identify = TRUE;
					break;
				}
			}
		}
		
		// if we get this far, it's okay
		skip_spaces(&argument);
		if (!*heading && *argument) {
			snprintf(heading, sizeof(heading), "Items matching '%s':", argument);
		}
		else if (!*heading && !*argument) {
			snprintf(heading, sizeof(heading), "Items:");
		}
		
		// build string
		size = snprintf(buf, sizeof(buf), "%s\r\n", heading);
		count = 0;
		
		LL_FOREACH2(ch->carrying, obj, next_content) {
			// break out early
			if (size + 80 > sizeof(buf)) {
				size += snprintf(buf + size, sizeof(buf) - size, "... and more\r\n");
				break;
			}
			
			// qualify it
			if (*argument && !multi_isname(argument, GET_OBJ_KEYWORDS(obj))) {
				continue;	// not matching keywords
			}
			if (cmp_type != NOTHING && GET_OBJ_CMP_TYPE(obj) != cmp_type) {
				continue;	// not matching component type
			}
			if (cmpf_type != NOBITS && (GET_OBJ_CMP_FLAGS(obj) & cmpf_type) != cmpf_type) {
				continue;	// not matching component flags
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
			
			// looks okay
			if (identify) {
				size += snprintf(buf + size, sizeof(buf) - size, "%2d. %s (%d)\r\n", ++count, GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT), GET_OBJ_CURRENT_SCALE_LEVEL(obj));
			}
			else {
				size += snprintf(buf + size, sizeof(buf) - size, "%2d. %s\r\n", ++count, GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT));
			}
		}
		
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}
}


ACMD(do_look) {
	void look_in_direction(char_data *ch, int dir);
	
	char arg2[MAX_INPUT_LENGTH];
	room_data *map;
	int look_type;

	if (!ch->desc)
		return;

	if (GET_POS(ch) < POS_SLEEPING)
		send_to_char("You can't see anything but stars!\r\n", ch);
	else if (AFF_FLAGGED(ch, AFF_BLIND))
		send_to_char("You can't see a damned thing, you're blind!\r\n", ch);
	
	/* prior to b5.31, this block gave an abbreviated version of look_at_room, which isn't necessary
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch)) && ROOM_IS_CLOSED(IN_ROOM(ch))) {
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
			if (!(map = (GET_MAP_LOC(IN_ROOM(ch)) ? real_room(GET_MAP_LOC(IN_ROOM(ch))->vnum) : NULL))) {
				msg_to_char(ch, "You can't do that from here.\r\n");
			}
			else if (map == IN_ROOM(ch) && !ROOM_IS_CLOSED(IN_ROOM(ch))) {
				clear_recent_moves(ch);
				look_at_room_by_loc(ch, map, LRR_LOOK_OUT);
			}
			else if (!IS_IMMORTAL(ch) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_LOOK_OUT) && !RMT_FLAGGED(IN_ROOM(ch), RMT_LOOK_OUT)) {
				msg_to_char(ch, "You can't do that from here.\r\n");
			}
			else {
				clear_recent_moves(ch);
				look_at_room_by_loc(ch, map, LRR_LOOK_OUT);
			}
		}
		else if (is_abbrev(arg, "in"))
			look_in_obj(ch, arg2);
		/* did the char type 'look <direction>?' */
		else if ((look_type = parse_direction(ch, arg)) != NO_DIR)
			look_in_direction(ch, look_type);
		else if (is_abbrev(arg, "at"))
			look_at_target(ch, arg2);
		else
			look_at_target(ch, arg);
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
	int dist, dir;
	room_data *mark;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't mark the map.\r\n");
	}
	else if (GET_ROOM_VNUM(IN_ROOM(ch)) >= MAP_SIZE) {
		msg_to_char(ch, "You may only mark distances on the map or from the entry room of a building.\r\n");
	}
	else {
		skip_spaces(&argument);
	
		if (*argument && !str_cmp(argument, "set")) {
			GET_MARK_LOCATION(ch) = GET_ROOM_VNUM(IN_ROOM(ch));
			msg_to_char(ch, "You have marked this location. Use 'mark' again to see the distance to it from any other map location.\r\n");
		}
		else {
			// report
			if (GET_MARK_LOCATION(ch) == NOWHERE || !(mark = real_room(GET_MARK_LOCATION(ch)))) {
				msg_to_char(ch, "You have not marked a location to measure from. Use 'mark set' to do so.\r\n");
			}
			else {
				dist = compute_distance(mark, IN_ROOM(ch));
				dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), mark));
				
				if (HAS_NAVIGATION(ch)) {
					msg_to_char(ch, "Your mark at (%d, %d) is %d map tile%s %s.\r\n", X_COORD(mark), Y_COORD(mark), dist, (dist == 1 ? "" : "s"), (dir == NO_DIR ? "away" : dirs[dir]));
				}
				else {
					msg_to_char(ch, "Your mark is %d map tile%s %s.\r\n", dist, (dist == 1 ? "" : "s"), (dir == NO_DIR ? "away" : dirs[dir]));
				}
			}
		}
	}
}


ACMD(do_messages) {
	extern struct automessage *automessages;
	
	struct automessage *msg, *next_msg;
	char buf[MAX_STRING_LENGTH * 2];
	struct player_automessage *pam;
	time_t now = time(0);
	int id, count = 0;
	size_t size;
	
	size = snprintf(buf, sizeof(buf), "Recent messages:\r\n");
	
	HASH_ITER(hh, automessages, msg, next_msg) {
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
	void mudstats_empires(char_data *ch, char *argument);
	int iter, pos;
	
	const struct {
		char *name;
		void (*func)(char_data *ch, char *argument);
	} stat_list[] = {
		{ "empires", mudstats_empires },
		
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
	extern const char *alt_dirs[];
	extern int highest_start_loc_index;
	extern int *start_locs;
	
	int max_dist = room_has_function_and_city_ok(IN_ROOM(ch), FNC_LARGER_NEARBY) ? 150 : 50;
	
	bool cities = TRUE, adventures = TRUE, starts = TRUE;
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	struct instance_data *inst;
	struct empire_city_data *city;
	empire_data *emp, *next_emp;
	int iter, dist, dir, size;
	bool found = FALSE;
	room_data *loc;
	any_vnum vnum;
	
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
	
	// argument-parsing
	skip_spaces(&argument);
	while (*argument == '-') {
		++argument;
	}
	
	if (is_abbrev(argument, "city") || is_abbrev(argument, "cities")) {
		adventures = FALSE;
		starts = FALSE;
	}
	else if (is_abbrev(argument, "adventures")) {
		cities = FALSE;
		starts = FALSE;
	}
	else if (is_abbrev(argument, "starting locations") || is_abbrev(argument, "starts") || is_abbrev(argument, "towers") || is_abbrev(argument, "tower of souls") || is_abbrev(argument, "tos")) {
		cities = FALSE;
		adventures = FALSE;
	}
	
	// displaying:
	size = snprintf(buf, sizeof(buf), "You find nearby:\r\n");
	#define NEARBY_DIR  (dir == NO_DIR ? "away" : (PRF_FLAGGED(ch, PRF_SCREEN_READER) ? dirs[dir] : alt_dirs[dir]))

	// check starting locations
	if (starts) {
		for (iter = 0; iter <= highest_start_loc_index; ++iter) {
			loc = real_room(start_locs[iter]);
			dist = compute_distance(IN_ROOM(ch), loc);
		
			if (dist <= max_dist) {
				found = TRUE;

				dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), loc));
				snprintf(line, sizeof(line), " %2d %s: %s%s\r\n", dist, NEARBY_DIR, get_room_name(loc, FALSE), coord_display_room(ch, loc, FALSE));
				
				if (size + strlen(line) < sizeof(buf)) {
					strcat(buf, line);
					size += strlen(line);
				}
			}
		}
	}
	
	// check cities
	if (cities) {
		HASH_ITER(hh, empire_table, emp, next_emp) {
			for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
				loc = city->location;
				dist = compute_distance(IN_ROOM(ch), loc);

				if (dist <= max_dist) {
					found = TRUE;
				
					dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), loc));
					snprintf(line, sizeof(line), " %d %s: the %s of %s%s / %s%s&0\r\n", dist, NEARBY_DIR, city_type[city->type].name, city->name, coord_display_room(ch, loc, FALSE), EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
					
					if (size + strlen(line) < sizeof(buf)) {
						strcat(buf, line);
						size += strlen(line);
					}
				}
			}
		}
	}
	
	// check instances
	if (adventures) {
		LL_FOREACH(instance_list, inst) {
			glb = NULL;	// init this now -- used later
			
			if (!INST_FAKE_LOC(inst) || INSTANCE_FLAGGED(inst, INST_COMPLETED)) {
				continue;
			}
		
			if (ADVENTURE_FLAGGED(INST_ADVENTURE(inst), ADV_NO_NEARBY)) {
				continue;
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
			
			// owner part
			if (ROOM_OWNER(loc)) {
				snprintf(part, sizeof(part), " / %s%s&0", EMPIRE_BANNER(ROOM_OWNER(loc)), EMPIRE_NAME(ROOM_OWNER(loc)));
			}
			else {
				*part = '\0';
			}
		
			// show instance
			found = TRUE;
			dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), loc));
			snprintf(line, sizeof(line), " %d %s: %s%s / %s%s\r\n", dist, NEARBY_DIR, GET_ADV_NAME(INST_ADVENTURE(inst)), coord_display_room(ch, loc, FALSE), instance_level_string(inst), part);
			
			if (glb) {	// just add it to the global list
				if (glb->str) {
					free(glb->str);
				}
				glb->str = str_dup(line);
			}
			else if (size + strlen(line) < sizeof(buf)) {
				// not global: append to buf
				strcat(buf, line);
				size += strlen(line);
			}
		}
	}
	
	// add globals to buf and free global data
	HASH_ITER(hh, hash, glb, next_glb) {
		HASH_DEL(hash, glb);
		if (glb->str) {
			if (size + strlen(glb->str) < sizeof(buf)) {
				strcat(buf, glb->str);
				size += strlen(glb->str);
			}
			free(glb->str);
		}
		free(glb);
	}
	
	if (!found) {
		size += snprintf(buf + size, sizeof(buf) - size, " nothing\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


ACMD(do_no_cmd) {
	switch (subcmd) {
		case NOCMD_CAST: {
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
	struct empire_city_data *city;
	struct empire_island *eisle;
	struct island_info *island;
	int ter_type;
	bool junk;
	
	msg_to_char(ch, "You survey the area:\r\n");
	
	if ((island = GET_ISLAND(IN_ROOM(ch)))) {
		// find out if it has a local name
		if (GET_LOYALTY(ch) && (eisle = get_empire_island(GET_LOYALTY(ch), island->id)) && eisle->name && str_cmp(eisle->name, island->name)) {
			msg_to_char(ch, "Location: %s (%s)%s%s\r\n", get_island_name_for(island->id, ch), island->name, IS_SET(island->flags, ISLE_NEWBIE) ? " (newbie island)" : "", IS_SET(island->flags, ISLE_CONTINENT) ? " (continent)" : "");
		}
		else {
			msg_to_char(ch, "Location: %s%s%s\r\n", get_island_name_for(island->id, ch), IS_SET(island->flags, ISLE_NEWBIE) ? " (newbie island)" : "", IS_SET(island->flags, ISLE_CONTINENT) ? " (continent)" : "");
		}
	}
	
	// empire
	if (ROOM_OWNER(IN_ROOM(ch))) {
		if ((ter_type = get_territory_type_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), FALSE, &junk)) == TER_CITY && (city = find_closest_city(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch)))) {
			msg_to_char(ch, "This is the %s%s&0 %s of %s.\r\n", EMPIRE_BANNER(ROOM_OWNER(IN_ROOM(ch))), EMPIRE_ADJECTIVE(ROOM_OWNER(IN_ROOM(ch))), city_type[city->type].name, city->name);
		}
		else if (ter_type == TER_OUTSKIRTS) {
			msg_to_char(ch, "This is the outskirts of %s%s&0.\r\n", EMPIRE_BANNER(ROOM_OWNER(IN_ROOM(ch))), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))));
		}
		else {
			msg_to_char(ch, "This area is claimed by %s%s&0.\r\n", EMPIRE_BANNER(ROOM_OWNER(IN_ROOM(ch))), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))));
		}
	}
	else if (GET_LOYALTY(ch)) {
		ter_type = get_territory_type_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), FALSE, &junk);
		msg_to_char(ch, "This location would be %s for your empire.\r\n", (ter_type == TER_CITY ? "in a city" : (ter_type == TER_OUTSKIRTS ? "on the outskirts of a city" : "on the frontier")));
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
	
	// adventure info
	if (find_instance_by_room(IN_ROOM(ch), FALSE, TRUE)) {
		do_adventure(ch, "", 0, 0);
	}
}


ACMD(do_time) {
	extern const char *seasons[];
	extern int pick_season(room_data *room);
	extern const char *weekdays[];
	const char *suf;
	int weekday, day;

	sprintf(buf, "It is %d o'clock %s, on ", ((time_info.hours % 12 == 0) ? 12 : ((time_info.hours) % 12)), ((time_info.hours >= 12) ? "pm" : "am"));

	/* 30 days in a month */
	weekday = ((30 * time_info.month) + time_info.day + 1) % 7;

	strcat(buf, weekdays[weekday]);
	strcat(buf, "\r\n");
	send_to_char(buf, ch);

	day = time_info.day + 1;	/* day in [1..30] */

	/* 11, 12, and 13 are the infernal exceptions to the rule */
	if ((day % 10) == 1 && day != 11)
		suf = "st";
	else if ((day % 10) == 2 && day != 12)
		suf = "nd";
	else if ((day % 10) == 3 && day != 13)
		suf = "rd";
	else
		suf = "th";

	msg_to_char(ch, "The %d%s day of the month of %s, year %d.\r\n", day, suf, month_name[(int) time_info.month], time_info.year);
	
	if (IS_OUTDOORS(ch)) {
		msg_to_char(ch, "%s\r\n", seasons[pick_season(IN_ROOM(ch))]);
	}
}


ACMD(do_tip) {
	void display_tip_to_char(char_data *ch);
	display_tip_to_char(ch);
}


ACMD(do_weather) {
	extern const char *seasons[];
	extern int pick_season(room_data *room);
	void list_moons_to_char(char_data *ch);
	const char *sky_look[] = {
		"cloudless",
		"cloudy",
		"rainy",
		"lit by flashes of lightning"
		};
	
	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_WEATHER)) {
		msg_to_char(ch, "There's nothing interesting about the weather.\r\n");
	}
	else if (IS_OUTDOORS(ch)) {
		msg_to_char(ch, "The sky is %s and %s.\r\n", sky_look[weather_info.sky], (weather_info.change >= 0 ? "you feel a warm wind from the south" : "your foot tells you bad weather is due"));
		if (weather_info.sunlight == SUN_SET || weather_info.sunlight == SUN_DARK) {
			list_moons_to_char(ch);
		}

		msg_to_char(ch, "%s\r\n", seasons[pick_season(IN_ROOM(ch))]);
	}
	else {
		msg_to_char(ch, "You can't see the sky from here.\r\n");
	}
}


ACMD(do_whereami) {
	msg_to_char(ch, "You are at: %s%s\r\n", get_room_name(IN_ROOM(ch), FALSE), coord_display_room(ch, IN_ROOM(ch), FALSE));
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
	void check_delayed_load(char_data *ch);
	extern const char *genders[];
	extern const char *level_names[][2];
	
	char part[MAX_STRING_LENGTH];
	char_data *victim = NULL;
	bool file = FALSE;
	int level;

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
	
	// basic info
	msg_to_char(ch, "%s%s&0\r\n", PERS(victim, victim, TRUE), NULLSAFE(GET_TITLE(victim)));
	sprinttype(GET_REAL_SEX(victim), genders, part);
	msg_to_char(ch, "Status: %s %s\r\n", CAP(part), level_names[(int) GET_ACCESS_LEVEL(victim)][1]);

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

	if (GET_LORE(victim)) {
		list_lore_to_char(victim, ch);
	}

	// cleanup	
	if (victim && file) {
		free_char(victim);
	}
}
