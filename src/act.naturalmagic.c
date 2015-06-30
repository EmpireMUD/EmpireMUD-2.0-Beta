/* ************************************************************************
*   File: act.naturalmagic.c                              EmpireMUD 2.0b1 *
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
extern const int universal_wait;

// external funcs
extern obj_data *find_obj(int n);
extern bool is_fight_ally(char_data *ch, char_data *frenemy);	// fight.c
extern bool is_fight_enemy(char_data *ch, char_data *frenemy);	// fight.c
void perform_resurrection(char_data *ch, char_data *rez_by, room_data *loc, int ability);
extern bool trigger_counterspell(char_data *ch);	// spells.c

// locals
char_data *has_familiar(char_data *ch);
void un_earthmeld(char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

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
		affects_from_char_by_aff_flag(ch, AFF_EARTHMELD);
		if (!AFF_FLAGGED(ch, AFF_EARTHMELD)) {
			msg_to_char(ch, "You rise from the ground!\r\n");
			act("$n rises from the ground!", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_STANDING;
			add_cooldown(ch, COOLDOWN_EARTHMELD, 2 * SECS_PER_REAL_MIN);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// POTIONS /////////////////////////////////////////////////////////////////

#define POTION_SPEC_NONE  0	// first!
#define POTION_SPEC_HEALING  1
#define POTION_SPEC_MOVES  2
#define POTION_SPEC_MANA  3


// master potion data
const struct potion_data_type potion_data[] = {
	
	/**
	* Ideas on cleaning this up and improving the potions system:
	* - make default healing amounts here (for each pool type)
	* - consider just attaching applies and effects to the potion object, then transferring them over on quaff
	* - consider adding cooldown types to this table
	*/

	// WARNING: DO NOT CHANGE THE ORDER, ONLY ADD TO THE END
	// The position in this array corresponds to obj val 0

	{ "health", 0, APPLY_NONE, NOBITS, POTION_SPEC_HEALING },	// 0
	{ "health regeneration", ATYPE_REGEN_POTION, APPLY_HEALTH_REGEN, NOBITS, POTION_SPEC_NONE },	// 1
	{ "moves", 0, APPLY_NONE, NOBITS, POTION_SPEC_MOVES }, // 2
	{ "move regeneration", ATYPE_REGEN_POTION, APPLY_MOVE_REGEN, NOBITS, POTION_SPEC_NONE },	// 3
	{ "mana", 0, APPLY_NONE, NOBITS, POTION_SPEC_MANA }, // 4
	{ "mana regeneration", ATYPE_REGEN_POTION, APPLY_MANA_REGEN, NOBITS, POTION_SPEC_NONE }, // 5
		{ "*", 0, APPLY_NONE, NOBITS, POTION_SPEC_NONE },	// 6. UNUSED
	{ "strength", ATYPE_NATURE_POTION, APPLY_STRENGTH, NOBITS, POTION_SPEC_NONE }, // 7
	{ "dexterity", ATYPE_NATURE_POTION, APPLY_DEXTERITY, NOBITS, POTION_SPEC_NONE }, // 8
	{ "charisma", ATYPE_NATURE_POTION, APPLY_CHARISMA, NOBITS, POTION_SPEC_NONE }, // 9
	{ "intelligence", ATYPE_NATURE_POTION, APPLY_INTELLIGENCE, NOBITS, POTION_SPEC_NONE }, // 10
	{ "wits", ATYPE_NATURE_POTION, APPLY_WITS, NOBITS, POTION_SPEC_NONE }, // 11
		{ "*", 0, APPLY_NONE, NOBITS, POTION_SPEC_NONE },	// 12. UNUSED
	{ "resist-physical", ATYPE_NATURE_POTION, APPLY_RESIST_PHYSICAL, NOBITS, POTION_SPEC_NONE },	// 13
	{ "haste", ATYPE_NATURE_POTION, APPLY_NONE, AFF_HASTE, POTION_SPEC_NONE }, // 14
	{ "block", ATYPE_NATURE_POTION, APPLY_BLOCK, NOBITS, POTION_SPEC_NONE }, // 15

	// last
	{ "\n", 0, APPLY_NONE, NOBITS, POTION_SPEC_NONE }
};


