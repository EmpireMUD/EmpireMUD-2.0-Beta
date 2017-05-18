/* ************************************************************************
*   File: act.stealth.c                                   EmpireMUD 2.0b5 *
*  Usage: code related to non-offensive skills and abilities              *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <math.h>

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Helpers
*   Poisons
*   Commands
*/

// external vars
extern const int rev_dir[];

// external funcs
void check_combat_start(char_data *ch);
extern bool is_fight_ally(char_data *ch, char_data *frenemy);	// fight.c
void scale_item_to_level(obj_data *obj, int level);

// locals
int apply_poison(char_data *ch, char_data *vict, int type);
obj_data *find_poison_by_type(obj_data *list, int type);
void trigger_distrust_from_stealth(char_data *ch, empire_data *emp);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Determine if ch can infiltrate emp -- sends its own messages
*
* @param char_data *ch
* @param empire_data *emp an opposing empire
* @return bool TRUE if ch is allowed to infiltrate emp
*/
bool can_infiltrate(char_data *ch, empire_data *emp) {
	struct empire_political_data *pol;
	empire_data *chemp = GET_LOYALTY(ch);
	
	// no empire = no problem
	if (!emp) {
		return TRUE;
	}
	
	if (emp == chemp) {
		msg_to_char(ch, "You can't infiltrate your own empire!\r\n");
		return FALSE;
	}
	
	if (EMPIRE_IMM_ONLY(emp)) {
		msg_to_char(ch, "You cannot infiltrate immortal empires.\r\n");
		return FALSE;
	}
	
	if (count_members_online(emp) == 0) {
		msg_to_char(ch, "There are no members of %s online.\r\n", EMPIRE_NAME(emp));
		return FALSE;
	}
	
	if (!PRF_FLAGGED(ch, PRF_STEALTHABLE)) {
		msg_to_char(ch, "You cannot infiltrate because your 'stealthable' toggle is off.\r\n");
		return FALSE;
	}
	
	// further checks only matter if ch is in an empire
	if (!chemp) {
		return TRUE;
	}
	
	pol = find_relation(chemp, emp);
	
	if (pol && IS_SET(pol->type, DIPL_ALLIED | DIPL_NONAGGR)) {
		msg_to_char(ch, "You can't infiltrate -- you have a non-aggression pact with %s.\r\n", EMPIRE_NAME(emp));
		return FALSE;
	}
	
	if (GET_RANK(ch) < EMPIRE_PRIV(chemp, PRIV_STEALTH) && (!pol || !IS_SET(pol->type, DIPL_WAR))) {
		msg_to_char(ch, "You don't have permission from your empire to infiltrate others. Are you trying to start a war?\r\n");
		return FALSE;
	}
	
	// got this far...
	return TRUE;
}


/**
* NOTE: Sometimes this sends a message itself, but never with a crlf; it
* expects that you are going to send an error message of your own, and is
* prefacing it with, e.g. "You have a treaty with Empire Name. "
*
* @param char_data *ch
* @param empire_data *emp
* @return TRUE if ch is capable of stealing from emp
*/
bool can_steal(char_data *ch, empire_data *emp) {	
	struct empire_political_data *pol;
	empire_data *chemp = GET_LOYALTY(ch);
	
	// no empire = ok
	if (!emp) {
		return TRUE;
	}
	
	if (!has_ability(ch, ABIL_STEAL)) {
		msg_to_char(ch, "You don't have the Steal ability.\r\n");
		return FALSE;
	}
	
	if (!PRF_FLAGGED(ch, PRF_STEALTHABLE)) {
		msg_to_char(ch, "You cannot steal because your 'stealthable' toggle is off.\r\n");
		return FALSE;
	}
	
	if (EMPIRE_IMM_ONLY(emp)) {
		msg_to_char(ch, "You can't steal from immortal empires.\r\n");
		return FALSE;
	}
	
	if (emp == chemp) {
		msg_to_char(ch, "You can't steal from your own empire.\r\n");
		return FALSE;
	}
	
	if (count_members_online(emp) == 0) {
		msg_to_char(ch, "There are no members of %s online.\r\n", EMPIRE_NAME(emp));
		return FALSE;
	}
	
	// further checks don't matter if ch is in no empire
	if (!chemp) {
		return TRUE;
	}
	
	pol = find_relation(chemp, emp);
	
	if (pol && IS_SET(pol->type, DIPL_ALLIED | DIPL_NONAGGR)) {
		msg_to_char(ch, "You have a treaty with %s.\r\n", EMPIRE_NAME(emp));
		return FALSE;
	}
	
	if (GET_RANK(ch) < EMPIRE_PRIV(chemp, PRIV_STEALTH) && (!pol || !IS_SET(pol->type, DIPL_WAR))) {
		msg_to_char(ch, "You don't have permission from your empire to steal from others. Are you trying to start a war?\r\n");
		return FALSE;
	}
	
	// got this far...
	return TRUE;
}


INTERACTION_FUNC(pickpocket_interact) {
	int iter;
	obj_data *obj = NULL;
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, get_approximate_level(inter_mob));
		obj_to_char(obj, ch);
		act("You find $p!", FALSE, ch, obj, NULL, TO_CHAR);
		load_otrigger(obj);
	}
		
	return TRUE;
}


// for do_escape
void perform_escape(char_data *ch) {	
	room_data *home, *to_room = NULL;
	
	// on a boat?
	if (GET_ROOM_VEHICLE(IN_ROOM(ch)) && (to_room = IN_VEHICLE_IN_ROOM(IN_ROOM(ch))) != IN_ROOM(ch)) {
		if (VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(ch)), VEH_IN)) {
			act("You dive out of $V!", FALSE, ch, NULL, GET_ROOM_VEHICLE(IN_ROOM(ch)), TO_CHAR);
			act("$n dives out of $V!", TRUE, ch, NULL, GET_ROOM_VEHICLE(IN_ROOM(ch)), TO_ROOM);
		}
		else {	// not an IN vehicle
			msg_to_char(ch, "You dive off the side!\r\n");
			act("$n dives off the side!", TRUE, ch, NULL, NULL, TO_ROOM);
		}
	}
	else {
		msg_to_char(ch, "You dive out the window!\r\n");
		act("$n dives out the window!", TRUE, ch, NULL, NULL, TO_ROOM);
		
		home = HOME_ROOM(IN_ROOM(ch));
		if (BUILDING_ENTRANCE(home) != NO_DIR) {
			to_room = real_shift(home, shift_dir[rev_dir[BUILDING_ENTRANCE(home)]][0], shift_dir[rev_dir[BUILDING_ENTRANCE(home)]][1]);
		}
	}

	if (!to_room) {
		msg_to_char(ch, "But you can't seem to escape from here...\r\n");
	}
	else {
		char_to_room(ch, to_room);
		qt_visit_room(ch, IN_ROOM(ch));
		look_at_room(ch);
		
		GET_LAST_DIR(ch) = NO_DIR;
		
		enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
		entry_memory_mtrigger(ch);
		greet_mtrigger(ch, NO_DIR);
		greet_memory_mtrigger(ch);
		greet_vtrigger(ch, NO_DIR);
		
		act("$n dives out a window and lands before you!", TRUE, ch, NULL, NULL, TO_ROOM);
	}
	
	GET_ACTION(ch) = ACT_NONE;
	gain_ability_exp(ch, ABIL_ESCAPE, 50);
}


