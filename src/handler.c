/* ************************************************************************
*   File: handler.c                                       EmpireMUD 2.0b5 *
*  Usage: internal funcs: moving and finding chars/objs                   *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#include "dg_event.h"
#include "vnums.h"
#include "constants.h"

/**
* Contents:
*   Affect Handlers
*   Character Handlers
*   Character Location Handlers
*   Character Targeting Handlers
*   Coin Handlers
*   Cooldown Handlers
*   Currency Handlers
*   Empire Handlers
*   Empire Dropped Item Handlers
*   Empire Production Total Handlers
*   Empire Needs Handlers
*   Empire Targeting Handlers
*   Follow Handlers
*   Global Handlers
*   Group Handlers
*   Help Handlers
*   Interaction Handlers
*   Learned Craft Handlers
*   Lore Handlers
*   Minipet and Companion Handlers
*   Mob Tagging Handlers
*   Mount Handlers
*   Object Handlers
*   Object Binding Handlers
*   Object Location Handlers
*   Custom Message Handlers
*   Object Targeting Handlers
*   Offer Handlers
*   Player Tech Handlers
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

// external vars
extern bool override_home_storage_cap;
extern const int remove_lore_types[];

// external funcs
ACMD(do_return);
EVENT_CANCEL_FUNC(cancel_room_event);
EVENT_CANCEL_FUNC(cancel_wait_event);

// locals
void add_dropped_item(empire_data *emp, obj_data *obj);
void add_dropped_item_anywhere(obj_data *obj, empire_data *only_if_emp);
void add_dropped_item_list(empire_data *emp, obj_data *list);
static void add_obj_binding(int idnum, struct obj_binding **list);
EVENT_CANCEL_FUNC(cancel_cooldown_event);
EVENTFUNC(cooldown_expire_event);
void remove_dropped_item(empire_data *emp, obj_data *obj);
void remove_dropped_item_anywhere(obj_data *obj);
void remove_dropped_item_list(empire_data *emp, obj_data *list);
void remove_lore_record(char_data *ch, struct lore_data *lore);

// local file scope variables
int char_extractions_pending = 0;
static int veh_extractions_pending = 0;


// for run_global_mob_interactions_func
struct glb_mob_interact_bean {
	char_data *mob;
	int type;
	INTERACTION_FUNC(*func);
};

// for run_global_obj_interactions_func
struct glb_obj_interact_bean {
	obj_data *obj;
	int type;
	INTERACTION_FUNC(*func);
};


 //////////////////////////////////////////////////////////////////////////////
//// AFFECT HANDLERS /////////////////////////////////////////////////////////

// expiry event handler for character affects
EVENTFUNC(affect_expire_event) {
	struct affect_expire_event_data *expire_data = (struct affect_expire_event_data *)event_obj;
	struct affected_type *af, *immune;
	char_data *ch;
	
	// grab data and free it
	ch = expire_data->character;
	af = expire_data->affect;
	free(expire_data);
	
	// cancel this first
	af->expire_event = NULL;
	
	// messaging?
	if ((af->type > 0)) {
		if (!af->next || (af->next->type != af->type) || (af->next->expire_time > af->expire_time)) {
			show_wear_off_msg(ch, af->type);
		}
	}
	
	// special case -- add immunity
	if (IS_SET(af->bitvector, AFF_STUNNED) && config_get_int("stun_immunity_time") > 0) {
		immune = create_flag_aff(ATYPE_STUN_IMMUNITY, config_get_int("stun_immunity_time"), AFF_IMMUNE_STUN, ch);
		affect_join(ch, immune, NOBITS);
	}
	
	affect_remove(ch, af);
	affect_total(ch);
	
	// do not reenqueue
	return 0;
}


// this runs every 5 seconds until it removes the DOT
EVENTFUNC(dot_update_event) {
	struct dot_event_data *data = (struct dot_event_data *)event_obj;
	struct over_time_effect_type *dot;
	char_data *ch, *caster;
	int type, result;
	generic_data *gen;
	
	// grab data and free it
	ch = data->ch;
	dot = data->dot;
	
	// cancel this first -- will re-enqueue if needed
	dot->update_event = NULL;
	
	// damage them if any time remains (if not, this dot is actually already over)
	if (dot->time_remaining > 0) {
		// determine type:
		type = damage_type_to_dot_attack[dot->damage_type];
		caster = find_player_in_room_by_id(IN_ROOM(ch), dot->cast_by);
		
		// custom messages?
		if (dot->type != NOTHING && (gen = real_generic(dot->type)) && GET_AFFECT_DOT_ATTACK(gen) > 0 && real_attack_message(GET_AFFECT_DOT_ATTACK(gen))) {
			type = GET_AFFECT_DOT_ATTACK(gen);
		}
		
		// bam! (damage func shows the messaging)
		result = damage(caster ? caster : ch, ch, dot->damage * dot->stack, type, dot->damage_type, NULL);
		
		if (result < 0 || IS_DEAD(ch) || EXTRACTED(ch)) {
			// done here (death and extraction should both remove the DOT themselves)
			free(data);
			return 0;	// do not re-enqueue
		}
		
		// they lived:
		dot->time_remaining -= DOT_INTERVAL;
		
		// cancel action if they were hit
		if (result > 0) {
			cancel_action(ch);
		}
	}
	
	// reschedule or expire
	if (GET_HEALTH(ch) >= 0 && dot->time_remaining > 0) {
		// NOTE: when a character drops below 0, dots are often removed in a way this CAN'T detect
		dot->update_event = the_event;
		return DOT_INTERVAL RL_SEC;	// re-enqueue
	}
	else {
		// expired
		if (dot->type > 0) {
			show_wear_off_msg(ch, dot->type);
		}
		free(data);
		dot_remove(ch, dot);
		return 0;	// do not re-enqueue
	}
}


// expiry event handler for rooms
EVENTFUNC(room_affect_expire_event) {
	struct room_expire_event_data *expire_data = (struct room_expire_event_data *)event_obj;
	struct affected_type *af;
	generic_data *gen;
	room_data *room;
	
	// grab data and free it
	room = expire_data->room;
	af = expire_data->affect;
	free(expire_data);
	
	// cancel this first
	af->expire_event = NULL;
	
	if ((af->type > 0)) {
		// this avoids sending messages multiple times for 1 affect type
		if (!af->next || (af->next->type != af->type) || (af->next->expire_time > af->expire_time)) {
			if ((gen = find_generic(af->type, GENERIC_AFFECT)) && GET_AFFECT_WEAR_OFF_TO_ROOM(gen) && ROOM_PEOPLE(room)) {
				act(GET_AFFECT_WEAR_OFF_TO_ROOM(gen), FALSE, ROOM_PEOPLE(room), 0, 0, TO_CHAR | TO_ROOM);
			}
		}
	}
	affect_remove_room(room, af);
	
	// do not reenqueue
	return 0;
}


// frees memory when character affect expiry is canceled
EVENT_CANCEL_FUNC(cancel_affect_expire_event) {
	struct affect_expire_event_data *data = (struct affect_expire_event_data*)event_obj;
	free(data);
}


// frees memory when character affect expiry is canceled
EVENT_CANCEL_FUNC(cancel_dot_event) {
	struct dot_event_data *data = (struct dot_event_data*)event_obj;
	free(data);
}


// frees memory when room expiry is canceled
EVENT_CANCEL_FUNC(cancel_room_expire_event) {
	struct room_expire_event_data *data = (struct room_expire_event_data *)event_obj;
	free(data);
}


/**
* Call affect_remove with every af of "type"
*
* @param char_data *ch The person to remove affects from.
* @param any_vnum type Any ATYPE_ const/vnum
* @param bool show_msg If TRUE, will show the wears-off message.
*/
void affect_from_char(char_data *ch, any_vnum type, bool show_msg) {
	struct over_time_effect_type *dot, *next_dot;
	struct affected_type *hjp, *next;
	bool shown = FALSE, any = FALSE;

	for (hjp = ch->affected; hjp; hjp = next) {
		next = hjp->next;
		if (hjp->type == type) {
			affect_remove(ch, hjp);
			if (show_msg && !shown) {
				show_wear_off_msg(ch, type);
				shown = TRUE;
			}
			any = TRUE;
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
			any = TRUE;
		}
	}
	
	if (any) {
		affect_total(ch);
	}
}


/**
* Calls affect_remove on every affect of type "type" with location "apply".
*
* @param char_data *ch The person to remove affects from.
* @param any_vnum type Any ATYPE_ const/vnum to match.
* @param int apply Any APPLY_ const to match.
* @param bool show_msg If TRUE, will show the wears-off message.
*/
void affect_from_char_by_apply(char_data *ch, any_vnum type, int apply, bool show_msg) {
	struct affected_type *aff, *next_aff;
	bool shown = FALSE, any = FALSE;

	for (aff = ch->affected; aff; aff = next_aff) {
		next_aff = aff->next;
		if (aff->type == type && aff->location == apply) {
			if (show_msg && !shown) {
				show_wear_off_msg(ch, type);
				shown = TRUE;
			}
			affect_remove(ch, aff);
			any = TRUE;
		}
	}
	
	if (any) {
		affect_total(ch);
	}
}


/**
* Calls affect_remove on every affect of type "type" that sets AFF flag "bits".
*
* @param char_data *ch The person to remove affects from.
* @param any_vnum type Any ATYPE_ const/vnum to match. Use NOTHING to match any atype and only check bitvector.
* @param bitvector_t bits Any AFF_ bit(s) to match.
* @param bool show_msg If TRUE, will show the wears-off message.
*/
void affect_from_char_by_bitvector(char_data *ch, any_vnum type, bitvector_t bits, bool show_msg) {
	struct affected_type *aff, *next_aff;
	bool shown = FALSE, any = FALSE;
	
	if (type == NOTHING && !bits) {
		return;	// no work at all
	}

	for (aff = ch->affected; aff; aff = next_aff) {
		next_aff = aff->next;
		if ((type == NOTHING || aff->type == type) && IS_SET(aff->bitvector, bits)) {
			if (show_msg && !shown) {
				show_wear_off_msg(ch, type);
				shown = TRUE;
			}
			affect_remove(ch, aff);
			any = TRUE;
		}
	}
	
	if (any) {
		affect_total(ch);
	}
}


