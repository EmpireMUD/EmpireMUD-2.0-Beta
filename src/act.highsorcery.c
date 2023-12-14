/* ************************************************************************
*   File: act.highsorcery.c                               EmpireMUD 2.0b5 *
*  Usage: implementation for high sorcery abilities                       *
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
#include "constants.h"

/**
* Contents:
*   Data
*   Helpers
*   Commands
*/


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

// for do_disenchant
// TODO updates for this:
//  - this should be moved to a live config for general disenchant -> possibly global interactions
//  - add the ability to set disenchant results by gear or scaled level
//  - add an obj interaction to allow custom disenchant results
const struct {
	obj_vnum vnum;
	double chance;
} disenchant_data[] = {
	{ o_LIGHTNING_STONE, 5 },
	{ o_BLOODSTONE, 5 },
	{ o_IRIDESCENT_IRIS, 5 },
	{ o_GLOWING_SEASHELL, 5 },

	// this must go last
	{ NOTHING, 0.0 }
};


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* @param char_data *ch The enchanter.
* @param int max_scale Optional: The highest scale level it will use (0 = no max)
* @return double The number of scale points available for an enchantment at that level.
*/
double get_enchant_scale_for_char(char_data *ch, int max_scale) {
	double points_available;
	int level;

	// enchant scale level is whichever is less: obj scale level, or player crafting level
	level = MAX(get_crafting_level(ch), get_approximate_level(ch));
	if (max_scale > 0) {
		level = MIN(max_scale, level);
	}
	points_available = level / 100.0 * config_get_double("enchant_points_at_100");
	return MAX(points_available, 1.0);
}


