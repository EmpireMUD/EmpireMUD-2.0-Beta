/* ************************************************************************
*   File: fight.c                                         EmpireMUD 2.0b5 *
*  Usage: Combat system                                                   *
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
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Getters / Helpers
*   Combat Meters
*   Player-Killed-By
*   Death and Corpses
*   Guard Towers
*   Messaging
*   Restrictors
*   Setters / Doers
*   Combat Engine: One Attack
*   Combat Engine: Rounds
*/


// external vars
extern struct message_list fight_messages[MAX_MESSAGES];
extern const double hit_per_dex;
extern struct character_size_data size_data[];

// external funcs
ACMD(do_flee);
bool check_scaling(char_data *mob, char_data *based_on);
extern struct resource_data *combine_resources(struct resource_data *combine_a, struct resource_data *combine_b);
extern int determine_best_scale_level(char_data *ch, bool check_group);
void end_pursuit(char_data *ch, char_data *target);
void scale_item_to_level(obj_data *obj, int level);

// locals
int damage(char_data *ch, char_data *victim, int dam, int attacktype, byte damtype);
void drop_loot(char_data *mob, char_data *killer);
int get_block_rating(char_data *ch, bool can_gain_skill);
int get_dodge_modifier(char_data *ch, char_data *attacker, bool can_gain_skill);
int get_to_hit(char_data *ch, char_data *victim, bool off_hand, bool can_gain_skill);
double get_weapon_speed(obj_data *weapon);
void heal(char_data *ch, char_data *vict, int amount);
int hit(char_data *ch, char_data *victim, obj_data *weapon, bool combat_round);
extern int lock_instance_level(room_data *room, int level);
obj_data *make_corpse(char_data *ch);
void perform_execute(char_data *ch, char_data *victim, int attacktype, int damtype);
void trigger_distrust_from_hostile(char_data *ch, empire_data *emp);


 //////////////////////////////////////////////////////////////////////////////
//// GETTERS / HELPERS ///////////////////////////////////////////////////////

/**
* Determine if ch blocks the attacker.
*
* @param char_data *ch The blocker.
* @param char_data *attacker The attacker.
* @param bool can_gain_skill Pass TRUE to do skillups or FALSE to just get info.
* @return bool TRUE if the block succeeds, or FALSE if not.
*/
bool check_block(char_data *ch, char_data *attacker, bool can_gain_skill) {
	obj_data *shield = GET_EQ(ch, WEAR_HOLD);
	double chance, rating, target;
	int level;
	
	int max_block = 50;	// never pass this value
	
	// must have a shield and Shield Block
	if (!shield || !IS_SHIELD(shield) || (!IS_NPC(ch) && !has_player_tech(ch, PTECH_BLOCK))) {
		return FALSE;
	}
	
	rating = get_block_rating(ch, can_gain_skill && can_gain_exp_from(ch, attacker));
	
	// penalty for blind/dark
	if (attacker && !CAN_SEE(ch, attacker)) {
		rating -= 50;
	}
	
	// block cap and target check
	level = attacker ? get_approximate_level(attacker) : get_approximate_level(ch);
	target = level * 0.5;
	if (attacker && MOB_FLAGGED(attacker, MOB_HARD)) {
		target *= 1.1;
	}
	if (attacker && MOB_FLAGGED(attacker, MOB_GROUP)) {
		target *= 1.3;
	}
	
	// Block cap is when rating == target
	chance = MIN(max_block, max_block + rating - target);
	
	return (number(1, 100) <= chance);
}


/**
* Cancels combat for a character if they or their target have certain flags
* that should make combat impossible.
*
* @param char_data *ch The fighter.
* @param char_data *victim The victim.
* @return bool TRUE if it's ok to fight; FALSE to stop.
*/
bool check_can_still_fight(char_data *ch, char_data *victim) {
	if (!ch || !victim) {
		return FALSE;
	}
	
	if (AFF_FLAGGED(victim, AFF_NO_ATTACK | AFF_EARTHMELD)) {
		return FALSE;
	}
	if (AFF_FLAGGED(ch, AFF_NO_ATTACK | AFF_EARTHMELD)) {
		return FALSE;
	}
	
	return TRUE;
}


/**
* Computes whether one person hits the other, or not, based on their to-hit
* and dodge ratings.
*
* @param char_data *attacker The person attacking.
* @param char_data *victim The target.
* @param bool off_hand Whether or not to apply the off-hand penalty to the attacker.
* @return bool TRUE if the attack hits, or FALSE if not.
*/
bool check_hit_vs_dodge(char_data *attacker, char_data *victim, bool off_hand) {
	double tohit, max, min, chance;
	
	int level_tolerance = 50;	// must be within X levels to get minimum hit chance
	int min_npc_to_hit = 25;	// min chance an NPC will hit the target
	int min_pc_to_hit = 5;	// min chance a player will hit the target
	
	// safety
	if (!attacker || !victim) {
		return FALSE;
	}
	
	if (!AWAKE(victim)) {
		// not awake: always hit
		return TRUE;
	}
	
	// modify caps based on abilities/roles
	if (!IS_NPC(victim) || !MOB_FLAGGED(victim, MOB_HARD | MOB_GROUP)) {
		if (IS_NPC(victim) || (GET_CLASS_ROLE(victim) != ROLE_TANK && (GET_CLASS_ROLE(victim) != ROLE_SOLO || !check_solo_role(victim)))) {
			min_npc_to_hit += 20;	// higher hit min if not in tank/solo-role
			min_pc_to_hit += 20;
		}
		if (IS_NPC(victim) || !has_player_tech(victim, PTECH_DODGE_CAP)) {
			min_npc_to_hit += 20;	// higher hit min if lacking dodge-cap tech
			min_pc_to_hit += 20;
		}
	}
	
	if (IS_NPC(attacker)) {
		// mob/player dodging a mob
		// NOTE: this currently uses two different formulae in order to provide a real dodge cap for tanks vs mobs
		chance = get_to_hit(attacker, victim, off_hand, TRUE) - get_dodge_modifier(victim, attacker, TRUE);
		
		// limits IF the character is at least close in level
		if (get_approximate_level(attacker) >= (get_approximate_level(victim) - level_tolerance)) {
			min = IS_NPC(attacker) ? min_npc_to_hit : min_pc_to_hit;
			chance = MAX(chance, min);
		}
		
		return (chance >= number(1, 100));
	}
	else {
		// mob/player dodging a player
		// NOTE: this version is mainly to make it easier for players to hit things
		max = get_dodge_modifier(victim, attacker, TRUE) + 100;	// hit must be higher than dodge by this much for a perfect score
		tohit = get_to_hit(attacker, victim, off_hand, TRUE);
	
		// limits IF the character is at least close in level
		if (get_approximate_level(attacker) >= (get_approximate_level(victim) - level_tolerance)) {
			min = IS_NPC(attacker) ? min_npc_to_hit : min_pc_to_hit;
		}
		else {
			min = 0;
		}
	
		// real chance to hit is what % chance is of max (ensure max is not less than 1)
		chance = tohit * 100 / MAX(1, max);
		return (chance >= (number(1, 100) - min));
	}
}


/**
* Determines what TYPE_ a character is actually using.
* 
* @param char_data *ch The character attacking.
* @param obj_data *weapon Optional: a weapon to use.
* @return int A TYPE_ value.
*/
int get_attack_type(char_data *ch, obj_data *weapon) {
	int w_type = TYPE_HIT;

	// always prefer weapon
	if (weapon && IS_WEAPON(weapon) && !AFF_FLAGGED(ch, AFF_DISARM)) {
		w_type = GET_WEAPON_TYPE(weapon);
	}
	else {
		// in order of priority
		if (AFF_FLAGGED(ch, AFF_CLAWS)) {
			w_type = TYPE_VAMPIRE_CLAWS;
		}
		else if (IS_MORPHED(ch)) {
			w_type = MORPH_ATTACK_TYPE(GET_MORPH(ch));
		}
		else if (IS_NPC(ch) && (MOB_ATTACK_TYPE(ch) != 0) && !AFF_FLAGGED(ch, AFF_DISARM)) {
			w_type = MOB_ATTACK_TYPE(ch);
		}
		else if (IS_NPC(ch) && (MOB_ATTACK_TYPE(ch) != 0) && AFF_FLAGGED(ch, AFF_DISARM)) {
			// disarmed mob
			if (attack_hit_info[MOB_ATTACK_TYPE(ch)].damage_type == DAM_MAGICAL) {
				w_type = TYPE_MANA_BLAST;
			}
			else {
				w_type = TYPE_HIT;
			}
		}
		else if (AFF_FLAGGED(ch, AFF_DISARM) && weapon && IS_WEAPON(weapon) && attack_hit_info[GET_WEAPON_TYPE(weapon)].damage_type == DAM_MAGICAL) {
			w_type = TYPE_MANA_BLAST;
		}
		else {
			w_type = TYPE_HIT;
		}
	}
	
	return w_type;
}


/**
* Gets the damage-per-second of a weapon before adding int/str.
*
* @param obj_data *weapon
* @return double The base damage per second of the weapon
*/
double get_base_dps(obj_data *weapon) {
	int damage;
	double speed;
	
	if (!weapon) {
		return 0.0;
	}
	
	if (IS_WEAPON(weapon)) {
		damage = GET_WEAPON_DAMAGE_BONUS(weapon);
		speed = get_weapon_speed(weapon);
		return (double)damage / speed;
	}
	else if (IS_MISSILE_WEAPON(weapon)) {
		damage = GET_MISSILE_WEAPON_DAMAGE(weapon);
		speed = get_weapon_speed(weapon);
		return (double)damage / speed;
	}
	else {
		return 0.0;
	}
}


/**
* Determine ch's chance to block (0-100).
*
* @param char_data *ch The blocker.
* @param bool can_gain_skill Pass TRUE to do skillups or FALSE to just get info.
* @return int The total block rating.
*/
int get_block_rating(char_data *ch, bool can_gain_skill) {
	double rating = 0.0;
	
	double quick_block_base = 10.0;
	double quick_block_scale = 0.1;	// % of level
	
	rating = GET_BLOCK(ch);
	
	// quick block procs to add 10%
	if (has_ability(ch, ABIL_QUICK_BLOCK)) {
		if (IS_CLASS_ABILITY(ch, ABIL_QUICK_BLOCK)) {
			rating += MAX(quick_block_base, get_approximate_level(ch) * quick_block_scale);
		}
		else {
			rating += quick_block_base;
		}
	}
	
	if (can_gain_skill) {
		gain_player_tech_exp(ch, PTECH_BLOCK, 2);
		gain_ability_exp(ch, ABIL_QUICK_BLOCK, 2);
	}
		
	return rating;
}


/**
* The player's basic speed -- just weapon or attack type.
*
* Do NOT do gain_skill_exp or skill_checks in this function. It is called every
* 0.1 seconds to determine when you act, and random modifiers will not work
* well here.
*
* @param char_data *ch the person whose speed to get
* @param int pos Which position to check (WEAR_WIELD, WEAR_HOLD, WEAR_RANGED)
* @return double The basic speed for the character.
*/
double get_base_speed(char_data *ch, int pos) {
	obj_data *weapon = GET_EQ(ch, pos);
	double base = 3.0;
	int w_type;
	
	w_type = get_attack_type(ch, weapon);
	
	if (weapon) {
		base = get_weapon_speed(weapon);
	}
	else {
		// basic speed
		base = attack_hit_info[w_type].speed[SPD_NORMAL];
	}
	
	return base;
}


/**
* Computes current real combat speed with abilities, affects, etc.
*
* Do NOT do gain_skill_exp or skill_checks in this function. It is called every
* 0.1 seconds to determine when you act, and random modifiers will not work
* well here.
*
* @param char_data *ch the person whose speed to get
* @param int pos Which position to check (WEAR_WIELD, WEAR_HOLD, WEAR_RANGED)
* @return double Get the composite combat speed for that slot.
*/
double get_combat_speed(char_data *ch, int pos) {
	obj_data *weapon = GET_EQ(ch, pos);
	double base = get_base_speed(ch, pos);
	
	double finesse[] = { 0.9, 0.85, 0.8 };
	double quick_draw[] = { 0.75, 0.75, 0.65 };
	
	// ability mods: player only
	if (!IS_NPC(ch) && weapon) {
		if (has_ability(ch, ABIL_FINESSE) && !IS_MISSILE_WEAPON(weapon)) {
			base *= CHOOSE_BY_ABILITY_LEVEL(finesse, ch, ABIL_FINESSE);
		}
		if (has_ability(ch, ABIL_QUICK_DRAW) && IS_MISSILE_WEAPON(weapon)) {
			base *= CHOOSE_BY_ABILITY_LEVEL(quick_draw, ch, ABIL_QUICK_DRAW);
		}
	}
	
	// affect changes
	if (AFF_FLAGGED(ch, AFF_HASTE)) {
		base *= 0.9;
	}
	if (AFF_FLAGGED(ch, AFF_SLOW)) {
		base *= 1.2;
	}
	
	// wits: it gets .1 second faster for every 4 wits
	if (!has_player_tech(ch, PTECH_FASTCASTING)) {
		base *= (1.0 - (0.025 * GET_WITS(ch)));
	}
	// round to .1 seconds
	base *= 10.0;
	base += 0.5;
	base = (double)((int)base);	// poor-man's floor()
	base /= 10.0;
	
	return base < 0.1 ? 0.1 : base;
}


/**
* This is subtracted from get_to_hit (which is 0-100, or more).
*
* @param char_data *ch The dodger.
* @param char_data *attacker The attacker, if any.
* @param bool can_gain_skill Only gains skill if TRUE, otherwise this is just informative.
* @return int The total dodge %.
*/
int get_dodge_modifier(char_data *ch, char_data *attacker, bool can_gain_skill) {
	double base, refl = 0.0;
	
	// no default dodge amount
	base = GET_DODGE(ch);
	
	// dexterity (balances to-hit dexterity)
	base += GET_DEXTERITY(ch) * hit_per_dex;
	
	// skills
	if (has_ability(ch, ABIL_REFLEXES)) {
		if (IS_CLASS_ABILITY(ch, ABIL_REFLEXES)) {
			refl = MAX(10.0, GET_COMPUTED_LEVEL(ch) * 0.1);
		}
		else if (IS_SPECIALTY_ABILITY(ch, ABIL_REFLEXES)) {
			refl = MAX(10.0, GET_COMPUTED_LEVEL(ch) * 0.05);
		}
		else {
			refl = 10.0;
		}
		
		base += refl;
		
		if (can_gain_skill && can_gain_exp_from(ch, attacker)) {
			gain_ability_exp(ch, ABIL_REFLEXES, 2);
		}
	}
	
	// npc
	if (IS_NPC(ch)) {
		base += MOB_TO_DODGE(ch);
	}
	
	// blind penalty
	if (attacker && !CAN_SEE(ch, attacker)) {
		base -= 50;
	}
	
	return (int) base;
}


/**
* Total to-hit value for a character. Final hit chance will subtract opponent's
* dodge for a number that is (ideally) 1-100, then the player rolls.
* 
* @param char_data *ch The hitter.
* @param char_adta *victim The victim (if any).
* @param bool off_hand If TRUE, penalizes to-hit due to off-hand item.
* @param bool can_gain_skill If FALSE, only fetches this as information.
* @return int The hit %.
*/
int get_to_hit(char_data *ch, char_data *victim, bool off_hand, bool can_gain_skill) {
	extern const int base_hit_chance;
	
	double base_chance, spar = 0.0;
	
	// starting value
	base_chance = base_hit_chance + GET_TO_HIT(ch);
	
	// add dexterity (will be counter-balanced by dodge dexterity)
	base_chance += GET_DEXTERITY(ch) * hit_per_dex;
	
	// skills: sparring
	if (has_ability(ch, ABIL_SPARRING)) {
		if (IS_CLASS_ABILITY(ch, ABIL_SPARRING)) {
			spar = MAX(10.0, GET_COMPUTED_LEVEL(ch) * 0.1);
		}
		else if (IS_SPECIALTY_ABILITY(ch, ABIL_SPARRING)) {
			spar = MAX(10.0, GET_COMPUTED_LEVEL(ch) * 0.05);
		}
		else {
			spar = 10.0;
		}
		
		base_chance += spar;
		
		if (can_gain_skill && can_gain_exp_from(ch, victim)) {
			gain_ability_exp(ch, ABIL_SPARRING, 2);
		}
	}
	
	// penalty
	if (off_hand) {
		base_chance -= 50;
	}
	
	// npc -- add raw hit bonus
	if (IS_NPC(ch)) {
		base_chance += MOB_TO_HIT(ch);
	}
	
	// blind/dark penalty
	if (victim && !CAN_SEE(ch, victim)) {
		base_chance -= 50;
	}
	
	return (int) base_chance;
}


/**
* @param obj_data *weapon
* @return double The speed of the weapon (in secs between attacks)
*/
double get_weapon_speed(obj_data *weapon) {
	int spd;
	double speed;

	// determine speed
	spd = SPD_NORMAL;
	if (OBJ_FLAGGED(weapon, OBJ_SLOW)) {
		spd = SPD_SLOW;
	}
	else if (OBJ_FLAGGED(weapon, OBJ_FAST)) {
		spd = SPD_FAST;
	}

	if (IS_WEAPON(weapon)) {
		speed = attack_hit_info[GET_WEAPON_TYPE(weapon)].speed[spd];
	}
	else if (IS_MISSILE_WEAPON(weapon)) {
		speed = attack_hit_info[GET_MISSILE_WEAPON_TYPE(weapon)].speed[spd];
	}
	else {
		speed = 2.0;
	}
	
	return speed;
}


/**
* This functions tries to determine if, in the current fight, a character
* (frenemy) is an ally of the actor (ch). Characters who fail this test may
* be enemies, or they may be neutral (not involved in this fight).
*
* @param char_data *ch The actor.
* @param char_data *frenemy The potential ally.
* @return bool TRUE if it's an ally of ch, or FALSE.
*/
bool is_fight_ally(char_data *ch, char_data *frenemy) {
	char_data *fighting = FIGHTING(frenemy), *ch_iter, *fr_iter;
	
	// one of them is fighting the other
	if (fighting == ch || FIGHTING(ch) == frenemy) {
		return FALSE;
	}
	
	// check "master" tree up both sides == people are allies if ch (or any of ch's masters) are the same as frenemy (or any of frenemy's masters)
	ch_iter = ch;
	while (ch_iter) {
		fr_iter = frenemy;
		while (fr_iter) {
			if (ch_iter == fr_iter) {
				// self is ally!
				return TRUE;
			}
			
			if (FIGHTING(fr_iter) != fr_iter->master) {
				fr_iter = fr_iter->master;
			}
			else {
				break;	// hit someone they are fighting AND following
			}
		}
		
		if (FIGHTING(ch_iter) != ch_iter->master) {
			ch_iter = ch_iter->master;
		}
		else {
			break;	// hit someone they are fighting AND following
		}
	}
	
	if (fighting) {
		// self
		if (frenemy == ch) {
			return TRUE;
		}
		// frenemy is a follower of ch
		if (frenemy->master == ch) {
			return TRUE;
		}
		// frenemy is ch's leader
		if (frenemy == ch->master && FIGHTING(ch) != frenemy) {
			return TRUE;
		}
		// frenemy and ch follow the same master
		if (ch->master != NULL && frenemy->master == ch->master && FIGHTING(ch) != frenemy->master && FIGHTING(frenemy) != ch->master) {
			return TRUE;
		}
		// frenemy and ch are fighting the same thing
		if (fighting == FIGHTING(ch)) {
			return TRUE;
		}
	}
	
	// possibly not even fighting
	return FALSE;
}


