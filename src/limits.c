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
#include "dg_event.h"
#include "dg_scripts.h"
#include "constants.h"

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

// external funcs
ACMD(do_dismount);
ACMD(do_respawn);

// local vars
int point_update_cycle = 0;	// helps spread out point updates

// local funcs
bool tick_obj_timer(obj_data *obj);


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER LIMITS ////////////////////////////////////////////////////////

/**
* This checks for players whose primary attributes have dropped below 1, and
* unequips gear to compensate.
*
* @param char_data *ch The player to check for attributes.
*/
void check_attribute_gear(char_data *ch) {
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
* Checks if the player needs their daily cycle reset, and resets it if so (with
* a message).
*
* @param char_data *ch The player.
*/
void check_daily_cycle_reset(char_data *ch) {
	int gain;
	
	if (!IS_NPC(ch) && GET_DAILY_CYCLE(ch) < data_get_long(DATA_DAILY_CYCLE)) {
		// other stuff that resets daily
		gain = compute_bonus_exp_per_day(ch);
		if (GET_DAILY_BONUS_EXPERIENCE(ch) < gain) {
			GET_DAILY_BONUS_EXPERIENCE(ch) = gain;
			update_MSDP_bonus_exp(ch, UPDATE_SOON);
		}
		GET_DAILY_QUESTS(ch) = 0;
		GET_EVENT_DAILY_QUESTS(ch) = 0;
	
		msg_to_char(ch, "\tjYour daily quests and bonus experience have reset!\t0\r\n");
		
		if (fail_daily_quests(ch, TRUE) | fail_daily_quests(ch, FALSE)) {
			msg_to_char(ch, "\tjYour daily quests expire.\t0\r\n");
		}
	
		// update to this cycle so it only happens once a day
		GET_DAILY_CYCLE(ch) = data_get_long(DATA_DAILY_CYCLE);
		queue_delayed_update(ch, CDU_SAVE);
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
* This checks if a character is too idle and disconnects or times them out. It
* should be called ONLY in the "real updates" (generally every 5 seconds).
*
* @param char_data *ch The person to check.
* @return bool TRUE if the player is still in-game, FALSE if they idled out.
*/
bool check_idling(char_data *ch) {
	if (IS_NPC(ch)) {
		return TRUE;
	}

	GET_IDLE_SECONDS(ch) += SECS_PER_REAL_UPDATE;
	
	// delay idle-out if active and acting
	if (ch->desc && GET_ACTION(ch) != ACT_NONE && (GET_IDLE_SECONDS(ch) / SECS_PER_REAL_MIN) < config_get_int("idle_action_rent_time")) {
		return TRUE;
	}

	if ((GET_IDLE_SECONDS(ch) / SECS_PER_REAL_MIN) > (ch->desc ? config_get_int("idle_rent_time") : config_get_int("idle_linkdead_rent_time"))) {
		return perform_idle_out(ch);
	}
	
	// survived this long
	return TRUE;
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
	
	#define IS_POINTLESS(ch)  (GET_HEALTH(ch) <= 0 || MOB_FLAGGED((ch), MOB_NO_ATTACK))
	
	if (!FIGHTING(mob) || !IS_POINTLESS(mob)) {
		return;	// mob is not pointless (or not fighting)
	}
	if (FIGHTING(FIGHTING(mob)) && !IS_POINTLESS(FIGHTING(mob))) {
		return;	// mob is fighting a non-pointless enemy
	}
	
	any = FALSE;
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(mob)), iter, next_in_room) {
		if (FIGHTING(iter) && (!IS_POINTLESS(iter) || !IS_POINTLESS(FIGHTING(iter)))) {
			any = TRUE;
			break;
		}
	}
	
	// did with find ANY non-pointless people fighting?
	if (!any) {
		// stop mob
		stop_fighting(mob);
		
		// stop everyone hitting mob
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(mob)), iter, next_in_room) {
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
	bool ok = TRUE;
	
	if (!IS_RIDING(ch)) {
		return;	// nm
	}
	else if (IS_MORPHED(ch)) {
		ok = FALSE;
	}
	else if ((IS_COMPLETE(IN_ROOM(ch)) || ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_CLOSED)) && !BLD_ALLOWS_MOUNTS(IN_ROOM(ch))) {
		ok = FALSE;
	}
	else if (GET_SITTING_ON(ch)) {
		ok = FALSE;
	}
	else if (MOUNT_FLAGGED(ch, MOUNT_FLYING) && !CAN_RIDE_FLYING_MOUNT(ch)) {
		ok = FALSE;
	}
	else if (DEEP_WATER_SECT(IN_ROOM(ch)) && !MOUNT_FLAGGED(ch, MOUNT_AQUATIC | MOUNT_WATERWALK) && !EFFECTIVELY_FLYING(ch)) {
		ok = FALSE;
	}
	else if (!CAN_RIDE_WATERWALK_MOUNT(ch) && DEEP_WATER_SECT(IN_ROOM(ch)) && MOUNT_FLAGGED(ch, MOUNT_WATERWALK) && !EFFECTIVELY_FLYING(ch)) {
		// has a waterwalk mount, in deep water, but is missing the riding upgrade
		ok = FALSE;
	}
	else if (!has_player_tech(ch, PTECH_RIDING_UPGRADE) && WATER_SECT(IN_ROOM(ch)) && !MOUNT_FLAGGED(ch, MOUNT_AQUATIC) && !EFFECTIVELY_FLYING(ch)) {
		ok = FALSE;
	}
	
	if (!ok) {
		do_dismount(ch, "", 0, 0);
	}
}


/**
* Interaction func for "decays-to" and "consumes-to".
*/
INTERACTION_FUNC(consumes_or_decays_interact) {
	obj_data *new_obj;
	bool fail = FALSE;
	int iter;
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		new_obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(new_obj, GET_OBJ_CURRENT_SCALE_LEVEL(inter_item));
		
		// ownership
		new_obj->last_owner_id = inter_item->last_owner_id;
		new_obj->last_empire_id = inter_item->last_empire_id;
		
		if (OBJ_FLAGGED(new_obj, OBJ_BIND_FLAGS)) {
			free_obj_binding(&OBJ_BOUND_TO(new_obj));	// unbind first
			OBJ_BOUND_TO(new_obj) = copy_obj_bindings(OBJ_BOUND_TO(inter_item));
		}
		
		// put it somewhere
		if (!CAN_WEAR(new_obj, ITEM_WEAR_TAKE) && inter_room) {
			obj_to_room(new_obj, inter_room);
		}
		else if (inter_item->carried_by) {
			obj_to_char(new_obj, inter_item->carried_by);
		}
		else if (inter_item->worn_by) {
			obj_to_char(new_obj, inter_item->worn_by);
		}
		else if (inter_item->in_obj) {
			obj_to_obj(new_obj, inter_item->in_obj);
		}
		else if (inter_item->in_vehicle) {
			obj_to_vehicle(new_obj, inter_item->in_vehicle);
		}
		else if (inter_room) {
			obj_to_room(new_obj, inter_room);
		}
		else {	// nowhere to put it
			fail = TRUE;
			extract_obj(new_obj);
			break;
		}
		
		if (!fail) {
			if (load_otrigger(new_obj) && new_obj->carried_by) {
				get_otrigger(new_obj, new_obj->carried_by, FALSE);
			}
		}
	}
	
	return TRUE;
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
	
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(victim)), iter, next_in_room) {
		if (iter != victim && affected_by_spell(iter, atype)) {
			++count;
			affect_from_char(iter, atype, TRUE);	// sends message
			act("$n recovers.", TRUE, iter, NULL, NULL, TO_ROOM);
		}
	}
	
	return count;
}


