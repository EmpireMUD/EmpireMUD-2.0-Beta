/* ************************************************************************
*   File: handler.c                                       EmpireMUD 2.0b5 *
*  Usage: internal funcs: moving and finding chars/objs                   *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <limits.h>
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
#include "dg_scripts.h"
#include "vnums.h"

/**
* Contents:
*   Affect Handlers
*   Character Handlers
*   Character Location Handlers
*   Character Targeting Handlers
*   Coin Handlers
*   Cooldown Handlers
*   Empire Handlers
*   Empire Targeting Handlers
*   Follow Handlers
*   Group Handlers
*   Help Handlers
*   Interaction Handlers
*   Lore Handlers
*   Mob Tagging Handlers
*   Mount Handlers
*   Object Handlers
*   Object Binding Handlers
*   Object Location Handlers
*   Custom Message Handlers
*   Object Targeting Handlers
*   Offer Handlers
*   Requirement Handlers
*   Resource Depletion Handlers
*   Room Handlers
*   Room Extra Handlers
*   Room Targeting Handlers
*   Sector Handlers
*   Storage Handlers
*   Targeting Handlers
*   Unique Storage Handlers
*   Vehicle Handlers
*   Vehicle Targeting Handlers
*   World Handlers
*   Miscellaneous Handlers
*/

// externs
extern const char *affect_wear_off_msgs[];
extern const int confused_dirs[NUM_2D_DIRS][2][NUM_OF_DIRS];
extern const char *drinks[];
extern int get_north_for_char(char_data *ch);
extern struct complex_room_data *init_complex_data();
const struct wear_data_type wear_data[NUM_WEARS];

// external funcs
void adjust_building_tech(empire_data *emp, room_data *room, bool add);
void check_delayed_load(char_data *ch);
void extract_trigger(trig_data *trig);
void scale_item_to_level(obj_data *obj, int level);

// locals
static void add_obj_binding(int idnum, struct obj_binding **list);
void remove_lore_record(char_data *ch, struct lore_data *lore);

// local file scope variables
static int extractions_pending = 0;


 //////////////////////////////////////////////////////////////////////////////
//// AFFECT HANDLERS /////////////////////////////////////////////////////////

/**
* Call affect_remove with every af of "type"
*
* @param char_data *ch The person to remove affects from.
* @param int type Any ATYPE_ const
* @param bool show_msg If TRUE, will show the wears-off message.
*/
void affect_from_char(char_data *ch, int type, bool show_msg) {
	struct over_time_effect_type *dot, *next_dot;
	struct affected_type *hjp, *next;
	bool shown = FALSE;

	for (hjp = ch->affected; hjp; hjp = next) {
		next = hjp->next;
		if (hjp->type == type) {
			if (show_msg && !shown) {
				show_wear_off_msg(ch, type);
				shown = TRUE;
			}
			affect_remove(ch, hjp);
		}
	}
	
	// over-time effects
	for (dot = ch->over_time_effects; dot; dot = next_dot) {
		next_dot = dot->next;
		if (dot->type == type) {
			if (show_msg && !shown) {
				show_wear_off_msg(ch, type);
				shown = TRUE;
			}
			dot_remove(ch, dot);
		}
	}
}


/**
* Calls affect_remove on every affect of type "type" with location "apply".
*
* @param char_data *ch The person to remove affects from.
* @param int type Any ATYPE_ const to match.
* @param int apply Any APPLY_ const to match.
* @param bool show_msg If TRUE, will show the wears-off message.
*/
void affect_from_char_by_apply(char_data *ch, int type, int apply, bool show_msg) {
	struct affected_type *aff, *next_aff;
	bool shown = FALSE;

	for (aff = ch->affected; aff; aff = next_aff) {
		next_aff = aff->next;
		if (aff->type == type && aff->location == apply) {
			if (show_msg && !shown) {
				show_wear_off_msg(ch, type);
				shown = TRUE;
			}
			affect_remove(ch, aff);
		}
	}
}


/**
* Calls affect_remove on every affect of type "type" that sets AFF flag "bits".
*
* @param char_data *ch The person to remove affects from.
* @param int type Any ATYPE_ const to match.
* @param bitvector_t bits Any AFF_ bit(s) to match.
* @param bool show_msg If TRUE, will show the wears-off message.
*/
void affect_from_char_by_bitvector(char_data *ch, int type, bitvector_t bits, bool show_msg) {
	struct affected_type *aff, *next_aff;
	bool shown = FALSE;

	for (aff = ch->affected; aff; aff = next_aff) {
		next_aff = aff->next;
		if (aff->type == type && IS_SET(aff->bitvector, bits)) {
			if (show_msg && !shown) {
				show_wear_off_msg(ch, type);
				shown = TRUE;
			}
			affect_remove(ch, aff);
		}
	}
}


/**
* Calls affect_remove on every affect of type "type" with location "apply".
*
* @param char_data *ch The person to remove affects from.
* @param int type Any ATYPE_ const to match.
* @param char_data *caster The person whose affects to remove.
* @param bool show_msg If TRUE, will send the wears-off message.
*/
void affect_from_char_by_caster(char_data *ch, int type, char_data *caster, bool show_msg) {
	struct affected_type *aff, *next_aff;
	bool shown = FALSE;
	
	LL_FOREACH_SAFE(ch->affected, aff, next_aff) {
		if (aff->type == type && aff->cast_by == CAST_BY_ID(caster)) {
			if (show_msg && !shown) {
				show_wear_off_msg(ch, type);
				shown = TRUE;
			}
			
			affect_remove(ch, aff);
		}
	}
}


/**
* Removes all affects that cause a given AFF flag, plus all other affects
* caused by the same thing (e.g. if something gives +1 Strength and FLY, then
* calling this function with AFF_FLY will remove both parts).
*
* @param char_data *ch The person to remove from.
* @param bitvector_t aff_flag Any AFF_ flags to remove.
* @param bool show_msg If TRUE, will show the wears-off message.
*/
void affects_from_char_by_aff_flag(char_data *ch, bitvector_t aff_flag, bool show_msg) {
	struct affected_type *af, *next_af;
	
	for (af = ch->affected; af; af = next_af) {
		next_af = af->next;
		if (IS_SET(af->bitvector, aff_flag)) {
			// calling it this way removes ALL affects of that ability
			affect_from_char(ch, af->type, show_msg);
		}
	}
}


/**
* Call affect_remove_room to remove all effects of "type"
*
* @param room_data *room The location to remove affects from.
* @param int type Any ATYPE_ const
*/
void affect_from_room(room_data *room, int type) {
	struct affected_type *hjp, *next;

	for (hjp = ROOM_AFFECTS(room); hjp; hjp = next) {
		next = hjp->next;
		if (hjp->type == type) {
			affect_remove_room(room, hjp);
		}
	}
}


