/* ************************************************************************
*   File: act.naturalmagic.c                              EmpireMUD 2.0b5 *
*  Usage: implementation for natural magic abilities                      *
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
*   Potions
*   Commands
*/

// external vars

// external funcs
ACMD(do_dismount);
extern obj_data *find_obj(int n, bool error);
extern bool is_fight_ally(char_data *ch, char_data *frenemy);	// fight.c
extern bool is_fight_enemy(char_data *ch, char_data *frenemy);	// fight.c
void perform_resurrection(char_data *ch, char_data *rez_by, room_data *loc, any_vnum ability);
extern bool trigger_counterspell(char_data *ch);	// spells.c

// locals
char_data *has_familiar(char_data *ch);
void un_earthmeld(char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Returns the amount of healing to add if the player has Ancestral Healing and
* the Greatness attribute.
*
* @param char_data *ch The character to check for the ability.
* @return int The amount of healing to add (or 0 if none).
*/
int ancestral_healing(char_data *ch) {
	double mod, amt;
	
	if (!has_ability(ch, ABIL_ANCESTRAL_HEALING) || !check_solo_role(ch)) {
		return 0;
	}
	
	mod = get_approximate_level(ch) / 150.0;
	amt = round(GET_GREATNESS(ch) * mod);
	return MAX(0, amt);
}


/**
* Finds a familiar belonging to ch that has the matching vnum and despawns it.
* 
* @param char_data *ch The player to find familiars for.
* @param mob_vnum vnum Despawn only if matching this vnum (NOTHING for all familiars).
* @return bool TRUE if it despawned a mob; FALSE if not.
*/
bool despawn_familiar(char_data *ch, mob_vnum vnum) {
	char_data *iter, *next_iter;
	bool any = FALSE;
	
	for (iter = character_list; iter; iter = next_iter) {
		next_iter = iter->next;
		
		if (!IS_NPC(iter)) {
			continue;
		}
		if (!MOB_FLAGGED(iter, MOB_FAMILIAR)) {
			continue;
		}
		if (iter->master != ch) {
			continue;
		}
		
		if (vnum != NOTHING && GET_MOB_VNUM(iter) != vnum) {
			continue;
		}
		
		act("$n leaves.", TRUE, iter, NULL, NULL, TO_ROOM);
		extract_char(iter);
		any = TRUE;
	}
	
	return any;
}


/**
* @param char_data *ch The person.
* @return int The total Bonus-Healing trait for that person, with any modifiers.
*/
int total_bonus_healing(char_data *ch) {
	return GET_BONUS_HEALING(ch) + ancestral_healing(ch);
}


/**
* @param char_data *ch
* @return char_data *the familiar if one exists, or NULL
*/
char_data *has_familiar(char_data *ch) {
	char_data *ch_iter, *found;
	
	// check existing familiars
	found = NULL;
	for (ch_iter = character_list; !found && ch_iter; ch_iter = ch_iter->next) {
		if (IS_NPC(ch_iter) && MOB_FLAGGED(ch_iter, MOB_FAMILIAR) && ch_iter->master == ch) {
			found = ch_iter;
		}
	}
	
	return found;
}


/**
* Sends an update to the healer on the status of the healee.
*
* @param char_data *healed Person who was healed.
* @param int amount The amount healed.
* @param char_data *report_to The person to send the message to (the healer).
*/
void report_healing(char_data *healed, int amount, char_data *report_to) {
	char buf[MAX_STRING_LENGTH];
	size_t size;
	
	size = snprintf(buf, sizeof(buf), "You healed %s for %d (", (healed == report_to) ? "yourself" : PERS(healed, report_to, FALSE), amount);
	
	if (is_fight_enemy(healed, report_to) || (IS_NPC(healed) && !is_fight_ally(healed, report_to))) {
		size += snprintf(buf + size, sizeof(buf) - size, "%.1f%% health).\r\n", (GET_HEALTH(healed) * 100.0 / MAX(1, GET_MAX_HEALTH(healed))));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, "%d/%d health).\r\n", GET_HEALTH(healed), GET_MAX_HEALTH(healed));
	}
	
	msg_to_char(report_to, "%s", buf);
}