/**
* This marks the player hostile and may lead to empire distrust.
*
* @param char_data *ch The player.
* @param empire_data *emp The empire the player has angered.
*/
void trigger_distrust_from_stealth(char_data *ch, empire_data *emp) {
	struct empire_political_data *pol;
	empire_data *chemp = GET_LOYALTY(ch);
	
	if (!emp || EMPIRE_IMM_ONLY(emp) || IS_IMMORTAL(ch) || GET_LOYALTY(ch) == emp) {
		return;
	}
	
	add_cooldown(ch, COOLDOWN_HOSTILE_FLAG, config_get_int("hostile_flag_time") * SECS_PER_REAL_MIN);
	
	// no player empire? mark rogue and done
	if (!chemp) {
		add_cooldown(ch, COOLDOWN_ROGUE_FLAG, config_get_int("rogue_flag_time") * SECS_PER_REAL_MIN);
		return;
	}
	
	// check chemp->emp politics
	if (!(pol = find_relation(chemp, emp))) {
		pol = create_relation(chemp, emp);
	}	
	if (!IS_SET(pol->type, DIPL_WAR | DIPL_DISTRUST)) {
		pol->offer = 0;
		pol->type = DIPL_DISTRUST;
		
		log_to_empire(chemp, ELOG_DIPLOMACY, "%s now distrusts this empire due to stealth activity (%s)", EMPIRE_NAME(emp), PERS(ch, ch, TRUE));
		EMPIRE_NEEDS_SAVE(chemp) = TRUE;
	}
	
	// check emp->chemp politics
	if (!(pol = find_relation(emp, chemp))) {
		pol = create_relation(emp, chemp);
	}	
	if (!IS_SET(pol->type, DIPL_WAR | DIPL_DISTRUST)) {
		pol->offer = 0;
		pol->type = DIPL_DISTRUST;
		
		log_to_empire(emp, ELOG_DIPLOMACY, "This empire now officially distrusts %s due to stealth activity", EMPIRE_NAME(chemp));
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
	
	// spawn guards?
}


// removes the disguise
void undisguise(char_data *ch) {
	char lbuf[MAX_INPUT_LENGTH];
	
	if (IS_DISGUISED(ch)) {
		sprintf(lbuf, "%s takes off $s disguise and is revealed as $n!", PERS(ch, ch, 0));
		*lbuf = UPPER(*lbuf);
		
		REMOVE_BIT(PLR_FLAGS(ch), PLR_DISGUISED);
		
		msg_to_char(ch, "You take off your disguise.\r\n");
		act(lbuf, TRUE, ch, NULL, NULL, TO_ROOM);
	}
}


/**
* Determines if a room qualifies for Unseen Passing (indoors/in-city).
*
* @param room_data *room Where to check.
* @return bool TRUE if Unseen Passing works here.
*/
bool valid_unseen_passing(room_data *room) {
	if (IS_ADVENTURE_ROOM(room)) {
		return FALSE;	// adventures do not trigger this ability
	}
	if (ROOM_OWNER(room) && find_city(ROOM_OWNER(room), room)) {
		return TRUE;	// IS inside a city (even if wilderness)
	}
	if (IS_OUTDOOR_TILE(room) && !IS_ROAD(room) && !IS_ANY_BUILDING(room)) {
		return FALSE;	// outdoors; this goes after the city check
	}
	
	// all other cases?
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// POISONS /////////////////////////////////////////////////////////////////

#define POISONS_LINE_BREAK  { "*", NO_ABIL,  0, 0, 0, 0, 0, 0, 0, 0, 0,	0, FALSE }

#define SPEC_NONE  0
#define SPEC_SEARING  1

// master potion data
const struct poison_data_type poison_data[] = {

	// WARNING: DO NOT CHANGE THE ORDER, ONLY ADD TO THE END
	// The position in this array corresponds to obj val 0

	{	// 0
		"searing", ABIL_POISONS,
		0, 0, 0, 0,	// no affect
		0, 0, 0, 0, 0,	// no dot
		SPEC_SEARING, FALSE
	},

	{	// 1
		"weakness", ABIL_POISONS,
		ATYPE_POISON, APPLY_HEALTH, -25, 0,
		0, 0, 0, 0, 0,	// no dot
		SPEC_NONE, FALSE
	},

	{	// 2
		"exhaustion", ABIL_POISONS,
		ATYPE_POISON, APPLY_MOVE, -25, 0,
		0, 0, 0, 0, 0,	// no dot
		SPEC_NONE, FALSE
	},

	{	// 3
		"braindrain", ABIL_POISONS,
		ATYPE_POISON, APPLY_MANA, -25, 0,
		0, 0, 0, 0, 0,	// no dot
		SPEC_NONE, FALSE
	},
	
	POISONS_LINE_BREAK,	// 4
	
	{	// 5
		"strength", ABIL_POISONS,
		ATYPE_POISON, APPLY_STRENGTH, -1, 0,
		0, 0, 0, 0, 0,	// no dot
		SPEC_NONE, FALSE
	},
	
	{	// 6
		"dexterity", ABIL_POISONS,
		ATYPE_POISON, APPLY_DEXTERITY, -1, 0,
		0, 0, 0, 0, 0,	// no dot
		SPEC_NONE, FALSE
	},
	
	{	// 7
		"charisma", ABIL_POISONS,
		ATYPE_POISON, APPLY_CHARISMA, -1, 0,
		0, 0, 0, 0, 0,	// no dot
		SPEC_NONE, FALSE
	},
	
	{	// 8
		"intelligence", ABIL_POISONS,
		ATYPE_POISON, APPLY_INTELLIGENCE, -1, 0,
		0, 0, 0, 0, 0,	// no dot
		SPEC_NONE, FALSE
	},
	
	{	// 9
		"wits", ABIL_POISONS,
		ATYPE_POISON, APPLY_WITS, -2, 0,
		0, 0, 0, 0, 0,	// no dot
		SPEC_NONE, FALSE
	},
	
	POISONS_LINE_BREAK,	// 10
	
	{	// 11
		"pain", ABIL_DEADLY_POISONS,
		0, 0, 0, 0,
		ATYPE_POISON, 5, DAM_POISON, 2, 5,	// no dot
		SPEC_NONE, TRUE	// likely doubled due to deadly poisons
	},
	
	{	// 12
		"slow", ABIL_DEADLY_POISONS,
		ATYPE_POISON, APPLY_NONE, 0, AFF_SLOW,
		0, 0, 0, 0, 0,	// no dot
		SPEC_NONE, FALSE
	},
	
	{	// 13
		"heartstop", ABIL_DEADLY_POISONS,
		ATYPE_POISON, APPLY_NONE, 0, AFF_CANT_SPEND_BLOOD,
		0, 0, 0, 0, 0,	// no dot
		SPEC_NONE, FALSE
	},

	// END
	{
		"\n", NO_ABIL,
		0, 0, 0, 0,
		0, 0, 0, 0, 0,	// no dot
		SPEC_NONE, FALSE
	}
};


/**
* finds a matching poison object in an object list
*
* @param obj_data *list ch->carrying, e.g.
* @param int type any poison_data pos
* @return obj_data *the poison, if found
*/
obj_data *find_poison_by_type(obj_data *list, int type) {
	obj_data *obj, *found = NULL;
	
	for (obj = list; obj && !found; obj = obj->next_content) {
		if (GET_POISON_TYPE(obj) == type) {
			found = obj;
		}
	}
	
	return found;
}


/**
* Apply the actual effects of a poison. This also makes sure there's some
* available, and manages the object.
*
* @param char_data *ch the person being poisoned
* @param int type poison_data[] pos
* @return -1 if poison killed the person, 0 if no hit at all, >0 if hit
*/
int apply_poison(char_data *ch, char_data *vict, int type) {
	obj_data *obj;
	struct affected_type *af;
	bool messaged = FALSE;
	int result = 0;
	
	if (IS_NPC(ch) || type < 0) {
		return 0;
	}
	
	// has poison?
	if (!(obj = find_poison_by_type(ch->carrying, type))) {
		return 0;
	}
	
	// ability check
	if (poison_data[type].ability != NO_ABIL && !has_ability(ch, poison_data[type].ability)) {
		return 0;
	}

	// stack check?
	if (poison_data[type].atype > 0 && !poison_data[type].allow_stack && affected_by_spell_and_apply(vict, poison_data[type].atype, poison_data[type].apply)) {
		return 0;
	}
	
	if (AFF_FLAGGED(vict, AFF_IMMUNE_STEALTH)) {
		return 0;
	}
	
	if (IS_GOD(vict) || IS_IMMORTAL(vict)) {
		return 0;
	}
	
	// GAIN SKILL NOW -- it at least attempts an application
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, ABIL_POISONS, 2);
		gain_ability_exp(ch, ABIL_DEADLY_POISONS, 2);
	}
	
	// skill check!
	if (!skill_check(ch, ABIL_POISONS, DIFF_HARD)) {
		return 0;
	}
	
	// applied -- charge a charge
	GET_OBJ_VAL(obj, VAL_POISON_CHARGES) -= 1;
	
	if (GET_POISON_CHARGES(obj) <= 0) {
		extract_obj(obj);
	}
	
	// attempt immunity/resist
	if (has_ability(vict, ABIL_POISON_IMMUNITY)) {
		if (can_gain_exp_from(vict, ch)) {
			gain_ability_exp(vict, ABIL_POISON_IMMUNITY, 10);
		}
		return 0;
	}
	if (has_ability(vict, ABIL_RESIST_POISON)) {
		if (can_gain_exp_from(vict, ch)) {
			gain_ability_exp(vict, ABIL_RESIST_POISON, 10);
		}
		if (!number(0, 2)) {
			return 0;
		}
	}

	// atype
	if (poison_data[type].atype > 0) {
		
		af = create_aff(poison_data[type].atype, 2 MUD_HOURS, poison_data[type].apply, poison_data[type].mod * (has_ability(ch, ABIL_DEADLY_POISONS) ? 2 : 1), poison_data[type].aff, ch);
		affect_join(vict, af, poison_data[type].allow_stack ? (AVG_DURATION|ADD_MODIFIER) : 0);
		
		if (!messaged) {
			act("You feel ill as you are poisoned!", FALSE, vict, NULL, NULL, TO_CHAR);
			act("$n looks ill as $e is poisoned!", FALSE, vict, NULL, NULL, TO_ROOM);
			messaged = TRUE;
		}
		
		result = 1;
	}
	
	// dot
	if (poison_data[type].dot_type > 0) {
		apply_dot_effect(vict, poison_data[type].dot_type, poison_data[type].dot_duration, poison_data[type].dot_damage_type, poison_data[type].dot_damage * (has_ability(ch, ABIL_DEADLY_POISONS) ? 2 : 1), poison_data[type].dot_max_stacks, ch);
		
		if (!messaged) {
			act("You feel ill as you are poisoned!", FALSE, vict, NULL, NULL, TO_CHAR);
			act("$n looks ill as $e is poisoned!", FALSE, vict, NULL, NULL, TO_ROOM);
			messaged = TRUE;
		}
		
		result = 1;
	}
	
	// special cases
	switch (poison_data[type].special) {
		case SPEC_SEARING: {
			result = damage(ch, vict, 5 * (has_ability(ch, ABIL_DEADLY_POISONS) ? 2 : 1), ATTACK_POISON, DAM_POISON);
			break;
		}
	}

	if (result >= 0 && GET_HEALTH(vict) > GET_MAX_HEALTH(vict)) {
		GET_HEALTH(vict) = GET_MAX_HEALTH(vict);
	}
	
	return result;
}