/**
* This attempts to determine if, in the current fight, a character (frenemy)
* is an enemy of the actor (ch).
*
* @param char_data *ch The actor.
* @param char_data *frenemy The potential enemy.
* @return bool TRUE if it's an enemy of ch, or FALSE.
*/
bool is_fight_enemy(char_data *ch, char_data *frenemy) {
	char_data *fighting = FIGHTING(frenemy);
	
	// nope
	if (ch == frenemy) {
		return FALSE;
	}
	
	if (fighting) {
		// ch is fighting frenemy
		if (FIGHTING(ch) == frenemy) {
			return TRUE;
		}
		// frenemy is fighting ch
		if (fighting == ch) {
			return TRUE;
		}
		// frenemy is fighting a follower of ch
		if (fighting->master == ch) {
			return TRUE;
		}
		// frenemy is fighting ch's master
		if (fighting == ch->master) {
			return TRUE;
		}
		// frenemy is fighting a follower of ch's master
		if (ch->master != NULL && fighting->master == ch->master) {
			return TRUE;
		}
	}
	
	// possibly not even fighting
	return FALSE;
}


/**
* Determines if a character is in combat in any way. This includes when someone
* is hitting them, even if they aren't hitting back.
*
* @param char_data *ch The character to check.
* @return bool TRUE if he is in combat, or FALSE if not.
*/
bool is_fighting(char_data *ch) {
	char_data *iter;
	
	if (FIGHTING(ch)) {
		return TRUE;
	}
	
	for (iter = ROOM_PEOPLE(IN_ROOM(ch)); iter; iter = iter->next_in_room) {
		if (iter != ch && FIGHTING(iter) == ch) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* Sets an object, its next-content, and all its own contents as last owned
* by the person in question.
*
* @param obj_data *obj The object to set.
* @param int idnum The last-owner-id.
* @param empire_data *emp The last-empire-id.
*/
static void recursive_loot_set(obj_data *obj, int idnum, empire_data *emp) {
	if (obj) {
		obj->last_owner_id = idnum;
		obj->last_empire_id = emp ? EMPIRE_VNUM(emp) : NOTHING;
	
		recursive_loot_set(obj->next_content, idnum, emp);
		recursive_loot_set(obj->contains, idnum, emp);
	}
}


/**
* Applies skills and resistance to damage, if applicable. This can be used for any
* function but is applied inside of damage() already.
*
* @param int dam The original damage.
* @param char_data *victim The person who is taking damage (may gain skills).
* @param char_data *attacker The person dealing the damage (optional).
* @param int damtype The DAM_x type of damage.
*/
int reduce_damage_from_skills(int dam, char_data *victim, char_data *attacker, int damtype) {
	extern bool check_blood_fortitude(char_data *ch, bool can_gain_skill);
	
	bool self = (!attacker || attacker == victim);
	int max_resist;
	double resist_prc, use_resist;
	
	if (!self) {
		if (has_ability(victim, ABIL_NOBLE_BEARING) && check_solo_role(victim)) {
			dam -= GET_GREATNESS(victim);
			gain_ability_exp(victim, ABIL_NOBLE_BEARING, 15);
		}
		
		// damage reduction (usually from armor/spells)
		if (attacker) {
			max_resist = get_approximate_level(attacker) / 2;
			use_resist = 0;
			
			if (damtype == DAM_PHYSICAL || damtype == DAM_FIRE || (damtype == DAM_POISON && has_ability(victim, ABIL_RESIST_POISON))) {
				use_resist = GET_RESIST_PHYSICAL(victim);
			}
			else if (damtype == DAM_MAGICAL) {
				use_resist = GET_RESIST_MAGICAL(victim);
			}
			
			// absolute cap
			use_resist = MIN(use_resist, max_resist);
			
			if (use_resist > 0) {	// positive resistance
				resist_prc = (use_resist / max_resist);
				dam = (int) round(dam * (1.0 - (resist_prc / 4.0))); // at 1.0 resist_prc, it reduces damage by 25% (1/4)
			}
			else if (use_resist < 0) {	// negative resistance (penalty)
				use_resist *= -1;	// make positive
				use_resist = diminishing_returns(use_resist, 2.0);	// diminish on a scale of 2
				use_resist /= 100;	// use_resist is now a % to increase
				dam = (int) round(dam * (1.0 + use_resist));
			}
		}
	
		if (check_blood_fortitude(victim, can_gain_exp_from(victim, attacker))) {
			dam = (int) round(0.9 * dam);
		}
	
		// redirect some damage to mana: player only
		if (damtype == DAM_MAGICAL && !IS_NPC(victim) && has_ability(victim, ABIL_NULL_MANA) && GET_MANA(victim) > 0) {
			int absorb = MIN(dam / 2, GET_MANA(victim));
		
			if (absorb > 0) {
				dam -= absorb;
				GET_MANA(victim) -= absorb;
				if (can_gain_exp_from(victim, attacker)) {
					gain_ability_exp(victim, ABIL_NULL_MANA, 5);
				}
			}
		}
	}
	
	if (damtype == DAM_POISON) {
		if (has_player_tech(victim, PTECH_NO_POISON)) {
			dam = 0;
		}
		if (can_gain_exp_from(victim, attacker)) {
			gain_player_tech_exp(victim, PTECH_NO_POISON, 2);
			gain_ability_exp(victim, ABIL_RESIST_POISON, 5);
		}
	}
	
	return dam;
}


/**
* Replaces #w and #W with first/third person attack types.
*
* @param const char *str The input string.
* @param const char *weapon_first The string to replace #w with.
* @param const char *weapon_third The string to replace #W with.
* @param const char *weapon_noun The string to replace #x with.
* @return char* The replaced string.
*/
static char *replace_fight_string(const char *str, const char *weapon_first, const char *weapon_third, const char *weapon_noun) {
	static char buf[MAX_STRING_LENGTH];
	char *cp = buf;

	for (; *str; str++) {
		if (*str == '#') {
			switch (*(++str)) {
				case 'W':
					for (; *weapon_third; *(cp++) = *(weapon_third++));
					break;
				case 'w':
					for (; *weapon_first; *(cp++) = *(weapon_first++));
					break;
				case 'x': {
					for (; *weapon_noun; *(cp++) = *(weapon_noun++));
					break;
				}
				default:
					*(cp++) = '#';
					break;
			}
		}
		else
			*(cp++) = *str;

		*cp = 0;
	}

	return (buf);
}


/**
* Pulls a character out of combat and stops every non-autokill person from
* hitting him/her.
*
* @param char_data *ch The person who is dying.
*/
void stop_combat_no_autokill(char_data *ch) {
	char_data *ch_iter;
	
	if (FIGHTING(ch)) {
		stop_fighting(ch);
	}

	// look for anybody in the room fighting ch who wouldn't execute:
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), ch_iter, next_in_room) {
		if (ch_iter != ch && FIGHTING(ch_iter) == ch && !WOULD_EXECUTE(ch_iter, ch)) {
			stop_fighting(ch_iter);
		}
	}

	/* knock 'em out */
	GET_HEALTH(ch) = -1;
	GET_POS(ch) = POS_INCAP;

	// remove all DoTs
	while (ch->over_time_effects) {
		dot_remove(ch, ch->over_time_effects);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMBAT METERS ///////////////////////////////////////////////////////////

/**
* Resets all of a player's meters and begins combat.
*
* @param char_data *ch The player.
*/
void reset_combat_meters(char_data *ch) {
	struct combat_meters *mtr;
	
	if (IS_NPC(ch)) {
		return;	// no meter
	}
	
	mtr = &GET_COMBAT_METERS(ch);
	mtr->hits = 0;
	mtr->misses = 0;
	mtr->hits_taken = 0;
	mtr->dodges = 0;
	mtr->blocks = 0;
	mtr->damage_dealt = 0;
	mtr->damage_taken = 0;
	mtr->pet_damage = 0;
	mtr->heals_dealt = 0;
	mtr->heals_taken = 0;
	mtr->start = mtr->end = time(0);
	mtr->over = FALSE;
}


/**
* Checks if a player is out of combat yet and ends the meter, if active.
*
* @param char_data *ch The player.
*/
void check_combat_end(char_data *ch) {
	if (!IS_NPC(ch) && GET_COMBAT_METERS(ch).over == FALSE && !is_fighting(ch)) {
		GET_COMBAT_METERS(ch).over = TRUE;
		GET_COMBAT_METERS(ch).end = time(0);
	}
}


/**
* Called in combat, to reset meters if a new combat has started.
*
* @param char_data *ch The player.
*/
void check_combat_start(char_data *ch) {
	if (!IS_NPC(ch) && GET_COMBAT_METERS(ch).over == TRUE) {
		if (PRF_FLAGGED(ch, PRF_CLEARMETERS)) {
			reset_combat_meters(ch);
		}
		else {
			// just update the time so combat length is still correct
			GET_COMBAT_METERS(ch).over = FALSE;
			GET_COMBAT_METERS(ch).start = time(0) - (GET_COMBAT_METERS(ch).end - GET_COMBAT_METERS(ch).start);
			GET_COMBAT_METERS(ch).end = GET_COMBAT_METERS(ch).start;
		}
	}
}


/**
* Marks damage dealt on the meters.
* @param char_data *ch The player.
*/
void combat_meter_damage_dealt(char_data *ch, int amt) {
	if (!IS_NPC(ch) && !GET_COMBAT_METERS(ch).over) {
		GET_COMBAT_METERS(ch).damage_dealt += amt;
	}
	else if (ch->master && !IS_NPC(ch->master) && !GET_COMBAT_METERS(ch->master).over) {
		// credit the NPC's immediate master
		GET_COMBAT_METERS(ch->master).pet_damage += amt;
	}
}


/**
* Marks damage taken on the meters.
* @param char_data *ch The player.
*/
void combat_meter_damage_taken(char_data *ch, int amt) {
	if (!IS_NPC(ch) && !GET_COMBAT_METERS(ch).over) {
		GET_COMBAT_METERS(ch).damage_taken += amt;
	}
}


/**
* Marks a block on the meters.
* @param char_data *ch The player.
*/
void combat_meter_block(char_data *ch) {
	if (!IS_NPC(ch) && !GET_COMBAT_METERS(ch).over) {
		GET_COMBAT_METERS(ch).blocks += 1;
	}
}


/**
* Marks a dodge on the meters.
* @param char_data *ch The player.
*/
void combat_meter_dodge(char_data *ch) {
	if (!IS_NPC(ch) && !GET_COMBAT_METERS(ch).over) {
		GET_COMBAT_METERS(ch).dodges += 1;
	}
}


/**
* Marks a heal dealt on the meters.
* @param char_data *ch The player.
*/
void combat_meter_heal_dealt(char_data *ch, int amt) {
	if (!IS_NPC(ch) && !GET_COMBAT_METERS(ch).over) {
		GET_COMBAT_METERS(ch).heals_dealt += amt;
	}
}


/**
* Marks a heal taken on the meters.
* @param char_data *ch The player.
*/
void combat_meter_heal_taken(char_data *ch, int amt) {
	if (!IS_NPC(ch) && !GET_COMBAT_METERS(ch).over) {
		GET_COMBAT_METERS(ch).heals_taken += amt;
	}
}


/**
* Marks a hit on the meters.
* @param char_data *ch The player.
*/
void combat_meter_hit(char_data *ch) {
	if (!IS_NPC(ch) && !GET_COMBAT_METERS(ch).over) {
		GET_COMBAT_METERS(ch).hits += 1;
	}
}


/**
* Marks a hit taken on the meters.
* @param char_data *ch The player.
*/
void combat_meter_hit_taken(char_data *ch) {
	if (!IS_NPC(ch) && !GET_COMBAT_METERS(ch).over) {
		GET_COMBAT_METERS(ch).hits_taken += 1;
	}
}


/**
* Marks a miss on the meters.
* @param char_data *ch The player.
*/
void combat_meter_miss(char_data *ch) {
	if (!IS_NPC(ch) && !GET_COMBAT_METERS(ch).over) {
		GET_COMBAT_METERS(ch).misses += 1;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER-KILLED-BY ////////////////////////////////////////////////////////

/**
* Marks a recent playerkill (by a PC or an empire mob).
*
* @param char_data *ch The player dying.
* @param char_data *killer Who killed them.
*/
void add_player_kill(char_data *ch, char_data *killer) {
	struct pk_data *iter, *data = NULL;
	
	// bump up the food chain to find a player master, if possible
	while (killer && IS_NPC(killer) && killer->master) {
		killer = killer->master;
	}
	
	if (!ch || !killer || IS_NPC(ch) || !GET_ACCOUNT(ch) || (IS_NPC(killer) && !GET_LOYALTY(killer))) {
		return;	// nothing to track here
	}
	
	// find matching data
	if (!IS_NPC(killer)) {
		if (GET_LOYALTY(killer)) {
			avenge_solo_offenses_from_player(GET_LOYALTY(killer), ch);
		}
		
		LL_SEARCH_SCALAR(GET_ACCOUNT(ch)->killed_by, data, player_id, GET_IDNUM(killer));
		
		// mark empire offense, only if not-pvp-enabled (always 'seen')
		if (!IS_PVP_FLAGGED(ch) && GET_LOYALTY(ch)) {
			remove_recent_offenses(GET_LOYALTY(ch), OFFENSE_ATTACKED_PLAYER, killer);
			add_offense(GET_LOYALTY(ch), OFFENSE_KILLED_PLAYER, killer, IN_ROOM(killer), OFF_SEEN);
		}
	}
	else if (GET_LOYALTY(killer)) {	// is npc
		LL_FOREACH(GET_ACCOUNT(ch)->killed_by, iter) {
			if (iter->player_id == NOTHING && iter->empire == EMPIRE_VNUM(GET_LOYALTY(killer))) {
				// found a match for no-player, same empire
				data = iter;
				break;
			}
		}
	}
	
	// add data if missing
	if (!data) {
		CREATE(data, struct pk_data, 1);
		data->player_id = IS_NPC(killer) ? NOTHING : GET_IDNUM(killer);
		LL_PREPEND(GET_ACCOUNT(ch)->killed_by, data);
	}
	
	// update data
	data->empire = GET_LOYALTY(killer) ? EMPIRE_VNUM(GET_LOYALTY(killer)) : NOTHING;
	data->killed_alt = GET_IDNUM(ch);
	data->last_time = time(0);
	
	SAVE_ACCOUNT(GET_ACCOUNT(ch));
}


/**
* Times out useless old pk entries (ones over ONE WEEK).
*
* @param char_data *ch The player to clean.
*/
void clean_player_kills(char_data *ch) {
	struct pk_data *iter, *next_iter;
	time_t now = time(0);
	bool any = FALSE;
	
	if (!ch || IS_NPC(ch) || !GET_ACCOUNT(ch)) {
		return;	// wut
	}
	
	LL_FOREACH_SAFE(GET_ACCOUNT(ch)->killed_by, iter, next_iter) {
		if (now - iter->last_time > SECS_PER_REAL_WEEK) {
			LL_DELETE(GET_ACCOUNT(ch)->killed_by, iter);
			free(iter);
			any = TRUE;
		}
	}
	
	if (any) {
		SAVE_ACCOUNT(GET_ACCOUNT(ch));
	}
}


/**
* Gets the last time a player was killed by a member of a given empire. This
* can be used e.g. to block hostile actions. This data is only accurate for
* ONE WEEK.
*
* @param char_data *ch The player to check.
* @param empire_data *emp The empire they might have been killed by.
* @return time_t The timestamp of the last kill by that empire, or 0 if no recent time.
*/
time_t get_last_killed_by_empire(char_data *ch, empire_data *emp) {
	struct pk_data *pk;
	time_t min = 0;
	
	if (IS_NPC(ch) || !GET_ACCOUNT(ch)) {
		return 0;	// le never
	}
	
	LL_FOREACH(GET_ACCOUNT(ch)->killed_by, pk) {
		if (pk->empire == EMPIRE_VNUM(emp) && (min == 0 || min > pk->last_time)) {
			min = pk->last_time;
		}
	}
	
	return min;	// may be 0
}


 //////////////////////////////////////////////////////////////////////////////
//// DEATH AND CORPSES ///////////////////////////////////////////////////////

/**
* Sends a death cry to the room and surrounding rooms.
*
* @param char_data *ch
*/
void death_cry(char_data *ch) {
	struct room_direction_data *ex;
	room_data *rl, *to_room;
	int iter;

	act("Your blood freezes as you hear $n's death cry.", FALSE, ch, 0, 0, TO_ROOM);

	if (ROOM_IS_CLOSED(IN_ROOM(ch)) && COMPLEX_DATA(IN_ROOM(ch))) {
		for (ex = COMPLEX_DATA(IN_ROOM(ch))->exits; ex; ex = ex->next) {
			if (ex->room_ptr) {
				send_to_room("Your blood freezes as you hear someone's death cry.\r\n", ex->room_ptr);
			}
		}
	}
	else {
		rl = HOME_ROOM(IN_ROOM(ch));
		
		for (iter = 0; iter < NUM_2D_DIRS; ++iter) {
			if ((to_room = real_shift(rl, shift_dir[iter][0], shift_dir[iter][1]))) {
				send_to_room("Your blood freezes as you hear someone's death cry.\r\n", to_room);
			}
		}
	}
}


/**
* Restores a player after death (either from a respawn or a resurrect) and
* ensures they are not in combat and are not affected by anything.
*
* The player will be standing and have at least SOME health after this is run.
*
* @param char_data *ch The person to death-restore.
*/
void death_restore(char_data *ch) {
	char_data *ch_iter;
	
	// ensure not fighting
	if (FIGHTING(ch)) {
		stop_fighting(ch);
	}
	for (ch_iter = combat_list; ch_iter; ch_iter = next_combat_list) {
		next_combat_list = ch_iter->next_fighting;
		
		if (FIGHTING(ch_iter) == ch) {
			stop_fighting(ch_iter);
		}
	}
	
	// remove respawn/rez
	remove_cooldown_by_type(ch, COOLDOWN_DEATH_RESPAWN);
	remove_offers_by_type(ch, OFFER_RESURRECTION);
	
	// Pools restore
	GET_HEALTH(ch) = MAX(1, GET_MAX_HEALTH(ch) / 4);
	GET_MOVE(ch) = MAX(1, GET_MAX_MOVE(ch) / 4);
	GET_MANA(ch) = MAX(1, GET_MAX_MANA(ch) / 4);
	GET_BLOOD(ch) = IS_VAMPIRE(ch) ? MAX(1, GET_MAX_BLOOD(ch) / 4) : GET_MAX_BLOOD(ch);
	
	// conditions restore
	if (GET_COND(ch, FULL) > 0) {
		GET_COND(ch, FULL) = 0;
	}
	if (GET_COND(ch, DRUNK) > 0) {
		GET_COND(ch, DRUNK) = 0;
	}
	if (GET_COND(ch, THIRST) > 0) {
		GET_COND(ch, THIRST) = 0;
	}
	
	INJURY_FLAGS(ch) = NOBITS;
	GET_POS(ch) = POS_STANDING;
	
	// in case they got re-affected as a corpse
	while (ch->affected) {
		affect_remove(ch, ch->affected);
	}
	while (ch->over_time_effects) {
		dot_remove(ch, ch->over_time_effects);
	}
}


/**
* die() -- kills a character, drops a corpse (usually)
*
* @param char_data *ch The person who is dying
* @param char_data *killer The person who killed them (may be self)
* @return obj_data *The corpse (if any), or NULL
*/
obj_data *die(char_data *ch, char_data *killer) {
	void cancel_blood_upkeeps(char_data *ch);
	void despawn_charmies(char_data *ch);
	void kill_empire_npc(char_data *ch);
	
	char_data *ch_iter, *player, *killmaster;
	obj_data *corpse = NULL;
	struct mob_tag *tag;
	int iter, trig_val;
	
	// no need to repeat
	if (EXTRACTED(ch)) {
		return NULL;
	}
	
	// find a player in the chain -- in case a pet kills the mob
	killmaster = killer;
	while (killmaster && IS_NPC(killmaster) && killmaster->master && IN_ROOM(killmaster) == IN_ROOM(killmaster->master)) {
		killmaster = killmaster->master;
	}
	
	// remove all DoTs (BEFORE phoenix)
	while (ch->over_time_effects) {
		dot_remove(ch, ch->over_time_effects);
	}
	
	// somebody saaaaaaave meeeeeeeee
	if (affected_by_spell(ch, ATYPE_PHOENIX_RITE)) {
		affect_from_char(ch, ATYPE_PHOENIX_RITE, FALSE);
		GET_HEALTH(ch) = GET_MAX_HEALTH(ch) / 4;
		GET_BLOOD(ch) = IS_VAMPIRE(ch) ? MAX(GET_BLOOD(ch), GET_MAX_BLOOD(ch) / 5) : GET_MAX_BLOOD(ch);
		GET_MOVE(ch) = MAX(GET_MOVE(ch), GET_MAX_MOVE(ch) / 5);
		GET_MANA(ch) = MAX(GET_MANA(ch), GET_MAX_MANA(ch) / 5);
		GET_POS(ch) = FIGHTING(ch) ? POS_FIGHTING : POS_STANDING;
		msg_to_char(ch, "A fiery phoenix erupts from your chest and restores you to your feet!\r\n");
		act("A fiery phoenix erupts from $n's chest and restores $m to $s feet!", FALSE, ch, NULL, NULL, TO_ROOM);
		gain_ability_exp(ch, ABIL_PHOENIX_RITE, 100);
		return NULL;
	}
	
	// hostile activity triggers distrust unless the ch is pvp-flagged or already hostile
	if (!IS_NPC(killmaster) && GET_LOYALTY(ch) && GET_LOYALTY(killmaster) != GET_LOYALTY(ch)) {
		// we check the ch's master if it's an NPC and the master is a PC
		char_data *check = (IS_NPC(ch) && ch->master && !IS_NPC(ch->master)) ? ch->master : ch;
		if ((IS_NPC(check) || !IS_PVP_FLAGGED(check)) && !IS_HOSTILE(check) && (!ROOM_OWNER(IN_ROOM(killer)) || ROOM_OWNER(IN_ROOM(killer)) != GET_LOYALTY(killmaster))) {
			trigger_distrust_from_hostile(killmaster, GET_LOYALTY(ch));
		}
	}
	
	// Alert Group if Applicable
	if (GROUP(ch)) {
		send_to_group(ch, GROUP(ch), "%s has died.", GET_NAME(ch));
	}
	
	// disable things
	perform_morph(ch, NULL);
	cancel_blood_upkeeps(ch);
	perform_dismount(ch);
	
	// pull from combat
	if (FIGHTING(ch)) {
		stop_fighting(ch);
	}
	for (ch_iter = combat_list; ch_iter; ch_iter = ch_iter->next_fighting) {
		if (FIGHTING(ch_iter) == ch) {
			stop_fighting(ch_iter);
		}
	}

	// unaffect
	while (ch->affected) {
		affect_remove(ch, ch->affected);
	}
	
	// get rid of any charmies who are lying around
	despawn_charmies(ch);
	
	// for players, die() ends here, until they respawn or quit
	if (!IS_NPC(ch)) {
		if (ch != killmaster) {
			add_player_kill(ch, killmaster);
		}
		add_cooldown(ch, COOLDOWN_DEATH_RESPAWN, config_get_int("death_release_minutes") * SECS_PER_REAL_MIN);
		msg_to_char(ch, "Type 'respawn' to come back at your tomb.\r\n");
		GET_HEALTH(ch) = MIN(GET_HEALTH(ch), -10);	// ensure negative health
		GET_POS(ch) = POS_DEAD;	// ensure pos
		run_kill_triggers(ch, killer, NULL);
		return NULL;
	}
	
	// -- only NPCs make it past here --

	/* To make ordinary commands work in scripts.  welcor*/  
	GET_POS(ch) = POS_STANDING;
	trig_val = death_mtrigger(ch, killmaster);
	trig_val &= run_kill_triggers(ch, killer, NULL);
	if (trig_val) {
		death_cry(ch);
	}
	GET_POS(ch) = POS_DEAD;
	
	// check if this was an empire npc
	if (IS_NPC(ch) && GET_EMPIRE_NPC_DATA(ch)) {
		kill_empire_npc(ch);
	}
	
	// expand tags, if there are any
	expand_mob_tags(ch);
	
	// mark killed and give faction rep
	LL_FOREACH(MOB_TAGGED_BY(ch), tag) {
		if ((player = is_playing(tag->idnum))) {
			qt_kill_mob(player, ch);
			
			// if in same room, give faction rep
			if (IN_ROOM(player) == IN_ROOM(ch)) {
				if (MOB_FACTION(ch)) {
					gain_reputation(player, FCT_VNUM(MOB_FACTION(ch)), -1, TRUE, TRUE);
				}
			}
		}
	}

	drop_loot(ch, killmaster);
	if (MOB_FLAGGED(ch, MOB_NO_CORPSE)) {
		// remove any gear
		for (iter = 0; iter < NUM_WEARS; ++iter) {
			if (GET_EQ(ch, iter)) {
				remove_otrigger(GET_EQ(ch, iter), ch);
				if (GET_EQ(ch, iter)) {	// if it wasn't lost to a trigger
					obj_to_char(unequip_char(ch, iter), ch);
				}
			}
		}
		if (!IS_NPC(killmaster) && IS_NPC(ch)) {
			recursive_loot_set(ch->carrying, GET_IDNUM(killmaster), GET_LOYALTY(killmaster));
		}
		while (ch->carrying) {
			if (IS_NPC(ch) && MOB_TAGGED_BY(ch)) {
				// mark as gathered
				add_production_total_for_tag_list(MOB_TAGGED_BY(ch), GET_OBJ_VNUM(ch->carrying), 1);
			}
			obj_to_room(ch->carrying, IN_ROOM(ch));
		}
	}
	else {
		corpse = make_corpse(ch);
		obj_to_room(corpse, IN_ROOM(ch));
		
		// tag corpse
		if (corpse && !IS_NPC(killmaster)) {
			recursive_loot_set(corpse, GET_IDNUM(killmaster), GET_LOYALTY(killmaster));
		}
		
		load_otrigger(corpse);
	}
	
	extract_char(ch);	
	return corpse;
}


// this drops the loot to the inventory of the 'ch' who is interacting -- so run it on the mob itself, usually
INTERACTION_FUNC(loot_interact) {
	obj_data *obj;
	int iter, scale_level = 0;
	
	// both ch and inter_mob are required
	if (!ch || !inter_mob) {
		return FALSE;
	}
	
	// determine scale level for loot
	scale_level = get_approximate_level(inter_mob);
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		obj = read_object(interaction->vnum, TRUE);
		
		if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			// set flags (before scaling)
			if (!OBJ_FLAGGED(obj, OBJ_GENERIC_DROP)) {
				if (MOB_FLAGGED(inter_mob, MOB_HARD)) {
					SET_BIT(GET_OBJ_EXTRA(obj), OBJ_HARD_DROP);
				}
				if (MOB_FLAGGED(inter_mob, MOB_GROUP)) {
					SET_BIT(GET_OBJ_EXTRA(obj), OBJ_GROUP_DROP);
				}
			}
		}
		
		// scale
		scale_item_to_level(obj, scale_level);
		
		// preemptive binding
		if (OBJ_FLAGGED(obj, OBJ_BIND_ON_PICKUP) && IS_NPC(inter_mob)) {
			bind_obj_to_tag_list(obj, MOB_TAGGED_BY(inter_mob));
		}
		
		// mark for extract if no player gets it
		SET_BIT(GET_OBJ_EXTRA(obj), OBJ_UNCOLLECTED_LOOT);
		
		obj_to_char(obj, inter_mob);
		load_otrigger(obj);
	}
	
	return TRUE;
}


/**
* This goes through a mob's loot table and adds item randomly to inventory,
* which will later be put in the mob's corpse (or whatever).
*
* @param char_data *mob the dying mob
* @param char_data *killer the person who killed it (optional)
*/
void drop_loot(char_data *mob, char_data *killer) {
	extern int mob_coins(char_data *mob);

	obj_data *obj;
	int coins;
	empire_data *coin_emp;

	if (!mob || !IS_NPC(mob) || MOB_FLAGGED(mob, MOB_NO_LOOT)) {
		return;
	}

	// find and drop loot
	run_interactions(mob, mob->interactions, INTERACT_LOOT, IN_ROOM(mob), mob, NULL, loot_interact);
	run_global_mob_interactions(mob, mob, INTERACT_LOOT, loot_interact);
	
	// coins?
	if (killer && !IS_NPC(killer) && (!GET_LOYALTY(mob) || GET_LOYALTY(mob) == GET_LOYALTY(killer) || char_has_relationship(killer, mob, DIPL_WAR | DIPL_THIEVERY))) {
		coins = mob_coins(mob);
		coin_emp = GET_LOYALTY(mob);
		if (coins > 0) {
			obj = create_money(coin_emp, coins);
			
			// mark for extract if no player gets it
			SET_BIT(GET_OBJ_EXTRA(obj), OBJ_UNCOLLECTED_LOOT);
			
			obj_to_char(obj, mob);
		}
	}
}


/**
* @param char_data *ch The person who died.
* @return obj_data* The corpse of ch.
*/
obj_data *make_corpse(char_data *ch) {	
	char shortdesc[MAX_INPUT_LENGTH], longdesc[MAX_INPUT_LENGTH], kws[MAX_INPUT_LENGTH];
	obj_data *corpse, *rope, *o, *next_o;
	int i, size;
	bool human = (!IS_NPC(ch) || MOB_FLAGGED(ch, MOB_HUMAN));

	corpse = read_object(o_CORPSE, TRUE);
	size = GET_SIZE(ch);
	
	// store as person's last corpse id
	if (!IS_NPC(ch)) {
		GET_LAST_CORPSE_ID(ch) = obj_script_id(corpse);
	}
	else {	// mob corpse setup
		if (!size_data[size].can_take_corpse) {
			REMOVE_BIT(GET_OBJ_WEAR(corpse), ITEM_WEAR_TAKE);
		}
		SET_BIT(GET_OBJ_EXTRA(corpse), size_data[size].corpse_flags);
	}
	
	// binding
	if (OBJ_FLAGGED(corpse, OBJ_BIND_ON_PICKUP)) {
		if (IS_NPC(ch)) {
			// mob corpse: bind to whoever tagged the mob
			bind_obj_to_tag_list(corpse, MOB_TAGGED_BY(ch));
		}
	}
	
	if (human) {
		sprintf(kws, "%s %s %s", GET_OBJ_KEYWORDS(corpse), skip_filler(PERS(ch, ch, FALSE)), size_data[size].corpse_keywords);
		sprintf(shortdesc, "%s's body", PERS(ch, ch, FALSE));
		snprintf(longdesc, sizeof(longdesc), size_data[size].body_long_desc, PERS(ch, ch, FALSE));
		CAP(longdesc);
	}
	else {
		sprintf(kws, "%s %s %s", GET_OBJ_KEYWORDS(corpse), skip_filler(PERS(ch, ch, FALSE)), size_data[size].corpse_keywords);
		sprintf(shortdesc, "the corpse of %s", PERS(ch, ch, FALSE));
		snprintf(longdesc, sizeof(longdesc), size_data[size].corpse_long_desc, PERS(ch, ch, FALSE));
		CAP(longdesc);
	}
	
	// set strings
	GET_OBJ_KEYWORDS(corpse) = str_dup(kws);
	GET_OBJ_SHORT_DESC(corpse) = str_dup(shortdesc);
	GET_OBJ_LONG_DESC(corpse) = str_dup(longdesc);

	GET_OBJ_VAL(corpse, VAL_CORPSE_IDNUM) = IS_NPC(ch) ? GET_MOB_VNUM(ch) : (-1 * GET_IDNUM(ch));
	GET_OBJ_VAL(corpse, VAL_CORPSE_SIZE) = size;
	GET_OBJ_VAL(corpse, VAL_CORPSE_FLAGS) = 0;
		
	if (human) {
		SET_BIT(GET_OBJ_VAL(corpse, VAL_CORPSE_FLAGS), CORPSE_HUMAN);
	}

	/* transfer character's inventory to the corpse -- ONLY FOR NPCs */
	if (IS_NPC(ch)) {
		corpse->contains = ch->carrying;
		for (o = corpse->contains; o != NULL; o = o->next_content)
			o->in_obj = corpse;
		object_list_no_owner(corpse);
		
		/* transfer character's equipment to the corpse */
		for (i = 0; i < NUM_WEARS; i++) {
			if (GET_EQ(ch, i)) {
				remove_otrigger(GET_EQ(ch, i), ch);
				if (GET_EQ(ch, i)) {	// if it wasn't eaten by a trigger
					obj_to_obj(unequip_char(ch, i), corpse);
				}
			}
		}
		
		// rope if it was tied
		if (MOB_FLAGGED(ch, MOB_TIED) && GET_ROPE_VNUM(ch) != NOTHING && (rope = read_object(GET_ROPE_VNUM(ch), TRUE))) {
			scale_item_to_level(rope, 1);
			obj_to_obj(rope, corpse);
			load_otrigger(rope);
		}
		GET_ROPE_VNUM(ch) = NOTHING;

		IS_CARRYING_N(ch) = 0;
		ch->carrying = NULL;
		
		if (MOB_TAGGED_BY(ch)) {
			LL_FOREACH2(corpse->contains, o, next_content) {
				add_production_total_for_tag_list(MOB_TAGGED_BY(ch), GET_OBJ_VNUM(o), 1);
			}
		}
	}
	else {
		// not an npc, but check for stolen
		for (o = ch->carrying; o; o = next_o) {
			next_o = o->next_content;
			
			// is it stolen?
			if (IS_STOLEN(o)) {
				obj_to_obj(o, corpse);
			}
		}
		
		// check eq for stolen
		for (i = 0; i < NUM_WEARS; ++i) {
			if (GET_EQ(ch, i) && IS_STOLEN(GET_EQ(ch, i))) {
				obj_to_obj(unequip_char(ch, i), corpse);
			}
		}
	}

	return corpse;
}

/**
* Performs the actual resurrection, sends some messages, and does a skillup.
*
* @param char_data *ch The player resurrecting.
* @param char_data *rez_by Optional (or NULL): The player who resurrected them.
* @param room_data *loc The location to resurrect to.
* @param any_vnum ability Optional (or NO_ABIL): The ability to skillup for rez_by.
*/
void perform_resurrection(char_data *ch, char_data *rez_by, room_data *loc, any_vnum ability) {
	extern obj_data *find_obj(int n, bool error);

	obj_data *corpse;
	int exp = 15;	// overridden by some abilities
	
	// sanity
	if (!ch || !loc) {
		return;
	}
	
	if (IN_ROOM(ch) != loc) {
		act("$n vanishes in a swirl of light!", TRUE, ch, NULL, NULL, TO_ROOM);
		GET_LAST_DIR(ch) = NO_DIR;
	}
	
	// move character
	char_to_room(ch, loc);
	qt_visit_room(ch, IN_ROOM(ch));
	msdp_update_room(ch);
	
	// take care of the corpse
	if ((corpse = find_obj(GET_LAST_CORPSE_ID(ch), FALSE)) && IS_CORPSE(corpse)) {
		while (corpse->contains) {
			obj_to_char(corpse->contains, ch);
		}
		extract_obj(corpse);
	}
	
	if (IS_DEAD(ch)) {
		death_restore(ch);
	}
	else {
		// resurrected from life: knock off 1 death count
		if (GET_RECENT_DEATH_COUNT(ch) > 0) {
			GET_RECENT_DEATH_COUNT(ch) -= 1;
		}
	}
	affect_from_char(ch, ATYPE_DEATH_PENALTY, FALSE);	// in case
	remove_offers_by_type(ch, OFFER_RESURRECTION);

	// log
	syslog(SYS_DEATH, GET_INVIS_LEV(ch), TRUE, "%s has been resurrected by %s at %s", GET_NAME(ch), rez_by ? GET_NAME(rez_by) : "(unknown)", room_log_identifier(loc));

	// abil-specific
	switch (ability) {
		case ABIL_RESURRECT: {
			// custom restore stats: 10% (death_restore puts it at 25%)
			GET_HEALTH(ch) = MAX(1, GET_MAX_HEALTH(ch) / 10);
			GET_MOVE(ch) = MAX(1, GET_MAX_MOVE(ch) / 10);
			GET_MANA(ch) = MAX(1, GET_MAX_MANA(ch) / 10);
			GET_BLOOD(ch) = IS_VAMPIRE(ch) ? MAX(1, GET_MAX_BLOOD(ch) / 10) : GET_MAX_BLOOD(ch);

			msg_to_char(ch, "A strange force lifts you up from the ground, and you seem to float back to your feet...\r\n");
			msg_to_char(ch, "You feel a rush of blood as your heart starts beating again...\r\n");
			if (rez_by && rez_by != ch) {
				act("You have been resurrected by $N!", FALSE, ch, NULL, rez_by, TO_CHAR);
			}
			else {
				act("You have been resurrected!", FALSE, ch, NULL, NULL, TO_CHAR);
			}
			act("A brilliant light bursts forth from $n's chest as $e floats up from the ground and comes back to life!", FALSE, ch, NULL, NULL, TO_ROOM);

			exp = 15;
			break;
		}
		case ABIL_MOONRISE: {
			// custom restore stats: 50% (death_restore puts it at 25%)
			GET_HEALTH(ch) = MAX(1, GET_MAX_HEALTH(ch) / 2);
			GET_MOVE(ch) = MAX(1, GET_MAX_MOVE(ch) / 2);
			GET_MANA(ch) = MAX(1, GET_MAX_MANA(ch) / 2);
			GET_BLOOD(ch) = IS_VAMPIRE(ch) ? MAX(1, GET_MAX_BLOOD(ch) / 2) : GET_MAX_BLOOD(ch);

			msg_to_char(ch, "A strange force lifts you up from the ground, and you seem to float back to your feet...\r\n");
			msg_to_char(ch, "You feel a rush of blood as your heart starts beating again...\r\n");
			if (rez_by && rez_by != ch) {
				act("You have been resurrected by $N!", FALSE, ch, NULL, rez_by, TO_CHAR);
			}
			else {
				act("You have been resurrected!", FALSE, ch, NULL, NULL, TO_CHAR);
			}
			act("An eerie light bursts forth from $n's chest as $e floats up from the ground and comes back to life!", FALSE, ch, NULL, NULL, TO_ROOM);

			exp = 50;
			break;
		}
		default: {
			if (rez_by && rez_by != ch) {
				act("You have been resurrected by $N!", FALSE, ch, NULL, rez_by, TO_CHAR);
			}
			else {
				act("You have been resurrected!", FALSE, ch, NULL, NULL, TO_CHAR);
			}
			act("$n has been resurrected!", TRUE, ch, NULL, NULL, TO_ROOM);
			break;
		}
	}

	// skillups?
	if (rez_by && ability != NO_ABIL) {
		gain_ability_exp(rez_by, ability, exp);
	}
}


/**
* Finally kills a player who was lying there, dead.
*
* @param char_data *ch The player who will die.
* @return obj_data* The player's corpse object, if any.
*/
obj_data *player_death(char_data *ch) {
	void cancel_adventure_summon(char_data *ch);
	
	obj_data *corpse;
	perform_dismount(ch);	// just to be sure
	death_restore(ch);
	
	// update death stats
	GET_LAST_DEATH_TIME(ch) = time(0);
	GET_RECENT_DEATH_COUNT(ch) += 1;
	
	// make them a player corpse	
	if ((corpse = make_corpse(ch))) {
		obj_to_room(corpse, IN_ROOM(ch));

		// player corpse -- set as belonging to self
		recursive_loot_set(corpse, GET_IDNUM(ch), GET_LOYALTY(ch));
		load_otrigger(corpse);
	}
	
	// penalize after so many deaths
	if (GET_RECENT_DEATH_COUNT(ch) >= config_get_int("deaths_before_penalty") || (is_at_war(GET_LOYALTY(ch)) && GET_RECENT_DEATH_COUNT(ch) >= config_get_int("deaths_before_penalty_war"))) {
		int duration = config_get_int("seconds_per_death") * (GET_RECENT_DEATH_COUNT(ch) + 1 - config_get_int("deaths_before_penalty")) / SECS_PER_REAL_UPDATE;
		struct affected_type *af = create_flag_aff(ATYPE_DEATH_PENALTY, duration, AFF_IMMUNE_PHYSICAL | AFF_NO_ATTACK | AFF_HARD_STUNNED, ch);
		affect_join(ch, af, ADD_DURATION);
	}
	
	if (PLR_FLAGGED(ch, PLR_ADVENTURE_SUMMONED)) {
		GET_LAST_CORPSE_ID(ch) = -1;	// invalidate their last-corpse-id to prevent a rez (they can be adventure-summoned)
		// don't actually cancel the summon -- they'll get whisked back when they respawn
	}
	
	return corpse;
}


 //////////////////////////////////////////////////////////////////////////////
//// TOWER PROCESSING ////////////////////////////////////////////////////////

/**
* @param room_data *from_room The tower
* @param char_data *ch The victim
*/
static void shoot_at_char(room_data *from_room, char_data *ch) {
	int dam, type = ATTACK_GUARD_TOWER;
	empire_data *emp = ROOM_OWNER(from_room);
	room_data *to_room = IN_ROOM(ch);
	struct affected_type *af;

	/* Now we're sure we can hit this person: gets worse with dex */
	if (!AWAKE(ch) || !number(0, MAX(0, (GET_DEXTERITY(ch)/2) - 1))) {
		dam = 25 + (ROOM_BLD_FLAGGED(from_room, BLD_UPGRADED) ? 50 : 0);
	}
	else {
		dam = 0;
	}
	
	// guard towers ALWAYS see the offender
	add_offense(emp, OFFENSE_GUARD_TOWER, ch, to_room, OFF_SEEN);
	
	if (damage(ch, ch, dam, type, DAM_PHYSICAL) != 0) {
		// slow effect (1 mud hour)
		af = create_flag_aff(ATYPE_ARROW_TO_THE_KNEE, 1 MUD_HOURS, AFF_SLOW, ch);
		affect_join(ch, af, ADD_DURATION);
		
		// distraction effect (5 sec)
		af = create_flag_aff(ATYPE_ARROW_TO_THE_KNEE, 1, AFF_DISTRACTED, ch);
		affect_join(ch, af, 0);
		
		// cancel any action the character is doing
		if (GET_ACTION(ch) != ACT_NONE) {
			void cancel_action(char_data *ch);
			cancel_action(ch);
		}
	}
}


// for picking a random victim
struct tower_victim_list {
	char_data *ch;
	struct tower_victim_list *next;
};


/**
* @param room_data *from_room Origin tower
* @param char_data *vict Potential target
*/
static bool tower_would_shoot(room_data *from_room, char_data *vict) {
	empire_data *emp = ROOM_OWNER(from_room);
	empire_data *enemy = IS_NPC(vict) ? NULL : GET_LOYALTY(vict);
	empire_data *m_empire;
	room_data *shift_room, *to_room = IN_ROOM(vict);
	char_data *m;
	int iter, distance;
	bool hostile = IS_HOSTILE(vict);
	
	// sanity check
	if (!emp || EXTRACTED(vict) || IS_DEAD(vict)) {
		return FALSE;
	}
	
	distance = compute_distance(from_room, to_room);
	
	// no tower shoots further than 3
	if (distance > 3) {
		return FALSE;
	}
	
	// basic guard tower only shoots 2
	if (distance > 2 && !ROOM_BLD_FLAGGED(from_room, BLD_UPGRADED)) {
		return FALSE;
	}
	
	// don't shoot npcs
	if (IS_NPC(vict)) {
		return FALSE;
	}
	
	// can't see into buildings/mountains
	if (ROOM_IS_CLOSED(to_room) || ROOM_SECT_FLAGGED(to_room, SECTF_OBSCURE_VISION) || SECT_FLAGGED(BASE_SECT(to_room), SECTF_OBSCURE_VISION)) {
		return FALSE;
	}

	if (IS_IMMORTAL(vict) || IS_GOD(vict)) {
		return FALSE;
	}
	
	// visibility
	if ((AFF_FLAGGED(vict, AFF_INVISIBLE) || AFF_FLAGGED(vict, AFF_HIDE))) {
		return FALSE;
	}

	if (CHECK_MAJESTY(vict) || CHAR_MORPH_FLAGGED(vict, MORPHF_ANIMAL)) {
		return FALSE;
	}
	
	if (AFF_FLAGGED(vict, AFF_NO_TARGET_IN_ROOM | AFF_NO_ATTACK | AFF_IMMUNE_PHYSICAL)) {
		return FALSE;
	}

	if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_DARK)) {
		return FALSE;
	}
	
	// non-aggressive island
	if (ISLAND_FLAGGED(to_room, ISLE_NO_AGGRO)) {
		return FALSE;
	}
	
	// check character OR their leader for permission to use the guard tower room -- that saves them
	if (!((m = vict->master) && in_same_group(vict, m))) {
		m = vict;
	}
	
	if ((!IS_NPC(vict) && can_use_room(vict, from_room, GUESTS_ALLOWED)) || (!IS_NPC(m) && can_use_room(m, from_room, GUESTS_ALLOWED))) {
		return FALSE;
	}
	
	if (!has_empire_trait(emp, to_room, ETRAIT_DISTRUSTFUL) && !enemy && !hostile) {
		return FALSE;
	}

	// check diplomacy: if there IS a relation but not a hostile one, no probs
	if (enemy && !empire_is_hostile(emp, enemy, to_room)) {
		return FALSE;
	}
	
	// check master
	m_empire = GET_LOYALTY(m);
	if (m_empire && !empire_is_hostile(emp, m_empire, to_room)) {
		return FALSE;
	}
	
	// 

	// check intervening terrain
	if (distance > 1) {
		for (iter = 1; iter <= distance; ++iter) {
			shift_room = straight_line(from_room, to_room, iter);
			
			// no path in straight line somehow
			if (!shift_room) {
				return FALSE;
			}
			
			// found destination
			if (shift_room == to_room) {
				break;
			}
			
			if (ROOM_SECT_FLAGGED(shift_room, SECTF_OBSCURE_VISION)) {
				return FALSE;
			}
		}
	}
	
	// cloak of darkness
	if (!IS_NPC(vict) && has_player_tech(vict, PTECH_MAP_INVIS)) {
		gain_player_tech_exp(vict, PTECH_MAP_INVIS, 15);
		if (!number(0, 1) && player_tech_skill_check(vict, PTECH_MAP_INVIS, DIFF_HARD)) {
			return FALSE;
		}
	}
	
	return TRUE;
}


