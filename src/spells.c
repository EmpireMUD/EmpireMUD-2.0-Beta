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
*   Summon Functions
*   Utilities
*   Commands
*/

 //////////////////////////////////////////////////////////////////////////////
//// SUMMON FUNCTIONS ////////////////////////////////////////////////////////

/**
* Sends a player back to where they came from, if they were adventure-summoned
* and they left the adventure.
*
* @param char_data *ch The person to return back where they came from.
*/
void adventure_unsummon(char_data *ch) {
	room_data *room, *map, *was_in;
	struct follow_type *fol, *next_fol;
	bool reloc = FALSE;
	
	// safety first
	if (!ch || IS_NPC(ch) || !PLR_FLAGGED(ch, PLR_ADVENTURE_SUMMONED)) {
		return;
	}
	
	REMOVE_BIT(PLR_FLAGS(ch), PLR_ADVENTURE_SUMMONED);
	
	was_in = IN_ROOM(ch);
	room = real_room(GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch));
	map = real_room(GET_ADVENTURE_SUMMON_RETURN_MAP(ch));
	
	act("$n vanishes in a wisp of smoke!", TRUE, ch, NULL, NULL, TO_ROOM);
	
	/*
	*/
	
	if (room && map && GET_ROOM_VNUM(map) == (GET_MAP_LOC(room) ? GET_MAP_LOC(room)->vnum : NOTHING)) {
		char_to_room(ch, room);
	}
	else {
		// nowhere safe to send back to
		char_to_room(ch, find_load_room(ch));
		reloc = TRUE;
	}
	
	act("$n appears in a burst of smoke!", TRUE, ch, NULL, NULL, TO_ROOM);
	GET_LAST_DIR(ch) = NO_DIR;
	qt_visit_room(ch, IN_ROOM(ch));
	pre_greet_mtrigger(ch, IN_ROOM(ch), NO_DIR, "summon");	// cannot pre-greet for summon

	look_at_room(ch);
	if (reloc) {
		msg_to_char(ch, "\r\nYour original location could not be located. You have been returned to a safe location after leaving the adventure.\r\n");
	}
	else {
		msg_to_char(ch, "\r\nYou have been returned to your original location after leaving the adventure.\r\n");
	}
	
	enter_wtrigger(IN_ROOM(ch), ch, NO_DIR, "summon");
	entry_memory_mtrigger(ch);
	greet_mtrigger(ch, NO_DIR, "summon");
	greet_memory_mtrigger(ch);
	greet_vtrigger(ch, NO_DIR, "summon");
	
	msdp_update_room(ch);
	
	// followers?
	LL_FOREACH_SAFE(ch->followers, fol, next_fol) {
		if (IS_NPC(fol->follower) && AFF_FLAGGED(fol->follower, AFF_CHARM) && IN_ROOM(fol->follower) == was_in && !FIGHTING(fol->follower)) {
			act("$n vanishes in a wisp of smoke!", TRUE, fol->follower, NULL, NULL, TO_ROOM);
			char_to_room(fol->follower, IN_ROOM(ch));
			GET_LAST_DIR(fol->follower) = NO_DIR;
			pre_greet_mtrigger(fol->follower, IN_ROOM(fol->follower), NO_DIR, "summon");	// cannot pre-greet for summon
			look_at_room(fol->follower);
			act("$n appears in a burst of smoke!", TRUE, fol->follower, NULL, NULL, TO_ROOM);
			
			enter_wtrigger(IN_ROOM(fol->follower), fol->follower, NO_DIR, "summon");
			entry_memory_mtrigger(fol->follower);
			greet_mtrigger(fol->follower, NO_DIR, "summon");
			greet_memory_mtrigger(fol->follower);
			greet_vtrigger(fol->follower, NO_DIR, "summon");
		}
	}
}


/**
* Ensures a character won't be returned home by adventure_unsummon()
*
* @param char_data *ch The person to cancel the return-summon data for.
*/
void cancel_adventure_summon(char_data *ch) {
	if (!IS_NPC(ch)) {
		REMOVE_BIT(PLR_FLAGS(ch), PLR_ADVENTURE_SUMMONED);
		GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch) = NOWHERE;
		GET_ADVENTURE_SUMMON_RETURN_MAP(ch) = NOWHERE;
		GET_ADVENTURE_SUMMON_INSTANCE_ID(ch) = NOTHING;
	}
}


