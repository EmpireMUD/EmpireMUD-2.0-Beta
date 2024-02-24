/* ************************************************************************
*   File: olc.attack.c                                    EmpireMUD 2.0b5 *
*  Usage: loading, saving, OLC, and functions for attack messages         *
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
"* death blows. As of EmpireMUD 2.0 b5.166, these have an in-game editor under\n"
"* the .attack command.\n"
"*\n"
"* All records must start with 'M### 0' (for 'Message') where ### is the vnum.\n"
"* then the name with a tilde (~)\n"
"* then an optional death log with a tilde (~)\n"
"* then the messages (one per line):\n"
"*   Death Message (damager, damagee, onlookers)\n"
"*   Miss Message (damager, damagee, onlookers)\n"
"*   Hit message (damager, damagee, onlookers)\n"
"*   God message (damager, damagee, onlookers)\n"
"*\n"
"* Each message must be contained in one line. They will be sent through\n"
"* act(), meaning that all standard act() codes apply. '#' can be substituted\n"
"* for a message if none should be printed. Note however that, unlike the\n"
"* socials file, all twelve lines must be present for each record, regardless\n"
"* of any '#' fields which may be contained in it.\n"
"*\n"
"* Messages that can be used as weapon or mob attacks have additional properties\n"
"* including flags, which on the same line as the M###:\n"
"*   a. can be used on a weapon            d. can apply poison\n"
"*   b. can be used on a mob/morph         e. auditor will ignore empty messages\n"
"*   c. is disarmable\n"
"* The final number on the M### line can be another attack message number, in\n"
"* which case this attack counts as the other type, too. This allows custom\n"
"* attack types that still count as stab for backstabbing, for example.\n"
"* For even more attack properties, the M### line can end with a + (plus sign):\n"
"*   ------------------------\n"
"*   M123 a 0 +\n"
"*   name~\n"
"*   death-log~\n"
"*   first-person verb~\n"
"*   third-person verb~\n"
"*   noun form of the attack~\n"
"*   0 0 4.0 4.0 4.0\n"
"*   ...message lines\n"
"*   ------------------------\n"
"* The final numeric line contains 'damage-type weapon-type fast normal slow'.\n"
"*   damage-type: 0=physical, 1=magical, 2=fire, 3=poison, 4=direct\n"
"*   weapon-type: 0=blunt, 1=sharp, 2=magic\n"
"*   speeds: number of seconds between attacks; lower is better\n"
"* The death log is optional and may just be a tilde (~) on its own line if the\n"
"* attack uses the default log of 'has been killed at'. Death logs automatically\n"
"* start with the player's name and end with the location.\n";

// local funcs
OLC_MODULE(attackedit_speed);
void clear_attack_message(attack_message_data *amd);
void free_attack_message_set(struct attack_message_set *ams);
void write_attack_message_to_file(FILE *fl, attack_message_data *amd);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Copy a single attack message set.
*
* @param struct attack_message_set *from_ams The message set to copy.
* @return struct attack_message_set* The copy.
*/
struct attack_message_set *copy_one_attack_message_entry(struct attack_message_set *from_ams) {
	int iter;
	struct attack_message_set *ams;
	
	CREATE(ams, struct attack_message_set, 1);
	for (iter = 0; iter < NUM_MSG_TYPES; ++iter) {
		if (from_ams->msg[iter].attacker_msg) {
			ams->msg[iter].attacker_msg = strdup(from_ams->msg[iter].attacker_msg);
		}
		if (from_ams->msg[iter].victim_msg) {
			ams->msg[iter].victim_msg = strdup(from_ams->msg[iter].victim_msg);
		}
		if (from_ams->msg[iter].room_msg) {
			ams->msg[iter].room_msg = strdup(from_ams->msg[iter].room_msg);
		}
	}
	
	return ams;
}


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
* Determines if one type is equal to another, also checking its counts-as data.
*
* @param any_vnum type An attack vnum/type to check.
* @param any_vnum match_to Another attack vnum/type.
* @return bool TRUE if type equals match_to, or if type counts-as match_to. FALSE if not.
*/
bool match_attack_type(any_vnum type, any_vnum match_to) {
	if (type == match_to) {
		return TRUE;	// shortcut
	}
	else {
		attack_message_data *amd = real_attack_message(type);
		return (amd && ATTACK_COUNTS_AS(amd) == match_to);
	}
}


