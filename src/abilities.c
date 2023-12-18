/* ************************************************************************
*   File: abilities.c                                     EmpireMUD 2.0b5 *
*  Usage: DB and OLC for ability data                                     *
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
#include "interpreter.h"
#include "db.h"
#include "comm.h"
#include "olc.h"
#include "skills.h"
#include "handler.h"
#include "dg_scripts.h"
#include "vnums.h"
#include "constants.h"

/**
* Contents:
*   Details screen
*   Helpers
*   Ability Actions
*   Ability Effects and Limits
*   Ability Interaction Funcs
*   Ability Messaging
*   Abilities Over Time
*   Ability Commands
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   Edit Modules
*/

// local data
const char *default_ability_name = "Unnamed Ability";
const bitvector_t conjure_types = ABILT_CONJURE_LIQUID | ABILT_CONJURE_OBJECT | ABILT_CONJURE_VEHICLE;

// local protos
OLC_MODULE(abiledit_costtype);
void call_ability(char_data *ch, ability_data *abil, char *argument, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, int level, int run_mode, struct ability_exec *data);
bool check_ability_limitations(char_data *ch, ability_data *abil, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room);
INTERACTION_FUNC(devastate_crop);
INTERACTION_FUNC(devastate_trees);
void free_ability_exec(struct ability_exec *data);
void free_ability_hooks(struct ability_hook *list);
bool has_matching_role(char_data *ch, ability_data *abil, bool ignore_solo_check);
void post_ability_procs(char_data *ch, ability_data *abil, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, struct ability_exec *data);
double standard_ability_scale(char_data *ch, ability_data *abil, int level, bitvector_t type, struct ability_exec *data);
void send_ability_per_char_messages(char_data *ch, char_data *vict, int quantity, ability_data *abil, struct ability_exec *data, char *replace_1);
void send_ability_per_item_messages(char_data *ch, obj_data *ovict, int quantity, ability_data *abil, struct ability_exec *data, char *replace_1);
void send_ability_per_vehicle_message(char_data *ch, vehicle_data *vvict, int quantity, ability_data *abil, struct ability_exec *data, char *replace_1);
void send_ability_special_messages(char_data *ch, char_data *vict, obj_data *ovict, ability_data *abil, struct ability_exec *data, char **replace, int replace_count);
void start_over_time_ability(char_data *ch, ability_data *abil, char *argument, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, int level, struct ability_exec *data);


// ability function prototypes
DO_ABIL(do_action_ability);
PREP_ABIL(prep_action_ability);

DO_ABIL(do_buff_ability);
PREP_ABIL(prep_buff_ability);

DO_ABIL(do_building_damage_ability);
PREP_ABIL(prep_building_damage_ability);

DO_ABIL(do_conjure_liquid_ability);
PREP_ABIL(prep_conjure_liquid_ability);

DO_ABIL(do_conjure_object_ability);
PREP_ABIL(prep_conjure_object_ability);

DO_ABIL(do_conjure_vehicle_ability);
PREP_ABIL(prep_conjure_vehicle_ability);

DO_ABIL(do_damage_ability);
PREP_ABIL(prep_damage_ability);

DO_ABIL(do_dot_ability);
PREP_ABIL(prep_dot_ability);

DO_ABIL(do_paint_building_ability);
PREP_ABIL(prep_paint_building_ability);

DO_ABIL(do_room_affect_ability);
PREP_ABIL(prep_room_affect_ability);

DO_ABIL(do_teleport_ability);
PREP_ABIL(prep_teleport_ability);


// setup for abilities
struct {
	bitvector_t type;	// ABILT_ const
	PREP_ABIL(*prep_func);	// does the cost setup
	DO_ABIL(*do_func);	// runs the ability
} do_ability_data[] = {
	
	// ABILT_x: setup by type; they run in this order:
	
	// types that don't run functions
	{ ABILT_CRAFT, NULL, NULL },
	{ ABILT_PLAYER_TECH, NULL, NULL },
	{ ABILT_PASSIVE_BUFF, NULL, NULL },
	{ ABILT_READY_WEAPONS, NULL, NULL },
	{ ABILT_COMPANION, NULL, NULL },
	{ ABILT_SUMMON_ANY, NULL, NULL },
	{ ABILT_SUMMON_RANDOM, NULL, NULL },
	{ ABILT_MORPH, NULL, NULL },
	{ ABILT_AUGMENT, NULL, NULL },
	{ ABILT_CUSTOM, NULL, NULL },
	
	// ones that should run early
	{ ABILT_TELEPORT, prep_teleport_ability, do_teleport_ability },
	
	{ ABILT_CONJURE_OBJECT, prep_conjure_object_ability, do_conjure_object_ability },
	{ ABILT_CONJURE_LIQUID, prep_conjure_liquid_ability, do_conjure_liquid_ability },
	{ ABILT_CONJURE_VEHICLE, prep_conjure_vehicle_ability, do_conjure_vehicle_ability },
	
	// run these late
	{ ABILT_PAINT_BUILDING, prep_paint_building_ability, do_paint_building_ability },
	{ ABILT_ROOM_AFFECT, prep_room_affect_ability, do_room_affect_ability },
	{ ABILT_BUFF, prep_buff_ability, do_buff_ability },
	{ ABILT_DOT, prep_dot_ability, do_dot_ability },
	{ ABILT_DAMAGE, prep_damage_ability, do_damage_ability },
	{ ABILT_BUILDING_DAMAGE, prep_building_damage_ability, do_building_damage_ability },
	
	// alaways run actions last
	{ ABILT_ACTION, prep_action_ability, do_action_ability },
	{ NOBITS }	// this goes last
};


 //////////////////////////////////////////////////////////////////////////////
//// DETAILS SCREEN //////////////////////////////////////////////////////////

/**
* Details display for an ability. This also chains to any abilities that have
* this one as both a parent and a hook -- those are multi-part abilities.
*
* @param char_data *ch The player viewing the ability.
* @param ability_data *abil Which ability.
* @param bool dependent FALSE for the first ability, TRUE down the chain.
* @param char *outbuf Buffer to save to.
* @param int sizeof_outbuf Max size of the buffer.
*/
void show_ability_details(char_data *ch, ability_data *abil, bool dependent, char *outbuf, int sizeof_outbuf) {
	bool any, same, has_param_details = FALSE;
	char lbuf[MAX_STRING_LENGTH], sbuf[MAX_STRING_LENGTH];
	char *ptr;
	int count, iter;
	size_t size, l_size;
	ability_data *abiter, *next_abil;
	skill_data *skill, *next_skill;
	struct ability_data_list *adl;
	struct apply_data *apply;
	struct synergy_ability *syn;
	
	// starting line is based on whether this is the core ability or a sub-ability
	if (FALSE && !dependent) {
		size = snprintf(outbuf, sizeof_outbuf, "Information about %s%s\t0:\r\n", ability_color(ch, abil), ABIL_NAME(abil));
	}
	else {
		strcpy(lbuf, " ");
		count = (60 - strlen(ABIL_NAME(abil)) - 4) / 2;
		for (iter = 0; iter < count; ++iter) {
			strcat(lbuf, "-");
		}
		sprintf(lbuf + strlen(lbuf), " %s ", ABIL_NAME(abil));
		for (iter = 0; iter < count; ++iter) {
			strcat(lbuf, "-");
		}
		size = snprintf(outbuf, sizeof_outbuf, "%s\r\n", lbuf);
	}
	
	if (ABIL_MASTERY_ABIL(abil) != NOTHING) {
		size += snprintf(outbuf + size, sizeof_outbuf - size, "Mastery ability: %s%s\t0%s\r\n", ability_color(ch, abil), get_ability_name_by_vnum(ABIL_MASTERY_ABIL(abil)), (PRF_FLAGGED(ch, PRF_SCREEN_READER) && !has_ability(ch, ABIL_VNUM(abil))) ? " (not known)" : "");
	}
	
	if (ABIL_ASSIGNED_SKILL(abil)) {
		size += snprintf(outbuf + size, sizeof_outbuf - size, "Skill: %s%s %d\t0", (get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) >= ABIL_SKILL_LEVEL(abil)) ? "\t0" : "\tr", SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
		if (has_ability(ch, ABIL_VNUM(abil)) && !IS_ANY_SKILL_CAP(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) && can_gain_skill_from(ch, abil)) {
			size += snprintf(outbuf + size, sizeof_outbuf - size, " (%d/%d levels gained)\r\n", levels_gained_from_ability(ch, abil), GAINS_PER_ABILITY);
		}
		else {
			size += snprintf(outbuf + size, sizeof_outbuf - size, "\r\n");
		}
		
		// check purchased
		if (!dependent) {
			any = same = FALSE;
			for (iter = 0; iter < NUM_SKILL_SETS; ++iter) {
				if (has_ability_in_set(ch, ABIL_VNUM(abil), iter)) {
					any = TRUE;
					if (iter == GET_CURRENT_SKILL_SET(ch)) {
						same = TRUE;
					}
				}
			}
			size += snprintf(outbuf + size, sizeof_outbuf - size, "Purchased: %s\r\n", (same ? "yes" : (any ? "other skill set" : "no")));
		}
		
		// prerequisite ability (chain?) -- maybe?
	}
	
	// assigned roles/synergies
	if (!dependent) {
		count = 0;
		l_size = 0;
		*lbuf = '\0';
		HASH_ITER(hh, skill_table, skill, next_skill) {
			LL_FOREACH(SKILL_SYNERGIES(skill), syn) {
				if (syn->ability == ABIL_VNUM(abil)) {
					if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
						snprintf(sbuf, sizeof(sbuf), "%s%s %d + %s %d (%s)\t0", syn->role == NOTHING ? "\tW" : class_role_color[syn->role], SKILL_NAME(skill), SKILL_MAX_LEVEL(skill), get_skill_name_by_vnum(syn->skill), syn->level, syn->role == NOTHING ? "All" : class_role[syn->role]);
					}
					else {
						snprintf(sbuf, sizeof(sbuf), "%s%s %d + %s %d (%c)\t0", syn->role == NOTHING ? "\tW" : class_role_color[syn->role], SKILL_ABBREV(skill), SKILL_MAX_LEVEL(skill), get_skill_abbrev_by_vnum(syn->skill), syn->level, syn->role == NOTHING ? 'A' : *class_role[syn->role]);
					}
					if (strlen(sbuf) > 41) {
						// too long for half a line
						l_size += snprintf(lbuf + l_size, sizeof(lbuf) - l_size, "%s %s\r\n", (!(++count % 2) && !PRF_FLAGGED(ch, PRF_SCREEN_READER) ? "\r\n" : ""), sbuf);
						if (count % 2) {
							++count;	// fix columns for the next line, if any
						}
					}
					else {
						l_size += snprintf(lbuf + l_size, sizeof(lbuf) - l_size, " %-41.41s%s", sbuf, (!(++count % 2) || PRF_FLAGGED(ch, PRF_SCREEN_READER) ? "\r\n" : " "));
					}
				}
			}
		}
		if (*lbuf) {
			size += snprintf(outbuf + size, sizeof_outbuf - size, "Synergies:\r\n%s%s", lbuf, (!(++count % 2) && !PRF_FLAGGED(ch, PRF_SCREEN_READER) ? "\r\n" : ""));
		}
	}
	
	// types, if parameterized
	if (ABIL_TYPE_LIST(abil)) {
		get_ability_type_display(ABIL_TYPE_LIST(abil), lbuf, TRUE);
		if (*lbuf) {
			if (strstr(lbuf, "buff") && ABILITY_FLAGGED(abil, ABILF_VIOLENT)) {
				// replace "buff" with "debuff"
				ptr = str_replace("buff", "debuff", lbuf);
				strcpy(lbuf, ptr);
				free(ptr);
			}
			has_param_details = TRUE;
			size += snprintf(outbuf + size, sizeof_outbuf - size, "Type%s: %s\r\n", (strchr(lbuf, ',') ? "s" : ""), lbuf);
		}
	}
	
	// linked trait, if parameterized
	if (ABIL_LINKED_TRAIT(abil) != APPLY_NONE) {
		has_param_details = TRUE;
		strcpy(lbuf, apply_types[ABIL_LINKED_TRAIT(abil)]);
		strtolower(lbuf);
		size += snprintf(outbuf + size, sizeof_outbuf - size, "Linked trait: %s (%d)\r\n", lbuf, get_attribute_by_apply(ch, ABIL_LINKED_TRAIT(abil)));
	}
	
	if (ABIL_REQUIRES_TOOL(abil)) {
		prettier_sprintbit(ABIL_REQUIRES_TOOL(abil), tool_flags, lbuf);
		size += snprintf(outbuf + size, sizeof_outbuf - size, "Requires tool%s: %s\r\n", (count_bits(ABIL_REQUIRES_TOOL(abil)) != 1) ? "s" : "", lbuf);
	}
	
	if (ABIL_COST(abil) > 0 || ABIL_COST_PER_SCALE_POINT(abil) > 0) {
		size += snprintf(outbuf + size, sizeof_outbuf - size, "Cost: %d%s %s\r\n", ABIL_COST(abil), (ABIL_COST_PER_SCALE_POINT(abil) > 0 ? "+" : ""), pool_types[ABIL_COST_TYPE(abil)]);
	}
	
	// Cooldown?
	if (ABIL_COOLDOWN_SECS(abil) >= 60 * 60) {
		size += snprintf(outbuf + size, sizeof_outbuf - size, "Cooldown: %d:%02d:%02d (hours)\r\n", (ABIL_COOLDOWN_SECS(abil) / 3600), ((ABIL_COOLDOWN_SECS(abil) % 3600) / 60), ((ABIL_COOLDOWN_SECS(abil) % 3600) % 60));
	}
	else if (ABIL_COOLDOWN_SECS(abil) >= 60) {
		size += snprintf(outbuf + size, sizeof_outbuf - size, "Cooldown: %d:%02d (minutes)\r\n", (ABIL_COOLDOWN_SECS(abil) / 60), (ABIL_COOLDOWN_SECS(abil) % 60));
	}
	else if (ABIL_COOLDOWN_SECS(abil) > 0) {
		size += snprintf(outbuf + size, sizeof_outbuf - size, "Cooldown: %d second%s\r\n", ABIL_COOLDOWN_SECS(abil), PLURAL(ABIL_COOLDOWN_SECS(abil)));
	}
	
	// data, if parameterized
	if (ABIL_DATA(abil)) {
		// techs
		*lbuf = '\0';
		l_size = 0;
		LL_FOREACH(ABIL_DATA(abil), adl) {
			if (adl->type == ADL_PLAYER_TECH) {
				l_size += snprintf(lbuf + l_size, sizeof(lbuf) - l_size, "%s%s", (*lbuf ? ", " : ""), player_tech_types[adl->vnum]);
			}
		}
		if (*lbuf) {
			has_param_details = TRUE;
			size += snprintf(outbuf + size, sizeof_outbuf - size, "Player tech%s: %s\r\n", (strchr(lbuf, ',') ? "s" : ""), lbuf);
		}
	}
	
	// applies
	*lbuf = '\0';
	LL_FOREACH(ABIL_APPLIES(abil), apply) {
		sprintf(lbuf + strlen(lbuf), "%s%s", *lbuf ? ", " : "", apply_types[apply->location]);
	}
	if (*lbuf) {
		size += snprintf(outbuf + size, sizeof_outbuf - size, "Modifies: %s\r\n", lbuf);
	}
	
	// affects
	if (ABIL_AFFECTS(abil)) {
		sprintbit(ABIL_AFFECTS(abil), affected_bits, lbuf, TRUE);
		size += snprintf(outbuf + size, sizeof_outbuf - size, "Affects: %s\r\n", lbuf);
	}
	
	// notes (flags), if parameterized -- LAST
	prettier_sprintbit(ABIL_FLAGS(abil), ability_flag_notes, lbuf);
	if (*lbuf && str_cmp(lbuf, "none")) {
		has_param_details = TRUE;
		size += snprintf(outbuf + size, sizeof_outbuf - size, "Notes: %s\r\n", lbuf);
	}
/*
	More things we could show (from vstat ability):
	
	sprintbit(ABIL_IMMUNITIES(abil), affected_bits, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Immunities: \tc%s\t0\r\n", part);

	sprintbit(ABIL_GAIN_HOOKS(abil), ability_gain_hooks, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Gain hooks: \tg%s\t0\r\n", part);

	// command-related portion
	if (ABIL_COMMAND(abil)) {
		sprintbit(ABIL_TARGETS(abil), ability_target_flags, part, TRUE);
		size += snprintf(buf + size, sizeof(buf) - size, "Command info: [\ty%s\t0], Targets: \tg%s\t0\r\n", ABIL_COMMAND(abil), part);
	}
	size += snprintf(buf + size, sizeof(buf) - size, "Cost: [\tc%d %s (+%d/scale)\t0], Cooldown: [\tc%d %s\t0], Cooldown time: [\tc%d second%s\t0]\r\n", ABIL_COST(abil), pool_types[ABIL_COST_TYPE(abil)], ABIL_COST_PER_SCALE_POINT(abil), ABIL_COOLDOWN(abil), get_generic_name_by_vnum(ABIL_COOLDOWN(abil)),  ABIL_COOLDOWN_SECS(abil), PLURAL(ABIL_COOLDOWN_SECS(abil)));

	// type-specific data
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_DOT)) {
		if (ABIL_SHORT_DURATION(abil) == UNLIMITED) {
			strcpy(part, "unlimited");
		}
		else {
			snprintf(part, sizeof(part), "%d", ABIL_SHORT_DURATION(abil));
		}
		if (ABIL_LONG_DURATION(abil) == UNLIMITED) {
			strcpy(part2, "unlimited");
		}
		else {
			snprintf(part2, sizeof(part2), "%d", ABIL_LONG_DURATION(abil));
		}
		size += snprintf(buf + size, sizeof(buf) - size, "Durations: [\tc%s/%s seconds\t0]\r\n", part, part2);
	
	}	// end buff/dot
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_PASSIVE_BUFF)) {
		sprintbit(ABIL_AFFECTS(abil), affected_bits, part, TRUE);
		size += snprintf(buf + size, sizeof(buf) - size, "Affect flags: \tg%s\t0\r\n", part);
	
		// applies
		size += snprintf(buf + size, sizeof(buf) - size, "Applies: ");
		count = 0;
		LL_FOREACH(ABIL_APPLIES(abil), app) {
			size += snprintf(buf + size, sizeof(buf) - size, "%s%d to %s", count++ ? ", " : "", app->weight, apply_types[app->location]);
		}
		if (!ABIL_APPLIES(abil)) {
			size += snprintf(buf + size, sizeof(buf) - size, "none");
		}
		size += snprintf(buf + size, sizeof(buf) - size, "\r\n");
	}	// end buff
	if (IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Attack type: [\tc%d\t0]\r\n", ABIL_ATTACK_TYPE(abil));
	}	// end damage
	if (IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE | ABILT_DOT)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Damage type: [\tc%s\t0]\r\n", damage_types[ABIL_DAMAGE_TYPE(abil)]);
	}	// end damage/dot
	if (IS_SET(ABIL_TYPES(abil), ABILT_DOT)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Max stacks: [\tc%d\t0]\r\n", ABIL_MAX_STACKS(abil));
	}	// end dot
	
	*/
	
	// children?
	HASH_ITER(hh, ability_table, abiter, next_abil) {
		if (!has_ability(ch, ABIL_VNUM(abiter))) {
			continue;	// must have for this
		}
		/*
		if (!has_ability_data_any(abiter, ADL_PARENT)) {
			continue;	// nobody's child
		}
		*/
		if (!has_ability_hook(abiter, AHOOK_ABILITY, ABIL_VNUM(abil))) {
			continue;	// does not hook on me
		}
		
		// seems ok
		show_ability_details(ch, abiter, TRUE, lbuf, sizeof(lbuf));
		if (*lbuf && size + strlen(lbuf) < sizeof_outbuf) {
			has_param_details = TRUE;
			strcat(outbuf, lbuf);
			size += strlen(lbuf);
		}
	}
	
	if (!has_param_details) {
		size += snprintf(outbuf + size, sizeof_outbuf - size, "(Not all abilities have additional details available to show here)\r\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

// stops execution, e.g. in a PREP function
#define CANCEL_ABILITY(data)  { data->stop = TRUE; data->should_charge_cost = FALSE; data->no_msg = TRUE; }


/**
* String display for one ADL item.
*
* @param struct ability_data_list *adl The data item to show.
* @return char* The string to display.
*/
char *ability_data_display(struct ability_data_list *adl) {
	static char output[MAX_STRING_LENGTH];
	char type_str[256], part[256];
	
	prettier_sprintbit(adl->type, ability_data_types, type_str);
	
	// ADL_x: display by type
	switch (adl->type) {
		case ADL_PLAYER_TECH: {
			snprintf(output, sizeof(output), "%s: %s", type_str, player_tech_types[adl->vnum]);
			break;
		}
		case ADL_EFFECT: {
			snprintf(output, sizeof(output), "%s: %s", type_str, ability_effects[adl->vnum]);
			break;
		}
		case ADL_READY_WEAPON: {
			snprintf(output, sizeof(output), "%s: [%d] %s", type_str, adl->vnum, get_obj_name_by_proto(adl->vnum));
			break;
		}
		case ADL_SUMMON_MOB: {
			snprintf(output, sizeof(output), "%s: [%d] %s", type_str, adl->vnum, get_mob_name_by_proto(adl->vnum, FALSE));
			break;
		}
		case ADL_LIMITATION: {
			// ABLIM_x:
			switch (ability_limitation_misc[adl->vnum]) {
				case ABLIM_ITEM_TYPE: {
					sprinttype(adl->misc, item_types, part, sizeof(part), "UNKNOWN");
					snprintf(output, sizeof(output), "%s: %s %s", type_str, ability_limitations[adl->vnum], part);
					break;
				}
				case ABLIM_ATTACK_TYPE: {
					snprintf(output, sizeof(output), "%s: %s %s", type_str, ability_limitations[adl->vnum], get_attack_name_by_vnum(adl->misc));
					break;
				}
				case ABLIM_WEAPON_TYPE: {
					sprinttype(adl->misc, weapon_types, part, sizeof(part), "UNKNOWN");
					snprintf(output, sizeof(output), "%s: %s %s", type_str, ability_limitations[adl->vnum], part);
					break;
				}
				case ABLIM_NOTHING:
				default: {
					snprintf(output, sizeof(output), "%s: %s", type_str, ability_limitations[adl->vnum]);
					break;
				}
			}
			break;
		}
		case ADL_PAINT_COLOR: {
			snprintf(output, sizeof(output), "%s: %s%s", type_str, paint_colors[adl->vnum], paint_names[adl->vnum]);
			break;
		}
		case ADL_ACTION: {
			snprintf(output, sizeof(output), "%s: %s", type_str, ability_actions[adl->vnum]);
			break;
		}
		case ADL_RANGE: {
			snprintf(output, sizeof(output), "%s: %d", type_str, adl->vnum);
			break;
		}
		case ADL_PARENT: {
			snprintf(output, sizeof(output), "%s: %d %s", type_str, adl->vnum, get_ability_name_by_vnum(adl->vnum));
			break;
		}
		default: {
			snprintf(output, sizeof(output), "%s: ???", type_str);
			break;
		}
	}
	
	return output;
}


/**
* String display for one ability hook.
*
* @param struct ability_data_list *adl The data item to show.
* @return char* The string to display.
*/
char *ability_hook_display(struct ability_hook *ahook) {
	static char output[MAX_STRING_LENGTH];
	char type_str[256], label[256];
	
	prettier_sprintbit(ahook->type, ability_hook_types, type_str);
	snprintf(label, sizeof(label), "%s %.2f%%", type_str, ahook->percent);
	
	// AHOOK_x: display by type
	switch (ahook->type) {
		case AHOOK_ABILITY: {
			snprintf(output, sizeof(output), "%s: [%d] %s", label, ahook->value, get_ability_name_by_vnum(ahook->value));
			break;
		}
		case AHOOK_ATTACK_TYPE: {
			snprintf(output, sizeof(output), "%s: %s", label, get_attack_name_by_vnum(ahook->value));
			break;
		}
		case AHOOK_WEAPON_TYPE: {
			snprintf(output, sizeof(output), "%s: %s", label, weapon_types[ahook->value]);
			break;
		}
		case AHOOK_DAMAGE_TYPE: {
			snprintf(output, sizeof(output), "%s: %s", label, damage_types[ahook->value]);
			break;
		}
		
		// simple types
		case AHOOK_ATTACK:
		case AHOOK_KILL:
		case AHOOK_MELEE_ATTACK:
		case AHOOK_RANGED_ATTACK: {
			snprintf(output, sizeof(output), "%s", label);
			break;
		}
		default: {
			snprintf(output, sizeof(output), "%s: Unknown type %llu %d %d", label, ahook->type, ahook->value, ahook->misc);
			break;
		}
	}
	
	return output;
}


/**
* Adds a gain hook for an ability.
*
* @param char_data *ch The player to add a hook to.
* @param ability_data *abil The ability to add a hook for.
*/
void add_ability_gain_hook(char_data *ch, ability_data *abil) {
	struct ability_gain_hook *agh;
	any_vnum vnum;
	
	if (!ch || IS_NPC(ch) || !abil) {
		return;
	}
	
	vnum = ABIL_VNUM(abil);
	HASH_FIND_INT(GET_ABILITY_GAIN_HOOKS(ch), &vnum, agh);
	if (!agh) {
		CREATE(agh, struct ability_gain_hook, 1);
		agh->ability = ABIL_VNUM(abil);
		HASH_ADD_INT(GET_ABILITY_GAIN_HOOKS(ch), ability, agh);
	}
	
	agh->triggers = ABIL_GAIN_HOOKS(abil);
}


/**
* Sets up the gain hooks for a player's ability on login.
*
* @param char_data *ch The player to set up hooks for.
*/
void add_all_gain_hooks(char_data *ch) {
	struct player_ability_data *abil, *next_abil;
	bool any;
	int iter;
	
	if (!ch || IS_NPC(ch)) {
		return;
	}
	
	HASH_ITER(hh, GET_ABILITY_HASH(ch), abil, next_abil) {
		any = FALSE;
		for (iter = 0; iter < NUM_SKILL_SETS && !any; ++iter) {
			if (abil->purchased[iter]) {
				any = TRUE;
				add_ability_gain_hook(ch, abil->ptr);
			}
		}
	}
}


/**
* This function adds a type to the ability's type list, and updates the summary
* type flags.
*
* @param ability_data *abil Which ability we're adding a type to.
* @param bitvector_t type Which ABILT_ flag.
* @param int weight How much weight it gets (for scaling).
*/
void add_type_to_ability(ability_data *abil, bitvector_t type, int weight) {
	bitvector_t total = NOBITS;
	struct ability_type *at;
	bool found = FALSE;
	
	if (!type) {
		return;
	}
	
	LL_FOREACH(ABIL_TYPE_LIST(abil), at) {
		total |= at->type;
		
		if (at->type == type) {
			at->weight = weight;
			found = TRUE;
		}
	}
	
	if (!found) {
		CREATE(at, struct ability_type, 1);
		at->type = type;
		at->weight = weight;
		LL_APPEND(ABIL_TYPE_LIST(abil), at);
	}
	
	total |= type;
	ABIL_TYPES(abil) = total;	// summarize flags
}


/**
* Builds the bit set for all hooks on the ability.
*
* @param ability_data *abil The ability to compile hooks for.
* @return bitvector_t The combined hooks for that ability.
*/
bitvector_t all_abil_hooks(ability_data *abil) {
	bitvector_t flags = NOBITS;
	struct ability_hook *ahook;
	
	LL_FOREACH(ABIL_HOOKS(abil), ahook) {
		flags |= ahook->type;
	}
	
	return flags;
}


/**
* Takes the techs from an ability and applies them to a player.
*
* @param char_data *ch The player to apply to.
* @param ability_data *abil The ability whose techs we'll apply.
*/
void apply_ability_techs_to_player(char_data *ch, ability_data *abil) {
	struct ability_data_list *adl;
	
	if (IS_NPC(ch) || !IS_SET(ABIL_TYPES(abil), ABILT_PLAYER_TECH)) {
		return;	// no techs to apply
	}
	
	LL_FOREACH(ABIL_DATA(abil), adl) {
		if (adl->type != ADL_PLAYER_TECH) {
			continue;
		}
		
		// ok
		add_player_tech(ch, ABIL_VNUM(abil), adl->vnum);
	}
}


/**
* Applies all the ability techs from a player's current skill set (e.g. upon
* login).
*
* @param char_data *ch The player.
*/
void apply_all_ability_techs(char_data *ch) {
	struct player_ability_data *plab, *next_plab;
	ability_data *abil;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		abil = plab->ptr;
		
		if (!plab->purchased[GET_CURRENT_SKILL_SET(ch)]) {
			continue;
		}
		
		if (IS_SET(ABIL_TYPES(abil), ABILT_PLAYER_TECH)) {
			apply_ability_techs_to_player(ch, abil);
		}
	}
}


/**
* Applies the passive buffs from 1 ability to the player. Call affect_total()
* when you're done applying passive buffs.
*
* @param char_data *ch The player.
* @param ability_data *abil The passive-buff ability to apply.
*/
void apply_one_passive_buff(char_data *ch, ability_data *abil) {
	double remaining_points = 0, total_points = 0, share, amt;
	struct ability_exec *data;
	struct affected_type *af;
	struct apply_data *apply;
	int cap, level, total_w = 0;
	bool unscaled;
	
	if (!ch || IS_NPC(ch) || !abil || !IS_SET(ABIL_TYPES(abil), ABILT_PASSIVE_BUFF)) {
		return;	// safety first
	}
	
	// remove if already on there
	remove_passive_buff_by_ability(ch, ABIL_VNUM(abil));
	
	CREATE(data, struct ability_exec, 1);
	data->abil = abil;
	data->matching_role = has_matching_role(ch, abil, FALSE);
	unscaled = (ABILITY_FLAGGED(abil, ABILF_UNSCALED_BUFF) ? TRUE : FALSE);
	GET_RUNNING_ABILITY_DATA(ch) = data;
	
	// things only needed for scaled buffs
	if (!unscaled) {
		level = get_approximate_level(ch);
		if (ABIL_ASSIGNED_SKILL(abil) && (cap = get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil)))) < CLASS_SKILL_CAP) {
			level = MIN(level, cap);	// constrain by skill level
		}
		total_points = remaining_points = standard_ability_scale(ch, abil, level, ABILT_PASSIVE_BUFF, data);
		
		if (total_points < 0) {	// no work
			GET_RUNNING_ABILITY_DATA(ch) = NULL;
			free_ability_exec(data);
			return;
		}
	}
	
	// affect flags? cost == level 100 ability
	if (ABIL_AFFECTS(abil)) {
		if (!unscaled) {
			remaining_points -= count_bits(ABIL_AFFECTS(abil)) * config_get_double("scale_points_at_100");
			remaining_points = MAX(0, remaining_points);
		}
		
		af = create_flag_aff(ABIL_VNUM(abil), UNLIMITED, ABIL_AFFECTS(abil), ch);
		LL_APPEND(GET_PASSIVE_BUFFS(ch), af);
		affect_modify(ch, af->location, af->modifier, af->bitvector, TRUE);
	}
	
	// determine share for effects
	if (!unscaled) {
		total_w = 0;
		LL_FOREACH(ABIL_APPLIES(abil), apply) {
			if (!apply_never_scales[apply->location]) {
				total_w += ABSOLUTE(apply->weight);
			}
		}
	}
	
	// now create affects for each apply that we can afford
	LL_FOREACH(ABIL_APPLIES(abil), apply) {
		if (apply_never_scales[apply->location] || unscaled) {
			af = create_mod_aff(ABIL_VNUM(abil), UNLIMITED, apply->location, apply->weight, ch);
			LL_APPEND(GET_PASSIVE_BUFFS(ch), af);
			affect_modify(ch, af->location, af->modifier, af->bitvector, TRUE);
			continue;
		}
		
		share = total_points * (double) ABSOLUTE(apply->weight) / (double) total_w;
		if (share > remaining_points) {
			share = MIN(share, remaining_points);
		}
		amt = round(share / apply_values[apply->location]) * ((apply->weight < 0) ? -1 : 1);
		if (share > 0 && amt != 0) {
			remaining_points -= share;
			remaining_points = MAX(0, total_points);
			
			af = create_mod_aff(ABIL_VNUM(abil), UNLIMITED, apply->location, amt, ch);
			LL_APPEND(GET_PASSIVE_BUFFS(ch), af);
			affect_modify(ch, af->location, af->modifier, af->bitvector, TRUE);
		}
	}
	
	GET_RUNNING_ABILITY_DATA(ch) = NULL;
	free_ability_exec(data);
}


/**
* Things that are checked early in perform_ability_command(), which may also
* be checked repeatedly in perform_over_time_ability(). This function should
* send a message if it fails, and ONLY if it fails.
*
* @param char_data *ch The person trying to do the ability.
* @param ability_data *abil Which ability they are doing.
* @return bool TRUE if ok, FALSE to cancel out.
*/
bool check_ability_pre_target(char_data *ch, ability_data *abil) {
	if (!can_use_ability(ch, ABIL_VNUM(abil), MOVE, 0, NOTHING)) {
		// pre-validates JUST the ability -- individual types must validate cost/cooldown
		// sent its own error message
		return FALSE;
	}
	if (!char_can_act(ch, ABIL_MIN_POS(abil), !ABILITY_FLAGGED(abil, ABILF_NO_ANIMAL), !ABILITY_FLAGGED(abil, ABILF_NO_INVULNERABLE | ABILF_VIOLENT), FALSE)) {
		// sent its own error message
		return FALSE;
	}
	if (ABILITY_FLAGGED(abil, ABILF_NOT_IN_COMBAT) && FIGHTING(ch)) {
		send_to_char("No way! You're fighting for your life!\r\n", ch);
		return FALSE;
	}
	if (RMT_FLAGGED(IN_ROOM(ch), RMT_PEACEFUL) && ABIL_IS_VIOLENT(abil)) {
		msg_to_char(ch, "You can't %s%s here.\r\n", ABIL_COMMAND(abil) ? "use " : "", SAFE_ABIL_COMMAND(abil));
		return FALSE;
	}
	if (ABIL_COST_TYPE(abil) == BLOOD && ABIL_COST(abil) > 0 && !ABILITY_FLAGGED(abil, ABILF_IGNORE_SUN) && !check_vampire_sun(ch, TRUE)) {
		return FALSE;	// sun fail
	}
	if (ABILITY_FLAGGED(abil, ABILF_SPOKEN) && ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_SILENT)) {
		msg_to_char(ch, "You can't seem to get the words out.\r\n");
		return FALSE;
	}
	
	// ok!
	return TRUE;
}


/**
* Audits abilities on startup.
*/
void check_abilities(void) {
	ability_data *abil, *next_abil;
	
	HASH_ITER(hh, ability_table, abil, next_abil) {
		if (ABIL_MASTERY_ABIL(abil) != NOTHING && !find_ability_by_vnum(ABIL_MASTERY_ABIL(abil))) {
			log("- Ability [%d] %s has invalid mastery ability %d", ABIL_VNUM(abil), ABIL_NAME(abil), ABIL_MASTERY_ABIL(abil));
			ABIL_MASTERY_ABIL(abil) = NOTHING;
		}
	}
}


/**
* This will delete any matching hooks.
*
* @param ability_data *abil Which ability to delete hooks from.
* @param int hook_type Which AHOOK_ const to look for.
* @param int hook_value Which value to match.
* @return bool TRUE if abil had that hook and it was deleted, FALSE if not.
*/

bool delete_from_ability_hooks(ability_data *abil, int hook_type, int hook_value) {
	struct ability_hook *ahook, *next;
	bool any = FALSE;
	
	LL_FOREACH_SAFE(ABIL_HOOKS(abil), ahook, next) {
		if (ahook->type == hook_type && ahook->value == hook_value) {
			LL_DELETE(ABIL_HOOKS(abil), ahook);
			free(ahook);
			any = TRUE;
		}
	}
	return any;
}


/**
* @param ability_data *abil Which ability.
* @param int hook_type Which AHOOK_ const to look for.
* @param int hook_value Which value to match (ability vnum or 0 for many other types).
* @return bool TRUE if abil has that hook, FALSE if not.
*/
bool has_ability_hook(ability_data *abil, int hook_type, int hook_value) {
	struct ability_hook *ahook;
	LL_FOREACH(ABIL_HOOKS(abil), ahook) {
		if (ahook->type == hook_type && ahook->value == hook_value) {
			return TRUE;
		}
	}
	return FALSE;
}


/**
* Runs simple checks, silently, to see if the ability is likely to be valid.
* Some of these will be checked again, with messages, later.
*
* @param char_data *ch The person performing abilities.
* @param ability_data *abil The pre-validated ability, which the character has.
* @return bool TRUE if ok to proceed; FALSE if not.
*/
bool pre_check_hooked_ability(char_data *ch, ability_data *abil) {
	// like check_ability_pre_target():
	if (GET_POS(ch) < ABIL_MIN_POS(abil) || (FIGHTING(ch) && ABILITY_FLAGGED(abil, ABILF_NOT_IN_COMBAT))) {
		return FALSE;	// min pos or combat
	}
	if (ABILITY_FLAGGED(abil, ABILF_NO_ANIMAL) && CHAR_MORPH_FLAGGED(ch, MORPHF_ANIMAL)) {
		return FALSE;	// animal
	}
	if (ABILITY_FLAGGED(abil, ABILF_NO_INVULNERABLE | ABILF_VIOLENT) && AFF_FLAGGED(ch, AFF_NO_ATTACK)) {
		return FALSE;	// invulnerable
	}
	if (RMT_FLAGGED(IN_ROOM(ch), RMT_PEACEFUL) && ABIL_IS_VIOLENT(abil)) {
		return FALSE;	// peaceful
	}
	if (ABIL_COST_TYPE(abil) == BLOOD && ABIL_COST(abil) > 0 && !ABILITY_FLAGGED(abil, ABILF_IGNORE_SUN) && !check_vampire_sun(ch, FALSE)) {
		return FALSE;	// sun fail
	}
	if (ABIL_COST_TYPE(abil) == BLOOD && ABIL_COST(abil) > 0 && !CAN_SPEND_BLOOD(ch)) {
		return FALSE;	// can't spend blood
	}
	if (ABILITY_FLAGGED(abil, ABILF_SPOKEN) && ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_SILENT)) {
		return FALSE;	// silent
	}
	if (ABIL_REQUIRES_TOOL(abil) && !has_all_tools(ch, ABIL_REQUIRES_TOOL(abil))) {
		return FALSE;	// missing tools
	}
	if (ABIL_RESOURCE_COST(abil) && !has_resources(ch, ABIL_RESOURCE_COST(abil), FALSE, FALSE, ABIL_NAME(abil))) {
		return FALSE;	// missing resources
	}
	
	// other checks similar to what will be called with messages in call_ability():
	if (ABIL_COST(abil) > 0 && GET_CURRENT_POOL(ch, ABIL_COST_TYPE(abil)) < ABIL_COST(abil)) {
		return FALSE;	// unlikely to afford cost
	}
	if (ABIL_COOLDOWN(abil) != NOTHING && get_cooldown_time(ch, ABIL_COOLDOWN(abil)) > 0) {
		return FALSE;	// on cooldown
	}
	
	// ok!
	return TRUE;
}