/**
* Apply the actual effects of a potion.
*
* @param char_data *ch the quaffer
* @param int type potion_data[] pos
* @param int scale The scale level of the potion
*/
void apply_potion(char_data *ch, int type, int scale) {
	extern const double apply_values[];
	
	struct affected_type *af;
	int value;
		
	act("A swirl of light passes over you!", FALSE, ch, NULL, NULL, TO_CHAR);
	act("A swirl of light passes over $n!", FALSE, ch, NULL, NULL, TO_ROOM);
	
	// atype
	if (potion_data[type].atype > 0) {
		// remove any old effect
		affect_from_char(ch, potion_data[type].atype);
		
		if (potion_data[type].apply != APPLY_NONE) {
			double points = config_get_double("potion_apply_per_100") * scale / 100.0;
			value = MAX(1, round(points * 1.0 / apply_values[potion_data[type].apply]));
		}
		else {
			value = 0;
		}
		
		af = create_aff(potion_data[type].atype, 24 MUD_HOURS, potion_data[type].apply, value, potion_data[type].aff);
		affect_join(ch, af, 0);
	}
	
	// special cases
	switch (potion_data[type].spec) {
		case POTION_SPEC_HEALING: {
			heal(ch, ch, MAX(1, config_get_double("potion_heal_scale") * scale));
			break;
		}
		case POTION_SPEC_MOVES: {
			GET_MOVE(ch) = MIN(GET_MOVE(ch) + MAX(1, config_get_double("potion_heal_scale") * scale), GET_MAX_MOVE(ch));
			break;
		}
		case POTION_SPEC_MANA: {
			GET_MANA(ch) = MIN(GET_MANA(ch) + MAX(1, config_get_double("potion_heal_scale") * scale), GET_MAX_MANA(ch));
			break;
		}
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
	int pos, iter, cost = 30;
	bool done_aff, uncleansable;
	
	// a list of affects cleanse should ignore
	int exclude_blacklist[] = {
		ATYPE_MANASHIELD,	// has a max-mana penalty
		ATYPE_VIGOR,	// has a move regen penalty
		ATYPE_MUMMIFY,	// has a strange set of affs
		ATYPE_DEATHSHROUD,	// has a strange set of affs
		ATYPE_DEATH_PENALTY,	// is a penalty
		ATYPE_WAR_DELAY,	// is a penalty
		-1	// last
	};
	
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
		charge_ability_cost(ch, MANA, cost, COOLDOWN_CLEANSE, 9);
		
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
			
			// ensure not uncleansable
			uncleansable = FALSE;
			for (iter = 0; exclude_blacklist[iter] != -1; ++iter) {
				if (aff->type == exclude_blacklist[iter]) {
					uncleansable = TRUE;
					break;
				}
			}
			if (uncleansable) {
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
					if (IS_SET(bitv, BIT(1)) && aff_is_bad[pos]) {
						affect_remove(vict, aff);
						done_aff = TRUE;
					}
				}
			}
		}
		
		// remove DoTs
		for (dot = vict->over_time_effects; dot; dot = next_dot) {
			next_dot = dot->next;
			
			// ensure not uncleansable
			uncleansable = FALSE;
			for (iter = 0; exclude_blacklist[iter] != -1; ++iter) {
				if (dot->type == exclude_blacklist[iter]) {
					uncleansable = TRUE;
					break;
				}
			}
			if (uncleansable) {
				continue;
			}

			if (dot->damage_type == DAM_PHYSICAL || dot->damage_type == DAM_POISON) {
				dot_remove(vict, dot);
			}
		}

		if (GET_COND(vict, DRUNK) > 0) {
			gain_condition(vict, DRUNK, -1 * GET_COND(vict, DRUNK));
		}
		
		gain_ability_exp(ch, ABIL_CLEANSE, 33.4);

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
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
		charge_ability_cost(ch, MANA, cost, NOTHING, 0);
		
		msg_to_char(ch, "You ready a counterspell.\r\n");
		act("$n flickers momentarily with a blue-white aura.", TRUE, ch, NULL, NULL, TO_ROOM);
		
		af = create_flag_aff(ATYPE_COUNTERSPELL, 1 MUD_HOURS, 0);
		affect_join(ch, af, 0);
	}
}


