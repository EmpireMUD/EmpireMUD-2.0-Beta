/* ************************************************************************
*   File: morph.c                                         EmpireMUD 2.0b5 *
*  Usage: morph loading, saving, and OLC                                  *
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
*   Edit Modules
*/

// external functions
ACMD(do_dismount);

// local data
const char *default_morph_keywords = "morph unnamed shapeless";
const char *default_morph_short_desc = "an unnamed morph";
const char *default_morph_long_desc = "A shapeless morph is standing here.";


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Scales morph applies and adds them to the character.
*
* @param char_data *ch The character to add morph affects to.
*/
void add_morph_affects(char_data *ch) {
	double points_available, remaining, share;
	morph_data *morph = GET_MORPH(ch);
	struct affected_type *af = NULL;
	int scale, total_weight, value;
	struct apply_data *app;
	bool any;
	
	// no work
	if (!morph) {
		return;
	}
	
	// determine scale cap
	scale = get_approximate_level(ch);
	if (MORPH_MAX_SCALE(morph) > 0) {
		scale = MIN(scale, MORPH_MAX_SCALE(morph));
	}
	
	// determine points
	points_available = scale / 100.0 * config_get_double("scale_points_at_100");
	points_available = MAX(points_available, 1.0);
	
	// weight by mob flags
	// TODO should this be configured somewhere? these are based on obj flag scaling -pc
	if (MOB_FLAGGED(ch, MOB_HARD)) {
		points_available *= 1.2;
	}
	if (MOB_FLAGGED(ch, MOB_GROUP)) {
		points_available *= 1.3333;
	}
	
	// figure out how many total weight points are used
	total_weight = 0;
	LL_FOREACH(MORPH_APPLIES(morph), app) {
		if (!apply_never_scales[app->location]) {
			if (app->weight > 0) {
				total_weight += app->weight;
			}
			else if (app->weight < 0) {
				points_available += ABSOLUTE(app->weight);
			}
		}
	}
	
	// start adding applies
	remaining = points_available;
	any = FALSE;
	LL_FOREACH(MORPH_APPLIES(morph), app) {
		if (apply_never_scales[app->location]) {
			af = create_mod_aff(ATYPE_MORPH, UNLIMITED, app->location, app->weight, ch);
		}
		else if (app->weight > 0 && remaining > 0) {	// positive aff
			// check remaining
			share = (((double)app->weight) / total_weight) * points_available;	// % of total
			share = MIN(share, remaining);	// check limit
			value = round(share * (1.0 / apply_values[app->location]));
			if (value > 0 || !any) {	// always give at least 1 point on the first one
				any = TRUE;
				value = MAX(1, value);
				remaining -= (value * apply_values[app->location]);	// subtract actual amount used
				
				// create the actual apply
				af = create_mod_aff(ATYPE_MORPH, UNLIMITED, app->location, value, ch);
			}
		}
		else if (app->weight < 0) {	// negative aff
			value = round(app->weight * (1.0 / apply_values[app->location]));
			value = MIN(-1, value);	// minimum of -1
			af = create_mod_aff(ATYPE_MORPH, UNLIMITED, app->location, value, ch);
		}
		else {	// no aff
			continue;
		}
		
		if (af) {
			affect_to_char(ch, af);
			free(af);
			af = NULL;
		}
	}
	
	if (MORPH_AFFECTS(morph)) {
		af = create_flag_aff(ATYPE_MORPH, UNLIMITED, MORPH_AFFECTS(morph), ch);
		affect_to_char(ch, af);
		free(af);
	}
	
	// in case nothing else did
	affect_total(ch);
}


/**
* Ensures that a player can still be in their morph. This may un-morph them.
*
* @param char_data *ch The character to check.
*/
void check_morph_ability(char_data *ch) {
	bool revert = FALSE;
	
	// nothing to check
	if (!IS_MORPHED(ch)) {
		return;
	}
	
	if (!revert && MORPH_ABILITY(GET_MORPH(ch)) != NO_ABIL && !has_ability(ch, MORPH_ABILITY(GET_MORPH(ch)))) {
		revert = TRUE;
	}
	if (!revert && CHAR_MORPH_FLAGGED(ch, MORPHF_CHECK_SOLO) && !check_solo_role(ch)) {
		revert = TRUE;
	}
	
	if (revert) {
		finish_morphing(ch, NULL);
	}
}


/**
* Morphs a player back to normal. This function takes just 1 arg so it can
* be passed e.g. as a function pointer in vampire blood upkeep.
*
* @param char_data *ch The morph-ed one.
*/
void end_morph(char_data *ch) {
	if (IS_MORPHED(ch)) {
		perform_morph(ch, NULL);
	}
}


/**
* Used for choosing a morph that's valid for the player.
*
* @param char_data *ch The person trying to morph.
* @param char *name The argument.
* @return morph_data* The matching morph, or NULL if none.
*/
morph_data *find_morph_by_name(char_data *ch, char *name) {
	morph_data *morph, *next_morph, *partial = NULL;
	char temp[MAX_STRING_LENGTH];
	int number;
	bool had_number = isdigit(*name) ? TRUE : FALSE;
	
	number = get_number(&name);
	if (number == 0) {
		return NULL;	// 0.morph has no meaning
	}
	
	HASH_ITER(sorted_hh, sorted_morphs, morph, next_morph) {
		if (MORPH_FLAGGED(morph, MORPHF_IN_DEVELOPMENT | MORPHF_SCRIPT_ONLY) && !IS_IMMORTAL(ch)) {
			continue;
		}
		if (MORPH_ABILITY(morph) != NO_ABIL && !has_ability(ch, MORPH_ABILITY(morph))) {
			continue;
		}
		if (MORPH_REQUIRES_OBJ(morph) != NOTHING && !has_required_object(ch, MORPH_REQUIRES_OBJ(morph))) {
			continue;
		}
		
		// matches:
		strcpy(temp, skip_filler(MORPH_SHORT_DESC(morph)));
		if (!had_number && !str_cmp(name, temp)) {
			// perfect match
			if (--number == 0) {
				return morph;
			}
		}
		if ((!partial || had_number) && multi_isname(name, MORPH_KEYWORDS(morph))) {
			// probable match
			if (had_number && --number == 0) {
				return morph;
			}
			else if (!had_number && !partial) {
				partial = morph;
			}
		}
	}
	
	// no exact match...
	return partial;
}