/**
* Copies ability hook list.
*
* @param struct ability_hook *input_list The list to copy.
* @return struct ability_hook* The copied list.
*/
struct ability_hook *copy_ability_hooks(struct ability_hook *input_list) {
	struct ability_hook *hook, *new_hook, *list;
	
	// copy hooks in order
	list = NULL;
	LL_FOREACH(input_list, hook) {
		CREATE(new_hook, struct ability_hook, 1);
		*new_hook = *hook;
		new_hook->next = NULL;
		LL_APPEND(list, new_hook);
	}
	
	return list;
}


/**
* Duplicates a list of ability data.
*
* @param struct ability_data_list *input The list to duplicate.
* @return struct ability_data_list* The copy.
*/
struct ability_data_list *copy_data_list(struct ability_data_list *input) {
	struct ability_data_list *list = NULL, *adl, *iter;
	
	LL_FOREACH(input, iter) {
		CREATE(adl, struct ability_data_list, 1);
		*adl = *iter;
		LL_APPEND(list, adl);
	}
	
	return list;
}


/**
* Finds and removes an entry from an ability data list, by type + vnum + misc.
*
* @param ability_data *abil Which ability.
* @param int type Which ADL_ type.
* @param any_vnum vnum Which vnum to remove.
* @param int misc Which misc value to match.
* @return bool TRUE if any were removed, FALSE if not found.
*/
bool delete_misc_from_ability_data_list(ability_data *abil, int type, any_vnum vnum, int misc) {
	struct ability_data_list *adl, *next;
	bool any = FALSE;
	
	LL_FOREACH_SAFE(ABIL_DATA(abil), adl, next) {
		if (adl->type == type && adl->vnum == vnum && adl->misc == misc) {
			LL_DELETE(ABIL_DATA(abil), adl);
			free(adl);
			any = TRUE;
		}
	}
	
	return any;
}


/**
* Finds and removes an entry from an ability data list, by vnum.
*
* @param ability_data *abil Which ability.
* @param int type Which ADL_ type.
* @param any_vnum vnum Which vnum to remove.
* @return bool TRUE if any were removed, FALSE if not found.
*/
bool delete_from_ability_data_list(ability_data *abil, int type, any_vnum vnum) {
	struct ability_data_list *adl, *next;
	bool any = FALSE;
	
	LL_FOREACH_SAFE(ABIL_DATA(abil), adl, next) {
		if (adl->type == type && adl->vnum == vnum) {
			LL_DELETE(ABIL_DATA(abil), adl);
			free(adl);
			any = TRUE;
		}
	}
	
	return any;
}


/**
* Finds an ability by ambiguous argument, which may be a vnum or a name.
* Names are matched by exact match first, or by multi-abbrev.
*
* @param char *argument The user input.
* @return ability_data* The ability, or NULL if it doesn't exist.
*/
ability_data *find_ability(char *argument) {
	ability_data *abil;
	any_vnum vnum;
	
	if (isdigit(*argument) && (vnum = atoi(argument)) >= 0 && (abil = find_ability_by_vnum(vnum))) {
		return abil;
	}
	else {
		return find_ability_by_name(argument);
	}
}


/**
* Look up an ability by multi-abbrev, preferring exact matches.
*
* @param char *name The ability name to look up.
* @param bool allow_abbrev If true, allows abbreviations.
* @return ability_data* The ability, or NULL if it doesn't exist.
*/
ability_data *find_ability_by_name_exact(char *name, bool allow_abbrev) {
	ability_data *abil, *next_abil, *partial = NULL;
	
	if (!*name) {
		return NULL;	// shortcut
	}
	
	HASH_ITER(sorted_hh, sorted_abilities, abil, next_abil) {
		// matches:
		if (!str_cmp(name, ABIL_NAME(abil))) {
			// perfect match
			return abil;
		}
		if (allow_abbrev && !partial && is_multiword_abbrev(name, ABIL_NAME(abil))) {
			// probable match
			partial = abil;
		}
	}
	
	// no exact match...
	return partial;
}


/**
* @param any_vnum vnum Any ability vnum
* @return ability_data* The ability, or NULL if it doesn't exist
*/
ability_data *find_ability_by_vnum(any_vnum vnum) {
	ability_data *abil;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(ability_table, &vnum, abil);
	return abil;
}


/**
* Finds an ability data entry that matches. This version ignores the 'misc'
* field.
*
* @param ability_data *abil Which ability.
* @param int type Which ADL_ type.
* @param any_vnum vnum Which vnum to find.
* @return struct ability_data_list* The matching entry if it exists, or NULL if not.
*/
struct ability_data_list *find_ability_data_entry_for(ability_data *abil, int type, any_vnum vnum) {
	struct ability_data_list *adl;
	
	LL_FOREACH(ABIL_DATA(abil), adl) {
		if (adl->type == type && adl->vnum == vnum) {
			return adl;
		}
	}
	
	return NULL;
}


/**
* Finds an ability data entry that matches, including the misc field.
*
* @param ability_data *abil Which ability.
* @param int type Which ADL_ type.
* @param any_vnum vnum Which vnum to find.
* @param int misc Which misc to find.
* @return struct ability_data_list* The matching entry if it exists, or NULL if not.
*/
struct ability_data_list *find_ability_data_entry_for_misc(ability_data *abil, int type, any_vnum vnum, int misc) {
	struct ability_data_list *adl;
	
	LL_FOREACH(ABIL_DATA(abil), adl) {
		if (adl->type == type && adl->vnum == vnum && adl->misc == misc) {
			return adl;
		}
	}
	
	return NULL;
}


/**
* Finds an ability that is attached to a skill -- this works similar to the
* find_ability() function except that it ignores partial matches that aren't
* attached to the skill.
*
* @param char *name The name to look up.
* @param skill_data *skill The skill to search on.
* @return ability_data* The found ability, if any.
*/
ability_data *find_ability_on_skill(char *name, skill_data *skill) {
	ability_data *abil, *partial = NULL;
	struct skill_ability *skab;
	any_vnum vnum = NOTHING;
	
	if (!skill || !*name) {
		return NULL;	// shortcut
	}
	
	if (isdigit(*name)) {
		vnum = atoi(name);
	}
	
	LL_FOREACH(SKILL_ABILITIES(skill), skab) {
		if (!(abil = find_ability_by_vnum(skab->vnum))) {
			continue;
		}
		
		if (vnum == ABIL_VNUM(abil) || !str_cmp(name, ABIL_NAME(abil))) {
			return abil;	// exact
		}
		else if (!partial && is_multiword_abbrev(name, ABIL_NAME(abil))) {
			partial = abil;
		}
	}
	
	return partial;	// if any
}


/**
* Finds which ability a player has that's giving them a ptech. If there's more
* than one, it returns the first one it finds.
*
* @param char_data *ch The player.
* @param int ptech Any PTECH_ const.
* @return ability_data* Which ability the player has that grants that ptech, or NULL if none.
*/
ability_data *find_player_ability_by_tech(char_data *ch, int ptech) {
	struct player_ability_data *plab, *next_plab;
	struct ability_data_list *adl;
	ability_data *abil;
	
	if (!ch || IS_NPC(ch)) {
		return NULL;
	}
	
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		abil = plab->ptr;
		
		if (IS_SET(ABIL_TYPES(abil), ABILT_PLAYER_TECH) && plab->purchased[GET_CURRENT_SKILL_SET(ch)]) {
			LL_FOREACH(ABIL_DATA(abil), adl) {
				if (adl->type == ADL_PLAYER_TECH && adl->vnum == ptech) {
					return abil;
				}
			}
		}
	}
	
	return NULL;	// if we got here
}


/**
* Gets numeric data from the ability, e.g. a RANGE value.
*
* @param ability_data *abil Which ability.
* @param int type Which ADL_ type, e.g. ADL_RANGE.
* @param bool allow_random If TRUE, picks at random when multiple values are set. If FALSE, returns the first match.
* @return int The data value if found, or 0 if none exists.
*/
int get_ability_data_value(ability_data *abil, int type, bool allow_random) {
	struct ability_data_list *adl;
	int found = 0, count = 0;
	
	LL_FOREACH(ABIL_DATA(abil), adl) {
		if (adl->type == type && !number(0, count++)) {
			found = adl->vnum;
			
			if (!allow_random) {
				break;	// only wanted first one
			}
		}
	}
	
	return found;
}


/**
* @param any_vnum vnum An ability vnum.
* @return char* The ability name, or "Unknown" if no match.
*/
char *get_ability_name_by_vnum(any_vnum vnum) {
	ability_data *abil = find_ability_by_vnum(vnum);
	return abil ? ABIL_NAME(abil) : "Unknown";
}


/**
* Gets the execution data for 1 type, from a larger set of data. If it isn't
* already in the list, this will add it.
*
* @param struct ability_exec *data The main data obj.
* @param bitvector_t type The ABILT_ const.
* @return struct ability_exec_type* The data entry (guaranteed).
*/
struct ability_exec_type *get_ability_type_data(struct ability_exec *data, bitvector_t type) {
	struct ability_exec_type *iter, *aet = NULL;
	
	LL_FOREACH(data->types, iter) {
		if (iter->type == type) {
			aet = iter;
			break;
		}
	}
	
	if (!aet) {
		CREATE(aet, struct ability_exec_type, 1);
		aet->type = type;
		LL_APPEND(data->types, aet);
	}
	
	return aet;
}


/**
* Displays the list of types for an ability.
*
* @param struct ability_type *list Pointer to the start of a list of types.
* @param char *save_buffer A buffer to store the result to.
* @param bool for_players If TRUE, shows the player version of the flags instead of the internal version.
*/
void get_ability_type_display(struct ability_type *list, char *save_buffer, bool for_players) {
	struct ability_type *at;
	char lbuf[256];
	int count = 0;
	
	*save_buffer = '\0';
	LL_FOREACH(list, at) {
		if (for_players) {
			prettier_sprintbit(at->type, ability_type_notes, lbuf);
			sprintf(save_buffer + strlen(save_buffer), "%s%s", (count++ > 0) ? ", " : "", lbuf);
		}
		else {
			sprintbit(at->type, ability_type_flags, lbuf, TRUE);
			sprintf(save_buffer + strlen(save_buffer), "%s%s(%d)", (count++ > 0) ? ", " : "", lbuf, at->weight);
		}
	}
	if (count == 0) {
		strcat(save_buffer, "none");
	}
}


/**
* Abilities which come from skills are capped by the player's skill level if
* they haven't specialized it to its max. When the player HAS capped the
* ability, their full level is used.
*
* @param char_data *ch The player.
* @param any_vnum abil_vnum Which ability.
* @return int Some level >= 1, based on the player, their skills, and the ability.
*/
int get_player_level_for_ability(char_data *ch, any_vnum abil_vnum) {
	int level, skill_level;
	ability_data *abil;
	skill_data *skl;
	
	if (!ch) {
		return 1;	// no work
	}
	
	// start here
	level = get_approximate_level(ch);
	
	if (abil_vnum == NO_ABIL || !(abil = find_ability_by_vnum(abil_vnum))) {
		return level;	// no abil = no adjustment
	}
	if (!(skl = ABIL_ASSIGNED_SKILL(abil)) || ABIL_IS_SYNERGY(abil)) {
		return level;	// no limit here either
	}
	
	skill_level = get_skill_level(ch, SKILL_VNUM(skl));
	
	// constrain only if player hasn't maxed this skill
	if (skill_level < SKILL_MAX_LEVEL(skl)) {
		level = MIN(level, skill_level);
	}
	
	return level;
}


/**
* Chooses a random room up to the ability's RANGE data. If there is no range
* data, it returns the current room. Does not return the current room if a
* valid range is available.
*
* @param char_data *ch The pperson lookin for a random room.
* @param ability_data *abil Which ability they are using.
* @return room_data* The random room, NULL if none found, or current room if range 0.
*/
room_data *get_random_room_for_ability(char_data *ch, ability_data *abil) {
	room_data *rand_room, *to_room = NULL;
	int rand_x, rand_y, range;
	int tries = 0;
	
	range = get_ability_data_value(abil, ADL_RANGE, TRUE);
	
	if (!range) {
		return IN_ROOM(ch);	// no range
	}
	
	while (!to_room && tries < 100) {
		// randomport!
		rand_x = number(0, 1) ? number(1, range) : number(-range, -1);
		rand_y = number(0, 1) ? number(1, range) : number(-range, -1);
		rand_room = real_shift(HOME_ROOM(IN_ROOM(ch)), rand_x, rand_y);
		
		// found outdoors! (we don't pick interiors at random)
		if (rand_room && !ROOM_IS_CLOSED(rand_room)) {
			to_room = rand_room;
		}
		++tries;
	}
	
	return to_room;	// if any
}


/**
* Gives a modifier (as a decimal, like 1.0 for 100%) based on a character's
* value in a given trait. For example, for Strength, 100% is the character's
* maximum possible strength.
*
* Not all traits have a maximum, so they can't all be used this way. This
* function returns 1.0 for traits like that.
*
* @param char_data *ch The character to check.
* @param int apply Any APPLY_ type, for which trait.
* @return double The modifier based on the trait (0 to 1.0).
*/
double get_trait_modifier(char_data *ch, int apply) {
	double value = 1.0;
	
	// APPLY_x: char's percent of max value for a trait
	switch (apply) {
		case APPLY_STRENGTH: {
			value = (double) GET_STRENGTH(ch) / att_max(ch);
			break;
		}
		case APPLY_DEXTERITY: {
			value = (double) GET_DEXTERITY(ch) / att_max(ch);
			break;
		}
		case APPLY_CHARISMA: {
			value = (double) GET_CHARISMA(ch) / att_max(ch);
			break;
		}
		case APPLY_GREATNESS: {
			value = (double) GET_GREATNESS(ch) / att_max(ch);
			break;
		}
		case APPLY_INTELLIGENCE: {
			value = (double) GET_INTELLIGENCE(ch) / att_max(ch);
			break;
		}
		case APPLY_WITS: {
			value = (double) GET_WITS(ch) / att_max(ch);
			break;
		}
		case APPLY_BLOCK: {
			value = 1.0;	// TODO: move block cap calculation to a function
			break;
		}
		case APPLY_TO_HIT: {
			value = 1.0;	// TODO: move to-hit cap calculation to a function
			break;
		}
		case APPLY_DODGE: {
			value = 1.0;	// TODO: move dodge cap calculation to a function
			break;
		}
		case APPLY_RESIST_PHYSICAL: {
			value = 1.0;	// TODO: move resist-phys cap calculation to a function
			break;
		}
		case APPLY_RESIST_MAGICAL: {
			value = 1.0;	// TODO: move resist-mag cap calculation to a function
			break;
		}
		
		// types that aren't really scalable like this always give 1.0
		default: {
			value = 1.0;
			break;
		}
	}
	
	return MAX(0, MIN(1.0, value));
}


/**
* Determines what percent of scale points go to one of an ability's types.
*
* @param ability_data *abil The ability to check.
* @param bitvector_t type The ABILT_ type to check.
* @return double The share of scale points (as a percent, 0 to 1.0) for that type.
*/
double get_type_modifier(ability_data *abil, bitvector_t type) {
	int total = 0, found = 0;
	struct ability_type *at;
	
	LL_FOREACH(ABIL_TYPE_LIST(abil), at) {
		total += at->weight;
		
		if (at->type == type) {
			found = at->weight;
		}
	}
	
	return (double)found / (double)MAX(total, 1);
}


/**
* Determines if an ability has ANY data entries of a given type.
*
* @param ability_data *abil Which ability.
* @param int type Which ADL_ type.
* @return bool TRUE if it has that data type, FALSE if not.
*/
bool has_ability_data_any(ability_data *abil, int type) {
	struct ability_data_list *adl;
	
	LL_FOREACH(ABIL_DATA(abil), adl) {
		if (adl->type == type) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* Determines if the player is in a matching role. This is always true if it's
* an NPC or if the ability doesn't have role flags.
*
* @param char_data *ch The player or npc.
* @param ability_data *abil The ability to check for a match.
* @param bool ignore_solo_check If TRUE, doesn't care if the player is alone for 'solo' abilities.
*/
bool has_matching_role(char_data *ch, ability_data *abil, bool ignore_solo_check) {
	if (IS_NPC(ch) || !ABILITY_FLAGGED(abil, ABILITY_ROLE_FLAGS)) {
		return TRUE;	// npc/no-role-required
	}
	if (ABILITY_FLAGGED(abil, ABILF_CASTER_ROLE) && (GET_CLASS_ROLE(ch) == ROLE_CASTER || GET_CLASS_ROLE(ch) == ROLE_SOLO) && (ignore_solo_check || check_solo_role(ch))) {
		return TRUE;
	}
	else if (ABILITY_FLAGGED(abil, ABILF_HEALER_ROLE) && (GET_CLASS_ROLE(ch) == ROLE_HEALER || GET_CLASS_ROLE(ch) == ROLE_SOLO) && (ignore_solo_check || check_solo_role(ch))) {
		return TRUE;
	}
	else if (ABILITY_FLAGGED(abil, ABILF_MELEE_ROLE) && (GET_CLASS_ROLE(ch) == ROLE_MELEE || GET_CLASS_ROLE(ch) == ROLE_SOLO) && (ignore_solo_check || check_solo_role(ch))) {
		return TRUE;
	}
	else if (ABILITY_FLAGGED(abil, ABILF_TANK_ROLE) && (GET_CLASS_ROLE(ch) == ROLE_TANK || GET_CLASS_ROLE(ch) == ROLE_SOLO) && (ignore_solo_check || check_solo_role(ch))) {
		return TRUE;
	}
	
	return FALSE;	// does not match
}


/**
* @param ability_data *abil An ability to check.
* @return bool TRUE if that ability is assigned to any class, or FALSE if not.
*/
bool is_class_ability(ability_data *abil) {
	class_data *class, *next_class;
	struct class_ability *clab;
	
	if (!abil) {
		return FALSE;
	}
	
	HASH_ITER(hh, class_table, class, next_class) {
		LL_SEARCH_SCALAR(CLASS_ABILITIES(class), clab, vnum, ABIL_VNUM(abil));
		if (clab) {
			return TRUE;
		}
	}
	
	return FALSE;	// no match
}


/**
* Loads a companion as a mob, from its companion_data entry. This places the
* mob in the room and runs load triggers, but does not send any messages of its
* own.
*
* @param char_data *leader The player summoning the companion.
* @param struct companion_data *cd The companion (data) to summon as a mob.
* @return char_data* The mob that was summoned, if possible (NULL otherwise).
*/
char_data *load_companion_mob(char_data *leader, struct companion_data *cd) {
	struct trig_proto_list *tp;
	struct trig_var_data *var;
	struct companion_mod *cm;
	char_data *mob, *proto;
	trig_data *trig;
	
	if (!leader || IS_NPC(leader) || !cd || !(proto = mob_proto(cd->vnum))) {
		return NULL;	// safety first
	}
	
	// get rid of any old companion
	despawn_companion(leader, NOTHING);
	
	// load mob
	mob = read_mobile(cd->vnum, cd->instantiated ? FALSE : TRUE);
	set_mob_flags(mob, MOB_NO_EXPERIENCE | MOB_SPAWNED | MOB_NO_EXPERIENCE | MOB_NO_LOOT);
	SET_BIT(AFF_FLAGS(mob), AFF_CHARM | AFF_NO_DRINK_BLOOD);
	setup_generic_npc(mob, GET_LOYALTY(leader), NOTHING, NOTHING);
	
	// CMOD_x: modify the companion
	LL_FOREACH(cd->mods, cm) {
		switch (cm->type) {
			case CMOD_SEX: {
				GET_REAL_SEX(mob) = cm->num;
				break;
			}
			case CMOD_KEYWORDS: {
				if (cm->str && *cm->str) {
					if (GET_PC_NAME(mob) && (!proto || GET_PC_NAME(mob) != GET_PC_NAME(proto))) {
						free(GET_PC_NAME(mob));
					}
					GET_PC_NAME(mob) = str_dup(cm->str);
				}
				break;
			}
			case CMOD_SHORT_DESC: {
				if (cm->str && *cm->str) {
					if (GET_SHORT_DESC(mob) && (!proto || GET_SHORT_DESC(mob) != GET_SHORT_DESC(proto))) {
						free(GET_SHORT_DESC(mob));
					}
					GET_SHORT_DESC(mob) = str_dup(cm->str);
				}
				break;
			}
			case CMOD_LONG_DESC: {
				if (cm->str && *cm->str) {
					if (GET_LONG_DESC(mob) && (!proto || GET_LONG_DESC(mob) != GET_LONG_DESC(proto))) {
						free(GET_LONG_DESC(mob));
					}
					GET_LONG_DESC(mob) = str_dup(cm->str);
				}
				break;
			}
			case CMOD_LOOK_DESC: {
				if (cm->str && *cm->str) {
					if (GET_LOOK_DESC(mob) && (!proto || GET_LOOK_DESC(mob) != GET_LOOK_DESC(proto))) {
						free(GET_LOOK_DESC(mob));
					}
					GET_LOOK_DESC(mob) = str_dup(cm->str);
				}
				break;
			}
		}
	}
	
	// re-attach scripts
	if (!SCRIPT(mob)) {
		create_script_data(mob, MOB_TRIGGER);
	}
	LL_FOREACH(cd->scripts, tp) {
		if ((trig = read_trigger(tp->vnum))) {
			add_trigger(SCRIPT(mob), trig, -1);
		}
	}
	
	// re-attach script vars
	LL_FOREACH(cd->vars, var) {
		add_var(&(SCRIPT(mob)->global_vars), var->name, var->value, var->context);
	}
	
	// set companion data
	GET_COMPANION(leader) = mob;
	GET_COMPANION(mob) = leader;
	GET_LAST_COMPANION(leader) = GET_MOB_VNUM(mob);
	
	// for new mobs, ensure any data is saved back
	if (!cd->instantiated) {
		// sex/string changes
		if (GET_REAL_SEX(mob) != GET_REAL_SEX(proto)) {
			add_companion_mod(cd, CMOD_SEX, GET_REAL_SEX(mob), NULL);
		}
		if (GET_PC_NAME(mob) != GET_PC_NAME(proto)) {
			add_companion_mod(cd, CMOD_KEYWORDS, 0, GET_PC_NAME(mob));
		}
		if (GET_SHORT_DESC(mob) != GET_SHORT_DESC(proto)) {
			add_companion_mod(cd, CMOD_SHORT_DESC, 0, GET_SHORT_DESC(mob));
		}
		if (GET_LONG_DESC(mob) != GET_LONG_DESC(proto)) {
			add_companion_mod(cd, CMOD_LONG_DESC, 0, GET_LONG_DESC(mob));
		}
		if (GET_LOOK_DESC(mob) != GET_LOOK_DESC(proto)) {
			add_companion_mod(cd, CMOD_LOOK_DESC, 0, GET_LOOK_DESC(mob));
		}
		
		// save scripts
		reread_companion_trigs(mob);
		
		// mark as instantiated
		cd->instantiated = TRUE;
		queue_delayed_update(leader, CDU_SAVE);
	}
	
	// scale to summoner
	scale_mob_as_companion(mob, leader, get_player_level_for_ability(leader, cd->from_abil));
	char_to_room(mob, IN_ROOM(leader));
	add_follower(mob, leader, FALSE);
	
	// triggers last
	load_mtrigger(mob);
	
	return mob;
}


/**
* For abilities stored to the character as action data, attempts to find and
* validate targets again.
*
* @param char_data *ch The player doing the ability.
* @param ability_data *abil Which ability.
* @param char_data **vict Will bind a character target here, if any. (Optional)
* @param obj_data **ovict Will bind an object target here, if any. (Optional)
* @param vehicle_data **vvict Will bind a vehicle target here, if any. (Optional)
* @param room_data **room_targ Will bind a room target here, if any. (Optional)
* @return bool TRUE if targets were ok, FALSE if they are invalid.
*/
bool redetect_ability_targets(char_data *ch, ability_data *abil, char_data **vict, obj_data **ovict, vehicle_data **vvict, room_data **room_targ) {
	bool any_targ = FALSE, match;
	
	// init these first
	if (vict) {
		*vict = NULL;
	}
	if (ovict) {
		*ovict = NULL;
	}
	if (vvict) {
		*vvict = NULL;
	}
	if (room_targ) {
		*room_targ = NULL;
	}
	
	// validate character first
	if (!ch || IS_NPC(ch)) {
		return FALSE;	// failure
	}
	
	// pull existing data first
	if (vict) {
		*vict = GET_ACTION_CHAR_TARG(ch);
	}
	if (ovict) {
		*ovict = GET_ACTION_OBJ_TARG(ch);
	}
	if (vvict) {
		*vvict = GET_ACTION_VEH_TARG(ch);
	}
	if (room_targ) {
		*room_targ = real_room(GET_ACTION_ROOM_TARG(ch));
	}
	
	// ATAR_x: attempt to validate
	if (abil) {
		// check char targets
		if (vict && *vict) {
			any_targ = TRUE;
			
			if (IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_ROOM) && !IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_CLOSEST | ATAR_CHAR_WORLD) && IN_ROOM(*vict) != IN_ROOM(ch)) {
				return FALSE;	// wrong room
			}
		}
		
		// check obj targets
		if (ovict && *ovict) {
			any_targ = TRUE;
			
			// obj location
			if (IS_SET(ABIL_TARGETS(abil), OBJ_ATARS)) {
				match = FALSE;
				match |= (IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_WORLD) ? TRUE : FALSE);
				if (IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_INV) && (*ovict)->carried_by == ch) {
					match = TRUE;	// ok: carried
				}
				if (IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_EQUIP) && (*ovict)->worn_by == ch) {
					match = TRUE;	// ok: worn
				}
				if (IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_ROOM) && IN_ROOM(*ovict) == IN_ROOM(ch)) {
					match = TRUE;	// ok: in room
				}
				if (!match) {
					return FALSE;	// bad location
				}
			}
		}
		
		// check vehicle targets
		if (vvict && *vvict) {
			any_targ = TRUE;
			
			if (IS_SET(ABIL_TARGETS(abil), ATAR_VEH_ROOM) && !IS_SET(ABIL_TARGETS(abil), ATAR_VEH_WORLD) && IN_ROOM(*vvict) != IN_ROOM(ch)) {
				return FALSE;	// wrong location
			}
		}
		
		// room targets
		if (room_targ && *room_targ) {
			any_targ = TRUE;
			
			match = FALSE;
			if (IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_COORDS)) {
				match = TRUE;	// ok: any coords
			}
			if (IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_ADJACENT | ATAR_ROOM_EXIT) && *room_targ != IN_ROOM(ch) && compute_distance(IN_ROOM(ch), *room_targ) < 2) {
				match = TRUE;	// ok: adjacent/exit, not here, within 2 distance
			}
			if (IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_HERE) && *room_targ == IN_ROOM(ch)) {
				match = TRUE;	// ok: same room
			}
			if (IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_HOME) && ROOM_PRIVATE_OWNER(HOME_ROOM(*room_targ)) == GET_IDNUM(ch)) {
				match = TRUE;	// ok: is home
			}
			if (IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_CITY) && ROOM_OWNER(*room_targ) == GET_LOYALTY(ch) && IS_CITY_CENTER(*room_targ)) {
				match = TRUE;	// ok: is home
			}
			if (IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_RANDOM) && compute_distance(IN_ROOM(ch), *room_targ) <= get_ability_data_value(abil, ADL_RANGE, TRUE)) {
				match = TRUE;	// ok: random and within range
			}
			
			if (!match) {
				return FALSE;	// bad room
			}
		}
		
		// Lastly: did we need a targ but lack one?
		if (IS_SET(ABIL_TARGETS(abil), ATAR_IGNORE)) {
			return TRUE;
		}
		else if (!any_targ && (!IS_SET(ABIL_TARGETS(abil), CHAR_ATARS) || !vict || !*vict) && (!IS_SET(ABIL_TARGETS(abil), OBJ_ATARS) || !ovict || !*ovict) && (!IS_SET(ABIL_TARGETS(abil), VEH_ATARS) || !vvict || !*vvict) && (!IS_SET(ABIL_TARGETS(abil), ROOM_ATARS) || !room_targ || !*room_targ)) {
			return FALSE;	// should have had at least one target
		}
	}
	
	return TRUE;	// if we got here it's fine
}


/**
* Removes all the passive buffs on a character and re-applies them. This can be
* called at startup, when the player gains an ability, or when a player gains
* a level (changes effect scaling).
*
* @param char_data *ch The player to refresh passive ability buffs on.
*/
void refresh_passive_buffs(char_data *ch) {
	struct player_ability_data *plab, *next_plab;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	// remove old ones
	while (GET_PASSIVE_BUFFS(ch)) {
		remove_passive_buff(ch, GET_PASSIVE_BUFFS(ch));
	}
	
	// re-add
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		if (!plab->ptr || !IS_SET(ABIL_TYPES(plab->ptr), ABILT_PASSIVE_BUFF)) {
			continue;	// not a passive buff
		}
		if (!plab->purchased[GET_CURRENT_SKILL_SET(ch)]) {
			continue;	// wrong skill set
		}
		
		// apply it
		apply_one_passive_buff(ch, plab->ptr);
	}
	
	// and finish
	affect_total(ch);
}


/**
* Removes 1 passive buff from a player. You should affect_total() when you're
* done with this.
*
* @param char_data *ch The player.
* @param struct affected_type *aff The passive buff affect to remove.
*/
void remove_passive_buff(char_data *ch, struct affected_type *aff) {
	if (ch && !IS_NPC(ch) && aff) {
		// NOTE: does not use affected_type->expire_event
		affect_modify(ch, aff->location, aff->modifier, aff->bitvector, FALSE);
		LL_DELETE(GET_PASSIVE_BUFFS(ch), aff);
		free(aff);
	}
}


/**
* Removes all passive buffs by ability vnum. You should affect_total() when
* you're done removing abilities.
*
* @param char_data *ch The player.
* @param any_vnum abil Which ability granted the passive buff.
*/
void remove_passive_buff_by_ability(char_data *ch, any_vnum abil) {
	struct affected_type *aff, *next;
	
	if (!IS_NPC(ch)) {
		LL_FOREACH_SAFE(GET_PASSIVE_BUFFS(ch), aff, next) {
			if (aff->type == abil) {
				remove_passive_buff(ch, aff);
			}
		}
	}
}


/**
* Removes a type from an ability and re-summarizes the type flags.
*
* @param ability_data *abil The ability to remove a type from.
* @param bitvector_t type The ABILT_ flag to remove.
*/
void remove_type_from_ability(ability_data *abil, bitvector_t type) {
	struct ability_type *at, *next_at;
	bitvector_t total = NOBITS;
	
	LL_FOREACH_SAFE(ABIL_TYPE_LIST(abil), at, next_at) {
		if (at->type == type) {
			LL_DELETE(ABIL_TYPE_LIST(abil), at);
			free(at);
		}
		else {
			total |= at->type;
		}
	}
	ABIL_TYPES(abil) = total;	// summarize flags
}


/**
* Triggers ability gains by type.
*
* @param char_data *ch The person to try to gain exp.
* @param char_data *opponent Optional: the opponent, if any (may be null).
* @param bitvector_t trigger Which AGH_ event (only 1 at a time).
*/
void run_ability_gain_hooks(char_data *ch, char_data *opponent, bitvector_t trigger) {
	struct ability_gain_hook *agh, *next_agh;
	struct ability_data_list *adl;
	ability_data *abil;
	double amount;
	bool found;
	int pos;
	
	// list of slots to check: terminate with -1
	int check_ready_weapon_slots[] = { WEAR_WIELD, WEAR_HOLD, WEAR_RANGED, -1 };
	
	if (!ch || IS_NPC(ch)) {
		return;
	}
	
	// AGH_x: gain amount based on trigger type
	switch (trigger) {
		case AGH_PASSIVE_FREQUENT: {
			amount = 0.5;
			break;
		}
		case AGH_MOVING: {
			amount = 1;
			break;
		}
		case AGH_VAMPIRE_FEEDING: {
			amount = 5;
			break;
		}
		case AGH_DYING: {
			// things that gain on death REALLY gain
			amount = 100;
			break;
		}
		case AGH_MELEE:
		case AGH_RANGED:
		case AGH_DODGE:
		case AGH_BLOCK:
		case AGH_TAKE_DAMAGE:
		case AGH_PASSIVE_HOURLY:
		default: {
			amount = 2;
			break;
		}
	}
	
	HASH_ITER(hh, GET_ABILITY_GAIN_HOOKS(ch), agh, next_agh) {
		if (!IS_SET(agh->triggers, trigger)) {
			continue;	// wrong trigger type
		}
		if (!has_ability(ch, agh->ability)) {
			continue;	// not currently having it
		}
		if (IS_SET(agh->triggers, AGH_NOT_WHILE_ASLEEP) && GET_POS(ch) <= POS_SLEEPING) {
			continue;	// person is sleeping
		}
		if (IS_SET(agh->triggers, AGH_ONLY_WHEN_AFFECTED) && (!(abil = find_ability_by_vnum(agh->ability)) || !affected_by_spell(ch, ABIL_AFFECT_VNUM(abil)))) {
			continue;	// not currently affected
		}
		if (IS_SET(agh->triggers, AGH_ONLY_DARK) && room_is_light(IN_ROOM(ch), TRUE, CAN_SEE_IN_MAGIC_DARKNESS(ch))) {
			continue;	// not dark
		}
		if (IS_SET(agh->triggers, AGH_ONLY_LIGHT) && !room_is_light(IN_ROOM(ch), TRUE, CAN_SEE_IN_MAGIC_DARKNESS(ch))) {
			continue;	// not light
		}
		if (IS_SET(agh->triggers, AGH_ONLY_VS_ANIMAL) && (!opponent || !MOB_FLAGGED(opponent, MOB_ANIMAL))) {
			continue;	// not an animal
		}
		if (IS_SET(agh->triggers, AGH_ONLY_USING_READY_WEAPON)) {
			// check if using a readied item
			found = FALSE;
			if ((abil = find_ability_by_vnum(agh->ability))) {
				LL_FOREACH(ABIL_DATA(abil), adl) {
					if (found) {
						break;	// exit early
					}
					if (adl->type != ADL_READY_WEAPON) {
						continue;
					}
					
					// is the player using the item in any slot
					for (pos = 0; check_ready_weapon_slots[pos] != -1 && !found; ++pos) {
						if (GET_EQ(ch, check_ready_weapon_slots[pos]) && GET_OBJ_VNUM(GET_EQ(ch, check_ready_weapon_slots[pos])) == adl->vnum) {
							found = TRUE;
						}
					}
				}
			}
			
			if (!found) {
				continue;	// not using the item
			}
		}
		if (IS_SET(agh->triggers, AGH_ONLY_USING_COMPANION)) {
			// check if using a cmpanion
			found = FALSE;
			if (GET_COMPANION(ch) && (abil = find_ability_by_vnum(agh->ability))) {
				LL_FOREACH(ABIL_DATA(abil), adl) {
					if (adl->type == ADL_SUMMON_MOB && adl->vnum == GET_MOB_VNUM(GET_COMPANION(ch))) {
						found = TRUE;
						break;
					}
				}
			}
			if (!found) {
				continue;	// not using the companion
			}
		}
		
		gain_ability_exp(ch, agh->ability, amount);
		run_ability_hooks(ch, AHOOK_ABILITY, agh->ability, opponent, NULL, NULL, NULL);
	}
}


/**
* Ensures a player has companion entries for all the companions granted by
* his/her abilities.
*
* @param char_data *ch The player.
*/
void setup_ability_companions(char_data *ch) {
	struct player_ability_data *plab, *next_plab;
	struct ability_data_list *adl;
	ability_data *abil;
	
	if (!ch || IS_NPC(ch)) {
		return;	// no work
	}
	
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		abil = plab->ptr;
		if (!abil || !plab->purchased[GET_CURRENT_SKILL_SET(ch)] || !IS_SET(ABIL_TYPES(abil), ABILT_COMPANION)) {
			continue;
		}
		
		LL_FOREACH(ABIL_DATA(abil), adl) {
			if (adl->type == ADL_SUMMON_MOB) {
				add_companion(ch, adl->vnum, ABIL_VNUM(abil));
			}
		}
	}
}


/**
* The standard number of scaling points for an ability.
*
* @param char_data *ch The user of the ability.
* @param ability_data *abil The ability being used.
* @param int level The level we're using the ability at.
* @param bitvector_t type The ABILT_ to modify for, if any.
* @param struct ability_exec *data The ability data being passed around.
*/
double standard_ability_scale(char_data *ch, ability_data *abil, int level, bitvector_t type, struct ability_exec *data) {
	double points;
	
	// determine points
	points = level / 100.0 * config_get_double("scale_points_at_100");
	if (type) {
		points *= get_type_modifier(abil, type);
	}
	points *= ABIL_SCALE(abil);
	if (ABIL_LINKED_TRAIT(abil) != APPLY_NONE) {
		// trait range is 0.75x to 1.25x
		points *= 0.75 + (get_trait_modifier(ch, ABIL_LINKED_TRAIT(abil)) / 2.0);
	}
	
	if (!IS_NPC(ch) && ABILITY_FLAGGED(abil, ABILITY_ROLE_FLAGS)) {
		points *= data->matching_role ? 1.20 : 0.80;
	}
	
	return MAX(1.0, points);	// ensure minimum of 1 point
}


