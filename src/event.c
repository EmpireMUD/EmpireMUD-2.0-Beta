/* ************************************************************************
*   File: event.c                                         EmpireMUD 2.0b5 *
*  Usage: timed global events that reward players for participation       *
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
*   Edit Modules
*/

// local data
const char *default_event_name = "Unnamed Event";
const char *default_event_description = "This event has no description.\r\n";
const char *default_event_complete_msg = "The event has ended.\r\n";

// external consts
extern const char *event_types[];
extern const char *event_flags[];
extern const char *event_rewards[];

// external funcs

// local protos


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Quick way to turn a vnum into a name, safely.
*
* @param any_vnum vnum The event vnum to look up.
* @return char* A name for the vnum, or "UNKNOWN".
*/
char *get_event_name_by_proto(any_vnum vnum) {
	event_data *proto = find_event_by_vnum(vnum);
	return proto ? EVT_NAME(proto) : "UNKNOWN";
}


/**
* Gets standard string display like "4x lumber" for an event reward.
*
* @param struct event_reward *reward The reward to show.
* @param bool show_vnums If TRUE, adds [1234] at the start of the string.
* @return char* The string display.
*/
char *event_reward_string(struct event_reward *reward, bool show_vnums) {
	static char output[256];
	char vnum[256];
	
	*output = '\0';
	if (!reward) {
		return output;
	}
	
	if (show_vnums) {
		snprintf(vnum, sizeof(vnum), "[%d] ", reward->vnum);
	}
	else {
		*vnum = '\0';
	}
	
	// EVTR_x
	switch (reward->type) {
	/*
		case EVTR_CURRENCY: {
			snprintf(output, sizeof(output), "%s%d %s", vnum, reward->amount, get_generic_string_by_vnum(reward->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(reward->amount)));
			break;
		}
	*/
		default: {
			snprintf(output, sizeof(output), "%s%dx UNKNOWN", vnum, reward->amount);
			break;
		}
	}
	
	return output;
}


