/* ************************************************************************
*   File: generic.c                                       EmpireMUD 2.0b5 *
*  Usage: loading, saving, OLC, and functions for generics                *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

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
*   Helpers
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   Generic OLC Modules
*   Action OLC Modules
*   Currency OLC Modules
*   Cooldown/Affect OLC Modules
*   Component OLC Modules
*   Liquid OLC Modules
*/

// local data
const char *default_generic_name = "Unnamed Generic";
#define MAX_LIQUID_COND  (MAX_CONDITION / REAL_UPDATES_PER_MUD_HOUR)	// approximate game hours of max cond

// local funcs
struct generic_relation *copy_generic_relations(struct generic_relation *list);
void free_generic_relations(struct generic_relation **list);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Adds a generic relationship (if it is not already present).
*
* @param struct generic_relation **list A pointer to the list to add to.
* @param any_vnum vnum The other generic's vnum, to add as a relation.
*/
void add_generic_relation(struct generic_relation **list, any_vnum vnum) {
	struct generic_relation *item;
	
	if (list && vnum != NOTHING) {
		HASH_FIND_INT(*list, &vnum, item);
		
		if (!item) {
			CREATE(item, struct generic_relation, 1);
			item->vnum = vnum;
			HASH_ADD_INT(*list, vnum, item);
		}
	}
}


/**
* Called at startup or after an OLC save to re-compute the full list of
* generic relations, to save time later.
*/
void compute_generic_relations(void) {
	struct generic_relation *rel, *next_rel, *alt, *next_alt;
	generic_data *gen, *next_gen, *other;
	bool changed;
	int safety;
	
	// first clear them all out
	HASH_ITER(hh, generic_table, gen, next_gen) {
		free_generic_relations(&GEN_COMPUTED_RELATIONS(gen));
	}
	
	// now repeatedly look for ones that have any to add
	safety = 0;
	do {
		changed = FALSE;	// look for anything that was added in this round
		++safety;
		
		HASH_ITER(hh, generic_table, gen, next_gen) {
			// check that it has its basic relations in its computed relations
			if (GEN_RELATIONS(gen) && !GEN_COMPUTED_RELATIONS(gen)) {
				changed = TRUE;
				GEN_COMPUTED_RELATIONS(gen) = copy_generic_relations(GEN_RELATIONS(gen));
			}
			
			// now attempt to expand computed relations
			HASH_ITER(hh, GEN_COMPUTED_RELATIONS(gen), rel, next_rel) {
				if ((other = real_generic(rel->vnum))) {
					HASH_ITER(hh, GEN_COMPUTED_RELATIONS(other), alt, next_alt) {
						if (!has_generic_relation(GEN_COMPUTED_RELATIONS(gen), alt->vnum)) {
							changed = TRUE;
							add_generic_relation(&GEN_COMPUTED_RELATIONS(gen), alt->vnum);
						}
					}
				}
			}
		}
	} while (changed && safety < 100);
	
	if (safety == 100) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: compute_generic_relations: looped over 100 times");
	}
}


/**
* Deletes a generic relationship, if present.
*
* @param struct generic_relation **list Pointer to the list to delete from.
* @param any_vnum vnum The other generic's vnum, to delete.
* @return bool TRUE if it had that relation and deleted it, FALSE if not.
*/
bool delete_generic_relation(struct generic_relation **list, any_vnum vnum) {
	struct generic_relation *item;
	HASH_FIND_INT(*list, &vnum, item);
	if (item) {
		HASH_DEL(*list, item);
		free(item);
		return TRUE;
	}
	return FALSE;
}


/**
* Gets a generic's name by vnum, safely.
*
* @param any_vnum vnum The generic vnum to look up.
* @return const char* The name, or UNKNOWN if none.
*/
const char *get_generic_name_by_vnum(any_vnum vnum) {
	generic_data *gen = real_generic(vnum);
	
	if (!gen) {
		return "UNKNOWN";	// sanity
	}
	else {
		return GEN_NAME(gen);
	}
}


/**
* Quick, safe lookup for a generic string. This checks that it's the expected
* type, and will return UNKNOWN if anything is out of place.
*
* @param any_vnum vnum The generic vnum to look up.
* @param int type Any GENERIC_ const that must match the vnum's type.
* @param int pos Which string position to fetch.
* @return const char* The string from the generic.
*/
const char *get_generic_string_by_vnum(any_vnum vnum, int type, int pos) {
	generic_data *gen = find_generic(vnum, type);
	
	if (!gen || pos < 0 || pos >= NUM_GENERIC_STRINGS) {
		return "UNKNOWN";	// sanity
	}
	else if (!GEN_STRING(gen, pos)) {
		return "(null)";
	}
	else {
		return GEN_STRING(gen, pos);
	}
}


/**
* Quick, safe lookup for a generic value. This checks that it's the expected
* type, and will return 0 if anything is out of place.
*
* @param any_vnum vnum The generic vnum to look up.
* @param int type Any GENERIC_ const that must match the vnum's type.
* @param int pos Which value position to fetch.
* @return int The value from the generic.
*/
int get_generic_value_by_vnum(any_vnum vnum, int type, int pos) {
	generic_data *gen = find_generic(vnum, type);
	
	if (!gen || pos < 0 || pos >= NUM_GENERIC_VALUES) {
		return 0;	// sanity
	}
	else {
		return GEN_VALUE(gen, pos);
	}
}


/**
* Checks liquid flags based on the liquid's vnum.
*
* @param any_vnum generic_liquid_vnum Which liquid (generic) vnum to check.
* @param bitvector_t flag Which flag(s) to look for.
* @return bool TRUE if ALL those flags are set on the liquid, FALSE if not.
*/
bool liquid_flagged(any_vnum generic_liquid_vnum, bitvector_t flag) {
	generic_data *gen = real_generic(generic_liquid_vnum);
	
	if (gen && flag && (GET_LIQUID_FLAGS(gen) & flag) == flag) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}


/**
* Determines if a generic has a given relationship with another generic.
*
* @param struct generic_relation *list The list to check.
* @param any_vnum vnum The other generic's vnum, to look for.
* @return bool TRUE if it has that relation, FALSE if not.
*/
bool has_generic_relation(struct generic_relation *list, any_vnum vnum) {
	struct generic_relation *item;
	HASH_FIND_INT(list, &vnum, item);
	return item ? TRUE : FALSE;
}


/**
* Counts the words of text in a generic's strings.
*
* @param generic_data *gen The generic whose strings to count.
* @return int The number of words in the generic's strings.
*/
int wordcount_generic(generic_data *gen) {
	int count = 0, iter;
	
	count += wordcount_string(GEN_NAME(gen));
	
	for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
		if (GEN_STRING(gen, iter)) {
			count += wordcount_string(GEN_STRING(gen, iter));
		}
	}
	
	return count;
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common generics problems and reports them to ch.
*
* @param generic_data *gen The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_generic(generic_data *gen, char_data *ch) {
	struct generic_relation *rel, *next_rel;
	char temp[MAX_STRING_LENGTH];
	bool problem = FALSE;
	attack_message_data *amd;
	generic_data *alt;
	obj_data *proto;
	
	if (!GEN_NAME(gen) || !*GEN_NAME(gen) || !str_cmp(GEN_NAME(gen), default_generic_name)) {
		olc_audit_msg(ch, GEN_VNUM(gen), "No name set");
		problem = TRUE;
	}
	if (GEN_FLAGGED(gen, GEN_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, GEN_VNUM(gen), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	
	// GENERIC_x: auditing by type
	switch (GEN_TYPE(gen)) {
		case GENERIC_LIQUID: {
			if (!GET_LIQUID_NAME(gen)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Liquid not set");
				problem = TRUE;
			}
			if (!GET_LIQUID_COLOR(gen)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Color not set");
				problem = TRUE;
			}
			break;
		}
		case GENERIC_ACTION: {
			if (!GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Build-to-char not set");
				problem = TRUE;
			}
			if (!GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Build-to-room not set");
				problem = TRUE;
			}
			if (!GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Craft-to-char not set");
				problem = TRUE;
			}
			if (!GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Craft-to-room not set");
				problem = TRUE;
			}
			if (!GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Repair-to-char not set");
				problem = TRUE;
			}
			if (!GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Repair-to-room not set");
				problem = TRUE;
			}
			break;
		}
		case GENERIC_CURRENCY: {
			if (!GEN_STRING(gen, GSTR_CURRENCY_SINGULAR)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "No singular name set");
				problem = TRUE;
			}
			if (!GEN_STRING(gen, GSTR_CURRENCY_PLURAL)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "No plural name set");
				problem = TRUE;
			}
			break;
		}
		case GENERIC_COMPONENT: {
			if (GET_COMPONENT_OBJ_VNUM(gen) == NOTHING || !(proto = obj_proto(GET_COMPONENT_OBJ_VNUM(gen)))) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Item vnum not set or invalid.");
				problem = TRUE;
			}
			else if (GET_OBJ_COMPONENT(proto) != GEN_VNUM(gen)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Matching item is not set to this component type.");
				problem = TRUE;
			}
			if (!GET_COMPONENT_PLURAL(gen) || !*GET_COMPONENT_PLURAL(gen)) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Plural form not set.");
				problem = TRUE;
			}
			HASH_ITER(hh, GEN_RELATIONS(gen), rel, next_rel) {
				if (rel->vnum == GEN_VNUM(gen)) {
					olc_audit_msg(ch, GEN_VNUM(gen), "Has itself as a relation.");
					problem = TRUE;
				}
				else if (!(alt = real_generic(rel->vnum))) {
					olc_audit_msg(ch, GEN_VNUM(gen), "Invalid relation vnum %d.", rel->vnum);
					problem = TRUE;
				}
				else if (has_generic_relation(GEN_RELATIONS(alt), GEN_VNUM(gen))) {
					olc_audit_msg(ch, GEN_VNUM(gen), "Circular relationship with %d.", rel->vnum);
					problem = TRUE;
				}
			}
			break;
		}
		case GENERIC_AFFECT: {
			if (GET_AFFECT_LOOK_AT_ROOM(gen)) {
				snprintf(temp, sizeof(temp), "%s", NULLSAFE(GET_AFFECT_LOOK_AT_ROOM(gen)));
				// only if present...
				if (strncmp(temp, "...", 3)) {
					olc_audit_msg(ch, GEN_VNUM(gen), "Look-at-room string does not begin with '...'");
					problem = TRUE;
				}
				else if (!strncmp(temp, "... ", 4)) {
					olc_audit_msg(ch, GEN_VNUM(gen), "Look-at-room should not have a space after the '...'");
					problem = TRUE;
				}
			}
			if (GET_AFFECT_DOT_ATTACK(gen) >= 0 && !(amd = real_attack_message(GET_AFFECT_DOT_ATTACK(gen)))) {
				olc_audit_msg(ch, GEN_VNUM(gen), "DoT attack type is invalid.");
				problem = TRUE;
			}
			break;
		}
		case GENERIC_COOLDOWN: {
			// everything here is optional
			break;
		}
		case GENERIC_MOON: {
			if (GET_MOON_CYCLE(gen) < 1) {
				olc_audit_msg(ch, GEN_VNUM(gen), "Moon cycle not set; moon can't be shown.");
				problem = TRUE;
			}
			break;
		}
		case GENERIC_LANGUAGE: {
			// everything here is optional
			// maybe
			break;
		}
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param generic_data *gen The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_generic(generic_data *gen, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		switch (GEN_TYPE(gen)) {
			case GENERIC_COOLDOWN: {
				snprintf(output, sizeof(output), "[%5d] %s (%s): %s", GEN_VNUM(gen), GEN_NAME(gen), generic_types[GEN_TYPE(gen)], NULLSAFE(GET_COOLDOWN_WEAR_OFF(gen)));
				break;
			}
			case GENERIC_COMPONENT: {
				snprintf(output, sizeof(output), "[%5d] %s%s (%s), obj: [%d] %s", GEN_VNUM(gen), GEN_NAME(gen), GEN_FLAGGED(gen, GEN_BASIC) ? " (basic)" : "", generic_types[GEN_TYPE(gen)], GET_COMPONENT_OBJ_VNUM(gen), get_obj_name_by_proto(GET_COMPONENT_OBJ_VNUM(gen)));
				break;
			}
			default: {
				snprintf(output, sizeof(output), "[%5d] %s (%s)", GEN_VNUM(gen), GEN_NAME(gen), generic_types[GEN_TYPE(gen)]);
				break;
			}
		}
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s (%s)", GEN_VNUM(gen), GEN_NAME(gen), generic_types[GEN_TYPE(gen)]);
	}
		
	return output;
}