/**
* Validates the targets of an ability when first used. Also re-validates on
* over-time abilities.
*
* @param char_data *ch The person using the ability.
* @paran ability_data *abil Which ability.
* @param char_data *vict Optional: The character target (may be NULL if not used).
* @param obj_data *ovict Optional: Object target (may be NULL).
* @param vehicle_data *vvict Optional: Vehicle target (may be NULL).
* @param room_data *room_targ Optional: The room target (may be NULL if not used).
* @param bool send_msgs If TRUE, sends a message on failure. If FALSE, is silent.
* @return bool TRUE if ok, FALSE if failed.
*/
bool validate_ability_target(char_data *ch, ability_data *abil, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, bool send_msgs) {
	if (!ch || !abil) {
		return FALSE;	// missing abil
	}
	
	if ((vict == ch) && ABIL_IS_VIOLENT(abil)) {
		if (send_msgs) {
			msg_to_char(ch, "You can't %s yourself -- that could be bad for your health!\r\n", ABIL_COMMAND(abil) ? ABIL_COMMAND(abil) : "use that ability on");
		}
		return FALSE;
	}
	if ((vict == ch) && IS_SET(ABIL_TARGETS(abil), ATAR_NOT_SELF)) {
		if (send_msgs) {
			msg_to_char(ch, "You cannot use that on yourself!\r\n");
		}
		return FALSE;
	}
	if (vict && (vict != ch) && IS_SET(ABIL_TARGETS(abil), ATAR_SELF_ONLY)) {
		if (send_msgs) {
			msg_to_char(ch, "You can only use that on yourself!\r\n");
		}
		return FALSE;
	}
	if (vict && ABIL_IS_VIOLENT(abil) && AFF_FLAGGED(ch, AFF_CHARM) && (GET_LEADER(ch) == vict)) {
		if (send_msgs) {
			msg_to_char(ch, "You are afraid you might hurt your leader!\r\n");
		}
		return FALSE;
	}
	if (vict && vict != ch && ABIL_IS_VIOLENT(abil)) {
		if (!can_fight(ch, vict)) {
			if (send_msgs) {
				act("You can't attack $N!", FALSE, ch, NULL, vict, TO_CHAR);
			}
			return FALSE;
		}
		if (!ABILITY_FLAGGED(abil, ABILF_RANGED | ABILF_RANGED_ONLY) && NOT_MELEE_RANGE(ch, vict)) {
			if (send_msgs) {
				msg_to_char(ch, "You need to be at melee range to do this.\r\n");
			}
			return FALSE;
		}
		if (ABILITY_FLAGGED(abil, ABILF_RANGED_ONLY) && !NOT_MELEE_RANGE(ch, vict) && FIGHTING(ch)) {
			if (send_msgs) {
				msg_to_char(ch, "You need to be in ranged combat to do this.\r\n");
			}
			return FALSE;
		}
	}
	if (room_targ && room_targ != IN_ROOM(ch) && IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_HERE)) {
		if (send_msgs) {
			msg_to_char(ch, "You have to use it on the room you're in.\r\n");
		}
		return FALSE;
	}
	if (!check_ability_limitations(ch, abil, vict, ovict, vvict, room_targ)) {
		// sends own message when false
		return FALSE;
	}
	/* TODO: if group abilities are added
	if (IS_SET(ABIL_TYPES(abil), ABILT_GROUPS) && !GROUP(ch)) {
		msg_to_char(ch, "You can't do that if you're not in a group!\r\n");
		return;
	}
	*/
	
	// looks ok
	return TRUE;
}


/**
* Counts the words of text in an ability's strings.
*
* @param ability_data *abil The ability whose strings to count.
* @return int The number of words in the ability's strings.
*/
int wordcount_ability(ability_data *abil) {
	int count = 0;
	
	count += wordcount_string(ABIL_NAME(abil));
	count += wordcount_string(ABIL_COMMAND(abil));
	count += wordcount_custom_messages(ABIL_CUSTOM_MSGS(abil));
	
	return count;
}


 //////////////////////////////////////////////////////////////////////////////
//// ABILITY ACTIONS /////////////////////////////////////////////////////////

// DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
DO_ABIL(abil_action_close_portal) {
	obj_data *obj, *reverse = NULL;
	room_data *to_room;

	if (ovict && IS_PORTAL(ovict) && GET_OBJ_TIMER(ovict) != UNLIMITED && (to_room = real_room(GET_PORTAL_TARGET_VNUM(ovict)))) {
		// find the reverse ovict
		DL_FOREACH2(ROOM_CONTENTS(to_room), obj, next_content) {
			if (GET_PORTAL_TARGET_VNUM(obj) == GET_ROOM_VNUM(IN_ROOM(ch))) {
				reverse = obj;
				break;
			}
		}
		
		// remove it
		send_ability_per_item_messages(ch, ovict, 1, abil, data, NULL);
		extract_obj(ovict);
		
		if (reverse) {
			if (ROOM_PEOPLE(to_room)) {
				act("$p is closed from the other side!", FALSE, ROOM_PEOPLE(to_room), reverse, NULL, TO_CHAR | TO_ROOM);
			}
			extract_obj(reverse);
		}
		
		data->success = TRUE;
		data->stop = TRUE;	// target is gone
	}
}


// DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
DO_ABIL(abil_action_detect_adventures_around) {
	bool wait;
	char where_str[256];
	char *dir_str, *repl_array[2];
	int adv_dist;
	struct instance_data *inst;
	struct empire_city_data *match_city;
	room_data *loc;
	
	if (!room_targ) {
		return;	// nothing to do
	}
	
	adv_dist = get_ability_data_value(abil, ADL_RANGE, TRUE);
	if (!adv_dist) {
		adv_dist = 50 + GET_EXTRA_ATT(ch, ATT_NEARBY_RANGE);	// default
	}
	
	match_city = (ROOM_OWNER(room_targ) && is_in_city_for_empire(room_targ, ROOM_OWNER(room_targ), TRUE, &wait)) ? find_city(ROOM_OWNER(room_targ), room_targ) : NULL;
	adv_dist = MAX(0, adv_dist);
	DL_FOREACH(instance_list, inst) {
		loc = INST_FAKE_LOC(inst);
		if (!loc || INSTANCE_FLAGGED(inst, INST_COMPLETED)) {
			continue;	// no location, or completed already
		}
		if (!ADVENTURE_FLAGGED(INST_ADVENTURE(inst), ADV_DETECTABLE)) {
			continue;	// not detectable
		}
		if ((!match_city || find_city(ROOM_OWNER(room_targ), loc) != match_city) && compute_distance(room_targ, loc) > adv_dist) {
			continue;	// wrong city and too far
		}
		
		// show instance
		data->success = TRUE;
		dir_str = get_partial_direction_to(ch, IN_ROOM(ch), loc, (PRF_FLAGGED(ch, PRF_SCREEN_READER) ? FALSE : TRUE));
		snprintf(where_str, sizeof(where_str), "%s%s - %d %s", get_room_name(loc, FALSE), coord_display_room(ch, loc, FALSE), compute_distance(IN_ROOM(ch), loc), (dir_str && *dir_str) ? dir_str : "away");
		
		repl_array[0] = where_str;
		repl_array[1] = GET_ADV_NAME(INST_ADVENTURE(inst));
		send_ability_special_messages(ch, NULL, NULL, abil, data, repl_array, 2);
	}
}


// DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
DO_ABIL(abil_action_detect_earthmeld) {
	char_data *targ;
	
	if (!room_targ) {
		return;	// no work
	}
	
	DL_FOREACH2(ROOM_PEOPLE(room_targ), targ, next_in_room) {
		if (targ == ch) {
			continue;
		}
		
		if (AFF_FLAGGED(targ, AFF_EARTHMELD)) {
			if (CAN_SEE(ch, targ)) {
				send_ability_per_char_messages(ch, targ, 1, abil, data, NULL);
				data->success = TRUE;
			}
		}
	}
}


// DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
DO_ABIL(abil_action_detect_hide) {
	char_data *targ;
	bool had_sense;
	
	if (!room_targ) {
		return;	// nothing to do
	}
	
	had_sense = AFF_FLAGGED(ch, AFF_SENSE_HIDE) ? TRUE : FALSE;
	
	DL_FOREACH2(ROOM_PEOPLE(room_targ), targ, next_in_room) {
		if (targ == ch) {
			continue;
		}
		
		if (AFF_FLAGGED(targ, AFF_HIDE)) {
			// hidden target
			SET_BIT(AFF_FLAGS(ch), AFF_SENSE_HIDE);

			if (CAN_SEE(ch, targ)) {
				send_ability_per_char_messages(ch, targ, 1, abil, data, NULL);
				REMOVE_BIT(AFF_FLAGS(targ), AFF_HIDE);
				affects_from_char_by_aff_flag(targ, AFF_HIDE, TRUE);
				data->success = TRUE;
			}
			
			if (!had_sense) {
				REMOVE_BIT(AFF_FLAGS(ch), AFF_SENSE_HIDE);
			}
		}
	}
}


// DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
DO_ABIL(abil_action_detect_players_around) {
	bool wait;
	char where_str[256];
	char *dir_str;
	int player_dist;
	char_data *ch_iter;
	struct empire_city_data *match_city;
	
	if (!room_targ) {
		return;	// no work
	}
	
	player_dist = get_ability_data_value(abil, ADL_RANGE, TRUE);
	if (!player_dist) {
		// default
		player_dist = GET_EXTRA_ATT(ch, ATT_WHERE_RANGE) + (has_player_tech(ch, PTECH_WHERE_UPGRADE) ? 75 : 20);
	}
	
	match_city = (ROOM_OWNER(room_targ) && is_in_city_for_empire(room_targ, ROOM_OWNER(room_targ), TRUE, &wait)) ? find_city(ROOM_OWNER(room_targ), room_targ) : NULL;
	player_dist = MAX(0, player_dist);
	DL_FOREACH2(player_character_list, ch_iter, next_plr) {
		if (ch_iter == ch || IS_IMMORTAL(ch_iter)) {
			continue;	// skip these
		}
		if ((!match_city || find_city(ROOM_OWNER(room_targ), IN_ROOM(ch_iter)) != match_city) && compute_distance(room_targ, IN_ROOM(ch_iter)) > player_dist) {
			continue;	// not same city and too far
		}

		// ok:
		data->success = TRUE;
		dir_str = get_partial_direction_to(ch, IN_ROOM(ch), IN_ROOM(ch_iter), (PRF_FLAGGED(ch, PRF_SCREEN_READER) ? FALSE : TRUE));
		snprintf(where_str, sizeof(where_str), "%s%s - %d %s", get_room_name(IN_ROOM(ch_iter), FALSE), coord_display_room(ch, IN_ROOM(ch_iter), FALSE), compute_distance(IN_ROOM(ch), IN_ROOM(ch_iter)), (dir_str && *dir_str) ? dir_str : "away");
		send_ability_per_char_messages(ch, ch_iter, 1, abil, data, where_str);
	}
}


// DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
DO_ABIL(abil_action_devastate_area) {
	room_data *rand_room, *to_room = NULL;
	crop_data *cp;
	int dist, iter;
	int x, y, range;
	
	#define CAN_DEVASTATE(room)  (((ROOM_SECT_FLAGGED((room), SECTF_HAS_CROP_DATA) && has_permission(ch, PRIV_HARVEST, room)) || (CAN_CHOP_ROOM(room) && has_permission(ch, PRIV_CHOP, room) && get_depletion((room), DPLTN_CHOP) < config_get_int("chop_depletion"))) && !ROOM_AFF_FLAGGED((room), ROOM_AFF_HAS_INSTANCE | ROOM_AFF_NO_EVOLVE))
	
	range = get_ability_data_value(abil, ADL_RANGE, TRUE);
	if (!range) {
		range = 3;	// default
	}
	
	if (!room_targ) {
		return;	// nothing to do
	}

	// check this room
	if (CAN_DEVASTATE(room_targ) && can_use_room(ch, room_targ, MEMBERS_ONLY)) {
		to_room = room_targ;
	}
	
	// check surrounding rooms in star patter
	for (dist = 1; !to_room && dist <= range; ++dist) {
		for (iter = 0; !to_room && iter < NUM_2D_DIRS; ++iter) {
			rand_room = real_shift(room_targ, shift_dir[iter][0] * dist, shift_dir[iter][1] * dist);
			if (rand_room && CAN_DEVASTATE(rand_room) && compute_distance(room_targ, rand_room) <= range && can_use_room(ch, rand_room, MEMBERS_ONLY)) {
				to_room = rand_room;
			}
		}
	}
	
	// check max radius
	for (x = -range; !to_room && x <= range; ++x) {
		for (y = -range; !to_room && y <= range; ++y) {
			rand_room = real_shift(room_targ, x, y);
			if (rand_room && CAN_DEVASTATE(rand_room) && compute_distance(room_targ, rand_room) <= range && can_use_room(ch, rand_room, MEMBERS_ONLY)) {
				to_room = rand_room;
			}
		}
	}
	
	// SUCCESS: distribute resources
	if (to_room) {
		if (ROOM_SECT_FLAGGED(to_room, SECTF_CROP) && (cp = ROOM_CROP(to_room)) && has_interaction(GET_CROP_INTERACTIONS(cp), INTERACT_HARVEST)) {
			run_room_interactions(ch, to_room, INTERACT_HARVEST, NULL, MEMBERS_ONLY, devastate_crop);
			run_room_interactions(ch, to_room, INTERACT_CHOP, NULL, MEMBERS_ONLY, devastate_trees);
			uncrop_tile(to_room);
			data->success = TRUE;
		}
		else if (CAN_CHOP_ROOM(to_room) && get_depletion(to_room, DPLTN_CHOP) < config_get_int("chop_depletion")) {
			run_room_interactions(ch, to_room, INTERACT_CHOP, NULL, MEMBERS_ONLY, devastate_trees);
			change_chop_territory(to_room);
			data->success = TRUE;
		}
		else if (ROOM_SECT_FLAGGED(to_room, SECTF_HAS_CROP_DATA) && (cp = ROOM_CROP(to_room))) {
			msg_to_char(ch, "You devastate a seeded field!\r\n");
			if (ROOM_PEOPLE(to_room)) {
				act("The seeded field is devastated!", FALSE, ROOM_PEOPLE(to_room), NULL, ch, TO_CHAR | TO_NOTVICT);
			}
			
			// check to default sect
			uncrop_tile(to_room);
			data->success = TRUE;
		}
		else {
			// fail -- end without success
		}
	}
	// else no room = no sucess
}


// DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
DO_ABIL(abil_action_magic_growth) {
	char was_name[256];
	char *repl_array[2];
	sector_data *preserve;
	struct evolution_data *evo;
	
	// percentage is checked in the evolution data
	if ((evo = get_evolution_by_type(SECT(room_targ), EVO_MAGIC_GROWTH)) && !ROOM_AFF_FLAGGED(room_targ, ROOM_AFF_NO_EVOLVE)) {
		preserve = (BASE_SECT(room_targ) != SECT(room_targ)) ? BASE_SECT(room_targ) : NULL;
		snprintf(was_name, sizeof(was_name), "%s", GET_SECT_NAME(SECT(room_targ)));
		
		change_terrain(room_targ, evo->becomes, preserve ? GET_SECT_VNUM(preserve) : NOTHING);
		
		repl_array[0] = was_name;
		repl_array[1] = GET_SECT_NAME(SECT(room_targ));
		send_ability_special_messages(ch, NULL, ovict, abil, data, repl_array, 2);
		
		remove_depletion(room_targ, DPLTN_PICK);
		remove_depletion(room_targ, DPLTN_FORAGE);
	}
	
	// always a success (no expected outcome)
	data->success = TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// ABILITY EFFECTS AND LIMITS //////////////////////////////////////////////

/**
* For abilities with 'effects' data, this function manages them. This function
* is silent unless an effect happens.
*
* @param ability_dta *abil The ability being used.
* @param char_data *ch The character using the ability.
* @param char_data *vict Optional: Character target (may be NULL).
* @param obj_data *ovict Optional: Object target (may be NULL).
* @param vehicle_data *vvict Optional: Vehicle target (may be NULL).
* @param room_data *room_targ Optional: For room-targeting abilities, must provide the room (may be NULL).
*/
void apply_ability_effects(ability_data *abil, char_data *ch, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ) {
	struct ability_data_list *data;
	
	if (!abil || !ch) {
		return;	// no harm
	}
	
	LL_FOREACH(ABIL_DATA(abil), data) {
		if (data->type != ADL_EFFECT) {
			continue;
		}
		
		// ABIL_EFFECT_x: applying the effect
		switch (data->vnum) {
			case ABIL_EFFECT_DISMOUNT: {
				if (IS_RIDING(ch)) {
					msg_to_char(ch, "You climb down from your mount.\r\n");
					act("$n climbs down from $s mount.", TRUE, ch, NULL, NULL, TO_ROOM);
					perform_dismount(ch);
				}
				break;
			}
			case ABIL_EFFECT_DISTRUST_FROM_HOSTILE: {
				empire_data *emp = NULL;
				
				if (vict && GET_LOYALTY(vict)) {
					emp = GET_LOYALTY(vict);
				}
				else if (vvict && VEH_OWNER(vvict)) {
					emp = VEH_OWNER(vvict);
				}
				else if (room_targ && ROOM_OWNER(room_targ)) {
					emp = ROOM_OWNER(vvict);
				}
				
				// if any
				if (emp && emp != GET_LOYALTY(ch) && !IS_NPC(ch)) {
					trigger_distrust_from_hostile(ch, emp);
				}
				
				break;
			}
		}
	}
}


/**
* Determines if a player can use an ability based on its limitations. Sends a
* message in case of failure, but not on success.
*
* @param char_data *ch The player using the ability.
* @param ability_data *abil Which ability.
* @param char_data *vict Optional: Character target (may be NULL).
* @param obj_data *ovict Optional: Object target (may be NULL).
* @param vehicle_data *vvict Optional: Vehicle target (may be NULL).
* @param room_data *room_targ Optional: For room-targeting abilities, must provide the room (may be NULL).
* @return bool TRUE if ok, FALSE if there was a problem. Only sends a message if it was false.
*/
bool check_ability_limitations(char_data *ch, ability_data *abil, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ) {
	char part[256];
	struct ability_data_list *adl;
	room_data *any_room, *other_room;
	
	// some types just require any match
	bool want_item_type = FALSE, have_item_type = FALSE;
	bool want_room_permission = FALSE, have_room_permission = FALSE;
	bool want_weapon = FALSE, have_weapon = FALSE;
	char item_type_error[256], room_permission_error[256], weapon_error[256];
	
	if (!ch || !abil) {
		return FALSE;	// no work
	}
	
	// inits
	*weapon_error = '\0';
	*item_type_error = '\0';
	*room_permission_error = '\0';
	
	// any_room is the target room IF it targets a room, or else ch's room
	any_room = room_targ ? room_targ : IN_ROOM(ch);
	
	// other_room is the room the target is in, for all types
	other_room = (room_targ ? room_targ : (vict ? IN_ROOM(vict) : (vvict ? IN_ROOM(vvict) : ovict ? obj_room(ovict) : IN_ROOM(ch))));
	
	LL_FOREACH(ABIL_DATA(abil), adl) {
		if (adl->type != ADL_LIMITATION) {
			continue;
		}
		
		// ABIL_LIMIT_x:
		switch (adl->vnum) {
			case ABIL_LIMIT_ON_BARRIER: {
				if (!ROOM_BLD_FLAGGED(any_room, BLD_BARRIER)) {
					msg_to_char(ch, "You need to do that on a barrier or wall of some kind.\r\n");
					return FALSE;
				}
				else if (!IS_COMPLETE(any_room)) {
					msg_to_char(ch, "You need to finish building the barrier first.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_OWN_TILE: {
				if (!GET_LOYALTY(ch) || ROOM_OWNER(any_room) != GET_LOYALTY(ch)) {
					msg_to_char(ch, "You need to do that at a location you own.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_CAN_USE_GUEST: {
				want_room_permission = TRUE;
				
				if (can_use_room(ch, any_room, GUESTS_ALLOWED)) {
					have_room_permission = TRUE;
				}
				else if (!*room_permission_error) {
					snprintf(room_permission_error, sizeof(room_permission_error), "You don't have permission to do that %s.\r\n", (any_room == IN_ROOM(ch) ? "here" : "there"));
				}
				break;
			}
			case ABIL_LIMIT_CAN_USE_ALLY: {
				want_room_permission = TRUE;
				
				if (can_use_room(ch, any_room, MEMBERS_AND_ALLIES)) {
					have_room_permission = TRUE;
				}
				else if (!*room_permission_error) {
					snprintf(room_permission_error, sizeof(room_permission_error), "You don't have permission to do that %s.\r\n", (any_room == IN_ROOM(ch) ? "here" : "there"));
				}
				break;
			}
			case ABIL_LIMIT_CAN_USE_MEMBER: {
				want_room_permission = TRUE;
				
				if (can_use_room(ch, any_room, MEMBERS_ONLY)) {
					have_room_permission = TRUE;
				}
				else if (!*room_permission_error) {
					snprintf(room_permission_error, sizeof(room_permission_error), "You don't have permission to do that %s.\r\n", (any_room == IN_ROOM(ch) ? "here" : "there"));
				}
				break;
			}
			case ABIL_LIMIT_ON_ROAD: {
				if (!IS_ROAD(any_room)) {
					msg_to_char(ch, "You need to do that on a road.\r\n");
					return FALSE;
				}
				else if (!IS_COMPLETE(any_room)) {
					msg_to_char(ch, "You need to finish building first.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_PAINTABLE_BUILDING: {
				room_data *home = any_room ? HOME_ROOM(any_room) : NULL;
				if (!home || !IS_ANY_BUILDING(home) || ROOM_AFF_FLAGGED(home, ROOM_AFF_PERMANENT_PAINT) || ROOM_BLD_FLAGGED(home, BLD_NO_PAINT)) {
					msg_to_char(ch, "You can't do that %s.\r\n", (any_room == IN_ROOM(ch) ? "here" : "there"));
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_IN_CITY: {
				bool wait = FALSE;
				if (!ROOM_OWNER(any_room) && !is_in_city_for_empire(any_room, ROOM_OWNER(any_room), TRUE, &wait)) {
					msg_to_char(ch, "You must be in a city to use that ability%s.\r\n", wait ? " (this city was founded too recently)" : "");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_HAVE_EMPIRE: {
				if (!GET_LOYALTY(ch)) {
					msg_to_char(ch, "You must be a member of an empire to do that.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_INDOORS: {
				if (IS_OUTDOOR_TILE(any_room)) {
					msg_to_char(ch, "You need to be indoors to do that.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_OUTDOORS: {
				if (!IS_OUTDOOR_TILE(any_room)) {
					msg_to_char(ch, "You need to be outdoors to do that.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_ON_MAP: {
				if (GET_ROOM_VNUM(any_room) >= MAP_SIZE) {
					msg_to_char(ch, "You need to be on the map to do that.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_TERRAFORM_APPROVAL: {
				if (!IS_APPROVED(ch) && config_get_bool("terraform_approval")) {
					send_config_msg(ch, "need_approval_string");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_VALID_SIEGE_TARGET: {
				if (room_targ && !validate_siege_target_room(ch, NULL, room_targ)) {
					// sends own messages
					return FALSE;
				}
				if (vvict && !validate_siege_target_vehicle(ch, NULL, vvict)) {
					// sends own messages
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_NOT_DISTRACTED: {
				if (AFF_FLAGGED(ch, AFF_DISTRACTED)) {
					msg_to_char(ch, "You're too distracted to do that.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_NOT_IMMOBILIZED: {
				if (AFF_FLAGGED(ch, AFF_IMMOBILIZED)) {
					msg_to_char(ch, "You can't do that while immobilized.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_CAN_TELEPORT_HERE: {
				if (!can_teleport_to(ch, IN_ROOM(ch), FALSE)) {
					msg_to_char(ch, "You can't do that here.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_WITHIN_RANGE: {
				if (compute_distance(IN_ROOM(ch), other_room) > get_ability_data_value(abil, ADL_RANGE, TRUE)) {
					if (vict) {
						act("$E is too far away.", FALSE, ch, NULL, vict, TO_CHAR);
					}
					else {
						msg_to_char(ch, "It's too far away.\r\n");
					}
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_NOT_GOD_TARGET: {
				if (vict && vict != ch && (IS_GOD(vict) || IS_IMMORTAL(vict))) {
					msg_to_char(ch, "You can't target a god!\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_GUEST_PERMISSION_AT_TARGET: {
				if (!can_use_room(ch, other_room, GUESTS_ALLOWED)) {
					msg_to_char(ch, "You don't have permission to do that %s.\r\n", (other_room == IN_ROOM(ch) ? "here" : "there"));
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_ALLY_PERMISSION_AT_TARGET: {
				if (!can_use_room(ch, other_room, MEMBERS_AND_ALLIES)) {
					msg_to_char(ch, "You don't have permission to do that %s.\r\n", (other_room == IN_ROOM(ch) ? "here" : "there"));
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_MEMBER_PERMISSION_AT_TARGET: {
				if (!can_use_room(ch, other_room, MEMBERS_ONLY)) {
					msg_to_char(ch, "You don't have permission to do that %s.\r\n", (other_room == IN_ROOM(ch) ? "here" : "there"));
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_CAN_TELEPORT_TARGET: {
				if (!can_teleport_to(ch, other_room, FALSE)) {
					msg_to_char(ch, "You can't teleport there.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_TARGET_NOT_FOREIGN_EMPIRE_NPC: {
				if (vict && IS_NPC(vict) && ((GET_LOYALTY(vict) && GET_LOYALTY(vict) != GET_LOYALTY(ch)) || !can_use_room(ch, IN_ROOM(vict), GUESTS_ALLOWED))) {
					msg_to_char(ch, "You can't target an NPC in another empire.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_NOT_HERE: {
				if (other_room && other_room == IN_ROOM(ch)) {
					msg_to_char(ch, "You can't target this location.\r\n");
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_CHECK_CITY_FOUND_TIME: {
				bool wait;
				if (ROOM_OWNER(other_room) && !is_in_city_for_empire(other_room, ROOM_OWNER(other_room), TRUE, &wait)) {
					msg_to_char(ch, "%s city was founded too recently; you'll have to wait.\r\n", (other_room == IN_ROOM(ch) ? "This" : "That"));
					return FALSE;
				}
				break;
			}
			case ABIL_LIMIT_ITEM_TYPE: {
				want_item_type = TRUE;
				
				if (!ovict) {
					have_item_type = TRUE;	// must target something other than an object
				}
				else if (GET_OBJ_TYPE(ovict) == adl->misc) {
					have_item_type = TRUE;
				}
				else if (!*item_type_error) {
					sprinttype(adl->misc, item_types, part, sizeof(part), "UNKNOWN");
					snprintf(item_type_error, sizeof(item_type_error), "You must target %s %s item.\r\n", AN(part), part);
				}
				break;
			}
			case ABIL_LIMIT_WIELD_ANY_WEAPON: {
				want_weapon = TRUE;
				
				if (GET_EQ(ch, WEAR_WIELD) && IS_WEAPON(GET_EQ(ch, WEAR_WIELD))) {
					have_weapon = TRUE;
				}
				else if (!*weapon_error) {
					snprintf(weapon_error, sizeof(weapon_error), "You can't do that while your weapon is disarmed!\r\n");
				}
				break;
			}
			case ABIL_LIMIT_WIELD_ATTACK_TYPE: {
				want_weapon = TRUE;
				
				if (GET_EQ(ch, WEAR_WIELD) && IS_WEAPON(GET_EQ(ch, WEAR_WIELD)) && GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) == adl->misc) {
					have_weapon = TRUE;
				}
				else if (!*weapon_error) {
					snprintf(weapon_error, sizeof(weapon_error), "You need to wield a '%s' weapon to do that.\r\n", get_attack_name_by_vnum(adl->misc));
				}
				break;
			}
			case ABIL_LIMIT_WIELD_WEAPON_TYPE: {
				want_weapon = TRUE;
				
				if (GET_EQ(ch, WEAR_WIELD) && IS_WEAPON(GET_EQ(ch, WEAR_WIELD)) && get_attack_weapon_type_by_vnum(GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD))) == adl->misc) {
					have_weapon = TRUE;
				}
				else if (!*weapon_error) {
					snprintf(weapon_error, sizeof(weapon_error), "You need to be wielding a %s weapon to do that.\r\n", weapon_types[adl->misc]);
				}
				break;
			}
			case ABIL_LIMIT_NOT_BEING_ATTACKED: {
				char_data *ch_iter;
				
				DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), ch_iter, next_in_room) {
					if (FIGHTING(ch_iter) == ch) {
						msg_to_char(ch, "You can't do that someone is attacking you!\r\n");
						return FALSE;
					}
				}
				break;
			}
		}
	}
	
	// things that can check multiples
	if (want_room_permission && !have_room_permission) {
		msg_to_char(ch, "%s", *room_permission_error ? room_permission_error : "You do not have permission to do that here.\r\n");
		return FALSE;
	}
	else if (want_weapon && !have_weapon) {
		msg_to_char(ch, "%s", *weapon_error ? weapon_error : "You need to be wielding a weapon to do that.\r\n");
		return FALSE;
	}
	else if (want_weapon && have_weapon && AFF_FLAGGED(ch, AFF_DISARMED)) {
		msg_to_char(ch, "You can't do that while your weapon is disarmed!\r\n");
		return FALSE;
	}
	else if (want_item_type && !have_item_type) {
		msg_to_char(ch, "%s", *item_type_error ? item_type_error : "You cannot use that ability on that type of item.\r\n");
		return FALSE;
	}
	
	// otherwise:
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// ABILITY INTERACTION FUNCS ///////////////////////////////////////////////

// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(conjure_liquid_interaction) {
	int amount;
	
	if (!inter_item || !IS_DRINK_CONTAINER(inter_item) || interaction->quantity < 1 || !find_generic(interaction->vnum, GENERIC_LIQUID)) {
		return FALSE;
	}
	
	amount = GET_DRINK_CONTAINER_CONTENTS(inter_item) + interaction->quantity;
	amount = MIN(amount, GET_DRINK_CONTAINER_CAPACITY(inter_item));
	
	set_obj_val(inter_item, VAL_DRINK_CONTAINER_CONTENTS, amount);
	set_obj_val(inter_item, VAL_DRINK_CONTAINER_TYPE, interaction->vnum);
	
	return TRUE;
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(conjure_object_interaction) {
	int level, num, obj_ok = 0;
	struct ability_exec *data = GET_RUNNING_ABILITY_DATA(ch);
	ability_data *abil = data->abil;
	obj_data *obj = NULL;
	
	level = get_ability_level(ch, ABIL_VNUM(abil));
	
	for (num = 0; num < interaction->quantity; ++num) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, level);
		
		if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
			obj_to_char(obj, ch);
		}
		else {
			obj_to_room(obj, IN_ROOM(ch));
		}
		
		obj_ok = load_otrigger(obj);
		if (obj_ok && obj->carried_by == ch) {
			get_otrigger(obj, ch, FALSE);
		}
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	// messaging?
	if (obj_ok && obj) {
		send_ability_per_item_messages(ch, obj, interaction->quantity, abil, data, NULL);
	}
	
	return TRUE;
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(conjure_vehicle_interaction) {
	bool junk;
	int level, num;
	struct ability_exec *data = GET_RUNNING_ABILITY_DATA(ch);
	ability_data *abil = data->abil;
	vehicle_data *veh = NULL;
	
	level = get_ability_level(ch, ABIL_VNUM(abil));
	
	for (num = 0; num < interaction->quantity; ++num) {
		veh = read_vehicle(interaction->vnum, TRUE);
		scale_vehicle_to_level(veh, level);
		vehicle_to_room(veh, IN_ROOM(ch));
		
		// this is borrowed from act.trade.c
		if (!VEH_FLAGGED(veh, VEH_NO_CLAIM)) {
			if (VEH_CLAIMS_WITH_ROOM(veh)) {
				// try to claim the room if unclaimed: can_claim checks total available land, but the outside is check done within this block
				if (!ROOM_OWNER(HOME_ROOM(IN_ROOM(ch))) && can_claim(ch) && !ROOM_AFF_FLAGGED(HOME_ROOM(IN_ROOM(ch)), ROOM_AFF_UNCLAIMABLE)) {
					empire_data *emp = get_or_create_empire(ch);
					if (emp) {
						int ter_type = get_territory_type_for_empire(HOME_ROOM(IN_ROOM(ch)), emp, FALSE, &junk, NULL);
						if (EMPIRE_TERRITORY(emp, ter_type) < land_can_claim(emp, ter_type)) {
							claim_room(HOME_ROOM(IN_ROOM(ch)), emp);
						}
					}
				}
			
				// and set the owner to the room owner
				perform_claim_vehicle(veh, ROOM_OWNER(HOME_ROOM(IN_ROOM(ch))));
			}
			else {
				perform_claim_vehicle(veh, GET_LOYALTY(ch));
			}
		}
		
		special_vehicle_setup(ch, veh);
		load_vtrigger(veh);
	}
	
	// messaging?
	if (veh) {
		send_ability_per_vehicle_message(ch, veh, interaction->quantity, abil, data, NULL);
	}
	
	return TRUE;
}


/**
* for devastation_ritual
*
* INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
*/
INTERACTION_FUNC(devastate_crop) {
	char type[256];
	int num = interaction->quantity, obj_ok = 0;
	struct ability_exec *data = GET_RUNNING_ABILITY_DATA(ch);
	crop_data *cp = ROOM_CROP(inter_room);
	ability_data *abil = data->abil;
	obj_data *newobj = NULL;
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, num);
	}
	
	while (num-- > 0) {
		obj_to_char_or_room((newobj = read_object(interaction->vnum, TRUE)), ch);
		scale_item_to_level(newobj, 1);	// minimum level
		if ((obj_ok = load_otrigger(newobj)) && newobj->carried_by) {
			get_otrigger(newobj, newobj->carried_by, FALSE);
		}
	}
	
	if (newobj && obj_ok) {
		snprintf(type, sizeof(type), "%s", GET_CROP_NAME(cp));
		strtolower(type);
		
		send_ability_per_item_messages(ch, newobj, interaction->quantity, abil, data, type);
	}
	
	return TRUE;
}


/**
* for devastation_ritual
*
* INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
*/
INTERACTION_FUNC(devastate_trees) {
	char type[256];
	int num, obj_ok = 0;
	struct ability_exec *data = GET_RUNNING_ABILITY_DATA(ch);
	ability_data *abil = data->abil;
	obj_data *newobj = NULL;
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	for (num = 0; num < interaction->quantity; ++num) {
		obj_to_char_or_room((newobj = read_object(interaction->vnum, TRUE)), ch);
		scale_item_to_level(newobj, 1);	// minimum level
		if ((obj_ok = load_otrigger(newobj)) && newobj->carried_by) {
			get_otrigger(newobj, newobj->carried_by, FALSE);
		}
	}
	
	if (newobj && obj_ok) {
		snprintf(type, sizeof(type), "%s", GET_SECT_NAME(SECT(inter_room)));
		strtolower(type);
		
		send_ability_per_item_messages(ch, newobj, interaction->quantity, abil, data, type);
	}
	
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// ABILITY MESSAGING ///////////////////////////////////////////////////////

/**
* Sends the main messages for using an ability.
*
* @param char_data *ch The player using the ability.
* @param char_data *vict The targeted player, if any (or NULL).
* @param obj_data *ovict The targeted object, if any (or NULL).
* @param vehicle_data *vvict The targeted vehicle (NULL if none).
* @param ability_data *abil The ability.
* @param int use_pos For over-time abilities, pass the current message pos. For all others, pass NOTHING instead.
* @param struct ability_exec *data The execution info for the ability (may be NULL).
*/
void send_ability_activation_messages(char_data *ch, char_data *vict, obj_data *ovict, vehicle_data *vvict, ability_data *abil, int use_pos, struct ability_exec *data) {
	bool any, invis;
	char buf[MAX_STRING_LENGTH];
	char *msg;
	
	bitvector_t act_flags = ACT_ABILITY;
	
	#define _AAM_MSG(type)  (use_pos == NOTHING ? get_custom_message(ABIL_CUSTOM_MSGS(abil), (type)) : get_custom_message_pos(ABIL_CUSTOM_MSGS(abil), (type), use_pos))
	
	if (!ch || !abil) {
		return;	// no work
	}
	if (data && data->no_msg) {
		return;
	}
	
	any = FALSE;
	invis = ABILITY_FLAGGED(abil, ABILF_INVISIBLE) ? TRUE : FALSE;
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF)) {
		// non-violent buff
		act_flags |= ACT_BUFF;
	}
	if (use_pos > 0) {
		// counts as action spam
		act_flags |= TO_SPAMMY;
	}
	
	if (vict || (!vict && !ovict && !vvict)) {	// messaging with char target or no target
		if (ch == vict || (!vict && !ovict)) {	// message: targeting self
			// to-char
			any = TRUE;
			if ((msg = _AAM_MSG(ABIL_CUSTOM_SELF_TO_CHAR))) {
				if (*msg != '*') {
					act(msg, FALSE, ch, ovict, vict, TO_CHAR | act_flags);
				}
			}
			else if (use_pos == NOTHING && (!IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE) || ABIL_ATTACK_TYPE(abil) <= 0)) {
				// don't message if it's damage + there's an attack type
				snprintf(buf, sizeof(buf), "You use %s!", SAFE_ABIL_COMMAND(abil));
				act(buf, FALSE, ch, ovict, vict, TO_CHAR | act_flags);
			}
		
			// to room
			if ((msg = _AAM_MSG(ABIL_CUSTOM_SELF_TO_ROOM))) {
				if (*msg != '*') {
					act(msg, invis, ch, ovict, vict, TO_ROOM | act_flags);
				}
			}
			else if (use_pos == NOTHING && (!IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE) || ABIL_ATTACK_TYPE(abil) <= 0)) {
				// don't message if it's damage + there's an attack type
				snprintf(buf, sizeof(buf), "$n uses %s!", SAFE_ABIL_COMMAND(abil));
				act(buf, invis, ch, ovict, vict, TO_ROOM | act_flags);
			}
		}
		else {	// message: ch != vict
			// to-char
			any = TRUE;
			if ((msg = _AAM_MSG(ABIL_CUSTOM_TARGETED_TO_CHAR))) {
				if (*msg != '*') {
					act(msg, FALSE, ch, ovict, vict, TO_CHAR | act_flags);
				}
			}
			else if (use_pos == NOTHING && (!IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE) || ABIL_ATTACK_TYPE(abil) <= 0)) {
				// don't message if it's damage + there's an attack type
				snprintf(buf, sizeof(buf), "You use %s on $N!", SAFE_ABIL_COMMAND(abil));
				act(buf, FALSE, ch, ovict, vict, TO_CHAR | act_flags);
			}
		
			// to vict
			if ((msg = _AAM_MSG(ABIL_CUSTOM_TARGETED_TO_VICT))) {
				if (*msg != '*') {
					act(msg, invis, ch, ovict, vict, TO_VICT | act_flags);
				}
			}
			else if (use_pos == NOTHING && (!IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE) || ABIL_ATTACK_TYPE(abil) <= 0)) {
				// don't message if it's damage + there's an attack type
				snprintf(buf, sizeof(buf), "$n uses %s on you!", SAFE_ABIL_COMMAND(abil));
				act(buf, invis, ch, ovict, vict, TO_VICT | act_flags);
			}
		
			// to room
			if ((msg = _AAM_MSG(ABIL_CUSTOM_TARGETED_TO_ROOM))) {
				if (*msg != '*') {
					act(msg, invis, ch, ovict, vict, TO_NOTVICT | act_flags);
				}
			}
			else if (use_pos == NOTHING && (!IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE) || ABIL_ATTACK_TYPE(abil) <= 0)) {
				// don't message if it's damage + there's an attack type
				snprintf(buf, sizeof(buf), "$n uses %s on $N!", SAFE_ABIL_COMMAND(abil));
				act(buf, invis, ch, ovict, vict, TO_NOTVICT | act_flags);
			}
		}
	}
	else if (ovict) {	// messaging with obj target
		// to-char
		any = TRUE;
		if ((msg = _AAM_MSG(ABIL_CUSTOM_TARGETED_TO_CHAR))) {
			if (*msg != '*') {
				act(msg, FALSE, ch, ovict, NULL, TO_CHAR | act_flags);
			}
		}
		else if (use_pos == NOTHING && (!IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE) || ABIL_ATTACK_TYPE(abil) <= 0)) {
			// don't message if it's damage + there's an attack type
			snprintf(buf, sizeof(buf), "You use %s on $p!", SAFE_ABIL_COMMAND(abil));
			act(buf, FALSE, ch, ovict, NULL, TO_CHAR | act_flags);
		}
	
		// to room
		if ((msg = _AAM_MSG(ABIL_CUSTOM_TARGETED_TO_ROOM))) {
			if (*msg != '*') {
				act(msg, invis, ch, ovict, NULL, TO_ROOM | act_flags);
			}
		}
		else if (use_pos == NOTHING && (!IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE) || ABIL_ATTACK_TYPE(abil) <= 0)) {
			// don't message if it's damage + there's an attack type
			snprintf(buf, sizeof(buf), "$n uses %s on $p!", SAFE_ABIL_COMMAND(abil));
			act(buf, invis, ch, ovict, NULL, TO_ROOM | act_flags);
		}
	}
	else if (vvict) {	// messaging with vehicle target
		// to-char
		any = TRUE;
		if ((msg = _AAM_MSG(ABIL_CUSTOM_TARGETED_TO_CHAR))) {
			if (*msg != '*') {
				act(msg, FALSE, ch, NULL, vvict, TO_CHAR | ACT_VEH_VICT | act_flags);
			}
		}
		else if (use_pos == NOTHING && (!IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE) || ABIL_ATTACK_TYPE(abil) <= 0)) {
			// don't message if it's damage + there's an attack type
			snprintf(buf, sizeof(buf), "You use %s on $V!", SAFE_ABIL_COMMAND(abil));
			act(buf, FALSE, ch, NULL, vvict, TO_CHAR | ACT_VEH_VICT | act_flags);
		}
	
		// to room
		if ((msg = _AAM_MSG(ABIL_CUSTOM_TARGETED_TO_ROOM))) {
			if (*msg != '*') {
				act(msg, invis, ch, NULL, vvict, TO_ROOM | ACT_VEH_VICT | act_flags);
			}
		}
		else if (use_pos == NOTHING && (!IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE) || ABIL_ATTACK_TYPE(abil) <= 0)) {
			// don't message if it's damage + there's an attack type
			snprintf(buf, sizeof(buf), "$n uses %s on $V!", SAFE_ABIL_COMMAND(abil));
			act(buf, invis, ch, NULL, vvict, TO_ROOM | ACT_VEH_VICT | act_flags);
		}
	}
	
	// mark this now
	if (data && any) {
		data->sent_any_msg = TRUE;
	}
}


/**
* Sends the message for a counterspell when using an ability.
*
* @param char_data *ch The player using the ability.
* @param char_data *vict The targeted player, who had the counterspell.
* @param ability_data *abil The ability.
* @param struct ability_exec *data The execution info for the ability (may be NULL).
*/
void send_ability_counterspell_messages(char_data *ch, char_data *vict, ability_data *abil, struct ability_exec *data) {
	char *msg;
	
	if (!ch || !abil) {
		return;	// no work?
	}
	if (data && data->no_msg) {
		return;
	}
	
	// mark this now
	if (data) {
		data->sent_any_msg = TRUE;	// guaranteed
	}
	
	// to-char
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_COUNTERSPELL_TO_CHAR))) {
		if (*msg != '*') {
			act(msg, FALSE, ch, NULL, vict, TO_CHAR | ACT_ABILITY);
		}
	}
	else {
		snprintf(buf, sizeof(buf), "You %s $N, but $E counterspells it!", SAFE_ABIL_COMMAND(abil));
		act(buf, FALSE, ch, NULL, vict, TO_CHAR | ACT_ABILITY);
	}
	
	// to vict
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_COUNTERSPELL_TO_VICT))) {
		if (*msg != '*') {
			act(msg, FALSE, ch, NULL, vict, TO_VICT | ACT_ABILITY);
		}
	}
	else {
		snprintf(buf, sizeof(buf), "$n tries to %s you, but you counterspell it!", SAFE_ABIL_COMMAND(abil));
		act(buf, FALSE, ch, NULL, vict, TO_VICT | ACT_ABILITY);
	}
	
	// to room
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_COUNTERSPELL_TO_ROOM))) {
		if (*msg != '*') {
			act(msg, FALSE, ch, NULL, vict, TO_NOTVICT | ACT_ABILITY);
		}
	}
	else {
		snprintf(buf, sizeof(buf), "$n tries to %s $N, but $E counterspells it!", SAFE_ABIL_COMMAND(abil));
		act(buf, FALSE, ch, NULL, vict, TO_NOTVICT | ACT_ABILITY);
	}
}


/**
* Sends the failure message for using an ability.
*
* @param char_data *ch The player using the ability.
* @param char_data *vict The targeted player, if any (or NULL).
* @param obj_data *ovict The targeted object, if any (or NULL).
* @param ability_data *abil The ability.
* @param struct ability_exec *data The execution info for the ability (may be NULL).
*/
void send_ability_fail_messages(char_data *ch, char_data *vict, obj_data *ovict, ability_data *abil, struct ability_exec *data) {
	bool invis;
	bitvector_t act_flags = ACT_ABILITY;
	char *msg;
	
	if (!ch || !abil) {
		return;	// no work
	}
	if (data && data->no_msg) {
		return;
	}
	
	invis = ABILITY_FLAGGED(abil, ABILF_INVISIBLE) ? TRUE : FALSE;
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF)) {
		// non-violent buff
		act_flags |= ACT_BUFF;
	}
	
	// mark this now
	if (data) {
		data->sent_any_msg = TRUE;	// guaranteed
	}
	
	if (ch == vict || (!vict && !ovict)) {	// message: targeting self
		// to-char
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_SELF_TO_CHAR))) {
			if (*msg != '*') {
				act(msg, FALSE, ch, ovict, vict, TO_CHAR | act_flags);
			}
		}
		else {
			snprintf(buf, sizeof(buf), "You fail to use %s!", SAFE_ABIL_COMMAND(abil));
			act(buf, FALSE, ch, ovict, vict, TO_CHAR | act_flags);
		}
	
		// to room
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_SELF_TO_ROOM))) {
			if (*msg != '*') {
				act(msg, invis, ch, ovict, vict, TO_ROOM | act_flags);
			}
		}
		else {
			snprintf(buf, sizeof(buf), "$n fails to use %s!", SAFE_ABIL_COMMAND(abil));
			act(buf, invis, ch, ovict, vict, TO_ROOM | act_flags);
		}
	}
	else if (vict) {	// message: ch != vict
		// to-char
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_CHAR))) {
			if (*msg != '*') {
				act(msg, FALSE, ch, ovict, vict, TO_CHAR | act_flags);
			}
		}
		else {
			snprintf(buf, sizeof(buf), "You try to use %s on $N, but fail!", SAFE_ABIL_COMMAND(abil));
			act(buf, FALSE, ch, ovict, vict, TO_CHAR | act_flags);
		}
	
		// to vict
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_VICT))) {
			if (*msg != '*') {
				act(msg, invis, ch, ovict, vict, TO_VICT | act_flags);
			}
		}
		else {
			snprintf(buf, sizeof(buf), "$n tries to use %s on you, but fails!", SAFE_ABIL_COMMAND(abil));
			act(buf, invis, ch, ovict, vict, TO_VICT | act_flags);
		}
	
		// to room
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_ROOM))) {
			if (*msg != '*') {
				act(msg, invis, ch, ovict, vict, TO_NOTVICT | act_flags);
			}
		}
		else {
			snprintf(buf, sizeof(buf), "$n tries to use %s on $N, but fails!", SAFE_ABIL_COMMAND(abil));
			act(buf, invis, ch, ovict, vict, TO_NOTVICT | act_flags);
		}
	}
	else if (ovict) {	// message: obj targeted without vict
		// to-char
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_CHAR))) {
			if (*msg != '*') {
				act(msg, FALSE, ch, ovict, NULL, TO_CHAR | act_flags);
			}
		}
		else {
			snprintf(buf, sizeof(buf), "You try to use %s on $p, but fail!", SAFE_ABIL_COMMAND(abil));
			act(buf, FALSE, ch, ovict, NULL, TO_CHAR | act_flags);
		}
		
		// to room
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_ROOM))) {
			if (*msg != '*') {
				act(msg, invis, ch, ovict, NULL, TO_ROOM | act_flags);
			}
		}
		else {
			snprintf(buf, sizeof(buf), "$n tries to use %s on $p, but fails!", SAFE_ABIL_COMMAND(abil));
			act(buf, invis, ch, ovict, NULL, TO_ROOM | act_flags);
		}
	}
}