/**
* Skips past the word 'summon ' or 'summons ' (case-insensitive).
*
* @param char *input The string to strip of 'summon(s) ', if present.
* @return char* A pointer to the position in the string after the word 'summon(s) '.
*/
char *format_summon_name(char *input) {
	char *start = input;
	
	skip_spaces(&start);
	if (!strn_cmp(start, "summon ", 7)) {
		start += 7;
	}
	else if (!strn_cmp(start, "summons ", 7)) {
		start += 8;
	}
	
	return start;
}


/**
* Gets the line display for one summonable thing.
*
* @param char_data *ch The player.
* @param const char *name The ability or mob name to show.
* @param int min_level The required level, if any.
* @param ability_data *from_abil The ability that grants the summon, if any.
* @return char* The text for the summon list.
*/
char *one_summon_entry(char_data *ch, const char *name, int min_level, ability_data *from_abil) {
	static char output[256];
	size_t size;
	
	size = snprintf(output, sizeof(output), "%s", name);
	
	if (from_abil && ABIL_COST(from_abil) > 0) {
		size += snprintf(output + size, sizeof(output) - size, " (%d %s)", ABIL_COST(from_abil), pool_types[ABIL_COST_TYPE(from_abil)]);
	}
	if (min_level > 0 && (from_abil ? get_player_level_for_ability(ch, ABIL_VNUM(from_abil)) : get_approximate_level(ch)) < min_level) {
		size += snprintf(output + size, sizeof(output) - size, " \trrequires level %d\t0", min_level);
	}
	
	return output;
}


/**
* For the "adventure summon <player>" command.
*
* @param char_data *ch The player doing the summoning.
* @param char *argument The typed argument.
*/
void do_adventure_summon(char_data *ch, char *argument) {
	char arg[MAX_INPUT_LENGTH];
	struct instance_data *inst;
	char_data *vict;
	
	one_argument(argument, arg);
	
	if (GET_POS(ch) < POS_STANDING) {
		send_low_pos_msg(ch);
	}
	else if (!(inst = find_instance_by_room(IN_ROOM(ch), FALSE, FALSE))) {
		msg_to_char(ch, "You can only use the adventure summon command inside an adventure.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to summon players here.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Summon whom to the adventure?\r\n");
	}
	else if (!(vict = get_player_vis(ch, arg, FIND_CHAR_WORLD | FIND_NO_DARK))) {
		send_config_msg(ch, "no_person");
	}
	else if (vict == ch) {
		msg_to_char(ch, "You summon yourself! Look, you're already here.\r\n");
	}
	else if (!GROUP(ch) || GROUP(ch) != GROUP(vict)) {
		msg_to_char(ch, "You can only summon members of your own group.\r\n");
	}
	else if (IN_ROOM(vict) == IN_ROOM(ch)) {
		msg_to_char(ch, "Your target is already here.\r\n");
	}
	else if (IS_ADVENTURE_ROOM(IN_ROOM(vict))) {
		msg_to_char(ch, "You cannot summon someone who is already in an adventure.\r\n");
	}
	else if (!vict->desc) {
		msg_to_char(ch, "You can't summon someone who is linkdead.\r\n");
	}
	else if (GET_ACCOUNT(ch) == GET_ACCOUNT(vict)) {
		msg_to_char(ch, "You can't summon your own alts.\r\n");
	}
	else if (IS_DEAD(vict)) {
		msg_to_char(ch, "You cannot summon the dead like that.\r\n");
	}
	else if (!can_enter_instance(vict, inst)) {
		msg_to_char(ch, "Your target can't enter this instance.\r\n");
	}
	else if (!can_teleport_to(vict, IN_ROOM(vict), TRUE)) {
		msg_to_char(ch, "Your target can't be summoned from %s current location.\r\n", REAL_HSHR(vict));
	}
	else if (!can_teleport_to(vict, IN_ROOM(ch), FALSE)) {
		msg_to_char(ch, "Your target can't be summoned here.\r\n");
	}
	else if (PLR_FLAGGED(vict, PLR_ADVENTURE_SUMMONED)) {
		msg_to_char(ch, "You can't summon someone who is already adventure-summoned.\r\n");
	}
	else {
		act("You start summoning $N...", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n starts summoning $N...", FALSE, ch, NULL, vict, TO_ROOM);
		msg_to_char(vict, "%s is trying to summon you to %s (%s) -- use 'accept/reject summon'.\r\n", PERS(ch, ch, TRUE), GET_ADV_NAME(INST_ADVENTURE(inst)), get_room_name(IN_ROOM(ch), FALSE));
		add_offer(vict, ch, OFFER_SUMMON, SUMMON_ADVENTURE);
		command_lag(ch, WAIT_OTHER);
	}
}


/**
* Command processing for the "summon materials" ptech, called via the
* central do_summon command.
*
* @param char_data *ch The person using the command.
* @param char *argument The typed arg.
*/
void do_summon_materials(char_data *ch, char *argument) {
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
	
	if (run_ability_triggers_by_player_tech(ch, PTECH_SUMMON_MATERIALS, NULL, NULL, NULL)) {
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
			run_ability_hooks_by_player_tech(ch, PTECH_SUMMON_MATERIALS, NULL, NULL, NULL, NULL);
		}
	}
	
	command_lag(ch, WAIT_OTHER);
}