/**
* Searches properties of generics.
*
* @param char_data *ch The person searching.
* @param char *argument The argument they entered.
*/
void olc_fullsearch_generic(char_data *ch, char *argument) {
	bool any;
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	int count, iter;
	
	bitvector_t only_flags = NOBITS, not_flagged = NOBITS, only_liquid_flags = NOBITS;
	int only_type = NOTHING, vmin = NOTHING, vmax = NOTHING;
	int only_drunk = INT_MIN, drunk_over = INT_MIN, drunk_under = INT_MIN, only_hunger = INT_MIN, hunger_over = INT_MIN, hunger_under = INT_MIN, only_thirst = INT_MIN, thirst_over = INT_MIN, thirst_under = INT_MIN;
	int only_moon = NOTHING, moon_over = NOTHING, moon_under = NOTHING;
	
	generic_data *gen, *next_gen;
	size_t size;
	
	if (!*argument) {
		msg_to_char(ch, "See HELP GENEDIT FULLSEARCH for syntax.\r\n");
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
		
		FULLSEARCH_FLAGS("flags", only_flags, generic_flags)
		FULLSEARCH_FLAGS("flagged", only_flags, generic_flags)
		FULLSEARCH_FLAGS("unflagged", not_flagged, generic_flags)
		FULLSEARCH_LIST("type", only_type, generic_types)
		FULLSEARCH_INT("vmin", vmin, 0, INT_MAX)
		FULLSEARCH_INT("vmax", vmax, 0, INT_MAX)
		
		// liquids
		FULLSEARCH_INT("drunk", only_drunk, INT_MIN, INT_MAX)
		FULLSEARCH_INT("drunkover", drunk_over, INT_MIN, INT_MAX)
		FULLSEARCH_INT("drunkunder", drunk_under, INT_MIN, INT_MAX)
		FULLSEARCH_INT("hunger", only_hunger, INT_MIN, INT_MAX)
		FULLSEARCH_INT("hungerover", hunger_over, INT_MIN, INT_MAX)
		FULLSEARCH_INT("hungerunder", hunger_under, INT_MIN, INT_MAX)
		FULLSEARCH_INT("thirst", only_thirst, INT_MIN, INT_MAX)
		FULLSEARCH_INT("thirstover", thirst_over, INT_MIN, INT_MAX)
		FULLSEARCH_INT("thirstunder", thirst_under, INT_MIN, INT_MAX)
		FULLSEARCH_FLAGS("liquidflags", only_liquid_flags, liquid_flags)
		FULLSEARCH_FLAGS("liquidflagged", only_liquid_flags, liquid_flags)
		
		// moons
		FULLSEARCH_INT("mooncycle", only_moon, -1, INT_MAX)
		FULLSEARCH_INT("mooncycleover", moon_over, 0, INT_MAX)
		FULLSEARCH_INT("mooncycleunder", moon_under, 0, INT_MAX)
		
		else {	// not sure what to do with it? treat it like a keyword
			sprintf(find_keywords + strlen(find_keywords), "%s%s", *find_keywords ? " " : "", type_arg);
		}
		
		// prepare for next loop
		skip_spaces(&argument);
	}
	
	size = snprintf(buf, sizeof(buf), "Generic fullsearch: %s\r\n", show_color_codes(find_keywords));
	count = 0;
	
	// okay now look up generics
	HASH_ITER(hh, generic_table, gen, next_gen) {
		if ((vmin != NOTHING && GEN_VNUM(gen) < vmin) || (vmax != NOTHING && GEN_VNUM(gen) > vmax)) {
			continue;	// vnum range
		}
		if (only_type != NOTHING && GEN_TYPE(gen) != only_type) {
			continue;
		}
		if (not_flagged != NOBITS && IS_SET(GEN_FLAGS(gen), not_flagged)) {
			continue;
		}
		if (only_flags != NOBITS && (GEN_FLAGS(gen) & only_flags) != only_flags) {
			continue;
		}
		
		// liquids
		if (only_drunk != INT_MIN && (GEN_TYPE(gen) != GENERIC_LIQUID || GEN_VALUE(gen, GVAL_LIQUID_DRUNK) != only_drunk)) {
			continue;
		}
		if (drunk_over != INT_MIN && (GEN_TYPE(gen) != GENERIC_LIQUID || GEN_VALUE(gen, GVAL_LIQUID_DRUNK) < drunk_over)) {
			continue;
		}
		if (drunk_under != INT_MIN && (GEN_TYPE(gen) != GENERIC_LIQUID || GEN_VALUE(gen, GVAL_LIQUID_DRUNK) > drunk_under)) {
			continue;
		}
		if (only_hunger != INT_MIN && (GEN_TYPE(gen) != GENERIC_LIQUID || GEN_VALUE(gen, GVAL_LIQUID_FULL) != only_hunger)) {
			continue;
		}
		if (hunger_over != INT_MIN && (GEN_TYPE(gen) != GENERIC_LIQUID || GEN_VALUE(gen, GVAL_LIQUID_FULL) < hunger_over)) {
			continue;
		}
		if (hunger_under != INT_MIN && (GEN_TYPE(gen) != GENERIC_LIQUID || GEN_VALUE(gen, GVAL_LIQUID_FULL) > hunger_under)) {
			continue;
		}
		if (only_thirst != INT_MIN && (GEN_TYPE(gen) != GENERIC_LIQUID || GEN_VALUE(gen, GVAL_LIQUID_THIRST) != only_thirst)) {
			continue;
		}
		if (thirst_over != INT_MIN && (GEN_TYPE(gen) != GENERIC_LIQUID || GEN_VALUE(gen, GVAL_LIQUID_THIRST) < thirst_over)) {
			continue;
		}
		if (thirst_under != INT_MIN && (GEN_TYPE(gen) != GENERIC_LIQUID || GEN_VALUE(gen, GVAL_LIQUID_THIRST) > thirst_under)) {
			continue;
		}
		if (only_liquid_flags != NOBITS && (GEN_TYPE(gen) != GENERIC_LIQUID || (GEN_VALUE(gen, GVAL_LIQUID_FLAGS) & only_liquid_flags) != only_liquid_flags)) {
			continue;
		}
		
		// moons
		if (only_moon != NOTHING && (GEN_TYPE(gen) != GENERIC_MOON || GEN_VALUE(gen, GVAL_MOON_CYCLE) != only_moon)) {
			continue;
		}
		if (moon_over != NOTHING && (GEN_TYPE(gen) != GENERIC_MOON || GEN_VALUE(gen, GVAL_MOON_CYCLE) < moon_over)) {
			continue;
		}
		if (moon_under != NOTHING && (GEN_TYPE(gen) != GENERIC_MOON || GEN_VALUE(gen, GVAL_MOON_CYCLE) > moon_under)) {
			continue;
		}
		
		// search strings
		if (*find_keywords) {
			any = FALSE;
			if (multi_isname(find_keywords, GEN_NAME(gen))) {
				any = TRUE;
			}
			
			for (iter = 0; iter < NUM_GENERIC_STRINGS && !any; ++iter) {
				if (GEN_STRING(gen, iter) && multi_isname(find_keywords, GEN_STRING(gen, iter))) {
					any = TRUE;
				}
			}
			
			// did we find a match in any string
			if (!any) {
				continue;
			}
		}
		
		// show it
		snprintf(line, sizeof(line), "[%5d] %s (%s)\r\n", GEN_VNUM(gen), GEN_NAME(gen), generic_types[GEN_TYPE(gen)]);
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
		size += snprintf(buf + size, sizeof(buf) - size, "(%d generics)\r\n", count);
	}
	else if (count == 0) {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


/**
* Searches for all uses of a generic and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The generic vnum.
*/
void olc_search_generic(char_data *ch, any_vnum vnum) {
	generic_data *gen = real_generic(vnum), *gen_iter, *next_gen;
	ability_data *abil, *next_abil;
	craft_data *craft, *next_craft;
	event_data *event, *next_event;
	struct interaction_item *inter;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	augment_data *aug, *next_aug;
	vehicle_data *veh, *next_veh;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	struct resource_data *res;
	bld_data *bld, *next_bld;
	obj_data *obj, *next_obj;
	int found;
	bool any;
	
	if (!gen) {
		msg_to_char(ch, "There is no generic %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	add_page_display(ch, "Occurrences of generic %d (%s):", vnum, GEN_NAME(gen));
	
	// abilities
	HASH_ITER(hh, ability_table, abil, next_abil) {
		any = FALSE;
		any |= (ABIL_AFFECT_VNUM(abil) == vnum);
		any |= (ABIL_COOLDOWN(abil) == vnum);
		
		LL_FOREACH(ABIL_INTERACTIONS(abil), inter) {
			if (interact_data[inter->type].vnum_type == TYPE_LIQUID && inter->vnum == vnum) {
				any = TRUE;
				break;
			}
		}
		
		if (any) {
			++found;
			add_page_display(ch, "ABIL [%5d] %s", ABIL_VNUM(abil), ABIL_NAME(abil));
		}
	}
	
	// augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		any = FALSE;
		for (res = GET_AUG_RESOURCES(aug); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID) || (GEN_TYPE(gen) == GENERIC_COMPONENT && res->type == RES_COMPONENT))) {
				any = TRUE;
				++found;
				add_page_display(ch, "AUG [%5d] %s", GET_AUG_VNUM(aug), GET_AUG_NAME(aug));
			}
		}
	}
	
	// buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		any = FALSE;
		for (res = GET_BLD_REGULAR_MAINTENANCE(bld); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID) || (GEN_TYPE(gen) == GENERIC_COMPONENT && res->type == RES_COMPONENT))) {
				any = TRUE;
				++found;
				add_page_display(ch, "BLD [%5d] %s", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
			}
		}
	}
	
	// crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		any = FALSE;
		if (CRAFT_FLAGGED(craft, CRAFT_SOUP) && GET_CRAFT_OBJECT(craft) == vnum) {
			any = TRUE;
			++found;
			add_page_display(ch, "CFT [%5d] %s", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
		}
		for (res = GET_CRAFT_RESOURCES(craft); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID) || (GEN_TYPE(gen) == GENERIC_COMPONENT && res->type == RES_COMPONENT))) {
				any = TRUE;
				++found;
				add_page_display(ch, "CFT [%5d] %s", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
			}
		}
	}
	
	// events
	HASH_ITER(hh, event_table, event, next_event) {
		// QR_x: event rewards
		any = find_event_reward_in_list(EVT_RANK_REWARDS(event), QR_CURRENCY, vnum);
		any |= find_event_reward_in_list(EVT_THRESHOLD_REWARDS(event), QR_CURRENCY, vnum);
		any |= find_event_reward_in_list(EVT_RANK_REWARDS(event), QR_SPEAK_LANGUAGE, vnum);
		any |= find_event_reward_in_list(EVT_THRESHOLD_REWARDS(event), QR_SPEAK_LANGUAGE, vnum);
		any |= find_event_reward_in_list(EVT_RANK_REWARDS(event), QR_RECOGNIZE_LANGUAGE, vnum);
		any |= find_event_reward_in_list(EVT_THRESHOLD_REWARDS(event), QR_RECOGNIZE_LANGUAGE, vnum);
		
		if (any) {
			++found;
			add_page_display(ch, "EVT [%5d] %s", EVT_VNUM(event), EVT_NAME(event));
		}
	}
	
	// other generics
	HASH_ITER(hh, generic_table, gen_iter, next_gen) {
		if (has_generic_relation(GEN_RELATIONS(gen_iter), vnum)) {
			++found;
			add_page_display(ch, "GEN [%5d] %s", GEN_VNUM(gen_iter), GEN_NAME(gen_iter));
		}
	}
	
	// objects
	HASH_ITER(hh, object_table, obj, next_obj) {
		any = FALSE;
		if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == vnum) {
			any = TRUE;
		}
		if (GEN_TYPE(gen) == GENERIC_CURRENCY && IS_CURRENCY(obj) && GET_CURRENCY_VNUM(obj) == vnum) {
			any = TRUE;
		}
		if (GEN_TYPE(gen) == GENERIC_COMPONENT && GET_OBJ_COMPONENT(obj) == vnum) {
			any = TRUE;
		}
		if (GEN_TYPE(gen) == GENERIC_AFFECT && GET_POTION_AFFECT(obj) == vnum) {
			any = TRUE;
		}
		if (GEN_TYPE(gen) == GENERIC_COOLDOWN && GET_POTION_COOLDOWN_TYPE(obj) == vnum) {
			any = TRUE;
		}
		if (GEN_TYPE(gen) == GENERIC_AFFECT && GET_POISON_AFFECT(obj) == vnum) {
			any = TRUE;
		}
		
		if (any) {
			++found;
			add_page_display(ch, "OBJ [%5d] %s", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
		}
	}
	
	
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_GET_CURRENCY, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_GET_COMPONENT, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_SPEAK_LANGUAGE, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_RECOGNIZE_LANGUAGE, vnum);
		
		// PRG_PERK_x:
		any |= find_progress_perk_in_list(PRG_PERKS(prg), PRG_PERK_SPEAK_LANGUAGE, vnum);
		any |= find_progress_perk_in_list(PRG_PERKS(prg), PRG_PERK_RECOGNIZE_LANGUAGE, vnum);
		
		if (any) {
			++found;
			add_page_display(ch, "PRG [%5d] %s", PRG_VNUM(prg), PRG_NAME(prg));
		}
	}
	
	// check quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		// REQ_x: quest types
		any = find_requirement_in_list(QUEST_TASKS(quest), REQ_GET_CURRENCY, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_GET_CURRENCY, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_GET_COMPONENT, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_GET_COMPONENT, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_SPEAK_LANGUAGE, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_SPEAK_LANGUAGE, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_RECOGNIZE_LANGUAGE, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_RECOGNIZE_LANGUAGE, vnum);
		
		// QR_x: quest types
		any |= find_quest_reward_in_list(QUEST_REWARDS(quest), QR_CURRENCY, vnum);
		any |= find_quest_reward_in_list(QUEST_REWARDS(quest), QR_SPEAK_LANGUAGE, vnum);
		any |= find_quest_reward_in_list(QUEST_REWARDS(quest), QR_RECOGNIZE_LANGUAGE, vnum);
		
		if (any) {
			++found;
			add_page_display(ch, "QST [%5d] %s", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		any = find_currency_in_shop_item_list(SHOP_ITEMS(shop), vnum);
		
		if (any) {
			++found;
			add_page_display(ch, "SHOP [%5d] %s", SHOP_VNUM(shop), SHOP_NAME(shop));
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		// REQ_x: social requirements
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_GET_CURRENCY, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_GET_COMPONENT, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_SPEAK_LANGUAGE, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_RECOGNIZE_LANGUAGE, vnum);
		
		if (any) {
			++found;
			add_page_display(ch, "SOC [%5d] %s", SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		any = FALSE;
		for (res = VEH_REGULAR_MAINTENANCE(veh); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID) || (GEN_TYPE(gen) == GENERIC_COMPONENT && res->type == RES_COMPONENT))) {
				any = TRUE;
				++found;
				add_page_display(ch, "VEH [%5d] %s", VEH_VNUM(veh), VEH_SHORT_DESC(veh));
			}
		}
	}
	
	if (found > 0) {
		add_page_display(ch, "%d location%s shown", found, PLURAL(found));
	}
	else {
		add_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


// Simple vnum sorter for the generics hash
int sort_generics(generic_data *a, generic_data *b) {
	return GEN_VNUM(a) - GEN_VNUM(b);
}


// for sorted_generics
int sort_generics_by_data(generic_data *a, generic_data *b) {
	if (GEN_TYPE(a) != GEN_TYPE(b)) {
		return GEN_TYPE(a) - GEN_TYPE(b);
	}
	else {
		return str_cmp(GEN_NAME(a), GEN_NAME(b));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts a generic into the hash table.
*
* @param generic_data *gen The generic data to add to the table.
*/
void add_generic_to_table(generic_data *gen) {
	generic_data *find;
	any_vnum vnum;
	
	if (gen) {
		vnum = GEN_VNUM(gen);
		
		// main table
		HASH_FIND_INT(generic_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(generic_table, vnum, gen);
			HASH_SORT(generic_table, sort_generics);
		}
		
		// sorted table
		HASH_FIND(sorted_hh, sorted_generics, &vnum, sizeof(int), find);
		if (!find) {
			HASH_ADD(sorted_hh, sorted_generics, vnum, sizeof(int), gen);
			HASH_SRT(sorted_hh, sorted_generics, sort_generics_by_data);
		}
	}
}


/**
* Removes a generic from the hash table.
*
* @param generic_data *gen The generic data to remove from the table.
*/
void remove_generic_from_table(generic_data *gen) {
	HASH_DEL(generic_table, gen);
	HASH_DELETE(sorted_hh, sorted_generics, gen);
}


/**
* Initializes a new generic. This clears all memory for it, so set the vnum
* AFTER.
*
* @param generic_data *gen The generic to initialize.
*/
void clear_generic(generic_data *gen) {
	memset((char *) gen, 0, sizeof(generic_data));
	
	GEN_VNUM(gen) = NOTHING;
}


/**
* Frees a list of generic relations.
*
* @param struct generic_relation **list The list to free.
*/
void free_generic_relations(struct generic_relation **list) {
	struct generic_relation *item, *next;
	HASH_ITER(hh, *list, item, next) {
		HASH_DEL(*list, item);
		free(item);
	}
}


/**
* frees up memory for a generic data item.
*
* See also: olc_delete_generic
*
* @param generic_data *gen The generic data to free.
*/
void free_generic(generic_data *gen) {
	generic_data *proto = real_generic(GEN_VNUM(gen));
	int iter;
	
	if (GEN_NAME(gen) && (!proto || GEN_NAME(gen) != GEN_NAME(proto))) {
		free(GEN_NAME(gen));
	}
	
	for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
		if (GEN_STRING(gen, iter) && (!proto || GEN_STRING(gen, iter) != GEN_STRING(proto, iter))) {
			free(GEN_STRING(gen, iter));
		}
	}
	
	if (GEN_RELATIONS(gen) && (!proto || GEN_RELATIONS(gen) != GEN_RELATIONS(proto))) {
		free_generic_relations(&GEN_RELATIONS(gen));
	}
	if (GEN_COMPUTED_RELATIONS(gen) && (!proto || GEN_COMPUTED_RELATIONS(gen) != GEN_COMPUTED_RELATIONS(proto))) {
		free_generic_relations(&GEN_COMPUTED_RELATIONS(gen));
	}
	
	free(gen);
}


/**
* Read one generic from file.
*
* @param FILE *fl The open .gen file
* @param any_vnum vnum The generic vnum
*/
void parse_generic(FILE *fl, any_vnum vnum) {
	char line[256], error[256], str_in[256], *ptr;
	generic_data *gen, *find;
	int int_in[4];
	
	CREATE(gen, generic_data, 1);
	clear_generic(gen);
	GEN_VNUM(gen) = vnum;
	
	HASH_FIND_INT(generic_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate generic vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_generic_to_table(gen);
		
	// for error messages
	sprintf(error, "generic vnum %d", vnum);
	
	// line 1: name
	GEN_NAME(gen) = fread_string(fl, error);
	
	// line 2: type flags
	if (!get_line(fl, line) || sscanf(line, "%d %s", &int_in[0], str_in) != 2) {
		log("SYSERR: Format error in line 2 of %s", error);
		exit(1);
	}
	
	GEN_TYPE(gen) = int_in[0];
	GEN_FLAGS(gen) = asciiflag_conv(str_in);
	
	// line 3: values
	if (!get_line(fl, line) || sscanf(line, "%d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
		log("SYSERR: Format error in line 3 of %s", error);
		exit(1);
	}
	
	GEN_VALUE(gen, 0) = int_in[0];
	GEN_VALUE(gen, 1) = int_in[1];
	GEN_VALUE(gen, 2) = int_in[2];
	GEN_VALUE(gen, 3) = int_in[3];
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'R': {	// relation
				if (sscanf(line, "R %d", &int_in[0]) != 1) {
					log("SYSERR: Format error in R line of %s", error);
					break;
				}
				
				add_generic_relation(&GEN_RELATIONS(gen), int_in[0]);
				break;
			}
			case 'T': {	// string
				int_in[0] = atoi(line+1);
				ptr = fread_string(fl, error);
				
				if (int_in[0] >= 0 && int_in[0] < NUM_GENERIC_STRINGS) {
					GEN_STRING(gen, int_in[0]) = ptr;
				}
				else {
					log(" - error in %s: invalid string pos T%d", error, int_in[0]);
					free(ptr);
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
* Version of real_generic() that also checks typesafety.
*
* @param any_vnum vnum Any generic vnum
* @param int type The GENERIC_ type to find.
* @return generic_data* The generic, or NULL if it doesn't exist or has the wrong type.
*/
generic_data *find_generic(any_vnum vnum, int type) {
	generic_data *gen = real_generic(vnum);
	
	if (gen && GEN_TYPE(gen) == type) {
		return gen;
	}
	else {
		return NULL;
	}
}


/**
* @param int type Any GENERIC_ type.
* @param char *name The name to search.
* @param bool exact Can only abbreviate if FALSE.
* @return generic_data* The generic, or NULL if it doesn't exist
*/
generic_data *find_generic_by_name(int type, char *name, bool exact) {
	generic_data *gen, *next_gen, *abbrev = FALSE;
	
	HASH_ITER(sorted_hh, sorted_generics, gen, next_gen) {
		if (GEN_TYPE(gen) != type) {
			continue;
		}
		if (!str_cmp(name, GEN_NAME(gen))) {
			return gen;	// exact match
		}
		else if (!exact && !abbrev && is_abbrev(name, GEN_NAME(gen))) {
			abbrev = gen;	// partial match
		}
	}
	
	return abbrev;	// if any
}


/**
* Finds a generic by type and name while ignoring spaces, dashes, and
* apostrophes. For example, "Guardian Tongue" would match "guardian-tongue" or
* "guardiantongue". It also accepts abbrevs.
*
* @param int type Any GENERIC_ type.
* @param char *name The name to search.
* @return generic_data* The generic, or NULL if it doesn't exist.
*/
generic_data *find_generic_no_spaces(int type, char *name) {
	generic_data *gen, *next_gen;
	int gen_pos, name_pos;
	char *skip = " -'";
	
	HASH_ITER(sorted_hh, sorted_generics, gen, next_gen) {
		if (GEN_TYPE(gen) != type) {
			continue;
		}
		
		// compare names
		for (gen_pos = name_pos = 0; name[name_pos] && GEN_NAME(gen)[gen_pos]; ++gen_pos, ++name_pos) {
			// ensure we're not at a skippable char
			while (name[name_pos] && strchr(skip, name[name_pos])) {
				++name_pos;
			}
			while (GEN_NAME(gen)[gen_pos] && strchr(skip, GEN_NAME(gen)[gen_pos])) {
				++gen_pos;
			}
			
			// did we hit the end
			if (!name[name_pos]) {
				// end of name: successful abbrev
				return gen;
			}
			else if (!GEN_NAME(gen)[gen_pos]) {
				// end of generic's name: not a match
				break;
			}
			else if (LOWER(name[name_pos]) != LOWER(GEN_NAME(gen)[gen_pos])) {
				// not a match
				break;
			}
		}
		
		// if we made it here, it's only a match if name reached the end
		if (!name[name_pos]) {
			return gen;
		}
		// otherwise, move on
	}
	
	return NULL;	// did not find
}


/**
* @param any_vnum vnum Any generic vnum
* @return generic_data* The generic, or NULL if it doesn't exist
*/
generic_data *real_generic(any_vnum vnum) {
	generic_data *gen;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(generic_table, &vnum, gen);
	return gen;
}


// writes entries in the generic index
void write_generic_index(FILE *fl) {
	generic_data *gen, *next_gen;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, generic_table, gen, next_gen) {
		// determine "zone number" by vnum
		this = (int)(GEN_VNUM(gen) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, GEN_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one generic item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param generic_data *gen The thing to save.
*/
void write_generic_to_file(FILE *fl, generic_data *gen) {
	struct generic_relation *rel, *next_rel;
	char temp[256];
	int iter;
	
	if (!fl || !gen) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_generic_to_file called without %s", !fl ? "file" : "generic");
		return;
	}
	
	fprintf(fl, "#%d\n", GEN_VNUM(gen));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(GEN_NAME(gen)));
	
	// 2. type flags
	strcpy(temp, bitv_to_alpha(GEN_FLAGS(gen)));
	fprintf(fl, "%d %s\n", GEN_TYPE(gen), temp);
	
	// 3. values -- need to adjust this if NUM_GENERIC_VALUES changes
	fprintf(fl, "%d %d %d %d\n", GEN_VALUE(gen, 0), GEN_VALUE(gen, 1), GEN_VALUE(gen, 2), GEN_VALUE(gen, 3));
	
	// 'R' relations
	HASH_ITER(hh, GEN_RELATIONS(gen), rel, next_rel) {
		fprintf(fl, "R %d\n", rel->vnum);
	}
	
	// 'T' strings
	for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
		if (GEN_STRING(gen, iter) && *GEN_STRING(gen, iter)) {
			fprintf(fl, "T%d\n%s~\n", iter, GEN_STRING(gen, iter));
		}
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////

/**
* Duplicates a list of generic relations, for OLC.
*
* @param struct generic_relation *list The list to copy.
* @return struct generic_relation* The new list.
*/
struct generic_relation *copy_generic_relations(struct generic_relation *list) {
	struct generic_relation *new_list = NULL, *rel, *next;
	
	HASH_ITER(hh, list, rel, next) {
		add_generic_relation(&new_list, rel->vnum);
	}
	return new_list;
}


/**
* Creates a new generic entry.
* 
* @param any_vnum vnum The number to create.
* @return generic_data* The new generic's prototype.
*/
generic_data *create_generic_table_entry(any_vnum vnum) {
	generic_data *gen;
	
	// sanity
	if (real_generic(vnum)) {
		log("SYSERR: Attempting to insert generic at existing vnum %d", vnum);
		return real_generic(vnum);
	}
	
	CREATE(gen, generic_data, 1);
	clear_generic(gen);
	GEN_VNUM(gen) = vnum;
	GEN_NAME(gen) = str_dup(default_generic_name);
	add_generic_to_table(gen);

	// save index and generic file now
	save_index(DB_BOOT_GEN);
	save_library_file_for_vnum(DB_BOOT_GEN, vnum);

	return gen;
}


/**
* WARNING: This function actually deletes a generic.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_generic(char_data *ch, any_vnum vnum) {
	struct trading_post_data *tpd, *next_tpd;
	struct player_currency *cur, *next_cur;
	struct empire_unique_storage *eus;
	ability_data *abil, *next_abil;
	craft_data *craft, *next_craft;
	event_data *event, *next_event;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	augment_data *aug, *next_aug;
	vehicle_data *veh, *next_veh;
	room_data *room, *next_room;
	empire_data *emp, *next_emp;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	bld_data *bld, *next_bld;
	obj_data *obj, *next_obj;
	descriptor_data *desc;
	generic_data *gen, *gen_iter, *next_gen;
	char_data *chiter, *next_ch;
	bool found, any_quest = FALSE, any_progress = FALSE;
	char name[256];
	int res_type;
	
	if (!(gen = real_generic(vnum))) {
		msg_to_char(ch, "There is no such generic %d.\r\n", vnum);
		return;
	}
	
	snprintf(name, sizeof(name), "%s", NULLSAFE(GEN_NAME(gen)));
	
	switch (GEN_TYPE(gen)) {
		case GENERIC_ACTION: {
			res_type = RES_ACTION;
			break;
		}
		case GENERIC_LIQUID: {
			res_type = RES_LIQUID;
			break;
		}
		case GENERIC_COMPONENT: {
			res_type = RES_COMPONENT;
			break;
		}
		default: {
			res_type = NOTHING;
			break;
		}
	}
	
	// remove from live lists: player currencies
	DL_FOREACH2(player_character_list, chiter, next_plr) {
		HASH_ITER(hh, GET_CURRENCIES(chiter), cur, next_cur) {
			if (cur->vnum == vnum) {
				HASH_DEL(GET_CURRENCIES(chiter), cur);
				free(cur);
				queue_delayed_update(chiter, CDU_SAVE);
			}
		}
		DL_FOREACH(GET_HOME_STORAGE(chiter), eus) {
			if (!eus->obj) {
				continue;
			}
			if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(eus->obj) && GET_DRINK_CONTAINER_TYPE(eus->obj) == vnum) {
				set_obj_val(eus->obj, VAL_DRINK_CONTAINER_TYPE, LIQ_WATER);
				queue_delayed_update(chiter, CDU_SAVE);
			}
		}
	
	}
	
	// remove from live lists: drink containers
	DL_FOREACH(object_list, obj) {
		if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == vnum) {
			set_obj_val(obj, VAL_DRINK_CONTAINER_TYPE, LIQ_WATER);
		}
	}
	
	// remove from live lists: trading post drink containers
	DL_FOREACH_SAFE(trading_list, tpd, next_tpd) {
		if (!tpd->obj) {
			continue;
		}
		
		if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(tpd->obj) && GET_DRINK_CONTAINER_TYPE(tpd->obj) == vnum) {
			set_obj_val(tpd->obj, VAL_DRINK_CONTAINER_TYPE, LIQ_WATER);
		}
	}
	
	// remove from live lists: unique storage of drink containers
	HASH_ITER(hh, empire_table, emp, next_emp) {
		DL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
			if (!eus->obj) {
				continue;
			}
			if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(eus->obj) && GET_DRINK_CONTAINER_TYPE(eus->obj) == vnum) {
				set_obj_val(eus->obj, VAL_DRINK_CONTAINER_TYPE, LIQ_WATER);
				EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
			}
		}
	}
	
	// remove from live resource lists: room resources
	HASH_ITER(hh, world_table, room, next_room) {
		if (!COMPLEX_DATA(room)) {
			continue;
		}
		
		if (GET_BUILT_WITH(room)) {
			if (remove_thing_from_resource_list(&GET_BUILT_WITH(room), res_type, vnum)) {
				request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
			}
		}
		if (BUILDING_RESOURCES(room)) {
			if (remove_thing_from_resource_list(&GET_BUILDING_RESOURCES(room), res_type, vnum)) {
				request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
			}
			
			if (!BUILDING_RESOURCES(room)) {
				// removing this resource finished the building
				if (IS_DISMANTLING(room)) {
					disassociate_building(room);
				}
				else {
					complete_building(room);
				}
			}
		}
	}
	
	// remove from live resource lists: vehicle maintenance
	DL_FOREACH_SAFE(vehicle_list, veh, next_veh) {
		if (VEH_NEEDS_RESOURCES(veh)) {
			remove_thing_from_resource_list(&VEH_NEEDS_RESOURCES(veh), res_type, vnum);
			request_vehicle_save_in_world(veh);
			
			if (!VEH_NEEDS_RESOURCES(veh)) {
				complete_vehicle(veh);	// this could purge it
			}
		}
	}
	
	// remove it from the hash table first
	remove_generic_from_table(gen);

	// save index and generic file now
	save_index(DB_BOOT_GEN);
	save_library_file_for_vnum(DB_BOOT_GEN, vnum);
	
	// remove from computed data
	if (GEN_TYPE(gen) == GENERIC_COMPONENT || GEN_RELATIONS(gen) || GEN_COMPUTED_RELATIONS(gen)) {
		compute_generic_relations();
	}
	
	// now remove from prototypes
	
	// update abilities
	HASH_ITER(hh, ability_table, abil, next_abil) {
		found = FALSE;
		if (ABIL_AFFECT_VNUM(abil) == vnum) {
			ABIL_AFFECT_VNUM(abil) = NOTHING;
			found = TRUE;
		}
		if (ABIL_COOLDOWN(abil) == vnum) {
			ABIL_COOLDOWN(abil) = NOTHING;
			found = TRUE;
		}
		
		found |= delete_from_interaction_list(&ABIL_INTERACTIONS(abil), TYPE_LIQUID, vnum);
		
		if (found) {
			save_library_file_for_vnum(DB_BOOT_ABIL, ABIL_VNUM(abil));
		}
	}
	
	// update augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		if (remove_thing_from_resource_list(&GET_AUG_RESOURCES(aug), res_type, vnum)) {
			SET_BIT(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Augment %d %s set IN-DEV due to deleted generic", GET_AUG_VNUM(aug), GET_AUG_NAME(aug));
			save_library_file_for_vnum(DB_BOOT_AUG, GET_AUG_VNUM(aug));
		}
	}
	
	// update buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		if (remove_thing_from_resource_list(&GET_BLD_REGULAR_MAINTENANCE(bld), res_type, vnum)) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Building %d %s lost deleted maintenance generic", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
			save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
		}
	}
	
	// update crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		found = FALSE;
		if (CRAFT_FLAGGED(craft, CRAFT_SOUP) && GET_CRAFT_OBJECT(craft) == vnum) {
			GET_CRAFT_OBJECT(craft) = LIQ_WATER;
			found |= TRUE;
		}
		found |= remove_thing_from_resource_list(&GET_CRAFT_RESOURCES(craft), res_type, vnum);
		if (found) {
			SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Craft %d %s set IN-DEV due to deleted generic", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// update events
	HASH_ITER(hh, event_table, event, next_event) {
		// QR_x: event reward types
		found = delete_event_reward_from_list(&EVT_RANK_REWARDS(event), QR_CURRENCY, vnum);
		found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(event), QR_CURRENCY, vnum);
		found |= delete_event_reward_from_list(&EVT_RANK_REWARDS(event), QR_SPEAK_LANGUAGE, vnum);
		found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(event), QR_SPEAK_LANGUAGE, vnum);
		found |= delete_event_reward_from_list(&EVT_RANK_REWARDS(event), QR_RECOGNIZE_LANGUAGE, vnum);
		found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(event), QR_RECOGNIZE_LANGUAGE, vnum);
		
		if (found) {
			// SET_BIT(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Event %d %s had rewards for a deleted generic (removed rewards but did not set IN-DEV)", EVT_VNUM(event), EVT_NAME(event));
			save_library_file_for_vnum(DB_BOOT_EVT, EVT_VNUM(event));
		}
	}
	
	// update other generics
	HASH_ITER(hh, generic_table, gen_iter, next_gen) {
		if (delete_generic_relation(&GEN_RELATIONS(gen_iter), vnum)) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Generic %d %s lost deleted related generic", GEN_VNUM(gen_iter), GEN_NAME(gen_iter));
			save_library_file_for_vnum(DB_BOOT_GEN, GEN_VNUM(gen_iter));
		}
	}
	
	// update objs
	HASH_ITER(hh, object_table, obj, next_obj) {
		found = FALSE;
		if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == vnum) {
			found = TRUE;
			set_obj_val(obj, VAL_DRINK_CONTAINER_TYPE, LIQ_WATER);
		}
		if (GEN_TYPE(gen) == GENERIC_CURRENCY && IS_CURRENCY(obj) && GET_CURRENCY_VNUM(obj) == vnum) {
			found = TRUE;
			set_obj_val(obj, VAL_CURRENCY_VNUM, NOTHING);
		}
		if (GEN_TYPE(gen) == GENERIC_COMPONENT && GET_OBJ_COMPONENT(obj) == vnum) {
			found = TRUE;
			obj->proto_data->component = NOTHING;
		}
		if (GEN_TYPE(gen) == GENERIC_AFFECT && GET_POTION_AFFECT(obj) == vnum) {
			found = TRUE;
			set_obj_val(obj, VAL_POTION_AFFECT, NOTHING);
		}
		if (GEN_TYPE(gen) == GENERIC_COOLDOWN && GET_POTION_COOLDOWN_TYPE(obj) == vnum) {
			found = TRUE;
			set_obj_val(obj, VAL_POTION_COOLDOWN_TYPE, NOTHING);
		}
		if (GEN_TYPE(gen) == GENERIC_AFFECT && GET_POISON_AFFECT(obj) == vnum) {
			found = TRUE;
			set_obj_val(obj, VAL_POISON_AFFECT, NOTHING);
		}
		
		if (found) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Object %d %s lost deleted generic", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
			save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		// REQ_x: progress tasks
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_GET_CURRENCY, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_GET_COMPONENT, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_SPEAK_LANGUAGE, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_RECOGNIZE_LANGUAGE, vnum);
		
		// PRG_PERK_x: perks
		found |= delete_progress_perk_from_list(&PRG_PERKS(prg), PRG_PERK_SPEAK_LANGUAGE, vnum);
		found |= delete_progress_perk_from_list(&PRG_PERKS(prg), PRG_PERK_RECOGNIZE_LANGUAGE, vnum);
		
		if (found) {
			any_progress = TRUE;
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Progress %d %s set IN-DEV due to deleted generic", PRG_VNUM(prg), PRG_NAME(prg));
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// remove from quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		// REQ_x: quest types
		found = delete_requirement_from_list(&QUEST_TASKS(quest), REQ_GET_CURRENCY, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_GET_CURRENCY, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_GET_COMPONENT, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_GET_COMPONENT, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_SPEAK_LANGUAGE, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_SPEAK_LANGUAGE, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_RECOGNIZE_LANGUAGE, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_RECOGNIZE_LANGUAGE, vnum);
		
		// QR_x: quest types
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(quest), QR_CURRENCY, vnum);
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(quest), QR_SPEAK_LANGUAGE, vnum);
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(quest), QR_RECOGNIZE_LANGUAGE, vnum);
		
		if (found) {
			any_quest = TRUE;
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Quest %d %s set IN-DEV due to deleted generic", QUEST_VNUM(quest), QUEST_NAME(quest));
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// update shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		if (find_currency_in_shop_item_list(SHOP_ITEMS(shop), vnum)) {
			SET_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Shop %d %s set IN-DEV due to deleted generic", SHOP_VNUM(shop), SHOP_NAME(shop));
			save_library_file_for_vnum(DB_BOOT_SHOP, SHOP_VNUM(shop));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		// REQ_x: social requirements
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_GET_CURRENCY, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_GET_COMPONENT, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_SPEAK_LANGUAGE, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_RECOGNIZE_LANGUAGE, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Social %d %s set IN-DEV due to deleted generic", SOC_VNUM(soc), SOC_NAME(soc));
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// update vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		if (remove_thing_from_resource_list(&VEH_REGULAR_MAINTENANCE(veh), res_type, vnum)) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Vehicle %d %s lost deleted maintenance generic", VEH_VNUM(veh), VEH_SHORT_DESC(veh));
			save_library_file_for_vnum(DB_BOOT_VEH, VEH_VNUM(veh));
		}
	}
	
	// olc editor updates
	LL_FOREACH(descriptor_list, desc) {
		if (desc->character && !IS_NPC(desc->character) && GET_ACTION_RESOURCES(desc->character)) {
			remove_thing_from_resource_list(&GET_ACTION_RESOURCES(desc->character), res_type, vnum);
		}
		
		if (GET_OLC_ABILITY(desc)) {
			found = FALSE;
			if (ABIL_AFFECT_VNUM(GET_OLC_ABILITY(desc)) == vnum) {
				ABIL_AFFECT_VNUM(GET_OLC_ABILITY(desc)) = NOTHING;
				found = TRUE;
			}
			if (ABIL_COOLDOWN(GET_OLC_ABILITY(desc)) == vnum) {
				ABIL_COOLDOWN(GET_OLC_ABILITY(desc)) = NOTHING;
				found = TRUE;
			}
			
			found |= delete_from_interaction_list(&ABIL_INTERACTIONS(GET_OLC_ABILITY(desc)), TYPE_LIQUID, vnum);
			
			if (found) {
				msg_to_char(desc->character, "A generic used by the ability you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_AUGMENT(desc)) {
			if (remove_thing_from_resource_list(&GET_AUG_RESOURCES(GET_OLC_AUGMENT(desc)), res_type, vnum)) {
				SET_BIT(GET_AUG_FLAGS(GET_OLC_AUGMENT(desc)), AUG_IN_DEVELOPMENT);
				msg_to_char(desc->character, "One of the resources used in the augment you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_BUILDING(desc)) {
			if (remove_thing_from_resource_list(&GET_BLD_REGULAR_MAINTENANCE(GET_OLC_BUILDING(desc)), res_type, vnum)) {
				msg_to_char(desc->character, "One of the resources used in the building you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_CRAFT(desc)) {
			found = FALSE;
			if (CRAFT_FLAGGED(GET_OLC_CRAFT(desc), CRAFT_SOUP) && GET_CRAFT_OBJECT(GET_OLC_CRAFT(desc)) == vnum) {
				GET_CRAFT_OBJECT(GET_OLC_CRAFT(desc)) = LIQ_WATER;
				found |= TRUE;
			}
			found |= remove_thing_from_resource_list(&GET_OLC_CRAFT(desc)->resources, res_type, vnum);
			if (found) {
				SET_BIT(GET_OLC_CRAFT(desc)->flags, CRAFT_IN_DEVELOPMENT);
				msg_to_char(desc->character, "One of the resources used in the craft you're editing was deleted.\r\n");
			}	
		}
		if (GET_OLC_GENERIC(desc)) {
			if (delete_generic_relation(&GEN_RELATIONS(GET_OLC_GENERIC(desc)), vnum)) {
				msg_to_char(ch, "One of the related generics for the one you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_EVENT(desc)) {
			// QR_x: event reward types
			found = delete_event_reward_from_list(&EVT_RANK_REWARDS(GET_OLC_EVENT(desc)), QR_CURRENCY, vnum);
			found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(GET_OLC_EVENT(desc)), QR_CURRENCY, vnum);
			found |= delete_event_reward_from_list(&EVT_RANK_REWARDS(GET_OLC_EVENT(desc)), QR_SPEAK_LANGUAGE, vnum);
			found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(GET_OLC_EVENT(desc)), QR_SPEAK_LANGUAGE, vnum);
			found |= delete_event_reward_from_list(&EVT_RANK_REWARDS(GET_OLC_EVENT(desc)), QR_RECOGNIZE_LANGUAGE, vnum);
			found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(GET_OLC_EVENT(desc)), QR_RECOGNIZE_LANGUAGE, vnum);
		
			if (found) {
				// SET_BIT(EVT_FLAGS(GET_OLC_EVENT(desc)), EVTF_IN_DEVELOPMENT);
				msg_to_desc(desc, "A generic used as a reward by the event you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_OBJECT(desc)) {
			found = FALSE;
			if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(GET_OLC_OBJECT(desc)) && GET_DRINK_CONTAINER_TYPE(GET_OLC_OBJECT(desc)) == vnum) {
				found = TRUE;
				set_obj_val(GET_OLC_OBJECT(desc), VAL_DRINK_CONTAINER_TYPE, LIQ_WATER);
				msg_to_char(desc->character, "The generic liquid used by the object you're editing was deleted.\r\n");
			}
			if (GEN_TYPE(gen) == GENERIC_CURRENCY && IS_CURRENCY(GET_OLC_OBJECT(desc)) && GET_CURRENCY_VNUM(GET_OLC_OBJECT(desc)) == vnum) {
				found = TRUE;
				set_obj_val(GET_OLC_OBJECT(desc), VAL_CURRENCY_VNUM, NOTHING);
				msg_to_char(desc->character, "The generic currency used by the object you're editing was deleted.\r\n");
			}
			if (GEN_TYPE(gen) == GENERIC_COMPONENT && GET_OBJ_COMPONENT(GET_OLC_OBJECT(desc)) == vnum) {
				found = TRUE;
				GET_OLC_OBJECT(desc)->proto_data->component = NOTHING;
				msg_to_char(desc->character, "The generic component used by the object you're editing was deleted.\r\n");
			}
			if (GEN_TYPE(gen) == GENERIC_AFFECT && GET_POTION_AFFECT(GET_OLC_OBJECT(desc)) == vnum) {
				found = TRUE;
				set_obj_val(GET_OLC_OBJECT(desc), VAL_POTION_AFFECT, NOTHING);
				msg_to_char(desc->character, "The generic affect used by the potion you're editing was deleted.\r\n");
			}
			if (GEN_TYPE(gen) == GENERIC_COOLDOWN && GET_POTION_COOLDOWN_TYPE(GET_OLC_OBJECT(desc)) == vnum) {
				found = TRUE;
				set_obj_val(GET_OLC_OBJECT(desc), VAL_POTION_COOLDOWN_TYPE, NOTHING);
				msg_to_char(desc->character, "The generic cooldown used by the potion you're editing was deleted.\r\n");
			}
			if (GEN_TYPE(gen) == GENERIC_AFFECT && GET_POISON_AFFECT(GET_OLC_OBJECT(desc)) == vnum) {
				found = TRUE;
				set_obj_val(GET_OLC_OBJECT(desc), VAL_POISON_AFFECT, NOTHING);
				msg_to_char(desc->character, "The generic affect used by the poison you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_PROGRESS(desc)) {
			// REQ_x: progress tasks
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_GET_CURRENCY, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_GET_COMPONENT, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_SPEAK_LANGUAGE, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_RECOGNIZE_LANGUAGE, vnum);
			
			found |= delete_progress_perk_from_list(&PRG_PERKS(GET_OLC_PROGRESS(desc)), PRG_PERK_SPEAK_LANGUAGE, vnum);
			found |= delete_progress_perk_from_list(&PRG_PERKS(GET_OLC_PROGRESS(desc)), PRG_PERK_RECOGNIZE_LANGUAGE, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "A 'generic' used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			// REQ_x: quest types
			found = delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_GET_CURRENCY, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_GET_CURRENCY, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_GET_COMPONENT, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_GET_COMPONENT, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_SPEAK_LANGUAGE, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_SPEAK_LANGUAGE, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_RECOGNIZE_LANGUAGE, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_RECOGNIZE_LANGUAGE, vnum);
			
			// QR_x: quest types
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_CURRENCY, vnum);
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_SPEAK_LANGUAGE, vnum);
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_RECOGNIZE_LANGUAGE, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "A 'generic' used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SHOP(desc)) {
			if (find_currency_in_shop_item_list(SHOP_ITEMS(GET_OLC_SHOP(desc)), vnum)) {
				msg_to_char(desc->character, "One of the currencies used for the shop you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			// REQ_x: social requirements
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_GET_CURRENCY, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_GET_COMPONENT, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_SPEAK_LANGUAGE, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_RECOGNIZE_LANGUAGE, vnum);
		
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A 'generic' required by the social you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_VEHICLE(desc)) {
			if (remove_thing_from_resource_list(&VEH_REGULAR_MAINTENANCE(GET_OLC_VEHICLE(desc)), res_type, vnum)) {
				msg_to_char(desc->character, "One of the resources used for maintenance for the vehicle you're editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted generic %d %s", GET_NAME(ch), vnum, name);
	msg_to_char(ch, "Generic %d (%s) deleted.\r\n", vnum, name);
	
	free_generic(gen);
	
	if (any_progress) {
		need_progress_refresh = TRUE;
	}
	if (any_quest) {
		DL_FOREACH_SAFE2(player_character_list, chiter, next_ch, next_plr) {
			refresh_all_quests(chiter);
		}
	}
	
	check_languages_all();
}


/**
* Function to save a player's changes to a generic (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_generic(descriptor_data *desc) {	
	generic_data *proto, *gen = GET_OLC_GENERIC(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	struct generic_relation *cmprel;
	UT_hash_handle hh, sorted_hh;
	int iter;

	// have a place to save it?
	if (!(proto = real_generic(vnum))) {
		proto = create_generic_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (GEN_NAME(proto)) {
		free(GEN_NAME(proto));
	}
	for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
		if (GEN_STRING(proto, iter)) {
			free(GEN_STRING(proto, iter));
		}
	}
	free_generic_relations(&GEN_RELATIONS(proto));
	
	// save these for later
	cmprel = GEN_COMPUTED_RELATIONS(proto);
	GEN_COMPUTED_RELATIONS(proto) = NULL;
	
	// sanity
	if (!GEN_NAME(gen) || !*GEN_NAME(gen)) {
		if (GEN_NAME(gen)) {
			free(GEN_NAME(gen));
		}
		GEN_NAME(gen) = str_dup(default_generic_name);
	}
	for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
		if (GEN_STRING(gen, iter) && !*GEN_STRING(gen, iter)) {
			free(GEN_STRING(gen, iter));
			GEN_STRING(gen, iter) = NULL;
		}
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handles
	sorted_hh = proto->sorted_hh;
	*proto = *gen;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handles
	proto->sorted_hh = sorted_hh;
	GEN_COMPUTED_RELATIONS(proto) = cmprel;	// restore old computed relations
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_GEN, vnum);
	
	// resort
	HASH_SRT(sorted_hh, sorted_generics, sort_generics_by_data);
	
	// update computed relations
	if (GEN_TYPE(gen) == GENERIC_COMPONENT || GEN_RELATIONS(gen) || GEN_COMPUTED_RELATIONS(gen)) {
		compute_generic_relations();
	}
	
	// update player languages
	if (GEN_TYPE(gen) == GENERIC_LANGUAGE) {
		check_languages_all();
	}
}


/**
* Creates a copy of a generic, or clears a new one, for editing.
* 
* @param generic_data *input The generic to copy, or NULL to make a new one.
* @return generic_data* The copied generic.
*/
generic_data *setup_olc_generic(generic_data *input) {
	generic_data *new;
	int iter;
	
	CREATE(new, generic_data, 1);
	clear_generic(new);
	
	if (input) {
		// copy normal data
		*new = *input;
		
		// copy things that are pointers
		GEN_NAME(new) = GEN_NAME(input) ? str_dup(GEN_NAME(input)) : NULL;
		for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
			GEN_STRING(new, iter) = GEN_STRING(input, iter) ? str_dup(GEN_STRING(input, iter)) : NULL;
		}
		
		// copy basic relations but skip computed ones
		GEN_RELATIONS(new) = copy_generic_relations(GEN_RELATIONS(input));
		GEN_COMPUTED_RELATIONS(new) = NULL;
	}
	else {
		// brand new: some defaults
		GEN_NAME(new) = str_dup(default_generic_name);
	}
	
	// done
	return new;
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* Displays relations on multiple lines.
*
* @param struct generic_relation *list The list to show.
* @param bool show_vnums If true, shows the vnums with each entry.
* @param char *save_buf A string buffer for the output.
* @param char *prefix Optional: Prepends this text to the output string while preserving line-formatting (NULL for none)
*/
void get_generic_relation_display(struct generic_relation *list, bool show_vnums, char *save_buf, char *prefix) {
	struct generic_relation *rel, *next;
	char part[MAX_STRING_LENGTH];
	int this_line = 0;
	bool any = FALSE;
	
	strcpy(save_buf, NULLSAFE(prefix));
	this_line = strlen(save_buf);
	
	HASH_ITER(hh, list, rel, next) {
		if (show_vnums) {
			sprintf(part, "[%d] %s", rel->vnum, get_generic_name_by_vnum(rel->vnum));
		}
		else {
			strcpy(part, get_generic_name_by_vnum(rel->vnum));
		}
		
		// append how
		if (this_line > 0 && this_line + strlen(part) + 2 >= 80) {
			sprintf(save_buf + strlen(save_buf), "%s\r\n  %s", (any ? "," : ""), part);
			this_line = strlen(part);
		}
		else {
			sprintf(save_buf + strlen(save_buf), "%s%s", (any ? (this_line ? ", " : "  ") : ""), part);
			this_line += strlen(part) + 2;
		}
		
		any = TRUE;
	}
	
	if (this_line > 0) {
		strcat(save_buf, "\r\n");
	}
}


/**
* For vstat.
*
* @param char_data *ch The player requesting stats.
* @param generic_data *gen The generic to display.
*/
void do_stat_generic(char_data *ch, generic_data *gen) {
	char part[MAX_STRING_LENGTH];
	
	if (!gen) {
		return;
	}
	
	// first line
	add_page_display(ch, "VNum: [\tc%d\t0], Name: \ty%s\t0, Type: \tc%s\t0", GEN_VNUM(gen), GEN_NAME(gen), generic_types[GEN_TYPE(gen)]);
	
	sprintbit(GEN_FLAGS(gen), generic_flags, part, TRUE);
	add_page_display(ch, "Flags: \tg%s\t0", part);
	
	// GENERIC_x
	switch (GEN_TYPE(gen)) {
		case GENERIC_LIQUID: {
			add_page_display(ch, "Liquid: \ty%s\t0, Color: \ty%s\t0", NULLSAFE(GET_LIQUID_NAME(gen)), NULLSAFE(GET_LIQUID_COLOR(gen)));
			add_page_display(ch, "Hunger: [\tc%d\t0], Thirst: [\tc%d\t0], Drunk: [\tc%d\t0]", GET_LIQUID_FULL(gen), GET_LIQUID_THIRST(gen), GET_LIQUID_DRUNK(gen));
			
			sprintbit(GET_LIQUID_FLAGS(gen), liquid_flags, part, TRUE);
			add_page_display(ch, "Liquid flags: \tg%s\t0", part);
			break;
		}
		case GENERIC_ACTION: {
			add_page_display(ch, "Build-to-Char: %s", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR)));
			add_page_display(ch, "Build-to-Room: %s", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM)));
			add_page_display(ch, "Craft-to-Char: %s", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR)));
			add_page_display(ch, "Craft-to-Room: %s", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM)));
			add_page_display(ch, "Repair-to-Char: %s", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR)));
			add_page_display(ch, "Repair-to-Room: %s", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM)));
			break;
		}
		case GENERIC_COOLDOWN: {
			add_page_display(ch, "Wear-off: %s", GET_COOLDOWN_WEAR_OFF(gen) ? GET_COOLDOWN_WEAR_OFF(gen) : "(none)");
			break;
		}
		case GENERIC_AFFECT: {
			add_page_display(ch, "DoT attack type: %d %s", GET_AFFECT_DOT_ATTACK(gen), (GET_AFFECT_DOT_ATTACK(gen) > 0) ? get_attack_name_by_vnum(GET_AFFECT_DOT_ATTACK(gen)) : "(none)");
			add_page_display(ch, "Apply to-char: %s", GET_AFFECT_APPLY_TO_CHAR(gen) ? GET_AFFECT_APPLY_TO_CHAR(gen) : "(none)");
			add_page_display(ch, "Apply to-room: %s", GET_AFFECT_APPLY_TO_ROOM(gen) ? GET_AFFECT_APPLY_TO_ROOM(gen) : "(none)");
			add_page_display(ch, "Wear-off: %s", GET_AFFECT_WEAR_OFF_TO_CHAR(gen) ? GET_AFFECT_WEAR_OFF_TO_CHAR(gen) : "(none)");
			add_page_display(ch, "Wear-off to room: %s", GET_AFFECT_WEAR_OFF_TO_ROOM(gen) ? GET_AFFECT_WEAR_OFF_TO_ROOM(gen) : "(none)");
			add_page_display(ch, "Look at char: %s", GET_AFFECT_LOOK_AT_CHAR(gen) ? GET_AFFECT_LOOK_AT_CHAR(gen) : "(none)");
			add_page_display(ch, "Look at room: %s", GET_AFFECT_LOOK_AT_ROOM(gen) ? GET_AFFECT_LOOK_AT_ROOM(gen) : "(none)");
			break;
		}
		case GENERIC_CURRENCY: {
			add_page_display(ch, "Singular: %s", NULLSAFE(GEN_STRING(gen, GSTR_CURRENCY_SINGULAR)));
			add_page_display(ch, "Plural: %s", NULLSAFE(GEN_STRING(gen, GSTR_CURRENCY_PLURAL)));
			break;
		}
		case GENERIC_COMPONENT: {
			add_page_display(ch, "Plural: %s", NULLSAFE(GEN_STRING(gen, GSTR_COMPONENT_PLURAL)));
			add_page_display(ch, "Item: [%d] %s", GET_COMPONENT_OBJ_VNUM(gen), get_obj_name_by_proto(GET_COMPONENT_OBJ_VNUM(gen)));
			
			get_generic_relation_display(GEN_RELATIONS(gen), TRUE, part, NULL);
			add_page_display(ch, "Relations:\r\n%s", GEN_RELATIONS(gen) ? part : " none");
			get_generic_relation_display(GEN_COMPUTED_RELATIONS(gen), TRUE, part, NULL);
			add_page_display(ch, "Extended Relations:\r\n%s", GEN_COMPUTED_RELATIONS(gen) ? part : " none");
			break;
		}
		case GENERIC_MOON: {
			add_page_display(ch, "Cycle: \ty%.2f day%s\t0", GET_MOON_CYCLE_DAYS(gen), PLURAL(GET_MOON_CYCLE_DAYS(gen)));
			break;
		}
		case GENERIC_LANGUAGE: {
			// no known properties
			break;
		}
	}

	send_page_display(ch);
}