/**
* This runs a point update (every hour tick) on a character. It runs on ONLY
* players, as of b5.152, right after their "real update" runs.
*
* @param char_data *ch The player to update.
* @return bool TRUE if the player survives the update, FALSE if not.
*/
bool point_update_player(char_data *ch) {
	obj_data *obj, *next_obj;
	bool found;
	
	// never for mobs
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	// remove stale offers -- this needs to happen even if dead (resurrect)
	clean_offers(ch);
	
	// everything beyond here only matters if still alive
	if (IS_DEAD(ch)) {
		return FALSE;
	}
	
	// check bad quest items
	remove_quest_items(ch);
	
	// check way over-inventory (2x overburdened)
	if (!IS_IMMORTAL(ch) && IS_CARRYING_N(ch) > 2 * GET_LARGEST_INVENTORY(ch)) {
		found = FALSE;
		DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
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
	
	// light-based gains
	if (IS_OUTDOORS(ch) && get_sun_status(IN_ROOM(ch)) == SUN_LIGHT) {
		gain_player_tech_exp(ch, PTECH_VAMPIRE_SUN_IMMUNITY, 2);
	}
	
	if (GET_MOUNT_LIST(ch)) {
		gain_ability_exp(ch, ABIL_STABLEMASTER, 2);
	}
	
	run_ability_gain_hooks(ch, NULL, AGH_PASSIVE_HOURLY);
	
	// death count decrease after 3 minutes without a death
	if (GET_RECENT_DEATH_COUNT(ch) > 0 && GET_LAST_DEATH_TIME(ch) + (3 * SECS_PER_REAL_MIN) < time(0)) {
		GET_RECENT_DEATH_COUNT(ch) -= 1;
	}
	
	// bloody upkeep
	if (IS_VAMPIRE(ch) && !IS_IMMORTAL(ch)) {
		if (GET_BLOOD_UPKEEP(ch) > 0) {
			set_blood(ch, GET_BLOOD(ch) - GET_BLOOD_UPKEEP(ch));
		}
		
		if (GET_BLOOD(ch) < 0) {
			out_of_blood(ch);
			return FALSE;
		}
	}
	else {	// not a vampire (or is imm)
		// don't gain blood whilst being fed upon
		if (GET_FED_ON_BY(ch) == NULL) {
			set_blood(ch, GET_BLOOD(ch) + 1);
		}
	}
	
	// warn about burning
	if (IS_BURNING(IN_ROOM(ch))) {
		if (ROOM_IS_CLOSED(IN_ROOM(ch))) {
			act("The walls crackle and crisp as they burn!", FALSE, ch, NULL, NULL, TO_CHAR);
		}
		else {
			act("The fire rages as the building burns!", FALSE, ch, NULL, NULL, TO_CHAR);
		}
	}
	
	return TRUE;	// we lived!
}


/**
* Runs a real update (generally 5 seconds) on ch. This is ONLY for players as
* of b5.152 -- NPCs schedule all their updates instead.
*
* @param char_data *ch The player to update.
*/
void real_update_player(char_data *ch) {
	char_data *room_ch, *next_ch;
	struct companion_data *compan;
	char buf[MAX_STRING_LENGTH];
	struct instance_data *inst;
	int fol_count, gain, iter;
	ability_data *abil;
	bool found, msg;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	// put stuff that happens when dead here
	
	// DEAD players: check for auto-respawn
	if (IS_DEAD(ch) && ch->desc && get_cooldown_time(ch, COOLDOWN_DEATH_RESPAWN) == 0) {
		do_respawn(ch, "", 0, 0);
		return;
	}
	
	// idle players: check very early
	if (!check_idling(ch)) {
		return;
	}
	
	// everything beyond here only matters if still alive
	if (IS_DEAD(ch)) {
		return;
	}
	
	// check for end of meters (in case it was missed in the fight code)
	if (!FIGHTING(ch)) {
		check_combat_end(ch);
	}
	
	// first check location: this may move the player
	if (PLR_FLAGGED(ch, PLR_ADVENTURE_SUMMONED) && (!(inst = find_instance_by_room(IN_ROOM(ch), FALSE, FALSE)) || INST_ID(inst) != GET_ADVENTURE_SUMMON_INSTANCE_ID(ch))) {
		adventure_unsummon(ch);
	}
	
	// ensure I don't have an invalid companion in the solo role
	if (GET_COMPANION(ch) && GET_CLASS_ROLE(ch) == ROLE_SOLO && (compan = has_companion(ch, GET_MOB_VNUM(GET_COMPANION(ch)))) && compan->from_abil != NO_ABIL && (abil = find_ability_by_vnum(compan->from_abil)) && ABIL_IS_SYNERGY(abil) && !check_solo_role(ch)) {
		act("$N vanishes because you're in the solo role but not alone.", FALSE, ch, NULL, GET_COMPANION(ch), TO_CHAR);
		act("$N vanishes.", FALSE, ch, NULL, GET_COMPANION(ch), TO_NOTVICT);
		extract_char(GET_COMPANION(ch));
	}
	// earthmeld damage
	if (!IS_IMMORTAL(ch) && AFF_FLAGGED(ch, AFF_EARTHMELD) && IS_COMPLETE(IN_ROOM(ch)) && (ROOM_IS_CLOSED(IN_ROOM(ch)) || ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_BARRIER))) {
		if (!affected_by_spell(ch, ATYPE_NATURE_BURN)) {
			msg_to_char(ch, "You are beneath a building and begin taking nature burn as the earth you're buried in is separated from fresh air...\r\n");
		}
		apply_dot_effect(ch, ATYPE_NATURE_BURN, 30, DAM_MAGICAL, 5, 60, ch);
	}
	
	// players only
	if (IS_MORPHED(ch)) {
		check_morph_ability(ch);
	}
	if (IS_RIDING(ch)) {
		check_should_dismount(ch);
	}
	if (CAN_CARRY_N(ch) > max_inventory_size) {
		// record maximum global inventory size, for script safety
		max_inventory_size = CAN_CARRY_N(ch);
	}
	
	// update recent level data if level has gone up or it's been too long since we've seen a higher level
	if (!IS_IMMORTAL(ch) && GET_COMPUTED_LEVEL(ch) > GET_HIGHEST_KNOWN_LEVEL(ch)) {
		if ((int) (GET_COMPUTED_LEVEL(ch) / 100) > (int) (GET_HIGHEST_KNOWN_LEVEL(ch) / 100)) {
			// first time over a new 100?
			log_to_slash_channel_by_name(PLAYER_LOG_CHANNEL, ch, "%s has reached level %d!", PERS(ch, ch, TRUE), (int)(GET_COMPUTED_LEVEL(ch) / 100) * 100);
		}
	
		GET_HIGHEST_KNOWN_LEVEL(ch) = GET_COMPUTED_LEVEL(ch);
	}
	// update the last-known-level
	GET_LAST_KNOWN_LEVEL(ch) = GET_COMPUTED_LEVEL(ch);

	// very drunk? more confused!
	if (IS_DRUNK(ch)) {
		GET_CONFUSED_DIR(ch) = number(0, NUM_SIMPLE_DIRS-1);
	}

	if (GET_EQ(ch, WEAR_SADDLE) && !IS_RIDING(ch)) {
		perform_remove(ch, WEAR_SADDLE);
	}

	if (GET_BLOOD(ch) > GET_MAX_BLOOD(ch)) {
		set_blood(ch, GET_MAX_BLOOD(ch));
	}

	// periodic exp and skill gain
	check_daily_cycle_reset(ch);

	/* Update conditions */
	if (HAS_BONUS_TRAIT(ch, BONUS_NO_HUNGER) || has_player_tech(ch, PTECH_NO_HUNGER)) {			
		gain_condition(ch, FULL, -1);
	}
	else {
		if (!number(0, 1)) {
			gain_condition(ch, FULL, 1);
		}
	}

	run_ability_gain_hooks(ch, NULL, AGH_PASSIVE_FREQUENT);

	// more thirsty?
	if (HAS_BONUS_TRAIT(ch, BONUS_NO_THIRST) || has_player_tech(ch, PTECH_NO_THIRST)) {
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
		set_health(ch, MIN(0, GET_HEALTH(ch)) - 1);	// fixing? a bug where a player whose health is positve but is in a bleeding out position, would not bleed out right away (but couldn't recover)
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
	if (!ch->over_time_effects) {
		// only heals if they have no DOTs
		gain = health_gain(ch, FALSE);
		heal(ch, ch, gain);
		GET_HEALTH_DEFICIT(ch) = MAX(0, GET_HEALTH_DEFICIT(ch) - gain);
	}

	// check move gain
	if (!IS_IMMORTAL(ch) && IS_SWIMMING(ch)) {
		// swimming: costs moves
		if (GET_MOVE(ch) > 0) {
			set_move(ch, GET_MOVE(ch) - 1);
		}
		if (GET_MOVE(ch) <= 0) {
			msg_to_char(ch, "You sink beneath the water and die!\r\n");
			act("$n sinks beneath the water and dies!", FALSE, ch, NULL, NULL, TO_ROOM);
			death_log(ch, ch, TYPE_SUFFERING);
			die(ch, ch);
			return;
		}
	}
	else {	// normal move gain
		gain = move_gain(ch, FALSE);
		set_move(ch, GET_MOVE(ch) + gain);
		GET_MOVE_DEFICIT(ch) = MAX(0, GET_MOVE_DEFICIT(ch) - gain);
	}

	// mana gain
	gain = mana_gain(ch, FALSE);
	set_mana(ch, GET_MANA(ch) + gain);
	GET_MANA_DEFICIT(ch) = MAX(0, GET_MANA_DEFICIT(ch) - gain);

	if (IS_VAMPIRE(ch)) {
		update_vampire_sun(ch);
	}

	if (!AWAKE(ch) && IS_MORPHED(ch) && CHAR_MORPH_FLAGGED(ch, MORPHF_NO_SLEEP)) {
		sprintf(buf, "%s has become $n!", PERS(ch, ch, 0));
		msg = !CHAR_MORPH_FLAGGED(ch, MORPHF_NO_MORPH_MESSAGE);

		perform_morph(ch, NULL);
	
		if (msg) {
			act(buf, TRUE, ch, 0, 0, TO_ROOM);
		}
		msg_to_char(ch, "You revert to normal!\r\n");
	}
	
	// temperature check
	update_player_temperature(ch);

	/* Blood check */
	if (GET_BLOOD(ch) <= 0 && !GET_FED_ON_BY(ch) && !GET_FEEDING_FROM(ch)) {
		out_of_blood(ch);
		return;
	}
	else if (IS_BLOOD_STARVED(ch)) {
		cancel_blood_upkeeps(ch);
		starving_vampire_aggro(ch);
	}

	// too-many-followers check
	fol_count = 0;
	DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), room_ch, next_ch, next_in_room) {
		// check is npc following ch
		if (room_ch == ch || room_ch->desc || !IS_NPC(room_ch) || GET_LEADER(room_ch) != ch) {
			continue;
		}
	
		// don't care about companions
		if (GET_COMPANION(room_ch)) {
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
	
	// DO THESE LAST:
	
	// call point-update if it's our turn
	if ((GET_IDNUM(ch) % REAL_UPDATES_PER_MUD_HOUR) == point_update_cycle) {
		if (!point_update_player(ch)) {
			return;
		}
	}
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
	room_data *to_room, *center = city->location;
	int radius = city_type[city->type].radius;
	bool found_building = FALSE;
	vehicle_data *veh;
	int x, y;
	
	// skip recently-founded cities
	if (get_room_extra_data(city->location, ROOM_EXTRA_FOUND_TIME) + (12 * SECS_PER_REAL_HOUR) > time(0)) {
		return FALSE;
	}
	
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
					if (IS_ANY_BUILDING(to_room) && !ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_DISREPAIR) && !ROOM_BLD_FLAGGED(to_room, BLD_IS_RUINS)) {
						found_building = TRUE;
					}
					else {	// check for building-vehicles
						DL_FOREACH2(ROOM_VEHICLES(to_room), veh, next_in_room) {
							if (VEH_OWNER(veh) == emp && VEH_FLAGGED(veh, VEH_BUILDING) && !VEH_FLAGGED(veh, VEH_IS_RUINS | VEH_EXTRACTED)) {
								found_building = TRUE;
								break;
							}
						}
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
	bool any = FALSE;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (!EMPIRE_IMM_ONLY(emp)) {
			for (city = EMPIRE_CITY_LIST(emp); city; city = next_city) {
				next_city = city->next;
				any |= check_one_city_for_ruin(emp, city);
			}
		}
	}
	
	if (any) {
		read_empire_territory(NULL, FALSE);
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
					
					syslog(SYS_EMPIRE, 0, TRUE, "DIPL: The war between %s and %s has timed out", EMPIRE_NAME(emp), EMPIRE_NAME(enemy));
					log_to_empire(emp, ELOG_DIPLOMACY, "The war with %s is over", EMPIRE_NAME(enemy));
					log_to_empire(enemy, ELOG_DIPLOMACY, "The war with %s is over", EMPIRE_NAME(emp));
				}
			}
			// check for a theivery permit that's over a day
			if (IS_SET(pol->type, DIPL_THIEVERY) && pol->start_time < (time(0) - SECS_PER_REAL_DAY)) {
				enemy = real_empire(pol->id);
				
				// remove war, set distrust
				REMOVE_BIT(pol->type, DIPL_THIEVERY);
				pol->start_time = time(0);
								
				syslog(SYS_EMPIRE, 0, TRUE, "DIPL: %s's thievery permit against %s has timed out", EMPIRE_NAME(emp), EMPIRE_NAME(enemy));
				log_to_empire(emp, ELOG_DIPLOMACY, "The thievery permit against %s has timed out", EMPIRE_NAME(enemy));
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
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
}