/**
* Begins a summon for a player -- see do_summon.
*
* @param char_data *ch The summoner.
* @param char *argument The typed-in arg.
*/
void do_summon_player(char_data *ch, char *argument) {
	char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	char_data *vict, *ch_iter;
	bool found;
	
	one_argument(argument, arg);
	
	if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_SUMMON_PLAYER)) {
		msg_to_char(ch, "You can't summon players here.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You must complete the building first.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to summon players here.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Summon whom?\r\n");
	}
	else if (!(vict = get_player_vis(ch, arg, FIND_CHAR_WORLD | FIND_NO_DARK))) {
		send_config_msg(ch, "no_person");
	}
	else if (vict == ch) {
		msg_to_char(ch, "You summon yourself! Look, you're already here.\r\n");
	}
	else if (!GROUP(ch) || GROUP(ch) != GROUP(vict)) {
		msg_to_char(ch, "You can only summon members of your own group.\r\n");
	}
	else if (IN_ROOM(vict) == IN_ROOM(ch)) {
		msg_to_char(ch, "Your target is already here.\r\n");
	}
	else if (IS_DEAD(vict)) {
		msg_to_char(ch, "You cannot summon the dead like that.\r\n");
	}
	else if (!can_teleport_to(vict, IN_ROOM(vict), TRUE)) {
		msg_to_char(ch, "Your target can't be summoned from %s current location.\r\n", REAL_HSHR(vict));
	}
	else if (!can_teleport_to(vict, IN_ROOM(ch), FALSE)) {
		msg_to_char(ch, "Your target can't be summoned here.\r\n");
	}
	else {
		// mostly-valid by now... just a little bit more to check
		// requires a 2nd group member present IF the target is from a different empire
		if (GET_LOYALTY(vict) != GET_LOYALTY(ch) || !GET_LOYALTY(ch)) {
			found = FALSE;
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), ch_iter, next_in_room) {
				if (IS_DEAD(ch_iter) || !ch_iter->desc) {
					continue;
				}
			
				if (ch_iter != ch && GROUP(ch_iter) == GROUP(ch)) {
					found = TRUE;
					break;
				}
			}
		
			if (!found) {
				msg_to_char(ch, "You need a second group member present to help summon.\r\n");
				return;
			}
		}
		
		act("You start summoning $N...", FALSE, ch, NULL, vict, TO_CHAR);
		snprintf(buf, sizeof(buf), "$o is trying to summon you to %s%s -- use 'accept/reject summon'.", get_room_name(IN_ROOM(ch), FALSE), coord_display_room(ch, IN_ROOM(ch), FALSE));
		act(buf, FALSE, ch, NULL, vict, TO_VICT | TO_SLEEP);
		add_offer(vict, ch, OFFER_SUMMON, SUMMON_PLAYER);
		command_lag(ch, WAIT_OTHER);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Determines if a character is protected by the MAJESTY affect, which makes it
