/* ************************************************************************
*   File: shop.c                                          EmpireMUD 2.0b5 *
*  Usage: loading, saving, OLC, and functions for shops                   *
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


/**
* Contents:
*   Helpers
*   Lookup Handlers
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   OLC Modules
*/

// local data
const char *default_shop_name = "Unnamed Shop";

// external consts
extern const char *olc_type_bits[NUM_OLC_TYPES+1];
extern struct faction_reputation_type reputation_levels[];
extern const char *shop_flags[];

// external funcs
extern struct quest_giver *copy_quest_givers(struct quest_giver *from);
extern bool find_quest_giver_in_list(struct quest_giver *list, int type, any_vnum vnum);
void free_quest_givers(struct quest_giver *list);
void get_quest_giver_display(struct quest_giver *list, char *save_buffer);

// local funcs
void add_shop_lookup(struct shop_lookup **list, shop_data *shop);
bool remove_shop_lookup(struct shop_lookup **list, shop_data *shop);
void update_mob_shop_lookups(mob_vnum vnum);
void update_obj_shop_lookups(obj_vnum vnum);
void update_vehicle_shop_lookups(any_vnum vnum);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Deletes entries by type+vnum.
*
* @param struct shop_item **list A pointer to the list to delete from.
* @param any_vnum vnum The obj vnum to remove.
* @return bool TRUE if the type+vnum was removed from the list. FALSE if not.
*/
bool delete_shop_item_from_list(struct shop_item **list, any_vnum vnum) {
	struct shop_item *iter, *next_iter;
	bool any = FALSE;
	
	LL_FOREACH_SAFE(*list, iter, next_iter) {
		if (iter->vnum == vnum) {
			any = TRUE;
			LL_DELETE(*list, iter);
			free(iter);
		}
	}
	
	return any;
}


/**
* @param struct shop_item *list A list to search.
* @param any_vnum vnum The currency vnum to look for.
* @return bool TRUE if the type+vnum is in the list. FALSE if not.
*/
bool find_currency_in_shop_item_list(struct shop_item *list, any_vnum vnum) {
	struct shop_item *iter;
	LL_FOREACH(list, iter) {
		if (iter->currency == vnum) {
			return TRUE;
		}
	}
	return FALSE;
}


/**
* @param struct shop_item *list A list to search.
* @param any_vnum vnum The obj vnum to look for.
* @return bool TRUE if the type+vnum is in the list. FALSE if not.
*/
bool find_shop_item_in_list(struct shop_item *list, any_vnum vnum) {
	struct shop_item *iter;
	LL_FOREACH(list, iter) {
		if (iter->vnum == vnum) {
			return TRUE;
		}
	}
	return FALSE;
}


/**
* Determines if a shop is open right now.
* 
* @param shop_data *shop The shop to check.
* @return bool TRUE if the shop is open (based on its hours)
*/
bool shop_is_open(shop_data *shop) {
	if (SHOP_OPEN_TIME(shop) == SHOP_CLOSE_TIME(shop)) {
		return TRUE;	// always
	}
	else if (SHOP_OPEN_TIME(shop) < SHOP_CLOSE_TIME(shop)) {
		return (time_info.hours >= SHOP_OPEN_TIME(shop) && time_info.hours < SHOP_CLOSE_TIME(shop));
	}
	else if (SHOP_OPEN_TIME(shop) > SHOP_CLOSE_TIME(shop)) {
		return (time_info.hours >= SHOP_OPEN_TIME(shop) || time_info.hours < SHOP_CLOSE_TIME(shop));
	}
	
	return FALSE;	// unreachable?
}