/**
* This is the main recipe display for generic OLC. It displays the user's
* currently-edited generic.
*
* @param char_data *ch The person who is editing a generic and will see its display.
*/
void olc_show_generic(char_data *ch) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	
	if (!gen) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !real_generic(GEN_VNUM(gen)) ? "new generic" : GEN_NAME(real_generic(GEN_VNUM(gen))));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(GEN_NAME(gen), default_generic_name), NULLSAFE(GEN_NAME(gen)));
	sprintf(buf + strlen(buf), "<%stype\t0> %s\r\n", OLC_LABEL_VAL(GEN_TYPE(gen), 0), generic_types[GEN_TYPE(gen)]);
	
	sprintbit(GEN_FLAGS(gen), generic_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(GEN_FLAGS(gen), NOBITS), lbuf);
	
	// GENERIC_x
	switch (GEN_TYPE(gen)) {
		case GENERIC_LIQUID: {
			sprintf(buf + strlen(buf), "<%sliquid\t0> %s\r\n", OLC_LABEL_STR(GET_LIQUID_NAME(gen), ""), GET_LIQUID_NAME(gen) ? GET_LIQUID_NAME(gen) : "(none)");
			sprintf(buf + strlen(buf), "<%scolor\t0> %s\r\n", OLC_LABEL_STR(GET_LIQUID_COLOR(gen), ""), GET_LIQUID_COLOR(gen) ? GET_LIQUID_COLOR(gen) : "(none)");
			sprintf(buf + strlen(buf), "<%shunger\t0> %d hour%s\r\n", OLC_LABEL_VAL(GET_LIQUID_FULL(gen), 0), GET_LIQUID_FULL(gen), PLURAL(GET_LIQUID_FULL(gen)));
			sprintf(buf + strlen(buf), "<%sthirst\t0> %d hour%s\r\n", OLC_LABEL_VAL(GET_LIQUID_THIRST(gen), 0), GET_LIQUID_THIRST(gen), PLURAL(GET_LIQUID_THIRST(gen)));
			sprintf(buf + strlen(buf), "<%sdrunk\t0> %d hour%s\r\n", OLC_LABEL_VAL(GET_LIQUID_DRUNK(gen), 0), GET_LIQUID_DRUNK(gen), PLURAL(GET_LIQUID_DRUNK(gen)));
			
			sprintbit(GET_LIQUID_FLAGS(gen), liquid_flags, lbuf, TRUE);
			sprintf(buf + strlen(buf), "<%sliquidflags\t0> %s\r\n", OLC_LABEL_VAL(GET_LIQUID_FLAGS(gen), NOBITS), lbuf);
			break;
		}
		case GENERIC_ACTION: {
			sprintf(buf + strlen(buf), "<%sbuild2char\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR), ""), GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR) ? GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR) : "(not set)");
			sprintf(buf + strlen(buf), "<%sbuild2room\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM), ""), GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM) ? GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM) : "(not set)");
			sprintf(buf + strlen(buf), "<%scraft2char\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR), ""), GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR) ? GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR) : "(not set)");
			sprintf(buf + strlen(buf), "<%scraft2room\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM), ""), GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM) ? GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM) : "(not set)");
			sprintf(buf + strlen(buf), "<%srepair2char\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR), ""), GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR) ? GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR) : "(not set)");
			sprintf(buf + strlen(buf), "<%srepair2room\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM), ""), GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM) ? GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM) : "(not set)");
			break;
		}
		case GENERIC_COOLDOWN: {
			sprintf(buf + strlen(buf), "<%swearoff\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_COOLDOWN_WEAR_OFF), ""), GET_COOLDOWN_WEAR_OFF(gen) ? GET_COOLDOWN_WEAR_OFF(gen) : "(none)");
			sprintf(buf + strlen(buf), "<%sstandardwearoff\t0> (to add a basic wear-off message based on the name)\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_COOLDOWN_WEAR_OFF), ""));
			break;
		}
		case GENERIC_AFFECT: {
			sprintf(buf + strlen(buf), "<%sapply2char\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_AFFECT_APPLY_TO_CHAR), ""), GET_AFFECT_APPLY_TO_CHAR(gen) ? GET_AFFECT_APPLY_TO_CHAR(gen) : "(none)");
			sprintf(buf + strlen(buf), "<%sapply2room\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_AFFECT_APPLY_TO_ROOM), ""), GET_AFFECT_APPLY_TO_ROOM(gen) ? GET_AFFECT_APPLY_TO_ROOM(gen) : "(none)");
			sprintf(buf + strlen(buf), "<%swearoff\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_AFFECT_WEAR_OFF_TO_CHAR), ""), GET_AFFECT_WEAR_OFF_TO_CHAR(gen) ? GET_AFFECT_WEAR_OFF_TO_CHAR(gen) : "(none)");
			sprintf(buf + strlen(buf), "<%sstandardwearoff\t0> (to add a basic wear-off message based on the name)\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_AFFECT_WEAR_OFF_TO_CHAR), ""));
			sprintf(buf + strlen(buf), "<%swearoff2room\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_AFFECT_WEAR_OFF_TO_ROOM), ""), GET_AFFECT_WEAR_OFF_TO_ROOM(gen) ? GET_AFFECT_WEAR_OFF_TO_ROOM(gen) : "(none)");
			sprintf(buf + strlen(buf), "<%slookatchar\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_AFFECT_LOOK_AT_CHAR), ""), GET_AFFECT_LOOK_AT_CHAR(gen) ? GET_AFFECT_LOOK_AT_CHAR(gen) : "(none)");
			sprintf(buf + strlen(buf), "<%slookatroom\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_AFFECT_LOOK_AT_ROOM), ""), GET_AFFECT_LOOK_AT_ROOM(gen) ? GET_AFFECT_LOOK_AT_ROOM(gen) : "(none)");
			sprintf(buf + strlen(buf), "<%sdotattack\t0> %d %s\r\n", OLC_LABEL_VAL(GET_AFFECT_DOT_ATTACK(gen), 0), GET_AFFECT_DOT_ATTACK(gen), (GET_AFFECT_DOT_ATTACK(gen) > 0) ? get_attack_name_by_vnum(GET_AFFECT_DOT_ATTACK(gen)) : "(none)");
			break;
		}
		case GENERIC_CURRENCY: {
			sprintf(buf + strlen(buf), "<%ssingular\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_CURRENCY_SINGULAR), ""), GEN_STRING(gen, GSTR_CURRENCY_SINGULAR) ? GEN_STRING(gen, GSTR_CURRENCY_SINGULAR) : "(not set)");
			sprintf(buf + strlen(buf), "<%splural\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_CURRENCY_PLURAL), ""), GEN_STRING(gen, GSTR_CURRENCY_PLURAL) ? GEN_STRING(gen, GSTR_CURRENCY_PLURAL) : "(not set)");
			break;
		}
		case GENERIC_COMPONENT: {
			sprintf(buf + strlen(buf), "<%splural\t0> %s\r\n", OLC_LABEL_STR(GEN_STRING(gen, GSTR_COMPONENT_PLURAL), ""), GEN_STRING(gen, GSTR_COMPONENT_PLURAL) ? GEN_STRING(gen, GSTR_COMPONENT_PLURAL) : "(not set)");
			sprintf(buf + strlen(buf), "<%sitem\t0> [%d] %s\r\n", OLC_LABEL_VAL(GET_COMPONENT_OBJ_VNUM(gen), NOTHING), GET_COMPONENT_OBJ_VNUM(gen), get_obj_name_by_proto(GET_COMPONENT_OBJ_VNUM(gen)));
			
			get_generic_relation_display(GEN_RELATIONS(gen), TRUE, lbuf, NULL);
			sprintf(buf + strlen(buf), "<%srelations\t0>\r\n%s", OLC_LABEL_PTR(GEN_RELATIONS(gen)), lbuf);
			break;
		}
		case GENERIC_MOON: {
			sprintf(buf + strlen(buf), "<%scycle\t0> %.2f day%s\r\n", OLC_LABEL_VAL(GET_MOON_CYCLE(gen), 0), GET_MOON_CYCLE_DAYS(gen), PLURAL(GET_MOON_CYCLE_DAYS(gen)));
			break;
		}
		case GENERIC_LANGUAGE: {
			// no properties
			break;
		}
	}
		
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the generic db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_generic(char *searchname, char_data *ch) {
	generic_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, generic_table, iter, next_iter) {
		if (multi_isname(searchname, GEN_NAME(iter))) {
			add_page_display(ch, "%3d. [%5d] %s (%s)", ++found, GEN_VNUM(iter), GEN_NAME(iter), generic_types[GEN_TYPE(iter)]);
		}
	}
	
	send_page_display(ch);
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// GENERIC OLC MODULES /////////////////////////////////////////////////////

OLC_MODULE(genedit_flags) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	bool had_in_dev = GEN_FLAGGED(gen, GEN_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	GEN_FLAGS(gen) = olc_process_flag(ch, argument, "generic", "flags", generic_flags, GEN_FLAGS(gen));
	
	// validate removal of GEN_IN_DEVELOPMENT
	if (had_in_dev && !GEN_FLAGGED(gen, GEN_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(GEN_FLAGS(gen), GEN_IN_DEVELOPMENT);
	}
}


OLC_MODULE(genedit_liquidflags) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		GEN_VALUE(gen, GVAL_LIQUID_FLAGS) = olc_process_flag(ch, argument, "liquid", "liquidflags", liquid_flags, GET_LIQUID_FLAGS(gen));
	}
}


OLC_MODULE(genedit_name) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	olc_process_string(ch, argument, "name", &GEN_NAME(gen));
}


OLC_MODULE(genedit_type) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	char buf[MAX_STRING_LENGTH];
	int old = GEN_TYPE(gen), iter;
	
	GEN_TYPE(gen) = olc_process_type(ch, argument, "type", "type", generic_types, GEN_TYPE(gen));
	
	// reset values to defaults now
	if (old != GEN_TYPE(gen)) {
		for (iter = 0; iter < NUM_GENERIC_VALUES; ++iter) {
			GEN_VALUE(gen, iter) = 0;
		}
		for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
			if (GEN_STRING(gen, iter)) {
				free(GEN_STRING(gen, iter));
				GEN_STRING(gen, iter) = NULL;
			}
		}
		
		if (GEN_TYPE(gen) != GENERIC_COMPONENT) {
			free_generic_relations(&GEN_RELATIONS(gen));
		}
		
		switch (GEN_TYPE(gen)) {
			case GENERIC_COMPONENT: {
				if (GEN_NAME(gen) && *GEN_NAME(gen)) {
					sprintf(buf, "%ss", GEN_NAME(gen));
					GEN_STRING(gen, GSTR_COMPONENT_PLURAL) = str_dup(buf);
				}
				if (obj_proto(GEN_VNUM(gen))) {
					// default to same-vnum and similar-plural
					GEN_VALUE(gen, GVAL_OBJ_VNUM) = GEN_VNUM(gen);
				}
				else {
					GEN_VALUE(gen, GVAL_OBJ_VNUM) = NOTHING;
				}
				break;
			}
		}
		
		if (generic_types_uses_in_dev[GEN_TYPE(gen)]) {
			SET_BIT(GEN_FLAGS(gen), GEN_IN_DEVELOPMENT);
			msg_to_char(ch, "Added IN-DEVELOPMENT flag because you changed it to %s %s.\r\n", AN(generic_types[GEN_TYPE(gen)]), generic_types[GEN_TYPE(gen)]);
		}
		else if (GEN_FLAGGED(gen, GEN_IN_DEVELOPMENT)) {
			REMOVE_BIT(GEN_FLAGS(gen), GEN_IN_DEVELOPMENT);
			msg_to_char(ch, "Removed IN-DEVELOPMENT flag because you changed it to %s %s.\r\n", AN(generic_types[GEN_TYPE(gen)]), generic_types[GEN_TYPE(gen)]);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ACTION OLC MODULES //////////////////////////////////////////////////////

OLC_MODULE(genedit_build2char) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR)) {
			free(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR));
		}
		GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR) = NULL;
		msg_to_char(ch, "Build2char message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "build2char", &GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR));
	}
}


OLC_MODULE(genedit_build2room) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM)) {
			free(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM));
		}
		GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM) = NULL;
		msg_to_char(ch, "Build2room message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "build2room", &GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM));
	}
}


OLC_MODULE(genedit_craft2char) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR)) {
			free(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR));
		}
		GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR) = NULL;
		msg_to_char(ch, "Craft2char message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "craft2char", &GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR));
	}
}


OLC_MODULE(genedit_craft2room) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM)) {
			free(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM));
		}
		GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM) = NULL;
		msg_to_char(ch, "Craft2room message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "craft2room", &GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM));
	}
}