* harder to attack them.
*
* @param char_data *ch The person with majesty.
* @param char_data *attacker Optional: Who is trying to hit them; will allow it if they're 50 levels above. (NULL to ignore)
* @return bool TRUE if the character is protected by MAJESTY, FALSE if not.
*/
bool has_majesty_immunity(char_data *ch, char_data *attacker) {
	ability_data *abil;
	struct player_ability_data *plab, *next_plab;
	int one_trait, best_trait = 0;
	
	const int over_level_ignore = 50;	// attacker this far above the char ignroes majesty
	
	// basic checks
	if (!AFF_FLAGGED(ch, AFF_MAJESTY)) {
		return FALSE;	// not affected
	}
	if (attacker && get_approximate_level(attacker) >= get_approximate_level(ch) + over_level_ignore) {
		return FALSE;	// over-level
	}
	
	// ok: try to find the majesty ability
	if (!IS_NPC(ch)) {
		HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
			if (!plab->purchased[GET_CURRENT_SKILL_SET(ch)]) {
				continue;	// wrong skill set
			}
			if (!(abil = plab->ptr) || !IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_PASSIVE_BUFF)) {
				continue;	// not a buff; not likely to be the source of Majesty
			}
			if (!IS_SET(ABIL_AFFECTS(abil), AFF_MAJESTY)) {
				continue;	// not a majesty ability
			}
			if (IS_SET(ABIL_TARGETS(abil), ATAR_NOT_SELF)) {
				continue;	// can't be one that doesn't target me
			}
		
			// ok, this ability is a likely source of majesty, but is it the best one?
			if (ABIL_LINKED_TRAIT(abil) != APPLY_NONE) {
				one_trait = get_attribute_by_apply(ch, ABIL_LINKED_TRAIT(abil));
				best_trait = MAX(best_trait, one_trait);
			}
		}
	}
	
	// if we failed to detect a trait, we default to Charisma for historical reasons
	// (NPCs also hit this block)
	if (best_trait < 1) {
		best_trait = GET_CHARISMA(ch);
	}
	
	// now the trait roll:
	return number(0, best_trait) ? TRUE : FALSE;
}


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
			run_ability_hooks(ch, AHOOK_ABILITY, ABIL_VNUM(abil), 0, triggered_by, NULL, NULL, NULL, NOBITS);
		}
		return TRUE;
	}
	
	return FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

// also do_chant, do_ritual: handles SCMD_CAST, SCMD_RITUAL, SCMD_CHANT
ACMD(do_cast) {
	bool found, full;
	char *arg2;
	size_t size, count;
	ability_data *abil;
	struct player_ability_data *plab, *next_plab;
	
	const char *cast_noun[] = { "spell", "ritual", "chant" };
	const char *cast_command[] = { "cast", "ritual", "chant" };
	
	#define VALID_CAST_ABIL(ch, plab)  ((plab)->ptr && (plab)->purchased[GET_CURRENT_SKILL_SET(ch)] && ABIL_COMMAND(abil) && !str_cmp(ABIL_COMMAND(abil), cast_command[subcmd]))
	
	arg2 = one_word(argument, arg);	// first arg: ritual/chant type
	skip_spaces(&arg2);	// remaining arg
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot do that.\r\n");
		return;
	}
	
	// no-arg: show list
	if (!*arg) {
		// already doing this?
		if (GET_ACTION(ch) == ACT_OVER_TIME_ABILITY && (abil = ability_proto(GET_ACTION_VNUM(ch, 0))) && (plab = get_ability_data(ch, ABIL_VNUM(abil), FALSE)) && VALID_CAST_ABIL(ch, plab)) {
			msg_to_char(ch, "You stop the %s.\r\n", cast_noun[subcmd]);
			cancel_action(ch);
			return;
		}
		
		size = snprintf(buf, sizeof(buf), "You know the following %ss:\r\n", cast_noun[subcmd]);
		
		found = full = FALSE;
		count = 0;
		HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
			abil = plab->ptr;
			if (!VALID_CAST_ABIL(ch, plab)) {
				continue;
			}
			
			// append
			if (size + strlen(ABIL_NAME(abil)) + 38 < sizeof(buf)) {
				size += snprintf(buf + size, sizeof(buf) - size, " %-34.34s%s", ABIL_NAME(abil), ((count++ % 2 || PRF_FLAGGED(ch, PRF_SCREEN_READER)) ? "\r\n" : ""));
			}
			else {
				full = TRUE;
				break;
			}
			
			// found 1
			found = TRUE;
		}
		
		if (!found) {
			strcat(buf, " nothing\r\n");	// always room for this if !found
			if (subcmd == SCMD_CAST) {
				strcat(buf, "(Most magical abilities have their own commands rather than 'cast'.)\r\n");
			}
		}
		else if (count % 2 && !full && !PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
			strcat(buf, "\r\n");
		}
		
		if (full) {
			snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
		}
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
		return;
	}	// end no-arg
	
	// with arg: determine what they typed
	found = FALSE;
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		abil = plab->ptr;
		if (!VALID_CAST_ABIL(ch, plab)) {
			continue;	// not a conjure ability
		}
		if (!multi_isname(arg, ABIL_NAME(abil))) {
			continue;	// wrong name: not-targeted
		}
		
		// match!
		perform_ability_command(ch, abil, arg2);
		found = TRUE;
		break;
	}
	
	if (!found) {
		msg_to_char(ch, "You don't know that %s.\r\n", cast_noun[subcmd]);
	}
}