/**
* Command processing for the "summon materials" ability, called via the
* central do_summon command.
*
* @param char_data *ch The person using the command.
* @param char *argument The typed arg.
*/
void summon_materials(char_data *ch, char *argument) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], *objname;
	struct empire_storage_data *store, *next_store;
	int count = 0, total = 1, number, pos, carry;
	struct empire_island *isle;
	ability_data *abil;
	empire_data *emp;
	int cost = 2;	// * number of things to summon
	obj_data *proto;
	bool one, found = FALSE, full = FALSE;

	half_chop(argument, arg1, arg2);
	
	if (!has_player_tech(ch, PTECH_SUMMON_MATERIALS)) {
		msg_to_char(ch, "You don't have the right ability to summon materials.\r\n");
		return;
	}
	if (!(emp = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You can't summon empire materials if you're not in an empire.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_STORAGE)) {
		msg_to_char(ch, "You aren't high enough rank to retrieve from the empire inventory.\r\n");
		return;
	}
	if (GET_POS(ch) < POS_RESTING) {
		send_low_pos_msg(ch);
		return;
	}
	if (AFF_FLAGGED(ch, AFF_STUNNED | AFF_HARD_STUNNED)) {
		msg_to_char(ch, "You can't do that... you're stunned!\r\n");
		return;
	}
	
	if (!GET_ISLAND(IN_ROOM(ch)) || !(isle = get_empire_island(emp, GET_ISLAND_ID(IN_ROOM(ch))))) {
		msg_to_char(ch, "You can't summon materials here.\r\n");
		return;
	}
	
	if (run_ability_triggers_by_player_tech(ch, PTECH_SUMMON_MATERIALS, NULL, NULL)) {
		return;
	}
	
	// pull out a number if the first arg is a number
	if (*arg1 && is_number(arg1)) {
		total = atoi(arg1);
		if (total < 1) {
			msg_to_char(ch, "You have to summon at least 1.\r\n");
			return;
		}
		strcpy(arg1, arg2);
	}
	else if (*arg1 && *arg2) {
		// re-combine if it wasn't a number
		sprintf(arg1 + strlen(arg1), " %s", arg2);
	}
	
	// arg1 now holds the desired name
	objname = arg1;
	number = get_number(&objname);

	// multiply cost for total, but don't store it
	if (!can_use_ability(ch, NO_ABIL, MANA, cost * total, NOTHING)) {
		return;
	}

	if (!*objname) {
		msg_to_char(ch, "What material would you like to summon (use einventory to see what you have)?\r\n");
		return;
	}
	
	// messaging (allow custom messages)
	abil = find_player_ability_by_tech(ch, PTECH_SUMMON_MATERIALS);
	if (abil && abil_has_custom_message(abil, ABIL_CUSTOM_SELF_TO_CHAR)) {
		act(abil_get_custom_message(abil, ABIL_CUSTOM_SELF_TO_CHAR), FALSE, ch, NULL, NULL, TO_CHAR);
	}
	else {
		act("You open a tiny portal to summon materials...", FALSE, ch, NULL, NULL, TO_CHAR);
	}
	if (abil && abil_has_custom_message(abil, ABIL_CUSTOM_SELF_TO_ROOM)) {
		act(abil_get_custom_message(abil, ABIL_CUSTOM_SELF_TO_ROOM), ABILITY_FLAGGED(abil, ABILF_INVISIBLE) ? TRUE : FALSE, ch, NULL, NULL, TO_CHAR);
	}
	else {
		act("$n opens a tiny portal to summon materials...", (abil && ABILITY_FLAGGED(abil, ABILF_INVISIBLE)) ? TRUE : FALSE, ch, NULL, NULL, TO_ROOM);
	}
	
	pos = 0;
	HASH_ITER(hh, isle->store, store, next_store) {
		if (found) {
			break;
		}
		if (store->amount < 1) {
			continue;	// none of this
		}
		
		proto = store->proto;
		if (proto && multi_isname(objname, GET_OBJ_KEYWORDS(proto)) && (++pos == number)) {
			found = TRUE;
			
			if (!CAN_WEAR(proto, ITEM_WEAR_TAKE)) {
				msg_to_char(ch, "You can't summon %s.\r\n", GET_OBJ_SHORT_DESC(proto));
				return;
			}
			if (stored_item_requires_withdraw(proto)) {
				msg_to_char(ch, "You can't summon materials out of the vault.\r\n");
				return;
			}
			
			while (count < total && store->amount > 0) {
				carry = IS_CARRYING_N(ch);
				one = retrieve_resource(ch, emp, store, FALSE);
				if (IS_CARRYING_N(ch) > carry) {
					++count;	// got one
				}
				if (!one) {
					full = TRUE;	// probably
					break;	// done with this loop
				}
			}
		}
	}
	
	if (found && count < total && count > 0) {
		if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch) || full) {
			if (ch->desc) {
				stack_msg_to_desc(ch->desc, "You managed to summon %d.\r\n", count);
			}
		}
		else if (ch->desc) {
			stack_msg_to_desc(ch->desc, "There weren't enough, but you managed to summon %d.\r\n", count);
		}
	}
	
	// result messages
	if (!found) {
		if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch) || full) {
			msg_to_char(ch, "Your arms are full.\r\n");
		}
		else {
			msg_to_char(ch, "Nothing like that is stored around here.\r\n");
		}
	}
	else if (count == 0) {
		// they must have gotten an error message
	}
	else {
		// save the empire
		if (found) {
			set_mana(ch, GET_MANA(ch) - (cost * count));	// charge only the amount retrieved
			read_vault(emp);
			gain_player_tech_exp(ch, PTECH_SUMMON_MATERIALS, 1);
		}
	}
	
	command_lag(ch, WAIT_OTHER);
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////