/**
* Ends an earthmeld and sends messages.
*
* @param char_data *ch The earthmelded one.
*/
void un_earthmeld(char_data *ch) {
	if (AFF_FLAGGED(ch, AFF_EARTHMELD)) {
		affects_from_char_by_aff_flag(ch, AFF_EARTHMELD, FALSE);
		if (!AFF_FLAGGED(ch, AFF_EARTHMELD)) {
			msg_to_char(ch, "You rise from the ground!\r\n");
			act("$n rises from the ground!", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_STANDING;
			add_cooldown(ch, COOLDOWN_EARTHMELD, 2 * SECS_PER_REAL_MIN);
			
			if (affected_by_spell(ch, ATYPE_NATURE_BURN)) {
				affect_from_char(ch, ATYPE_NATURE_BURN, TRUE);
				command_lag(ch, WAIT_ABILITY);
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// POTIONS /////////////////////////////////////////////////////////////////

/**
* Apply the actual effects of a potion. This does NOT extract the potion.
*
* @param obj_data *obj the potion
* @param char_data *ch the quaffer
*/
void apply_potion(obj_data *obj, char_data *ch) {
	void scale_item_to_level(obj_data *obj, int level);
	
	any_vnum aff_type = GET_POTION_AFFECT(obj) != NOTHING ? GET_POTION_AFFECT(obj) : ATYPE_POTION;
	struct affected_type *af;
	struct obj_apply *apply;
	
	act("A swirl of light passes over you!", FALSE, ch, NULL, NULL, TO_CHAR);
	act("A swirl of light passes over $n!", FALSE, ch, NULL, NULL, TO_ROOM);
	
	// ensure scaled
	if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
		scale_item_to_level(obj, 1);	// minimum level
	}
	
	// remove any old buffs (if adding a new one)
	if (GET_OBJ_AFF_FLAGS(obj) || GET_OBJ_APPLIES(obj)) {
		affect_from_char(ch, aff_type, FALSE);
	}
	
	if (GET_OBJ_AFF_FLAGS(obj)) {
		af = create_flag_aff(aff_type, 24 MUD_HOURS, GET_OBJ_AFF_FLAGS(obj), ch);
		affect_to_char(ch, af);
		free(af);
	}

	LL_FOREACH(GET_OBJ_APPLIES(obj), apply) {
		af = create_mod_aff(aff_type, 24 MUD_HOURS, apply->location, apply->modifier, ch);
		affect_to_char(ch, af);
		free(af);
	}
	
	if (GET_POTION_COOLDOWN_TYPE(obj) != NOTHING && GET_POTION_COOLDOWN_TIME(obj) > 0) {
		add_cooldown(ch, GET_POTION_COOLDOWN_TYPE(obj), GET_POTION_COOLDOWN_TIME(obj));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_cleanse) {
	extern const bool aff_is_bad[];
	extern const double apply_values[];
	
	struct over_time_effect_type *dot, *next_dot;
	struct affected_type *aff, *next_aff;
	bitvector_t bitv;
	char_data *vict = ch;
	int pos, cost = 30;
	bool done_aff;
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_CLEANSE, MANA, cost, COOLDOWN_CLEANSE)) {
		return;
	}
	else if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_CLEANSE)) {
		return;
	}
	else {
		charge_ability_cost(ch, MANA, cost, COOLDOWN_CLEANSE, 9, WAIT_SPELL);
		
		if (ch == vict) {
			msg_to_char(ch, "You shine brightly as your mana cleanses you.\r\n");
			act("$n shines brightly for a moment as $s mana cleanses $m.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			act("You shoot a bolt of white mana at $N, cleansing $M.", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n shoots a bolt of white mana at you, cleansing you.", FALSE, ch, NULL, vict, TO_VICT);
			act("$n shoots a bolt of white mana at $N, cleansing $M.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}

		// remove bad effects
		for (aff = vict->affected; aff; aff = next_aff) {
			next_aff = aff->next;
			
			// can't cleanse penalties (things cast by self)
			if (aff->cast_by == CAST_BY_ID(vict)) {
				continue;
			}
			
			done_aff = FALSE;
			if (aff->location != APPLY_NONE && (apply_values[(int) aff->location] == 0.0 || aff->modifier < 0)) {
				affect_remove(vict, aff);
				done_aff = TRUE;
			}
			if (!done_aff && (bitv = aff->bitvector) != NOBITS) {
				// check each bit
				for (pos = 0; bitv && !done_aff; ++pos, bitv >>= 1) {
					if (IS_SET(bitv, BIT(0)) && aff_is_bad[pos]) {
						affect_remove(vict, aff);
						done_aff = TRUE;
					}
				}
			}
		}
		
		// remove DoTs
		for (dot = vict->over_time_effects; dot; dot = next_dot) {
			next_dot = dot->next;
			
			// can't cleanse penalties (things cast by self)
			if (dot->cast_by == CAST_BY_ID(vict)) {
				continue;
			}

			if (dot->damage_type == DAM_PHYSICAL || dot->damage_type == DAM_POISON) {
				dot_remove(vict, dot);
			}
		}

		if (!IS_NPC(vict) && GET_COND(vict, DRUNK) > 0) {
			gain_condition(vict, DRUNK, -1 * GET_COND(vict, DRUNK));
		}
		
		if (can_gain_exp_from(ch, vict)) {
			gain_ability_exp(ch, ABIL_CLEANSE, 33.4);
		}

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
	}
}


ACMD(do_confer) {
	extern const double apply_values[];
	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	struct affected_type *aff, *aff_iter;
	bool any, found_existing, found_ch;
	int amt, iter, abbrev, type, conferred_amt, avail_str;
	int match_duration = 0;
	char_data *vict = ch;
	
	// configs
	int duration = 6 * REAL_UPDATES_PER_MIN;
	int cost = 50;

	struct {
		char *name;
		int apply;
	} confer_list[] = {
		{ "block", APPLY_BLOCK },
		{ "dodge", APPLY_DODGE },
		{ "health", APPLY_HEALTH },
		{ "health-regen", APPLY_HEALTH_REGEN },
		{ "mana", APPLY_MANA },
		{ "mana-regen", APPLY_MANA_REGEN },
		{ "move", APPLY_MOVE },
		{ "move-regen", APPLY_MOVE_REGEN },
		{ "resist-magical", APPLY_RESIST_MAGICAL },
		{ "resist-physical", APPLY_RESIST_PHYSICAL },
		{ "to-hit", APPLY_TO_HIT },
		
		// last
		{ "\n", APPLY_NONE }
	};
	
	two_arguments(argument, arg1, arg2);
	
	if (!can_use_ability(ch, ABIL_CONFER, MANA, cost, NOTHING)) {
		return;
	}
	
	if (!*arg1) {
		msg_to_char(ch, "Usage: confer <trait> [person]\r\n");
		msg_to_char(ch, "You know how to confer:");
		any = FALSE;
		for (iter = 0; *confer_list[iter].name != '\n'; ++iter) {
			msg_to_char(ch, "%s%s", (any ? ", " : " "), confer_list[iter].name);
			any = TRUE;
		}
		if (!any) {
			msg_to_char(ch, " nothing");
		}
		msg_to_char(ch, "\r\n");
		return;
	}

	// optional 2nd arg
	if (*arg2 && !(vict = get_char_vis(ch, arg2, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
		return;
	}
	
	// compute max confer
	conferred_amt = 0;
	LL_FOREACH(vict->affected, aff) {
		if (aff->type == ATYPE_CONFER && aff->cast_by == GET_IDNUM(ch)) {
			conferred_amt += apply_values[(int) aff->location] / apply_values[APPLY_STRENGTH];
		}
	}
	avail_str = GET_STRENGTH(ch);
	LL_FOREACH(ch->affected, aff) {
		if (aff->type == ATYPE_CONFERRED && aff->location == APPLY_STRENGTH) {
			avail_str += (-1 * aff->modifier);
		}
	}
	if (conferred_amt >= avail_str || conferred_amt >= att_max(ch)) {
		msg_to_char(ch, "You cannot confer any more strength on %s.\r\n", (ch == vict) ? "yourself" : PERS(vict, vict, FALSE));
		return;
	}
	
	// find type in list: prefer exact match
	type = abbrev = NOTHING;
	for (iter = 0; *confer_list[iter].name != '\n'; ++iter) {
		if (!str_cmp(arg1, confer_list[iter].name)) {
			type = iter;
			break;
		}
		else if (is_abbrev(arg1, confer_list[iter].name)) {
			abbrev = iter;
		}
	}
	if (type == NOTHING) {
		type = abbrev;	// no exact match
	}
	
	// pre-validation complete
	if (GET_STRENGTH(ch) < 2) {
		msg_to_char(ch, "You have no strength to confer.\r\n");
	}
	else if (type == NOTHING || confer_list[type].apply == APPLY_NONE) {
		msg_to_char(ch, "You don't know how to confer '%s'.\r\n", arg1);
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_CONFER)) {
		return;
	}
	else {
		// good to go
		charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
		
		// messaging
		if (ch == vict) {
			msg_to_char(ch, "You confer your own strength into %s!\r\n", confer_list[type].name);
			// no message to room!
		}
		else {
			sprintf(buf, "You confer your strength into $N's %s!", confer_list[type].name);
			act(buf, FALSE, ch, NULL, vict, TO_CHAR);
			sprintf(buf, "$n confers $s strength into your %s!", confer_list[type].name);
			act(buf, FALSE, ch, NULL, vict, TO_VICT);
			act("$n confers $s strength into $N!", FALSE, ch, NULL, vict, TO_NOTVICT);
		}
		
		// determine how much to give: based on what a point of strength is worth
		amt = round(apply_values[APPLY_STRENGTH] / apply_values[confer_list[type].apply]);
		amt = MAX(amt, 1);	// ensure at least 1 point of stuff
		
		// attempt to find an existing confer effect that matches and just add to its amount
		found_existing = found_ch = FALSE;
		for (aff_iter = vict->affected; aff_iter; aff_iter = aff_iter->next) {
			if (aff_iter->type == ATYPE_CONFER && aff_iter->cast_by == CAST_BY_ID(ch) && aff_iter->location == confer_list[type].apply) {
				found_existing = TRUE;
				aff_iter->modifier += amt;
				match_duration = aff_iter->duration;	// store this to match it later
				aff_iter->duration = duration;	// reset to max duration

				// ensure stats are correct
				affect_modify(vict, aff_iter->location, amt, NOBITS, TRUE);
				affect_total(vict);
				break;
			}
		}
		
		if (found_existing) {
			// if there was an existing aff on the target, maybe there is one on ch
			found_ch = FALSE;
			for (aff_iter = ch->affected; aff_iter; aff_iter = aff_iter->next) {
				// we match by duration because lengthening any -str affect that had the same duration is equally good
				if (aff_iter->type == ATYPE_CONFERRED && aff_iter->duration == match_duration) {
					found_ch = TRUE;
					aff_iter->modifier -= 1;	// additional -1 strength
					aff_iter->duration = duration;	// reset to max duration

					// ensure stats are correct
					affect_modify(ch, aff_iter->location, -1, NOBITS, TRUE);
					affect_total(ch);
					break;
				}
			}
		}
		else {
			// did not find existing: add if needed
			aff = create_mod_aff(ATYPE_CONFER, duration, confer_list[type].apply, amt, ch);
			
			// use affect_to_char instead of affect_join because we will allow multiple copies of this with different durations
			affect_to_char(vict, aff);
			free(aff);
		}
		
		// separately ...
		if (!found_ch) {
			// need a new strength effect on ch
			aff = create_mod_aff(ATYPE_CONFERRED, duration, APPLY_STRENGTH, -1, ch);
			
			// use affect_to_char instead of affect_join because we will allow multiple copies of this with different durations
			affect_to_char(ch, aff);
			free(aff);
		}
	}
}


ACMD(do_counterspell) {
	struct affected_type *af;
	int cost = 15;
	
	if (!can_use_ability(ch, ABIL_COUNTERSPELL, MANA, cost, NOTHING)) {
		return;
	}
	else if (affected_by_spell(ch, ATYPE_COUNTERSPELL)) {
		msg_to_char(ch, "You already have a counterspell ready.\r\n");
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_COUNTERSPELL)) {
		return;
	}
	else {
		charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
		
		msg_to_char(ch, "You ready a counterspell.\r\n");
		act("$n flickers momentarily with a blue-white aura.", TRUE, ch, NULL, NULL, TO_ROOM);
		
		af = create_flag_aff(ATYPE_COUNTERSPELL, 1 MUD_HOURS, 0, ch);
		affect_join(ch, af, 0);
	}
}


ACMD(do_earthmeld) {
	struct affected_type *af;
	int cost = 50;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs may not earthmeld.\r\n");
		return;
	}

	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_EARTHMELD)) {
		return;
	}

	if (AFF_FLAGGED(ch, AFF_EARTHMELD)) {
		// only check sector on rise if the person has earth mastery, otherwise they are trapped
		if (has_ability(ch, ABIL_WORM) && IS_ANY_BUILDING(IN_ROOM(ch)) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_OPEN)) {
			msg_to_char(ch, "You can't rise from the earth here!\r\n");
		}
		else {
			un_earthmeld(ch);
			look_at_room(ch);
		}
		return;
	}

	// sect validation
	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_SHALLOW_WATER | SECTF_INSIDE) || (GET_BUILDING(IN_ROOM(ch)) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_OPEN))) {
		msg_to_char(ch, "You can't earthmeld without natural ground below you!\r\n");
		return;
	}
	
	if (SECT_FLAGGED(BASE_SECT(IN_ROOM(ch)), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_SHALLOW_WATER | SECTF_INSIDE)) {
		msg_to_char(ch, "You can't earthmeld without solid ground below you!\r\n");
		return;
	}
	
	if (IS_ADVENTURE_ROOM(IN_ROOM(ch)) && (!RMT_FLAGGED(IN_ROOM(ch), RMT_OUTDOOR) || ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_NEED_BOAT) || RMT_FLAGGED(IN_ROOM(ch), RMT_NEED_BOAT))) {
		msg_to_char(ch, "You can't earthmeld without natural ground below you!\r\n");
		return;
	}

	if (!can_use_ability(ch, ABIL_EARTHMELD, MANA, cost, COOLDOWN_EARTHMELD)) {
		return;
	}
	
	if (GET_POS(ch) == POS_FIGHTING) {
		msg_to_char(ch, "You can't do that while fighting!\r\n");
		return;
	}
	if (GET_POS(ch) < POS_STANDING) {
		msg_to_char(ch, "You can't do that right now. You need to be standing.\r\n");
		return;
	}
	
	if (IS_RIDING(ch)) {
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "You can't do that while mounted.\r\n");
			return;
		}
	}

	GET_MANA(ch) -= cost;
	
	msg_to_char(ch, "You dissolve into pure mana and sink into the ground!\r\n");
	act("$n dissolves into pure mana and sinks right into the ground!", TRUE, ch, 0, 0, TO_ROOM);
	GET_POS(ch) = POS_SLEEPING;

	af = create_aff(ATYPE_EARTHMELD, -1, APPLY_NONE, 0, AFF_NO_TARGET_IN_ROOM | AFF_NO_SEE_IN_ROOM | AFF_EARTHMELD, ch);
	affect_join(ch, af, 0);
	
	gain_ability_exp(ch, ABIL_EARTHMELD, 15);
}


