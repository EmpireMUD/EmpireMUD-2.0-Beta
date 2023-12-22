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
	bool found, full;
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH];
	size_t size, lsize;
	ability_data *abil, *found_abil;
	obj_data *proto;
	struct ability_data_list *adl;
	struct player_ability_data *plab, *next_plab;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't use ready.\r\n");
		return;
	}
	
	#define VALID_READY_ABIL(ch, plab, abil)  ((abil) && (plab) && (plab)->purchased[GET_CURRENT_SKILL_SET(ch)] && IS_SET(ABIL_TYPES(abil), ABILT_READY_WEAPONS) && (!ABIL_COMMAND(abil) || !str_cmp(ABIL_COMMAND(abil), "ready")))
	
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
}
