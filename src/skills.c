/* ************************************************************************
*   File: skills.c                                        EmpireMUD 2.0b1 *
*  Usage: code related to the skill and ability system                    *
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
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   End Affect When Skill Lost -- core code for shutting off effects
*   Core Skill Functions
*   Core Skill Commands
*   Skill Helpers
*/

// external variables
void update_class(char_data *ch);

// protos
bool can_gain_skill_from(char_data *ch, int ability);
int get_ability_points_spent(char_data *ch, int skill);
bool green_skill_deadend(char_data *ch, int skill);


 //////////////////////////////////////////////////////////////////////////////
//// END AFFECT WHEN SKILL LOST //////////////////////////////////////////////

/**
* General test function for abilities. It allows mobs who are not charmed to
* use any ability. Otherwise, it supports an energy pool cost and/or cooldowns,
* as well as checking that the player has the ability.
*
* @param char_data *ch The player or NPC.
* @param int ability Any ABIL_x const.
* @param int cost_pool HEALTH, MANA, MOVE, BLOOD (NOTHING if no charge).
* @param int cost_amount Mana (or whatever) amount required, if any.
* @param int cooldown_type Any COOLDOWN_x const, or NOTHING for no cooldown check.
* @return bool TRUE if ch can use ability; FALSE if not.
*/
bool can_use_ability(char_data *ch, int ability, int cost_pool, int cost_amount, int cooldown_type) {
	extern const char *pool_types[];
	
	char buf[MAX_STRING_LENGTH];
	int time, needs_cost;
	
	// this actually blocks npcs, too, so it's higher than other checks
	if (cost_pool == BLOOD && cost_amount > 0 && !CAN_SPEND_BLOOD(ch)) {
		msg_to_char(ch, "Your blood is inert, you can't do that!\r\n");
		return FALSE;
	}
	
	// special rules for npcs
	if (IS_NPC(ch)) {
		return (AFF_FLAGGED(ch, AFF_CHARM) ? FALSE : TRUE);
	}
	
	// special rule: require that blood or health costs not reduce player below 1
	needs_cost = cost_amount + ((cost_pool == HEALTH || cost_pool == BLOOD) ? 1 : 0);
	
	// players:
	if (!HAS_ABILITY(ch, ability)) {
		msg_to_char(ch, "You have not purchased the %s ability.\r\n", ability_data[ability].name);
		return FALSE;
	}
	if (cost_pool >= 0 && cost_pool < NUM_POOLS && cost_amount > 0 && GET_CURRENT_POOL(ch, cost_pool) < needs_cost) {
		msg_to_char(ch, "You need %d %s point%s to do that.\r\n", cost_amount, pool_types[cost_pool], PLURAL(cost_amount));
		return FALSE;
	}
	if (cooldown_type > COOLDOWN_RESERVED && (time = get_cooldown_time(ch, cooldown_type)) > 0) {
		snprintf(buf, sizeof(buf), "%s is still on cooldown for %d second%s.\r\n", ability_data[ability].name, time, (time != 1 ? "s" : ""));
		CAP(buf);
		send_to_char(buf, ch);
		return FALSE;
	}

	return TRUE;
}


/**
* Safely charges a player the cost for using an ability (won't go below 0) and
* applies a cooldown, if applicable. This works on both players and NPCs, but
* only applies the cooldown to players.
*
* The player will also experience universal_wait for using the ability.
*
* @param char_data *ch The player or NPC.
* @param int cost_pool HEALTH, MANA, MOVE, BLOOD (NOTHING if no charge).
* @param int cost_amount Mana (or whatever) amount required, if any.
* @param int cooldown_type Any COOLDOWN_x const to apply (NOTHING for none).
* @param int cooldown_time Cooldown duration, if any.
*/
void charge_ability_cost(char_data *ch, int cost_pool, int cost_amount, int cooldown_type, int cooldown_time) {
	extern const int universal_wait;
	
	if (cost_pool >= 0 && cost_pool < NUM_POOLS && cost_amount > 0) {
		GET_CURRENT_POOL(ch, cost_pool) = MAX(0, GET_CURRENT_POOL(ch, cost_pool) - cost_amount);
	}
	
	// only npcs get cooldowns here
	if (cooldown_type > COOLDOWN_RESERVED && cooldown_time > 0 && !IS_NPC(ch)) {
		add_cooldown(ch, cooldown_type, cooldown_time);
	}
	WAIT_STATE(ch, universal_wait);
}