ACMD(do_entangle) {
	char_data *vict = NULL;
	struct affected_type *af;
	int cost = 20;
	
	if (!can_use_ability(ch, ABIL_ENTANGLE, MANA, cost, COOLDOWN_ENTANGLE)) {
		return;
	}

	// find target
	one_argument(argument, arg);
	if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
		return;
	}
	if (!*arg && !(vict = FIGHTING(ch))) {
		msg_to_char(ch, "Who would you like to cast that at?\r\n");
		return;
	}
	if (ch == vict) {
		msg_to_char(ch, "You wouldn't want to cast that on yourself.\r\n");
		return;
	}
	
	// check validity
	if (!can_fight(ch, vict)) {
		act("You can't attack $M!", FALSE, ch, NULL, vict, TO_CHAR);
		return;
	}
	
	if (NOT_MELEE_RANGE(ch, vict)) {
		msg_to_char(ch, "You need to be at melee range to do this.\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_ENTANGLE)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, COOLDOWN_ENTANGLE, 30, WAIT_COMBAT_SPELL);
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// counterspell??
	if (trigger_counterspell(vict) || AFF_FLAGGED(vict, AFF_IMMUNE_NATURAL_MAGIC)) {
		act("You send out vines of green mana to entangle $N, but they can't seem to grasp $M.", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n sends out vines of green mana to entangle you, but they can't seem to latch on.", FALSE, ch, NULL, vict, TO_VICT);
		act("$n sends out vines of green mana to entangle $N, but they can't seem to grasp $M.", FALSE, ch, NULL, vict, TO_NOTVICT);
	}
	else {
		// succeed
	
		act("You shoot out vines of green mana, which entangle $N!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n shoots vines of green mana at you, entangling you!", FALSE, ch, NULL, vict, TO_VICT);
		act("$n shoots vines of green mana at $N, entangling $M!", FALSE, ch, NULL, vict, TO_NOTVICT);
	
		af = create_aff(ATYPE_ENTANGLE, 6, APPLY_DEXTERITY, -1, AFF_ENTANGLED, ch);
		affect_join(vict, af, 0);

		engage_combat(ch, vict, TRUE);
		
		// release other entangleds here
		limit_crowd_control(vict, ATYPE_ENTANGLE);
	}
	
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, ABIL_ENTANGLE, 15);
	}
}


