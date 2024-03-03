/* ************************************************************************
*   File: act.immortal.c                                  EmpireMUD 2.0b5 *
*  Usage: Player-level imm commands and other goodies                     *
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
#include "comm.h"
#include "vnums.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "olc.h"
#include "dg_scripts.h"
#include "dg_event.h"
#include "constants.h"

/**
* Contents:
*   Helpers
*   Displays
*   Admin Util System
*   Instance Management
*   Set Data And Functions
*   Stat / Vstat
*   Vnum Searches
*   Commands
*/

// external variables
extern bool manual_evolutions;

// external functions
struct instance_data *build_instance_loc(adv_data *adv, struct adventure_link_rule *rule, room_data *loc, int dir);
void do_stat_vehicle(char_data *ch, vehicle_data *veh, bool details);
room_data *find_location_for_rule(adv_data *adv, struct adventure_link_rule *rule, int *which_dir);

// locals
void instance_list_row(struct instance_data *inst, int number, char *save_buffer, size_t size);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Removes custom data from an island (as customized by empires).
*
* @param int island_id The island being decustomized.
*/
void decustomize_island(int island_id) {
	struct island_info *island = get_island(island_id, FALSE);
	
	struct empire_island *eisle;
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		HASH_FIND_INT(EMPIRE_ISLANDS(emp), &island_id, eisle);
		if (eisle && eisle->name) {
			log_to_empire(emp, ELOG_TERRITORY, "%s has lost its custom name and is now called %s", eisle->name, island ? island->name : "???");
			if (eisle->name) {
				free(eisle->name);
			}
			eisle->name = NULL;
			EMPIRE_NEEDS_SAVE(emp) = TRUE;
		}
	}
}


/**
* Approves a character.
*
* @param char_data *ch The character to approve.
*/
void perform_approve(char_data *ch) {
	if (config_get_bool("approve_per_character")) {
		SET_BIT(PLR_FLAGS(ch), PLR_APPROVED);
	}
	else {	// per-account (default)
		SET_BIT(GET_ACCOUNT(ch)->flags, ACCT_APPROVED);
		SAVE_ACCOUNT(GET_ACCOUNT(ch));
	}
	
	if (GET_ACCESS_LEVEL(ch) < LVL_MORTAL) {
		GET_ACCESS_LEVEL(ch) = LVL_MORTAL;
	}
	
	SAVE_CHAR(ch);
}


/**
* Rescinds approval for a character.
*
* @param char_data *ch The character to unapprove.
*/
void perform_unapprove(char_data *ch) {
	REMOVE_BIT(GET_ACCOUNT(ch)->flags, ACCT_APPROVED);
	SAVE_ACCOUNT(GET_ACCOUNT(ch));
	REMOVE_BIT(PLR_FLAGS(ch), PLR_APPROVED);
	SAVE_CHAR(ch);
}


/**
* Sends a poofout/poofin message, moves a character, and looks at the room.
*
* @param char_data *ch The person.
* @param room_data *to_room The target location.
*/
static void perform_goto(char_data *ch, room_data *to_room) {
	char_data *t;
	
	if (!ch || !to_room) {
		return;
	}
	
	if (!IS_NPC(ch) && POOFOUT(ch)) {
		if (!strstr(POOFOUT(ch), "$n"))
			sprintf(buf, "$n %s", POOFOUT(ch));
		else
			strcpy(buf, POOFOUT(ch));
	}
	else {
		strcpy(buf, "$n disappears in a puff of smoke.");
	}
	
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), t, next_in_room) {
		if (REAL_NPC(t) || t == ch) {
			continue;
		}
		if (!CAN_SEE(t, ch) || !WIZHIDE_OK(t, ch)) {
			continue;
		}

		act(buf, TRUE, ch, 0, t, TO_VICT | DG_NO_TRIG);
	}

	char_from_room(ch);
	char_to_room(ch, to_room);
	GET_LAST_DIR(ch) = NO_DIR;
	RESET_LAST_MESSAGED_TEMPERATURE(ch);

	if (!IS_NPC(ch) && POOFIN(ch)) {
		if (!strstr(POOFIN(ch), "$n"))
			sprintf(buf, "$n %s", POOFIN(ch));
		else
			strcpy(buf, POOFIN(ch));
	}
	else {
		strcpy(buf, "$n appears with an ear-splitting bang.");
	}
	
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), t, next_in_room) {
		if (REAL_NPC(t) || t == ch) {
			continue;
		}
		if (!CAN_SEE(t, ch) || !WIZHIDE_OK(t, ch)) {
			continue;
		}
		
		act(buf, TRUE, ch, 0, t, TO_VICT | DG_NO_TRIG);
	}
	
	qt_visit_room(ch, IN_ROOM(ch));
	look_at_room(ch);
	enter_triggers(ch, NO_DIR, "goto", FALSE);
	greet_triggers(ch, NO_DIR, "goto", FALSE);
	msdp_update_room(ch);	// once we're sure we're staying
}


/* Turns an immortal invisible */
void perform_immort_invis(char_data *ch, int level) {
	char_data *tch;

	if (IS_NPC(ch))
		return;
	
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), tch, next_in_room) {
		if (tch == ch)
			continue;
		if (GET_ACCESS_LEVEL(tch) >= GET_INVIS_LEV(ch) && GET_ACCESS_LEVEL(tch) < level)
			act("You blink and suddenly realize that $n is gone.", FALSE, ch, 0, tch, TO_VICT | DG_NO_TRIG);
		if (GET_ACCESS_LEVEL(tch) < GET_INVIS_LEV(ch) && GET_ACCESS_LEVEL(tch) >= level)
			act("You suddenly realize that $n is standing beside you.", FALSE, ch, 0, tch, TO_VICT | DG_NO_TRIG);
	}

	GET_INVIS_LEV(ch) = level;
	sprintf(buf, "Your invisibility level is %d.\r\n", level);
	send_to_char(buf, ch);
}


/* Immortal visible command, different than mortals' */
void perform_immort_vis(char_data *ch) {
	if (GET_INVIS_LEV(ch) == 0 && !AFF_FLAGGED(ch, AFF_HIDDEN) && !PRF_FLAGGED(ch, PRF_WIZHIDE | PRF_INCOGNITO)) {
		send_to_char("You are already fully visible.\r\n", ch);
		return;
	}
	
	REMOVE_BIT(PRF_FLAGS(ch), PRF_WIZHIDE | PRF_INCOGNITO);
	GET_INVIS_LEV(ch) = 0;
	appear(ch);
	send_to_char("You are now fully visible.\r\n", ch);
}


/* Stop a person from snooping (cannot be used on the government) */
void stop_snooping(char_data *ch) {
	if (!ch->desc->snooping)
		send_to_char("You aren't snooping anyone.\r\n", ch);
	else {
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "GC: %s has stopped snooping %s", GET_REAL_NAME(ch), GET_REAL_NAME(ch->desc->snooping->character));
		send_to_char("You stop snooping.\r\n", ch);
		ch->desc->snooping->snoop_by = NULL;
		ch->desc->snooping = NULL;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

// returns TRUE if the user sees the target, FALSE if not
bool users_output(char_data *to, char_data *tch, descriptor_data *d, char *name_search, int low, int high, int rp) {
	char_data *ch = tch;
	char levelname[20], *timeptr, idletime[10];
	char line[200], line2[220], state[30];
	const char *format = "%3d %s %-13s %-15s %-3s %-8s ";

	if (!ch && STATE(d) == CON_PLAYING) {
		if (d->original)
			ch = d->original;
		else if (!(ch = d->character))
			return FALSE;
	}

	if (ch) {
		if (*name_search && !is_abbrev(name_search, GET_NAME(ch)))
			return FALSE;
		if (!CAN_SEE_GLOBAL(to, ch) || GET_ACCESS_LEVEL(ch) < low || GET_ACCESS_LEVEL(ch) > high)
			return FALSE;
		if (rp && !PRF_FLAGGED(ch, PRF_RP))
			return FALSE;
		if (GET_INVIS_LEV(ch) > GET_ACCESS_LEVEL(to))
			return FALSE;

		sprintf(levelname, "%d", GET_ACCESS_LEVEL(ch));
	}
	else
		strcpy(levelname, "-");

	if (d) {
		timeptr = asctime(localtime(&d->login_time));
		timeptr += 11;
		*(timeptr + 8) = '\0';
	}
	else
		timeptr = str_dup("");

	if (ch && d && ch == d->original)
		strcpy(state, "Switched");
	else if (d)
		strcpy(state, connected_types[STATE(d)]);
	else
		strcpy(state, "Linkdead");

	if (ch)
		sprintf(idletime, "%3d", GET_IDLE_SECONDS(ch) / SECS_PER_REAL_MIN);
	else
		strcpy(idletime, "");

	sprintf(line, format, d ? d->desc_num : 0, levelname, (ch && GET_PC_NAME(ch)) ? GET_PC_NAME(ch) : (d && d->character && GET_PC_NAME(d->character)) ? GET_PC_NAME(d->character) : "UNDEFINED", state, idletime, timeptr);

	if (d && d->host && *d->host && (!ch || GET_ACCESS_LEVEL(ch) <= GET_ACCESS_LEVEL(to)))
		sprintf(line + strlen(line), "[%s]\r\n", d->host);
	else if (d)
		strcat(line, "[Hostname unknown]\r\n");
	else
		strcat(line, "\r\n");

	if (!d)
		sprintf(line2, "&c%s&0", line);
	else if (STATE(d) != CON_PLAYING)
		sprintf(line2, "&g%s&0", line);
	else
		strcpy(line2, line);
	strcpy(line, line2);

	send_to_char(line, to);
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// ADMIN UTIL SYSTEM ///////////////////////////////////////////////////////

#define ADMIN_UTIL(name)  void (name)(char_data *ch, char *argument)

ADMIN_UTIL(util_approval);
ADMIN_UTIL(util_bldconvert);
ADMIN_UTIL(util_clear_roles);
ADMIN_UTIL(util_diminish);
ADMIN_UTIL(util_evolve);
ADMIN_UTIL(util_exportcsv);
ADMIN_UTIL(util_islandsize);
ADMIN_UTIL(util_pathtest);
ADMIN_UTIL(util_playerdump);
ADMIN_UTIL(util_randtest);
ADMIN_UTIL(util_redo_islands);
ADMIN_UTIL(util_rescan);
ADMIN_UTIL(util_resetbuildingtriggers);
ADMIN_UTIL(util_strlen);
ADMIN_UTIL(util_temperature);
ADMIN_UTIL(util_tool);
ADMIN_UTIL(util_wipeprogress);
ADMIN_UTIL(util_yearly);


struct {
	char *command;
	int level;
	ADMIN_UTIL(*func);
} admin_utils[] = {
	{ "approval", LVL_CIMPL, util_approval },
	{ "bldconvert", LVL_CIMPL, util_bldconvert },
	{ "clearroles", LVL_CIMPL, util_clear_roles },
	{ "diminish", LVL_START_IMM, util_diminish },
	{ "evolve", LVL_CIMPL, util_evolve },
	{ "exportcsv", LVL_CIMPL, util_exportcsv },
	{ "islandsize", LVL_START_IMM, util_islandsize },
	{ "pathtest", LVL_START_IMM, util_pathtest },
	{ "playerdump", LVL_IMPL, util_playerdump },
	{ "randtest", LVL_CIMPL, util_randtest },
	{ "redoislands", LVL_CIMPL, util_redo_islands },
	{ "rescan", LVL_START_IMM, util_rescan },
	{ "resetbuildingtriggers", LVL_CIMPL, util_resetbuildingtriggers },
	{ "strlen", LVL_START_IMM, util_strlen },
	{ "temperature", LVL_START_IMM, util_temperature },
	{ "tool", LVL_IMPL, util_tool },
	{ "wipeprogress", LVL_CIMPL, util_wipeprogress },
	{ "yearly", LVL_CIMPL, util_yearly },

	// last
	{ "\n", LVL_TOP+1, NULL }
};


// secret implementor-only util for quick changes -- util tool
ADMIN_UTIL(util_tool) {
	// msg_to_char(ch, "Ok.\r\n");
	trig_data *trig, *next_trig;
	
	HASH_ITER(hh, trigger_table, trig, next_trig) {
		if (trig->attach_type == WLD_TRIGGER || trig->attach_type == RMT_TRIGGER || trig->attach_type == BLD_TRIGGER || trig->attach_type == ADV_TRIGGER) {
			if (IS_SET(GET_TRIG_TYPE(trig), WTRIG_RANDOM)) {
				msg_to_char(ch, "[%5d] %s\r\n", GET_TRIG_VNUM(trig), GET_TRIG_NAME(trig));
			}
		}
	}
}


/**
* This utility converts player approval to/from "by account" or "by character".
*/
ADMIN_UTIL(util_approval) {
	bool to_acc = FALSE, to_char = FALSE, save_acct = FALSE, loaded = FALSE;
	account_data *acct, *next_acct;
	struct account_player *plr;
	char_data *pers;
	
	skip_spaces(&argument);
	
	// optional "by " account/character
	if (!strn_cmp(argument, "by ", 3)) {
		argument += 3;
		skip_spaces(&argument);
	}
	if (is_abbrev(argument, "account")) {
		to_acc = TRUE;
	}
	else if (is_abbrev(argument, "character")) {
		to_char = TRUE;
	}
	else {
		msg_to_char(ch, "Usage: util approval by <character | account>\r\n");
		msg_to_char(ch, "This utility will change all existing approvals to be either by-whole-account or by-character.\r\n");
		return;
	}
	
	// check all accounts
	HASH_ITER(hh, account_table, acct, next_acct) {
		save_acct = FALSE;
		
		// check players on the account
		for (plr = acct->players; plr; plr = plr->next) {
			if (plr->player) {
				if (to_acc && IS_SET(plr->player->plr_flags, PLR_APPROVED) && !IS_SET(acct->flags, ACCT_APPROVED)) {
					// upgrade approval to account
					SET_BIT(acct->flags, ACCT_APPROVED);
					save_acct = TRUE;
					break;	// converting to account: only requires 1 approved alt
				}
				else if (to_char && IS_SET(acct->flags, ACCT_APPROVED) && !IS_SET(plr->player->plr_flags, PLR_APPROVED)) {
					// distribute account approval to characters
					if ((pers = find_or_load_player(plr->name, &loaded))) {
						SET_BIT(PLR_FLAGS(pers), PLR_APPROVED);
						if (loaded) {
							store_loaded_char(pers);
						}
						else {
							queue_delayed_update(pers, CDU_SAVE);
						}
					}
					// no break in this one -- need to do all alts
				}
			}
		}
		
		// shut off account-wide approval now?
		if (to_char && IS_SET(acct->flags, ACCT_APPROVED)) {
			REMOVE_BIT(acct->flags, ACCT_APPROVED);
			save_acct = TRUE;
		}
		
		// and save
		if (save_acct) {
			save_library_file_for_vnum(DB_BOOT_ACCT, acct->id);
		}
	}
	
	syslog(SYS_VALID, GET_INVIS_LEV(ch), TRUE, "VALID: %s changed existing approvals to be %s", GET_NAME(ch), to_acc ? "account-wide" : "by character, not account");
	msg_to_char(ch, "All approvals are now %s.\r\n", to_acc ? "account-wide" : "by character, not account");
}


// util bldconvert <building vnum> <start of new vnums>
ADMIN_UTIL(util_bldconvert) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH], *remaining_args;
	any_vnum from_vnum, start_to_vnum, to_vnum = NOTHING;
	craft_data *from_craft = NULL, *to_craft = NULL;
	bld_data *from_bld = NULL, *to_bld = NULL;
	struct trig_proto_list *proto_script;
	struct bld_relation *new_rel;
	vehicle_data *to_veh = NULL;
	int iter;
	
	// for searching
	struct interaction_item *inter, *next_inter;
	struct progress_perk *perk, *next_perk;
	vehicle_data *veh_iter, *next_veh;
	struct adventure_link_rule *link;
	quest_data *quest, *next_quest;
	bld_data *bld_iter, *next_bld;
	progress_data *prg, *next_prg;
	struct cmdlist_element *cmd;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	trig_data *trig, *next_trig;
	struct bld_relation *relat;
	adv_data *adv, *next_adv;
	obj_data *obj, *next_obj;
	bool any, add_force = FALSE;
	
	// stuff to copy
	bitvector_t functions = NOBITS, designate = NOBITS, room_affs = NOBITS;
	int fame = 0, military = 0, max_dam = 0, extra_rooms = 0;
	struct resource_data *regular_maintenance = NULL;
	struct interaction_item *interactions = NULL;
	struct extra_descr_data *ex_descs = NULL;
	struct bld_relation *relations = NULL;
	struct spawn_info *spawns = NULL;
	char *icon = NULL, *half_icon = NULL, *quarter_icon = NULL;
	
	// basic safety
	if (!ch->desc) {
		return;
	}
	
	remaining_args = two_arguments(argument, arg1, arg2);
	skip_spaces(&remaining_args);
	
	// basic args
	if (!*arg1 || !*arg2 || !isdigit(*arg1) || !isdigit(*arg2)) {
		msg_to_char(ch, "Usage: util bldconvert <building vnum> <start of new vnums>\r\n");
		msg_to_char(ch, "This will find the first free vnum after the <start>, without going over the current 100-vnum block.\r\n");
		return;
	}
	if ((from_vnum = atoi(arg1)) < 0 || !(from_bld = building_proto(from_vnum)) || BLD_FLAGGED(from_bld, BLD_ROOM)) {
		msg_to_char(ch, "Invalid building vnum '%s'.\r\n", arg1);
		return;
	}
	if (BLD_FLAGGED(from_bld, BLD_BARRIER | BLD_TWO_ENTRANCES)) {
		msg_to_char(ch, "Building %d %s cannot be converted because it has BARRIER or TWO-ENTRANCES.\r\n", from_vnum, GET_BLD_NAME(from_bld));
		return;
	}
	if ((start_to_vnum = atoi(arg2)) < 0) {
		msg_to_char(ch, "Invalid start-of-new-vnums number '%s'.\r\n", arg2);
		return;
	}
	
	// ensure there's a basic craft
	if ((from_craft = craft_proto(from_vnum)) && (!CRAFT_IS_BUILDING(from_craft) || GET_CRAFT_BUILD_TYPE(from_craft) != from_vnum)) {
		msg_to_char(ch, "Unable to convert: craft %d is for something other than that building.\r\n", from_vnum);
		return;
	}
	if (from_craft && CRAFT_FLAGGED(from_craft, CRAFT_DISMANTLE_ONLY)) {
		msg_to_char(ch, "Unable to convert: craft %d is already dismantle-only so this building has probably already been converted (remove dismantle-only to force this conversion).\r\n", from_vnum);
		return;
	}
	
	// find free vnum -- check the 100-vnum block starting with start_to_vnum
	for (iter = start_to_vnum; iter < 100 * (int)(start_to_vnum / 100 + 1) && to_vnum == NOTHING; ++iter) {
		if (vehicle_proto(iter)) {
			continue;	// vehicle vnum in use
		}
		if (from_craft && craft_proto(iter)) {
			continue;	// need a craft but this one is in-use
		}
		if (!BLD_FLAGGED(from_bld, BLD_OPEN) && building_proto(iter)) {
			continue;	// need a building but this one is in-use
		}
		
		// found!
		to_vnum = iter;
		break;
	}
	
	// did we find one?
	if (to_vnum == NOTHING) {
		msg_to_char(ch, "Unable to find a free vnum in the %d block after vnum %d.\r\n", 100 * (int)(start_to_vnum / 100), start_to_vnum);
		return;
	}
	
	// any additional args?
	if (!str_cmp(remaining_args, "force") || !str_cmp(remaining_args, "-force")) {
		add_force = TRUE;
	}
	else if (*remaining_args) {
		msg_to_char(ch, "Unknown argument(s): %s\r\n", remaining_args);
		return;
	}
	
	// check olc access
	if (!can_start_olc_edit(ch, OLC_VEHICLE, to_vnum) || (from_craft && !can_start_olc_edit(ch, OLC_CRAFT, to_vnum)) || (!BLD_FLAGGED(from_bld, BLD_OPEN) && !can_start_olc_edit(ch, OLC_BUILDING, to_vnum))) {
		return;	// sends own message
	}
	if (add_force && !can_start_olc_edit(ch, OLC_BUILDING, GET_BLD_VNUM(from_bld))) {
		return; // sends own message
	}
	
	msg_to_char(ch, "Creating new building-vehicle %d from building %d %s%s%s:\r\n", to_vnum, from_vnum, GET_BLD_NAME(from_bld), from_craft ? ", with craft" : "", !BLD_FLAGGED(from_bld, BLD_OPEN) ? ", with interior" : "");
	
	// this will use the character's olc access because it make saving much easier
	GET_OLC_VNUM(ch->desc) = to_vnum;
	
	// ok start with interior (non-open buildings only):
	if (!BLD_FLAGGED(from_bld, BLD_OPEN)) {
		// create the interior
		GET_OLC_TYPE(ch->desc) = OLC_BUILDING;
		GET_OLC_BUILDING(ch->desc) = to_bld = setup_olc_building(from_bld);
		GET_BLD_VNUM(to_bld) = to_vnum;
		
		// will move these to the vehicle
		icon = GET_BLD_ICON(to_bld);
		GET_BLD_ICON(to_bld) = NULL;
		half_icon = GET_BLD_HALF_ICON(to_bld);
		GET_BLD_HALF_ICON(to_bld) = NULL;
		quarter_icon = GET_BLD_QUARTER_ICON(to_bld);
		GET_BLD_QUARTER_ICON(to_bld) = NULL;
		fame = GET_BLD_FAME(to_bld);
		GET_BLD_FAME(to_bld) = 0;
		military = GET_BLD_MILITARY(to_bld);
		GET_BLD_MILITARY(to_bld) = 0;
		max_dam = GET_BLD_MAX_DAMAGE(to_bld);
		GET_BLD_MAX_DAMAGE(to_bld) = 1;
		extra_rooms = GET_BLD_EXTRA_ROOMS(to_bld);
		GET_BLD_EXTRA_ROOMS(to_bld) = 0;
		designate = GET_BLD_DESIGNATE_FLAGS(to_bld);
		GET_BLD_DESIGNATE_FLAGS(to_bld) = NOBITS;
		room_affs = GET_BLD_BASE_AFFECTS(to_bld);
		GET_BLD_BASE_AFFECTS(to_bld) = NOBITS;
		regular_maintenance = GET_BLD_REGULAR_MAINTENANCE(to_bld);
		GET_BLD_REGULAR_MAINTENANCE(to_bld) = NULL;
		
		// flags that change
		REMOVE_BIT(GET_BLD_FLAGS(to_bld), BLD_OPEN | BLD_CLOSED | BLD_TWO_ENTRANCES);
		SET_BIT(GET_BLD_FLAGS(to_bld), BLD_ROOM);
		
		// add a stores-like for the original bld
		CREATE(new_rel, struct bld_relation, 1);
		new_rel->type = BLD_REL_STORES_LIKE_BLD;
		new_rel->vnum = GET_BLD_VNUM(from_bld);
		LL_APPEND(GET_BLD_RELATIONS(to_bld), new_rel);
		
		// check for a ruins-to interaction to take
		LL_FOREACH_SAFE(GET_BLD_INTERACTIONS(to_bld), inter, next_inter) {
			if (inter->type == INTERACT_RUINS_TO_VEH) {
				// move veh ruins straight across
				LL_DELETE(GET_BLD_INTERACTIONS(to_bld), inter);
				LL_APPEND(interactions, inter);
			}
			else if (inter->type == INTERACT_RUINS_TO_BLD) {
				if (inter->vnum == 5006 || inter->vnum == 5007 || inter->vnum == 5010) {
					// safe to move (keep vnum, change to vehicle)
					inter->type = INTERACT_RUINS_TO_VEH;
					LL_DELETE(GET_BLD_INTERACTIONS(to_bld), inter);
					LL_APPEND(interactions, inter);
				}
				else {
					msg_to_char(ch, "- Building %d %s has RUINS-TO-BLD interactions that cannot be converted (removing instead).\r\n", from_vnum, GET_BLD_NAME(from_bld));
					LL_DELETE(GET_BLD_INTERACTIONS(to_bld), inter);
					free(inter);
				}
			}
		}
		
		LL_FOREACH(GET_BLD_SCRIPTS(from_bld), proto_script) {
			msg_to_char(ch, "- Building %d %s has trigger %d %s which was copied (to %d) but should be reviewed.\r\n", from_vnum, GET_BLD_NAME(from_bld), proto_script->vnum, (real_trigger(proto_script->vnum) ? GET_TRIG_NAME(real_trigger(proto_script->vnum)) : "UNKNOWN"), to_vnum);
		}
		
		// and save it
		save_olc_building(ch->desc);
		free_building(GET_OLC_BUILDING(ch->desc));
		GET_OLC_BUILDING(ch->desc) = NULL;
	}
	else {
		// open building: copy portions of it for the vehicle but don't create a new bld
		icon = GET_BLD_ICON(from_bld) ? GET_BLD_ICON(from_bld) : NULL;
		half_icon = GET_BLD_HALF_ICON(from_bld) ? GET_BLD_HALF_ICON(from_bld) : NULL;
		quarter_icon = GET_BLD_QUARTER_ICON(from_bld) ? GET_BLD_QUARTER_ICON(from_bld) : NULL;
		fame = GET_BLD_FAME(from_bld);
		military = GET_BLD_MILITARY(from_bld);
		functions = GET_BLD_FUNCTIONS(from_bld);
		max_dam = GET_BLD_MAX_DAMAGE(from_bld);
		ex_descs = copy_extra_descs(GET_BLD_EX_DESCS(from_bld));
		room_affs = GET_BLD_BASE_AFFECTS(from_bld);
		spawns = copy_spawn_list(GET_BLD_SPAWNS(from_bld));
		relations = copy_bld_relations(GET_BLD_RELATIONS(from_bld));
		interactions = copy_interaction_list(GET_BLD_INTERACTIONS(from_bld));
		regular_maintenance = copy_resource_list(GET_BLD_REGULAR_MAINTENANCE(from_bld));
		
		// validate ruins
		LL_FOREACH_SAFE(interactions, inter, next_inter) {
			if (inter->type == INTERACT_RUINS_TO_BLD) {
				if (inter->vnum == 5006 || inter->vnum == 5007 || inter->vnum == 5010) {
					// just change type (keep vnum)
					inter->type = INTERACT_RUINS_TO_VEH;
				}
				else {
					msg_to_char(ch, "- Building %d %s has RUINS-TO-BLD interactions that cannot be converted (removing instead).\r\n", from_vnum, GET_BLD_NAME(from_bld));
					LL_DELETE(interactions, inter);
					free(inter);
				}
			}
		}
		
		// add a stores-like for the original building
		CREATE(new_rel, struct bld_relation, 1);
		new_rel->type = BLD_REL_STORES_LIKE_BLD;
		new_rel->vnum = GET_BLD_VNUM(from_bld);
		LL_APPEND(relations, new_rel);
		
		if (!GET_BLD_DESC(from_bld)) {
			msg_to_char(ch, "- Open building %d %s has no description to copy.\r\n", from_vnum, GET_BLD_NAME(from_bld));
		}
		if (GET_BLD_ARTISAN(from_bld) > 0) {
			msg_to_char(ch, "- Open building %d %s has an artisan that cannot be copied (to %d).\r\n", from_vnum, GET_BLD_NAME(from_bld), to_vnum);
		}
		LL_FOREACH(GET_BLD_SCRIPTS(from_bld), proto_script) {
			msg_to_char(ch, "- Open building %d %s has trigger %d %s which cannot be copied (to %d).\r\n", from_vnum, GET_BLD_NAME(from_bld), proto_script->vnum, (real_trigger(proto_script->vnum) ? GET_TRIG_NAME(real_trigger(proto_script->vnum)) : "UNKNOWN"), to_vnum);
		}
	}
	
	// set up the vehicle
	{
		GET_OLC_TYPE(ch->desc) = OLC_VEHICLE;
		GET_OLC_VEHICLE(ch->desc) = to_veh = setup_olc_vehicle(NULL);
		VEH_VNUM(to_veh) = to_vnum;
		
		// copy kws
		set_vehicle_keywords(to_veh, GET_BLD_NAME(from_bld));
		strtolower(VEH_KEYWORDS(to_veh));
		
		// build short desc
		safe_snprintf(buf, sizeof(buf), "%s %s", AN(GET_BLD_NAME(from_bld)), GET_BLD_NAME(from_bld));
		strtolower(buf);
		set_vehicle_short_desc(to_veh, buf);
		
		// build long desc
		safe_snprintf(buf, sizeof(buf), "%s %s is here.", AN(GET_BLD_NAME(from_bld)), GET_BLD_NAME(from_bld));
		strtolower(buf);
		set_vehicle_long_desc(to_veh, CAP(buf));
		
		// look desc?
		set_vehicle_look_desc(to_veh, GET_BLD_DESC(from_bld), FALSE);
		
		// icon?
		set_vehicle_icon(to_veh, icon);
		if (half_icon) {
			set_vehicle_half_icon(to_veh, half_icon);
		}
		if (quarter_icon) {
			set_vehicle_quarter_icon(to_veh, quarter_icon);
		}
		
		// basic traits
		VEH_SIZE(to_veh) = 1;
		VEH_INTERIOR_ROOM_VNUM(to_veh) = to_bld ? to_vnum : NOWHERE;
		
		// flags
		set_vehicle_flags(to_veh, VEH_CUSTOMIZABLE | VEH_NO_BUILDING | VEH_NO_LOAD_ONTO_VEHICLE | VEH_VISIBLE_IN_DARK | VEH_BUILDING);
		if (!BLD_FLAGGED(from_bld, BLD_OPEN)) {
			set_vehicle_flags(to_veh, VEH_IN);
		}
		if (BLD_FLAGGED(from_bld, BLD_BURNABLE)) {
			set_vehicle_flags(to_veh, VEH_BURNABLE);
		}
		if (BLD_FLAGGED(from_bld, BLD_INTERLINK)) {
			set_vehicle_flags(to_veh, VEH_INTERLINK);
		}
		if (BLD_FLAGGED(from_bld, BLD_ALLOW_MOUNTS | BLD_HERD)) {
			set_vehicle_flags(to_veh, VEH_CARRY_MOBS);
		}
		if (BLD_FLAGGED(from_bld, BLD_ALLOW_MOUNTS)) {
			set_vehicle_flags(to_veh, VEH_CARRY_VEHICLES);
		}
		if (BLD_FLAGGED(from_bld, BLD_IS_RUINS)) {
			set_vehicle_flags(to_veh, VEH_IS_RUINS);
		}
		if (BLD_FLAGGED(from_bld, BLD_NO_PAINT)) {
			set_vehicle_flags(to_veh, VEH_NO_PAINT);
		}
		if (BLD_FLAGGED(from_bld, BLD_DEDICATE)) {
			set_vehicle_flags(to_veh, VEH_DEDICATE);
		}
		if (IS_SET(room_affs, ROOM_AFF_CHAMELEON)) {
			REMOVE_BIT(room_affs, ROOM_AFF_CHAMELEON);
			set_vehicle_flags(to_veh, VEH_CHAMELEON);
		}
		
		// copy traits over
		VEH_DESIGNATE_FLAGS(to_veh) = designate;
		VEH_EX_DESCS(to_veh) = ex_descs;
		VEH_FAME(to_veh) = fame;
		VEH_FUNCTIONS(to_veh) = functions;
		VEH_INTERACTIONS(to_veh) = interactions;
		VEH_MAX_HEALTH(to_veh) = max_dam;
		VEH_MAX_ROOMS(to_veh) = extra_rooms;
		VEH_MILITARY(to_veh) = military;
		VEH_RELATIONS(to_veh) = relations;
		VEH_ROOM_AFFECTS(to_veh) = room_affs | ROOM_AFF_NO_WORKFORCE_EVOS;
		VEH_SPAWNS(to_veh) = spawns;
		VEH_REGULAR_MAINTENANCE(to_veh) = regular_maintenance;
		
		if (room_affs) {
			sprintbit(room_affs, room_aff_bits, buf, TRUE);
			msg_to_char(ch, "- Vehicle %d %s has affect flags: %s\r\n", to_vnum, GET_BLD_NAME(from_bld), buf);
		}
		if (BLD_FLAGGED(from_bld, BLD_LARGE_CITY_RADIUS)) {
			msg_to_char(ch, "- Building %d %s has LARGE-CITY-RADIUS flag that cannot be copied.\r\n", to_vnum, GET_BLD_NAME(from_bld));
		}
		
		// and save it
		save_olc_vehicle(ch->desc);
		free_vehicle(GET_OLC_VEHICLE(ch->desc));
		GET_OLC_VEHICLE(ch->desc) = NULL;
	}
	
	// add new craft
	if (from_craft) {
		GET_OLC_TYPE(ch->desc) = OLC_CRAFT;
		GET_OLC_CRAFT(ch->desc) = to_craft = setup_olc_craft(from_craft);
		GET_CRAFT_VNUM(to_craft) = to_vnum;
		
		SET_BIT(GET_CRAFT_FLAGS(to_craft), CRAFT_VEHICLE);
		GET_CRAFT_QUANTITY(to_craft) = 1;
		GET_CRAFT_OBJECT(to_craft) = to_vnum;
		GET_CRAFT_BUILD_TYPE(to_craft) = NOTHING;
		GET_CRAFT_TIME(to_craft) = 1;
		
		// only keep facing flags if certain ones are set
		if (!IS_SET(GET_CRAFT_BUILD_FACING(to_craft), BLD_ON_MOUNTAIN | BLD_ON_RIVER | BLD_ON_NOT_PLAYER_MADE | BLD_ON_OCEAN | BLD_ON_OASIS | BLD_ON_SWAMP | BLD_ON_SHALLOW_SEA | BLD_ON_COAST | BLD_ON_RIVERBANK | BLD_ON_ESTUARY | BLD_ON_LAKE | BLD_ON_WATER)) {
			GET_CRAFT_BUILD_FACING(to_craft) = NOBITS;
		}
		else if (GET_CRAFT_BUILD_FACING(to_craft) == (BLD_ON_MOUNTAIN | BLD_ON_FLAT_TERRAIN | BLD_FACING_OPEN_BUILDING)) {
			// special case for common mountain-facing-mountain buildings
			GET_CRAFT_BUILD_FACING(to_craft) = NOBITS;
		}
		else if (GET_CRAFT_BUILD_FACING(to_craft) == (BLD_ON_SWAMP | BLD_ON_FLAT_TERRAIN | BLD_FACING_OPEN_BUILDING)) {
			// special case for common swamp-facing-swamp buildings
			GET_CRAFT_BUILD_FACING(to_craft) = NOBITS;
		}
		else {
			msg_to_char(ch, "- Craft for building %d %s needs build-facing review.\r\n", to_vnum, GET_BLD_NAME(from_bld));
		}
		
		// and save it
		save_olc_craft(ch->desc);
		free_craft(GET_OLC_CRAFT(ch->desc));
		GET_OLC_CRAFT(ch->desc) = NULL;
		
		if (CRAFT_FLAGGED(from_craft, CRAFT_LEARNED)) {
			msg_to_char(ch, "- Craft for building %d %s is LEARNED - you must update whatever taught craft %d.\r\n", to_vnum, GET_BLD_NAME(from_bld), from_vnum);
		}
	}
	else {
		msg_to_char(ch, "- Building %d %s has no craft.\r\n", to_vnum, GET_BLD_NAME(from_bld));
	}
	
	// convert old craft
	if (from_craft) {
		GET_OLC_TYPE(ch->desc) = OLC_CRAFT;
		GET_OLC_CRAFT(ch->desc) = setup_olc_craft(from_craft);
		GET_OLC_VNUM(ch->desc) = from_vnum;
		
		safe_snprintf(buf, sizeof(buf), "dismantle %s", GET_CRAFT_NAME(GET_OLC_CRAFT(ch->desc)));
		free(GET_CRAFT_NAME(GET_OLC_CRAFT(ch->desc)));
		GET_CRAFT_NAME(GET_OLC_CRAFT(ch->desc)) = str_dup(buf);
		
		SET_BIT(GET_CRAFT_FLAGS(GET_OLC_CRAFT(ch->desc)), CRAFT_DISMANTLE_ONLY);
		
		// and save it
		save_olc_craft(ch->desc);
		free_craft(GET_OLC_CRAFT(ch->desc));
		GET_OLC_CRAFT(ch->desc) = NULL;
	}
	
	// force-upgrade?
	if (add_force) {
		GET_OLC_TYPE(ch->desc) = OLC_BUILDING;
		GET_OLC_BUILDING(ch->desc) = setup_olc_building(from_bld);
		GET_OLC_VNUM(ch->desc) = from_vnum;
		
		CREATE(new_rel, struct bld_relation, 1);
		new_rel->type = BLD_REL_FORCE_UPGRADE_VEH;
		new_rel->vnum = to_vnum;
		LL_APPEND(GET_BLD_RELATIONS(GET_OLC_BUILDING(ch->desc)), new_rel);
		
		// and save it
		save_olc_building(ch->desc);
		free_building(GET_OLC_BUILDING(ch->desc));
		GET_OLC_BUILDING(ch->desc) = NULL;
	}
	
	// and clean up
	GET_OLC_TYPE(ch->desc) = 0;
	GET_OLC_VNUM(ch->desc) = NOTHING;
	
	// now warn about things linked to it (similar to .b search
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		for (link = GET_ADV_LINKING(adv); link; link = link->next) {
			if (link->type == ADV_LINK_BUILDING_EXISTING || link->type == ADV_LINK_BUILDING_NEW || link->type == ADV_LINK_PORTAL_BUILDING_EXISTING || link->type == ADV_LINK_PORTAL_BUILDING_NEW) {
				if (link->value == from_vnum) {
					msg_to_char(ch, "- Building %d %s is linked by ADV [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
					break;
				}
			}
		}
	}
	HASH_ITER(hh, building_table, bld_iter, next_bld) {
		if (GET_BLD_VNUM(bld_iter) == to_vnum) {
			continue;	// skip self
		}
		LL_FOREACH(GET_BLD_INTERACTIONS(bld_iter), inter) {
			if (interact_data[inter->type].vnum_type == TYPE_BLD && inter->vnum == from_vnum) {
				msg_to_char(ch, "- Building %d %s in interactions for BLD [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), GET_BLD_VNUM(bld_iter), GET_BLD_NAME(bld_iter));
				break;
			}
		}
		LL_FOREACH(GET_BLD_RELATIONS(bld_iter), relat) {
			if (bld_relationship_vnum_types[relat->type] != TYPE_BLD) {
				continue;
			}
			if (relat->vnum != from_vnum) {
				continue;
			}
			msg_to_char(ch, "- Building %d %s in relations for BLD [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), GET_BLD_VNUM(bld_iter), GET_BLD_NAME(bld_iter));
			break;
		}
	}
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (IS_RECIPE(obj) && GET_RECIPE_VNUM(obj) == from_vnum) {
			msg_to_char(ch, "- Craft %d %s in taught by recipe OBJ [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
		}
	}
	HASH_ITER(hh, progress_table, prg, next_prg) {
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_OWN_BUILDING, from_vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_VISIT_BUILDING, from_vnum);
		
		if (any) {
			msg_to_char(ch, "- Building %d %s in tasks for PRG [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), PRG_VNUM(prg), PRG_NAME(prg));
		}
		
		LL_FOREACH_SAFE(PRG_PERKS(prg), perk, next_perk) {
			if (perk->type == PRG_PERK_CRAFT && perk->value == from_vnum) {
				msg_to_char(ch, "- Craft %d %s in taught by PRG [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), PRG_VNUM(prg), PRG_NAME(prg));
				break;
			}
		}
	}
	HASH_ITER(hh, quest_table, quest, next_quest) {
		if (find_quest_giver_in_list(QUEST_STARTS_AT(quest), QG_BUILDING, from_vnum) || find_quest_giver_in_list(QUEST_ENDS_AT(quest), QG_BUILDING, from_vnum) || find_requirement_in_list(QUEST_TASKS(quest), REQ_OWN_BUILDING, from_vnum) || find_requirement_in_list(QUEST_PREREQS(quest), REQ_OWN_BUILDING, from_vnum) || find_requirement_in_list(QUEST_TASKS(quest), REQ_VISIT_BUILDING, from_vnum) || find_requirement_in_list(QUEST_PREREQS(quest), REQ_VISIT_BUILDING, from_vnum)) {
			msg_to_char(ch, "- Building %d %s in data for QST [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	HASH_ITER(hh, shop_table, shop, next_shop) {
		if (find_quest_giver_in_list(SHOP_LOCATIONS(shop), QG_BUILDING, from_vnum)) {
			msg_to_char(ch, "- Building %d %s is location for SHOP [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), SHOP_VNUM(shop), SHOP_NAME(shop));
		}
	}
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_OWN_BUILDING, from_vnum) || find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_VISIT_BUILDING, from_vnum)) {
			msg_to_char(ch, "- Building %d %s in requirements for SOC [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	safe_snprintf(buf, sizeof(buf), "%d", from_vnum);
	HASH_ITER(hh, trigger_table, trig, next_trig) {
		LL_FOREACH(trig->cmdlist, cmd) {
			if (strstr(cmd->cmd, buf)) {
				msg_to_char(ch, "- Possible mention of building or craft %d %s in TRG [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), GET_TRIG_VNUM(trig), GET_TRIG_NAME(trig));
				break;
			}
		}
	}
	HASH_ITER(hh, vehicle_table, veh_iter, next_veh) {
		if (VEH_VNUM(veh_iter) == to_vnum) {
			continue;	// skip new one
		}
		if (VEH_INTERIOR_ROOM_VNUM(veh_iter) == from_vnum) {
			msg_to_char(ch, "- Building %d %s in interior room for VEH [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), VEH_VNUM(veh_iter), VEH_SHORT_DESC(veh_iter));
		}
		LL_FOREACH(VEH_INTERACTIONS(veh_iter), inter) {
			if (interact_data[inter->type].vnum_type == TYPE_BLD && inter->vnum == from_vnum) {
				msg_to_char(ch, "- Building %d %s in interaction for VEH [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), VEH_VNUM(veh_iter), VEH_SHORT_DESC(veh_iter));
				break;
			}
		}
		LL_FOREACH(VEH_RELATIONS(veh_iter), relat) {
			if (bld_relationship_vnum_types[relat->type] != TYPE_BLD) {
				continue;
			}
			if (relat->vnum != from_vnum) {
				continue;
			}
			msg_to_char(ch, "- Building %d %s in relations VEH [%d %s].\r\n", to_vnum, GET_BLD_NAME(from_bld), VEH_VNUM(veh_iter), VEH_SHORT_DESC(veh_iter));
			break;
		}
	}
	
	safe_snprintf(buf, sizeof(buf), "OLC: %s has cloned building %d %s to vehicle %d", GET_NAME(ch), from_vnum, GET_BLD_NAME(from_bld), to_vnum);
	if (!BLD_FLAGGED(from_bld, BLD_OPEN)) {
		safe_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ", interior room bld %d", to_vnum);
	}
	if (from_craft) {
		safe_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ", and craft %d (with updates to craft %d)", to_vnum, from_vnum);
	}
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "%s", buf);
}


// for util_clear_roles
PLAYER_UPDATE_FUNC(update_clear_roles) {
	if (IS_IMMORTAL(ch)) {
		return;
	}
	
	GET_CLASS_ROLE(ch) = ROLE_NONE;
	assign_class_and_extra_abilities(ch, NULL, NOTHING);
	
	if (!is_file) {
		msg_to_char(ch, "Your group role has been reset.\r\n");
	}
}


ADMIN_UTIL(util_clear_roles) {
	update_all_players(ch, update_clear_roles);
	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has cleared all player roles", GET_NAME(ch));
	msg_to_char(ch, "Ok.\r\n");
}


ADMIN_UTIL(util_diminish) {
	double number, scale, result;
	
	half_chop(argument, buf1, buf2);
	
	if (!*buf1 || !*buf2) {
		msg_to_char(ch, "Usage: diminish <number> <scale>\r\n");
	}
	else if (!isdigit(*buf1) && *buf1 != '.') {
		msg_to_char(ch, "%s is not a valid floating point number.\r\n", buf1);
	}
	else if (!isdigit(*buf2) && *buf2 != '.') {
		msg_to_char(ch, "%s is not a valid floating point number.\r\n", buf2);
	}
	else {
		number = atof(buf1);
		scale = atof(buf2);
		result = diminishing_returns(number, scale);
		
		msg_to_char(ch, "Diminished value: %.2f\r\n", result);
	}
}


ADMIN_UTIL(util_evolve) {
	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s used util evolve", GET_NAME(ch));
	send_config_msg(ch, "ok_string");
	manual_evolutions = TRUE;	// triggers a log
	run_external_evolutions();
}


ADMIN_UTIL(util_exportcsv) {
	char str1[MAX_STRING_LENGTH], str2[MAX_STRING_LENGTH];
	struct trig_proto_list *trig;
	obj_data *obj, *next_obj;
	struct obj_apply *apply;
	adv_data *adv;
	int found;
	FILE *fl;
	
	if (is_abbrev(argument, "equipment")) {
		msg_to_char(ch, "Exporting equipment CSV to lib/equipment.csv...\r\n");
		if (!(fl = fopen("equipment.csv", "w"))) {
			msg_to_char(ch, "Failed to open file.\r\n");
			return;
		}
		
		// header
		fprintf(fl, "Adventure,Vnum,Name,Min,Max,");
		fprintf(fl, "Wear,Type,Flags,Attack,");
		fprintf(fl, "Applies,Affects,Triggers");
		
		HASH_ITER(hh, object_table, obj, next_obj) {
			if (!CAN_WEAR(obj, ~ITEM_WEAR_TAKE)) {
				continue;	// wearable only
			}
			
			adv = get_adventure_for_vnum(GET_OBJ_VNUM(obj));	// if any
			
			// (leading \n) adv, vnum, name, min, max
			fprintf(fl, "\n\"%s\",%d,\"%s\",%d,%d,", (adv ? GET_ADV_NAME(adv) : ""), GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj), GET_OBJ_MIN_SCALE_LEVEL(obj), GET_OBJ_MAX_SCALE_LEVEL(obj));
			
			// wear, type, flags, attack
			sprintbit(GET_OBJ_WEAR(obj) & ~ITEM_WEAR_TAKE, wear_bits, str1, TRUE);
			sprintbit(GET_OBJ_EXTRA(obj), extra_bits, str2, TRUE);
			fprintf(fl, "%s,%s,%s,%s,", str1, item_types[GET_OBJ_TYPE(obj)], str2, IS_WEAPON(obj) ? get_attack_name_by_vnum(GET_WEAPON_TYPE(obj)) : "");
			
			// applies, affects, triggers
			fprintf(fl, "\"");	// leading quote for applies
			found = 0;
			LL_FOREACH(GET_OBJ_APPLIES(obj), apply) {
				if (apply->apply_type != APPLY_TYPE_NATURAL) {
					sprintf(str2, " (%s)", apply_type_names[(int)apply->apply_type]);
				}
				else {
					*str2 = '\0';
				}
				fprintf(fl, "%s%+d to %s%s", found++ ? ", " : "", apply->modifier, apply_types[(int) apply->location], str2);
			}
			
			sprintbit(GET_OBJ_AFF_FLAGS(obj), affected_bits, str1, TRUE);
			fprintf(fl, "\",%s,\"", str1);	// including quotes for applies and triggers
			
			found = 0;
			LL_FOREACH(obj->proto_script, trig) {
				fprintf(fl, "%s%d", found++ ? ", " : "", trig->vnum);
			}
			
			fprintf(fl, "\"");	// trailing " for trigs	
			
			// no trailing \n
		}
		
		fclose(fl);
	}
	else {
		msg_to_char(ch, "Export options:\r\n");
		msg_to_char(ch, "  equipment - All equippable items.\r\n");
	}
}


// util_islandsize: helper type
struct isf_type {
	int island;
	int count;
	UT_hash_handle hh;
};
int sort_isf_list(struct isf_type *a, struct isf_type *b) {
	return a->island - b->island;
}

ADMIN_UTIL(util_islandsize) {
	struct isf_type *isf, *next_isf, *list = NULL;
	room_data *room, *next_room;
	int isle;
	
	HASH_ITER(hh, world_table, room, next_room) {
		if (GET_ROOM_VNUM(room) < MAP_SIZE) {
			isle = GET_ISLAND_ID(room);
			HASH_FIND_INT(list, &isle, isf);
			if (!isf) {
				CREATE(isf, struct isf_type, 1);
				isf->island = isle;
				isf->count = 0;
				HASH_ADD_INT(list, island, isf);
			}
			
			isf->count += 1;
		}
	}
	
	HASH_SORT(list, sort_isf_list);
	
	build_page_display_str(ch, "Island sizes:");
	HASH_ITER(hh, list, isf, next_isf) {
		build_page_display(ch, "%2d: %d tile%s", isf->island, isf->count, PLURAL(isf->count));
		
		// free as we go
		HASH_DEL(list, isf);
		free(isf);
	}
	
	send_page_display(ch);
}


ADMIN_UTIL(util_pathtest) {
	unsigned long long timer;
	PATHFIND_VALIDATOR(*vdr);
	room_data *to_room;
	char *path;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	// find validator
	if (*argument && is_abbrev(argument, "roads")) {
		vdr = pathfind_road;
	}
	else if (*argument && is_abbrev(argument, "ocean")) {
		vdr = pathfind_ocean;
	}
	else if (*argument && is_abbrev(argument, "pilot")) {
		vdr = pathfind_pilot;
	}
	else {
		msg_to_char(ch, "Usage: util pathtest <room> <ocean | road | pilot>\r\n");
		return;
	}
	
	timer = microtime();
	
	if (!*arg || !*argument) {
		msg_to_char(ch, "Usage: util pathtest <room> <ocean | road | pilot>\r\n");
	}
	else if (!(to_room = find_target_room(ch, arg))) {
		msg_to_char(ch, "Unknown target: %s\r\n", arg);
	}
	else if (!(path = get_pathfind_string(IN_ROOM(ch), to_room, ch, NULL, vdr))) {
		msg_to_char(ch, "Unable to find a valid path there.\r\n");
	}
	else {
		msg_to_char(ch, "Path found: %s\r\n", path);
		msg_to_char(ch, "Time: %f seconds\r\n", (microtime()-timer) / 1000000.0);
	}
}


ADMIN_UTIL(util_playerdump) {
	player_index_data *index, *next_index;
	char_data *plr;
	bool is_file;
	FILE *fl;
	
	if (!(fl = fopen("plrdump.txt", "w"))) {
		msg_to_char(ch, "Unable to open file for writing.\r\n");
		return;
	}
	
	fprintf(fl, "name\taccount\thours\thost\n");
	
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {		
		if (!(plr = find_or_load_player(index->name, &is_file))) {
			continue;
		}
		
		fprintf(fl, "%s\t%d\t%d\t%s\n", GET_NAME(plr), GET_ACCOUNT(plr)->id, (plr->player.time.played / SECS_PER_REAL_HOUR), plr->prev_host);
		
		// done
		if (is_file && plr) {
			free_char(plr);
			is_file = FALSE;
			plr = NULL;
		}
	}
	
	fclose(fl);
	msg_to_char(ch, "Ok.\r\n");
}


ADMIN_UTIL(util_randtest) {
	const int default_num = 1000, default_size = 100;
	
	int *values;
	int iter;
	int num, size, total, under, over;
	double avg;
	
	half_chop(argument, buf1, buf2);
	
	if (*buf1) {
		if (isdigit(*buf1)) {
			num = atoi(buf1);
		}
		else {
			msg_to_char(ch, "Usage: randtest [number of values] [size of values]\r\n");
			return;
		}
	}
	else {
		num = default_num;
	}
	
	if (*buf2) {
		if (isdigit(*buf2)) {
			size = atoi(buf2);
		}
		else {
			msg_to_char(ch, "Usage: randtest [number of values] [size of values]\r\n");
			return;
		}
	}
	else {
		size = default_size;
	}
	
	if (num > 10000 || num < 0 || size < 1) {
		msg_to_char(ch, "Invalid parameters.\r\n");
		return;
	}
	
	CREATE(values, int, num);
	total = 0;
	
	for (iter = 0; iter < num; ++iter) {
		values[iter] = number(1, size);
		total += values[iter];
	}
	
	avg = (double) total / num;
	
	under = over = 0;
	for (iter = 0; iter < num; ++iter) {
		if (values[iter] < (int) avg) {
			++under;
		}
		else {
			++over;
		}
	}
	
	msg_to_char(ch, "Generated %d numbers of size %d:\r\n", num, size);
	msg_to_char(ch, "Average result: %.2f\r\n", avg);
	msg_to_char(ch, "Results below average: %d\r\n", under);
	msg_to_char(ch, "Results above average: %d\r\n", over);
	
	free(values);
}


ADMIN_UTIL(util_redo_islands) {
	skip_spaces(&argument);
	
	if (!*argument || str_cmp(argument, "confirm")) {
		msg_to_char(ch, "WARNING: This command will re-number all islands and may make some einvs unreachable.\r\n");
		msg_to_char(ch, "Usage: util redoislands confirm\r\n");
	}
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has renumbered islands", GET_NAME(ch));
		number_and_count_islands(TRUE);
		msg_to_char(ch, "Islands renumbered. Caution: empire inventories may now be in the wrong place.\r\n");
	}
}


ADMIN_UTIL(util_rescan) {
	empire_data *emp;
	
	if (GET_ACCESS_LEVEL(ch) < LVL_CIMPL && !IS_GRANTED(ch, GRANT_EMPIRES)) {
		msg_to_char(ch, "You don't have permission to rescan empires.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Usage: rescan <empire | all>\r\n");
	}
	else if (!str_cmp(argument, "all")) {
		syslog(SYS_INFO, GET_INVIS_LEV(ch), TRUE, "Rescanning all empires");
		reread_empire_tech(NULL);
		refresh_empire_dropped_items(NULL);
		send_config_msg(ch, "ok_string");
	}
	else if (!(emp = get_empire_by_name(argument))) {
		msg_to_char(ch, "Unknown empire.\r\n");
	}
	else {
		syslog(SYS_INFO, GET_INVIS_LEV(ch), TRUE, "Rescanning empire: %s", EMPIRE_NAME(emp));
		reread_empire_tech(emp);
		refresh_empire_goals(emp, NOTHING);
		refresh_empire_dropped_items(emp);
		check_ruined_cities(emp);
		send_config_msg(ch, "ok_string");
	}
}


ADMIN_UTIL(util_resetbuildingtriggers) {
	room_data *room, *next_room;
	bld_data *proto = NULL;
	bool all = FALSE;
	int count = 0;
	
	all = !str_cmp(argument, "all");
	
	if (!all && (!*argument || !isdigit(*argument))) {
		msg_to_char(ch, "Usage: resetbuildingtriggers <vnum | all>\r\n");
	}
	else if (!all && !(proto = building_proto(atoi(argument)))) {
		msg_to_char(ch, "Invalid building/room vnum '%s'.\r\n", argument);
	}
	else {
		HASH_ITER(hh, world_table, room, next_room) {
			if (!GET_BUILDING(room)) {
				continue;
			}
			if (!all && GET_BUILDING(room) != proto) {
				continue;
			}
			
			// remove old triggers
			remove_all_triggers(room, WLD_TRIGGER);
			free_proto_scripts(&room->proto_script);
			
			// add any triggers
			room->proto_script = copy_trig_protos(GET_BLD_SCRIPTS(GET_BUILDING(room)));
			assign_triggers(room, WLD_TRIGGER);
			
			++count;
		}
		
		msg_to_char(ch, "%d building%s/room%s updated.\r\n", count, PLURAL(count), PLURAL(count));
	}
}


ADMIN_UTIL(util_strlen) {
	msg_to_char(ch, "String: %s\r\n", argument);
	msg_to_char(ch, "Raw: %s\r\n", show_color_codes(argument));
	msg_to_char(ch, "strlen: %d\r\n", (int)strlen(argument));
	msg_to_char(ch, "color_strlen: %d\r\n", (int)color_strlen(argument));
	msg_to_char(ch, "color_code_length: %d\r\n", color_code_length(argument));
}


ADMIN_UTIL(util_temperature) {
	int calc_temp, climate_val, season_count, season_val, sun_count, sun_val, bit, find;
	int use_sun = get_sun_status(IN_ROOM(ch)), use_season = GET_SEASON(IN_ROOM(ch));
	double season_mod, sun_mod, temperature, cold_mod, heat_mod;
	bitvector_t climates;
	char cold_mod_part[256], heat_mod_part[256], season_part[256], sun_part[256], arg[MAX_INPUT_LENGTH];
	
	argument = one_argument(argument, arg);
	while (*arg) {
		if ((find = search_block(arg, sun_types, FALSE)) != NOTHING) {
			use_sun = find;
		}
		else if ((find = search_block(arg, icon_types, FALSE)) != NOTHING && find != TILESET_ANY) {
			use_season = find;
		}
		else {
			msg_to_char(ch, "Invalid argument: %s\r\n", arg);
			return;
		}
		
		// repeat
		argument = one_argument(argument, arg);
	}
	
	msg_to_char(ch, "Temperature information for this room:\r\n");
	
	// init
	climate_val = 0;
	season_val = season_temperature[use_season];
	sun_val = sun_temperature[use_sun];
	season_count = sun_count = 0;
	season_mod = sun_mod = 0.0;
	cold_mod = heat_mod = 1.0;
	climates = get_climate(IN_ROOM(ch));
	
	msg_to_char(ch, "Base season value: %+d (%s)\r\n", season_val, icon_types[use_season]);
	msg_to_char(ch, "Base sun value: %+d (%s)\r\n", sun_val, sun_types[use_sun]);
	
	// determine climates
	for (bit = 0; climates; ++bit, climates >>= 1) {
		if (IS_SET(climates, BIT(0))) {
			climate_val += climate_temperature[bit].base_add;
			
			if (climate_temperature[bit].season_weight != NO_TEMP_MOD) {
				season_mod += climate_temperature[bit].season_weight;
				++season_count;
				safe_snprintf(season_part, sizeof(season_part), "%.1f%%", 100.0 * climate_temperature[bit].season_weight);
			}
			else {
				strcpy(season_part, "no mod for");
			}
			if (climate_temperature[bit].sun_weight != NO_TEMP_MOD) {
				sun_mod += climate_temperature[bit].sun_weight;
				++sun_count;
				safe_snprintf(sun_part, sizeof(sun_part), "%.1f%%", 100.0 * climate_temperature[bit].sun_weight);
			}
			else {
				strcpy(sun_part, "no mod for");
			}
			
			if (climate_temperature[bit].cold_modifier != NO_TEMP_MOD && climate_temperature[bit].cold_modifier != 1.0) {
				cold_mod *= climate_temperature[bit].cold_modifier;
				safe_snprintf(cold_mod_part, sizeof(cold_mod_part), ", %.2f cold modifier", climate_temperature[bit].cold_modifier);
			}
			else {
				*cold_mod_part = '\0';
			}
			if (climate_temperature[bit].heat_modifier != NO_TEMP_MOD && climate_temperature[bit].heat_modifier != 1.0) {
				heat_mod *= climate_temperature[bit].heat_modifier;
				safe_snprintf(heat_mod_part, sizeof(heat_mod_part), ", %.2f heat modifier", climate_temperature[bit].heat_modifier);
			}
			else {
				*heat_mod_part = '\0';
			}
			
			msg_to_char(ch, "%s: %+d, %s sun, %s seasonal%s%s\r\n", climate_flags[bit], climate_temperature[bit].base_add, sun_part, season_part, cold_mod_part, heat_mod_part);
		}
	}
	
	// final math
	temperature = climate_val;
	
	if (season_count > 0) {
		season_mod /= season_count;
		temperature += season_val * season_mod;
	}
	else {
		temperature += sun_val;
	}
	
	if (sun_count > 0) {
		sun_mod /= sun_count;
		temperature += sun_val * sun_mod;
	}
	else {
		temperature += sun_val;
	}
	
	// overall modifiers
	if (temperature < 0) {
		temperature *= cold_mod;
	}
	if (temperature > 0) {
		temperature *= heat_mod;
	}
	
	msg_to_char(ch, "Final temperature: %.1f (%s), %.1f%% sun, %.1f%% seasonal\r\n", temperature, temperature_to_string((int) (temperature + 0.5)), 100.0 * sun_mod, 100.0 * season_mod);
	
	// and now using internal funcs
	calc_temp = calculate_temperature(TEMPERATURE_USE_CLIMATE, get_climate(IN_ROOM(ch)), use_season, use_sun);
	msg_to_char(ch, "Computed temperature for verification: %d %s.\r\n", calc_temp, temperature_to_string(calc_temp));
}


ADMIN_UTIL(util_wipeprogress) {
	empire_data *emp = NULL;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: util wipeprogress <empire | all>\r\n");
	}
	else if (str_cmp(argument, "all") && !(emp = get_empire_by_name(argument))) {
		msg_to_char(ch, "Unknown empire '%s'.\r\n", argument);
	}
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has wiped empire progress for %s", GET_REAL_NAME(ch), emp ? EMPIRE_NAME(emp) : "all empires");
		send_config_msg(ch, "ok_string");
		full_reset_empire_progress(emp);	// if NULL, does ALL
	}
}


ADMIN_UTIL(util_yearly) {
	if (!*argument || str_cmp(argument, "confirm")) {
		msg_to_char(ch, "You must type 'util yearly confirm' to do this. It will cause decay on the entire map.\r\n");
	}
	else {
		send_config_msg(ch, "ok_string");
		annual_world_update();
	}
}


// main interface for admin utils
ACMD(do_admin_util) {
	char util[MAX_INPUT_LENGTH], larg[MAX_INPUT_LENGTH];
	int iter, pos;
	bool found;
	
	half_chop(argument, util, larg);
	
	// find command?
	pos = NOTHING;
	for (iter = 0; admin_utils[iter].func != NULL && pos == NOTHING; ++iter) {
		if (GET_ACCESS_LEVEL(ch) >= admin_utils[iter].level && is_abbrev(util, admin_utils[iter].command)) {
			pos = iter;
		}
	}
	
	if (!*util) {
		msg_to_char(ch, "You have the following utilities: ");
		
		found = FALSE;
		for (iter = 0; admin_utils[iter].func != NULL; ++iter) {
			msg_to_char(ch, "%s%s", (found ? ", " : ""), admin_utils[iter].command);
			found = TRUE;
		}
		
		msg_to_char(ch, "\r\n");
	}
	else if (pos == NOTHING) {
		msg_to_char(ch, "Invalid utility.\r\n");
	}
	else {
		(admin_utils[pos].func)(ch, larg);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// INSTANCE MANAGEMENT /////////////////////////////////////////////////////


void do_instance_add(char_data *ch, char *argument) {
	struct adventure_link_rule *rule, *rule_iter;
	int num_rules, tries, dir = NO_DIR;
	bool found = FALSE;
	room_data *loc;
	adv_vnum vnum;
	adv_data *adv;
	
	if (!*argument || !isdigit(*argument) || (vnum = atoi(argument)) < 0 || !(adv = adventure_proto(vnum))) {
		msg_to_char(ch, "Invalid adventure vnum '%s'.\r\n", *argument ? argument : "<blank>");
		return;
	}
	
	if (!can_instance(adv)) {
		msg_to_char(ch, "Unable to add an instance of that adventure zone.\r\n");
		return;
	}
	
	// randomly choose one rule to attempt
	for (tries = 0; tries < 5 && !found; ++tries) {
		num_rules = 0;
		rule = NULL;
		for (rule_iter = GET_ADV_LINKING(adv); rule_iter; rule_iter = rule_iter->next) {
			if (adventure_link_is_location_rule[rule_iter->type]) {
				// choose one at random
				if (!number(0, num_rules++) || !rule) {
					rule = rule_iter;
				}
			}
		}
	
		if (rule) {
			if ((loc = find_location_for_rule(adv, rule, &dir))) {
				// make it so!
				if (build_instance_loc(adv, rule, loc, dir)) {
					found = TRUE;
				}
			}
		}
	}
	
	if (found) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s created an instance of %s at %s", GET_REAL_NAME(ch), GET_ADV_NAME(adv), room_log_identifier(loc));
		msg_to_char(ch, "Instance added at %s.\r\n", room_log_identifier(loc));
	}
	else {
		msg_to_char(ch, "Unable to find a location to link that adventure.\r\n");
	}
}


void do_instance_delete(char_data *ch, char *argument) {
	struct instance_data *inst;
	room_data *loc;
	int num;
	
	if (!*argument || !isdigit(*argument) || (num = atoi(argument)) < 1) {
		msg_to_char(ch, "Invalid instance number '%s'.\r\n", *argument ? argument : "<blank>");
		return;
	}
	
	DL_FOREACH(instance_list, inst) {
		if (--num == 0) {
			if ((loc = INST_LOCATION(inst))) {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s deleted an instance of %s at %s", GET_REAL_NAME(ch), GET_ADV_NAME(INST_ADVENTURE(inst)), room_log_identifier(loc));
			}
			else {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s deleted an instance of %s at unknown location", GET_REAL_NAME(ch), GET_ADV_NAME(INST_ADVENTURE(inst)));
			}
			msg_to_char(ch, "Instance of %s deleted.\r\n", GET_ADV_NAME(INST_ADVENTURE(inst)));
			delete_instance(inst, TRUE);
			break;
		}
	}
	
	if (num > 0) {
		msg_to_char(ch, "Invalid instance number %d.\r\n", atoi(argument));
	}
}


void do_instance_delete_all(char_data *ch, char *argument) {
	struct instance_data *inst, *next_inst;
	descriptor_data *desc;
	adv_vnum vnum;
	adv_data *adv = NULL;
	int count = 0;
	bool all;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: instance deleteall <adventure vnum | all>\r\n");
		return;
	}
	
	all = !str_cmp(argument, "all");
	if (!all && (!isdigit(*argument) || (vnum = atoi(argument)) < 0 || !(adv = adventure_proto(vnum)))) {
		msg_to_char(ch, "Invalid adventure vnum '%s'.\r\n", argument);
		return;
	}
	
	// warn players of lag on 'all'
	LL_FOREACH(descriptor_list, desc) {
		if (STATE(desc) == CON_PLAYING && desc->character) {
			write_to_descriptor(desc->descriptor, "\r\nThe game is performing a brief update... this will take a moment.\r\n");
			desc->has_prompt = FALSE;
		}
	}
	
	DL_FOREACH_SAFE(instance_list, inst, next_inst) {
		if (all || INST_ADVENTURE(inst) == adv) {
			++count;
			delete_instance(inst, TRUE);
		}
	}
	
	if (count > 0) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s deleted %d instance%s of %s", GET_REAL_NAME(ch), count, PLURAL(count), all ? "adventures" : GET_ADV_NAME(adv));
		msg_to_char(ch, "%d instance%s of '%s' deleted.\r\n", count, PLURAL(count), all ? "adventures" : GET_ADV_NAME(adv));
	}
	else {
		msg_to_char(ch, "No instances of %s found.\r\n", all ? "any adventures" : GET_ADV_NAME(adv));
	}
}


void do_instance_info(char_data *ch, char *argument) {
	struct instance_mob *mc, *next_mc;
	char buf[MAX_STRING_LENGTH];
	struct instance_data *iter, *inst = NULL;
	int num = 1;
	
	// attempt to find instance here with no arg
	if (!*argument) {
		inst = find_instance_by_room(IN_ROOM(ch), FALSE, TRUE);
	}
	
	// otherwise check arg
	if (!inst && (!*argument || !isdigit(*argument) || (num = atoi(argument)) < 1)) {
		msg_to_char(ch, "Invalid instance number '%s'.\r\n", *argument ? argument : "<blank>");
		return;
	}
	
	// look up by number if needed
	if (!inst) {
		DL_FOREACH(instance_list, iter) {
			if (--num == 0) {
				inst = iter;
				break;
			}
		}
	}
	
	// show if found
	if (inst) {
		msg_to_char(ch, "\tcInstance %d: [%d] %s\t0\r\n", atoi(argument), GET_ADV_VNUM(INST_ADVENTURE(inst)), GET_ADV_NAME(INST_ADVENTURE(inst)));
		
		if (INST_LOCATION(inst)) {
			if (ROOM_OWNER(INST_LOCATION(inst))) {
				sprintf(buf, " / %s%s\t0", EMPIRE_BANNER(ROOM_OWNER(INST_LOCATION(inst))), EMPIRE_NAME(ROOM_OWNER(INST_LOCATION(inst))));
			}
			else {
				*buf = '\0';
			}
			msg_to_char(ch, "Location: [%d] %s%s%s\r\n", GET_ROOM_VNUM(INST_LOCATION(inst)), get_room_name(INST_LOCATION(inst), FALSE), coord_display_room(ch, INST_LOCATION(inst), FALSE), buf);
		}
		if (INST_FAKE_LOC(inst) && INST_FAKE_LOC(inst) != INST_LOCATION(inst)) {
			if (ROOM_OWNER(INST_FAKE_LOC(inst))) {
				sprintf(buf, " / %s%s\t0", EMPIRE_BANNER(ROOM_OWNER(INST_FAKE_LOC(inst))), EMPIRE_NAME(ROOM_OWNER(INST_FAKE_LOC(inst))));
			}
			else {
				*buf = '\0';
			}
			msg_to_char(ch, "Fake Location: [%d] %s%s%s\r\n", GET_ROOM_VNUM(INST_FAKE_LOC(inst)), get_room_name(INST_FAKE_LOC(inst), FALSE), coord_display_room(ch, INST_FAKE_LOC(inst), FALSE), buf);
		}
		
		if (INST_START(inst)) {
			msg_to_char(ch, "First room: [%d] %s\r\n", GET_ROOM_VNUM(INST_START(inst)), get_room_name(INST_START(inst), FALSE));
		}
		
		if (INST_LEVEL(inst) > 0) {
			msg_to_char(ch, "Level: %d\r\n", INST_LEVEL(inst));
		}
		else {
			msg_to_char(ch, "Level: unscaled\r\n");
		}
		
		sprintbit(INST_FLAGS(inst), instance_flags, buf, TRUE);
		msg_to_char(ch, "Flags: %s\r\n", buf);
		
		msg_to_char(ch, "Created: %-24.24s\r\n", (char *) asctime(localtime(&INST_CREATED(inst))));
		if (INST_LAST_RESET(inst) != INST_CREATED(inst)) {
			msg_to_char(ch, "Last reset: %-24.24s\r\n", (char *) asctime(localtime(&INST_LAST_RESET(inst))));
		}
		
		if (INST_DIR(inst) != NO_DIR) {
			msg_to_char(ch, "Facing: %s\r\n", dirs[INST_DIR(inst)]);
		}
		if (INST_ROTATION(inst) != NO_DIR && IS_SET(GET_ADV_FLAGS(INST_ADVENTURE(inst)), ADV_ROTATABLE)) {
			msg_to_char(ch, "Rotation: %s\r\n", dirs[INST_ROTATION(inst)]);
		}
		
		if (INST_MOB_COUNTS(inst)) {
			msg_to_char(ch, "Mob counts:\r\n");
			HASH_ITER(hh, INST_MOB_COUNTS(inst), mc, next_mc) {
				msg_to_char(ch, "%3d %s\r\n", mc->count, skip_filler(get_mob_name_by_proto(mc->vnum, FALSE)));
			}
		}
	}
	else {
		msg_to_char(ch, "Invalid instance number %d.\r\n", atoi(argument));
	}
}


// shows by adventure
void do_instance_list_all(char_data *ch) {
	adv_data *adv, *next_adv;
	int count = 0;
	
	build_page_display_str(ch, "Instances by adventure:");
	
	// list by adventure
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		// skip in-dev adventures with no count
		count = count_instances(adv);
		if (ADVENTURE_FLAGGED(adv, ADV_IN_DEVELOPMENT) && !count) {
			continue;
		}
		
		build_page_display(ch, "[%5d] %s (%d/%d)", GET_ADV_VNUM(adv), GET_ADV_NAME(adv), count, adjusted_instance_limit(adv));
	}
	
	send_page_display(ch);
}


// list by name
void do_instance_list(char_data *ch, char *argument) {
	char line[MAX_STRING_LENGTH];
	struct instance_data *inst;
	adv_data *adv = NULL;
	adv_vnum vnum;
	int num = 0, count = 0;
	
	if (!ch->desc) {
		return;
	}
	
	// new in b3.20: no-arg shows a different list entirely
	if (!*argument) {
		do_instance_list_all(ch);
		return;
	}
	
	if (*argument && (!isdigit(*argument) || (vnum = atoi(argument)) < 0 || !(adv = adventure_proto(vnum)))) {
		msg_to_char(ch, "Invalid adventure vnum '%s'.\r\n", argument);
		return;
	}
	
	DL_FOREACH(instance_list, inst) {
		// num is out of the total instances, not just ones shown
		++num;
		
		if (!adv || adv == INST_ADVENTURE(inst)) {
			++count;
			instance_list_row(inst, num, line, sizeof(line));
			build_page_display_str(ch, line);
		}
	}
	
	build_page_display(ch, "%d total instances shown", count);
	send_page_display(ch);
}


void do_instance_nearby(char_data *ch, char *argument) {
	char line[MAX_STRING_LENGTH];
	struct instance_data *inst;
	int num = 0, count = 0, distance = 50;
	room_data *inst_loc, *loc;
	
	loc = GET_MAP_LOC(IN_ROOM(ch)) ? real_room(GET_MAP_LOC(IN_ROOM(ch))->vnum) : NULL;
	
	if (*argument && (!isdigit(*argument) || (distance = atoi(argument)) < 0)) {
		msg_to_char(ch, "Invalid distance '%s'.\r\n", argument);
		return;
	}
	
	build_page_display(ch, "Instances within %d tiles:", distance);
	
	if (loc) {	// skip work if no map location found
		DL_FOREACH(instance_list, inst) {
			++num;
		
			inst_loc = INST_FAKE_LOC(inst);
			if (inst_loc && !INSTANCE_FLAGGED(inst, INST_COMPLETED) && compute_distance(loc, inst_loc) <= distance) {
				++count;
				instance_list_row(inst, num, line, sizeof(line));
				build_page_display_str(ch, line);
			}
		}
	}
	
	build_page_display(ch, "%d total instances shown", count);
	send_page_display(ch);
}


void do_instance_reset(char_data *ch, char *argument) {
	struct instance_data *inst;
	room_data *loc = NULL;
	int num;
	
	if (*argument) {
		if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid instance number '%s'.\r\n", *argument ? argument : "<blank>");
			return;
		}
	
		DL_FOREACH(instance_list, inst) {
			if (--num == 0) {
				loc = INST_FAKE_LOC(inst);
				reset_instance(inst);
				break;
			}
		}
		
		if (num > 0) {
			msg_to_char(ch, "Invalid instance number %d.\r\n", atoi(argument));
			return;
		}
	}
	else {
		// no argument
		if (!(inst = find_instance_by_room(IN_ROOM(ch), FALSE, TRUE))) {
			msg_to_char(ch, "You are not in or near an adventure zone instance.\r\n");
			return;
		}

		loc = INST_FAKE_LOC(inst);
		reset_instance(inst);
	}
	
	if (loc) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s forced a reset of an instance of %s at %s", GET_REAL_NAME(ch), GET_ADV_NAME(INST_ADVENTURE(inst)), room_log_identifier(loc));
	}
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s forced a reset of an instance of %s at unknown location", GET_REAL_NAME(ch), GET_ADV_NAME(INST_ADVENTURE(inst)));
	}
	send_config_msg(ch, "ok_string");
}


/**
* Fetches a single row of instance data (usually for do_instance_list or
* do_instance_nearby).
*
* @param struct instance_data *inst The instance to list.
* @param int number The number to show it as (numbered list).
* @param char *save_buffer A buffer to save the output to.
* @param size_t size Size of the buffer.
*/
void instance_list_row(struct instance_data *inst, int number, char *save_buffer, size_t size) {
	char flg[256], info[256], owner[MAX_STRING_LENGTH];

	if (INST_LEVEL(inst) > 0) {
		sprintf(info, " L-%d", INST_LEVEL(inst));
	}
	else {
		*info = '\0';
	}
	
	if (INST_FAKE_LOC(inst) && ROOM_OWNER(INST_FAKE_LOC(inst))) {
		safe_snprintf(owner, sizeof(owner), "(%s%s\t0)", EMPIRE_BANNER(ROOM_OWNER(INST_FAKE_LOC(inst))), EMPIRE_NAME(ROOM_OWNER(INST_FAKE_LOC(inst))));
	}
	else {
		*owner = '\0';
	}

	sprintbit(INST_FLAGS(inst), instance_flags, flg, TRUE);
	safe_snprintf(save_buffer, size, "%3d. [%5d] %s [%d] (%d, %d)%s %s%s\r\n", number, GET_ADV_VNUM(INST_ADVENTURE(inst)), GET_ADV_NAME(INST_ADVENTURE(inst)), INST_FAKE_LOC(inst) ? GET_ROOM_VNUM(INST_FAKE_LOC(inst)) : NOWHERE, INST_FAKE_LOC(inst) ? X_COORD(INST_FAKE_LOC(inst)) : NOWHERE, INST_FAKE_LOC(inst) ? Y_COORD(INST_FAKE_LOC(inst)) : NOWHERE, info, INST_FLAGS(inst) != NOBITS ? flg : "", owner);
}


void do_instance_spawn(char_data *ch, char *argument) {
	int num = 1;
	
	if (*argument && isdigit(*argument)) {
		num = atoi(argument);
	}
	if (num < 1 || num > 100) {
		msg_to_char(ch, "You may only run 1-100 spawn cycles.\r\n");
		return;
	}
	
	msg_to_char(ch, "Running %d instance spawn cycle%s...\r\n", num, PLURAL(num));
	while (num-- > 0) {
		generate_adventure_instances();
	}
}


void do_instance_test(char_data *ch, char *argument) {
	struct adventure_link_rule rule;
	bool found = FALSE;
	adv_vnum vnum;
	adv_data *adv;
	
	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "An instance is already linked here.\r\n");
		return;
	}
	
	if (!*argument || !isdigit(*argument) || (vnum = atoi(argument)) < 0 || !(adv = adventure_proto(vnum))) {
		msg_to_char(ch, "Invalid adventure vnum '%s'.\r\n", *argument ? argument : "<blank>");
		return;
	}
	
	// fake a link rule for this room
	rule.type = ADV_LINK_PORTAL_WORLD;
	rule.flags = NOBITS;
	rule.value = GET_SECT_VNUM(SECT(IN_ROOM(ch)));
	rule.portal_in = o_PORTAL;
	rule.portal_out = o_PORTAL;
	rule.dir = DIR_RANDOM;
	rule.bld_on = NOBITS;
	rule.bld_facing = NOBITS;
	rule.next = NULL;
	
	if (build_instance_loc(adv, &rule, IN_ROOM(ch), DIR_RANDOM)) {
		found = TRUE;
	}
	
	if (found) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s created a test instance of %s at %s", GET_REAL_NAME(ch), GET_ADV_NAME(adv), room_log_identifier(IN_ROOM(ch)));
		msg_to_char(ch, "Test instance added at %s.\r\n", room_log_identifier(IN_ROOM(ch)));
	}
	else {
		msg_to_char(ch, "Unable to link that adventure here.\r\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// SET DATA AND FUNCTIONS //////////////////////////////////////////////////

#define PC   1
#define NPC  2
#define BOTH 3

#define MISC	0
#define BINARY	1
#define NUMBER	2

#define SET_OR_REMOVE(flagset, flags) { \
	if (on) SET_BIT(flagset, flags); \
	else if (off) REMOVE_BIT(flagset, flags); }

#define RANGE(low, high) (value = MAX((low), MIN((high), (value))))

#define SET_CASE(str)		(!str_cmp(str, set_fields[mode].cmd))


/* The set options available */
struct set_struct {
	const char *cmd;
	const char level;
	const char pcnpc;
	const char type;
	} set_fields[] = {
		{ "invstart", 	LVL_START_IMM, 	PC, 	BINARY },
		{ "title",		LVL_START_IMM, 	PC, 	MISC },
		{ "notitle",	LVL_START_IMM,	PC, 	BINARY },
		{ "nocustomize", LVL_START_IMM,	PC,		BINARY },

		{ "health",		LVL_START_IMM, 	BOTH, 	NUMBER },
		{ "move",		LVL_START_IMM, 	BOTH, 	NUMBER },
		{ "mana",		LVL_START_IMM, 	BOTH, 	NUMBER },
		{ "blood",		LVL_START_IMM, 	BOTH, 	NUMBER },

		{ "coins",		LVL_START_IMM,	PC,		MISC },
		{ "frozen",		LVL_START_IMM,	PC, 	BINARY },
		{ "drunk",		LVL_START_IMM, 	BOTH, 	MISC },
		{ "hunger",		LVL_START_IMM, 	BOTH, 	MISC },
		{ "thirst",		LVL_START_IMM, 	BOTH, 	MISC },
		{ "account",	LVL_TO_SEE_ACCOUNTS, PC,MISC },
		{ "access",		LVL_CIMPL, 	PC, 	NUMBER },
		{ "siteok",		LVL_START_IMM, 	PC, 	BINARY },
		{ "nowizlist", 	LVL_START_IMM, 	PC, 	BINARY },
		{ "nofriends", 	LVL_START_IMM, 	PC, 	BINARY },
		{ "loadroom", 	LVL_START_IMM, 	PC, 	MISC },
		{ "password",	LVL_CIMPL, 	PC, 	MISC },
		{ "nodelete", 	LVL_CIMPL, 	PC, 	BINARY },
		{ "noidleout",	LVL_START_IMM,	PC,		BINARY },
		{ "sex", 		LVL_START_IMM, 	BOTH, 	MISC },
		{ "age",		LVL_START_IMM,	BOTH,	NUMBER },
		{ "lastname",	LVL_START_IMM,	PC,		MISC },
		{ "muted",		LVL_START_IMM,	PC, 	BINARY },
		{ "name",		LVL_CIMPL,	PC,		MISC },
		{ "incognito",	LVL_START_IMM,	PC,		BINARY },
		{ "ipmask",		LVL_START_IMM,	PC,		BINARY },
		{ "multi-ip",	LVL_START_IMM,	PC,		BINARY },
		{ "multi-char",	LVL_START_IMM,	PC,		BINARY },	// deliberately after multi-ip, which is more common
		{ "vampire",	LVL_START_IMM,	PC, 	BINARY },
		{ "wizhide",	LVL_START_IMM,	PC,		BINARY },
		{ "bonustrait",	LVL_START_IMM,	PC,		MISC },
		{ "bonusexp", LVL_START_IMM, PC, NUMBER },
		{ "dailyquestscompleted", LVL_START_IMM, PC, NUMBER },
		{ "eventdailyquestscompleted", LVL_START_IMM, PC, NUMBER },
		{ "grants",		LVL_CIMPL,	PC,		MISC },
		{ "maxlevel", LVL_START_IMM, PC, NUMBER },
		{ "skill", LVL_START_IMM, PC, MISC },
		{ "faction", LVL_START_IMM, PC, MISC },
		{ "language", LVL_START_IMM, PC, MISC },
		{ "learned", LVL_START_IMM, PC, MISC },
		{ "minipet", LVL_START_IMM, PC, MISC },
		{ "mount", LVL_START_IMM, PC, MISC },
		{ "currency", LVL_START_IMM, PC, MISC },
		{ "companion", LVL_START_IMM, PC, MISC },
		{ "unlockedarchetype", LVL_START_IMM, PC, MISC },

		{ "strength",	LVL_START_IMM,	BOTH,	NUMBER },
		{ "dexterity",	LVL_START_IMM,	BOTH,	NUMBER },
		{ "charisma",	LVL_START_IMM,	BOTH,	NUMBER },
		{ "greatness",	LVL_START_IMM,	BOTH,	NUMBER },
		{ "intelligence",LVL_START_IMM,	BOTH,	NUMBER },
		{ "wits",		LVL_START_IMM,	BOTH,	NUMBER },
		
		{ "scale", LVL_START_IMM, NPC, NUMBER },

		{ "\n", 0, BOTH, MISC }
};


/* All setting is done here, for simplicity */
int perform_set(char_data *ch, char_data *vict, int mode, char *val_arg) {
	player_index_data *index, *next_index, *found_index;
	int i, iter, on = 0, off = 0, value = 0;
	empire_data *emp;
	room_vnum rvnum;
	char output[MAX_STRING_LENGTH], oldname[MAX_INPUT_LENGTH], newname[MAX_INPUT_LENGTH], temp[MAX_STRING_LENGTH];
	char_data *alt;
	bool file = FALSE;

	*output = '\0';

	/* Check to make sure all the levels are correct */
	if (GET_ACCESS_LEVEL(ch) != LVL_IMPL) {
		if (!IS_NPC(vict) && GET_ACCESS_LEVEL(ch) <= GET_ACCESS_LEVEL(vict) && vict != ch) {
			send_to_char("Maybe that's not such a great idea...\r\n", ch);
			return (0);
		}
	}
	if (GET_ACCESS_LEVEL(ch) < set_fields[mode].level) {
		send_to_char("You are not godly enough for that!\r\n", ch);
		return (0);
	}

	/* Make sure the PC/NPC is correct */
	if (IS_NPC(vict) && !(set_fields[mode].pcnpc & NPC)) {
		send_to_char("You can't do that to a beast!\r\n", ch);
		return (0);
	}
	else if (!IS_NPC(vict) && !(set_fields[mode].pcnpc & PC)) {
		send_to_char("That can only be done to a beast!\r\n", ch);
		return (0);
	}

	/* Find the value of the argument */
	if (set_fields[mode].type == BINARY) {
		if (!str_cmp(val_arg, "on") || !str_cmp(val_arg, "yes"))
			on = 1;
		else if (!str_cmp(val_arg, "off") || !str_cmp(val_arg, "no"))
			off = 1;
		if (!(on || off)) {
			send_to_char("Value must be 'on' or 'off'.\r\n", ch);
			return (0);
		}
		sprintf(output, "%s %s for %s.", set_fields[mode].cmd, ONOFF(on), GET_NAME(vict));
	}
	else if (set_fields[mode].type == NUMBER) {
		value = atoi(val_arg);
		sprintf(output, "%s's %s set to %%d.", GET_NAME(vict), set_fields[mode].cmd);
	}
	else
		strcpy(output, "Okay.");


	if SET_CASE("invstart") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_INVSTART);
	}
	else if SET_CASE("title") {
		set_title(vict, val_arg);
		sprintf(output, "%s's title is now: %s", GET_NAME(vict), NULLSAFE(GET_TITLE(vict)));
	}

	else if SET_CASE("health") {
		set_health(vict, RANGE(0, GET_MAX_HEALTH(vict)));
		affect_total(vict);
	}
	else if SET_CASE("move") {
		set_move(vict, RANGE(0, GET_MAX_MOVE(vict)));
		affect_total(vict);
	}
	else if SET_CASE("mana") {
		set_mana(vict, RANGE(0, GET_MAX_MANA(vict)));
		affect_total(vict);
	}
	else if SET_CASE("blood") {
		set_blood(vict, RANGE(0, GET_MAX_BLOOD(vict)));
		affect_total(vict);
	}

	else if SET_CASE("strength") {
		vict->real_attributes[STRENGTH] = RANGE(1, att_max(vict));
		affect_total(vict);
	}
	else if SET_CASE("dexterity") {
		vict->real_attributes[DEXTERITY] = RANGE(1, att_max(vict));
		affect_total(vict);
	}
	else if SET_CASE("charisma") {
		vict->real_attributes[CHARISMA] = RANGE(1, att_max(vict));
		affect_total(vict);
	}
	else if SET_CASE("greatness") {
		vict->real_attributes[GREATNESS] = RANGE(1, att_max(vict));
		affect_total(vict);
	}
	else if SET_CASE("intelligence") {
		vict->real_attributes[INTELLIGENCE] = RANGE(1, att_max(vict));
		affect_total(vict);
	}
	else if SET_CASE("wits") {
		vict->real_attributes[WITS] = RANGE(1, att_max(vict));
		affect_total(vict);
	}

	else if SET_CASE("frozen") {
		if (ch == vict && on) {
			send_to_char("Better not -- could be a long winter!\r\n", ch);
			return (0);
		}
		if (get_highest_access_level(GET_ACCOUNT(vict)) >= GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "Maybe that's not such a great idea.\r\n");
			return 0;
		}
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_FROZEN);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("muted") {
		if (get_highest_access_level(GET_ACCOUNT(vict)) >= GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "Maybe that's not such a great idea.\r\n");
			return 0;
		}
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_MUTED);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("nocustomize") {
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_NOCUSTOMIZE);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("notitle") {
		if (get_highest_access_level(GET_ACCOUNT(vict)) >= GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "Maybe that's not such a great idea.\r\n");
			return 0;
		}
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_NOTITLE);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("ipmask") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_IPMASK);
	}
	else if SET_CASE("incognito") {
		SET_OR_REMOVE(PRF_FLAGS(vict), PRF_INCOGNITO);
	}
	else if SET_CASE("wizhide") {
		SET_OR_REMOVE(PRF_FLAGS(vict), PRF_WIZHIDE);
	}
	else if SET_CASE("multi-ip") {
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_MULTI_IP);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("multi-char") {
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_MULTI_CHAR);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("vampire") {
		if (IS_VAMPIRE(vict) && !str_cmp(val_arg, "off")) {
			check_un_vampire(vict, TRUE);
		}
		else if (!IS_VAMPIRE(vict) && !str_cmp(val_arg, "on")) {
			make_vampire(vict, TRUE, NOTHING);
		}
		// else nothing to do but the syslog won't be inaccurate
	}
	else if SET_CASE("hunger") {
		if (!str_cmp(val_arg, "off")) {
			GET_COND(vict, FULL) = UNLIMITED;
			sprintf(output, "%s's hunger now off.", GET_NAME(vict));
		}
		else if (is_number(val_arg)) {
			value = atoi(val_arg);
			RANGE(0, MAX_CONDITION);
			GET_COND(vict, FULL) = value;
			sprintf(output, "%s's hunger set to %d.", GET_NAME(vict), value);
		}
		else {
			msg_to_char(ch, "Must be 'off' or a value from 0 to %d.\r\n", MAX_CONDITION);
			return (0);
		}
	}
	else if SET_CASE("drunk") {
		if (!str_cmp(val_arg, "off")) {
			GET_COND(vict, DRUNK) = UNLIMITED;
			sprintf(output, "%s's drunk now off.", GET_NAME(vict));
		}
		else if (is_number(val_arg)) {
			value = atoi(val_arg);
			RANGE(0, MAX_CONDITION);
			GET_COND(vict, DRUNK) = value;
			sprintf(output, "%s's drunk set to %d.", GET_NAME(vict), value);
		}
		else {
			msg_to_char(ch, "Must be 'off' or a value from 0 to %d.\r\n", MAX_CONDITION);
			return (0);
		}
	}
	else if SET_CASE("thirst") {
		if (!str_cmp(val_arg, "off")) {
			GET_COND(vict, THIRST) = UNLIMITED;
			sprintf(output, "%s's thirst now off.", GET_NAME(vict));
		}
		else if (is_number(val_arg)) {
			value = atoi(val_arg);
			RANGE(0, MAX_CONDITION);
			GET_COND(vict, THIRST) = value;
			sprintf(output, "%s's thirst set to %d.", GET_NAME(vict), value);
		}
		else {
			msg_to_char(ch, "Must be 'off' or a value from 0 to %d.\r\n", MAX_CONDITION);
			return (0);
		}
	}
	else if SET_CASE("access") {
		if (value > GET_ACCESS_LEVEL(ch) || value > LVL_IMPL) {
			send_to_char("You can't do that.\r\n", ch);
			return (0);
		}
		
		if (!IS_NPC(vict) && GET_ACCESS_LEVEL(vict) < LVL_START_IMM && value >= LVL_START_IMM) {
			perform_approve(vict);
			SET_BIT(PRF_FLAGS(vict), PRF_HOLYLIGHT | PRF_ROOMFLAGS | PRF_NOHASSLE);
		
			// turn on all syslogs
			for (iter = 0; *syslog_types[iter] != '\n'; ++iter) {
				SYSLOG_FLAGS(vict) |= BIT(iter);
			}
		}
		
		RANGE(0, LVL_IMPL);
		vict->player.access_level = (byte) value;
		if (!IS_NPC(vict)) {
			GET_IMMORTAL_LEVEL(vict) = (GET_ACCESS_LEVEL(vict) > LVL_MORTAL ? (LVL_TOP - GET_ACCESS_LEVEL(vict)) : -1);
		}
		SAVE_CHAR(vict);
		check_autowiz(ch);
	}
	else if SET_CASE("siteok") {
		SET_OR_REMOVE(GET_ACCOUNT(vict)->flags, ACCT_SITEOK);
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
	}
	else if SET_CASE("nowizlist") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NOWIZLIST);
	}
	else if SET_CASE("nofriends") {
		SET_OR_REMOVE(PRF_FLAGS(vict), PRF_NO_FRIENDS);
	}
	else if SET_CASE("loadroom") {
		if (!str_cmp(val_arg, "off")) {
			REMOVE_BIT(PLR_FLAGS(vict), PLR_LOADROOM);
			sprintf(output, "Loadroom for %s off.", GET_NAME(vict));
		}
		else if (is_number(val_arg)) {
			rvnum = atoi(val_arg);
			if (real_room(rvnum)) {
				SET_BIT(PLR_FLAGS(vict), PLR_LOADROOM);
				GET_LOADROOM(vict) = rvnum;
				sprintf(output, "%s will enter at room #%d.", GET_NAME(vict), GET_LOADROOM(vict));
			}
			else {
				send_to_char("That room does not exist!\r\n", ch);
				return (0);
			}
		}
		else {
			send_to_char("Must be 'off' or a room's virtual number.\r\n", ch);
			return (0);
		}
	}
	else if SET_CASE("password") {
		if (GET_ACCESS_LEVEL(vict) >= LVL_IMPL) {
			send_to_char("You cannot change that.\r\n", ch);
			return (0);
		}
		if (GET_PASSWD(vict)) {
			free(GET_PASSWD(vict));
		}
		GET_PASSWD(vict) = str_dup(CRYPT(val_arg, PASSWORD_SALT));
		sprintf(output, "Password for %s changed.", GET_NAME(vict));
	}
	else if SET_CASE("nodelete") {
		SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NODELETE);
	}
	else if SET_CASE("noidleout") {
		SET_OR_REMOVE(PRF_FLAGS(vict), PRF_NO_IDLE_OUT);
	}
	else if SET_CASE("sex") {
		if ((i = search_block(val_arg, genders, FALSE)) == NOTHING) {
			send_to_char("Must be 'male', 'female', or 'neutral'.\r\n", ch);
			return (0);
		}
		change_sex(vict, i);
		sprintf(output, "%s's sex is now %s.", GET_NAME(vict), genders[(int) GET_REAL_SEX(vict)]);
	}
	else if SET_CASE("age") {
		if (value < 2 || value > 95) {	/* Arbitrary limits. */
			send_to_char("Ages 2 to 95 accepted.\r\n", ch);
			return (0);
		}
		vict->player.time.birth = time(0) - ((value - 17) * SECS_PER_MUD_YEAR);
	}
	else if SET_CASE("lastname") {
		char va_1[MAX_INPUT_LENGTH], va_2[MAX_INPUT_LENGTH];
		
		half_chop(val_arg, va_1, va_2);
		
		if (!*val_arg) {
			msg_to_char(ch, "Usage: set <name> lastname <new personal lastname | none>\r\n");
			msg_to_char(ch, "Usage: set <name> lastname <add | remove> <name>\r\n");
			return 0;
		}
		else if (!str_cmp(va_1, "add")) {
			add_lastname(vict, va_2);
			sprintf(output, "Added lastname for %s: %s.", GET_NAME(vict), va_2);
		}
		else if (!str_cmp(va_1, "remove")) {
			if (has_lastname(vict, va_2)) {
				remove_lastname(vict, va_2);
				sprintf(output, "Removed lastname for %s: %s.", GET_NAME(vict), va_2);
			}
			else {
				msg_to_char(ch, "%s doesn't have that lastname.\r\n", GET_NAME(vict));
				return 0;
			}
		}
		else if (!str_cmp(val_arg, "off") || !str_cmp(val_arg, "none")) {
			change_personal_lastname(vict, NULL);
    		sprintf(output, "%s no longer has a last name.", GET_NAME(vict));
		}
    	else {
			change_personal_lastname(vict, val_arg);
			if (!GET_CURRENT_LASTNAME(vict) && GET_PERSONAL_LASTNAME(vict)) {
				GET_CURRENT_LASTNAME(vict) = str_dup(GET_PERSONAL_LASTNAME(vict));
			}
    		sprintf(output, "%s's last name is now: %s", GET_NAME(vict), GET_PERSONAL_LASTNAME(vict));
		}
		
		update_MSDP_name(vict, UPDATE_NOW);
	}
	else if SET_CASE("bonustrait") {
		bitvector_t diff, new, old = GET_BONUS_TRAITS(vict);
		new = GET_BONUS_TRAITS(vict) = olc_process_flag(ch, val_arg, "bonus", NULL, bonus_bits, GET_BONUS_TRAITS(vict));
		
		// this indicates a change
		if (new != old) {
			sprintbit(new, bonus_bits, buf, TRUE);
			sprintf(output, "%s now has bonus traits: %s", GET_NAME(vict), buf);
			
			// apply them
			if ((diff = new & ~old)) {
				apply_bonus_trait(vict, diff, TRUE);
			}
			if ((diff = old & ~new)) {
				// removing?
				apply_bonus_trait(vict, diff, FALSE);
			}
		}
		else {
			// no change
			return 0;
		}
	}
	else if SET_CASE("bonusexp") {
		GET_DAILY_BONUS_EXPERIENCE(vict) = RANGE(0, 255);
		update_MSDP_bonus_exp(vict, UPDATE_SOON);
	}
	else if SET_CASE("dailyquestscompleted") {
		GET_DAILY_QUESTS(vict) = RANGE(0, config_get_int("dailies_per_day"));
	}
	else if SET_CASE("eventdailyquestscompleted") {
		GET_EVENT_DAILY_QUESTS(vict) = RANGE(0, config_get_int("dailies_per_day"));
	}
	else if SET_CASE("maxlevel") {
		GET_HIGHEST_KNOWN_LEVEL(vict) = RANGE(0, SHRT_MAX);
	}
	else if SET_CASE("grants") {
		bitvector_t new, old = GET_GRANT_FLAGS(vict);
		new = GET_GRANT_FLAGS(vict) = olc_process_flag(ch, val_arg, "grants", NULL, grant_bits, GET_GRANT_FLAGS(vict));
		
		// this indicates a change
		if (old & ~new) {	// removed
			prettier_sprintbit(old & ~new, grant_bits, buf);
			sprintf(output, "%s lost grants: %s", GET_NAME(vict), buf);
		}
		else if (new & ~old) {	// added
			prettier_sprintbit(new & ~old, grant_bits, buf);
			sprintf(output, "%s gained grants: %s", GET_NAME(vict), buf);
		}
		else {
			// no change
			return 0;
		}
	}
	else if SET_CASE("coins") {
		char typearg[MAX_INPUT_LENGTH], *amtarg;
		struct coin_data *coin;
		empire_data *type;
		int amt;
		
		// set <name> coins <type> <amount>
		amtarg = any_one_arg(val_arg, typearg);
		skip_spaces(&amtarg);
		
		if (!*typearg || !*amtarg || !isdigit(*amtarg)) {
			msg_to_char(ch, "Usage: set <name> coins <type> <amount>\r\n");
			return 0;
		}
		else if (!(type = get_empire_by_name(typearg)) && !is_abbrev(typearg, "other") && !is_abbrev(typearg, "miscellaneous") && !is_abbrev(typearg, "simple")) {
			msg_to_char(ch, "Invalid coin type. Specify an empire name or 'miscellaneous'.\r\n");
			return 0;
		}
		else if ((amt = atoi(amtarg)) < 0) {
			msg_to_char(ch, "You can't set negative coins.\r\n");
		}
		else {
			// prepare message first (amt changes)
			sprintf(output, "%s now has %s.", GET_NAME(vict), money_amount(type, amt));
			
			// we're going to use increase_coins, so first see if they already had some
			if ((coin = find_coin_entry(GET_PLAYER_COINS(vict), type))) {
				amt -= coin->amount;
			}
			increase_coins(vict, type, amt);
		}
	}
	else if SET_CASE("companion") {
		char vnum_arg[MAX_INPUT_LENGTH], onoff_arg[MAX_INPUT_LENGTH];
		char_data *pet;
		
		half_chop(val_arg, vnum_arg, onoff_arg);
		
		if (!*vnum_arg || !isdigit(*vnum_arg) || !*onoff_arg) {
			msg_to_char(ch, "Usage: set <name> companion <mob vnum> <on | off>\r\n");
			return 0;
		}
		if (!(pet = mob_proto(atoi(vnum_arg)))) {
			msg_to_char(ch, "Invalid mob vnum.\r\n");
			return 0;
		}
		
		if (!str_cmp(onoff_arg, "on")) {
			add_companion(vict, GET_MOB_VNUM(pet), NO_ABIL);
			sprintf(output, "%s: gained companion %d %s.", GET_NAME(vict), GET_MOB_VNUM(pet), GET_SHORT_DESC(pet));
		}
		else if (!str_cmp(onoff_arg, "off")) {
			remove_companion(vict, GET_MOB_VNUM(pet));
			sprintf(output, "%s: removed companion %d %s.", GET_NAME(vict), GET_MOB_VNUM(pet), GET_SHORT_DESC(pet));
			if (GET_COMPANION(vict) && GET_MOB_VNUM(GET_COMPANION(vict)) == GET_MOB_VNUM(pet)) {
				if (!AFF_FLAGGED(vict, AFF_HIDDEN | AFF_NO_SEE_IN_ROOM)) {
					act("$n leaves.", TRUE, GET_COMPANION(vict), NULL, NULL, TO_ROOM);
				}
				extract_char(GET_COMPANION(vict));
			}
		}
		else {
			msg_to_char(ch, "Do you want to turn it on or off?\r\n");
			return 0;
		}
	}
	else if SET_CASE("faction") {
		int min_idx, min_rep, max_idx, max_rep, new_rep, new_val = 0;
		char fct_arg[MAX_INPUT_LENGTH], *fct_val;
		struct player_faction_data *pfd;
		bool del_rep = FALSE;
		faction_data *fct;
		
		// usage: set <player> faction <name/vnum> <rep/value>
		fct_val = any_one_word(val_arg, fct_arg);
		skip_spaces(&fct_val);
		
		if (!*fct_arg || !*fct_val) {
			msg_to_char(ch, "Usage: set <player> faction <name/vnum> <rep/value>\r\n");
			return 0;
		}
		if (!(fct = find_faction(fct_arg)) || FACTION_FLAGGED(fct, FCT_IN_DEVELOPMENT)) {
			msg_to_char(ch, "Unknown faction '%s'.\r\n", fct_arg);
			return 0;
		}
		
		// helper data
		min_idx = rep_const_to_index(FCT_MIN_REP(fct));
		min_rep = (min_idx != NOTHING) ? reputation_levels[min_idx].value : MIN_REPUTATION;
		max_idx = rep_const_to_index(FCT_MAX_REP(fct));
		max_rep = (max_idx != NOTHING) ? reputation_levels[max_idx].value : MAX_REPUTATION;
		
		// parse val arg
		if (!str_cmp(fct_val, "none")) {
			del_rep = TRUE;
		}
		else if (isdigit(*fct_val) || *fct_val == '-') {
			new_val = atoi(fct_val);
		}
		else if ((new_rep = get_reputation_by_name(fct_val)) == NOTHING) {
			msg_to_char(ch, "Invalid reputation level '%s'.\r\n", fct_val);
			return 0;
		}
		else {
			new_val = reputation_levels[rep_const_to_index(new_rep)].value;
		}
		
		// bounds check
		if (!del_rep && (new_val < min_rep || new_val > max_rep)) {
			msg_to_char(ch, "You can't set the reputation to that level. That faction has a range of %d-%d.\r\n", min_rep, max_rep);
			return 0;
		}
		
		// load data
		if (!(pfd = get_reputation(vict, FCT_VNUM(fct), TRUE))) {
			msg_to_char(ch, "Unable to set reputation for that player and faction.\r\n");
			return 0;
		}
		
		// and go
		if (del_rep) {
			HASH_DEL(GET_FACTIONS(vict), pfd);
			free(pfd);
			sprintf(output, "%s's reputation with %s deleted.", GET_NAME(vict), FCT_NAME(fct));
		}
		else {
			pfd->value = new_val;
			update_reputations(vict);
			pfd = get_reputation(vict, FCT_VNUM(fct), TRUE);
			new_rep = rep_const_to_index(pfd->rep);
			sprintf(output, "%s's reputation with %s set to %s / %d.", GET_NAME(vict), FCT_NAME(fct), reputation_levels[new_rep].name, pfd->value);
		}
	}
	else if SET_CASE("skill") {
		char skillname[MAX_INPUT_LENGTH], *skillval;
		int level = -1;
		skill_data *skill;
		
		// set <name> skill "<name>" <level>
		skillval = any_one_word(val_arg, skillname);
		skip_spaces(&skillval);
		
		if (!*skillname || !*skillval) {
			msg_to_char(ch, "Usage: set <name> skill <skill> <level>\r\n");
			return 0;
		}
		if (!isdigit(*skillval) || (level = atoi(skillval)) < 0 || level > MAX_SKILL_CAP) {
			msg_to_char(ch, "You must choose a level between 0 and %d.\r\n", MAX_SKILL_CAP);
			return 0;
		}
		else if (!(skill = find_skill(skillname))) {
			msg_to_char(ch, "Unknown skill '%s'.\r\n", skillname);
			return 0;
		}

		// victory
		set_skill(vict, SKILL_VNUM(skill), level);
		update_class_and_abilities(vict);
		check_ability_levels(vict, SKILL_VNUM(skill));
		sprintf(output, "%s's %s set to %d.", GET_NAME(vict), SKILL_NAME(skill), level);
	}
	else if SET_CASE("language") {
		char vnum_arg[MAX_INPUT_LENGTH], onoff_arg[MAX_INPUT_LENGTH];
		generic_data *lang;
		
		half_chop(val_arg, vnum_arg, onoff_arg);
		
		if (!*vnum_arg || !*onoff_arg) {
			msg_to_char(ch, "Usage: set <name> language <vnum | language> <on/speak | recognize | off/unknown>\r\n");
			return 0;
		}
		if (!((isdigit(*vnum_arg) && (lang = find_generic(atoi(vnum_arg), GENERIC_LANGUAGE))) || (lang = find_generic_no_spaces(GENERIC_LANGUAGE, vnum_arg)))) {
			msg_to_char(ch, "Invalid language '%s'.\r\n", vnum_arg);
			return 0;
		}
		if (GEN_FLAGGED(lang, GEN_IN_DEVELOPMENT)) {
			msg_to_char(ch, "Language is set in-development.\r\n");
			return 0;
		}
		
		if (!str_cmp(onoff_arg, "on") || is_abbrev(onoff_arg, "speaks")) {
			add_language(vict, GEN_VNUM(lang), LANG_SPEAK);
			sprintf(output, "%s: now speaks language [%d] %s.", GET_NAME(vict), GEN_VNUM(lang), GEN_NAME(lang));
			check_languages(vict);
		}
		else if (is_abbrev(onoff_arg, "recognize")) {
			if (GET_LOYALTY(vict) && speaks_language_empire(GET_LOYALTY(vict), GEN_VNUM(lang)) == LANG_SPEAK) {
				msg_to_char(ch, "You cannot turn off recognition for that langauge because that player's empire can speak it.\r\n");
				return 0;
			}
			add_language(vict, GEN_VNUM(lang), LANG_RECOGNIZE);
			sprintf(output, "%s: now recognizes language [%d] %s.", GET_NAME(vict), GEN_VNUM(lang), GEN_NAME(lang));
			check_languages(vict);
		}
		else if (!str_cmp(onoff_arg, "off") || is_abbrev(onoff_arg, "unknown")) {
			if (GET_LOYALTY(vict) && speaks_language_empire(GET_LOYALTY(vict), GEN_VNUM(lang)) != LANG_UNKNOWN) {
				msg_to_char(ch, "You cannot turn off that langauge because that player's empire can speak it.\r\n");
				return 0;
			}
			add_language(vict, GEN_VNUM(lang), LANG_UNKNOWN);
			sprintf(output, "%s: no longer speaks [%d] %s.", GET_NAME(vict), GEN_VNUM(lang), GEN_NAME(lang));
			check_languages(vict);
		}
		else {
			msg_to_char(ch, "Do you want to set it on/speak, recognize, or off/unknown?\r\n");
			return 0;
		}
	}
	else if SET_CASE("learned") {
		char vnum_arg[MAX_INPUT_LENGTH], onoff_arg[MAX_INPUT_LENGTH];
		craft_data *cft;
		
		half_chop(val_arg, vnum_arg, onoff_arg);
		
		if (!*vnum_arg || !isdigit(*vnum_arg) || !*onoff_arg) {
			msg_to_char(ch, "Usage: set <name> learned <craft vnum> <on | off>\r\n");
			return 0;
		}
		if (!(cft = craft_proto(atoi(vnum_arg))) || !CRAFT_FLAGGED(cft, CRAFT_LEARNED)) {
			msg_to_char(ch, "Invalid craft (must be LEARNED and not IN-DEV).\r\n");
			return 0;
		}
		
		if (!str_cmp(onoff_arg, "on")) {
			if (CRAFT_FLAGGED(cft, CRAFT_IN_DEVELOPMENT) && !IS_IMMORTAL(vict)) {
				msg_to_char(ch, "Craft must not be IN-DEV to set it on a player.\r\n");
				return 0;
			}
			
			add_learned_craft(ch, GET_CRAFT_VNUM(cft));
			sprintf(output, "%s learned craft %d %s.", GET_NAME(vict), GET_CRAFT_VNUM(cft), GET_CRAFT_NAME(cft));
		}
		else if (!str_cmp(onoff_arg, "off")) {
			remove_learned_craft(ch, GET_CRAFT_VNUM(cft));
			sprintf(output, "%s un-learned craft %d %s.", GET_NAME(vict), GET_CRAFT_VNUM(cft), GET_CRAFT_NAME(cft));
		}
		else {
			msg_to_char(ch, "Do you want to turn it on or off?\r\n");
			return 0;
		}
	}
	else if SET_CASE("minipet") {
		char vnum_arg[MAX_INPUT_LENGTH], onoff_arg[MAX_INPUT_LENGTH];
		char_data *pet;
		
		half_chop(val_arg, vnum_arg, onoff_arg);
		
		if (!*vnum_arg || !isdigit(*vnum_arg) || !*onoff_arg) {
			msg_to_char(ch, "Usage: set <name> minipet <mob vnum> <on | off>\r\n");
			return 0;
		}
		if (!(pet = mob_proto(atoi(vnum_arg)))) {
			msg_to_char(ch, "Invalid mob vnum.\r\n");
			return 0;
		}
		
		if (!str_cmp(onoff_arg, "on")) {
			add_minipet(vict, GET_MOB_VNUM(pet));
			sprintf(output, "%s: gained minipet %d %s.", GET_NAME(vict), GET_MOB_VNUM(pet), GET_SHORT_DESC(pet));
		}
		else if (!str_cmp(onoff_arg, "off")) {
			remove_minipet(vict, GET_MOB_VNUM(pet));
			sprintf(output, "%s: removed minipet %d %s.", GET_NAME(vict), GET_MOB_VNUM(pet), GET_SHORT_DESC(pet));
		}
		else {
			msg_to_char(ch, "Do you want to turn it on or off?\r\n");
			return 0;
		}
	}
	else if SET_CASE("mount") {
		char vnum_arg[MAX_INPUT_LENGTH], onoff_arg[MAX_INPUT_LENGTH];
		struct mount_data *mentry;
		char_data *mount;
		
		half_chop(val_arg, vnum_arg, onoff_arg);
		
		if (!*vnum_arg || !isdigit(*vnum_arg) || !*onoff_arg) {
			msg_to_char(ch, "Usage: set <name> mount <mob vnum> <on | off>\r\n");
			return 0;
		}
		if (!(mount = mob_proto(atoi(vnum_arg)))) {
			msg_to_char(ch, "Invalid mob vnum.\r\n");
			return 0;
		}
		
		if (!str_cmp(onoff_arg, "on")) {
			if (find_mount_data(vict, GET_MOB_VNUM(mount))) {
				msg_to_char(ch, "%s already has that mount.\r\n", GET_NAME(vict));
				return 0;
			}
			add_mount(vict, GET_MOB_VNUM(mount), get_mount_flags_by_mob(mount));
			sprintf(output, "%s: gained mount %d %s.", GET_NAME(vict), GET_MOB_VNUM(mount), GET_SHORT_DESC(mount));
		}
		else if (!str_cmp(onoff_arg, "off")) {
			if ((mentry = find_mount_data(vict, GET_MOB_VNUM(mount)))) {
				HASH_DEL(GET_MOUNT_LIST(vict), mentry);
				free(mentry);
				sprintf(output, "%s: removed mount %d %s.", GET_NAME(vict), GET_MOB_VNUM(mount), GET_SHORT_DESC(mount));
				if (GET_MOUNT_VNUM(vict) == GET_MOB_VNUM(mount)) {
					GET_MOUNT_VNUM(vict) = NOTHING;
				}
			}
			else {
				msg_to_char(ch, "%s does not have that mount.\r\n", GET_NAME(vict));
				return 0;
			}
		}
		else {
			msg_to_char(ch, "Do you want to turn it on or off?\r\n");
			return 0;
		}
	}
	else if SET_CASE("currency") {
		char vnum_arg[MAX_INPUT_LENGTH], amt_arg[MAX_INPUT_LENGTH];
		generic_data *gen;
		int amt;
		
		half_chop(val_arg, vnum_arg, amt_arg);
		
		if (!*vnum_arg || !isdigit(*vnum_arg) || !*amt_arg) {
			msg_to_char(ch, "Usage: set <name> currency <vnum> <amount>\r\n");
			return 0;
		}
		if (!(gen = find_generic(atoi(vnum_arg), GENERIC_CURRENCY))) {
			msg_to_char(ch, "Invalid currency vnum.\r\n");
			return 0;
		}
		if (!isdigit(*amt_arg) || (amt = atoi(amt_arg)) < 0) {
			msg_to_char(ch, "You must set it to zero or greater.\r\n");
			return 0;
		}
		
		amt = add_currency(vict, GEN_VNUM(gen), amt - get_currency(vict, GEN_VNUM(gen)));
		sprintf(output, "%s's %d %s set to %d.", GET_NAME(vict), GEN_VNUM(gen), GEN_NAME(gen), amt);
	}
	else if SET_CASE("unlockedarchetype") {
		char vnum_arg[MAX_INPUT_LENGTH], onoff_arg[MAX_INPUT_LENGTH];
		archetype_data *arch;
		
		half_chop(val_arg, vnum_arg, onoff_arg);
		
		if (!*vnum_arg || !isdigit(*vnum_arg) || !*onoff_arg) {
			msg_to_char(ch, "Usage: set <name> unlockedarchetype <archetype vnum> <on | off>\r\n");
			return 0;
		}
		if (!(arch = archetype_proto(atoi(vnum_arg)))) {
			msg_to_char(ch, "Invalid archetype vnum.\r\n");
			return 0;
		}
		if (ARCHETYPE_FLAGGED(arch, ARCH_IN_DEVELOPMENT)) {
			msg_to_char(ch, "Archetype %d %s is set IN-DEVELOPMENT.\r\n", GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch));
			return 0;
		}
		if (!ARCHETYPE_FLAGGED(arch, ARCH_LOCKED)) {
			msg_to_char(ch, "Archetype %d %s is not LOCKED.\r\n", GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch));
			return 0;
		}
		
		if (!str_cmp(onoff_arg, "on")) {
			add_unlocked_archetype(vict, GET_ARCH_VNUM(arch));
			sprintf(output, "%s: unlocked archetype %d %s.", GET_NAME(vict), GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch));
		}
		else if (!str_cmp(onoff_arg, "off")) {
			remove_unlocked_archetype(vict, GET_ARCH_VNUM(arch));
			sprintf(output, "%s: removed unlocked archetype %d %s.", GET_NAME(vict), GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch));
		}
		else {
			msg_to_char(ch, "Do you want to turn it on or off?\r\n");
			return 0;
		}
	}

	else if SET_CASE("account") {
		if (get_highest_access_level(GET_ACCOUNT(vict)) > GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "You can't edit accounts for people above your access level.\r\n");
			return 0;
		}
		if (!str_cmp(val_arg, "new")) {
			sprintf(output, "%s is now associated with a new account.", GET_NAME(vict));
			remove_player_from_account(vict);
			create_account_for_player(vict);
		}
		else {
			// load 2nd player
			if ((alt = find_or_load_player(val_arg, &file))) {
				if (get_highest_access_level(GET_ACCOUNT(alt)) > GET_ACCESS_LEVEL(ch)) {
					msg_to_char(ch, "You can't edit accounts for people above your access level.\r\n");
					if (file) {
						free_char(alt);
					}
					return 0;
				}
				
				sprintf(output, "%s is now associated with %s's account.", GET_NAME(vict), GET_NAME(alt));
				
				remove_player_from_account(vict);
				// does 2nd player have an account already? if not, make one
				if (!GET_ACCOUNT(alt)) {
					create_account_for_player(alt);
					add_player_to_account(vict, GET_ACCOUNT(alt));

					if (file) {
						SAVE_CHAR(alt);
						// leave them open to avoid a conflict with who you're setting
						// store_loaded_char(alt);
						// file = FALSE;
						// alt = NULL;
					}
					else {
						queue_delayed_update(alt, CDU_SAVE);
					}
				}
				else {
					// already has acct
					add_player_to_account(vict, GET_ACCOUNT(alt));
				}
				
				if (file && alt) {
					// leave the alt open to avoid a conflict with who you're setting
					// free_char(alt);
				}
			}
			else {
				msg_to_char(ch, "Unable to associate account: no such player '%s'.\r\n", val_arg);
				return 0;
			}
		}
		
		if ((emp = GET_LOYALTY(vict))) {
			TRIGGER_DELAYED_REFRESH(emp, DELAY_REFRESH_MEMBERS);
		}
	}

	else if SET_CASE("name") {
		SAVE_CHAR(vict);
		one_argument(val_arg, buf);
		if (_parse_name(buf, newname, ch->desc, TRUE)) {
			// sends own message
			return 0;
		}
		if (!Valid_Name(newname)) {
			msg_to_char(ch, "Invalid name.\r\n");
			return 0;
		}
		found_index = NULL;
		HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
			if (!str_cmp(index->name, newname)) {
				msg_to_char(ch, "That name is already taken.\r\n");
				return 0;
			}
			if (!str_cmp(index->name, GET_NAME(vict))) {
				if (found_index) {
					msg_to_char(ch, "WARNING: possible pfile corruption, more than one entry with that name.\r\n");
				}
				else {
					found_index = index;
				}
			}
		}
		if (!found_index) {
			msg_to_char(ch, "WARNING: That character does not have a record in the pfile.\r\nName not changed.\r\n");
			return 0;
		}
		strcpy(oldname, GET_PC_NAME(vict));
		if (GET_PC_NAME(vict)) {
			free(GET_PC_NAME(vict));
		}
		GET_PC_NAME(vict) = strdup(CAP(newname));
		
		// ensure we really have the right index
		if ((found_index = find_player_index_by_idnum(GET_IDNUM(vict)))) {
			remove_player_from_table(found_index);	// temporary remove
			
			if (found_index->name) {
				free(found_index->name);
			}
			found_index->name = str_dup(GET_PC_NAME(vict));
			strtolower(found_index->name);
			
			// now update the rest of the data
			update_player_index(found_index, vict);
			
			// now re-add to index
			add_player_to_table(found_index);
		}
		
		// rename the save files
		get_filename(oldname, buf1, PLR_FILE);
		get_filename(GET_NAME(vict), buf2, PLR_FILE);
		rename(buf1, buf2);
		get_filename(oldname, buf1, DELAYED_FILE);
		get_filename(GET_NAME(vict), buf2, DELAYED_FILE);
		rename(buf1, buf2);
		get_filename(oldname, buf1, MAP_MEMORY_FILE);
		get_filename(GET_NAME(vict), buf2, MAP_MEMORY_FILE);
		rename(buf1, buf2);
		
		// update msdp
		update_MSDP_name(vict, UPDATE_NOW);
		
		SAVE_CHAR(vict);
		save_library_file_for_vnum(DB_BOOT_ACCT, GET_ACCOUNT(vict)->id);
		sprintf(output, "%s's name changed to %s.", oldname, GET_NAME(vict));
	}
	
	else if SET_CASE("scale") {
		// adjust here to log the right number
		value = MAX(GET_MIN_SCALE_LEVEL(vict), value);
		value = MIN(GET_MAX_SCALE_LEVEL(vict), value);
		scale_mob_to_level(vict, value);
		set_mob_flags(vict, MOB_NO_RESCALE);
	}
	
	// this goes last, after all SET_CASE statements
	else {
		send_to_char("Can't set that!\r\n", ch);
		return (0);
	}
	
	if (*output) {
		if (strstr(output, "%d")) {
			// put value in now if a %d is in the output string
			strcpy(temp, output);
			sprintf(output, temp, value);
		}
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s used set: %s", GET_REAL_NAME(ch), output);
		strcat(output, "\r\n");
		send_to_char(CAP(output), ch);
	}
	return (1);
}


 //////////////////////////////////////////////////////////////////////////////
//// STAT / VSTAT ////////////////////////////////////////////////////////////

/**
* Shows summarized spawn info for any spawn_info list to the character, only
* if there is something to show. No output is shown if there are no spawns.
*
* @param char_data *ch The person who will see the spawn summary.
* @param bool use_page_display If TRUE, stores it to the character's page_display instead of sending it now.
* @param struct spawn_info *list Any list of spawn_info.
*/
void show_spawn_summary_display(char_data *ch, bool use_page_display, struct spawn_info *list) {
	bool any;
	char line[MAX_STRING_LENGTH], entry[MAX_INPUT_LENGTH], flg[MAX_INPUT_LENGTH];
	struct spawn_info *spawn;
	
	if (!ch || !list) {
		return;	// nothing to show
	}
	
	build_page_display_str(ch, "Spawn info:");
	
	// spawns?
	any = FALSE;
	*line = '\0';
	for (spawn = list; spawn; spawn = spawn->next) {
		if (spawn->flags) {
			sprintbit(spawn->flags, spawn_flags_short, flg, TRUE);
			flg[strlen(flg) - 1] = '\0';	// removes the trailing space
			sprintf(entry, " %s (%d) %.2f%% %s", skip_filler(get_mob_name_by_proto(spawn->vnum, FALSE)), spawn->vnum, spawn->percent, flg);
		}
		else {
			// no flags
			sprintf(entry, " %s (%d) %.2f%%", skip_filler(get_mob_name_by_proto(spawn->vnum, FALSE)), spawn->vnum, spawn->percent);
		}
		
		if (any) {
			strcat(line, ",");
		}
		if (strlen(line) + strlen(entry) > 78) {
			build_page_display_str(ch, line);
			*line = '\0';
		}
		strcat(line, entry);
		any = TRUE;
	}
	
	// anything left in the line?
	if (*line) {
		build_page_display_str(ch, line);
	}
	
	if (!use_page_display) {
		send_page_display(ch);
	}
}


/**
* @param char_data *ch The player requesting stats.
* @param adv_data *adv The adventure to display.
*/
void do_stat_adventure(char_data *ch, adv_data *adv) {
	char lbuf[MAX_STRING_LENGTH];
	
	if (!adv) {
		return;
	}
	
	build_page_display(ch, "VNum: [&c%d&0], Name: &c%s&0 (by &c%s&0)", GET_ADV_VNUM(adv), GET_ADV_NAME(adv), GET_ADV_AUTHOR(adv));
	build_page_display_str(ch, NULLSAFE(GET_ADV_DESCRIPTION(adv)));
	
	build_page_display(ch, "VNum range: [&c%d&0-&c%d&0], Level range: [&c%d&0-&c%d&0]", GET_ADV_START_VNUM(adv), GET_ADV_END_VNUM(adv), GET_ADV_MIN_LEVEL(adv), GET_ADV_MAX_LEVEL(adv));

	// reset time display helper
	if (GET_ADV_RESET_TIME(adv) == 0) {
		strcpy(lbuf, "never");
	}
	else if (GET_ADV_RESET_TIME(adv) > 60) {
		strcpy(lbuf, colon_time(GET_ADV_RESET_TIME(adv), TRUE, NULL));
	}
	else {
		sprintf(lbuf, "%d min", GET_ADV_RESET_TIME(adv));
	}
	
	build_page_display(ch, "Instance limit: [&c%d&0/&c%d&0 (&c%d&0)], Player limit: [&c%d&0], Reset time: [&c%s&0]", count_instances(adventure_proto(GET_ADV_VNUM(adv))), adjusted_instance_limit(adv), GET_ADV_MAX_INSTANCES(adv), GET_ADV_PLAYER_LIMIT(adv), lbuf);
	
	sprintbit(GET_ADV_FLAGS(adv), adventure_flags, lbuf, TRUE);
	build_page_display(ch, "Flags: &g%s&0", lbuf);
	
	build_page_display(ch, "Temperature: [\tc%s\t0]", temperature_types[GET_ADV_TEMPERATURE_TYPE(adv)]);
	
	build_page_display_str(ch, "Linking rules:");
	show_adventure_linking_display(ch, GET_ADV_LINKING(adv), FALSE);
	
	if (GET_ADV_SCRIPTS(adv)) {
		build_page_display(ch, "Scripts:");
		show_script_display(ch, GET_ADV_SCRIPTS(adv), FALSE);
	}
	else {
		build_page_display_str(ch, "Scripts: none");
	}
	
	send_page_display(ch);
}


/**
* Show the stats on a book.
*
* @param char_data *ch The player requesting stats.
* @param book_data *book The book to stat.
* @param bool details If TRUE, sends whole paragraphs. FALSE just sends previews.
*/
void do_stat_book(char_data *ch, book_data *book, bool details) {
	char line[MAX_STRING_LENGTH];
	struct paragraph_data *para;
	player_index_data *index;
	int count, len, num;
	char *ptr, *txt;
	
	build_page_display(ch, "Book VNum: [\tc%d\t0], Author: \ty%s\t0 (\tc%d\t0)", BOOK_VNUM(book), (index = find_player_index_by_idnum(BOOK_AUTHOR(book))) ? index->fullname : "nobody", BOOK_AUTHOR(book));
	build_page_display(ch, "Title: %s\t0", BOOK_TITLE(book));
	build_page_display(ch, "Byline: %s\t0", BOOK_BYLINE(book));
	build_page_display(ch, "Item: [%s]", BOOK_ITEM_NAME(book));
	build_page_display_str(ch, NULLSAFE(BOOK_ITEM_DESC(book)));	// desc has its own crlf
	
	// precompute number of paragraphs
	num = 0;
	LL_FOREACH(BOOK_PARAGRAPHS(book), para) {
		++num;
	}
	
	count = 0;
	LL_FOREACH(BOOK_PARAGRAPHS(book), para) {
		++count;
		
		if (details) {
			build_page_display(ch, "\r\nParagraph %d:\r\n%s", count, para->text);
		}
		else {
			// just previews
			txt = para->text;
			skip_spaces(&txt);
			len = strlen(txt);
			len = MIN(len, 62);	// aiming for full page width then a ...
			safe_snprintf(line, sizeof(line), "Paragraph %*d: %-*.*s...", (num >= 10 ? 2 : 1), count, len, len, txt);
			if ((ptr = strstr(line, "\r\n"))) {	// line ended early?
				sprintf(ptr, "...");	// overwrite the crlf
			}
			
			build_page_display_str(ch, line);
		}
	}
	
	send_page_display(ch);
}


/**
* Show a character stats on a particular building.
*
* @param char_data *ch The player requesting stats.
* @param bld_data *bdg The building to stat.
* @param bool details If TRUE, shows full messages (due to -d option on vstat).
*/
void do_stat_building(char_data *ch, bld_data *bdg, bool details) {
	char lbuf[MAX_STRING_LENGTH];
	struct page_display *line;
	
	build_page_display(ch, "Building VNum: [&c%d&0], Name: '&c%s&0'", GET_BLD_VNUM(bdg), GET_BLD_NAME(bdg));
	
	build_page_display(ch, "Room Title: %s", GET_BLD_TITLE(bdg));
	
	// icon line
	line = build_page_display(ch, "Icon: %s&0", GET_BLD_ICON(bdg) ? one_icon_display(GET_BLD_ICON(bdg), NULL) : "none");
	if (GET_BLD_HALF_ICON(bdg)) {
		append_page_display_line(line, "  Half Icon: %s&0", GET_BLD_HALF_ICON(bdg) ? one_icon_display(GET_BLD_HALF_ICON(bdg), NULL) : "none");
	}
	if (GET_BLD_QUARTER_ICON(bdg)) {
		append_page_display_line(line, "  Quarter Icon: %s&0", GET_BLD_QUARTER_ICON(bdg) ? one_icon_display(GET_BLD_QUARTER_ICON(bdg), NULL) : "none");
	}
	
	if (GET_BLD_DESC(bdg) && *GET_BLD_DESC(bdg)) {
		build_page_display(ch, "Description:\r\n%s", GET_BLD_DESC(bdg));
	}
	
	if (GET_BLD_COMMANDS(bdg) && *GET_BLD_COMMANDS(bdg)) {
		build_page_display(ch, "Command list: &c%s&0", GET_BLD_COMMANDS(bdg));
	}
	
	build_page_display(ch, "Hitpoints: [&g%d&0], Fame: [&g%d&0], Extra Rooms: [&g%d&0], Height: [&g%d&0]", GET_BLD_MAX_DAMAGE(bdg), GET_BLD_FAME(bdg), GET_BLD_EXTRA_ROOMS(bdg), GET_BLD_HEIGHT(bdg));
	build_page_display(ch, "Citizens: [&g%d&0], Military: [&g%d&0], Artisan: [&g%d&0] &c%s&0", GET_BLD_CITIZENS(bdg), GET_BLD_MILITARY(bdg), GET_BLD_ARTISAN(bdg), GET_BLD_ARTISAN(bdg) != NOTHING ? get_mob_name_by_proto(GET_BLD_ARTISAN(bdg), FALSE) : "none");
	
	if (GET_BLD_RELATIONS(bdg)) {
		build_page_display_str(ch, "Relations:");
		show_bld_relations_display(ch, GET_BLD_RELATIONS(bdg), FALSE);
	}
	
	sprintbit(GET_BLD_FLAGS(bdg), bld_flags, lbuf, TRUE);
	build_page_display(ch, "Building flags: &c%s&0", lbuf);
	
	sprintbit(GET_BLD_FUNCTIONS(bdg), function_flags, lbuf, TRUE);
	build_page_display(ch, "Functions: &g%s&0", lbuf);
	
	sprintbit(GET_BLD_DESIGNATE_FLAGS(bdg), designate_flags, lbuf, TRUE);
	build_page_display(ch, "Designate flags: &c%s&0", lbuf);
	
	sprintbit(GET_BLD_BASE_AFFECTS(bdg), room_aff_bits, lbuf, TRUE);
	build_page_display(ch, "Base affects: &g%s&0", lbuf);
	
	build_page_display(ch, "Temperature: [\tc%s\t0]", temperature_types[GET_BLD_TEMPERATURE_TYPE(bdg)]);
	
	if (GET_BLD_EX_DESCS(bdg)) {
		struct extra_descr_data *desc;
		
		if (details) {
			build_page_display_str(ch, "Extra descs:");
			LL_FOREACH(GET_BLD_EX_DESCS(bdg), desc) {
				build_page_display(ch, "[ &c%s&0 ]\r\n%s", desc->keyword, desc->description);
			}
		}
		else {
			sprintf(lbuf, "Extra descs:&c");
			LL_FOREACH(GET_BLD_EX_DESCS(bdg), desc) {
				strcat(lbuf, " ");
				strcat(lbuf, desc->keyword);
			}
			strcat(lbuf, "&0");
			build_page_display_str(ch, lbuf);
		}
	}
	
	if (GET_BLD_INTERACTIONS(bdg)) {
		build_page_display_str(ch, "Interactions:");
		show_interaction_display(ch, GET_BLD_INTERACTIONS(bdg), FALSE);
	}
	
	if (GET_BLD_REGULAR_MAINTENANCE(bdg)) {
		build_page_display_str(ch, "Regular maintenance:");
		show_resource_display(ch, GET_BLD_REGULAR_MAINTENANCE(bdg), FALSE);
	}
	
	if (GET_BLD_SCRIPTS(bdg)) {
		build_page_display_str(ch, "Scripts:");
		show_script_display(ch, GET_BLD_SCRIPTS(bdg), FALSE);
	}
	else {
		build_page_display_str(ch, "Scripts: none");
	}
	
	show_spawn_summary_display(ch, TRUE, GET_BLD_SPAWNS(bdg));
	
	send_page_display(ch);
}


/**
* Sends ch information on the character or mobile k.
*
* @param char_data *ch The person getting stats.
* @param char_data *k The player or mob to view stats on.
* @param bool details If TRUE, shows full messages (due to -d option on vstat).
*/
void do_stat_character(char_data *ch, char_data *k, bool details) {
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH], lbuf2[MAX_STRING_LENGTH], lbuf3[MAX_STRING_LENGTH];
	struct script_memory *mem;
	struct cooldown_data *cool;
	int count, i, i2, iter, diff, duration, found = 0, val;
	obj_data *j;
	struct follow_type *fol;
	struct over_time_effect_type *dot;
	struct affected_type *aff;
	archetype_data *arch;
	struct page_display *line;
	
	bool is_proto = (IS_NPC(k) && k == mob_proto(GET_MOB_VNUM(k)));
	
	// ensure fully loaded
	check_delayed_load(k);

	sprinttype(GET_REAL_SEX(k), genders, lbuf, sizeof(lbuf), "???");
	CAP(lbuf);
	if (!IS_NPC(k)) {
		build_page_display(ch, "%s PC '\ty%s\t0', Lastname '\ty%s\t0', IDNum: [%5d], In room [%5d]", lbuf, GET_NAME(k), GET_CURRENT_LASTNAME(k) ? GET_CURRENT_LASTNAME(k) : "none", GET_IDNUM(k), IN_ROOM(k) ? GET_ROOM_VNUM(IN_ROOM(k)) : NOWHERE);
	}
	else {	// mob
		build_page_display(ch, "%s %s '\ty%s\t0', ID: [%5d], In room [%5d]", lbuf, (!IS_MOB(k) ? "NPC" : "MOB"), GET_NAME(k), k->script_id, IN_ROOM(k) ? GET_ROOM_VNUM(IN_ROOM(k)) : NOWHERE);
	}
	
	if (!IS_NPC(k) && GET_ACCOUNT(k)) {
		if (GET_ACCESS_LEVEL(ch) >= LVL_TO_SEE_ACCOUNTS) {
			sprintbit(GET_ACCOUNT(k)->flags, account_flags, lbuf, TRUE);
			build_page_display(ch, "Account: [%d], Flags: &g%s&0", GET_ACCOUNT(k)->id, lbuf);
		}
		else {	// low-level imms only see certain account flags
			sprintbit(GET_ACCOUNT(k)->flags & VISIBLE_ACCT_FLAGS, account_flags, lbuf, TRUE);
			build_page_display(ch, "Account: &g%s&0", lbuf);
		}
	}
	
	if (k->desc) {
		// protocol info
		build_page_display(ch, "Connection info: Client: [%s], X-Colors: [%s\t0], MSDP: [%s\t0],", NULLSAFE(k->desc->pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString), ((k->desc->pProtocol->b256Support || k->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt) ? "\tgyes" : "\trno"), (k->desc->pProtocol->bMSDP ? "\tgyes" : "\trno"));
		build_page_display(ch, "   MSP: [%s\t0], MXP: [%s\t0], NAWS: [%s\t0], Screen: [%dx%d]", ((k->desc->pProtocol->bMSP || k->desc->pProtocol->pVariables[eMSDP_SOUND]->ValueInt) ? "\tgyes" : "\trno"), ((k->desc->pProtocol->bMXP || k->desc->pProtocol->pVariables[eMSDP_MXP]->ValueInt) ? "\tgyes" : "\trno"), (k->desc->pProtocol->bNAWS ? "\tgyes" : "\trno"), k->desc->pProtocol->ScreenWidth, k->desc->pProtocol->ScreenHeight);
	}
	
	if (IS_MOB(k)) {
		build_page_display(ch, "Alias: &y%s&0, VNum: [&c%5d&0]", GET_PC_NAME(k), GET_MOB_VNUM(k));
		build_page_display(ch, "L-Des: &y%s&0%s", (GET_LONG_DESC(k) ? GET_LONG_DESC(k) : "<None>\r\n"), NULLSAFE(GET_LOOK_DESC(k)));
	}

	if (IS_NPC(k)) {
		build_page_display(ch, "Scaled level: [&c%d&0|&c%d&0-&c%d&0], Faction: [\tt%s\t0]", GET_CURRENT_SCALE_LEVEL(k), GET_MIN_SCALE_LEVEL(k), GET_MAX_SCALE_LEVEL(k), MOB_FACTION(k) ? FCT_NAME(MOB_FACTION(k)) : "none");
	}
	else {	// not NPC
		build_page_display(ch, "Title: %s&0", (GET_TITLE(k) ? GET_TITLE(k) : "<None>"));
		
		if (GET_REFERRED_BY(k) && *GET_REFERRED_BY(k)) {
			build_page_display(ch, "Referred by: %s", GET_REFERRED_BY(k));
		}
		if (GET_PROMO_ID(k) > 0) {
			build_page_display(ch, "Promo code: %s", promo_codes[GET_PROMO_ID(k)].code);
		}

		get_player_skill_string(k, lbuf, TRUE);
		build_page_display(ch, "Access Level: [&c%d&0], Class: [%s/&c%s&0], Skill Level: [&c%d&0], Gear Level: [&c%d&0], Total: [&c%d&0/&c%d&0]", GET_ACCESS_LEVEL(k), lbuf, class_role[(int) GET_CLASS_ROLE(k)], GET_SKILL_LEVEL(k), GET_GEAR_LEVEL(k), IN_ROOM(k) ? GET_COMPUTED_LEVEL(k) : GET_LAST_KNOWN_LEVEL(k), GET_HIGHEST_KNOWN_LEVEL(k));
		
		line = build_page_display(ch, "Archetypes:");
		for (iter = 0, count = 0; iter < NUM_ARCHETYPE_TYPES; ++iter) {
			if ((arch = archetype_proto(CREATION_ARCHETYPE(k, iter)))) {
				append_page_display_line(line, "%s%s", (count++ > 0) ? ", " : " ", GET_ARCH_NAME(arch));
			}
		}
		append_page_display_line(line, "%s", count ? "" : " none");
		
		coin_string(GET_PLAYER_COINS(k), lbuf);
		build_page_display(ch, "Coins: %s", lbuf);

		strcpy(buf1, (char *) asctime(localtime(&(k->player.time.birth))));
		strcpy(buf2, buf1 + 20);
		buf1[10] = '\0';	// DoW Mon Day
		buf2[4] = '\0';	// get only year

		build_page_display(ch, "Created: [%s, %s], Played [%dh %dm], Age [%d]", buf1, buf2, k->player.time.played / SECS_PER_REAL_HOUR, ((k->player.time.played % SECS_PER_REAL_HOUR) / SECS_PER_REAL_MIN), age(k)->year);
		if (get_highest_access_level(GET_ACCOUNT(k)) <= GET_ACCESS_LEVEL(ch) && GET_ACCESS_LEVEL(ch) >= LVL_TO_SEE_ACCOUNTS) {
			build_page_display(ch, "Created from host: [%s]", NULLSAFE(GET_CREATION_HOST(k)));
		}
		
		if (GET_ACCESS_LEVEL(k) >= LVL_BUILDER) {
			sprintbit(GET_OLC_FLAGS(k), olc_flag_bits, lbuf, TRUE);
			build_page_display(ch, "OLC Vnums: [&c%d-%d&0], Flags: &g%s&0", GET_OLC_MIN_VNUM(k), GET_OLC_MAX_VNUM(k), lbuf);
		}
	}

	if (!IS_NPC(k) || GET_CURRENT_SCALE_LEVEL(k) > 0) {
		build_page_display(ch, "Health: [&g%d&0/&g%d&0]  Move: [&g%d&0/&g%d&0]  Mana: [&g%d&0/&g%d&0]  Blood: [&g%d&0/&g%d&0]", GET_HEALTH(k), GET_MAX_HEALTH(k), GET_MOVE(k), GET_MAX_MOVE(k), GET_MANA(k), GET_MAX_MANA(k), GET_BLOOD(k), GET_MAX_BLOOD(k));

		build_page_display_str(ch, display_attributes(k));

		// dex is removed from dodge to make it easier to compare to caps
		val = get_dodge_modifier(k, NULL, FALSE) - (hit_per_dex * GET_DEXTERITY(k));;
		sprintf(lbuf, "Dodge  [%s%d&0]", HAPPY_COLOR(val, 0), val);
		
		val = get_block_rating(k, FALSE);
		sprintf(lbuf2, "Block  [%s%d&0]", HAPPY_COLOR(val, 0), val);
		
		sprintf(lbuf3, "Resist  [%d|%d]", GET_RESIST_PHYSICAL(k), GET_RESIST_MAGICAL(k));
		build_page_display(ch, "  %-28.28s %-28.28s %-28.28s", lbuf, lbuf2, lbuf3);
	
		sprintf(lbuf, "Physical  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_PHYSICAL(k), 0), GET_BONUS_PHYSICAL(k));
		sprintf(lbuf2, "Magical  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_MAGICAL(k), 0), GET_BONUS_MAGICAL(k));
		sprintf(lbuf3, "Healing  [%s%+d&0]", HAPPY_COLOR(GET_BONUS_HEALING(k), 0), GET_BONUS_HEALING(k));
		build_page_display(ch, "  %-28.28s %-28.28s %-28.28s", lbuf, lbuf2, lbuf3);

		// dex is removed from to-hit to make it easier to compare to caps
		val = get_to_hit(k, NULL, FALSE, FALSE) - (hit_per_dex * GET_DEXTERITY(k));;
		sprintf(lbuf, "To-hit  [%s%d&0]", HAPPY_COLOR(val, base_hit_chance), val);
		sprintf(lbuf2, "Speed  [&0%.2f&0]", get_combat_speed(k, WEAR_WIELD));
		sprintf(lbuf3, "Crafting  [%s%d&0]", HAPPY_COLOR(get_crafting_level(k), IS_NPC(k) ? get_approximate_level(k) : GET_SKILL_LEVEL(k)), get_crafting_level(k));
		build_page_display(ch, "  %-28.28s %-28.28s %-28.28s", lbuf, lbuf2, lbuf3);
		
		if (IS_NPC(k)) {
			sprintf(lbuf, "Mob-dmg  [%d]", MOB_DAMAGE(k));
			sprintf(lbuf2, "Mob-hit  [%d]", MOB_TO_HIT(k));
			sprintf(lbuf3, "Mob-dodge  [%d]", MOB_TO_DODGE(k));
			build_page_display(ch, "  %-24.24s %-24.24s %-24.24s", lbuf, lbuf2, lbuf3);
			
			build_page_display(ch, "NPC Bare Hand Dam: %d", MOB_DAMAGE(k));
		}
	}

	sprinttype(GET_POS(k), position_types, buf2, sizeof(buf2), "UNDEFINED");
	line = build_page_display(ch, "Pos: %s, Fighting: %s", buf2, (FIGHTING(k) ? GET_NAME(FIGHTING(k)) : "Nobody"));

	if (IS_NPC(k)) {
		append_page_display_line(line, ", Attack: %d %s, Move: %s, Size: %s", MOB_ATTACK_TYPE(k), get_attack_name_by_vnum(MOB_ATTACK_TYPE(k)), mob_move_types[(int)MOB_MOVE_TYPE(k)], size_types[GET_SIZE(k)]);
	}
	if (k->desc) {
		sprinttype(STATE(k->desc), connected_types, buf2, sizeof(buf2), "UNDEFINED");
		append_page_display_line(line, ", Connected: %s", buf2);
	}

	if (IS_NPC(k)) {
		sprintbit(MOB_FLAGS(k), action_bits, buf2, TRUE);
		build_page_display(ch, "NPC flags: &c%s&0", buf2);
		build_page_display(ch, "Nameset: \ty%s\t0, Language: [\tc%d\t0] \ty%s\t0", name_sets[MOB_NAME_SET(k)], MOB_LANGUAGE(k), get_generic_name_by_vnum(MOB_LANGUAGE(k)));
	}
	else {
		build_page_display(ch, "Idle Timer (minutes) [\tg%.1f\t0], View Height: [\tg%d\t0]", GET_IDLE_SECONDS(k) / (double) SECS_PER_REAL_MIN, get_view_height(k, IN_ROOM(k)));
		
		sprintbit(PLR_FLAGS(k), player_bits, buf2, TRUE);
		build_page_display(ch, "PLR: &c%s&0", buf2);
		
		sprintbit(PRF_FLAGS(k), preference_bits, buf2, TRUE);
		build_page_display(ch, "PRF: &g%s&0", buf2);
		
		sprintbit(GET_BONUS_TRAITS(k), bonus_bits, buf2, TRUE);
		build_page_display(ch, "BONUS: &c%s&0", buf2);
		
		prettier_sprintbit(GET_GRANT_FLAGS(k), grant_bits, buf2);
		build_page_display(ch, "GRANTS: &g%s&0", buf2);
		
		sprintbit(SYSLOG_FLAGS(k), syslog_types, buf2, TRUE);
		build_page_display(ch, "SYSLOGS: &c%s&0", buf2);
	}

	if (!is_proto) {
		line = build_page_display(ch, "Carried items: %d/%d; ", IS_CARRYING_N(k), CAN_CARRY_N(k));
		DL_COUNT2(k->carrying, j, i, next_content);	// TODO these var names are absurd
		append_page_display_line(line, "Items in: inventory: %d, ", i);

		for (i = 0, i2 = 0; i < NUM_WEARS; i++) {
			if (GET_EQ(k, i)) {
				i2++;
			}
		}
		append_page_display_line(line, "eq: %d", i2);
	}

	if (IS_NPC(k) && k->interactions) {
		build_page_display_str(ch, "Interactions:");
		show_interaction_display(ch, k->interactions, FALSE);
	}
	
	if (MOB_CUSTOM_MSGS(k)) {
		struct custom_message *mcm;
		
		if (details) {
			build_page_display(ch, "Custom messages:");
			LL_FOREACH(MOB_CUSTOM_MSGS(k), mcm) {
				build_page_display(ch, " %s: %s", mob_custom_types[mcm->type], mcm->msg);
			}
		}
		else {
			LL_COUNT(MOB_CUSTOM_MSGS(k), mcm, count);
			build_page_display(ch, "Custom messages: %d", count);
		}
	}

	if (!IS_NPC(k)) {
		build_page_display(ch, "Hunger: %d, Thirst: %d, Drunk: %d", GET_COND(k, FULL), GET_COND(k, THIRST), GET_COND(k, DRUNK));
		build_page_display(ch, "Temperature: %d (%s), Warmth: %d, Cooling: %d", get_relative_temperature(k), temperature_to_string(get_relative_temperature(k)), GET_WARMTH(k), GET_COOLING(k));
		build_page_display(ch, "Speaking: %s, Recent deaths: %d", get_generic_name_by_vnum(GET_SPEAKING(k)), GET_RECENT_DEATH_COUNT(k));
	}
	
	if (IS_MORPHED(k)) {
		build_page_display(ch, "Morphed into: %d - %s", MORPH_VNUM(GET_MORPH(k)), get_morph_desc(k, FALSE));
	}
	if (IS_DISGUISED(k)) {
		build_page_display(ch, "Disguised as: %s", GET_DISGUISED_NAME(k));
	}

	if (!is_proto) {
		sprintf(lbuf, "Leader is: %s, Followers are:", ((GET_LEADER(k)) ? GET_NAME(GET_LEADER(k)) : "<none>"));
		for (fol = k->followers; fol; fol = fol->next) {
			sprintf(buf2, "%s %s", found++ ? "," : "", PERS(fol->follower, ch, 1));
			strcat(lbuf, buf2);
			if (strlen(lbuf) >= 62) {
				if (fol->next) {
					build_page_display(ch, "%s,", lbuf);
				}
				else {
					build_page_display_str(ch, lbuf);
				}
				*lbuf = found = 0;
			}
		}

		if (*lbuf) {
			build_page_display_str(ch, lbuf);
		}
	}

	// cooldowns
	if (k->cooldowns) {
		found = FALSE;
		line = build_page_display(ch, "Cooldowns: ");
		
		for (cool = k->cooldowns; cool; cool = cool->next) {
			diff = cool->expire_time - time(0);
			
			if (diff > 0) {
				append_page_display_line(line, "%s&c%s&0 %s", (found ? ", ": ""), get_generic_name_by_vnum(cool->type), colon_time(diff, FALSE, NULL));
				
				found = TRUE;
			}
		}
		
		if (!found) {
			append_page_display_line(line, "none");
		}
	}

	/* Showing the bitvector */
	sprintbit(AFF_FLAGS(k), affected_bits, buf2, TRUE);
	build_page_display(ch, "AFF: &c%s&0", buf2);

	/* Routine to show what spells a char is affected by */
	if (k->affected) {
		for (aff = k->affected; aff; aff = aff->next) {
			*buf2 = '\0';
			
			// duration setup
			if (aff->expire_time == UNLIMITED) {
				strcpy(lbuf, "infinite");
			}
			else if (AFFECTS_CONVERTED(k)) {
				duration = aff->expire_time - time(0);
				duration = MAX(duration, 0);
				strcpy(lbuf, colon_time(duration, FALSE, NULL));
			}
			else {
				// still in seconds
				strcpy(lbuf, colon_time(aff->expire_time, FALSE, NULL));
			}

			sprintf(buf, "TYPE: (%s) &c%s&0 ", lbuf, get_generic_name_by_vnum(aff->type));

			if (aff->modifier) {
				sprintf(buf2, "%+d to %s", aff->modifier, apply_types[(int) aff->location]);
				strcat(buf, buf2);
			}
			if (aff->bitvector) {
				if (*buf2)
					strcat(buf, ", sets ");
				else
					strcat(buf, "sets ");
				sprintbit(aff->bitvector, affected_bits, buf2, TRUE);
				strcat(buf, buf2);
			}
			build_page_display_str(ch, buf);
		}
	}
	
	// dots
	for (dot = k->over_time_effects; dot; dot = dot->next) {
		build_page_display(ch, "TYPE: (%s) &r%s&0 %d %s damage (%d/%d)", colon_time(dot->time_remaining, FALSE, NULL), get_generic_name_by_vnum(dot->type), dot->damage * dot->stack, damage_types[dot->damage_type], dot->stack, dot->max_stack);
	}

	/* check mobiles for a script */
	if (IS_NPC(k)) {
		do_sstat_character(ch, k);
		if (SCRIPT_MEM(k)) {
			mem = SCRIPT_MEM(k);
			build_page_display_str(ch, "Script memory:\r\n  Remember             Command");
			while (mem) {
				char_data *mc = find_char(mem->id);
				if (!mc) {
					build_page_display(ch, "  ** Corrupted!");
				}
				else {
					if (mem->cmd) {
						build_page_display(ch, "  %-20.20s%s", PERS(mc, mc, TRUE), mem->cmd);
					}
					else {
						build_page_display(ch, "  %-20.20s <default>", PERS(mc, mc, TRUE));
					}
				}
				
				mem = mem->next;
			}
		}
	}
	
	send_page_display(ch);
}


void do_stat_craft(char_data *ch, craft_data *craft) {
	char lbuf[MAX_STRING_LENGTH];
	ability_data *abil;
	bld_data *bld;
	int seconds;
	struct page_display *line;
	
	build_page_display(ch, "Name: '&y%s&0', Vnum: [&g%d&0], Type: &c%s&0", GET_CRAFT_NAME(craft), GET_CRAFT_VNUM(craft), craft_types[GET_CRAFT_TYPE(craft)]);
	
	if (CRAFT_IS_BUILDING(craft)) {
		bld = building_proto(GET_CRAFT_BUILD_TYPE(craft));
		build_page_display(ch, "Builds: [&c%d&0] %s", GET_CRAFT_BUILD_TYPE(craft), bld ? GET_BLD_NAME(bld) : "UNKNOWN");
	}
	else if (CRAFT_IS_VEHICLE(craft)) {
		build_page_display(ch, "Creates Vehicle: [&c%d&0] %s", GET_CRAFT_OBJECT(craft), (GET_CRAFT_OBJECT(craft) == NOTHING ? "NOTHING" : get_vehicle_name_by_proto(GET_CRAFT_OBJECT(craft))));
	}
	else if (CRAFT_FLAGGED(craft, CRAFT_SOUP)) {
		build_page_display(ch, "Creates Volume: [&g%d drink%s&0], Liquid: [&g%d&0] %s", GET_CRAFT_QUANTITY(craft), PLURAL(GET_CRAFT_QUANTITY(craft)), GET_CRAFT_OBJECT(craft), get_generic_string_by_vnum(GET_CRAFT_OBJECT(craft), GENERIC_LIQUID, GSTR_LIQUID_NAME));
	}
	else {
		build_page_display(ch, "Creates Quantity: [&g%d&0], Item: [&c%d&0] %s", GET_CRAFT_QUANTITY(craft), GET_CRAFT_OBJECT(craft), get_obj_name_by_proto(GET_CRAFT_OBJECT(craft)));
	}
	
	sprintf(lbuf, "[%d] %s", GET_CRAFT_ABILITY(craft), (GET_CRAFT_ABILITY(craft) == NO_ABIL ? "none" : get_ability_name_by_vnum(GET_CRAFT_ABILITY(craft))));
	if ((abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft))) && ABIL_ASSIGNED_SKILL(abil) != NULL) {
		sprintf(lbuf + strlen(lbuf), " ([%d] %s %d)", SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil)), SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
	}
	line = build_page_display(ch, "Ability: &y%s&0, Level: &g%d&0", lbuf, GET_CRAFT_MIN_LEVEL(craft));
	
	if (!CRAFT_IS_BUILDING(craft) && !CRAFT_IS_VEHICLE(craft)) {
		seconds = GET_CRAFT_TIME(craft) * ACTION_CYCLE_TIME;
		append_page_display_line(line, ", Time: [&g%d action tick%s&0 | &g%s&0]", GET_CRAFT_TIME(craft), PLURAL(GET_CRAFT_TIME(craft)), colon_time(seconds, FALSE, NULL));
	}

	sprintbit(GET_CRAFT_FLAGS(craft), craft_flags, lbuf, TRUE);
	build_page_display(ch, "Flags: &c%s&0", lbuf);
	
	prettier_sprintbit(GET_CRAFT_REQUIRES_TOOL(craft), tool_flags, lbuf);
	build_page_display(ch, "Requires tool: &y%s&0", lbuf);
	
	sprintbit(GET_CRAFT_REQUIRES_FUNCTION(craft), function_flags, lbuf, TRUE);
	build_page_display(ch, "Requires Functions: \tg%s\t0", lbuf);
	
	if (CRAFT_IS_BUILDING(craft) || CRAFT_IS_VEHICLE(craft)) {
		ordered_sprintbit(GET_CRAFT_BUILD_ON(craft), bld_on_flags, bld_on_flags_order, TRUE, lbuf);
		build_page_display(ch, "Build on: &g%s&0", lbuf);
		ordered_sprintbit(GET_CRAFT_BUILD_FACING(craft), bld_on_flags, bld_on_flags_order, TRUE, lbuf);
		build_page_display(ch, "Build facing: &c%s&0", lbuf);
	}
	
	if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING) {
		build_page_display(ch, "Requires item: [%d] &g%s&0", GET_CRAFT_REQUIRES_OBJ(craft), skip_filler(get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(craft))));
	}

	// resources
	build_page_display_str(ch, "Resources required:");
	show_resource_display(ch, GET_CRAFT_RESOURCES(craft), FALSE);
	
	send_page_display(ch);
}


/**
* Show a character stats on a particular crop.
*
* @param char_data *ch The player requesting stats.
* @param crop_data *cp The crop to stat.
* @param bool details If TRUE, shows full messages (due to -d option on vstat).
*/
void do_stat_crop(char_data *ch, crop_data *cp, bool details) {
	int count;
	struct custom_message *ocm;
	
	build_page_display(ch, "Crop VNum: [&c%d&0], Name: '&c%s&0'", GET_CROP_VNUM(cp), GET_CROP_NAME(cp));
	build_page_display(ch, "Room Title: %s, Mapout Color: %s", GET_CROP_TITLE(cp), mapout_color_names[GET_CROP_MAPOUT(cp)]);
	
	ordered_sprintbit(GET_CROP_CLIMATE(cp), climate_flags, climate_flags_order, (CROP_FLAGGED(cp, CROPF_ANY_LISTED_CLIMATE) ? TRUE : FALSE), buf);
	build_page_display(ch, "Climate: &c%s&0", GET_CROP_CLIMATE(cp) ? buf : "(none)");
	
	sprintbit(GET_CROP_FLAGS(cp), crop_flags, buf, TRUE);
	build_page_display(ch, "Crop flags: &g%s&0", buf);
	
	if (GET_CROP_ICONS(cp)) {
		build_page_display_str(ch, "Icons:");
		show_icons_display(ch, GET_CROP_ICONS(cp), FALSE);
	}
	
	build_page_display(ch, "Location: X-Min: [&g%d&0], X-Max: [&g%d&0], Y-Min: [&g%d&0], Y-Max: [&g%d&0]", GET_CROP_X_MIN(cp), GET_CROP_X_MAX(cp), GET_CROP_Y_MIN(cp), GET_CROP_Y_MAX(cp));
	
	if (GET_CROP_EX_DESCS(cp)) {
		struct extra_descr_data *desc;
		
		if (details) {
			build_page_display(ch, "Extra descs:");
			LL_FOREACH(GET_CROP_EX_DESCS(cp), desc) {
				build_page_display(ch, "[ &c%s&0 ]\r\n%s", desc->keyword, desc->description);
			}
		}
		else {
			sprintf(buf, "Extra descs:&c");
			LL_FOREACH(GET_CROP_EX_DESCS(cp), desc) {
				strcat(buf, " ");
				strcat(buf, desc->keyword);
			}
			build_page_display(ch, "%s&0", buf);
		}
	}
	
	if (GET_CROP_CUSTOM_MSGS(cp)) {
		if (details) {
			build_page_display(ch, "Custom messages:");
			LL_FOREACH(GET_CROP_CUSTOM_MSGS(cp), ocm) {
				build_page_display(ch, " %s: %s", crop_custom_types[ocm->type], ocm->msg);
			}
		}
		else {
			LL_COUNT(GET_CROP_CUSTOM_MSGS(cp), ocm, count);
			build_page_display(ch, "Custom messages: %d", count);
		}
	}
	
	if (GET_CROP_INTERACTIONS(cp)) {
		build_page_display_str(ch, "Interactions:");
		show_interaction_display(ch, GET_CROP_INTERACTIONS(cp), FALSE);
	}
	
	show_spawn_summary_display(ch, TRUE, GET_CROP_SPAWNS(cp));
	
	send_page_display(ch);
}


/**
* Shows immortal stats on an empire.
*
* @param char_data *ch The person viewing the stats.
* @param empire_data *emp The empire.
*/
void do_stat_empire(char_data *ch, empire_data *emp) {
	empire_data *emp_iter, *next_emp;
	int iter, found_rank, total, len, *ptime;
	player_index_data *index;
	char line[256];
	bool any;
	struct page_display *pline;
	
	// determine rank by iterating over the sorted empire list
	found_rank = 0;
	HASH_ITER(hh, empire_table, emp_iter, next_emp) {
		++found_rank;
		if (emp == emp_iter) {
			break;
		}
	}
	
	build_page_display(ch, "%s%s\t0, Adjective: [%s%s\t0], VNum: [\tc%5d\t0]", EMPIRE_BANNER(emp), EMPIRE_NAME(emp), EMPIRE_BANNER(emp), EMPIRE_ADJECTIVE(emp), EMPIRE_VNUM(emp));
	build_page_display(ch, "Leader: [\ty%s\t0], Created: [\ty%-24.24s\t0], Score/rank: [\tc%d #%d\t0]", (index = find_player_index_by_idnum(EMPIRE_LEADER(emp))) ? index->fullname : "UNKNOWN", ctime(&EMPIRE_CREATE_TIME(emp)), get_total_score(emp), found_rank);
	
	ptime = summarize_weekly_playtime(emp);
	pline = build_page_display(ch, "Last %d weeks playtime: ", PLAYTIME_WEEKS_TO_TRACK);
	for (iter = 0; iter < PLAYTIME_WEEKS_TO_TRACK; ++iter) {
		append_page_display_line(pline, "%s%s", (iter > 0 ? ", " : ""), simple_time_since(time(0) - ptime[iter]));
	}
	
	if (EMPIRE_NEXT_TIMEOUT(emp) > time(0)) {
		build_page_display(ch, "Next timeout check: [%s %s]", colon_time(EMPIRE_NEXT_TIMEOUT(emp) - time(0), FALSE, NULL), (EMPIRE_NEXT_TIMEOUT(emp) >= time(0) ? "from now" : "ago"));
	}
	else {
		build_page_display(ch, "Next timeout check: [%ld / not scheduled]", EMPIRE_NEXT_TIMEOUT(emp));
	}
	
	sprintbit(EMPIRE_ADMIN_FLAGS(emp), empire_admin_flags, line, TRUE);
	build_page_display(ch, "Admin flags: \tg%s\t0", line);
	
	sprintbit(EMPIRE_FRONTIER_TRAITS(emp), empire_trait_types, line, TRUE);
	build_page_display(ch, "Frontier traits: \tc%s\t0", line);

	pline = build_page_display(ch, "Technology: \tg");
	for (iter = 0, len = 0, any = FALSE; iter < NUM_TECHS; ++iter) {
		if (EMPIRE_HAS_TECH(emp, iter)) {
			any = TRUE;
			if (len > 0 && len + strlen(empire_tech_types[iter]) + 3 >= 80) {	// new line
				append_page_display_line(pline, ",\r\n%s", empire_tech_types[iter]);
				len = strlen(empire_tech_types[iter]);
			}
			else {	// same line
				append_page_display_line(pline, "%s%s", (len > 0) ? ", " : "", empire_tech_types[iter]);
				len += strlen(empire_tech_types[iter]) + 2;
			}
		}
	}
	if (!any) {
		append_page_display_line(pline, "none");
	}
	append_page_display_line(pline, "\t0");	// end tech
	
	build_page_display(ch, "Members: [\tc%d\t0/\tc%d\t0], Citizens: [\tc%d\t0], Military: [\tc%d\t0]", EMPIRE_MEMBERS(emp), EMPIRE_TOTAL_MEMBER_COUNT(emp), EMPIRE_POPULATION(emp), EMPIRE_MILITARY(emp));
	build_page_display(ch, "Territory: [\tc%d\t0/\tc%d\t0], In-City: [\tc%d\t0], Outskirts: [\tc%d\t0/\tc%d\t0], Frontier: [\tc%d\t0/\tc%d\t0]", EMPIRE_TERRITORY(emp, TER_TOTAL), land_can_claim(emp, TER_TOTAL), EMPIRE_TERRITORY(emp, TER_CITY), EMPIRE_TERRITORY(emp, TER_OUTSKIRTS), land_can_claim(emp, TER_OUTSKIRTS), EMPIRE_TERRITORY(emp, TER_FRONTIER), land_can_claim(emp, TER_FRONTIER));

	build_page_display(ch, "Wealth: [\ty%d\t0], Treasure: [\ty%d\t0], Coins: [\ty%.1f\t0]", (int) GET_TOTAL_WEALTH(emp), EMPIRE_WEALTH(emp), EMPIRE_COINS(emp));
	build_page_display(ch, "Greatness: [\tc%d\t0], Fame: [\tc%d\t0]", EMPIRE_GREATNESS(emp), EMPIRE_FAME(emp));
	
	// progress points by category
	total = 0;
	pline = build_page_display_str(ch, "");
	for (iter = 1; iter < NUM_PROGRESS_TYPES; ++iter) {
		total += EMPIRE_PROGRESS_POINTS(emp, iter);
		append_page_display_line(pline, "%s: [\ty%d\t0], ", progress_types[iter], EMPIRE_PROGRESS_POINTS(emp, iter));
	}
	append_page_display_line(pline, "Total: [\ty%d\t0]", total);
	
	// attributes
	pline = build_page_display_str(ch, "");
	for (iter = 0, len = 0; iter < NUM_EMPIRE_ATTRIBUTES; ++iter) {
		switch (iter) {
			case EATT_TERRITORY_PER_GREATNESS: {
				sprintf(line, "%s: [%d \tc%+d\t0]", empire_attributes[iter], config_get_int("land_per_greatness"), EMPIRE_ATTRIBUTE(emp, iter));
				break;
			}
			default: {
				sprintf(line, "%s: [\tc%+d\t0]", empire_attributes[iter], EMPIRE_ATTRIBUTE(emp, iter));
				break;
			}
		}
		
		if (len > 0 && len + strlen(line) + 2 >= 80) {	// start new line
			append_page_display_line(pline, "\r\n%s", line);
			len = strlen(line);
		}
		else {	// same line
			append_page_display_line(pline, "%s%s", (len > 0) ? ", " : "", line);
			len += strlen(line) + 2;
		}
	}
	
	build_page_display_str(ch, "Script information:");
	if (SCRIPT(emp)) {
		script_stat(ch, SCRIPT(emp));
	}
	else {
		build_page_display_str(ch, "  None.");
	}
	
	send_page_display(ch);
}


/**
* Show a character stats on a particular global.
*
* @param char_data *ch The player requesting stats.
* @param struct global_data *glb The global to stat.
*/
void do_stat_global(char_data *ch, struct global_data *glb) {
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
	ability_data *abil;
	
	build_page_display(ch, "Global VNum: [&c%d&0], Type: [&c%s&0], Name: '&c%s&0'", GET_GLOBAL_VNUM(glb), global_types[GET_GLOBAL_TYPE(glb)], GET_GLOBAL_NAME(glb));

	sprintf(buf, "%s", (GET_GLOBAL_ABILITY(glb) == NO_ABIL ? "none" : get_ability_name_by_vnum(GET_GLOBAL_ABILITY(glb))));
	if ((abil = find_ability_by_vnum(GET_GLOBAL_ABILITY(glb))) && ABIL_ASSIGNED_SKILL(abil) != NULL) {
		sprintf(buf + strlen(buf), " (%s %d)", SKILL_ABBREV(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
	}
	build_page_display(ch, "Requires ability: [&y%s&0], Percent: [&g%.2f&0]", buf, GET_GLOBAL_PERCENT(glb));
	
	sprintbit(GET_GLOBAL_FLAGS(glb), global_flags, buf, TRUE);
	build_page_display(ch, "Flags: &g%s&0", buf);
	
	// GLOBAL_x
	switch (GET_GLOBAL_TYPE(glb)) {
		case GLOBAL_MOB_INTERACTIONS: {
			sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), action_bits, buf, TRUE);
			sprintbit(GET_GLOBAL_TYPE_EXCLUDE(glb), action_bits, buf2, TRUE);
			build_page_display(ch, "Levels: [&g%s&0], Mob Flags: &c%s&0, Exclude: &c%s&0", level_range_string(GET_GLOBAL_MIN_LEVEL(glb), GET_GLOBAL_MAX_LEVEL(glb), 0), buf, buf2);
			break;
		}
		case GLOBAL_OBJ_INTERACTIONS: {
			sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), extra_bits, buf, TRUE);
			sprintbit(GET_GLOBAL_TYPE_EXCLUDE(glb), extra_bits, buf2, TRUE);
			build_page_display(ch, "Levels: [&g%s&0], Obj Flags: &c%s&0, Exclude: &c%s&0", level_range_string(GET_GLOBAL_MIN_LEVEL(glb), GET_GLOBAL_MAX_LEVEL(glb), 0), buf, buf2);
			break;
		}
		case GLOBAL_MINE_DATA: {
			sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), sector_flags, buf, TRUE);
			sprintbit(GET_GLOBAL_TYPE_EXCLUDE(glb), sector_flags, buf2, TRUE);
			build_page_display(ch, "Capacity: [&g%d-%d normal, %d-%d deep&0], Sector Flags: &c%s&0, Exclude: &c%s&0", GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE)/2, GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE), (int)(GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE) / 2.0 * 1.5), (int)(GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE) * 1.5), buf, buf2);
			break;
		}
		case GLOBAL_NEWBIE_GEAR: {
			build_page_display_str(ch, "Gear:");
			show_archetype_gear_display(ch, GET_GLOBAL_GEAR(glb), FALSE);
			break;
		}
		case GLOBAL_MAP_SPAWNS: {
			ordered_sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), climate_flags, climate_flags_order, TRUE, buf);
			ordered_sprintbit(GET_GLOBAL_TYPE_EXCLUDE(glb), climate_flags, climate_flags_order, TRUE, buf2);
			build_page_display(ch, "Climate: &c%s&0 (Exclude: &c%s&0)", buf, buf2);
			sprintbit(GET_GLOBAL_SPARE_BITS(glb), spawn_flags, buf, TRUE);
			build_page_display(ch, "Spawn flags: &g%s&0", buf);
			show_spawn_summary_display(ch, TRUE, GET_GLOBAL_SPAWNS(glb));
			break;
		}
	}
	
	if (GET_GLOBAL_INTERACTIONS(glb)) {
		build_page_display_str(ch, "Interactions:");
		show_interaction_display(ch, GET_GLOBAL_INTERACTIONS(glb), FALSE);
	}
	
	send_page_display(ch);
}


/**
* Gives detailed information on an object (j) to ch
*
* @param char_data *ch The person viewing stats.
* @param obj_data *j The object to view stats on.
* @param bool details If TRUE, shows full messages (due to -d option on vstat).
*/
void do_stat_object(char_data *ch, obj_data *j, bool details) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	int count, found, minutes;
	struct obj_apply *apply;
	room_data *room;
	obj_vnum vnum = GET_OBJ_VNUM(j);
	obj_data *j2;
	struct obj_storage_type *store;
	struct custom_message *ocm;
	player_index_data *index;
	crop_data *cp;
	bool any;
	struct page_display *line;

	build_page_display(ch, "Name: '&y%s&0', Keywords: %s", GET_OBJ_DESC(j, ch, OBJ_DESC_SHORT), GET_OBJ_KEYWORDS(j));

	if (GET_OBJ_CURRENT_SCALE_LEVEL(j) > 0) {
		sprintf(buf, " (%d)", GET_OBJ_CURRENT_SCALE_LEVEL(j));
	}
	else if (GET_OBJ_MIN_SCALE_LEVEL(j) > 0 || GET_OBJ_MAX_SCALE_LEVEL(j) > 0) {
		sprintf(buf, " (%d-%d)", GET_OBJ_MIN_SCALE_LEVEL(j), GET_OBJ_MAX_SCALE_LEVEL(j));
	}
	else {
		strcpy(buf, " (no scale limit)");
	}
	
	build_page_display(ch, "Gear rating: [&g%.2f%s&0], VNum: [&g%5d&0], Type: &c%s&0", rate_item(j), buf, vnum, item_types[(int) GET_OBJ_TYPE(j)]);
	build_page_display(ch, "L-Des: %s", GET_OBJ_DESC(j, ch, OBJ_DESC_LONG));
	
	if (GET_OBJ_ACTION_DESC(j)) {
		build_page_display_str(ch, NULLSAFE(GET_OBJ_ACTION_DESC(j)));
	}

	*buf = 0;
	if (GET_OBJ_EX_DESCS(j)) {
		struct extra_descr_data *desc;
		
		if (details) {
			build_page_display(ch, "Extra descs:");
			LL_FOREACH(GET_OBJ_EX_DESCS(j), desc) {
				build_page_display(ch, "[ &c%s&0 ]\r\n%s", desc->keyword, desc->description);
			}
		}
		else {
			sprintf(buf, "Extra descs:&c");
			LL_FOREACH(GET_OBJ_EX_DESCS(j), desc) {
				strcat(buf, " ");
				strcat(buf, desc->keyword);
			}
			build_page_display(ch, "%s&0", buf);
		}
	}
	
	sprintbit(GET_OBJ_WEAR(j), wear_bits, buf, TRUE);
	build_page_display(ch, "Can be worn on: &g%s&0", buf);

	sprintbit(GET_OBJ_AFF_FLAGS(j), affected_bits, buf, TRUE);
	build_page_display(ch, "Set char bits : &y%s&0", buf);

	sprintbit(GET_OBJ_EXTRA(j), extra_bits, buf, TRUE);
	build_page_display(ch, "Extra flags   : &g%s&0", buf);
	
	prettier_sprintbit(GET_OBJ_TOOL_FLAGS(j), tool_flags, buf);
	build_page_display(ch, "Tool types: &y%s&0", buf);
	
	prettier_sprintbit(GET_OBJ_REQUIRES_TOOL(j), tool_flags, buf);
	build_page_display(ch, "Requires tool to use when crafting: \tg%s\t0", buf);
	
	if (GET_OBJ_TIMER(j) > 0) {
		minutes = GET_OBJ_TIMER(j) * SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN;
		safe_snprintf(part, sizeof(part), "%d tick%s (%s%s)", GET_OBJ_TIMER(j), PLURAL(GET_OBJ_TIMER(j)), colon_time(minutes, TRUE, NULL), GET_OBJ_TIMER(j) < 60 ? " minutes" : "");
	}
	else {
		strcpy(part, "none");
	}
	
	build_page_display(ch, "Timer: &y%s&0, Material: &y%s&0, Component type: [&y%d&0] &y%s&0", part, materials[GET_OBJ_MATERIAL(j)].name, GET_OBJ_COMPONENT(j), GET_OBJ_COMPONENT(j) != NOTHING ? get_generic_name_by_vnum(GET_OBJ_COMPONENT(j)) : "none");
	
	if (GET_OBJ_REQUIRES_QUEST(j) != NOTHING) {
		build_page_display(ch, "Requires quest: [%d] &c%s&0", GET_OBJ_REQUIRES_QUEST(j), get_quest_name_by_proto(GET_OBJ_REQUIRES_QUEST(j)));
	}
	
	strcpy(buf, "In room: ");
	if (!IN_ROOM(j))
		strcat(buf, "Nowhere");
	else {
		sprintf(buf2, "%d", GET_ROOM_VNUM(IN_ROOM(j)));
		strcat(buf, buf2);
	}

	/*
	 * NOTE: In order to make it this far, we must already be able to see the
	 *       character holding the object. Therefore, we do not need CAN_SEE().
	 */
	strcat(buf, ", In object: ");
	strcat(buf, j->in_obj ? GET_OBJ_DESC(j->in_obj, ch, OBJ_DESC_SHORT) : "None");
	strcat(buf, ", In vehicle: ");
	strcat(buf, j->in_vehicle ? VEH_SHORT_DESC(j->in_vehicle) : "None");
	strcat(buf, ", Carried by: ");
	strcat(buf, j->carried_by ? GET_NAME(j->carried_by) : "Nobody");
	strcat(buf, ", Worn by: ");
	strcat(buf, j->worn_by ? GET_NAME(j->worn_by) : "Nobody");
	strcat(buf, "\r\n");
	build_page_display_str(ch, buf);
	
	// binding section
	if (OBJ_BOUND_TO(j)) {
		struct obj_binding *bind;
		
		line = build_page_display(ch, "Bound to:");
		any = FALSE;
		for (bind = OBJ_BOUND_TO(j); bind; bind = bind->next) {
			append_page_display_line(line, "%s %s", (any ? "," : ""), (index = find_player_index_by_idnum(bind->idnum)) ? index->fullname : "<unknown>");
			any = TRUE;
		}
	}
	
	// ITEM_X: stat obj
	switch (GET_OBJ_TYPE(j)) {
		case ITEM_BOOK: {
			book_data *book = book_proto(GET_BOOK_ID(j));
			build_page_display(ch, "Book: %d - %s", GET_BOOK_ID(j), (book ? BOOK_TITLE(book) : "unknown"));
			break;
		}
		case ITEM_POISON: {
			build_page_display(ch, "Poison affect type: [%d] %s", GET_POISON_AFFECT(j), GET_POISON_AFFECT(j) != NOTHING ? get_generic_name_by_vnum(GET_POISON_AFFECT(j)) : "not custom");
			build_page_display(ch, "Charges remaining: %d", GET_POISON_CHARGES(j));
			break;
		}
		case ITEM_RECIPE: {
			craft_data *cft = craft_proto(GET_RECIPE_VNUM(j));
			build_page_display(ch, "Teaches craft: %d %s (%s)", GET_RECIPE_VNUM(j), cft ? GET_CRAFT_NAME(cft) : "UNKNOWN", cft ? craft_types[GET_CRAFT_TYPE(cft)] : "?");
			break;
		}
		case ITEM_WEAPON:
			build_page_display(ch, "Speed: %.2f, Damage: %d (%s+%.2f base dps)", get_weapon_speed(j), GET_WEAPON_DAMAGE_BONUS(j), (IS_MAGIC_ATTACK(GET_WEAPON_TYPE(j)) ? "Intelligence" : "Strength"), get_base_dps(j));
			build_page_display(ch, "Damage type: %d %s (%s/%s)", GET_WEAPON_TYPE(j), get_attack_name_by_vnum(GET_WEAPON_TYPE(j)), weapon_types[get_attack_weapon_type_by_vnum(GET_WEAPON_TYPE(j))], damage_types[get_attack_damage_type_by_vnum(GET_WEAPON_TYPE(j))]);
			break;
		case ITEM_ARMOR:
			build_page_display(ch, "Armor type: %s", armor_types[GET_ARMOR_TYPE(j)]);
			break;
		case ITEM_CONTAINER:
			build_page_display(ch, "Holds: %d items", GET_MAX_CONTAINER_CONTENTS(j));

			sprintbit(GET_CONTAINER_FLAGS(j), container_bits, buf, TRUE);
			build_page_display(ch, "Flags: %s", buf);
			break;
		case ITEM_DRINKCON:
			build_page_display(ch, "Contains: %d/%d drinks of %s", GET_DRINK_CONTAINER_CONTENTS(j), GET_DRINK_CONTAINER_CAPACITY(j), get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(j), GENERIC_LIQUID, GSTR_LIQUID_NAME));
			break;
		case ITEM_FOOD:
			build_page_display(ch, "Fills for: %d hour%s", GET_FOOD_HOURS_OF_FULLNESS(j), PLURAL(GET_FOOD_HOURS_OF_FULLNESS(j)));
			break;
		case ITEM_CORPSE:
			line = build_page_display(ch, "Corpse of: ");

			if (IS_NPC_CORPSE(j)) {
				append_page_display_line(line, "[%d] %s", GET_CORPSE_NPC_VNUM(j), get_mob_name_by_proto(GET_CORPSE_NPC_VNUM(j), FALSE));
			}
			else if (IS_PC_CORPSE(j)) {
				append_page_display_line(line, "[%d] %s", GET_CORPSE_PC_ID(j), (index = find_player_index_by_idnum(GET_CORPSE_PC_ID(j))) ? index->fullname : "a player");
			}
			else {
				append_page_display_line(line, "unknown");
			}
			
			sprintbit(GET_CORPSE_FLAGS(j), corpse_flags, buf, TRUE);
			build_page_display(ch, "Corpse flags: %s", buf);
			build_page_display(ch, "Corpse size: %s", size_types[GET_CORPSE_SIZE(j)]);
			break;
		case ITEM_COINS: {
			build_page_display(ch, "Amount: %s", money_amount(real_empire(GET_COINS_EMPIRE_ID(j)), GET_COINS_AMOUNT(j)));
			break;
		}
		case ITEM_CURRENCY: {
			build_page_display(ch, "Amount: %d %s", GET_CURRENCY_AMOUNT(j), get_generic_string_by_vnum(GET_CURRENCY_VNUM(j), GENERIC_CURRENCY, WHICH_CURRENCY(GET_CURRENCY_AMOUNT(j))));
			break;
		}
		case ITEM_MISSILE_WEAPON:
			build_page_display(ch, "Speed: %.2f, Damage: %d (%s+%.2f base dps)", get_weapon_speed(j), GET_MISSILE_WEAPON_DAMAGE(j), (IS_MAGIC_ATTACK(GET_MISSILE_WEAPON_TYPE(j)) ? "Intelligence" : "Strength"), get_base_dps(j));
			build_page_display(ch, "Damage type: %s", get_attack_name_by_vnum(GET_MISSILE_WEAPON_TYPE(j)));
			build_page_display(ch, "Ammo type: %c", 'A' + GET_MISSILE_WEAPON_AMMO_TYPE(j));
			break;
		case ITEM_AMMO:
			if (GET_AMMO_QUANTITY(j) > 0) {
				build_page_display(ch, "Quantity: %d", GET_AMMO_QUANTITY(j));
			}
			if (GET_AMMO_DAMAGE_BONUS(j) > 0) {
				build_page_display(ch, "Damage: %+d", GET_AMMO_DAMAGE_BONUS(j));
			}
			build_page_display(ch, "Ammo type: %c", 'A' + GET_AMMO_TYPE(j));
			if (GET_OBJ_AFF_FLAGS(j) || GET_OBJ_APPLIES(j)) {
				generic_data *aftype = find_generic(GET_OBJ_VNUM(j), GENERIC_AFFECT);
				build_page_display(ch, "Debuff name: %s", aftype ? GEN_NAME(aftype) : get_generic_name_by_vnum(ATYPE_RANGED_WEAPON));
			}
			break;
		case ITEM_PACK: {
			build_page_display(ch, "Adds inventory space: %d", GET_PACK_CAPACITY(j));
			break;
		}
		case ITEM_PORTAL:
			if (j != obj_proto(GET_OBJ_VNUM(j))) {
				line = build_page_display(ch, "Portal destination: ");
				room = real_room(GET_PORTAL_TARGET_VNUM(j));
				if (!room) {
					append_page_display_line(line, "None");
				}
				else {
					append_page_display_line(line, "%d - %s", GET_PORTAL_TARGET_VNUM(j), get_room_name(room, FALSE));
				}
			}
			else {
				// vstat -- just show target
				build_page_display(ch, "Portal destination: %d", GET_PORTAL_TARGET_VNUM(j));
			}
			break;
		case ITEM_PAINT: {
			sprinttype(GET_PAINT_COLOR(j), paint_names, buf, sizeof(buf), "UNDEFINED");
			sprinttype(GET_PAINT_COLOR(j), paint_colors, part, sizeof(part), "&0");
			build_page_display(ch, "Paint color: %s%s\t0", part, buf);
			break;
		}
		case ITEM_POTION: {
			build_page_display(ch, "Potion affect type: [%d] %s", GET_POTION_AFFECT(j), GET_POTION_AFFECT(j) != NOTHING ? get_generic_name_by_vnum(GET_POTION_AFFECT(j)) : "not custom");
			build_page_display(ch, "Potion cooldown: [%d] %s, %d second%s", GET_POTION_COOLDOWN_TYPE(j), GET_POTION_COOLDOWN_TYPE(j) != NOTHING ? get_generic_name_by_vnum(GET_POTION_COOLDOWN_TYPE(j)) : "no cooldown", GET_POTION_COOLDOWN_TIME(j), PLURAL(GET_POTION_COOLDOWN_TIME(j)));
			break;
		}
		case ITEM_WEALTH: {
			sprintbit(GET_WEALTH_MINT_FLAGS(j), mint_flags, buf, TRUE);
			build_page_display(ch, "Wealth value: \ts%d\t0, flags: \tc%s\t0", GET_WEALTH_VALUE(j), buf);
			break;
		}
		case ITEM_LIGHTER: {
			if (GET_LIGHTER_USES(j) == UNLIMITED) {
				build_page_display(ch, "Lighter uses: unlimited");
			}
			else {
				build_page_display(ch, "Lighter uses: %d", GET_LIGHTER_USES(j));
			}
			break;
		}
		case ITEM_MINIPET: {
			build_page_display(ch, "Minipet: [%d] %s", GET_MINIPET_VNUM(j), get_mob_name_by_proto(GET_MINIPET_VNUM(j), FALSE));
			break;
		}
		case ITEM_LIGHT: {
			if (GET_LIGHT_HOURS_REMAINING(j) == UNLIMITED) {
				safe_snprintf(part, sizeof(part), "unlimited light");
			}
			else {
				safe_snprintf(part, sizeof(part), "%d hour%s remaining", GET_LIGHT_HOURS_REMAINING(j), PLURAL(GET_LIGHT_HOURS_REMAINING(j)));
			}
			sprintbit(GET_LIGHT_FLAGS(j), light_flags, buf, TRUE);
			build_page_display(ch, "Light: \tc%s\t0 (\tc%s\t0), flags: \tc%s\t0", part, (GET_LIGHT_IS_LIT(j) ? "lit" : "unlit"), buf);
			break;
		}
		default:
			build_page_display(ch, "Values 0-2: [&g%d&0] [&g%d&0] [&g%d&0]", GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2));
			break;
	}
	
	// data that isn't type-based:
	if (OBJ_FLAGGED(j, OBJ_PLANTABLE) && (cp = crop_proto(GET_OBJ_VAL(j, VAL_FOOD_CROP_TYPE)))) {
		ordered_sprintbit(GET_CROP_CLIMATE(cp), climate_flags, climate_flags_order, CROP_FLAGGED(cp, CROPF_ANY_LISTED_CLIMATE) ? TRUE : FALSE, buf);
		build_page_display(ch, "Plants [%d] %s (%s%s).", GET_OBJ_VAL(j, VAL_FOOD_CROP_TYPE), GET_CROP_NAME(cp), GET_CROP_CLIMATE(cp) ? buf : "any climate", (CROP_FLAGGED(cp, CROPF_REQUIRES_WATER) ? "; must be near water" : ""));
	}

	/*
	 * I deleted the "equipment status" code from here because it seemed
	 * more or less useless and just takes up valuable screen space.
	 */

	if (j->contains) {
		line = NULL;
		sprintf(buf, "Contents:&g");
		found = 0;
		DL_FOREACH2(j->contains, j2, next_content) {
			sprintf(buf2, "%s %s", found++ ? "," : "", GET_OBJ_DESC(j2, ch, OBJ_DESC_SHORT));
			strcat(buf, buf2);
			if (strlen(buf) >= 62) {
				if (j2->next_content) {
					line = build_page_display(ch, "%s,", buf);
				}
				else {
					line = build_page_display_str(ch, buf);
				}
				*buf = found = 0;
			}
		}

		if (*buf) {
			line = build_page_display_str(ch, buf);
     	}
     	if (line) {
     		append_page_display_line(line, "&0");
		}
	}
	found = 0;
	line = build_page_display_str(ch, "Applies:");
	for (apply = GET_OBJ_APPLIES(j); apply; apply = apply->next) {
		if (apply->apply_type != APPLY_TYPE_NATURAL) {
			sprintf(part, " (%s)", apply_type_names[(int)apply->apply_type]);
		}
		else {
			*part = '\0';
		}
		append_page_display_line(line, "%s %+d to %s%s", found++ ? "," : "", apply->modifier, apply_types[(int) apply->location], part);
	}
	if (!found) {
		append_page_display_line(line, " None");
	}
	
	if (GET_OBJ_STORAGE(j)) {
		line = build_page_display(ch, "Storage locations:");
		
		found = 0;
		LL_FOREACH(GET_OBJ_STORAGE(j), store) {
			// TYPE_x: storage type
			if (store->type == TYPE_BLD) {
				append_page_display_line(line, "%s[B%d] %s", (found++ > 0 ? ", " : " "), store->vnum, get_bld_name_by_proto(store->vnum));
			}
			else if (store->type == TYPE_VEH) {
				append_page_display_line(line, "%s[V%d] %s", (found++ > 0 ? ", " : " "), store->vnum, get_vehicle_name_by_proto(store->vnum));
			}
			else {
				// none?
				continue;
			}
			
			if (store->flags) {
				sprintbit(store->flags, storage_bits, buf2, TRUE);
				append_page_display_line(line, " ( %s)", buf2);
			}
		}
	}
	
	if (GET_OBJ_INTERACTIONS(j)) {
		build_page_display_str(ch, "Interactions:");
		show_interaction_display(ch, GET_OBJ_INTERACTIONS(j), FALSE);
	}
	
	if (GET_OBJ_CUSTOM_MSGS(j)) {
		if (details) {
			build_page_display(ch, "Custom messages:");
		
			LL_FOREACH(GET_OBJ_CUSTOM_MSGS(j), ocm) {
				build_page_display(ch, " %s: %s", obj_custom_types[ocm->type], ocm->msg);
			}
		}
		else {
			LL_COUNT(GET_OBJ_CUSTOM_MSGS(j), ocm, count);
			build_page_display(ch, "Custom messages: %d", count);
		}
	}

	/* check the object for a script */
	do_sstat_object(ch, j);
	
	send_page_display(ch);
}


/* Displays the vital statistics of IN_ROOM(ch) to ch */
void do_stat_room(char_data *ch) {
	char buf[MAX_STRING_LENGTH], buf1[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], buf3[MAX_STRING_LENGTH], *nstr;
	struct depletion_data *dep;
	struct empire_city_data *city;
	struct time_info_data tinfo;
	int duration, found, max, num, zenith;
	bool comma;
	adv_data *adv;
	obj_data *j;
	char_data *k;
	crop_data *cp;
	empire_data *emp;
	struct affected_type *aff;
	struct room_extra_data *red, *next_red;
	struct generic_name_data *name_set;
	struct empire_territory_data *ter;
	struct empire_npc_data *npc;
	player_index_data *index;
	struct global_data *glb;
	room_data *home = HOME_ROOM(IN_ROOM(ch));
	struct instance_data *inst, *inst_iter;
	double latitude, longitude;
	struct map_data *map;
	vehicle_data *veh;
	struct string_hash *str_iter, *next_str, *str_hash = NULL;
	size_t size;
	struct page_display *line;
	
	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_HAS_CROP_DATA) && (cp = ROOM_CROP(IN_ROOM(ch)))) {
		strcpy(buf2, GET_CROP_NAME(cp));
		CAP(buf2);
	}
	else {
		strcpy(buf2, GET_SECT_NAME(SECT(IN_ROOM(ch))));
	}
	
	// check for natural sect
	if (GET_ROOM_VNUM(IN_ROOM(ch)) < MAP_SIZE) {
		sprintf(buf3, "/&c%s&0", GET_SECT_NAME(world_map[X_COORD(IN_ROOM(ch))][Y_COORD(IN_ROOM(ch))].natural_sector));
	}
	else {
		*buf3 = '\0';
	}
	
	// title
	build_page_display(ch, "(%d, %d) %s", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)), get_room_name(IN_ROOM(ch), FALSE));
	
	// sector data
	line = build_page_display_str(ch, "");
	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_HAS_CROP_DATA) && (cp = ROOM_CROP(IN_ROOM(ch)))) {
		append_page_display_line(line, "Crop: [\to%d\t0 - \to%s\t0], ", GET_CROP_VNUM(cp), GET_CROP_NAME(cp));
	}
	append_page_display_line(line, "Sector: [\tc%d\t0 - \tc%s\t0]", GET_SECT_VNUM(SECT(IN_ROOM(ch))), GET_SECT_NAME(SECT(IN_ROOM(ch))));
	append_page_display_line(line, ", Base: [\ty%d\t0 - \ty%s\t0]", GET_SECT_VNUM(BASE_SECT(IN_ROOM(ch))), GET_SECT_NAME(BASE_SECT(IN_ROOM(ch))));
	if (GET_ROOM_VNUM(IN_ROOM(ch)) < MAP_SIZE && (map = &world_map[X_COORD(IN_ROOM(ch))][Y_COORD(IN_ROOM(ch))])) {
		append_page_display_line(line, ", Natural: [\tg%d\t0 - \tg%s\t0]", GET_SECT_VNUM(map->natural_sector), GET_SECT_NAME(map->natural_sector));
	}
	
	// building/room data
	if (GET_ROOM_TEMPLATE(IN_ROOM(ch))) {
		build_page_display(ch, "Room template: [\to%d\t0 - \to%s\t0]", GET_RMT_VNUM(GET_ROOM_TEMPLATE(IN_ROOM(ch))), GET_RMT_TITLE(GET_ROOM_TEMPLATE(IN_ROOM(ch))));
		if ((adv = get_adventure_for_vnum(GET_RMT_VNUM(GET_ROOM_TEMPLATE(IN_ROOM(ch)))))) {
			build_page_display(ch, "Adventure: [\to%d\t0 - \to%s\t0]", GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
		}
	}
	if (GET_BUILDING(IN_ROOM(ch))) {
		build_page_display(ch, "Building: [\to%d\t0 - \to%s\t0]", GET_BLD_VNUM(GET_BUILDING(IN_ROOM(ch))), GET_BLD_NAME(GET_BUILDING(IN_ROOM(ch))));
	}
	
	build_page_display(ch, "VNum: [\tg%d\t0], Lights: [\tg%d\t0], Island: [\tg%d\t0] %s, Height [\tg%d/%d\t0]", GET_ROOM_VNUM(IN_ROOM(ch)), ROOM_LIGHTS(IN_ROOM(ch)), GET_ISLAND_ID(IN_ROOM(ch)), GET_ISLAND(IN_ROOM(ch)) ? GET_ISLAND(IN_ROOM(ch))->name : "no island", ROOM_HEIGHT(IN_ROOM(ch)), get_room_blocking_height(IN_ROOM(ch), NULL));
	
	// location/time data
	if (X_COORD(IN_ROOM(ch)) != -1) {
		tinfo = get_local_time(IN_ROOM(ch));
		latitude = Y_TO_LATITUDE(Y_COORD(IN_ROOM(ch)));
		longitude = X_TO_LONGITUDE(X_COORD(IN_ROOM(ch)));
		build_page_display(ch, "Globe: [\tc%.2f %s, %.2f %s\t0], Time: [\tc%d%s\t0], Sun: [\tc%s\t0], Hours of sun today: [\tc%.2f\t0]", ABSOLUTE(latitude), (latitude >= 0.0 ? "N" : "S"), ABSOLUTE(longitude), (longitude >= 0.0 ? "E" : "W"), TIME_TO_12H(tinfo.hours), AM_PM(tinfo.hours), sun_types[get_sun_status(IN_ROOM(ch))], get_hours_of_sun(IN_ROOM(ch)));
		if ((zenith = get_zenith_days_from_solstice(IN_ROOM(ch))) != -1) {
			build_page_display(ch, "Zenith passage: [\tg%d day%s from the solstice\t0]", zenith, PLURAL(zenith));
		}
	}
	else {
		build_page_display(ch, "Globe: no data available (location is not on the map)");
	}
	
	// temperature info
	build_page_display(ch, "Temperature: \ty%d\t0 (\ty%s\t0), Season: \ty%s\t0", get_room_temperature(IN_ROOM(ch)), temperature_to_string(get_room_temperature(IN_ROOM(ch))), icon_types[GET_SEASON(IN_ROOM(ch))]);
	if (get_climate(IN_ROOM(ch)) != NOBITS) {
		ordered_sprintbit(get_climate(IN_ROOM(ch)), climate_flags, climate_flags_order, FALSE, buf);
		build_page_display(ch, "Climate: \tc%s\t0", buf);
	}
	
	if (home != IN_ROOM(ch)) {
		build_page_display(ch, "Home room: &g%d&0 %s", GET_ROOM_VNUM(home), get_room_name(home, FALSE));
	}
	
	if (ROOM_CUSTOM_NAME(IN_ROOM(ch)) || ROOM_CUSTOM_ICON(IN_ROOM(ch)) || ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))) {
		build_page_display(ch, "Custom:&y%s%s%s&0", ROOM_CUSTOM_NAME(IN_ROOM(ch)) ? " NAME" : "", ROOM_CUSTOM_ICON(IN_ROOM(ch)) ? " ICON" : "", ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch)) ? " DESCRIPTION" : "");
	}

	// ownership
	if ((emp = ROOM_OWNER(home))) {
		line = build_page_display(ch, "Owner: %s%s&0", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		if ((city = find_city(emp, IN_ROOM(ch)))) {
			append_page_display_line(line, ", City: %s", city->name);
		}
		if (ROOM_PRIVATE_OWNER(home) != NOBODY) {
			append_page_display_line(line, ", Home: %s", (index = find_player_index_by_idnum(ROOM_PRIVATE_OWNER(home))) ? index->fullname : "someone");
		}
	}

	if (ROOM_AFF_FLAGS(IN_ROOM(ch))) {	
		sprintbit(ROOM_AFF_FLAGS(IN_ROOM(ch)), room_aff_bits, buf2, TRUE);
		build_page_display(ch, "Status: &c%s&0", buf2);
	}
	
	if (COMPLEX_DATA(IN_ROOM(ch))) {
		if (GET_INSIDE_ROOMS(home) > 0) {
			build_page_display(ch, "Designated rooms: %d", GET_INSIDE_ROOMS(home));
		}
		
		if (IS_BURNING(home)) {
			sprintf(buf2, "Burns down in: %ld seconds", BUILDING_BURN_DOWN_TIME(home) - time(0));
		}
		else {
			strcpy(buf2, "Not on fire");
		}
		build_page_display(ch, "%s, Damage: %d/%d", buf2, (int) BUILDING_DAMAGE(home), GET_BUILDING(home) ? GET_BLD_MAX_DAMAGE(GET_BUILDING(home)) : 0);
	}

	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_CAN_MINE) || room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_MINE)) {
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_GLB_VNUM) <= 0 || !(glb = global_proto(get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_GLB_VNUM))) || GET_GLOBAL_TYPE(glb) != GLOBAL_MINE_DATA) {
			build_page_display(ch, "This area is unmined.");
		}
		else {
			build_page_display(ch, "Mine type: %s, Amount remaining: %d", GET_GLOBAL_NAME(glb), get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT));
		}
	}
	
	if ((inst = find_instance_by_room(IN_ROOM(ch), FALSE, TRUE))) {
		num = 0;
		DL_FOREACH(instance_list, inst_iter) {
			++num;
			if (inst_iter == inst) {
				break;
			}
		}
		sprintbit(INST_FLAGS(inst), instance_flags, buf2, TRUE);
		build_page_display(ch, "Instance \tc%d\t0: [\tg%d\t0] \ty%s\t0, Main Room: [\tg%d\t0], Flags: \tc%s\t0", num, GET_ADV_VNUM(INST_ADVENTURE(inst)), GET_ADV_NAME(INST_ADVENTURE(inst)), (INST_START(inst) ? GET_ROOM_VNUM(INST_START(inst)) : NOWHERE), buf2);
	}

	sprintf(buf, "Chars present:&y");
	found = 0;
	line = NULL;
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), k, next_in_room) {
		if (!CAN_SEE(ch, k))
			continue;
		sprintf(buf2, "%s %s(%s)", found++ ? "," : "", GET_NAME(k), (!IS_NPC(k) ? "PC" : (!IS_MOB(k) ? "NPC" : "MOB")));
		strcat(buf, buf2);
		if (strlen(buf) >= 62) {
			if (k->next_in_room) {
				line = build_page_display(ch, "%s,", buf);
			}
			else {
				line = build_page_display_str(ch, buf);
			}
			*buf = found = 0;
		}
	}
	if (*buf) {
		line = build_page_display_str(ch, buf);
	}
	if (line) {
		append_page_display_line(line, "&0");
	}
	
	if (ROOM_VEHICLES(IN_ROOM(ch))) {
		sprintf(buf, "Vehicles:&w");
		found = 0;
		line = NULL;
		DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
			if (!CAN_SEE_VEHICLE(ch, veh)) {
				continue;
			}
			sprintf(buf2, "%s %s", found++ ? "," : "", VEH_SHORT_DESC(veh));
			strcat(buf, buf2);
			if (strlen(buf) >= 62) {
				if (veh->next_in_room) {
					line = build_page_display(ch, "%s,", buf);
				}
				else {
					line = build_page_display_str(ch, buf);
				}
				*buf = found = 0;
			}
		}
		if (*buf) {
			line = build_page_display_str(ch, buf);
		}
		if (line) {
			append_page_display_line(line, "&0");
		}
	}
	
	if (ROOM_CONTENTS(IN_ROOM(ch))) {
		// build contents list
		DL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), j, next_content) {
			if (!CAN_SEE_OBJ(ch, j)) {
				continue;
			}
			add_string_hash(&str_hash, GET_OBJ_DESC(j, ch, OBJ_DESC_SHORT), 1);
		}
		
		if (str_hash) {
			size = snprintf(buf, sizeof(buf), "Contents:\tg");
			found = 0;
			line = NULL;
			
			HASH_ITER(hh, str_hash, str_iter, next_str) {
				if (str_iter->count == 1) {
					safe_snprintf(buf2, sizeof(buf2), "%s %s", (found++ ? "," : ""), str_iter->str);
				}
				else {
					safe_snprintf(buf2, sizeof(buf2), "%s %s (x%d)", (found++ ? "," : ""), str_iter->str, str_iter->count);
				}
				if (size + strlen(buf2) > 79 && *buf) {
					// end of line
					line = build_page_display(ch, "%s%s", buf, (found > 1) ? "," : "");
					found = 0;
					size = 0;
					*buf = '\0';
				}
				else {
					// append to line
					strcat(buf, buf2);
					size += strlen(buf2);
				}
			}
			free_string_hash(&str_hash);
			
			// anything left?
			if (*buf) {
				line = build_page_display_str(ch, buf);
			}
			if (line) {
				append_page_display_line(line, "\t0");
			}
		}
	}
	
	// empire citizens
	if (ROOM_OWNER(IN_ROOM(ch)) && (ter = find_territory_entry(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch))) && ter->npcs) {
		*buf = '\0';
		LL_FOREACH(ter->npcs, npc) {
			if (npc->mob) {
				sprintf(buf + strlen(buf), "%s[%d] \ty%s\t0", *buf ? ", " : "", GET_MOB_VNUM(npc->mob), GET_SHORT_DESC(npc->mob));
			}
			else if ((k = mob_proto(npc->vnum))) {
				name_set = get_best_name_list(MOB_NAME_SET(k), npc->sex);
				nstr = str_replace("#n", name_set->names[npc->name], GET_SHORT_DESC(k));
				sprintf(buf + strlen(buf), "%s[%d] \tc%s\t0", *buf ? ", " : "", npc->vnum, nstr);
				free(nstr);
			}
			else {
				sprintf(buf + strlen(buf), "%s[%d] \trUNKNOWN\t0", *buf ? ", " : "", npc->vnum);
			}
		}
		
		if (*buf) {
			build_page_display(ch, "Citizens: %s", buf);
		}
	}
	
	if (COMPLEX_DATA(IN_ROOM(ch))) {
		struct room_direction_data *ex;
		for (ex = COMPLEX_DATA(IN_ROOM(ch))->exits; ex; ex = ex->next) {
			if (ex->to_room == NOWHERE) {
				sprintf(buf1, " &cNONE&0");
			}
			else {
				sprintf(buf1, "&c%5d&0", ex->to_room);
			}
			sprintbit(ex->exit_info, exit_bits, buf2, TRUE);
			build_page_display(ch, "Exit &c%-5s&0:  To: [%s], Keyword: %s, Type: %s", dirs[get_direction_for_char(ch, ex->dir)], buf1, ex->keyword ? ex->keyword : "None", buf2);
		}
	}
	
	if (ROOM_DEPLETION(IN_ROOM(ch))) {
		line = build_page_display(ch, "Depletion: ");
		
		comma = FALSE;
		for (dep = ROOM_DEPLETION(IN_ROOM(ch)); dep; dep = dep->next) {
			if (dep->count > 0 && dep->type < NUM_DEPLETION_TYPES) {
				max = get_depletion_max(IN_ROOM(ch), dep->type);
				if (max > 0) {
					append_page_display_line(line, "%s%s (%d, %d%%)", comma ? ", " : "", depletion_types[dep->type], dep->count, dep->count * 100 / max);
				}
				else {
					// can't detect max
					append_page_display_line(line, "%s%s (%d)", comma ? ", " : "", depletion_types[dep->type], dep->count);
				}
				comma = TRUE;
			}
		}
	}

	/* Routine to show what spells a room is affected by */
	if (ROOM_AFFECTS(IN_ROOM(ch))) {
		for (aff = ROOM_AFFECTS(IN_ROOM(ch)); aff; aff = aff->next) {
			*buf2 = '\0';

			// duration setup
			if (aff->expire_time == UNLIMITED) {
				strcpy(buf3, "infinite");
			}
			else {
				duration = aff->expire_time - time(0);
				duration = MAX(duration, 0);
				strcpy(buf3, colon_time(duration, FALSE, NULL));
			}

			sprintf(buf, "Affect: (%s) &c%s&0", buf3, get_generic_name_by_vnum(aff->type));

			if (aff->modifier) {
				sprintf(buf2, " %+d to %s", aff->modifier, apply_types[(int) aff->location]);
				strcat(buf, buf2);
			}
			if (aff->bitvector) {
				if (*buf2) {
					strcat(buf, ", sets ");
				}
				else {
					strcat(buf, " sets ");
				}
				sprintbit(aff->bitvector, room_aff_bits, buf2, TRUE);
				strcat(buf, buf2);
			}
			build_page_display_str(ch, buf);
		}
	}
	
	if (ROOM_EXTRA_DATA(IN_ROOM(ch))) {
		build_page_display(ch, "Extra data:");
		
		HASH_ITER(hh, ROOM_EXTRA_DATA(IN_ROOM(ch)), red, next_red) {
			sprinttype(red->type, room_extra_types, buf, sizeof(buf), "UNDEFINED");
			build_page_display(ch, " %s: %d", buf, red->value);
		}
	}

	/* check the room for a script */
	do_sstat_room(ch, ch);
	
	send_page_display(ch);
}


/**
* @param char_data *ch The player requesting stats.
* @param room_template *rmt The room template to display.
* @param bool details If TRUE, shows full messages (due to -d option on vstat).
*/
void do_stat_room_template(char_data *ch, room_template *rmt, bool details) {
	char lbuf[MAX_STRING_LENGTH];
	adv_data *adv;
	struct page_display *line;
	
	if (!rmt) {
		return;
	}
	
	build_page_display(ch, "VNum: [&c%d&0], Title: %s", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt));
	build_page_display_str(ch, NULLSAFE(GET_RMT_DESC(rmt)));

	adv = get_adventure_for_vnum(GET_RMT_VNUM(rmt));
	line = build_page_display(ch, "Adventure: [&c%d %s&0]", !adv ? NOTHING : GET_ADV_VNUM(adv), !adv ? "NONE" : GET_ADV_NAME(adv));
	if (GET_RMT_SUBZONE(rmt) != NOWHERE) {
		append_page_display_line(line, ", Subzone: [\ty%d\t0]", GET_RMT_SUBZONE(rmt));
	}
	else {
		append_page_display_line(line, ", Subzone: [\tynone\t0]");
	}
	
	sprintbit(GET_RMT_FLAGS(rmt), room_template_flags, lbuf, TRUE);
	build_page_display(ch, "Flags: &g%s&0", lbuf);
	
	sprintbit(GET_RMT_FUNCTIONS(rmt), function_flags, lbuf, TRUE);
	build_page_display(ch, "Functions: &y%s&0", lbuf);

	sprintbit(GET_RMT_BASE_AFFECTS(rmt), room_aff_bits, lbuf, TRUE);
	build_page_display(ch, "Affects: &g%s&0", lbuf);
	
	build_page_display(ch, "Temperature: [\tc%s\t0]", temperature_types[GET_RMT_TEMPERATURE_TYPE(rmt)]);
	
	if (GET_RMT_EX_DESCS(rmt)) {
		struct extra_descr_data *desc;
		
		if (details) {
			build_page_display(ch, "Extra descs:");
			LL_FOREACH(GET_RMT_EX_DESCS(rmt), desc) {
				build_page_display(ch, "[ &c%s&0 ]\r\n%s", desc->keyword, desc->description);
			}
		}
		else {
			sprintf(buf, "Extra descs:&c");
			LL_FOREACH(GET_RMT_EX_DESCS(rmt), desc) {
				strcat(buf, " ");
				strcat(buf, desc->keyword);
			}
			build_page_display(ch, "%s&0", buf);
		}
	}

	build_page_display_str(ch, "Exits:");
	show_exit_template_display(ch, GET_RMT_EXITS(rmt), FALSE);
	
	if (GET_RMT_INTERACTIONS(rmt)) {
		build_page_display_str(ch, "Interactions:");
		show_interaction_display(ch, GET_RMT_INTERACTIONS(rmt), FALSE);
	}
	
	build_page_display_str(ch, "Spawns:");
	show_template_spawns_display(ch, GET_RMT_SPAWNS(rmt), FALSE);

	build_page_display_str(ch, "Scripts:");
	show_script_display(ch, GET_RMT_SCRIPTS(rmt), FALSE);
	
	send_page_display(ch);
}


/**
* Show a character stats on a particular sector.
*
* @param char_data *ch The player requesting stats.
* @param sector_data *st The sector to stat.
* @param bool details If TRUE, shows full messages (due to -d option on vstat).
*/
void do_stat_sector(char_data *ch, sector_data *st, bool details) {
	int count;
	struct sector_index_type *idx = find_sector_index(GET_SECT_VNUM(st));
	char buf[MAX_STRING_LENGTH];
	struct custom_message *ocm;
	
	build_page_display(ch, "Sector VNum: [&c%d&0], Name: '&c%s&0', Live Count [&c%d&0/&c%d&0]", GET_SECT_VNUM(st), GET_SECT_NAME(st), idx->sect_count, idx->base_count);
	build_page_display(ch, "Room Title: %s", GET_SECT_TITLE(st));
	
	if (GET_SECT_COMMANDS(st) && *GET_SECT_COMMANDS(st)) {
		build_page_display(ch, "Command list: &c%s&0", GET_SECT_COMMANDS(st));
	}
	
	build_page_display(ch, "Movement cost: [&g%d&0]  Roadside Icon: %c  Mapout Color: %s", GET_SECT_MOVE_LOSS(st), GET_SECT_ROADSIDE_ICON(st), mapout_color_names[GET_SECT_MAPOUT(st)]);
	
	if (GET_SECT_ICONS(st)) {
		build_page_display_str(ch, "Icons:");
		show_icons_display(ch, GET_SECT_ICONS(st), FALSE);
	}
	
	sprintbit(GET_SECT_FLAGS(st), sector_flags, buf, TRUE);
	build_page_display(ch, "Sector flags: &g%s&0", buf);
	
	ordered_sprintbit(GET_SECT_CLIMATE(st), climate_flags, climate_flags_order, FALSE, buf);
	build_page_display(ch, "Temperature: [\tc%s\t0], Climate: &c%s&0", temperature_types[GET_SECT_TEMPERATURE_TYPE(st)], buf);
	
	ordered_sprintbit(GET_SECT_BUILD_FLAGS(st), bld_on_flags, bld_on_flags_order, TRUE, buf);
	build_page_display(ch, "Build flags: &g%s&0", buf);
	
	if (GET_SECT_EVOS(st)) {
		build_page_display_str(ch, "Evolution information:");
		show_evolution_display(ch, GET_SECT_EVOS(st), FALSE);
	}
	
	if (GET_SECT_EX_DESCS(st)) {
		struct extra_descr_data *desc;
		
		if (details) {
			build_page_display(ch, "Extra descs:");
			LL_FOREACH(GET_SECT_EX_DESCS(st), desc) {
				build_page_display(ch, "[ &c%s&0 ]\r\n%s", desc->keyword, desc->description);
			}
		}
		else {
			sprintf(buf, "Extra descs:&c");
			LL_FOREACH(GET_SECT_EX_DESCS(st), desc) {
				strcat(buf, " ");
				strcat(buf, desc->keyword);
			}
			build_page_display(ch, "%s&0", buf);
		}
	}
	
	if (GET_SECT_CUSTOM_MSGS(st)) {
		if (details) {
			build_page_display(ch, "Custom messages:");
			LL_FOREACH(GET_SECT_CUSTOM_MSGS(st), ocm) {
				build_page_display(ch, " %s: %s", sect_custom_types[ocm->type], ocm->msg);
			}
		}
		else {
			LL_COUNT(GET_SECT_CUSTOM_MSGS(st), ocm, count);
			build_page_display(ch, "Custom messages: %d", count);
		}
	}

	if (GET_SECT_INTERACTIONS(st)) {
		build_page_display_str(ch, "Interactions:");
		show_interaction_display(ch, GET_SECT_INTERACTIONS(st), FALSE);
	}
	
	show_spawn_summary_display(ch, TRUE, GET_SECT_SPAWNS(st));
	
	if (GET_SECT_NOTES(st) && *GET_SECT_NOTES(st)) {
		build_page_display(ch, "Notes:\r\n%s", GET_SECT_NOTES(st));
	}
	
	send_page_display(ch);
}


 //////////////////////////////////////////////////////////////////////////////
//// VNUM SEARCHES ///////////////////////////////////////////////////////////

/**
* Searches the adventure db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_adventure(char *searchname, char_data *ch) {
	adv_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, adventure_table, iter, next_iter) {
		if (multi_isname(searchname, GET_ADV_NAME(iter)) || multi_isname(searchname, GET_ADV_AUTHOR(iter))) {
			build_page_display(ch, "%3d. [%5d] %s (%d-%d)", ++found, GET_ADV_VNUM(iter), GET_ADV_NAME(iter), GET_ADV_START_VNUM(iter), GET_ADV_END_VNUM(iter));
		}
	}
	
	send_page_display(ch);
	return found;
}


/**
* Searches the building db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_book(char *searchname, char_data *ch) {
	book_data *book, *next_book;
	int found = 0;
	
	HASH_ITER(hh, book_table, book, next_book) {
		if (multi_isname(searchname, BOOK_TITLE(book)) || multi_isname(searchname, BOOK_BYLINE(book))) {
			build_page_display(ch, "%3d. [%5d] %s\t0 (%s\t0)", ++found, BOOK_VNUM(book), BOOK_TITLE(book), BOOK_BYLINE(book));
		}
	}

	send_page_display(ch);
	return (found);
}


/**
* Searches the building db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_building(char *searchname, char_data *ch) {
	bld_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, building_table, iter, next_iter) {
		if (multi_isname(searchname, GET_BLD_NAME(iter))) {
			build_page_display(ch, "%3d. [%5d] %s", ++found, GET_BLD_VNUM(iter), GET_BLD_NAME(iter));
		}
	}
	
	send_page_display(ch);
	return found;
}


/**
* Searches the craft db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_craft(char *searchname, char_data *ch) {
	craft_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, craft_table, iter, next_iter) {
		if (multi_isname(searchname, GET_CRAFT_NAME(iter))) {
			build_page_display(ch, "%3d. [%5d] %s (%s)", ++found, GET_CRAFT_VNUM(iter), GET_CRAFT_NAME(iter), craft_types[GET_CRAFT_TYPE(iter)]);
		}
	}
	
	send_page_display(ch);
	return found;
}


/**
* Searches the crop db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_crop(char *searchname, char_data *ch) {
	crop_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, crop_table, iter, next_iter) {
		if (multi_isname(searchname, GET_CROP_NAME(iter)) || multi_isname(searchname, GET_CROP_TITLE(iter))) {
			build_page_display(ch, "%3d. [%5d] %s", ++found, GET_CROP_VNUM(iter), GET_CROP_NAME(iter));
		}
	}
	
	send_page_display(ch);
	return found;
}


/**
* Searches the global db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_global(char *searchname, char_data *ch) {
	struct global_data *iter, *next_iter;
	char flags[MAX_STRING_LENGTH], flags2[MAX_STRING_LENGTH];
	int found = 0;
	
	HASH_ITER(hh, globals_table, iter, next_iter) {
		if (multi_isname(searchname, GET_GLOBAL_NAME(iter))) {
			// GLOBAL_x
			switch (GET_GLOBAL_TYPE(iter)) {
				case GLOBAL_MOB_INTERACTIONS: {
					sprintbit(GET_GLOBAL_TYPE_FLAGS(iter), action_bits, flags, TRUE);
					build_page_display(ch, "%3d. [%5d] %s (%s) %s (%s)", ++found, GET_GLOBAL_VNUM(iter), GET_GLOBAL_NAME(iter), level_range_string(GET_GLOBAL_MIN_LEVEL(iter), GET_GLOBAL_MAX_LEVEL(iter), 0), flags, global_types[GET_GLOBAL_TYPE(iter)]);
					break;
				}
				case GLOBAL_OBJ_INTERACTIONS: {
					sprintbit(GET_GLOBAL_TYPE_FLAGS(iter), extra_bits, flags, TRUE);
					build_page_display(ch, "%3d. [%5d] %s (%s) %s (%s)", ++found, GET_GLOBAL_VNUM(iter), GET_GLOBAL_NAME(iter), level_range_string(GET_GLOBAL_MIN_LEVEL(iter), GET_GLOBAL_MAX_LEVEL(iter), 0), flags, global_types[GET_GLOBAL_TYPE(iter)]);
					break;
				}
				case GLOBAL_MINE_DATA: {
					sprintbit(GET_GLOBAL_TYPE_FLAGS(iter), sector_flags, flags, TRUE);
					build_page_display(ch, "%3d. [%5d] %s - %s (%s)", ++found, GET_GLOBAL_VNUM(iter), GET_GLOBAL_NAME(iter), flags, global_types[GET_GLOBAL_TYPE(iter)]);
					break;
				}
				case GLOBAL_MAP_SPAWNS: {
					ordered_sprintbit(GET_GLOBAL_TYPE_FLAGS(iter), climate_flags, climate_flags_order, TRUE, flags);
					sprintbit(GET_GLOBAL_SPARE_BITS(iter), spawn_flags_short, flags2, TRUE);
					build_page_display(ch, "%3d. [%5d] %s (%s | %s) (%s)", ++found, GET_GLOBAL_VNUM(iter), GET_GLOBAL_NAME(iter), flags, trim(flags2), global_types[GET_GLOBAL_TYPE(iter)]);
					break;
				}
				default: {
					build_page_display(ch, "%3d. [%5d] %s (%s)", ++found, GET_GLOBAL_VNUM(iter), GET_GLOBAL_NAME(iter), global_types[GET_GLOBAL_TYPE(iter)]);
					break;
				}
			}
		}
	}
	
	send_page_display(ch);
	return found;
}


int vnum_mobile(char *searchname, char_data *ch) {
	char_data *mob, *next_mob;
	int found = 0;
	
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		if (multi_isname(searchname, mob->player.name)) {
			build_page_display(ch, "%3d. [%5d] %s%s", ++found, mob->vnum, mob->proto_script ? "[TRIG] " : "", mob->player.short_descr);
		}
	}

	send_page_display(ch);
	return (found);
}


int vnum_object(char *searchname, char_data *ch) {
	obj_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, object_table, iter, next_iter) {
		if (multi_isname(searchname, GET_OBJ_KEYWORDS(iter))) {
			build_page_display(ch, "%3d. [%5d] %s%s", ++found, GET_OBJ_VNUM(iter), iter->proto_script ? "[TRIG] " : "", GET_OBJ_SHORT_DESC(iter));
		}
	}
	
	send_page_display(ch);
	return (found);
}


/**
* Searches the room template db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_room_template(char *searchname, char_data *ch) {
	room_template *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, room_template_table, iter, next_iter) {
		if (multi_isname(searchname, GET_RMT_TITLE(iter))) {
			build_page_display(ch, "%3d. [%5d] %s%s", ++found, GET_RMT_VNUM(iter), iter->proto_script ? "[TRIG] " : "", GET_RMT_TITLE(iter));
		}
	}
	
	send_page_display(ch);
	return found;
}


/**
* Searches the sector db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_sector(char *searchname, char_data *ch) {
	sector_data *sect, *next_sect;
	int found = 0;
	
	HASH_ITER(hh, sector_table, sect, next_sect) {
		if (multi_isname(searchname, GET_SECT_NAME(sect)) || multi_isname(searchname, GET_SECT_TITLE(sect))) {
			build_page_display(ch, "%3d. [%5d] %s", ++found, GET_SECT_VNUM(sect), GET_SECT_NAME(sect));
		}
	}
	
	send_page_display(ch);
	return found;
}


/**
* Searches the trigger db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_trigger(char *searchname, char_data *ch) {
	trig_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, trigger_table, iter, next_iter) {
		if (multi_isname(searchname, GET_TRIG_NAME(iter))) {
			build_page_display(ch, "%3d. [%5d] %s", ++found, GET_TRIG_VNUM(iter), GET_TRIG_NAME(iter));
		}
	}
	
	send_page_display(ch);
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_addnotes) {
	char notes[MAX_ADMIN_NOTES_LENGTH + 1], buf[MAX_STRING_LENGTH];
	player_index_data *index = NULL;
	account_data *acct = NULL;
	
	argument = one_argument(argument, arg);	// target
	skip_spaces(&argument);	// text to add
	
	if (!*arg || !*argument) {
		msg_to_char(ch, "Usage: addnotes <name> <text>\r\n");
		return;
	}
	
	// argument processing
	if (isdigit(*arg)) {
		if (!(acct = find_account(atoi(arg)))) {
			msg_to_char(ch, "Unknown account '%s'.\r\n", arg);
			return;
		}
		if (get_highest_access_level(acct) > GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "You can't add notes for accounts above your access level.\r\n");
			return;
		}
	}
	else {
		if (!(index = find_player_index_by_name(arg))) {
			send_to_char("There is no such player.\r\n", ch);
			return;
		}
		if (index->access_level > GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "You cannot add notes for players of that level.\r\n");
			return;
		}
		if (!(acct = find_account(index->account_id))) {
			msg_to_char(ch, "Unable to find the account for that player.\r\n");
			return;
		}
	}
	
	// final checks
	if (!acct) {
		msg_to_char(ch, "Unknown account.\r\n");
	}
	if (strlen(NULLSAFE(acct->notes)) + strlen(argument) + 2 > MAX_ADMIN_NOTES_LENGTH) {
		msg_to_char(ch, "Notes too long, unable to add text. Use editnotes instead.\r\n");
	}
	else {
		safe_snprintf(notes, sizeof(notes), "%s%s\r\n", NULLSAFE(acct->notes), argument);
		if (acct->notes) {
			free(acct->notes);
		}
		acct->notes = str_dup(notes);
		SAVE_ACCOUNT(acct);
		
		if (index) {
			strcpy(buf, index->fullname);
		}
		else {
			sprintf(buf, "account %d", acct->id);
		}
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has added notes for %s", GET_NAME(ch), buf);
		msg_to_char(ch, "Notes added to %s.\r\n", buf);
	}
}


ACMD(do_advance) {
	char_data *victim;
	char *name = arg, *level = buf2;
	int newlevel, oldlevel, iter;

	two_arguments(argument, name, level);

	if (*name) {
   		if (!(victim = get_char_vis(ch, name, NULL, FIND_CHAR_WORLD))) {
			send_to_char("That player is not here.\r\n", ch);
			return;
		}
	}
	else {
		send_to_char("Advance whom?\r\n", ch);
		return;
	}

	if (GET_ACCESS_LEVEL(ch) <= GET_ACCESS_LEVEL(victim)) {
		send_to_char("Maybe that's not such a great idea.\r\n", ch);
		return;
	}
	if (IS_NPC(victim)) {
		send_to_char("NO! Not on NPC's.\r\n", ch);
		return;
	}
	if (!*level || (newlevel = atoi(level)) <= 0) {
		send_to_char("That's not a level!\r\n", ch);
		return;
	}
	if (newlevel > LVL_IMPL) {
		sprintf(buf, "%d is the highest possible level.\r\n", LVL_IMPL);
		send_to_char(buf, ch);
		return;
	}
	if (newlevel > GET_ACCESS_LEVEL(ch)) {
		send_to_char("Yeah, right.\r\n", ch);
		return;
	}
	if (newlevel == GET_ACCESS_LEVEL(victim)) {
		send_to_char("They are already at that level.\r\n", ch);
		return;
	}
	oldlevel = GET_ACCESS_LEVEL(victim);
	if (newlevel < GET_ACCESS_LEVEL(victim)) {
		start_new_character(victim);
		GET_ACCESS_LEVEL(victim) = newlevel;
		GET_IMMORTAL_LEVEL(victim) = GET_ACCESS_LEVEL(victim) > LVL_MORTAL ? (LVL_TOP - GET_ACCESS_LEVEL(victim)) : -1;
		send_to_char("You are momentarily enveloped by darkness!\r\nYou feel somewhat diminished.\r\n", victim);
	}
	else {
		act("$n makes some strange gestures.\r\n"
			"A strange feeling comes upon you,\r\n"
			"Like a giant hand, light comes down\r\n"
			"from above, grabbing your body, that\r\n"
			"begins to pulse with colored lights\r\n"
			"from inside.\r\n\r\n"
			"Your head seems to be filled with demons\r\n"
			"from another plane as your body dissolves\r\n"
			"to the elements of time and space itself.\r\n"
			"Suddenly a silent explosion of light\r\n"
			"snaps you back to reality.\r\n\r\n"
			"You feel slightly different.", FALSE, ch, 0, victim, TO_VICT | DG_NO_TRIG);
	}

	send_config_msg(ch, "ok_string");

	if (newlevel < oldlevel)
		syslog(SYS_LVL, GET_INVIS_LEV(ch), TRUE, "LVL: %s demoted %s from level %d to %d", GET_NAME(ch), GET_NAME(victim), oldlevel, newlevel);
	else
		syslog(SYS_LVL, GET_INVIS_LEV(ch), TRUE, "LVL: %s has promoted %s to level %d (from %d)", GET_NAME(ch), GET_NAME(victim), newlevel, oldlevel);

	if (oldlevel < LVL_START_IMM && newlevel >= LVL_START_IMM) {
		perform_approve(victim);
		SET_BIT(PRF_FLAGS(victim), PRF_HOLYLIGHT | PRF_ROOMFLAGS | PRF_NOHASSLE);
		
		// turn on all syslogs
		for (iter = 0; *syslog_types[iter] != '\n'; ++iter) {
			SYSLOG_FLAGS(victim) |= BIT(iter);
		}
	}

	GET_ACCESS_LEVEL(victim) = newlevel;
	GET_IMMORTAL_LEVEL(victim) = GET_ACCESS_LEVEL(victim) > LVL_MORTAL ? (LVL_TOP - GET_ACCESS_LEVEL(victim)) : -1;
	SAVE_CHAR(victim);
	check_autowiz(victim);
}


// also do_unapprove
ACMD(do_approve) {
	char_data *vict = NULL;
	bool file = FALSE;

	one_argument(argument, arg);

	if (!*arg)
		msg_to_char(ch, "Usage: %sapprove <character>\r\n", (subcmd == SCMD_UNAPPROVE ? "un" : ""));
	else if (!(vict = find_or_load_player(arg, &file))) {
		msg_to_char(ch, "Unable to find character '%s'.\r\n", arg);
	}
	else if (GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch) && ch != vict) {
		msg_to_char(ch, "You can only %sapprove players below your access level.\r\n", (subcmd == SCMD_UNAPPROVE ? "un" : ""));
	}
	else if (subcmd == SCMD_APPROVE && IS_APPROVED(vict)) {
		msg_to_char(ch, "That player is already approved.\r\n");
	}
	else if (subcmd == SCMD_UNAPPROVE && !IS_APPROVED(vict)) {
		msg_to_char(ch, "That player is not even approved.\r\n");
	}
	else {
		if (subcmd == SCMD_APPROVE) {
			perform_approve(vict);
		}
		else {
			perform_unapprove(vict);
		}
		
		if (!file) {
			msg_to_char(vict, "Your character %s approved.\r\n", (subcmd == SCMD_APPROVE ? "has been" : "is no longer"));
		}
		syslog(SYS_VALID, GET_INVIS_LEV(ch), TRUE, "VALID: %s has been %sapproved by %s", GET_PC_NAME(vict), (subcmd == SCMD_UNAPPROVE ? "un" : ""), GET_NAME(ch));
		msg_to_char(ch, "%s %sapproved.\r\n", GET_PC_NAME(vict), (subcmd == SCMD_UNAPPROVE ? "un" : ""));

		if (file) {
			store_loaded_char(vict);
			vict = NULL;
			file = FALSE;
		}
		
	}
	
	if (vict && file) {
		free_char(vict);
	}
}


ACMD(do_at) {
	room_data *location, *original_loc;

	argument = one_word(argument, buf);

	if (!*buf) {
		send_to_char("You must supply a room number or a name.\r\n", ch);
		return;
	}

	if (!*argument) {
		send_to_char("What do you want to do there?\r\n", ch);
		return;
	}

	if (!(location = find_target_room(ch, buf)))
		return;

	/* a location has been found. */
	original_loc = IN_ROOM(ch);
	char_from_room(ch);
	char_to_room(ch, location);
	command_interpreter(ch, argument);

	/* check if the char is still there */
	if (IN_ROOM(ch) == location) {
		char_from_room(ch);
		char_to_room(ch, original_loc);
	}
	
	msdp_update_room(ch);	// in case it changed
}


ACMD(do_automessage) {
	char part[MAX_STRING_LENGTH], cmd_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], id_arg[MAX_STRING_LENGTH];
	struct automessage *msg, *next_msg;
	int id, iter, type, interval = 5;
	player_index_data *plr;
	
	argument = any_one_arg(argument, cmd_arg);
	
	if (is_abbrev(cmd_arg, "list")) {
		build_page_display_str(ch, "Automessages:");
		
		HASH_ITER(hh, automessages_table, msg, next_msg) {
			switch (msg->timing) {
				case AUTOMSG_REPEATING: {
					safe_snprintf(part, sizeof(part), "%s (%dm)", automessage_types[msg->timing], msg->interval);
					break;
				}
				default: {
					strcpy(part, automessage_types[msg->timing]);
					break;
				}
			}
			
			plr = find_player_index_by_idnum(msg->author);
			build_page_display(ch, "%d. %s (%s): %s", msg->id, part, plr->fullname, msg->msg);
		}
		
		if (!automessages_table) {
			build_page_display_str(ch, " none");
		}
		send_page_display(ch);
	}
	else if (is_abbrev(cmd_arg, "add")) {
		argument = any_one_arg(argument, type_arg);
		
		if (!*type_arg) {
			msg_to_char(ch, "Usage: automessage add <type> [interval] <msg>\r\n");
			return;
		}
		if ((type = search_block(type_arg, automessage_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid message type '%s'.\r\n", type_arg);
			return;
		}
		
		// special handling for repeats
		if (type == AUTOMSG_REPEATING) {
			argument = any_one_arg(argument, type_arg);
			if (!*type_arg) {
				msg_to_char(ch, "Usage: automessage add <type> [interval] <msg>\r\n");
				return;
			}
			if (!isdigit(*type_arg) || (interval = atoi(type_arg)) < 1) {
				msg_to_char(ch, "Invalid repeating interval '%s'.\r\n", type_arg);
				return;
			}
		}
		
		skip_spaces(&argument);
		if (!*argument) {
			msg_to_char(ch, "Usage: automessage add <type> [interval] <msg>\r\n");
			return;
		}
		else if (strlen(argument) >= 255) {
			// fread_string won't be able to read it beyond this.
			msg_to_char(ch, "Message too long.\r\n");
			return;
		}
		
		// ready!
		CREATE(msg, struct automessage, 1);
		msg->id = new_automessage_id();
		msg->timestamp = time(0);
		msg->author = GET_IDNUM(ch);
		msg->timing = type;
		msg->interval = interval;
		msg->msg = str_dup(argument);
		
		HASH_ADD_INT(automessages_table, id, msg);
		
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s added automessage %d: %s", GET_NAME(ch), msg->id, msg->msg);
		msg_to_char(ch, "You create a new message %d: %s\r\n", msg->id, NULLSAFE(msg->msg));
		
		HASH_SORT(automessages_table, sort_automessage_by_data);
		save_automessages();
	}
	else if (is_abbrev(cmd_arg, "change")) {
		argument = any_one_arg(argument, id_arg);
		
		if (!*id_arg || !isdigit(*id_arg) || (id = atoi(id_arg)) < 0) {
			msg_to_char(ch, "Usage: automessage change <id> <property> <value>\r\n");
			return;
		}
		
		HASH_FIND_INT(automessages_table, &id, msg);
		if (!msg) {
			msg_to_char(ch, "Invalid message id %d.\r\n", id);
			return;
		}
		
		argument = any_one_arg(argument, type_arg);
		skip_spaces(&argument);
		
		if (is_abbrev(type_arg, "type")) {
			if (!*argument) {
				msg_to_char(ch, "Change the type to what?\r\n");
				return;
			}
			if ((type = search_block(argument, automessage_types, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid message type '%s'.\r\n", type_arg);
				return;
			}
			
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s changed automessage %d's type to %s", GET_NAME(ch), msg->id, automessage_types[type]);
			msg_to_char(ch, "You change automessage %d's type to %s\r\n", msg->id, automessage_types[type]);
			if (type == AUTOMSG_REPEATING && type != msg->timing) {
				msg->interval = 5;	// safe default
			}
			msg->timing = type;
			HASH_SORT(automessages_table, sort_automessage_by_data);
			save_automessages();
		}
		else if (is_abbrev(type_arg, "interval")) {
			if (msg->timing != AUTOMSG_REPEATING) {
				msg_to_char(ch, "You can only change that on a repeating message.\r\n");
				return;
			}
			if (!*argument || !isdigit(*argument) || (interval = atoi(argument)) < 1) {
				msg_to_char(ch, "Invalid interval.\r\n");
				return;
			}
			
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s changed automessage %d's interval to %d minute%s", GET_NAME(ch), msg->id, interval, PLURAL(interval));
			msg_to_char(ch, "You change automessage %d's interval to %d minute%s\r\n", msg->id, interval, PLURAL(interval));
			msg->interval = interval;
			save_automessages();
		}
		else if (is_abbrev(type_arg, "message")) {
			if (!*argument) {
				msg_to_char(ch, "Change the message to what?\r\n");
				return;
			}
			else if (strlen(argument) >= 255) {
				// fread_string won't be able to read it beyond this.
				msg_to_char(ch, "Message too long.\r\n");
				return;
			}
			
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s changed automessage %d to: %s", GET_NAME(ch), msg->id, argument);
			msg_to_char(ch, "You change automessage %d from: %s\r\nTo: %s\r\n", msg->id, NULLSAFE(msg->msg), argument);
			
			if (msg->msg) {
				free(msg->msg);
			}
			msg->msg = str_dup(argument);
			save_automessages();
		}
		else {
			msg_to_char(ch, "You can change the type, interval, or message.\r\n");
			return;
		}
	}
	else if (is_abbrev(cmd_arg, "delete")) {
		skip_spaces(&argument);
		if (!*argument) {
			msg_to_char(ch, "Delete which automessage (id)?\r\n");
			return;
		}
		
		id = atoi(argument);
		HASH_FIND_INT(automessages_table, &id, msg);
		if (!msg) {
			msg_to_char(ch, "No such message id to delete.\r\n");
			return;
		}
		
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s deleted automessage %d: %s", GET_NAME(ch), msg->id, msg->msg);
		msg_to_char(ch, "You delete message %d: %s\r\n", msg->id, NULLSAFE(msg->msg));
		HASH_DEL(automessages_table, msg);
		free_automessage(msg);
		save_automessages();
	}
	else {
		msg_to_char(ch, "Usage: automessage list\r\n");
		msg_to_char(ch, "       automessage add <type> [interval] <msg>\r\n");
		msg_to_char(ch, "       automessage change <id> <property> <new val>\r\n");
		msg_to_char(ch, "       automessage delete <id>\r\n");
		msg_to_char(ch, "Types: ");
		for (iter = 0; *automessage_types[iter] != '\n'; ++iter) {
			msg_to_char(ch, "%s%s", (iter > 0 ? ", " : ""), automessage_types[iter]);
		}
		msg_to_char(ch, "\r\n");
	}
}


ACMD(do_autostore) {
	obj_data *obj, *next_obj;
	vehicle_data *veh;
	empire_data *emp = ROOM_OWNER(IN_ROOM(ch));

	one_argument(argument, arg);

	if (!emp && !*arg) {
		msg_to_char(ch, "Nobody owns this spot. Use purge instead.\r\n");
	}
	else if (*arg) {
		generic_find(arg, NULL, FIND_OBJ_ROOM | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, NULL, &obj, &veh);
		
		if (obj) {
			act("$n auto-stores $p.", FALSE, ch, obj, NULL, TO_ROOM | DG_NO_TRIG);
			perform_force_autostore(obj, emp, GET_ISLAND_ID(IN_ROOM(ch)));
			read_vault(emp);
		}
		else if (veh) {
			act("$n auto-stores items in $V.", FALSE, ch, NULL, veh, TO_ROOM | DG_NO_TRIG | ACT_VEH_VICT);
			
			DL_FOREACH_SAFE2(VEH_CONTAINS(veh), obj, next_obj, next_content) {
				perform_force_autostore(obj, (VEH_OWNER(veh) && VEH_OWNER(veh) != emp) ? VEH_OWNER(veh) : emp, GET_ISLAND_ID(IN_ROOM(ch)));
			}
			read_vault((VEH_OWNER(veh) && VEH_OWNER(veh) != emp) ? VEH_OWNER(veh) : emp);
		}
		else {
			send_to_char("Nothing here by that name.\r\n", ch);
			return;
		}

		send_config_msg(ch, "ok_string");
	}
	else {			// no argument. clean out the room
		act("$n gestures...", FALSE, ch, 0, 0, TO_ROOM | DG_NO_TRIG);
		send_to_room("The world seems a little cleaner.\r\n", IN_ROOM(ch));
		force_autostore(IN_ROOM(ch));
	}
}


ACMD(do_autowiz) {
	check_autowiz(ch);
}


ACMD(do_breakreply) {
	char_data *iter;
	
	DL_FOREACH(character_list, iter) {
		if (IS_NPC(iter) || GET_ACCESS_LEVEL(iter) >= GET_ACCESS_LEVEL(ch)) {
			continue;
		}
		if (GET_LAST_TELL(iter) != GET_IDNUM(ch)) {
			continue;
		}
		
		GET_LAST_TELL(iter) = NOBODY;
	}
	
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		msg_to_char(ch, "Players currently in-game can no longer reply to you (unless you send them another tell).\r\n");
	}
}


ACMD(do_clearabilities) {
	char_data *vict;
	char typestr[MAX_STRING_LENGTH];
	skill_data *skill = NULL;
	bool all;

	argument = one_argument(argument, arg);
	skip_spaces(&argument);
	vict = get_player_vis(ch, arg, FIND_CHAR_WORLD);
	
	if (!vict || !*argument) {
		msg_to_char(ch, "Usage: clearabilities <name> <skill | all>\r\n");
		return;
	}
	if (!str_cmp(argument, "all")) {
		all = TRUE;
	}
	else {
		all = FALSE;
		if (!(skill = find_skill(argument))) {
			msg_to_char(ch, "Invalid skill '%s'.\r\n", argument);
			return;
		}
	}
	
	clear_char_abilities(vict, all ? NO_SKILL : SKILL_VNUM(skill));
	
	if (all) {
		*typestr = '\0';
	}
	else {
		sprintf(typestr, "%s ", SKILL_NAME(skill));
	}
	
	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has cleared %s's %sabilities", GET_REAL_NAME(ch), GET_REAL_NAME(vict), typestr);
	msg_to_char(vict, "Your %sabilities have been reset.\r\n", typestr);
	msg_to_char(ch, "You have cleared %s's %sabilities.\r\n", GET_REAL_NAME(vict), typestr);
}


ACMD(do_date) {
	char *tmstr;
	time_t mytime;
	int d, h, m;

	if (subcmd == SCMD_DATE)
		mytime = time(0);
	else
		mytime = boot_time;

	tmstr = (char *) asctime(localtime(&mytime));
	*(tmstr + strlen(tmstr) - 1) = '\0';

	if (subcmd == SCMD_DATE)
		sprintf(buf, "Current machine time: %s\r\n", tmstr);
	else {
		mytime = time(0) - boot_time;
		d = mytime / 86400;
		h = (mytime / 3600) % 24;
		m = (mytime / 60) % 60;

		sprintf(buf, "Up since %s: %d day%s, %d:%02d\r\n", tmstr, d, ((d == 1) ? "" : "s"), h, m);
	}

	send_to_char(buf, ch);
}


ACMD(do_dc) {
	descriptor_data *d;
	int num_to_dc;

	one_argument(argument, arg);
	if (!(num_to_dc = atoi(arg))) {
		send_to_char("Usage: DC <user number> (type USERS for a list)\r\n", ch);
		return;
	}
	for (d = descriptor_list; d && d->desc_num != num_to_dc; d = d->next);

	if (!d) {
		send_to_char("No such connection.\r\n", ch);
		return;
	}
	if (d->character && GET_ACCESS_LEVEL(d->character) >= GET_ACCESS_LEVEL(ch)) {
		if (!CAN_SEE(ch, d->character))
			send_to_char("No such connection.\r\n", ch);
		else
			send_to_char("Umm... maybe that's not such a good idea...\r\n", ch);
		return;
	}

	/* We used to just close the socket here using close_socket(), but
	 * various people pointed out this could cause a crash if you're
	 * closing the person below you on the descriptor list.  Just setting
	 * to CON_CLOSE leaves things in a massively inconsistent state so I
	 * had to add this new flag to the descriptor. -je
	 *
	 * It is a much more logical extension for a CON_DISCONNECT to be used
	 * for in-game socket closes and CON_CLOSE for out of game closings.
	 * This will retain the stability of the close_me hack while being
	 * neater in appearance. -gg 12/1/97
	 *
	 * For those unlucky souls who actually manage to get disconnected
	 * by two different immortals in the same 1/10th of a second, we have
	 * the below 'if' check. -gg 12/17/99
	 */
	if (STATE(d) == CON_DISCONNECT || STATE(d) == CON_CLOSE)
		send_to_char("They're already being disconnected.\r\n", ch);
	else {
		/*
		 * Remember that we can disconnect people not in the game and
		 * that rather confuses the code when it expected there to be
		 * a character context.
		 */
		if (STATE(d) == CON_PLAYING)
			STATE(d) = CON_DISCONNECT;
		else
			STATE(d) = CON_CLOSE;

		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has disconnected %s", GET_REAL_NAME(ch), (d->character ? GET_REAL_NAME(d->character) : "an empty connection"));
		sprintf(buf, "Connection #%d closed.\r\n", num_to_dc);
		send_to_char(buf, ch);
		log("Connection closed by %s.", GET_NAME(ch));
	}
}


// do_directions
ACMD(do_distance) {
	const char *dir_str;
	room_data *target;
	int dist;
	
	skip_spaces(&argument);
	
	if (!IS_IMMORTAL(ch) && !IS_NPC(ch) && !HAS_NAVIGATION(ch)) {
		msg_to_char(ch, "You don't know how to navigate or determine distances.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Get the direction and distance to where?\r\n");
	}
	else if (!IS_IMMORTAL(ch) && ((!isdigit(*argument) && *argument != '(') || !strchr(argument, ','))) {
		msg_to_char(ch, "You can only find distances to coordinates.\r\n");
	}
	else if (!(target = parse_room_from_coords(argument)) && !(target = find_target_room(ch, argument))) {
		msg_to_char(ch, "Unknown target.\r\n");
	}
	else {	
		dir_str = get_partial_direction_to(ch, IN_ROOM(ch), target, FALSE);
		dist = compute_distance(IN_ROOM(ch), target);
		msg_to_char(ch, "(%d, %d) is %d tile%s %s.\r\n", X_COORD(target), Y_COORD(target), dist, PLURAL(dist), (*dir_str ? dir_str : "away"));
	}
}


// this also handles emote
ACMD(do_echo) {
	char string[MAX_INPUT_LENGTH], lbuf[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH];
	char hbuf[MAX_INPUT_LENGTH];
	char *ptr, *end;
	char_data *vict = NULL, *tmp_char, *c;
	vehicle_data *tmp_veh = NULL;
	obj_data *obj = NULL;
	int len;
	bool needs_name = TRUE;

	skip_spaces(&argument);
	delete_doubledollar(argument);
	strcpy(string, argument);

	if (!*argument) {
		send_to_char("Yes... but what?\r\n", ch);
		return;
	}
	if (subcmd == SCMD_EMOTE && strstr(string, "$O")) {
		msg_to_char(ch, "You can't use the $O symbol in an emote.\r\n");
		return;
	}

	// emote features like $n to move your name, @target, #item
	if (subcmd == SCMD_ECHO || strstr(string, "$n")) {
		needs_name = FALSE;
	}
	// find target
	if ((ptr = strchr(string, '@')) && *(ptr+1) != ' ') {
		one_argument(ptr+1, lbuf);
		if ((end = strchr(lbuf, '.')) || (end = strchr(lbuf, '!')) || (end = strchr(lbuf, ','))) {
			*end = '\0';
		}
		len = strlen(lbuf);
		vict = get_char_vis(ch, lbuf, NULL, FIND_CHAR_ROOM);

		if (vict) {		
			// replace with $N
			*ptr = '\0';
			strcpy(temp, string);
			strcat(temp, "$N");
			strcat(temp, ptr + len + 1);
			strcpy(string, temp);
		}
	}
	if ((ptr = strchr(string, '#')) && *(ptr+1) != ' ') {
		one_argument(ptr+1, lbuf);
		if ((end = strchr(lbuf, '.')) || (end = strchr(lbuf, '!')) || (end = strchr(lbuf, ','))) {
			*end = '\0';
		}
		len = strlen(lbuf);
		generic_find(lbuf, NULL, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &tmp_char, &obj, &tmp_veh);
		
		if (obj) {
			// replace with $p
			*ptr = '\0';
			strcpy(temp, string);
			strcat(temp, "$p");
			strcat(temp, ptr + len + 1);
			strcpy(string, temp);
		}
	}

	if (needs_name) {
		sprintf(lbuf, "$n %s", string);
	}
	else {
		strcpy(lbuf, string);
	}
	
	strcat(lbuf, "&0");
	
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		if (ch->desc) {
			clear_last_act_message(ch->desc);
		}
		
		if (subcmd == SCMD_EMOTE && !IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) {
			msg_to_char(ch, "&%c", GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE));
		}
		
		// send message
		act(lbuf, FALSE, ch, obj, vict, TO_CHAR | TO_IGNORE_BAD_CODE | DG_NO_TRIG);
		
		// channel history
		if (ch->desc && ch->desc->last_act_message) {
			// the message was sent via act(), we can retrieve it from the desc			
			if (subcmd == SCMD_EMOTE && !IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE)) {
				sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_EMOTE));
			}
			else {
				*hbuf = '\0';
			}
			strcat(hbuf, ch->desc->last_act_message);
			add_to_channel_history(ch, CHANNEL_HISTORY_SAY, ch, hbuf, FALSE, 0, NOTHING);
		}
	}

	if (vict) {
		// clear last act messages for everyone in the room
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
			if (c->desc && c != ch && c != vict) {
				clear_last_act_message(c->desc);
				
				if (subcmd == SCMD_EMOTE && !IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					msg_to_char(c, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
				}
			}
		}
		
		act(lbuf, FALSE, ch, obj, vict, TO_NOTVICT | TO_IGNORE_BAD_CODE | DG_NO_TRIG);

		// fetch and store channel history for the room
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
			if (c->desc && c != ch && c != vict && !is_ignoring(c, ch) && c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				if (subcmd == SCMD_EMOTE && !IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
				}
				else {
					*hbuf = '\0';
				}
				strcat(hbuf, c->desc->last_act_message);
				add_to_channel_history(c, CHANNEL_HISTORY_SAY, ch, hbuf, (IS_MORPHED(ch) || IS_DISGUISED(ch)), 0, NOTHING);
			}
			else if (c->desc && c != ch && c != vict) {
				// just in case
				msg_to_char(c, "&0");
			}
		}
		
		// prepare to send to vict
		if ((ptr = strstr(lbuf, "$N"))) {
			*ptr = '\0';
			strcpy(temp, lbuf);
			strcat(temp, "you");
			strcat(temp, ptr+2);
			
			strcpy(lbuf, temp);
		}
		
		if (!is_ignoring(vict, ch)) {
			if (vict->desc) {
				clear_last_act_message(vict->desc);
			}
			
			if (subcmd == SCMD_EMOTE && !IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE)) {
				msg_to_char(vict, "&%c", GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE));
			}
		
			// send to vict
			if (vict != ch) {
				act(lbuf, FALSE, ch, obj, vict, TO_VICT | TO_IGNORE_BAD_CODE | DG_NO_TRIG);
			}
		
			// channel history
			if (vict->desc && vict->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc			
				if (subcmd == SCMD_EMOTE && !IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE)) {
					sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_EMOTE));
				}
				else {
					*hbuf = '\0';
				}
				strcat(hbuf, vict->desc->last_act_message);
				add_to_channel_history(vict, CHANNEL_HISTORY_SAY, ch, hbuf, (IS_MORPHED(ch) || IS_DISGUISED(ch)), 0, NOTHING);
			}
		}
	}
	else {
		// clear last act messages for everyone in the room
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
			if (c->desc && c != ch) {
				clear_last_act_message(c->desc);
							
				if (subcmd == SCMD_EMOTE && !IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					msg_to_char(c, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
				}
			}
		}
		
		// send to room
		act(lbuf, FALSE, ch, obj, vict, TO_ROOM | TO_NOT_IGNORING | TO_IGNORE_BAD_CODE | DG_NO_TRIG);

		// fetch and store channel history for the room
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
			if (c->desc && c != ch && !is_ignoring(c, ch) && c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc			
				if (subcmd == SCMD_EMOTE && !IS_NPC(c) && GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE)) {
					sprintf(hbuf, "&%c", GET_CUSTOM_COLOR(c, CUSTOM_COLOR_EMOTE));
				}
				else {
					*hbuf = '\0';
				}
				strcat(hbuf, c->desc->last_act_message);
				add_to_channel_history(c, CHANNEL_HISTORY_SAY, ch, hbuf, (IS_MORPHED(ch) || IS_DISGUISED(ch)), 0, NOTHING);
			}
			else if (c->desc && c != ch) {
				// just in case
				msg_to_char(c, "&0");
			}
		}
	}
}


ACMD(do_edelete) {
	empire_data *e;

	skip_spaces(&argument);
	if (!*argument)
		msg_to_char(ch, "What empire do you want to delete?\r\n");
	else if (!(e = get_empire_by_name(argument)))
		msg_to_char(ch, "Invalid empire.\r\n");
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has deleted empire %s", GET_NAME(ch), EMPIRE_NAME(e));
		delete_empire(e);
		msg_to_char(ch, "Empire deleted.\r\n");
	}
}


ACMD(do_editnotes) {
	player_index_data *index = NULL;
	char buf[MAX_STRING_LENGTH];
	account_data *acct = NULL;
	descriptor_data *desc;
	
	one_argument(argument, arg);
	
	// preliminaries
	if (!ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
		return;
	}
	if (ch->desc->str) {
		msg_to_char(ch, "You're already editing something else.\r\n");
		return;
	}
	if (!*arg) {
		msg_to_char(ch, "Edit notes for whom?\r\n");
		return;
	}
	
	// argument processing
	if (isdigit(*arg)) {
		if (!(acct = find_account(atoi(arg)))) {
			msg_to_char(ch, "Unknown account '%s'.\r\n", arg);
			return;
		}
		if (get_highest_access_level(acct) > GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "You can't edit notes for accounts above your access level.\r\n");
			return;
		}
	}
	else {
		if (!(index = find_player_index_by_name(arg))) {
			send_to_char("There is no such player.\r\n", ch);
			return;
		}
		if (!(acct = find_account(index->account_id))) {
			msg_to_char(ch, "Unable to find account for that player.\r\n");
			return;
		}
		if (get_highest_access_level(acct) >= GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "You cannot edit notes for accounts above your access level.\r\n");
			return;
		}
	}
	
	if (!acct) {
		msg_to_char(ch, "Unable to find account.\r\n");
		return;
	}
	
	// ensure nobody else is writing notes for the same person
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (desc != ch->desc && desc->str == &(acct->notes)) {
			msg_to_char(ch, "Someone is already editing notes for that account.\r\n");
			return;
		}
	}
	
	// good to go
	if (index) {
		sprintf(buf, "notes for %s", index->fullname);
	}
	else {
		sprintf(buf, "notes for account %d", acct->id);
	}
	start_string_editor(ch->desc, buf, &(acct->notes), MAX_ADMIN_NOTES_LENGTH-1, TRUE);
	ch->desc->notes_id = acct->id;

	act("$n begins editing some notes.", TRUE, ch, FALSE, FALSE, TO_ROOM | DG_NO_TRIG);
}


ACMD(do_endwar) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	empire_data *iter, *next_iter, *other, *first = NULL, *second = NULL;
	struct empire_political_data *pol, *rev;
	bool any, line;
	
	argument = any_one_word(argument, arg1);
	argument = any_one_word(argument, arg2);
	
	if (!*arg1 && !*arg2) {	// no args: list active wars
		msg_to_char(ch, "Active wars:\r\n");
		
		any = FALSE;
		HASH_ITER(hh, empire_table, iter, next_iter) {
			line = FALSE;
			
			LL_FOREACH(EMPIRE_DIPLOMACY(iter), pol) {
				if (IS_SET(pol->type, DIPL_WAR) && (other = real_empire(pol->id))) {
					if (!line) {
						msg_to_char(ch, " %s%s\t0 vs ", EMPIRE_BANNER(iter), EMPIRE_NAME(iter));
					}
					
					msg_to_char(ch, "%s%s%s\t0", (line ? ", " : ""), EMPIRE_BANNER(other), EMPIRE_NAME(other));
					any = line = TRUE;
				}
			}
			
			if (line) {
				msg_to_char(ch, "\r\n");
			}
		}
		
		if (!any) {
			msg_to_char(ch, " none\r\n");
		}
	}	// end no-arg
	else if (!(first = get_empire(arg1))) {
		msg_to_char(ch, "Invalid empire '%s'.\r\n", arg1);
	}
	else if (*arg2 && !(second = get_empire(arg2))) {
		msg_to_char(ch, "Invalid empire '%s'.\r\n", arg2);
	}
	else {	// ok now ends wars
		any = FALSE;
		
		// we are guaranteed a "first" empire but not a "second"
		LL_FOREACH(EMPIRE_DIPLOMACY(first), pol) {
			if (second && pol->id != EMPIRE_VNUM(second)) {
				continue;	// doing 1? or all?
			}
			if (!IS_SET(pol->type, DIPL_WAR)) {
				continue;	// not war
			}
			
			other = (second ? second : real_empire(pol->id));
			
			// remove war, set distrust
			REMOVE_BIT(pol->type, DIPL_WAR);
			SET_BIT(pol->type, DIPL_DISTRUST);
			pol->start_time = time(0);
			
			// and back
			if ((rev = find_relation(other, first))) {
				REMOVE_BIT(rev->type, DIPL_WAR);
				SET_BIT(rev->type, DIPL_DISTRUST);
				rev->start_time = time(0);
			}
			
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "ABUSE: DIPL: %s has ended the war between %s and %s", GET_NAME(ch), EMPIRE_NAME(first), EMPIRE_NAME(other));
			log_to_empire(first, ELOG_DIPLOMACY, "The war with %s is over", EMPIRE_NAME(other));
			log_to_empire(other, ELOG_DIPLOMACY, "The war with %s is over", EMPIRE_NAME(first));
			any = TRUE;
		}
		
		if (!any && second) {
			msg_to_char(ch, "You didn't find a war to end between %s and %s.\r\n", EMPIRE_NAME(first), EMPIRE_NAME(second));
		}
		else if (!any) {
			msg_to_char(ch, "%s is not at war with anybody.\r\n", EMPIRE_NAME(first));
		}
	}
}


/*
 * do_file
 *  by Haddixx <haddixx@megamed.com>
 * A neat little utility that looks up the last X lines of a given file.
 * I've made some minor improvements to it.. I can't just leave well-enough
 * alone.  Anyway, it's a useful toy and I'm sure EmpireMUD users will love
 * it.
 *
 * see config.c for the list of loadable files
 */
ACMD(do_file) {
	FILE *req_file;
 	int cur_line = 0, num_lines = 0, req_lines = 0, i, l;
	char field[MAX_INPUT_LENGTH], value[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH];

	skip_spaces(&argument);

	if (!*argument) {
		msg_to_char(ch, "USAGE: file <option> <num lines>\r\n\r\nFile options:\r\n");
		for (i = 1; file_lookup[i].level; i++)
			if (file_lookup[i].level <= GET_ACCESS_LEVEL(ch))
				msg_to_char(ch, "%-15s%s\r\n", file_lookup[i].cmd, file_lookup[i].file);
		return;
	}

	strcpy(arg, two_arguments(argument, field, value));

	for (l = 0; *file_lookup[l].cmd != '\n' && strn_cmp(field, file_lookup[l].cmd, strlen(field)); l++);

	if (*file_lookup[l].cmd == '\n') {
		msg_to_char(ch, "That is not a valid option!\r\n");
		return;
	}

	if (GET_ACCESS_LEVEL(ch) < file_lookup[l].level) {
		msg_to_char(ch, "You are not godly enough to view that file!\r\n");
		return;
	}

	req_lines = (*value ? atoi(value) : 15);

	/* open the requested file */
	if (!(req_file = fopen(file_lookup[l].file, "r"))) {
		syslog(SYS_ERROR, GET_INVIS_LEV(ch), TRUE, "SYSERR: Error opening file %s using 'file' command", file_lookup[l].file);
		return;
	}

	/* count lines in requested file */
	get_line(req_file,buf);
	while (!feof(req_file)) {
		num_lines++;
		get_line(req_file,buf);
	}
	fclose(req_file);

	/* Limit # of lines printed to # requested or # of lines in file or 150 lines */
	req_lines = MIN(150, MIN(req_lines, num_lines));

	/* re-open */
	req_file = fopen(file_lookup[l].file, "r");
	*output = '\0';

	/* and print the requested lines */
	get_line(req_file, buf);
	while (!feof(req_file) && (strlen(output) + strlen(buf) + 2) < MAX_STRING_LENGTH) {
		cur_line++;
		if (cur_line > (num_lines - req_lines)) {
			strcat(output, buf);
			strcat(output, "\r\n");
		}
		get_line(req_file, buf);
	}
	page_string(ch->desc, show_color_codes(output), TRUE);

	fclose(req_file);
}


ACMD(do_force) {
	descriptor_data *i, *next_desc;
	char_data *vict, *next_force;
	char to_force[MAX_INPUT_LENGTH + 2];

	half_chop(argument, arg, to_force);

	if (!*arg || !*to_force)
		send_to_char("Whom do you wish to force do what?\r\n", ch);
	else if ((GET_ACCESS_LEVEL(ch) < LVL_IMPL) || (str_cmp("all", arg) && str_cmp("room", arg))) {
		if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD)))
			send_config_msg(ch, "no_person");
		else if (!REAL_NPC(vict) && GET_REAL_LEVEL(ch) <= GET_REAL_LEVEL(vict))
			send_to_char("No, no, no!\r\n", ch);
		else {
			send_config_msg(ch, "ok_string");
			sprintf(buf1, "$n has forced you to '%s'.", to_force);
			act(buf1, TRUE, ch, NULL, vict, TO_VICT | DG_NO_TRIG);
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s forced %s to %s", GET_NAME(ch), GET_NAME(vict), to_force);
			command_interpreter(vict, to_force);
		}
	}
	else if (!str_cmp("room", arg)) {
		send_config_msg(ch, "ok_string");
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s forced room %s to %s", GET_NAME(ch), room_log_identifier(IN_ROOM(ch)), to_force);
		
		DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_force, next_in_room) {
			if (!REAL_NPC(vict) && GET_REAL_LEVEL(vict) >= GET_REAL_LEVEL(ch))
				continue;
			sprintf(buf1, "$n has forced you to '%s'.", to_force);
			act(buf1, TRUE, ch, NULL, vict, TO_VICT | DG_NO_TRIG);
			command_interpreter(vict, to_force);
		}
	}
	else { /* force all */
		send_config_msg(ch, "ok_string");
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s forced all to %s", GET_NAME(ch), to_force);

		for (i = descriptor_list; i; i = next_desc) {
			next_desc = i->next;

			if (STATE(i) != CON_PLAYING || !(vict = i->character) || (!REAL_NPC(vict) && GET_REAL_LEVEL(vict) >= GET_REAL_LEVEL(ch)))
				continue;
			sprintf(buf1, "$n has forced you to '%s'.", to_force);
			act(buf1, TRUE, ch, NULL, vict, TO_VICT | DG_NO_TRIG);
			command_interpreter(vict, to_force);
		}
	}
}


ACMD(do_forgive) {
	char_data *vict;
	bool any;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Forgive whom?r\n");
	}
	else if (!(vict = get_player_vis(ch, arg, FIND_CHAR_WORLD | FIND_NO_DARK))) {
		send_config_msg(ch, "no_person");
	}
	else if (IS_NPC(vict)) {
		msg_to_char(ch, "You can't forgive an NPC for anything.\r\n");
	}
	else {
		any = FALSE;
		
		if (get_cooldown_time(vict, COOLDOWN_HOSTILE_FLAG) > 0) {
			remove_cooldown_by_type(vict, COOLDOWN_HOSTILE_FLAG);
			msg_to_char(ch, "Hostile flag forgiven.\r\n");
			if (ch != vict) {
				act("$n has forgiven your hostile flag.", FALSE, ch, NULL, vict, TO_VICT | DG_NO_TRIG);
			}
			any = TRUE;
		}
		
		if (affected_by_spell(vict, ATYPE_HOSTILE_DELAY)) {
			affect_from_char(vict, ATYPE_HOSTILE_DELAY, FALSE);
			msg_to_char(ch, "Hostile delay forgiven.\r\n");
			if (ch != vict) {
				act("$n has forgiven your hostile delay.", FALSE, ch, NULL, vict, TO_VICT | DG_NO_TRIG);
			}
			any = TRUE;
		}
		
		if (get_cooldown_time(vict, COOLDOWN_ROGUE_FLAG) > 0) {
			remove_cooldown_by_type(vict, COOLDOWN_ROGUE_FLAG);
			msg_to_char(ch, "Rogue flag forgiven.\r\n");
			if (ch != vict) {
				act("$n has forgiven your rogue flag.", FALSE, ch, NULL, vict, TO_VICT | DG_NO_TRIG);
			}
			any = TRUE;
		}
		
		if (get_cooldown_time(vict, COOLDOWN_LEFT_EMPIRE) > 0) {
			remove_cooldown_by_type(vict, COOLDOWN_LEFT_EMPIRE);
			msg_to_char(ch, "Defect timer forgiven.\r\n");
			if (ch != vict) {
				act("$n has forgiven your empire defect timer.", FALSE, ch, NULL, vict, TO_VICT | DG_NO_TRIG);
			}
			any = TRUE;
		}
		
		if (get_cooldown_time(vict, COOLDOWN_PVP_FLAG) > 0) {
			remove_cooldown_by_type(vict, COOLDOWN_PVP_FLAG);
			msg_to_char(ch, "PVP cooldown forgiven.\r\n");
			if (ch != vict) {
				act("$n has forgiven your PVP cooldown.", FALSE, ch, NULL, vict, TO_VICT | DG_NO_TRIG);
			}
			any = TRUE;
		}
		
		if (any) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has forgiven %s", GET_NAME(ch), GET_NAME(vict));
		}
		else {
			act("There's nothing you can forgive $N for.", FALSE, ch, NULL, vict, TO_CHAR | DG_NO_TRIG);
		}
	}
}


ACMD(do_fullsave) {
	unsigned long long time;
	
	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has triggered a full map save", GET_REAL_NAME(ch));
	syslog(SYS_INFO, 0, FALSE, "Updating zone files...");
	
	time = microtime();
	write_fresh_binary_map_file();
	write_all_wld_files();
	write_whole_binary_world_index();
	send_config_msg(ch, "ok_string");
	
	if (ch->desc) {
		stack_msg_to_desc(ch->desc, "World save time: %.2f seconds\r\n", (microtime() - time) / 1000000.0);
	}
}


ACMD(do_gecho) {
	descriptor_data *pt;

	skip_spaces(&argument);
	delete_doubledollar(argument);

	if (!IS_NPC(ch) && ACCOUNT_FLAGGED(ch, ACCT_MUTED))
		msg_to_char(ch, "You can't use gecho while muted.\r\n");
	else if (!*argument)
		send_to_char("That must be a mistake...\r\n", ch);
	else {
		sprintf(buf, "%s\r\n", argument);
		for (pt = descriptor_list; pt; pt = pt->next) {
			if (STATE(pt) == CON_PLAYING && pt->character && pt->character != ch) {
				send_to_char(buf, pt->character);
				
				if (GET_ACCESS_LEVEL(pt->character) >= GET_ACCESS_LEVEL(ch)) {
					msg_to_char(pt->character, "(gecho by %s)\r\n", GET_REAL_NAME(ch));
				}
			}
		}
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			send_to_char(buf, ch);
		}
	}
}


ACMD(do_goto) {
	room_data *location;
	char_data *iter;

	one_word(argument, arg);
	
	if (!(location = parse_room_from_coords(argument)) && !(location = find_target_room(ch, arg))) {
		return;
	}

	if (subcmd != SCMD_GOTO && !can_use_room(ch, location, GUESTS_ALLOWED)) {
		msg_to_char(ch, "You can't teleport there.\r\n");
		return;
	}
	
	// wizhide safety
	if (!PRF_FLAGGED(ch, PRF_WIZHIDE) && GET_INVIS_LEV(ch) < LVL_START_IMM) {
		DL_FOREACH2(ROOM_PEOPLE(location), iter, next_in_room) {
			if (ch == iter || !IS_IMMORTAL(iter)) {
				continue;
			}
			if (GET_INVIS_LEV(iter) > GET_ACCESS_LEVEL(ch)) {
				continue;	// ignore -- ch can't see them at all
			}
			if (PRF_FLAGGED(iter, PRF_WIZHIDE)) {
				msg_to_char(ch, "You can't teleport there because someone there is using wizhide, and you aren't.\r\n");
				return;
			}
		}
	}
	
	perform_goto(ch, location);
}


ACMD(do_hostile) {
	char_data *vict;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Mark whom hostile?\r\n");
	}
	else if (!(vict = get_player_vis(ch, arg, FIND_CHAR_WORLD | FIND_NO_DARK))) {
		send_config_msg(ch, "no_person");
	}
	else if (IS_NPC(vict)) {
		msg_to_char(ch, "You can't mark an NPC hostile.\r\n");
	}
	else if (GET_ACCESS_LEVEL(vict) > GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else {
		add_cooldown(vict, COOLDOWN_HOSTILE_FLAG, config_get_int("hostile_flag_time") * SECS_PER_REAL_MIN);
		msg_to_char(vict, "You are now hostile!\r\n");
		if (ch != vict) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has marked %s as hostile", GET_NAME(ch), GET_NAME(vict));
			act("$N is now hostile.", FALSE, ch, NULL, vict, TO_CHAR | DG_NO_TRIG);
		}
	}
}


ACMD(do_instance) {
	struct instance_data *inst;
	int count;
	
	argument = any_one_arg(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		count = 0;
		DL_FOREACH(instance_list, inst) {
			++count;
		}
		
		msg_to_char(ch, "Usage: instance <list | add | delete | deleteall | info | nearby | reset | spawn | test> [argument]\r\n");
		msg_to_char(ch, "There are %d live instances.\r\n", count);
	}
	else if (is_abbrev(arg, "list")) {
		do_instance_list(ch, argument);
	}
	else if (is_abbrev(arg, "add")) {
		do_instance_add(ch, argument);
	}
	else if (is_abbrev(arg, "delete")) {
		do_instance_delete(ch, argument);
	}
	else if (is_abbrev(arg, "deleteall")) {
		do_instance_delete_all(ch, argument);
	}
	else if (is_abbrev(arg, "information")) {
		do_instance_info(ch, argument);
	}
	else if (is_abbrev(arg, "nearby")) {
		do_instance_nearby(ch, argument);
	}
	else if (is_abbrev(arg, "reset")) {
		do_instance_reset(ch, argument);
	}
	else if (is_abbrev(arg, "spawn")) {
		do_instance_spawn(ch, argument);
	}
	else if (is_abbrev(arg, "test")) {
		do_instance_test(ch, argument);
	}
	else {
		msg_to_char(ch, "Invalid instance command.\r\n");
	}
}


ACMD(do_invis) {
	int level;

	if (IS_NPC(ch)) {
		send_to_char("You can't do that!\r\n", ch);
		return;
	}

	one_argument(argument, arg);
	if (!*arg) {
		if (GET_INVIS_LEV(ch) > 0)
			perform_immort_vis(ch);
		else
			perform_immort_invis(ch, GET_ACCESS_LEVEL(ch));
	}
	else {
		level = atoi(arg);
		if (level > GET_ACCESS_LEVEL(ch))
			send_to_char("You can't go invisible above your own level.\r\n", ch);
		else if (level < 1)
			perform_immort_vis(ch);
		else
			perform_immort_invis(ch, level);
	}
}


ACMD(do_island) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], flags[256];
	struct island_info *isle, *next_isle;
	room_data *center;
	bitvector_t old;
	struct page_display *line;
	
	argument = any_one_arg(argument, arg1);	// command
	skip_spaces(&argument);	// remainder
	
	if (!ch->desc) {
		// don't bother
		return;
	}
	else if (is_abbrev(arg1, "list")) {
		build_page_display(ch, "Islands:");
		
		HASH_ITER(hh, island_table, isle, next_isle) {
			if (*argument && !multi_isname(argument, isle->name)) {
				continue;
			}
			
			center = real_room(isle->center);
			
			line = build_page_display(ch, "%2d. %s (%d, %d), size %d, levels %d-%d", isle->id, isle->name, (center ? FLAT_X_COORD(center) : -1), (center ? FLAT_Y_COORD(center) : -1), isle->tile_size, isle->min_level, isle->max_level);
			if (isle->flags) {
				sprintbit(isle->flags, island_bits, flags, TRUE);
				append_page_display_line(line, ", %s", flags);
			}
		}
		
		send_page_display(ch);
	}
	else if (is_abbrev(arg1, "rename")) {
		argument = one_argument(argument, arg2);
		skip_spaces(&argument);
		
		if (!*arg2 || !*argument || (!isdigit(*arg2) && strcmp(arg2, "-1"))) {
			msg_to_char(ch, "Usage: island rename <id> <name>\r\n");
		}
		else if (!(isle = get_island(atoi(arg2), FALSE))) {
			msg_to_char(ch, "Unknown island id '%s'.\r\n", arg2);
		}
		else if (strlen(argument) > MAX_ISLAND_NAME) {
			msg_to_char(ch, "Island names may not be longer than %d characters.\r\n", MAX_ISLAND_NAME);
		}
		else if (strchr(argument, '"')) {
			msg_to_char(ch, "Island names may not contain quotation marks.\r\n");
		}
		else {
			send_config_msg(ch, "ok_string");
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has renamed island %d (%s) to %s", GET_NAME(ch), isle->id, NULLSAFE(isle->name), argument);
			
			if (isle->name) {
				free(isle->name);
			}
			isle->name = str_dup(argument);
			
			save_island_table();
		}
	}
	else if (is_abbrev(arg1, "description")) {
		if (!*argument) {
			msg_to_char(ch, "Usage: island description <id>\r\n");
		}
		else if (!ch->desc) {
			msg_to_char(ch, "You can't edit text right now.\r\n");
		}
		else if (ch->desc->str) {
			msg_to_char(ch, "You are already editing something else right now.\r\n");
		}
		else if (isdigit(*argument) && !(isle = get_island(atoi(argument), FALSE))) {
			msg_to_char(ch, "Unknown island id '%s'.\r\n", argument);
		}
		else if (!isdigit(*argument) && !(isle = get_island_by_name(ch, argument))) {
			msg_to_char(ch, "Unknown island '%s'.\r\n", argument);
		}
		else {
			start_string_editor(ch->desc, "island description", &isle->desc, MAX_STRING_LENGTH, TRUE);
			ch->desc->island_desc_id = isle->id;
		}
	}
	else if (is_abbrev(arg1, "flags")) {
		argument = one_argument(argument, arg2);
		skip_spaces(&argument);
		
		if (!*arg2 || !isdigit(*arg2)) {
			msg_to_char(ch, "Usage: island flags <id> [add | remove] [flags]\r\n");
		}
		else if (!(isle = get_island(atoi(arg2), FALSE))) {
			msg_to_char(ch, "Unknown island id '%s'.\r\n", arg2);
		}
		else {
			old = isle->flags;
			isle->flags = olc_process_flag(ch, argument, "island", "island flags", island_bits, isle->flags);
			
			if (isle->flags != old) {
				sprintbit(isle->flags, island_bits, flags, TRUE);
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has set island %d (%s) flags to: %s", GET_NAME(ch), isle->id, NULLSAFE(isle->name), flags);
				save_island_table();
				
				if (IS_SET(isle->flags, ISLE_NO_CUSTOMIZE) && !IS_SET(old, ISLE_NO_CUSTOMIZE)) {
					decustomize_island(isle->id);
				}
			}
		}
	}
	else {
		msg_to_char(ch, "Usage: island list [keywords]\r\n");
		msg_to_char(ch, "       island rename <id> <name>\r\n");
		msg_to_char(ch, "       island description <id>\r\n");
		msg_to_char(ch, "       island flags <id> [add | remove] [flags]\r\n");
	}
}


ACMD(do_last) {
	char_data *plr = NULL;
	bool file = FALSE;
	char status[10], ago_buf[256], *ago_ptr;

	one_argument(argument, arg);

	if (!*arg) {
		send_to_char("For whom do you wish to search?\r\n", ch);
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else if ((GET_ACCESS_LEVEL(plr) > GET_ACCESS_LEVEL(ch)) && (GET_ACCESS_LEVEL(ch) < LVL_IMPL)) {
		send_to_char("You are not sufficiently godly for that!\r\n", ch);
	}
	else {
		strcpy(status, level_names[(int) GET_ACCESS_LEVEL(plr)][0]);
		// crlf built into ctime
		msg_to_char(ch, "[%5d] [%s] %-12s : %-18s : %-20s", GET_IDNUM(plr), status, GET_PC_NAME(plr), plr->desc ? plr->desc->host : plr->prev_host, file ? ctime(&plr->prev_logon) : ctime(&plr->player.time.logon));
		if (file) {
			ago_ptr = strcpy(ago_buf, simple_time_since(plr->prev_logon));
			skip_spaces(&ago_ptr);
			msg_to_char(ch, "Last online %s ago\r\n", ago_ptr);
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


ACMD(do_load) {
	vehicle_data *veh;
	char_data *mob, *mort;
	obj_data *obj;
	any_vnum number;

	two_arguments(argument, buf, buf2);

	if (!*buf || !*buf2 || !isdigit(*buf2)) {
		send_to_char("Usage: load { obj | mob | vehicle | book } <number>\r\n", ch);
		return;
	}
	if ((number = atoi(buf2)) < 0) {
		send_to_char("A NEGATIVE number??\r\n", ch);
		return;
	}
	if (is_abbrev(buf, "mob")) {
		if (!mob_proto(number)) {
			send_to_char("There is no monster with that number.\r\n", ch);
			return;
		}
		mob = read_mobile(number, TRUE);
		setup_generic_npc(mob, NULL, NOTHING, NOTHING);
		char_to_room(mob, IN_ROOM(ch));

		act("$n makes a quaint, magical gesture with one hand.", TRUE, ch, 0, 0, TO_ROOM | DG_NO_TRIG);
		act("$n has created $N!", FALSE, ch, 0, mob, TO_ROOM | DG_NO_TRIG);
		act("You create $N.", FALSE, ch, 0, mob, TO_CHAR | DG_NO_TRIG);
		load_mtrigger(mob);
		
		if ((mort = find_mortal_in_room(IN_ROOM(ch)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s loaded mob %s with mortal present (%s) at %s", GET_NAME(ch), GET_NAME(mob), GET_NAME(mort), room_log_identifier(IN_ROOM(ch)));
		}
		else if (ROOM_OWNER(IN_ROOM(ch)) && !EMPIRE_IMM_ONLY(ROOM_OWNER(IN_ROOM(ch)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s loaded mob %s in mortal empire (%s) at %s", GET_NAME(ch), GET_NAME(mob), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))), room_log_identifier(IN_ROOM(ch)));
		}
	}
	else if (is_abbrev(buf, "obj")) {
		if (!obj_proto(number)) {
			send_to_char("There is no object with that number.\r\n", ch);
			return;
		}
		obj = read_object(number, TRUE);
		if (CAN_WEAR(obj, ITEM_WEAR_TAKE))
			obj_to_char(obj, ch);
		else
			obj_to_room(obj, IN_ROOM(ch));
		act("$n makes a strange magical gesture.", TRUE, ch, 0, 0, TO_ROOM | DG_NO_TRIG);
		act("$n has created $p!", FALSE, ch, obj, 0, TO_ROOM | DG_NO_TRIG);
		act("You create $p.", FALSE, ch, obj, 0, TO_CHAR | DG_NO_TRIG);
		if (load_otrigger(obj) && obj->carried_by) {
			get_otrigger(obj, obj->carried_by, FALSE);
		}
	}
	else if (is_abbrev(buf, "vehicle")) {
		if (!vehicle_proto(number)) {
			msg_to_char(ch, "There is no vehicle with that number.\r\n");
			return;
		}
		veh = read_vehicle(number, TRUE);
		vehicle_to_room(veh, IN_ROOM(ch));
		scale_vehicle_to_level(veh, 0);	// attempt auto-detect of level
		get_vehicle_interior(veh);	// ensure inside is loaded
		act("$n makes an odd magical gesture.", TRUE, ch, NULL, NULL, TO_ROOM | DG_NO_TRIG);
		act("$n has created $V!", FALSE, ch, NULL, veh, TO_ROOM | DG_NO_TRIG | ACT_VEH_VICT);
		act("You create $V.", FALSE, ch, NULL, veh, TO_CHAR | DG_NO_TRIG | ACT_VEH_VICT);
		
		if (VEH_CLAIMS_WITH_ROOM(veh) && ROOM_OWNER(HOME_ROOM(IN_ROOM(veh)))) {
			perform_claim_vehicle(veh, ROOM_OWNER(HOME_ROOM(IN_ROOM(veh))));
		}
		
		if ((mort = find_mortal_in_room(IN_ROOM(ch)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s loaded vehicle %s with mortal present (%s) at %s", GET_NAME(ch), VEH_SHORT_DESC(veh), GET_NAME(mort), room_log_identifier(IN_ROOM(ch)));
		}
		else if (ROOM_OWNER(IN_ROOM(ch)) && !EMPIRE_IMM_ONLY(ROOM_OWNER(IN_ROOM(ch)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s loaded vehicle %s in mortal empire (%s) at %s", GET_NAME(ch), VEH_SHORT_DESC(veh), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))), room_log_identifier(IN_ROOM(ch)));
		}
		
		load_vtrigger(veh);
	}
	else if (is_abbrev(buf, "book")) {
		if (!book_proto(number)) {
			msg_to_char(ch, "There is no book with that number.\r\n");
			return;
		}
		
		obj = create_book_obj(book_proto(number));
		obj_to_char(obj, ch);
		act("$n makes a strange magical gesture.", TRUE, ch, NULL, NULL, TO_ROOM | DG_NO_TRIG);
		act("$n has created $p!", FALSE, ch, obj, NULL, TO_ROOM | DG_NO_TRIG);
		act("You create $p.", FALSE, ch, obj, NULL, TO_CHAR | DG_NO_TRIG);
		if (load_otrigger(obj) && obj->carried_by) {
			get_otrigger(obj, obj->carried_by, FALSE);
		}
	}
	else {
		send_to_char("That'll have to be either 'obj', 'mob', or 'vehicle'.\r\n", ch);
	}
}


ACMD(do_moveeinv) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	struct empire_unique_storage *unique;
	struct empire_storage_data *store, *next_store, *new_store;
	struct empire_island *eisle;
	int island_from, island_to, count;
	empire_data *emp;
	
	argument = any_one_word(argument, arg1);	// empire
	two_arguments(argument, arg2, arg3);	// island_from, island_to
	
	if (!*arg1 || !*arg2 || !*arg3) {
		msg_to_char(ch, "Usage: moveeinv \"<empire>\" <from island> <to island>\r\n");
	}
	else if (!(emp = get_empire_by_name(arg1))) {
		msg_to_char(ch, "Invalid empire '%s'.\r\n", arg1);
	}
	else if (!is_number(arg2) || (island_from = atoi(arg2)) < -1) {
		msg_to_char(ch, "Invalid from-island '%s'.\r\n", arg2);
	}
	else if (!is_number(arg3) || (island_to = atoi(arg3)) < -1) {
		msg_to_char(ch, "Invalid to-island '%s'.\r\n", arg2);
	}
	else if (island_to == island_from) {
		msg_to_char(ch, "Those are the same island.\r\n");
	}
	else {
		count = 0;
		eisle = get_empire_island(emp, island_from);
		HASH_ITER(hh, eisle->store, store, next_store) {
			if (store->amount > 0) {
				SAFE_ADD(count, store->amount, 0, INT_MAX, FALSE);
				new_store = add_to_empire_storage(emp, island_to, store->vnum, store->amount, 0);
				if (new_store) {
					merge_storage_timers(&new_store->timers, store->timers, new_store->amount);
				}
			}
			else {
				new_store = find_stored_resource(emp, island_to, store->vnum);
			}
			
			// translate keep?
			if (new_store && new_store->keep != UNLIMITED) {
				if (store->keep == UNLIMITED) {
					new_store->keep = UNLIMITED;
				}
				else {
					SAFE_ADD(new_store->keep, store->keep, 0, INT_MAX, FALSE);
				}
			}
			
			HASH_DEL(eisle->store, store);
			free_empire_storage_data(store);
		}
		DL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), unique) {
			if (unique->island == island_from) {
				unique->island = island_to;
				SAFE_ADD(count, unique->amount, 0, INT_MAX, FALSE);
				
				// does not consolidate
			}
		}
		
		if (count != 0) {
			EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has moved %d einv items for %s from island %d to island %d", GET_REAL_NAME(ch), count, EMPIRE_NAME(emp), island_from, island_to);
			msg_to_char(ch, "Moved %d items for %s%s&0 from island %d to island %d.\r\n", count, EMPIRE_BANNER(emp), EMPIRE_NAME(emp), island_from, island_to);
		}
		else {
			msg_to_char(ch, "No items to move.\r\n");
		}
	}
}


ACMD(do_oset) {
	char obj_arg[MAX_INPUT_LENGTH], field_arg[MAX_INPUT_LENGTH], *obj_arg_ptr = obj_arg;
	obj_data *obj;
	int number;
	bool was_lit;
	
	argument = one_argument(argument, obj_arg);
	argument = any_one_arg(argument, field_arg);
	skip_spaces(&argument);	// remainder
	number = get_number(&obj_arg_ptr);
	
	if (!*obj_arg_ptr || !*field_arg) {
		msg_to_char(ch, "Usage: oset <object> <field> <value>\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, obj_arg_ptr, &number, ch->carrying)) && !(obj = get_obj_in_list_vis(ch, obj_arg_ptr, &number, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(obj_arg), obj_arg);
	}
	else if (is_abbrev(field_arg, "flags")) {
		was_lit = LIGHT_IS_LIT(obj);
		
		GET_OBJ_EXTRA(obj) = olc_process_flag(ch, argument, "extra", "oset name flags", extra_bits, GET_OBJ_EXTRA(obj));
							
		// check lights
		if (was_lit != LIGHT_IS_LIT(obj)) {
			if (was_lit) {
				apply_obj_light(obj, FALSE);
			}
			else {
				apply_obj_light(obj, TRUE);
			}
		}
		request_obj_save_in_world(obj);
	}
	else if (is_abbrev(field_arg, "keywords") || is_abbrev(field_arg, "aliases")) {
		if (!*argument) {
			msg_to_char(ch, "Set the keywords to what?\r\n");
		}
		else {
			if (!str_cmp(argument, "none")) {
				set_obj_keywords(obj, NULL);
				msg_to_char(ch, "You restore its original keywords.\r\n");
			}
			else {
				set_obj_keywords(obj, argument);
				msg_to_char(ch, "You change its keywords to '%s'.\r\n", GET_OBJ_KEYWORDS(obj));
			}
		}
	}
	else if (is_abbrev(field_arg, "longdescription")) {
		if (!*argument) {
			msg_to_char(ch, "Set the long description to what?\r\n");
		}
		else {
			if (!str_cmp(argument, "none")) {
				set_obj_long_desc(obj, NULL);
				msg_to_char(ch, "You restore its original long description.\r\n");
			}
			else {
				set_obj_long_desc(obj, argument);
				msg_to_char(ch, "You change its long description to '%s'.\r\n", GET_OBJ_LONG_DESC(obj));
			}
		}
	}
	else if (is_abbrev(field_arg, "shortdescription")) {
		if (!*argument) {
			msg_to_char(ch, "Set the short description to what?\r\n");
		}
		else {
			if (!str_cmp(argument, "none")) {
				set_obj_short_desc(obj, NULL);
				msg_to_char(ch, "You restore its original short description.\r\n");
			}
			else {
				set_obj_short_desc(obj, argument);
				msg_to_char(ch, "You change its short description to '%s'.\r\n", GET_OBJ_SHORT_DESC(obj));
			}
		}
	}
	else if (is_abbrev(field_arg, "timer")) {
		if (!*argument || (!isdigit(*argument) && *argument != '-')) {
			msg_to_char(ch, "Set the timer to what?\r\n");
		}
		else {
			GET_OBJ_TIMER(obj) = atoi(argument);
			schedule_obj_timer_update(obj, TRUE);
			request_obj_save_in_world(obj);
			msg_to_char(ch, "You change its timer to %d.\r\n", GET_OBJ_TIMER(obj));
		}
	}
	else {
		msg_to_char(ch, "Invalid field.\r\n");
	}
}


ACMD(do_peace) {
	struct txt_block *inq, *next_inq;
	char_data *iter, *next_iter;
	trig_data *trig;
	struct pursuit_data *purs, *next_purs;
	
	DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_iter, next_in_room) {
		// stop fighting
		if (FIGHTING(iter) || GET_POS(iter) == POS_FIGHTING) {
			stop_fighting(iter);
		}
		
		// clear input
		if (iter != ch && iter->desc) {
			LL_FOREACH_SAFE(iter->desc->input.head, inq, next_inq) {
				LL_DELETE(iter->desc->input.head, inq);
				free(inq->text);
				free(inq);
			}
		}
		
		// clear DOTs (could restart a fight)
		while (iter->over_time_effects) {
			dot_remove(iter, iter->over_time_effects);
		}
		
		// cancel combat scripts (could keep players in combat)
		if (IS_NPC(iter) && SCRIPT(iter)) {
			LL_FOREACH(TRIGGERS(SCRIPT(iter)), trig) {
				if (GET_TRIG_WAIT(trig) && GET_TRIG_DEPTH(trig) && IS_SET(GET_TRIG_TYPE(trig), MTRIG_FIGHT)) {
					dg_event_cancel(GET_TRIG_WAIT(trig), cancel_wait_event);
					GET_TRIG_WAIT(trig) = NULL;
				}
			}
		}
		
		// cancel pursuit
		if (IS_NPC(iter) && MOB_PURSUIT(iter)) {
			return_to_pursuit_location(iter);
			
			LL_FOREACH_SAFE(MOB_PURSUIT(iter), purs, next_purs) {
				free_pursuit(purs);
			}
			MOB_PURSUIT(iter) = NULL;
		}
	}
	
	if (find_mortal_in_room(IN_ROOM(ch))) {
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s used peace with mortal present at %s", GET_NAME(ch), room_log_identifier(IN_ROOM(ch)));
	}
	
	act("You raise your hands and a feeling of peace sweeps over the room.", FALSE, ch, NULL, NULL, TO_CHAR | DG_NO_TRIG);
	act("$n raises $s hands and a feeling of peace enters your heart.", FALSE, ch, NULL, NULL, TO_ROOM | DG_NO_TRIG);
}


ACMD(do_playerdelete) {
	descriptor_data *desc, *next_desc;
	char name[MAX_INPUT_LENGTH];
	char_data *victim = NULL;
	bool file = FALSE;
	
	argument = any_one_arg(argument, name);
	skip_spaces(&argument);
	
	if (!*argument || !*name) {
		msg_to_char(ch, "Usage: playerdelete <name> CONFIRM\r\n");
	}
	else if (!(victim = find_or_load_player(name, &file))) {
		msg_to_char(ch, "Unknown player '%s'.\r\n", name);
	}
	else if (get_highest_access_level(GET_ACCOUNT(victim)) >= GET_ACCESS_LEVEL(ch) && GET_ACCOUNT(victim) != GET_ACCOUNT(ch)) {
		msg_to_char(ch, "No, no, no, no, no, no, no.\r\n");
	}
	else if (PLR_FLAGGED(victim, PLR_NODELETE)) {
		msg_to_char(ch, "You cannot delete that player because of a !DEL player flag.\r\n");
	}
	else if (strcmp(argument, "CONFIRM")) {
		msg_to_char(ch, "You must type the word CONFIRM, in all caps, after the target name.\r\n");
		msg_to_char(ch, "WARNING: This will permanently delete the character.\r\n");
	}
	else {
		// logs and messaging
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "DEL: %s has deleted player %s", GET_NAME(ch), GET_NAME(victim));
		if (!file) {
			if (!GET_INVIS_LEV(victim)) {
				act("$n has left the game.", TRUE, victim, FALSE, FALSE, TO_ROOM);
			}
			if (GET_INVIS_LEV(victim) == 0) {
				if (config_get_bool("public_logins")) {
					mortlog("%s has left the game", PERS(victim, victim, TRUE));
				}
				else {
					mortlog_friends(victim, "%s has left the game", PERS(victim, victim, TRUE));
					if (GET_LOYALTY(victim)) {
						log_to_empire(GET_LOYALTY(victim), ELOG_LOGINS, "%s has left the game", PERS(victim, victim, TRUE));
					}
				}
			}
			msg_to_char(victim, "Your character has been deleted. Goodbye...\r\n");
		}
		
		// look for this character at a menu, in case
		for (desc = descriptor_list; desc; desc = next_desc) {
			next_desc = desc->next;
			
			if (desc->character && desc->character != victim && GET_IDNUM(desc->character) == GET_IDNUM(victim)) {
				msg_to_desc(desc, "Your character has been deleted. Goodbye...\r\n");
				if (STATE(desc) == CON_PLAYING) {
					STATE(desc) = CON_DISCONNECT;
				}
				else {
					STATE(desc) = CON_CLOSE;
				}
			}
		}
		
		if (file) {
			// offline delete (will be freed at the end)
			delete_player_character(victim);
		}
		else {
			// in-game delete (remove items first, and extract)
			extract_all_items(victim);
			delete_player_character(victim);
			extract_char(victim);
			victim = NULL;	// prevent cleanup
		}
	}
	
	// cleanup
	if (victim && file) {
		free_char(victim);
	}
}


ACMD(do_poofset) {
	char **msg;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that now.\r\n");
		return;
	}

	switch (subcmd) {
		case SCMD_POOFIN:    msg = &(POOFIN(ch));    break;
		case SCMD_POOFOUT:   msg = &(POOFOUT(ch));   break;
		default:    return;
	}

	skip_spaces(&argument);
	delete_doubledollar(argument);
	
	if (!*argument) {
		msg_to_char(ch, "Current %s: %s\r\n", subcmd == SCMD_POOFIN ? "poofin" : "poofout", *msg ? *msg : "none");
		if (*msg) {
			msg_to_char(ch, "Use '%s none' to remove it.\r\n", subcmd == SCMD_POOFIN ? "poofin" : "poofout");
		}
		return;
	}
	
	if (strlen(argument) > MAX_POOFIN_LENGTH) {
		msg_to_char(ch, "You can't set that to anything longer than %d characters.\r\n", MAX_POOF_LENGTH);
		return;
	}

	if (*msg) {
		free(*msg);
	}

	if (!str_cmp(argument, "none")) {
		*msg = NULL;
	}
	else {
		*msg = str_dup(argument);
	}

	send_config_msg(ch, "ok_string");
}


ACMD(do_purge) {
	char_data *vict, *next_v;
	vehicle_data *veh;
	obj_data *obj, *next_o;
	int number;
	char *arg;

	one_argument(argument, buf);
	arg = buf;
	number = get_number(&arg);

	if (*arg) {			/* argument supplied. destroy single object or char */
		if ((vict = get_char_vis(ch, arg, &number, FIND_CHAR_ROOM)) != NULL) {
			if (!REAL_NPC(vict) && (GET_REAL_LEVEL(ch) <= GET_REAL_LEVEL(vict))) {
				send_to_char("Fuuuuuuuuu!\r\n", ch);
				return;
			}
			act("$n disintegrates $N.", FALSE, ch, 0, vict, TO_NOTVICT | DG_NO_TRIG);

			if (!REAL_NPC(vict)) {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has purged %s", GET_NAME(ch), GET_NAME(vict));
				SAVE_CHAR(vict);
				if (vict->desc) {
					STATE(vict->desc) = CON_CLOSE;
					vict->desc->character = NULL;
					vict->desc = NULL;
				}
				
				extract_all_items(vict);	// otherwise it duplicates items
			}
			extract_char(vict);
		}
		else if ((obj = get_obj_in_list_vis(ch, arg, &number, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL) {
			act("$n destroys $p.", FALSE, ch, obj, 0, TO_ROOM | DG_NO_TRIG);
			extract_obj(obj);
		}
		else if ((veh = get_vehicle_in_room_vis(ch, arg, &number))) {
			act("$n destroys $V.", FALSE, ch, NULL, veh, TO_ROOM | DG_NO_TRIG | ACT_VEH_VICT);
			if (IN_ROOM(veh) != IN_ROOM(ch) && ROOM_PEOPLE(IN_ROOM(veh))) {
				act("$V is destroyed!", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM | DG_NO_TRIG | ACT_VEH_VICT);
			}
			extract_vehicle(veh);
		}
		else {
			send_to_char("Nothing here by that name.\r\n", ch);
			return;
		}

		send_config_msg(ch, "ok_string");
	}
	else {			/* no argument. clean out the room */
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has purged room %s", GET_REAL_NAME(ch), room_log_identifier(IN_ROOM(ch)));
		act("$n gestures... You are surrounded by scorching flames!", FALSE, ch, 0, 0, TO_ROOM | DG_NO_TRIG);
		send_to_room("The world seems a little cleaner.\r\n", IN_ROOM(ch));
		
		DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_v, next_in_room) {
			if (REAL_NPC(vict) && !MOB_FLAGGED(vict, MOB_IMPORTANT)) {
				extract_char(vict);
			}
		}

		DL_FOREACH_SAFE2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_o, next_content) {
			if (!OBJ_FLAGGED(obj, OBJ_IMPORTANT)) {
				extract_obj(obj);
			}
		}
	}
}


ACMD(do_random) {
	struct map_data *map;
	int tries;
	room_vnum roll;
	room_data *loc;
	
	// looks for a random non-ocean location
	for (tries = 0; tries < 100; ++tries) {
		roll = number(0, MAP_SIZE - 1);
		map = &world_map[MAP_X_COORD(roll)][MAP_Y_COORD(roll)];
		
		if (GET_SECT_VNUM(map->sector_type) == BASIC_OCEAN) {
			continue;
		}
		
		if ((loc = real_room(roll)) && !ROOM_IS_CLOSED(loc)) {
			perform_goto(ch, loc);
			return;
		}
	}
	
	msg_to_char(ch, "Failed to find a good random spot to send you.\r\n");
}


// also do_shutdown
ACMD(do_reboot) {
	char arg[MAX_INPUT_LENGTH];
	descriptor_data *desc;
	int type, time = 0, var;
	bool no_arg = FALSE;

	// any number of args in any order
	argument = any_one_arg(argument, arg);
	no_arg = !*arg;
	while (*arg) {
		if (is_number(arg)) {
			if ((time = atoi(arg)) <= 0) {
				msg_to_char(ch, "Invalid amount of time!\r\n");
				return;
			}
		}
		else if (!str_cmp(arg, "now")) {
			reboot_control.immediate = TRUE;
		}
		else if ((type = search_block(arg, shutdown_types, TRUE)) != NOTHING) {
			reboot_control.level = type;
		}
		else if (!str_cmp(arg, "cancel")) {
			if (reboot_control.type != subcmd) {
				msg_to_char(ch, "There's no %s scheduled.\r\n", reboot_types[subcmd]);
			}
			else {
				reboot_control.type = REBOOT_NONE;
				reboot_control.time = -1;
				msg_to_char(ch, "You have canceled the %s.\r\n", reboot_types[subcmd]);
			}
			return;
		}
		else {
			msg_to_char(ch, "Unknown reboot option '%s'.\r\n", arg);
			return;
		}
		
		argument = any_one_arg(argument, arg);
	}
	
	// All good to set up the reboot:
	if (!no_arg) {
		reboot_control.time = time + 1;	// minutes
		reboot_control.type = subcmd;
		
		if (subcmd == REBOOT_REBOOT) {
			reboot_control.level = SHUTDOWN_NORMAL;	// prevent a reboot from using a shutdown type
		}
	}

	if (reboot_control.immediate) {
		msg_to_char(ch, "Preparing for imminent %s...\r\n", reboot_types[reboot_control.type]);
	}
	else if (reboot_control.type == REBOOT_NONE || reboot_control.time < 0) {
		msg_to_char(ch, "There is no %s scheduled.\r\n", reboot_types[subcmd]);
	}
	else {
		var = (no_arg ? reboot_control.time : time);
		msg_to_char(ch, "The %s is set for %d minute%s (%s).\r\n", reboot_types[reboot_control.type], var, PLURAL(var), shutdown_types[reboot_control.level]);
		if (no_arg) {
			msg_to_char(ch, "Players blocking the %s:\r\n", reboot_types[reboot_control.type]);
			for (desc = descriptor_list; desc; desc = desc->next) {
				if (STATE(desc) != CON_PLAYING || !desc->character) {
					msg_to_char(ch, " %s at menu\r\n", (desc->character ? GET_NAME(desc->character) : "New connection"));
				}
				else if (!REBOOT_CONF(desc->character)) {
					msg_to_char(ch, " %s not confirmed\r\n", GET_NAME(desc->character));
				}
			}
		}
		else {
			update_reboot();
		}
	}
}


/*
 * Too bad it doesn't check the return values to let the user
 * know about -1 values.  This will result in an 'Okay.' to a
 * 'reload' command even when the string was not replaced.
 * To fix later, if desired. -gg 6/24/99
 *
 * Could also replace the control here with a structure... and it
 * would be nice to have a syslog. -paul 12/8/2014
 */
ACMD(do_reload) {
	int iter;

	one_argument(argument, arg);

	if (!str_cmp(arg, "all") || *arg == '*') {
		load_intro_screens();
		
		for (iter = 0; iter < NUM_TEXT_FILE_STRINGS; ++iter) {
			reload_text_string(iter);
		}
	}
	else if (!str_cmp(arg, "wizlist")) {
		reload_text_string(TEXT_FILE_WIZLIST);
	}
	else if (!str_cmp(arg, "godlist")) {
		reload_text_string(TEXT_FILE_GODLIST);
	}
	else if (!str_cmp(arg, "credits")) {
		reload_text_string(TEXT_FILE_CREDITS);
	}
	else if (!str_cmp(arg, "motd")) {
		reload_text_string(TEXT_FILE_MOTD);
	}
	else if (!str_cmp(arg, "imotd")) {
		reload_text_string(TEXT_FILE_IMOTD);
	}
	else if (!str_cmp(arg, "news")) {
		reload_text_string(TEXT_FILE_NEWS);
	}
	else if (!str_cmp(arg, "help")) {
		reload_text_string(TEXT_FILE_HELP_SCREEN);
		reload_text_string(TEXT_FILE_HELP_SCREEN_SCREENREADER);
	}
	else if (!str_cmp(arg, "info")) {
		reload_text_string(TEXT_FILE_INFO);
	}
	else if (!str_cmp(arg, "policy")) {
		reload_text_string(TEXT_FILE_POLICY);
	}
	else if (!str_cmp(arg, "handbook")) {
		reload_text_string(TEXT_FILE_HANDBOOK);
	}
	else if (!str_cmp(arg, "shortcredits")) {
		reload_text_string(TEXT_FILE_SHORT_CREDITS);
	}
	else if (!str_cmp(arg, "intros")) {
		load_intro_screens();
	}
	else if (!str_cmp(arg, "xhelp")) {
		if (help_table) {
			for (iter = 0; iter <= top_of_helpt; ++iter) {
				if (help_table[iter].keyword) {
					free(help_table[iter].keyword);
				}
				if (help_table[iter].entry && !help_table[iter].duplicate) {
					free(help_table[iter].entry);
				}
			}
			free(help_table);
		}
		top_of_helpt = 0;
		index_boot_help();
	}
	else if (!str_cmp(arg, "fmessages") || !str_cmp(arg, "fightmessages")) {
		load_fight_messages();
	}
	else if (!str_cmp(arg, "data")) {
		load_data_table();
	}
	else {
		send_to_char("Unknown reload option.\r\n", ch);
		return;
	}

	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has reloaded: %s", GET_NAME(ch), arg);
	send_config_msg(ch, "ok_string");
}


ACMD(do_rescale) {
	vehicle_data *veh;
	obj_data *obj, *new;
	char_data *vict;
	int level;

	argument = one_argument(argument, arg);
	skip_spaces(&argument);
	level = atoi(argument);
	
	if (!*argument || !*arg || !isdigit(*argument)) {
		msg_to_char(ch, "Usage: rescale <item> <level>\r\n");
	}
	else if (level < 0) {
		msg_to_char(ch, "Invalid level.\r\n");
	}
	else if (!generic_find(arg, NULL, FIND_CHAR_ROOM | FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_VEHICLE_ROOM, ch, &vict, &obj, &veh)) {
		msg_to_char(ch, "You don't see %s %s here.\r\n", AN(arg), arg);
	}
	else if (vict) {
		// victim mode
		if (!IS_NPC(vict)) {
			msg_to_char(ch, "You can only rescale NPCs.\r\n");
		}
		else {
			scale_mob_to_level(vict, level);
			set_mob_flags(vict, MOB_NO_RESCALE);
			
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has rescaled mob %s to level %d at %s", GET_NAME(ch), PERS(vict, vict, FALSE), GET_CURRENT_SCALE_LEVEL(vict), room_log_identifier(IN_ROOM(vict)));
			
			sprintf(buf, "You rescale $N to level %d.", GET_CURRENT_SCALE_LEVEL(vict));
			act("$n rescales $N.", FALSE, ch, NULL, vict, TO_NOTVICT | DG_NO_TRIG);
			act(buf, FALSE, ch, NULL, vict, TO_CHAR | DG_NO_TRIG);
			if (vict->desc) {
				sprintf(buf, "$n rescales you to level %d.", GET_CURRENT_SCALE_LEVEL(vict));
				act(buf, FALSE, ch, NULL, vict, TO_VICT | DG_NO_TRIG);
			}
		}
	}
	else if (veh) {
		scale_vehicle_to_level(veh, level);
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has rescaled vehicle %s to level %d at %s", GET_NAME(ch), VEH_SHORT_DESC(veh), VEH_SCALE_LEVEL(veh), room_log_identifier(IN_ROOM(ch)));
		sprintf(buf, "You rescale $V to level %d.", VEH_SCALE_LEVEL(veh));
		act(buf, FALSE, ch, NULL, veh, TO_CHAR | DG_NO_TRIG | ACT_VEH_VICT);
		act("$n rescales $V.", FALSE, ch, NULL, veh, TO_ROOM | DG_NO_TRIG | ACT_VEH_VICT);
	}
	else if (obj) {
		// item mode
		if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			scale_item_to_level(obj, level);
		}
		else {
			new = fresh_copy_obj(obj, level, TRUE, TRUE);
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
			obj = new;
		}
		
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has rescaled obj %s to level %d at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), GET_OBJ_CURRENT_SCALE_LEVEL(obj), room_log_identifier(IN_ROOM(ch)));
		sprintf(buf, "You rescale $p to level %d.", GET_OBJ_CURRENT_SCALE_LEVEL(obj));
		act(buf, FALSE, ch, obj, NULL, TO_CHAR | DG_NO_TRIG);
		act("$n rescales $p.", FALSE, ch, obj, NULL, TO_ROOM | DG_NO_TRIG);
	}
	else {
		msg_to_char(ch, "You don't see %s %s here.\r\n", AN(arg), arg);
	}
}


ACMD(do_restore) {
	char name_arg[MAX_INPUT_LENGTH], *type_args, arg[MAX_INPUT_LENGTH], msg[MAX_STRING_LENGTH], types[MAX_STRING_LENGTH];
	ability_data *abil, *next_abil;
	skill_data *skill, *next_skill;
	struct cooldown_data *cool;
	generic_data *gen, *next_gen;
	vehicle_data *veh;
	char_data *vict;
	int i, iter;
	
	// modes
	bool all = FALSE, blood = FALSE, cds = FALSE, dots = FALSE, drunk = FALSE, health = FALSE, hunger = FALSE, mana = FALSE, moves = FALSE, thirst = FALSE, temperature = FALSE;

	type_args = one_argument(argument, name_arg);
	skip_spaces(&type_args);
	
	if (!*name_arg) {
		send_to_char("Whom do you wish to restore?\r\n", ch);
		return;
	}
	if (!generic_find(name_arg, NULL, FIND_CHAR_ROOM | FIND_CHAR_WORLD | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE | FIND_VEHICLE_WORLD, ch, &vict, NULL, &veh)) {
		send_config_msg(ch, "no_person");
		return;
	}
	
	// vehicle mode
	if (veh) {
		// found vehicle target here
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has restored %s at %s", GET_REAL_NAME(ch), VEH_SHORT_DESC(veh), room_log_identifier(IN_ROOM(ch)));
		act("You restore $V!", FALSE, ch, NULL, veh, TO_CHAR | DG_NO_TRIG | ACT_VEH_VICT);

		if (GET_INVIS_LEV(ch) > 1 || PRF_FLAGGED(ch, PRF_WIZHIDE)) {
			act("$V is restored!", FALSE, ch, NULL, veh, TO_ROOM | DG_NO_TRIG | ACT_VEH_VICT);
		}
		else {
			act("$n waves $s hand and restores $V!", FALSE, ch, NULL, veh, TO_ROOM | DG_NO_TRIG | ACT_VEH_VICT);
		}
		
		remove_vehicle_flags(veh, VEH_ON_FIRE);
		if (!VEH_IS_DISMANTLING(veh)) {
			complete_vehicle(veh);
		}
		return;
	}
	
	// parse type args
	if (!*type_args) {
		all = TRUE;
	}
	else {
		all = FALSE;
		while (*type_args) {
			type_args = any_one_arg(type_args, arg);
			skip_spaces(&type_args);
			
			if (is_abbrev(arg, "all") || is_abbrev(arg, "full")) {
				all = TRUE;
			}
			else if (is_abbrev(arg, "blood")) {
				blood = TRUE;
			}
			else if (is_abbrev(arg, "cooldowns")) {
				cds = TRUE;
			}
			else if (is_abbrev(arg, "dots")) {
				dots = TRUE;
			}
			else if (is_abbrev(arg, "drunkenness")) {
				drunk = TRUE;
			}
			else if (is_abbrev(arg, "health") || is_abbrev(arg, "hitpoints")) {
				health = TRUE;
			}
			else if (is_abbrev(arg, "hunger")) {
				hunger = TRUE;
			}
			else if (is_abbrev(arg, "mana")) {
				mana = TRUE;
			}
			else if (is_abbrev(arg, "moves") || is_abbrev(arg, "movement") || is_abbrev(arg, "vitality")) {
				// "vitality" catches "v" on "restore <name> h m v"
				moves = TRUE;
			}
			else if (is_abbrev(arg, "thirsty")) {
				thirst = TRUE;
			}
			else if (is_abbrev(arg, "temperature")) {
				temperature = TRUE;
			}
			else {
				msg_to_char(ch, "Unknown restore type '%s'.\r\n", arg);
				return;
			}
		}
	}
	
	// OK: setup default messages
	*types = '\0';
	if (ch == vict) {
		if (all) {
			strcpy(msg, "You have fully restored yourself!");
		}
		else {
			strcpy(msg, "You have restored your");
		}
	}
	else {
		if (all) {
			strcpy(msg, "You have been fully restored by $N!");
		}
		else {
			strcpy(msg, "$N has restored your");
		}
	}
	
	// OK: do the work
	
	// fill pools
	if (all || health) {
		GET_DEFICIT(vict, HEALTH) = 0;
		set_health(vict, GET_MAX_HEALTH(vict));
		
		if (GET_POS(vict) < POS_SLEEPING) {
			GET_POS(vict) = POS_STANDING;
		}
		
		update_pos(vict);
		sprintf(types + strlen(types), "%s health", *types ? "," : "");
	}
	if (all || mana) {
		GET_DEFICIT(vict, MANA) = 0;
		set_mana(vict, GET_MAX_MANA(vict));
		sprintf(types + strlen(types), "%s mana", *types ? "," : "");
	}
	if (all || moves) {
		GET_DEFICIT(vict, MOVE) = 0;
		set_move(vict, GET_MAX_MOVE(vict));
		sprintf(types + strlen(types), "%s moves", *types ? "," : "");
	}
	if (all || blood) {
		GET_DEFICIT(vict, BLOOD) = 0;
		set_blood(vict, GET_MAX_BLOOD(vict));
		sprintf(types + strlen(types), "%s blood", *types ? "," : "");
	}
	
	// remove DoTs
	if (all || dots) {
		while (vict->over_time_effects) {
			dot_remove(vict, vict->over_time_effects);
		}
		
		sprintf(types + strlen(types), "%s DoTs", *types ? "," : "");
	}
	
	// remove all cooldowns
	if (all || cds) {
		while ((cool = vict->cooldowns)) {
			remove_cooldown(vict, cool);
		}
		
		sprintf(types + strlen(types), "%s cooldowns", *types ? "," : "");
	}
	
	// conditions
	if (all || hunger) {
		if (!IS_NPC(vict) && GET_COND(vict, FULL) != UNLIMITED) {
			GET_COND(vict, FULL) = 0;
		}
		sprintf(types + strlen(types), "%s hunger", *types ? "," : "");
	}
	if (all || thirst) {
		if (!IS_NPC(vict) && GET_COND(vict, THIRST) != UNLIMITED) {
			GET_COND(vict, THIRST) = 0;
		}
		sprintf(types + strlen(types), "%s thirst", *types ? "," : "");
	}
	if (all || drunk) {
		if (!IS_NPC(vict) && GET_COND(vict, DRUNK) != UNLIMITED) {
			GET_COND(vict, DRUNK) = 0;
		}
		sprintf(types + strlen(types), "%s drunkenness", *types ? "," : "");
	}
	
	// temperature
	if (all || temperature) {
		reset_player_temperature(vict);
		sprintf(types + strlen(types), "%s temperature", *types ? "," : "");
	}
	
	// affects that could get a person stuck
	if (all && AFF_FLAGGED(vict, AFF_DEATHSHROUDED | AFF_MUMMIFIED)) {
		affects_from_char_by_aff_flag(vict, AFF_DEATHSHROUDED, TRUE);
		affects_from_char_by_aff_flag(vict, AFF_MUMMIFIED, TRUE);
	}

	if (all && !IS_NPC(vict) && (GET_ACCESS_LEVEL(ch) >= LVL_GOD) && (GET_ACCESS_LEVEL(vict) >= LVL_GOD)) {
		for (i = 0; i < NUM_CONDS; i++)
			GET_COND(vict, i) = UNLIMITED;

		for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
			vict->real_attributes[iter] = att_max(vict);
		}
		
		HASH_ITER(hh, skill_table, skill, next_skill) {
			set_skill(vict, SKILL_VNUM(skill), SKILL_MAX_LEVEL(skill));
		}
		
		update_class_and_abilities(vict);
		
		HASH_ITER(hh, ability_table, abil, next_abil) {
			// add abilities to set 0
			add_ability_by_set(vict, abil, 0, TRUE);
		}
		
		affect_total(vict);
		
		// languages
		HASH_ITER(hh, generic_table, gen, next_gen) {
			if (GEN_TYPE(gen) != GENERIC_LANGUAGE || GEN_FLAGGED(gen, GEN_IN_DEVELOPMENT)) {
				continue;
			}
			
			add_language(vict, GEN_VNUM(gen), LANG_SPEAK);
		}
	}
	
	if (ch != vict) {
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s has restored %s:%s", GET_REAL_NAME(ch), GET_REAL_NAME(vict), all ? " full restore" : types);
	}
	
	send_config_msg(ch, "ok_string");
	
	if (!all) {
		sprintf(msg + strlen(msg), "%s!", types);
	}
	act(msg, FALSE, vict, NULL, ch, TO_CHAR | DG_NO_TRIG);
	
	// show 3rd-party message in some cases
	if (all || health || moves || mana || blood || dots) {
		act("$n is restored!", TRUE, vict, NULL, NULL, TO_ROOM | DG_NO_TRIG);
	}
}


ACMD(do_return) {
	char_data *orig;
	
	if (ch->desc && (orig = ch->desc->original)) {
		syslog(SYS_GC, GET_INVIS_LEV(ch->desc->original), TRUE, "GC: %s has returned to %s original body", GET_REAL_NAME(ch->desc->original), REAL_HSHR(ch->desc->original));
		send_to_char("You return to your original body.\r\n", ch);

		/*
		 * If someone switched into your original body, disconnect them.
		 *   - JE 2/22/95
		 *
		 * Zmey: here we put someone switched in our body to disconnect state
		 * but we must also NULL his pointer to our character, otherwise
		 * close_socket() will damage our character's pointer to our descriptor
		 * (which is assigned below in this function). 12/17/99
		 */

		if (orig->desc) {
			orig->desc->character = NULL;
			STATE(orig->desc) = CON_DISCONNECT;
		}

		/* Now our descriptor points to our original body. */
		ch->desc->character = orig;
		ch->desc->original = NULL;

		/* And our body's pointer to descriptor now points to our descriptor. */
		orig->desc = ch->desc;
		ch->desc = NULL;
		
		send_initial_MSDP(orig->desc);
	}
}


ACMD(do_send) {
	char_data *vict;

	half_chop(argument, arg, buf);

	if (!*arg) {
		send_to_char("Send what to whom?\r\n", ch);
		return;
	}
	if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD))) {
		send_config_msg(ch, "no_person");
		return;
	}
	send_to_char(buf, vict);
	send_to_char("\r\n", vict);
	if (PRF_FLAGGED(ch, PRF_NOREPEAT))
		send_to_char("Sent.\r\n", ch);
	else {
		sprintf(buf2, "You send '%s' to %s.\r\n", buf, GET_NAME(vict));
		send_to_char(buf2, ch);
	}
}


ACMD(do_set) {
	char field[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	char temp[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	bool is_file = FALSE, load_from_file = FALSE;
	int mode, len, retval;
	char_data *vict = NULL;
	char is_player = 0;

	half_chop(argument, name, buf);

	if (!str_cmp(name, "file")) {
		is_file = 1;
		half_chop(buf, name, temp);
		strcpy(buf, temp);
	}
	else if (!str_cmp(name, "player")) {
		is_player = 1;
		half_chop(buf, name, temp);
		strcpy(buf, temp);
	}
	else if (!str_cmp(name, "mob")) {
		half_chop(buf, name, temp);
		strcpy(buf, temp);
	}

	half_chop(buf, field, val_arg);

	if (!*name || !*field) {
		send_to_char("Usage: set <victim> <field> <value>\r\n", ch);
		return;
	}

	/* find the target */
	if (!is_file) {
		if (is_player) {
			if (!(vict = get_player_vis(ch, name, FIND_CHAR_WORLD))) {
				send_to_char("There is no such player.\r\n", ch);
				return;
			}
		}
		else { /* is_mob */
			if (!(vict = get_char_vis(ch, name, NULL, FIND_CHAR_WORLD))) {
				send_to_char("There is no such creature.\r\n", ch);
				return;
			}
		}
		if (!IS_NPC(vict) && ch != vict && GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch)) {
			send_to_char("Sorry, you can't do that.\r\n", ch);
			return;
		}
	}
	else if (is_file) {
		if (!(vict = find_or_load_player(name, &load_from_file))) {
			send_to_char("There is no such player.\r\n", ch);
			return;
		}
		if (GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch)) {
			send_to_char("Sorry, you can't do that.\r\n", ch);
			if (load_from_file) {
				free_char(vict);
			}
			return;
		}
		
		// some sets need the delay file
		check_delayed_load(vict);
	}

	/* find the command in the list */
	len = strlen(field);
	for (mode = 0; *(set_fields[mode].cmd) != '\n'; mode++)
		if (!strn_cmp(field, set_fields[mode].cmd, len))
			break;

	/* perform the set */
	retval = perform_set(ch, vict, mode, val_arg);

	/* save the character if a change was made */
	if (retval) {
		if (load_from_file) {
			store_loaded_char(vict);
			send_to_char("Saved in file.\r\n", ch);
		}
		else if (!IS_NPC(vict)) {
			queue_delayed_update(vict, CDU_SAVE);
		}
	}
	else if (load_from_file) {
		free_char(vict);
	}
}


ACMD(do_slay) {
	char_data *vict;

	argument = one_argument(argument, arg);
	skip_spaces(&argument);

	if (!*arg)
		send_to_char("Slay whom?\r\n", ch);
	else {
		if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
			send_to_char("They aren't here.\r\n", ch);
		}
		else if (ch == vict && str_cmp(argument, "confirm")) {
			msg_to_char(ch, "You can't slay yourself (unless you type 'confirm' at the end).\r\n");
		}
		else if (!IS_NPC(vict) && vict != ch && GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch)) {
			act("Surely you don't expect $N to let you slay $M, do you?", FALSE, ch, NULL, vict, TO_CHAR | DG_NO_TRIG);
		}
		else {
			if (ch == vict) {
				syslog(SYS_GC | SYS_DEATH, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has slain %sself at %s", GET_REAL_NAME(ch), HMHR(ch), room_log_identifier(IN_ROOM(vict)));
				log_to_slash_channel_by_name(DEATH_LOG_CHANNEL, NULL, "%s has been slain at (%d, %d)", PERS(vict, vict, TRUE), X_COORD(IN_ROOM(vict)), Y_COORD(IN_ROOM(vict)));
				act("You slay yourself!", FALSE, ch, NULL, NULL, TO_CHAR | DG_NO_TRIG);
				act("$n slays $mself!", FALSE, ch, NULL, NULL, TO_ROOM | DG_NO_TRIG);
			}
			else {
				if (!IS_NPC(vict)) {
					syslog(SYS_GC | SYS_DEATH, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has slain %s at %s", GET_REAL_NAME(ch), GET_REAL_NAME(vict), room_log_identifier(IN_ROOM(vict)));
					log_to_slash_channel_by_name(DEATH_LOG_CHANNEL, NULL, "%s has been slain at (%d, %d)", PERS(vict, vict, TRUE), X_COORD(IN_ROOM(vict)), Y_COORD(IN_ROOM(vict)));
				}
				
				act("You chop $M to pieces! Ah! The blood!", FALSE, ch, NULL, vict, TO_CHAR | DG_NO_TRIG);
				act("$N chops you to pieces!", FALSE, vict, NULL, ch, TO_CHAR | DG_NO_TRIG);
				act("$n brutally slays $N!", FALSE, ch, NULL, vict, TO_NOTVICT | DG_NO_TRIG);
				
				check_scaling(vict, ch);	// ensure scaling
				tag_mob(vict, ch);	// ensures loot binding if applicable
			}

			// this would prevent the death
			if (AFF_FLAGGED(vict, AFF_AUTO_RESURRECT)) {
				// remove ALL (unlike normal auto-rez)
				affects_from_char_by_aff_flag(vict, AFF_AUTO_RESURRECT, FALSE);
				REMOVE_BIT(AFF_FLAGS(vict), AFF_AUTO_RESURRECT);
			}

			die(vict, ch);
		}
	}
}


ACMD(do_snoop) {
	char_data *victim, *tch;

	if (!ch->desc)
		return;

	one_argument(argument, arg);

	if (!*arg)
		stop_snooping(ch);
	else if (!(victim = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD)))
		send_to_char("No such person around.\r\n", ch);
	else if (!victim->desc)
		send_to_char("There's no link... nothing to snoop.\r\n", ch);
	else if (victim == ch)
		stop_snooping(ch);
	else if (victim->desc->snoop_by)
		send_to_char("Busy already. \r\n", ch);
	else if (victim->desc->snooping == ch->desc)
		send_to_char("Don't be stupid.\r\n", ch);
	else {
		if (victim->desc->original)
			tch = victim->desc->original;
		else
			tch = victim;

		if (GET_ACCESS_LEVEL(tch) >= GET_ACCESS_LEVEL(ch)) {
			send_to_char("You can't.\r\n", ch);
			return;
		}
		
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "GC: %s is now snooping %s", GET_REAL_NAME(ch), GET_REAL_NAME(victim));
		send_config_msg(ch, "ok_string");

		if (ch->desc->snooping)
			ch->desc->snooping->snoop_by = NULL;

		ch->desc->snooping = victim->desc;
		victim->desc->snoop_by = ch->desc;
	}
}


ACMD(do_stat) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], some_arg[MAX_INPUT_LENGTH], last_arg[MAX_INPUT_LENGTH];
	struct instance_data *inst;
	char_data *victim = NULL;
	vehicle_data *veh;
	empire_data *emp;
	crop_data *cp;
	obj_data *obj;
	bool details = FALSE, file = FALSE;
	int tmp;

	// check for optional -d arg
	chop_last_arg(argument, some_arg, last_arg);
	if (*last_arg && is_abbrev(last_arg, "-details")) {
		details = TRUE;
		half_chop(some_arg, arg1, arg2);
	}
	else {
		half_chop(argument, arg1, arg2);
	}

	if (!*arg1) {
		send_to_char("Stats on who or what?\r\n", ch);
		return;
	}
	else if (is_abbrev(arg1, "room")) {
		do_stat_room(ch);
	}
	else if (!strn_cmp(arg1, "adventure", 3) && is_abbrev(arg1, "adventure")) {
		if ((inst = find_instance_by_room(IN_ROOM(ch), TRUE, TRUE))) {
			do_stat_adventure(ch, INST_ADVENTURE(inst));
		}
		else {
			msg_to_char(ch, "You are not in an adventure zone.\r\n");
		}
	}
	else if (!strn_cmp(arg1, "template", 4) && is_abbrev(arg1, "template")) {
		if (GET_ROOM_TEMPLATE(IN_ROOM(ch))) {
			do_stat_room_template(ch, GET_ROOM_TEMPLATE(IN_ROOM(ch)), details);
		}
		else {
			msg_to_char(ch, "This is not a templated room.\r\n");
		}
	}
	else if (!str_cmp(arg1, "building")) {
		if (GET_BUILDING(IN_ROOM(ch))) {
			do_stat_building(ch, GET_BUILDING(IN_ROOM(ch)), details);
		}
		else {
			msg_to_char(ch, "You are not in a building.\r\n");
		}
	}
	else if (!str_cmp(arg1, "crop")) {
		if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_HAS_CROP_DATA) && (cp = ROOM_CROP(IN_ROOM(ch)))) {
			do_stat_crop(ch, cp, details);
		}
		else {
			msg_to_char(ch, "You are not on a crop tile.\r\n");
		}
	}
	else if (!strn_cmp(arg1, "emp", 3) && is_abbrev(arg1, "empire")) {
		if ((emp = get_empire_by_name(arg2))) {
			do_stat_empire(ch, emp);
		}
		else if (!*arg2) {
			msg_to_char(ch, "Get stats on which empire?\r\n");
		}
		else {
			msg_to_char(ch, "Unknown empire.\r\n");
		}
	}
	else if (!strn_cmp(arg1, "sect", 4) && is_abbrev(arg1, "sector")) {
		do_stat_sector(ch, SECT(IN_ROOM(ch)), details);
	}
	else if (is_abbrev(arg1, "mob")) {
		if (!*arg2)
			send_to_char("Stats on which mobile?\r\n", ch);
		else {
			if ((victim = get_char_vis(ch, arg2, NULL, FIND_CHAR_WORLD | FIND_NPC_ONLY)) != NULL)
				do_stat_character(ch, victim, details);
			else
				send_to_char("No such mobile around.\r\n", ch);
		}
	}
	else if (is_abbrev(arg1, "vehicle")) {
		if (!*arg2)
			send_to_char("Stats on which vehicle?\r\n", ch);
		else {
			if ((veh = get_vehicle_vis(ch, arg2, NULL)) != NULL) {
				do_stat_vehicle(ch, veh, details);
			}
			else {
				send_to_char("No such vehicle around.\r\n", ch);
			}
		}
	}
	else if (is_abbrev(arg1, "player")) {
		if (!*arg2) {
			send_to_char("Stats on which player?\r\n", ch);
		}
		else {
			if ((victim = get_player_vis(ch, arg2, FIND_CHAR_WORLD)) != NULL)
				do_stat_character(ch, victim, details);
			else
				send_to_char("No such player around.\r\n", ch);
		}
	}
	else if (!str_cmp(arg1, "file")) {
		if (!*arg2) {
			send_to_char("Stats on which player?\r\n", ch);
		}
		else if (!(victim = find_or_load_player(arg2, &file))) {
			send_to_char("There is no such player.\r\n", ch);
		}
		else if (GET_ACCESS_LEVEL(victim) > GET_ACCESS_LEVEL(ch)) {
			send_to_char("Sorry, you can't do that.\r\n", ch);
		}
		else {
			do_stat_character(ch, victim, details);
		}
		
		if (victim && file) {
			free_char(victim);
		}
	}
	else if (is_abbrev(arg1, "object")) {
		if (!*arg2)
			send_to_char("Stats on which object?\r\n", ch);
		else {
			if ((obj = get_obj_vis(ch, arg2, NULL)) != NULL)
				do_stat_object(ch, obj, details);
			else
				send_to_char("No such object around.\r\n", ch);
		}
	}
	else {
		int number;
		char *arg;
		
		arg = arg1;
		number = get_number(&arg);
		
		if ((obj = get_obj_in_equip_vis(ch, arg, &number, ch->equipment, &tmp)) != NULL) {
			do_stat_object(ch, obj, details);
		}
		else if ((obj = get_obj_in_list_vis(ch, arg, &number, ch->carrying)) != NULL) {
			do_stat_object(ch, obj, details);
		}
		else if ((victim = get_char_vis(ch, arg, &number, FIND_CHAR_ROOM)) != NULL) {
			do_stat_character(ch, victim, details);
		}
		else if ((veh = get_vehicle_in_room_vis(ch, arg, &number))) {
			do_stat_vehicle(ch, veh, details);
		}
		else if ((obj = get_obj_in_list_vis(ch, arg, &number, ROOM_CONTENTS(IN_ROOM(ch)))) != NULL) {
			do_stat_object(ch, obj, details);
		}
		else if ((victim = get_char_vis(ch, arg, &number, FIND_CHAR_WORLD)) != NULL) {
			do_stat_character(ch, victim, details);
		}
		else if ((veh = get_vehicle_vis(ch, arg, &number))) {
			do_stat_vehicle(ch, veh, details);
		}
		else if ((obj = get_obj_vis(ch, arg, &number)) != NULL) {
			do_stat_object(ch, obj, details);
		}
		else {
			send_to_char("Nothing around by that name.\r\n", ch);
		}
	}
}


ACMD(do_switch) {
	char_data *victim;

	one_argument(argument, arg);
	
	if (!ch->desc) {
		msg_to_char(ch, "You can't do that now.\r\n");
	}
	else if (ch->desc->original)
		send_to_char("You're already switched.\r\n", ch);
	else if (!*arg)
		send_to_char("Switch with whom?\r\n", ch);
	else if (!(victim = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD)))
		send_to_char("No such character.\r\n", ch);
	else if (ch == victim)
		send_to_char("Hee hee... we are jolly funny today, eh?\r\n", ch);
	else if (victim->desc)
		send_to_char("You can't do that, the body is already in use!\r\n", ch);
	else if ((GET_ACCESS_LEVEL(ch) < LVL_IMPL) && !IS_NPC(victim))
		send_to_char("You aren't holy enough to use a mortal's body.\r\n", ch);
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has switched into %s at %s", GET_REAL_NAME(ch), GET_REAL_NAME(victim), room_log_identifier(IN_ROOM(victim)));
		send_config_msg(ch, "ok_string");

		ch->desc->character = victim;
		ch->desc->original = ch;

		victim->desc = ch->desc;
		ch->desc = NULL;
		
		send_initial_MSDP(victim->desc);
	}
}


ACMD(do_syslog) {
	int iter;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't change your syslog settings right now.\r\n");
	}
	else if (!str_cmp(argument, "all")) {
		// turn on all syslogs
		for (iter = 0; *syslog_types[iter] != '\n'; ++iter) {
			SYSLOG_FLAGS(ch) |= BIT(iter);
		}
		msg_to_char(ch, "Your syslogs are now all on.\r\n");
	}
	else {
		SYSLOG_FLAGS(ch) = olc_process_flag(ch, argument, "syslog", "syslog", syslog_types, SYSLOG_FLAGS(ch));
	}
}


ACMD(do_tedit) {
	// see configs.c for text_file_data[]
	int count, iter, type;
	char field[MAX_INPUT_LENGTH];

	if (ch->desc == NULL) {
		send_to_char("Get outta here you linkdead head!\r\n", ch);
		return;
	}
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing something else. Type ,/h for help.\r\n");
		return;
	}

	half_chop(argument, field, buf);

	if (!*field) {
		build_page_display(ch, "Files available to be edited:");
		count = 1;
		for (iter = 0; iter < NUM_TEXT_FILE_STRINGS; ++iter) {
			if (text_file_data[iter].can_edit && GET_ACCESS_LEVEL(ch) >= text_file_data[iter].level) {
				++count;
				build_page_display_col_str(ch, 4, FALSE, text_file_data[iter].name);
			}
		}
		
		if (count == 0) {
			build_page_display(ch, " None.");
		}

		send_page_display(ch);
		return;
	}
	for (type = 0; type < NUM_TEXT_FILE_STRINGS; type++) {
		if (text_file_data[type].can_edit && !strn_cmp(field, text_file_data[type].name, strlen(field))) {
			break;
		}
	}

	if (type >= NUM_TEXT_FILE_STRINGS) {
		send_to_char("Invalid text editor option.\r\n", ch);
		return;
	}

	if (GET_ACCESS_LEVEL(ch) < text_file_data[type].level) {
		send_to_char("You are not godly enough for that!\r\n", ch);
		return;
	}

	/* set up editor stats */
	start_string_editor(ch->desc, "file", &text_file_strings[type], text_file_data[type].size, FALSE);
	ch->desc->file_storage = str_dup(text_file_data[type].filename);
	act("$n begins editing a file.", TRUE, ch, NULL, NULL, TO_ROOM | DG_NO_TRIG);
}


// do_transfer <- search alias because why is it called do_trans? -pc
ACMD(do_trans) {
	struct shipping_data *shipd, *next_shipd;
	descriptor_data *i;
	char_data *victim;
	room_data *to_room = IN_ROOM(ch);
	vehicle_data *veh;

	argument = one_argument(argument, buf);
	one_word(argument, arg);

	if (*arg) {
		if (!(to_room = parse_room_from_coords(argument)) && !(to_room = find_target_room(ch, arg))) {
			// sent own error message
			return;
		}
	}

	if (!*buf) {
		send_to_char("Whom do you wish to transfer (and where)?\r\n", ch);
	}
	else if (!str_cmp(buf, "all")) {			/* Trans All */
		if (GET_ACCESS_LEVEL(ch) < LVL_IMPL) {
			send_to_char("I think not.\r\n", ch);
			return;
		}

		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has transferred all players to %s", GET_REAL_NAME(ch), room_log_identifier(to_room));

		for (i = descriptor_list; i; i = i->next) {
			if (STATE(i) == CON_PLAYING && i->character && i->character != ch) {
				victim = i->character;
				if (GET_ACCESS_LEVEL(victim) >= GET_ACCESS_LEVEL(ch))
					continue;
				act("$n disappears in a mushroom cloud.", FALSE, victim, 0, 0, TO_ROOM | DG_NO_TRIG);
				char_from_room(victim);
				char_to_room(victim, to_room);
				GET_LAST_DIR(victim) = NO_DIR;
				act("$n arrives from a puff of smoke.", FALSE, victim, 0, 0, TO_ROOM | DG_NO_TRIG);
				act("$n has transferred you!", FALSE, ch, 0, victim, TO_VICT | DG_NO_TRIG);
				qt_visit_room(victim, IN_ROOM(victim));
				look_at_room(victim);
				enter_triggers(victim, NO_DIR, "transfer", FALSE);
				greet_triggers(victim, NO_DIR, "transfer", FALSE);
				RESET_LAST_MESSAGED_TEMPERATURE(victim);
				msdp_update_room(victim);	// once we're sure we're staying
			}
		}
		
		send_config_msg(ch, "ok_string");
	}
	else if ((victim = get_char_vis(ch, buf, NULL, FIND_CHAR_WORLD))) {
		if (victim == ch)
			send_to_char("That doesn't make much sense, does it?\r\n", ch);
		else {
			if ((GET_REAL_LEVEL(ch) < GET_REAL_LEVEL(victim)) && !REAL_NPC(victim)) {
				send_to_char("Go transfer someone your own size.\r\n", ch);
				return;
			}
			
			if (!IS_NPC(victim)) {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has transferred %s to %s", GET_REAL_NAME(ch), GET_REAL_NAME(victim), room_log_identifier(to_room));
			}

			act("$n disappears in a mushroom cloud.", FALSE, victim, 0, 0, TO_ROOM | DG_NO_TRIG);
			char_from_room(victim);
			char_to_room(victim, to_room);
			GET_LAST_DIR(victim) = NO_DIR;
			act("$n arrives from a puff of smoke.", FALSE, victim, 0, 0, TO_ROOM | DG_NO_TRIG);
			act("$n has transferred you!", FALSE, ch, 0, victim, TO_VICT | DG_NO_TRIG);
			qt_visit_room(victim, IN_ROOM(victim));
			look_at_room(victim);
			enter_triggers(victim, NO_DIR, "transfer", FALSE);
			greet_triggers(victim, NO_DIR, "transfer", FALSE);
			RESET_LAST_MESSAGED_TEMPERATURE(victim);
			msdp_update_room(victim);	// once we're sure we're staying
			send_config_msg(ch, "ok_string");
		}
	}
	else if ((veh = get_vehicle_vis(ch, buf, NULL))) {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has transferred %s to %s", GET_REAL_NAME(ch), VEH_SHORT_DESC(veh), room_log_identifier(to_room));
	
		// finish the shipment before transferring
		if (VEH_OWNER(veh)) {
			DL_FOREACH_SAFE(EMPIRE_SHIPPING_LIST(VEH_OWNER(veh)), shipd, next_shipd) {
				if (shipd->shipping_id == VEH_IDNUM(veh)) {
					deliver_shipment(VEH_OWNER(veh), shipd);
				}
			}
		}
		
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			act("$V disappears in a mushroom cloud.", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM | DG_NO_TRIG | ACT_VEH_VICT);
		}
		
		vehicle_from_room(veh);
		vehicle_to_room(veh, to_room);
		
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			act("$V arrives from a puff of smoke.", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM | DG_NO_TRIG | ACT_VEH_VICT);
		}
		send_config_msg(ch, "ok_string");
	}
	else {
		send_config_msg(ch, "no_person");
	}
}


ACMD(do_unbind) {
	obj_data *obj;
	
	one_argument(argument, arg);

	if (!*arg) {
		msg_to_char(ch, "Unbind which object?\r\n");
	}
	else if (!generic_find(arg, NULL, FIND_OBJ_INV | FIND_OBJ_ROOM, ch, NULL, &obj, NULL)) {
		msg_to_char(ch, "Unable to find '%s'.\r\n", argument);
	}
	else if (!OBJ_BOUND_TO(obj)) {
		act("$p isn't bound to anybody.", FALSE, ch, obj, NULL, TO_CHAR | DG_NO_TRIG);
	}
	else {
		free_obj_binding(&OBJ_BOUND_TO(obj));
		request_obj_save_in_world(obj);
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s used unbind on %s", GET_REAL_NAME(ch), GET_OBJ_SHORT_DESC(obj));
		act("You unbind $p.", FALSE, ch, obj, NULL, TO_CHAR | DG_NO_TRIG);
	}
}


ACMD(do_unprogress) {
	char emp_arg[MAX_INPUT_LENGTH], prg_arg[MAX_INPUT_LENGTH];
	empire_data *emp, *next_emp, *only = NULL;
	struct empire_goal *goal;
	progress_data *prg;
	int count;
	bool all;
	
	argument = any_one_word(argument, emp_arg);
	argument = any_one_arg(argument, prg_arg);
	all = !str_cmp(emp_arg, "all");
	
	if (!*emp_arg && !*prg_arg) {
		msg_to_char(ch, "Usage: unprogress <empire | all> <goal vnum>\r\n");
	}
	else if (!all && !(only = get_empire_by_name(emp_arg))) {
		msg_to_char(ch, "Invalid empire '%s'.\r\n", emp_arg);
	}
	else if (!isdigit(*prg_arg) || !(prg = real_progress(atoi(prg_arg)))) {
		msg_to_char(ch, "Invalid progression vnum '%s'.\r\n", prg_arg);
	}
	else {
		count = 0;
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if (!all && emp != only) {
				continue;	// only doing one?
			}
			
			// found current goal?
			if ((goal = get_current_goal(emp, PRG_VNUM(prg)))) {
				++count;
				cancel_empire_goal(emp, goal);
			}
			
			// found completed goal?
			if (empire_has_completed_goal(emp, PRG_VNUM(prg))) {
				remove_completed_goal(emp, PRG_VNUM(prg));
				++count;
			}
			
			refresh_empire_goals(emp, NOTHING);
		}
		
		if (all) {
			if (count < 1) {
				msg_to_char(ch, "No empires have that goal.\r\n");
			}
			else {
				msg_to_char(ch, "Goal removed from %d empire%s.\r\n", count, PLURAL(count));
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has removed progress goal %d from %d empire%s", GET_REAL_NAME(ch), PRG_VNUM(prg), count, PLURAL(count));
			}
		}
		else if (count < 1) {
			msg_to_char(ch, "%s does not have that goal.\r\n", only ? EMPIRE_NAME(only) : "UNKNOWN");
		}
		else {
			msg_to_char(ch, "Goal removed from %s.\r\n", only ? EMPIRE_NAME(only) : "UNKNOWN");
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has removed progress goal %d from %s", GET_REAL_NAME(ch), PRG_VNUM(prg), only ? EMPIRE_NAME(only) : "UNKNOWN");
		}
	}
}


ACMD(do_unquest) {
	struct player_completed_quest *pcq, *next_pcq;
	struct player_quest *pq;
	char arg[MAX_INPUT_LENGTH];
	quest_data *quest;
	char_data *vict;
	bool found;
	
	argument = one_argument(argument, arg);
	skip_spaces(&argument);	// vnum
	
	if (!*arg || !*argument || !isdigit(*argument)) {
		msg_to_char(ch, "Usage: unquest <target> <quest vnum>\r\n");
	}
	else if (!(vict = get_player_vis(ch, arg, FIND_CHAR_WORLD)) || IS_NPC(vict)) {
		send_config_msg(ch, "no_person");
	}
	else if (GET_ACCESS_LEVEL(vict) > GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "You simply can't do that.\r\n");
	}
	else if (!(quest = quest_proto(atoi(argument)))) {
		msg_to_char(ch, "Invalid quest vnum.\r\n");
	}
	else {
		found = FALSE;
		
		// remove from active quests
		LL_FOREACH_SAFE(GET_QUESTS(vict), pq, global_next_player_quest) {
			if (pq->vnum == QUEST_VNUM(quest)) {
				drop_quest(vict, pq);
				found = TRUE;
			}
		}
		global_next_player_quest = NULL;
		
		// remove from completed quests
		HASH_ITER(hh, GET_COMPLETED_QUESTS(vict), pcq, next_pcq) {
			if (pcq->vnum == QUEST_VNUM(quest)) {
				HASH_DEL(GET_COMPLETED_QUESTS(vict), pcq);
				free(pcq);
				found = TRUE;
			}
		}
		
		if (ch == vict) {
			if (found) {
				// no need to syslog for self
				msg_to_char(ch, "You remove [%d] %s from your quest lists.\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
			}
			else {
				msg_to_char(ch, "You are not on that quest.\r\n");
			}
		}
		else {	// ch != vict
			if (found) {
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has removed [%d] %s from %s's quest lists", GET_NAME(ch), QUEST_VNUM(quest), QUEST_NAME(quest), GET_NAME(vict));
				msg_to_char(ch, "You remove [%d] %s from %s's quest lists.\r\n", QUEST_VNUM(quest), QUEST_NAME(quest), PERS(vict, ch, TRUE));
			}
			else {
				msg_to_char(ch, "%s is not on that quest.\r\n", PERS(vict, ch, TRUE));
			}
		}
	}
}


ACMD(do_users) {
	char mode;
	char name_search[MAX_INPUT_LENGTH], host_search[MAX_INPUT_LENGTH];
	char_data *tch;
	descriptor_data *d;
	int low = 0, high = LVL_IMPL, num_can_see = 0;
	int rp = 0, playing = 0, deadweight = 0;
	bool result;

	host_search[0] = name_search[0] = '\0';

	strcpy(buf, argument);
	while (*buf) {
		half_chop(buf, arg, buf1);
		if (*arg == '-') {
			mode = *(arg + 1);  /* just in case; we destroy arg in the switch */
			switch (mode) {
				case 'r':
					rp = 1;
					playing = 1;
					strcpy(buf, buf1);
					break;
				case 'p':
					playing = 1;
					strcpy(buf, buf1);
					break;
				case 'd':
					deadweight = 1;
					strcpy(buf, buf1);
					break;
				case 'l':
					playing = 1;
					half_chop(buf1, arg, buf);
					sscanf(arg, "%d-%d", &low, &high);
					break;
				case 'n':
					playing = 1;
					half_chop(buf1, name_search, buf);
					break;
				case 'h':
					playing = 1;
					half_chop(buf1, host_search, buf);
					break;
				default:
					send_to_char("format: users [-l minlevel[-maxlevel]] [-n name] [-h host] [-r] [-p]\r\n", ch);
					return;
			}				/* end of switch */

		}
		else {			/* endif */
			send_to_char("format: users [-l minlevel[-maxlevel]] [-n name] [-h host] [-r] [-p]\r\n", ch);
			return;
		}
	}				/* end while (parser) */
	msg_to_char(ch, "Num L Name          State           Idl Login@   Site\r\n");
	msg_to_char(ch, "--- - ------------- --------------- --- -------- ------------------------\r\n");

	one_argument(argument, arg);

	if (!*host_search) {
		DL_FOREACH2(player_character_list, tch, next_plr) {
			if (tch->desc) {
				continue;
			}
			result = users_output(ch, tch, NULL, name_search, low, high, rp);
			
			if (result) {
				num_can_see++;
			}
		}
	}

	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) != CON_PLAYING && playing)
			continue;
		if (STATE(d) == CON_PLAYING && deadweight)
			continue;
		if (*host_search && !strstr(d->host, host_search))
			continue;
		
		result =users_output(ch, NULL, d, name_search, low, high, rp);
		
		if (result) {
			++num_can_see;
		}
	}

	msg_to_char(ch, "\r\n%d visible sockets connected.\r\n", num_can_see);
}


ACMD(do_vnum) {
	char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
	
	half_chop(argument, buf, buf2);

	if (!*buf || !*buf2) {
		send_to_char("Usage: vnum <type> <name>\r\n", ch);
	}
	else if (is_abbrev(buf, "mobile")) {
		if (!vnum_mobile(buf2, ch)) {
			send_to_char("No mobiles by that name.\r\n", ch);
		}
	}
	else if (is_abbrev(buf, "object")) {
		if (!vnum_object(buf2, ch)) {
			send_to_char("No objects by that name.\r\n", ch);
		}
	}
	else if (is_abbrev(buf, "craft")) {
		if (!vnum_craft(buf2, ch)) {
			msg_to_char(ch, "No crafts by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "adventure")) {	// name precedence for "vnum a"
		if (!vnum_adventure(buf2, ch)) {
			msg_to_char(ch, "No adventures by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "ability")) {
		extern int vnum_ability(char *searchname, char_data *ch);
		if (!vnum_ability(buf2, ch)) {
			msg_to_char(ch, "No abilities by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "archetype")) {
		extern int vnum_archetype(char *searchname, char_data *ch);
		if (!vnum_archetype(buf2, ch)) {
			msg_to_char(ch, "No archetypes by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "attack") || is_abbrev(buf, "attackmessage")) {
		extern int vnum_attack_message(char *searchname, char_data *ch);
		if (!vnum_attack_message(buf2, ch)) {
			msg_to_char(ch, "No attack messages by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "augment")) {
		extern int vnum_augment(char *searchname, char_data *ch);
		if (!vnum_augment(buf2, ch)) {
			msg_to_char(ch, "No augments by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "building")) {
		if (!vnum_building(buf2, ch)) {
			msg_to_char(ch, "No buildings by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "book")) {
		if (!vnum_book(buf2, ch)) {
			send_to_char("No books by that name.\r\n", ch);
		}
	}
	else if (is_abbrev(buf, "class")) {
		extern int vnum_class(char *searchname, char_data *ch);
		if (!vnum_class(buf2, ch)) {
			msg_to_char(ch, "No classes by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "crop")) {
		if (!vnum_crop(buf2, ch)) {
			msg_to_char(ch, "No crops by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "event")) {
		extern int vnum_event(char *searchname, char_data *ch);
		if (!vnum_event(buf2, ch)) {
			msg_to_char(ch, "No events by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "faction")) {
		extern int vnum_faction(char *searchname, char_data *ch);
		if (!vnum_faction(buf2, ch)) {
			msg_to_char(ch, "No factions by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "global")) {	// takes precedence on 'g'
		if (!vnum_global(buf2, ch)) {
			msg_to_char(ch, "No globals by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "generic")) {
		extern int vnum_generic(char *searchname, char_data *ch);
		if (!vnum_generic(buf2, ch)) {
			msg_to_char(ch, "No generics by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "morph")) {
		extern int vnum_morph(char *searchname, char_data *ch);
		if (!vnum_morph(buf2, ch)) {
			msg_to_char(ch, "No morphs by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "progression")) {
		extern int vnum_progress(char *searchname, char_data *ch);
		if (!vnum_progress(buf2, ch)) {
			msg_to_char(ch, "No progression goals by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "quest")) {
		extern int vnum_quest(char *searchname, char_data *ch);
		if (!vnum_quest(buf2, ch)) {
			msg_to_char(ch, "No quests by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "roomtemplate")) {
		if (!vnum_room_template(buf2, ch)) {
			msg_to_char(ch, "No room templates by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "sector")) {
		if (!vnum_sector(buf2, ch)) {
			msg_to_char(ch, "No sectors by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "shop")) {
		extern int vnum_shop(char *searchname, char_data *ch);
		if (!vnum_shop(buf2, ch)) {
			msg_to_char(ch, "No shops by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "skill")) {
		extern int vnum_skill(char *searchname, char_data *ch);
		if (!vnum_skill(buf2, ch)) {
			msg_to_char(ch, "No skills by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "social")) {
		extern int vnum_social(char *searchname, char_data *ch);
		if (!vnum_social(buf2, ch)) {
			msg_to_char(ch, "No socials by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "trigger")) {
		if (!vnum_trigger(buf2, ch)) {
			msg_to_char(ch, "No triggers by that name.\r\n");
		}
	}
	else if (is_abbrev(buf, "vehicle")) {
		extern int vnum_vehicle(char *searchname, char_data *ch);
		if (!vnum_vehicle(buf2, ch)) {
			msg_to_char(ch, "No vehicles by that name.\r\n");
		}
	}
	else {
		send_to_char("Usage: vnum <type> <name>\r\n", ch);
		send_to_char("Valid types are: adventure, ability, archetype, attack, augment, book,\r\n"
		"                 building, craft, class, crop, event, faction, global, generic,\r\n"
		"                 mob, morph, obj,  progress, quest, roomtemplate, sector, shop,\r\n"
		"                 skill, social, trigger, vehicle\r\n", ch);
	}
}


ACMD(do_vstat) {
	bool details;
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	empire_data *emp;
	char_data *mob;
	obj_data *obj;
	any_vnum number;

	half_chop(argument, arg1, arg);	// type
	half_chop(arg, arg2, arg3);	// vnum, optional -d
	details = (*arg3 && is_abbrev(arg3, "-details"));

	if (!*arg1 || !*arg2 || !isdigit(*arg2)) {
		send_to_char("Usage: vstat <type> <vnum>\r\n", ch);
		return;
	}
	if ((number = atoi(arg2)) < 0) {
		send_to_char("A NEGATIVE number??\r\n", ch);
		return;
	}
	
	if (is_abbrev(arg1, "adventure")) {	// alphabetic precedence for "vstat a"
		adv_data *adv = adventure_proto(number);
		if (!adv) {
			msg_to_char(ch, "There is no adventure zone with that number.\r\n");
			return;
		}
		do_stat_adventure(ch, adv);
	}
	else if (is_abbrev(arg1, "ability")) {
		void do_stat_ability(char_data *ch, ability_data *abil, bool details);
		ability_data *abil = find_ability_by_vnum(number);
		if (!abil) {
			msg_to_char(ch, "There is no ability with that number.\r\n");
			return;
		}
		do_stat_ability(ch, abil, details);
	}
	else if (is_abbrev(arg1, "archetype")) {
		void do_stat_archetype(char_data *ch, archetype_data *arch);
		archetype_data *arch = archetype_proto(number);
		if (!arch) {
			msg_to_char(ch, "There is no archetype with that number.\r\n");
			return;
		}
		do_stat_archetype(ch, arch);
	}
	else if (is_abbrev(arg1, "attack") || is_abbrev(arg1, "attackmessage")) {
		void do_stat_attack_message(char_data *ch, attack_message_data *amd, bool details);
		attack_message_data *amd = real_attack_message(number);
		if (!amd) {
			msg_to_char(ch, "There is no attack message with that number.\r\n");
			return;
		}
		do_stat_attack_message(ch, amd, details);
	}
	else if (is_abbrev(arg1, "augment")) {
		void do_stat_augment(char_data *ch, augment_data *aug);
		augment_data *aug = augment_proto(number);
		if (!aug) {
			msg_to_char(ch, "There is no augment with that number.\r\n");
			return;
		}
		do_stat_augment(ch, aug);
	}
	else if (is_abbrev(arg1, "building")) {	// alphabetic precedence for "vstat b"
		bld_data *bld = building_proto(number);
		if (!bld) {
			msg_to_char(ch, "There is no building with that number.\r\n");
			return;
		}
		do_stat_building(ch, bld, details);
	}
	else if (is_abbrev(arg1, "book")) {
		book_data *book = book_proto(number);
		if (!book) {
			msg_to_char(ch, "There is no book with that number.\r\n");
			return;
		}
		do_stat_book(ch, book, details);
	}
	else if (is_abbrev(arg1, "craft")) {	// alphabetic precedence for "vstat c"
		craft_data *craft = craft_proto(number);
		if (!craft) {
			msg_to_char(ch, "There is no craft with that number.\r\n");
			return;
		}
		do_stat_craft(ch, craft);
	}
	else if (is_abbrev(arg1, "class")) {
		void do_stat_class(char_data *ch, class_data *cls);
		class_data *cls = find_class_by_vnum(number);
		if (!cls) {
			msg_to_char(ch, "There is no class with that number.\r\n");
			return;
		}
		do_stat_class(ch, cls);
	}
	else if (is_abbrev(arg1, "crop")) {
		crop_data *crop = crop_proto(number);
		if (!crop) {
			msg_to_char(ch, "There is no crop with that number.\r\n");
			return;
		}
		do_stat_crop(ch, crop, details);
	}
	else if (!strn_cmp(arg1, "emp", 3) && is_abbrev(arg1, "empire")) {
		if (!(emp = real_empire(number))) {
			msg_to_char(ch, "There is no empire with that vnum.\r\n");
			return;
		}
		do_stat_empire(ch, emp);
	}
	else if (is_abbrev(arg1, "event")) {
		void do_stat_event(char_data *ch, event_data *event);
		event_data *event = find_event_by_vnum(number);
		if (!event) {
			msg_to_char(ch, "There is no event with that number.\r\n");
			return;
		}
		do_stat_event(ch, event);
	}
	else if (is_abbrev(arg1, "faction")) {
		void do_stat_faction(char_data *ch, faction_data *fct);
		faction_data *fct = find_faction_by_vnum(number);
		if (!fct) {
			msg_to_char(ch, "There is no faction with that number.\r\n");
			return;
		}
		do_stat_faction(ch, fct);
	}
	else if (is_abbrev(arg1, "global")) {	// precedence on 'g'
		struct global_data *glb = global_proto(number);
		if (!glb) {
			msg_to_char(ch, "There is no global with that number.\r\n");
			return;
		}
		do_stat_global(ch, glb);
	}
	else if (is_abbrev(arg1, "generic")) {
		void do_stat_generic(char_data *ch, generic_data *gen);
		generic_data *gen = real_generic(number);
		if (!gen) {
			msg_to_char(ch, "There is no generic with that number.\r\n");
			return;
		}
		do_stat_generic(ch, gen);
	}
	else if (is_abbrev(arg1, "mobile")) {
		if (!mob_proto(number)) {
			send_to_char("There is no monster with that number.\r\n", ch);
			return;
		}
		mob = read_mobile(number, TRUE);
		// put it somewhere, briefly
		char_to_room(mob, world_table);
		do_stat_character(ch, mob, details);
		extract_char(mob);
	}
	else if (is_abbrev(arg1, "morph")) {
		void do_stat_morph(char_data *ch, morph_data *morph);
		morph_data *morph = morph_proto(number);
		if (!morph) {
			msg_to_char(ch, "There is no morph with that number.\r\n");
			return;
		}
		do_stat_morph(ch, morph);
	}
	else if (is_abbrev(arg1, "object")) {
		if (!obj_proto(number)) {
			send_to_char("There is no object with that number.\r\n", ch);
			return;
		}
		obj = read_object(number, TRUE);
		do_stat_object(ch, obj, details);
		extract_obj(obj);
	}
	else if (is_abbrev(arg1, "progression")) {
		void do_stat_progress(char_data *ch, progress_data *prg);
		progress_data *prg = real_progress(number);
		if (!prg) {
			msg_to_char(ch, "There is no progression goal with that number.\r\n");
			return;
		}
		do_stat_progress(ch, prg);
	}
	else if (is_abbrev(arg1, "quest")) {
		void do_stat_quest(char_data *ch, quest_data *quest);
		quest_data *quest = quest_proto(number);
		if (!quest) {
			msg_to_char(ch, "There is no quest with that number.\r\n");
			return;
		}
		do_stat_quest(ch, quest);
	}
	else if (is_abbrev(arg1, "roomtemplate")) {
		room_template *rmt = room_template_proto(number);
		if (!rmt) {
			msg_to_char(ch, "There is no room template with that number.\r\n");
			return;
		}
		do_stat_room_template(ch, rmt, details);
	}
	else if (is_abbrev(arg1, "sector")) {
		sector_data *sect = sector_proto(number);
		if (!sect) {
			msg_to_char(ch, "There is no sector with that number.\r\n");
			return;
		}
		do_stat_sector(ch, sect, details);
	}
	else if (is_abbrev(arg1, "shop")) {
		void do_stat_shop(char_data *ch, shop_data *shop);
		shop_data *shop = real_shop(number);
		if (!shop) {
			msg_to_char(ch, "There is no shop with that number.\r\n");
			return;
		}
		do_stat_shop(ch, shop);
	}
	else if (is_abbrev(arg1, "skill")) {
		void do_stat_skill(char_data *ch, skill_data *skill);
		skill_data *skill = find_skill_by_vnum(number);
		if (!skill) {
			msg_to_char(ch, "There is no skill with that number.\r\n");
			return;
		}
		do_stat_skill(ch, skill);
	}
	else if (is_abbrev(arg1, "social")) {
		void do_stat_social(char_data *ch, social_data *soc);
		social_data *soc = social_proto(number);
		if (!soc) {
			msg_to_char(ch, "There is no social with that number.\r\n");
			return;
		}
		do_stat_social(ch, soc);
	}
	else if (is_abbrev(arg1, "trigger")) {
		trig_data *trig = real_trigger(number);
		if (!trig) {
			msg_to_char(ch, "That vnum does not exist.\r\n");
			return;
		}

		do_stat_trigger(ch, trig);
	}
	else if (is_abbrev(arg1, "vehicle")) {
		vehicle_data *veh;
		if (!vehicle_proto(number)) {
			msg_to_char(ch, "There is no vehicle with that vnum.\r\n");
		}
		else {
			// load a temporary copy
			veh = read_vehicle(number, TRUE);
			
			// do not waste an idnum here
			if (VEH_IDNUM(veh) == data_get_int(DATA_TOP_VEHICLE_ID)) {
				// give back the top id
				data_set_int(DATA_TOP_VEHICLE_ID, data_get_int(DATA_TOP_VEHICLE_ID) - 1);
			}
			
			do_stat_vehicle(ch, veh, details);
			extract_vehicle(veh);
		}
	}
	else {
		send_to_char("Usage: vstat <type> <vnum>\r\n", ch);
		send_to_char("Valid types are: adventure, ability, archetype, attack, augment, book,\r\n"
		"                 building, craft, class, crop, event, faction, global, generic,\r\n"
		"                 mob, morph, obj,  progress, quest, roomtemplate, sector, shop,\r\n"
		"                 skill, social, trigger, vehicle\r\n", ch);
	}
}


ACMD(do_wizlock) {
	int value;
	const char *when;

	argument = one_argument(argument, arg);
	if (*arg) {
		value = atoi(arg);
		if (value < 0 || value > GET_ACCESS_LEVEL(ch)) {
			send_to_char("Invalid wizlock value.\r\n", ch);
			return;
		}
		if (wizlock_message) {
			free(wizlock_message);
			wizlock_message = NULL;
		}
		skip_spaces(&argument);
		if (*argument && value != 0) {	// do not copy message on unlock
			wizlock_message = str_dup(argument);
		}
		wizlock_level = value;
		when = "now";
		
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has %swizlocked the game at level %d", GET_NAME(ch), (wizlock_level < 1 ? "un" : ""), wizlock_level);
		if (wizlock_message) {
			syslog(SYS_GC, LVL_START_IMM, TRUE, "GC: Wizlock: %s", wizlock_message);
		}
	}
	else {
		when = "currently";
	}

	// messaging
	switch (wizlock_level) {
		case 0:
			msg_to_char(ch, "The game is %s completely open.\r\n", when);
			break;
		case 1:
			msg_to_char(ch, "The game is %s closed to new players.\r\n", when);
			break;
		default:
			msg_to_char(ch, "Only level %d and above may enter the game %s.\r\n", wizlock_level, when);
			break;
	}
	
	if (wizlock_message) {
		msg_to_char(ch, "%s\r\n", wizlock_message);
	}
}


// General fn for wizcommands of the sort: cmd <player>
ACMD(do_wizutil) {
	char_data *vict;
	int result;

	one_argument(argument, arg);

	if (!*arg)
		send_to_char("Yes, but for whom?!?\r\n", ch);
	else if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD)))
		send_to_char("There is no such player.\r\n", ch);
	else if (IS_NPC(vict))
		send_to_char("You can't do that to a mob!\r\n", ch);
	else if (get_highest_access_level(GET_ACCOUNT(vict)) >= GET_ACCESS_LEVEL(ch) && subcmd != SCMD_THAW) {
		send_to_char("Hmmm... you'd better not.\r\n", ch);
	}
	else {
		switch (subcmd) {
			case SCMD_NOTITLE:
				result = ((TOGGLE_BIT(GET_ACCOUNT(vict)->flags, ACCT_NOTITLE)) & ACCT_NOTITLE);
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: Notitle %s for %s by %s", ONOFF(result), GET_NAME(vict), GET_NAME(ch));
				msg_to_char(ch, "Notitle %s for %s.\r\n", ONOFF(result), GET_NAME(vict));
				break;
			case SCMD_MUTE:
				result = ((TOGGLE_BIT(GET_ACCOUNT(vict)->flags, ACCT_MUTED)) & ACCT_MUTED);
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: Mute %s for %s by %s", ONOFF(result), GET_NAME(vict), GET_NAME(ch));
				msg_to_char(ch, "Mute %s for %s.\r\n", ONOFF(result), GET_NAME(vict));
				break;
			case SCMD_FREEZE:
				if (ch == vict) {
					send_to_char("Oh, yeah, THAT'S real smart...\r\n", ch);
					return;
				}
				if (ACCOUNT_FLAGGED(vict, ACCT_FROZEN)) {
					send_to_char("Your victim is already pretty cold.\r\n", ch);
					return;
				}
				SET_BIT(GET_ACCOUNT(vict)->flags, ACCT_FROZEN);
				send_to_char("A bitter wind suddenly rises and drains every erg of heat from your body!\r\nYou feel frozen!\r\n", vict);
				send_to_char("Frozen.\r\n", ch);
				act("A sudden cold wind conjured from nowhere freezes $n!", FALSE, vict, 0, 0, TO_ROOM | DG_NO_TRIG);
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s frozen by %s", GET_NAME(vict), GET_NAME(ch));
				break;
			case SCMD_THAW:
				if (!ACCOUNT_FLAGGED(vict, ACCT_FROZEN)) {
					send_to_char("Sorry, your victim is not morbidly encased in ice at the moment.\r\n", ch);
					return;
				}
				syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s un-frozen by %s", GET_NAME(vict), GET_NAME(ch));
				REMOVE_BIT(GET_ACCOUNT(vict)->flags, ACCT_FROZEN);
				send_to_char("A fireball suddenly explodes in front of you, melting the ice!\r\nYou feel thawed.\r\n", vict);
				send_to_char("Thawed.\r\n", ch);
				act("A sudden fireball conjured from nowhere thaws $n!", FALSE, vict, 0, 0, TO_ROOM | DG_NO_TRIG);
				break;
			default:
				log("SYSERR: Unknown subcmd %d passed to do_wizutil (%s)", subcmd, __FILE__);
				break;
		}
		SAVE_ACCOUNT(GET_ACCOUNT(vict));
		queue_delayed_update(vict, CDU_SAVE);
	}
}