/**
* Code that must run when skills are sold.
*
* @param char_data *ch
* @param int abil The ability to sell
*/
void check_skill_sell(char_data *ch, int abil) {
	void end_majesty(char_data *ch);
	extern char_data *has_familiar(char_data *ch);
	void remove_armor_by_type(char_data *ch, int armor_type);
	void retract_claws(char_data *ch);
	void undisguise(char_data *ch);	
	
	char_data *vict;
	obj_data *obj;
	bool found = TRUE;	// inverted detection, see default below
	
	// empire_data *emp = GET_LOYALTY(ch);
	
	switch (abil) {
		case ABIL_ALACRITY: {
			void end_alacrity(char_data *ch);
			end_alacrity(ch);
			break;
		}
		case ABIL_BLOODSWORD: {
			if ((obj = GET_EQ(ch, WEAR_WIELD))) {
				if (GET_OBJ_VNUM(obj) == o_BLOODSWORD_LOW || GET_OBJ_VNUM(obj) == o_BLOODSWORD_MEDIUM || GET_OBJ_VNUM(obj) == o_BLOODSWORD_HIGH || GET_OBJ_VNUM(obj) == o_BLOODSWORD_LEGENDARY) {
					act("You stop using $p.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), NULL, TO_CHAR);
					unequip_char_to_inventory(ch, WEAR_WIELD, TRUE);
				}
			}
			break;
		}
		case ABIL_BOOST: {
			affect_from_char(ch, ATYPE_BOOST);
			break;
		}
		case ABIL_CLAWS: {
			retract_claws(ch);
			break;
		}
		case ABIL_DEATHSHROUD: {
			if (affected_by_spell(ch, ATYPE_DEATHSHROUD)) {
				void un_deathshroud(char_data *ch);
				un_deathshroud(ch);
			}
			break;
		}
		case ABIL_DISGUISE: {
			if (IS_DISGUISED(ch)) {
				undisguise(ch);
			}
			break;
		}
		case ABIL_EARTHMELD: {
			if (affected_by_spell(ch, ATYPE_EARTHMELD)) {
				void un_earthmeld(char_data *ch);
				un_earthmeld(ch);
			}
			break;
		}
		case ABIL_FLY: {
			affect_from_char(ch, ATYPE_FLY);
			break;
		}
		case ABIL_HORRID_FORM: {
			if (GET_MORPH(ch) == MORPH_HORRID_FORM) {
				perform_morph(ch, MORPH_NONE);
			}
			break;
		}
		case ABIL_DREAD_BLOOD_FORM: {
			if (GET_MORPH(ch) == MORPH_DREAD_BLOOD) {
				perform_morph(ch, MORPH_NONE);
			}
			break;
		}
		case ABIL_SAVAGE_WEREWOLF_FORM: {
			if (GET_MORPH(ch) == MORPH_SAVAGE_WEREWOLF) {
				perform_morph(ch, MORPH_NONE);
			}
			break;
		}
		case ABIL_TOWERING_WEREWOLF_FORM: {
			if (GET_MORPH(ch) == MORPH_TOWERING_WEREWOLF) {
				perform_morph(ch, MORPH_NONE);
			}
			break;
		}
		case ABIL_SAGE_WEREWOLF_FORM: {
			if (GET_MORPH(ch) == MORPH_SAGE_WEREWOLF) {
				perform_morph(ch, MORPH_NONE);
			}
			break;
		}
		case ABIL_ANIMAL_FORMS: {
			if (GET_MORPH(ch) == MORPH_DEER || GET_MORPH(ch) == MORPH_OSTRICH || GET_MORPH(ch) == MORPH_TAPIR) {
				perform_morph(ch, MORPH_NONE);
			}
			break;
		}
		case ABIL_MAJESTY: {
			end_majesty(ch);
			break;
		}
		case ABIL_MIST_FORM: {
			if (GET_MORPH(ch) == MORPH_MIST) {
				perform_morph(ch, MORPH_NONE);
			}
			break;
		}
		case ABIL_MUMMIFY: {
			if (affected_by_spell(ch, ATYPE_MUMMIFY)) {
				void un_mummify(char_data *ch);
				un_mummify(ch);
			}
			break;
		}
		case ABIL_NIGHTSIGHT: {
			if (affected_by_spell(ch, ATYPE_NIGHTSIGHT)) {
				msg_to_char(ch, "You end your nightsight.\r\n");
				act("The glow in $n's eyes fades.", TRUE, ch, NULL, NULL, TO_ROOM);
				affect_from_char(ch, ATYPE_NIGHTSIGHT);
			}
			break;
		}
		case ABIL_RIDE: {
			if (IS_RIDING(ch)) {
				msg_to_char(ch, "You climb down from your mount.\r\n");
				perform_dismount(ch);
			}
			break;
		}
		case ABIL_FISH: {
			if (GET_ACTION(ch) == ACT_FISHING) {
				cancel_action(ch);
			}
			break;
		}
		case ABIL_NAVIGATION: {
			GET_CONFUSED_DIR(ch) = NORTH;
			break;
		}
		case ABIL_SHIELD_BLOCK: {
			if ((obj = GET_EQ(ch, WEAR_HOLD)) && IS_SHIELD(obj)) {
				act("You stop using $p.", FALSE, ch, GET_EQ(ch, WEAR_HOLD), NULL, TO_CHAR);
				unequip_char_to_inventory(ch, WEAR_HOLD, TRUE);
			}
			break;
		}
		case ABIL_LEATHER_ARMOR: {
			remove_armor_by_type(ch, ARMOR_LEATHER);
			break;
		}
		case ABIL_MEDIUM_ARMOR: {
			remove_armor_by_type(ch, ARMOR_MEDIUM);
			break;
		}
		case ABIL_HEAVY_ARMOR: {
			remove_armor_by_type(ch, ARMOR_HEAVY);
			break;
		}
		case ABIL_CLOTH_ARMOR: {
			remove_armor_by_type(ch, ARMOR_CLOTH);
			break;
		}
		case ABIL_READY_FIREBALL: {
			if ((obj = GET_EQ(ch, WEAR_WIELD))) {
				if (GET_OBJ_VNUM(obj) == o_FIREBALL) {
					act("You stop using $p.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), NULL, TO_CHAR);
					unequip_char_to_inventory(ch, WEAR_WIELD, TRUE);
				}
			}
			break;
		}
		case ABIL_COUNTERSPELL: {
			affect_from_char(ch, ATYPE_COUNTERSPELL);
			break;
		}
		case ABIL_CHANT_OF_NATURE: {
			if (GET_ACTION(ch) == ACT_CHANTING) {
				msg_to_char(ch, "You end the chant.\r\n");
				act("$n ends the chant.", FALSE, ch, NULL, NULL, TO_ROOM);
				cancel_action(ch);
			}
			break;
		}
		case ABIL_RADIANCE: {
			affect_from_char(ch, ATYPE_RADIANCE);
			break;
		}
		case ABIL_BAT_FORM: {
			if (GET_MORPH(ch) == MORPH_BAT) {
				perform_morph(ch, MORPH_NONE);
			}
			break;
		}
		case ABIL_MANASHIELD: {
			affect_from_char(ch, ATYPE_MANASHIELD);
			break;
		}
		case ABIL_FORESIGHT: {
			affect_from_char(ch, ATYPE_FORESIGHT);
			break;
		}
		case ABIL_SIPHON: {
			affect_from_char(ch, ATYPE_SIPHON);
			break;
		}
		case ABIL_PHOENIX_RITE: {
			affect_from_char(ch, ATYPE_PHOENIX_RITE);
			break;
		}
		case ABIL_MIRRORIMAGE: {
			if ((vict = has_familiar(ch)) && !vict->desc && GET_MOB_VNUM(vict) == MIRROR_IMAGE_MOB) {
				act("$n vanishes.", FALSE, vict, NULL, NULL, TO_ROOM);
				extract_char(vict);
			}
			break;
		}
		case ABIL_FAMILIAR: {
			if ((vict = has_familiar(ch)) && !vict->desc) {
				if (GET_MOB_VNUM(vict) == FAMILIAR_CAT || GET_MOB_VNUM(vict) == FAMILIAR_SABERTOOTH || GET_MOB_VNUM(vict) == FAMILIAR_SPHINX || GET_MOB_VNUM(vict) == FAMILIAR_GRIFFIN) {
					act("$n vanishes.", FALSE, vict, NULL, NULL, TO_ROOM);
					extract_char(vict);
				}
			}
			break;
		}
		case ABIL_WOLF_FORM: {
			if (GET_MORPH(ch) == MORPH_WOLF) {
				perform_morph(ch, MORPH_NONE);
			}
			break;
		}
		
		default: {
			found = FALSE;
		}
	}
	
	if (found) {
		affect_total(ch);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CORE SKILL FUNCTIONS ////////////////////////////////////////////////////

/**
* Determines how to color an ability based on its level.
*
* @param char_data *ch The player wit
*/
char *ability_color(char_data *ch, int abil) {
	bool can_buy, has_bought, has_maxed;
		
	has_bought = HAS_ABILITY(ch, abil);
	can_buy = (GET_SKILL(ch, ability_data[abil].parent_skill) >= ability_data[abil].parent_skill_required) && (ability_data[abil].parent_ability == NO_PREREQ || HAS_ABILITY(ch, ability_data[abil].parent_ability));
	has_maxed = has_bought && (GET_LEVELS_GAINED_FROM_ABILITY(ch, abil) >= GAINS_PER_ABILITY || IS_ANY_SKILL_CAP(ch, ability_data[abil].parent_skill) || !can_gain_skill_from(ch, abil));
	
	if (has_bought && has_maxed) {
		return "&g";
	}
	if (has_bought) {
		return "&y";
	}
	if (can_buy) {
		return "&c";
	}
	return "&r";
}


/**
* This function reads abilities out of a player and modifies the empire technology.
*
* @param char_data *ch
* @param empire_data *emp The empire
* @param bool add Adds the abilities if TRUE, or removes them if FALSE
*/
void adjust_abilities_to_empire(char_data *ch, empire_data *emp, bool add) {
	int mod = (add ? 1 : -1);
	
	if (HAS_ABILITY(ch, ABIL_WORKFORCE)) {
		EMPIRE_TECH(emp, TECH_WORKFORCE) += mod;
	}
	if (HAS_ABILITY(ch, ABIL_SKILLED_LABOR)) {
		EMPIRE_TECH(emp, TECH_SKILLED_LABOR) += mod;
	}
	if (HAS_ABILITY(ch, ABIL_TRADE_ROUTES)) {
		EMPIRE_TECH(emp, TECH_TRADE_ROUTES) += mod;
	}
	if (HAS_ABILITY(ch, ABIL_LOCKS)) {
		EMPIRE_TECH(emp, TECH_LOCKS) += mod;
	}
	if (HAS_ABILITY(ch, ABIL_PROMINENCE)) {
		EMPIRE_TECH(emp, TECH_PROMINENCE) += mod;
	}
	if (HAS_ABILITY(ch, ABIL_COMMERCE)) {
		EMPIRE_TECH(emp, TECH_COMMERCE) += mod;
	}
	if (HAS_ABILITY(ch, ABIL_CITY_LIGHTS)) {
		EMPIRE_TECH(emp, TECH_CITY_LIGHTS) += mod;
	}
	if (HAS_ABILITY(ch, ABIL_PORTAL_MAGIC)) {
		EMPIRE_TECH(emp, TECH_PORTALS) += mod;
	}
	if (HAS_ABILITY(ch, ABIL_PORTAL_MASTER)) {
		EMPIRE_TECH(emp, TECH_MASTER_PORTALS) += mod;
	}
}


/**
* @param char_data *ch
* @param int ability ABIL_x
* @return TRUE if ch can still gain skill from ability
*/
bool can_gain_skill_from(char_data *ch, int ability) {
	// must have the ability and not gained too many from it
	if (GET_SKILL(ch, ability_data[ability].parent_skill) < CLASS_SKILL_CAP && HAS_ABILITY(ch, ability) && GET_LEVELS_GAINED_FROM_ABILITY(ch, ability) < GAINS_PER_ABILITY) {
	
		// these limit abilities purchased under each cap to players who are still under that cap
		if (ability_data[ability].parent_skill_required >= BASIC_SKILL_CAP || GET_SKILL(ch, ability_data[ability].parent_skill) < BASIC_SKILL_CAP) {
			if (ability_data[ability].parent_skill_required >= SPECIALTY_SKILL_CAP || GET_SKILL(ch, ability_data[ability].parent_skill) < SPECIALTY_SKILL_CAP) {
				if (ability_data[ability].parent_skill_required >= CLASS_SKILL_CAP || GET_SKILL(ch, ability_data[ability].parent_skill) < CLASS_SKILL_CAP) {
					return TRUE;
				}
			}
		}
	}
	
	return FALSE;
}


/**
* removes all abilities for a player in a given skill
*
* @param char_data *ch the player
* @param int skill which skill, or NO_SKILL for all
*/
void clear_char_abilities(char_data *ch, int skill) {
	void check_skill_sell(char_data *ch, int abil);
	void resort_empires();
	
	int iter;
	empire_data *emp = GET_LOYALTY(ch);
	bool all = (skill == NO_SKILL);
	
	if (!IS_NPC(ch)) {
		// remove ability techs -- only if playing
		if (emp && ch->desc && STATE(ch->desc) == CON_PLAYING) {
			adjust_abilities_to_empire(ch, emp, FALSE);
		}
	
		for (iter = 0; iter < NUM_ABILITIES; ++iter) {
			if (all || ability_data[iter].parent_skill == skill) {
				if (HAS_ABILITY(REAL_CHAR(ch), iter)) {
					check_skill_sell(REAL_CHAR(ch), iter);
				}
				REAL_CHAR(ch)->player_specials->saved.abilities[iter].purchased = 0;
			}
		}
		SAVE_CHAR(ch);
		
		// add ability techs -- only if playing
		if (emp && ch->desc && STATE(ch->desc) == CON_PLAYING) {
			adjust_abilities_to_empire(ch, emp, TRUE);
			resort_empires();
		}
	}
}


/**
* Currently, you get 15 bonus exp per day.
*
* @param char_data *ch
* @return int the number of points ch can earn per day
*/
int compute_bonus_exp_per_day(char_data *ch) {
	int perdiem = 0;
	
	if (!IS_NPC(ch)) {
		perdiem = config_get_int("num_daily_skill_points");
	}
	
	if (HAS_BONUS_TRAIT(ch, BONUS_EXTRA_DAILY_SKILLS)) {
		perdiem += config_get_int("num_bonus_trait_daily_skills");
	}
	
	return perdiem;
}


/**
* Gives a skillup for anybody in the empire with the ability.
*
* @param empire_data *emp the empire to skillup
* @param double amount The amount of experience to gain
*/
void empire_skillup(empire_data *emp, int ability, double amount) {
	descriptor_data *d;
	char_data *ch;
	
	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) == CON_PLAYING && (ch = d->character)) {
			if (GET_LOYALTY(ch) == emp) {
				gain_ability_exp(ch, ability, amount);
			}
		}
	}
}

/**
* char *name input
* return int an ability number or NO_ABIL for not found
*/
int find_ability_by_name(char *name, bool allow_abbrev) {
	int iter, found = NO_ABIL, backup = NO_ABIL;
	
	for (iter = 0; iter < NUM_ABILITIES && found == NO_ABIL; ++iter) {
		if (ability_data[iter].name != NULL) {
			if (!str_cmp(name, ability_data[iter].name)) {
				found = iter;
			}
			else if (is_abbrev(name, ability_data[iter].name)) {
				if (allow_abbrev) {
					found = iter;
				}
				else if (backup == NO_ABIL) {
					backup = iter;
				}
			}
		}
	}
	
	// nothing
	return found != NO_ABIL ? found : backup;
}


/**
* char *name input
* return int a skill number or NO_SKILL for not found
*/
int find_skill_by_name(char *name) {
	int iter;
	for (iter = 0; iter < NUM_SKILLS; ++iter) {
		if (is_abbrev(name, skill_data[iter].name)) {
			return iter;
		}
	}
	
	// no?
	return NO_SKILL;
}


/**
* This is the main interface for ability-based skill learning. Ability gain
* caps are checked here. The amount to gain is automatically reduced if the
* player has no daily points available.
*
* @param char_data *ch The player character who will gain.
* @param int ability The ABIL_x const to gain.
* @param double amount The amount to gain (0-100).
*/
void gain_ability_exp(char_data *ch, int ability, double amount) {
	int skill = ability_data[ability].parent_skill;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	if (!can_gain_skill_from(ch, ability)) {
		return;
	}
		
	// try gain
	if (skill != NO_SKILL && gain_skill_exp(ch, skill, amount)) {
		// increment gains from this
		ch->player_specials->saved.abilities[ability].levels_gained += 1;
	}
}


/**
* Mostly-raw skill gain/loss -- slightly more checking than set_skill(). This
* will not pass skill cap boundaries. It will NEVER gain you from 0 either.
*
* @param char_data *ch The character who is to gain/lose the skill.
* @param int skill The skill to gain/lose.
* @param int amount The number of skill points to gain/lose (gaining will stop at any cap).
* @param bool Returns TRUE if a skill point was gained or lost.
*/
bool gain_skill(char_data *ch, int skill, int amount) {
	bool any = FALSE, pos = (amount > 0);
	int points;
	
	if (skill == NO_SKILL || skill >= MAX_SKILLS || !ch || IS_NPC(ch)) {
		return FALSE;
	}
	
	// inability to gain from 0?
	if (GET_SKILL(ch, skill) == 0 && !CAN_GAIN_NEW_SKILLS(ch)) {
		return FALSE;
	}
	
	// reasons a character would not gain no matter what
	if (amount > 0 && NOSKILL_BLOCKED(ch, skill)) {
		return FALSE;
	}

	while (amount != 0 && (amount > 0 || GET_SKILL(ch, skill) > 0) && (amount < 0 || !IS_ANY_SKILL_CAP(ch, skill))) {
		any = TRUE;
		if (pos) {
			set_skill(ch, skill, GET_SKILL(ch, skill) + 1);
			--amount;
		}
		else {
			set_skill(ch, skill, GET_SKILL(ch, skill) - 1);
			++amount;
		}
	}
	
	if (any) {
		SAVE_CHAR(ch);
		
		// messaging
		if (pos) {
			msg_to_char(ch, "&yYou improve your %s skill to %d.&0\r\n", skill_data[skill].name, GET_SKILL(ch, skill));
	
			points = get_ability_points_available_for_char(ch, skill);
			if (points > 0) {
				msg_to_char(ch, "&yYou have %d ability point%s to spend. Type 'skill %s' to see %s.&0\r\n", points, (points != 1 ? "s" : ""), skill_data[skill].name, (points != 1 ? "them" : "it"));
			}
	
			// did we hit a cap? free reset!
			if (IS_ANY_SKILL_CAP(ch, skill)) {
				GET_FREE_SKILL_RESETS(ch, skill) = MIN(GET_FREE_SKILL_RESETS(ch, skill) + 1, MAX_SKILL_RESETS);
				msg_to_char(ch, "&yYou have earned a free skill reset in %s. Type 'skill reset %s' to use it.&0\r\n", skill_data[skill].name, skill_data[skill].name);
			}
		}
		else {
			msg_to_char(ch, "&yYour %s skill drops to %d.&0\r\n", skill_data[skill].name, GET_SKILL(ch, skill));
		}
		
		// update class and progression
		update_class(ch);
	}
	
	return any;
}


/**
* This is the main interface for leveling skills. Every time a skill gains
* experience, it has a chance to level up. The chance to level up grows as
* the skill approaches 100. Passing an amount of 100 guarantees a skillup,
* if the character can gain at all.
*
* This function also accounts for daily bonus exp, and automatically
* reduces amount if the character has no daily points left.
*
* @param char_data *ch The player character who will gain.
* @param int skill The skill to gain experience in.
* @param double amount The amount to gain (0-100).
* @return bool TRUE if the character gained a skill point from the exp.
*/
bool gain_skill_exp(char_data *ch, int skill, double amount) {
	bool gained;
	
	// simply sanitation
	if (amount <= 0 || skill == NOTHING || skill >= MAX_SKILLS || !ch || IS_NPC(ch)) {
		return FALSE;
	}
	
	// reasons a character would not gain no matter what
	if (NOSKILL_BLOCKED(ch, skill) || IS_ANY_SKILL_CAP(ch, skill)) {
		return FALSE;
	}

	// this allows bonus skillups...
	if (GET_DAILY_BONUS_EXPERIENCE(ch) <= 0) {
		amount /= 50.0;
	}
	
	// gain the exp
	GET_SKILL_EXP(ch, skill) = MIN(100.0, GET_SKILL_EXP(ch, skill) + amount);
	
	// can gain at all?
	if (GET_SKILL_EXP(ch, skill) < config_get_int("min_exp_to_roll_skillup")) {
		return FALSE;
	}
	
	// check for skill gain
	gained = (number(1, 100) <= GET_SKILL_EXP(ch, skill));
	
	if (gained) {
		GET_DAILY_BONUS_EXPERIENCE(ch) = MAX(0, GET_DAILY_BONUS_EXPERIENCE(ch) - 1);
		gained = gain_skill(ch, skill, 1);
	}
	
	return gained;
}


/**
* Determines what skill level the player uses an ability at. This is usually
* based on the parent skill of the ability for players, and the scaled level
* for NPCs. In all cases, it returns a range of 0-100.
*
* @param char_data *ch The player or NPC.
* @param int Any ABIL_x const.
* @return int A skill level of 0 to 100.
*/
int get_ability_level(char_data *ch, int ability) {
	if (IS_NPC(ch)) {
		return get_approximate_level(ch);
	}
	else {
		// players
		if (ability != NO_ABIL && !HAS_ABILITY(ch, ability)) {
			return 0;
		}
		else if (ability != NO_ABIL && ability_data[ability].parent_skill != NO_SKILL) {
			return GET_SKILL(ch, ability_data[ability].parent_skill);
		}
		else {
			return MIN(CLASS_SKILL_CAP, GET_COMPUTED_LEVEL(ch));
		}
	}
}


/**
* Gets the computed level of a player, or the scaled level of a mob. If the
* mob has not yet been scaled, it picks the lowest valid level for the mob.
*
* @param char_data *ch The player or NPC.
* @return int The computed level.
*/
int get_approximate_level(char_data *ch) {
	int level = 1;
	
	if (IS_NPC(ch)) {
		if (GET_CURRENT_SCALE_LEVEL(ch) > 0) {
			level = GET_CURRENT_SCALE_LEVEL(ch);
		}
		else {
			if (GET_MAX_SCALE_LEVEL(ch) > 0) {
				level = MIN(GET_MAX_SCALE_LEVEL(ch), level);
			}
			if (GET_MIN_SCALE_LEVEL(ch) > 0) {
				level = MAX(GET_MIN_SCALE_LEVEL(ch), level);
			}
		}
	}
	else {
		// player
		level = GET_COMPUTED_LEVEL(ch);
	}
	
	return level;
}


/**
* int skill which SKILL_x
* int level at what level
* return int how many abilities are available by that level
*/
int get_ability_points_available(int skill, int level) {
	extern int master_ability_levels[];
	
	int iter, count;
	
	count = 0;
	for (iter = 0; master_ability_levels[iter] != -1; ++iter) {
		if (level >= master_ability_levels[iter]) {
			++count;
		}
	}
	
	return count;
}


/**
* char_data *ch the person to check
* int skill Which SKILL_x to check
* return int how many ability points ch has available in skill
*/
int get_ability_points_available_for_char(char_data *ch, int skill) {
	int max = get_ability_points_available(skill, NEXT_CAP_LEVEL(ch, skill));
	int spent = get_ability_points_spent(ch, skill);
	int avail = MAX(0, get_ability_points_available(skill, GET_SKILL(ch, skill)) - spent);
	
	// allow early if they're at a deadend
	if (avail == 0 && spent < max && GET_SKILL(ch, skill) >= EMPIRE_CHORE_SKILL_CAP && green_skill_deadend(ch, skill)) {
		return 1;
	}
	else {
		return avail;
	}
}


/**
* char_data *ch the person to check
* int skill Which SKILL_x to check
* return int total abilities bought in a skill
*/
int get_ability_points_spent(char_data *ch, int skill) {
	int count = 0, iter;
	
	for (iter = 0; iter < NUM_ABILITIES; ++iter) {
		if (ability_data[iter].parent_skill == skill && HAS_ABILITY(ch, iter)) {
			++count;
		}
	}
	
	return count;
}


/**
* @param char_data *ch The character whose skills to use, and who to send to
* @param int skill Which SKILL_x const
* @param int ability Which ability, for dependent abilities (NO_PREREQ for base abils)
* @param int indent How far to indent (goes up as the list goes deeper)
* @return string the display
*/
char *get_skill_abilities_display(char_data *ch, int skill, int ability, int indent) {
	extern int ability_sort[NUM_ABILITIES];

	char out[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH], colorize[16];
	static char retval[MAX_STRING_LENGTH];
	int iter, ind, abil, max_skill = 0;
	
	*out = '\0';
	
	for (iter = 0; iter < NUM_ABILITIES; ++iter) {
		abil = ability_sort[iter];
		if (ability_data[abil].parent_skill == skill && ability_data[abil].parent_ability == ability) {
			// indent
			for (ind = 0; ind < (2 * indent); ++ind) {
				lbuf[ind] = ' ';
			}
			lbuf[2 * indent] = '\0';
			
			if (ability_data[abil].parent_ability != NO_PREREQ) {
				strcat(lbuf, "+ ");
			}
			
			if (ability_data[abil].parent_skill_required < BASIC_SKILL_CAP) {
				max_skill = BASIC_SKILL_CAP;
			}
			else if (ability_data[abil].parent_skill_required < SPECIALTY_SKILL_CAP) {
				max_skill = SPECIALTY_SKILL_CAP;
			}
			else if (ability_data[abil].parent_skill_required < CLASS_SKILL_CAP) {
				max_skill = CLASS_SKILL_CAP;
			}
			
			// get the proper color for this ability
			strcpy(colorize, ability_color(ch, abil));
			
			sprintf(out + strlen(out), "%s(%s) %s%s&0 [%d-%d]", lbuf, (HAS_ABILITY(ch, abil) ? "x" : " "), colorize, ability_data[abil].name, ability_data[abil].parent_skill_required, max_skill);
			
			if (HAS_ABILITY(ch, abil)) {
				// this is kind of a hack to quickly make sure you can still use the ability
				if (GET_LEVELS_GAINED_FROM_ABILITY(ch, abil) < GAINS_PER_ABILITY && !strcmp(colorize, "&y")) {
					sprintf(out + strlen(out), " %d/%d skill points gained", GET_LEVELS_GAINED_FROM_ABILITY(ch, abil), GAINS_PER_ABILITY);
				}
				else {
					strcat(out, " (max)");
				}
			}
			else if (ability_data[abil].parent_skill != NOTHING) {
				// does not have the ability at all
				// we check that the parent skill exists first, since this reports it
				
				if (ability_data[abil].parent_ability != NO_ABIL && !HAS_ABILITY(ch, ability_data[abil].parent_ability)) {
					sprintf(out + strlen(out), " requires %s", ability_data[ability_data[abil].parent_ability].name);
				}
				else if (GET_SKILL(ch, ability_data[abil].parent_skill) < ability_data[abil].parent_skill_required) {
					sprintf(out + strlen(out), " requires %s %d", skill_data[ability_data[abil].parent_skill].name, ability_data[abil].parent_skill_required);
				}
				else {
					strcat(out, " unpurchased");
				}
			}
			
			strcat(out, "\r\n");

			// dependencies
			strcat(out, get_skill_abilities_display(ch, skill, abil, indent+1));
		}
	}
	
	strcpy(retval, out);
	
	return retval;
}


/**
* Gets the display text for how many points you have available.
*
* @param char_data *ch The player.
* @return char* The display text.
*/
char *get_skill_gain_display(char_data *ch) {
	static char out[MAX_STRING_LENGTH];
	
	*out = '\0';
	if (!IS_NPC(ch)) {
		sprintf(out + strlen(out), "You have %d bonus experience point%s available today. Use 'noskill <skill>' to toggle skill gain.\r\n", GET_DAILY_BONUS_EXPERIENCE(ch), PLURAL(GET_DAILY_BONUS_EXPERIENCE(ch)));
	}
	
	return out;
}


char *get_skill_row_display(char_data *ch, int skill) {
	static char out[MAX_STRING_LENGTH];
	char exp[256];
	int points = get_ability_points_available_for_char(ch, skill);
	
	if (!NOSKILL_BLOCKED(ch, skill) && !IS_ANY_SKILL_CAP(ch, skill)) {
		sprintf(exp, ", %.1f%% exp", GET_SKILL_EXP(ch, skill));
	}
	else {
		*exp = '\0';
	}
	
	sprintf(out, "[%3d] %s%s&0%s (%s%s) - %s\r\n", GET_SKILL(ch, skill), IS_ANY_SKILL_CAP(ch, skill) ? "&g" : "&y", skill_data[skill].name, (points > 0 ? "*" : ""), IS_ANY_SKILL_CAP(ch, skill) ? "&ymax&0" : (NOSKILL_BLOCKED(ch, skill) ? "&rnoskill&0" : "&cgaining&0"), exp, skill_data[skill].description);
	return out;
}


/**
* Determines if a player has hit a deadend with abilities they can level off
* of.
*
* @param char_data *ch The player.
* @param int skill Any SKILL_x
* @return bool TRUE if the player has found no levelable abilities
*/
bool green_skill_deadend(char_data *ch, int skill) {
	int avail = get_ability_points_available(skill, GET_SKILL(ch, skill)) - get_ability_points_spent(ch, skill);
	int iter;
	bool yellow = FALSE;
		
	if (avail <= 0) {
		for (iter = 0; iter < NUM_ABILITIES; ++iter) {
			if (ability_data[iter].parent_skill == skill) {
				yellow |= can_gain_skill_from(ch, iter);
			}
		}
	}
	
	// true if we found no yellows
	return !yellow;
}


/**
* When a character loses skill levels, we clear the "levels_gained" tracker
* for abilities that are >= their new level, so they can successfully regain
* those levels later.
*
* @param char_data *ch The player.
* @param int skill Which SKILL_x.
*/
void reset_skill_gain_tracker_on_abilities_above_level(char_data *ch, int skill) {
	int iter;
	
	if (!IS_NPC(ch)) {
		for (iter = 0; iter < NUM_ABILITIES; ++iter) {
			if (ability_data[iter].parent_skill == skill && ability_data[iter].parent_skill_required >= GET_SKILL(ch, skill)) {
				ch->player_specials->saved.abilities[iter].levels_gained = 0;
			}
		}
	}
}


// set a skill directly to a level
void set_skill(char_data *ch, int skill, int level) {
	bool gain = FALSE;
	
	if (!IS_NPC(ch)) {
		gain = (level > GET_SKILL(ch, skill));
		
		ch->player_specials->saved.skills[skill].level = level;
		ch->player_specials->saved.skills[skill].exp = 0;
		
		if (!gain) {
			reset_skill_gain_tracker_on_abilities_above_level(ch, skill);
		}
	}
}


/**
* @param char_data *ch who to check for
* @param int ability which ABIL_x
* @param int difficulty any DIFF_x const
* @return bool TRUE for success, FALSE for fail
*/
bool skill_check(char_data *ch, int ability, int difficulty) {
	extern double skill_check_difficulty_modifier[NUM_DIFF_TYPES];

	int chance = get_ability_level(ch, ability);

	// players without the ability have no chance	
	if (!IS_NPC(ch) && !HAS_ABILITY(ch, ability)) {
		chance = 0;
	}
	
	// modify chance based on diff
	chance = (int) (skill_check_difficulty_modifier[difficulty] * (double) chance);
	
	// roll
	return (number(1, 100) <= chance);
}


 //////////////////////////////////////////////////////////////////////////////
//// CORE SKILL COMMANDS /////////////////////////////////////////////////////

ACMD(do_noskill) {
	int skill;

	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		return;
	}
	else if (!*argument) {
		msg_to_char(ch, "Usage: noskill <skill>\r\n");
		msg_to_char(ch, "Type 'skills' to see which skills are gaining and which are not.\r\n");
	}
	else if ((skill = find_skill_by_name(argument)) == NO_SKILL) {
		msg_to_char(ch, "Unknown skill.\r\n");
	}
	else {
		if (NOSKILL_BLOCKED(ch, skill)) {
			NOSKILL_BLOCKED(ch, skill) = FALSE;
			msg_to_char(ch, "You will now &cbe able to gain&0 %s skill.\r\n", skill_data[skill].name);
		}
		else {
			NOSKILL_BLOCKED(ch, skill) = TRUE;
			msg_to_char(ch, "You will &rno longer&0 gain %s skill.\r\n", skill_data[skill].name);
		}
	}
}


