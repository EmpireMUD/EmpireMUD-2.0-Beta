/* ************************************************************************
*   File: olc.attack.c                                    EmpireMUD 2.0b5 *
*  Usage: loading, saving, OLC, and functions for attack messages         *
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
#include "constants.h"

/**
* Contents:
*   Helpers
*   Quick Getters
*   Dynamic Attack Messages
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   OLC Message Modules
*   OLC Modules
*/

// local data
const char *default_attack_name = "Unnamed Attack";
const char *default_attack_first_person = "attack";
const char *default_attack_third_person = "attacks";
const char *default_attack_noun = "attack";

const char *messages_file_header =
"* Note: all lines between records which start with '*' are comments and\n"
"* are ignored. Comments can only be between records, not within them.\n"
"*\n"
"* This file is where the damage messages go for offensive spells, and\n"
"* skills such as kick and backstab. Also, starting with Circle 3.0, these\n"
"* messages are used for failed spells, and weapon messages for misses and\n"
"* death blows.\n"
"*\n"
"* All records must start with 'M###' (for 'Message') where ### is the vnum.\n"
"* then the name with a tilde (~)\n"
"* then the messages (one per line):\n"
"*   Death Message (damager, damagee, onlookers)\n"
"*   Miss Message (damager, damagee, onlookers)\n"
"*   Hit message (damager, damagee, onlookers)\n"
"*   God message (damager, damagee, onlookers)\n"
"*\n"
"* All messages must be contained in one line. They will be sent through\n"
"* act(), meaning that all standard act() codes apply, and each message will\n"
"* automatically be wrapped to 79 columns when they are printed. '#' can\n"
"* be substituted for a message if none should be printed. Note however\n"
"* that, unlike the socials file, all twelve lines must be present for each\n"
"* record, regardless of any '#' fields which may be contained in it.\n";

// local funcs
void clear_attack_message(attack_message_data *amd);
void free_attack_message_set(struct attack_message_set *ams);
void write_attack_message_to_file(FILE *fl, attack_message_data *amd);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Fetch 1 message set from an attack type, either by number or at random.
*
* @param attack_message_data *amd The attack type to get 1 message from.
* @param int num Which number to get (1-N) or pass NOTHING to get one at random.
* @return struct attack_message_set* The message, or NULL if none found.
*/
struct attack_message_set *get_one_attack_message(attack_message_data *amd, int num) {
	struct attack_message_set *ams, *found = NULL;
	
	// shortcut
	if (!ATTACK_NUM_MSGS(amd) || !ATTACK_MSG_LIST(amd)) {
		return NULL;
	}
	
	// random?
	if (num == NOTHING) {
		num = number(1, ATTACK_NUM_MSGS(amd));
	}
	
	// find message
	LL_FOREACH(ATTACK_MSG_LIST(amd), ams) {
		if (--num <= 0) {
			found = ams;
			break;
		}
	}
	
	return found;	// if any
}


/**
* Counts the words of text in an attack's strings.
*
* @param attack_message_data *amd The attack whose strings to count.
* @return int The number of words in the attack's strings.
*/
int wordcount_attack_message(attack_message_data *amd) {
	struct attack_message_set *ams;
	int count = 0, iter;
	
	count += wordcount_string(ATTACK_NAME(amd));
	
	LL_FOREACH(ATTACK_MSG_LIST(amd), ams) {
		for (iter = 0; iter < NUM_MSG_TYPES; ++iter) {
			count += wordcount_string(ams->msg[iter].attacker_msg);
			count += wordcount_string(ams->msg[iter].victim_msg);
			count += wordcount_string(ams->msg[iter].room_msg);
		}
	}
	
	return count;
}


 //////////////////////////////////////////////////////////////////////////////
//// QUICK GETTERS ///////////////////////////////////////////////////////////

/**
* Gets a attack's DAMAGE_ type by vnum, safely.
*
* @param any_vnum vnum The attack vnum to look up -- these are also ATTACK_ or TYPE_ consts.
* @return int The DAMAGE_ type, or UNKNOWN if none.
*/
int get_attack_damage_type_by_vnum(any_vnum vnum) {
	attack_message_data *amd = real_attack_message(vnum);
	
	if (!amd) {
		return 0;
	}
	else {
		return ATTACK_DAMAGE_TYPE(amd);
	}
}


/**
* Gets a attack's first-person string by vnum, safely.
*
* @param any_vnum vnum The attack vnum to look up -- these are also ATTACK_ or TYPE_ consts.
* @return const char* The first-person string, or UNKNOWN if none.
*/
const char *get_attack_first_person_by_vnum(any_vnum vnum) {
	attack_message_data *amd = real_attack_message(vnum);
	
	if (!amd || !ATTACK_FIRST_PERSON(amd)) {
		return "UNKNOWN";	// sanity
	}
	else {
		return ATTACK_FIRST_PERSON(amd);
	}
}


/**
* Gets a attack's name by vnum, safely.
*
* @param any_vnum vnum The attack vnum to look up -- these are also ATTACK_ or TYPE_ consts.
* @return const char* The name, or UNKNOWN if none.
*/
const char *get_attack_name_by_vnum(any_vnum vnum) {
	attack_message_data *amd = real_attack_message(vnum);
	
	if (!amd) {
		return "UNKNOWN";	// sanity
	}
	else {
		return ATTACK_NAME(amd);
	}
}

/**
* Gets a attack's noun string by vnum, safely.
*
* @param any_vnum vnum The attack vnum to look up -- these are also ATTACK_ or TYPE_ consts.
* @return const char* The noun string, or UNKNOWN if none.
*/
const char *get_attack_noun_by_vnum(any_vnum vnum) {
	attack_message_data *amd = real_attack_message(vnum);
	
	if (!amd || !ATTACK_NOUN(amd)) {
		return "UNKNOWN";	// sanity
	}
	else {
		return ATTACK_NOUN(amd);
	}
}


/**
* Gets a attack's third-person string by vnum, safely.
*
* @param any_vnum vnum The attack vnum to look up -- these are also ATTACK_ or TYPE_ consts.
* @return const char* The third-person string, or UNKNOWN if none.
*/
const char *get_attack_third_person_by_vnum(any_vnum vnum) {
	attack_message_data *amd = real_attack_message(vnum);
	
	if (!amd || !ATTACK_THIRD_PERSON(amd)) {
		return "UNKNOWN";	// sanity
	}
	else {
		return ATTACK_THIRD_PERSON(amd);
	}
}


/**
* Gets a attack's WEAPON_ type by vnum, safely.
*
* @param any_vnum vnum The attack vnum to look up -- these are also ATTACK_ or TYPE_ consts.
* @return int The WEAPON_ type, or UNKNOWN if none.
*/
int get_attack_weapon_type_by_vnum(any_vnum vnum) {
	attack_message_data *amd = real_attack_message(vnum);
	
	if (!amd) {
		return 0;
	}
	else {
		return ATTACK_WEAPON_TYPE(amd);
	}
}


