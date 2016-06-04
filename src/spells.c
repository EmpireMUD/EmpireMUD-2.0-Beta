/* ************************************************************************
*   File: spells.c                                        EmpireMUD 2.0b4 *
*  Usage: implementation for spells                                       *
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
*   Utilities
*   Damage Spells
*   Chants
*   Readies
*/

// external vars


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* This function checks if the character has a counterspell available and
* pops it if so.
*
* @param char_data *ch
* @return bool TRUE if a counterspell fired, FALSE if the spell can proceed
*/
bool trigger_counterspell(char_data *ch) {
	if (affected_by_spell(ch, ATYPE_COUNTERSPELL)) {
		msg_to_char(ch, "Your counterspell goes off!\r\n");
		affect_from_char(ch, ATYPE_COUNTERSPELL, FALSE);
		gain_ability_exp(ch, ABIL_COUNTERSPELL, 100);
		return TRUE;
	}
	
	return FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// DAMAGE SPELLS ///////////////////////////////////////////////////////////

struct damage_spell_type {
	any_vnum ability;	// ABIL_ type
	int cost;	// mana
	int attack_type;	// ATTACK_x
	double damage_mod;	// 1.0 = normal damage, balance based on affects
	bitvector_t aff_immunity;	// AFF_x flag making person immune
	
	// affect group: all this only matters if aff_type != -1
	int aff_type;	// ATYPE_, -1 for none
	int duration;	// time for the affect
	int apply;	// APPLY_, 0 for none
	int modifier;	// +/- value, if apply != 0
	bitvector_t aff_flag;	// AFF_x, 0 for none
	
	// dot affect
	int dot_type;	// ATYPE_, -1 for none
	int dot_duration;	// time for the dot
	int dot_damage_type;	// DAM_x for the dot
	int dot_damage;	// damage for the dot
	int dot_max_stacks;	// how high the dot can stack
	
	int cooldown_type;	// COOLDOWN_x
	int cooldown_time;	// seconds
};

const struct damage_spell_type damage_spell[] = {
	// Lightningbolt
	{ ABIL_LIGHTNINGBOLT, 25, ATTACK_LIGHTNINGBOLT, 0.8, AFF_IMMUNE_NATURAL_MAGIC,
		-1, 0, 0, 0, NOBITS,
		ATYPE_SHOCKED, 3, DAM_MAGICAL, 5, 1,
		COOLDOWN_LIGHTNINGBOLT, 9
	},
	
	// SUNSHOCK
	{ ABIL_SUNSHOCK, 25, ATTACK_SUNSHOCK, 0.6, AFF_IMMUNE_HIGH_SORCERY,
		ATYPE_SUNSHOCK, 1, APPLY_NONE, 0, AFF_BLIND,
		-1, 0, 0, 0, 0,
		COOLDOWN_SUNSHOCK, 9
	},
	
	{ NO_ABIL, 0, 1.0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0 }
};
	

/**
* @param int subcmd ABIL_x to match to the damage_spell[] struct
*/
ACMD(do_damage_spell) {
	char_data *vict = NULL;
	struct affected_type *af;
	int iter, type = NOTHING, cost, dmg, result;
	
	double level_mod[] = { 1.0, 1.5, 2.0 };
	
	// find ability
	for (iter = 0; damage_spell[iter].ability != NO_ABIL; ++iter) {
		if (damage_spell[iter].ability == subcmd) {
			type = iter;
			break;
		}
	}
	
	// sanity check
	if (type == NOTHING) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "do_damage_spell: unable to find ability %d", subcmd);
		msg_to_char(ch, "There is a bug in this ability. Please try again later.\r\n");
		return;
	}
	
	// cost calculations
	cost = damage_spell[type].cost;
	
	if (!can_use_ability(ch, subcmd, MANA, cost, damage_spell[type].cooldown_type)) {
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
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, damage_spell[type].ability)) {
		return;
	}
	
	dmg = get_ability_level(ch, subcmd) / 8.0 * damage_spell[type].damage_mod;
	dmg += GET_INTELLIGENCE(ch) * CHOOSE_BY_ABILITY_LEVEL(level_mod, ch, subcmd);
	dmg += GET_BONUS_MAGICAL(ch);
	
	if (damage_spell[type].ability == ABIL_SUNSHOCK && IS_VAMPIRE(vict)) {
		dmg *= 2;
	}
	
	charge_ability_cost(ch, MANA, cost, damage_spell[type].cooldown_type, damage_spell[type].cooldown_time, WAIT_COMBAT_SPELL);
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// check counterspell and then damage
	if (!trigger_counterspell(vict)) {
		result = damage(ch, vict, dmg, damage_spell[type].attack_type, DAM_MAGICAL);
		
		// damage returns -1 on death
		if (result > 0 && !IS_DEAD(vict) && !EXTRACTED(vict) && (damage_spell[type].aff_immunity == NOBITS || !AFF_FLAGGED(vict, damage_spell[type].aff_immunity))) {
			if (damage_spell[type].aff_type > 0) {
				af = create_aff(damage_spell[type].aff_type, damage_spell[type].duration, damage_spell[type].apply, damage_spell[type].modifier, damage_spell[type].aff_flag, ch);
				affect_join(vict, af, 0);
			}
			if (damage_spell[type].dot_type > 0) {
				apply_dot_effect(vict, damage_spell[type].dot_type, damage_spell[type].dot_duration, damage_spell[type].dot_damage_type, damage_spell[type].dot_damage, damage_spell[type].dot_max_stacks, ch);
			}
		}
	}
	else {
		// counterspell
		damage(ch, vict, 0, damage_spell[type].attack_type, DAM_MAGICAL);
	}
	
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, subcmd, 15);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// READIES /////////////////////////////////////////////////////////////////

