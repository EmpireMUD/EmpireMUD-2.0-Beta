/* ************************************************************************
*   File: skills.c                                        EmpireMUD 2.0b5 *
*  Usage: code related to skills, including DB and OLC                    *
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
#include "olc.h"

/**
* Contents:
*   End Affect When Skill Lost -- core code for shutting off effects
*   Core Skill Functions
*   Core Skill Commands
*   Helpers
*   Utilities
*   Database
*   OLC Handlers
*   Skill Ability Display
*   Main Displays
*   Edit Modules
*/

// local data
const char *default_skill_name = "Unnamed Skill";
const char *default_skill_abbrev = "???";
const char *default_skill_desc = "New skill";

// external consts
extern const char *skill_flags[];

// eternal functions
void resort_empires(bool force);
extern bool is_class_ability(ability_data *abil);
void update_class(char_data *ch);

// local protos
bool can_gain_skill_from(char_data *ch, ability_data *abil);
void clear_char_abilities(char_data *ch, any_vnum skill);
struct skill_ability *find_skill_ability(skill_data *skill, ability_data *abil);
int get_ability_points_available(any_vnum skill, int level);
int get_ability_points_spent(char_data *ch, any_vnum skill);
bool green_skill_deadend(char_data *ch, any_vnum skill);
void remove_ability_by_set(char_data *ch, ability_data *abil, int skill_set, bool reset_levels);
int sort_skill_abilities(struct skill_ability *a, struct skill_ability *b);


 //////////////////////////////////////////////////////////////////////////////
//// END AFFECT WHEN SKILL LOST //////////////////////////////////////////////