/**
* Sends the message for using an ability with a per-char message set. These
* messages have no default and will be omitted if the ability doesn't have
* them.
*
* @param char_data *ch The player using the ability.
* @param char_data *vict The victim, if any (or NULL; but won't show messages if NULL).
* @param int quantity How many (for the x2 display when > 1).
* @param ability_data *abil The ability.
* @param struct ability_exec *data The execution info for the ability (may be NULL).
* @param char *replace_1 Optional: Will replace $1 in the message with this (may be NULL).
*/
void send_ability_per_char_messages(char_data *ch, char_data *vict, int quantity, ability_data *abil, struct ability_exec *data, char *replace_1) {
	bool invis;
	char buf[256], multi[24];
	char *msg, *repl;
	bitvector_t act_flags = ACT_ABILITY;
	
	if (!ch || !abil || !vict) {
		return;	// no work
	}
	if (data && data->no_msg) {
		return;
	}
	
	invis = ABILITY_FLAGGED(abil, ABILF_INVISIBLE) ? TRUE : FALSE;
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF)) {
		// non-violent buff
		act_flags |= ACT_BUFF;
	}
	
	if (quantity > 1) {
		snprintf(multi, sizeof(multi), " (x%d)", quantity);
	}
	else {
		*multi = '\0';
	}
	
	// to-char ONLY if there's a custom message
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PER_CHAR_TO_CHAR)) && *msg != '*') {
		snprintf(buf, sizeof(buf), "%s%s", msg, multi);
		repl = str_replace("$1", NULLSAFE(replace_1), buf);
		act(repl, FALSE, ch, NULL, vict, TO_CHAR | act_flags);
		free(repl);
	
		// mark this now
		if (data) {
			data->sent_any_msg = TRUE;
		}
	}
	
	// to vict ONLY if there's a custom message
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PER_CHAR_TO_VICT)) && *msg != '*') {
		snprintf(buf, sizeof(buf), "%s%s", msg, multi);
		repl = str_replace("$1", NULLSAFE(replace_1), buf);
		act(repl, invis, ch, NULL, vict, TO_VICT | act_flags);
		free(repl);
	}
	
	// to room ONLY if there's a custom message
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PER_CHAR_TO_ROOM)) && *msg != '*') {
		snprintf(buf, sizeof(buf), "%s%s", msg, multi);
		repl = str_replace("$1", NULLSAFE(replace_1), buf);
		act(repl, invis, ch, NULL, vict, TO_NOTVICT | act_flags);
		free(repl);
	}
}


/**
* Sends the message for using an ability with a per-item message set. These
* messages have no default and will be omitted if the ability doesn't have
* them.
*
* @param char_data *ch The player using the ability.
* @param obj_data *ovict The item, if any (or NULL).
* @param int quantity How many items (for the x2 display when > 1).
* @param ability_data *abil The ability.
* @param struct ability_exec *data The execution info for the ability (may be NULL).
* @param char *replace_1 Optional: Will replace $1 in the message with this (may be NULL).
*/
void send_ability_per_item_messages(char_data *ch, obj_data *ovict, int quantity, ability_data *abil, struct ability_exec *data, char *replace_1) {
	bool invis;
	char buf[256], multi[24];
	char *msg, *repl;
	bitvector_t act_flags = ACT_ABILITY;
	
	if (!ch || !abil) {
		return;	// no work
	}
	if (data && data->no_msg) {
		return;
	}
	
	invis = ABILITY_FLAGGED(abil, ABILF_INVISIBLE) ? TRUE : FALSE;
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF)) {
		// non-violent buff
		act_flags |= ACT_BUFF;
	}
	
	if (quantity > 1) {
		snprintf(multi, sizeof(multi), " (x%d)", quantity);
	}
	else {
		*multi = '\0';
	}
	
	// to-char ONLY if there's a custom message
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PER_ITEM_TO_CHAR)) && *msg != '*') {
		snprintf(buf, sizeof(buf), "%s%s", msg, multi);
		repl = str_replace("$1", NULLSAFE(replace_1), buf);
		act(repl, FALSE, ch, ovict, NULL, TO_CHAR | act_flags);
		free(repl);
	
		// mark this now
		if (data) {
			data->sent_any_msg = TRUE;
		}
	}
	
	// to room ONLY if there's a custom message
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PER_ITEM_TO_ROOM)) && *msg != '*') {
		snprintf(buf, sizeof(buf), "%s%s", msg, multi);
		repl = str_replace("$1", NULLSAFE(replace_1), buf);
		act(repl, invis, ch, ovict, NULL, TO_ROOM | act_flags);
		free(repl);
	}
}


/**
* Sends the message for using an ability with a per-vehicle message set. These
* messages have no default and will be omitted if the ability doesn't have
* them.
*
* @param char_data *ch The player using the ability.
* @param vehicle_data *vvict The vehicle, if any (or NULL; but won't show messages if NULL).
* @param int quantity How many vehicles (for the x2 display when > 1).
* @param ability_data *abil The ability.
* @param struct ability_exec *data The execution info for the ability (may be NULL).
* @param char *replace_1 Optional: Will replace $1 in the message with this (may be NULL).
*/
void send_ability_per_vehicle_message(char_data *ch, vehicle_data *vvict, int quantity, ability_data *abil, struct ability_exec *data, char *replace_1) {
	bool invis;
	char buf[256], multi[24];
	char *msg, *repl;
	bitvector_t act_flags = ACT_ABILITY;
	
	if (!ch || !abil || !vvict) {
		return;	// no work
	}
	if (data && data->no_msg) {
		return;
	}
	
	invis = ABILITY_FLAGGED(abil, ABILF_INVISIBLE) ? TRUE : FALSE;
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF)) {
		// non-violent buff
		act_flags |= ACT_BUFF;
	}
	
	if (quantity > 1) {
		snprintf(multi, sizeof(multi), " (x%d)", quantity);
	}
	else {
		*multi = '\0';
	}
	
	// to-char ONLY if there's a custom message
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PER_VEH_TO_CHAR)) && *msg != '*') {
		snprintf(buf, sizeof(buf), "%s%s", msg, multi);
		repl = str_replace("$1", NULLSAFE(replace_1), buf);
		act(repl, FALSE, ch, NULL, vvict, TO_CHAR | ACT_VEH_VICT | act_flags);
		free(repl);
	
		// mark this now
		if (data) {
			data->sent_any_msg = TRUE;
		}
	}
	
	// to room ONLY if there's a custom message
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PER_VEH_TO_ROOM)) && *msg != '*') {
		snprintf(buf, sizeof(buf), "%s%s", msg, multi);
		repl = str_replace("$1", NULLSAFE(replace_1), buf);
		act(repl, invis, ch, NULL, vvict, TO_ROOM | ACT_VEH_VICT | act_flags);
		free(repl);
	}
}


/**
* Sends the message for using an ability with a "special" message set, which
* allows more replacements. These messages have no default and will be omitted
* if the ability doesn't have them.
*
* Replacement dollarsign codes are 1-based, not 0-based, so:
*   $1 = replace[0]
*   $2 = replace[1]
*   (replace_count would be 2 here)
*
* @param char_data *ch The player using the ability.
* @param char_data *vict Optional: Character target (may be NULL).
* @param obj_data *ovict Optional: Object target (may be NULL).
* @param ability_data *abil The ability.
* @param struct ability_exec *data The execution info for the ability (may be NULL).
* @param char **replace Array of replacements for $1, $2, $3, etc.
* @param int replace_count Number of elements in the replace set.
*/
void send_ability_special_messages(char_data *ch, char_data *vict, obj_data *ovict, ability_data *abil, struct ability_exec *data, char **replace, int replace_count) {
	bool invis;
	char tok[4];
	char *msg, *repl;
	int iter;
	bitvector_t act_flags = ACT_ABILITY;
	
	if (!ch || !abil) {
		return;	// no work
	}
	if (data && data->no_msg) {
		return;
	}
	
	invis = ABILITY_FLAGGED(abil, ABILF_INVISIBLE) ? TRUE : FALSE;
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF)) {
		// non-violent buff
		act_flags |= ACT_BUFF;
	}
	
	// to-char ONLY if there's a custom message
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_SPEC_TO_CHAR)) && *msg != '*') {
		for (iter = 0; iter < replace_count && replace && replace[iter]; ++iter) {
			snprintf(tok, sizeof(tok), "$%d", iter+1);
			repl = str_replace(tok, replace[iter], msg);
			if (iter > 0) {
				free(msg);
			}
			msg = repl;
		}
		
		act(msg, FALSE, ch, ovict, vict, TO_CHAR | act_flags);
	
		// mark this now
		if (data) {
			data->sent_any_msg = TRUE;
		}
	}
	
	// to vict ONLY if there's a custom message
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_SPEC_TO_VICT)) && *msg != '*') {
		for (iter = 0; iter < replace_count && replace && replace[iter]; ++iter) {
			snprintf(tok, sizeof(tok), "$%d", iter+1);
			repl = str_replace(tok, replace[iter], msg);
			if (iter > 0) {
				free(msg);
			}
			msg = repl;
		}
		
		act(msg, invis, ch, ovict, vict, TO_VICT | act_flags);
	}
	
	// to room ONLY if there's a custom message
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_SPEC_TO_ROOM)) && *msg != '*') {
		for (iter = 0; iter < replace_count && replace && replace[iter]; ++iter) {
			snprintf(tok, sizeof(tok), "$%d", iter+1);
			repl = str_replace(tok, replace[iter], msg);
			if (iter > 0) {
				free(msg);
			}
			msg = repl;
		}
		
		act(msg, invis, ch, ovict, vict, TO_NOTVICT | act_flags);
	}
}


/**
* Sends the pre-ability messages, if any. These are optional, and only appear
* if set in the ability's custom data.
*
* @param char_data *ch The player using the ability.
* @param char_data *vict The targeted player, if any (or NULL).
* @param obj_data *ovict The targeted object, if any (or NULL).
* @param ability_data *abil The ability.
* @param struct ability_exec *data The execution info for the ability (may be NULL).
*/
void send_pre_ability_messages(char_data *ch, char_data *vict, obj_data *ovict, ability_data *abil, struct ability_exec *data) {
	bool any, invis;
	bitvector_t act_flags = ACT_ABILITY;
	char *msg;
	
	if (!ch || !abil) {
		return;	// no work
	}
	if (data && data->no_msg) {
		return;
	}
	
	any = FALSE;
	invis = ABILITY_FLAGGED(abil, ABILF_INVISIBLE) ? TRUE : FALSE;
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF)) {
		// non-violent buff
		act_flags |= ACT_BUFF;
	}
	
	if (ch == vict || (!vict && !ovict)) {	// message: targeting self
		// to-char
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PRE_SELF_TO_CHAR)) && *msg != '*') {
			act(msg, FALSE, ch, ovict, vict, TO_CHAR | act_flags);
			any = TRUE;
		}
	
		// to room
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PRE_SELF_TO_ROOM)) && *msg != '*') {
			act(msg, invis, ch, ovict, vict, TO_ROOM | act_flags);
		}
	}
	else if (vict) {	// message: ch != vict
		// to-char
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PRE_TARGETED_TO_CHAR)) && *msg != '*') {
			act(msg, FALSE, ch, ovict, vict, TO_CHAR | act_flags);
			any = TRUE;
		}
	
		// to vict
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PRE_TARGETED_TO_VICT)) && *msg != '*') {
			act(msg, invis, ch, ovict, vict, TO_VICT | act_flags);
		}
	
		// to room
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PRE_TARGETED_TO_ROOM)) && *msg != '*') {
			act(msg, invis, ch, ovict, vict, TO_NOTVICT | act_flags);
		}
	}
	else if (ovict) {	// message: ovict without vict
		// to-char
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PRE_TARGETED_TO_CHAR)) && *msg != '*') {
			act(msg, FALSE, ch, ovict, NULL, TO_CHAR | act_flags);
			any = TRUE;
		}
	
		// to room
		if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_PRE_TARGETED_TO_ROOM)) && *msg != '*') {
			act(msg, invis, ch, ovict, NULL, TO_ROOM | act_flags);
		}
	}
	
	// mark this now
	if (data && any) {
		data->sent_any_msg = TRUE;
	}
}


/**
* Sends the toggle messages, if present, for toggling off certain abilities.
* If no toggle-to-char message is set, the character sees "Okay".
*
* @param char_data *ch The player toggling the ability off.
* @param ability_data *abil The ability.
* @param struct ability_exec *data The execution info for the ability (may be NULL).
*/
void send_ability_toggle_messages(char_data *ch, ability_data *abil, struct ability_exec *data) {
	bool invis;
	char *msg;
	bitvector_t act_flags = ACT_ABILITY;
	
	if (!ch || !abil) {
		return;	// no work
	}
	if (data && data->no_msg) {
		return;
	}
	
	invis = ABILITY_FLAGGED(abil, ABILF_INVISIBLE) ? TRUE : FALSE;
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF)) {
		// non-violent buff
		act_flags |= ACT_BUFF;
	}
	
	// to-char
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_TOGGLE_TO_CHAR))) {
		if (*msg != '*') {
			act(msg, FALSE, ch, NULL, NULL, TO_CHAR | act_flags);
			if (data) {
				data->sent_any_msg = TRUE;
			}
		}
	}
	else {
		send_config_msg(ch, "ok_string");
		if (data) {
			data->sent_any_msg = TRUE;
		}
	}
	
	// to room ONLY if there's a custom message
	if ((msg = abil_get_custom_message(abil, ABIL_CUSTOM_TOGGLE_TO_ROOM)) && *msg != '*') {
		act(msg, invis, ch, NULL, NULL, TO_ROOM | act_flags);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ABILITIES OVER TIME COMMANDS ////////////////////////////////////////////

/**
* Cancels an over-time ability and attempts to refund any costs.
*
* @param char_data *ch The player.
*/
void cancel_over_time_ability(char_data *ch) {
	ability_data *abil;
	struct cooldown_data *cool, *next_cool;
	
	if ((abil = ability_proto(GET_ACTION_VNUM(ch, 0)))) {
		// refund
		if (GET_ACTION_VNUM(ch, 2) > 0) {
			set_current_pool(ch, ABIL_COST_TYPE(abil), GET_CURRENT_POOL(ch, ABIL_COST_TYPE(abil)) + GET_ACTION_VNUM(ch, 2));
		}
		
		// cooldown
		if (ABIL_COOLDOWN(abil) != NOTHING) {
			LL_FOREACH_SAFE(ch->cooldowns, cool, next_cool) {
				if (cool->type == ABIL_COOLDOWN(abil)) {
					remove_cooldown(ch, cool);
				}
			}
		}
		
		// refund the real resources they used
		give_resources(ch, GET_ACTION_RESOURCES(ch), FALSE);
		free_resource_list(GET_ACTION_RESOURCES(ch));
		GET_ACTION_RESOURCES(ch) = NULL;
	}
}


/**
* This function is called by the action message each update tick, for a person
* who is doing an over-time ability.
*
* @param char_data *ch The person doing the ability.
*/
void perform_over_time_ability(char_data *ch) {
	any_vnum abil_vnum = GET_ACTION_VNUM(ch, 0);
	char arg[MAX_INPUT_LENGTH];
	ability_data *abil;
	char_data *vict;
	obj_data *ovict;
	vehicle_data *vvict;
	room_data *room_targ;
	struct ability_exec *data;
	
	// re-validate ability
	if (!(abil = ability_proto(abil_vnum)) || !has_ability(ch, abil_vnum) || !check_ability_pre_target(ch, abil)) {
		// lost the ability or can't use it now -- this sometimes sent a message but doesn't need to if not
		cancel_action(ch);
		return;
	}
	
	// re-validate target
	if (!redetect_ability_targets(ch, abil, &vict, &ovict, &vvict, &room_targ) || !validate_ability_target(ch, abil, vict, ovict, vvict, room_targ, TRUE)) {
		cancel_action(ch);
		return;
	}
	
	// construct data
	CREATE(data, struct ability_exec, 1);
	data->abil = abil;
	data->should_charge_cost = FALSE;	// already charged, generally
	data->matching_role = has_matching_role(ch, abil, TRUE);
	GET_RUNNING_ABILITY_DATA(ch) = data;
	
	// message position is controlled by action timer
	GET_ACTION_TIMER(ch) += 1;
	send_ability_activation_messages(ch, vict, ovict, vvict, abil, GET_ACTION_TIMER(ch), data);
	
	// detect continuing?
	if (vict && vict != ch && has_custom_message_pos(ABIL_CUSTOM_MSGS(abil), ABIL_CUSTOM_TARGETED_TO_CHAR, GET_ACTION_TIMER(ch) + 1)) {
		// ongoing ability: do nothing
	}
	else if ((!vict || vict == ch) && has_custom_message_pos(ABIL_CUSTOM_MSGS(abil), ABIL_CUSTOM_SELF_TO_CHAR, GET_ACTION_TIMER(ch) + 1)) {
		// ongoing ability: do nothing
	}
	else {
		// done!
		call_ability(ch, abil, NULLSAFE(GET_ACTION_STRING(ch)), vict, ovict, vvict, room_targ, GET_ACTION_VNUM(ch, 1), RUN_ABIL_OVER_TIME, data);
		
		if (data->success && ABILITY_FLAGGED(abil, ABILF_REPEAT_OVER_TIME)) {
			// auto-repeat
			strcpy(arg, NULLSAFE(GET_ACTION_STRING(ch)));
			end_action(ch);
			start_over_time_ability(ch, abil, arg, vict, ovict, vvict, room_targ, GET_ACTION_VNUM(ch, 1), data);
		}
		else {
			// end
			end_action(ch);
		}
	}
	
	// clean up data
	GET_RUNNING_ABILITY_DATA(ch) = NULL;
	free_ability_exec(data);
}


/**
* Begins an over-time ability and sends the first message. It will also charge
* any costs. Everything else should be pre-validated here.
*
* @param char_data *ch The person doing the ability (must be a player, not NPC).
* @param ability_data *abil The ability being used.
* @param char *argument The remaining arguments.
* @param char_data *vict The character target (NULL if none).
* @param obj_data *ovict The object target (NULL if none).
* @param vehicle_data *vvict The vehicle target (NULL if none).
* @param room_data *room_targ The room target (NULL if none).
* @param int level Pre-determined ability level.
* @param struct ability_exec *data The execution data for the ability.
*/
void start_over_time_ability(char_data *ch, ability_data *abil, char *argument, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, int level, struct ability_exec *data) {
	if (!ch || IS_NPC(ch) || !abil) {
		return;	// nothing to see here
	}
	
	send_ability_activation_messages(ch, vict, ovict, vvict, abil, 0, data);
	
	start_action(ch, ACT_OVER_TIME_ABILITY, 0);
	GET_ACTION_VNUM(ch, 0) = ABIL_VNUM(abil);
	GET_ACTION_VNUM(ch, 1) = level;
	GET_ACTION_VNUM(ch, 2) = data->cost;
	GET_ACTION_CHAR_TARG(ch) = vict;
	GET_ACTION_OBJ_TARG(ch) = ovict;
	GET_ACTION_VEH_TARG(ch) = vvict;
	GET_ACTION_ROOM_TARG(ch) = room_targ ? GET_ROOM_VNUM(room_targ) : NOWHERE;
	if (GET_ACTION_STRING(ch)) {
		GET_ACTION_STRING(ch) = str_dup(NULLSAFE(argument));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ABILITY COMMANDS ////////////////////////////////////////////////////////

/**
* This checks if "string" is an ability's command, and performs it if so. This
* is called by the command interpreter. To call an ability directly in code,
* use perform_ability_command().
*
* @param char_data *ch The actor.
* @param char *string The command they typed.
* @param bool exact if TRUE, must be an exact match; FALSE can be abbrev
* @return bool TRUE if it was an ability command and we acted; FALSE if not
*/
bool check_ability(char_data *ch, char *string, bool exact) {
	char cmd[MAX_STRING_LENGTH], arg1[MAX_STRING_LENGTH];
	ability_data *iter, *next_iter, *abil, *abbrev;
	
	skip_spaces(&string);
	half_chop(string, cmd, arg1);
	
	if (!*cmd) {
		return FALSE;
	}
	if (IS_NPC(ch) && AFF_FLAGGED(ch, AFF_ORDERED)) {
		return FALSE;
	}
	
	// look for an ability that matches
	abil = abbrev = NULL;
	HASH_ITER(sorted_hh, sorted_abilities, iter, next_iter) {
		if (!ABIL_COMMAND(iter) || LOWER(*cmd) != LOWER(*ABIL_COMMAND(iter))) {
			continue;	// no command or not matching first letter
		}
		if (!IS_NPC(ch) && !has_ability(ch, ABIL_VNUM(iter))) {
			continue;	// pc does not have the ability
		}
		
		// ok match string
		if (!str_cmp(cmd, ABIL_COMMAND(iter))) {
			abil = iter;	// found exact
			break;
		}
		else if (!exact && !abbrev && is_abbrev(cmd, ABIL_COMMAND(iter))) {
			abbrev = iter;	// partial match
		}
	}
	
	if (!abil) {
		abil = abbrev;	// if any
	}
	if (!abil) {
		return FALSE;	// did not match any anything
	}
	
	// does a command trigger override this ability command?
	if (strlen(string) < strlen(ABIL_COMMAND(abil)) && check_command_trigger(ch, ABIL_COMMAND(abil), arg1, CMDTRG_EXACT)) {
		return TRUE;
	}
	
	perform_ability_command(ch, abil, arg1);
	return TRUE;
}


/* This function is the very heart of the entire magic system.  All invocations
 * of all types of magic -- objects, spoken and unspoken PC and NPC spells, the
 * works -- all come through this function eventually. This is also the entry
 * point for non-spoken or unrestricted spells. Spellnum 0 is legal but silently
 * ignored here, to make callers simpler. */

/**
* This function is the core of the parameterized ability system. All normal
* ability invocations go through this function. This is also a safe entry point
* for non-cast or unrestricted abilities.
*
* @param char_data *ch The person performing the ability.
* @param ability_data *abil The ability being used.
* @param char_data *vict The character target, if any (may be NULL).
* @param obj_data *ovict The object target, if any (may be NULL).
* @param vehicle_data *vvict The vehicle target, if any (may be NULL).
* @param room_data *room_targ For room target, if any (may be NULL).
* @param int level The level to use the ability at.
* @param int run_mode May be RUN_ABIL_NORMAL, RUN_ABIL_OVER_TIME, etc.
* @param struct ability_exec *data The execution data to pass back and forth.
*/
void call_ability(char_data *ch, ability_data *abil, char *argument, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, int level, int run_mode, struct ability_exec *data) {
	int iter;
	
	if (!ch || !abil) {
		return;
	}
	
	if (run_mode == RUN_ABIL_OVER_TIME) {
		// ability is being called at the end of an over-time action (already charged)
		data->should_charge_cost = FALSE;
	}
	
	// determine costs and scales
	for (iter = 0; do_ability_data[iter].type != NOBITS && !data->stop; ++iter) {
		if (IS_SET(ABIL_TYPES(abil), do_ability_data[iter].type) && do_ability_data[iter].prep_func) {
			call_prep_abil(do_ability_data[iter].prep_func);
			
			// adjust cost
			if (ABIL_COST_PER_SCALE_POINT(abil)) {
				data->cost += get_ability_type_data(data, do_ability_data[iter].type)->scale_points * ABIL_COST_PER_SCALE_POINT(abil);
			}
		}
	}
	
	// early exit?
	if (data->stop) {
		return;
	}
	
	if (run_mode != RUN_ABIL_OVER_TIME) {
		// check costs and cooldowns now -- not on over-time
		if (!can_use_ability(ch, ABIL_VNUM(abil), ABIL_COST_TYPE(abil), data->cost, ABIL_COOLDOWN(abil))) {
			// sends own message
			data->stop = TRUE;
			data->should_charge_cost = FALSE;
			return;
		}
	}
	if (run_mode == RUN_ABIL_NORMAL) {
		// resource cost check -- normal ONLY
		if (ABIL_RESOURCE_COST(abil) && !has_resources(ch, ABIL_RESOURCE_COST(abil), FALSE, TRUE, ABIL_NAME(abil))) {
			// sends own message
			data->stop = TRUE;
			data->should_charge_cost = FALSE;
			return;
		}
	}
	
	// ready to start the ability:
	if (ABIL_IS_VIOLENT(abil) && SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// these happen immediately before the first message
	if (ABILITY_TRIGGERS(ch, vict, ovict, ABIL_VNUM(abil))) {
		data->stop = TRUE;
		data->should_charge_cost = FALSE;
		return;
	}
	
	// locked in! apply the effects
	apply_ability_effects(abil, ch, vict, ovict, vvict, room_targ);
	
	// pre-messages if any (skip on over-time: already sent)
	if (run_mode != RUN_ABIL_OVER_TIME) {
		send_pre_ability_messages(ch, vict, ovict, abil, data);
	}
	
	// WAIT! Over-time abilities stop here
	if (ABILITY_FLAGGED(abil, ABILF_OVER_TIME) && run_mode == RUN_ABIL_NORMAL) {
		start_over_time_ability(ch, abil, argument, vict, ovict, vvict, room_targ, level, data);
		return;
	}
	
	// start meters now, to track direct damage()
	if (ABIL_IS_VIOLENT(abil)) {
		check_start_combat_meters(ch);
		if (vict) {
			check_start_combat_meters(vict);
		}
	}
	
	// counterspell?
	if (!data->stop && ABILITY_FLAGGED(abil, ABILF_COUNTERSPELLABLE) && ABIL_IS_VIOLENT(abil) && vict && vict != ch && trigger_counterspell(vict)) {
		send_ability_counterspell_messages(ch, vict, abil, data);
		data->stop = TRUE;	// prevent routines from firing
		data->success = TRUE;	// counts as a successful ability use
		data->no_msg = TRUE;	// don't show more messages
	}
	
	// check for FAILURE:
	if (!data->stop && !skill_check(ch, ABIL_VNUM(abil), ABIL_DIFFICULTY(abil))) {
		send_ability_fail_messages(ch, vict, ovict, abil, data);
		
		data->success = TRUE;	// causes it to charge, skillup, and cooldown
		data->stop = TRUE;	// prevents normal activation
		data->no_msg = TRUE;	// prevents success message
		data->engage_anyway = TRUE;	// allows engage-combat through a data->stop
	}
	
	// main messaging
	if (run_mode != RUN_ABIL_OVER_TIME) {
		send_ability_activation_messages(ch, vict, ovict, vvict, abil, NOTHING, data);
	}
	
	// run the abilities
	for (iter = 0; do_ability_data[iter].type != NOBITS && !data->stop; ++iter) {
		if (IS_SET(ABIL_TYPES(abil), do_ability_data[iter].type) && do_ability_data[iter].do_func) {
			call_do_abil(do_ability_data[iter].do_func);
		}
	}
	
	// right after the do-funcs
	post_ability_procs(ch, abil, vict, ovict, vvict, room_targ, data);
	
	// exp gain unless we hit something that prevented costs
	if (data->should_charge_cost && (!vict || can_gain_exp_from(ch, vict))) {
		// TODO some way to modify this amount?
		gain_ability_exp(ch, ABIL_VNUM(abil), 15);
	}
	
	// check if should be in combat
	if ((!data->stop && data->should_charge_cost) || data->engage_anyway) {
		if (vict && vict != ch && !ABILITY_FLAGGED(abil, ABILF_NO_ENGAGE) && !EXTRACTED(vict) && !IS_DEAD(vict)) {
			// auto-assist if we used an ability on someone who is fighting
			if (!ABIL_IS_VIOLENT(abil) && FIGHTING(vict) && !FIGHTING(ch)) {
				engage_combat(ch, FIGHTING(vict), ABILITY_FLAGGED(abil, ABILF_RANGED | ABILF_RANGED_ONLY) ? FALSE : TRUE);
			}
			
			// auto-attack if used on an enemy
			if (ABIL_IS_VIOLENT(abil) && AWAKE(vict) && !FIGHTING(vict)) {
				engage_combat(vict, ch, ABILITY_FLAGGED(abil, ABILF_RANGED | ABILF_RANGED_ONLY) ? FALSE : TRUE);
			}
		}
	}
	
	// check for a no-effect/fail message
	if (!data->success && !data->no_msg) {
		/* currently always showing the whole fail message
		if (data->sent_any_msg) {
			msg_to_char(ch, "It doesn't seem to have any effect.\r\n");
		}
		else {
			// send a full fail
		*/
			send_ability_fail_messages(ch, vict, ovict, abil, data);
		// }
	}
}


/**
* Things that proc right after an ability runs.
*
* @param char_data *ch The person performing the ability.
* @param ability_data *abil The ability being used.
* @param char_data *vict The character target, if any (may be NULL).
* @param obj_data *ovict The object target, if any (may be NULL).
* @param vehicle_data *vvict The vehicle target, if any (may be NULL).
* @param room_data *room_targ For room target, if any (may be NULL).
* @param struct ability_exec *data The execution data to pass back and forth.
*/
void post_ability_procs(char_data *ch, ability_data *abil, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, struct ability_exec *data) {
	if (data->stop) {
		return;
	}
	
	// counts as a weapon hit
	if (vict && vict != ch && data->success && ABILITY_FLAGGED(abil, ABILF_WEAPON_HIT)) {
		if (has_player_tech(ch, PTECH_POISON) && GET_EQ(ch, WEAR_WIELD) && is_attack_flagged_by_vnum(GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)), AMDF_APPLY_POISON)) {
			// chance to poison
			if (!number(0, 1) && apply_poison(ch, vict) < 0) {
				// dedz
				data->stop = TRUE;
				return;
			}
		}
	}
}


/**
* Attempts to run any abilities that have a matching hook type. This is used
* to chain abilities, or to let abilities modify damage.
*
* @param char_data *ch The player (not supported for NPCS).
* @param bitvector_t hook_type Which AHOOK_ type is running.
* @param char_data *vict Optional: Character target (may be NULL).
* @param obj_data *ovict Optional: Character object (may be NULL).
* @param vehicle_data *vvict Optional: Character vehicle (may be NULL).
* @param room_data *room_targ Optional: Character room (may be NULL).
* @param any_vnum hook_value The value for that hook, e.g. an ability vnum (hooks that don't have this always use 0).
*/
void run_ability_hooks(char_data *ch, bitvector_t hook_type, any_vnum hook_value, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ) {
	bool any_targ, free_limiter = FALSE;
	int cap, level;
	struct ability_hook *ahook;
	char_data *use_char;
	obj_data *use_obj;
	room_data *use_room;
	vehicle_data *use_veh;
	struct ability_exec *data;
	struct player_ability_data *plab, *next_plab;
	struct vnum_hash **use_limiter = NULL, *my_limiter = NULL;
	
	if (IS_NPC(ch) || IS_DEAD(ch) || EXTRACTED(ch)) {
		return;
	}
	
	// inherit or create a limiter
	if (GET_RUNNING_ABILITY_LIMITER(ch)) {
		use_limiter = GET_RUNNING_ABILITY_LIMITER(ch);
	}
	else {
		use_limiter = &my_limiter;
		GET_RUNNING_ABILITY_LIMITER(ch) = use_limiter;
		free_limiter = TRUE;
	}
	
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		if (!plab->ptr || !plab->purchased[GET_CURRENT_SKILL_SET(ch)]) {
			continue;
		}
		if (!ABIL_HAS_HOOK(plab->ptr, hook_type)) {
			continue;	// shortcut
		}
		if (find_in_vnum_hash(*use_limiter, ABIL_VNUM(plab->ptr))) {
			continue;	// already ran
		}
		
		LL_FOREACH(ABIL_HOOKS(plab->ptr), ahook) {
			if (!IS_SET(ahook->type, hook_type)) {
				continue;	// wrong type
			}
			if (ahook->value != hook_value) {
				continue;	// wrong value
			}
			if (number(1, 10000) > ahook->percent * 100) {
				continue;	// failed percent check
			}
			if (!pre_check_hooked_ability(ch, plab->ptr)) {
				continue;	// unlikely to succeed
			}
			
			// incoming targets
			use_char = vict;
			use_obj = ovict;
			use_veh = vvict;
			use_room = room_targ;
			
			// compare targets
			if (IS_SET(ABIL_TARGETS(plab->ptr), ATAR_SELF_ONLY)) {
				use_char = ch;	// convert to self
			}
			if (!use_char && FIGHTING(ch) && IS_SET(ABIL_TARGETS(plab->ptr), ATAR_FIGHT_VICT)) {
				use_char = FIGHTING(ch);	// set to self
			}
			if (!use_char && FIGHTING(ch) && IS_SET(ABIL_TARGETS(plab->ptr), ATAR_FIGHT_SELF)) {
				use_char = ch;	// set to self
			}
			if (IS_SET(ABIL_TARGETS(plab->ptr), ATAR_ROOM_HERE) && (!use_room || !IS_SET(ABIL_TARGETS(plab->ptr), ATAR_ROOM_ADJACENT | ATAR_ROOM_EXIT | ATAR_ROOM_HOME | ATAR_ROOM_RANDOM | ATAR_ROOM_CITY | ATAR_ROOM_COORDS))) {
				use_room = IN_ROOM(ch);	// convert to this room
			}
				
			// validation of targets
			if (use_char == ch && (ABIL_IS_VIOLENT(plab->ptr) || IS_SET(ABIL_TARGETS(plab->ptr), ATAR_NOT_SELF))) {
				use_char = NULL;	// can't target self
			}
			if (use_char && use_char != ch && IN_ROOM(use_char) != IN_ROOM(ch) && IS_SET(ABIL_TARGETS(plab->ptr), ATAR_CHAR_ROOM) && !IS_SET(ABIL_TARGETS(plab->ptr), ATAR_CHAR_WORLD | ATAR_CHAR_CLOSEST)) {
				use_char = NULL;	// char in wrong room
			}
			if (use_char && (IS_DEAD(use_char) || EXTRACTED(use_char))) {
				use_char = NULL;	// gone
			}
			if (use_veh && IN_ROOM(use_veh) != IN_ROOM(ch) && IS_SET(ABIL_TARGETS(plab->ptr), ATAR_VEH_ROOM) && !IS_SET(ABIL_TARGETS(plab->ptr), ATAR_VEH_WORLD)) {
				use_veh = NULL;	// veh in wrong room
			}
			if (use_veh && VEH_IS_EXTRACTED(use_veh)) {
				use_veh = NULL;	// gone
			}
			
			// do we have any mandatory targets left?
			if (!IS_SET(ABIL_TARGETS(plab->ptr), ATAR_IGNORE)) {
				any_targ = FALSE;
				if (IS_SET(ABIL_TARGETS(plab->ptr), CHAR_ATARS) && use_char) {
					any_targ = TRUE;
				}
				else if (IS_SET(ABIL_TARGETS(plab->ptr), OBJ_ATARS) && use_obj) {
					any_targ = TRUE;
				}
				else if (IS_SET(ABIL_TARGETS(plab->ptr), VEH_ATARS) && use_veh) {
					any_targ = TRUE;
				}
				else if (IS_SET(ABIL_TARGETS(plab->ptr), ROOM_ATARS) && use_room) {
					any_targ = TRUE;
				}
				if (!any_targ || !validate_ability_target(ch, plab->ptr, use_char, use_obj, use_veh, use_room, FALSE)) {
					continue;	// no apparent targets
				}
			}
			
			// match!
			
			// mark already run FIRST
			add_vnum_hash(use_limiter, ABIL_VNUM(plab->ptr), 1);
			
			// determine level
			level = get_approximate_level(ch);
			if (!IS_NPC(ch) && ABIL_ASSIGNED_SKILL(plab->ptr) && (cap = get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(plab->ptr)))) < CLASS_SKILL_CAP) {
				level = MIN(level, cap);	// constrain by skill level
			}
			
			// construct data
			CREATE(data, struct ability_exec, 1);
			data->abil = plab->ptr;
			data->should_charge_cost = TRUE;	// may still charge
			data->matching_role = has_matching_role(ch, plab->ptr, TRUE);
			GET_RUNNING_ABILITY_DATA(ch) = data;	// this may override one still on them but is ok
			
			// the big GO
			call_ability(ch, plab->ptr, "", use_char, use_obj, use_veh, use_room, level, RUN_ABIL_HOOKED, data);
			
			// clean up data
			GET_RUNNING_ABILITY_DATA(ch) = NULL;
			free_ability_exec(data);
		}
	}
	
	if (free_limiter) {
		GET_RUNNING_ABILITY_LIMITER(ch) = NULL;
		free_vnum_hash(&my_limiter);
	}
}