void process_tower(room_data *room) {
	empire_data *emp;
	int x, y;
	room_data *to_room;
	char_data *ch, *found = NULL;
	struct tower_victim_list *victim_list = NULL, *tvl;
	int num_victs = 0, pick;
	
	// empire check
	if (!(emp = ROOM_OWNER(room))) {
		return;
	}
	
	// building complete?
	if (!IS_COMPLETE(room)) {
		return;
	}
	
	// darkness effect
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_DARK)) {
		return;
	}
	
	// non-aggressive island
	if (ISLAND_FLAGGED(room, ISLE_NO_AGGRO)) {
		return;
	}
	
	for (x = -3; x <= 3; ++x) {
		for (y = -3; y <= 3; ++y) {
			to_room = real_shift(room, x, y);
			
			if (to_room) {
				for (ch = ROOM_PEOPLE(to_room); ch; ch = ch->next_in_room) {
					if (tower_would_shoot(room, ch)) {
						CREATE(tvl, struct tower_victim_list, 1);
						tvl->ch = ch;
					
						tvl->next = victim_list;
						victim_list = tvl;
						++num_victs;
					}
				}
			}
		}
	}

	if (num_victs > 0) {
		pick = number(0, num_victs-1);
		
		found = NULL;
		for (tvl = victim_list; tvl && !found; tvl = tvl->next) {
			if (pick-- <= 0) {
				found = tvl->ch;
			}
		}
	}
	
	// found or not, free the victim list
	while ((tvl = victim_list)) {
		victim_list = tvl->next;
		free(tvl);
	}
	
	if (found) {
		shoot_at_char(room, found);
	}
}


