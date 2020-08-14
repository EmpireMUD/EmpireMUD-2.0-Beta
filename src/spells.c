/* ************************************************************************
*   File: spells.c                                        EmpireMUD 2.0b5 *
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

// external funcs
void check_combat_start(char_data *ch);


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
	double cost_mod;	// mana cost as a % of normal spells (1.0 = normal)
	int attack_type;	// ATTACK_
	double damage_mod;	// 1.0 = normal damage, balance based on affects
	bitvector_t aff_immunity;	// AFF_ flag making person immune
	
	// affect group: all this only matters if aff_type != -1
	int aff_type;	// ATYPE_, -1 for none
	int duration;	// time for the affect
	int apply;	// APPLY_, 0 for none
	int modifier;	// +/- value, if apply != 0
	bitvector_t aff_flag;	// AFF_, 0 for none
	
	// dot affect
	int dot_type;	// ATYPE_, -1 for none
	int dot_duration;	// time for the dot
	int dot_damage_type;	// DAM_ for the dot
	double dot_dmg_mod;	// % of scaled dot damage (1.0 = normal)
	int dot_max_stacks;	// how high the dot can stack
	
	int cooldown_type;	// COOLDOWN_
	int cooldown_time;	// seconds
};

// shortcuts
#define NO_SPELL_AFFECT  -1, 0, 0, 0, NOBITS
#define NO_DOT_AFFECT  -1, 0, 0, 0, 0

const struct damage_spell_type damage_spell[] = {
	// ABIL_, cost-mod, ATTACK_, damage-mod, immunity-aff,
	// affect (optional): ATYPE_, duration, apply, modifier, adds-aff
	// dot (optional): ATYPE_, duration, DAM_, damage, max-stacks
	// COOLDOWN_, cooldown
	
	// Lightningbolt
	{ ABIL_LIGHTNINGBOLT, 1, ATTACK_LIGHTNINGBOLT, 0.8, AFF_IMMUNE_NATURAL_MAGIC,
		NO_SPELL_AFFECT,
		ATYPE_SHOCKED, 3, DAM_MAGICAL, 0.5, 1,
		COOLDOWN_LIGHTNINGBOLT, 9
	},
	
	// SUNSHOCK
	{ ABIL_SUNSHOCK, 1.3, ATTACK_SUNSHOCK, 0.6, AFF_IMMUNE_HIGH_SORCERY,
		ATYPE_SUNSHOCK, 1, APPLY_NONE, 0, AFF_BLIND,
		NO_DOT_AFFECT,
		COOLDOWN_SUNSHOCK, 9
	},
	
	// ABLATE
	{ ABIL_ABLATE, 1.33, ATTACK_ABLATE, 0.6, AFF_IMMUNE_HIGH_SORCERY,
		ATYPE_ABLATE, 2, APPLY_RESIST_PHYSICAL, -10, NOBITS,
		NO_DOT_AFFECT,
		COOLDOWN_ABLATE, 9
	},
		
	// ARCLIGHT
	{ ABIL_ARCLIGHT, 1, ATTACK_ARCLIGHT, 1.4, NOBITS,
		NO_SPELL_AFFECT,
		NO_DOT_AFFECT,
		COOLDOWN_ARCLIGHT, 9
	},
	
	// SHADOWLASH
	{ ABIL_SHADOWLASH, 1.2, ATTACK_SHADOWLASH, 0.25, AFF_IMMUNE_HIGH_SORCERY,
		ATYPE_SHADOWLASH_BLIND, 1, APPLY_NONE, 0, AFF_BLIND,
		ATYPE_SHADOWLASH_DOT, 3, DAM_MAGICAL, 0.75, 3,
		COOLDOWN_SHADOWLASH, 9
	},
	
	// THORNLASH
	{ ABIL_THORNLASH, 1, ATTACK_THORNLASH, 0.4, AFF_IMMUNE_HIGH_SORCERY,
		NO_SPELL_AFFECT,
		ATYPE_THORNLASH, 3, DAM_POISON, 1.0, 3,
		COOLDOWN_THORNLASH, 9
	},
	
	// CHRONOBLAST
	{ ABIL_CHRONOBLAST, 1.15, ATTACK_CHRONOBLAST, 0.9, AFF_IMMUNE_HIGH_SORCERY,
		ATYPE_CHRONOBLAST, 1, APPLY_NONE, 0, AFF_SLOW,
		NO_DOT_AFFECT,
		COOLDOWN_CHRONOBLAST, 6
	},
	
	// SOULCHAIN
	{ ABIL_SOULCHAIN, 1.1, ATTACK_SOULCHAIN, 0.9, AFF_IMMUNE_HIGH_SORCERY,
		ATYPE_SOULCHAIN, 2, APPLY_INTELLIGENCE, -4, NOBITS,
		NO_DOT_AFFECT,
		COOLDOWN_SOULCHAIN, 9
	},
	
	// ASTRALCLAW
	{ ABIL_ASTRALCLAW, 1, ATTACK_ASTRALCLAW, 0.4, AFF_IMMUNE_NATURAL_MAGIC,
		NO_SPELL_AFFECT,
		ATYPE_ASTRALCLAW, 3, DAM_PHYSICAL, 1.0, 3,
		COOLDOWN_ASTRALCLAW, 9
	},
		
	// DISPIRIT
	{ ABIL_DISPIRIT, 1.1, ATTACK_DISPIRIT, 0.9, AFF_IMMUNE_NATURAL_MAGIC,
		ATYPE_DISPIRIT, 2, APPLY_WITS, -4, NOBITS,
		NO_DOT_AFFECT,
		COOLDOWN_DISPIRIT, 9
	},
	
	// STARSTRIKE
	{ ABIL_STARSTRIKE, 1, ATTACK_STARSTRIKE, 1.5, AFF_IMMUNE_NATURAL_MAGIC,
		NO_SPELL_AFFECT,
		NO_DOT_AFFECT,
		COOLDOWN_STARSTRIKE, 9
	},
	
	// ACIDBLAST
	{ ABIL_ACIDBLAST, 1.33, ATTACK_ACIDBLAST, 0.6, AFF_IMMUNE_NATURAL_MAGIC,
		ATYPE_ACIDBLAST, 2, APPLY_RESIST_MAGICAL, -10, NOBITS,
		NO_DOT_AFFECT,
		COOLDOWN_ACIDBLAST, 9
	},
	
	// DEATHTOUCH
	{ ABIL_DEATHTOUCH, 1.05, ATTACK_DEATHTOUCH, 1.0, AFF_IMMUNE_NATURAL_MAGIC,
		NO_SPELL_AFFECT,
		NO_DOT_AFFECT,
		COOLDOWN_DEATHTOUCH, 6
	},
	
	{ NO_ABIL, 0, 1.0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0 }
};
	

/**
* @param int subcmd ABIL_x to match to the damage_spell[] struct
*/
ACMD(do_damage_spell) {
	char_data *vict = NULL;
	struct affected_type *af;
	int iter, type = NOTHING, cost, dmg, dot_dmg, result;
	
	double cost_mod[] = { 1.5, 1.2, 0.9 };
	
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
	
	// calculate damage in order to calculate cost
	if ((IS_NPC(ch) || GET_CLASS_ROLE(ch) == ROLE_CASTER || GET_CLASS_ROLE(ch) == ROLE_SOLO) && check_solo_role(ch)) {
		dmg = get_approximate_level(ch) / 8.0;
		dmg += GET_BONUS_MAGICAL(ch);
	}
	else {	// not on a role
		dmg = get_ability_level(ch, subcmd) / 8.0;
	}
	
	dmg += GET_INTELLIGENCE(ch);	// both add this
	dmg *= damage_spell[type].damage_mod;	// modify by the spell
	
	// calculate DoT damage if any
	dot_dmg = 0;
	if (damage_spell[type].dot_type > 0) {
		dot_dmg = get_ability_level(ch, subcmd) / 24;	// skill level base
		if ((IS_NPC(ch) || GET_CLASS_ROLE(ch) == ROLE_CASTER || GET_CLASS_ROLE(ch) == ROLE_SOLO) && check_solo_role(ch)) {
			dot_dmg = MAX(dot_dmg, (get_approximate_level(ch) / 24));	// total level base
			dot_dmg += GET_BONUS_MAGICAL(ch) / MAX(1, damage_spell[type].dot_duration);
		}
		
		dot_dmg += GET_INTELLIGENCE(ch) / MAX(1, damage_spell[type].dot_duration);	// always add int
		
		// finally:
		dot_dmg = round(dot_dmg * damage_spell[type].dot_dmg_mod);
		dot_dmg = MAX(1, dot_dmg);
	}
	
	// cost calculations
	cost = round((dmg + dot_dmg) * CHOOSE_BY_ABILITY_LEVEL(cost_mod, ch, ABIL_SKYBRAND) * damage_spell[type].cost_mod);
	
	// check ability and cost
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
	
	if (NOT_MELEE_RANGE(ch, vict)) {
		msg_to_char(ch, "You need to be at melee range to do this.\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, damage_spell[type].ability)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, damage_spell[type].cooldown_type, damage_spell[type].cooldown_time, WAIT_COMBAT_SPELL);
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// start meters now, to track direct damage()
	check_combat_start(ch);
	check_combat_start(vict);
	
	// special-casing damage (done AFTER cost); requires vict
	if (damage_spell[type].ability == ABIL_SUNSHOCK && IS_VAMPIRE(vict)) {
		dmg *= 1.5;
	}
	
	// check counterspell and then damage
	if (!trigger_counterspell(vict)) {
		//msg_to_char(ch, "Damage: %d\r\n", dmg);
		result = damage(ch, vict, dmg, damage_spell[type].attack_type, DAM_MAGICAL);
		
		// damage returns -1 on death
		if (result > 0 && !IS_DEAD(vict) && !EXTRACTED(vict) && (damage_spell[type].aff_immunity == NOBITS || !AFF_FLAGGED(vict, damage_spell[type].aff_immunity))) {
			if (damage_spell[type].aff_type > 0) {
				af = create_aff(damage_spell[type].aff_type, damage_spell[type].duration, damage_spell[type].apply, damage_spell[type].modifier, damage_spell[type].aff_flag, ch);
				affect_join(vict, af, 0);
			}
			if (damage_spell[type].dot_type > 0 && dot_dmg > 0) {
				apply_dot_effect(vict, damage_spell[type].dot_type, damage_spell[type].dot_duration, damage_spell[type].dot_damage_type, dot_dmg, damage_spell[type].dot_max_stacks, ch);
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
	int cost;
	int cost_pool;	// MANA, etc
	any_vnum ability;
	obj_vnum vnum;
} ready_magic_weapon[] = {
	{ "bloodaxe", 40, BLOOD, ABIL_READY_BLOOD_WEAPONS, o_BLOODAXE },
	{ "bloodmace", 40, BLOOD, ABIL_READY_BLOOD_WEAPONS, o_BLOODMACE },
	{ "bloodmattock", 40, BLOOD, ABIL_READY_BLOOD_WEAPONS, o_BLOODMATTOCK },
	{ "bloodmaul", 40, BLOOD, ABIL_READY_BLOOD_WEAPONS, o_BLOODMAUL },
	{ "bloodskean", 40, BLOOD, ABIL_READY_BLOOD_WEAPONS, o_BLOODSKEAN },
	{ "bloodspear", 40, BLOOD, ABIL_READY_BLOOD_WEAPONS, o_BLOODSPEAR },
	{ "bloodsword", 40, BLOOD, ABIL_READY_BLOOD_WEAPONS, o_BLOODSWORD },
	{ "bloodstaff", 40, BLOOD, ABIL_READY_BLOOD_WEAPONS, o_BLOODSTAFF },
	{ "bloodwhip", 40, BLOOD, ABIL_READY_BLOOD_WEAPONS, o_BLOODWHIP },
	
	{ "fireball", 30, MANA, ABIL_READY_FIREBALL, o_FIREBALL },
	
	{ "\n", 0, NO_ABIL, NOTHING }
};


ACMD(do_ready) {
	extern bool check_vampire_sun(char_data *ch, bool message);
	void scale_item_to_level(obj_data *obj, int level);
	extern const char *pool_types[];
	
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH];
	struct player_ability_data *plab, *next_plab;
	int scale_level, ch_level = 0, pos;
	ability_data *abil, *found_abil;
	bool found, full, later = TRUE;
	struct ability_data_list *adl;
	obj_data *obj, *proto;
	size_t size, lsize;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't use ready.\r\n");
		return;
	}
	
	#define VALID_READY_ABIL(ch, plab, abil)  ((abil) && (plab) && (plab)->purchased[GET_CURRENT_SKILL_SET(ch)] && IS_SET(ABIL_TYPES(abil), ABILT_READY_WEAPONS))
	
	if (!*argument) {
		size = snprintf(buf, sizeof(buf), "You know how to ready the following weapons:\r\n");
		
		found = full = FALSE;
		HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
			abil = plab->ptr;
			if (!VALID_READY_ABIL(ch, plab, abil)) {
				continue;
			}
			
			LL_FOREACH(ABIL_DATA(abil), adl) {
				if (adl->type == ADL_READY_WEAPON && obj_proto(adl->vnum)) {
					// build display
					lsize = snprintf(line, sizeof(line), " %s", skip_filler(get_obj_name_by_proto(adl->vnum)));
					
					if (ABIL_COST(abil) > 0) {
						lsize += snprintf(line + lsize, sizeof(line) - lsize, " (%d %s)", ABIL_COST(abil), pool_types[ABIL_COST_TYPE(abil)]);
					}
					
					strcat(line, "\r\n");
					lsize += 2;
					
					if (size + lsize < sizeof(buf)) {
						strcat(buf, line);
						size += lsize;
						full = TRUE;
						break;
					}
				}
			}
			
			if (full) {
				break;
			}
		}
		
		if (!found) {
			strcat(buf, " none\r\n");	// always room for this if !found
		}
		if (full) {
			snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
		}
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
		return;
	}
	
	// lookup
	found = FALSE;
	found_abil = NULL;
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		abil = plab->ptr;
		if (!VALID_READY_ABIL(ch, plab, abil)) {
			continue;
		}
		
		LL_FOREACH(ABIL_DATA(abil), adl) {
			if (adl->type == ADL_READY_WEAPON && (proto = obj_proto(adl->vnum))) {
				if (multi_isname(argument, GET_OBJ_KEYWORDS(proto))) {
					found = TRUE;
					found_abil = abil;
					break;
				}
			}
		}
		
		if (found) {
			break;
		}
	}
	
	// validate
	if (!found || !proto || !found_abil) {
		msg_to_char(ch, "You don't know how to ready that.\r\n");
		return;
	}
	if (!can_use_ability(ch, ABIL_VNUM(found_abil), ABIL_COST_TYPE(found_abil), ABIL_COST(found_abil), NOTHING)) {
		return;
	}
	if (!ABILITY_FLAGGED(found_abil, ABILF_IGNORE_SUN) && ABIL_COST(found_abil) > 0 && ABIL_COST_TYPE(found_abil) == BLOOD && !check_vampire_sun(ch, TRUE)) {
		return;
	}
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_VNUM(found_abil))) {
		return;
	}
	
	// determine wear position
	if (CAN_WEAR(proto, ITEM_WEAR_WIELD)) {
		pos = WEAR_WIELD;
	}
	else if (CAN_WEAR(proto, ITEM_WEAR_HOLD)) {
		pos = WEAR_HOLD;	// ONLY if they can't wield it
	}
	else if (CAN_WEAR(proto, ITEM_WEAR_RANGED)) {
		pos = WEAR_RANGED;
	}
	else {
		log("SYSERR: %s trying to ready %d %s with no valid wear bits (Ability %d %s)", GET_NAME(ch), GET_OBJ_VNUM(proto), GET_OBJ_SHORT_DESC(proto), ABIL_VNUM(found_abil), ABIL_NAME(found_abil));
		msg_to_char(ch, "You can't seem to ready that item.\r\n");
		return;
	}
	
	// if they are using a NON-1-use item, determine level now
	if (GET_EQ(ch, pos) && !OBJ_FLAGGED(GET_EQ(ch, pos), OBJ_SINGLE_USE)) {
		ch_level = get_approximate_level(ch);
		later = FALSE;
	}
	
	// attempt to remove existing item
	if (GET_EQ(ch, pos)) {
		perform_remove(ch, pos);
		
		// did it work? if not, player got an error
		if (GET_EQ(ch, pos)) {
			return;
		}
	}
		
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// if they are using a 1-use item, determine level at the end here
	if (later) {
		ch_level = get_approximate_level(ch);
	}
	
	charge_ability_cost(ch, ABIL_COST_TYPE(found_abil), ABIL_COST(found_abil), NOTHING, 0, WAIT_SPELL);
	
	// load the object
	obj = read_object(GET_OBJ_VNUM(proto), TRUE);
	if (IS_CLASS_ABILITY(ch, ABIL_VNUM(found_abil)) || ABIL_ASSIGNED_SKILL(found_abil) == NULL) {
		scale_level = ch_level;	// class-level
	}
	else {
		scale_level = MIN(ch_level, get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(found_abil))));
	}
	if (GET_SKILL_LEVEL(ch) < CLASS_SKILL_CAP) {	// ensure they'll be able to use the final item
		scale_level = MIN(scale_level, GET_SKILL_LEVEL(ch));
	}
	scale_item_to_level(obj, scale_level);
	
	switch (ABIL_COST_TYPE(found_abil)) {
		case MANA: {
			act("Mana twists and swirls around your hand and becomes $p!", FALSE, ch, obj, NULL, TO_CHAR);
			act("Mana twists and swirls around $n's hand and becomes $p!", TRUE, ch, obj, NULL, TO_ROOM);
			break;
		}
		case BLOOD: {
			act("You drain blood from your wrist and mold it into $p!", FALSE, ch, obj, NULL, TO_CHAR);
			act("$n twists and molds $s own blood into $p!", TRUE, ch, obj, NULL, TO_ROOM);
			break;
		}
		// HEALTH, MOVE (these could have their own messages if they were used)
		default: {
			act("You pull $p from the ether!", FALSE, ch, obj, NULL, TO_CHAR);
			act("$n pulls $p from the ether!", TRUE, ch, obj, NULL, TO_ROOM);
			break;
		}
	}
	
	equip_char(ch, obj, pos);
	determine_gear_level(ch);
	
	gain_ability_exp(ch, ABIL_VNUM(found_abil), 15);
	
	load_otrigger(obj);
}
