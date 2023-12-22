/* ************************************************************************
*   File: spells.c                                        EmpireMUD 2.0b5 *
*  Usage: implementation for spells                                       *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
*   Utilities
*   Ready
*/

 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* This function checks if the character has a counterspell available and
* pops it if so.
*
* @param char_data *ch The player who might have a counterspell.
* @param char_data *triggered_by Optional: the person who caused the counterspell, for ability hook targets (may be NULL).
* @return bool TRUE if a counterspell fired, FALSE if the spell can proceed.
*/
bool trigger_counterspell(char_data *ch, char_data *triggered_by) {
	ability_data *abil = NULL;
	struct affected_type *aff;
	
	if (AFF_FLAGGED(ch, AFF_COUNTERSPELL)) {
		msg_to_char(ch, "Your counterspell goes off!\r\n");
		
		// find first counterspell aff for later
		LL_FOREACH(ch->affected, aff) {
			if (IS_SET(aff->bitvector, AFF_COUNTERSPELL)) {
				abil = has_buff_ability_by_affect_and_affect_vnum(ch, AFF_COUNTERSPELL, aff->type);
				break;
			}
		}
		
		// remove first one
		remove_first_aff_flag_from_char(ch, AFF_COUNTERSPELL, FALSE);
		
		// did we find an ability that caused it?
		if (abil) {
			gain_ability_exp(ch, ABIL_VNUM(abil), 100);
			run_ability_hooks(ch, AHOOK_ABILITY, ABIL_VNUM(abil), 0, triggered_by, NULL, NULL, NULL);
		}
		return TRUE;
	}
	
	return FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// READY ///////////////////////////////////////////////////////////////////

ACMD(do_ready) {
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH], *to_char, *to_room;
	struct player_ability_data *plab, *next_plab;
	int scale_level, ch_level = 0, pos, obj_ok;
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
					found = TRUE;
					
					if (size + lsize < sizeof(buf)) {
						strcat(buf, line);
						size += lsize;
					}
					else {
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
	if (!char_can_act(ch, ABIL_MIN_POS(found_abil), !ABILITY_FLAGGED(found_abil, ABILF_NO_ANIMAL), !ABILITY_FLAGGED(found_abil, ABILF_NO_INVULNERABLE | ABILF_VIOLENT), FALSE)) {
		return;
	}
	
	// pass through to ready-weapon ability
	perform_ability_command(ch, found_abil, argument);
	return;
	
	if (!can_use_ability(ch, ABIL_VNUM(found_abil), ABIL_COST_TYPE(found_abil), ABIL_COST(found_abil), ABIL_COOLDOWN(found_abil))) {
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
	
	charge_ability_cost(ch, ABIL_COST_TYPE(found_abil), ABIL_COST(found_abil), ABIL_COOLDOWN(abil), ABIL_COOLDOWN_SECS(abil), ABIL_WAIT_TYPE(abil));
	send_pre_ability_messages(ch, NULL, NULL, found_abil, NULL);
	
	if (!skill_check(ch, ABIL_VNUM(found_abil), ABIL_DIFFICULTY(found_abil))) {
		send_ability_fail_messages(ch, NULL, NULL, found_abil, NULL);
		return;
	}
	
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
	
	// non-custom messages
	switch (ABIL_COST_TYPE(found_abil)) {
		case MANA: {
			to_char = "Mana twists and swirls around your hand and becomes $p!";
			to_room = "Mana twists and swirls around $n's hand and becomes $p!";
			break;
		}
		case BLOOD: {
			to_char = "You drain blood from your wrist and mold it into $p!";
			to_room = "$n twists and molds $s own blood into $p!";
			break;
		}
		// HEALTH, MOVE (these could have their own messages if they were used)
		default: {
			to_char = "You pull $p from the ether!";
			to_room = "$n pulls $p from the ether!";
			break;
		}
	}
	
	// custom overrides
	if (abil_has_custom_message(found_abil, ABIL_CUSTOM_SELF_TO_CHAR)) {
		to_char = abil_get_custom_message(found_abil, ABIL_CUSTOM_SELF_TO_CHAR);
	}
	if (abil_has_custom_message(found_abil, ABIL_CUSTOM_SELF_TO_ROOM)) {
		to_room = abil_get_custom_message(found_abil, ABIL_CUSTOM_SELF_TO_ROOM);
	}
	
	act(to_char, FALSE, ch, obj, NULL, TO_CHAR);
	act(to_room, ABILITY_FLAGGED(found_abil, ABILF_INVISIBLE) ? TRUE : FALSE, ch, obj, NULL, TO_ROOM);
	
	equip_char(ch, obj, pos);
	determine_gear_level(ch);
	
	gain_ability_exp(ch, ABIL_VNUM(found_abil), 15);
	
	obj_ok = load_otrigger(obj);
	// this goes directly to equipment so a GET trigger does not fire
	
	run_ability_hooks(ch, AHOOK_ABILITY, ABIL_VNUM(found_abil), 0, NULL, (obj_ok ? obj : NULL), NULL, NULL);
}