/**
* Performs an ability action.
*
* DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
DO_ABIL(do_action_ability) {
	struct ability_data_list *adl;
	
	if (!ch || !abil) {
		return;	// no work
	}
	
	LL_FOREACH(ABIL_DATA(abil), adl) {
		if (adl->type != ADL_ACTION) {
			continue;
		}
		
		// ABIL_ACTION_x: these should set data->success to TRUE only if they succeed
		switch (adl->vnum) {
			case ABIL_ACTION_DETECT_HIDE: {
				call_do_abil(abil_action_detect_hide);
				break;
			}
			case ABIL_ACTION_DETECT_EARTHMELD: {
				call_do_abil(abil_action_detect_earthmeld);
				break;
			}
			case ABIL_ACTION_DETECT_PLAYERS_AROUND: {
				call_do_abil(abil_action_detect_players_around);
				break;
			}
			case ABIL_ACTION_DETECT_ADVENTURES_AROUND: {
				call_do_abil(abil_action_detect_adventures_around);
				break;
			}
			case ABIL_ACTION_DEVASTATE_AREA: {
				call_do_abil(abil_action_devastate_area);
				break;
			}
			case ABIL_ACTION_MAGIC_GROWTH: {
				call_do_abil(abil_action_magic_growth);
				break;
			}
			case ABIL_ACTION_CLOSE_PORTAL: {
				call_do_abil(abil_action_close_portal);
				break;
			}
		}
	}
}


/**
* All buff-type abilities come through here. This handles scaling and buff
* maintenance/replacement.
*
* DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
DO_ABIL(do_buff_ability) {
	struct affected_type *af;
	struct apply_data *apply;
	any_vnum affect_vnum;
	double total_points = 1, remaining_points = 1, share, amt;
	int dur, total_w = 1;
	bool messaged, unscaled;
	bitvector_t aff_options;
	
	affect_vnum = (ABIL_AFFECT_VNUM(abil) != NOTHING) ? ABIL_AFFECT_VNUM(abil) : ATYPE_BUFF;
	
	unscaled = ABILITY_FLAGGED(abil, ABILF_UNSCALED_BUFF) ? TRUE : FALSE;
	if (!unscaled) {
		total_points = get_ability_type_data(data, ABILT_BUFF)->scale_points;
		remaining_points = total_points;
		
		if (total_points <= 0) {
			return;
		}
	}
	
	if (ABIL_IMMUNITIES(abil) && AFF_FLAGGED(vict, ABIL_IMMUNITIES(abil))) {
		act("$N is immune!", FALSE, ch, NULL, vict, TO_CHAR);
		return;
	}
	
	// determine duration (in seconds)
	dur = IS_CLASS_ABILITY(ch, ABIL_VNUM(abil)) ? ABIL_LONG_DURATION(abil) : ABIL_SHORT_DURATION(abil);
	
	messaged = FALSE;	// to prevent duplicates
	
	// affect flags? cost == level 100 ability
	if (ABIL_AFFECTS(abil)) {
		if (!unscaled) {
			remaining_points -= count_bits(ABIL_AFFECTS(abil)) * config_get_double("scale_points_at_100");
			remaining_points = MAX(0, remaining_points);
		}
		
		aff_options = (messaged ? SILENT_AFF : NOBITS) | (ABILITY_FLAGGED(abil, ABILF_CUMULATIVE_BUFF) ? ADD_MODIFIER : NOBITS) | (ABILITY_FLAGGED(abil, ABILF_CUMULATIVE_DURATION) ? ADD_DURATION : NOBITS);
		
		af = create_flag_aff(affect_vnum, dur, ABIL_AFFECTS(abil), ch);
		affect_join(vict, af, aff_options);
		messaged = TRUE;
	}
	
	// determine share for effects
	if (!unscaled) {
		total_w = 0;
		LL_FOREACH(ABIL_APPLIES(abil), apply) {
			if (!apply_never_scales[apply->location]) {
				total_w += ABSOLUTE(apply->weight);
			}
		}
	}
	
	// now create affects for each apply that we can afford
	LL_FOREACH(ABIL_APPLIES(abil), apply) {
		if (apply_never_scales[apply->location] || unscaled) {
			af = create_mod_aff(affect_vnum, dur, apply->location, apply->weight, ch);
			aff_options = (messaged ? SILENT_AFF : NOBITS) | (ABILITY_FLAGGED(abil, ABILF_CUMULATIVE_BUFF) ? ADD_MODIFIER : NOBITS) | (ABILITY_FLAGGED(abil, ABILF_CUMULATIVE_DURATION) ? ADD_DURATION : NOBITS);
			affect_join(vict, af, aff_options);
			messaged = TRUE;
			continue;
		}

		share = total_points * (double) ABSOLUTE(apply->weight) / (double) total_w;
		if (share > remaining_points) {
			share = MIN(share, remaining_points);
		}
		amt = round(share / apply_values[apply->location]) * ((apply->weight < 0) ? -1 : 1);
		if (share > 0 && amt != 0) {
			remaining_points -= share;
			remaining_points = MAX(0, total_points);
			
			af = create_mod_aff(affect_vnum, dur, apply->location, amt, ch);
			aff_options = (messaged ? SILENT_AFF : NOBITS) | (ABILITY_FLAGGED(abil, ABILF_CUMULATIVE_BUFF) ? ADD_MODIFIER : NOBITS) | (ABILITY_FLAGGED(abil, ABILF_CUMULATIVE_DURATION) ? ADD_DURATION : NOBITS);
			affect_join(vict, af, aff_options);
			messaged = TRUE;
		}
	}
	
	// check if it kills them
	if (GET_POS(vict) == POS_INCAP && ABIL_IS_VIOLENT(abil)) {
		perform_execute(ch, vict, TYPE_UNDEFINED, DAM_PHYSICAL);
		data->stop = TRUE;
	}
	
	data->success = TRUE;
}


/**
* Deals siege damage to the targeted vehicle or room.
*
* DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
DO_ABIL(do_building_damage_ability) {
	int dam = 1 + GET_INTELLIGENCE(ch);
	sector_data *secttype;
	bld_data *bldtype;
	
	if (vvict) {
		besiege_vehicle(ch, vvict, dam, SIEGE_MAGICAL, NULL);
		data->success = TRUE;
	}
	else if (room_targ) {
		secttype = SECT(room_targ);
		bldtype = GET_BUILDING(room_targ);
		
		besiege_room(ch, room_targ, dam, NULL);
		
		if (SECT(room_targ) != secttype || bldtype != GET_BUILDING(room_targ)) {
			msg_to_char(ch, "It is destroyed!\r\n");
			act("$n's target is destroyed!", FALSE, ch, NULL, NULL, TO_ROOM);
		}
		data->success = TRUE;
	}
}


/**
* Handler for conjure-liquid abilities.
*
* DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
DO_ABIL(do_conjure_liquid_ability) {
	if (!ovict) {
		// can do nothing
		return;
	}
	
	data->success |= run_interactions(ch, ABIL_INTERACTIONS(abil), INTERACT_LIQUID_CONJURE, IN_ROOM(ch), NULL, ovict, NULL, conjure_liquid_interaction);
}


/**
* Handler for conjure-object abilities.
*
* DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
DO_ABIL(do_conjure_object_ability) {
	data->success |= run_interactions(ch, ABIL_INTERACTIONS(abil), INTERACT_OBJECT_CONJURE, IN_ROOM(ch), NULL, NULL, NULL, conjure_object_interaction);
}


/**
* Handler for conjure-vehicle abilities.
*
* DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
DO_ABIL(do_conjure_vehicle_ability) {
	data->success |= run_interactions(ch, ABIL_INTERACTIONS(abil), INTERACT_VEHICLE_CONJURE, IN_ROOM(ch), NULL, NULL, NULL, conjure_vehicle_interaction);
}


/**
* All damage abilities come through here.
*
* DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
DO_ABIL(do_damage_ability) {
	struct ability_exec_type *subdata = get_ability_type_data(data, ABILT_DAMAGE);
	double reduced_scale;
	int result, dmg;
	
	// fine-tuning ability damage
	double arbitrary_modifier = 4.0;
	
	// smoother scaling, for bonuses, averaged toward 1.0
	reduced_scale = (1.0 + ABIL_SCALE(abil)) / 2.0;
	
	dmg = subdata->scale_points * arbitrary_modifier;
	
	// bonus damage
	switch (ABIL_DAMAGE_TYPE(abil)) {
		case DAM_PHYSICAL: {
			dmg += GET_BONUS_PHYSICAL(ch) * reduced_scale;
			break;
		}
		case DAM_MAGICAL: {
			dmg += GET_BONUS_MAGICAL(ch) * reduced_scale;
			break;
		}
	}
	
	result = damage(ch, vict, dmg, ABIL_ATTACK_TYPE(abil), ABIL_DAMAGE_TYPE(abil), NULL);
	data->success = TRUE;
	
	if (result < 0) {	// dedz
		data->stop = TRUE;
	}
}


/**
* All damage-over-time abilities come through here. This handles scaling and
* stacking.
*
* DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
DO_ABIL(do_dot_ability) {
	any_vnum affect_vnum;
	double points, reduced_scale;
	int dur, dmg;
	
	// fine-tuning ability damage
	double arbitrary_modifier = 1.5;
	
	// smoother scaling, for bonuses, averaged toward 1.0
	reduced_scale = (1.0 + ABIL_SCALE(abil)) / 2.0;
	
	affect_vnum = (ABIL_AFFECT_VNUM(abil) != NOTHING) ? ABIL_AFFECT_VNUM(abil) : ATYPE_DOT;
	
	points = get_ability_type_data(data, ABILT_DOT)->scale_points;
	
	if (points <= 0) {
		return;
	}
	
	if (ABIL_IMMUNITIES(abil) && AFF_FLAGGED(vict, ABIL_IMMUNITIES(abil))) {
		act("$N is immune!", FALSE, ch, NULL, vict, TO_CHAR);
		return;
	}
	
	// determine duration
	dur = IS_CLASS_ABILITY(ch, ABIL_VNUM(abil)) ? ABIL_LONG_DURATION(abil) : ABIL_SHORT_DURATION(abil);
	
	dmg = points * arbitrary_modifier;
	
	// bonus damage
	switch (ABIL_DAMAGE_TYPE(abil)) {
		case DAM_PHYSICAL: {
			dmg += GET_BONUS_PHYSICAL(ch) * reduced_scale / MAX(1, dur/DOT_INTERVAL);
			break;
		}
		case DAM_MAGICAL: {
			dmg += GET_BONUS_MAGICAL(ch) * reduced_scale / MAX(1, dur/DOT_INTERVAL);
			break;
		}
	}

	dmg = MAX(1, dmg);
	apply_dot_effect(vict, affect_vnum, dur, ABIL_DAMAGE_TYPE(abil), dmg, ABIL_MAX_STACKS(abil), ch);
	data->success = TRUE;
}


/**
* Applies color to the building.
*
* DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
DO_ABIL(do_paint_building_ability) {
	struct ability_data_list *adl;
	room_data *home;
	int count = 0, color = -1;
	
	// find a color
	LL_FOREACH(ABIL_DATA(abil), adl) {
		if (adl->type != ADL_PAINT_COLOR) {
			continue;
		}
		if (!number(0, count++)) {
			color = adl->vnum;
		}
	}
	
	if (color != -1 && room_targ && (home = HOME_ROOM(room_targ)) && IS_ANY_BUILDING(home)) {
		set_room_extra_data(home, ROOM_EXTRA_PAINT_COLOR, color);
	}
	data->success = TRUE;
}


/**
* All room-affect abilities come through here.
*
* DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
DO_ABIL(do_room_affect_ability) {
	struct affected_type *af;
	any_vnum affect_vnum;
	double total_points = 1, remaining_points = 1;
	int dur;
	
	affect_vnum = (ABIL_AFFECT_VNUM(abil) != NOTHING) ? ABIL_AFFECT_VNUM(abil) : ATYPE_BUFF;
	
	total_points = get_ability_type_data(data, ABILT_ROOM_AFFECT)->scale_points;
	remaining_points = total_points;
	
	if (total_points <= 0) {
		return;
	}
	
	// determine duration (in seconds)
	dur = IS_CLASS_ABILITY(ch, ABIL_VNUM(abil)) ? ABIL_LONG_DURATION(abil) : ABIL_SHORT_DURATION(abil);
	
	// affect flags? cost == level 100 ability
	if (room_targ && ABIL_AFFECTS(abil)) {
		remaining_points -= count_bits(ABIL_AFFECTS(abil)) * config_get_double("scale_points_at_100");
		remaining_points = MAX(0, remaining_points);
		
		af = create_flag_aff(affect_vnum, dur, ABIL_AFFECTS(abil), ch);
		affect_to_room(room_targ, af);
		free(af);	// affect_to_room duplicates affects
	}
	
	data->success = TRUE;
}


/**
* Various types of teleportation.
*
* DO_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
DO_ABIL(do_teleport_ability) {
	room_data *was_in = IN_ROOM(ch), *to_room;
	bool infiltrate;
	
	to_room = (room_targ ? room_targ : (vict ? IN_ROOM(vict) : (vvict ? IN_ROOM(vvict) : ovict ? obj_room(ovict) : IN_ROOM(ch))));
	
	if (to_room != IN_ROOM(ch)) {
		infiltrate = HOME_ROOM(to_room) != HOME_ROOM(IN_ROOM(ch)) && !can_use_room(ch, to_room, GUESTS_ALLOWED);
		
		if (infiltrate && !has_player_tech(ch, PTECH_INFILTRATE_UPGRADE) && !player_tech_skill_check(ch, PTECH_INFILTRATE, DIFF_HARD)) {
			// failure (will message as a fail on the ability)
			// fall through to log infiltrate attempt
		}
		else if (!pre_greet_mtrigger(ch, to_room, NO_DIR, "ability")) {
			// no message (will message as a fail on the ability)
			return;
		}
		else {
			// any existing adventure summon location is no longer valid after a voluntary teleport
			// ... if the travel is further than the ability's range (which is used for random or targeted teleports, and is usually very close)
			if (compute_distance(was_in, to_room) > get_ability_data_value(abil, ADL_RANGE, TRUE)) {
				cancel_adventure_summon(ch);
			}
			
			char_from_room(ch);
			char_to_room(ch, to_room);
			qt_visit_room(ch, to_room);
			look_at_room(ch);
			
			send_ability_special_messages(ch, vict, ovict, abil, data, NULL, 0);
			
			GET_LAST_DIR(ch) = NO_DIR;
			RESET_LAST_MESSAGED_TEMPERATURE(ch);
		
			enter_wtrigger(IN_ROOM(ch), ch, NO_DIR, "ability");
			entry_memory_mtrigger(ch);
			greet_mtrigger(ch, NO_DIR, "ability");
			greet_memory_mtrigger(ch);
			greet_vtrigger(ch, NO_DIR, "ability");
			
			msdp_update_room(ch);	// once we're sure we're staying
			data->success = TRUE;
			
			// teleport companion, too
			if (GET_COMPANION(ch) && !FIGHTING(GET_COMPANION(ch)) && IN_ROOM(ch) != was_in && IN_ROOM(GET_COMPANION(ch)) == was_in) {
				act("$n vanishes!", TRUE, GET_COMPANION(ch), NULL, NULL, TO_ROOM);
				char_to_room(GET_COMPANION(ch), IN_ROOM(ch));
				send_ability_special_messages(GET_COMPANION(ch), vict, ovict, abil, data, NULL, 0);
				enter_wtrigger(IN_ROOM(ch), GET_COMPANION(ch), NO_DIR, "ability");
				entry_memory_mtrigger(GET_COMPANION(ch));
				greet_mtrigger(GET_COMPANION(ch), NO_DIR, "ability");
				greet_memory_mtrigger(GET_COMPANION(ch));
				greet_vtrigger(GET_COMPANION(ch), NO_DIR, "ability");
			}
		}
		
		// chance to log
		if (infiltrate && ROOM_OWNER(to_room) && !player_tech_skill_check(ch, PTECH_INFILTRATE_UPGRADE, DIFF_HARD)) {
			if (has_player_tech(ch, PTECH_INFILTRATE_UPGRADE)) {
				log_to_empire(ROOM_OWNER(to_room), ELOG_HOSTILITY, "An infiltrator has been spotted shadowstepping into (%d, %d)!", X_COORD(IN_ROOM(vict)), Y_COORD(IN_ROOM(vict)));
			}
			else {
				log_to_empire(ROOM_OWNER(to_room), ELOG_HOSTILITY, "%s has been spotted shadowstepping into (%d, %d)!", PERS(ch, ch, FALSE), X_COORD(IN_ROOM(vict)), Y_COORD(IN_ROOM(vict)));
			}
		}
	
		if (infiltrate && ROOM_OWNER(to_room)) {
			// distrust just in case
			trigger_distrust_from_stealth(ch, ROOM_OWNER(to_room));
			gain_player_tech_exp(ch, PTECH_INFILTRATE, 50);
			gain_player_tech_exp(ch, PTECH_INFILTRATE_UPGRADE, 50);
			add_offense(ROOM_OWNER(to_room), OFFENSE_INFILTRATED, ch, IN_ROOM(ch), offense_was_seen(ch, ROOM_OWNER(to_room), was_in) ? OFF_SEEN : NOBITS);
		}
	}
}


/**
* Performs an ability typed by a character. This function find the targets,
* and pre-validates the ability.
*
* @param char_data *ch The person who typed the command.
* @param ability_data *abil The ability being used.
* @param char *argument The typed-in args.
*/
void perform_ability_command(char_data *ch, ability_data *abil, char *argument) {
	char arg[MAX_INPUT_LENGTH], *argptr = arg;
	struct ability_exec *data;
	struct empire_city_data *city;
	vehicle_data *vvict = NULL;
	char_data *vict = NULL;
	obj_data *ovict = NULL;
	room_data *room_targ = NULL, *to_room, *find_room;
	bool has = FALSE;
	int cap, find_dir, iter, level, number;
	
	if (!ch || !abil) {
		log("SYSERR: perform_ability_command called without %s.", ch ? "ability" : "character");
		return;
	}
	
	if (ABILITY_FLAGGED(abil, ABILF_OVER_TIME) && GET_ACTION(ch) == ACT_OVER_TIME_ABILITY && GET_ACTION_VNUM(ch, 0) == ABIL_VNUM(abil)) {
		// already doing it
		msg_to_char(ch, "You stop what you were doing.\r\n");
		cancel_action(ch);
		return;
	}
	if (ABILITY_FLAGGED(abil, ABILF_OVER_TIME) && GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already doing something else.\r\n");
		return;
	}
	
	// 1. pre-targeting checks
	if (!check_ability_pre_target(ch, abil)) {
		// sent its own error message
		return;
	}
	
	// 2. ATAR_x: find the target
	skip_spaces(&argument);
	if (ABIL_TARGETS(abil) == ATAR_IGNORE) {
		// if it has _only_ ATAR_IGNORE, skip argument entirely
		has = TRUE;
	}
	else if (*argument) {
		argument = one_argument(argument, arg);
		skip_spaces(&argument);	// anything left
		
		// extract numbering
		number = get_number(&argptr);
		
		// char targets
		if (!has && (IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_ROOM | ATAR_SELF_ONLY))) {
			if ((vict = get_char_vis(ch, argptr, &number, FIND_CHAR_ROOM)) != NULL) {
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_CLOSEST)) {
			if ((vict = find_closest_char(ch, argptr, FALSE)) != NULL) {
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_WORLD)) {
			if ((vict = get_char_vis(ch, argptr, &number, FIND_CHAR_WORLD)) != NULL) {
				has = TRUE;
			}
		}
		
		// obj targets
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_INV)) {
			if ((ovict = get_obj_in_list_vis(ch, argptr, &number, ch->carrying)) != NULL) {
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_EQUIP) && number > 0) {
			for (iter = 0; !has && iter < NUM_WEARS; ++iter) {
				if (GET_EQ(ch, iter) && isname(argptr, GET_EQ(ch, iter)->name) && --number == 0) {
					ovict = GET_EQ(ch, iter);
					has = TRUE;
				}
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_ROOM)) {
			if ((ovict = get_obj_in_list_vis(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL) {
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_WORLD)) {
			if ((ovict = get_obj_vis(ch, argptr, &number)) != NULL) {
				has = TRUE;
			}
		}
		
		// vehicle targets
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_VEH_ROOM)) {
			if ((vvict = get_vehicle_in_room_vis(ch, argptr, &number))) {
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_VEH_WORLD)) {
			if ((vvict = get_vehicle_vis(ch, argptr, &number))) {
				has = TRUE;
			}
		}
		
		// room target
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_HERE)) {
			if (is_abbrev(argptr, "room") || is_abbrev(argptr, "area") || is_abbrev(argptr, "here")) {
				room_targ = IN_ROOM(ch);
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_EXIT)) {
			if (is_abbrev(argptr, "exit") && (find_room = get_exit_room(IN_ROOM(ch)))) {
				room_targ = find_room;
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_HOME)) {
			if (is_abbrev(argptr, "home") && (find_room = find_home(ch)) && can_use_room(ch, find_room, GUESTS_ALLOWED)) {
				room_targ = find_room;
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_ADJACENT)) {
			if ((find_dir = parse_direction(ch, argptr)) != NO_DIR && is_flat_dir[find_dir] && (to_room = dir_to_room(IN_ROOM(ch), find_dir, TRUE)) && to_room != IN_ROOM(ch)) {
				room_targ = to_room;
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_COORDS)) {
			if (HAS_NAVIGATION(ch) && (find_room = parse_room_from_coords(argptr))) {
				room_targ = find_room;
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_CITY)) {
			if (GET_LOYALTY(ch) && (city = find_city_by_name(GET_LOYALTY(ch), argptr))) {
				room_targ = city->location;
				has = TRUE;
			}
		}
	}
	else if (IS_SET(ABIL_TARGETS(abil), ATAR_IGNORE)) {
		// if it had multiple types including IGNORE, but no arg:
		has = TRUE;
	}
	else {	// no arg and no tar-ignore
		// room target
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_RANDOM)) {
			room_targ = get_random_room_for_ability(ch, abil);
			has = TRUE;
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_HERE)) {
			room_targ = IN_ROOM(ch);
			has = TRUE;
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_HOME)) {
			if ((room_targ = find_home(ch))) {
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_ROOM_EXIT)) {
			if ((room_targ = get_exit_room(IN_ROOM(ch)))) {
				has = TRUE;
			}
		}
		
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_FIGHT_SELF)) {
			if (FIGHTING(ch) != NULL) {
				vict = ch;
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_FIGHT_VICT)) {
			if (FIGHTING(ch) != NULL) {
				vict = FIGHTING(ch);
				has = TRUE;
			}
		}
		// if no target specified, and the spell isn't violent, default to self
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_ROOM) && !ABIL_IS_VIOLENT(abil)) {
			vict = ch;
			has = TRUE;
		}
		if (!has) {
			if (abil_has_custom_message(abil, ABIL_CUSTOM_NO_ARGUMENT)) {
				act(abil_get_custom_message(abil, ABIL_CUSTOM_NO_ARGUMENT), FALSE, ch, NULL, NULL, TO_CHAR);
			}
			else {
				msg_to_char(ch, "&Z%s %s?\r\n", ABIL_COMMAND(abil) ? ABIL_COMMAND(abil) : "Use that ability on", IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_ROOM | ATAR_CHAR_CLOSEST | ATAR_CHAR_WORLD) ? "whom" : "what");
			}
			return;
		}
	}
	
	// 3. target validation
	if (!has) {
		if (IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_ROOM | ATAR_CHAR_CLOSEST | ATAR_CHAR_WORLD)) {
			send_config_msg(ch, "no_person");
		}
		else {
			msg_to_char(ch, "There's nothing like that here.\r\n");
		}
		return;
	}
	else if (!validate_ability_target(ch, abil, vict, ovict, vvict, room_targ, TRUE)) {
		// sent own message
		return;
	}
	
	// 4. determine correct level (sometimes limited by skill level)
	level = get_approximate_level(ch);
	if (!IS_NPC(ch) && ABIL_ASSIGNED_SKILL(abil) && (cap = get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil)))) < CLASS_SKILL_CAP) {
		level = MIN(level, cap);	// constrain by skill level
	}
	
	// 5. exec data to pass through
	CREATE(data, struct ability_exec, 1);
	data->abil = abil;
	data->cost = ABIL_COST(abil);	// base cost, may be modified
	data->should_charge_cost = TRUE;	// may be turned off any time
	data->matching_role = has_matching_role(ch, abil, TRUE);
	GET_RUNNING_ABILITY_DATA(ch) = data;
	
	// 6. ** run the ability **
	call_ability(ch, abil, argument, vict, ovict, vvict, room_targ, level, RUN_ABIL_NORMAL, data);
	
	// 7. costs and consequences
	if (data->should_charge_cost) {
		// charge costs and cooldown unless the ability stopped itself -- regardless of success
		charge_ability_cost(ch, ABIL_COST_TYPE(abil), data->cost, ABIL_COOLDOWN(abil), ABIL_COOLDOWN_SECS(abil), ABIL_WAIT_TYPE(abil));
		if (ABIL_RESOURCE_COST(abil)) {
			extract_resources(ch, ABIL_RESOURCE_COST(abil), FALSE, GET_ACTION(ch) == ACT_OVER_TIME_ABILITY ? &GET_ACTION_RESOURCES(ch) : NULL);
		}
	}
	else {
		// has wait type even without cost
		command_lag(ch, ABIL_WAIT_TYPE(abil));
	}
	if (data->success) {
		// only if successful
		if (ABILITY_FLAGGED(abil, ABILF_LIMIT_CROWD_CONTROL) && ABIL_AFFECT_VNUM(abil) != NOTHING) {
			limit_crowd_control(vict, ABIL_AFFECT_VNUM(abil));
		}
		run_ability_hooks(ch, AHOOK_ABILITY, ABIL_VNUM(abil), vict, ovict, vvict, room_targ);
	}
	
	// 9. clean up data
	GET_RUNNING_ABILITY_DATA(ch) = NULL;
	free_ability_exec(data);
}


/**
* PREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
PREP_ABIL(prep_action_ability) {
	get_ability_type_data(data, ABILT_ACTION)->scale_points = standard_ability_scale(ch, abil, level, ABILT_ACTION, data);
}


/**
* This function 'stops' if the ability is a toggle and you're toggling it off,
* which keeps it from charging/cooldowning.
* PREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
PREP_ABIL(prep_buff_ability) {
	any_vnum affect_vnum;
	
	affect_vnum = (ABIL_AFFECT_VNUM(abil) != NOTHING) ? ABIL_AFFECT_VNUM(abil) : ATYPE_BUFF;
	
	// toggle off?
	if (ABILITY_FLAGGED(abil, ABILF_TOGGLE) && vict == ch && affected_by_spell_from_caster(vict, affect_vnum, ch)) {
		send_ability_toggle_messages(vict, abil, data);
		affect_from_char_by_caster(vict, affect_vnum, ch, TRUE);
		data->stop = TRUE;
		data->should_charge_cost = FALSE;	// free cancel
		data->success = TRUE;	// I think this is a success?
		return;
	}
	
	get_ability_type_data(data, ABILT_BUFF)->scale_points = standard_ability_scale(ch, abil, level, ABILT_BUFF, data);
}


// PREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
PREP_ABIL(prep_building_damage_ability) {
	get_ability_type_data(data, ABILT_BUILDING_DAMAGE)->scale_points = standard_ability_scale(ch, abil, level, ABILT_BUILDING_DAMAGE, data);
}


/**
* Determines if ovict is a valid target for creating the liquid.
* 
* PREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
PREP_ABIL(prep_conjure_liquid_ability) {
	char buf[256];
	struct interaction_item *interact;
	generic_data *existing, *gen;
	
	get_ability_type_data(data, ABILT_CONJURE_LIQUID)->scale_points = standard_ability_scale(ch, abil, level, ABILT_CONJURE_LIQUID, data);
	
	if (!ovict) {
		// should this error? or just do nothing
		// data->stop = TRUE;
		return;
	}
	if (!IS_DRINK_CONTAINER(ovict)) {
		act("$p is not a drink container.", FALSE, ch, ovict, NULL, TO_CHAR);
		CANCEL_ABILITY(data);
		return;
	}
	if (GET_DRINK_CONTAINER_CONTENTS(ovict) == GET_DRINK_CONTAINER_CAPACITY(ovict)) {
		act("$p is already full.", FALSE, ch, ovict, NULL, TO_CHAR);
		CANCEL_ABILITY(data);
		return;
	}
	if (GET_DRINK_CONTAINER_CONTENTS(ovict) > 0 && (existing = find_generic(GET_DRINK_CONTAINER_TYPE(ovict), GENERIC_LIQUID))) {
		// check compatible contents
		LL_FOREACH(ABIL_INTERACTIONS(abil), interact) {
			if (interact->type != INTERACT_LIQUID_CONJURE) {
				continue;	// not a liquid
			}
			if (!(gen = find_generic(interact->vnum, GENERIC_LIQUID))) {
				continue;	// invalid liquid
			}
			if (GEN_VNUM(gen) == GEN_VNUM(existing)) {
				continue;	// ok: same liquid
			}
			
			// ok, both are different liquids... is it a problem?
			if (!GEN_FLAGGED(existing, GEN_BASIC) || !IS_SET(GET_LIQUID_FLAGS(gen), LIQF_WATER) || !IS_SET(GET_LIQUID_FLAGS(existing), LIQF_WATER)) {
				// 1 is not water, or existing is not basic water
				snprintf(buf, sizeof(buf), "$p already contains %s.", GET_LIQUID_NAME(existing));
				act(buf, FALSE, ch, ovict, NULL, TO_CHAR);
				CANCEL_ABILITY(data);
				return;
			}
		}
	}
	
	// otherwise it seems ok
}


/**
* Determines if player can conjure objects.
* 
* PREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
PREP_ABIL(prep_conjure_object_ability) {
	bool has_any, one_ata, any_inv, any_room;
	int any_size, iter;
	struct interaction_item *interact;
	obj_data *proto;
	
	get_ability_type_data(data, ABILT_CONJURE_OBJECT)->scale_points = standard_ability_scale(ch, abil, level, ABILT_CONJURE_OBJECT, data);
	
	one_ata = ABILITY_FLAGGED(abil, ABILF_ONE_AT_A_TIME) ? TRUE : FALSE;
	
	// check interactions
	any_size = 0;
	any_inv = any_room = FALSE;
	
	LL_FOREACH(ABIL_INTERACTIONS(abil), interact) {
		if (interact->type != INTERACT_OBJECT_CONJURE) {
			continue;	// not an object
		}
		if (!(proto = obj_proto(interact->vnum))) {
			continue;	// no proto
		}
		
		// data to find
		any_size = MAX(any_size, obj_carry_size(proto));
		has_any = FALSE;
		
		if (CAN_WEAR(proto, ITEM_WEAR_TAKE)) {
			any_inv = TRUE;
			for (iter = 0; iter < NUM_WEARS && !has_any; ++iter) {
				if (one_ata && GET_EQ(ch, iter) && count_objs_by_vnum(interact->vnum, GET_EQ(ch, iter))) {
					has_any = TRUE;
				}
			}
			if (one_ata && !has_any && count_objs_by_vnum(interact->vnum, ch->carrying)) {
				has_any = TRUE;
			}
			
			// oops
			if (one_ata && has_any) {
				act("You can't use that ability because you already have $p.", FALSE, ch, proto, NULL, TO_CHAR);
			}
		}
		else {	// no-take
			any_room = TRUE;
			if (one_ata && count_objs_by_vnum(interact->vnum, ROOM_CONTENTS(IN_ROOM(ch)))) {
				act("You can't use that ability because $p is already here.", FALSE, ch, proto, NULL, TO_CHAR);
				has_any = TRUE;
			}
		}
		
		if (one_ata && has_any) {
			CANCEL_ABILITY(data);
			return;
		}
	}
	
	if (any_inv && any_size > 0 && IS_CARRYING_N(ch) + any_size > CAN_CARRY_N(ch)) {
		msg_to_char(ch, "You can't use that ability because your inventory is full.\r\n");
		CANCEL_ABILITY(data);
		return;
	}
	if (any_room && any_size > 0 && ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_ITEM_LIMIT) && (any_size + count_objs_in_room(IN_ROOM(ch))) > config_get_int("room_item_limit")) {
		msg_to_char(ch, "You can't use that ability because the room is full.\r\n");
		CANCEL_ABILITY(data);
		return;
	}
}


/**
* Determines if player can conjure vehicles.
* 
* PREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
PREP_ABIL(prep_conjure_vehicle_ability) {
	struct interaction_item *interact;
	vehicle_data *proto, *viter;
	
	get_ability_type_data(data, ABILT_CONJURE_VEHICLE)->scale_points = standard_ability_scale(ch, abil, level, ABILT_CONJURE_VEHICLE, data);
	
	// check interactions
	LL_FOREACH(ABIL_INTERACTIONS(abil), interact) {
		if (interact->type != INTERACT_VEHICLE_CONJURE) {
			continue;	// not an object
		}
		if (!(proto = vehicle_proto(interact->vnum))) {
			continue;	// no proto
		}
		
		// check room for any if one-at-a-time
		if (ABILITY_FLAGGED(abil, ABILF_ONE_AT_A_TIME)) {
			DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), viter, next_in_room) {
				if (VEH_VNUM(viter) == interact->vnum && (!VEH_OWNER(viter) || VEH_OWNER(viter) == GET_LOYALTY(ch))) {
					act("You can't use that ability because $V is already here.", FALSE, ch, NULL, viter, TO_CHAR | ACT_VEH_VICT);
					CANCEL_ABILITY(data);
					return;
				}
			}
		}
		
		// check vehicle flagging
		if (VEH_FLAGGED(proto, VEH_NO_BUILDING) && (ROOM_IS_CLOSED(IN_ROOM(ch)) || (GET_BUILDING(IN_ROOM(ch)) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_OPEN)))) {
			msg_to_char(ch, "You can't do that inside a building.\r\n");
			CANCEL_ABILITY(data);
			return;
		}
		if (GET_ROOM_VEHICLE(IN_ROOM(ch)) && (VEH_FLAGGED(proto, VEH_NO_LOAD_ONTO_VEHICLE) || !VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(ch)), VEH_CARRY_VEHICLES))) {
			msg_to_char(ch, "You can't do that inside a %s.\r\n", VEH_OR_BLD(GET_ROOM_VEHICLE(IN_ROOM(ch))));
			CANCEL_ABILITY(data);
			return;
		}
		if (VEH_SIZE(proto) > 0 && total_vehicle_size_in_room(IN_ROOM(ch), NULL) + VEH_SIZE(proto) > config_get_int("vehicle_size_per_tile")) {
			msg_to_char(ch, "This area is already too full to do that.\r\n");
			CANCEL_ABILITY(data);
			return;
		}
		if (VEH_SIZE(proto) == 0 && total_small_vehicles_in_room(IN_ROOM(ch), NULL) >= config_get_int("vehicle_max_per_tile")) {
			msg_to_char(ch, "This area is already too full to do that.\r\n");
			CANCEL_ABILITY(data);
			return;
		}
	}
}


/**
* PREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
PREP_ABIL(prep_damage_ability) {
	get_ability_type_data(data, ABILT_DAMAGE)->scale_points = standard_ability_scale(ch, abil, level, ABILT_DAMAGE, data);
}


/**
* PREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
PREP_ABIL(prep_dot_ability) {
	get_ability_type_data(data, ABILT_DOT)->scale_points = standard_ability_scale(ch, abil, level, ABILT_DOT, data);
}


/**
* PREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
PREP_ABIL(prep_paint_building_ability) {
	get_ability_type_data(data, ABILT_PAINT_BUILDING)->scale_points = standard_ability_scale(ch, abil, level, ABILT_PAINT_BUILDING, data);
}


/**
* This function 'stops' if the ability is a toggle and you're toggling it off,
* which keeps it from charging/cooldowning.
* PREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, dataPREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
*/
PREP_ABIL(prep_room_affect_ability) {
	any_vnum affect_vnum;
	
	affect_vnum = (ABIL_AFFECT_VNUM(abil) != NOTHING) ? ABIL_AFFECT_VNUM(abil) : ATYPE_BUFF;
	
	// toggle off?
	if (ABILITY_FLAGGED(abil, ABILF_TOGGLE) && room_targ && room_affected_by_spell_from_caster(room_targ, affect_vnum, ch)) {
		send_ability_toggle_messages(ch, abil, data);
		affect_from_room_by_caster(room_targ, affect_vnum, ch, TRUE);
		data->stop = TRUE;
		data->should_charge_cost = FALSE;	// free cancel
		data->success = TRUE;	// I think this is a success?
		return;
	}
	
	// one-at-a-time?
	if (ABILITY_FLAGGED(abil, ABILF_ONE_AT_A_TIME) && room_targ && room_affected_by_spell_from_caster(room_targ, affect_vnum, ch)) {
		msg_to_char(ch, "The area is already affected by that ability.\r\n");
		CANCEL_ABILITY(data);
		return;
	}
	
	get_ability_type_data(data, ABILT_ROOM_AFFECT)->scale_points = standard_ability_scale(ch, abil, level, ABILT_ROOM_AFFECT, data);
}


// PREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, dataPREP_ABIL provides: ch, abil, level, vict, ovict, vvict, room_targ, data
PREP_ABIL(prep_teleport_ability) {
	room_data *to_room;
	
	// determine what we are teleporting to
	to_room = (room_targ ? room_targ : (vict ? IN_ROOM(vict) : (vvict ? IN_ROOM(vvict) : ovict ? obj_room(ovict) : IN_ROOM(ch))));
	
	// see if we're infiltrating
	// TODO could this be moved to a function
	if (HOME_ROOM(to_room) != HOME_ROOM(IN_ROOM(ch))) {
		if (ROOM_OWNER(to_room) && !can_use_room(ch, to_room, GUESTS_ALLOWED) && (!GET_LOYALTY(ch) || !has_relationship(GET_LOYALTY(ch), ROOM_OWNER(to_room), DIPL_WAR | DIPL_THIEVERY))) {
			// stealthable seems to apply
			if (!PRF_FLAGGED(ch, PRF_STEALTHABLE)) {
				msg_to_char(ch, "You need to toggle 'stealthable' on to do that.\r\n");
				CANCEL_ABILITY(data);
				return;
			}
			if (GET_LOYALTY(ch) && GET_RANK(ch) < EMPIRE_PRIV(GET_LOYALTY(ch), PRIV_STEALTH)) {
				msg_to_char(ch, "You don't have the empire rank required to commit stealth acts.\r\n");
				CANCEL_ABILITY(data);
				return;
			}
			if (!can_infiltrate(ch, ROOM_OWNER(to_room))) {
				// sends own message
				CANCEL_ABILITY(data);
				return;
			}
			
			// otherwise attempt the infiltrate and logs on failure
			if (!has_player_tech(ch, PTECH_INFILTRATE_UPGRADE) && !player_tech_skill_check(ch, PTECH_INFILTRATE, DIFF_HARD)) {
				// infiltrate failed -- log it
				if (has_player_tech(ch, PTECH_INFILTRATE_UPGRADE)) {
					log_to_empire(ROOM_OWNER(to_room), ELOG_HOSTILITY, "An infiltrator has been spotted at (%d, %d)!", X_COORD(to_room), Y_COORD(to_room));
				}
				else {
					log_to_empire(ROOM_OWNER(to_room), ELOG_HOSTILITY, "%s has been spotted infiltrating at (%d, %d)!", PERS(ch, ch, FALSE), X_COORD(to_room), Y_COORD(to_room));
				}
				msg_to_char(ch, "You fail to infiltrate.\r\n");
				CANCEL_ABILITY(data);
				return;
			}
		}
	}
	
	get_ability_type_data(data, ABILT_TELEPORT)->scale_points = standard_ability_scale(ch, abil, level, ABILT_TELEPORT, data);
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common ability problems and reports them to ch.
*
* @param ability_data *abil The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_ability(ability_data *abil, char_data *ch) {
	ability_data *abiter, *next_abiter;
	struct ability_data_list *adl;
	struct interaction_item *interact;
	bitvector_t bits;
	bool problem = FALSE;
	int iter;
	
	const char *ignore_same_command[] = {
		"chant", "conjure", "ritual",
		"\n"
	};
	
	if (!ABIL_NAME(abil) || !*ABIL_NAME(abil) || !str_cmp(ABIL_NAME(abil), default_ability_name)) {
		olc_audit_msg(ch, ABIL_VNUM(abil), "No name set");
		problem = TRUE;
	}
	if (ABIL_MASTERY_ABIL(abil) != NOTHING) {
		if (ABIL_MASTERY_ABIL(abil) == ABIL_VNUM(abil)) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "Mastery ability is itself");
			problem = TRUE;
		}
		if (!find_ability_by_vnum(ABIL_MASTERY_ABIL(abil))) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "Mastery ability is invalid");
			problem = TRUE;
		}
	}
	if (ABIL_COMMAND(abil) && ABIL_TARGETS(abil) == NOBITS) {
		olc_audit_msg(ch, ABIL_VNUM(abil), "No targets set");
		problem = TRUE;
	}
	if (ABILITY_FLAGGED(abil, ABILF_OVER_TIME) && ABIL_HOOKS(abil)) {
		olc_audit_msg(ch, ABIL_VNUM(abil), "OVER-TIME flag with ability hooks");
		problem = TRUE;
	}
	if (ABILITY_FLAGGED(abil, ABILF_REPEAT_OVER_TIME) && !ABILITY_FLAGGED(abil, ABILF_OVER_TIME)) {
		olc_audit_msg(ch, ABIL_VNUM(abil), "REPEAT-OVER-TIME flag without OVER-TIME");
		problem = TRUE;
	}
	if (ABIL_DIFFICULTY(abil) != DIFF_TRIVIAL && !abil_has_custom_message(abil, ABIL_CUSTOM_FAIL_SELF_TO_CHAR) && !abil_has_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_CHAR)) {
		olc_audit_msg(ch, ABIL_VNUM(abil), "No fail messages");
		problem = TRUE;
	}
	
	// interactions
	problem |= audit_interactions(ABIL_VNUM(abil), ABIL_INTERACTIONS(abil), TYPE_ABIL, ch);
	
	// other abils
	HASH_ITER(hh, ability_table, abiter, next_abiter) {
		if (abiter == abil || ABIL_VNUM(abiter) == ABIL_VNUM(abil)) {
			continue;
		}
		if (!str_cmp(ABIL_NAME(abiter), ABIL_NAME(abil))) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "Same name as ability %d", ABIL_VNUM(abiter));
			problem = TRUE;
		}
		if (ABIL_COMMAND(abil) && ABIL_COMMAND(abiter) && !str_cmp(ABIL_COMMAND(abil), ABIL_COMMAND(abiter)) && search_block(ABIL_COMMAND(abil), ignore_same_command, TRUE) == NOTHING) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "Same command as ability %d %s", ABIL_VNUM(abiter), ABIL_NAME(abiter));
			problem = TRUE;
		}
	}
	
	// data
	LL_FOREACH(ABIL_DATA(abil), adl) {
		switch (adl->type) {
			case ADL_ACTION: {
				// ABIL_ACTION_x: audits
				switch (adl->vnum) {
					case ABIL_ACTION_DETECT_HIDE:
					case ABIL_ACTION_DETECT_EARTHMELD:
					case ABIL_ACTION_DETECT_PLAYERS_AROUND: {
						if (!abil_has_custom_message(abil, ABIL_CUSTOM_PER_CHAR_TO_CHAR)) {
							olc_audit_msg(ch, ABIL_VNUM(abil), "Missing per-char-to-char message for ability action");
							problem = TRUE;
						}
						break;
					}
					case ABIL_ACTION_DETECT_ADVENTURES_AROUND:
					case ABIL_ACTION_MAGIC_GROWTH: {
						if (!abil_has_custom_message(abil, ABIL_CUSTOM_SPEC_TO_CHAR)) {
							olc_audit_msg(ch, ABIL_VNUM(abil), "Missing spec-to-char message for ability action");
							problem = TRUE;
						}
						break;
					}
					case ABIL_ACTION_DEVASTATE_AREA: {
						if (!abil_has_custom_message(abil, ABIL_CUSTOM_PER_ITEM_TO_CHAR)) {
							olc_audit_msg(ch, ABIL_VNUM(abil), "Missing per-item-to-char message for ability action");
							problem = TRUE;
						}
						break;
					}
					case ABIL_LIMIT_ITEM_TYPE: {
						if (!adl->misc) {
							olc_audit_msg(ch, ABIL_VNUM(abil), "No type set on item type limitation");
							problem = TRUE;
						}
						break;
					}
				}
				break;
			}
		}
	}
	
	// violent types?
	if (IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE | ABILT_DOT | ABILT_BUILDING_DAMAGE) && !ABILITY_FLAGGED(abil, ABILF_VIOLENT)) {
		olc_audit_msg(ch, ABIL_VNUM(abil), "Missing VIOLENT flag");
		problem = TRUE;
	}
	
	// buffs and passives
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_PASSIVE_BUFF)) {
		for (bits = ABIL_AFFECTS(abil), iter = 0; bits; bits >>= 1, ++iter) {
			if (*affected_bits[iter] == '\n' && bits != NOBITS) {
				olc_audit_msg(ch, ABIL_VNUM(abil), "Invalid affect flags are set");
				problem = TRUE;
			}
		}
	}
	
	// conjure liquid
	if (IS_SET(ABIL_TYPES(abil), ABILT_CONJURE_LIQUID)) {
		if (!has_interaction(ABIL_INTERACTIONS(abil), INTERACT_LIQUID_CONJURE)) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "No LIQUID-CONJURE interactions set");
			problem = TRUE;
		}
		if (!IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_INV | ATAR_OBJ_ROOM | ATAR_OBJ_WORLD | ATAR_OBJ_EQUIP)) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "Must target object");
			problem = TRUE;
		}
		
		LL_FOREACH(ABIL_INTERACTIONS(abil), interact) {
			if (interact->type != INTERACT_LIQUID_CONJURE) {
				olc_audit_msg(ch, ABIL_VNUM(abil), "Has non-LIQUID-CONJURE interaction.");
				problem = TRUE;
				break;
			}
		}
	}
	
	// conjure object
	if (IS_SET(ABIL_TYPES(abil), ABILT_CONJURE_OBJECT)) {
		if (!has_interaction(ABIL_INTERACTIONS(abil), INTERACT_OBJECT_CONJURE)) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "No OBJECT-CONJURE interactions set");
			problem = TRUE;
		}
		
		LL_FOREACH(ABIL_INTERACTIONS(abil), interact) {
			if (interact->type != INTERACT_OBJECT_CONJURE) {
				olc_audit_msg(ch, ABIL_VNUM(abil), "Has non-OBJECT-CONJURE interaction.");
				problem = TRUE;
				break;
			}
		}
	}
	
	// conjure vehicle
	if (IS_SET(ABIL_TYPES(abil), ABILT_CONJURE_VEHICLE)) {
		if (!has_interaction(ABIL_INTERACTIONS(abil), INTERACT_VEHICLE_CONJURE)) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "No VEHICLE-CONJURE interactions set");
			problem = TRUE;
		}
		
		LL_FOREACH(ABIL_INTERACTIONS(abil), interact) {
			if (interact->type != INTERACT_VEHICLE_CONJURE) {
				olc_audit_msg(ch, ABIL_VNUM(abil), "Has non-VEHICLE-CONJURE interaction.");
				problem = TRUE;
				break;
			}
		}
	}
	
	// dots
	if (IS_SET(ABIL_TYPES(abil), ABILT_DOT)) {
		if (ABIL_SHORT_DURATION(abil) == UNLIMITED) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "Unlimited short duration not supported on DOTs");
			problem = TRUE;
		}
		if (ABIL_LONG_DURATION(abil) == UNLIMITED) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "Unlimited long duration not supported on DOTs");
			problem = TRUE;
		}
	}
	
	// room affect
	if (IS_SET(ABIL_TYPES(abil), ABILT_ROOM_AFFECT)) {
		if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_PASSIVE_BUFF)) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "Incompatible types: ROOM-AFFECT with a BUFF type");
			problem = TRUE;
		}
		if (!ABIL_AFFECTS(abil)) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "No room affect flags set");
			problem = TRUE;
		}
		
		for (bits = ABIL_AFFECTS(abil), iter = 0; bits; bits >>= 1, ++iter) {
			if (*room_aff_bits[iter] == '\n' && bits != NOBITS) {
				olc_audit_msg(ch, ABIL_VNUM(abil), "Invalid room affects are set");
				problem = TRUE;
			}
		}
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param ability_data *abil The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_ability(ability_data *abil, bool detail) {
	static char output[MAX_STRING_LENGTH];
	char part[MAX_STRING_LENGTH];
	ability_data *mastery;
	
	if (detail) {
		if ((mastery = find_ability_by_vnum(ABIL_MASTERY_ABIL(abil)))) {
			snprintf(part, sizeof(part), " (%s)", ABIL_NAME(mastery));
		}
		else {
			*part = '\0';
		}
		
		snprintf(output, sizeof(output), "[%5d] %s%s", ABIL_VNUM(abil), ABIL_NAME(abil), part);
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", ABIL_VNUM(abil), ABIL_NAME(abil));
	}
		
	return output;
}