/**
* Iterates over empires, finds guard towers, and tries to shoot with them.
*/
void update_guard_towers(void) {
	struct empire_territory_data *ter, *ter_next;
	room_data *tower;
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter, ter_next) {
			tower = ter->room;
			
			if (room_has_function_and_city_ok(tower, FNC_GUARD_TOWER)) {
				process_tower(tower);
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MESSAGING ///////////////////////////////////////////////////////////////

/**
* Displays the messages for an actual block (and ensures combat).
* 
* @param char_data *ch the attacker
* @param char_data *victim the guy who blocked
* @param int w_type the weapon type
*/
void block_attack(char_data *ch, char_data *victim, int w_type) {
	char *pbuf;
	
	// block message to onlookers
	pbuf = replace_fight_string("$N blocks $n's #x.", attack_hit_info[w_type].first_pers, attack_hit_info[w_type].third_pers, attack_hit_info[w_type].noun);
	act(pbuf, FALSE, ch, NULL, victim, TO_NOTVICT | TO_COMBAT_MISS);

	// block message to ch
	if (ch->desc) {
		// send color separately because of act capitalization
		send_to_char("&y", ch);
		pbuf = replace_fight_string("You try to #w $N, but $E blocks.&0", attack_hit_info[w_type].first_pers, attack_hit_info[w_type].third_pers, attack_hit_info[w_type].noun);
		act(pbuf, FALSE, ch, NULL, victim, TO_CHAR | TO_COMBAT_MISS);
	}

	// block message to victim
	if (victim->desc) {
		// send color separately because of act capitalization
		send_to_char("&r", victim);
		pbuf = replace_fight_string("You block $n's #x.&0", attack_hit_info[w_type].first_pers, attack_hit_info[w_type].third_pers, attack_hit_info[w_type].noun);
		act(pbuf, FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP | TO_COMBAT_MISS);
	}
	
	// ensure combat
	engage_combat(victim, ch, TRUE);
}


/**
* Messaging for missile attack blocking.
*
* @param char_data *ch the attacker
* @param char_data *victim the guy who blocked
* @param int type The TYPE_ attack type.
*/
void block_missile_attack(char_data *ch, char_data *victim, int type) {
	char buf[MAX_STRING_LENGTH];
	
	snprintf(buf, sizeof(buf), "You %s at $N, but $E blocks.", attack_hit_info[type].first_pers);
	act(buf, FALSE, ch, NULL, victim, TO_CHAR | TO_COMBAT_MISS);
	
	snprintf(buf, sizeof(buf), "$n %s at $N, who blocks.", attack_hit_info[type].third_pers);
	act(buf, FALSE, ch, NULL, victim, TO_NOTVICT | TO_COMBAT_MISS);
	
	snprintf(buf, sizeof(buf), "$n %s at you, but you block.", attack_hit_info[type].third_pers);
	act(buf, FALSE, ch, NULL, victim, TO_VICT | TO_COMBAT_MISS);
}


/**
* Generic weapon damage messages.
*
* @param int dam How much damage was done.
* @param char_data *ch The character dealing the damage.
* @param char_data *victim The person receiving the damage.
* @param int w_type The weapon type (TYPE_)
*/
void dam_message(int dam, char_data *ch, char_data *victim, int w_type) {
	bitvector_t fmsg_type;
	char *buf;
	int iter, msgnum;

	static struct dam_weapon_type {
		int damage;
		const char *to_char;
		const char *to_victim;
		const char *to_room;	// rearranged this from circle3 to match the order of the "messages" file -pc
	} dam_weapons[] = {

		/* use #w for 1st-person (i.e. "slash") and #W for 3rd-person (i.e. "slashes") */

		{ 0,
		"You try to #w $N, but miss.",
		"$n tries to #w you, but misses.",
		"$n tries to #w $N, but misses." },

		{ 1,
		"You tickle $N as you #w $M.",
		"$n tickles you as $e #W you.",
		"$n tickles $N as $e #W $M." },

		{ 3,
		"You barely #w $N.",
		"$n barely #W you.",
		"$n barely #W $N." },

		{ 10,
		"You #w $N.",
		"$n #W you.",
		"$n #W $N." },

		{ 14,
		"You #w $N hard.",
		"$n #W you hard.",
		"$n #W $N hard." },

		{ 16,
		"You #w $N very hard.",
		"$n #W you very hard.",
		"$n #W $N very hard." },

		{ 20,
		"You #w $N extremely hard.",
		"$n #W you extremely hard.",
		"$n #W $N extremely hard." },

		{ 24,
		"You maul $N with your #x.",
		"$n mauls you with $s #x.",
		"$n mauls $N with $s #x." },

		{ 28,
		"You massacre $N with your #x.",
		"$n massacres you with $s #x.",
		"$n massacres $N with $s #x." },

		{ 32,
		"You decimate $N with your #x.",
		"$n decimates you with $s #x.",
		"$n decimates $N with $s #x." },

		{ 45,
		"You OBLITERATE $N with your deadly #x!!",
		"$n OBLITERATES you with $s deadly #x!!",
		"$n OBLITERATES $N with $s deadly #x!!" },

		{ 60,
		"You ANNIHILATE $N with your deadly #x!!",
		"$n ANNIHILATES you with $s deadly #x!!",
		"$n ANNIHILATES $N with $s deadly #x!!" },

		{ 75,
		"You MUTILATE $N with your deadly #x!!",
		"$n MUTILATES you with $s deadly #x!!",
		"$n MUTILATES $N with $s deadly #x!!" },

		{ 100,
		"You SLAUGHTER $N with your deadly #x!!",
		"$n SLAUGHTERS you with $s deadly #x!!",
		"$n SLAUGHTERS $N with $s deadly #x!!" },

		{ 125,
		"You **ERADICATE** $N with your deadly #x!!",
		"$n **ERADICATES** you with $s deadly #x!!",
		"$n **ERADICATES** $N with $s deadly #x!!" },

		// use -1 as the final entry (REQUIRED) -- captures any higher damage
		{ -1,
		"You **LIQUIDATE** $N with your deadly #x!!",
		"$n **LIQUIDATES** you with $s deadly #x!!",
		"$n **LIQUIDATES** $N with $s deadly #x!!" }
	};

	// find matching message
	msgnum = 0;
	for (iter = 0;; ++iter) {
		if (dam <= dam_weapons[iter].damage || dam_weapons[iter].damage == -1) {
			msgnum = iter;
			break;
		}
	}
	
	fmsg_type = (dam > 0 ? TO_COMBAT_HIT : TO_COMBAT_MISS);

	/* damage message to onlookers */
	if (!AFF_FLAGGED(victim, AFF_NO_SEE_IN_ROOM)) {
		buf = replace_fight_string(dam_weapons[msgnum].to_room, attack_hit_info[w_type].first_pers, attack_hit_info[w_type].third_pers, attack_hit_info[w_type].noun);
		act(buf, FALSE, ch, NULL, victim, TO_NOTVICT | fmsg_type);
	}

	/* damage message to damager */
	if (ch->desc && ch != victim && !AFF_FLAGGED(victim, AFF_NO_SEE_IN_ROOM)) {
		send_to_char("&y", ch);
		buf = replace_fight_string(dam_weapons[msgnum].to_char, attack_hit_info[w_type].first_pers, attack_hit_info[w_type].third_pers, attack_hit_info[w_type].noun);
		act(buf, FALSE, ch, NULL, victim, TO_CHAR | fmsg_type);
		send_to_char("&0", ch);
	}

	/* damage message to damagee */
	if (victim->desc) {
		send_to_char("&r", victim);
		buf = replace_fight_string(dam_weapons[msgnum].to_victim, attack_hit_info[w_type].first_pers, attack_hit_info[w_type].third_pers, attack_hit_info[w_type].noun);
		act(buf, FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP | fmsg_type);
		send_to_char("&0", victim);
	}
}


/**
* message for doing damage with a spell or skill
*  C3.0: Also used for weapon damage on miss and death blows
*
* @param int dam How much damage was done.
* @param char_data *ch The character dealing the damage.
* @param char_data *victim The person receiving the damage.
* @param int w_type The attack type (ATTACK_x)
* @return int 1: sent message, 0: no message found
*/
int skill_message(int dam, char_data *ch, char_data *vict, int attacktype) {
	int i, j, nr;
	struct message_type *msg;

	obj_data *weap = GET_EQ(ch, WEAR_WIELD);

	for (i = 0; i < MAX_MESSAGES; i++) {
		if (fight_messages[i].a_type == attacktype) {
			nr = dice(1, fight_messages[i].number_of_attacks);
			for (j = 1, msg = fight_messages[i].msg; (j < nr) && msg && msg->next; j++) {
				msg = msg->next;
			}
			
			// somehow
			if (!msg) {
				return 0;
			}

			if (!IS_NPC(vict) && (IS_IMMORTAL(vict) || (IS_GOD(vict) && !IS_GOD(ch)))) {
				if (!AFF_FLAGGED(vict, AFF_NO_SEE_IN_ROOM)) {
					if (ch != vict) {
						act(msg->god_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR | TO_COMBAT_MISS);
					}
					act(msg->god_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT | TO_COMBAT_MISS);
				}
				act(msg->god_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_COMBAT_MISS);
			}
			else if (dam != 0) {
				if (GET_POS(vict) == POS_DEAD) {
					if (!AFF_FLAGGED(vict, AFF_NO_SEE_IN_ROOM)) {
						if (ch != vict) {
							send_to_char("&y", ch);
							act(msg->die_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR | TO_COMBAT_HIT);
							send_to_char("&0", ch);
						}
						
						act(msg->die_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT | TO_COMBAT_HIT);
					}
					send_to_char("&r", vict);
					act(msg->die_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP | TO_COMBAT_HIT);
					send_to_char("&0", vict);
				}
				else {
					if (!AFF_FLAGGED(vict, AFF_NO_SEE_IN_ROOM)) {
						if (ch != vict) {
							send_to_char("&y", ch);
							act(msg->hit_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR | TO_COMBAT_HIT);
							send_to_char("&0", ch);
						}
						
						act(msg->hit_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT | TO_COMBAT_HIT);
					}
					send_to_char("&r", vict);
					act(msg->hit_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP | TO_COMBAT_HIT);
					send_to_char("&0", vict);
				}
			}
			else if (ch != vict) {	/* Dam == 0 */
				if (!AFF_FLAGGED(vict, AFF_NO_SEE_IN_ROOM)) {
					if (ch != vict) {
						send_to_char("&y", ch);
						act(msg->miss_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR | TO_COMBAT_MISS);
						send_to_char("&0", ch);
					}
					
					act(msg->miss_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT | TO_COMBAT_MISS);
				}
				send_to_char("&r", vict);
				act(msg->miss_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP | TO_COMBAT_MISS);
				send_to_char("&0", vict);
			}
			return (1);
		}
	}
	return (0);
}


 //////////////////////////////////////////////////////////////////////////////
//// RESTRICTORS /////////////////////////////////////////////////////////////

/*
 * can_fight()
 * Controls player-killing.  It sends its own message, so use it
 * appropriately.  Also, it stops people from fighting, so they
 * aren't spammed.
 */
bool can_fight(char_data *ch, char_data *victim) {
	extern bool has_one_day_playtime(char_data *ch);
	extern bitvector_t pk_ok;
	
	empire_data *ch_emp, *victim_emp;
	obj_data *obj;

	if (!ch || !victim) {
		syslog(SYS_ERROR, 0, TRUE, "SYSERR: can_fight() called without ch or victim");
		if (ch && FIGHTING(ch))
			stop_fighting(ch);
		return FALSE;
	}

	// absolutely never?
	if (EXTRACTED(victim) || EXTRACTED(ch) || IS_DEAD(victim) || IS_DEAD(ch)) {
		return FALSE;
	}

	if (ch == victim)
		return TRUE;
	
	check_scaling(victim, ch);
	
	/* charmies don't attack people */
	if (AFF_FLAGGED(ch, AFF_CHARM) && !IS_NPC(victim))
		return FALSE;

	/* Already fighting */
	if (FIGHTING(victim) == ch)
		return TRUE;

	if (AFF_FLAGGED(victim, AFF_NO_ATTACK | AFF_NO_TARGET_IN_ROOM) || AFF_FLAGGED(ch, AFF_NO_ATTACK | AFF_NO_TARGET_IN_ROOM))
		return FALSE;

	// try to hit people through majesty?
	if (CHECK_MAJESTY(victim) && !AFF_FLAGGED(ch, AFF_IMMUNE_VAMPIRE)) {
		return FALSE;
	}

	if (AFF_FLAGGED(victim, AFF_MUMMIFY))
		return FALSE;
	
	if (RMT_FLAGGED(IN_ROOM(victim), RMT_PEACEFUL)) {
		return FALSE;
	}

	if (ROOM_SECT_FLAGGED(IN_ROOM(victim), SECTF_START_LOCATION)) {
		return FALSE;
	}
	
	// CUTOFF: npcs can really always attack
	if (IS_NPC(ch)) {
		return TRUE;
	}
	
	// need empires after this
	ch_emp = GET_LOYALTY(ch);
	victim_emp = GET_LOYALTY(victim);
	
	// rules for attacking an NPC
	if (IS_NPC(victim)) {
		if (!victim_emp) {
			return TRUE;
		}
		if (ch_emp == victim_emp) {
			return TRUE;
		}
		if (has_relationship(ch_emp, victim_emp, DIPL_ALLIED | DIPL_NONAGGR)) {
			return FALSE;
		}
		// any other case
		return TRUE;
	}

	if ((IS_GOD(ch) && !IS_GOD(victim)) || (IS_GOD(victim) && !IS_GOD(ch)))
		return FALSE;

	if (NOHASSLE(ch) || NOHASSLE(victim))
		return FALSE;
	
	// final stop before play-time
	
	// hostile!
	if (IS_HOSTILE(victim)) {
		return TRUE;
	}
	// allow-pvp
	if (IS_PVP_FLAGGED(ch) && IS_PVP_FLAGGED(victim)) {
		return TRUE;
	}
	// is stealing from you
	if (GET_LOYALTY(ch)) {
		LL_FOREACH2(victim->carrying, obj, next_content) {
			if (IS_STOLEN(obj) && obj->last_empire_id == EMPIRE_VNUM(GET_LOYALTY(ch))) {
				return TRUE;	// has at least 1 stolen obj in inventory
			}
		}
	}

	// playtime
	if (!has_one_day_playtime(ch) || !has_one_day_playtime(victim)) {
		return FALSE;
	}

	// cascading order of pk modes
	
	if (IS_SET(pk_ok, PK_FULL))
		return TRUE;

	if (IS_SET(pk_ok, PK_WAR)) {
		if (has_relationship(ch_emp, victim_emp, DIPL_WAR) || has_relationship(victim_emp, ch_emp, DIPL_THIEVERY)) {
			return TRUE;
		}

		/* owned territory */
		if (can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES) && !can_use_room(victim, IN_ROOM(victim), GUESTS_ALLOWED)) {
			return TRUE;
		}
	}

	/* Oops... we shouldn't be fighting */

	if (FIGHTING(ch) == victim)
		stop_fighting(ch);
	if (FIGHTING(victim) == ch)
		stop_fighting(victim);

	return FALSE;
}


/**
* Validate a room target for a siege command.
*
* @param char_data *ch The person firing (Optional; will receive errors and trigger certain checks).
* @param vehicle_data *veh The vehicle firing. (OPTIONAL: Could just be ch firing.)
* @param room_data *to_room The target room.
* @return bool TRUE if okay, FALSE if not.
*/
bool validate_siege_target_room(char_data *ch, vehicle_data *veh, room_data *to_room) {
	room_data *from_room = veh ? IN_ROOM(veh) : (ch ? IN_ROOM(ch) : NULL);
	
	if (ch && ROOM_SECT_FLAGGED(from_room, SECTF_ROUGH)) {
		msg_to_char(ch, "You can't lay siege from rough terrain!\r\n");
	}
	else if (ch && ROOM_IS_CLOSED(from_room)) {
		msg_to_char(ch, "You can't lay siege from indoors.\r\n");
	}
	else if (ch && ROOM_BLD_FLAGGED(from_room, BLD_BARRIER)) {
		msg_to_char(ch, "You can't lay siege from so close to a barrier.\r\n");
	}
	else if (!IS_MAP_BUILDING(to_room)) {
		if (ch) {
			msg_to_char(ch, "That isn't a building.\r\n");
		}
	}
	else if (IS_CITY_CENTER(to_room)) {
		if (ch) {
			msg_to_char(ch, "You can't besiege a city center.\r\n");
		}
	}
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_START_LOCATION)) {
		if (ch) {
			msg_to_char(ch, "You can't besiege a starting location.\r\n");
		}
	}
	else if (ch && ROOM_OWNER(to_room) && ROOM_OWNER(to_room) == GET_LOYALTY(ch)) {
		msg_to_char(ch, "You can't besiege your own property. Abandon it first.\r\n");
	}
	else if (ch && ROOM_OWNER(to_room) && !has_relationship(GET_LOYALTY(ch), ROOM_OWNER(to_room), DIPL_WAR)) {
		msg_to_char(ch, "You can't besiege that direction because you're not at war with %s.\r\n", EMPIRE_NAME(ROOM_OWNER(to_room)));
	}
	else {
		// looks ok!
		return TRUE;
	}
	
	return FALSE;
}