/**
* For the "use" command -- switches your poison.
*
* @param char_data *ch
* @param obj_data *obj the poison
*/
void use_poison(char_data *ch, obj_data *obj) {
	int type = GET_POISON_TYPE(obj);
	
	if (!IS_POISON(obj)) {
		// ??? shouldn't ever get here
		act("$p isn't even a poison.", FALSE, ch, obj, NULL, TO_CHAR);
		return;
	}
	
	if (poison_data[type].ability != NO_ABIL && !has_ability(ch, poison_data[type].ability)) {
		msg_to_char(ch, "You don't have the correct ability to use that poison.\r\n");
		return;
	}
	
	USING_POISON(ch) = type;
	act("You are now using $p as your poison.", FALSE, ch, obj, NULL, TO_CHAR);
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////


ACMD(do_backstab) {
	char_data *vict;
	int dam, cost = 15;
	bool success = FALSE, fighting_me = FALSE;
	
	for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict && !fighting_me; vict = vict->next_in_room) {
		if (FIGHTING(vict) == ch) {
			fighting_me = TRUE;
		}
	}

	one_argument(argument, arg);

	if (!can_use_ability(ch, ABIL_BACKSTAB, MOVE, cost, COOLDOWN_BACKSTAB)) {
		// sends own messages
	}
	else if (!IS_NPC(ch) && !GET_EQ(ch, WEAR_WIELD)) {
		send_to_char("You need to wield a weapon to make it a success.\r\n", ch);
	}
	else if (!IS_NPC(ch) && GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_STAB) {
		send_to_char("You must use a stabbing weapon to backstab.\r\n", ch);
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM)) && !(vict = FIGHTING(ch))) {
		send_to_char("Backstab whom?\r\n", ch);
	}
	else if (FIGHTING(vict) == ch) {
		act("It's hard to get at $N's back when $E is attacking you.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (fighting_me) {
		msg_to_char(ch, "You can't backstab while someone is attacking you.\r\n");
	}
	else if (vict == ch) {
		send_to_char("You can't backstab yourself!\r\n", ch);
	}
	else if (FIGHT_MODE(vict) == FMODE_MISSILE || FIGHT_MODE(ch) == FMODE_MISSILE) {
		msg_to_char(ch, "You aren't close enough.\r\n");
	}
	else if (!can_fight(ch, vict)) {
		act("You can't attack $N!", FALSE, ch, 0, vict, TO_CHAR);
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_BACKSTAB)) {
		return;
	}
	else {
		charge_ability_cost(ch, MOVE, cost, COOLDOWN_BACKSTAB, 9, WAIT_COMBAT_ABILITY);
	
		// start meters now, to track direct damage()
		check_combat_start(ch);
		check_combat_start(vict);

		success = !AWAKE(vict) || !CAN_SEE(vict, ch) || skill_check(ch, ABIL_BACKSTAB, DIFF_EASY);

		if (!success) {
			damage(ch, vict, 0, ATTACK_BACKSTAB, DAM_PHYSICAL);
		}
		else {
			dam = GET_STRENGTH(ch) + (!IS_NPC(ch) ? GET_WEAPON_DAMAGE_BONUS(GET_EQ(ch, WEAR_WIELD)) : MOB_DAMAGE(ch));
			dam += GET_BONUS_PHYSICAL(ch);
			dam *= 2;
		
			// 2nd skill check for more damage
			if (skill_check(ch, ABIL_BACKSTAB, DIFF_MEDIUM)) {
				dam *= 2;
			}

			if (damage(ch, vict, dam, ATTACK_BACKSTAB, DAM_PHYSICAL) > 0) {
				if (has_ability(ch, ABIL_POISONS)) {
					if (!number(0, 1) && apply_poison(ch, vict, USING_POISON(ch)) < 0) {
						// dedz
					}
				}
			}		
		}
		
		if (can_gain_exp_from(ch, vict)) {
			gain_ability_exp(ch, ABIL_BACKSTAB, 15);
		}
	}
}


ACMD(do_blind) {
	struct affected_type *af;
	char_data *vict;
	int cost = 15;

	one_argument(argument, arg);

	if (!can_use_ability(ch, ABIL_BLIND, MOVE, cost, COOLDOWN_BLIND)) {
		// sends own messages
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM)) && !(vict = FIGHTING(ch))) {
		send_to_char("Blind whom?\r\n", ch);
	}
	else if (vict == ch) {
		send_to_char("You shouldn't blind yourself!\r\n", ch);
	}
	else if (FIGHT_MODE(vict) == FMODE_MISSILE || FIGHT_MODE(ch) == FMODE_MISSILE) {
		msg_to_char(ch, "You aren't close enough.\r\n");
	}
	else if (!can_fight(ch, vict)) {
		act("You can't attack $N!", FALSE, ch, 0, vict, TO_CHAR);
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_BLIND)) {
		return;
	}
	else {
		charge_ability_cost(ch, MOVE, cost, COOLDOWN_BLIND, 10, WAIT_COMBAT_ABILITY);

		if (skill_check(ch, ABIL_BLIND, DIFF_MEDIUM) && !AFF_FLAGGED(vict, AFF_IMMUNE_STEALTH)) {
			act("You toss a handful of sand into $N's eyes!", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n tosses a handful of sand into your eyes!\r\n&rYou're blind!&0", FALSE, ch, NULL, vict, TO_VICT);
			act("$n tosses a handful of sand into $N's eyes!", FALSE, ch, NULL, vict, TO_NOTVICT);
			
			af = create_flag_aff(ATYPE_BLIND, 2, AFF_BLIND, ch);
			affect_join(vict, af, 0);
		
			engage_combat(ch, vict, TRUE);
		}
		else {
			act("You toss a handful of sand at $N, but miss $S eyes!", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n tosses a handful of sand at you, but misses your eyes!", FALSE, ch, NULL, vict, TO_VICT);
			act("$n tosses a handful of sand but misses $N's eyes!", FALSE, ch, NULL, vict, TO_NOTVICT);
			
			engage_combat(ch, vict, TRUE);
		}
		
		if (can_gain_exp_from(ch, vict)) {
			gain_ability_exp(ch, ABIL_BLIND, 15);
		}
	}
}


ACMD(do_darkness) {
	struct affected_type *af;
	int cost = 15;

	if (!can_use_ability(ch, ABIL_DARKNESS, MOVE, cost, COOLDOWN_DARKNESS)) {
		// sends own message
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_DARKNESS)) {
		return;
	}
	else if (room_affected_by_spell(IN_ROOM(ch), ATYPE_DARKNESS)) {
		affect_from_room(IN_ROOM(ch), ATYPE_DARKNESS);
		
		msg_to_char(ch, "You wave your hand and the darkness dissipates.\r\n");
		act("$n waves $s hand and the darkness dissipates.", FALSE, ch, 0, 0, TO_ROOM);

		charge_ability_cost(ch, MOVE, cost, NOTHING, 0, WAIT_OTHER);
	}
	else {
		msg_to_char(ch, "You draw upon the shadows to blanket the area in an inky darkness!\r\n");
		act("Shadows seem to spread from $n, blanketing the area in an inky darkness!", FALSE, ch, 0, 0, TO_ROOM);

		CREATE(af, struct affected_type, 1);
		af->type = ATYPE_DARKNESS;
		af->cast_by = CAST_BY_ID(ch);
		af->duration = 1;
		af->modifier = 0;
		af->location = APPLY_NONE;
		af->bitvector = ROOM_AFF_DARK;

		affect_to_room(IN_ROOM(ch), af);
		free(af);	// affect_to_room duplicates affects
		gain_ability_exp(ch, ABIL_DARKNESS, 20);
		
		charge_ability_cost(ch, MOVE, cost, COOLDOWN_DARKNESS, 15, WAIT_ABILITY);
	}
}


