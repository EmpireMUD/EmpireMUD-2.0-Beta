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

/**
* Contents:
*   Helpers
*   Ability Commands
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   Edit Modules
*/

// local data
const char *default_ability_name = "Unnamed Ability";

// local protos
void perform_ability_command(char_data *ch, ability_data *abil, char *argument);

// external consts
extern const char *ability_custom_types[];
extern const char *ability_data_types[];
extern const char *ability_effects[];
extern const char *ability_gain_hooks[];
extern const char *ability_flags[];
extern const char *ability_target_flags[];
extern const char *ability_type_flags[];
extern const char *affected_bits[];
extern const char *apply_types[];
extern const char *apply_type_names[];
extern const double apply_values[];
extern const char *damage_types[];
extern const char *player_tech_types[];
extern const char *pool_types[];
extern const char *position_types[];
extern const char *skill_check_difficulty[];
extern const char *wait_types[];

// external funcs
void check_combat_start(char_data *ch);
extern bool check_vampire_sun(char_data *ch, bool message);
extern int get_attack_type_by_name(char *name);
extern bool trigger_counterspell(char_data *ch);	// spells.c


// ability funcs
DO_ABIL(do_buff_ability);
PREP_ABIL(prep_buff_ability);
DO_ABIL(do_damage_ability);
PREP_ABIL(prep_damage_ability);
DO_ABIL(do_dot_ability);
PREP_ABIL(prep_dot_ability);


