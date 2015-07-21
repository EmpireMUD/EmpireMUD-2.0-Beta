/* ************************************************************************
*   File: act.informative.c                               EmpireMUD 2.0b2 *
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

/**
* Contents:
*   Helpers
*   Look Assist Functions
*   Character Display Functions
*   Object Display Functions
*   Commands
*/

// extern variables
extern struct city_metadata_type city_type[];
extern const char *dirs[];
extern struct help_index_element *help_table;
extern int top_of_helpt;
extern const char *month_name[];
extern const struct wear_data_type wear_data[NUM_WEARS];

// external functions
extern struct instance_data *find_instance_by_room(room_data *room);
extern char *get_room_name(room_data *room, bool color);
extern char *morph_string(char_data *ch, byte type);

// local protos
ACMD(do_affects);
void list_obj_to_char(obj_data *list, char_data *ch, int mode, int show);
void list_one_char(char_data *i, char_data *ch, int num);
void look_at_char(char_data *i, char_data *ch, bool show_eq);
void show_obj_to_char(obj_data *obj, char_data *ch, int mode);
void show_one_stored_item_to_char(char_data *ch, empire_data *emp, struct empire_storage_data *store, bool show_zero);

// for the who list
#define WHO_MORTALS  0
#define WHO_GODS  1
#define WHO_IMMORTALS  2


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
	
	if (inst->level) {
		snprintf(output, sizeof(output), "level %d", inst->level);
	}
	else if (GET_ADV_MIN_LEVEL(inst->adventure) > 0 && GET_ADV_MAX_LEVEL(inst->adventure) == 0) {
		snprintf(output, sizeof(output), "levels %d+", GET_ADV_MIN_LEVEL(inst->adventure));
	}
	else if (GET_ADV_MIN_LEVEL(inst->adventure) == 0 && GET_ADV_MAX_LEVEL(inst->adventure) > 0) {
		snprintf(output, sizeof(output), "levels 1-%d", GET_ADV_MAX_LEVEL(inst->adventure));
	}
	else if (GET_ADV_MIN_LEVEL(inst->adventure) == GET_ADV_MAX_LEVEL(inst->adventure)) {
		snprintf(output, sizeof(output), "level %d", GET_ADV_MAX_LEVEL(inst->adventure));
	}
	else {
		snprintf(output, sizeof(output), "levels %d-%d", GET_ADV_MIN_LEVEL(inst->adventure), GET_ADV_MAX_LEVEL(inst->adventure));
	}
	
	return output;
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
	char *desc;

	if (!ch->desc)
		return;

	if (!*arg) {
		send_to_char("Look at what?\r\n", ch);
		return;
		}

	bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP | FIND_CHAR_ROOM, ch, &found_char, &found_obj);

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
	extern const char *color_liquid[];
	extern const char *fullness[];
	obj_data *obj = NULL;
	char_data *dummy = NULL;
	int amt, bits;

	if (!*arg)
		send_to_char("Look in what?\r\n", ch);
	else if (!(bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &dummy, &obj))) {
		sprintf(buf, "There doesn't seem to be %s %s here.\r\n", AN(arg), arg);
		send_to_char(buf, ch);
	}
	else if ((GET_OBJ_TYPE(obj) != ITEM_DRINKCON) && (GET_OBJ_TYPE(obj) != ITEM_CORPSE) && (GET_OBJ_TYPE(obj) != ITEM_CONTAINER) && (GET_OBJ_TYPE(obj) != ITEM_CART))
		send_to_char("There's nothing inside that!\r\n", ch);
	else {
		if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER || GET_OBJ_TYPE(obj) == ITEM_CORPSE || GET_OBJ_TYPE(obj) == ITEM_CART) {
			if (OBJVAL_FLAGGED(obj, CONT_CLOSED) && GET_OBJ_TYPE(obj) != ITEM_CORPSE && GET_OBJ_TYPE(obj) != ITEM_CART)
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
					sprinttype(GET_DRINK_CONTAINER_TYPE(obj), color_liquid, buf2);
					sprintf(buf, "It's %sfull of a %s liquid.\r\n", fullness[amt], buf2);
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
	if (!ch || !to || !to->desc) {
		return;
	}
	
	msg_to_char(to, "        Physical                   Social                      Mental\r\n");
	msg_to_char(to, "  Strength  [%s%2d&0]           Charisma  [%s%2d&0]           Intelligence  [%s%2d&0]\r\n", HAPPY_COLOR(GET_STRENGTH(ch), ch->real_attributes[STRENGTH]), GET_STRENGTH(ch), HAPPY_COLOR(GET_CHARISMA(ch), ch->real_attributes[CHARISMA]), GET_CHARISMA(ch), HAPPY_COLOR(GET_INTELLIGENCE(ch), ch->real_attributes[INTELLIGENCE]), GET_INTELLIGENCE(ch));
	msg_to_char(to, "  Dexterity  [%s%2d&0]          Greatness  [%s%2d&0]          Wits  [%s%2d&0]\r\n", HAPPY_COLOR(GET_DEXTERITY(ch), ch->real_attributes[DEXTERITY]), GET_DEXTERITY(ch), HAPPY_COLOR(GET_GREATNESS(ch), ch->real_attributes[GREATNESS]), GET_GREATNESS(ch), HAPPY_COLOR(GET_WITS(ch), ch->real_attributes[WITS]), GET_WITS(ch));
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
	extern int get_blood_upkeep_cost(char_data *ch);
	extern int get_crafting_level(char_data *ch);
	extern int total_bonus_healing(char_data *ch);
	extern int get_dodge_modifier(char_data *ch, char_data *attacker, bool can_gain_skill);
	extern int get_to_hit(char_data *ch, char_data *victim, bool off_hand, bool can_gain_skill);
	extern int health_gain(char_data *ch, bool info_only);
	extern int move_gain(char_data *ch, bool info_only);
	extern int mana_gain(char_data *ch, bool info_only);
	extern int get_ability_points_available_for_char(char_data *ch, int skill);
	extern const struct material_data materials[NUM_MATERIALS];
	extern int skill_sort[NUM_SKILLS];
	extern const int base_hit_chance;
	extern const double hit_per_dex;

	char lbuf[MAX_STRING_LENGTH], lbuf2[MAX_STRING_LENGTH], lbuf3[MAX_STRING_LENGTH];
	int i, j, count, iter, sk, pts, cols, val;
	empire_data *emp;
	struct time_info_data playing_time;


	msg_to_char(to, " +----------------------------- EmpireMUD 2.0b2 -----------------------------+\r\n");
	
	// row 1 col 1: name
	msg_to_char(to, "  Name: %-18.18s", PERS(ch, ch, 1));

	// row 1 col 2: class
	msg_to_char(to, " Class: %-17.17s", IS_IMMORTAL(ch) ? "Immortal" : class_data[GET_CLASS(ch)].name);
	msg_to_char(to, " Level: %d (%d)\r\n", GET_COMPUTED_LEVEL(ch), GET_SKILL_LEVEL(ch));

	// row 1 col 3: levels

	// row 2 col 1
	if (IS_VAMPIRE(ch) && GET_REAL_AGE(ch) > GET_AGE(ch)) {
		sprintf(buf, "%d/%d years", GET_AGE(ch), GET_REAL_AGE(ch));
	}
	else {
		sprintf(buf, "%d years", GET_AGE(ch));
	}
	playing_time = *real_time_passed((time(0) - ch->player.time.logon) + ch->player.time.played, 0);
	sprintf(buf1, "%dd, %dh", playing_time.day, playing_time.hours);
	msg_to_char(to, "  Age: %-19.19s Play Time: %-13.13s", buf, buf1);
	
	// row 2 col 3: rank
	if ((emp = GET_LOYALTY(ch))) {
		msg_to_char(to, " Rank: %s&0\r\n", EMPIRE_RANK(emp, GET_RANK(ch)-1));
	}
	else {
		msg_to_char(to, "\r\n");
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
	count = 37 + (2 * count_color_codes(lbuf));
	sprintf(lbuf2, "  Conditions: %%-%d.%ds", count, count);
	msg_to_char(to, lbuf2, lbuf);
	
	if (IS_VAMPIRE(ch)) {
		msg_to_char(to, " Blood: &r%d&0/&r%d&0-&r%d&0/hr\r\n", GET_BLOOD(ch), GET_MAX_BLOOD(ch), get_blood_upkeep_cost(ch));
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
	sprintf(lbuf3, "Crafting [%s%d&0]", HAPPY_COLOR(get_crafting_level(ch), GET_SKILL_LEVEL(ch)), get_crafting_level(ch));
	// note: the "%-24.24s" for speed is lower because it contains no color codes
	msg_to_char(to, "  %-28.28s %-24.24s %-28.28s\r\n", lbuf, lbuf2, lbuf3);

	msg_to_char(to, " +--------------------------------- Skills ----------------------------------+\r\n ");

	count = 0;
	for (iter = 0; iter < NUM_SKILLS; ++iter) {
		sk = skill_sort[iter];
		if (GET_SKILL(ch, sk) > 0) {
			sprintf(lbuf, " %s: %s%d", skill_data[sk].name, IS_ANY_SKILL_CAP(ch, sk) ? "&g" : "&y", GET_SKILL(ch, sk));
			pts = get_ability_points_available_for_char(ch, sk);
			if (pts > 0) {
				sprintf(lbuf + strlen(lbuf), " &g(%d)", pts);
			}
			
			cols = 25 + (2 * count_color_codes(lbuf));
			sprintf(lbuf2, "%%-%d.%ds&0", cols, cols);
			msg_to_char(to, lbuf2, lbuf);
			
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
	#define MOB_CAN_STACK(ch)  (use_mob_stacking && !GET_LED_BY(ch) && !GET_PULLING(ch) && GET_POS(ch) != POS_FIGHTING && !MOB_FLAGGED((ch), MOB_EMPIRE | MOB_TIED | MOB_MOUNTABLE | MOB_FAMILIAR))
	
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
	extern char *get_name_by_id(int id);
	extern long load_time();

	struct lore_data *lore;
	char daystring[MAX_INPUT_LENGTH];
	struct time_info_data t;
	empire_data *e;
	long beginning_of_time = load_time();

	msg_to_char(to, "%s's lore:\r\n", PERS(ch, ch, 1));

	for (lore = GET_LORE(ch); lore; lore = lore->next) {
		t = *mud_time_passed((time_t) lore->date, (time_t) beginning_of_time);

		strcpy(buf, month_name[(int) t.month]);
		if (!strn_cmp(buf, "the ", 4))
			strcpy(buf1, buf + 4);
		else
			strcpy(buf1, buf);

		sprintf(daystring, "%d %s, Year %d", t.day + 1, buf1, t.year);

		switch (lore->type) {
			case LORE_FOUND_EMPIRE:
				if ((e = real_empire(lore->value)))
					msg_to_char(to, " Proudly founded %s%s&0 on %s.\r\n", EMPIRE_BANNER(e), EMPIRE_NAME(e), daystring);
				break;
			case LORE_JOIN_EMPIRE:
				if ((e = real_empire(lore->value)))
					msg_to_char(to, " Honorably accepted into %s%s&0 on %s.\r\n", EMPIRE_BANNER(e), EMPIRE_NAME(e), daystring);
				break;
			case LORE_DEFECT_EMPIRE:
				if ((e = real_empire(lore->value)))
					msg_to_char(to, " Defected from %s%s&0 on %s.\r\n", EMPIRE_BANNER(e), EMPIRE_NAME(e), daystring);
				break;
			case LORE_KICKED_EMPIRE:
				if ((e = real_empire(lore->value)))
					msg_to_char(to, " Dishonorably discharged from %s%s&0 on %s.\r\n", EMPIRE_BANNER(e), EMPIRE_NAME(e), daystring);
				break;
			case LORE_PLAYER_KILL:
				msg_to_char(to, " Killed %s in battle on %s.\r\n", get_name_by_id(lore->value) ? CAP(get_name_by_id(lore->value)) : "an unknown foe", daystring);
				break;
			case LORE_PLAYER_DEATH:
				msg_to_char(to, " Slain by %s in battle on %s.\r\n", get_name_by_id(lore->value) ? CAP(get_name_by_id(lore->value)) : "an unknown foe", daystring);
				break;
			case LORE_TOWER_DEATH:
				msg_to_char(to, " Killed by a guard tower on %s.\r\n", daystring);
				break;
			case LORE_DEATH:
				msg_to_char(to, " Died on %s.\r\n", daystring);
				break;
			case LORE_START_VAMPIRE:
				if (IS_VAMPIRE(to))
					msg_to_char(to, " Sired prior to %s.\r\n", daystring);
				break;
			case LORE_PURIFY:
				msg_to_char(to, " Purified on %s.\r\n", daystring);
				break;
			case LORE_SIRE_VAMPIRE:
				if (IS_VAMPIRE(to))
					msg_to_char(to, " Sired by %s on %s.\r\n", get_name_by_id(lore->value) ? CAP(get_name_by_id(lore->value)) : "an unknown Cainite", daystring);
				break;
			case LORE_MAKE_VAMPIRE:
				if (IS_VAMPIRE(to))
					msg_to_char(to, " Sired %s on %s.\r\n", get_name_by_id(lore->value) ? CAP(get_name_by_id(lore->value)) : "an unknown Cainite", daystring);
				break;
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
	extern struct action_data_struct action_data[];
	
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

	if (num > 1) {
		msg_to_char(ch, "(%2d) ", num);
	}

	if (GROUP(i) && in_same_group(ch, i)) {
		msg_to_char(ch, "(%s) ", GROUP_LEADER(GROUP(i)) == i ? "leader" : "group");
	}
	
	// empire prefixing
	if (!MORPH_FLAGGED(i, MORPH_FLAG_ANIMAL)) {
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
				msg_to_char(ch, "<%s&y> ", EMPIRE_RANK(GET_LOYALTY(i), GET_RANK(i)-1));
			}
		}
	}
	
	if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_ROOMFLAGS) && IS_NPC(i)) {
		msg_to_char(ch, "[%d] %s", GET_MOB_VNUM(i), SCRIPT(i) ? "[TRIG] " : "");
	}

	if (IS_NPC(i) && GET_LONG_DESC(i) && GET_POS(i) == POS_STANDING) {
		if (AFF_FLAGGED(i, AFF_INVISIBLE)) {
			msg_to_char(ch, "*");
		}

		msg_to_char(ch, GET_LONG_DESC(i));
	}
	else {
		if (IS_NPC(i)) {
			strcpy(buf, GET_SHORT_DESC(i));
			CAP(buf);
		}
		else {
			if (AFF_FLAGGED(i, AFF_INVISIBLE))
				strcpy(buf, "*");
			else
				*buf = '\0';

			sprintf(buf1, PERS(i, ch, 0));
			strcat(buf, CAP(buf1));

			switch (GET_SEX(i)) {
				case SEX_MALE:		strcpy(buf1, "man");	break;
				case SEX_FEMALE:	strcpy(buf1, "woman");	break;
				default:			strcpy(buf1, "person");	break;
			}
		}

		if (AFF_FLAGGED(i, AFF_HIDE))
			strcat(buf, " (hidden)");
		if (!IS_NPC(i) && !i->desc)
			strcat(buf, " (linkless)");
		if (!IS_NPC(i) && PLR_FLAGGED(i, PLR_WRITING))
			strcat(buf, " (writing)");

		if (GET_POS(i) != POS_FIGHTING) {
			if (ON_CHAIR(i) && !IS_DEAD(i)) {
				sprintf(buf + strlen(buf), " is sitting on %s.", CAN_SEE_OBJ(ch, ON_CHAIR(i)) ? GET_OBJ_DESC(ON_CHAIR(i), ch, OBJ_DESC_SHORT) : "something");			
			}
			else if (!IS_NPC(i) && GET_ACTION(i) != ACT_NONE) {
				sprintf(buf + strlen(buf), " %s", action_data[GET_ACTION(i)].long_desc);
			}
			else if (AFF_FLAGGED(i, AFF_DEATHSHROUD) && GET_POS(i) <= POS_RESTING) {
				sprintf(buf + strlen(buf), " %s", positions[POS_DEAD]);
			}
			else if (GET_POS(i) == POS_STANDING && AFF_FLAGGED(i, AFF_FLY)) {
				strcat(buf, " is flying here.");
			}
			else {	// normal positions
				sprintf(buf + strlen(buf), " %s", positions[(int) GET_POS(i)]);
			}
		}
		else {
			if (FIGHTING(i)) {
				strcat(buf, " is here, fighting ");
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
				strcat(buf, " is here struggling with thin air.");
		}

		msg_to_char(ch, "%s\r\n", buf);
	}

	if (affected_by_spell(i, ATYPE_FLY)) {
		act("...$e is flying with gossamer mana wings!", FALSE, i, 0, ch, TO_VICT);
	}
	if (IS_RIDING(i)) {
		sprintf(buf, "...$E is mounted upon %s.", get_mob_name_by_proto(GET_MOUNT_VNUM(i)));
		act(buf, FALSE, ch, 0, i, TO_CHAR);
	}
	if (GET_LED_BY(i)) {
		sprintf(buf, "...%s is being led by %s.", HSSH(i), GET_LED_BY(i) == ch ? "you" : "$N");
		act(buf, FALSE, ch, 0, GET_LED_BY(i), TO_CHAR);
	}
	if (GET_PULLING(i)) {
		act("...$e is pulling $p.", FALSE, i, GET_PULLING(i), ch, TO_VICT);
	}
	if (MOB_FLAGGED(i, MOB_TIED))
		act("...$e is tied up here.", FALSE, i, 0, ch, TO_VICT);
	if (AFF_FLAGGED(i, AFF_STUNNED)) {
		act("...$e is stunned!", FALSE, i, 0, ch, TO_VICT);
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
	if (IS_IMMORTAL(ch) && !IS_NPC(i) && GET_MORPH(i) != MORPH_NONE)
		act("...this appears to be $o.", FALSE, i, 0, ch, TO_VICT);
	
	if (IS_IMMORTAL(ch) && !IS_NPC(i) && IS_DISGUISED(i)) {
		act("...this appears to be $o.", FALSE, i, 0, ch, TO_VICT);
	}
	
	// these 
	if (AFF_FLAGGED(i, AFF_NO_SEE_IN_ROOM) && IS_IMMORTAL(ch)) {
		if (AFF_FLAGGED(i, AFF_EARTHMELD)) {
			act("...$e is earthmelded.", FALSE, i, 0, ch, TO_VICT);
		}
		else {
			act("...$e is unable to be seen by players.", FALSE, i, 0, ch, TO_VICT);
		}
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
	char buf[MAX_STRING_LENGTH];
	int j, found;

	if (!i || !ch || !ch->desc)
		return;
	
	if (show_eq && ch != i && !IS_IMMORTAL(ch) && !IS_NPC(i) && HAS_ABILITY(i, ABIL_CONCEALMENT)) {
		show_eq = FALSE;
		gain_ability_exp(i, ABIL_CONCEALMENT, 5);
	}

	if (ch != i) {
		act("You look at $N.", FALSE, ch, 0, i, TO_CHAR);
	}

	// For morphs, we show a description
	if (!IS_NPC(i) && GET_MORPH(i) != MORPH_NONE) {
		act(morph_string(i, MORPH_STRING_DESC), FALSE, ch, 0, i, TO_CHAR);
	}
	
	// only show this block if the person is not morphed, or the morph is not an npc disguise
	if (IS_NPC(i) || IS_IMMORTAL(ch) || GET_MORPH(i) == MORPH_NONE || !MORPH_FLAGGED(i, MORPH_FLAG_ANIMAL)) {
		
		if (GET_LOYALTY(i) && !IS_DISGUISED(i) && !MORPH_FLAGGED(i, MORPH_FLAG_ANIMAL)) {
			sprintf(buf, "   $E is a member of %s.", EMPIRE_NAME(GET_LOYALTY(i)));
			act(buf, FALSE, ch, NULL, i, TO_CHAR);
		}
		
		if (!IS_NPC(i) && !IS_DISGUISED(i)) {
			// basic description -- don't show if morphed
			if (GET_LONG_DESC(i) && (IS_NPC(i) || GET_MORPH(i) == MORPH_NONE)) {
				msg_to_char(ch, "%s&0", GET_LONG_DESC(i));
			}

			if (HAS_INFRA(i)) {
				act("   You notice a distinct, red glint in $S eyes.", FALSE, ch, NULL, i, TO_CHAR);
			}
			if (AFF_FLAGGED(i, AFF_CLAWS)) {
				act("   $N's hands are huge, distorted, and very sharp!", FALSE, ch, NULL, i, TO_CHAR);
			}
			if (AFF_FLAGGED(i, AFF_MAJESTY)) {
				act("   $N has an aura of majesty about $M.", FALSE, ch, NULL, i, TO_CHAR);
			}
			if (AFF_FLAGGED(i, AFF_MUMMIFY)) {
				act("   $E is mummified in a hard, dark substance!", FALSE, ch, NULL, i, TO_CHAR);
			}
			diag_char_to_char(i, ch);
		}
		else {
			diag_char_to_char(i, ch);
		}

		if (!show_eq) {
			return;
		}

		found = FALSE;
		for (j = 0; !found && j < NUM_WEARS; j++) {
			if (GET_EQ(i, j) && CAN_SEE_OBJ(ch, GET_EQ(i, j))) {
				found = TRUE;
			}
		}

		if (found) {
			msg_to_char(ch, "\r\n");	/* act() does capitalization. */
			act("$n is using:", FALSE, i, 0, ch, TO_VICT);
			for (j = 0; j < NUM_WEARS; j++) {
				if (GET_EQ(i, j) && CAN_SEE_OBJ(ch, GET_EQ(i, j))) {
					msg_to_char(ch, wear_data[j].eq_prompt);
					show_obj_to_char(GET_EQ(i, j), ch, OBJ_DESC_EQUIPMENT);
				}
			}
		}
	}

	if (!show_eq) {
		return;
	}

	if (ch != i && (IS_IMMORTAL(ch) || IS_NPC(i) || GET_MORPH(i) == MORPH_NONE || !MORPH_FLAGGED(i, MORPH_FLAG_ANIMAL)) && HAS_ABILITY(ch, ABIL_APPRAISAL)) {
		act("\r\nYou appraise $s inventory:", FALSE, i, 0, ch, TO_VICT);
		list_obj_to_char(i->carrying, ch, OBJ_DESC_INVENTORY, TRUE);

		if (ch != i && i->carrying) {
			gain_ability_exp(ch, ABIL_APPRAISAL, 5);
			GET_WAIT_STATE(ch) = MAX(GET_WAIT_STATE(ch), 0.5 RL_SEC);
		}
	}
}


/**
* Get the "who" display for one person.
*
* @param char_data *ch The person to get WHO info for.
* @param bool shortlist If TRUE, only gets a short entry.
* @return char* A pointer to the output.
*/
char *one_who_line(char_data *ch, bool shortlist) {
	static char out[MAX_STRING_LENGTH];
	char buf[MAX_STRING_LENGTH], buf1[MAX_STRING_LENGTH];
	int num, size = 0;
	
	*out = '\0';
	
	// level/class info
	if (!IS_GOD(ch) && !IS_IMMORTAL(ch)) {
		if (shortlist) {
			size += snprintf(out + size, sizeof(out) - size, "[%3d] ", GET_COMPUTED_LEVEL(ch));
		}
		else if (GET_CLASS(ch) != CLASS_NONE) {
			size += snprintf(out + size, sizeof(out) - size, "[%3d %s] ", GET_COMPUTED_LEVEL(ch), class_data[GET_CLASS(ch)].abbrev);
		}
		else {	// classless
			size += snprintf(out + size, sizeof(out) - size, "[%3d Advn] ", GET_COMPUTED_LEVEL(ch));
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
		num = count_color_codes(out);
		sprintf(buf, "%%-%d.%ds", 35 + 2 * num, 35 + 2 * num);
		strcpy(buf1, out);
		
		size = snprintf(out, sizeof(out), buf, buf1);
		
		// append invis even in short list
		if (GET_INVIS_LEV(ch)) {
			size += snprintf(out + size, sizeof(out) - size, " (i%d)", GET_INVIS_LEV(ch));
		}
		
		return out;
	}
	
	// title
	size += snprintf(out + size, sizeof(out) - size, "%s&0", GET_TITLE(ch));
	
	// tags
	if (IS_AFK(ch)) {
		size += snprintf(out + size, sizeof(out) - size, " &r[AFK]&0");
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
	else if (AFF_FLAGGED(ch, AFF_INVISIBLE)) {
		size += snprintf(out + size, sizeof(out) - size, " (invis)");
	}
	if (PLR_FLAGGED(ch, PLR_WRITING)) {
		size += snprintf(out + size, sizeof(out) - size, " &c(writing)&0");
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
	extern int max_players_today;
	extern int max_players_this_uptime;
	
	static char who_output[MAX_STRING_LENGTH];
	char whobuf[MAX_STRING_LENGTH], buf[MAX_STRING_LENGTH], online[MAX_STRING_LENGTH];
	descriptor_data *d;
	char_data *tch;
	int iter, count = 0, size;
	
	// WHO_x
	const char *who_titles[] = { "Mortals", "Gods", "Immortals" };

	*whobuf = '\0';	// lines of chars
	size = 0;	// whobuf size

	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) != CON_PLAYING)
			continue;

		if (d->original)
			tch = d->original;
		else if (!(tch = d->character))
			continue;

		if (*name_search && !is_abbrev(name_search, PERS(tch, tch, 1)) && !strstr(GET_TITLE(tch), name_search))
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

		// show one char
		++count;
		size += snprintf(whobuf + size, sizeof(whobuf) - size, "%s", one_who_line(tch, shortlist));
		
		// columnar spacing
		if (shortlist) {
			size += snprintf(whobuf + size, sizeof(whobuf) - size, "%s", !(count % 2) ? "\r\n" : " ");
		}
	}

	if (*whobuf) {
		// repurposing size
		size = 0;
		
		if (type == WHO_MORTALS) {
			// update counts in case
			max_players_today = MAX(max_players_today, count);
			max_players_this_uptime = MAX(max_players_this_uptime, count);
			snprintf(online, sizeof(online), "%d online (max today %d, this uptime %d)", count, max_players_today, max_players_this_uptime);
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


/**
* Lists the affects on a character in a way that can be shown to players.
* 
* @param char_data *ch Whose effects
* @param char_data *to Who to send to
*/
void show_character_affects(char_data *ch, char_data *to) {
	extern const char *apply_types[];
	extern const char *affect_types[];
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
		sprintf(buf, "   &c%s&0 (%s) ", affect_types[aff->type], lbuf);

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
		msg_to_char(to, "  &r%s&0 (%s) %d %s damage (%d/%d)\r\n", affect_types[dot->type], lbuf, dot->damage * dot->stack, damage_types[dot->damage_type], dot->stack, dot->max_stack);
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
	extern const char *drinks[];
	extern char *get_book_item_name_by_id(int id);
	extern const struct material_data materials[NUM_MATERIALS];

	static char output[MAX_STRING_LENGTH];
	char sdesc[MAX_STRING_LENGTH];

	*output = '\0';

	/* sdesc will be empty unless the short desc is modified */
	*sdesc = '\0';

	if (IS_BOOK(obj)) {
		strcpy(sdesc, get_book_item_name_by_id(GET_BOOK_ID(obj)));
	}
	else if (IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_CONTENTS(obj) > 0) {
		sprintf(sdesc, "%s of %s", GET_OBJ_SHORT_DESC(obj), drinks[GET_DRINK_CONTAINER_TYPE(obj)]);
	}
	else if (IS_ARROW(obj)) {
		sprintf(sdesc, "%s (%d)", GET_OBJ_SHORT_DESC(obj), MAX(1, GET_ARROW_QUANTITY(obj)));
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
	bool found = FALSE;
	struct empire_storage_data *store;
	obj_data *proto;

	/* Must be in an empire */
	if (!emp) {
		return found;
	}
	
	if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_VAULT)) {
		msg_to_char(ch, "\r\nVault: %d coin%s, %d treasure (%d total)\r\n", EMPIRE_COINS(emp), (EMPIRE_COINS(emp) != 1 ? "s" : ""), EMPIRE_WEALTH(emp), GET_TOTAL_WEALTH(emp));
	}

	for (store = EMPIRE_STORAGE(emp); store; store = store->next) {
		// things not stored here
		if (store->island != GET_ISLAND_ID(room)) {
			continue;
		}
		
		if ((proto = obj_proto(store->vnum))) {
			if (obj_can_be_stored(proto, room)) {
				if (!found) {
					msg_to_char(ch, "\r\n%s inventory available here:\r\n", EMPIRE_ADJECTIVE(emp));
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
			
			strcpy(buf, GET_OBJ_DESC(i, ch, OBJ_DESC_SHORT));
			for (j = list; j != i; j = j->next_content) {
				if (OBJ_CAN_STACK(j) && OBJS_ARE_SAME(i, j)) {
					if (!strcmp(buf, GET_OBJ_DESC(j, ch, OBJ_DESC_SHORT))) {
						if (GET_OBJ_VNUM(j) == NOTHING)
							break;
						else if (GET_OBJ_VNUM(j) == GET_OBJ_VNUM(i))
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
					strcpy(buf, GET_OBJ_DESC(j, ch, OBJ_DESC_SHORT));
					if (!strcmp(buf, GET_OBJ_DESC(i, ch, OBJ_DESC_SHORT))) {
						if (GET_OBJ_VNUM(j) == NOTHING)
							num++;
						else if (GET_OBJ_VNUM(j) == GET_OBJ_VNUM(i))
							num++;
					}
				}
			}
		}

		if (CAN_SEE_OBJ(ch, i) && (!IN_CHAIR(i) || ch == IN_CHAIR(i))) {
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
	extern char *get_book_item_description_by_id(int id);
	extern int Board_show_board(int board_type, char_data *ch, char *arg, obj_data *board);
	extern int board_loaded;
	void init_boards(void);
	extern int find_board(char_data *ch);
	extern const char *extra_bits_inv_flags[];

	char flags[256];
	int board_type;
	room_data *room;

	// initialize buf as the obj desc
	if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
		sprintf(buf, "[%d] %s%s", GET_OBJ_VNUM(obj), (SCRIPT(obj) ? "[TRIG] " : ""), GET_OBJ_DESC(obj, ch, mode));
	}
	else {
		strcpy(buf, GET_OBJ_DESC(obj, ch, mode));
	}
	
	if (mode == OBJ_DESC_LONG && IN_CHAIR(obj) == ch) {
		strcat(buf, "...you are sitting on it.");
	}

	if (mode == OBJ_DESC_EQUIPMENT) {
		if (obj->worn_by && AFF_FLAGGED(obj->worn_by, AFF_DISARM) && (obj->worn_on == WEAR_WIELD || obj->worn_on == WEAR_RANGED || obj->worn_on == WEAR_HOLD)) {
			strcat(buf, " (disarmed)");
		}
	}
	
	if (mode == OBJ_DESC_INVENTORY || mode == OBJ_DESC_EQUIPMENT || mode == OBJ_DESC_CONTENTS || mode == OBJ_DESC_LONG) {
		sprintbit(GET_OBJ_EXTRA(obj), extra_bits_inv_flags, flags, TRUE);
		if (strncmp(flags, "NOBITS", 6)) {
			sprintf(buf + strlen(buf), " %*s", ((int)strlen(flags)-1), flags);	// remove trailing space
		}
		
		if (IS_STOLEN(obj)) {
			strcat(buf, " (STOLEN)");
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
		else if (IS_BOOK(obj)) {
			strcpy(buf, get_book_item_description_by_id(GET_BOOK_ID(obj)));
		}
		else if (GET_OBJ_TYPE(obj) == ITEM_MAIL) {
			page_string(ch->desc, GET_OBJ_ACTION_DESC(obj) ? GET_OBJ_ACTION_DESC(obj) : "It's blank.\r\n", 1);
			return;
		}
		else if (IS_PORTAL(obj)) {
			room = real_room(GET_PORTAL_TARGET_VNUM(obj));
			if (room) {
				sprintf(buf, "%sYou peer into %s and see: %s", NULLSAFE(GET_OBJ_ACTION_DESC(obj)), GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT), get_room_name(room, TRUE));
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
	
	if (buf[strlen(buf)-1] != '\n') {
		strcat(buf, "\r\n");
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
	int total = get_total_stored_count(emp, store->vnum);
	char lbuf[MAX_INPUT_LENGTH];
	
	if (total > store->amount || show_zero) {
		sprintf(lbuf, " (%d total)", total);
	}
	else {
		*lbuf = '\0';
	}
	
	msg_to_char(ch, "(%4d) %s%s\r\n", (show_zero ? 0 : store->amount), get_obj_name_by_proto(store->vnum), lbuf);
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_adventure) {
	struct instance_data *inst;
	
	if (!(inst = find_instance_by_room(IN_ROOM(ch)))) {
		msg_to_char(ch, "You are not in or near an adventure zone.\r\n");
		return;
	}

	msg_to_char(ch, "%s (%s)\r\n", NULLSAFE(GET_ADV_NAME(inst->adventure)), instance_level_string(inst));
	
	if (GET_ADV_AUTHOR(inst->adventure) && *(GET_ADV_AUTHOR(inst->adventure))) {
		msg_to_char(ch, "by %s\r\n", GET_ADV_AUTHOR(inst->adventure));
	}
	msg_to_char(ch, "%s", NULLSAFE(GET_ADV_DESCRIPTION(inst->adventure)));
	if (GET_ADV_PLAYER_LIMIT(inst->adventure) > 0) {
		msg_to_char(ch, "Player limit: %d\r\n", GET_ADV_PLAYER_LIMIT(inst->adventure));
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
	else if (HAS_ABILITY(ch, ABIL_RIDE) && GET_MOUNT_VNUM(ch) != NOTHING && mob_proto(GET_MOUNT_VNUM(ch))) {
		msg_to_char(ch, "   You have %s. Type 'mount' to ride it.\r\n", get_mob_name_by_proto(GET_MOUNT_VNUM(ch)));
	}

	/* Morph */
	if (GET_MORPH(ch) != MORPH_NONE) {
		msg_to_char(ch, "   You are in the form of %s!\r\n", morph_string(ch, MORPH_STRING_NAME));
	}
	else if (IS_DISGUISED(ch)) {
		msg_to_char(ch, "   You are disguised as %s!\r\n", PERS(ch, ch, 0));
	}

	show_character_affects(ch, ch);
}


ACMD(do_coins) {
	if (!IS_NPC(ch)) {
		coin_string(GET_PLAYER_COINS(ch), buf);
		msg_to_char(ch, "You have %s.\r\n", buf);
	}
	else {
		msg_to_char(ch, "NPCs don't carry coins.\r\n");
	}
}


ACMD(do_cooldowns) {
	extern const char *cooldown_types[];
	
	struct cooldown_data *cool;
	int diff;
	bool found = FALSE;
	
	msg_to_char(ch, "Abilities on cooldown:\r\n");
	
	for (cool = ch->cooldowns; cool; cool = cool->next) {
		// only show if not expired (in case it wasn't cleaned up yet due to close timing)
		diff = cool->expire_time - time(0);
		if (diff > 0) {
			sprinttype(cool->type, cooldown_types, buf);
			msg_to_char(ch, " &c%s&0 %d:%02d\r\n", buf, (diff / 60), (diff % 60));

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
		msg_to_char(ch, "%s\r\n", replace_prompt_codes(ch, argument));
	}
}


ACMD(do_equipment) {
	int i;
	bool all = FALSE, found = FALSE;
	
	one_argument(argument, arg);
	
	if (*arg && (!str_cmp(arg, "all") || !str_cmp(arg, "-all") || !str_cmp(arg, "-a"))) {
		all = TRUE;
	}
	
	if (!IS_NPC(ch)) {
		msg_to_char(ch, "You are using (gear level %d):\r\n", (int) GET_GEAR_LEVEL(ch));
	}
	else {
		send_to_char("You are using:\r\n", ch);
	}
	
	for (i = 0; i < NUM_WEARS; i++) {
		if (GET_EQ(ch, i)) {
			if (CAN_SEE_OBJ(ch, GET_EQ(ch, i))) {
				send_to_char(wear_data[i].eq_prompt, ch);
				show_obj_to_char(GET_EQ(ch, i), ch, OBJ_DESC_EQUIPMENT);
				found = TRUE;
			}
			else {
				send_to_char(wear_data[i].eq_prompt, ch);
				send_to_char("Something.\r\n", ch);
				found = TRUE;
			}
		}
		else if (all) {
			msg_to_char(ch, "%s\r\n", wear_data[i].eq_prompt);
			found = TRUE;
		}
	}
	if (!found) {
		send_to_char(" Nothing.\r\n", ch);
	}
}


ACMD(do_examine) {
	char_data *tmp_char;
	obj_data *tmp_object;

	one_argument(argument, arg);

	if (!*arg) {
		send_to_char("Examine what?\r\n", ch);
		return;
	}
	look_at_target(ch, arg);

	generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_CHAR_ROOM | FIND_OBJ_EQUIP, ch, &tmp_char, &tmp_object);

	if (tmp_object) {
		if ((GET_OBJ_TYPE(tmp_object) == ITEM_DRINKCON) || (GET_OBJ_TYPE(tmp_object) == ITEM_CONTAINER) || (GET_OBJ_TYPE(tmp_object) == ITEM_CORPSE) || (GET_OBJ_TYPE(tmp_object) == ITEM_CART)) {
			send_to_char("When you look inside, you see:\r\n", ch);
			look_in_obj(ch, arg);
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
	int iter;
	bool found;
	size_t size;
	
	// this removes leading filler words, which are going to show up in a lot of helps
	one_argument(argument, arg);
	
	if (!ch->desc) {
		// don't bother
		return;
	}
	else if (!*arg) {
		msg_to_char(ch, "Search for help about what?\r\n");
	}
	else if (!help_table) {
		msg_to_char(ch, "No help available.r\n");
	}
	else {
		size = snprintf(output, sizeof(output), "You find help on that in the following help entries:\r\n");
		found = FALSE;
		
		for (iter = 0; iter <= top_of_helpt; ++iter) {
			if (GET_ACCESS_LEVEL(ch) >= help_table[iter].level && (!help_table[iter].duplicate && str_str(help_table[iter].entry, arg))) {
				snprintf(line, sizeof(line), " %s\r\n", help_table[iter].keyword);
				found = TRUE;
				
				if (size + strlen(line) < sizeof(output)) {
					strcat(output, line);
					size += strlen(line);
				}
				else {
					size += snprintf(output + size, sizeof(output) - size, "... and more\r\n");
					break;
				}
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
}


ACMD(do_inventory) {
	empire_data *ch_emp, *room_emp;
	
	if (!IS_NPC(ch)) {
		do_coins(ch, "", 0, 0);
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


ACMD(do_look) {
	void look_in_direction(char_data *ch, int dir);
	
	char arg2[MAX_INPUT_LENGTH];
	int look_type;

	if (!ch->desc)
		return;

	if (GET_POS(ch) < POS_SLEEPING)
		send_to_char("You can't see anything but stars!\r\n", ch);
	else if (AFF_FLAGGED(ch, AFF_BLIND))
		send_to_char("You can't see a damned thing, you're blind!\r\n", ch);
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch)) && ROOM_IS_CLOSED(IN_ROOM(ch))) {
		send_to_char("It is pitch black...\r\n", ch);
		list_char_to_char(ROOM_PEOPLE(IN_ROOM(ch)), ch);	/* glowing red eyes */
	}
	else {
		half_chop(argument, arg, arg2);

		if (!*arg)			/* "look" alone, without an argument at all */
			look_at_room(ch);
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


ACMD(do_mapsize) {	
	int size, cur;
	
	// NOTE: player picks the total size, but we store it as distance
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		return;
	}
	
	cur = GET_MAPSIZE(ch);
	if (cur == 0) {
		cur = config_get_int("default_map_size");
	}
	
	if (!*argument) {
		msg_to_char(ch, "Current map size: %d\r\n", cur * 2 + 1);
	}
	else if ((size = atoi(argument)) < 3 || size > (config_get_int("max_map_size") * 2 + 1)) {
		msg_to_char(ch, "You must choose a size between 3 and %d.\r\n", config_get_int("max_map_size") * 2 + 1);
	}
	else {
		GET_MAPSIZE(ch) = size/2;
		msg_to_char(ch, "Your map size is now %d.\r\n", GET_MAPSIZE(ch) * 2 + 1);
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
				
				if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
					msg_to_char(ch, "Your mark at (%d, %d) is %d map tile%s %s.\r\n", X_COORD(mark), Y_COORD(mark), dist, (dist == 1 ? "" : "s"), (dir == NO_DIR ? "away" : dirs[dir]));
				}
				else {
					msg_to_char(ch, "Your mark is %d map tile%s %s.\r\n", dist, (dist == 1 ? "" : "s"), (dir == NO_DIR ? "away" : dirs[dir]));
				}
			}
		}
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
	extern int num_of_start_locs;
	extern int *start_locs;
	
	int max_dist = 50;
	
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	struct instance_data *inst;
	struct empire_city_data *city;
	empire_data *emp, *next_emp;
	int iter, dist, dir, size;
	bool found = FALSE;
	room_data *loc;
	
	if (!ch->desc) {
		return;
	}
	
	size = snprintf(buf, sizeof(buf), "You find nearby:\r\n");

	// check starting locations
	for (iter = 0; iter <= num_of_start_locs; ++iter) {
		loc = real_room(start_locs[iter]);
		dist = compute_distance(IN_ROOM(ch), loc);
		
		if (dist <= max_dist) {
			found = TRUE;

			dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), loc));
			
			if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
				snprintf(line, sizeof(line), " %d tile%s %s: %s (%d, %d)\r\n", dist, (dist != 1 ? "s" : ""), (dir == NO_DIR ? "away" : dirs[dir]), get_room_name(loc, FALSE), X_COORD(loc), Y_COORD(loc));
			}
			else {
				snprintf(line, sizeof(line), " %d tile%s %s: %s\r\n", dist, (dist != 1 ? "s" : ""), (dir == NO_DIR ? "away" : dirs[dir]), get_room_name(loc, FALSE));
			}
			
			size += snprintf(buf + size, sizeof(buf) - size, "%s", line);
		}
	}
	
	// check cities
	HASH_ITER(hh, empire_table, emp, next_emp) {
		for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
			loc = city->location;
			dist = compute_distance(IN_ROOM(ch), loc);

			if (dist <= max_dist) {
				found = TRUE;
				
				dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), loc));

				if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
					snprintf(line, sizeof(line), " %d tile%s %s: the %s of %s (%d, %d) / %s%s&0\r\n", dist, (dist != 1 ? "s" : ""), (dir == NO_DIR ? "away" : dirs[dir]), city_type[city->type].name, city->name, X_COORD(loc), Y_COORD(loc), EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
				}
				else {
					snprintf(line, sizeof(line), " %d tile%s %s: the %s of %s / %s%s&0\r\n", dist, (dist != 1 ? "s" : ""), (dir == NO_DIR ? "away" : dirs[dir]), city_type[city->type].name, city->name, EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
				}
				
				size += snprintf(buf + size, sizeof(buf) - size, "%s", line);
			}
		}
	}
	
	// check instances
	for (inst = instance_list; inst; inst = inst->next) {
		if (!inst->location || INSTANCE_FLAGGED(inst, INST_COMPLETED)) {
			continue;
		}
		
		if (ADVENTURE_FLAGGED(inst->adventure, ADV_NO_NEARBY)) {
			continue;
		}
		
		loc = inst->location;
		if (!loc || (dist = compute_distance(IN_ROOM(ch), loc)) > max_dist) {
			continue;
		}
		
		// show instance
		found = TRUE;
		dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), inst->location));
		if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
			snprintf(line, sizeof(line), " %d tile%s %s: %s (%d, %d) / %s\r\n", dist, PLURAL(dist), (dir == NO_DIR ? "away" : dirs[dir]), GET_ADV_NAME(inst->adventure), X_COORD(loc), Y_COORD(loc), instance_level_string(inst));
		}
		else {
			snprintf(line, sizeof(line), " %d tile%s %s: %s / %s\r\n", dist, PLURAL(dist), (dir == NO_DIR ? "away" : dirs[dir]), GET_ADV_NAME(inst->adventure), instance_level_string(inst));
		}
		
		size += snprintf(buf + size, sizeof(buf) - size, "%s", line);
	}
	
	if (!found) {
		size += snprintf(buf + size, sizeof(buf) - size, " nothing\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


ACMD(do_score) {
	char_data *victim;

	if (REAL_NPC(ch))
		return;

	one_argument(argument, arg);

	if (IS_IMMORTAL(ch) && *arg) {
		if (!(victim = get_char_vis(ch, arg, FIND_CHAR_WORLD)))
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
	
	msg_to_char(ch, "You survey the area:\r\n");
	
	if (GET_ISLAND_ID(IN_ROOM(ch)) != NO_ISLAND) {
		msg_to_char(ch, "Location: %s\r\n", get_island(GET_ISLAND_ID(IN_ROOM(ch)), TRUE)->name);
	}
	
	// empire
	if (ROOM_OWNER(IN_ROOM(ch))) {
		if ((city = find_city(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch)))) {
			msg_to_char(ch, "This is the %s%s&0 %s of %s.\r\n", EMPIRE_BANNER(ROOM_OWNER(IN_ROOM(ch))), EMPIRE_ADJECTIVE(ROOM_OWNER(IN_ROOM(ch))), city_type[city->type].name, city->name);
		}	
		else {
			msg_to_char(ch, "This area is claimed by %s%s&0.\r\n", EMPIRE_BANNER(ROOM_OWNER(IN_ROOM(ch))), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))));
		}
	}
	
	// building info
	if (COMPLEX_DATA(IN_ROOM(ch))) {
		if (BUILDING_DISREPAIR(IN_ROOM(ch)) > config_get_int("disrepair_minor")) {
			msg_to_char(ch, "It's in need of %s repair.\r\n", (BUILDING_DISREPAIR(IN_ROOM(ch)) > config_get_int("disrepair_major")) ? "major" : "some");
		}
		if (BUILDING_DAMAGE(IN_ROOM(ch)) > 0) {
			msg_to_char(ch, "It has been damaged in a siege.\r\n");
		}
		if (BUILDING_BURNING(IN_ROOM(ch)) > 0) {
			msg_to_char(ch, "It's on fire!\r\n");
		}
	}
	
	// adventure info
	if (find_instance_by_room(IN_ROOM(ch))) {
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

	if (IS_OUTDOORS(ch)) {
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
	if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
		msg_to_char(ch, "You are at: %s (%d, %d)\r\n", get_room_name(IN_ROOM(ch), FALSE), X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
	}
	else {
		msg_to_char(ch, "You are at: %s\r\n", get_room_name(IN_ROOM(ch), FALSE));
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


ACMD(do_whois) {
	void read_lore(char_data *ch);
	extern const char *class_role[NUM_ROLES];
	extern const char *level_names[][2];
	
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

	// ready
	if (file) {
		read_lore(victim);
	}
	
	// basic info
	msg_to_char(ch, "%s%s&0\r\n", PERS(victim, victim, TRUE), GET_TITLE(victim));
	msg_to_char(ch, "Status: %s\r\n", level_names[(int) GET_ACCESS_LEVEL(victim)][1]);

	// show class (but don't bother for immortals, as they generally have all skills
	if (!IS_GOD(victim) && !IS_IMMORTAL(victim)) {
		level = file ? GET_LAST_KNOWN_LEVEL(victim) : GET_COMPUTED_LEVEL(victim);
		
		if (GET_CLASS(victim) != CLASS_NONE) {
			msg_to_char(ch, "Class: %d %s (%s)\r\n", level, class_data[GET_PC_CLASS(victim)].name, class_role[(int) GET_CLASS_ROLE(victim)]);
		}
		else {
			msg_to_char(ch, "Level: %d\r\n", level);
		}
	}

	if (GET_LOYALTY(victim)) {
		msg_to_char(ch, "%s&0 of %s%s&0\r\n", EMPIRE_RANK(GET_LOYALTY(victim), GET_RANK(victim)-1), EMPIRE_BANNER(GET_LOYALTY(victim)), EMPIRE_NAME(GET_LOYALTY(victim)));
		msg_to_char(ch, "Territory: %d, Members: %d, Greatness: %d\r\n", EMPIRE_CITY_TERRITORY(GET_LOYALTY(victim)) + EMPIRE_OUTSIDE_TERRITORY(GET_LOYALTY(victim)), EMPIRE_MEMBERS(GET_LOYALTY(victim)), EMPIRE_GREATNESS(GET_LOYALTY(victim)));
	}

	if (GET_LORE(victim)) {
		list_lore_to_char(victim, ch);
	}

	// cleanup	
	if (victim && file) {
		free_char(victim);
	}
}