/**
* This runs periodically to ensure that empires don't have WAY too many cities.
* It triggers on empires that meet these two criteria:
* - Have at least 2 cities
* - Are used at least 2x their currently-earned city points.
*/
void reduce_city_overages(void) {
	empire_data *iter, *next_iter;
	int points;
	
	time_t overage_timeout = time(0) - (config_get_int("city_overage_timeout") * SECS_PER_REAL_HOUR);
	
	HASH_ITER(hh, empire_table, iter, next_iter) {
		// only bother on !imm empires that have MORE than one city (they can always keep the last one)
		if (!EMPIRE_IMM_ONLY(iter) && count_cities(iter) > 1) {
			points = city_points_available(iter);
			
			if (points >= 0) {	// no overage
				// remove warning time, if any
				EMPIRE_CITY_OVERAGE_WARNING_TIME(iter) = 0;
			}
			else {	// over on points
				
				if (EMPIRE_CITY_OVERAGE_WARNING_TIME(iter) == 0) {
					EMPIRE_CITY_OVERAGE_WARNING_TIME(iter) = time(0);
					log_to_empire(iter, ELOG_TERRITORY, "Your empire is using more city points than it has available and will abandon one soon");
				}
				else if (EMPIRE_CITY_OVERAGE_WARNING_TIME(iter) <= overage_timeout) {
					// time's up
					reduce_city_overage_one(iter);
				}
				else {
					// still over -- log again
					log_to_empire(iter, ELOG_TERRITORY, "Your empire is using more city points than it has available and will abandon one soon");
				}
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
	struct empire_city_data *city;
	room_data *iter, *next_iter, *loc, *farthest;
	int dist, this_far, far_dist, far_type, ter_type;
	bool junk, outskirts_over, frontier_over, total_over, was_large;
	
	// sanity
	if (!emp || EMPIRE_IMM_ONLY(emp)) {
		return;
	}
	
	// see which is over
	outskirts_over = EMPIRE_TERRITORY(emp, TER_OUTSKIRTS) > OUTSKIRTS_CLAIMS_AVAILABLE(emp);
	frontier_over = EMPIRE_TERRITORY(emp, TER_FRONTIER) > land_can_claim(emp, TER_FRONTIER);
	total_over = EMPIRE_TERRITORY(emp, TER_TOTAL) > land_can_claim(emp, TER_TOTAL);
	if (!outskirts_over && !frontier_over && !total_over) {
		return;	// no work
	}
	
	farthest = NULL;
	far_dist = -1;	// always less
	far_type = TER_FRONTIER;
	
	// check the whole map to determine the farthest outside claim
	HASH_ITER(hh, world_table, iter, next_iter) {
		// map only
		if (GET_ROOM_VNUM(iter) >= MAP_SIZE) {
			continue;
		}
		loc = HOME_ROOM(iter);
		
		// if owner matches AND it's not in a city
		if (ROOM_OWNER(loc) == emp && ((ter_type = get_territory_type_for_empire(loc, emp, FALSE, &junk, &was_large)) != TER_CITY || was_large)) {
			if (ter_type == TER_CITY && !was_large) {
				continue;	// NEVER do a city, even if total is over (except large-radius portions)
			}
			if (ter_type == TER_FRONTIER && !frontier_over && !total_over) {
				continue;
			}
			else if ((ter_type == TER_OUTSKIRTS || was_large) && !outskirts_over && !total_over) {
				continue;
			}
			
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
				far_type = ter_type;
			}
		}
	}
	
	if (farthest) {
		switch (far_type) {
			case TER_OUTSKIRTS: {
				log_to_empire(emp, ELOG_TERRITORY, "Abandoning %s (%d, %d) because too much outskirts territory has been claimed (%d/%d)", get_room_name(farthest, FALSE), X_COORD(farthest), Y_COORD(farthest), EMPIRE_TERRITORY(emp, TER_OUTSKIRTS), OUTSKIRTS_CLAIMS_AVAILABLE(emp));
				break;
			}
			case TER_FRONTIER: {
				log_to_empire(emp, ELOG_TERRITORY, "Abandoning %s (%d, %d) because too much frontier territory has been claimed (%d/%d)", get_room_name(farthest, FALSE), X_COORD(farthest), Y_COORD(farthest), EMPIRE_TERRITORY(emp, TER_FRONTIER), land_can_claim(emp, TER_FRONTIER));
				break;
			}
			default: {
				log_to_empire(emp, ELOG_TERRITORY, "Abandoning %s (%d, %d) because too much territory has been claimed (%d/%d)", get_room_name(farthest, FALSE), X_COORD(farthest), Y_COORD(farthest), EMPIRE_TERRITORY(emp, TER_TOTAL), land_can_claim(emp, TER_TOTAL));
				break;
			}
		}
		
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
		if (EMPIRE_IMM_ONLY(iter)) {
			continue;	// ignore imms
		}
		
		if (EMPIRE_TERRITORY(iter, TER_OUTSKIRTS) > OUTSKIRTS_CLAIMS_AVAILABLE(iter) || EMPIRE_TERRITORY(iter, TER_FRONTIER) > land_can_claim(iter, TER_FRONTIER)) {
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
	bool outside_only = (EMPIRE_TERRITORY(emp, TER_OUTSKIRTS) > 0 || EMPIRE_TERRITORY(emp, TER_FRONTIER) > 0);
	room_data *iter, *next_iter, *found_room = NULL;
	bool junk;
	
	// try interior first -- we'll take the first secondary room we find
	if (!outside_only) {
		DL_FOREACH_SAFE2(interior_room_list, iter, next_iter, next_interior) {
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
	
	// syslog(SYS_INFO, 0, TRUE, "Debug: Starting reduce_stale_empires");
	
	HASH_ITER(hh, empire_table, iter, next_iter) {
		// check if we need to rescan
		if (EMPIRE_MEMBERS(iter) > 0 && EMPIRE_NEXT_TIMEOUT(iter) != 0 && EMPIRE_NEXT_TIMEOUT(iter) <= time(0)) {
			// syslog(SYS_INFO, 0, TRUE, "Debug: Rescanning empire %s based on timeout", EMPIRE_NAME(iter));
			reread_empire_tech(iter);
		}
		
		// check overages
		if (!EMPIRE_IMM_ONLY(iter) && EMPIRE_MEMBERS(iter) == 0 && EMPIRE_TERRITORY(iter, TER_TOTAL) > 0) {
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


/**
* Processes needs for a given empire/island/need. This generally involves
* taking resources, and will set the UNSUPPLIED flag if there aren't enough.
*
* @param empire_data *emp Which empire.
* @param struct empire_island *eisle The island being processed for needs.
* @param struct empire_needs *needs The current 'needs' to process.
*/
void update_empire_needs(empire_data *emp, struct empire_island *eisle, struct empire_needs *needs) {
	struct empire_storage_data *store, *next_store;
	struct island_info *island = get_island(eisle->island, TRUE);
	bool any = TRUE, vault = FALSE;
	int amount, max, can_take;
	obj_data *obj;
	
	while (needs->needed > 0 && any) {
		any = FALSE;	// ensure we charged any item this cycle
		HASH_ITER(hh, eisle->store, store, next_store) {
			if (needs->needed < 1) {
				break;	// done early
			}
			if (store->keep == UNLIMITED || store->amount <= store->keep || store->amount < 1 || !(obj = store->proto)) {
				continue;
			}
			
			can_take = store->amount - store->keep;	// already checked for keep=UNLIMITED
			amount = 0;
			
			// ENEED_x: processing the item
			switch (needs->type) {
				case ENEED_WORKFORCE: {
					if (IS_FOOD(obj)) {
						if (GET_FOOD_HOURS_OF_FULLNESS(obj) > needs->needed) {
							amount = 1;
							needs->needed = 0;
						}
						else {	// need more than 1
							amount = ceil((double) needs->needed / GET_FOOD_HOURS_OF_FULLNESS(obj));
							max = MAX(100, can_take / 4);
							amount = MIN(amount, can_take);
							amount = MIN(amount, max);	// don't take more than this of any 1 thing per cycle
							SAFE_ADD(needs->needed, -(amount * GET_FOOD_HOURS_OF_FULLNESS(obj)), 0, INT_MAX, FALSE);
						}
					}
					break;
				}
				default: {
					// type not implemented
					
					// Note for implementation:
					// if (IS_WEALTH_ITEM(obj)) { vault = TRUE; }
					break;
				}
			}
			
			// amount we could take
			if (amount > 0) {
				any = TRUE;
				add_to_empire_storage(emp, eisle->island, store->vnum, -amount);
				log_to_empire(emp, ELOG_WORKFORCE, "Consumed %s (x%d) on %s", GET_OBJ_SHORT_DESC(obj), amount, eisle->name ? eisle->name : island->name);
			}
		}
	}
	
	if (needs->needed > 0) {
		if (!IS_SET(needs->status, ENEED_STATUS_UNSUPPLIED)) {
			// ENEED_x: logging the problem
			switch (needs->type) {
				case ENEED_WORKFORCE: {
					// this logs to TRADE because otherwise members won't see it
					log_to_empire(emp, ELOG_TRADE, "Your workforce on %s is starving!", eisle->name ? eisle->name : island->name);
					deactivate_workforce_island(emp, eisle->island);
					break;
				}
			}
			
			SET_BIT(needs->status, ENEED_STATUS_UNSUPPLIED);
		}
	}
	else {
		REMOVE_BIT(needs->status, ENEED_STATUS_UNSUPPLIED);
	}
	
	if (vault) {
		read_vault(emp);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT LIMITS ///////////////////////////////////////////////////////////

/**
* Checks and runs an autostore for an object. This function does NOT check the
* basic autostore timer, but it WILL check the timer if it's in a room with
* long-autostore (unless force=TRUE).
*
* @param obj_data *obj The item to check/store.
* @param bool force If TRUE, ignores timers and players present and stores all storables.
* @param empire_data *override_emp Optional: If not NULL, will store to this empire.
* @return bool TRUE if the item is still in the world, FALSE if it was extracted (or if its autostore event should be canceled)
*/
bool check_autostore(obj_data *obj, bool force, empire_data *override_emp) {
	player_index_data *index;
	empire_data *emp = override_emp;
	char_data *loaded_ch;
	vehicle_data *in_veh;
	room_data *real_loc, *last_loc;
	obj_data *top_obj;
	bool store, unique, full, is_home, file;
	int islid, home_idnum = NOBODY;
	
	// easy exclusions
	top_obj = get_top_object(obj);
	if (top_obj->carried_by || top_obj->worn_by) {
		return FALSE;	// on a person
	}
	if (!CAN_WEAR(obj, ITEM_WEAR_TAKE) && !OBJ_CAN_STORE(obj)) {
		return FALSE;	// no-take AND no-store
	}
	if (!IN_ROOM(obj) && !obj->in_vehicle && !obj->in_obj) {
		// we aren't somewhere that autostore would even work on us -- cancel it
		return FALSE;
	}
	if (obj->in_vehicle && !force) {
		// contents of a vehicle only auto-store on a force: cancel if automatic
		return FALSE;
	}
	
	// ensure object is in a room, or in an object in a room
	real_loc = IN_ROOM(top_obj);
	in_veh = top_obj->in_vehicle;
	if (in_veh) {
		real_loc = IN_ROOM(in_veh);
	}
	
	// follow up the chain of rooms until SOMETHING is claimed:
	do {
		last_loc = real_loc;
		
		// backup: if in a room which is inside a vehicle, go up the chain
		if (real_loc && !ROOM_OWNER(real_loc) && GET_ROOM_VEHICLE(real_loc) && IN_ROOM(GET_ROOM_VEHICLE(real_loc))) {
			real_loc = IN_ROOM(GET_ROOM_VEHICLE(real_loc));
		}
	} while (last_loc != real_loc);
	
	// detect home?
	if (in_veh && VEH_INTERIOR_HOME_ROOM(in_veh) && (home_idnum = ROOM_PRIVATE_OWNER(VEH_INTERIOR_HOME_ROOM(in_veh))) != NOBODY) {
		is_home = TRUE;
	}
	else if (real_loc && (home_idnum = ROOM_PRIVATE_OWNER(HOME_ROOM(real_loc))) != NOBODY) {
		is_home = TRUE;
	}
	else {
		is_home = FALSE;
	}
	
	// detect owner here
	if (!emp && in_veh) {
		emp = VEH_OWNER(in_veh) ? VEH_OWNER(in_veh) : ROOM_OWNER(IN_ROOM(in_veh));
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
	else if (is_home && UNIQUE_OBJ_CAN_STORE(obj, TRUE) && bind_ok_idnum(obj, home_idnum)) {
		// trigger home storage
		store = TRUE;
		unique = TRUE;
	}
	else if (!is_home && UNIQUE_OBJ_CAN_STORE(obj, FALSE)) {
		// store unique items outside a private home
		store = TRUE;
		unique = TRUE;
	}
	else if (OBJ_BOUND_TO(obj) && !is_home && (GET_AUTOSTORE_TIMER(obj) + config_get_int("bound_item_junk_time") * SECS_PER_REAL_MIN) < time(0)) {
		// room owned, item is bound, not a private home, but not storable? junk it
		store = TRUE;
		// DON'T mark unique -- we are just junking it
	}
	else if (is_home) {
		// all other items in homes are junk
		store = TRUE;
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
		else if (unique && is_home) {
			// home storage
			if ((index = find_player_index_by_idnum(home_idnum)) && (loaded_ch = find_or_load_player(index->name, &file))) {
				check_delayed_load(loaded_ch);
				store_unique_item(NULL, &GET_HOME_STORAGE(loaded_ch), obj, NULL, NULL, &full);
				queue_delayed_update(loaded_ch, CDU_SAVE);
				return FALSE;
				// note: leaving loaded_ch open, to be auto-saved in a second, as this may run on many items
			}
			// failed to load owner: fall through to extract
		}
		else if (unique) {
			// this extracts it itself
			store_unique_item(NULL, &EMPIRE_UNIQUE_STORAGE(emp), obj, emp, real_loc, &full);
			return FALSE;
		}
		else if (OBJ_CAN_STORE(obj)) {
			// find where to store it, especially if we got this far with emp but no real_loc
			islid = real_loc ? GET_ISLAND_ID(real_loc) : NO_ISLAND;
			if (islid == NO_ISLAND) {
				islid = get_main_island(emp);
			}
			
			if (islid != NO_ISLAND) {	// MUST be not-nowhere to autostore
				add_to_empire_storage(emp, islid, GET_OBJ_VNUM(obj), 1);
			}
		}
	}
	
	// get rid of it either way
	extract_obj(obj);
	return FALSE;
}


/**
* Determines when the timer on an object timer's DG event should be set. A
* result of 0 means "no need for this".
*
* @param obj_data *obj The object.
* @return int Number of seconds to schedule for, or 0 if it doesn't need it.
*/
static int compute_obj_event_timer(obj_data *obj) {
	int seconds = 0;
	
	if (GET_OBJ_TIMER(obj) > 0 || (LIGHT_IS_LIT(obj) && GET_LIGHT_HOURS_REMAINING(obj) > 0)) {
		// timer or lit light
		seconds = SECS_PER_MUD_HOUR;
	}
	else if (IN_ROOM(obj) && CAN_WEAR(obj, ITEM_WEAR_TAKE) && ROOM_SECT_FLAGGED(IN_ROOM(obj), SECTF_FRESH_WATER | SECTF_OCEAN)) {
		// floating obj
		seconds = SECS_PER_MUD_HOUR;
	}
	
	return seconds;	// may still be 0
}


EVENTFUNC(obj_autostore_event) {
	struct obj_event_data *data = (struct obj_event_data*)event_obj;
	obj_data *obj = data->obj;
	long when;
	
	when = GET_AUTOSTORE_TIMER(obj) + (config_get_int("autostore_time") * SECS_PER_REAL_MIN) - time(0);
	
	// double-check that it isn't in the future
	if (when > 0) {
		// ... and just re-enqueue it for the future
		return when RL_SEC;
	}
	
	// ok, always delete before doing anything else (storing the object would try to cancel this event)
	delete_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_AUTOSTORE);
	
	if (!check_autostore(obj, FALSE, NULL)) {
		// obj was autostored/purged, or other reasons not to reschedule
		free(data);
		return 0;	// don't re-enqueue
	}
	else {
		// autostore prevented... check again soon
		add_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_AUTOSTORE, the_event);
		return 60 RL_SEC;	// re-enqueue for 1 minute from now and try again
	}
}


EVENTFUNC(obj_hour_event) {
	struct obj_event_data *data = (struct obj_event_data*)event_obj;
	obj_data *obj = data->obj;
	char_data *ch_iter, *pyro;
	empire_data *emp, *enemy;
	struct empire_political_data *pol;
	room_data *home, *to_room;
	vehicle_data *veh;
	int new_timer;
	time_t timer;
	
	// always delete first
	delete_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_TIMER);
	
	// ensure the object is in-game SOMEWHERE
	if (!obj_room(obj)) {
		free(data);
		return 0;	// do not re-enqueue
	}
	
	// things we do here:
	
	// check for burning-out message
	if (LIGHT_IS_LIT(obj)) {
		if (GET_LIGHT_HOURS_REMAINING(obj) == 2 || GET_OBJ_TIMER(obj) == 2) {
			if (obj->worn_by) {
				act("Your light begins to flicker and fade.", FALSE, obj->worn_by, obj, NULL, TO_CHAR);
				act("$n's light begins to flicker and fade.", TRUE, obj->worn_by, obj, NULL, TO_ROOM);
			}
			else if (obj->carried_by) {
				act("$p begins to flicker and fade.", FALSE, obj->carried_by, obj, NULL, TO_CHAR);
			}
			else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
				act("$p begins to flicker and fade.", FALSE, ROOM_PEOPLE(IN_ROOM(obj)), obj, NULL, TO_CHAR | TO_ROOM);
			}
		}
		else if (GET_OBJ_TIMER(obj) == 1) {
			// this section does NOT message for GET_LIGHT_HOURS_REMAINING(obj) == 1 (see: use_hour_of_light)
			if (obj->worn_by) {
				act("Your light burns out.", FALSE, obj->worn_by, obj, NULL, TO_CHAR);
				act("$n's light burns out.", TRUE, obj->worn_by, obj, NULL, TO_ROOM);
			}
			else if (obj->carried_by) {
				act("$p burns out.", FALSE, obj->carried_by, obj, NULL, TO_CHAR);
			}
			else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
				act("$p burns out.", FALSE, ROOM_PEOPLE(IN_ROOM(obj)), obj, NULL, TO_CHAR | TO_ROOM);
			}
		}
	}
	
	// check light burnout
	if (IS_LIGHT(obj) && !use_hour_of_light(obj, TRUE)) {
		// extracted
		free(data);
		return 0;	// don't re-enqueue
	}
	
	// float or sink
	if (IN_ROOM(obj) && CAN_WEAR(obj, ITEM_WEAR_TAKE) && ROOM_SECT_FLAGGED(IN_ROOM(obj), SECTF_FRESH_WATER | SECTF_OCEAN)) {
		if (materials[GET_OBJ_MATERIAL(obj)].floats && (to_room = real_shift(IN_ROOM(obj), shift_dir[WEST][0], shift_dir[WEST][1]))) {
			if (!number(0, 2) && !ROOM_SECT_FLAGGED(to_room, SECTF_ROUGH) && !ROOM_IS_CLOSED(to_room)) {
				// float-west message
				DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(obj)), ch_iter, next_in_room) {
					if (ch_iter->desc) {
						sprintf(buf, "$p floats %s.", dirs[get_direction_for_char(ch_iter, WEST)]);
						act(buf, TRUE, ch_iter, obj, NULL, TO_CHAR);
					}
				}
				
				// move object but keep autostore time
				timer = GET_AUTOSTORE_TIMER(obj);
				suspend_autostore_updates = TRUE;
				obj_to_room(obj, to_room);
				suspend_autostore_updates = FALSE;
				schedule_obj_autostore_check(obj, timer);
				
				// floats-in message
				DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(obj)), ch_iter, next_in_room) {
					if (ch_iter->desc) {
						sprintf(buf, "$p floats in from %s.", from_dir[get_direction_for_char(ch_iter, WEST)]);
						act(buf, TRUE, ch_iter, obj, NULL, TO_CHAR);
					}
				}
			}
		}
		else { // does not float, or could not find a new room to float it to
			if (ROOM_PEOPLE(IN_ROOM(obj))) {
				act("$p sinks quickly to the bottom.", TRUE, ROOM_PEOPLE(IN_ROOM(obj)), obj, 0, TO_CHAR | TO_ROOM);
			}
			free(data);
			extract_obj(obj);
			return 0;	// don't re-enqueue
		}
	}
	
	// check main obj timer
	if (!tick_obj_timer(obj)) {
		free(data);
		return 0;	// don't re-enqueue
	}
	
	// burn the room? ONLY if we got this far
	if (LIGHT_IS_LIT(obj) && LIGHT_FLAGGED(obj, LIGHT_FLAG_LIGHT_FIRE) && IN_ROOM(obj) && IS_ANY_BUILDING(IN_ROOM(obj))) {
		if ((veh = GET_ROOM_VEHICLE(IN_ROOM(obj))) && VEH_FLAGGED(veh, VEH_BURNABLE) && !VEH_FLAGGED(veh, VEH_ON_FIRE)) {
			// burnable vehicle
			/// ensure it SHOULD catch fire
			if (obj->last_empire_id != NOTHING || !VEH_OWNER(veh)) {
				// ensure empire is at war
				emp = VEH_OWNER(veh);
				enemy = real_empire(obj->last_empire_id);
				
				// check for war
				if (!emp || (enemy && (pol = find_relation(enemy, emp)) && IS_SET(pol->type, DIPL_WAR))) {
					if (ROOM_PEOPLE(IN_ROOM(obj))) {
						act("A stray ember from $p ignites $V!", FALSE, ROOM_PEOPLE(IN_ROOM(obj)), obj, veh, TO_CHAR | TO_ROOM);
					}
					start_vehicle_burning(veh);
					
					if (emp && obj->last_owner_id > 0 && (pyro = is_playing(obj->last_owner_id))) {
						add_offense(emp, OFFENSE_BURNED_VEHICLE, pyro, IN_ROOM(obj), offense_was_seen(pyro, emp, IN_ROOM(obj)) ? OFF_SEEN : NOBITS);
					}
				}
			}
		}
		else if (!veh && (home = HOME_ROOM(IN_ROOM(obj))) && ROOM_BLD_FLAGGED(home, BLD_BURNABLE) && !IS_BURNING(home)) {
			// burnable building
			// only items with an empire id are considered: you can't burn stuff down by accident (unless the building is unowned)
			if (obj->last_empire_id != NOTHING || !ROOM_OWNER(home)) {
				// check that the empire is at war
				emp = ROOM_OWNER(home);
				enemy = real_empire(obj->last_empire_id);
				
				// check for war
				if (!emp || (enemy && (pol = find_relation(enemy, emp)) && IS_SET(pol->type, DIPL_WAR))) {
					if (ROOM_PEOPLE(IN_ROOM(obj))) {
						act("A stray ember from $p ignites the room!", FALSE, ROOM_PEOPLE(IN_ROOM(obj)), obj, NULL, TO_CHAR | TO_ROOM);
					}
					start_burning(home);
					
					if (emp && obj->last_owner_id > 0 && (pyro = is_playing(obj->last_owner_id))) {
						add_offense(emp, OFFENSE_BURNED_BUILDING, pyro, IN_ROOM(obj), offense_was_seen(pyro, emp, IN_ROOM(obj)) ? OFF_SEEN : NOBITS);
					}
				}
			}
		}
	}
	
	// END: determine new timer
	if ((new_timer = compute_obj_event_timer(obj)) > 0) {
		// re-store, re-enqueue, and try again
		if (!find_stored_event(GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_TIMER)) {
			add_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_TIMER, the_event);
			return new_timer RL_SEC;
		}
		else {
			// already added a new one -- just flush this one
			free(data);
			return 0;	// don't re-enqueue
		}
	
	}
	else {	// no longer need this
		free(data);
		return 0;	// don't re-enqueue
	}
}


/**
* Schedules a check_autostore when an item is put into a room, vehicle, or
* other object.
*
* NOTE: The "autostore timer" on the object is the time it was dropped, NOT the
* time at which it will store.
*
* @param obj_data *obj The object.
* @param long new_autostore_timer Optional: Will also update the autostore timer to this timestamp and reschedule if necessary (pass 0 to not-change).
*/
void schedule_obj_autostore_check(obj_data *obj, long new_autostore_timer) {
	struct obj_event_data *data;
	struct dg_event *ev;
	long seconds;
	bool change = FALSE;
	
	if (!obj) {
		return;	// ???
	}
	
	// figure out correct time
	if (new_autostore_timer > 0 && new_autostore_timer != GET_AUTOSTORE_TIMER(obj)) {
		GET_AUTOSTORE_TIMER(obj) = new_autostore_timer;
		change = TRUE;
	}
	
	// already have an event?
	if (find_stored_event(GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_AUTOSTORE)) {
		if (change) {
			// ok: cancel and reschedule
			cancel_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_AUTOSTORE);
		}
		else {
			return;	// have one already (and we're not updating it)
		}
	}
	
	// find out when
	seconds = GET_AUTOSTORE_TIMER(obj) + (config_get_int("autostore_time") * SECS_PER_REAL_MIN) - time(0);
	if (seconds < 1) {
		// do it in the near-future if it's passed (usually due to a reboot)
		// random timing spreads them out when there's a bunch
		seconds = number(1, 20);
	}
	
	CREATE(data, struct obj_event_data, 1);
	data->obj = obj;
	
	ev = dg_event_create(obj_autostore_event, data, seconds RL_SEC);
	add_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_AUTOSTORE, ev);
}