ACMD(do_eartharmor) {
	struct affected_type *af;
	char_data *vict = ch;
	int cost = 20;
	double amount;
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_EARTHARMOR, MANA, cost, NOTHING)) {
		return;
	}
	
	// targeting
	if (!*arg) {
		vict = ch;
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, ch, NULL, ABIL_EARTHARMOR)) {
		return;
	}
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	charge_ability_cost(ch, MANA, cost, NOTHING, 0);
	
	// 100/14 = 7 resistance at max level
	amount = get_ability_level(ch, ABIL_EARTHARMOR) / 14.0;
	amount += GET_INTELLIGENCE(ch) / 3.0;
	af = create_mod_aff(ATYPE_EARTHARMOR, 30, APPLY_RESIST_PHYSICAL, (int)amount);
	
	if (ch == vict) {
		msg_to_char(ch, "You form a thick layer of mana over your body, and it hardens to solid earth!\r\n");
		act("$n forms a thick layer of mana over $s body, and it hardens to solid earth!", TRUE, ch, NULL, NULL, TO_ROOM);
	}
	else {
		act("You form a thick layer of mana over $N and then harden it to solid earth!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n forms a thick layer of mana over you, and it hardens to solid earth!", FALSE, ch, NULL, vict, TO_VICT);
		act("$n forms a thick layer of mana over $N, and it hardens to solid earth!", FALSE, ch, NULL, vict, TO_NOTVICT);
	}
	
	affect_join(vict, af, 0);
	
	gain_ability_exp(ch, ABIL_EARTHARMOR, 15);
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
		if (HAS_ABILITY(ch, ABIL_WORM) && IS_ANY_BUILDING(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't rise from the earth here!\r\n");
		}
		else {
			un_earthmeld(ch);
			look_at_room(ch);
		}
		return;
	}

	// sect validation
	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_SHALLOW_WATER | SECTF_MAP_BUILDING | SECTF_INSIDE)) {
		msg_to_char(ch, "You can't earthmeld without natural ground below you!\r\n");
		return;
	}
	
	if (SECT_FLAGGED(ROOM_ORIGINAL_SECT(IN_ROOM(ch)), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_SHALLOW_WATER | SECTF_MAP_BUILDING | SECTF_INSIDE)) {
		msg_to_char(ch, "You can't earthmeld without solid ground below you!\r\n");
		return;
	}
	
	if (IS_ADVENTURE_ROOM(IN_ROOM(ch)) && (!RMT_FLAGGED(IN_ROOM(ch), RMT_OUTDOOR) || RMT_FLAGGED(IN_ROOM(ch), RMT_NEED_BOAT))) {
		msg_to_char(ch, "You can't earthmeld without natural ground below you!\r\n");
		return;
	}

	if (!can_use_ability(ch, ABIL_EARTHMELD, MANA, cost, COOLDOWN_EARTHMELD)) {
		return;
	}
	
	if (GET_POS(ch) < POS_STANDING) {
		msg_to_char(ch, "You can't do that right now. You need to be standing.\r\n");
		return;
	}
	
	if (IS_RIDING(ch)) {
		msg_to_char(ch, "You can't do that while mounted.\r\n");
		return;
	}

	GET_MANA(ch) -= cost;
	
	msg_to_char(ch, "You dissolve into pure mana and sink into the ground!\r\n");
	act("$n dissolves into pure mana and sinks right into the ground!", TRUE, ch, 0, 0, TO_ROOM);
	GET_POS(ch) = POS_SLEEPING;

	af = create_aff(ATYPE_EARTHMELD, -1, APPLY_NONE, 0, AFF_NO_TARGET_IN_ROOM | AFF_NO_SEE_IN_ROOM | AFF_EARTHMELD);
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
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_ENTANGLE)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, COOLDOWN_ENTANGLE, 30);
	
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
	
		af = create_aff(ATYPE_ENTANGLE, 6, APPLY_DEXTERITY, -1, AFF_ENTANGLED);
		affect_join(vict, af, 0);

		engage_combat(ch, vict, FALSE);
		
		// release other entangleds here
		limit_crowd_control(vict, ATYPE_ENTANGLE);
	}

	gain_ability_exp(ch, ABIL_ENTANGLE, 15);
}