/**
* Searches for all uses of an ability and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The ability vnum.
*/
void olc_search_ability(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	ability_data *abil = find_ability_by_vnum(vnum);
	struct global_data *glb, *next_glb;
	ability_data *abiter, *next_abiter;
	craft_data *craft, *next_craft;
	morph_data *morph, *next_morph;
	quest_data *quest, *next_quest;
	skill_data *skill, *next_skill;
	progress_data *prg, *next_prg;
	augment_data *aug, *next_aug;
	struct synergy_ability *syn;
	social_data *soc, *next_soc;
	class_data *cls, *next_cls;
	struct class_ability *clab;
	struct skill_ability *skab;
	int size, found;
	bool any;
	
	if (!abil) {
		msg_to_char(ch, "There is no ability %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of ability %d (%s):\r\n", vnum, ABIL_NAME(abil));
	
	// abilities
	HASH_ITER(hh, ability_table, abiter, next_abiter) {
		any = FALSE;
		any |= (ABIL_MASTERY_ABIL(abiter) == vnum);
		any |= has_ability_hook(abiter, AHOOK_ABILITY, vnum);
		any |= find_ability_data_entry_for(abiter, ADL_PARENT, vnum) ? TRUE : FALSE;
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "ABIL [%5d] %s\r\n", ABIL_VNUM(abiter), ABIL_NAME(abiter));
		}
	}
	
	// augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		if (GET_AUG_ABILITY(aug) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "AUG [%5d] %s\r\n", GET_AUG_VNUM(aug), GET_AUG_NAME(aug));
		}
	}
	
	// classes
	HASH_ITER(hh, class_table, cls, next_cls) {
		LL_FOREACH(CLASS_ABILITIES(cls), clab) {
			if (clab->vnum == vnum) {
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "CLS [%5d] %s\r\n", CLASS_VNUM(cls), CLASS_NAME(cls));
				break;	// only need 1
			}
		}
	}
	
	// update crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		if (GET_CRAFT_ABILITY(craft) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
		}
	}
	
	// globals
	HASH_ITER(hh, globals_table, glb, next_glb) {
		if (GET_GLOBAL_ABILITY(glb) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "GLB [%5d] %s\r\n", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb));
		}
	}
	
	// morphs
	HASH_ITER(hh, morph_table, morph, next_morph) {
		if (MORPH_ABILITY(morph) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "MPH [%5d] %s\r\n", MORPH_VNUM(morph), MORPH_SHORT_DESC(morph));
		}
	}
	
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_HAVE_ABILITY, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "PRG [%5d] %s\r\n", PRG_VNUM(prg), PRG_NAME(prg));
		}
	}
	
	// quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(QUEST_TASKS(quest), REQ_HAVE_ABILITY, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_HAVE_ABILITY, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// skills
	HASH_ITER(hh, skill_table, skill, next_skill) {
		any = FALSE;
		
		LL_FOREACH(SKILL_ABILITIES(skill), skab) {
			if (skab->vnum == vnum) {
				any = TRUE;
				break;	// only need 1
			}
		}
		LL_FOREACH(SKILL_SYNERGIES(skill), syn) {
			if (syn->ability == vnum) {
				any = TRUE;
				break;	// only need 1
			}
		}
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SKL [%5d] %s\r\n", CLASS_VNUM(skill), CLASS_NAME(skill));
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_HAVE_ABILITY, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


// Simple vnum sorter for the ability hash
int sort_abilities(ability_data *a, ability_data *b) {
	return ABIL_VNUM(a) - ABIL_VNUM(b);
}


// typealphabetic sorter for sorted_abilities
int sort_abilities_by_data(ability_data *a, ability_data *b) {
	return strcmp(NULLSAFE(ABIL_NAME(a)), NULLSAFE(ABIL_NAME(b)));
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts an ability into the hash table.
*
* @param ability_data *abil The ability data to add to the table.
*/
void add_ability_to_table(ability_data *abil) {
	ability_data *find;
	any_vnum vnum;
	
	if (abil) {
		vnum = ABIL_VNUM(abil);
		HASH_FIND_INT(ability_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(ability_table, vnum, abil);
			HASH_SORT(ability_table, sort_abilities);
		}
		
		// sorted table
		HASH_FIND(sorted_hh, sorted_abilities, &vnum, sizeof(int), find);
		if (!find) {
			HASH_ADD(sorted_hh, sorted_abilities, vnum, sizeof(int), abil);
			HASH_SRT(sorted_hh, sorted_abilities, sort_abilities_by_data);
		}
	}
}


/**
* Removes an ability from the hash table.
*
* @param ability_data *abil The ability data to remove from the table.
*/
void remove_ability_from_table(ability_data *abil) {
	HASH_DEL(ability_table, abil);
	HASH_DELETE(sorted_hh, sorted_abilities, abil);
}


/**
* Initializes a new ability. This clears all memory for it, so set the vnum
* AFTER.
*
* @param ability_data *abil The ability to initialize.
*/
void clear_ability(ability_data *abil) {
	memset((char *) abil, 0, sizeof(ability_data));
	
	ABIL_VNUM(abil) = NOTHING;
	ABIL_MASTERY_ABIL(abil) = NOTHING;
	ABIL_COOLDOWN(abil) = NOTHING;
	ABIL_AFFECT_VNUM(abil) = NOTHING;
	ABIL_SCALE(abil) = 1.0;
	ABIL_MAX_STACKS(abil) = 1;
	ABIL_MIN_POS(abil) = POS_STANDING;
}


/**
* frees up memory for an ability data item.
*
* See also: olc_delete_ability
*
* @param ability_data *abil The ability data to free.
*/
void free_ability(ability_data *abil) {
	ability_data *proto = find_ability_by_vnum(ABIL_VNUM(abil));
	struct ability_data_list *adl;
	struct ability_type *atl;
	struct apply_data *app;
	
	if (ABIL_NAME(abil) && (!proto || ABIL_NAME(abil) != ABIL_NAME(proto))) {
		free(ABIL_NAME(abil));
	}
	if (ABIL_COMMAND(abil) && (!proto || ABIL_COMMAND(abil) != ABIL_COMMAND(proto))) {
		free(ABIL_COMMAND(abil));
	}
	if (ABIL_RESOURCE_COST(abil) && (!proto || ABIL_RESOURCE_COST(abil) != ABIL_RESOURCE_COST(proto))) {
		free_resource_list(ABIL_RESOURCE_COST(abil));
	}
	if (ABIL_CUSTOM_MSGS(abil) && (!proto || ABIL_CUSTOM_MSGS(abil) != ABIL_CUSTOM_MSGS(proto))) {
		free_custom_messages(ABIL_CUSTOM_MSGS(abil));
	}
	if (ABIL_APPLIES(abil) && (!proto || ABIL_APPLIES(abil) != ABIL_APPLIES(proto))) {
		while ((app = ABIL_APPLIES(abil))) {
			ABIL_APPLIES(abil) = app->next;
			free(app);
		}
	}
	if (ABIL_DATA(abil) && (!proto || ABIL_DATA(abil) != ABIL_DATA(proto))) {
		while ((adl = ABIL_DATA(abil))) {
			ABIL_DATA(abil) = adl->next;
			free(adl);
		}
	}
	if (ABIL_TYPE_LIST(abil) && (!proto || ABIL_TYPE_LIST(abil) != ABIL_TYPE_LIST(proto))) {
		while ((atl = ABIL_TYPE_LIST(abil))) {
			ABIL_TYPE_LIST(abil) = atl->next;
			free(atl);
		}
	}
	if (ABIL_INTERACTIONS(abil) && (!proto || ABIL_INTERACTIONS(abil) != ABIL_INTERACTIONS(proto))) {
		free_interactions(&ABIL_INTERACTIONS(abil));
	}
	if (ABIL_HOOKS(abil) && (!proto || ABIL_HOOKS(abil) != ABIL_HOOKS(proto))) {
		free_ability_hooks(ABIL_HOOKS(abil));
	}
	
	free(abil);
}


/**
* Frees the ability execution data when done.
*
* @param struct ability_exec *data The data to free.
*/
void free_ability_exec(struct ability_exec *data) {
	struct ability_exec_type *aet;
	
	if (data) {
		while ((aet = data->types)) {
			data->types = aet->next;
			free(aet);
		}
		free(data);
	}
}


/**
* Frees a list of ability hooks.
*
* @param struct ability_hook *list The list to free.
*/
void free_ability_hooks(struct ability_hook *list) {
	struct ability_hook *hook, *next;
	
	LL_FOREACH_SAFE(list, hook, next) {
		free(hook);
	}
}


/**
* Read one ability from file.
*
* @param FILE *fl The open .abil file
* @param any_vnum vnum The ability vnum
*/
void parse_ability(FILE *fl, any_vnum vnum) {
	char line[256], error[256], str_in[256], str_in2[256], str_in3[256], str_in4[256];
	struct ability_data_list *adl;
	struct ability_hook *ahook;
	ability_data *abil, *find;
	bitvector_t type;
	int int_in[10];
	double dbl_in;
	
	CREATE(abil, ability_data, 1);
	clear_ability(abil);
	ABIL_VNUM(abil) = vnum;
	
	HASH_FIND_INT(ability_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate ability vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_ability_to_table(abil);
		
	// for error messages
	sprintf(error, "ability vnum %d", vnum);
	
	// line 1: name
	ABIL_NAME(abil) = fread_string(fl, error);
	
	// line 2: flags master-abil scale immunities
	if (!get_line(fl, line)) {
		log("SYSERR: Missing line 2 of %s", error);
		exit(1);
	}
	
	// line 2 is backwards-compatible with previous versions
	if (sscanf(line, "%s %d %lf %s %s %s", str_in, &int_in[0], &dbl_in, str_in2, str_in3, str_in4) != 6) {
		strcpy(str_in4, "0");	// default requires-tool
		if (sscanf(line, "%s %d %lf %s %s", str_in, &int_in[0], &dbl_in, str_in2, str_in3) != 5) {
			strcpy(str_in3, "0");	// default gain hooks
			if (sscanf(line, "%s %d %lf %s", str_in, &int_in[0], &dbl_in, str_in2) != 4) {
				dbl_in = 1.0;	// default scale
				strcpy(str_in2, "0");	// default immunities
				if (sscanf(line, "%s %d", str_in, &int_in[0]) != 2) {
					log("SYSERR: Format error in line 2 of %s", error);
					exit(1);
				}
			}
		}
	}
	
	ABIL_FLAGS(abil) = asciiflag_conv(str_in);
	ABIL_MASTERY_ABIL(abil) = int_in[0];
	ABIL_SCALE(abil) = dbl_in;
	ABIL_IMMUNITIES(abil) = asciiflag_conv(str_in2);
	ABIL_GAIN_HOOKS(abil) = asciiflag_conv(str_in3);
	ABIL_REQUIRES_TOOL(abil) = asciiflag_conv(str_in4);
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'A': {	// applies
				parse_apply(fl, &ABIL_APPLIES(abil), error);
				break;
			}
			
			case 'C': {	// command info
				if (!get_line(fl, line)) {
					log("SYSERR: Missing C line of %s", error);
					exit(1);
				}
				
				// backwards-compatible with older versions
				if (sscanf(line, "%s %d %s %d %d %d %d %d %d %d %d", str_in, &int_in[0], str_in2, &int_in[1], &int_in[2], &int_in[3], &int_in[4], &int_in[5], &int_in[6], &int_in[7], &int_in[8]) != 11) {
					int_in[8] = DIFF_TRIVIAL;
					if (sscanf(line, "%s %d %s %d %d %d %d %d %d %d", str_in, &int_in[0], str_in2, &int_in[1], &int_in[2], &int_in[3], &int_in[4], &int_in[5], &int_in[6], &int_in[7]) != 10) {
						int_in[3] = 0;	// default cost-per-scale-point
						if (sscanf(line, "%s %d %s %d %d %d %d %d %d", str_in, &int_in[0], str_in2, &int_in[1], &int_in[2], &int_in[4], &int_in[5], &int_in[6], &int_in[7]) != 9) {
							log("SYSERR: Format error in C line of %s", error);
							exit(1);
						}
					}
				}
				
				if (ABIL_COMMAND(abil)) {
					free(ABIL_COMMAND(abil));
				}
				ABIL_COMMAND(abil) = (*str_in && str_cmp(str_in, "unknown")) ? str_dup(str_in) : NULL;
				ABIL_MIN_POS(abil) = int_in[0];
				ABIL_TARGETS(abil) = asciiflag_conv(str_in2);
				ABIL_COST_TYPE(abil) = int_in[1];
				ABIL_COST(abil) = int_in[2];
				ABIL_COST_PER_SCALE_POINT(abil) = int_in[3];
				ABIL_COOLDOWN(abil) = int_in[4];
				ABIL_COOLDOWN_SECS(abil) = int_in[5];
				ABIL_LINKED_TRAIT(abil) = int_in[6];
				ABIL_WAIT_TYPE(abil) = int_in[7];
				ABIL_DIFFICULTY(abil) = int_in[8];
				break;
			}
			
			case 'D': {	// data
				if (sscanf(line, "D %d %d %d", &int_in[0], &int_in[1], &int_in[2]) != 3) {
					log("SYSERR: Format error in D line of %s", error);
					exit(1);
				}
				
				CREATE(adl, struct ability_data_list, 1);
				adl->type = int_in[0];
				adl->vnum = int_in[1];
				adl->misc = int_in[2];
				
				LL_APPEND(ABIL_DATA(abil), adl);
				break;
			}
			case 'H': {	// ability hook
				if (sscanf(line, "H %llu %d %lf %d", &type, &int_in[1], &dbl_in, &int_in[3]) != 4) {
					log("SYSERR: Format error in H line of %s", error);
					exit(1);
				}
				
				CREATE(ahook, struct ability_hook, 1);
				ahook->type = type;
				ahook->value = int_in[1];
				ahook->percent = dbl_in;
				ahook->misc = int_in[3];
				
				LL_APPEND(ABIL_HOOKS(abil), ahook);
				ABIL_HOOK_FLAGS(abil) |= type;
				break;
			}
			case 'I': {	// interaction item
				parse_interaction(line, &ABIL_INTERACTIONS(abil), error);
				break;
			}
			
			case 'M': {	// custom messages
				parse_custom_message(fl, &ABIL_CUSTOM_MSGS(abil), error);
				break;
			}
			
			case 'R': {	// resources
				parse_resource(fl, &ABIL_RESOURCE_COST(abil), error);
				break;
			}
			
			case 'T': {	// type
				if (sscanf(line, "T %s %d", str_in, &int_in[0]) != 2) {
					log("SYSERR: Format error in T line of %s", error);
					exit(1);
				}
				add_type_to_ability(abil, asciiflag_conv(str_in), int_in[0]);
				break;
			}
			
			case 'X': {	// extended data (type-based)
				type = asciiflag_conv(line+2);
				// ABILT_x: parsing ability data
				switch (type) {
					case ABILT_BUFF: {
						if (!get_line(fl, line) || sscanf(line, "%d %d %d %s", &int_in[0], &int_in[1], &int_in[2], str_in) != 4) {
							log("SYSERR: Format error in 'X %s' line of %s", line+2, error);
							exit(1);
						}
						
						ABIL_AFFECT_VNUM(abil) = int_in[0];
						ABIL_SHORT_DURATION(abil) = int_in[1];
						ABIL_LONG_DURATION(abil) = int_in[2];
						ABIL_AFFECTS(abil) = asciiflag_conv(str_in);
						break;
					}
					case ABILT_DAMAGE: {
						if (!get_line(fl, line) || sscanf(line, "%d %d", &int_in[0], &int_in[1]) != 2) {
							log("SYSERR: Format error in 'X %s' line of %s", line+2, error);
							exit(1);
						}
						
						ABIL_ATTACK_TYPE(abil) = int_in[0];
						ABIL_DAMAGE_TYPE(abil) = int_in[1];
						break;
					}
					case ABILT_DOT: {
						if (!get_line(fl, line) || sscanf(line, "%d %d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3], &int_in[4]) != 5) {
							log("SYSERR: Format error in 'X %s' line of %s", line+2, error);
							exit(1);
						}
						
						ABIL_AFFECT_VNUM(abil) = int_in[0];
						ABIL_SHORT_DURATION(abil) = int_in[1];
						ABIL_LONG_DURATION(abil) = int_in[2];
						ABIL_DAMAGE_TYPE(abil) = int_in[3];
						ABIL_MAX_STACKS(abil) = int_in[4];
						break;
					}
					case ABILT_PASSIVE_BUFF: {
						if (!get_line(fl, line) || sscanf(line, "%s", str_in) != 1) {
							log("SYSERR: Format error in 'X %s' line of %s", line+2, error);
							exit(1);
						}
						
						ABIL_AFFECTS(abil) = asciiflag_conv(str_in);
						break;
					}
					case ABILT_ROOM_AFFECT: {
						if (!get_line(fl, line) || sscanf(line, "%d %d %d %s", &int_in[0], &int_in[1], &int_in[2], str_in) != 4) {
							log("SYSERR: Format error in 'X %s' line of %s", line+2, error);
							exit(1);
						}
						
						ABIL_AFFECT_VNUM(abil) = int_in[0];
						ABIL_SHORT_DURATION(abil) = int_in[1];
						ABIL_LONG_DURATION(abil) = int_in[2];
						ABIL_AFFECTS(abil) = asciiflag_conv(str_in);
						break;
					}
					default: {
						log("SYSERR: Unknown flag X%llu in %s", type, error);
						exit(1);
					}
				}
				break;
			}
			
			// end
			case 'S': {
				return;
			}
			
			default: {
				log("SYSERR: Format error in %s, expecting alphabetic flags", error);
				exit(1);
			}
		}
	}
}


/**
* Caches ability requirements. This should be called at startup and whenever
* a skill is saved.
*/
void read_ability_requirements(void) {
	struct skill_ability *iter;
	ability_data *abil, *next_abil;
	class_data *class, *next_class;
	skill_data *skill, *next_skill;
	struct synergy_ability *syn;
	struct class_ability *clab;
	
	// clear existing requirements
	HASH_ITER(hh, ability_table, abil, next_abil) {
		ABIL_ASSIGNED_SKILL(abil) = NULL;
		ABIL_SKILL_LEVEL(abil) = 0;
		ABIL_IS_CLASS(abil) = FALSE;
		ABIL_IS_SYNERGY(abil) = FALSE;
	}
	
	HASH_ITER(hh, skill_table, skill, next_skill) {
		LL_FOREACH(SKILL_ABILITIES(skill), iter) {
			// TODO should this be moved up? or be checking ability in-dev?
			if (IS_SET(SKILL_FLAGS(skill), SKILLF_IN_DEVELOPMENT)) {
				continue;	// don't count if in-dev
			}
			if (!(abil = find_ability_by_vnum(iter->vnum))) {
				continue;
			}
			
			// read assigned skill data
			ABIL_ASSIGNED_SKILL(abil) = skill;
			ABIL_SKILL_LEVEL(abil) = iter->level;
		}
		
		LL_FOREACH(SKILL_SYNERGIES(skill), syn) {
			if (!(abil = find_ability_by_vnum(syn->ability))) {
				continue;
			}
			ABIL_IS_SYNERGY(abil) = TRUE;
		}
	}
	
	HASH_ITER(hh, class_table, class, next_class) {
		LL_FOREACH(CLASS_ABILITIES(class), clab) {
			if (!(abil = find_ability_by_vnum(clab->vnum))) {
				continue;
			}
			ABIL_IS_CLASS(abil) = TRUE;
		}
	}
	
	// audit
	HASH_ITER(hh, ability_table, abil, next_abil) {
		if (ABIL_IS_PURCHASE(abil) && (ABIL_IS_CLASS(abil) || ABIL_IS_SYNERGY(abil))) {
			syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: ability %d %s is set purchasable (skill tree) and class/synergy", ABIL_VNUM(abil), ABIL_NAME(abil));
		}
	}
}


// writes entries in the ability index
void write_ability_index(FILE *fl) {
	ability_data *abil, *next_abil;
	int this, last;
	
	last = NO_WEAR;
	HASH_ITER(hh, ability_table, abil, next_abil) {
		// determine "zone number" by vnum
		this = (int)(ABIL_VNUM(abil) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, ABIL_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one ability in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param ability_data *abil The thing to save.
*/
void write_ability_to_file(FILE *fl, ability_data *abil) {
	char temp[256], temp2[256], temp3[256], temp4[256];
	struct ability_data_list *adl;
	struct ability_hook *ahook;
	struct ability_type *at;
	
	if (!fl || !abil) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_ability_to_file called without %s", !fl ? "file" : "ability");
		return;
	}
	
	fprintf(fl, "#%d\n", ABIL_VNUM(abil));
	
	// 1: name
	fprintf(fl, "%s~\n", NULLSAFE(ABIL_NAME(abil)));
	
	// 2: flags mastery-abil scale immunities gain-hooks
	strcpy(temp, bitv_to_alpha(ABIL_FLAGS(abil)));
	strcpy(temp2, bitv_to_alpha(ABIL_IMMUNITIES(abil)));
	strcpy(temp3, bitv_to_alpha(ABIL_GAIN_HOOKS(abil)));
	strcpy(temp4, bitv_to_alpha(ABIL_REQUIRES_TOOL(abil)));
	fprintf(fl, "%s %d %.2f %s %s %s\n", temp, ABIL_MASTERY_ABIL(abil), ABIL_SCALE(abil), temp2, temp3, temp4);
	
	// 'A': applies
	write_applies_to_file(fl, ABIL_APPLIES(abil));
	
	// 'C' command
	if (ABIL_COMMAND(abil) || ABIL_MIN_POS(abil) != POS_STANDING || ABIL_TARGETS(abil) || ABIL_COST(abil) || ABIL_COST_PER_SCALE_POINT(abil) || ABIL_COOLDOWN(abil) != NOTHING || ABIL_COOLDOWN_SECS(abil) || ABIL_WAIT_TYPE(abil) || ABIL_LINKED_TRAIT(abil) || ABIL_DIFFICULTY(abil)) {
		fprintf(fl, "C\n%s %d %s %d %d %d %d %d %d %d %d\n", ABIL_COMMAND(abil) ? ABIL_COMMAND(abil) : "unknown", ABIL_MIN_POS(abil), bitv_to_alpha(ABIL_TARGETS(abil)), ABIL_COST_TYPE(abil), ABIL_COST(abil), ABIL_COST_PER_SCALE_POINT(abil), ABIL_COOLDOWN(abil), ABIL_COOLDOWN_SECS(abil), ABIL_LINKED_TRAIT(abil), ABIL_WAIT_TYPE(abil), ABIL_DIFFICULTY(abil));
	}
	
	// 'D' data
	LL_FOREACH(ABIL_DATA(abil), adl) {
		fprintf(fl, "D %d %d %d\n", adl->type, adl->vnum, adl->misc);
	}
	
	// 'H' hooks
	LL_FOREACH(ABIL_HOOKS(abil), ahook) {
		fprintf(fl, "H %llu %d %.2f %d\n", ahook->type, ahook->value, ahook->percent, ahook->misc);
	}
	
	// 'I' interactions
	write_interactions_to_file(fl, ABIL_INTERACTIONS(abil));
	
	// 'M' custom message
	write_custom_messages_to_file(fl, 'M', ABIL_CUSTOM_MSGS(abil));
	
	// 'R': resources
	write_resources_to_file(fl, 'R', ABIL_RESOURCE_COST(abil));
	
	// 'T' types
	LL_FOREACH(ABIL_TYPE_LIST(abil), at) {
		fprintf(fl, "T %s %d\n", bitv_to_alpha(at->type), at->weight);
	}
	
	// 'X' ABILT_x: type-based data
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF)) {
		strcpy(temp, bitv_to_alpha(ABILT_BUFF));
		strcpy(temp2, bitv_to_alpha(ABIL_AFFECTS(abil)));
		fprintf(fl, "X %s\n%d %d %d %s\n", temp, ABIL_AFFECT_VNUM(abil), ABIL_SHORT_DURATION(abil), ABIL_LONG_DURATION(abil), temp2);
	}
	if (IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE)) {
		fprintf(fl, "X %s\n%d %d\n", bitv_to_alpha(ABILT_DAMAGE), ABIL_ATTACK_TYPE(abil), ABIL_DAMAGE_TYPE(abil));
	}
	if (IS_SET(ABIL_TYPES(abil), ABILT_DOT)) {
		fprintf(fl, "X %s\n%d %d %d %d %d\n", bitv_to_alpha(ABILT_DOT), ABIL_AFFECT_VNUM(abil), ABIL_SHORT_DURATION(abil), ABIL_LONG_DURATION(abil), ABIL_DAMAGE_TYPE(abil), ABIL_MAX_STACKS(abil));
	}
	if (IS_SET(ABIL_TYPES(abil), ABILT_PASSIVE_BUFF)) {
		strcpy(temp, bitv_to_alpha(ABILT_PASSIVE_BUFF));
		strcpy(temp2, bitv_to_alpha(ABIL_AFFECTS(abil)));
		fprintf(fl, "X %s\n%s\n", temp, temp2);
	}
	if (IS_SET(ABIL_TYPES(abil), ABILT_ROOM_AFFECT)) {
		strcpy(temp, bitv_to_alpha(ABILT_ROOM_AFFECT));
		strcpy(temp2, bitv_to_alpha(ABIL_AFFECTS(abil)));
		fprintf(fl, "X %s\n%d %d %d %s\n", temp, ABIL_AFFECT_VNUM(abil), ABIL_SHORT_DURATION(abil), ABIL_LONG_DURATION(abil), temp2);
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////

/**
* Creates a new ability entry.
* 
* @param any_vnum vnum The number to create.
* @return ability_data* The new ability's prototype.
*/
ability_data *create_ability_table_entry(any_vnum vnum) {
	ability_data *abil;
	
	// sanity
	if (find_ability_by_vnum(vnum)) {
		log("SYSERR: Attempting to insert ability at existing vnum %d", vnum);
		return find_ability_by_vnum(vnum);
	}
	
	CREATE(abil, ability_data, 1);
	clear_ability(abil);
	ABIL_VNUM(abil) = vnum;
	ABIL_NAME(abil) = str_dup(default_ability_name);
	add_ability_to_table(abil);

	// save index and ability file now
	save_index(DB_BOOT_ABIL);
	save_library_file_for_vnum(DB_BOOT_ABIL, vnum);

	return abil;
}


/**
* WARNING: This function actually deletes an ability.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_ability(char_data *ch, any_vnum vnum) {
	struct player_ability_data *plab, *next_plab;
	ability_data *abil, *abiter, *next_abiter;
	struct global_data *glb, *next_glb;
	craft_data *craft, *next_craft;
	morph_data *morph, *next_morph;
	quest_data *quest, *next_quest;
	skill_data *skill, *next_skill;
	progress_data *prg, *next_prg;
	augment_data *aug, *next_aug;
	social_data *soc, *next_soc;
	class_data *cls, *next_cls;
	descriptor_data *desc;
	char_data *chiter;
	char name[256];
	bool found;
	
	if (!(abil = find_ability_by_vnum(vnum))) {
		msg_to_char(ch, "There is no such ability %d.\r\n", vnum);
		return;
	}
	
	snprintf(name, sizeof(name), "%s", NULLSAFE(ABIL_NAME(abil)));
		
	// remove it from the hash table first
	remove_ability_from_table(abil);
	
	// update other abilities
	HASH_ITER(hh, ability_table, abiter, next_abiter) {
		found = FALSE;
		if (ABIL_MASTERY_ABIL(abiter) == vnum) {
			ABIL_MASTERY_ABIL(abiter) = NOTHING;
			found = TRUE;
		}
		found |= delete_from_ability_hooks(abil, AHOOK_ABILITY, vnum);
		found |= delete_from_ability_data_list(abil, ADL_PARENT, vnum);
		
		if (found) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Ability %d %s lost related deleted ability", ABIL_VNUM(abiter), ABIL_NAME(abiter));
			save_library_file_for_vnum(DB_BOOT_ABIL, ABIL_VNUM(abiter));
		}
	}
	
	// update augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		if (GET_AUG_ABILITY(aug) == vnum) {
			GET_AUG_ABILITY(aug) = NOTHING;
			SET_BIT(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Augment %d %s set IN-DEV due to deleted ability", GET_AUG_VNUM(aug), GET_AUG_NAME(aug));
			save_library_file_for_vnum(DB_BOOT_AUG, GET_AUG_VNUM(aug));
		}
	}
	
	// update classes
	HASH_ITER(hh, class_table, cls, next_cls) {
		found = remove_vnum_from_class_abilities(&CLASS_ABILITIES(cls), vnum);
		if (found) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Class %d %s lost deleted ability", CLASS_VNUM(cls), CLASS_NAME(cls));
			save_library_file_for_vnum(DB_BOOT_CLASS, CLASS_VNUM(cls));
		}
	}
	
	// update crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		if (GET_CRAFT_ABILITY(craft) == vnum) {
			GET_CRAFT_ABILITY(craft) = NOTHING;
			SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Craft %d %s set IN-DEV due to deleted ability", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// update globals
	HASH_ITER(hh, globals_table, glb, next_glb) {
		if (GET_GLOBAL_ABILITY(glb) == vnum) {
			GET_GLOBAL_ABILITY(glb) = NOTHING;
			SET_BIT(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Global %d %s set IN-DEV due to deleted ability", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb));
			save_library_file_for_vnum(DB_BOOT_GLB, GET_GLOBAL_VNUM(glb));
		}
	}
	
	// update morphs
	HASH_ITER(hh, morph_table, morph, next_morph) {
		if (MORPH_ABILITY(morph) == vnum) {
			MORPH_ABILITY(morph) = NOTHING;
			SET_BIT(MORPH_FLAGS(morph), MORPHF_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Morph %d %s set IN-DEV due to deleted ability", MORPH_VNUM(morph), MORPH_SHORT_DESC(morph));
			save_library_file_for_vnum(DB_BOOT_MORPH, MORPH_VNUM(morph));
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_HAVE_ABILITY, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Progress %d %s set IN-DEV due to deleted ability", PRG_VNUM(prg), PRG_NAME(prg));
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// update quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		found = delete_requirement_from_list(&QUEST_TASKS(quest), REQ_HAVE_ABILITY, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_HAVE_ABILITY, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Quest %d %s set IN-DEV due to deleted ability", QUEST_VNUM(quest), QUEST_NAME(quest));
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// update skills
	HASH_ITER(hh, skill_table, skill, next_skill) {
		found = remove_vnum_from_skill_abilities(&SKILL_ABILITIES(skill), vnum);
		found |= remove_ability_from_synergy_abilities(&SKILL_SYNERGIES(skill), vnum);
		if (found) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Skill %d %s lost deleted ability", SKILL_VNUM(skill), SKILL_NAME(skill));
			save_library_file_for_vnum(DB_BOOT_SKILL, SKILL_VNUM(skill));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_HAVE_ABILITY, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Social %d %s set IN-DEV due to deleted ability", SOC_VNUM(soc), SOC_NAME(soc));
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// update live players
	DL_FOREACH2(player_character_list, chiter, next_plr) {
		found = FALSE;
		
		HASH_ITER(hh, GET_ABILITY_HASH(chiter), plab, next_plab) {
			if (plab->vnum == vnum) {
				HASH_DEL(GET_ABILITY_HASH(chiter), plab);
				free(plab);
				found = TRUE;
			}
		}
		
		remove_player_tech(chiter, vnum);
	}
	
	// update olc editors
	LL_FOREACH(descriptor_list, desc) {
		if (GET_OLC_ABILITY(desc)) {
			found = FALSE;
			if (ABIL_MASTERY_ABIL(GET_OLC_ABILITY(desc)) == vnum) {
				ABIL_MASTERY_ABIL(GET_OLC_ABILITY(desc)) = NOTHING;
				found = TRUE;
			}
			found |= delete_from_ability_hooks(GET_OLC_ABILITY(desc), AHOOK_ABILITY, vnum);
			found |= delete_from_ability_data_list(GET_OLC_ABILITY(desc), ADL_PARENT, vnum);
			
			if (found) {
				msg_to_desc(desc, "An ability linked to the one you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_AUGMENT(desc)) {
			if (GET_AUG_ABILITY(GET_OLC_AUGMENT(desc)) == vnum) {
				GET_AUG_ABILITY(GET_OLC_AUGMENT(desc)) = NOTHING;
				msg_to_desc(desc, "The required ability has been deleted from the augment you're editing.\r\n");
			}
		}
		if (GET_OLC_CLASS(desc)) {
			found = remove_vnum_from_class_abilities(&CLASS_ABILITIES(GET_OLC_CLASS(desc)), vnum);
			if (found) {
				msg_to_desc(desc, "An ability has been deleted from the class you're editing.\r\n");
			}
		}
		if (GET_OLC_CRAFT(desc)) {
			if (GET_CRAFT_ABILITY(GET_OLC_CRAFT(desc)) == vnum) {
				GET_CRAFT_ABILITY(GET_OLC_CRAFT(desc)) = NOTHING;
				msg_to_desc(desc, "The required ability has been deleted from the craft you're editing.\r\n");
			}
		}
		if (GET_OLC_GLOBAL(desc)) {
			if (GET_GLOBAL_ABILITY(GET_OLC_GLOBAL(desc)) == vnum) {
				GET_GLOBAL_ABILITY(GET_OLC_GLOBAL(desc)) = NOTHING;
				msg_to_desc(desc, "The required ability has been deleted from the global you're editing.\r\n");
			}
		}
		if (GET_OLC_MORPH(desc)) {
			if (MORPH_ABILITY(GET_OLC_MORPH(desc)) == vnum) {
				MORPH_ABILITY(GET_OLC_MORPH(desc)) = NOTHING;
				msg_to_desc(desc, "The required ability has been deleted from the morph you're editing.\r\n");
			}
		}
		if (GET_OLC_PROGRESS(desc)) {
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_HAVE_ABILITY, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "An ability used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			found = delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_HAVE_ABILITY, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_HAVE_ABILITY, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "An ability has been deleted from the quest you're editing.\r\n");
			}
		}
		if (GET_OLC_SKILL(desc)) {
			found = remove_vnum_from_skill_abilities(&SKILL_ABILITIES(GET_OLC_SKILL(desc)), vnum);
			found |= remove_ability_from_synergy_abilities(&SKILL_SYNERGIES(GET_OLC_SKILL(desc)), vnum);
			if (found) {
				msg_to_desc(desc, "An ability has been deleted from the skill you're editing.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_HAVE_ABILITY, vnum);
		
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A required ability has been deleted from the social you're editing.\r\n");
			}
		}
	}
	
	save_index(DB_BOOT_ABIL);
	save_library_file_for_vnum(DB_BOOT_ABIL, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted ability %d %s", GET_NAME(ch), vnum, name);
	msg_to_char(ch, "Ability %d (%s) deleted.\r\n", vnum, name);
	
	free_ability(abil);
}


/**
* Searches properties of abilities.
*
* @param char_data *ch The person searching.
* @param char *argument The argument they entered.
*/
void olc_fullsearch_abil(char_data *ch, char *argument) {
	#define FAKE_DUR  -999	// arbitrary control number
	
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	bitvector_t find_applies = NOBITS, found_applies, not_flagged = NOBITS, only_flags = NOBITS;
	bitvector_t only_affs = NOBITS, only_immunities = NOBITS, only_gains = NOBITS, only_targets = NOBITS, find_custom = NOBITS, found_custom, only_tools = NOBITS, only_room_affs = NOBITS;
	bitvector_t find_interacts = NOBITS, found_interacts;
	int count, only_cost_type = NOTHING, only_type = NOTHING, only_scale = NOTHING, scale_over = NOTHING, scale_under = NOTHING, min_pos = POS_DEAD, max_pos = POS_STANDING;
	int min_cost = NOTHING, max_cost = NOTHING, min_cost_per = NOTHING, max_cost_per = NOTHING, min_cd = NOTHING, max_cd = NOTHING, min_dur = FAKE_DUR, max_dur = FAKE_DUR;
	int only_wait = NOTHING, only_linked = NOTHING, only_diff = NOTHING, only_damage = NOTHING, only_ptech = NOTHING;
	int only_action = NOTHING, only_effect = NOTHING, only_limit = NOTHING, only_paint = NOTHING;
	int vmin = NOTHING, vmax = NOTHING;
	ability_data *abil, *next_abil;
	struct custom_message *cust;
	struct apply_data *app;
	struct interaction_item *inter;
	attack_message_data *only_attack = NULL;
	size_t size;
	
	if (!*argument) {
		msg_to_char(ch, "See HELP ABILEDIT FULLSEARCH for syntax.\r\n");
		return;
	}
	
	// process argument
	*find_keywords = '\0';
	while (*argument) {
		// figure out a type
		argument = any_one_arg(argument, type_arg);
		
		if (!strcmp(type_arg, "-")) {
			continue;	// just skip stray dashes
		}
		
		FULLSEARCH_LIST("action", only_action, ability_actions)
		FULLSEARCH_FLAGS("affects", only_affs, affected_bits)
		FULLSEARCH_FLAGS("apply", find_applies, apply_types)
		FULLSEARCH_FLAGS("applies", find_applies, apply_types)
		FULLSEARCH_FUNC("attacktype", only_attack, find_attack_message_by_name_or_vnum(val_arg, FALSE))
		FULLSEARCH_LIST("costtype", only_cost_type, pool_types)
		FULLSEARCH_FLAGS("custom", find_custom, ability_custom_types)
		FULLSEARCH_LIST("damagetype", only_damage, damage_types)
		FULLSEARCH_LIST("difficulty", only_diff, skill_check_difficulty)
		FULLSEARCH_LIST("effect", only_effect, ability_effects)
		FULLSEARCH_FLAGS("flags", only_flags, ability_flags)
		FULLSEARCH_FLAGS("flagged", only_flags, ability_flags)
		FULLSEARCH_FLAGS("gains", only_gains, ability_gain_hooks)
		FULLSEARCH_FLAGS("gainhooks", only_gains, ability_gain_hooks)
		FULLSEARCH_FLAGS("immunities", only_immunities, affected_bits)
		FULLSEARCH_FLAGS("immunity", only_immunities, affected_bits)
		FULLSEARCH_FLAGS("immune", only_immunities, affected_bits)
		FULLSEARCH_FLAGS("interaction", find_interacts, interact_types)
		FULLSEARCH_LIST("limitation", only_limit, ability_limitations)
		FULLSEARCH_LIST("linkedtrait", only_linked, apply_types)
		FULLSEARCH_INT("maxcooldowntime", max_cd, 0, INT_MAX)
		FULLSEARCH_INT("mincooldowntime", min_cd, 0, INT_MAX)
		FULLSEARCH_INT("maxcost", max_cost, 0, INT_MAX)
		FULLSEARCH_INT("mincost", min_cost, 0, INT_MAX)
		FULLSEARCH_INT("maxcostperscalepoint", max_cost_per, 0, INT_MAX)
		FULLSEARCH_INT("mincostperscalepoint", min_cost_per, 0, INT_MAX)
		FULLSEARCH_LIST("maxposition", max_pos, position_types)
		FULLSEARCH_LIST("minposition", min_pos, position_types)
		FULLSEARCH_LIST("paintcolor", only_paint, paint_names)
		FULLSEARCH_LIST("ptech", only_ptech, player_tech_types)
		FULLSEARCH_FLAGS("roomaffects", only_room_affs, room_aff_bits)
		FULLSEARCH_INT("scale", only_scale, 0, INT_MAX)
		FULLSEARCH_INT("scaleover", scale_over, 0, INT_MAX)
		FULLSEARCH_INT("scaleunder", scale_under, 0, INT_MAX)
		FULLSEARCH_FLAGS("targets", only_targets, ability_target_flags)
		FULLSEARCH_FLAGS("tools", only_tools, tool_flags)
		FULLSEARCH_LIST("type", only_type, ability_type_flags)
		FULLSEARCH_FLAGS("unflagged", not_flagged, ability_flags)
		FULLSEARCH_LIST("waittype", only_wait, wait_types)
		FULLSEARCH_INT("vmin", vmin, 0, INT_MAX)
		FULLSEARCH_INT("vmax", vmax, 0, INT_MAX)
		
		// custom:
		else if (is_abbrev(type_arg, "-maxduration")) {
			argument = any_one_word(argument, val_arg);
			if (is_abbrev(val_arg, "unlimited")) {
				max_dur = UNLIMITED;
			}
			else if (!isdigit(*val_arg) || (max_dur = atoi(val_arg)) < 0) {
				msg_to_char(ch, "Invalid duration '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-minduration")) {
			argument = any_one_word(argument, val_arg);
			if (is_abbrev(val_arg, "unlimited")) {
				min_dur = UNLIMITED;
			}
			else if (!isdigit(*val_arg) || (min_dur = atoi(val_arg)) < 0) {
				msg_to_char(ch, "Invalid duration '%s'.\r\n", val_arg);
				return;
			}
		}
		
		else {	// not sure what to do with it? treat it like a keyword
			sprintf(find_keywords + strlen(find_keywords), "%s%s", *find_keywords ? " " : "", type_arg);
		}
		
		// prepare for next loop
		skip_spaces(&argument);
	}
	
	size = snprintf(buf, sizeof(buf), "Ability fullsearch: %s\r\n", show_color_codes(find_keywords));
	count = 0;
	
	// okay now look up items
	HASH_ITER(hh, ability_table, abil, next_abil) {
		if ((vmin != NOTHING && ABIL_VNUM(abil) < vmin) || (vmax != NOTHING && ABIL_VNUM(abil) > vmax)) {
			continue;	// vnum range
		}
		if (ABIL_MIN_POS(abil) < min_pos || ABIL_MIN_POS(abil) > max_pos) {
			continue;
		}
		if (only_cost_type != NOTHING && ABIL_COST_TYPE(abil) != only_cost_type) {
			continue;
		}
		if (only_attack && ABIL_ATTACK_TYPE(abil) != ATTACK_VNUM(only_attack)) {
			continue;
		}
		if (only_damage != NOTHING && ABIL_DAMAGE_TYPE(abil) != only_damage) {
			continue;
		}
		if (only_diff != NOTHING && ABIL_DIFFICULTY(abil) != only_diff) {
			continue;
		}
		if (only_linked != NOTHING && ABIL_LINKED_TRAIT(abil) != only_linked) {
			continue;
		}
		if (only_wait != NOTHING && ABIL_WAIT_TYPE(abil) != only_wait) {
			continue;
		}
		if (max_cd != NOTHING && ABIL_COOLDOWN_SECS(abil) > max_cd) {
			continue;
		}
		if (min_cd != NOTHING && ABIL_COOLDOWN_SECS(abil) < min_cd) {
			continue;
		}
		if (max_cost != NOTHING && ABIL_COST(abil) > max_cost) {
			continue;
		}
		if (min_cost != NOTHING && ABIL_COST(abil) < min_cost) {
			continue;
		}
		if (max_cost_per != NOTHING && ABIL_COST_PER_SCALE_POINT(abil) > max_cost_per) {
			continue;
		}
		if (min_cost_per != NOTHING && ABIL_COST_PER_SCALE_POINT(abil) < min_cost_per) {
			continue;
		}
		if (max_dur != FAKE_DUR && ABIL_SHORT_DURATION(abil) > max_dur && ABIL_LONG_DURATION(abil) > max_dur) {
			continue;
		}
		if (min_dur != FAKE_DUR && ABIL_SHORT_DURATION(abil) < min_dur && ABIL_LONG_DURATION(abil) < min_dur) {
			continue;
		}
		if (only_scale != NOTHING && (int)(ABIL_SCALE(abil) * 100) != only_scale) {
			continue;
		}
		if (scale_over != NOTHING && ABIL_SCALE(abil) * 100 < scale_over) {
			continue;
		}
		if (scale_under != NOTHING && ABIL_SCALE(abil) * 100 > scale_under) {
			continue;
		}
		if (only_type != NOTHING && !IS_SET(ABIL_TYPES(abil), BIT(only_type))) {
			continue;
		}
		if (not_flagged != NOBITS && ABILITY_FLAGGED(abil, not_flagged)) {
			continue;
		}
		if (only_affs != NOBITS && ((ABIL_AFFECTS(abil) & only_affs) != only_affs || IS_SET(ABIL_TYPES(abil), ABILT_ROOM_AFFECT))) {
			continue;
		}
		if (only_room_affs != NOBITS && (!IS_SET(ABIL_TYPES(abil), ABILT_ROOM_AFFECT) || (ABIL_AFFECTS(abil) & only_room_affs) != only_room_affs)) {
			continue;
		}
		if (only_flags != NOBITS && (ABIL_FLAGS(abil) & only_flags) != only_flags) {
			continue;
		}
		if (only_gains != NOBITS && (ABIL_GAIN_HOOKS(abil) & only_gains) != only_gains) {
			continue;
		}
		if (only_immunities != NOBITS && (ABIL_IMMUNITIES(abil) & only_immunities) != only_immunities) {
			continue;
		}
		if (only_targets != NOBITS && (ABIL_TARGETS(abil) & only_targets) != only_targets) {
			continue;
		}
		if (only_tools != NOBITS && (ABIL_REQUIRES_TOOL(abil) & only_tools) != only_tools) {
			continue;
		}
		if (find_applies) {	// look up its applies
			found_applies = NOBITS;
			LL_FOREACH(ABIL_APPLIES(abil), app) {
				found_applies |= BIT(app->location);
			}
			if ((find_applies & found_applies) != find_applies) {
				continue;
			}
		}
		if (find_custom) {	// look up its custom messages
			found_custom = NOBITS;
			LL_FOREACH(ABIL_CUSTOM_MSGS(abil), cust) {
				found_custom |= BIT(cust->type);
			}
			if ((find_custom & found_custom) != find_custom) {
				continue;
			}
		}
		if (only_ptech != NOTHING && !find_ability_data_entry_for(abil, ADL_PLAYER_TECH, only_ptech)) {
			continue;
		}
		if (only_action != NOTHING && !find_ability_data_entry_for(abil, ADL_ACTION, only_action)) {
			continue;
		}
		if (only_effect != NOTHING && !find_ability_data_entry_for(abil, ADL_EFFECT, only_effect)) {
			continue;
		}
		if (only_limit != NOTHING && !find_ability_data_entry_for(abil, ADL_LIMITATION, only_limit)) {
			continue;
		}
		if (only_paint != NOTHING && !find_ability_data_entry_for(abil, ADL_PAINT_COLOR, only_paint)) {
			continue;
		}
		if (find_interacts) {	// look up its interactions
			found_interacts = NOBITS;
			LL_FOREACH(ABIL_INTERACTIONS(abil), inter) {
				found_interacts |= BIT(inter->type);
			}
			if ((find_interacts & found_interacts) != find_interacts) {
				continue;
			}
		}
		if (*find_keywords && !multi_isname(find_keywords, ABIL_NAME(abil)) && !multi_isname(find_keywords, NULLSAFE(ABIL_COMMAND(abil))) && !search_custom_messages(find_keywords, ABIL_CUSTOM_MSGS(abil))) {
			continue;
		}
		
		// show it
		snprintf(line, sizeof(line), "[%5d] %s\r\n", ABIL_VNUM(abil), ABIL_NAME(abil));
		if (strlen(line) + size < sizeof(buf)) {
			size += snprintf(buf + size, sizeof(buf) - size, "%s", line);
			++count;
		}
		else {
			size += snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
			break;
		}
	}
	
	if (count > 0 && (size + 20) < sizeof(buf)) {
		size += snprintf(buf + size, sizeof(buf) - size, "(%d abilities)\r\n", count);
	}
	else if (count == 0) {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


/**
* Function to save a player's changes to an ability (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_ability(descriptor_data *desc) {	
	ability_data *proto, *abil = GET_OLC_ABILITY(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	struct player_ability_data *abd;
	struct ability_data_list *adl;
	UT_hash_handle hh, sorted;
	char_data *chiter;
	int iter;
	bool any;

	// have a place to save it?
	if (!(proto = find_ability_by_vnum(vnum))) {
		proto = create_ability_table_entry(vnum);
	}
	
	// update live players' gain hooks and techs
	DL_FOREACH2(player_character_list, chiter, next_plr) {
		if ((abd = get_ability_data(chiter, vnum, FALSE))) {
			any = FALSE;
			for (iter = 0; iter < NUM_SKILL_SETS && !any; ++iter) {
				if (abd->purchased[iter]) {
					any = TRUE;
					add_ability_gain_hook(chiter, abd->ptr);
				}
			}
			
			if (abd->purchased[GET_CURRENT_SKILL_SET(chiter)]) {
				remove_player_tech(chiter, ABIL_VNUM(abil));
				apply_ability_techs_to_player(chiter, abil);
				remove_passive_buff_by_ability(chiter, vnum);
				add_ability_gain_hook(chiter, abil);
			}
		}
	}
	
	// free prototype strings and pointers
	if (ABIL_NAME(proto)) {
		free(ABIL_NAME(proto));
	}
	if (ABIL_COMMAND(proto)) {
		free(ABIL_COMMAND(proto));
	}
	while ((adl = ABIL_DATA(proto))) {
		ABIL_DATA(proto) = adl->next;
		free(adl);
	}
	free_resource_list(ABIL_RESOURCE_COST(proto));
	free_custom_messages(ABIL_CUSTOM_MSGS(proto));
	free_apply_list(ABIL_APPLIES(proto));
	free_ability_hooks(ABIL_HOOKS(proto));
	
	// sanity
	if (!ABIL_NAME(abil) || !*ABIL_NAME(abil)) {
		if (ABIL_NAME(abil)) {
			free(ABIL_NAME(abil));
		}
		ABIL_NAME(abil) = str_dup(default_ability_name);
	}
	if (ABIL_COMMAND(abil) && !*ABIL_COMMAND(abil)) {
		free(ABIL_COMMAND(abil));	// don't allow empty
		ABIL_COMMAND(abil) = NULL;
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	sorted = proto->sorted_hh;
	*proto = *abil;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	proto->sorted_hh = sorted;
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_ABIL, vnum);

	// ... and update some things
	HASH_SRT(sorted_hh, sorted_abilities, sort_abilities_by_data);
	read_ability_requirements();	// may have lost/changed its skill assignment
	
	// apply passive buffs
	if (IS_SET(ABIL_TYPES(proto), ABILT_PASSIVE_BUFF)) {
		DL_FOREACH2(player_character_list, chiter, next_plr) {
			if ((abd = get_ability_data(chiter, vnum, FALSE))) {
				if (abd->purchased[GET_CURRENT_SKILL_SET(chiter)]) {
					apply_one_passive_buff(chiter, proto);
				}
			}
		}
	}
}


/**
* Creates a copy of an ability, or clears a new one, for editing.
* 
* @param ability_data *input The ability to copy, or NULL to make a new one.
* @return ability_data* The copied ability.
*/
ability_data *setup_olc_ability(ability_data *input) {
	ability_data *new;
	
	CREATE(new, ability_data, 1);
	clear_ability(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		ABIL_NAME(new) = ABIL_NAME(input) ? str_dup(ABIL_NAME(input)) : NULL;
		ABIL_COMMAND(new) = ABIL_COMMAND(input) ? str_dup(ABIL_COMMAND(input)) : NULL;
		ABIL_RESOURCE_COST(new) = copy_resource_list(ABIL_RESOURCE_COST(input));
		ABIL_CUSTOM_MSGS(new) = copy_custom_messages(ABIL_CUSTOM_MSGS(input));
		ABIL_APPLIES(new) = copy_apply_list(ABIL_APPLIES(input));
		ABIL_DATA(new) = copy_data_list(ABIL_DATA(input));
		ABIL_INTERACTIONS(new) = copy_interaction_list(ABIL_INTERACTIONS(input));
		ABIL_HOOKS(new) = copy_ability_hooks(ABIL_HOOKS(input));
	}
	else {
		// brand new: some defaults
		ABIL_NAME(new) = str_dup(default_ability_name);
	}
	
	// done
	return new;	
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* For vstat.
*
* @param char_data *ch The player requesting stats.
* @param ability_data *abil The ability to display.
*/
void do_stat_ability(char_data *ch, ability_data *abil) {
	char buf[MAX_STRING_LENGTH*4], part[MAX_STRING_LENGTH], part2[MAX_STRING_LENGTH];
	struct ability_data_list *adl;
	struct ability_hook *ahook;
	struct custom_message *custm;
	struct apply_data *app;
	size_t size;
	int count;
	
	if (!abil) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \tc%s\t0\r\n", ABIL_VNUM(abil), ABIL_NAME(abil));

	size += snprintf(buf + size, sizeof(buf) - size, "Scale: [\ty%d%%\t0], Mastery ability: [\ty%d\t0] \ty%s\t0\r\n", (int)(ABIL_SCALE(abil) * 100), ABIL_MASTERY_ABIL(abil), ABIL_MASTERY_ABIL(abil) == NOTHING ? "none" : get_ability_name_by_vnum(ABIL_MASTERY_ABIL(abil)));
	
	get_ability_type_display(ABIL_TYPE_LIST(abil), part, FALSE);
	size += snprintf(buf + size, sizeof(buf) - size, "Types: \tc%s\t0\r\n", part);
	
	sprintbit(ABIL_FLAGS(abil), ability_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	sprintbit(ABIL_IMMUNITIES(abil), affected_bits, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Immunities: \tc%s\t0\r\n", part);
	
	sprintbit(ABIL_GAIN_HOOKS(abil), ability_gain_hooks, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Gain hooks: \tg%s\t0\r\n", part);
	
	// command-related portion
	sprintbit(ABIL_TARGETS(abil), ability_target_flags, part, TRUE);
	if (!ABIL_COMMAND(abil)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Command info: [\tcnot a command\t0], Targets: \tg%s\t0\r\n", part);
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, "Command info: [\ty%s\t0], Targets: \tg%s\t0\r\n", ABIL_COMMAND(abil), part);
	}
	size += snprintf(buf + size, sizeof(buf) - size, "Cost: [\tc%d %s (+%d/scale)\t0], Cooldown: [\tc%d %s\t0], Cooldown time: [\tc%d second%s\t0]\r\n", ABIL_COST(abil), pool_types[ABIL_COST_TYPE(abil)], ABIL_COST_PER_SCALE_POINT(abil), ABIL_COOLDOWN(abil), get_generic_name_by_vnum(ABIL_COOLDOWN(abil)),  ABIL_COOLDOWN_SECS(abil), PLURAL(ABIL_COOLDOWN_SECS(abil)));
	
	// resources
	get_resource_display(ABIL_RESOURCE_COST(abil), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Resources cost:%s\r\n%s", ABIL_RESOURCE_COST(abil) ? "" : " none", ABIL_RESOURCE_COST(abil) ? part : "");
	
	size += snprintf(buf + size, sizeof(buf) - size, "Min position: [\tc%s\t0], Linked trait: [\ty%s\t0]\r\n", position_types[ABIL_MIN_POS(abil)], apply_types[ABIL_LINKED_TRAIT(abil)]);
	
	prettier_sprintbit(ABIL_REQUIRES_TOOL(abil), tool_flags, part);
	size += snprintf(buf + size, sizeof(buf) - size, "Requires tool: &g%s&0\r\n", part);
	
	size += snprintf(buf + size, sizeof(buf) - size, "Difficulty: \ty%s\t0, Wait type: [\ty%s\t0]\r\n", skill_check_difficulty[ABIL_DIFFICULTY(abil)], wait_types[ABIL_WAIT_TYPE(abil)]);
	
	// ABILT_x: type-specific data
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_DOT | ABILT_ROOM_AFFECT)) {
		if (ABIL_SHORT_DURATION(abil) == UNLIMITED) {
			strcpy(part, "unlimited");
		}
		else {
			snprintf(part, sizeof(part), "%d", ABIL_SHORT_DURATION(abil));
		}
		if (ABIL_LONG_DURATION(abil) == UNLIMITED) {
			strcpy(part2, "unlimited");
		}
		else {
			snprintf(part2, sizeof(part2), "%d", ABIL_LONG_DURATION(abil));
		}
		size += snprintf(buf + size, sizeof(buf) - size, "Durations: [\tc%s/%s seconds\t0]\r\n", part, part2);
		
		size += snprintf(buf + size, sizeof(buf) - size, "Custom affect: [\ty%d %s\t0]\r\n", ABIL_AFFECT_VNUM(abil), get_generic_name_by_vnum(ABIL_AFFECT_VNUM(abil)));
	}	// end buff/dot
	if (IS_SET(ABIL_TYPES(abil), ABILT_ROOM_AFFECT)) {
		sprintbit(ABIL_AFFECTS(abil), room_aff_bits, part, TRUE);
		size += snprintf(buf + size, sizeof(buf) - size, "Room affect flags: \tg%s\t0\r\n", part);
	}
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_PASSIVE_BUFF)) {
		sprintbit(ABIL_AFFECTS(abil), affected_bits, part, TRUE);
		size += snprintf(buf + size, sizeof(buf) - size, "Affect flags: \tg%s\t0\r\n", part);
		
		// applies
		size += snprintf(buf + size, sizeof(buf) - size, "Applies: ");
		count = 0;
		LL_FOREACH(ABIL_APPLIES(abil), app) {
			size += snprintf(buf + size, sizeof(buf) - size, "%s%d to %s", count++ ? ", " : "", app->weight, apply_types[app->location]);
		}
		if (!ABIL_APPLIES(abil)) {
			size += snprintf(buf + size, sizeof(buf) - size, "none");
		}
		size += snprintf(buf + size, sizeof(buf) - size, "\r\n");
	}	// end buff
	if (IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Attack type: [\tc%d %s\t0]\r\n", ABIL_ATTACK_TYPE(abil), get_attack_name_by_vnum(ABIL_ATTACK_TYPE(abil)));
	}	// end damage
	if (IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE | ABILT_DOT)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Damage type: [\tc%s\t0]\r\n", damage_types[ABIL_DAMAGE_TYPE(abil)]);
	}	// end damage/dot
	if (IS_SET(ABIL_TYPES(abil), ABILT_DOT)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Max stacks: [\tc%d\t0]\r\n", ABIL_MAX_STACKS(abil));
	}	// end dot
	
	if (ABIL_CUSTOM_MSGS(abil)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Custom messages:\r\n");
		
		LL_FOREACH(ABIL_CUSTOM_MSGS(abil), custm) {
			size += snprintf(buf + size, sizeof(buf) - size, " %s: %s\r\n", ability_custom_types[custm->type], custm->msg);
		}
	}
	
	// hooks
	if (ABIL_HOOKS(abil)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Ability hooks:\r\n");
		count = 0;
		LL_FOREACH(ABIL_HOOKS(abil), ahook) {
			size += snprintf(buf + size, sizeof(buf) - size, " %d. %s\r\n", ++count, ability_hook_display(ahook));
		}
	}
	
	if (ABIL_INTERACTIONS(abil)) {
		get_interaction_display(ABIL_INTERACTIONS(abil), part);
		size += snprintf(buf + size, sizeof(buf) - size, "Interactions:\r\n%s", part);
	}
	
	// data
	if (ABIL_DATA(abil)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Extra data:\r\n");
		count = 0;
		LL_FOREACH(ABIL_DATA(abil), adl) {
			size += snprintf(buf + size, sizeof(buf) - size, " %d. %s\r\n", ++count, ability_data_display(adl));
		}
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* This is the main recipe display for ability OLC. It displays the user's
* currently-edited ability.
*
* @param char_data *ch The person who is editing an ability and will see its display.
*/
void olc_show_ability(char_data *ch) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	char buf[MAX_STRING_LENGTH * 4], lbuf[MAX_STRING_LENGTH];
	struct ability_data_list *adl;
	struct ability_hook *ahook;
	struct custom_message *custm;
	struct apply_data *apply;
	int count;
	
	if (!abil) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !find_ability_by_vnum(ABIL_VNUM(abil)) ? "new ability" : get_ability_name_by_vnum(ABIL_VNUM(abil)));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(ABIL_NAME(abil), default_ability_name), NULLSAFE(ABIL_NAME(abil)));
	
	get_ability_type_display(ABIL_TYPE_LIST(abil), lbuf, FALSE);
	sprintf(buf + strlen(buf), "<%stypes\t0> %s\r\n", OLC_LABEL_PTR(ABIL_TYPE_LIST(abil)), lbuf);
	
	sprintf(buf + strlen(buf), "<%smasteryability\t0> %d %s\r\n", OLC_LABEL_VAL(ABIL_MASTERY_ABIL(abil), NOTHING), ABIL_MASTERY_ABIL(abil), ABIL_MASTERY_ABIL(abil) == NOTHING ? "none" : get_ability_name_by_vnum(ABIL_MASTERY_ABIL(abil)));
	sprintf(buf + strlen(buf), "<%sscale\t0> %d%%\r\n", OLC_LABEL_VAL(ABIL_SCALE(abil), 1.0), (int)(ABIL_SCALE(abil) * 100));
	
	sprintbit(ABIL_FLAGS(abil), ability_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(ABIL_FLAGS(abil), NOBITS), lbuf);
	
	sprintbit(ABIL_IMMUNITIES(abil), affected_bits, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%simmunities\t0> %s\r\n", OLC_LABEL_VAL(ABIL_IMMUNITIES(abil), NOBITS), lbuf);
	
	sprintbit(ABIL_GAIN_HOOKS(abil), ability_gain_hooks, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sgainhooks\t0> %s\r\n", OLC_LABEL_VAL(ABIL_GAIN_HOOKS(abil), NOBITS), lbuf);
	
	// command-related portion
	sprintbit(ABIL_TARGETS(abil), ability_target_flags, lbuf, TRUE);
	if (!ABIL_COMMAND(abil)) {
		sprintf(buf + strlen(buf), "<%scommand\t0> (not a command), <%stargets\t0> %s\r\n", OLC_LABEL_UNCHANGED, OLC_LABEL_VAL(ABIL_TARGETS(abil), NOBITS), lbuf);
	}
	else {
		sprintf(buf + strlen(buf), "<%scommand\t0> %s, <%stargets\t0> %s\r\n", OLC_LABEL_CHANGED, ABIL_COMMAND(abil), OLC_LABEL_VAL(ABIL_TARGETS(abil), NOBITS), lbuf);
	}
	sprintf(buf + strlen(buf), "<%scost\t0> %d, <%scostperscalepoint\t0> %d, <%scosttype\t0> %s\r\n", OLC_LABEL_VAL(ABIL_COST(abil), 0), ABIL_COST(abil), OLC_LABEL_VAL(ABIL_COST_PER_SCALE_POINT(abil), 0), ABIL_COST_PER_SCALE_POINT(abil), OLC_LABEL_VAL(ABIL_COST_TYPE(abil), 0), pool_types[ABIL_COST_TYPE(abil)]);
	sprintf(buf + strlen(buf), "<%scooldown\t0> [%d] %s, <%scdtime\t0> %d second%s\r\n", OLC_LABEL_VAL(ABIL_COOLDOWN(abil), NOTHING), ABIL_COOLDOWN(abil), get_generic_name_by_vnum(ABIL_COOLDOWN(abil)), OLC_LABEL_VAL(ABIL_COOLDOWN_SECS(abil), 0), ABIL_COOLDOWN_SECS(abil), PLURAL(ABIL_COOLDOWN_SECS(abil)));

	// resources
	get_resource_display(ABIL_RESOURCE_COST(abil), lbuf);
	sprintf(buf + strlen(buf), "<%sresourcecost\t0>%s\r\n%s", OLC_LABEL_PTR(ABIL_RESOURCE_COST(abil)), ABIL_RESOURCE_COST(abil) ? "" : " none", ABIL_RESOURCE_COST(abil) ? lbuf : "");

	sprintf(buf + strlen(buf), "<%sminposition\t0> %s (minimum), <%slinkedtrait\t0> %s\r\n", OLC_LABEL_VAL(ABIL_MIN_POS(abil), POS_STANDING), position_types[ABIL_MIN_POS(abil)], OLC_LABEL_VAL(ABIL_LINKED_TRAIT(abil), APPLY_NONE), apply_types[ABIL_LINKED_TRAIT(abil)]);
	
	sprintbit(ABIL_REQUIRES_TOOL(abil), tool_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%stools\t0> %s\r\n", OLC_LABEL_VAL(ABIL_REQUIRES_TOOL(abil), NOBITS), lbuf);
	
	sprintf(buf + strlen(buf), "<%sdifficulty\t0> %s, <%swaittype\t0> %s\r\n", OLC_LABEL_VAL(ABIL_DIFFICULTY(abil), 0), skill_check_difficulty[ABIL_DIFFICULTY(abil)], OLC_LABEL_VAL(ABIL_WAIT_TYPE(abil), WAIT_NONE), wait_types[ABIL_WAIT_TYPE(abil)]);
	
	// type-specific data
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_DOT | ABILT_ROOM_AFFECT)) {
		if (ABIL_SHORT_DURATION(abil) == UNLIMITED) {
			sprintf(buf + strlen(buf), "<%sshortduration\t0> unlimited, ", OLC_LABEL_CHANGED);
		}
		else {
			sprintf(buf + strlen(buf), "<%sshortduration\t0> %d second%s, ", OLC_LABEL_VAL(ABIL_SHORT_DURATION(abil), 0), ABIL_SHORT_DURATION(abil), PLURAL(ABIL_SHORT_DURATION(abil)));
		}
		
		if (ABIL_LONG_DURATION(abil) == UNLIMITED) {
			sprintf(buf + strlen(buf), "<%slongduration\t0> unlimited\r\n", OLC_LABEL_CHANGED);
		}
		else {
			sprintf(buf + strlen(buf), "<%slongduration\t0> %d second%s\r\n", OLC_LABEL_VAL(ABIL_LONG_DURATION(abil), 0), ABIL_LONG_DURATION(abil), PLURAL(ABIL_LONG_DURATION(abil)));
		}
	}	// end buff/dot
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_PASSIVE_BUFF)) {
		sprintbit(ABIL_AFFECTS(abil), affected_bits, lbuf, TRUE);
		sprintf(buf + strlen(buf), "<%saffects\t0> %s\r\n", OLC_LABEL_VAL(ABIL_AFFECTS(abil), NOBITS), lbuf);
		
		sprintf(buf + strlen(buf), "Applies: <%sapply\t0>\r\n", OLC_LABEL_PTR(ABIL_APPLIES(abil)));
		count = 0;
		LL_FOREACH(ABIL_APPLIES(abil), apply) {
			sprintf(buf + strlen(buf), " %2d. %d to %s\r\n", ++count, apply->weight, apply_types[apply->location]);
		}
	}	// end buff
	if (IS_SET(ABIL_TYPES(abil), ABILT_ROOM_AFFECT)) {
		sprintbit(ABIL_AFFECTS(abil), room_aff_bits, lbuf, TRUE);
		sprintf(buf + strlen(buf), "<%saffects\t0> %s\r\n", OLC_LABEL_VAL(ABIL_AFFECTS(abil), NOBITS), lbuf);
	}	// end room affect
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_DOT | ABILT_ROOM_AFFECT)) {
		sprintf(buf + strlen(buf), "<%saffectvnum\t0> %d %s\r\n", OLC_LABEL_VAL(ABIL_AFFECT_VNUM(abil), NOTHING), ABIL_AFFECT_VNUM(abil), get_generic_name_by_vnum(ABIL_AFFECT_VNUM(abil)));
	}	// end buff/dot
	if (IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE)) {
		sprintf(buf + strlen(buf), "<%sattacktype\t0> %d %s\r\n", OLC_LABEL_VAL(ABIL_ATTACK_TYPE(abil), 0), ABIL_ATTACK_TYPE(abil), get_attack_name_by_vnum(ABIL_ATTACK_TYPE(abil)));
	}	// end damage
	if (IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE | ABILT_DOT)) {
		sprintf(buf + strlen(buf), "<%sdamagetype\t0> %s\r\n", OLC_LABEL_VAL(ABIL_DAMAGE_TYPE(abil), 0), damage_types[ABIL_DAMAGE_TYPE(abil)]);
	}	// end damage/dot
	if (IS_SET(ABIL_TYPES(abil), ABILT_DOT)) {
		sprintf(buf + strlen(buf), "<%smaxstacks\t0> %d\r\n", OLC_LABEL_VAL(ABIL_MAX_STACKS(abil), 1), ABIL_MAX_STACKS(abil));
	}	// end dot
	
	// custom messages
	sprintf(buf + strlen(buf), "Custom messages: <%scustom\t0>\r\n", OLC_LABEL_PTR(ABIL_CUSTOM_MSGS(abil)));
	count = 0;
	LL_FOREACH(ABIL_CUSTOM_MSGS(abil), custm) {
		sprintf(buf + strlen(buf), " \ty%d\t0. [%s] %s\r\n", ++count, ability_custom_types[custm->type], custm->msg);
	}
	
	// hooks
	sprintf(buf + strlen(buf), "Ability Hooks: <%shook\t0>\r\n", OLC_LABEL_PTR(ABIL_HOOKS(abil)));
	count = 0;
	LL_FOREACH(ABIL_HOOKS(abil), ahook) {
		sprintf(buf + strlen(buf), " \ty%d\t0. %s\r\n", ++count, ability_hook_display(ahook));
	}
	
	// interactions
	sprintf(buf + strlen(buf), "Interactions: <%sinteraction\t0>\r\n", OLC_LABEL_PTR(ABIL_INTERACTIONS(abil)));
	if (ABIL_INTERACTIONS(abil)) {
		get_interaction_display(ABIL_INTERACTIONS(abil), lbuf);
		strcat(buf, lbuf);
	}
	
	// data
	sprintf(buf + strlen(buf), "Data: <%sdata\t0>\r\n", OLC_LABEL_PTR(ABIL_DATA(abil)));
	count = 0;
	LL_FOREACH(ABIL_DATA(abil), adl) {
		sprintf(buf + strlen(buf), " \ty%d\t0. %s\r\n", ++count, ability_data_display(adl));
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the ability db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_ability(char *searchname, char_data *ch) {
	ability_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, ability_table, iter, next_iter) {
		if (multi_isname(searchname, ABIL_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, ABIL_VNUM(iter), ABIL_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(abiledit_affects) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_PASSIVE_BUFF)) {
		ABIL_AFFECTS(abil) = olc_process_flag(ch, argument, "affects", "affects", affected_bits, ABIL_AFFECTS(abil));
	}
	else if (IS_SET(ABIL_TYPES(abil), ABILT_ROOM_AFFECT)) {
		ABIL_AFFECTS(abil) = olc_process_flag(ch, argument, "affects", "affects", room_aff_bits, ABIL_AFFECTS(abil));
	}
	else {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
}


OLC_MODULE(abiledit_affectvnum) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	generic_data *gen;
	any_vnum old;
	
	bitvector_t allowed_types = ABILT_BUFF | ABILT_DOT | ABILT_ROOM_AFFECT;
	
	if (!IS_SET(ABIL_TYPES(abil), allowed_types)) {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		ABIL_AFFECT_VNUM(abil) = NOTHING;
		msg_to_char(ch, "It no longer has a custom affect type.\r\n");
	}
	else {
		old = ABIL_AFFECT_VNUM(abil);
		ABIL_AFFECT_VNUM(abil) = olc_process_number(ch, argument, "affect vnum", "affectvnum", 0, MAX_VNUM, ABIL_AFFECT_VNUM(abil));

		if ((gen = find_generic(ABIL_AFFECT_VNUM(abil), GENERIC_AFFECT))) {
			msg_to_char(ch, "It is now: %s\r\n", GEN_NAME(gen));
		}
		else {
			msg_to_char(ch, "Invalid affect generic vnum %d. Old value restored.\r\n", ABIL_AFFECT_VNUM(abil));
			ABIL_AFFECT_VNUM(abil) = old;
		}
	}
}


OLC_MODULE(abiledit_apply) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	olc_process_applies(ch, argument, &ABIL_APPLIES(abil));
}