// setup for abilities
struct {
	bitvector_t type;	// ABILT_ const
	PREP_ABIL(*prep_func);	// does the cost setup
	DO_ABIL(*do_func);	// runs the ability
} do_ability_data[] = {
	
	// ABILT_x: setup by type
	{ ABILT_CRAFT, NULL, NULL },
	{ ABILT_BUFF, prep_buff_ability, do_buff_ability },
	{ ABILT_DAMAGE, prep_damage_ability, do_damage_ability },
	{ ABILT_DOT, prep_dot_ability, do_dot_ability },
	{ ABILT_PLAYER_TECH, NULL, NULL },
	
	{ NOBITS }	// this goes last
};


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* String display for one ADL item.
*
* @param struct ability_data_list *adl The data item to show.
* @return char* The string to display.
*/
char *ability_data_display(struct ability_data_list *adl) {
	static char output[MAX_STRING_LENGTH];
	char temp[256];
	
	prettier_sprintbit(adl->type, ability_data_types, temp);
	
	// ADL_x: display by type
	switch (adl->type) {
		case ADL_PLAYER_TECH: {
			snprintf(output, sizeof(output), "%s: %s", temp, player_tech_types[adl->vnum]);
			break;
		}
		case ADL_EFFECT: {
			snprintf(output, sizeof(output), "%s: %s", temp, ability_effects[adl->vnum]);
			break;
		}
		default: {
			snprintf(output, sizeof(output), "%s: ???", temp);
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
		adl->next = NULL;
		LL_APPEND(list, adl);
	}
	
	return list;
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
* @return ability_data* The ability, or NULL if it doesn't exist.
*/
ability_data *find_ability_by_name(char *name) {
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
		if (!partial && is_multiword_abbrev(name, ABIL_NAME(abil))) {
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
*/
void get_ability_type_display(struct ability_type *list, char *save_buffer) {
	struct ability_type *at;
	char lbuf[256];
	int count = 0;
	
	*save_buffer = '\0';
	LL_FOREACH(list, at) {
		sprintbit(at->type, ability_type_flags, lbuf, TRUE);
		sprintf(save_buffer + strlen(save_buffer), "%s%s(%d)", (count++ > 0) ? ", " : "", lbuf, at->weight);
	}
	if (count == 0) {
		strcat(save_buffer, "none");
	}
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
	ability_data *abil;
	double amount;
	
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
		if (IS_SET(agh->triggers, AGH_ONLY_WHEN_AFFECTED) && (!(abil = find_ability_by_vnum(agh->ability)) || !affected_by_spell(ch, ABIL_AFFECT_VNUM(abil)))) {
			continue;	// not currently affected
		}
		if (IS_SET(agh->triggers, AGH_ONLY_DARK) && !IS_DARK(IN_ROOM(ch))) {
			continue;	// not dark
		}
		if (IS_SET(agh->triggers, AGH_ONLY_LIGHT) && IS_DARK(IN_ROOM(ch))) {
			continue;	// not light
		}
		if (IS_SET(agh->triggers, AGH_ONLY_VS_ANIMAL) && (!opponent || !MOB_FLAGGED(opponent, MOB_ANIMAL))) {
			continue;	// not an animal
		}
		
		gain_ability_exp(ch, agh->ability, amount);
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
		points *= 1.0 + get_trait_modifier(ch, ABIL_LINKED_TRAIT(abil));
	}
	
	if (!IS_NPC(ch) && ABILITY_FLAGGED(abil, ABILITY_ROLE_FLAGS)) {
		points *= data->matching_role ? 1.20 : 0.80;
	}
	
	return MAX(1.0, points);	// ensure minimum of 1 point
}


 //////////////////////////////////////////////////////////////////////////////
//// ABILITY COMMANDS ////////////////////////////////////////////////////////

/**
* For abilities with 'effects' data, this function manages them. This function
* is silent unless an effect happens.
*
* @param ability_dta *abil The ability being used.
* @param char_data *ch The character using the ability.
*/
void apply_ability_effects(ability_data *abil, char_data *ch) {
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
		}
	}
}


/**
* This checks if "string" is an ability's command, and performs it if so.
*
* @param char_data *ch The actor.
* @param char *string The command they typed.
* @param bool exact if TRUE, must be an exact match; FALSE can be abbrev
* @return bool TRUE if it was an ability command and we acted; FALSE if not
*/
bool check_ability(char_data *ch, char *string, bool exact) {
	extern bool char_can_act(char_data *ch, int min_pos, bool allow_animal, bool allow_invulnerable);
	
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
	
	// ok check if we can perform it
	if (!char_can_act(ch, ABIL_MIN_POS(abil), !ABILITY_FLAGGED(abil, ABILF_NO_ANIMAL), !ABILITY_FLAGGED(abil, ABILF_NO_INVULNERABLE | ABILF_VIOLENT))) {
		return TRUE;	// sent its own error message
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
* @param char_data *cvict The character target, if any (may be NULL).
* @param obj_data *ovict The object target, if any (may be NULL).
* @param vehicle_data *vvict The vehicle target, if any (may be NULL).
* @param int level The level to use the ability at.
* @param int casttype SPELL_CAST, etc.
* @param struct ability_exec *data The execution data to pass back and forth.
*/
void call_ability(char_data *ch, ability_data *abil, char *argument, char_data *cvict, obj_data *ovict, vehicle_data *vvict, int level, int casttype, struct ability_exec *data) {
	char buf[MAX_STRING_LENGTH];
	bool violent, invis;
	int iter;
	
	if (!ch || !abil) {
		return;
	}
	
	violent = (ABILITY_FLAGGED(abil, ABILF_VIOLENT) || IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE | ABILT_DOT));
	invis = ABILITY_FLAGGED(abil, ABILF_INVISIBLE) ? TRUE : FALSE;
	
	if (RMT_FLAGGED(IN_ROOM(ch), RMT_PEACEFUL) && violent) {
		msg_to_char(ch, "You can't %s here.\r\n", SAFE_ABIL_COMMAND(abil));
		data->stop = TRUE;
		return;
	}
	
	if (cvict && cvict != ch && violent) {
		if (!can_fight(ch, cvict)) {
			act("You can't attack $N!", FALSE, ch, NULL, cvict, TO_CHAR);
			data->stop = TRUE;
			return;
		}
		if (!ABILITY_FLAGGED(abil, ABILF_RANGED | ABILF_RANGED_ONLY) && NOT_MELEE_RANGE(ch, cvict)) {
			msg_to_char(ch, "You need to be at melee range to do this.\r\n");
			data->stop = TRUE;
			return;
		}
		if (ABILITY_FLAGGED(abil, ABILF_RANGED_ONLY) && !NOT_MELEE_RANGE(ch, cvict) && FIGHTING(ch)) {
			msg_to_char(ch, "You need to be in ranged combat to do this.\r\n");
			data->stop = TRUE;
			return;
		}
	}
	if (ABILITY_TRIGGERS(ch, cvict, ovict, ABIL_VNUM(abil))) {
		data->stop = TRUE;
		return;
	}
	
	// determine costs and scales
	for (iter = 0; do_ability_data[iter].type != NOBITS && !data->stop; ++iter) {
		if (IS_SET(ABIL_TYPES(abil), do_ability_data[iter].type) && do_ability_data[iter].prep_func) {
			(do_ability_data[iter].prep_func)(ch, abil, level, cvict, data);
			
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
	
	// check costs and cooldowns now
	if (ABIL_COST_TYPE(abil) == BLOOD && !ABILITY_FLAGGED(abil, ABILF_IGNORE_SUN) && !check_vampire_sun(ch, TRUE)) {
		return;	// sun fail
	}
	if (!can_use_ability(ch, ABIL_VNUM(abil), ABIL_COST_TYPE(abil), data->cost, ABIL_COOLDOWN(abil))) {
		return;
	}
	
	// ready to start the ability:
	if (violent) {
		if (SHOULD_APPEAR(ch)) {
			appear(ch);
		}
	
		// start meters now, to track direct damage()
		check_combat_start(ch);
		if (cvict) {
			check_combat_start(cvict);
		}
	}
	
	// locked in! apply the effects
	apply_ability_effects(abil, ch);
	
	// counterspell?
	if (ABILITY_FLAGGED(abil, ABILF_COUNTERSPELLABLE) && violent && cvict && cvict != ch && trigger_counterspell(cvict)) {
		// to-char
		if (abil_has_custom_message(abil, ABIL_CUSTOM_COUNTERSPELL_TO_CHAR)) {
			act(abil_get_custom_message(abil, ABIL_CUSTOM_COUNTERSPELL_TO_CHAR), FALSE, ch, NULL, cvict, TO_CHAR);
		}
		else {
			snprintf(buf, sizeof(buf), "You %s $N, but $E counterspells it!", SAFE_ABIL_COMMAND(abil));
			act(buf, FALSE, ch, NULL, cvict, TO_CHAR);
		}
		
		// to vict
		if (abil_has_custom_message(abil, ABIL_CUSTOM_COUNTERSPELL_TO_VICT)) {
			act(abil_get_custom_message(abil, ABIL_CUSTOM_COUNTERSPELL_TO_VICT), FALSE, ch, NULL, cvict, TO_VICT);
		}
		else {
			snprintf(buf, sizeof(buf), "$n tries to %s you, but you counterspell it!", SAFE_ABIL_COMMAND(abil));
			act(buf, FALSE, ch, NULL, cvict, TO_VICT);
		}
		
		// to room
		if (abil_has_custom_message(abil, ABIL_CUSTOM_COUNTERSPELL_TO_ROOM)) {
			act(abil_get_custom_message(abil, ABIL_CUSTOM_COUNTERSPELL_TO_ROOM), FALSE, ch, NULL, cvict, TO_NOTVICT);
		}
		else {
			snprintf(buf, sizeof(buf), "$n tries to %s $N, but $E counterspells it!", SAFE_ABIL_COMMAND(abil));
			act(buf, FALSE, ch, NULL, cvict, TO_NOTVICT);
		}
		
		data->stop = TRUE;	// prevent routines from firing
		data->success = TRUE;	// counts as a successful ability use
		data->no_msg = TRUE;	// don't show more messages
	}
	
	// check for FAILURE:
	if (!skill_check(ch, ABIL_VNUM(abil), ABIL_DIFFICULTY(abil))) {
		if (ch == cvict || !cvict) {	// message: targeting self
			// to-char
			if (abil_has_custom_message(abil, ABIL_CUSTOM_FAIL_SELF_TO_CHAR)) {
				act(abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_SELF_TO_CHAR), FALSE, ch, NULL, cvict, TO_CHAR);
			}
			else {
				snprintf(buf, sizeof(buf), "You fail to use %s!", SAFE_ABIL_COMMAND(abil));
				act(buf, FALSE, ch, NULL, cvict, TO_CHAR);
			}
		
			// to room
			if (abil_has_custom_message(abil, ABIL_CUSTOM_FAIL_SELF_TO_ROOM)) {
				act(abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_SELF_TO_ROOM), invis, ch, NULL, cvict, TO_ROOM);
			}
			else {
				snprintf(buf, sizeof(buf), "$n fails to use %s!", SAFE_ABIL_COMMAND(abil));
				act(buf, invis, ch, NULL, cvict, TO_ROOM);
			}
		}
		else {	// message: ch != cvict
			// to-char
			if (abil_has_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_CHAR)) {
				act(abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_CHAR), FALSE, ch, NULL, cvict, TO_CHAR);
			}
			else {
				snprintf(buf, sizeof(buf), "You try to use %s on $N, but fail!", SAFE_ABIL_COMMAND(abil));
				act(buf, FALSE, ch, NULL, cvict, TO_CHAR);
			}
		
			// to cvict
			if (abil_has_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_VICT)) {
				act(abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_VICT), invis, ch, NULL, cvict, TO_VICT);
			}
			else {
				snprintf(buf, sizeof(buf), "$n tries to use %s on you, but fails!", SAFE_ABIL_COMMAND(abil));
				act(buf, invis, ch, NULL, cvict, TO_VICT);
			}
		
			// to room
			if (abil_has_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_ROOM)) {
				act(abil_get_custom_message(abil, ABIL_CUSTOM_FAIL_TARGETED_TO_ROOM), invis, ch, NULL, cvict, TO_NOTVICT);
			}
			else {
				snprintf(buf, sizeof(buf), "$n tries to use %s on $N, but fails!", SAFE_ABIL_COMMAND(abil));
				act(buf, invis, ch, NULL, cvict, TO_NOTVICT);
			}
		}
		
		data->success = TRUE;	// causes it to charge, skillup, and cooldown
		data->stop = TRUE;	// prevents normal activation
		data->no_msg = TRUE;	// prevents success message
	}
	
	// messaging
	if (cvict && !data->no_msg) {	// messaging with char target
		if (ch == cvict) {	// message: targeting self
			// to-char
			if (abil_has_custom_message(abil, ABIL_CUSTOM_SELF_TO_CHAR)) {
				act(abil_get_custom_message(abil, ABIL_CUSTOM_SELF_TO_CHAR), FALSE, ch, NULL, cvict, TO_CHAR);
			}
			else {
				snprintf(buf, sizeof(buf), "You use %s!", SAFE_ABIL_COMMAND(abil));
				act(buf, FALSE, ch, NULL, cvict, TO_CHAR);
			}
		
			// to room
			if (abil_has_custom_message(abil, ABIL_CUSTOM_SELF_TO_ROOM)) {
				act(abil_get_custom_message(abil, ABIL_CUSTOM_SELF_TO_ROOM), invis, ch, NULL, cvict, TO_ROOM);
			}
			else {
				snprintf(buf, sizeof(buf), "$n uses %s!", SAFE_ABIL_COMMAND(abil));
				act(buf, invis, ch, NULL, cvict, TO_ROOM);
			}
		}
		else {	// message: ch != cvict
			// to-char
			if (abil_has_custom_message(abil, ABIL_CUSTOM_TARGETED_TO_CHAR)) {
				act(abil_get_custom_message(abil, ABIL_CUSTOM_TARGETED_TO_CHAR), FALSE, ch, NULL, cvict, TO_CHAR);
			}
			else {
				snprintf(buf, sizeof(buf), "You use %s on $N!", SAFE_ABIL_COMMAND(abil));
				act(buf, FALSE, ch, NULL, cvict, TO_CHAR);
			}
		
			// to cvict
			if (abil_has_custom_message(abil, ABIL_CUSTOM_TARGETED_TO_VICT)) {
				act(abil_get_custom_message(abil, ABIL_CUSTOM_TARGETED_TO_VICT), invis, ch, NULL, cvict, TO_VICT);
			}
			else {
				snprintf(buf, sizeof(buf), "$n uses %s on you!", SAFE_ABIL_COMMAND(abil));
				act(buf, invis, ch, NULL, cvict, TO_VICT);
			}
		
			// to room
			if (abil_has_custom_message(abil, ABIL_CUSTOM_TARGETED_TO_ROOM)) {
				act(abil_get_custom_message(abil, ABIL_CUSTOM_TARGETED_TO_ROOM), invis, ch, NULL, cvict, TO_NOTVICT);
			}
			else {
				snprintf(buf, sizeof(buf), "$n uses %s on $N!", SAFE_ABIL_COMMAND(abil));
				act(buf, invis, ch, NULL, cvict, TO_NOTVICT);
			}
		}
	}
	
	// run the abilities
	for (iter = 0; do_ability_data[iter].type != NOBITS && !data->stop; ++iter) {
		if (IS_SET(ABIL_TYPES(abil), do_ability_data[iter].type) && do_ability_data[iter].do_func) {
			(do_ability_data[iter].do_func)(ch, abil, level, cvict, data);
		}
	}
	
	/* special handling for damage... integrate this into the for() loop above -- don't forget about exp tho
	if (IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE) && !data->stop) {
		if (mag_damage(level, ch, cvict, abil) == -1) {
			data->stop = TRUE;
			return;	// Successful and target died, don't cast again.
		}
	}
	*/
	
	if (data->success) {
		// experience
		if (!cvict || can_gain_exp_from(ch, cvict)) {
			gain_ability_exp(ch, ABIL_VNUM(abil), 15);
		}
		
		// check if should be in combat
		if (cvict && cvict != ch && !ABILITY_FLAGGED(abil, ABILF_NO_ENGAGE) && !EXTRACTED(cvict) && !IS_DEAD(cvict)) {
			// auto-assist if we used an ability on someone who is fighting
			if (!violent && FIGHTING(cvict) && !FIGHTING(ch)) {
				engage_combat(ch, FIGHTING(cvict), ABILITY_FLAGGED(abil, ABILF_RANGED | ABILF_RANGED_ONLY) ? FALSE : TRUE);
			}
			
			// auto-attack if used on an enemy
			if (violent && AWAKE(cvict) && !FIGHTING(cvict)) {
				engage_combat(cvict, ch, ABILITY_FLAGGED(abil, ABILF_RANGED | ABILF_RANGED_ONLY) ? FALSE : TRUE);
			}
		}
	}
	else if (!data->no_msg) {
		msg_to_char(ch, "It doesn't seem to have any effect.\r\n");
	}
}