ACMD(do_collapse) {
	obj_data *portal, *obj, *reverse = NULL;
	room_data *to_room;
	int cost = 15;
	
	one_argument(argument, arg);
	
	if (!has_player_tech(ch, PTECH_PORTAL_UPGRADE)) {
		msg_to_char(ch, "You don't have the correct ability to collapse portals.\r\n");
		return;
	}
	if (!can_use_ability(ch, NO_ABIL, MANA, cost, NOTHING)) {
		return;
	}
	
	if (!*arg) {
		msg_to_char(ch, "Collapse which portal?\r\n");
		return;
	}
	
	if (!(portal = get_obj_in_list_vis(ch, arg, NULL, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "You don't see %s %s here.\r\n", AN(arg), arg);
		return;
	}
	
	if (!IS_PORTAL(portal) || GET_OBJ_TIMER(portal) == UNLIMITED) {
		msg_to_char(ch, "You can't collapse that.\r\n");
		return;
	}
	
	if (!(to_room = real_room(GET_PORTAL_TARGET_VNUM(portal)))) {
		msg_to_char(ch, "You can't collapse that.\r\n");
		return;
	}
	
	// find the reverse portal
	DL_FOREACH2(ROOM_CONTENTS(to_room), obj, next_content) {
		if (GET_PORTAL_TARGET_VNUM(obj) == GET_ROOM_VNUM(IN_ROOM(ch))) {
			reverse = obj;
			break;
		}
	}

	// do it
	charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
	
	act("You grab $p and draw it shut!", FALSE, ch, portal, NULL, TO_CHAR);
	act("$n grabs $p and draws it shut!", FALSE, ch, portal, NULL, TO_ROOM);
	
	extract_obj(portal);
	
	if (reverse) {
		if (ROOM_PEOPLE(to_room)) {
			act("$p is collapsed from the other side!", FALSE, ROOM_PEOPLE(to_room), reverse, NULL, TO_CHAR | TO_ROOM);
		}
		extract_obj(reverse);
	}
	
	gain_player_tech_exp(ch, PTECH_PORTAL_UPGRADE, 20);
}


ACMD(do_colorburst) {
	char_data *vict = NULL;
	struct affected_type *af;
	int amt;
	
	int costs[] = { 15, 30, 45 };
	int levels[] = { -5, -10, -15 };
	
	if (!can_use_ability(ch, ABIL_COLORBURST, MANA, CHOOSE_BY_ABILITY_LEVEL(costs, ch, ABIL_COLORBURST), COOLDOWN_COLORBURST)) {
		return;
	}
	
	// find target
	one_argument(argument, arg);
	if (*arg && !(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
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
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_COLORBURST)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, CHOOSE_BY_ABILITY_LEVEL(costs, ch, ABIL_COLORBURST), COOLDOWN_COLORBURST, 30, WAIT_COMBAT_SPELL);
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// counterspell??
	if (trigger_counterspell(vict) || AFF_FLAGGED(vict, AFF_IMMUNE_MAGICAL_DEBUFFS)) {
		act("You fire a burst of color at $N, but $E deflects it!", FALSE, ch, NULL, vict, TO_CHAR | TO_ABILITY);
		act("$n fires a burst of color at you, but it's deflected by your counterspell!", FALSE, ch, NULL, vict, TO_VICT | TO_ABILITY);
		act("$n fires a burst of color at $N, but $E deflects it.", FALSE, ch, NULL, vict, TO_NOTVICT | TO_ABILITY);
	}
	else {
		// succeed
	
		act("You whip your hand forward and fire a burst of color at $N!", FALSE, ch, NULL, vict, TO_CHAR | TO_ABILITY);
		act("$n whips $s hand forward and fires a burst of color at you!", FALSE, ch, NULL, vict, TO_VICT | TO_ABILITY);
		act("$n whips $s hand forward and fires a burst of color at $N!", FALSE, ch, NULL, vict, TO_NOTVICT | TO_ABILITY);
		
		amt = CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_COLORBURST) - GET_INTELLIGENCE(ch);
	
		af = create_mod_aff(ATYPE_COLORBURST, 30, APPLY_TO_HIT, amt, ch);
		affect_join(vict, af, 0);

		engage_combat(ch, vict, FALSE);
	}
	
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, ABIL_COLORBURST, 15);
	}
}


