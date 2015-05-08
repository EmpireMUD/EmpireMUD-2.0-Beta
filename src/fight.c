/* ************************************************************************
*   File: fight.c                                         EmpireMUD 2.0b1 *
*  Usage: Combat system                                                   *
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
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Getters / Helpers
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

// external funcs
ACMD(do_flee);
bool check_scaling(char_data *mob, char_data *based_on);
extern int determine_best_scale_level(char_data *ch, bool check_group);

// locals
int damage(char_data *ch, char_data *victim, int dam, int attacktype, byte damtype);
void drop_loot(char_data *mob, char_data *killer);
int get_effective_block(char_data *ch);
int get_effective_dodge(char_data *ch);
double get_weapon_speed(obj_data *weapon);
void heal(char_data *ch, char_data *vict, int amount);
int hit(char_data *ch, char_data *victim, obj_data *weapon, bool combat_round);
obj_data *make_corpse(char_data *ch);
void perform_execute(char_data *ch, char_data *victim, int attacktype, int damtype);
void trigger_distrust_from_hostile(char_data *ch, empire_data *emp);


 //////////////////////////////////////////////////////////////////////////////
//// GETTERS / HELPERS ///////////////////////////////////////////////////////

/**
* Determines what TYPE_x a character is actually using.
* 
* @param char_data *ch The character attacking.
* @param obj_data *weapon Optional: a weapon to use.
* @return int A TYPE_x value.
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
		else if (IS_NPC(ch) && (MOB_ATTACK_TYPE(ch) != 0) && !AFF_FLAGGED(ch, AFF_DISARM)) {
			w_type = MOB_ATTACK_TYPE(ch);
		}
		else if (!IS_NPC(ch) && GET_MORPH(ch) != MORPH_NONE) {
			w_type = get_morph_attack_type(ch);
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
* Not to be confused with get_effective_block().
*
* @param char_data *ch The blocker.
* @param char_data *attacker The attacker.
* @return int The block chance 0-100%.
*/
int get_block_chance(char_data *ch, char_data *attacker) {
	obj_data *shield = GET_EQ(ch, WEAR_HOLD);
	double base = 5.0;
	double quick_block[] = { 0.0, 10.0, 20.0 };
	
	// must have a shield
	if (!shield || !IS_SHIELD(shield)) {
		return 0;
	}
	
	// skill required for players to block at all
	if (!IS_NPC(ch) && !HAS_ABILITY(ch, ABIL_SHIELD_BLOCK)) {
		return 0;
	}
	
	// quick block procs to add 10%
	if (skill_check(ch, ABIL_QUICK_BLOCK, DIFF_MEDIUM)) {
		base += CHOOSE_BY_ABILITY_LEVEL(quick_block, ch, ABIL_QUICK_BLOCK);
	}
	
	gain_ability_exp(ch, ABIL_SHIELD_BLOCK, 1);
	gain_ability_exp(ch, ABIL_QUICK_BLOCK, 1);
		
	// block modifiers
	base += get_effective_block(ch);
	
	return (int) base;
}


/**
* Computes combat speed based on weapon, abilities, etc.
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
	double base = 3.0;
	int w_type;
	
	double finesse[] = { 0.9, 0.85, 0.8 };
	double quick_draw[] = { 0.75, 0.75, 0.65 };
	
	w_type = get_attack_type(ch, weapon);
	
	if (weapon) {
		base = get_weapon_speed(weapon);
	}
	else {
		// basic speed
		base = attack_hit_info[w_type].speed[SPD_NORMAL];
	}

	// ability mods: player only
	if (!IS_NPC(ch) && weapon) {
		if (HAS_ABILITY(ch, ABIL_FINESSE) && !IS_MISSILE_WEAPON(weapon)) {
			base *= CHOOSE_BY_ABILITY_LEVEL(finesse, ch, ABIL_FINESSE);
		}
		if (HAS_ABILITY(ch, ABIL_QUICK_DRAW) && IS_MISSILE_WEAPON(weapon)) {
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
	base *= (1.0 - (0.025 * GET_WITS(ch)));
	
	// round to .1 seconds
	base *= 10.0;
	base += 0.5;
	base = (double)((int)base);	// poor-man's floor()
	base /= 10.0;
	
	return base < 0.1 ? 0.1 : base;
}


/**
* This is subtracted from get_to_hit (which is 0-100, or more).
* Not to be confused with get_effective_dodge()
*
* @param char_data *ch The dodger.
* @return int The total dodge %.
*/
int get_dodge_modifier(char_data *ch) {
	double base;
	double reflexes[] = { 10.0, 20.0, 30.0 };
	
	base = 0.0;
	
	// 10% per dexterity (balances to-hit dexterity)
	base += 10.0 * GET_DEXTERITY(ch) + get_effective_dodge(ch);
	
	// skills
	gain_ability_exp(ch, ABIL_REFLEXES, 1);
	if (!IS_NPC(ch) && skill_check(ch, ABIL_REFLEXES, DIFF_EASY)) {
		base += CHOOSE_BY_ABILITY_LEVEL(reflexes, ch, ABIL_REFLEXES);
	}
	
	// npc
	if (IS_NPC(ch)) {
		base += MOB_TO_DODGE(ch);
	}
	
	return (int) base;
}


/**
* Applies diminishing returns to GET_BLOCK.
* Not to be confused with get_block_chance().
*
* @param char_data *ch The person whose block rating to get.
* @return int The effective block rating.
*/
int get_effective_block(char_data *ch) {
	return (int) diminishing_returns(GET_BLOCK(ch), 20);
}


/**
* Applies diminishing returns to GET_DODGE.
* Not to be confused with get_dodge_modifier.
*
* @param char_data *ch The person whose dodge rating to get.
* @return int The effective dodge rating.
*/
int get_effective_dodge(char_data *ch) {
	return (int) diminishing_returns(GET_DODGE(ch), 50);
}


/**
* Applies diminishing returns to soak.
*
* @param char_data *ch The person whose soak rating to get.
* @return int The effective soak rating.
*/
int get_effective_soak(char_data *ch) {
	return GET_SOAK(ch); //(int) diminishing_returns(GET_SOAK(ch), 10);
}


/**
* Applies diminishing returns to GET_TO_HIT.
* Not to be confused with get_to_hit().
*
* @param char_data *ch The person whose to-hit rating to get.
* @return int The effective to-hit rating.
*/
int get_effective_to_hit(char_data *ch) {
	return (int) diminishing_returns(GET_TO_HIT(ch), 50);
}