OLC_MODULE(genedit_repair2char) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR)) {
			free(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR));
		}
		GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR) = NULL;
		msg_to_char(ch, "Repair2char message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "repair2char", &GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR));
	}
}


OLC_MODULE(genedit_repair2room) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM)) {
			free(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM));
		}
		GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM) = NULL;
		msg_to_char(ch, "Repair2room message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "repair2room", &GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CURRENCY OLC MODULES ////////////////////////////////////////////////////

OLC_MODULE(genedit_plural) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	int pos = NOTHING;
	
	switch (GEN_TYPE(gen)) {
		case GENERIC_CURRENCY: {
			pos = GSTR_CURRENCY_PLURAL;
			break;
		}
		case GENERIC_COMPONENT: {
			pos = GSTR_COMPONENT_PLURAL;
			break;
		}
		default: {
			msg_to_char(ch, "You can't set a plural on this type of generic.\r\n");
			return;
		}
	}
	
	if (pos != NOTHING) {
		olc_process_string(ch, argument, "plural", &GEN_STRING(gen, pos));
	}
}


OLC_MODULE(genedit_singular) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_CURRENCY) {
		msg_to_char(ch, "You can only change that on an CURRENCY generic.\r\n");
	}
	else {
		olc_process_string(ch, argument, "singular", &GEN_STRING(gen, GSTR_CURRENCY_SINGULAR));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COOLDOWN/AFFECT OLC MODULES /////////////////////////////////////////////

OLC_MODULE(genedit_apply2char) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	int pos = 0;
	
	switch (GEN_TYPE(gen)) {
		case GENERIC_AFFECT: {
			pos = GSTR_AFFECT_APPLY_TO_CHAR;
			break;
		}
		default: {
			msg_to_char(ch, "You can only change that on an AFFECT generic.\r\n");
			return;
		}
	}
	
	if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, pos)) {
			free(GEN_STRING(gen, pos));
		}
		GEN_STRING(gen, pos) = NULL;
		msg_to_char(ch, "Apply-to-char message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "apply2char", &GEN_STRING(gen, pos));
	}
}