/**
* Code that must run when skills are sold.
*
* @param char_data *ch
* @param ability_data *abil The ability to sell
*/
void check_skill_sell(char_data *ch, ability_data *abil) {
	bool despawn_familiar(char_data *ch, mob_vnum vnum);
	void end_majesty(char_data *ch);
	void finish_morphing(char_data *ch, morph_data *morph);
	void remove_armor_by_type(char_data *ch, int armor_type);
	void remove_honed_gear(char_data *ch);
	void retract_claws(char_data *ch);
	void undisguise(char_data *ch);	
	
	obj_data *obj;
	bool found = TRUE;	// inverted detection, see default below
	
	// empire_data *emp = GET_LOYALTY(ch);
	
	// un-morph
	if (IS_MORPHED(ch) && MORPH_ABILITY(GET_MORPH(ch)) == ABIL_VNUM(abil)) {
		finish_morphing(ch, NULL);
	}
	
	switch (ABIL_VNUM(abil)) {
		case ABIL_ALACRITY: {
			void end_alacrity(char_data *ch);
			end_alacrity(ch);
			break;
		}
		case ABIL_ARCHERY: {
			if (GET_EQ(ch, WEAR_RANGED) && IS_MISSILE_WEAPON(GET_EQ(ch, WEAR_RANGED))) {
				act("You stop using $p.", FALSE, ch, GET_EQ(ch, WEAR_RANGED), NULL, TO_CHAR);
				unequip_char_to_inventory(ch, WEAR_RANGED);
				determine_gear_level(ch);
			}
			break;
		}
		case ABIL_BANSHEE: {
			despawn_familiar(ch, FAMILIAR_BANSHEE);
			break;
		}
		case ABIL_BASILISK: {
			despawn_familiar(ch, FAMILIAR_BASILISK);
			break;
		}
		case ABIL_READY_BLOOD_WEAPONS: {
			if ((obj = GET_EQ(ch, WEAR_WIELD)) && IS_BLOOD_WEAPON(obj)) {
				act("You stop using $p.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), NULL, TO_CHAR);
				unequip_char_to_inventory(ch, WEAR_WIELD);
				determine_gear_level(ch);
			}
			break;
		}
		case ABIL_SUMMON_BODYGUARD: {
			despawn_familiar(ch, BODYGUARD);
			break;
		}
		case ABIL_BOOST: {
			affect_from_char(ch, ATYPE_BOOST, TRUE);
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
		case ABIL_CLAWS: {
			retract_claws(ch);
			break;
		}
		case ABIL_MAGE_ARMOR: {
			remove_armor_by_type(ch, ARMOR_MAGE);
			break;
		}
		case ABIL_COUNTERSPELL: {
			affect_from_char(ch, ATYPE_COUNTERSPELL, TRUE);
			break;
		}
		case ABIL_DEATHSHROUD: {
			if (affected_by_spell(ch, ATYPE_DEATHSHROUD)) {
				void un_deathshroud(char_data *ch);
				un_deathshroud(ch);
			}
			break;
		}
		case ABIL_DIRE_WOLF: {
			despawn_familiar(ch, FAMILIAR_DIRE_WOLF);
			break;
		}
		case ABIL_DISGUISE: {
			if (IS_DISGUISED(ch)) {
				undisguise(ch);
			}
			break;
		}
		case ABIL_EARTHARMOR: {
			affect_from_char_by_caster(ch, ATYPE_EARTHARMOR, ch, TRUE);
			break;
		}
		case ABIL_EARTHMELD: {
			if (affected_by_spell(ch, ATYPE_EARTHMELD)) {
				void un_earthmeld(char_data *ch);
				un_earthmeld(ch);
			}
			break;
		}
		case ABIL_FAMILIAR: {
			despawn_familiar(ch, FAMILIAR_CAT);
			despawn_familiar(ch, FAMILIAR_SABERTOOTH);
			despawn_familiar(ch, FAMILIAR_SPHINX);
			despawn_familiar(ch, FAMILIAR_GIANT_TORTOISE);
			break;
		}
		case ABIL_FISH: {
			if (GET_ACTION(ch) == ACT_FISHING) {
				cancel_action(ch);
			}
			break;
		}
		case ABIL_FLY: {
			affect_from_char(ch, ATYPE_FLY, TRUE);
			break;
		}
		case ABIL_FORESIGHT: {
			affect_from_char_by_caster(ch, ATYPE_FORESIGHT, ch, TRUE);
			break;
		}
		case ABIL_GRIFFIN: {
			despawn_familiar(ch, FAMILIAR_GRIFFIN);
			break;
		}
		case ABIL_HASTEN: {
			affect_from_char_by_caster(ch, ATYPE_HASTEN, ch, TRUE);
			break;
		}
		case ABIL_HEAVY_ARMOR: {
			remove_armor_by_type(ch, ARMOR_HEAVY);
			break;
		}
		case ABIL_HONE: {
			remove_honed_gear(ch);
			break;
		}
		case ABIL_LIGHT_ARMOR: {
			remove_armor_by_type(ch, ARMOR_LIGHT);
			break;
		}
		case ABIL_MAJESTY: {
			end_majesty(ch);
			break;
		}
		case ABIL_MANASHIELD: {
			affect_from_char(ch, ATYPE_MANASHIELD, TRUE);
			break;
		}
		case ABIL_MANTICORE: {
			despawn_familiar(ch, FAMILIAR_MANTICORE);
			break;
		}
		case ABIL_MEDIUM_ARMOR: {
			remove_armor_by_type(ch, ARMOR_MEDIUM);
			break;
		}
		case ABIL_MIRRORIMAGE: {
			despawn_familiar(ch, MIRROR_IMAGE_MOB);
			break;
		}
		case ABIL_MOON_RABBIT: {
			despawn_familiar(ch, FAMILIAR_MOON_RABBIT);
			break;
		}
		case ABIL_MUMMIFY: {
			if (affected_by_spell(ch, ATYPE_MUMMIFY)) {
				void un_mummify(char_data *ch);
				un_mummify(ch);
			}
			break;
		}
		case ABIL_NAVIGATION: {
			GET_CONFUSED_DIR(ch) = NORTH;
			break;
		}
		case ABIL_NIGHTSIGHT: {
			if (affected_by_spell(ch, ATYPE_NIGHTSIGHT)) {
				msg_to_char(ch, "You end your nightsight.\r\n");
				act("The glow in $n's eyes fades.", TRUE, ch, NULL, NULL, TO_ROOM);
				affect_from_char(ch, ATYPE_NIGHTSIGHT, TRUE);
			}
			break;
		}
		case ABIL_OWL_SHADOW: {
			despawn_familiar(ch, FAMILIAR_OWL_SHADOW);
			break;
		}
		case ABIL_PHOENIX: {
			despawn_familiar(ch, FAMILIAR_PHOENIX);
			break;
		}
		case ABIL_PHOENIX_RITE: {
			affect_from_char(ch, ATYPE_PHOENIX_RITE, TRUE);
			break;
		}
		case ABIL_RADIANCE: {
			affect_from_char(ch, ATYPE_RADIANCE, TRUE);
			break;
		}
		case ABIL_READY_FIREBALL: {
			if ((obj = GET_EQ(ch, WEAR_WIELD))) {
				if (GET_OBJ_VNUM(obj) == o_FIREBALL) {
					act("You stop using $p.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), NULL, TO_CHAR);
					unequip_char_to_inventory(ch, WEAR_WIELD);
					determine_gear_level(ch);
				}
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
		case ABIL_RITUAL_OF_BURDENS: {
			if (affected_by_spell(ch, ATYPE_UNBURDENED)) {
				msg_to_char(ch, "Your burdens return.\r\n");
				affect_from_char(ch, ATYPE_UNBURDENED, TRUE);
			}
			break;
		}
		case ABIL_SALAMANDER: {
			despawn_familiar(ch, FAMILIAR_SALAMANDER);
			break;
		}
		case ABIL_SCORPION_SHADOW: {
			despawn_familiar(ch, FAMILIAR_SCORPION_SHADOW);
			break;
		}
		case ABIL_SHIELD_BLOCK: {
			if ((obj = GET_EQ(ch, WEAR_HOLD)) && IS_SHIELD(obj)) {
				act("You stop using $p.", FALSE, ch, GET_EQ(ch, WEAR_HOLD), NULL, TO_CHAR);
				unequip_char_to_inventory(ch, WEAR_HOLD);
				determine_gear_level(ch);
			}
			break;
		}
		case ABIL_SIPHON: {
			affect_from_char(ch, ATYPE_SIPHON, TRUE);
			break;
		}
		case ABIL_SKELETAL_HULK: {
			despawn_familiar(ch, FAMILIAR_SKELETAL_HULK);
			break;
		}
		case ABIL_SPIRIT_WOLF: {
			despawn_familiar(ch, FAMILIAR_SPIRIT_WOLF);
			break;
		}
		case ABIL_SOULMASK: {
			affect_from_char(ch, ATYPE_SOULMASK, TRUE);
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
* @param char_data *ch The player with the ability
* @param ability_data *abil The ability to color;
*/
char *ability_color(char_data *ch, ability_data *abil) {
	bool can_buy, has_bought, has_maxed;
	struct skill_ability *skab;
	
	has_bought = has_ability(ch, ABIL_VNUM(abil));
	can_buy = (ABIL_ASSIGNED_SKILL(abil) && get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) >= ABIL_SKILL_LEVEL(abil)) && (skab = find_skill_ability(ABIL_ASSIGNED_SKILL(abil), abil)) && (skab->prerequisite == NO_ABIL || has_ability(ch, skab->prerequisite));
	has_maxed = has_bought && (levels_gained_from_ability(ch, abil) >= GAINS_PER_ABILITY || (!ABIL_ASSIGNED_SKILL(abil) || IS_ANY_SKILL_CAP(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) || !can_gain_skill_from(ch, abil)));
	
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
* Gives an ability to a player on any of their skill sets.
*
* @param char_data *ch The player to check.
* @param ability_data *abil Any valid ability.
* @param int skill_set Which skill set number (0..NUM_SKILL_SETS-1).
* @param bool reset_levels If TRUE, wipes out the number of levels gained from the ability.
*/
void add_ability_by_set(char_data *ch, ability_data *abil, int skill_set, bool reset_levels) {
	struct player_ability_data *data = get_ability_data(ch, ABIL_VNUM(abil), TRUE);
	
	if (skill_set < 0 || skill_set >= NUM_SKILL_SETS) {
		log("SYSERR: Attempting to give ability '%s' to player '%s' on invalid skill set '%d'", GET_NAME(ch), ABIL_NAME(abil), skill_set);
		return;
	}
	
	if (data) {
		data->purchased[skill_set] = TRUE;
		if (reset_levels) {
			data->levels_gained = 0;
		}
		qt_change_ability(ch, ABIL_VNUM(abil));
	}
}


/**
* Gives an ability to a player on their current skill set.
*
* @param char_data *ch The player to check.
* @param ability_data *abil Any valid ability.
* @param bool reset_levels If TRUE, wipes out the number of levels gained from the ability.
*/
void add_ability(char_data *ch, ability_data *abil, bool reset_levels) {
	add_ability_by_set(ch, abil, GET_CURRENT_SKILL_SET(ch), reset_levels);
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
	
	if (has_ability(ch, ABIL_EXARCH_CRAFTS)) {
		EMPIRE_TECH(emp, TECH_EXARCH_CRAFTS) += mod;
	}
	if (has_ability(ch, ABIL_WORKFORCE)) {
		EMPIRE_TECH(emp, TECH_WORKFORCE) += mod;
	}
	if (has_ability(ch, ABIL_SKILLED_LABOR)) {
		EMPIRE_TECH(emp, TECH_SKILLED_LABOR) += mod;
	}
	if (has_ability(ch, ABIL_TRADE_ROUTES)) {
		EMPIRE_TECH(emp, TECH_TRADE_ROUTES) += mod;
	}
	if (has_ability(ch, ABIL_LOCKS)) {
		EMPIRE_TECH(emp, TECH_LOCKS) += mod;
	}
	if (has_ability(ch, ABIL_PROMINENCE)) {
		EMPIRE_TECH(emp, TECH_PROMINENCE) += mod;
	}
	if (has_ability(ch, ABIL_COMMERCE)) {
		EMPIRE_TECH(emp, TECH_COMMERCE) += mod;
	}
	if (has_ability(ch, ABIL_CITY_LIGHTS)) {
		EMPIRE_TECH(emp, TECH_CITY_LIGHTS) += mod;
	}
	if (has_ability(ch, ABIL_PORTAL_MAGIC)) {
		EMPIRE_TECH(emp, TECH_PORTALS) += mod;
	}
	if (has_ability(ch, ABIL_PORTAL_MASTER)) {
		EMPIRE_TECH(emp, TECH_MASTER_PORTALS) += mod;
	}
}


/** 
* @param char_data *ch The player
* @param ability_data *abil The ability
* @return TRUE if ch can still gain skill from ability
*/
bool can_gain_skill_from(char_data *ch, ability_data *abil) {
	// must have the ability and not gained too many from it
	if (ABIL_ASSIGNED_SKILL(abil) && get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) < SKILL_MAX_LEVEL(ABIL_ASSIGNED_SKILL(abil)) && has_ability(ch, ABIL_VNUM(abil)) && levels_gained_from_ability(ch, abil) < GAINS_PER_ABILITY) {
		// these limit abilities purchased under each cap to players who are still under that cap
		if (ABIL_SKILL_LEVEL(abil) >= BASIC_SKILL_CAP || get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) < BASIC_SKILL_CAP) {
			if (ABIL_SKILL_LEVEL(abil) >= SPECIALTY_SKILL_CAP || get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) < SPECIALTY_SKILL_CAP) {
				if (ABIL_SKILL_LEVEL(abil) >= CLASS_SKILL_CAP || get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) < CLASS_SKILL_CAP) {
					return TRUE;
				}
			}
		}
	}
	
	return FALSE;
}


/**
* General test function for abilities. It allows mobs who are not charmed to
* use any ability. Otherwise, it supports an energy pool cost and/or cooldowns,
* as well as checking that the player has the ability.
*
* @param char_data *ch The player or NPC.
* @param any_vnum ability Any ABIL_ const or vnum.
* @param int cost_pool HEALTH, MANA, MOVE, BLOOD (NOTHING if no charge).
* @param int cost_amount Mana (or whatever) amount required, if any.
* @param int cooldown_type Any COOLDOWN_ const, or NOTHING for no cooldown check.
* @return bool TRUE if ch can use ability; FALSE if not.
*/
bool can_use_ability(char_data *ch, any_vnum ability, int cost_pool, int cost_amount, int cooldown_type) {
	extern const char *pool_types[];
	
	char buf[MAX_STRING_LENGTH];
	int time, needs_cost;
	
	// purchase check first, or the rest don't make sense.
	if (!IS_NPC(ch) && ability != NO_ABIL && !has_ability(ch, ability)) {
		msg_to_char(ch, "You have not purchased the %s ability.\r\n", get_ability_name_by_vnum(ability));
		return FALSE;
	}
	
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
	
	// more player checks
	if (cost_pool >= 0 && cost_pool < NUM_POOLS && cost_amount > 0 && GET_CURRENT_POOL(ch, cost_pool) < needs_cost) {
		msg_to_char(ch, "You need %d %s point%s to do that.\r\n", cost_amount, pool_types[cost_pool], PLURAL(cost_amount));
		return FALSE;
	}
	if (cooldown_type > COOLDOWN_RESERVED && (time = get_cooldown_time(ch, cooldown_type)) > 0) {
		snprintf(buf, sizeof(buf), "%s is still on cooldown for %d second%s.\r\n", get_ability_name_by_vnum(ability), time, (time != 1 ? "s" : ""));
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
* @param char_data *ch The player or NPC.
* @param int cost_pool HEALTH, MANA, MOVE, BLOOD (NOTHING if no charge).
* @param int cost_amount Mana (or whatever) amount required, if any.
* @param int cooldown_type Any COOLDOWN_ const to apply (NOTHING for none).
* @param int cooldown_time Cooldown duration, if any.
* @param int wait_type Any WAIT_x const or WAIT_NONE for no command lag.
*/
void charge_ability_cost(char_data *ch, int cost_pool, int cost_amount, int cooldown_type, int cooldown_time, int wait_type) {
	if (cost_pool >= 0 && cost_pool < NUM_POOLS && cost_amount > 0) {
		GET_CURRENT_POOL(ch, cost_pool) = MAX(0, GET_CURRENT_POOL(ch, cost_pool) - cost_amount);
	}
	
	// only npcs get cooldowns here
	if (cooldown_type > COOLDOWN_RESERVED && cooldown_time > 0 && !IS_NPC(ch)) {
		add_cooldown(ch, cooldown_type, cooldown_time);
	}
	
	command_lag(ch, wait_type);
}


/**
* Checks one skill for a player, and removes and abilities that are above the
* players range. If the player is overspent on points after that, the whole
* ability is cleared.
*
* @param char_data *ch the player
* @param any_vnum skill which skill, or NO_SKILL for all
*/
void check_ability_levels(char_data *ch, any_vnum skill) {
	struct player_ability_data *abil, *next_abil;
	empire_data *emp = GET_LOYALTY(ch);
	bool all = (skill == NO_SKILL);
	ability_data *abd;
	int iter;
	
	if (IS_NPC(ch)) {	// somehow
		return;
	}
	
	// remove ability techs -- only if playing
	if (emp && ch->desc && STATE(ch->desc) == CON_PLAYING) {
		adjust_abilities_to_empire(ch, emp, FALSE);
	}
	
	HASH_ITER(hh, GET_ABILITY_HASH(ch), abil, next_abil) {
		abd = abil->ptr;
		
		// is assigned to some skill
		if (!ABIL_ASSIGNED_SKILL(abd)) {
			continue;
		}
		// matches requested skill
		if (!all && SKILL_VNUM(ABIL_ASSIGNED_SKILL(abd)) != skill) {
			continue;
		}
		
		if (get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abd))) < ABIL_SKILL_LEVEL(abd)) {
			// whoops too low
			for (iter = 0; iter < NUM_SKILL_SETS; ++iter) {
				if (abil->purchased[iter]) {
					if (iter == GET_CURRENT_SKILL_SET(ch)) {
						check_skill_sell(REAL_CHAR(ch), abil->ptr);
					}
					remove_ability_by_set(ch, abil->ptr, iter, FALSE);
				}
			}
		}
	}
	
	// check if they have too many points spent now (e.g. got early points)
	if (get_ability_points_available(skill, get_skill_level(ch, skill)) < get_ability_points_spent(ch, skill)) {
		clear_char_abilities(ch, skill);
	}
	
	SAVE_CHAR(ch);
	
	// add ability techs -- only if playing
	if (emp && ch->desc && STATE(ch->desc) == CON_PLAYING) {
		adjust_abilities_to_empire(ch, emp, TRUE);
		resort_empires(FALSE);
	}
}


/**
* removes all abilities for a player in a given skill on their CURRENT skill set
*
* @param char_data *ch the player
* @param any_vnum skill which skill, or NO_SKILL for all
*/
void clear_char_abilities(char_data *ch, any_vnum skill) {
	struct player_ability_data *abil, *next_abil;
	empire_data *emp = GET_LOYALTY(ch);
	bool all = (skill == NO_SKILL);
	ability_data *abd;
	
	if (!IS_NPC(ch)) {
		// remove ability techs -- only if playing
		if (emp && ch->desc && STATE(ch->desc) == CON_PLAYING) {
			adjust_abilities_to_empire(ch, emp, FALSE);
		}
		
		HASH_ITER(hh, GET_ABILITY_HASH(ch), abil, next_abil) {
			abd = abil->ptr;
			if (all || (ABIL_ASSIGNED_SKILL(abd) && SKILL_VNUM(ABIL_ASSIGNED_SKILL(abd)) == skill)) {
				if (abil->purchased[GET_CURRENT_SKILL_SET(ch)] && ABIL_SKILL_LEVEL(abd) > 0) {
					check_skill_sell(REAL_CHAR(ch), abil->ptr);
					remove_ability(ch, abil->ptr, FALSE);
				}
			}
		}
		SAVE_CHAR(ch);
		
		// add ability techs -- only if playing
		if (emp && ch->desc && STATE(ch->desc) == CON_PLAYING) {
			adjust_abilities_to_empire(ch, emp, TRUE);
			resort_empires(FALSE);
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
void empire_skillup(empire_data *emp, any_vnum ability, double amount) {
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
* This is the main interface for ability-based skill learning. Ability gain
* caps are checked here. The amount to gain is automatically reduced if the
* player has no daily points available.
*
* @param char_data *ch The player character who will gain.
* @param any_vnum ability The ABIL_ const to gain.
* @param double amount The amount to gain (0-100).
*/
void gain_ability_exp(char_data *ch, any_vnum ability, double amount) {
	ability_data *abil = find_ability_by_vnum(ability);
	skill_data *skill = abil ? ABIL_ASSIGNED_SKILL(abil) : NULL;
	
	if (IS_NPC(ch) || !skill) {
		return;
	}
	
	if (!can_gain_skill_from(ch, abil)) {
		return;
	}
		
	// try gain
	if (skill && gain_skill_exp(ch, SKILL_VNUM(skill), amount)) {
		// increment gains from this
		mark_level_gained_from_ability(ch, abil);
	}
}


/**
* Mostly-raw skill gain/loss -- slightly more checking than set_skill(). This
* will not pass skill cap boundaries. It will NEVER gain you from 0 either.
*
* @param char_data *ch The character who is to gain/lose the skill.
* @param skill_data *skill The skill to gain/lose.
* @param int amount The number of skill points to gain/lose (gaining will stop at any cap).
* @param bool Returns TRUE if a skill point was gained or lost.
*/
bool gain_skill(char_data *ch, skill_data *skill, int amount) {
	bool any = FALSE, pos = (amount > 0);
	struct player_skill_data *skdata;
	int points;
	
	if (!ch || IS_NPC(ch) || !skill) {
		return FALSE;
	}
	
	if (!IS_APPROVED(ch) && config_get_bool("skill_gain_approval")) {
		return FALSE;
	}
	
	// inability to gain from 0?
	if (get_skill_level(ch, SKILL_VNUM(skill)) == 0 && !CAN_GAIN_NEW_SKILLS(ch)) {
		return FALSE;
	}
	
	// reasons a character would not gain no matter what
	if (amount > 0 && !noskill_ok(ch, SKILL_VNUM(skill))) {
		return FALSE;
	}
	
	// load skill data now (this is the final check)
	if (!(skdata = get_skill_data(ch, SKILL_VNUM(skill), TRUE))) {
		return FALSE;
	}
	
	while (amount != 0 && (amount > 0 || skdata->level > 0) && (amount < 0 || !IS_ANY_SKILL_CAP(ch, SKILL_VNUM(skill)))) {
		any = TRUE;
		if (pos) {
			set_skill(ch, SKILL_VNUM(skill), skdata->level + 1);
			--amount;
		}
		else {
			set_skill(ch, SKILL_VNUM(skill), skdata->level - 1);
			++amount;
		}
	}
	
	if (any) {
		// messaging
		if (pos) {
			msg_to_char(ch, "&yYou improve your %s skill to %d.&0\r\n", SKILL_NAME(skill), skdata->level);
			
			points = get_ability_points_available_for_char(ch, SKILL_VNUM(skill));
			if (points > 0) {
				msg_to_char(ch, "&yYou have %d ability point%s to spend. Type 'skill %s' to see %s.&0\r\n", points, (points != 1 ? "s" : ""), SKILL_NAME(skill), (points != 1 ? "them" : "it"));
			}
			
			// did we hit a cap? free reset!
			if (IS_ANY_SKILL_CAP(ch, SKILL_VNUM(skill))) {
				skdata->resets = MIN(skdata->resets + 1, MAX_SKILL_RESETS);
				msg_to_char(ch, "&yYou have earned a free skill reset in %s. Type 'skill reset %s' to use it.&0\r\n", SKILL_NAME(skill), SKILL_NAME(skill));
			}
		}
		else {
			msg_to_char(ch, "&yYour %s skill drops to %d.&0\r\n", SKILL_NAME(skill), skdata->level);
		}
		
		// update class and progression
		update_class(ch);
		SAVE_CHAR(ch);
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
* @param any_vnum skill_vnum The skill to gain experience in.
* @param double amount The amount to gain (0-100).
* @return bool TRUE if the character gained a skill point from the exp.
*/
bool gain_skill_exp(char_data *ch, any_vnum skill_vnum, double amount) {
	struct player_skill_data *skdata;
	skill_data *skill;
	bool gained;
	
	// simply sanitation
	if (amount <= 0 || !ch || !(skdata = get_skill_data(ch, skill_vnum, TRUE)) || !(skill = skdata->ptr)) {
		return FALSE;
	}
	
	// reasons a character would not gain no matter what
	if (skdata->noskill || IS_ANY_SKILL_CAP(ch, skill_vnum)) {
		return FALSE;
	}

	// this allows bonus skillups...
	if (GET_DAILY_BONUS_EXPERIENCE(ch) <= 0) {
		amount /= 50.0;
	}
	
	// gain the exp
	skdata->exp += amount;
	
	// can gain at all?
	if (skdata->exp < config_get_int("min_exp_to_roll_skillup")) {
		return FALSE;
	}
	
	// check for skill gain
	gained = (number(1, 100) <= skdata->exp);
	
	if (gained) {
		GET_DAILY_BONUS_EXPERIENCE(ch) = MAX(0, GET_DAILY_BONUS_EXPERIENCE(ch) - 1);
		gained = gain_skill(ch, skill, 1);
	}
	
	return gained;
}


/**
* Get one ability from a player's hash (or add it if missing). Note that the
* add_if_missing param does not guarantee a non-NULL return if the other
* arguments are bad.
*
* @param char_data *ch The player to get the ability for (NPCs always return NULL).
* @param any_vnum abil_id A valid ability number.
* @param bool add_if_missing If TRUE, will add a ability entry if they player has none.
* @return struct player_ability_data* The ability entry if possible, or NULL.
*/
struct player_ability_data *get_ability_data(char_data *ch, any_vnum abil_id, bool add_if_missing) {
	struct player_ability_data *data;
	ability_data *ptr;
	
	// check bounds
	if (!ch || IS_NPC(ch)) {
		return NULL;
	}
	
	HASH_FIND_INT(GET_ABILITY_HASH(ch), &abil_id, data);
	if (!data && add_if_missing && (ptr = find_ability_by_vnum(abil_id))) {
		CREATE(data, struct player_ability_data, 1);
		data->vnum = abil_id;
		data->ptr = ptr;
		HASH_ADD_INT(GET_ABILITY_HASH(ch), vnum, data);
	}
	
	return data;	// may be NULL
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
int get_ability_level(char_data *ch, any_vnum ability) {
	ability_data *abd;
	
	if (IS_NPC(ch)) {
		return get_approximate_level(ch);
	}
	else {
		// players
		if (ability != NO_ABIL && !has_ability(ch, ability)) {
			return 0;
		}
		else if (ability != NO_ABIL && (abd = find_ability_by_vnum(ability)) && ABIL_ASSIGNED_SKILL(abd)) {
			return get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abd)));
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
* @param any_vnum skill which SKILL_x
* @param int level at what level
* @return int how many abilities are available by that level
*/
int get_ability_points_available(any_vnum skill, int level) {
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
* @param char_data *ch the person to check
* @param any_vnum skill Which SKILL_x to check
* @return int how many ability points ch has available in skill
*/
int get_ability_points_available_for_char(char_data *ch, any_vnum skill) {
	int max = get_ability_points_available(skill, NEXT_CAP_LEVEL(ch, skill));
	int spent = get_ability_points_spent(ch, skill);
	int avail = MAX(0, get_ability_points_available(skill, get_skill_level(ch, skill)) - spent);
	
	// allow early if they're at a deadend
	if (avail == 0 && spent < max && get_skill_level(ch, skill) >= EMPIRE_CHORE_SKILL_CAP && green_skill_deadend(ch, skill)) {
		return 1;
	}
	else {
		return avail;
	}
}


/**
* @param char_data *ch the person to check
* @param any_vnum skill Which SKILL_x to check
* @return int total abilities bought in a skill
*/
int get_ability_points_spent(char_data *ch, any_vnum skill) {
	struct skill_ability *skab;
	skill_data *skd;
	int count = 0;
	
	if (!(skd = find_skill_by_vnum(skill))) {
		return count;
	}
	
	LL_FOREACH(SKILL_ABILITIES(skd), skab) {
		if (skab->level <= 0) {
			continue;	// skip level-0 abils
		}
		if (!has_ability(ch, skab->vnum)) {
			continue;	// does not have ability
		}
		
		// found
		++count;
	}
	
	return count;
}


/**
* @param char_data *ch The character whose skills to use, and who to send to.
* @param skill_data *skill Which skill to show.
* @param any_vnum prereq Which ability, for dependent abilities (NO_PREREQ for base abils).
* @param int indent How far to indent (goes up as the list goes deeper).
* @return string the display
*/
char *get_skill_abilities_display(char_data *ch, skill_data *skill, any_vnum prereq, int indent) {
	char out[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH], colorize[16];
	static char retval[MAX_STRING_LENGTH];
	struct skill_ability *skab;
	int ind, max_skill = 0;
	ability_data *abil;
	
	*out = '\0';
	
	LL_FOREACH(SKILL_ABILITIES(skill), skab) {
		if (skab->prerequisite != prereq) {
			continue;
		}
		if (!(abil = find_ability_by_vnum(skab->vnum))) {
			continue;
		}
		
		// indent
		for (ind = 0; ind < (2 * indent); ++ind) {
			lbuf[ind] = ' ';
		}
		lbuf[2 * indent] = '\0';
		
		if (prereq != NO_PREREQ) {
			strcat(lbuf, "+ ");
		}
		
		if (skab->level < BASIC_SKILL_CAP) {
			max_skill = BASIC_SKILL_CAP;
		}
		else if (skab->level < SPECIALTY_SKILL_CAP) {
			max_skill = SPECIALTY_SKILL_CAP;
		}
		else if (skab->level <= CLASS_SKILL_CAP) {
			max_skill = CLASS_SKILL_CAP;
		}
		
		// get the proper color for this ability
		strcpy(colorize, ability_color(ch, abil));
		
		sprintf(out + strlen(out), "%s(%s) %s%s&0 [%d-%d]", lbuf, (has_ability(ch, ABIL_VNUM(abil)) ? "x" : " "), colorize, ABIL_NAME(abil), skab->level, max_skill);
		
		if (has_ability(ch, ABIL_VNUM(abil))) {
			// this is kind of a hack to quickly make sure you can still use the ability
			if (levels_gained_from_ability(ch, abil) < GAINS_PER_ABILITY && !strcmp(colorize, "&y")) {
				sprintf(out + strlen(out), " %d/%d skill points gained", levels_gained_from_ability(ch, abil), GAINS_PER_ABILITY);
			}
			else {
				strcat(out, " (max)");
			}
		}
		else {
			// does not have the ability at all
			// we check that the parent skill exists first, since this reports it
			
			if (skab->prerequisite != NO_PREREQ && !has_ability(ch, skab->prerequisite)) {
				sprintf(out + strlen(out), " requires %s", get_ability_name_by_vnum(skab->prerequisite));
			}
			else if (get_skill_level(ch, SKILL_VNUM(skill)) < skab->level) {
				sprintf(out + strlen(out), " requires %s %d", SKILL_NAME(skill), skab->level);
			}
			else {
				strcat(out, " unpurchased");
			}
		}
		
		strcat(out, "\r\n");

		// dependencies
		strcat(out, get_skill_abilities_display(ch, skill, skab->vnum, indent+1));
	}
	
	strcpy(retval, out);
	return retval;
}


/**
* Get one skill from a player's hash (or add it if missing). Note that the
* add_if_missing param does not guarantee a non-NULL return if the other
* arguments are bad.
*
* @param char_data *ch The player to get skills for (NPCs always return NULL).
* @param any_vnum vnum A valid skill number.
* @param bool add_if_missing If TRUE, will add a skill entry if they player has none.
* @return struct player_skill_data* The skill entry if possible, or NULL.
*/
struct player_skill_data *get_skill_data(char_data *ch, any_vnum vnum, bool add_if_missing) {
	struct player_skill_data *sk = NULL;
	skill_data *ptr;
	
	// check bounds
	if (!ch || IS_NPC(ch)) {
		return NULL;
	}
	
	HASH_FIND_INT(GET_SKILL_HASH(ch), &vnum, sk);
	if (!sk && add_if_missing && (ptr = find_skill_by_vnum(vnum))) {
		CREATE(sk, struct player_skill_data, 1);
		sk->vnum = vnum;
		sk->ptr = ptr;
		HASH_ADD_INT(GET_SKILL_HASH(ch), vnum, sk);
	}
	
	return sk;	// may be NULL
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


/**
* Get the info row for one skill.
*
* @param char_data *ch The person whose numbers to use.
* @param skill_data *skill Which skill.
* @return char* The display.
*/
char *get_skill_row_display(char_data *ch, skill_data *skill) {
	static char out[MAX_STRING_LENGTH];
	struct player_skill_data *skdata;
	char exp[256];
	int points = get_ability_points_available_for_char(ch, SKILL_VNUM(skill));
	
	skdata = get_skill_data(ch, SKILL_VNUM(skill), FALSE);
	
	if (skdata && !skdata->noskill && !IS_ANY_SKILL_CAP(ch, SKILL_VNUM(skill))) {
		sprintf(exp, ", %.1f%% exp", skdata->exp);
	}
	else {
		*exp = '\0';
	}
	
	sprintf(out, "[%3d] %s%s&0%s (%s%s) - %s\r\n", (skdata ? skdata->level : 0), IS_ANY_SKILL_CAP(ch, SKILL_VNUM(skill)) ? "&g" : "&y", SKILL_NAME(skill), (points > 0 ? "*" : ""), IS_ANY_SKILL_CAP(ch, SKILL_VNUM(skill)) ? "&ymax&0" : ((skdata && skdata->noskill) ? "\trnoskill\t0" : "\tcgaining\t0"), exp, SKILL_DESC(skill));
	return out;
}


/**
* Determines if a player has hit a deadend with abilities they can level off
* of.
*
* @param char_data *ch The player.
* @param any_vnum skill Any SKILL_ const or vnum.
* @return bool TRUE if the player has found no levelable abilities
*/
bool green_skill_deadend(char_data *ch, any_vnum skill) {
	int avail = get_ability_points_available(skill, get_skill_level(ch, skill)) - get_ability_points_spent(ch, skill);
	skill_data *skdata = find_skill_by_vnum(skill);
	struct skill_ability *skab;
	bool yellow = FALSE;
		
	if (avail <= 0 && skdata) {
		LL_FOREACH(SKILL_ABILITIES(skdata), skab) {
			yellow |= can_gain_skill_from(ch, find_ability_by_vnum(skab->vnum));
		}
	}
	
	// true if we found no yellows
	return !yellow;
}


/**
* Adds one to the number of levels ch gained from an ability.
*
* @param char_data *ch The player to check.
* @param ability_data *abil Any valid ability.
*/
void mark_level_gained_from_ability(char_data *ch, ability_data *abil) {
	struct player_ability_data *data = get_ability_data(ch, ABIL_VNUM(abil), TRUE);
	if (data) {
		data->levels_gained += 1;
	}
}


/**
* Switches a player's current skill set from one to the other.
*
* @param char_data *ch The player to swap.
*/
void perform_swap_skill_sets(char_data *ch) {
	void assign_class_abilities(char_data *ch, class_data *cls, int role);
	
	struct player_ability_data *plab, *next_plab;
	int cur_set, old_set;
	ability_data *abil;
	
	if (IS_NPC(ch)) { // somehow...
		return;
	}
	
	// note: if you ever raise NUM_SKILL_SETS, this is going to need an update
	old_set = GET_CURRENT_SKILL_SET(ch);
	cur_set = (old_set == 1) ? 0 : 1;
	
	// remove ability techs
	if (GET_LOYALTY(ch)) {
		adjust_abilities_to_empire(ch, GET_LOYALTY(ch), FALSE);
	}
	
	// update skill set
	GET_CURRENT_SKILL_SET(ch) = cur_set;
	
	// update abilities:
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		abil = plab->ptr;
		
		if (ABIL_ASSIGNED_SKILL(abil) != NULL) {	// skill ability
			if (plab->purchased[cur_set] && !plab->purchased[old_set]) {
				// added
				qt_change_ability(ch, ABIL_VNUM(abil));
			}
			else if (plab->purchased[old_set] && !plab->purchased[cur_set]) {
				// removed
				check_skill_sell(ch, abil);
				qt_change_ability(ch, ABIL_VNUM(abil));
			}
		}
		else {	// class ability: just ensure it matches the old one
			plab->purchased[cur_set] = plab->purchased[old_set];
			qt_change_ability(ch, ABIL_VNUM(abil));	// in case
		}
	}
	
	// call this at the end just in case
	assign_class_abilities(ch, NULL, NOTHING);
	affect_total(ch);
	
	// add ability techs -- only if playing
	if (GET_LOYALTY(ch)) {
		adjust_abilities_to_empire(ch, GET_LOYALTY(ch), TRUE);
		resort_empires(FALSE);
	}
}


/**
* Takes an ability away from a player, on a given skill set.
*
* @param char_data *ch The player to check.
* @param ability_data *abil Any ability.
* @param int skill_set Which skill set number (0..NUM_SKILL_SETS-1).
* @param bool reset_levels If TRUE, wipes out the number of levels gained from the ability.
*/
void remove_ability_by_set(char_data *ch, ability_data *abil, int skill_set, bool reset_levels) {
	struct player_ability_data *data = get_ability_data(ch, ABIL_VNUM(abil), FALSE);
	
	if (skill_set < 0 || skill_set >= NUM_SKILL_SETS) {
		log("SYSERR: Attempting to remove ability '%s' to player '%s' on invalid skill set '%d'", GET_NAME(ch), ABIL_NAME(abil), skill_set);
		return;
	}
	
	if (data) {
		data->purchased[skill_set] = FALSE;
		
		if (reset_levels) {
			data->levels_gained = 0;
		}
		
		qt_change_ability(ch, ABIL_VNUM(abil));
	}
}


/**
* Takes an ability away from a player on their CURRENT set.
*
* @param char_data *ch The player to check.
* @param ability_data *abil Any ability.
* @param bool reset_levels If TRUE, wipes out the number of levels gained from the ability.
*/
void remove_ability(char_data *ch, ability_data *abil, bool reset_levels) {
	remove_ability_by_set(ch, abil, GET_CURRENT_SKILL_SET(ch), reset_levels);
}


/**
* When a character loses skill levels, we clear the "levels_gained" tracker
* for abilities that are >= their new level, so they can successfully regain
* those levels later.
*
* @param char_data *ch The player.
* @param any_vnum skill Which SKILL_x.
*/
void reset_skill_gain_tracker_on_abilities_above_level(char_data *ch, any_vnum skill) {
	struct player_ability_data *abil, *next_abil;
	ability_data *ability;
	
	if (!IS_NPC(ch)) {
		HASH_ITER(hh, GET_ABILITY_HASH(ch), abil, next_abil) {
			ability = abil->ptr;
			if (ABIL_ASSIGNED_SKILL(ability) && SKILL_VNUM(ABIL_ASSIGNED_SKILL(ability)) == skill && ABIL_SKILL_LEVEL(ability) >= get_skill_level(ch, skill)) {
				abil->levels_gained = 0;
			}
		}
	}
}


// set a skill directly to a level
void set_skill(char_data *ch, any_vnum skill, int level) {
	struct player_skill_data *skdata;
	bool gain = FALSE;
	
	if ((skdata = get_skill_data(ch, skill, TRUE))) {
		gain = (level > skdata->level);
		
		skdata->level = level;
		skdata->exp = 0.0;
		
		if (!gain) {
			reset_skill_gain_tracker_on_abilities_above_level(ch, skill);
		}
		
		qt_change_skill_level(ch, skill);
	}
}


/**
* @param char_data *ch who to check for
* @param any_vnum ability which ABIL_x
* @param int difficulty any DIFF_x const
* @return bool TRUE for success, FALSE for fail
*/
bool skill_check(char_data *ch, any_vnum ability, int difficulty) {
	extern double skill_check_difficulty_modifier[NUM_DIFF_TYPES];

	int chance = get_ability_level(ch, ability);

	// players without the ability have no chance	
	if (!IS_NPC(ch) && !has_ability(ch, ability)) {
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
	struct player_skill_data *skdata;
	skill_data *skill;

	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		return;
	}
	else if (!*argument) {
		msg_to_char(ch, "Usage: noskill <skill>\r\n");
		msg_to_char(ch, "Type 'skills' to see which skills are gaining and which are not.\r\n");
	}
	else if (!(skill = find_skill_by_name(argument)) || !(skdata = get_skill_data(ch, SKILL_VNUM(skill), TRUE))) {
		msg_to_char(ch, "Unknown skill.\r\n");
	}
	else {
		if (skdata->noskill) {
			skdata->noskill = FALSE;
			msg_to_char(ch, "You will now &cbe able to gain&0 %s skill.\r\n", SKILL_NAME(skill));
		}
		else {
			skdata->noskill = TRUE;
			msg_to_char(ch, "You will &rno longer&0 gain %s skill.\r\n", SKILL_NAME(skill));
		}
	}
}


ACMD(do_skills) {
	void clear_char_abilities(char_data *ch, any_vnum skill);
	
	char arg2[MAX_INPUT_LENGTH], lbuf[MAX_INPUT_LENGTH], outbuf[MAX_STRING_LENGTH];
	struct player_skill_data *skdata;
	skill_data *skill, *next_skill;
	struct skill_ability *skab;
	ability_data *abil;
	int points, level;
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
		HASH_ITER(sorted_hh, sorted_skills, skill, next_skill) {
			if (SKILL_FLAGGED(skill, SKILLF_IN_DEVELOPMENT)) {
				continue;
			}
			
			strcat(outbuf, get_skill_row_display(ch, skill));
			if (!found && get_ability_points_available_for_char(ch, SKILL_VNUM(skill)) > 0) {
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
		
		if (!(abil = find_ability_by_name(lbuf))) {
			msg_to_char(ch, "No such ability '%s'.\r\n", lbuf);
			return;
		}
		
		if (has_ability(ch, ABIL_VNUM(abil))) {
			msg_to_char(ch, "You already know the %s ability.\r\n", ABIL_NAME(abil));
			return;
		}
		
		if (!ABIL_ASSIGNED_SKILL(abil)) {
			msg_to_char(ch, "You cannot buy that ability.\r\n");
			return;
		}
		
		if (get_ability_points_available_for_char(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) < 1 && !IS_IMMORTAL(ch)) {
			msg_to_char(ch, "You have no points available to spend in %s.\r\n", SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)));
			return;
		}
		
		// check level
		if (get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) < ABIL_SKILL_LEVEL(abil)) {
			msg_to_char(ch, "You need at least %d in %s to buy %s.\r\n", ABIL_SKILL_LEVEL(abil), SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)), ABIL_NAME(abil));
			return;
		}
		
		// check pre-req
		if ((skab = find_skill_ability(ABIL_ASSIGNED_SKILL(abil), abil)) && skab->prerequisite != NO_PREREQ && !has_ability(ch, skab->prerequisite)) {
			msg_to_char(ch, "You need to buy %s before you can buy %s.\r\n", get_ability_name_by_vnum(skab->prerequisite), ABIL_NAME(abil));
			return;
		}
		
		// good to go
		
		emp = GET_LOYALTY(ch);
		
		// remove empire abilities temporarily
		if (emp) {
			adjust_abilities_to_empire(ch, emp, FALSE);
		}
		
		add_ability(ch, abil, FALSE);
		msg_to_char(ch, "You purchase %s.\r\n", ABIL_NAME(abil));
		SAVE_CHAR(ch);
		
		// re-add empire abilities
		if (emp) {
			adjust_abilities_to_empire(ch, emp, TRUE);
			resort_empires(FALSE);
		}
	}
	else if (!strn_cmp(argument, "reset", 5)) {
		// self-clear!
		half_chop(argument, arg, lbuf);
		
		if (!*lbuf) {
			msg_to_char(ch, "Usage: skills reset <skill>\r\n");
			msg_to_char(ch, "You have free resets available in: ");
			
			found = FALSE;
			HASH_ITER(sorted_hh, sorted_skills, skill, next_skill) {
				if (SKILL_FLAGGED(skill, SKILLF_IN_DEVELOPMENT)) {
					continue;
				}
				
				if (get_skill_resets(ch, SKILL_VNUM(skill)) > 0) {
					msg_to_char(ch, "%s%s", (found ? ", " : ""), SKILL_NAME(skill));
					if (get_skill_resets(ch, SKILL_VNUM(skill)) > 1) {
						msg_to_char(ch, " (%d)", get_skill_resets(ch, SKILL_VNUM(skill)));
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
		else if (!(skill = find_skill_by_name(lbuf))) {
			msg_to_char(ch, "No such skill %s.\r\n", lbuf);
		}
		else if (!IS_IMMORTAL(ch) && get_skill_resets(ch, SKILL_VNUM(skill)) == 0) {
			msg_to_char(ch, "You do not have a free reset available for %s.\r\n", SKILL_NAME(skill));
		}
		else {
			if (!IS_IMMORTAL(ch) && (skdata = get_skill_data(ch, SKILL_VNUM(skill), TRUE))) {
				skdata->resets = MAX(skdata->resets - 1, 0);
			}
			clear_char_abilities(ch, SKILL_VNUM(skill));
			
			msg_to_char(ch, "You have reset your %s abilities.\r\n", SKILL_NAME(skill));
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
		else if (!(skill = find_skill_by_name(arg))) {
			msg_to_char(ch, "Unknown skill '%s'.\r\n", arg);
		}
		else if (SKILL_MIN_DROP_LEVEL(skill) >= CLASS_SKILL_CAP) {
			msg_to_char(ch, "You can't drop your skill level in %s.\r\n", SKILL_NAME(skill));
		}
		else if (!is_number(arg2)) {
			msg_to_char(ch, "Invalid level.\r\n");
		}
		else if ((level = atoi(arg2)) >= get_skill_level(ch, SKILL_VNUM(skill))) {
			msg_to_char(ch, "You can only drop skills to lower levels.\r\n");
		}
		else if (level < SKILL_MIN_DROP_LEVEL(skill)) {
			msg_to_char(ch, "You can't drop %s lower than %d.\r\n", SKILL_NAME(skill), SKILL_MIN_DROP_LEVEL(skill));
		}
		else if (level != SKILL_MIN_DROP_LEVEL(skill) && level != BASIC_SKILL_CAP && level != SPECIALTY_SKILL_CAP) {
			if (SKILL_MIN_DROP_LEVEL(skill) < BASIC_SKILL_CAP) {
				msg_to_char(ch, "You can only drop %s to the following levels: %d, %d, %d\r\n", SKILL_NAME(skill), SKILL_MIN_DROP_LEVEL(skill), BASIC_SKILL_CAP, SPECIALTY_SKILL_CAP);
			}
			else if (SKILL_MIN_DROP_LEVEL(skill) < SPECIALTY_SKILL_CAP) {
				msg_to_char(ch, "You can only drop %s to the following levels: %d, %d\r\n", SKILL_NAME(skill), SKILL_MIN_DROP_LEVEL(skill), SPECIALTY_SKILL_CAP);
			}
			else {
				msg_to_char(ch, "You can only drop %s to the following levels: %d\r\n", SKILL_NAME(skill), SKILL_MIN_DROP_LEVEL(skill));
			}
		}
		else {
			// good to go!
			msg_to_char(ch, "You have dropped your %s skill to %d and reset abilities above that level.\r\n", SKILL_NAME(skill), level);
			set_skill(ch, SKILL_VNUM(skill), level);
			update_class(ch);
			check_ability_levels(ch, SKILL_VNUM(skill));
			
			SAVE_CHAR(ch);
		}
	}
	else if (!str_cmp(argument, "swap")) {
		if (IS_IMMORTAL(ch)) {
			perform_swap_skill_sets(ch);
			msg_to_char(ch, "You swap skill sets.\r\n");
		}
		else if (!config_get_bool("skill_swap_allowed")) {
			msg_to_char(ch, "This game does not allow skill swap.\r\n");
		}
		else if (get_approximate_level(ch) < config_get_int("skill_swap_min_level")) {
			msg_to_char(ch, "You must be at least level %d to swap skill sets.\r\n", config_get_int("skill_swap_min_level"));
		}
		else if (GET_POS(ch) < POS_STANDING) {
			msg_to_char(ch, "You can't swap skill sets right now.\r\n");
		}
		else if (GET_ACTION(ch) == ACT_SWAP_SKILL_SETS) {
			msg_to_char(ch, "You're already doing that. Type 'stop' to cancel.\r\n");
		}
		else if (GET_ACTION(ch) != ACT_NONE) {
			msg_to_char(ch, "You're too busy to swap skill sets right now.\r\n");
		}
		else {
			start_action(ch, ACT_SWAP_SKILL_SETS, 3);
			act("You prepare to swap skill sets...", FALSE, ch, NULL, NULL, TO_CHAR);
			act("$n prepares to swap skill sets...", TRUE, ch, NULL, NULL, TO_ROOM);
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
		
		if (!(abil = find_ability_by_name(lbuf))) {
			msg_to_char(ch, "No such ability '%s'.\r\n", lbuf);
			return;
		}
		
		if (!has_ability(ch, ABIL_VNUM(abil))) {
			msg_to_char(ch, "You do not know the %s ability.\r\n", ABIL_NAME(abil));
			return;
		}
		
		// good to go
		msg_to_char(ch, "You no longer know %s.\r\n", ABIL_NAME(abil));

		if (GET_LOYALTY(ch)) {
			adjust_abilities_to_empire(ch, GET_LOYALTY(ch), FALSE);
		}

		remove_ability(ch, abil, FALSE);
		check_skill_sell(ch, abil);
		SAVE_CHAR(ch);
		
		if (GET_LOYALTY(ch)) {
			adjust_abilities_to_empire(ch, GET_LOYALTY(ch), TRUE);
			resort_empires(FALSE);
		}
	}
	else {
		*outbuf = '\0';
		
		// argument: show abilities for 1 skill
		skill = find_skill_by_name(argument);
		if (skill) {
			// header
			strcat(outbuf, get_skill_row_display(ch, skill));
			
			points = get_ability_points_available_for_char(ch, SKILL_VNUM(skill));
			if (points > 0) {
				sprintf(outbuf + strlen(outbuf), "You have %d ability point%s to spend. Type 'skill buy <ability>' to purchase a new ability.\r\n", points, (points != 1 ? "s" : ""));
			}
			
			// list
			strcat(outbuf, get_skill_abilities_display(ch, skill, NO_PREREQ, 1));
			
			page_string(ch->desc, outbuf, 1);
		}
		else {
			msg_to_char(ch, "No such skill.\r\n");
		}
	}
}


// this is a temporary command for picking skill specs, and will ultimately be replaced by quests or other mechanisms
ACMD(do_specialize) {
	struct player_skill_data *plsk, *next_plsk;
	int count, specialty_allowed;
	skill_data *sk;
	
	skip_spaces(&argument);
	
	// make certain these are up-to-datec
	specialty_allowed = NUM_SPECIALTY_SKILLS_ALLOWED + (CAN_GET_BONUS_SKILLS(ch) ? BONUS_SPECIALTY_SKILLS_ALLOWED : 0);
	
	if (!*argument) {
		msg_to_char(ch, "Specialize in which skill?\r\n");
	}
	else if (!(sk = find_skill_by_name(argument))) {
		msg_to_char(ch, "No such skill.\r\n");
	}
	else if (get_skill_level(ch, SKILL_VNUM(sk)) != BASIC_SKILL_CAP && get_skill_level(ch, SKILL_VNUM(sk)) != SPECIALTY_SKILL_CAP) {
		msg_to_char(ch, "You can only specialize skills which are at %d or %d.\r\n", BASIC_SKILL_CAP, SPECIALTY_SKILL_CAP);
	}
	else if (get_skill_level(ch, SKILL_VNUM(sk)) >= SKILL_MAX_LEVEL(sk)) {
		msg_to_char(ch, "%s cannot go above %d.\r\n", SKILL_NAME(sk), SKILL_MAX_LEVEL(sk));
	}
	else {
		// check > basic
		if (get_skill_level(ch, SKILL_VNUM(sk)) == BASIC_SKILL_CAP) {
			count = 0;
			HASH_ITER(hh, GET_SKILL_HASH(ch), plsk, next_plsk) {
				if (plsk->level > BASIC_SKILL_CAP) {
					++count;
				}
			}
			if ((count + 1) > specialty_allowed) {
				msg_to_char(ch, "You can only have at most %d skill%s above level %d.\r\n", specialty_allowed, (specialty_allowed != 1 ? "s" : ""), BASIC_SKILL_CAP);
				return;
			}
		}
		
		// check > specialty
		if (get_skill_level(ch, SKILL_VNUM(sk)) == SPECIALTY_SKILL_CAP) {
			count = 0;
			HASH_ITER(hh, GET_SKILL_HASH(ch), plsk, next_plsk) {
				if (plsk->level > SPECIALTY_SKILL_CAP) {
					++count;
				}
			}
			if ((count + 1) > NUM_CLASS_SKILLS_ALLOWED) {
				msg_to_char(ch, "You can only have %d skill%s above level %d.\r\n", NUM_CLASS_SKILLS_ALLOWED, (NUM_CLASS_SKILLS_ALLOWED != 1 ? "s" : ""), SPECIALTY_SKILL_CAP);
				return;
			}
		}

		// le done
		set_skill(ch, SKILL_VNUM(sk), get_skill_level(ch, SKILL_VNUM(sk)) + 1);
		msg_to_char(ch, "You have specialized in %s.\r\n", SKILL_NAME(sk));

		// check class and skill levels
		update_class(ch);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Checks if ch can legally gain experience from an ability used against vict.
*
* @param char_data *ch The player trying to gain exp.
* @param char_data *vict The victim of the ability.
* @return bool TRUE if okay to gain experience, or FALSE.
*/
bool can_gain_exp_from(char_data *ch, char_data *vict) {
	if (IS_NPC(ch)) {
		return FALSE;	// mobs gain no exp
	}
	if (ch == vict || !vict) {
		return TRUE;	// always okay
	}
	if (MOB_FLAGGED(vict, MOB_NO_EXPERIENCE)) {
		return FALSE;
	}
	if ((!IS_NPC(vict) || GET_CURRENT_SCALE_LEVEL(vict) > 0) && get_approximate_level(vict) < get_approximate_level(ch) - config_get_int("exp_level_difference")) {
		return FALSE;
	}
	
	// seems ok
	return TRUE;
}


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
	any_vnum abil = NO_ABIL;
	struct obj_apply *app;
	int iter, level_min;
	bool honed;

	// players won't be able to use gear >= these levels if their skill level is < the level
	int skill_level_ranges[] = { CLASS_SKILL_CAP, SPECIALTY_SKILL_CAP, BASIC_SKILL_CAP, -1 };	// terminate with -1
	
	if (IS_NPC(ch)) {
		return TRUE;
	}
	
	if (IS_ARMOR(item)) {
		switch (GET_ARMOR_TYPE(item)) {
			case ARMOR_MAGE: {
				abil = ABIL_MAGE_ARMOR;
				break;
			}
			case ARMOR_LIGHT: {
				abil = ABIL_LIGHT_ARMOR;
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
	
	if (abil != NO_ABIL && !has_ability(ch, abil)) {
		if (send_messages) {
			snprintf(buf, sizeof(buf), "You require the %s ability to use $p.", get_ability_name_by_vnum(abil));
			act(buf, FALSE, ch, item, NULL, TO_CHAR);
		}
		return FALSE;
	}
	
	// check honed
	honed = FALSE;
	LL_FOREACH(GET_OBJ_APPLIES(item), app) {
		if (app->apply_type == APPLY_TYPE_HONED) {
			honed = TRUE;
		}
	}
	if (honed && !has_ability(ch, ABIL_HONE)) {
		if (send_messages) {
			snprintf(buf, sizeof(buf), "You require the %s ability to use $p.", get_ability_name_by_vnum(ABIL_HONE));
			act(buf, FALSE, ch, item, NULL, TO_CHAR);
		}
		return FALSE;
	}
	
	// check levels
	if (!IS_IMMORTAL(ch)) {
		if (GET_OBJ_CURRENT_SCALE_LEVEL(item) <= CLASS_SKILL_CAP) {
			for (iter = 0; skill_level_ranges[iter] != -1; ++iter) {
				if (GET_OBJ_CURRENT_SCALE_LEVEL(item) > skill_level_ranges[iter] && GET_SKILL_LEVEL(ch) < skill_level_ranges[iter]) {
					if (send_messages) {
						snprintf(buf, sizeof(buf), "You need to be skill level %d to use $p.", skill_level_ranges[iter]);
						act(buf, FALSE, ch, item, NULL, TO_CHAR);
					}
					return FALSE;
				}
			}
		}
		else {	// > 100
			if (OBJ_FLAGGED(item, OBJ_BIND_ON_PICKUP)) {
				level_min = GET_OBJ_CURRENT_SCALE_LEVEL(item) - 50;
			}
			else {
				level_min = GET_OBJ_CURRENT_SCALE_LEVEL(item) - 25;
			}
			level_min = MAX(level_min, CLASS_SKILL_CAP);
			if (GET_SKILL_LEVEL(ch) < CLASS_SKILL_CAP) {
				if (send_messages) {
					snprintf(buf, sizeof(buf), "You need to be skill level %d and total level %d to use $p.", CLASS_SKILL_CAP, level_min);
					act(buf, FALSE, ch, item, NULL, TO_CHAR);
				}
				return FALSE;
			}
			if (GET_HIGHEST_KNOWN_LEVEL(ch) < level_min) {
				if (send_messages) {
					snprintf(buf, sizeof(buf), "You need to be level %d to use $p.", level_min);
					act(buf, FALSE, ch, item, NULL, TO_CHAR);
				}
				return FALSE;
			}
		}
	}
	
	return TRUE;
}


/**
* Audits skills on startup. Erroring skills are set IN-DEVELOPMENT
*/
void check_skills(void) {
	struct skill_ability *skab, *next_skab;
	skill_data *skill, *next_skill;
	bool error;
	
	HASH_ITER(hh, skill_table, skill, next_skill) {
		error = FALSE;
		LL_FOREACH_SAFE(SKILL_ABILITIES(skill), skab, next_skab) {
			if (!find_ability_by_vnum(skab->vnum)) {
				log("- Skill [%d] %s has invalid ability %d", SKILL_VNUM(skill), SKILL_NAME(skill), skab->vnum);
				error = TRUE;
				LL_DELETE(SKILL_ABILITIES(skill), skab);
				free(skab);
			}
		}
		
		if (error) {
			SET_BIT(SKILL_FLAGS(skill), SKILLF_IN_DEVELOPMENT);
		}
		
		// ensure sort
		LL_SORT(SKILL_ABILITIES(skill), sort_skill_abilities);
	}
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
* Finds a particular ability's skill assignment data, if any.
*
* @param skill_data *skill The skill whose assignments to check.
* @param ability_data *abil The ability we're looking for.
* @return struct skill_ability* The skill_ability entry, or NULL if none.
*/
struct skill_ability *find_skill_ability(skill_data *skill, ability_data *abil) {
	struct skill_ability *find;
	
	if (!skill || !abil) {
		return NULL;
	}
	
	LL_SEARCH_SCALAR(SKILL_ABILITIES(skill), find, vnum, ABIL_VNUM(abil));
	return find;
}


/**
* Finds a skill by ambiguous argument, which may be a vnum or a name.
* Names are matched by exact match first, or by multi-abbrev.
*
* @param char *argument The user input.
* @return skill_data* The skill, or NULL if it doesn't exist.
*/
skill_data *find_skill(char *argument) {
	skill_data *skill;
	any_vnum vnum;
	
	if (isdigit(*argument) && (vnum = atoi(argument)) >= 0 && (skill = find_skill_by_vnum(vnum))) {
		return skill;
	}
	else {
		return find_skill_by_name(argument);
	}
}


/**
* Look up a skill by multi-abbrev, preferring exact matches. Won't find in-dev
* skills.
*
* @param char *name The skill name to look up.
* @return skill_data* The skill, or NULL if it doesn't exist.
*/
skill_data *find_skill_by_name(char *name) {
	skill_data *skill, *next_skill, *partial = NULL;
	
	if (!*name) {
		return NULL;	// shortcut
	}
	
	HASH_ITER(sorted_hh, sorted_skills, skill, next_skill) {
		if (SKILL_FLAGGED(skill, SKILLF_IN_DEVELOPMENT)) {
			continue;
		}
		
		// matches:
		if (!str_cmp(name, SKILL_NAME(skill)) || !str_cmp(name, SKILL_ABBREV(skill))) {
			// perfect match
			return skill;
		}
		if (!partial && is_multiword_abbrev(name, SKILL_NAME(skill))) {
			// probable match
			partial = skill;
		}
	}
	
	// no exact match...
	return partial;
}


/**
* @param any_vnum vnum Any skill vnum
* @return skill_data* The skill, or NULL if it doesn't exist
*/
skill_data *find_skill_by_vnum(any_vnum vnum) {
	skill_data *skill;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(skill_table, &vnum, skill);
	return skill;
}


/**
* @param any_vnum vnum A skill vnum.
* @return char* The skill abbreviation, or "???" if no match.
*/
char *get_skill_abbrev_by_vnum(any_vnum vnum) {
	skill_data *skill = find_skill_by_vnum(vnum);
	return skill ? SKILL_ABBREV(skill) : "???";
}


/**
* @param any_vnum vnum A skill vnum.
* @return char* The skill name, or "Unknown" if no match.
*/
char *get_skill_name_by_vnum(any_vnum vnum) {
	skill_data *skill = find_skill_by_vnum(vnum);
	return skill ? SKILL_NAME(skill) : "Unknown";
}


/**
* Ensures the player has all level-zero abilities. These are abilities that all
* players always have.
*
* @param char_data *ch The character to give the abilitiies to.
*/
void give_level_zero_abilities(char_data *ch) {
	ability_data *abil, *next_abil;
	int set;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	HASH_ITER(hh, ability_table, abil, next_abil) {
		if (!ABIL_ASSIGNED_SKILL(abil)) {
			continue;	// must be assigned
		}
		if (SKILL_FLAGGED(ABIL_ASSIGNED_SKILL(abil), SKILLF_IN_DEVELOPMENT)) {
			continue;	// skip in-dev skills
		}
		if (ABIL_SKILL_LEVEL(abil) > 0) {
			continue;	// must be level-0
		}
		
		// ensure the character has a skill entry for the skill
		get_skill_data(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil)), TRUE);
		
		// check that the player has it
		for (set = 0; set < NUM_SKILL_SETS; ++set) {
			if (!has_ability_in_set(ch, ABIL_VNUM(abil), set)) {
				// if current set, need to update empire abilities
				if (set == GET_CURRENT_SKILL_SET(ch) && GET_LOYALTY(ch)) {
					adjust_abilities_to_empire(ch, GET_LOYALTY(ch), FALSE);
				}
				
				// add the ability for this set
				add_ability_by_set(ch, abil, set, FALSE);
				
				// if current set, need to update empire abilities
				if (set == GET_CURRENT_SKILL_SET(ch) && GET_LOYALTY(ch)) {
					adjust_abilities_to_empire(ch, GET_LOYALTY(ch), TRUE);
				}
			}
		}
	}
}


/**
* @param char_data *ch
* @return bool TRUE if the character is somewhere he can cook, else FALSE
*/
bool has_cooking_fire(char_data *ch) {
	obj_data *obj;
	
	if (!IS_NPC(ch) && has_ability(ch, ABIL_TOUCH_OF_FLAME)) {
		return TRUE;
	}

	if (room_has_function_and_city_ok(IN_ROOM(ch), FNC_COOKING_FIRE)) {	
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
	int iter;
	
	// slots to check (ensure a -1 terminator)
	int slots[] = { WEAR_WIELD, WEAR_HOLD, WEAR_SHEATH_1, WEAR_SHEATH_2, -1 };
	
	for (iter = 0; slots[iter] != -1; ++iter) {
		obj = GET_EQ(ch, slots[iter]);
		if (obj && IS_WEAPON(obj) && attack_hit_info[GET_WEAPON_TYPE(obj)].weapon_type == WEAPON_SHARP) {
			return obj;
		}
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
		obj_to_char((rope = read_object(o_ROPE, TRUE)), ch);
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


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common skill problems and reports them to ch.
*
* @param skill_data *skill The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_skill(skill_data *skill, char_data *ch) {
	struct skill_ability *skab, *find;
	skill_data *iter, *next_iter;
	bool problem = FALSE;
	ability_data *abil;
	
	if (SKILL_FLAGGED(skill, SKILLF_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, SKILL_VNUM(skill), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!SKILL_NAME(skill) || !*SKILL_NAME(skill) || !str_cmp(SKILL_NAME(skill), default_skill_name)) {
		olc_audit_msg(ch, SKILL_VNUM(skill), "No name set");
		problem = TRUE;
	}
	if (!SKILL_ABBREV(skill) || !*SKILL_ABBREV(skill) || !str_cmp(SKILL_ABBREV(skill), default_skill_abbrev)) {
		olc_audit_msg(ch, SKILL_VNUM(skill), "No abbrev set");
		problem = TRUE;
	}
	if (!SKILL_DESC(skill) || !*SKILL_DESC(skill) || !str_cmp(SKILL_DESC(skill), default_skill_desc)) {
		olc_audit_msg(ch, SKILL_VNUM(skill), "No description set");
		problem = TRUE;
	}
	
	// ability assignments
	LL_FOREACH(SKILL_ABILITIES(skill), skab) {
		if (!(abil = find_ability_by_vnum(skab->vnum))) {
			olc_audit_msg(ch, SKILL_VNUM(skill), "Invalid ability %d", skab->vnum);
			problem = TRUE;
			continue;
		}
		
		if (is_class_ability(abil)) {
			olc_audit_msg(ch, SKILL_VNUM(skill), "Ability %d %s is a class ability", ABIL_VNUM(abil), ABIL_NAME(abil));
			problem = TRUE;
		}
		
		// verify tree
		if (skab->prerequisite) {
			if (skab->vnum == skab->prerequisite) {
				olc_audit_msg(ch, SKILL_VNUM(skill), "Ability %d %s is its own prerequisite", ABIL_VNUM(abil), ABIL_NAME(abil));
				problem = TRUE;
			}
			
			LL_SEARCH_SCALAR(SKILL_ABILITIES(skill), find, vnum, skab->prerequisite);
			if (!find) {
				olc_audit_msg(ch, SKILL_VNUM(skill), "Ability %d %s has missing prerequisite", ABIL_VNUM(abil), ABIL_NAME(abil));
				problem = TRUE;
			}
			else if (skab->level < find->level) {
				olc_audit_msg(ch, SKILL_VNUM(skill), "Ability %d %s is lower level than its prerequisite", ABIL_VNUM(abil), ABIL_NAME(abil));
				problem = TRUE;
			}
		}
	}
	
	// other skills
	HASH_ITER(hh, skill_table, iter, next_iter) {
		if (iter == skill) {
			continue;
		}
		
		if (!str_cmp(SKILL_NAME(iter), SKILL_NAME(skill))) {
			olc_audit_msg(ch, SKILL_VNUM(skill), "Same name as skill %d", SKILL_VNUM(iter));
			problem = TRUE;
		}
		
		// ensure no abilities are assigned to both
		LL_FOREACH(SKILL_ABILITIES(skill), skab) {
			LL_SEARCH_SCALAR(SKILL_ABILITIES(iter), find, vnum, skab->vnum);
			if (find && (abil = find_ability_by_vnum(skab->vnum))) {
				olc_audit_msg(ch, SKILL_VNUM(skill), "Ability %d %s is also assigned to skill %d %s", ABIL_VNUM(abil), ABIL_NAME(abil), SKILL_VNUM(iter), SKILL_NAME(iter));
				problem = TRUE;
			}
		}
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param skill_data *skill The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_skill(skill_data *skill, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s - %s", SKILL_VNUM(skill), SKILL_NAME(skill), SKILL_DESC(skill));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", SKILL_VNUM(skill), SKILL_NAME(skill));
	}
		
	return output;
}


/**
* Searches for all uses of a skill and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The skill vnum.
*/
void olc_search_skill(char_data *ch, any_vnum vnum) {
	extern bool find_quest_reward_in_list(struct quest_reward *list, int type, any_vnum vnum);
	extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);
	
	char buf[MAX_STRING_LENGTH];
	skill_data *skill = find_skill_by_vnum(vnum);
	archetype_data *arch, *next_arch;
	quest_data *quest, *next_quest;
	struct archetype_skill *arsk;
	struct class_skill_req *clsk;
	social_data *soc, *next_soc;
	class_data *cls, *next_cls;
	int size, found;
	bool any;
	
	if (!skill) {
		msg_to_char(ch, "There is no skill %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of skill %d (%s):\r\n", vnum, SKILL_NAME(skill));
	
	// archetypes
	HASH_ITER(hh, archetype_table, arch, next_arch) {
		LL_FOREACH(GET_ARCH_SKILLS(arch), arsk) {
			if (arsk->skill == vnum) {
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "ARCH [%5d] %s\r\n", GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch));
				break;	// only need 1
			}
		}
	}
	
	// classes
	HASH_ITER(hh, class_table, cls, next_cls) {
		LL_FOREACH(CLASS_SKILL_REQUIREMENTS(cls), clsk) {
			if (clsk->vnum == vnum) {
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "CLS [%5d] %s\r\n", CLASS_VNUM(cls), CLASS_NAME(cls));
				break;	// only need 1
			}
		}
	}
	
	// quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = find_quest_reward_in_list(QUEST_REWARDS(quest), QR_SET_SKILL, vnum);
		any |= find_quest_reward_in_list(QUEST_REWARDS(quest), QR_SKILL_EXP, vnum);
		any |= find_quest_reward_in_list(QUEST_REWARDS(quest), QR_SKILL_LEVELS, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_SKILL_LEVEL_OVER, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_SKILL_LEVEL_OVER, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_SKILL_LEVEL_UNDER, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_SKILL_LEVEL_UNDER, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_SKILL_LEVEL_OVER, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_SKILL_LEVEL_UNDER, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Delete by vnum: skill abilities
*
* @param struct skill_ability **list Pointer to a linked list of skill abilities.
* @param any_vnum vnum The vnum to delete from that list.
* @return bool TRUE if it deleted at least one, FALSE if it wasn't in the list.
*/
bool remove_vnum_from_skill_abilities(struct skill_ability **list, any_vnum vnum) {
	struct skill_ability *iter, *next_iter;
	bool found = FALSE;
	
	LL_FOREACH_SAFE(*list, iter, next_iter) {
		if (iter->vnum == vnum) {
			LL_DELETE(*list, iter);
			free(iter);
			found = TRUE;
		}
	}
	
	return found;
}


/**
* Sorts skill abilities alphabetically.
*
* @param struct skill_ability *a First element.
* @param struct skill_ability *b Second element.
* @return int sort code
*/
int sort_skill_abilities(struct skill_ability *a, struct skill_ability *b) {
	ability_data *a_abil, *b_abil;
	
	a_abil = find_ability_by_vnum(a->vnum);
	b_abil = find_ability_by_vnum(b->vnum);
	
	if (a_abil && b_abil) {
		return strcmp(ABIL_NAME(a_abil), ABIL_NAME(b_abil));
	}
	else if (a_abil) {
		return -1;
	}
	else {
		return 1;
	}
}


// Simple vnum sorter for the skill hash
int sort_skills(skill_data *a, skill_data *b) {
	return SKILL_VNUM(a) - SKILL_VNUM(b);
}


// typealphabetic sorter for sorted_skills
int sort_skills_by_data(skill_data *a, skill_data *b) {
	return strcmp(NULLSAFE(SKILL_NAME(a)), NULLSAFE(SKILL_NAME(b)));
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts a skill into the hash table.
*
* @param skill_data *skill The skill data to add to the table.
*/
void add_skill_to_table(skill_data *skill) {
	skill_data *find;
	any_vnum vnum;
	
	if (skill) {
		vnum = SKILL_VNUM(skill);
		HASH_FIND_INT(skill_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(skill_table, vnum, skill);
			HASH_SORT(skill_table, sort_skills);
		}
		
		// sorted table
		HASH_FIND(sorted_hh, sorted_skills, &vnum, sizeof(int), find);
		if (!find) {
			HASH_ADD(sorted_hh, sorted_skills, vnum, sizeof(int), skill);
			HASH_SRT(sorted_hh, sorted_skills, sort_skills_by_data);
		}
	}
}


/**
* Removes a skill from the hash table.
*
* @param skill_data *skill The skill data to remove from the table.
*/
void remove_skill_from_table(skill_data *skill) {
	HASH_DEL(skill_table, skill);
	HASH_DELETE(sorted_hh, sorted_skills, skill);
}


/**
* Initializes a new skill. This clears all memory for it, so set the vnum
* AFTER.
*
* @param skill_data *skill The skill to initialize.
*/
void clear_skill(skill_data *skill) {
	memset((char *) skill, 0, sizeof(skill_data));
	
	SKILL_VNUM(skill) = NOTHING;
	SKILL_MAX_LEVEL(skill) = CLASS_SKILL_CAP;
}


/**
* Duplicates a list of skill abilities, for editing.
*
* @param struct skill_ability *input The head of the list to copy.
* @return struct skill_ability* The copied list.
*/
struct skill_ability *copy_skill_abilities(struct skill_ability *input) {
	struct skill_ability *el, *iter, *list = NULL;
	
	LL_FOREACH(input, iter) {
		CREATE(el, struct skill_ability, 1);
		*el = *iter;
		el->next = NULL;
		LL_APPEND(list, el);
	}
	
	return list;
}


/**
* @param struct skill_ability *list Frees the memory for this list.
*/
void free_skill_abilities(struct skill_ability *list) {
	struct skill_ability *tmp, *next;
	
	LL_FOREACH_SAFE(list, tmp, next) {
		free(tmp);
	}
}


/**
* frees up memory for a skill data item.
*
* See also: olc_delete_skill
*
* @param skill_data *skill The skill data to free.
*/
void free_skill(skill_data *skill) {
	skill_data *proto = find_skill_by_vnum(SKILL_VNUM(skill));
	
	if (SKILL_NAME(skill) && (!proto || SKILL_NAME(skill) != SKILL_NAME(proto))) {
		free(SKILL_NAME(skill));
	}
	if (SKILL_ABBREV(skill) && (!proto || SKILL_ABBREV(skill) != SKILL_ABBREV(proto))) {
		free(SKILL_ABBREV(skill));
	}
	if (SKILL_DESC(skill) && (!proto || SKILL_DESC(skill) != SKILL_DESC(proto))) {
		free(SKILL_DESC(skill));
	}
	if (SKILL_ABILITIES(skill) && (!proto || SKILL_ABILITIES(skill) != SKILL_ABILITIES(proto))) {
		free_skill_abilities(SKILL_ABILITIES(skill));
	}
	
	free(skill);
}


/**
* Read one skill from file.
*
* @param FILE *fl The open .skill file
* @param any_vnum vnum The skill vnum
*/
void parse_skill(FILE *fl, any_vnum vnum) {
	char line[256], error[256], str_in[256];
	struct skill_ability *skabil;
	skill_data *skill, *find;
	int int_in[4];
	
	CREATE(skill, skill_data, 1);
	clear_skill(skill);
	SKILL_VNUM(skill) = vnum;
	
	HASH_FIND_INT(skill_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate skill vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_skill_to_table(skill);
		
	// for error messages
	sprintf(error, "skill vnum %d", vnum);
	
	// lines 1-3: string
	SKILL_NAME(skill) = fread_string(fl, error);
	SKILL_ABBREV(skill) = fread_string(fl, error);
	SKILL_DESC(skill) = fread_string(fl, error);
	
	// line 4: flags
	if (!get_line(fl, line) || sscanf(line, "%s", str_in) != 1) {
		log("SYSERR: Format error in line 4 of %s", error);
		exit(1);
	}
	
	SKILL_FLAGS(skill) = asciiflag_conv(str_in);
		
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'A': {	// ability assignment
				if (sscanf(line, "A %d %d %d", &int_in[0], &int_in[1], &int_in[2]) != 3) {
					log("SYSERR: Format error in A line of %s", error);
					exit(1);
				}
				
				CREATE(skabil, struct skill_ability, 1);
				skabil->vnum = int_in[0];
				skabil->prerequisite = int_in[1];
				skabil->level = int_in[2];
				
				LL_APPEND(SKILL_ABILITIES(skill), skabil);
				break;
			}
			
			case 'L': {	// additional level data
				if (sscanf(line, "L %d %d", &int_in[0], &int_in[1]) != 2) {
					log("SYSERR: Format error in L line of %s", error);
					exit(1);
				}
				
				SKILL_MAX_LEVEL(skill) = int_in[0];
				SKILL_MIN_DROP_LEVEL(skill) = int_in[1];
				break;
			}
			
			// end
			case 'S': {
				return;
			}
			
			default: {
				log("SYSERR: Format error in %s, expecting alphabetic flags", error);
				exit(1);
			}
		}
	}
}


// writes entries in the skill index
void write_skill_index(FILE *fl) {
	skill_data *skill, *next_skill;
	int this, last;
	
	last = NO_WEAR;
	HASH_ITER(hh, skill_table, skill, next_skill) {
		// determine "zone number" by vnum
		this = (int)(SKILL_VNUM(skill) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, SKILL_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one skill in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param skill_data *skill The thing to save.
*/
void write_skill_to_file(FILE *fl, skill_data *skill) {
	struct skill_ability *iter;
	
	if (!fl || !skill) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_skill_to_file called without %s", !fl ? "file" : "skill");
		return;
	}
	
	fprintf(fl, "#%d\n", SKILL_VNUM(skill));
	
	// 1-3: strings
	fprintf(fl, "%s~\n", NULLSAFE(SKILL_NAME(skill)));
	fprintf(fl, "%s~\n", NULLSAFE(SKILL_ABBREV(skill)));
	fprintf(fl, "%s~\n", NULLSAFE(SKILL_DESC(skill)));
	
	// 4: flags
	fprintf(fl, "%s\n", bitv_to_alpha(SKILL_FLAGS(skill)));
	
	// A: abilities
	LL_FOREACH(SKILL_ABILITIES(skill), iter) {
		fprintf(fl, "A %d %d %d\n", iter->vnum, iter->prerequisite, iter->level);
	}
	
	// L: additional level data
	fprintf(fl, "L %d %d\n", SKILL_MAX_LEVEL(skill), SKILL_MIN_DROP_LEVEL(skill));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////


/**
* Creates a new skill entry.
* 
* @param any_vnum vnum The number to create.
* @return skill_data* The new skill's prototype.
*/
skill_data *create_skill_table_entry(any_vnum vnum) {
	skill_data *skill;
	
	// sanity
	if (find_skill_by_vnum(vnum)) {
		log("SYSERR: Attempting to insert skill at existing vnum %d", vnum);
		return find_skill_by_vnum(vnum);
	}
	
	CREATE(skill, skill_data, 1);
	clear_skill(skill);
	SKILL_VNUM(skill) = vnum;
	SKILL_NAME(skill) = str_dup(default_skill_name);
	SKILL_ABBREV(skill) = str_dup(default_skill_abbrev);
	SKILL_DESC(skill) = str_dup(default_skill_desc);
	add_skill_to_table(skill);

	// save index and skill file now
	save_index(DB_BOOT_SKILL);
	save_library_file_for_vnum(DB_BOOT_SKILL, vnum);

	return skill;
}


/**
* WARNING: This function actually deletes a skill.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_skill(char_data *ch, any_vnum vnum) {
	extern bool delete_quest_reward_from_list(struct quest_reward **list, int type, any_vnum vnum);
	extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
	extern bool remove_vnum_from_class_skill_reqs(struct class_skill_req **list, any_vnum vnum);
	
	struct player_skill_data *plsk, *next_plsk;
	struct archetype_skill *arsk, *next_arsk;
	archetype_data *arch, *next_arch;
	ability_data *abil, *next_abil;
	quest_data *quest, *next_quest;
	social_data *soc, *next_soc;
	class_data *cls, *next_cls;
	descriptor_data *desc;
	skill_data *skill;
	char_data *chiter;
	bool found;
	
	if (!(skill = find_skill_by_vnum(vnum))) {
		msg_to_char(ch, "There is no such skill %d.\r\n", vnum);
		return;
	}
	
	// remove it from the hash table first
	remove_skill_from_table(skill);
	
	// remove from live abilities
	HASH_ITER(hh, ability_table, abil, next_abil) {
		if (ABIL_ASSIGNED_SKILL(abil) == skill) {
			ABIL_ASSIGNED_SKILL(abil) = NULL;
			ABIL_SKILL_LEVEL(abil) = 0;
			// this is not saved data, no need to save library files
		}
	}
	
	// remove from archetypes
	HASH_ITER(hh, archetype_table, arch, next_arch) {
		found = FALSE;
		LL_FOREACH_SAFE(GET_ARCH_SKILLS(arch), arsk, next_arsk) {
			if (arsk->skill == vnum) {
				LL_DELETE(GET_ARCH_SKILLS(arch), arsk);
				free(arsk);
				found |= TRUE;
			}
		}
		
		if (found) {
			SET_BIT(GET_ARCH_FLAGS(arch), ARCH_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_ARCH, GET_ARCH_VNUM(arch));
		}
	}
	
	// remove from classes
	HASH_ITER(hh, class_table, cls, next_cls) {
		found = remove_vnum_from_class_skill_reqs(&CLASS_SKILL_REQUIREMENTS(cls), vnum);
		if (found) {
			SET_BIT(CLASS_FLAGS(cls), CLASSF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_CLASS, CLASS_VNUM(cls));
		}
	}
	
	// remove from quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		found = delete_quest_reward_from_list(&QUEST_REWARDS(quest), QR_SET_SKILL, vnum);
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(quest), QR_SKILL_EXP, vnum);
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(quest), QR_SKILL_LEVELS, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_SKILL_LEVEL_OVER, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_SKILL_LEVEL_OVER, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_SKILL_LEVEL_UNDER, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_SKILL_LEVEL_UNDER, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// remove from socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_SKILL_LEVEL_OVER, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_SKILL_LEVEL_UNDER, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// remove from live players
	LL_FOREACH(character_list, chiter) {
		found = FALSE;
		if (IS_NPC(chiter)) {
			continue;
		}
		
		HASH_ITER(hh, GET_SKILL_HASH(chiter), plsk, next_plsk) {
			if (plsk->vnum == vnum) {
				clear_char_abilities(chiter, plsk->vnum);
				HASH_DEL(GET_SKILL_HASH(chiter), plsk);
				free(plsk);
				found = TRUE;
			}
		}
	}
	
	// look for olc editors
	LL_FOREACH(descriptor_list, desc) {
		if (GET_OLC_ARCHETYPE(desc)) {
			found = FALSE;
			LL_FOREACH_SAFE(GET_ARCH_SKILLS(GET_OLC_ARCHETYPE(desc)), arsk, next_arsk) {
				if (arsk->skill == vnum) {
					LL_DELETE(GET_ARCH_SKILLS(GET_OLC_ARCHETYPE(desc)), arsk);
					free(arsk);
					found |= TRUE;
				}
			}
			if (found) {
				msg_to_desc(desc, "A skill has been deleted from the archetype you're editing.\r\n");
			}
		}
		if (GET_OLC_CLASS(desc)) {
			found = remove_vnum_from_class_skill_reqs(&CLASS_SKILL_REQUIREMENTS(GET_OLC_CLASS(desc)), vnum);
			if (found) {
				msg_to_desc(desc, "A skill requirement has been deleted from the class you're editing.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			found = delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_SET_SKILL, vnum);
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_SKILL_EXP, vnum);
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_SKILL_LEVELS, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_SKILL_LEVEL_OVER, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_SKILL_LEVEL_OVER, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_SKILL_LEVEL_UNDER, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_SKILL_LEVEL_UNDER, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "A skill used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_SKILL_LEVEL_OVER, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_SKILL_LEVEL_UNDER, vnum);
		
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A skill required by the social you are editing was deleted.\r\n");
			}
		}
	}

	// save index and skill file now
	save_index(DB_BOOT_SKILL);
	save_library_file_for_vnum(DB_BOOT_SKILL, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted skill %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Skill %d deleted.\r\n", vnum);
	
	free_skill(skill);
}


/**
* Function to save a player's changes to a skill (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_skill(descriptor_data *desc) {
	void read_ability_requirements();
	
	skill_data *proto, *skill = GET_OLC_SKILL(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh, sorted;
	char_data *ch_iter, *next_ch;

	// have a place to save it?
	if (!(proto = find_skill_by_vnum(vnum))) {
		proto = create_skill_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (SKILL_NAME(proto)) {
		free(SKILL_NAME(proto));
	}
	if (SKILL_ABBREV(proto)) {
		free(SKILL_ABBREV(proto));
	}
	if (SKILL_DESC(proto)) {
		free(SKILL_DESC(proto));
	}
	free_skill_abilities(SKILL_ABILITIES(proto));
	
	// sanity
	if (!SKILL_NAME(skill) || !*SKILL_NAME(skill)) {
		if (SKILL_NAME(skill)) {
			free(SKILL_NAME(skill));
		}
		SKILL_NAME(skill) = str_dup(default_skill_name);
	}
	if (!SKILL_ABBREV(skill) || !*SKILL_ABBREV(skill)) {
		if (SKILL_ABBREV(skill)) {
			free(SKILL_ABBREV(skill));
		}
		SKILL_ABBREV(skill) = str_dup(default_skill_abbrev);
	}
	if (!SKILL_DESC(skill) || !*SKILL_DESC(skill)) {
		if (SKILL_DESC(skill)) {
			free(SKILL_DESC(skill));
		}
		SKILL_DESC(skill) = str_dup(default_skill_desc);
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	sorted = proto->sorted_hh;
	*proto = *skill;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	proto->sorted_hh = sorted;
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_SKILL, vnum);

	// ... and update some things
	HASH_SRT(sorted_hh, sorted_skills, sort_skills_by_data);
	read_ability_requirements();
	
	// update all players in case there are new level-0 abilities
	LL_FOREACH_SAFE(character_list, ch_iter, next_ch) {
		if (!IS_NPC(ch_iter)) {
			give_level_zero_abilities(ch_iter);
		}
	}
}


/**
* Creates a copy of a skill, or clears a new one, for editing.
* 
* @param skill_data *input The skill to copy, or NULL to make a new one.
* @return skill_data* The copied skill.
*/
skill_data *setup_olc_skill(skill_data *input) {
	skill_data *new;
	
	CREATE(new, skill_data, 1);
	clear_skill(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		SKILL_NAME(new) = SKILL_NAME(input) ? str_dup(SKILL_NAME(input)) : NULL;
		SKILL_ABBREV(new) = SKILL_ABBREV(input) ? str_dup(SKILL_ABBREV(input)) : NULL;
		SKILL_DESC(new) = SKILL_DESC(input) ? str_dup(SKILL_DESC(input)) : NULL;
		SKILL_ABILITIES(new) = copy_skill_abilities(SKILL_ABILITIES(input));
	}
	else {
		// brand new: some defaults
		SKILL_NAME(new) = str_dup(default_skill_name);
		SKILL_ABBREV(new) = str_dup(default_skill_abbrev);
		SKILL_DESC(new) = str_dup(default_skill_desc);
		SKILL_FLAGS(new) = SKILLF_IN_DEVELOPMENT;
	}
	
	// done
	return new;	
}


 //////////////////////////////////////////////////////////////////////////////
//// SKILL ABILITY DISPLAY ///////////////////////////////////////////////////

struct skad_element {
	char **text;
	int lines;
	struct skad_element *next;	// linked list
};


/**
* Builds one block of text for a hierarchical ability list.
*
* @param struct skill_ability *list The whole list we're showing part of.
* @param struct skill_ability *parent Which parent ability we are showing.
* @param int indent Number of times to indent this row.
* @param struct skad_element **display A pointer to a list of skad elements, for storing the display.
*/
void get_skad_partial(struct skill_ability *list, struct skill_ability *parent, int indent, struct skad_element **display, struct skad_element *skad) {
	char buf[MAX_STRING_LENGTH];
	struct skill_ability *abil;
	
	LL_FOREACH(list, abil) {
		if (!parent && abil->prerequisite != NO_ABIL) {
			continue;	// only showing freestanding abilities here
		}
		if (parent && abil->prerequisite != parent->vnum) {
			continue;	// wrong sub-tree
		}
		
		// have one to display
		if (!skad) {
			CREATE(skad, struct skad_element, 1);
			skad->lines = 0;
			skad->text = NULL;
			LL_APPEND(*display, skad);
		}
		
		snprintf(buf, sizeof(buf), "%*s%s[%d] %-.17s @ %d", (2 * indent), " ", (parent ? "+ " : ""), abil->vnum, get_ability_name_by_vnum(abil->vnum), abil->level);
		
		// append line
		if (skad->lines > 0) {
			RECREATE(skad->text, char*, skad->lines + 1);
		}
		else {
			CREATE(skad->text, char*, 1);
		}
		skad->text[skad->lines] = str_dup(buf);
		++(skad->lines);
		
		// find any dependent abilities
		get_skad_partial(list, abil, indent + 1, display, skad);
		if (!parent) {
			skad = NULL;	// create new block
		}
	}
}


/**
* Builds the two-column display of abilities for a skill.
*
* @param struct skill_ability *list The list to show.
* @param char *save_buffer Text to write the result to.
* @param size_t buflen The max length of save_buffer.
*/
void get_skill_ability_display(struct skill_ability *list, char *save_buffer, size_t buflen) {
	struct skad_element *skad, *mid, *display = NULL;
	char **left_text = NULL, **right_text = NULL;
	int left_lines = 0, right_lines = 0;
	int count, iter, total_lines;
	double half;
	size_t size;
	
	// prepare...
	*save_buffer = '\0';
	size = 0;
	
	// fetch set of columns
	get_skad_partial(list, NULL, 0, &display, NULL);
	
	if (!display) {
		return;	// no work
	}
	
	// determine number of blocks per column
	total_lines = 0;
	LL_FOREACH(display, skad) {
		total_lines += skad->lines;
	}
	
	// determine approximate middle
	mid = NULL;
	count = 0;
	half = total_lines / 2.0;
	LL_FOREACH(display, skad) {
		count += skad->lines;
		
		if ((double)count == half) {
			mid = skad;
			break;
		}
		else if (count > half) {
			mid = skad->next;
			break;
		}
	}
	
	// build left column: move string pointers over to new list
	for (skad = display; skad && skad != mid; skad = skad->next) {
		if (left_lines > 0) {
			RECREATE(left_text, char*, left_lines + skad->lines);
		}
		else {
			CREATE(left_text, char*, skad->lines);
		}
		for (iter = 0; iter < skad->lines; ++iter) {
			left_text[left_lines++] = skad->text[iter];
		}
	}
	
	// build right column: move string pointers over to new list
	for (skad = mid; skad; skad = skad->next) {
		if (right_lines > 0) {
			RECREATE(right_text, char*, right_lines + skad->lines);
		}
		else {
			CREATE(right_text, char*, skad->lines);
		}
		for (iter = 0; iter < skad->lines; ++iter) {
			right_text[right_lines++] = skad->text[iter];
		}
	}
	
	iter = 0;
	while (iter < left_lines || iter < right_lines) {
		size += snprintf(save_buffer + size, buflen - size, " %-38.38s", (iter < left_lines ? left_text[iter] : ""));
		if (iter < right_lines) {
			size += snprintf(save_buffer + size, buflen - size, " %-38.38s\r\n", right_text[iter]);
		}
		else {
			size += snprintf(save_buffer + size, buflen - size, "\r\n");
		}
		
		++iter;
	}
	
	// free all the things
	LL_FOREACH_SAFE(display, skad, mid) {
		free(skad);
	}
	for (iter = 0; iter < left_lines; ++iter) {
		free(left_text[iter]);
	}
	for (iter = 0; iter < right_lines; ++iter) {
		free(right_text[iter]);
	}
	free(left_text);
	free(right_text);
}


 //////////////////////////////////////////////////////////////////////////////
//// MAIN DISPLAYS ///////////////////////////////////////////////////////////

/**
* For vstat.
*
* @param char_data *ch The player requesting stats.
* @param skill_data *skill The skill to display.
*/
void do_stat_skill(char_data *ch, skill_data *skill) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	struct skill_ability *skab;
	size_t size;
	int total;
	
	if (!skill) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \tc%s\t0, Abbrev: \tc%s\t0\r\n", SKILL_VNUM(skill), SKILL_NAME(skill), SKILL_ABBREV(skill));
	
	size += snprintf(buf + size, sizeof(buf) - size, "Description: %s\r\n", SKILL_DESC(skill));
	
	size += snprintf(buf + size, sizeof(buf) - size, "Minimum drop level: [\tc%d\t0], Maximum level: [\tc%d\t0]\r\n", SKILL_MIN_DROP_LEVEL(skill), SKILL_MAX_LEVEL(skill));
	
	sprintbit(SKILL_FLAGS(skill), skill_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	LL_COUNT(SKILL_ABILITIES(skill), skab, total);
	size += snprintf(buf + size, sizeof(buf) - size, "Simplified skill tree: (%d total)\r\n", total);
	get_skill_ability_display(SKILL_ABILITIES(skill), part, sizeof(part));
	if (*part) {
		size += snprintf(buf + size, sizeof(buf) - size, "%s", part);
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* This is the main recipe display for skill OLC. It displays the user's
* currently-edited skill.
*
* @param char_data *ch The person who is editing a skill and will see its display.
*/
void olc_show_skill(char_data *ch) {
	skill_data *skill = GET_OLC_SKILL(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	struct skill_ability *skab;
	int total;
	
	if (!skill) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[\tc%d\t0] \tc%s\t0\r\n", GET_OLC_VNUM(ch->desc), !find_skill_by_vnum(SKILL_VNUM(skill)) ? "new skill" : get_skill_name_by_vnum(SKILL_VNUM(skill)));
	sprintf(buf + strlen(buf), "<\tyname\t0> %s\r\n", NULLSAFE(SKILL_NAME(skill)));
	sprintf(buf + strlen(buf), "<\tyabbrev\t0> %s\r\n", NULLSAFE(SKILL_ABBREV(skill)));
	sprintf(buf + strlen(buf), "<\tydescription\t0> %s\r\n", NULLSAFE(SKILL_DESC(skill)));
	
	sprintbit(SKILL_FLAGS(skill), skill_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<\tyflags\t0> %s\r\n", lbuf);
	
	sprintf(buf + strlen(buf), "<\tymaxlevel\t0> %d\r\n", SKILL_MAX_LEVEL(skill));
	sprintf(buf + strlen(buf), "<\tymindrop\t0> %d\r\n", SKILL_MIN_DROP_LEVEL(skill));
	
	LL_COUNT(SKILL_ABILITIES(skill), skab, total);
	sprintf(buf + strlen(buf), "<\tytree\t0> %d %s\r\n", total, total == 1 ? "ability" : "abilities");
	get_skill_ability_display(SKILL_ABILITIES(skill), lbuf, sizeof(lbuf));
	if (*lbuf) {
		sprintf(buf + strlen(buf), "%s", lbuf);
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the skill db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_skill(char *searchname, char_data *ch) {
	skill_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, skill_table, iter, next_iter) {
		if (multi_isname(searchname, SKILL_NAME(iter)) || is_abbrev(searchname, SKILL_ABBREV(iter)) || multi_isname(searchname, SKILL_DESC(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, SKILL_VNUM(iter), SKILL_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(skilledit_abbrev) {
	skill_data *skill = GET_OLC_SKILL(ch->desc);
	
	if (color_strlen(argument) != 3) {
		msg_to_char(ch, "Skill abbreviations must be 3 letters.\r\n");
	}
	else if (color_code_length(argument) > 0) {
		msg_to_char(ch, "Skill abbreviations may not contain color codes.\r\n");
	}
	else {
		olc_process_string(ch, argument, "abbreviation", &SKILL_ABBREV(skill));
	}
}


OLC_MODULE(skilledit_description) {
	skill_data *skill = GET_OLC_SKILL(ch->desc);
	olc_process_string(ch, argument, "description", &SKILL_DESC(skill));
}


OLC_MODULE(skilledit_flags) {
	skill_data *skill = GET_OLC_SKILL(ch->desc);
	bool had_indev = IS_SET(SKILL_FLAGS(skill), SKILLF_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	SKILL_FLAGS(skill) = olc_process_flag(ch, argument, "skill", "flags", skill_flags, SKILL_FLAGS(skill));
	
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !IS_SET(SKILL_FLAGS(skill), SKILLF_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(SKILL_FLAGS(skill), SKILLF_IN_DEVELOPMENT);
	}
}


OLC_MODULE(skilledit_maxlevel) {
	skill_data *skill = GET_OLC_SKILL(ch->desc);
	SKILL_MAX_LEVEL(skill) = olc_process_number(ch, argument, "maximum level", "maxlevel", 1, CLASS_SKILL_CAP, SKILL_MAX_LEVEL(skill));
}


OLC_MODULE(skilledit_mindrop) {
	skill_data *skill = GET_OLC_SKILL(ch->desc);
	SKILL_MIN_DROP_LEVEL(skill) = olc_process_number(ch, argument, "minimum drop level", "mindrop", 0, CLASS_SKILL_CAP, SKILL_MIN_DROP_LEVEL(skill));
}


OLC_MODULE(skilledit_name) {
	skill_data *skill = GET_OLC_SKILL(ch->desc);
	olc_process_string(ch, argument, "name", &SKILL_NAME(skill));
}


OLC_MODULE(skilledit_tree) {
	extern ability_data *find_ability_on_skill(char *name, skill_data *skill);

	skill_data *skill = GET_OLC_SKILL(ch->desc);
	char cmd_arg[MAX_INPUT_LENGTH], abil_arg[MAX_INPUT_LENGTH], sub_arg[MAX_INPUT_LENGTH], req_arg[MAX_INPUT_LENGTH];
	struct skill_ability *skab, *next_skab, *change;
	ability_data *abil = NULL, *requires = NULL;
	bool all = FALSE, found, found_prq;
	int level;
	
	argument = any_one_arg(argument, cmd_arg);
	argument = any_one_word(argument, abil_arg);
	argument = any_one_arg(argument, sub_arg);	// may be level or type
	argument = any_one_word(argument, req_arg);	// may be requires ability or "new value"
	
	// check for "all" arg
	if (!str_cmp(abil_arg, "all")) {
		all = TRUE;
	}
	
	if (is_abbrev(cmd_arg, "add")) {
		if (!*abil_arg) {
			msg_to_char(ch, "Add what ability?\r\n");
		}
		else if (!(abil = find_ability(abil_arg))) {
			msg_to_char(ch, "Unknown ability '%s'.\r\n", abil_arg);
		}
		else if (is_class_ability(abil)) {
			msg_to_char(ch, "You can't assign %s to this skill because it's already assigned to a class.\r\n", ABIL_NAME(abil));
		}
		else if (ABIL_ASSIGNED_SKILL(abil) && SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil)) != SKILL_VNUM(skill)) {
			msg_to_char(ch, "You can't assign %s to this skill because it's already assigned to [%d] %s.\r\n", ABIL_NAME(abil), SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil)), SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)));
		}
		else if (!*sub_arg || !isdigit(*sub_arg) || (level = atoi(sub_arg)) < 0) {
			msg_to_char(ch, "Add the ability at what level?\r\n");
		}
		else if (*req_arg && str_cmp(req_arg, "none") && !(requires = find_ability(req_arg))) {
			msg_to_char(ch, "Invalid pre-requisite ability '%s'.\r\n", req_arg);
		}
		else if (abil == requires) {
			msg_to_char(ch, "It cannot require itself.\r\n");
		}
		else {
			// this does some validation
			found = found_prq = FALSE;
			change = NULL;
			LL_FOREACH(SKILL_ABILITIES(skill), skab) {
				if (skab->vnum == ABIL_VNUM(abil)) {
					change = skab;
					found = TRUE;
				}
				if (requires && skab->vnum == ABIL_VNUM(requires)) {
					found_prq = TRUE;
				}
			}
			
			if (requires && !found_prq) {
				msg_to_char(ch, "You can't add a prerequisite that isn't assigned to this skill.\r\n");
				return;
			}
			
			if (found && change) {
				change->level = level;
				change->prerequisite = requires ? ABIL_VNUM(requires) : NOTHING;
			}
			else {
				CREATE(skab, struct skill_ability, 1);
				skab->vnum = ABIL_VNUM(abil);
				skab->level = level;
				skab->prerequisite = requires ? ABIL_VNUM(requires) : NOTHING;
				LL_APPEND(SKILL_ABILITIES(skill), skab);
			}
			
			msg_to_char(ch, "You assign %s at level %d", ABIL_NAME(abil), level);
			if (requires) {
				msg_to_char(ch, " (branching from %s).\r\n", ABIL_NAME(requires));
			}
			else {
				msg_to_char(ch, ".\r\n");
			}
			
			// in case
			LL_SORT(SKILL_ABILITIES(skill), sort_skill_abilities);
		}
	}
	else if (is_abbrev(cmd_arg, "remove")) {
		if (!all && !*abil_arg) {
			msg_to_char(ch, "Remove which ability?\r\n");
			return;
		}
		if (!all && !(abil = find_ability_on_skill(abil_arg, skill))) {
			msg_to_char(ch, "There is no such ability on this skill.\r\n");
			return;
		}
		
		found = found_prq = FALSE;
		LL_FOREACH_SAFE(SKILL_ABILITIES(skill), skab, next_skab) {
			if (all || (abil && skab->vnum == ABIL_VNUM(abil))) {
				LL_DELETE(SKILL_ABILITIES(skill), skab);
				free(skab);
				found = TRUE;
			}
			else if (abil && skab->prerequisite == ABIL_VNUM(abil)) {
				LL_DELETE(SKILL_ABILITIES(skill), skab);
				free(skab);
				found = found_prq = TRUE;
			}
		}
		
		if (!found) {
			msg_to_char(ch, "Couldn't find anything to remove.\r\n");
		}
		else if (all) {
			msg_to_char(ch, "You remove all abilities.\r\n");
		}
		else {
			msg_to_char(ch, "You remove the %s ability%s.\r\n", ABIL_NAME(abil), found_prq ? " (and things that required it)" : "");
		}
	}
	else if (is_abbrev(cmd_arg, "change")) {
		if (!*abil_arg) {
			msg_to_char(ch, "Change which ability?\r\n");
		}
		else if (!(abil = find_ability_on_skill(abil_arg, skill))) {
			msg_to_char(ch, "There is no such ability on this skill.\r\n");
		}
		else if (is_abbrev(sub_arg, "level")) {
			if (!*argument || !isdigit(*argument) || (level = atoi(argument)) < 0) {
				msg_to_char(ch, "Set it to what level?\r\n");
				return;
			}
			
			found = FALSE;
			LL_FOREACH(SKILL_ABILITIES(skill), skab) {
				if (skab->vnum == ABIL_VNUM(abil)) {
					skab->level = level;
					found = TRUE;
				}
			}
			
			if (found) {
				msg_to_char(ch, "You change %s to level %d.\r\n", ABIL_NAME(abil), level);
			}
			else {
				msg_to_char(ch, "%s is not assigned to this skill.\r\n", ABIL_NAME(abil));
			}
		}
		else if (is_abbrev(sub_arg, "requires") || is_abbrev(sub_arg, "requirement") || is_abbrev(sub_arg, "prerequisite")) {
			if (!*argument) {
				msg_to_char(ch, "Require which ability (or none)?\r\n");
			}
			else if (str_cmp(argument, "none") && !(requires = find_ability(argument))) {
				msg_to_char(ch, "Invalid pre-requisite ability '%s'.\r\n", argument);
			}
			else {
				found = FALSE;
				found_prq = (requires == NULL);
				change = NULL;
				LL_FOREACH(SKILL_ABILITIES(skill), skab) {
					if (skab->vnum == ABIL_VNUM(abil)) {
						change = skab;
						found = TRUE;
					}
					else if (requires && skab->vnum == ABIL_VNUM(requires)) {
						found_prq = TRUE;
					}
				}
				
				if (!found || !change) {
					msg_to_char(ch, "%s is not assigned to this skill.\r\n", ABIL_NAME(abil));
				}
				else if (!found_prq) {
					msg_to_char(ch, "You can't require an ability that isn't assigned to this skill.\r\n");
				}
				else {
					change->prerequisite = requires ? ABIL_VNUM(requires) : -1;
					if (requires) {
						msg_to_char(ch, "%s now requires the %s ability.\r\n", ABIL_NAME(abil), ABIL_NAME(requires));
					}
					else {
						msg_to_char(ch, "%s no longer requires a prerequisite.\r\n", ABIL_NAME(abil));
					}
				}
			}
		}
		else {
			msg_to_char(ch, "You can change the level or requirement.\r\n");
		}
		
		// in case
		LL_SORT(SKILL_ABILITIES(skill), sort_skill_abilities);
	}
	else {
		msg_to_char(ch, "Usage: tree add <ability> <level> [requires ability]\r\n");
		msg_to_char(ch, "       tree remove <ability | all>\r\n");
		msg_to_char(ch, "       tree change <ability> <level | requires> <new value>\r\n");
	}
}
