/* ************************************************************************
*   File: limits.c                                        EmpireMUD 2.0b5 *
*  Usage: Periodic updates and limiters                                   *
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
#include "db.h"
#include "handler.h"
#include "interpreter.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Character Limits
*   Empire Limits
*   Object Limits
*   Room Limits
*   Vehicle Limits
*   Miscellaneous Limits
*   Periodic Gainers
*   Core Periodicals
*/

// external vars
extern const char *affect_wear_off_msgs[];
extern const char *dirs[];
extern const char *from_dir[];
extern const struct material_data materials[NUM_MATERIALS];
extern const int regen_by_pos[];
extern const struct wear_data_type wear_data[NUM_WEARS];

// external funcs
extern obj_data *die(char_data *ch, char_data *killer);
void death_log(char_data *ch, char_data *killer, int type);
extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom);
extern struct instance_data *get_instance_by_id(any_vnum instance_id);
extern room_data *obj_room(obj_data *obj);
void out_of_blood(char_data *ch);
void perform_abandon_city(empire_data *emp, struct empire_city_data *city, bool full_abandon);
void stop_room_action(room_data *room, int action, int chore);

// locals
int health_gain(char_data *ch, bool info_only);
int mana_gain(char_data *ch, bool info_only);
int move_gain(char_data *ch, bool info_only);


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER LIMITS ////////////////////////////////////////////////////////

/**
* This checks for players whose primary attributes have dropped below 1, and
* unequips gear to compensate.
*
* @param char_data *ch The player to check for attributes.
*/
void check_attribute_gear(char_data *ch) {
	extern const int apply_attribute[];
	extern const int primary_attributes[];
	
	struct obj_apply *apply;
	int iter, pos;
	obj_data *obj;
	bool found;
	
	if (!ch || IS_NPC(ch)) {
		return;
	}
	
	// don't bother if morphed -- let them keep using gear, but not equip any more
	if (IS_MORPHED(ch)) {
		return;
	}
	// don't remove gear while fighting
	if (FIGHTING(ch)) {
		return;
	}
	
	// ensure work to do (shortcut)
	found = FALSE;
	for (iter = 0; primary_attributes[iter] != NOTHING; ++iter) {
		if (GET_ATT(ch, primary_attributes[iter]) < 1) {
			found = TRUE;
		}
	}
	
	if (!found) {
		return;
	}
	
	// check all equipped items
	for (pos = 0; pos < NUM_WEARS; ++pos) {
		obj = GET_EQ(ch, pos);
		if (!obj || !wear_data[pos].count_stats) {
			continue;
		}
		
		// check all applies on item
		found = FALSE;
		for (apply = GET_OBJ_APPLIES(obj); apply; apply = apply->next) {
			if (apply->modifier >= 0) {
				continue;
			}
			
			// check each primary attribute
			for (iter = 0; primary_attributes[iter] != NOTHING && !found; ++iter) {
				if (GET_ATT(ch, primary_attributes[iter]) < 1 && primary_attributes[iter] == apply_attribute[(int) apply->location]) {
					act("You are too weak to keep using $p.", FALSE, ch, obj, NULL, TO_CHAR);
					act("$n stops using $p.", TRUE, ch, obj, NULL, TO_ROOM);
					// this may extract it
					unequip_char_to_inventory(ch, pos);
					
					found = TRUE;
				}
			}
		}
	}
	
	if (found) {
		determine_gear_level(ch);
	}
}


/**
* Called periodically to force players to respawn from death.
*/
void check_death_respawn(void) {
	ACMD(do_respawn);
	
	descriptor_data *desc;
	char_data *ch;
	
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (STATE(desc) != CON_PLAYING || !(ch = desc->character)) {
			continue;
		}
		
		if (IS_DEAD(ch) && get_cooldown_time(ch, COOLDOWN_DEATH_RESPAWN) == 0) {
			do_respawn(ch, "", 0, 0);
		}
	}
}


/**
* It's not necessary to force-expire cooldowns like this, but players may
* benefit from an explicit message. As such, to save computational power, this
* only runs on players who are connected. Nobody else, including mobs, needs
* to know.
*/
void check_expired_cooldowns(void) {
	extern const char *cooldown_types[];
	
	struct cooldown_data *cool, *next_cool;
	char lbuf[MAX_STRING_LENGTH];
	char_data *ch;
	descriptor_data *d;
	
	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) == CON_PLAYING && (ch = d->character)) {
			for (cool = ch->cooldowns; cool; cool = next_cool) {
				next_cool = cool->next;
				
				if ((cool->expire_time - time(0)) <= 0) {
					sprinttype(cool->type, cooldown_types, lbuf);
					msg_to_char(ch, "&%cYour %s cooldown has ended.&0\r\n", (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_STATUS)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_STATUS) : '0', lbuf);
					remove_cooldown(ch, cool);
				}
			}
		}
	}
}


/**
* Times out players who are sitting at the password or name prompt.
*/
void check_idle_passwords(void) {
	descriptor_data *d, *next_d;

	for (d = descriptor_list; d; d = next_d) {
		next_d = d->next;
		if (STATE(d) != CON_PASSWORD && STATE(d) != CON_GET_NAME)
			continue;
			
		if (d->idle_tics < 2) {
			++d->idle_tics;
		}
		else {
			if (STATE(d) == CON_PASSWORD) {
				ProtocolNoEcho(d, false);
			}
			SEND_TO_Q("\r\nTimed out... goodbye.\r\n", d);
			STATE(d) = CON_CLOSE;
		}
	}
}


/**
* This checks if a character is too idle and disconnects or times them out.
*
* @param char_data *ch The person to check.
*/
void check_idling(char_data *ch) {
	if (IS_NPC(ch)) {
		return;
	}

	ch->char_specials.timer++;

	if ((ch->desc && ch->char_specials.timer > config_get_int("idle_rent_time")) || (!ch->desc && ch->char_specials.timer > config_get_int("idle_linkdead_rent_time"))) {
		perform_idle_out(ch);
	}
}


/**
* This function looks for mobs that are stuck in a pointless fight. This is
* a fight where nobody is able to attack. If the mob is in such a state, this
* function will cancel the fight.
*
* @param char_data *mob The mob to check.
*/
void check_pointless_fight(char_data *mob) {
	char_data *iter;
	bool any;
	
	#define IS_POINTLESS(ch)  (GET_HEALTH(ch) <= 0 || MOB_FLAGGED(ch, MOB_NO_ATTACK))
	
	if (!FIGHTING(mob) || !IS_POINTLESS(mob)) {
		return;	// mob is not pointless (or not fighting)
	}
	if (FIGHTING(FIGHTING(mob)) && !IS_POINTLESS(FIGHTING(mob))) {
		return;	// mob is fighting a non-pointless enemy
	}
	
	any = FALSE;
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(mob)), iter, next_in_room) {
		if (iter == mob || FIGHTING(iter) != mob) {
			continue;	// only care about people fighting mob
		}
		if (!IS_POINTLESS(iter)) {
			any = TRUE;
			break;
		}
	}
	
	// did with find ANY non-pointless people hitting the mob?
	if (!any) {
		// stop mob
		stop_fighting(mob);
		
		// stop everyone hitting mob
		LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(mob)), iter, next_in_room) {
			if (FIGHTING(iter) == mob) {
				stop_fighting(iter);
			}
		}
	}
}


/**
* Determines if a player can really be riding. Dismounts them if not.
*
* @param char_data *ch The player to check.
*/
void check_should_dismount(char_data *ch) {
	ACMD(do_dismount);
	
	bool ok = TRUE;
	
	if (!IS_RIDING(ch)) {
		return;	// nm
	}
	else if (IS_MORPHED(ch)) {
		ok = FALSE;
	}
	else if (IS_COMPLETE(IN_ROOM(ch)) && !BLD_ALLOWS_MOUNTS(IN_ROOM(ch))) {
		ok = FALSE;
	}
	else if (GET_SITTING_ON(ch)) {
		ok = FALSE;
	}
	else if (MOUNT_FLAGGED(ch, MOUNT_FLYING) && !CAN_RIDE_FLYING_MOUNT(ch)) {
		ok = FALSE;
	}
	else if (DEEP_WATER_SECT(IN_ROOM(ch)) && !MOUNT_FLAGGED(ch, MOUNT_AQUATIC) && !EFFECTIVELY_FLYING(ch)) {
		ok = FALSE;
	}
	else if (!has_ability(ch, ABIL_ALL_TERRAIN_RIDING) && WATER_SECT(IN_ROOM(ch)) && !MOUNT_FLAGGED(ch, MOUNT_AQUATIC) && !EFFECTIVELY_FLYING(ch)) {
		ok = FALSE;
	}
	
	if (!ok) {
		do_dismount(ch, "", 0, 0);
	}
}


/**
* This clears the linkdead players out, and should be called before you run
* anything that updates offline players, as players in this state can cause
* actual problems if they reconnect.
*/
void eliminate_linkdead_players(void) {
	void extract_pending_chars();
	
	char_data *ch, *next_ch;
	
	for (ch = character_list; ch; ch = next_ch) {
		next_ch = ch->next;
		
		if (!IS_NPC(ch) && !ch->desc) {
			perform_idle_out(ch);
		}
	}
	
	// they may be in the "pending" state
	extract_pending_chars();
}


/**
* This function restricts "crowd control" abilities to one target per room at
* a time. You should call it any time you add an affect that should be limited.
* All other chars in the room that have the same affect will be freed from it.
*
* @param char_data *victim The person that was just crowd-controlled.
* @param int atype The ATYPE_ spell type to limit.
* @return int The number of people freed by this function.
*/
int limit_crowd_control(char_data *victim, int atype) {
	char_data *iter;
	int count = 0;
	
	// sanitation
	if (!victim || !IN_ROOM(victim)) {
		return count;
	}
	
	for (iter = ROOM_PEOPLE(IN_ROOM(victim)); iter; iter = iter->next_in_room) {
		if (iter != victim && affected_by_spell(iter, atype)) {
			++count;
			affect_from_char(iter, atype, TRUE);	// sends message
			act("$n recovers.", TRUE, iter, NULL, NULL, TO_ROOM);
		}
	}
	
	return count;
}