ACMD(do_skills) {
	void check_skill_sell(char_data *ch, int abil);
	void clear_char_abilities(char_data *ch, int skill);
	extern int skill_sort[NUM_SKILLS];
	void resort_empires();
	
	char arg2[MAX_INPUT_LENGTH], lbuf[MAX_INPUT_LENGTH], outbuf[MAX_STRING_LENGTH];
	int iter, points, sk, ab, level;
	empire_data *emp;
	bool found;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "As an NPC, you have no skills.\r\n");
		return;
	}
	
	if (!*argument) {
		*outbuf = '\0';
		
		// no argument? list all
		sprintf(outbuf + strlen(outbuf), "You know the following skills (skill level %d):\r\n", GET_SKILL_LEVEL(ch));
		found = FALSE;
		for (iter = 0; iter < NUM_SKILLS; ++iter) {
			strcat(outbuf, get_skill_row_display(ch, skill_sort[iter]));
			if (!found && get_ability_points_available_for_char(ch, skill_sort[iter]) > 0) {
				found = TRUE;
			}
		}
		
		if (found) {
			strcat(outbuf, "* You have ability points available to spend.\r\n");
		}
		strcat(outbuf, get_skill_gain_display(ch));
		
		page_string(ch->desc, outbuf, 1);
	}
	else if (!strn_cmp(argument, "buy", 3)) {
		// purchase
		half_chop(argument, arg, lbuf);
		
		if (!*lbuf) {
			msg_to_char(ch, "Usage: skills buy <ability>\r\n");
			msg_to_char(ch, "You can see a list of abilities for each skill by typing 'skills <skill>'.\r\n");
			return;
		}
		
		ab = find_ability_by_name(lbuf, FALSE);
		if (ab == NO_ABIL) {
			msg_to_char(ch, "No such ability '%s'.\r\n", lbuf);
			return;
		}
		
		if (HAS_ABILITY(ch, ab)) {
			msg_to_char(ch, "You already know the %s ability.\r\n", ability_data[ab].name);
			return;
		}
		
		if (ability_data[ab].parent_skill == NOTHING) {
			msg_to_char(ch, "You cannot buy that ability.\r\n");
			return;
		}
		
		if (get_ability_points_available_for_char(ch, ability_data[ab].parent_skill) < 1 && !IS_IMMORTAL(ch)) {
			msg_to_char(ch, "You have no points available to spend in %s.\r\n", skill_data[ability_data[ab].parent_skill].name);
			return;
		}
		
		// check level
		if (GET_SKILL(ch, ability_data[ab].parent_skill) < ability_data[ab].parent_skill_required) {
			msg_to_char(ch, "You need at least %d in %s to buy %s.\r\n", ability_data[ab].parent_skill_required, skill_data[ability_data[ab].parent_skill].name, ability_data[ab].name);
			return;
		}
		
		// check pre-req
		if (ability_data[ab].parent_ability != NO_PREREQ && !HAS_ABILITY(ch, ability_data[ab].parent_ability)) {
			msg_to_char(ch, "You need to buy %s before you can buy %s.\r\n", ability_data[ability_data[ab].parent_ability].name, ability_data[ab].name);
			return;
		}
		
		// good to go
		
		emp = GET_LOYALTY(ch);
		
		// remove empire abilities temporarily
		if (emp) {
			adjust_abilities_to_empire(ch, emp, FALSE);
		}
		
		ch->player_specials->saved.abilities[ab].purchased = TRUE;
		msg_to_char(ch, "You purchase %s.\r\n", ability_data[ab].name);
		SAVE_CHAR(ch);
		
		// re-add empire abilities
		if (emp) {
			adjust_abilities_to_empire(ch, emp, TRUE);
			resort_empires();
		}
	}
	else if (!strn_cmp(argument, "reset", 5)) {
		// self-clear!
		half_chop(argument, arg, lbuf);
		
		if (!*lbuf) {
			msg_to_char(ch, "Usage: skills reset <skill>\r\n");
			msg_to_char(ch, "You have free resets available in: ");
			
			found = FALSE;
			for (iter = 0; iter < NUM_SKILLS; ++iter) {
				if (IS_IMMORTAL(ch) || GET_FREE_SKILL_RESETS(ch, iter) > 0) {
					msg_to_char(ch, "%s%s", (found ? ", " : ""), skill_data[iter].name);
					if (GET_FREE_SKILL_RESETS(ch, iter) > 1) {
						msg_to_char(ch, " (%d)", GET_FREE_SKILL_RESETS(ch, iter));
					}
					found = TRUE;
				}
			}
			
			if (found) {
				msg_to_char(ch, "\r\n");
			}
			else {
				msg_to_char(ch, "none\r\n");
			}
		}
		else if ((sk = find_skill_by_name(lbuf)) == NO_SKILL) {
			msg_to_char(ch, "No such skill %s.\r\n", lbuf);
		}
		else if (!IS_IMMORTAL(ch) && GET_FREE_SKILL_RESETS(ch, sk) == 0) {
			msg_to_char(ch, "You do not have a free reset available for %s.\r\n", skill_data[sk].name);
		}
		else {
			GET_FREE_SKILL_RESETS(ch, sk) -= 1;
			clear_char_abilities(ch, sk);
			
			msg_to_char(ch, "You have reset your %s abilities.\r\n", skill_data[sk].name);
			SAVE_CHAR(ch);
		}
		
		// end "reset"
		return;
	}
	else if (!strn_cmp(argument, "drop", 4)) {
		half_chop(argument, arg, lbuf);	// "drop" "skill level"
		half_chop(lbuf, arg, arg2);	// "skill" "level"
		
		if (!*arg || !*arg2) {
			msg_to_char(ch, "Warning: This command reduces your level in a skill.\r\n");
			msg_to_char(ch, "Usage: skill drop <skill> <0/%d/%d>\r\n", BASIC_SKILL_CAP, SPECIALTY_SKILL_CAP);
		}
		else if ((sk = find_skill_by_name(arg)) == NOTHING) {
			msg_to_char(ch, "Unknown skill '%s'.\r\n", arg);
		}
		else if (!is_number(arg2)) {
			msg_to_char(ch, "Invalid level.\r\n");
		}
		else if ((level = atoi(arg2)) >= GET_SKILL(ch, sk)) {
			msg_to_char(ch, "You can only drop skills to lower levels.\r\n");
		}
		else if (level != 0 && level != BASIC_SKILL_CAP && level != SPECIALTY_SKILL_CAP) {
			msg_to_char(ch, "You can only drop skills to the following levels: 0, %d, %d\r\n", BASIC_SKILL_CAP, SPECIALTY_SKILL_CAP);
		}
		else {
			// good to go!
			msg_to_char(ch, "You have dropped your %s skill to %d and reset its abilities.\r\n", skill_data[sk].name, level);
			set_skill(ch, sk, level);
			update_class(ch);
			clear_char_abilities(ch, sk);
			
			SAVE_CHAR(ch);
		}
	}
	else if (IS_IMMORTAL(ch) && !strn_cmp(argument, "sell", 4)) {
		// purchase
		half_chop(argument, arg, lbuf);
		
		if (!*lbuf) {
			msg_to_char(ch, "Usage: skills sell <ability>\r\n");
			msg_to_char(ch, "You can see a list of abilities for each skill by typing 'skills <skill>'.\r\n");
			return;
		}
		
		ab = find_ability_by_name(lbuf, FALSE);
		if (ab == NO_ABIL) {
			msg_to_char(ch, "No such ability '%s'.\r\n", lbuf);
			return;
		}
		
		if (!HAS_ABILITY(ch, ab)) {
			msg_to_char(ch, "You do not know the %s ability.\r\n", ability_data[ab].name);
			return;
		}
		
		// good to go
		ch->player_specials->saved.abilities[ab].purchased = FALSE;
		msg_to_char(ch, "You no longer know %s.\r\n", ability_data[ab].name);
		SAVE_CHAR(ch);

		if (GET_LOYALTY(ch)) {
			adjust_abilities_to_empire(ch, GET_LOYALTY(ch), FALSE);
		}

		check_skill_sell(ch, ab);
		
		if (GET_LOYALTY(ch)) {
			adjust_abilities_to_empire(ch, GET_LOYALTY(ch), TRUE);
			resort_empires();
		}
	}
	else {
		*outbuf = '\0';
		
		// argument: show abilities for 1 skill
		sk = find_skill_by_name(argument);
		if (sk != NO_SKILL) {
			// header
			strcat(outbuf, get_skill_row_display(ch, sk));
			
			points = get_ability_points_available_for_char(ch, sk);
			if (points > 0) {
				sprintf(outbuf + strlen(outbuf), "You have %d ability point%s to spend. Type 'skill buy <ability>' to purchase a new ability.\r\n", points, (points != 1 ? "s" : ""));
			}
			
			// list
			strcat(outbuf, get_skill_abilities_display(ch, sk, NO_PREREQ, 1));
			
			page_string(ch->desc, outbuf, 1);
		}
		else {
			msg_to_char(ch, "No such skill.\r\n");
		}
	}
}