ACMD(do_disguise) {
	char_data *vict;
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (IS_DISGUISED(ch)) {
		undisguise(ch);
		command_lag(ch, WAIT_OTHER);
	}
	else if (!can_use_ability(ch, ABIL_DISGUISE, NOTHING, 0, NOTHING)) {
		// sends own message
	}
	else if (IS_MORPHED(ch)) {
		msg_to_char(ch, "You can't disguise yourself while morphed.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Disguise yourself as whom?\r\n");
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (!IS_NPC(vict) && !IS_DISGUISED(vict)) {
		msg_to_char(ch, "You can only disguise yourself as an NPC.\r\n");
	}
	else if (IS_NPC(vict) && !MOB_FLAGGED(vict, MOB_HUMAN)) {
		act("You can't disguise yourself as $N!", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_DISGUISE)) {
		return;
	}
	else if (!skill_check(ch, ABIL_DISGUISE, DIFF_MEDIUM)) {
		msg_to_char(ch, "You try to disguise yourself, but fail.\r\n");
		act("$n tries to disguise $mself as $N, but fails.", FALSE, ch, NULL, vict, TO_ROOM);
		
		gain_ability_exp(ch, ABIL_DISGUISE, 33.4);
		command_lag(ch, WAIT_ABILITY);
	}
	else {
		act("You skillfully disguise yourself as $N!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n disguises $mself as $N!", TRUE, ch, NULL, vict, TO_ROOM);
		
		SET_BIT(PLR_FLAGS(ch), PLR_DISGUISED);
		
		// copy name and check limit
		if (GET_DISGUISED_NAME(ch)) {
			free(GET_DISGUISED_NAME(ch));
		}
		GET_DISGUISED_NAME(ch) = str_dup(PERS(vict, vict, FALSE));

		// copy the sex
		GET_DISGUISED_SEX(ch) = GET_SEX(vict);
		
		gain_ability_exp(ch, ABIL_DISGUISE, 33.4);
		GET_WAIT_STATE(ch) = 4 RL_SEC;	// long wait
	}
}


ACMD(do_diversion) {
	char_data *victim, *next_vict;
	struct affected_type *af;
	int value, cost = 50;
	bool first = TRUE;
	
	if (!can_use_ability(ch, ABIL_DIVERSION, MOVE, cost, COOLDOWN_DIVERSION)) {
		return;
	}
	else if (!check_solo_role(ch)) {
		msg_to_char(ch, "You must be alone to use that ability in the solo role.\r\n");
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_DIVERSION)) {
		return;
	}
	else {
		if (SHOULD_APPEAR(ch)) {
			appear(ch);
		}
		
		charge_ability_cost(ch, MOVE, cost, COOLDOWN_DIVERSION, 30, WAIT_COMBAT_ABILITY);
		
		// variable messaging
		switch (number(0, 2)) {
			case 0: {
				msg_to_char(ch, "You release a small flock of pigeons into the melee!\r\n");
				act("$n releases a small flock of pigeons into the melee!", FALSE, ch, NULL, NULL, TO_ROOM);
				break;
			}
			case 1: {
				msg_to_char(ch, "You toss some firecrackers into the melee!\r\n");
				act("$n tosses some firecrackers into the melee!", FALSE, ch, NULL, NULL, TO_ROOM);
				break;
			}
			case 2: {
				msg_to_char(ch, "You throw a pocketful of sparkling dust into the melee!\r\n");
				act("$n throws a pocketful of sparkling dust into the melee!", FALSE, ch, NULL, NULL, TO_ROOM);
				break;
			}
		}
		
		for (victim = ROOM_PEOPLE(IN_ROOM(ch)); victim; victim = next_vict) {
			next_vict = victim->next_in_room;
			
			if (AFF_FLAGGED(victim, AFF_IMMUNE_STEALTH)) {
				continue;
			}
			
			// TODO could macro this whole line since it's used in several AoEs
			if (victim != ch && IS_NPC(victim) && CAN_SEE(ch, victim) && !is_fight_ally(ch, victim) && !in_same_group(ch, victim) && can_fight(ch, victim)) {
				value = ceil(GET_CHARISMA(ch) * (first ? 1.0 : 0.5));
				first = FALSE;
				
				af = create_mod_aff(ATYPE_DIVERSION, 3, APPLY_WITS, -value, ch);
				affect_join(victim, af, NOBITS);
				
				msg_to_char(victim, "You can't seem to focus on the battle!\r\n");
				engage_combat(ch, victim, TRUE);
			}
		}
	}
}


ACMD(do_escape) {
	int cost = 10;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use escape.\r\n");
	}
	else if (!can_use_ability(ch, ABIL_ESCAPE, MOVE, cost, NOTHING)) {
		// sends own messages
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_ESCAPE)) {
		return;
	}
	else if (!IS_INSIDE(IN_ROOM(ch))) {
		msg_to_char(ch, "You don't need to escape from here.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to try to escape from here.\r\n");
	}
	else {
		if (GET_ROOM_VEHICLE(IN_ROOM(ch)) && IN_VEHICLE_IN_ROOM(IN_ROOM(ch)) != IN_ROOM(ch) && !VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(ch)), VEH_IN)) {
			msg_to_char(ch, "You run for the edge to escape!\r\n");
			act("$n runs toward the edge to escape!", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {	// this handles buildings and IN-vehicles
			msg_to_char(ch, "You run for the window to escape!\r\n");
			act("$n runs toward the window!", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		
		charge_ability_cost(ch, MOVE, cost, NOTHING, 0, WAIT_ABILITY);
		start_action(ch, ACT_ESCAPING, 1);
		GET_WAIT_STATE(ch) = 4 RL_SEC;	// long wait
	}
}


ACMD(do_hide) {
	char_data *c;
	
	if (!can_use_ability(ch, ABIL_HIDE, NOTHING, 0, NOTHING)) {
		return;
	}
	
	if (GET_POS(ch) == POS_FIGHTING || is_fighting(ch)) {
		msg_to_char(ch, "You can't hide in combat!\r\n");
		return;
	}
	
	if (IS_RIDING(ch)) {
		msg_to_char(ch, "You can't hide while mounted!\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_HIDE)) {
		return;
	}

	msg_to_char(ch, "You attempt to hide yourself.\r\n");

	if (AFF_FLAGGED(ch, AFF_HIDE)) {
		REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE);
	}

	command_lag(ch, WAIT_ABILITY);

	for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
		if (c != ch && CAN_SEE(c, ch) && (!IS_NPC(c) || !MOB_FLAGGED(c, MOB_ANIMAL)) && !skill_check(ch, ABIL_HIDE, DIFF_HARD) && !skill_check(ch, ABIL_CLING_TO_SHADOW, DIFF_MEDIUM)) {
			msg_to_char(ch, "You can't hide with somebody watching!\r\n");
			return;
		}
	}

	gain_ability_exp(ch, ABIL_HIDE, 33.4);
	gain_ability_exp(ch, ABIL_CLING_TO_SHADOW, 10);

	if (has_ability(ch, ABIL_CLING_TO_SHADOW) || skill_check(ch, ABIL_HIDE, DIFF_MEDIUM)) {
		SET_BIT(AFF_FLAGS(ch), AFF_HIDE);
	}
}


ACMD(do_howl) {
	char_data *victim, *next_vict;
	struct affected_type *af;
	int value, cost = 50;
	bool first = TRUE;
	
	if (!can_use_ability(ch, ABIL_HOWL, MOVE, cost, COOLDOWN_HOWL)) {
		return;
	}
	else if (!check_solo_role(ch)) {
		msg_to_char(ch, "You must be alone to use that ability in the solo role.\r\n");
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_HOWL)) {
		return;
	}
	else {
		if (SHOULD_APPEAR(ch)) {
			appear(ch);
		}
		
		charge_ability_cost(ch, MOVE, cost, COOLDOWN_HOWL, 30, WAIT_COMBAT_ABILITY);
		
		msg_to_char(ch, "You let out a fearsome howl!\r\n");
		act("$n lets out a bone-chilling howl!", FALSE, ch, NULL, NULL, TO_ROOM);
		
		for (victim = ROOM_PEOPLE(IN_ROOM(ch)); victim; victim = next_vict) {
			next_vict = victim->next_in_room;
			
			if (AFF_FLAGGED(victim, AFF_IMMUNE_STEALTH)) {
				continue;
			}
			
			if (victim != ch && IS_NPC(victim) && CAN_SEE(ch, victim) && !is_fight_ally(ch, victim) && !in_same_group(ch, victim) && can_fight(ch, victim)) {
				value = GET_CHARISMA(ch) * (first ? 4 : 2);
				first = FALSE;
				
				af = create_mod_aff(ATYPE_HOWL, 3, APPLY_TO_HIT, -value, ch);
				affect_join(victim, af, NOBITS);
				
				msg_to_char(victim, "You are too scared to aim effectively!\r\n");
				engage_combat(ch, victim, TRUE);
			}
		}
	}
}