/**
* This runs a point update (every tick) on a character. It runs on both players
* and NPCS, but players have also had their "real update" run already this
* tick.
*
* @param char_data *ch The character to update.
*/
void point_update_char(char_data *ch) {
	void despawn_mob(char_data *ch);
	extern int perform_drop(char_data *ch, obj_data *obj, byte mode, const char *sname);
	void remove_quest_items(char_data *ch);
	
	struct cooldown_data *cool, *next_cool;
	struct instance_data *inst;
	obj_data *obj, *next_obj;
	empire_data *emp;
	char_data *c;
	bool found;
	
	if (IS_NPC(ch) && FIGHTING(ch)) {
		check_pointless_fight(ch);
	}
	
	if (!IS_NPC(ch)) {
		emp = GET_LOYALTY(ch);
		
		// check bad quest items
		remove_quest_items(ch);
		
		// check way over-inventory (2x overburdened)
		if (!IS_IMMORTAL(ch) && IS_CARRYING_N(ch) > 2 * GET_LARGEST_INVENTORY(ch)) {
			found = FALSE;
			LL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
				if (IS_CARRYING_N(ch) > 2 * GET_LARGEST_INVENTORY(ch)) {
					if (!found) {
						found = TRUE;
						msg_to_char(ch, "You are way overburdened and begin losing items...\r\n");
					}
					if (perform_drop(ch, obj, SCMD_DROP, "drop") <= 0) {
						perform_drop(ch, obj, SCMD_JUNK, "lose");
					}
				}
			}
		}
		
		if (IS_BLOOD_STARVED(ch)) {
			msg_to_char(ch, "You are starving!\r\n");
		}
	
		// in an empire with Prominence?
		if (emp) {
			gain_ability_exp(ch, ABIL_PROMINENCE, 2);
			gain_ability_exp(ch, ABIL_LOCKS, 1);
		}
		// city lights after dark
		if (weather_info.sunlight == SUN_DARK) {
			gain_ability_exp(ch, ABIL_CITY_LIGHTS, 2);
		}
		else if (weather_info.sunlight == SUN_LIGHT && IS_OUTDOORS(ch)) {
			gain_ability_exp(ch, ABIL_DAYWALKING, 2);
		}
		
		gain_ability_exp(ch, ABIL_COMMERCE, 2);
		gain_ability_exp(ch, ABIL_SATED_THIRST, 2);
		
		if (GET_MOUNT_LIST(ch)) {
			gain_ability_exp(ch, ABIL_STABLEMASTER, 2);
		}
		
		if (IS_VAMPIRE(ch)) {
			gain_ability_exp(ch, ABIL_UNNATURAL_THIRST, 2);
		}
		
		if (affected_by_spell(ch, ATYPE_RADIANCE)) {
			gain_ability_exp(ch, ABIL_RADIANCE, 2);
		}
		
		gain_ability_exp(ch, ABIL_GIFT_OF_NATURE, 2);
		gain_ability_exp(ch, ABIL_ARCANE_POWER, 2);
		
		// death count decrease after 3 minutes without a death
		if (GET_RECENT_DEATH_COUNT(ch) > 0 && GET_LAST_DEATH_TIME(ch) + (3 * SECS_PER_REAL_MIN) < time(0)) {
			GET_RECENT_DEATH_COUNT(ch) -= 1;
		}
	}
	
	// check spawned
	if (REAL_NPC(ch) && !ch->desc && MOB_FLAGGED(ch, MOB_SPAWNED) && (!MOB_FLAGGED(ch, MOB_ANIMAL) || !room_has_function_and_city_ok(IN_ROOM(ch), FNC_STABLE)) && MOB_SPAWN_TIME(ch) < (time(0) - config_get_int("mob_spawn_interval") * SECS_PER_REAL_MIN)) {
		if (!GET_LED_BY(ch) && !GET_LEADING_MOB(ch) && !GET_LEADING_VEHICLE(ch) && !MOB_FLAGGED(ch, MOB_TIED)) {
			if (distance_to_nearest_player(IN_ROOM(ch)) > config_get_int("mob_despawn_radius")) {
				despawn_mob(ch);
				return;
			}
		}
	}
	
	// bloody upkeep
	if (IS_VAMPIRE(ch) && !IS_IMMORTAL(ch) && !IS_NPC(ch)) {
		GET_BLOOD(ch) -= MAX(0, GET_BLOOD_UPKEEP(ch));
		
		if (GET_BLOOD(ch) < 0) {
			out_of_blood(ch);
			return;
		}
	}
	else {	// not a vampire (or is imm/npc)
		// don't gain blood whilst being fed upon
		if (GET_FED_ON_BY(ch) == NULL) {
			GET_BLOOD(ch) = MIN(GET_BLOOD(ch) + 1, GET_MAX_BLOOD(ch));
		}
	}

	// healing for NPCs -- pcs are in real_update
	if (IS_NPC(ch)) {
		if (GET_POS(ch) >= POS_STUNNED && !FIGHTING(ch) && !GET_FED_ON_BY(ch)) {
			// verify not fighting at all
			for (c = ROOM_PEOPLE(IN_ROOM(ch)), found = FALSE; c && !found; c = c->next_in_room) {
				if (FIGHTING(c) == ch) {
					found = TRUE;
				}
			}
			
			if (!found) {
				// not fighting for a tick? full health! (and reset tags)
				free_mob_tags(&MOB_TAGGED_BY(ch));
				GET_HEALTH(ch) = GET_MAX_HEALTH(ch);
				GET_MOVE(ch) = GET_MAX_MOVE(ch);
				GET_MANA(ch) = GET_MAX_MANA(ch);
				if (GET_POS(ch) < POS_SLEEPING) {
					GET_POS(ch) = POS_STANDING;
				}
				
				// reset scaling if possible...
				if (!MOB_FLAGGED(ch, MOB_NO_RESCALE)) {
					inst = get_instance_by_id(MOB_INSTANCE_ID(ch));
					if (!inst && IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
						inst = find_instance_by_room(IN_ROOM(ch), FALSE);
					}
					// if no instance or not level-locked
					if (!inst || inst->level <= 0) {
						GET_CURRENT_SCALE_LEVEL(ch) = 0;
					}
				}
			}
		}
		else if (GET_POS(ch) < POS_STUNNED) {
			GET_HEALTH(ch) -= 1;
			update_pos(ch);
			if (GET_POS(ch) == POS_DEAD) {
				die(ch, ch);
				return;
			}
		}
	}

	// check all cooldowns -- ignore chars with descriptors, as they'll want
	// the OTHER function to remove this (it sends messages; this one includes
	// NPCs and doesn't)
	if (!ch->desc) {
		for (cool = ch->cooldowns; cool; cool = next_cool) {
			next_cool = cool->next;
		
			// is expired?
			if (time(0) >= cool->expire_time) {
				remove_cooldown(ch, cool);
			}
		}
	}

	// these must go last
	if (!IS_NPC(ch)) {
		check_idling(ch);
	}
}