ACMD(do_familiar) {
	void scale_mob_as_familiar(char_data *mob, char_data *master);
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	ability_data *abil = NULL;
	char_data *mob;
	int iter, type;
	bool any;
	
	struct {
		char *name;
		any_vnum ability;
		int level;	// natural magic level required
		mob_vnum vnum;
		int cost;
	} familiars[] = {
		// base familiars
		{ "cat", ABIL_FAMILIAR, 0, FAMILIAR_CAT, 40 },
		{ "saber-toothed cat", ABIL_FAMILIAR, 51, FAMILIAR_SABERTOOTH, 40 },
		{ "sphinx", ABIL_FAMILIAR, 76, FAMILIAR_SPHINX, 40 },
		{ "giant tortoise", ABIL_FAMILIAR, 100, FAMILIAR_GIANT_TORTOISE, 40 },
		
		// class animals
		{ "griffin", ABIL_GRIFFIN, 100, FAMILIAR_GRIFFIN, 40 },
		{ "dire wolf", ABIL_DIRE_WOLF, 100, FAMILIAR_DIRE_WOLF, 40 },
		{ "moon rabbit", ABIL_MOON_RABBIT, 100, FAMILIAR_MOON_RABBIT, 40 },
		{ "spirit wolf", ABIL_SPIRIT_WOLF, 100, FAMILIAR_SPIRIT_WOLF, 40 },
		{ "manticore", ABIL_MANTICORE, 100, FAMILIAR_MANTICORE, 40 },
		{ "phoenix", ABIL_PHOENIX, 100, FAMILIAR_PHOENIX, 40 },
		{ "scorpion shadow", ABIL_SCORPION_SHADOW, 100, FAMILIAR_SCORPION_SHADOW, 40 },
		{ "owl shadow", ABIL_OWL_SHADOW, 100, FAMILIAR_OWL_SHADOW, 40 },
		{ "basilisk", ABIL_BASILISK, 100, FAMILIAR_BASILISK, 40 },
		{ "salamander", ABIL_SALAMANDER, 100, FAMILIAR_SALAMANDER, 40 },
		{ "skeletal hulk", ABIL_SKELETAL_HULK, 100, FAMILIAR_SKELETAL_HULK, 40 },
		{ "banshee", ABIL_BANSHEE, 100, FAMILIAR_BANSHEE, 40 },

		{ "\n", NO_ABIL, 0, NOTHING, 0 }
	};
	
	if (has_familiar(ch)) {
		msg_to_char(ch, "You can't summon a familiar while you already have one.\r\n");
		return;
	}
	
	skip_spaces(&argument);
	
	// no-arg: just list
	if (!*argument) {
		msg_to_char(ch, "Summon which familiar:");
		any = FALSE;
		for (iter = 0; *familiars[iter].name != '\n'; ++iter) {
			if (!IS_NPC(ch) && familiars[iter].ability != NO_ABIL && !has_ability(ch, familiars[iter].ability)) {
				continue;
			}
			abil = find_ability_by_vnum(familiars[iter].ability);
			if (abil && ABIL_ASSIGNED_SKILL(abil) != NULL && get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) < familiars[iter].level) {
				continue;
			}
			if (!abil && GET_SKILL_LEVEL(ch) < familiars[iter].level) {
				continue;
			}
			
			msg_to_char(ch, "%s%s", any ? ", " : " ", familiars[iter].name);
			any = TRUE;
		}
		
		if (!any) {
			msg_to_char(ch, " (you know of none)");
		}
		msg_to_char(ch, "\r\n");
		
		return;
	}
	
	// find which one they wanted
	type = -1;
	for (iter = 0; *familiars[iter].name != '\n'; ++iter) {
		if (!IS_NPC(ch) && familiars[iter].ability != NO_ABIL && !has_ability(ch, familiars[iter].ability)) {
			continue;
		}
		abil = find_ability_by_vnum(familiars[iter].ability);
		if (abil && ABIL_ASSIGNED_SKILL(abil) != NULL && get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) < familiars[iter].level) {
			continue;
		}
		if (!abil && GET_SKILL_LEVEL(ch) < familiars[iter].level) {
			continue;
		}
		if (is_abbrev(argument, familiars[iter].name)) {
			type = iter;
			break;
		}
	}
	
	if (type == -1) {
		msg_to_char(ch, "Unknown familiar.\r\n");
		return;
	}
	
	if (!can_use_ability(ch, familiars[type].ability, MANA, familiars[type].cost, NOTHING)) {
		return;
	}
	
	if (familiars[type].ability != NO_ABIL && ABILITY_TRIGGERS(ch, NULL, NULL, familiars[type].ability)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, familiars[type].cost, NOTHING, 0, WAIT_SPELL);
	mob = read_mobile(familiars[type].vnum, TRUE);
	SET_BIT(MOB_FLAGS(mob), MOB_NO_EXPERIENCE);
	if (IS_NPC(ch)) {
		MOB_INSTANCE_ID(mob) = MOB_INSTANCE_ID(ch);
		if (MOB_INSTANCE_ID(mob) != NOTHING) {
			add_instance_mob(real_instance(MOB_INSTANCE_ID(mob)), GET_MOB_VNUM(mob));
		}
	}
	setup_generic_npc(mob, GET_LOYALTY(ch), NOTHING, NOTHING);
	
	// scale to summoner
	scale_mob_as_familiar(mob, ch);
	
	char_to_room(mob, IN_ROOM(ch));
	
	act("You send up a jet of sparkling blue mana and $N appears!", FALSE, ch, NULL, mob, TO_CHAR);
	act("$n sends up a jet of sparkling blue mana and $N appears!", FALSE, ch, NULL, mob, TO_NOTVICT);
	
	SET_BIT(AFF_FLAGS(mob), AFF_CHARM);
	SET_BIT(MOB_FLAGS(mob), MOB_FAMILIAR);
	add_follower(mob, ch, TRUE);
	
	if (familiars[type].ability != NO_ABIL) {
		gain_ability_exp(ch, familiars[type].ability, 33.4);
	}
	
	load_mtrigger(mob);
}