OLC_MODULE(genedit_apply2room) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	int pos = 0;
	
	switch (GEN_TYPE(gen)) {
		case GENERIC_AFFECT: {
			pos = GSTR_AFFECT_APPLY_TO_ROOM;
			break;
		}
		default: {
			msg_to_char(ch, "You can only change that on an AFFECT generic.\r\n");
			return;
		}
	}
	
	if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, pos)) {
			free(GEN_STRING(gen, pos));
		}
		GEN_STRING(gen, pos) = NULL;
		msg_to_char(ch, "Apply-to-room message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "apply2room", &GEN_STRING(gen, pos));
	}
}


OLC_MODULE(genedit_dotattack) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	int pos = 0;
	attack_message_data *amd;
	
	switch (GEN_TYPE(gen)) {
		case GENERIC_AFFECT: {
			pos = GVAL_AFFECT_DOT_ATTACK;
			break;
		}
		default: {
			msg_to_char(ch, "You can only change that on an AFFECT generic.\r\n");
			return;
		}
	}
	
	if (!*argument) {
		msg_to_char(ch, "Set the custom DoT attack type to what (vnum or name)?\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		GEN_VALUE(gen, pos) = 0;
		msg_to_char(ch, "Custom DoT attack type removed.\r\n");
	}
	else if (!(amd = find_attack_message_by_name_or_vnum(argument, FALSE))) {
		msg_to_char(ch, "Unknown attack message '%s'.\r\n", argument);
	}
	else {
		GEN_VALUE(gen, pos) = ATTACK_VNUM(amd);
		msg_to_char(ch, "DoT attack type set to [%d] %s.\r\n", ATTACK_VNUM(amd), ATTACK_NAME(amd));
	}
}