/**
* Tells you whether or not an attack type is flagged with a given flag, by
* vnum, safely.
*
* @param any_vnum vnum The attack vnum to look up -- these are also ATTACK_ or TYPE_ consts.
* @param bitvector_t amdf_flags Which AMDF_ flags to look for.
* @return bool TRUE if at least 1 of those flags is set, FALSE if not.
*/
bool is_attack_flagged_by_vnum(any_vnum vnum, bitvector_t amdf_flags) {
	attack_message_data *amd = real_attack_message(vnum);
	
	if (!amd) {
		return 0;
	}
	else {
		return ATTACK_FLAGGED(amd, amdf_flags) ? TRUE : FALSE;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// DYNAMIC ATTACK MESSAGES /////////////////////////////////////////////////

/**
* Adds messages to a set of fight messages.
*
* @param attack_message_data *add_to Which set of messages to add to.
* @param struct attack_message_set *messages The messages to put in there.
*/
void add_attack_message(attack_message_data *add_to, struct attack_message_set *messages) {
	if (add_to && messages) {
		++ATTACK_NUM_MSGS(add_to);
		LL_APPEND(ATTACK_MSG_LIST(add_to), messages);
	}
}


/**
* Creates a blank message list for use in the damage() function.
*
* @param any_vnum vnum Usually a TYPE_ or ATTACK_ const.
* @return attack_message_data* The allocated list.
*/
attack_message_data *create_attack_message(any_vnum vnum) {
	attack_message_data *amd;
	CREATE(amd, attack_message_data, 1);
	clear_attack_message(amd);
	ATTACK_VNUM(amd) = vnum;
	return amd;
}


/**
* Creates an entry for a fight message. Any strings may be NULL.
*
* @param bool duplicate_strings If TRUE, will str_dup any strings that are provided. Otherwise, uses the provided pointers.
* @param char *die_to_attacker Message shown to the attacker when fatal (may be NULL).
* @param char *die_to_victim Message shown to the victim when fatal (may be NULL).
* @param char *die_to_room Message shown to the room when fatal (may be NULL).
* @param char *miss_to_attacker Message shown to the attacker when missing (may be NULL).
* @param char *miss_to_victim Message shown to the victim when missing (may be NULL).
* @param char *miss_to_room Message shown to the room when missing (may be NULL).
* @param char *hit_to_attacker Message shown to the attacker when hit (may be NULL).
* @param char *hit_to_victim Message shown to the victim when hit (may be NULL).
* @param char *hit_to_room Message shown to the room when hit (may be NULL).
* @param char *god_to_attacker Message shown to the attacker when hitting a god (may be NULL).
* @param char *god_to_victim Message shown to the victim when hitting a god (may be NULL).
* @param char *god_to_room Message shown to the room when hitting a god (may be NULL).
*/
struct attack_message_set *create_attack_message_entry(bool duplicate_strings, char *die_to_attacker, char *die_to_victim, char *die_to_room, char *miss_to_attacker, char *miss_to_victim, char *miss_to_room, char *hit_to_attacker, char *hit_to_victim, char *hit_to_room, char *god_to_attacker, char *god_to_victim, char *god_to_room) {
	struct attack_message_set *messages;
	
	CREATE(messages, struct attack_message_set, 1);

	messages->msg[MSG_DIE].attacker_msg = (die_to_attacker && *die_to_attacker) ? (duplicate_strings ? str_dup(die_to_attacker) : die_to_attacker) : NULL;
	messages->msg[MSG_DIE].victim_msg = (die_to_victim && *die_to_victim) ? (duplicate_strings ? str_dup(die_to_victim) : die_to_victim) : NULL;
	messages->msg[MSG_DIE].room_msg = (die_to_room && *die_to_room) ? (duplicate_strings ? str_dup(die_to_room) : die_to_room) : NULL;
	messages->msg[MSG_MISS].attacker_msg = (miss_to_attacker && *miss_to_attacker) ? (duplicate_strings ? str_dup(miss_to_attacker) : miss_to_attacker) : NULL;
	messages->msg[MSG_MISS].victim_msg = (miss_to_victim && *miss_to_victim) ? (duplicate_strings ? str_dup(miss_to_victim) : miss_to_victim) : NULL;
	messages->msg[MSG_MISS].room_msg = (miss_to_room && *miss_to_room) ? (duplicate_strings ? str_dup(miss_to_room) : miss_to_room) : NULL;
	messages->msg[MSG_HIT].attacker_msg = (hit_to_attacker && *hit_to_attacker) ? (duplicate_strings ? str_dup(hit_to_attacker) : hit_to_attacker) : NULL;
	messages->msg[MSG_HIT].victim_msg = (hit_to_victim && *hit_to_victim) ? (duplicate_strings ? str_dup(hit_to_victim) : hit_to_victim) : NULL;
	messages->msg[MSG_HIT].room_msg = (hit_to_room && *hit_to_room) ? (duplicate_strings ? str_dup(hit_to_room) : hit_to_room) : NULL;
	messages->msg[MSG_GOD].attacker_msg = (god_to_attacker && *god_to_attacker) ? (duplicate_strings ? str_dup(god_to_attacker) : god_to_attacker) : NULL;
	messages->msg[MSG_GOD].victim_msg = (god_to_victim && *god_to_victim) ? (duplicate_strings ? str_dup(god_to_victim) : god_to_victim) : NULL;
	messages->msg[MSG_GOD].room_msg = (god_to_room && *god_to_room) ? (duplicate_strings ? str_dup(god_to_room) : god_to_room) : NULL;
	
	return messages;
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common attack message problems and reports them to ch.
*
* @param attack_message_data *amd The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_attack_message(attack_message_data *amd, char_data *ch) {
	bool problem = FALSE;
	attack_message_data *alt, *next_alt;
	struct attack_message_set *ams;
	bool dta, dtv, dtr, mta, mtv, mtr, gta, gtv, gtr;
	
	if (!ATTACK_NAME(amd) || !*ATTACK_NAME(amd) || !str_cmp(ATTACK_NAME(amd), default_attack_name)) {
		olc_audit_msg(ch, ATTACK_VNUM(amd), "No name set");
		problem = TRUE;
	}
	if (!ATTACK_MSG_LIST(amd)) {
		olc_audit_msg(ch, ATTACK_VNUM(amd), "No messages set");
		problem = TRUE;
	}
	
	// check my messages
	dta = dtv = dtr = mta = mtv = mtr = gta = gtv = gtr = FALSE;
	LL_FOREACH(ATTACK_MSG_LIST(amd), ams) {
		if (!ams->msg[MSG_DIE].attacker_msg && !dta) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty die2char message.");
			problem = dta = TRUE;
		}
		if (!ams->msg[MSG_DIE].victim_msg && !dtv) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty die2vict message.");
			problem = dtv = TRUE;
		}
		if (!ams->msg[MSG_DIE].room_msg && !dtr) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty die2room message.");
			problem = dtr = TRUE;
		}
		if (!ams->msg[MSG_MISS].attacker_msg && !mta) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty miss2char message.");
			problem = mta = TRUE;
		}
		if (!ams->msg[MSG_MISS].victim_msg && !mtv) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty miss2vict message.");
			problem = mtv = TRUE;
		}
		if (!ams->msg[MSG_MISS].room_msg && !mtr) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty miss2room message.");
			problem = mtr = TRUE;
		}
		if (!ams->msg[MSG_GOD].attacker_msg && !gta) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty god2char message.");
			problem = gta = TRUE;
		}
		if (!ams->msg[MSG_GOD].victim_msg && !gtv) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty god2vict message.");
			problem = gtv = TRUE;
		}
		if (!ams->msg[MSG_GOD].room_msg && !gtr) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty god2room message.");
			problem = gtr = TRUE;
		}
	}
	
	// check other message types
	HASH_ITER(hh, attack_message_table, alt, next_alt) {
		if (ATTACK_VNUM(alt) == ATTACK_VNUM(amd)) {
			continue;	// same
		}
		
		// check same name
		if (ATTACK_NAME(amd) && ATTACK_NAME(alt) && !str_cmp(ATTACK_NAME(amd), ATTACK_NAME(alt))) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Same name as [%d] %s", ATTACK_VNUM(amd), ATTACK_NAME(amd));
			problem = TRUE;
		}
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param attack_message_data *amd The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_attack_message(attack_message_data *amd, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s (%d)", ATTACK_VNUM(amd), ATTACK_NAME(amd), ATTACK_NUM_MSGS(amd));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", ATTACK_VNUM(amd), ATTACK_NAME(amd));
	}
		
	return output;
}


