/* ************************************************************************
*   File: faction.c                                       EmpireMUD 2.0b5 *
*  Usage: code related to factions, including DB and OLC                  *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
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
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "olc.h"

/**
* Contents:
*   Helpers
*   Utilities
*   Database
*   Reputation Handlers
*   OLC Handlers
*   Displays
*   Edit Modules
*/

// local data
const char *default_faction_name = "Unnamed Faction";
const int default_min_rep = REP_DESPISED;
const int default_max_rep = REP_REVERED;
const int default_starting_rep = REP_NEUTRAL;

// local protos
void get_faction_relation_display(struct faction_relation *list, char *save_buffer);

// external consts
extern const char *faction_flags[];
extern const char *relationship_flags[];
extern struct faction_reputation_type reputation_levels[];

// external funcs
extern bool delete_quest_reward_from_list(struct quest_reward **list, int type, any_vnum vnum);
extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
extern bool find_quest_reward_in_list(struct quest_reward *list, int type, any_vnum vnum);
extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);


 /////////////////////////////////////////////////////////////////////////////
//// HELPERS ////////////////////////////////////////////////////////////////

/**
* Gets the name of a faction by vnum.
*
* @param any_vnum vnum Which faction.
* @return const char* Its name.
*/
const char *get_faction_name_by_vnum(any_vnum vnum) {
	const char *unknown = "UNKNOWN";
	faction_data *fct;
	
	if ((fct = find_faction_by_vnum(vnum))) {
		return FCT_NAME(fct);
	}
	else {
		return unknown;
	}
}


/**
* Gets the name of a reputation by REP_ type.
*
* @param int type REP_ const
* @return const char* The name it uses.
*/
const char *get_reputation_name(int type) {
	const char *unknown = "UNKNOWN";
	int iter;
	
	for (iter = 0; *reputation_levels[iter].name != '\n'; ++iter) {
		if (reputation_levels[iter].type == type) {
			return reputation_levels[iter].name;
		}
	}
	
	// not found
	return unknown;
}


/**
* Finds a faction by ambiguous argument, which may be a vnum or a name.
* Names are matched by exact match first, or by multi-abbrev.
*
* @param char *argument The user input.
* @return faction_data* The faction, or NULL if it doesn't exist.
*/
faction_data *find_faction(char *argument) {
	faction_data *fct;
	any_vnum vnum;
	
	if (isdigit(*argument) && (vnum = atoi(argument)) >= 0 && (fct = find_faction_by_vnum(vnum))) {
		return fct;
	}
	else {
		return find_faction_by_name(argument);
	}
}


/**
* Look up a faction by multi-abbrev, preferring exact matches.
*
* @param char *name The faction name to look up.
* @return faction_data* The faction, or NULL if it doesn't exist.
*/
faction_data *find_faction_by_name(char *name) {
	faction_data *fct, *next_fct, *partial = NULL;
	
	if (!*name) {
		return NULL;	// shortcut
	}
	
	HASH_ITER(sorted_hh, sorted_factions, fct, next_fct) {
		// matches:
		if (!str_cmp(name, FCT_NAME(fct))) {
			// perfect match
			return fct;
		}
		if (!partial && multi_isname(name, FCT_NAME(fct))) {
			// probable match
			partial = fct;
		}
	}
	
	// no exact match...
	return partial;
}


/**
* @param any_vnum vnum Any faction vnum
* @return faction_data* The faction, or NULL if it doesn't exist
*/
faction_data *find_faction_by_vnum(any_vnum vnum) {
	faction_data *fct;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(faction_table, &vnum, fct);
	return fct;
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common faction problems and reports them to ch.
*
* @param faction_data *fct The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_faction(faction_data *fct, char_data *ch) {
	struct faction_relation *rel, *next_rel, *back;
	int min_idx, max_idx, start_idx;
	bool problem = FALSE;
	
	if (FACTION_FLAGGED(fct, FCT_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, FCT_VNUM(fct), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!FCT_NAME(fct) || !*FCT_NAME(fct) || !str_cmp(FCT_NAME(fct), default_faction_name)) {
		olc_audit_msg(ch, FCT_VNUM(fct), "No name set");
		problem = TRUE;
	}
	if (!FCT_DESCRIPTION(fct) || !*FCT_DESCRIPTION(fct)) {
		olc_audit_msg(ch, FCT_VNUM(fct), "No description set");
		problem = TRUE;
	}
	
	// limits
	min_idx = rep_const_to_index(FCT_MIN_REP(fct));
	max_idx = rep_const_to_index(FCT_MAX_REP(fct));
	start_idx = rep_const_to_index(FCT_STARTING_REP(fct));
	if (min_idx == NOTHING || max_idx == NOTHING || start_idx == NOTHING) {
		olc_audit_msg(ch, FCT_VNUM(fct), "Invalid reputation settings");
		problem = TRUE;
	}
	if (max_idx < min_idx) {
		olc_audit_msg(ch, FCT_VNUM(fct), "Max reputation is lower than min reputation");
		problem = TRUE;
	}
	if (start_idx < min_idx || start_idx > max_idx) {
		olc_audit_msg(ch, FCT_VNUM(fct), "Starting reputation is out of bounds");
		problem = TRUE;
	}
	
	HASH_ITER(hh, FCT_RELATIONS(fct), rel, next_rel) {
		if (IS_SET(rel->flags, FCTR_SHARED_GAINS) && IS_SET(rel->flags, FCTR_INVERSE_GAINS | FCTR_MUTUALLY_EXCLUSIVE)) {
			olc_audit_msg(ch, FCT_VNUM(fct), "Nonsensical relationship flags with %d %s", FCT_VNUM(rel->ptr), FCT_NAME(rel->ptr));
			problem = TRUE;
		}
		
		HASH_FIND_INT(FCT_RELATIONS(rel->ptr), &FCT_VNUM(fct), back);
		if (!back || rel->flags != back->flags) {
			olc_audit_msg(ch, FCT_VNUM(fct), "Asymmetrical relationship flags with %d %s", FCT_VNUM(rel->ptr), FCT_NAME(rel->ptr));
			problem = TRUE;
		}
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param faction_data *fct The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_faction(faction_data *fct, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s", FCT_VNUM(fct), FCT_NAME(fct));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", FCT_VNUM(fct), FCT_NAME(fct));
	}
		
	return output;
}


/**
* Searches for all uses of an faction and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The faction vnum.
*/
void olc_search_faction(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	faction_data *fct = find_faction_by_vnum(vnum);
	faction_data *iter, *next_iter, *find;
	quest_data *qiter, *next_qiter;
	event_data *event, *next_event;
	progress_data *prg, *next_prg;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	char_data *mob, *next_mob;
	int size, found;
	bool any;
	
	if (!fct) {
		msg_to_char(ch, "There is no faction %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of faction %d (%s):\r\n", vnum, FCT_NAME(fct));
	
	// events
	HASH_ITER(hh, event_table, event, next_event) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QR_x: event rewards
		any = find_event_reward_in_list(EVT_RANK_REWARDS(event), QR_REPUTATION, vnum);
		any |= find_event_reward_in_list(EVT_THRESHOLD_REWARDS(event), QR_REPUTATION, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "EVT [%5d] %s\r\n", EVT_VNUM(event), EVT_NAME(event));
		}
	}
	
	// check other factions
	HASH_ITER(hh, faction_table, iter, next_iter) {
		HASH_FIND_INT(FCT_RELATIONS(iter), &FCT_VNUM(fct), find);
		if (find) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "FCT [%5d] %s\r\n", FCT_VNUM(iter), FCT_NAME(iter));
		}
	}
	
	// check mob allegiance
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		if (MOB_FACTION(mob) == fct) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "MOB [%5d] %s\r\n", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob));
		}
	}
	
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_REP_OVER, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_REP_UNDER, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "PRG [%5d] %s\r\n", PRG_VNUM(prg), PRG_NAME(prg));
		}
	}
	
	// check quests
	HASH_ITER(hh, quest_table, qiter, next_qiter) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QR_x, REQ_x: quest types
		any = find_requirement_in_list(QUEST_TASKS(qiter), REQ_REP_OVER, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(qiter), REQ_REP_OVER, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(qiter), REQ_REP_UNDER, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(qiter), REQ_REP_UNDER, vnum);
		any |= find_quest_reward_in_list(QUEST_REWARDS(qiter), QR_REPUTATION, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(qiter), QUEST_NAME(qiter));
		}
	}
	
	// check shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		if (size >= sizeof(buf)) {
			break;
		}
		
		if (SHOP_ALLEGIANCE(shop) == fct) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SHOP [%5d] %s\r\n", SHOP_VNUM(shop), SHOP_NAME(shop));
		}
	}
	
	// check socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirements
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_REP_OVER, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_REP_UNDER, vnum);
		
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