// for do_ready
struct ready_magic_weapon_type {
	char *name;
	int mana;
	any_vnum ability;
	obj_vnum vnum;
	double min_dps;	// the lowest the DPS should ever go (or close to it)
	double target_dps;	// DPS at level 100 -- will get as close to this as possible without going over
} ready_magic_weapon[] = {
	{ "fireball", 30, ABIL_READY_FIREBALL, o_FIREBALL, 2.0, 5.0 },
	
	{ "\n", 0, NO_ABIL, NOTHING, 0.0, 0.0 }
};


ACMD(do_ready) {	
	obj_data *obj;
	int type, iter, cost, speed, min_damage;
	bool found;
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't use ready.\r\n");
		return;
	}
	
	if (!*arg) {
		msg_to_char(ch, "You know how to ready the following weapons: ");
		
		found = FALSE;
		for (iter = 0; *ready_magic_weapon[iter].name != '\n'; ++iter) {
			if (ready_magic_weapon[iter].ability == NO_ABIL || has_ability(ch, ready_magic_weapon[iter].ability)) {
				msg_to_char(ch, "%s%s", (found ? ", " : ""), ready_magic_weapon[iter].name);
				found = TRUE;
			}
		}
		
		msg_to_char(ch, "%s\r\n", (found ? "" : "none"));
		return;
	}

	found = FALSE;
	for (iter = 0; *ready_magic_weapon[iter].name != '\n' && !found; ++iter) {
		if (is_abbrev(arg, ready_magic_weapon[iter].name) && (ready_magic_weapon[iter].ability == NO_ABIL || has_ability(ch, ready_magic_weapon[iter].ability))) {
			type = iter;
			found = TRUE;
		}
	}
	
	if (!found) {
		msg_to_char(ch, "You don't know how to ready that.\r\n");
		return;
	}
	
	cost = ready_magic_weapon[type].mana;
	
	if (GET_MANA(ch) < cost) {
		msg_to_char(ch, "You need %d mana to do that.\r\n", cost);
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ready_magic_weapon[type].ability)) {
		return;
	}
	
	// attempt to remove existing wield
	if (GET_EQ(ch, WEAR_WIELD)) {
		perform_remove(ch, WEAR_WIELD);
		
		// did it work? if not, player got an error
		if (GET_EQ(ch, WEAR_WIELD)) {
			return;
		}
	}
		
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	GET_MANA(ch) -= cost;
	obj = read_object(ready_magic_weapon[type].vnum, TRUE);
	
	// damage based on skill
	if (ready_magic_weapon[type].ability != NO_ABIL) {		
		speed = (OBJ_FLAGGED(obj, OBJ_FAST) ? SPD_FAST : (OBJ_FLAGGED(obj, OBJ_SLOW) ? SPD_SLOW : SPD_NORMAL));
		min_damage = MAX(1, (int) ceil(ready_magic_weapon[type].min_dps * attack_hit_info[GET_WEAPON_TYPE(obj)].speed[speed]));
		GET_OBJ_VAL(obj, VAL_WEAPON_DAMAGE_BONUS) = MAX(min_damage, (int)(ready_magic_weapon[type].target_dps * (get_ability_level(ch, ready_magic_weapon[type].ability) / 100.0) * attack_hit_info[GET_WEAPON_TYPE(obj)].speed[speed]));
	}
	
	act("Mana twists and swirls around your hand and becomes $p!", FALSE, ch, obj, 0, TO_CHAR);
	act("Mana twists and swirls around $n's hand and becomes $p!", TRUE, ch, obj, 0, TO_ROOM);
	
	equip_char(ch, obj, WEAR_WIELD);
	determine_gear_level(ch);
	
	if (ready_magic_weapon[type].ability != NO_ABIL) {
		gain_ability_exp(ch, ready_magic_weapon[type].ability, 15);
	}

	load_otrigger(obj);
	
	command_lag(ch, WAIT_SPELL);
}