/**
* Searches properties of attack messages.
*
* @param char_data *ch The person searching.
* @param char *argument The argument they entered.
*/
void olc_fullsearch_attack_message(char_data *ch, char *argument) {
	bool any;
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	int count, iter;
	struct attack_message_set *ams;
	
	bitvector_t only_flags = NOBITS, not_flagged = NOBITS;
	int only_damage = NOTHING, only_weapon = NOTHING;
	int vmin = NOTHING, vmax = NOTHING;
	double speed = 0.0;
	
	attack_message_data *amd, *next_amd;
	size_t size;
	
	if (!*argument) {
		msg_to_char(ch, "See HELP ATTACKEDIT FULLSEARCH for syntax.\r\n");
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
		
		FULLSEARCH_LIST("damagetype", only_damage, damage_types)
		FULLSEARCH_FLAGS("flags", only_flags, action_bits)
		FULLSEARCH_FLAGS("flagged", only_flags, action_bits)
		FULLSEARCH_DOUBLE("speed", speed, 0.1, 10.0)
		FULLSEARCH_FLAGS("unflagged", not_flagged, action_bits)
		FULLSEARCH_INT("vmin", vmin, 0, INT_MAX)
		FULLSEARCH_INT("vmax", vmax, 0, INT_MAX)
		FULLSEARCH_LIST("weapontype", only_weapon, weapon_types)

		else {	// not sure what to do with it? treat it like a keyword
			sprintf(find_keywords + strlen(find_keywords), "%s%s", *find_keywords ? " " : "", type_arg);
		}
		
		// prepare for next loop
		skip_spaces(&argument);
	}
	
	size = snprintf(buf, sizeof(buf), "Attack message fullsearch: %s\r\n", show_color_codes(find_keywords));
	count = 0;
	
	// okay now look up messagess
	HASH_ITER(hh, attack_message_table, amd, next_amd) {
		if ((vmin != NOTHING && ATTACK_VNUM(amd) < vmin) || (vmax != NOTHING && ATTACK_VNUM(amd) > vmax)) {
			continue;	// vnum range
		}
		if (only_damage != NOTHING && (ATTACK_DAMAGE_TYPE(amd) != only_damage || !ATTACK_FLAGGED(amd, AMDF_FLAGS_REQUIRE_EXTENDED_DATA))) {
			continue;	// wrong damage
		}
		if (only_weapon != NOTHING && (ATTACK_WEAPON_TYPE(amd) != only_weapon || !ATTACK_FLAGGED(amd, AMDF_FLAGS_REQUIRE_EXTENDED_DATA))) {
			continue;	// wrong damage
		}
		if (only_flags != NOBITS && (ATTACK_FLAGS(amd) & only_flags) != only_flags) {
			continue;
		}
		if (not_flagged != NOBITS && ATTACK_FLAGGED(amd, not_flagged)) {
			continue;
		}
		if (speed > 0.0 && (speed < ATTACK_SPEED(amd, SPD_FAST) || speed > ATTACK_SPEED(amd, SPD_SLOW) || ATTACK_FLAGGED(amd, AMDF_FLAGS_REQUIRE_EXTENDED_DATA))) {
			continue;	// speed out of range
		}
		
		// search strings
		if (*find_keywords) {
			any = FALSE;
			if (multi_isname(find_keywords, ATTACK_NAME(amd))) {
				any = TRUE;
			}
			else if (multi_isname(find_keywords, ATTACK_NOUN(amd))) {
				any = TRUE;
			}
			else if (multi_isname(find_keywords, ATTACK_FIRST_PERSON(amd))) {
				any = TRUE;
			}
			else if (multi_isname(find_keywords, ATTACK_THIRD_PERSON(amd))) {
				any = TRUE;
			}
			LL_FOREACH(ATTACK_MSG_LIST(amd), ams) {
				for (iter = 0; iter < NUM_MSG_TYPES && !any; ++iter) {
					if (ams->msg[iter].attacker_msg && multi_isname(find_keywords, ams->msg[iter].attacker_msg)) {
						any = TRUE;
					}
					else if (ams->msg[iter].victim_msg && multi_isname(find_keywords, ams->msg[iter].victim_msg)) {
						any = TRUE;
					}
					else if (ams->msg[iter].room_msg && multi_isname(find_keywords, ams->msg[iter].room_msg)) {
						any = TRUE;
					}
				}
			}
			
			// did we find a match in any string
			if (!any) {
				continue;
			}
		}
		
		// show it
		snprintf(line, sizeof(line), "[%5d] %s\r\n", ATTACK_VNUM(amd), ATTACK_NAME(amd));
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
		size += snprintf(buf + size, sizeof(buf) - size, "(%d attacks)\r\n", count);
	}
	else if (count == 0) {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


/**
* Searches for all uses of an attack message and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The attack message vnum.
*/
void olc_search_attack_message(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	attack_message_data *amd = real_attack_message(vnum);
	ability_data *abil, *next_abil;
	int size, found;
	bool any;
	
	if (!amd) {
		msg_to_char(ch, "There is no attack message %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of attack message %d (%s):\r\n", vnum, ATTACK_NAME(amd));
	
	// abilities
	HASH_ITER(hh, ability_table, abil, next_abil) {
		any = FALSE;
		any |= (ABIL_ATTACK_TYPE(abil) == vnum);
		any |= find_ability_data_entry_for_misc(abil, ADL_LIMITATION, ABIL_LIMIT_WIELD_ATTACK_TYPE, vnum) ? TRUE : FALSE;
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "ABIL [%5d] %s\r\n", ABIL_VNUM(abil), ABIL_NAME(abil));
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


// Simple vnum sorter for the attack messages hash
int sort_attack_messages(attack_message_data *a, attack_message_data *b) {
	return ATTACK_VNUM(a) - ATTACK_VNUM(b);
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts an attack message into the hash table.
*
* @param attack_message_data *amd The message data to add to the table.
*/
void add_attack_message_to_table(attack_message_data *amd) {
	attack_message_data *find;
	any_vnum vnum;
	
	if (amd) {
		vnum = ATTACK_VNUM(amd);
		
		// main table
		HASH_FIND_INT(attack_message_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(attack_message_table, vnum, amd);
			HASH_SORT(attack_message_table, sort_attack_messages);
		}
	}
}


/**
* Removes an attack message from the hash table.
*
* @param attack_message_data *amd The message data to remove from the table.
*/
void remove_attack_message_from_table(attack_message_data *amd) {
	HASH_DEL(attack_message_table, amd);
}


/**
* Initializes a new attack message. This clears all memory for it, so set the
* vnum AFTER.
*
* @param attack_message_data *amd The message data to initialize.
*/
void clear_attack_message(attack_message_data *amd) {
	int iter;
	
	memset((char *) amd, 0, sizeof(attack_message_data));
	
	ATTACK_VNUM(amd) = NOTHING;
	
	for (iter = 0; iter < NUM_ATTACK_SPEEDS; ++iter) {
		ATTACK_SPEED(amd, iter) = basic_speed;
	}
}


/**
* Copies a set of attack messages.
*
* @param struct attack_message_set *input_list The set to copy.
* @return struct attack_message_set* The copy.
*/
struct attack_message_set *copy_attack_msg_list(struct attack_message_set *input_list) {
	int iter;
	struct attack_message_set *ams, *new_ams, *list;
	
	// copy set in order
	list = NULL;
	LL_FOREACH(input_list, ams) {
		CREATE(new_ams, struct attack_message_set, 1);
		*new_ams = *ams;
		
		// copy messages
		for (iter = 0; iter < NUM_MSG_TYPES; ++iter) {
			new_ams->msg[iter].attacker_msg = ams->msg[iter].attacker_msg ? strdup(ams->msg[iter].attacker_msg) : NULL;
			new_ams->msg[iter].victim_msg = ams->msg[iter].victim_msg ? strdup(ams->msg[iter].victim_msg) : NULL;
			new_ams->msg[iter].room_msg = ams->msg[iter].room_msg ? strdup(ams->msg[iter].room_msg) : NULL;
		}
		
		new_ams->next = NULL;
		LL_APPEND(list, new_ams);
	}
	
	return list;
}


/**
* frees up memory for an attack message data item.
*
* See also: olc_delete_attack_message
*
* @param attack_message_data *amd The message data to free.
*/
void free_attack_message(attack_message_data *amd) {
	attack_message_data *proto = real_attack_message(ATTACK_VNUM(amd));
	struct attack_message_set *msg, *next_msg;
	
	if (ATTACK_NAME(amd) && (!proto || ATTACK_NAME(amd) != ATTACK_NAME(proto))) {
		free(ATTACK_NAME(amd));
	}
	if (ATTACK_FIRST_PERSON(amd) && (!proto || ATTACK_FIRST_PERSON(amd) != ATTACK_FIRST_PERSON(proto))) {
		free(ATTACK_FIRST_PERSON(amd));
	}
	if (ATTACK_THIRD_PERSON(amd) && (!proto || ATTACK_THIRD_PERSON(amd) != ATTACK_THIRD_PERSON(proto))) {
		free(ATTACK_THIRD_PERSON(amd));
	}
	if (ATTACK_NOUN(amd) && (!proto || ATTACK_NOUN(amd) != ATTACK_NOUN(proto))) {
		free(ATTACK_NOUN(amd));
	}
	
	if (ATTACK_MSG_LIST(amd) && (!proto || ATTACK_MSG_LIST(amd) != ATTACK_MSG_LIST(proto))) {
		LL_FOREACH_SAFE(ATTACK_MSG_LIST(amd), msg, next_msg) {
			free_attack_message_set(msg);
		}
	}
	
	free(amd);
}


/**
* Frees a single fight message (message_type) and its strings.
*
* @param struct attack_message_set *type The fight message to free.
*/
void free_attack_message_set(struct attack_message_set *ams) {
	int iter;
	if (ams) {
		for (iter = 0; iter < NUM_MSG_TYPES; ++iter) {
			if (ams->msg[iter].attacker_msg) {
				free(ams->msg[iter].attacker_msg);
			}
			if (ams->msg[iter].victim_msg) {
				free(ams->msg[iter].victim_msg);
			}
			if (ams->msg[iter].room_msg) {
				free(ams->msg[iter].room_msg);
			}
		}
		free(ams);
	}
}


/**
* Finds or creates an entry in the attack_message_table hash.
*
* @param any_vnum a_type The vnum or ATTACK_ const to find the fight message list for.
* @param bool create_if_missing If TRUE, will create the entry for the a_type.
* @return attack_message_data* The fight message list, if any (guaranteed if create_if_missing=TRUE).
*/
attack_message_data *find_attack_message(any_vnum a_type, bool create_if_missing) {
	attack_message_data *fmes;
	
	HASH_FIND_INT(attack_message_table, &a_type, fmes);
	if (!fmes && create_if_missing) {
		fmes = create_attack_message(a_type);
		HASH_ADD_INT(attack_message_table, vnum, fmes);
	}
	
	return fmes;	// if any
}


/**
* @param char *name The name or vnum to search.
* @param bool exact Can only abbreviate if FALSE.
* @return attack_message_data* The message data, or NULL if it doesn't exist.
*/
attack_message_data *find_attack_message_by_name_or_vnum(char *name, bool exact) {
	attack_message_data *amd, *next_amd, *abbrev = FALSE;
	
	if (isdigit(*name) && (amd = real_attack_message(atoi(name)))) {
		return amd;	// vnum shortcut
	}
	
	// otherwise look up by name
	HASH_ITER(hh, attack_message_table, amd, next_amd) {
		if (!str_cmp(name, ATTACK_NAME(amd))) {
			return amd;	// exact match
		}
		else if (!exact && !abbrev && is_abbrev(name, ATTACK_NAME(amd))) {
			abbrev = amd;	// partial match
		}
	}
	
	return abbrev;	// if any
}


/**
* Reads in one "action line" for a damage message. This also checks for:
*   # - send no message for this line
*
* @param FILE *fl The open fight messages file.
* @param int type The attack type for this record (for error reporting).
* @param int type_pos Which message for that attack type (for error reporting).
* @return char* The string as read from the file, or NULL for no message.
*/
char *fread_action(FILE *fl, int type, int type_pos) {
	char buf[MAX_STRING_LENGTH], *rslt;
	
	fgets(buf, MAX_STRING_LENGTH, fl);
	if (feof(fl)) {
		log("SYSERR: fread_action - unexpected EOF in fight message type %d, message %d", type, type_pos);
		exit(1);
	}
	if (*buf == '#') {
		return (NULL);
	}
	else {
		*(buf + strlen(buf) - 1) = '\0';
		CREATE(rslt, char, strlen(buf) + 1);
		strcpy(rslt, buf);
		return (rslt);
	}
}


/**
* Loads the "messages" file, which contains damage messages for various attack
* types. As of b5.166, this is a dynamic file written by OLC, and supports more
* than 1 format.
*/
void load_fight_messages(void) {
	any_vnum type;
	bool extended = FALSE;
	char chk[READ_SIZE+1], error[256], str_in[256], line[256];
	char char_in;
	double dbl_in[3];
	int iter, int_in[2];
	int version;	// new versions must be greater than old ones
	struct attack_message_set *messages;
	attack_message_data *amd, *next_amd;
	FILE *fl;

	if (!(fl = fopen(ATTACK_MESSAGES_FILE, "r"))) {
		log("SYSERR: Error reading combat message file %s: %s", ATTACK_MESSAGES_FILE, strerror(errno));
		exit(1);
	}
	
	// free existing messages if any (for reload)
	HASH_ITER(hh, attack_message_table, amd, next_amd) {
		HASH_DEL(attack_message_table, amd);
		free_attack_message(amd);
	}

	fgets(chk, 128, fl);
	while (!feof(fl) && (*chk == '\n' || *chk == '*')) {
		fgets(chk, 128, fl);
	}
	
	// load all records
	while (*chk == 'M') {
		// determine type/format
		if (sscanf(chk, "M%d %s %c", &type, str_in, &char_in) == 3 && char_in == '+') {
			// b5.166 format: type was in M line, extended format
			version = 5166;
			extended = TRUE;
		}
		else if (sscanf(chk, "M%d %s ", &type, str_in) == 2) {
			// b5.166 format: type was in M line; simple format
			version = 5166;
			extended = FALSE;
		}
		else {
			// older format: read type from next line
			fgets(chk, READ_SIZE, fl);
			sscanf(chk, " %d\n", &type);
			
			version = 30;	// circle 3.0
			extended = FALSE;
			strcpy(str_in, "0");
		}
		
		// find or create message entry
		amd = find_attack_message(type, TRUE);
		snprintf(error, sizeof(error), "attack message %d:%d", type, ATTACK_NUM_MSGS(amd));
		
		// header info?
		ATTACK_FLAGS(amd) = asciiflag_conv(str_in);
		
		if (version >= 5166) {
			// read: name
			if (ATTACK_NAME(amd)) {
				// free first: more than 1 entry can have the name
				free(ATTACK_NAME(amd));
			}
			ATTACK_NAME(amd) = fread_string(fl, error);
			
			// had the + indicator
			if (extended) {
				// read: first-person
				if (ATTACK_FIRST_PERSON(amd)) {
					free(ATTACK_FIRST_PERSON(amd));
				}
				ATTACK_FIRST_PERSON(amd) = fread_string(fl, error);
				
				// read: third-person
				if (ATTACK_THIRD_PERSON(amd)) {
					free(ATTACK_THIRD_PERSON(amd));
				}
				ATTACK_THIRD_PERSON(amd) = fread_string(fl, error);
				
				// read: noun
				if (ATTACK_NOUN(amd)) {
					free(ATTACK_NOUN(amd));
				}
				ATTACK_NOUN(amd) = fread_string(fl, error);
				
				// damagetype weapontype slow normalspeed fast
				if (!get_line(fl, line)) {
					log("SYSERR: Missing numeric line of %s", error);
					exit(1);
				}
				if (sscanf(line, "%d %d %lf %lf %lf", &int_in[0], &int_in[1], &dbl_in[SPD_FAST], &dbl_in[SPD_NORMAL], &dbl_in[SPD_SLOW]) != 5) {
					log("SYSERR: Unexpected data in numeric line of %s", error);
					exit(1);
				}
				
				ATTACK_DAMAGE_TYPE(amd) = int_in[0];
				ATTACK_WEAPON_TYPE(amd) = int_in[1];
				for (iter = 0; iter < NUM_ATTACK_SPEEDS; ++iter) {
					ATTACK_SPEED(amd, iter) = dbl_in[iter];
				}
			}
		}
		
		// read 1 set: does not use create_attack_message_entry():
		CREATE(messages, struct attack_message_set, 1);
		messages->msg[MSG_DIE].attacker_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		messages->msg[MSG_DIE].victim_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		messages->msg[MSG_DIE].room_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		messages->msg[MSG_MISS].attacker_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		messages->msg[MSG_MISS].victim_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		messages->msg[MSG_MISS].room_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		messages->msg[MSG_HIT].attacker_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		messages->msg[MSG_HIT].victim_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		messages->msg[MSG_HIT].room_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		messages->msg[MSG_GOD].attacker_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		messages->msg[MSG_GOD].victim_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		messages->msg[MSG_GOD].room_msg = fread_action(fl, type, ATTACK_NUM_MSGS(amd));
		
		add_attack_message(amd, messages);
		
		// skip to next real line
		fgets(chk, READ_SIZE, fl);
		while (!feof(fl) && (*chk == '\n' || *chk == '*')) {
			fgets(chk, READ_SIZE, fl);
		}
	}

	fclose(fl);
}


/**
* @param any_vnum vnum Any attack message vnum or ATTACK_ const
* @return attack_message_data* The attack message, or NULL if it doesn't exist
*/
attack_message_data *real_attack_message(any_vnum vnum) {
	attack_message_data *amd;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(attack_message_table, &vnum, amd);
	return amd;
}


/**
* Writes the 'messages' file for attack messages.
*/
void save_attack_message_file(void) {
	attack_message_data *amd, *next_amd;
	FILE *fl;
	
	if (!(fl = fopen(ATTACK_MESSAGES_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to open attack messages file '%s' for writing", ATTACK_MESSAGES_FILE TEMP_SUFFIX);
		return;
	}
	
	// file header
	fprintf(fl, "%s", messages_file_header);
	
	HASH_ITER(hh, attack_message_table, amd, next_amd) {
		write_attack_message_to_file(fl, amd);
	}
	
	// footer
	fprintf(fl, "\n$\n");
	
	// and overwrite
	fclose(fl);
	rename(ATTACK_MESSAGES_FILE TEMP_SUFFIX, ATTACK_MESSAGES_FILE);
}


/**
* Outputs one attack message in its own special format.
*
* @param FILE *fl The file to write it to.
* @param attack_message_data *amd The thing to save.
*/
void write_attack_message_to_file(FILE *fl, attack_message_data *amd) {
	int iter;
	struct attack_message_set *ams;
	bool wrote_extended = FALSE;
	
	const int msg_order[] = { MSG_DIE, MSG_MISS, MSG_HIT, MSG_GOD, -1 };	// requires -1 terminator at end
	#define WAMTF_MSG(pos, mtype)  ((ams->msg[msg_order[(pos)]].mtype && *(ams->msg[msg_order[(pos)]].mtype)) ? ams->msg[msg_order[(pos)]].mtype : "#")
	
	if (!fl || !amd) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_attack_message_to_file called without %s", !fl ? "file" : "attack message");
		return;
	}
	
	// sets of messages
	LL_FOREACH(ATTACK_MSG_LIST(amd), ams) {
		fprintf(fl, "\n");	// records are spaced by blank lines
		
		// record begins M#### flags +
		fprintf(fl, "M%d %s %s\n", ATTACK_VNUM(amd), bitv_to_alpha(ATTACK_FLAGS(amd)), (ATTACK_HAS_EXTENDED_DATA(amd) && !wrote_extended) ? "+" : "");	// M# indicates the b5.166 attack message format
		fprintf(fl, "%s~\n", NULLSAFE(ATTACK_NAME(amd)));
		
		if (ATTACK_HAS_EXTENDED_DATA(amd) && !wrote_extended) {
			fprintf(fl, "%s~\n", NULLSAFE(ATTACK_FIRST_PERSON(amd)));
			fprintf(fl, "%s~\n", NULLSAFE(ATTACK_THIRD_PERSON(amd)));
			fprintf(fl, "%s~\n", NULLSAFE(ATTACK_NOUN(amd)));
			fprintf(fl, "%d %d %.1f %.1f %.1f\n", ATTACK_DAMAGE_TYPE(amd), ATTACK_WEAPON_TYPE(amd), ATTACK_SPEED(amd, SPD_FAST), ATTACK_SPEED(amd, SPD_NORMAL), ATTACK_SPEED(amd, SPD_SLOW));
			
			// only need this section once
			wrote_extended = TRUE;
		}
		
		// print message triplets in order, with '#' in place of blanks.
		for (iter = 0; msg_order[iter] != -1; ++iter) {
			fprintf(fl, "%s\n", WAMTF_MSG(iter, attacker_msg));
			fprintf(fl, "%s\n", WAMTF_MSG(iter, victim_msg));
			fprintf(fl, "%s\n", WAMTF_MSG(iter, room_msg));
		}
		
		// messages have no record-end marker, just the end of the strings
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////

/**
* Creates a new attack_message_data entry.
* 
* @param any_vnum vnum The number to create.
* @return attack_message_data* The new message's prototype.
*/
attack_message_data *create_attack_message_table_entry(any_vnum vnum) {
	attack_message_data *amd;
	
	// sanity
	if (real_attack_message(vnum)) {
		log("SYSERR: Attempting to insert attack message at existing vnum %d", vnum);
		return real_attack_message(vnum);
	}
	
	CREATE(amd, attack_message_data, 1);
	clear_attack_message(amd);
	ATTACK_VNUM(amd) = vnum;
	ATTACK_NAME(amd) = str_dup(default_attack_name);
	add_attack_message_to_table(amd);

	// unlike most types, we don't save at this point
	return amd;
}


/**
* WARNING: This function actually deletes an attack message.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_attack_message(char_data *ch, any_vnum vnum) {
	ability_data *abil, *next_abil;
	descriptor_data *desc;
	attack_message_data *amd;
	bool found;
	char name[256];
	
	if (!(amd = real_attack_message(vnum))) {
		msg_to_char(ch, "There is no such attack message %d.\r\n", vnum);
		return;
	}
	
	snprintf(name, sizeof(name), "%s", NULLSAFE(ATTACK_NAME(amd)));
	
	// remove it from the hash table first
	remove_attack_message_from_table(amd);

	// save the file without it right away
	save_attack_message_file();
	
	// now remove from prototypes
	
	// update abilities
	HASH_ITER(hh, ability_table, abil, next_abil) {
		found = FALSE;
		if (ABIL_ATTACK_TYPE(abil) == vnum) {
			ABIL_ATTACK_TYPE(abil) = 0;
			found = TRUE;
		}
		found |= delete_misc_from_ability_data_list(abil, ADL_LIMITATION, ABIL_LIMIT_WIELD_ATTACK_TYPE, vnum);
		
		if (found) {
			save_library_file_for_vnum(DB_BOOT_ABIL, ABIL_VNUM(abil));
		}
	}
	
	// olc editor updates
	LL_FOREACH(descriptor_list, desc) {
		if (GET_OLC_ABILITY(desc)) {
			found = FALSE;
			if (ABIL_ATTACK_TYPE(GET_OLC_ABILITY(desc)) == vnum) {
				ABIL_ATTACK_TYPE(GET_OLC_ABILITY(desc)) = 0;
				found = TRUE;
			}
			found |= delete_misc_from_ability_data_list(GET_OLC_ABILITY(desc), ADL_LIMITATION, ABIL_LIMIT_WIELD_ATTACK_TYPE, vnum);
			
			if (found) {
				msg_to_char(desc->character, "An attack type used by the ability you're editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted attack message %d %s", GET_NAME(ch), vnum, name);
	msg_to_char(ch, "Attack message %d (%s) deleted.\r\n", vnum, name);
	
	free_attack_message(amd);
}


/**
* Function to save a player's changes to an attack message (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_attack_message(descriptor_data *desc) {	
	attack_message_data *proto, *amd = GET_OLC_ATTACK(desc);
	struct attack_message_set *ams, *next_ams;
	any_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh;

	// have a place to save it?
	if (!(proto = real_attack_message(vnum))) {
		proto = create_attack_message_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (ATTACK_NAME(proto)) {
		free(ATTACK_NAME(proto));
	}
	if (ATTACK_FIRST_PERSON(proto)) {
		free(ATTACK_FIRST_PERSON(proto));
	}
	if (ATTACK_THIRD_PERSON(proto)) {
		free(ATTACK_THIRD_PERSON(proto));
	}
	if (ATTACK_NOUN(proto)) {
		free(ATTACK_NOUN(proto));
	}
	LL_FOREACH_SAFE(ATTACK_MSG_LIST(proto), ams, next_ams) {
		free_attack_message_set(ams);
	}
	
	// sanity: name required
	if (!ATTACK_NAME(amd) || !*ATTACK_NAME(amd)) {
		if (ATTACK_NAME(amd)) {
			free(ATTACK_NAME(amd));
		}
		ATTACK_NAME(amd) = str_dup(default_attack_name);
	}
	
	// these are optional
	if (ATTACK_FIRST_PERSON(amd) && !*ATTACK_FIRST_PERSON(amd)) {
		free(ATTACK_FIRST_PERSON(amd));
		ATTACK_FIRST_PERSON(amd) = NULL;
	}
	if (ATTACK_THIRD_PERSON(amd) && !*ATTACK_THIRD_PERSON(amd)) {
		free(ATTACK_THIRD_PERSON(amd));
		ATTACK_THIRD_PERSON(amd) = NULL;
	}
	if (ATTACK_NOUN(amd) && !*ATTACK_NOUN(amd)) {
		free(ATTACK_NOUN(amd));
		ATTACK_NOUN(amd) = NULL;
	}
	
	// if it can be used on weapons and mobs, they are required
	if (ATTACK_FLAGGED(amd, AMDF_FLAGS_REQUIRE_EXTENDED_DATA)) {
		if (!ATTACK_FIRST_PERSON(amd)) {
			ATTACK_FIRST_PERSON(amd) = strdup(default_attack_first_person);
		}
		if (!ATTACK_THIRD_PERSON(amd)) {
			ATTACK_THIRD_PERSON(amd) = strdup(default_attack_third_person);
		}
		if (!ATTACK_NOUN(amd)) {
			ATTACK_NOUN(amd) = strdup(default_attack_noun);
		}
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handles
	*proto = *amd;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handles
	
	// and save to file
	save_attack_message_file();
}


/**
* Creates a copy of an attack message, or clears a new one, for editing.
* 
* @param attack_message_data *input The attack message to copy, or NULL to make a new one.
* @return attack_message_data* The copied attack message.
*/
attack_message_data *setup_olc_attack_message(attack_message_data *input) {
	attack_message_data *dupe;
	
	CREATE(dupe, attack_message_data, 1);
	clear_attack_message(dupe);
	
	if (input) {
		// copy normal data
		*dupe = *input;
		
		// copy things that are pointers
		ATTACK_NAME(dupe) = ATTACK_NAME(input) ? str_dup(ATTACK_NAME(input)) : NULL;
		ATTACK_FIRST_PERSON(dupe) = ATTACK_FIRST_PERSON(input) ? str_dup(ATTACK_FIRST_PERSON(input)) : NULL;
		ATTACK_THIRD_PERSON(dupe) = ATTACK_THIRD_PERSON(input) ? str_dup(ATTACK_THIRD_PERSON(input)) : NULL;
		ATTACK_NOUN(dupe) = ATTACK_NOUN(input) ? str_dup(ATTACK_NOUN(input)) : NULL;
		
		ATTACK_MSG_LIST(dupe) = copy_attack_msg_list(ATTACK_MSG_LIST(input));
	}
	else {
		// brand new: some defaults
		ATTACK_NAME(dupe) = str_dup(default_attack_name);
	}
	
	// done
	return dupe;
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* For vstat.
*
* @param char_data *ch The player requesting stats.
* @param attack_message_data *amd The attack message to display.
*/
void do_stat_attack_message(char_data *ch, attack_message_data *amd) {
	char buf[MAX_STRING_LENGTH * 2], lbuf[MAX_STRING_LENGTH];;
	char *to_show;
	int count, iter;
	size_t size;
	struct attack_message_set *ams;
	
	if (!amd) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \ty%s\t0, Message count: [\tc%d\t0]\r\n", ATTACK_VNUM(amd), ATTACK_NAME(amd), ATTACK_NUM_MSGS(amd));

	sprintbit(ATTACK_FLAGS(amd), attack_message_flags, lbuf, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", lbuf);
	
	if (ATTACK_HAS_EXTENDED_DATA(amd)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Strings: [\ty%s\t0, \ty%s\t0, \ty%s\t0]\r\n", NULLSAFE(ATTACK_FIRST_PERSON(amd)), NULLSAFE(ATTACK_THIRD_PERSON(amd)), NULLSAFE(ATTACK_NOUN(amd)));
		
		// Damage, Weapon, Speeds (all same line)
		size += snprintf(buf + size, sizeof(buf) - size, "Damage type: [\tg%s\t0], Weapon type: [\tg%s\t0], Speeds: [", damage_types[ATTACK_DAMAGE_TYPE(amd)], weapon_types[ATTACK_WEAPON_TYPE(amd)]);
		for (iter = 0; iter < NUM_ATTACK_SPEEDS; ++iter) {
			size += snprintf(buf + size, sizeof(buf) - size, "%s\tc%.1f\t0", iter > 0 ? " | " : "", ATTACK_SPEED(amd, iter));
		}
		size += snprintf(buf + size, sizeof(buf) - size, "]\r\n");
	}

	size += snprintf(buf + size, sizeof(buf) - size, "Messages:\r\n");
	
	// message section (abbreviated)
	count = 0;
	LL_FOREACH(ATTACK_MSG_LIST(amd), ams) {
		if (ams->msg[MSG_HIT].attacker_msg) {
			to_show = ams->msg[MSG_HIT].attacker_msg;
		}
		else if (ams->msg[MSG_DIE].attacker_msg) {
			to_show = ams->msg[MSG_DIE].attacker_msg;
		}
		else if (ams->msg[MSG_MISS].attacker_msg) {
			to_show = ams->msg[MSG_MISS].attacker_msg;
		}
		else if (ams->msg[MSG_GOD].attacker_msg) {
			to_show = ams->msg[MSG_GOD].attacker_msg;
		}
		else if (ams->msg[MSG_HIT].room_msg) {
			to_show = ams->msg[MSG_HIT].room_msg;
		}
		else if (ams->msg[MSG_DIE].room_msg) {
			to_show = ams->msg[MSG_DIE].room_msg;
		}
		else if (ams->msg[MSG_MISS].room_msg) {
			to_show = ams->msg[MSG_MISS].room_msg;
		}
		else if (ams->msg[MSG_GOD].room_msg) {
			to_show = ams->msg[MSG_GOD].room_msg;
		}
		else {
			to_show = NULL;	// maybe empty
		}
		
		// only show 1 line per message on stat
		size += snprintf(buf + size, sizeof(buf) - size, "%d. %s\r\n", ++count, to_show ? to_show : "(blank)");
	}
	if (!count) {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Sub-menu when editing a specific messsage.
*
* @param char_data *ch The person who is editing a message and will see its display.
*/
void olc_show_one_message(char_data *ch) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	char buf[MAX_STRING_LENGTH * 2];
	int iter;
	struct attack_message_set *ams;
	
	// find message
	if (!(ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc)))) {
		// return to main menu
		GET_OLC_ATTACK_NUM(ch->desc) = 0;
		olc_show_attack_message(ch);
		return;
	}
	
	// one message view
	*buf = '\0';
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0: Message %s%d\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, ATTACK_NAME(amd), OLC_LABEL_UNCHANGED, GET_OLC_ATTACK_NUM(ch->desc));
	
	for (iter = 1; iter < 80; ++iter) {
		strcat(buf, "-");
	}
	strcat(buf, "\r\n");
	
	// messages
	sprintf(buf + strlen(buf), "<%sdie2char\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_DIE].attacker_msg), ams->msg[MSG_DIE].attacker_msg ? ams->msg[MSG_DIE].attacker_msg : "(none)");
	sprintf(buf + strlen(buf), "<%sdie2vict\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_DIE].victim_msg), ams->msg[MSG_DIE].victim_msg ? ams->msg[MSG_DIE].victim_msg : "(none)");
	sprintf(buf + strlen(buf), "<%sdie2room\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_DIE].room_msg), ams->msg[MSG_DIE].room_msg ? ams->msg[MSG_DIE].room_msg : "(none)");
	
	for (iter = 1; iter < 80; ++iter) {
		strcat(buf, "-");
	}
	strcat(buf, "\r\n");
	
	sprintf(buf + strlen(buf), "<%smiss2char\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_MISS].attacker_msg), ams->msg[MSG_MISS].attacker_msg ? ams->msg[MSG_MISS].attacker_msg : "(none)");
	sprintf(buf + strlen(buf), "<%smiss2vict\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_MISS].victim_msg), ams->msg[MSG_MISS].victim_msg ? ams->msg[MSG_MISS].victim_msg : "(none)");
	sprintf(buf + strlen(buf), "<%smiss2room\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_MISS].room_msg), ams->msg[MSG_MISS].room_msg ? ams->msg[MSG_MISS].room_msg : "(none)");
	
	for (iter = 1; iter < 80; ++iter) {
		strcat(buf, "-");
	}
	strcat(buf, "\r\n");
	
	sprintf(buf + strlen(buf), "<%shit2char\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_HIT].attacker_msg), ams->msg[MSG_HIT].attacker_msg ? ams->msg[MSG_HIT].attacker_msg : "(none)");
	sprintf(buf + strlen(buf), "<%shit2vict\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_HIT].victim_msg), ams->msg[MSG_HIT].victim_msg ? ams->msg[MSG_HIT].victim_msg : "(none)");
	sprintf(buf + strlen(buf), "<%shit2room\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_HIT].room_msg), ams->msg[MSG_HIT].room_msg ? ams->msg[MSG_HIT].room_msg : "(none)");
	
	for (iter = 1; iter < 80; ++iter) {
		strcat(buf, "-");
	}
	strcat(buf, "\r\n");
	
	sprintf(buf + strlen(buf), "<%sgod2char\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_GOD].attacker_msg), ams->msg[MSG_GOD].attacker_msg ? ams->msg[MSG_GOD].attacker_msg : "(none)");
	sprintf(buf + strlen(buf), "<%sgod2vict\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_GOD].victim_msg), ams->msg[MSG_GOD].victim_msg ? ams->msg[MSG_GOD].victim_msg : "(none)");
	sprintf(buf + strlen(buf), "<%sgod2room\t0> %s\r\n", OLC_LABEL_PTR(ams->msg[MSG_GOD].room_msg), ams->msg[MSG_GOD].room_msg ? ams->msg[MSG_GOD].room_msg : "(none)");
	
	for (iter = 1; iter < 80; ++iter) {
		strcat(buf, "-");
	}
	strcat(buf, "\r\n");
	
	sprintf(buf + strlen(buf), "Return to main menu: <%sback\t0>\r\n", OLC_LABEL_UNCHANGED);
	page_string(ch->desc, buf, TRUE);
}


/**
* This is the main recipe display for attack OLC. It displays the user's
* currently-edited attack.
*
* @param char_data *ch The person who is editing an attack and will see its display.
*/
void olc_show_attack_message(char_data *ch) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	char buf[MAX_STRING_LENGTH * 2], lbuf[MAX_STRING_LENGTH];
	char *to_show;
	int count, iter;
	struct attack_message_set *ams;
	
	if (!amd) {
		return;
	}
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		// show message instead
		olc_show_one_message(ch);
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !real_attack_message(ATTACK_VNUM(amd)) ? "new attack message" : ATTACK_NAME(real_attack_message(ATTACK_VNUM(amd))));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(ATTACK_NAME(amd), default_attack_name), NULLSAFE(ATTACK_NAME(amd)));
	
	sprintbit(ATTACK_FLAGS(amd), attack_message_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(ATTACK_FLAGS(amd), NOBITS), lbuf);
	
	if (ATTACK_HAS_EXTENDED_DATA(amd)) {
		sprintf(buf + strlen(buf), "<%sfirstperson\t0> %s\r\n", OLC_LABEL_STR(ATTACK_FIRST_PERSON(amd), default_attack_first_person), ATTACK_FIRST_PERSON(amd) ? ATTACK_FIRST_PERSON(amd) : "(none)");
		sprintf(buf + strlen(buf), "<%sthirdperson\t0> %s\r\n", OLC_LABEL_STR(ATTACK_THIRD_PERSON(amd), default_attack_third_person), ATTACK_THIRD_PERSON(amd) ? ATTACK_THIRD_PERSON(amd) : "(none)");
		sprintf(buf + strlen(buf), "<%snoun\t0> %s\r\n", OLC_LABEL_STR(ATTACK_NOUN(amd), default_attack_noun), ATTACK_NOUN(amd) ? ATTACK_NOUN(amd) : "(none)");
		
		sprintf(buf + strlen(buf), "<%sdamagetype\t0> %s\r\n", OLC_LABEL_VAL(ATTACK_DAMAGE_TYPE(amd), 0), damage_types[ATTACK_DAMAGE_TYPE(amd)]);
		sprintf(buf + strlen(buf), "<%sweapontype\t0> %s\r\n", OLC_LABEL_VAL(ATTACK_WEAPON_TYPE(amd), 0), weapon_types[ATTACK_WEAPON_TYPE(amd)]);
		
		sprintf(buf + strlen(buf), "Speeds: <%sspeed\t0>\r\n", OLC_LABEL_CHANGED);
		for (iter = 0; iter < NUM_ATTACK_SPEEDS; ++iter) {
			sprintf(buf + strlen(buf), "   %s: %s%.1f\t0 seconds\r\n", attack_speed_types[iter], OLC_LABEL_VAL(ATTACK_SPEED(amd, iter), basic_speed), ATTACK_SPEED(amd, iter));
		}
	}
	
	sprintf(buf + strlen(buf), "Messages: <%smessage\t0>\r\n", OLC_LABEL_PTR(ATTACK_MSG_LIST(amd)));
	
	// show list preview
	count = 0;
	LL_FOREACH(ATTACK_MSG_LIST(amd), ams) {
		if (ams->msg[MSG_HIT].attacker_msg) {
			to_show = ams->msg[MSG_HIT].attacker_msg;
		}
		else if (ams->msg[MSG_DIE].attacker_msg) {
			to_show = ams->msg[MSG_DIE].attacker_msg;
		}
		else if (ams->msg[MSG_MISS].attacker_msg) {
			to_show = ams->msg[MSG_MISS].attacker_msg;
		}
		else if (ams->msg[MSG_GOD].attacker_msg) {
			to_show = ams->msg[MSG_GOD].attacker_msg;
		}
		else if (ams->msg[MSG_HIT].room_msg) {
			to_show = ams->msg[MSG_HIT].room_msg;
		}
		else if (ams->msg[MSG_DIE].room_msg) {
			to_show = ams->msg[MSG_DIE].room_msg;
		}
		else if (ams->msg[MSG_MISS].room_msg) {
			to_show = ams->msg[MSG_MISS].room_msg;
		}
		else if (ams->msg[MSG_GOD].room_msg) {
			to_show = ams->msg[MSG_GOD].room_msg;
		}
		else {
			to_show = NULL;	// maybe empty
		}
		
		// only show 1 line per message on stat
		sprintf(buf + strlen(buf), "%d. %s\r\n", ++count, to_show ? to_show : "(blank)");
	}
	
	if (ATTACK_HAS_EXTENDED_DATA(amd) && !ATTACK_FLAGGED(amd, AMDF_FLAGS_REQUIRE_EXTENDED_DATA)) {
		sprintf(buf + strlen(buf), "To clear extended data: <%sclearextended\t0>\r\n", OLC_LABEL_CHANGED);
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the attack message db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_attack_message(char *searchname, char_data *ch) {
	attack_message_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, attack_message_table, iter, next_iter) {
		if (multi_isname(searchname, ATTACK_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, ATTACK_VNUM(iter), ATTACK_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MESSAGE MODULES /////////////////////////////////////////////////////

OLC_MODULE(attackedit_die2char) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_DIE].attacker_msg) {
			free(ams->msg[MSG_DIE].attacker_msg);
		}
		ams->msg[MSG_DIE].attacker_msg = NULL;
		msg_to_char(ch, "You remove the die2char message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "die2char", &ams->msg[MSG_DIE].attacker_msg);
	}
}


OLC_MODULE(attackedit_die2vict) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_DIE].victim_msg) {
			free(ams->msg[MSG_DIE].victim_msg);
		}
		ams->msg[MSG_DIE].victim_msg = NULL;
		msg_to_char(ch, "You remove the die2vict message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "die2vict", &ams->msg[MSG_DIE].victim_msg);
	}
}


OLC_MODULE(attackedit_die2room) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_DIE].room_msg) {
			free(ams->msg[MSG_DIE].room_msg);
		}
		ams->msg[MSG_DIE].room_msg = NULL;
		msg_to_char(ch, "You remove the die2room message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "die2room", &ams->msg[MSG_DIE].room_msg);
	}
}


OLC_MODULE(attackedit_miss2char) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_MISS].attacker_msg) {
			free(ams->msg[MSG_MISS].attacker_msg);
		}
		ams->msg[MSG_MISS].attacker_msg = NULL;
		msg_to_char(ch, "You remove the miss2char message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "miss2char", &ams->msg[MSG_MISS].attacker_msg);
	}
}


OLC_MODULE(attackedit_miss2vict) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_MISS].victim_msg) {
			free(ams->msg[MSG_MISS].victim_msg);
		}
		ams->msg[MSG_MISS].victim_msg = NULL;
		msg_to_char(ch, "You remove the miss2vict message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "miss2vict", &ams->msg[MSG_MISS].victim_msg);
	}
}


OLC_MODULE(attackedit_miss2room) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_MISS].room_msg) {
			free(ams->msg[MSG_MISS].room_msg);
		}
		ams->msg[MSG_MISS].room_msg = NULL;
		msg_to_char(ch, "You remove the miss2room message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "miss2room", &ams->msg[MSG_MISS].room_msg);
	}
}


OLC_MODULE(attackedit_hit2char) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_HIT].attacker_msg) {
			free(ams->msg[MSG_HIT].attacker_msg);
		}
		ams->msg[MSG_HIT].attacker_msg = NULL;
		msg_to_char(ch, "You remove the hit2char message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "hit2char", &ams->msg[MSG_HIT].attacker_msg);
	}
}


