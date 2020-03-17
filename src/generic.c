/* ************************************************************************
*   File: generic.c                                       EmpireMUD 2.0b5 *
*  Usage: loading, saving, OLC, and functions for generics                *
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
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   Generic OLC Modules
*   Action OLC Modules
*   Component OLC Modules
*   Liquid OLC Modules
*/

// local data
const char *default_generic_name = "Unnamed Generic";
#define MAX_LIQUID_COND  (MAX_CONDITION / 15)	// approximate game hours of max cond

// external consts
extern const char *generic_flags[];
extern const char *generic_types[];
extern const char *olc_type_bits[NUM_OLC_TYPES+1];

// local funcs
struct generic_relation *copy_generic_relations(struct generic_relation *list);
void free_generic_relations(struct generic_relation **list);
bool has_generic_relation(struct generic_relation *list, any_vnum vnum);

// external funcs
extern bool can_start_olc_edit(char_data *ch, int type, any_vnum vnum);
extern bool delete_quest_reward_from_list(struct quest_reward **list, int type, any_vnum vnum);
extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
extern bool find_currency_in_shop_item_list(struct shop_item *list, any_vnum vnum);
extern bool find_quest_reward_in_list(struct quest_reward *list, int type, any_vnum vnum);
extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);
extern bool remove_thing_from_resource_list(struct resource_data **list, int type, any_vnum vnum);


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
	bool problem = FALSE;
	generic_data *alt;
	obj_data *proto;
	
	if (!GEN_NAME(gen) || !*GEN_NAME(gen) || !str_cmp(GEN_NAME(gen), default_generic_name)) {
		olc_audit_msg(ch, GEN_VNUM(gen), "No name set");
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
		case GENERIC_AFFECT:
		case GENERIC_COOLDOWN: {
			// everything here is optional
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
				snprintf(output, sizeof(output), "[%5d] %s%s (%s)", GEN_VNUM(gen), GEN_NAME(gen), GEN_FLAGGED(gen, GEN_BASIC) ? " (basic)" : "", generic_types[GEN_TYPE(gen)]);
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
* Searches for all uses of a generic and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The generic vnum.
*/
void olc_search_generic(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	generic_data *gen = real_generic(vnum), *gen_iter, *next_gen;
	ability_data *abil, *next_abil;
	craft_data *craft, *next_craft;
	event_data *event, *next_event;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	augment_data *aug, *next_aug;
	vehicle_data *veh, *next_veh;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	struct resource_data *res;
	bld_data *bld, *next_bld;
	obj_data *obj, *next_obj;
	int size, found;
	bool any;
	
	if (!gen) {
		msg_to_char(ch, "There is no generic %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of generic %d (%s):\r\n", vnum, GEN_NAME(gen));
	
	// abilities
	HASH_ITER(hh, ability_table, abil, next_abil) {
		any = FALSE;
		any |= (ABIL_AFFECT_VNUM(abil) == vnum);
		any |= (ABIL_COOLDOWN(abil) == vnum);
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "ABIL [%5d] %s\r\n", ABIL_VNUM(abil), ABIL_NAME(abil));
		}
	}
	
	// augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		any = FALSE;
		for (res = GET_AUG_RESOURCES(aug); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID) || (GEN_TYPE(gen) == GENERIC_COMPONENT && res->type == RES_COMPONENT))) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "AUG [%5d] %s\r\n", GET_AUG_VNUM(aug), GET_AUG_NAME(aug));
			}
		}
	}
	
	// buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		any = FALSE;
		for (res = GET_BLD_YEARLY_MAINTENANCE(bld); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID) || (GEN_TYPE(gen) == GENERIC_COMPONENT && res->type == RES_COMPONENT))) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "BLD [%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
			}
		}
	}
	
	// crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		any = FALSE;
		if (CRAFT_FLAGGED(craft, CRAFT_SOUP) && GET_CRAFT_OBJECT(craft) == vnum) {
			any = TRUE;
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
		}
		for (res = GET_CRAFT_RESOURCES(craft); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID) || (GEN_TYPE(gen) == GENERIC_COMPONENT && res->type == RES_COMPONENT))) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
			}
		}
	}
	
	// events
	HASH_ITER(hh, event_table, event, next_event) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QR_x: event rewards
		any = find_event_reward_in_list(EVT_RANK_REWARDS(event), QR_CURRENCY, vnum);
		any |= find_event_reward_in_list(EVT_THRESHOLD_REWARDS(event), QR_CURRENCY, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "EVT [%5d] %s\r\n", EVT_VNUM(event), EVT_NAME(event));
		}
	}
	
	// other generics
	HASH_ITER(hh, generic_table, gen_iter, next_gen) {
		if (has_generic_relation(GEN_RELATIONS(gen_iter), vnum)) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "GEN [%5d] %s\r\n", GEN_VNUM(gen_iter), GEN_NAME(gen_iter));
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
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "OBJ [%5d] %s\r\n", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
		}
	
	
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_GET_CURRENCY, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_GET_COMPONENT, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "PRG [%5d] %s\r\n", PRG_VNUM(prg), PRG_NAME(prg));
		}
	}}
	
	// check quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QR_x, REQ_x: quest types
		any = find_requirement_in_list(QUEST_TASKS(quest), REQ_GET_CURRENCY, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_GET_CURRENCY, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_GET_COMPONENT, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_GET_COMPONENT, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		any |= find_quest_reward_in_list(QUEST_REWARDS(quest), QR_CURRENCY, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		any = find_currency_in_shop_item_list(SHOP_ITEMS(shop), vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SHOP [%5d] %s\r\n", SHOP_VNUM(shop), SHOP_NAME(shop));
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_GET_CURRENCY, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_GET_COMPONENT, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		any = FALSE;
		for (res = VEH_YEARLY_MAINTENANCE(veh); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID) || (GEN_TYPE(gen) == GENERIC_COMPONENT && res->type == RES_COMPONENT))) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "VEH [%5d] %s\r\n", VEH_VNUM(veh), VEH_SHORT_DESC(veh));
			}
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
		HASH_FIND_INT(generic_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(generic_table, vnum, gen);
			HASH_SORT(generic_table, sort_generics);
			HASH_SORT(sorted_generics, sort_generics_by_data);
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
	void adjust_vehicle_tech(vehicle_data *veh, bool add);
	void complete_building(room_data *room);
	void refresh_all_quests(char_data *ch);
	
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
	int res_type;
	
	if (!(gen = real_generic(vnum))) {
		msg_to_char(ch, "There is no such generic %d.\r\n", vnum);
		return;
	}
	
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
	LL_FOREACH(character_list, chiter) {
		if (IS_NPC(chiter)) {
			continue;
		}
		
		HASH_ITER(hh, GET_CURRENCIES(chiter), cur, next_cur) {
			if (cur->vnum == vnum) {
				HASH_DEL(GET_CURRENCIES(chiter), cur);
				free(cur);
			}
		}
	}
	
	// remove from live lists: drink containers
	LL_FOREACH(object_list, obj) {
		if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == vnum) {
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
		}
	}
	
	// remove from live lists: trading post drink containers
	for (tpd = trading_list; tpd; tpd = next_tpd) {
		next_tpd = tpd->next;
		if (!tpd->obj) {
			continue;
		}
		
		if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(tpd->obj) && GET_DRINK_CONTAINER_TYPE(tpd->obj) == vnum) {
			GET_OBJ_VAL(tpd->obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
		}
	}
	
	// remove from live lists: unique storage of drink containers
	HASH_ITER(hh, empire_table, emp, next_emp) {
		LL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
			if (!eus->obj) {
				continue;
			}
			if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(eus->obj) && GET_DRINK_CONTAINER_TYPE(eus->obj) == vnum) {
				GET_OBJ_VAL(eus->obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
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
			remove_thing_from_resource_list(&GET_BUILT_WITH(room), res_type, vnum);
		}
		if (GET_BUILDING_RESOURCES(room)) {
			remove_thing_from_resource_list(&GET_BUILDING_RESOURCES(room), res_type, vnum);
			
			if (!GET_BUILDING_RESOURCES(room)) {
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
	LL_FOREACH(vehicle_list, veh) {
		if (VEH_NEEDS_RESOURCES(veh)) {
			remove_thing_from_resource_list(&VEH_NEEDS_RESOURCES(veh), res_type, vnum);
			
			if (!VEH_NEEDS_RESOURCES(veh)) {
				// removing the resource finished the vehicle
				if (VEH_FLAGGED(veh, VEH_INCOMPLETE)) {
					REMOVE_BIT(VEH_FLAGS(veh), VEH_INCOMPLETE);
					adjust_vehicle_tech(veh, TRUE);
					load_vtrigger(veh);
				}
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
		
		if (found) {
			save_library_file_for_vnum(DB_BOOT_ABIL, ABIL_VNUM(abil));
		}
	}
	
	// update augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		if (remove_thing_from_resource_list(&GET_AUG_RESOURCES(aug), res_type, vnum)) {
			SET_BIT(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_AUG, GET_AUG_VNUM(aug));
		}
	}
	
	// update buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		if (remove_thing_from_resource_list(&GET_BLD_YEARLY_MAINTENANCE(bld), res_type, vnum)) {
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
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// update events
	HASH_ITER(hh, event_table, event, next_event) {
		// QR_x: event reward types
		found = delete_event_reward_from_list(&EVT_RANK_REWARDS(event), QR_CURRENCY, vnum);
		found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(event), QR_CURRENCY, vnum);
		
		if (found) {
			// SET_BIT(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_EVT, EVT_VNUM(event));
		}
	}
	
	// update other generics
	HASH_ITER(hh, generic_table, gen_iter, next_gen) {
		if (delete_generic_relation(&GEN_RELATIONS(gen_iter), vnum)) {
			save_library_file_for_vnum(DB_BOOT_GEN, GEN_VNUM(gen_iter));
		}
	}
	
	// update objs
	HASH_ITER(hh, object_table, obj, next_obj) {
		found = FALSE;
		if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == vnum) {
			found = TRUE;
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
		}
		if (GEN_TYPE(gen) == GENERIC_CURRENCY && IS_CURRENCY(obj) && GET_CURRENCY_VNUM(obj) == vnum) {
			found = TRUE;
			GET_OBJ_VAL(obj, VAL_CURRENCY_VNUM) = NOTHING;
		}
		if (GEN_TYPE(gen) == GENERIC_COMPONENT && GET_OBJ_COMPONENT(obj) == vnum) {
			found = TRUE;
			obj->proto_data->component = NOTHING;
		}
		
		if (found) {
			save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_GET_CURRENCY, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_GET_COMPONENT, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		
		if (found) {
			any_progress = TRUE;
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// remove from quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		// REQ_x, QR_x: quest types
		found = delete_requirement_from_list(&QUEST_TASKS(quest), REQ_GET_CURRENCY, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_GET_CURRENCY, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_GET_COMPONENT, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_GET_COMPONENT, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(quest), QR_CURRENCY, vnum);
		
		if (found) {
			any_quest = TRUE;
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// update shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		if (find_currency_in_shop_item_list(SHOP_ITEMS(shop), vnum)) {
			SET_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SHOP, SHOP_VNUM(shop));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_GET_CURRENCY, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_GET_COMPONENT, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// update vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		if (remove_thing_from_resource_list(&VEH_YEARLY_MAINTENANCE(veh), res_type, vnum)) {
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
			if (remove_thing_from_resource_list(&GET_BLD_YEARLY_MAINTENANCE(GET_OLC_BUILDING(desc)), res_type, vnum)) {
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
		
			if (found) {
				// SET_BIT(EVT_FLAGS(GET_OLC_EVENT(desc)), EVTF_IN_DEVELOPMENT);
				msg_to_desc(desc, "A generic currency used as a reward by the event you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_OBJECT(desc)) {
			found = FALSE;
			if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(GET_OLC_OBJECT(desc)) && GET_DRINK_CONTAINER_TYPE(GET_OLC_OBJECT(desc)) == vnum) {
				found = TRUE;
				GET_OBJ_VAL(GET_OLC_OBJECT(desc), VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
				msg_to_char(desc->character, "The generic liquid used by the object you're editing was deleted.\r\n");
			}
			if (GEN_TYPE(gen) == GENERIC_CURRENCY && IS_CURRENCY(GET_OLC_OBJECT(desc)) && GET_CURRENCY_VNUM(GET_OLC_OBJECT(desc)) == vnum) {
				found = TRUE;
				GET_OBJ_VAL(GET_OLC_OBJECT(desc), VAL_CURRENCY_VNUM) = NOTHING;
				msg_to_char(desc->character, "The generic currency used by the object you're editing was deleted.\r\n");
			}
			if (GEN_TYPE(gen) == GENERIC_COMPONENT && GET_OBJ_COMPONENT(GET_OLC_OBJECT(desc)) == vnum) {
				found = TRUE;
				GET_OLC_OBJECT(desc)->proto_data->component = NOTHING;
				msg_to_char(desc->character, "The generic component used by the object you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_PROGRESS(desc)) {
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_GET_CURRENCY, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_GET_COMPONENT, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "A 'generic' used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			// REQ_x, QR_x: quest types
			found = delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_GET_CURRENCY, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_GET_CURRENCY, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_GET_COMPONENT, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_GET_COMPONENT, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
			
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_CURRENCY, vnum);
		
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
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_GET_CURRENCY, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_GET_COMPONENT, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_EMPIRE_PRODUCED_COMPONENT, vnum);
		
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A 'generic' required by the social you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_VEHICLE(desc)) {
			if (remove_thing_from_resource_list(&VEH_YEARLY_MAINTENANCE(GET_OLC_VEHICLE(desc)), res_type, vnum)) {
				msg_to_char(desc->character, "One of the resources used for maintenance for the vehicle you're editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted generic %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Generic %d deleted.\r\n", vnum);
	
	free_generic(gen);
	
	if (any_progress) {
		need_progress_refresh = TRUE;
	}
	if (any_quest) {
		LL_FOREACH_SAFE(character_list, chiter, next_ch) {
			if (!IS_NPC(chiter)) {
				refresh_all_quests(chiter);
			}
		}
	}
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
	HASH_SORT(sorted_generics, sort_generics_by_data);
	
	// update computed relations
	if (GEN_TYPE(gen) == GENERIC_COMPONENT || GEN_RELATIONS(gen) || GEN_COMPUTED_RELATIONS(gen)) {
		compute_generic_relations();
	}
}


/**
* Creates a copy of a generic, or clears a new one, for editing.
* 
* @param generic_data *input The generic to copy, or NULL to make a new one.
* @return generic_data* The copied generic.
*/
generic_data *setup_olc_generic(generic_data *input) {
	extern struct req_data *copy_requirements(struct req_data *from);
	
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
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH * 2];
	size_t size;
	
	if (!gen) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \ty%s\t0, Type: \tc%s\t0\r\n", GEN_VNUM(gen), GEN_NAME(gen), generic_types[GEN_TYPE(gen)]);
	
	sprintbit(GEN_FLAGS(gen), generic_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	// GENERIC_x
	switch (GEN_TYPE(gen)) {
		case GENERIC_LIQUID: {
			size += snprintf(buf + size, sizeof(buf) - size, "Liquid: \ty%s\t0, Color: \ty%s\t0\r\n", NULLSAFE(GET_LIQUID_NAME(gen)), NULLSAFE(GET_LIQUID_COLOR(gen)));
			size += snprintf(buf + size, sizeof(buf) - size, "Hunger: [\tc%d\t0], Thirst: [\tc%d\t0], Drunk: [\tc%d\t0]\r\n", GET_LIQUID_FULL(gen), GET_LIQUID_THIRST(gen), GET_LIQUID_DRUNK(gen));
			break;
		}
		case GENERIC_ACTION: {
			size += snprintf(buf + size, sizeof(buf) - size, "Build-to-Char: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR)));
			size += snprintf(buf + size, sizeof(buf) - size, "Build-to-Room: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM)));
			size += snprintf(buf + size, sizeof(buf) - size, "Craft-to-Char: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR)));
			size += snprintf(buf + size, sizeof(buf) - size, "Craft-to-Room: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM)));
			size += snprintf(buf + size, sizeof(buf) - size, "Repair-to-Char: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR)));
			size += snprintf(buf + size, sizeof(buf) - size, "Repair-to-Room: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM)));
			break;
		}
		case GENERIC_COOLDOWN: {
			size += snprintf(buf + size, sizeof(buf) - size, "Wear-off: %s\r\n", GET_COOLDOWN_WEAR_OFF(gen) ? GET_COOLDOWN_WEAR_OFF(gen) : "(none)");
			break;
		}
		case GENERIC_AFFECT: {
			size += snprintf(buf + size, sizeof(buf) - size, "Apply to-char: %s\r\n", GET_AFFECT_APPLY_TO_CHAR(gen) ? GET_AFFECT_APPLY_TO_CHAR(gen) : "(none)");
			size += snprintf(buf + size, sizeof(buf) - size, "Apply to-room: %s\r\n", GET_AFFECT_APPLY_TO_ROOM(gen) ? GET_AFFECT_APPLY_TO_ROOM(gen) : "(none)");
			size += snprintf(buf + size, sizeof(buf) - size, "Wear-off: %s\r\n", GET_AFFECT_WEAR_OFF_TO_CHAR(gen) ? GET_AFFECT_WEAR_OFF_TO_CHAR(gen) : "(none)");
			size += snprintf(buf + size, sizeof(buf) - size, "Wear-off to room: %s\r\n", GET_AFFECT_WEAR_OFF_TO_ROOM(gen) ? GET_AFFECT_WEAR_OFF_TO_ROOM(gen) : "(none)");
			break;
		}
		case GENERIC_CURRENCY: {
			size += snprintf(buf + size, sizeof(buf) - size, "Singular: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_CURRENCY_SINGULAR)));
			size += snprintf(buf + size, sizeof(buf) - size, "Plural: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_CURRENCY_PLURAL)));
			break;
		}
		case GENERIC_COMPONENT: {
			size += snprintf(buf + size, sizeof(buf) - size, "Plural: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_COMPONENT_PLURAL)));
			size += snprintf(buf + size, sizeof(buf) - size, "Item: [%d] %s\r\n", GET_COMPONENT_OBJ_VNUM(gen), get_obj_name_by_proto(GET_COMPONENT_OBJ_VNUM(gen)));
			
			get_generic_relation_display(GEN_RELATIONS(gen), TRUE, part, NULL);
			size += snprintf(buf + size, sizeof(buf) - size, "Relations:\r\n%s", GEN_RELATIONS(gen) ? part : " none\r\n");
			get_generic_relation_display(GEN_COMPUTED_RELATIONS(gen), TRUE, part, NULL);
			size += snprintf(buf + size, sizeof(buf) - size, "Extended Relations:\r\n%s", GEN_COMPUTED_RELATIONS(gen) ? part : " none\r\n");
			break;
		}
	}

	page_string(ch->desc, buf, TRUE);
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
			msg_to_char(ch, "%3d. [%5d] %s (%s)\r\n", ++found, GEN_VNUM(iter), GEN_NAME(iter), generic_types[GEN_TYPE(iter)]);
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// GENERIC OLC MODULES /////////////////////////////////////////////////////

OLC_MODULE(genedit_flags) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	GEN_FLAGS(gen) = olc_process_flag(ch, argument, "generic", "flags", generic_flags, GEN_FLAGS(gen));
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


OLC_MODULE(genedit_drunk) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		GEN_VALUE(gen, DRUNK) = olc_process_number(ch, argument, "drunk", "drunk", -MAX_LIQUID_COND, MAX_LIQUID_COND, GET_LIQUID_DRUNK(gen));
	}
}


OLC_MODULE(genedit_hunger) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		GEN_VALUE(gen, FULL) = olc_process_number(ch, argument, "hunger", "hunger", -MAX_LIQUID_COND, MAX_LIQUID_COND, GET_LIQUID_FULL(gen));
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
		GEN_VALUE(gen, THIRST) = olc_process_number(ch, argument, "thirst", "thirst", -MAX_LIQUID_COND, MAX_LIQUID_COND, GET_LIQUID_THIRST(gen));
	}
}