/**
* Schedules any timer updates an item needs as DG events.
*
* NOTE: some of the when-to-schedule also appears in obj_hour_event
*
* @param obj_data *obj The object.
* @param bool override If TRUE, will delete any existing event.
*/
void schedule_obj_timer_update(obj_data *obj, bool override) {
	struct obj_event_data *data;
	struct dg_event *ev;
	int seconds;
	
	if (!obj) {
		return;	// ???
	}
	else if (!override && find_stored_event(GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_TIMER)) {
		return;	// have one already
	}
	
	// delete first?
	if (override) {
		cancel_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_TIMER);
	}
	
	// do we need to be scheduled?
	seconds = compute_obj_event_timer(obj);
	
	// we only schedule here, not cancel -- it will cancel itself
	if (seconds > 0) {
		if (add_chaos_to_obj_timers) {
			seconds += number(0, SECS_PER_MUD_HOUR-1);
		}
		
		CREATE(data, struct obj_event_data, 1);
		data->obj = obj;
		
		ev = dg_event_create(obj_hour_event, data, seconds RL_SEC);
		add_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_TIMER, ev);
	}
}


/**
* Schedules obj timers on an object plus any objects inside it, and inside
* those objects, and so on.
*
* @param obj_data *obj The object to schedule timers on.
*/
static void schedule_all_obj_timers_recursive(obj_data *obj) {
	obj_data *inner, *next_obj;
	
	if (obj) {
		schedule_obj_timer_update(obj, TRUE);
		DL_FOREACH_SAFE2(obj->contains, inner, next_obj, next_content) {
			schedule_all_obj_timers_recursive(inner);
	    }
	}
}