OLC_MODULE(attackedit_hit2vict) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_HIT].victim_msg) {
			free(ams->msg[MSG_HIT].victim_msg);
		}
		ams->msg[MSG_HIT].victim_msg = NULL;
		msg_to_char(ch, "You remove the hit2vict message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "hit2vict", &ams->msg[MSG_HIT].victim_msg);
	}
}


OLC_MODULE(attackedit_hit2room) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_HIT].room_msg) {
			free(ams->msg[MSG_HIT].room_msg);
		}
		ams->msg[MSG_HIT].room_msg = NULL;
		msg_to_char(ch, "You remove the hit2room message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "hit2room", &ams->msg[MSG_HIT].room_msg);
	}
}


OLC_MODULE(attackedit_god2char) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_GOD].attacker_msg) {
			free(ams->msg[MSG_GOD].attacker_msg);
		}
		ams->msg[MSG_GOD].attacker_msg = NULL;
		msg_to_char(ch, "You remove the god2char message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "god2char", &ams->msg[MSG_GOD].attacker_msg);
	}
}


OLC_MODULE(attackedit_god2vict) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_GOD].victim_msg) {
			free(ams->msg[MSG_GOD].victim_msg);
		}
		ams->msg[MSG_GOD].victim_msg = NULL;
		msg_to_char(ch, "You remove the god2vict message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "god2vict", &ams->msg[MSG_GOD].victim_msg);
	}
}


