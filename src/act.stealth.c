/* ************************************************************************
*   File: act.stealth.c                                   EmpireMUD 2.0b5 *
*  Usage: code related to stealthy commands and activities                *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#include "constants.h"

/**
* Contents:
*   Helpers
*   Poisons
*   Commands
*/

// external funcs
ACMD(do_dismount);


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
	struct empire_political_data *pol = NULL;
	empire_data *chemp = GET_LOYALTY(ch);
	
	// no empire = no problem
	if (!emp) {
		return TRUE;
	}
	if (chemp) {	// look this up for later
		pol = find_relation(chemp, emp);
	}
	
	if (emp == chemp) {
		msg_to_char(ch, "You can't infiltrate your own empire!\r\n");
		return FALSE;
	}
	
	if (EMPIRE_IMM_ONLY(emp)) {
		msg_to_char(ch, "You cannot infiltrate immortal empires.\r\n");
		return FALSE;
	}
	
	if (count_members_online(emp) == 0 && (!chemp || !pol || !IS_SET(pol->type, DIPL_WAR | DIPL_THIEVERY))) {
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
	
	if (pol && IS_SET(pol->type, DIPL_ALLIED | DIPL_NONAGGR)) {
		msg_to_char(ch, "You can't infiltrate -- you have a non-aggression pact with %s.\r\n", EMPIRE_NAME(emp));
		return FALSE;
	}
	
	if (GET_RANK(ch) < EMPIRE_PRIV(chemp, PRIV_STEALTH) && (!pol || !IS_SET(pol->type, DIPL_WAR | DIPL_THIEVERY))) {
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
	time_t timediff;
	
	int death_penalty_time = config_get_int("steal_death_penalty");
	
	// no empire = ok
	if (!emp) {
		return TRUE;
	}
	
	if (!has_player_tech(ch, PTECH_STEAL_COMMAND)) {
		msg_to_char(ch, "You can't steal anything.\r\n");
		return FALSE;
	}
	
	if (chemp && EMPIRE_ADMIN_FLAGGED(chemp, EADM_NO_STEAL)) {
		msg_to_char(ch, "Your empire has been forbidden from stealing.\r\n");
		return FALSE;
	}
	
	if (!PRF_FLAGGED(ch, PRF_STEALTHABLE)) {
		msg_to_char(ch, "You cannot steal because your 'stealthable' toggle is off.\r\n");
		return FALSE;
	}
	
	if (!chemp || !has_relationship(chemp, emp, DIPL_WAR | DIPL_THIEVERY)) {
		msg_to_char(ch, "You need a thievery permit (using the diplomacy command) to steal from that empire.\r\n");
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
	
	timediff = (death_penalty_time * SECS_PER_REAL_MIN) - (time(0) - get_last_killed_by_empire(ch, emp));
	if (death_penalty_time && get_last_killed_by_empire(ch, emp) && timediff > 0) {
		msg_to_char(ch, "You cannot steal from %s because they have killed you too recently (%d:%02d remain).\r\n", EMPIRE_NAME(emp), (int)(timediff / 60), (int)(timediff % 60));
		return FALSE;
	}
	
	if (count_members_online(emp) == 0 && !has_relationship(chemp, emp, DIPL_WAR | DIPL_THIEVERY)) {
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
	
	if (GET_RANK(ch) < EMPIRE_PRIV(chemp, PRIV_STEALTH) && (!pol || !IS_SET(pol->type, DIPL_WAR | DIPL_THIEVERY))) {
		msg_to_char(ch, "You don't have permission from your empire to steal from others. Are you trying to start a war?\r\n");
		return FALSE;
	}
	
	// got this far...
	return TRUE;
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(pickpocket_interact) {
	int iter;
	obj_data *obj = NULL;
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, get_approximate_level(inter_mob));
		obj_to_char(obj, ch);
		act("You find $p!", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		if (load_otrigger(obj)) {
			get_otrigger(obj, ch, FALSE);
		}
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
		
	return TRUE;
}


/**
* Sets up a player's disguise.
*
* @param char_data *ch The player (not NPC).
* @param const char *name A name to set (Optional: pass NULL if you're just changing the sex).
* @param int sex The sex to disguise as (Optional: pass NOTHING if you're just changing the name).
*/
void set_disguise(char_data *ch, const char *name, int sex) {
	if (IS_NPC(ch)) {
		return;	// no disguises
	}
	
	SET_BIT(PLR_FLAGS(ch), PLR_DISGUISED);
	
	// copy name and check limit
	if (name && *name) {
		if (GET_DISGUISED_NAME(ch)) {
			free(GET_DISGUISED_NAME(ch));
		}
		GET_DISGUISED_NAME(ch) = str_dup(name);
	}

	// copy the sex
	if (sex != NOTHING) {
		GET_DISGUISED_SEX(ch) = sex;
	}
	
	// msdp updates
	update_MSDP_gender(ch, UPDATE_SOON);
	update_MSDP_name(ch, UPDATE_SOON);
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
	
		// msdp updates
		update_MSDP_gender(ch, UPDATE_SOON);
		update_MSDP_name(ch, UPDATE_SOON);
		
		msg_to_char(ch, "You take off your disguise.\r\n");
		act(lbuf, TRUE, ch, NULL, NULL, TO_ROOM);
	}
}


/**
* Determines if a room qualifies for Unseen Passing (indoors/in-city).
*
* TODO: rename something more generic
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

/**
* finds a matching poison object in an object list, preferring the one with the
* fewest charges.
*
* @param obj_data *list ch->carrying, e.g.
* @param any_vnum vnum The poison vnum to look for.
* @return obj_data *the poison, if found
*/
obj_data *find_poison_by_vnum(obj_data *list, any_vnum vnum) {
	obj_data *obj, *found = NULL;
	
	DL_FOREACH2(list, obj, next_content) {
		if (IS_POISON(obj) && GET_OBJ_VNUM(obj) == vnum) {
			if (!found || GET_POISON_CHARGES(obj) < GET_POISON_CHARGES(found)) {
				found = obj;
			}
		}
	}
	
	return found;
}


/**
* Apply the actual effects of a poison. This also makes sure there's some
* available, and manages the object.
*
* @param char_data *ch the person doing the poisoning
* @param char_data *vict the person being poisoned
* @return -1 if poison killed the person, 0 if no hit at all, >0 if hit
*/
int apply_poison(char_data *ch, char_data *vict) {
	int aff_type = ATYPE_POISON, result = 0;
	struct affected_type *af;
	struct obj_apply *apply;
	bool messaged = FALSE;
	obj_data *obj;
	double mod;
	
	if (IS_NPC(ch) || IS_GOD(vict) || IS_IMMORTAL(vict)) {
		return 0;
	}
	
	if (AFF_FLAGGED(vict, AFF_IMMUNE_POISON_DEBUFFS)) {
		return 0;	// immune to poison debuffs specfically
	}
	
	if (!(obj = find_poison_by_vnum(ch->carrying, USING_POISON(ch)))) {
		return 0;	// out of poison
	}
	
	// update aff type
	aff_type = GET_POISON_AFFECT(obj) != NOTHING ? GET_POISON_AFFECT(obj) : ATYPE_POTION;

	if (affected_by_spell_from_caster(vict, aff_type, ch)) {
		return 0;	// stack check: don't waste charges
	}
	
	// GAIN SKILL NOW -- it at least attempts an application
	if (can_gain_exp_from(ch, vict)) {
		gain_player_tech_exp(ch, PTECH_POISON, 2);
		gain_player_tech_exp(ch, PTECH_POISON_UPGRADE, 2);
	}
	run_ability_hooks_by_player_tech(ch, PTECH_POISON, vict, NULL, NULL, NULL);
	
	// skill check!
	if (!player_tech_skill_check_by_ability_difficulty(ch, PTECH_POISON)) {
		return 0;
	}
	
	// applied -- charge a charge (can no longer be stored)
	set_obj_val(obj, VAL_POISON_CHARGES, GET_POISON_CHARGES(obj) - 1);
	SET_BIT(GET_OBJ_EXTRA(obj), OBJ_NO_STORE);
	request_obj_save_in_world(obj);
	
	// attempt immunity/resist
	if (has_player_tech(vict, PTECH_NO_POISON)) {
		if (can_gain_exp_from(vict, ch)) {
			gain_player_tech_exp(vict, PTECH_NO_POISON, 10);
		}
		run_ability_hooks_by_player_tech(vict, PTECH_NO_POISON, ch, NULL, NULL, NULL);
		
		if (GET_POISON_CHARGES(obj) <= 0) {
			run_interactions(ch, GET_OBJ_INTERACTIONS(obj), INTERACT_CONSUMES_TO, IN_ROOM(ch), NULL, obj, NULL, consumes_or_decays_interact);
			extract_obj(obj);
		}
		return 0;
	}
	if (has_player_tech(vict, PTECH_RESIST_POISON)) {
		if (can_gain_exp_from(vict, ch)) {
			gain_player_tech_exp(vict, PTECH_RESIST_POISON, 10);
		}
		run_ability_hooks_by_player_tech(vict, PTECH_RESIST_POISON, ch, NULL, NULL, NULL);
		if (!number(0, 2)) {
			if (GET_POISON_CHARGES(obj) <= 0) {
				run_interactions(ch, GET_OBJ_INTERACTIONS(obj), INTERACT_CONSUMES_TO, IN_ROOM(ch), NULL, obj, NULL, consumes_or_decays_interact);
				extract_obj(obj);
			}
			return 0;
		}
	}
	
	// OK GO:
	
	mod = has_player_tech(ch, PTECH_POISON_UPGRADE) ? 1.5 : 1.0;
	
	// ensure scaled
	if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
		scale_item_to_level(obj, 1);	// minimum level
	}
	
	// remove any old buffs (if adding a new one)
	if (GET_OBJ_AFF_FLAGS(obj) || GET_OBJ_APPLIES(obj)) {
		affect_from_char_by_caster(vict, aff_type, ch, FALSE);
	}
	
	if (GET_OBJ_AFF_FLAGS(obj)) {
		af = create_flag_aff(aff_type, 30 * SECS_PER_REAL_MIN, GET_OBJ_AFF_FLAGS(obj), ch);
		affect_to_char(vict, af);
		free(af);
		
		if (!messaged) {
			act("You feel ill as you are poisoned!", FALSE, vict, NULL, NULL, TO_CHAR);
			act("$n looks ill as $e is poisoned!", FALSE, vict, NULL, NULL, TO_ROOM);
			messaged = TRUE;
		}
	}
	
	LL_FOREACH(GET_OBJ_APPLIES(obj), apply) {
		af = create_mod_aff(aff_type, 30 * SECS_PER_REAL_MIN, apply->location, round(apply->modifier * mod), ch);
		affect_to_char(vict, af);
		free(af);
		
		if (!messaged) {
			act("You feel ill as you are poisoned!", FALSE, vict, NULL, NULL, TO_CHAR);
			act("$n looks ill as $e is poisoned!", FALSE, vict, NULL, NULL, TO_ROOM);
			messaged = TRUE;
		}
	}
	
	// mark result if there's a consume trigger
	if (!result && SCRIPT_CHECK(obj, OTRIG_CONSUME)) {
		result = 1;
	}
	
	if (result >= 0 && GET_HEALTH(vict) > GET_MAX_HEALTH(vict)) {
		set_health(vict, GET_MAX_HEALTH(vict));
	}
	
	// fire a consume trigger but it can't block execution here
	if (consume_otrigger(obj, ch, OCMD_POISON, (!EXTRACTED(vict) && !IS_DEAD(vict)) ? vict : NULL)) {
		if (GET_POISON_CHARGES(obj) <= 0) {
			run_interactions(ch, GET_OBJ_INTERACTIONS(obj), INTERACT_CONSUMES_TO, IN_ROOM(ch), NULL, obj, NULL, consumes_or_decays_interact);
			extract_obj(obj);
		}
	}
	
	// either way
	return result;
}


/**
* For the "use" command -- switches your poison.
*
* @param char_data *ch
* @param obj_data *obj the poison
*/
void use_poison(char_data *ch, obj_data *obj) {
	if (!has_player_tech(ch, PTECH_POISON)) {
		msg_to_char(ch, "You don't have the correct ability to use poisons.\r\n");
	}
	else if (!IS_POISON(obj)) {
		// ??? shouldn't ever get here
		act("$p isn't even a poison.", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else {
		USING_POISON(ch) = GET_OBJ_VNUM(obj);
		act("You are now using $p as your poison.", FALSE, ch, obj, NULL, TO_CHAR);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

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
	else if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (!IS_NPC(vict) && !IS_DISGUISED(vict)) {
		msg_to_char(ch, "You can only disguise yourself as an NPC.\r\n");
	}
	else if (IS_NPC(vict) && !MOB_FLAGGED(vict, MOB_HUMAN)) {
		act("You can't disguise yourself as $N!", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, NULL, ABIL_DISGUISE)) {
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
		
		set_disguise(ch, PERS(vict, vict, FALSE), GET_SEX(vict));
		gain_ability_exp(ch, ABIL_DISGUISE, 33.4);
		GET_WAIT_STATE(ch) = 4 RL_SEC;	// long wait
		
		run_ability_hooks(ch, AHOOK_ABILITY, ABIL_DISGUISE, 0, vict, NULL, NULL, NULL, NOBITS);
	}
}


ACMD(do_infiltrate) {
	room_data *to_room, *was_in;
	int dir;
	empire_data *emp;
	int cost = 10;

	skip_spaces(&argument);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use infiltrate.\r\n");
	}
	else if (!has_player_tech(ch, PTECH_INFILTRATE)) {
		msg_to_char(ch, "You don't have the correct ability to infiltrate.\r\n");
	}
	else if (!can_use_ability(ch, NO_ABIL, MOVE, cost, NOTHING)) {
		// sends own message
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_INFILTRATE, NULL, NULL, NULL)) {
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
	else if (IS_RIDING(ch) && !PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
		msg_to_char(ch, "You can't infiltrate while riding.\r\n");
	}
	else if (!ROOM_IS_CLOSED(to_room) || can_use_room(ch, to_room, GUESTS_ALLOWED))
		msg_to_char(ch, "You can just walk in.\r\n");
	else if (BUILDING_ENTRANCE(to_room) != dir && (!ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES) || BUILDING_ENTRANCE(to_room) != rev_dir[dir]))
		msg_to_char(ch, "You can only infiltrate at the entrance.\r\n");
	else if (!can_infiltrate(ch, (emp = ROOM_OWNER(to_room)))) {
		// sends own message
	}
	else {
		// auto-dismount
		if (IS_RIDING(ch)) {
			do_dismount(ch, "", 0, 0);
		}
		
		was_in = IN_ROOM(ch);
		charge_ability_cost(ch, MOVE, cost, NOTHING, 0, WAIT_ABILITY);
		
		gain_player_tech_exp(ch, PTECH_INFILTRATE, 50);
		gain_player_tech_exp(ch, PTECH_INFILTRATE_UPGRADE, 50);
		run_ability_hooks_by_player_tech(ch, PTECH_INFILTRATE, NULL, NULL, NULL, to_room);
		
		if (!has_player_tech(ch, PTECH_INFILTRATE_UPGRADE) && !player_tech_skill_check(ch, PTECH_INFILTRATE, (emp && EMPIRE_HAS_TECH(emp, TECH_LOCKS)) ? DIFF_RARELY : DIFF_HARD)) {
			msg_to_char(ch, "You fail.\r\n");
		}
		else if (!pre_greet_mtrigger(ch, to_room, dir, "move")) {
			// no message
		}
		else {
			char_from_room(ch);
			char_to_room(ch, to_room);
			qt_visit_room(ch, IN_ROOM(ch));
			look_at_room(ch);
			msg_to_char(ch, "\r\nInfiltration successful.\r\n");
			
			GET_LAST_DIR(ch) = dir;
			
			enter_wtrigger(IN_ROOM(ch), ch, NO_DIR, "move");
			entry_memory_mtrigger(ch);
			greet_mtrigger(ch, NO_DIR, "move");
			greet_memory_mtrigger(ch);
			greet_vtrigger(ch, NO_DIR, "move");
			greet_otrigger(ch, NO_DIR, "move");
	
			RESET_LAST_MESSAGED_TEMPERATURE(ch);
			msdp_update_room(ch);	// once we're sure we're staying
		}

		// chance to log
		if (emp && !player_tech_skill_check(ch, PTECH_INFILTRATE_UPGRADE, DIFF_HARD)) {
			if (has_player_tech(ch, PTECH_INFILTRATE_UPGRADE)) {
				log_to_empire(emp, ELOG_HOSTILITY, "An infiltrator has been spotted at (%d, %d)!", X_COORD(to_room), Y_COORD(to_room));
			}
			else {
				log_to_empire(emp, ELOG_HOSTILITY, "%s has been spotted infiltrating at (%d, %d)!", PERS(ch, ch, 0), X_COORD(to_room), Y_COORD(to_room));
			}
		}
		
		// distrust just in case
		if (emp) {
			trigger_distrust_from_stealth(ch, emp);
			add_offense(emp, OFFENSE_INFILTRATED, ch, IN_ROOM(ch), offense_was_seen(ch, emp, was_in) ? OFF_SEEN : NOBITS);
		}
	}
}


ACMD(do_pickpocket) {
	empire_data *ch_emp = NULL, *vict_emp = NULL;
	bool any, low_level;
	char_data *vict;
	int coins;

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		send_to_char("You have no idea how to do that.\r\n", ch);
	}
	else if (!has_player_tech(ch, PTECH_PICKPOCKET)) {
		msg_to_char(ch, "You don't have the correct ability to pickpocket anybody.\r\n");
	}
	else if (!can_use_ability(ch, NOTHING, NOTHING, 0, NOTHING)) {
		// sends own messages
	}
	else if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
		send_to_char("Pickpocket whom?\r\n", ch);
	}
	else if (vict == ch) {
		send_to_char("Come on now, that's rather stupid!\r\n", ch);
	}
	else if (!IS_NPC(vict)) {
		msg_to_char(ch, "You may only pickpocket NPCs.\r\n");
	}
	else if ((ch_emp = GET_LOYALTY(ch)) && (vict_emp = GET_LOYALTY(vict)) && has_relationship(ch_emp, vict_emp, DIPL_ALLIED | DIPL_NONAGGR)) {
		msg_to_char(ch, "You can't pickpocket mobs you are allied or have a non-aggression pact with.\r\n");
	}
	else if (ch_emp && vict_emp && GET_RANK(ch) < EMPIRE_PRIV(ch_emp, PRIV_STEALTH) && !has_relationship(ch_emp, vict_emp, DIPL_WAR | DIPL_THIEVERY)) {
		msg_to_char(ch, "You don't have permission to steal that -- you could start a war!\r\n");
	}
	else if (ch_emp && vict_emp && ch_emp != vict_emp && !PRF_FLAGGED(ch, PRF_STEALTHABLE) && !has_relationship(ch_emp, vict_emp, DIPL_WAR | DIPL_THIEVERY)) {
		msg_to_char(ch, "You cannot pickpocket that target because your 'stealthable' toggle is off.\r\n");
	}
	else if (FIGHTING(vict)) {
		msg_to_char(ch, "You can't steal from someone who's fighting.\r\n");
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_PICKPOCKET, vict, NULL, NULL)) {
		return;
	}
	else if (MOB_FLAGGED(vict, MOB_PICKPOCKETED | MOB_NO_LOOT) || (!MOB_FLAGGED(vict, MOB_COINS) && AFF_FLAGGED(vict, AFF_NO_ATTACK) && !has_interaction(vict->interactions, INTERACT_PICKPOCKET))) {
		act("$E doesn't appear to be carrying anything in $S pockets.", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else {
		check_scaling(vict, ch);

		// some random coins (negative coins are not given)
		if (MOB_FLAGGED(vict, MOB_COINS) && (!GET_LOYALTY(vict) || GET_LOYALTY(vict) == GET_LOYALTY(ch) || char_has_relationship(ch, vict, DIPL_WAR | DIPL_THIEVERY))) {
			coins = mob_coins(vict);
		}
		else {
			coins = 0;
		}
		vict_emp = GET_LOYALTY(vict);	// in case not set earlier
		
		low_level = (get_approximate_level(ch) + 50 < get_approximate_level(vict));
		
		if (!low_level && (!CAN_SEE(vict, ch) || !AWAKE(vict) || AFF_FLAGGED(vict, AFF_STUNNED | AFF_HARD_STUNNED) || player_tech_skill_check_by_ability_difficulty(ch, PTECH_PICKPOCKET))) {
			// success!
			set_mob_flags(vict, MOB_PICKPOCKETED);
			act("You pick $N's pocket...", FALSE, ch, NULL, vict, TO_CHAR);

			// any will tell us if we got at least 1 item (also sends messages)
			any = run_interactions(ch, vict->interactions, INTERACT_PICKPOCKET, IN_ROOM(ch), vict, NULL, NULL, pickpocket_interact);
			any |= run_global_mob_interactions(ch, vict, INTERACT_PICKPOCKET, pickpocket_interact);
			
			if (coins > 0) {
				increase_coins(ch, vict_emp, coins);
			}
			
			// messaging
			if (coins > 0) {
				if (ch->desc) {
					stack_msg_to_desc(ch->desc, "You find %s!\r\n", money_amount(vict_emp, coins));
				}
				act("$n pickpockets $N.", TRUE, ch, NULL, vict, TO_NOTVICT | TO_GROUP_ONLY);
			}
			else if (any) {
				act("$n pickpockets $N.", TRUE, ch, NULL, vict, TO_NOTVICT | TO_GROUP_ONLY);
			}
			else {
				if (ch->desc) {
					stack_msg_to_desc(ch->desc, "You find nothing of any use.\r\n");
				}
			}
		}
		else {
			// oops... put these back
			if (coins > 0 && GET_LOYALTY(vict)) {
				increase_empire_coins(GET_LOYALTY(vict), GET_LOYALTY(vict), coins);
			}
			
			if (!AWAKE(vict)) {
				wake_and_stand(vict);
			}
			
			// fail
			act("You try to pickpocket $N, but $E catches you!", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n tries to pick your pocket, but you catch $m in the act!", FALSE, ch, NULL, vict, TO_VICT);
			act("$n tries to pickpocket $N, but is caught!", FALSE, ch, NULL, vict, TO_NOTVICT);
			
			if (!AFF_FLAGGED(vict, AFF_NO_ATTACK)) {
				engage_combat(ch, vict, TRUE);
			}
		}
		
		if (vict_emp && vict_emp != ch_emp) {
			trigger_distrust_from_stealth(ch, vict_emp);
			add_offense(vict_emp, OFFENSE_PICKPOCKETED, ch, IN_ROOM(ch), offense_was_seen(ch, vict_emp, NULL) ? OFF_SEEN : NOBITS);
		}
		
		// gain either way
		if (can_gain_exp_from(ch, vict) && !AFF_FLAGGED(vict, AFF_NO_ATTACK)) {
			gain_player_tech_exp(ch, PTECH_PICKPOCKET, 25);
		}
		run_ability_hooks_by_player_tech(ch, PTECH_PICKPOCKET, vict, NULL, NULL, NULL);
		command_lag(ch, WAIT_ABILITY);
	}
}


ACMD(do_search) {
	char_data *targ;
	bool found = FALSE, earthmeld = FALSE;
	
	if (AFF_FLAGGED(ch, AFF_BLIND)) {
		msg_to_char(ch, "How can you do that? You're blind!\r\n");
	}
	else if (!has_player_tech(ch, PTECH_SEARCH_COMMAND) || !can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		// fruitless search
		act("$n begins searching around!", TRUE, ch, NULL, NULL, TO_ROOM);
		msg_to_char(ch, "You search, but find nobody.\r\n");
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_SEARCH_COMMAND, NULL, NULL, NULL)) {
		// failed trigger
	}
	else {
		act("$n begins searching around!", TRUE, ch, NULL, NULL, TO_ROOM);
		
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), targ, next_in_room) {
			if (ch == targ) {
				continue;
			}
			
			// hidden targets
			if (AFF_FLAGGED(targ, AFF_HIDDEN)) {
				if (has_player_tech(targ, PTECH_HIDE_UPGRADE)) {
					gain_player_tech_exp(targ, PTECH_HIDE_UPGRADE, 20);
					run_ability_hooks_by_player_tech(targ, PTECH_HIDE_UPGRADE, ch, NULL, NULL, NULL);
					continue;
				}

				SET_BIT(AFF_FLAGS(ch), AFF_SENSE_HIDDEN);

				if (player_tech_skill_check_by_ability_difficulty(ch, PTECH_SEARCH_COMMAND) && CAN_SEE(ch, targ)) {
					act("You find $N!", FALSE, ch, 0, targ, TO_CHAR);
					msg_to_char(targ, "You are discovered!\r\n");
					REMOVE_BIT(AFF_FLAGS(targ), AFF_HIDDEN);
					affects_from_char_by_aff_flag(targ, AFF_HIDDEN, FALSE);
					found = TRUE;
				}

				REMOVE_BIT(AFF_FLAGS(ch), AFF_SENSE_HIDDEN);
			}
			else if (!earthmeld && AFF_FLAGGED(targ, AFF_EARTHMELDED)) {
				// earthmelded targets (only do once)
				if (player_tech_skill_check_by_ability_difficulty(ch, PTECH_SEARCH_COMMAND) && CAN_SEE(ch, targ)) {
					act("You find signs that someone is earthmelded here.", FALSE, ch, NULL, NULL, TO_CHAR);
					found = earthmeld = TRUE;
				}
			}
		}

		if (!found)
			msg_to_char(ch, "You search, but find nobody.\r\n");

		charge_ability_cost(ch, NOTHING, 0, COOLDOWN_SEARCH, 10, WAIT_ABILITY);
		gain_player_tech_exp(ch, PTECH_SEARCH_COMMAND, 20);
		run_ability_hooks_by_player_tech(ch, PTECH_SEARCH_COMMAND, NULL, NULL, NULL, IN_ROOM(ch));
	}
}