/**
* Runs a real update (every 5 seconds) on ch. This is mainly for player
* updates, but NPCs hit it for things like affects.
*
* @param char_data *ch The character to update.
*/
void real_update_char(char_data *ch) {
	void adventure_unsummon(char_data *ch);
	extern bool can_wear_item(char_data *ch, obj_data *item, bool send_messages);
	void check_combat_end(char_data *ch);
	void check_morph_ability(char_data *ch);
	void combat_meter_damage_dealt(char_data *ch, int amt);
	extern int compute_bonus_exp_per_day(char_data *ch);
	void do_unseat_from_vehicle(char_data *ch);
	extern bool fail_daily_quests(char_data *ch);
	void random_encounter(char_data *ch);
	void update_biting_char(char_data *ch);
	void update_vampire_sun(char_data *ch);
	
	struct over_time_effect_type *dot, *next_dot;
	struct affected_type *af, *next_af, *immune;
	char_data *room_ch, *next_ch, *caster;
	int result, iter, type;
	int fol_count, gain;
	bool found, took_dot;
	
	// check for end of meters (in case it was missed in the fight code)
	if (!FIGHTING(ch)) {
		check_combat_end(ch);
	}
	
	// first check location: this may move the player
	if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_ADVENTURE_SUMMONED) && !find_instance_by_room(IN_ROOM(ch), FALSE)) {
		adventure_unsummon(ch);
	}
	
	if (!IS_NPC(ch) && IS_MORPHED(ch)) {
		check_morph_ability(ch);
	}
	
	if (!IS_NPC(ch) && IS_RIDING(ch)) {
		check_should_dismount(ch);
	}
	
	if (GET_LEADING_VEHICLE(ch) && IN_ROOM(ch) != IN_ROOM(GET_LEADING_VEHICLE(ch))) {
		act("You have lost $V and stop leading it.", FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_CHAR);
		VEH_LED_BY(GET_LEADING_VEHICLE(ch)) = NULL;
		GET_LEADING_VEHICLE(ch) = NULL;
	}
	if (GET_LEADING_MOB(ch) && IN_ROOM(ch) != IN_ROOM(GET_LEADING_MOB(ch))) {
		act("You have lost $N and stop leading $M.", FALSE, ch, NULL, GET_LEADING_MOB(ch), TO_CHAR);
		GET_LED_BY(GET_LEADING_MOB(ch)) = NULL;
		GET_LEADING_MOB(ch) = NULL;
	}
	if (GET_SITTING_ON(ch)) {
		// things that cancel sitting-on:
		if (IN_ROOM(ch) != IN_ROOM(GET_SITTING_ON(ch)) || GET_POS(ch) != POS_SITTING || IS_RIDING(ch) || GET_LEADING_MOB(ch) || GET_LEADING_VEHICLE(ch)) {
			do_unseat_from_vehicle(ch);
		}
	}
	
	// check master's solo role
	if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_FAMILIAR) && ch->master && !check_solo_role(ch->master)) {
		act("$N vanishes because you're in the solo role but not alone.", FALSE, ch->master, NULL, ch, TO_CHAR);
		act("$N vanishes.", FALSE, ch->master, NULL, ch, TO_NOTVICT);
		extract_char(ch);
		return;
	}
	// earthmeld damage
	if (!IS_NPC(ch) && !IS_IMMORTAL(ch) && AFF_FLAGGED(ch, AFF_EARTHMELD) && ROOM_IS_CLOSED(IN_ROOM(ch))) {
		if (!affected_by_spell(ch, ATYPE_NATURE_BURN)) {
			msg_to_char(ch, "You are beneath a building and begin taking nature burn as the earth you're buried in is separated from fresh air...\r\n");
		}
		apply_dot_effect(ch, ATYPE_NATURE_BURN, 6, DAM_MAGICAL, 5, 60, ch);
	}
	
	// update affects (NPCs get this, too)
	for (af = ch->affected; af; af = next_af) {
		next_af = af->next;
		if (af->duration >= 1) {
			af->duration--;
		}
		else if (af->duration != UNLIMITED) {
			if ((af->type > 0)) {
				if (!af->next || (af->next->type != af->type) || (af->next->duration > 0)) {
					show_wear_off_msg(ch, af->type);
				}
			}
			
			// special case -- add immunity
			if (IS_SET(af->bitvector, AFF_STUNNED) && config_get_int("stun_immunity_time") > 0) {
				immune = create_flag_aff(ATYPE_STUN_IMMUNITY, config_get_int("stun_immunity_time") / SECS_PER_REAL_UPDATE, AFF_IMMUNE_STUN, ch);
				affect_join(ch, immune, 0);
			}
			
			affect_remove(ch, af);
		}
	}
	
	// heal-per-5 ? (stops at 0 health or incap)
	if (GET_HEAL_OVER_TIME(ch) > 0 && !IS_DEAD(ch) && GET_POS(ch) >= POS_SLEEPING && GET_HEALTH(ch) > 0) {
		heal(ch, ch, GET_HEAL_OVER_TIME(ch));
	}

	// update DoTs (NPCs get this, too)
	took_dot = FALSE;
	for (dot = ch->over_time_effects; dot; dot = next_dot) {
		next_dot = dot->next;
		
		type = dot->damage_type == DAM_MAGICAL ? ATTACK_MAGICAL_DOT : (
			dot->damage_type == DAM_FIRE ? ATTACK_FIRE_DOT : (
			dot->damage_type == DAM_POISON ? ATTACK_POISON_DOT : 
			ATTACK_PHYSICAL_DOT
		));
		
		caster = find_player_in_room_by_id(IN_ROOM(ch), dot->cast_by);
		result = damage(caster ? caster : ch, ch, dot->damage * dot->stack, type, dot->damage_type);
		if (result > 0 && caster) {
			took_dot = TRUE;
		}
		else if (result < 0 || EXTRACTED(ch) || IS_DEAD(ch)) {
			return;
		}
		
		// remove?
		if (dot->duration >= 1) {
			--dot->duration;
		}
		else if (dot->duration != UNLIMITED) {
			// expired
			if (dot->type > 0) {
				show_wear_off_msg(ch, dot->type);
			}
			dot_remove(ch, dot);
		}
	}
	
	// biting -- this is usually PC-only, but NPCs could learn to do it
	if (GET_FEEDING_FROM(ch)) {
		update_biting_char(ch);
		if (EXTRACTED(ch) || IS_DEAD(ch)) {
			return;
		}
	}

	// nothing else pertains to NPCs	
	if (IS_NPC(ch)) {
		return;
	}
	
	// update recent level data if level has gone up or it's been too long since we've seen a higher level
	if (GET_COMPUTED_LEVEL(ch) > GET_HIGHEST_KNOWN_LEVEL(ch)) {
		GET_HIGHEST_KNOWN_LEVEL(ch) = GET_COMPUTED_LEVEL(ch);
	}
	// update the last-known-level
	GET_LAST_KNOWN_LEVEL(ch) = GET_COMPUTED_LEVEL(ch);
	
	// very drunk? more confused!
	if (GET_COND(ch, DRUNK) > 350) {
		GET_CONFUSED_DIR(ch) = number(0, NUM_SIMPLE_DIRS-1);
	}
	
	if (GET_EQ(ch, WEAR_SADDLE) && !IS_RIDING(ch)) {
		perform_remove(ch, WEAR_SADDLE);
	}
	
	if (GET_BLOOD(ch) > GET_MAX_BLOOD(ch)) {
		GET_BLOOD(ch) = GET_MAX_BLOOD(ch);
	}

	// periodic exp and skill gain
	if (GET_DAILY_CYCLE(ch) < data_get_long(DATA_DAILY_CYCLE)) {
		// other stuff that resets daily
		gain = compute_bonus_exp_per_day(ch);
		if (GET_DAILY_BONUS_EXPERIENCE(ch) < gain) {
			GET_DAILY_BONUS_EXPERIENCE(ch) = gain;
		}
		GET_DAILY_QUESTS(ch) = 0;
		for (iter = 0; iter < MAX_REWARDS_PER_DAY; ++iter) {
			GET_REWARDED_TODAY(ch, iter) = -1;
		}
		
		msg_to_char(ch, "&yYour daily quests and bonus experience have reset!&0\r\n");
		
		if (fail_daily_quests(ch)) {
			msg_to_char(ch, "Your daily quests expire.\r\n");
		}
		
		// update to this cycle so it only happens once a day
		GET_DAILY_CYCLE(ch) = data_get_long(DATA_DAILY_CYCLE);
	}

	/* Update conditions */
	if (IS_VAMPIRE(ch) && has_ability(ch, ABIL_UNNATURAL_THIRST)) {			
		gain_condition(ch, FULL, -1);
	}
	else {
		if (!number(0, 1)) {
			gain_condition(ch, FULL, 1);
		}
	}
		
	if (IS_DARK(IN_ROOM(ch))) {
		if (affected_by_spell(ch, ATYPE_NIGHTSIGHT)) {
			gain_ability_exp(ch, ABIL_NIGHTSIGHT, 0.5);
		}
		gain_ability_exp(ch, ABIL_PREDATOR_VISION, 0.5);
		gain_ability_exp(ch, ABIL_BY_MOONLIGHT, 0.5);
	}
	
	// more thirsty?
	if (has_ability(ch, ABIL_SATED_THIRST) || (IS_VAMPIRE(ch) && has_ability(ch, ABIL_UNNATURAL_THIRST))) {
		gain_condition(ch, THIRST, -1);
	}
	else {
		if (!number(0, 1)) {
			gain_condition(ch, THIRST, 1);
		}
	}
	
	// less drunk
	gain_condition(ch, DRUNK, AWAKE(ch) ? -1 : -6);
	
	// ensure character isn't under on primary attributes
	check_attribute_gear(ch);
	
	// ensure character isn't using any gear they shouldn't be
	found = FALSE;
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (wear_data[iter].count_stats && GET_EQ(ch, iter) && !can_wear_item(ch, GET_EQ(ch, iter), TRUE)) {
			// can_wear_item sends own message to ch
			act("$n stops using $p.", TRUE, ch, GET_EQ(ch, iter), NULL, TO_ROOM);
			// this may extract it
			unequip_char_to_inventory(ch, iter);
			found = TRUE;
		}
	}
	if (found) {
		determine_gear_level(ch);
	}

	/* moving on.. */
	if (GET_POS(ch) < POS_STUNNED || (GET_POS(ch) == POS_STUNNED && health_gain(ch, TRUE) <= 0)) {
		GET_HEALTH(ch) = MIN(0, GET_HEALTH(ch));	// fixing? a bug where a player whose health is positve but is in a bleeding out position, would not bleed out right away (but couldn't recover)
		GET_HEALTH(ch) -= 1;
		update_pos(ch);
		if (GET_POS(ch) == POS_DEAD) {
			msg_to_char(ch, "You die from your wounds!\r\n");
			act("$n falls down, dead.", FALSE, ch, 0, 0, TO_ROOM);
			death_log(ch, ch, TYPE_SUFFERING);
			die(ch, ch);
			return;
		}
		else {
			msg_to_char(ch, "You are bleeding and will die soon without aid.\r\n");
		}
		return;
	}
	else {
		// position > stunned		
	}

	// regenerate: do not put move_gain and mana_gain inside of MIN/MAX macros -- this will call them twice
	if (!took_dot) {
		gain = health_gain(ch, FALSE);
		heal(ch, ch, gain);
		GET_HEALTH_DEFICIT(ch) = MAX(0, GET_HEALTH_DEFICIT(ch) - gain);
	}
	
	gain = move_gain(ch, FALSE);
	GET_MOVE(ch) += gain;
	GET_MOVE(ch) = MIN(GET_MOVE(ch), GET_MAX_MOVE(ch));
	GET_MOVE_DEFICIT(ch) = MAX(0, GET_MOVE_DEFICIT(ch) - gain);
	
	gain = mana_gain(ch, FALSE);
	GET_MANA(ch) += gain;
	GET_MANA(ch) = MIN(GET_MANA(ch), GET_MAX_MANA(ch));
	GET_MANA_DEFICIT(ch) = MAX(0, GET_MANA_DEFICIT(ch) - gain);
	
	if (IS_VAMPIRE(ch)) {
		update_vampire_sun(ch);
	}

	if (!AWAKE(ch) && IS_MORPHED(ch) && CHAR_MORPH_FLAGGED(ch, MORPHF_NO_SLEEP)) {
		sprintf(buf, "%s has become $n!", PERS(ch, ch, 0));

		perform_morph(ch, NULL);

		act(buf, TRUE, ch, 0, 0, TO_ROOM);
		msg_to_char(ch, "You revert to normal!\r\n");
	}

	/* Blood check */
	if (GET_BLOOD(ch) <= 0 && !GET_FED_ON_BY(ch) && !GET_FEEDING_FROM(ch)) {
		out_of_blood(ch);
		return;
	}
	
	// too-many-followers check
	fol_count = 0;
	for (room_ch = ROOM_PEOPLE(IN_ROOM(ch)); room_ch; room_ch = next_ch) {
		next_ch = room_ch->next_in_room;
		
		// check is npc following ch
		if (room_ch == ch || room_ch->desc || !IS_NPC(room_ch) || room_ch->master != ch) {
			continue;
		}
		
		// don't care about familiars
		if (MOB_FLAGGED(room_ch, MOB_FAMILIAR)) {
			continue;
		}
		
		if (++fol_count > config_get_int("npc_follower_limit")) {
			REMOVE_BIT(AFF_FLAGS(room_ch), AFF_CHARM);
			stop_follower(room_ch);
			
			if (can_fight(room_ch, ch)) {
				act("$n becomes enraged!", FALSE, room_ch, NULL, NULL, TO_ROOM);
				engage_combat(room_ch, ch, TRUE);
			}
		}
	}

	random_encounter(ch);
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE LIMITS ///////////////////////////////////////////////////////////