/**
* This will copy any messages on 'from' that are not on 'to'.
*
* @param char_data *ch The person editing.
* @param attack_message_data *from Messages to copy from.
* @param attack_message_data *to Messages to copy to.
*/
void smart_copy_attack_messages(char_data *ch, attack_message_data *from, attack_message_data *to) {
	bool fail, has;
	int iter, copied;
	struct attack_message_set *from_ams, *to_ams, *ams;
	
	copied = 0;
	
	LL_FOREACH(ATTACK_MSG_LIST(from), from_ams) {
		has = FALSE;
		LL_FOREACH(ATTACK_MSG_LIST(to), to_ams) {
			fail = FALSE;
			for (iter = 0; iter < NUM_MSG_TYPES && !has && !fail; ++iter) {
				if (from_ams->msg[iter].attacker_msg && !to_ams->msg[iter].attacker_msg) {
					fail = TRUE;	// one has, other doesn't
				}
				else if (to_ams->msg[iter].attacker_msg && !from_ams->msg[iter].attacker_msg) {
					fail = TRUE;	// one has, other doesn't
				}
				else if (to_ams->msg[iter].attacker_msg && from_ams->msg[iter].attacker_msg && strcmp(to_ams->msg[iter].attacker_msg, from_ams->msg[iter].attacker_msg)) {
					fail = TRUE;	// not identical
				}
				else if (from_ams->msg[iter].victim_msg && !to_ams->msg[iter].victim_msg) {
					fail = TRUE;	// one has, other doesn't
				}
				else if (to_ams->msg[iter].victim_msg && !from_ams->msg[iter].victim_msg) {
					fail = TRUE;	// one has, other doesn't
				}
				else if (to_ams->msg[iter].victim_msg && from_ams->msg[iter].victim_msg && strcmp(to_ams->msg[iter].victim_msg, from_ams->msg[iter].victim_msg)) {
					fail = TRUE;	// not identical
				}
				if (from_ams->msg[iter].room_msg && !to_ams->msg[iter].room_msg) {
					fail = TRUE;	// one has, other doesn't
				}
				else if (to_ams->msg[iter].room_msg && !from_ams->msg[iter].room_msg) {
					fail = TRUE;	// one has, other doesn't
				}
				else if (to_ams->msg[iter].room_msg && from_ams->msg[iter].room_msg && strcmp(to_ams->msg[iter].room_msg, from_ams->msg[iter].room_msg)) {
					fail = TRUE;	// not identical
				}
			}
			if (!fail) {
				has = TRUE;	// looks like we have it
			}
		}
		
		if (!has) {
			// didn't find it on the 'to'.. add it now
			++copied;
			
			ams = copy_one_attack_message_entry(from_ams);
			add_attack_message(to, ams);
		}
	}
	
	if (copied) {
		msg_to_char(ch, "Imported %d message set%s.\r\n", copied, PLURAL(copied));
	}
	else {
		msg_to_char(ch, "No message sets to import.\r\n");
	}
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
	count += wordcount_string(ATTACK_DEATH_LOG(amd));
	
	count += wordcount_string(ATTACK_FIRST_PERSON(amd));
	count += wordcount_string(ATTACK_THIRD_PERSON(amd));
	count += wordcount_string(ATTACK_NOUN(amd));
	
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
	bool mob_only, spec_attack, problem = FALSE;
	attack_message_data *alt, *next_alt;
	struct attack_message_set *ams;
	bool dta, dtv, dtr, mta, mtv, mtr, hta, htv, htr, gta, gtv, gtr;
	
	mob_only = (ATTACK_FLAGGED(amd, AMDF_MOBILE) && !ATTACK_FLAGGED(amd, AMDF_WEAPON));
	spec_attack = !ATTACK_FLAGGED(amd, AMDF_FLAGS_REQUIRE_EXTENDED_DATA) ? TRUE : FALSE;
	
	if (!ATTACK_NAME(amd) || !*ATTACK_NAME(amd) || !str_cmp(ATTACK_NAME(amd), default_attack_name)) {
		olc_audit_msg(ch, ATTACK_VNUM(amd), "No name set");
		problem = TRUE;
	}
	if (!ATTACK_MSG_LIST(amd)) {
		olc_audit_msg(ch, ATTACK_VNUM(amd), "No messages set");
		problem = TRUE;
	}
	
	if (ATTACK_FIRST_PERSON(amd) && isupper(*ATTACK_FIRST_PERSON(amd))) {
		olc_audit_msg(ch, ATTACK_VNUM(amd), "Capitalized first-person string");
		problem = TRUE;
	}
	if (ATTACK_THIRD_PERSON(amd) && isupper(*ATTACK_THIRD_PERSON(amd))) {
		olc_audit_msg(ch, ATTACK_VNUM(amd), "Capitalized third-person string");
		problem = TRUE;
	}
	if (ATTACK_NOUN(amd) && isupper(*ATTACK_NOUN(amd))) {
		olc_audit_msg(ch, ATTACK_VNUM(amd), "Capitalized noun string");
		problem = TRUE;
	}
	
	if (ATTACK_DEATH_LOG(amd)) {
		if (isupper(*ATTACK_DEATH_LOG(amd))) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Death log should not start with capital letter");
			problem = TRUE;
		}
		if (ispunct(*(ATTACK_DEATH_LOG(amd) + strlen(ATTACK_DEATH_LOG(amd)) - 1))) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Death log should not end with punctuation");
			problem = TRUE;
		}
		if (strchr(ATTACK_DEATH_LOG(amd), '$')) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Death log should not contain dollarsigns");
			problem = TRUE;
		}
	}
	
	if (ATTACK_FLAGGED(amd, AMDF_DISARMABLE) && !ATTACK_HAS_EXTENDED_DATA(amd)) {
		olc_audit_msg(ch, ATTACK_VNUM(amd), "DISARMABLE flag but is not a weapon/mob attack type");
		problem = TRUE;
	}
	
	if (ATTACK_HAS_EXTENDED_DATA(amd)) {
		if (ATTACK_SPEED(amd, SPD_SLOW) == ATTACK_SPEED(amd, SPD_NORMAL) || ATTACK_SPEED(amd, SPD_SLOW) == ATTACK_SPEED(amd, SPD_FAST) || ATTACK_SPEED(amd, SPD_FAST) == ATTACK_SPEED(amd, SPD_NORMAL)) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Speeds are the same");
			problem = TRUE;
		}
		if (ATTACK_SPEED(amd, SPD_SLOW) < ATTACK_SPEED(amd, SPD_NORMAL) || ATTACK_SPEED(amd, SPD_SLOW) < ATTACK_SPEED(amd, SPD_FAST) || ATTACK_SPEED(amd, SPD_NORMAL) < ATTACK_SPEED(amd, SPD_FAST)) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Speeds are out of order (lower speed number will be faster in combat)");
			problem = TRUE;
		}
	}
	
	// check my messages
	dta = dtv = dtr = mta = mtv = mtr = hta = htv = htr = gta = gtv = gtr = FALSE;
	LL_FOREACH(ATTACK_MSG_LIST(amd), ams) {
		if (ATTACK_FLAGGED(amd, AMDF_IGNORE_MISSING)) {
			// just skip whole section
			break;
		}
		if (!ams->msg[MSG_DIE].attacker_msg && !dta && !mob_only) {
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
		if (!ams->msg[MSG_MISS].attacker_msg && !mta && !mob_only) {
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
		if (!ams->msg[MSG_HIT].attacker_msg && !hta && spec_attack) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty hit2char message.");
			problem = hta = TRUE;
		}
		if (!ams->msg[MSG_HIT].victim_msg && !htv && spec_attack) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty hit2vict message.");
			problem = htv = TRUE;
		}
		if (!ams->msg[MSG_HIT].room_msg && !htr && spec_attack) {
			olc_audit_msg(ch, ATTACK_VNUM(amd), "Empty hit2room message.");
			problem = htr = TRUE;
		}
		if (!ams->msg[MSG_GOD].attacker_msg && !gta && !mob_only) {
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
	char lbuf[1024];
	
	if (detail) {
		sprintbit(ATTACK_FLAGS(amd), attack_message_flags, lbuf, TRUE);
		snprintf(output, sizeof(output), "[%5d] %s (%d) %s", ATTACK_VNUM(amd), ATTACK_NAME(amd), ATTACK_NUM_MSGS(amd), lbuf);
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
	char type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	int count, iter;
	attack_message_data *amd, *next_amd;
	struct attack_message_set *ams;
	
	bitvector_t only_flags = NOBITS, not_flagged = NOBITS;
	int only_damage = NOTHING, only_weapon = NOTHING;
	int vmin = NOTHING, vmax = NOTHING;
	double speed = 0.0;
	
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
	
	add_page_display(ch, "Attack message fullsearch: %s", show_color_codes(find_keywords));
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
			if (ATTACK_DEATH_LOG(amd) && multi_isname(find_keywords, ATTACK_DEATH_LOG(amd))) {
				any = TRUE;
			}
			else if (ATTACK_NOUN(amd) && multi_isname(find_keywords, ATTACK_NOUN(amd))) {
				any = TRUE;
			}
			else if (ATTACK_FIRST_PERSON(amd) && multi_isname(find_keywords, ATTACK_FIRST_PERSON(amd))) {
				any = TRUE;
			}
			else if (ATTACK_THIRD_PERSON(amd) && multi_isname(find_keywords, ATTACK_THIRD_PERSON(amd))) {
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
		add_page_display(ch, "[%5d] %s", ATTACK_VNUM(amd), ATTACK_NAME(amd));
		++count;
	}
	
	if (count > 0) {
		add_page_display(ch, "(%d attacks)", count);
	}
	else {
		add_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


/**
* Searches for all uses of an attack message and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The attack message vnum.
*/
void olc_search_attack_message(char_data *ch, any_vnum vnum) {
	bool any;
	int found;
	attack_message_data *amd = real_attack_message(vnum);
	attack_message_data *amditer, *next_amd;
	ability_data *abil, *next_abil;
	generic_data *gen, *next_gen;
	char_data *mob, *next_mob;
	morph_data *morph, *next_morph;
	obj_data *obj, *next_obj;
	
	if (!amd) {
		msg_to_char(ch, "There is no attack message %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	add_page_display(ch, "Occurrences of attack message %d (%s):", vnum, ATTACK_NAME(amd));
	
	// abilities
	HASH_ITER(hh, ability_table, abil, next_abil) {
		any = FALSE;
		any |= (ABIL_ATTACK_TYPE(abil) == vnum);
		any |= find_ability_data_entry_for_misc(abil, ADL_LIMITATION, ABIL_LIMIT_WIELD_ATTACK_TYPE, vnum) ? TRUE : FALSE;
		
		if (any) {
			++found;
			add_page_display(ch, "ABIL [%5d] %s", ABIL_VNUM(abil), ABIL_NAME(abil));
		}
	}
	
	// other attacks
	HASH_ITER(hh, attack_message_table, amditer, next_amd) {
		if (ATTACK_COUNTS_AS(amditer) == vnum) {
			++found;
			add_page_display(ch, "ATTACK [%5d] %s", ATTACK_VNUM(amditer), ATTACK_NAME(amditer));
		}
	}
	
	// generics
	HASH_ITER(hh, generic_table, gen, next_gen) {
		if (GET_AFFECT_DOT_ATTACK(gen) > 0 && GET_AFFECT_DOT_ATTACK(gen) == vnum) {
			++found;
			add_page_display(ch, "GEN [%5d] %s", GEN_VNUM(gen), GEN_NAME(gen));
		}
	}
	
	// mobs
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		if (MOB_ATTACK_TYPE(mob) == vnum) {
			++found;
			add_page_display(ch, "MOB [%5d] %s", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob));
		}
	}
	
	// morphs
	HASH_ITER(hh, morph_table, morph, next_morph) {
		if (MORPH_ATTACK_TYPE(morph) == vnum) {
			++found;
			add_page_display(ch, "MPH [%5d] %s", MORPH_VNUM(morph), MORPH_SHORT_DESC(morph));
		}
	}
	
	// objs
	HASH_ITER(hh, object_table, obj, next_obj) {
		any = (IS_WEAPON(obj) && GET_WEAPON_TYPE(obj) == vnum);
		any |= (IS_MISSILE_WEAPON(obj) && GET_MISSILE_WEAPON_TYPE(obj) == vnum);
		
		if (any) {
			++found;
			add_page_display(ch, "OBJ [%5d] %s", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
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
	struct attack_message_set *ams, *new_ams, *list;
	
	// copy set in order
	list = NULL;
	LL_FOREACH(input_list, ams) {
		new_ams = copy_one_attack_message_entry(ams);
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
	if (ATTACK_DEATH_LOG(amd) && (!proto || ATTACK_DEATH_LOG(amd) != ATTACK_DEATH_LOG(proto))) {
		free(ATTACK_DEATH_LOG(amd));
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
		if (sscanf(chk, "M%d %s %d %c", &type, str_in, &int_in[0], &char_in) == 4 && char_in == '+') {
			// b5.166 format: type was in M line, extended format
			version = 5166;
			extended = TRUE;
		}
		else if (sscanf(chk, "M%d %s %d ", &type, str_in, &int_in[0]) == 3) {
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
			int_in[0] = 0;
		}
		
		// find or create message entry
		amd = find_attack_message(type, TRUE);
		snprintf(error, sizeof(error), "attack message %d:%d", type, ATTACK_NUM_MSGS(amd));
		
		// header info?
		ATTACK_FLAGS(amd) = asciiflag_conv(str_in);
		ATTACK_COUNTS_AS(amd) = int_in[0];
		
		if (version >= 5166) {
			// read: name
			if (ATTACK_NAME(amd)) {
				// free first: more than 1 entry can have the name
				free(ATTACK_NAME(amd));
			}
			ATTACK_NAME(amd) = fread_string(fl, error);
			
			// read: deathlog
			if (ATTACK_DEATH_LOG(amd)) {
				// free first: more than 1 entry can have this
				free(ATTACK_DEATH_LOG(amd));
			}
			ATTACK_DEATH_LOG(amd) = fread_string(fl, error);
			
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
		fprintf(fl, "M%d %s %d %s\n", ATTACK_VNUM(amd), bitv_to_alpha(ATTACK_FLAGS(amd)), ATTACK_COUNTS_AS(amd), (ATTACK_HAS_EXTENDED_DATA(amd) && !wrote_extended) ? "+" : "");	// M# indicates the b5.166 attack message format
		fprintf(fl, "%s~\n", NULLSAFE(ATTACK_NAME(amd)));
		fprintf(fl, "%s~\n", NULLSAFE(ATTACK_DEATH_LOG(amd)));
		
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
	bool found;
	char name[256];
	ability_data *abil, *next_abil;
	attack_message_data *amd, *amditer, *next_amd;
	descriptor_data *desc;
	generic_data *gen, *next_gen;
	char_data *mob, *next_mob;
	morph_data *morph, *next_morph;
	obj_data *obj, *next_obj;
	
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
	
	// update other attacks
	HASH_ITER(hh, attack_message_table, amditer, next_amd) {
		if (ATTACK_COUNTS_AS(amditer) == vnum) {
			ATTACK_COUNTS_AS(amditer) = 0;
			save_attack_message_file();
		}
	}
	
	// update generics
	HASH_ITER(hh, generic_table, gen, next_gen) {
		if (GET_AFFECT_DOT_ATTACK(gen) > 0 && GET_AFFECT_DOT_ATTACK(gen) == vnum) {
			GEN_VALUE(gen, GVAL_AFFECT_DOT_ATTACK) = 0;
			save_library_file_for_vnum(DB_BOOT_GEN, GEN_VNUM(gen));
		}
	}
	
	// update mobs
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		if (MOB_ATTACK_TYPE(mob) == vnum) {
			MOB_ATTACK_TYPE(mob) = 0;
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Mobile %d %s lost deleted attack type", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob));
			save_library_file_for_vnum(DB_BOOT_MOB, GET_MOB_VNUM(mob));
		}
	}
	
	// update morphs
	HASH_ITER(hh, morph_table, morph, next_morph) {
		if (MORPH_ATTACK_TYPE(morph) == vnum) {
			MORPH_ATTACK_TYPE(morph) = 0;
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Morph %d %s lost deleted attack type", MORPH_VNUM(morph), MORPH_SHORT_DESC(morph));
			save_library_file_for_vnum(DB_BOOT_MORPH, MORPH_VNUM(morph));
		}
	}
	
	// update objs
	HASH_ITER(hh, object_table, obj, next_obj) {
		found = FALSE;
		if (IS_WEAPON(obj) && GET_WEAPON_TYPE(obj) == vnum) {
			set_obj_val(obj, VAL_WEAPON_TYPE, 0);
			found = TRUE;
		}
		else if (IS_MISSILE_WEAPON(obj) && GET_MISSILE_WEAPON_TYPE(obj) == vnum) {
			set_obj_val(obj, VAL_MISSILE_WEAPON_TYPE, 0);
			found = TRUE;
		}
		
		if (found) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Object %d %s lost deleted weapon type", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
			save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
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
		if (GET_OLC_ATTACK(desc)) {
			if (ATTACK_COUNTS_AS(GET_OLC_ATTACK(desc)) == vnum) {
				ATTACK_COUNTS_AS(GET_OLC_ATTACK(desc)) = 0;
				msg_to_char(desc->character, "The counts-as attack for the attack you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_GENERIC(desc)) {
			if (GET_AFFECT_DOT_ATTACK(GET_OLC_GENERIC(desc)) > 0 && GET_AFFECT_DOT_ATTACK(GET_OLC_GENERIC(desc)) == vnum) {
				GEN_VALUE(GET_OLC_GENERIC(desc), GVAL_AFFECT_DOT_ATTACK) = 0;
				msg_to_char(desc->character, "An attack type used by the generic you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_MOBILE(desc)) {
			if (MOB_ATTACK_TYPE(GET_OLC_MOBILE(desc)) == vnum) {
				MOB_ATTACK_TYPE(GET_OLC_MOBILE(desc)) = 0;
				msg_to_char(desc->character, "An attack type used by the mob you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_MORPH(desc)) {
			if (MORPH_ATTACK_TYPE(GET_OLC_MORPH(desc)) == vnum) {
				MORPH_ATTACK_TYPE(GET_OLC_MORPH(desc)) = 0;
				msg_to_char(desc->character, "An attack type used by the morph you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_OBJECT(desc)) {
			found = FALSE;
			if (IS_WEAPON(GET_OLC_OBJECT(desc)) && GET_WEAPON_TYPE(GET_OLC_OBJECT(desc)) == vnum) {
				set_obj_val(GET_OLC_OBJECT(desc), VAL_WEAPON_TYPE, 0);
				found = TRUE;
			}
			else if (IS_MISSILE_WEAPON(GET_OLC_OBJECT(desc)) && GET_MISSILE_WEAPON_TYPE(GET_OLC_OBJECT(desc)) == vnum) {
				set_obj_val(GET_OLC_OBJECT(desc), VAL_MISSILE_WEAPON_TYPE, 0);
				found = TRUE;
			}
			
			if (found) {
				msg_to_char(desc->character, "An attack type used by the weapon you're editing was deleted.\r\n");
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
	if (ATTACK_DEATH_LOG(proto)) {
		free(ATTACK_DEATH_LOG(proto));
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
	if (ATTACK_DEATH_LOG(amd) && !*ATTACK_DEATH_LOG(amd)) {
		free(ATTACK_DEATH_LOG(amd));
		ATTACK_DEATH_LOG(amd) = NULL;
	}
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
		ATTACK_DEATH_LOG(dupe) = ATTACK_DEATH_LOG(input) ? str_dup(ATTACK_DEATH_LOG(input)) : NULL;
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
* @param bool details If TRUE, shows full messages (due to -d option on vstat).
*/
void do_stat_attack_message(char_data *ch, attack_message_data *amd, bool details) {
	char lbuf[MAX_STRING_LENGTH];
	char *to_show;
	int count, iter;
	struct attack_message_set *ams, *next_ams;
	struct page_display *pd;
	
	if (!amd) {
		return;
	}
	
	// first line
	add_page_display(ch, "VNum: [\tc%d\t0], Name: \ty%s\t0, Message count: [\tc%d\t0]", ATTACK_VNUM(amd), ATTACK_NAME(amd), ATTACK_NUM_MSGS(amd));
	
	if (ATTACK_COUNTS_AS(amd) > 0) {
		add_page_display(ch, "Also counts as: [\tc%d\t0] \ty%s\t0", ATTACK_COUNTS_AS(amd), (ATTACK_COUNTS_AS(amd) > 0) ? get_attack_name_by_vnum(ATTACK_COUNTS_AS(amd)) : "(none)");
	}
	
	sprintbit(ATTACK_FLAGS(amd), attack_message_flags, lbuf, TRUE);
	add_page_display(ch, "Flags: \tg%s\t0", lbuf);
	
	if (ATTACK_HAS_EXTENDED_DATA(amd)) {
		add_page_display(ch, "Strings: [\ty%s\t0, \ty%s\t0, \ty%s\t0]", NULLSAFE(ATTACK_FIRST_PERSON(amd)), NULLSAFE(ATTACK_THIRD_PERSON(amd)), NULLSAFE(ATTACK_NOUN(amd)));
		
		// Damage, Weapon, Speeds (all same line)
		pd = add_page_display(ch, "Damage type: [\tg%s\t0], Weapon type: [\tg%s\t0], Speeds: [", damage_types[ATTACK_DAMAGE_TYPE(amd)], weapon_types[ATTACK_WEAPON_TYPE(amd)]);
		for (iter = 0; iter < NUM_ATTACK_SPEEDS; ++iter) {
			append_page_display_line(pd, "%s\tc%.1f\t0", iter > 0 ? " | " : "", ATTACK_SPEED(amd, iter));
		}
		append_page_display_line(pd, "]");
	}
	
	add_page_display(ch, "Death log: %s", ATTACK_DEATH_LOG(amd) ? ATTACK_DEATH_LOG(amd) : "(default)");
	
	add_page_display(ch, "Messages:");
	count = 0;
	
	// message section
	if (details) {
		// show entire message set (hopefully)
		LL_FOREACH_SAFE(ATTACK_MSG_LIST(amd), ams, next_ams) {
			++count;
			for (iter = 0; iter < NUM_MSG_TYPES; ++iter) {
				if (ams->msg[iter].attacker_msg) {
					add_page_display_str(ch, ams->msg[iter].attacker_msg);
				}
				else {
					add_page_display_str(ch, "#");
				}
				if (ams->msg[iter].victim_msg) {
					add_page_display_str(ch, ams->msg[iter].victim_msg);
				}
				else {
					add_page_display_str(ch, "#");
				}
				if (ams->msg[iter].room_msg) {
					add_page_display_str(ch, ams->msg[iter].room_msg);
				}
				else {
					add_page_display_str(ch, "#");
				}
			}
			
			// separator
			if (next_ams) {
				for (iter = 0; iter < 79; ++iter) {
					lbuf[iter] = '-';
				}
				lbuf[iter] = '\0';
				add_page_display_str(ch, lbuf);
			}
		}
	}
	else {	// abbreviated messages
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
			add_page_display(ch, "%d. %s", ++count, to_show ? to_show : "(blank)");
		}
	}
	if (!count) {
		add_page_display(ch, " none");
	}
	
	send_page_display(ch);
}


/**
* Sub-menu when editing a specific messsage.
*
* @param char_data *ch The person who is editing a message and will see its display.
*/
void olc_show_one_message(char_data *ch) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams;
	
	const char *divider = "-------------------------------------------------------------------------------";
	
	// find message
	if (!(ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc)))) {
		// return to main menu
		GET_OLC_ATTACK_NUM(ch->desc) = 0;
		olc_show_attack_message(ch);
		return;
	}
	
	// one message view
	add_page_display(ch, "[%s%d\t0] %s%s\t0: Message %s%d\t0", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, ATTACK_NAME(amd), OLC_LABEL_UNCHANGED, GET_OLC_ATTACK_NUM(ch->desc));
	
	add_page_display_str(ch, divider);
	
	// messages
	add_page_display(ch, "<%sdie2char\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_DIE].attacker_msg), ams->msg[MSG_DIE].attacker_msg ? ams->msg[MSG_DIE].attacker_msg : "(none)");
	add_page_display(ch, "<%sdie2vict\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_DIE].victim_msg), ams->msg[MSG_DIE].victim_msg ? ams->msg[MSG_DIE].victim_msg : "(none)");
	add_page_display(ch, "<%sdie2room\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_DIE].room_msg), ams->msg[MSG_DIE].room_msg ? ams->msg[MSG_DIE].room_msg : "(none)");
	
	add_page_display_str(ch, divider);
	
	add_page_display(ch, "<%smiss2char\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_MISS].attacker_msg), ams->msg[MSG_MISS].attacker_msg ? ams->msg[MSG_MISS].attacker_msg : "(none)");
	add_page_display(ch, "<%smiss2vict\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_MISS].victim_msg), ams->msg[MSG_MISS].victim_msg ? ams->msg[MSG_MISS].victim_msg : "(none)");
	add_page_display(ch, "<%smiss2room\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_MISS].room_msg), ams->msg[MSG_MISS].room_msg ? ams->msg[MSG_MISS].room_msg : "(none)");
	
	add_page_display_str(ch, divider);
	
	add_page_display(ch, "<%shit2char\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_HIT].attacker_msg), ams->msg[MSG_HIT].attacker_msg ? ams->msg[MSG_HIT].attacker_msg : "(none)");
	add_page_display(ch, "<%shit2vict\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_HIT].victim_msg), ams->msg[MSG_HIT].victim_msg ? ams->msg[MSG_HIT].victim_msg : "(none)");
	add_page_display(ch, "<%shit2room\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_HIT].room_msg), ams->msg[MSG_HIT].room_msg ? ams->msg[MSG_HIT].room_msg : "(none)");
	
	add_page_display_str(ch, divider);
	
	add_page_display(ch, "<%sgod2char\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_GOD].attacker_msg), ams->msg[MSG_GOD].attacker_msg ? ams->msg[MSG_GOD].attacker_msg : "(none)");
	add_page_display(ch, "<%sgod2vict\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_GOD].victim_msg), ams->msg[MSG_GOD].victim_msg ? ams->msg[MSG_GOD].victim_msg : "(none)");
	add_page_display(ch, "<%sgod2room\t0> %s", OLC_LABEL_PTR(ams->msg[MSG_GOD].room_msg), ams->msg[MSG_GOD].room_msg ? ams->msg[MSG_GOD].room_msg : "(none)");
	
	add_page_display_str(ch, divider);
	
	add_page_display(ch, "Return to main menu: <%sback\t0>", OLC_LABEL_UNCHANGED);
	
	send_page_display(ch);
}


/**
* This is the main recipe display for attack OLC. It displays the user's
* currently-edited attack.
*
* @param char_data *ch The person who is editing an attack and will see its display.
*/
void olc_show_attack_message(char_data *ch) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	char lbuf[MAX_STRING_LENGTH];
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
	
	add_page_display(ch, "[%s%d\t0] %s%s\t0", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !real_attack_message(ATTACK_VNUM(amd)) ? "new attack message" : ATTACK_NAME(real_attack_message(ATTACK_VNUM(amd))));
	add_page_display(ch, "<%sname\t0> %s", OLC_LABEL_STR(ATTACK_NAME(amd), default_attack_name), NULLSAFE(ATTACK_NAME(amd)));
	add_page_display(ch, "<%scountsas\t0> %d %s", OLC_LABEL_VAL(ATTACK_COUNTS_AS(amd), 0), ATTACK_COUNTS_AS(amd), (ATTACK_COUNTS_AS(amd) > 0) ? get_attack_name_by_vnum(ATTACK_COUNTS_AS(amd)) : "(none)");
	add_page_display(ch, "<%sdeathlog\t0> %s", OLC_LABEL_PTR(ATTACK_DEATH_LOG(amd)), ATTACK_DEATH_LOG(amd) ? ATTACK_DEATH_LOG(amd) : "(default)");
	
	sprintbit(ATTACK_FLAGS(amd), attack_message_flags, lbuf, TRUE);
	add_page_display(ch, "<%sflags\t0> %s", OLC_LABEL_VAL(ATTACK_FLAGS(amd), NOBITS), lbuf);
	
	if (ATTACK_HAS_EXTENDED_DATA(amd)) {
		add_page_display(ch, "<%sfirstperson\t0> %s", OLC_LABEL_STR(ATTACK_FIRST_PERSON(amd), default_attack_first_person), ATTACK_FIRST_PERSON(amd) ? ATTACK_FIRST_PERSON(amd) : "(none)");
		add_page_display(ch, "<%sthirdperson\t0> %s", OLC_LABEL_STR(ATTACK_THIRD_PERSON(amd), default_attack_third_person), ATTACK_THIRD_PERSON(amd) ? ATTACK_THIRD_PERSON(amd) : "(none)");
		add_page_display(ch, "<%snoun\t0> %s", OLC_LABEL_STR(ATTACK_NOUN(amd), default_attack_noun), ATTACK_NOUN(amd) ? ATTACK_NOUN(amd) : "(none)");
		
		add_page_display(ch, "<%sdamagetype\t0> %s", OLC_LABEL_VAL(ATTACK_DAMAGE_TYPE(amd), 0), damage_types[ATTACK_DAMAGE_TYPE(amd)]);
		add_page_display(ch, "<%sweapontype\t0> %s", OLC_LABEL_VAL(ATTACK_WEAPON_TYPE(amd), 0), weapon_types[ATTACK_WEAPON_TYPE(amd)]);
		
		add_page_display(ch, "Speeds: <%sspeed\t0>", OLC_LABEL_CHANGED);
		for (iter = 0; iter < NUM_ATTACK_SPEEDS; ++iter) {
			add_page_display(ch, "   %s: %s%.1f\t0 seconds", attack_speed_types[iter], OLC_LABEL_VAL(ATTACK_SPEED(amd, iter), basic_speed), ATTACK_SPEED(amd, iter));
		}
	}
	
	add_page_display(ch, "Messages: <%smessage\t0>", OLC_LABEL_PTR(ATTACK_MSG_LIST(amd)));
	
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
		add_page_display(ch, "%d. %s", ++count, to_show ? to_show : "(blank)");
	}
	
	if (ATTACK_HAS_EXTENDED_DATA(amd) && !ATTACK_FLAGGED(amd, AMDF_FLAGS_REQUIRE_EXTENDED_DATA)) {
		add_page_display(ch, "To clear extended data: <%sclearextended\t0>", OLC_LABEL_CHANGED);
	}
	
	send_page_display(ch);
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
			add_page_display(ch, "%3d. [%5d] %s", ++found, ATTACK_VNUM(iter), ATTACK_NAME(iter));
		}
	}
	
	send_page_display(ch);
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MESSAGE MODULES /////////////////////////////////////////////////////

#define _AMS_TARG_ATTACKER	0
#define _AMS_TARG_VICTIM	1
#define _AMS_TARG_ROOM		2

/**
* Handles all the message modules.
*
* @param char_data *ch The person editing.
* @param char *prompt The name of the field, like "die2char".
* @param char *argument What the player typed.
* @param int msg_type One of the MSG_ types.
* @param int ams_targ _AMS_TARG_ATTACKER, etc.
*/
void attackedit_message_str(char_data *ch, char *prompt, char *argument, int msg_type, int ams_targ) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	struct attack_message_set *ams = get_one_attack_message(amd, GET_OLC_ATTACK_NUM(ch->desc));
	char **str_ptr;
	
	if (GET_OLC_ATTACK_NUM(ch->desc) == 0 || !ams) {
		msg_to_char(ch, "You are not editing an attack message. Selet one with .message <number> before setting this.\r\n");
		return;
	}
	
	switch (ams_targ) {
		case _AMS_TARG_ATTACKER: {
			str_ptr = &(ams->msg[msg_type].attacker_msg);
			break;
		}
		case _AMS_TARG_VICTIM: {
			str_ptr = &(ams->msg[msg_type].victim_msg);
			break;
		}
		case _AMS_TARG_ROOM: {
			str_ptr = &(ams->msg[msg_type].room_msg);
			break;
		}
		default: {
			msg_to_char(ch, "That field is configured incorrectly. Please report it as a bug.\r\n");
			return;
		}
	}
	
	if (!str_cmp(argument, "none") || !str_cmp(argument, "#")) {
		if (*str_ptr) {
			free(*str_ptr);
		}
		*str_ptr = NULL;
		msg_to_char(ch, "You remove the %s message.\r\n", prompt);
	}
	else {
		olc_process_string(ch, argument, prompt, str_ptr);
	}
}


OLC_MODULE(attackedit_die2char) {
	attackedit_message_str(ch, "die2char", argument, MSG_DIE, _AMS_TARG_ATTACKER);
}

OLC_MODULE(attackedit_die2vict) {
	attackedit_message_str(ch, "die2vict", argument, MSG_DIE, _AMS_TARG_VICTIM);
}

OLC_MODULE(attackedit_die2room) {
	attackedit_message_str(ch, "die2room", argument, MSG_DIE, _AMS_TARG_ROOM);
}


OLC_MODULE(attackedit_miss2char) {
	attackedit_message_str(ch, "miss2char", argument, MSG_MISS, _AMS_TARG_ATTACKER);
}

OLC_MODULE(attackedit_miss2vict) {
	attackedit_message_str(ch, "miss2vict", argument, MSG_MISS, _AMS_TARG_VICTIM);
}

OLC_MODULE(attackedit_miss2room) {
	attackedit_message_str(ch, "miss2room", argument, MSG_MISS, _AMS_TARG_ROOM);
}


OLC_MODULE(attackedit_hit2char) {
	attackedit_message_str(ch, "hit2char", argument, MSG_HIT, _AMS_TARG_ATTACKER);
}

OLC_MODULE(attackedit_hit2vict) {
	attackedit_message_str(ch, "hit2vict", argument, MSG_HIT, _AMS_TARG_VICTIM);
}

OLC_MODULE(attackedit_hit2room) {
	attackedit_message_str(ch, "hit2room", argument, MSG_HIT, _AMS_TARG_ROOM);
}


OLC_MODULE(attackedit_god2char) {
	attackedit_message_str(ch, "god2char", argument, MSG_GOD, _AMS_TARG_ATTACKER);
}

OLC_MODULE(attackedit_god2vict) {
	attackedit_message_str(ch, "god2vict", argument, MSG_GOD, _AMS_TARG_VICTIM);
}

OLC_MODULE(attackedit_god2room) {
	attackedit_message_str(ch, "god2room", argument, MSG_GOD, _AMS_TARG_ROOM);
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


OLC_MODULE(attackedit_countsas) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	attack_message_data *find;
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You can't set that while editing a message (use .back to return to the menu).\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		ATTACK_COUNTS_AS(amd) = 0;
		msg_to_char(ch, "You remove the counts-as type.\r\n");
	}
	else if (!(find = find_attack_message_by_name_or_vnum(argument, FALSE))) {
		msg_to_char(ch, "Unknown attack type '%s'.\r\n", argument);
	}
	else {
		ATTACK_COUNTS_AS(amd) = ATTACK_VNUM(find);
		msg_to_char(ch, "It now counts as [%d] %s.\r\n", ATTACK_VNUM(find), ATTACK_NAME(find));
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


OLC_MODULE(attackedit_deathlog) {
	attack_message_data *amd = GET_OLC_ATTACK(ch->desc);
	
	if (GET_OLC_ATTACK_NUM(ch->desc) != 0) {
		msg_to_char(ch, "You can't change the death log while editing a message (use .back to return to the menu).\r\n");
	}
	else if (!str_cmp(argument, "none") || !str_cmp(argument, "default")) {
		if (ATTACK_DEATH_LOG(amd)) {
			free(ATTACK_DEATH_LOG(amd));
		}
		ATTACK_DEATH_LOG(amd) = NULL;
		msg_to_char(ch, "It will now use the default death log.\r\n");
	}
	else {
		olc_process_string(ch, argument, "death log", &ATTACK_DEATH_LOG(amd));
	}
}


OLC_MODULE(attackedit_fast) {
	char arg[MAX_INPUT_LENGTH];
	snprintf(arg, sizeof(arg), "fast %s", argument);
	attackedit_speed(ch, type, arg);
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
	attack_message_data *other;
	struct attack_message_set *new_ams, *ams, *next_ams;
	
	char *USAGE = "Usage:  .message <number | add | copy | duplicate | remove>\r\n";
	
	// arg2 for remove only
	arg2 = one_argument(argument, arg);
	
	if (!*arg) {
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
	else if (!str_cmp(arg, "copy")) {
		skip_spaces(&arg2);
		if (GET_OLC_ATTACK_NUM(ch->desc) > 0) {
			msg_to_char(ch, "You can't do that while editing a message. Go back to the menu first.\r\n");
		}
		else if (!*arg2 || !isdigit(*arg2)) {
			msg_to_char(ch, "Usage:  .message copy <vnum>\r\n");
		}
		else if (!(other = real_attack_message(atoi(arg2)))) {
			msg_to_char(ch, "Unknown attack vnum '%s'.\r\n", arg2);
		}
		else {
			// this sends its own messages
			smart_copy_attack_messages(ch, other, amd);
		}
	}
	else if (is_abbrev(arg, "duplicate")) {
		skip_spaces(&arg2);
		if (GET_OLC_ATTACK_NUM(ch->desc) > 0) {
			msg_to_char(ch, "You can't do that while editing a message. Go back to the menu first.\r\n");
		}
		else if (!*arg2 || !isdigit(*arg2)) {
			msg_to_char(ch, "Usage:  .message duplicate <message number>\r\n");
		}
		else if ((num = atoi(arg2)) < 1 || num > ATTACK_NUM_MSGS(amd)) {
			msg_to_char(ch, "Invalid message number to duplicate.\r\n");
		}
		else {
			new_ams = NULL;
			LL_FOREACH(ATTACK_MSG_LIST(amd), ams) {
				if (--num <= 0) {
					// found
					new_ams = copy_one_attack_message_entry(ams);
					break;	// only copy 1
				}
			}
			
			if (new_ams) {
				// success
				add_attack_message(amd, new_ams);
				GET_OLC_ATTACK_NUM(ch->desc) = ATTACK_NUM_MSGS(amd);
				olc_show_attack_message(ch);
			}
			else {
				msg_to_char(ch, "Invalid message number to duplicate.\r\n");
			}
		}
	}
	else if (is_abbrev(arg, "remove")) {
		skip_spaces(&arg2);
		if (GET_OLC_ATTACK_NUM(ch->desc) > 0) {
			msg_to_char(ch, "You can't do that while editing a message. Go back to the menu first.\r\n");
		}
		else if (!*arg2 || !isdigit(*arg2)) {
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


OLC_MODULE(attackedit_normal) {
	char arg[MAX_INPUT_LENGTH];
	snprintf(arg, sizeof(arg), "normal %s", argument);
	attackedit_speed(ch, type, arg);
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


OLC_MODULE(attackedit_slow) {
	char arg[MAX_INPUT_LENGTH];
	snprintf(arg, sizeof(arg), "slow %s", argument);
	attackedit_speed(ch, type, arg);
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