/**
* Validate a vehicle target for a siege command.
*
* @param char_data *ch The person firing (will receive errors).
* @param vehicle_data *veh The vehicle firing. (OPTIONAL: could just be ch firing.)
* @param vehicle_data *target The target vehicle.
* @return bool TRUE if okay, FALSE if not.
*/
bool validate_siege_target_vehicle(char_data *ch, vehicle_data *veh, vehicle_data *target) {
	if (VEH_OWNER(target) && VEH_OWNER(target) == GET_LOYALTY(ch)) {
		msg_to_char(ch, "You can't besiege your own property. Abandon it first.\r\n");
	}
	else if (VEH_OWNER(target) && !has_relationship(GET_LOYALTY(ch), VEH_OWNER(target), DIPL_WAR)) {
		msg_to_char(ch, "You can't besiege that because you're not at war with %s.\r\n", EMPIRE_NAME(VEH_OWNER(target)));
	}
	else {
		// looks ok!
		return TRUE;
	}
	
	return FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// SETTERS / DOERS /////////////////////////////////////////////////////////

/**
* This is used when a character is in a state where they can't be seen, and
* they have done something that cancels those conditions.
*
* @param char_data *ch The person who will appear.
*/
void appear(char_data *ch) {
	affects_from_char_by_aff_flag(ch, AFF_HIDE | AFF_INVISIBLE, FALSE);
	REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE | AFF_INVISIBLE);

	if (GET_ACCESS_LEVEL(ch) < LVL_GOD) {
		act("$n slowly fades into existence.", FALSE, ch, 0, 0, TO_ROOM);
		msg_to_char(ch, "You fade back into view.\r\n");
	}
	else {
		act("You feel a strange presence as $n appears, seemingly from nowhere.", FALSE, ch, 0, 0, TO_ROOM);
	}
}