/**
* Checks the contents of a city to see if it counts as "in ruin" and deletes
* it if so.
*
* @param empire_data *emp The empire that owns the city.
* @param struct empire_city_data *city The city to check.
* @return TRUE if it destroys the city, FALSE otherwise.
*/
static bool check_one_city_for_ruin(empire_data *emp, struct empire_city_data *city) {	
	extern struct city_metadata_type city_type[];

	room_data *to_room, *center = city->location;
	int radius = city_type[city->type].radius;
	bool found_building = FALSE;
	int x, y;
	
	for (x = -1 * radius; x <= radius && !found_building; ++x) {
		for (y = -1 * radius; y <= radius && !found_building; ++y) {
			// skip 0,0 because that is the city center
			if (x != 0 || y != 0) {
				to_room = real_shift(center, x, y);
				
				// we skip compute_distance here so we're really checking a
				// rectangle, not a circle, but we're just looking for
				// buildings belonging to this empire that are still standing,
				// so it's not an important distinction
				
				if (to_room && ROOM_OWNER(to_room) == emp) {
					// is any building, and isn't ruins?
					// TODO: maybe need a ruins flag, as we are up to 3 ruins
					if (IS_ANY_BUILDING(to_room) && !ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_DISREPAIR) && BUILDING_VNUM(to_room) != BUILDING_RUINS_OPEN && BUILDING_VNUM(to_room) != BUILDING_RUINS_FLOODED && BUILDING_VNUM(to_room) != BUILDING_RUINS_CLOSED) {
						found_building = TRUE;
					}
				}
			}
		}
	}
	
	if (!found_building) {
		log_to_empire(emp, ELOG_TERRITORY, "%s (%d, %d) abandoned as ruins", city->name, X_COORD(city->location), Y_COORD(city->location));
		perform_abandon_city(emp, city, TRUE);
		return TRUE;
	}
	
	return FALSE;
}


/**
* Looks for cities that contain no buildings and destroys them. This prevents
* old, expired empires from taking up city space near start locations, which
* is common. -paul 5/22/2014
*/
void check_ruined_cities(void) {
	struct empire_city_data *city, *next_city;
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (!EMPIRE_IMM_ONLY(emp)) {
			for (city = EMPIRE_CITY_LIST(emp); city; city = next_city) {
				next_city = city->next;
				check_one_city_for_ruin(emp, city);
			}
		}
	}
}


/**
* This function checks the expiration time on wars every 60 seconds.
*/
void check_wars(void) {
	struct empire_political_data *pol, *rev;
	empire_data *emp, *next_emp, *enemy;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		for (pol = EMPIRE_DIPLOMACY(emp); pol; pol = pol->next) {
			// check for a war that's gone on over an hour
			if (IS_SET(pol->type, DIPL_WAR) && pol->start_time < (time(0) - SECS_PER_REAL_HOUR)) {
				enemy = real_empire(pol->id);
				
				// check for no members online
				if (count_members_online(enemy) == 0) {
					// remove war, set distrust
					REMOVE_BIT(pol->type, DIPL_WAR);
					SET_BIT(pol->type, DIPL_DISTRUST);
					pol->start_time = time(0);
					
					// and back
					if ((rev = find_relation(enemy, emp))) {
						REMOVE_BIT(rev->type, DIPL_WAR);
						SET_BIT(rev->type, DIPL_DISTRUST);
						rev->start_time = time(0);
					}
					
					syslog(SYS_INFO, 0, TRUE, "DIPL: The war between %s and %s has timed out", EMPIRE_NAME(emp), EMPIRE_NAME(enemy));
					log_to_empire(emp, ELOG_DIPLOMACY, "The war with %s is over", EMPIRE_NAME(enemy));
					log_to_empire(enemy, ELOG_DIPLOMACY, "The war with %s is over", EMPIRE_NAME(emp));
				}
			}
		}
	}
}


/**
* Processes a single city reduction on an empire that is over their limit.
*
* @param empire_data *emp The empire to reduce by 1 city.
*/
static void reduce_city_overage_one(empire_data *emp) {
	struct empire_city_data *city = NULL;
	room_data *loc;

	if (!emp || EMPIRE_IMM_ONLY(emp)) {
		return;
	}
	
	// find the last city entry to remove
	if ((city = EMPIRE_CITY_LIST(emp))) {
		while (city->next) {
			city = city->next;
		}
	}
	
	// no work
	if (!city) {
		return;
	}
	
	loc = city->location;
	
	if (city->type > 0) {
		log_to_empire(emp, ELOG_TERRITORY, "%s (%d, %d) is shrinking because of too many city points in use", city->name, X_COORD(loc), Y_COORD(loc));
		city->type -= 1;
	}
	else {
		log_to_empire(emp, ELOG_TERRITORY, "%s (%d, %d) is no longer a city because of too many city points in use", city->name, X_COORD(loc), Y_COORD(loc));
		perform_abandon_city(emp, city, FALSE);
	}
	
	save_empire(emp);
}


/**
* This runs periodically to ensure that empires don't have WAY too many cities.
* It triggers on empires that meet these two criteria:
* - Have at least 2 cities
* - Are used at least 2x their currently-earned city points.
*/
void reduce_city_overages(void) {
	extern int city_points_available(empire_data *emp);
	extern int count_cities(empire_data *emp);
	extern int count_city_points_used(empire_data *emp);
	
	empire_data *iter, *next_iter;
	int used, points;
	
	HASH_ITER(hh, empire_table, iter, next_iter) {
		// only bother on !imm empires that have MORE than one city (they can always keep the last one)
		if (!EMPIRE_IMM_ONLY(iter) && count_cities(iter) > 1) {
			used = count_city_points_used(iter);
			points = city_points_available(iter);
			
			// cutoff for keeping cities is being at 2x currently-earned points
			if (used > (2 * (points + used))) {
				reduce_city_overage_one(iter);
			}
		}
	}
}


/**
* Removes 1 "outside" territory from an empire, and warns them.
*
* @param empire_data *emp The empire to reduce
*/
static void reduce_outside_territory_one(empire_data *emp) {
	extern char *get_room_name(room_data *room, bool color);
	
	struct empire_city_data *city;
	room_data *iter, *next_iter, *loc, *farthest;
	int dist, this_far, far_dist;
	bool junk;
	
	// sanity
	if (!emp || EMPIRE_IMM_ONLY(emp) || EMPIRE_OUTSIDE_TERRITORY(emp) <= land_can_claim(emp, TRUE)) {
		return;
	}
	
	farthest = NULL;
	far_dist = -1;	// always less
	
	// check the whole map to determine the farthest outside claim
	HASH_ITER(hh, world_table, iter, next_iter) {
		// map only
		if (GET_ROOM_VNUM(iter) >= MAP_SIZE) {
			continue;
		}
		loc = HOME_ROOM(iter);
		
		// if owner matches AND it's not in a city
		if (ROOM_OWNER(loc) == emp && !is_in_city_for_empire(loc, emp, FALSE, &junk)) {
			// check its distance from each city, find the city it's closest to
			this_far = MAP_SIZE;
			for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
				dist = compute_distance(loc, city->location);
				if (dist < this_far) {
					this_far = dist;
				}
			}
			
			// now compare its distance from the closest city, and find the one farthest from a city
			if (this_far > far_dist) {
				far_dist = this_far;
				farthest = loc;
			}
		}
	}
	
	if (farthest) {
		log_to_empire(emp, ELOG_TERRITORY, "Abandoning %s (%d, %d) because too much outside territory has been claimed", get_room_name(farthest, FALSE), X_COORD(farthest), Y_COORD(farthest));
		abandon_room(farthest);
	}
}


/**
* This runs periodically to reduce the overages on empire claims. It only
* takes away 1 territory per empire at a time, so that active members have
* a chance to notice and deal with their territory issues. It only reduces
* outside-of-city territory, because:
* - This helps clean up scattered claims that might block new players.
* - Claims in-city are clumped together and less of a problem when over.
*/
void reduce_outside_territory(void) {
	empire_data *iter, *next_iter;
	
	HASH_ITER(hh, empire_table, iter, next_iter) {
		if (!EMPIRE_IMM_ONLY(iter) && EMPIRE_OUTSIDE_TERRITORY(iter) > land_can_claim(iter, TRUE)) {
			reduce_outside_territory_one(iter);
		}
	}
}