/**
* Called when a player enters the game to schedule timers as-needed for all
* their items. May be called in other cases where this is appropriate, too. It
* will override any existing timers on the items.
*
* @param char_data *ch The person whose objects need scheduling.
*/
void schedule_all_obj_timers(char_data *ch) {
	obj_data *obj, *next_obj;
	int pos;
	
	for (pos = 0; pos < NUM_WEARS; ++pos) {
		if (GET_EQ(ch, pos)) {
			schedule_all_obj_timers_recursive(GET_EQ(ch, pos));
		}
	}
	
	DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		schedule_all_obj_timers_recursive(obj);
	}
}


/**
* Does a game-hour tick on an object's timer.
*
* @param obj_data *obj The object.
* @return bool FALSE if the object was purged, TRUE if it survived
*/
bool tick_obj_timer(obj_data *obj) {
	char *to_char_str = NULL, *to_room_str = NULL;
	room_data *obj_r;
	int trig_result;
	
	// timer count down
	if (GET_OBJ_TIMER(obj) > 0) {
		GET_OBJ_TIMER(obj)--;
	}

	if (GET_OBJ_TIMER(obj) == 0) {
		obj_r = obj_room(obj);
		
		// run triggers (-1 = purged, 0 is just cancel this expiry)
		trig_result = timer_otrigger(obj);
		if (trig_result == -1) {
			return FALSE;
		}
		else if (trig_result == 0 || obj_room(obj) != obj_r) {
			return TRUE;	// survived, but not finishing the timer
		}
		
		if (IS_DRINK_CONTAINER(obj)) {
			// special handling: drink containers USUALLY just get emptied
			// messaging: 
			if (GET_DRINK_CONTAINER_CONTENTS(obj) > 0 || OBJ_FLAGGED(obj, OBJ_SINGLE_USE)) {
				// messaging to char
				if (obj->carried_by || obj->worn_by) {
					if (obj_has_custom_message(obj, OBJ_CUSTOM_DECAYS_ON_CHAR)) {
						to_char_str = obj_get_custom_message(obj, OBJ_CUSTOM_DECAYS_ON_CHAR);
					}
					else {
						to_char_str = "$p has gone bad and you pour it out.";
					}

					if (obj->carried_by && to_char_str) {
						act(to_char_str, FALSE, obj->carried_by, obj, NULL, TO_CHAR);
					}
					else if (obj->worn_by && to_char_str) {
						act(to_char_str, FALSE, obj->worn_by, obj, NULL, TO_CHAR);
					}
				}
				// messaging to room
				else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
					if (obj_has_custom_message(obj, OBJ_CUSTOM_DECAYS_IN_ROOM)) {
						to_room_str = obj_get_custom_message(obj, OBJ_CUSTOM_DECAYS_IN_ROOM);
					}
					else {
						to_room_str = "$p has gone bad.";
					}
					
					if (to_room_str) {
						act(to_room_str, FALSE, ROOM_PEOPLE(IN_ROOM(obj)), obj, NULL, TO_CHAR | TO_ROOM | TO_QUEUE);
					}
				}
			}
			
			set_obj_val(obj, VAL_DRINK_CONTAINER_CONTENTS, 0);
			set_obj_val(obj, VAL_DRINK_CONTAINER_TYPE, 0);
			
			GET_OBJ_TIMER(obj) = UNLIMITED;
			request_obj_save_in_world(obj);
			
			// do not extract unless it's 1-use
			if (OBJ_FLAGGED(obj, OBJ_SINGLE_USE)) {
				// decays-to
				run_interactions(NULL, GET_OBJ_INTERACTIONS(obj), INTERACT_DECAYS_TO, obj_room(obj), NULL, obj, NULL, consumes_or_decays_interact);
				
				empty_obj_before_extract(obj);
				extract_obj(obj);
				return FALSE;
			}
		}
		else {  // all others (not drink containers) actually decay
			// check custom messages first, then material message, then default
			if (obj->carried_by || obj->worn_by) {
				if (obj_has_custom_message(obj, OBJ_CUSTOM_DECAYS_ON_CHAR)) {
					to_char_str = obj_get_custom_message(obj, OBJ_CUSTOM_DECAYS_ON_CHAR);
				}
				else if (materials[GET_OBJ_MATERIAL(obj)].decay_on_char) {
					to_char_str = materials[GET_OBJ_MATERIAL(obj)].decay_on_char;
				}
				else {
					to_char_str = "$p disintegrates in your hands.";
				}
			}
			else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
				if (obj_has_custom_message(obj, OBJ_CUSTOM_DECAYS_IN_ROOM)) {
					to_room_str = obj_get_custom_message(obj, OBJ_CUSTOM_DECAYS_IN_ROOM);
				}
				else if (materials[GET_OBJ_MATERIAL(obj)].decay_in_room) {
					to_room_str = materials[GET_OBJ_MATERIAL(obj)].decay_in_room;
				}
				else {
					to_room_str = "$p disintegrates and falls apart.";
				}
			}
			
			// send messages
			if (obj->carried_by && to_char_str) {
				act(to_char_str, FALSE, obj->carried_by, obj, NULL, TO_CHAR | TO_QUEUE);
			}
			else if (obj->worn_by && to_char_str) {
				act(to_char_str, FALSE, obj->worn_by, obj, NULL, TO_CHAR | TO_QUEUE);
			}
			if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj)) && to_room_str) {
				act(to_room_str, FALSE,ROOM_PEOPLE(IN_ROOM(obj)), obj, NULL, TO_CHAR | TO_ROOM | TO_QUEUE);
			}
			
			// decays-to
			run_interactions(NULL, GET_OBJ_INTERACTIONS(obj), INTERACT_DECAYS_TO, obj_room(obj), NULL, obj, NULL, consumes_or_decays_interact);
			
			empty_obj_before_extract(obj);
			extract_obj(obj);
			return FALSE;
		} // end non-drink decay
	}
	
	// obj survived?
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// ROOM LIMITS /////////////////////////////////////////////////////////////


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
	DL_FOREACH_SAFE2(VEH_CONTAINS(veh), obj, next_obj, next_content) {
		check_autostore(obj, TRUE, VEH_OWNER(veh));
	}
}