/**
* For catapults, siege ritual, etc -- damages/destroys a room
*
* @param char_data *ch Optional: The attacker (may be NULL, used for offenses)
* @param room_data *to_room The target room
* @param int damage How much damage to deal to the room
* @param vehicle_data *by_vehicle Optional: Which vehicle gets credit for the damage, if any.
*/
void besiege_room(char_data *attacker, room_data *to_room, int damage, vehicle_data *by_vehicle) {
	static struct resource_data *default_res = NULL;
	char_data *c, *next_c;
	obj_data *o, *next_o;
	empire_data *emp = ROOM_OWNER(to_room);
	struct resource_data *old_list;
	int max_dam;
	bool junk;
	room_data *rm;
	
	// resources if it doesn't have its own
	if (!default_res) {
		add_to_resource_list(&default_res, RES_ACTION, RES_ACTION_REPAIR, 1, 0);
	}
	
	// make sure we only hit the home-room
	to_room = HOME_ROOM(to_room);
	
	// absolutely no damaging city centers
	if (IS_CITY_CENTER(to_room)) {
		return;
	}
	
	// start locations are always safezones
	if (ROOM_SECT_FLAGGED(to_room, SECTF_START_LOCATION)) {
		return;
	}

	if (attacker && emp) {
		add_offense(emp, OFFENSE_SIEGED_BUILDING, attacker, to_room, offense_was_seen(attacker, emp, to_room) ? OFF_SEEN : NOBITS);
	}
	
	if (IS_MAP_BUILDING(to_room)) {
		COMPLEX_DATA(to_room)->damage += damage;
		
		// maximum damage
		max_dam = GET_BLD_MAX_DAMAGE(building_proto(BUILDING_VNUM(to_room)));
		
		// incomplete?
		if (!IS_COMPLETE(to_room)) {
			max_dam = MAX(1, max_dam / 2);
		}
		
		if (BUILDING_DAMAGE(to_room) >= max_dam) {
			disassociate_building(to_room);
			// only abandon outside cities
			if (emp && !is_in_city_for_empire(to_room, emp, TRUE, &junk)) {
				// this does check the city found time so that recently-founded cities don't get abandon protection
				abandon_room(to_room);
			}
			if (ROOM_PEOPLE(to_room)) {
				act("The building is hit and crumbles!", FALSE, ROOM_PEOPLE(to_room), 0, 0, TO_CHAR | TO_ROOM);
			}
		}
		else {	// not over-damaged
			// apply needed maintenance if we did more than 10% damage
			if (GET_BUILDING(to_room) && damage >= (GET_BLD_MAX_DAMAGE(GET_BUILDING(to_room)) / 10)) {
				old_list = GET_BUILDING_RESOURCES(to_room);
				GET_BUILDING_RESOURCES(to_room) = combine_resources(old_list, GET_BLD_YEARLY_MAINTENANCE(GET_BUILDING(to_room)) ? GET_BLD_YEARLY_MAINTENANCE(GET_BUILDING(to_room)) : default_res);
				free_resource_list(old_list);
			}
		
			if (ROOM_PEOPLE(to_room)) {
				act("The building is hit by something and shakes violently!", FALSE, ROOM_PEOPLE(to_room), 0, 0, TO_CHAR | TO_ROOM);
			}
			for (rm = interior_room_list; rm; rm = rm->next_interior) {
				if (HOME_ROOM(rm) == to_room && ROOM_PEOPLE(rm)) {
					act("The building is hit by something and shakes violently!", FALSE, ROOM_PEOPLE(rm), 0, 0, TO_CHAR | TO_ROOM);
				}
			}
			
			// not destroyed
			return;
		}
	}
	else {	// not building/multi	
		if (ROOM_PEOPLE(to_room)) {
			act("The area is under siege!", FALSE, ROOM_PEOPLE(to_room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
	}

	// if we got this far, we need to hurt some people
	for (c = ROOM_PEOPLE(to_room); c; c = next_c) {
		next_c = c->next_in_room;
		if ((GET_DEXTERITY(c) >= 3 && number(0, GET_DEXTERITY(c) / 3)) || IS_IMMORTAL(c)) {
			msg_to_char(c, "You leap out of the way!\r\n");
		}
		else {
			msg_to_char(c, "You are hit and killed!\r\n");
			if (!IS_NPC(c)) {
				log_to_slash_channel_by_name(DEATH_LOG_CHANNEL, c, "%s has been killed by siege damage at (%d, %d)!", PERS(c, c, 1), X_COORD(IN_ROOM(c)), Y_COORD(IN_ROOM(c)));
				syslog(SYS_DEATH, 0, TRUE, "DEATH: %s has been killed by siege damage at %s", GET_NAME(c), room_log_identifier(IN_ROOM(c)));
			}
			run_kill_triggers(c, attacker, by_vehicle);
			die(c, c);
		}
	}

	for (o = ROOM_CONTENTS(to_room); o; o = next_o) {
		next_o = o->next_content;
		if (!number(0, 1)) {
			extract_obj(o);
		}
	}
}


/**
* Does siege damage, which may destroy the vehicle.
*
* @param char_data *attacker Optional: The person sieging (may be NULL, used for offenses)
* @param vehicle_data *veh The vehicle to damage.
* @param int damage How much siege damage to deal.
* @param int siege_type What SIEGE_ damage type.
* @param vehicle_data *by_vehicle Optional: Which vehicle gets credit for the damage, if any.
* @return bool TRUE if the target survives, FALSE if it's extracted
*/
bool besiege_vehicle(char_data *attacker, vehicle_data *veh, int damage, int siege_type, vehicle_data *by_vehicle) {
	void adjust_vehicle_tech(vehicle_data *veh, bool add);
	void fully_empty_vehicle(vehicle_data *veh);

	static struct resource_data *default_res = NULL;
	struct resource_data *old_list;
	struct vehicle_room_list *vrl;
	char_data *ch, *next_ch;
	
	// resources if it doesn't have its own
	if (!default_res) {
		add_to_resource_list(&default_res, RES_OBJECT, o_NAILS, 1, 0);
	}
	
	// no effect
	if (damage <= 0) {
		return TRUE;
	}
	
	if (attacker && VEH_OWNER(veh)) {
		add_offense(VEH_OWNER(veh), OFFENSE_SIEGED_VEHICLE, attacker, IN_ROOM(veh), offense_was_seen(attacker, VEH_OWNER(veh), IN_ROOM(veh)) ? OFF_SEEN : NOBITS);
	}
	
	// deal damage
	VEH_HEALTH(veh) -= damage;
	
	// not dead yet
	if (VEH_HEALTH(veh) > 0) {
		// apply needed maintenance if we did more than 10% damage
		if (damage >= (VEH_MAX_HEALTH(veh) / 10)) {
			old_list = VEH_NEEDS_RESOURCES(veh);
			VEH_NEEDS_RESOURCES(veh) = combine_resources(old_list, VEH_YEARLY_MAINTENANCE(veh) ? VEH_YEARLY_MAINTENANCE(veh) : default_res);
			free_resource_list(old_list);
		}
		
		// SIEGE_x: warn the occupants
		switch (siege_type) {
			case SIEGE_BURNING: {
				msg_to_vehicle(veh, FALSE, "Your skin blisters as %s burns around you!", VEH_SHORT_DESC(veh));
				break;
			}
			case SIEGE_PHYSICAL:
			case SIEGE_MAGICAL:
			default: {
				msg_to_vehicle(veh, FALSE, "You shake with %s as it is besieged!", VEH_SHORT_DESC(veh));
				break;
			}
		}
	}
	else {
		// return 0 prevents the destruction
		if (!destroy_vtrigger(veh)) {
			VEH_HEALTH(veh) = MAX(1, VEH_HEALTH(veh));	// ensure health
			REMOVE_BIT(VEH_FLAGS(veh), VEH_ON_FIRE);	// cancel fire
			return TRUE;
		}

		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			// SIEGE_x: warn the occupants
			switch (siege_type) {
				case SIEGE_BURNING: {
					act("$V burns down!", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM);
					break;
				}
				case SIEGE_PHYSICAL:
				case SIEGE_MAGICAL:
				default: {
					act("$V is destroyed!", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM);
					break;
				}
			}
		}
		
		if (VEH_ROOM_LIST(veh)) {
			LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
				LL_FOREACH_SAFE2(ROOM_PEOPLE(vrl->room), ch, next_ch, next_in_room) {
					act("You are killed as $V is destroyed!", FALSE, ch, NULL, veh, TO_CHAR);
					if (!IS_NPC(ch)) {
						log_to_slash_channel_by_name(DEATH_LOG_CHANNEL, ch, "%s has been killed by siege damage at (%d, %d)!", PERS(ch, ch, TRUE), X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
						syslog(SYS_DEATH, 0, TRUE, "DEATH: %s has been killed by siege damage at %s", GET_NAME(ch), room_log_identifier(IN_ROOM(ch)));
					}
					run_kill_triggers(ch, attacker, by_vehicle);
					die(ch, ch);
				}
			}
		}
		if (VEH_SITTING_ON(veh)) {
			act("You are killed as $V is destroyed!", FALSE, VEH_SITTING_ON(veh), NULL, veh, TO_CHAR);
			if (!IS_NPC(VEH_SITTING_ON(veh))) {
				log_to_slash_channel_by_name(DEATH_LOG_CHANNEL, VEH_SITTING_ON(veh), "%s has been killed by siege damage at (%d, %d)!", PERS(VEH_SITTING_ON(veh), VEH_SITTING_ON(veh), TRUE), X_COORD(IN_ROOM(veh)), Y_COORD(IN_ROOM(veh)));
				syslog(SYS_DEATH, 0, TRUE, "DEATH: %s has been killed by siege damage at %s", GET_NAME(VEH_SITTING_ON(veh)), room_log_identifier(IN_ROOM(veh)));
			}
			run_kill_triggers(VEH_SITTING_ON(veh), attacker, by_vehicle);
			die(VEH_SITTING_ON(veh), VEH_SITTING_ON(veh));
		}
		
		if (VEH_OWNER(veh)) {
			// do this before removing it from room
			adjust_vehicle_tech(veh, FALSE);
		}
		
		vehicle_from_room(veh);	// remove from room first to destroy anything inside
		fully_empty_vehicle(veh);
		
		if (VEH_OWNER(veh) && VEH_IS_COMPLETE(veh)) {
			qt_empire_players(VEH_OWNER(veh), qt_lose_vehicle, VEH_VNUM(veh));
			et_lose_vehicle(VEH_OWNER(veh), VEH_VNUM(veh));
		}
		
		extract_vehicle(veh);
		return FALSE;
	}
	
	return TRUE;
}


/**
* Checks for helpers in the room to start combat too.
*
* @param char_data *ch The person who needs help!
*/
void check_auto_assist(char_data *ch) {
	void perform_rescue(char_data *ch, char_data *vict, char_data *from, int msg);
	
	char_data *ch_iter, *next_iter, *iter_master;
	bool assist;
	
	// sanity
	if (!ch || !FIGHTING(ch)) {
		return;
	}

	for (ch_iter = ROOM_PEOPLE(IN_ROOM(ch)); ch_iter; ch_iter = next_iter) {
		next_iter = ch_iter->next_in_room;
		iter_master = (ch_iter->master ? ch_iter->master : ch_iter);
		assist = FALSE;
		
		// it's possible for this to drop mid-combat
		if (!FIGHTING(ch)) {
			break;
		}
		
		// already busy
		if (ch == ch_iter || FIGHTING(ch) == ch_iter || GET_POS(ch_iter) < POS_STANDING || FIGHTING(ch_iter) || AFF_FLAGGED(ch_iter, AFF_STUNNED | AFF_HARD_STUNNED | AFF_NO_ATTACK) || IS_INJURED(ch_iter, INJ_TIED | INJ_STAKED) || !CAN_SEE(ch_iter, FIGHTING(ch)) || GET_FEEDING_FROM(ch_iter) || GET_FED_ON_BY(ch_iter)) {
			continue;
		}
		
		// champion
		if (MOB_FLAGGED(ch_iter, MOB_CHAMPION) && iter_master == ch && FIGHTING(ch) && FIGHTING(FIGHTING(ch)) == ch && IS_NPC(ch_iter)) {
			if (FIGHT_MODE(FIGHTING(ch)) == FMODE_MELEE) {
				// can rescue only in melee
				perform_rescue(ch_iter, ch, FIGHTING(ch), RESCUE_RESCUE);
			}
			// else { champion but not in melee? just fall through to the continue
			continue;
		}
		
		// things which stop normal auto-assist but not champion or follower
		if (MOB_FLAGGED(ch_iter, MOB_NO_ATTACK) && iter_master != ch) {
			continue;
		}
		
		if (!IS_NPC(ch_iter) && in_same_group(ch, ch_iter)) {
			// party assist
			assist = TRUE;
		}		
		else if (IS_NPC(ch) && IS_NPC(ch_iter) && MOB_FLAGGED(ch_iter, MOB_BRING_A_FRIEND) && !AFF_FLAGGED(ch_iter, AFF_CHARM) && !AFF_FLAGGED(ch, AFF_CHARM)) {
			// BAF (if both are NPCs and neither is charmed)
			assist = TRUE;
		}
		else if (iter_master == ch && AFF_FLAGGED(ch_iter, AFF_CHARM)) {
			// charm
			assist = TRUE;
		}
		
		// if we got this far and hit an assist condition
		if (assist && can_fight(ch_iter, FIGHTING(ch))) {
			act("You jump to $N's aid!", FALSE, ch_iter, 0, ch, TO_CHAR);
			act("$n jumps to your aid!", FALSE, ch_iter, 0, ch, TO_VICT);
			act("$n jumps to $N's aid!", FALSE, ch_iter, 0, ch, TO_NOTVICT);
			engage_combat(ch_iter, FIGHTING(ch), FALSE);
			continue;
		}
	}
}


/**
* Checks that a character's position is valid for combat rounds.
*
* @param char_data *ch who to check
* @param double speed the speed this happened at, in seconds
* @return bool TRUE if position is ok, FALSE to abort
*/
bool check_combat_position(char_data *ch, double speed) {
	// auto-dismount in combat
	if (IS_RIDING(ch)) {
		msg_to_char(ch, "You jump down from your mount.\r\n");
		perform_dismount(ch);
		// does not block combat round
	}

	// check mob wait and position
	if (IS_NPC(ch)) {
		if (GET_WAIT_STATE(ch) > 0) {
			GET_WAIT_STATE(ch) -= (int) (speed RL_SEC);
			return FALSE;
		}
		GET_WAIT_STATE(ch) = 0;
		
		if (GET_POS(ch) < POS_FIGHTING) {
			GET_POS(ch) = POS_FIGHTING;
			act("$n scrambles to $s feet!", TRUE, ch, 0, 0, TO_ROOM);
			return FALSE;
		}
	}
	
	// feeding/fed-on prevent combat
	if (GET_FEEDING_FROM(ch) || GET_FED_ON_BY(ch)) {
		return FALSE;
	}

	// check position
	if (GET_POS(ch) < POS_FIGHTING) {
		send_to_char("You can't fight while sitting!!\r\n", ch);
		return FALSE;
	}
	
	return TRUE;
}


/*
 *	< 0	Victim died.
 *	= 0	No damage.
 *	> 0	How much damage done.
 */
int damage(char_data *ch, char_data *victim, int dam, int attacktype, byte damtype) {
	void start_drinking_blood(char_data *ch, char_data *victim);
	extern const struct wear_data_type wear_data[NUM_WEARS];
	
	struct instance_data *inst;
	int iter;
	bool full_miss = (dam <= 0);
	
	if (GET_POS(victim) <= POS_DEAD) {
	    /* This is "normal"-ish now with delayed extraction. -gg 3/15/2001 */
	    if (EXTRACTED(victim) || IS_DEAD(victim)) {
	    	return -1;
	    }

		log("SYSERR: Attempt to damage corpse '%s' in room #%d by '%s'.", GET_NAME(victim), GET_ROOM_VNUM(IN_ROOM(victim)), GET_NAME(ch));
		die(victim, ch);
		return (-1);			/* -je, 7/7/92 */
	}
	
	// look for an instance to lock (before running triggers)
	if (!IS_NPC(ch) && IS_ADVENTURE_ROOM(IN_ROOM(ch)) && COMPLEX_DATA(IN_ROOM(ch)) && (inst = COMPLEX_DATA(IN_ROOM(ch))->instance)) {
		if (ADVENTURE_FLAGGED(INST_ADVENTURE(inst), ADV_LOCK_LEVEL_ON_COMBAT) && !IS_IMMORTAL(ch)) {
			lock_instance_level(IN_ROOM(ch), determine_best_scale_level(ch, TRUE));
		}
	}
	
	check_scaling(victim, ch);

	/* You can't damage an immortal! */
	if (!IS_NPC(victim) && (IS_IMMORTAL(victim) || (IS_GOD(victim) && !IS_GOD(ch))))
		dam = 0;

	if (GET_MOB_VNUM(ch) != DG_CASTER_PROXY && ch != victim && !can_fight(ch, victim))
		return 0;

	/* Only damage to self (sun) still hurts */
	if (AFF_FLAGGED(victim, AFF_IMMUNE_PHYSICAL) && damtype == DAM_PHYSICAL)
		dam = 0;
	
	// full immunity
	if (AFF_FLAGGED(victim, AFF_IMMUNE_DAMAGE)) {
		dam = 0;
	}

	if (victim != ch) {
		/* Start the attacker fighting the victim */
		if (GET_POS(ch) > POS_STUNNED && (FIGHTING(ch) == NULL))
			set_fighting(ch, victim, FMODE_MELEE);

		/* Start the victim fighting the attacker */
		if (GET_POS(victim) > POS_STUNNED && (FIGHTING(victim) == NULL)) {
			unsigned long long timestamp = microtime();
			set_fighting(victim, ch, FMODE_MELEE);
			GET_LAST_SWING_MAINHAND(victim) = timestamp - (get_combat_speed(victim, WEAR_WIELD)/2 SEC_MICRO);	// half-round time offset
			GET_LAST_SWING_OFFHAND(victim) = timestamp - (get_combat_speed(victim, WEAR_HOLD)/2 SEC_MICRO);	// half-round time offset
		}
	}

	/* If you attack a pet, it hates your guts */
	if (victim->master == ch) {
		stop_follower(victim);
	}
	if (ch->master == victim) {
		stop_follower(ch);	// don't allow following of people while fighting them
	}

	/* If the attacker is invisible, he becomes visible */
	if (SHOULD_APPEAR(ch))
		appear(ch);
		
	dam = reduce_damage_from_skills(dam, victim, ch, damtype);
	
	// lethal damage?? check Master Survivalist
	if ((ch != victim) && dam >= GET_HEALTH(victim) && !IS_NPC(victim) && AWAKE(victim) && has_ability(victim, ABIL_MASTER_SURVIVALIST)) {
		if (!number(0, 2)) {
			msg_to_char(victim, "You dive out of the way at the last second!\r\n");
			act("$n dives out of the way at the last second!", FALSE, victim, NULL, NULL, TO_ROOM);
			dam = 0;
			// wait is based on % of max dexterity, where it's longest when you have 1 dex and shortest at max
			GET_WAIT_STATE(victim) = (3 RL_SEC * (1.0 - ((double) GET_DEXTERITY(victim) / (att_max(victim) + 1))));
		}
		
		gain_ability_exp(victim, ABIL_MASTER_SURVIVALIST, 33.4);
	}

	/* Minimum damage of 0.. can't do negative */
	dam = MAX(0, dam);

	// Add Damage
	GET_HEALTH(victim) = GET_HEALTH(victim) - dam;
	update_pos(victim);
	
	if (ch != victim) {
		combat_meter_damage_dealt(ch, dam);
	}
	combat_meter_damage_taken(victim, dam);
	
	/*
	 * skill_message sends a message from the messages file in lib/misc.
	 * dam_message just sends a generic "You hit $n extremely hard.".
	 * skill_message is preferable to dam_message because it is more
	 * descriptive.
	 * 
	 * If we are _not_ attacking with a weapon (i.e. a spell), always use
	 * skill_message. If we are attacking with a weapon: If this is a miss or a
	 * death blow, send a skill_message if one exists; if not, default to a
	 * dam_message. Otherwise, always send a dam_message.
	 */
	if (!IS_WEAPON_TYPE(attacktype))
		skill_message(dam, ch, victim, attacktype);
	else {
		if (dam == 0 || ch == victim || (GET_POS(victim) == POS_DEAD && WOULD_EXECUTE(ch, victim))) {
			if (!skill_message(dam, ch, victim, attacktype))
				dam_message(dam, ch, victim, attacktype);
		}
		else
			dam_message(dam, ch, victim, attacktype);
	}

	/* Use send_to_char -- act() doesn't send message if you are DEAD. */
	switch (GET_POS(victim)) {
		case POS_MORTALLYW:
			act("$n is mortally wounded, and will die soon, if not aided.", TRUE, victim, 0, 0, TO_ROOM);
			send_to_char("You are mortally wounded, and will die soon, if not aided.\r\n", victim);
			break;
		case POS_INCAP:
			act("$n is incapacitated and will slowly die, if not aided.", TRUE, victim, 0, 0, TO_ROOM);
			send_to_char("You are incapacitated and will slowly die, if not aided.\r\n", victim);
			break;
		case POS_STUNNED:
			act("$n is stunned, but will probably regain consciousness again.", TRUE, victim, 0, 0, TO_ROOM);
			send_to_char("You're stunned, but will probably regain consciousness again.\r\n", victim);
			break;
		case POS_DEAD:
			/* These messages will be handled in perform_execute */
			break;
		default:			/* >= POSITION SLEEPING */
			if (dam > (GET_MAX_HEALTH(victim) / 10))
				send_to_char("&rThat really did HURT!&0\r\n", victim);

			if (GET_HEALTH(victim) <= (GET_MAX_HEALTH(victim) / 20))
				msg_to_char(victim, "&rYou wish that your wounds would stop BLEEDING so much!&0\r\n");

			break;
	}
	
	// did we do any damage? tag the mob
	if (dam > 0) {
		tag_mob(victim, ch);
	}
	
	// skill gains when you take damage
	if (!full_miss && !IS_NPC(victim) && ch != victim && !EXTRACTED(victim)) {
		// endurance (extra HP)
		if (can_gain_exp_from(victim, ch)) {
			gain_ability_exp(victim, ABIL_ENDURANCE, 2);
			run_ability_gain_hooks(victim, ch, AGH_TAKE_DAMAGE);
		}

		// armor skills
		for (iter = 0; iter < NUM_WEARS; ++iter) {
			if (wear_data[iter].count_stats && GET_EQ(victim, iter) && GET_ARMOR_TYPE(GET_EQ(victim, iter)) != NOTHING && can_gain_exp_from(victim, ch)) {
				switch (GET_ARMOR_TYPE(GET_EQ(victim, iter))) {
					case ARMOR_MAGE: {
						gain_player_tech_exp(victim, PTECH_ARMOR_MAGE, 2);
						break;
					}
					case ARMOR_LIGHT: {
						gain_player_tech_exp(victim, PTECH_ARMOR_LIGHT, 2);
						break;
					}
					case ARMOR_MEDIUM: {
						gain_player_tech_exp(victim, PTECH_ARMOR_MEDIUM, 2);
						break;
					}
					case ARMOR_HEAVY: {
						gain_player_tech_exp(victim, PTECH_ARMOR_HEAVY, 2);
						break;
					}
				}
			}
		}
	
		if (affected_by_spell(victim, ATYPE_MANASHIELD) && can_gain_exp_from(victim, ch)) {
			gain_ability_exp(victim, ABIL_MANASHIELD, 2);
		}
	}
	
	// final cleanup

	/* Help out poor linkless people who are attacked */
	if (!IS_NPC(victim) && !(victim->desc) && GET_POS(victim) > POS_STUNNED)
		do_flee(victim, NULL, 0, 0);

	/* stop someone from fighting if they're stunned or worse */
	if ((GET_POS(victim) <= POS_STUNNED) && (FIGHTING(victim) != NULL))
		stop_fighting(victim);

	/* Uh oh.  Victim died. */
	if (GET_POS(victim) == POS_DEAD) {
		if (attacktype == ATTACK_VAMPIRE_BITE && ch != victim && !AFF_FLAGGED(victim, AFF_NO_DRINK_BLOOD) && !GET_FEEDING_FROM(ch) && IN_ROOM(ch) == IN_ROOM(victim)) {
			GET_HEALTH(victim) = 0;
			GET_POS(victim) = POS_STUNNED;
			start_drinking_blood(ch, victim);
			// fall through to return dam below
		}
		else {
			perform_execute(ch, victim, attacktype, damtype);
			return -1;
		}
	}
	else if (ch != victim && GET_POS(victim) < POS_SLEEPING && !WOULD_EXECUTE(ch, victim)) {
		// SHOULD this also remove DoTs etc? -paul 5/6/2017
		
		stop_combat_no_autokill(victim);
		return -1;	// prevents other damage/effects
		// no need to message here; they already got a pos message
	}
	
	return dam;
}


/**
* Adds a death log to a character who is about to die.
*
* @param char_data *ch The person dying.
* @param char_data *killer The person who killed him.
* @param int type An ATTACK_ or TYPE_ damage type that killed ch (e.g. ATTACK_EXECUTE).
*/
void death_log(char_data *ch, char_data *killer, int type) {
	char *msg;
	char output[MAX_STRING_LENGTH];
	bool has_killer = FALSE;
	
	// nope
	if (IS_NPC(ch)) {
		return;
	}

	// don't deathlog for phoenix rite -- they won't be dying
	if (affected_by_spell(ch, ATYPE_PHOENIX_RITE)) {
		return;
	}

	// these messages will all be appened with " (x, y)"
	switch (type) {
		case ATTACK_GUARD_TOWER:
			msg = "%s has been killed by a guard tower at";
			break;
		case TYPE_SUFFERING:
			msg = "%s has died at";
			break;
		case ATTACK_EXECUTE:
			msg = "%s has been killed by %s at";
			has_killer = TRUE;
			break;
		default:
			msg = "%s has been killed at";
			break;
	}

	if (ch == killer && has_killer) {
		syslog(SYS_ERROR, 0, TRUE, "Unusual error in death_log(): ch == killer, but combat attack type passed");
		msg = "%s has been killed at";
		has_killer = FALSE;
	}

	if (has_killer) {
		strcpy(buf, PERS(killer, killer, 1));
		sprintf(output, msg, PERS(ch, ch, 1), buf);
	}
	else
		sprintf(output, msg, PERS(ch, ch, 1));

	log_to_slash_channel_by_name(DEATH_LOG_CHANNEL, ch, "%s (%d, %d)", output, X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
	syslog(SYS_DEATH, 0, TRUE, "DEATH: %s %s", output, room_log_identifier(IN_ROOM(ch)));
}


/**
* Simple method to make sure people are fighting each other.
* ch/vict order are not significant here
*
* @param char_data *ch
* @param char_data *vict
* @param bool melee if TRUE goes straight to melee; otherwise WAITING
*/
void engage_combat(char_data *ch, char_data *vict, bool melee) {
	/* Don't bother with can-fight here -- it has certainly been pre-checked before anything that would engage combat
	if (!can_fight(ch, vict)) {
		return;
	}
	*/

	// this prevents players from using things that engage combat over and over
	if (GET_POS(vict) == POS_INCAP && GET_POS(ch) >= POS_FIGHTING) {
		perform_execute(ch, vict, TYPE_UNDEFINED, DAM_PHYSICAL);
		return;
	}
	else if (GET_POS(ch) == POS_INCAP && GET_POS(vict) >= POS_FIGHTING) {
		perform_execute(vict, ch, TYPE_UNDEFINED, DAM_PHYSICAL);
		return;
	}

	if (!FIGHTING(ch) && AWAKE(ch)) {
		set_fighting(ch, vict, melee ? FMODE_MELEE : FMODE_WAITING);
	}
	if (!FIGHTING(vict) && AWAKE(vict)) {
		unsigned long long timestamp = microtime();
		set_fighting(vict, ch, melee ? FMODE_MELEE : FMODE_WAITING);
		
		GET_LAST_SWING_MAINHAND(vict) = timestamp - (get_combat_speed(vict, WEAR_WIELD)/2 SEC_MICRO);	// half-round time offset
		GET_LAST_SWING_OFFHAND(vict) = timestamp - (get_combat_speed(vict, WEAR_HOLD)/2 SEC_MICRO);	// half-round time offset
	}
}


/**
* Apply healing. Does not work on dead players.
*
* @param char_data *ch The person casting/providing the heal
* @param char_data *vict The person receiving the heal
* @param int amount How much to heal
*/
void heal(char_data *ch, char_data *vict, int amount) {
	// cannot heal the dead
	if (IS_DEAD(vict)) {
		return;
	}
	
	// no negative healing
	amount = MAX(0, amount);
	
	combat_meter_heal_dealt(ch, amount);
	combat_meter_heal_taken(vict, amount);
	
	// apply heal
	GET_HEALTH(vict) = MIN(GET_MAX_HEALTH(vict), GET_HEALTH(vict) + amount);
	
	if (GET_POS(vict) <= POS_STUNNED) {
		update_pos(vict);
	}
	
	// check recovery
	if (GET_POS(vict) < POS_SLEEPING && GET_HEALTH(vict) > 0) {
		msg_to_char(vict, "You recover and wake up.\r\n");
		GET_POS(vict) = IS_NPC(vict) ? POS_STANDING : POS_SITTING;
		end_pursuit(vict, ch);	// good samaritan
	}
}


/**
* @param char_data *ch The hitter.
* @param char_data *victim The target.
* @param obj_data *weapon The weapon ch will use (usually what's in WEAR_WIELD, but could be anything).
* @param bool combat_round if TRUE, can gain skills like a combat round.
* @return int the result of damage() or -1
*/
int hit(char_data *ch, char_data *victim, obj_data *weapon, bool combat_round) {
	void add_pursuit(char_data *ch, char_data *target);
	extern int apply_poison(char_data *ch, char_data *vict);
	extern const double basic_speed;
	
	struct instance_data *inst;
	int w_type, result, bonus, ret_val;
	bool success = FALSE, block = FALSE;
	bool can_gain_skill;
	empire_data *victim_emp;
	struct affected_type *af;
	double attack_speed, cur_speed, dam;
	char_data *check;
	
	// some config TODO move this into the config system?
	int cut_deep_durations[] = { 3, 3, 6 };
	int stunning_blow_durations[] = { 1, 1, 2 };
	double big_game_hunter[] = { 1.05, 1.05, 1.10 };

	// brief sanity
	if (!victim || !ch || EXTRACTED(victim) || IS_DEAD(victim)) {
		return -1;
	}
	
	// ensure meters started
	check_combat_start(ch);
	check_combat_start(victim);
	
	// set up some vars
	w_type = get_attack_type(ch, weapon);
	victim_emp = GET_LOYALTY(victim);
	
	// weapons not allowed if disarmed (do this after get_attack type, which accounts for this)
	if (AFF_FLAGGED(ch, AFF_DISARM)) {
		weapon = NULL;
	}
	
	// determine speeds now
	attack_speed = get_base_speed(ch, weapon ? weapon->worn_on : WEAR_WIELD);
	cur_speed = get_combat_speed(ch, weapon ? weapon->worn_on : WEAR_WIELD);
	
	// look for an instance to lock (before running triggers)
	if (!IS_NPC(ch) && IS_ADVENTURE_ROOM(IN_ROOM(ch)) && COMPLEX_DATA(IN_ROOM(ch)) && (inst = COMPLEX_DATA(IN_ROOM(ch))->instance)) {
		if (ADVENTURE_FLAGGED(INST_ADVENTURE(inst), ADV_LOCK_LEVEL_ON_COMBAT) && !IS_IMMORTAL(ch)) {
			lock_instance_level(IN_ROOM(ch), determine_best_scale_level(ch, TRUE));
		}
	}

	/* check if the character has a fight trigger */
	fight_mtrigger(ch);
	
	// some mobs can run fight triggers when they are hitting, but never actually hit
	if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_NO_ATTACK)) {
		return 0;
	}
	
	// hostile activity triggers distrust unless the victim is pvp-flagged or already hostile
	if (!IS_NPC(ch) && victim_emp && GET_LOYALTY(ch) != victim_emp) {
		// we check the victim's master if it's an NPC and the master is a PC
		check = (IS_NPC(victim) && victim->master && !IS_NPC(victim->master)) ? victim->master : victim;
		if ((IS_NPC(check) || !IS_PVP_FLAGGED(check)) && !IS_HOSTILE(check) && (!ROOM_OWNER(IN_ROOM(ch)) || ROOM_OWNER(IN_ROOM(ch)) != GET_LOYALTY(ch))) {
			trigger_distrust_from_hostile(ch, victim_emp);
		}
	}
	
	// ensure scaling
	check_scaling(victim, ch);

	/* Do some sanity checking, in case someone flees, etc. */
	if (IN_ROOM(ch) != IN_ROOM(victim)) {
		if (FIGHTING(ch) && FIGHTING(ch) == victim) {
			stop_fighting(ch);
		}
		return -1;
	}
	
	// mark for pursuit
	if (IS_NPC(victim) && MOB_FLAGGED(victim, MOB_PURSUE)) {
		add_pursuit(victim, ch);
	}
	
	// ensure this isn't someone continuing to attack someone who has been
	// incapacitated by turning off auto-kill
	if (GET_POS(victim) == POS_INCAP) {
		perform_execute(ch, victim, TYPE_UNDEFINED, DAM_PHYSICAL);
		if (IS_DEAD(victim)) {
			return -1;
		}
	}
	
	// no gaining skill if there's no damage to do
	can_gain_skill = GET_HEALTH(victim) > 0;
	
	// determine hit (if WEAR_HOLD, pass off_hand=TRUE)
	success = check_hit_vs_dodge(ch, victim, (weapon && weapon->worn_on == WEAR_HOLD));
	
	// blockable?
	if (success && AWAKE(victim)) {
		if (attack_hit_info[w_type].damage_type == DAM_PHYSICAL) {
			block = check_block(victim, ch, TRUE);
		}
		else if (has_player_tech(victim, PTECH_BLOCK_MAGICAL) && check_solo_role(victim) && attack_hit_info[w_type].damage_type == DAM_MAGICAL) {
			// half-chance
			block = check_block(victim, ch, TRUE) && !number(0, 1);
			gain_player_tech_exp(victim, PTECH_BLOCK_MAGICAL, 2);
		}
	}

	// outcome:
	if (!success) {
		// miss
		combat_meter_miss(ch);
		combat_meter_dodge(victim);
		ret_val = damage(ch, victim, 0, w_type, attack_hit_info[w_type].damage_type);
		if (can_gain_exp_from(victim, ch)) {
			run_ability_gain_hooks(victim, ch, AGH_DODGE);
		}
		return ret_val;
	}
	else if (block) {
		combat_meter_miss(ch);
		combat_meter_block(victim);
		block_attack(ch, victim, w_type);
		if (can_gain_exp_from(victim, ch)) {
			run_ability_gain_hooks(victim, ch, AGH_BLOCK);
		}
		return 0;
	}
	else {
		/* okay, we know the guy has been hit.  now calculate damage. */
		combat_meter_hit(ch);
		combat_meter_hit_taken(victim);

		// TODO move this damage computation to its own function so that it can be called remotely too

		dam = 0;	// to start
		
		if (weapon && IS_WEAPON(weapon)) {
			/* Add weapon-based damage if a weapon is being wielded */
			dam += GET_WEAPON_DAMAGE_BONUS(weapon);
		}
		
		if (IS_NPC(ch)) {
			dam += MOB_DAMAGE(ch) / (AFF_FLAGGED(ch, AFF_DISARM) ? 2 : 1);
		}
		
		// applicable bonuses
		if (IS_MAGIC_ATTACK(w_type)) {
			bonus = GET_INTELLIGENCE(ch) + GET_BONUS_MAGICAL(ch);
		}
		else {
			bonus = GET_STRENGTH(ch) + GET_BONUS_PHYSICAL(ch);
		}
		
		// bonus add is based on speeds
		dam += bonus * (attack_speed / basic_speed) * (attack_speed / cur_speed);
		
		// TODO this is probably WAY overpowered. WAY WAY. Consider changing it to a 'ready' weapon.
		if (w_type == TYPE_VAMPIRE_CLAWS) {
			dam *= 2;
		}
		
		// All these abilities add damage: no skill gain on an already-beated foe
		if (can_gain_skill) {
			if (can_gain_exp_from(ch, victim)) {
				run_ability_gain_hooks(ch, victim, AGH_MELEE);
			}
			
			if (!IS_NPC(ch) && has_ability(ch, ABIL_DAGGER_MASTERY) && weapon && GET_WEAPON_TYPE(weapon) == TYPE_STAB) {
				dam *= 1.5;
				if (can_gain_exp_from(ch, victim)) {
					gain_ability_exp(ch, ABIL_DAGGER_MASTERY, 2);
				}
			}
			if (!IS_NPC(ch) && has_ability(ch, ABIL_STAFF_MASTERY) && weapon && IS_STAFF(weapon)) {
				dam *= 1.5;
				if (can_gain_exp_from(ch, victim)) {
					gain_ability_exp(ch, ABIL_STAFF_MASTERY, 2);
				}
			}	
			if (!IS_NPC(ch) && has_ability(ch, ABIL_CLAWS) && w_type == TYPE_VAMPIRE_CLAWS && can_gain_exp_from(ch, victim)) {
				gain_ability_exp(ch, ABIL_CLAWS, 2);
			}
			if (!IS_NPC(ch) && GET_EQ(ch, WEAR_WIELD) && IS_BLOOD_WEAPON(GET_EQ(ch, WEAR_WIELD)) && w_type == GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) && can_gain_exp_from(ch, victim)) {
				// this verifies w_type to make sure it's a normal weapon attack
				gain_ability_exp(ch, ABIL_READY_BLOOD_WEAPONS, 2);
			}
			
			// raw damage modified by hunt
			if (IS_NPC(victim) && MOB_FLAGGED(victim, MOB_ANIMAL) && has_player_tech(ch, PTECH_BONUS_VS_ANIMALS)) {
				dam *= 2;
			}
		
			// raw damage modified by big game hunter
			if (!IS_NPC(ch) && has_ability(ch, ABIL_BIG_GAME_HUNTER) && (IS_VAMPIRE(victim) || IS_MAGE(victim))) {
				if (can_gain_exp_from(ch, victim)) {
					gain_ability_exp(ch, ABIL_BIG_GAME_HUNTER, 1);
				}
		
				if (skill_check(ch, ABIL_BIG_GAME_HUNTER, DIFF_MEDIUM)) {
					dam = (int) (CHOOSE_BY_ABILITY_LEVEL(big_game_hunter, ch, ABIL_BIG_GAME_HUNTER) * dam);
				}
			}
		}
		
		dam = round(dam);
		dam = MAX(0, dam);

		// anything after this must NOT rely on victim being alive
		result = damage(ch, victim, (int) dam, w_type, attack_hit_info[w_type].damage_type);
		
		// exp gain
		if (combat_round && can_gain_skill && can_gain_exp_from(ch, victim)) {
			if (!IS_NPC(ch)) {
				if (attack_hit_info[w_type].disarmable) {
					gain_ability_exp(ch, ABIL_WEAPON_PROFICIENCY, 5);
				}
				gain_ability_exp(ch, ABIL_FINESSE, 2);
			
				// fireball skill gain
				if (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_VNUM(GET_EQ(ch, WEAR_WIELD)) == o_FIREBALL) {
					gain_ability_exp(ch, ABIL_READY_FIREBALL, 2);
				}
			}
		}
		if (result >= 0 && combat_round && can_gain_skill && !IS_NPC(victim) && can_gain_exp_from(victim, ch)) {
			gain_ability_exp(victim, ABIL_EVASION, 5);
		}
		
		/* check if the victim has a hitprcnt trigger */
		if (result > 0 && !EXTRACTED(victim) && !IS_DEAD(victim) && IN_ROOM(victim) == IN_ROOM(ch)) {
			hitprcnt_mtrigger(victim);
		}
		
		// check post-hit skills
		if (result > 0 && !EXTRACTED(victim) && !IS_DEAD(victim) && IN_ROOM(victim) == IN_ROOM(ch)) {
			// cut deep: players only
			if (!IS_NPC(ch) && !AFF_FLAGGED(victim, AFF_IMMUNE_BATTLE) && skill_check(ch, ABIL_CUT_DEEP, DIFF_RARELY) && weapon && attack_hit_info[w_type].weapon_type == WEAPON_SHARP) {
				apply_dot_effect(victim, ATYPE_CUT_DEEP, CHOOSE_BY_ABILITY_LEVEL(cut_deep_durations, ch, ABIL_CUT_DEEP), DAM_PHYSICAL, 5, 5, ch);
				
				act("You cut deep wounds in $N -- $E is bleeding!", FALSE, ch, NULL, victim, TO_CHAR);
				act("$n's last attack cuts deep -- you are bleeding!", FALSE, ch, NULL, victim, TO_VICT);
				act("$n's last attack cuts deep -- $N is bleeding!", FALSE, ch, NULL, victim, TO_NOTVICT);

				if (can_gain_skill && can_gain_exp_from(ch, victim)) {
					gain_ability_exp(ch, ABIL_CUT_DEEP, 10);
				}
			}
		
			// stunning blow: players only
			if (!IS_NPC(ch) && !AFF_FLAGGED(victim, AFF_IMMUNE_BATTLE | AFF_IMMUNE_STUN) && skill_check(ch, ABIL_STUNNING_BLOW, DIFF_RARELY) && weapon && attack_hit_info[w_type].weapon_type == WEAPON_BLUNT) {
				af = create_flag_aff(ATYPE_STUNNING_BLOW, CHOOSE_BY_ABILITY_LEVEL(stunning_blow_durations, ch, ABIL_STUNNING_BLOW), AFF_STUNNED, ch);
				affect_join(victim, af, 0);
				
				act("That last blow seems to stun $N!", FALSE, ch, NULL, victim, TO_CHAR);
				act("$n's last blow hit you hard! You're knocked to the floor, stunned.", FALSE, ch, NULL, victim, TO_VICT);
				act("$n's last blow seems to stun $N!", FALSE, ch, NULL, victim, TO_NOTVICT);

				if (can_gain_skill && can_gain_exp_from(ch, victim)) {
					gain_ability_exp(ch, ABIL_STUNNING_BLOW, 10);
				}
			}
			
			// poison could kill too
			if (!IS_NPC(ch) && has_player_tech(ch, PTECH_POISON) && weapon && attack_hit_info[w_type].weapon_type == WEAPON_SHARP) {
				if (!number(0, 1) && apply_poison(ch, victim) < 0) {
					// dedz
					result = -1;
				}
			}
		}
		
		return result;
	}
}