/**
* Removes claimed land, one tile at a time, from very stale empires. Since
* these empires have been gone a long time, there's no need to do this in any
* fancy order.
*
* @param empire_data *emp The empire to reduce.
*/
static void reduce_stale_empires_one(empire_data *emp) {
	bool outside_only = (EMPIRE_OUTSIDE_TERRITORY(emp) > 0);
	room_data *iter, *next_iter, *found_room = NULL;
	bool junk;
	
	// try interior first -- we'll take the first secondary room we find
	if (!outside_only) {
		for (iter = interior_room_list; iter; iter = next_iter) {
			next_iter = iter->next_interior;
			
			// only want rooms owned by this empire and only if they are their own home room (like a ship)
			if (ROOM_OWNER(iter) != emp || HOME_ROOM(iter) != iter) {
				continue;
			}
			// only looking for secondary territory
			if (!ROOM_BLD_FLAGGED(iter, BLD_SECONDARY_TERRITORY)) {
				continue;
			}
		
			// found one!
			found_room = iter;
			break;
		}
	}
	
	if (!found_room) {
		// otherwise find a random room
		HASH_ITER(hh, world_table, iter, next_iter) {
			// map only
			if (GET_ROOM_VNUM(iter) >= MAP_SIZE) {
				continue;
			}
			if (ROOM_OWNER(iter) != emp) {
				continue;
			}
			// caution: do not abandon city centers this way
			if (IS_CITY_CENTER(iter)) {
				continue;
			}
			if (outside_only && is_in_city_for_empire(iter, emp, FALSE, &junk)) {
				continue;
			}
			
			// first match
			found_room = iter;
			break;
		}
	}
	
	// did we find one?
	if (found_room) {
		// this is only called on VERY stale empires (no members), so there's no real need to log this abandon
		abandon_room(found_room);
	}
}


/**
* This runs periodically to remove global claims by empires that have timed
* out completely, but aren't stale enough to delete the whole empire. The goal
* is to increase available space for active empires. An empire is timed out if
* all its members are timed out (members == 0).
*/
void reduce_stale_empires(void) {
	empire_data *iter, *next_iter;
	
	HASH_ITER(hh, empire_table, iter, next_iter) {
		if (!EMPIRE_IMM_ONLY(iter) && EMPIRE_MEMBERS(iter) == 0 && (EMPIRE_OUTSIDE_TERRITORY(iter) > 0 || EMPIRE_CITY_TERRITORY(iter) > 0)) {
			// when members hit 0, we consider the empire timed out
			reduce_stale_empires_one(iter);
		}
	}
}