/**
* do_heal connects several abilities: ABIL_HEAL, ABIL_HEAL_FRIEND,
* and ABIL_HEAL_PARTY. Restrictions are based on which of these the player
* has purchased.
*/
ACMD(do_heal) {
	char_data *vict = ch, *ch_iter, *next_ch;
	bool party = FALSE, will_gain = FALSE;
	int cost, abil = NO_ABIL, gain = 15, amount, bonus;
	
	int heal_levels[] = { 15, 25, 35 };
	double intel_bonus[] = { 0.5, 1.5, 2.0 };
	double base_cost_ratio = 0.75;	// multiplied by amount healed
	double party_cost = 1.25;
	double self_cost = 0.75;
	
	// Healer ability features
	double healer_cost_ratio[] = { 0.75, 0.5, 0.25 };	// multiplied by amount healed
	double healer_level_bonus[] = { 0.5, 1.0, 1.5 };	// times levels over 100
	
	one_argument(argument, arg);
	
	// targeting
	if (!*arg) {
		vict = ch;
	}
	else if (!str_cmp(arg, "party") || !str_cmp(arg, "group")) {
		party = TRUE;
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
		return;
	}
	
	// target validation
	if (!party && ch == vict && !can_use_ability(ch, ABIL_HEAL, NOTHING, 0, NOTHING)) {
		return;
	}
	else if (!party && ch != vict && !can_use_ability(ch, ABIL_HEAL_FRIEND, NOTHING, 0, NOTHING)) {
		return;
	}
	else if (party && !can_use_ability(ch, ABIL_HEAL_PARTY, NOTHING, 0, NOTHING)) {
		return;
	}
	
	if (!party && GET_HEALTH(vict) >= GET_MAX_HEALTH(vict)) {
		msg_to_char(ch, "There aren't any wounds to heal!\r\n");
		return;
	}
	
	if (!party && IS_DEAD(vict)) {
		msg_to_char(ch, "Unfortunately you can't heal someone who is already dead.\r\n");
		return;
	}
	if (!party && is_fight_enemy(ch, vict)) {
		msg_to_char(ch, "You can't heal an enemy like that.\r\n");
		return;
	}
	if (party && !GROUP(ch)) {
		msg_to_char(ch, "You need to be in a group in order to do this.\r\n");
		return;
	}
	
	// determine cost
	if (party) {
		abil = ABIL_HEAL_PARTY;
		gain = 25;
	}
	else if (ch == vict) {
		abil = ABIL_HEAL;
	}
	else {
		// ch != vict
		abil = ABIL_HEAL_FRIEND;
	}

	// amount to heal will determine the cost
	amount = CHOOSE_BY_ABILITY_LEVEL(heal_levels, ch, abil) + (GET_INTELLIGENCE(ch) * CHOOSE_BY_ABILITY_LEVEL(intel_bonus, ch, abil));
	if (!IS_NPC(ch) && (GET_CLASS_ROLE(ch) == ROLE_HEALER || GET_CLASS_ROLE(ch) == ROLE_SOLO || has_player_tech(ch, PTECH_HEALING_BOOST)) && check_solo_role(ch)) {
		amount += (MAX(0, get_approximate_level(ch) - 100) * CHOOSE_BY_ABILITY_LEVEL(healer_level_bonus, ch, abil));
	}
	bonus = total_bonus_healing(ch);
	
	if (vict && !party) {
		// subtract bonus-healing because it will be re-added at the end
		amount = MIN(amount, GET_MAX_HEALTH(vict) - GET_HEALTH(vict) - bonus);
		amount = MAX(1, amount);
	}
	
	if (!IS_NPC(ch) && (GET_CLASS_ROLE(ch) == ROLE_HEALER || GET_CLASS_ROLE(ch) == ROLE_SOLO || has_player_tech(ch, PTECH_HEALING_BOOST)) && check_solo_role(ch)) {
		cost = amount * CHOOSE_BY_ABILITY_LEVEL(healer_cost_ratio, ch, abil);
	}
	else {
		cost = amount * base_cost_ratio;
	}
	
	// bonus healing does not add to cost
	amount += bonus;
	
	if (party) {
		cost = round(cost * party_cost);
	}
	else if (ch == vict) {
		cost = round(cost * self_cost);
	}
	
	cost = MAX(1, cost);

	// cost check	
	if (!can_use_ability(ch, abil, MANA, cost, NOTHING)) {
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, abil)) {
		return;
	}
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// done!
	charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
	
	if (party) {
		msg_to_char(ch, "You muster up as much mana as you can and send out a shockwave, healing the entire party!\r\n");
		act("$n draws up as much mana as $e can and sends it out in a shockwave, healing $s entire party!", FALSE, ch, NULL, NULL, TO_ROOM);
		for (ch_iter = ROOM_PEOPLE(IN_ROOM(ch)); ch_iter; ch_iter = next_ch) {
			next_ch = ch_iter->next_in_room;
			
			if (!IS_DEAD(ch_iter) && in_same_group(ch, ch_iter)) {
				msg_to_char(ch_iter, "You are healed!\r\n");
				heal(ch, ch_iter, amount * 0.75);
				
				if (FIGHTING(ch_iter) && !FIGHTING(ch)) {
					engage_combat(ch, FIGHTING(ch_iter), FALSE);
				}
				
				report_healing(ch_iter, amount * 0.75, ch);
				
				will_gain |= can_gain_exp_from(ch, ch_iter);
			}
		}
	}
	else {
		// not party
		if (ch == vict) {
			msg_to_char(ch, "You swirl your mana around your body to heal your wounds.\r\n");
			act("$n's body swirls with mana and $s wounds seem to heal.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			act("You let your mana pulse and wave around $N, healing $S wounds.", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n sends forth a wave of mana, which pulses through your body and heals your wounds.", FALSE, ch, NULL, vict, TO_VICT);
			act("A wave of mana shoots from $n to $N, healing $S wounds.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}
		
		heal(ch, vict, amount);

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
		
		report_healing(vict, amount, ch);
		
		will_gain = can_gain_exp_from(ch, vict);
	}
	
	if (abil != NO_ABIL && will_gain) {
		gain_ability_exp(ch, abil, gain);
		gain_ability_exp(ch, ABIL_ANCESTRAL_HEALING, gain);	// triggers on all heals
	}
}


ACMD(do_moonrise) {
	void death_restore(char_data *ch);
	
	obj_data *corpse;
	char_data *vict;
	int cost = 200;
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_MOONRISE, MANA, cost, COOLDOWN_MOONRISE)) {
		// sends own messages
	}
	else if (!*arg) {
		msg_to_char(ch, "Use Moonrise to resurrect whom?\r\n");
	}
	else if ((vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		if (!IS_DEAD(vict)) {
			msg_to_char(ch, "You can only resurrect a dead person.\r\n");
		}
		else if (IS_NPC(vict)) {
			msg_to_char(ch, "You can only resurrect players, not NPCs.\r\n");
		}
		else if (GET_ACCOUNT(ch) == GET_ACCOUNT(vict)) {
			msg_to_char(ch, "You can't resurrect your own alts.\r\n");
		}
		else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_MOONRISE)) {
			return;
		}
		else {
			// success: resurrect dead target
			charge_ability_cost(ch, MANA, cost, COOLDOWN_MOONRISE, 20 * SECS_PER_REAL_MIN, WAIT_SPELL);
			msg_to_char(ch, "You let out a bone-chilling howl...\r\n");
			act("$n lets out a bone-chilling howl...", FALSE, ch, NULL, NULL, TO_ROOM);
			act("$O is attempting to resurrect you (use 'accept/reject resurrection').", FALSE, vict, NULL, ch, TO_CHAR | TO_NODARK);
			add_offer(vict, ch, OFFER_RESURRECTION, ABIL_MOONRISE);
		}
	}
	else if ((corpse = get_obj_in_list_vis(ch, arg, ch->carrying)) || (corpse = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		// obj target
		if (!IS_CORPSE(corpse)) {
			msg_to_char(ch, "You can't resurrect that.\r\n");
		}
		else if (!IS_PC_CORPSE(corpse)) {
			msg_to_char(ch, "You can only resurrect player corpses.\r\n");
		}
		else if (!(vict = is_playing(GET_CORPSE_PC_ID(corpse)))) {
			msg_to_char(ch, "You can only resurrect the corpse of someone who is still playing.\r\n");
		}
		else if (vict == ch) {
			msg_to_char(ch, "You can't resurrect your own corpse, that's just silly.\r\n");
		}
		else if (GET_ACCOUNT(ch) == GET_ACCOUNT(vict)) {
			msg_to_char(ch, "You can't resurrect your own alts.\r\n");
		}
		else if (IS_DEAD(vict) || corpse != find_obj(GET_LAST_CORPSE_ID(vict), FALSE) || !IS_CORPSE(corpse)) {
			// victim has died AGAIN
			act("You can only resurrect $N using $S most recent corpse.", FALSE, ch, NULL, vict, TO_CHAR | TO_NODARK);
		}
		else {
			// seems legit...
			charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
			msg_to_char(ch, "You let out a bone-chilling howl...\r\n");
			act("$n lets out a bone-chilling howl...", FALSE, ch, NULL, NULL, TO_ROOM);
			act("$O is attempting to resurrect you (use 'accept/reject resurrection').", FALSE, vict, NULL, ch, TO_CHAR | TO_NODARK);
			add_offer(vict, ch, OFFER_RESURRECTION, ABIL_MOONRISE);
		}
	}
	else {
		send_config_msg(ch, "no_person");
	}
}