/**
* Kills a character from lack of blood.
*
* I'm not sure this really goes in fight.c but I also am not sure where else
* it would go. -pc
*
* @param char_data *ch The person to kill.
*/
void out_of_blood(char_data *ch) {
	msg_to_char(ch, "You die from lack of blood!\r\n");
	act("$n falls down, dead.", FALSE, ch, 0, 0, TO_ROOM);
	death_log(ch, ch, TYPE_SUFFERING);
	die(ch, ch);
}


/**
* Actual execute -- sometimes this exits early by knocking the victim
* unconscious, unless the execute is deliberate.
*
* @param char_data *ch The murderer.
* @param char_data *victim The unfortunate one.
* @param int attacktype Any ATTACK_ or TYPE_.
* @param int damtype DAM_x.
*/
void perform_execute(char_data *ch, char_data *victim, int attacktype, int damtype) {
	bool ok = FALSE;
	bool revert = TRUE;
	char_data *m;
	obj_data *weapon;
	bool msg;

	/* stop_fighting() is split around here to help with exp */

	/* Probably sent here by damage() */
	if (ch == victim)
		ok = TRUE;

	/* Sent here by damage() */
	if (attacktype > NUM_ATTACK_TYPES || WOULD_EXECUTE(ch, victim)) {
		ok = TRUE;
	}
	
	// victim is already incapacitated and we're attacking it again?
	if (GET_POS(victim) == POS_INCAP) {
		ok = TRUE;
	}

	/* Sent here by do_execute() */
	if (attacktype == TYPE_UNDEFINED)
		ok = TRUE;

	if (!ok) {
		stop_combat_no_autokill(victim);
		act("$n is knocked unconscious!", FALSE, victim, 0, 0, TO_ROOM);
		msg_to_char(victim, "You are knocked unconscious.\r\n");
		return;
	}

	/* We will TRY to find a slicing/slashing weapon */
	weapon = GET_EQ(ch, WEAR_WIELD);
	if ((!weapon || (GET_WEAPON_TYPE(weapon) != TYPE_SLASH && GET_WEAPON_TYPE(weapon) != TYPE_SLICE)) && GET_EQ(ch, WEAR_HOLD)) {
		weapon = GET_EQ(ch, WEAR_HOLD);
		if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON || (GET_WEAPON_TYPE(weapon) != TYPE_SLASH && GET_WEAPON_TYPE(weapon) != TYPE_SLICE)) {
			weapon = GET_EQ(ch, WEAR_WIELD);
		}
	}

	if (revert && IS_MORPHED(victim)) {
		sprintf(buf, "%s reverts into $n!", PERS(victim, victim, FALSE));
		msg = !CHAR_MORPH_FLAGGED(victim, MORPHF_NO_MORPH_MESSAGE);

		perform_morph(victim, NULL);
		
		if (msg) {
			act(buf, TRUE, victim, 0, 0, TO_ROOM);
		}
	}
	
	if (ch == victim) {
		act("$n is dead -- R.I.P.", FALSE, victim, NULL, ch, TO_NOTVICT);
	}
	else {
		act("$n is dead, killed by $N! R.I.P.", FALSE, victim, NULL, ch, TO_NOTVICT);
		act("You have killed $n! R.I.P.", FALSE, victim, NULL, ch, TO_VICT);
	}

	// cleanup
	end_pursuit(ch, victim);

	/* logs */
	if (!IS_NPC(victim)) {
		if (ch != victim)
			death_log(victim, ch, ATTACK_EXECUTE);
		else
			death_log(victim, ch, TYPE_SUFFERING);
	}

	/* Actually killed him! */
	if (victim != ch && !IS_NPC(victim)) {
		for (m = ROOM_PEOPLE(IN_ROOM(victim)); m; m = m->next_in_room)
			if (FIGHTING(m) == victim && !IS_NPC(m))
				stop_fighting(m);
	}
	if (FIGHTING(victim))
		stop_fighting(victim);
	if (FIGHTING(ch) == victim)
		stop_fighting(ch);
	msg_to_char(victim, "You are dead! Sorry...\r\n");
	if (!IS_NPC(victim) && !IS_NPC(ch)) {
		if (ch == victim) {
			if (attacktype == ATTACK_GUARD_TOWER) {
				if (IN_ROOM(ch) && ROOM_OWNER(IN_ROOM(ch))) {
					add_lore(ch, LORE_TOWER_DEATH, "Killed by a guard tower on %s%s&0 land", EMPIRE_BANNER(ROOM_OWNER(IN_ROOM(ch))), EMPIRE_ADJECTIVE(ROOM_OWNER(IN_ROOM(ch))));
				}
				else {
					add_lore(ch, LORE_TOWER_DEATH, "Killed by a guard tower");
				}
			}
		}
		else {
			add_lore(ch, LORE_PLAYER_KILL, "Killed %s in battle", PERS(victim, victim, TRUE));
			add_lore(victim, LORE_PLAYER_DEATH, "Slain by %s in battle", PERS(ch, ch, TRUE));
		}
	}
	
	die(victim, ch);	// returns a corpse, but we don't need it here
}


/* start one char fighting another (yes, it is horrible, I know... )  */
void set_fighting(char_data *ch, char_data *vict, byte mode) {
	if (ch == vict)
		return;

	if (FIGHTING(ch))
		return;
	
	// look for possible offense (if vict is in an empire and is not already fighting ch)
	if (!IS_NPC(ch) && GET_LOYALTY(vict) && FIGHTING(vict) != ch) {
		if (!IS_NPC(vict) && !IS_PVP_FLAGGED(vict)) {
			add_offense(GET_LOYALTY(vict), OFFENSE_ATTACKED_PLAYER, ch, IN_ROOM(ch), OFF_SEEN);
		}
		else if (IS_NPC(vict)) {
			add_offense(GET_LOYALTY(vict), OFFENSE_ATTACKED_NPC, ch, IN_ROOM(ch), OFF_SEEN);
		}
	}

	ch->next_fighting = combat_list;
	combat_list = ch;

	FIGHTING(ch) = vict;
	FIGHT_MODE(ch) = mode;
	if (mode == FMODE_WAITING) {
		FIGHT_WAIT(ch) = 4;
	}
	else {
		FIGHT_WAIT(ch) = 0;
	}
	GET_POS(ch) = POS_FIGHTING;
	GET_LAST_SWING_MAINHAND(ch) = microtime();
	GET_LAST_SWING_OFFHAND(ch) = GET_LAST_SWING_MAINHAND(ch);
	
	// remove all stuns when combat starts
	affects_from_char_by_aff_flag(ch, AFF_STUNNED, FALSE);
	
	// mark start
	check_combat_start(ch);
}


/* remove a char from the list of fighting chars */
void stop_fighting(char_data *ch) {
	char_data *temp;

	if (ch == next_combat_list)
		next_combat_list = ch->next_fighting;

	REMOVE_FROM_LIST(ch, combat_list, next_fighting);
	ch->next_fighting = NULL;
	FIGHTING(ch) = NULL;
	GET_POS(ch) = POS_STANDING;
	update_pos(ch);
	
	check_combat_end(ch);
}