// this is a temporary command for picking skill specs, and will ultimately be replaced by quests or other mechanisms
ACMD(do_specialize) {
	int iter, count, sk, specialty_allowed;
	
	skip_spaces(&argument);
	
	// make certain these are up-to-datec
	specialty_allowed = NUM_SPECIALTY_SKILLS_ALLOWED + (CAN_GET_BONUS_SKILLS(ch) ? BONUS_SPECIALTY_SKILLS_ALLOWED : 0);
	
	if (!*argument) {
		msg_to_char(ch, "Specialize in which skill?\r\n");
	}
	else if ((sk = find_skill_by_name(argument)) == NO_SKILL) {
		msg_to_char(ch, "No such skill.\r\n");
	}
	else if (GET_SKILL(ch, sk) != BASIC_SKILL_CAP && GET_SKILL(ch, sk) != SPECIALTY_SKILL_CAP) {
		msg_to_char(ch, "You can only specialize skills which are at %d or %d.\r\n", BASIC_SKILL_CAP, SPECIALTY_SKILL_CAP);
	}
	else {
		// check > basic
		if (GET_SKILL(ch, sk) == BASIC_SKILL_CAP) {
			for (iter = count = 0; iter < NUM_SKILLS; ++iter) {
				if (GET_SKILL(ch, iter) > BASIC_SKILL_CAP) {
					++count;
				}
			}
			if ((count + 1) > specialty_allowed) {
				msg_to_char(ch, "You can only have at most %d skill%s above level %d.\r\n", specialty_allowed, (specialty_allowed != 1 ? "s" : ""), BASIC_SKILL_CAP);
				return;
			}
		}
		
		// check > specialty
		if (GET_SKILL(ch, sk) == SPECIALTY_SKILL_CAP) {
			for (iter = count = 0; iter < NUM_SKILLS; ++iter) {
				if (GET_SKILL(ch, iter) > SPECIALTY_SKILL_CAP) {
					++count;
				}
			}
			if ((count + 1) > SKILLS_PER_CLASS) {
				msg_to_char(ch, "You can only have %d skill%s above level %d.\r\n", SKILLS_PER_CLASS, (SKILLS_PER_CLASS != 1 ? "s" : ""), SPECIALTY_SKILL_CAP);
				return;
			}
		}

		// le done
		set_skill(ch, sk, GET_SKILL(ch, sk) + 1);
		msg_to_char(ch, "You have specialized in %s.\r\n", skill_data[sk].name);

		// check class and skill levels
		update_class(ch);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// SKILL HELPERS ///////////////////////////////////////////////////////////

/**
* Determines if the character has the required skill and level to wear an item
* (armor, shield, weapons).
*
* @param char_data *ch The person trying to wear it.
* @param obj_data *item The thing he's trying to wear.
* @param bool send_messages If TRUE, sends its own errors.
* @return bool TRUE if ch can use the item, or FALSE.
*/
bool can_wear_item(char_data *ch, obj_data *item, bool send_messages) {
	char buf[MAX_STRING_LENGTH];
	int abil = NO_ABIL;
	bool level_ok = TRUE;
	int iter;
	
	// players won't be able to use gear >= these levels if their skill level is < the level
	int level_ranges[] = { CLASS_SKILL_CAP, SPECIALTY_SKILL_CAP, BASIC_SKILL_CAP, -1 };	// terminate with -1
	
	if (IS_NPC(ch)) {
		return TRUE;
	}
	
	if (IS_ARMOR(item)) {
		switch (GET_ARMOR_TYPE(item)) {
			case ARMOR_CLOTH: {
				abil = ABIL_CLOTH_ARMOR;
				break;
			}
			case ARMOR_LEATHER: {
				abil = ABIL_LEATHER_ARMOR;
				break;
			}
			case ARMOR_MEDIUM: {
				abil = ABIL_MEDIUM_ARMOR;
				break;
			}
			case ARMOR_HEAVY: {
				abil = ABIL_HEAVY_ARMOR;
				break;
			}
		}
	}
	else if (OBJ_FLAGGED(item, OBJ_TWO_HANDED)) {
		abil = ABIL_TWO_HANDED_WEAPONS;
	}
	else if (IS_MISSILE_WEAPON(item)) {
		abil = ABIL_ARCHERY;
	}
	else if (IS_SHIELD(item)) {
		abil = ABIL_SHIELD_BLOCK;
	}
	
	if (abil != NO_ABIL && !HAS_ABILITY(ch, abil)) {
		if (send_messages) {
			snprintf(buf, sizeof(buf), "You must purchase the %s ability to use $p.", ability_data[abil].name);
			act(buf, FALSE, ch, item, NULL, TO_CHAR);
		}
		return FALSE;
	}
	
	// check skill levels?
	if (GET_SKILL_LEVEL(ch) < CLASS_SKILL_CAP && GET_OBJ_CURRENT_SCALE_LEVEL(item) > 0) {
		for (iter = 0; level_ranges[iter] != -1 && level_ok; ++iter) {
			if (GET_OBJ_CURRENT_SCALE_LEVEL(item) > level_ranges[iter] && GET_SKILL_LEVEL(ch) < level_ranges[iter]) {
				if (send_messages) {
					snprintf(buf, sizeof(buf), "You need to be skill level %d to use $p.", level_ranges[iter]);
					act(buf, FALSE, ch, item, NULL, TO_CHAR);
				}
				return FALSE;
			}
		}
	}
	
	return TRUE;
}


/**
* @param char_data *ch the user
* @return obj_data *a valid chipper, or NULL if the user has none
*/
obj_data *find_chip_weapon(char_data *ch) {
	obj_data *weapon;

	// find valid weapon
	weapon = GET_EQ(ch, WEAR_WIELD);
	if (!weapon || (GET_WEAPON_TYPE(weapon) != TYPE_HAMMER && GET_OBJ_VNUM(weapon) != o_ROCK)) {
		weapon = GET_EQ(ch, WEAR_HOLD);
		if (!weapon || (GET_WEAPON_TYPE(weapon) != TYPE_HAMMER && GET_OBJ_VNUM(weapon) != o_ROCK)) {
			weapon = NULL;
		}
	}
	
	return weapon;
}


/**
* @param char_data *ch
* @return bool TRUE if the character is somewhere he can cook, else FALSE
*/
bool has_cooking_fire(char_data *ch) {
	obj_data *obj;
	
	if (!IS_NPC(ch) && HAS_ABILITY(ch, ABIL_TOUCH_OF_FLAME)) {
		return TRUE;
	}

	if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_COOKING_FIRE)) {	
		return TRUE;
	}
	
	for (obj = ROOM_CONTENTS(IN_ROOM(ch)); obj; obj = obj->next_content) {
		if (OBJ_FLAGGED(obj, OBJ_LIGHT) && !CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* Checks to see if ch has a sharp tool equipped, and returns it if so
*
* char_data *ch the person to check
* return obj_data *the sharp tool or NULL
*/
obj_data *has_sharp_tool(char_data *ch) {
	obj_data *obj;
	
	// wield
	obj = GET_EQ(ch, WEAR_WIELD);
	if (obj && IS_WEAPON(obj) && attack_hit_info[GET_WEAPON_TYPE(obj)].weapon_type == WEAPON_SHARP) {
		return obj;
	}
	
	// hold
	obj = GET_EQ(ch, WEAR_HOLD);
	if (obj && IS_WEAPON(obj) && attack_hit_info[GET_WEAPON_TYPE(obj)].weapon_type == WEAPON_SHARP) {
		return obj;
	}
	
	return NULL;
}


/* tie/untie an npc */
void perform_npc_tie(char_data *ch, char_data *victim, int subcmd) {
	obj_data *rope;

	if (!subcmd && MOB_FLAGGED(victim, MOB_TIED))
		act("$E's already tied up!", FALSE, ch, 0, victim, TO_CHAR);
	else if (subcmd && !MOB_FLAGGED(victim, MOB_TIED))
		act("$E isn't even tied up!", FALSE, ch, 0, victim, TO_CHAR);
	else if (MOB_FLAGGED(victim, MOB_TIED)) {
		act("You untie $N.", FALSE, ch, 0, victim, TO_CHAR);
		act("$n unties you!", FALSE, ch, 0, victim, TO_VICT | TO_SLEEP);
		act("$n unties $N.", FALSE, ch, 0, victim, TO_NOTVICT);
		REMOVE_BIT(MOB_FLAGS(victim), MOB_TIED);
		obj_to_char((rope = read_object(o_ROPE)), ch);
		load_otrigger(rope);
	}
	else if (!MOB_FLAGGED(victim, MOB_ANIMAL)) {
		msg_to_char(ch, "You can only tie animals.\r\n");
	}
	else if (!(rope = get_obj_in_list_num(o_ROPE, ch->carrying)))
		msg_to_char(ch, "You don't have any rope.\r\n");
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN))
		msg_to_char(ch, "You can't tie it here.\r\n");
	else {
		act("You tie $N up.", FALSE, ch, 0, victim, TO_CHAR);
		act("$n ties you up.", FALSE, ch, 0, victim, TO_VICT | TO_SLEEP);
		act("$n ties $N up.", FALSE, ch, 0, victim, TO_NOTVICT);
		SET_BIT(MOB_FLAGS(victim), MOB_TIED);
		extract_obj(rope);
	}
}