/**
* determines if emp qualifies for auto-deletion
*
* @param empire_data *emp The empire to check.
* @return bool TRUE if we should delete the empire entirely, or FALSE
*/
bool should_delete_empire(empire_data *emp) {	
	// no members at all (not just timed out ones)
	if (EMPIRE_TOTAL_MEMBER_COUNT(emp) <= 0) {
		return TRUE;
	}
	
	if (EMPIRE_LAST_LOGON(emp) + (config_get_int("time_to_empire_delete") * SECS_PER_REAL_WEEK) < time(0)) {
		return TRUE;
	}
	
	return FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT LIMITS ///////////////////////////////////////////////////////////

/**
* Checks and runs an autostore for an object.
*
* @param obj_data *obj The item to check/store.
* @param bool force If TRUE, ignores timers and players present and stores all storables.
* @return bool TRUE if the item is still in the world, FALSE if it was extracted
*/
bool check_autostore(obj_data *obj, bool force) {
	extern int get_main_island(empire_data *emp);
	
	empire_data *emp = NULL;
	vehicle_data *in_veh;
	room_data *real_loc;
	obj_data *top_obj;
	bool store, unique, full;
	int islid;
	
	// easy exclusions
	if (obj->carried_by || obj->worn_by || !CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
		return TRUE;
	}
	
	// timer check (if not forced)
	if (!force && (GET_AUTOSTORE_TIMER(obj) + config_get_int("autostore_time") * SECS_PER_REAL_MIN) > time(0)) {
		return TRUE;
	}
	
	// ensure object is in a room, or in an object in a room
	top_obj = get_top_object(obj);
	real_loc = IN_ROOM(top_obj);
	in_veh = top_obj->in_vehicle;
	if (in_veh && !real_loc) {
		real_loc = IN_ROOM(in_veh);
	}
	
	// detect owner here
	if (!emp && in_veh) {
		emp = VEH_OWNER(in_veh);
	}
	if (!emp && real_loc) {
		emp = ROOM_OWNER(HOME_ROOM(real_loc));
	}
	
	// validate location
	if (in_veh && !force) {
		return TRUE;	// vehicles do their own checking and call this with force
	}
	if (!in_veh && (!real_loc || IS_ADVENTURE_ROOM(real_loc))) {
		return TRUE;
	}
		
	// never do it in front of players
	if (!force && real_loc && any_players_in_room(real_loc)) {
		return TRUE;
	}
	
	// reasons something is storable (timer was already checked)
	store = unique = FALSE;
	if (OBJ_FLAGGED(obj, OBJ_JUNK | OBJ_UNCOLLECTED_LOOT)) {
		store = TRUE;
	}
	else if (OBJ_CAN_STORE(obj)) {
		store = TRUE;
	}
	else if (IS_COINS(obj)) {
		store = TRUE;
	}
	else if (!emp) {	// no owner
		store = TRUE;
	}
	else if (OBJ_FLAGGED(obj, OBJ_NO_AUTOSTORE)) {
		// this goes after the !emp check because we "store" them on unclaimed land anyway
		// but this otherwise blocks the item from storing
		store = FALSE;
	}
	else if (UNIQUE_OBJ_CAN_STORE(obj) && real_loc && ROOM_PRIVATE_OWNER(HOME_ROOM(real_loc)) == NOBODY) {
		// store unique items but not in private homes
		store = TRUE;
		unique = TRUE;
	}
	else if (OBJ_BOUND_TO(obj) && real_loc && ROOM_PRIVATE_OWNER(HOME_ROOM(real_loc)) == NOBODY && (GET_AUTOSTORE_TIMER(obj) + config_get_int("bound_item_junk_time") * SECS_PER_REAL_MIN) < time(0)) {
		// room owned, item is bound, not a private home, but not storable? junk it
		store = TRUE;
		// DON'T mark unique -- we are just junking it
	}
	
	// do we even bother?
	if (!store) {
		return TRUE;
	}

	// final timer check (long-autostore)
	if (!force && real_loc && ROOM_BLD_FLAGGED(real_loc, BLD_LONG_AUTOSTORE) && (GET_AUTOSTORE_TIMER(obj) + config_get_int("long_autostore_time") * SECS_PER_REAL_MIN) > time(0)) {
		return TRUE;
	}
	
	// OK GO:
	empty_obj_before_extract(obj);
	
	if (emp) {					
		if (IS_COINS(obj)) {
			increase_empire_coins(emp, real_empire(GET_COINS_EMPIRE_ID(obj)), GET_COINS_AMOUNT(obj));
		}
		else if (unique) {
			// this extracts it itself
			store_unique_item(NULL, obj, emp, real_loc, &full);
			return FALSE;
		}
		else if (OBJ_CAN_STORE(obj)) {
			// find where to store it, especially if we got this far with emp but no real_loc
			islid = real_loc ? GET_ISLAND_ID(real_loc) : NO_ISLAND;
			if (islid == NO_ISLAND) {
				islid = get_main_island(emp);
			}
			
			add_to_empire_storage(emp, islid, GET_OBJ_VNUM(obj), 1);
		}
	}
	
	// get rid of it either way
	extract_obj(obj);
	return FALSE;
}


/**
* This runs a point update (every tick) on an object.
*
* @param obj_data *obj The object to update.
*/
void point_update_obj(obj_data *obj) {
	room_data *to_room, *obj_r;
	obj_data *top;
	char_data *c;
	time_t timer;
	
	// ensure this obj is actually in-game
	top = get_top_object(obj);
	if ((top->carried_by && !IN_ROOM(top->carried_by)) || (top->worn_by && !IN_ROOM(top->worn_by))) {
		return;
	}

	if (OBJ_FLAGGED(obj, OBJ_LIGHT)) {
		if (GET_OBJ_TIMER(obj) == 2) {
			if (obj->worn_by) {
				act("Your light begins to flicker and fade.", FALSE, obj->worn_by, obj, 0, TO_CHAR);
				act("$n's light begins to flicker and fade.", TRUE, obj->worn_by, obj, 0, TO_ROOM);
			}
			else if (obj->carried_by)
				act("$p begins to flicker and fade.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
			else if (IN_ROOM(obj))
				if (ROOM_PEOPLE(IN_ROOM(obj)))
					act("$p begins to flicker and fade.", FALSE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_CHAR | TO_ROOM);
		}
		else if (GET_OBJ_TIMER(obj) == 1) {
			if (obj->worn_by) {
				act("Your light burns out.", FALSE, obj->worn_by, obj, 0, TO_CHAR);
				act("$n's light burns out.", TRUE, obj->worn_by, obj, 0, TO_ROOM);
			}
			else if (obj->carried_by) {
				act("$p burns out.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
			}
			else if (IN_ROOM(obj)) {
				if (ROOM_PEOPLE(IN_ROOM(obj)))
					act("$p burns out.", FALSE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_CHAR | TO_ROOM);
			}
		}
	}

	// float or sink
	if (IN_ROOM(obj) && CAN_WEAR(obj, ITEM_WEAR_TAKE) && ROOM_SECT_FLAGGED(IN_ROOM(obj), SECTF_FRESH_WATER | SECTF_OCEAN)) {
		if (materials[GET_OBJ_MATERIAL(obj)].floats && (to_room = real_shift(IN_ROOM(obj), shift_dir[WEST][0], shift_dir[WEST][1]))) {
			if (!number(0, 2) && !ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !ROOM_IS_CLOSED(to_room)) {
				// float-west message
				for (c = ROOM_PEOPLE(IN_ROOM(obj)); c; c = c->next_in_room) {
					if (c->desc) {
						sprintf(buf, "$p floats %s.", dirs[get_direction_for_char(c, WEST)]);
						act(buf, TRUE, c, obj, NULL, TO_CHAR);
					}
				}
				
				// move object but keep autostore
				timer = GET_AUTOSTORE_TIMER(obj);
				obj_to_room(obj, to_room);
				GET_AUTOSTORE_TIMER(obj) = timer;
				
				// floats-in message
				for (c = ROOM_PEOPLE(IN_ROOM(obj)); c; c = c->next_in_room) {
					if (c->desc) {
						sprintf(buf, "$p floats in from %s.", from_dir[get_direction_for_char(c, WEST)]);
						act(buf, TRUE, c, obj, NULL, TO_CHAR);
					}
				}
			}
		}
		else { // does not float, or could not find a new room to float it to
			if (ROOM_PEOPLE(IN_ROOM(obj))) {
				act("$p sinks quickly to the bottom.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_CHAR | TO_ROOM);
			}
			extract_obj(obj);
			return;
		}
	}

	/* timer count down */
	if (GET_OBJ_TIMER(obj) > 0) {
		GET_OBJ_TIMER(obj)--;
	}

	if (GET_OBJ_TIMER(obj) == 0) {
		obj_r = obj_room(obj);
		
		if (!timer_otrigger(obj) || obj_room(obj) != obj_r) {
			return;
		}
		
		if (IS_DRINK_CONTAINER(obj)) {
			if (GET_DRINK_CONTAINER_CONTENTS(obj) > 0) {
				if (obj->carried_by) {
					act("$p has gone bad and you pour it out.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
				}
				else if (obj->worn_by) {
					act("$p has gone bad and you pour it out.", FALSE, obj->worn_by, obj, 0, TO_CHAR);
				}
			}
			
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = 0;
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = 0;
			
			GET_OBJ_TIMER(obj) = UNLIMITED;
			
			// do not extract
		}
		else {  // all others actually decay
			switch (GET_OBJ_MATERIAL(obj)) {
				case MAT_FLESH:
					if (obj->carried_by)
						act("$p decays in your hands.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
					else if (obj->worn_by)
						act("$p decays in your hands.", FALSE, obj->worn_by, obj, 0, TO_CHAR);
					else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
						act("A quivering horde of maggots consumes $p.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_ROOM);
						act("A quivering horde of maggots consumes $p.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_CHAR);
					}
					break;
				case MAT_IRON:
					if (obj->carried_by)
						act("$p rusts in your hands.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
					else if (obj->worn_by)
						act("$p rusts in your hands.", FALSE, obj->worn_by, obj, 0, TO_CHAR);
					else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
						act("$p rusts and disintegrates.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_ROOM);
						act("$p rusts and disintegrates.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_CHAR);
					}
					break;
				case MAT_ROCK:
				case MAT_FLINT:
					if (obj->carried_by)
						act("$p disintegrates in your hands.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
					else if (obj->worn_by)
						act("$p disintegrates in your hands.", FALSE, obj->worn_by, obj, 0, TO_CHAR);
					else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
						act("$p disintegrates.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_ROOM);
						act("$p disintegrates.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_CHAR);
					}
					break;
				case MAT_GOLD:
				case MAT_SILVER:
				case MAT_COPPER:
				case MAT_CLAY:
					if (obj->carried_by)
						act("$p cracks and disintegrates in your hands.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
					else if (obj->worn_by)
						act("$p cracks and disintegrates in your hands.", FALSE, obj->worn_by, obj, 0, TO_CHAR);
					else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
						act("$p cracks and disintegrates.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_ROOM);
						act("$p cracks and disintegrates.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_CHAR);
					}
					break;
				case MAT_MAGIC:
					if (obj->carried_by)
						act("$p flickers briefly in your hands, then vanishes with a poof.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
					else if (obj->worn_by)
						act("$p flickers briefly in your hands, then vanishes with a poof.", FALSE, obj->worn_by, obj, 0, TO_CHAR);
					else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
						act("$p flickers briefly, then vanishes with a poof.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_ROOM);
						act("$p flickers briefly, then vanishes with a poof.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_CHAR);
					}
					break;
				case MAT_WAX: {
					if (obj->carried_by)
						act("$p melts in your hands and is gone.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
					else if (obj->worn_by)
						act("$p melts off of you and is gone.", FALSE, obj->worn_by, obj, 0, TO_CHAR);
					else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
						act("$p melts and is gone.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_ROOM);
						act("$p melts and is gone.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_CHAR);
					}
					break;
				}
				case MAT_WOOD:
				case MAT_BONE:
				case MAT_HAIR:
				default:
					if (obj->carried_by)
						act("$p rots in your hands.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
					else if (obj->worn_by)
						act("$p rots in your hands.", FALSE, obj->worn_by, obj, 0, TO_CHAR);
					else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
						act("$p rots and disintegrates.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_ROOM);
						act("$p rots and disintegrates.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_CHAR);
					}
					break;
			}

			empty_obj_before_extract(obj);
			extract_obj(obj);
			return;
		} // end non-drink decay
	}
	
	// 2nd type of timer: the autostore timer (keeps the world clean of litter)
	if (!check_autostore(obj, FALSE)) {
		return;
	}
}


/**
* Real update (per 5 seconds) on an object. This is only for things that need
* to happen that often. Everything else happens in a point update.
*
* @param obj_data *obj The object to update.
*/
void real_update_obj(obj_data *obj) {
	struct empire_political_data *pol;
	empire_data *emp, *enemy;
	room_data *home;
	
	// burny
	if (OBJ_FLAGGED(obj, OBJ_LIGHT) && IN_ROOM(obj) && IS_ANY_BUILDING(IN_ROOM(obj))) {
		home = HOME_ROOM(IN_ROOM(obj));
		if (ROOM_BLD_FLAGGED(home, BLD_BURNABLE) && !BUILDING_BURNING(home)) {
			// only items with an empire id are considered: you can't burn stuff down by accident (unless the building is unowned)
			if (obj->last_empire_id != NOTHING || !ROOM_OWNER(home)) {
				// check that the empire is at war
				emp = ROOM_OWNER(home);
				enemy = real_empire(obj->last_empire_id);
				
				// check for war
				if (!emp || (enemy && (pol = find_relation(enemy, emp)) && IS_SET(pol->type, DIPL_WAR))) {
					// TODO magic number -- this should be a config
					COMPLEX_DATA(home)->burning = number(4, 12);
					if (ROOM_PEOPLE(home)) {
						act("A stray ember from $p ignites the room!", FALSE, ROOM_PEOPLE(home), obj, 0, TO_CHAR | TO_ROOM);

						// ensure no building or dismantling
						stop_room_action(home, ACT_BUILDING, CHORE_BUILDING);
						stop_room_action(home, ACT_DISMANTLING, CHORE_BUILDING);
					}
				}
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ROOM LIMITS /////////////////////////////////////////////////////////////

/**
* Point update (per mud hour / 75 seconds) for each room.
*
* @param room_data *room The room to update.
*/
void point_update_room(room_data *room) {
	void death_log(char_data *ch, char_data *killer, int type);
	void fill_trench(room_data *room);

	char_data *ch, *next_ch, *sub_ch, *next_sub;
	obj_data *o, *next_o;
	struct track_data *track, *next_track, *temp;
	struct affected_type *af, *next_af;
	empire_data *emp;
	time_t now = time(0);
	bool junk;
	int count;
	
	int allowed_animals = config_get_int("num_duplicates_in_stable");

	// map-only portion
	if (GET_ROOM_VNUM(room) < MAP_SIZE) {
		if (ROOM_SECT_FLAGGED(room, SECTF_IS_TRENCH) && get_room_extra_data(room, ROOM_EXTRA_TRENCH_PROGRESS) >= 0) {
			if (weather_info.sky >= SKY_RAINING) {
				add_to_room_extra_data(room, ROOM_EXTRA_TRENCH_PROGRESS, config_get_int("trench_gain_from_rain"));
				if (get_room_extra_data(room, ROOM_EXTRA_TRENCH_PROGRESS) >= config_get_int("trench_full_value")) {
					fill_trench(room);
				}
			}
			
			// still a trench? (may have been filled by rain)
			if (ROOM_SECT_FLAGGED(room, SECTF_IS_TRENCH)) {
				if (find_flagged_sect_within_distance_from_room(room, SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_SHALLOW_WATER, NOBITS, 1)) {
					fill_trench(room);
				}
			}
		}

		if (room_affected_by_spell(room, ATYPE_DARKNESS) && weather_info.sunlight != SUN_DARK && !number(0, 1)) {
			affect_from_room(room, ATYPE_DARKNESS);
			if (ROOM_PEOPLE(room))
				act("The darkness dissipates.", FALSE, ROOM_PEOPLE(room), 0, 0, TO_CHAR | TO_ROOM);
		}

		if (COMPLEX_DATA(room) && HOME_ROOM(room) == room && BUILDING_BURNING(room)) {
			/* Reduce by one tick */
			--COMPLEX_DATA(room)->burning;
		
			emp = ROOM_OWNER(room);
			if (emp) {
				log_to_empire(emp, ELOG_HOSTILITY, "Building on fire at (%d, %d) -- douse it quickly", X_COORD(room), Y_COORD(room));
			}

			/* Time's up! */
			if (BUILDING_BURNING(room) == 0) {
				if (ROOM_IS_CLOSED(room)) {
					if (ROOM_PEOPLE(room)) {
						act("The building collapses in flames around you!", FALSE, ROOM_PEOPLE(room), 0, 0, TO_CHAR | TO_ROOM);
					}
					for (ch = ROOM_PEOPLE(room); ch; ch = next_ch) {
						next_ch = ch->next_in_room;
						
						if (!IS_NPC(ch)) {
							death_log(ch, ch, TYPE_SUFFERING);
						}
						die(ch, ch);
					}
				}
				else {
					// not closed
					if (ROOM_PEOPLE(room)) {
						act("The building burns to the ground!", FALSE, ROOM_PEOPLE(room), 0, 0, TO_CHAR | TO_ROOM);
					}
				}
			
				disassociate_building(room);
				
				// auto-abandon if not in city
				if (emp && !is_in_city_for_empire(room, emp, TRUE, &junk)) {
					// does check the city time limit for abandon protection
					abandon_room(room);
				}

				/* Destroy 50% of the objects */
				for (o = ROOM_CONTENTS(room); o; o = next_o) {
					next_o = o->next_content;
					if (!number(0, 1)) {
						extract_obj(o);
					}
				}

				return;
			}

			/* Otherwise, just send a message */
			if (ROOM_PEOPLE(room)) {
				if (ROOM_IS_CLOSED(room)) {
					act("The walls crackle and crisp as they burn!", FALSE, ROOM_PEOPLE(room), 0, 0, TO_CHAR | TO_ROOM);
				}
				else {
					act("The fire rages as the building burns!", FALSE, ROOM_PEOPLE(room), 0, 0, TO_CHAR | TO_ROOM);
				}
			}
		}
	}

	// For remaining interior rooms, make sure everyone knows the place is crispy
	if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		if (HOME_ROOM(room) != room && BUILDING_BURNING(room) && ROOM_PEOPLE(room)) {
			act("The walls crackle and crisp as they burn!", FALSE, ROOM_PEOPLE(room), 0, 0, TO_CHAR | TO_ROOM);
		}
	}

	// WHOLE WORLD:
	
	// check tracks
	for (track = ROOM_TRACKS(room); track; track = next_track) {
		next_track = track->next;
		
		if (now - track->timestamp > config_get_int("tracks_lifespan") * SECS_PER_REAL_MIN) {
			REMOVE_FROM_LIST(track, ROOM_TRACKS(room), next);
			free(track);
		}
	}
	
	// check mob crowding
	if (HAS_FUNCTION(room, FNC_STABLE)) {
		for (ch = ROOM_PEOPLE(room); ch; ch = next_ch) {
			next_ch = ch->next_in_room;
			
			// skip non-npcs and familiars
			if (!IS_NPC(ch) || MOB_FLAGGED(ch, MOB_FAMILIAR)) {
				continue;
			}
			
			// look for more here than allowed
			count = 1;
			for (sub_ch = ch->next_in_room; sub_ch; sub_ch = next_sub) {
				next_sub = sub_ch->next_in_room;
				
				// only looking for dupes
				if (!IS_NPC(sub_ch) || sub_ch->desc || GET_MOB_VNUM(sub_ch) != GET_MOB_VNUM(ch)) {
					continue;
				}
				
				if (count++ >= allowed_animals) {
					act("$n is feeling overcrowded, and leaves.", TRUE, sub_ch, NULL, NULL, TO_ROOM);
					extract_char(sub_ch);
				}
			}
		}
	}

	// update room ffects
	for (af = ROOM_AFFECTS(room); af; af = next_af) {
		next_af = af->next;
		if (af->duration >= 1) {
			af->duration--;
		}
		else if (af->duration != UNLIMITED) {
			if ((af->type > 0)) {
				if (!af->next || (af->next->type != af->type) || (af->next->duration > 0)) {
					if (*affect_wear_off_msgs[af->type] && ROOM_PEOPLE(room)) {
						act(affect_wear_off_msgs[af->type], FALSE, ROOM_PEOPLE(room), 0, 0, TO_CHAR | TO_ROOM);
					}
				}
			}
			affect_remove_room(room, af);
		}
	}
	
	// ensure these are up to date
	SET_BIT(ROOM_AFF_FLAGS(room), ROOM_BASE_FLAGS(room));
}


 //////////////////////////////////////////////////////////////////////////////
//// VEHICLE LIMITS //////////////////////////////////////////////////////////

/**
* Attempts to autostore the contents of the vehicle. This will check for
* players present first.
*
* @param vehicle_data *veh The vehicle to autostore.
*/
void autostore_vehicle_contents(vehicle_data *veh) {
	struct vehicle_room_list *vrl;
	obj_data *obj, *next_obj;
	
	// things that block autostore
	if (IN_ROOM(veh) && any_players_in_room(IN_ROOM(veh))) {
		return;
	}
	if (VEH_SITTING_ON(veh) && !IS_NPC(VEH_SITTING_ON(veh))) {
		return;
	}
	if (VEH_DRIVER(veh) && !IS_NPC(VEH_DRIVER(veh))) {
		return;
	}
	if (VEH_LED_BY(veh) && !IS_NPC(VEH_LED_BY(veh))) {
		return;
	}
	LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
		if (any_players_in_room(vrl->room)) {
			return;
		}
	}
	
	// ok we are good to autostore
	LL_FOREACH_SAFE2(VEH_CONTAINS(veh), obj, next_obj, next_content) {
		check_autostore(obj, TRUE);
	}
}


/**
* This runs an hourly "point update" on a vehicle.
*
* @param vehicle_data *veh The vehicle to update.
*/
void point_update_vehicle(vehicle_data *veh) {
	bool besiege_vehicle(vehicle_data *veh, int damage, int siege_type);
	
	// autostore
	if ((time(0) - VEH_LAST_MOVE_TIME(veh)) > (config_get_int("autostore_time") * SECS_PER_REAL_MIN)) {
		autostore_vehicle_contents(veh);
	}

	// burny burny burny! (do this last)
	if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			act("The flames roar as they envelop $V!", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM);
		}
		if (!besiege_vehicle(veh, MAX(1, (VEH_MAX_HEALTH(veh) / 12)), SIEGE_BURNING)) {
			// extracted
			return;
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS LIMITS ////////////////////////////////////////////////////

/**
* Determines if a player can legally teleport to a location.
*
* @param char_data *ch The teleporter.
* @param room_data *loc The target location.
* @param bool check_owner If TRUE, also checks can-use-room permission.
* @return bool TRUE if you can legally teleport there, otherwise FALSE.
*/
bool can_teleport_to(char_data *ch, room_data *loc, bool check_owner) {
	extern bool can_enter_instance(char_data *ch, struct instance_data *inst);
	
	struct instance_data *inst;
	char_data *mob;
	
	// immortals except
	if (IS_IMMORTAL(ch)) {
		return TRUE;
	}
	
	if (RMT_FLAGGED(loc, RMT_NO_TELEPORT) || ROOM_AFF_FLAGGED(loc, ROOM_AFF_NO_TELEPORT)) {
		return FALSE;
	}

	// permissions, maybe
	if (check_owner && !can_use_room(ch, loc, GUESTS_ALLOWED)) {
		return FALSE;
	}
	
	// player limit, maybe
	if (IS_ADVENTURE_ROOM(loc) && (inst = find_instance_by_room(loc, FALSE))) {
		// only if not already in there
		if (!IS_ADVENTURE_ROOM(IN_ROOM(ch)) || find_instance_by_room(IN_ROOM(ch), FALSE) != inst) {
			if (!can_enter_instance(ch, inst)) {
				return FALSE;
			}
		}
	}
	
	// !teleport mob?
	for (mob = ROOM_PEOPLE(loc); mob; mob = mob->next_in_room) {
		if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_NO_TELEPORT)) {
			return FALSE;
		}
	}
	
	return TRUE;
}


/**
* Called periodically to time out any expired trades.
*/
void update_trading_post(void) {
	void expire_trading_post_item(struct trading_post_data *tpd);
	void save_trading_post();
	
	struct trading_post_data *tpd, *next_tpd, *temp;
	int *notify_list = NULL, top_notify = -1;
	bool changed = FALSE;
	char_data *ch;
	int iter, sub, count;
	long diff;
	
	int trading_post_days_to_timeout = config_get_int("trading_post_days_to_timeout");
	
	for (tpd = trading_list; tpd; tpd = next_tpd) {
		next_tpd = tpd->next;
		diff = time(0) - tpd->start;
		
		if ((!IS_SET(tpd->state, TPD_FOR_SALE | TPD_OBJ_PENDING | TPD_COINS_PENDING) || diff >= (trading_post_days_to_timeout * SECS_PER_REAL_DAY)) && !is_playing(tpd->player)) {
			// expired (or missing a flag that indicates it's still relevant), remove completely
			if (tpd->obj) {
				add_to_object_list(tpd->obj);
				extract_obj(tpd->obj);
				tpd->obj = NULL;
			}
			
			REMOVE_FROM_LIST(tpd, trading_list, next);
			free(tpd);
			changed = TRUE;
		}
		else if (diff >= (tpd->for_hours * SECS_PER_REAL_HOUR) && IS_SET(tpd->state, TPD_FOR_SALE)) {
			// sale ended with no buyer
			expire_trading_post_item(tpd);
			
			if (notify_list) {
				RECREATE(notify_list, int, top_notify + 2);
			}
			else {
				CREATE(notify_list, int, top_notify + 2);
			}
			notify_list[++top_notify] = tpd->player;
			changed = TRUE;
		}
		// in all other cases, it's ok to leave it
	}
	
	// alert players to expiries
	if (notify_list) {
		for (iter = 0; iter <= top_notify; ++iter) {
			if (notify_list[iter] != NOBODY) {
				// tally and prevent dupes
				count = 1;
				for (sub = iter+1; sub <= top_notify; ++sub) {
					if (notify_list[sub] == notify_list[iter]) {
						++count;
						notify_list[sub] = NOBODY;
					}
				}
			
				if ((ch = is_playing(notify_list[iter]))) {
					msg_to_char(ch, "%d of your auctions %s expired.\r\n", count, (count != 1 ? "have" : "has"));
				}
			}
		}
		
		free(notify_list);
		notify_list = NULL;
	}
	
	// save as needed
	if (changed) {
		save_trading_post();
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// PERIODIC GAINERS ////////////////////////////////////////////////////////

/**
* This function is used to make a person hungrier or less hungry (or thirsty,
* or drunk). If the player becomes hungrier or thirstier, it may also send a
* warning message. It also sends a message when a drunk person becomes sober.
*
* @param char_data *ch The player.
* @param int condition FULL, THIRST, DRUNK
* @param int value The amount to gain or lose
*/
void gain_condition(char_data *ch, int condition, int value) {
	extern bool gain_cond_messsage;
	bool intoxicated;

	// no change?
	if (IS_NPC(ch) || GET_COND(ch, condition) == UNLIMITED) {
		return;
	}
	
	// things that prevent thirst
	if (value > 0 && condition == THIRST && (has_ability(ch, ABIL_SATED_THIRST) || has_ability(ch, ABIL_UNNATURAL_THIRST))) {
		return;
	}
	
	// things that prevent hunger
	if (value > 0 && condition == FULL && has_ability(ch, ABIL_UNNATURAL_THIRST)) {
		return;
	}

	intoxicated = (GET_COND(ch, DRUNK) > 0);

	GET_COND(ch, condition) += value;

	GET_COND(ch, condition) = MAX(0, GET_COND(ch, condition));
	GET_COND(ch, condition) = MIN(MAX_CONDITION, GET_COND(ch, condition));
	
	// prevent well-fed if hungry
	if (IS_HUNGRY(ch) && value > 0) {
		affect_from_char(ch, ATYPE_WELL_FED, TRUE);
	}

	if (!gain_cond_messsage) {
		return;
	}

	switch (condition) {
		case FULL: {
			if (IS_HUNGRY(ch) && value > 0) {
				msg_to_char(ch, "You are hungry.\r\n");
			}
			return;
		}
		case THIRST: {
			if (IS_THIRSTY(ch) && value > 0) {
				msg_to_char(ch, "You are thirsty.\r\n");
			}
			break;
		}
		case DRUNK: {
			if (intoxicated && !GET_COND(ch, condition)) {
				msg_to_char(ch, "You are now sober.\r\n");
			}
			break;
		}
	}
}


/**
* health per 5 seconds
*
* @param char_data *ch the person
* @param bool info_only TRUE = no skillups
* @return int How much health to gain per 5 seconds.
*/
int health_gain(char_data *ch, bool info_only) {
	double gain, min;
	
	// no health gain in combat
	if (GET_POS(ch) == POS_FIGHTING || FIGHTING(ch) || IS_INJURED(ch, INJ_STAKED | INJ_TIED)) {
		return 0;
	}
	
	if (IS_NPC(ch)) {
		gain = get_approximate_level(ch) / 10.0;
		gain += GET_HEALTH_REGEN(ch);
	}
	else {
		gain = regen_by_pos[(int) GET_POS(ch)];
		gain += GET_HEALTH_REGEN(ch);
		
		if (HAS_BONUS_TRAIT(ch, BONUS_HEALTH_REGEN)) {
			gain += 1 + (get_approximate_level(ch) / 20);
		}
		
		if (GET_FEEDING_FROM(ch) && has_ability(ch, ABIL_SANGUINE_RESTORATION)) {
			gain *= 4;
		}
		
		if (GET_POS(ch) == POS_SLEEPING && !AFF_FLAGGED(ch, AFF_EARTHMELD)) {
			min = round((double) GET_MAX_HEALTH(ch) / ((double) config_get_int("max_sleeping_regen_time") / (room_has_function_and_city_ok(IN_ROOM(ch), FNC_BEDROOM) ? 2.0 : 1.0) / SECS_PER_REAL_UPDATE));
			gain = MAX(gain, min);
		}
		
		// put this last
		if (IS_HUNGRY(ch) || IS_THIRSTY(ch) || IS_BLOOD_STARVED(ch)) {
			gain /= 4;
		}
	}
	
	return MAX(0, (int)gain);
}


/**
* mana per 5 seconds
*
* @param char_data *ch the person
* @param bool info_only TRUE = no skillups
* @return int How much mana to gain per 5 seconds.
*/
int mana_gain(char_data *ch, bool info_only) {
	double gain, min;
	
	double solar_power_levels[] = { 2, 2.5, 2.5 };
	
	if (IS_INJURED(ch, INJ_STAKED | INJ_TIED)) {
		return 0;
	}
	
	if (IS_NPC(ch)) {
		gain = get_approximate_level(ch) / 10.0;
		gain += GET_MANA_REGEN(ch);
	}
	else {
		gain = regen_by_pos[(int) GET_POS(ch)];
		gain += GET_MANA_REGEN(ch);
		
		if (has_ability(ch, ABIL_SOLAR_POWER)) {
			if (IS_CLASS_ABILITY(ch, ABIL_SOLAR_POWER) || check_sunny(IN_ROOM(ch))) {
				gain *= CHOOSE_BY_ABILITY_LEVEL(solar_power_levels, ch, ABIL_SOLAR_POWER);
			
				if (!info_only) {
					gain_ability_exp(ch, ABIL_SOLAR_POWER, 1);
				}
			}
		}
		
		if (HAS_BONUS_TRAIT(ch, BONUS_MANA_REGEN)) {
			gain += 1 + (get_approximate_level(ch) / 20);
		}
		if (GET_FEEDING_FROM(ch) && has_ability(ch, ABIL_SANGUINE_RESTORATION)) {
			gain *= 4;
		}
		
		if (GET_POS(ch) == POS_SLEEPING && !AFF_FLAGGED(ch, AFF_EARTHMELD)) {
			min = round((double) GET_MAX_MANA(ch) / ((double) config_get_int("max_sleeping_regen_time") / (room_has_function_and_city_ok(IN_ROOM(ch), FNC_BEDROOM) ? 2.0 : 1.0) / SECS_PER_REAL_UPDATE));
			gain = MAX(gain, min);
		}
		
		// this goes last
		if (IS_HUNGRY(ch) || IS_THIRSTY(ch) || IS_BLOOD_STARVED(ch)) {
			gain /= 4;
		}
	}
	
	return MAX(0, (int)gain);
}


/**
* move per 5 seconds
*
* @param char_data *ch the person
* @param bool info_only TRUE = no skillups
* @return int How much move to gain per 5 seconds.
*/
int move_gain(char_data *ch, bool info_only) {
	double gain, min;
	
	if (IS_INJURED(ch, INJ_STAKED | INJ_TIED)) {
		return 0;
	}

	if (IS_NPC(ch)) {
		gain = get_approximate_level(ch) / 10.0;
		gain += GET_MOVE_REGEN(ch);
	}
	else {
		gain = regen_by_pos[(int) GET_POS(ch)];
		gain += GET_MOVE_REGEN(ch);
		
		if (has_ability(ch, ABIL_STAMINA)) {
			gain *= 2;
			
			if (GET_MOVE(ch) < GET_MAX_MOVE(ch) && !info_only) {
				gain_ability_exp(ch, ABIL_STAMINA, 1);
			}
		}
		
		if (HAS_BONUS_TRAIT(ch, BONUS_MOVE_REGEN)) {
			gain += 1 + (get_approximate_level(ch) / 20);
		}
		if (GET_FEEDING_FROM(ch) && has_ability(ch, ABIL_SANGUINE_RESTORATION)) {
			gain *= 4;
		}
		
		if (GET_POS(ch) == POS_SLEEPING && !AFF_FLAGGED(ch, AFF_EARTHMELD)) {
			min = round((double) GET_MAX_MOVE(ch) / ((double) config_get_int("max_sleeping_regen_time") / (room_has_function_and_city_ok(IN_ROOM(ch), FNC_BEDROOM) ? 2.0 : 1.0) / SECS_PER_REAL_UPDATE));
			gain = MAX(gain, min);
		}

		if (IS_HUNGRY(ch) || IS_THIRSTY(ch) || IS_BLOOD_STARVED(ch)) {
			gain /= 4;
		}
	}

	return MAX(0, (int)gain);
}

 //////////////////////////////////////////////////////////////////////////////
//// CORE PERIODICALS ////////////////////////////////////////////////////////

/**
* Point Update: runs once per tick (75 seconds). This also calls the "real"
* update for that tick, to avoid iterating a second time over the same data.
*/
void point_update(bool run_real) {
	void clean_offers(char_data *ch);
	void setup_daily_quest_cycles(int only_cycle);
	void update_players_online_stats();
	
	vehicle_data *veh, *next_veh;
	room_data *room, *next_room;
	obj_data *obj, *next_obj;
	char_data *ch, *next_ch;
	
	long daily_cycle = data_get_long(DATA_DAILY_CYCLE);
	
	// check if the skill cycle must reset (daily)
	if (time(0) > daily_cycle + SECS_PER_REAL_DAY) {
		// put this in a while so that it doesn't repeatedly update if the mud is down for more than a day
		// but it only adds 1 day at a time so that the cycle time doesn't move
		while (time(0) > daily_cycle + SECS_PER_REAL_DAY) {
			daily_cycle += SECS_PER_REAL_DAY;
		}
		data_set_long(DATA_DAILY_CYCLE, daily_cycle);
		
		// reset players seen today too
		data_set_int(DATA_MAX_PLAYERS_TODAY, 0);
		update_players_online_stats();
		setup_daily_quest_cycles(NOTHING);
	}
	
	// characters
	for (ch = character_list; ch; ch = next_ch) {
		next_ch = ch->next;
		
		// remove stale offers -- this needs to happen even if dead (resurrect)
		// TODO shouldn't this logic be inside the point_update_char function?
		if (!IS_NPC(ch)) {
			clean_offers(ch);
		}
		
		if (EXTRACTED(ch)) {
			continue;
		}
		if (IS_DEAD(ch)) {
			check_idling(ch);
			continue;
		}
		
		real_update_char(ch);
		point_update_char(ch);
	}
	
	// vehicles
	LL_FOREACH_SAFE(vehicle_list, veh, next_veh) {
		point_update_vehicle(veh);
	}
	
	// objs
	for (obj = object_list; obj; obj = next_obj) {
		next_obj = obj->next;
		
		real_update_obj(obj);
		point_update_obj(obj);
	}
	
	// rooms
	HASH_ITER(hh, world_table, room, next_room) {
		point_update_room(room);
	}
}


/**
* Real Update: runs every 5 seconds, primarily for player characters and
* affects.
*/
void real_update(void) {
	obj_data *obj, *next_obj;
	char_data *ch, *next_ch;

	// characters
	for (ch = character_list; ch; ch = next_ch) {
		next_ch = ch->next;
		
		if (EXTRACTED(ch) || IS_DEAD(ch)) {
			continue;
		}

		real_update_char(ch);
	}

	// objs
	for (obj = object_list; obj; obj = next_obj) {
		next_obj = obj->next;
		real_update_obj(obj);
	}
}
