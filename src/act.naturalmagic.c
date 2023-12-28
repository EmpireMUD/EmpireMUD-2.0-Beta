/* ************************************************************************
*   File: act.naturalmagic.c                              EmpireMUD 2.0b5 *
*  Usage: implementation for natural magic abilities                      *
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
*   Helpers
*   Commands
*/

// external funcs
ACMD(do_dismount);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Despawns ch's companion if it has the given vnum. This will send a 'leaves'
* message.
* 
* @param char_data *ch The player whose companion we might despawn.
* @param mob_vnum vnum Despawn only if matching this vnum (pass NOTHING for all companions).
* @return bool TRUE if it despawned a mob; FALSE if not.
*/
bool despawn_companion(char_data *ch, mob_vnum vnum) {
	char_data *mob;
	
	if (!(mob = GET_COMPANION(ch))) {
		return FALSE;	// no companion
	}
	if (!IS_NPC(mob)) {
		return FALSE;	// is a player
	}
	if (vnum != NOTHING && GET_MOB_VNUM(mob) != vnum) {
		return FALSE;	// wrong vnum
	}
	
	// seems despawnable
	if (!AFF_FLAGGED(mob, AFF_HIDE | AFF_NO_SEE_IN_ROOM)) {
		act("$n leaves.", TRUE, mob, NULL, NULL, TO_ROOM);
	}
	extract_char(mob);
	return TRUE;
}