ACMD(do_disenchant) {
	struct obj_apply *apply, *next_apply;
	obj_data *obj, *reward;
	int iter, prc, rnd;
	obj_vnum vnum = NOTHING;
	int cost = 5;
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_DISENCHANT, MANA, cost, NOTHING)) {
		// sends its own messages
	}
	else if (!*arg) {
		msg_to_char(ch, "Disenchant what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (ABILITY_TRIGGERS(ch, NULL, obj, ABIL_DISENCHANT)) {
		return;
	}
	else if (!bind_ok(obj, ch)) {
		msg_to_char(ch, "You can't disenchant something that is bound to someone else.\r\n");
	}
	else if (!OBJ_FLAGGED(obj, OBJ_ENCHANTED)) {
		act("$p is not even enchanted.", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else {
		charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
		REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_ENCHANTED);
		
		for (apply = GET_OBJ_APPLIES(obj); apply; apply = next_apply) {
			next_apply = apply->next;
			if (apply->apply_type == APPLY_TYPE_ENCHANTMENT) {
				LL_DELETE(GET_OBJ_APPLIES(obj), apply);
				free(apply);
			}
		}
		
		act("You hold $p over your head and shout 'KA!' as you release the mana bound to it!\r\nThere is a burst of red light from $p and then it fizzles and is disenchanted.", FALSE, ch, obj, NULL, TO_CHAR);
		act("$n shouts 'KA!' and cracks $p, which blasts out red light, and then fizzles.", FALSE, ch, obj, NULL, TO_ROOM);
		gain_ability_exp(ch, ABIL_DISENCHANT, 33.4);
		
		request_obj_save_in_world(obj);
		
		// obj back?
		if (skill_check(ch, ABIL_DISENCHANT, DIFF_MEDIUM)) {
			rnd = number(1, 10000);
			prc = 0;
			for (iter = 0; disenchant_data[iter].vnum != NOTHING && vnum == NOTHING; ++iter) {
				prc += disenchant_data[iter].chance * 100;
				if (rnd <= prc) {
					vnum = disenchant_data[iter].vnum;
				}
			}
		
			if (vnum != NOTHING) {
				reward = read_object(vnum, TRUE);
				obj_to_char(reward, ch);
				act("You manage to weave the freed mana into $p!", FALSE, ch, reward, NULL, TO_CHAR);
				act("$n weaves the freed mana into $p!", TRUE, ch, reward, NULL, TO_ROOM);
				if (load_otrigger(reward)) {
					get_otrigger(reward, ch, FALSE);
				}
			}
		}
	}
}


ACMD(do_dispel) {
	struct over_time_effect_type *dot, *next_dot;
	char_data *vict = ch;
	int cost = 30;
	bool found;
	
	one_argument(argument, arg);
	
	
	if (!can_use_ability(ch, ABIL_DISPEL, MANA, cost, COOLDOWN_DISPEL)) {
		return;
	}
	else if (*arg && !(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_DISPEL)) {
		return;
	}
	else {
		// verify they have dots
		found = FALSE;
		for (dot = vict->over_time_effects; dot && !found; dot = dot->next) {
			if (dot->damage_type == DAM_MAGICAL || dot->damage_type == DAM_FIRE) {
				found = TRUE;
			}
		}
		if (!found) {
			if (ch == vict) {
				msg_to_char(ch, "You aren't afflicted by anything that can be dispelled.\r\n");
			}
			else {
				act("$E isn't afflicted by anything that can be dispelled.", FALSE, ch, NULL, vict, TO_CHAR);
			}
			return;
		}
	
		charge_ability_cost(ch, MANA, cost, COOLDOWN_DISPEL, 9, WAIT_SPELL);
		
		if (ch == vict) {
			msg_to_char(ch, "You shout 'KA!' and dispel your afflictions.\r\n");
			act("$n shouts 'KA!' and dispels $s afflictions.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			act("You shout 'KA!' and dispel $N's afflictions.", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n shouts 'KA!' and dispels your afflictions.", FALSE, ch, NULL, vict, TO_VICT);
			act("$n shouts 'KA!' and dispels $N's afflictions.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}

		// remove DoTs
		for (dot = vict->over_time_effects; dot; dot = next_dot) {
			next_dot = dot->next;

			if (dot->damage_type == DAM_MAGICAL || dot->damage_type == DAM_FIRE) {
				dot_remove(vict, dot);
			}
		}
		
		if (can_gain_exp_from(ch, vict)) {
			gain_ability_exp(ch, ABIL_DISPEL, 33.4);
		}

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
	}
}


ACMD(do_enervate) {
	char_data *vict = NULL;
	struct affected_type *af, *af2;
	int cost = 20;
	
	int levels[] = { 1, 1, 3 };
		
	if (!can_use_ability(ch, ABIL_ENERVATE, MANA, cost, COOLDOWN_ENERVATE)) {
		return;
	}
	
	// find target
	one_argument(argument, arg);
	if (*arg && !(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
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
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_ENERVATE)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, COOLDOWN_ENERVATE, 75, WAIT_COMBAT_SPELL);
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// counterspell??
	if (trigger_counterspell(vict) || AFF_FLAGGED(vict, AFF_IMMUNE_MAGICAL_DEBUFFS)) {
		act("You attempt to hex $N with enervate, but it fails!", FALSE, ch, NULL, vict, TO_CHAR | TO_ABILITY);
		act("$n attempts to hex you with enervate, but it's deflected by your counterspell!", FALSE, ch, NULL, vict, TO_VICT | TO_ABILITY);
		act("$n attempts to hex $N with enervate, but it fails!", FALSE, ch, NULL, vict, TO_NOTVICT | TO_ABILITY);
	}
	else {
		// succeed
	
		act("$N starts to glow red as you shout the enervate hex at $M! You feel your own stamina grow as you drain $S.", FALSE, ch, NULL, vict, TO_CHAR | TO_ABILITY);
		act("$n shouts something at you... The world takes on a reddish hue and you feel your stamina drain.", FALSE, ch, NULL, vict, TO_VICT | TO_ABILITY);
		act("$n shouts some kind of hex at $N, who starts to glow red and seems weakened!", FALSE, ch, NULL, vict, TO_NOTVICT | TO_ABILITY);
	
		af = create_mod_aff(ATYPE_ENERVATE, 75, APPLY_MOVE_REGEN, -1 * GET_INTELLIGENCE(ch) / 2, ch);
		affect_join(vict, af, 0);
		af2 = create_mod_aff(ATYPE_ENERVATE_GAIN, 75, APPLY_MOVE_REGEN, CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_ENERVATE), ch);
		affect_join(ch, af2, 0);

		engage_combat(ch, vict, FALSE);
	}
	
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, ABIL_ENERVATE, 15);
	}
}


ACMD(do_manashield) {
	struct affected_type *af1, *af2, *af3;
	int cost = 25;
	int amt;
	
	int levels[] = { 2, 4, 6 };

	if (affected_by_spell(ch, ATYPE_MANASHIELD)) {
		msg_to_char(ch, "You wipe the symbols off your arm and cancel your mana shield.\r\n");
		act("$n wipes the arcane symbols off $s arm.", TRUE, ch, NULL, NULL, TO_ROOM);
		affect_from_char(ch, ATYPE_MANASHIELD, FALSE);
	}
	else if (!can_use_ability(ch, ABIL_MANASHIELD, MANA, cost, NOTHING)) {
		return;
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_MANASHIELD)) {
		return;
	}
	else {
		charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
		
		msg_to_char(ch, "You pull out a grease pencil and draw a series of arcane symbols down your left arm...\r\nYou feel yourself shielded by your mana!\r\n");
		act("$n pulls out a grease pencil and draws a series of arcane symbols down $s left arm.", TRUE, ch, NULL, NULL, TO_ROOM);
		
		amt = CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_MANASHIELD) + (GET_INTELLIGENCE(ch) / 3);
		
		af1 = create_mod_aff(ATYPE_MANASHIELD, 30 * SECS_PER_REAL_MIN, APPLY_MANA, -25, ch);
		affect_to_char(ch, af1);
		free(af1);
		
		af2 = create_mod_aff(ATYPE_MANASHIELD, 30 * SECS_PER_REAL_MIN, APPLY_RESIST_PHYSICAL, amt, ch);
		affect_to_char(ch, af2);
		free(af2);
		
		af3 = create_mod_aff(ATYPE_MANASHIELD, 30 * SECS_PER_REAL_MIN, APPLY_RESIST_MAGICAL, amt, ch);
		affect_to_char(ch, af3);
		free(af3);
		
		// possible to go negative here
		if (GET_MANA(ch) < 0) {
			set_mana(ch, 0);
		}
	}
}