/**
* Calls affect_remove_room on every affect of type "type" that sets AFF flag
* "bits".
*
* @param room_data *rom The room to remove affects from.
* @param int type Any ATYPE_ const to match.
* @param bitvector_t bits Any AFF_ bit(s) to match.
* @param bool show_msg If TRUE, shows the wear-off message.
*/
void affect_from_room_by_bitvector(room_data *room, int type, bitvector_t bits, bool show_msg) {
	struct affected_type *aff, *next_aff;
	bool shown = FALSE;
	
	LL_FOREACH_SAFE(ROOM_AFFECTS(room), aff, next_aff) {
		if (aff->type == type && IS_SET(aff->bitvector, bits)) {
			if (show_msg && !shown) {
				if (*affect_wear_off_msgs[aff->type] && ROOM_PEOPLE(room)) {
					act(affect_wear_off_msgs[aff->type], FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
				}
				shown = TRUE;
			}
			affect_remove_room(room, aff);
		}
	}
}


/**
* Applies an effect to a character, replacing the version that used 4 arguments
* instead of bitvectors. You cannot set both add & average; it prefers add if
* you do.
*
* @param char_data *ch The character to attach to
* @param struct affected_type *af The affect to attach
* @param int flags ADD_ or AVG_ + DURATION or MODIFIER (bitvectors)
*/
void affect_join(char_data *ch, struct affected_type *af, int flags) {
	struct affected_type *af_iter;
	bool found = FALSE;
	
	for (af_iter = ch->affected; af_iter && !found; af_iter = af_iter->next) {
		if (af_iter->type == af->type && af_iter->location == af->location) {
			if (IS_SET(flags, ADD_DURATION)) {
				af->duration += af_iter->duration;
			}
			else if (IS_SET(flags, AVG_DURATION)) {
				af->duration = (af->duration + af_iter->duration) / 2;
			}

			if (IS_SET(flags, ADD_MODIFIER)) {
				af->modifier += af_iter->modifier;
			}
			if (IS_SET(flags, AVG_MODIFIER)) {
				af->modifier = (af->modifier + af_iter->modifier) / 2;
			}

			affect_remove(ch, af_iter);
			affect_to_char(ch, af);
			found = TRUE;
			break;
		}
	}
	
	if (!found) {
		affect_to_char(ch, af);
	}
	
	// affect_to_char seems to duplicate af so we must free it
	free(af);
}


/**
* This is the core function used by various affects and equipment to apply or
* remove things to a player.
*
* This is where APPLY_ consts are mapped to their effects.
*
* @param char_data *ch The person to apply to
* @param byte loc APPLY_ const
* @param sh_int mod The modifier amount for the apply
* @param bitvector_t bitv AFF_ bits
* @param bool add if TRUE, applies this effect; if FALSE, removes it
*/
void affect_modify(char_data *ch, byte loc, sh_int mod, bitvector_t bitv, bool add) {
	empire_data *emp = GET_LOYALTY(ch);
	// int diff, orig;
	
	if (add) {
		SET_BIT(AFF_FLAGS(ch), bitv);
	}
	else {
		REMOVE_BIT(AFF_FLAGS(ch), bitv);
		mod = -mod;
	}
	
	// APPLY_x:
	switch (loc) {
		case APPLY_NONE:
			break;
		case APPLY_INVENTORY: {
			SAFE_ADD(GET_BONUS_INVENTORY(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_STRENGTH:
			SAFE_ADD(GET_STRENGTH(ch), mod, SHRT_MIN, SHRT_MAX, TRUE);
			break;
		case APPLY_DEXTERITY:
			SAFE_ADD(GET_DEXTERITY(ch), mod, SHRT_MIN, SHRT_MAX, TRUE);
			break;
		case APPLY_CHARISMA:
			SAFE_ADD(GET_CHARISMA(ch), mod, SHRT_MIN, SHRT_MAX, TRUE);
			break;
		case APPLY_GREATNESS:
			// only update greatness if ch is in a room (playing)
			if (!IS_NPC(ch) && emp && IN_ROOM(ch)) {
				EMPIRE_GREATNESS(emp) += mod;
			}
			SAFE_ADD(GET_GREATNESS(ch), mod, SHRT_MIN, SHRT_MAX, TRUE);
			break;
		case APPLY_INTELLIGENCE:
			SAFE_ADD(GET_INTELLIGENCE(ch), mod, SHRT_MIN, SHRT_MAX, TRUE);
			break;
		case APPLY_WITS:
			SAFE_ADD(GET_WITS(ch), mod, SHRT_MIN, SHRT_MAX, TRUE);
			break;
		case APPLY_AGE:
			SAFE_ADD(ch->player.time.birth, -(mod * SECS_PER_MUD_YEAR), LONG_MIN, LONG_MAX, TRUE);
			break;
		case APPLY_MOVE:
			SAFE_ADD(GET_MAX_MOVE(ch), mod, INT_MIN, INT_MAX, TRUE);
			
			// prevent from going negative
			//orig = GET_MOVE(ch);
			SAFE_ADD(GET_MOVE(ch), mod, INT_MIN, INT_MAX, TRUE);
			/*
			if (!IS_NPC(ch)) {
				if (GET_MOVE(ch) < 0) {
					GET_MOVE_DEFICIT(ch) -= GET_MOVE(ch);
					GET_MOVE(ch) = 0;
				}
				else if (GET_MOVE_DEFICIT(ch) > 0) {
					diff = MIN(GET_MOVE_DEFICIT(ch), MAX(0, GET_MOVE(ch) - orig));
					GET_MOVE_DEFICIT(ch) -= diff;
					GET_MOVE(ch) -= diff;
				}
			}
			*/
			break;
		case APPLY_HEALTH:
			SAFE_ADD(GET_MAX_HEALTH(ch), mod, INT_MIN, INT_MAX, TRUE);
			
			// prevent from going negative
			//orig = GET_HEALTH(ch);
			SAFE_ADD(GET_HEALTH(ch), mod, INT_MIN, INT_MAX, TRUE);
			GET_HEALTH(ch) = MAX(1, GET_HEALTH(ch));
			/*
			if (!IS_NPC(ch)) {
				if (GET_HEALTH(ch) < 1) {	// min 1 on health
					GET_HEALTH_DEFICIT(ch) -= (GET_HEALTH(ch)-1);
					GET_HEALTH(ch) = 1;
				}
				else if (GET_HEALTH_DEFICIT(ch) > 0) {
					diff = MIN(GET_HEALTH_DEFICIT(ch), MAX(0, GET_HEALTH(ch) - orig));
					diff = MIN(diff, GET_HEALTH(ch)-1);
					GET_HEALTH_DEFICIT(ch) -= diff;
					GET_HEALTH(ch) -= diff;
				}
			}
			else {
				// npcs cannot die this way
				GET_HEALTH(ch) = MAX(1, GET_HEALTH(ch));
			}
			*/
			break;
		case APPLY_MANA:
			SAFE_ADD(GET_MAX_MANA(ch), mod, INT_MIN, INT_MAX, TRUE);
			
			// prevent from going negative
			//orig = GET_MANA(ch);
			SAFE_ADD(GET_MANA(ch), mod, INT_MIN, INT_MAX, TRUE);
			/*
			if (!IS_NPC(ch)) {
				if (GET_MANA(ch) < 0) {
					GET_MANA_DEFICIT(ch) -= GET_MANA(ch);
					GET_MANA(ch) = 0;
				}
				else if (GET_MANA_DEFICIT(ch) > 0) {
					diff = MIN(GET_MANA_DEFICIT(ch), MAX(0, GET_MANA(ch) - orig));
					GET_MANA_DEFICIT(ch) -= diff;
					GET_MANA(ch) -= diff;
				}
			}
			*/
			break;
		case APPLY_BLOOD: {
			SAFE_ADD(GET_EXTRA_BLOOD(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_RESIST_PHYSICAL: {
			SAFE_ADD(GET_RESIST_PHYSICAL(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_RESIST_MAGICAL: {
			SAFE_ADD(GET_RESIST_MAGICAL(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_TO_HIT: {
			SAFE_ADD(GET_TO_HIT(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_DODGE: {
			SAFE_ADD(GET_DODGE(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_BLOCK:
			SAFE_ADD(GET_BLOCK(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		case APPLY_HEALTH_REGEN: {
			SAFE_ADD(GET_HEALTH_REGEN(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_MOVE_REGEN: {
			SAFE_ADD(GET_MOVE_REGEN(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_MANA_REGEN: {
			SAFE_ADD(GET_MANA_REGEN(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_BONUS_PHYSICAL: {
			SAFE_ADD(GET_BONUS_PHYSICAL(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_BONUS_MAGICAL: {
			SAFE_ADD(GET_BONUS_MAGICAL(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_BONUS_HEALING: {
			SAFE_ADD(GET_BONUS_HEALING(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_HEAL_OVER_TIME: {
			SAFE_ADD(GET_HEAL_OVER_TIME(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_CRAFTING: {
			SAFE_ADD(GET_CRAFTING_BONUS(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_BLOOD_UPKEEP: {
			SAFE_ADD(GET_EXTRA_ATT(ch, ATT_BLOOD_UPKEEP), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		default:
			log("SYSERR: Unknown apply adjust %d attempt (%s, affect_modify).", loc, __FILE__);
			break;

	} /* switch */
}


/**
* Remove an affected_type structure from a char (called when duration
* reaches zero). Pointer *af must never be NIL!  Frees mem and calls
* affect_location_apply
*
* @param char_data *ch The person to remove the af from.
* @param struct affected_by *af The affect to remove.
*/
void affect_remove(char_data *ch, struct affected_type *af) {
	struct affected_type *temp;

	// not affected by it at all somehow?
	if (ch->affected == NULL) {
		return;
	}

	affect_modify(ch, af->location, af->modifier, af->bitvector, FALSE);
	REMOVE_FROM_LIST(af, ch->affected, next);
	free(af);
	affect_total(ch);
}


/**
* Remove an affected_type structure from a room (called when duration
* reaches zero). Pointer *af must never be NIL!  Frees mem and calls
* affect_location_apply
*
* @param room_data *room The room to remove the affect from.
* @param struct affected_by *af The affect to remove.
*/
void affect_remove_room(room_data *room, struct affected_type *af) {
	struct affected_type *temp;

	// no effects on the room?
	if (ROOM_AFFECTS(room) == NULL) {
		return;
	}

	REMOVE_BIT(ROOM_AFF_FLAGS(room), af->bitvector);
	// restore base flags, in case we removed one of them
	SET_BIT(ROOM_AFF_FLAGS(room), ROOM_BASE_FLAGS(room));

	REMOVE_FROM_LIST(af, ROOM_AFFECTS(room), next);
	free(af);
}


/**
* Insert an affect_type in a char_data structure
*  Automatically sets apropriate bits and apply's
*
* Caution: this duplicates af (because of how it used to load from the pfile)
*
* @param char_data *ch The person to add the affect to
* @param struct affected_type *af The affect to add.
*/
void affect_to_char(char_data *ch, struct affected_type *af) {
	struct affected_type *affected_alloc;

	CREATE(affected_alloc, struct affected_type, 1);

	*affected_alloc = *af;
	affected_alloc->next = ch->affected;
	ch->affected = affected_alloc;

	affect_modify(ch, af->location, af->modifier, af->bitvector, TRUE);
	affect_total(ch);
}


/**
* Add an affect to a room
*
* Caution: this duplicates the af
*
* @param room_ room The room to apply the af to
* @param struct affected_type *af The affect to apply
*/
void affect_to_room(room_data *room, struct affected_type *af) {
	struct affected_type *affected_alloc;

	CREATE(affected_alloc, struct affected_type, 1);

	*affected_alloc = *af;
	affected_alloc->next = ROOM_AFFECTS(room);
	ROOM_AFFECTS(room) = affected_alloc;

	SET_BIT(ROOM_AFF_FLAGS(room), af->bitvector);
}


/**
* This updates a character by subtracting everything he is affected by,
* restoring original abilities, and then affecting all again. This is called
* periodically to ensure that all effects are correctly applied and updated.
*
* @param char_data *ch The person whose effects to update
*/
void affect_total(char_data *ch) {
	extern const int base_player_pools[NUM_POOLS];

	struct affected_type *af;
	int i, iter, level;
	empire_data *emp = GET_LOYALTY(ch);
	struct obj_apply *apply;
	int health, move, mana;
	
	int pool_bonus_amount = config_get_int("pool_bonus_amount");
	
	// save these for later -- they shouldn't change during an affect_total
	health = GET_HEALTH(ch);
	move = GET_MOVE(ch);
	mana = GET_MANA(ch);
	level = get_approximate_level(ch);
	
	// only update greatness if ch is in a room (playing)
	if (!IS_NPC(ch) && emp && IN_ROOM(ch)) {
		EMPIRE_GREATNESS(emp) -= GET_GREATNESS(ch);
	}

	for (i = 0; i < NUM_WEARS; i++) {
		if (GET_EQ(ch, i) && wear_data[i].count_stats) {
			for (apply = GET_OBJ_APPLIES(GET_EQ(ch, i)); apply; apply = apply->next) {
				affect_modify(ch, apply->location, apply->modifier, NOBITS, FALSE);
			}
			if (GET_OBJ_AFF_FLAGS(GET_EQ(ch, i))) {
				affect_modify(ch, APPLY_NONE, 0, GET_OBJ_AFF_FLAGS(GET_EQ(ch, i)), FALSE);
			}
		}
	}
	
	// abilities
	if (!IS_NPC(ch)) {
	}

	// affects
	for (af = ch->affected; af; af = af->next)
		affect_modify(ch, af->location, af->modifier, af->bitvector, FALSE);

	// RESET!
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		ch->aff_attributes[iter] = ch->real_attributes[iter];
	}
	
	// Base energies do not change
	if (!IS_NPC(ch)) {
		GET_MAX_HEALTH(ch) = base_player_pools[HEALTH];
		GET_MAX_MOVE(ch) = base_player_pools[MOVE];
		GET_MAX_MANA(ch) = base_player_pools[MANA];
		
		if (GET_CLASS(ch)) {
			GET_MAX_HEALTH(ch) += MAX(0, GET_CLASS_PROGRESSION(ch) * (CLASS_POOL(GET_CLASS(ch), HEALTH) - base_player_pools[HEALTH]) / 100);
			GET_MAX_MOVE(ch) += MAX(0, GET_CLASS_PROGRESSION(ch) * (CLASS_POOL(GET_CLASS(ch), MOVE) - base_player_pools[MOVE]) / 100);
			GET_MAX_MANA(ch) += MAX(0, GET_CLASS_PROGRESSION(ch) * (CLASS_POOL(GET_CLASS(ch), MANA) - base_player_pools[MANA]) / 100);
		}
	}

	for (i = 0; i < NUM_WEARS; i++) {
		if (GET_EQ(ch, i) && wear_data[i].count_stats) {
			for (apply = GET_OBJ_APPLIES(GET_EQ(ch, i)); apply; apply = apply->next) {
				affect_modify(ch, apply->location, apply->modifier, NOBITS, TRUE);
			}
			if (GET_OBJ_AFF_FLAGS(GET_EQ(ch, i))) {
				affect_modify(ch, APPLY_NONE, 0, GET_OBJ_AFF_FLAGS(GET_EQ(ch, i)), TRUE);
			}
		}
	}

	for (af = ch->affected; af; af = af->next) {
		affect_modify(ch, af->location, af->modifier, af->bitvector, TRUE);
	}
	
	if (HAS_BONUS_TRAIT(ch, BONUS_HEALTH)) {
		GET_MAX_HEALTH(ch) += pool_bonus_amount * (1 + (level / 25));
	}
	if (HAS_BONUS_TRAIT(ch, BONUS_MOVES)) {
		GET_MAX_MOVE(ch) += pool_bonus_amount * (1 + (level / 25));
	}
	if (HAS_BONUS_TRAIT(ch, BONUS_MANA)) {
		GET_MAX_MANA(ch) += pool_bonus_amount * (1 + (level / 25));
	}

	// ability-based modifiers
	if (!IS_NPC(ch)) {
		if (has_ability(ch, ABIL_ENDURANCE)) {
			GET_MAX_HEALTH(ch) = MIN(GET_MAX_HEALTH(ch), 1000) * 2.0 + MAX(GET_MAX_HEALTH(ch) - 1000, 0) * 1.25;
		}
		if (has_ability(ch, ABIL_GIFT_OF_NATURE)) {
			GET_MAX_MANA(ch) *= 1.35;
		}
		if (has_ability(ch, ABIL_ARCANE_POWER)) {
			GET_MAX_MANA(ch) *= 1.35;
		}
	}

	/* Make sure maximums are considered */
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		GET_ATT(ch, iter) = MAX(0, MIN(GET_ATT(ch, iter), att_max(ch)));
	}
	
	// only update greatness if ch is in a room (playing)
	if (!IS_NPC(ch) && emp && IN_ROOM(ch)) {
		EMPIRE_GREATNESS(emp) += GET_GREATNESS(ch);
	}
	
	// limit this
	GET_MAX_HEALTH(ch) = MAX(1, GET_MAX_HEALTH(ch));
	
	// restore these because in some cases, they mess up during an affect_total
	GET_HEALTH(ch) = health;
	GET_MOVE(ch) = move;
	GET_MANA(ch) = mana;
	
	// check for inventory size
	if (!IS_NPC(ch) && CAN_CARRY_N(ch) > GET_LARGEST_INVENTORY(ch)) {
		GET_LARGEST_INVENTORY(ch) = CAN_CARRY_N(ch);
	}
	
	// this is to prevent weird quirks because GET_MAX_BLOOD is a function
	GET_MAX_POOL(ch, BLOOD) = GET_MAX_BLOOD(ch);
}


/**
* @param char_data *ch The person to check
* @param int type Any ATYPE_ const
* @return bool TRUE if ch is affected by anything with matching type
*/
bool affected_by_spell(char_data *ch, int type) {
	struct over_time_effect_type *dot;
	struct affected_type *hjp;
	bool found = FALSE;

	for (hjp = ch->affected; hjp && !found; hjp = hjp->next) {
		if (hjp->type == type) {
			found = TRUE;
		}
	}
	
	// dot effects
	for (dot = ch->over_time_effects; dot && !found; dot = dot->next) {
		if (dot->type == type) {
			found = TRUE;
		}
	}

	return found;
}


/**
* Matches both an ATYPE_ and an APPLY_ on an effect.
*
* @param char_data *ch The character to check
* @param int type the ATYPE_ flag
* @param int apply the APPLY_ flag
* @return bool TRUE if an effect matches both conditions
*/
bool affected_by_spell_and_apply(char_data *ch, int type, int apply) {
	struct affected_type *hjp;
	bool found = FALSE;

	for (hjp = ch->affected; hjp && !found; hjp = hjp->next) {
		if (hjp->type == type && hjp->location == apply) {
			found = TRUE;
		}
	}

	return found;
}


/**
* Create an affect that modifies a trait.
*
* @param int type ATYPE_
* @param int duration in 5-second ticks
* @param int location APPLY_
* @param int modifier +/- amount
* @param bitvector_t bitvector AFF_
* @param char_data *cast_by The caster who made the effect (may be NULL; use the person themselves for penalty effects as those won't cleanse).
* @return struct affected_type* The created af
*/
struct affected_type *create_aff(int type, int duration, int location, int modifier, bitvector_t bitvector, char_data *cast_by) {
	struct affected_type *af;
	
	CREATE(af, struct affected_type, 1);
	af->type = type;
	af->cast_by = cast_by ? CAST_BY_ID(cast_by) : 0;
	af->duration = duration;
	af->modifier = modifier;
	af->location = location;
	af->bitvector = bitvector;
			
	return af;
}


/**
* @param char_data *ch Person receiving the DoT.
* @param sh_int type ATYPE_ spell that caused it.
* @param sh_int duration Affect time, in 5-second intervals.
* @param sh_int damage_type DAM_ type.
* @param sh_int damage How much damage to do per 5-seconds.
* @param sh_int max_stack Number of times this can stack when re-applied before it expires.
* @param sh_int char_data *cast_by The caster.
*/
void apply_dot_effect(char_data *ch, sh_int type, sh_int duration, sh_int damage_type, sh_int damage, sh_int max_stack, char_data *cast_by) {
	struct over_time_effect_type *iter, *dot;
	bool found = FALSE;
	int id = (cast_by ? CAST_BY_ID(cast_by) : 0);
	
	// first see if they already have one
	for (iter = ch->over_time_effects; iter && !found; iter = iter->next) {
		if (iter->type == type && iter->cast_by == id && iter->damage_type == damage_type && iter->damage == damage) {
			// refresh effect
			iter->duration = MAX(iter->duration, duration);
			if (iter->stack < MIN(iter->max_stack, max_stack)) {
				++iter->stack;
			}
			found = TRUE;
		}
	}
	
	if (!found) {
		CREATE(dot, struct over_time_effect_type, 1);
		dot->next = ch->over_time_effects;
		ch->over_time_effects = dot;
		
		dot->type = type;
		dot->cast_by = id;
		dot->duration = duration;
		dot->damage_type = damage_type;
		dot->damage = damage;
		dot->stack = 1;
		dot->max_stack = max_stack;
	}
}


/**
* Remove a Damage-Over-Time effect from a character.
*
* @param char_data *ch The person to remove the DoT from.
* @param struct over_time_effect_type *dot The DoT to remove.
*/
void dot_remove(char_data *ch, struct over_time_effect_type *dot) {
	struct over_time_effect_type *temp;

	REMOVE_FROM_LIST(dot, ch->over_time_effects, next);
	free(dot);
}


/**
* @param room_data *room The room to check
* @param int type Any ATYPE_ const
* @return bool TRUE if the room is affected by the spell
*/
bool room_affected_by_spell(room_data *room, int type) {
	struct affected_type *hjp;
	bool found = FALSE;

	for (hjp = ROOM_AFFECTS(room); hjp && !found; hjp = hjp->next) {
		if (hjp->type == type) {
			found = TRUE;
		}
	}

	return found;
}


/**
* Shows the affect-wear-off message for a given type.
*
* @param char_data *ch The person wearing off of.
* @param int atype The ATYPE_ affect type.
*/
void show_wear_off_msg(char_data *ch, int atype) {
	if (*affect_wear_off_msgs[atype] && ch->desc) {
		msg_to_char(ch, "&%c%s&0\r\n", (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_STATUS)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_STATUS) : '0', affect_wear_off_msgs[atype]);
	}
}


//////////////////////////////////////////////////////////////////////////////
//// CHARACTER HANDLERS /////////////////////////////////////////////////////

/* Extract a ch completely from the world, and leave his stuff behind */
void extract_char_final(char_data *ch) {
	void die_follower(char_data *ch);
	ACMD(do_return);

	empire_data *rescan_emp = IS_NPC(ch) ? NULL : GET_LOYALTY(ch);
	char_data *k, *temp;
	descriptor_data *t_desc;
	obj_data *obj;
	int i;
	bool freed = FALSE;
	struct pursuit_data *purs;
	
	// sanitation checks
	if (!IN_ROOM(ch)) {
		log("SYSERR: Extracting char %s not in any room. (%s, extract_char)", GET_NAME(ch), __FILE__);
		exit(1);
	}

	// things checked for both pcs and npcs
	if (ch->followers || ch->master) {
		die_follower(ch);
	}

	/* Check to see if we are grouped! */
	if (GROUP(ch)) {
		leave_group(ch);
	}
	
	if (ch->desc) {
		/* Forget snooping, if applicable */
		if (ch->desc->snooping) {
			ch->desc->snooping->snoop_by = NULL;
			ch->desc->snooping = NULL;
		}
		if (ch->desc->snoop_by) {
			SEND_TO_Q("Your victim is no longer among us.\r\n", ch->desc->snoop_by);
			ch->desc->snoop_by->snooping = NULL;
			ch->desc->snoop_by = NULL;
		}
	}
	if (GET_FED_ON_BY(ch)) {
		GET_FEEDING_FROM(GET_FED_ON_BY(ch)) = NULL;
		GET_FED_ON_BY(ch) = NULL;
	}
	if (GET_FEEDING_FROM(ch)) {
		GET_FED_ON_BY(GET_FEEDING_FROM(ch)) = NULL;
		GET_FEEDING_FROM(ch) = NULL;
	}
	if (GET_LEADING_MOB(ch)) {
		GET_LED_BY(GET_LEADING_MOB(ch)) = NULL;
		GET_LEADING_MOB(ch) = NULL;
	}
	if (GET_LED_BY(ch)) {
		GET_LEADING_MOB(GET_LED_BY(ch)) = NULL;
		GET_LED_BY(ch) = NULL;
	}
	if (GET_LEADING_VEHICLE(ch)) {
		VEH_LED_BY(GET_LEADING_VEHICLE(ch)) = NULL;
		GET_LEADING_VEHICLE(ch) = NULL;
	}
	if (GET_SITTING_ON(ch)) {
		unseat_char_from_vehicle(ch);
	}
	if (GET_DRIVING(ch)) {
		VEH_DRIVER(GET_DRIVING(ch)) = NULL;
		GET_DRIVING(ch) = NULL;
	}
	
	// npc-only frees	
	if (IS_NPC(ch)) {
		// free up pursuit
		if (MOB_PURSUIT(ch)) {
			while ((purs = MOB_PURSUIT(ch))) {
				MOB_PURSUIT(ch) = purs->next;
				free(purs);
			}
			MOB_PURSUIT(ch) = NULL;
		}
			
		// alert empire data the mob is despawned
		if (GET_EMPIRE_NPC_DATA(ch)) {
			GET_EMPIRE_NPC_DATA(ch)->mob = NULL;
			GET_EMPIRE_NPC_DATA(ch) = NULL;
		}
	}

	// pc-only frees
	if (!IS_NPC(ch)) {
		// find if someone was switched into this person, and return them
		if (!ch->desc) {
			for (t_desc = descriptor_list; t_desc; t_desc = t_desc->next) {
				if (t_desc->original == ch) {
					do_return(t_desc->character, NULL, 0, 0);
				}
			}
		}
	}

	/* transfer objects to room, if any */
	while ((obj = ch->carrying)) {
		obj_from_char(obj);
		obj_to_room(obj, IN_ROOM(ch));
	}

	/* transfer equipment to room, if any */
	for (i = 0; i < NUM_WEARS; i++) {
		if (GET_EQ(ch, i)) {
			unequip_char_to_room(ch, i);
		}
	}

	// cancel fighting both directions
	if (FIGHTING(ch)) {
		stop_fighting(ch);
	}
	for (k = combat_list; k; k = temp) {
		temp = k->next_fighting;
		if (FIGHTING(k) == ch) {
			stop_fighting(k);
		}
	}

	// remove from the room
	char_from_room(ch);

	// if this was a switched player, return them back
	if (ch->desc && ch->desc->original) {
		do_return(ch, NULL, 0, 0);
	}

	if (IS_NPC(ch)) {
		if (SCRIPT(ch)) 
			extract_script(ch, MOB_TRIGGER);

		if (SCRIPT_MEM(ch))
			extract_script_mem(SCRIPT_MEM(ch));
		
		free_char(ch);
		freed = TRUE;
	}
	
	// close the desc
	if (!freed && ch->desc) {
		ch->desc->character = NULL;
		STATE(ch->desc) = CON_CLOSE;
		ch->desc = NULL;
	}

	/* if a player gets purged from within the game */
	if (!freed) {
		free_char(ch);
	}
	
	// update empire numbers -- only if we detected empire membership back at the beginning
	// this prevents incorrect greatness or other traits on logout
	if (rescan_emp) {
		read_empire_members(rescan_emp, FALSE);
	}
}


/**
* Imported this from tbaMUD (later CircleMUD) for delayed extraction to make
* lots of code cleaner and safer -- no longer removing people in the middle of
* things.
*
* @param char_data *ch The character to mark for extraction.
*/
void extract_char(char_data *ch) {
	void despawn_charmies(char_data *ch);
	
	if (ch == dg_owner_mob) {
		dg_owner_purged = 1;
		dg_owner_mob = NULL;
	}
	
	if (!EXTRACTED(ch)) {
		if (IS_NPC(ch)) {
			SET_BIT(MOB_FLAGS(ch), MOB_EXTRACTED);
			
			if (MOB_INSTANCE_ID(ch) != NOTHING) {
				subtract_instance_mob(real_instance(MOB_INSTANCE_ID(ch)), GET_MOB_VNUM(ch));
			}
		}
		else {
			SET_BIT(PLR_FLAGS(ch), PLR_EXTRACTED);
		}
		++extractions_pending;
	}

	
	// get rid of friends now (extracts them as well)
	despawn_charmies(ch);
}


/* I'm not particularly pleased with the MOB/PLR hoops that have to be jumped
 * through but it hardly calls for a completely new variable. Ideally it would
 * be its own list, but that would change the '->next' pointer, potentially
 * confusing some code. -gg This doesn't handle recursive extractions. */
void extract_pending_chars(void) {
	char_data *vict, *next_vict, *prev_vict;

	if (extractions_pending < 0) {
		log("SYSERR: Negative (%d) extractions pending.", extractions_pending);
	}

	for (vict = character_list, prev_vict = NULL; vict && extractions_pending > 0; vict = next_vict) {
		next_vict = vict->next;

		if (MOB_FLAGGED(vict, MOB_EXTRACTED)) {
			REMOVE_BIT(MOB_FLAGS(vict), MOB_EXTRACTED);
		}
		else if (!IS_NPC(vict) && PLR_FLAGGED(vict, PLR_EXTRACTED)) {
			REMOVE_BIT(PLR_FLAGS(vict), PLR_EXTRACTED);
		}
		else {
			/* Last non-free'd character to continue chain from. */
			prev_vict = vict;
			continue;
		}

		if (prev_vict) {
			prev_vict->next = next_vict;
		}
		else {
			character_list = next_vict;
		}

		// moving this down below the prev_vict block because ch was still in
		// the character list late in the process, causing a crash in some rare
		// cases -pc 1/14/2015
		extract_char_final(vict);
		--extractions_pending;
	}

	if (extractions_pending != 0) {
		if (extractions_pending > 0) {
			log("SYSERR: Couldn't find %d extractions as counted.", extractions_pending);
		}
		
		// likely an error -- search for people who need extracting (for next time)
		extractions_pending = 0;
		for (vict = character_list; vict; vict = vict->next) {
			if (EXTRACTED(vict)) {
				++extractions_pending;
			}
		}
	}
	else {
		extractions_pending = 0;
	}
}


/**
* Determines if a string matches a character name, based on things like
* "is the target disguised" and "can ch see through that disguise?"
*
* @param char_data *ch Optiona: The character who is looking for someone. (may be NULL If nobody is looking)
* @param char_data *target The potential match.
* @param char *name The string ch typed when looking for target.
* @param bitvector_t flags MATCH_GLOBAL, MATCH_IN_ROOM
* @return bool TRUE if "name" is valid for "target" according to "ch", FALSE if not
*/
bool match_char_name(char_data *ch, char_data *target, char *name, bitvector_t flags) {
	bool recognize, old_ignore_dark = Global_ignore_dark;
	
	if (IS_SET(flags, MATCH_GLOBAL)) {
		Global_ignore_dark = TRUE;
	}
	
	// visibility (shortcuts)
	if (ch && IS_SET(flags, MATCH_IN_ROOM) && AFF_FLAGGED(target, AFF_HIDE | AFF_NO_SEE_IN_ROOM) && !CAN_SEE(ch, target)) {
		Global_ignore_dark = old_ignore_dark;
		return FALSE;	// hidden
	}
	
	// done with this:
	Global_ignore_dark = old_ignore_dark;
	
	// recognize part: things that let you recognize
	recognize = IS_SET(flags, MATCH_GLOBAL) || (ch ? CAN_RECOGNIZE(ch, target) : TRUE);
	
	// name-matching part
	if (recognize && isname(name, GET_PC_NAME(target))) {
		return TRUE;	// name/kw match
	}
	else if (recognize && !IS_NPC(target) && GET_LASTNAME(target) && isname(name, GET_LASTNAME(target))) {
		return TRUE;	// lastname match
	}
	else if (IS_MORPHED(target) && isname(name, MORPH_KEYWORDS(GET_MORPH(target)))) {
		return TRUE;	// morph kw match
	}
	else if (IS_DISGUISED(target) && isname(name, GET_DISGUISED_NAME(target))) {
		return TRUE;	// disguise name match
	}
	else {
		// nope
		return FALSE;
	}
}


/**
* Handles the actual extract of an idle character.
* 
* @param char_data *ch The player to idle out.
*/
void perform_idle_out(char_data *ch) {
	extern obj_data *player_death(char_data *ch);
	
	empire_data *emp = NULL;
	bool died = FALSE;
	
	if (!ch) {
		return;
	}
	
	// block idle-out entirely with this prf
	if (ch->desc && PRF_FLAGGED(ch, PRF_NO_IDLE_OUT)) {
		return;
	}
	
	emp = GET_LOYALTY(ch);
	
	if (ch->desc) {
		STATE(ch->desc) = CON_DISCONNECT;
		ch->desc->character = NULL;
		ch->desc = NULL;
	}

	if (GET_POS(ch) < POS_STUNNED) {
		msg_to_char(ch, "You shuffle off this mortal coil, and die...\r\n");
		act("$n shuffles off $s mortal coil and dies.", FALSE, ch, NULL, NULL, TO_ROOM);
		player_death(ch);
		died = TRUE;
	}
	else {
		act("$n is idle too long, and vanishes.", TRUE, ch, NULL, NULL, TO_ROOM);
	}
	
	save_char(ch, died ? NULL : IN_ROOM(ch));
	
	syslog(SYS_LOGIN, GET_INVIS_LEV(ch), TRUE, "%s force-rented and extracted (idle).", GET_NAME(ch));
	extract_all_items(ch);
	extract_char(ch);
	
	if (emp) {
		extract_pending_chars();	// ensure char is gone
		read_empire_members(emp, FALSE);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER LOCATION HANDLERS /////////////////////////////////////////////


/**
* move a player out of a room
*
* @param char_data *ch The character to remove
*/
void char_from_room(char_data *ch) {
	char_data *temp;
	obj_data *obj;
	int pos;

	if (ch == NULL || !IN_ROOM(ch)) {
		log("SYSERR: NULL character or no location in %s, char_from_room", __FILE__);
		exit(1);
	}
	
	if (FIGHTING(ch) != NULL) {
		stop_fighting(ch);
	}

	// update lights
	for (pos = 0; pos < NUM_WEARS; pos++) {
		if (GET_EQ(ch, pos) && OBJ_FLAGGED(GET_EQ(ch, pos), OBJ_LIGHT)) {
			ROOM_LIGHTS(IN_ROOM(ch))--;
		}
	}
	for (obj = ch->carrying; obj; obj = obj->next_content) {
		if (OBJ_FLAGGED(obj, OBJ_LIGHT)) {
			ROOM_LIGHTS(IN_ROOM(ch))--;
		}
	}

	REMOVE_FROM_LIST(ch, ROOM_PEOPLE(IN_ROOM(ch)), next_in_room);
	IN_ROOM(ch) = NULL;
	ch->next_in_room = NULL;
}


/**
* place a character in a room, or move them from one room to another
*
* @param char_data *ch The person to place
* @param room_data *room The place to put 'em
*/
void char_to_room(char_data *ch, room_data *room) {
	extern int determine_best_scale_level(char_data *ch, bool check_group);
	extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom);
	extern int lock_instance_level(room_data *room, int level);
	void msdp_update_room(char_data *ch);
	void spawn_mobs_from_center(room_data *center);
	
	int pos;
	obj_data *obj;
	struct instance_data *inst;

	if (!ch || !room) {
		log("SYSERR: Illegal value(s) passed to char_to_room. (Room :%p, Ch: %p)", room, ch);
	}
	else {
		// sanitation: remove them from the old room first
		if (IN_ROOM(ch)) {
			char_from_room(ch);
		}

		ch->next_in_room = ROOM_PEOPLE(room);
		ROOM_PEOPLE(room) = ch;
		IN_ROOM(ch) = room;

		// update lights
		for (pos = 0; pos < NUM_WEARS; pos++) {
			if (GET_EQ(ch, pos) && OBJ_FLAGGED(GET_EQ(ch, pos), OBJ_LIGHT)) {
				ROOM_LIGHTS(room)++;
			}
		}
		for (obj = ch->carrying; obj; obj = obj->next_content) {
			if (OBJ_FLAGGED(obj, OBJ_LIGHT)) {
				ROOM_LIGHTS(room)++;
			}
		}

		// check npc spawns whenever a player is places in a room
		if (!IS_NPC(ch)) {
			spawn_mobs_from_center(IN_ROOM(ch));
		}
		
		// look for an instance to lock
		if (!IS_NPC(ch) && IS_ADVENTURE_ROOM(room) && (inst = find_instance_by_room(room, FALSE))) {
			if (ADVENTURE_FLAGGED(inst->adventure, ADV_LOCK_LEVEL_ON_ENTER) && !IS_IMMORTAL(ch)) {
				lock_instance_level(room, determine_best_scale_level(ch, TRUE));
			}
		}
		
		// store last room to player
		if (!IS_NPC(ch)) {
			GET_LAST_ROOM(ch) = GET_ROOM_VNUM(room);
		}
		
		// update location
		msdp_update_room(ch);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER TARGETING HANDLERS ////////////////////////////////////////////


/**
* Finds the closest visible character to ch. This checks in-room visibility
* as it is used to find VISIBLE characters (even though they are likely not
* in the same room).
*
* @param char_data *ch The finder.
* @param char *arg The argument/name.
* @param bool pc_only Whether to exclude NPCs.
* @return char_data *The nearest matching character.
*/
char_data *find_closest_char(char_data *ch, char *arg, bool pc_only) {
	char_data *vict, *best = NULL;
	int dist, best_dist = MAP_SIZE;
	
	if ((vict = get_char_room_vis(ch, arg)) && (!pc_only || !IS_NPC(vict))) {
		return vict;
	}
	
	LL_FOREACH(character_list, vict) {
		if (pc_only && IS_NPC(vict)) {
			continue;
		}
		if (!CAN_SEE(ch, vict) || !CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(vict))) {
			continue;
		}
		if (!match_char_name(ch, vict, arg, MATCH_IN_ROOM)) {
			continue;
		}
		
		dist = compute_distance(IN_ROOM(ch), IN_ROOM(vict));
		if (!best || dist < best_dist) {
			best_dist = dist;
			best = vict;
		}
	}
	
	return best;
}


/**
* Returns the first copy of a mob with a matching vnum.
*
* @param room_data *room where to look
* @param mob_vnum vnum what to find
* @return char_data *the mob, or NULL if none
*/
char_data *find_mob_in_room_by_vnum(room_data *room, mob_vnum vnum) {
	char_data *mob, *found = FALSE;
	
	for (mob = ROOM_PEOPLE(room); mob && !found; mob = mob->next_in_room) {
		if (IS_NPC(mob) && GET_MOB_VNUM(mob) == vnum) {
			found = mob;
		}
	}
	
	return found;
}


/**
* Checks if any mortal is present, and returns the first one if so.
*
* @param room_data *room The room to check.
* @return char_data* The mortal found, or NULL if none.
*/
char_data *find_mortal_in_room(room_data *room) {
	char_data *iter;
	
	LL_FOREACH2(ROOM_PEOPLE(room), iter, next_in_room) {
		if (!IS_NPC(iter) && !IS_IMMORTAL(iter) && !IS_GOD(iter)) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* search a room for a char, and return a pointer if found..
* This has no source character, so does not rely on visibility.
*
* @param char *name The keyword/name to look for.
* @param room_data *room Where to look.
* @return char_data *The matching character, if any.
*/
char_data *get_char_room(char *name, room_data *room) {
	char_data *i, *found = NULL;
	int j = 0, number;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname;

	strcpy(tmp, name);
	if (!(number = get_number(&tmp)))
		return (NULL);

	for (i = ROOM_PEOPLE(room); i && (j <= number) && !found; i = i->next_in_room) {
		if (match_char_name(NULL, i, tmp, MATCH_IN_ROOM)) {
			if (++j == number) {
				found = i;
			}
		}
	}

	return found;
}


/**
* Find a character in the room.
*
* @param char_data *ch The person who's looking.
* @param char *name The target argument.
* @return char_data *A matching character in the room, or NULL.
*/
char_data *get_char_room_vis(char_data *ch, char *name) {
	char_data *i, *found = NULL;
	int j = 0, number;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname;

	/* JE 7/18/94 :-) :-) */
	if (!str_cmp(name, "self") || !str_cmp(name, "me"))
		return (ch);

	/* 0.<name> means PC with name */
	strcpy(tmp, name);
	if ((number = get_number(&tmp)) == 0) {
		return get_player_vis(ch, tmp, FIND_CHAR_ROOM);
	}

	for (i = ROOM_PEOPLE(IN_ROOM(ch)); i && j <= number && !found; i = i->next_in_room) {
		if (CAN_SEE(ch, i) && WIZHIDE_OK(ch, i) && !AFF_FLAGGED(i, AFF_NO_TARGET_IN_ROOM) && match_char_name(ch, i, tmp, MATCH_IN_ROOM)) {
			if (++j == number) {
				found = i;
			}
		}
	}

	return found;
}


/**
* Find a character (pc or npc) using FIND_x locations.
*
* @param char_data *ch The person who's looking.
* @param char *name The target argument.
* @param bitvector_t where Any FIND_x flags.
* @return char_data *The found character, or NULL.
*/
char_data *get_char_vis(char_data *ch, char *name, bitvector_t where) {
	char_data *i, *found = NULL;
	int j = 0, number;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname;

	/* check the room first */
	if (IS_SET(where, FIND_CHAR_ROOM))
		return get_char_room_vis(ch, name);
	else if (IS_SET(where, FIND_CHAR_WORLD)) {
		if ((i = get_char_room_vis(ch, name)) != NULL) {
			return (i);
		}

		strcpy(tmp, name);
		if ((number = get_number(&tmp)) == 0) {
			return get_player_vis(ch, tmp, where);
		}

		for (i = character_list; i && (j <= number) && !found; i = i->next) {
			if (IS_SET(where, FIND_NPC_ONLY) && !IS_NPC(i)) {	
				continue;
			}
			if (!match_char_name(ch, i, tmp, (IS_SET(where, FIND_NO_DARK) ? MATCH_GLOBAL : 0))) {
				continue;
			}
			
			// found
			if (++j == number) {
				found = i;
			}
		}
	}

	return found;
}


/**
* Finds a player.
*
* @param char_data *ch The person looking.
* @param char *name The argument string.
* @param bitvector_t flags FIND_x flags.
* @return char_data *The found player, or NULL.
*/
char_data *get_player_vis(char_data *ch, char *name, bitvector_t flags) {
	char_data *i, *found = NULL;

	for (i = character_list; i && !found; i = i->next) {
		if (IS_NPC(i))
			continue;
		if (IS_SET(flags, FIND_CHAR_ROOM) && !WIZHIDE_OK(ch, i)) {
			continue;
		}
		if (IS_SET(flags, FIND_CHAR_ROOM) && IN_ROOM(i) != IN_ROOM(ch))
			continue;
		if (IS_SET(flags, FIND_CHAR_ROOM) && AFF_FLAGGED(i, AFF_NO_TARGET_IN_ROOM))
			continue;
		if (!match_char_name(ch, i, name, (IS_SET(flags, FIND_CHAR_ROOM) ? MATCH_IN_ROOM : 0) | (IS_SET(flags, FIND_NO_DARK | FIND_CHAR_WORLD) ? MATCH_GLOBAL : 0))) {
			continue;
		}
		
		found = i;
	}

	return found;
}


/**
* Searches for a character in the whole world with a matching name, without
* requiring visibility.
*
* @param char *name The name of the target.
* @return char_data *the found character, or NULL
*/
char_data *get_char_world(char *name) {
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	int number, pos = 0;
	bool pc_only = FALSE;
	char_data *ch, *found = NULL;
	
	strcpy(tmp, name);
	if ((number = get_number(&tmp)) == 0) {
		pc_only = TRUE;
	}

	for (ch = character_list; ch && (pos <= number) && !found; ch = ch->next) {
		if ((!IS_NPC(ch) || !pc_only) && match_char_name(NULL, ch, tmp, MATCH_GLOBAL)) {
			if (++pos == number || pc_only) {	// pc_only messes up pos
				found = ch;
			}
		}
	}

	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// COIN HANDLERS ///////////////////////////////////////////////////////////

/**
* This determines if a player has enough coins to afford a cost in a certain
* coinage, converting from the types he has.
*
* @param char_data *ch The player.
* @param empire_data *type Empire who is charging the player (or OTHER_COIN for any).
* @param int amount How much the player needs to afford.
* @return bool TRUE if the player can afford it, otherwise FALSE.
*/
bool can_afford_coins(char_data *ch, empire_data *type, int amount) {
	struct coin_data *coin;
	int local;
	
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	// try the native type first
	if ((coin = find_coin_entry(GET_PLAYER_COINS(ch), type))) {
		amount -= coin->amount;
	}
	
	// then try the "other"
	if (amount > 0 && type != REAL_OTHER_COIN && (coin = find_coin_entry(GET_PLAYER_COINS(ch), REAL_OTHER_COIN))) {
		local = exchange_coin_value(coin->amount, REAL_OTHER_COIN, type);
		amount -= local;
	}
	
	for (coin = GET_PLAYER_COINS(ch); coin && amount > 0; coin = coin->next) {
		if (coin->empire_id != OTHER_COIN && (type == REAL_OTHER_COIN || coin->empire_id != EMPIRE_VNUM(type))) {
			local = exchange_coin_value(coin->amount, real_empire(coin->empire_id), type);
			amount -= local;
		}
	}

	// did we find enough?
	return (amount <= 0);
}


/**
* Function to charge a player coins of a specific type, converting from other
* types as necessary. This does NOT guarantee the player had enough to begin
* with (see can_afford_coins).
*
* @param char_data *ch The player.
* @param empire_data *type Empire who is charging the player (or OTHER_COIN for any coin type).
* @param int amount How much to charge the player -- must be positive.
* @param struct resource_data **build_used_list Optional: if not NULL, will build a resource list of the specifc coin types charged.
*/
void charge_coins(char_data *ch, empire_data *type, int amount, struct resource_data **build_used_list) {
	struct coin_data *coin;
	int this, this_amount;
	double rate, inv;
	empire_data *emp;
	
	if (IS_NPC(ch) || amount <= 0) {
		return;
	}
	
	// try the native type first
	if ((coin = find_coin_entry(GET_PLAYER_COINS(ch), type))) {
		this = MIN(coin->amount, amount);
		decrease_coins(ch, type, this);
		amount -= this;
		
		if (build_used_list) {
			add_to_resource_list(build_used_list, RES_COINS, type ? EMPIRE_VNUM(type) : OTHER_COIN, this, 0);
		}
	}
		
	// then try the "other"
	if (amount > 0 && type != REAL_OTHER_COIN && (coin = find_coin_entry(GET_PLAYER_COINS(ch), REAL_OTHER_COIN))) {
		// inverse exchange rate to figure out how much we owe
		inv = 1.0 / (rate = exchange_rate(REAL_OTHER_COIN, type));
		this_amount = round(amount * inv);
		this = MIN(coin->amount, this_amount);
		decrease_coins(ch, REAL_OTHER_COIN, this);
		// we know it was at least one -- prevent never-hits-zero errors
		amount -= MAX(1, round(this * rate));
		
		if (build_used_list) {
			add_to_resource_list(build_used_list, RES_COINS, OTHER_COIN, this, 0);
		}
	}
	
	for (coin = GET_PLAYER_COINS(ch); coin && amount > 0; coin = coin->next) {
		if ((type == REAL_OTHER_COIN || coin->empire_id != EMPIRE_VNUM(type)) && coin->empire_id != OTHER_COIN) {
			emp = real_empire(coin->empire_id);
			// inverse exchange rate to figure out how much we owe
			inv = 1.0 / (rate = exchange_rate(emp, type));
			this_amount = round(amount * inv);
			this = MIN(coin->amount, this_amount);
			decrease_coins(ch, emp, this);
			// we know it was at least one -- prevent never-hits-zero errors
			amount -= MAX(1, round(this * rate));
			
			if (build_used_list) {
				add_to_resource_list(build_used_list, RES_COINS, emp ? EMPIRE_VNUM(emp) : OTHER_COIN, this, 0);
			}
		}
	}

	// coins were cleaned up with each increase
}


/**
* Calls cleanup_all_coins on all players in-game and connected.
*/
void cleanup_all_coins(void) {
	char_data *ch;
	descriptor_data *desc;
	
	for (ch = character_list; ch; ch = ch->next) {
		if (!IS_NPC(ch)) {
			cleanup_coins(ch);
		}
	}
		
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (STATE(desc) != CON_PLAYING && (ch = desc->character) && !IS_NPC(ch)) {
			cleanup_coins(ch);
		}
	}
}


/**
* Removes zero-entries and reduces extra coins to the OTHER category.
*/
void cleanup_coins(char_data *ch) {
	struct coin_data *iter, *least, *next_iter, *temp;
	int count, add_other, least_amount;
	
	if (IS_NPC(ch)) {
		return;
	}

	add_other = 0;
	
	do {
		count = 0;
		least = NULL;
		least_amount = 0;
		
		for (iter = GET_PLAYER_COINS(ch); iter; iter = next_iter) {
			next_iter = iter->next;
		
			if (iter->amount == 0) {
				// delete zeroes
				REMOVE_FROM_LIST(iter, GET_PLAYER_COINS(ch), next);
				free(iter);
			}
			else if (iter->empire_id != OTHER_COIN && real_empire(iter->empire_id) == NULL) {
				// no more empire
				add_other += iter->amount;
				REMOVE_FROM_LIST(iter, GET_PLAYER_COINS(ch), next);
				free(iter);
			}
			else {
				// still good
				++count;
				
				if (iter->empire_id != OTHER_COIN && (iter->amount < least_amount || least == NULL)) {
					least = iter;
					least_amount = iter->amount;
				}
			}
		}
		
		// remove one -- keep it simple
		if (count > MAX_COIN_TYPES && least) {
			add_other += least->amount;
			REMOVE_FROM_LIST(least, GET_PLAYER_COINS(ch), next);
			free(least);
		}
	} while (count > MAX_COIN_TYPES && MAX_COIN_TYPES > 1);
	// safety on that while in case there's only one type of coin allowed (which could repeatedly trigger the loop)
	
	// and repay them
	if (add_other > 0) {
		increase_coins(ch, REAL_OTHER_COIN, add_other);
	}
}


/**
* Converts a coin list into a player-readable string.
*
* @param struct coin_data *list The coins list.
* @param char *storage A buffer to store to.
*/
void coin_string(struct coin_data *list, char *storage) {
	char local[MAX_STRING_LENGTH], temp[MAX_STRING_LENGTH];
	struct coin_data *iter, *other = NULL;
	
	if (!list) {
		strcpy(storage, "no coins");
		return;
	}
	
	*local = '\0';
	
	for (iter = list; iter; iter = iter->next) {
		if (iter->empire_id == OTHER_COIN) {
			// save
			other = iter;
		}
		else {
			sprintf(temp, "%s%s", (*local ? ", " : ""), money_amount(real_empire(iter->empire_id), iter->amount));
			if (strlen(local) + strlen(temp) + 3 > MAX_STRING_LENGTH) {
				// safety
				strcat(local, "...");
				break;
			}
			else {
				strcat(local, temp);
			}
		}
	}
	
	if (other) {
		sprintf(temp, "%s%d %scoin%s", (*local ? ", and " : ""), other->amount, (*local ? "other " : ""), (other->amount != 1 ? "s" : ""));
		if (strlen(local) + strlen(temp) > MAX_STRING_LENGTH) {
			// safety
			strcat(local, "...");
		}
		else {
			strcat(local, temp);
		}
	}
	
	// save to the buffer
	strcpy(storage, local);
}


/**
* @param empire_data *type The empire who minted the coins, or OTHER_COIN.
* @param int amount How many coins.
*/
obj_data *create_money(empire_data *type, int amount) {
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
	obj_data *obj;

	if (amount <= 0) {
		log("SYSERR: Try to create negative or 0 money. (%d)", amount);
		return (NULL);
	}
	obj = create_obj();

	strcpy(buf, money_desc(type, amount));
	
	// strings
	GET_OBJ_KEYWORDS(obj) = str_dup(skip_filler(buf));
	GET_OBJ_SHORT_DESC(obj) = str_dup(buf);
	snprintf(buf2, sizeof(buf2), "%s is lying here.", CAP(buf));
	GET_OBJ_LONG_DESC(obj) = str_dup(buf2);

	// description
	if (amount == 1) {
		GET_OBJ_ACTION_DESC(obj) = str_dup("It's just one miserable little coin.");
	}
	else {
		if (amount < 10)
			snprintf(buf, sizeof(buf), "There are %d coins.", amount);
		else if (amount < 100)
			snprintf(buf, sizeof(buf), "There are about %d coins.", 10 * (amount / 10));
		else if (amount < 1000)
			snprintf(buf, sizeof(buf), "It looks to be about %d coins.", 100 * (amount / 100));
		else if (amount < 100000)
			snprintf(buf, sizeof(buf), "You guess there are, maybe, %d coins.", 1000 * ((amount / 1000) + number(0, (amount / 1000))));
		else
			strcpy(buf, "There are a LOT of coins.");	/* strcpy: OK (is < MAX_STRING_LENGTH) */

		GET_OBJ_ACTION_DESC(obj) = str_dup(buf);
	}

	// data
	GET_OBJ_TYPE(obj) = ITEM_COINS;
	GET_OBJ_MATERIAL(obj) = MAT_GOLD;
	SET_BIT(GET_OBJ_WEAR(obj), ITEM_WEAR_TAKE);

	GET_OBJ_VAL(obj, VAL_COINS_AMOUNT) = amount;
	GET_OBJ_VAL(obj, VAL_COINS_EMPIRE_ID) = (!type ? OTHER_COIN : EMPIRE_VNUM(type));

	return (obj);
}


/**
* Gets the value of a currency when converted to another empire.
*
* @param double amount How much money.
* @param empire_data *convert_from Empire whose currency it was.
* @param empire_data *convert_to The empire to exchange to.
* @return double The new value of the money.
*/
double exchange_coin_value(double amount, empire_data *convert_from, empire_data *convert_to) {
	return (amount * exchange_rate(convert_from, convert_to));
}


/**
* Gets the exchange rate for one currency to another.
*
* @param empire_data *from Empire whose currency you have.
* @param empire_data *to The empire to exchange to.
* @return double The exchange rate (multiplier for the amount of coins).
*/
double exchange_rate(empire_data *from, empire_data *to) {
	struct empire_political_data *pol = NULL;
	double modifier = 1.0;
	
	if (to == from) {
		// self
		modifier = 1.0;
	}
	else if (!to) {
		// no to?
		modifier = 1.0;
	}
	else if (!from) {
		// no from
		modifier = 0.5;
	}
	else if (!(pol = find_relation(to, from))) {
		// no relations
		modifier = 0.5;
	}
	else if (IS_SET(pol->type, DIPL_ALLIED)) {
		modifier = 0.75;
	}
	else if (IS_SET(pol->type, DIPL_NONAGGR)) {
		modifier = 0.65;
	}
	else if (IS_SET(pol->type, DIPL_WAR)) {
		modifier = 0.1;
	}
	else if (IS_SET(pol->type, DIPL_DISTRUST)) {
		modifier = 0.25;
	}
	else {
		// other relations: half value
		modifier = 0.5;
	}
	
	// trade bonus
	if (pol && IS_SET(pol->type, DIPL_TRADE)) {
		modifier += 0.25;
	}

	modifier = MAX(0.1, MIN(1.0, modifier));
	return modifier;
}


/**
* Determines a coin argument and returns a pointer to the rest of the argument.
* This is considered to have found a string if the returned pointer is > input,
* or *amount_found > 0.
*
* This parses: <number> [empire] coin[s]
*
* @param char *input The input string.
* @param empire_data **emp_found A place to store the found empire id, if any.
* @param int *amount_found The numerical argument.
* @param bool assume_coins If TRUE, the word "coins" can be omitted.
* @return char* A pointer to the remaining argument (or the full argument, if no coins).
*/
char *find_coin_arg(char *input, empire_data **emp_found, int *amount_found, bool assume_coins) {
	char arg[MAX_INPUT_LENGTH];
	char *pos, *final;
	int amt;
	
	// clear immediately
	*emp_found = NULL;
	*amount_found = 0;
	
	// quick check: prevent work
	if (!assume_coins && !strstr(input, "coin")) {
		return input;
	}
	
	// check for numeric arg
	pos = one_argument(input, arg);
	if (!isdigit(*arg)) {
		// nope
		return input;
	}
	
	// we found a numeric arg.. save it in case it works out.
	amt = atoi(arg);
	
	// check for empire arg OR coins here
	pos = any_one_word(pos, arg);
	
	if (!str_cmp(arg, "coin") || !str_cmp(arg, "coins")) {
		// no empire arg but we're done
		*amount_found = amt;
		*emp_found = REAL_OTHER_COIN;
		return pos;
	}
	
	if (!(*emp_found = get_empire_by_name(arg))) {
		if (is_abbrev(arg, "other") || is_abbrev(arg, "miscellaneous") || is_abbrev(arg, "simple")) {
			*emp_found = REAL_OTHER_COIN;
		}
		else {
			// no match -- no success
			return input;
		}
	}
	
	// still here? then they provided number and empire; check that the next arg is coins
	final = one_argument(pos, arg);
	if (!str_cmp(arg, "coin") || !str_cmp(arg, "coins")) {
		// success!
		*amount_found = amt;
		skip_spaces(&final);
		return final;
	}
	else if (assume_coins) {
		skip_spaces(&pos);
		*amount_found = amt;
		return pos;
	}
	
	// oops... if we got here, there was no match
	return input;
}


/**
* This function finds a coin entry for an empire.
*
* @param struct coin_data *list The list to search.
* @param empire_data *emp The empire whose coinage to find, or OTHER_COIN for the "others".
* @return struct coin_data* The found entry, or NULL if none.
*/
struct coin_data *find_coin_entry(struct coin_data *list, empire_data *emp) {
	struct coin_data *iter;
	
	for (iter = list; iter; iter = iter->next) {
		if ((!emp && iter->empire_id == OTHER_COIN) || (emp && iter->empire_id == EMPIRE_VNUM(emp))) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* Main interface for giving/taking coins.
*
* @param char_data *ch The player to add coins to.
* @param empire_data *emp The empire who minted the coins, or OTHER_COIN.
* @param int amount How much to +/-.
* @return int The player's new value of that type of coins.
*/
int increase_coins(char_data *ch, empire_data *emp, int amount) {
	struct coin_data *coin;
	int curr, value = 0;
	
	if (IS_NPC(ch)) {
		return 0;
	}
	
	coin = find_coin_entry(GET_PLAYER_COINS(ch), emp);
	
	if (!coin && amount > 0) {
		CREATE(coin, struct coin_data, 1);
		coin->amount = 0;
		coin->empire_id = !emp ? OTHER_COIN : EMPIRE_VNUM(emp);
		coin->next = GET_PLAYER_COINS(ch);
		GET_PLAYER_COINS(ch) = coin;
	}

	// now, if we have a coin object, add the money
	if (coin) {
		coin->last_acquired = time(0);
		curr = coin->amount;
		
		if (amount < 0) {
			coin->amount = MAX(0, curr + amount);
			
			// validate to prevent overflow
			if (coin->amount > curr) {
				coin->amount = 0;
			}
		}
		else {
			coin->amount = MIN(MAX_COIN, coin->amount + amount);
			
			// validate to prevent overflow
			if (coin->amount < curr) {
				coin->amount = MAX_COIN;
			}
		}
		
		value = coin->amount;
	}
	
	cleanup_coins(ch);
	return value;
}


/**
* "25 empire coins" -- shorthand for reporting the amount/type of money in
* many common strings. Not to be confused with money_desc().
*
* @param empire_data *type Which empire's coins (or REAL_OTHER_COIN).
* @param int amount How many coins.
* @return const char* The coin string.
*/
const char *money_amount(empire_data *type, int amount) {
	static char desc[MAX_STRING_LENGTH];
	snprintf(desc, sizeof(desc), "%d %s coin%s", amount, (type != REAL_OTHER_COIN ? EMPIRE_ADJECTIVE(type) : (amount == 1 ? "simple" : "miscellaneous")), PLURAL(amount));
	return (const char*)desc;
}


/**
* "Short description" for an amount of money. For normal player reports, use
* money_amount() instead.
*
* @param empire_data *type Which empire's coins (or REAL_OTHER_COIN).
* @param int amount How many coins.
* @return const char* A short description describing that many coins.
*/
const char *money_desc(empire_data *type, int amount) {
	static char desc[MAX_STRING_LENGTH];
	char temp[MAX_STRING_LENGTH];
	int cnt;

	struct {
		int limit;
		const char *description;
	} money_table[] = {
		// %s is replaced with empire adjective
		{ 1, "a single %s coin" },
		{ 10, "a tiny pile of %s coins" },
		{ 20, "a handful of %s coins" },
		{ 75, "a little pile of %s coins" },
		{ 200, "a small pile of %s coins" },
		{ 1000, "a pile of %s coins" },
		{ 5000, "a big pile of %s coins" },
		{ 10000, "a large heap of %s coins" },
		{ 20000, "a huge mound of %s coins" },
		{ 75000, "an enormous mound of %s coins" },
		{ 150000, "a small mountain of %s coins" },
		{ 250000, "a mountain of %s coins" },
		{ 500000, "a huge mountain of %s coins" },
		{ 1000000, "an enormous mountain of %s coins" },
		
		// last
		{ 0, NULL }
	};

	if (amount <= 0) {
		log("SYSERR: Try to create negative or 0 money (%d).", amount);
		return (NULL);
	}

	*temp = '\0';
	for (cnt = 0; money_table[cnt].limit > 0; ++cnt) {
		if (amount <= money_table[cnt].limit) {
			strcpy(temp, money_table[cnt].description);
			break;
		}
	}
	if (!*temp) {
		strcpy(temp, "an absolutely colossal mountain of %s coins");
	}
	
	if (strstr(temp, "%s")) {
		sprintf(desc, temp, (type != REAL_OTHER_COIN ? EMPIRE_ADJECTIVE(type) : (amount == 1 ? "simple" : "miscellaneous")));
	}
	else {
		strcpy(desc, temp);
	}
	
	return (const char*)desc;
}


/**
* Returns the total amount of coin a player has, without regard to type.
*
* @param char_data *ch The player.
* @return int The total number of coins.
*/
int total_coins(char_data *ch) {
	struct coin_data *coin;
	int total = 0;
	
	if (IS_NPC(ch)) {
		return 0;
	}
	
	for (coin = GET_PLAYER_COINS(ch); coin; coin = coin->next) {
		SAFE_ADD(total, coin->amount, INT_MIN, INT_MAX, FALSE);
	}

	return total;
}


 //////////////////////////////////////////////////////////////////////////////
//// COOLDOWN HANDLERS ///////////////////////////////////////////////////////


/**
* Adds a cooldown to the character, only if the duration is positive. If the
* character somehow already has a cooldown of this type, the longer of the
* two durations is kept.
*
* @param char_data *ch The character.
* @param int type Any COOLDOWN_.
* @param int seconds_duration How long it lasts.
*/
void add_cooldown(char_data *ch, int type, int seconds_duration) {
	struct cooldown_data *cool;
	bool found = FALSE;
	
	// check for existing
	for (cool = ch->cooldowns; cool && !found; cool = cool->next) {
		if (cool->type == type) {
			found = TRUE;
			cool->expire_time = MAX(cool->expire_time, time(0) + seconds_duration);
		}
	}
	
	// add?
	if (!found && seconds_duration > 0) {
		CREATE(cool, struct cooldown_data, 1);
		cool->type = type;
		cool->expire_time = time(0) + seconds_duration;
		cool->next = ch->cooldowns;
		ch->cooldowns = cool;
	}
}


/**
* Returns the time left on a cooldown of a given type, or 0 if the character
* does not have that ability on cooldown.
*
* @param char_data *ch The character.
* @param int type Any COOLDOWN_.
* @return int The time remaining on the cooldown (in seconds), or 0.
*/
int get_cooldown_time(char_data *ch, int type) {
	struct cooldown_data *cool;
	int remain = 0;
	
	for (cool = ch->cooldowns; cool && remain <= 0; cool = cool->next) {
		if (cool->type == type) {
			remain = cool->expire_time - time(0);
		}
	}
	
	return MAX(0, remain);
}


/**
* Silently removes a cooldown from the character's list.
*
* @param char_data *ch The character.
* @param struct cooldown_data *cool The cooldown to remove.
*/
void remove_cooldown(char_data *ch, struct cooldown_data *cool) {
	struct cooldown_data *temp;
	
	REMOVE_FROM_LIST(cool, ch->cooldowns, next);
	free(cool);
}


/**
* Removes any cooldowns of a given type from the character.
*
* @param char_data *ch The character.
* @param int type Any COOLDOWN_.
*/
void remove_cooldown_by_type(char_data *ch, int type) {
	struct cooldown_data *cool, *next_cool;
	
	for (cool = ch->cooldowns; cool; cool = next_cool) {
		next_cool = cool->next;
		
		if (cool->type == type) {
			remove_cooldown(ch, cool);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE HANDLERS /////////////////////////////////////////////////////////

/**
* This function abandons any room and all its associated rooms.
*
* @param room_data *room The room to abandon.
*/
void abandon_room(room_data *room) {
	void clear_private_owner(int id);
	
	room_data *iter, *next_iter, *home = HOME_ROOM(room);
	
	if (ROOM_PRIVATE_OWNER(room) != NOBODY) {
		clear_private_owner(ROOM_PRIVATE_OWNER(room));
	}
	
	perform_abandon_room(room);
	
	// inside
	LL_FOREACH_SAFE2(interior_room_list, iter, next_iter, next_interior) {
		if (HOME_ROOM(iter) == home) {
			perform_abandon_room(iter);
		}
	}
}


/**
* This function claims any room and all its associated rooms.
*
* @param room_data *room The room to claim.
* @param empire_data *emp The empire to claim for.
*/
void claim_room(room_data *room, empire_data *emp) {
	room_data *home = HOME_ROOM(room);
	room_data *iter, *next_iter;
	
	if (!room || !emp || ROOM_OWNER(home)) {
		return;
	}
	
	perform_claim_room(home, emp);
	
	LL_FOREACH_SAFE2(interior_room_list, iter, next_iter, next_interior) {
		if (HOME_ROOM(iter) == home) {
			perform_claim_room(iter, emp);
		}
	}
}


// creates and returns a fresh political data between two empires
struct empire_political_data *create_relation(empire_data *a, empire_data *b) {
	struct empire_political_data *pol;
	
	CREATE(pol, struct empire_political_data, 1);
	pol->id = EMPIRE_VNUM(b);
	pol->start_time = time(0);
	pol->next = EMPIRE_DIPLOMACY(a);
	EMPIRE_DIPLOMACY(a) = pol;
	
	EMPIRE_NEEDS_SAVE(a) = TRUE;
	return pol;
}


/**
* Looks up an empire rank by typed argument (ignoring color codes) -- this
* may take a number or a rank name.
*
* @param empire_data *emp The empire whose rank we're looking up.
* @param char *name The typed-in arg.
* @return int The rank position (zero-based) or NOTHING if none.
*/
int find_rank_by_name(empire_data *emp, char *name) {
	int iter, found = NOTHING, num;
	
	if ((num = atoi(name)) > 0 && num <= EMPIRE_NUM_RANKS(emp)) {
		found = num - 1;
	}
	
	for (iter = 0; iter < EMPIRE_NUM_RANKS(emp) && found == NOTHING; ++iter) {
		if (is_abbrev(name, strip_color(EMPIRE_RANK(emp, iter)))) {
			found = iter;
		}
	}
	
	return found;
}


/* Find a relationship between two given empires */
struct empire_political_data *find_relation(empire_data *from, empire_data *to) {
	struct empire_political_data *emp_pol;

	if (!from || !to)
		return NULL;

	for (emp_pol = EMPIRE_DIPLOMACY(from); emp_pol && emp_pol->id != EMPIRE_VNUM(to); emp_pol = emp_pol->next);

	return emp_pol;
}


/**
* finds the empire territory_list entry for a room
*
* @param empire_data *emp The empire
* @param room_data *room The room to find
* @return struct empire_territory_data* the territory data, or NULL if not found
*/
struct empire_territory_data *find_territory_entry(empire_data *emp, room_data *room) {
	struct empire_territory_data *found = NULL, *ter_iter;
	
	if (emp) {
		for (ter_iter = EMPIRE_TERRITORY_LIST(emp); ter_iter && !found; ter_iter = ter_iter->next) {
			if (ter_iter->room == room) {
				found = ter_iter;
			}
		}
	}
	
	return found;
}


/**
* @param empire_data *emp The empire to search.
* @param int type TRADE_IMPORT or TRADE_EXPORT
* @param obj_vnum vnum What object to find in the trade list.
* @return struct empire_trade_data* The matching entry, or NULL if none.
*/
struct empire_trade_data *find_trade_entry(empire_data *emp, int type, obj_vnum vnum) {
	struct empire_trade_data *iter;
	
	for (iter = EMPIRE_TRADE(emp); iter; iter = iter->next) {
		if (iter->type == type && iter->vnum == vnum) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* Main interfacing for adding/removing coins from an empire. Adding coins will
* always add the local value of the currency. Subtracting coins will remove
* them directly, ignoring coin_empire.
*
* @param empire_data *emp_gaining The empire who is gaining/losing money.
* @param empire_data *coin_empire The empire who minted the coins, or OTHER_COIN.
* @param int amount How much to +/-.
* @return int The new coin total of the empire.
*/
int increase_empire_coins(empire_data *emp_gaining, empire_data *coin_empire, double amount) {
	double local;
	
	// who??
	if (!emp_gaining) {
		return 0;
	}
	
	if (amount < 0) {
		SAFE_ADD(EMPIRE_COINS(emp_gaining), amount, 0, MAX_COIN, FALSE);
	}
	else {
		if ((local = exchange_coin_value(amount, coin_empire, emp_gaining)) > 0) {
			SAFE_ADD(EMPIRE_COINS(emp_gaining), local, 0, MAX_COIN, FALSE);
		}
	}

	EMPIRE_NEEDS_SAVE(emp_gaining) = TRUE;
	return EMPIRE_COINS(emp_gaining);
}


/**
* Called by abandon_room().
*
* @param room_data *room The room to abandon.
*/
void perform_abandon_room(room_data *room) {
	void deactivate_workforce_room(empire_data *emp, room_data *room);
	void delete_territory_entry(empire_data *emp, struct empire_territory_data *ter);
	
	empire_data *emp = ROOM_OWNER(room);
	struct empire_territory_data *ter;
	bool junk;
	
	// updates based on owner
	if (emp) {
		deactivate_workforce_room(emp, room);
		adjust_building_tech(emp, room, FALSE);
		
		// update territory counts
		if (COUNTS_AS_TERRITORY(room)) {
			struct empire_island *eisle = get_empire_island(emp, GET_ISLAND_ID(room));
			if (is_in_city_for_empire(room, emp, FALSE, &junk)) {
				EMPIRE_CITY_TERRITORY(emp) -= 1;
				eisle->city_terr -= 1;
			}
			else {
				EMPIRE_OUTSIDE_TERRITORY(emp) -= 1;
				eisle->outside_terr -= 1;
			}
		}
		// territory list
		if (BELONGS_IN_TERRITORY_LIST(room) && (ter = find_territory_entry(emp, room))) {
			delete_territory_entry(emp, ter);
		}
		
		// quest tracker for members
		if (GET_BUILDING(room) && IS_COMPLETE(room)) {
			qt_empire_players(emp, qt_lose_building, GET_BLD_VNUM(GET_BUILDING(room)));
		}
	}
	
	ROOM_OWNER(room) = NULL;

	REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_PUBLIC | ROOM_AFF_NO_WORK);
	REMOVE_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_PUBLIC | ROOM_AFF_NO_WORK);

	if (ROOM_PRIVATE_OWNER(room) != NOBODY) {
		COMPLEX_DATA(room)->private_owner = NOBODY;
	}
	
	// if a city center is abandoned, destroy it
	if (IS_CITY_CENTER(room)) {
		disassociate_building(room);
	}
}


/**
* Called by claim_room() to do the actual work.
*
* @param room_data *room The room to claim.
* @param empire_data *emp The empire to claim for.
*/
void perform_claim_room(room_data *room, empire_data *emp) {
	extern struct empire_territory_data *create_territory_entry(empire_data *emp, room_data *room);
	
	struct empire_territory_data *ter;
	bool junk;
	
	ROOM_OWNER(room) = emp;
	remove_room_extra_data(room, ROOM_EXTRA_CEDED);	// not ceded if just claimed
	
	adjust_building_tech(emp, room, TRUE);
	
	// update territory counts
	if (COUNTS_AS_TERRITORY(room)) {
		struct empire_island *eisle = get_empire_island(emp, GET_ISLAND_ID(room));
		if (is_in_city_for_empire(room, emp, FALSE, &junk)) {
			EMPIRE_CITY_TERRITORY(emp) += 1;
			eisle->city_terr += 1;
		}
		else {
			EMPIRE_OUTSIDE_TERRITORY(emp) += 1;
			eisle->outside_terr += 1;
		}
	}
	// territory list
	if (BELONGS_IN_TERRITORY_LIST(room) && !(ter = find_territory_entry(emp, room))) {
		ter = create_territory_entry(emp, room);
	}
	
	if (GET_BUILDING(room) && IS_COMPLETE(room)) {
		qt_empire_players(emp, qt_gain_building, GET_BLD_VNUM(GET_BUILDING(room)));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE TARGETING HANDLERS ///////////////////////////////////////////////

/**
* @param empire_data *emp Which empire to check for cities in
* @param room_data *loc Location to check
* @return struct empire_city_data* Returns the closest city that loc is inside, or NULL if none
*/
struct empire_city_data *find_city(empire_data *emp, room_data *loc) {
	extern struct city_metadata_type city_type[];

	struct empire_city_data *city, *found = NULL;
	int dist, min = -1;

	if (!emp) {
		return FALSE;
	}
	
	for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
		if ((dist = compute_distance(loc, city->location)) <= city_type[city->type].radius) {
			if (!found || min == -1 || dist < min) {
				found = city;
				min = dist;
			}
		}
	}
	
	return found;
}


/**
* @param empire_data *emp which empire
* @param room_data *location map location
* @return struct empire_city_data* The city object, or NULL if there isn't one.
*/
struct empire_city_data *find_city_entry(empire_data *emp, room_data *location) {
	struct empire_city_data *city, *found = NULL;
	
	if (!emp) {
		return NULL;
	}
	
	for (city = EMPIRE_CITY_LIST(emp); city && !found; city = city->next) {
		if (city->location == location) {
			found = city;
		}
	}
	
	return found;
}


/**
* @param empire_data *emp which empire
* @param char *name user input
* @return struct empire_city_data* A pointer to the city if match found, NULL otherwise
*/
struct empire_city_data *find_city_by_name(empire_data *emp, char *name) {
	struct empire_city_data *city, *abbrev = NULL, *found = NULL;
	int num = -1, count;
	
	if (!emp) {
		return NULL;
	}
	
	if (isdigit(*name)) {
		num = atoi(name);
	}
	
	count = 0;
	for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
		if (!str_cmp(name, city->name) || ++count == num) {
			found = city;
			break;
		}
		else if (is_abbrev(name, city->name)) {
			abbrev = city;
		}
	}
	
	return found ? found : abbrev;
}


/**
* Finds the closest city WITHOUT regard to the city's radius, unlike
* find_city(), which only finds a city if it covers the location.
*
* @param empire_data *emp The empire you're checking for.
* @param room_data *loc The location to use.
* @return struct empire_city_data* A pointer to the empire's closest city, or NULL if they have none.
*/
struct empire_city_data *find_closest_city(empire_data *emp, room_data *loc) {
	struct empire_city_data *city, *found = NULL;
	int dist, min = -1;

	if (!emp) {
		return FALSE;
	}
	
	for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
		dist = compute_distance(loc, city->location);
		if (!found || min == -1 || dist < min) {
			found = city;
			min = dist;
		}
	}
	
	return found;
}


/**
* Finds an empire by name/number. This prefers exact matches and also checks
* the empire's adjective name.
*
* @param char *name The user input (name/number of empire)
* @return empire_data *The empire, or NULL if none found.
*/
empire_data *get_empire_by_name(char *name) {
	empire_data *pos, *next_pos, *full_exact, *full_abbrev, *adj_exact, *adj_abbrev;
	int num;
	
	// we'll take any of these if we don't find a perfect match
	full_exact = full_abbrev = adj_exact = adj_abbrev = NULL;

	if (is_number(name))
		num = atoi(name);
	else {
		num = 0;
	}

	// we break out early if we find a full exact match
	HASH_ITER(hh, empire_table, pos, next_pos) {
		if (!str_cmp(name, EMPIRE_NAME(pos)) || --num == 0) {
			full_exact = pos;
			break;	// just need this one
		}
		if (!full_abbrev && is_abbrev(name, EMPIRE_NAME(pos))) {
			full_abbrev = pos;
		}
		if (!adj_exact && !str_cmp(name, EMPIRE_ADJECTIVE(pos))) {
			adj_exact = pos;
		}
		if (!adj_abbrev && is_abbrev(name, EMPIRE_ADJECTIVE(pos))) {
			adj_abbrev = pos;
		}
	}

	// prefer full_exact > adj_exact > full_abbrev > adj_abbrev	
	return (full_exact ? full_exact : (adj_exact ? adj_exact : (full_abbrev ? full_abbrev : adj_abbrev)));
}


 //////////////////////////////////////////////////////////////////////////////
//// FOLLOW HANDLERS /////////////////////////////////////////////////////////


/*
 * Do NOT call this before having checked if a circle of followers
 * will arise. CH will follow leader
 *
 * @param char_data *ch The new follower.
 * @param char_data *leader The leader to follow.
 * @param bool msg If TRUE, sends starts-following messages.
 */
void add_follower(char_data *ch, char_data *leader, bool msg) {
	struct follow_type *k;

	if (ch->master || ch == leader)
		return;

	ch->master = leader;

	CREATE(k, struct follow_type, 1);

	k->follower = ch;
	k->next = leader->followers;
	leader->followers = k;

	if (msg) {
		act("You now follow $N.", FALSE, ch, 0, leader, TO_CHAR);
		if (CAN_SEE(leader, ch) && WIZHIDE_OK(leader, ch)) {
			act("$n starts following you.", TRUE, ch, 0, leader, TO_VICT);
		}
		act("$n starts to follow $N.", TRUE, ch, 0, leader, TO_NOTVICT);
	}
}


/**
* Called when a character that follows/is followed dies -- cancels both
* who he is following, and who is following him.
*
* @param char_data *ch The dying follower.
*/
void die_follower(char_data *ch) {
	struct follow_type *j, *k;

	if (ch->master) {
		stop_follower(ch);
	}

	for (k = ch->followers; k; k = j) {
		j = k->next;
		stop_follower(k->follower);
	}
}


/**
* Called when stop following persons, or stopping charm
* This will NOT do if a character quits/dies!!
*
* @param char_data *ch The character who will stop following
*/
void stop_follower(char_data *ch) {
	struct follow_type *j, *k;

	if (ch->master == NULL)
		return;

	act("You stop following $N.", FALSE, ch, 0, ch->master, TO_CHAR);
	act("$n stops following $N.", TRUE, ch, 0, ch->master, TO_NOTVICT);
	if (CAN_SEE(ch->master, ch) && WIZHIDE_OK(ch->master, ch)) {
		act("$n stops following you.", TRUE, ch, 0, ch->master, TO_VICT);
	}

	if (ch->master->followers->follower == ch) {	/* Head of follower-list? */
		k = ch->master->followers;
		ch->master->followers = k->next;
		free(k);
	}
	else {			/* locate follower who is not head of list */
		for (k = ch->master->followers; k->next->follower != ch; k = k->next);

		j = k->next;
		k->next = j->next;
		free(j);
	}

	ch->master = NULL;
	REMOVE_BIT(AFF_FLAGS(ch), AFF_CHARM);
}


 //////////////////////////////////////////////////////////////////////////////
//// GROUP HANDLERS ///////////////////////////////////////////////////////////

/**
* Count player members of a group.
*
* @param struct group_data *group The group to count.
*/
int count_group_members(struct group_data *group) {
	struct group_member_data *mem;
	int count = 0;
	
	for (mem = group->members; mem; mem = mem->next) {
		if (!IS_NPC(mem->member)) {
			++count;
		}
	}
	
	return count;
}


/**
* Create a new group for a person.
*
* @param char_data *leader The new group leader.
* @return struct group_data* The group.
*/
struct group_data *create_group(char_data *leader) {
	struct group_data *new_group, *tail;

	CREATE(new_group, struct group_data, 1);
	new_group->next = NULL;
	
	// add to end
	if ((tail = group_list)) {
		while (tail->next) {
			tail = tail->next;
		}
		tail->next = new_group;
	}
	else {
		group_list = new_group;
	}

	join_group(leader, new_group);
	return new_group;
}


/**
* Removes any remaining members and frees a group.
*
* @param struct group_data *group The group to free.
*/
void free_group(struct group_data *group) {
	struct group_member_data *mem;
	struct group_data *temp;
	
	// short remove-from-group
	while ((mem = group->members)) {
		group->members = mem->next;
		mem->member->group = NULL;
		free(mem);
	}

	REMOVE_FROM_LIST(group, group_list, next);
	free(group);
}

/**
* Determines if two chars are in the same group, or could be considered as
* such. This accounts for NPC followers, etc, and is better than doing a
* GROUP() == GROUP()
*
* @param char_data *ch One character.
* @param char_data *vict Another character.
* @return bool TRUE if they are effectively the same group.
*/
bool in_same_group(char_data *ch, char_data *vict) {
	char_data *use_ch, *use_vict;

	// use immediate leader if it's an npc
	use_ch = IS_NPC(ch) ? ch->master : ch;
	use_vict = IS_NPC(vict) ? vict->master : vict;

	if (use_ch && use_vict && !IS_NPC(use_ch) && !IS_NPC(use_vict) && GROUP(use_ch) && GROUP(use_ch) == GROUP(use_vict)) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}


/**
* Adds a player to a group.
*
* @param char_data *ch The player to add.
* @param struct group_data *group The group to add to.
*/
void join_group(char_data *ch, struct group_data *group) {
	struct group_member_data *mem, *tail;
	
	CREATE(mem, struct group_member_data, 1);
	mem->member = ch;
	mem->next = NULL;
	
	// add to tail
	if ((tail = group->members)) {
		while (tail->next) {
			tail = tail->next;
		}
		tail->next = mem;
	}
	else {
		group->members = mem;
	}

	ch->group = group;  
	if (!group->leader) {
		group->leader = ch;
	}

	if (group->leader == ch) {
		send_to_group(NULL, group, "%s becomes leader of the group.", GET_NAME(ch));
	}
	else {
		send_to_group(NULL, group, "%s joins the group.", GET_NAME(ch));
	}
}


/**
* Remove a player from their group, if any.
*
* @param char_data *ch The player to remove.
*/
void leave_group(char_data *ch) {
	struct group_member_data *mem, *next_mem, *temp;
	char_data *first_pc = NULL;
	struct group_data *group;

	if ((group = ch->group) == NULL)
		return;

	send_to_group(NULL, group, "%s has left the group.", GET_NAME(ch));
	
	// remove from member list
	for (mem = group->members; mem; mem = next_mem) {
		next_mem = mem->next;
		if (mem->member == ch) {
			REMOVE_FROM_LIST(mem, group->members, next);
			free(mem);
		}
		else if (!IS_NPC(mem->member) && !first_pc) {
			first_pc = mem->member;	// store for later
		}
	}

	ch->group = NULL;

	if (GROUP_LEADER(group) == ch && first_pc) {
		group->leader = first_pc;
		send_to_group(NULL, group, "%s has assumed leadership of the group.", GET_NAME(GROUP_LEADER(group)));
	}
	else if (count_group_members(group) < 1) {
		free_group(group);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// HELP HANDLERS ///////////////////////////////////////////////////////////

/**
* Look up help by name.
* int level -- level-restrict help results
* char *word -- The word to get help on
* returns help_index_element* or NULL
*/
struct help_index_element *find_help_entry(int level, const char *word) {
	extern struct help_index_element *help_table;
	extern int top_of_helpt;
	
	int chk, bot, top, mid, minlen;
	
	if (help_table) {
		bot = 0;
		top = top_of_helpt;
		minlen = strlen(word);

		for (;;) {
			mid = (bot + top) / 2;

			if (bot > top) {
				return NULL;
			}
			else if (!(chk = strn_cmp(word, help_table[mid].keyword, minlen))) {
				/* trace backwards to find first matching entry. Thanks Jeff Fink! */
				while ((mid > 0) && (!(chk = strn_cmp(word, help_table[mid - 1].keyword, minlen)))) {
					--mid;
				}
				// now iterate forward to find the first matching entry the player can use
				while (mid < top && help_table[mid].level > level && !(chk = strn_cmp(word, help_table[mid + 1].keyword, minlen))) {
					++mid;
				}
				
				if (level < help_table[mid].level) {
					return NULL;
				}
				else {
					return &help_table[mid];
				}
			}
			else {
				if (chk > 0) {
					bot = mid + 1;
				}
				else {
					top = mid - 1;
				}
			}
		}
	}
	else {
		return NULL;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// INTERACTION HANDLERS ////////////////////////////////////////////////////

/**
* Compares an interaction's chances against an exclusion set. Interactions are
* exclusive with other interactions that share an exclusion code, and the
* chance rolls on exclusion sets are cumulative.
*
* @param struct interact_exclusion_data **set A pointer to the hash table that holds the data (may add an entry).
* @param char code The exclusion code (must be a single letter; any empty or invalid code is treated as non-exclusive).
* @param double percent The percent (0.01-100.00) chance of passing.
* @return bool TRUE if the interaction succeeds
*/
bool check_exclusion_set(struct interact_exclusion_data **set, char code, double percent) {
	struct interact_exclusion_data *find;
	
	// limited set of codes
	if (!isalpha(code)) {
		code = 0;
	}
	
	// no exclusion? just roll
	if (!code) {
		return (number(1, 10000) <= (percent * 100));
	}
	
	// this 'find' is guaranteed because it makes it if it doesn't exist
	if ((find = find_exclusion_data(set, code))) {
		if (find->done) {
			// already excluded
			return FALSE;
		}
		
		// add to cumulative percent and check it
		find->cumulative_value += (percent * 100);
		if (find->rolled_value <= find->cumulative_value) {
			find->done = TRUE;
			return TRUE;
		}
		
		return FALSE;
	}
	
	// just in case
	return FALSE;
}


/**
* Finds (or creates) an exclusion entry for a given code. Exclusion codes must
* be single letters.
*
* @param struct interact_exclusion_data **set A pointer to the hash table that holds the data (may add an entry).
* @param char code The exclusion code (must be a single letter; any empty or invalid code is treated as non-exclusive).
* @return struct interact_exclusion_data* The exclusion data for that code.
*/
struct interact_exclusion_data *find_exclusion_data(struct interact_exclusion_data **set, char code) {
	struct interact_exclusion_data *find;
	int val;
	
	val = (int) code;
	HASH_FIND_INT(*set, &val, find);
	if (!find) {
		CREATE(find, struct interact_exclusion_data, 1);
		find->code = (int) code;
		// this rolls now -- rolls are for the entire exclusion set
		find->rolled_value = number(1, 10000);
		find->cumulative_value = 0;
		find->done = FALSE;
		HASH_ADD_INT(*set, code, find);
	}
	
	return find;
}


/**
* Free a set of exclusion data.
* 
* @param struct interact_exclusion_data *list The list to free.
*/
void free_exclusion_data(struct interact_exclusion_data *list) {
	struct interact_exclusion_data *iter, *next;
	HASH_ITER(hh, list, iter, next) {
		HASH_DEL(list, iter);
		free(iter);
	}
}


/**
* @param struct interaction_item *list The list to check.
* @param int type The INTERACT_ type to check for.
* @return bool TRUE if at least one interaction of that type is in the list.
*/
bool has_interaction(struct interaction_item *list, int type) {
	struct interaction_item *interact;
	bool found = FALSE;
	
	for (interact = list; interact && !found; interact = interact->next) {
		if (interact->type == type) {
			found = TRUE;
		}
	}
	
	return found;
}


/**
* Attempts to run global mob interactions -- interactions from the globals table.
*
* @param char_data *ch The player who is interacting.
* @param char_data *mob The NPC being interacted-with.
* @param int type Any INTERACT_ const.
* @param INTERACTION_FUNC(*func) A callback function to run for the interaction.
*/
bool run_global_mob_interactions(char_data *ch, char_data *mob, int type, INTERACTION_FUNC(*func)) {
	extern adv_data *get_adventure_for_vnum(rmt_vnum vnum);
	
	bool any = FALSE, done_cumulative = FALSE;
	struct global_data *glb, *next_glb, *choose_last;
	int cumulative_prc;
	adv_data *adv;
	
	// no work
	if (!ch || !mob || !IS_NPC(mob) || !func) {
		return FALSE;
	}
	
	adv = get_adventure_for_vnum(GET_MOB_VNUM(mob));
	cumulative_prc = number(1, 10000);
	choose_last = NULL;

	HASH_ITER(hh, globals_table, glb, next_glb) {
		if (GET_GLOBAL_TYPE(glb) != GLOBAL_MOB_INTERACTIONS) {
			continue;
		}
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT)) {
			continue;
		}
		if (GET_GLOBAL_ABILITY(glb) != NO_ABIL && !has_ability(ch, GET_GLOBAL_ABILITY(glb))) {
			continue;
		}
		
		// level limits
		if (GET_GLOBAL_MIN_LEVEL(glb) > 0 && GET_CURRENT_SCALE_LEVEL(mob) < GET_GLOBAL_MIN_LEVEL(glb)) {
			continue;
		}
		if (GET_GLOBAL_MAX_LEVEL(glb) > 0 && GET_CURRENT_SCALE_LEVEL(mob) > GET_GLOBAL_MAX_LEVEL(glb)) {
			continue;
		}
		
		// match ALL type-flags
		if ((MOB_FLAGS(mob) & GET_GLOBAL_TYPE_FLAGS(glb)) != GET_GLOBAL_TYPE_FLAGS(glb)) {
			continue;
		}
		// match ZERO type-excludes
		if ((MOB_FLAGS(mob) & GET_GLOBAL_TYPE_EXCLUDE(glb)) != 0) {
			continue;
		}
		
		// check adventure-only -- late-matching because it does more work than other conditions
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_ADVENTURE_ONLY) && get_adventure_for_vnum(GET_GLOBAL_VNUM(glb)) != adv) {
			continue;
		}
		
		// percent checks last
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CUMULATIVE_PERCENT)) {
			if (done_cumulative) {
				continue;
			}
			cumulative_prc -= (int)(GET_GLOBAL_PERCENT(glb) * 100);
			if (cumulative_prc <= 0) {
				done_cumulative = TRUE;
			}
			else {
				continue;	// not this time
			}
		}
		else if (number(1, 10000) > (int)(GET_GLOBAL_PERCENT(glb) * 100)) {
			// normal not-cumulative percent
			continue;
		}
		
		// we have a match!
		
		// check choose-last
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CHOOSE_LAST)) {
			if (!choose_last) {
				choose_last = glb;
			}
			continue;
		}
		
		// not choose-last: run it
		any |= run_interactions(ch, GET_GLOBAL_INTERACTIONS(glb), type, IN_ROOM(ch), mob, NULL, func);
	}
	
	// do the choose-last
	if (choose_last && !any) {
		any |= run_interactions(ch, GET_GLOBAL_INTERACTIONS(choose_last), type, IN_ROOM(ch), mob, NULL, func);
	}
	
	return any;
}


/**
* Runs a set of interactions and passes successful ones through to a function
* pointer.
*
* @param char_data *ch The actor.
* @param struct interaction_item *run_list A pointer to the start of the list to run.
* @param int type Any INTERACT_ const.
* @param room_data *inter_room For room interactions, the room.
* @param char_data *inter_mob For mob interactions, the mob.
* @param obj_data *inter_item For item interactions, the item.
* @param INTERACTION_FUNC(*func) A function pointer that runs each successful interaction (func returns TRUE if an interaction happens; FALSE if it aborts)
* @return bool TRUE if any interactions ran successfully, FALSE if not.
*/
bool run_interactions(char_data *ch, struct interaction_item *run_list, int type, room_data *inter_room, char_data *inter_mob, obj_data *inter_item, INTERACTION_FUNC(*func)) {
	struct interact_exclusion_data *exclusion = NULL;
	struct interaction_item *interact;
	bool success = FALSE;

	for (interact = run_list; interact; interact = interact->next) {
		if (interact->type == type && check_exclusion_set(&exclusion, interact->exclusion_code, interact->percent)) {
			if (func) {
				// run function
				success |= (func)(ch, interact, inter_room, inter_mob, inter_item);
			}
		}
	}
	
	free_exclusion_data(exclusion);
	return success;
}


/**
* Runs all interactions for a room (sect, crop, building; as applicable).
* They run from most-specific to least: bdg -> crop -> sect
*
* @param char_data *ch The actor.
* @param room_data *room The location to run on.
* @param int type Any INTERACT_ const.
* @param INTERACTION_FUNC(*func) A function pointer that runs each successful interaction (func returns TRUE if an interaction happens; FALSE if it aborts)
* @return bool TRUE if any interactions ran successfully, FALSE if not.
*/
bool run_room_interactions(char_data *ch, room_data *room, int type, INTERACTION_FUNC(*func)) {
	bool success;
	crop_data *crop;
	
	success = FALSE;
	
	// building first
	if (!success && GET_BUILDING(room)) {
		success |= run_interactions(ch, GET_BLD_INTERACTIONS(GET_BUILDING(room)), type, room, NULL, NULL, func);
	}
	
	// crop second
	if (!success && (crop = ROOM_CROP(room))) {
		success |= run_interactions(ch, GET_CROP_INTERACTIONS(crop), type, room, NULL, NULL, func);
	}
	
	// rmt third
	if (!success && GET_ROOM_TEMPLATE(room)) {
		success |= run_interactions(ch, GET_RMT_INTERACTIONS(GET_ROOM_TEMPLATE(room)), type, room, NULL, NULL, func);
	}
	
	// sector fourth
	if (!success) {
		success |= run_interactions(ch, GET_SECT_INTERACTIONS(SECT(room)), type, room, NULL, NULL, func);
	}
	
	return success;
}


 //////////////////////////////////////////////////////////////////////////////
//// LORE HANDLERS ///////////////////////////////////////////////////////////

/**
* Add lore to a character's list
*
* @param char_data *ch The person receiving the lore.
* @param int type LORE_x const
* @param const char *str String formatting.
* @param ... printf-style args for str.
*/
void add_lore(char_data *ch, int type, const char *str, ...) {
	struct lore_data *new, *lore;
	char text[MAX_STRING_LENGTH];
	va_list tArgList;

	if (!ch || IS_NPC(ch) || !str)
		return;
	
	// need the old lore, in case the player is offline
	check_delayed_load(ch);
	
	// find end
	if ((lore = GET_LORE(ch))) {
		while (lore->next) {
			lore = lore->next;
		}
	}
	
	va_start(tArgList, str);
	vsprintf(text, str, tArgList);
	
	CREATE(new, struct lore_data, 1);
	new->type = type;
	new->date = (long) time(0);
	new->text = str_dup(text);
	new->next = NULL;

	// append to end
	if (lore) {
		lore->next = new;
	}
	else {
		GET_LORE(ch) = new;
	}
	
	va_end(tArgList);
}


/**
* Clean up old kill/death lore
*
* @param char_data *ch The person whose lore to clean.
*/
void clean_lore(char_data *ch) {
	extern const int remove_lore_types[];

	struct lore_data *lore, *next_lore;
	struct time_info_data t;
	int iter;
	bool remove;
	
	int remove_lore_after_years = config_get_int("remove_lore_after_years");
	int starting_year = config_get_int("starting_year");
	
	// need the old lore, in case the player is offline
	check_delayed_load(ch);

	if (!IS_NPC(ch)) {
		for (lore = GET_LORE(ch); lore; lore = next_lore) {
			next_lore = lore->next;
			
			// match type
			remove = FALSE;
			for (iter = 0; remove_lore_types[iter] != -1 && !remove; ++iter) {
				if (lore->type == remove_lore_types[iter]) {
					remove = TRUE;
				}
			}

			// did it match? remove if outdated
			if (remove) {
				t = *mud_time_passed(time(0), (time_t) lore->date);
				if ((t.year - starting_year) >= remove_lore_after_years) {
					remove_lore_record(ch, lore);
				}
			}
		}
	}
}


/**
* Remove all lore of a given type.
*
* @param char_data *ch The person whose lore to remove
* @param int type The LORE_x type to remove
*/
void remove_lore(char_data *ch, int type) {
	struct lore_data *lore, *next_lore;

	if (IS_NPC(ch))
		return;
	
	// need the old lore, in case the player is offline
	check_delayed_load(ch);

	for (lore = GET_LORE(ch); lore; lore = next_lore) {
		next_lore = lore->next;

		if (lore->type == type) {
			remove_lore_record(ch, lore);
		}
	}
}


/**
* Remove lore of a given type if it was recent, but leaves older copies. This
* prevents spammy lore but keeps good historical data. The cutoff is 1 mud
* year.
*
* @param char_data *ch The person whose lore to remove
* @param int type The LORE_x type to remove
*/
void remove_recent_lore(char_data *ch, int type) {
	struct lore_data *lore, *next_lore;

	if (IS_NPC(ch))
		return;
	
	// need the old lore, in case the player is offline
	check_delayed_load(ch);

	LL_FOREACH_SAFE(GET_LORE(ch), lore, next_lore) {
		if (lore->type != type) {
			continue;
		}
		if (lore->date + SECS_PER_MUD_YEAR < time(0)) {
			continue;
		}
			
		
		remove_lore_record(ch, lore);
	}
}


/* Remove specific lore */
void remove_lore_record(char_data *ch, struct lore_data *lore) {
	struct lore_data *temp;

	if (!ch || IS_NPC(ch) || !lore)
		return;

	REMOVE_FROM_LIST(lore, GET_LORE(ch), next);
	if (lore->text) {
		free(lore->text);
	}
	free(lore);
}


 //////////////////////////////////////////////////////////////////////////////
//// MOB TAGGING HANDLERS ////////////////////////////////////////////////////

/**
* Create a new mob tag and add it to a tag list.
*
* @param int idnum The idnum to tag for.
* @param struct mob_tag **list A pointer to the list to add to.
*/
static void add_mob_tag(int idnum, struct mob_tag **list) {
	struct mob_tag *tag;
	
	CREATE(tag, struct mob_tag, 1);
	tag->idnum = idnum;
	tag->next = *list;
	*list = tag;
}


/**
* @param int id Any player id.
* @param struct mob_tag *list The tag list to search.
* @return bool TRUE if the tag is in the list; otherwise FALSE.
*/
bool find_id_in_tag_list(int id, struct mob_tag *list) {
	struct mob_tag *iter;
	
	for (iter = list; iter; iter = iter->next) {
		if (iter->idnum == id) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* Called when a mob dies to expand the tags to current group members of anyone
* already tagged.
*
* @param char_data *mob The mob whose tags to expand.
*/
void expand_mob_tags(char_data *mob) {
	struct group_member_data *mem;
	struct mob_tag *tag, *new_list;
	char_data *player;
	
	// sanity
	if (!mob || !IS_NPC(mob) || !MOB_TAGGED_BY(mob)) {
		return;
	}
	
	new_list = NULL;
	
	for (tag = MOB_TAGGED_BY(mob); tag; tag = tag->next) {
		player = is_playing(tag->idnum);
		
		if (player) {
			// re-add existing player
			if (!find_id_in_tag_list(GET_IDNUM(player), new_list)) {
				add_mob_tag(GET_IDNUM(player), &new_list);
			}
			
			// check player's group
			if (GROUP(player)) {
				for (mem = GROUP(player)->members; mem; mem = mem->next) {
					if (IS_NPC(mem->member) || find_id_in_tag_list(GET_IDNUM(mem->member), new_list)) {
						continue;
					}
					
					// check proximity (only if present)
					if (IN_ROOM(mem->member) != IN_ROOM(mob)) {
						continue;
					}
					
					add_mob_tag(GET_IDNUM(mem->member), &new_list);
				}
			}
		}
		else {
			if (!find_id_in_tag_list(tag->idnum, new_list)) {
				add_mob_tag(tag->idnum, &new_list);
			}
		}
	}
	
	free_mob_tags(&MOB_TAGGED_BY(mob));
	MOB_TAGGED_BY(mob) = new_list;
}


/**
* Frees a list of mob tags.
*
* @param struct mob_tag **list A pointer to the list to free.
*/
void free_mob_tags(struct mob_tag **list) {
	struct mob_tag *tag;
	
	while (list && (tag = *list)) {
		*list = tag->next;
		free(tag);
	}
}


/**
* Tags a mob for a player and their group. This will never re-tag a mob.
*
* @param char_data *mob The mob to tag.
* @param char_data *player The player to tag them for.
*/
void tag_mob(char_data *mob, char_data *player) {
	struct group_member_data *mem;
	
	// simple sanity
	if (!mob || !player || mob == player || !IS_NPC(mob) || IS_NPC(player)) {
		return;
	}
	
	// do not re-tag
	if (MOB_TAGGED_BY(mob)) {
		return;
	}

	if (GROUP(player)) {
		// tag for whole group
		for (mem = GROUP(player)->members; mem; mem = mem->next) {
			if (IS_NPC(mem->member)) {
				continue;
			}
			
			// check proximity (must be present)
			if (IN_ROOM(mem->member) != IN_ROOM(mob)) {
				continue;
			}
			
			// never tag for someone with no descriptor
			if (!mem->member->desc) {
				continue;
			}
			
			add_mob_tag(GET_IDNUM(mem->member), &MOB_TAGGED_BY(mob));
		}
	}
	else if (player->desc) { // never tag for someone with no descriptor
		// just tag for player
		add_mob_tag(GET_IDNUM(player), &MOB_TAGGED_BY(mob));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MOUNT HANDLERS //////////////////////////////////////////////////////////

/**
* Adds a mount to a player by vnum/flags. Won't add a duplicate.
*
* @param char_data *ch The player.
* @param mob_vnum vnum The mount's vnum.
* @param bitvector_t flags The MOUNT_ flags to set.
*/
void add_mount(char_data *ch, mob_vnum vnum, bitvector_t flags) {
	struct mount_data *mount;
	
	// npcs can't add mounts
	if (IS_NPC(ch)) {
		return;
	}
	
	// find or add data (don't want to double up)
	if (!(mount = find_mount_data(ch, vnum))) {
		CREATE(mount, struct mount_data, 1);
		mount->vnum = vnum;
		HASH_ADD_INT(GET_MOUNT_LIST(ch), vnum, mount);
	}
	
	// we can safely overwrite flags here
	mount->flags = flags;
}


/**
* @param char_data *mob The mount mob.
* @return bitvector_t MOUNT_ flags corresponding to that mob.
*/
bitvector_t get_mount_flags_by_mob(char_data *mob) {
	bitvector_t flags = NOBITS;
	
	// MOUNT_x: detect mount flags
	if (AFF_FLAGGED(mob, AFF_FLY)) {
		SET_BIT(flags, MOUNT_FLYING);
	}
	if (MOB_FLAGGED(mob, MOB_AQUATIC)) {
		SET_BIT(flags, MOUNT_AQUATIC);
	}
	
	return flags;
}


/**
* Find the mount_data entry for a certain mount on a player.
*
* @param char_data *ch The player.
* @param mob_vnum vnum The mount mob vnum to find.
* @return struct mount_data* The found data, or NULL if player doesn't have it.
*/
struct mount_data *find_mount_data(char_data *ch, mob_vnum vnum) {
	struct mount_data *data;
	
	if (IS_NPC(ch)) {
		return NULL;
	}
	
	HASH_FIND_INT(GET_MOUNT_LIST(ch), &vnum, data);
	return data;
}


/**
* This dismounts a player, but keeps the mount data stored.
*
* @param char_data *ch The player who is dismounting.
*/
void perform_dismount(char_data *ch) {
	// NPCs don't mount
	if (IS_NPC(ch)) {
		return;
	}
	
	// un-set the riding flag but keep the mount info stored	
	REMOVE_BIT(GET_MOUNT_FLAGS(ch), MOUNT_RIDING);
	
	if (GET_EQ(ch, WEAR_SADDLE)) {
		perform_remove(ch, WEAR_SADDLE);
	}
}


/**
* Caution: this function will extract the mount mob
*
* @param char_data *ch The player doing the mounting
* @param char_data *mount The NPC to be mounted
*/
void perform_mount(char_data *ch, char_data *mount) {
	bitvector_t flags;
	
	// sanity check
	if (IS_NPC(ch) || !IS_NPC(mount)) {
		return;
	}
	
	// store mount vnum and set riding
	flags = get_mount_flags_by_mob(mount);
	add_mount(ch, GET_MOB_VNUM(mount), flags);
	GET_MOUNT_VNUM(ch) = GET_MOB_VNUM(mount);
	SET_BIT(GET_MOUNT_FLAGS(ch), MOUNT_RIDING | flags);
	
	// extract the mount mob
	extract_char(mount);
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT HANDLERS /////////////////////////////////////////////////////////

/**
* Put an object into the global object list.
*
* @param obj_data *obj The item to add to the global object list.
*/
void add_to_object_list(obj_data *obj) {
	obj->next = object_list;
	object_list = obj;
}


/**
* Copies an item, even a modified one, to a new item.
*
* @param obj_data *input The item to copy.
*/
obj_data *copy_warehouse_obj(obj_data *input) {
	extern struct extra_descr_data *copy_extra_descs(struct extra_descr_data *list);

	obj_data *obj, *proto;
	trig_data *trig;
	int iter;
	
	if (!input) {
		return NULL;
	}
	
	// store this
	proto = obj_proto(GET_OBJ_VNUM(input));
	
	// create dupe
	if (proto) {
		obj = read_object(GET_OBJ_VNUM(input), FALSE);	// no scripts
	}
	else {
		obj = create_obj();
	}
	
	// error in either case?
	if (!obj) {
		return NULL;
	}
	
	// copy only existing scripts
	if (SCRIPT(input)) {
		if (!SCRIPT(obj)) {
			create_script_data(obj, OBJ_TRIGGER);
		}

		for (trig = TRIGGERS(SCRIPT(input)); trig; trig = trig->next) {
			add_trigger(SCRIPT(obj), read_trigger(GET_TRIG_VNUM(trig)), -1);
		}
		
		// TODO should also copy variables, if they have been made to save to file yet
	}
	
	// pointer copies
	if (GET_OBJ_KEYWORDS(input) && (!proto || GET_OBJ_KEYWORDS(input) != GET_OBJ_KEYWORDS(proto))) {
		GET_OBJ_KEYWORDS(obj) = str_dup(GET_OBJ_KEYWORDS(input));
	}
	if (GET_OBJ_SHORT_DESC(input) && (!proto || GET_OBJ_SHORT_DESC(input) != GET_OBJ_SHORT_DESC(proto))) {
		GET_OBJ_SHORT_DESC(obj) = str_dup(GET_OBJ_SHORT_DESC(input));
	}
	if (GET_OBJ_LONG_DESC(input) && (!proto || GET_OBJ_LONG_DESC(input) != GET_OBJ_LONG_DESC(proto))) {
		GET_OBJ_LONG_DESC(obj) = str_dup(GET_OBJ_LONG_DESC(input));
	}
	if (GET_OBJ_ACTION_DESC(input) && (!proto || GET_OBJ_ACTION_DESC(input) != GET_OBJ_ACTION_DESC(proto))) {
		GET_OBJ_ACTION_DESC(obj) = str_dup(GET_OBJ_ACTION_DESC(input));
	}
	if (input->ex_description && (!proto || input->ex_description != proto->ex_description)) {
		obj->ex_description = copy_extra_descs(input->ex_description);
	}
	
	// non-point copies
	GET_OBJ_EXTRA(obj) = GET_OBJ_EXTRA(input);
	GET_OBJ_CURRENT_SCALE_LEVEL(obj) = GET_OBJ_CURRENT_SCALE_LEVEL(input);
	GET_OBJ_AFF_FLAGS(obj) = GET_OBJ_AFF_FLAGS(input);
	GET_OBJ_TIMER(obj) = GET_OBJ_TIMER(input);
	GET_OBJ_TYPE(obj) = GET_OBJ_TYPE(input);
	GET_OBJ_WEAR(obj) = GET_OBJ_WEAR(input);
	GET_STOLEN_TIMER(obj) = GET_STOLEN_TIMER(input);
	GET_STOLEN_FROM(obj) = GET_STOLEN_FROM(input);
	
	for (iter = 0; iter < NUM_OBJ_VAL_POSITIONS; ++iter) {
		GET_OBJ_VAL(obj, iter) = GET_OBJ_VAL(input, iter);
	}
	GET_OBJ_APPLIES(obj) = copy_obj_apply_list(GET_OBJ_APPLIES(input));
	
	return obj;
}


/**
* Empties the contents of an object into the place the object is. For example,
* an object in a room empties out to the room.
*
* @param obj_data *obj The object to empty.
*/
void empty_obj_before_extract(obj_data *obj) {
	void get_check_money(char_data *ch, obj_data *obj);
	
	obj_data *jj, *next_thing;
	
	for (jj = obj->contains; jj; jj = next_thing) {
		next_thing = jj->next_content;		/* Next in inventory */

		if (obj->in_obj) {
			obj_to_obj(jj, obj->in_obj);
		}
		else if (obj->carried_by) {
			obj_to_char_if_okay(jj, obj->carried_by);
			get_check_money(obj->carried_by, jj);
		}
		else if (obj->worn_by) {
			obj_to_char_if_okay(jj, obj->worn_by);
			get_check_money(obj->worn_by, jj);
		}
		else if (obj->in_vehicle) {
			obj_to_vehicle(jj, obj->in_vehicle);
		}
		else if (IN_ROOM(obj)) {
			obj_to_room(jj, IN_ROOM(obj));
		}
		else {
			extract_obj(jj);
		}
	}
}


/**
* Extract an object from the world -- the main way of destroying an obj_data*
*
* @param obj_data *obj The object to extract and free.
*/
void extract_obj(obj_data *obj) {
	obj_data *proto = obj_proto(GET_OBJ_VNUM(obj));
	
	if (obj == dg_owner_obj) {
		dg_owner_purged = 1;
		dg_owner_obj = NULL;
	}

	// remove from anywhere
	check_obj_in_void(obj);

	/* Get rid of the contents of the object, as well. */
	while (obj->contains) {
		extract_obj(obj->contains);
	}
	
	remove_from_object_list(obj);

	if (SCRIPT(obj)) {
		extract_script(obj, OBJ_TRIGGER);
	}

	if (!proto || obj->proto_script != proto->proto_script) {
		free_proto_scripts(&obj->proto_script);
	}

	free_obj(obj);
}


/**
* Makes a fresh copy of an object, preserving certain flags such as superior
* and keep. All temporary properties including bindings are copied over. You
* can swap it out using swap_obj_for_obj() or any other method, then extract
* the original object when done.
*
* @param obj_data *obj The item to load a fresh copy of.
* @param int scale_level If >0, will scale the new copy to that level.
* @return obj_data* The new object.
*/
obj_data *fresh_copy_obj(obj_data *obj, int scale_level) {
	struct obj_binding *bind;
	obj_data *proto, *new;
	int iter;
	
	if (!obj || !(proto = obj_proto(GET_OBJ_VNUM(obj)))) {
		// get a normal 'bug' object
		return read_object(0, TRUE);
	}

	new = read_object(GET_OBJ_VNUM(obj), TRUE);
	
	// preserve some flags
	GET_OBJ_EXTRA(new) |= GET_OBJ_EXTRA(obj) & (OBJ_SUPERIOR | OBJ_KEEP);
	if (!OBJ_FLAGGED(new, OBJ_GENERIC_DROP)) {
		GET_OBJ_EXTRA(new) |= GET_OBJ_EXTRA(obj) & (OBJ_HARD_DROP | OBJ_GROUP_DROP);
	}
	
	// copy bindings	
	for (bind = OBJ_BOUND_TO(obj); bind; bind = bind->next) {
		add_obj_binding(bind->idnum, &OBJ_BOUND_TO(new));
	}
	
	if (GET_OBJ_TIMER(obj) != UNLIMITED && GET_OBJ_TIMER(proto) != UNLIMITED) {
		// only change if BOTH are unlimited. Otherwise, this trait was changed and we should inherit it.
		GET_OBJ_TIMER(new) = GET_OBJ_TIMER(obj);
	}
	GET_AUTOSTORE_TIMER(new) = GET_AUTOSTORE_TIMER(obj);
	new->stolen_timer = obj->stolen_timer;
	GET_STOLEN_FROM(new) = GET_STOLEN_FROM(obj);
	new->last_owner_id = obj->last_owner_id;
	new->last_empire_id = obj->last_empire_id;
	
	// custom strings?
	if (GET_OBJ_SHORT_DESC(obj) && GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(proto)) {
		GET_OBJ_SHORT_DESC(new) = str_dup(GET_OBJ_SHORT_DESC(obj));
	}
	if (GET_OBJ_LONG_DESC(obj) && GET_OBJ_LONG_DESC(obj) != GET_OBJ_LONG_DESC(proto)) {
		GET_OBJ_LONG_DESC(new) = str_dup(GET_OBJ_LONG_DESC(obj));
	}
	if (GET_OBJ_KEYWORDS(obj) && GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(proto)) {
		GET_OBJ_KEYWORDS(new) = str_dup(GET_OBJ_KEYWORDS(obj));
	}
	if (GET_OBJ_ACTION_DESC(obj) && GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(proto)) {
		GET_OBJ_ACTION_DESC(new) = str_dup(GET_OBJ_ACTION_DESC(obj));
	}
	
	// certain things that must always copy over
	switch (GET_OBJ_TYPE(new)) {
		case ITEM_ARROW: {
			GET_OBJ_VAL(new, VAL_ARROW_QUANTITY) = GET_OBJ_VAL(obj, VAL_ARROW_QUANTITY);
			break;
		}
		case ITEM_BOOK: {
			GET_OBJ_VAL(new, VAL_BOOK_ID) = GET_OBJ_VAL(obj, VAL_BOOK_ID);
			break;
		}
		case ITEM_DRINKCON: {
			GET_OBJ_VAL(new, VAL_DRINK_CONTAINER_CONTENTS) = GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS);
			GET_OBJ_VAL(new, VAL_DRINK_CONTAINER_TYPE) = GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE);
			break;
		}
		case ITEM_PORTAL: {
			GET_OBJ_VAL(new, VAL_PORTAL_TARGET_VNUM) = GET_OBJ_VAL(obj, VAL_PORTAL_TARGET_VNUM);
			break;
		}
		case ITEM_POISON: {
			GET_OBJ_VAL(new, VAL_POISON_CHARGES) = GET_OBJ_VAL(obj, VAL_POISON_CHARGES);
			break;
		}
		case ITEM_SHIP: {
			// copy these blind
			for (iter = 0; iter < NUM_OBJ_VAL_POSITIONS; ++iter) {
				GET_OBJ_VAL(new, iter) = GET_OBJ_VAL(obj, iter);
			}
			break;
		}
	}
	

	if (scale_level > 0) {
		scale_item_to_level(new, scale_level);
	}
	
	return new;
}


/**
* Compares two items for identicallity. These may be highly-customized items.
* 
* @param obj_data *obj_a First object to compare.
* @param obj_data *obj_b Second object to compare.
* @return bool TRUE if the two items are functionally identical.
*/
bool objs_are_identical(obj_data *obj_a, obj_data *obj_b) {
	struct obj_apply *a_apply, *b_list, *b_apply, *temp;
	bool found;
	int iter;
	
	if (GET_OBJ_VNUM(obj_a) != GET_OBJ_VNUM(obj_b)) {
		return FALSE;
	}
	if (GET_OBJ_EXTRA(obj_a) != GET_OBJ_EXTRA(obj_b)) {
		return FALSE;
	}
	if (GET_OBJ_CURRENT_SCALE_LEVEL(obj_a) != GET_OBJ_CURRENT_SCALE_LEVEL(obj_b)) {
		return FALSE;
	}
	if (GET_OBJ_AFF_FLAGS(obj_a) != GET_OBJ_AFF_FLAGS(obj_b)) {
		return FALSE;
	}
	if (GET_OBJ_TIMER(obj_a) != GET_OBJ_TIMER(obj_b)) {
		return FALSE;
	}
	if (GET_OBJ_TYPE(obj_a) != GET_OBJ_TYPE(obj_b)) {
		return FALSE;
	}
	if (GET_OBJ_WEAR(obj_a) != GET_OBJ_WEAR(obj_b)) {
		return FALSE;
	}
	if (GET_STOLEN_TIMER(obj_a) != GET_STOLEN_TIMER(obj_b)) {
		return FALSE;
	}
	if (GET_STOLEN_FROM(obj_a) != GET_STOLEN_FROM(obj_b)) {
		return FALSE;
	}
	for (iter = 0; iter < NUM_OBJ_VAL_POSITIONS; ++iter) {
		if (GET_OBJ_VAL(obj_a, iter) != GET_OBJ_VAL(obj_b, iter)) {
			return FALSE;
		}
	}
	if (GET_OBJ_KEYWORDS(obj_a) != GET_OBJ_KEYWORDS(obj_b) && !str_cmp(GET_OBJ_KEYWORDS(obj_a), GET_OBJ_KEYWORDS(obj_b))) {
		return FALSE;
	}
	if (GET_OBJ_SHORT_DESC(obj_a) != GET_OBJ_SHORT_DESC(obj_b) && !str_cmp(GET_OBJ_SHORT_DESC(obj_a), GET_OBJ_SHORT_DESC(obj_b))) {
		return FALSE;
	}
	if (GET_OBJ_LONG_DESC(obj_a) != GET_OBJ_LONG_DESC(obj_b) && !str_cmp(GET_OBJ_LONG_DESC(obj_a), GET_OBJ_LONG_DESC(obj_b))) {
		return FALSE;
	}
	if (GET_OBJ_ACTION_DESC(obj_a) != GET_OBJ_ACTION_DESC(obj_b) && !str_cmp(GET_OBJ_ACTION_DESC(obj_a), GET_OBJ_ACTION_DESC(obj_b))) {
		return FALSE;
	}
	if (obj_a->ex_description != obj_b->ex_description) {
		return FALSE;
	}
	
	// to compare applies, we're going to copy and delete as we find them
	b_list = copy_obj_apply_list(GET_OBJ_APPLIES(obj_b));
	for (a_apply = GET_OBJ_APPLIES(obj_a); a_apply; a_apply = a_apply->next) {
		found = FALSE;
		for (b_apply = b_list; b_apply; b_apply = b_apply->next) {
			if (a_apply->location == b_apply->location && a_apply->modifier == b_apply->modifier && a_apply->apply_type == b_apply->apply_type) {
				found = TRUE;
				REMOVE_FROM_LIST(b_apply, b_list, next);
				free(b_apply);
				break;	// only need one, plus we freed it
			}
		}
		
		if (!found) {
			free_obj_apply_list(b_list);
			return FALSE;
		}
	}
	if (b_list) {	// more things in b_list than a
		free_obj_apply_list(b_list);
		return FALSE;
	}
	
	// all good then
	return TRUE;
}


/**
* Takes player input of a complex component like "bunch block" or
* "large, single fruit" and gets the CMP_ and CMPF_ settings from it.
*
* @param char *str The character's input.
* @param int *type A variable to bind a CMP_ type (may be NOTHING or CMP_NONE).
* @param bitvector_t *flags Any CMPF_ flags requested.
* @return bool TRUE if the whole string parses into a component, FALSE if not.
*/
bool parse_component(char *str, int *type, bitvector_t *flags) {
	extern const char *component_types[];
	extern const char *component_flags[];
	
	char temp[MAX_INPUT_LENGTH], word[MAX_INPUT_LENGTH], *ptr;
	int flg;
	
	// base setup
	*type = NOTHING;
	*flags = NOBITS;
	
	// make copy
	strncpy(temp, str, MAX_INPUT_LENGTH-1);
	temp[MAX_INPUT_LENGTH-1] = '\0';	// safety firsty
	
	// remove commas
	while ((ptr = strchr(temp, ','))) {
		*ptr = ' ';
	}
	
	// check words 1 at a time
	while (*temp) {
		half_chop(temp, word, temp);	// split out first word
		
		if (!*temp) {	// final word is the component type
			*type = search_block(word, component_types, FALSE);
		}
		else {	// each previous word is a possible flag
			flg = search_block(word, component_flags, FALSE);
			if (flg != NOTHING) {
				*flags |= BIT(flg);
			}
			else {
				// bad flag
				*type = NOTHING;
				return FALSE;
			}
		}
	}
	
	return (*type != NOTHING && *type != CMP_NONE);
}


/**
* Remove an object from the global object list.
*
* @param obj_data *obj The item to remove from the global object list.
*/
void remove_from_object_list(obj_data *obj) {
	obj_data *temp;
	REMOVE_FROM_LIST(obj, object_list, next);
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT BINDING HANDLERS /////////////////////////////////////////////////

/**
* Create a new binding and add it to a list.
*
* @param int idnum The id to bind to.
* @param struct obj_binding **list A pointer to the list to add to.
*/
static void add_obj_binding(int idnum, struct obj_binding **list) {
	struct obj_binding *bind;
	
	CREATE(bind, struct obj_binding, 1);
	bind->idnum = idnum;
	bind->next = *list;
	*list = bind;
}


/**
* Binds an object to a whole group.
*
* @param obj_data *obj The object to bind.
* @param struct group_data *group The group to bind to.
*/
void bind_obj_to_group(obj_data *obj, struct group_data *group) {
	struct group_member_data *mem;
	
	// sanity
	if (!obj || !group || !OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
		return;
	}
	
	// is obj already bound?
	if (OBJ_BOUND_TO(obj)) {
		return;
	}
	
	for (mem = group->members; mem; mem = mem->next) {
		if (!IS_NPC(mem->member) && !IS_IMMORTAL(mem->member)) {
			add_obj_binding(GET_IDNUM(mem->member), &OBJ_BOUND_TO(obj));
		}
	}
}


/**
* Binds an object to a player.
*
* @param obj_data *obj The object to bind.
* @param char_data *ch The player to bind to.
*/
void bind_obj_to_player(obj_data *obj, char_data *ch) {
	// sanity
	if (!obj || !ch || IS_NPC(ch) || IS_IMMORTAL(ch) || !OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
		return;
	}
	
	// is obj already bound?
	if (OBJ_BOUND_TO(obj)) {
		return;
	}
	
	add_obj_binding(GET_IDNUM(ch), &OBJ_BOUND_TO(obj));
}


/**
* Binds an object to everyone on a mob tag list.
*
* @param obj_data *obj The object to bind.
* @param struct mob_tag *list The list of mob tags.
*/
void bind_obj_to_tag_list(obj_data *obj, struct mob_tag *list) {
	bool at_least_one = FALSE;
	struct mob_tag *tag;
	char_data *plr;
	
	// sanity
	if (!obj || !list || !OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
		return;
	}
	
	// is obj already bound?
	if (OBJ_BOUND_TO(obj)) {
		return;
	}
	
	for (tag = list; tag; tag = tag->next) {
		if (!(plr = is_playing(tag->idnum))) {
			continue;	// don't bind to missing players
		}
		if (!plr->desc) {
			continue;	// don't bind to linkdead players
		}
		add_obj_binding(tag->idnum, &OBJ_BOUND_TO(obj));
		at_least_one = TRUE;
	}
	
	// guarantee we bound to at least 1 -- if not, we'll have to bind to at least one anyway
	if (list && !at_least_one) {
		for (tag = list; tag; tag = tag->next) {
			add_obj_binding(tag->idnum, &OBJ_BOUND_TO(obj));
			break;
		}
	}
}


/**
* Determines if a player can legally obtain a bound object.
*
* @param obj_data *obj The item to check.
* @param char_data *ch The player to check.
* @return bool TRUE if ch can use obj, FALSE if binding prevents it.
*/
bool bind_ok(obj_data *obj, char_data *ch) {
	struct obj_binding *bind;
	
	// basic sanity
	if (!obj || !ch) {
		return FALSE;
	}
	
	// bound at all?
	if (!OBJ_BOUND_TO(obj)) {
		return TRUE;
	}
	
	// imm/npc override
	if (IS_NPC(ch) || IS_IMMORTAL(ch)) {
		return TRUE;
	}
	
	for (bind = OBJ_BOUND_TO(obj); bind; bind = bind->next) {
		if (bind->idnum == GET_IDNUM(ch)) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* Duplicates an obj binding list.
*
* @param struct obj_binding *from The list to copy.
* @return struct obj_binding* The copied list.
*/
struct obj_binding *copy_obj_bindings(struct obj_binding *from) {
	struct obj_binding *list = NULL, *bind, *iter;
	
	LL_FOREACH(from, iter) {
		CREATE(bind, struct obj_binding, 1);
		*bind = *iter;
		LL_APPEND(list, bind);
	}
	
	return list;
}


/**
* Removes all bindings on an object other than a player's, for things that were
* bound to multiple players but are now reduced to just one.
*
* @param obj_data *obj The object to simplify bindings on.
* @param char_data *player The player to bind to.
*/
void reduce_obj_binding(obj_data *obj, char_data *player) {
	struct obj_binding *bind, *next_bind, *temp;
	
	if (!obj || !player || IS_NPC(player) || IS_IMMORTAL(player)) {
		return;
	}
	
	for (bind = OBJ_BOUND_TO(obj); bind; bind = next_bind) {
		next_bind = bind->next;
		if (bind->idnum != GET_IDNUM(player)) {
			REMOVE_FROM_LIST(bind, OBJ_BOUND_TO(obj), next);
			free(bind);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT LOCATION HANDLERS ////////////////////////////////////////////////


/**
* Make sure an object is not anywhere! This function removes the object from
* anywhere it happens to be.
*
* @param obj_data *obj The object to remove.
*/
void check_obj_in_void(obj_data *obj) {
	if (obj) {
		if (IN_ROOM(obj)) {
			obj_from_room(obj);
		}
		if (obj->in_obj) {
			obj_from_obj(obj);
		}
		if (obj->carried_by) {
			obj_from_char(obj);
		}
		if (obj->in_vehicle) {
			obj_from_vehicle(obj);
		}
		if (obj->worn_by) {
			if (unequip_char(obj->worn_by, obj->worn_on) != obj) {
				log("SYSERR: Inconsistent worn_by and worn_on pointers!!");
			}
		}
	}
}


/**
* formally equips an item to a person
*
* @param char_data *ch The person to equip
* @param obj_data *obj The item to equip
* @param int pos the WEAR_x spot to it
*/
void equip_char(char_data *ch, obj_data *obj, int pos) {
	struct obj_apply *apply;

	if (pos < 0 || pos >= NUM_WEARS) {
		log("SYSERR: Trying to equip gear to invalid position: %d", pos);
	}
	else if (GET_EQ(ch, pos)) {
		log("SYSERR: Char is already equipped: %s, %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj));
	}
	else {
		check_obj_in_void(obj);
		
		// check binding
		if (OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
			bind_obj_to_player(obj, ch);
			reduce_obj_binding(obj, ch);	// in case it was bound to multiple people before equipping
		}
		
		// check that it's scaled? if we got this far and it's not, scale it to its minimum level
		if (OBJ_FLAGGED(obj, OBJ_SCALABLE) && !IS_NPC(ch)) {
			scale_item_to_level(obj, GET_OBJ_MIN_SCALE_LEVEL(obj));
		}

		GET_EQ(ch, pos) = obj;
		obj->worn_by = ch;
		obj->worn_on = pos;

		// lights?
		if (IN_ROOM(ch) && OBJ_FLAGGED(obj, OBJ_LIGHT)) {
			ROOM_LIGHTS(IN_ROOM(ch))++;
		}

		if (wear_data[pos].count_stats) {
			for (apply = GET_OBJ_APPLIES(obj); apply; apply = apply->next) {
				affect_modify(ch, apply->location, apply->modifier, NOBITS, TRUE);
			}
			if (GET_OBJ_AFF_FLAGS(obj)) {
				affect_modify(ch, APPLY_NONE, 0, GET_OBJ_AFF_FLAGS(obj), TRUE);
			}
		}

		affect_total(ch);
		qt_wear_obj(ch, obj);
	}
}


/**
* take an object from a char's inventory
*
* @param obj_data *object The item to remove
*/
void obj_from_char(obj_data *object) {
	obj_data *temp;

	if (object == NULL) {
		log("SYSERR: NULL object passed to obj_from_char.");
	}
	else {
		REMOVE_FROM_LIST(object, object->carried_by->carrying, next_content);
		object->next_content = NULL;

		IS_CARRYING_N(object->carried_by) -= obj_carry_size(object);

		// check lights
		if (IN_ROOM(object->carried_by) && OBJ_FLAGGED(object, OBJ_LIGHT)) {
			ROOM_LIGHTS(IN_ROOM(object->carried_by))--;
		}
		
		qt_drop_obj(object->carried_by, object);

		object->carried_by = NULL;
	}
}


/**
* remove an object from an object
*
* @param obj_data *obj The object to remove.
*/
void obj_from_obj(obj_data *obj) {
	obj_data *temp, *obj_from;

	if (obj->in_obj == NULL) {
		log("SYSERR: (%s): trying to illegally extract obj from obj.", __FILE__);
	}
	else {
		obj_from = obj->in_obj;
		REMOVE_FROM_LIST(obj, obj_from->contains, next_content);

		GET_OBJ_CARRYING_N(obj_from) -= obj_carry_size(obj);

		obj->in_obj = NULL;
		obj->next_content = NULL;
	}
}


/**
* Take an object from a room
*
* @param obj_data *object The item to remove.
*/
void obj_from_room(obj_data *object) {
	obj_data *temp;

	if (!object || !IN_ROOM(object)) {
		log("SYSERR: NULL object (%p) or obj not in a room (%p) passed to obj_from_room", object, IN_ROOM(object));
	}
	else {
		// update lights
		if (OBJ_FLAGGED(object, OBJ_LIGHT)) {
			ROOM_LIGHTS(IN_ROOM(object))--;
		}

		REMOVE_FROM_LIST(object, ROOM_CONTENTS(IN_ROOM(object)), next_content);
		IN_ROOM(object) = NULL;
		object->next_content = NULL;
	}
}


/**
* @param obj_data *object The item to remove from whatever vehicle it's in.
*/
void obj_from_vehicle(obj_data *object) {
	if (!object || !object->in_vehicle) {
		log("SYSERR: NULL object (%p) or obj not in a vehicle (%p) passed to obj_from_vehicle", object, object->in_vehicle);
	}
	else {
		VEH_LAST_MOVE_TIME(object->in_vehicle) = time(0);	// reset autostore time
		VEH_CARRYING_N(object->in_vehicle) -= obj_carry_size(object);
		LL_DELETE2(VEH_CONTAINS(object->in_vehicle), object, next_content);
		object->in_vehicle = NULL;
		object->next_content = NULL;
	}
}


/**
* Set all carried_by to point to new owner
*
* @param obj_data *list_start The head of any object list.
*/
void object_list_no_owner(obj_data *list_start) {
	if (list_start) {
		object_list_no_owner(list_start->contains);
		object_list_no_owner(list_start->next_content);
		list_start->carried_by = NULL;
	}
}


/**
* give an object to a char's inventory
*
* @param obj_data *object The item to give.
* @param char_data *ch Who to give it to.
*/
void obj_to_char(obj_data *object, char_data *ch) {
	check_obj_in_void(object);

	if (object && ch) {
		object->next_content = ch->carrying;
		ch->carrying = object;
		object->carried_by = ch;
		IS_CARRYING_N(ch) += obj_carry_size(object);
		
		// binding
		if (OBJ_FLAGGED(object, OBJ_BIND_ON_PICKUP)) {
			bind_obj_to_player(object, ch);
		}
		
		// set the timer here; actual rules for it are in limits.c
		GET_AUTOSTORE_TIMER(object) = time(0);
		
		// unmark uncollected loot
		if (!IS_NPC(ch)) {
			REMOVE_BIT(GET_OBJ_EXTRA(object), OBJ_UNCOLLECTED_LOOT);
		}
		
		// update last owner/empire -- only if not stolen
		if (!IS_STOLEN(object)) {
			if (IS_NPC(ch)) {
				object->last_owner_id = NOBODY;
				object->last_empire_id = NOTHING;
			}
			else {
				object->last_owner_id = GET_IDNUM(ch);
				object->last_empire_id = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING;
			}
		}

		// check if the room needs to be lit
		if (IN_ROOM(ch) && OBJ_FLAGGED(object, OBJ_LIGHT)) {
			ROOM_LIGHTS(IN_ROOM(ch))++;
		}
		
		qt_get_obj(ch, object);
	}
	else {
		log("SYSERR: NULL obj (%p) or char (%p) passed to obj_to_char.", object, ch);
	}
}


/**
* Validates bind and quest before allowing an obj to go to a char. Sends to
* the room as a backup.
*
* @param obj_data *obj The item.
* @param char_data *ch The person you're trying to give it to.
*/
void obj_to_char_if_okay(obj_data *obj, char_data *ch) {
	extern struct player_quest *is_on_quest(char_data *ch, any_vnum quest);
	
	bool ok = TRUE;
	
	if (!bind_ok(obj, ch)) {
		ok = FALSE;
	}
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !IS_NPC(ch) && !IS_IMMORTAL(ch) && !is_on_quest(ch, GET_OBJ_REQUIRES_QUEST(obj))) {
		ok = FALSE;
	}
	
	if (ok || !IN_ROOM(ch)) {
		obj_to_char(obj, ch);
	}
	else {
		// unmark uncollected loot if it was meant to go to a person
		if (!IS_NPC(ch)) {
			REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_UNCOLLECTED_LOOT);
		}
		
		// set ownership as if they got it -- if not stolen
		if (!IS_STOLEN(obj)) {
			if (IS_NPC(ch)) {
				obj->last_owner_id = NOBODY;
				obj->last_empire_id = NOTHING;
			}
			else {
				obj->last_owner_id = GET_IDNUM(ch);
				obj->last_empire_id = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING;
			}
		}
		
		obj_to_room(obj, IN_ROOM(ch));
	}
}


/**
* determines whethet the object goes to the room or char based on
* inventory capacity -- it will drop on the ground if ch can't lift it
*
* @param obj_data *obj The item to try to give.
* @param char_data *ch The person to try to give it to.
*/
void obj_to_char_or_room(obj_data *obj, char_data *ch) {
	if (!IS_NPC(ch) && !CAN_CARRY_OBJ(ch, obj) && IN_ROOM(ch)) {
		// bind it to the player anyway, as if they received it, if it's BoP
		if (OBJ_FLAGGED(obj, OBJ_BIND_ON_PICKUP)) {
			bind_obj_to_player(obj, ch);
		}
		
		obj_to_room(obj, IN_ROOM(ch));
		
		// unmark uncollected loot if it was meant to go to a person
		if (!IS_NPC(ch)) {
			REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_UNCOLLECTED_LOOT);
		}
		
		// set ownership as if they got it -- if not stolen
		if (!IS_STOLEN(obj)) {
			if (IS_NPC(ch)) {
				obj->last_owner_id = NOBODY;
				obj->last_empire_id = NOTHING;
			}
			else {
				obj->last_owner_id = GET_IDNUM(ch);
				obj->last_empire_id = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING;
			}
		}
	}
	else {
		obj_to_char(obj, ch);
	}
}


/**
* put an object in an object (quaint) 
*
* @param obj_data *obj The item to put
* @param obj_data *obj_to What to put it into
*/
void obj_to_obj(obj_data *obj, obj_data *obj_to) {
	if (!obj || !obj_to || obj == obj_to) {
		log("SYSERR: NULL object (%p) or same source (%p) and target (%p) obj passed to obj_to_obj.", obj, obj, obj_to);
	}
	else {
		check_obj_in_void(obj);
	
		GET_OBJ_CARRYING_N(obj_to) += obj_carry_size(obj);

		// set the timer here; actual rules for it are in limits.c
		GET_AUTOSTORE_TIMER(obj) = time(0);
		
		// clear keep now
		REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);

		obj->next_content = obj_to->contains;
		obj_to->contains = obj;
		obj->in_obj = obj_to;
	}
}


/**
* put an object in a room
*
* @param obj_data *object The item to place.
* @param room_data *room Where to place it.
*/
void obj_to_room(obj_data *object, room_data *room) {
	if (!object || !room) {
		log("SYSERR: Illegal value(s) passed to obj_to_room. (Room %p, obj %p)", room, object);
	}
	else {
		check_obj_in_void(object);
		object->next_content = ROOM_CONTENTS(room);
		ROOM_CONTENTS(room) = object;
		IN_ROOM(object) = room;
		object->carried_by = NULL;
		
		// check light
		if (OBJ_FLAGGED(object, OBJ_LIGHT)) {
			ROOM_LIGHTS(IN_ROOM(object))++;
		}
		
		// clear keep now
		REMOVE_BIT(GET_OBJ_EXTRA(object), OBJ_KEEP);

		// set the timer here; actual rules for it are in limits.c
		GET_AUTOSTORE_TIMER(object) = time(0);
	}
}


/**
* Put an object into a vehicle.
*
* @param obj_data *object The object.
* @param vehicle_data *veh The vehicle to put it in.
*/
void obj_to_vehicle(obj_data *object, vehicle_data *veh) {
	if (!object || !veh) {
		log("SYSERR: Illegal value(s) passed to obj_to_vehicle. (Vehicle %p, obj %p)", veh, object);
	}
	else {
		check_obj_in_void(object);
		
		LL_PREPEND2(VEH_CONTAINS(veh), object, next_content);
		object->in_vehicle = veh;
		VEH_CARRYING_N(veh) += obj_carry_size(object);
		
		// clear keep now
		REMOVE_BIT(GET_OBJ_EXTRA(object), OBJ_KEEP);
		
		// set the timer here; actual rules for it are in limits.c
		VEH_LAST_MOVE_TIME(veh) = GET_AUTOSTORE_TIMER(object) = time(0);
	}
}


/**
* This will put a new object wherever an old object was (in obj, in room, etc)
* and will leave the old object nowhere. This is called e.g. to replace an
* item with a fresh copy of that item.
*
* @param obj_data *old The old item, being replaced.
* @param obj_data *new The new item.
*/
void swap_obj_for_obj(obj_data *old, obj_data *new) {
	char_data *bearer;
	int pos;
	
	// make sure the new obj isn't somewhere
	check_obj_in_void(new);
	
	if (old->carried_by) {
		obj_to_char(new, old->carried_by);
	}
	else if (old->in_vehicle) {
		obj_to_vehicle(new, old->in_vehicle);
	}
	else if (IN_ROOM(old)) {
		obj_to_room(new, IN_ROOM(old));
	}
	else if (old->in_obj) {
		obj_to_obj(new, old->in_obj);
	}
	else if (old->worn_by) {
		pos = old->worn_on;
		bearer = old->worn_by;
		
		unequip_char(bearer, pos);
		equip_char(bearer, new, pos);
	}
	
	// move contents
	new->contains = old->contains;
	old->contains = NULL;
	GET_OBJ_CARRYING_N(new) = GET_OBJ_CARRYING_N(old);
	GET_OBJ_CARRYING_N(old) = 0;
	check_obj_in_void(old);
}


/**
* Remove whatever ch has equipped in pos, and return the item.
*
* @param char_data *ch The person to unequip.
* @param int pos The WEAR_x slot to remove.
* @return obj_data *The removed item, or NULL if there was none.
*/
obj_data *unequip_char(char_data *ch, int pos) {	
	struct obj_apply *apply;
	obj_data *obj = NULL;

	if ((pos >= 0 && pos < NUM_WEARS) && GET_EQ(ch, pos) != NULL) {
		obj = GET_EQ(ch, pos);
		obj->worn_by = NULL;
		obj->worn_on = NO_WEAR;

		// adjust lights
		if (IN_ROOM(ch) && OBJ_FLAGGED(obj, OBJ_LIGHT)) {
			ROOM_LIGHTS(IN_ROOM(ch))--;
		}

		// actual remove
		GET_EQ(ch, pos) = NULL;

		// un-apply affects
		if (wear_data[pos].count_stats) {
			for (apply = GET_OBJ_APPLIES(obj); apply; apply = apply->next) {
				affect_modify(ch, apply->location, apply->modifier, NOBITS, FALSE);
			}
			if (GET_OBJ_AFF_FLAGS(obj)) {
				affect_modify(ch, APPLY_NONE, 0, GET_OBJ_AFF_FLAGS(obj), FALSE);
			}
		}

		affect_total(ch);
		qt_remove_obj(ch, obj);
	}

	return obj;
}


/**
* Calls unequip_char() and then gives it to the character if it's not 1-use.
* This also handles single-use items by extracting them.
*
* @param char_data *ch The person to unequip
* @param int pos The WEAR_x slot to remove
* @return obj_data* A pointer to the object removed IF it wasn't extracted.
*/
obj_data *unequip_char_to_inventory(char_data *ch, int pos) {
	obj_data *obj = unequip_char(ch, pos);
	
	if (OBJ_FLAGGED(obj, OBJ_SINGLE_USE)) {
		extract_obj(obj);
	}
	else {
		obj_to_char(obj, ch);
		return obj;
	}
	
	return NULL;
}


/**
* Calls unequip_char() and then drops it in the room if it's not 1-use.
* Extracts it if it IS single-use.
*
* @param char_data *ch The person to unequip
* @param int pos The WEAR_x position to remove
* @return obj_data* A pointer to the obj IF it wasn't extracted.
*/
obj_data *unequip_char_to_room(char_data *ch, int pos) {
	obj_data *obj = unequip_char(ch, pos);
	
	if (OBJ_FLAGGED(obj, OBJ_SINGLE_USE)) {
		extract_obj(obj);
	}
	else if (IN_ROOM(ch)) {
		obj_to_room(obj, IN_ROOM(ch));
		return obj;
	}
	
	return NULL;
}


 //////////////////////////////////////////////////////////////////////////////
//// CUSTOM MESSAGE HANDLERS /////////////////////////////////////////////////

/**
* Duplicates a list of custom messages.
*
* @param struct custom_message *from The list to copy.
* @return struct custom_message* The copied list.
*/
struct custom_message *copy_custom_messages(struct custom_message *from) {
	struct custom_message *list = NULL, *mes, *iter;
	
	LL_FOREACH(from, iter) {
		CREATE(mes, struct custom_message, 1);
		
		mes->type = iter->type;
		mes->msg = iter->msg ? str_dup(iter->msg) : NULL;
		
		LL_APPEND(list, mes);
	}
	
	return list;
}


/**
* Frees a list of custom messages.
*
* @param struct custom_message *mes The list to free.
*/
void free_custom_messages(struct custom_message *mes) {
	struct custom_message *iter, *next;
	
	LL_FOREACH_SAFE(mes, iter, next) {
		if (iter->msg) {
			free(iter->msg);
		}
		free(iter);
	}
}


/**
* This gets a custom message of a given type for an object. If there is more
* than one message of the requested type, it returns one at random. You will
* get back a null if there are no messages of the requested type; you can check
* this ahead of time with has_custom_message().
*
* @param obj_data *obj The object.
* @param int type The OBJ_CUSTOM_x type of message.
* @return char* The custom message, or NULL if there is none.
*/
char *get_custom_message(obj_data *obj, int type) {
	struct custom_message *ocm;
	char *found = NULL;
	int num_found = 0;
	
	for (ocm = obj->custom_msgs; ocm; ocm = ocm->next) {
		if (ocm->type == type) {
			if (!number(0, num_found++) || !found) {
				found = ocm->msg;
			}
		}
	}
	
	return found;
}


/**
* @param obj_data *obj The object to check.
* @param int type Any OBJ_CUSTOM_x type.
* @return bool TRUE if the object has at least one message of the requested type.
*/
bool has_custom_message(obj_data *obj, int type) {
	struct custom_message *ocm;
	bool found = FALSE;
	
	for (ocm = obj->custom_msgs; ocm && !found; ocm = ocm->next) {
		if (ocm->type == type) {
			found = TRUE;
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT TARGETING HANDLERS ///////////////////////////////////////////////

/**
* Find an object in another person's share slot, by character name.
*
* @param char_data *ch The person looking for a shared obj.
* @param char *arg The potential name of a PLAYER.
*/
obj_data *get_obj_by_char_share(char_data *ch, char *arg) {
	char_data *targ;
	
	// find person by name
	if (!(targ = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		return NULL;
	}
	
	return GET_EQ(targ, WEAR_SHARE);
}


/**
* Finds a matching object in a character's equipment.
*
* @param char_data *ch The person who is looking...
* @param char *arg The typed-in arg.
* @param obj_data *equipment[] The character's EQ array.
* @return obj_data *The found object if any match, or NULL.
*/
obj_data *get_obj_in_equip_vis(char_data *ch, char *arg, obj_data *equipment[]) {
	int j, num;

	num = get_number(&arg);

	if (num == 0)
		return (NULL);

	for (j = 0; j < NUM_WEARS; j++)
		if (equipment[j] && CAN_SEE_OBJ(ch, equipment[j]) && isname(arg, equipment[j]->name))
			if (--num == 0)
				return (equipment[j]);

	return (NULL);
}


/**
* Search a given list for an object number, and return a ptr to that obj
*
* @param obj_vnum num The obj vnum to find
* @param obj_data *list The start of any object list
* @return obj_data *The first matching object in the list, if any
*/
obj_data *get_obj_in_list_num(obj_vnum num, obj_data *list) {
	obj_data *i, *found = NULL;

	for (i = list; i && !found; i = i->next_content) {
		if (GET_OBJ_VNUM(i) == num) {
			found = i;
		}
	}

	return found;
}


/**
* @param obj_vnum vnum The vnum to find.
* @param obj_data *list The start of any object list
* @return obj_data *The first matching object in the list, if any
*/
obj_data *get_obj_in_list_vnum(obj_vnum vnum, obj_data *list) {
	obj_data *i, *found = NULL;

	for (i = list; i && !found; i = i->next_content) {
		if (GET_OBJ_VNUM(i) == vnum) {
			found = i;
		}
	}

	return found;
}


/**
* Finds and object the char can see in any list (ch->carrying, etc).
*
* @param char_data *ch The person who's looking.
* @param char *name The target argument.
* @param obj_data *list The list to search.
* @return obj_data *The item found, or NULL.
*/
obj_data *get_obj_in_list_vis(char_data *ch, char *name, obj_data *list) {	
	obj_data *i, *found = NULL;
	int j = 0, number;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname;

	strcpy(tmp, name);
	
	// 0.x does not target items
	if ((number = get_number(&tmp)) == 0) {
		return (NULL);
	}

	for (i = list; i && (j <= number) && !found; i = i->next_content) {
		if (CAN_SEE_OBJ(ch, i) && MATCH_ITEM_NAME(tmp, i)) {
			if (++j == number) {
				found = i;
			}
		}
	}

	return found;
}


/**
* Gets the position of a piece of equipment the character is using, by name.
*
* @param char_data *ch The person who's looking.
* @param char *arg The typed argument (item name).
* @param obj_data *equipment[] The character's gear array.
* @return int The WEAR_x position, or NO_WEAR if no match was found.
*/
int get_obj_pos_in_equip_vis(char_data *ch, char *arg, obj_data *equipment[]) {
	int j, num;

	num = get_number(&arg);

	if (num == 0)
		return NO_WEAR;

	for (j = 0; j < NUM_WEARS; j++)
		if (equipment[j] && CAN_SEE_OBJ(ch, equipment[j]) && isname(arg, equipment[j]->name))
			if (--(num) == 0)
				return (j);

	return NO_WEAR;
}


/**
* Uses multi_isname to match any number of name args.
*
* @param char *name The search name.
* @param bool storable_only If TRUE, only storable items will be found.
* @return obj_vnum A matching object, or NOTHING.
*/
obj_vnum get_obj_vnum_by_name(char *name, bool storable_only) {
	obj_data *obj, *next_obj;
	
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (storable_only && !obj->storage) {
			continue;
		}
		if (!multi_isname(name, GET_OBJ_KEYWORDS(obj))) {
			continue;
		}
		
		// found!
		return GET_OBJ_VNUM(obj);
	}
	
	return NOTHING;
}


/**
* search the entire world for an object, and return a pointer
*
* @param char_data *ch The person who is looking for an item.
* @param char *name The target argument.
* @return obj_data *The found item, or NULL.
*/
obj_data *get_obj_vis(char_data *ch, char *name) {
	obj_data *i, *found = NULL;
	int j = 0, number;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname;

	/* scan items carried */
	if ((i = get_obj_in_list_vis(ch, name, ch->carrying)) != NULL)
		return (i);

	/* scan room */
	if ((i = get_obj_in_list_vis(ch, name, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL)
		return (i);

	strcpy(tmp, name);
	if ((number = get_number(&tmp)) == 0)
		return (NULL);

	/* ok.. no luck yet. scan the entire obj list   */
	for (i = object_list; i && (j <= number) && !found; i = i->next) {
		if (CAN_SEE_OBJ(ch, i) && MATCH_ITEM_NAME(tmp, i)) {
			if (++j == number) {
				found = i;
			}
		}
	}

	return found;
}


/**
* Finds a matching object in an array of equipped gear. If no objects is found,
* the "pos" variable is set to NOTHING.
*
* @param char_data *ch The person who's looking for an item.
* @param char *arg The target argument.
* @param obj_data *equipment[] A pointer to an equipment array.
* @param int *pos A variable to store the WEAR_x const if an item is found.
* @return obj_data *The found object, or NULL.
*/
obj_data *get_object_in_equip_vis(char_data *ch, char *arg, obj_data *equipment[], int *pos) {
	obj_data *found = NULL;
	int iter;
	
	*pos = NOTHING;
	
	for (iter = 0; iter < NUM_WEARS && !found; ++iter) {
		if (equipment[iter] && CAN_SEE_OBJ(ch, equipment[iter]) && MATCH_ITEM_NAME(arg, equipment[iter])) {
			found = equipment[iter];
			*pos = iter;
		}
	}

	return found;
}


/**
* search the entire world for an object, and return a pointer -- without
* regard to visibility.
*
* @param char *name The target argument.
* @return obj_data *The found item, or NULL.
*/
obj_data *get_obj_world(char *name) {
	obj_data *i, *found = NULL;
	int j = 0, number;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname;

	strcpy(tmp, name);
	if ((number = get_number(&tmp)) == 0)
		return (NULL);

	for (i = object_list; i && (j <= number) && !found; i = i->next) {
		if (MATCH_ITEM_NAME(tmp, i)) {
			if (++j == number) {
				found = i;
			}
		}
	}

	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OFFER HANDLERS //////////////////////////////////////////////////////////

/**
* Adds an offer to ch. This is used by do_accept. This function does not send
* messages. This will overwrite a previous identical entry.
*
* @param char_data *ch The person who is getting an offer.
* @param char_data *from The person sending the offer.
* @param int type Any OFFER_ type.
* @param int data A misc integer that may be passed based on type (use 0 for none).
* @return struct offer_data* A pointer to the attached new offer, if it succeeds.
*/
struct offer_data *add_offer(char_data *ch, char_data *from, int type, int data) {
	struct offer_data *iter, *offer = NULL;
	
	if (!ch || !from || IS_NPC(ch) || IS_NPC(from)) {
		return NULL;
	}
	
	// ensure no existing offer (overwrite if so)
	for (iter = GET_OFFERS(ch); iter; iter = iter->next) {
		if (iter->type == type && iter->from == GET_IDNUM(from)) {
			offer = iter;
			break;
		}
	}
	
	if (!offer) {
		CREATE(offer, struct offer_data, 1);
		offer->next = GET_OFFERS(ch);
		GET_OFFERS(ch) = offer;
	}
	
	offer->from = GET_IDNUM(from);
	offer->type = type;
	offer->location = IN_ROOM(from) ? GET_ROOM_VNUM(IN_ROOM(from)) : NOWHERE;
	offer->time = time(0);
	offer->data = data;
	
	return offer;
}


/**
* Removes any expired offers the character may have.
*
* @param char_data *ch The player to clean up offers for.
*/
void clean_offers(char_data *ch) {
	struct offer_data *offer, *next_offer, *temp;
	int max_duration = config_get_int("offer_time");
	
	if (!ch || IS_NPC(ch)) {
		return;
	}
	
	for (offer = GET_OFFERS(ch); offer; offer = next_offer) {
		next_offer = offer->next;
		
		if (time(0) - offer->time > max_duration) {
			REMOVE_FROM_LIST(offer, GET_OFFERS(ch), next);
			free(offer);
		}
	}
}


/**
* Removes all offers of a given type.
*
* @param char_data *ch The person whose offers to remove.
* @param int type Any OFFER_ type.
*/
void remove_offers_by_type(char_data *ch, int type) {
	struct offer_data *offer, *next_offer, *temp;
	
	if (!ch || IS_NPC(ch)) {
		return;
	}
	
	for (offer = GET_OFFERS(ch); offer; offer = next_offer) {
		next_offer = offer->next;
		
		if (offer->type == type) {
			REMOVE_FROM_LIST(offer, GET_OFFERS(ch), next);
			free(offer);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// REQUIREMENT HANDLERS ////////////////////////////////////////////////////


/**
* @param struct req_data *from The list to copy.
* @return struct req_data* The copy of the list.
*/
struct req_data *copy_requirements(struct req_data *from) {
	struct req_data *el, *iter, *list = NULL, *end = NULL;
	
	LL_FOREACH(from, iter) {
		CREATE(el, struct req_data, 1);
		*el = *iter;
		el->next = NULL;
		
		if (end) {
			end->next = el;
		}
		else {
			list = el;
		}
		end = el;
	}
	
	return list;
}


/**
* Deletes entries by type+vnum.
*
* @param struct req_data **list A pointer to the list to delete from.
* @param int type REQ_ type.
* @param any_vnum vnum The vnum to remove.
* @return bool TRUE if the type+vnum was removed from the list. FALSE if not.
*/
bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum) {
	struct req_data *iter, *next_iter;
	bool any = FALSE;
	
	LL_FOREACH_SAFE(*list, iter, next_iter) {
		if (iter->type == type && iter->vnum == vnum) {
			any = TRUE;
			LL_DELETE(*list, iter);
			free(iter);
		}
	}
	
	return any;
}


/**
* Extracts items from a character, based on a list of requirements (e.g. quest
* tasks). This function does NOT error if the character is missing some of the
* items; it only removes them if present.
*
* @param char_data *ch The character losing items.
* @param struct req_data *list The items to lose (other task types are ignored).
*/
void extract_required_items(char_data *ch, struct req_data *list) {
	// helper type
	struct extract_items_data {
		int group;	// cast from char
		int complete;	// number to do
		int tasks;	// total tasks
		UT_hash_handle hh;
	};
	
	struct extract_items_data *eid, *next_eid, *eid_list = NULL;
	struct req_data *req, *found_req = NULL;
	struct resource_data *res = NULL;
	bool done = FALSE;
	int group, which;
	
	// build a list of which tasks might be complete
	LL_FOREACH(list, req) {
		if (!req->group) {	// ungrouped requirement
			if (req->current >= req->needed) {
				// complete!
				found_req = req;
				done = TRUE;
				break;
			}
		}
		else {	// grouped req
			// find or add data
			group = (int)req->group;
			HASH_FIND_INT(eid_list, &group, eid);
			if (!eid) {
				CREATE(eid, struct extract_items_data, 1);
				eid->group = group;
				HASH_ADD_INT(eid_list, group, eid);
			}
			
			// compute data
			eid->tasks += 1;
			if (req->current >= req->needed) {
				eid->complete += 1;
			}
		}
	}
	
	// figure out which group
	which = -1;
	HASH_ITER(hh, eid_list, eid, next_eid) {
		if (which == -1 && eid->complete >= eid->tasks) {
			which = eid->group;
		}
		
		// free data now
		free(eid);
	}
	
	// now that we know what to extract
	LL_FOREACH(list, req) {
		// is this one we extract from?
		if (done && found_req != req) {
			continue;
		}
		else if (!done && which != (int)req->group) {
			continue;
		}
		
		// REQ_x: types that are extractable
		switch (req->type) {
			case REQ_GET_COMPONENT: {
				add_to_resource_list(&res, RES_COMPONENT, req->vnum, req->needed, req->misc);
				break;
			}
			case REQ_GET_OBJECT: {
				add_to_resource_list(&res, RES_OBJECT, req->vnum, req->needed, 0);
				break;
			}
		}
	}
	
	if (res) {
		extract_resources(ch, res, FALSE, NULL);
		free_resource_list(res);
	}
}


/**
* @param struct req_data *list A list to search.
* @param int type REQ_ type.
* @param any_vnum vnum The vnum to look for.
* @return bool TRUE if the type+vnum is in the list. FALSE if not.
*/
bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum) {
	struct req_data *iter;
	LL_FOREACH(list, iter) {
		if (iter->type == type && iter->vnum == vnum) {
			return TRUE;
		}
	}
	return FALSE;
}


/**
* @param struct req_data *list The list to free.
*/
void free_requirements(struct req_data *list) {
	struct req_data *iter, *next_iter;
	LL_FOREACH_SAFE(list, iter, next_iter) {
		free(iter);
	}
}


/**
* Determines if a character meets a set of requirements. This only works on
* requirements that can be determined in realtime, not ones that require quest
* trackers.
*
* @param char_data *ch The character to check.
* @param struct req_data *list The list of requirements.
* @param struct instance_data *instance Optional: A related instance (e.g. for quests that only check REQ_COMPLETED_QUEST on the same instance; may be NULL).
* @return bool TRUE if the character meets those requirements, FALSE if not.
*/
bool meets_requirements(char_data *ch, struct req_data *list, struct instance_data *instance) {
	extern int count_owned_buildings(empire_data *emp, bld_vnum vnum);
	extern int count_owned_vehicles(empire_data *emp, any_vnum vnum);
	extern struct player_completed_quest *has_completed_quest(char_data *ch, any_vnum quest, int instance_id);
	extern struct player_quest *is_on_quest(char_data *ch, any_vnum quest);
	
	// helper struct
	struct meets_req_data {
		int group;	// actually a char, but cast
		bool ok;	// good until failed
		UT_hash_handle hh;
	};
	
	struct meets_req_data *mrd, *next_mrd, *mrd_list = NULL;
	bool global_ok = FALSE, ok;
	struct req_data *req;
	int group;
	
	// shortcut
	if (!list) {
		return TRUE;
	}
	
	LL_FOREACH(list, req) {
		// first look up or create data for this group
		group = req->group;
		HASH_FIND_INT(mrd_list, &group, mrd);
		if (!mrd) {
			CREATE(mrd, struct meets_req_data, 1);
			mrd->group = (int)req->group;
			mrd->ok = TRUE;	// default
			HASH_ADD_INT(mrd_list, group, mrd);
		}
		
		// shortcut if the group already failed (group=none skips this because it's an "or")
		if (mrd->group && !mrd->ok) {
			continue;
		}
		
		// alright, true unless proven otherwise
		ok = TRUE;
		
		// REQ_x: only requirements that can be prereqs (don't require a tracker)
		switch(req->type) {
			case REQ_COMPLETED_QUEST: {
				if (!has_completed_quest(ch, req->vnum, instance ? instance->id : NOTHING)) {
					ok = FALSE;
				}
				break;
			}
			case REQ_GET_COMPONENT: {
				struct resource_data *res = NULL;
				add_to_resource_list(&res, RES_COMPONENT, req->vnum, req->needed, req->misc);
				if (!has_resources(ch, res, FALSE, FALSE)) {
					ok = FALSE;
				}
				free_resource_list(res);
				break;
			}
			case REQ_GET_OBJECT: {
				struct resource_data *res = NULL;
				add_to_resource_list(&res, RES_OBJECT, req->vnum, req->needed, 0);
				if (!has_resources(ch, res, FALSE, FALSE)) {
					ok = FALSE;
				}
				free_resource_list(res);
				break;
			}
			case REQ_NOT_COMPLETED_QUEST: {
				if (has_completed_quest(ch, req->vnum, instance ? instance->id : NOTHING)) {
					ok = FALSE;
				}
				break;
			}
			case REQ_NOT_ON_QUEST: {
				if (is_on_quest(ch, req->vnum)) {
					ok = FALSE;
				}
				break;
			}
			case REQ_OWN_BUILDING: {
				if (!GET_LOYALTY(ch) || count_owned_buildings(GET_LOYALTY(ch), req->vnum) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_OWN_VEHICLE: {
				if (!GET_LOYALTY(ch) || count_owned_vehicles(GET_LOYALTY(ch), req->vnum) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_SKILL_LEVEL_OVER: {
				if (get_skill_level(ch, req->vnum) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_SKILL_LEVEL_UNDER: {
				if (get_skill_level(ch, req->vnum) > req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_HAVE_ABILITY: {
				if (!has_ability(ch, req->vnum)) {
					ok = FALSE;
				}
				break;
			}
			case REQ_REP_OVER: {
				struct player_faction_data *pfd = get_reputation(ch, req->vnum, FALSE);
				faction_data *fct = find_faction_by_vnum(req->vnum);
				if (compare_reptuation((pfd ? pfd->rep : (fct ? FCT_STARTING_REP(fct) : REP_NEUTRAL)), req->needed) < 0) {
					ok = FALSE;
				}
				break;
			}
			case REQ_REP_UNDER: {
				struct player_faction_data *pfd = get_reputation(ch, req->vnum, FALSE);
				faction_data *fct = find_faction_by_vnum(req->vnum);
				if (compare_reptuation((pfd ? pfd->rep : (fct ? FCT_STARTING_REP(fct) : REP_NEUTRAL)), req->needed) > 0) {
					ok = FALSE;
				}
				break;
			}
			case REQ_WEARING: {
				bool found = FALSE;
				int iter;
				
				for (iter = 0; iter < NUM_WEARS; ++iter) {
					if (GET_EQ(ch, iter) && GET_OBJ_VNUM(GET_EQ(ch, iter)) == req->vnum) {
						found = TRUE;
						break;
					}
				}
				
				if (!found) {
					ok = FALSE;
				}
				
				break;
			}
			case REQ_WEARING_OR_HAS: {
				struct resource_data *res = NULL;
				bool found = FALSE;
				int iter;
				
				for (iter = 0; iter < NUM_WEARS; ++iter) {
					if (GET_EQ(ch, iter) && GET_OBJ_VNUM(GET_EQ(ch, iter)) == req->vnum) {
						found = TRUE;
						break;
					}
				}
				
				if (!found) {
					// check inventory
					add_to_resource_list(&res, RES_OBJECT, req->vnum, req->needed, 0);
					if (!has_resources(ch, res, FALSE, FALSE)) {
						ok = FALSE;
					}
					free_resource_list(res);
				}
				
				break;
			}
			
			// some types do not support pre-reqs
			case REQ_KILL_MOB:
			case REQ_KILL_MOB_FLAGGED:
			case REQ_TRIGGERED:
			case REQ_VISIT_BUILDING:
			case REQ_VISIT_ROOM_TEMPLATE:
			case REQ_VISIT_SECTOR:
			default: {
				break;
			}
		}	// end switch
		
		if (!ok) {	// did we survive the switch?
			mrd->ok = FALSE;
		}
		else if (ok && !mrd->group) {	// the non-grouped conditions are "OR"s
			global_ok = TRUE;
			break;	// exit early
		}
	}
	
	if (!global_ok) {	// did any sub-groups succeed?
		HASH_ITER(hh, mrd_list, mrd, next_mrd) {
			if (mrd->ok && mrd->group) {	// only grouped requirements count here
				global_ok = TRUE;
			}
			
			// free memory
			free(mrd);
		}
	}
	
	return global_ok;
}


/**
* Gets standard string display like "4x lumber" for a requirement (e.g. a
* quest task).
*
* @param struct req_data *req The requirement to show.
* @param bool show_vnums If TRUE, adds [1234] at the start of the string.
* @return char* The string display.
*/
char *requirement_string(struct req_data *req, bool show_vnums) {
	extern const char *action_bits[];
	
	char vnum[256], lbuf[256];
	static char output[256];
	
	*output = '\0';
	if (!req) {
		return output;
	}
	
	if (show_vnums) {
		snprintf(vnum, sizeof(vnum), "[%d] ", req->vnum);
	}
	else {
		*vnum = '\0';
	}
	
	// REQ_x
	switch (req->type) {
		case REQ_COMPLETED_QUEST: {
			snprintf(output, sizeof(output), "Complete quest: %s%s", vnum, get_quest_name_by_proto(req->vnum));
			break;
		}
		case REQ_GET_COMPONENT: {
			snprintf(output, sizeof(output), "Get component%s: %dx %s%s", PLURAL(req->needed), req->needed, vnum, component_string(req->vnum, req->misc));
			break;
		}
		case REQ_GET_OBJECT: {
			snprintf(output, sizeof(output), "Get object%s: %dx %s%s", PLURAL(req->needed), req->needed, vnum, get_obj_name_by_proto(req->vnum));
			break;
		}
		case REQ_KILL_MOB: {
			snprintf(output, sizeof(output), "Kill %dx mob%s: %s%s", req->needed, PLURAL(req->needed), vnum, get_mob_name_by_proto(req->vnum));
			break;
		}
		case REQ_KILL_MOB_FLAGGED: {
			sprintbit(req->misc, action_bits, lbuf, TRUE);
			// does not show vnum
			snprintf(output, sizeof(output), "Kill %dx mob%s flagged: %s", req->needed, PLURAL(req->needed), lbuf);
			break;
		}
		case REQ_NOT_COMPLETED_QUEST: {
			snprintf(output, sizeof(output), "Not completed quest %s%s", vnum, get_quest_name_by_proto(req->vnum));
			break;
		}
		case REQ_NOT_ON_QUEST: {
			snprintf(output, sizeof(output), "Not on quest %s%s", vnum, get_quest_name_by_proto(req->vnum));
			break;
		}
		case REQ_OWN_BUILDING: {
			bld_data *bld = building_proto(req->vnum);
			snprintf(output, sizeof(output), "Own %dx building%s: %s%s", req->needed, PLURAL(req->needed), vnum, bld ? GET_BLD_NAME(bld) : "UNKNOWN");
			break;
		}
		case REQ_OWN_VEHICLE: {
			snprintf(output, sizeof(output), "Own %dx vehicle%s: %s%s", req->needed, PLURAL(req->needed), vnum, get_vehicle_name_by_proto(req->vnum));
			break;
		}
		case REQ_SKILL_LEVEL_OVER: {
			snprintf(output, sizeof(output), "%s%s at least %d", vnum, get_skill_name_by_vnum(req->vnum), req->needed);
			break;
		}
		case REQ_SKILL_LEVEL_UNDER: {
			snprintf(output, sizeof(output), "%s%s not over %d", vnum, get_skill_name_by_vnum(req->vnum), req->needed);
			break;
		}
		case REQ_TRIGGERED: {
			strcpy(output, "Scripted condition");
			break;
		}
		case REQ_VISIT_BUILDING: {
			bld_data *bld = building_proto(req->vnum);
			snprintf(output, sizeof(output), "Visit building: %s%s", vnum, bld ? GET_BLD_NAME(bld) : "UNKNOWN");
			break;
		}
		case REQ_VISIT_ROOM_TEMPLATE: {
			room_template *rmt = room_template_proto(req->vnum);
			snprintf(output, sizeof(output), "Visit location: %s%s", vnum, rmt ? GET_RMT_TITLE(rmt) : "UNKNOWN");
			break;
		}
		case REQ_VISIT_SECTOR: {
			sector_data *sect = sector_proto(req->vnum);
			snprintf(output, sizeof(output), "Visit terrain: %s%s", vnum, sect ? GET_SECT_NAME(sect) : "UNKNOWN");
			break;
		}
		case REQ_HAVE_ABILITY: {
			snprintf(output, sizeof(output), "Have ability: %s%s", vnum, get_ability_name_by_vnum(req->vnum));
			break;
		}
		case REQ_REP_OVER: {
			snprintf(output, sizeof(output), "%s%s at least %s", vnum, get_faction_name_by_vnum(req->vnum), get_reputation_name(req->needed));
			break;
		}
		case REQ_REP_UNDER: {
			snprintf(output, sizeof(output), "%s%s not over %s", vnum, get_faction_name_by_vnum(req->vnum), get_reputation_name(req->needed));
			break;
		}
		case REQ_WEARING: {
			snprintf(output, sizeof(output), "Wearing object: %s%s", vnum, get_obj_name_by_proto(req->vnum));
			break;
		}
		case REQ_WEARING_OR_HAS: {
			snprintf(output, sizeof(output), "Wearing or has object: %s%s", vnum, get_obj_name_by_proto(req->vnum));
			break;
		}
		default: {
			sprintf(buf, "Unknown condition");
			break;
		}
	}
	
	if (show_vnums && req->group) {
		snprintf(output + strlen(output), sizeof(output) - strlen(output), " (%c)", req->group);
	}
	
	return output;
}


 //////////////////////////////////////////////////////////////////////////////
//// RESOURCE DEPLETION HANDLERS /////////////////////////////////////////////


/**
* Add to the room's depletion counter
*
* @param room_data *room which location e.g. IN_ROOM(ch)
* @param int type DPLTN_
* @param bool multiple if TRUE, chance to add more than 1
*/
void add_depletion(room_data *room, int type, bool multiple) {
	struct depletion_data *dep;
	bool found = FALSE;
	
	for (dep = ROOM_DEPLETION(room); dep && !found; dep = dep->next) {
		if (dep->type == type) {
			dep->count += 1 + ((multiple && !number(0, 3)) ? 1 : 0);
			found = TRUE;
		}
	}
	
	if (!found) {
		CREATE(dep, struct depletion_data, 1);
		dep->type = type;
		dep->count = 1 + ((multiple && !number(0, 3)) ? 1 : 0);;
		
		dep->next = ROOM_DEPLETION(room);
		ROOM_DEPLETION(room) = dep;
	}
}


/**
* @param room_data *room which location e.g. IN_ROOM(ch)
* @param int type DPLTN_
* @return int The depletion counter on that resource in that room.
*/
int get_depletion(room_data *room, int type) {
	struct depletion_data *dep;
	int found = 0;
	
	for (dep = ROOM_DEPLETION(room); dep && !found; dep = dep->next) {
		if (dep->type == type) {
			found = dep->count;
		}
	}
	
	return found;
}


/**
* Removes all depletion data for a certain type from the room.
*
* @param room_data *room where
* @param int type DPLTN_
*/
void remove_depletion(room_data *room, int type) {
	struct depletion_data *dep, *next_dep, *temp;
	
	for (dep = ROOM_DEPLETION(room); dep; dep = next_dep) {
		next_dep = dep->next;
		
		if (dep->type == type) {
			REMOVE_FROM_LIST(dep, ROOM_DEPLETION(room), next);
		}
	}
}


/**
* Sets a room's depletion to a specific value.
*
* @param room_data *room The room to set depletion on.
* @param int type DPLTN_ const
* @param int value How much to set the depletion to.
*/
void set_depletion(room_data *room, int type, int value) {
	struct depletion_data *dep;
	bool found = FALSE;
	
	// shortcut
	if (value <= 0) {
		remove_depletion(room, type);
		return;
	}
	
	// existing?
	LL_FOREACH(ROOM_DEPLETION(room), dep) {
		if (dep->type == type) {
			dep->count = value;
			found = TRUE;
			break;
		}
	}
	
	// add
	if (!found) {
		CREATE(dep, struct depletion_data, 1);
		dep->type = type;
		dep->count = value;
		
		LL_PREPEND(ROOM_DEPLETION(room), dep);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ROOM HANDLERS ///////////////////////////////////////////////////////////

/**
* Sets the building data on a room. If the room isn't already complex, this
* will automatically add complex data. This should always be called with
* triggers unless you're loading saved rooms from a file, or some other place
* where triggers might have been detached.
*
* @param bld_data *bld The building prototype (from building_table).
* @param room_data *room The world room to attach it to.
* @param bool with_triggers If TRUE, attaches triggers too.
*/
void attach_building_to_room(bld_data *bld, room_data *room, bool with_triggers) {
	if (!bld || !room) {
		log("SYSERR: attach_building_to_room called without %s", bld ? "room" : "building");
		return;
	}
	if (!COMPLEX_DATA(room)) {
		COMPLEX_DATA(room) = init_complex_data();
	}
	COMPLEX_DATA(room)->bld_ptr = bld;

	// copy proto script
	if (with_triggers) {
		struct trig_proto_list *temp;
		if ((temp = copy_trig_protos(GET_BLD_SCRIPTS(bld)))) {
			LL_CONCAT(room->proto_script, temp);
		}
		assign_triggers(room, WLD_TRIGGER);
	}
}


/**
* Sets the room template data on a room. If the room isn't already complex, 
* this will automatically add complex data.
*
* @param room_template *rmt The room template prototype (from room_template_table).
* @param room_data *room The world room to attach it to.
*/
void attach_template_to_room(room_template *rmt, room_data *room) {
	if (!rmt || !room) {
		return;
	}
	if (!COMPLEX_DATA(room)) {
		COMPLEX_DATA(room) = init_complex_data();
	}
	COMPLEX_DATA(room)->rmt_ptr = rmt;
}


/**
* Sets the building data on a room. If the room isn't already complex, this
* will automatically add complex data. This should always be called with
* triggers unless you're loading saved rooms from a file, or some other place
* where triggers might have been detached.
*
* @param bld_data *bld The building prototype (from building_table).
* @param room_data *room The world room to attach it to.
* @param bool with_triggers If TRUE, attaches triggers too.
*/
void detach_building_from_room(room_data *room) {
	struct trig_proto_list *tpl, *next_tpl, *search;
	trig_data *trig, *next_trig;
	bld_data *bld;
	bool any;
	
	if (!room) {
		log("SYSERR: detach_building_from_room called without room");
		return;
	}
	if (!COMPLEX_DATA(room) || !(bld = COMPLEX_DATA(room)->bld_ptr)) {
		return;	// nothing to do
	}
	
	COMPLEX_DATA(room)->bld_ptr = NULL;
	LL_FOREACH_SAFE(room->proto_script, tpl, next_tpl) {
		LL_SEARCH_SCALAR(GET_BLD_SCRIPTS(bld), search, vnum, tpl->vnum);
		if (search) {	// matching vnum on the proto
			LL_DELETE(room->proto_script, tpl);
			free(tpl);
		}
	}
	
	if (SCRIPT(room)) {
		any = FALSE;
		LL_FOREACH_SAFE(TRIGGERS(SCRIPT(room)), trig, next_trig) {
			LL_SEARCH_SCALAR(GET_BLD_SCRIPTS(bld), search, vnum, GET_TRIG_VNUM(trig));
			if (search) {	// matching vnum on the proto
				LL_DELETE(TRIGGERS(SCRIPT(room)), trig);
				extract_trigger(trig);
				any = TRUE;
			}
		}
		
		if (any) {	// update script types
			SCRIPT_TYPES(SCRIPT(room)) = 0;
			LL_FOREACH(TRIGGERS(SCRIPT(room)), trig) {
				SCRIPT_TYPES(SCRIPT(room)) |= GET_TRIG_TYPE(trig);
			}
		}
		if (!TRIGGERS(SCRIPT(room))) {
			extract_script(room, WLD_TRIGGER);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ROOM EXTRA HANDLERS /////////////////////////////////////////////////////

/**
* Adds to (or creates) a room extra data value.
*
* @param room_data *room The room to modify data on.
* @param int type The ROOM_EXTRA_x type to update.
* @param int add_value The amount to add (or subtract) to the value.
*/
void add_to_room_extra_data(room_data *room, int type, int add_value) {
	struct room_extra_data *red;
	
	if ((red = find_room_extra_data(room, type))) {
		SAFE_ADD(red->value, add_value, INT_MIN, INT_MAX, TRUE);
		
		// delete zeroes for cleanliness
		if (red->value == 0) {
			remove_room_extra_data(room, type);
		}
	}
	else {
		set_room_extra_data(room, type, add_value);
	}
}


/**
* Finds an extra data object by type.
*
* @param room_data *room The room to check.
* @param int type Any ROOM_EXTRA_x type.
* @return struct room_extra_data* The matching entry, or NULL.
*/
struct room_extra_data *find_room_extra_data(room_data *room, int type) {
	struct room_extra_data *red;
	HASH_FIND_INT(room->extra_data, &type, red);
	return red;
}


/**
* Gets the value of an extra data type for a room; defaults to 0 if none is set.
*
* @param room_data *room The room to check.
* @param int type The ROOM_EXTRA_x type to check.
* @return int The value of that type (default: 0).
*/
int get_room_extra_data(room_data *room, int type) {
	struct room_extra_data *red = find_room_extra_data(room, type);
	return (red ? red->value : 0);
}


/**
* Multiplies an existing room extra data value by a number.
*
* @param room_data *room The room to modify data on.
* @param int type The ROOM_EXTRA_x type to update.
* @param double multiplier How much to multiply the value by.
*/
void multiply_room_extra_data(room_data *room, int type, double multiplier) {
	struct room_extra_data *red;
	
	if ((red = find_room_extra_data(room, type))) {
		red->value = (int) (multiplier * red->value);
		
		// delete zeroes for cleanliness
		if (red->value == 0) {
			remove_room_extra_data(room, type);
		}
	}
	// does nothing if it doesn't exist; 0*X=0
}


/**
* Removes any extra data of a given type from the room.
*
* @param room_data *room The room to remove from.
* @param int type The ROOM_EXTRA_x type to remove.
*/
void remove_room_extra_data(room_data *room, int type) {
	struct room_extra_data *red = find_room_extra_data(room, type);
	if (red) {
		HASH_DEL(room->extra_data, red);
		free(red);
	}
}


/**
* Sets an extra data value to a specific number, overriding any old value.
*
* @param room_data *room The room to set.
* @param int type Any ROOM_EXTRA_x type.
* @param int value The value to set it to.
*/
void set_room_extra_data(room_data *room, int type, int value) {
	struct room_extra_data *red = find_room_extra_data(room, type);
	
	// create if needed
	if (!red) {
		CREATE(red, struct room_extra_data, 1);
		red->type = type;
		HASH_ADD_INT(room->extra_data, type, red);
	}
	
	red->value = value;
}


 //////////////////////////////////////////////////////////////////////////////
//// ROOM TARGETING HANDLERS /////////////////////////////////////////////////

/**
* Take a string, and return a room.. used for goto, at, etc.  -je 4/6/93
* Updated for coordinates and such. The "ch" is now optional, but it only
* sends error messages if it has one.
*
* @param char_data *ch Person to give the error message to (optional).
* @param char *rawroomstr The argument to find.
* @return room_data* The matching room, or NULL if none.
*/
room_data *find_target_room(char_data *ch, char *rawroomstr) {
	extern vehicle_data *get_vehicle(char *name);
	extern room_data *obj_room(obj_data *obj);
	extern room_data *find_room_template_in_instance(struct instance_data *inst, rmt_vnum vnum);
	
	struct instance_data *inst;
	room_vnum tmp;
	room_data *location = NULL;
	vehicle_data *target_veh;
	char_data *target_mob;
	obj_data *target_obj;
	char roomstr[MAX_INPUT_LENGTH];
	int x, y;
	char *srch;

	// we may modify it as we go
	skip_spaces(&rawroomstr);
	strcpy(roomstr, rawroomstr);

	if (!*roomstr) {
		if (ch) {
			send_to_char("You must supply a room number or name.\r\n", ch);
		}
   		location = NULL;
	}
	else if (*roomstr == UID_CHAR) {
		// maybe
		location = find_room(atoi(roomstr + 1));
		
		if (!location && (target_mob = get_char(roomstr))) {
			location = IN_ROOM(target_mob);
		}
		if (!location && (target_veh = get_vehicle(roomstr))) {
			location = IN_ROOM(target_veh);
		}
		if (!location && (target_obj = get_obj(roomstr))) {
			location = obj_room(target_obj);
		}
	}
	else if (*roomstr == 'i' && isdigit(*(roomstr+1)) && ch && IS_NPC(ch) && (inst = real_instance(MOB_INSTANCE_ID(ch))) != NULL) {
		// find room in instance by template vnum
		location = find_room_template_in_instance(inst, atoi(roomstr+1));
	}
	else if (isdigit(*roomstr) && (srch = strchr(roomstr, ','))) {
		// coords
		*(srch++) = '\0';
		skip_spaces(&srch);
		x = atoi(roomstr);
		y = atoi(srch);
		if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
			location = real_room((y * MAP_WIDTH) + x);
		}
		else {
			if (ch) {
				msg_to_char(ch, "Bad coordinates (%d, %d).\r\n", x, y);
			}
			location = NULL;
		}
	}
	else if (isdigit(*roomstr) && !strchr(roomstr, '.')) {
		tmp = atoi(roomstr);
		if (!(location = real_room(tmp)) && ch) {
			send_to_char("No room exists with that number.\r\n", ch);
		}
	}
	else if (ch && (target_mob = get_char_vis(ch, roomstr, FIND_CHAR_WORLD)) != NULL) {
		if (WIZHIDE_OK(ch, target_mob)) {
			location = IN_ROOM(target_mob);
		}
		else {
			msg_to_char(ch, "That person is not available.\r\n");
		}
	}
	else if (!ch && (target_mob = get_char_world(roomstr)) != NULL) {
		location = IN_ROOM(target_mob);
	}
	else if ((ch && (target_veh = get_vehicle_vis(ch, roomstr))) || (!ch && (target_veh = get_vehicle_world(roomstr)))) {
		if (IN_ROOM(target_veh)) {
			location = IN_ROOM(target_veh);
		}
		else {
			if (ch) {
				msg_to_char(ch, "That vehicle is not available.\r\n");
			}
			location = NULL;
		}
	}
	else if ((ch && (target_obj = get_obj_vis(ch, roomstr)) != NULL) || (!ch && (target_obj = get_obj_world(roomstr)) != NULL)) {
		if (IN_ROOM(target_obj))
			location = IN_ROOM(target_obj);
		else {
			if (ch) {
				send_to_char("That object is not available.\r\n", ch);
			}
			location = NULL;
		}
	}
	else {
		if (ch) {
			send_to_char("No such creature or object around.\r\n", ch);
		}
		location = NULL;
	}

	return location;
}


 //////////////////////////////////////////////////////////////////////////////
//// SECTOR HANDLERS /////////////////////////////////////////////////////////

/**
* Centralize running of evolution percents because we may change the precision
* later...
*
* @param struct evolution_data *evo The evolution to check.
* @return bool TRUE if it passes.
*/
bool check_evolution_percent(struct evolution_data *evo) {
	return (number(1, 10000) <= ((int) 100 * evo->percent));
}


/**
* Gets an evolution of the given type, if its percentage passes. If more than
* one of a type exists, the first one that matches the percentage will be
* returned.
*
* @param sector_data *st The sector to check.
* @param int type The EVO_x type to get.
* @return struct evolution_data* The found evolution, or NULL.
*/
struct evolution_data *get_evolution_by_type(sector_data *st, int type) {
	struct evolution_data *evo, *found = NULL;
	
	if (!st) {
		return NULL;
	}
	
	// this iterates over matching types checks their percent chance until it finds one
	for (evo = GET_SECT_EVOS(st); evo && !found; evo = evo->next) {
		if (evo->type == type && check_evolution_percent(evo)) {
			found = evo;
		}
	}
	
	return found;
}


/**
* @param sector_data *st The sector to check.
* @param int type The EVO_x type to check.
* @return bool TRUE if the sector has at least one evolution of this type.
*/
bool has_evolution_type(sector_data *st, int type) {
	struct evolution_data *evo;
	bool found = FALSE;
	
	if (!st) {
		return found;
	}
	
	for (evo = GET_SECT_EVOS(st); evo && !found; evo = evo->next) {
		if (evo->type == type) {
			found = TRUE;
		}
	}
	
	return found;
}


/**
* This finds a sector that can evolve to be the argument 'in_sect'.
*
* @param sector_data *in_sect The sector you have already.
* @param int evo_type Any EVO_x const.
* @return sector_data* The sector that can evolve to become in_sect, or NULL if there isn't one.
*/
sector_data *reverse_lookup_evolution_for_sector(sector_data *in_sect, int evo_type) {
	sector_data *sect, *next_sect;
	struct evolution_data *evo;
	
	if (!in_sect) {
		return NULL;
	}
	
	HASH_ITER(hh, sector_table, sect, next_sect) {
		if (sect != in_sect) {
			for (evo = GET_SECT_EVOS(sect); evo; evo = evo->next) {
				if (evo->type == evo_type && evo->becomes == GET_SECT_VNUM(in_sect)) {
					return sect;
				}
			}
		}
	}
	
	return NULL;
}


// quick-switch of linked list positions
struct evolution_data *switch_evolution_pos(struct evolution_data *l1, struct evolution_data *l2) {
    l1->next = l2->next;
    l2->next = l1;
    return l2;
}


/**
* Sorter for evolutions on a sector.
*
* @param sector_data *sect The sector to sort evolutions on.
*/
void sort_evolutions(sector_data *sect) {
	struct evolution_data *start, *p, *q, *top;
    bool changed = TRUE;
        
    // safety first
    if (!sect) {
    	return;
    }
    
    start = sect->evolution;

	CREATE(top, struct evolution_data, 1);

    top->next = start;
    if (start && start->next) {
    	// q is always one item behind p

        while (changed) {
            changed = FALSE;
            q = top;
            p = top->next;
            while (p->next != NULL) {
				if (p->type > p->next->type || (p->type == p->next->type && p->percent > p->next->percent)) {
					q->next = switch_evolution_pos(p, p->next);
					changed = TRUE;
				}
				
                q = p;
                if (p->next) {
                    p = p->next;
                }
            }
        }
    }
    
    sect->evolution = top->next;
    free(top);
}


 //////////////////////////////////////////////////////////////////////////////
//// STORAGE HANDLERS ////////////////////////////////////////////////////////

/**
* Adds to empire storage by vnum
*
* @param empire_data *emp The empire vnum
* @param int island Which island to store it on
* @param obj_vnum vnum Any object to store.
* @param int amount How much to add
*/
void add_to_empire_storage(empire_data *emp, int island, obj_vnum vnum, int amount) {
	struct empire_storage_data *temp, *store = find_stored_resource(emp, island, vnum);
	
	int old;
	
	// nothing to do
	if (island == NOTHING) {
		return;
	}
	if (amount == 0) {
		return;
	}
	if (amount < 0 && !store) {
		// nothing to take
		return;
	}
	
	if (!store) {
		CREATE(store, struct empire_storage_data, 1);
		store->next = EMPIRE_STORAGE(emp);
		EMPIRE_STORAGE(emp) = store;

		store->vnum = vnum;
		store->island = island;
	}
	
	old = store->amount;
	if (amount > 0) {
		store->amount += amount;
		if (store->amount > MAX_STORAGE || store->amount < old) {
			// check wrapping
			store->amount = MAX_STORAGE;
		}
	}
	else if (amount < 0) {
		store->amount -= amount;
		if (store->amount < 0 || store->amount > old) {
			// check wrapping
			store->amount = 0;
		}
	}
	
	if (store && store->amount <= 0) {
		REMOVE_FROM_LIST(store, EMPIRE_STORAGE(emp), next);
		free(store);
	}
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
}


/**
* removes X stored components from an empire
*
* @param empire_data *emp
* @param int island Which island to charge for storage, or ANY_ISLAND to take from any available storage
* @param int cmp_type Which CMP_ type to charge
* @param int cmp_flags Required CMPF_ flags to match on the component
* @param int amount How much to charge*
* @param struct resource_data **build_used_list Optional: A place to store the exact item used, e.g. for later dismantling. (NULL if none)
* @return bool TRUE if it was able to charge enough, FALSE if not
*/
bool charge_stored_component(empire_data *emp, int island, int cmp_type, int cmp_flags, int amount, struct resource_data **build_used_list) {
	struct empire_storage_data *store, *next_store;
	int this, found = 0;
	obj_data *proto;
	
	// can't charge a negative amount
	if (amount < 0) {
		return TRUE;
	}
	
	LL_FOREACH_SAFE(EMPIRE_STORAGE(emp), store, next_store) {
		if (island != ANY_ISLAND && island != store->island) {
			continue;
		}
		
		// need obj
		if (!(proto = obj_proto(store->vnum))) {
			continue;
		}
		
		// matching component?
		if (GET_OBJ_CMP_TYPE(proto) != cmp_type || (GET_OBJ_CMP_FLAGS(proto) & cmp_flags) != cmp_flags) {
			continue;
		}
		
		// ok make it so
		this = MIN(amount, store->amount);
		found += this;
		SAFE_ADD(store->amount, -this, 0, INT_MAX, FALSE);
		
		if (build_used_list) {
			add_to_resource_list(build_used_list, RES_OBJECT, store->vnum, this, 0);
		}
		
		if (store->amount <= 0) {
			LL_DELETE(EMPIRE_STORAGE(emp), store);
			free(store);
		}
		
		// done?
		if (found >= amount) {
			break;
		}
	}
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
	return (found >= amount);
}


/**
* removes X stored resources from an empire
*
* @param empire_data *emp
* @param int island Which island to charge for storage, or ANY_ISLAND to take from any available storage
* @param obj_vnum vnum type to charge
* @param int amount How much to charge
* @return bool TRUE if it was able to charge enough, FALSE if not
*/
bool charge_stored_resource(empire_data *emp, int island, obj_vnum vnum, int amount) {
	struct empire_storage_data *store, *next_store, *temp;
	int old;
	
	// can't charge a negative amount
	if (amount < 0) {
		return TRUE;
	}
	
	for (store = EMPIRE_STORAGE(emp); store; store = next_store) {
		next_store = store->next;
		
		if (island != ANY_ISLAND && island != store->island) {
			continue;
		}
		if (vnum != store->vnum) {
			continue;
		}
		
		// ok?
		old = store->amount;
		store->amount -= amount;
		
		// bounds check
		if (store->amount > old) {
			store->amount = 0;
		}
		
		if (store->amount >= 0) {
			// success
			amount = 0;
		}
		else if (store->amount < 0) {
			// if it went negative, credit it back to amount
			amount -= store->amount;
		}
	
		if (store->amount <= 0) {
			REMOVE_FROM_LIST(store, EMPIRE_STORAGE(emp), next);
			free(store);
		}
	}
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
	return (amount <= 0);
}


/**
* Removes all of an empire's storage for 1 vnum and then returns TRUE if it
* found any to remove.
*
* @param empire_data *emp Which empire to update.
* @param obj_vnum The vnum of the object to delete.
* @return bool TRUE if it deleted at least 1, FALSE if it deleted 0.
*/
bool delete_stored_resource(empire_data *emp, obj_vnum vnum) {
	struct empire_storage_data *sto, *next_sto, *temp;
	int deleted = 0;
	
	for (sto = EMPIRE_STORAGE(emp); sto; sto = next_sto) {
		next_sto = sto->next;
		
		if (sto->vnum == vnum) {
			deleted += sto->amount;
			REMOVE_FROM_LIST(sto, EMPIRE_STORAGE(emp), next);
			free(sto);
		}
	}
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
	return (deleted > 0) ? TRUE : FALSE;
}


/**
* This finds a matching item's empire_storage_data object for a component type,
* IF there is any match stored to the empire on that island.
*
* @param empire_data *emp The empire.
* @param int island Which island to search.
* @param int cmp_type Any CMP_ type.
* @param int cmp_flags Any CMPF_ flags to match all of.
* @param int amount The number that must be available.
*/
bool empire_can_afford_component(empire_data *emp, int island, int cmp_type, int cmp_flags, int amount) {
	struct empire_storage_data *store;
	obj_data *proto;
	int found = 0;
	
	LL_FOREACH(EMPIRE_STORAGE(emp), store) {
		if (store->island != island) {
			continue;
		}
		
		// need obj
		if (!(proto = obj_proto(store->vnum))) {
			continue;
		}
		
		// is it a match, though?
		if (GET_OBJ_CMP_TYPE(proto) == cmp_type && (GET_OBJ_CMP_FLAGS(proto) & cmp_flags) == cmp_flags) {
			found += store->amount;
			if (found >= amount) {
				break;
			}
		}
	}
	
	return (found >= amount);
}


/**
* Finds empire storage on a given island by item keyword.
* 
* @param empire_data *emp The empire whose storage to search.
* @param int island_id Which island to look on.
* @param char *keywords The keyword(s) to match using multi_isname().
* @return struct empire_storage_data* The storage entry, or NULL if no matches.
*/
struct empire_storage_data *find_island_storage_by_keywords(empire_data *emp, int island_id, char *keywords) {
	struct empire_storage_data *store;
	obj_data *proto;
	
	for (store = EMPIRE_STORAGE(emp); store; store = store->next) {
		if (store->island != island_id) {
			continue;
		}
		if (!(proto = obj_proto(store->vnum))) {
			continue;
		}
		if (!multi_isname(keywords, GET_OBJ_KEYWORDS(proto))) {
			continue;
		}
		
		// found!
		return store;
	}
	
	return NULL;
}


/**
* This is used by the einv sorter (sort_storage) to sort by storage locations,
* where the order of the storage locations on the object won't matter. The
* return value is not significant other than it can be used to compare two
* objects and sort their storage locations.
*
* @param obj_data *obj Any object.
* @return int A unique identifier for its lowest storage location.
*/
int find_lowest_storage_loc(obj_data *obj) {
	struct obj_storage_type *store;
	int loc = MAX_INT;
	
	for (store = obj->storage; store; store = store->next) {
		loc = MIN(loc, store->building_type);
	}
	
	return loc;
}


/**
* This finds the empire_storage_data object for a given vnum in an empire,
* IF there is any of that vnum stored to the empire.
*
* @param empire_data *emp The empire.
* @param int island Which island to search.
* @param obj_vnum vnum The object vnum to check for.
* @return struct empire_storage_data* A pointer to the storage object for the empire, if any (otherwise NULL).
*/
struct empire_storage_data *find_stored_resource(empire_data *emp, int island, obj_vnum vnum) {
	struct empire_storage_data *store;

	for (store = EMPIRE_STORAGE(emp); store; store = store->next) {
		if (store->island == island && store->vnum == vnum) {
			return store;
		}
	}
	
	return NULL;
}


/**
* Counts the total number of something an empire has in all storage.
*
* @param empire_data *emp The empire to check.
* @param obj_vnum vnum The item to look for.
* @param bool count_shipping If TRUE, also count items in the shipping system.
* @return int The total number the empire has stored.
*/
int get_total_stored_count(empire_data *emp, obj_vnum vnum, bool count_shipping) {
	struct empire_storage_data *sto;
	struct shipping_data *shipd;
	int count = 0;
	
	if (!emp) {
		return count;
	}
	
	for (sto = EMPIRE_STORAGE(emp); sto; sto = sto->next) {
		if (sto->vnum == vnum) {
			SAFE_ADD(count, sto->amount, INT_MIN, INT_MAX, TRUE);
		}
	}
	
	if (count_shipping) {
		for (shipd = EMPIRE_SHIPPING_LIST(emp); shipd; shipd = shipd->next) {
			if (shipd->vnum == vnum) {
				SAFE_ADD(count, shipd->amount, INT_MIN, INT_MAX, TRUE);
			}
		}
	}
	
	return count;
}


/**
* @param obj_data *obj The item to check
* @param room_data *loc The world location you want to store it in
* @return bool TRUE if the object can be stored here; otherwise FALSE
*/
bool obj_can_be_stored(obj_data *obj, room_data *loc) {
	struct obj_storage_type *store;
	
	// quest items don't store
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
		return FALSE;
	}
	
	for (store = obj->storage; store; store = store->next) {
		if (BUILDING_VNUM(loc) != NOTHING && store->building_type == BUILDING_VNUM(loc)) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* re-read the vault of an empire
*
* @empire_data *emp
*/
void read_vault(empire_data *emp) {
	struct empire_storage_data *store;
	obj_data *proto;
	
	EMPIRE_WEALTH(emp) = 0;

	for (store = EMPIRE_STORAGE(emp); store; store = store->next) {
		if ((proto = obj_proto(store->vnum))) {
			if (IS_WEALTH_ITEM(proto)) {
				EMPIRE_WEALTH(emp) += GET_WEALTH_VALUE(proto) * store->amount;
			}
		}
	}
}


/**
* This fetches an item from empire storage. If ch's arms are full, the item
* is not retrieved. Warning: it may remove and free the 'store' arg if the
* empire runs out of the item.
*
* @param char_data *ch The player retrieving.
* @param empire_data *emp The empire.
* @param struct empire_storage_data *store The store location to retrieve.
* @param bool stolen if TRUE, sets the stolen timer on the item
* @return bool TRUE if something was retrieved and there are more left, FALSE in any other case
*/
bool retrieve_resource(char_data *ch, empire_data *emp, struct empire_storage_data *store, bool stolen) {
	void trigger_distrust_from_stealth(char_data *ch, empire_data *emp);
	
	obj_data *obj, *proto;
	int available;

	proto = obj_proto(store->vnum);
	
	// somehow
	if (!proto) {
		return FALSE;
	}

	if (!CAN_CARRY_OBJ(ch, proto)) {
		msg_to_char(ch, "Your arms are full.\r\n");
		return FALSE;
	}

	obj = read_object(store->vnum, TRUE);
	available = store->amount - 1;	// for later
	charge_stored_resource(emp, GET_ISLAND_ID(IN_ROOM(ch)), store->vnum, 1);
	scale_item_to_level(obj, 1);	// scale to its minimum

	obj_to_char(obj, ch);
	act("You retrieve $p.", FALSE, ch, obj, 0, TO_CHAR);
	act("$n retrieves $p.", TRUE, ch, obj, 0, TO_ROOM);
	load_otrigger(obj);
	
	if (stolen) {
		GET_STOLEN_TIMER(obj) = time(0);
		GET_STOLEN_FROM(obj) = EMPIRE_VNUM(emp);
		trigger_distrust_from_stealth(ch, emp);
	}
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
	
	// if it ran out, return false to prevent loops
	return (available > 0);
}


/**
* Store an object to an empire, with messaging.
*
* @param char_data *ch The person storing it.
* @param empire_data *emp The empire.
* @param obj_data *obj The item to store (will be extracted).
* @return int 1 -- it always returns 1
*/
int store_resource(char_data *ch, empire_data *emp, obj_data *obj) {
	act("You store $p.", FALSE, ch, obj, 0, TO_CHAR);
	act("$n stores $p.", TRUE, ch, obj, 0, TO_ROOM);

	add_to_empire_storage(emp, GET_ISLAND_ID(IN_ROOM(ch)), GET_OBJ_VNUM(obj), 1);
	extract_obj(obj);
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
	
	return 1;
}


/**
* Determines whether an item requires WITHDRAW permission to retrieve it.
* This is true if ANY of its storage locations require WITHDRAW.
*
* @param obj_data *obj Which object (or prototype) to check.
* @return bool TRUE if it requires withdraw; FALSE otherwise
*/
bool stored_item_requires_withdraw(obj_data *obj) {
	struct obj_storage_type *sdt;
	
	if (obj) {
		for (sdt = obj->storage; sdt; sdt = sdt->next) {
			if (IS_SET(sdt->flags, STORAGE_WITHDRAW)) {
				return TRUE;
			}
		}
	}
	
	return FALSE;
}

 //////////////////////////////////////////////////////////////////////////////
//// UNIQUE STORAGE HANDLERS /////////////////////////////////////////////////


/**
* Adds a unique storage entry to an empire and to the global list.
*
* @param struct empire_unique_storage *eus The item to add.
* @param empire_data *emp The empire to add storage to.
*/
void add_eus_entry(struct empire_unique_storage *eus, empire_data *emp) {
	struct empire_unique_storage *temp;
	
	if (!eus || !emp) {
		return;
	}
	
	eus->next = NULL;
	
	if ((temp = EMPIRE_UNIQUE_STORAGE(emp))) {
		while (temp->next) {
			temp = temp->next;
		}
		temp->next = eus;
	}
	else {
		EMPIRE_UNIQUE_STORAGE(emp) = eus;
	}
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
}


/**
* Remove items from the empire's storage list by vnum (e.g. if the item was
* deleted).
*
* @param empire_data *emp The empire to check.
* @param obj_vnum vnum The object vnum to remove.
* @return bool TRUE if at least 1 was deleted.
*/
bool delete_unique_storage_by_vnum(empire_data *emp, obj_vnum vnum) {
	struct empire_unique_storage *iter, *next_iter;
	bool any = FALSE;
	
	if (!emp) {
		return FALSE;
	}
	
	for (iter = EMPIRE_UNIQUE_STORAGE(emp); iter; iter = next_iter) {
		next_iter = iter->next;
		
		if (iter->obj && GET_OBJ_VNUM(iter->obj) == vnum) {
			add_to_object_list(iter->obj);
			extract_obj(iter->obj);
			iter->obj = NULL;
			remove_eus_entry(iter, emp);
			any = TRUE;
		}
	}
	
	if (any) {
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
	return any;
}


/**
* Find a matching entry for an object in unique storage.
*
* @param obj_data *obj The item to find a matching entry for.
* @param empire_data *emp The empire to search.
* @param room_data *location The room to find matching storage for (optional).
* @return struct empire_unique_storage* The storage entry, if it exists (or NULL).
*/
struct empire_unique_storage *find_eus_entry(obj_data *obj, empire_data *emp, room_data *location) {
	struct empire_unique_storage *iter;
	
	if (!emp || !obj) {
		return NULL;
	}
	
	for (iter = EMPIRE_UNIQUE_STORAGE(emp); iter; iter = iter->next) {
		if (location && GET_ISLAND_ID(location) != iter->island) {
			continue;
		}
		if (location && room_has_function_and_city_ok(location, FNC_VAULT) && !IS_SET(iter->flags, EUS_VAULT)) {
			continue;
		}
		if (location && !room_has_function_and_city_ok(location, FNC_VAULT) && IS_SET(iter->flags, EUS_VAULT)) {
			continue;
		}
		
		if (objs_are_identical(iter->obj, obj)) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* Removes a unique object entry from a local list and from the global list.
*
* @param struct empire_unique_storage *eus The item to remove.
* @param empire_data *emp The empire to remove from.
*/
void remove_eus_entry(struct empire_unique_storage *eus, empire_data *emp) {
	struct empire_unique_storage *temp;
	
	if (!eus || !emp) {
		return;
	}
	
	REMOVE_FROM_LIST(eus, EMPIRE_UNIQUE_STORAGE(emp), next);
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
	free(eus);
}


/**
* Store a unique item to an empire. The object MAY be extracted.
*
* @param char_data *ch Person doing the storing (may be NULL; sends messages if not).
* @param obj_data *obj The unique item to store.
* @param empire_data *emp The empire to store to.
* @param room_data *room The location being store (for vault flag detection).
* @param bool *full A variable to set TRUE if the storage is full and the item can't be stored.
*/
void store_unique_item(char_data *ch, obj_data *obj, empire_data *emp, room_data *room, bool *full) {
	extern int get_main_island(empire_data *emp);
	
	struct empire_unique_storage *eus;
	bool extract = FALSE;
	
	*full = FALSE;
	
	if (!obj || !emp) {
		return;
	}
	
	// empty/clear first
	REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
	LAST_OWNER_ID(obj) = NOBODY;
	obj->last_empire_id = NOTHING;
	empty_obj_before_extract(obj);
	if (IS_DRINK_CONTAINER(obj)) {
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = 0;
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
	}

	if ((eus = find_eus_entry(obj, emp, room))) {
		// check limits
		if (eus->amount >= MAX_STORAGE) {
			*full = TRUE;
			return;
		}
		
		// existing entry
		eus->amount += 1;
		extract = TRUE;
	}
	else {
		// new entry
		CREATE(eus, struct empire_unique_storage, 1);
		add_eus_entry(eus, emp);
		check_obj_in_void(obj);
		eus->obj = obj;
		eus->amount = 1;
		eus->island = room ? GET_ISLAND_ID(room) : NO_ISLAND;
		if (eus->island == NO_ISLAND) {
			eus->island = get_main_island(emp);
		}
		if (room && room_has_function_and_city_ok(room, FNC_VAULT)) {
			eus->flags = EUS_VAULT;
		}
			
		// get it out of the object list
		remove_from_object_list(obj);
	}
	
	if (ch) {
		act("You store $p.", FALSE, ch, obj, NULL, TO_CHAR);
		act("$n stores $p.", FALSE, ch, obj, NULL, TO_ROOM);
	}
	
	if (extract) {
		extract_obj(obj);
	}
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// TARGETING HANDLERS //////////////////////////////////////////////////////

/**
* a function to scan for "all" or "all.x" -- and modify the arg to only
* contain the name, e.g. "all.bear" becomes "bear" and returns FIND_ALLDOT
*
* @param char *arg The input, which will be modified if it has a dotmode
* @return int FIND_ALL, FIND_ALLDOT, or FIND_INDIV
*/
int find_all_dots(char *arg) {
	int mode;
	
	if (!str_cmp(arg, "all")) {
		mode = FIND_ALL;
	}
	else if (!strn_cmp(arg, "all.", 4)) {
		strcpy(arg, arg + 4);
		mode = FIND_ALLDOT;
	}
	else {
		mode = FIND_INDIV;
	}
	
	return mode;
}


/* Generic Find, designed to find any object/character
 *
 * Calling:
 *  *arg     is the pointer containing the string to be searched for.
 *           This string doesn't have to be a single word, the routine
 *           extracts the next word itself.
 *  bitv..   All those bits that you want to "search through".
 *           Bit found will be result of the function
 *  *ch      This is the person that is trying to "find"
 *  **tar_ch Will be NULL if no character was found, otherwise points
 * **tar_obj Will be NULL if no object was found, otherwise points
 * **tar_veh Will be NULL if no vehicle was found, otherwise points
 *
 * The routine used to return a pointer to the next word in *arg (just
 * like the one_argument routine), but now it returns an integer that
 * describes what it filled in.
 */
int generic_find(char *arg, bitvector_t bitvector, char_data *ch, char_data **tar_ch, obj_data **tar_obj, vehicle_data **tar_veh) {
	int i, found;
	char name[256];

	*tar_ch = NULL;
	*tar_obj = NULL;
	*tar_veh = NULL;

	one_argument(arg, name);

	if (!*name) {
		return (0);
	}

	if (IS_SET(bitvector, FIND_CHAR_ROOM)) {	/* Find person in room */
		if ((*tar_ch = get_char_vis(ch, name, FIND_CHAR_ROOM)) != NULL) {
			return (FIND_CHAR_ROOM);
		}
	}
	if (IS_SET(bitvector, FIND_CHAR_WORLD)) {
		if ((*tar_ch = get_char_vis(ch, name, FIND_CHAR_WORLD)) != NULL) {
			return (FIND_CHAR_WORLD);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_EQUIP)) {
		for (found = FALSE, i = 0; i < NUM_WEARS && !found; i++) {
			if (GET_EQ(ch, i) && isname(name, GET_OBJ_KEYWORDS(GET_EQ(ch, i)))) {
				*tar_obj = GET_EQ(ch, i);
				found = TRUE;
			}
		}
		if (found) {
			return (FIND_OBJ_EQUIP);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_INV)) {
		if ((*tar_obj = get_obj_in_list_vis(ch, name, ch->carrying)) != NULL) {
			return (FIND_OBJ_INV);
		}
	}
	if (IS_SET(bitvector, FIND_VEHICLE_ROOM)) {
		if ((*tar_veh = get_vehicle_in_room_vis(ch, name)) != NULL) {
			return (FIND_VEHICLE_ROOM);
		}
	}
	if (IS_SET(bitvector, FIND_VEHICLE_INSIDE)) {
		if (GET_ROOM_VEHICLE(IN_ROOM(ch)) && isname(name, VEH_KEYWORDS(GET_ROOM_VEHICLE(IN_ROOM(ch))))) {
			*tar_veh = GET_ROOM_VEHICLE(IN_ROOM(ch));
			return (FIND_VEHICLE_INSIDE);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_ROOM)) {
		if ((*tar_obj = get_obj_in_list_vis(ch, name, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL) {
			return (FIND_OBJ_ROOM);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_WORLD)) {
		if ((*tar_obj = get_obj_vis(ch, name))) {
			return (FIND_OBJ_WORLD);
		}
	}
	
	// nope
	return (0);
}


/**
* This function looks for syntax like "1.foo", removes the number from the
* beginning, and returns the result ("foo").
*
* @param char **name A pointer to the string to extract the number from
* @return int The number from the beginning of the string
*/
int get_number(char **name) {
	int i;
	char *ppos;
	char number[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH];

	*number = '\0';

	if ((ppos = strchr(*name, '.')) != NULL) {
		*ppos++ = '\0';
		strcpy(number, *name);
		// this required a 3rd variable or the strcpy was changing BOTH strings as it went -paul 4/22/13
		strcpy(temp, ppos);
		strcpy(*name, temp);

		for (i = 0; *(number + i); i++) {
			if (!isdigit(*(number + i))) {
				return (0);
			}
		}

		return (atoi(number));
	}
	
	// default
	return (1);
}


 //////////////////////////////////////////////////////////////////////////////
//// VEHICLE HANDLERS ////////////////////////////////////////////////////////

/**
* Extracts a vehicle from the game.
*
* @param vehicle_data *veh The vehicle to extract and free.
*/
void extract_vehicle(vehicle_data *veh) {
	void empty_vehicle(vehicle_data *veh);
	void relocate_players(room_data *room, room_data *to_room);
	extern char_data *unharness_mob_from_vehicle(struct vehicle_attached_mob *vam, vehicle_data *veh);
	
	struct vehicle_room_list *vrl, *next_vrl;
	room_data *main_room;
	
	if (veh == dg_owner_veh) {
		dg_owner_purged = 1;
		dg_owner_veh = NULL;
	}
	
	// delete interior
	if ((main_room = VEH_INTERIOR_HOME_ROOM(veh)) || VEH_ROOM_LIST(veh)) {
		LL_FOREACH_SAFE(VEH_ROOM_LIST(veh), vrl, next_vrl) {
			if (vrl->room == main_room) {
				continue;	// do this one last
			}
			
			if (IN_ROOM(veh)) {
				relocate_players(vrl->room, IN_ROOM(veh));
			}
			delete_room(vrl->room, FALSE);	// MUST check_all_exits later
		}
		
		if (main_room) {
			if (IN_ROOM(veh)) {
				relocate_players(main_room, IN_ROOM(veh));
			}
			delete_room(main_room, FALSE);
		}
		check_all_exits();
	}
	
	if (VEH_LED_BY(veh)) {
		GET_LEADING_VEHICLE(VEH_LED_BY(veh)) = NULL;
		VEH_LED_BY(veh) = NULL;
	}
	if (VEH_SITTING_ON(veh)) {
		unseat_char_from_vehicle(VEH_SITTING_ON(veh));
	}
	if (VEH_DRIVER(veh)) {
		GET_DRIVING(VEH_DRIVER(veh)) = NULL;
		VEH_DRIVER(veh) = NULL;
	}
	
	if (IN_ROOM(veh)) {
		vehicle_from_room(veh);
	}
	
	// dump contents (this will extract them since it's not in a room)
	empty_vehicle(veh);
	
	// remove animals: doing this without the vehicle in a room will not spawn the mobs
	while (VEH_ANIMALS(veh)) {
		unharness_mob_from_vehicle(VEH_ANIMALS(veh), veh);
	}
	
	LL_DELETE2(vehicle_list, veh, next);
	free_vehicle(veh);
}


/**
* @param char_data *ch Someone trying to sit.
* @param vehicle_data *veh The vehicle to seat them on.
*/
void sit_on_vehicle(char_data *ch, vehicle_data *veh) {
	// safety first
	if (GET_SITTING_ON(ch)) {
		unseat_char_from_vehicle(ch);
	}
	if (VEH_SITTING_ON(veh)) {
		unseat_char_from_vehicle(VEH_SITTING_ON(veh));
	}
	
	GET_SITTING_ON(ch) = veh;
	VEH_SITTING_ON(veh) = ch;
}


/**
* @param char_data *ch A player to remove from the seat he is in/on.
*/
void unseat_char_from_vehicle(char_data *ch) {
	if (GET_SITTING_ON(ch)) {
		VEH_SITTING_ON(GET_SITTING_ON(ch)) = NULL;
	}
	GET_SITTING_ON(ch) = NULL;
}


/**
* Remove a vehicle from the room it is in.
*
* @param vehicle_data *veh The vehicle to remove from its room.
*/
void vehicle_from_room(vehicle_data *veh) {
	if (!veh || !IN_ROOM(veh)) {
		log("SYSERR: NULL vehicle (%p) or vehicle not in a room (%p) passed to vehicle_from_room", veh, IN_ROOM(veh));
		return;
	}
	
	LL_DELETE2(ROOM_VEHICLES(IN_ROOM(veh)), veh, next_in_room);
	IN_ROOM(veh) = NULL;
}


/**
* Put a vehicle in a room.
*
* @param vehicle_data *veh The vehicle.
* @param room_data *room The room to put it in.
*/
void vehicle_to_room(vehicle_data *veh, room_data *room) {
	if (!veh || !room) {
		log("SYSERR: Illegal value(s) passed to vehicle_to_room. (Room %p, vehicle %p)", room, veh);
		return;
	}
	
	if (IN_ROOM(veh)) {
		vehicle_from_room(veh);
	}
	
	LL_PREPEND2(ROOM_VEHICLES(room), veh, next_in_room);
	IN_ROOM(veh) = room;
	VEH_LAST_MOVE_TIME(veh) = time(0);
}


 //////////////////////////////////////////////////////////////////////////////
//// VEHICLE TARGETING HANDLERS //////////////////////////////////////////////

/**
* Finds a vehicle the char can see in the room.
*
* @param char_data *ch The person who's looking.
* @param char *name The target argument.
* @parma room_data *room Which room to look for a vehicle in.
* @return vehicle_data* The vehicle found, or NULL.
*/
vehicle_data *get_vehicle_in_target_room_vis(char_data *ch, room_data *room, char *name) {
	int found = 0, number;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname;
	vehicle_data *iter;

	strcpy(tmp, name);
	
	// 0.x does not target vehicles
	if ((number = get_number(&tmp)) == 0) {
		return (NULL);
	}
	
	LL_FOREACH2(ROOM_VEHICLES(room), iter, next_in_room) {
		if (!isname(tmp, VEH_KEYWORDS(iter))) {
			continue;
		}
		if (!CAN_SEE_VEHICLE(ch, iter)) {
			continue;
		}
		
		// found: check number
		if (++found == number) {
			return iter;
		}
	}

	return NULL;
}


/**
* Find a vehicle in the world, visible to the character.
*
* @param char_data *ch The person looking.
* @param char *name The string they typed.
* @return vehicle_data* The vehicle found, or NULL.
*/
vehicle_data *get_vehicle_vis(char_data *ch, char *name) {
	int found = 0, number;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname;
	vehicle_data *iter;
	
	// prefer match in same room
	if ((iter = get_vehicle_in_room_vis(ch, name))) {
		return iter;
	}

	strcpy(tmp, name);
	
	// 0.x does not target vehicles
	if ((number = get_number(&tmp)) == 0) {
		return (NULL);
	}
	
	LL_FOREACH2(vehicle_list, iter, next) {
		if (!isname(tmp, VEH_KEYWORDS(iter))) {
			continue;
		}
		if (!CAN_SEE_VEHICLE(ch, iter)) {
			continue;
		}
		
		// found: check number
		if (++found == number) {
			return iter;
		}
	}

	return NULL;
}


/**
* Find a vehicle in the room, without regard to visibility.
*
* @param room_data *room The room to look in.
* @param char *name The string to search for.
* @return vehicle_data* The found vehicle, or NULL.
*/
vehicle_data *get_vehicle_room(room_data *room, char *name) {
	int found = 0, number;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname;
	vehicle_data *iter;

	strcpy(tmp, name);
	
	// 0.x does not target vehicles
	if ((number = get_number(&tmp)) == 0) {
		return (NULL);
	}
	
	LL_FOREACH2(ROOM_VEHICLES(room), iter, next_in_room) {
		if (!isname(tmp, VEH_KEYWORDS(iter))) {
			continue;
		}
		
		// found: check number
		if (++found == number) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* Find a vehicle in the world, without regard to visibility.
*
* @param char *name The string to search for.
* @return vehicle_data* The vehicle found, or NULL.
*/
vehicle_data *get_vehicle_world(char *name) {
	int found = 0, number;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname;
	vehicle_data *iter;

	strcpy(tmp, name);
	
	// 0.x does not target vehicles
	if ((number = get_number(&tmp)) == 0) {
		return (NULL);
	}
	
	LL_FOREACH2(vehicle_list, iter, next) {
		if (!isname(tmp, VEH_KEYWORDS(iter))) {
			continue;
		}
		
		// found: check number
		if (++found == number) {
			return iter;
		}
	}

	return NULL;
}


 //////////////////////////////////////////////////////////////////////////////
//// WORLD HANDLERS //////////////////////////////////////////////////////////

/**
* Finds an exit in a world room, and returns it.
*
* @param room_data *room The room to check.
* @param int dir The direction to find.
* @return struct room_direction_data* The exit, or NULL if none exists.
*/
struct room_direction_data *find_exit(room_data *room, int dir) {
	struct room_direction_data *iter;
	
	if (!COMPLEX_DATA(room)) {
		return NULL;
	}
	
	for (iter = COMPLEX_DATA(room)->exits; iter; iter = iter->next) {
		if (iter->dir == dir) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* This function modifies the desired direction based on which way the
* character thinks is north.
*
* All direction-related commands should pass through this.
*
* @param char_data *ch the character
* @param int dir the original dir the character was trying to go
* @return int the modified direction
*/
int get_direction_for_char(char_data *ch, int dir) {
	// special case -- pass through a NO_DIR
	if (dir == NO_DIR) {
		return dir;
	}
	
	return confused_dirs[get_north_for_char(ch)][1][dir];
}


/**
* Converts a direction string into a direction const like NORTH, based on
* which direction a character thinks he's facing.
*
* This function does not allow mortals to pick DIR_RANDOM (imms can, though).
*
* @param char_data *ch The possibly-confused character (optional).
* @param char *dir The argument string ("north")
* @return int A real direction (EAST), or NO_DIR if none.
*/
int parse_direction(char_data *ch, char *dir) {
	extern const char *alt_dirs[];
	extern const char *dirs[];

	int d;

	// two sets of dirs to check -- alt_dirs contains short names like "ne"
	if ((d = search_block(dir, dirs, FALSE)) == NOTHING) {
		d = search_block(dir, alt_dirs, FALSE);
	}
	
	// confused?
	if (ch && d != NOTHING) {
		d = confused_dirs[get_north_for_char(ch)][0][d];
	}
	
	// random check
	if ((!ch || !IS_IMMORTAL(ch)) && d == DIR_RANDOM) {
		d = NO_DIR;
	}

	return d;
}


 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS HANDLERS //////////////////////////////////////////////////

/**
* Marks 1 trade expired.
*
* @param struct trading_post_data *tpd The trade to expire.
*/
void expire_trading_post_item(struct trading_post_data *tpd) {
	REMOVE_BIT(tpd->state, TPD_FOR_SALE);
	SET_BIT(tpd->state, TPD_EXPIRED);
	
	if (tpd->obj) {
		SET_BIT(tpd->state, TPD_OBJ_PENDING);
	}
}