ACMD(do_infiltrate) {
	void empire_skillup(empire_data *emp, any_vnum ability, double amount);

	room_data *to_room;
	int dir;
	empire_data *emp;
	int cost = 10;

	skip_spaces(&argument);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use infiltrate.\r\n");
	}
	else if (!can_use_ability(ch, ABIL_INFILTRATE, MOVE, cost, NOTHING)) {
		// sends own message
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_INFILTRATE)) {
		return;
	}
	else if (!*argument)
		msg_to_char(ch, "You must choose a direction to infiltrate.\r\n");
	else if ((dir = parse_direction(ch, argument)) == NO_DIR)
		msg_to_char(ch, "Which direction would you like to infiltrate?\r\n");
	else if (dir >= NUM_2D_DIRS || !(to_room = real_shift(IN_ROOM(ch), shift_dir[dir][0], shift_dir[dir][1])) || to_room == IN_ROOM(ch))
		msg_to_char(ch, "You can't infiltrate that direction!\r\n");
	else if (!IS_MAP_BUILDING(to_room))
		msg_to_char(ch, "You can only infiltrate buildings.\r\n");
	else if (IS_INSIDE(IN_ROOM(ch)))
		msg_to_char(ch, "You're already inside.\r\n");
	else if (IS_RIDING(ch))
		msg_to_char(ch, "You can't infiltrate while riding.\r\n");
	else if (!ROOM_IS_CLOSED(to_room) || can_use_room(ch, to_room, GUESTS_ALLOWED))
		msg_to_char(ch, "You can just walk in.\r\n");
	else if (BUILDING_ENTRANCE(to_room) != dir && (!ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES) || BUILDING_ENTRANCE(to_room) != rev_dir[dir]))
		msg_to_char(ch, "You can only infiltrate at the entrance.\r\n");
	else if (!can_infiltrate(ch, (emp = ROOM_OWNER(to_room)))) {
		// sends own message
	}
	else {
		charge_ability_cost(ch, MOVE, cost, NOTHING, 0, WAIT_ABILITY);
		
		gain_ability_exp(ch, ABIL_INFILTRATE, 50);
		gain_ability_exp(ch, ABIL_IMPROVED_INFILTRATE, 50);
		
		if (!has_ability(ch, ABIL_IMPROVED_INFILTRATE) && !skill_check(ch, ABIL_INFILTRATE, (emp && EMPIRE_HAS_TECH(emp, TECH_LOCKS)) ? DIFF_RARELY : DIFF_HARD)) {
			if (emp && EMPIRE_HAS_TECH(emp, TECH_LOCKS)) {
				empire_skillup(emp, ABIL_LOCKS, 10);
			}
			msg_to_char(ch, "You fail.\r\n");
		}
		else {
			char_from_room(ch);
			char_to_room(ch, to_room);
			qt_visit_room(ch, IN_ROOM(ch));
			look_at_room(ch);
			msg_to_char(ch, "\r\nInfiltration successful.\r\n");
			
			GET_LAST_DIR(ch) = dir;
			
			enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
			entry_memory_mtrigger(ch);
			greet_mtrigger(ch, NO_DIR);
			greet_memory_mtrigger(ch);
			greet_vtrigger(ch, NO_DIR);
		}

		// chance to log
		if (emp && !skill_check(ch, ABIL_IMPROVED_INFILTRATE, DIFF_HARD)) {
			if (has_ability(ch, ABIL_IMPROVED_INFILTRATE)) {
				log_to_empire(emp, ELOG_HOSTILITY, "An infiltrator has been spotted at (%d, %d)!", X_COORD(to_room), Y_COORD(to_room));
			}
			else {
				log_to_empire(emp, ELOG_HOSTILITY, "%s has been spotted infiltrating at (%d, %d)!", PERS(ch, ch, 0), X_COORD(to_room), Y_COORD(to_room));
			}
		}
		
		// distrust just in case
		trigger_distrust_from_stealth(ch, emp);
	}
}


ACMD(do_jab) {
	char_data *vict;
	int cost = 15;

	one_argument(argument, arg);

	if (!can_use_ability(ch, ABIL_JAB, MOVE, cost, COOLDOWN_JAB)) {
		// sends own messages
	}
	else if (!IS_NPC(ch) && !GET_EQ(ch, WEAR_WIELD)) {
		send_to_char("You need to wield a weapon to make it a success.\r\n", ch);
	}
	else if (!IS_NPC(ch) && GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_STAB) {
		send_to_char("You must use a stabbing weapon to jab.\r\n", ch);
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM)) && !(vict = FIGHTING(ch))) {
		send_to_char("Jab whom?\r\n", ch);
	}
	else if (vict == ch) {
		send_to_char("You can't jab yourself!\r\n", ch);
	}
	else if (FIGHT_MODE(vict) == FMODE_MISSILE || FIGHT_MODE(ch) == FMODE_MISSILE) {
		msg_to_char(ch, "You aren't close enough.\r\n");
	}
	else if (!can_fight(ch, vict)) {
		act("You can't attack $N!", FALSE, ch, 0, vict, TO_CHAR);
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_JAB)) {
		return;
	}
	else {
		charge_ability_cost(ch, MOVE, cost, COOLDOWN_JAB, 9, WAIT_COMBAT_ABILITY);

		if (IS_NPC(ch)) {
			// NPC has no weapon
			act("You move close to jab $N with your weapon...", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n moves in close to jab you with $s weapon...", FALSE, ch, NULL, vict, TO_VICT);
			act("$n moves in close to jab $N with $s weapon...", FALSE, ch, NULL, vict, TO_NOTVICT);
		}
		else {
			act("You move close to jab $N with $p...", FALSE, ch, GET_EQ(ch, WEAR_WIELD), vict, TO_CHAR);
			act("$n moves in close to jab you with $p...", FALSE, ch, GET_EQ(ch, WEAR_WIELD), vict, TO_VICT);
			act("$n moves in close to jab $N with $p...", FALSE, ch, GET_EQ(ch, WEAR_WIELD), vict, TO_NOTVICT);
		}
		
		if (hit(ch, vict, GET_EQ(ch, WEAR_WIELD), FALSE) > 0) {
			apply_dot_effect(vict, ATYPE_JABBED, 3, DAM_PHYSICAL, get_ability_level(ch, ABIL_JAB) / 24, 2, ch);
			
			if (has_ability(ch, ABIL_STAGGER_JAB) && !AFF_FLAGGED(vict, AFF_IMMUNE_STEALTH) && check_solo_role(ch)) {
				struct affected_type *af;
				int value = ceil(GET_CHARISMA(ch) / 5);
				af = create_mod_aff(ATYPE_STAGGER_JAB, 3, APPLY_TO_HIT, -value, ch);
				affect_join(vict, af, ADD_MODIFIER);
			}
			
			if (has_ability(ch, ABIL_CRUCIAL_JAB) && !AFF_FLAGGED(vict, AFF_IMMUNE_STEALTH) && check_solo_role(ch)) {
				struct affected_type *af;
				int value = round(GET_COMPUTED_LEVEL(ch) / 80);
				af = create_mod_aff(ATYPE_CRUCIAL_JAB, 2, APPLY_DEXTERITY, -value, ch);
				affect_join(vict, af, NOBITS);
			}
			
			if (has_ability(ch, ABIL_SHADOW_JAB) && !AFF_FLAGGED(vict, AFF_IMMUNE_STEALTH) && check_solo_role(ch)) {
				struct affected_type *af;
				int value = ceil(GET_CHARISMA(ch) / 5);
				af = create_mod_aff(ATYPE_SHADOW_JAB, 3, APPLY_DEXTERITY, -value, ch);
				affect_join(vict, af, ADD_MODIFIER);
			}
		}
		if (can_gain_exp_from(ch, vict)) {
			gain_ability_exp(ch, ABIL_JAB, 15);
		}
	}
}