/**
* do_ability is used generically to perform any ability, assuming we already
* have the target char/obj/veh and most things have been validated. This checks
* further restrictions.
*
* If an NPC uses an ability directly in the code, this is the correct entry
* point for it.
*
* @param char_data *ch The person performing the ability.
* @param ability_data *abil The ability being used.
* @param char *argument Any remaining argument.
* @param char_data *targ The char target, if any (may be NULL).
* @param obj_data *obj The obj target, if any (may be NULL).
* @param vehicle_data *veh The vehicle target, if any (may be NULL).
* @param struct ability_exec *data The execution data to pass back and forth.
*/
void do_ability(char_data *ch, ability_data *abil, char *argument, char_data *targ, obj_data *obj, vehicle_data *veh, struct ability_exec *data) {
	int level, cap;
	
	if (!ch || !abil) {
		log("SYSERR: do_ability called without %s.", ch ? "ability" : "character");
		data->stop = TRUE;
		return;
	}
	
	if (GET_POS(ch) < ABIL_MIN_POS(abil)) {
		send_low_pos_msg(ch);
		data->stop = TRUE;
		return;
	}
	if (targ && AFF_FLAGGED(ch, AFF_CHARM) && (ch->master == targ)) {
		msg_to_char(ch, "You are afraid you might hurt your master!\r\n");
		data->stop = TRUE;
		return;
	}
	if ((targ != ch) && IS_SET(ABIL_TARGETS(abil), ATAR_SELF_ONLY)) {
		msg_to_char(ch, "You can only use that on yourself!\r\n");
		data->stop = TRUE;
		return;
	}
	if ((targ == ch) && IS_SET(ABIL_TARGETS(abil), ATAR_NOT_SELF)) {
		msg_to_char(ch, "You cannot use that on yourself!\r\n");
		data->stop = TRUE;
		return;
	}
	/* TODO: if group abilities are added
	if (IS_SET(ABIL_TYPES(abil), ABILT_GROUPS) && !GROUP(ch)) {
		msg_to_char(ch, "You can't do that if you're not in a group!\r\n");
		data->stop = TRUE;
		return;
	}
	*/
	
	// determine correct level (sometimes limited by skill level)
	level = get_approximate_level(ch);
	if (!IS_NPC(ch) && ABIL_ASSIGNED_SKILL(abil) && (cap = get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil)))) < CLASS_SKILL_CAP) {
		level = MIN(level, cap);	// constrain by skill level
	}
	
	call_ability(ch, abil, argument, targ, obj, veh, level, RUN_ABIL_NORMAL, data);
}


/**
* All buff-type abilities come through here. This handles scaling and buff
* maintenance/replacement.
*
* DO_ABIL provides: ch, abil, level, vict, data
*/
DO_ABIL(do_buff_ability) {
	extern const bool apply_never_scales[];
	
	struct affected_type *af;
	struct apply_data *apply;
	any_vnum affect_vnum;
	double total_points, remaining_points, share, amt;
	int dur, total_w;
	bool messaged;
	
	affect_vnum = (ABIL_AFFECT_VNUM(abil) != NOTHING) ? ABIL_AFFECT_VNUM(abil) : ATYPE_BUFF;
	
	total_points = get_ability_type_data(data, ABILT_BUFF)->scale_points;
	remaining_points = total_points;
	
	if (total_points <= 0) {
		return;
	}
	
	if (ABIL_IMMUNITIES(abil) && AFF_FLAGGED(vict, ABIL_IMMUNITIES(abil))) {
		act("$N is immune!", FALSE, ch, NULL, vict, TO_CHAR);
		return;
	}
	
	// determine duration
	dur = IS_CLASS_ABILITY(ch, ABIL_VNUM(abil)) ? ABIL_LONG_DURATION(abil) : ABIL_SHORT_DURATION(abil);
	if (dur != UNLIMITED) {
		dur = (int) ceil((double)dur / SECS_PER_REAL_UPDATE);	// convert units
	}
	
	messaged = FALSE;	// to prevent duplicates
	
	// affect flags? cost == level 100 ability
	if (ABIL_AFFECTS(abil)) {
		remaining_points -= count_bits(ABIL_AFFECTS(abil)) * config_get_double("scale_points_at_100");
		remaining_points = MAX(0, remaining_points);
		
		af = create_flag_aff(affect_vnum, dur, ABIL_AFFECTS(abil), ch);
		affect_join(vict, af, messaged ? SILENT_AFF : NOBITS);
		messaged = TRUE;
	}
	
	// determine share for effects
	total_w = 0;
	LL_FOREACH(ABIL_APPLIES(abil), apply) {
		if (!apply_never_scales[apply->location]) {
			total_w += ABSOLUTE(apply->weight);
		}
	}
	
	// now create affects for each apply that we can afford
	LL_FOREACH(ABIL_APPLIES(abil), apply) {
		if (apply_never_scales[apply->location]) {
			af = create_mod_aff(affect_vnum, dur, apply->location, apply->weight, ch);
			affect_join(vict, af, messaged ? SILENT_AFF : NOBITS);
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
			affect_join(vict, af, messaged ? SILENT_AFF : NOBITS);
			messaged = TRUE;
		}
	}
	
	data->success = TRUE;
}


/**
* All damage abilities come through here.
*
* DO_ABIL provides: ch, abil, level, vict, data
*/
DO_ABIL(do_damage_ability) {
	struct ability_exec_type *subdata = get_ability_type_data(data, ABILT_DAMAGE);
	int result, dmg;
	
	dmg = subdata->scale_points * (data->matching_role ? 3 : 2);	// could go higher?
	
	// bonus damage if role matches
	if (data->matching_role) {
		switch (ABIL_DAMAGE_TYPE(abil)) {
			case DAM_PHYSICAL: {
				dmg += GET_BONUS_PHYSICAL(ch);
				break;
			}
			case DAM_MAGICAL: {
				dmg += GET_BONUS_MAGICAL(ch);
				break;
			}
		}
	}
	
	// msg_to_char(ch, "Damage: %d\r\n", dmg);
	result = damage(ch, vict, dmg, ABIL_ATTACK_TYPE(abil), ABIL_DAMAGE_TYPE(abil));
	data->success = TRUE;
	
	if (result < 0) {	// dedz
		data->stop = TRUE;
	}
}