/**
* This runs an hourly "point update" on a vehicle.
*
* @param vehicle_data *veh The vehicle to update.
*/
void point_update_vehicle(vehicle_data *veh) {
	if (VEH_IS_EXTRACTED(veh)) {
		return;
	}
	
	// autostore
	if ((time(0) - VEH_LAST_MOVE_TIME(veh)) > (config_get_int("autostore_time") * SECS_PER_REAL_MIN)) {
		autostore_vehicle_contents(veh);
	}
	
	// climate check: only once per day (based on room vnum)
	if (main_time_info.hours == (GET_ROOM_VNUM(IN_ROOM(veh)) % 24)) {
		if (!check_vehicle_climate_change(veh, FALSE)) {
			return;
		}
	}
	
	if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		// burny burny burny!
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			act("The flames roar as they envelop $V!", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM);
		}
		if (!besiege_vehicle(NULL, veh, MAX(1, (VEH_MAX_HEALTH(veh) / 12)), SIEGE_BURNING, NULL)) {
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
	if (IS_ADVENTURE_ROOM(loc) && (inst = find_instance_by_room(loc, FALSE, FALSE))) {
		// only if not already in there
		if (!IS_ADVENTURE_ROOM(IN_ROOM(ch)) || find_instance_by_room(IN_ROOM(ch), FALSE, FALSE) != inst) {
			if (!can_enter_instance(ch, inst)) {
				return FALSE;
			}
		}
	}
	
	// !teleport mob?
	DL_FOREACH2(ROOM_PEOPLE(loc), mob, next_in_room) {
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
	struct trading_post_data *tpd, *next_tpd;
	int *notify_list = NULL, top_notify = -1;
	bool changed = FALSE;
	char_data *ch;
	int iter, sub, count;
	long diff;
	
	int trading_post_days_to_timeout = config_get_int("trading_post_days_to_timeout");
	
	DL_FOREACH_SAFE(trading_list, tpd, next_tpd) {
		diff = time(0) - tpd->start;
		
		if ((!IS_SET(tpd->state, TPD_FOR_SALE | TPD_OBJ_PENDING | TPD_COINS_PENDING) || diff >= (trading_post_days_to_timeout * SECS_PER_REAL_DAY)) && !is_playing(tpd->player)) {
			// expired (or missing a flag that indicates it's still relevant), remove completely
			if (tpd->obj) {
				add_to_object_list(tpd->obj);
				extract_obj(tpd->obj);
				tpd->obj = NULL;
			}
			
			DL_DELETE(trading_list, tpd);
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
	bool intoxicated;

	// no change?
	if (IS_NPC(ch) || GET_COND(ch, condition) == UNLIMITED) {
		return;
	}
	
	// things that prevent thirst
	if (value > 0 && condition == THIRST && (HAS_BONUS_TRAIT(ch, BONUS_NO_THIRST) || has_player_tech(ch, PTECH_NO_THIRST))) {
		return;
	}
	
	// things that prevent hunger
	if (value > 0 && condition == FULL && (HAS_BONUS_TRAIT(ch, BONUS_NO_HUNGER) || has_player_tech(ch, PTECH_NO_HUNGER))) {
		return;
	}

	intoxicated = (GET_COND(ch, DRUNK) > 0);
	
	// add the value
	SAFE_ADD(GET_COND(ch, condition), value, 0, MAX_CONDITION, FALSE);
	
	// prevent well-fed if hungry
	if (IS_HUNGRY(ch) && value > 0) {
		affect_from_char(ch, ATYPE_WELL_FED, TRUE);
	}
	
	// too soon for a message?
	if (GET_LAST_COND_MESSAGE_TIME(ch, condition) + SECS_PER_REAL_MIN > time(0)) {
		return;
	}

	switch (condition) {
		case FULL: {
			if (IS_HUNGRY(ch) && value > 0) {
				msg_to_char(ch, "You are hungry.\r\n");
				GET_LAST_COND_MESSAGE_TIME(ch, condition) = time(0);
			}
			return;
		}
		case THIRST: {
			if (IS_THIRSTY(ch) && value > 0) {
				msg_to_char(ch, "You are thirsty.\r\n");
				GET_LAST_COND_MESSAGE_TIME(ch, condition) = time(0);
			}
			break;
		}
		case DRUNK: {
			if (intoxicated && !GET_COND(ch, condition)) {
				msg_to_char(ch, "You are now sober.\r\n");
				GET_LAST_COND_MESSAGE_TIME(ch, condition) = time(0);
			}
			break;
		}
	}
}


// processes heal-over-time applis
EVENTFUNC(heal_over_time_event) {
	struct char_event_data *data = (struct char_event_data*)event_obj;
	char_data *ch = data->character;
	
	// ensure in-game or skip it
	if (!IN_ROOM(ch)) {
		return 5 RL_SEC;	// re-enqueue
	}
	
	// always delete first
	delete_stored_event(&GET_STORED_EVENTS(ch), SEV_HEAL_OVER_TIME);
	
	// work
	if (GET_HEAL_OVER_TIME(ch) > 0 && !IS_DEAD(ch) && GET_POS(ch) >= POS_SLEEPING && GET_HEALTH(ch) > 0) {
		heal(ch, ch, GET_HEAL_OVER_TIME(ch));
	}
	
	// check done?
	if (GET_HEAL_OVER_TIME(ch) <= 0 || IS_DEAD(ch) || EXTRACTED(ch)) {
		free(data);
		return 0;	// no re-enqueue
	}
	else {
		// re-store, re-enqueue, and try again
		if (!find_stored_event(GET_STORED_EVENTS(ch), SEV_HEAL_OVER_TIME)) {
			add_stored_event(&GET_STORED_EVENTS(ch), SEV_HEAL_OVER_TIME, the_event);
			return 5 RL_SEC;
		}
		else {
			// already added a new one -- just flush this one
			free(data);
			return 0;
		}
	}
}


/**
* health per real update (usually 5 seconds)
*
* @param char_data *ch the person
* @param bool info_only TRUE = no skillups
* @return int How much health to gain per update.
*/
int health_gain(char_data *ch, bool info_only) {
	double gain, min, needed;
	
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
		
		if (GET_POS(ch) == POS_SLEEPING && !AFF_FLAGGED(ch, AFF_EARTHMELD)) {
			needed = GET_MAX_HEALTH(ch) + GET_HEALTH_DEFICIT(ch);
			min = round(needed / ((double) config_get_int("max_sleeping_regen_time") / (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_BEDROOM) ? 2.0 : 1.0) / SECS_PER_REAL_UPDATE));
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
* mana per real update (usually 5 seconds)
*
* @param char_data *ch the person
* @param bool info_only TRUE = no skillups
* @return int How much mana to gain per update.
*/
int mana_gain(char_data *ch, bool info_only) {
	double gain, min, needed;
	
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
		
		if (GET_POS(ch) == POS_SLEEPING && !AFF_FLAGGED(ch, AFF_EARTHMELD)) {
			needed = GET_MAX_MANA(ch) + GET_MANA_DEFICIT(ch);
			min = round(needed / ((double) config_get_int("max_sleeping_regen_time") / (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_BEDROOM) ? 2.0 : 1.0) / SECS_PER_REAL_UPDATE));
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
* move per real update (usually 5 seconds)
*
* @param char_data *ch the person
* @param bool info_only TRUE = no skillups
* @return int How much move to gain per update.
*/
int move_gain(char_data *ch, bool info_only) {
	double gain, min, needed;
	
	if (IS_INJURED(ch, INJ_STAKED | INJ_TIED)) {
		return 0;
	}

	if (IS_NPC(ch)) {
		gain = get_approximate_level(ch) / 10.0;
		gain += GET_MOVE_REGEN(ch);
	}
	else {
		// swimming shows as -1 on info
		if (info_only && !IS_IMMORTAL(ch) && IS_SWIMMING(ch)) {
			return -1;
		}
	
		gain = regen_by_pos[(int) GET_POS(ch)];
		gain += GET_MOVE_REGEN(ch);
		
		if (HAS_BONUS_TRAIT(ch, BONUS_MOVE_REGEN)) {
			gain += 1 + (get_approximate_level(ch) / 20);
		}
		
		if (GET_POS(ch) == POS_SLEEPING && !AFF_FLAGGED(ch, AFF_EARTHMELD)) {
			needed = GET_MAX_MOVE(ch) + GET_MOVE_DEFICIT(ch);
			min = round(needed / ((double) config_get_int("max_sleeping_regen_time") / (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_BEDROOM) ? 2.0 : 1.0) / SECS_PER_REAL_UPDATE));
			gain = MAX(gain, min);
		}

		if (IS_HUNGRY(ch) || IS_THIRSTY(ch) || IS_BLOOD_STARVED(ch)) {
			gain /= 4;
		}
	}

	return MAX(0, (int)gain);
}


/**
* Schedules an event for heal-over-time applies, if needed.
*
* @param char_data *ch The person with a heal-over-time effect.
*/
void schedule_heal_over_time(char_data *ch) {
	struct char_event_data *data;
	struct dg_event *ev;
	
	if (ch && GET_HEAL_OVER_TIME(ch) > 0 && !find_stored_event(GET_STORED_EVENTS(ch), SEV_HEAL_OVER_TIME)) {
		CREATE(data, struct char_event_data, 1);
		data->character = ch;
		
		ev = dg_event_create(heal_over_time_event, data, 5 RL_SEC);
		add_stored_event(&GET_STORED_EVENTS(ch), SEV_HEAL_OVER_TIME, ev);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CORE PERIODICALS ////////////////////////////////////////////////////////

/**
* Real Update: generally every 5 seconds, primarily for player characters,
* affects, and objects. REAL_UPDATES_PER_MUD_HOUR determines actual timing.
*/
void real_update(void) {
	char_data *ch;
	vehicle_data *veh;
	
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
	
	// advance the cycles
	if (++point_update_cycle >= REAL_UPDATES_PER_MUD_HOUR) {
		point_update_cycle = 0;
	}

	// players
	DL_FOREACH_SAFE2(player_character_list, ch, global_next_player, next_plr) {
		if (!EXTRACTED(ch)) {
			real_update_player(ch);
		}
	}
	
	// vehicles
	DL_FOREACH_SAFE(vehicle_list, veh, global_next_vehicle) {
		// each vehicle runs during part of the hour
		if (!VEH_IS_EXTRACTED(veh) && (ABSOLUTE(VEH_VNUM(veh)) % REAL_UPDATES_PER_MUD_HOUR) == point_update_cycle) {
			point_update_vehicle(veh);
		}
	}
}