OLC_MODULE(attackedit_god2room) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ams->msg[MSG_GOD].room_msg) {
			free(ams->msg[MSG_GOD].room_msg);
		}
		ams->msg[MSG_GOD].room_msg = NULL;
		msg_to_char(ch, "You remove the god2room message.\r\n");
	}
	else {
		olc_process_string(ch, argument, "god2room", &ams->msg[MSG_GOD].room_msg);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(attackedit_back) {
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0) {
		msg_to_char(ch, "You are already at the main attack message menu.\r\n");
	}
	else {
		GET_OLC_ATTACK_NUM(ch->desc) = 0;
		olc_show_attack_message(ch);
	}
}


OLC_MODULE(attackedit_clearextended) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You can't do that while editing a message (use .back to return to the menu).\r\n");
		return;
	}
	
	if (ATTACK_HAS_EXTENDED_DATA(amd) && !ATTACK_FLAGGED(amd, AMDF_FLAGS_REQUIRE_EXTENDED_DATA)) {
		ATTACK_DAMAGE_TYPE(amd) = 0;
		ATTACK_WEAPON_TYPE(amd) = 0;
		
		if (ATTACK_FIRST_PERSON(amd)) {
			free(ATTACK_FIRST_PERSON(amd));
			ATTACK_FIRST_PERSON(amd) = NULL;
		}
		if (ATTACK_THIRD_PERSON(amd)) {
			free(ATTACK_THIRD_PERSON(amd));
			ATTACK_THIRD_PERSON(amd) = NULL;
		}
		if (ATTACK_NOUN(amd)) {
			free(ATTACK_NOUN(amd));
			ATTACK_NOUN(amd) = NULL;
		}
		
		msg_to_char(ch, "Extended data cleared.\r\n");
	}
	else {
		msg_to_char(ch, "Nothing to clear.\r\n");
	}
}