// Simple vnum sorter for the faction hash
int sort_factions(faction_data *a, faction_data *b) {
	return FCT_VNUM(a) - FCT_VNUM(b);
}


// alphabetic sorter for sorted_factions
int sort_factions_by_data(faction_data *a, faction_data *b) {
	return strcmp(NULLSAFE(FCT_NAME(a)), NULLSAFE(FCT_NAME(b)));
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts a faction into the hash table.
*
* @param faction_data *fct The faction data to add to the table.
*/
void add_faction_to_table(faction_data *fct) {
	faction_data *find;
	any_vnum vnum;
	
	if (fct) {
		vnum = FCT_VNUM(fct);
		HASH_FIND_INT(faction_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(faction_table, vnum, fct);
			HASH_SORT(faction_table, sort_factions);
		}
		
		// sorted table
		HASH_FIND(sorted_hh, sorted_factions, &vnum, sizeof(int), find);
		if (!find) {
			HASH_ADD(sorted_hh, sorted_factions, vnum, sizeof(int), fct);
			HASH_SRT(sorted_hh, sorted_factions, sort_factions_by_data);
		}
	}
}


/**
* Removes a faction from the hash table.
*
* @param faction_data *fct The faction data to remove from the table.
*/
void remove_faction_from_table(faction_data *fct) {
	HASH_DEL(faction_table, fct);
	HASH_DELETE(sorted_hh, sorted_factions, fct);
}


/**
* Validates and links all the faction relationships on startup.
*/
void check_and_link_faction_relations(void) {
	struct faction_relation *rel, *next_rel;
	faction_data *fct, *next_fct;
	
	HASH_ITER(hh, faction_table, fct, next_fct) {
		HASH_ITER(hh, FCT_RELATIONS(fct), rel, next_rel) {
			rel->ptr = find_faction_by_vnum(rel->vnum);
			
			if (!rel->ptr) {
				HASH_DEL(FCT_RELATIONS(fct), rel);
				free(rel);
			}
		}
	}
}


/**
* Initializes a new faction. This clears all memory for it, so set the vnum
* AFTER.
*
* @param faction_data *fct The faction to initialize.
*/
void clear_faction(faction_data *fct) {
	memset((char *) fct, 0, sizeof(faction_data));
	
	FCT_VNUM(fct) = NOTHING;
}


/**
* Duplicates a hash table of faction relationships, for editing.
*
* @param struct faction_relation *input The hash table to copy.
* @return struct faction_relation* The copied hash.
*/
struct faction_relation *copy_faction_relations(struct faction_relation *input) {
	struct faction_relation *el, *iter, *next_iter, *hash = NULL;
	
	HASH_ITER(hh, input, iter, next_iter) {
		CREATE(el, struct faction_relation, 1);
		*el = *iter;
		HASH_ADD_INT(hash, vnum, el);
	}
	
	return hash;
}


/**
* @param struct faction_relation *hash Frees the memory for this hash table.
*/
void free_faction_relations(struct faction_relation *hash) {
	struct faction_relation *tmp, *next;
	
	HASH_ITER(hh, hash, tmp, next) {
		HASH_DEL(hash, tmp);
		free(tmp);
	}
}


/**
* frees up memory for a faction data item.
*
* See also: olc_delete_faction
*
* @param faction_data *fct The faction data to free.
*/
void free_faction(faction_data *fct) {
	faction_data *proto = find_faction_by_vnum(FCT_VNUM(fct));
	
	if (FCT_NAME(fct) && (!proto || FCT_NAME(fct) != FCT_NAME(proto))) {
		free(FCT_NAME(fct));
	}
	if (FCT_DESCRIPTION(fct) && (!proto || FCT_DESCRIPTION(fct) != FCT_DESCRIPTION(proto))) {
		free(FCT_DESCRIPTION(fct));
	}
	
	if (FCT_RELATIONS(fct) && (!proto || FCT_RELATIONS(fct) != FCT_RELATIONS(proto))) {
		free_faction_relations(FCT_RELATIONS(fct));
	}
	
	free(fct);
}


/**
* Read one faction from file.
*
* @param FILE *fl The open .fct file
* @param any_vnum vnum The faction vnum
*/
void parse_faction(FILE *fl, any_vnum vnum) {
	char line[256], error[256], str_in[256];
	struct faction_relation *rel;
	faction_data *fct, *find;
	int int_in[4];
	
	CREATE(fct, faction_data, 1);
	clear_faction(fct);
	FCT_VNUM(fct) = vnum;
	
	HASH_FIND_INT(faction_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate faction vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_faction_to_table(fct);
		
	// for error messages
	sprintf(error, "faction vnum %d", vnum);
	
	// lines 1-2: name, desc
	FCT_NAME(fct) = fread_string(fl, error);
	FCT_DESCRIPTION(fct) = fread_string(fl, error);
	
	// line 3: flags
	if (!get_line(fl, line) || sscanf(line, "%s", str_in) != 1) {
		log("SYSERR: Format error in line 3 of %s", error);
		exit(1);
	}
	
	FCT_FLAGS(fct) = asciiflag_conv(str_in);
	
	// line 4: rep levels
	if (!get_line(fl, line) || sscanf(line, "%d %d %d", &int_in[0], &int_in[1], &int_in[2]) != 3) {
		log("SYSERR: Format error in line 4 of %s", error);
		exit(1);
	}
	
	FCT_MIN_REP(fct) = int_in[0];
	FCT_MAX_REP(fct) = int_in[1];
	FCT_STARTING_REP(fct) = int_in[2];
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'R': {	// relations
				if (sscanf(line, "R %d %s", &int_in[0], str_in) != 2) {
					log("SYSERR: Format error in line R of %s", error);
					exit(1);
				}
				
				HASH_FIND_INT(FCT_RELATIONS(fct), &int_in[0], rel);
				if (!rel) {
					CREATE(rel, struct faction_relation, 1);
				}
				
				rel->vnum = int_in[0];
				rel->flags = asciiflag_conv(str_in);
				HASH_ADD_INT(FCT_RELATIONS(fct), vnum, rel);
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


// writes entries in the faction index
void write_faction_index(FILE *fl) {
	faction_data *fct, *next_fct;
	int this, last;
	
	last = NO_WEAR;
	HASH_ITER(hh, faction_table, fct, next_fct) {
		// determine "zone number" by vnum
		this = (int)(FCT_VNUM(fct) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, FCT_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one faction in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param faction_data *fct The thing to save.
*/
void write_faction_to_file(FILE *fl, faction_data *fct) {
	struct faction_relation *rel, *next_rel;
	char temp[MAX_STRING_LENGTH];
	
	if (!fl || !fct) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_faction_to_file called without %s", !fl ? "file" : "faction");
		return;
	}
	
	fprintf(fl, "#%d\n", FCT_VNUM(fct));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(FCT_NAME(fct)));
	
	// 2. desc
	strcpy(temp, NULLSAFE(FCT_DESCRIPTION(fct)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// 3. flags
	fprintf(fl, "%s\n", bitv_to_alpha(FCT_FLAGS(fct)));
	
	// 4. rep
	fprintf(fl, "%d %d %d\n", FCT_MIN_REP(fct), FCT_MAX_REP(fct), FCT_STARTING_REP(fct));
	
	// 'R': relations
	HASH_ITER(hh, FCT_RELATIONS(fct), rel, next_rel) {
		fprintf(fl, "R %d %s\n", rel->vnum, bitv_to_alpha(rel->flags));
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// REPUTATION HANDLERS /////////////////////////////////////////////////////

/**
* Comparison function to determine if one REP_ const is higher than another.
* This does NOT account for 'magnitude', where a negative rank is "higher"
* than another negative rank if its absolute value is larger.
*/
int compare_reptuation(int rep_a, int rep_b) {
	int ind_a, ind_b;
	
	ind_a = rep_const_to_index(rep_a);
	ind_b = rep_const_to_index(rep_b);
	
	if (ind_a < ind_b) {
		return -1;
	}
	else if (ind_a > ind_b) {
		return 1;
	}
	else {
		return 0;
	}
}


/**
* Causes basic reputation gain or loss.
*
* @param char_data *ch The player.
* @param any_vnum vnum The faction to gain/lose rep with.
* @param int amount The value to gain/lose (bounded by rep limits).
* @param bool is_kill If TRUE, gains can be toggled by faction flags.
* @param bool cascade If TRUE, will call this function recursively on shared-/inverse-gain factions.
*/
void gain_reputation(char_data *ch, any_vnum vnum, int amount, bool is_kill, bool cascade) {
	int idx, min_idx, max_idx, min_rep, max_rep, old_val, old_rep;
	struct faction_relation *rel, *next_rel;
	faction_data *fct, *iter, *next_iter;
	struct player_faction_data *pfd;
	bool changed;
	
	if (IS_NPC(ch) || amount == 0) {
		return;
	}
	if (!(fct = find_faction_by_vnum(vnum)) || FACTION_FLAGGED(fct, FCT_IN_DEVELOPMENT)) {
		return;	// faction doesn't exist?
	}
	if (!(pfd = get_reputation(ch, vnum, TRUE))) {
		return;	// unable to get a rep entry?
	}
	
	// check for exclusions
	HASH_ITER(hh, FCT_RELATIONS(fct), rel, next_rel) {
		if (IS_SET(rel->flags, FCTR_MUTUALLY_EXCLUSIVE) && amount > 0 && get_reputation_value(ch, rel->vnum) > 0) {
			return;	// can't gain while it's positive
		}
	}
	
	// safe to attempt to cascade NOW (before we cancel out due to min/max)
	if (cascade) {
		HASH_ITER(hh, faction_table, iter, next_iter) {
			if (iter == fct || FACTION_FLAGGED(iter, FCT_IN_DEVELOPMENT)) {
				continue;
			}
			
			HASH_FIND_INT(FCT_RELATIONS(iter), &FCT_VNUM(fct), rel);
			if (rel) {
				if (IS_SET(rel->flags, FCTR_SHARED_GAINS)) {
					gain_reputation(ch, FCT_VNUM(iter), amount, is_kill, FALSE);
				}
				if (IS_SET(rel->flags, FCTR_INVERSE_GAINS)) {
					gain_reputation(ch, FCT_VNUM(iter), -amount, is_kill, FALSE);
				}
			}
		}
	}
	
	// limits based on kills (do AFTER cascading)
	if (is_kill && !FACTION_FLAGGED(fct, FCT_REP_FROM_KILLS)) {
		return;
	}
	
	// bounds
	min_idx = rep_const_to_index(FCT_MIN_REP(fct));
	min_rep = (min_idx != NOTHING) ? reputation_levels[min_idx].value : MIN_REPUTATION;
	if (amount < 0 && pfd->value <= min_rep) {
		return;	// at min
	}
	max_idx = rep_const_to_index(FCT_MAX_REP(fct));
	max_rep = (max_idx != NOTHING) ? reputation_levels[max_idx].value : MAX_REPUTATION;
	if (amount > 0 && pfd->value >= max_rep) {
		return;	// at max
	}
	
	// seems ok
	old_val = pfd->value;
	old_rep = pfd->rep;
	SAFE_ADD(pfd->value, amount, MIN_REPUTATION, MAX_REPUTATION, FALSE);
	
	// detect new rep now
	do {
		changed = FALSE;
		idx = rep_const_to_index(pfd->rep);
		if (amount > 0) {
			if (pfd->value > 0 && pfd->value >= reputation_levels[idx+1].value) {
				pfd->rep = reputation_levels[idx+1].type;
				changed = TRUE;
			}
			else if (pfd->value < 0 && pfd->value > reputation_levels[idx].value) {
				pfd->rep = reputation_levels[idx+1].type;
				changed = TRUE;
			}
		}
		else if (amount < 0) {
			if (pfd->value > 0 && pfd->value < reputation_levels[idx].value) {
				pfd->rep = reputation_levels[idx-1].type;
				changed = TRUE;
			}
			else if (pfd->value < 0 && pfd->value <= reputation_levels[idx-1].value) {
				pfd->rep = reputation_levels[idx-1].type;
				changed = TRUE;
			}
		}
	}
	while (changed);
	
	// and message
	if (cascade) {
		idx = rep_const_to_index(pfd->rep);
		amount = pfd->value - old_val;
		msg_to_char(ch, "%sYou %s %d reputation with %s.\t0\r\n", reputation_levels[idx].color, (amount > 0 ? "gain" : "lose"), ABSOLUTE(amount), FCT_NAME(fct));
		if (old_rep != pfd->rep) {
			msg_to_char(ch, "%sYou are now %s with %s.\t0\r\n", reputation_levels[idx].color, reputation_levels[idx].name, FCT_NAME(fct));
			qt_change_reputation(ch, FCT_VNUM(fct));
		}
	}
}


/**
* Finds a player's reputation data for a faction. This will attempt (not
* guarantee) to create an entry if it doesn't exist, if desired.
*
* @param char_data *ch The player.
* @param any_vnum A faction vnum.
* @param bool create If TRUE, will attempt to create the entry.
* @return struct player_faction_data* The player's faction rep entry, if it exists.
*/
struct player_faction_data *get_reputation(char_data *ch, any_vnum vnum, bool create) {
	struct player_faction_data *pfd;
	faction_data *fct;
	int idx;
	
	if (IS_NPC(ch)) {
		return NULL;	// only players have this
	}
	
	HASH_FIND_INT(GET_FACTIONS(ch), &vnum, pfd);
	if (!pfd && create && (fct = find_faction_by_vnum(vnum))) {
		CREATE(pfd, struct player_faction_data, 1);
		pfd->vnum = vnum;
		idx = rep_const_to_index(FCT_STARTING_REP(fct));
		pfd->rep = idx != NOTHING ? FCT_STARTING_REP(fct) : REP_NEUTRAL;
		pfd->value = (idx != NOTHING ? reputation_levels[idx].value : 0);
		HASH_ADD_INT(GET_FACTIONS(ch), vnum, pfd);
	}
	
	return pfd;
}


/**
* Finds a reputation const by name.
*
* @param char *name The name to find.
* @return int Any REP_ const, or NOTHING if not found.
*/
int get_reputation_by_name(char *name) {
	int iter, partial = NOTHING;
	
	for (iter = 0; *reputation_levels[iter].name != '\n'; ++iter) {
		if (!str_cmp(name, reputation_levels[iter].name)) {
			return reputation_levels[iter].type;
		}
		else if (is_abbrev(name, reputation_levels[iter].name)) {
			partial = iter;
		}
	}
	
	return partial != NOTHING ? reputation_levels[partial].type : NOTHING;
}


/**
* Fetches a player's reputation value with a given faction, if they have one.
*
* @param char_data *ch The player.
* @param any_vnum vnum The faction's vnum.
* @return int The reputation value, or 0 if no reputation exists.
*/
int get_reputation_value(char_data *ch, any_vnum vnum) {
	struct player_faction_data *pfd = get_reputation(ch, vnum, FALSE);
	return pfd ? pfd->value : 0;
}


/**
* Determines if a character's reputation with a faction is at least this high.
*
* @param char_data *ch The player.
* @param any_vnum faction Which faction.
* @param int rep Any REP_ const.
* @return bool TRUE if it's high enough.
*/
bool has_reputation(char_data *ch, any_vnum faction, int rep) {
	struct player_faction_data *pfd = get_reputation(ch, faction, TRUE);
	return pfd ? (rep_const_to_index(pfd->rep) >= rep_const_to_index(rep)) : FALSE;
}


/**
* Detect the min/max values for reputation at startup.
*/
void init_reputation(void) {
	int iter;
	
	for (iter = 0; *reputation_levels[iter].name != '\n'; ++iter) {
		MAX_REPUTATION = MAX(MAX_REPUTATION, reputation_levels[iter].value);
		MIN_REPUTATION = MIN(MIN_REPUTATION, reputation_levels[iter].value);
	}
}


/**
* Converts a REP_ const into a reputation_levels[] index.
*
* @param int rep_const Any REP_ type.
* @return int Its location in the reputation_levels array, or NOTHING if not found.
*/
int rep_const_to_index(int rep_const) {
	int iter;
	
	for (iter = 0; *reputation_levels[iter].name != '\n'; ++iter) {
		if (reputation_levels[iter].type == rep_const) {
			return iter;
		}
	}
	
	return NOTHING;	// not found
}


/**
* Sets a reputation to a specific level. This will be constrained by the
* faction's min/max reputation.
*
* @param char_data *ch The player.
* @param any_vnum vnum The faction to set rep with.
* @param int rep Any REP_ constant to set it to.
*/
void set_reputation(char_data *ch, any_vnum vnum, int rep) {
	int rep_idx, min_idx, max_idx, old_rep;
	struct player_faction_data *pfd;
	faction_data *fct;
	
	if (IS_NPC(ch) || rep == REP_NONE) {
		return;
	}
	if (!(fct = find_faction_by_vnum(vnum)) || FACTION_FLAGGED(fct, FCT_IN_DEVELOPMENT)) {
		return;	// faction doesn't exist?
	}
	if (!(pfd = get_reputation(ch, vnum, TRUE))) {
		return;	// unable to get a rep entry?
	}
	if ((rep_idx = rep_const_to_index(rep)) == NOTHING) {
		return;	// no valid reputation?
	}
	
	// bounds
	min_idx = rep_const_to_index(FCT_MIN_REP(fct));
	rep_idx = MAX(rep_idx, min_idx);
	max_idx = rep_const_to_index(FCT_MAX_REP(fct));
	rep_idx = MIN(rep_idx, max_idx);
	
	// seems ok
	old_rep = pfd->rep;
	
	pfd->rep = rep;
	pfd->value = reputation_levels[rep_idx].value;
	
	// and message
	if (old_rep != pfd->rep) {
		msg_to_char(ch, "%sYou are now %s with %s.\t0\r\n", reputation_levels[rep_idx].color, reputation_levels[rep_idx].name, FCT_NAME(fct));
		qt_change_reputation(ch, FCT_VNUM(fct));
	}
}


/**
* Updates all of a player's REP_ consts on their factions.
*
* @param char_data *ch The player.
*/
void update_reputations(char_data *ch) {
	struct player_faction_data *pfd, *next_pfd;
	int iter, min_idx, max_idx;
	faction_data *fct;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	HASH_ITER(hh, GET_FACTIONS(ch), pfd, next_pfd) {
		if (!(fct = find_faction_by_vnum(pfd->vnum)) || FACTION_FLAGGED(fct, FCT_IN_DEVELOPMENT)) {
			HASH_DEL(GET_FACTIONS(ch), pfd);
			free(pfd);
			continue;
		}
		
		min_idx = rep_const_to_index(FCT_MIN_REP(fct));
		max_idx = rep_const_to_index(FCT_MAX_REP(fct));
		pfd->value = MAX(reputation_levels[min_idx].value, pfd->value);
		pfd->value = MIN(reputation_levels[max_idx].value, pfd->value);
		pfd->rep = FCT_STARTING_REP(fct);	// default
		
		for (iter = min_idx; iter <= max_idx; ++iter) {
			if (reputation_levels[iter].value == pfd->value) {
				pfd->rep = reputation_levels[iter].type;
				break;
			}
			else if (reputation_levels[iter].value < 0 && pfd->value <= reputation_levels[iter].value) {
				pfd->rep = reputation_levels[iter].type;
				break;	// break -- looking for lowest value match only
			}
			else if (reputation_levels[iter].value > 0 && pfd->value >= reputation_levels[iter].value) {
				pfd->rep = reputation_levels[iter].type;
				// don't break, looking for highest value
			}
		}
		
		qt_change_reputation(ch, pfd->vnum);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////

/**
* Creates a new faction entry.
* 
* @param any_vnum vnum The number to create.
* @return faction_data* The new faction's prototype.
*/
faction_data *create_faction_table_entry(any_vnum vnum) {
	faction_data *fct;
	
	// sanity
	if (find_faction_by_vnum(vnum)) {
		log("SYSERR: Attempting to insert faction at existing vnum %d", vnum);
		return find_faction_by_vnum(vnum);
	}
	
	CREATE(fct, faction_data, 1);
	clear_faction(fct);
	FCT_VNUM(fct) = vnum;
	FCT_NAME(fct) = str_dup(default_faction_name);
	add_faction_to_table(fct);

	// save index and faction file now
	save_index(DB_BOOT_FCT);
	save_library_file_for_vnum(DB_BOOT_FCT, vnum);

	return fct;
}


/**
* WARNING: This function actually deletes a faction.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_faction(char_data *ch, any_vnum vnum) {
	faction_data *fct, *iter, *next_iter, *find;
	char_data *mob, *next_mob, *chiter;
	event_data *event, *next_event;
	quest_data *qiter, *next_qiter;
	progress_data *prg, *next_prg;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	descriptor_data *desc;
	bool found;
	
	if (!(fct = find_faction_by_vnum(vnum))) {
		msg_to_char(ch, "There is no such faction %d.\r\n", vnum);
		return;
	}
	
	// remove it from the hash table first
	remove_faction_from_table(fct);
	
	// remove from live players
	LL_FOREACH(character_list, chiter) {
		if (IS_NPC(chiter) && MOB_FACTION(chiter) == fct) {
			MOB_FACTION(chiter) = NULL;
		}
		else if (!IS_NPC(chiter)) {
			update_reputations(chiter);
		}
	}

	
	// update events
	HASH_ITER(hh, event_table, event, next_event) {
		// QR_x: event reward types
		found = delete_event_reward_from_list(&EVT_RANK_REWARDS(event), QR_REPUTATION, vnum);
		found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(event), QR_REPUTATION, vnum);
		
		if (found) {
			// SET_BIT(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_EVT, EVT_VNUM(event));
		}
	}
	
	// remove from other factions
	HASH_ITER(hh, faction_table, iter, next_iter) {
		HASH_FIND_INT(FCT_RELATIONS(iter), &FCT_VNUM(fct), find);
		if (find) {
			HASH_DEL(FCT_RELATIONS(iter), find);
			free(find);
			save_library_file_for_vnum(DB_BOOT_FCT, FCT_VNUM(iter));
		}
	}
	
	// remove from mobs
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		if (MOB_FACTION(mob) == fct) {
			MOB_FACTION(mob) = NULL;
			save_library_file_for_vnum(DB_BOOT_MOB, GET_MOB_VNUM(mob));
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_REP_OVER, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_REP_UNDER, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// remove from quests
	HASH_ITER(hh, quest_table, qiter, next_qiter) {
		// REQ_x, QR_x: quest types
		found = delete_requirement_from_list(&QUEST_TASKS(qiter), REQ_REP_OVER, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(qiter), REQ_REP_OVER, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(qiter), REQ_REP_UNDER, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(qiter), REQ_REP_UNDER, vnum);
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(qiter), QR_REPUTATION, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(qiter), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(qiter));
		}
	}
	
	// remove from shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		if (SHOP_ALLEGIANCE(shop) == fct) {
			SHOP_ALLEGIANCE(shop) = NULL;
			SET_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SHOP, SHOP_VNUM(shop));
		}
	}
	
	// remove from socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		// REQ_x: requirements
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_REP_OVER, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_REP_UNDER, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// remove from active editors
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (GET_OLC_EVENT(desc)) {
			// QR_x: event reward types
			found = delete_event_reward_from_list(&EVT_RANK_REWARDS(GET_OLC_EVENT(desc)), QR_REPUTATION, vnum);
			found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(GET_OLC_EVENT(desc)), QR_REPUTATION, vnum);
		
			if (found) {
				// SET_BIT(EVT_FLAGS(GET_OLC_EVENT(desc)), EVTF_IN_DEVELOPMENT);
				msg_to_desc(desc, "A faction used as a reward by the event you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_FACTION(desc)) {
			HASH_FIND_INT(FCT_RELATIONS(GET_OLC_FACTION(desc)), &FCT_VNUM(fct), find);
			if (find) {
				HASH_DEL(FCT_RELATIONS(GET_OLC_FACTION(desc)), find);
				free(find);
				msg_to_char(desc->character, "A related faction for the faction you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_MOBILE(desc)) {
			if (MOB_FACTION(GET_OLC_MOBILE(desc)) == fct) {
				MOB_FACTION(GET_OLC_MOBILE(desc)) = NULL;
				msg_to_char(desc->character, "The allegiance faction for the mob you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_PROGRESS(desc)) {
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_REP_OVER, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_REP_UNDER, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "A faction used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			// REQ_x, QR_x: quest types
			found = delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_REP_OVER, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_REP_OVER, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_REP_UNDER, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_REP_UNDER, vnum);
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_REPUTATION, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "A faction used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SHOP(desc)) {
			if (SHOP_ALLEGIANCE(GET_OLC_SHOP(desc)) == fct) {
				SHOP_ALLEGIANCE(GET_OLC_SHOP(desc)) = NULL;
				SET_BIT(SHOP_FLAGS(GET_OLC_SHOP(desc)), SHOP_IN_DEVELOPMENT);
				msg_to_char(desc->character, "The allegiance faction for the shop you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			// REQ_x: requirements
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_REP_OVER, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_REP_UNDER, vnum);
		
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A faction required by the social you are editing was deleted.\r\n");
			}
		}
	}
	
	// save index and faction file now
	save_index(DB_BOOT_FCT);
	save_library_file_for_vnum(DB_BOOT_FCT, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted faction %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Faction %d deleted.\r\n", vnum);
	
	free_faction(fct);
}


/**
* Function to save a player's changes to a faction (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_faction(descriptor_data *desc) {	
	faction_data *proto, *fct = GET_OLC_FACTION(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh, sorted;
	char_data *chiter;

	// have a place to save it?
	if (!(proto = find_faction_by_vnum(vnum))) {
		proto = create_faction_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (FCT_NAME(proto)) {
		free(FCT_NAME(proto));
	}
	if (FCT_DESCRIPTION(proto)) {
		free(FCT_DESCRIPTION(proto));
	}
	free_faction_relations(FCT_RELATIONS(proto));
	
	// sanity
	if (!FCT_NAME(fct) || !*FCT_NAME(fct)) {
		if (FCT_NAME(fct)) {
			free(FCT_NAME(fct));
		}
		FCT_NAME(fct) = str_dup(default_faction_name);
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	sorted = proto->sorted_hh;
	*proto = *fct;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	proto->sorted_hh = sorted;
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_FCT, vnum);

	// ... and re-sort
	HASH_SRT(sorted_hh, sorted_factions, sort_factions_by_data);
	
	// update live players
	LL_FOREACH(character_list, chiter) {
		if (IS_NPC(chiter)) {
			continue;
		}
		update_reputations(chiter);
	}
}


/**
* Creates a copy of a faction, or clears a new one, for editing.
* 
* @param faction_data *input The faction to copy, or NULL to make a new one.
* @return faction_data* The copied faction.
*/
faction_data *setup_olc_faction(faction_data *input) {
	faction_data *new;
	
	CREATE(new, faction_data, 1);
	clear_faction(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		FCT_NAME(new) = FCT_NAME(input) ? str_dup(FCT_NAME(input)) : NULL;
		FCT_DESCRIPTION(new) = FCT_DESCRIPTION(input) ? str_dup(FCT_DESCRIPTION(input)) : NULL;
		
		// copy lists
		FCT_RELATIONS(new) = copy_faction_relations(FCT_RELATIONS(input));
	}
	else {
		// brand new: some defaults
		FCT_NAME(new) = str_dup(default_faction_name);
		FCT_FLAGS(new) = FCT_IN_DEVELOPMENT;
		FCT_MIN_REP(new) = default_min_rep;
		FCT_MAX_REP(new) = default_max_rep;
		FCT_STARTING_REP(new) = default_starting_rep;
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
* @param faction_data *fct The faction to display.
*/
void do_stat_faction(char_data *ch, faction_data *fct) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	size_t size;
	
	if (!fct) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \tc%s\t0\r\n%s", FCT_VNUM(fct), FCT_NAME(fct), NULLSAFE(FCT_DESCRIPTION(fct)));
	size += snprintf(buf + size, sizeof(buf) - size, "Min Reputation: \ty%s\t0, Max Reputation: \ty%s\t0, Starting Reputation: \ty%s\t0\r\n", get_reputation_name(FCT_MIN_REP(fct)), get_reputation_name(FCT_MAX_REP(fct)), get_reputation_name(FCT_STARTING_REP(fct)));
	
	sprintbit(FCT_FLAGS(fct), faction_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	get_faction_relation_display(FCT_RELATIONS(fct), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Relations:\r\n%s", part);
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Gets the faction relation display for olc, stat, or other uses.
*
* @param struct faction_relation *list The list of relations to display.
* @param char *save_buffer A buffer to store the display to.
*/
void get_faction_relation_display(struct faction_relation *list, char *save_buffer) {
	struct faction_relation *iter, *next_iter;
	char lbuf[MAX_STRING_LENGTH];
	
	*save_buffer = '\0';
	
	HASH_ITER(hh, list, iter, next_iter) {
		sprintbit(iter->flags, relationship_flags, lbuf, TRUE);
		sprintf(save_buffer + strlen(save_buffer), " [%5d] %s - %s\r\n", iter->vnum, FCT_NAME(iter->ptr), lbuf);
	}
	if (!list) {
		strcpy(save_buffer, " none\r\n");
	}
}


/**
* This is the main recipe display for faction OLC. It displays the user's
* currently-edited faction.
*
* @param char_data *ch The person who is editing a faction and will see its display.
*/
void olc_show_faction(char_data *ch) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	
	if (!fct) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !find_faction_by_vnum(FCT_VNUM(fct)) ? "new faction" : FCT_NAME(find_faction_by_vnum(FCT_VNUM(fct))));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(FCT_NAME(fct), default_faction_name), NULLSAFE(FCT_NAME(fct)));
	sprintf(buf + strlen(buf), "<%sdescription\t0>\r\n%s", OLC_LABEL_STR(FCT_DESCRIPTION(fct), ""), NULLSAFE(FCT_DESCRIPTION(fct)));
	
	sprintbit(FCT_FLAGS(fct), faction_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(FCT_FLAGS(fct), FCT_IN_DEVELOPMENT), lbuf);
	
	sprintf(buf + strlen(buf), "<%sminreputation\t0> %s\r\n", OLC_LABEL_VAL(FCT_MIN_REP(fct), default_min_rep), get_reputation_name(FCT_MIN_REP(fct)));
	sprintf(buf + strlen(buf), "<%smaxreputation\t0> %s\r\n", OLC_LABEL_VAL(FCT_MAX_REP(fct), default_max_rep), get_reputation_name(FCT_MAX_REP(fct)));
	sprintf(buf + strlen(buf), "<%sstartingreuptation\t0> %s\r\n", OLC_LABEL_VAL(FCT_STARTING_REP(fct), default_starting_rep), get_reputation_name(FCT_STARTING_REP(fct)));
	
	get_faction_relation_display(FCT_RELATIONS(fct), lbuf);
	sprintf(buf + strlen(buf), "Relationships: <%srelation\t0>, <%smatchrelations\t0>\r\n%s", OLC_LABEL_PTR(FCT_RELATIONS(fct)), OLC_LABEL_PTR(FCT_RELATIONS(fct)), FCT_RELATIONS(fct) ? lbuf : "");
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the faction db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_faction(char *searchname, char_data *ch) {
	faction_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, faction_table, iter, next_iter) {
		if (multi_isname(searchname, FCT_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, FCT_VNUM(iter), FCT_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(fedit_description) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", FCT_NAME(fct));
		start_string_editor(ch->desc, buf, &FCT_DESCRIPTION(fct), MAX_FACTION_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(fedit_flags) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	bool had_indev = IS_SET(FCT_FLAGS(fct), FCT_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	FCT_FLAGS(fct) = olc_process_flag(ch, argument, "faction", "flags", faction_flags, FCT_FLAGS(fct));
	
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !IS_SET(FCT_FLAGS(fct), FCT_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(FCT_FLAGS(fct), FCT_IN_DEVELOPMENT);
	}
}


OLC_MODULE(fedit_matchrelations) {
	faction_data *fct = GET_OLC_FACTION(ch->desc), *iter, *next_iter;
	struct faction_relation *find, *rel;
	bool found = FALSE;
	
	HASH_ITER(hh, faction_table, iter, next_iter) {
		if (FCT_VNUM(iter) == FCT_VNUM(fct)) {
			continue;
		}
		
		// find us
		HASH_FIND_INT(FCT_RELATIONS(iter), &FCT_VNUM(fct), find);
		if (!find) {
			continue;
		}
		
		// our entry
		HASH_FIND_INT(FCT_RELATIONS(fct), &FCT_VNUM(iter), rel);
		if (!rel) {
			CREATE(rel, struct faction_relation, 1);
			rel->vnum = FCT_VNUM(iter);
			rel->ptr = iter;
			HASH_ADD_INT(FCT_RELATIONS(fct), vnum, rel);
		}
		
		// copy
		rel->flags = find->flags;
		sprintbit(rel->flags, relationship_flags, buf, TRUE);
		msg_to_char(ch, "Matched [%5d] %s: %s\r\n", FCT_VNUM(iter), FCT_NAME(iter), buf);
		found = TRUE;
	}
	
	if (!found) {
		msg_to_char(ch, "No relations to match.\r\n");
	}
}


OLC_MODULE(fedit_maxreputation) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	int rep, idx;
	
	if (!*argument) {
		msg_to_char(ch, "Available reputation levels:\r\n");
		for (idx = 0; *reputation_levels[idx].name != '\n'; ++idx) {
			msg_to_char(ch, " %s%s\t0 (%d)\r\n", reputation_levels[idx].color, reputation_levels[idx].name, reputation_levels[idx].value);
		}
		return;
	}
	
	if ((rep = get_reputation_by_name(argument)) == NOTHING || (idx = rep_const_to_index(rep)) == NOTHING) {
		msg_to_char(ch, "Unknown reputation level '%s'.\r\n", argument);
		return;
	}
	
	FCT_MAX_REP(fct) = rep;
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		msg_to_char(ch, "You set the maximum reputation to %s.\r\n", reputation_levels[idx].name);
	}
}


OLC_MODULE(fedit_minreputation) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	int rep, idx;
	
	if (!*argument) {
		msg_to_char(ch, "Available reputation levels:\r\n");
		for (idx = 0; *reputation_levels[idx].name != '\n'; ++idx) {
			msg_to_char(ch, " %s%s\t0 (%d)\r\n", reputation_levels[idx].color, reputation_levels[idx].name, reputation_levels[idx].value);
		}
		return;
	}
	
	if ((rep = get_reputation_by_name(argument)) == NOTHING || (idx = rep_const_to_index(rep)) == NOTHING) {
		msg_to_char(ch, "Unknown reputation level '%s'.\r\n", argument);
		return;
	}
	
	FCT_MIN_REP(fct) = rep;
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		msg_to_char(ch, "You set the minimum reputation to %s.\r\n", reputation_levels[idx].name);
	}
}


OLC_MODULE(fedit_name) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	olc_process_string(ch, argument, "name", &FCT_NAME(fct));
}


OLC_MODULE(fedit_relation) {
	char which_arg[MAX_INPUT_LENGTH], cmd_arg[MAX_INPUT_LENGTH], more_args[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	faction_data *fct = GET_OLC_FACTION(ch->desc), *other;
	struct faction_relation *rel;
	
	// .rel <name/vnum> [flags | delete] [which flags]
	// .rel <name/vnum> -> shows status and mutual status?
	argument = any_one_word(argument, which_arg);
	half_chop(argument, cmd_arg, more_args);
	
	if (!*which_arg) {
		msg_to_char(ch, ".relation <name/vnum> [flags | delete] [which flags]\r\n");
	}
	else if (!(other = find_faction(which_arg))) {
		msg_to_char(ch, "Unknown faction '%s'.\r\n", which_arg);
	}
	else if (FCT_VNUM(other) == FCT_VNUM(fct)) {
		msg_to_char(ch, "Factions aren't allowed to have relations with themselves.\r\n");
	}
	else if (!*cmd_arg) {
		// show status: us to them
		HASH_FIND_INT(FCT_RELATIONS(fct), &FCT_VNUM(other), rel);
		if (rel) {
			sprintbit(rel->flags, relationship_flags, buf, TRUE);
			msg_to_char(ch, "%s -> %s: %s\r\n", FCT_NAME(fct), FCT_NAME(other), buf);
		}
		else {
			msg_to_char(ch, "%s -> %s: no relations\r\n", FCT_NAME(fct), FCT_NAME(other));
		}
		
		// show status: them
		HASH_FIND_INT(FCT_RELATIONS(other), &FCT_VNUM(fct), rel);
		if (rel) {
			sprintbit(rel->flags, relationship_flags, buf, TRUE);
			msg_to_char(ch, "%s -> %s: %s\r\n", FCT_NAME(other), FCT_NAME(fct), buf);
		}
		else {
			msg_to_char(ch, "%s -> %s: no relations\r\n", FCT_NAME(other), FCT_NAME(fct));
		}
	}
	else if (is_abbrev(cmd_arg, "delete") || is_abbrev(cmd_arg, "remove")) {
		// delete one
		HASH_FIND_INT(FCT_RELATIONS(fct), &FCT_VNUM(other), rel);
		if (!rel) {
			msg_to_char(ch, "There are no relations with that faction.\r\n");
		}
		else {
			HASH_DEL(FCT_RELATIONS(fct), rel);
			free(rel);
			msg_to_char(ch, "You delete the relations with %s.\r\n", FCT_NAME(other));
		}
	}
	else if (is_abbrev(cmd_arg, "flags")) {
		// find/add relations
		HASH_FIND_INT(FCT_RELATIONS(fct), &FCT_VNUM(other), rel);
		if (!rel) {
			CREATE(rel, struct faction_relation, 1);
			rel->vnum = FCT_VNUM(other);
			rel->ptr = other;
			HASH_ADD_INT(FCT_RELATIONS(fct), vnum, rel);
		}
		
		rel->flags = olc_process_flag(ch, more_args, "relationship", NULL, relationship_flags, rel->flags);
		
		// remove if no flags
		if (!rel->flags) {
			HASH_DEL(FCT_RELATIONS(fct), rel);
			free(rel);
		}
	}
	else {
		msg_to_char(ch, ".relation <name/vnum> [flags | delete] [which flags]\r\n");
	}
}


OLC_MODULE(fedit_startingreputation) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	int rep, idx;
	
	if (!*argument) {
		msg_to_char(ch, "Available reputation levels:\r\n");
		for (idx = 0; *reputation_levels[idx].name != '\n'; ++idx) {
			msg_to_char(ch, " %s%s\t0 (%d)\r\n", reputation_levels[idx].color, reputation_levels[idx].name, reputation_levels[idx].value);
		}
		return;
	}
	
	if ((rep = get_reputation_by_name(argument)) == NOTHING || (idx = rep_const_to_index(rep)) == NOTHING) {
		msg_to_char(ch, "Unknown reputation level '%s'.\r\n", argument);
		return;
	}
	
	FCT_STARTING_REP(fct) = rep;
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		msg_to_char(ch, "You set the starting reputation to %s.\r\n", reputation_levels[idx].name);
	}
}