ACMD(do_pickpocket) {
	extern bool check_scaling(char_data *mob, char_data *attacker);
	extern int mob_coins(char_data *mob);

	empire_data *ch_emp = NULL, *vict_emp = NULL;
	bool any, low_level;
	char_data *vict;
	int coins;

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		send_to_char("You have no idea how to do that.\r\n", ch);
	}
	else if (!can_use_ability(ch, ABIL_PICKPOCKET, NOTHING, 0, NOTHING)) {
		// sends own messages
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_to_char("Pickpocket whom?\r\n", ch);
	}
	else if (vict == ch) {
		send_to_char("Come on now, that's rather stupid!\r\n", ch);
	}
	else if (!IS_NPC(vict)) {
		msg_to_char(ch, "You may only pickpocket npcs.\r\n");
	}
	else if ((ch_emp = GET_LOYALTY(ch)) && (vict_emp = GET_LOYALTY(vict)) && has_relationship(ch_emp, vict_emp, DIPL_ALLIED | DIPL_NONAGGR)) {
		msg_to_char(ch, "You can't pickpocket mobs you are allied or have a non-aggression pact with.\r\n");
	}
	else if (ch_emp && vict_emp && GET_RANK(ch) < EMPIRE_PRIV(ch_emp, PRIV_STEALTH) && !has_relationship(ch_emp, vict_emp, DIPL_WAR)) {
		msg_to_char(ch, "You don't have permission to steal that -- you could start a war!\r\n");
	}
	else if (ch_emp && vict_emp && !PRF_FLAGGED(ch, PRF_STEALTHABLE) && !has_relationship(ch_emp, vict_emp, DIPL_WAR)) {
		msg_to_char(ch, "You cannot pickpocket that target because your 'stealthable' toggle is off.\r\n");
	}
	else if (FIGHTING(vict)) {
		msg_to_char(ch, "You can't steal from someone who's fighting.\r\n");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_PICKPOCKET)) {
		return;
	}
	else if (MOB_FLAGGED(vict, MOB_PICKPOCKETED | MOB_NO_LOOT) || (AFF_FLAGGED(vict, AFF_NO_ATTACK) && !!has_interaction(vict->interactions, INTERACT_PICKPOCKET))) {
		act("$E doesn't appear to be carrying anything in $S pockets.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else {
		check_scaling(vict, ch);

		// some random coins (negative coins are not given)
		if (MOB_FLAGGED(vict, MOB_HUMAN) && (!GET_LOYALTY(vict) || GET_LOYALTY(vict) == GET_LOYALTY(ch) || char_has_relationship(ch, vict, DIPL_WAR))) {
			coins = mob_coins(vict);
		}
		else {
			coins = 0;
		}
		vict_emp = GET_LOYALTY(vict);	// in case not set earlier
		
		low_level = (get_approximate_level(ch) + 50 < get_approximate_level(vict));
		
		if (!low_level && (!CAN_SEE(vict, ch) || !AWAKE(vict) || AFF_FLAGGED(vict, AFF_STUNNED) || skill_check(ch, ABIL_PICKPOCKET, DIFF_EASY))) {
			// success!
			SET_BIT(MOB_FLAGS(vict), MOB_PICKPOCKETED);
			act("You pick $N's pocket...", FALSE, ch, NULL, vict, TO_CHAR);

			// any will tell us if we got at least 1 item (also sends messages)
			any = run_interactions(ch, vict->interactions, INTERACT_PICKPOCKET, IN_ROOM(ch), vict, NULL, pickpocket_interact);
			any |= run_global_mob_interactions(ch, vict, INTERACT_PICKPOCKET, pickpocket_interact);
			
			if (coins > 0) {
				increase_coins(ch, vict_emp, coins);
			}
			
			// messaging
			if (coins > 0) {
				msg_to_char(ch, "You find %s!\r\n", money_amount(vict_emp, coins));
			}
			else if (!any) {
				msg_to_char(ch, "You find nothing of any use.\r\n");
			}
		}
		else {
			// oops... put these back
			if (coins > 0 && GET_LOYALTY(vict)) {
				EMPIRE_COINS(GET_LOYALTY(vict)) += coins;
			}
			
			if (!AWAKE(vict)) {
				wake_and_stand(vict);
			}
			
			// fail
			act("You try to pickpocket $N, but $E catches you!", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n tries to pick your pocket, but you catch $m in the act!", FALSE, ch, NULL, vict, TO_VICT);
			act("$n tries to pickpocket $N, but is caught!", FALSE, ch, NULL, vict, TO_NOTVICT);
			
			engage_combat(ch, vict, TRUE);
		}
		
		if (vict_emp && vict_emp != ch_emp) {
			trigger_distrust_from_stealth(ch, vict_emp);
		}
		
		// gain either way
		if (can_gain_exp_from(ch, vict)) {
			gain_ability_exp(ch, ABIL_PICKPOCKET, 25);
		}
		command_lag(ch, WAIT_ABILITY);
	}
}


ACMD(do_prick) {
	char_data *vict = FIGHTING(ch);
	int cost = 10;
	int type = IS_NPC(ch) ? NOTHING : USING_POISON(ch);
	
	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use this ability.\r\n");
	}
	else if (!can_use_ability(ch, ABIL_PRICK, MOVE, cost, COOLDOWN_PRICK)) {
		// sends own messages
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM)) && !(vict = FIGHTING(ch))) {
		send_to_char("Prick whom?\r\n", ch);
	}
	else if (vict == ch) {
		send_to_char("You can't prick yourself!\r\n", ch);
	}
	else if (FIGHT_MODE(vict) == FMODE_MISSILE || FIGHT_MODE(ch) == FMODE_MISSILE) {
		msg_to_char(ch, "You aren't close enough.\r\n");
	}
	else if (!can_fight(ch, vict)) {
		act("You can't attack $N!", FALSE, ch, 0, vict, TO_CHAR);
	}
	else if (!find_poison_by_type(ch->carrying, type)) {
		msg_to_char(ch, "You seem to be out of poison.\r\n");
	}
	else if (GET_MOVE(ch) < cost) {
		msg_to_char(ch, "You need %d move points to prick.\r\n", cost);
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_PRICK)) {
		return;
	}
	else {
		charge_ability_cost(ch, MOVE, cost, COOLDOWN_PRICK, 9, WAIT_COMBAT_ABILITY);
		
		if (SHOULD_APPEAR(ch)) {
			appear(ch);
		}

		act("You quickly prick $N with poison!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n pricks you with poison!", FALSE, ch, NULL, vict, TO_VICT);
		act("$n pricks $N with poison!", TRUE, ch, NULL, vict, TO_NOTVICT);

		// possibly fatal
		if (apply_poison(ch, vict, type) == 0) {
			msg_to_char(ch, "It seems to have no effect.\r\n");
		}
		
		// apply_poison could have killed vict -- check location, etc
		if (!EXTRACTED(vict) && !IS_DEAD(vict) && CAN_SEE(vict, ch)) {
			engage_combat(ch, vict, TRUE);
		}
		
		if (can_gain_exp_from(ch, vict)) {
			gain_ability_exp(ch, ABIL_PRICK, 15);
		}
	}
}


ACMD(do_sap) {
	char_data *vict = NULL;
	struct affected_type *af;
	int cost = 15;
	bool success;
	
	one_argument(argument, arg);

	if (!can_use_ability(ch, ABIL_SAP, MOVE, cost, COOLDOWN_SAP)) {
		// sends own messages
	}
	else if (!*arg) {
		msg_to_char(ch, "Sap whom?\r\n");
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (ch == vict) {
		msg_to_char(ch, "You wouldn't want to sap yourself.\r\n");
	}
	else if (FIGHTING(vict)) {
		msg_to_char(ch, "Your target is already busy fighting.\r\n");
	}
	else if (!can_fight(ch, vict)) {
		act("You can't attack $M!", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (AFF_FLAGGED(vict, AFF_IMMUNE_STUN | AFF_IMMUNE_STEALTH)) {
		act("$E doesn't look like $E'd be affected by that.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (AFF_FLAGGED(vict, AFF_STUNNED)) {
		act("$E is already stunned!", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_SAP)) {
		return;
	}
	else {
		charge_ability_cost(ch, MOVE, cost, COOLDOWN_SAP, 9, WAIT_COMBAT_ABILITY);
	
		success = !AFF_FLAGGED(vict, AFF_IMMUNE_STEALTH) && (!CAN_SEE(vict, ch) || skill_check(ch, ABIL_SAP, DIFF_MEDIUM));

		act("You sap $N in the back of the head.", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n saps you in the back of the head. That hurt!", FALSE, ch, NULL, vict, TO_VICT);
		act("$n saps $N in the back of the head.", TRUE, ch, NULL, vict, TO_NOTVICT);
		
		if (success) {
			af = create_flag_aff(ATYPE_SAP, REAL_UPDATES_PER_MIN, AFF_STUNNED, ch);
			affect_join(vict, af, 0);
			
			msg_to_char(vict, "You are momentarily stunned!\r\n");
			act("$n is stunned!", TRUE, vict, NULL, NULL, TO_ROOM);
		}
		else {
			engage_combat(ch, vict, TRUE);
		}
		
		if (can_gain_exp_from(ch, vict)) {
			gain_ability_exp(ch, ABIL_SAP, 20);
		}
		
		// release other saps here
		limit_crowd_control(vict, ATYPE_SAP);
	}
}


ACMD(do_search) {
	char_data *targ;
	bool found = FALSE, earthmeld = FALSE;

	if (!can_use_ability(ch, ABIL_SEARCH, NOTHING, 0, COOLDOWN_SEARCH)) {
		// sends own message
	}
	else if (AFF_FLAGGED(ch, AFF_BLIND))
		msg_to_char(ch, "How can you do that, you're blind!\r\n");
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch)))
		msg_to_char(ch, "You can't see well enough here to search for anyone!\r\n");
	else if (AFF_FLAGGED(ch, AFF_SENSE_HIDE))
		msg_to_char(ch, "You search, but find nobody.\r\n");
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_SEARCH)) {
		return;
	}
	else {
		act("$n begins searching around!", TRUE, ch, 0, 0, TO_ROOM);

		for (targ = ROOM_PEOPLE(IN_ROOM(ch)); targ; targ = targ->next_in_room) {
			if (ch == targ)
				continue;
			
			// hidden targets
			if (AFF_FLAGGED(targ, AFF_HIDE)) {
					continue;
			
				if (has_ability(targ, ABIL_CLING_TO_SHADOW)) {
					gain_ability_exp(targ, ABIL_CLING_TO_SHADOW, 20);
					continue;
				}

				SET_BIT(AFF_FLAGS(ch), AFF_SENSE_HIDE);

				if (skill_check(ch, ABIL_SEARCH, DIFF_HARD) && CAN_SEE(ch, targ)) {
					act("You find $N!", FALSE, ch, 0, targ, TO_CHAR);
					msg_to_char(targ, "You are discovered!\r\n");
					REMOVE_BIT(AFF_FLAGS(targ), AFF_HIDE);
					affects_from_char_by_aff_flag(targ, AFF_HIDE, FALSE);
					found = TRUE;
				}

				REMOVE_BIT(AFF_FLAGS(ch), AFF_SENSE_HIDE);
			}
			else if (!earthmeld && AFF_FLAGGED(targ, AFF_EARTHMELD)) {
				// earthmelded targets (only do once)
				if (skill_check(ch, ABIL_SEARCH, DIFF_HARD) && CAN_SEE(ch, targ)) {
					act("You find signs that someone is earthmelded here.", FALSE, ch, NULL, NULL, TO_CHAR);
					found = earthmeld = TRUE;
				}
			}
		}

		if (!found)
			msg_to_char(ch, "You search, but find nobody.\r\n");

		charge_ability_cost(ch, NOTHING, 0, COOLDOWN_SEARCH, 10, WAIT_ABILITY);
		gain_ability_exp(ch, ABIL_SEARCH, 20);
	}
}