ACMD(do_conjure) {
	bool found, full, needs_target;
	char whole_arg[MAX_INPUT_LENGTH];
	char *arg2;
	const char *ptr;
	size_t size, count;
	ability_data *abil;
	struct player_ability_data *plab, *next_plab;
	
	bitvector_t my_types = ABILT_CONJURE_LIQUID | ABILT_CONJURE_OBJECT | ABILT_CONJURE_VEHICLE;
	
	#define VALID_CONJURE_ABIL(ch, plab)  ((plab)->ptr && (plab)->purchased[GET_CURRENT_SKILL_SET(ch)] && IS_SET(ABIL_TYPES((plab)->ptr), my_types) && ABIL_COMMAND(abil) && !str_cmp(ABIL_COMMAND(abil), "conjure"))
	
	quoted_arg_or_all(argument, whole_arg);	// keep whole arg
	arg2 = one_word(argument, arg);	// also split first arg: conjure type
	skip_spaces(&arg2);	// remaining args
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot conjure.\r\n");
		return;
	}
	
	// no-arg: show conjurable list
	if (!*arg) {
		// already doing this?
		if (GET_ACTION(ch) == ACT_OVER_TIME_ABILITY && (abil = ability_proto(GET_ACTION_VNUM(ch, 0))) && (plab = get_ability_data(ch, ABIL_VNUM(abil), FALSE)) && VALID_CONJURE_ABIL(ch, plab)) {
			msg_to_char(ch, "You stop conjuring.\r\n");
			cancel_action(ch);
			return;
		}
		
		size = snprintf(buf, sizeof(buf), "You can conjure the following things:\r\n");
		
		found = full = FALSE;
		count = 0;
		HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
			abil = plab->ptr;
			if (!VALID_CONJURE_ABIL(ch, plab)) {
				continue;	// not a conjure ability
			}
			
			// show it
			if (IS_SET(ABIL_TYPES(abil), my_types)) {
				ptr = skip_wordlist(ABIL_NAME(abil), conjure_words, FALSE);
				
				// append
				if (size + strlen(ptr) + 38 < sizeof(buf)) {
					size += snprintf(buf + size, sizeof(buf) - size, " %-34.34s%s", ptr, ((count++ % 2 || PRF_FLAGGED(ch, PRF_SCREEN_READER)) ? "\r\n" : ""));
				}
				else {
					full = TRUE;
					break;
				}
			}
			else {
				continue;
			}
			
			// found 1
			found = TRUE;
			
			if (full) {
				break;
			}
		}
		
		if (!found) {
			strcat(buf, " nothing\r\n");	// always room for this if !found
		}
		else if (count % 2 && !full && !PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
			strcat(buf, "\r\n");
		}
		
		if (full) {
			snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
		}
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
		return;
	}	// end no-arg
	
	// with arg: determine what they typed
	found = FALSE;
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		abil = plab->ptr;
		needs_target = (IS_SET(ABIL_TYPES(abil), ABILT_CONJURE_LIQUID) ? TRUE : FALSE);
		if (!VALID_CONJURE_ABIL(ch, plab)) {
			continue;	// not a conjure ability
		}
		if (needs_target && !multi_isname(arg, skip_wordlist(ABIL_NAME(abil), conjure_words, FALSE))) {
			continue;	// wrong name: targeted
		}
		if (!needs_target && !multi_isname(whole_arg, skip_wordlist(ABIL_NAME(abil), conjure_words, FALSE))) {
			continue;	// wrong name: not-targeted
		}
		
		// run it? only if it matches
		if (IS_SET(ABIL_TYPES(abil), my_types)) {
			if (GET_POS(ch) < POS_RESTING || GET_POS(ch) < ABIL_MIN_POS(abil)) {
				send_low_pos_msg(ch);	// not high enough pos for this conjure
				return;
			}
			
			perform_ability_command(ch, abil, needs_target ? arg2 : "");
			found = TRUE;
			break;
		}
	}
	
	if (!found) {
		msg_to_char(ch, "You don't know how to conjure that.\r\n");
	}
}