ACMD(do_mirrorimage) {
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], *tmp;
	char_data *mob, *other;
	obj_data *wield;
	int cost = GET_MAX_MANA(ch) / 5;
	mob_vnum vnum = MIRROR_IMAGE_MOB;
	struct custom_message *ocm;
	bool found;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use mirrorimage.\r\n");
		return;
	}
	
	if (!can_use_ability(ch, ABIL_MIRRORIMAGE, MANA, cost, COOLDOWN_MIRRORIMAGE)) {
		return;
	}
	
	// limit 1
	found = FALSE;
	DL_FOREACH(character_list, other) {
		if (ch != other && IS_NPC(other) && GET_MOB_VNUM(other) == vnum && GET_LEADER(other) == ch) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		msg_to_char(ch, "You can't summon a second mirror image.\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_MIRRORIMAGE)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, COOLDOWN_MIRRORIMAGE, 5 * SECS_PER_REAL_MIN, WAIT_COMBAT_SPELL);
	mob = read_mobile(vnum, TRUE);
	
	// scale mob to the summoner -- so it won't change its attributes later
	scale_mob_as_companion(mob, ch, 0);
	
	char_to_room(mob, IN_ROOM(ch));
	
	// restrings
	GET_PC_NAME(mob) = str_dup(PERS(ch, ch, FALSE));
	GET_SHORT_DESC(mob) = str_dup(GET_PC_NAME(mob));
	change_sex(mob, GET_SEX(ch));	// need this for some desc stuff
	
	// longdesc is more complicated
	if (GET_MORPH(ch)) {
		sprintf(buf, "%s\r\n", get_morph_desc(ch, TRUE));
	}
	else if ((ocm = pick_custom_longdesc(ch))) {
		sprintf(buf, "%s\r\n", ocm->msg);
		
		// must process $n, $s, $e, $m
		if (strstr(buf, "$n")) {
			tmp = str_replace("$n", GET_SHORT_DESC(mob), buf);
			strcpy(buf, tmp);
			free(tmp);
		}
		if (strstr(buf, "$s")) {
			tmp = str_replace("$s", HSHR(mob), buf);
			strcpy(buf, tmp);
			free(tmp);
		}
		if (strstr(buf, "$e")) {
			tmp = str_replace("$e", HSSH(mob), buf);
			strcpy(buf, tmp);
			free(tmp);
		}
		if (strstr(buf, "$m")) {
			tmp = str_replace("$m", HMHR(mob), buf);
			strcpy(buf, tmp);
			free(tmp);
		}

	}
	else {
		sprintf(buf, "%s is standing here.\r\n", GET_SHORT_DESC(mob));
	}
	*buf = UPPER(*buf);
	
	// attach rank if needed
	if (GET_LOYALTY(ch)) {
		sprintf(buf2, "<%s&0&y> %s", EMPIRE_RANK(GET_LOYALTY(ch), GET_RANK(ch)-1), buf);
		GET_LONG_DESC(mob) = str_dup(buf2);
	}
	else {
		GET_LONG_DESC(mob) = str_dup(buf);
	}
	
	// stats
	
	// inherit scaled mob health
	// mob->points.max_pools[HEALTH] = get_approximate_level(ch) * level_health_mod;
	// mob->points.current_pools[HEALTH] = mob->points.max_pools[HEALTH];
	mob->points.max_pools[MOVE] = GET_MAX_MOVE(ch);
	mob->points.current_pools[MOVE] = GET_MOVE(ch);
	mob->points.max_pools[MANA] = GET_MAX_MANA(ch);
	mob->points.current_pools[MANA] = GET_MANA(ch);
	mob->points.max_pools[BLOOD] = 10;	// not meant to be a blood source
	mob->points.current_pools[BLOOD] = 10;
	
	// mimic weapon
	wield = GET_EQ(ch, WEAR_WIELD);
	MOB_ATTACK_TYPE(mob) = wield ? GET_WEAPON_TYPE(wield) : TYPE_HIT;
	MOB_DAMAGE(mob) = 3;	// deliberately low (it will miss anyway)
	
	// mirrors are no good at hitting or dodging
	mob->mob_specials.to_hit = 0;
	mob->mob_specials.to_dodge = 0;
	
	mob->real_attributes[STRENGTH] = GET_STRENGTH(ch);
	mob->real_attributes[DEXTERITY] = GET_DEXTERITY(ch);
	mob->real_attributes[CHARISMA] = GET_CHARISMA(ch);
	mob->real_attributes[GREATNESS] = GET_GREATNESS(ch);
	mob->real_attributes[INTELLIGENCE] = GET_INTELLIGENCE(ch);
	mob->real_attributes[WITS] = GET_WITS(ch);

	SET_BIT(AFF_FLAGS(mob), AFF_CHARM);
	set_mob_flags(mob, MOB_NO_RESCALE);
	affect_total(mob);
	
	act("You create a mirror image to distract your foes!", FALSE, ch, NULL, NULL, TO_CHAR);
	
	// switch at least 1 thing that's hitting ch
	found = FALSE;
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), other, next_in_room) {
		if (FIGHTING(other) == ch) {
			if (!found || !number(0, 1)) {
				found = TRUE;
				FIGHTING(other) = mob;
			}
		}
		else if (other != ch) {	// only people not hitting ch see the split
			act("$n suddenly splits in two!", TRUE, ch, NULL, other, TO_VICT);
		}
	}
	
	if (FIGHTING(ch)) {
		set_fighting(mob, FIGHTING(ch), FIGHT_MODE(ch));
	}
	
	add_follower(mob, ch, FALSE);
	gain_ability_exp(ch, ABIL_MIRRORIMAGE, 15);
	
	load_mtrigger(mob);
}