ACMD(do_purify) {
	void un_vampire(char_data *ch);
	
	char_data *vict;
	int cost = 50;
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_PURIFY, MANA, cost, NOTHING)) {
		return;
	}
	else if (!*arg) {
		msg_to_char(ch, "Purify whom?\r\n");
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (!IS_NPC(vict) && has_ability(vict, ABIL_DAYWALKING)) {
		msg_to_char(ch, "The light of your purify spell has no effect on daywalkers.\r\n");
	}
	else if (ch != vict && IS_NPC(vict) && IS_VAMPIRE(vict) && MOB_FLAGGED(vict, MOB_HARD | MOB_GROUP)) {
		msg_to_char(ch, "You cannot purify so powerful a vampire.\r\n");
	}
	else if (vict != ch && !IS_NPC(vict) && !PRF_FLAGGED(vict, PRF_BOTHERABLE)) {
		act("You can't purify someone without permission (ask $M to type 'toggle bother').", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (ch != vict && AFF_FLAGGED(vict, AFF_IMMUNE_NATURAL_MAGIC)) {
		msg_to_char(ch, "Your victim is immune to that spell.\r\n");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_PURIFY)) {
		return;
	}
	else {
		charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
		
		if (ch == vict) {
			msg_to_char(ch, "You let your mana wash over your body and purify your form.\r\n");
			act("$n seems to dance as $s mana washes over $s body and purifies $m.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			act("You let your mana wash over $N, purifying $M.", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n holds out $s hands and $s mana washes over you, purifying you.", FALSE, ch, NULL, vict, TO_VICT);
			act("$n holds out $s hands and $s mana washes over $N, purifying $M.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}
		
		if (IS_VAMPIRE(vict)) {
			msg_to_char(vict, "You feel the power of your blood fade and your vampiric powers vanish.\r\n");
			if (IS_NPC(vict)) {
				REMOVE_BIT(MOB_FLAGS(vict), MOB_VAMPIRE);
			}
			else {
				un_vampire(vict);
			}
		}
		
		if (can_gain_exp_from(ch, vict)) {
			gain_ability_exp(ch, ABIL_PURIFY, 50);
		}

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
	}
}


ACMD(do_quaff) {
	void scale_item_to_level(obj_data *obj, int level);
	
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Which potion would you like to quaff?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have a %s.\r\n", arg);
	}
	else if (!IS_POTION(obj)) {
		msg_to_char(ch, "You can only quaff potions.\r\n");
	}
	else if (GET_POTION_COOLDOWN_TYPE(obj) != NOTHING && get_cooldown_time(ch, GET_POTION_COOLDOWN_TYPE(obj)) > 0) {
		msg_to_char(ch, "You can't quaff that until your %s cooldown expires.\r\n", get_generic_name_by_vnum(GET_POTION_COOLDOWN_TYPE(obj)));
	}
	else {
		if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) == 0) {
			scale_item_to_level(obj, 1);	// just in case
		}
		
		if (!consume_otrigger(obj, ch, OCMD_QUAFF, NULL)) {
			return;	// check trigger last
		}
		
		act("You quaff $p!", FALSE, ch, obj, NULL, TO_CHAR);
		act("$n quaffs $p!", TRUE, ch, obj, NULL, TO_ROOM);

		apply_potion(obj, ch);
		extract_obj(obj);
	}	
}