/**
* This is called by the morphing action, as well as the instamorph in do_morph.
* It sends messages and finishes the morph.
*
* @param char_data *ch The person for whom it's morphin' time.
* @param morph_data *morph Which form to change into (NULL for none).
*/
void finish_morphing(char_data *ch, morph_data *morph) {
	char lbuf[MAX_STRING_LENGTH];
	
	// can't be disguised while morphed
	if (IS_DISGUISED(ch) && morph != NULL) {
		undisguise(ch);
	}
	
	sprintf(lbuf, "%s has become $n!", PERS(ch, ch, FALSE));

	perform_morph(ch, morph);

	act(lbuf, TRUE, ch, 0, 0, TO_ROOM);
	act("You have become $n!", FALSE, ch, 0, 0, TO_CHAR);
	
	if (morph && MORPH_ABILITY(morph) != NO_ABIL) {
		gain_ability_exp(ch, MORPH_ABILITY(morph), 33.4);
		run_ability_hooks(ch, AHOOK_ABILITY, MORPH_ABILITY(morph), 0, ch, NULL, NULL, NULL, NOBITS);
	}

	// update msdp
	update_MSDP_name(ch, UPDATE_SOON);
}


/**
* This function gets/generates a short/long description for a morphed person
* or npc. It returns simple output if NOT morphed, but you should really only
* use this for a morphed person.
* 
* @param char_data *ch The person who is morphed.
* @param bool long_desc_if_true If TRUE, returns a long description instead of a short desc.
* @return const char* A pointer to the generated description.
*/
const char *get_morph_desc(char_data *ch, bool long_desc_if_true) {
	static char output[MAX_STRING_LENGTH], realname[128];
	
	// all senses end up needing the real name
	if (IS_NPC(ch)) {
		strcpy(realname, GET_SHORT_DESC(ch));
	}
	else {
		snprintf(realname, sizeof(realname), "%s%s%s", GET_PC_NAME(ch), GET_CURRENT_LASTNAME(ch) ? " " : "", NULLSAFE(GET_CURRENT_LASTNAME(ch)));
	}
	
	if (GET_MORPH(ch)) {
		char *source = long_desc_if_true ? MORPH_LONG_DESC(GET_MORPH(ch)) : MORPH_SHORT_DESC(GET_MORPH(ch));
		if (strstr(source, "#n")) {
			char *tmp = str_replace("#n", realname, source);
			strcpy(output, tmp);
			free(tmp);	// str_replace allocated this
			if (long_desc_if_true) {
				CAP(output);
			}
			return output;
		}
		else if (long_desc_if_true) {
			return MORPH_LONG_DESC(GET_MORPH(ch));
		}
		else {	// short desc and no data
			return MORPH_SHORT_DESC(GET_MORPH(ch));
		}
	}
	else {	// not morphed
		snprintf(output, sizeof(output), "%s%s", realname, long_desc_if_true ? " is standing here." : "");
		return output;
	}
}


/**
* Checks morph affinities.
*
* @param room_data *location Where the morph affinity is happening.
* @param morph_data *morph Which morph the person is trying to morph into.
* @return bool TRUE if the morph passes the affinities check.
*/
bool morph_affinity_ok(room_data *location, morph_data *morph) {
	bool ok = TRUE;
	bitvector_t climate;
	
	// shortcut
	if (!morph) {
		return TRUE;
	}
	
	climate = get_climate(location);
	
	if (MORPH_FLAGGED(morph, MORPHF_TEMPERATE_AFFINITY) && !IS_SET(climate, CLIM_TEMPERATE)) {
		ok = FALSE;
	}
	if (MORPH_FLAGGED(morph, MORPHF_ARID_AFFINITY) && !IS_SET(climate, CLIM_ARID)) {
		ok = FALSE;
	}
	if (MORPH_FLAGGED(morph, MORPHF_TROPICAL_AFFINITY) && !IS_SET(climate, CLIM_TROPICAL)) {
		ok = FALSE;
	}
	
	return ok;
}


/**
* Puts a character into a morph form (or back) with no message.
*
* @param char_data *ch The person to morph.
* @param morph_data *morph Which morph (NULL to revert to normal).
*/
void perform_morph(char_data *ch, morph_data *morph) {
	double move_mod, health_mod, mana_mod;
	
	// read current pools
	health_mod = (double) GET_HEALTH(ch) / MAX(1, GET_MAX_HEALTH(ch));
	move_mod = (double) GET_MOVE(ch) / MAX(1, GET_MAX_MOVE(ch));
	mana_mod = (double) GET_MANA(ch) / MAX(1, GET_MAX_MANA(ch));
	
	// remove all existing morph effects
	affect_from_char(ch, ATYPE_MORPH, FALSE);

	if (IS_RIDING(ch) && morph != NULL) {
		do_dismount(ch, "", 0, 0);
	}

	// Set the new form
	GET_MORPH(ch) = morph;
	add_morph_affects(ch);

	// set new pools
	set_health(ch, (int) (GET_MAX_HEALTH(ch) * health_mod));
	set_move(ch, (int) (GET_MAX_MOVE(ch) * move_mod));
	set_mana(ch, (int) (GET_MAX_MANA(ch) * mana_mod));
	
	// in case this is called by something else while they're already morphing
	if (!IS_NPC(ch) && GET_ACTION(ch) == ACT_MORPHING) {
		end_action(ch);
	}
}


