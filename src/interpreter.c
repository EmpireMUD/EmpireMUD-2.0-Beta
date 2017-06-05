/* ************************************************************************
*   File: interpreter.c                                   EmpireMUD 2.0b5 *
*  Usage: parse user commands, search for specials, call ACMD functions   *
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
#include "comm.h"
#include "interpreter.h"
#include "db.h"
#include "utils.h"
#include "handler.h"
#include "utils.h"
#include "olc.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Command Prototypes
*   Master Command List
*   Command Interpreter
*   Alias System
*   Helper Functions
*   Command Functions
*   Character Creation
*   Menu Interpreter Functions
*/

// external vars
extern struct archetype_menu_type archetype_menu[];

// external funcs
void parse_archetype_menu(descriptor_data *desc, char *argument);

// locals
void set_creation_state(descriptor_data *d, int state);
void show_bonus_trait_menu(char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// COMMAND PROTOTYPES //////////////////////////////////////////////////////

ACMD(do_abandon);
ACMD(do_accept);
ACMD(do_addnotes);
ACMD(do_admin_util);
ACMD(do_advance);
ACMD(do_adventure);
ACMD(do_affects);
ACMD(do_alacrity);
ACMD(do_alias);
ACMD(do_alternate);
ACMD(do_approve);
ACMD(do_assist);
ACMD(do_at);
ACMD(do_autostore);
ACMD(do_autowiz);
ACMD(do_avoid);

ACMD(do_backstab);
ACMD(do_ban);
ACMD(do_barde);
ACMD(do_bash);
ACMD(do_bathe);
ACMD(do_bite);
ACMD(do_blind);
ACMD(do_bloodsweat);
ACMD(do_board);
ACMD(do_boost);
ACMD(do_breakreply);
ACMD(do_build);
ACMD(do_butcher);

ACMD(do_cede);
ACMD(do_changepass);
ACMD(do_chip);
ACMD(do_chop);
ACMD(do_circle);
ACMD(do_city);
ACMD(do_claim);
ACMD(do_class);
ACMD(do_claws);
ACMD(do_cleanse);
ACMD(do_clearabilities);
ACMD(do_clearmeters);
ACMD(do_coins);
ACMD(do_collapse);
ACMD(do_colorburst);
ACMD(do_combine);
ACMD(do_command);
ACMD(do_commands);
ACMD(do_confer);
ACMD(do_config);
ACMD(do_confirm);
ACMD(do_consider);
ACMD(do_cooldowns);
ACMD(do_counterspell);
ACMD(do_create);
ACMD(do_credits);
ACMD(do_customize);

ACMD(do_damage_spell);
ACMD(do_darkness);
ACMD(do_date);
ACMD(do_dc);
ACMD(do_deathshroud);
ACMD(do_dedicate);
ACMD(do_defect);
ACMD(do_demote);
ACMD(do_deposit);
ACMD(do_designate);
ACMD(do_diagnose);
ACMD(do_dig);
ACMD(do_diplomacy);
ACMD(do_disarm);
ACMD(do_disembark);
ACMD(do_disenchant);
ACMD(do_disguise);
ACMD(do_dismantle);
ACMD(do_dismiss);
ACMD(do_dismount);
ACMD(do_dispatch);
ACMD(do_dispel);
ACMD(do_display);
ACMD(do_distance);
ACMD(do_diversion);
ACMD(do_douse);
ACMD(do_drag);
ACMD(do_drive);
ACMD(do_draw);
ACMD(do_drink);
ACMD(do_drop);

ACMD(do_eartharmor);
ACMD(do_earthmeld);
ACMD(do_eat);
ACMD(do_echo);
ACMD(do_edelete);
ACMD(do_editnotes);
ACMD(do_eedit);
ACMD(do_efind);
ACMD(do_elog);
ACMD(do_emotd);
ACMD(do_empire_inventory);
ACMD(do_empires);
ACMD(do_enervate);
ACMD(do_enroll);
ACMD(do_entangle);
ACMD(do_enter);
ACMD(do_equipment);
ACMD(do_esay);
ACMD(do_escape);
ACMD(do_estats);
ACMD(do_examine);
ACMD(do_excavate);
ACMD(do_exchange);
ACMD(do_execute);
ACMD(do_exit);
ACMD(do_exits);
ACMD(do_expel);

ACMD(do_factions);
ACMD(do_familiar);
ACMD(do_feed);
ACMD(do_fightmessages);
ACMD(do_file);
ACMD(do_fillin);
ACMD(do_findmaintenance);
ACMD(do_fire);
ACMD(do_firstaid);
ACMD(do_fish);
ACMD(do_flee);
ACMD(do_fly);
ACMD(do_follow);
ACMD(do_forage);
ACMD(do_force);
ACMD(do_foresight);
ACMD(do_forgive);
ACMD(do_fullsave);

ACMD(do_gather);
ACMD(do_gecho);
ACMD(do_gen_augment);
ACMD(do_gen_craft);
ACMD(do_gen_door);
ACMD(do_gen_ps);
ACMD(do_gen_write);
ACMD(do_get);
ACMD(do_give);
ACMD(do_goto);
ACMD(do_grab);
ACMD(do_group);
ACMD(do_gsay);

ACMD(do_harness);
ACMD(do_harvest);
ACMD(do_hasten);
ACMD(do_heal);
ACMD(do_heartstop);
ACMD(do_help);
ACMD(do_helpsearch);
ACMD(do_herd);
ACMD(do_hide);
ACMD(do_history);
ACMD(do_hit);
ACMD(do_home);
ACMD(do_hostile);
ACMD(do_howl);

ACMD(do_identify);
ACMD(do_ignore);
ACMD(do_import);
ACMD(do_infiltrate);
ACMD(do_instance);
ACMD(do_info);
ACMD(do_inspire);
ACMD(do_insult);
ACMD(do_interlink);
ACMD(do_inventory);
ACMD(do_invis);
ACMD(do_island);
ACMD(do_islands);

ACMD(do_jab);

ACMD(do_keep);
ACMD(do_kick);

ACMD(do_land);
ACMD(do_last);
ACMD(do_lay);
ACMD(do_lead);
ACMD(do_library);
ACMD(do_light);
ACMD(do_load);
ACMD(do_load_vehicle);
ACMD(do_look);

ACMD(do_mail);
ACMD(do_maintain);
ACMD(do_majesty);
ACMD(do_manashield);
ACMD(do_mapout);
ACMD(do_mapsize);
ACMD(do_mark);
ACMD(do_meters);
ACMD(do_milk);
ACMD(do_mine);
ACMD(do_mint);
ACMD(do_mirrorimage);
ACMD(do_missing_help_files);
ACMD(do_moonrise);
ACMD(do_morph);
ACMD(do_mount);
ACMD(do_move);
ACMD(do_moveeinv);
ACMD(do_mudstats);
ACMD(do_mummify);
ACMD(do_mydescription);

ACMD(do_nearby);
ACMD(do_nightsight);
ACMD(do_nodismantle);
ACMD(do_noskill);

ACMD(do_olc);
ACMD(do_oset);
ACMD(do_order);
ACMD(do_outrage);

ACMD(do_page);
ACMD(do_pan);
ACMD(do_pick);
ACMD(do_pickpocket);
ACMD(do_plant);
ACMD(do_play);
ACMD(do_playerdelete);
ACMD(do_pledge);
ACMD(do_point);
ACMD(do_poofset);
ACMD(do_portal);
ACMD(do_pour);
ACMD(do_prick);
ACMD(do_promote);
ACMD(do_prompt);
ACMD(do_prospect);
ACMD(do_pub_comm);
ACMD(do_publicize);
ACMD(do_purge);
ACMD(do_purify);
ACMD(do_put);

ACMD(do_quaff);
ACMD(do_quarry);
ACMD(do_quest);
ACMD(do_quit);

ACMD(do_radiance);
ACMD(do_random);
ACMD(do_read);
ACMD(do_ready);
ACMD(do_reboot);
ACMD(do_recipes);
ACMD(do_reclaim);
ACMD(do_recolor);
ACMD(do_reforge);
ACMD(do_regenerate);
ACMD(do_rejuvenate);
ACMD(do_reload);
ACMD(do_remove);
ACMD(do_repair);
ACMD(do_reply);
ACMD(do_rescale);
ACMD(do_rescue);
ACMD(do_respawn);
ACMD(do_respond);
ACMD(do_rest);
ACMD(do_restore);
ACMD(do_resurrect);
ACMD(do_retrieve);
ACMD(do_return);
ACMD(do_reward);
ACMD(do_ritual);
ACMD(do_roadsign);
ACMD(do_roll);
ACMD(do_roster);

ACMD(do_sacrifice);
ACMD(do_sap);
ACMD(do_save);
ACMD(do_saw);
ACMD(do_say);
ACMD(do_scan);
ACMD(do_score);
ACMD(do_scrap);
ACMD(do_scrape);
ACMD(do_search);
ACMD(do_send);
ACMD(do_selfdelete);
ACMD(do_separate);
ACMD(do_set);
ACMD(do_shadowcage);
ACMD(do_shadowstep);
ACMD(do_share);
ACMD(do_shear);
ACMD(do_sheathe);
ACMD(do_ship);
ACMD(do_shoot);
ACMD(do_show);
ACMD(do_siphon);
ACMD(do_sire);
ACMD(do_sit);
ACMD(do_skills);
ACMD(do_skin);
ACMD(do_skybrand);
ACMD(do_slash_channel);
ACMD(do_slay);
ACMD(do_sleep);
ACMD(do_slow);
ACMD(do_sneak);
ACMD(do_snoop);
ACMD(do_socials);
ACMD(do_soulmask);
ACMD(do_soulsight);
ACMD(do_spec_comm);
ACMD(do_specialize);
ACMD(do_split);
ACMD(do_stake);
ACMD(do_stand);
ACMD(do_stat);
ACMD(do_steal);
ACMD(do_stop);
ACMD(do_store);
ACMD(do_string_editor);
ACMD(do_struggle);
ACMD(do_summary);
ACMD(do_summon);
ACMD(do_survey);
ACMD(do_swap);
ACMD(do_switch);
ACMD(do_syslog);

ACMD(do_tan);
ACMD(do_tavern);
ACMD(do_tedit);
ACMD(do_tell);
ACMD(do_terrify);
ACMD(do_territory);
ACMD(do_throw);
ACMD(do_tie);
ACMD(do_time);
ACMD(do_tip);
ACMD(do_title);
ACMD(do_toggle);
ACMD(do_tomb);
ACMD(do_track);
ACMD(do_trade);
ACMD(do_trans);
ACMD(do_transport);
ACMD(do_tunnel);

ACMD(do_unban);
ACMD(do_unbind);
ACMD(do_unharness);
ACMD(do_unload_vehicle);
ACMD(do_unpublicize);
ACMD(do_unquest);
ACMD(do_unshare);
ACMD(do_upgrade);
ACMD(do_use);
ACMD(do_users);

ACMD(do_veintap);
ACMD(do_vigor);
ACMD(do_visible);
ACMD(do_vnum);
ACMD(do_vstat);

ACMD(do_wake);
ACMD(do_warehouse);
ACMD(do_weaken);
ACMD(do_wear);
ACMD(do_weather);
ACMD(do_where);
ACMD(do_whereami);
ACMD(do_whisperstride);
ACMD(do_who);
ACMD(do_whois);
ACMD(do_wield);
ACMD(do_withdraw);
ACMD(do_wizlock);
ACMD(do_wizutil);
ACMD(do_workforce);
ACMD(do_worm);
ACMD(do_write);

/* DG Script ACMD's */
ACMD(do_tattach);
ACMD(do_tdetach);
ACMD(do_madventurecomplete);
ACMD(do_maggro);
ACMD(do_masound);
ACMD(do_mkill);
ACMD(do_mjunk);
ACMD(do_mdoor);
ACMD(do_mechoaround);
ACMD(do_mechoneither);
ACMD(do_msend);
ACMD(do_mecho);
ACMD(do_mload);
ACMD(do_mmorph);
ACMD(do_mmove);
ACMD(do_mpurge);
ACMD(do_mquest);
ACMD(do_mgoto);
ACMD(do_maoe);
ACMD(do_mat);
ACMD(do_mbuild);
ACMD(do_mdamage);
ACMD(do_mdot);
ACMD(do_mrestore);
ACMD(do_msiege);
ACMD(do_mteleport);
ACMD(do_mterracrop);
ACMD(do_mterraform);
ACMD(do_mforce);
ACMD(do_mhunt);
ACMD(do_mremember);
ACMD(do_mforget);
ACMD(do_mscale);
ACMD(do_mtransform);
ACMD(do_mbuildingecho);
ACMD(do_mown);
ACMD(do_mregionecho);
ACMD(do_mvehicleecho);
ACMD(do_vdelete);
ACMD(do_mfollow);


 //////////////////////////////////////////////////////////////////////////////
//// MASTER COMMAND LIST /////////////////////////////////////////////////////

/* This is the Master Command List(tm).
 * You can put new commands in, take commands out, change the order
 * they appear in, etc.  You can adjust the "priority" of commands
 * simply by changing the order they appear in the command list.
 * (For example, if you want "as" to mean "assist" instead of "ask",
 * just put "assist" above "ask" in the Master Command List(tm).
 *
 * In general, utility commands such as "at" should have high priority;
 * infrequently used and dangerously destructive commands should have low
 * priority.
 */
#define NO_MIN  0

#define ABILITY_CMD( string, pos, func, lvl, type, abil )  { string, pos, func, lvl, NO_GRANTS, NO_SCMD, type, NOBITS, abil }
#define GRANT_CMD( string, pos, func, lvl, type, grant )  { string, pos, func, lvl, grant, NO_SCMD, type, NOBITS, NO_ABIL }
#define SIMPLE_CMD( string, pos, func, lvl, type )  { string, pos, func, lvl, NO_GRANTS, NO_SCMD, type, NOBITS, NO_ABIL }
#define SCMD_CMD( string, pos, func, lvl, type, subcmd )  { string, pos, func, lvl, NO_GRANTS, subcmd, type, NOBITS, NO_ABIL }
#define STANDARD_CMD( string, pos, func, lvl, grants, subcmd, type, flags, abil )  { string, pos, func, lvl, grants, subcmd, type, flags, abil }

cpp_extern const struct command_info cmd_info[] = {
	/* this must be first -- for specprocs */
	STANDARD_CMD( "RESERVED", POS_DEAD, NULL, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_UTIL, CMD_NO_ABBREV, NO_ABIL ),
	
	// basic command setup:
	// STANDARD_CMD( "command", POS_x, do_function, MIN_LEVEL, GRANTS_x, SCMD_x, CTYPE_x, CMD_x, ABIL_x ),
	// STANDARD_CMD( "defaults", POS_STANDING, do_function, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_UTIL, NOBITS, NO_ABIL ),

	/* directions come before other commands to preserve abbrevs */
	SCMD_CMD( "north", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, NORTH ),
	SCMD_CMD( "east", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, EAST ),
	SCMD_CMD( "south", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, SOUTH ),
	SCMD_CMD( "west", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, WEST ),
	SCMD_CMD( "northeast", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, NORTHEAST ),
	SCMD_CMD( "northwest", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, NORTHWEST ),
	SCMD_CMD( "southeast", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, SOUTHEAST ),
	SCMD_CMD( "southwest", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, SOUTHWEST ),
	SCMD_CMD( "ne", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, NORTHEAST ),
	SCMD_CMD( "nw", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, NORTHWEST ),
	SCMD_CMD( "se", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, SOUTHEAST ),
	SCMD_CMD( "sw", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, SOUTHWEST ),
	SCMD_CMD( "up", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, UP ),
	SCMD_CMD( "down", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, DOWN ),
	SCMD_CMD( "fore", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, FORE ),
	SCMD_CMD( "starboard", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, STARBOARD ),
	SCMD_CMD( "port", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, PORT ),
	SCMD_CMD( "aft", POS_STANDING, do_move, NO_MIN, CTYPE_MOVE, AFT ),

	/* now, the main list */
	SIMPLE_CMD( "/", POS_DEAD, do_slash_channel, NO_MIN, CTYPE_COMM ),

	// . is "olc" for imms but "bookedit" for mortals
	GRANT_CMD( ".", POS_DEAD, do_olc, LVL_BUILDER, CTYPE_OLC, GRANT_OLC ),
	SCMD_CMD( ".", POS_STANDING, do_library, NO_MIN, CTYPE_UTIL, SCMD_BOOKEDIT ),
	
	SIMPLE_CMD( "at", POS_DEAD, do_at, LVL_START_IMM, CTYPE_IMMORTAL ),
	SIMPLE_CMD( "abandon", POS_RESTING, do_abandon, NO_MIN, CTYPE_EMPIRE ),
	STANDARD_CMD( "ablate", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_ABLATE, CTYPE_COMBAT, NOBITS, ABIL_ABLATE ),
	SCMD_CMD( "accept", POS_DEAD, do_accept, NO_MIN, CTYPE_UTIL, SCMD_ACCEPT ),
	STANDARD_CMD( "acidblast", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_ACIDBLAST, CTYPE_COMBAT, NOBITS, ABIL_ACIDBLAST ),
	SIMPLE_CMD( "adventure", POS_RESTING, do_adventure, NO_MIN, CTYPE_UTIL ),
	GRANT_CMD( "addnotes", POS_STANDING, do_addnotes, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_EDITNOTES ),
	GRANT_CMD( "advance", POS_DEAD, do_advance, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_ADVANCE ),
	SIMPLE_CMD( "alias", POS_DEAD, do_alias, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "alacrity", POS_RESTING, do_alacrity, NO_MIN, CTYPE_SKILL, ABIL_ALACRITY ),
	SIMPLE_CMD( "alternate", POS_DEAD, do_alternate, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "affects", POS_DEAD, do_affects, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "approve", POS_DEAD, do_approve, LVL_CIMPL, GRANT_APPROVE, SCMD_APPROVE, CTYPE_IMMORTAL, NOBITS, NO_ABIL ),
	STANDARD_CMD( "arclight", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_ARCLIGHT, CTYPE_COMBAT, NOBITS, ABIL_ARCLIGHT ),
	SIMPLE_CMD( "assist", POS_FIGHTING, do_assist, NO_MIN, CTYPE_COMBAT ),
	SCMD_CMD( "ask", POS_RESTING, do_spec_comm, NO_MIN, CTYPE_COMM, SCMD_ASK ),
	STANDARD_CMD( "astralclaw", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_ASTRALCLAW, CTYPE_COMBAT, NOBITS, ABIL_ASTRALCLAW ),
	SIMPLE_CMD( "autostore", POS_RESTING, do_autostore, LVL_CIMPL, CTYPE_IMMORTAL ),
	SIMPLE_CMD( "autowiz", POS_DEAD, do_autowiz, LVL_CIMPL, CTYPE_IMMORTAL ),
	SIMPLE_CMD( "avoid", POS_STANDING, do_avoid, NO_MIN, CTYPE_MOVE ),

	STANDARD_CMD( "build", POS_DEAD, do_build, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	ABILITY_CMD( "backstab", POS_FIGHTING, do_backstab, NO_MIN, CTYPE_COMBAT, ABIL_BACKSTAB ),
	GRANT_CMD( "ban", POS_DEAD, do_ban, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_BAN ),
	STANDARD_CMD( "barde", POS_STANDING, do_barde, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_SKILL, CMD_NO_ANIMALS, ABIL_BARDE ),
	ABILITY_CMD( "bash", POS_FIGHTING, do_bash, NO_MIN, CTYPE_COMBAT, ABIL_BASH ),
	SIMPLE_CMD( "bathe", POS_STANDING, do_bathe, NO_MIN, CTYPE_MOVE ),
	STANDARD_CMD( "bite", POS_FIGHTING, do_bite, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_COMBAT, CMD_NO_ANIMALS, ABIL_BITE ),
	ABILITY_CMD( "blind", POS_FIGHTING, do_blind, NO_MIN, CTYPE_COMBAT, ABIL_BLIND ),
	ABILITY_CMD( "bloodsweat", POS_SLEEPING, do_bloodsweat, NO_MIN, CTYPE_SKILL, ABIL_BLOODSWEAT ),
	SCMD_CMD( "board", POS_STANDING, do_board, NO_MIN, CTYPE_MOVE, SCMD_BOARD ),
	ABILITY_CMD( "boost", POS_RESTING, do_boost, NO_MIN, CTYPE_UTIL, ABIL_BOOST ),
	SCMD_CMD( "bookedit", POS_STANDING, do_library, NO_MIN, CTYPE_UTIL, SCMD_BOOKEDIT ),
	STANDARD_CMD( "brew", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_BREW, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "breakreply", POS_DEAD, do_breakreply, LVL_START_IMM, CTYPE_IMMORTAL ),
	SCMD_CMD( "bug", POS_DEAD, do_gen_write, NO_MIN, CTYPE_COMM, SCMD_BUG ),
	ABILITY_CMD( "butcher", POS_STANDING, do_butcher, NO_MIN, CTYPE_SKILL, ABIL_BUTCHER ),

	STANDARD_CMD( "chop", POS_STANDING, do_chop, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "cd", POS_DEAD, do_cooldowns, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "cede", POS_DEAD, do_cede, NO_MIN, CTYPE_EMPIRE ),
	STANDARD_CMD( "chant", POS_STANDING, do_ritual, NO_MIN, NO_GRANTS, SCMD_CHANT, CTYPE_SKILL, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "changepass", POS_DEAD, do_changepass, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "chip", POS_STANDING, do_chip, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "chronoblast", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_CHRONOBLAST, CTYPE_COMBAT, NOBITS, ABIL_CHRONOBLAST ),
	SIMPLE_CMD( "circle", POS_STANDING, do_circle, NO_MIN, CTYPE_MOVE ),
	SIMPLE_CMD( "cities", POS_DEAD, do_city, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "city", POS_DEAD, do_city, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "claim", POS_RESTING, do_claim, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "class", POS_DEAD, do_class, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "claws", POS_RESTING, do_claws, NO_MIN, CTYPE_SKILL, ABIL_CLAWS ),
	ABILITY_CMD( "cleanse", POS_FIGHTING, do_cleanse, NO_MIN, CTYPE_SKILL, ABIL_CLEANSE ),
	SCMD_CMD( "clear", POS_DEAD, do_gen_ps, NO_MIN, CTYPE_UTIL, SCMD_CLEAR ),
	SIMPLE_CMD( "clearmeters", POS_DEAD, do_clearmeters, NO_MIN, CTYPE_UTIL ),
	GRANT_CMD( "clearabilities", POS_DEAD, do_clearabilities, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_CLEARABILITIES ),
	SCMD_CMD( "close", POS_SITTING, do_gen_door, NO_MIN, CTYPE_MOVE, SCMD_CLOSE ),
	SCMD_CMD( "cls", POS_DEAD, do_gen_ps, NO_MIN, CTYPE_UTIL, SCMD_CLEAR ),
	SIMPLE_CMD( "coins", POS_DEAD, do_coins, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "collapse", POS_STANDING, do_collapse, NO_MIN, CTYPE_SKILL, ABIL_PORTAL_MASTER ),
	ABILITY_CMD( "colorburst", POS_FIGHTING, do_colorburst, NO_MIN, CTYPE_COMBAT, ABIL_COLORBURST ),
	SIMPLE_CMD( "combine", POS_RESTING, do_combine, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "command", POS_STANDING, do_command, NO_MIN, CTYPE_SKILL, ABIL_COMMAND ),
	SCMD_CMD( "commands", POS_DEAD, do_commands, NO_MIN, CTYPE_UTIL, SCMD_COMMANDS ),
	SIMPLE_CMD( "consider", POS_RESTING, do_consider, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "config", POS_DEAD, do_config, LVL_CIMPL, CTYPE_IMMORTAL ),
	SIMPLE_CMD( "confirm", POS_SLEEPING, do_confirm, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "confer", POS_RESTING, do_confer, NO_MIN, CTYPE_SKILL, ABIL_CONFER ),
	STANDARD_CMD( "cook", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_COOK, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "cooldowns", POS_DEAD, do_cooldowns, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "counterspell", POS_FIGHTING, do_counterspell, NO_MIN, CTYPE_SKILL, ABIL_COUNTERSPELL ),
	STANDARD_CMD( "craft", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_CRAFT, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SCMD_CMD( "credits", POS_DEAD, do_gen_ps, NO_MIN, CTYPE_UTIL, SCMD_CREDITS ),
	SIMPLE_CMD( "create", POS_STANDING, do_create, LVL_GOD, CTYPE_IMMORTAL ),
	SIMPLE_CMD( "customize", POS_STANDING, do_customize, NO_MIN, CTYPE_BUILD ),

	ABILITY_CMD( "darkness", POS_STANDING, do_darkness, NO_MIN, CTYPE_SKILL, ABIL_DARKNESS ),
	SCMD_CMD( "date", POS_DEAD, do_date, LVL_START_IMM, CTYPE_IMMORTAL, SCMD_DATE ),
	GRANT_CMD( "dc", POS_DEAD, do_dc, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_DC ),
	SCMD_CMD( "drink", POS_RESTING, do_drink, NO_MIN, CTYPE_UTIL, SCMD_DRINK ),
	ABILITY_CMD( "deathshroud", POS_STUNNED, do_deathshroud, NO_MIN, CTYPE_SKILL, ABIL_DEATHSHROUD ),
	STANDARD_CMD( "deathtouch", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_DEATHTOUCH, CTYPE_COMBAT, NOBITS, ABIL_DEATHTOUCH ),
	SIMPLE_CMD( "dedicate", POS_STANDING, do_dedicate, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "demote", POS_DEAD, do_demote, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "deposit", POS_STANDING, do_deposit, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "defect", POS_DEAD, do_defect, NO_MIN, CTYPE_EMPIRE ),
	SCMD_CMD( "designate", POS_STANDING, do_designate, NO_MIN, CTYPE_BUILD, SCMD_DESIGNATE ),
	SIMPLE_CMD( "diagnose", POS_RESTING, do_diagnose, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "diplomacy", POS_DEAD, do_diplomacy, NO_MIN, CTYPE_EMPIRE ),
	ABILITY_CMD( "disarm", POS_FIGHTING, do_disarm, NO_MIN, CTYPE_COMBAT, ABIL_DISARM ),
	ABILITY_CMD( "disenchant", POS_STANDING, do_disenchant, NO_MIN, CTYPE_SKILL, ABIL_DISENCHANT ),
	ABILITY_CMD( "disguise", POS_STANDING, do_disguise, NO_MIN, CTYPE_SKILL, ABIL_DISGUISE ),
	ABILITY_CMD( "dismount", POS_SITTING, do_dismount, NO_MIN, CTYPE_MOVE, ABIL_RIDE ),
	STANDARD_CMD( "dismantle", POS_STANDING, do_dismantle, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "dismiss", POS_STANDING, do_dismiss, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "disembark", POS_STANDING, do_disembark, NO_MIN, CTYPE_MOVE ),
	STANDARD_CMD( "dispirit", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_DISPIRIT, CTYPE_COMBAT, NOBITS, ABIL_DISPIRIT ),
	SIMPLE_CMD( "distance", POS_DEAD, do_distance, LVL_START_IMM, CTYPE_IMMORTAL ),
	STANDARD_CMD( "dig", POS_STANDING, do_dig, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "display", POS_DEAD, do_display, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "dispatch", POS_RESTING, do_dispatch, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "dispel", POS_FIGHTING, do_dispel, NO_MIN, CTYPE_SKILL, ABIL_DISPEL ),
	ABILITY_CMD( "diversion", POS_FIGHTING, do_diversion, NO_MIN, CTYPE_SKILL, ABIL_DIVERSION ),
	SIMPLE_CMD( "douse", POS_STANDING, do_douse, NO_MIN, CTYPE_BUILD ),
	SCMD_CMD( "drop", POS_RESTING, do_drop, NO_MIN, CTYPE_MOVE, SCMD_DROP ),
	STANDARD_CMD( "drag", POS_STANDING, do_drag, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "draw", POS_RESTING, do_draw, NO_MIN, CTYPE_COMBAT ),
	STANDARD_CMD( "drive", POS_SITTING, do_drive, NO_MIN, NO_GRANTS, SCMD_DRIVE, CTYPE_MOVE, CMD_NO_ANIMALS, NO_ABIL ),

	SCMD_CMD( "eat", POS_RESTING, do_eat, NO_MIN, CTYPE_UTIL, SCMD_EAT ),
	ABILITY_CMD( "eartharmor", POS_RESTING, do_eartharmor, NO_MIN, CTYPE_SKILL, ABIL_EARTHARMOR ),
	ABILITY_CMD( "earthmeld", POS_STUNNED, do_earthmeld, NO_MIN, CTYPE_MOVE, ABIL_EARTHMELD ),
	STANDARD_CMD( "echo", POS_SLEEPING, do_echo, LVL_CIMPL, GRANT_ECHO, SCMD_ECHO, CTYPE_IMMORTAL, NOBITS, NO_ABIL ),
	GRANT_CMD( "editnotes", POS_STANDING, do_editnotes, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_EDITNOTES ),
	SIMPLE_CMD( "eedit", POS_DEAD, do_eedit, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "efind", POS_SLEEPING, do_efind, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "elog", POS_DEAD, do_elog, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "empires", POS_DEAD, do_empires, NO_MIN, CTYPE_EMPIRE ),
	SCMD_CMD( "empireidentify", POS_DEAD, do_empire_inventory, NO_MIN, CTYPE_EMPIRE, SCMD_EIDENTIFY ),
	SCMD_CMD( "empireinventory", POS_DEAD, do_empire_inventory, NO_MIN, CTYPE_EMPIRE, SCMD_EINVENTORY ),
	SCMD_CMD( "einventory", POS_DEAD, do_empire_inventory, NO_MIN, CTYPE_EMPIRE, SCMD_EINVENTORY ),
	SCMD_CMD( "eidentify", POS_DEAD, do_empire_inventory, NO_MIN, CTYPE_EMPIRE, SCMD_EIDENTIFY ),
	SIMPLE_CMD( "emotd", POS_DEAD, do_emotd, NO_MIN, CTYPE_EMPIRE ),
	SCMD_CMD( "emote", POS_RESTING, do_echo, NO_MIN, CTYPE_COMM, SCMD_EMOTE ),
	SCMD_CMD( ":", POS_RESTING, do_echo, NO_MIN, CTYPE_COMM, SCMD_EMOTE ),
	SCMD_CMD( "empirehistory", POS_DEAD, do_history, NO_MIN, CTYPE_COMM, CHANNEL_HISTORY_EMPIRE ),
	SCMD_CMD( "ehistory", POS_DEAD, do_history, NO_MIN, CTYPE_COMM, CHANNEL_HISTORY_EMPIRE ),
	STANDARD_CMD( "enchant", POS_STANDING, do_gen_augment, NO_MIN, NO_GRANTS, AUGMENT_ENCHANTMENT, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	ABILITY_CMD( "enervate", POS_FIGHTING, do_enervate, NO_MIN, CTYPE_COMBAT, ABIL_ENERVATE ),
	SIMPLE_CMD( "enter", POS_STANDING, do_enter, NO_MIN, CTYPE_MOVE ),
	ABILITY_CMD( "entangle", POS_FIGHTING, do_entangle, NO_MIN, CTYPE_COMBAT, ABIL_ENTANGLE ),
	SIMPLE_CMD( "enroll", POS_DEAD, do_enroll, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "equipment", POS_DEAD, do_equipment, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "erode", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_ERODE, CTYPE_COMBAT, NOBITS, ABIL_ERODE ),
	SIMPLE_CMD( "esay", POS_DEAD, do_esay, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "etalk", POS_DEAD, do_esay, NO_MIN, CTYPE_EMPIRE ),
	ABILITY_CMD( "escape", POS_STANDING, do_escape, NO_MIN, CTYPE_MOVE, ABIL_ESCAPE ),
	SIMPLE_CMD( "estats", POS_DEAD, do_estats, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "examine", POS_SITTING, do_examine, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "excavate", POS_STANDING, do_excavate, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "exchange", POS_STANDING, do_exchange, NO_MIN, CTYPE_BUILD ),
	SIMPLE_CMD( "execute", POS_STANDING, do_execute, NO_MIN, CTYPE_COMBAT ),
	SCMD_CMD( "exits", POS_RESTING, do_exits, NO_MIN, CTYPE_UTIL, -1 ),
	SCMD_CMD( "export", POS_DEAD, do_import, NO_MIN, CTYPE_UTIL, TRADE_EXPORT ),
	SIMPLE_CMD( "expel", POS_DEAD, do_expel, NO_MIN, CTYPE_EMPIRE ),
	STANDARD_CMD( "edelete", POS_DEAD, do_edelete, LVL_CIMPL, GRANT_EMPIRES, NO_SCMD, CTYPE_EMPIRE, CMD_NO_ABBREV, NO_ABIL ),

	SCMD_CMD( "fill", POS_STANDING, do_pour, NO_MIN, CTYPE_UTIL, SCMD_FILL ),
	SIMPLE_CMD( "factions", POS_DEAD, do_factions, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "familiar", POS_STANDING, do_familiar, NO_MIN, CTYPE_SKILL ),
	SCMD_CMD( "fastmorph", POS_RESTING, do_morph, NO_MIN, CTYPE_MOVE, SCMD_FASTMORPH ),
	SIMPLE_CMD( "feed", POS_STANDING, do_feed, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "fightmessages", POS_DEAD, do_fightmessages, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "fmessages", POS_DEAD, do_fightmessages, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "file", POS_DEAD, do_file, LVL_START_IMM, CTYPE_IMMORTAL ),
	STANDARD_CMD( "fillin", POS_STANDING, do_fillin, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "findmaintenance", POS_DEAD, do_findmaintenance, NO_MIN, CTYPE_EMPIRE ),
	STANDARD_CMD( "fire", POS_SITTING, do_fire, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_COMBAT, CMD_NO_ANIMALS, NO_ABIL ),
	ABILITY_CMD( "firstaid", POS_STANDING, do_firstaid, NO_MIN, CTYPE_SKILL, ABIL_FIRSTAID ),
	STANDARD_CMD( "fish", POS_SITTING, do_fish, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_SKILL, CMD_NO_ANIMALS, ABIL_FISH ),
	STANDARD_CMD( "flee", POS_FIGHTING, do_flee, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_COMBAT, CMD_NO_ABBREV, NO_ABIL ),
	ABILITY_CMD( "fly", POS_STANDING, do_fly, NO_MIN, CTYPE_SKILL, ABIL_FLY ),
	SIMPLE_CMD( "follow", POS_RESTING, do_follow, NO_MIN, CTYPE_MOVE ),
	ABILITY_CMD( "forage", POS_STANDING, do_forage, NO_MIN, CTYPE_SKILL, ABIL_FORAGE ),
	GRANT_CMD( "force", POS_SLEEPING, do_force, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_FORCE ),
	ABILITY_CMD( "foresight", POS_RESTING, do_foresight, NO_MIN, CTYPE_COMBAT, ABIL_FORESIGHT ),
	STANDARD_CMD( "forge", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_FORGE, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	GRANT_CMD( "forgive", POS_DEAD, do_forgive, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_FORGIVE ),
	SCMD_CMD( "fprompt", POS_DEAD, do_prompt, NO_MIN, CTYPE_UTIL, SCMD_FPROMPT ),
	SIMPLE_CMD( "fullsave", POS_DEAD, do_fullsave, LVL_TOP, CTYPE_IMMORTAL ),
	STANDARD_CMD( "freeze", POS_DEAD, do_wizutil, LVL_CIMPL, GRANT_FREEZE, SCMD_FREEZE, CTYPE_IMMORTAL, NOBITS, NO_ABIL ),

	SIMPLE_CMD( "get", POS_RESTING, do_get, NO_MIN, CTYPE_MOVE ),
	STANDARD_CMD( "gather", POS_STANDING, do_gather, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	GRANT_CMD( "gecho", POS_DEAD, do_gecho, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_GECHO ),
	SIMPLE_CMD( "give", POS_RESTING, do_give, NO_MIN, CTYPE_MOVE ),
	SCMD_CMD( "goto", POS_SLEEPING, do_goto, LVL_START_IMM, CTYPE_IMMORTAL, SCMD_GOTO ),
	SCMD_CMD( "godnet", POS_DEAD, do_pub_comm, LVL_GOD, CTYPE_IMMORTAL, SCMD_GODNET ),
	SCMD_CMD( "godhistory", POS_DEAD, do_history, LVL_GOD, CTYPE_COMM, CHANNEL_HISTORY_GOD ),
	SCMD_CMD( "ghistory", POS_DEAD, do_history, LVL_GOD, CTYPE_COMM, CHANNEL_HISTORY_GOD ),
	SCMD_CMD( "godlist", POS_DEAD, do_gen_ps, NO_MIN, CTYPE_UTIL, SCMD_GODLIST ),
	SIMPLE_CMD( "gold", POS_DEAD, do_coins, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "grab", POS_RESTING, do_grab, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "group", POS_DEAD, do_group, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "gsay", POS_DEAD, do_gsay, NO_MIN, CTYPE_COMM ),
	SIMPLE_CMD( "gtell", POS_DEAD, do_gsay, NO_MIN, CTYPE_COMM ),

	SIMPLE_CMD( "help", POS_DEAD, do_help, NO_MIN, CTYPE_UTIL ),
	SCMD_CMD( "handbook", POS_DEAD, do_gen_ps, LVL_START_IMM, CTYPE_IMMORTAL, SCMD_HANDBOOK ),
	STANDARD_CMD( "harness", POS_STANDING, do_harness, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "harvest", POS_STANDING, do_harvest, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	ABILITY_CMD( "hasten", POS_RESTING, do_hasten, NO_MIN, CTYPE_SKILL, ABIL_HASTEN ),
	SIMPLE_CMD( "heal", POS_FIGHTING, do_heal, NO_MIN, CTYPE_SKILL ),
	SIMPLE_CMD( "herd", POS_STANDING, do_herd, NO_MIN, CTYPE_MOVE ),
	ABILITY_CMD( "heartstop", POS_FIGHTING, do_heartstop, NO_MIN, CTYPE_COMBAT, ABIL_HEARTSTOP ),
	SIMPLE_CMD( "helpsearch", POS_DEAD, do_helpsearch, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "hide", POS_RESTING, do_hide, NO_MIN, CTYPE_MOVE, ABIL_HIDE ),
	SIMPLE_CMD( "hint", POS_DEAD, do_tip, NO_MIN, CTYPE_UTIL ),
	SCMD_CMD( "hit", POS_FIGHTING, do_hit, NO_MIN, CTYPE_COMBAT, SCMD_HIT ),
	SIMPLE_CMD( "hold", POS_RESTING, do_grab, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "home", POS_SLEEPING, do_home, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "hone", POS_STANDING, do_gen_augment, NO_MIN, NO_GRANTS, AUGMENT_HONE, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	GRANT_CMD( "hostile", POS_DEAD, do_hostile, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_HOSTILE ),
	ABILITY_CMD( "howl", POS_FIGHTING, do_howl, NO_MIN, CTYPE_SKILL, ABIL_HOWL ),

	SIMPLE_CMD( "inventory", POS_DEAD, do_inventory, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "identify", POS_RESTING, do_identify, NO_MIN, CTYPE_SKILL ),
	SCMD_CMD( "idea", POS_DEAD, do_gen_write, NO_MIN, CTYPE_COMM, SCMD_IDEA ),
	SIMPLE_CMD( "ignore", POS_DEAD, do_ignore, NO_MIN, CTYPE_UTIL ),
	SCMD_CMD( "import", POS_DEAD, do_import, NO_MIN, CTYPE_UTIL, TRADE_IMPORT ),
	SCMD_CMD( "imotd", POS_DEAD, do_gen_ps, LVL_START_IMM, CTYPE_IMMORTAL, SCMD_IMOTD ),
	ABILITY_CMD( "infiltrate", POS_STANDING, do_infiltrate, NO_MIN, CTYPE_MOVE, ABIL_INFILTRATE ),
	GRANT_CMD( "instance", POS_DEAD, do_instance, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_INSTANCE ),
	SCMD_CMD( "info", POS_DEAD, do_gen_ps, NO_MIN, CTYPE_UTIL, SCMD_INFO ),
	ABILITY_CMD( "inspire", POS_STANDING, do_inspire, NO_MIN, CTYPE_SKILL, ABIL_INSPIRE ),
	SIMPLE_CMD( "insult", POS_RESTING, do_insult, NO_MIN, CTYPE_COMM ),
	SIMPLE_CMD( "interlink", POS_STANDING, do_interlink, NO_MIN, CTYPE_BUILD ),
	SIMPLE_CMD( "invis", POS_DEAD, do_invis, LVL_GOD, CTYPE_IMMORTAL ),
	GRANT_CMD( "island", POS_DEAD, do_island, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_ISLAND ),
	SIMPLE_CMD( "islands", POS_DEAD, do_islands, NO_MIN, CTYPE_EMPIRE ),

	ABILITY_CMD( "jab", POS_FIGHTING, do_jab, NO_MIN, CTYPE_COMBAT, ABIL_JAB ),
	SCMD_CMD( "junk", POS_RESTING, do_drop, NO_MIN, CTYPE_UTIL, SCMD_JUNK ),

	SIMPLE_CMD( "kill", POS_FIGHTING, do_hit, NO_MIN, CTYPE_COMBAT ),
	SCMD_CMD( "keep", POS_DEAD, do_keep, NO_MIN, CTYPE_UTIL, SCMD_KEEP ),
	ABILITY_CMD( "kick", POS_FIGHTING, do_kick, NO_MIN, CTYPE_COMBAT, ABIL_KICK ),

	SCMD_CMD( "look", POS_RESTING, do_look, NO_MIN, CTYPE_UTIL, SCMD_LOOK ),
	STANDARD_CMD( "lay", POS_STANDING, do_lay, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "land", POS_FIGHTING, do_land, NO_MIN, CTYPE_MOVE ),
	SIMPLE_CMD( "last", POS_DEAD, do_last, LVL_START_IMM, CTYPE_IMMORTAL ),
	SIMPLE_CMD( "lead", POS_STANDING, do_lead, NO_MIN, CTYPE_MOVE ),
	SIMPLE_CMD( "light", POS_SITTING, do_light, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "lightningbolt", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_LIGHTNINGBOLT, CTYPE_COMBAT, NOBITS, ABIL_LIGHTNINGBOLT ),
	SCMD_CMD( "library", POS_STANDING, do_library, NO_MIN, CTYPE_UTIL, SCMD_LIBRARY ),
	GRANT_CMD( "load", POS_DEAD, do_load, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_LOAD ),
	STANDARD_CMD( "load", POS_STANDING, do_load_vehicle, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "loadvehicle", POS_STANDING, do_load_vehicle, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_NO_ANIMALS, NO_ABIL ),

	ABILITY_CMD( "mount", POS_STANDING, do_mount, NO_MIN, CTYPE_MOVE, ABIL_RIDE ),
	STANDARD_CMD( "maintain", POS_STANDING, do_maintain, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	ABILITY_CMD( "majesty", POS_RESTING, do_majesty, NO_MIN, CTYPE_SKILL, ABIL_MAJESTY ),
	ABILITY_CMD( "manashield", POS_RESTING, do_manashield, NO_MIN, CTYPE_COMBAT, ABIL_MANASHIELD ),
	STANDARD_CMD( "manufacture", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_MANUFACTURE, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "mapsize", POS_DEAD, do_mapsize, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "mapout", POS_DEAD, do_mapout, LVL_CIMPL, CTYPE_IMMORTAL ),
	SIMPLE_CMD( "mark", POS_RESTING, do_mark, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "meters", POS_DEAD, do_meters, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "melt", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_SMELT, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "mine", POS_STANDING, do_mine, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "mill", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_MILL, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "milk", POS_STANDING, do_milk, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "mint", POS_STANDING, do_mint, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	ABILITY_CMD( "mirrorimage", POS_FIGHTING, do_mirrorimage, NO_MIN, CTYPE_COMBAT, ABIL_MIRRORIMAGE ),
	SIMPLE_CMD( "missinghelp", POS_DEAD, do_missing_help_files, LVL_START_IMM, CTYPE_IMMORTAL ),
	STANDARD_CMD( "mix", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_MIX, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	ABILITY_CMD( "moonrise", POS_FIGHTING, do_moonrise, NO_MIN, CTYPE_COMBAT, ABIL_MOONRISE ),
	SCMD_CMD( "morph", POS_FIGHTING, do_morph, NO_MIN, CTYPE_MOVE, SCMD_MORPH ),
	SCMD_CMD( "motd", POS_DEAD, do_gen_ps, NO_MIN, CTYPE_UTIL, SCMD_MOTD ),
	GRANT_CMD( "moveeinv", POS_DEAD, do_moveeinv, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_EMPIRES ),
	SIMPLE_CMD( "mudstats", POS_DEAD, do_mudstats, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "mummify", POS_STUNNED, do_mummify, NO_MIN, CTYPE_MOVE, ABIL_MUMMIFY ),
	SIMPLE_CMD( "mail", POS_STANDING, do_mail, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "mute", POS_DEAD, do_wizutil, LVL_CIMPL, GRANT_MUTE, SCMD_MUTE, CTYPE_IMMORTAL, NOBITS, NO_ABIL ),
	SIMPLE_CMD( "mydescription", POS_STANDING, do_mydescription, NO_MIN, CTYPE_UTIL ),

	SIMPLE_CMD( "nearby", POS_RESTING, do_nearby, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "nightsight", POS_RESTING, do_nightsight, NO_MIN, CTYPE_SKILL, ABIL_NIGHTSIGHT ),
	SIMPLE_CMD( "nodismantle", POS_SLEEPING, do_nodismantle, NO_MIN, CTYPE_BUILD ),
	SIMPLE_CMD( "noskill", POS_DEAD, do_noskill, NO_MIN, CTYPE_UTIL ),
	SCMD_CMD( "notitle", POS_DEAD, do_wizutil, LVL_CIMPL, CTYPE_IMMORTAL, SCMD_NOTITLE ),

	SIMPLE_CMD( "order", POS_RESTING, do_order, NO_MIN, CTYPE_COMM ),
	GRANT_CMD( "oset", POS_DEAD, do_oset, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_OSET ),
	SCMD_CMD( "open", POS_SITTING, do_gen_door, NO_MIN, CTYPE_MOVE, SCMD_OPEN ),
	SCMD_CMD( "oocsay", POS_RESTING, do_say, NO_MIN, CTYPE_COMM, SCMD_OOCSAY ),
	SCMD_CMD( "osay", POS_RESTING, do_say, NO_MIN, CTYPE_COMM, SCMD_OOCSAY ),
	SCMD_CMD( "\"", POS_RESTING, do_say, NO_MIN, CTYPE_COMM, SCMD_OOCSAY ),
	ABILITY_CMD( "outrage", POS_FIGHTING, do_outrage, NO_MIN, CTYPE_COMBAT, ABIL_OUTRAGE ),
	GRANT_CMD( "olc", POS_DEAD, do_olc, LVL_BUILDER, CTYPE_OLC, GRANT_OLC ),

	SIMPLE_CMD( "put", POS_RESTING, do_put, NO_MIN, CTYPE_MOVE ),
	GRANT_CMD( "page", POS_DEAD, do_page, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_PAGE ),
	STANDARD_CMD( "pan", POS_STANDING, do_pan, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "party", POS_DEAD, do_group, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "pick", POS_STANDING, do_pick, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "pickpocket", POS_STANDING, do_pickpocket, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_COMBAT, CMD_STAY_HIDDEN, ABIL_PICKPOCKET ),
	STANDARD_CMD( "pilot", POS_SITTING, do_drive, NO_MIN, NO_GRANTS, SCMD_PILOT, CTYPE_MOVE, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "play", POS_STANDING, do_play, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "playerdelete", POS_SLEEPING, do_playerdelete, LVL_CIMPL, GRANT_PLAYERDELETE, NO_SCMD, CTYPE_IMMORTAL, CMD_NO_ABBREV, NO_ABIL ),
	STANDARD_CMD( "plant", POS_STANDING, do_plant, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "pledge", POS_SLEEPING, do_pledge, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "point", POS_RESTING, do_point, NO_MIN, CTYPE_UTIL ),
	SCMD_CMD( "policy", POS_DEAD, do_gen_ps, NO_MIN, CTYPE_UTIL, SCMD_POLICIES ),
	SCMD_CMD( "poofin", POS_DEAD, do_poofset, LVL_GOD, CTYPE_IMMORTAL, SCMD_POOFIN ),
	SCMD_CMD( "poofout", POS_DEAD, do_poofset, LVL_GOD, CTYPE_IMMORTAL, SCMD_POOFOUT ),
	SIMPLE_CMD( "portal", POS_STANDING, do_portal, NO_MIN, CTYPE_MOVE ),
	SCMD_CMD( "pour", POS_STANDING, do_pour, NO_MIN, CTYPE_UTIL, SCMD_POUR ),
	STANDARD_CMD( "press", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_PRESS, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	ABILITY_CMD( "prick", POS_FIGHTING, do_prick, NO_MIN, CTYPE_COMBAT, ABIL_PRICK ),
	SIMPLE_CMD( "promote", POS_DEAD, do_promote, NO_MIN, CTYPE_EMPIRE ),
	SCMD_CMD( "prompt", POS_DEAD, do_prompt, NO_MIN, CTYPE_UTIL, SCMD_PROMPT ),
	STANDARD_CMD( "prospect", POS_STANDING, do_prospect, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_NO_ANIMALS, ABIL_PROSPECT ),
	SIMPLE_CMD( "publicize", POS_RESTING, do_publicize, NO_MIN, CTYPE_EMPIRE ),
	GRANT_CMD( "purge", POS_DEAD, do_purge, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_PURGE ),
	ABILITY_CMD( "purify", POS_STANDING, do_purify, NO_MIN, CTYPE_SKILL, ABIL_PURIFY ),
	SIMPLE_CMD( "psay", POS_DEAD, do_gsay, NO_MIN, CTYPE_COMM ),
	SIMPLE_CMD( "ptell", POS_DEAD, do_gsay, NO_MIN, CTYPE_COMM ),

	SIMPLE_CMD( "quests", POS_DEAD, do_quest, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "quaff", POS_RESTING, do_quaff, NO_MIN, CTYPE_MOVE ),
	STANDARD_CMD( "quarry", POS_STANDING, do_quarry, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "quit", POS_DEAD, do_quit, NO_MIN, NO_GRANTS, SCMD_QUIT, CTYPE_UTIL, CMD_NO_ABBREV, NO_ABIL ),

	SIMPLE_CMD( "reply", POS_DEAD, do_reply, NO_MIN, CTYPE_COMM ),
	ABILITY_CMD( "radiance", POS_STANDING, do_radiance, NO_MIN, CTYPE_SKILL, ABIL_RADIANCE ),
	SIMPLE_CMD( "random", POS_SLEEPING, do_random, LVL_START_IMM, CTYPE_IMMORTAL ),
	SIMPLE_CMD( "read", POS_RESTING, do_read, NO_MIN, CTYPE_COMM ),
	SIMPLE_CMD( "ready", POS_FIGHTING, do_ready, NO_MIN, CTYPE_COMBAT ),
	STANDARD_CMD( "reboot", POS_DEAD, do_reboot, LVL_CIMPL, GRANT_REBOOT, SCMD_REBOOT, CTYPE_IMMORTAL, CMD_NO_ABBREV, NO_ABIL ),
	SIMPLE_CMD( "recipes", POS_DEAD, do_recipes, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "reclaim", POS_STANDING, do_reclaim, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "recolor", POS_DEAD, do_recolor, 0, CTYPE_UTIL ),
	SCMD_CMD( "redesignate", POS_STANDING, do_designate, NO_MIN, CTYPE_BUILD, SCMD_REDESIGNATE ),
	STANDARD_CMD( "refashion", POS_STANDING, do_reforge, NO_MIN, NO_GRANTS, SCMD_REFASHION, CTYPE_SKILL, NOBITS, ABIL_REFASHION ),
	STANDARD_CMD( "reforge", POS_STANDING, do_reforge, NO_MIN, NO_GRANTS, SCMD_REFORGE, CTYPE_SKILL, NOBITS, ABIL_REFORGE ),
	ABILITY_CMD( "regenerate", POS_MORTALLYW, do_regenerate, NO_MIN, CTYPE_COMBAT, ABIL_REGENERATE ),
	ABILITY_CMD( "rejuvenate", POS_FIGHTING, do_rejuvenate, NO_MIN, CTYPE_SKILL, ABIL_REJUVENATE ),
	SCMD_CMD( "reject", POS_DEAD, do_accept, NO_MIN, CTYPE_UTIL, SCMD_REJECT ),
	GRANT_CMD( "reload", POS_DEAD, do_reload, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_RELOAD ),
	SIMPLE_CMD( "remove", POS_RESTING, do_remove, NO_MIN, CTYPE_COMM ),
	STANDARD_CMD( "repair", POS_STANDING, do_repair, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	GRANT_CMD( "rescale", POS_RESTING, do_rescale, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_RESCALE ),
	ABILITY_CMD( "rescue", POS_FIGHTING, do_rescue, NO_MIN, CTYPE_COMBAT, ABIL_RESCUE ),
	SIMPLE_CMD( "respawn", POS_DEAD, do_respawn, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "respond", POS_RESTING, do_respond, NO_MIN, CTYPE_COMM ),
	SIMPLE_CMD( "rest", POS_RESTING, do_rest, NO_MIN, CTYPE_MOVE ),
	GRANT_CMD( "restore", POS_DEAD, do_restore, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_RESTORE ),
	ABILITY_CMD( "resurrect", POS_STANDING, do_resurrect, NO_MIN, CTYPE_SKILL, ABIL_RESURRECT ),
	SIMPLE_CMD( "retrieve", POS_STANDING, do_retrieve, NO_MIN, CTYPE_MOVE ),
	SIMPLE_CMD( "return", POS_DEAD, do_return, NO_MIN, CTYPE_IMMORTAL ),
	ABILITY_CMD( "reward", POS_RESTING, do_reward, NO_MIN, CTYPE_SKILL, ABIL_REWARD ),
	ABILITY_CMD( "ride", POS_STANDING, do_mount, NO_MIN, CTYPE_MOVE, ABIL_RIDE ),
	STANDARD_CMD( "rite", POS_STANDING, do_ritual, NO_MIN, NO_GRANTS, SCMD_RITUAL, CTYPE_SKILL, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "ritual", POS_STANDING, do_ritual, NO_MIN, NO_GRANTS, SCMD_RITUAL, CTYPE_SKILL, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "roadsign", POS_STANDING, do_roadsign, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, ABIL_ROADS ),
	SIMPLE_CMD( "roll", POS_RESTING, do_roll, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "roster", POS_DEAD, do_roster, NO_MIN, CTYPE_EMPIRE ),

	SCMD_CMD( "say", POS_RESTING, do_say, NO_MIN, CTYPE_COMM, SCMD_SAY ),
	SCMD_CMD( "'", POS_RESTING, do_say, NO_MIN, CTYPE_COMM, SCMD_SAY ),
	SIMPLE_CMD( "sacrifice", POS_STANDING, do_sacrifice, NO_MIN, CTYPE_MOVE ),
	STANDARD_CMD( "sail", POS_SITTING, do_drive, NO_MIN, NO_GRANTS, SCMD_SAIL, CTYPE_MOVE, CMD_NO_ANIMALS, NO_ABIL ),
	ABILITY_CMD( "sap", POS_STANDING, do_sap, NO_MIN, CTYPE_COMBAT, ABIL_SAP ),
	SIMPLE_CMD( "save", POS_STUNNED, do_save, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "saw", POS_STANDING, do_saw, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SCMD_CMD( "sayhistory", POS_DEAD, do_history, NO_MIN, CTYPE_COMM, CHANNEL_HISTORY_SAY ),
	SIMPLE_CMD( "score", POS_DEAD, do_score, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "scan", POS_RESTING, do_scan, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "scour", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_SCOUR, CTYPE_COMBAT, NOBITS, ABIL_SCOUR ),
	STANDARD_CMD( "scrap", POS_STANDING, do_scrap, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ABBREV, NO_ABIL ),
	STANDARD_CMD( "scrape", POS_STANDING, do_scrape, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	ABILITY_CMD( "search", POS_STANDING, do_search, NO_MIN, CTYPE_COMBAT, ABIL_SEARCH ),
	STANDARD_CMD( "selfdelete", POS_SLEEPING, do_selfdelete, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_UTIL, CMD_NO_ABBREV, NO_ABIL ),
	GRANT_CMD( "send", POS_SLEEPING, do_send, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_SEND ),
	SIMPLE_CMD( "separate", POS_RESTING, do_separate, NO_MIN, CTYPE_UTIL ),
	GRANT_CMD( "set", POS_DEAD, do_set, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_SET ),
	STANDARD_CMD( "sew", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_SEW, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "share", POS_RESTING, do_share, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "shadowcage", POS_FIGHTING, do_shadowcage, NO_MIN, CTYPE_SKILL, ABIL_SHADOWCAGE ),
	STANDARD_CMD( "shadowlash", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_SHADOWLASH, CTYPE_COMBAT, NOBITS, ABIL_SHADOWLASH ),
	ABILITY_CMD( "shadowstep", POS_STANDING, do_shadowstep, NO_MIN, CTYPE_MOVE, ABIL_SHADOWSTEP ),
	STANDARD_CMD( "shear", POS_STANDING, do_shear, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "sheathe", POS_RESTING, do_sheathe, NO_MIN, CTYPE_COMBAT ),
	SIMPLE_CMD( "ship", POS_RESTING, do_ship, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "shoot", POS_STANDING, do_shoot, NO_MIN, CTYPE_COMBAT ),
	SCMD_CMD( "shout", POS_RESTING, do_pub_comm, NO_MIN, CTYPE_COMM, SCMD_SHOUT ),
	SIMPLE_CMD( "show", POS_DEAD, do_show, LVL_START_IMM, CTYPE_IMMORTAL ),
	STANDARD_CMD( "shutdown", POS_DEAD, do_reboot, LVL_CIMPL, GRANT_SHUTDOWN, SCMD_SHUTDOWN, CTYPE_IMMORTAL, CMD_NO_ABBREV, NO_ABIL ),
	SCMD_CMD( "sip", POS_RESTING, do_drink, NO_MIN, CTYPE_UTIL, SCMD_SIP ),
	ABILITY_CMD( "siphon", POS_FIGHTING, do_siphon, NO_MIN, CTYPE_COMBAT, ABIL_SIPHON ),
	SIMPLE_CMD( "sire", POS_STANDING, do_sire, NO_MIN, CTYPE_COMBAT ),
	SIMPLE_CMD( "sit", POS_RESTING, do_sit, NO_MIN, CTYPE_MOVE ),
	SIMPLE_CMD( "skills", POS_DEAD, do_skills, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "skin", POS_STANDING, do_skin, NO_MIN, CTYPE_SKILL ),
	ABILITY_CMD( "skybrand", POS_FIGHTING, do_skybrand, NO_MIN, CTYPE_COMBAT, ABIL_SKYBRAND ),
	SIMPLE_CMD( "sleep", POS_SLEEPING, do_sleep, NO_MIN, CTYPE_MOVE ),
	GRANT_CMD( "slay", POS_RESTING, do_slay, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_SLAY ),
	ABILITY_CMD( "slow", POS_FIGHTING, do_slow, NO_MIN, CTYPE_COMBAT, ABIL_SLOW ),
	STANDARD_CMD( "smelt", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_SMELT, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "sneak", POS_STANDING, do_sneak, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_STAY_HIDDEN, ABIL_SNEAK ),
	GRANT_CMD( "snoop", POS_DEAD, do_snoop, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_SNOOP ),
	SIMPLE_CMD( "socials", POS_DEAD, do_socials, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "soulchain", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_SOULCHAIN, CTYPE_COMBAT, NOBITS, ABIL_SOULCHAIN ),
	ABILITY_CMD( "soulmask", POS_RESTING, do_soulmask, NO_MIN, CTYPE_SKILL, ABIL_SOULMASK ),
	ABILITY_CMD( "soulsight", POS_RESTING, do_soulsight, NO_MIN, CTYPE_SKILL, ABIL_SOULSIGHT ),
	SIMPLE_CMD( "specialize", POS_STANDING, do_specialize, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "split", POS_RESTING, do_split, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "stand", POS_RESTING, do_stand, NO_MIN, CTYPE_MOVE ),
	STANDARD_CMD( "starstrike", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_STARSTRIKE, CTYPE_COMBAT, NOBITS, ABIL_STARSTRIKE ),
	SCMD_CMD( "stake", POS_FIGHTING, do_stake, NO_MIN, CTYPE_COMBAT, FALSE ),
	SIMPLE_CMD( "stat", POS_DEAD, do_stat, LVL_START_IMM, CTYPE_IMMORTAL ),
	ABILITY_CMD( "steal", POS_STANDING, do_steal, NO_MIN, CTYPE_COMBAT, ABIL_STEAL ),
	SIMPLE_CMD( "store", POS_STANDING, do_store, NO_MIN, CTYPE_MOVE ),
	SIMPLE_CMD( "stop", POS_DEAD, do_stop, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "struggle", POS_STUNNED, do_struggle, NO_MIN, CTYPE_COMBAT ),
	SIMPLE_CMD( "summary", POS_DEAD, do_summary, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "summon", POS_STANDING, do_summon, NO_MIN, CTYPE_SKILL ),
	STANDARD_CMD( "sunshock", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_SUNSHOCK, CTYPE_COMBAT, NOBITS, ABIL_SUNSHOCK ),
	SIMPLE_CMD( "survey", POS_STANDING, do_survey, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "swap", POS_RESTING, do_swap, NO_MIN, CTYPE_UTIL ),
	GRANT_CMD( "switch", POS_DEAD, do_switch, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_SWITCH ),
	SIMPLE_CMD( "syslog", POS_DEAD, do_syslog, LVL_START_IMM, CTYPE_IMMORTAL ),

	SIMPLE_CMD( "tell", POS_DEAD, do_tell, NO_MIN, CTYPE_COMM ),
	SIMPLE_CMD( "take", POS_RESTING, do_get, NO_MIN, CTYPE_MOVE ),
	STANDARD_CMD( "tan", POS_STANDING, do_tan, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_EMPIRE, CMD_NO_ANIMALS, NO_ABIL ),
	SCMD_CMD( "taste", POS_RESTING, do_eat, NO_MIN, CTYPE_UTIL, SCMD_TASTE ),
	STANDARD_CMD( "tavern", POS_STANDING, do_tavern, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_EMPIRE, CMD_NO_ANIMALS, NO_ABIL ),
	GRANT_CMD( "tedit", POS_DEAD, do_tedit, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_TEDIT ),
	SCMD_CMD( "teleport", POS_STANDING, do_goto, LVL_GOD, CTYPE_IMMORTAL, SCMD_TELEPORT ),
	SCMD_CMD( "tellhistory", POS_DEAD, do_history, NO_MIN, CTYPE_COMM, CHANNEL_HISTORY_TELLS ),
	ABILITY_CMD( "terrify", POS_FIGHTING, do_terrify, NO_MIN, CTYPE_COMBAT, ABIL_TERRIFY ),
	SIMPLE_CMD( "territory", POS_DEAD, do_territory, NO_MIN, CTYPE_EMPIRE ),
	STANDARD_CMD( "thornlash", POS_FIGHTING, do_damage_spell, NO_MIN, NO_GRANTS, ABIL_THORNLASH, CTYPE_COMBAT, NOBITS, ABIL_THORNLASH ),
	STANDARD_CMD( "throw", POS_FIGHTING, do_throw, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_COMBAT, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "thaw", POS_DEAD, do_wizutil, LVL_CIMPL, GRANT_FREEZE, SCMD_THAW, CTYPE_IMMORTAL, CMD_NO_ANIMALS, NO_ABIL ),
	SCMD_CMD( "tie", POS_STANDING, do_tie, NO_MIN, CTYPE_COMBAT, FALSE ),
	SIMPLE_CMD( "time", POS_DEAD, do_time, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "tips", POS_DEAD, do_tip, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "title", POS_DEAD, do_title, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "toggles", POS_DEAD, do_toggle, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "tomb", POS_DEAD, do_tomb, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "track", POS_STANDING, do_track, NO_MIN, CTYPE_SKILL, ABIL_TRACK ),
	SIMPLE_CMD( "trade", POS_RESTING, do_trade, NO_MIN, CTYPE_MOVE ),
	GRANT_CMD( "transfer", POS_SLEEPING, do_trans, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_TRANSFER ),
	SIMPLE_CMD( "transport", POS_STANDING, do_transport, NO_MIN, CTYPE_MOVE ),
	STANDARD_CMD( "tunnel", POS_STANDING, do_tunnel, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_BUILD, CMD_NO_ANIMALS, ABIL_TUNNEL ),
	SCMD_CMD( "typo", POS_DEAD, do_gen_write, NO_MIN, CTYPE_COMM, SCMD_TYPO ),

	STANDARD_CMD( "unapprove", POS_DEAD, do_approve, LVL_CIMPL, GRANT_APPROVE, SCMD_UNAPPROVE, CTYPE_IMMORTAL, NOBITS, NO_ABIL ),
	GRANT_CMD( "unbind", POS_SLEEPING, do_unbind, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_UNBIND ),
	STANDARD_CMD( "unharness", POS_STANDING, do_unharness, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_NO_ANIMALS, NO_ABIL ),
	SCMD_CMD( "unkeep", POS_DEAD, do_keep, NO_MIN, CTYPE_UTIL, SCMD_UNKEEP ),
	STANDARD_CMD( "unload", POS_STANDING, do_unload_vehicle, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "unloadvehicle", POS_STANDING, do_unload_vehicle, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_NO_ANIMALS, NO_ABIL ),
	GRANT_CMD( "unquest", POS_SLEEPING, do_unquest, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_UNQUEST ),
	SCMD_CMD( "unstake", POS_STANDING, do_stake, NO_MIN, CTYPE_COMBAT, TRUE ),
	STANDARD_CMD( "untie", POS_STANDING, do_tie, NO_MIN, NO_GRANTS, TRUE, CTYPE_COMBAT, CMD_NO_ANIMALS, NO_ABIL ),
	GRANT_CMD( "unban", POS_DEAD, do_unban, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_BAN ),
	SIMPLE_CMD( "unpublicize", POS_DEAD, do_unpublicize, NO_MIN, CTYPE_EMPIRE ),
	SIMPLE_CMD( "unshare", POS_RESTING, do_unshare, NO_MIN, CTYPE_UTIL ),
	SCMD_CMD( "uptime", POS_DEAD, do_date, LVL_START_IMM, CTYPE_IMMORTAL, SCMD_UPTIME ),
	SIMPLE_CMD( "upgrade", POS_STANDING, do_upgrade, NO_MIN, CTYPE_BUILD ),
	SIMPLE_CMD( "use", POS_RESTING, do_use, NO_MIN, CTYPE_MOVE ),
	GRANT_CMD( "users", POS_DEAD, do_users, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_USERS ),
	SIMPLE_CMD( "utility", POS_DEAD, do_admin_util, LVL_START_IMM, CTYPE_IMMORTAL ),

	ABILITY_CMD( "veintap", POS_STANDING, do_veintap, NO_MIN, CTYPE_SKILL, ABIL_VEINTAP ),
	SCMD_CMD( "version", POS_DEAD, do_gen_ps, NO_MIN, CTYPE_UTIL, SCMD_VERSION ),
	ABILITY_CMD( "vigor", POS_FIGHTING, do_vigor, NO_MIN, CTYPE_SKILL, ABIL_VIGOR ),
	SIMPLE_CMD( "visible", POS_RESTING, do_visible, NO_MIN, CTYPE_UTIL ),
	GRANT_CMD( "vnum", POS_DEAD, do_vnum, LVL_START_IMM, CTYPE_IMMORTAL, GRANT_OLC ),
	GRANT_CMD( "vstat", POS_DEAD, do_vstat, LVL_START_IMM, CTYPE_IMMORTAL, GRANT_OLC ),

	SIMPLE_CMD( "wake", POS_SLEEPING, do_wake, NO_MIN, CTYPE_MOVE ),
	SIMPLE_CMD( "warehouse", POS_DEAD, do_warehouse, NO_MIN, CTYPE_MOVE ),
	SIMPLE_CMD( "wear", POS_RESTING, do_wear, NO_MIN, CTYPE_UTIL ),
	ABILITY_CMD( "weaken", POS_FIGHTING, do_weaken, NO_MIN, CTYPE_COMBAT, ABIL_WEAKEN ),
	SIMPLE_CMD( "weather", POS_RESTING, do_weather, NO_MIN, CTYPE_UTIL ),
	STANDARD_CMD( "weave", POS_DEAD, do_gen_craft, NO_MIN, NO_GRANTS, CRAFT_TYPE_WEAVE, CTYPE_BUILD, CMD_NO_ANIMALS, NO_ABIL ),
	STANDARD_CMD( "who", POS_DEAD, do_who, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_COMM, CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "whois", POS_DEAD, do_whois, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_COMM, CMD_STAY_HIDDEN, NO_ABIL ),
	SIMPLE_CMD( "where", POS_RESTING, do_where, NO_MIN, CTYPE_COMM ),
	SIMPLE_CMD( "whereami", POS_RESTING, do_whereami, NO_MIN, CTYPE_COMM ),
	SCMD_CMD( "whisper", POS_RESTING, do_spec_comm, NO_MIN, CTYPE_COMM, SCMD_WHISPER ),
	ABILITY_CMD( "whisperstride", POS_STANDING, do_whisperstride, NO_MIN, CTYPE_SKILL, ABIL_WHISPERSTRIDE ),
	SIMPLE_CMD( "wield", POS_RESTING, do_wield, NO_MIN, CTYPE_UTIL ),
	SIMPLE_CMD( "withdraw", POS_STANDING, do_withdraw, NO_MIN, CTYPE_EMPIRE ),
	SCMD_CMD( "wiznet", POS_DEAD, do_pub_comm, LVL_START_IMM, CTYPE_IMMORTAL, SCMD_WIZNET ),
	SCMD_CMD( ";", POS_DEAD, do_pub_comm, LVL_START_IMM, CTYPE_IMMORTAL, SCMD_WIZNET ),
	SCMD_CMD( "wizhelp", POS_DEAD, do_commands, LVL_GOD, CTYPE_IMMORTAL, SCMD_WIZHELP ),
	SCMD_CMD( "wizlist", POS_DEAD, do_gen_ps, NO_MIN, CTYPE_UTIL, SCMD_WIZLIST ),
	GRANT_CMD( "wizlock", POS_DEAD, do_wizlock, LVL_CIMPL, CTYPE_IMMORTAL, GRANT_WIZLOCK ),
	SIMPLE_CMD( "workforce", POS_DEAD, do_workforce, NO_MIN, CTYPE_EMPIRE ),
	ABILITY_CMD( "worm", POS_STUNNED, do_worm, NO_MIN, CTYPE_MOVE, ABIL_WORM ),
	SIMPLE_CMD( "write", POS_STANDING, do_write, NO_MIN, CTYPE_COMM ),
	
	{ ",", POS_DEAD, do_string_editor, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_UTIL, NOBITS, NO_ABIL },
	
	/* DG trigger commands */
	ABILITY_CMD( "tattach", POS_DEAD, do_tattach, NO_MIN, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY ),
	ABILITY_CMD( "tdetach", POS_DEAD, do_tdetach, NO_MIN, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY ),
	SIMPLE_CMD( "vdelete", POS_DEAD, do_vdelete, LVL_CIMPL, CTYPE_IMMORTAL ),
	STANDARD_CMD( "madventurecomplete", POS_DEAD, do_madventurecomplete, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "maggro", POS_RESTING, do_maggro, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "masound", POS_DEAD, do_masound, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mbuild", POS_DEAD, do_mbuild, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mkill", POS_FIGHTING, do_mkill, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mjunk", POS_DEAD, do_mjunk, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mdamage", POS_DEAD, do_mdamage, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "maoe", POS_DEAD, do_maoe, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mdot", POS_DEAD, do_mdot, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mdoor", POS_DEAD, do_mdoor, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mecho", POS_DEAD, do_mecho, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mechoaround", POS_DEAD, do_mechoaround, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mechoneither", POS_DEAD, do_mechoneither, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "msend", POS_DEAD, do_msend, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mload", POS_DEAD, do_mload, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mmorph", POS_DEAD, do_mmorph, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mmove", POS_STANDING, do_mmove, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_MOVE, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mpurge", POS_DEAD, do_mpurge, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mquest", POS_DEAD, do_mquest, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mgoto", POS_DEAD, do_mgoto, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mat", POS_DEAD, do_mat, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mown", POS_DEAD, do_mown, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mrestore", POS_DEAD, do_mrestore, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mscale", POS_DEAD, do_mscale, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "msiege", POS_DEAD, do_msiege, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mteleport", POS_DEAD, do_mteleport, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mterracrop", POS_DEAD, do_mterracrop, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mterraform", POS_DEAD, do_mterraform, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mforce", POS_DEAD, do_mforce, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mhunt", POS_DEAD, do_mhunt, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mremember", POS_DEAD, do_mremember, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mforget", POS_DEAD, do_mforget, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mtransform", POS_DEAD, do_mtransform, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mbuildingecho", POS_DEAD, do_mbuildingecho, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mvehicleecho", POS_DEAD, do_mvehicleecho, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mregionecho", POS_DEAD, do_mregionecho, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),
	STANDARD_CMD( "mfollow", POS_DEAD, do_mfollow, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_IMMORTAL, CMD_IMM_OR_MOB_ONLY | CMD_STAY_HIDDEN, NO_ABIL ),

	/* this must be last */
	STANDARD_CMD( "\n", POS_DEAD, NULL, NO_MIN, NO_GRANTS, NO_SCMD, CTYPE_UTIL, NOBITS, NO_ABIL )
};


 //////////////////////////////////////////////////////////////////////////////
//// COMMAND INTERPRETER /////////////////////////////////////////////////////

/*
 * This is the actual command interpreter called from game_loop() in comm.c
 * It makes sure you are the proper level and position to execute the command,
 * then calls the appropriate function.
 */
void command_interpreter(char_data *ch, char *argument) {
	extern bool check_social(char_data *ch, char *string, bool exact);
	int cmd, length, iter;
	char *line;

	/* just drop to next line for hitting CR */
	skip_spaces(&argument);
	if (!*argument)
		return;

	/*
	 * special case to handle one-character, non-alphanumeric commands;
	 * requested by many people so "'hi" or ";godnet test" is possible.
	 * Patch sent by Eric Green and Stefan Wasilewski.
	 */
	if (!isalpha(*argument)) {
		arg[0] = argument[0];
		arg[1] = '\0';
		line = argument + 1;
	}
	else {
		line = any_one_arg(argument, arg);
	}
	
	// lowercase arg to speed up command comparisons
	for (iter = 0; iter < strlen(arg); ++iter) {
		arg[iter] = LOWER(arg[iter]);
	}

	// Command trigger (1/3): exact match on typed-in word
	if (check_command_trigger(ch, arg, line, CMDTRG_EXACT)) {
		return;
	}

	/* otherwise, find the command */
	for (length = strlen(arg), cmd = 0; *cmd_info[cmd].command != '\n'; cmd++) {
		if (GET_ACCESS_LEVEL(ch) < cmd_info[cmd].minimum_level && (cmd_info[cmd].grants == NO_GRANTS || !IS_GRANTED(ch, cmd_info[cmd].grants))) {
			continue;
		}
		if (IS_SET(cmd_info[cmd].flags, CMD_NO_ABBREV) ? strcmp(arg, cmd_info[cmd].command) : strncmp(cmd_info[cmd].command, arg, length)) {
			continue;
		}		
		if (IS_SET(cmd_info[cmd].flags, CMD_VAMPIRE_ONLY) && !IS_VAMPIRE(ch)) {
			continue;
		}
		if (IS_SET(cmd_info[cmd].flags, CMD_IMM_OR_MOB_ONLY) && GET_ACCESS_LEVEL(ch) < LVL_START_IMM && !IS_NPC(ch)) {
			continue;
		}
		// NPCs can use ability commands IF they aren't charmed; players require the ability
		if (cmd_info[cmd].ability != NO_ABIL && (IS_NPC(ch) ? AFF_FLAGGED(ch, AFF_CHARM) : !has_ability(ch, cmd_info[cmd].ability))) {
			continue;
		}
		
		// found!
		break;
	}

	if (!IS_SET(cmd_info[cmd].flags, CMD_STAY_HIDDEN | CMD_UNHIDE_AFTER))
		REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE);

	if (*cmd_info[cmd].command == '\n' && check_social(ch, argument, FALSE))
		return;
	else if (strlen(arg) < strlen(cmd_info[cmd].command) && check_social(ch, argument, TRUE)) {
		// If the player abbreviated the actual command ("nod" for
		// nodismantle), and what they typed is an exact match for a social,
		// do the social instead.
		return;
	}
	else if (*cmd_info[cmd].command == '\n') {
		// Command trigger (2/3): abbrev match on non-matching command
		if (check_command_trigger(ch, arg, line, CMDTRG_ABBREV)) {
			return;
		}
		// otherwise, no match
		send_config_msg(ch, "huh_string");
	}
	else if (!IS_NPC(ch) && ACCOUNT_FLAGGED(ch, ACCT_FROZEN))
		send_to_char("You try, but the mind-numbing cold prevents you...\r\n", ch);
	else if (IS_SET(cmd_info[cmd].flags, CMD_NOT_RP) && !IS_NPC(ch) && !IS_GOD(ch) && !IS_IMMORTAL(ch) && PRF_FLAGGED(ch, PRF_RP)) {
		msg_to_char(ch, "You can't do that while role-playing!\r\n");
	}
	else if (IS_SET(cmd_info[cmd].flags, CMD_NO_ANIMALS) && CHAR_MORPH_FLAGGED(ch, MORPHF_ANIMAL)) {
		msg_to_char(ch, "You can't do that in this form!\r\n");
	}
	else if (IS_INJURED(ch, INJ_STAKED) && cmd_info[cmd].minimum_position >= POS_SLEEPING && !IS_IMMORTAL(ch))
		msg_to_char(ch, "You can't do that while staked!\r\n");
	else if (AFF_FLAGGED(ch, AFF_STUNNED) && cmd_info[cmd].minimum_position >= POS_SLEEPING && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You can't do that while stunned!\r\n");
	}
	else if (AFF_FLAGGED(ch, AFF_EARTHMELD) && cmd_info[cmd].minimum_position >= POS_SLEEPING)
		msg_to_char(ch, "You can't do that while in earthmeld.\r\n");
	else if (AFF_FLAGGED(ch, AFF_MUMMIFY) && cmd_info[cmd].minimum_position >= POS_SLEEPING)
		msg_to_char(ch, "You can't do that while mummified.\r\n");
	else if (AFF_FLAGGED(ch, AFF_DEATHSHROUD) && cmd_info[cmd].minimum_position >= POS_SLEEPING)
		msg_to_char(ch, "You can't do that while in deathshroud!\r\n");
	else if (GET_FED_ON_BY(ch) && cmd_info[cmd].minimum_position >= POS_SLEEPING)
		msg_to_char(ch, "The ecstasy of the fangs in your flesh is too enchanting to do that...\r\n");
	else if (GET_FEEDING_FROM(ch) && cmd_info[cmd].minimum_position >= POS_SLEEPING && cmd_info[cmd].command_pointer != do_bite)
		msg_to_char(ch, "You can't do that while feeding!\r\n");
	else if (cmd_info[cmd].command_pointer == NULL)
		send_to_char("Sorry, that command hasn't been implemented yet.\r\n", ch);
	else if (IS_NPC(ch) && cmd_info[cmd].minimum_level >= LVL_GOD)
		send_to_char("You can't use immortal commands while switched.\r\n", ch);
	else if (IS_INJURED(ch, INJ_TIED) && cmd_info[cmd].minimum_position >= POS_SLEEPING)
		msg_to_char(ch, "You're tied up!\r\n");
	else if (AFF_FLAGGED(ch, AFF_NO_ATTACK) && !IS_NPC(ch) && (cmd_info[cmd].ctype == CTYPE_COMBAT || cmd_info[cmd].ctype == CTYPE_SKILL || cmd_info[cmd].ctype == CTYPE_BUILD)) {
		msg_to_char(ch, "You can't do that in this state.\r\n");
	}
	else if (GET_POS(ch) < cmd_info[cmd].minimum_position) {
		send_low_pos_msg(ch);
	}

	// Command trigger (3/3): exact match on abbreviated command
	else if (check_command_trigger(ch, (char*)cmd_info[cmd].command, line, CMDTRG_EXACT)) {
		return;
	}
	
	else {
		((*cmd_info[cmd].command_pointer) (ch, line, cmd, cmd_info[cmd].subcmd));
	}

	/* Unhide after ? */
	if (ch && IS_SET(cmd_info[cmd].flags, CMD_UNHIDE_AFTER)) {
		REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE);
	}
}


/* Used in specprocs, mostly.  (Exactly) matches "command" to cmd number */
int find_command(const char *command) {
	int cmd;

	for (cmd = 0; *cmd_info[cmd].command != '\n'; cmd++)
	if (!str_cmp(cmd_info[cmd].command, command))
		return (cmd);

	return (-1);
}


 //////////////////////////////////////////////////////////////////////////////
//// ALIAS SYSTEM ////////////////////////////////////////////////////////////

struct alias_data *find_alias(struct alias_data *alias_list, char *str) {
	while (alias_list != NULL) {
		if (LOWER(*str) == LOWER(*alias_list->alias))	/* hey, every little bit counts :-) */
			if (!str_cmp(str, alias_list->alias))
				return (alias_list);

		alias_list = alias_list->next;
	}

	return (NULL);
}


void free_alias(struct alias_data *a) {
	if (a->alias)
		free(a->alias);
	if (a->replacement)
		free(a->replacement);
	free(a);
}


/*
 * Valid numeric replacements are only $1 .. $9 (makes parsing a little
 * easier, and it's not that much of a limitation anyway.)  Also valid
 * is "$*", which stands for the entire original line after the alias.
 * ";" is used to delimit commands.
 */
#define NUM_TOKENS       9

void perform_complex_alias(struct txt_q *input_q, char *orig, struct alias_data *a) {
	struct txt_q temp_queue;
	char *tokens[NUM_TOKENS], *temp, *write_point;
	int num_of_tokens = 0, num;

	skip_spaces(&orig);

	/* First, parse the original string */
	temp = strtok(strcpy(buf2, orig), " ");
	while (temp != NULL && num_of_tokens < NUM_TOKENS) {
		tokens[num_of_tokens++] = temp;
		temp = strtok(NULL, " ");
	}

	/* initialize */
	write_point = buf;
	temp_queue.head = temp_queue.tail = NULL;

	/* now parse the alias */
	for (temp = a->replacement; *temp; temp++) {
		if (*temp == ALIAS_SEP_CHAR) {
			*write_point = '\0';
			buf[MAX_INPUT_LENGTH - 1] = '\0';
			write_to_q(buf, &temp_queue, 1, FALSE);
			write_point = buf;
		}
		else if (*temp == ALIAS_VAR_CHAR) {
			temp++;
			if ((num = *temp - '1') < num_of_tokens && num >= 0) {
				strcpy(write_point, tokens[num]);
				write_point += strlen(tokens[num]);
			}
			else if (num >= 0 && num >= num_of_tokens) {
				// no arg, just skip it
			}
			else if (*temp == ALIAS_GLOB_CHAR) {
				strcpy(write_point, orig);
				write_point += strlen(orig);
			}
			else if ((*(write_point++) = *temp) == '$')	/* redouble $ for act safety */
				*(write_point++) = '$';
		}
		else
			*(write_point++) = *temp;
	}

	*write_point = '\0';
	buf[MAX_INPUT_LENGTH - 1] = '\0';
	write_to_q(buf, &temp_queue, 1, FALSE);

	/* push our temp_queue on to the _front_ of the input queue */
	if (input_q->head == NULL)
		*input_q = temp_queue;
	else {
		temp_queue.tail->next = input_q->head;
		input_q->head = temp_queue.head;
	}
}


/*
 * Given a character and a string, perform alias replacement on it.
 *
 * Return values:
 *   0: String was modified in place; call command_interpreter immediately.
 *   1: String was _not_ modified in place; rather, the expanded aliases
 *      have been placed at the front of the character's input queue.
 */
int perform_alias(descriptor_data *d, char *orig) {
	char first_arg[MAX_INPUT_LENGTH], *ptr;
	struct alias_data *a, *tmp;

	/* Mobs don't have aliases. */
	if (IS_NPC(d->character))
		return (0);

	/* bail out immediately if the guy doesn't have any aliases */
	if ((tmp = GET_ALIASES(d->character)) == NULL)
		return (0);

	/* find the alias we're supposed to match */
	ptr = any_one_arg(orig, first_arg);

	/* bail out if it's null */
	if (!*first_arg)
		return (0);

	/* if the first arg is not an alias, return without doing anything */
	if ((a = find_alias(tmp, first_arg)) == NULL)
		return (0);

	if (a->type == ALIAS_SIMPLE) {
		strcpy(orig, a->replacement);
		return (0);
	}
	else {
		perform_complex_alias(&d->input, ptr, a);
		return (1);
	}
}


/* The interface to the outside world: do_alias */
ACMD(do_alias) {
	extern char *show_color_codes(char *string);
	
	char *repl;
	struct alias_data *a, *temp;

	if (IS_NPC(ch))
		return;

	repl = any_one_arg(argument, arg);

	if (!*arg) {			/* no argument specified -- list currently defined aliases */
		send_to_char("Currently defined aliases:\r\n", ch);
		if ((a = GET_ALIASES(ch)) == NULL)
			send_to_char(" None.\r\n", ch);
		else {
			while (a != NULL) {
				sprintf(buf, "%-15s %s\r\n", a->alias, show_color_codes(a->replacement));
				send_to_char(buf, ch);
				a = a->next;
			}
		}
	}
	else {			/* otherwise, add or remove aliases */
		/* is this an alias we've already defined? */
		if ((a = find_alias(GET_ALIASES(ch), arg)) != NULL) {
			REMOVE_FROM_LIST(a, GET_ALIASES(ch), next);
			free_alias(a);
		}
		/* if no replacement string is specified, assume we want to delete */
		if (!*repl) {
			if (a == NULL) {
				send_to_char("No such alias.\r\n", ch);
			}
			else {
				send_to_char("Alias deleted.\r\n", ch);
			}
		}
		else {			/* otherwise, either add or redefine an alias */
			if (!str_cmp(arg, "alias")) {
				send_to_char("You can't alias 'alias'.\r\n", ch);
				return;
			}
			CREATE(a, struct alias_data, 1);
			a->alias = str_dup(arg);
			delete_doubledollar(repl);
			a->replacement = str_dup(repl);
			if (strchr(repl, ALIAS_SEP_CHAR) || strchr(repl, ALIAS_VAR_CHAR))
				a->type = ALIAS_COMPLEX;
			else
				a->type = ALIAS_SIMPLE;
			a->next = GET_ALIASES(ch);
			GET_ALIASES(ch) = a;
			send_to_char("Alias added.\r\n", ch);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPER FUNCTIONS ////////////////////////////////////////////////////////

/**
* Sends a message telling the character their position is too low to perform
* some task.
*
* Usage:
*   if (GET_POS(ch) < required_pos) {
*     send_low_pos_msg(ch);
*     return;
*   }
*
* @param char_data *ch The player receiving the error.
*/
void send_low_pos_msg(char_data *ch) {
	switch (GET_POS(ch)) {
		case POS_DEAD: {
			msg_to_char(ch, "Lie still; you are DEAD!!!\r\n");
			msg_to_char(ch, "(Type 'respawn' to come back at your tomb.)\r\n");
			break;
		}
		case POS_INCAP:
		case POS_MORTALLYW: {
			send_to_char("You are in a pretty bad shape, unable to do anything!\r\n", ch);
			break;
		}
		case POS_STUNNED: {
			send_to_char("All you can do right now is think about the stars!\r\n", ch);
			break;
		}
		case POS_SLEEPING: {
			send_to_char("In your dreams, or what?\r\n", ch);
			break;
		}
		case POS_RESTING: {
			send_to_char("Nah... You feel too relaxed to do that.\r\n", ch);
			break;
		}
		case POS_SITTING: {
			send_to_char("Maybe you should get on your feet first?\r\n", ch);
			break;
		}
		case POS_FIGHTING: {
			send_to_char("No way! You're fighting for your life!\r\n", ch);
			break;
		}
		// nothing to send at higher positions
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMAND FUNCTIONS ///////////////////////////////////////////////////////

struct sort_struct {
	int sort_pos;
	byte is_social;
} *cmd_sort_info = NULL;
int num_of_cmds;

void sort_commands(void) {
	int a, b, tmp;

	num_of_cmds = 0;

	/*
	 * first, count commands (num_of_commands is actually one greater than the
	 * number of commands; it inclues the '\n'.
	 */
	while (*cmd_info[num_of_cmds].command != '\n')
		num_of_cmds++;

	/* create data array */
	CREATE(cmd_sort_info, struct sort_struct, num_of_cmds);

	/* initialize it */
	for (a = 1; a < num_of_cmds; a++) {
		cmd_sort_info[a].sort_pos = a;
		cmd_sort_info[a].is_social = 0;
	}

	/* Sort.  'a' starts at 1, not 0, to remove 'RESERVED' */
	for (a = 1; a < num_of_cmds - 1; a++) {
		for (b = a + 1; b < num_of_cmds; b++) {
			if (str_cmp(cmd_info[cmd_sort_info[a].sort_pos].command, cmd_info[cmd_sort_info[b].sort_pos].command) > 0) {
				tmp = cmd_sort_info[a].sort_pos;
				cmd_sort_info[a].sort_pos = cmd_sort_info[b].sort_pos;
				cmd_sort_info[b].sort_pos = tmp;
			}
		}
	}
}


ACMD(do_commands) {
	int no, i, cmd_num;
	int wizhelp = 0;
	char_data *vict;

	one_argument(argument, arg);

	if (*arg) {
		if (!(vict = get_char_vis(ch, arg, FIND_CHAR_WORLD))) {
			send_to_char("Who is that?\r\n", ch);
			return;
		}
		if (IS_NPC(vict)) {
			send_to_char("You can't check the commands of an NPC.\r\n", ch);
			return;
		}
		if (GET_ACCESS_LEVEL(ch) < GET_ACCESS_LEVEL(vict)) {
			send_to_char("You can't see the commands of people above your access level.\r\n", ch);
			return;
		}
	}
	else
		vict = ch;

	if (subcmd == SCMD_WIZHELP)
		wizhelp = 1;

	sprintf(buf, "The following %s%s are available to %s:\r\n", wizhelp ? "privileged " : "", "commands", vict == ch ? "you" : PERS(vict, ch, 1));

	/* cmd_num starts at 1, not 0, to remove 'RESERVED' */
	for (no = 1, cmd_num = 1; cmd_num < num_of_cmds; cmd_num++) {
		i = cmd_sort_info[cmd_num].sort_pos;
		if (cmd_info[i].minimum_level >= 0 && (cmd_info[i].ability == NO_ABIL || has_ability(vict, cmd_info[i].ability)) && (GET_ACCESS_LEVEL(vict) >= cmd_info[i].minimum_level || (cmd_info[i].grants != NO_GRANTS && IS_GRANTED(vict, cmd_info[i].grants))) && (cmd_info[i].minimum_level >= LVL_GOD) == wizhelp) {
			if (!IS_SET(cmd_info[i].flags, CMD_IMM_OR_MOB_ONLY) || GET_ACCESS_LEVEL(vict) >= LVL_START_IMM || IS_NPC(vict)) {
				sprintf(buf + strlen(buf), "%-15s", cmd_info[i].command);
				if (!(no % 5))
					strcat(buf, "\r\n");
				no++;
			}
		}
	}

	if ((no - 1) % 5) {
		strcat(buf, "\r\n");
	}
	send_to_char(buf, ch);
}


ACMD(do_missing_help_files) {
	extern struct help_index_element *find_help_entry(int level, const char *word);
	
	struct help_index_element *found;
	int iter, count;
	char lbuf[MAX_STRING_LENGTH];
	
	*lbuf = 0;
	
	count = 0;
	for (iter = 0; *cmd_info[iter].command != '\n'; ++iter) {
		if (strcmp(cmd_info[iter].command, "RESERVED") != 0) {
			found = find_help_entry(LVL_TOP, cmd_info[iter].command);
		
			if (!found) {
				sprintf(lbuf, "%s %-12.12s", lbuf, cmd_info[iter].command);
				if ((++count % 4) == 0) {
					strcat(lbuf, "\r\n");
				}
			}
		}
	}
	
	// possible need for trailing crlf
	if ((++count % 4) == 0) {
		strcat(lbuf, "\r\n");
	}
	
	if (strlen(lbuf) == 0) {
		msg_to_char(ch, "All commands appear to have help files (but some may just be abbreviations).\r\n");
	}
	else {
		msg_to_char(ch, "The following commands need help files:\r\n%s", lbuf);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER CREATION //////////////////////////////////////////////////////

// for character creation: order of creation
struct {
	int state;
} creation_data[] = {
	{ CON_NEWPASSWD },
	{ CON_CNFPASSWD },
	{ CON_QLAST_NAME },	// skips to CON_QSEX sometimes
	{ CON_SLAST_NAME },
	{ CON_CLAST_NAME },
	{ CON_QSEX },
	{ CON_Q_SCREEN_READER },	// skips to CON_Q_HAS_ALT
	
	{ CON_Q_HAS_ALT },	// skips to CON_Q_ARCHETYPE
	{ CON_Q_ALT_NAME },
	{ CON_Q_ALT_PASSWORD },
	
	{ CON_Q_ARCHETYPE },	// skips to CON_BONUS_CREATION if no archetypes exist
	{ CON_ARCHETYPE_CNFRM },
	{ CON_BONUS_CREATION },
	
	{ CON_PROMO_CODE },
	{ CON_CONFIRM_PROMO_CODE },	// only if given invalid code
	
	{ CON_REFERRAL },
	{ CON_FINISH_CREATION },
	
	// put this last
	{ NOTHING }
};


/**
* Sends a character creation prompt based on current state
*
* @param descriptor_data *d the user
*/
void prompt_creation(descriptor_data *d) {
	switch (STATE(d)) {
		case CON_Q_SCREEN_READER: {
			SEND_TO_Q("\r\nEmpireMUD makes heavy use of an ascii map, but also supports screen\r\n", d);
			SEND_TO_Q("readers for the visually impaired. This will replace the map with a short\r\n", d);
			SEND_TO_Q("description of what you can see in each direction on the world map. This\r\n", d);
			SEND_TO_Q("option is only recommended for players using screen readers. You can see\r\n", d);
			SEND_TO_Q("HELP SCREEN READER once you're in the game for more information.\r\n", d);
			SEND_TO_Q("\r\nAre you using a screen reader (y/n)? ", d);
			break;
		}
		case CON_Q_HAS_ALT: {
			SEND_TO_Q("\r\n&cMultiple characters and Alts:&0\r\n", d);
			SEND_TO_Q("If you have an existing character on this game, you must link this new char-\r\n", d);
			SEND_TO_Q("acter to your other one. This is not optional. We require that you link all\r\n", d);
			SEND_TO_Q("your characters together so that the game can process you as the same person.\r\n", d);
			SEND_TO_Q("Other players will NOT be informed who your alts are. Only immortals will know.\r\n", d);
			SEND_TO_Q("\r\n", d);
			SEND_TO_Q("Do you have an existing character (y/n)? ", d);
			break;
		}
		case CON_Q_ALT_NAME: {
			SEND_TO_Q("\r\nEnter the name of any one of your other characters (leave blank to cancel): ", d);
			break;
		}
		case CON_Q_ALT_PASSWORD: {
			SEND_TO_Q("\r\nEnter the password for that character: ", d);
			ProtocolNoEcho(d, true);
			break;
		}
		case CON_NEWPASSWD: {
			SEND_TO_Q("New character.\r\n\r\n", d);
			sprintf(buf, "Give me a password for %s: ", GET_PC_NAME(d->character));
			SEND_TO_Q(buf, d);
			ProtocolNoEcho(d, true);
			break;
		}
		case CON_CNFPASSWD: {
			SEND_TO_Q("\r\n\r\nPlease retype password: ", d);
			break;
		}
		case CON_QLAST_NAME: {
			SEND_TO_Q("\r\n\r\nWould you like a last name (y/n)? ", d);
			break;
		}
		case CON_SLAST_NAME: {
			SEND_TO_Q("\r\nEnter your last name: ", d);
			break;
		}
		case CON_CLAST_NAME: {
			msg_to_desc(d, "\r\nDid I get that right, %s %s%s (y/n)? ", GET_PC_NAME(d->character), GET_LASTNAME(d->character), (UPPER(*GET_LASTNAME(d->character)) != *GET_LASTNAME(d->character)) ? " (first letter is not capitalized)" : "");
			break;
		}
		case CON_QSEX: {
			SEND_TO_Q("\r\nWhat is your sex (M/F)? ", d);
			break;
		}
		case CON_Q_ARCHETYPE: {
			if (archetype_menu[0].type != NOTHING) {
				SUBMENU(d) = 0;
				parse_archetype_menu(d, "");
			}
			else {
				// no archetypes for some reason?
				set_creation_state(d, CON_BONUS_CREATION);
			}
			break;
		}
		case CON_ARCHETYPE_CNFRM: {
			int iter, sub;
			
			msg_to_desc(d, "\r\nYou chose the following archetypes:\r\n");
			for (iter = 0; iter < NUM_ARCHETYPE_TYPES; ++iter) {
				archetype_data *arch = archetype_proto(CREATION_ARCHETYPE(d->character, iter));
				if (arch) {
					// find menu posm for name
					for (sub = 0; archetype_menu[sub].type != NOTHING; ++sub) {
						if (archetype_menu[sub].type == iter) {
							break;
						}
					}
					
					msg_to_desc(d, "%s: \tc%s\t0 - %s\r\n", archetype_menu[sub].name, GET_ARCH_NAME(arch), GET_ARCH_DESC(arch));
				}
			}
			
			msg_to_desc(d, "\r\nIs this correct (y/n)? ");
			break;
		}
		case CON_PROMO_CODE: {
			SEND_TO_Q("\r\nIf you have a promo code, enter it now. Otherwise, just leave it blank > ", d);
			break;
		}
		case CON_CONFIRM_PROMO_CODE: {
			SEND_TO_Q("\r\nUnknown promo code. Proceed without one (y/n)? ", d);
			break;
		}
		case CON_REFERRAL: {
			SEND_TO_Q("\r\nWhere did you hear about us (optional): ", d);
			break;
		}
		case CON_FINISH_CREATION: {
			SEND_TO_Q("\r\n*** Press ENTER: ", d);
			break;
		}
		case CON_BONUS_EXISTING:
		case CON_BONUS_CREATION: {
			show_bonus_trait_menu(d->character);
			break;
		}
	}
}


/**
* Advances the user to the next creation step, and prompts
*
* @param descriptor_data *d the user
*/
void next_creation_step(descriptor_data *d) {
	int iter, found = NOTHING;
	
	for (iter = 0; found == NOTHING && creation_data[iter].state != NOTHING; ++iter) {
		if (STATE(d) == creation_data[iter].state) {
			// found the current state, pick the next one
			found = iter+1;
		}
	}
	
	if (found == NOTHING || creation_data[found].state == NOTHING) {
		set_creation_state(d, CON_FINISH_CREATION);
	}
	else {
		set_creation_state(d, creation_data[found].state);
	}
}


// Handler for CON_Q_ALT_NAME
void process_alt_name(descriptor_data *d, char *arg) {
	player_index_data *index;
	
	if (!*arg) {
		// nothing -- send back to has-alt
		set_creation_state(d, CON_Q_HAS_ALT);
	}
	else if ((index = find_player_index_by_name(arg))) {
		GET_CREATION_ALT_ID(d->character) = index->idnum;
		next_creation_step(d);
	}
	else {
		msg_to_desc(d, "Unable to load character '%s'...\r\nPlease enter a valid alt name or leave blank to cancel: ", arg);
	}
}


// Handler for CON_Q_ALT_PASSWORD
void process_alt_password(descriptor_data *d, char *arg) {	
	char_data *alt = NULL;
	bool file = FALSE, save = FALSE;
	player_index_data *index;

	if (!*arg) {
		// nothing -- send back to has-alt
		set_creation_state(d, CON_Q_HAS_ALT);
	}	
	else if ((index = find_player_index_by_idnum(GET_CREATION_ALT_ID(d->character))) && (alt = find_or_load_player(index->name, &file))) {
		// loaded char, now check password
		if (strncmp(CRYPT(arg, PASSWORD_SALT), GET_PASSWD(alt), MAX_PWD_LENGTH)) {
			syslog(SYS_LOGIN, 0, TRUE, "BAD PW: unable to register alt %s for %s [%s]", GET_NAME(d->character), GET_NAME(alt), d->host);
			GET_BAD_PWS(alt)++;
			save = TRUE;

			if (++(d->bad_pws) >= config_get_int("max_bad_pws")) {	/* 3 strikes and you're out. */
				SEND_TO_Q("Wrong password... disconnecting.\r\n", d);
				STATE(d) = CON_CLOSE;
			}
			else {
				SEND_TO_Q("Wrong password.\r\nPassword: ", d);
				ProtocolNoEcho(d, true);
			}
		}
		else {	// password ok
			syslog(SYS_LOGIN, 0, TRUE, "NEW: associating new user %s with account for %s", GET_NAME(d->character), GET_NAME(alt));
			
			// does 2nd player have an account already? if not, make one
			if (!GET_ACCOUNT(alt)) {
				create_account_for_player(alt);
				save = TRUE;
			}
			GET_TEMPORARY_ACCOUNT_ID(d->character) = GET_ACCOUNT(alt)->id;
			
			next_creation_step(d);
		}
		
		// need to take char of "alt"
		if (save) {
			if (file) {
				store_loaded_char(alt);
				file = FALSE;
				alt = NULL;
			}
			else {
				SAVE_CHAR(alt);
			}
		}
		
		if (file && alt) {
			free_char(alt);
		}
		
		// state was set above
	}
	else {
		msg_to_desc(d, "Unable to load alternate character...\r\nHit enter to return to the creation process: ");
	}
}


/**
* Sets the state and prompts.
*
* @param descriptor_data *d the user
* @param int state any CON_x
*/
void set_creation_state(descriptor_data *d, int state) {
	STATE(d) = state;
	prompt_creation(d);
}


/**
* Shows the menu of bonus traits, marking ones a player already has.
*
* @param char_data *ch The player to show the menu to.
*/
void show_bonus_trait_menu(char_data *ch) {
	extern const char *bonus_bit_descriptions[];

	int iter;
	
	if (IS_NPC(ch) || !ch->desc) {
		return;
	}
	
	msg_to_char(ch, "\r\nAdd a bonus trait:\r\n");
	for (iter = 0; iter < NUM_BONUS_TRAITS; ++iter) {
		msg_to_char(ch, "%2d. %s%s\r\n", (iter+1), bonus_bit_descriptions[iter], (HAS_BONUS_TRAIT(ch, BIT(iter)) ? " &g(already chosen)&0" : ""));
	}
	
	msg_to_char(ch, "\r\nEnter a number to choose (or 'skip' to choose later) > ");
}


/**
* Starts the character creation process with prompt
*
* @param descriptor_data *d the user
*/
void start_creation_process(descriptor_data *d) {
	set_creation_state(d, creation_data[0].state);
}


 //////////////////////////////////////////////////////////////////////////////
//// MENU INTERPRETER FUNCTIONS //////////////////////////////////////////////


/* Determine if a person is multiplaying (FALSE), true is "ok to log in" for some reason */
bool check_multiplaying(descriptor_data *d) {
	descriptor_data *c, *next_c;
	bool ok = TRUE;
	
	if (ACCOUNT_FLAGGED(d->character, ACCT_MULTI_CHAR)) {
		return TRUE;
	}
	
	/* Check for connected players with identical hosts */
	for (c = descriptor_list; c && ok; c = next_c) {
		next_c = c->next;
		
		if (c == d || STATE(c) != CON_PLAYING || GET_IDNUM(c->character) == GET_IDNUM(d->character)) {
			continue;
		}
		
		if (!ACCOUNT_FLAGGED(d->character, ACCT_MULTI_CHAR) && GET_ACCOUNT(d->character) == GET_ACCOUNT(c->character)) {
			// account is already online: disconnect the other one (rather than bounce them)
			SEND_TO_Q("\r\nYour login has been usurped by another character from your account!\r\n", c);
			STATE(c) = (STATE(c) == CON_PLAYING) ? CON_DISCONNECT : CON_CLOSE;
			// ok = FALSE;
		}
		else if (!ACCOUNT_FLAGGED(d->character, ACCT_MULTI_IP | ACCT_MULTI_CHAR) && !ACCOUNT_FLAGGED(c->character, ACCT_MULTI_IP | ACCT_MULTI_CHAR) && !PLR_FLAGGED(d->character, PLR_IPMASK) && !strcmp(c->host, d->host)) {
			// IP is already logged in: just decline the connection
			ok = FALSE;
		}
	}
	
	return ok;
}


// simple motd
void send_motd(descriptor_data *d) {
	extern char *motd;
	extern char *imotd;
	extern char *CREDIT_MESSG;
	int i;

	SEND_TO_Q(CREDIT_MESSG, d);

	SEND_TO_Q(" ", d);
	for (i = 0; i < 79; i++)
		SEND_TO_Q("=", d);
	SEND_TO_Q("\r\n\r\n", d);

	if (IS_IMMORTAL(d->character))
		SEND_TO_Q(imotd, d);
	else
		SEND_TO_Q(motd, d);

	SEND_TO_Q("\r\n ", d);
	for (i = 0; i < 79; i++)
		SEND_TO_Q("=", d);
	SEND_TO_Q("\r\n", d);
}


/**
* Sends the MOTD, MXP tags, and other data that should be shown when a player
* logs in, which may happen in several different ways.
*
* @param descriptor_data *desc The person to send it to.
* @param int bad_pws Number of bad password attempts, which sometimes must be retrieved and cleared ahead of time.
*/
void send_login_motd(descriptor_data *desc, int bad_pws) {
	send_motd(desc);
	MXPSendTag(desc, "<VERSION>");
	
	/* Check bad passwords */
	if (bad_pws) {
		sprintf(buf, "\r\n\r\n\007\007\007&r%d LOGIN FAILURE%s SINCE LAST SUCCESSFUL LOGIN.&0\r\n", bad_pws, (bad_pws > 1) ? "S" : "");
		SEND_TO_Q(buf, desc);
		GET_BAD_PWS(desc->character) = 0;
	}

	/* Check previous logon */
	if (desc->character->prev_host) {
		sprintf(buf, "Your last login was on %6.10s from %s.\r\n", ctime(&desc->character->prev_logon), desc->character->prev_host);
		SEND_TO_Q(buf, desc);
	}
}


/*
 * XXX: Make immortals 'return' instead of being disconnected when switched
 *      into person returns.  This function seems a bit over-extended too.
 */
int perform_dupe_check(descriptor_data *d) {
	void refresh_all_quests(char_data *ch);
	
	descriptor_data *k, *next_k;
	char_data *target = NULL, *ch, *next_ch;
	int mode = 0;

	#define RECON		1
	#define USURP		2
	#define UNSWITCH	3

	int id = GET_IDNUM(d->character);

	/*
	 * Now that this descriptor has successfully logged in, disconnect all
	 * other descriptors controlling a character with the same ID number.
	 */

	for (k = descriptor_list; k; k = next_k) {
		next_k = k->next;

		if (k == d)
			continue;

		if (k->original && (GET_IDNUM(k->original) == id)) {    /* switched char */
			SEND_TO_Q("\r\nMultiple login detected -- disconnecting.\r\n", k);
			STATE(k) = CON_CLOSE;
			if (!target) {
				target = k->original;
				mode = UNSWITCH;
			}
			if (k->character) {
				k->character->desc = NULL;
			}
			k->character = NULL;
			k->original = NULL;
		}
		else if (k->character && (GET_IDNUM(k->character) == id)) {
			if (!target && STATE(k) == CON_PLAYING) {
				SEND_TO_Q("\r\nThis body has been usurped!\r\n", k);
				target = k->character;
				mode = USURP;
			}
			k->character->desc = NULL;
			k->character = NULL;
			k->original = NULL;
			SEND_TO_Q("\r\nMultiple login detected -- disconnecting.\r\n", k);
			STATE(k) = CON_CLOSE;
		}
	}

	/*
	 * now, go through the character list, deleting all characters that
	 * are not already marked for deletion from the above step (i.e., in the
	 * CON_HANGUP state), and have not already been selected as a target for
	 * switching into.  In addition, if we haven't already found a target,
	 * choose one if one is available (while still deleting the other
	 * duplicates, though theoretically none should be able to exist).
	 */

	for (ch = character_list; ch; ch = next_ch) {
		next_ch = ch->next;

		if (IS_NPC(ch))
			continue;
		if (GET_IDNUM(ch) != id)
			continue;

		/* ignore chars with descriptors (already handled by above step) */
		if (ch->desc)
			continue;

		/* don't extract the target char we've found one already */
		if (ch == target)
			continue;

		/* we don't already have a target and found a candidate for switching */
		if (!target) {
			target = ch;
			mode = RECON;
			continue;
		}

		/* we've found a duplicate - blow him away. */
		if (IN_ROOM(ch))
			char_from_room(ch);
		char_to_room(ch, world_table);	// put somewhere extractable
		extract_char(ch);
	}

	/* no target for swicthing into was found - allow login to continue */
	if (!target)
		return (0);

	/* Okay, we've found a target.  Connect d to target. */
	free_char(d->character); /* get rid of the old char */
	d->character = target;
	d->character->desc = d;
	d->original = NULL;
	d->character->char_specials.timer = 0;
	REMOVE_BIT(PLR_FLAGS(d->character), PLR_MAILING);
	STATE(d) = CON_PLAYING;

	if (PLR_FLAGGED(d->character, PLR_IPMASK))
		strcpy(d->host, "masked");

	switch (mode) {
		case RECON:
			SEND_TO_Q("Reconnecting.\r\n", d);
			act("$n has reconnected.", TRUE, d->character, 0, 0, TO_ROOM);
			syslog(SYS_LOGIN, GET_INVIS_LEV(d->character), TRUE, "%s [%s] has reconnected.", GET_NAME(d->character), d->host);
			break;
		case USURP:
			SEND_TO_Q("You take over your own body, already in use!\r\n", d);
			act("$n suddenly keels over in pain, surrounded by a white aura...\r\n"
				"$n's body has been taken over by a new spirit!", TRUE, d->character, 0, 0, TO_ROOM);
			syslog(SYS_LOGIN, GET_INVIS_LEV(d->character), TRUE, "%s has re-logged in ... disconnecting old socket.", GET_NAME(d->character));
			break;
		case UNSWITCH:
			SEND_TO_Q("Reconnecting to unswitched char.", d);
			syslog(SYS_LOGIN, GET_INVIS_LEV(d->character), TRUE, "%s [%s] has reconnected.", GET_NAME(d->character), d->host);
			break;
	}
	
	refresh_all_quests(d->character);
	MXPSendTag(d, "<VERSION>");
	
	return (1);
}


// basic name validation and processing
int _parse_name(char *arg, char *name) {
	int i, iter, caps;
	int max_caps = config_get_int("max_capitals_in_name");

	/* skip whitespaces */
	for (; isspace(*arg); arg++);
	
	if (max_caps > 0) {
		caps = 0;
		for (iter = 0; iter < strlen(arg); ++iter) {
			if (isupper(arg[iter])) {
				++caps;
			}
		}
		if (caps > max_caps) {
			return 1;
		}
	}
	
	// don't allow leading apostrophe or dash
	if (*arg == '\'' || *arg == '-') {
		return 1;
	}

	// check for anything that's not an apostrophe or dash
	for (i = 0; (*name = *arg); arg++, i++, name++)
		if (!isalpha(*arg) && *arg != '\'' && *arg != '-')
			return 1;

	if (!i)
		return (1);

	return (0);
}


/**
* Master "socket nanny" for processing menu input.
*/
void nanny(descriptor_data *d, char *arg) {
	void check_delayed_load(char_data *ch);
	void display_tip_to_char(char_data *ch);
	extern void enter_player_game(descriptor_data *d, int dolog, bool fresh);
	extern int isbanned(char *hostname);
	extern int num_earned_bonus_traits(char_data *ch);
	void start_new_character(char_data *ch);
	extern int Valid_Name(char *newname);
	
	extern struct promo_code_list promo_codes[];
	extern const char *unapproved_login_message;
	extern char *START_MESSG;
	extern int wizlock_level;
	extern char *wizlock_message;

	char buf[MAX_STRING_LENGTH], tmp_name[MAX_INPUT_LENGTH];
	int load_result, i, iter;
	bool show_start = FALSE;
	char_data *temp_char;
	
	// this avoids treating telnet negotiation as menu input
	if (d->no_nanny) {
		d->no_nanny = FALSE;
		return;
	}

	skip_spaces(&arg);

	switch (STATE(d)) {
		case CON_GET_NAME: {	/* wait for input of name */
			if (d->character == NULL) {
				CREATE(d->character, char_data, 1);
				clear_char(d->character);
				init_player_specials(d->character);
				d->character->desc = d;
				d->character->prev_host = str_dup(d->host);	// this will be overwritten if it's not a new char
			}
			
			if (!*arg) {
				SET_BIT(PLR_FLAGS(d->character), PLR_KEEP_LAST_LOGIN_INFO);	// prevent login storing
				STATE(d) = CON_CLOSE;
			}
			else if (!str_cmp(arg, "new")) {
				// special case for players who typed "new"
				SEND_TO_Q("\r\nEnter new character name: ", d);
				return;
			}
			else {
				if ((_parse_name(arg, tmp_name)) || strlen(tmp_name) < 2 || strlen(tmp_name) > MAX_NAME_LENGTH || !Valid_Name(tmp_name) || fill_word(strcpy(buf, tmp_name)) || reserved_word(buf)) {
					SEND_TO_Q("Invalid name, please try another.\r\nName: ", d);
					return;
				}
				if ((temp_char = load_player(tmp_name, TRUE))) {
					free_char(d->character);
					d->character = temp_char;	// can't load directly; overwrites the existing char
					d->character->desc = d;
					check_delayed_load(d->character);
					
					/* undo it just in case they are set */
					REMOVE_BIT(PLR_FLAGS(d->character), PLR_MAILING);

					SEND_TO_Q("Password: ", d);
					ProtocolNoEcho(d, true);
					d->idle_tics = 0;
					STATE(d) = CON_PASSWORD;
				}
				else {
					/* player unknown -- make new character */

					/* Check for multiple creations of a character. */
					if (!Valid_Name(tmp_name)) {
						SEND_TO_Q("Invalid name, please try another.\r\nName: ", d);
						return;
					}
					GET_PC_NAME(d->character) = str_dup(CAP(tmp_name));

					sprintf(buf, "Did I get that right, %s (Y/N)? ", tmp_name);
					SEND_TO_Q(buf, d);
					STATE(d) = CON_NAME_CNFRM;
				}
			}
			break;
		}

		case CON_NAME_CNFRM: {	/* wait for conf. of new name    */
			if (UPPER(*arg) == 'Y') {
				if (isbanned(d->host) >= BAN_NEW) {
					syslog(SYS_LOGIN, 0, TRUE, "Request for new char %s denied from [%s] (siteban)", GET_PC_NAME(d->character), d->host);
					SEND_TO_Q("Sorry, new characters are not allowed from your site!\r\n", d);
					STATE(d) = CON_CLOSE;
					return;
				}
				if (wizlock_level) {
					if (!wizlock_message)
						SEND_TO_Q("Sorry, new players can't be created at the moment.\r\n", d);
					else {
						msg_to_desc(d, "%s\r\n", wizlock_message);
					}
					syslog(SYS_LOGIN, 0, TRUE, "Request for new char %s denied from [%s] (wizlock)", GET_PC_NAME(d->character), d->host);
					STATE(d) = CON_CLOSE;
					return;
				}
				
				start_creation_process(d);
			}
			else if (*arg == 'n' || *arg == 'N') {
				SEND_TO_Q("Okay, what IS it, then? ", d);
				free(GET_PC_NAME(d->character));
				GET_PC_NAME(d->character) = NULL;
				STATE(d) = CON_GET_NAME;
			}
			else {
				SEND_TO_Q("Please type Yes or No: ", d);
			}
			break;
		}

		case CON_PASSWORD: {	/* get pwd for known player      */
			/*
			 * To really prevent duping correctly, the player's record should
			 * be reloaded from disk at this point (after the password has been
			 * typed).  However I'm afraid that trying to load a character over
			 * an already loaded character is going to cause some problem down the
			 * road that I can't see at the moment.  So to compensate, I'm going to
			 * (1) add a 15 or 20-second time limit for entering a password, and (2)
			 * re-add the code to cut off duplicates when a player quits.  JE 6 Feb 96
			 */

			/* New echo-on eats the return on telnet. Extra space better than none. */
			SEND_TO_Q("\r\n", d);

			if (!*arg) {
				SET_BIT(PLR_FLAGS(d->character), PLR_KEEP_LAST_LOGIN_INFO);	// prevent login from storing
				STATE(d) = CON_CLOSE;
			}
			else {
				if (strncmp(CRYPT(arg, PASSWORD_SALT), GET_PASSWD(d->character), MAX_PWD_LENGTH)) {
					syslog(SYS_LOGIN, 0, TRUE, "BAD PW: %s [%s]", GET_NAME(d->character), d->host);
					GET_BAD_PWS(d->character)++;
					SET_BIT(PLR_FLAGS(d->character), PLR_KEEP_LAST_LOGIN_INFO);
					SAVE_CHAR(d->character);
					if (++(d->bad_pws) >= config_get_int("max_bad_pws")) {	/* 3 strikes and you're out. */
						SEND_TO_Q("Wrong password... disconnecting.\r\n", d);
						STATE(d) = CON_CLOSE;
					}
					else {
						SEND_TO_Q("Wrong password.\r\nPassword: ", d);
					}
					return;
				}
				
				// echo back on
				ProtocolNoEcho(d, false);

				/* Password was correct. */
				load_result = GET_BAD_PWS(d->character);
				GET_BAD_PWS(d->character) = 0;
				d->bad_pws = 0;

				if (isbanned(d->host) == BAN_SELECT && !ACCOUNT_FLAGGED(d->character, ACCT_SITEOK)) {
					SEND_TO_Q("Sorry, this account has not been cleared for login from your site!\r\n", d);
					STATE(d) = CON_CLOSE;
					syslog(SYS_LOGIN, 0, TRUE, "Connection attempt for %s denied from %s", GET_NAME(d->character), d->host);
					return;
				}
				if (GET_ACCESS_LEVEL(d->character) < wizlock_level) {
					if (wizlock_message) {
						msg_to_desc(d, "%s\r\n", wizlock_message);
					}
					else
						SEND_TO_Q("The game is temporarily restricted... try again later.\r\n", d);
					STATE(d) = CON_CLOSE;
					syslog(SYS_LOGIN, 0, TRUE, "Request for login denied for %s [%s] (wizlock)", GET_NAME(d->character), d->host);
					return;
				}
				/* check and make sure no other copies of this player are logged in */
				if (!check_multiplaying(d)) {
					SEND_TO_Q("\r\n\033[31mAccess Denied: Multiplaying detected\033[0m\r\n", d);

					SEND_TO_Q("There is already a character logged in from the same IP address or account as\r\n", d);
					SEND_TO_Q("you. If you are controlling that character, you must remove it from the game\r\n", d);
					SEND_TO_Q("before this character can enter. Rarely, computers may share IP addresses. If\r\n", d);
					SEND_TO_Q("this is the case, we may grant exceptions. You will need to petition the staff\r\n", d);
					SEND_TO_Q("to allow more than one person from your IP if you have multiple people who want\r\n", d);
					SEND_TO_Q("to play together.\r\n", d);
					SEND_TO_Q("\r\n", d);
					SEND_TO_Q("Press ENTER to continue:\r\n", d);
					syslog(SYS_LOGIN, 0, TRUE, "Login denied: Multiplaying detected for %s [%s]", GET_NAME(d->character), d->host);

					STATE(d) = CON_GOODBYE;
					return;
				}
				
				if (perform_dupe_check(d))
					return;
				
				if (!PLR_FLAGGED(d->character, PLR_INVSTART)) {
					syslog(SYS_LOGIN, GET_INVIS_LEV(d->character), TRUE, "%s [%s] has connected.", GET_NAME(d->character), PLR_FLAGGED(d->character, PLR_IPMASK) ? "masked" : d->host);
				}

				// check here if they need more traits than they have (IF they are an existing char?)
				if (GET_ACCESS_LEVEL(d->character) > 0 && num_earned_bonus_traits(d->character) > count_bits(GET_BONUS_TRAITS(d->character))) {
					show_bonus_trait_menu(d->character);
					STATE(d) = CON_BONUS_EXISTING;
					return;
				}
				
				send_login_motd(d, load_result);
				
				// send on to motd
				SEND_TO_Q("\r\n*** Press ENTER: ", d);
				STATE(d) = CON_RMOTD;
			}
			break;
		}

		case CON_NEWPASSWD: {
			if (!*arg || strlen(arg) > MAX_PWD_LENGTH || strlen(arg) < 3 || !str_cmp(arg, GET_PC_NAME(d->character))) {
				SEND_TO_Q("\r\nIllegal password.\r\n", d);
				SEND_TO_Q("Password: ", d);
				return;
			}
			
			GET_PASSWD(d->character) = str_dup(CRYPT(arg, PASSWORD_SALT));
			next_creation_step(d);
			break;
		}

		case CON_CNFPASSWD: {
			if (strncmp(CRYPT(arg, PASSWORD_SALT), GET_PASSWD(d->character), MAX_PWD_LENGTH)) {
				SEND_TO_Q("\r\nPasswords don't match... start over.\r\n", d);
				SEND_TO_Q("Password: ", d);
				STATE(d) = CON_NEWPASSWD;
				return;
			}
			ProtocolNoEcho(d, false);
			next_creation_step(d);
			break;
		}

		case CON_QLAST_NAME: {	/* Want a last name? */
			if (UPPER(*arg) == 'Y') {
				set_creation_state(d, CON_SLAST_NAME);
			}
			else if (UPPER(*arg) == 'N') {
				GET_LASTNAME(d->character) = NULL;
				set_creation_state(d, CON_QSEX);
				break;
			}
			else {
				SEND_TO_Q("\r\nPlease type Yes or No: ", d);
			}
			
			break;
		}

		case CON_Q_SCREEN_READER: {
			if (UPPER(*arg) == 'Y') {
				SET_BIT(PRF_FLAGS(d->character), PRF_SCREEN_READER | PRF_COMPACT | PRF_SCROLLING);
				set_creation_state(d, CON_Q_HAS_ALT);
			}
			else if (UPPER(*arg) == 'N') {
				REMOVE_BIT(PRF_FLAGS(d->character), PRF_SCREEN_READER);
				next_creation_step(d);
			}
			else {
				SEND_TO_Q("\r\nPlease type Yes or No: ", d);
			}
			break;
		}

		case CON_Q_HAS_ALT: {
			if (UPPER(*arg) == 'Y') {
				next_creation_step(d);
			}
			else if (UPPER(*arg) == 'N') {
				set_creation_state(d, CON_Q_ARCHETYPE);
			}
			else {
				SEND_TO_Q("\r\nPlease type Yes or No: ", d);
			}
			break;
		}
		case CON_Q_ALT_NAME: {
			process_alt_name(d, arg);
			break;
		}
		case CON_Q_ALT_PASSWORD: {
			ProtocolNoEcho(d, false);
			SEND_TO_Q("\r\n", d);	// echo-off usually hides the CR
			process_alt_password(d, arg);
			break;
		}

		case CON_SLAST_NAME: {	/* What's yer last name? */
			if (!*arg) {
				SEND_TO_Q("\r\nEnter a last name: ", d);
				return;
			}
			else if ((_parse_name(arg, tmp_name)) || !Valid_Name(tmp_name) || strlen(tmp_name) < 2 || strlen(tmp_name) > MAX_NAME_LENGTH || fill_word(strcpy(buf, tmp_name)) || reserved_word(buf)) {
				SEND_TO_Q("\r\nInvalid last name, please try another.\r\n"
						  "Enter a last name: ", d);
				return;
			}
			else {
				if (GET_LASTNAME(d->character)) {
					free(GET_LASTNAME(d->character));
				}

				GET_LASTNAME(d->character) = str_dup(tmp_name);
				next_creation_step(d);
			}
			break;
		}

		case CON_CLAST_NAME: {	/* Wait for conf. of last name    */
			if (UPPER(*arg) == 'Y') {
				next_creation_step(d);
			}
			else if (UPPER(*arg) == 'N') {
				SEND_TO_Q("Okay, what IS it, then? ", d);
				free(GET_LASTNAME(d->character));
				GET_LASTNAME(d->character) = NULL;
				STATE(d) = CON_SLAST_NAME;
			}
			else {
				SEND_TO_Q("Please type Yes or No: ", d);
			}
			break;
		}

		case CON_QSEX: {	/* query sex of new user         */
			switch (LOWER(*arg)) {
				case 'm':
					d->character->player.sex = SEX_MALE;
					break;
				case 'f':
					d->character->player.sex = SEX_FEMALE;
					break;
				default:
					SEND_TO_Q("That is not a sex...\r\nWhat IS your sex? ", d);
					return;
			}

			next_creation_step(d);
			break;
		}
		
		case CON_REFERRAL: {
			if (*arg) {
				// store for later
				arg[MAX_REFERRED_BY_LENGTH-1] = '\0';
				if (GET_REFERRED_BY(d->character)) {
					free(GET_REFERRED_BY(d->character));
				}
				GET_REFERRED_BY(d->character) = str_dup(arg);
			}
			
			next_creation_step(d);
			break;
		}

		case CON_FINISH_CREATION: {
			// some finalization
			
			if (GET_ACCESS_LEVEL(d->character) == 0) {
				// set to base level now
				GET_ACCESS_LEVEL(d->character) = 1;
			}
			init_player(d->character);
			SAVE_CHAR(d->character);
			
			send_login_motd(d, GET_BAD_PWS(d->character));
			
			SEND_TO_Q("\r\n*** Press ENTER: ", d);
			STATE(d) = CON_RMOTD;

			syslog(SYS_LOGIN, 0, TRUE, "NEW: %s [%s] (promo: %s)", GET_NAME(d->character), d->host, GET_PROMO_ID(d->character) > 0 ? promo_codes[GET_PROMO_ID(d->character)].code : "none");
			if (GET_REFERRED_BY(d->character) && *GET_REFERRED_BY(d->character)) {
				syslog(SYS_LOGIN, 0, FALSE, "Referral: %s", GET_REFERRED_BY(d->character));
			}
			break;
		}

		case CON_Q_ARCHETYPE: {
			parse_archetype_menu(d, arg);
			break;
		}
		
		case CON_ARCHETYPE_CNFRM: {
			if (is_abbrev(arg, "yes")) {
				next_creation_step(d);
			}
			else if (is_abbrev(arg, "no")) {
				set_creation_state(d, CON_Q_ARCHETYPE);
			}
			else {
				msg_to_desc(d, "\r\nPlease type YES or NO: ");
			}
			break;
		}
		
		case CON_PROMO_CODE: {
			int promo = -1;
			
			skip_spaces(&arg);
			if (!*arg) {
				// skip entirely
				set_creation_state(d, CON_REFERRAL);
				return;
			}
			
			for (iter = 0; *promo_codes[iter].code != '\n'; ++iter) {
				if (!promo_codes[iter].expired && !str_cmp(arg, promo_codes[iter].code)) {
					promo = iter;
					break;
				}
			}
			
			GET_PROMO_ID(d->character) = promo;
			
			// pass off based on code validity
			if (promo < 0) {
				set_creation_state(d, CON_CONFIRM_PROMO_CODE);
			}
			else {
				SEND_TO_Q("\r\nPromo code accepted.\r\n", d);
				set_creation_state(d, CON_REFERRAL);
			}
			break;
		}
		
		case CON_CONFIRM_PROMO_CODE: {
			switch (LOWER(*arg)) {
				case 'y': {
					next_creation_step(d);
					break;
				}
				case 'n': {
					set_creation_state(d, CON_PROMO_CODE);
					break;
				}
				default: {
					SEND_TO_Q("Please type YES or NO: ", d);
					return;
				}
			}
			break;
		}

		case CON_RMOTD: {		/* read CR after printing motd   */
			if (PLR_FLAGGED(d->character, PLR_IPMASK)) {
				strcpy(d->host, "masked");
			}
	
			// READY TO ENTER THE GAME
			if (!check_multiplaying(d)) {
				SEND_TO_Q("\r\n\033[31mAccess Denied: Multiplaying detected\033[0m\r\n", d);
				SEND_TO_Q("There is already someone logged in from the same IP address as you. If you\r\n", d);
				SEND_TO_Q("are controlling that character, you must remove it from the game before this\r\n", d);
				SEND_TO_Q("character can enter. Rarely, computers may share IP addresses. If this is\r\n", d);
				SEND_TO_Q("the case, exceptions may be granted. You will need to petition the staff\r\n", d);
				SEND_TO_Q("member in charge of authorization. When you are able to log into the mud,\r\n", d);
				SEND_TO_Q("type HELP AUTHORIZATION for the appropriate e-mail address, or contact the\r\n", d);
				SEND_TO_Q("staff member via the game.\r\n", d);
				SEND_TO_Q("\r\nPress ENTER to continue: ", d);
				syslog(SYS_LOGIN, 0, TRUE, "Login denied: Multiplaying detected for %s [%s]", GET_NAME(d->character), d->host);

				STATE(d) = CON_GOODBYE;
				return;
			}
			
			if (LOWER(*arg) == 'i' && (IS_GOD(d->character) || IS_IMMORTAL(d->character))) {
				GET_INVIS_LEV(d->character) = GET_ACCESS_LEVEL(d->character);
			}
			
			// TODO most* of this block is repeated in do_alternate

			// put them in-game
			enter_player_game(d, TRUE, TRUE);
			
			msg_to_desc(d, "\r\n%s\r\n\r\n", config_get_string("welcome_message"));
			act("$n has entered the game.", TRUE, d->character, 0, 0, TO_ROOM);

			STATE(d) = CON_PLAYING;
			
			// needs newbie setup (gear, etc?)
			if (PLR_FLAGGED(d->character, PLR_NEEDS_NEWBIE_SETUP)) {
				start_new_character(d->character);
				show_start = TRUE;
			}
			
			if (AFF_FLAGGED(d->character, AFF_EARTHMELD)) {
				msg_to_char(d->character, "You are earthmelded.\r\n");
			}
			else {
				look_at_room(d->character);
			}
			
			msg_to_char(d->character, "\r\n");	// leading \r\n between the look and the rest
			
			if (GET_LOYALTY(d->character) && EMPIRE_MOTD(GET_LOYALTY(d->character))) {
				msg_to_char(d->character, "Empire MOTD:\r\n%s\r\n", EMPIRE_MOTD(GET_LOYALTY(d->character)));
			}
			
			display_tip_to_char(d->character);
			
			if (GET_MAIL_PENDING(d->character)) {
				send_to_char("&rYou have mail waiting.&0\r\n", d->character);
			}
			
			if (!IS_APPROVED(d->character)) {
				send_to_char(unapproved_login_message, d->character);
			}
			if (show_start) {
				send_to_char(START_MESSG, d->character);
			}
			
			d->has_prompt = 0;
			break;
		}

		// both add-trait menus
		case CON_BONUS_CREATION:
		case CON_BONUS_EXISTING: {
			bool skip = FALSE;
			i = 0;
			
			if (!str_cmp(arg, "skip")) {
				skip = TRUE;
			}
			else {
				if (!*arg || !isdigit(*arg)) {
					// just re-show
					show_bonus_trait_menu(d->character);
					return;
				}
				if ((i = atoi(arg)) < 1 || i > NUM_BONUS_TRAITS) {
					SEND_TO_Q("\r\nInvalid trait choice. Try again > ", d);
					return;
				}
			
				// i is 1 over the value we want (menu is 1-based)
				--i;
			
				if (HAS_BONUS_TRAIT(d->character, BIT(i))) {
					SEND_TO_Q("\r\nYou already have that trait! Try again > ", d);
					return;
				}
			
				// seems ok
				SET_BIT(GET_BONUS_TRAITS(d->character), BIT(i));
			}
			
			// only apply now if they are NOT creating
			if (STATE(d) != CON_BONUS_CREATION) {
				void apply_bonus_trait(char_data *ch, bitvector_t trait, bool add);
				
				if (!skip) {
					apply_bonus_trait(d->character, BIT(i), TRUE);
				}

				// and send them to the motd
				send_login_motd(d, GET_BAD_PWS(d->character));
				
				SEND_TO_Q("\r\n*** Press ENTER: ", d);
				STATE(d) = CON_RMOTD;
			}
			else {
				// creating
				next_creation_step(d);
			}
			
			break;
		}

		case CON_GOODBYE: {	/* take an ENTER and then quit */
			SEND_TO_Q("Goodbye.\r\n", d);
			STATE(d) = CON_CLOSE;
			break;
		}

		/*
		 * It's possible, if enough pulses are missed, to kick someone off
		 * while they are at the password prompt. We'll just defer to let
		 * the game_loop() axe them.
		 */
		case CON_CLOSE: {
			break;
		}

		default: {
			log("SYSERR: Nanny: illegal state of con'ness (%d) for '%s'; closing connection.", STATE(d), d->character ? GET_NAME(d->character) : "<unknown>");
			STATE(d) = CON_DISCONNECT;	/* Safest to do. */
			break;
		}
	}
}