/**
* Copies entries from one list into another, only if they are not already in
* the to_list.
*
* @param struct shop_item **to_list A pointer to the destination list.
* @param struct shop_item *from_list The list to copy from.
*/
void smart_copy_shop_items(struct shop_item **to_list, struct shop_item *from_list) {
	struct shop_item *iter, *search, *item;
	bool found;
	
	LL_FOREACH(from_list, iter) {
		// ensure not already in list
		found = FALSE;
		LL_FOREACH(*to_list, search) {
			if (search->vnum == iter->vnum && search->cost == iter->cost && search->currency == iter->currency && search->min_rep == iter->min_rep) {
				found = TRUE;
				break;
			}
		}
		
		// add it
		if (!found) {
			CREATE(item, struct shop_item, 1);
			*item = *iter;
			item->next = NULL;
			LL_APPEND(*to_list, item);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// LOOKUP HANDLERS /////////////////////////////////////////////////////////

/**
* Processes all the shop into lookup hint tables.
*
* @param shop_data *shop The shop to update hints for.
* @param bool add If TRUE, adds shop lookup hints. If FALSE, removes them.
*/
void add_or_remove_all_shop_lookups_for(shop_data *shop, bool add) {
	struct quest_giver *giver;
	room_template *rmt;
	vehicle_data *veh;
	char_data *mob;
	bld_data *bld;
	obj_data *obj;
	
	if (!shop) {	// somehow
		return;
	}
	
	LL_FOREACH(SHOP_LOCATIONS(shop), giver) {
		// QG_x -- except trigger, quest
		switch (giver->type) {
			case QG_BUILDING: {
				if ((bld = building_proto(giver->vnum))) {
					if (add) {
						add_shop_lookup(&GET_BLD_SHOP_LOOKUPS(bld), shop);
					}
					else {
						remove_shop_lookup(&GET_BLD_SHOP_LOOKUPS(bld), shop);
					}
					// does not require live update
				}
				break;
			}
			case QG_MOBILE: {
				if ((mob = mob_proto(giver->vnum))) {
					if (add) {
						add_shop_lookup(&MOB_SHOP_LOOKUPS(mob), shop);
					}
					else {
						remove_shop_lookup(&MOB_SHOP_LOOKUPS(mob), shop);
					}
					update_mob_shop_lookups(GET_MOB_VNUM(mob));
				}
				break;
			}
			case QG_OBJECT: {
				if ((obj = obj_proto(giver->vnum))) {
					if (add) {
						add_shop_lookup(&GET_OBJ_SHOP_LOOKUPS(obj), shop);
					}
					else {
						remove_shop_lookup(&GET_OBJ_SHOP_LOOKUPS(obj), shop);
					}
					update_obj_shop_lookups(GET_OBJ_VNUM(obj));
				}
				break;
			}
			case QG_ROOM_TEMPLATE: {
				if ((rmt = room_template_proto(giver->vnum))) {
					if (add) {
						add_shop_lookup(&GET_RMT_SHOP_LOOKUPS(rmt), shop);
					}
					else {
						remove_shop_lookup(&GET_RMT_SHOP_LOOKUPS(rmt), shop);
					}
					// does not require live update
				}
				break;
			}
			case QG_VEHICLE: {
				if ((veh = vehicle_proto(giver->vnum))) {
					if (add) {
						add_shop_lookup(&VEH_SHOP_LOOKUPS(veh), shop);
					}
					else {
						remove_shop_lookup(&VEH_SHOP_LOOKUPS(veh), shop);
					}
					update_vehicle_shop_lookups(VEH_VNUM(veh));
				}
				break;
			}
		}
	}
}


/**
* Adds a shop lookup hint to a list (e.g. on a mob).
*
* Note: For mob/obj/veh shops, run update_mob_shop_lookups() etc after this.
*
* @param struct shop_lookup **list A pointer to the list to add to.
* @param shop_data *shop The shop to add.
*/
void add_shop_lookup(struct shop_lookup **list, shop_data *shop) {
	struct shop_lookup *sl;
	bool found = FALSE;
	
	if (list && shop) {
		// no dupes
		LL_FOREACH(*list, sl) {
			if (sl->shop == shop) {
				found = TRUE;
			}
		}
		
		if (!found) {
			CREATE(sl, struct shop_lookup, 1);
			sl->shop = shop;
			LL_PREPEND(*list, sl);
		}
	}
}


/**
* Adds a shop to a temporary shop list (don't forget to free the list later).
*
* @param struct shop_temp_list **shop_list A pointer to the list to add to.
* @param shop_data *shop The shop to add (will check duplicates automatically).
* @param char_data *mob The mob vendor, IF any.
* @param obj_data *obj The obj vendor, IF any.
* @param room_data *room The room vendor, IF any.
* @param vehicle_data *veh The vehicle vendor, IF any.
*/
void add_temp_shop(struct shop_temp_list **shop_list, shop_data *shop, char_data *mob, obj_data *obj, room_data *room, vehicle_data *veh) {
	struct shop_temp_list *stl;
	bool found = FALSE;
	
	// ensure it's not already in the list
	LL_FOREACH(*shop_list, stl) {
		if (stl->shop == shop) {
			found = TRUE;
			break;
		}
	}
	
	if (!found) {
		CREATE(stl, struct shop_temp_list, 1);
		stl->shop = shop;
		stl->from_mob = mob;
		stl->from_obj = obj;
		stl->from_room = room;
		stl->from_veh = veh;
		LL_APPEND(*shop_list, stl);
	}
}


/**
* Builds all the shop lookup tables on startup.
*/
void build_all_shop_lookups(void) {
	shop_data *shop, *next_shop;
	HASH_ITER(hh, shop_table, shop, next_shop) {
		add_or_remove_all_shop_lookups_for(shop, TRUE);
	}
}


/**
* Finds all available shops at a character's location. You should use
* free_shop_temp_list() when you're done with this list.
*
* @param char_data *ch The player looking for shops.
* @return struct shop_temp_list* The shop list.
*/
struct shop_temp_list *build_available_shop_list(char_data *ch) {
	struct shop_temp_list *shop_list = NULL;
	struct shop_lookup *sl;
	vehicle_data *veh;
	char_data *mob;
	obj_data *obj;
	
	// mobs
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), mob, next_in_room) {
		if (!IS_NPC(mob) || EXTRACTED(mob) || !CAN_SEE(ch, mob) || FIGHTING(mob) || GET_POS(mob) < POS_RESTING || IS_DEAD(mob)) {
			continue;
		}
		LL_FOREACH(MOB_SHOP_LOOKUPS(mob), sl) {
			if (shop_is_open(sl->shop) && find_quest_giver_in_list(SHOP_LOCATIONS(sl->shop), QG_MOBILE, GET_MOB_VNUM(mob))) {
				add_temp_shop(&shop_list, sl->shop, mob, NULL, NULL, NULL);
			}
		}
	}
	
	// search in inventory
	LL_FOREACH2(ch->carrying, obj, next_content) {
		if (!CAN_SEE_OBJ(ch, obj)) {
			continue;
		}
		LL_FOREACH(GET_OBJ_SHOP_LOOKUPS(obj), sl) {
			if (shop_is_open(sl->shop) && find_quest_giver_in_list(SHOP_LOCATIONS(sl->shop), QG_OBJECT, GET_OBJ_VNUM(obj))) {
				add_temp_shop(&shop_list, sl->shop, NULL, obj, NULL, NULL);
			}
		}
	}
	
	// objs in room
	LL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_content) {
		if (!CAN_SEE_OBJ(ch, obj)) {
			continue;
		}
		LL_FOREACH(GET_OBJ_SHOP_LOOKUPS(obj), sl) {
			if (shop_is_open(sl->shop) && find_quest_giver_in_list(SHOP_LOCATIONS(sl->shop), QG_OBJECT, GET_OBJ_VNUM(obj))) {
				add_temp_shop(&shop_list, sl->shop, NULL, obj, NULL, NULL);
			}
		}
	}
	
	// vehicles
	LL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
		if (!CAN_SEE_VEHICLE(ch, veh)) {
			continue;
		}
		LL_FOREACH(VEH_SHOP_LOOKUPS(veh), sl) {
			if (shop_is_open(sl->shop) && find_quest_giver_in_list(SHOP_LOCATIONS(sl->shop), QG_VEHICLE, VEH_VNUM(veh))) {
				add_temp_shop(&shop_list, sl->shop, NULL, NULL, NULL, veh);
			}
		}
	}
	
	// rooms
	if (CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		// search room: building
		if (GET_BUILDING(IN_ROOM(ch))) {
			LL_FOREACH(GET_BLD_SHOP_LOOKUPS(GET_BUILDING(IN_ROOM(ch))), sl) {
				if (shop_is_open(sl->shop) && find_quest_giver_in_list(SHOP_LOCATIONS(sl->shop), QG_BUILDING, GET_BLD_VNUM(GET_BUILDING(IN_ROOM(ch))))) {
					add_temp_shop(&shop_list, sl->shop, NULL, NULL, IN_ROOM(ch), NULL);
				}
			}
		}
		
		// search room: template
		if (GET_ROOM_TEMPLATE(IN_ROOM(ch))) {
			LL_FOREACH(GET_RMT_SHOP_LOOKUPS(GET_ROOM_TEMPLATE(IN_ROOM(ch))), sl) {
				if (shop_is_open(sl->shop) && find_quest_giver_in_list(SHOP_LOCATIONS(sl->shop), QG_ROOM_TEMPLATE, GET_RMT_VNUM(GET_ROOM_TEMPLATE(IN_ROOM(ch))))) {
					add_temp_shop(&shop_list, sl->shop, NULL, NULL, IN_ROOM(ch), NULL);
				}
			}
		}
	}
	
	return shop_list;
}