OLC_MODULE(attackedit_damagetype) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You can't change the damage type while editing a message (use .back to return to the menu).\r\n");
	}
	else if (!ATTACK_HAS_EXTENDED_DATA(amd)) {
		msg_to_char(ch, "Set the WEAPON or MOBILE flag to enable extended attack data.\r\n");
	}
	else {
		ATTACK_DAMAGE_TYPE(amd) = olc_process_type(ch, argument, "damage type", "damagetype", damage_types, ATTACK_DAMAGE_TYPE(amd));
	}
}


OLC_MODULE(attackedit_firstperson) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You can't change the strings while editing a message (use .back to return to the menu).\r\n");
	}
	else if (!ATTACK_HAS_EXTENDED_DATA(amd)) {
		msg_to_char(ch, "Set the WEAPON or MOBILE flag to enable extended attack data.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ATTACK_FLAGGED(amd, AMDF_FLAGS_REQUIRE_EXTENDED_DATA)) {
			msg_to_char(ch, "You cannot remove this property while the WEAPON or MOBILE flags are on.\r\n");
		}
		else {
			if (ATTACK_FIRST_PERSON(amd)) {
				free(ATTACK_FIRST_PERSON(amd));
			}
			ATTACK_FIRST_PERSON(amd) = NULL;
			msg_to_char(ch, "You remove the first-person string.\r\n");
		}
	}
	else {
		olc_process_string(ch, argument, "first-person", &ATTACK_FIRST_PERSON(amd));
	}
}