ACMD(do_ready) {
	bool found, full;
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH];
	size_t size, lsize;
	ability_data *abil, *found_abil;
	obj_data *proto;
	struct ability_data_list *adl;
	struct player_ability_data *plab, *next_plab;
	
	quoted_arg_or_all(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't use ready.\r\n");
		return;
	}
	
	#define VALID_READY_ABIL(ch, plab, abil)  ((abil) && (plab) && (plab)->purchased[GET_CURRENT_SKILL_SET(ch)] && IS_SET(ABIL_TYPES(abil), ABILT_READY_WEAPONS) && (!ABIL_COMMAND(abil) || !str_cmp(ABIL_COMMAND(abil), "ready")))
	
	if (!*arg) {
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
				if (multi_isname(arg, GET_OBJ_KEYWORDS(proto))) {
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
	
	// pass through to ready-weapon ability
	perform_ability_command(ch, found_abil, arg);
}


ACMD(do_summon) {
	bool found, full;
	char buf[MAX_STRING_LENGTH * 2], arg[MAX_INPUT_LENGTH], *arg2, *line;
	int count, fol_count;
	size_t size, lsize;
	ability_data *abil;
	char_data *mob, *proto = NULL;
	struct ability_data_list *adl;
	struct player_ability_data *plab, *next_plab;
	
	// maximum npcs present when summoning
	int max_npcs = config_get_int("summon_npc_limit");
	int max_followers = config_get_int("npc_follower_limit");
	
	#define VALID_SUMMON_ABIL(ch, plab)  ((plab)->ptr && (plab)->purchased[GET_CURRENT_SKILL_SET(ch)] && IS_SET(ABIL_TYPES((plab)->ptr), ABILT_SUMMON_ANY | ABILT_SUMMON_RANDOM) && (!ABIL_COMMAND((plab)->ptr) || !str_cmp(ABIL_COMMAND((plab)->ptr), "summon")))
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't use this command.\r\n");
		return;
	}
	
	// special summons first
	arg2 = one_argument(argument, arg);	// split out first word for certain special summons
	if (!IS_NPC(ch) && *arg && is_abbrev(arg, "materials")) {
		do_summon_materials(ch, arg2);
		return;
	}
	if (!IS_NPC(ch) && *arg && is_abbrev(arg, "player")) {
		do_summon_player(ch, arg2);
		return;
	}
	
	// not a special summon: use the whole arg; check for quotes
	quoted_arg_or_all(argument, arg);
	
	// no-arg: show summonable list
	if (!*arg) {
		// already doing this?
		if (GET_ACTION(ch) == ACT_OVER_TIME_ABILITY && (abil = ability_proto(GET_ACTION_VNUM(ch, 0))) && (plab = get_ability_data(ch, ABIL_VNUM(abil), FALSE)) && VALID_SUMMON_ABIL(ch, plab)) {
			msg_to_char(ch, "You stop summoning.\r\n");
			cancel_action(ch);
			return;
		}
		
		size = snprintf(buf, sizeof(buf), "You can summon the following things:\r\n");
		
		found = full = FALSE;
		HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
			abil = plab->ptr;
			if (!VALID_SUMMON_ABIL(ch, plab)) {
				continue;
			}
			
			// show it
			if (IS_SET(ABIL_TYPES(abil), ABILT_SUMMON_ANY)) {
				// summon-any lists the mobs themselves
				LL_FOREACH(ABIL_DATA(abil), adl) {
					if (adl->type == ADL_SUMMON_MOB && (proto = mob_proto(adl->vnum))) {
						line = one_summon_entry(ch, skip_filler(GET_SHORT_DESC(proto)), GET_MIN_SCALE_LEVEL(proto), abil);
						lsize = strlen(line);
						if (size + lsize + 3 < sizeof(buf)) {
							size += snprintf(buf + size, sizeof(buf) - size, " %s\r\n", line);
						}
						else {
							full = TRUE;
							break;
						}
					}
				}
			}
			else if (IS_SET(ABIL_TYPES(abil), ABILT_SUMMON_RANDOM)) {
				// summon-random just shows the ability name, minus the word 'summon'
				line = one_summon_entry(ch, format_summon_name(ABIL_NAME(abil)), 0, abil);
				lsize = strlen(line);
				if (size + lsize + 3 < sizeof(buf)) {
					size += snprintf(buf + size, sizeof(buf) - size, " %s\r\n", line);
				}
				else {
					full = TRUE;
				}
			}
			else {
				continue;
			}
			
			// if we got here, we did find one
			found = TRUE;
			
			if (full) {
				break;
			}
		}
		
		if (!found) {
			strcat(buf, " nothing\r\n");	// always room for this if !found
		}
		if (full) {
			snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
		}
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
		return;
	}
	
	// things that alway block, unrelated to the mob/ability
	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_START_LOCATION) || ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_BARRIER)) {
		msg_to_char(ch, "You can't summon anyone here.\r\n");
		return;
	}
	
	// count mobs and check limit
	count = 0;
	fol_count = 0;
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), mob, next_in_room) {
		if (IS_NPC(mob)) {
			++count;
			
			if (!GET_COMPANION(mob) && GET_LEADER(mob) == ch) {
				++fol_count;
			}
		}
	}
	if (count >= max_npcs) {
		msg_to_char(ch, "There are too many NPCs here to summon more.\r\n");
		return;
	}
	if (fol_count >= max_followers) {
		msg_to_char(ch, "You have too many npcs folowers already.\r\n");
		return;
	}
	
	// lookup
	found = FALSE;
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		abil = plab->ptr;
		if (!VALID_SUMMON_ABIL(ch, plab)) {
			continue;
		}
		
		// try it
		if (IS_SET(ABIL_TYPES(abil), ABILT_SUMMON_ANY)) {
			// summon by mob name
			LL_FOREACH(ABIL_DATA(abil), adl) {
				if (adl->type != ADL_SUMMON_MOB || !(proto = mob_proto(adl->vnum))) {
					continue;	// no match
				}
				if (!multi_isname(arg, GET_PC_NAME(proto))) {
					continue;	// no string match
				}
				
				// ok!
				perform_ability_command(ch, abil, arg);
				found = TRUE;
				break;
			}
		}
		else if (IS_SET(ABIL_TYPES(abil), ABILT_SUMMON_RANDOM)) {
			// summon-random by ability name (minus the word summon)
			if (!multi_isname(arg, format_summon_name(ABIL_NAME(abil)))) {
				continue;	// no name match
			}
			
			// ok!
			perform_ability_command(ch, abil, arg);
			found = TRUE;
			break;
		}
		
		if (found) {
			break;	// only 1 successful match
		}
	}
	
	if (!found) {
		msg_to_char(ch, "You don't know how to summon %s '%s'.\r\n", AN(arg), arg);
		return;
	}
}