/**
* All damage-over-time abilities come through here. This handles scaling and
* stacking.
*
* DO_ABIL provides: ch, abil, level, vict, data
*/
DO_ABIL(do_dot_ability) {
	any_vnum affect_vnum;
	double points;
	int dur, dmg;
	
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
	if (dur != UNLIMITED) {
		dur = (int) ceil((double)dur / SECS_PER_REAL_UPDATE);	// convert units
	}
	
	dmg = points * (data->matching_role ? 2 : 1);
	
	// bonus damage if role matches
	if (data->matching_role) {
		switch (ABIL_DAMAGE_TYPE(abil)) {
			case DAM_PHYSICAL: {
				dmg += GET_BONUS_PHYSICAL(ch) / MAX(1, dur);
				break;
			}
			case DAM_MAGICAL: {
				dmg += GET_BONUS_MAGICAL(ch) / MAX(1, dur);
				break;
			}
		}
	}

	dmg = MAX(1, dmg);
	apply_dot_effect(vict, affect_vnum, dur, ABIL_DAMAGE_TYPE(abil), dmg, ABIL_MAX_STACKS(abil), ch);
	data->success = TRUE;
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
	char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	struct ability_exec_type *aet, *next_aet;
	struct ability_exec *data;
	vehicle_data *veh = NULL;
	char_data *targ = NULL;
	obj_data *obj = NULL;
	bool has = FALSE;
	int iter;
	
	skip_spaces(&argument);
	
	// pre-validates JUST the ability -- individual types must validate cost/cooldown
	if (!can_use_ability(ch, ABIL_VNUM(abil), MOVE, 0, NOTHING)) {
		return;
	}
		
	// Find the target
	if (IS_SET(ABIL_TARGETS(abil), ATAR_IGNORE)) {
		has = TRUE;
	}
	else if (*argument) {
		argument = one_argument(argument, arg);
		skip_spaces(&argument);	// anything left
		
		// char targets
		if (!has && (IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_ROOM | ATAR_SELF_ONLY))) {
			if ((targ = get_char_vis(ch, arg, FIND_CHAR_ROOM)) != NULL) {
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_CLOSEST)) {
			if ((targ = find_closest_char(ch, arg, FALSE)) != NULL) {
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_WORLD)) {
			if ((targ = get_char_vis(ch, arg, FIND_CHAR_WORLD)) != NULL) {
				has = TRUE;
			}
		}
		
		// obj targets
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_INV)) {
			if ((obj = get_obj_in_list_vis(ch, arg, ch->carrying)) != NULL) {
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_EQUIP)) {
			for (iter = 0; !has && iter < NUM_WEARS; ++iter) {
				if (GET_EQ(ch, iter) && isname(arg, GET_EQ(ch, iter)->name)) {
					obj = GET_EQ(ch, iter);
					has = TRUE;
				}
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_ROOM)) {
			if ((obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL) {
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_OBJ_WORLD)) {
			if ((obj = get_obj_vis(ch, arg)) != NULL) {
				has = TRUE;
			}
		}
		
		// vehicle targets
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_VEH_ROOM)) {
			if ((veh = get_vehicle_in_room_vis(ch, arg))) {
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_VEH_WORLD)) {
			if ((veh = get_vehicle_vis(ch, arg))) {
				has = TRUE;
			}
		}
	}
	else {	// no arg
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_FIGHT_SELF)) {
			if (FIGHTING(ch) != NULL) {
				targ = ch;
				has = TRUE;
			}
		}
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_FIGHT_VICT)) {
			if (FIGHTING(ch) != NULL) {
				targ = FIGHTING(ch);
				has = TRUE;
			}
		}
		// if no target specified, and the spell isn't violent, default to self
		if (!has && IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_ROOM) && !ABILITY_FLAGGED(abil, ABILF_VIOLENT)) {
			targ = ch;
			has = TRUE;
		}
		if (!has) {
			snprintf(buf, sizeof(buf), "%s %s?\r\n", SAFE_ABIL_COMMAND(abil), IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_ROOM | ATAR_CHAR_CLOSEST | ATAR_CHAR_WORLD) ? "whom" : "what");
			msg_to_char(ch, "%s", CAP(buf));
			return;
		}
	}

	if (has && (targ == ch) && ABILITY_FLAGGED(abil, ABILF_VIOLENT)) {
		msg_to_char(ch, "You can't %s yourself -- that could be bad for your health!\r\n", SAFE_ABIL_COMMAND(abil));
		return;
	}
	if (!has) {
		if (IS_SET(ABIL_TARGETS(abil), ATAR_CHAR_ROOM | ATAR_CHAR_CLOSEST | ATAR_CHAR_WORLD)) {
			send_config_msg(ch, "no_person");
		}
		else {
			msg_to_char(ch, "There's nothing like that here.\r\n");
		}
		return;
	}
	
	// exec data to pass through
	CREATE(data, struct ability_exec, 1);
	data->cost = ABIL_COST(abil);	// base cost, may be modified
	
	// detect role
	if (!IS_NPC(ch) && ABILITY_FLAGGED(abil, ABILITY_ROLE_FLAGS)) {
		if (ABILITY_FLAGGED(abil, ABILF_CASTER_ROLE) && (GET_CLASS_ROLE(ch) == ROLE_CASTER || GET_CLASS_ROLE(ch) == ROLE_SOLO) && check_solo_role(ch)) {
			data->matching_role = TRUE;
		}
		else if (ABILITY_FLAGGED(abil, ABILF_HEALER_ROLE) && (GET_CLASS_ROLE(ch) == ROLE_HEALER || GET_CLASS_ROLE(ch) == ROLE_SOLO) && check_solo_role(ch)) {
			data->matching_role = TRUE;
		}
		else if (ABILITY_FLAGGED(abil, ABILF_MELEE_ROLE) && (GET_CLASS_ROLE(ch) == ROLE_MELEE || GET_CLASS_ROLE(ch) == ROLE_SOLO) && check_solo_role(ch)) {
			data->matching_role = TRUE;
		}
		else if (ABILITY_FLAGGED(abil, ABILF_TANK_ROLE) && (GET_CLASS_ROLE(ch) == ROLE_TANK || GET_CLASS_ROLE(ch) == ROLE_SOLO) && check_solo_role(ch)) {
			data->matching_role = TRUE;
		}
		else {
			data->matching_role = FALSE;	// does not match
		}
	}
	else {
		data->matching_role = TRUE;	// by default
	}
	
	// run the ability
	do_ability(ch, abil, argument, targ, obj, veh, data);
	
	if (data->success) {
		charge_ability_cost(ch, ABIL_COST_TYPE(abil), data->cost, ABIL_COOLDOWN(abil), ABIL_COOLDOWN_SECS(abil), ABIL_WAIT_TYPE(abil));
	}
	
	// clean up data
	LL_FOREACH_SAFE(data->types, aet, next_aet) {
		free(aet);
	}
	free(data);
}


/**
* This function 'stops' if the ability is a toggle and you're toggling it off,
* which keeps it from charging/cooldowning.
* PREP_ABIL provides: ch, abil, level, vict, data
*/
PREP_ABIL(prep_buff_ability) {
	any_vnum affect_vnum;
	
	affect_vnum = (ABIL_AFFECT_VNUM(abil) != NOTHING) ? ABIL_AFFECT_VNUM(abil) : ATYPE_BUFF;
	
	// toggle off?
	if (ABILITY_FLAGGED(abil, ABILF_TOGGLE) && vict == ch && affected_by_spell_from_caster(vict, affect_vnum, ch)) {
		send_config_msg(ch, "ok_string");
		affect_from_char_by_caster(vict, affect_vnum, ch, TRUE);
		data->stop = TRUE;
		
		command_lag(ch, ABIL_WAIT_TYPE(abil));
		return;	// prevent charging for the ability or adding a cooldown by not setting success
	}
	
	get_ability_type_data(data, ABILT_BUFF)->scale_points = standard_ability_scale(ch, abil, level, ABILT_BUFF, data);
}