/**
* Frees a temporary shop list.
*
* @param struct shop_temp_list *list The list to free.
*/
void free_shop_temp_list(struct shop_temp_list *list) {
	struct shop_temp_list *stl, *next_stl;
	LL_FOREACH_SAFE(list, stl, next_stl) {
		free(stl);
	}
}


/**
* Adds a shop lookup hint to a list (e.g. on a mob).
*
* Note: For mob/obj/veh shop, run update_mob_shop_lookups() etc after this.
*
* @param struct shop_lookup **list A pointer to the list to add to.
* @param shop_data *shop The shop to add.
* @return bool TRUE if it removed an entry, FALSE for no matches.
*/
bool remove_shop_lookup(struct shop_lookup **list, shop_data *shop) {
	struct shop_lookup *sl, *next_sl;
	bool any = FALSE;
	
	if (list && *list && shop) {
		LL_FOREACH_SAFE(*list, sl, next_sl) {
			if (sl->shop == shop) {
				LL_DELETE(*list, sl);
				free(sl);
				any = TRUE;
			}
		}
	}
	
	return any;
}


/**
* Fixes shop lookup pointers on live copies of mobs -- this should ALWAYS
* point to the proto.
*/
void update_mob_shop_lookups(mob_vnum vnum) {
	char_data *proto, *mob;
	
	if (!(proto = mob_proto(vnum))) {
		return;
	}
	
	LL_FOREACH(character_list, mob) {
		if (IS_NPC(mob) && GET_MOB_VNUM(mob) == vnum) {
			// re-set the pointer
			MOB_SHOP_LOOKUPS(mob) = MOB_SHOP_LOOKUPS(proto);
		}
	}
}


/**
* Fixes shop lookup pointers on live copies of objs -- this should ALWAYS
* point to the proto.
*/
void update_obj_shop_lookups(obj_vnum vnum) {
	obj_data *proto, *obj;
	
	if (!(proto = obj_proto(vnum))) {
		return;
	}
	
	LL_FOREACH(object_list, obj) {
		if (GET_OBJ_VNUM(obj) == vnum) {
			// re-set the pointer
			GET_OBJ_SHOP_LOOKUPS(obj) = GET_OBJ_SHOP_LOOKUPS(proto);
		}
	}
}