ACMD(do_shadowcage) {
	char_data *victim, *next_vict;
	struct affected_type *af;
	int value, cost = 50;
	bool first = TRUE;
	
	if (!can_use_ability(ch, ABIL_SHADOWCAGE, MOVE, cost, COOLDOWN_SHADOWCAGE)) {
		return;
	}
	else if (!check_solo_role(ch)) {
		msg_to_char(ch, "You must be alone to use that ability in the solo role.\r\n");
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_SHADOWCAGE)) {
		return;
	}
	else {
		if (SHOULD_APPEAR(ch)) {
			appear(ch);
		}
		
		charge_ability_cost(ch, MOVE, cost, COOLDOWN_SHADOWCAGE, 30, WAIT_COMBAT_ABILITY);
		
		msg_to_char(ch, "You shoot webs of pure shadow, forming a tight cage!\r\n");
		act("$n shoots webs of pure shadow, forming a tight cage!", FALSE, ch, NULL, NULL, TO_ROOM);
		
		for (victim = ROOM_PEOPLE(IN_ROOM(ch)); victim; victim = next_vict) {
			next_vict = victim->next_in_room;
			
			if (AFF_FLAGGED(victim, AFF_IMMUNE_STEALTH)) {
				continue;
			}
			
			if (victim != ch && IS_NPC(victim) && CAN_SEE(ch, victim) && !is_fight_ally(ch, victim) && !in_same_group(ch, victim) && can_fight(ch, victim)) {
				value = GET_CHARISMA(ch) * (first ? 4 : 2);
				first = FALSE;
				
				af = create_mod_aff(ATYPE_SHADOWCAGE, 3, APPLY_DODGE, -value, ch);
				affect_join(victim, af, NOBITS);
				
				msg_to_char(victim, "You can't seem to dodge as well in the shadowcage!\r\n");
				engage_combat(ch, victim, TRUE);
			}
		}
	}
}


ACMD(do_shadowstep) {
	bool can_infiltrate(char_data *ch, empire_data *emp);

	char_data *vict = NULL;
	empire_data *emp = NULL;
	int cost = 50;
	bool infil = FALSE;

	skip_spaces(&argument);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot shadowstep.\r\n");
	}
	else if (!can_use_ability(ch, ABIL_SHADOWSTEP, MOVE, cost, COOLDOWN_SHADOWSTEP)) {
		// sends own messages
	}
	else if (IS_RIDING(ch))
		msg_to_char(ch, "You can't shadowstep while riding.\r\n");
	else if (!can_teleport_to(ch, IN_ROOM(ch), FALSE)) {
		msg_to_char(ch, "You can't teleport out of here.\r\n");
	}
	else if (!*argument)
		msg_to_char(ch, "Shadowstep to whom?\r\n");
	else if (!isdigit(*argument) && (!(vict = find_closest_char(ch, argument, FALSE)) || compute_distance(IN_ROOM(ch), IN_ROOM(vict)) > 7)) {
		// simple targeting: find closest
		msg_to_char(ch, "Nobody by that name within range.\r\n");
	}
	else if (!vict && isdigit(*argument) && (!(vict = get_char_vis(ch, arg, FIND_CHAR_WORLD)) || compute_distance(IN_ROOM(ch), IN_ROOM(vict)) > 7)) {
		// number targeting: find by name
		msg_to_char(ch, "Nobody by that name within range.\r\n");
	}
	else if (ch == vict) {
		msg_to_char(ch, "You can't shadowstep to yourself.\r\n");
	}
	else if (IS_GOD(vict) || IS_IMMORTAL(vict)) {
		msg_to_char(ch, "You can't target a god!\r\n");
	}
	else if (!IS_OUTDOORS(vict) && !can_use_room(ch, IN_ROOM(vict), GUESTS_ALLOWED) && (emp = ROOM_OWNER(IN_ROOM(vict))) && !can_infiltrate(ch, emp)) {
		// can_infiltrate sends its own message
	}
	else if (IS_NPC(vict) && ((GET_LOYALTY(vict) && GET_LOYALTY(vict) != GET_LOYALTY(ch)) || !can_use_room(ch, IN_ROOM(vict), GUESTS_ALLOWED))) {
		msg_to_char(ch, "You can't shadowstep to NPCs in other empires.\r\n");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_SHADOWSTEP)) {
		return;
	}
	else if (!can_teleport_to(ch, IN_ROOM(vict), FALSE)) {
		msg_to_char(ch, "You can't shadowstep there.\r\n");
	}
	else {
		infil = !can_use_room(ch, IN_ROOM(vict), GUESTS_ALLOWED);
		
		if (infil && emp && GET_LOYALTY(ch) && !has_relationship(GET_LOYALTY(ch), emp, DIPL_WAR)) {
			if (!PRF_FLAGGED(ch, PRF_STEALTHABLE)) {
				msg_to_char(ch, "You cannot shadowstep there while your 'stealthable' toggle is off.\r\n");
				return;
			}
			if (GET_RANK(ch) < EMPIRE_PRIV(GET_LOYALTY(ch), PRIV_STEALTH)) {
				msg_to_char(ch, "You don't have the empire rank required to commit stealth acts.\r\n");
				return;
			}
		}

		charge_ability_cost(ch, MOVE, cost, COOLDOWN_SHADOWSTEP, SECS_PER_REAL_MIN, WAIT_ABILITY);
		gain_ability_exp(ch, ABIL_SHADOWSTEP, 20);
		
		if (infil && !has_ability(ch, ABIL_IMPROVED_INFILTRATE) && !skill_check(ch, ABIL_INFILTRATE, DIFF_HARD)) {
			msg_to_char(ch, "You fail to shadowstep to that location.\r\n");
		}
		else {
			act("You shadowstep to $N!", FALSE, ch, NULL, vict, TO_CHAR);
			char_from_room(ch);
			char_to_room(ch, IN_ROOM(vict));
			qt_visit_room(ch, IN_ROOM(ch));
			look_at_room(ch);
			act("$n steps out of the shadows!", TRUE, ch, NULL, NULL, TO_ROOM);
			
			GET_LAST_DIR(ch) = NO_DIR;
			
			enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
			entry_memory_mtrigger(ch);
			greet_mtrigger(ch, NO_DIR);
			greet_memory_mtrigger(ch);
			greet_vtrigger(ch, NO_DIR);
		}

		// chance to log
		if (infil && emp && !skill_check(ch, ABIL_IMPROVED_INFILTRATE, DIFF_HARD)) {
			if (has_ability(ch, ABIL_IMPROVED_INFILTRATE)) {
				log_to_empire(emp, ELOG_HOSTILITY, "An infiltrator has been spotted shadowstepping into (%d, %d)!", X_COORD(IN_ROOM(vict)), Y_COORD(IN_ROOM(vict)));
			}
			else {
				log_to_empire(emp, ELOG_HOSTILITY, "%s has been spotted shadowstepping into (%d, %d)!", PERS(ch, ch, 0), X_COORD(IN_ROOM(vict)), Y_COORD(IN_ROOM(vict)));
			}
		}
		
		if (infil && emp) {
			// distrust just in case
			trigger_distrust_from_stealth(ch, emp);
			gain_ability_exp(ch, ABIL_INFILTRATE, 50);
			gain_ability_exp(ch, ABIL_IMPROVED_INFILTRATE, 50);
		}
	}
}