/**
* PREP_ABIL provides: ch, abil, level, vict, data
*/
PREP_ABIL(prep_damage_ability) {
	get_ability_type_data(data, ABILT_DAMAGE)->scale_points = standard_ability_scale(ch, abil, level, ABILT_DAMAGE, data);
}


/**
* PREP_ABIL provides: ch, abil, level, vict, data
*/
PREP_ABIL(prep_dot_ability) {
	get_ability_type_data(data, ABILT_DOT)->scale_points = standard_ability_scale(ch, abil, level, ABILT_DOT, data);
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
	ability_data *iter, *next_iter;
	bool problem = FALSE;
	
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
	
	// other abils
	HASH_ITER(hh, ability_table, iter, next_iter) {
		if (iter != abil && ABIL_VNUM(iter) != ABIL_VNUM(abil) && !str_cmp(ABIL_NAME(iter), ABIL_NAME(abil))) {
			olc_audit_msg(ch, ABIL_VNUM(abil), "Same name as ability %d", ABIL_VNUM(iter));
			problem = TRUE;
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
	extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);
	
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
		if (ABIL_MASTERY_ABIL(abiter) == vnum) {
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
	
	if (ABIL_NAME(abil) && (!proto || ABIL_NAME(abil) != ABIL_NAME(proto))) {
		free(ABIL_NAME(abil));
	}
	if (ABIL_COMMAND(abil) && (!proto || ABIL_COMMAND(abil) != ABIL_COMMAND(proto))) {
		free(ABIL_COMMAND(abil));
	}
	if (ABIL_CUSTOM_MSGS(abil) && (!proto || ABIL_CUSTOM_MSGS(abil) != ABIL_CUSTOM_MSGS(proto))) {
		free_custom_messages(ABIL_CUSTOM_MSGS(abil));
	}
	if (ABIL_DATA(abil) && (!proto || ABIL_DATA(abil) != ABIL_DATA(proto))) {
		while ((adl = ABIL_DATA(abil))) {
			ABIL_DATA(abil) = adl->next;
			free(adl);
		}
	}
	
	free(abil);
}


/**
* Read one ability from file.
*
* @param FILE *fl The open .abil file
* @param any_vnum vnum The ability vnum
*/
void parse_ability(FILE *fl, any_vnum vnum) {
	void parse_apply(FILE *fl, struct apply_data **list, char *error_str);
	void parse_custom_message(FILE *fl, struct custom_message **list, char *error);
	
	char line[256], error[256], str_in[256], str_in2[256], str_in3[256];
	struct ability_data_list *adl;
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
	
	ABIL_FLAGS(abil) = asciiflag_conv(str_in);
	ABIL_MASTERY_ABIL(abil) = int_in[0];
	ABIL_SCALE(abil) = dbl_in;
	ABIL_IMMUNITIES(abil) = asciiflag_conv(str_in2);
	ABIL_GAIN_HOOKS(abil) = asciiflag_conv(str_in3);
	
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
				ABIL_COMMAND(abil) = *str_in ? str_dup(str_in) : NULL;
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
			
			case 'M': {	// custom messages
				parse_custom_message(fl, &ABIL_CUSTOM_MSGS(abil), error);
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
	void write_applies_to_file(FILE *fl, struct apply_data *list);
	void write_custom_messages_to_file(FILE *fl, char letter, struct custom_message *list);
	
	char temp[256], temp2[256], temp3[256];
	struct ability_data_list *adl;
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
	fprintf(fl, "%s %d %.2f %s %s\n", temp, ABIL_MASTERY_ABIL(abil), ABIL_SCALE(abil), temp2, temp3);
	
	// 'A': applies
	write_applies_to_file(fl, ABIL_APPLIES(abil));
	
	// 'C' command
	if (ABIL_COMMAND(abil) || ABIL_TARGETS(abil) || ABIL_COST(abil) || ABIL_COST_PER_SCALE_POINT(abil) || ABIL_COOLDOWN(abil) != NOTHING || ABIL_COOLDOWN_SECS(abil) || ABIL_WAIT_TYPE(abil) || ABIL_LINKED_TRAIT(abil) || ABIL_DIFFICULTY(abil)) {
		fprintf(fl, "C\n%s %d %s %d %d %d %d %d %d %d %d\n", ABIL_COMMAND(abil) ? ABIL_COMMAND(abil) : "unknown", ABIL_MIN_POS(abil), bitv_to_alpha(ABIL_TARGETS(abil)), ABIL_COST_TYPE(abil), ABIL_COST(abil), ABIL_COST_PER_SCALE_POINT(abil), ABIL_COOLDOWN(abil), ABIL_COOLDOWN_SECS(abil), ABIL_LINKED_TRAIT(abil), ABIL_WAIT_TYPE(abil), ABIL_DIFFICULTY(abil));
	}
	
	// 'D' data
	LL_FOREACH(ABIL_DATA(abil), adl) {
		fprintf(fl, "D %d %d %d\n", adl->type, adl->vnum, adl->misc);
	}
	
	// M: custom message
	write_custom_messages_to_file(fl, 'M', ABIL_CUSTOM_MSGS(abil));
	
	// 'T' types
	LL_FOREACH(ABIL_TYPE_LIST(abil), at) {
		fprintf(fl, "T %s %d\n", bitv_to_alpha(at->type), at->weight);
	}
	
	// 'X' type data
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
	extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
	extern bool remove_vnum_from_class_abilities(struct class_ability **list, any_vnum vnum);
	extern bool remove_vnum_from_skill_abilities(struct skill_ability **list, any_vnum vnum);
	extern bool remove_ability_from_synergy_abilities(struct synergy_ability **list, any_vnum abil_vnum);
	
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
	bool found;
	
	if (!(abil = find_ability_by_vnum(vnum))) {
		msg_to_char(ch, "There is no such ability %d.\r\n", vnum);
		return;
	}
		
	// remove it from the hash table first
	remove_ability_from_table(abil);
	
	// update other abilities
	HASH_ITER(hh, ability_table, abiter, next_abiter) {
		if (ABIL_MASTERY_ABIL(abiter) == vnum) {
			ABIL_MASTERY_ABIL(abiter) = NOTHING;
			save_library_file_for_vnum(DB_BOOT_ABIL, ABIL_VNUM(abiter));
		}
	}
	
	// update augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		if (GET_AUG_ABILITY(aug) == vnum) {
			GET_AUG_ABILITY(aug) = NOTHING;
			SET_BIT(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_AUG, GET_AUG_VNUM(aug));
		}
	}
	
	// update classes
	HASH_ITER(hh, class_table, cls, next_cls) {
		found = remove_vnum_from_class_abilities(&CLASS_ABILITIES(cls), vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_CLASS, CLASS_VNUM(cls));
		}
	}
	
	// update crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		if (GET_CRAFT_ABILITY(craft) == vnum) {
			GET_CRAFT_ABILITY(craft) = NOTHING;
			SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// update globals
	HASH_ITER(hh, globals_table, glb, next_glb) {
		if (GET_GLOBAL_ABILITY(glb) == vnum) {
			GET_GLOBAL_ABILITY(glb) = NOTHING;
			SET_BIT(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_GLB, GET_GLOBAL_VNUM(glb));
		}
	}
	
	// update morphs
	HASH_ITER(hh, morph_table, morph, next_morph) {
		if (MORPH_ABILITY(morph) == vnum) {
			MORPH_ABILITY(morph) = NOTHING;
			SET_BIT(MORPH_FLAGS(morph), MORPHF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_MORPH, MORPH_VNUM(morph));
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_HAVE_ABILITY, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
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
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// update skills
	HASH_ITER(hh, skill_table, skill, next_skill) {
		found = remove_vnum_from_skill_abilities(&SKILL_ABILITIES(skill), vnum);
		found |= remove_ability_from_synergy_abilities(&SKILL_SYNERGIES(skill), vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_SKILL, SKILL_VNUM(skill));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_HAVE_ABILITY, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// update live players
	LL_FOREACH(character_list, chiter) {
		found = FALSE;
		if (IS_NPC(chiter)) {
			continue;
		}
		
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
			if (ABIL_MASTERY_ABIL(GET_OLC_ABILITY(desc)) == vnum) {
				ABIL_MASTERY_ABIL(GET_OLC_ABILITY(desc)) = NOTHING;
				msg_to_desc(desc, "The mastery ability has been deleted from the ability you're editing.\r\n");
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
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted ability %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Ability %d deleted.\r\n", vnum);
	
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
	
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	bitvector_t find_applies = NOBITS, found_applies, not_flagged = NOBITS, only_flags = NOBITS;
	bitvector_t only_affs = NOBITS, only_immunities = NOBITS, only_gains = NOBITS, only_targets = NOBITS, find_custom = NOBITS, found_custom;
	int count, only_cost_type = NOTHING, only_type = NOTHING, only_scale = NOTHING, scale_over = NOTHING, scale_under = NOTHING, min_pos = POS_DEAD, max_pos = POS_STANDING;
	int min_cost = NOTHING, max_cost = NOTHING, min_cost_per = NOTHING, max_cost_per = NOTHING, min_cd = NOTHING, max_cd = NOTHING, min_dur = FAKE_DUR, max_dur = FAKE_DUR;
	int only_wait = NOTHING, only_linked = NOTHING, only_diff = NOTHING, only_attack = NOTHING, only_damage = NOTHING, only_ptech = NOTHING;
	struct ability_data_list *data;
	ability_data *abil, *next_abil;
	struct custom_message *cust;
	struct apply_data *app;
	size_t size;
	bool found;
	
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
		
		FULLSEARCH_FLAGS("affects", only_affs, affected_bits)
		FULLSEARCH_FLAGS("apply", find_applies, apply_types)
		FULLSEARCH_FLAGS("applies", find_applies, apply_types)
		FULLSEARCH_FUNC("attacktype", only_attack, get_attack_type_by_name(val_arg))
		FULLSEARCH_LIST("costtype", only_cost_type, pool_types)
		FULLSEARCH_FLAGS("custom", find_custom, ability_custom_types)
		FULLSEARCH_LIST("damagetype", only_damage, damage_types)
		FULLSEARCH_LIST("difficulty", only_diff, skill_check_difficulty)
		FULLSEARCH_FLAGS("flags", only_flags, ability_flags)
		FULLSEARCH_FLAGS("flagged", only_flags, ability_flags)
		FULLSEARCH_FLAGS("gains", only_gains, ability_gain_hooks)
		FULLSEARCH_FLAGS("gainhooks", only_gains, ability_gain_hooks)
		FULLSEARCH_FLAGS("immunities", only_immunities, affected_bits)
		FULLSEARCH_FLAGS("immunity", only_immunities, affected_bits)
		FULLSEARCH_FLAGS("immune", only_immunities, affected_bits)
		FULLSEARCH_LIST("linkedtrait", only_linked, apply_types)
		FULLSEARCH_INT("maxcooldowntime", max_cd, 0, INT_MAX)
		FULLSEARCH_INT("mincooldowntime", min_cd, 0, INT_MAX)
		FULLSEARCH_INT("maxcost", max_cost, 0, INT_MAX)
		FULLSEARCH_INT("mincost", min_cost, 0, INT_MAX)
		FULLSEARCH_INT("maxcostperscalepoint", max_cost_per, 0, INT_MAX)
		FULLSEARCH_INT("mincostperscalepoint", min_cost_per, 0, INT_MAX)
		FULLSEARCH_LIST("maxposition", max_pos, position_types)
		FULLSEARCH_LIST("minposition", min_pos, position_types)
		FULLSEARCH_LIST("ptech", only_ptech, player_tech_types)
		FULLSEARCH_INT("scale", only_scale, 0, INT_MAX)
		FULLSEARCH_INT("scaleover", scale_over, 0, INT_MAX)
		FULLSEARCH_INT("scaleunder", scale_under, 0, INT_MAX)
		FULLSEARCH_FLAGS("targets", only_targets, ability_target_flags)
		FULLSEARCH_LIST("type", only_type, ability_type_flags)
		FULLSEARCH_FLAGS("unflagged", not_flagged, ability_flags)
		FULLSEARCH_LIST("waittype", only_wait, wait_types)
		
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
	
	size = snprintf(buf, sizeof(buf), "Ability fullsearch: %s\r\n", find_keywords);
	count = 0;
	
	// okay now look up items
	HASH_ITER(hh, ability_table, abil, next_abil) {
		if (ABIL_MIN_POS(abil) < min_pos || ABIL_MIN_POS(abil) > max_pos) {
			continue;
		}
		if (only_cost_type != NOTHING && ABIL_COST_TYPE(abil) != only_cost_type) {
			continue;
		}
		if (only_attack != NOTHING && ABIL_ATTACK_TYPE(abil) != only_attack) {
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
		if (only_affs != NOBITS && (ABIL_AFFECTS(abil) & only_affs) != only_affs) {
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
		if (only_ptech != NOTHING) {
			found = FALSE;
			LL_FOREACH(ABIL_DATA(abil), data) {
				if (data->type == ADL_PLAYER_TECH && data->vnum == only_ptech) {
					found = TRUE;
				}
			}
			if (!found) {
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
	
	if (count > 0 && (size + 14) < sizeof(buf)) {
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
	LL_FOREACH(character_list, chiter) {
		if (!IS_NPC(chiter) && (abd = get_ability_data(chiter, vnum, FALSE))) {
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
	free_custom_messages(ABIL_CUSTOM_MSGS(proto));
	free_apply_list(ABIL_APPLIES(proto));
	
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
}


/**
* Creates a copy of an ability, or clears a new one, for editing.
* 
* @param ability_data *input The ability to copy, or NULL to make a new one.
* @return ability_data* The copied ability.
*/
ability_data *setup_olc_ability(ability_data *input) {
	extern struct apply_data *copy_apply_list(struct apply_data *input);
	
	ability_data *new;
	
	CREATE(new, ability_data, 1);
	clear_ability(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		ABIL_NAME(new) = ABIL_NAME(input) ? str_dup(ABIL_NAME(input)) : NULL;
		ABIL_COMMAND(new) = ABIL_COMMAND(input) ? str_dup(ABIL_COMMAND(input)) : NULL;
		ABIL_CUSTOM_MSGS(new) = copy_custom_messages(ABIL_CUSTOM_MSGS(input));
		ABIL_APPLIES(new) = copy_apply_list(ABIL_APPLIES(input));
		ABIL_DATA(new) = copy_data_list(ABIL_DATA(input));
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
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH], part2[MAX_STRING_LENGTH];
	struct ability_data_list *adl;
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
	
	get_ability_type_display(ABIL_TYPE_LIST(abil), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Types: \tc%s\t0\r\n", part);
	
	sprintbit(ABIL_FLAGS(abil), ability_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	sprintbit(ABIL_IMMUNITIES(abil), affected_bits, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Immunities: \tc%s\t0\r\n", part);
	
	sprintbit(ABIL_GAIN_HOOKS(abil), ability_gain_hooks, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Gain hooks: \tg%s\t0\r\n", part);
	
	// command-related portion
	if (!ABIL_COMMAND(abil)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Command info: [\tcnot a command\t0]\r\n");
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, "Command info: [\ty%s\t0], Min position: [\tc%s\t0]\r\n", ABIL_COMMAND(abil), position_types[ABIL_MIN_POS(abil)]);
		
		sprintbit(ABIL_TARGETS(abil), ability_target_flags, part, TRUE);
		size += snprintf(buf + size, sizeof(buf) - size, "Targets: \tg%s\t0\r\n", part);
		size += snprintf(buf + size, sizeof(buf) - size, "Cost: [\tc%d %s (+%d/scale)\t0], Cooldown: [\tc%d %s\t0], Cooldown time: [\tc%d second%s\t0]\r\n", ABIL_COST(abil), pool_types[ABIL_COST_TYPE(abil)], ABIL_COST_PER_SCALE_POINT(abil), ABIL_COOLDOWN(abil), get_generic_name_by_vnum(ABIL_COOLDOWN(abil)),  ABIL_COOLDOWN_SECS(abil), PLURAL(ABIL_COOLDOWN_SECS(abil)));
		size += snprintf(buf + size, sizeof(buf) - size, "Wait type: [\ty%s\t0], Linked trait: [\ty%s\t0]\r\n", wait_types[ABIL_WAIT_TYPE(abil)], apply_types[ABIL_LINKED_TRAIT(abil)]);
		size += snprintf(buf + size, sizeof(buf) - size, "Difficulty: \ty%s\t0\r\n", skill_check_difficulty[ABIL_DIFFICULTY(abil)]);
		
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
			
			size += snprintf(buf + size, sizeof(buf) - size, "Custom affect: [\ty%d %s\t0]\r\n", ABIL_AFFECT_VNUM(abil), get_generic_name_by_vnum(ABIL_AFFECT_VNUM(abil)));
		}	// end buff/dot
		if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF)) {
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
	}
	
	if (ABIL_CUSTOM_MSGS(abil)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Custom messages:\r\n");
		
		LL_FOREACH(ABIL_CUSTOM_MSGS(abil), custm) {
			size += snprintf(buf + size, sizeof(buf) - size, " %s: %s\r\n", ability_custom_types[custm->type], custm->msg);
		}
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
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	struct ability_data_list *adl;
	struct custom_message *custm;
	struct apply_data *apply;
	int count;
	
	if (!abil) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !find_ability_by_vnum(ABIL_VNUM(abil)) ? "new ability" : get_ability_name_by_vnum(ABIL_VNUM(abil)));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(ABIL_NAME(abil), default_ability_name), NULLSAFE(ABIL_NAME(abil)));
	
	get_ability_type_display(ABIL_TYPE_LIST(abil), lbuf);
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
	if (!ABIL_COMMAND(abil)) {
		sprintf(buf + strlen(buf), "<%scommand\t0> (not a command)\r\n", OLC_LABEL_UNCHANGED);
	}
	else {
		sprintf(buf + strlen(buf), "<%scommand\t0> %s, <%sminposition\t0> %s (minimum)\r\n", OLC_LABEL_CHANGED, ABIL_COMMAND(abil), OLC_LABEL_VAL(ABIL_MIN_POS(abil), POS_DEAD), position_types[ABIL_MIN_POS(abil)]);
		sprintbit(ABIL_TARGETS(abil), ability_target_flags, lbuf, TRUE);
		sprintf(buf + strlen(buf), "<%stargets\t0> %s\r\n", OLC_LABEL_VAL(ABIL_TARGETS(abil), NOBITS), lbuf);
		sprintf(buf + strlen(buf), "<%scost\t0> %d, <%scostperscalepoint\t0> %d, <%scosttype\t0> %s\r\n", OLC_LABEL_VAL(ABIL_COST(abil), 0), ABIL_COST(abil), OLC_LABEL_VAL(ABIL_COST_PER_SCALE_POINT(abil), 0), ABIL_COST_PER_SCALE_POINT(abil), OLC_LABEL_VAL(ABIL_COST_TYPE(abil), 0), pool_types[ABIL_COST_TYPE(abil)]);
		sprintf(buf + strlen(buf), "<%scooldown\t0> [%d] %s, <%scdtime\t0> %d second%s\r\n", OLC_LABEL_VAL(ABIL_COOLDOWN(abil), NOTHING), ABIL_COOLDOWN(abil), get_generic_name_by_vnum(ABIL_COOLDOWN(abil)), OLC_LABEL_VAL(ABIL_COOLDOWN_SECS(abil), 0), ABIL_COOLDOWN_SECS(abil), PLURAL(ABIL_COOLDOWN_SECS(abil)));
		sprintf(buf + strlen(buf), "<%swaittype\t0> %s, <%slinkedtrait\t0> %s\r\n", OLC_LABEL_VAL(ABIL_WAIT_TYPE(abil), WAIT_NONE), wait_types[ABIL_WAIT_TYPE(abil)], OLC_LABEL_VAL(ABIL_LINKED_TRAIT(abil), APPLY_NONE), apply_types[ABIL_LINKED_TRAIT(abil)]);
		sprintf(buf + strlen(buf), "<%sdifficulty\t0> %s\r\n", OLC_LABEL_VAL(ABIL_DIFFICULTY(abil), 0), skill_check_difficulty[ABIL_DIFFICULTY(abil)]);
		
		// type-specific data
		if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_DOT)) {
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
		if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF)) {
			sprintbit(ABIL_AFFECTS(abil), affected_bits, lbuf, TRUE);
			sprintf(buf + strlen(buf), "<%saffects\t0> %s\r\n", OLC_LABEL_VAL(ABIL_AFFECTS(abil), NOBITS), lbuf);
			
			sprintf(buf + strlen(buf), "Applies: <%sapply\t0>\r\n", OLC_LABEL_PTR(ABIL_APPLIES(abil)));
			count = 0;
			LL_FOREACH(ABIL_APPLIES(abil), apply) {
				sprintf(buf + strlen(buf), " %2d. %d to %s\r\n", ++count, apply->weight, apply_types[apply->location]);
			}
		}	// end buff
		if (IS_SET(ABIL_TYPES(abil), ABILT_BUFF | ABILT_DOT)) {
			sprintf(buf + strlen(buf), "<%saffectvnum\t0> %d %s\r\n", OLC_LABEL_VAL(ABIL_AFFECT_VNUM(abil), NOTHING), ABIL_AFFECT_VNUM(abil), get_generic_name_by_vnum(ABIL_AFFECT_VNUM(abil)));
		}	// end buff/dot
		if (IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE)) {
			sprintf(buf + strlen(buf), "<%sattacktype\t0> %d\r\n", OLC_LABEL_VAL(ABIL_ATTACK_TYPE(abil), 0), ABIL_ATTACK_TYPE(abil));
		}	// end damage
		if (IS_SET(ABIL_TYPES(abil), ABILT_DAMAGE | ABILT_DOT)) {
			sprintf(buf + strlen(buf), "<%sdamagetype\t0> %s\r\n", OLC_LABEL_VAL(ABIL_DAMAGE_TYPE(abil), 0), damage_types[ABIL_DAMAGE_TYPE(abil)]);
		}	// end damage/dot
		if (IS_SET(ABIL_TYPES(abil), ABILT_DOT)) {
			sprintf(buf + strlen(buf), "<%smaxstacks\t0> %d\r\n", OLC_LABEL_VAL(ABIL_MAX_STACKS(abil), 1), ABIL_MAX_STACKS(abil));
		}	// end dot
	}
	
	// custom messages
	sprintf(buf + strlen(buf), "Custom messages: <%scustom\t0>\r\n", OLC_LABEL_PTR(ABIL_CUSTOM_MSGS(abil)));
	count = 0;
	LL_FOREACH(ABIL_CUSTOM_MSGS(abil), custm) {
		sprintf(buf + strlen(buf), " \ty%d\t0. [%s] %s\r\n", ++count, ability_custom_types[custm->type], custm->msg);
	}
	
	// data
	sprintf(buf + strlen(buf), "Extra data: <%sdata\t0>\r\n", OLC_LABEL_PTR(ABIL_DATA(abil)));
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
	
	bitvector_t allowed_types = ABILT_BUFF;
	
	if (!ABIL_COMMAND(abil) || !IS_SET(ABIL_TYPES(abil), allowed_types)) {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
	else {
		ABIL_AFFECTS(abil) = olc_process_flag(ch, argument, "affects", "affects", affected_bits, ABIL_AFFECTS(abil));
	}
}


OLC_MODULE(abiledit_affectvnum) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	any_vnum old;
	
	bitvector_t allowed_types = ABILT_BUFF | ABILT_DOT;
	
	if (!ABIL_COMMAND(abil) || !IS_SET(ABIL_TYPES(abil), allowed_types)) {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		ABIL_AFFECT_VNUM(abil) = NOTHING;
		msg_to_char(ch, "It no longer has a custom affect type.\r\n");
	}
	else {
		old = ABIL_AFFECT_VNUM(abil);
		ABIL_AFFECT_VNUM(abil) = olc_process_number(ch, argument, "affect vnum", "affectvnum", 0, MAX_VNUM, ABIL_AFFECT_VNUM(abil));

		if (!find_generic(ABIL_AFFECT_VNUM(abil), GENERIC_AFFECT)) {
			msg_to_char(ch, "Invalid affect generic vnum %d. Old value restored.\r\n", ABIL_AFFECT_VNUM(abil));
			ABIL_AFFECT_VNUM(abil) = old;
		}
	}
}


OLC_MODULE(abiledit_apply) {
	void olc_process_applies(char_data *ch, char *argument, struct apply_data **list);
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	olc_process_applies(ch, argument, &ABIL_APPLIES(abil));
}


OLC_MODULE(abiledit_attacktype) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	bitvector_t allowed_types = ABILT_DAMAGE;
	
	if (!ABIL_COMMAND(abil) || !IS_SET(ABIL_TYPES(abil), allowed_types)) {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
	else {
		ABIL_ATTACK_TYPE(abil) = olc_process_number(ch, argument, "attack type", "attacktype", 0, MAX_VNUM, ABIL_ATTACK_TYPE(abil));
	}
}


OLC_MODULE(abiledit_cdtime) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	if (!ABIL_COMMAND(abil)) {
		msg_to_char(ch, "Only command abilities have this property.\r\n");
	}
	else if (ABIL_COOLDOWN(abil) == NOTHING) {
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
		
		// clear other data
		ABIL_TARGETS(abil) = NOBITS;
		
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
	}
}


OLC_MODULE(abiledit_cooldown) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	any_vnum old;
	
	if (!ABIL_COMMAND(abil)) {
		msg_to_char(ch, "Only command abilities have this property.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		ABIL_COOLDOWN(abil) = NOTHING;
		ABIL_COOLDOWN_SECS(abil) = 0;
		msg_to_char(ch, "It no longer has a cooldown.\r\n");
	}
	else {
		old = ABIL_COOLDOWN(abil);
		ABIL_COOLDOWN(abil) = olc_process_number(ch, argument, "cooldown vnum", "cooldown", 0, MAX_VNUM, ABIL_COOLDOWN(abil));

		if (!find_generic(ABIL_COOLDOWN(abil), GENERIC_COOLDOWN)) {
			msg_to_char(ch, "Invalid cooldown generic vnum %d. Old value restored.\r\n", ABIL_COOLDOWN(abil));
			ABIL_COOLDOWN(abil) = old;
		}
	}
}


OLC_MODULE(abiledit_cost) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	if (!ABIL_COMMAND(abil)) {
		msg_to_char(ch, "Only command abilities have this property.\r\n");
	}
	else {
		ABIL_COST(abil) = olc_process_number(ch, argument, "cost", "cost", 0, INT_MAX, ABIL_COST(abil));
	}
}


OLC_MODULE(abiledit_costperscalepoint) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	if (!ABIL_COMMAND(abil)) {
		msg_to_char(ch, "Only command abilities have this property.\r\n");
	}
	else {
		ABIL_COST_PER_SCALE_POINT(abil) = olc_process_number(ch, argument, "cost per scale point", "costperscalepoint", 0, INT_MAX, ABIL_COST_PER_SCALE_POINT(abil));
	}
}


OLC_MODULE(abiledit_costtype) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	if (!ABIL_COMMAND(abil)) {
		msg_to_char(ch, "Only command abilities have this property.\r\n");
	}
	else {
		ABIL_COST_TYPE(abil) = olc_process_type(ch, argument, "cost type", "costtype", pool_types, ABIL_COST_TYPE(abil));
	}
}


OLC_MODULE(abiledit_custom) {
	void olc_process_custom_messages(char_data *ch, char *argument, struct custom_message **list, const char **type_names);
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	olc_process_custom_messages(ch, argument, &ABIL_CUSTOM_MSGS(abil), ability_custom_types);
}


OLC_MODULE(abiledit_damagetype) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	bitvector_t allowed_types = ABILT_DAMAGE | ABILT_DOT;
	
	if (!ABIL_COMMAND(abil) || !IS_SET(ABIL_TYPES(abil), allowed_types)) {
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
	struct ability_data_list *adl, *next_adl;
	bitvector_t allowed_types = 0;
	int iter, num, type_id, val_id;
	bool found;
	
	// determine valid types first
	allowed_types |= ADL_EFFECT;
	if (IS_SET(ABIL_TYPES(abil), ABILT_PLAYER_TECH)) {
		allowed_types |= ADL_PLAYER_TECH;
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
					if ((val_id = search_block(val_arg, ability_effects, FALSE)) == NOTHING) {
						msg_to_char(ch, "Invalid ability effect '%s'.\r\n", val_arg);
						return;
					}
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
	
	if (!ABIL_COMMAND(abil)) {
		msg_to_char(ch, "Only command abilities have this property.\r\n");
	}
	else {
		ABIL_DIFFICULTY(abil) = olc_process_type(ch, argument, "difficulty", "difficulty", skill_check_difficulty, ABIL_DIFFICULTY(abil));
	}
}


OLC_MODULE(abiledit_gainhooks) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);	
	ABIL_GAIN_HOOKS(abil) = olc_process_flag(ch, argument, "gain hook", "gainhooks", ability_gain_hooks, ABIL_GAIN_HOOKS(abil));
}


OLC_MODULE(abiledit_flags) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);	
	ABIL_FLAGS(abil) = olc_process_flag(ch, argument, "ability", "flags", ability_flags, ABIL_FLAGS(abil));
}


OLC_MODULE(abiledit_immunities) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	ABIL_IMMUNITIES(abil) = olc_process_flag(ch, argument, "immunity", "immunities", affected_bits, ABIL_IMMUNITIES(abil));
}


OLC_MODULE(abiledit_linkedtrait) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	if (!ABIL_COMMAND(abil)) {
		msg_to_char(ch, "Only command abilities have this property.\r\n");
	}
	else {
		ABIL_LINKED_TRAIT(abil) = olc_process_type(ch, argument, "linked trait", "linkedtrait", apply_types, ABIL_LINKED_TRAIT(abil));
	}
}