OLC_MODULE(attackedit_flags) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You can't change the flags while editing a message (use .back to return to the menu).\r\n");
		return;
	}
	
	ATTACK_FLAGS(amd) = olc_process_flag(ch, argument, "attack", "flags", attack_message_flags, ATTACK_FLAGS(amd));
	
	if (ATTACK_HAS_EXTENDED_DATA(amd)) {
		// ensure strings if they added a flag that causes extended data
		if (!ATTACK_FIRST_PERSON(amd)) {
			ATTACK_FIRST_PERSON(amd) = strdup(default_attack_first_person);
		}
		if (!ATTACK_THIRD_PERSON(amd)) {
			ATTACK_THIRD_PERSON(amd) = strdup(default_attack_third_person);
		}
		if (!ATTACK_NOUN(amd)) {
			ATTACK_NOUN(amd) = strdup(default_attack_noun);
		}
	}
}


OLC_MODULE(attackedit_message) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	char *arg2;
	int num;
	struct attack_message_set *ams, *next_ams;
	
	char *USAGE = "Usage:  .message <number | add | remove>\r\n";
	
	// arg2 for remove only
	arg2 = one_argument(argument, arg);
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You are already editing a message (use .back to return to the menu).\r\n");
	}
	else if (!*arg) {
		send_to_char(USAGE, ch);
	}
	else if (!str_cmp(arg, "add")) {
		// create message
		CREATE(ams, struct attack_message_set, 1);
		add_attack_message(amd, ams);
		
		// and send to the editor
		GET_OLC_ATTACK_NUM(ch->desc) = ATTACK_NUM_MSGS(amd);
		olc_show_attack_message(ch);
	}
	else if (is_abbrev(arg, "remove")) {
		skip_spaces(&arg2);
		if (!*arg2 || !isdigit(*arg2)) {
			msg_to_char(ch, "Usage:  .message remove <number>\r\n");
		}
		else if ((num = atoi(arg2)) < 1 || num > ATTACK_NUM_MSGS(amd)) {
			msg_to_char(ch, "Invalid message number '%s'.\r\n", arg2);
		}
		else {
			LL_FOREACH_SAFE(ATTACK_MSG_LIST(amd), ams, next_ams) {
				if (--num <= 0) {
					// found
					msg_to_char(ch, "You remove message %d.\r\n", atoi(arg2));
					LL_DELETE(ATTACK_MSG_LIST(amd), ams);
					free_attack_message_set(ams);
					return;	// done
				}
			}
			
			// if we got here, somehow there was no match
			msg_to_char(ch, "Message %d not found.\r\n", atoi(arg2));
		}
	}
	else if (isdigit(*arg)) {
		num = atoi(arg);
		if (num < 1 || num > ATTACK_NUM_MSGS(amd)) {
			msg_to_char(ch, "Invalid message number '%s'.\r\n", arg);
		}
		else {
			GET_OLC_ATTACK_NUM(ch->desc) = num;
			olc_show_attack_message(ch);
		}
	}
	else {
		send_to_char(USAGE, ch);
	}
}