/* Sneak is now sneak <direction>.  Note that entire parties may not sneak together. */
ACMD(do_sneak) {
	extern int perform_move(char_data *ch, int dir, int need_specials_check, byte mode);
	
	int dir;
	bool sneaking = FALSE;

	// Already sneaking? Allow without skill
	if (AFF_FLAGGED(ch, AFF_SNEAK)) {
		sneaking = TRUE;
	}
	if (!sneaking && !can_use_ability(ch, ABIL_SNEAK, NOTHING, 0, NOTHING)) {
		return;
	}
	if (AFF_FLAGGED(ch, AFF_ENTANGLED)) {
		msg_to_char(ch, "You are entangled and can't sneak.\r\n");
		return;
	}
	if (GET_LEADING_MOB(ch) || GET_LEADING_VEHICLE(ch)) {
		msg_to_char(ch, "You can't sneak while leading something.\r\n");
		return;
	}
	if (IS_RIDING(ch)) {
		msg_to_char(ch, "You can't sneak while mounted!\r\n");
		return;
	}

	one_argument(argument, arg);

	if ((dir = parse_direction(ch, arg)) == NO_DIR || dir == DIR_RANDOM) {
		msg_to_char(ch, "Sneak which direction?\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_SNEAK)) {
		return;
	}

	/* Check successfulness */
	if (skill_check(ch, ABIL_SNEAK, DIFF_EASY)) {
		SET_BIT(AFF_FLAGS(ch), AFF_SNEAK);
	}
	else {
		/* If we fail to sneak, we also lose hide.  Else, hide stays */
		REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE);
	}

	if (perform_move(ch, dir, FALSE, 0)) {	// should be MOVE_NORMAL
		gain_ability_exp(ch, ABIL_SNEAK, 5);
	}
	
	// Delay after sneaking -- on top of normal move lag
	GET_WAIT_STATE(ch) += 1 RL_SEC;
	
	if (!sneaking) {
		REMOVE_BIT(AFF_FLAGS(ch), AFF_SNEAK);
	}
}


ACMD(do_steal) {
	bool inventory_store_building(char_data *ch, room_data *room, empire_data *emp);
	void read_vault(empire_data *emp);
	
	struct empire_storage_data *store, *next_store;
	empire_data *emp = ROOM_OWNER(HOME_ROOM(IN_ROOM(ch)));
	obj_data *proto;
	bool found = FALSE;
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot steal.\r\n");
	}
	else if (!can_use_ability(ch, ABIL_STEAL, NOTHING, 0, NOTHING)) {
		// sends own message
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_STEAL)) {
		return;
	}
	else if (!IS_IMMORTAL(ch) && !can_steal(ch, emp)) {
		// sends own message
	}
	else if (!emp) {
		msg_to_char(ch, "Nothing is stored here that you can steal.\r\n");
	}
	else if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CEDED)) {
		msg_to_char(ch, "You can't steal from a building which was ceded to an empire but never used by that empire.\r\n");
	}
	else if (!*arg) {
		if (!(inventory_store_building(ch, IN_ROOM(ch), emp))) {
			msg_to_char(ch, "Nothing is stored here.\r\n");
		}
	}
	else {
		for (store = EMPIRE_STORAGE(emp); store && !found; store = next_store) {
			next_store = store->next;
			
			// island check
			if (store->island != GET_ISLAND_ID(IN_ROOM(ch))) {
				continue;
			}
			
			proto = obj_proto(store->vnum);
			
			if (proto && obj_can_be_stored(proto, IN_ROOM(ch)) && isname(arg, GET_OBJ_KEYWORDS(proto))) {
				found = TRUE;
				
				if (stored_item_requires_withdraw(proto) && !has_ability(ch, ABIL_VAULTCRACKING)) {
					msg_to_char(ch, "You can't steal that without Vaultcracking!\r\n");
					return;
				}
				else {
					// SUCCESS!
					if (IS_IMMORTAL(ch)) {
						syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s stealing %s from %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(proto), EMPIRE_NAME(emp));
					}
					else if (!skill_check(ch, ABIL_STEAL, DIFF_HARD)) {
						log_to_empire(emp, ELOG_HOSTILITY, "Theft at (%d, %d)", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
					}

					retrieve_resource(ch, emp, store, TRUE);
					gain_ability_exp(ch, ABIL_STEAL, 50);
				
					if (stored_item_requires_withdraw(proto)) {
						gain_ability_exp(ch, ABIL_VAULTCRACKING, 50);
					}

					// save the empire
					EMPIRE_NEEDS_SAVE(emp) = TRUE;
					read_vault(emp);
				
					GET_WAIT_STATE(ch) = 4 RL_SEC;	// long wait
				}
			}
		}

		if (!found) {
			msg_to_char(ch, "Nothing like that is stored here!\r\n");
			return;
		}
	}
}


ACMD(do_terrify) {
	struct affected_type *af;
	int value, cost = 15;
	char_data *victim;

	one_argument(argument, arg);

	if (!can_use_ability(ch, ABIL_TERRIFY, MOVE, cost, COOLDOWN_TERRIFY)) {
		// sends own message
	}
	else if (!*arg && !FIGHTING(ch))
		msg_to_char(ch, "Terrify whom?\r\n");
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_ROOM)) && !(victim = FIGHTING(ch)))
		send_config_msg(ch, "no_person");
	else if (victim == ch) {
		msg_to_char(ch, "If you want to terrify yourself, just think back on whether or not you've accomplished all your life goals.\r\n");
	}
	else if (!can_fight(ch, victim))
		act("You can't terrify $N!", FALSE, ch, 0, victim, TO_CHAR);
	else if (!AWAKE(victim) || IS_DEAD(victim)) {
		msg_to_char(ch, "You can't use your shadows to terrify someone who can't even see them!\r\n");
	}
	else if (AFF_FLAGGED(victim, AFF_IMMUNE_STEALTH)) {
		act("$E doesn't look like $E'd be affected by that.", FALSE, ch, NULL, victim, TO_CHAR);
	}
	else if (ABILITY_TRIGGERS(ch, victim, NULL, ABIL_TERRIFY)) {
		return;
	}
	else {
		charge_ability_cost(ch, MOVE, cost, COOLDOWN_TERRIFY, 15, WAIT_COMBAT_ABILITY);
		
		if (SHOULD_APPEAR(ch)) {
			appear(ch);
		}
		
		act("Shadows creep up around you and then strike out at $N!", FALSE, ch, 0, victim, TO_CHAR);
		act("Shadows creep up around $n and then strike out at you!", FALSE, ch, 0, victim, TO_VICT);
		act("Shadows creep up around $n and then strike out at $N!", FALSE, ch, 0, victim, TO_NOTVICT);
		
		if (skill_check(ch, ABIL_TERRIFY, DIFF_HARD) && !AFF_FLAGGED(victim, AFF_IMMUNE_STEALTH)) {
			value = round(GET_COMPUTED_LEVEL(ch) / 30);
			af = create_mod_aff(ATYPE_TERRIFY, 0.5 * REAL_UPDATES_PER_MIN, APPLY_BONUS_PHYSICAL, -value, ch);
			affect_join(victim, af, 0);
			af = create_mod_aff(ATYPE_TERRIFY, 0.5 * REAL_UPDATES_PER_MIN, APPLY_BONUS_MAGICAL, -value, ch);
			affect_join(victim, af, 0);
			
			msg_to_char(victim, "You are terrified!\r\n");
			act("$n is terrified!", TRUE, victim, NULL, NULL, TO_ROOM);
		}
		
		engage_combat(victim, ch, TRUE);
		if (can_gain_exp_from(ch, victim)) {
			gain_ability_exp(ch, ABIL_TERRIFY, 15);
		}
	}
}


ACMD(do_whisperstride) {
	struct affected_type *af;
	int cost = 100;
	int duration = 6; // 30 seconds
	if (!can_use_ability(ch, ABIL_WHISPERSTRIDE, MOVE, cost, COOLDOWN_WHISPERSTRIDE)) {
		return;
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_WHISPERSTRIDE)) {
		return;
	}
	else if (IS_RIDING(ch)) {
		msg_to_char(ch, "You can't use Whisperstride while mounted!\r\n");
		return;
	}
	else {
		charge_ability_cost(ch, MOVE, cost, COOLDOWN_WHISPERSTRIDE, 5 * SECS_PER_REAL_MIN, WAIT_ABILITY);
		
		msg_to_char(ch, "You cloak yourself with dark whispers, muffling your movement...\r\n");
		act("$n is surrounded by dark whispers...", TRUE, ch, NULL, NULL, TO_ROOM);
		
		af = create_flag_aff(ATYPE_WHISPERSTRIDE, duration, AFF_SNEAK, ch);
		affect_join(ch, af, 0);
	}
}