OLC_MODULE(abiledit_longduration) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	bitvector_t allowed_types = ABILT_BUFF | ABILT_DOT;
	
	if (!ABIL_COMMAND(abil) || !IS_SET(ABIL_TYPES(abil), allowed_types)) {
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
	
	if (!ABIL_COMMAND(abil) || !IS_SET(ABIL_TYPES(abil), allowed_types)) {
		msg_to_char(ch, "This type of ability does not have this property.\r\n");
	}
	else {
		ABIL_MAX_STACKS(abil) = olc_process_number(ch, argument, "max stacks", "maxstacks", 1, 1000, ABIL_MAX_STACKS(abil));
	}
}


OLC_MODULE(abiledit_minposition) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	if (!ABIL_COMMAND(abil)) {
		msg_to_char(ch, "Only command abilities have this property.\r\n");
	}
	else {
		ABIL_MIN_POS(abil) = olc_process_type(ch, argument, "position", "minposition", position_types, ABIL_MIN_POS(abil));
	}
}


OLC_MODULE(abiledit_name) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	olc_process_string(ch, argument, "name", &ABIL_NAME(abil));
}


OLC_MODULE(abiledit_scale) {
	int scale;
	
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	scale = olc_process_number(ch, argument, "scale", "scale", 1, 1000, ABIL_SCALE(abil) * 100);
	ABIL_SCALE(abil) = scale / 100.0;
}


OLC_MODULE(abiledit_shortduration) {
	ability_data *abil = GET_OLC_ABILITY(ch->desc);
	
	bitvector_t allowed_types = ABILT_BUFF | ABILT_DOT;
	
	if (!ABIL_COMMAND(abil) || !IS_SET(ABIL_TYPES(abil), allowed_types)) {
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
	
	if (!ABIL_COMMAND(abil)) {
		msg_to_char(ch, "Only command abilities have this property.\r\n");
	}
	else {
		ABIL_TARGETS(abil) = olc_process_flag(ch, argument, "target", "targets", ability_target_flags, ABIL_TARGETS(abil));
	}
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
					msg_to_char(ch, "You remove %s.\r\n", buf);
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
	
	if (!ABIL_COMMAND(abil)) {
		msg_to_char(ch, "Only command abilities have this property.\r\n");
	}
	else {
		ABIL_WAIT_TYPE(abil) = olc_process_type(ch, argument, "wait type", "waittype", wait_types, ABIL_WAIT_TYPE(abil));
	}
}