ACMD(do_steal) {
	struct empire_storage_data *store, *next_store;
	empire_data *emp = ROOM_OWNER(HOME_ROOM(IN_ROOM(ch)));
	struct empire_island *isle;
	obj_data *proto;
	bool found = FALSE;
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot steal.\r\n");
	}
	else if (!has_player_tech(ch, PTECH_STEAL_COMMAND)) {
		msg_to_char(ch, "You can't steal anything.\r\n");
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_STEAL_COMMAND, NULL, NULL, NULL)) {
		// triggered
	}
	else if (!IS_IMMORTAL(ch) && !can_steal(ch, emp)) {
		// sends own message
	}
	else if (!emp) {
		msg_to_char(ch, "Nothing is stored here that you can steal.\r\n");
	}
	else if (GET_ISLAND_ID(IN_ROOM(ch)) == NO_ISLAND || !(isle = get_empire_island(emp, GET_ISLAND_ID(IN_ROOM(ch))))) {
		msg_to_char(ch, "You can't steal anything here.\r\n");
	}
	else if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CEDED)) {
		msg_to_char(ch, "You can't steal from a building which was ceded to an empire but never used by that empire.\r\n");
	}
	else if (!*arg) {
		if (!(show_local_einv(ch, IN_ROOM(ch), TRUE))) {
			msg_to_char(ch, "Nothing is stored here.\r\n");
		}
	}
	else {
		HASH_ITER(hh, isle->store, store, next_store) {
			if (found) {
				break;
			}
			
			proto = store->proto;
			
			if (store->amount > 0 && proto && obj_can_be_retrieved(proto, IN_ROOM(ch), NULL) && isname(arg, GET_OBJ_KEYWORDS(proto))) {
				found = TRUE;
				
				if (stored_item_requires_withdraw(proto) && !has_player_tech(ch, PTECH_STEAL_UPGRADE)) {
					msg_to_char(ch, "You can't steal that without Vaultcracking!\r\n");
					return;
				}
				else {
					// SUCCESS!
					if (IS_IMMORTAL(ch)) {
						syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s stealing %s from %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(proto), EMPIRE_NAME(emp));
					}
					else if (!player_tech_skill_check_by_ability_difficulty(ch, PTECH_STEAL_COMMAND)) {
						log_to_empire(emp, ELOG_HOSTILITY, "Theft at (%d, %d)", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
					}

					retrieve_resource(ch, emp, store, TRUE);
					gain_player_tech_exp(ch, PTECH_STEAL_COMMAND, 50);
				
					if (stored_item_requires_withdraw(proto)) {
						gain_player_tech_exp(ch, PTECH_STEAL_UPGRADE, 50);
					}

					read_vault(emp);
				
					GET_WAIT_STATE(ch) = 4 RL_SEC;	// long wait
					run_ability_hooks_by_player_tech(ch, PTECH_STEAL_COMMAND, NULL, NULL, NULL, NULL);
				}
			}
		}

		if (!found) {
			msg_to_char(ch, "Nothing like that is stored here!\r\n");
			return;
		}
	}
}