/**
* Calls affect_remove on every affect of type "type" with a given caster.
*
* @param char_data *ch The person to remove affects from.
* @param any_vnum type Any ATYPE_ const/vnum to match.
* @param char_data *caster The person whose affects to remove.
* @param bool show_msg If TRUE, will send the wears-off message.
*/
void affect_from_char_by_caster(char_data *ch, any_vnum type, char_data *caster, bool show_msg) {
	struct affected_type *aff, *next_aff;
	bool shown = FALSE, any = FALSE;
	
	LL_FOREACH_SAFE(ch->affected, aff, next_aff) {
		if (aff->type == type && aff->cast_by == CAST_BY_ID(caster)) {
			if (show_msg && !shown) {
				show_wear_off_msg(ch, type);
				shown = TRUE;
			}
			
			affect_remove(ch, aff);
			any = TRUE;
		}
	}
	
	if (any) {
		affect_total(ch);
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
* @param any_vnum type Any ATYPE_ const/vnum
*/
void affect_from_room(room_data *room, any_vnum type) {
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
* @param room_data *room The room to remove affects from.
* @param any_vnum type Any ATYPE_ const/vnum to match.
* @param bitvector_t bits Any AFF_ bit(s) to match.
* @param bool show_msg If TRUE, shows the wear-off message.
*/
void affect_from_room_by_bitvector(room_data *room, any_vnum type, bitvector_t bits, bool show_msg) {
	struct affected_type *aff, *next_aff;
	generic_data *gen;
	bool shown = FALSE;
	
	LL_FOREACH_SAFE(ROOM_AFFECTS(room), aff, next_aff) {
		if (aff->type == type && IS_SET(aff->bitvector, bits)) {
			if (show_msg && !shown && (gen = find_generic(aff->type, GENERIC_AFFECT))) {
				if (GET_AFFECT_WEAR_OFF_TO_ROOM(gen) && ROOM_PEOPLE(room)) {
					act(GET_AFFECT_WEAR_OFF_TO_ROOM(gen), FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
				}
				shown = TRUE;
			}
			affect_remove_room(room, aff);
		}
	}
}


/**
* Calls affect_remove on every affect of type "type" with a given caster.
*
* @param room_data *room The room to remove affects from.
* @param any_vnum type Any ATYPE_ const/vnum to match.
* @param char_data *caster The person whose affects to remove.
* @param bool show_msg If TRUE, will send the wears-off message.
*/
void affect_from_room_by_caster(room_data *room, any_vnum type, char_data *caster, bool show_msg) {
	bool shown = FALSE;
	struct affected_type *aff, *next_aff;
	generic_data *gen;
	
	LL_FOREACH_SAFE(ROOM_AFFECTS(room), aff, next_aff) {
		if (aff->type == type && aff->cast_by == CAST_BY_ID(caster)) {
			if (show_msg && !shown && (gen = find_generic(aff->type, GENERIC_AFFECT))) {
				if (GET_AFFECT_WEAR_OFF_TO_ROOM(gen) && ROOM_PEOPLE(room)) {
					act(GET_AFFECT_WEAR_OFF_TO_ROOM(gen), FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
				}
				shown = TRUE;
			}
			affect_remove_room(room, aff);
		}
	}
}


/**
* Shuts off any unlimited-duration effects on the room, e.g. when dismantling.
*
* @param room_data *room The room to remove "unlimited" effects from.
*/
void cancel_permanent_affects_room(room_data *room) {
	struct affected_type *hjp, *next;

	LL_FOREACH_SAFE(ROOM_AFFECTS(room), hjp, next) {
		next = hjp->next;
		if (hjp->expire_time == UNLIMITED) {
			affect_remove_room(room, hjp);
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
	generic_data *gen;
	bool found = FALSE;
	
	// FIX: merge af into af_iter instead of vice versa; do not remove/add the effect, but may still need to call messaging
	#define NOT_UNLIM(aff)  ((aff)->expire_time != UNLIMITED)
	
	for (af_iter = ch->affected; af_iter && !found; af_iter = af_iter->next) {
		if (af_iter->type == af->type && af_iter->location == af->location && af_iter->bitvector == af->bitvector) {
			// found match: will merge the new af into the old one
			if (IS_SET(flags, ADD_DURATION) && NOT_UNLIM(af_iter) && NOT_UNLIM(af)) {
				af_iter->expire_time += (af->expire_time - time(0));
			}
			else if (IS_SET(flags, AVG_DURATION) && NOT_UNLIM(af_iter) && NOT_UNLIM(af)) {
				af_iter->expire_time = time(0) + (((af->expire_time - time(0)) + (af_iter->expire_time - time(0))) / 2);
			}
			else {	// otherwise keep the new duration
				af_iter->expire_time = af->expire_time;
			}
			
			if (IS_SET(flags, ADD_MODIFIER)) {
				af_iter->modifier += af->modifier;
			}
			else if (IS_SET(flags, AVG_MODIFIER)) {
				af_iter->modifier = (af->modifier + af_iter->modifier) / 2;
			}
			else {	// otherwise keep the new modifier
				af_iter->modifier = af->modifier;
			}

			// prior to b5.129b this removed the old aff and applied a new one,
			// which caused list iteration problems
			
			// update this:
			af_iter->cast_by = af->cast_by;
			
			// send the message, if needed
			if (!IS_SET(flags, SILENT_AFF) && (gen = find_generic(af->type, GENERIC_AFFECT))) {
				if (GET_AFFECT_APPLY_TO_CHAR(gen)) {
					act(GET_AFFECT_APPLY_TO_CHAR(gen), FALSE, ch, NULL, NULL, TO_CHAR | ACT_AFFECT);
				}
				if (GET_AFFECT_APPLY_TO_ROOM(gen)) {
					act(GET_AFFECT_APPLY_TO_ROOM(gen), TRUE, ch, NULL, NULL, TO_ROOM | ACT_AFFECT);
				}
			}
			
			// reschedule it (duration may have changed)
			schedule_affect_expire(ch, af_iter);
			
			// and save
			queue_delayed_update(ch, CDU_MSDP_AFFECTS);
			found = TRUE;
			break;
		}
	}
	
	// add it if not found
	if (!found) {
		if (IS_SET(flags, SILENT_AFF)) {
			affect_to_char_silent(ch, af);
		}
		else {
			affect_to_char(ch, af);
		}
	}
	
	// af is always copied or duplicated; must free it now
	free(af);
	affect_total(ch);
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
	int diff, orig, grt;
	
	if (add) {
		SET_BIT(AFF_FLAGS(ch), bitv);
		
		// check lights
		if (IS_SET(bitv, AFF_LIGHT)) {
			++GET_LIGHTS(ch);
			if (IN_ROOM(ch)) {
				++ROOM_LIGHTS(IN_ROOM(ch));
			}
		}
	}
	else {
		REMOVE_BIT(AFF_FLAGS(ch), bitv);
		mod = -mod;
		
		// check lights
		if (IS_SET(bitv, AFF_LIGHT)) {
			--GET_LIGHTS(ch);
			if (IN_ROOM(ch)) {
				--ROOM_LIGHTS(IN_ROOM(ch));
			}
		}
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
		case APPLY_GREATNESS: {
			SAFE_ADD(GET_GREATNESS(ch), mod, SHRT_MIN, SHRT_MAX, TRUE);
			if (!IS_NPC(ch) && GET_LOYALTY(ch) && IN_ROOM(ch)) {
				grt = GET_GREATNESS(ch);	// store temporarily
				GET_GREATNESS(ch) = MIN(grt, att_max(ch));
				GET_GREATNESS(ch) = MAX(0, GET_GREATNESS(ch));
				update_member_data(ch);	// update empire
				GET_GREATNESS(ch) = grt;	// restore to what it just was
				TRIGGER_DELAYED_REFRESH(GET_LOYALTY(ch), DELAY_REFRESH_GREATNESS);
			}
			break;
		}
		case APPLY_INTELLIGENCE:
			SAFE_ADD(GET_INTELLIGENCE(ch), mod, SHRT_MIN, SHRT_MAX, TRUE);
			break;
		case APPLY_WITS:
			SAFE_ADD(GET_WITS(ch), mod, SHRT_MIN, SHRT_MAX, TRUE);
			break;
		case APPLY_AGE:
			SAFE_ADD(GET_AGE_MODIFIER(ch), mod, INT_MIN, INT_MAX, TRUE);
			break;
		case APPLY_MOVE: {
			SAFE_ADD(GET_MAX_MOVE(ch), mod, INT_MIN, INT_MAX, TRUE);
			
			// prevent from going negative
			orig = GET_MOVE(ch);
			set_move(ch, GET_MOVE(ch) + mod);
			
			if (!IS_NPC(ch)) {
				if (GET_MOVE(ch) < 0) {
					GET_MOVE_DEFICIT(ch) -= GET_MOVE(ch);
					set_move(ch, 0);
				}
				else if (GET_MOVE_DEFICIT(ch) > 0) {
					diff = MAX(0, GET_MOVE(ch) - orig);
					diff = MIN(GET_MOVE_DEFICIT(ch), diff);
					GET_MOVE_DEFICIT(ch) -= diff;
					set_move(ch, GET_MOVE(ch) - diff);
				}
			}
			break;
		}
		case APPLY_HEALTH: {
			// apply to max
			SAFE_ADD(GET_MAX_HEALTH(ch), mod, INT_MIN, INT_MAX, TRUE);
			
			// apply to current
			orig = GET_HEALTH(ch);
			set_health(ch, GET_HEALTH(ch) + mod);
			
			if (!IS_NPC(ch)) {
				if (GET_HEALTH(ch) < 1) {
					if (GET_POS(ch) >= POS_SLEEPING) {
						// min 1 on health unless unconscious
						GET_HEALTH_DEFICIT(ch) -= (GET_HEALTH(ch)-1);
						set_health(ch, 1);
					}
					// otherwise leave them dead/negative
				}
				else if (GET_HEALTH_DEFICIT(ch) > 0) {
					// positive health plus a health deficit
					diff = MAX(0, GET_HEALTH(ch) - orig);
					diff = MIN(diff, GET_HEALTH_DEFICIT(ch));
					diff = MIN(diff, GET_HEALTH(ch)-1);
					GET_HEALTH_DEFICIT(ch) -= diff;
					set_health(ch, GET_HEALTH(ch) - diff);
				}
			}
			else {
				// npcs cannot die this way
				set_health(ch, MAX(1, GET_HEALTH(ch)));
			}
			break;
		}
		case APPLY_MANA: {
			SAFE_ADD(GET_MAX_MANA(ch), mod, INT_MIN, INT_MAX, TRUE);
			
			// prevent from going negative
			orig = GET_MANA(ch);
			set_mana(ch, GET_MANA(ch) + mod);
			
			if (!IS_NPC(ch)) {
				if (GET_MANA(ch) < 0) {
					GET_MANA_DEFICIT(ch) -= GET_MANA(ch);
					set_mana(ch, 0);
				}
				else if (GET_MANA_DEFICIT(ch) > 0) {
					diff = MAX(0, GET_MANA(ch) - orig);
					diff = MIN(GET_MANA_DEFICIT(ch), diff);
					GET_MANA_DEFICIT(ch) -= diff;
					set_mana(ch, GET_MANA(ch) - diff);
				}
			}
			break;
		}
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
			if (mod > 0) {
				schedule_heal_over_time(ch);
			}
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
		case APPLY_NIGHT_VISION: {
			SAFE_ADD(GET_EXTRA_ATT(ch, ATT_NIGHT_VISION), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_NEARBY_RANGE: {
			SAFE_ADD(GET_EXTRA_ATT(ch, ATT_NEARBY_RANGE), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_WHERE_RANGE: {
			SAFE_ADD(GET_EXTRA_ATT(ch, ATT_WHERE_RANGE), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_WARMTH: {
			SAFE_ADD(GET_EXTRA_ATT(ch, ATT_WARMTH), mod, INT_MIN, INT_MAX, TRUE);
			break;
		}
		case APPLY_COOLING: {
			SAFE_ADD(GET_EXTRA_ATT(ch, ATT_COOLING), mod, INT_MIN, INT_MAX, TRUE);
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
* You should call affect_total(ch) when you are done removing affects.
*
* @param char_data *ch The person to remove the af from.
* @param struct affected_by *af The affect to remove.
*/
void affect_remove(char_data *ch, struct affected_type *af) {
	// not affected by it at all somehow?
	if (!ch || !af || ch->affected == NULL) {
		return;
	}
	
	if (af->expire_event) {
		dg_event_cancel(af->expire_event, cancel_affect_expire_event);
		af->expire_event = NULL;
	}

	affect_modify(ch, af->location, af->modifier, af->bitvector, FALSE);
	LL_DELETE(ch->affected, af);
	free(af);
	
	queue_delayed_update(ch, CDU_MSDP_AFFECTS);
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
	// only prevent basic errors
	if (!room || !af) {
		return;
	}
	
	if (af->expire_event) {
		dg_event_cancel(af->expire_event, cancel_room_expire_event);
		af->expire_event = NULL;
	}
	
	REMOVE_BIT(ROOM_AFF_FLAGS(room), af->bitvector);
	
	LL_DELETE(ROOM_AFFECTS(room), af);
	free(af);
	
	affect_total_room(room);
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
}


/**
* Insert an affect_type in a char_data structure. Automatically sets apropriate
* bits and applies.
*
* NOTE: This version does not send the apply message.
*
* Caution: this duplicates af (because of how it used to load from the pfile)
*
* @param char_data *ch The person to add the affect to
* @param struct affected_type *af The affect to add.
*/
void affect_to_char_silent(char_data *ch, struct affected_type *af) {
	struct affected_type *affected_alloc;

	CREATE(affected_alloc, struct affected_type, 1);

	*affected_alloc = *af;
	affected_alloc->expire_event = NULL;
	LL_PREPEND(ch->affected, affected_alloc);

	affect_modify(ch, af->location, af->modifier, af->bitvector, TRUE);
	affect_total(ch);
	schedule_affect_expire(ch, affected_alloc);
	
	queue_delayed_update(ch, CDU_MSDP_AFFECTS);
}


/**
* Insert an affect_type in a char_data structure. Automatically sets apropriate
* bits and applies.
*
* Caution: this duplicates af (because of how it used to load from the pfile)
*
* @param char_data *ch The person to add the affect to
* @param struct affected_type *af The affect to add.
*/
void affect_to_char(char_data *ch, struct affected_type *af) {
	generic_data *gen = find_generic(af->type, GENERIC_AFFECT);
	
	if (gen && GET_AFFECT_APPLY_TO_CHAR(gen)) {
		act(GET_AFFECT_APPLY_TO_CHAR(gen), FALSE, ch, NULL, NULL, TO_CHAR | ACT_AFFECT);
	}
	if (gen && GET_AFFECT_APPLY_TO_ROOM(gen)) {
		act(GET_AFFECT_APPLY_TO_ROOM(gen), TRUE, ch, NULL, NULL, TO_ROOM | ACT_AFFECT);
	}
	
	affect_to_char_silent(ch, af);
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
	generic_data *gen = find_generic(af->type, GENERIC_AFFECT);
	
	if (gen && GET_AFFECT_APPLY_TO_ROOM(gen) && ROOM_PEOPLE(room)) {
		act(GET_AFFECT_APPLY_TO_ROOM(gen), FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM | ACT_AFFECT);
	}

	CREATE(affected_alloc, struct affected_type, 1);

	*affected_alloc = *af;
	affected_alloc->expire_event = NULL;
	LL_PREPEND(ROOM_AFFECTS(room), affected_alloc);
	
	affected_alloc->expire_event = NULL;	// cannot have an event in the copied af at this point
	
	SET_BIT(ROOM_AFF_FLAGS(room), affected_alloc->bitvector);
	schedule_room_affect_expire(room, affected_alloc);
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	
	affect_total_room(room);
}


/**
* This updates a character by subtracting everything he is affected by,
* restoring original abilities, and then affecting all again. This is called
* periodically to ensure that all effects are correctly applied and updated.
*
* @param char_data *ch The person whose effects to update
*/
void affect_total(char_data *ch) {
	struct affected_type *af;
	int i, iter, level;
	struct obj_apply *apply;
	int health, move, mana, greatness;
	
	int pool_bonus_amount = config_get_int("pool_bonus_amount");
	
	// this prevents over-totaling
	if (pause_affect_total) {
		return;
	}
	
	// save these for later -- they shouldn't change during an affect_total
	health = GET_HEALTH(ch);
	move = GET_MOVE(ch);
	mana = GET_MANA(ch);
	level = get_approximate_level(ch);
	greatness = GET_GREATNESS(ch);
	
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
	
	// remove passive buff abilities
	if (!IS_NPC(ch)) {
		LL_FOREACH(GET_PASSIVE_BUFFS(ch), af) {
			affect_modify(ch, af->location, af->modifier, af->bitvector, FALSE);
		}
	}

	// remove affects
	LL_FOREACH(ch->affected, af) {
		affect_modify(ch, af->location, af->modifier, af->bitvector, FALSE);
	}

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
	
	// passive buff abilities
	if (!IS_NPC(ch)) {
		LL_FOREACH(GET_PASSIVE_BUFFS(ch), af) {
			affect_modify(ch, af->location, af->modifier, af->bitvector, TRUE);
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
	
	/* Make sure maximums are considered */
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		GET_ATT(ch, iter) = MAX(0, MIN(GET_ATT(ch, iter), att_max(ch)));
	}
	
	// limit this
	GET_MAX_HEALTH(ch) = MAX(1, GET_MAX_HEALTH(ch));
	
	// restore these because in some cases, they mess up during an affect_total
	set_health(ch, health);
	set_move(ch, move);
	set_mana(ch, mana);
	
	// check for inventory size
	if (!IS_NPC(ch) && CAN_CARRY_N(ch) > GET_LARGEST_INVENTORY(ch)) {
		GET_LARGEST_INVENTORY(ch) = CAN_CARRY_N(ch);
	}
	
	// this is to prevent weird quirks because GET_MAX_BLOOD is a function
	GET_MAX_POOL(ch, BLOOD) = GET_MAX_BLOOD(ch);
	
	// check greatness thresholds
	if (!IS_NPC(ch) && GET_GREATNESS(ch) != greatness && GET_LOYALTY(ch) && IN_ROOM(ch)) {
		update_member_data(ch);
		TRIGGER_DELAYED_REFRESH(GET_LOYALTY(ch), DELAY_REFRESH_GREATNESS);
	}
	
	// look for changed traits if player has trait hooks
	if (!IS_NPC(ch)) {
		queue_delayed_update(ch, CDU_TRAIT_HOOKS);
	}
	
	// delayed re-send of msdp affects
	if (ch->desc) {
		queue_delayed_update(ch, CDU_MSDP_AFFECTS | CDU_MSDP_ATTRIBUTES);
	}
}


/**
* Ensures a room's affects are up-to-date.
*
* @param room_data *room The room to check.
*/
void affect_total_room(room_data *room) {
	bitvector_t old_affs = ROOM_AFF_FLAGS(room);
	struct affected_type *af;
	vehicle_data *veh;
	
	// reset to base
	ROOM_AFF_FLAGS(room) = ROOM_BASE_FLAGS(room);
	
	// flags from affs
	LL_FOREACH(ROOM_AFFECTS(room), af) {
		SET_BIT(ROOM_AFF_FLAGS(room), af->bitvector);
	}
	
	// flags from vehicles: do this even if incomplete
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (!VEH_IS_EXTRACTED(veh)) {
			SET_BIT(ROOM_AFF_FLAGS(room), VEH_ROOM_AFFECTS(veh));
			
			if (VEH_IS_VISIBLE_ON_MAPOUT(veh)) {
				SET_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_MAPOUT_BUILDING);
			}
		}
	}
	
	// flags from building: don't use IS_COMPLETE because this function may be called before resources are added
	if (GET_BUILDING(room) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_INCOMPLETE)) {
		SET_BIT(ROOM_AFF_FLAGS(room), GET_BLD_BASE_AFFECTS(GET_BUILDING(room)));
	}
	
	// flags from template
	if (GET_ROOM_TEMPLATE(room)) {
		SET_BIT(ROOM_AFF_FLAGS(room), GET_RMT_BASE_AFFECTS(GET_ROOM_TEMPLATE(room)));
	}
	
	// if unclaimable changed, update room lights
	if (IS_SET(ROOM_AFF_FLAGS(room), ROOM_AFF_UNCLAIMABLE) != IS_SET(old_affs, ROOM_AFF_UNCLAIMABLE)) {
		reset_light_count(room);
	}
	
	// if chameleon changed, update mapout
	if (IS_SET(ROOM_AFF_FLAGS(room), ROOM_AFF_CHAMELEON) != IS_SET(old_affs, ROOM_AFF_CHAMELEON)) {
		request_mapout_update(GET_ROOM_VNUM(room));
	}
	
	// check for map save
	if (ROOM_AFF_FLAGS(room) != old_affs) {
		request_world_save(GET_ROOM_VNUM(room), WSAVE_MAP | WSAVE_ROOM);
	}
}


/**
* @param char_data *ch The person to check
* @param any_vnum type Any ATYPE_ const/vnum
* @return bool TRUE if ch is affected by anything with matching type
*/
bool affected_by_spell(char_data *ch, any_vnum type) {
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
* Matches both an ATYPE_ and an APPLY_ and/or AFF_ on an effect.
*
* @param char_data *ch The character to check
* @param any_vnum type the ATYPE_ const/vnum
* @param int apply the APPLY_ flag, if any (NOTHING to check only aff_flag instead)
* @param bitvector_t aff_flag the AFF_ flag, if any (NOBITS to check only apply instead)
* @return bool TRUE if an effect matches both conditions
*/
bool affected_by_spell_and_apply(char_data *ch, any_vnum type, int apply, bitvector_t aff_flag) {
	struct affected_type *hjp;
	bool found = FALSE;
	
	if (apply == NOTHING && aff_flag == NOBITS) {
		return FALSE;	// nothing to look for
	}

	for (hjp = ch->affected; hjp && !found; hjp = hjp->next) {
		if (hjp->type != type) {
			continue;
		}
		if (apply != NOTHING && hjp->location != apply) {
			continue;
		}
		if (aff_flag && !IS_SET(hjp->bitvector, aff_flag)) {
			continue;
		}
		
		found = TRUE;
		break;
	}

	return found;
}


/**
* Matches both an ATYPE or affect generic, and a caster ID.
*
* @param char_data *ch The person to look for an affect on.
* @param any_vnum type The ATYPE_ const or affect generic.
* @param char_data *caster The caster to look for.
* @return bool TRUE if so-affected, FALSE if not.
*/
bool affected_by_spell_from_caster(char_data *ch, any_vnum type, char_data *caster) {
	struct affected_type *hjp;
	bool found = FALSE;
	
	for (hjp = ch->affected; hjp && !found; hjp = hjp->next) {
		if (hjp->type == type && hjp->cast_by == CAST_BY_ID(caster)) {
			found = TRUE;
		}
	}
	
	return found;
}


/**
* Create an affect that modifies a trait.
*
* @param any_vnum type ATYPE_ const/vnum
* @param int duration in seconds (prior to b5.152, this was 5-second "real update" ticks)
* @param int location APPLY_
* @param int modifier +/- amount
* @param bitvector_t bitvector AFF_
* @param char_data *cast_by The caster who made the effect (may be NULL; use the person themselves for penalty effects as those won't cleanse).
* @return struct affected_type* The created af
*/
struct affected_type *create_aff(any_vnum type, int duration, int location, int modifier, bitvector_t bitvector, char_data *cast_by) {
	struct affected_type *af;
	
	CREATE(af, struct affected_type, 1);
	af->type = type;
	af->cast_by = cast_by ? CAST_BY_ID(cast_by) : 0;
	af->expire_time = (duration == UNLIMITED) ? UNLIMITED : time(0) + duration;
	af->modifier = modifier;
	af->location = location;
	af->bitvector = bitvector;
			
	return af;
}


/**
* Applies a DOT (damage over time) effect to a character and schedules it. If
* the duration is not divisible by 5, it may last slightly longer.
*
* Note that prior to b5.152, this took a duration in 5-second "real update"
* ticks, not in seconds.
*
* @param char_data *ch Person receiving the DoT.
* @param any_vnum type ATYPE_ const/vnum that caused it.
* @param int seconds_duration How long, in seconds, to apply this (will round up to a multple of 5).
* @param sh_int damage_type DAM_ type.
* @param sh_int damage How much damage to do per 5-seconds.
* @param sh_int max_stack Number of times this can stack when re-applied before it expires.
* @param sh_int char_data *cast_by The caster.
*/
void apply_dot_effect(char_data *ch, any_vnum type, int seconds_duration, sh_int damage_type, sh_int damage, sh_int max_stack, char_data *cast_by) {
	struct over_time_effect_type *iter, *dot = NULL;
	generic_data *gen;
	int id = (cast_by ? CAST_BY_ID(cast_by) : 0);
	
	// adjust duration to ensure a multiple of 5 seconds
	seconds_duration = (int) (ceil(seconds_duration / ((double)DOT_INTERVAL)) * DOT_INTERVAL);
	seconds_duration = MAX(DOT_INTERVAL, seconds_duration);
	
	// first see if they already have one
	LL_FOREACH(ch->over_time_effects, iter) {
		if (iter->type == type && iter->cast_by == id && iter->damage_type == damage_type && iter->damage == damage) {
			// refresh timer (if longer)
			iter->time_remaining = MAX(iter->time_remaining, seconds_duration);
			
			// take larger of the two stack sizes, and stack it
			iter->max_stack = MAX(iter->max_stack, max_stack);
			if (iter->stack < iter->max_stack) {
				++iter->stack;
			}
			
			dot = iter;
			break;	// only need 1
		}
	}
	
	if (!dot) {
		CREATE(dot, struct over_time_effect_type, 1);
		LL_PREPEND(ch->over_time_effects, dot);
		
		dot->type = type;
		dot->cast_by = id;
		dot->time_remaining = seconds_duration;
		dot->damage_type = damage_type;
		dot->damage = damage;
		dot->stack = 1;
		dot->max_stack = max_stack;
	}
	
	// schedule it -- this will reschedule if it was combined
	schedule_dot_update(ch, dot);
	queue_delayed_update(ch, CDU_MSDP_DOTS);
	
	// any messaging
	if ((gen = find_generic(type, GENERIC_AFFECT))) {
		if (GET_AFFECT_APPLY_TO_CHAR(gen)) {
			act(GET_AFFECT_APPLY_TO_CHAR(gen), FALSE, ch, NULL, NULL, TO_CHAR | ACT_AFFECT);
		}
		if (GET_AFFECT_APPLY_TO_ROOM(gen)) {
			act(GET_AFFECT_APPLY_TO_ROOM(gen), TRUE, ch, NULL, NULL, TO_ROOM | ACT_AFFECT);
		}
	}
}


/**
* Remove a Damage-Over-Time effect from a character.
*
* @param char_data *ch The person to remove the DoT from.
* @param struct over_time_effect_type *dot The DoT to remove.
*/
void dot_remove(char_data *ch, struct over_time_effect_type *dot) {
	// cancel event?
	if (dot->update_event) {
		dg_event_cancel(dot->update_event, cancel_dot_event);
		dot->update_event = NULL;
	}
	
	// remove from list and send it off to be freed
	LL_DELETE(ch->over_time_effects, dot);
	dot->time_remaining = 0;	// prevent any accidental use
	LL_PREPEND(free_dots_list, dot);
	
	queue_delayed_update(ch, CDU_MSDP_DOTS);
}


/**
* Damage-over-time effects (DOTs) are stored in a list and freed at a time when
* no DOTs can be processing. The reason for this is that DOTs can kill the
* character, causing the DOT to be removed by that death while it's still
* processing in the DG event func.
*
* This is called right after DG event updates.
*/
void free_freeable_dots(void) {
	struct over_time_effect_type *dot, *next_dot;
	
	LL_FOREACH_SAFE(free_dots_list, dot, next_dot) {
		// this should NOT be scheduled, but double-check now
		if (dot->update_event) {
			dg_event_cancel(dot->update_event, cancel_dot_event);
			dot->update_event = NULL;
		}
		
		free(dot);
	}
	free_dots_list = NULL;
}


/**
* Removes the first affect a character has that gives them a certain aff flag.
* If this affect isn't a basic "buff" or script "affect", it will also remove
* other affects on the character with the same affect type. If it cannot find
* an affect granting the aff flag, it will remove the aff from the character's
* base affects (e.g. a mob who had the aff built in).
*
* @param char_data *ch The character to remove from.
* @param bitvector_t aff_flag Which AFF_ flag to remove.
* @param bool show_msg If TRUE, will show any affect wear-off message.
*/
void remove_first_aff_flag_from_char(char_data *ch, bitvector_t aff_flag, bool show_msg) {
	struct affected_type *aff;
	bool removed = FALSE;
	
	LL_FOREACH(ch->affected, aff) {
		if (IS_SET(aff->bitvector, AFF_COUNTERSPELL)) {
			removed = TRUE;
			
			if (aff->type == ATYPE_BUFF || aff->type == ATYPE_DG_AFFECT) {
				// basic buff: only remove this one
				if (show_msg) {
					show_wear_off_msg(ch, aff->type);
				}
				affect_remove(ch, aff);
				affect_total(ch);
			}
			else {
				// other types: remove ALL affs of the type
				affect_from_char(ch, aff->type, show_msg);
			}
			
			// done either way: only removing 1
			break;
		}
	}
	
	if (!removed) {
		// has a aff flag that's not from an affect?
		REMOVE_BIT(AFF_FLAGS(ch), aff_flag);
	}
}


/**
* @param room_data *room The room to check
* @param any_vnum type Any ATYPE_ const/vnum
* @return bool TRUE if the room is affected by the spell
*/
bool room_affected_by_spell(room_data *room, any_vnum type) {
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
* Matches both an ATYPE or affect generic, and a caster ID.
*
* @param room_data *room The room to check.
* @param any_vnum type The ATYPE_ const or affect generic.
* @param char_data *caster The caster to look for.
* @return bool TRUE if so-affected, FALSE if not.
*/
bool room_affected_by_spell_from_caster(room_data *room, any_vnum type, char_data *caster) {
	struct affected_type *hjp;
	bool found = FALSE;
	
	for (hjp = ROOM_AFFECTS(room); hjp && !found; hjp = hjp->next) {
		if (hjp->type == type && hjp->cast_by == CAST_BY_ID(caster)) {
			found = TRUE;
		}
	}
	
	return found;
}


/**
* Schedules the event handler for a character's affect expiration.
*
* @param char_data *ch The person with the affect on them.
* @param struct affected_type *af The affect (already on the person) to set up expiry for.
*/
void schedule_affect_expire(char_data *ch, struct affected_type *af) {
	struct affect_expire_event_data *expire_data;
	
	// check for and remove old event
	if (af && af->expire_event) {
		dg_event_cancel(af->expire_event, cancel_affect_expire_event);
		af->expire_event = NULL;
	}
	
	// safety next
	if (!ch || !af) {
		return;
	}
	
	// check durations
	if (af->expire_time == UNLIMITED) {
		af->expire_event = NULL;	// ensure null
	}
	else if (!IN_ROOM(ch) || (!IS_NPC(ch) && !AFFECTS_CONVERTED(ch))) {
		// do not schedule in this case?
		// no work
	}
	else {
		// create the event
		CREATE(expire_data, struct affect_expire_event_data, 1);
		expire_data->character = ch;
		expire_data->affect = af;
		
		af->expire_event = dg_event_create(affect_expire_event, (void*)expire_data, (af->expire_time - time(0)) RL_SEC);
	}
}


/**
* Schedules the event handler for damage-over-time (DOT) effects.
*
* @param char_data *ch The person with the DOT on them.
* @param struct over_time_effect_type *dot The DOT (already on the person) to set up an event for.
*/
void schedule_dot_update(char_data *ch, struct over_time_effect_type *dot) {
	struct dot_event_data *expire_data;
	
	// check for and remove old event
	/*
	if (dot && dot->update_event) {
		dg_event_cancel(dot->update_event, cancel_dot_event);
		dot->update_event = NULL;
	}
	*/
	
	// safety next
	if (!ch || !dot) {
		return;
	}
	if (!IN_ROOM(ch)) {
		return; // do not schedule in this case? player is not in-game
	}
	if (dot->update_event) {
		return;	// already scheduled
	}
	
	// ok: create the event
	CREATE(expire_data, struct dot_event_data, 1);
	expire_data->ch = ch;
	expire_data->dot = dot;
	
	dot->update_event = dg_event_create(dot_update_event, (void*)expire_data, DOT_INTERVAL RL_SEC);
}


/**
* Schedules the event handler for a room's affect expiration.
*
* @param room_data *room The room with the effect on it.
* @param struct affected_type *af The affect (already on the room) to set up expiry for.
*/
void schedule_room_affect_expire(room_data *room, struct affected_type *af) {
	struct room_expire_event_data *expire_data;
	
	// check for and remove old event
	if (af->expire_event) {
		dg_event_cancel(af->expire_event, cancel_room_expire_event);
		af->expire_event = NULL;
	}
	
	if (af->expire_time != UNLIMITED) {
		// create the event
		CREATE(expire_data, struct room_expire_event_data, 1);
		expire_data->room = room;
		expire_data->affect = af;
		
		af->expire_event = dg_event_create(room_affect_expire_event, (void*)expire_data, (af->expire_time - time(0)) RL_SEC);
	}
	else {
		af->expire_event = NULL;	// ensure null
	}
}


/**
* Shows the affect-wear-off message for a given type.
*
* @param char_data *ch The person wearing off of.
* @param any_vnum atype The ATYPE_ affect type.
*/
void show_wear_off_msg(char_data *ch, any_vnum atype) {
	generic_data *gen = find_generic(atype, GENERIC_AFFECT);
	char_data *vict;
	
	if (!ch || !IN_ROOM(ch) || !gen) {
		return;	// no work need doing
	}
	
	if (GET_AFFECT_WEAR_OFF_TO_CHAR(gen) && ch->desc && (IS_NPC(ch) || GET_LAST_AFF_WEAR_OFF_ID(ch) != CAST_BY_ID(ch) || GET_LAST_AFF_WEAR_OFF_VNUM(ch) != atype || GET_LAST_AFF_WEAR_OFF_TIME(ch) != time(0))) {
		msg_to_char(ch, "&%c%s&0\r\n", CUSTOM_COLOR_CHAR(ch, CUSTOM_COLOR_STATUS), GET_AFFECT_WEAR_OFF_TO_CHAR(gen));
		if (!IS_NPC(ch)) {
			GET_LAST_AFF_WEAR_OFF_ID(ch) = CAST_BY_ID(ch);
			GET_LAST_AFF_WEAR_OFF_VNUM(ch) = atype;
			GET_LAST_AFF_WEAR_OFF_TIME(ch) = time(0);
		}
	}
	if (GET_AFFECT_WEAR_OFF_TO_ROOM(gen)) {
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
			if (vict->desc && (IS_NPC(vict) || GET_LAST_AFF_WEAR_OFF_ID(vict) != CAST_BY_ID(ch) || GET_LAST_AFF_WEAR_OFF_VNUM(vict) != atype || GET_LAST_AFF_WEAR_OFF_TIME(vict) != time(0))) {
				act(GET_AFFECT_WEAR_OFF_TO_ROOM(gen), TRUE, ch, NULL, vict, TO_VICT);
				if (!IS_NPC(vict)) {
					GET_LAST_AFF_WEAR_OFF_ID(vict) = CAST_BY_ID(ch);
					GET_LAST_AFF_WEAR_OFF_VNUM(vict) = atype;
					GET_LAST_AFF_WEAR_OFF_TIME(vict) = time(0);
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////////
//// CHARACTER HANDLERS /////////////////////////////////////////////////////

/* Extract a ch completely from the world, and leave his stuff behind */
void extract_char_final(char_data *ch) {
	empire_data *rescan_emp = IS_NPC(ch) ? NULL : GET_LOYALTY(ch);
	char_data *k;
	char_data *chiter;
	descriptor_data *t_desc;
	obj_data *obj;
	int i;
	bool freed = FALSE;
	struct pursuit_data *purs;
	
	// sanitation checks
	if (!IN_ROOM(ch)) {
		log("SYSERR: Extracting char %s not in any room. (%s, extract_char_final)", GET_NAME(ch), __FILE__);
		exit(1);
	}
	
	// shut this off -- no need to total during an extract
	pause_affect_total = TRUE;
	
	// update iterators
	if (ch == global_next_player) {
		global_next_player = global_next_player->next_plr;
	}
	
	check_dg_owner_purged_char(ch);
	cancel_all_stored_events(&GET_STORED_EVENTS(ch));

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
	
	// check I'm not being used by someone's action
	DL_FOREACH2(player_character_list, chiter, next_plr) {
		if (GET_ACTION_CHAR_TARG(chiter) == ch) {
			GET_ACTION_CHAR_TARG(chiter) = NULL;
		}
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
	LL_FOREACH_SAFE2(combat_list, k, next_combat_list, next_fighting) {
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
	
	pause_affect_total = FALSE;
	
	// update empire numbers -- only if we detected empire membership back at the beginning
	// this prevents incorrect greatness or other traits on logout
	if (rescan_emp) {
		TRIGGER_DELAYED_REFRESH(rescan_emp, DELAY_REFRESH_MEMBERS);
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
	char_data *chiter;
	
	// update iterators
	if (ch == global_next_player) {
		global_next_player = global_next_player->next_plr;
	}
	
	if (!EXTRACTED(ch)) {
		check_dg_owner_purged_char(ch);
		
		if (IS_NPC(ch)) {
			SET_BIT(MOB_FLAGS(ch), MOB_EXTRACTED);
			
			if (MOB_INSTANCE_ID(ch) != NOTHING) {
				subtract_instance_mob(real_instance(MOB_INSTANCE_ID(ch)), GET_MOB_VNUM(ch));
			}
			
			if (GET_LOYALTY(ch)) {
				remove_from_workforce_where_log(GET_LOYALTY(ch), ch);
			}
		}
		else {
			SET_BIT(PLR_FLAGS(ch), PLR_EXTRACTED);
		}
		++char_extractions_pending;
	}
	
	// check I'm not being used by someone's action
	DL_FOREACH2(player_character_list, chiter, next_plr) {
		if (GET_ACTION_CHAR_TARG(chiter) == ch) {
			GET_ACTION_CHAR_TARG(chiter) = NULL;
		}
	}
	
	// get rid of friends now (extracts them as well)
	despawn_charmies(ch, NOTHING);
	
	// remove dots to avoid one firing mid-extract
	while (ch->over_time_effects) {
		dot_remove(ch, ch->over_time_effects);
	}
	
	// ensure no stored events
	cancel_all_stored_events(&GET_STORED_EVENTS(ch));
	
	// clear companion (both directions) if any
	if (GET_COMPANION(ch)) {
		if (!IS_NPC(ch)) {
			GET_LAST_COMPANION(ch) = NOTHING;
		}
		if (!IS_NPC(GET_COMPANION(ch))) {
			GET_LAST_COMPANION(GET_COMPANION(ch)) = NOTHING;
		}
		if (GET_COMPANION(GET_COMPANION(ch)) == ch) {
			GET_COMPANION(GET_COMPANION(ch)) = NULL;
		}
		GET_COMPANION(ch) = NULL;
	}
	
	// end following now
	if (ch->followers || GET_LEADER(ch)) {
		die_follower(ch);
	}
	
	// if any:
	clear_delayed_update(ch);
}


/* I'm not particularly pleased with the MOB/PLR hoops that have to be jumped
 * through but it hardly calls for a completely new variable. Ideally it would
 * be its own list, but that would change the '->next' pointer, potentially
 * confusing some code. -gg This doesn't handle recursive extractions. */
void extract_pending_chars(void) {
	char_data *vict, *next_vict;

	if (char_extractions_pending < 0) {
		log("SYSERR: Negative (%d) character extractions pending.", char_extractions_pending);
	}
	
	DL_FOREACH_SAFE(character_list, vict, next_vict) {
		// check if done?
		if (char_extractions_pending <= 0) {
			break;
		}
		
		if (MOB_FLAGGED(vict, MOB_EXTRACTED)) {
			REMOVE_BIT(MOB_FLAGS(vict), MOB_EXTRACTED);
		}
		else if (!IS_NPC(vict) && PLR_FLAGGED(vict, PLR_EXTRACTED)) {
			REMOVE_BIT(PLR_FLAGS(vict), PLR_EXTRACTED);
		}
		else {	// not extracting
			continue;
		}
		
		// ensure they're really (probably) in the character list
		if (character_list && (character_list == vict || vict->prev || vict->next)) {
			DL_DELETE(character_list, vict);
		}
		if (!IS_NPC(vict) && player_character_list && (player_character_list == vict || vict->prev_plr || vict->next_plr)) {
			DL_DELETE2(player_character_list, vict, prev_plr, next_plr);
		}

		// moving this down below the prev_vict block because ch was still in
		// the character list late in the process, causing a crash in some rare
		// cases -pc 1/14/2015
		extract_char_final(vict);
		--char_extractions_pending;
	}

	if (char_extractions_pending != 0) {
		if (char_extractions_pending > 0) {
			log("SYSERR: Couldn't find %d character extractions as counted.", char_extractions_pending);
		}
		
		// likely an error -- search for people who need extracting (for next time)
		char_extractions_pending = 0;
		DL_FOREACH(character_list, vict) {
			if (EXTRACTED(vict)) {
				++char_extractions_pending;
			}
		}
	}
	else {
		char_extractions_pending = 0;
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
* @param bool *was_exact Optional; If provided will set to TRUE if the name was an exact match not an abbreviation. (Pass NULL to skip this.)
* @return bool TRUE if "name" is valid for "target" according to "ch", FALSE if not
*/
bool match_char_name(char_data *ch, char_data *target, char *name, bitvector_t flags, bool *was_exact) {
	bool recognize, old_ignore_dark = Global_ignore_dark;
	
	if (was_exact) {
		// initialize
		*was_exact = FALSE;
	}
	
	// shortcut with no name or when UID character requested
	if (!name || !*name || (*name == UID_CHAR && isdigit(*(name+1)))) {
		return FALSE;
	}
	
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
	if (recognize && isname_check_exact(name, GET_PC_NAME(target), was_exact)) {
		return TRUE;	// name/kw match
	}
	else if (recognize && !IS_NPC(target) && GET_CURRENT_LASTNAME(target) && isname_check_exact(name, GET_CURRENT_LASTNAME(target), was_exact)) {
		return TRUE;	// lastname match
	}
	else if (IS_MORPHED(target) && isname_check_exact(name, MORPH_KEYWORDS(GET_MORPH(target)), was_exact)) {
		return TRUE;	// morph kw match
	}
	else if (IS_DISGUISED(target) && isname_check_exact(name, GET_DISGUISED_NAME(target), was_exact)) {
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
* @return bool TRUE if the character is still in, FALSE if extracted
*/
bool perform_idle_out(char_data *ch) {
	empire_data *emp = NULL;
	bool died = FALSE;
	
	if (!ch) {
		return FALSE;
	}
	
	// block idle-out entirely with this prf
	if (ch->desc && PRF_FLAGGED(ch, PRF_NO_IDLE_OUT)) {
		return TRUE;
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
	dismiss_any_minipet(ch);
	despawn_companion(ch, NOTHING);
	
	syslog(SYS_LOGIN, GET_INVIS_LEV(ch), TRUE, "%s force-rented and extracted (idle) at %s", GET_NAME(ch), IN_ROOM(ch) ? room_log_identifier(IN_ROOM(ch)) : "an unknown location");
	
	pause_affect_total = TRUE;	// save unnecessary processing
	extract_all_items(ch);
	extract_char(ch);
	pause_affect_total = FALSE;
	
	if (emp) {
		extract_pending_chars();	// ensure char is gone
	}
	
	return FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER LOCATION HANDLERS /////////////////////////////////////////////


/**
* move a player out of a room
*
* @param char_data *ch The character to remove
*/
void char_from_room(char_data *ch) {
	if (ch == NULL || !IN_ROOM(ch)) {
		log("SYSERR: NULL character or no location in %s, char_from_room", __FILE__);
		exit(1);
	}
	
	if (FIGHTING(ch) != NULL) {
		stop_fighting(ch);
	}
	
	request_char_save_in_world(ch);

	// update lights
	ROOM_LIGHTS(IN_ROOM(ch)) -= GET_LIGHTS(ch);
    
    DL_DELETE2(ROOM_PEOPLE(IN_ROOM(ch)), ch, prev_in_room, next_in_room);
	IN_ROOM(ch) = NULL;
	ch->prev_in_room = NULL;
	ch->next_in_room = NULL;
}


/**
* place a character in a room, or move them from one room to another
*
* @param char_data *ch The person to place
* @param room_data *room The place to put 'em
*/
void char_to_room(char_data *ch, room_data *room) {
	struct instance_data *inst = NULL;

	if (!ch || !room) {
		log("SYSERR: Illegal value(s) passed to char_to_room. (Room :%p, Ch: %p)", room, ch);
	}
	else {
		// check if it needs an instance setup (before putting the character there)
		if (!IS_NPC(ch) && (inst = find_instance_by_room(room, FALSE, TRUE))) {
			check_instance_is_loaded(inst);
		}
		
		if (!IS_NPC(ch)) {
			// day/night can change when moving
			qt_check_day_and_night(ch);
			
			// check npc spawns whenever a player is places in a room
			spawn_mobs_from_center(room);
		}
		
		// sanitation: remove them from the old room first
		if (IN_ROOM(ch)) {
			char_from_room(ch);
		}
		
		DL_PREPEND2(ROOM_PEOPLE(room), ch, prev_in_room, next_in_room);
		IN_ROOM(ch) = room;

		// update lights
		ROOM_LIGHTS(room) += GET_LIGHTS(ch);
		
		if (!IS_NPC(ch) && !IS_IMMORTAL(ch)) {
			check_island_levels(room, (int) GET_COMPUTED_LEVEL(ch));
		}
		
		// look for an instance to lock
		if (!IS_NPC(ch) && IS_ADVENTURE_ROOM(room) && (inst || (inst = find_instance_by_room(room, FALSE, TRUE)))) {
			if (ADVENTURE_FLAGGED(INST_ADVENTURE(inst), ADV_LOCK_LEVEL_ON_ENTER) && !IS_IMMORTAL(ch)) {
				lock_instance_level(room, determine_best_scale_level(ch, TRUE));
			}
		}
		
		if (!IS_NPC(ch)) {
			// store last room to player
			GET_LAST_ROOM(ch) = GET_ROOM_VNUM(room);
		}
		
		schedule_movement_events(ch);
		request_char_save_in_world(ch);
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
	
	if ((vict = get_char_room_vis(ch, arg, NULL)) && (!pc_only || !IS_NPC(vict))) {
		return vict;
	}
	
	DL_FOREACH(character_list, vict) {
		if (pc_only && IS_NPC(vict)) {
			continue;
		}
		if (!CAN_SEE(ch, vict) || !can_see_in_dark_room(ch, IN_ROOM(vict), FALSE)) {
			continue;
		}
		if (!match_char_name(ch, vict, arg, MATCH_IN_ROOM, NULL)) {
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
	
	DL_FOREACH2(ROOM_PEOPLE(room), mob, next_in_room) {
		if (IS_NPC(mob) && GET_MOB_VNUM(mob) == vnum) {
			found = mob;
			break;
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
	
	DL_FOREACH2(ROOM_PEOPLE(room), iter, next_in_room) {
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
	
	DL_FOREACH2(ROOM_PEOPLE(room), i, next_in_room) {
		if (j > number) {
			break;
		}
		else if (match_char_name(NULL, i, tmp, MATCH_IN_ROOM, NULL)) {
			if (++j == number) {
				found = i;
				break;
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
* @param int *number Optional: For multi-list number targeting (look 4.bob; may be NULL)
* @return char_data *A matching character in the room, or NULL.
*/
char_data *get_char_room_vis(char_data *ch, char *name, int *number) {
	char copy[MAX_INPUT_LENGTH], *tmp = copy;
	char_data *i;
	int num;
	
	/* JE 7/18/94 :-) :-) */
	if (!str_cmp(name, "self") || !str_cmp(name, "me"))
		return (ch);
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {	// 0.name means PC
		return get_player_vis(ch, tmp, FIND_CHAR_ROOM);
	}
	
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), i, next_in_room) {
		if (CAN_SEE(ch, i) && WIZHIDE_OK(ch, i) && !AFF_FLAGGED(i, AFF_NO_TARGET_IN_ROOM) && match_char_name(ch, i, tmp, MATCH_IN_ROOM, NULL)) {
			if (--(*number) == 0) {
				return i;
			}
		}
	}

	return NULL;
}


/**
* Find a character (pc or npc) using FIND_x locations.
*
* @param char_data *ch The person who's looking.
* @param char *name The target argument.
* @param int *number Optional: For multi-list number targeting (look 4.bob; may be NULL)
* @param bitvector_t where Any FIND_x flags.
* @return char_data *The found character, or NULL.
*/
char_data *get_char_vis(char_data *ch, char *name, int *number, bitvector_t where) {
	char copy[MAX_INPUT_LENGTH], *tmp = copy;
	char_data *i;
	int num, store;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return get_player_vis(ch, tmp, where);
	}
	
	/* check the room first */
	if (IS_SET(where, FIND_CHAR_ROOM))
		return get_char_room_vis(ch, tmp, number);
	else if (IS_SET(where, FIND_CHAR_WORLD)) {
		store = *number;
		if ((i = get_char_room_vis(ch, tmp, number)) != NULL) {
			return (i);
		}
		
		// if we didn't find it, restore the number, because otherwise it counts the same person twice
		*number = store;
		
		DL_FOREACH(character_list, i) {
			if (IS_SET(where, FIND_NPC_ONLY) && !IS_NPC(i)) {	
				continue;
			}
			if (!match_char_name(ch, i, tmp, (IS_SET(where, FIND_NO_DARK) ? MATCH_GLOBAL : 0), NULL)) {
				continue;
			}
			
			// found
			if (--(*number) == 0) {
				return i;
			}
		}
	}

	return NULL;
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
	bool was_exact = FALSE, had_number;
	char_data *i, *abbrev = NULL, *found = NULL;
	int number;
	
	had_number = (isdigit(*name) ? TRUE : FALSE);
	number = get_number(&name);
	
	DL_FOREACH2(player_character_list, i, next_plr) {
		if (IS_SET(flags, FIND_CHAR_ROOM) && !WIZHIDE_OK(ch, i)) {
			continue;
		}
		if (IS_SET(flags, FIND_CHAR_ROOM) && IN_ROOM(i) != IN_ROOM(ch))
			continue;
		if (IS_SET(flags, FIND_CHAR_ROOM) && AFF_FLAGGED(i, AFF_NO_TARGET_IN_ROOM))
			continue;
		if (!(IS_SET(flags, FIND_NO_DARK) && CAN_SEE_NO_DARK(ch, i)) && !CAN_SEE(ch, i)) {
			continue;
		}
		if (!match_char_name(ch, i, name, (IS_SET(flags, FIND_CHAR_ROOM) ? MATCH_IN_ROOM : 0) | (IS_SET(flags, FIND_NO_DARK | FIND_CHAR_WORLD) ? MATCH_GLOBAL : 0), &was_exact)) {
			continue;
		}
		if (had_number && --number > 0) {
			continue;
		}
		
		if (had_number || was_exact) {
			// perfect match
			found = i;
			break;	// done
		}
		else if (!abbrev) {
			// save for later
			abbrev = i;
		}
	}

	return found ? found : abbrev;	// may be NULL
}


/**
* Searches for a character in the whole world with a matching name, without
* requiring visibility.
*
* @param char *name The name of the target.
* @param int *number Optional: For multi-list number targeting (look 4.bob; may be NULL)
* @return char_data *the found character, or NULL
*/
char_data *get_char_world(char *name, int *number) {
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	char_data *ch;
	int num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return get_player_world(name, number);
	}
	
	DL_FOREACH(character_list, ch) {
		if (match_char_name(NULL, ch, tmp, MATCH_GLOBAL, NULL)) {
			if (--(*number) == 0) {
				return ch;	// done
			}
		}
	}

	return NULL;
}


/**
* Searches for a player in the whole world with a matching name, without
* requiring visibility. This ignores mobs.
*
* @param char *name The name of the target.
* @param int *number Optional: For multi-list number targeting (look 4.bob; may be NULL)
* @return char_data* The found player, or NULL.
*/
char_data *get_player_world(char *name, int *number) {
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	bool had_number, was_exact = FALSE;
	char_data *ch, *abbrev = NULL;
	int num;
	
	if (!number) {
		had_number = (isdigit(*name) ? TRUE : FALSE);
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		had_number = (*number != 1);	// a guess that they provided a number
		tmp = name;
	}
	
	DL_FOREACH2(player_character_list, ch, next_plr) {
		if (!match_char_name(NULL, ch, tmp, MATCH_GLOBAL, &was_exact)) {
			continue;
		}
		if (had_number && --(*number) > 0) {
			continue;
		}
		
		// match!
		if (had_number || was_exact) {
			return ch;	// done
		}
		else if (!abbrev) {
			abbrev = ch;	// for later
		}
	}

	return abbrev;	// if any
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
* @param char *build_string Optional: if not NULL, will build a string of exactly what was charged, e.g. "450 crown coins and 100 miscellaneous coins". This string should ideally be MAX_STRING_LENGTH, but in practice will be way shorter.
*/
void charge_coins(char_data *ch, empire_data *type, int amount, struct resource_data **build_used_list, char *build_string) {
	struct coin_data *coin;
	char temp[1024];
	char *ptr;
	int this, this_amount;
	double rate, inv;
	empire_data *emp;
	
	if (build_string) {
		// initialize
		*build_string = '\0';
	}
	
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
		if (build_string) {
			sprintf(build_string + strlen(build_string), "%s%s", (*build_string ? ", " : ""), money_amount(type, this));
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
		if (build_string) {
			sprintf(build_string + strlen(build_string), "%s%s", (*build_string ? ", " : ""), money_amount(REAL_OTHER_COIN, this));
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
			if (build_string) {
				sprintf(build_string + strlen(build_string), "%s%s", (*build_string ? ", " : ""), money_amount(real_empire(coin->empire_id), this));
			}
		}
	}

	// coins were cleaned up with each increase
	// lastly, look for the last comma
	if (build_string && (ptr = strrchr(build_string, ','))) {
		// and replace comma with and
		strcpy(temp, ptr+1);
		strcpy(ptr, " and");
		strcat(ptr, temp);
	}
}


/**
* Calls cleanup_all_coins on all players in-game and connected.
*/
void cleanup_all_coins(void) {
	char_data *ch;
	descriptor_data *desc;
	
	DL_FOREACH2(player_character_list, ch, next_plr) {
		cleanup_coins(ch);
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
	struct coin_data *iter, *least, *next_iter;
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
				LL_DELETE(GET_PLAYER_COINS(ch), iter);
				free(iter);
			}
			else if (iter->empire_id != OTHER_COIN && real_empire(iter->empire_id) == NULL) {
				// no more empire
				add_other += iter->amount;
				LL_DELETE(GET_PLAYER_COINS(ch), iter);
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
			LL_DELETE(GET_PLAYER_COINS(ch), least);
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
		sprintf(temp, "%s%d %smiscellaneous coin%s", (*local ? ", and " : ""), other->amount, (*local ? "other " : ""), (other->amount != 1 ? "s" : ""));
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
* Counts all of a player's coins, in their value as a specific empire's coins.
*
* @param char_data *ch The player.
* @param empire_data *type An empire whose coins we'll conver to.
* @return int The player's total coins as that currency.
*/
int count_total_coins_as(char_data *ch, empire_data *type) {
	struct coin_data *coin;
	double total;
	
	if (IS_NPC(ch)) {
		return 0;
	}
	
	total = 0;
	LL_FOREACH(GET_PLAYER_COINS(ch), coin) {
		total += exchange_coin_value(coin->amount, real_empire(coin->empire_id), type);
	}
	
	return round(total);
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
	set_obj_keywords(obj, skip_filler(buf));
	set_obj_short_desc(obj, buf);
	snprintf(buf2, sizeof(buf2), "%s is lying here.", CAP(buf));
	set_obj_long_desc(obj, buf2);

	// description
	if (amount == 1) {
		set_obj_look_desc(obj, "It's just one miserable little coin.", TRUE);
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

		set_obj_look_desc(obj, buf, TRUE);
	}

	// data
	obj->proto_data->type_flag = ITEM_COINS;
	obj->proto_data->material = MAT_GOLD;
	SET_BIT(GET_OBJ_WEAR(obj), ITEM_WEAR_TAKE);

	set_obj_val(obj, VAL_COINS_AMOUNT, amount);
	set_obj_val(obj, VAL_COINS_EMPIRE_ID, (!type ? OTHER_COIN : EMPIRE_VNUM(type)));

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
* This parses: <number> <empire | misc> coin[s]
*
* b5.98 adds the require_coin_type arg because some functions REQUIRE it.
*
* @param char *input The input string.
* @param empire_data **emp_found A place to store the found empire id, if any.
* @param int *amount_found The numerical argument.
* @param bool require_coin_type If TRUE, the player MUST type an empire (adjective) or "misc" for the coin type.
* @param bool assume_coins If TRUE, the word "coins" can be omitted.
* @param bool *gave_coin_type Optional: A variable to bind whether or not the person typed a coin type ("10 misc coins" instead of "10 coins")
* @return char* A pointer to the remaining argument (or the full argument, if no coins).
*/
char *find_coin_arg(char *input, empire_data **emp_found, int *amount_found, bool require_coin_type, bool assume_coins, bool *gave_coin_type) {
	char arg[MAX_INPUT_LENGTH];
	char *pos, *final;
	int amt;
	
	// clear immediately
	*emp_found = NULL;
	*amount_found = 0;
	if (gave_coin_type) {
		*gave_coin_type = FALSE;
	}
	
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
	
	// check for empire arg	// OR coins here
	pos = any_one_word(pos, arg);
	
	// 1st arg may be 'coins' unless the type is REQUIRED
	if (!require_coin_type && (!str_cmp(arg, "coin") || !str_cmp(arg, "coins"))) {
		// no empire arg but we're done
		*amount_found = amt;
		*emp_found = REAL_OTHER_COIN;
		if (gave_coin_type) {
			*gave_coin_type = FALSE;
		}
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
	
	// at this point they must have specified a type
	if (gave_coin_type) {
		*gave_coin_type = TRUE;
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
		LL_PREPEND(GET_PLAYER_COINS(ch), coin);
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
	
	if (amount != 0) {
		qt_change_coins(ch);
		update_MSDP_money(ch, UPDATE_SOON);
	}
	
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
* @param any_vnum type Any cooldown vnum.
* @param int seconds_duration How long it lasts.
*/
void add_cooldown(char_data *ch, any_vnum type, int seconds_duration) {
	struct cooldown_data *cool;
	struct cooldown_expire_event_data *data;
	bool found = FALSE;
	
	if (!find_generic(type, GENERIC_COOLDOWN)) {
		log("SYSERR: add_cooldown called with invalid cooldown vnum %d", type);
		return;
	}
	
	// check for existing
	for (cool = ch->cooldowns; cool && !found; cool = cool->next) {
		if (cool->type == type) {
			found = TRUE;
			cool->expire_time = MAX(cool->expire_time, time(0) + seconds_duration);
			if (cool->expire_event) {
				dg_event_cancel(cool->expire_event, cancel_cooldown_event);
				cool->expire_event = NULL;
			}
			break;	// only 1
		}
	}
	
	// add?
	if (!found && seconds_duration > 0) {
		CREATE(cool, struct cooldown_data, 1);
		cool->type = type;
		cool->expire_time = time(0) + seconds_duration;
		LL_PREPEND(ch->cooldowns, cool);
	}
	
	// schedule?
	if (cool) {
		CREATE(data, struct cooldown_expire_event_data, 1);
		data->character = ch;
		data->cooldown = cool;
		cool->expire_event = dg_event_create(cooldown_expire_event, data, (cool->expire_time - time(0)) RL_SEC);
	}
	
	if (ch->desc) {
		queue_delayed_update(ch, CDU_MSDP_COOLDOWNS);
	}
	if (IS_NPC(ch)) {
		// only save mobs for this. players don't need it
		request_char_save_in_world(ch);
	}
}


// canceller for cooldown events
EVENT_CANCEL_FUNC(cancel_cooldown_event) {
	struct cooldown_expire_event_data *data = (struct cooldown_expire_event_data*)event_obj;
	free(data);
}


// called when a cooldown times out
EVENTFUNC(cooldown_expire_event) {
	struct cooldown_expire_event_data *data = (struct cooldown_expire_event_data*)event_obj;
	char_data *ch = data->character;
	struct cooldown_data *cool = data->cooldown;
	generic_data *gen;
	
	// always delete first
	cool->expire_event = NULL;
	free(data);
	
	// messaging is a maybe
	if (SHOW_STATUS_MESSAGES(ch, SM_COOLDOWNS) && IN_ROOM(ch) && (gen = find_generic(cool->type, GENERIC_COOLDOWN)) && GET_COOLDOWN_WEAR_OFF(gen)) {
		msg_to_char(ch, "\t%c%s\t0\r\n", CUSTOM_COLOR_CHAR(ch, CUSTOM_COLOR_STATUS), GET_COOLDOWN_WEAR_OFF(gen));
	}
	
	remove_cooldown(ch, cool);
	return 0;	// do not re-enqueue
}


/**
* Frees a cooldown after ensuring it doesn't have an expiry event scheduled.
*
* @param struct cooldown_data *cool The cooldown.
*/
void free_cooldown(struct cooldown_data *cool) {
	if (cool) {
		if (cool->expire_event) {
			dg_event_cancel(cool->expire_event, cancel_cooldown_event);
			cool->expire_event = NULL;
		}
		
		free(cool);
	}
}


/**
* Returns the time left on a cooldown of a given type, or 0 if the character
* does not have that ability on cooldown.
*
* @param char_data *ch The character.
* @param any_vnum type Any cooldown vnum.
* @return int The time remaining on the cooldown (in seconds), or 0.
*/
int get_cooldown_time(char_data *ch, any_vnum type) {
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
	LL_DELETE(ch->cooldowns, cool);
	free_cooldown(cool);
	
	if (ch->desc) {
		queue_delayed_update(ch, CDU_MSDP_COOLDOWNS);
	}
	if (IS_NPC(ch)) {
		// only mobs need a save for this
		request_char_save_in_world(ch);
	}
}


/**
* Removes any cooldowns of a given type from the character.
*
* @param char_data *ch The character.
* @param any_vnum type Any cooldown vnum.
*/
void remove_cooldown_by_type(char_data *ch, any_vnum type) {
	struct cooldown_data *cool, *next_cool;
	
	for (cool = ch->cooldowns; cool; cool = next_cool) {
		next_cool = cool->next;
		
		if (cool->type == type) {
			remove_cooldown(ch, cool);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CURRENCY HANDLERS ///////////////////////////////////////////////////////

/**
* Adds (or removes) adventure currencies for the player.
*
* @param char_data *ch The player.
* @param any_vnum vnum The currency (generic) vnum.
* @param int amount The amount to add (or remove).
* @return int The player's new total.
*/
int add_currency(char_data *ch, any_vnum vnum, int amount) {
	struct player_currency *cur;
	
	if (IS_NPC(ch) || !find_generic(vnum, GENERIC_CURRENCY)) {
		return 0;
	}
	if (amount == 0) {
		return get_currency(ch, vnum);
	}
	
	HASH_FIND_INT(GET_CURRENCIES(ch), &vnum, cur);
	if (!cur) {
		CREATE(cur, struct player_currency, 1);
		cur->vnum = vnum;
		HASH_ADD_INT(GET_CURRENCIES(ch), vnum, cur);
	}
	
	SAFE_ADD(cur->amount, amount, 0, INT_MAX, FALSE);
	qt_change_currency(ch, vnum, cur->amount);
	
	// housecleaning
	if (cur->amount <= 0) {
		HASH_DEL(GET_CURRENCIES(ch), cur);
		free(cur);
		return 0;
	}
	else {
		return cur->amount;
	}
}


/**
* Checks a player's adventure currency.
*
* @param char_data *ch The player.
* @param any_vnum vnum The currency (generic) vnum.
* @return int The amount the player has.
*/
int get_currency(char_data *ch, any_vnum vnum) {
	struct player_currency *cur;
	
	if (IS_NPC(ch)) {
		return 0;
	}
	
	HASH_FIND_INT(GET_CURRENCIES(ch), &vnum, cur);
	return cur ? cur->amount : 0;
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE HANDLERS /////////////////////////////////////////////////////////

/**
* This function abandons any room and all its associated rooms.
*
* @param room_data *room The room to abandon.
*/
void abandon_room(room_data *room) {
	room_data *iter, *next_iter, *home = HOME_ROOM(room);
	
	if (ROOM_PRIVATE_OWNER(room) != NOBODY) {
		clear_private_owner(ROOM_PRIVATE_OWNER(room));
	}
	
	// inside
	DL_FOREACH_SAFE2(interior_room_list, iter, next_iter, next_interior) {
		if (iter != home && HOME_ROOM(iter) == home) {
			perform_abandon_room(iter);
		}
	}
	
	// do room itself last -- this fixes a bug where citizens weren't deducted because the home room was already abandoned
	perform_abandon_room(room);
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
	
	DL_FOREACH_SAFE2(interior_room_list, iter, next_iter, next_interior) {
		if (iter != home && HOME_ROOM(iter) == home) {
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
	LL_PREPEND(EMPIRE_DIPLOMACY(a), pol);
	
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
	struct empire_territory_data *ter;
	room_vnum vnum = GET_ROOM_VNUM(room);
	
	if (emp) {
		HASH_FIND_INT(EMPIRE_TERRITORY_LIST(emp), &vnum, ter);
		return ter;	// if any
	}
	
	return NULL;
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
		SAFE_ADD_DOUBLE(EMPIRE_COINS(emp_gaining), amount, 0, MAX_COIN, FALSE);
		et_change_coins(emp_gaining, amount);
	}
	else {
		if ((local = exchange_coin_value(amount, coin_empire, emp_gaining)) > 0) {
			SAFE_ADD_DOUBLE(EMPIRE_COINS(emp_gaining), local, 0, MAX_COIN, FALSE);
			et_change_coins(emp_gaining, local);
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
	empire_data *emp = ROOM_OWNER(room);
	struct empire_territory_data *ter;
	vehicle_data *veh;
	int ter_type;
	bool junk;
	
	// updates based on owner
	if (emp) {
		// update any building-flagged vehicles
		DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
			if (VEH_OWNER(veh) == emp && VEH_CLAIMS_WITH_ROOM(veh)) {
				perform_abandon_vehicle(veh);
			}
		}
		
		deactivate_workforce_room(emp, room);
		adjust_building_tech(emp, room, FALSE);
		
		// update territory counts
		if (COUNTS_AS_TERRITORY(room)) {
			struct empire_island *eisle = get_empire_island(emp, GET_ISLAND_ID(room));
			ter_type = get_territory_type_for_empire(room, emp, FALSE, &junk, NULL);
			
			SAFE_ADD(EMPIRE_TERRITORY(emp, ter_type), -1, 0, UINT_MAX, FALSE);
			SAFE_ADD(eisle->territory[ter_type], -1, 0, UINT_MAX, FALSE);
			
			SAFE_ADD(EMPIRE_TERRITORY(emp, TER_TOTAL), -1, 0, UINT_MAX, FALSE);
			SAFE_ADD(eisle->territory[TER_TOTAL], -1, 0, UINT_MAX, FALSE);
		}
		// territory list
		if ((ter = find_territory_entry(emp, room))) {
			delete_territory_entry(emp, ter, TRUE);
		}
		
		// quest tracker for members
		qt_empire_players(emp, qt_lose_tile_sector, GET_SECT_VNUM(SECT(room)));
		et_lose_tile_sector(emp, GET_SECT_VNUM(SECT(room)));
		if (GET_BUILDING(room) && IS_COMPLETE(room)) {
			qt_empire_players(emp, qt_lose_building, GET_BLD_VNUM(GET_BUILDING(room)));
			et_lose_building(emp, GET_BLD_VNUM(GET_BUILDING(room)));
		}
		
		remove_dropped_item_list(emp, ROOM_CONTENTS(room));
	}
	
	ROOM_OWNER(room) = NULL;

	REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_PUBLIC | ROOM_AFF_NO_WORK | ROOM_AFF_NO_ABANDON | ROOM_AFF_NO_DISMANTLE);

	if (ROOM_PRIVATE_OWNER(room) != NOBODY) {
		set_private_owner(room, NOBODY);
	}
	
	// reschedule unload check now that it's unowned
	if (GET_ROOM_VNUM(room) < MAP_SIZE) {
		schedule_check_unload(room, FALSE);
	}
	
	// if a city center is abandoned, destroy it
	if (IS_CITY_CENTER(room)) {
		disassociate_building(room);
	}
	else {	// other building types
		check_tavern_setup(room);
	}
	
	affect_total_room(room);
	update_MSDP_empire_data_all(emp, TRUE, TRUE);
	request_mapout_update(GET_ROOM_VNUM(room));
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM | WSAVE_MAP);
}


/**
* Removes ownership of a vehicle.
*
* @param vehicle_data *veh The vehicle to abandon.
*/
void perform_abandon_vehicle(vehicle_data *veh) {
	if (veh) {
		empire_data *emp = VEH_OWNER(veh);
		bool provided_light = VEH_PROVIDES_LIGHT(veh);
		
		remove_vehicle_flags(veh, VEH_PLAYER_NO_WORK | VEH_PLAYER_NO_DISMANTLE);
	
		if (VEH_INTERIOR_HOME_ROOM(veh)) {
			abandon_room(VEH_INTERIOR_HOME_ROOM(veh));
		}
		
		adjust_vehicle_tech(veh, FALSE);
		VEH_OWNER(veh) = NULL;
		
		if (VEH_IS_COMPLETE(veh) && emp) {
			qt_empire_players_vehicle(emp, qt_lose_vehicle, veh);
			et_lose_vehicle(emp, veh);
		}
		
		if (emp) {
			remove_dropped_item_list(emp, VEH_CONTAINS(veh));
		}
		
		// check if light changed
		if (IN_ROOM(veh)) {
			if (!VEH_PROVIDES_LIGHT(veh) && provided_light) {
				--ROOM_LIGHTS(IN_ROOM(veh));
			}
			else if (VEH_PROVIDES_LIGHT(veh) && !provided_light) {
				++ROOM_LIGHTS(IN_ROOM(veh));
			}
		}
		
		request_vehicle_save_in_world(veh);
	}
}


/**
* Called by claim_room() to do the actual work.
*
* @param room_data *room The room to claim.
* @param empire_data *emp The empire to claim for.
*/
void perform_claim_room(room_data *room, empire_data *emp) {
	struct empire_territory_data *ter;
	vehicle_data *veh;
	int ter_type;
	bool junk;
	
	ROOM_OWNER(room) = emp;
	remove_room_extra_data(room, ROOM_EXTRA_CEDED);	// not ceded if just claimed
	
	adjust_building_tech(emp, room, TRUE);
	
	// update territory counts
	if (COUNTS_AS_TERRITORY(room)) {
		struct empire_island *eisle = get_empire_island(emp, GET_ISLAND_ID(room));
		ter_type = get_territory_type_for_empire(room, emp, FALSE, &junk, NULL);
		
		SAFE_ADD(EMPIRE_TERRITORY(emp, ter_type), 1, 0, UINT_MAX, FALSE);
		SAFE_ADD(eisle->territory[ter_type], 1, 0, UINT_MAX, FALSE);
		
		SAFE_ADD(EMPIRE_TERRITORY(emp, TER_TOTAL), 1, 0, UINT_MAX, FALSE);
		SAFE_ADD(eisle->territory[TER_TOTAL], 1, 0, UINT_MAX, FALSE);
	}
	// territory list
	if (BELONGS_IN_TERRITORY_LIST(room) && !(ter = find_territory_entry(emp, room))) {
		ter = create_territory_entry(emp, room);
	}
	
	qt_empire_players(emp, qt_gain_tile_sector, GET_SECT_VNUM(SECT(room)));
	et_gain_tile_sector(emp, GET_SECT_VNUM(SECT(room)));
	if (GET_BUILDING(room) && IS_COMPLETE(room)) {
		qt_empire_players(emp, qt_gain_building, GET_BLD_VNUM(GET_BUILDING(room)));
		et_gain_building(emp, GET_BLD_VNUM(GET_BUILDING(room)));
	}
	
	// claimed rooms are never unloadable anyway
	if (ROOM_UNLOAD_EVENT(room)) {
		dg_event_cancel(ROOM_UNLOAD_EVENT(room), cancel_room_event);
		ROOM_UNLOAD_EVENT(room) = NULL;
	}
	
	// update any building-flagged vehicles
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (!VEH_OWNER(veh) && VEH_CLAIMS_WITH_ROOM(veh)) {
			perform_claim_vehicle(veh, emp);
		}
	}
	
	add_dropped_item_list(emp, ROOM_CONTENTS(room));
	update_MSDP_empire_data_all(emp, TRUE, TRUE);
	request_mapout_update(GET_ROOM_VNUM(room));
	request_world_save(GET_ROOM_VNUM(room), WSAVE_MAP | WSAVE_ROOM);
}


/**
* Sets ownership of a vehicle.
*
* @param vehicle_data *veh The vehicle to claim.
* @param empire_data *emp The empire claiming it.
*/
void perform_claim_vehicle(vehicle_data *veh, empire_data *emp) {
	bool provided_light;
	
	if (VEH_OWNER(veh)) {
		perform_abandon_vehicle(veh);
	}
	
	// check current lights AFTER abandoning
	provided_light = VEH_PROVIDES_LIGHT(veh);
	
	if (emp) {
		VEH_OWNER(veh) = emp;
		VEH_SHIPPING_ID(veh) = -1;
	
		if (VEH_INTERIOR_HOME_ROOM(veh)) {
			if (ROOM_OWNER(VEH_INTERIOR_HOME_ROOM(veh)) && ROOM_OWNER(VEH_INTERIOR_HOME_ROOM(veh)) != emp) {
				abandon_room(VEH_INTERIOR_HOME_ROOM(veh));
			}
			claim_room(VEH_INTERIOR_HOME_ROOM(veh), emp);
		}
		
		adjust_vehicle_tech(veh, TRUE);
		if (VEH_IS_COMPLETE(veh)) {
			qt_empire_players_vehicle(emp, qt_gain_vehicle, veh);
			et_gain_vehicle(emp, veh);
		}
		
		add_dropped_item_list(emp, VEH_CONTAINS(veh));
		
		// check if light changed
		if (IN_ROOM(veh)) {
			if (VEH_PROVIDES_LIGHT(veh) && !provided_light) {
				++ROOM_LIGHTS(IN_ROOM(veh));
			}
			else if (!VEH_PROVIDES_LIGHT(veh) && provided_light) {
				--ROOM_LIGHTS(IN_ROOM(veh));
			}
		}
		
		request_vehicle_save_in_world(veh);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE DROPPED ITEM HANDLERS ////////////////////////////////////////////

/**
* Adds 1 to the dropped-item count for the empire. Cascades to contained objs.
*
* @param empire_data *emp The empire.
* @param obj_data *obj The dropped item.
*/
void add_dropped_item(empire_data *emp, obj_data *obj) {
	struct empire_dropped_item *edi;
	obj_vnum vnum = GET_OBJ_VNUM(obj);
	HASH_FIND_INT(EMPIRE_DROPPED_ITEMS(emp), &vnum, edi);
	if (!edi) {
		CREATE(edi, struct empire_dropped_item, 1);
		edi->vnum = vnum;
		HASH_ADD_INT(EMPIRE_DROPPED_ITEMS(emp), vnum, edi);
	}
	++edi->count;
	
	if (obj->contains) {
		add_dropped_item_list(emp, obj->contains);
	}
}


/**
* For objects that might be nested inside something, finds the top owner and
* adds to the dropped-items count.
*
* @param obj_data *obj The object.
* @param empire_data *only_emp Optional: Only update 1 empire. (NULL for all)
*/
void add_dropped_item_anywhere(obj_data *obj, empire_data *only_if_emp) {
	obj_data *top = obj;
	
	// un-nest
	while (top->in_obj) {
		top = top->in_obj;
	}
	
	if (top->in_vehicle && VEH_OWNER(top->in_vehicle) && (!only_if_emp || VEH_OWNER(top->in_vehicle) == only_if_emp)) {
		add_dropped_item(VEH_OWNER(top->in_vehicle), obj);
	}
	else if (IN_ROOM(top) && ROOM_OWNER(IN_ROOM(top)) && (!only_if_emp || ROOM_OWNER(IN_ROOM(top)) == only_if_emp)) {
		add_dropped_item(ROOM_OWNER(IN_ROOM(top)), obj);
	}
}


/**
* Adds a whole list of dropped-items to the empire, e.g. when a room or
* vehicle is claimed.
*
* @param empire_data *emp The empire.
* @param obj_data *list The content list (ROOM_CONTENTS, VEH_CONTAINS, etc).
*/
void add_dropped_item_list(empire_data *emp, obj_data *list) {
	obj_data *obj;
	if (emp) {
		DL_FOREACH2(list, obj, next_content) {
			add_dropped_item(emp, obj);
		}
	}
}


/**
* Gets the number of dropped items with a given vnum in the empire.
*
* @param empire_data *emp The empire.
* @param obj_vnum vnum The item to get a count for.
* @return int The number of that item dropped around the empire.
*/
int count_dropped_items(empire_data *emp, obj_vnum vnum) {
	struct empire_dropped_item *edi;
	HASH_FIND_INT(EMPIRE_DROPPED_ITEMS(emp), &vnum, edi);
	return edi ? edi->count : 0;
}


/**
* Frees a set of empire-dropped-items.
*
* @param struct empire_dropped_item **list A pointer to the hash to free.
*/
void free_dropped_items(struct empire_dropped_item **list) {
	struct empire_dropped_item *iter, *next;
	if (list) {
		HASH_ITER(hh, *list, iter, next) {
			HASH_DEL(*list, iter);
			free(iter);
		}
	}
}


/**
* Refreshes the EMPIRE_DROPPED_ITEMS() counts for 1 (or all) empire(s).
*
* @param empire_data *only_emp Optional: Only update 1 empire. (NULL for all)
*/
void refresh_empire_dropped_items(empire_data *only_emp) {
	empire_data *emp, *next_emp;
	obj_data *obj;
	
	if (only_emp) {
		free_dropped_items(&EMPIRE_DROPPED_ITEMS(only_emp));
	}
	else {
		HASH_ITER(hh, empire_table, emp, next_emp) {
			free_dropped_items(&EMPIRE_DROPPED_ITEMS(emp));
		}
	}
	
	DL_FOREACH(object_list, obj) {
		if (obj->in_vehicle && VEH_OWNER(obj->in_vehicle) && (!only_emp || VEH_OWNER(obj->in_vehicle) == only_emp)) {
			add_dropped_item(VEH_OWNER(obj->in_vehicle), obj);
		}
		else if (IN_ROOM(obj) && ROOM_OWNER(IN_ROOM(obj)) && (!only_emp || ROOM_OWNER(IN_ROOM(obj)) == only_emp)) {
			add_dropped_item(ROOM_OWNER(IN_ROOM(obj)), obj);
		}
		// only items directly in these places are added; everything else is hit by the cascade
	}
}


/**
* Removes 1 from the dropped-item count for the empire. Cascades to contained
* items.
*
* @param empire_data *emp The empire.
* @param obj_data *obj The dropped item.
*/
void remove_dropped_item(empire_data *emp, obj_data *obj) {
	struct empire_dropped_item *edi;
	obj_vnum vnum = GET_OBJ_VNUM(obj);
	HASH_FIND_INT(EMPIRE_DROPPED_ITEMS(emp), &vnum, edi);
	if (edi) {
		--edi->count;
		if (edi->count <= 0) {
			HASH_DEL(EMPIRE_DROPPED_ITEMS(emp), edi);
			free(edi);
		}
	}
	if (obj->contains) {
		remove_dropped_item_list(emp, obj->contains);
	}
}


/**
* For objects that might be nested inside something, finds the top owner and
* removes from the dropped-items count.
*
* @param obj_data *obj The object.
*/
void remove_dropped_item_anywhere(obj_data *obj) {
	obj_data *top = obj;
	
	// un-nest
	while (top->in_obj) {
		top = top->in_obj;
	}
	
	if (top->in_vehicle && VEH_OWNER(top->in_vehicle)) {
		remove_dropped_item(VEH_OWNER(top->in_vehicle), obj);
	}
	else if (IN_ROOM(top) && ROOM_OWNER(IN_ROOM(top))) {
		remove_dropped_item(ROOM_OWNER(IN_ROOM(top)), obj);
	}
}


/**
* Removes a whole list of dropped-items from the empire, e.g. when a room or
* vehicle is abandoned.
*
* @param empire_data *emp The empire.
* @param obj_data *list The content list (ROOM_CONTENTS, VEH_CONTAINS, etc).
*/
void remove_dropped_item_list(empire_data *emp, obj_data *list) {
	obj_data *obj;
	if (emp) {
		DL_FOREACH2(list, obj, next_content) {
			remove_dropped_item(emp, obj);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE PRODUCTION TOTAL HANDLERS ////////////////////////////////////////

/**
* Marks that the empire has gathered (or produced, or traded for) an item.
* If you are adding as the result of an import, you should call
* mark_production_trade too.
*
* @param empire_data *emp Which empire.
* @param obj_vnum vnum The item gained.
* @param int amount How many were gained.
*/
void add_production_total(empire_data *emp, obj_vnum vnum, int amount) {
	struct empire_production_total *egt;
	
	if (!emp || vnum == NOTHING) {
		return;	// no work
	}
	
	if ((egt = get_production_total_entry(emp, vnum))) {
		SAFE_ADD(egt->amount, amount, 0, INT_MAX, FALSE);
		EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	
		// update trackers
		et_change_production_total(emp, vnum, amount);
	}
}


/**
* Marks an item as produced for everyone on a mob's tag list, ignoring anybody
* whose empire has already been done (adds at most 1 per empire).
*
* @param struct mob_tag *list The list of mob tags.
* @param obj_vnum vnum The item gained.
* @param int amount How many were gained.
*/
void add_production_total_for_tag_list(struct mob_tag *list, obj_vnum vnum, int amount) {
	struct mob_tag *tag;
	char_data *plr;
	bool done;
	
	// mini data type for preventing duplicates
	struct agtftl_data {
		any_vnum eid;
		struct agtftl_data *next;
	} *dat, *next_dat, *dat_list = NULL;
	
	if (vnum == NOTHING || !list) {
		return;	// sanity
	}
	
	for (tag = list; tag; tag = tag->next) {
		if (!(plr = is_playing(tag->idnum))) {
			continue;	// ignore missing players
		}
		if (!GET_LOYALTY(plr)) {
			continue;	// not in an empire
		}
		
		// check if already done this empire
		done = FALSE;
		LL_FOREACH(dat_list, dat) {
			if (dat->eid == EMPIRE_VNUM(GET_LOYALTY(plr))) {
				done = TRUE;
				break;
			}
		}
		if (done) {
			continue;	// we only do each empire once
		}
		
		// OK: add it
		add_production_total(GET_LOYALTY(plr), vnum, amount);
		
		// and mark the empire
		CREATE(dat, struct agtftl_data, 1);
		dat->eid = EMPIRE_VNUM(GET_LOYALTY(plr));
		LL_PREPEND(dat_list, dat);
	}
	
	// free the data list
	LL_FOREACH_SAFE(dat_list, dat, next_dat) {
		free(dat);
	}
}


/**
* Gets the total count produced (or traded for) by an empire.
*
* @param empire_data *emp Which empire.
* @param obj_vnum vnum The item to check
* @return int How many the empire has ever gained.
*/
int get_production_total(empire_data *emp, obj_vnum vnum) {
	struct empire_production_total *egt;
	int amt;
	
	if (!emp || vnum == NOTHING) {
		return 0;	// no work
	}
	
	HASH_FIND_INT(EMPIRE_PRODUCTION_TOTALS(emp), &vnum, egt);
	if (egt) {
		amt = egt->amount;
		
		// ignores any that may have been exported then imported, to prevent
		// abuse where 2 empires could constantly import/export from each other
		if (egt->imported > 0 && egt->exported > 0) {
			amt -= MIN(egt->exported, egt->imported);
		}
		
		return amt;
	}
	else {
		return 0;
	}
}


/**
* Gets the total count produced (or traded for) by an empire, for a component.
*
* @param empire_data *emp Which empire.
* @param any_vnum cmp_vnum The generic component type to check for (counts other versions of it too).
* @return int How many matching items the empire has ever gained.
*/
int get_production_total_component(empire_data *emp, any_vnum cmp_vnum) {
	struct empire_production_total *egt, *next_egt;
	generic_data *cmp = real_generic(cmp_vnum);
	int count = 0;
	
	if (!emp || !cmp) {
		return count;	// no work
	}
	
	HASH_ITER(hh, EMPIRE_PRODUCTION_TOTALS(emp), egt, next_egt) {
		if (!egt->proto || GET_OBJ_COMPONENT(egt->proto) == NOTHING) {
			continue;
		}
		
		if (is_component(egt->proto, cmp)) {
			SAFE_ADD(count, egt->amount, 0, INT_MAX, FALSE);
			
			if (egt->imported > 0 && egt->exported > 0) {
				// no need to safe-add as this will always leave count >= 0
				count -= MIN(egt->imported, egt->exported);
			}
		}
	}
	
	return count;
}


/**
* Finds (or creates) an empire_production_total entry, if possible.
*
* @param empire_data *emp The empire.
* @param any_vnum vnum Which object vnum (must correspond to an object).
* @return struct empire_production_total* The entry for that object, or NULL if not possible.
*/
struct empire_production_total *get_production_total_entry(empire_data *emp, any_vnum vnum) {
	struct empire_production_total *egt;
	obj_data *proto;
	
	if (!emp || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(EMPIRE_PRODUCTION_TOTALS(emp), &vnum, egt);
	if (!egt) {
		// ensure real obj now
		if (!(proto = obj_proto(vnum))) {
			return NULL;
		}
		
		CREATE(egt, struct empire_production_total, 1);
		egt->vnum = vnum;
		egt->proto = proto;
		HASH_ADD_INT(EMPIRE_PRODUCTION_TOTALS(emp), vnum, egt);
	}
	return egt;
}


/**
* Marks that an empire has imported/exported a resource, which will affect how
* totals are determined. Note that this only marks an amount as imported or
* exported; it does not add to the production amount (imports should do this
* separately).
*
* @param empire_data *emp Which empire.
* @param obj_vnum vnum The item gained.
* @param int imported The amount that were imported (0 or more).
* @param int exported The amount that were exported (0 or more).
*/
void mark_production_trade(empire_data *emp, obj_vnum vnum, int imported, int exported) {
	struct empire_production_total *egt;
	
	if (!emp || vnum == NOTHING) {
		return;	// no work
	}
	
	if ((egt = get_production_total_entry(emp, vnum))) {
		if (imported) {
			SAFE_ADD(egt->imported, imported, 0, INT_MAX, FALSE);
		}
		if (exported) {
			SAFE_ADD(egt->exported, exported, 0, INT_MAX, FALSE);
		}
		EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	}
}


// Simple amount-sorter for production item totals
int sort_empire_production_totals(struct empire_production_total *a, struct empire_production_total *b) {
	return b->amount - a->amount;
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE NEEDS HANDLERS ///////////////////////////////////////////////////

/**
* Marks needed items of a given type, on the island.
*
* @param empire_data *emp The empire to mark needs for.
* @param int island Which island (id) to mark needs for in that empire.
* @param int Which ENEED_ const type.
* @param int The amount needed.
*/
void add_empire_needs(empire_data *emp, int island, int type, int amount) {
	struct empire_needs *need = get_empire_needs(emp, island, type);
	if (need) {
		SAFE_ADD(need->needed, amount, 0, INT_MAX, FALSE);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}


/**
* Gets the 'needs' entry by type, for an empire+island combo. It will create
* the entry if _needed_.
*
* @param empire_data *emp The empire to fetch needs for.
* @param int island Which island (id) to fetch needs for in that empire.
* @param int Which ENEED_ const to fetch data for.
* @return struct empire_needs* The needs data.
*/
struct empire_needs *get_empire_needs(empire_data *emp, int island, int type) {
	struct empire_island *isle;
	struct empire_needs *need;
	
	if (!emp || !(isle = get_empire_island(emp, island))) {
		log("SYSERR: get_empire_needs called without valid %s", emp ? "island" : "empire");
		return NULL;	// somehow
	}
	
	HASH_FIND_INT(isle->needs, &type, need);
	if (!need) {
		CREATE(need, struct empire_needs, 1);
		need->type = type;
		HASH_ADD_INT(isle->needs, type, need);
	}
	
	return need;
}


/**
* Determines if a given island has a certain status on its 'needs' for the
* empire.
* 
* @param empire_data *emp The empire to check needs for.
* @param int island Which island (id) to check needs for in that empire.
* @param int Which ENEED_ const type.
* @param bitvector_t status Which ENEED_STATUS_ flags to check for.
* @return bool TRUE if all those flags are marked on the needs, FALSE if not.
*/
bool empire_has_needs_status(empire_data *emp, int island, int type, bitvector_t status) {
	struct empire_needs *need = get_empire_needs(emp, island, type);
	return need ? ((need->status & status) == status) : FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE TARGETING HANDLERS ///////////////////////////////////////////////

/**
* @param empire_data *emp Which empire to check for cities in
* @param room_data *loc Location to check
* @return struct empire_city_data* Returns the closest city that loc is inside, or NULL if none
*/
struct empire_city_data *find_city(empire_data *emp, room_data *loc) {
	struct empire_city_data *city, *found = NULL;
	int dist, min = -1;

	if (!emp) {
		return NULL;
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
* @param char *raw_name The user input (name/number of empire)
* @return empire_data *The empire, or NULL if none found.
*/
empire_data *get_empire_by_name(char *raw_name) {
	empire_data *pos, *next_pos, *full_exact, *full_abbrev, *adj_exact, *adj_abbrev;
	char name[MAX_INPUT_LENGTH];
	int num;
	
	// we'll take any of these if we don't find a perfect match
	full_exact = full_abbrev = adj_exact = adj_abbrev = NULL;
	
	if (*raw_name == '"') {	// strip quotes if any
		any_one_word(raw_name, name);
	}
	else {
		strcpy(name, raw_name);
	}

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

	if (GET_LEADER(ch) || ch == leader)
		return;

	GET_LEADER(ch) = leader;

	CREATE(k, struct follow_type, 1);

	k->follower = ch;
	LL_PREPEND(leader->followers, k);

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

	if (GET_LEADER(ch)) {
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
	struct follow_type *fol, *next_fol;

	if (GET_LEADER(ch) == NULL)
		return;
	
	// only message if neither was just extracted
	if (!EXTRACTED(ch) && !EXTRACTED(GET_LEADER(ch))) {
		act("You stop following $N.", FALSE, ch, 0, GET_LEADER(ch), TO_CHAR);
		act("$n stops following $N.", TRUE, ch, 0, GET_LEADER(ch), TO_NOTVICT);
		if (CAN_SEE(GET_LEADER(ch), ch) && WIZHIDE_OK(GET_LEADER(ch), ch)) {
			act("$n stops following you.", TRUE, ch, 0, GET_LEADER(ch), TO_VICT);
		}
	}
	
	// delete from list
	LL_FOREACH_SAFE(GET_LEADER(ch)->followers, fol, next_fol) {
		if (fol->follower == ch) {
			LL_DELETE(GET_LEADER(ch)->followers, fol);
			free(fol);
		}
	}

	GET_LEADER(ch) = NULL;
	REMOVE_BIT(AFF_FLAGS(ch), AFF_CHARM);
}


 //////////////////////////////////////////////////////////////////////////////
//// GLOBAL HANDLERS /////////////////////////////////////////////////////////

/**
* This is the main function for running a set of globals.
*
* If you need to pass additional data through to the validator, put it into a
* pointer and pass it as 'other_data' (see: struct glb_emp_bean).
*
* For optional parameters, pass NULL or 0 to omit that parameter.
*
* @param int glb_type The GLOBAL_ type to run.
* @param GLB_FUNCTION(*func) The function to be called on any successful global(s).
* @param bool allow_many If TRUE, multiple globals can run. If FALSE, stops at the first successful one.
* @param bitvector_t type_flags The flag set to compare against the global's type-flags and type-exclude.
* @param char_data *ch Optional: The person running it (for level/ability requirements).
* @param adv_data *adv Optional: The adventure it's running in, if any (GLB_FLAG_ADVENTURE_ONLY).
* @param int level Optional: For level-constrained globals.
* @param GLB_VALIDATOR(*validator) Optional: A function for additional validation.
* @param void *other_data Optional: Additional data that can be passed through to the validator and to the final run function. This data may be cast to whatever type you need it to be.
* @return bool TRUE if any globals ran; FALSE if not.
*/
bool run_globals(int glb_type, GLB_FUNCTION(*func), bool allow_many, bitvector_t type_flags, char_data *ch, adv_data *adv, int level, GLB_VALIDATOR(*validator), void *other_data) {
	struct global_data *glb, *next_glb, *choose_last;
	bool done_cumulative = FALSE, found = FALSE;
	int cumulative_prc;
	
	cumulative_prc = number(1, 10000);
	choose_last = NULL;
	
	HASH_ITER(hh, globals_table, glb, next_glb) {
		if (GET_GLOBAL_TYPE(glb) != glb_type) {
			continue;
		}
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT)) {
			continue;
		}
		if (GET_GLOBAL_ABILITY(glb) != NO_ABIL && (!ch || !has_ability(ch, GET_GLOBAL_ABILITY(glb)))) {
			continue;
		}
		
		// level limits
		if (GET_GLOBAL_MIN_LEVEL(glb) > 0 && level < GET_GLOBAL_MIN_LEVEL(glb)) {
			continue;
		}
		if (GET_GLOBAL_MAX_LEVEL(glb) > 0 && level > GET_GLOBAL_MAX_LEVEL(glb)) {
			continue;
		}
		
		// match ALL type-flags
		if ((type_flags & GET_GLOBAL_TYPE_FLAGS(glb)) != GET_GLOBAL_TYPE_FLAGS(glb)) {
			continue;
		}
		// match ZERO type-excludes
		if ((type_flags & GET_GLOBAL_TYPE_EXCLUDE(glb)) != 0) {
			continue;
		}
		
		// check adventure-only -- late-matching because it does more work than other conditions
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_ADVENTURE_ONLY) && (!adv || get_adventure_for_vnum(GET_GLOBAL_VNUM(glb)) != adv)) {
			continue;
		}
		
		// now the user-specified validator
		if (validator && !validator(glb, ch, other_data)) {
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
		else {	// not choose-last
			if (func) {
				found |= func(glb, ch, other_data);
			}
			if (!allow_many) {
				break;	// only use first match
			}
			// otherwise, continue with execution
		}
	}
	
	// failover/choose-last
	if (!found && choose_last && func) {
		found |= func(choose_last, ch, other_data);
	}
	
	return found;
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
	struct group_data *new_group;

	CREATE(new_group, struct group_data, 1);
	LL_APPEND(group_list, new_group);
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
	
	// short remove-from-group
	while ((mem = group->members)) {
		group->members = mem->next;
		mem->member->group = NULL;
		free(mem);
	}

	LL_DELETE(group_list, group);
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
	use_ch = IS_NPC(ch) ? GET_LEADER(ch) : ch;
	use_vict = IS_NPC(vict) ? GET_LEADER(vict) : vict;

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
	struct group_member_data *mem;
	
	CREATE(mem, struct group_member_data, 1);
	mem->member = ch;
	LL_APPEND(group->members, mem);

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
	struct group_member_data *mem, *next_mem;
	char_data *first_pc = NULL;
	struct group_data *group;

	if ((group = ch->group) == NULL)
		return;

	send_to_group(NULL, group, "%s has left the group.", GET_NAME(ch));
	
	// remove from member list
	for (mem = group->members; mem; mem = next_mem) {
		next_mem = mem->next;
		if (mem->member == ch) {
			LL_DELETE(group->members, mem);
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
* Determines if an interaction is available in the room, from any source.
*
* @param room_data *room The location.
* @param int type Any INTERACT_ const.
* @return bool TRUE if you can, FALSE if not.
*/ 
bool can_interact_room(room_data *room, int type) {
	vehicle_data *veh;
	
	if (SECT_CAN_INTERACT_ROOM(room, type) || BLD_CAN_INTERACT_ROOM(room, type) || RMT_CAN_INTERACT_ROOM(room, type) || CROP_CAN_INTERACT_ROOM(room, type)) {
		return TRUE;	// simple
	}
	
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (VEH_IS_COMPLETE(veh) && VEH_HEALTH(veh) > 0 && has_interaction(VEH_INTERACTIONS(veh), type)) {
			return TRUE;
		}
	}
	
	// reached end
	return FALSE;
}


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
* Gets the highest available depletion level amongst matching interactions in
* the list. This mainly returns the highest 'quantity' from a
* interact_one_at_a_time[] interaction, or else common_depletion.
*
* @param char_data *ch Optional: The actor, to determine interaction restrictions. (may be NULL)
* @param empire_data *emp Optional: The empire, to determine interaction restrictions. (may be NULL)
* @param struct interaction_item *list The list of interactions to check.
* @param int interaction_type Any type, but interact_one_at_a_time types are the main purpose here.
* @return int The depletion cap.
*/
int get_interaction_depletion(char_data *ch, empire_data *emp, struct interaction_item *list, int interaction_type, bool require_storable) {
	struct interaction_item *interact;
	obj_data *proto;
	int highest = 0;
	
	if (!interact_one_at_a_time[interaction_type]) {
		return config_get_int("common_depletion");
	}
	
	// for one-at-a-time chores, look for the highest depletion
	LL_FOREACH(list, interact) {
		if (interact->type != interaction_type) {
			continue;
		}
		if (require_storable && (!(proto = obj_proto(interact->vnum)) || !GET_OBJ_STORAGE(proto))) {
			continue;	// MUST be storable
		}
		if (!meets_interaction_restrictions(interact->restrictions, ch, emp, NULL, NULL)) {
			continue;
		}
		
		// found valid interaction
		if (interact->quantity > highest) {
			highest = interact->quantity;
		}
	}
	
	return highest;
}


/**
* Checks all the room interactions (crop etc) to find the highest depletion.
* 
*
* @param char_data *ch Optional: The actor, to determine interaction restrictions. (may be NULL)
* @param empire_data *emp Optional: The empire, to determine interaction restrictions. (may be NULL)
* @param room_data *room The room whose sector/crop/building to check for interaction caps.
* @param int interaction_type Any type, but interact_one_at_a_time types are the main purpose here.
* @return int The depletion cap.
*/
int get_interaction_depletion_room(char_data *ch, empire_data *emp, room_data *room, int interaction_type, bool require_storable) {
	crop_data *cp;
	int this, highest = 0;
	
	if (!interact_one_at_a_time[interaction_type]) {
		return config_get_int("common_depletion");	// shortcut
	}
	
	highest = get_interaction_depletion(ch, emp, GET_SECT_INTERACTIONS(SECT(room)), interaction_type, require_storable);
	if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && (cp = ROOM_CROP(room))) {
		this = get_interaction_depletion(ch, emp, GET_CROP_INTERACTIONS(cp), interaction_type, require_storable);
		highest = MAX(highest, this);
	}
	if (GET_BUILDING(room) && IS_COMPLETE(room)) {
		this = get_interaction_depletion(ch, emp, GET_BLD_INTERACTIONS(GET_BUILDING(room)), interaction_type, require_storable);
		highest = MAX(highest, this);
	}
	
	return highest;
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
* Validates a set of restrictions on an interaction.
*
* @param struct interact_restriction *list The list of restrictions to check.
* @param char_data *ch Optional: The person trying to interact (for ability/ptech/local tech; may be NULL).
* @param empire_data *emp Optional: The empire trying to interact (for techs; may be NULL).
* @param char_data *inter_mob The mob who has the interactions, if any.
* @param obj_data *inter_item The object that has the interactions, if any.
* @return bool TRUE if okay, FALSE if failed.
*/
bool meets_interaction_restrictions(struct interact_restriction *list, char_data *ch, empire_data *emp, char_data *inter_mob, obj_data *inter_item) {
	struct interact_restriction *res;
	bool any_diff = FALSE, diff_ok = FALSE;
	
	LL_FOREACH(list, res) {
		// INTERACT_RESTRICT_x
		switch (res->type) {
			case INTERACT_RESTRICT_ABILITY: {
				if (!ch || IS_NPC(ch) || !has_ability(ch, res->vnum)) {
					return FALSE;
				}
				break;
			}
			case INTERACT_RESTRICT_BOSS: {
				any_diff = TRUE;
				if (inter_mob && MOB_FLAGGED(inter_mob, MOB_HARD) && MOB_FLAGGED(inter_mob, MOB_GROUP)) {
					diff_ok = TRUE;	// any matching diff is ok
				}
				if (inter_item && OBJ_FLAGGED(inter_item, OBJ_HARD_DROP) && OBJ_FLAGGED(inter_item, OBJ_GROUP_DROP)) {
					diff_ok = TRUE;
				}
				break;
			}
			case INTERACT_RESTRICT_GROUP: {
				any_diff = TRUE;
				if (inter_mob && !MOB_FLAGGED(inter_mob, MOB_HARD) && MOB_FLAGGED(inter_mob, MOB_GROUP)) {
					diff_ok = TRUE;	// any matching diff is ok
				}
				if (inter_item && !OBJ_FLAGGED(inter_item, OBJ_HARD_DROP) && OBJ_FLAGGED(inter_item, OBJ_GROUP_DROP)) {
					diff_ok = TRUE;
				}
				break;
			}
			case INTERACT_RESTRICT_HARD: {
				any_diff = TRUE;
				if (inter_mob && MOB_FLAGGED(inter_mob, MOB_HARD) && !MOB_FLAGGED(inter_mob, MOB_GROUP)) {
					diff_ok = TRUE;	// any matching diff is ok
				}
				if (inter_item && OBJ_FLAGGED(inter_item, OBJ_HARD_DROP) && !OBJ_FLAGGED(inter_item, OBJ_GROUP_DROP)) {
					diff_ok = TRUE;
				}
				break;
			}
			case INTERACT_RESTRICT_NORMAL: {
				any_diff = TRUE;
				if (inter_mob && !MOB_FLAGGED(inter_mob, MOB_HARD | MOB_GROUP)) {
					diff_ok = TRUE;	// any matching diff is ok
				}
				if (inter_item && !OBJ_FLAGGED(inter_item, OBJ_HARD_DROP | OBJ_GROUP_DROP)) {
					diff_ok = TRUE;
				}
				break;
			}
			case INTERACT_RESTRICT_PTECH: {
				if (!ch || IS_NPC(ch) || !has_player_tech(ch, res->vnum)) {
					return FALSE;
				}
				break;
			}
			case INTERACT_RESTRICT_TECH: {
				if (!(ch && has_tech_available(ch, res->vnum)) && !(emp && EMPIRE_HAS_TECH(emp, res->vnum))) {
					return FALSE;
				}
				break;
			}
			// no default: restriction does not work
		}
	}
	
	if (any_diff) {
		// if any difficulty was requested, we require diff_ok
		return diff_ok;
	}
	// else:
	return TRUE;	// made it this far
}



GLB_FUNCTION(run_global_mob_interactions_func) {
	struct glb_mob_interact_bean *data = (struct glb_mob_interact_bean*)other_data;
	return run_interactions(ch, GET_GLOBAL_INTERACTIONS(glb), data->type, IN_ROOM(ch), data->mob, NULL, NULL, data->func);
}


GLB_FUNCTION(run_global_obj_interactions_func) {
	struct glb_obj_interact_bean *data = (struct glb_obj_interact_bean*)other_data;
	return run_interactions(ch, GET_GLOBAL_INTERACTIONS(glb), data->type, IN_ROOM(ch), NULL, data->obj, NULL, data->func);
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
	struct glb_mob_interact_bean *data;
	struct instance_data *inst;
	bool any = FALSE;
	
	// no work
	if (!ch || !mob || !IS_NPC(mob) || !func) {
		return FALSE;
	}
	
	inst = real_instance(MOB_INSTANCE_ID(mob));
	
	CREATE(data, struct glb_mob_interact_bean, 1);
	data->mob = mob;
	data->type = type;
	data->func = func;
	any = run_globals(GLOBAL_MOB_INTERACTIONS, run_global_mob_interactions_func, TRUE, MOB_FLAGS(mob), ch, (inst ? INST_ADVENTURE(inst) : NULL), GET_CURRENT_SCALE_LEVEL(mob), NULL, data);
	free(data);
	
	return any;
}


/**
* Attempts to run global obj interactions -- interactions from the globals table.
*
* @param char_data *ch The player who is interacting.
* @param char_data *obj The object being interacted-with.
* @param int type Any INTERACT_ const.
* @param INTERACTION_FUNC(*func) A callback function to run for the interaction.
*/
bool run_global_obj_interactions(char_data *ch, obj_data *obj, int type, INTERACTION_FUNC(*func)) {
	struct glb_obj_interact_bean *data;
	bool any = FALSE;
	
	// no work
	if (!ch || !obj || !func) {
		return FALSE;
	}
	
	CREATE(data, struct glb_obj_interact_bean, 1);
	data->obj = obj;
	data->type = type;
	data->func = func;
	any = run_globals(GLOBAL_OBJ_INTERACTIONS, run_global_obj_interactions_func, TRUE, GET_OBJ_EXTRA(obj), ch, get_adventure_for_vnum(GET_OBJ_VNUM(obj)), GET_OBJ_CURRENT_SCALE_LEVEL(obj), NULL, data);
	free(data);
	
	return any;
}


/**
* Runs a set of interactions and passes successful ones through to a function
* pointer.
*
* @param char_data *ch Optional: The actor.
* @param struct interaction_item *run_list A pointer to the start of the list to run.
* @param int type Any INTERACT_ const.
* @param room_data *inter_room For room interactions, the room.
* @param char_data *inter_mob For mob interactions, the mob.
* @param obj_data *inter_item For item interactions, the item.
* @param vehicle_data *inter_veh For vehicle interactions, the vehicle.
* @param INTERACTION_FUNC(*func) A function pointer that runs each successful interaction (func returns TRUE if an interaction happens; FALSE if it aborts)
* @return bool TRUE if any interactions ran successfully, FALSE if not.
*/
bool run_interactions(char_data *ch, struct interaction_item *run_list, int type, room_data *inter_room, char_data *inter_mob, obj_data *inter_item, vehicle_data *inter_veh, INTERACTION_FUNC(*func)) {
	struct interact_exclusion_data *exclusion = NULL;
	struct interaction_item *interact;
	struct interact_restriction *res;
	bool success = FALSE;

	for (interact = run_list; interact; interact = interact->next) {
		if (interact->type == type && meets_interaction_restrictions(interact->restrictions, ch, ch ? GET_LOYALTY(ch) : NULL, inter_mob, inter_item) && check_exclusion_set(&exclusion, interact->exclusion_code, interact->percent)) {
			if (func) {
				// run function
				success |= (func)(ch, interact, inter_room, inter_mob, inter_item, inter_veh);
				
				// skill gains?
				if (ch && !IS_NPC(ch)) {
					LL_FOREACH(interact->restrictions, res) {
						switch (res->type) {
							case INTERACT_RESTRICT_ABILITY: {
								gain_ability_exp(ch, res->vnum, 5);
								run_ability_hooks(ch, AHOOK_ABILITY, res->vnum, 0, inter_mob, inter_item, inter_veh, inter_room, NOBITS);
								break;
							}
							case INTERACT_RESTRICT_PTECH: {
								gain_player_tech_exp(ch, res->vnum, 5);
								run_ability_hooks_by_player_tech(ch, res->vnum, inter_mob, inter_item, inter_veh, inter_room);
								break;
							}
						}
					}
				}
			}
		}
	}
	
	free_exclusion_data(exclusion);
	return success;
}


/**
* Runs all interactions for a room (sect, crop, building, vehicle; as
* applicable). They run from most-specific to least: veh -> bdg -> crop -> sect
*
* @param char_data *ch The actor.
* @param room_data *room The location to run on.
* @param int type Any INTERACT_ const.
* @param vehicle_data *inter_veh Optional: Will pass this vehicle to any interaction func, and won't call other vehicles' interactions if set. (Pass NULL if not applicable.)
* @param int access_type GUESTS_ALLOWED, MEMBERS_ONLY, etc; use NOTHING to skip this
* @param INTERACTION_FUNC(*func) A function pointer that runs each successful interaction (func returns TRUE if an interaction happens; FALSE if it aborts)
* @return bool TRUE if any interactions ran successfully, FALSE if not.
*/
bool run_room_interactions(char_data *ch, room_data *room, int type, vehicle_data *inter_veh, int access_type, INTERACTION_FUNC(*func)) {
	vehicle_data *veh;
	crop_data *crop;
	bool success;
	int num;
	
	struct temp_veh_helper {
		vehicle_data *veh;
		struct temp_veh_helper *next, *prev;
	} *list = NULL, *tvh, *next_tvh;
	
	success = FALSE;
	
	// first, build a list of vehicles that match this interaction type in the room
	num = 0;
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (inter_veh && veh != inter_veh) {
			continue;	// if they provided an inter_veh, skip other vehicles
		}
		if (!VEH_IS_COMPLETE(veh) || VEH_HEALTH(veh) < 1) {
			continue;	// not complete anyway
		}
		if (access_type != NOTHING && ch && !can_use_vehicle(ch, veh, access_type)) {
			continue;	// no permission
		}
		if (has_interaction(VEH_INTERACTIONS(veh), type)) {
			CREATE(tvh, struct temp_veh_helper, 1);
			tvh->veh = veh;
			
			// attempt a semi-random order (randomly goes first or else last)
			if (!number(0, num++) || !list) {
				DL_PREPEND(list, tvh);
			}
			else {
				DL_APPEND(list, tvh);
			}
		}
	}
	
	// now, try vehicles in random order...
	DL_FOREACH_SAFE(list, tvh, next_tvh) {
		if (!success) {
			success |= run_interactions(ch, VEH_INTERACTIONS(tvh->veh), type, room, NULL, NULL, tvh->veh, func);
		}
		DL_DELETE(list, tvh);
		free(tvh);	// clean up
	}
	
	// building first
	if (!success && GET_BUILDING(room) && IS_COMPLETE(room) && (access_type == NOTHING || !ch || can_use_room(ch, room, access_type))) {
		success |= run_interactions(ch, GET_BLD_INTERACTIONS(GET_BUILDING(room)), type, room, NULL, NULL, inter_veh, func);
	}
	
	// crop second
	if (!success && ROOM_SECT_FLAGGED(room, SECTF_CROP) && (crop = ROOM_CROP(room)) && (access_type == NOTHING || !ch || can_use_room(ch, room, access_type))) {
		success |= run_interactions(ch, GET_CROP_INTERACTIONS(crop), type, room, NULL, NULL, inter_veh, func);
	}
	
	// rmt third
	if (!success && GET_ROOM_TEMPLATE(room) && (access_type == NOTHING || !ch || can_use_room(ch, room, access_type))) {
		success |= run_interactions(ch, GET_RMT_INTERACTIONS(GET_ROOM_TEMPLATE(room)), type, room, NULL, NULL, inter_veh, func);
	}
	
	// sector fourth
	if (!success && (access_type == NOTHING || !ch || can_use_room(ch, room, access_type))) {
		success |= run_interactions(ch, GET_SECT_INTERACTIONS(SECT(room)), type, room, NULL, NULL, inter_veh, func);
	}
	
	return success;
}


 //////////////////////////////////////////////////////////////////////////////
//// LEARNED CRAFT HANDLERS //////////////////////////////////////////////////

// for learned/show learned
int sort_learned_recipes(struct player_craft_data *a, struct player_craft_data *b) {
	craft_data *acr = craft_proto(a->vnum), *bcr = craft_proto(b->vnum);
	
	if (!acr || !bcr) {
		return 0;
	}
	else if (GET_CRAFT_TYPE(acr) != GET_CRAFT_TYPE(bcr)) {
		return GET_CRAFT_TYPE(acr) - GET_CRAFT_TYPE(bcr);
	}
	else {
		return strcmp(GET_CRAFT_NAME(acr), GET_CRAFT_NAME(bcr));
	}
}


/**
* Adds a craft vnum to a player's learned list.
*
* @param char_data *ch The player.
* @param any_vnum vnum The craft vnum to learn.
*/
void add_learned_craft(char_data *ch, any_vnum vnum) {
	struct player_craft_data *pcd;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	HASH_FIND_INT(GET_LEARNED_CRAFTS(ch), &vnum, pcd);
	if (!pcd) {
		CREATE(pcd, struct player_craft_data, 1);
		pcd->vnum = vnum;
		HASH_ADD_INT(GET_LEARNED_CRAFTS(ch), vnum, pcd);
		HASH_SORT(GET_LEARNED_CRAFTS(ch), sort_learned_recipes);
	}
}


/**
* Adds a craft vnum to an empire's learned list -- this is stackable, so
* learning it more than once just adds to the count.
*
* @param empire_data *emp The empire.
* @param any_vnum vnum The craft vnum to learn.
*/
void add_learned_craft_empire(empire_data *emp, any_vnum vnum) {
	struct player_craft_data *pcd;
	
	HASH_FIND_INT(EMPIRE_LEARNED_CRAFTS(emp), &vnum, pcd);
	if (!pcd) {
		CREATE(pcd, struct player_craft_data, 1);
		pcd->vnum = vnum;
		pcd->count = 0;
		HASH_ADD_INT(EMPIRE_LEARNED_CRAFTS(emp), vnum, pcd);
		HASH_SORT(EMPIRE_LEARNED_CRAFTS(emp), sort_learned_recipes);
	}
	++pcd->count;
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
}


/**
* @param empire_data *emp The empire.
* @param any_vnum vnum The craft vnum to check.
* @return bool TRUE if the empire has learned it.
*/
bool empire_has_learned_craft(empire_data *emp, any_vnum vnum) {
	struct player_craft_data *pcd;
	HASH_FIND_INT(EMPIRE_LEARNED_CRAFTS(emp), &vnum, pcd);
	return pcd ? TRUE : FALSE;
}


/**
* @param char_data *ch The player.
* @param any_vnum vnum The craft vnum to check.
* @return bool TRUE if the player has learned it.
*/
bool has_learned_craft(char_data *ch, any_vnum vnum) {
	struct player_craft_data *pcd;
	
	if (IS_NPC(ch)) {
		return TRUE;
	}
	
	HASH_FIND_INT(GET_LEARNED_CRAFTS(ch), &vnum, pcd);
	
	if (pcd || (GET_LOYALTY(ch) && empire_has_learned_craft(GET_LOYALTY(ch), vnum))) {
		return TRUE;
	}
	return FALSE;
}


/**
* Removes a craft vnum from a player's learned list.
*
* @param char_data *ch The player.
* @param any_vnum vnum The craft vnum to forget.
*/
void remove_learned_craft(char_data *ch, any_vnum vnum) {
	struct player_craft_data *pcd;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	HASH_FIND_INT(GET_LEARNED_CRAFTS(ch), &vnum, pcd);
	if (pcd) {
		HASH_DEL(GET_LEARNED_CRAFTS(ch), pcd);
		free(pcd);
	}
}


/**
* Loses a craft in the learned list. If the craft was learned from more than
* 1 source, this only reduces the source count instead.
*
* @param empire_data *emp The empire.
* @param any_vnum vnum The craft vnum to forget.
* @param bool full_remove If TRUE, fully removes the entry. Otherwise decrements by 1 and removes if 0.
*/
void remove_learned_craft_empire(empire_data *emp, any_vnum vnum, bool full_remove) {
	struct player_craft_data *pcd;
	
	HASH_FIND_INT(EMPIRE_LEARNED_CRAFTS(emp), &vnum, pcd);
	if (pcd) {
		--pcd->count;
		if (pcd->count < 1 || full_remove) {
			HASH_DEL(EMPIRE_LEARNED_CRAFTS(emp), pcd);
			free(pcd);
		}
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
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
	struct lore_data *new;
	char text[MAX_STRING_LENGTH];
	va_list tArgList;

	if (!ch || IS_NPC(ch) || !str)
		return;
	
	// need the old lore, in case the player is offline
	check_delayed_load(ch);
	
	va_start(tArgList, str);
	vsprintf(text, str, tArgList);
	
	CREATE(new, struct lore_data, 1);
	new->type = type;
	new->date = (long) time(0);
	new->text = str_dup(text);
	LL_APPEND(GET_LORE(ch), new);
	
	va_end(tArgList);
}


/**
* Clean up old kill/death lore
*
* @param char_data *ch The person whose lore to clean.
*/
void clean_lore(char_data *ch) {
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
	if (!ch || IS_NPC(ch) || !lore)
		return;

	LL_DELETE(GET_LORE(ch), lore);
	if (lore->text) {
		free(lore->text);
	}
	free(lore);
}


 //////////////////////////////////////////////////////////////////////////////
//// MINIPET AND COMPANION HANDLERS //////////////////////////////////////////

// for minipets/show minipets
int sort_minipets(struct minipet_data *a, struct minipet_data *b) {
	char_data *acr = mob_proto(a->vnum), *bcr = mob_proto(b->vnum);
	
	if (!acr || !bcr) {
		return 0;
	}
	else {	// sort by 1st keyword
		return strcmp(GET_PC_NAME(acr), GET_PC_NAME(bcr));
	}
}


/**
* Adds a companion (mob) vnum to a player's list.
*
* @param char_data *ch The player.
* @param any_vnum vnum The mob vnum of the companion to gain.
* @param any_vnum from_abil If the companion comes from an ability, specify it here. Otherwise, use NO_ABIL.
* @return struct companion_data* The added entry.
*/
struct companion_data *add_companion(char_data *ch, any_vnum vnum, any_vnum from_abil) {
	struct companion_data *cd;
	
	if (IS_NPC(ch)) {
		return NULL;
	}
	
	HASH_FIND_INT(GET_COMPANIONS(ch), &vnum, cd);
	if (!cd) {
		CREATE(cd, struct companion_data, 1);
		cd->vnum = vnum;
		HASH_ADD_INT(GET_COMPANIONS(ch), vnum, cd);
	}
	cd->from_abil = from_abil;	// may be NO_ABIL
	
	return cd;
}


/**
* Stores a companion modification (new name, sex, etc).
*
* @param struct companion_data *companion The data for the modified companion.
* @param int type A CMOD_ const.
* @param int num The numeric value of the change (or 0 if not applicable).
* @param char *str The string value of the change (or NULL if not applicable). This string will be copied.
*/
void add_companion_mod(struct companion_data *companion, int type, int num, char *str) {
	struct companion_mod *mod;
	bool found = FALSE;
	
	LL_FOREACH(companion->mods, mod) {
		if (mod->type == type) {
			mod->num = num;
			if (mod->str) {
				free(mod->str);
			}
			mod->str = str ? str_dup(str) : NULL;
			found = TRUE;
		}
	}
	
	if (!found) {
		CREATE(mod, struct companion_mod, 1);
		mod->type = type;
		mod->num = num;
		mod->str = str ? str_dup(str) : NULL;
		LL_PREPEND(companion->mods, mod);
	}
}


/**
* If the mob is a companion, ensures that the var is saved.
*
* @param char_data *mob The possible companion.
* @param char *name The var name.
* @param char *value The var's value.
* @param int id The var's context/id.
*/
void add_companion_var(char_data *mob, char *name, char *value, int id) {
	struct companion_data *cd;
	
	if (!mob || !name || !value || !IS_NPC(mob) || !GET_COMPANION(mob) || !(cd = has_companion(GET_COMPANION(mob), GET_MOB_VNUM(mob)))) {
		return;	// no work
	}
	
	add_var(&(cd->vars), name, value, id);
	queue_delayed_update(GET_COMPANION(mob), CDU_SAVE);
}


/**
* Adds a minipet vnum to a player's list.
*
* @param char_data *ch The player.
* @param any_vnum vnum The minipet vnum to learn.
*/
void add_minipet(char_data *ch, any_vnum vnum) {
	struct minipet_data *mini;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	HASH_FIND_INT(GET_MINIPETS(ch), &vnum, mini);
	if (!mini) {
		CREATE(mini, struct minipet_data, 1);
		mini->vnum = vnum;
		HASH_ADD_INT(GET_MINIPETS(ch), vnum, mini);
		HASH_SORT(GET_MINIPETS(ch), sort_minipets);
	}
}


/**
* Checks that all a player's minipets are valid.
*
* @param char_data *ch The player to check.
*/
void check_minipets_and_companions(char_data *ch) {
	struct minipet_data *mini, *next_mini;
	struct companion_data *cd, *next_cd;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	HASH_ITER(hh, GET_COMPANIONS(ch), cd, next_cd) {
		if (!mob_proto(cd->vnum)) {
			remove_companion(ch, cd->vnum);
		}
	}

	HASH_ITER(hh, GET_MINIPETS(ch), mini, next_mini) {
		if (!mob_proto(mini->vnum)) {
			remove_minipet(ch, mini->vnum);
		}
	}
}


/**
* Frees the data for 1 player companion.
*
* @param struct companion_data *cd The companion data to free.
*/
void free_companion(struct companion_data *cd) {
	if (cd) {
		struct companion_mod *mod, *next_mods;
		
		free_proto_scripts(&cd->scripts);
		free_varlist(cd->vars);
		
		LL_FOREACH_SAFE(cd->mods, mod, next_mods) {
			if (mod->str) {
				free(mod->str);
			}
			free(mod);
		}
		
		free(cd);
	}
}


/**
* Find a companion modification in the list, by type.
*
* @param struct companion_data *cd The companion's data.
* @param int type The CMOD_ type to fetch.
* @return struct companion_mod* The matching data, if any (or NULL).
*/
struct companion_mod *get_companion_mod_by_type(struct companion_data *cd, int type) {
	struct companion_mod *mod;
	
	if (cd) {
		LL_FOREACH(cd->mods, mod) {
			if (mod->type == type) {
				return mod;
			}
		}
	}
	
	return NULL;	// if not found
}


/**
* @param char_data *ch The player.
* @param any_vnum vnum The companion vnum to check.
* @return struct companion_data* The companion entry, if the player has it AND has whatever ability it requires.
*/
struct companion_data *has_companion(char_data *ch, any_vnum vnum) {
	struct companion_data *cd;
	
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	HASH_FIND_INT(GET_COMPANIONS(ch), &vnum, cd);
	
	if (cd && (cd->from_abil == NO_ABIL || has_ability(ch, cd->from_abil))) {
		return cd;
	}
	return NULL;
}


/**
* @param char_data *ch The player.
* @param any_vnum vnum The minipet vnum to check.
* @return bool TRUE if the player has it.
*/
bool has_minipet(char_data *ch, any_vnum vnum) {
	struct minipet_data *mini;
	
	if (IS_NPC(ch)) {
		return TRUE;
	}
	
	HASH_FIND_INT(GET_MINIPETS(ch), &vnum, mini);
	
	if (mini) {
		return TRUE;
	}
	return FALSE;
}


/**
* Removes a companion vnum from a player's list. Normally you DON'T call this
* on ones granted by abilities when losing those abilities -- they will be
* hidden from the player but their data remains. However, if you DO remove a
* companion that comes from an ability, it will result in the player getting
* a fresh copy of it next time they summon it.
*
* @param char_data *ch The player.
* @param any_vnum vnum The companion vnum to lose.
*/
void remove_companion(char_data *ch, any_vnum vnum) {
	struct companion_data *cd;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	HASH_FIND_INT(GET_COMPANIONS(ch), &vnum, cd);
	if (cd) {
		HASH_DEL(GET_COMPANIONS(ch), cd);
		free_companion(cd);
	}
}


/**
* Deletes a companion modification, if present.
*
* @param struct companion_data **companion Pointer to the list of mods.
* @param int type A CMOD_ const to remove.
*/
void remove_companion_mod(struct companion_data **companion, int type) {
	struct companion_mod *mod, *next;
	
	if (!companion) {
		return;	// no work
	}
	
	LL_FOREACH_SAFE((*companion)->mods, mod, next) {
		if (mod->type == type) {
			LL_DELETE((*companion)->mods, mod);
			if (mod->str) {
				free(mod->str);
			}
			free(mod);
		}
	}
}


/**
* If the mob is a companion, ensures that the var is deleted.
*
* @param char_data *mob The possible companion.
* @param char *name The var name.
* @param int id The var's context/id.
*/
void remove_companion_var(char_data *mob, char *name, int context) {
	struct trig_var_data *vd, *next_vd;
	struct companion_data *cd;
	bool found = FALSE;
	
	if (!mob || !name || !IS_NPC(mob) || !GET_COMPANION(mob) || !(cd = has_companion(GET_COMPANION(mob), GET_MOB_VNUM(mob)))) {
		return;	// no work
	}
	
	LL_FOREACH_SAFE(cd->vars, vd, next_vd) {
		if (!str_cmp(vd->name, name) && (vd->context == 0 || vd->context == context)) {
			found = TRUE;
			LL_DELETE(cd->vars, vd);
			free(vd->value);
			free(vd->name);
			free(vd);
		}
	}
	
	if (found) {
		queue_delayed_update(GET_COMPANION(mob), CDU_SAVE);
	}
}


/**
* Removes a minipet vnum from a player's list.
*
* @param char_data *ch The player.
* @param any_vnum vnum The minipet vnum to forget.
*/
void remove_minipet(char_data *ch, any_vnum vnum) {
	struct minipet_data *mini;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	HASH_FIND_INT(GET_MINIPETS(ch), &vnum, mini);
	if (mini) {
		HASH_DEL(GET_MINIPETS(ch), mini);
		free(mini);
	}
}


/**
* If the mob is a companion, ensures that its triggers are saved.
*
* @param char_data *mob The possible companion.
*/
void reread_companion_trigs(char_data *mob) {
	struct trig_proto_list *tp;
	struct companion_data *cd;
	trig_data *trig;
	
	if (!mob || !IS_NPC(mob) || !GET_COMPANION(mob) || !(cd = has_companion(GET_COMPANION(mob), GET_MOB_VNUM(mob)))) {
		return;	// no work
	}
	
	free_proto_scripts(&cd->scripts);
	cd->scripts = NULL;
	
	if (SCRIPT(mob)) {
		LL_FOREACH(TRIGGERS(SCRIPT(mob)), trig) {
			CREATE(tp, struct trig_proto_list, 1);
			tp->vnum = GET_TRIG_VNUM(trig);
			LL_APPEND(cd->scripts, tp);
		}
	}
	
	queue_delayed_update(GET_COMPANION(mob), CDU_SAVE);
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
	LL_PREPEND(*list, tag);
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
	
	// find top player -- if it's a companion or charmie of some kind
	while (player && IS_NPC(player) && GET_LEADER(player) && IN_ROOM(player) == IN_ROOM(GET_LEADER(player))) {
		player = GET_LEADER(player);
	}
	
	// simple sanity
	if (!mob || !player || mob == player || !IS_NPC(mob) || IS_NPC(player)) {
		return;
	}
	
	// tagged mobs need a reset later
	schedule_reset_mob(mob);
	
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
	char_data *proto = mob_proto(GET_MOB_VNUM(mob));
	
	if (!proto) {
		// somehow? just fall back
		proto = mob;
	}
	
	// MOUNT_x: detect mount flags
	if (AFF_FLAGGED(proto, AFF_FLY)) {
		SET_BIT(flags, MOUNT_FLYING);
	}
	if (MOB_FLAGGED(proto, MOB_AQUATIC)) {
		SET_BIT(flags, MOUNT_AQUATIC);
	}
	if (AFF_FLAGGED(proto, AFF_WATERWALK)) {
		SET_BIT(flags, MOUNT_WATERWALK);
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
	DL_PREPEND(object_list, obj);
}


/**
* Raises or lowers the light count where the object is. This does NOT check if
* the object is lit or not. The object is only used to get the location, if
* any.
*
* @param obj_data *obj The object (ideally this is a light, but it does not check).
* @param bool add If TRUE, adds a light. If FALSE, removes one.
*/
void apply_obj_light(obj_data *obj, bool add) {
	if (obj) {
		if (obj->carried_by) {
			GET_LIGHTS(obj->carried_by) += (add ? 1 : -1);
			if (IN_ROOM(obj->carried_by)) {
				ROOM_LIGHTS(IN_ROOM(obj->carried_by)) += (add ? 1 : -1);
			}
		}
		else if (obj->worn_by) {
			GET_LIGHTS(obj->worn_by) += (add ? 1 : -1);
			if (IN_ROOM(obj->worn_by)) {
				ROOM_LIGHTS(IN_ROOM(obj->worn_by)) += (add ? 1 : -1);
			}
		}
		else if (IN_ROOM(obj)) {
			ROOM_LIGHTS(IN_ROOM(obj)) += (add ? 1 : -1);
		}
	}
}


/**
* Copies an item, even a modified one, to a new item.
*
* @param obj_data *input The item to copy.
*/
obj_data *copy_warehouse_obj(obj_data *input) {
	struct trig_var_data *var, *copy;
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
		
		LL_FOREACH(SCRIPT(input)->global_vars, var) {
			CREATE(copy, struct trig_var_data, 1);
			copy->name = str_dup(var->name);
			copy->value = str_dup(var->value);
			copy->context = var->context;
			LL_APPEND(SCRIPT(obj)->global_vars, copy);
		}
	}
	
	// pointer copies
	if (GET_OBJ_KEYWORDS(input) && (!proto || GET_OBJ_KEYWORDS(input) != GET_OBJ_KEYWORDS(proto))) {
		set_obj_keywords(obj, GET_OBJ_KEYWORDS(input));
	}
	if (GET_OBJ_SHORT_DESC(input) && (!proto || GET_OBJ_SHORT_DESC(input) != GET_OBJ_SHORT_DESC(proto))) {
		set_obj_short_desc(obj, GET_OBJ_SHORT_DESC(input));
	}
	if (GET_OBJ_LONG_DESC(input) && (!proto || GET_OBJ_LONG_DESC(input) != GET_OBJ_LONG_DESC(proto))) {
		set_obj_long_desc(obj, GET_OBJ_LONG_DESC(input));
	}
	if (GET_OBJ_ACTION_DESC(input) && (!proto || GET_OBJ_ACTION_DESC(input) != GET_OBJ_ACTION_DESC(proto))) {
		set_obj_look_desc(obj, GET_OBJ_ACTION_DESC(input), FALSE);
	}
	
	// non-point copies
	GET_OBJ_EXTRA(obj) = GET_OBJ_EXTRA(input);
	GET_OBJ_CURRENT_SCALE_LEVEL(obj) = GET_OBJ_CURRENT_SCALE_LEVEL(input);
	GET_OBJ_AFF_FLAGS(obj) = GET_OBJ_AFF_FLAGS(input);
	GET_OBJ_TIMER(obj) = GET_OBJ_TIMER(input);
	GET_OBJ_WEAR(obj) = GET_OBJ_WEAR(input);
	GET_STOLEN_TIMER(obj) = GET_STOLEN_TIMER(input);
	GET_STOLEN_FROM(obj) = GET_STOLEN_FROM(input);
	
	for (iter = 0; iter < NUM_OBJ_VAL_POSITIONS; ++iter) {
		set_obj_val(obj, iter, GET_OBJ_VAL(input, iter));
	}
	GET_OBJ_APPLIES(obj) = copy_obj_apply_list(GET_OBJ_APPLIES(input));
	OBJ_BOUND_TO(obj) = copy_obj_bindings(OBJ_BOUND_TO(input));
	
	return obj;
}


/**
* Empties the contents of an object into the place the object is. For example,
* an object in a room empties out to the room.
*
* @param obj_data *obj The object to empty.
*/
void empty_obj_before_extract(obj_data *obj) {
	obj_data *jj, *next_thing;
	
	DL_FOREACH_SAFE2(obj->contains, jj, next_thing, next_content) {
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
	char_data *chiter;
	obj_data *proto = obj_proto(GET_OBJ_VNUM(obj));
	
	// safety checks
	check_dg_owner_purged_obj(obj);
	if (obj == purge_bound_items_next) {
		purge_bound_items_next = purge_bound_items_next->next;
	}
	if (obj == global_next_obj) {
		global_next_obj = global_next_obj->next;
	}
	
	// remove from anywhere
	check_obj_in_void(obj);
	
	// check I'm not being used by someone's action
	DL_FOREACH2(player_character_list, chiter, next_plr) {
		if (GET_ACTION_OBJ_TARG(chiter) == obj) {
			GET_ACTION_OBJ_TARG(chiter) = NULL;
		}
	}

	/* Get rid of the contents of the object, as well. */
	while (obj->contains) {
		extract_obj(obj->contains);
	}
	
	cancel_all_stored_events(&GET_OBJ_STORED_EVENTS(obj));
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
* Finds the room whose .pack file this object should be saved in, if any.
*
* This is mainly called by: request_obj_save_in_world(obj)
*
* @param obj_data *obj The object.
* @return room_data* A room that this object should save in.
*/
room_data *find_room_obj_saves_in(obj_data *obj) {
	// find parent obj
	while (obj->in_obj) {
		obj = obj->in_obj;
	}
	
	if (IN_ROOM(obj)) {
		return IN_ROOM(obj);
	}
	else if (obj->in_vehicle) {
		return IN_ROOM(obj->in_vehicle);
	}
	else {	// is not in a room (or in a vehicle in a room)
		return NULL;
	}
}


/**
* Makes a fresh copy of an object, preserving certain flags such as superior
* and keep. All temporary properties including bindings are copied over. You
* can swap it out using swap_obj_for_obj() or any other method, then extract
* the original object when done.
*
* @param obj_data *obj The item to load a fresh copy of.
* @param int scale_level If >0, will scale the new copy to that level.
* @param bool keep_strings If TRUE, preserves any changed strings.
* @param bool keep_augments If TRUE, preserves any enchantments/hones etc.
* @return obj_data* The new object.
*/
obj_data *fresh_copy_obj(obj_data *obj, int scale_level, bool keep_strings, bool keep_augments) {
	struct trig_var_data *var, *copy;
	struct obj_binding *bind;
	obj_data *proto, *new;
	trig_data *trig;
	struct eq_set_obj *eq_set, *new_set;
	struct obj_apply *apply_iter, *old_apply, *new_apply;
	int iter;
	bool found;
	
	if (!obj || !(proto = obj_proto(GET_OBJ_VNUM(obj)))) {
		// get a normal 'bug' object
		return read_object(0, FALSE);
	}

	new = read_object(GET_OBJ_VNUM(obj), FALSE);
	
	// preserve some flags (see later for enchanted)
	GET_OBJ_EXTRA(new) |= GET_OBJ_EXTRA(obj) & OBJ_PRESERVE_FLAGS;
	
	// remove preservable flags that are absent in the original
	GET_OBJ_EXTRA(new) &= ~(OBJ_PRESERVE_FLAGS & ~GET_OBJ_EXTRA(obj));
	
	// always remove quality flags if it's now generic
	if (OBJ_FLAGGED(new, OBJ_GENERIC_DROP)) {
		REMOVE_BIT(GET_OBJ_EXTRA(new), (OBJ_HARD_DROP | OBJ_GROUP_DROP));
	}
	
	// copy exact bind flags and bindings
	GET_OBJ_EXTRA(new) &= ~OBJ_BIND_FLAGS;
	GET_OBJ_EXTRA(new) |= (GET_OBJ_EXTRA(obj) & OBJ_BIND_FLAGS);
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
	
	// copy eq sets
	LL_FOREACH(GET_OBJ_EQ_SETS(obj), eq_set) {
		CREATE(new_set, struct eq_set_obj, 1);
		*new_set = *eq_set;
		new_set->next = NULL;
		LL_APPEND(GET_OBJ_EQ_SETS(new), new_set);
	}
	
	// custom strings?
	if (keep_strings) {
		if (GET_OBJ_SHORT_DESC(obj) && GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(proto)) {
			set_obj_short_desc(new, GET_OBJ_SHORT_DESC(obj));
		}
		if (GET_OBJ_LONG_DESC(obj) && GET_OBJ_LONG_DESC(obj) != GET_OBJ_LONG_DESC(proto)) {
			set_obj_long_desc(new, GET_OBJ_LONG_DESC(obj));
		}
		if (GET_OBJ_KEYWORDS(obj) && GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(proto)) {
			set_obj_keywords(new, GET_OBJ_KEYWORDS(obj));
		}
		if (GET_OBJ_ACTION_DESC(obj) && GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(proto)) {
			set_obj_look_desc(new, GET_OBJ_ACTION_DESC(obj), FALSE);
		}
	}
	
	// ITEM_x: certain things that must always copy over
	switch (GET_OBJ_TYPE(new)) {
		case ITEM_AMMO: {
			set_obj_val(new, VAL_AMMO_QUANTITY, GET_OBJ_VAL(obj, VAL_AMMO_QUANTITY));
			break;
		}
		case ITEM_BOOK: {
			set_obj_val(new, VAL_BOOK_ID, GET_OBJ_VAL(obj, VAL_BOOK_ID));
			break;
		}
		case ITEM_COINS: {
			set_obj_val(new, VAL_COINS_AMOUNT, GET_OBJ_VAL(obj, VAL_COINS_AMOUNT));
			set_obj_val(new, VAL_COINS_EMPIRE_ID, GET_OBJ_VAL(obj, VAL_COINS_EMPIRE_ID));
			break;
		}
		case ITEM_CORPSE: {
			set_obj_val(new, VAL_CORPSE_IDNUM, GET_OBJ_VAL(obj, VAL_CORPSE_IDNUM));
			set_obj_val(new, VAL_CORPSE_SIZE, GET_OBJ_VAL(obj, VAL_CORPSE_SIZE));
			set_obj_val(new, VAL_CORPSE_FLAGS, GET_OBJ_VAL(obj, VAL_CORPSE_FLAGS));
			break;
		}
		case ITEM_DRINKCON: {
			set_obj_val(new, VAL_DRINK_CONTAINER_CONTENTS, GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS));
			set_obj_val(new, VAL_DRINK_CONTAINER_TYPE, GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE));
			
			// check capacity
			if (GET_OBJ_VAL(new, VAL_DRINK_CONTAINER_CONTENTS) > GET_OBJ_VAL(new, VAL_DRINK_CONTAINER_CAPACITY)) {
				set_obj_val(new, VAL_DRINK_CONTAINER_CONTENTS, GET_OBJ_VAL(new, VAL_DRINK_CONTAINER_CAPACITY));
			}
			break;
		}
		case ITEM_LIGHT: {
			// only copy hours-remaining if not unlimited on either end
			if (GET_OBJ_VAL(new, VAL_LIGHT_HOURS_REMAINING) != UNLIMITED && GET_OBJ_VAL(obj, VAL_LIGHT_HOURS_REMAINING) != UNLIMITED) {
				set_obj_val(new, VAL_LIGHT_HOURS_REMAINING, GET_OBJ_VAL(obj, VAL_LIGHT_HOURS_REMAINING));
			}
			set_obj_val(new, VAL_LIGHT_IS_LIT, GET_OBJ_VAL(obj, VAL_LIGHT_IS_LIT));
			break;
		}
		case ITEM_LIGHTER: {
			// only copy uses if not unlimited on either end
			if (GET_OBJ_VAL(new, VAL_LIGHTER_USES) != UNLIMITED && GET_OBJ_VAL(obj, VAL_LIGHTER_USES) != UNLIMITED) {
				set_obj_val(new, VAL_LIGHTER_USES, GET_OBJ_VAL(obj, VAL_LIGHTER_USES));
			}
			break;
		}
		case ITEM_OTHER: {
			// copy everything for "other"
			for (iter = 0; iter < NUM_OBJ_VAL_POSITIONS; ++iter) {
				set_obj_val(new, iter, GET_OBJ_VAL(obj, iter));
			}
			break;
		}
		case ITEM_PORTAL: {
			set_obj_val(new, VAL_PORTAL_TARGET_VNUM, GET_OBJ_VAL(obj, VAL_PORTAL_TARGET_VNUM));
			break;
		}
		case ITEM_POISON: {
			set_obj_val(new, VAL_POISON_CHARGES, GET_OBJ_VAL(obj, VAL_POISON_CHARGES));
			break;
		}
		case ITEM_RECIPE: {
			set_obj_val(new, VAL_RECIPE_VNUM, GET_OBJ_VAL(obj, VAL_RECIPE_VNUM));
			break;
		}
		case ITEM_SHIP: {
			// copy these blind
			for (iter = 0; iter < NUM_OBJ_VAL_POSITIONS; ++iter) {
				set_obj_val(new, iter, GET_OBJ_VAL(obj, iter));
			}
			break;
		}
	}
	
	// copy only existing scripts
	if (SCRIPT(obj)) {
		if (!SCRIPT(new)) {
			create_script_data(new, OBJ_TRIGGER);
		}

		for (trig = TRIGGERS(SCRIPT(obj)); trig; trig = trig->next) {
			add_trigger(SCRIPT(new), read_trigger(GET_TRIG_VNUM(trig)), -1);
		}
		
		LL_FOREACH(SCRIPT(obj)->global_vars, var) {
			CREATE(copy, struct trig_var_data, 1);
			copy->name = str_dup(var->name);
			copy->value = str_dup(var->value);
			copy->context = var->context;
			LL_APPEND(SCRIPT(new)->global_vars, copy);
		}
	}

	if (scale_level > 0) {
		scale_item_to_level(new, scale_level);
	}
	
	// copy enchantments/hone ONLY if level is the same
	if (keep_augments && GET_OBJ_CURRENT_SCALE_LEVEL(new) == GET_OBJ_CURRENT_SCALE_LEVEL(obj)) {
		if (OBJ_FLAGGED(obj, OBJ_ENCHANTED) && !OBJ_FLAGGED(new, OBJ_ENCHANTED)) {
			SET_BIT(GET_OBJ_EXTRA(new), OBJ_ENCHANTED);
		}
		
		LL_FOREACH(GET_OBJ_APPLIES(obj), apply_iter) {
			// only copies ones added by the player
			if (apply_type_from_player[apply_iter->apply_type]) {
				// ensure it's not on the proto
				found = FALSE;
				LL_FOREACH(GET_OBJ_APPLIES(proto), old_apply) {
					if (old_apply->apply_type == apply_iter->apply_type && old_apply->location == apply_iter->location) {
						found = TRUE;
						break;
					}
				}
				if (found) {
					continue;	// no need to copy
				}
				
				// copy apply
				CREATE(new_apply, struct obj_apply, 1);
				*new_apply = *apply_iter;
				new_apply->next = NULL;
				LL_APPEND(GET_OBJ_APPLIES(new), new_apply);
			}
		}
	}
	
	return new;
}


/**
* Compare the person/people two objects are bound to.
*
* @param obj_data *obj_a The first obj.
* @param obj_data *obj_b The second obj.
* @return bool TRUE if the bindings are the same; FALSE if not.
*/
bool identical_bindings(obj_data *obj_a, obj_data *obj_b) {
	struct obj_binding *a_bind, *b_bind, *b_bind_list, *b_bind_next;
	bool found;
	
	if (!OBJ_BOUND_TO(obj_a) && !OBJ_BOUND_TO(obj_b)) {
		return TRUE;	// no bindings on either
	}
	
	// compare bindings just like applies
	b_bind_list = copy_obj_bindings(OBJ_BOUND_TO(obj_b));
	LL_FOREACH(OBJ_BOUND_TO(obj_a), a_bind) {
		found = FALSE;
		LL_FOREACH_SAFE(b_bind_list, b_bind, b_bind_next) {
			if (a_bind->idnum == b_bind->idnum) {
				LL_DELETE(b_bind_list, b_bind);
				free(b_bind);
				found = TRUE;
				break;
			}
		}
		
		if (!found) {
			free_obj_binding(&b_bind_list);	// remaining items
			return FALSE;
		}
	}
	if (b_bind_list) {	// more things in b_bind_list than a
		free_obj_binding(&b_bind_list);
		return FALSE;
	}
	
	// otherwise
	return TRUE;
}


/**
* Compares two items for identicallity. These may be highly-customized items.
* 
* @param obj_data *obj_a First object to compare.
* @param obj_data *obj_b Second object to compare.
* @return bool TRUE if the two items are functionally identical.
*/
bool objs_are_identical(obj_data *obj_a, obj_data *obj_b) {
	struct obj_apply *a_apply, *b_list, *b_apply, *b_apply_next;
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
	if (GET_OBJ_KEYWORDS(obj_a) != GET_OBJ_KEYWORDS(obj_b) && strcmp(GET_OBJ_KEYWORDS(obj_a), GET_OBJ_KEYWORDS(obj_b))) {
		return FALSE;
	}
	if (GET_OBJ_SHORT_DESC(obj_a) != GET_OBJ_SHORT_DESC(obj_b) && strcmp(GET_OBJ_SHORT_DESC(obj_a), GET_OBJ_SHORT_DESC(obj_b))) {
		return FALSE;
	}
	if (GET_OBJ_LONG_DESC(obj_a) != GET_OBJ_LONG_DESC(obj_b) && strcmp(GET_OBJ_LONG_DESC(obj_a), GET_OBJ_LONG_DESC(obj_b))) {
		return FALSE;
	}
	if (GET_OBJ_ACTION_DESC(obj_a) != GET_OBJ_ACTION_DESC(obj_b) && strcmp(GET_OBJ_ACTION_DESC(obj_a), GET_OBJ_ACTION_DESC(obj_b))) {
		return FALSE;
	}
	if (!identical_bindings(obj_a, obj_b)) {
		return FALSE;
	}
	
	// to compare applies, we're going to copy and delete as we find them
	b_list = copy_obj_apply_list(GET_OBJ_APPLIES(obj_b));
	for (a_apply = GET_OBJ_APPLIES(obj_a); a_apply; a_apply = a_apply->next) {
		found = FALSE;
		for (b_apply = b_list; b_apply; b_apply = b_apply_next) {
			b_apply_next = b_apply->next;
			if (a_apply->location == b_apply->location && a_apply->modifier == b_apply->modifier && a_apply->apply_type == b_apply->apply_type) {
				found = TRUE;
				LL_DELETE(b_list, b_apply);
				free(b_apply);
				break;	// only need one, plus we freed it
			}
		}
		
		if (!found) {
			free_obj_apply_list(b_list);	// remaining items
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
* Remove an object from the global object list.
*
* @param obj_data *obj The item to remove from the global object list.
*/
void remove_from_object_list(obj_data *obj) {
	// ensure it's (probably) in the list first
	if (object_list && (object_list == obj || obj->next || obj->prev)) {
		DL_DELETE(object_list, obj);
		obj->prev = obj->next = NULL;
	}
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
	LL_PREPEND(*list, bind);
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
	request_obj_save_in_world(obj);
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
	request_obj_save_in_world(obj);
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
	request_obj_save_in_world(obj);
}


/**
* Determines if a player-idnum can legally have a bound object.
*
* @param obj_data *obj The item to check.
* @param int idnum The player idnum to check.
* @return bool TRUE if the player can use obj, FALSE if binding prevents it.
*/
bool bind_ok_idnum(obj_data *obj, int idnum) {
	struct obj_binding *bind;
	
	// basic sanity
	if (!obj) {
		return FALSE;
	}
	
	// bound at all?
	if (!OBJ_BOUND_TO(obj)) {
		return TRUE;
	}
	
	LL_FOREACH(OBJ_BOUND_TO(obj), bind) {
		if (bind->idnum == idnum) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* Determines if a player can legally obtain a bound object.
*
* @param obj_data *obj The item to check.
* @param char_data *ch The player to check.
* @return bool TRUE if ch can use obj, FALSE if binding prevents it.
*/
bool bind_ok(obj_data *obj, char_data *ch) {
	// basic sanity
	if (!obj || !ch) {
		return FALSE;
	}
	
	// imm/npc override
	if (IS_NPC(ch) || IS_IMMORTAL(ch)) {
		return TRUE;
	}
	
	return bind_ok_idnum(obj, GET_IDNUM(ch));
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
		LL_PREPEND(list, bind);
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
	struct obj_binding *bind, *next_bind;
	
	if (!obj || !player || IS_NPC(player) || IS_IMMORTAL(player)) {
		return;
	}
	
	for (bind = OBJ_BOUND_TO(obj); bind; bind = next_bind) {
		next_bind = bind->next;
		if (bind->idnum != GET_IDNUM(player)) {
			LL_DELETE(OBJ_BOUND_TO(obj), bind);
			free(bind);
		}
	}
	request_obj_save_in_world(obj);
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
* @param int pos the WEAR_ spot to it
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
		if (LIGHT_IS_LIT(obj)) {
			++GET_LIGHTS(ch);
			if (IN_ROOM(ch)) {
				++ROOM_LIGHTS(IN_ROOM(ch));
			}
		}
		
		// TODO this seems like a huge error: why is it adding to is-carrying when equipping a container
		// or is it because contents still need to count against it?
		if (IS_CONTAINER(obj)) {
			IS_CARRYING_N(ch) += obj_carry_size(obj);
			update_MSDP_inventory(ch, UPDATE_SOON);
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
		if (!IS_NPC(ch)) {
			queue_delayed_update(ch, CDU_PASSIVE_BUFFS);
		}
	}
}


/**
* take an object from a char's inventory
*
* @param obj_data *object The item to remove
*/
void obj_from_char(obj_data *object) {
	if (object == NULL) {
		log("SYSERR: NULL object passed to obj_from_char.");
	}
	else {
		request_obj_save_in_world(object);
		
		DL_DELETE2(object->carried_by->carrying, object, prev_content, next_content);
		object->next_content = object->prev_content = NULL;

		IS_CARRYING_N(object->carried_by) -= obj_carry_size(object);
		update_MSDP_inventory(object->carried_by, UPDATE_SOON);

		// check lights
		if (LIGHT_IS_LIT(object)) {
			--GET_LIGHTS(object->carried_by);
			if (IN_ROOM(object->carried_by)) {
				--ROOM_LIGHTS(IN_ROOM(object->carried_by));
			}
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
	obj_data *obj_from;

	if (obj->in_obj == NULL) {
		log("SYSERR: (%s): trying to illegally extract obj from obj.", __FILE__);
	}
	else {
		cancel_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_AUTOSTORE);
		remove_dropped_item_anywhere(obj);
		request_obj_save_in_world(obj);
		
		obj_from = obj->in_obj;
		DL_DELETE2(obj_from->contains, obj, prev_content, next_content);

		GET_OBJ_CARRYING_N(obj_from) -= obj_carry_size(obj);
		if (obj_from->carried_by) {
			IS_CARRYING_N(obj_from->carried_by) -= obj_carry_size(obj);
			update_MSDP_inventory(obj_from->carried_by, UPDATE_SOON);
		}
		if (obj_from->worn_by && IS_CONTAINER(obj_from)) {
			IS_CARRYING_N(obj_from->worn_by) -= obj_carry_size(obj);
			update_MSDP_inventory(obj_from->worn_by, UPDATE_SOON);
		}

		obj->in_obj = NULL;
		obj->next_content = obj->prev_content = NULL;
	}
}


/**
* Take an object from a room
*
* @param obj_data *object The item to remove.
*/
void obj_from_room(obj_data *object) {
	if (!object || !IN_ROOM(object)) {
		log("SYSERR: NULL object (%p) or obj not in a room (%p) passed to obj_from_room", object, IN_ROOM(object));
	}
	else {
		cancel_stored_event(&GET_OBJ_STORED_EVENTS(object), SEV_OBJ_AUTOSTORE);
		request_obj_save_in_world(object);
		
		// update lights
		if (LIGHT_IS_LIT(object)) {
			--ROOM_LIGHTS(IN_ROOM(object));
		}
		if (ROOM_OWNER(IN_ROOM(object))) {
			remove_dropped_item(ROOM_OWNER(IN_ROOM(object)), object);
		}
		
		DL_DELETE2(ROOM_CONTENTS(IN_ROOM(object)), object, prev_content, next_content);
		IN_ROOM(object) = NULL;
		object->next_content = object->prev_content = NULL;
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
		cancel_stored_event(&GET_OBJ_STORED_EVENTS(object), SEV_OBJ_AUTOSTORE);
		request_obj_save_in_world(object);
		if (VEH_OWNER(object->in_vehicle)) {
			remove_dropped_item(VEH_OWNER(object->in_vehicle), object);
		}
		VEH_LAST_MOVE_TIME(object->in_vehicle) = time(0);	// reset autostore time
		VEH_CARRYING_N(object->in_vehicle) -= obj_carry_size(object);
		DL_DELETE2(VEH_CONTAINS(object->in_vehicle), object, prev_content, next_content);
		object->in_vehicle = NULL;
		object->next_content = object->prev_content = NULL;
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
		DL_PREPEND2(ch->carrying, object, prev_content, next_content);
		object->carried_by = ch;
		IS_CARRYING_N(ch) += obj_carry_size(object);
		
		update_MSDP_inventory(ch, UPDATE_SOON);
		
		// binding
		if (OBJ_FLAGGED(object, OBJ_BIND_ON_PICKUP)) {
			bind_obj_to_player(object, ch);
		}
		
		// new owner?
		if (IS_NPC(ch) || object->last_owner_id != GET_IDNUM(ch)) {
			clear_obj_eq_sets(object);
		}
		
		// set the timer here; actual rules for it are in limits.c
		// we do NOT schedule the actual autostore check here (items on chars don't autostore)
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
		
		// update lights
		if (LIGHT_IS_LIT(object)) {
			++GET_LIGHTS(ch);
			
			// check if the room needs to be lit
			if (IN_ROOM(ch)) {
				ROOM_LIGHTS(IN_ROOM(ch))++;
			}
		}
		
		qt_get_obj(ch, object);
		schedule_obj_timer_update(object, FALSE);
		request_obj_save_in_world(object);
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
	if (IN_ROOM(ch) && (!CAN_WEAR(obj, ITEM_WEAR_TAKE) || (!IS_NPC(ch) && !CAN_CARRY_OBJ(ch, obj)))) {
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
		if (obj_to->carried_by) {
			IS_CARRYING_N(obj_to->carried_by) += obj_carry_size(obj);
			update_MSDP_inventory(obj_to->carried_by, UPDATE_SOON);
		}
		if (obj_to->worn_by && IS_CONTAINER(obj_to)) {
			IS_CARRYING_N(obj_to->worn_by) += obj_carry_size(obj);
			update_MSDP_inventory(obj_to->worn_by, UPDATE_SOON);
		}
		
		// set the timer here; actual rules for it are in limits.c
		if (!suspend_autostore_updates) {
			schedule_obj_autostore_check(obj, time(0));
		}
		
		// clear these now
		REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
		clear_obj_eq_sets(obj);
		
		DL_PREPEND2(obj_to->contains, obj, prev_content, next_content);
		obj->in_obj = obj_to;
		
		add_dropped_item_anywhere(obj, NULL);
		schedule_obj_timer_update(obj, FALSE);
		request_obj_save_in_world(obj);
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
		DL_PREPEND2(ROOM_CONTENTS(room), object, prev_content, next_content);
		IN_ROOM(object) = room;
		
		// check light
		if (LIGHT_IS_LIT(object)) {
			++ROOM_LIGHTS(IN_ROOM(object));
		}
		
		// clear these now
		REMOVE_BIT(GET_OBJ_EXTRA(object), OBJ_KEEP);
		clear_obj_eq_sets(object);

		// set the timer here; actual rules for it are in limits.c
		if (!suspend_autostore_updates) {
			schedule_obj_autostore_check(object, time(0));
		}
		
		if (ROOM_OWNER(room)) {
			add_dropped_item(ROOM_OWNER(room), object);
		}
		
		schedule_obj_timer_update(object, FALSE);
		request_obj_save_in_world(object);
		
		// see if anybody wants to eat it
		if (IS_CORPSE(object)) {
			check_scavengers(room);
		}
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
		
		DL_PREPEND2(VEH_CONTAINS(veh), object, prev_content, next_content);
		object->in_vehicle = veh;
		VEH_CARRYING_N(veh) += obj_carry_size(object);
		
		// clear these now
		REMOVE_BIT(GET_OBJ_EXTRA(object), OBJ_KEEP);
		clear_obj_eq_sets(object);
		
		// set the timer here; actual rules for it are in limits.c
		VEH_LAST_MOVE_TIME(veh) = time(0);
		if (!suspend_autostore_updates) {
			// update this time but don't schedule an autostore event -- vehicles do it themselves
			GET_AUTOSTORE_TIMER(object) = time(0);
		}
		
		if (VEH_OWNER(veh)) {
			add_dropped_item(VEH_OWNER(veh), object);
		}
		
		schedule_obj_timer_update(object, FALSE);
		request_obj_save_in_world(object);
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
* @param int pos The WEAR_ slot to remove.
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
		if (LIGHT_IS_LIT(obj)) {
			--GET_LIGHTS(ch);
			if (IN_ROOM(ch)) {
				--ROOM_LIGHTS(IN_ROOM(ch));
			}
		}
		
		if (IS_CONTAINER(obj)) {
			IS_CARRYING_N(ch) -= obj_carry_size(obj);
			update_MSDP_inventory(ch, UPDATE_SOON);
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
		if (!IS_NPC(ch)) {
			queue_delayed_update(ch, CDU_PASSIVE_BUFFS);
		}
	}

	return obj;
}


/**
* Calls unequip_char() and then gives it to the character if it's not 1-use.
* This also handles single-use items by extracting them.
*
* @param char_data *ch The person to unequip
* @param int pos The WEAR_ slot to remove
* @return obj_data* A pointer to the object removed IF it wasn't extracted.
*/
obj_data *unequip_char_to_inventory(char_data *ch, int pos) {
	obj_data *obj = unequip_char(ch, pos);
	
	if (obj && OBJ_FLAGGED(obj, OBJ_SINGLE_USE) && pos != WEAR_SHARE) {
		extract_obj(obj);
	}
	else if (obj) {
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
* @param int pos The WEAR_ position to remove
* @return obj_data* A pointer to the obj IF it wasn't extracted.
*/
obj_data *unequip_char_to_room(char_data *ch, int pos) {
	obj_data *obj = unequip_char(ch, pos);
	
	if (obj && OBJ_FLAGGED(obj, OBJ_SINGLE_USE) && pos != WEAR_SHARE) {
		extract_obj(obj);
	}
	else if (obj && IN_ROOM(ch)) {
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
* Counts how many messages are available of a given type.
*
* @param struct custom_message *list The list of messages to check.
* @param int type The type const to check for.
* @return int How many messages of that type were in the list.
*/
int count_custom_messages(struct custom_message *list, int type) {
	struct custom_message *ocm;
	int count = 0;
	
	LL_FOREACH(list, ocm) {
		if (ocm->type == type) {
			++count;
		}
	}
	
	return count;
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
* This gets a custom message of a given type from a list. If there is more
* than one message of the requested type, it returns one at random. You will
* get back a null if there are no messages of the requested type; you can check
* this ahead of time with has_custom_message().
*
* @param struct custom_message *list The list of messages to check.
* @param int type The type const for the message.
* @return char* The custom message, or NULL if there is none.
*/
char *get_custom_message(struct custom_message *list, int type) {
	struct custom_message *ocm;
	char *found = NULL;
	int num_found = 0;
	
	LL_FOREACH(list, ocm) {
		if (ocm->type == type) {
			if (!number(0, num_found++) || !found) {
				found = ocm->msg;
			}
		}
	}
	
	return found;
}


/**
* This gets a specific custom message of a given type from a list. Unlike
* get_custom_message(), this only returns the one in the exact position you
* requested, not random. You can check its existence in advance with
* has_custom_message_pos().
*
* @param struct custom_message *list The list of messages to check.
* @param int type The type const for the message.
* @param int pos Which message to get, in order (0 is the first message).
* @return char* The custom message, or NULL if there is none in that position.
*/
char *get_custom_message_pos(struct custom_message *list, int type, int pos) {
	struct custom_message *ocm;
	char *found = NULL;
	
	if (pos == NOTHING) {
		return NULL;	// shortcut
	}
	
	LL_FOREACH(list, ocm) {
		if (ocm->type == type && pos-- <= 0) {
			found = ocm->msg;
			break;
		}
	}
	
	return found;
}


/**
* Picks a custom message at random from a set (by type), and then returns
* the position number it was in, for use with get_custom_message_pos().
*
* @param struct custom_message *list The list of messages to check.
* @param int type The type const for the message.
* @return int A random message position, or NOTHING if no messages of that type were found.
*/
int get_custom_message_random_pos_number(struct custom_message *list, int type) {
	struct custom_message *ocm;
	int found = NOTHING;
	int num_found = 0;
	
	LL_FOREACH(list, ocm) {
		if (ocm->type == type) {
			if (!number(0, num_found++)) {
				found = num_found - 1;
			}
		}
	}
	
	return found;
}


/**
* @param struct custom_message *list The list of messages to check.
* @param int type The type const for the message.
* @return bool TRUE if the object has at least one message of the requested type.
*/
bool has_custom_message(struct custom_message *list, int type) {
	struct custom_message *ocm;
	bool found = FALSE;
	
	LL_FOREACH(list, ocm) {
		if (ocm->type == type) {
			found = TRUE;
			break;
		}
	}
	
	return found;
}


/**
* This is similar to has_custom_message() but checks for a specific message
* position, for things that send the messages in order such as play-instrument.
*
* @param struct custom_message *list The list of messages to check.
* @param int type The type const for the message.
* @param int pos Must have at least pos+1 messages (0 is the first message).
* @return bool TRUE if the object has at a message of the requested type and position number.
*/
bool has_custom_message_pos(struct custom_message *list, int type, int pos) {
	struct custom_message *ocm;
	bool found = FALSE;
	
	LL_FOREACH(list, ocm) {
		if (ocm->type == type && pos-- <= 0) {
			found = TRUE;
			break;
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT TARGETING HANDLERS ///////////////////////////////////////////////

/**
* Search a given list for a matching component and return it (will not return
* kept items). If finds an extended match (not perfect match) on the component,
* it will prefer a 'basic' component over a non-basic one.
*
* @param any_vnum cmp_vnum The generic component to look for (or any of its subtypes).
* @param obj_data *list The start of any object list.
* @param bool *kept A variable to bind to if there was a match but it was marked 'keep' (which won't be returned).
* @return obj_data *The first matching object in the list, if any.
*/
obj_data *get_component_in_list(any_vnum cmp_vnum, obj_data *list, bool *kept) {
	obj_data *obj, *basic = NULL, *non_basic = NULL;
	bool found_keep = FALSE;
	generic_data *gen, *tmp;
	
	*kept = FALSE;
	
	// load the generic component
	if (!(gen = real_generic(cmp_vnum)) || GEN_TYPE(gen) != GENERIC_COMPONENT) {
		return NULL;
	}
	
	DL_FOREACH2(list, obj, next_content) {
		if (GET_OBJ_COMPONENT(obj) == cmp_vnum) {
			// full match
			if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
				found_keep = TRUE;
			}
			else {
				return obj;	// found perfect match
			}
		}
		else if (is_component(obj, gen)) {
			// partial match
			if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
				found_keep = TRUE;
			}
			else if (!basic) {
				if ((tmp = real_generic(GET_OBJ_COMPONENT(obj))) && GEN_FLAGGED(tmp, GEN_BASIC)) {
					basic = obj;
				}
				else if (tmp && !non_basic) {
					non_basic = obj;
				}
			}
		}
	}
	
	// found a partial match?
	if (basic || non_basic) {
		return basic ? basic : non_basic;
	}
	
	// failed
	*kept = found_keep;
	return NULL;
}


/**
* Find an object in another person's share slot, by character name.
*
* @param char_data *ch The person looking for a shared obj.
* @param char *arg The potential name of a PLAYER.
*/
obj_data *get_obj_by_char_share(char_data *ch, char *arg) {
	char_data *targ;
	
	// find person by name
	if (!(targ = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
		return NULL;
	}
	
	return GET_EQ(targ, WEAR_SHARE);
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
	
	DL_FOREACH2(list, i, next_content) {
		if (GET_OBJ_VNUM(i) == num) {
			found = i;
			break;
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
	
	DL_FOREACH2(list, i, next_content) {
		if (GET_OBJ_VNUM(i) == vnum) {
			found = i;
			break;
		}
	}

	return found;
}


/**
* Finds and object the char can see in any list (ch->carrying, etc).
*
* @param char_data *ch The person who's looking.
* @param char *name The target argument.
* @param int *number Optional: For multi-list number targeting (look 4.hat; may be NULL)
* @param obj_data *list The list to search.
* @return obj_data *The item found, or NULL.
*/
obj_data *get_obj_in_list_vis(char_data *ch, char *name, int *number, obj_data *list) {
	char copy[MAX_INPUT_LENGTH], *tmp = copy;
	obj_data *i;
	int num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	DL_FOREACH2(list, i, next_content) {
		if (CAN_SEE_OBJ(ch, i) && MATCH_ITEM_NAME(tmp, i)) {
			if (--(*number) == 0) {
				return i;
			}
		}
	}

	return NULL;
}


/**
* Finds an object the char can see in any list (ch->carrying, etc), with a
* preference for one that has a given interaction type. However, if the player
* gave a specific object using a number (2.tree) this will ignore the interact
* request.
*
* @param char_data *ch The person who's looking.
* @param char *name The target argument.
* @param int *number Optional: For multi-list number targeting (look 4.hat; may be NULL)
* @param obj_data *list The list to search.
* @return int interact_type Any INTERACT_ to prefer on a matching object.
* @return obj_data *The item found, or NULL. May or may not have the interaction.
*/
obj_data *get_obj_in_list_vis_prefer_interaction(char_data *ch, char *name, int *number, obj_data *list, int interact_type) {
	char copy[MAX_INPUT_LENGTH], *tmp = copy;
	obj_data *i, *backup = NULL;
	int num;
	bool gave_num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	// if the number is > 1 (PROBABLY requested #.name) take any item that matches
	// note: this changed in b5.108; previously it could detect this more accurately
	gave_num = (*number > 1);
	
	DL_FOREACH2(list, i, next_content) {
		if (CAN_SEE_OBJ(ch, i) && MATCH_ITEM_NAME(tmp, i)) {
			if (gave_num) {
				if (--(*number) == 0) {
					return i;
				}
			}
			else {	// did not give a number
				if (has_interaction(GET_OBJ_INTERACTIONS(i), interact_type)) {
					return i;	// perfect match
				}
				else if (!backup) {
					backup = i;	// missing interaction but otherwise a match
				}
			}
		}
	}

	return backup;
}


/**
* Finds an object the char can see in any list (ch->carrying, etc), with a
* preference for one that has a given object type. However, if the player
* gave a specific object using a number (2.tree) this will ignore the type
* request.
*
* @param char_data *ch The person who's looking.
* @param char *name The target argument.
* @param int *number Optional: For multi-list number targeting (look 4.hat; may be NULL)
* @param obj_data *list The list to search.
* @return int obj_type Any ITEM_ to prefer on a matching object.
* @return obj_data *The item found, or NULL. May or may not have the type.
*/
obj_data *get_obj_in_list_vis_prefer_type(char_data *ch, char *name, int *number, obj_data *list, int obj_type) {
	char copy[MAX_INPUT_LENGTH], *tmp = copy;
	obj_data *i, *backup = NULL;
	int num;
	bool gave_num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	// if the number is > 1 (PROBABLY requested #.name) take any item that matches
	// note: this changed in b5.108; previously it could detect this more accurately
	gave_num = (*number > 1);
	
	DL_FOREACH2(list, i, next_content) {
		if (CAN_SEE_OBJ(ch, i) && MATCH_ITEM_NAME(tmp, i)) {
			if (gave_num) {
				if (--(*number) == 0) {
					return i;
				}
			}
			else {	// did not give a number
				if (GET_OBJ_TYPE(i) == obj_type) {
					return i;	// perfect match
				}
				else if (!backup) {
					backup = i;	// missing interaction but otherwise a match
				}
			}
		}
	}

	return backup;
}


/**
* Gets the position of a piece of equipment the character is using, by name.
*
* @param char_data *ch The person who's looking.
* @param char *arg The typed argument (item name).
* @param int *number Optional: For multi-list number targeting (look 4.cart; may be NULL)
* @param obj_data *equipment[] The character's gear array.
* @return int The WEAR_ position, or NO_WEAR if no match was found.
*/
int get_obj_pos_in_equip_vis(char_data *ch, char *arg, int *number, obj_data *equipment[]) {
	char copy[MAX_INPUT_LENGTH], *tmp = copy;
	int j, num;

	if (!number) {
		strcpy(tmp, arg);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = arg;
	}
	if (*number == 0) {
		return NO_WEAR;
	}

	for (j = 0; j < NUM_WEARS; j++)
		if (equipment[j] && CAN_SEE_OBJ(ch, equipment[j]) && isname(arg, equipment[j]->name))
			if (--(*number) == 0)
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
		if (storable_only && !GET_OBJ_STORAGE(obj)) {
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
* @param int *number Optional: For multi-list number targeting (look 4.hat; may be NULL)
* @return obj_data *The found item, or NULL.
*/
obj_data *get_obj_vis(char_data *ch, char *name, int *number) {
	char copy[MAX_INPUT_LENGTH], *tmp = copy;
	obj_data *i;
	int num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	/* scan items carried */
	if ((i = get_obj_in_list_vis(ch, tmp, number, ch->carrying)) != NULL)
		return (i);
	
	/* scan room */
	if ((i = get_obj_in_list_vis(ch, tmp, number, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL)
		return (i);
	
	/* ok.. no luck yet. scan the entire obj list   */
	DL_FOREACH(object_list, i) {
		if (CAN_SEE_OBJ(ch, i) && MATCH_ITEM_NAME(tmp, i)) {
			if (--(*number) == 0) {
				return i;
			}
		}
	}

	return NULL;
}


/**
* Finds a matching object in an array of equipped gear. If no objects is found,
* the "pos" variable is set to NOTHING.
*
* @param char_data *ch The person who's looking for an item.
* @param char *arg The target argument.
* @param int *number Optional: For multi-list number targeting (look 4.hat; may be NULL)
* @param obj_data *equipment[] A pointer to an equipment array.
* @param int *pos A variable to store the WEAR_ const if an item is found.
* @return obj_data *The found object, or NULL.
*/
obj_data *get_obj_in_equip_vis(char_data *ch, char *arg, int *number, obj_data *equipment[], int *pos) {
	char copy[MAX_INPUT_LENGTH], *tmp = copy;
	obj_data *found = NULL;
	int iter, num;
	
	if (pos) {
		*pos = NOTHING;
	}
	
	if (!number) {
		strcpy(tmp, arg);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = arg;
	}
	if (*number == 0) {
		return NULL;
	}
	
	for (iter = 0; iter < NUM_WEARS && !found; ++iter) {
		if (equipment[iter] && CAN_SEE_OBJ(ch, equipment[iter]) && MATCH_ITEM_NAME(tmp, equipment[iter])) {
			if (--(*number) == 0) {
				found = equipment[iter];
				if (pos) {
					*pos = iter;
				}
			}
		}
	}

	return found;
}


/**
* search the entire world for an object, and return a pointer -- without
* regard to visibility.
*
* @param char *name The target argument.
* @param int *number Optional: For multi-list number targeting (look 4.cart; may be NULL)
* @return obj_data *The found item, or NULL.
*/
obj_data *get_obj_world(char *name, int *number) {
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	obj_data *i;
	int num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	DL_FOREACH(object_list, i) {
		if (MATCH_ITEM_NAME(tmp, i)) {
			if (--(*number) == 0) {
				return i;
			}
		}
	}

	return NULL;
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
		LL_PREPEND(GET_OFFERS(ch), offer);
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
	struct offer_data *offer, *next_offer;
	int max_duration = config_get_int("offer_time");
	
	if (!ch || IS_NPC(ch)) {
		return;
	}
	
	for (offer = GET_OFFERS(ch); offer; offer = next_offer) {
		next_offer = offer->next;
		
		if (time(0) - offer->time > max_duration) {
			LL_DELETE(GET_OFFERS(ch), offer);
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
	struct offer_data *offer, *next_offer;
	
	if (!ch || IS_NPC(ch)) {
		return;
	}
	
	for (offer = GET_OFFERS(ch); offer; offer = next_offer) {
		next_offer = offer->next;
		
		if (offer->type == type) {
			LL_DELETE(GET_OFFERS(ch), offer);
			free(offer);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER TECH HANDLERS ////////////////////////////////////////////////////

// Simple sorter to help display player techs
int sort_player_techs(struct player_tech *a, struct player_tech *b) {
	return (a->id - b->id);
}

/**
* Adds a player tech (by ability) to the player.
*
* @param char_data *ch The player gaining a tech.
* @param any_vnum abil The ability that's granting it.
* @param int tech The PTECH_ to gain.
*/
void add_player_tech(char_data *ch, any_vnum abil, int tech) {
	struct player_tech *iter, *pt;
	bool found = FALSE;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_TECHS(ch), iter) {
		if (iter->abil == abil && iter->id == tech) {
			found = TRUE;
			break;
		}
	}
	
	// add it
	if (!found) {
		CREATE(pt, struct player_tech, 1);
		pt->id = tech;
		pt->abil = abil;
		LL_INSERT_INORDER(GET_TECHS(ch), pt, sort_player_techs);
	}
}


/**
* Whether or not a PC has the requested tech.
*
* @param char_data *ch The player.
* @param int tech Which PTECH_ to see if he/she has.
* @return bool TRUE if the player has it, FALSE otherwise.
*/
bool has_player_tech(char_data *ch, int tech) {
	struct player_tech *iter;
	struct int_hash *find;
	
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	// check inherent techs
	HASH_FIND_INT(inherent_ptech_hash, &tech, find);
	if (find) {
		return TRUE;
	}
	
	// check player techs
	LL_FOREACH(GET_TECHS(ch), iter) {
		if (iter->id == tech) {
			return TRUE;
		}
	}
	
	// not found
	return FALSE;
}


/**
* Removes player techs by ability.
*
* @param char_data *ch The player losing techs.
* @param any_vnum abil The ability whose techs are being lost.
*/
void remove_player_tech(char_data *ch, any_vnum abil) {
	struct player_tech *iter, *next;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH_SAFE(GET_TECHS(ch), iter, next) {
		if (iter->abil == abil) {
			LL_DELETE(GET_TECHS(ch), iter);
			free(iter);
		}
	}
}


/**
* Runs ability triggers on any ability that's giving a player a certain tech.
* Stops if any of those triggers blocks it.
*
* @param char_data *ch The person using the ability.
* @param int tech Which PTECH_ to trigger.
* @param char_data *cvict The target of the ability, if any.
* @param obj_data *ovict The target of the ability, if any.
* @param bool TRUE if a trigger blocked the ability, FALSE if it's safe to proceed.
*/
bool run_ability_triggers_by_player_tech(char_data *ch, int tech, char_data *cvict, obj_data *ovict) {
	struct player_tech *iter;
	
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	LL_FOREACH(GET_TECHS(ch), iter) {
		if (iter->id == tech) {
			if (ABILITY_TRIGGERS(ch, cvict, ovict, iter->abil)) {
				return TRUE;
			}
		}
	}
	
	// survived
	return FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// REQUIREMENT HANDLERS ////////////////////////////////////////////////////


/**
* @param struct req_data *from The list to copy.
* @return struct req_data* The copy of the list.
*/
struct req_data *copy_requirements(struct req_data *from) {
	struct req_data *el, *iter, *list = NULL;
	
	LL_FOREACH(from, iter) {
		CREATE(el, struct req_data, 1);
		*el = *iter;
		if (iter->custom) {
			el->custom = str_dup(iter->custom);
		}
		LL_APPEND(list, el);
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
			if (iter->custom) {
				free(iter->custom);
			}
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
		HASH_DEL(eid_list, eid);
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
			case REQ_GET_CURRENCY: {
				add_currency(ch, req->vnum, -req->needed);
				break;
			}
			case REQ_GET_COINS: {
				charge_coins(ch, REAL_OTHER_COIN, req->needed, NULL, NULL);
				break;
			}
			case REQ_CROP_VARIETY: {
				extract_crop_variety(ch, req->needed);
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
		if (iter->custom) {
			free(iter->custom);
		}
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
				if (!has_completed_quest(ch, req->vnum, instance ? INST_ID(instance) : NOTHING)) {
					ok = FALSE;
				}
				break;
			}
			case REQ_COMPLETED_QUEST_EVER: {
				if (!has_completed_quest_any(ch, req->vnum)) {
					ok = FALSE;
				}
				break;
			}
			case REQ_GET_CURRENCY: {
				if (get_currency(ch, req->vnum) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_GET_COINS: {
				if (!can_afford_coins(ch, REAL_OTHER_COIN, req->needed)) {
					ok = FALSE;
				}
				break;
			}
			case REQ_GET_COMPONENT: {
				struct resource_data *res = NULL;
				add_to_resource_list(&res, RES_COMPONENT, req->vnum, req->needed, req->misc);
				if (!has_resources(ch, res, FALSE, FALSE, NULL)) {
					ok = FALSE;
				}
				free_resource_list(res);
				break;
			}
			case REQ_GET_OBJECT: {
				struct resource_data *res = NULL;
				add_to_resource_list(&res, RES_OBJECT, req->vnum, req->needed, 0);
				if (!has_resources(ch, res, FALSE, FALSE, NULL)) {
					ok = FALSE;
				}
				free_resource_list(res);
				break;
			}
			case REQ_NOT_COMPLETED_QUEST: {
				if (has_completed_quest(ch, req->vnum, instance ? INST_ID(instance) : NOTHING)) {
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
			case REQ_OWN_BUILDING_FUNCTION: {
				if (!GET_LOYALTY(ch) || count_owned_buildings_by_function(GET_LOYALTY(ch), req->misc) < req->needed) {
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
			case REQ_OWN_VEHICLE_FLAGGED: {
				if (!GET_LOYALTY(ch) || count_owned_vehicles_by_flags(GET_LOYALTY(ch), req->misc) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_OWN_VEHICLE_FUNCTION: {
				if (!GET_LOYALTY(ch) || count_owned_vehicles_by_function(GET_LOYALTY(ch), req->misc) < req->needed) {
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
					if (!has_resources(ch, res, FALSE, FALSE, NULL)) {
						ok = FALSE;
					}
					free_resource_list(res);
				}
				
				break;
			}
			case REQ_CAN_GAIN_SKILL: {
				if (!check_can_gain_skill(ch, req->vnum)) {
					ok = FALSE;
				}
				break;
			}
			case REQ_CROP_VARIETY: {
				if (count_crop_variety_in_list(ch->carrying) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_OWN_HOMES: {
				if (!GET_LOYALTY(ch) || count_owned_homes(GET_LOYALTY(ch)) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_OWN_SECTOR: {
				if (!GET_LOYALTY(ch) || count_owned_sector(GET_LOYALTY(ch), req->vnum) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_EMPIRE_WEALTH: {
				if (!GET_LOYALTY(ch) || GET_TOTAL_WEALTH(GET_LOYALTY(ch)) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_EMPIRE_FAME: {
				if (!GET_LOYALTY(ch) || EMPIRE_FAME(GET_LOYALTY(ch)) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_EMPIRE_MILITARY: {
				if (!GET_LOYALTY(ch) || EMPIRE_MILITARY(GET_LOYALTY(ch)) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_EMPIRE_GREATNESS: {
				if (!GET_LOYALTY(ch) || EMPIRE_GREATNESS(GET_LOYALTY(ch)) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_DIPLOMACY: {
				if (!GET_LOYALTY(ch) || count_diplomacy(GET_LOYALTY(ch), req->misc) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_HAVE_CITY: {
				if (!GET_LOYALTY(ch) || count_cities(GET_LOYALTY(ch)) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_EMPIRE_PRODUCED_OBJECT: {
				if (!GET_LOYALTY(ch) || get_production_total(GET_LOYALTY(ch), req->vnum) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_EMPIRE_PRODUCED_COMPONENT: {
				if (!GET_LOYALTY(ch) || get_production_total_component(GET_LOYALTY(ch), req->vnum) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_EVENT_RUNNING: {
				if (!find_running_event_by_vnum(req->vnum)) {
					ok = FALSE;
				}
				break;
			}
			case REQ_EVENT_NOT_RUNNING: {
				if (find_running_event_by_vnum(req->vnum)) {
					ok = FALSE;
				}
				break;
			}
			case REQ_LEVEL_UNDER: {
				if (get_approximate_level(ch) > req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_LEVEL_OVER: {
				if (get_approximate_level(ch) < req->needed) {
					ok = FALSE;
				}
				break;
			}
			case REQ_SPEAK_LANGUAGE: {
				ok = (speaks_language(ch, req->vnum) == LANG_SPEAK);
				break;
			}
			case REQ_RECOGNIZE_LANGUAGE: {
				int level = speaks_language(ch, req->vnum);
				ok = (level == LANG_RECOGNIZE || level == LANG_SPEAK);
				break;
			}
			case REQ_DAYTIME: {
				ok = (IN_ROOM(ch) && get_sun_status(IN_ROOM(ch)) == SUN_LIGHT);
				break;
			}
			case REQ_NIGHTTIME: {
				ok = (IN_ROOM(ch) && get_sun_status(IN_ROOM(ch)) != SUN_LIGHT);
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
	
	// check if any sub-groups succeeded, if necessary (and free the mrd_list even if not)
	HASH_ITER(hh, mrd_list, mrd, next_mrd) {
		if (!global_ok && mrd->ok && mrd->group) {	// only grouped requirements count here (if we didn't already find one)
			global_ok = TRUE;
		}
		
		// free memory
		HASH_DEL(mrd_list, mrd);
		free(mrd);
	}
	
	return global_ok;
}


/**
* Gets standard string display like "4x lumber" for a requirement (e.g. a
* quest task).
*
* @param struct req_data *req The requirement to show.
* @param bool show_vnums If TRUE, adds [1234] at the start of the string.
* @param bool allow_custom If TRUE, will show a custom string isntead.
* @return char* The string display.
*/
char *requirement_string(struct req_data *req, bool show_vnums, bool allow_custom) {
	char vnum[256], lbuf[256];
	static char output[256];
	vehicle_data *vproto;
	
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
		case REQ_COMPLETED_QUEST:	// both the same
		case REQ_COMPLETED_QUEST_EVER: {
			snprintf(output, sizeof(output), "Complete quest: %s%s", vnum, get_quest_name_by_proto(req->vnum));
			break;
		}
		case REQ_GET_COMPONENT: {
			snprintf(output, sizeof(output), "Get component%s: %dx (%s)", PLURAL(req->needed), req->needed, req->needed == 1 ? get_generic_name_by_vnum(req->vnum) : get_generic_string_by_vnum(req->vnum, GENERIC_COMPONENT, GSTR_COMPONENT_PLURAL));
			break;
		}
		case REQ_GET_OBJECT: {
			snprintf(output, sizeof(output), "Get object%s: %dx %s%s", PLURAL(req->needed), req->needed, vnum, get_obj_name_by_proto(req->vnum));
			break;
		}
		case REQ_GET_CURRENCY: {
			snprintf(output, sizeof(output), "Get currency: %d %s%s", req->needed, vnum, get_generic_string_by_vnum(req->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(req->needed)));
			break;
		}
		case REQ_GET_COINS: {
			snprintf(output, sizeof(output), "Get coins: %d coins", req->needed);
			break;
		}
		case REQ_KILL_MOB: {
			snprintf(output, sizeof(output), "Kill %dx mob%s: %s%s", req->needed, PLURAL(req->needed), vnum, get_mob_name_by_proto(req->vnum, TRUE));
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
		case REQ_OWN_BUILDING_FUNCTION: {
			sprintbit(req->misc, function_flags, lbuf, TRUE);
			// does not show vnum
			snprintf(output, sizeof(output), "Own %dx building%s with: %s", req->needed, PLURAL(req->needed), lbuf);
			break;
		}
		case REQ_OWN_VEHICLE: {
			vproto = vehicle_proto(req->vnum);
			snprintf(output, sizeof(output), "Own %dx %s%s: %s%s", req->needed, vproto ? VEH_OR_BLD(vproto) : "vehicle", PLURAL(req->needed), vnum, vproto ? VEH_SHORT_DESC(vproto) : "unknown");
			break;
		}
		case REQ_OWN_VEHICLE_FLAGGED: {
			sprintbit(req->misc, vehicle_flags, lbuf, TRUE);
			// does not show vnum
			snprintf(output, sizeof(output), "Own %dx vehicle%s flagged: %s", req->needed, PLURAL(req->needed), lbuf);
			break;
		}
		case REQ_OWN_VEHICLE_FUNCTION: {
			sprintbit(req->misc, function_flags, lbuf, TRUE);
			// does not show vnum
			snprintf(output, sizeof(output), "Own %dx vehicle%s with: %s", req->needed, PLURAL(req->needed), lbuf);
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
			snprintf(output, sizeof(output), "Scripted condition %dx", req->needed);
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
		case REQ_CAN_GAIN_SKILL: {
			snprintf(output, sizeof(output), "Able to gain skill: %s%s", vnum, get_skill_name_by_vnum(req->vnum));
			break;
		}
		case REQ_CROP_VARIETY: {
			snprintf(output, sizeof(output), "Have produce from %d%s crop%s", req->needed, req->needed > 1 ? " different" : "", PLURAL(req->needed));
			break;
		}
		case REQ_OWN_HOMES: {
			snprintf(output, sizeof(output), "Own %dx home%s for citizens", req->needed, PLURAL(req->needed));
			break;
		}
		case REQ_OWN_SECTOR: {
			sector_data *sect = sector_proto(req->vnum);
			snprintf(output, sizeof(output), "Own %dx tile%s of: %s%s", req->needed, PLURAL(req->needed), vnum, sect ? GET_SECT_NAME(sect) : "UNKNOWN");
			break;
		}
		case REQ_EMPIRE_WEALTH: {
			snprintf(output, sizeof(output), "Have empire wealth over: %d", req->needed);
			break;
		}
		case REQ_EMPIRE_FAME: {
			snprintf(output, sizeof(output), "Have empire fame over: %d", req->needed);
			break;
		}
		case REQ_EMPIRE_MILITARY: {
			snprintf(output, sizeof(output), "Have empire military over: %d", req->needed);
			break;
		}
		case REQ_EMPIRE_GREATNESS: {
			snprintf(output, sizeof(output), "Have empire greatness over: %d", req->needed);
			break;
		}
		case REQ_DIPLOMACY: {
			sprintbit(req->misc, diplomacy_flags, lbuf, TRUE);
			if (lbuf[strlen(lbuf)-1] == ' ') {
				lbuf[strlen(lbuf)-1] = '\0';	// strip training space
			}
			snprintf(output, sizeof(output), "Have diplomatic relations: %dx %s", req->needed, lbuf);
			break;
		}
		case REQ_HAVE_CITY: {
			snprintf(output, sizeof(output), "Have %d cit%s", req->needed, req->needed == 1 ? "y" : "ies");
			break;
		}
		case REQ_EMPIRE_PRODUCED_OBJECT: {
			snprintf(output, sizeof(output), "Empire has produced: %dx %s%s", req->needed, vnum, get_obj_name_by_proto(req->vnum));
			break;
		}
		case REQ_EMPIRE_PRODUCED_COMPONENT: {
			snprintf(output, sizeof(output), "Empire has produced: %dx (%s)", req->needed, req->needed == 1 ? get_generic_name_by_vnum(req->vnum) : get_generic_string_by_vnum(req->vnum, GENERIC_COMPONENT, GSTR_COMPONENT_PLURAL));
			break;
		}
		case REQ_EVENT_RUNNING: {
			snprintf(output, sizeof(output), "Event is running: %s%s", vnum, get_event_name_by_proto(req->vnum));
			break;
		}
		case REQ_EVENT_NOT_RUNNING: {
			snprintf(output, sizeof(output), "Event is not running: %s%s", vnum, get_event_name_by_proto(req->vnum));
			break;
		}
		case REQ_LEVEL_UNDER: {
			snprintf(output, sizeof(output), "Level under %d", req->needed);
			break;
		}
		case REQ_LEVEL_OVER: {
			snprintf(output, sizeof(output), "Level over %d", req->needed);
			break;
		}
		case REQ_SPEAK_LANGUAGE: {
			snprintf(output, sizeof(output), "Able to speak %s%s", vnum, get_generic_name_by_vnum(req->vnum));
			break;
		}
		case REQ_RECOGNIZE_LANGUAGE: {
			snprintf(output, sizeof(output), "Able to recognize or speak %s%s", vnum, get_generic_name_by_vnum(req->vnum));
			break;
		}
		case REQ_DAYTIME: {
			snprintf(output, sizeof(output), "Daytime");
			break;
		}
		case REQ_NIGHTTIME: {
			snprintf(output, sizeof(output), "Nighttime");
			break;
		}
		default: {
			sprintf(output, "Unknown condition");
			break;
		}
	}
	
	// override with custom?
	if (allow_custom && req->custom && *req->custom) {
		snprintf(output, sizeof(output), "%s", req->custom);
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
	// shortcut: oceans are undepletable
	if (SHARED_DATA(room) == &ocean_shared_data) {
		return;
	}
	perform_add_depletion(&ROOM_DEPLETION(room), type, multiple);
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
}


/**
* Clears all depletions on the room, e.g. when the terrain changes.
*
* @param room_data *room Which room.
*/
void clear_depletions(room_data *room) {
	struct depletion_data *dep, *next_dep;
	
	if (room && ROOM_DEPLETION(room)) {
		LL_FOREACH_SAFE(ROOM_DEPLETION(room), dep, next_dep) {
			LL_DELETE(ROOM_DEPLETION(room), dep);
			free(dep);
		}
		
		request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	}
}


/**
* Fetch a depletion amount.
*
* @param struct depletion_data *list List of depletions.
* @param int type DPLTN_ Which type to get.
* @return int The depletion counter on that resource in that room.
*/
int get_depletion_amount(struct depletion_data *list, int type) {
	struct depletion_data *dep;
	
	LL_FOREACH(list, dep) {
		if (dep->type == type) {
			return dep->count;
		}
	}
	
	return 0;
}



/**
* Add to a depletion counter.
*
* @param struct depletion_data **list The set of depletions to add to.
* @param int type DPLTN_
* @param bool multiple if TRUE, chance to add more than 1
*/
void perform_add_depletion(struct depletion_data **list, int type, bool multiple) {
	struct depletion_data *dep;
	bool found = FALSE;
	
	if (!list) {
		return;
	}
	
	// find existing
	LL_FOREACH(*list, dep) {
		if (dep->type == type) {
			dep->count += 1 + ((multiple && !number(0, 3)) ? 1 : 0);
			found = TRUE;
			break;
		}
	}
	
	if (!found) {
		CREATE(dep, struct depletion_data, 1);
		dep->type = type;
		dep->count = 1 + ((multiple && !number(0, 3)) ? 1 : 0);;
		LL_PREPEND(*list, dep);
	}
}



/**
* Removes all depletion data for a certain type from the room.
*
* @param room_data *room where
* @param int type DPLTN_
* @return bool TRUE if it removed one.
*/
bool remove_depletion_from_list(struct depletion_data **list, int type) {
	struct depletion_data *dep, *next_dep;
	bool any = FALSE;
	
	LL_FOREACH_SAFE(*list, dep, next_dep) {
		if (dep->type == type) {
			LL_DELETE(*list, dep);
			free(dep);
			any = TRUE;
		}
	}
	
	return any;
}


/**
* Removes all depletion data for a certain type from the room.
*
* @param room_data *room where
* @param int type DPLTN_
*/
void remove_depletion(room_data *room, int type) {
	if (remove_depletion_from_list(&ROOM_DEPLETION(room), type)) {
		request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	}
}


/**
* Sets a depletion to a specific value.
*
* @param struct depletion_data **list A pointer to the depletion list (ROOM_DEPLETION, etc).
* @param int type DPLTN_ const
* @param int value How much to set the depletion to.
*/
void set_depletion(struct depletion_data **list, int type, int value) {
	struct depletion_data *dep;
	bool found = FALSE;
	
	// safety first
	if (!list) {
		return;
	}
	
	// shortcut
	if (value <= 0) {
		remove_depletion_from_list(list, type);
		return;
	}
	
	// existing?
	LL_FOREACH(*list, dep) {
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
		
		LL_PREPEND(*list, dep);
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
	
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
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
	
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
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
			update_script_types(SCRIPT(room));
		}
		check_extract_script(room, WLD_TRIGGER);
	}
	
	affect_total_room(room);
}


/**
* Recounts and sets the number of lights in a room: ROOM_LIGHTS(room)
*
* @param room_data *room The room to check for lights again.
*/
void reset_light_count(room_data *room) {
	vehicle_data *veh;
	obj_data *obj;
	char_data *ch;
	// int pos;
	
	ROOM_LIGHTS(room) = 0;
	
	// building
	if (IS_ANY_BUILDING(room) && (ROOM_OWNER(room) || ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE))) {
		++ROOM_LIGHTS(room);
	}
	
	// lighted vehicle-type buildings
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (VEH_PROVIDES_LIGHT(veh)) {
			++ROOM_LIGHTS(room);
		}
	}
	
	// people
	DL_FOREACH2(ROOM_PEOPLE(room), ch, next_in_room) {
		ROOM_LIGHTS(room) += GET_LIGHTS(ch);
		/*
		if (AFF_FLAGGED(ch, AFF_LIGHT)) {
			++ROOM_LIGHTS(room);
		}
		for (pos = 0; pos < NUM_WEARS; ++pos) {
			if (GET_EQ(ch, pos) && LIGHT_IS_LIT(GET_EQ(ch, pos))) {
				++ROOM_LIGHTS(room);
			}
		}
		DL_FOREACH2(ch->carrying, obj, next_content) {
			if (LIGHT_IS_LIT(obj)) {
				++ROOM_LIGHTS(room);
			}
		}
		*/
	}
	
	// objects
	DL_FOREACH2(ROOM_CONTENTS(room), obj, next_content) {
		if (LIGHT_IS_LIT(obj)) {
			++ROOM_LIGHTS(room);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ROOM EXTRA HANDLERS /////////////////////////////////////////////////////

/**
* Adds to (or creates) a room extra data value.
*
* @param room_extra_data **list The extra data list to modify.
* @param int type The ROOM_EXTRA_ type to update.
* @param int add_value The amount to add (or subtract) to the value.
*/
void add_to_extra_data(struct room_extra_data **list, int type, int add_value) {
	struct room_extra_data *red;
	
	if ((red = find_extra_data(*list, type))) {
		SAFE_ADD(red->value, add_value, INT_MIN, INT_MAX, TRUE);
		
		// delete zeroes for cleanliness
		if (red->value == 0) {
			remove_extra_data(list, type);
		}
	}
	else {
		set_extra_data(list, type, add_value);
	}
}


/**
* Finds an extra data ptr by type.
*
* @param struct room_extra_data *list The list of extra data to check.
* @param int type Any ROOM_EXTRA_ type.
* @return struct room_extra_data* The matching entry, or NULL.
*/
struct room_extra_data *find_extra_data(struct room_extra_data *list, int type) {
	struct room_extra_data *red;
	HASH_FIND_INT(list, &type, red);
	return red;
}


/**
* Frees a list of extra data.
*
* @param struct room_extra_data **hash Pointer to the hash to free.
*/
void free_extra_data(struct room_extra_data **hash) {
	struct room_extra_data *red, *next;
	HASH_ITER(hh, *hash, red, next) {
		HASH_DEL(*hash, red);
		free(red);
	}
}


/**
* Gets the value of an extra data type; defaults to 0 if none is set.
*
* @param struct room_extra_data *list The list to get data from.
* @param int type The ROOM_EXTRA_ type to check.
* @return int The value of that type (default: 0).
*/
int get_extra_data(struct room_extra_data *list, int type) {
	struct room_extra_data *red = find_extra_data(list, type);
	return (red ? red->value : 0);
}

/**
* Multiplies an existing extra data value by a number.
*
* @param struct room_extra_data **list The list to multiple an entry in.
* @param int type The ROOM_EXTRA_ type to update.
* @param double multiplier How much to multiply the value by.
*/
void multiply_extra_data(struct room_extra_data **list, int type, double multiplier) {
	struct room_extra_data *red;
	
	if ((red = find_extra_data(*list, type))) {
		red->value = (int) (multiplier * red->value);
		
		// delete zeroes for cleanliness
		if (red->value == 0) {
			remove_extra_data(list, type);
		}
	}
	// does nothing if it doesn't exist; 0*X=0
}


/**
* Removes any extra data of a given type from the list.
*
* @param struct room_extra_data **list The list to remove from.
* @param int type The ROOM_EXTRA_ type to remove.
*/
void remove_extra_data(struct room_extra_data **list, int type) {
	struct room_extra_data *red = find_extra_data(*list, type);
	if (red) {
		HASH_DEL(*list, red);
		free(red);
	}
}


/**
* Sets an extra data value to a specific number, overriding any old value.
*
* @param struct room_extra_data **list The list to set data in.
* @param int type Any ROOM_EXTRA_ type.
* @param int value The value to set it to.
*/
void set_extra_data(struct room_extra_data **list, int type, int value) {
	struct room_extra_data *red = find_extra_data(*list, type);
	
	// create if needed
	if (!red) {
		CREATE(red, struct room_extra_data, 1);
		red->type = type;
		HASH_ADD_INT(*list, type, red);
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
	struct instance_data *inst;
	room_vnum tmp;
	room_data *location = NULL;
	vehicle_data *target_veh;
	char_data *target_mob;
	obj_data *target_obj;
	char roomstr[MAX_INPUT_LENGTH], *tmpstr;
	int x, y, number;
	char *srch;

	// we may modify it as we go
	skip_spaces(&rawroomstr);
	strcpy(roomstr, rawroomstr);
	tmpstr = roomstr;
	number = get_number(&tmpstr);

	if (!*roomstr) {
		if (ch) {
			send_to_char("You must supply a room number or name.\r\n", ch);
		}
   		location = NULL;
	}
	else if (*tmpstr == UID_CHAR) {
		// maybe
		location = find_room(atoi(tmpstr + 1));
		
		if (!location && (target_mob = get_char(tmpstr))) {
			location = IN_ROOM(target_mob);
		}
		if (!location && (target_veh = get_vehicle(tmpstr))) {
			location = IN_ROOM(target_veh);
		}
		if (!location && (target_obj = get_obj(tmpstr))) {
			location = obj_room(target_obj);
		}
	}
	else if (*tmpstr == 'i' && isdigit(*(tmpstr+1)) && ch && IS_NPC(ch) && (inst = real_instance(MOB_INSTANCE_ID(ch))) != NULL) {
		// find room in instance by template vnum
		location = find_room_template_in_instance(inst, atoi(tmpstr+1));
	}
	else if (isdigit(*tmpstr) && (srch = strchr(tmpstr, ','))) {
		// coords
		*(srch++) = '\0';
		skip_spaces(&srch);
		x = atoi(tmpstr);
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
	else if (isdigit(*tmpstr) && !strchr(tmpstr, '.')) {
		tmp = atoi(tmpstr);
		if (!(location = real_room(tmp)) && ch) {
			send_to_char("No room exists with that number.\r\n", ch);
		}
	}
	else if (ch && (target_mob = get_char_vis(ch, tmpstr, &number, FIND_CHAR_WORLD)) != NULL) {
		if (WIZHIDE_OK(ch, target_mob)) {
			location = IN_ROOM(target_mob);
		}
		else {
			msg_to_char(ch, "That person is not available.\r\n");
		}
	}
	else if (!ch && (target_mob = get_char_world(tmpstr, &number)) != NULL) {
		location = IN_ROOM(target_mob);
	}
	else if ((ch && (target_veh = get_vehicle_vis(ch, tmpstr, &number))) || (!ch && (target_veh = get_vehicle_world(tmpstr, &number)))) {
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
	else if ((ch && (target_obj = get_obj_vis(ch, tmpstr, &number)) != NULL) || (!ch && (target_obj = get_obj_world(tmpstr, &number)) != NULL)) {
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


/**
* This function finds a target room from coordinates, if the string contains
* ONLY those coordinates. It will accept a variety of coordinate formats but
* will fail if ANYTHING other than the coordinates (and possible parentheses/
* quotes) is in the string.
*
* @param char *string The input string.
* @return room_data* The target room if the string contained only coordinates.
*/
room_data *parse_room_from_coords(char *string) {
	char copy[MAX_INPUT_LENGTH], word[MAX_INPUT_LENGTH];
	room_data *room = NULL;
	char *ptr, *srch;
	int x, y;
	
	strcpy(copy, string);
	ptr = copy;
	
	skip_spaces(&ptr);
	if (*ptr == '(' || *ptr == '"') {
		ptr = any_one_word(ptr, word);
		skip_spaces(&ptr);
		if (*ptr) {
			// invalid: there was something (other than spaces) AFTER the coords
			return NULL;
		}
		
		// if we got this far it's possibly coords; point ptr at it
		ptr = word;
	}
	else {
		// no parens: point to whole string
		ptr = copy;
	}
	skip_spaces(&ptr);
	
	// ptr is the start of possible coords
	if (isdigit(*ptr) && (srch = strchr(ptr, ','))) {
		// coords
		*(srch++) = '\0';
		skip_spaces(&srch);
		x = atoi(ptr);
		y = atoi(srch);
		if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
			// success
			room = real_room((y * MAP_WIDTH) + x);
		}
		
		// ensure nothing was AFTER srch
		while (isdigit(*srch)) {
			++srch;
		}
		skip_spaces(&srch);
		
		if (*srch) {
			// found a non-space character after the coords
			room = NULL;
		}
	}
	
	return room;	// if any
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
* @param int type The EVO_ type to get.
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
* @param int type The EVO_ type to check.
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
* @param int evo_type Any EVO_ const.
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


 //////////////////////////////////////////////////////////////////////////////
//// STORAGE HANDLERS ////////////////////////////////////////////////////////

/**
* Adds to empire storage by vnum
*
* @param empire_data *emp The empire vnum
* @param int island Which island to store it on
* @param obj_vnum vnum Any object to store.
* @param int amount How much to add
* @return struct empire_storage_data* Returns a pointer to the storage entry.
*/
struct empire_storage_data *add_to_empire_storage(empire_data *emp, int island, obj_vnum vnum, int amount) {
	struct empire_storage_data *store = find_stored_resource(emp, island, vnum);
	struct empire_island *isle = get_empire_island(emp, island);
	
	if (!isle || !amount) {
		return NULL;	// nothing to do
	}
	if (amount < 0 && !store) {
		return NULL;	// nothing to take
	}
	
	if (!store) {	// create storage
		CREATE(store, struct empire_storage_data, 1);
		store->vnum = vnum;
		store->proto = obj_proto(vnum);
		store->keep = EMPIRE_ATTRIBUTE(emp, EATT_DEFAULT_KEEP);
		HASH_ADD_INT(isle->store, vnum, store);
	}
	
	SAFE_ADD(store->amount, amount, 0, MAX_STORAGE, FALSE);
	store->amount = MAX(store->amount, 0);
	
	if (store->amount == 0 && !store->keep) {
		HASH_DEL(isle->store, store);
		free(store);
		store = NULL;
	}
	
	isle->store_is_sorted = FALSE;
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	
	et_get_obj(emp, obj_proto(vnum), amount, store ? store->amount : 0);
	return store;
}


/**
* removes X stored components from an empire (will also accept sub-types/
* related components)
*
* @param empire_data *emp
* @param int island Which island to charge for storage, or ANY_ISLAND to take from any available storage
* @param any_vnum cmp_vnum The vnum of the component to charge (will also accept sub-types of this).
* @param int amount How much to charge*
* @param bool use_kept If TRUE, will use items with the 'keep' flag instead of ignorning them
* @param bool basic_only If TRUE, will only use basic components.
* @param struct resource_data **build_used_list Optional: A place to store the exact item used, e.g. for later dismantling. (NULL if none)
* @return bool TRUE if it was able to charge enough, FALSE if not
*/
bool charge_stored_component(empire_data *emp, int island, any_vnum cmp_vnum, int amount, bool use_kept, bool basic_only, struct resource_data **build_used_list) {
	struct empire_storage_data *store, *next_store;
	generic_data *cmp = real_generic(cmp_vnum), *gen;
	struct empire_island *isle, *next_isle;
	int this, can_take, found = 0;
	obj_data *proto;
	
	if (amount < 0) {
		return TRUE;	// can't charge a negative amount
	}
	if (!cmp) {
		return FALSE;	// no valid component to charge
	}
	
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		if (found >= amount) {
			break;	// done early
		}
		if (island != ANY_ISLAND && island != isle->island) {
			continue;	// wrong island
		}
		
		HASH_ITER(hh, isle->store, store, next_store) {
			if (store->amount < 1) {
				continue;
			}
			if ((store->keep == UNLIMITED || store->amount <= store->keep) && !use_kept) {
				continue;
			}
			
			// need obj
			if (!(proto = store->proto)) {
				continue;
			}
			if (basic_only && cmp_vnum != GET_OBJ_COMPONENT(proto) && (!(gen = real_generic(GET_OBJ_COMPONENT(proto))) || !GEN_FLAGGED(gen, GEN_BASIC))) {
				continue;	// must be basic (or exact match)
			}
		
			// matching component?
			if (GET_OBJ_COMPONENT(proto) == NOTHING || !is_component(proto, cmp)) {
				continue;
			}
		
			// ok make it so
			can_take = (store->keep > 0 ? store->amount - store->keep : store->amount);	// because we already know either keep is not unlimited, or we can ignore keep if it is
			this = MIN(amount, can_take);
			found += this;
			
			if (build_used_list) {
				add_to_resource_list(build_used_list, RES_OBJECT, store->vnum, this, 0);
			}
			
			// remove it from the island AFTER being done with store->vnum
			add_to_empire_storage(emp, isle->island, store->vnum, -this);
		
			// done?
			if (found >= amount) {
				break;
			}
		}
	}
	
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
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
	struct empire_island *isle, *next_isle;
	struct empire_storage_data *store;
	int this;
	
	if (amount < 0) {
		return TRUE;	// can't charge a negative amount
	}
	
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		if (amount <= 0) {
			break;	// done early
		}
		if (island != ANY_ISLAND && island != isle->island) {
			continue;	// wrong island
		}
		
		HASH_FIND_INT(isle->store, &vnum, store);
		if (!store || store->amount < 1) {
			continue;	// none here
		}
		
		this = MIN(amount, store->amount);
		amount -= this;
		
		add_to_empire_storage(emp, isle->island, vnum, -this);
	}
	
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
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
	struct empire_island *isle, *next_isle;
	struct empire_storage_data *sto;
	int deleted = 0;
	
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		HASH_FIND_INT(isle->store, &vnum, sto);
		if (sto) {
			SAFE_ADD(deleted, sto->amount, 0, MAX_INT, FALSE);
			HASH_DEL(isle->store, sto);
			free(sto);
		}
	}
	
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	return (deleted > 0);
}


/**
* This finds a matching item's empire_storage_data object for a component type,
* IF there is any match stored to the empire on that island.
*
* @param empire_data *emp The empire.
* @param int island Which island to search.
* @param any_vnum cmp_vnum The generic component to check for (also allows subtypes).
* @param int amount The number that must be available.
* @param bool include_kept If TRUE, ignores the 'keep' flag and will use kept items.
* @param bool basic_only If TRUE, skips non-basic components.
*/
bool empire_can_afford_component(empire_data *emp, int island, any_vnum cmp_vnum, int amount, bool include_kept, bool basic_only) {
	struct empire_storage_data *store, *next_store;
	generic_data *cmp = real_generic(cmp_vnum), *gen;
	struct empire_island *isle;
	obj_data *proto;
	int amt, found = 0;
	
	if (!cmp || island == NO_ISLAND || !(isle = get_empire_island(emp, island))) {
		return FALSE;	// shortcut out
	}
	
	HASH_ITER(hh, isle->store, store, next_store) {
		if (store->amount < 1) {
			continue;	// got none
		}
		if ((store->keep == UNLIMITED || store->amount <= store->keep) && !include_kept) {
			continue;
		}
		
		if (!(proto = store->proto)) {
			continue;	// need obj
		}
		if (basic_only && cmp_vnum != GET_OBJ_COMPONENT(proto) && (!(gen = real_generic(GET_OBJ_COMPONENT(proto))) || !GEN_FLAGGED(gen, GEN_BASIC))) {
			continue;	// must be basic (or exact match)
		}
		
		// is it a match, though?
		if (GET_OBJ_COMPONENT(proto) != NOTHING && is_component(proto, cmp)) {
			amt = (store->keep > 0 ? store->amount - store->keep : store->amount);
			SAFE_ADD(found, amt, 0, MAX_INT, FALSE);
			if (found >= amount) {
				break;
			}
		}
	}
	
	return (found >= amount);
}


/**
* Finds empire storage on a given island by item keyword. Note this can find
* one with an amount of 0 (because keep amounts are saved).
* 
* @param empire_data *emp The empire whose storage to search.
* @param int island_id Which island to look on.
* @param char *keywords The keyword(s) to match using multi_isname().
* @return struct empire_storage_data* The storage entry, or NULL if no matches.
*/
struct empire_storage_data *find_island_storage_by_keywords(empire_data *emp, int island_id, char *keywords) {
	struct empire_storage_data *store, *next_store;
	struct empire_island *isle = get_empire_island(emp, island_id);
	obj_data *proto;
	
	HASH_ITER(hh, isle->store, store, next_store) {
		if (!(proto = store->proto)) {
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
* This finds the empire_storage_data object for a given vnum in an empire,
* IF there is any of that vnum stored to the empire. Note that the resource
* MAY have an amount of 0 (empty entries are retained for 'keep' info).
*
* @param empire_data *emp The empire.
* @param int island Which island to search.
* @param obj_vnum vnum The object vnum to check for.
* @return struct empire_storage_data* A pointer to the storage object for the empire, if any (otherwise NULL).
*/
struct empire_storage_data *find_stored_resource(empire_data *emp, int island, obj_vnum vnum) {
	struct empire_storage_data *store = NULL;
	struct empire_island *isle;
	
	if ((isle = get_empire_island(emp, island))) {
		HASH_FIND_INT(isle->store, &vnum, store);
	}
	
	return store;
}


/**
* Counts the total number of something an empire has in all storage.
*
* @param empire_data *emp The empire to check.
* @param obj_vnum vnum The item to look for.
* @param bool count_secondary If TRUE, also count items in the shipping system and items on the ground.
* @return int The total number the empire has stored.
*/
int get_total_stored_count(empire_data *emp, obj_vnum vnum, bool count_secondary) {
	struct empire_island *isle, *next_isle;
	struct empire_storage_data *sto;
	struct shipping_data *shipd;
	int count = 0;
	
	if (!emp) {
		return count;
	}
	
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		HASH_FIND_INT(isle->store, &vnum, sto);
		if (sto && sto->amount > 0) {
			SAFE_ADD(count, sto->amount, 0, INT_MAX, FALSE);
		}
	}
	
	if (count_secondary) {
		DL_FOREACH(EMPIRE_SHIPPING_LIST(emp), shipd) {
			if (shipd->vnum == vnum) {
				SAFE_ADD(count, shipd->amount, 0, INT_MAX, FALSE);
			}
		}
		
		SAFE_ADD(count, count_dropped_items(emp, vnum), 0, INT_MAX, FALSE);
	}
	
	return count;
}


/**
* @param obj_data *obj The item to check
* @param room_data *loc The world location you want to store it in
* @param empire_data *by_emp Optional: Check if the empire would have permission to do it here. (Pass NULL to skip this check entirely, e.g. when stealing.)
* @param retrieval_mode bool TRUE if we're retrieving, FALSE if we're storing
* @return bool TRUE if the object can be stored here; otherwise FALSE
*/
bool obj_can_be_stored(obj_data *obj, room_data *loc, empire_data *by_emp, bool retrieval_mode) {
	struct obj_storage_type *store;
	bld_data *bld = GET_BUILDING(loc);
	vehicle_data *veh;
	
	bool use_room = (!by_emp || emp_can_use_room(by_emp, loc, GUESTS_ALLOWED));
	bool bld_ok = use_room && (!by_emp || !ROOM_OWNER(loc) || by_emp == ROOM_OWNER(loc) || has_relationship(by_emp, ROOM_OWNER(loc), DIPL_TRADE));
	
	// We skip this check in retrieval mode, since STORE_ALL does not function for retrieval.
	if (!retrieval_mode && GET_OBJ_STORAGE(obj) && room_has_function_and_city_ok(NULL, loc, FNC_STORE_ALL)) {
		return TRUE; // As long as it can be stored anywhere, it can be stored here.
	}
	
	// TYPE_x: storage locations
	LL_FOREACH(GET_OBJ_STORAGE(obj), store) {
		if (bld && bld_ok) {
			if (store->type == TYPE_BLD && (store->vnum == GET_BLD_VNUM(bld) || bld_has_relation(bld, BLD_REL_STORES_LIKE_BLD, store->vnum))) {
				return TRUE;
			}
			else if (store->type == TYPE_VEH && bld_has_relation(bld, BLD_REL_STORES_LIKE_VEH, store->vnum)) {
				return TRUE;
			}
		}
		
		// check in-veh: storage doesn't work from here
		/*
		if ((veh = GET_ROOM_VEHICLE(loc)) && (!by_emp || !VEH_OWNER(veh) || by_emp == VEH_OWNER(veh) || (emp_can_use_vehicle(by_emp, veh, GUESTS_ALLOWED) && has_relationship(by_emp, VEH_OWNER(veh), DIPL_TRADE)))) {
			if (store->type == TYPE_VEH && (store->vnum == VEH_VNUM(veh) || veh_has_relation(veh, BLD_REL_STORES_LIKE_VEH, store->vnum))) {
				return TRUE;
			}
			else if (store->type == TYPE_BLD && veh_has_relation(veh, BLD_REL_STORES_LIKE_BLD, store->vnum)) {
				return TRUE;
			}
		}
		*/
		
		// vehicles in room
		DL_FOREACH2(ROOM_VEHICLES(loc), veh, next_in_room) {
			if (!VEH_IS_COMPLETE(veh) || VEH_FLAGGED(veh, VEH_ON_FIRE)) {
				continue;	// incomplete or on fire
			}
			if (by_emp && VEH_OWNER(veh) && by_emp != VEH_OWNER(veh) && (!emp_can_use_vehicle(by_emp, veh, GUESTS_ALLOWED) || !has_relationship(by_emp, VEH_OWNER(veh), DIPL_TRADE))) {
				continue;	// no permission for veh
			}
			if (by_emp && !VEH_OWNER(veh) && ROOM_OWNER(loc) && by_emp != ROOM_OWNER(loc) && (!use_room || !has_relationship(by_emp, ROOM_OWNER(loc), DIPL_TRADE))) {
				continue;	// unowned vehicles inherit room permission
			}
			
			if (store->type == TYPE_VEH && (store->vnum == VEH_VNUM(veh) || veh_has_relation(veh, BLD_REL_STORES_LIKE_VEH, store->vnum))) {
				return TRUE;
			}
			else if (store->type == TYPE_BLD && veh_has_relation(veh, BLD_REL_STORES_LIKE_BLD, store->vnum)) {
				return TRUE;
			}
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
	struct empire_storage_data *store, *next_store;
	struct empire_island *isle, *next_isle;
	int old = EMPIRE_WEALTH(emp);
	obj_data *proto;
	
	EMPIRE_WEALTH(emp) = 0;
	
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		HASH_ITER(hh, isle->store, store, next_store) {
			if (store->amount > 0 && (proto = store->proto)) {
				if (IS_WEALTH_ITEM(proto)) {
					SAFE_ADD(EMPIRE_WEALTH(emp), (GET_WEALTH_VALUE(proto) * store->amount), 0, INT_MAX, FALSE);
				}
			}
		}
	}
	
	if (old != EMPIRE_WEALTH(emp)) {
		et_change_coins(emp, 0);	// will trigger wealth-based goals to reread
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
	obj_data *obj, *proto;
	bool room = FALSE;
	int available;

	proto = store->proto;
	
	// somehow
	if (!proto) {
		return FALSE;
	}

	if (!CAN_CARRY_OBJ(ch, proto)) {
		stack_msg_to_desc(ch->desc, "Your arms are full.\r\n");
		return FALSE;
	}

	obj = read_object(store->vnum, TRUE);
	available = store->amount - 1;	// for later
	charge_stored_resource(emp, GET_ISLAND_ID(IN_ROOM(ch)), store->vnum, 1);
	scale_item_to_level(obj, 1);	// scale to its minimum
	
	if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
		obj_to_char(obj, ch);
	}
	else {
		obj_to_room(obj, IN_ROOM(ch));
		room = TRUE;
	}
	act("You retrieve $p.", FALSE, ch, obj, 0, TO_CHAR | TO_QUEUE);
	act("$n retrieves $p.", TRUE, ch, obj, 0, TO_ROOM | TO_QUEUE);
	
	if (stolen) {
		record_theft_log(emp, GET_OBJ_VNUM(obj), 1);
		GET_STOLEN_TIMER(obj) = time(0);
		GET_STOLEN_FROM(obj) = EMPIRE_VNUM(emp);
		trigger_distrust_from_stealth(ch, emp);
		add_offense(emp, OFFENSE_STEALING, ch, IN_ROOM(ch), offense_was_seen(ch, emp, NULL) ? OFF_SEEN : NOBITS);
	}
	
	// do this after the "stolen" section, in case it'll purge the item
	if (load_otrigger(obj)) {
		get_otrigger(obj, ch, FALSE);
	}
	
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	
	// if it ran out, return false to prevent loops; same for if it went to the room (one at a time)
	return (available > 0 && !room);
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
	act("You store $p.", FALSE, ch, obj, 0, TO_CHAR | TO_QUEUE);
	act("$n stores $p.", TRUE, ch, obj, 0, TO_ROOM | TO_QUEUE);

	add_to_empire_storage(emp, GET_ISLAND_ID(IN_ROOM(ch)), GET_OBJ_VNUM(obj), 1);
	extract_obj(obj);
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	
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
		for (sdt = GET_OBJ_STORAGE(obj); sdt; sdt = sdt->next) {
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
* Checks if a player can store an item in their home.
*
* @param char_data *ch The player.
* @parma obj_data *obj The object to try to store.
* @param bool message If TRUE, sends its own error message when there's a failure.
* @param bool *capped A variable to bind to: becomes TRUE if the player has hit the cap -- useful if you're not messaging here.
* @return bool TRUE if the player can store, FALSE if they're over the limit.
*/
bool check_home_store_cap(char_data *ch, obj_data *obj, bool message, bool *capped) {
	struct empire_unique_storage *eus;
	int count;
	
	*capped = FALSE;
	
	if (!ch || !obj || IS_NPC(ch) || override_home_storage_cap) {
		if (message) {
			msg_to_char(ch, "Error trying to store that.\r\n");
		}
		return FALSE;	// sanity check
	}
	
	if (!find_eus_entry(obj, GET_HOME_STORAGE(ch), NULL)) {
		DL_COUNT(GET_HOME_STORAGE(ch), eus, count);
		if (count >= config_get_int("max_home_store_uniques")) {
			*capped = TRUE;
			if (message) {
				msg_to_char(ch, "You have already hit the %d-item limit for your home storage.\r\n", config_get_int("max_home_store_uniques"));
			}
			return FALSE;
		}
	}
	
	// appears ok
	return TRUE;
}


/**
* Remove items from the unique item storage list by vnum (e.g. if the item was
* deleted).
*
* @param struct empire_unique_storage **list A pointer to the list to remove it from.
* @param obj_vnum vnum The object vnum to remove.
* @return bool TRUE if at least 1 was deleted.
*/
bool delete_unique_storage_by_vnum(struct empire_unique_storage **list, obj_vnum vnum) {
	struct empire_unique_storage *iter, *next_iter;
	bool any = FALSE;
	
	if (!list) {
		return FALSE;
	}
	
	DL_FOREACH_SAFE(*list, iter, next_iter) {
		if (iter->obj && GET_OBJ_VNUM(iter->obj) == vnum) {
			add_to_object_list(iter->obj);
			extract_obj(iter->obj);
			iter->obj = NULL;
			DL_DELETE(*list, iter);
			free(iter);
			any = TRUE;
		}
	}
	
	return any;
}


/**
* Find a matching entry for an object in unique storage.
*
* @param obj_data *obj The item to find a matching entry for.
* @param struct empire_unique_storage **list The unique storage list to search
* @param room_data *location The room to find matching storage for (optional).
* @return struct empire_unique_storage* The storage entry, if it exists (or NULL).
*/
struct empire_unique_storage *find_eus_entry(obj_data *obj, struct empire_unique_storage *list, room_data *location) {
	struct empire_unique_storage *iter;
	
	if (!obj) {
		return NULL;
	}
	
	DL_FOREACH(list, iter) {
		if (location && GET_ISLAND_ID(location) != iter->island) {
			continue;
		}
		if (location && !IS_SET(iter->flags, EUS_VAULT) && room_has_function_and_city_ok(NULL, location, FNC_VAULT)) {
			continue;
		}
		if (location && IS_SET(iter->flags, EUS_VAULT) && !room_has_function_and_city_ok(NULL, location, FNC_VAULT)) {
			continue;
		}
		
		if (objs_are_identical(iter->obj, obj)) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* Store a unique item to an empire. The object MAY be extracted. If save_emp
* is NULL, it assumes you're saving character home storage instead of empire
* warehouse storage.
*
* @param char_data *ch Person doing the storing (may be NULL; sends messages if not).
* @param struct empire_unique_storage **to_list Where to store it to (empire or home storage).
* @param obj_data *obj The unique item to store.
* @param empire_data *save_emp The empire to save, if this is empire storage. If NULL, assumes home storage.
* @param room_data *room The location being stored (for vault flag detection).
* @param bool *full A variable to set TRUE if the storage is full and the item can't be stored.
*/
void store_unique_item(char_data *ch, struct empire_unique_storage **to_list, obj_data *obj, empire_data *save_emp, room_data *room, bool *full) {
	struct empire_unique_storage *eus;
	bool extract = FALSE;
	trig_data *trig;
	
	*full = FALSE;
	
	if (!obj || !to_list) {
		return;
	}
	
	// empty/clear first
	REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
	clear_obj_eq_sets(obj);
	LAST_OWNER_ID(obj) = NOBODY;
	obj->last_empire_id = NOTHING;
	empty_obj_before_extract(obj);
	if (IS_DRINK_CONTAINER(obj)) {
		set_obj_val(obj, VAL_DRINK_CONTAINER_CONTENTS, 0);
		set_obj_val(obj, VAL_DRINK_CONTAINER_TYPE, LIQ_WATER);
	}
	
	// SEV_x: events that must be canceled or changed when an item is stored
	cancel_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_AUTOSTORE);
	cancel_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_TIMER);
	// TODO: convert to a timer that can tick while stored
	
	// existing eus entry or new one? only passes 'room' if it's an empire; player storage is global
	if ((eus = find_eus_entry(obj, *to_list, save_emp ? room : NULL))) {
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
		DL_PREPEND(*to_list, eus);
		check_obj_in_void(obj);
		eus->obj = obj;
		eus->amount = 1;
		eus->island = (save_emp && room) ? GET_ISLAND_ID(room) : NO_ISLAND;
		if (save_emp && eus->island == NO_ISLAND) {
			eus->island = get_main_island(save_emp);
		}
		if (save_emp && room && room_has_function_and_city_ok(ch ? GET_LOYALTY(ch) : ROOM_OWNER(room), room, FNC_VAULT)) {
			eus->flags = EUS_VAULT;
		}
			
		// get it out of the object list
		remove_from_object_list(obj);
		
		// cancel running trigs and shut off random trigs
		if (SCRIPT(obj)) {
			LL_FOREACH(TRIGGERS(SCRIPT(obj)), trig) {
				remove_trigger_from_global_lists(trig, TRUE);
				
				if (GET_TRIG_WAIT(trig)) {
					dg_event_cancel(GET_TRIG_WAIT(trig), cancel_wait_event);
					GET_TRIG_WAIT(trig) = NULL;
					GET_TRIG_DEPTH(trig) = 0;
					free_varlist(GET_TRIG_VARS(trig));
					GET_TRIG_VARS(trig) = NULL;
					trig->curr_state = NULL;
				}
			}
		}
	}
	
	if (ch) {
		act("You store $p.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		act("$n stores $p.", FALSE, ch, obj, NULL, TO_ROOM | TO_QUEUE);
		queue_delayed_update(ch, CDU_SAVE);
	}
	
	if (extract) {
		extract_obj(obj);
	}
	
	if (save_emp) {
		EMPIRE_NEEDS_STORAGE_SAVE(save_emp) = TRUE;
	}
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
	char temp[MAX_INPUT_LENGTH];
	int mode;
	
	if (!str_cmp(arg, "all")) {
		mode = FIND_ALL;
	}
	else if (!strn_cmp(arg, "all.", 4)) {
		strcpy(temp, arg);
		strcpy(arg, temp + 4);	// safer to copy twice to prevent memory overlap warning
		mode = FIND_ALLDOT;
	}
	else {
		mode = FIND_INDIV;
	}
	
	return mode;
}


/* Generic Find, designed to find any object/character
 *
 * @param char *arg is the pointer containing the string to be searched for.
 *           This string doesn't have to be a single word, the routine
 *           extracts the next word itself.
 * @param int *number Optional, will pass through #.name targeting (detects it itself otherwise)
 * @param bitvector_t bitvector   All those bits that you want to "search through". Bit found will be result of the function
 * @param char_data *ch This is the person that is trying to "find" something
 * @param char_data **tar_ch Will be NULL if no character was found, otherwise points
 * @param obj_data **tar_obj Will be NULL if no object was found, otherwise points
 * @param vehicle_data **tar_veh Will be NULL if no vehicle was found, otherwise points
 * @return bitvector_t Which type was found, or NOBITS for none
 */
bitvector_t generic_find(char *arg, int *number, bitvector_t bitvector, char_data *ch, char_data **tar_ch, obj_data **tar_obj, vehicle_data **tar_veh) {
	char name[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH], *name_ptr = name, *tmp = temp;
	int i, found, num;
	bitvector_t npc_only = (bitvector & FIND_NPC_ONLY);
	
	if (tar_ch) {
		*tar_ch = NULL;
	}
	if (tar_obj) {
		*tar_obj = NULL;
	}
	if (tar_veh) {
		*tar_veh = NULL;
	}
	
	// check if a number is passed throguh
	if (!number) {
		strcpy(tmp, arg);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = arg;	// use whole arg instead
	}
	
	one_argument(tmp, name);

	if (!*name_ptr) {
		return (0);
	}
	if (*number == 0) {
		// only looking for players
		if (!npc_only && IS_SET(bitvector, FIND_CHAR_ROOM) && tar_ch && (*tar_ch = get_player_vis(ch, name_ptr, FIND_CHAR_ROOM))) {
			return FIND_CHAR_ROOM;
		}
		else if (!npc_only && IS_SET(bitvector, FIND_CHAR_WORLD) && tar_ch && (*tar_ch = get_player_vis(ch, name_ptr, FIND_CHAR_WORLD))) {
			return FIND_CHAR_WORLD;
		}
		
		// otherwise can't handle 0.name
		return (0);
	}

	if (IS_SET(bitvector, FIND_CHAR_ROOM) && tar_ch) {	/* Find person in room */
		if ((*tar_ch = get_char_vis(ch, name_ptr, number, FIND_CHAR_ROOM | npc_only)) != NULL) {
			return (FIND_CHAR_ROOM);
		}
	}
	if (IS_SET(bitvector, FIND_VEHICLE_ROOM) && tar_veh) {
		if ((*tar_veh = get_vehicle_in_room_vis(ch, name_ptr, number)) != NULL) {
			return (FIND_VEHICLE_ROOM);
		}
	}
	if (IS_SET(bitvector, FIND_CHAR_WORLD) && tar_ch) {
		if ((*tar_ch = get_char_vis(ch, name_ptr, number, FIND_CHAR_WORLD | npc_only)) != NULL) {
			return (FIND_CHAR_WORLD);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_EQUIP) && tar_obj) {
		for (found = FALSE, i = 0; i < NUM_WEARS && !found; i++) {
			if (GET_EQ(ch, i) && isname(name_ptr, GET_OBJ_KEYWORDS(GET_EQ(ch, i))) && --(*number) == 0) {
				*tar_obj = GET_EQ(ch, i);
				found = TRUE;
			}
		}
		if (found) {
			return (FIND_OBJ_EQUIP);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_INV) && tar_obj) {
		if ((*tar_obj = get_obj_in_list_vis(ch, name_ptr, number, ch->carrying)) != NULL) {
			return (FIND_OBJ_INV);
		}
	}
	if (IS_SET(bitvector, FIND_VEHICLE_INSIDE) && tar_veh) {
		if (GET_ROOM_VEHICLE(IN_ROOM(ch)) && isname(name_ptr, VEH_KEYWORDS(GET_ROOM_VEHICLE(IN_ROOM(ch)))) && --(*number) == 0) {
			*tar_veh = GET_ROOM_VEHICLE(IN_ROOM(ch));
			return (FIND_VEHICLE_INSIDE);
		}
	}
	if (IS_SET(bitvector, FIND_VEHICLE_WORLD) && tar_veh) {
		if ((*tar_veh = get_vehicle_world_vis(ch, name_ptr, number)) != NULL) {
			return (FIND_VEHICLE_WORLD);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_ROOM) && tar_obj) {
		if ((*tar_obj = get_obj_in_list_vis(ch, name_ptr, number, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL) {
			return (FIND_OBJ_ROOM);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_WORLD) && tar_obj) {
		if ((*tar_obj = get_obj_vis(ch, name_ptr, number))) {
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
* Pre-extracts a vehicle from the game. The actual extraction will happen
* slightly later in extract_pending_vehicles().
*
* @param vehicle_data *veh The vehicle to extract.
*/
void extract_vehicle(vehicle_data *veh) {
	char_data *chiter;
	
	if (!VEH_IS_EXTRACTED(veh)) {
		check_dg_owner_purged_vehicle(veh);
		set_vehicle_flags(veh, VEH_EXTRACTED);
		++veh_extractions_pending;
	}

	// check I'm not being used by someone's action
	DL_FOREACH2(player_character_list, chiter, next_plr) {
		if (GET_ACTION_VEH_TARG(chiter) == veh) {
			GET_ACTION_VEH_TARG(chiter) = NULL;
		}
	}
}


/**
* Finishes extracting a vehicle from the game.
*
* @param vehicle_data *veh The vehicle to extract and free.
*/
void extract_vehicle_final(vehicle_data *veh) {
	char_data *chiter;
	
	// safety
	check_dg_owner_purged_vehicle(veh);
	if (veh == global_next_vehicle) {
		global_next_vehicle = global_next_vehicle->next;
	}
	if (veh == next_pending_vehicle) {
		next_pending_vehicle = next_pending_vehicle->next;
	}

	// check I'm not being used by someone's action
	DL_FOREACH2(player_character_list, chiter, next_plr) {
		if (GET_ACTION_VEH_TARG(chiter) == veh) {
			GET_ACTION_VEH_TARG(chiter) = NULL;
		}
	}
	
	// delete interior
	delete_vehicle_interior(veh);
	
	// ownership stuff
	adjust_vehicle_tech(veh, FALSE);
	if (VEH_IS_COMPLETE(veh) && VEH_OWNER(veh)) {
		qt_empire_players_vehicle(VEH_OWNER(veh), qt_lose_vehicle, veh);
		et_lose_vehicle(VEH_OWNER(veh), veh);
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
	empty_vehicle(veh, NULL);
	
	// remove animals: doing this without the vehicle in a room will not spawn the mobs
	while (VEH_ANIMALS(veh)) {
		unharness_mob_from_vehicle(VEH_ANIMALS(veh), veh);
	}
	
	// ensure it's (probably) in the list first
	if (vehicle_list && (vehicle_list == veh || veh->prev || veh->next)) {
		DL_DELETE(vehicle_list, veh);
	}
	free_vehicle(veh);
}


/**
* Looks for vehicles in the EXTRACTED state and finishes extracting them.
* Doing this late prevents issues with vehicles being extracted multiple times.
*/
void extract_pending_vehicles(void) {
	vehicle_data *veh;

	if (veh_extractions_pending < 0) {
		log("SYSERR: Negative (%d) vehicle extractions pending.", veh_extractions_pending);
	}
	
	DL_FOREACH_SAFE(vehicle_list, veh, next_pending_vehicle) {
		// check if done?
		if (veh_extractions_pending <= 0) {
			break;
		}
		
		if (VEH_IS_EXTRACTED(veh)) {
			remove_vehicle_flags(veh, VEH_EXTRACTED);
		}
		else {	// not extracting
			continue;
		}
		
		extract_vehicle_final(veh);
		--veh_extractions_pending;
	}

	if (veh_extractions_pending != 0) {
		if (veh_extractions_pending > 0) {
			log("SYSERR: Couldn't find %d vehicle extractions as counted.", veh_extractions_pending);
		}
		
		// likely an error -- search for vehicles who need extracting (for next time)
		veh_extractions_pending = 0;
		DL_FOREACH(vehicle_list, veh) {
			if (VEH_IS_EXTRACTED(veh)) {
				++veh_extractions_pending;
			}
		}
	}
	else {
		veh_extractions_pending = 0;
	}
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
	room_data *was_in;
	
	if (!veh || !(was_in = IN_ROOM(veh))) {
		log("SYSERR: NULL vehicle (%p) or vehicle not in a room (%p) passed to vehicle_from_room", veh, IN_ROOM(veh));
		return;
	}
	
	// yank empire tech (which may be island-based)
	adjust_vehicle_tech(veh, FALSE);
	request_vehicle_save_in_world(veh);
	
	// check lights
	if (VEH_PROVIDES_LIGHT(veh)) {
		--ROOM_LIGHTS(was_in);
	}
	
	DL_DELETE2(ROOM_VEHICLES(was_in), veh, prev_in_room, next_in_room);
	veh->next_in_room = veh->prev_in_room = NULL;
	IN_ROOM(veh) = NULL;
	
	affect_total_room(was_in);
	
	// update mapout if applicable
	if (VEH_IS_VISIBLE_ON_MAPOUT(veh)) {
		request_mapout_update(GET_ROOM_VNUM(was_in));
	}
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
	
	DL_PREPEND2(ROOM_VEHICLES(room), veh, prev_in_room, next_in_room);
	IN_ROOM(veh) = room;
	VEH_LAST_MOVE_TIME(veh) = time(0);
	update_vehicle_island_and_loc(veh, room);
	affect_total_room(room);
	
	// check lights
	if (VEH_PROVIDES_LIGHT(veh)) {
		++ROOM_LIGHTS(room);
	}
	
	// apply empire tech (which may be island-based)
	adjust_vehicle_tech(veh, TRUE);
	
	// update mapout if applicable
	if (VEH_IS_VISIBLE_ON_MAPOUT(veh)) {
		request_mapout_update(GET_ROOM_VNUM(room));
	}
	
	request_vehicle_save_in_world(veh);
}


 //////////////////////////////////////////////////////////////////////////////
//// VEHICLE TARGETING HANDLERS //////////////////////////////////////////////

/**
* Finds a vehicle the char can see in the room.
*
* @param char_data *ch The person who's looking.
* @pararm room_data *room Which room to look for a vehicle in.
* @param char *name The target argument.
* @param int *number Optional: For multi-list number targeting (look 4.cart; may be NULL)
* @return vehicle_data* The vehicle found, or NULL.
*/
vehicle_data *get_vehicle_in_target_room_vis(char_data *ch, room_data *room, char *name, int *number) {
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	vehicle_data *iter;
	int num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	DL_FOREACH2(ROOM_VEHICLES(room), iter, next_in_room) {
		if (!isname(tmp, VEH_KEYWORDS(iter))) {
			continue;
		}
		if (!CAN_SEE_VEHICLE(ch, iter)) {
			continue;
		}
		
		// found: check number
		if (--(*number) == 0) {
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
* @param int *number Optional: For multi-list number targeting (look 4.cart; may be NULL)
* @return vehicle_data* The vehicle found, or NULL.
*/
vehicle_data *get_vehicle_vis(char_data *ch, char *name, int *number) {
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	vehicle_data *iter;
	int num;
	
	// prefer match in same room
	if ((iter = get_vehicle_in_room_vis(ch, name, number))) {
		return iter;
	}
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	DL_FOREACH(vehicle_list, iter) {
		if (!isname(tmp, VEH_KEYWORDS(iter))) {
			continue;
		}
		if (!CAN_SEE_VEHICLE(ch, iter)) {
			continue;
		}
		
		// found: check number
		if (--(*number) == 0) {
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
* @param int *number Optional: For multi-list number targeting (look 4.cart; may be NULL)
* @return vehicle_data* The found vehicle, or NULL.
*/
vehicle_data *get_vehicle_room(room_data *room, char *name, int *number) {
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	vehicle_data *iter;
	int num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	DL_FOREACH2(ROOM_VEHICLES(room), iter, next_in_room) {
		if (!isname(tmp, VEH_KEYWORDS(iter))) {
			continue;
		}
		
		// found: check number
		if (--(*number) == 0) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* Find a vehicle in the world, without regard to visibility.
*
* @param char *name The string to search for.
* @param int *number Optional: For multi-list number targeting (look 4.cart; may be NULL)
* @return vehicle_data* The vehicle found, or NULL.
*/
vehicle_data *get_vehicle_world(char *name, int *number) {
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	vehicle_data *iter;
	int num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	DL_FOREACH(vehicle_list, iter) {
		if (!isname(tmp, VEH_KEYWORDS(iter))) {
			continue;
		}
		
		// found: check number
		if (--(*number)) {
			return iter;
		}
	}

	return NULL;
}


/**
* Find a vehicle in the world that ch can see.
*
* @param char_data *ch The person looking for a vehicle.
* @param char *name The string to search for.
* @param int *number Optional: For multi-list number targeting (look 4.cart; may be NULL)
* @return vehicle_data* The vehicle found, or NULL.
*/
vehicle_data *get_vehicle_world_vis(char_data *ch, char *name, int *number) {
	char tmpname[MAX_INPUT_LENGTH], *tmp = tmpname;
	vehicle_data *iter;
	int num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	DL_FOREACH(vehicle_list, iter) {
		if (!CAN_SEE_VEHICLE(ch, iter) || !isname(tmp, VEH_KEYWORDS(iter))) {
			continue;
		}
		
		// found: check number
		if (--(*number)) {
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