OLC_MODULE(attackedit_name) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You can't change the name while editing a message (use .back to return to the menu).\r\n");
	}
	else {
		olc_process_string(ch, argument, "name", &ATTACK_NAME(amd));
	}
}


OLC_MODULE(attackedit_noun) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You can't change the strings while editing a message (use .back to return to the menu).\r\n");
	}
	else if (!ATTACK_HAS_EXTENDED_DATA(amd)) {
		msg_to_char(ch, "Set the WEAPON or MOBILE flag to enable extended attack data.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ATTACK_FLAGGED(amd, AMDF_FLAGS_REQUIRE_EXTENDED_DATA)) {
			msg_to_char(ch, "You cannot remove this property while the WEAPON or MOBILE flags are on.\r\n");
		}
		else {
			if (ATTACK_NOUN(amd)) {
				free(ATTACK_NOUN(amd));
			}
			ATTACK_NOUN(amd) = NULL;
			msg_to_char(ch, "You remove the noun string.\r\n");
		}
	}
	else {
		olc_process_string(ch, argument, "noun", &ATTACK_NOUN(amd));
	}
}


OLC_MODULE(attackedit_speed) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	double val;
	int speed_type;
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You can't change the speed while editing a message (use .back to return to the menu).\r\n");
	}
	else if (!ATTACK_HAS_EXTENDED_DATA(amd)) {
		msg_to_char(ch, "Set the WEAPON or MOBILE flag to enable extended attack data.\r\n");
	}
	else {
		half_chop(argument, arg1, arg2);
		
		if (!*arg1 || !*arg2) {
			msg_to_char(ch, "Usage:  .speed <fast | normal | slow> <value>\r\n");
		}
		else if ((speed_type = search_block(arg1, attack_speed_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid speed type '%s'.\r\n", arg1);
		}
		else if (!str_cmp(arg2, "basic") || !str_cmp(arg2, "base")) {
			ATTACK_SPEED(amd, speed_type) = basic_speed;
			msg_to_char(ch, "%s attack set to %.1f seconds.\r\n", attack_speed_types[speed_type], ATTACK_SPEED(amd, speed_type));
		}
		else if ((val = atof(arg2)) < 0.1 || val > 10.0) {
			msg_to_char(ch, "You must specify a speed between 0.1 and 10.0 seconds.\r\n");
		}
		else {
			ATTACK_SPEED(amd, speed_type) = val;
			msg_to_char(ch, "%s attack set to %.1f seconds.\r\n", attack_speed_types[speed_type], ATTACK_SPEED(amd, speed_type));
		}
	}
}


OLC_MODULE(attackedit_thirdperson) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You can't change the strings while editing a message (use .back to return to the menu).\r\n");
	}
	else if (!ATTACK_HAS_EXTENDED_DATA(amd)) {
		msg_to_char(ch, "Set the WEAPON or MOBILE flag to enable extended attack data.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		if (ATTACK_FLAGGED(amd, AMDF_FLAGS_REQUIRE_EXTENDED_DATA)) {
			msg_to_char(ch, "You cannot remove this property while the WEAPON or MOBILE flags are on.\r\n");
		}
		else {
			if (ATTACK_THIRD_PERSON(amd)) {
				free(ATTACK_THIRD_PERSON(amd));
			}
			ATTACK_THIRD_PERSON(amd) = NULL;
			msg_to_char(ch, "You remove the third-person string.\r\n");
		}
	}
	else {
		olc_process_string(ch, argument, "third-person", &ATTACK_THIRD_PERSON(amd));
	}
}


OLC_MODULE(attackedit_weapontype) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You can't change the weapon type while editing a message (use .back to return to the menu).\r\n");
	}
	else if (!ATTACK_HAS_EXTENDED_DATA(amd)) {
		msg_to_char(ch, "Set the WEAPON or MOBILE flag to enable extended attack data.\r\n");
	}
	else {
		ATTACK_WEAPON_TYPE(amd) = olc_process_type(ch, argument, "weapon type", "weapontype", weapon_types, ATTACK_WEAPON_TYPE(amd));
	}
}