OLC_MODULE(genedit_lookatchar) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	int pos = 0;
	
	switch (GEN_TYPE(gen)) {
		case GENERIC_AFFECT: {
			pos = GSTR_AFFECT_LOOK_AT_CHAR;
			break;
		}
		default: {
			msg_to_char(ch, "You can only change that on an AFFECT generic.\r\n");
			return;
		}
	}
	
	if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, pos)) {
			free(GEN_STRING(gen, pos));
		}
		GEN_STRING(gen, pos) = NULL;
		msg_to_char(ch, "Look-at-char message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "lookatchar", &GEN_STRING(gen, pos));
	}
}


OLC_MODULE(genedit_lookatroom) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	int pos = 0;
	
	switch (GEN_TYPE(gen)) {
		case GENERIC_AFFECT: {
			pos = GSTR_AFFECT_LOOK_AT_ROOM;
			break;
		}
		default: {
			msg_to_char(ch, "You can only change that on an AFFECT generic.\r\n");
			return;
		}
	}
	
	if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, pos)) {
			free(GEN_STRING(gen, pos));
		}
		GEN_STRING(gen, pos) = NULL;
		msg_to_char(ch, "Look-at-room message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "lookatroom", &GEN_STRING(gen, pos));
	}
}


// creates a new cooldown with pre-filled fields
OLC_MODULE(genedit_quick_cooldown) {
	char type_arg[MAX_INPUT_LENGTH], vnum_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	int from_type;
	any_vnum vnum;
	
	two_arguments(argument, type_arg, vnum_arg);
	
	if (!*type_arg || !*vnum_arg) {
		msg_to_char(ch, "Usage: .generic quickcooldown <type> <vnum>\r\n");
		return;
	}
	if (!(from_type = find_olc_type(type_arg))) {
		msg_to_char(ch, "Invalid olc type '%s'.\r\n", type_arg);
		return;
	}
	if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0 || vnum > MAX_VNUM) {
		msg_to_char(ch, "You must pick a valid vnum between 0 and %d.\r\n", MAX_VNUM);
		return;
	}
	if (real_generic(vnum)) {
		msg_to_char(ch, "There is already a generic with that vnum.\r\n");
		return;
	}
	if (!can_start_olc_edit(ch, OLC_GENERIC, vnum)) {
		return;	// sends own message
	}
	
	// OLC_x: see if there's something to create and set it up
	switch (from_type) {
		case OLC_ABILITY: {
			ability_data *abil = find_ability_by_vnum(vnum);
			if (!abil) {
				msg_to_char(ch, "There is no ability by that vnum.\r\n");
				return;
			}
			else {
				GET_OLC_GENERIC(ch->desc) = setup_olc_generic(NULL);
				free(GEN_NAME(GET_OLC_GENERIC(ch->desc)));
				GEN_NAME(GET_OLC_GENERIC(ch->desc)) = str_dup(NULLSAFE(ABIL_NAME(abil)));
			}
			break;
		}
		case OLC_AUGMENT: {
			augment_data *aug = augment_proto(vnum);
			if (!aug) {
				msg_to_char(ch, "There is no augment by that vnum.\r\n");
				return;
			}
			else {
				GET_OLC_GENERIC(ch->desc) = setup_olc_generic(NULL);
				free(GEN_NAME(GET_OLC_GENERIC(ch->desc)));
				GEN_NAME(GET_OLC_GENERIC(ch->desc)) = str_dup(NULLSAFE(GET_AUG_NAME(aug)));
			}
			break;
		}
		case OLC_BUILDING: {
			bld_data *bld = building_proto(vnum);
			if (!bld) {
				msg_to_char(ch, "There is no building by that vnum.\r\n");
				return;
			}
			else {
				GET_OLC_GENERIC(ch->desc) = setup_olc_generic(NULL);
				free(GEN_NAME(GET_OLC_GENERIC(ch->desc)));
				GEN_NAME(GET_OLC_GENERIC(ch->desc)) = str_dup(NULLSAFE(GET_BLD_NAME(bld)));
			}
			break;
		}
		case OLC_CRAFT: {
			craft_data *cft = craft_proto(vnum);
			if (!cft) {
				msg_to_char(ch, "There is no craft by that vnum.\r\n");
				return;
			}
			else {
				GET_OLC_GENERIC(ch->desc) = setup_olc_generic(NULL);
				free(GEN_NAME(GET_OLC_GENERIC(ch->desc)));
				GEN_NAME(GET_OLC_GENERIC(ch->desc)) = str_dup(NULLSAFE(GET_CRAFT_NAME(cft)));
			}
			break;
		}
		case OLC_MOBILE: {
			char_data *mob = mob_proto(vnum);
			if (!mob) {
				msg_to_char(ch, "There is no mobile by that vnum.\r\n");
				return;
			}
			else {
				GET_OLC_GENERIC(ch->desc) = setup_olc_generic(NULL);
				free(GEN_NAME(GET_OLC_GENERIC(ch->desc)));
				GEN_NAME(GET_OLC_GENERIC(ch->desc)) = str_dup(skip_filler(NULLSAFE(GET_SHORT_DESC(mob))));
			}
			break;
		}
		case OLC_MORPH: {
			morph_data *morph = morph_proto(vnum);
			if (!morph) {
				msg_to_char(ch, "There is no morph by that vnum.\r\n");
				return;
			}
			else {
				GET_OLC_GENERIC(ch->desc) = setup_olc_generic(NULL);
				free(GEN_NAME(GET_OLC_GENERIC(ch->desc)));
				GEN_NAME(GET_OLC_GENERIC(ch->desc)) = str_dup(skip_filler(NULLSAFE(MORPH_SHORT_DESC(morph))));
			}
			break;
		}
		case OLC_OBJECT: {
			obj_data *obj = obj_proto(vnum);
			if (!obj) {
				msg_to_char(ch, "There is no object by that vnum.\r\n");
				return;
			}
			else {
				GET_OLC_GENERIC(ch->desc) = setup_olc_generic(NULL);
				free(GEN_NAME(GET_OLC_GENERIC(ch->desc)));
				GEN_NAME(GET_OLC_GENERIC(ch->desc)) = str_dup(skip_filler(NULLSAFE(GET_OBJ_SHORT_DESC(obj))));
			}
			break;
		}
		case OLC_SKILL: {
			skill_data *skl = find_skill_by_vnum(vnum);
			if (!skl) {
				msg_to_char(ch, "There is no skill by that vnum.\r\n");
				return;
			}
			else {
				GET_OLC_GENERIC(ch->desc) = setup_olc_generic(NULL);
				free(GEN_NAME(GET_OLC_GENERIC(ch->desc)));
				GEN_NAME(GET_OLC_GENERIC(ch->desc)) = str_dup(NULLSAFE(SKILL_NAME(skl)));
			}
			break;
		}
		case OLC_TRIGGER: {
			trig_data *trig = real_trigger(vnum);
			if (!trig) {
				msg_to_char(ch, "There is no trigger by that vnum.\r\n");
				return;
			}
			else {
				GET_OLC_GENERIC(ch->desc) = setup_olc_generic(NULL);
				free(GEN_NAME(GET_OLC_GENERIC(ch->desc)));
				GEN_NAME(GET_OLC_GENERIC(ch->desc)) = str_dup(NULLSAFE(GET_TRIG_NAME(trig)));
			}
			break;
		}
		case OLC_VEHICLE: {
			vehicle_data *veh = vehicle_proto(vnum);
			if (!veh) {
				msg_to_char(ch, "There is no vehicle by that vnum.\r\n");
				return;
			}
			else {
				GET_OLC_GENERIC(ch->desc) = setup_olc_generic(NULL);
				free(GEN_NAME(GET_OLC_GENERIC(ch->desc)));
				GEN_NAME(GET_OLC_GENERIC(ch->desc)) = str_dup(skip_filler(NULLSAFE(VEH_SHORT_DESC(veh))));
			}
			break;
		}
		default: {
			msg_to_char(ch, "Quickcooldown is not supported for that type.\r\n");
			return;
		}
	}
	
	// SUCCESS
	msg_to_char(ch, "You create a quick cooldown generic %d:\r\n", vnum);
	GET_OLC_TYPE(ch->desc) = OLC_GENERIC;
	GET_OLC_VNUM(ch->desc) = vnum;
	
	// ensure some data
	GEN_VNUM(GET_OLC_GENERIC(ch->desc)) = vnum;
	GEN_TYPE(GET_OLC_GENERIC(ch->desc)) = GENERIC_COOLDOWN;
	snprintf(buf, sizeof(buf), "Your %s cooldown has ended.", GEN_NAME(GET_OLC_GENERIC(ch->desc)));
	if (GEN_STRING(GET_OLC_GENERIC(ch->desc), GSTR_COOLDOWN_WEAR_OFF)) {
		free(GEN_STRING(GET_OLC_GENERIC(ch->desc), GSTR_COOLDOWN_WEAR_OFF));
	}
	GEN_STRING(GET_OLC_GENERIC(ch->desc), GSTR_COOLDOWN_WEAR_OFF) = str_dup(buf);
	
	olc_show_generic(ch);
}