OLC_MODULE(abiledit_attacktype) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	int old = ABIL_ATTACK_TYPE(abil);
	attack_message_data *amd;
	
	bitvector_t allowed_types = ABILT_DAMAGE;
	
	if (!IS_SET(ABIL_TYPES(abil), allowed_types)) {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
	else {
		ABIL_ATTACK_TYPE(abil) = olc_process_number(ch, argument, "attack type", "attacktype", 0, MAX_VNUM, ABIL_ATTACK_TYPE(abil));
		if ((amd = real_attack_message(ABIL_ATTACK_TYPE(abil)))) {
			msg_to_char(ch, "It is now: %s\r\n", ATTACK_NAME(amd));
		}
		else {
			msg_to_char(ch, "Invalid attack message vnum %d. Old value restored.\r\n", ABIL_ATTACK_TYPE(abil));
			ABIL_ATTACK_TYPE(abil) = old;
		}
	}
}


OLC_MODULE(abiledit_cdtime) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	if (ABIL_COOLDOWN(abil) == NOTHING) {
		msg_to_char(ch, "Set a cooldown vnum first.\r\n");
	}
	else {
		ABIL_COOLDOWN_SECS(abil) = olc_process_number(ch, argument, "cooldown time", "cdtime", 0, INT_MAX, ABIL_COOLDOWN_SECS(abil));
	}
}


OLC_MODULE(abiledit_command) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	if (!str_cmp(argument, "none")) {
		if (ABIL_COMMAND(abil)) {
			free(ABIL_COMMAND(abil));
		}
		ABIL_COMMAND(abil) = NULL;
		msg_to_char(ch, "It no longer has a command.\r\n");
	}
	else if (strchr(argument, ' ')) {
		msg_to_char(ch, "Commands can only be 1 word long (no spaces).\r\n");
	}
	else if (!isalpha(*argument)) {
		msg_to_char(ch, "Command must start with a letter.\r\n");
	}
	else {
		olc_process_string(ch, argument, "command", &ABIL_COMMAND(abil));
		
		if (ABIL_WAIT_TYPE(abil) == WAIT_NONE) {
			// set a default now
			ABIL_WAIT_TYPE(abil) = WAIT_ABILITY;
		}
	}
}


OLC_MODULE(abiledit_cooldown) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	any_vnum old;
	char arg2[MAX_INPUT_LENGTH];
	
	if (!str_cmp(argument, "none")) {
		ABIL_COOLDOWN(abil) = NOTHING;
		ABIL_COOLDOWN_SECS(abil) = 0;
		msg_to_char(ch, "It no longer has a cooldown.\r\n");
	}
	else {
		two_arguments(argument, arg, arg2);
		
		old = ABIL_COOLDOWN(abil);
		ABIL_COOLDOWN(abil) = olc_process_number(ch, arg, "cooldown vnum", "cooldown", 0, MAX_VNUM, ABIL_COOLDOWN(abil));

		if (!find_generic(ABIL_COOLDOWN(abil), GENERIC_COOLDOWN)) {
			msg_to_char(ch, "Invalid cooldown generic vnum %d. Old value restored.\r\n", ABIL_COOLDOWN(abil));
			ABIL_COOLDOWN(abil) = old;
		}
		else if (*arg2) {
			// pass thru to cdtime
			abiledit_cdtime(ch, type, arg2);
		}
	}
}


OLC_MODULE(abiledit_cost) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	char arg2[MAX_INPUT_LENGTH];
	
	two_arguments(argument, arg, arg2);
	ABIL_COST(abil) = olc_process_number(ch, arg, "cost", "cost", 0, INT_MAX, ABIL_COST(abil));
	
	// optional cost type: pass through
	if (*arg2) {
		abiledit_costtype(ch, type, arg2);
	}
}


OLC_MODULE(abiledit_costperscalepoint) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	ABIL_COST_PER_SCALE_POINT(abil) = olc_process_number(ch, argument, "cost per scale point", "costperscalepoint", 0, INT_MAX, ABIL_COST_PER_SCALE_POINT(abil));
}


OLC_MODULE(abiledit_costtype) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	ABIL_COST_TYPE(abil) = olc_process_type(ch, argument, "cost type", "costtype", pool_types, ABIL_COST_TYPE(abil));
}


OLC_MODULE(abiledit_custom) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	olc_process_custom_messages(ch, argument, &ABIL_CUSTOM_MSGS(abil), ability_custom_types, ability_custom_type_help);
}


OLC_MODULE(abiledit_damagetype) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	bitvector_t allowed_types = ABILT_DAMAGE | ABILT_DOT;
	
	if (!IS_SET(ABIL_TYPES(abil), allowed_types)) {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
	else {
		ABIL_DAMAGE_TYPE(abil) = olc_process_type(ch, argument, "damage type", "damagetype", damage_types, ABIL_DAMAGE_TYPE(abil));
	}
}


OLC_MODULE(abiledit_data) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	char type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], temp[256];
	ability_data *find_abil;
	struct ability_data_list *adl, *next_adl;
	bitvector_t allowed_types = 0;
	int iter, num, type_id, val_id, misc = 0;
	bool found;
	
	// ADL_x: determine valid types first
	allowed_types |= ADL_EFFECT | ADL_LIMITATION | ADL_RANGE | ADL_PARENT;
	if (IS_SET(ABIL_TYPES(abil), ABILT_ACTION)) {
		allowed_types |= ADL_ACTION;
	}
	if (IS_SET(ABIL_TYPES(abil), ABILT_PLAYER_TECH)) {
		allowed_types |= ADL_PLAYER_TECH;
	}
	if (IS_SET(ABIL_TYPES(abil), ABILT_READY_WEAPONS)) {
		allowed_types |= ADL_READY_WEAPON;
	}
	if (IS_SET(ABIL_TYPES(abil), ABILT_COMPANION | ABILT_SUMMON_ANY | ABILT_SUMMON_RANDOM)) {
		allowed_types |= ADL_SUMMON_MOB;
	}
	if (IS_SET(ABIL_TYPES(abil), ABILT_PAINT_BUILDING)) {
		allowed_types |= ADL_PAINT_COLOR;
	}
	
	// arg1 arg2
	half_chop(argument, arg1, arg2);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which data entry number?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			while ((adl = ABIL_DATA(abil))) {
				ABIL_DATA(abil) = adl->next;
				free(adl);
			}
			ABIL_DATA(abil) = NULL;
			msg_to_char(ch, "You remove all the data.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid data number to remove.\r\n");
		}
		else {
			found = FALSE;
			LL_FOREACH_SAFE(ABIL_DATA(abil), adl, next_adl) {
				if (--num == 0) {
					found = TRUE;
					msg_to_char(ch, "You remove data #%d: %s\r\n", atoi(arg2), ability_data_display(adl));
					LL_DELETE(ABIL_DATA(abil), adl);
					free(adl);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid data number to remove.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		half_chop(arg2, type_arg, val_arg);
		
		if (!*type_arg || !*val_arg) {
			msg_to_char(ch, "Usage: data add <type> <name | vnum>\r\n");
		}
		else if ((type_id = search_block(type_arg, ability_data_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", type_arg);
		}
		else if (!IS_SET(allowed_types, BIT(type_id))) {
			msg_to_char(ch, "This ability doesn't allow that type of data.\r\n");
		}
		else {
			val_id = NOTHING;
			
			// ADL_x: determine which one to add
			switch (BIT(type_id)) {
				case ADL_PLAYER_TECH: {
					if ((val_id = search_block(val_arg, player_tech_types, FALSE)) == NOTHING) {
						msg_to_char(ch, "Invalid player tech '%s'.\r\n", val_arg);
						return;
					}
					break;
				}
				case ADL_EFFECT: {
					if ((val_id = search_block_multi_isname(val_arg, ability_effects)) == NOTHING) {
						msg_to_char(ch, "Invalid ability effect '%s'.\r\n", val_arg);
						return;
					}
					break;
				}
				case ADL_READY_WEAPON: {
					if ((val_id = atoi(val_arg)) <= 0 || !obj_proto(val_id)) {
						msg_to_char(ch, "Invalid weapon vnum to ready '%s'.\r\n", val_arg);
						return;
					}
					break;
				}
				case ADL_SUMMON_MOB: {
					if ((val_id = atoi(val_arg)) <= 0 || !mob_proto(val_id)) {
						msg_to_char(ch, "Invalid mob vnum for summonable mob '%s'.\r\n", val_arg);
						return;
					}
					break;
				}
				case ADL_LIMITATION: {
					char val_a[MAX_INPUT_LENGTH], val_b[MAX_INPUT_LENGTH];
					
					// these sometimes have a "misc" value so we try to split this
					chop_last_arg(val_arg, val_a, val_b);
					
					// try whole arg first
					if ((val_id = search_block_multi_isname(val_arg, ability_limitations)) != NOTHING) {
						if (ability_limitation_misc[val_id] != ABLIM_NOTHING) {
							msg_to_char(ch, "You need to provide a type for the %s limitation.\r\n", ability_limitations[val_id]);
							return;
						}
					}
					else if ((val_id = search_block_multi_isname(val_a, ability_limitations)) != NOTHING && ability_limitation_misc[val_id] != ABLIM_NOTHING) {
						// ABLIM_x: partial arg: process val_b as limit type
						switch (ability_limitation_misc[val_id]) {
							case ABLIM_ITEM_TYPE: {
								if ((misc = search_block(val_b, item_types, FALSE)) == NOTHING) {
									msg_to_char(ch, "Invalid item type '%s'. See HELP OEDIT TYPES.\r\n", val_b);
									return;
								}
								break;
							}
							case ABLIM_ATTACK_TYPE: {
								attack_message_data *amd;
								
								if (!*val_b) {
									msg_to_char(ch, "Set the attack type to what attack message (vnum or name)?\r\n");
									return;
								}
								else if (!(amd = find_attack_message_by_name_or_vnum(argument, FALSE))) {
									msg_to_char(ch, "Unknown attack message '%s'.\r\n", argument);
									return;
								}
								else {
									misc = ATTACK_VNUM(amd);
								}
								break;
							}
							case ABLIM_WEAPON_TYPE: {
								if ((misc = search_block(val_b, weapon_types, FALSE)) == NOTHING) {
									msg_to_char(ch, "Invalid weapon type '%s'. See HELP OEDIT TYPES.\r\n", val_b);
									return;
								}
								break;
							}
						}
						// otherwise pass through and we're good
					}
					else {
						msg_to_char(ch, "Invalid limitation '%s'.\r\n", val_arg);
						return;
					}
					break;
				}
				case ADL_PAINT_COLOR: {
					if ((val_id = search_block(val_arg, paint_names, FALSE)) == NOTHING) {
						msg_to_char(ch, "Invalid paint color '%s'.\r\n", val_arg);
						return;
					}
					break;
				}
				case ADL_ACTION: {
					if ((val_id = search_block_multi_isname(val_arg, ability_actions)) == NOTHING) {
						msg_to_char(ch, "Invalid ability action '%s'.\r\n", val_arg);
						return;
					}
					break;
				}
				case ADL_RANGE: {
					if (is_abbrev(val_arg, "unlimited") || is_abbrev(val_arg, "infinite") || !strcmp(val_arg, "-1")) {
						val_id = UNLIMITED;
					}
					else if (isdigit(*val_arg) && atoi(val_arg) > 0) {
						val_id = atoi(val_arg);
					}
					else {
						msg_to_char(ch, "Range must be 'unlimited' or a number. Invalid range '%s' given.\r\n", val_arg);
						return;
					}
				}
				case ADL_PARENT: {
					if (ABIL_ASSIGNED_SKILL(abil)) {
						msg_to_char(ch, "You cannot add a parent ability because this one is already assigned to a skill.\r\n");
						return;
					}
					else if (ABIL_IS_SYNERGY(abil)) {
						msg_to_char(ch, "You cannot add a parent ability because this one is already assigned as a synergy ability.\r\n");
						return;
					}
					else if (!(find_abil = find_ability(val_arg))) {
						msg_to_char(ch, "Unknown ability '%s'.\r\n", val_arg);
						return;
					}
					else if (has_ability_data_any(find_abil, ADL_PARENT)) {
						msg_to_char(ch, "You cannot chain parent abilities.\r\n");
						return;
					}
					val_id = ABIL_VNUM(find_abil);
					break;
				}
			}
			
			if (val_id == NOTHING) {
				msg_to_char(ch, "Invalid data '%s'.\r\n", val_arg);
			}
			else {
				CREATE(adl, struct ability_data_list, 1);
				adl->type = BIT(type_id);
				adl->vnum = val_id;
				adl->misc = misc;
				LL_APPEND(ABIL_DATA(abil), adl);
				
				msg_to_char(ch, "You add data: %s\r\n", ability_data_display(adl));
			}
		}
	}
	else {
		msg_to_char(ch, "Usage: data add <type> <name | vnum>\r\n");
		msg_to_char(ch, "Usage: data remove <number | all>\r\n");
		
		found = FALSE;
		msg_to_char(ch, "Allowed types:");
		for (iter = 0; *ability_data_types[iter] != '\n'; ++iter) {
			if (IS_SET(allowed_types, BIT(iter))) {
				prettier_sprintbit(BIT(iter), ability_data_types, temp);
				msg_to_char(ch, "%s%s", found ? ", " : " ", temp);
				found = TRUE;
			}
		}
		msg_to_char(ch, "%s\r\n", found ? "" : " none");
	}
}


OLC_MODULE(abiledit_difficulty) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	ABIL_DIFFICULTY(abil) = olc_process_type(ch, argument, "difficulty", "difficulty", skill_check_difficulty, ABIL_DIFFICULTY(abil));
}


OLC_MODULE(abiledit_gainhooks) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);	
	ABIL_GAIN_HOOKS(abil) = olc_process_flag(ch, argument, "gain hook", "gainhooks", ability_gain_hooks, ABIL_GAIN_HOOKS(abil));
}


OLC_MODULE(abiledit_hook) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	char type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], prc_arg[MAX_INPUT_LENGTH];
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], temp[256];
	struct ability_hook *ahook, *next_ahook;
	double percent = 100.0;
	int iter, num, type_id, val_id, misc = 0;
	bool found;
	
	// arg1 arg2
	half_chop(argument, arg1, arg2);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which ability hook entry (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			while ((ahook = ABIL_HOOKS(abil))) {
				ABIL_HOOKS(abil) = ahook->next;
				free(ahook);
			}
			ABIL_HOOKS(abil) = NULL;
			msg_to_char(ch, "You remove all the ability hooks.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid ability hook number to remove.\r\n");
		}
		else {
			found = FALSE;
			LL_FOREACH_SAFE(ABIL_HOOKS(abil), ahook, next_ahook) {
				if (--num == 0) {
					found = TRUE;
					msg_to_char(ch, "You remove ability hook #%d: %s\r\n", atoi(arg2), ability_hook_display(ahook));
					LL_DELETE(ABIL_HOOKS(abil), ahook);
					free(ahook);
					ABIL_HOOK_FLAGS(abil) = all_abil_hooks(abil);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid ability hook number to remove.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		half_chop(arg2, type_arg, arg3);
		half_chop(arg3, prc_arg, val_arg);
		
		if (!*type_arg || !*prc_arg) {
			msg_to_char(ch, "Usage: hook add <type> <percent> [value]\r\n");
		}
		else if ((type_id = search_block(type_arg, ability_hook_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", type_arg);
		}
		else if ((percent = atof(prc_arg)) < 0.01 || percent > 100.0) {
			msg_to_char(ch, "Percent must be between 0.01 and 100.0; '%s' given.\r\n", prc_arg);
		}
		else {
			val_id = NOTHING;
			
			// AHOOK_x: determine which one to add
			switch (BIT(type_id)) {
				case AHOOK_ABILITY: {
					ability_data *lookup;
					
					if (!*val_arg) {
						msg_to_char(ch, "No ability given.\r\n");
						return;
					}
					if (!(lookup = find_ability(val_arg))) {
						msg_to_char(ch, "Unknown ability '%s'.\r\n", val_arg);
						return;
					}
					val_id = ABIL_VNUM(lookup);
					break;
				}
				case AHOOK_ATTACK_TYPE: {
					attack_message_data *amd;
					
					if (!*val_arg) {
						msg_to_char(ch, "No attack type given.\r\n");
						return;
					}
					else if (!(amd = find_attack_message_by_name_or_vnum(argument, FALSE))) {
						msg_to_char(ch, "Unknown attack message '%s'.\r\n", argument);
						return;
					}
					else {
						val_id = ATTACK_VNUM(amd);
					}
					break;
				}
				case AHOOK_WEAPON_TYPE: {
					if (!*val_arg) {
						msg_to_char(ch, "No weapon type given.\r\n");
						return;
					}
					if ((val_id = search_block(val_arg, weapon_types, FALSE)) == NOTHING) {
						msg_to_char(ch, "Invalid weapon type '%s'.\r\n", val_arg);
						return;
					}
					break;
				}
				case AHOOK_DAMAGE_TYPE: {
					if (!*val_arg) {
						msg_to_char(ch, "No damage type given.\r\n");
						return;
					}
					if ((val_id = search_block(val_arg, damage_types, FALSE)) == NOTHING) {
						msg_to_char(ch, "Invalid damage type '%s'.\r\n", val_arg);
						return;
					}
					break;
				}
				
				// these require no data
				case AHOOK_ATTACK:
				case AHOOK_KILL:
				case AHOOK_MELEE_ATTACK:
				case AHOOK_RANGED_ATTACK: {
					val_id = 0;
					break;
				}
			}
			
			if (val_id == NOTHING) {
				msg_to_char(ch, "Invalid ability hook '%s'.\r\n", val_arg);
			}
			else {
				CREATE(ahook, struct ability_hook, 1);
				ahook->type = BIT(type_id);
				ahook->percent = percent;
				ahook->value = val_id;
				ahook->misc = misc;
				LL_APPEND(ABIL_HOOKS(abil), ahook);
				
				ABIL_HOOK_FLAGS(abil) |= ahook->type;
				
				msg_to_char(ch, "You add an ability hook: %s\r\n", ability_hook_display(ahook));
			}
		}
	}
	else {
		msg_to_char(ch, "Usage: hook add <type> <percent> [value]\r\n");
		msg_to_char(ch, "Usage: hook remove <number | all>\r\n");
		
		found = FALSE;
		msg_to_char(ch, "Allowed types:");
		for (iter = 0; *ability_hook_types[iter] != '\n'; ++iter) {
			prettier_sprintbit(BIT(iter), ability_hook_types, temp);
			msg_to_char(ch, "%s%s", found ? ", " : " ", temp);
			found = TRUE;
		}
		msg_to_char(ch, "%s\r\n", found ? "" : " none");
	}
}


OLC_MODULE(abiledit_flags) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);	
	ABIL_FLAGS(abil) = olc_process_flag(ch, argument, "ability", "flags", ability_flags, ABIL_FLAGS(abil));
}


OLC_MODULE(abiledit_immunities) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	ABIL_IMMUNITIES(abil) = olc_process_flag(ch, argument, "immunity", "immunities", affected_bits, ABIL_IMMUNITIES(abil));
}


OLC_MODULE(abiledit_interaction) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	olc_process_interactions(ch, argument, &ABIL_INTERACTIONS(abil), TYPE_ABIL);
}


OLC_MODULE(abiledit_linkedtrait) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	if (!IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_DOT | ABILT_DAMAGE | ABILT_PASSIVE_BUFF | ABILT_SUMMON_ANY | ABILT_SUMMON_RANDOM)) {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
	else {
		ABIL_LINKED_TRAIT(abil) = olc_process_type(ch, argument, "linked trait", "linkedtrait", apply_types, ABIL_LINKED_TRAIT(abil));
	}
}


OLC_MODULE(abiledit_longduration) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	bitvector_t allowed_types = ABILT_BUFF | ABILT_DOT | ABILT_ROOM_AFFECT;
	
	if (!IS_SET(ABIL_TYPES(abil), allowed_types)) {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
	else if (is_abbrev(argument, "unlimited")) {
		if (IS_SET(ABIL_TYPES(abil), ABILT_DOT)) {
			msg_to_char(ch, "DoTs cannot be unlimited.\r\n");
		}
		else {
			ABIL_LONG_DURATION(abil) = UNLIMITED;
			msg_to_char(ch, "It now has unlimited long duration.\r\n");
		}
	}
	else {
		ABIL_LONG_DURATION(abil) = olc_process_number(ch, argument, "long duration", "longduration", 1, MAX_INT, ABIL_LONG_DURATION(abil));
	}
}


OLC_MODULE(abiledit_masteryability) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	ability_data *find;
	
	if (!*argument) {
		msg_to_char(ch, "Set the mastery ability to what?\r\n");
	}
	else if (!str_cmp(argument, "none") || atoi(argument) == -1) {
		ABIL_MASTERY_ABIL(abil) = NOTHING;
		msg_to_char(ch, "%s\r\n", PRF_FLAGGED(ch, PRF_NOREPEAT) ? config_get_string("ok_string") : "It now has no mastery ability.");
	}
	else if ((find = find_ability(argument))) {
		ABIL_MASTERY_ABIL(abil) = ABIL_VNUM(find);
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It now has %s (%d) as its mastery ability.\r\n", ABIL_NAME(find), ABIL_VNUM(find));
		}
	}
	else {
		msg_to_char(ch, "Unknown ability '%s'.\r\n", argument);
	}
}


OLC_MODULE(abiledit_maxstacks) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	bitvector_t allowed_types = ABILT_DOT;
	
	if (!IS_SET(ABIL_TYPES(abil), allowed_types)) {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
	else {
		ABIL_MAX_STACKS(abil) = olc_process_number(ch, argument, "max stacks", "maxstacks", 1, 1000, ABIL_MAX_STACKS(abil));
	}
}


OLC_MODULE(abiledit_minposition) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	ABIL_MIN_POS(abil) = olc_process_type(ch, argument, "position", "minposition", position_types, ABIL_MIN_POS(abil));
}


OLC_MODULE(abiledit_name) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	olc_process_string(ch, argument, "name", &ABIL_NAME(abil));
}


OLC_MODULE(abiledit_resourcecost) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	olc_process_resources(ch, argument, &ABIL_RESOURCE_COST(abil));
}


OLC_MODULE(abiledit_scale) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	int scale;
	
	scale = olc_process_number(ch, argument, "scale", "scale", 1, 1000, ABIL_SCALE(abil) * 100);
	ABIL_SCALE(abil) = scale / 100.0;
}


OLC_MODULE(abiledit_shortduration) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	bitvector_t allowed_types = ABILT_BUFF | ABILT_DOT | ABILT_ROOM_AFFECT;
	
	if (!IS_SET(ABIL_TYPES(abil), allowed_types)) {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
	else if (is_abbrev(argument, "unlimited")) {
		if (IS_SET(ABIL_TYPES(abil), ABILT_DOT)) {
			msg_to_char(ch, "DoTs cannot be unlimited.\r\n");
		}
		else {
			ABIL_SHORT_DURATION(abil) = UNLIMITED;
			msg_to_char(ch, "It now has unlimited short duration.\r\n");
		}
	}
	else {
		ABIL_SHORT_DURATION(abil) = olc_process_number(ch, argument, "short duration", "shortduration", 1, MAX_INT, ABIL_SHORT_DURATION(abil));
	}
}


OLC_MODULE(abiledit_targets) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	ABIL_TARGETS(abil) = olc_process_flag(ch, argument, "target", "targets", ability_target_flags, ABIL_TARGETS(abil));
}


OLC_MODULE(abiledit_tools) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	ABIL_REQUIRES_TOOL(abil) = olc_process_flag(ch, argument, "tool", "tools", tool_flags, ABIL_REQUIRES_TOOL(abil));
}


OLC_MODULE(abiledit_types) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], *weight_arg;
	struct ability_type *at, *next_at, *change;
	int iter, typeid, weight;
	bool found;
	
	// arg1 arg2
	half_chop(argument, arg1, arg2);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which type?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			while ((at = ABIL_TYPE_LIST(abil))) {
				remove_type_from_ability(abil, at->type);
			}
			msg_to_char(ch, "You remove all the types.\r\n");
		}
		else if ((typeid = search_block(arg2, ability_type_flags, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type to remove.\r\n");
		}
		else {
			found = FALSE;
			LL_FOREACH_SAFE(ABIL_TYPE_LIST(abil), at, next_at) {
				if (at->type == BIT(typeid)) {
					found = TRUE;
					sprintbit(BIT(typeid), ability_type_flags, buf, TRUE);
					msg_to_char(ch, "You remove %s(%d).\r\n", buf, at->weight);
					LL_DELETE(ABIL_TYPE_LIST(abil), at);
					free(at);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "None of that type to remove.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		weight_arg = any_one_word(arg2, arg);
		skip_spaces(&weight_arg);
		weight = 1;	// default
		
		if (!*arg) {
			msg_to_char(ch, "Usage: types add <type> [weight]\r\n");
		}
		else if ((typeid = search_block(arg, ability_type_flags, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", arg);
		}
		else if (*weight_arg && (!isdigit(*weight_arg) || (weight = atoi(weight_arg)) < 0)) {
			msg_to_char(ch, "Weight must be 0 or higher.\r\n");
		}
		else {
			add_type_to_ability(abil, BIT(typeid), weight);
			sprintbit(BIT(typeid), ability_type_flags, buf, TRUE);
			msg_to_char(ch, "You add a type: %s(%d)\r\n", buf, weight);
		}
	}
	else if (is_abbrev(arg1, "change")) {
		half_chop(arg2, num_arg, val_arg);
		
		if (!*num_arg || !*val_arg || !isdigit(*val_arg)) {
			msg_to_char(ch, "Usage: types change <type> <weight>\r\n");
			return;
		}
		if ((typeid = search_block(num_arg, ability_type_flags, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", num_arg);
			return;
		}
		
		// find which one to change
		change = NULL;
		LL_FOREACH(ABIL_TYPE_LIST(abil), at) {
			if (at->type == BIT(typeid)) {
				change = at;
				break;
			}
		}
		
		sprintbit(BIT(typeid), ability_type_flags, buf, TRUE);
		
		if (!change) {
			msg_to_char(ch, "Invalid type.\r\n");
		}
		else if ((weight = atoi(val_arg)) < 0) {
			msg_to_char(ch, "Weight must be 0 or higher.\r\n");
		}
		else {
			change->weight = weight;
			msg_to_char(ch, "Type '%s' changed to weight: %d\r\n", buf, weight);
		}
	}
	else {
		msg_to_char(ch, "Usage: types add <type> [weight]\r\n");
		msg_to_char(ch, "Usage: types change <type> <weight>\r\n");
		msg_to_char(ch, "Usage: types remove <type | all>\r\n");
		msg_to_char(ch, "Available types:\r\n");
		for (iter = 0; *ability_type_flags[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %s\r\n", ability_type_flags[iter]);
		}
	}
}


OLC_MODULE(abiledit_waittype) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	ABIL_WAIT_TYPE(abil) = olc_process_type(ch, argument, "wait type", "waittype", wait_types, ABIL_WAIT_TYPE(abil));
}