/**
* Copies entries from one list into another, only if they are not already in
* the to_list.
*
* @param struct event_reward **to_list A pointer to the destination list.
* @param struct event_reward *from_list The list to copy from.
*/
void smart_copy_event_rewards(struct event_reward **to_list, struct event_reward *from_list) {
	struct event_reward *iter, *search, *reward;
	bool found;
	
	LL_FOREACH(from_list, iter) {
		// ensure not already in list
		found = FALSE;
		LL_FOREACH(*to_list, search) {
			if (search->type == iter->type && search->amount == iter->amount && search->vnum == iter->vnum && search->min == iter->max && search->max == iter->max) {
				found = TRUE;
				break;
			}
		}
		
		// add it
		if (!found) {
			CREATE(reward, struct event_reward, 1);
			reward->min = iter->min;
			reward->max = iter->max;
			reward->type = iter->type;
			reward->amount = iter->amount;
			reward->vnum = iter->vnum;
			LL_APPEND(*to_list, reward);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common event problems and reports them to ch.
*
* @param event_data *event The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_event(event_data *event, char_data *ch) {
	bool problem = FALSE;
	
	if (EVT_FLAGGED(event, EVTF_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, EVT_VNUM(event), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!EVT_NAME(event) || !*EVT_NAME(event) || !str_cmp(EVT_NAME(event), default_event_name)) {
		olc_audit_msg(ch, EVT_VNUM(event), "Name not set");
		problem = TRUE;
	}
	if (!isupper(*EVT_NAME(event))) {
		olc_audit_msg(ch, EVT_VNUM(event), "Name not capitalized");
		problem = TRUE;
	}
	if (!EVT_DESCRIPTION(event) || !*EVT_DESCRIPTION(event) || !str_cmp(EVT_DESCRIPTION(event), default_event_description)) {
		olc_audit_msg(ch, EVT_VNUM(event), "Description not set");
		problem = TRUE;
	}
	if (!EVT_COMPLETE_MSG(event) || !*EVT_COMPLETE_MSG(event) || !str_cmp(EVT_COMPLETE_MSG(event), default_event_complete_msg)) {
		olc_audit_msg(ch, EVT_VNUM(event), "Complete message not set");
		problem = TRUE;
	}
	
	if (EVT_MIN_LEVEL(event) > EVT_MAX_LEVEL(event) && EVT_MAX_LEVEL(event) != 0) {
		olc_audit_msg(ch, EVT_VNUM(event), "Min level higher than max level");
		problem = TRUE;
	}
	
	// TODO more audits...
	
	return problem;
}


/**
* Deletes entries by type+vnum.
*
* @param struct event_reward **list A pointer to the list to delete from.
* @param int type QG_ type.
* @param any_vnum vnum The vnum to remove.
* @return bool TRUE if the type+vnum was removed from the list. FALSE if not.
*/
bool delete_event_reward_from_list(struct event_reward **list, int type, any_vnum vnum) {
	struct event_reward *iter, *next_iter;
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
* @param struct event_reward *list A list to search.
* @param int type REQ_ type.
* @param any_vnum vnum The vnum to look for.
* @return bool TRUE if the type+vnum is in the list. FALSE if not.
*/
bool find_event_reward_in_list(struct event_reward *list, int type, any_vnum vnum) {
	struct event_reward *iter;
	LL_FOREACH(list, iter) {
		if (iter->type == type && iter->vnum == vnum) {
			return TRUE;
		}
	}
	return FALSE;
}


/**
* For the .list command.
*
* @param event_data *event The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_event(event_data *event, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s", EVT_VNUM(event), EVT_NAME(event));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", EVT_VNUM(event), EVT_NAME(event));
	}
		
	return output;
}


/**
* Searches for all uses of an event and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The event vnum.
*/
void olc_search_event(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	event_data *event = find_event_by_vnum(vnum);
	// quest_data *quest, *next_quest;
	int size, found;
	// bool any;
	
	if (!event) {
		msg_to_char(ch, "There is no event %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of event %d (%s):\r\n", vnum, EVT_NAME(event));
	
	/*
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_NOT_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_NOT_ON_QUEST, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "PRG [%5d] %s\r\n", PRG_VNUM(prg), PRG_NAME(prg));
		}
	}
	
	// on other quests
	HASH_ITER(hh, quest_table, qiter, next_qiter) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QR_x, REQ_x: quest types
		any = find_requirement_in_list(QUEST_TASKS(qiter), REQ_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(qiter), REQ_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(qiter), REQ_NOT_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(qiter), REQ_NOT_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(qiter), REQ_NOT_ON_QUEST, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(qiter), REQ_NOT_ON_QUEST, vnum);
		any |= find_quest_reward_in_list(QUEST_REWARDS(qiter), QR_QUEST_CHAIN, vnum);
		any |= find_quest_giver_in_list(QUEST_STARTS_AT(qiter), QG_QUEST, vnum);
		any |= find_quest_giver_in_list(QUEST_ENDS_AT(qiter), QG_QUEST, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(qiter), QUEST_NAME(qiter));
		}
	}
	
	// on other shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QG_x: shop types
		any = find_quest_giver_in_list(SHOP_LOCATIONS(shop), QG_QUEST, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SHOP [%5d] %s\r\n", SHOP_VNUM(shop), SHOP_NAME(shop));
		}
	}
	
	// on socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: quest types
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_NOT_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_NOT_ON_QUEST, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	*/
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


// Simple vnum sorter for the event hash
int sort_events(event_data *a, event_data *b) {
	return EVT_VNUM(a) - EVT_VNUM(b);
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* @param any_vnum vnum Any event vnum
* @return event_data* The event, or NULL if it doesn't exist
*/
event_data *find_event_by_vnum(any_vnum vnum) {
	event_data *event;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(event_table, &vnum, event);
	return event;
}


/**
* Puts an event into the hash table.
*
* @param event_data *event The event data to add to the table.
*/
void add_event_to_table(event_data *event) {
	event_data *find;
	any_vnum vnum;
	
	if (event) {
		vnum = EVT_VNUM(event);
		HASH_FIND_INT(event_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(event_table, vnum, event);
			HASH_SORT(event_table, sort_events);
		}
	}
}


/**
* Removes an event from the hash table.
*
* @param event_data *event The event data to remove from the table.
*/
void remove_event_from_table(event_data *event) {
	HASH_DEL(event_table, event);
}


/**
* Initializes a new event. This clears all memory for it, so set the vnum
* AFTER.
*
* @param event_data *event The event to initialize.
*/
void clear_event(event_data *event) {
	memset((char *) event, 0, sizeof(event_data));
	
	EVT_VNUM(event) = NOTHING;
	EVT_REPEATS_AFTER(event) = NOT_REPEATABLE;
	EVT_DURATION(event) = 1;
}


/**
* @param struct event_reward *from The list to copy.
* @return struct event_reward* The copy of the list.
*/
struct event_reward *copy_event_rewards(struct event_reward *from) {
	struct event_reward *el, *iter, *list = NULL, *end = NULL;
	
	LL_FOREACH(from, iter) {
		CREATE(el, struct event_reward, 1);
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
* @param struct event_reward *list The list to free.
*/
void free_event_rewards(struct event_reward *list) {
	struct event_reward *iter, *next_iter;
	LL_FOREACH_SAFE(list, iter, next_iter) {
		free(iter);
	}
}


/**
* frees up memory for an event data.
*
* See also: olc_delete_event
*
* @param event_data *event The event data to free.
*/
void free_event(event_data *event) {
	event_data *proto = find_event_by_vnum(EVT_VNUM(event));
	
	// strings
	if (EVT_NAME(event) && (!proto || EVT_NAME(event) != EVT_NAME(proto))) {
		free(EVT_NAME(event));
	}
	if (EVT_DESCRIPTION(event) && (!proto || EVT_DESCRIPTION(event) != EVT_DESCRIPTION(proto))) {
		free(EVT_DESCRIPTION(event));
	}
	if (EVT_COMPLETE_MSG(event) && (!proto || EVT_COMPLETE_MSG(event) != EVT_COMPLETE_MSG(proto))) {
		free(EVT_COMPLETE_MSG(event));
	}
	if (EVT_NOTES(event) && (!proto || EVT_NOTES(event) != EVT_NOTES(proto))) {
		free(EVT_NOTES(event));
	}
	
	// pointers
	if (EVT_RANK_REWARDS(event) && (!proto || EVT_RANK_REWARDS(event) != EVT_RANK_REWARDS(proto))) {
		free_event_rewards(EVT_RANK_REWARDS(event));
	}
	if (EVT_THRESHOLD_REWARDS(event) && (!proto || EVT_THRESHOLD_REWARDS(event) != EVT_THRESHOLD_REWARDS(proto))) {
		free_event_rewards(EVT_THRESHOLD_REWARDS(event));
	}
	
	free(event);
}


/**
* Parses an event reward, saved as:
*
* R min max type vnum amount
* T min max type vnum amount
*
* @param char *line The reward line as read from the file.
* @param struct event_reward **list The list to append to.
* @param char *error_str How to report if there is an error.
*/
void parse_event_reward(char *line, struct event_reward **list, char *error_str) {
	struct event_reward *reward;
	int min, max, type, amount;
	any_vnum vnum;
	char key;
	
	if (!line || !*line || !list) {
		log("SYSERR: data error in event reward line of %s: %s", error_str ? error_str : "UNKNOWN", NULLSAFE(line));
		exit(1);
	}
	if (sscanf(line, "%c %d %d %d %d %d", &key, &min, &max, &type, &vnum, &amount) != 6) {
		log("SYSERR: format error in event reward line of %s", error_str ? error_str : "UNKNOWN");
		exit(1);
	}
	
	CREATE(reward, struct event_reward, 1);
	reward->min = min;
	reward->max = max;
	reward->type = type;
	reward->vnum = vnum;
	reward->amount = amount;
	
	LL_APPEND(*list, reward);
}


/**
* Read one event from file.
*
* @param FILE *fl The open .evt file
* @param any_vnum vnum The event vnum
*/
void parse_event(FILE *fl, any_vnum vnum) {
	void parse_requirement(FILE *fl, struct req_data **list, char *error_str);
	
	char line[256], error[256], str_in[256];
	event_data *event, *find;
	int int_in[6];
	
	CREATE(event, event_data, 1);
	clear_event(event);
	EVT_VNUM(event) = vnum;
	
	HASH_FIND_INT(event_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate event vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_event_to_table(event);
		
	// for error messages
	sprintf(error, "event vnum %d", vnum);
	
	// lines 1-4: strings
	EVT_NAME(event) = fread_string(fl, error);
	EVT_DESCRIPTION(event) = fread_string(fl, error);
	EVT_COMPLETE_MSG(event) = fread_string(fl, error);
	EVT_NOTES(event) = fread_string(fl, error);
	
	// 5. version type flags min max duration repeats
	if (!get_line(fl, line) || sscanf(line, "%d %d %s %d %d %d %d", &int_in[0], &int_in[1], str_in, &int_in[2], &int_in[3], &int_in[4], &int_in[5]) != 7) {
		log("SYSERR: Format error in line 4 of %s", error);
		exit(1);
	}
	
	EVT_VERSION(event) = int_in[0];
	EVT_TYPE(event) = int_in[1];
	EVT_FLAGS(event) = asciiflag_conv(str_in);
	EVT_MIN_LEVEL(event) = int_in[2];
	EVT_MAX_LEVEL(event) = int_in[3];
	EVT_DURATION(event) = int_in[4];
	EVT_REPEATS_AFTER(event) = int_in[5];
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'R': {	// rank rewards
				parse_event_reward(line, &EVT_RANK_REWARDS(event), error);
				break;
			}
			case 'T': {	// threshold rewards
				parse_event_reward(line, &EVT_THRESHOLD_REWARDS(event), error);
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


// writes entries in the event index
void write_event_index(FILE *fl) {
	event_data *event, *next_event;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, event_table, event, next_event) {
		// determine "zone number" by vnum
		this = (int)(EVT_VNUM(event) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, EVT_SUFFIX);
			last = this;
		}
	}
}


/**
* Writes a list of 'event_reward' to a data file.
*
* @param FILE *fl The file, open for writing.
* @param char letter The tag letter.
* @param struct event_reward *list The list to write.
*/
void write_event_rewards_to_file(FILE *fl, char letter, struct event_reward *list) {
	struct event_reward *iter;
	LL_FOREACH(list, iter) {
		fprintf(fl, "%c %d %d %d %d %d\n", letter, iter->min, iter->max, iter->type, iter->vnum, iter->amount);
	}
}


/**
* Outputs one event item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param event_data *event The thing to save.
*/
void write_event_to_file(FILE *fl, event_data *event) {
	char temp[MAX_STRING_LENGTH];
	
	if (!fl || !event) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_event_to_file called without %s", !fl ? "file" : "event");
		return;
	}
	
	fprintf(fl, "#%d\n", EVT_VNUM(event));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(EVT_NAME(event)));
	
	// 2. desc
	strcpy(temp, NULLSAFE(EVT_DESCRIPTION(event)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// 3. complete msg
	strcpy(temp, NULLSAFE(EVT_COMPLETE_MSG(event)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// 4. notes
	strcpy(temp, NULLSAFE(EVT_NOTES(event)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// 5. version type flags min max duration repeats
	fprintf(fl, "%d %d %s %d %d %d %d\n", EVT_VERSION(event), EVT_TYPE(event), bitv_to_alpha(EVT_FLAGS(event)), EVT_MIN_LEVEL(event), EVT_MAX_LEVEL(event), EVT_DURATION(event), EVT_REPEATS_AFTER(event));
	
	// R. rank rewards
	write_event_rewards_to_file(fl, 'R', EVT_RANK_REWARDS(event));
	
	// R. rank thresholds
	write_event_rewards_to_file(fl, 'T', EVT_THRESHOLD_REWARDS(event));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////


/**
* Creates a new event entry.
* 
* @param any_vnum vnum The number to create.
* @return event_data* The new event's prototype.
*/
event_data *create_event_table_entry(any_vnum vnum) {
	event_data *event;
	
	// sanity
	if (find_event_by_vnum(vnum)) {
		log("SYSERR: Attempting to insert event at existing vnum %d", vnum);
		return find_event_by_vnum(vnum);
	}
	
	CREATE(event, event_data, 1);
	clear_event(event);
	EVT_VNUM(event) = vnum;
	EVT_NAME(event) = str_dup(default_event_name);
	EVT_DESCRIPTION(event) = str_dup(default_event_description);
	EVT_COMPLETE_MSG(event) = str_dup(default_event_complete_msg);
	EVT_FLAGS(event) = EVTF_IN_DEVELOPMENT;
	add_event_to_table(event);

	// save index and event file now
	save_index(DB_BOOT_EVT);
	save_library_file_for_vnum(DB_BOOT_EVT, vnum);

	return event;
}


/**
* WARNING: This function actually deletes an event.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_event(char_data *ch, any_vnum vnum) {
	event_data *event;
	// quest_data *quest, *next_quest;
	// progress_data *prg, *next_prg;
	// social_data *soc, *next_soc;
	// shop_data *shop, *next_shop;
	descriptor_data *desc;
	// char_data *chiter;
	// bool found;
	
	if (!(event = find_event_by_vnum(vnum))) {
		msg_to_char(ch, "There is no such event %d.\r\n", vnum);
		return;
	}
	
	// remove it from the hash table first
	remove_event_from_table(event);
	
	// look for people/empires on the event and force a refresh
	/*
	LL_FOREACH(character_list, chiter) {
		if (IS_NPC(chiter)) {
			continue;
		}
		if (!is_on_quest(chiter, vnum)) {
			continue;
		}
		refresh_all_quests(chiter);
	}
	*/
	
	// save index and event file now
	save_index(DB_BOOT_EVT);
	save_library_file_for_vnum(DB_BOOT_EVT, vnum);
	
	/*
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_NOT_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_NOT_ON_QUEST, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// update other quests
	HASH_ITER(hh, quest_table, qiter, next_qiter) {
		// REQ_x, QR_x: quest types
		found = delete_requirement_from_list(&QUEST_TASKS(qiter), REQ_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(qiter), REQ_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(qiter), REQ_NOT_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(qiter), REQ_NOT_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(qiter), REQ_NOT_ON_QUEST, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(qiter), REQ_NOT_ON_QUEST, vnum);
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(qiter), QR_QUEST_CHAIN, vnum);
		found |= delete_quest_giver_from_list(&QUEST_STARTS_AT(qiter), QG_QUEST, vnum);
		found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(qiter), QG_QUEST, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(qiter), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(qiter));
		}
	}
	
	// update shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		// QG_x: quest types
		found = delete_quest_giver_from_list(&SHOP_LOCATIONS(shop), QG_QUEST, vnum);
		
		if (found) {
			SET_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SHOP, SHOP_VNUM(shop));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		// REQ_x: quest types
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_NOT_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_NOT_ON_QUEST, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	*/
	
	// remove from from active editors
	for (desc = descriptor_list; desc; desc = desc->next) {
	/*
		if (GET_OLC_PROGRESS(desc)) {
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_NOT_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_NOT_ON_QUEST, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "A quest used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			// REQ_x, QR_x: quest types
			found = delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_NOT_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_NOT_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_NOT_ON_QUEST, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_NOT_ON_QUEST, vnum);
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_QUEST_CHAIN, vnum);
			found |= delete_quest_giver_from_list(&QUEST_STARTS_AT(GET_OLC_QUEST(desc)), QG_QUEST, vnum);
			found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(GET_OLC_QUEST(desc)), QG_QUEST, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "Another quest used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SHOP(desc)) {
			// QG_x: quest types
			found = delete_quest_giver_from_list(&SHOP_LOCATIONS(GET_OLC_SHOP(desc)), QG_QUEST, vnum);
		
			if (found) {
				SET_BIT(SHOP_FLAGS(GET_OLC_SHOP(desc)), SHOP_IN_DEVELOPMENT);
				msg_to_desc(desc, "A quest used by the shop you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			// REQ_x: quest types
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_NOT_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_NOT_ON_QUEST, vnum);
			
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A quest required by the social you are editing was deleted.\r\n");
			}
		}
		*/
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted event %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Event %d deleted.\r\n", vnum);
	
	free_event(event);
}


/**
* Function to save a player's changes to an event (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_event(descriptor_data *desc) {	
	event_data *proto, *event = GET_OLC_EVENT(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	// descriptor_data *iter;
	UT_hash_handle hh;
	
	// have a place to save it?
	if (!(proto = find_event_by_vnum(vnum))) {
		proto = create_event_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (EVT_NAME(proto)) {
		free(EVT_NAME(proto));
	}
	if (EVT_DESCRIPTION(proto)) {
		free(EVT_DESCRIPTION(proto));
	}
	if (EVT_COMPLETE_MSG(proto)) {
		free(EVT_COMPLETE_MSG(proto));
	}
	if (EVT_NOTES(proto)) {
		free(EVT_NOTES(proto));
	}
	free_event_rewards(EVT_RANK_REWARDS(proto));
	free_event_rewards(EVT_THRESHOLD_REWARDS(proto));
	
	// sanity
	if (!EVT_NAME(event) || !*EVT_NAME(event)) {
		if (EVT_NAME(event)) {
			free(EVT_NAME(event));
		}
		EVT_NAME(event) = str_dup(default_event_name);
	}
	if (!EVT_DESCRIPTION(event) || !*EVT_DESCRIPTION(event)) {
		if (EVT_DESCRIPTION(event)) {
			free(EVT_DESCRIPTION(event));
		}
		EVT_DESCRIPTION(event) = str_dup(default_event_description);
	}
	if (!EVT_COMPLETE_MSG(event) || !*EVT_COMPLETE_MSG(event)) {
		if (EVT_COMPLETE_MSG(event)) {
			free(EVT_COMPLETE_MSG(event));
		}
		EVT_COMPLETE_MSG(event) = str_dup(default_event_complete_msg);
	}
	if (EVT_NOTES(event) && !*EVT_NOTES(event)) {
		free(EVT_NOTES(event));
		EVT_NOTES(event) = NULL;
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *event;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_EVT, vnum);
	
	// look for players on the event and update them
	/*
	LL_FOREACH(descriptor_list, iter) {
		if (STATE(iter) != CON_PLAYING || !iter->character) {
			continue;
		}
		if (!is_on_quest(iter->character, vnum)) {
			continue;
		}
		refresh_all_quests(iter->character);
	}
	*/
}


/**
* Creates a copy of a event, or clears a new one, for editing.
* 
* @param event_data *input The event to copy, or NULL to make a new one.
* @return event_data* The copied event.
*/
event_data *setup_olc_event(event_data *input) {
	extern struct apply_data *copy_apply_list(struct apply_data *input);
	
	event_data *new;
	
	CREATE(new, event_data, 1);
	clear_event(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		EVT_NAME(new) = EVT_NAME(input) ? str_dup(EVT_NAME(input)) : NULL;
		EVT_DESCRIPTION(new) = EVT_DESCRIPTION(input) ? str_dup(EVT_DESCRIPTION(input)) : NULL;
		EVT_COMPLETE_MSG(new) = EVT_COMPLETE_MSG(input) ? str_dup(EVT_COMPLETE_MSG(input)) : NULL;
		EVT_NOTES(new) = EVT_NOTES(input) ? str_dup(EVT_NOTES(input)) : NULL;
		
		EVT_RANK_REWARDS(new) = copy_event_rewards(EVT_RANK_REWARDS(input));
		EVT_THRESHOLD_REWARDS(new) = copy_event_rewards(EVT_THRESHOLD_REWARDS(input));
		
		// update version number
		EVT_VERSION(new) += 1;
	}
	else {
		// brand new: some defaults
		EVT_NAME(new) = str_dup(default_event_name);
		EVT_DESCRIPTION(new) = str_dup(default_event_description);
		EVT_COMPLETE_MSG(new) = str_dup(default_event_complete_msg);
		EVT_FLAGS(new) = EVTF_IN_DEVELOPMENT;
		EVT_VERSION(new) = 1;
	}
	
	// done
	return new;	
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* Gets the display for a set of event rewards.
*
* @param struct event_reward *list Pointer to the start of a list of event rewards.
* @param char *save_buffer A buffer to store the result to.
*/
void get_event_reward_display(struct event_reward *list, char *save_buffer) {
	struct event_reward *reward;
	int count = 0;
	
	*save_buffer = '\0';
	LL_FOREACH(list, reward) {		
		sprintf(save_buffer + strlen(save_buffer), "%2d. %s: %s\r\n", ++count, event_rewards[reward->type], event_reward_string(reward, TRUE));
	}
	
	// empty list not shown
}


/**
* For vstat.
*
* @param char_data *ch The player requesting stats.
* @param event_data *event The event to display.
*/
void do_stat_event(char_data *ch, event_data *event) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	size_t size;
	
	if (!event) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \tc%s\t0\r\n", EVT_VNUM(event), EVT_NAME(event));
	size += snprintf(buf + size, sizeof(buf) - size, "%s", EVT_DESCRIPTION(event));
	size += snprintf(buf + size, sizeof(buf) - size, "-------------------------------------------------\r\n");
	size += snprintf(buf + size, sizeof(buf) - size, "%s", EVT_COMPLETE_MSG(event));
	
	if (EVT_NOTES(event)) {
		size += snprintf(buf + size, sizeof(buf) - size, "- Notes -----------------------------------------\r\n");
		size += snprintf(buf + size, sizeof(buf) - size, "%s", EVT_NOTES(event));
	}
	
	sprintbit(EVT_FLAGS(event), event_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);	
	
	if (EVT_REPEATS_AFTER(event) == NOT_REPEATABLE) {
		strcpy(part, "never");
	}
	else if (EVT_REPEATS_AFTER(event) == 0) {
		strcpy(part, "never");	// 0 is not a valid repeat time, unlike quests
	}
	else {
		sprintf(part, "%d minutes (%d:%02d:%02d)", EVT_REPEATS_AFTER(event), (EVT_REPEATS_AFTER(event) / (60 * 24)), ((EVT_REPEATS_AFTER(event) % (60 * 24)) / 60), ((EVT_REPEATS_AFTER(event) % (60 * 24)) % 60));
	}
	size += snprintf(buf + size, sizeof(buf) - size, "Level limits: [\tc%s\t0], Duration: [\tc%d minutes (%d:%02d:%02d)\t0], Repeatable: [\tc%s\t0]\r\n", level_range_string(EVT_MIN_LEVEL(event), EVT_MAX_LEVEL(event), 0), EVT_DURATION(event), (EVT_DURATION(event) / (60 * 24)), ((EVT_DURATION(event) % (60 * 24)) / 60), ((EVT_DURATION(event) % (60 * 24)) % 60), part);
	
	get_event_reward_display(EVT_RANK_REWARDS(event), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Rank Rewards:\r\n%s", *part ? part : " none\r\n");
	
	get_event_reward_display(EVT_THRESHOLD_REWARDS(event), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Threshold Rewards:\r\n%s", *part ? part : " none\r\n");
	
	page_string(ch->desc, buf, TRUE);
}


/**
* This is the main recipe display for event OLC. It displays the user's
* currently-edited event.
*
* @param char_data *ch The person who is editing an event and will see its display.
*/
void olc_show_event(char_data *ch) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	
	if (!event) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !find_event_by_vnum(EVT_VNUM(event)) ? "new event" : get_event_name_by_proto(EVT_VNUM(event)));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(EVT_NAME(event), default_event_name), NULLSAFE(EVT_NAME(event)));
	sprintf(buf + strlen(buf), "<%sdescription\t0>\r\n%s", OLC_LABEL_STR(EVT_DESCRIPTION(event), default_event_description), NULLSAFE(EVT_DESCRIPTION(event)));
	sprintf(buf + strlen(buf), "<%scompletemessage\t0>\r\n%s", OLC_LABEL_STR(EVT_COMPLETE_MSG(event), default_event_complete_msg), NULLSAFE(EVT_COMPLETE_MSG(event)));
	
	sprintbit(EVT_FLAGS(event), event_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT), lbuf);
	
	if (EVT_MIN_LEVEL(event) > 0) {
		sprintf(buf + strlen(buf), "<%sminlevel\t0> %d\r\n", OLC_LABEL_CHANGED, EVT_MIN_LEVEL(event));
	}
	else {
		sprintf(buf + strlen(buf), "<%sminlevel\t0> none\r\n", OLC_LABEL_UNCHANGED);
	}
	if (EVT_MAX_LEVEL(event) > 0) {
		sprintf(buf + strlen(buf), "<%smaxlevel\t0> %d\r\n", OLC_LABEL_CHANGED, EVT_MAX_LEVEL(event));
	}
	else {
		sprintf(buf + strlen(buf), "<%smaxlevel\t0> none\r\n", OLC_LABEL_UNCHANGED);
	}
	
	sprintf(buf + strlen(buf), "<%sduration\t0> %d minutes (%d:%02d:%02d)\r\n", OLC_LABEL_VAL(EVT_DURATION(event), 0), EVT_DURATION(event), (EVT_DURATION(event) / (60 * 24)), ((EVT_DURATION(event) % (60 * 24)) / 60), ((EVT_DURATION(event) % (60 * 24)) % 60));
	
	if (EVT_REPEATS_AFTER(event) == NOT_REPEATABLE || EVT_REPEATS_AFTER(event) == 0) {
		sprintf(buf + strlen(buf), "<%srepeat\t0> never\r\n", OLC_LABEL_VAL(EVT_REPEATS_AFTER(event), NOT_REPEATABLE));
	}
	else if (EVT_REPEATS_AFTER(event) > 0) {
		sprintf(buf + strlen(buf), "<%srepeat\t0> %d minutes (%d:%02d:%02d)\r\n", OLC_LABEL_VAL(EVT_REPEATS_AFTER(event), 0), EVT_REPEATS_AFTER(event), (EVT_REPEATS_AFTER(event) / (60 * 24)), ((EVT_REPEATS_AFTER(event) % (60 * 24)) / 60), ((EVT_REPEATS_AFTER(event) % (60 * 24)) % 60));
	}
	
	get_event_reward_display(EVT_RANK_REWARDS(event), lbuf);
	sprintf(buf + strlen(buf), "Rank rewards: <%srank\t0>\r\n%s", OLC_LABEL_PTR(EVT_RANK_REWARDS(event)), lbuf);
	
	get_event_reward_display(EVT_THRESHOLD_REWARDS(event), lbuf);
	sprintf(buf + strlen(buf), "Threshold rewards: <%sthreshold\t0>\r\n%s", OLC_LABEL_PTR(EVT_THRESHOLD_REWARDS(event)), lbuf);
	
	sprintf(buf + strlen(buf), "<%snotes\t0>\r\n%s", OLC_LABEL_PTR(EVT_NOTES(event)), NULLSAFE(EVT_NOTES(event)));
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the event db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_event(char *searchname, char_data *ch) {
	event_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, event_table, iter, next_iter) {
		if (multi_isname(searchname, EVT_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, EVT_VNUM(iter), EVT_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(evedit_completemessage) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "completion message for %s", EVT_NAME(event));
		start_string_editor(ch->desc, buf, &EVT_COMPLETE_MSG(event), MAX_ITEM_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(evedit_description) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", EVT_NAME(event));
		start_string_editor(ch->desc, buf, &EVT_DESCRIPTION(event), MAX_ITEM_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(evedit_duration) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	
	if (!*argument) {
		msg_to_char(ch, "Set the duration to how many minutes?\r\n");
	}
	else if (isdigit(*argument)) {
		EVT_DURATION(event) = olc_process_number(ch, argument, "duration", "duration", 1, MAX_INT, EVT_DURATION(event));
		msg_to_char(ch, "It now runs for %d minutes (%d:%02d:%02d).\r\n", EVT_DURATION(event), (EVT_DURATION(event) / (60 * 24)), ((EVT_DURATION(event) % (60 * 24)) / 60), ((EVT_DURATION(event) % (60 * 24)) % 60));
	}
	else {
		msg_to_char(ch, "Invalid duration.\r\n");
	}
}


OLC_MODULE(evedit_flags) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	bool had_indev = IS_SET(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	EVT_FLAGS(event) = olc_process_flag(ch, argument, "event", "flags", event_flags, EVT_FLAGS(event));
	
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !IS_SET(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT);
	}
}


OLC_MODULE(evedit_name) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	olc_process_string(ch, argument, "name", &EVT_NAME(event));
}


OLC_MODULE(evedit_maxlevel) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	EVT_MAX_LEVEL(event) = olc_process_number(ch, argument, "maximum level", "maxlevel", 0, MAX_INT, EVT_MAX_LEVEL(event));
}


OLC_MODULE(evedit_minlevel) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	EVT_MIN_LEVEL(event) = olc_process_number(ch, argument, "minimum level", "minlevel", 0, MAX_INT, EVT_MIN_LEVEL(event));
}


OLC_MODULE(evedit_notes) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "notes for %s", EVT_NAME(event));
		start_string_editor(ch->desc, buf, &EVT_NOTES(event), MAX_ITEM_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(evedit_repeat) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	
	if (!*argument) {
		msg_to_char(ch, "Set the repeat interval to how many minutes (or never)?\r\n");
	}
	else if (is_abbrev(argument, "never") || is_abbrev(argument, "none")) {
		EVT_REPEATS_AFTER(event) = NOT_REPEATABLE;
		msg_to_char(ch, "It is now non-repeatable.\r\n");
	}
	else if (isdigit(*argument)) {
		EVT_REPEATS_AFTER(event) = olc_process_number(ch, argument, "repeats after", "repeat", 0, MAX_INT, EVT_REPEATS_AFTER(event));
		msg_to_char(ch, "It now repeats after %d minutes (%d:%02d:%02d).\r\n", EVT_REPEATS_AFTER(event), (EVT_REPEATS_AFTER(event) / (60 * 24)), ((EVT_REPEATS_AFTER(event) % (60 * 24)) / 60), ((EVT_REPEATS_AFTER(event) % (60 * 24)) % 60));
	}
	else {
		msg_to_char(ch, "Invalid repeat interval.\r\n");
	}
}

/*
OLC_MODULE(qedit_rewards) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	char cmd_arg[MAX_INPUT_LENGTH], field_arg[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH];
	char vnum_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	struct quest_reward *reward, *iter, *change, *copyfrom;
	struct quest_reward **list = &QUEST_REWARDS(quest);
	int findtype, num, stype;
	faction_data *fct;
	bool found, ok;
	any_vnum vnum;
	
	argument = any_one_arg(argument, cmd_arg);	// add/remove/change/copy
	
	if (is_abbrev(cmd_arg, "copy")) {
		// usage: rewards copy <from type> <from vnum>
		argument = any_one_arg(argument, type_arg);	// just "quest" for now
		argument = any_one_arg(argument, vnum_arg);	// any vnum for that type
		
		if (!*type_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: rewards copy <from type> <from vnum>\r\n");
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
				case OLC_QUEST: {
					quest_data *from_qst = quest_proto(vnum);
					if (from_qst) {
						copyfrom = QUEST_REWARDS(from_qst);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy rewards from %ss.\r\n", buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, vnum_arg);
			}
			else {
				smart_copy_quest_rewards(list, copyfrom);
				msg_to_char(ch, "Copied rewards from %s %d.\r\n", buf, vnum);
			}
		}
	}	// end 'copy'
	else if (is_abbrev(cmd_arg, "remove")) {
		// usage: rewards remove <number | all>
		skip_spaces(&argument);	// only arg is number
		
		if (!*argument) {
			msg_to_char(ch, "Remove which reward (number)?\r\n");
		}
		else if (!str_cmp(argument, "all")) {
			free_quest_rewards(*list);
			*list = NULL;
			msg_to_char(ch, "You remove all the rewards.\r\n");
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid reward number.\r\n");
		}
		else {
			found = FALSE;
			LL_FOREACH(*list, iter) {
				if (--num == 0) {
					found = TRUE;
					
					if (iter->vnum > 0) {
						msg_to_char(ch, "You remove the reward for %dx %s %d.\r\n", iter->amount, quest_reward_types[iter->type], iter->vnum);
					}
					else {
						msg_to_char(ch, "You remove the reward for %dx %s.\r\n", iter->amount, quest_reward_types[iter->type]);
					}
					LL_DELETE(*list, iter);
					free(iter);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid reward number.\r\n");
			}
		}
	}	// end 'remove'
	else if (is_abbrev(cmd_arg, "add")) {
		// usage: rewards add <type> <amount> <vnum/type>
		argument = any_one_arg(argument, type_arg);
		argument = any_one_arg(argument, num_arg);
		argument = any_one_word(argument, vnum_arg);
		
		if (!*type_arg || !*num_arg || !isdigit(*num_arg)) {
			msg_to_char(ch, "Usage: rewards add <type> <amount> <vnum/type>\r\n");
		}
		else if ((stype = search_block(type_arg, quest_reward_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", type_arg);
		}
		else if ((num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid amount '%s'.\r\n", num_arg);
		}
		else {		
			// QR_x: validate vnum
			vnum = 0;
			ok = FALSE;
			switch (stype) {
				case QR_BONUS_EXP: {
					// vnum not required
					ok = TRUE;
					break;
				}
				case QR_COINS: {
					if (is_abbrev(vnum_arg, "miscellaneous") || is_abbrev(vnum_arg, "simple") || is_abbrev(vnum_arg, "other")) {
						vnum = OTHER_COIN;
						ok = TRUE;
					}
					else if (is_abbrev(vnum_arg, "empire")) {
						vnum = REWARD_EMPIRE_COIN;
						ok = TRUE;
					}
					else {
						msg_to_char(ch, "You must choose misc or empire coins.\r\n");
						return;
					}
					break;	
				}
				case QR_CURRENCY: {
					if (!*vnum_arg) {
						msg_to_char(ch, "Usage: rewards add currency <amount> <generic vnum>\r\n");
						return;
					}
					if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
						msg_to_char(ch, "Invalid generic vnum '%s'.\r\n", vnum_arg);
						return;
					}
					if (find_generic(vnum, GENERIC_CURRENCY)) {
						ok = TRUE;
					}
					break;
				}
				case QR_OBJECT: {
					if (!*vnum_arg) {
						msg_to_char(ch, "Usage: rewards add object <amount> <object vnum>\r\n");
						return;
					}
					if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
						msg_to_char(ch, "Invalid obj vnum '%s'.\r\n", vnum_arg);
						return;
					}
					if (obj_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QR_SET_SKILL:
				case QR_SKILL_EXP:
				case QR_SKILL_LEVELS: {
					if (!*vnum_arg) {
						msg_to_char(ch, "Usage: rewards add <set-skill | skill-exp | skill-levels> <level> <skill vnum>\r\n");
						return;
					}
					if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
						msg_to_char(ch, "Invalid skill vnum '%s'.\r\n", vnum_arg);
						return;
					}
					if (find_skill_by_vnum(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QR_QUEST_CHAIN: {
					if (!*vnum_arg) {
						strcpy(vnum_arg, num_arg);	// they may have omitted amount
					}
					if (!*vnum_arg || !isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
						msg_to_char(ch, "Invalid quest vnum '%s'.\r\n", vnum_arg);
						return;
					}
					if (quest_proto(vnum)) {
						ok = TRUE;
					}
					// amount is not used here
					num = 1;
					break;
				}
				case QR_REPUTATION: {
					if (!*vnum_arg) {
						msg_to_char(ch, "Usage: rewards add reputation <amount> <faction>\r\n");
						return;
					}
					if (!(fct = find_faction(vnum_arg))) {
						msg_to_char(ch, "Invalid faction '%s'.\r\n", vnum_arg);
						return;
					}
					vnum = FCT_VNUM(fct);
					ok = TRUE;
					break;
				}
			}
			
			// did we find one?
			if (!ok) {
				msg_to_char(ch, "Unable to find %s %d.\r\n", quest_reward_types[stype], vnum);
				return;
			}
			
			// success
			CREATE(reward, struct quest_reward, 1);
			reward->type = stype;
			reward->amount = num;
			reward->vnum = vnum;
			
			LL_APPEND(*list, reward);
			msg_to_char(ch, "You add %s %s reward: %s\r\n", AN(quest_reward_types[stype]), quest_reward_types[stype], quest_reward_string(reward, TRUE));
		}
	}	// end 'add'
	else if (is_abbrev(cmd_arg, "change")) {
		// usage: rewards change <number> <amount | vnum> <value>
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		argument = any_one_word(argument, vnum_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*field_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: rewards change <number> <amount | vnum> <value>\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		LL_FOREACH(*list, iter) {
			if (--num == 0) {
				change = iter;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid reward number.\r\n");
		}
		else if (is_abbrev(field_arg, "amount") || is_abbrev(field_arg, "quantity")) {
			if (!isdigit(*vnum_arg) || (num = atoi(vnum_arg)) < 0) {
				msg_to_char(ch, "Invalid amount '%s'.\r\n", vnum_arg);
				return;
			}
			else {
				change->amount = num;
				msg_to_char(ch, "You change reward %d to: %s\r\n", atoi(num_arg), quest_reward_string(change, TRUE));
			}
		}
		else if (is_abbrev(field_arg, "vnum")) {
			// QR_x: validate vnum
			vnum = 0;
			ok = FALSE;
			switch (change->type) {
				case QR_BONUS_EXP: {
					msg_to_char(ch, "You can't change the vnum on that.\r\n");
					break;
				}
				case QR_COINS: {
					if (is_abbrev(vnum_arg, "miscellaneous") || is_abbrev(vnum_arg, "simple") || is_abbrev(vnum_arg, "other")) {
						vnum = OTHER_COIN;
						ok = TRUE;
					}
					else if (is_abbrev(vnum_arg, "empire")) {
						vnum = REWARD_EMPIRE_COIN;
						ok = TRUE;
					}
					else {
						msg_to_char(ch, "You must choose misc or empire coins.\r\n");
						return;
					}
					break;	
				}
				case QR_CURRENCY: {
					if (!*vnum_arg || !isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
						msg_to_char(ch, "Invalid currency vnum '%s'.\r\n", vnum_arg);
						return;
					}
					if (find_generic(vnum, GENERIC_CURRENCY)) {
						ok = TRUE;
					}
					break;
				}
				case QR_OBJECT: {
					if (!*vnum_arg || !isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
						msg_to_char(ch, "Invalid obj vnum '%s'.\r\n", vnum_arg);
						return;
					}
					if (obj_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QR_SET_SKILL:
				case QR_SKILL_EXP:
				case QR_SKILL_LEVELS: {
					if (!*vnum_arg || !isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
						msg_to_char(ch, "Invalid skill vnum '%s'.\r\n", vnum_arg);
						return;
					}
					if (find_skill_by_vnum(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QR_QUEST_CHAIN: {
					if (!*vnum_arg || !isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
						msg_to_char(ch, "Invalid quest vnum '%s'.\r\n", vnum_arg);
						return;
					}
					if (quest_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QR_REPUTATION: {
					if (!*vnum_arg || !(fct = find_faction(vnum_arg))) {
						msg_to_char(ch, "Invalid faction '%s'.\r\n", vnum_arg);
						return;
					}
					vnum = FCT_VNUM(fct);
					ok = TRUE;
					break;
				}
			}
			
			// did we find one?
			if (!ok) {
				msg_to_char(ch, "Unable to find %s %d.\r\n", quest_giver_types[change->type], vnum);
				return;
			}
			
			change->vnum = vnum;
			msg_to_char(ch, "Changed reward %d to: %s\r\n", atoi(num_arg), quest_reward_string(change, TRUE));
		}
		else {
			msg_to_char(ch, "You can only change the amount or vnum.\r\n");
		}
	}	// end 'change'
	else if (is_abbrev(cmd_arg, "move")) {
		struct quest_reward *to_move, *prev, *a, *b, *a_next, *b_next, iitem;
		bool up;
		
		// usage: rewards move <number> <up | down>
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		up = is_abbrev(field_arg, "up");
		
		if (!*num_arg || !*field_arg) {
			msg_to_char(ch, "Usage: rewards move <number> <up | down>\r\n");
		}
		else if (!isdigit(*num_arg) || (num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid reward number.\r\n");
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
			for (reward = *list; reward && !to_move; reward = reward->next) {
				if (--num == 0) {
					to_move = reward;
				}
				else {
					// store for next iteration
					prev = reward;
				}
			}
			
			if (!to_move) {
				msg_to_char(ch, "Invalid reward number.\r\n");
			}
			else if (!up && !to_move->next) {
				msg_to_char(ch, "You can't move it down; it's already at the bottom of the list.\r\n");
			}
			else {
				// SUCCESS: "move" them by swapping data
				if (up) {
					a = prev;
					b = to_move;
				}
				else {
					a = to_move;
					b = to_move->next;
				}
				
				// store next pointers
				a_next = a->next;
				b_next = b->next;
				
				// swap data
				iitem = *a;
				*a = *b;
				*b = iitem;
				
				// restore next pointers
				a->next = a_next;
				b->next = b_next;
				
				// message: re-atoi(num_arg) because we destroyed num finding our target
				msg_to_char(ch, "You move reward %d %s.\r\n", atoi(num_arg), (up ? "up" : "down"));
			}
		}
	}	// end 'move'
	else {
		msg_to_char(ch, "Usage: rewards add <type> <amount> <vnum/type>\r\n");
		msg_to_char(ch, "Usage: rewards change <number> vnum <value>\r\n");
		msg_to_char(ch, "Usage: rewards copy <from type> <from vnum>\r\n");
		msg_to_char(ch, "Usage: rewards remove <number | all>\r\n");
		msg_to_char(ch, "Usage: rewards move <number> <up | down>\r\n");
	}
}

*/