OLC_MODULE(genedit_standardwearoff) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	char buf[MAX_STRING_LENGTH];
	int pos = 0;
	
	switch (GEN_TYPE(gen)) {
		case GENERIC_COOLDOWN: {
			snprintf(buf, sizeof(buf), "Your %s cooldown has ended.", GEN_NAME(gen));
			pos = GSTR_COOLDOWN_WEAR_OFF;
			break;
		}
		case GENERIC_AFFECT: {
			snprintf(buf, sizeof(buf), "Your %s wears off.", GEN_NAME(gen));
			pos = GSTR_AFFECT_WEAR_OFF_TO_CHAR;
			break;
		}
		default: {
			msg_to_char(ch, "You can only change that on an AFFECT or COOLDOWN generic.\r\n");
			return;
		}
	}
	
	if (GEN_STRING(gen, pos)) {
		free(GEN_STRING(gen, pos));
	}
	GEN_STRING(gen, pos) = str_dup(buf);

	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		msg_to_char(ch, "Wear-off message changed to: %s\r\n", buf);
	}
}


OLC_MODULE(genedit_wearoff) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	int pos = 0;
	
	switch (GEN_TYPE(gen)) {
		case GENERIC_COOLDOWN: {
			pos = GSTR_COOLDOWN_WEAR_OFF;
			break;
		}
		case GENERIC_AFFECT: {
			pos = GSTR_AFFECT_WEAR_OFF_TO_CHAR;
			break;
		}
		default: {
			msg_to_char(ch, "You can only change that on an AFFECT or COOLDOWN generic.\r\n");
			return;
		}
	}
	
	if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, pos)) {
			free(GEN_STRING(gen, pos));
		}
		GEN_STRING(gen, pos) = NULL;
		msg_to_char(ch, "Wear-off message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "wearoff", &GEN_STRING(gen, pos));
	}
}


