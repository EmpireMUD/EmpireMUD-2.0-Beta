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
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   OLC Modules
*/

// local data
const char *default_shop_name = "Unnamed Shop";

// external consts
extern struct faction_reputation_type reputation_levels[];
extern const char *shop_flags[];

// external funcs
extern struct quest_giver *copy_quest_givers(struct quest_giver *from);
void free_quest_givers(struct quest_giver *list);
void get_quest_giver_display(struct quest_giver *list, char *save_buffer);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////


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
	bool problem = FALSE;
	
	if (SHOP_FLAGGED(shop, SHOP_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, SHOP_VNUM(shop), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	
	if (!SHOP_NAME(shop) || !*SHOP_NAME(shop) || !str_cmp(SHOP_NAME(shop), default_shop_name)) {
		olc_audit_msg(ch, SHOP_VNUM(shop), "No name set");
		problem = TRUE;
	}
	
	// TODO more audits
	
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
	void complete_building(room_data *room);
	
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
		
		sprintf(save_buffer + strlen(save_buffer), "%2d. [%5d] %s for %d %s%s\r\n", ++count, item->vnum, get_obj_name_by_proto(item->vnum), item->cost, (item->currency == NOTHING ? "coins" : get_generic_string_by_vnum(item->currency, GENERIC_CURRENCY, item->cost == 1 ? GSTR_CURRENCY_SINGULAR : GSTR_CURRENCY_PLURAL)), buf);
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
	
	size += snprintf(buf + size, sizeof(buf) - size, "Opens at: [\tc%d%s\t0], Closes at: [\tc%d%s\t0], Faction allegiance: [\ty%s\t0]\r\n", TIME_TO_12H(SHOP_OPEN_TIME(shop)), AM_PM(SHOP_OPEN_TIME(shop)), TIME_TO_12H(SHOP_CLOSE_TIME(shop)), AM_PM(SHOP_CLOSE_TIME(shop)), SHOP_ALLEGIANCE(shop) ? FCT_NAME(SHOP_ALLEGIANCE(shop)) : "none");
	
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
	
	sprintf(buf + strlen(buf), "[\tc%d\t0] \tc%s\t0\r\n", GET_OLC_VNUM(ch->desc), !real_shop(SHOP_VNUM(shop)) ? "new shop" : SHOP_NAME(real_shop(SHOP_VNUM(shop))));
	sprintf(buf + strlen(buf), "<\tyname\t0> %s\r\n", NULLSAFE(SHOP_NAME(shop)));
	
	sprintbit(SHOP_FLAGS(shop), shop_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<\tyflags\t0> %s\r\n", lbuf);
	
	sprintf(buf + strlen(buf), "<\tyopens\t0> %d%s\r\n", TIME_TO_12H(SHOP_OPEN_TIME(shop)), AM_PM(SHOP_OPEN_TIME(shop)));
	sprintf(buf + strlen(buf), "<\tycloses\t0> %d%s\r\n", TIME_TO_12H(SHOP_CLOSE_TIME(shop)), AM_PM(SHOP_CLOSE_TIME(shop)));
	sprintf(buf + strlen(buf), "<\tyallegiance\t0> %s\r\n", SHOP_ALLEGIANCE(shop) ? FCT_NAME(SHOP_ALLEGIANCE(shop)) : "none");
	
	get_quest_giver_display(SHOP_LOCATIONS(shop), lbuf);
	sprintf(buf + strlen(buf), "Locations: <\tylocations\t0>\r\n%s", lbuf);
	
	get_shop_items_display(shop, lbuf);
	sprintf(buf + strlen(buf), "Items: <\tyitems\t0>\r\n%s", lbuf);
	
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
		msg_to_char(ch, "You set its allegience to 'none'.\r\n");
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


OLC_MODULE(shopedit_locations) {
	void qedit_process_quest_givers(char_data *ch, char *argument, struct quest_giver **list, char *command);
	
	shop_data *shop = GET_OLC_SHOP(ch->desc);
	qedit_process_quest_givers(ch, argument, &SHOP_LOCATIONS(shop), "locations");
}


OLC_MODULE(shopedit_name) {
	shop_data *shop = GET_OLC_SHOP(ch->desc);
	olc_process_string(ch, argument, "name", &SHOP_NAME(shop));
}

// opens
// closes
// items