ACMD(do_rejuvenate) {
	struct affected_type *af;
	char_data *vict = ch;
	int amount, bonus, cost = 25;
	
	int heal_levels[] = { 4, 6, 8 };	// x6 ticks (24, 36, 42)
	double int_mod = 0.3333;
	double bonus_heal_mod = 1.0/4.0;
	double base_cost_mod = 2.0;
	
	// healer ability mods
	double over_level_mod = 1.0/4.0;
	double healer_cost_mod[] = { 2.0, 1.5, 1.1 };
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_REJUVENATE, MANA, 0, COOLDOWN_REJUVENATE)) {
		return;
	}
	if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
		return;
	}
	
	// amount determines cost
	amount = CHOOSE_BY_ABILITY_LEVEL(heal_levels, ch, ABIL_REJUVENATE);
	amount += round(GET_INTELLIGENCE(ch) * int_mod);
	if (!IS_NPC(ch) && (GET_CLASS_ROLE(ch) == ROLE_HEALER || GET_CLASS_ROLE(ch) == ROLE_SOLO || has_player_tech(ch, PTECH_HEALING_BOOST)) && check_solo_role(ch)) {
		amount += round(MAX(0, get_approximate_level(ch) - 100) * over_level_mod);
		cost = round(amount * CHOOSE_BY_ABILITY_LEVEL(healer_cost_mod, ch, ABIL_REJUVENATE));
	}
	else {
		cost = round(amount * base_cost_mod);
	}
	
	cost = MAX(1, cost);
	
	// does not affect the cost
	bonus = total_bonus_healing(ch);
	amount += round(bonus * bonus_heal_mod);
	
	// run this again but with the cost
	if (!can_use_ability(ch, ABIL_REJUVENATE, MANA, cost, NOTHING)) {
		return;
	}
	
	// validated -- check triggers
	if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_REJUVENATE)) {
		return;
	}
		
	charge_ability_cost(ch, MANA, cost, COOLDOWN_REJUVENATE, 15, WAIT_SPELL);
	
	if (ch == vict) {
		msg_to_char(ch, "You surround yourself with the bright white mana of rejuvenation.\r\n");
		act("$n surrounds $mself with the bright white mana of rejuvenation.", TRUE, ch, NULL, NULL, TO_ROOM);
	}
	else {
		act("You surround $N with the bright white mana of rejuvenation.", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n surrounds you with the bright white mana of rejuvenation.", FALSE, ch, NULL, vict, TO_VICT);
		act("$n surrounds $N with the bright white mana of rejuvenation.", FALSE, ch, NULL, vict, TO_NOTVICT);
	}
	
	af = create_mod_aff(ATYPE_REJUVENATE, 6, APPLY_HEAL_OVER_TIME, amount, ch);
	affect_join(vict, af, 0);
	
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, ABIL_REJUVENATE, 15);
		gain_ability_exp(ch, ABIL_ANCESTRAL_HEALING, 15);
	}

	if (FIGHTING(vict) && !FIGHTING(ch)) {
		engage_combat(ch, FIGHTING(vict), FALSE);
	}
}