OLC_MODULE(genedit_wearoff2room) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	int pos = 0;
	
	switch (GEN_TYPE(gen)) {
		case GENERIC_AFFECT: {
			pos = GSTR_AFFECT_WEAR_OFF_TO_ROOM;
			break;
		}
		default: {
			msg_to_char(ch, "You can only change that on an AFFECT generic.\r\n");
			return;
		}
	}
	
	if (!str_cmp(argument, "none")) {
		if (GEN_STRING(gen, pos)) {
			free(GEN_STRING(gen, pos));
		}
		GEN_STRING(gen, pos) = NULL;
		msg_to_char(ch, "Wear-off-to-room message removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "wearoff2room", &GEN_STRING(gen, pos));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMPONENT OLC MODULES ///////////////////////////////////////////////////

OLC_MODULE(genedit_item) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	obj_vnum old = GET_COMPONENT_OBJ_VNUM(gen);
	
	if (GEN_TYPE(gen) != GENERIC_COMPONENT) {
		msg_to_char(ch, "You can only change that on a COMPONENT generic.\r\n");
	}
	else {
		GEN_VALUE(gen, GVAL_OBJ_VNUM) = olc_process_number(ch, argument, "object vnum", "item", 0, MAX_VNUM, GET_COMPONENT_OBJ_VNUM(gen));
		if (!obj_proto(GET_COMPONENT_OBJ_VNUM(gen))) {
			GEN_VALUE(gen, GVAL_OBJ_VNUM) = old;
			msg_to_char(ch, "There is no object with that vnum. Old value restored.\r\n");
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It is now represented by item [%d] %s.\r\n", GET_COMPONENT_OBJ_VNUM(gen), get_obj_name_by_proto(GET_COMPONENT_OBJ_VNUM(gen)));
		}
	}
}


OLC_MODULE(genedit_relations) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc), *cmp = NULL;
	struct generic_relation **list = &GEN_RELATIONS(gen);
	char cmd_arg[MAX_INPUT_LENGTH], vnum_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	struct generic_relation *iter, *copyfrom, *next_rel;
	bool found, none;
	any_vnum vnum;
	
	if (GEN_TYPE(gen) != GENERIC_COMPONENT) {
		msg_to_char(ch, "You can only set relations on COMPONENT generics.\r\n");
		return;
	}
	
	argument = any_one_arg(argument, cmd_arg);	// add/remove/change/copy
	
	if (is_abbrev(cmd_arg, "copy")) {
		// usage: relation copy <from vnum>
		argument = any_one_arg(argument, vnum_arg);	// any vnum for that type
		
		if (!*vnum_arg) {
			msg_to_char(ch, "Usage: relation copy <from vnum>\r\n");
		}
		else if (!isdigit(*vnum_arg)) {
			msg_to_char(ch, "Copy from which generic?\r\n");
		}
		else if ((vnum = atoi(vnum_arg)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			generic_data *from_gen = real_generic(vnum);
			if (from_gen) {
				copyfrom = GEN_RELATIONS(from_gen);
				none = copyfrom ? FALSE : TRUE;
			}
			else {
				copyfrom = NULL;
				none = FALSE;
			}
			
			if (none) {
				msg_to_char(ch, "No relations to copy from that.\r\n");
			}
			else if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, vnum_arg);
			}
			else {
				HASH_ITER(hh, copyfrom, iter, next_rel) {
					add_generic_relation(list, iter->vnum);
				}
				msg_to_char(ch, "Copied relations from generic %d.\r\n", vnum);
			}
		}
	}	// end 'copy'
	else if (is_abbrev(cmd_arg, "remove")) {
		// usage: relation remove <vnum | all>
		skip_spaces(&argument);	// only arg is number
		
		if (!*argument) {
			msg_to_char(ch, "Remove which relation (vnum or name)?\r\n");
		}
		else if (!str_cmp(argument, "all")) {
			free_generic_relations(list);
			*list = NULL;
			msg_to_char(ch, "You remove all the relations.\r\n");
		}
		else if (!(cmp = find_generic_component(argument))) {
			msg_to_char(ch, "Invalid related generic (vnum or name).\r\n");
		}
		else {
			found = FALSE;
			HASH_ITER(hh, *list, iter, next_rel) {
				if (cmp && iter->vnum == GEN_VNUM(cmp)) {
					found = TRUE;
					msg_to_char(ch, "You remove the relation to [%d] %s.\r\n", iter->vnum, get_generic_name_by_vnum(iter->vnum));
					HASH_DEL(*list, iter);
					free(iter);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid relation vnum or name.\r\n");
			}
		}
	}	// end 'remove'
	else if (is_abbrev(cmd_arg, "add")) {
		// usage: relation add <vnum>
		argument = any_one_arg(argument, vnum_arg);
		
		if (!*vnum_arg) {
			msg_to_char(ch, "Usage: relation add <vnum | name>\r\n");
		}
		else if (!(cmp = find_generic_component(vnum_arg))) {
			msg_to_char(ch, "Invalid generic '%s'.\r\n", vnum_arg);
		}
		else {
			// success
			add_generic_relation(list, GEN_VNUM(cmp));
			msg_to_char(ch, "You add relation: [%d] %s\r\n", GEN_VNUM(cmp), GEN_NAME(cmp));
		}
	}	// end 'add'
	else {
		msg_to_char(ch, "Usage: relation add <vnum>\r\n");
		msg_to_char(ch, "Usage: relation copy <from vnum>\r\n");
		msg_to_char(ch, "Usage: relation remove <vnum | all>\r\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// LIQUID OLC MODULES //////////////////////////////////////////////////////

OLC_MODULE(genedit_color) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		olc_process_string(ch, argument, "color", &GEN_STRING(gen, GSTR_LIQUID_COLOR));
	}
}


OLC_MODULE(genedit_cycle) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	double days;
	
	if (GEN_TYPE(gen) != GENERIC_MOON) {
		msg_to_char(ch, "You can only change that on a MOON generic.\r\n");
	}
	else if ((days = olc_process_double(ch, argument, "days in the moon's cycle", "cycle", .01, 100000.00, 0.0)) > 0.0) {
		 GEN_VALUE(gen, GVAL_MOON_CYCLE) = (int)(days * 100.0);
	}
}


OLC_MODULE(genedit_drunk) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		GEN_VALUE(gen, GVAL_LIQUID_DRUNK) = olc_process_number(ch, argument, "drunk", "drunk", -MAX_LIQUID_COND, MAX_LIQUID_COND, GET_LIQUID_DRUNK(gen));
	}
}


OLC_MODULE(genedit_hunger) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		GEN_VALUE(gen, GVAL_LIQUID_FULL) = olc_process_number(ch, argument, "hunger", "hunger", -MAX_LIQUID_COND, MAX_LIQUID_COND, GET_LIQUID_FULL(gen));
	}
}


OLC_MODULE(genedit_liquid) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		olc_process_string(ch, argument, "liquid", &GEN_STRING(gen, GSTR_LIQUID_NAME));
	}
}


OLC_MODULE(genedit_thirst) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		GEN_VALUE(gen, GVAL_LIQUID_THIRST) = olc_process_number(ch, argument, "thirst", "thirst", -MAX_LIQUID_COND, MAX_LIQUID_COND, GET_LIQUID_THIRST(gen));
	}
}