/**
* Ends an earthmeld and sends messages.
*
* @param char_data *ch The earthmelded one.
*/
void un_earthmeld(char_data *ch) {
	if (AFF_FLAGGED(ch, AFF_EARTHMELD)) {
		affects_from_char_by_aff_flag(ch, AFF_EARTHMELD, FALSE);
		if (!AFF_FLAGGED(ch, AFF_EARTHMELD)) {
			msg_to_char(ch, "You rise from the ground!\r\n");
			act("$n rises from the ground!", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_STANDING;
			add_cooldown(ch, COOLDOWN_EARTHMELD, 2 * SECS_PER_REAL_MIN);
			
			if (affected_by_spell(ch, ATYPE_NATURE_BURN)) {
				affect_from_char(ch, ATYPE_NATURE_BURN, TRUE);
				command_lag(ch, WAIT_ABILITY);
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_confer) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	struct affected_type *aff, *aff_iter;
	bool any, found_existing, found_ch;
	int amt, iter, abbrev, type, conferred_amt, avail_str;
	long match_duration = 0;
	char_data *vict = ch;
	
	// configs
	long duration = 6 * SECS_PER_REAL_MIN;
	int cost = 50;

	struct {
		char *name;
		int apply;
	} confer_list[] = {
		{ "block", APPLY_BLOCK },
		{ "dodge", APPLY_DODGE },
		{ "health", APPLY_HEALTH },
		{ "health-regen", APPLY_HEALTH_REGEN },
		{ "mana", APPLY_MANA },
		{ "mana-regen", APPLY_MANA_REGEN },
		{ "move", APPLY_MOVE },
		{ "move-regen", APPLY_MOVE_REGEN },
		{ "resist-magical", APPLY_RESIST_MAGICAL },
		{ "resist-physical", APPLY_RESIST_PHYSICAL },
		{ "to-hit", APPLY_TO_HIT },
		
		// last
		{ "\n", APPLY_NONE }
	};
	
	two_arguments(argument, arg1, arg2);
	
	if (!can_use_ability(ch, ABIL_CONFER, MANA, cost, NOTHING)) {
		return;
	}
	
	if (!*arg1) {
		msg_to_char(ch, "Usage: confer <trait> [person]\r\n");
		msg_to_char(ch, "You know how to confer:");
		any = FALSE;
		for (iter = 0; *confer_list[iter].name != '\n'; ++iter) {
			msg_to_char(ch, "%s%s", (any ? ", " : " "), confer_list[iter].name);
			any = TRUE;
		}
		if (!any) {
			msg_to_char(ch, " nothing");
		}
		msg_to_char(ch, "\r\n");
		return;
	}

	// optional 2nd arg
	if (*arg2 && !(vict = get_char_vis(ch, arg2, NULL, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
		return;
	}
	
	// compute max confer
	conferred_amt = 0;
	LL_FOREACH(vict->affected, aff) {
		if (aff->type == ATYPE_CONFER && aff->cast_by == GET_IDNUM(ch)) {
			conferred_amt += apply_values[(int) aff->location] / apply_values[APPLY_STRENGTH];
		}
	}
	avail_str = GET_STRENGTH(ch);
	LL_FOREACH(ch->affected, aff) {
		if (aff->type == ATYPE_CONFERRED && aff->location == APPLY_STRENGTH) {
			avail_str += (-1 * aff->modifier);
		}
	}
	if (conferred_amt >= avail_str || conferred_amt >= att_max(ch)) {
		msg_to_char(ch, "You cannot confer any more strength on %s.\r\n", (ch == vict) ? "yourself" : PERS(vict, vict, FALSE));
		return;
	}
	
	// find type in list: prefer exact match
	type = abbrev = NOTHING;
	for (iter = 0; *confer_list[iter].name != '\n'; ++iter) {
		if (!str_cmp(arg1, confer_list[iter].name)) {
			type = iter;
			break;
		}
		else if (is_abbrev(arg1, confer_list[iter].name)) {
			abbrev = iter;
		}
	}
	if (type == NOTHING) {
		type = abbrev;	// no exact match
	}
	
	// pre-validation complete
	if (GET_STRENGTH(ch) < 2) {
		msg_to_char(ch, "You have no strength to confer.\r\n");
	}
	else if (type == NOTHING || confer_list[type].apply == APPLY_NONE) {
		msg_to_char(ch, "You don't know how to confer '%s'.\r\n", arg1);
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_CONFER)) {
		return;
	}
	else {
		// good to go
		charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
		
		// messaging
		if (ch == vict) {
			snprintf(buf, sizeof(buf), "You confer your own strength into %s!", confer_list[type].name);
			act(buf, FALSE, ch, NULL, NULL, TO_CHAR | ACT_BUFF);
			// no message to room!
		}
		else {
			sprintf(buf, "You confer your strength into $N's %s!", confer_list[type].name);
			act(buf, FALSE, ch, NULL, vict, TO_CHAR | ACT_BUFF);
			sprintf(buf, "$n confers $s strength into your %s!", confer_list[type].name);
			act(buf, FALSE, ch, NULL, vict, TO_VICT | ACT_BUFF);
			act("$n confers $s strength into $N!", FALSE, ch, NULL, vict, TO_NOTVICT | ACT_BUFF);
		}
		
		// determine how much to give: based on what a point of strength is worth
		amt = round(apply_values[APPLY_STRENGTH] / apply_values[confer_list[type].apply]);
		amt = MAX(amt, 1);	// ensure at least 1 point of stuff
		
		// attempt to find an existing confer effect that matches and just add to its amount
		found_existing = found_ch = FALSE;
		for (aff_iter = vict->affected; aff_iter; aff_iter = aff_iter->next) {
			if (aff_iter->type == ATYPE_CONFER && aff_iter->cast_by == CAST_BY_ID(ch) && aff_iter->location == confer_list[type].apply) {
				found_existing = TRUE;
				aff_iter->modifier += amt;
				match_duration = aff_iter->expire_time;	// store this to match it later
				aff_iter->expire_time = time(0) + duration;	// reset to max duration

				// ensure stats are correct
				affect_modify(vict, aff_iter->location, amt, NOBITS, TRUE);
				schedule_affect_expire(vict, aff_iter);
				affect_total(vict);
				break;
			}
		}
		
		if (found_existing) {
			// if there was an existing aff on the target, maybe there is one on ch
			found_ch = FALSE;
			for (aff_iter = ch->affected; aff_iter; aff_iter = aff_iter->next) {
				// we match by duration because lengthening any -str affect that had the same duration is equally good
				if (aff_iter->type == ATYPE_CONFERRED && aff_iter->expire_time == match_duration) {
					found_ch = TRUE;
					aff_iter->modifier -= 1;	// additional -1 strength
					aff_iter->expire_time = time(0) + duration;	// reset to max duration

					// ensure stats are correct
					affect_modify(ch, aff_iter->location, -1, NOBITS, TRUE);
					schedule_affect_expire(ch, aff_iter);
					affect_total(ch);
					break;
				}
			}
		}
		else {
			// did not find existing: add if needed
			aff = create_mod_aff(ATYPE_CONFER, duration, confer_list[type].apply, amt, ch);
			
			// use affect_to_char instead of affect_join because we will allow multiple copies of this with different durations
			affect_to_char(vict, aff);
			free(aff);
		}
		
		// separately ...
		if (!found_ch) {
			// need a new strength effect on ch
			aff = create_mod_aff(ATYPE_CONFERRED, duration, APPLY_STRENGTH, -1, ch);
			
			// use affect_to_char instead of affect_join because we will allow multiple copies of this with different durations
			affect_to_char(ch, aff);
			free(aff);
		}
	}
}


ACMD(do_earthmeld) {
	struct affected_type *af;
	int cost = 50;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs may not earthmeld.\r\n");
		return;
	}

	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_EARTHMELD)) {
		return;
	}

	if (AFF_FLAGGED(ch, AFF_EARTHMELD)) {
		// only check sector on rise if the person has earth mastery, otherwise they are trapped
		if (has_ability(ch, ABIL_WORM) && IS_COMPLETE(IN_ROOM(ch)) && IS_ANY_BUILDING(IN_ROOM(ch)) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_OPEN) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_BARRIER)) {
			msg_to_char(ch, "You can't rise from the earth here!\r\n");
		}
		else {
			un_earthmeld(ch);
			look_at_room(ch);
		}
		return;
	}

	// sect validation
	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_SHALLOW_WATER | SECTF_INSIDE) || (GET_BUILDING(IN_ROOM(ch)) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_OPEN))) {
		msg_to_char(ch, "You can't earthmeld without natural ground below you!\r\n");
		return;
	}
	
	if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_BARRIER) && IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't earthmeld here.\r\n");
		return;
	}
	
	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_SHALLOW_WATER) || SECT_FLAGGED(BASE_SECT(IN_ROOM(ch)), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_SHALLOW_WATER) || IS_SET(get_climate(IN_ROOM(ch)), CLIM_FRESH_WATER | CLIM_SALT_WATER | CLIM_FROZEN_WATER | CLIM_OCEAN | CLIM_LAKE)) {
		msg_to_char(ch, "You can't earthmeld without solid ground below you!\r\n");
		return;
	}
	
	if (IS_ADVENTURE_ROOM(IN_ROOM(ch)) && (!RMT_FLAGGED(IN_ROOM(ch), RMT_OUTDOOR) || ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_NEED_BOAT) || RMT_FLAGGED(IN_ROOM(ch), RMT_NEED_BOAT))) {
		msg_to_char(ch, "You can't earthmeld without natural ground below you!\r\n");
		return;
	}

	if (!can_use_ability(ch, ABIL_EARTHMELD, MANA, cost, COOLDOWN_EARTHMELD)) {
		return;
	}
	
	if (GET_POS(ch) == POS_FIGHTING) {
		msg_to_char(ch, "You can't do that while fighting!\r\n");
		return;
	}
	if (GET_POS(ch) < POS_STANDING) {
		msg_to_char(ch, "You can't do that right now. You need to be standing.\r\n");
		return;
	}
	
	if (IS_RIDING(ch)) {
		if (PRF_FLAGGED(ch, PRF_AUTODISMOUNT)) {
			do_dismount(ch, "", 0, 0);
		}
		else {
			msg_to_char(ch, "You can't do that while mounted.\r\n");
			return;
		}
	}
	
	// TODO why isn't this using charge ability cost
	set_mana(ch, GET_MANA(ch) - cost);
	
	msg_to_char(ch, "You dissolve into pure mana and sink into the ground!\r\n");
	act("$n dissolves into pure mana and sinks right into the ground!", TRUE, ch, 0, 0, TO_ROOM);
	GET_POS(ch) = POS_SLEEPING;

	af = create_aff(ATYPE_EARTHMELD, UNLIMITED, APPLY_NONE, 0, AFF_NO_TARGET_IN_ROOM | AFF_NO_SEE_IN_ROOM | AFF_EARTHMELD, ch);
	affect_join(ch, af, 0);
	
	gain_ability_exp(ch, ABIL_EARTHMELD, 15);
	run_ability_hooks(ch, AHOOK_ABILITY, ABIL_EARTHMELD, 0, NULL, NULL, NULL, NULL, NOBITS);
}