ACMD(do_resurrect) {
	void death_restore(char_data *ch);
	
	obj_data *corpse;
	char_data *vict;
	int cost = 75;
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_RESURRECT, MANA, cost, NOTHING)) {
		// sends own messages
	}
	else if (!*arg) {
		msg_to_char(ch, "Resurrect whom?\r\n");
	}
	else if ((vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		// person target
		if (!IS_DEAD(vict)) {
			msg_to_char(ch, "You can only resurrect a dead person.\r\n");
		}
		else if (IS_NPC(vict)) {
			msg_to_char(ch, "You can only resurrect players, not NPCs.\r\n");
		}
		else if (GET_ACCOUNT(ch) == GET_ACCOUNT(vict)) {
			msg_to_char(ch, "You can't resurrect your own alts.\r\n");
		}
		else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_RESURRECT)) {
			return;
		}
		else {
			// success: resurrect in room
			charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
			act("You begin channeling mana to resurrect $O...", FALSE, ch, NULL, vict, TO_CHAR | TO_NODARK);
			act("$n glows with white light as $e begins to channel $s mana to resurrect $O...", FALSE, ch, NULL, vict, TO_NOTVICT);
			act("$O is attempting to resurrect you (use 'accept/reject resurrection').", FALSE, vict, NULL, ch, TO_CHAR | TO_NODARK);
			add_offer(vict, ch, OFFER_RESURRECTION, ABIL_RESURRECT);
		}
	}
	else if ((corpse = get_obj_in_list_vis(ch, arg, ch->carrying)) || (corpse = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		// obj target
		if (!IS_CORPSE(corpse)) {
			msg_to_char(ch, "You can't resurrect that.\r\n");
		}
		else if (!IS_PC_CORPSE(corpse)) {
			msg_to_char(ch, "You can only resurrect player corpses.\r\n");
		}
		else if (!(vict = is_playing(GET_CORPSE_PC_ID(corpse)))) {
			msg_to_char(ch, "You can only resurrect the corpse of someone who is still playing.\r\n");
		}
		else if (vict == ch) {
			msg_to_char(ch, "You can't resurrect your own corpse, that's just silly.\r\n");
		}
		else if (GET_ACCOUNT(ch) == GET_ACCOUNT(vict)) {
			msg_to_char(ch, "You can't resurrect your own alts.\r\n");
		}
		else if (IS_DEAD(vict) || corpse != find_obj(GET_LAST_CORPSE_ID(vict), FALSE) || !IS_CORPSE(corpse)) {
			// victim has died AGAIN
			act("You can't resurrect $N with that corpse.", FALSE, ch, NULL, vict, TO_CHAR | TO_NODARK);
		}
		else {
			// seems legit...
			charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
			act("You begin channeling mana to resurrect $O...", FALSE, ch, NULL, vict, TO_CHAR | TO_NODARK);
			act("$n glows with white light as $e begins to channel $s mana to resurrect $O...", FALSE, ch, NULL, vict, TO_NOTVICT);
			act("$O is attempting to resurrect you (use 'accept/reject resurrection').", FALSE, vict, NULL, ch, TO_CHAR | TO_NODARK);
			add_offer(vict, ch, OFFER_RESURRECTION, ABIL_RESURRECT);
		}
	}
	else {
		send_config_msg(ch, "no_person");
	}
}


ACMD(do_skybrand) {
	char_data *vict = NULL;
	int cost;
	int dur = 6;	// ticks
	int dmg;
	
	double cost_mod[] = { 2.0, 1.5, 1.1 };
	
	// determine damage (which determines cost
	dmg = get_ability_level(ch, ABIL_SKYBRAND) / 24;	// skill level base
	if ((IS_NPC(ch) || GET_CLASS_ROLE(ch) == ROLE_CASTER || GET_CLASS_ROLE(ch) == ROLE_SOLO) && check_solo_role(ch)) {
		dmg = MAX(dmg, (get_approximate_level(ch) / 24));	// total level base
		dmg += GET_BONUS_MAGICAL(ch) / dur;
	}
	
	dmg += GET_INTELLIGENCE(ch) / dur;	// always add int
	
	// determine cost
	cost = round(dmg * CHOOSE_BY_ABILITY_LEVEL(cost_mod, ch, ABIL_SKYBRAND));
	
	if (!can_use_ability(ch, ABIL_SKYBRAND, MANA, cost, COOLDOWN_SKYBRAND)) {
		return;
	}
	
	// find target
	one_argument(argument, arg);
	if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
		return;
	}
	if (!*arg && !(vict = FIGHTING(ch))) {
		msg_to_char(ch, "Who would you like to cast that at?\r\n");
		return;
	}
	if (ch == vict) {
		msg_to_char(ch, "You wouldn't want to cast that on yourself.\r\n");
		return;
	}
	
	// check validity
	if (!can_fight(ch, vict)) {
		act("You can't attack $M!", FALSE, ch, NULL, vict, TO_CHAR);
		return;
	}
	if (NOT_MELEE_RANGE(ch, vict)) {
		msg_to_char(ch, "You need to be at melee range to do this.\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_SKYBRAND)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, COOLDOWN_SKYBRAND, 6, WAIT_COMBAT_SPELL);
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// counterspell??
	if (trigger_counterspell(vict) || AFF_FLAGGED(vict, AFF_IMMUNE_NATURAL_MAGIC)) {
		act("You can't seem to mark $N with the skybrand!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n tries to mark you with a skybrand, but fails!", FALSE, ch, NULL, vict, TO_VICT);
		act("$n tries to mark $N with a skybrand, but fails!", FALSE, ch, NULL, vict, TO_NOTVICT);
	}
	else {
		// succeed
		act("You mark $N with a glowing blue skybrand!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n marks you with a glowing blue skybrand!", FALSE, ch, NULL, vict, TO_VICT);
		act("$n marks $N with a glowing blue skybrand!", FALSE, ch, NULL, vict, TO_NOTVICT);
		
		apply_dot_effect(vict, ATYPE_SKYBRAND, 6, DAM_MAGICAL, dmg, 3, ch);
		engage_combat(ch, vict, TRUE);
	}
	
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, ABIL_SKYBRAND, 15);
	}
}


ACMD(do_soulsight) {
	void show_character_affects(char_data *ch, char_data *to);
	
	char_data *vict;
	int cost = 5;

	one_argument(argument, arg);

	if (!can_use_ability(ch, ABIL_SOULSIGHT, MANA, cost, NOTHING)) {
		return;
	}
	else if (!*arg)
		msg_to_char(ch, "Use soulsight upon whom?\r\n");
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (vict == ch)
		msg_to_char(ch, "You can't use this upon yourself!\r\n");
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_SOULSIGHT)) {
		return;
	}
	else {
		charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_NONE);

		act("$n's eyes flash briefly.", TRUE, ch, NULL, NULL, TO_ROOM);

		act("Your magical analysis of $N reveals:", FALSE, ch, NULL, vict, TO_CHAR);
		if (AFF_FLAGGED(vict, AFF_SOULMASK)) {
			sprintf(buf, " $E is %s.", (IS_NPC(vict) && MOB_FLAGGED(vict, MOB_ANIMAL)) ? "an animal" : (MOB_FLAGGED(vict, MOB_HUMAN) ? "a human" : "a creature"));
			act(buf, FALSE, ch, 0, vict, TO_CHAR);
		}
		else {
			sprintf(buf, " $E is %s.", IS_VAMPIRE(vict) ? "a vampire" : (IS_HUMAN(vict) ? ((IS_NPC(vict) && MOB_FLAGGED(vict, MOB_ANIMAL)) ? "an animal" : (MOB_FLAGGED(vict, MOB_HUMAN) ? "a human" : "a creature")) : "unknown"));
			act(buf, FALSE, ch, NULL, vict, TO_CHAR);
			
			show_character_affects(vict, ch);
		}
		
		if (can_gain_exp_from(ch, vict)) {
			gain_ability_exp(ch, ABIL_SOULSIGHT, 33.4);
		}
	}
}