/**
* Chance of ch hitting as 0-100, or more.
* Not to be confused with get_effective_to_hit().
* 
* @param chat_data *ch The hitter.
* @param bool off_hand If TRUE, penalizes to-hit due to off-hand item.
* @return int The hit %.
*/
int get_to_hit(char_data *ch, bool off_hand) {
	double base_chance;
	double sparring_bonus[] = { 10.0, 20.0, 30.0 };
	
	// start at 50%
	base_chance = 50.0 + get_effective_to_hit(ch);
	
	// add 10 per dexterity (will be counter-balanced by dodge dexterity)
	base_chance += 10.0 * GET_DEXTERITY(ch);
	
	// skills
	gain_ability_exp(ch, ABIL_SPARRING, 1);
	if (skill_check(ch, ABIL_SPARRING, DIFF_EASY)) {
		base_chance += CHOOSE_BY_ABILITY_LEVEL(sparring_bonus, ch, ABIL_SPARRING);
	}
	
	// penalty
	if (off_hand) {
		base_chance -= 50;	// TODO is this high enough? could be a config
	}
	
	// npc
	if (IS_NPC(ch)) {
		base_chance += MOB_TO_HIT(ch);
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
		spd = SPD_NORMAL;
		
		speed = missile_weapon_speed[GET_MISSILE_WEAPON_SPEED(weapon)];
		
		if (spd == SPD_FAST) {
			speed *= 0.8;
		}
		else if (spd == SPD_SLOW) {
			speed *= 1.2;
		}
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
	char_data *fighting = FIGHTING(frenemy);
	
	// self is ally
	if (ch == frenemy) {
		return TRUE;
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
		if (frenemy == ch->master) {
			return TRUE;
		}
		// frenemy and ch follow the same master
		if (ch->master != NULL && frenemy->master == ch->master) {
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
* Applies skills and soak to damage, if applicable. This can be used for any
* function but is applied inside of damage() already.
*
* @param int dam The original damage.
* @param char_data *victim The person who is taking damage (may gain skills).
* @param char_data *attacker The person dealing the damage (optional).
* @param int damtype The DAM_x type of damage.
*/
int reduce_damage_from_skills(int dam, char_data *victim, char_data *attacker, int damtype) {
	extern bool check_blood_fortitude(char_data *ch);
	
	bool self = (!attacker || attacker == victim);
	
	if (!self) {
		// damage reduction (usually from armor)
		if ((damtype != DAM_POISON && damtype != DAM_DIRECT)) {
			dam -= get_effective_soak(victim);
		}
	
		if (check_blood_fortitude(victim)) {
			dam = (int) (0.9 * dam);
		}
	
		// redirect some damage to mana: player only
		if (damtype == DAM_MAGICAL && !IS_NPC(victim) && HAS_ABILITY(victim, ABIL_NULL_MANA) && GET_MANA(victim) > 0) {
			int absorb = MIN(dam / 2, GET_MANA(victim));
		
			if (absorb > 0) {
				dam -= absorb;
				GET_MANA(victim) -= absorb;
				gain_ability_exp(victim, ABIL_NULL_MANA, 5);
			}
		}
	}
	
	if (damtype == DAM_POISON) {
		if (HAS_ABILITY(victim, ABIL_POISON_IMMUNITY)) {
			dam = 0;
			gain_ability_exp(victim, ABIL_POISON_IMMUNITY, 2);
		}
		else if (skill_check(victim, ABIL_RESIST_POISON, DIFF_HARD)) {
			dam *= 0.5;
		}
		gain_ability_exp(victim, ABIL_RESIST_POISON, 5);
	}
	
	return dam;
}


/**
* Replaces #w and #W with singular and plural attack types.
*
* @param const char *str The input string.
* @param const char *weapon_singular The string to replace #w with.
* @param const char *weapon_plural The string to replace #W with.
* @return char* The replaced string.
*/
static char *replace_fight_string(const char *str, const char *weapon_singular, const char *weapon_plural) {
	static char buf[MAX_STRING_LENGTH];
	char *cp = buf;

	for (; *str; str++) {
		if (*str == '#') {
			switch (*(++str)) {
				case 'W':
					for (; *weapon_plural; *(cp++) = *(weapon_plural++));
					break;
				case 'w':
					for (; *weapon_singular; *(cp++) = *(weapon_singular++));
					break;
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
	
	// remove respawn
	remove_cooldown_by_type(ch, COOLDOWN_DEATH_RESPAWN);
	
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
	void Objsave_char(char_data *ch, int rent_code);
	
	char_data *ch_iter;
	obj_data *corpse = NULL;
	
	// no need to repeat
	if (EXTRACTED(ch)) {
		return NULL;
	}
		
	// remove all DoTs (BEFORE phoenix)
	while (ch->over_time_effects) {
		dot_remove(ch, ch->over_time_effects);
	}
	
	// somebody saaaaaaave meeeeeeeee
	if (affected_by_spell(ch, ATYPE_PHOENIX_RITE)) {
		affect_from_char(ch, ATYPE_PHOENIX_RITE);
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

	// Alert Group if Applicable
	if (GROUP(ch)) {
		send_to_group(ch, GROUP(ch), "%s has died.\r\n", GET_NAME(ch));
	}
	
	// disable things
	perform_morph(ch, MORPH_NONE);
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
		add_cooldown(ch, COOLDOWN_DEATH_RESPAWN, config_get_int("death_release_minutes") * SECS_PER_REAL_MIN);
		GET_RESURRECT_LOCATION(ch) = NOWHERE;	// ensure no pending resurrect
		GET_RESURRECT_BY(ch) = NOBODY;
		msg_to_char(ch, "Type 'respawn' to come back at your tomb.\r\n");
		GET_POS(ch) = POS_DEAD;	// ensure pos
		return NULL;
	}
	
	// -- only NPCs make it past here --

	/* To make ordinary commands work in scripts.  welcor*/  
	GET_POS(ch) = POS_STANDING;
	if (death_mtrigger(ch, killer)) {
		death_cry(ch);
	}
	GET_POS(ch) = POS_DEAD;
	
	// check if this was an empire npc
	if (IS_NPC(ch) && GET_EMPIRE_NPC_DATA(ch)) {
		kill_empire_npc(ch);
	}
	
	// expand tags, if there are any
	expand_mob_tags(ch);

	drop_loot(ch, killer);
	if (MOB_FLAGGED(ch, MOB_UNDEAD)) {
		if (!IS_NPC(killer) && IS_NPC(ch)) {
			recursive_loot_set(ch->carrying, GET_IDNUM(killer), GET_LOYALTY(killer));
		}
		while (ch->carrying) {
			obj_to_room(ch->carrying, IN_ROOM(ch));
		}
	}
	else {
		corpse = make_corpse(ch);
		obj_to_room(corpse, IN_ROOM(ch));
		
		// tag corpse
		if (corpse && !IS_NPC(killer)) {
			recursive_loot_set(corpse, GET_IDNUM(killer), GET_LOYALTY(killer));
		}
		
		load_otrigger(corpse);
	}
	
	extract_char(ch);	
	return corpse;
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
	void scale_item_to_level(obj_data *obj, int level);

	struct interaction_item *interact;
	obj_data *obj;
	int iter, coins, scale_level = 0;
	empire_data *coin_emp;

	if (!mob || !IS_NPC(mob) || MOB_FLAGGED(mob, MOB_NO_LOOT)) {
		return;
	}
	
	// determine scale level for loot
	scale_level = get_approximate_level(mob);
	
	// loot?
	if (killer && !IS_NPC(killer) && (!GET_LOYALTY(mob) || GET_LOYALTY(mob) == GET_LOYALTY(killer) || char_has_relationship(killer, mob, DIPL_WAR))) {
		coins = mob_coins(mob);
		coin_emp = GET_LOYALTY(mob);
		if (coins > 0) {
			obj = create_money(coin_emp, coins);
			obj_to_char(obj, mob);
		}
	}

	// find and drop loot
	for (interact = mob->interactions; interact; interact = interact->next) {
		if (CHECK_INTERACT(interact, INTERACT_LOOT)) {
			for (iter = 0; iter < interact->quantity; ++iter) {
				obj = read_object(interact->vnum);
				
				// scale
				if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
					scale_item_to_level(obj, scale_level);
				}
				
				// preemptive binding
				if (OBJ_FLAGGED(obj, OBJ_BIND_ON_PICKUP) && IS_NPC(mob)) {
					bind_obj_to_tag_list(obj, MOB_TAGGED_BY(mob));
				}
				
				obj_to_char(obj, mob);
				load_otrigger(obj);
			}
		}
	}
}


/**
* @param char_data *ch The person who died.
* @return obj_data* The corpse of ch.
*/
obj_data *make_corpse(char_data *ch) {	
	char shortdesc[MAX_INPUT_LENGTH], longdesc[MAX_INPUT_LENGTH], kws[MAX_INPUT_LENGTH];
	obj_data *corpse, *o, *next_o;
	int i;
	bool human = (!IS_NPC(ch) || MOB_FLAGGED(ch, MOB_HUMAN));
	
	int stolen_object_timer = config_get_int("stolen_object_timer") * SECS_PER_REAL_MIN;

	corpse = read_object(o_CORPSE);
	
	// store as person's last corpse id
	if (!IS_NPC(ch)) {
		GET_LAST_CORPSE_ID(ch) = GET_ID(corpse);
	}
	
	// binding
	if (OBJ_FLAGGED(corpse, OBJ_BIND_ON_PICKUP)) {
		if (IS_NPC(ch)) {
			// mob corpse: bind to whoever tagged the mob
			bind_obj_to_tag_list(corpse, MOB_TAGGED_BY(ch));
		}
	}
	
	if (human) {
		sprintf(kws, "corpse body %s", skip_filler(PERS(ch, ch, FALSE)));
		sprintf(shortdesc, "%s's body", PERS(ch, ch, FALSE));
		snprintf(longdesc, sizeof(longdesc), "%s's body is lying here.", PERS(ch, ch, FALSE));
		CAP(longdesc);
	}
	else {
		sprintf(kws, "corpse body %s", skip_filler(PERS(ch, ch, FALSE)));
		sprintf(shortdesc, "the corpse of %s", PERS(ch, ch, FALSE));
		snprintf(longdesc, sizeof(longdesc), "%s's corpse is festering on the ground.", PERS(ch, ch, FALSE));
		CAP(longdesc);
	}
	
	// set strings
	GET_OBJ_KEYWORDS(corpse) = str_dup(kws);
	GET_OBJ_SHORT_DESC(corpse) = str_dup(shortdesc);
	GET_OBJ_LONG_DESC(corpse) = str_dup(longdesc);

	GET_OBJ_VAL(corpse, VAL_CORPSE_IDNUM) = IS_NPC(ch) ? GET_MOB_VNUM(ch) : (-1 * GET_IDNUM(ch));
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
				obj_to_obj(unequip_char(ch, i), corpse);
			}
		}
		
		// rope if it was pulling or tied
		if (GET_PULLING(ch) || MOB_FLAGGED(ch, MOB_TIED)) {
			obj_to_obj(read_object(o_ROPE), corpse);
		}

		IS_CARRYING_N(ch) = 0;
		ch->carrying = NULL;
	}
	else {
		// not an npc, but check for stolen
		for (o = ch->carrying; o; o = next_o) {
			next_o = o->next;
			
			// is it stolen?
			if (GET_STOLEN_TIMER(o) + stolen_object_timer > time(0)) {
				obj_to_obj(o, corpse);
			}
		}
		
		// check eq for stolen
		for (i = 0; i < NUM_WEARS; ++i) {
			if (GET_EQ(ch, i) && GET_STOLEN_TIMER(GET_EQ(ch, i)) + stolen_object_timer > time(0)) {
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
* @param int ability Optional (or NO_ABIL): The ability to skillup for rez_by.
*/
void perform_resurrection(char_data *ch, char_data *rez_by, room_data *loc, int ability) {
	extern obj_data *find_obj(int n);

	obj_data *corpse;
	int exp = 15;	// overridden by some abilities
	
	// sanity
	if (!ch || !loc) {
		return;
	}
	
	if (IN_ROOM(ch) != loc) {
		act("$n vanishes in a swirl of light!", TRUE, ch, NULL, NULL, TO_ROOM);
	}
	
	// move character
	char_to_room(ch, loc);
	
	// take care of the corpse
	if ((corpse = find_obj(GET_LAST_CORPSE_ID(ch))) && IS_CORPSE(corpse)) {
		while (corpse->contains) {
			obj_to_char_or_room(corpse->contains, ch);
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
	affect_from_char(ch, ATYPE_DEATH_PENALTY);	// in case
	
	GET_RESURRECT_LOCATION(ch) = NOWHERE;
	GET_RESURRECT_BY(ch) = NOBODY;
	GET_RESURRECT_ABILITY(ch) = NO_ABIL;

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
		struct affected_type *af = create_flag_aff(ATYPE_DEATH_PENALTY, duration, AFF_IMMUNE_PHYSICAL | AFF_NO_ATTACK | AFF_STUNNED);
		affect_join(ch, af, ADD_DURATION);
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

	/* Now we're sure we can hit this person: gets worse with dex */
	if (!AWAKE(ch) || !number(0, MAX(0, (GET_DEXTERITY(ch)/2) - 1))) {
		dam = 15 + (BUILDING_VNUM(from_room) == BUILDING_GUARD_TOWER2 ? 5 : 0) + (BUILDING_VNUM(from_room) == BUILDING_GUARD_TOWER3 ? 5 : 0);
	}
	else {
		dam = 0;
	}

	if (GET_PULLING(ch) && CART_CAN_FIRE(GET_PULLING(ch))) {
		log_to_empire(emp, ELOG_HOSTILITY, "A catapult has been spotted at (%d, %d)!", X_COORD(to_room), Y_COORD(to_room));
	}
	
	if (damage(ch, ch, dam, type, DAM_PHYSICAL) != 0) {
		log_to_empire(emp, ELOG_HOSTILITY, "Guard tower at (%d, %d) is shooting at an infiltrator at (%d, %d)", X_COORD(from_room), Y_COORD(from_room), X_COORD(to_room), Y_COORD(to_room));
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
	obj_data *pulling = GET_PULLING(vict);
	int iter, distance;
	bool hostile = (!IS_NPC(vict) && get_cooldown_time(vict, COOLDOWN_HOSTILE_FLAG) > 0);
	
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
	if (distance > 2 && BUILDING_VNUM(from_room) == BUILDING_GUARD_TOWER) {
		return FALSE;
	}
	
	// can't see into buildings/mountains
	if (ROOM_IS_CLOSED(to_room) || ROOM_SECT_FLAGGED(to_room, SECTF_OBSCURE_VISION) || SECT_FLAGGED(ROOM_ORIGINAL_SECT(to_room), SECTF_OBSCURE_VISION)) {
		return FALSE;
	}

	if (IS_IMMORTAL(vict) || IS_GOD(vict)) {
		return FALSE;
	}
	
	// visibility
	if ((AFF_FLAGGED(vict, AFF_INVISIBLE) || AFF_FLAGGED(vict, AFF_HIDE)) && !GET_LEADING(vict)) {
		return FALSE;
	}

	if (CHECK_MAJESTY(vict) || MORPH_FLAGGED(vict, MORPH_FLAG_ANIMAL)) {
		return FALSE;
	}
	
	if (AFF_FLAGGED(vict, AFF_NO_TARGET_IN_ROOM | AFF_NO_ATTACK | AFF_IMMUNE_PHYSICAL)) {
		return FALSE;
	}

	if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_DARK)) {
		return FALSE;
	}

	// Special handling for mobs -- skip any mob not pulling a catapult
	if (IS_NPC(vict)) {
		if (!pulling || !CART_CAN_FIRE(pulling)) {
			return FALSE;
		}
	}

	// check character OR their leader for permission to use the guard tower room -- that saves them
	if (!((m = vict->master) && in_same_group(vict, m))) {
		m = vict;
	}
	// try for master of other-pulling
	if (m == vict && pulling && GET_PULLED_BY(pulling, 0) && (GET_PULLED_BY(pulling, 0) != vict)) {
		m = GET_PULLED_BY(pulling, 0)->master ? GET_PULLED_BY(pulling, 0)->master : vict;
	}
	// try for master of other-other-pulling
	if (m == vict && pulling && GET_PULLED_BY(pulling, 1) && (GET_PULLED_BY(pulling, 1) != vict)) {
		m = GET_PULLED_BY(pulling, 1)->master ? GET_PULLED_BY(pulling, 1)->master : vict;
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
	if (!IS_NPC(vict) && HAS_ABILITY(vict, ABIL_CLOAK_OF_DARKNESS)) {
		gain_ability_exp(vict, ABIL_CLOAK_OF_DARKNESS, 15);
		if (!number(0, 1) && skill_check(vict, ABIL_CLOAK_OF_DARKNESS, DIFF_HARD)) {
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
	
	// building is in city?
	if (!IS_IN_CITY_ROOM(room)) {
		return;
	}
	
	// darkness effect
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_DARK)) {
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
	struct empire_territory_data *ter;
	room_data *tower;
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		for (ter = EMPIRE_TERRITORY_LIST(emp); ter; ter = ter->next) {
			tower = ter->room;
			// TODO this could be a flag... guard towers need some work
			if (BUILDING_VNUM(tower) == BUILDING_GUARD_TOWER || BUILDING_VNUM(tower) == BUILDING_GUARD_TOWER2 || BUILDING_VNUM(tower) == BUILDING_GUARD_TOWER3) {
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
	pbuf = replace_fight_string("$N blocks $n's #w.", attack_hit_info[w_type].singular, attack_hit_info[w_type].plural);
	act(pbuf, FALSE, ch, NULL, victim, TO_NOTVICT);

	// block message to ch
	if (ch->desc) {
		// send color separately because of act capitalization
		send_to_char("&y", ch);
		pbuf = replace_fight_string("You try to #w $N, but $E blocks.&0", attack_hit_info[w_type].singular, attack_hit_info[w_type].plural);
		act(pbuf, FALSE, ch, NULL, victim, TO_CHAR);
	}

	// block message to victim
	if (victim->desc) {
		// send color separately because of act capitalization
		send_to_char("&r", victim);
		pbuf = replace_fight_string("You block $n's #w.&0", attack_hit_info[w_type].singular, attack_hit_info[w_type].plural);
		act(pbuf, FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
	}
	
	// ensure combat
	engage_combat(victim, ch, TRUE);
}


/**
* Messaging for missile attack blocking.
*
* @param char_data *ch the attacker
* @param char_data *victim the guy who blocked
* @param obj_data *arrow The arrow item being shot.
*/
void block_missile_attack(char_data *ch, char_data *victim, obj_data *arrow) {
	act("You shoot $p at $N, but $E blocks.", FALSE, ch, arrow, victim, TO_CHAR);
	act("$n shoots $p at $N, who blocks.", FALSE, ch, arrow, victim, TO_NOTVICT);
	act("$n shoots $p at you, but you block.", FALSE, ch, arrow, victim, TO_VICT);
}


/**
* Generic weapon damage messages.
*
* @param int dam How much damage was done.
* @param char_data *ch The character dealing the damage.
* @param char_data *victim The person receiving the damage.
* @param int w_type The weapon type (TYPE_x)
*/
void dam_message(int dam, char_data *ch, char_data *victim, int w_type) {
	char *buf;
	int iter, msgnum;

	static struct dam_weapon_type {
		int damage;
		const char *to_char;
		const char *to_victim;
		const char *to_room;	// rearranged this from circle3 to match the order of the "messages" file -pc
	} dam_weapons[] = {

		/* use #w for singular (i.e. "slash") and #W for plural (i.e. "slashes") */

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
		"You maul $N with your #w.",
		"$n mauls you with $s #w.",
		"$n mauls $N with $s #w." },

		{ 28,
		"You massacre $N with your #w.",
		"$n massacres you with $s #w.",
		"$n massacres $N with $s #w." },

		{ 32,
		"You decimate $N with your #w.",
		"$n decimates you with $s #w.",
		"$n decimates $N with $s #w." },

		{ 40,
		"You OBLITERATE $N with your deadly #w!!",
		"$n OBLITERATES you with $s deadly #w!!",
		"$n OBLITERATES $N with $s deadly #w!!" },

		{ 50,
		"You ANNIHILATE $N with your deadly #w!!",
		"$n ANNIHILATES you with $s deadly #w!!",
		"$n ANNIHILATES $N with $s deadly #w!!" },

		{ 60,
		"You MUTILATE $N with your deadly #w!!",
		"$n MUTILATES you with $s deadly #w!!",
		"$n MUTILATES $N with $s deadly #w!!" },

		// use -1 as the final entry (REQUIRED) -- captures any higher damage
		{ -1,
		"You LIQUIDATE $N with your deadly #w!!",
		"$n LIQUIDATES you with $s deadly #w!!",
		"$n LIQUIDATES $N with $s deadly #w!!" }
	};

	// find matching message
	msgnum = 0;
	for (iter = 0;; ++iter) {
		if (dam <= dam_weapons[iter].damage || dam_weapons[iter].damage == -1) {
			msgnum = iter;
			break;
		}
	}

	/* damage message to onlookers */
	buf = replace_fight_string(dam_weapons[msgnum].to_room, attack_hit_info[w_type].singular, attack_hit_info[w_type].plural);
	act(buf, FALSE, ch, NULL, victim, TO_NOTVICT);

	/* damage message to damager */
	if (ch->desc) {
		send_to_char("&y", ch);
		buf = replace_fight_string(dam_weapons[msgnum].to_char, attack_hit_info[w_type].singular, attack_hit_info[w_type].plural);
		act(buf, FALSE, ch, NULL, victim, TO_CHAR);
		send_to_char("&0", ch);
	}

	/* damage message to damagee */
	if (victim->desc) {
		send_to_char("&r", victim);
		buf = replace_fight_string(dam_weapons[msgnum].to_victim, attack_hit_info[w_type].singular, attack_hit_info[w_type].plural);
		act(buf, FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
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
				act(msg->god_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
				act(msg->god_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT);
				act(msg->god_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
			}
			else if (dam != 0) {
				if (GET_POS(vict) == POS_DEAD) {
					send_to_char("&y", ch);
					act(msg->die_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
					send_to_char("&0", ch);

					send_to_char("&r", vict);
					act(msg->die_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
					send_to_char("&0", vict);

					act(msg->die_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
				}
				else {
					send_to_char("&y", ch);
					act(msg->hit_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
					send_to_char("&0", ch);

					send_to_char("&r", vict);
					act(msg->hit_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
					send_to_char("&0", vict);

					act(msg->hit_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
				}
			}
			else if (ch != vict) {	/* Dam == 0 */
				send_to_char("&y", ch);
				act(msg->miss_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
				send_to_char("&0", ch);

				send_to_char("&r", vict);
				act(msg->miss_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
				send_to_char("&0", vict);

				act(msg->miss_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
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
	gain_ability_exp(victim, ABIL_MAJESTY, 33.4);
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
	if (get_cooldown_time(victim, COOLDOWN_HOSTILE_FLAG) > 0) {
		return TRUE;
	}
	// allow-pvp
	if (IS_PVP_FLAGGED(ch) && IS_PVP_FLAGGED(victim)) {
		return TRUE;
	}

	// playtime
	if (!has_one_day_playtime(ch) || !has_one_day_playtime(victim)) {
		return FALSE;
	}

	// cascading order of pk modes
	
	if (IS_SET(pk_ok, PK_FULL))
		return TRUE;

	if (IS_SET(pk_ok, PK_WAR)) {
		if (has_relationship(ch_emp, victim_emp, DIPL_WAR)) {
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


 //////////////////////////////////////////////////////////////////////////////
//// SETTERS / DOERS /////////////////////////////////////////////////////////

/**
* This is used when a character is in a state where they can't be seen, and
* they have done something that cancels those conditions.
*
* @param char_data *ch The person who will appear.
*/
void appear(char_data *ch) {
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
* @param char_data *ch The attacker
* @param room_data *to_room The target room
* @param int damage How much damage to deal to the room
*/
void besiege_room(room_data *to_room, int damage) {	
	char_data *c, *next_c;
	obj_data *o, *next_o;
	empire_data *emp = ROOM_OWNER(to_room);
	int max_dam;
	room_data *rm, *next_rm;
	
	// make sure we only hit the home-room
	to_room = HOME_ROOM(to_room);
	
	// absolutely no damaging city centers
	if (IS_CITY_CENTER(to_room)) {
		return;
	}

	if (emp) {
		log_to_empire(emp, ELOG_HOSTILITY, "Building under siege at (%d, %d)", X_COORD(to_room), Y_COORD(to_room));
	}

	if (IS_MAP_BUILDING(to_room)) {
		COMPLEX_DATA(to_room)->damage += damage;
		
		// maximum damage
		max_dam = GET_BLD_MAX_DAMAGE(building_proto(BUILDING_VNUM(to_room)));
		
		if (BUILDING_DAMAGE(to_room) >= max_dam) {
			disassociate_building(to_room);
			// only abandon outside cities
			if (emp && !find_city(emp, to_room)) {
				abandon_room(to_room);
				read_empire_territory(emp);
			}
			if (ROOM_PEOPLE(to_room)) {
				act("The building is hit and crumbles!", FALSE, ROOM_PEOPLE(to_room), 0, 0, TO_CHAR | TO_ROOM);
			}
		}
		else {	// not over-damaged
			if (ROOM_PEOPLE(to_room)) {
				act("The building is hit by something and shakes violently!", FALSE, ROOM_PEOPLE(to_room), 0, 0, TO_CHAR | TO_ROOM);
			}
			HASH_ITER(interior_hh, interior_world_table, rm, next_rm) {
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
		if (GET_DEXTERITY(c) >= 3 && number(0, GET_DEXTERITY(c) / 3)) {
			msg_to_char(c, "You leap out of the way!\r\n");
		}
		else {
			msg_to_char(c, "You are hit and killed!\r\n");
			if (!IS_NPC(c)) {
				mortlog("%s has been killed by siege damage at (%d, %d)!", PERS(c, c, 1), X_COORD(IN_ROOM(c)), Y_COORD(IN_ROOM(c)));
				syslog(SYS_DEATH, 0, TRUE, "DEATH: %s has been killed by siege damage at %s", GET_NAME(c), room_log_identifier(IN_ROOM(c)));
			}
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
* Checks for helpers in the room to start combat too.
*
* @param char_data *ch The person who needs help!
*/
void check_auto_assist(char_data *ch) {
	void perform_rescue(char_data *ch, char_data *vict, char_data *from);
	
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
		if (ch == ch_iter || FIGHTING(ch) == ch_iter || GET_POS(ch_iter) < POS_STANDING || FIGHTING(ch_iter) || !CAN_SEE(ch_iter, FIGHTING(ch))) {
			continue;
		}
		
		if (in_same_group(ch, ch_iter)) {
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
		
		// champion
		if (iter_master == ch && FIGHTING(ch) && FIGHTING(FIGHTING(ch)) == ch && IS_NPC(ch_iter) && MOB_FLAGGED(ch_iter, MOB_CHAMPION) && FIGHT_MODE(FIGHTING(ch)) == FMODE_MELEE) {
			perform_rescue(ch_iter, ch, FIGHTING(ch));
			continue;
		}
		
		// if we got this far and hit an assist condition
		if (assist) {
			act("You jump to $N's aide!", FALSE, ch_iter, 0, ch, TO_CHAR);
			act("$n jumps to your aide!", FALSE, ch_iter, 0, ch, TO_VICT);
			act("$n jumps to $N's aide!", FALSE, ch_iter, 0, ch, TO_NOTVICT);
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
		msg_to_char(ch, "You jump down from your mount..\r\n");
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
	
	check_scaling(victim, ch);

	/* You can't damage an immortal! */
	if (!IS_NPC(victim) && (IS_IMMORTAL(victim) || (IS_GOD(victim) && !IS_GOD(ch))))
		dam = 0;

	if (GET_MOB_VNUM(ch) != DG_CASTER_PROXY && ch != victim && !can_fight(ch, victim))
		return 0;

	/* Only damage to self (sun) still hurts */
	if (AFF_FLAGGED(victim, AFF_IMMUNE_PHYSICAL) && damtype == DAM_PHYSICAL)
		dam = 0;

	if (victim != ch) {
		/* Start the attacker fighting the victim */
		if (GET_POS(ch) > POS_STUNNED && (FIGHTING(ch) == NULL))
			set_fighting(ch, victim, FMODE_MELEE);

		/* Start the victim fighting the attacker */
		if (GET_POS(victim) > POS_STUNNED && (FIGHTING(victim) == NULL))
			set_fighting(victim, ch, FMODE_MELEE);
	}

	/* If you attack a pet, it hates your guts */
	if (victim->master == ch)
		stop_follower(victim);

	/* If the attacker is invisible, he becomes visible */
	if (SHOULD_APPEAR(ch))
		appear(ch);
		
	dam = reduce_damage_from_skills(dam, victim, ch, damtype);
	
	// lethal damage?? check Master Survivalist
	if ((ch != victim) && dam >= GET_HEALTH(victim) && !IS_NPC(victim) && AWAKE(victim) && CAN_SEE(victim, ch) && HAS_ABILITY(victim, ABIL_MASTER_SURVIVALIST)) {
		if (!number(0, 2)) {
			msg_to_char(victim, "You dive out of the way at the last second!\r\n");
			act("$n dives out of the way at the last second!", FALSE, victim, NULL, NULL, TO_ROOM);
			dam = 0;
			// wait is based on % of max dexterity, where it's longest when you have 1 dex and shortest at max
			WAIT_STATE(victim, (3 RL_SEC * (1.0 - ((double) GET_DEXTERITY(victim) / (att_max(victim) + 1)))));
		}
		
		gain_ability_exp(ch, ABIL_MASTER_SURVIVALIST, 33.4);
	}

	/* Minimum damage of 0.. can't do negative */
	dam = MAX(0, dam);

	// Add Damage
	GET_HEALTH(victim) = GET_HEALTH(victim) - dam;

	update_pos(victim);

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
		if (dam == 0 || ch == victim || (GET_POS(victim) == POS_DEAD && !(!IS_NPC(ch) && !PRF_FLAGGED(ch, PRF_AUTOKILL)))) {
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
			send_to_char("You are incapacitated an will slowly die, if not aided.\r\n", victim);
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
		gain_ability_exp(victim, ABIL_ENDURANCE, 1);

		// armor skills
		for (iter = 0; iter < NUM_WEARS; ++iter) {
			if (GET_EQ(victim, iter) && GET_ARMOR_TYPE(GET_EQ(victim, iter)) != NOTHING) {
				switch (GET_ARMOR_TYPE(GET_EQ(victim, iter))) {
					case ARMOR_CLOTH: {
						gain_ability_exp(victim, ABIL_CLOTH_ARMOR, 1);
						break;
					}
					case ARMOR_LEATHER: {
						gain_ability_exp(victim, ABIL_LEATHER_ARMOR, 1);
						break;
					}
					case ARMOR_MEDIUM: {
						gain_ability_exp(victim, ABIL_MEDIUM_ARMOR, 1);
						break;
					}
					case ARMOR_HEAVY: {
						gain_ability_exp(victim, ABIL_HEAVY_ARMOR, 1);
						break;
					}
				}
			}
		}
	
		if (affected_by_spell(victim, ATYPE_MANASHIELD)) {
			gain_ability_exp(victim, ABIL_MANASHIELD, 1);
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
		perform_execute(ch, victim, attacktype, damtype);
		return -1;
	}

	return dam;
}


/**
* Adds a death log to a character who is about to die.
*
* @param char_data *ch The person dying.
* @param char_data *killer The person who killed him.
* @param int type An ATTACK_x or TYPE_x damage type that killed ch (e.g. ATTACK_EXECUTE).
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
		free(msg);
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

	mortlog("%s (%d, %d)", output, X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
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
	// nope
	if (IS_IMMORTAL(ch) || IS_IMMORTAL(vict) || !can_fight(ch, vict)) {
		return;
	}

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
		set_fighting(vict, ch, melee ? FMODE_MELEE : FMODE_WAITING);
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
	
	// apply heal
	GET_HEALTH(vict) = MIN(GET_MAX_HEALTH(vict), GET_HEALTH(vict) + amount);
	
	if (GET_POS(vict) <= POS_STUNNED) {
		update_pos(vict);
	}
	
	// check recovery
	if (GET_POS(vict) < POS_SLEEPING && GET_HEALTH(vict) > 0) {
		msg_to_char(vict, "You recover and wake up.\r\n");
		GET_POS(vict) = POS_SITTING;
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
	extern int apply_poison(char_data *ch, char_data *vict, int type);
	extern int lock_instance_level(room_data *room, int level);
	
	struct instance_data *inst;
	int w_type;
	int dam, hit_chance, result;
	bool success = FALSE, block = FALSE;
	bool can_gain_skill;
	empire_data *victim_emp;
	struct affected_type *af;
	
	// some config TODO move this into the config system?
	int cut_deep_durations[] = { 3, 3, 6 };
	int stunning_blow_durations[] = { 1, 1, 2 };
	double big_game_hunter[] = { 1.05, 1.05, 1.10 };

	// brief sanity
	if (!victim || !ch || EXTRACTED(victim) || IS_DEAD(victim)) {
		return -1;
	}
	
	// weapons not allowed if disarmed
	if (AFF_FLAGGED(ch, AFF_DISARM)) {
		weapon = NULL;
	}
	w_type = get_attack_type(ch, weapon);
	victim_emp = GET_LOYALTY(victim);
	
	// look for an instance to lock
	if (!IS_NPC(ch) && IS_ADVENTURE_ROOM(IN_ROOM(ch)) && COMPLEX_DATA(IN_ROOM(ch)) && (inst = COMPLEX_DATA(IN_ROOM(ch))->instance)) {
		if (ADVENTURE_FLAGGED(inst->adventure, ADV_LOCK_LEVEL_ON_COMBAT) && !IS_IMMORTAL(ch)) {
			lock_instance_level(IN_ROOM(ch), determine_best_scale_level(ch, TRUE));
		}
	}

	/* check if the character has a fight trigger */
	fight_mtrigger(ch);
	
	if (!IS_NPC(ch) && victim_emp && GET_LOYALTY(ch) != victim_emp) {
		trigger_distrust_from_hostile(ch, victim_emp);
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
	hit_chance = get_to_hit(ch, (weapon && weapon->worn_on == WEAR_HOLD));
	
	// evasion
	if (AWAKE(victim) && CAN_SEE(victim, ch)) {
		hit_chance -= get_dodge_modifier(victim);
	}

	success = !AWAKE(victim) || (hit_chance >= number(1, 100));
	
	if (success && AWAKE(victim) && CAN_SEE(victim, ch) && attack_hit_info[w_type].damage_type == DAM_PHYSICAL) {
		block = (get_block_chance(victim, ch) > number(1, 100));
	}

	// outcome:
	if (!success) {
		// miss
		return damage(ch, victim, 0, w_type, attack_hit_info[w_type].damage_type);
	}
	else if (block) {
		block_attack(ch, victim, w_type);
		return 0;
	}
	else {
		/* okay, we know the guy has been hit.  now calculate damage. */

		// TODO move this damage computation to its own function so that it can be called remotely too

		if (IS_MAGIC_ATTACK(w_type)) {
			dam = GET_INTELLIGENCE(ch);
		}
		else {
			/* Melee attacks are based upon strength.. */
			dam = GET_STRENGTH(ch);
		}
		
		if (w_type == TYPE_VAMPIRE_CLAWS) {
			dam *= 2;
		}

		if (weapon && IS_WEAPON(weapon)) {
			/* Add weapon-based damage if a weapon is being wielded */
			dam += GET_WEAPON_DAMAGE_BONUS(weapon);
		}
		
		if (IS_NPC(ch)) {
			dam += MOB_DAMAGE(ch) / (AFF_FLAGGED(ch, AFF_DISARM) ? 2 : 1);
		}
		
		if (IS_MAGIC_ATTACK(w_type)) {
			dam += GET_BONUS_MAGICAL(ch);
		}
		else {
			dam += GET_BONUS_PHYSICAL(ch);
		}
				
		// All these abilities add damage: no skill gain on an already-beated foe
		if (can_gain_skill) {
			if (!IS_NPC(ch) && HAS_ABILITY(ch, ABIL_DAGGER_MASTERY) && weapon && GET_WEAPON_TYPE(weapon) == TYPE_STAB) {
				dam *= 1.5;
				gain_ability_exp(ch, ABIL_DAGGER_MASTERY, 1);
			}
			if (!IS_NPC(ch) && HAS_ABILITY(ch, ABIL_STAFF_MASTERY) && weapon && IS_STAFF(weapon)) {
				dam *= 1.5;
				gain_ability_exp(ch, ABIL_STAFF_MASTERY, 1);
			}	
			if (!IS_NPC(ch) && HAS_ABILITY(ch, ABIL_CLAWS) && w_type == TYPE_VAMPIRE_CLAWS) {
				gain_ability_exp(ch, ABIL_CLAWS, 1);
			}

			// raw damage modified by hunt
			if (IS_NPC(victim) && MOB_FLAGGED(victim, MOB_ANIMAL) && HAS_ABILITY(ch, ABIL_HUNT)) {
				gain_ability_exp(ch, ABIL_HUNT, 1);
			
				if (skill_check(ch, ABIL_HUNT, DIFF_EASY)) {
					dam *= 2;
				}
			}
		
			// raw damage modified by big game hunter
			if (!IS_NPC(ch) && HAS_ABILITY(ch, ABIL_BIG_GAME_HUNTER) && (IS_VAMPIRE(victim) || IS_MAGE(victim))) {
				gain_ability_exp(ch, ABIL_BIG_GAME_HUNTER, 1);
			
				if (skill_check(ch, ABIL_BIG_GAME_HUNTER, DIFF_MEDIUM)) {
					dam = (int) (CHOOSE_BY_ABILITY_LEVEL(big_game_hunter, ch, ABIL_BIG_GAME_HUNTER) * dam);
				}
			}
		}

		dam = MAX(0, dam);

		// anything after this must NOT rely on victim being alive
		result = damage(ch, victim, dam, w_type, attack_hit_info[w_type].damage_type);
		
		if (!IS_NPC(ch) && combat_round && can_gain_skill) {
			gain_ability_exp(ch, ABIL_FINESSE, 1);
			if (GET_SKILL(ch, SKILL_BATTLE) < EMPIRE_CHORE_SKILL_CAP) {
				gain_skill_exp(ch, SKILL_BATTLE, 2);
			}
			if (affected_by_spell(ch, ATYPE_ALACRITY)) {
				gain_ability_exp(ch, ABIL_ALACRITY, 1);
			}
			if (result >= 0) {
				if (affected_by_spell(victim, ATYPE_FORESIGHT)) {
					gain_ability_exp(victim, ABIL_FORESIGHT, 1);
				}
			}
			
			// fireball skill gain
			if (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_VNUM(GET_EQ(ch, WEAR_WIELD)) == o_FIREBALL) {
				gain_ability_exp(ch, ABIL_READY_FIREBALL, 1);
			}
		}
		
		/* check if the victim has a hitprcnt trigger */
		if (result > 0 && !EXTRACTED(victim) && !IS_DEAD(victim) && IN_ROOM(victim) == IN_ROOM(ch)) {
			hitprcnt_mtrigger(victim);
		}
		
		// check post-hit skills
		if (result > 0 && !EXTRACTED(victim) && !IS_DEAD(victim) && IN_ROOM(victim) == IN_ROOM(ch)) {
			// cut deep: players only
			if (!IS_NPC(ch) && !AFF_FLAGGED(victim, AFF_IMMUNE_BATTLE) && skill_check(ch, ABIL_CUT_DEEP, DIFF_RARELY) && weapon && attack_hit_info[w_type].weapon_type == WEAPON_SHARP) {
				apply_dot_effect(victim, ATYPE_CUT_DEEP, CHOOSE_BY_ABILITY_LEVEL(cut_deep_durations, ch, ABIL_CUT_DEEP), DAM_PHYSICAL, 5, 5);
				
				act("You cut deep wounds in $N -- $E is bleeding!", FALSE, ch, NULL, victim, TO_CHAR);
				act("$n's last attack cuts deep -- you are bleeding!", FALSE, ch, NULL, victim, TO_VICT);
				act("$n's last attack cuts deep -- $N is bleeding!", FALSE, ch, NULL, victim, TO_NOTVICT);

				if (can_gain_skill) {
					gain_ability_exp(ch, ABIL_CUT_DEEP, 1);
				}
			}
		
			// stunning blow: players only
			if (!IS_NPC(ch) && !AFF_FLAGGED(victim, AFF_IMMUNE_BATTLE | AFF_IMMUNE_STUN) && skill_check(ch, ABIL_STUNNING_BLOW, DIFF_RARELY) && weapon && attack_hit_info[w_type].weapon_type == WEAPON_BLUNT) {
				af = create_flag_aff(ATYPE_STUNNING_BLOW, CHOOSE_BY_ABILITY_LEVEL(stunning_blow_durations, ch, ABIL_STUNNING_BLOW), AFF_STUNNED);
				affect_join(victim, af, 0);
				
				act("That last blow seems to stun $N!", FALSE, ch, NULL, victim, TO_CHAR);
				act("$n's last blow hit you hard! You're knocked to the floor, stunned.", FALSE, ch, NULL, victim, TO_VICT);
				act("$n's last blow seems to stun $N!", FALSE, ch, NULL, victim, TO_NOTVICT);

				if (can_gain_skill) {
					gain_ability_exp(ch, ABIL_STUNNING_BLOW, 1);
				}
			}
			
			// poison could kill too
			if (!IS_NPC(ch) && HAS_ABILITY(ch, ABIL_POISONS) && weapon && attack_hit_info[w_type].weapon_type == WEAPON_SHARP) {
				if (!number(0, 1) && apply_poison(ch, victim, USING_POISON(ch)) < 0) {
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
	add_lore(ch, LORE_DEATH, 0);
	die(ch, ch);
}


/**
* Actual execute -- sometimes this exits early by knocking the victim
* unconscious, unless the execute is deliberate.
*
* @param char_data *ch The murderer.
* @param char_data *victim The unfortunate one.
* @param int attacktype Any ATTACK_x or TYPE_x.
* @param int damtype DAM_x.
*/
void perform_execute(char_data *ch, char_data *victim, int attacktype, int damtype) {
	void end_pursuit(char_data *ch, char_data *target);

	bool ok = FALSE;
	bool revert = TRUE;
	char_data *m, *ch_iter;
	obj_data *weapon;

	#define WOULD_EXECUTE(ch)  (IS_NPC(ch) ? (!MOB_FLAGGED((ch), MOB_ANIMAL) || MOB_FLAGGED((ch), MOB_AGGRESSIVE)) : (PRF_FLAGGED((ch), PRF_AUTOKILL)))

	/* stop_fighting() is split around here to help with exp */

	/* Probably sent here by damage() */
	if (ch == victim)
		ok = TRUE;

	/* Sent here by damage() */
	if (attacktype > NUM_ATTACK_TYPES || WOULD_EXECUTE(ch)) {
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
		if (FIGHTING(victim))
			stop_fighting(victim);

		// look for anybody in the room fighting victim (including ch) who wouldn't execute:
		for (ch_iter = ROOM_PEOPLE(IN_ROOM(victim)); ch_iter; ch_iter = ch_iter->next_in_room) {
			if (ch_iter != victim && FIGHTING(ch_iter) == victim && !WOULD_EXECUTE(ch_iter)) {
				stop_fighting(ch_iter);
			}
		}

		/* knock 'em out */
		GET_HEALTH(victim) = -1;
		GET_POS(victim) = POS_INCAP;
	
		// remove all DoTs
		while (victim->over_time_effects) {
			dot_remove(victim, victim->over_time_effects);
		}
		
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

	if (revert && !IS_NPC(victim) && GET_MORPH(victim) != MORPH_NONE) {
		sprintf(buf, "%s reverts into $n!", PERS(victim, victim, FALSE));

		perform_morph(victim, MORPH_NONE);
		act(buf, TRUE, victim, 0, 0, TO_ROOM);
	}
		
	act("$n is dead! R.I.P.", FALSE, victim, 0, 0, TO_ROOM);

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
			if (attacktype == ATTACK_GUARD_TOWER)
				add_lore(ch, LORE_TOWER_DEATH, 0);
			else
				add_lore(ch, LORE_DEATH, 0);
		}
		else {
			add_lore(ch, LORE_PLAYER_KILL, GET_IDNUM(victim));
			add_lore(victim, LORE_PLAYER_DEATH, GET_IDNUM(ch));
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
	
	if (!emp || EMPIRE_IMM_ONLY(emp) || IS_IMMORTAL(ch)) {
		return;
	}
	
	add_cooldown(ch, COOLDOWN_HOSTILE_FLAG, config_get_int("hostile_flag_time") * SECS_PER_REAL_MIN);
	
	// no player empire? done
	if (!chemp) {
		return;
	}
	
	// check chemp->emp politics
	if (!(pol = find_relation(chemp, emp))) {
		pol = create_relation(chemp, emp);
	}	
	if (!IS_SET(pol->type, DIPL_WAR | DIPL_DISTRUST)) {
		pol->offer = 0;
		pol->type = DIPL_DISTRUST;
		
		log_to_empire(chemp, ELOG_DIPLOMACY, "%s now distrusts this empire due to hostile activity", EMPIRE_NAME(emp));	
		save_empire(chemp);
	}
	
	// check emp->chemp politics
	if (!(pol = find_relation(emp, chemp))) {
		pol = create_relation(emp, chemp);
	}	
	if (!IS_SET(pol->type, DIPL_WAR | DIPL_DISTRUST)) {
		pol->offer = 0;
		pol->type = DIPL_DISTRUST;
		
		log_to_empire(emp, ELOG_DIPLOMACY, "This empire now officially distrusts %s due to hostile activity", EMPIRE_NAME(chemp));
		save_empire(emp);
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
	// sanity
	if (weapon && !IS_WEAPON(weapon)) {
		weapon = NULL;
	}
	
	if (hit(ch, FIGHTING(ch), weapon, TRUE) < 0) {
		return;
	}
	
	if (!IS_NPC(ch) && FIGHTING(ch) && GET_HEALTH(FIGHTING(ch)) <= 0 && !PRF_FLAGGED(ch, PRF_AUTOKILL)) {
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
	obj_data *arrow, *best = NULL;
	int dam = 0;
	int to_hit;
	bool success = FALSE, block = FALSE;
	
	if (!FIGHTING(ch)) {
		return;
	}

	if (!weapon || !IS_MISSILE_WEAPON(weapon)) {
		msg_to_char(ch, "You don't have a ranged weapon to shoot with!\r\n");
		if (FIGHT_MODE(FIGHTING(ch)) == FMODE_MISSILE) {
			FIGHT_MODE(ch) = FMODE_WAITING;
			FIGHT_WAIT(ch) = 2;
		}
		else {
			FIGHT_MODE(ch) = FMODE_MELEE;
		}
		return;
	}
	
	best = NULL;
	for (arrow = ch->carrying; arrow; arrow = arrow->next_content) {
		if (IS_ARROW(arrow) && GET_ARROW_TYPE(arrow) == GET_MISSILE_WEAPON_TYPE(weapon)) {
			if (!best || GET_ARROW_DAMAGE_BONUS(arrow) > GET_ARROW_DAMAGE_BONUS(best)) {
				best = arrow;
			}
		}
	}
	
	if (!best) {
		msg_to_char(ch, "You don't have anything that you can shoot!\r\n");
		if (FIGHT_MODE(FIGHTING(ch)) == FMODE_MISSILE) {
			FIGHT_MODE(ch) = FMODE_WAITING;
			FIGHT_WAIT(ch) = 2;
			}
		else
			FIGHT_MODE(ch) = FMODE_MELEE;
		return;
	}

	// compute
	to_hit = get_to_hit(ch, FALSE);

	if (AWAKE(FIGHTING(ch)) && CAN_SEE(FIGHTING(ch), ch)) {
		to_hit -= get_dodge_modifier(FIGHTING(ch));
	}
	
	success = (to_hit >= number(1, 100));
	
	if (success && AWAKE(FIGHTING(ch)) && CAN_SEE(FIGHTING(ch), ch) && HAS_ABILITY(FIGHTING(ch), ABIL_BLOCK_ARROWS)) {
		block = (get_block_chance(FIGHTING(ch), ch) > number(1, 100));
		gain_ability_exp(FIGHTING(ch), ABIL_BLOCK_ARROWS, 2);
	}
	
	if (block) {
		block_missile_attack(ch, FIGHTING(ch), best);
	}
	else if (!success) {
		damage(ch, FIGHTING(ch), 0, ATTACK_ARROW, DAM_PHYSICAL);
	}
	else {
		// compute damage
		dam = GET_MISSILE_WEAPON_DAMAGE(weapon) + GET_ARROW_DAMAGE_BONUS(best);
		
		// damage last! it's sometimes fatal for FIGHTING(ch)
		damage(ch, FIGHTING(ch), dam, ATTACK_ARROW, DAM_PHYSICAL);
		
		// McSkillups
		gain_ability_exp(ch, ABIL_ARCHERY, 1);
		gain_ability_exp(ch, ABIL_QUICK_DRAW, 1);
		if (affected_by_spell(ch, ATYPE_ALACRITY)) {
			gain_ability_exp(ch, ABIL_ALACRITY, 1);
		}
	}
	
	// arrow countdown/extract
	GET_OBJ_VAL(best, VAL_ARROW_QUANTITY) -= 1;
	if (GET_ARROW_QUANTITY(best) <= 0) {
		extract_obj(best);
	}
		
	if (!IS_NPC(ch) && FIGHTING(ch) && GET_HEALTH(FIGHTING(ch)) <= 0 && !PRF_FLAGGED(ch, PRF_AUTOKILL)) {
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
	
	check_auto_assist(ch);

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
	if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch))) {
		diag_char_to_char(FIGHTING(ch), ch);
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
	// sanity
	if (!FIGHTING(ch)) {
		return;
	}
	
	// if both are players, refresh the quit timer
	if (!IS_NPC(ch) && !IS_NPC(FIGHTING(ch))) {
		add_cooldown(ch, COOLDOWN_PVP_QUIT_TIMER, 45);
		add_cooldown(FIGHTING(ch), COOLDOWN_PVP_QUIT_TIMER, 45);
	}
	
	check_auto_assist(ch);
	
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
	char_data *ch, *vict;
	double speed;
	
	for (ch = combat_list; ch; ch = next_combat_list) {
		next_combat_list = ch->next_fighting;
		vict = FIGHTING(ch);

		// verify still fighting
		if (vict == NULL || IN_ROOM(ch) != IN_ROOM(vict) || IS_DEAD(vict)) {
			stop_fighting(ch);
			continue;
		}
		
		// reasons you would not get a round
		if (IS_DEAD(ch) || EXTRACTED(ch) || GET_POS(ch) < POS_SLEEPING || IS_INJURED(ch, INJ_STAKED | INJ_TIED) || AFF_FLAGGED(ch, AFF_STUNNED | AFF_NO_TARGET_IN_ROOM | AFF_NO_ATTACK | AFF_MUMMIFY | AFF_DEATHSHROUD)) {
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
				if ((pulse % ((int)(speed RL_SEC))) == 0) {
					one_combat_round(ch, speed, GET_EQ(ch, WEAR_RANGED));
				}
				break;
			}
			case FMODE_MELEE:
			default: {
				// main hand
				speed = get_combat_speed(ch, WEAR_WIELD);
				if ((pulse % ((int)(speed RL_SEC))) == 0) {
					one_combat_round(ch, speed, GET_EQ(ch, WEAR_WIELD));
				}
				
				// still fighting and can dual-wield?
				if (!IS_NPC(ch) && FIGHTING(ch) && !IS_DEAD(ch) && !EXTRACTED(ch) && !EXTRACTED(FIGHTING(ch)) && HAS_ABILITY(ch, ABIL_DUAL_WIELD) && GET_EQ(ch, WEAR_HOLD) && IS_WEAPON(GET_EQ(ch, WEAR_HOLD))) {
					speed = get_combat_speed(ch, WEAR_HOLD);
					if ((pulse % ((int)(speed RL_SEC))) == 0) {
						one_combat_round(ch, speed, GET_EQ(ch, WEAR_HOLD));
					}
				}
				break;
			}
		}
	}
}