/**
* This marks the player hostile and declares distrust.
*
* @param char_data *ch The player.
* @param empire_data *emp The empire that will distrust him.
*/
void trigger_distrust_from_hostile(char_data *ch, empire_data *emp) {	
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
		
		log_to_empire(chemp, ELOG_DIPLOMACY, "%s now distrusts this empire due to hostile activity (%s)", EMPIRE_NAME(emp), PERS(ch, ch, TRUE));
		EMPIRE_NEEDS_SAVE(chemp) = TRUE;
	}
	
	// check emp->chemp politics
	if (!(pol = find_relation(emp, chemp))) {
		pol = create_relation(emp, chemp);
	}	
	if (!IS_SET(pol->type, DIPL_WAR | DIPL_DISTRUST)) {
		pol->offer = 0;
		pol->type = DIPL_DISTRUST;
		
		log_to_empire(emp, ELOG_DIPLOMACY, "This empire now officially distrusts %s due to hostile activity", EMPIRE_NAME(chemp));
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
	
	// spawn guards?
}


/**
* Updates a person's position based on the amount of health they have.
*
* @param char_data *victim The person whose position should be updated.
*/
void update_pos(char_data *victim) {
	if (GET_HEALTH(victim) > 0 && GET_POS(victim) > POS_STUNNED) {
		return;
	}
	else if (GET_HEALTH(victim) < -5) {
		GET_POS(victim) = POS_DEAD;
	}
	else if (GET_HEALTH(victim) < 0) {
		GET_POS(victim) = POS_INCAP;
	}
	else if (GET_HEALTH(victim) == 0) {
		GET_POS(victim) = POS_STUNNED;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMBAT ENGINE: ONE ATTACK ///////////////////////////////////////////////

/**
* One melee attack by ch.
*
* @param char_data *ch The attacker.
* @param obj_data *weapon Optional: Which weapon to attack with (NULL means fists)
*/
void perform_violence_melee(char_data *ch, obj_data *weapon) {
	extern bool starving_vampire_aggro(char_data *ch);
	
	// sanity
	if (weapon && !IS_WEAPON(weapon)) {
		weapon = NULL;
	}
	
	if (weapon && OBJ_FLAGGED(weapon, OBJ_TWO_HANDED) && (!has_player_tech(ch, PTECH_TWO_HANDED_WEAPONS) || !check_solo_role(ch))) {
		msg_to_char(ch, "You must be alone to use two-handed weapons in the solo role.\r\n");
		return;
	}
	
	// random chance of this INSTEAD of 'hit' when blood-starved
	if (!IS_NPC(ch) && IS_VAMPIRE(ch) && IS_BLOOD_STARVED(ch) && !number(0, 5)) {
		if (starving_vampire_aggro(ch)) {
			return;
		}
	}
	
	if (hit(ch, FIGHTING(ch), weapon, TRUE) < 0) {
		return;
	}
	
	if (FIGHTING(ch) && GET_HEALTH(FIGHTING(ch)) <= 0 && !WOULD_EXECUTE(ch, FIGHTING(ch))) {
		stop_fighting(ch);
	}
}


/**
* One ranged attack by ch.
*
* @param char_data *ch The attacker.
* @param obj_data *weapon Which weapon to attack with.
*/
void perform_violence_missile(char_data *ch, obj_data *weapon) {
	bool success = FALSE, block = FALSE, purge = TRUE;
	obj_data *ammo, *best = NULL;
	struct affected_type *af;
	struct obj_apply *apply;
	int dam = 0, ret, atype;
	char_data *vict;
	
	if (!(vict = FIGHTING(ch))) {
		return;
	}
	
	// ensure missile weapon
	if (!weapon || !IS_MISSILE_WEAPON(weapon)) {
		msg_to_char(ch, "You don't have a ranged weapon to shoot with!\r\n");
		if (FIGHT_MODE(vict) == FMODE_MISSILE) {
			FIGHT_MODE(ch) = FMODE_WAITING;
			FIGHT_WAIT(ch) = 4;
		}
		else {
			FIGHT_MODE(ch) = FMODE_MELEE;
		}
		return;
	}
	
	// detect ammo
	best = NULL;
	
	if (!IS_NPC(ch)) {
		LL_FOREACH2(ch->carrying, ammo, next_content) {
			if (IS_AMMO(ammo) && GET_OBJ_VNUM(ammo) == USING_AMMO(ch) && GET_AMMO_TYPE(ammo) == GET_MISSILE_WEAPON_AMMO_TYPE(weapon)) {
				if (!best || GET_AMMO_DAMAGE_BONUS(ammo) > GET_AMMO_DAMAGE_BONUS(best)) {
					best = ammo;	// found a [better] match!
				}
			}
		}
	}
	
	if (!best) {	// if we didn't find a preferred one
		for (ammo = ch->carrying; ammo; ammo = ammo->next_content) {
			if (IS_AMMO(ammo) && GET_AMMO_TYPE(ammo) == GET_MISSILE_WEAPON_AMMO_TYPE(weapon)) {
				if (!best || GET_AMMO_DAMAGE_BONUS(ammo) > GET_AMMO_DAMAGE_BONUS(best)) {
					best = ammo;
				}
			}
		}
	}
	
	if (!best && !IS_NPC(ch)) {
		msg_to_char(ch, "You don't have anything that you can shoot!\r\n");
		if (FIGHT_MODE(vict) == FMODE_MISSILE) {
			FIGHT_MODE(ch) = FMODE_WAITING;
			FIGHT_WAIT(ch) = 2;
			}
		else
			FIGHT_MODE(ch) = FMODE_MELEE;
		return;
	}
	
	// update ammo
	if (best) {
		GET_OBJ_VAL(best, VAL_AMMO_QUANTITY) -= 1;
		SET_BIT(GET_OBJ_EXTRA(best), OBJ_NO_STORE);	// can no longer be stored
	}
	
	// compute
	success = check_hit_vs_dodge(ch, vict, FALSE);
	
	if (success && AWAKE(vict) && has_player_tech(vict, PTECH_BLOCK_RANGED)) {
		block = check_block(vict, ch, TRUE);
		if (GET_EQ(vict, WEAR_HOLD) && IS_SHIELD(GET_EQ(vict, WEAR_HOLD)) && can_gain_exp_from(vict, ch)) {
			gain_player_tech_exp(vict, PTECH_BLOCK_RANGED, 2);
		}
	}
	
	if (block) {
		block_missile_attack(ch, vict, GET_MISSILE_WEAPON_TYPE(weapon));
	}
	else if (!success) {
		damage(ch, vict, 0, GET_MISSILE_WEAPON_TYPE(weapon), DAM_PHYSICAL);
		if (can_gain_exp_from(vict, ch)) {
			run_ability_gain_hooks(vict, ch, AGH_DODGE);
		}
	}
	else {
		// compute damage
		dam = GET_MISSILE_WEAPON_DAMAGE(weapon) + (best ? GET_AMMO_DAMAGE_BONUS(best) : 0);
		
		if (!IS_NPC(ch) && has_ability(ch, ABIL_BOWMASTER)) {
			dam *= 1.5;
			if (can_gain_exp_from(ch, vict)) {
				gain_ability_exp(ch, ABIL_BOWMASTER, 2);
			}
		}
		
		// damage last! it's sometimes fatal for vict
		ret = damage(ch, vict, dam, GET_MISSILE_WEAPON_TYPE(weapon), attack_hit_info[GET_MISSILE_WEAPON_TYPE(weapon)].damage_type);
		
		if (ret > 0 && !EXTRACTED(vict) && !IS_DEAD(vict) && IN_ROOM(vict) == IN_ROOM(ch)) {
			// affects?
			if (best && (GET_OBJ_AFF_FLAGS(best) || GET_OBJ_APPLIES(best))) {
				atype = find_generic(GET_OBJ_VNUM(best), GENERIC_AFFECT) ? GET_OBJ_VNUM(best) : ATYPE_RANGED_WEAPON;
			
				if (GET_OBJ_AFF_FLAGS(best)) {
					af = create_flag_aff(atype, 1, GET_OBJ_AFF_FLAGS(best), ch);
					affect_to_char(vict, af);
					free(af);
				}
			
				LL_FOREACH(GET_OBJ_APPLIES(best), apply) {
					af = create_mod_aff(atype, 1, apply->location, apply->modifier, ch);
					affect_to_char(vict, af);
					free(af);
				}
			}
		
			// ability effects
			if (!IS_NPC(ch) && weapon && !AFF_FLAGGED(vict, AFF_IMMUNE_BATTLE) && skill_check(ch, ABIL_TRICK_SHOTS, DIFF_RARELY)) {
				switch (GET_MISSILE_WEAPON_TYPE(weapon)) {
					case TYPE_BOW: {
						af = create_flag_aff(ATYPE_TRICK_SHOT, 2, AFF_SLOW, ch);
						affect_join(vict, af, 0);
						
						act("That shot to the leg seems to slow $N!", FALSE, ch, NULL, vict, TO_CHAR);
						act("$n's last shot hit your leg! You feel slower.", FALSE, ch, NULL, vict, TO_VICT);
						act("$n's last shot seems to stun $N!", FALSE, ch, NULL, vict, TO_NOTVICT);
						break;
					}
					case TYPE_CROSSBOW: {
						apply_dot_effect(vict, ATYPE_TRICK_SHOT, 5, DAM_PHYSICAL, 5, 5, ch);
						
						act("Your shot opens a deep artery in $N -- $E is bleeding!", FALSE, ch, NULL, vict, TO_CHAR);
						act("$n's shot opens a deep artery -- you are bleeding!", FALSE, ch, NULL, vict, TO_VICT);
						act("$n's shot opens a deep artery -- $N is bleeding!", FALSE, ch, NULL, vict, TO_NOTVICT);
						break;
					}
					case TYPE_PISTOL: {
						af = create_mod_aff(ATYPE_TRICK_SHOT, 2, APPLY_DODGE, -(GET_DEXTERITY(ch) * hit_per_dex), ch);
						affect_join(vict, af, 0);
						
						act("That shot to the arm seems to shake $N!", FALSE, ch, NULL, vict, TO_CHAR);
						act("$n's last shot hit your arm! You feel shaken.", FALSE, ch, NULL, vict, TO_VICT);
						act("$n's last shot seems to shake $N!", FALSE, ch, NULL, vict, TO_NOTVICT);
						break;
					}
					case TYPE_MUSKET: {
						apply_dot_effect(vict, ATYPE_TRICK_SHOT, 5, DAM_PHYSICAL, 5, 5, ch);
						
						act("Your shot opens a deep artery in $N -- $E is bleeding!", FALSE, ch, NULL, vict, TO_CHAR);
						act("$n's shot opens a deep artery -- you are bleeding!", FALSE, ch, NULL, vict, TO_VICT);
						act("$n's shot opens a deep artery -- $N is bleeding!", FALSE, ch, NULL, vict, TO_NOTVICT);
						break;
					}
				}
			}
		}
		
		// fire a consume trigger but it can't block execution here
		if (best && !consume_otrigger(best, ch, OCMD_SHOOT, (!EXTRACTED(vict) && !IS_DEAD(vict)) ? vict : NULL)) {
			purge = FALSE;	// ammo likely extracted
		}
		
		// McSkillups
		if (can_gain_exp_from(ch, vict)) {
			gain_player_tech_exp(ch, PTECH_RANGED_COMBAT, 2);
			gain_ability_exp(ch, ABIL_QUICK_DRAW, 2);
			gain_ability_exp(ch, ABIL_TRICK_SHOTS, 2);
			run_ability_gain_hooks(ch, vict, AGH_RANGED);
		}
	}
	
	// ammo countdown/extract (only if the ammo wasn't extracted by a script)
	if (purge && best) {
		if (GET_AMMO_QUANTITY(best) <= 0) {
			extract_obj(best);
		}
	}
	
	// if still fighting
	if (FIGHTING(ch) && GET_HEALTH(FIGHTING(ch)) <= 0 && WOULD_EXECUTE(ch, FIGHTING(ch))) {
		stop_fighting(ch);
	}
}


/**
* Process one round of combat for a character.
*
* @param char_data *ch the character fighting
* @param double speed the speed of this attack in seconds
* @param obj_data *weapon Optional: Which weapon to attack with
*/
void one_combat_round(char_data *ch, double speed, obj_data *weapon) {
	void undisguise(char_data *ch);
	void diag_char_to_char(char_data *i, char_data *ch);
	
	// sanity check again
	if (!FIGHTING(ch)) {
		return;
	}
	
	// if both are players, refresh the quit timer
	if (!IS_NPC(ch) && !IS_NPC(FIGHTING(ch))) {
		add_cooldown(ch, COOLDOWN_PVP_QUIT_TIMER, 45);
		add_cooldown(FIGHTING(ch), COOLDOWN_PVP_QUIT_TIMER, 45);
	}
	
	if (!check_combat_position(ch, speed)) {
		return;
	}
	
	undisguise(ch);

	// check fighting type
	switch (FIGHT_MODE(ch)) {
		case FMODE_MELEE: {
			perform_violence_melee(ch, weapon);
			break;
		}
		case FMODE_MISSILE: {
			perform_violence_missile(ch, weapon);
			break;
		}
		default: {
			// somehow got here? fix it
			FIGHT_MODE(ch) = FMODE_WAITING;
			
			// return to avoid diagnose
			return;
		}
	}

	// still fighting?
	if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)) && SHOW_FIGHT_MESSAGES(ch, FM_AUTO_DIAGNOSE)) {
		diag_char_to_char(FIGHTING(ch), ch);
	}
	
	if (!FIGHTING(ch)) {
		check_combat_end(ch);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMBAT ENGINE: ROUNDS ///////////////////////////////////////////////////

/**
* manage characters who are in FMODE_FIGHT_WAIT
* @param char_data *ch the character running in
* @param double speed the speed of this round in seconds
*/
void fight_wait_run(char_data *ch, double speed) {
	unsigned long long timestamp;
	
	// sanity
	if (!FIGHTING(ch)) {
		return;
	}
	
	// if both are players, refresh the quit timer
	if (!IS_NPC(ch) && !IS_NPC(FIGHTING(ch))) {
		add_cooldown(ch, COOLDOWN_PVP_QUIT_TIMER, 45);
		add_cooldown(FIGHTING(ch), COOLDOWN_PVP_QUIT_TIMER, 45);
	}
	
	if (!check_combat_position(ch, speed)) {
		return;
	}
	
	if (AFF_FLAGGED(ch, AFF_ENTANGLED)) {
		msg_to_char(ch, "You are entangled and can't run into combat!\r\n");
		return;
	}

	// animals try to flee ranged combat
	if (IS_NPC(ch) && !ch->master && MOB_FLAGGED(ch, MOB_ANIMAL) && !number(0, 10)) {
		do_flee(ch, "", 0, 0);
	}
	
	// still fighting?
	if (!FIGHTING(ch) || IN_ROOM(ch) != IN_ROOM(FIGHTING(ch))) {
		return;
	}

	act("You run toward $N!", FALSE, ch, 0, FIGHTING(ch), TO_CHAR);
	act("$n runs toward you!", FALSE, ch, 0, FIGHTING(ch), TO_VICT);
	
	--FIGHT_WAIT(ch);
	
	if (FIGHT_WAIT(ch) <= 0 || FIGHT_MODE(FIGHTING(ch)) != FMODE_MISSILE) {
		act("You engage $M in melee combat!", FALSE, ch, 0, FIGHTING(ch), TO_CHAR);
		act("$e engages you in melee combat!", FALSE, ch, 0, FIGHTING(ch), TO_VICT);
		
		FIGHT_MODE(ch) = FMODE_MELEE;
		
		// Only change modes if he's fighting us, too
		if (FIGHTING(FIGHTING(ch)) == ch) {
			FIGHT_MODE(FIGHTING(ch)) = FMODE_MELEE;
		}
		
		// half-round bonus if you are the one to run in
		timestamp = microtime();
		GET_LAST_SWING_MAINHAND(ch) = timestamp - (get_combat_speed(ch, WEAR_WIELD)/2 SEC_MICRO);
		GET_LAST_SWING_OFFHAND(ch) = timestamp - (get_combat_speed(ch, WEAR_HOLD)/2 SEC_MICRO);
	}
}


/**
* Primary combat loop -- runs every 0.1 seconds. Characters act only
* based on their speed. This runs much much more often than actual
* hits.
*
* @param int pulse the current game pulse, for determining whose turn it is
*/
void frequent_combat(int pulse) {
	extern bool catch_up_combat;
	
	char_data *ch, *vict;
	double speed;
	
	// prevent running multiple combat rounds during a catch-up cycle
	if (!catch_up_combat) {
		return;
	}
	catch_up_combat = FALSE;
	
	for (ch = combat_list; ch; ch = next_combat_list) {
		next_combat_list = ch->next_fighting;
		vict = FIGHTING(ch);
		
		// never!
		if (IS_DEAD(ch) || EXTRACTED(ch)) {
			continue;
		}

		// verify still fighting
		if (vict == NULL || IN_ROOM(ch) != IN_ROOM(vict) || IS_DEAD(vict) || !check_can_still_fight(ch, vict) || AFF_FLAGGED(ch, AFF_NO_ATTACK)) {
			stop_fighting(ch);
			continue;
		}
		
		// bring friends in no matter what (on the real seconds
		if ((pulse % (1 RL_SEC)) == 0) {
			check_auto_assist(ch);
		}
		
		// reasons you would not get a round
		if (GET_POS(ch) < POS_SLEEPING || IS_INJURED(ch, INJ_STAKED | INJ_TIED) || AFF_FLAGGED(ch, AFF_STUNNED | AFF_HARD_STUNNED | AFF_NO_TARGET_IN_ROOM | AFF_MUMMIFY | AFF_DEATHSHROUD)) {
			continue;
		}
		
		switch (FIGHT_MODE(ch)) {
			case FMODE_WAITING: {
				if ((pulse % (1 RL_SEC)) == 0) {
					fight_wait_run(ch, 1);
				}
				break;
			}
			case FMODE_MISSILE: {
				speed = get_combat_speed(ch, WEAR_RANGED);
				
				// my turn?
				if (GET_LAST_SWING_MAINHAND(ch) + (speed SEC_MICRO) <= microtime()) {
					GET_LAST_SWING_MAINHAND(ch) = microtime();
					one_combat_round(ch, speed, GET_EQ(ch, WEAR_RANGED));
				}
				break;
			}
			case FMODE_MELEE:
			default: {
				unsigned long long timestamp = microtime();
				
				// main hand
				speed = get_combat_speed(ch, WEAR_WIELD);
				if (GET_LAST_SWING_MAINHAND(ch) + (speed SEC_MICRO) <= timestamp) {
					GET_LAST_SWING_MAINHAND(ch) = timestamp;
					one_combat_round(ch, speed, GET_EQ(ch, WEAR_WIELD));
				}
				
				// still fighting and can dual-wield?
				if (!IS_NPC(ch) && FIGHTING(ch) && !IS_DEAD(ch) && !EXTRACTED(ch) && !EXTRACTED(FIGHTING(ch)) && has_player_tech(ch, PTECH_DUAL_WIELD) && check_solo_role(ch) && GET_EQ(ch, WEAR_HOLD) && IS_WEAPON(GET_EQ(ch, WEAR_HOLD))) {
					speed = get_combat_speed(ch, WEAR_HOLD);
					if (GET_LAST_SWING_OFFHAND(ch) + (speed SEC_MICRO) <= timestamp) {
						GET_LAST_SWING_OFFHAND(ch) = timestamp;
						one_combat_round(ch, speed, GET_EQ(ch, WEAR_HOLD));
					}
				}
				break;
			}
		}
	}
}