ACMD(do_siphon) {
	char_data *vict = NULL;
	struct affected_type *af;
	int cost = 10;
	
	int levels[] = { 2, 5, 9 };
		
	if (!can_use_ability(ch, ABIL_SIPHON, MANA, cost, COOLDOWN_SIPHON)) {
		return;
	}
	if (get_cooldown_time(ch, COOLDOWN_SIPHON) > 0) {
		msg_to_char(ch, "Siphon is still on cooldown.\r\n");
		return;
	}
	
	// find target
	one_argument(argument, arg);
	if (*arg && !(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
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
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_SIPHON)) {
		return;
	}
	
	if (GET_MANA(vict) <= 0) {
		msg_to_char(ch, "Your target has no mana to siphon.\r\n");
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, COOLDOWN_SIPHON, 20, WAIT_COMBAT_SPELL);
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// counterspell??
	if (trigger_counterspell(vict) || AFF_FLAGGED(vict, AFF_IMMUNE_MAGICAL_DEBUFFS)) {
		act("You try to siphon mana from $N, but are deflected by a counterspell!", FALSE, ch, NULL, vict, TO_CHAR | TO_ABILITY);
		act("$n tries to siphon mana from you, but it's deflected by your counterspell!", FALSE, ch, NULL, vict, TO_VICT | TO_ABILITY);
		act("$n tries to siphon mana from $N, but it fails!", FALSE, ch, NULL, vict, TO_NOTVICT | TO_ABILITY);
	}
	else {
		// succeed
	
		act("$N starts to glow violet as you shout the mana siphon hex at $M! You feel your own mana grow as you drain $S.", FALSE, ch, NULL, vict, TO_CHAR | TO_ABILITY);
		act("$n shouts something at you... The world takes on a violet glow and you feel your mana siphoned away.", FALSE, ch, NULL, vict, TO_VICT | TO_ABILITY);
		act("$n shouts some kind of hex at $N, who starts to glow violet as mana flows away from $S skin!", FALSE, ch, NULL, vict, TO_NOTVICT | TO_ABILITY);

		af = create_mod_aff(ATYPE_SIPHON, 20, APPLY_MANA_REGEN, CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_SIPHON), ch);
		affect_join(ch, af, 0);
		
		af = create_mod_aff(ATYPE_SIPHON_DRAIN, 20, APPLY_MANA_REGEN, -1 * CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_SIPHON), ch);
		affect_join(vict, af, 0);

		engage_combat(ch, vict, FALSE);
	}
	
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, ABIL_SIPHON, 15);
	}
}