/**
* Counts the words of text in a morph's strings.
*
* @param morph_data *mph The morph whose strings to count.
* @return int The number of words in the morph's strings.
*/
int wordcount_morph(morph_data *mph) {
	int count = 0;
	
	count += wordcount_string(MORPH_KEYWORDS(mph));
	count += wordcount_string(MORPH_SHORT_DESC(mph));
	count += wordcount_string(MORPH_LONG_DESC(mph));
	count += wordcount_string(MORPH_LOOK_DESC(mph));
	
	return count;
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common morph problems and reports them to ch.
*
* @param morph_data *morph The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_morph(morph_data *morph, char_data *ch) {
	char temp[MAX_STRING_LENGTH];
	struct apply_data *app;
	bool problem = FALSE;
	attack_message_data *amd = NULL;
	obj_data *obj = NULL;
	
	if (MORPH_FLAGGED(morph, MORPHF_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!MORPH_KEYWORDS(morph) || !*MORPH_KEYWORDS(morph) || !str_cmp(MORPH_KEYWORDS(morph), default_morph_keywords)) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "Keywords not set");
		problem = TRUE;
	}
	
	if (!MORPH_SHORT_DESC(morph) || !*MORPH_SHORT_DESC(morph) || !str_cmp(MORPH_SHORT_DESC(morph), default_morph_short_desc)) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "No short description");
		problem = TRUE;
	}
	any_one_arg(MORPH_SHORT_DESC(morph), temp);
	if ((fill_word(temp) || reserved_word(temp)) && isupper(*temp)) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "Short desc capitalized");
		problem = TRUE;
	}
	if (ispunct(MORPH_SHORT_DESC(morph)[strlen(MORPH_SHORT_DESC(morph)) - 1])) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "Short desc has punctuation");
		problem = TRUE;
	}
	
	if (!MORPH_LONG_DESC(morph) || !*MORPH_LONG_DESC(morph) || !str_cmp(MORPH_LONG_DESC(morph), default_morph_long_desc)) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "No long description");
		problem = TRUE;
	}
	if (!ispunct(MORPH_LONG_DESC(morph)[strlen(MORPH_LONG_DESC(morph)) - 1])) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "Long desc missing punctuation");
		problem = TRUE;
	}
	if (islower(*MORPH_LONG_DESC(morph))) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "Long desc not capitalized");
		problem = TRUE;
	}
	
	if (!MORPH_LOOK_DESC(morph) || !*MORPH_LOOK_DESC(morph)) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "No look description");
		problem = TRUE;
	}
	
	for (app = MORPH_APPLIES(morph); app; app = app->next) {
		if (app->location == APPLY_NONE || app->weight == 0) {
			olc_audit_msg(ch, MORPH_VNUM(morph), "Invalid apply: %d to %s", app->weight, apply_types[app->location]);
			problem = TRUE;
		}
	}
	
	if (MORPH_ABILITY(morph) == NO_ABIL && MORPH_REQUIRES_OBJ(morph) == NOTHING && !MORPH_FLAGGED(morph, MORPHF_SCRIPT_ONLY)) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "Requires no ability or object");
		problem = TRUE;
	}
	if (MORPH_REQUIRES_OBJ(morph) != NOTHING) {
		if (!(obj = obj_proto(MORPH_REQUIRES_OBJ(morph)))) {
			olc_audit_msg(ch, MORPH_VNUM(morph), "Requires-object does not exist");
			problem = TRUE;
		}
		if (MORPH_FLAGGED(morph, MORPHF_CONSUME_OBJ) && obj && OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			olc_audit_msg(ch, MORPH_VNUM(morph), "Consumed requires-obj is scalable");
			problem = TRUE;
		}
	}
	if (MORPH_FLAGGED(morph, MORPHF_CONSUME_OBJ) && MORPH_REQUIRES_OBJ(morph) == NOTHING) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "Flagged CONSUME-OBJ but no object required");
		problem = TRUE;
	}
	if (MORPH_ATTACK_TYPE(morph) == ATTACK_RESERVED || !(amd = real_attack_message(MORPH_ATTACK_TYPE(morph)))) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "Invalid attack type");
		problem = TRUE;
	}
	if (amd && !ATTACK_FLAGGED(amd, AMDF_MOBILE)) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "Attack type %d is not valid (it is missing the MOBILE flag)", MORPH_ATTACK_TYPE(morph));
		problem = TRUE;
	}
	
	if (count_bits(MORPH_FLAGS(morph) & (MORPHF_TEMPERATE_AFFINITY | MORPHF_ARID_AFFINITY | MORPHF_TROPICAL_AFFINITY)) > 1) {
		olc_audit_msg(ch, MORPH_VNUM(morph), "Multiple affinities");
		problem = TRUE;
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param morph_data *morph The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_morph(morph_data *morph, bool detail) {
	static char output[MAX_STRING_LENGTH];
	char part[MAX_STRING_LENGTH];
	ability_data *abil;
	
	if (detail) {
		// ability required
		if (MORPH_ABILITY(morph) == NO_ABIL) {
			*part = '\0';
		}
		else {
			sprintf(part, " (%s", get_ability_name_by_vnum(MORPH_ABILITY(morph)));
			if ((abil = find_ability_by_vnum(MORPH_ABILITY(morph))) && ABIL_ASSIGNED_SKILL(abil) != NULL) {
				sprintf(part + strlen(part), " - %s %d", SKILL_ABBREV(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
			}
			strcat(part, ")");
		}
		
		snprintf(output, sizeof(output), "[%5d] %s%s", MORPH_VNUM(morph), MORPH_SHORT_DESC(morph), part);
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", MORPH_VNUM(morph), MORPH_SHORT_DESC(morph));
	}
		
	return output;
}


/**
* Searches for all uses of a morph and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The morph vnum.
*/
void olc_search_morph(char_data *ch, any_vnum vnum) {
	morph_data *morph = morph_proto(vnum);
	int found;
	
	if (!morph) {
		msg_to_char(ch, "There is no morph %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	build_page_display(ch, "Occurrences of morph %d (%s):", vnum, MORPH_SHORT_DESC(morph));
	
	// morphs are not actually used anywhere else
	
	if (found > 0) {
		build_page_display(ch, "%d location%s shown", found, PLURAL(found));
	}
	else {
		build_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


// Simple vnum sorter for the morph hash
int sort_morphs(morph_data *a, morph_data *b) {
	return MORPH_VNUM(a) - MORPH_VNUM(b);
}


// typealphabetic sorter for sorted_morphs
int sort_morphs_by_data(morph_data *a, morph_data *b) {
	ability_data *a_abil, *b_abil;
	int a_level, b_level;
	
	a_abil = find_ability_by_vnum(MORPH_ABILITY(a));
	b_abil = find_ability_by_vnum(MORPH_ABILITY(b));
	a_level = a_abil ? ABIL_SKILL_LEVEL(a_abil) : 0;
	b_level = b_abil ? ABIL_SKILL_LEVEL(b_abil) : 0;
	
	// reverse level sort
	if (a_level != b_level) {
		return b_level - a_level;
	}
	
	return strcmp(NULLSAFE(MORPH_KEYWORDS(a)), NULLSAFE(MORPH_KEYWORDS(b)));
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts a morph into the hash table.
*
* @param morph_data *morph The morph data to add to the table.
*/
void add_morph_to_table(morph_data *morph) {
	morph_data *find;
	any_vnum vnum;
	
	if (morph) {
		vnum = MORPH_VNUM(morph);
		HASH_FIND_INT(morph_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(morph_table, vnum, morph);
			HASH_SORT(morph_table, sort_morphs);
		}
		
		// sorted table
		HASH_FIND(sorted_hh, sorted_morphs, &vnum, sizeof(int), find);
		if (!find) {
			HASH_ADD(sorted_hh, sorted_morphs, vnum, sizeof(int), morph);
			HASH_SRT(sorted_hh, sorted_morphs, sort_morphs_by_data);
		}
	}
}


/**
* @param any_vnum vnum Any morph vnum
* @return morph_data* The morph, or NULL if it doesn't exist
*/
morph_data *morph_proto(any_vnum vnum) {
	morph_data *morph;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(morph_table, &vnum, morph);
	return morph;
}


/**
* Removes a morph from the hash table.
*
* @param morph_data *morph The morph data to remove from the table.
*/
void remove_morph_from_table(morph_data *morph) {
	HASH_DEL(morph_table, morph);
	HASH_DELETE(sorted_hh, sorted_morphs, morph);
}


/**
* Initializes a new morph. This clears all memory for it, so set the vnum
* AFTER.
*
* @param morph_data *morph The morph to initialize.
*/
void clear_morph(morph_data *morph) {
	memset((char *) morph, 0, sizeof(morph_data));
	
	MORPH_VNUM(morph) = NOTHING;
	MORPH_ABILITY(morph) = NO_ABIL;
	MORPH_REQUIRES_OBJ(morph) = NOTHING;
	MORPH_SIZE(morph) = SIZE_NORMAL;
}


/**
* frees up memory for a morph data item.
*
* See also: olc_delete_morph
*
* @param morph_data *morph The morph data to free.
*/
void free_morph(morph_data *morph) {
	morph_data *proto = morph_proto(MORPH_VNUM(morph));
	
	if (MORPH_KEYWORDS(morph) && (!proto || MORPH_KEYWORDS(morph) != MORPH_KEYWORDS(proto))) {
		free(MORPH_KEYWORDS(morph));
	}
	if (MORPH_SHORT_DESC(morph) && (!proto || MORPH_SHORT_DESC(morph) != MORPH_SHORT_DESC(proto))) {
		free(MORPH_SHORT_DESC(morph));
	}
	if (MORPH_LONG_DESC(morph) && (!proto || MORPH_LONG_DESC(morph) != MORPH_LONG_DESC(proto))) {
		free(MORPH_LONG_DESC(morph));
	}
	if (MORPH_LOOK_DESC(morph) && (!proto || MORPH_LOOK_DESC(morph) != MORPH_LOOK_DESC(proto))) {
		free(MORPH_LOOK_DESC(morph));
	}
	
	if (MORPH_APPLIES(morph) && (!proto || MORPH_APPLIES(morph) != MORPH_APPLIES(proto))) {
		free_apply_list(MORPH_APPLIES(morph));
	}
	
	free(morph);
}


/**
* Read one morph from file.
*
* @param FILE *fl The open .morph file
* @param any_vnum vnum The morph vnum
*/
void parse_morph(FILE *fl, any_vnum vnum) {
	char line[256], error[256], str_in[256], str_in2[256];
	morph_data *morph, *find;
	int int_in[4];
	
	CREATE(morph, morph_data, 1);
	clear_morph(morph);
	MORPH_VNUM(morph) = vnum;
	
	HASH_FIND_INT(morph_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate morph vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_morph_to_table(morph);
		
	// for error messages
	sprintf(error, "morph vnum %d", vnum);
	
	// lines 1-3
	MORPH_KEYWORDS(morph) = fread_string(fl, error);
	MORPH_SHORT_DESC(morph) = fread_string(fl, error);
	MORPH_LONG_DESC(morph) = fread_string(fl, error);
	
	// 4. flags attack-type move-type max-level affects [size]
	if (!get_line(fl, line) || sscanf(line, "%s %d %d %d %s %d", str_in, &int_in[0], &int_in[1], &int_in[2], str_in2, &int_in[3]) != 6) {
		int_in[3] = SIZE_NORMAL;	// backward-compatible
		
		if (sscanf(line, "%s %d %d %d %s", str_in, &int_in[0], &int_in[1], &int_in[2], str_in2) != 5) {
			log("SYSERR: Format error in line 4 of %s", error);
			exit(1);
		}
	}
	
	MORPH_FLAGS(morph) = asciiflag_conv(str_in);
	MORPH_ATTACK_TYPE(morph) = int_in[0];
	MORPH_MOVE_TYPE(morph) = int_in[1];
	MORPH_MAX_SCALE(morph) = int_in[2];
	MORPH_AFFECTS(morph) = asciiflag_conv(str_in2);
	MORPH_SIZE(morph) = int_in[3];
	
	// 5. cost-type cost-amount ability requires-obj
	if (!get_line(fl, line) || sscanf(line, "%d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
		log("SYSERR: Format error in line 5 of %s", error);
		exit(1);
	}
	
	MORPH_COST_TYPE(morph) = int_in[0];
	MORPH_COST(morph) = int_in[1];
	MORPH_ABILITY(morph) = int_in[2];
	MORPH_REQUIRES_OBJ(morph) = int_in[3];
		
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'A': {	// applies
				parse_apply(fl, &MORPH_APPLIES(morph), error);
				break;
			}
			case 'D': { // look desc
				MORPH_LOOK_DESC(morph) = fread_string(fl, error);
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


// writes entries in the morph index
void write_morphs_index(FILE *fl) {
	morph_data *morph, *next_morph;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, morph_table, morph, next_morph) {
		// determine "zone number" by vnum
		this = (int)(MORPH_VNUM(morph) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, MORPH_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one morph item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param morph_data *morph The thing to save.
*/
void write_morph_to_file(FILE *fl, morph_data *morph) {
	char temp[256], temp2[256];
	
	if (!fl || !morph) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_morph_to_file called without %s", !fl ? "file" : "morph");
		return;
	}
	
	fprintf(fl, "#%d\n", MORPH_VNUM(morph));
	
	// 1-3. strings
	fprintf(fl, "%s~\n", NULLSAFE(MORPH_KEYWORDS(morph)));
	fprintf(fl, "%s~\n", NULLSAFE(MORPH_SHORT_DESC(morph)));
	fprintf(fl, "%s~\n", NULLSAFE(MORPH_LONG_DESC(morph)));
	
	// 4. flags attack-type move-type max-level affs size
	strcpy(temp, bitv_to_alpha(MORPH_FLAGS(morph)));
	strcpy(temp2, bitv_to_alpha(MORPH_AFFECTS(morph)));
	fprintf(fl, "%s %d %d %d %s %d\n", temp, MORPH_ATTACK_TYPE(morph), MORPH_MOVE_TYPE(morph), MORPH_MAX_SCALE(morph), temp2, MORPH_SIZE(morph));
	
	// 5. cost-type cost-amount ability requires-obj
	fprintf(fl, "%d %d %d %d\n", MORPH_COST_TYPE(morph), MORPH_COST(morph), MORPH_ABILITY(morph), MORPH_REQUIRES_OBJ(morph));
	
	// 'A': applies
	write_applies_to_file(fl, MORPH_APPLIES(morph));
	
	// D: look desc
	if (MORPH_LOOK_DESC(morph) && *MORPH_LOOK_DESC(morph)) {
		strcpy(temp, MORPH_LOOK_DESC(morph));
		strip_crlf(temp);
		fprintf(fl, "D\n%s~\n", temp);
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////


/**
* Creates a new morph entry.
* 
* @param any_vnum vnum The number to create.
* @return morph_data* The new morph's prototype.
*/
morph_data *create_morph_table_entry(any_vnum vnum) {
	morph_data *morph;
	
	// sanity
	if (morph_proto(vnum)) {
		log("SYSERR: Attempting to insert morph at existing vnum %d", vnum);
		return morph_proto(vnum);
	}
	
	CREATE(morph, morph_data, 1);
	clear_morph(morph);
	MORPH_VNUM(morph) = vnum;
	MORPH_KEYWORDS(morph) = str_dup(default_morph_keywords);
	MORPH_SHORT_DESC(morph) = str_dup(default_morph_short_desc);
	MORPH_LONG_DESC(morph) = str_dup(default_morph_long_desc);
	add_morph_to_table(morph);

	// save index and morph file now
	save_index(DB_BOOT_MORPH);
	save_library_file_for_vnum(DB_BOOT_MORPH, vnum);

	return morph;
}


/**
* WARNING: This function actually deletes a morph.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_morph(char_data *ch, any_vnum vnum) {
	char_data *chiter, *next_ch;
	morph_data *morph;
	char name[256];
	
	if (!(morph = morph_proto(vnum))) {
		msg_to_char(ch, "There is no such morph %d.\r\n", vnum);
		return;
	}
	
	snprintf(name, sizeof(name), "%s", NULLSAFE(MORPH_SHORT_DESC(morph)));
	
	// un-morph everyone
	DL_FOREACH_SAFE(character_list, chiter, next_ch) {
		if (IS_MORPHED(chiter) && MORPH_VNUM(GET_MORPH(chiter)) == vnum) {
			finish_morphing(chiter, NULL);
		}
	}
	
	// remove it from the hash table first
	remove_morph_from_table(morph);

	// save index and morph file now
	save_index(DB_BOOT_MORPH);
	save_library_file_for_vnum(DB_BOOT_MORPH, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted morph %d %s", GET_NAME(ch), vnum, name);
	msg_to_char(ch, "Morph %d (%s) deleted.\r\n", vnum, name);
	
	free_morph(morph);
}


/**
* Function to save a player's changes to a morph (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_morph(descriptor_data *desc) {	
	morph_data *proto, *morph = GET_OLC_MORPH(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh, sorted;

	// have a place to save it?
	if (!(proto = morph_proto(vnum))) {
		proto = create_morph_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (MORPH_KEYWORDS(proto)) {
		free(MORPH_KEYWORDS(proto));
	}
	if (MORPH_SHORT_DESC(proto)) {
		free(MORPH_SHORT_DESC(proto));
	}
	if (MORPH_LONG_DESC(proto)) {
		free(MORPH_LONG_DESC(proto));
	}
	if (MORPH_LOOK_DESC(proto)) {
		free(MORPH_LOOK_DESC(proto));
	}
	free_apply_list(MORPH_APPLIES(proto));
	
	// sanity
	if (!MORPH_KEYWORDS(morph) || !*MORPH_KEYWORDS(morph)) {
		if (MORPH_KEYWORDS(morph)) {
			free(MORPH_KEYWORDS(morph));
		}
		MORPH_KEYWORDS(morph) = str_dup(default_morph_keywords);
	}
	if (!MORPH_SHORT_DESC(morph) || !*MORPH_SHORT_DESC(morph)) {
		if (MORPH_SHORT_DESC(morph)) {
			free(MORPH_SHORT_DESC(morph));
		}
		MORPH_SHORT_DESC(morph) = str_dup(default_morph_short_desc);
	}
	if (!MORPH_LONG_DESC(morph) || !*MORPH_LONG_DESC(morph)) {
		if (MORPH_LONG_DESC(morph)) {
			free(MORPH_LONG_DESC(morph));
		}
		MORPH_LONG_DESC(morph) = str_dup(default_morph_long_desc);
	}
	if (MORPH_LOOK_DESC(morph) && !*MORPH_LOOK_DESC(morph)) {
		free(MORPH_LOOK_DESC(morph));
		MORPH_LOOK_DESC(morph) = NULL;
	}

	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	sorted = proto->sorted_hh;
	*proto = *morph;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	proto->sorted_hh = sorted;
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_MORPH, vnum);

	// ... and re-sort
	HASH_SRT(sorted_hh, sorted_morphs, sort_morphs_by_data);
}


/**
* Creates a copy of a morph, or clears a new one, for editing.
* 
* @param morph_data *input The morph to copy, or NULL to make a new one.
* @return morph_data* The copied morph.
*/
morph_data *setup_olc_morph(morph_data *input) {
	morph_data *new;
	
	CREATE(new, morph_data, 1);
	clear_morph(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		MORPH_KEYWORDS(new) = MORPH_KEYWORDS(input) ? str_dup(MORPH_KEYWORDS(input)) : NULL;
		MORPH_SHORT_DESC(new) = MORPH_SHORT_DESC(input) ? str_dup(MORPH_SHORT_DESC(input)) : NULL;
		MORPH_LONG_DESC(new) = MORPH_LONG_DESC(input) ? str_dup(MORPH_LONG_DESC(input)) : NULL;
		MORPH_LOOK_DESC(new) = MORPH_LOOK_DESC(input) ? str_dup(MORPH_LOOK_DESC(input)) : NULL;
		
		// copy lists
		MORPH_APPLIES(new) = copy_apply_list(MORPH_APPLIES(input));
	}
	else {
		// brand new: some defaults
		MORPH_KEYWORDS(new) = str_dup(default_morph_keywords);
		MORPH_SHORT_DESC(new) = str_dup(default_morph_short_desc);
		MORPH_LONG_DESC(new) = str_dup(default_morph_long_desc);
		MORPH_FLAGS(new) = MORPHF_IN_DEVELOPMENT;
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
* @param morph_data *morph The morph to display.
*/
void do_stat_morph(char_data *ch, morph_data *morph) {
	char part[MAX_STRING_LENGTH];
	struct apply_data *app;
	ability_data *abil;
	int num;
	struct page_display *pd;
	
	if (!morph) {
		return;
	}
	
	// first line
	build_page_display(ch, "VNum: [\tc%d\t0], Keywords: \tc%s\t0, Short desc: \ty%s\t0", MORPH_VNUM(morph), MORPH_KEYWORDS(morph), MORPH_SHORT_DESC(morph));
	build_page_display(ch, "L-Desc: \ty%s\t0\r\n%s", MORPH_LONG_DESC(morph), NULLSAFE(MORPH_LOOK_DESC(morph)));
	
	snprintf(part, sizeof(part), "%s", (MORPH_ABILITY(morph) == NO_ABIL ? "none" : get_ability_name_by_vnum(MORPH_ABILITY(morph))));
	if ((abil = find_ability_by_vnum(MORPH_ABILITY(morph))) && ABIL_ASSIGNED_SKILL(abil) != NULL) {
		snprintf(part + strlen(part), sizeof(part) - strlen(part), " (%s %d)", SKILL_ABBREV(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
	}
	build_page_display(ch, "Cost: [\ty%d %s\t0], Max Level: [\ty%d%s\t0], Requires Ability: [\ty%s\t0]", MORPH_COST(morph), MORPH_COST(morph) > 0 ? pool_types[MORPH_COST_TYPE(morph)] : "/ none", MORPH_MAX_SCALE(morph), (MORPH_MAX_SCALE(morph) == 0 ? " none" : ""), part);
	
	if (MORPH_REQUIRES_OBJ(morph) != NOTHING) {
		build_page_display(ch, "Requires item: [%d] \tg%s\t0", MORPH_REQUIRES_OBJ(morph), skip_filler(get_obj_name_by_proto(MORPH_REQUIRES_OBJ(morph))));
	}
	
	build_page_display(ch, "Attack type: \ty%d %s\t0, Move type: \ty%s\t0, Size: \ty%s\t0", MORPH_ATTACK_TYPE(morph), get_attack_name_by_vnum(MORPH_ATTACK_TYPE(morph)), mob_move_types[MORPH_MOVE_TYPE(morph)], size_types[MORPH_SIZE(morph)]);
	
	sprintbit(MORPH_FLAGS(morph), morph_flags, part, TRUE);
	build_page_display(ch, "Flags: \tg%s\t0", part);
	
	sprintbit(MORPH_AFFECTS(morph), affected_bits, part, TRUE);
	build_page_display(ch, "Affects: \tc%s\t0", part);
	
	// applies
	pd = build_page_display(ch, "Applies: ");
	for (app = MORPH_APPLIES(morph), num = 0; app; app = app->next, ++num) {
		append_page_display_line(pd, "%s%d to %s", num ? ", " : "", app->weight, apply_types[app->location]);
	}
	if (!MORPH_APPLIES(morph)) {
		append_page_display_line(pd, "none");
	}
	
	send_page_display(ch);
}


/**
* This is the main recipe display for morph OLC. It displays the user's
* currently-edited morph.
*
* @param char_data *ch The person who is editing a morph and will see its display.
*/
void olc_show_morph(char_data *ch) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	char lbuf[MAX_STRING_LENGTH];
	struct apply_data *app;
	ability_data *abil;
	int num;
	
	if (!morph) {
		return;
	}
	
	build_page_display(ch, "[%s%d\t0] %s%s\t0", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !morph_proto(MORPH_VNUM(morph)) ? "new morph" : MORPH_SHORT_DESC(morph_proto(MORPH_VNUM(morph))));
	build_page_display(ch, "<%skeywords\t0> %s", OLC_LABEL_STR(MORPH_KEYWORDS(morph), default_morph_keywords), NULLSAFE(MORPH_KEYWORDS(morph)));
	build_page_display(ch, "<%sshortdescription\t0> %s", OLC_LABEL_STR(MORPH_SHORT_DESC(morph), default_morph_short_desc), NULLSAFE(MORPH_SHORT_DESC(morph)));
	build_page_display(ch, "<%slongdescription\t0> %s", OLC_LABEL_STR(MORPH_LONG_DESC(morph), default_morph_long_desc), NULLSAFE(MORPH_LONG_DESC(morph)));
	build_page_display(ch, "<%slookdescription\t0>\r\n%s", OLC_LABEL_PTR(MORPH_LOOK_DESC(morph)), NULLSAFE(MORPH_LOOK_DESC(morph)));
	
	sprintbit(MORPH_FLAGS(morph), morph_flags, lbuf, TRUE);
	build_page_display(ch, "<%sflags\t0> %s", OLC_LABEL_VAL(MORPH_FLAGS(morph), MORPHF_IN_DEVELOPMENT), lbuf);
	
	build_page_display(ch, "<%sattack\t0> %d %s", OLC_LABEL_VAL(MORPH_ATTACK_TYPE(morph), 0), MORPH_ATTACK_TYPE(morph), get_attack_name_by_vnum(MORPH_ATTACK_TYPE(morph)));
	build_page_display(ch, "<%smovetype\t0> %s", OLC_LABEL_VAL(MORPH_MOVE_TYPE(morph), 0), mob_move_types[MORPH_MOVE_TYPE(morph)]);
	build_page_display(ch, "<%ssize\t0> %s", OLC_LABEL_VAL(MORPH_SIZE(morph), SIZE_NORMAL), size_types[MORPH_SIZE(morph)]);

	sprintbit(MORPH_AFFECTS(morph), affected_bits, lbuf, TRUE);
	build_page_display(ch, "<%saffects\t0> %s", OLC_LABEL_VAL(MORPH_AFFECTS(morph), NOBITS), lbuf);
	
	if (MORPH_MAX_SCALE(morph) > 0) {
		build_page_display(ch, "<%smaxlevel\t0> %d", OLC_LABEL_CHANGED, MORPH_MAX_SCALE(morph));
	}
	else {
		build_page_display(ch, "<%smaxlevel\t0> none", OLC_LABEL_UNCHANGED);
	}
	
	build_page_display(ch, "<%scost\t0> %d", OLC_LABEL_VAL(MORPH_COST(morph), 0), MORPH_COST(morph));
	build_page_display(ch, "<%scosttype\t0> %s", OLC_LABEL_VAL(MORPH_COST_TYPE(morph), 0), pool_types[MORPH_COST_TYPE(morph)]);
	
	// ability required
	if (MORPH_ABILITY(morph) == NO_ABIL || !(abil = find_ability_by_vnum(MORPH_ABILITY(morph)))) {
		strcpy(buf1, "none");
	}
	else {
		sprintf(buf1, "%s", ABIL_NAME(abil));
		if (ABIL_ASSIGNED_SKILL(abil)) {
			sprintf(buf1 + strlen(buf1), " (%s %d)", SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
		}
	}
	build_page_display(ch, "<%srequiresability\t0> %s", OLC_LABEL_VAL(MORPH_ABILITY(morph), NO_ABIL), buf1);

	build_page_display(ch, "<%srequiresobject\t0> %d - %s", OLC_LABEL_VAL(MORPH_REQUIRES_OBJ(morph), NOTHING), MORPH_REQUIRES_OBJ(morph), MORPH_REQUIRES_OBJ(morph) == NOTHING ? "none" : get_obj_name_by_proto(MORPH_REQUIRES_OBJ(morph)));
	
	// applies
	build_page_display(ch, "Applies: <%sapply\t0>", OLC_LABEL_PTR(MORPH_APPLIES(morph)));
	for (app = MORPH_APPLIES(morph), num = 1; app; app = app->next, ++num) {
		build_page_display_col(ch, 2, FALSE, " %2d. %d to %s", num, app->weight, apply_types[app->location]);
	}
	
	send_page_display(ch);
}


/**
* Searches the morph db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_morph(char *searchname, char_data *ch) {
	morph_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, morph_table, iter, next_iter) {
		if (multi_isname(searchname, MORPH_KEYWORDS(iter))) {
			build_page_display(ch, "%3d. [%5d] %s", ++found, MORPH_VNUM(iter), MORPH_SHORT_DESC(iter));
		}
	}
	
	send_page_display(ch);
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(morphedit_ability) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	ability_data *abil;
	
	if (!*argument) {
		msg_to_char(ch, "Require what ability (or 'none')?\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		MORPH_ABILITY(morph) = NO_ABIL;
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It will require no ability.\r\n");
		}
	}
	else if (!(abil = find_ability(argument))) {
		msg_to_char(ch, "Invalid ability '%s'.\r\n", argument);
	}
	else {
		MORPH_ABILITY(morph) = ABIL_VNUM(abil);
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It now requires the %s ability.\r\n", ABIL_NAME(abil));
		}
	}
}


OLC_MODULE(morphedit_affects) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	MORPH_AFFECTS(morph) = olc_process_flag(ch, argument, "affects", "affects", affected_bits, MORPH_AFFECTS(morph));
}


OLC_MODULE(morphedit_apply) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	olc_process_applies(ch, argument, &MORPH_APPLIES(morph));
}


OLC_MODULE(morphedit_attack) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	attack_message_data *amd;
	
	if (!*argument) {
		msg_to_char(ch, "Set the attack type to what attack message (vnum or name)?\r\n");
	}
	else if (!(amd = find_attack_message_by_name_or_vnum(argument, FALSE))) {
		msg_to_char(ch, "Unknown attack message '%s'.\r\n", argument);
	}
	else if (!ATTACK_FLAGGED(amd, AMDF_MOBILE)) {
		msg_to_char(ch, "That attack type is not available on morphs.\r\n");
	}
	else {
		MORPH_ATTACK_TYPE(morph) = ATTACK_VNUM(amd);
		msg_to_char(ch, "Attack type set to [%d] %s.\r\n", ATTACK_VNUM(amd), ATTACK_NAME(amd));
	}
}


OLC_MODULE(morphedit_cost) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	MORPH_COST(morph) = olc_process_number(ch, argument, "cost", "cost", 0, INT_MAX, MORPH_COST(morph));
}


OLC_MODULE(morphedit_costtype) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	MORPH_COST_TYPE(morph) = olc_process_type(ch, argument, "cost type", "costtype", pool_types, MORPH_COST_TYPE(morph));
}


OLC_MODULE(morphedit_flags) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	bool had_indev = IS_SET(MORPH_FLAGS(morph), MORPHF_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	MORPH_FLAGS(morph) = olc_process_flag(ch, argument, "morph", "flags", morph_flags, MORPH_FLAGS(morph));
	
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !IS_SET(MORPH_FLAGS(morph), MORPHF_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(MORPH_FLAGS(morph), MORPHF_IN_DEVELOPMENT);
	}
}


OLC_MODULE(morphedit_keywords) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	olc_process_string(ch, argument, "keywords", &MORPH_KEYWORDS(morph));
}


OLC_MODULE(morphedit_longdesc) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	olc_process_string(ch, argument, "long description", &MORPH_LONG_DESC(morph));
}


OLC_MODULE(morphedit_lookdescription) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);

	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", MORPH_SHORT_DESC(morph));
		start_string_editor(ch->desc, buf, &MORPH_LOOK_DESC(morph), MAX_PLAYER_DESCRIPTION, TRUE);
	}
}


OLC_MODULE(morphedit_maxlevel) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	MORPH_MAX_SCALE(morph) = olc_process_number(ch, argument, "maximum level", "maxlevel", 0, MAX_INT, MORPH_MAX_SCALE(morph));
}


OLC_MODULE(morphedit_movetype) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	MORPH_MOVE_TYPE(morph) = olc_process_type(ch, argument, "move type", "movetype", mob_move_types, MORPH_MOVE_TYPE(morph));
}


OLC_MODULE(morphedit_requiresobject) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	obj_vnum old = MORPH_REQUIRES_OBJ(morph);
	
	if (!str_cmp(argument, "none") || atoi(argument) == NOTHING) {
		MORPH_REQUIRES_OBJ(morph) = NOTHING;
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It no longer requires an object.\r\n");
		}
	}
	else {
		MORPH_REQUIRES_OBJ(morph) = olc_process_number(ch, argument, "object vnum", "requiresobject", 0, MAX_VNUM, MORPH_REQUIRES_OBJ(morph));
		if (!obj_proto(MORPH_REQUIRES_OBJ(morph))) {
			MORPH_REQUIRES_OBJ(morph) = old;
			msg_to_char(ch, "There is no object with that vnum. Old value restored.\r\n");
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It now requires %s.\r\n", get_obj_name_by_proto(MORPH_REQUIRES_OBJ(morph)));
		}
	}
}


OLC_MODULE(morphedit_shortdesc) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	olc_process_string(ch, argument, "short description", &MORPH_SHORT_DESC(morph));
}


OLC_MODULE(morphedit_size) {
	morph_data *morph = GET_OLC_MORPH(ch->desc);
	MORPH_SIZE(morph) = olc_process_type(ch, argument, "size", "size", size_types, MORPH_SIZE(morph));
}