/**
* Fixes shop lookup pointers on live copies of vehicles -- this should ALWAYS
* point to the proto.
*/
void update_vehicle_shop_lookups(mob_vnum vnum) {
	vehicle_data *proto, *veh;
	
	if (!(proto = vehicle_proto(vnum))) {
		return;
	}
	
	LL_FOREACH(vehicle_list, veh) {
		if (VEH_VNUM(veh) == vnum) {
			// re-set the pointer
			VEH_SHOP_LOOKUPS(veh) = VEH_SHOP_LOOKUPS(proto);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common shops problems and reports them to ch.
*
* @param shop_data *shop The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_shop(shop_data *shop, char_data *ch) {
	struct shop_item *item, *dupe;
	bool problem = FALSE;
	
	if (SHOP_FLAGGED(shop, SHOP_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, SHOP_VNUM(shop), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	
	if (!SHOP_NAME(shop) || !*SHOP_NAME(shop) || !str_cmp(SHOP_NAME(shop), default_shop_name)) {
		olc_audit_msg(ch, SHOP_VNUM(shop), "No name set");
		problem = TRUE;
	}
	
	if (!SHOP_LOCATIONS(shop)) {
		olc_audit_msg(ch, SHOP_VNUM(shop), "No locations");
		problem = TRUE;
	}
	
	LL_FOREACH(SHOP_ITEMS(shop), item) {
		if (!obj_proto(item->vnum)) {
			olc_audit_msg(ch, SHOP_VNUM(shop), "Bad item vnum %d", item->vnum);
			problem = TRUE;
			continue;	// only relevant error
		}
		if (item->currency != NOTHING && !find_generic(item->currency, GENERIC_CURRENCY)) {
			olc_audit_msg(ch, SHOP_VNUM(shop), "Bad currency vnum %d", item->currency);
			problem = TRUE;
		}
		if (item->min_rep != REP_NONE && !SHOP_ALLEGIANCE(shop)) {
			olc_audit_msg(ch, SHOP_VNUM(shop), "Minimum reputation set with no allegiance (%s)", get_obj_name_by_proto(item->vnum));
			problem = TRUE;
		}
		
		// duplicate check
		LL_FOREACH(SHOP_ITEMS(shop), dupe) {
			if (dupe->vnum != item->vnum) {
				continue;	// looking for same vnum
			}
			if (dupe == item) {
				continue;	// but not same item
			}
			
			// so it's basically a full duplicate
			if (dupe->currency == item->currency) {
				olc_audit_msg(ch, SHOP_VNUM(shop), "Possible duplicate item %d %s", item->vnum, get_obj_name_by_proto(item->vnum));
				problem = TRUE;
				break;
			}
		}
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param shop_data *shop The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_shop(shop_data *shop, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s%s", SHOP_VNUM(shop), SHOP_NAME(shop), SHOP_FLAGGED(shop, SHOP_IN_DEVELOPMENT) ? " (IN-DEV)" : "");
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s%s", SHOP_VNUM(shop), SHOP_NAME(shop), SHOP_FLAGGED(shop, SHOP_IN_DEVELOPMENT) ? " (IN-DEV)" : "");
	}
		
	return output;
}


/**
* Searches for all uses of a shop and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The shop vnum.
*/
void olc_search_shop(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	shop_data *shop = real_shop(vnum);
	int size, found;
	
	if (!shop) {
		msg_to_char(ch, "There is no shop %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of shop %d (%s):\r\n", vnum, SHOP_NAME(shop));
	
	// none yet
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


// Simple vnum sorter for the shops hash
int sort_shops(shop_data *a, shop_data *b) {
	return SHOP_VNUM(a) - SHOP_VNUM(b);
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts a shop into the hash table.
*
* @param shop_data *shop The shop data to add to the table.
*/
void add_shop_to_table(shop_data *shop) {
	shop_data *find;
	any_vnum vnum;
	
	if (shop) {
		vnum = SHOP_VNUM(shop);
		HASH_FIND_INT(shop_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(shop_table, vnum, shop);
			HASH_SORT(shop_table, sort_shops);
		}
	}
}


/**
* Removes a shop from the hash table.
*
* @param shop_data *shop The shop data to remove from the table.
*/
void remove_shop_from_table(shop_data *shop) {
	HASH_DEL(shop_table, shop);
}


/**
* Initializes a new shop. This clears all memory for it, so set the vnum
* AFTER.
*
* @param shop_data *shop The shop to initialize.
*/
void clear_shop(shop_data *shop) {
	memset((char *) shop, 0, sizeof(shop_data));
	
	SHOP_VNUM(shop) = NOTHING;
}


/**
* @param struct shop_item *from The list to copy.
* @return struct shop_item* The copy of the list.
*/
struct shop_item *copy_shop_item_list(struct shop_item *from) {
	struct shop_item *el, *iter, *list = NULL, *end = NULL;
	
	LL_FOREACH(from, iter) {
		CREATE(el, struct shop_item, 1);
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
* Frees a set of shop items.
*
* @param struct shop_item *list The list to free.
*/
void free_shop_item_list(struct shop_item *list) {
	struct shop_item *iter;
	while ((iter = list)) {
		list = iter->next;
		free(iter);
	}
}


/**
* frees up memory for a shop data item.
*
* See also: olc_delete_shop
*
* @param shop_data *shop The shop data to free.
*/
void free_shop(shop_data *shop) {
	shop_data *proto = real_shop(SHOP_VNUM(shop));
	
	if (SHOP_NAME(shop) && (!proto || SHOP_NAME(shop) != SHOP_NAME(proto))) {
		free(SHOP_NAME(shop));
	}
	
	if (SHOP_ITEMS(shop) && (!proto || SHOP_ITEMS(shop) != SHOP_ITEMS(proto))) {
		free_shop_item_list(SHOP_ITEMS(shop));
	}
	if (SHOP_LOCATIONS(shop) && (!proto || SHOP_LOCATIONS(shop) != SHOP_LOCATIONS(proto))) {
		free_quest_givers(SHOP_LOCATIONS(shop));
	}
	
	free(shop);
}


/**
* Read one shop from file.
*
* @param FILE *fl The open .shop file
* @param any_vnum vnum The shop vnum
*/
void parse_shop(FILE *fl, any_vnum vnum) {
	void parse_quest_giver(FILE *fl, struct quest_giver **list, char *error_str);
	
	char line[256], error[256], str_in[256];
	struct shop_item *item;
	shop_data *shop, *find;
	int int_in[4];
	
	CREATE(shop, shop_data, 1);
	clear_shop(shop);
	SHOP_VNUM(shop) = vnum;
	
	HASH_FIND_INT(shop_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate shop vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_shop_to_table(shop);
		
	// for error messages
	sprintf(error, "shop vnum %d", vnum);
	
	// line 1: name
	SHOP_NAME(shop) = fread_string(fl, error);
	
	// line 2: allegiance open close flags
	if (!get_line(fl, line) || sscanf(line, "%d %d %d %s", &int_in[0], &int_in[1], &int_in[2], str_in) != 4) {
		log("SYSERR: Format error in line 2 of %s", error);
		exit(1);
	}
	
	SHOP_ALLEGIANCE(shop) = find_faction_by_vnum(int_in[0]);
	SHOP_OPEN_TIME(shop) = int_in[1];
	SHOP_CLOSE_TIME(shop) = int_in[2];
	SHOP_FLAGS(shop) = asciiflag_conv(str_in);
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'I': {	// item
				if (sscanf(line, "I %d %d %d %d\n", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
					log("SYSERR: Format error in I line of %s", error);
					exit(1);
				}
				
				CREATE(item, struct shop_item, 1);
				item->vnum = int_in[0];
				item->cost = int_in[1];
				item->currency = int_in[2];
				item->min_rep = int_in[3];
				
				LL_APPEND(SHOP_ITEMS(shop), item);
				break;
			}
			case 'L': {	// locations
				parse_quest_giver(fl, &SHOP_LOCATIONS(shop), error);
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
* @param any_vnum vnum Any shop vnum
* @return shop_data* The shop, or NULL if it doesn't exist
*/
shop_data *real_shop(any_vnum vnum) {
	shop_data *shop;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(shop_table, &vnum, shop);
	return shop;
}


// writes entries in the shop index
void write_shop_index(FILE *fl) {
	shop_data *shop, *next_shop;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, shop_table, shop, next_shop) {
		// determine "zone number" by vnum
		this = (int)(SHOP_VNUM(shop) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, SHOP_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one shop item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param shop_data *shop The thing to save.
*/
void write_shop_to_file(FILE *fl, shop_data *shop) {
	void write_quest_givers_to_file(FILE *fl, char letter, struct quest_giver *list);
	
	struct shop_item *item;
	char temp[256];
	
	if (!fl || !shop) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_shop_to_file called without %s", !fl ? "file" : "shop");
		return;
	}
	
	fprintf(fl, "#%d\n", SHOP_VNUM(shop));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(SHOP_NAME(shop)));
	
	// 2. allegiance open close flags
	strcpy(temp, bitv_to_alpha(SHOP_FLAGS(shop)));
	fprintf(fl, "%d %d %d %s\n", SHOP_ALLEGIANCE(shop) ? FCT_VNUM(SHOP_ALLEGIANCE(shop)) : NOTHING, SHOP_OPEN_TIME(shop), SHOP_CLOSE_TIME(shop), temp);
	
	// 'I' items
	LL_FOREACH(SHOP_ITEMS(shop), item) {
		fprintf(fl, "I %d %d %d %d\n", item->vnum, item->cost, item->currency, item->min_rep);
	}
	
	// 'L' locations
	write_quest_givers_to_file(fl, 'L', SHOP_LOCATIONS(shop));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////

/**
* Creates a new shop entry.
* 
* @param any_vnum vnum The number to create.
* @return shop_data* The new shop's prototype.
*/
shop_data *create_shop_table_entry(any_vnum vnum) {
	shop_data *shop;
	
	// sanity
	if (real_shop(vnum)) {
		log("SYSERR: Attempting to insert shop at existing vnum %d", vnum);
		return real_shop(vnum);
	}
	
	CREATE(shop, shop_data, 1);
	clear_shop(shop);
	SHOP_VNUM(shop) = vnum;
	SHOP_NAME(shop) = str_dup(default_shop_name);
	add_shop_to_table(shop);

	// save index and shop file now
	save_index(DB_BOOT_SHOP);
	save_library_file_for_vnum(DB_BOOT_SHOP, vnum);

	return shop;
}


/**
* WARNING: This function actually deletes a shop.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_shop(char_data *ch, any_vnum vnum) {
	shop_data *shop;
	
	if (!(shop = real_shop(vnum))) {
		msg_to_char(ch, "There is no such shop %d.\r\n", vnum);
		return;
	}
	
	// removing live instances goes here
	
	// remove it from the hash table first
	remove_shop_from_table(shop);

	// save index and shop file now
	save_index(DB_BOOT_SHOP);
	save_library_file_for_vnum(DB_BOOT_SHOP, vnum);
	
	// delete from lookups
	add_or_remove_all_shop_lookups_for(shop, FALSE);
	
	// removing from prototypes goes here
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted shop %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Shop %d deleted.\r\n", vnum);
	
	free_shop(shop);
}


/**
* Function to save a player's changes to a shop (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_shop(descriptor_data *desc) {	
	shop_data *proto, *shop = GET_OLC_SHOP(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh;

	// have a place to save it?
	if (!(proto = real_shop(vnum))) {
		proto = create_shop_table_entry(vnum);
	}
	
	// delete from lookups FIRST
	add_or_remove_all_shop_lookups_for(proto, FALSE);
	
	// free prototype strings and pointers
	if (SHOP_NAME(proto)) {
		free(SHOP_NAME(proto));
	}
	free_shop_item_list(SHOP_ITEMS(proto));
	free_quest_givers(SHOP_LOCATIONS(proto));
	
	// sanity
	if (!SHOP_NAME(shop) || !*SHOP_NAME(shop)) {
		if (SHOP_NAME(shop)) {
			free(SHOP_NAME(shop));
		}
		SHOP_NAME(shop) = str_dup(default_shop_name);
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handles
	*proto = *shop;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handles
	
	// re-add lookups
	add_or_remove_all_shop_lookups_for(proto, TRUE);
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_SHOP, vnum);
}


/**
* Creates a copy of a shop, or clears a new one, for editing.
* 
* @param shop_data *input The shop to copy, or NULL to make a new one.
* @return shop_data* The copied shop.
*/
shop_data *setup_olc_shop(shop_data *input) {
	extern struct req_data *copy_requirements(struct req_data *from);
	
	shop_data *new;
	
	CREATE(new, shop_data, 1);
	clear_shop(new);
	
	if (input) {
		// copy normal data
		*new = *input;
		
		// copy things that are pointers
		SHOP_NAME(new) = SHOP_NAME(input) ? str_dup(SHOP_NAME(input)) : NULL;
		SHOP_ITEMS(new) = copy_shop_item_list(SHOP_ITEMS(input));
		SHOP_LOCATIONS(new) = copy_quest_givers(SHOP_LOCATIONS(input));
	}
	else {
		// brand new: some defaults
		SHOP_NAME(new) = str_dup(default_shop_name);
		SHOP_FLAGS(new) = SHOP_IN_DEVELOPMENT;
	}
	
	// done
	return new;
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* Gets the display for a set of shop items.
*
* @param shop_data *shop The shop being shown.
* @param struct shop_item *list Pointer to the start of a list of shop items.
* @param char *save_buffer A buffer to store the result to.
*/
void get_shop_items_display(shop_data *shop, char *save_buffer) {
	char buf[MAX_STRING_LENGTH];
	struct shop_item *item;
	int count = 0;
	
	*save_buffer = '\0';
	LL_FOREACH(SHOP_ITEMS(shop), item) {
		if (SHOP_ALLEGIANCE(shop)) {
			snprintf(buf, sizeof(buf), " (%s)", reputation_levels[rep_const_to_index(item->min_rep)].name);
		}
		else {
			*buf = '\0';
		}
		
		sprintf(save_buffer + strlen(save_buffer), "%2d. [%5d] %s for %d %s%s\r\n", ++count, item->vnum, get_obj_name_by_proto(item->vnum), item->cost, (item->currency == NOTHING ? "coins" : get_generic_string_by_vnum(item->currency, GENERIC_CURRENCY, WHICH_CURRENCY(item->cost))), buf);
	}
	
	// empty list not shown
}


/**
* For vstat.
*
* @param char_data *ch The player requesting stats.
* @param shop_data *shop The shop to display.
*/
void do_stat_shop(char_data *ch, shop_data *shop) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	size_t size;
	
	if (!shop) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \ty%s\t0\r\n", SHOP_VNUM(shop), SHOP_NAME(shop));
	
	// 2nd line
	if (SHOP_OPEN_TIME(shop) == SHOP_CLOSE_TIME(shop)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Times: [\tcalways open\t0]");
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, "Times: [\tc%d%s\t0 - \tc%d%s\t0]", TIME_TO_12H(SHOP_OPEN_TIME(shop)), AM_PM(SHOP_OPEN_TIME(shop)), TIME_TO_12H(SHOP_CLOSE_TIME(shop)), AM_PM(SHOP_CLOSE_TIME(shop)));
	}
	// still 2nd line
	size += snprintf(buf + size, sizeof(buf) - size, ", Faction allegiance: [\ty%s\t0]\r\n", SHOP_ALLEGIANCE(shop) ? FCT_NAME(SHOP_ALLEGIANCE(shop)) : "none");
	
	sprintbit(SHOP_FLAGS(shop), shop_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	get_quest_giver_display(SHOP_LOCATIONS(shop), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Locations:\r\n%s", part);
	
	get_shop_items_display(shop, part);
	sprintf(buf + strlen(buf), "Items:\r\n%s", part);
	
	page_string(ch->desc, buf, TRUE);
}


/**
* This is the main recipe display for shop OLC. It displays the user's
* currently-edited shop.
*
* @param char_data *ch The person who is editing a shop and will see its display.
*/
void olc_show_shop(char_data *ch) {
	shop_data *shop = GET_OLC_SHOP(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	
	if (!shop) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !real_shop(SHOP_VNUM(shop)) ? "new shop" : SHOP_NAME(real_shop(SHOP_VNUM(shop))));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(SHOP_NAME(shop), default_shop_name), NULLSAFE(SHOP_NAME(shop)));
	
	sprintbit(SHOP_FLAGS(shop), shop_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT), lbuf);
	
	sprintf(buf + strlen(buf), "<%sopens\t0> %d%s%s\r\n", OLC_LABEL_VAL(SHOP_OPEN_TIME(shop), 0), TIME_TO_12H(SHOP_OPEN_TIME(shop)), AM_PM(SHOP_OPEN_TIME(shop)), (SHOP_OPEN_TIME(shop) == SHOP_CLOSE_TIME(shop)) ? " (always open)" : "");
	sprintf(buf + strlen(buf), "<%scloses\t0> %d%s\r\n", OLC_LABEL_VAL(SHOP_CLOSE_TIME(shop), 0), TIME_TO_12H(SHOP_CLOSE_TIME(shop)), AM_PM(SHOP_CLOSE_TIME(shop)));
	sprintf(buf + strlen(buf), "<%sallegiance\t0> %s\r\n", OLC_LABEL_PTR(SHOP_ALLEGIANCE(shop)), SHOP_ALLEGIANCE(shop) ? FCT_NAME(SHOP_ALLEGIANCE(shop)) : "none");
	
	get_quest_giver_display(SHOP_LOCATIONS(shop), lbuf);
	sprintf(buf + strlen(buf), "Locations: <%slocation\t0>\r\n%s", OLC_LABEL_PTR(SHOP_LOCATIONS(shop)), lbuf);
	
	get_shop_items_display(shop, lbuf);
	sprintf(buf + strlen(buf), "Items: <%sitem\t0>\r\n%s", OLC_LABEL_PTR(SHOP_ITEMS(shop)), lbuf);
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the shop db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_shop(char *searchname, char_data *ch) {
	shop_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, shop_table, iter, next_iter) {
		if (multi_isname(searchname, SHOP_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, SHOP_VNUM(iter), SHOP_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// SHOP OLC MODULES ////////////////////////////////////////////////////////

OLC_MODULE(shopedit_allegiance) {
	shop_data *shop = GET_OLC_SHOP(ch->desc);
	faction_data *fct;
	
	if (!*argument) {
		msg_to_char(ch, "Set the shop's allegiance to which faction (or 'none')?\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		msg_to_char(ch, "You set its allegiance to 'none'.\r\n");
		SHOP_ALLEGIANCE(shop) = NULL;
	}
	else if (!(fct = find_faction(argument))) {
		msg_to_char(ch, "Unknown faction '%s'.\r\n", argument);
	}
	else {
		SHOP_ALLEGIANCE(shop) = fct;
		msg_to_char(ch, "You set its allegiance to %s.\r\n", FCT_NAME(fct));
	}
}


OLC_MODULE(shopedit_closes) {
	shop_data *shop = GET_OLC_SHOP(ch->desc);
	int hour;
	
	if (!isdigit(*argument) || (hour = atoi(argument)) < 0 || hour > 23) {
		msg_to_char(ch, "Time must be an hour between 0 and 23, or 1-12am or pm.\r\n");
		return;
	}
	
	if (strstr(argument, "am") && hour >= 1 && hour <= 12) {
		SHOP_CLOSE_TIME(shop) = (hour == 12 ? 0 : hour);
	}
	else if (strstr(argument, "pm") && hour >= 1 && hour <= 12) {
		SHOP_CLOSE_TIME(shop) = (hour == 12 ? hour : (hour + 12));
	}
	else {
		SHOP_CLOSE_TIME(shop) = hour;
	}
	
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		msg_to_char(ch, "It now closes at %d%s.\r\n", TIME_TO_12H(SHOP_CLOSE_TIME(shop)), AM_PM(SHOP_CLOSE_TIME(shop)));
	}
}


OLC_MODULE(shopedit_flags) {
	shop_data *shop = GET_OLC_SHOP(ch->desc);
	bool had_indev = IS_SET(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT) ? TRUE : FALSE;
	SHOP_FLAGS(shop) = olc_process_flag(ch, argument, "shop", "flags", shop_flags, SHOP_FLAGS(shop));
		
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !SHOP_FLAGGED(shop, SHOP_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
	}
}


OLC_MODULE(shopedit_items) {
	shop_data *shop = GET_OLC_SHOP(ch->desc);

	char cmd_arg[MAX_INPUT_LENGTH], field_arg[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH];
	char vnum_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	char cost_arg[MAX_INPUT_LENGTH], cur_arg[MAX_INPUT_LENGTH];
	char rep_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	struct shop_item *item, *iter, *change, *copyfrom;
	int findtype, num, cost, rep;
	any_vnum vnum, cur_vnum;
	bool found;
	
	argument = any_one_arg(argument, cmd_arg);	// add/remove/change/copy
	
	if (is_abbrev(cmd_arg, "copy")) {
		argument = any_one_arg(argument, type_arg);	// just "quest" for now
		argument = any_one_arg(argument, vnum_arg);	// any vnum for that type
		
		if (!*type_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: item copy <from type> <from vnum>\r\n");
		}
		else if ((findtype = find_olc_type(type_arg)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", type_arg);
		}
		else if (!isdigit(*vnum_arg)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy from which %s?\r\n", buf);
		}
		else if ((vnum = atoi(vnum_arg)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			copyfrom = NULL;
			
			switch (findtype) {
				case OLC_SHOP: {
					shop_data *find = real_shop(vnum);
					if (find) {
						copyfrom = SHOP_ITEMS(find);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy items from %ss.\r\n", buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, vnum_arg);
			}
			else {
				smart_copy_shop_items(&SHOP_ITEMS(shop), copyfrom);
				msg_to_char(ch, "Copied items from %s %d.\r\n", buf, vnum);
			}
		}
	}	// end 'copy'
	else if (is_abbrev(cmd_arg, "remove")) {
		skip_spaces(&argument);	// only arg is number
		
		if (!*argument) {
			msg_to_char(ch, "Remove which item (number)?\r\n");
		}
		else if (!str_cmp(argument, "all")) {
			free_shop_item_list(SHOP_ITEMS(shop));
			SHOP_ITEMS(shop) = NULL;
			msg_to_char(ch, "You remove all the items.\r\n");
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid item number.\r\n");
		}
		else {
			found = FALSE;
			LL_FOREACH(SHOP_ITEMS(shop), iter) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the item: %d %s.\r\n", iter->vnum, get_obj_name_by_proto(iter->vnum));
					LL_DELETE(SHOP_ITEMS(shop), iter);
					free(iter);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid item number.\r\n");
			}
		}
	}	// end 'remove'
	else if (is_abbrev(cmd_arg, "add")) {
		argument = any_one_arg(argument, vnum_arg);
		argument = any_one_arg(argument, cost_arg);
		argument = any_one_arg(argument, cur_arg);
		argument = any_one_arg(argument, rep_arg);
		cur_vnum = NOTHING;	// defaults to this if coins
		rep = REP_NONE;	// does not require rep
		
		if (!*vnum_arg || !*cost_arg || !*cur_arg) {
			msg_to_char(ch, "Usage: item add <vnum> <cost> <currency vnum | coins> [min reputation]\r\n");
		}
		else if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0 || !obj_proto(vnum)) {
			msg_to_char(ch, "Invalid vnum '%s'.\r\n", vnum_arg);
		}
		else if (!isdigit(*cost_arg) || (cost = atoi(cost_arg)) < 1) {
			msg_to_char(ch, "Invalid cost '%s'.\r\n", cost_arg);
		}
		else if (str_cmp(cur_arg, "coins") && ((cur_vnum = atoi(cur_arg)) < 0 || !find_generic(cur_vnum, GENERIC_CURRENCY))) {
			msg_to_char(ch, "Invalid currency '%s'. Specify a currency vnum or 'coins'.\r\n", cur_arg);
		}
		else if (*rep_arg && (rep = get_reputation_by_name(rep_arg)) == NOTHING) {
			msg_to_char(ch, "Invalid minimum reputation '%s'.\r\n", rep_arg);
		}
		else {
			// success
			CREATE(item, struct shop_item, 1);
			item->vnum = vnum;
			item->cost = cost;
			item->currency = cur_vnum;
			item->min_rep = rep;
			
			LL_APPEND(SHOP_ITEMS(shop), item);
			
			if (rep) {
				sprintf(buf, " (%s)", reputation_levels[rep_const_to_index(item->min_rep)].name);
			}
			else {
				*buf = '\0';
			}
			msg_to_char(ch, "You add item: [%d] %s for %d %s%s\r\n", vnum, get_obj_name_by_proto(vnum), cost, cur_vnum == NOTHING ? "coins" : get_generic_string_by_vnum(cur_vnum, GENERIC_CURRENCY, WHICH_CURRENCY(cost)), buf);
		}
	}	// end 'add'
	else if (is_abbrev(cmd_arg, "change")) {
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		argument = any_one_arg(argument, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*field_arg || !*val_arg) {
			msg_to_char(ch, "Usage: item change <number> <vnum | cost | currency | reputation> <value>\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		LL_FOREACH(SHOP_ITEMS(shop), iter) {
			if (--num == 0) {
				change = iter;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid item number.\r\n");
		}
		else if (is_abbrev(field_arg, "vnum")) {
			if (!isdigit(*val_arg) || (vnum = atoi(val_arg)) < 0 || !obj_proto(vnum)) {
				msg_to_char(ch, "Invalid vnum '%s'.\r\n", val_arg);
				return;
			}
			
			change->vnum = vnum;
			msg_to_char(ch, "Changed item %d to: [%d] %s\r\n", atoi(num_arg), vnum, get_obj_name_by_proto(vnum));
		}
		else if (is_abbrev(field_arg, "cost")) {
			if (!isdigit(*val_arg) || (cost = atoi(val_arg)) < 1) {
				msg_to_char(ch, "Invalid cost '%s'.\r\n", val_arg);
				return;
			}
			
			change->cost = cost;
			msg_to_char(ch, "Changed item %d's cost to: %d\r\n", atoi(num_arg), cost);
		}
		else if (is_abbrev(field_arg, "currency")) {
			if (!str_cmp(val_arg, "coins")) {
				cur_vnum = NOTHING;
			}
			else if (!isdigit(*val_arg) || (cur_vnum = atoi(val_arg)) < 0 || !find_generic(cur_vnum, GENERIC_CURRENCY)) {
				msg_to_char(ch, "Invalid currency vnum '%s'.\r\n", val_arg);
				return;
			}
			
			change->currency = cur_vnum;
			msg_to_char(ch, "Changed item %d's currency to: [%d] %s\r\n", atoi(num_arg), cur_vnum, get_generic_name_by_vnum(cur_vnum));
		}
		else if (is_abbrev(field_arg, "reputation")) {
			if (!str_cmp(val_arg, "none")) {
				rep = REP_NONE;
			}
			else if ((rep = get_reputation_by_name(val_arg)) == NOTHING) {
				msg_to_char(ch, "Invalid reputation '%s'.\r\n", val_arg);
				return;
			}
			
			change->min_rep = rep;
			msg_to_char(ch, "Changed item %d's minimum reputation to: %s\r\n", atoi(num_arg), rep == REP_NONE ? "none" : reputation_levels[rep_const_to_index(rep)].name);
		}
		else {
			msg_to_char(ch, "You can only change the vnum, cost, currency, or reputation.\r\n");
		}
	}	// end 'change'

	else if (is_abbrev(cmd_arg, "move")) {
		struct shop_item *to_move, *prev, *next;
		bool up;
		
		// usage: item move <number> <up | down>
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		up = is_abbrev(field_arg, "up");
		
		if (!*num_arg || !*field_arg) {
			msg_to_char(ch, "Usage: item move <number> <up | down>\r\n");
		}
		else if (!isdigit(*num_arg) || (num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid item number.\r\n");
		}
		else if (!is_abbrev(field_arg, "up") && !is_abbrev(field_arg, "down")) {
			msg_to_char(ch, "You must specify whether you're moving it up or down in the list.\r\n");
		}
		else if (up && num == 1) {
			msg_to_char(ch, "You can't move it up; it's already at the top of the list.\r\n");
		}
		else {
			// find the one to move
			to_move = prev = NULL;
			for (item = SHOP_ITEMS(shop); item && !to_move; item = item->next) {
				if (--num == 0) {
					to_move = item;
				}
				else {
					// store for next iteration
					prev = item;
				}
			}
			
			if (!to_move) {
				msg_to_char(ch, "Invalid item number.\r\n");
			}
			else if (!up && !to_move->next) {
				msg_to_char(ch, "You can't move it down; it's already at the bottom of the list.\r\n");
			}
			else {
				// SUCCESS: "move" them by swapping data
				if (up) {
					LL_DELETE(SHOP_ITEMS(shop), to_move);
					LL_PREPEND_ELEM(SHOP_ITEMS(shop), prev, to_move);
				}
				else {
					next = to_move->next;
					LL_DELETE(SHOP_ITEMS(shop), to_move);
					LL_APPEND_ELEM(SHOP_ITEMS(shop), next, to_move);
				}
				
				// message: re-atoi(num_arg) because we destroyed num finding our target
				msg_to_char(ch, "You move item %d %s.\r\n", atoi(num_arg), (up ? "up" : "down"));
			}
		}
	}	// end 'move'
	else {
		msg_to_char(ch, "Usage: item add <vnum> <cost> <currency vnum | coins> [min reputation]\r\n");
		msg_to_char(ch, "Usage: item change <number> vnum <value>\r\n");
		msg_to_char(ch, "Usage: item move <number> <up | down>\r\n");
		msg_to_char(ch, "Usage: item copy <from type> <from vnum>\r\n");
		msg_to_char(ch, "Usage: item remove <number | all>\r\n");
	}
}


OLC_MODULE(shopedit_locations) {
	void qedit_process_quest_givers(char_data *ch, char *argument, struct quest_giver **list, char *command);
	
	shop_data *shop = GET_OLC_SHOP(ch->desc);
	qedit_process_quest_givers(ch, argument, &SHOP_LOCATIONS(shop), "locations");
}


OLC_MODULE(shopedit_name) {
	shop_data *shop = GET_OLC_SHOP(ch->desc);
	olc_process_string(ch, argument, "name", &SHOP_NAME(shop));
}


OLC_MODULE(shopedit_opens) {
	shop_data *shop = GET_OLC_SHOP(ch->desc);
	int hour;
	
	if (!isdigit(*argument) || (hour = atoi(argument)) < 0 || hour > 23) {
		msg_to_char(ch, "Time must be an hour between 0 and 23, or 1-12am or pm.\r\n");
		return;
	}
	
	if (strstr(argument, "am") && hour >= 1 && hour <= 12) {
		SHOP_OPEN_TIME(shop) = (hour == 12 ? 0 : hour);
	}
	else if (strstr(argument, "pm") && hour >= 1 && hour <= 12) {
		SHOP_OPEN_TIME(shop) = (hour == 12 ? hour : (hour + 12));
	}
	else {
		SHOP_OPEN_TIME(shop) = hour;
	}
	
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		msg_to_char(ch, "It now opens at %d%s.\r\n", TIME_TO_12H(SHOP_OPEN_TIME(shop)), AM_PM(SHOP_OPEN_TIME(shop)));
	}
}