ACMD(do_familiar) {
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	bool check_scaling(char_data *mob, char_data *attacker);
	
	char_data *mob;
	int iter, type;
	bool any;
	
	struct {
		char *name;
		int ability;
		int level;	// skill level required
		mob_vnum vnum;
		int cost;
	} familiars[] = {
		{ "cat", ABIL_FAMILIAR, 0, FAMILIAR_CAT, 40 },
		{ "saber-tooth", ABIL_FAMILIAR, 51, FAMILIAR_SABERTOOTH, 40 },
		{ "sphinx", ABIL_FAMILIAR, 76, FAMILIAR_SPHINX, 40 },
		{ "griffin", ABIL_FAMILIAR, 100, FAMILIAR_GRIFFIN, 40 },
		
		{ "\n", NO_ABIL, 0, NOTHING, 0 }
	};
	
	if (has_familiar(ch)) {
		msg_to_char(ch, "You can't summon a familiar while you already have one.\r\n");
		return;
	}
	
	one_argument(argument, arg);
	
	// no-arg: just list
	if (!*arg) {
		msg_to_char(ch, "Summon which familiar:");
		any = FALSE;
		for (iter = 0; *familiars[iter].name != '\n'; ++iter) {
			if (!IS_NPC(ch) && familiars[iter].ability != NO_ABIL && !HAS_ABILITY(ch, familiars[iter].ability)) {
				continue;
			}
			if (GET_SKILL_LEVEL(ch) < familiars[iter].level) {
				continue;
			}
			
			msg_to_char(ch, "%s%s", any ? ", " : " ", familiars[iter].name);
			any = TRUE;
		}
		
		return;
	}
	
	// find which one they wanted
	type = -1;
	for (iter = 0; *familiars[iter].name != '\n'; ++iter) {
		if (!IS_NPC(ch) && familiars[iter].ability != NO_ABIL && !HAS_ABILITY(ch, familiars[iter].ability)) {
			continue;
		}
		if (GET_SKILL_LEVEL(ch) < familiars[iter].level) {
			continue;
		}
		if (is_abbrev(arg, familiars[iter].name)) {
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
	
	charge_ability_cost(ch, MANA, familiars[type].cost, NOTHING, 0);
	mob = read_mobile(familiars[type].vnum);
	if (IS_NPC(ch)) {
		MOB_INSTANCE_ID(mob) = MOB_INSTANCE_ID(ch);
	}
	setup_generic_npc(mob, GET_LOYALTY(ch), NOTHING, NOTHING);
	
	// scale to summoner
	check_scaling(mob, ch);
	
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


ACMD(do_fly) {
	struct affected_type *af;
	int cost = 50;
	int fly_durations[] = { 6 MUD_HOURS, 6 MUD_HOURS, 24 MUD_HOURS };
	
	// cancel out for free
	if (affected_by_spell(ch, ATYPE_FLY)) {
		msg_to_char(ch, "You stop flying and your wings fade away.\r\n");
		act("$n lands and $s wings fade away.", TRUE, ch, NULL, NULL, TO_ROOM);
		affect_from_char(ch, ATYPE_FLY);
		WAIT_STATE(ch, universal_wait);
		return;
	}
	
	if (!can_use_ability(ch, ABIL_FLY, MANA, cost, NOTHING)) {
		return;
	}
	if (IS_RIDING(ch)) {
		msg_to_char(ch, "You can't fly while mounted.\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_FLY)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, NOTHING, 0);
	
	af = create_flag_aff(ATYPE_FLY, CHOOSE_BY_ABILITY_LEVEL(fly_durations, ch, ABIL_FLY), AFF_FLY);
	affect_join(ch, af, 0);
	
	msg_to_char(ch, "You concentrate for a moment...\r\nSparkling blue wings made of pure mana erupt from your back!\r\n");
	act("$n seems to concentrate for a moment...Sparkling blue wings made of pure mana erupt from $s back!", TRUE, ch, NULL, NULL, TO_ROOM);
	
	gain_ability_exp(ch, ABIL_FLY, 5);
}


ACMD(do_hasten) {
	struct affected_type *af;
	char_data *vict = ch;
	int cost = 15;
	int durations[] = { 6 MUD_HOURS, 6 MUD_HOURS, 24 MUD_HOURS };
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_HASTEN, MANA, cost, NOTHING)) {
		return;
	}
	else if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_HASTEN)) {
		return;
	}
	else {
		charge_ability_cost(ch, MANA, cost, NOTHING, 0);
		
		if (ch == vict) {
			msg_to_char(ch, "You concentrate and veins of red mana streak down your skin.\r\n");
			act("Veins of red mana streak down $n's skin and $e seems to move a little faster.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			act("You touch $N and veins of red mana streak down $S skin.", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n touches you on the arm. Veins of red mana streak down your skin and you feel a little faster.", FALSE, ch, NULL, vict, TO_VICT);
			act("$n touches $N on the arm. Veins of red mana streak down $S skin and $E seems to move a little faster.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}
		
		af = create_flag_aff(ATYPE_HASTEN, CHOOSE_BY_ABILITY_LEVEL(durations, ch, ABIL_HASTEN), AFF_HASTE);
		affect_join(vict, af, 0);
		
		gain_ability_exp(ch, ABIL_HASTEN, 15);

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
	}
}


/**
* do_heal connects several abilities: ABIL_HEAL, ABIL_HEAL_FRIEND,
* and ABIL_HEAL_PARTY. Restrictions are based on which of these the player
* has purchased.
*/
ACMD(do_heal) {
	char_data *vict = ch, *ch_iter, *next_ch;
	bool party = FALSE;
	int cost, abil = NO_ABIL, gain = 15, amount;
	
	int heal_levels[] = { 15, 25, 35 };
	double intel_bonus[] = { 0.5, 1.5, 2.0 };
	double level_bonus[] = { 0.5, 1.0, 1.5 };
	double cost_ratio[] = { 0.75, 0.5, 0.25 };	// multiplied by amount healed
	double party_cost = 1.25;
	double self_cost = 0.75;
	
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
		msg_to_char(ch, "You must purchase the Heal Friend ability to heal others.\r\n");
		return;
	}
	else if (party && !can_use_ability(ch, ABIL_HEAL_PARTY, NOTHING, 0, NOTHING)) {
		msg_to_char(ch, "You must purchase the Heal Party ability to heal the whole party.\r\n");
		return;
	}
	
	if (!party && GET_HEALTH(vict) >= GET_MAX_HEALTH(vict)) {
		msg_to_char(ch, "There aren't any wounds to heal!\r\n");
		return;
	}
	
	if (!party && IS_DEAD(vict)) {
		msg_to_char(ch, "Unfortunately you can't heal on someone who is already dead.\r\n");
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
	amount = CHOOSE_BY_ABILITY_LEVEL(heal_levels, ch, abil) + (GET_INTELLIGENCE(ch) * CHOOSE_BY_ABILITY_LEVEL(intel_bonus, ch, abil)) + (MAX(0, get_approximate_level(ch) - 100) * CHOOSE_BY_ABILITY_LEVEL(level_bonus, ch, abil));
	
	if (vict && !party) {
		// subtract bonus-healing because it will be re-added at the end
		amount = MIN(amount, GET_MAX_HEALTH(vict) - GET_HEALTH(vict) - GET_BONUS_HEALING(ch));
		amount = MAX(1, amount);
	}
	
	cost = amount * CHOOSE_BY_ABILITY_LEVEL(cost_ratio, ch, abil);
	
	// bonus healing does not add to cost
	amount += GET_BONUS_HEALING(ch);
	
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
	charge_ability_cost(ch, MANA, cost, NOTHING, 0);
	
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
	}
	
	if (abil != NO_ABIL) {
		gain_ability_exp(ch, abil, gain);
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
		else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_MOONRISE)) {
			return;
		}
		else {
			// success: resurrect dead target
			charge_ability_cost(ch, MANA, cost, COOLDOWN_MOONRISE, 20 * SECS_PER_REAL_MIN);
			msg_to_char(ch, "You let out a bone-chilling howl...\r\n");
			act("$n lets out a bone-chilling howl...", FALSE, ch, NULL, NULL, TO_ROOM);
			perform_resurrection(vict, ch, IN_ROOM(ch), ABIL_MOONRISE);
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
		else if (IS_DEAD(vict) || corpse != find_obj(GET_LAST_CORPSE_ID(vict)) || !IS_CORPSE(corpse)) {
			// victim has died AGAIN
			act("You can only resurrect $N using $S most recent corpse.", FALSE, ch, NULL, vict, TO_CHAR | TO_NODARK);
		}
		else {
			// seems legit...
			charge_ability_cost(ch, MANA, cost, NOTHING, 0);
			msg_to_char(ch, "You let out a bone-chilling howl...\r\n");
			act("$n lets out a bone-chilling howl...", FALSE, ch, NULL, NULL, TO_ROOM);
			
			// set up rez data
			GET_RESURRECT_LOCATION(vict) = GET_ROOM_VNUM(IN_ROOM(ch));
			GET_RESURRECT_BY(vict) = IS_NPC(ch) ? NOBODY : GET_IDNUM(ch);
			GET_RESURRECT_ABILITY(vict) = ABIL_MOONRISE;
			GET_RESURRECT_TIME(vict) = time(0);
			act("$N is attempting to resurrect you. Type 'respawn' to accept.", FALSE, vict, NULL, ch, TO_CHAR | TO_NODARK);
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
	else if (!IS_NPC(vict) && HAS_ABILITY(vict, ABIL_DAYWALKING)) {
		msg_to_char(ch, "The light of your purify spell has no effect on daywalkers.\r\n");
	}
	else if (vict != ch && !IS_NPC(vict) && !PRF_FLAGGED(vict, PRF_BOTHERABLE)) {
		act("You can't purify someone without permission (ask $M to type NOBOTHER).", FALSE, ch, NULL, vict, TO_CHAR);
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_PURIFY)) {
		return;
	}
	else {
		charge_ability_cost(ch, MANA, cost, NOTHING, 0);
		
		if (ch == vict) {
			msg_to_char(ch, "You let your mana wash over your body and purify your form.\r\n");
			act("$n seems to dance as $s mana washes over $s body and purifies $m.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			act("You let your mana wash over $N, purifying $M.", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n holds out $s hands and $s mana washes over you, purifying you.", FALSE, ch, NULL, vict, TO_VICT);
			act("$n holds out $s hands and $s mana washes over $N, purifying $M.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}
		
		if (!IS_NPC(vict) && IS_VAMPIRE(vict)) {
			un_vampire(vict);
			msg_to_char(vict, "You feel the power of your blood fade and your vampiric powers vanish.\r\n");
		}
		
		gain_ability_exp(ch, ABIL_PURIFY, 50);

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
	}
}


ACMD(do_quaff) {
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
	else if (!consume_otrigger(obj, ch, OCMD_QUAFF)) {
		/* check trigger */
		return;
	}
	else {
		act("You quaff $p!", FALSE, ch, obj, NULL, TO_CHAR);
		act("$n quaffs $p!", TRUE, ch, obj, NULL, TO_ROOM);

		apply_potion(ch, GET_POTION_TYPE(obj), GET_POTION_SCALE(obj));
		extract_obj(obj);
	}	
}


ACMD(do_rejuvenate) {
	struct affected_type *af;
	char_data *vict = ch;
	int amount, cost = 25;
	
	int heal_levels[] = { 4, 6, 8 };
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_REJUVENATE, MANA, cost, COOLDOWN_REJUVENATE)) {
		return;
	}
	else if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_REJUVENATE)) {
		return;
	}
	else {
		charge_ability_cost(ch, MANA, cost, COOLDOWN_REJUVENATE, 15);
		
		if (ch == vict) {
			msg_to_char(ch, "You surround yourself with the bright white mana of rejuvenation.\r\n");
			act("$n surrounds $mself with the bright white mana of rejuvenation.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			act("You surround $N with the bright white mana of rejuvenation.", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n surrounds you with the bright white mana of rejuvenation.", FALSE, ch, NULL, vict, TO_VICT);
			act("$n surrounds $N with the bright white mana of rejuvenation.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}
		
		amount = CHOOSE_BY_ABILITY_LEVEL(heal_levels, ch, ABIL_REJUVENATE);
		amount += GET_INTELLIGENCE(ch) / 3;
		
		af = create_mod_aff(ATYPE_REJUVENATE, 6, APPLY_HEAL_OVER_TIME, amount);
		affect_join(vict, af, 0);
		
		gain_ability_exp(ch, ABIL_REJUVENATE, 15);

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
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
		else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_RESURRECT)) {
			return;
		}
		else {
			// success: resurrect in room
			charge_ability_cost(ch, MANA, cost, NOTHING, 0);
			msg_to_char(ch, "You begin channeling mana...\r\n");
			act("$n glows with white light as $e begins to channel $s mana...", FALSE, ch, NULL, NULL, TO_ROOM);
			perform_resurrection(vict, ch, IN_ROOM(ch), ABIL_RESURRECT);
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
		else if (IS_DEAD(vict) || corpse != find_obj(GET_LAST_CORPSE_ID(vict)) || !IS_CORPSE(corpse)) {
			// victim has died AGAIN
			act("You can only resurrect $N using $S most recent corpse.", FALSE, ch, NULL, vict, TO_CHAR | TO_NODARK);
		}
		else {
			// seems legit...
			charge_ability_cost(ch, MANA, cost, NOTHING, 0);
			act("You begin channeling mana to resurrect $N...", FALSE, ch, NULL, vict, TO_CHAR | TO_NODARK);
			act("$n glows with white light as $e begins to channel $s mana to resurrect $N...", FALSE, ch, NULL, vict, TO_NOTVICT);
			
			// set up rez data
			GET_RESURRECT_LOCATION(vict) = GET_ROOM_VNUM(IN_ROOM(ch));
			GET_RESURRECT_BY(vict) = IS_NPC(ch) ? NOBODY : GET_IDNUM(ch);
			GET_RESURRECT_ABILITY(vict) = ABIL_RESURRECT;
			GET_RESURRECT_TIME(vict) = time(0);
			act("$N is attempting to resurrect you. Type 'respawn' to accept.", FALSE, vict, NULL, ch, TO_CHAR | TO_NODARK);
		}
	}
	else {
		send_config_msg(ch, "no_person");
	}
}


ACMD(do_skybrand) {
	char_data *vict = NULL;
	int cost = 30;
	
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
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_SKYBRAND)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, COOLDOWN_SKYBRAND, 6);
	
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
	
		apply_dot_effect(vict, ATYPE_SKYBRAND, 6, DAM_MAGICAL, get_ability_level(ch, ABIL_SKYBRAND) / 24, 3);
		engage_combat(ch, vict, FALSE);
	}

	gain_ability_exp(ch, ABIL_SKYBRAND, 15);
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
		charge_ability_cost(ch, MANA, cost, NOTHING, 0);

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

		gain_ability_exp(ch, ABIL_SOULSIGHT, 33.4);
	}
}