ACMD(do_vigor) {
	struct affected_type *af;
	char_data *vict = ch, *iter;
	int gain;
	bool fighting;
	
	// gain levels:		basic, specialty, class skill
	int costs[] = { 15, 25, 40 };
	int move_gain[] = { 30, 65, 125 };
	int bonus_per_intelligence = 5;
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_VIGOR, MANA, CHOOSE_BY_ABILITY_LEVEL(costs, ch, ABIL_VIGOR), NOTHING)) {
		return;
	}
	else if (*arg && !(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_VIGOR)) {
		return;
	}
	else if (GET_MOVE(vict) >= GET_MAX_MOVE(vict)) {
		if (ch == vict) {
			msg_to_char(ch, "You already have full movement points.\r\n");
		}
		else {
			act("$E already has full movement points.", FALSE, ch, NULL, vict, TO_CHAR);
		}
	}
	else if (affected_by_spell(vict, ATYPE_VIGOR)) {
		if (ch == vict) {
			msg_to_char(ch, "You can't cast vigor on yourself again until the effects of the last one have worn off.\r\n");
		}
		else {
			act("$E has had vigor cast on $M too recently to do it again.", FALSE, ch, NULL, vict, TO_CHAR);
		}
	}
	else {
		charge_ability_cost(ch, MANA, CHOOSE_BY_ABILITY_LEVEL(costs, ch, ABIL_VIGOR), NOTHING, 0, WAIT_SPELL);
		
		if (ch == vict) {
			msg_to_char(ch, "You focus your thoughts and say the word 'maktso', and you feel a burst of vigor!\r\n");
			act("$n closes $s eyes and says the word 'maktso', and $e seems suddenly refreshed.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			act("You focus your thoughts and say the word 'maktso', and $N suddenly seems refreshed.", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n closes $s eyes and says the word 'maktso', and you feel a sudden burst of vigor!", FALSE, ch, NULL, vict, TO_VICT);
			act("$n closes $s eyes and says the word 'maktso', and $N suddenly seems refreshed.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}
		
		// check if vict is in combat
		fighting = (FIGHTING(vict) != NULL);
		if (!fighting) {
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_in_room) {
				if (FIGHTING(iter) == vict) {
					fighting = TRUE;
					break;
				}
			}
		}

		gain = CHOOSE_BY_ABILITY_LEVEL(move_gain, ch, ABIL_VIGOR) + (bonus_per_intelligence * GET_INTELLIGENCE(ch));
		
		// bonus if not in combat
		if (!fighting) {
			gain *= 2;
		}
		
		set_move(vict, GET_MOVE(vict) + gain);
		
		// the cast_by on this is vict himself, because it is a penalty and this will block cleanse
		af = create_mod_aff(ATYPE_VIGOR, 75, APPLY_MOVE_REGEN, -5, vict);
		affect_join(vict, af, 0);
		
		gain_ability_exp(ch, ABIL_VIGOR, 33.4);

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
	}
}
