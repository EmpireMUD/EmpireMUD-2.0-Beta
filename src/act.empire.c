/* ************************************************************************
*   File: act.empire.c                                    EmpireMUD 2.0b2 *
*  Usage: stores all of the empire-related commands                       *
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
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Helpers
*   City Helpers
*   Efind Helpers
*   Import / Export Helpers
*   Inspire Helpers
*   Islands Helpers
*   Tavern Helpers
*   Territory Helpers
*   Empire Commands
*/

// external vars
extern struct empire_chore_type chore_data[NUM_CHORES];
extern struct city_metadata_type city_type[];
extern const char *empire_trait_types[];
extern const char *trade_type[];
extern const char *trade_mostleast[];
extern const char *trade_overunder[];

// external funcs
extern bool can_claim(char_data *ch);
extern int city_points_available(empire_data *emp);
void clear_private_owner(int id);
void eliminate_linkdead_players();
extern int get_total_score(empire_data *emp);
extern char *get_room_name(room_data *room, bool color);
extern bool is_trading_with(empire_data *emp, empire_data *partner);
extern bitvector_t olc_process_flag(char_data *ch, char *argument, char *name, char *command, const char **flag_names, bitvector_t existing_bits);

// locals
void perform_abandon_city(empire_data *emp, struct empire_city_data *city, bool full_abandon);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Determines how much an empire must spend to start a war with another empire,
* based on the configs "war_cost_max" and "war_cost_min". This is calculated
* by the difference in score between the two empires with the maximum value at
* a difference of 50 or more. The function is a simple parabola.
*
* @param empire_data *emp The empire declaring war.
* @param empire_data *victim The victim empire.
* @return int The cost, in coins, to go to war.
*/
int get_war_cost(empire_data *emp, empire_data *victim) {
	int min = config_get_int("war_cost_min"), max = config_get_int("war_cost_max");
	int score_e, score_v;
	double diff, max_diff;
	int cost;
	
	score_e = emp ? get_total_score(emp) : 0;
	score_v = victim ? get_total_score(victim) : 0;
	
	// difference between scores (caps at 50)
	max_diff = 50;
	diff = ABSOLUTE(score_e - score_v);
	diff = MIN(diff, max_diff);
	
	// simple parabola: y = ax^2 + b, a = (max-min)/max_diff^2, b = min
	cost = (max - min) / (max_diff * max_diff) * (diff * diff) + min;
	return cost;
}


/**
* for do_empires
*
* @param char_data *ch who to send to
* @param empire_data *e The empire to show
*/
static void show_detailed_empire(char_data *ch, empire_data *e) {
	extern const char *priv[];
	extern const char *score_type[];
	extern const char *techs[];
	
	struct empire_political_data *emp_pol;
	int iter, sub, found_rank;
	empire_data *other, *emp_iter, *next_emp;
	bool found, is_own_empire, comma;
	char line[256];
	
	// for displaying diplomacy below
	struct diplomacy_display_data {
		bitvector_t type;	// DIPL_x
		char *name; // "Offering XXX to empire"
		char *text;	// "XXX empire, empire, empire"
		bool offers_only;	// only shows separately if it's an offer
	} diplomacy_display[] = {
		{ DIPL_ALLIED, "an alliance", "Allied with", FALSE },
		{ DIPL_NONAGGR, "a non-aggression pact", "In a non-aggression pact with", FALSE },
		{ DIPL_PEACE, "peace", "At peace with", FALSE },
		{ DIPL_TRUCE, "a truce", "In a truce with", FALSE },
		{ DIPL_DISTRUST, "distrust", "Distrustful of", FALSE },
		{ DIPL_WAR, "war", "At war with", FALSE },
		{ DIPL_TRADE, "trade", "", TRUE },
		
		// goes last
		{ NOTHING, "\n" }
	};
	
	
	is_own_empire = (GET_LOYALTY(ch) == e) || GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES);

	// add empire vnum for imms
	if (IS_IMMORTAL(ch)) {
		sprintf(line, " (vnum %d)", EMPIRE_VNUM(e));
	}
	else {
		*line = '\0';
	}
	
	msg_to_char(ch, "%s%s&0%s, led by %s\r\n", EMPIRE_BANNER(e), EMPIRE_NAME(e), line, (get_name_by_id(EMPIRE_LEADER(e)) ? CAP(get_name_by_id(EMPIRE_LEADER(e))) : "(Unknown)"));
	
	if (IS_IMMORTAL(ch)) {
		msg_to_char(ch, "Created: %-24.24s\r\n", ctime(&EMPIRE_CREATE_TIME(e)));
	}
	
	if (EMPIRE_DESCRIPTION(e)) {
		msg_to_char(ch, "%s&0", EMPIRE_DESCRIPTION(e));
	}
	
	msg_to_char(ch, "Adjective form: %s\r\n", EMPIRE_ADJECTIVE(e));

	msg_to_char(ch, "Ranks%s:\r\n", (is_own_empire ? " and privileges" : ""));
	for (iter = 1; iter <= EMPIRE_NUM_RANKS(e); ++iter) {
		// rank name
		msg_to_char(ch, " %2d. %s&0", iter, EMPIRE_RANK(e, iter-1));
		
		// privs -- only shown to own empire
		if (is_own_empire) {
			found = FALSE;
			for (sub = 0; sub < NUM_PRIVILEGES; ++sub) {
				if (EMPIRE_PRIV(e, sub) == iter) {
					msg_to_char(ch, "%s%s", (found ? ", " : " - "), priv[sub]);
					found = TRUE;
				}
			}
		}
		
		msg_to_char(ch, "\r\n");
	}

	prettier_sprintbit(EMPIRE_FRONTIER_TRAITS(e), empire_trait_types, buf);
	msg_to_char(ch, "Frontier traits: %s\r\n", buf);
	msg_to_char(ch, "Population: %d player%s, %d citizen%s, %d military\r\n", EMPIRE_MEMBERS(e), (EMPIRE_MEMBERS(e) != 1 ? "s" : ""), EMPIRE_POPULATION(e), (EMPIRE_POPULATION(e) != 1 ? "s" : ""), EMPIRE_MILITARY(e));
	msg_to_char(ch, "Territory: %d in cities, %d/%d outside (%d/%d total)\r\n", EMPIRE_CITY_TERRITORY(e), EMPIRE_OUTSIDE_TERRITORY(e), land_can_claim(e, TRUE), EMPIRE_CITY_TERRITORY(e) + EMPIRE_OUTSIDE_TERRITORY(e), land_can_claim(e, FALSE));
	msg_to_char(ch, "Wealth: %d coin%s (at %d%%), %d treasure (%d total)\r\n", EMPIRE_COINS(e), (EMPIRE_COINS(e) != 1 ? "s" : ""), (int)(COIN_VALUE * 100), EMPIRE_WEALTH(e), GET_TOTAL_WEALTH(e));
	msg_to_char(ch, "Fame: %d\r\n", EMPIRE_FAME(e));
	msg_to_char(ch, "Greatness: %d\r\n", EMPIRE_GREATNESS(e));
	
	msg_to_char(ch, "Technology: ");
	for (iter = 0, comma = FALSE; iter < NUM_TECHS; ++iter) {
		if (EMPIRE_HAS_TECH(e, iter)) {
			msg_to_char(ch, "%s%s", (comma ? ", " : ""), techs[iter]);
			comma = TRUE;
		}
	}
	if (!comma) {
		msg_to_char(ch, "none");
	}
	msg_to_char(ch, "\r\n");
	
	// determine rank by iterating over the sorted empire list
	found_rank = 0;
	HASH_ITER(hh, empire_table, emp_iter, next_emp) {
		++found_rank;
		if (e == emp_iter) {
			break;
		}
	}

	msg_to_char(ch, "Score: %d, ranked #%d (", get_total_score(e), found_rank);
	for (iter = 0, comma = FALSE; iter < NUM_SCORES; ++iter) {
		sprinttype(iter, score_type, buf);
		msg_to_char(ch, "%s%s %d", (comma ? ", " : ""), buf, EMPIRE_SCORE(e, iter));
		comma = TRUE;
	}
	msg_to_char(ch, ")\r\n");

	// show war cost?
	if (GET_LOYALTY(ch) && GET_LOYALTY(ch) != e && !EMPIRE_IMM_ONLY(e) && !EMPIRE_IMM_ONLY(GET_LOYALTY(ch)) && !has_relationship(GET_LOYALTY(ch), e, DIPL_NONAGGR | DIPL_ALLIED)) {
		int war_cost = get_war_cost(GET_LOYALTY(ch), e);
		if (war_cost > 0) {
			msg_to_char(ch, "Cost to declare war on this empire: %d coin%s\r\n", war_cost, PLURAL(war_cost));
		}
	}

	// diplomacy
	if (EMPIRE_DIPLOMACY(e)) {
		msg_to_char(ch, "Diplomatic relations:\r\n");
	}

	// display political information by diplomacy type
	for (iter = 0; diplomacy_display[iter].type != NOTHING; ++iter) {
		if (!diplomacy_display[iter].offers_only) {
			found = FALSE;
			for (emp_pol = EMPIRE_DIPLOMACY(e); emp_pol; emp_pol = emp_pol->next) {
				if (IS_SET(emp_pol->type, diplomacy_display[iter].type) && (other = real_empire(emp_pol->id))) {
					if (!found) {
						msg_to_char(ch, "%s ", diplomacy_display[iter].text);
					}
				
					msg_to_char(ch, "%s%s%s&0%s", (found ? ", " : ""), EMPIRE_BANNER(other), EMPIRE_NAME(other), (IS_SET(emp_pol->type, DIPL_TRADE) ? " (trade)" : ""));
					found = TRUE;
				}
			}
			
			if (found) {
				msg_to_char(ch, ".\r\n");
			}
		}
	}

	// now show any open offers
	for (iter = 0; diplomacy_display[iter].type != NOTHING; ++iter) {
		found = FALSE;
		for (emp_pol = EMPIRE_DIPLOMACY(e); emp_pol; emp_pol = emp_pol->next) {
			// only show offers to members
			if (is_own_empire || (GET_LOYALTY(ch) && EMPIRE_VNUM(GET_LOYALTY(ch)) == emp_pol->id)) {
				if (IS_SET(emp_pol->offer, diplomacy_display[iter].type) && (other = real_empire(emp_pol->id))) {
					if (!found) {
						msg_to_char(ch, "Offering %s to ", diplomacy_display[iter].name);
					}
				
					msg_to_char(ch, "%s%s%s&0", (found ? ", " : ""), EMPIRE_BANNER(other), EMPIRE_NAME(other));
					found = TRUE;
				}
			}
		}
		
		if (found) {
			msg_to_char(ch, ".\r\n");
		}
	}

	// If it's your own empire, show open offers from others
	if (is_own_empire) {
		HASH_ITER(hh, empire_table, emp_iter, next_emp) {
			if (emp_iter != e) {
				for (emp_pol = EMPIRE_DIPLOMACY(emp_iter); emp_pol; emp_pol = emp_pol->next) {
					if (emp_pol->id == EMPIRE_VNUM(e) && emp_pol->offer) {
						found = FALSE;
						for (iter = 0; diplomacy_display[iter].type != NOTHING; ++iter) {
							if (IS_SET(emp_pol->offer, diplomacy_display[iter].type)) {
								// found a valid offer!
								if (!found) {
									msg_to_char(ch, "%s%s&0 offers ", EMPIRE_BANNER(emp_iter), EMPIRE_NAME(emp_iter));
								}
								
								msg_to_char(ch, "%s%s", (found ? ", " : ""), diplomacy_display[iter].name);
								found = TRUE;
							}
						}
						
						if (found) {
							msg_to_char(ch, ".\r\n");
						}
					}
				}
			}
		}
	}
}


/**
* called by do_empire_inventory to show einv
*
* @param char_data *ch The player requesting the einv.
* @param empire_data *emp The empire whose inventory to show.
* @param char *argument The requested inventory item, if any.
*/
static void show_empire_inventory_to_char(char_data *ch, empire_data *emp, char *argument) {
	// helper type
	struct einv_type {
		obj_vnum vnum;
		int local;
		int total;
		UT_hash_handle hh;
	};

	char output[MAX_STRING_LENGTH*2], line[MAX_STRING_LENGTH];
	struct einv_type *einv, *next_einv, *list = NULL;
	obj_vnum vnum, last_vnum = NOTHING;
	struct empire_storage_data *store;
	struct shipping_data *shipd;
	obj_data *proto = NULL;
	size_t lsize, size;
	bool all = FALSE, any = FALSE;
	
	if (!ch->desc) {
		return;
	}
	
	if (GET_ISLAND_ID(IN_ROOM(ch)) == NOTHING && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You can't check any empire inventory here.\r\n");
		return;
	}
	
	if (!str_cmp(argument, "all") || !strn_cmp(argument, "all ", 4)) {
		*argument = '\0';
		all = TRUE;
	}
	
	// build list
	for (store = EMPIRE_STORAGE(emp); store; store = store->next) {
		// prototype lookup
		if (store->vnum != last_vnum) {
			proto = obj_proto(store->vnum);
			last_vnum = store->vnum;
		}
		
		if (!proto) {
			continue;
		}
		
		// argument given but doesn't match
		if (*argument && !multi_isname(argument, GET_OBJ_KEYWORDS(proto))) {
			continue;
		}
		
		// ready to add
		vnum = store->vnum;
		HASH_FIND_INT(list, &vnum, einv);
		if (!einv) {
			CREATE(einv, struct einv_type, 1);
			einv->vnum = vnum;
			einv->local = einv->total = 0;
			HASH_ADD_INT(list, vnum, einv);
		}
		
		// add
		einv->total += store->amount;
		if (store->island == GET_ISLAND_ID(IN_ROOM(ch))) {
			einv->local += store->amount;
		}
	}
	
	// add shipping amounts to totals
	for (shipd = EMPIRE_SHIPPING_LIST(emp); shipd; shipd = shipd->next) {
		vnum = shipd->vnum;
		
		// have this?
		HASH_FIND_INT(list, &vnum, einv);
		if (einv) {
			einv->total += shipd->amount;
		}
	}
	
	// build output
	size = snprintf(output, sizeof(output), "Inventory of %s%s&0 on this island:\r\n", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
	
	HASH_ITER(hh, list, einv, next_einv) {
		// only display it if it's on the requested island, or if they requested it by name, or all
		if (all || einv->local > 0 || *argument) {
			if (einv->total > einv->local) {
				lsize = snprintf(line, sizeof(line), "(%4d) %s (%d total)\r\n", einv->local, get_obj_name_by_proto(einv->vnum), einv->total);
			}
			else {
				lsize = snprintf(line, sizeof(line), "(%4d) %s\r\n", einv->local, get_obj_name_by_proto(einv->vnum));
			}
			
			// append if room
			if (size + lsize < sizeof(output)) {
				size += lsize;
				strcat(output, line);
				any = TRUE;
			}
		}
		
		// clean up either way
		HASH_DEL(list, einv);
		free(einv);
	}
	
	if (!any) {
		size += snprintf(output + size, sizeof(output) - size, " nothing\r\n");
	}
	
	page_string(ch->desc, output, TRUE);
}


/**
* Finds workforce mobs belonging to an empire and reports on how many there
* are.
*
* @param empire_data *emp The empire to check.
* @param char_data *to The person to show the info to.
*/
void show_workforce_where(empire_data *emp, char_data *to) {
	// helper data type
	struct workforce_count_type {
		int chore;
		int count;
		UT_hash_handle hh;
	};

	struct workforce_count_type *find, *wct, *next_wct, *counts = NULL;
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	char_data *ch_iter;
	int chore, iter;
	size_t size;
	
	if (!emp) {
		msg_to_char(to, "No empire workforce found.\r\n");
		return;
	}
	
	// count up workforce mobs
	for (ch_iter = character_list; ch_iter; ch_iter = ch_iter->next) {
		if (!IS_NPC(ch_iter) || GET_LOYALTY(ch_iter) != emp) {
			continue;
		}
		
		chore = -1;
		for (iter = 0; iter < NUM_CHORES; ++iter) {
			if (GET_MOB_VNUM(ch_iter) == chore_data[iter].mob) {
				chore = iter;
				break;
			}
		}
		
		// not a workforce mob
		if (chore == -1) {
			continue;
		}
		
		HASH_FIND_INT(counts, &chore, find);
		if (!find) {
			CREATE(find, struct workforce_count_type, 1);
			find->chore = chore;
			find->count = 0;
			HASH_ADD_INT(counts, chore, find);
		}
		
		find->count += 1;
	}
	
	// short circuit: no workforce found
	if (!counts) {
		msg_to_char(to, "No working citizens found.\r\n");
		return;
	}
	
	size = snprintf(buf, sizeof(buf), "Working %s citizens:\r\n", EMPIRE_ADJECTIVE(emp));
	
	HASH_ITER(hh, counts, wct, next_wct) {
		// only bother adding if there's room in the buffer
		if (size < sizeof(buf) - 5) {
			snprintf(line, sizeof(line), "%s: %d worker%s\r\n", chore_data[wct->chore].name, wct->count, PLURAL(wct->count));
			size += snprintf(buf + size, sizeof(buf) - size, "%s", CAP(line));
		}
		else {
			size += snprintf(buf + size, sizeof(buf) - size, "...\r\n");
		}
		
		// remove and free
		HASH_DEL(counts, wct);
		free(wct);
	}
	
	page_string(to->desc, buf, TRUE);
}


/**
* Shows current workforce settings for an empire, to a character.
*
* @param empire_data *emp The empire whose settings to show.
* @param char_data *ch The person to show it to.
*/
void show_workforce_setup_to_char(empire_data *emp, char_data *ch) {
	int iter;
	
	if (!emp) {
		msg_to_char(ch, "No workforce is set up.\r\n");
	}
	
	msg_to_char(ch, "Workforce setup for %s%s&0:\r\n", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
	
	for (iter = 0; iter < NUM_CHORES; ++iter) {
		msg_to_char(ch, " %s %-15.15s%s", EMPIRE_CHORE(emp, iter) ? " &con&0" : "&yoff&0", chore_data[iter].name, !((iter+1)%3) ? "\r\n" : " ");
	}
	if (iter % 3) {
		msg_to_char(ch, "\r\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CITY HELPERS ////////////////////////////////////////////////////////////

void abandon_city(char_data *ch, char *argument) {	
	struct empire_city_data *city;
	empire_data *emp = GET_LOYALTY(ch);
	
	skip_spaces(&argument);
	
	// check legality
	if (!emp) {
		msg_to_char(ch, "You aren't a member of an empire.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_CITIES)) {
		msg_to_char(ch, "You don't have permission to abandon cities.\r\n");
		return;
	}
	if (!*argument) {
		msg_to_char(ch, "Usage: city abandon <name>\r\n");
		return;
	}
	if (!(city = find_city_by_name(emp, argument))) {
		msg_to_char(ch, "Your empire has no city by that name.\r\n");
		return;
	}
	
	log_to_empire(emp, ELOG_TERRITORY, "%s has abandoned %s", PERS(ch, ch, 1), city->name);
	perform_abandon_city(emp, city, TRUE);
	
	read_empire_territory(emp);
	save_empire(emp);
}


void city_traits(char_data *ch, char *argument) {	
	empire_data *emp = GET_LOYALTY(ch);
	struct empire_city_data *city;
	bitvector_t old;
	
	argument = any_one_word(argument, arg);
	skip_spaces(&argument);
	
	// check legality
	if (!emp) {
		msg_to_char(ch, "You aren't in an empire.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_CITIES)) {
		msg_to_char(ch, "You don't have permission to change cities.\r\n");
		return;
	}
	if (!*arg || !*argument) {
		msg_to_char(ch, "Usage: city traits <city> [add | remove] <traits>\r\n");
		return;
	}
	if (!(city = find_city_by_name(emp, arg))) {
		msg_to_char(ch, "Your empire has no city by that name.\r\n");
		return;
	}

	old = city->traits;
	city->traits = olc_process_flag(ch, argument, "city trait", NULL, empire_trait_types, city->traits);
	
	if (city->traits != old) {
		prettier_sprintbit(city->traits, empire_trait_types, buf);
		log_to_empire(emp, ELOG_ADMIN, "%s has changed city traits for %s to %s", PERS(ch, ch, 1), city->name, buf);
		save_empire(emp);
	}
}


void claim_city(char_data *ch, char *argument) {
	empire_data *emp = GET_LOYALTY(ch);
	struct empire_city_data *city;
	int x, y, radius;
	room_data *iter, *next_iter, *to_room, *center, *home;
	bool found = FALSE;
	
	skip_spaces(&argument);
	
	// check legality
	if (!emp) {
		msg_to_char(ch, "You aren't even in an empire.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_CLAIM)) {
		msg_to_char(ch, "You don't have permission to claim territory.\r\n");
		return;
	}
	if (!*argument) {
		msg_to_char(ch, "Usage: city claim <name>\r\n");
		return;
	}
	if (!(city = find_city_by_name(emp, argument))) {
		msg_to_char(ch, "Your empire has no city by that name.\r\n");
		return;
	}
	if (!can_claim(ch)) {
		msg_to_char(ch, "You can't claim any more land.\r\n");
		return;
	}

	center = city->location;
	radius = city_type[city->type].radius;
	for (x = -1 * radius; x <= radius; ++x) {
		for (y = -1 * radius; y <= radius && can_claim(ch); ++y) {
			to_room = real_shift(center, x, y);
			
			if (to_room && SECT(to_room) != ROOM_ORIGINAL_SECT(to_room) && !ROOM_OWNER(to_room) && !ROOM_AFF_FLAGGED(to_room, ROOM_AFF_HAS_INSTANCE)) {
				found = TRUE;
				claim_room(to_room, emp);
				
				// increment territory now so can_claim updates
				EMPIRE_CITY_TERRITORY(emp) += 1;
			}
		}
	}
	
	if (found) {
		// update the inside (interior rooms only)
		HASH_ITER(interior_hh, interior_world_table, iter, next_iter) {
			home = HOME_ROOM(iter);
			if (home != iter && ROOM_OWNER(home) == emp) {
				ROOM_OWNER(iter) = emp;
			}
		}
				
		msg_to_char(ch, "You have claimed all unclaimed structures in the %s of %s.\r\n", city_type[city->type].name, city->name);
		read_empire_territory(emp);
		save_empire(emp);
	}
	else {
		msg_to_char(ch, "The %s of %s has no unclaimed structures.\r\n", city_type[city->type].name, city->name);
	}
}


void downgrade_city(char_data *ch, char *argument) {
	empire_data *emp = GET_LOYALTY(ch);
	struct empire_city_data *city;
	
	skip_spaces(&argument);
	
	// check legality
	if (!emp) {
		msg_to_char(ch, "You can't downgrade any cities right now.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_CITIES)) {
		msg_to_char(ch, "You don't have permission to downgrade cities.\r\n");
		return;
	}
	if (!*argument) {
		msg_to_char(ch, "Usage: city downgrade <name>\r\n");
		return;
	}
	if (!(city = find_city_by_name(emp, argument))) {
		msg_to_char(ch, "Your empire has no city by that name.\r\n");
		return;
	}

	if (city->type > 0) {
		city->type--;
		log_to_empire(emp, ELOG_TERRITORY, "%s has downgraded %s to a %s", PERS(ch, ch, 1), city->name, city_type[city->type].name);
	}
	else {
		log_to_empire(emp, ELOG_TERRITORY, "%s has downgraded %s - it is no longer a city", PERS(ch, ch, 1), city->name);
		perform_abandon_city(emp, city, FALSE);
	}
	
	save_empire(emp);
	read_empire_territory(emp);
}


void found_city(char_data *ch, char *argument) {
	extern struct empire_city_data *create_city_entry(empire_data *emp, char *name, room_data *location, int type);
	void stop_room_action(room_data *room, int action, int chore);
	extern int num_of_start_locs;
	extern int *start_locs;
	
	empire_data *emp = get_or_create_empire(ch);
	empire_data *emp_iter, *next_emp;
	int iter, dist;
	struct empire_city_data *city;
	
	// flags which block city found
	bitvector_t nocity_flags = SECTF_ADVENTURE | SECTF_NON_ISLAND | SECTF_NO_CLAIM | SECTF_START_LOCATION | SECTF_IS_ROAD | SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_HAS_CROP_DATA | SECTF_CROP | SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_IS_TRENCH | SECTF_SHALLOW_WATER;
	
	int min_distance_between_ally_cities = config_get_int("min_distance_between_ally_cities");
	int min_distance_between_cities = config_get_int("min_distance_between_cities");
	int min_distance_from_city_to_starting_location = config_get_int("min_distance_from_city_to_starting_location");
	
	skip_spaces(&argument);
	
	// check legality
	if (!emp || city_points_available(emp) < 1) {
		msg_to_char(ch, "You can't found any cities right now.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_CITIES)) {
		msg_to_char(ch, "You don't have permission to found cities.\r\n");
		return;
	}
	if (ROOM_IS_CLOSED(IN_ROOM(ch)) || COMPLEX_DATA(IN_ROOM(ch)) || IS_WATER_SECT(SECT(IN_ROOM(ch))) || ROOM_SECT_FLAGGED(IN_ROOM(ch), nocity_flags)) {
		msg_to_char(ch, "You can't found a city on this type of terrain.\r\n");
		return;
	}
	if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You can't found a city here! You don't even own the territory.\r\n");
		return;
	}
	
	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't do that here.\r\n");
		return;
	}

	// check starting locations
	for (iter = 0; iter <= num_of_start_locs; ++iter) {
		if (compute_distance(IN_ROOM(ch), real_room(start_locs[iter])) < min_distance_from_city_to_starting_location) {
			msg_to_char(ch, "You can't found a city within %d tiles of a starting location.\r\n", min_distance_from_city_to_starting_location);
			return;
		}
	}
	
	// check city proximity
	HASH_ITER(hh, empire_table, emp_iter, next_emp) {
		if (emp_iter != emp) {
			if ((city = find_closest_city(emp_iter, IN_ROOM(ch)))) {
				dist = compute_distance(IN_ROOM(ch), city->location);

				if (dist < min_distance_between_cities) {
					// are they allied?
					if (has_relationship(emp, emp_iter, DIPL_ALLIED)) {
						// lower minimum
						if (dist < min_distance_between_ally_cities) {
							msg_to_char(ch, "You can't found a city here. It's within %d tiles of the center of %s.\r\n", min_distance_between_ally_cities, city->name);
							return;
						}
					}
					else {
						msg_to_char(ch, "You can't found a city here. It's within %d tiles of the center of %s.\r\n", min_distance_between_cities, city->name);
						return;
					}
				}
			}
		}
	}
	
	// check argument
	if (!*argument) {
		msg_to_char(ch, "Usage: city found <name>\r\n");
		return;
	}
	if (count_color_codes(argument) > 0) {
		msg_to_char(ch, "City names may not contain color codes.\r\n");
		return;
	}
	if (strlen(argument) > 30) {
		msg_to_char(ch, "That name is too long.\r\n");
		return;
	}
	if (strchr(argument, '"')) {
		msg_to_char(ch, "City names may not include quotation marks (\").\r\n");
		return;
	}
	if (strchr(argument, '%')) {
		msg_to_char(ch, "City names may not include the percent sign (%%).\r\n");
		return;
	}
	
	// ok create it -- this will take care of the buildings and everything
	if (!(city = create_city_entry(emp, argument, IN_ROOM(ch), 0))) {
		msg_to_char(ch, "Unable to found a city here -- unknown error.\r\n");
		return;
	}
	
	if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
		log_to_empire(emp, ELOG_TERRITORY, "%s has founded %s at (%d, %d)", PERS(ch, ch, 1), city->name, X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
	}
	else {
		log_to_empire(emp, ELOG_TERRITORY, "%s has founded %s", PERS(ch, ch, 1), city->name);
	}
	
	stop_room_action(IN_ROOM(ch), ACT_CHOPPING, CHORE_CHOPPING);
	stop_room_action(IN_ROOM(ch), ACT_PICKING, CHORE_FARMING);
	stop_room_action(IN_ROOM(ch), ACT_DIGGING, NOTHING);
	stop_room_action(IN_ROOM(ch), ACT_EXCAVATING, NOTHING);
	stop_room_action(IN_ROOM(ch), ACT_FILLING_IN, NOTHING);
	stop_room_action(IN_ROOM(ch), ACT_GATHERING, NOTHING);
	stop_room_action(IN_ROOM(ch), ACT_HARVESTING, NOTHING);
	stop_room_action(IN_ROOM(ch), ACT_PLANTING, NOTHING);
	
	read_empire_territory(emp);
	save_empire(emp);
}


/**
* Determines if a location counts as in-city, and has passed the required wait
* time since the city was founded.
*
* @param room_data *loc The location to check.
* @param empire_data *emp The empire to check.
* @param bool check_wait If TRUE, requires the city wait time to have passed.
* @param bool *too_soon Will be set to TRUE if there was a city but it was founded too recently.
* @return bool TRUE if in-city, FALSE if not.
*/
bool is_in_city_for_empire(room_data *loc, empire_data *emp, bool check_wait, bool *too_soon) {
	struct empire_city_data *city;
	int dist;
	
	int wait = config_get_int("minutes_to_full_city") * SECS_PER_REAL_MIN;
	bool large_radius = (ROOM_BLD_FLAGGED(loc, BLD_LARGE_CITY_RADIUS) || ROOM_SECT_FLAGGED(loc, SECTF_LARGE_CITY_RADIUS));
	
	*too_soon = FALSE;

	if (!emp) {
		return FALSE;
	}
	
	for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
		dist = compute_distance(loc, city->location);
		
		if (dist <= city_type[city->type].radius || (large_radius && dist <= (3 * city_type[city->type].radius))) {
			if (!check_wait || (get_room_extra_data(city->location, ROOM_EXTRA_FOUND_TIME) + wait) < time(0)) {
				return TRUE;
			}
			else {
				*too_soon = TRUE;
			}
		}
	}
	
	return FALSE;
}


// for do_city
void list_cities(char_data *ch, char *argument) {
	extern int count_city_points_used(empire_data *emp);
	
	struct empire_city_data *city;
	struct island_info *isle;
	empire_data *emp;
	int points, used, count;
	bool pending, found = FALSE;
	room_data *rl;
	
	any_one_word(argument, arg);
	
	if (*arg && (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES))) {
		emp = get_empire_by_name(arg);
		if (!emp) {
			msg_to_char(ch, "Unknown empire.\r\n");
			return;
		}
	}
	else if ((emp = GET_LOYALTY(ch)) == NULL) {
		msg_to_char(ch, "You're not in an empire.\r\n");
		return;
	}

	used = count_city_points_used(emp);
	points = city_points_available(emp);
	msg_to_char(ch, "%s cities (%d/%d city point%s):\r\n", EMPIRE_ADJECTIVE(emp), used, (points + used), ((points + used) != 1 ? "s" : ""));
	
	count = 0;
	for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
		found = TRUE;
		rl = city->location;
		prettier_sprintbit(city->traits, empire_trait_types, buf);
		isle = get_island(GET_ISLAND_ID(rl), TRUE);
		
		pending = (get_room_extra_data(city->location, ROOM_EXTRA_FOUND_TIME) + (config_get_int("minutes_to_full_city") * SECS_PER_REAL_MIN) > time(0));			
		msg_to_char(ch, "%d. (%*d, %*d) %s, on %s (%s/%d), traits: %s%s\r\n", ++count, X_PRECISION, X_COORD(rl), Y_PRECISION, Y_COORD(rl), city->name, isle->name, city_type[city->type].name, city_type[city->type].radius, buf, pending ? " &r(new)&0" : "");
	}
	
	if (!found) {
		msg_to_char(ch, "  none\r\n");
	}
	
	if (points > 0) {
		msg_to_char(ch, "* The empire has %d city point%s available.\r\n", points, (points != 1 ? "s" : ""));
	}
}


/**
* Removes a city from an empire, and (optionally) abandons the entire thing.
*
* @param empire_data *emp The owner.
* @param struct empire_city_data *city The city to abandon.
* @param bool full_abandon If TRUE, abandons the city's entire area.
*/
void perform_abandon_city(empire_data *emp, struct empire_city_data *city, bool full_abandon) {
	struct empire_city_data *temp;
	room_data *cityloc, *to_room;
	int x, y, radius;
	bool junk;
	
	// store location & radius now
	cityloc = city->location;
	radius = city_type[city->type].radius;
	
	// delete the city
	if (IS_CITY_CENTER(cityloc)) {
		disassociate_building(cityloc);
	}
	REMOVE_FROM_LIST(city, EMPIRE_CITY_LIST(emp), next);
	if (city->name) {
		free(city->name);
	}
	free(city);
	
	if (full_abandon) {
		// abandon the radius
		for (x = -1 * radius; x <= radius; ++x) {
			for (y = -1 * radius; y <= radius; ++y) {
				to_room = real_shift(cityloc, x, y);
			
				// check ownership
				if (to_room && ROOM_OWNER(to_room) == emp) {
					// warning: never abandon things that are still within another city
					if (!is_in_city_for_empire(to_room, emp, FALSE, &junk)) {
						// check if ACTUALLY within the abandoned city
						if (compute_distance(cityloc, to_room) <= radius) {
							abandon_room(to_room);
						}
					}
				}
			}
		}
	}
	else {
		abandon_room(cityloc);
	}
}


void rename_city(char_data *ch, char *argument) {
	char newname[MAX_INPUT_LENGTH];
	empire_data *emp = GET_LOYALTY(ch);
	struct empire_city_data *city;

	half_chop(argument, arg, newname);
	
	// check legality
	if (!emp) {
		msg_to_char(ch, "You aren't in an empire.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_CITIES)) {
		msg_to_char(ch, "You don't have permission to rename cities.\r\n");
		return;
	}
	if (!*arg || !*newname) {
		msg_to_char(ch, "Usage: city rename <number> <new name>\r\n");
		return;
	}
	if (!(city = find_city_by_name(emp, arg))) {
		msg_to_char(ch, "Your empire has no city by that name.\r\n");
		return;
	}
	if (count_color_codes(newname) > 0) {
		msg_to_char(ch, "City names may not contain color codes.\r\n");
		return;
	}
	if (strlen(newname) > 30) {
		msg_to_char(ch, "That name is too long.\r\n");
		return;
	}
	
	log_to_empire(emp, ELOG_TERRITORY, "%s has renamed %s to %s", PERS(ch, ch, 1), city->name, newname);
	
	free(city->name);
	city->name = str_dup(newname);
	save_empire(emp);
}


void upgrade_city(char_data *ch, char *argument) {	
	empire_data *emp = GET_LOYALTY(ch);
	struct empire_city_data *city;
	
	skip_spaces(&argument);
	
	// check legality
	if (!emp || city_points_available(emp) < 1) {
		msg_to_char(ch, "You can't upgrade any cities right now.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_CITIES)) {
		msg_to_char(ch, "You don't have permission to upgrade cities.\r\n");
		return;
	}
	if (!*argument) {
		msg_to_char(ch, "Usage: city upgrade <name>\r\n");
		return;
	}
	if (!(city = find_city_by_name(emp, argument))) {
		msg_to_char(ch, "Your empire has no city by that name.\r\n");
		return;
	}
	if (*city_type[city->type+1].name == '\n') {
		msg_to_char(ch, "That city is already at the maximum level.\r\n");
		return;
	}
	
	city->type++;
	
	log_to_empire(emp, ELOG_TERRITORY, "%s has upgraded %s to a %s", PERS(ch, ch, 1), city->name, city_type[city->type].name);
	read_empire_territory(emp);
	save_empire(emp);
}


 //////////////////////////////////////////////////////////////////////////////
//// EFIND HELPERS ///////////////////////////////////////////////////////////

// for better efind
struct efind_group {
	room_data *location;	// where
	obj_data *obj;	// 1st object for this set (used for names, etc)
	int count;	// how many found
	bool stackable;	// whether or not this can stack
	
	struct efind_group *next;
};


// simple increment/add function for managing efind groups
void add_obj_to_efind(struct efind_group **list, obj_data *obj, room_data *location) {
	struct efind_group *eg, *temp;
	bool found = FALSE;
	
	if (OBJ_CAN_STACK(obj)) {
		for (eg = *list; !found && eg; eg = eg->next) {
			if (eg->location == location && eg->stackable && GET_OBJ_VNUM(eg->obj) == GET_OBJ_VNUM(obj)) {
				eg->count += 1;
				found = TRUE;
			}
		}
	}
	
	if (!found) {
		CREATE(eg, struct efind_group, 1);
		
		eg->location = location;
		eg->obj = obj;
		eg->count = 1;
		eg->stackable = OBJ_CAN_STACK(obj);
		eg->next = NULL;
		
		if (*list) {
			// add to end
			temp = *list;
			while (temp->next) {
				temp = temp->next;
			}
			temp->next = eg;
		}
		else {
			// empty list
			*list = eg;
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// IMPORT / EXPORT HELPERS /////////////////////////////////////////////////

// adds import/export entry for do_import
void do_import_add(char_data *ch, empire_data *emp, char *argument, int subcmd) {
	void sort_trade_data(struct empire_trade_data **list);

	char cost_arg[MAX_INPUT_LENGTH], limit_arg[MAX_INPUT_LENGTH];
	struct empire_trade_data *trade;
	int cost, limit;
	obj_vnum vnum = NOTHING;
	obj_data *obj;
	
	// 3 args
	argument = one_argument(argument, cost_arg);
	argument = one_argument(argument, limit_arg);
	skip_spaces(&argument);	// remaining arg is item name
	
	// for later
	cost = atoi(cost_arg);
	limit = atoi(limit_arg);
	
	if (!*cost_arg || !*limit_arg || !*argument) {
		msg_to_char(ch, "Usage: %s add <%scost> <%s limit> <item name>\r\n", trade_type[subcmd], trade_mostleast[subcmd], trade_overunder[subcmd]);
	}
	else if (!isdigit(*cost_arg)) {
		msg_to_char(ch, "Cost must be a number, '%s' given.\r\n", cost_arg);
	}
	else if (!isdigit(*limit_arg)) {
		msg_to_char(ch, "Limit must be a number, '%s' given.\r\n", limit_arg);
	}
	else if (cost < 1 || cost > MAX_COIN) {
		msg_to_char(ch, "That cost is out of bounds.\r\n");
	}
	else if (limit < 0 || limit > MAX_COIN) {
		// max coin is a safe limit here
		msg_to_char(ch, "That limit is out of bounds.\r\n");
	}
	else if (((obj = get_obj_in_list_vis(ch, argument, ch->carrying)) || (obj = get_obj_in_list_vis(ch, argument, ROOM_CONTENTS(IN_ROOM(ch))))) && (vnum = GET_OBJ_VNUM(obj)) == NOTHING) {
		// targeting an item in room/inventory
		act("$p can't be traded.", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else if (vnum == NOTHING && (vnum = get_obj_vnum_by_name(argument)) == NOTHING) {
		msg_to_char(ch, "Unknown item '%s'.\r\n", argument);
	}
	else {
		// find or create matching entry
		if (!(trade = find_trade_entry(emp, subcmd, vnum))) {
			CREATE(trade, struct empire_trade_data, 1);
			trade->type = subcmd;
			trade->vnum = vnum;
			
			// add anywhere; sort later
			trade->next = EMPIRE_TRADE(emp);
			EMPIRE_TRADE(emp) = trade;
		}
		
		// update values
		trade->cost = cost;
		trade->limit = limit;
		
		sort_trade_data(&EMPIRE_TRADE(emp));
		save_empire(emp);
		
		msg_to_char(ch, "%s now %ss '%s' costing %s%d, when %s %d.\r\n", EMPIRE_NAME(emp), trade_type[subcmd], get_obj_name_by_proto(vnum), trade_mostleast[subcmd], cost, trade_overunder[subcmd], limit);
	}
}


// removes an import/export entry for do_import
void do_import_remove(char_data *ch, empire_data *emp, char *argument, int subcmd) {
	struct empire_trade_data *trade, *temp;
	obj_vnum vnum = NOTHING;
	obj_data *obj;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: %s remove <name>\r\n", trade_type[subcmd]);
	}
	else if (((obj = get_obj_in_list_vis(ch, argument, ch->carrying)) || (obj = get_obj_in_list_vis(ch, argument, ROOM_CONTENTS(IN_ROOM(ch))))) && (vnum = GET_OBJ_VNUM(obj)) == NOTHING) {
		// targeting an item in room/inventory
		act("$p can't be traded.", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else if (vnum == NOTHING && (vnum = get_obj_vnum_by_name(argument)) == NOTHING) {
		msg_to_char(ch, "Unknown item '%s'.\r\n", argument);
	}
	else if (!(trade = find_trade_entry(emp, subcmd, vnum))) {
		msg_to_char(ch, "The empire doesn't appear to be %sing '%s'.\r\n", trade_type[subcmd], get_obj_name_by_proto(vnum));
	}
	else {
		REMOVE_FROM_LIST(trade, EMPIRE_TRADE(emp), next);
		free(trade);
		save_empire(emp);
		
		msg_to_char(ch, "%s no longer %ss '%s'.\r\n", EMPIRE_NAME(emp), trade_type[subcmd], get_obj_name_by_proto(vnum));
	}
}


// lists curent import/exports for do_import
void do_import_list(char_data *ch, empire_data *emp, char *argument, int subcmd) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], coin_conv[256], indicator[256], over_part[256];
	empire_data *partner = NULL, *use_emp = emp;
	struct empire_trade_data *trade;
	int haveamt, count = 0, use_type = subcmd;
	double rate = 1.0;
	obj_data *proto;
	
	// TRADE_x
	char *fromto[] = { "to", "from" };
	
	if (*argument && !(partner = get_empire_by_name(argument))) {
		msg_to_char(ch, "Invalid empire.\r\n");
		return;
	}
	
	// two different things we can show here:
	*buf = '\0';
	
	if (!partner) {
		// show our own imports/exports based on type
		use_emp = emp;
		use_type = subcmd;
		sprintf(buf, "%s is %sing:\r\n", EMPIRE_NAME(use_emp), trade_type[subcmd]);
	}
	else {
		// show what we can import/export from a partner based on type
		use_emp = partner;
		use_type = (subcmd == TRADE_IMPORT ? TRADE_EXPORT : TRADE_IMPORT);
		if (subcmd == TRADE_IMPORT) {
			rate = 1/exchange_rate(emp, partner);
		}
		else {
			rate = exchange_rate(partner, emp);
		}
		sprintf(buf, "You can %s %s %s:\r\n", trade_type[subcmd] /* not use_type */, fromto[subcmd], EMPIRE_NAME(use_emp));
	}
	
	// build actual list
	*coin_conv = '\0';
	*indicator = '\0';
	*over_part = '\0';
	for (trade = EMPIRE_TRADE(use_emp); trade; trade = trade->next) {
		if (trade->type == use_type && (proto = obj_proto(trade->vnum))) {
			++count;
			
			// figure out actual cost
			if (rate != 1.0 && ((int)(rate * 10))%10 == 0) {
				snprintf(coin_conv, sizeof(coin_conv), " (%d)", (int)round(trade->cost * rate));
			}
			else if (rate != 1.0) {
				snprintf(coin_conv, sizeof(coin_conv), " (%.1f)", trade->cost * rate);
			}
			
			// figure out indicator
			haveamt = get_total_stored_count(use_emp, trade->vnum, TRUE);
			if (use_type == TRADE_IMPORT && haveamt >= trade->limit) {
				strcpy(indicator, " &r(full)&0");
			}
			else if (use_type == TRADE_EXPORT && haveamt <= trade->limit) {
				strcpy(indicator, " &r(out)&0");
			}
			else {
				*indicator = '\0';
			}
			
			if (emp == use_emp) {
				sprintf(over_part, " when %s &g%d&0", trade_overunder[use_type], trade->limit);
			}
			
			snprintf(line, sizeof(line), " &c%s&0 for %s&y%d%s&0 coin%s%s%s\r\n", GET_OBJ_SHORT_DESC(proto), trade_mostleast[use_type], trade->cost, coin_conv, (trade->cost != 1 ? "s" : ""), over_part, indicator);
			if (strlen(buf) + strlen(line) < MAX_STRING_LENGTH + 12) {
				strcat(buf, line);
			}
			else {
				strcat(buf, " and more\r\n");	// strcat: OK (+12 saved room)
			}
		}
	}
	if (count == 0) {
		strcat(buf, " nothing\r\n");
	}
	
	if (*buf) {
		page_string(ch->desc, buf, TRUE);
	}
}


// analysis of an item for import/export for do_import
void do_import_analysis(char_data *ch, empire_data *emp, char *argument, int subcmd) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], coin_conv[256];
	struct empire_trade_data *trade;
	empire_data *iter, *next_iter;
	int haveamt, find_type = (subcmd == TRADE_IMPORT ? TRADE_EXPORT : TRADE_IMPORT);
	bool has_avail, is_buying, found = FALSE;
	double rate;
	obj_vnum vnum = NOTHING;
	obj_data *obj;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: %s analyze <item name>\r\n", trade_type[subcmd]);
	}
	else if (((obj = get_obj_in_list_vis(ch, argument, ch->carrying)) || (obj = get_obj_in_list_vis(ch, argument, ROOM_CONTENTS(IN_ROOM(ch))))) && (vnum = GET_OBJ_VNUM(obj)) == NOTHING) {
		// targeting an item in room/inventory
		act("$p can't be traded.", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else if (vnum == NOTHING && (vnum = get_obj_vnum_by_name(argument)) == NOTHING) {
		msg_to_char(ch, "Unknown item '%s'.\r\n", argument);
	}
	else {
		sprintf(buf, "Analysis of %s:\r\n", get_obj_name_by_proto(vnum));
		
		// vnum is valid (obj was a throwaway)
		HASH_ITER(hh, empire_table, iter, next_iter) {
			// success! now, are they importing/exporting it?
			if (!(trade = find_trade_entry(iter, find_type, vnum))) {
				continue;
			}
			
			haveamt = get_total_stored_count(iter, trade->vnum, TRUE);
			has_avail = (find_type == TRADE_EXPORT) ? (haveamt > trade->limit) : FALSE;
			is_buying = (find_type == TRADE_IMPORT) ? (haveamt < trade->limit) : FALSE;
			rate = exchange_rate(iter, emp);
			
			// flip for export
			if (find_type == TRADE_EXPORT) {
				rate = 1/rate;
			}
			
			// figure out actual cost
			if (rate != 1.0 && ((int)(rate * 10))%10 == 0) {
				snprintf(coin_conv, sizeof(coin_conv), " (%d)", (int)round(trade->cost * rate));
			}
			else if (rate != 1.0) {
				snprintf(coin_conv, sizeof(coin_conv), " (%.1f)", trade->cost * rate);
			}
			else {
				*coin_conv = '\0';
			}
			
			sprintf(line, " %s (%d%%) is %sing it at &y%d%s coin%s&0%s%s%s\r\n", EMPIRE_NAME(iter), (int)(100 * rate), trade_type[find_type], trade->cost, coin_conv, (trade->cost != 1 ? "s" : ""), ((find_type != TRADE_EXPORT || has_avail) ? "" : " (none available)"), ((find_type != TRADE_IMPORT || is_buying) ? "" : " (full)"), (is_trading_with(emp, iter) ? "" : " (not trading)"));
			found = TRUE;
			
			if (strlen(buf) + strlen(line) < MAX_STRING_LENGTH + 15) {
				strcat(buf, line);
			}
			else {
				strcat(buf, " ...and more\r\n");
				break;
			}
		}
		
		if (!found) {
			sprintf(buf + strlen(buf), " Nobody is %sing it.\r\n", trade_type[find_type]);
		}
		
		page_string(ch->desc, buf, TRUE);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// INSPIRE HELPERS /////////////////////////////////////////////////////////

struct {
	char *name;
	int first_apply;
	int second_apply;
} inspire_data[] = {
								// use APPLY_NONE to give no second bonus
	{ "accuracy", APPLY_TO_HIT, APPLY_NONE },
	{ "evasion", APPLY_DODGE, APPLY_NONE },
	{ "mana", APPLY_MANA, APPLY_MANA_REGEN },
	{ "might", APPLY_STRENGTH, APPLY_BONUS_PHYSICAL },
	{ "prowess", APPLY_DEXTERITY, APPLY_WITS },
	{ "sorcery", APPLY_INTELLIGENCE, APPLY_BONUS_MAGICAL },
	{ "stamina", APPLY_MOVE, APPLY_MOVE_REGEN },
	{ "toughness", APPLY_HEALTH, APPLY_NONE, },

	{ "\n", APPLY_NONE, APPLY_NONE }
};


/**
* @param char *input The name typed by the player
* @return int an inspire_data index, or NOTHING for not found
*/
int find_inspire(char *input) {
	int iter, abbrev = NOTHING;
	
	for (iter = 0; *inspire_data[iter].name != '\n'; ++iter) {
		if (!str_cmp(input, inspire_data[iter].name)) {
			// found exact match
			return iter;
		}
		else if (abbrev == NOTHING && is_abbrev(input, inspire_data[iter].name)) {
			abbrev= iter;
		}
	}

	return abbrev;	// if any
}


/**
* Apply the actual inspiration.
*
* @param char_data *ch the person using the ability
* @param char_data *vict the inspired one
* @param int type The inspire_data index
*/
void perform_inspire(char_data *ch, char_data *vict, int type) {
	extern const double apply_values[];
	
	double points, any = FALSE;
	struct affected_type *af;
	int time, value;
	bool two;
	
	affect_from_char(vict, ATYPE_INSPIRE);
	
	if (ch == vict || (GET_LOYALTY(ch) && GET_LOYALTY(ch) == GET_LOYALTY(vict))) {
		time = 24 MUD_HOURS;
	}
	else {
		time = 4 MUD_HOURS;
	}
	
	// amount to give
	points = GET_GREATNESS(ch) / 4.0;
	if (ch == vict) {
		points /= 2.0;
	}
	two = (inspire_data[type].second_apply != APPLY_NONE);
	
	if (inspire_data[type].first_apply != APPLY_NONE) {
		value = round((points * (two ? 0.5 : 1.0)) / apply_values[inspire_data[type].first_apply]);
		if (value > 0) {
			af = create_mod_aff(ATYPE_INSPIRE, time, inspire_data[type].first_apply, value, ch);
			affect_join(vict, af, 0);
			any = TRUE;
		}
	}
	if (inspire_data[type].second_apply != APPLY_NONE) {
		value = round((points * 0.5) / apply_values[inspire_data[type].second_apply]);
		if (value > 0) {
			af = create_mod_aff(ATYPE_INSPIRE, time, inspire_data[type].second_apply, value, ch);
			affect_join(vict, af, 0);
			any = TRUE;
		}
	}
	
	if (any) {
		msg_to_char(vict, "You feel inspired!\r\n");
		act("$n seems inspired!", FALSE, vict, NULL, NULL, TO_ROOM);
		gain_ability_exp(ch, ABIL_INSPIRE, 33.4);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ISLANDS HELPERS /////////////////////////////////////////////////////////

// helper data for do_islands
struct do_islands_data {
	int id;
	bool has_territory;
	int einv_size;
	UT_hash_handle hh;
};


/**
* Helper for do_islands: adds to einv count.
*
* @param struct do_islands_data **list Pointer to a do_islands hash.
* @param int island_id Which island.
* @param int amount How much einv to add.
*/
void do_islands_add_einv(struct do_islands_data **list, int island_id, int amount) {
	struct do_islands_data *isle;
	
	HASH_FIND_INT(*list, &island_id, isle);
	if (!isle) {
		CREATE(isle, struct do_islands_data, 1);
		isle->id = island_id;
		HASH_ADD_INT(*list, id, isle);
	}
	SAFE_ADD(isle->einv_size, amount, INT_MIN, INT_MAX, TRUE);
}


/**
* Helper for do_islands: marks territory on the island.
*
* @param struct do_islands_data **list Pointer to a do_islands hash.
* @param int island_id Which island.
*/
void do_islands_has_territory(struct do_islands_data **list, int island_id) {
	struct do_islands_data *isle;
	
	HASH_FIND_INT(*list, &island_id, isle);
	if (!isle) {
		CREATE(isle, struct do_islands_data, 1);
		isle->id = island_id;
		HASH_ADD_INT(*list, id, isle);
	}
	isle->has_territory = TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// TAVERN HELPERS //////////////////////////////////////////////////////////

// for do_tavern, BREW_x
const struct tavern_data_type tavern_data[] = {
	{ "nothing", 0, { NOTHING } },
	{ "ale", LIQ_ALE, { o_BARLEY, o_HOPS, NOTHING } },
	{ "lager", LIQ_LAGER, { o_CORN, o_HOPS, NOTHING } },
	{ "wheatbeer", LIQ_WHEATBEER, { o_WHEAT, o_BARLEY, NOTHING } },
	{ "cider", LIQ_CIDER, { o_APPLES, o_PEACHES, NOTHING } },
	
	{ "\n", 0, { NOTHING } }
};


/**
* @param room_data *room Tavern
* @return TRUE if it was able to get resources from an empire, FALSE if not
*/
bool extract_tavern_resources(room_data *room) {
	struct empire_storage_data *store;
	empire_data *emp = ROOM_OWNER(room);
	int type = get_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE);
	int iter;
	bool ok;
	
	int cost = 5;
	
	if (!emp) {
		return FALSE;
	}
	
	// check if they can afford it
	ok = TRUE;
	for (iter = 0; ok && tavern_data[type].ingredients[iter] != NOTHING; ++iter) {
		store = find_stored_resource(emp, GET_ISLAND_ID(room), tavern_data[type].ingredients[iter]);
		
		if (!store || store->amount < cost) {
			ok = FALSE;
		}
	}
	
	// extract resources
	if (ok) {
		for (iter = 0; tavern_data[type].ingredients[iter] != NOTHING; ++iter) {
			charge_stored_resource(emp, GET_ISLAND_ID(room), tavern_data[type].ingredients[iter], cost);
		}
	}
	
	return ok;
}


/**
* Displays all the empire's taverns to char.
*
* @param char_data *ch The person to display to.
*/
void show_tavern_status(char_data *ch) {
	empire_data *emp = GET_LOYALTY(ch);
	struct empire_territory_data *ter;
	bool found = FALSE;
	
	if (!emp) {
		return;
	}
	
	msg_to_char(ch, "Your taverns:\r\n");
	
	for (ter = EMPIRE_TERRITORY_LIST(emp); ter; ter = ter->next) {
		if (ROOM_BLD_FLAGGED(ter->room, BLD_TAVERN)) {
			found = TRUE;
			
			if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
				msg_to_char(ch, "(%*d, %*d) %s : %s\r\n", X_PRECISION, X_COORD(ter->room), Y_PRECISION, Y_COORD(ter->room), get_room_name(ter->room, FALSE), tavern_data[get_room_extra_data(ter->room, ROOM_EXTRA_TAVERN_TYPE)].name);
			}
			else {
				msg_to_char(ch, " %s: %s\r\n", get_room_name(ter->room, FALSE), tavern_data[get_room_extra_data(ter->room, ROOM_EXTRA_TAVERN_TYPE)].name);
			}
		}
	}
	
	if (!found) {
		msg_to_char(ch, " none\r\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// TERRITORY HELPERS ///////////////////////////////////////////////////////

// for do_findmaintenance and do_territory
struct find_territory_node {
	room_data *loc;
	int count;
	
	struct find_territory_node *next;
};


/**
* @param struct find_territory_node *list The node list to count.
* @return int The number of elements in the list.
*/
int count_node_list(struct find_territory_node *list) {
	struct find_territory_node *node;
	int count = 0;
	
	for (node = list; node; node = node->next) {
		++count;
	}
	
	return count;
}


// sees if the room is within 10 spaces of any existing node
static struct find_territory_node *find_nearby_territory_node(room_data *room, struct find_territory_node *list, int distance) {
	struct find_territory_node *node, *found = NULL;
	
	for (node = list; node && !found; node = node->next) {
		if (compute_distance(room, node->loc) <= distance) {
			found = node;
		}
	}
	
	return found;
}


/**
* This reduces the territory node list to at most some pre-defined number, so
* that there aren't too many to display.
*
* @param struct find_territory_node *list The incoming list.
* @return struct find_territory_node* The new, reduced list.
*/
struct find_territory_node *reduce_territory_node_list(struct find_territory_node *list) {
	struct find_territory_node *node, *next_node, *find, *temp;
	int size = 10;
	
	// iterate until there are no more than 44 nodes
	while (count_node_list(list) > 44) {
		for (node = list; node && node->next; node = next_node) {
			next_node = node->next;
			
			// is there a node later in the list that is within range?
			if ((find = find_nearby_territory_node(node->loc, node->next, size))) {
				find->count += node->count;
				REMOVE_FROM_LIST(node, list, next);
				free(node);
			}
		}
		
		// double size on each pass
		size *= 2;
	}
	
	// return the list (head of list may have changed)
	return list;
}


/**
* Scans within the character's mapsize for matching tiles.
*
* @param char_data *ch The player.
* @param char_data *argument The tile to search for.
*/
void scan_for_tile(char_data *ch, char *argument) {
	extern byte distance_can_see(char_data *ch);
	extern int get_map_radius(char_data *ch);
	void sort_territory_node_list_by_distance(room_data *from, struct find_territory_node **node_list);
	extern const char *dirs[];

	struct find_territory_node *node_list = NULL, *node, *next_node;
	int dir, dist, mapsize, total, x, y, check_x, check_y;
	char output[MAX_STRING_LENGTH], line[128];
	room_data *map, *room;
	size_t size, lsize;
	crop_data *crop;
	bool ok;
	
	skip_spaces(&argument);
	
	if (!ch->desc) {
		return;	// don't bother
	}
	if (!*argument) {
		msg_to_char(ch, "Scan for what?\r\n");
		return;
	}
	if (!(map = get_map_location_for(IN_ROOM(ch)))) {
		msg_to_char(ch, "You can't scan for anything here.\r\n");
		return;
	}

	mapsize = get_map_radius(ch);
	
	for (x = -mapsize; x <= mapsize; ++x) {
		for (y = -mapsize; y <= mapsize; ++y) {
			if (!(room = real_shift(map, x, y))) {
				continue;
			}
			
			// actual distance check (compute circle)
			if (compute_distance(room, IN_ROOM(ch)) > mapsize) {
				continue;
			}
			
			// darkness check
			if (room != IN_ROOM(ch) && !CAN_SEE_IN_DARK_ROOM(ch, room) && compute_distance(room, IN_ROOM(ch)) > distance_can_see(ch) && !adjacent_room_is_light(room)) {
				continue;
			}
			
			// validate tile
			ok = FALSE;
			if (multi_isname(argument, GET_SECT_NAME(SECT(room)))) {
				ok = TRUE;
			}
			else if (GET_BUILDING(room) && multi_isname(argument, GET_BLD_NAME(GET_BUILDING(room)))) {
				ok = TRUE;
			}
			else if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && (crop = crop_proto(ROOM_CROP_TYPE(room))) && multi_isname(argument, GET_CROP_NAME(crop))) {
				ok = TRUE;
			}
			else if (multi_isname(argument, get_room_name(room, FALSE))) {
				ok = TRUE;
			}
			else {
				continue;
			}
			
			if (ok) {
				CREATE(node, struct find_territory_node, 1);
				node->loc = room;
				node->count = 1;
				node->next = node_list;
				node_list = node;
			}
		}
	}

	if (node_list) {
		sort_territory_node_list_by_distance(IN_ROOM(ch), &node_list);
		node_list = reduce_territory_node_list(node_list);
		
		size = snprintf(output, sizeof(output), "Nearby tiles matching '%s' within %d tile%s:\r\n", argument, mapsize, PLURAL(mapsize));
		
		// display and free the nodes
		total = 0;
		for (node = node_list; node; node = next_node) {
			next_node = node->next;
			total += node->count;
			
			// territory can be off the map (e.g. ships) and get a -1 here
			check_x = X_COORD(node->loc);
			check_y = Y_COORD(node->loc);
			
			dist = compute_distance(IN_ROOM(ch), node->loc);
			dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), node->loc));
			
			if (CHECK_MAP_BOUNDS(check_x, check_y) && HAS_ABILITY(ch, ABIL_NAVIGATION)) {
				lsize = snprintf(line, sizeof(line), "%2d tile%s %s (%d, %d) - %s", dist, PLURAL(dist), (dir == NO_DIR ? "away" : dirs[dir]), check_x, check_y, get_room_name(node->loc, FALSE));
			}
			else {
				lsize = snprintf(line, sizeof(line), "%2d tile%s %s - %s", dist, PLURAL(dist), (dir == NO_DIR ? "away" : dirs[dir]), get_room_name(node->loc, FALSE));
			}
			
			if (node->count > 1) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " (and %d nearby tile%s)", node->count, PLURAL(node->count));
			}
			
			if (size + lsize + 32 < sizeof(output)) {
				size += snprintf(output + size, sizeof(output) - size, "%s\r\n", line);
			}
			
			free(node);
		}
		
		node_list = NULL;
		size += snprintf(output + size, sizeof(output) - size, "Total: %d\r\n", total);
		page_string(ch->desc, output, TRUE);
	}
	else {
		msg_to_char(ch, "No matching territory found.\r\n");
	}
	
	GET_WAIT_STATE(ch) = 1 RL_SEC;	// short lag for scannings
}


// quick-switch of linked list positions
inline struct find_territory_node *switch_node_pos(struct find_territory_node *l1, struct find_territory_node *l2) {
    l1->next = l2->next;
    l2->next = l1;
    return l2;
}


/**
* Sort a territory node list, by distance from a room.
*
* @param room_data *from The room to measure from.
* @param struct find_territory_node **node_list A pointer to the node list to sort.
*/
void sort_territory_node_list_by_distance(room_data *from, struct find_territory_node **node_list) {
	struct find_territory_node *start, *p, *q, *top;
    bool changed = TRUE;
        
    // safety first
    if (!from) {
    	return;
    }
    
    start = *node_list;

	CREATE(top, struct find_territory_node, 1);

    top->next = start;
    if (start && start->next) {
    	// q is always one item behind p

        while (changed) {
            changed = FALSE;
            q = top;
            p = top->next;
            while (p->next != NULL) {
            	if (compute_distance(from, p->loc) > compute_distance(from, p->next->loc)) {
					q->next = switch_node_pos(p, p->next);
					changed = TRUE;
				}
				
                q = p;
                if (p->next) {
                    p = p->next;
                }
            }
        }
    }
    
    *node_list = top->next;
    free(top);
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE COMMANDS /////////////////////////////////////////////////////////

ACMD(do_abandon) {
	empire_data *e;

	if ((e = ROOM_OWNER(IN_ROOM(ch))) != GET_LOYALTY(ch))
		msg_to_char(ch, "You don't own this acre.\r\n");
	else if (IS_CITY_CENTER(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't abandon a city center that way -- use \"city abandon\".\r\n");
	}
	else if (!has_permission(ch, PRIV_CEDE)) {
		msg_to_char(ch, "You don't have permission to abandon land.\r\n");
	}
	else if (HOME_ROOM(IN_ROOM(ch)) != IN_ROOM(ch)) {
		msg_to_char(ch, "Just abandon the main room for the building.\r\n");
	}
	else {
		msg_to_char(ch, "Territory abandoned.\r\n");
		abandon_room(IN_ROOM(ch));
		read_empire_territory(e);
	}
}


ACMD(do_barde) {
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	Resource res[2] = { { o_IRON_INGOT, 10 }, END_RESOURCE_LIST };
	struct interact_exclusion_data *excl = NULL;
	struct interaction_item *interact;
	char_data *mob, *newmob = NULL;
	bool found;
	double prc;
	int num;

	one_argument(argument, arg);

	if (!can_use_ability(ch, ABIL_BARDE, NOTHING, 0, NOTHING)) {
		// nope
	}
	else if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_STABLE) || !IS_COMPLETE(IN_ROOM(ch)))
		msg_to_char(ch, "You must barde animals in the stable.\r\n");
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED))
		msg_to_char(ch, "You don't have permission to barde animals here.\r\n");
	else if (!*arg)
		msg_to_char(ch, "Which animal would you like to barde?\r\n");
	else if (!(mob = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (!IS_NPC(mob)) {
		act("You can't barde $N!", FALSE, ch, 0, mob, TO_CHAR);
	}
	else if (GET_PULLING(mob) || GET_LED_BY(mob)) {
		act("You can't barde $M right now.", FALSE, ch, NULL, mob, TO_CHAR);
	}
	else if (ABILITY_TRIGGERS(ch, mob, NULL, ABIL_BARDE)) {
		return;
	}
	else if (!IS_NPC(ch) && !has_resources(ch, res, TRUE, TRUE)) {
		// messages itself
	}
	else {
		// find interact
		found = FALSE;
		for (interact = mob->interactions; interact; interact = interact->next) {
			if (interact->type == INTERACT_BARDE && check_exclusion_set(&excl, interact->exclusion_code, interact->percent)) {
				if (!found) {
					// first one found
					act("You strap heavy armor onto $N.", FALSE, ch, NULL, mob, TO_CHAR);
					act("$n straps heavy armor onto $N.", FALSE, ch, NULL, mob, TO_NOTVICT);
					
					gain_ability_exp(ch, ABIL_BARDE, 50);
					command_lag(ch, WAIT_ABILITY);
					found = TRUE;
				}
				
				for (num = 0; num < interact->quantity; ++num) {
					newmob = read_mobile(interact->vnum);
					setup_generic_npc(newmob, GET_LOYALTY(mob), MOB_DYNAMIC_NAME(mob), MOB_DYNAMIC_SEX(mob));
					char_to_room(newmob, IN_ROOM(ch));
					MOB_INSTANCE_ID(newmob) = MOB_INSTANCE_ID(ch);
		
					prc = (double)GET_HEALTH(mob) / MAX(1, GET_MAX_HEALTH(mob));
					GET_HEALTH(newmob) = (int)(prc * GET_MAX_HEALTH(newmob));
				}
				
				if (interact->quantity > 1) {
					sprintf(buf, "$n is now $N (x%d)!", interact->quantity);
					act(buf, FALSE, mob, NULL, newmob, TO_ROOM);
				}
				else {
					act("$n is now $N!", FALSE, mob, NULL, newmob, TO_ROOM);
				}
				extract_char(mob);
				
				// save the data
				mob = newmob;
				load_mtrigger(mob);
								
				// barde ALWAYS breaks because the original mob and interactions are gone
				break;
			}
		}

		if (found) {
			if (!IS_NPC(ch)) {
				extract_resources(ch, res, TRUE);
			}
		}
		else {
			act("You can't barde $N!", FALSE, ch, NULL, mob, TO_CHAR);
		}
		
		free_exclusion_data(excl);
	}
}


ACMD(do_cede) {
	char arg2[MAX_INPUT_LENGTH];
	empire_data *e = GET_LOYALTY(ch), *f;
	room_data *room, *iter, *next_iter;
	char_data *targ;
	bool junk;

	if (IS_NPC(ch))
		return;

	argument = one_argument(argument, arg);
	any_one_word(argument, arg2);

	if (!e)
		msg_to_char(ch, "You own no territory.\r\n");
	else if (!*arg)
		msg_to_char(ch, "Usage: cede <person> (x, y)\r\n");
	else if (!(targ = get_player_vis(ch, arg, FIND_CHAR_WORLD | FIND_NO_DARK)))
		send_config_msg(ch, "no_person");
	else if (IS_NPC(targ))
		msg_to_char(ch, "You can't cede land to NPCs!\r\n");
	else if (ch == targ)
		msg_to_char(ch, "You can't cede land to yourself!\r\n");
	else if (*arg2 && !strstr(arg2, ",")) {
		msg_to_char(ch, "Usage: cede <person> (x, y)\r\n");
	}
	else if (!(*arg2 ? (room = HOME_ROOM(find_target_room(ch, arg2))) : (room = HOME_ROOM(IN_ROOM(ch)))))
		msg_to_char(ch, "Invalid location.\r\n");
	else if (GET_RANK(ch) < EMPIRE_PRIV(e, PRIV_CEDE)) {
		// don't use has_permission here because it would check permits on the room you're in
		msg_to_char(ch, "You don't have permission to cede.\r\n");
	}
	else if (ROOM_OWNER(room) != e)
		msg_to_char(ch, "You don't even own %s acre.\r\n", room == IN_ROOM(ch) ? "this" : "that");
	else if (ROOM_PRIVATE_OWNER(room) != NOBODY) {
		msg_to_char(ch, "You can't cede a private house.\r\n");
	}
	else if (IS_CITY_CENTER(room)) {
		msg_to_char(ch, "You can't cede a city center.\r\n");
	}
	else if ((f = get_or_create_empire(targ)) == NULL)
		msg_to_char(ch, "You can't seem to cede land to %s.\r\n", HMHR(targ));
	else if (f == e)
		msg_to_char(ch, "You can't cede land to your own empire!\r\n");
	else if (EMPIRE_CITY_TERRITORY(f) + EMPIRE_OUTSIDE_TERRITORY(f) >= land_can_claim(f, FALSE))
		msg_to_char(ch, "You can't cede land to %s, %s empire can't own any more land.\r\n", HMHR(targ), HSHR(targ));
	else if (!is_in_city_for_empire(room, f, FALSE, &junk) && EMPIRE_OUTSIDE_TERRITORY(f) >= land_can_claim(f, TRUE)) {
		msg_to_char(ch, "You can't cede land to that empire as it is over its limit for territory outside of cities.\r\n");
	}
	else if (is_at_war(f)) {
		msg_to_char(ch, "You can't cede land to an empire that is at war.\r\n");
	}
	else {
		log_to_empire(e, ELOG_TERRITORY, "%s has ceded (%d, %d) to %s", PERS(ch, ch, 1), X_COORD(room), Y_COORD(room), EMPIRE_NAME(f));
		log_to_empire(f, ELOG_TERRITORY, "%s has ceded (%d, %d) to this empire", PERS(ch, ch, 1), X_COORD(room), Y_COORD(room));
		send_config_msg(ch, "ok_string");

		abandon_room(room);
		claim_room(room, f);
		
		// mark as ceded
		set_room_extra_data(room, ROOM_EXTRA_CEDED, 1);
		HASH_ITER(interior_hh, interior_world_table, iter, next_iter) {
			if (HOME_ROOM(iter) == room) {
				set_room_extra_data(iter, ROOM_EXTRA_CEDED, 1);
			}
		}

		/* Transfers wealth, etc */
		read_empire_territory(e);
		read_empire_territory(f);
	}
}


ACMD(do_city) {
	char arg1[MAX_INPUT_LENGTH];

	half_chop(argument, arg, arg1);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: city <list | found | upgrade | downgrade | claim | abandon | rename>\r\n");
	}
	else if (is_abbrev(arg, "list")) {
		list_cities(ch, arg1);
	}
	else if (is_abbrev(arg, "found")) {
		found_city(ch, arg1);
	}
	else if (is_abbrev(arg, "upgrade")) {
		upgrade_city(ch, arg1);
	}
	else if (is_abbrev(arg, "downgrade")) {
		downgrade_city(ch, arg1);
	}
	else if (is_abbrev(arg, "abandon")) {
		abandon_city(ch, arg1);
	}
	else if (is_abbrev(arg, "claim")) {
		claim_city(ch, arg1);
	}
	else if (is_abbrev(arg, "rename")) {
		rename_city(ch, arg1);
	}
	else if (is_abbrev(arg, "traits")) {
		city_traits(ch, arg1);
	}
	else {
		msg_to_char(ch, "Usage: city <list | found | upgrade | claim | abandon | rename | traits>\r\n");
	}
}


ACMD(do_claim) {
	empire_data *e;
	bool junk;

	if (IS_NPC(ch))
		return;

	// this will found an empire if the character has none
	e = get_or_create_empire(ch);

	if (!e)
		msg_to_char(ch, "You don't belong to any empire.\r\n");
	else if (ROOM_OWNER(IN_ROOM(ch)) == e)
		msg_to_char(ch, "Your empire already owns this acre.\r\n");
	else if (GET_RANK(ch) < EMPIRE_PRIV(e, PRIV_CLAIM)) {
		// this doesn't use has_permission because that would check if the land is owned already
		msg_to_char(ch, "You don't have permission to claim land for the empire.\r\n");
	}
	else if (IS_CITY_CENTER(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't claim a city center.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_NO_CLAIM) || ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE | ROOM_AFF_HAS_INSTANCE))
		msg_to_char(ch, "This tile can't be claimed.\r\n");
	else if (ROOM_OWNER(IN_ROOM(ch)) != NULL)
		msg_to_char(ch, "This acre is already claimed.\r\n");
	else if (HOME_ROOM(IN_ROOM(ch)) != IN_ROOM(ch))
		msg_to_char(ch, "Just claim the main room for the building.\r\n");
	else if (!can_claim(ch))
		msg_to_char(ch, "You can't claim any more land.\r\n");
	else if (!can_build_or_claim_at_war(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "You can't claim here while at war with the empire that controls this area.\r\n");
	}
	else if (!is_in_city_for_empire(IN_ROOM(ch), e, FALSE, &junk) && EMPIRE_OUTSIDE_TERRITORY(e) >= land_can_claim(e, TRUE)) {
		msg_to_char(ch, "You can't claim this land because you're over the 20%% of your territory that can be outside of cities.\r\n");
	}
	else {
		send_config_msg(ch, "ok_string");
		claim_room(IN_ROOM(ch), e);
		read_empire_territory(e);
		save_empire(e);
	}
}


ACMD(do_defect) {
	empire_data *e;

	if (IS_NPC(ch))
		return;
	else if (!(e = GET_LOYALTY(ch)))
		msg_to_char(ch, "You don't seem to belong to any empire.\r\n");
	else if (GET_IDNUM(ch) == EMPIRE_LEADER(e))
		msg_to_char(ch, "The leader can't defect!\r\n");
	else {
		GET_LOYALTY(ch) = NULL;
		GET_EMPIRE_VNUM(ch) = NOTHING;
		add_cooldown(ch, COOLDOWN_LEFT_EMPIRE, 2 * SECS_PER_REAL_HOUR);
		SAVE_CHAR(ch);
		
		log_to_empire(e, ELOG_MEMBERS, "%s has defected from the empire", PERS(ch, ch, 1));
		msg_to_char(ch, "You defect from the empire!\r\n");
		add_lore(ch, LORE_DEFECT_EMPIRE, EMPIRE_VNUM(e));
		
		clear_private_owner(GET_IDNUM(ch));
		
		// this will adjust the empire's player count
		reread_empire_tech(e);
	}
}


ACMD(do_demote) {
	empire_data *e;
	int to_rank = NOTHING;
	bool file = FALSE;
	char_data *victim;

	if (IS_NPC(ch))
		return;

	argument = one_argument(argument, arg);
	skip_spaces(&argument);

	e = GET_LOYALTY(ch);

	if (!e) {
		msg_to_char(ch, "You don't belong to any empire.\r\n");
		return;
	}
	else if (GET_RANK(ch) < EMPIRE_PRIV(e, PRIV_PROMOTE)) {
		// don't use has_permission, it would check the ownership of the room
		msg_to_char(ch, "You can't demote anybody!\r\n");
		return;
	}
	else if (!*arg) {
		msg_to_char(ch, "Demote whom?\r\n");
		return;
	}

	// 2nd argument: demote to level?
	if (*argument) {
		if ((to_rank = find_rank_by_name(e, argument)) == NOTHING) {
			msg_to_char(ch, "Invalid rank.\r\n");
			return;
		}
		
		// 1-based not zero-based :-/
		++to_rank;
	}

	// do NOT return from any of these -- let it fall through to the "if (file)" later
	if (!(victim = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else if (ch == victim)
		msg_to_char(ch, "You can't demote yourself!\r\n");
	else if (IS_NPC(victim) || GET_LOYALTY(victim) != e)
		msg_to_char(ch, "That person is not in your empire.\r\n");
	else if ((to_rank != NOTHING ? to_rank : (to_rank = GET_RANK(victim) - 1)) > GET_RANK(victim))
		msg_to_char(ch, "Use promote for that.\r\n");
	else if (GET_RANK(victim) > GET_RANK(ch)) {
		msg_to_char(ch, "You can't demote someone above your rank.\r\n");
	}
	else if (GET_IDNUM(victim) == EMPIRE_LEADER(e)) {
		msg_to_char(ch, "You cannot demote the leader.\r\n");
	}
	else if (to_rank == GET_RANK(victim))
		act("$E is already that rank.", FALSE, ch, 0, victim, TO_CHAR);
	else if (to_rank < 1)
		act("You can't demote $M THAT low!", FALSE, ch, 0, victim, TO_CHAR);
	else {
		GET_RANK(victim) = to_rank;

		log_to_empire(e, ELOG_MEMBERS, "%s has been demoted to %s%s", PERS(victim, victim, 1), EMPIRE_RANK(e, to_rank-1), EMPIRE_BANNER(e));
		send_config_msg(ch, "ok_string");

		// save now
		if (file) {
			store_loaded_char(victim);
			file = FALSE;
		}
		else {
			SAVE_CHAR(victim);
		}
	}

	// clean up
	if (file && victim) {
		free_char(victim);
	}
}


ACMD(do_deposit) {
	empire_data *emp, *coin_emp;
	struct coin_data *coin;
	int coin_amt;
	double rate;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't deposit anything.\r\n");
	}
	else if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_VAULT)) {
		msg_to_char(ch, "You can only deposit coins in a vault.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You must finish building it first.\r\n");
	}
	else if (!(emp = ROOM_OWNER(IN_ROOM(ch)))) {
		msg_to_char(ch, "No empire stores coins here.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		// real members only
		msg_to_char(ch, "You don't have permission to deposit coins here.\r\n");
	}
	else if (find_coin_arg(argument, &coin_emp, &coin_amt, TRUE) == argument || coin_amt < 1) {
		msg_to_char(ch, "Usage: deposit <number> [type] coins\r\n");
	}
	else if (!(coin = find_coin_entry(GET_PLAYER_COINS(ch), coin_emp)) || coin->amount < coin_amt) {
		msg_to_char(ch, "You don't have %s.\r\n", money_amount(coin_emp, coin_amt));
	}
	else if ((rate = exchange_rate(coin_emp, emp)) < 0.01) {
		msg_to_char(ch, "Those coins have no value here.\r\n");
	}
	else if ((int)(rate * coin_amt) < 1) {
		msg_to_char(ch, "At this exchange rate, that many coins wouldn't be worth anything.\r\n");
	}
	else {
		msg_to_char(ch, "You deposit %s.\r\n", money_amount(coin_emp, coin_amt));
		sprintf(buf, "$n deposits %s.\r\n", money_amount(coin_emp, coin_amt));
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		increase_empire_coins(emp, coin_emp, coin_amt);
		decrease_coins(ch, coin_emp, coin_amt);
	}
}


ACMD(do_diplomacy) {	
	struct empire_political_data *pol_a, *pol_b;
	empire_data *e, *f;
	int i;

	char *dipl_commands[] = {
		"peace",	"war",		"ally",		"pact",
		"trade",	"distrust",	"truce",	"\n"
		};

	#define DIPLOMACY_FORMAT		\
		"Usage: diplomacy <action> <empire>\r\n"	\
		"Actions are:\r\n"	\
		" &gtrade&0 - propose or accept a trade agreement\r\n" \
		" &gally&0 - propose or accept a full alliance\r\n" \
		" &ypact&0 - propose or accept a pact of non-aggression\r\n" \
		" &ypeace&0 - end a war or begin a relationship with a neutral empire\r\n"	\
		" &ytruce&0 - end a war without declaring peace\r\n"	\
		" &rdistrust&0 - declare that your empire distrusts, but is not at war with, another\r\n"	\
		" &rwar&0 - declare war on an empire\r\n"

	argument = one_argument(argument, arg);
	skip_spaces(&argument);

	if (!*arg || !*argument) {
		msg_to_char(ch, DIPLOMACY_FORMAT);
		return;
	}

	if (!(e = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You don't belong to any empire!\r\n");
		return;
	}
	if (EMPIRE_IMM_ONLY(e)) {
		msg_to_char(ch, "Empires belonging to immortals cannot engage in diplomacy.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(e, PRIV_DIPLOMACY)) {
		// don't use has_permission, it would check the ownership of the room
		msg_to_char(ch, "You don't have the authority to make diplomatic relations.\r\n");
		return;
	}
	if (!(f = get_empire_by_name(argument))) {
		msg_to_char(ch, "Unknown empire.\r\n");
		return;
	}
	if (EMPIRE_IMM_ONLY(f)) {
		msg_to_char(ch, "Empires belonging to immortals cannot engage in diplomacy.\r\n");
		return;
	}

	if (e == f) {
		msg_to_char(ch, "You can't engage in diplomacy with your own empire!\r\n");
		return;
	}

	/* Find the command */
	for (i = 0; str_cmp(dipl_commands[i], "\n") && !is_abbrev(arg, dipl_commands[i]); i++);

	if (!str_cmp(dipl_commands[i], "\n")) {
		msg_to_char(ch, DIPLOMACY_FORMAT);
		return;
		}

	if (!(pol_a = find_relation(e, f))) {
		pol_a = create_relation(e, f);
	}
	if (!(pol_b = find_relation(f, e))) {
		pol_b = create_relation(f, e);
	}

	// TODO could split this out into different functions
	switch (i) {
		case 0:		/* Peace */
			if (IS_SET(pol_b->offer, DIPL_PEACE)) {
				REMOVE_BIT(pol_a->offer, DIPL_PEACE | DIPL_TRUCE);
				REMOVE_BIT(pol_b->offer, DIPL_PEACE | DIPL_TRUCE);
				if (IS_SET(pol_a->type, DIPL_WAR)) {
					syslog(SYS_INFO, 0, TRUE, "WAR: %s (%s) has ended the war with %s", EMPIRE_NAME(e), GET_NAME(ch), EMPIRE_NAME(f));
				}
				pol_a->type = pol_b->type = DIPL_PEACE;
				log_to_empire(e, ELOG_DIPLOMACY, "The empire is now at peace with %s", EMPIRE_NAME(f));
				log_to_empire(f, ELOG_DIPLOMACY, "The empire is now at peace with %s", EMPIRE_NAME(e));
				send_config_msg(ch, "ok_string");
			}
			else if (IS_SET(pol_a->type, DIPL_WAR | DIPL_DISTRUST | DIPL_TRUCE) || !pol_a->type) {
				SET_BIT(pol_a->offer, DIPL_PEACE);
				log_to_empire(e, ELOG_DIPLOMACY, "The empire has offered peace to %s", EMPIRE_NAME(f));
				log_to_empire(f, ELOG_DIPLOMACY, "%s has offered peace to the empire", EMPIRE_NAME(e));
				send_config_msg(ch, "ok_string");
			}
			else if (IS_SET(pol_a->type, DIPL_ALLIED))
				msg_to_char(ch, "But you're already allied!\r\n");
			else if (IS_SET(pol_a->type, DIPL_NONAGGR))
				msg_to_char(ch, "But you've already got a non-aggression pact!\r\n");
			else
				msg_to_char(ch, "But you already have better relations!\r\n");
			break;
		case 1: {	/* War */
			int war_cost = get_war_cost(e, f);
			
			if (IS_SET(pol_a->type, DIPL_WAR))
				msg_to_char(ch, "You're already at war!\r\n");
			else if (count_members_online(f) == 0) {
				msg_to_char(ch, "You can't declare war on an empire if none of their members are online!\r\n");
			}
			else if (EMPIRE_COINS(e) < war_cost) {
				msg_to_char(ch, "The empire requires %d coin%s in the vault in order to finance the war with %s!\r\n", war_cost, PLURAL(war_cost), EMPIRE_NAME(f));
			}
			else {
				pol_a->start_time = pol_b->start_time = time(0);
				pol_a->offer = pol_b->offer = 0;
				pol_a->type = pol_b->type = DIPL_WAR;
				
				EMPIRE_COINS(e) -= war_cost;
				
				log_to_empire(e, ELOG_DIPLOMACY, "War has been declared upon %s for %d coin%s!", EMPIRE_NAME(f), war_cost, PLURAL(war_cost));
				log_to_empire(f, ELOG_DIPLOMACY, "%s has declared war!", EMPIRE_NAME(e));
				syslog(SYS_INFO, 0, TRUE, "WAR: %s (%s) has declared war on %s", EMPIRE_NAME(e), GET_NAME(ch), EMPIRE_NAME(f));
				send_config_msg(ch, "ok_string");
			}
			break;
		}
		case 2:		/* Ally */
			if (IS_SET(pol_b->offer, DIPL_ALLIED)) {
				REMOVE_BIT(pol_b->offer, DIPL_ALLIED | DIPL_NONAGGR | DIPL_PEACE | DIPL_TRUCE);
				REMOVE_BIT(pol_a->offer, DIPL_ALLIED | DIPL_NONAGGR | DIPL_PEACE | DIPL_TRUCE);
				REMOVE_BIT(pol_b->type, DIPL_NONAGGR | DIPL_PEACE | DIPL_TRUCE);
				REMOVE_BIT(pol_a->type, DIPL_NONAGGR | DIPL_PEACE | DIPL_TRUCE);
				SET_BIT(pol_a->type, DIPL_ALLIED);
				SET_BIT(pol_b->type, DIPL_ALLIED);
				log_to_empire(e, ELOG_DIPLOMACY, "An alliance has been established with %s!", EMPIRE_NAME(f));
				log_to_empire(f, ELOG_DIPLOMACY, "%s has accepted the offer of alliance!", EMPIRE_NAME(e));
				send_config_msg(ch, "ok_string");
			}
			else if (IS_SET(pol_a->type, DIPL_WAR | DIPL_DISTRUST))
				msg_to_char(ch, "You'll have to establish peace first.\r\n");
			else if (IS_SET(pol_a->type, DIPL_ALLIED))
				msg_to_char(ch, "You're already allied!\r\n");
			else {
				SET_BIT(pol_a->offer, DIPL_ALLIED);
				log_to_empire(e, ELOG_DIPLOMACY, "The empire has suggested an alliance to %s", EMPIRE_NAME(f));
				log_to_empire(f, ELOG_DIPLOMACY, "%s has suggested an alliance", EMPIRE_NAME(e));
				send_config_msg(ch, "ok_string");
			}
			break;
		case 3:		/* Pact */
			if (IS_SET(pol_b->offer, DIPL_NONAGGR)) {
				REMOVE_BIT(pol_b->offer, DIPL_NONAGGR | DIPL_PEACE | DIPL_TRUCE);
				REMOVE_BIT(pol_a->offer, DIPL_NONAGGR | DIPL_PEACE | DIPL_TRUCE);
				REMOVE_BIT(pol_b->type, DIPL_PEACE | DIPL_TRUCE);
				REMOVE_BIT(pol_a->type, DIPL_PEACE | DIPL_TRUCE);
				SET_BIT(pol_a->type, DIPL_NONAGGR);
				SET_BIT(pol_b->type, DIPL_NONAGGR);
				log_to_empire(e, ELOG_DIPLOMACY, "A non-aggression pact has been established with %s!", EMPIRE_NAME(f));
				log_to_empire(f, ELOG_DIPLOMACY, "%s has accepted the offer of a non-aggression pact!", EMPIRE_NAME(e));
				send_config_msg(ch, "ok_string");
			}
			else if (IS_SET(pol_a->type, DIPL_WAR | DIPL_DISTRUST))
				msg_to_char(ch, "You'll have to establish peace first.\r\n");
			else if (IS_SET(pol_a->type, DIPL_ALLIED))
				msg_to_char(ch, "You're already allied!\r\n");
			else if (IS_SET(pol_a->type, DIPL_NONAGGR))
				msg_to_char(ch, "You've already got a pact!\r\n");
			else {
				SET_BIT(pol_a->offer, DIPL_NONAGGR);
				log_to_empire(e, ELOG_DIPLOMACY, "The empire has offered a non-aggression pact to %s", EMPIRE_NAME(f));
				log_to_empire(f, ELOG_DIPLOMACY, "%s has offered a non-aggression pact", EMPIRE_NAME(e));
				send_config_msg(ch, "ok_string");
			}
			break;
		case 4:		/* Trade */
			if (IS_SET(pol_b->offer, DIPL_TRADE)) {
				REMOVE_BIT(pol_b->offer, DIPL_TRADE);
				SET_BIT(pol_a->type, DIPL_TRADE);
				SET_BIT(pol_b->type, DIPL_TRADE);
				log_to_empire(e, ELOG_DIPLOMACY, "A trade agreement has been established with %s!", EMPIRE_NAME(f));
				log_to_empire(f, ELOG_DIPLOMACY, "%s has accepted the offer of a trade agreement!", EMPIRE_NAME(e));
				send_config_msg(ch, "ok_string");
			}
			else if (IS_SET(pol_a->type, DIPL_WAR | DIPL_DISTRUST) || !pol_a->type)
				msg_to_char(ch, "You'll have to establish peace first.\r\n");
			else if (IS_SET(pol_a->type, DIPL_TRADE))
				msg_to_char(ch, "You're already trading with them!\r\n");
			else {
				SET_BIT(pol_a->offer, DIPL_TRADE);
				log_to_empire(e, ELOG_DIPLOMACY, "The empire has offered a trade agreement to %s", EMPIRE_NAME(f));
				log_to_empire(f, ELOG_DIPLOMACY, "%s has offered a trade agreement", EMPIRE_NAME(e));
				send_config_msg(ch, "ok_string");
			}
			break;
		case 5:		/* Distrust */
			if (IS_SET(pol_a->type, DIPL_WAR))
				msg_to_char(ch, "You're already at war!\r\n");
			else if (IS_SET(pol_a->type, DIPL_DISTRUST))
				msg_to_char(ch, "You already distrust them!\r\n");
			else {
				pol_a->offer = pol_b->offer = 0;
				pol_a->type = pol_b->type = DIPL_DISTRUST;
				log_to_empire(e, ELOG_DIPLOMACY, "The empire now officially distrusts %s", EMPIRE_NAME(f));
				log_to_empire(f, ELOG_DIPLOMACY, "%s has declared that they officially distrust the empire", EMPIRE_NAME(e));
				send_config_msg(ch, "ok_string");
			}
			break;
		case 6: {		/* Truce */
			if (IS_SET(pol_b->offer, DIPL_TRUCE)) {
				REMOVE_BIT(pol_b->offer, DIPL_TRUCE | DIPL_PEACE);
				if (IS_SET(pol_a->type, DIPL_WAR)) {
					syslog(SYS_INFO, 0, TRUE, "WAR: %s (%s) has ended the war with %s", EMPIRE_NAME(e), GET_NAME(ch), EMPIRE_NAME(f));
				}
				pol_a->type = pol_b->type = DIPL_TRUCE;
				log_to_empire(e, ELOG_DIPLOMACY, "The empire now has a truce with %s", EMPIRE_NAME(f));
				log_to_empire(f, ELOG_DIPLOMACY, "The empire now has a truce with %s", EMPIRE_NAME(e));
				send_config_msg(ch, "ok_string");
			}
			else if (IS_SET(pol_a->type, DIPL_WAR | DIPL_DISTRUST)) {
				SET_BIT(pol_a->offer, DIPL_TRUCE);
				log_to_empire(e, ELOG_DIPLOMACY, "The empire has offered a truce to %s", EMPIRE_NAME(f));
				log_to_empire(f, ELOG_DIPLOMACY, "%s has offered a truce to the empire", EMPIRE_NAME(e));
				send_config_msg(ch, "ok_string");
			}
			else if (IS_SET(pol_a->type, DIPL_ALLIED))
				msg_to_char(ch, "But you're already allied!\r\n");
			else if (IS_SET(pol_a->type, DIPL_NONAGGR))
				msg_to_char(ch, "But you've already got a non-aggression pact!\r\n");
			else
				msg_to_char(ch, "But you already have better relations!\r\n");
			break;
		}
	}

	save_empire(e);
	save_empire(f);
}


ACMD(do_efind) {
	char buf[MAX_STRING_LENGTH*2];
	obj_data *obj;
	empire_data *emp;
	int check_x, check_y, total;
	bool all = FALSE;
	room_data *last_rm, *iter, *next_iter;
	struct efind_group *eg, *next_eg, *list = NULL;
	size_t size;
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch) || !ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!(emp = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You aren't in an empire.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Usage: efind <item>\r\n");
	}
	else {
		if (!str_cmp(arg, "all")) {
			all = TRUE;
		}
	
		total = 0;
		
		// first, gotta find them all
		HASH_ITER(world_hh, world_table, iter, next_iter) {
			if (ROOM_OWNER(iter) == emp) {			
				for (obj = ROOM_CONTENTS(iter); obj; obj = obj->next_content) {
					if ((all && CAN_WEAR(obj, ITEM_WEAR_TAKE)) || (!all && isname(arg, obj->name))) {
						add_obj_to_efind(&list, obj, iter);
						++total;
					}
				}
			}
		}

		if (total > 0) {
			size = snprintf(buf, sizeof(buf), "You discover:");	// leave off \r\n
			last_rm = NULL;
			
			for (eg = list; eg; eg = next_eg) {
				next_eg = eg->next;
				
				// length limit check
				if (size + 24 > sizeof(buf)) {
					size += snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
					break;
				}
				
				// first item at this location?
				if (eg->location != last_rm) {
					if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
						// count have no coordinates
						check_x = X_COORD(eg->location);
						check_y = Y_COORD(eg->location);
						
						if (CHECK_MAP_BOUNDS(check_x, check_y)) {
							size += snprintf(buf + size, sizeof(buf) - size, "\r\n(%*d, %*d) ", X_PRECISION, check_x, Y_PRECISION, check_y);
						}
						else {
							size += snprintf(buf + size, sizeof(buf) - size, "\r\n(unknown) ");
						}
					}
					else {
						size += snprintf(buf + size, sizeof(buf) - size, "\r\n");
					}
					size += snprintf(buf + size, sizeof(buf) - size, "%s: ", get_room_name(eg->location, FALSE));
					
					last_rm = eg->location;
				}
				else {
					size += snprintf(buf + size, sizeof(buf) - size, ", ");
				}
				
				if (eg->count > 1) {
					size += snprintf(buf + size, sizeof(buf) - size, "%dx ", eg->count);
				}
				
				size += snprintf(buf + size, sizeof(buf) - size, "%s", get_obj_desc(eg->obj, ch, OBJ_DESC_SHORT));
				free(eg);
			}
			// all free! free!
			list = NULL;
			
			size += snprintf(buf + size, sizeof(buf) - size, "\r\n");	// training crlf
			page_string(ch->desc, buf, TRUE);
		}
		else {
			msg_to_char(ch, "You don't discover anything like that in your empire.\r\n");
		}
	}
}


// syntax: elog [empire] [type] [lines]
ACMD(do_elog) {
	extern const char *empire_log_types[];
	
	char *argptr, *tempptr, *time_s, buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	int iter, count, type = NOTHING, lines = -1;
	struct empire_log_data *elog;
	empire_data *emp = NULL;
	time_t logtime;
	bool found;
	bool imm_access = GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES);
	
	// in case none is provided
	const int default_lines = 15;
	
	if (IS_NPC(ch) || !ch->desc) {
		return;
	}
	
	// optional first arg (empire) and empire detection
	argptr = any_one_word(argument, arg);
	if (!imm_access || !(emp = get_empire_by_name(arg))) {
		emp = GET_LOYALTY(ch);
		argptr = argument;
	}
	
	// found one?
	if (!emp) {
		msg_to_char(ch, "You aren't in an empire.\r\n");
		return;
	}
	
	// allowed?
	if (!imm_access && GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_LOGS)) {
		msg_to_char(ch, "You don't have permission to read empire logs.\r\n");
		return;
	}
	
	// optional arg: type lines
	tempptr = any_one_arg(argptr, arg);
	if (*arg && isdigit(*arg)) {
		lines = atoi(arg);
		argptr = tempptr;
	}
	else if (*arg && (type = search_block(arg, empire_log_types, FALSE)) != NOTHING) {
		argptr = tempptr;
	}
	else if (*arg) {
		msg_to_char(ch, "Unknown log type. Valid log types are: ");
		for (iter = 0, found = FALSE; *empire_log_types[iter] != '\n'; ++iter) {
			if (iter != ELOG_NONE) {
				msg_to_char(ch, "%s%s", (found ? ", " : ""), empire_log_types[iter]);
				found = TRUE;
			}
		}
		msg_to_char(ch, "\r\n");
		return;
	}
	
	// optional arg: lines (if not detected before)
	tempptr = any_one_arg(argptr, arg);
	if (lines == -1 && *arg && isdigit(*arg)) {
		lines = atoi(arg);
		argptr = tempptr;
	}
	else if (*arg) {
		msg_to_char(ch, "Invalid number of lines.\r\n");
		return;
	}
	
	// did we find a line count?
	if (lines == -1) {
		lines = default_lines;
	}
	
	// ok, ready to show logs: count total matching logs
	count = 0;
	for (elog = EMPIRE_LOGS(emp); elog; elog = elog->next) {
		if (type == NOTHING || elog->type == type) {
			++count;
		}
	}
	
	sprintf(buf, "%s logs for %s:\r\n", (type == NOTHING ? "All" : empire_log_types[type]), EMPIRE_NAME(emp));
	
	// now show the LAST [lines] log entries (show if remaining-lines<=0)
	for (elog = EMPIRE_LOGS(emp); elog; elog = elog->next) {
		if (type == NOTHING || elog->type == type) {
			if (count-- - lines <= 0) {
				logtime = elog->timestamp;
				time_s = asctime(localtime(&logtime));
				sprintf(line, "%-20.20s: %s&0\r\n", time_s + 4, strip_color(elog->string));
				
				if (strlen(buf) + strlen(line) < MAX_STRING_LENGTH) {
					strcat(buf, line);
				}
				else {
					strcat(buf, "OVERFLOW\r\n");
					break;
				}
			}
		}
	}
	
	page_string(ch->desc, buf, TRUE);
}


ACMD(do_emotd) {
	if (!GET_LOYALTY(ch)) {
		msg_to_char(ch, "You aren't in an empire.\r\n");
	}
	else if (!EMPIRE_MOTD(GET_LOYALTY(ch)) || !*EMPIRE_MOTD(GET_LOYALTY(ch))) {
		msg_to_char(ch, "Your empire has no MOTD.\r\n");
	}
	else if (ch->desc) {
		page_string(ch->desc, EMPIRE_MOTD(GET_LOYALTY(ch)), TRUE);
	}
}


ACMD(do_empires) {
	int whole_empire_timeout = config_get_int("whole_empire_timeout") * SECS_PER_REAL_DAY;

	empire_data *e, *emp, *next_emp;
	char_data *vict = NULL;
	int min = 1, count;
	bool more = FALSE, all = FALSE, file = FALSE;
	char title[80];

	skip_spaces(&argument);

	if (!empire_table) {
		msg_to_char(ch, "No empires have been formed.\r\n");
		return;
	}

	// argument processing
	if (!strn_cmp(argument, "-m", 2)) {
		more = TRUE;
	}
	else if (!strn_cmp(argument, "-a", 2)) {
		all = TRUE;
	}
	else if (!strn_cmp(argument, "-p", 2)) {
		// lookup by name
		one_argument(argument + 2, arg);
		if (!*arg) {
			msg_to_char(ch, "Look up which player's empire?\r\n");
		}
		else if (!(vict = find_or_load_player(arg, &file))) {
			send_to_char("There is no such player.\r\n", ch);
		}
		else if (!GET_LOYALTY(vict)) {
			msg_to_char(ch, "That player is not in an empire.\r\n");
		}
		else {
			show_detailed_empire(ch, GET_LOYALTY(vict));
		}
		
		if (vict && file) {
			free_char(vict);
		}
		return;
	}
	else if (!strn_cmp(argument, "-v", 2) && IS_IMMORTAL(ch)) {
		// lookup by vnum
		one_argument(argument + 2, arg);
		
		if (!*arg) {
			msg_to_char(ch, "Look up which empire vnum?\r\n");
		}
		else if ((e = real_empire(atoi(arg)))) {
			show_detailed_empire(ch, e);
		}
		else {
			msg_to_char(ch, "No such empire vnum %s.\r\n", arg);
		}
		return;
	}
	else if (*argument == '-')
		min = atoi((argument + 1));
	else if (*argument) {
		// see just 1 empire
		if ((e = get_empire_by_name(argument))) {
			show_detailed_empire(ch, e);
		}
		else {
			msg_to_char(ch, "There is no empire by that name or number.\r\n");
		}
		return;
	}
	
	// helpers
	*title = '\0';
	if (min > 1) {
		all = TRUE;
		sprintf(title, "Empires with %d empires or more:", min);
	}
	if (more) {
		strcpy(title, "Most empires:");
	}
	if (all) {
		more = TRUE;
		strcpy(title, "All empires:");
	}
	if (!*title) {
		strcpy(title, "Prominent empires:");
	}

	msg_to_char(ch, "%-35.35s  Sc  Mm  Grt  Territory\r\n", title);

	count = 0;
	HASH_ITER(hh, empire_table, emp, next_emp) {
		++count;
		
		if (EMPIRE_MEMBERS(emp) < min) {
			continue;
		}
		if (!all && EMPIRE_IMM_ONLY(emp)) {
			continue;
		}
		if (!more && !EMPIRE_HAS_TECH(emp, TECH_PROMINENCE)) {
			continue;
		}
		if (!all && (EMPIRE_CITY_TERRITORY(emp) + EMPIRE_OUTSIDE_TERRITORY(emp)) <= 0) {
			continue;
		}
		if (!all && EMPIRE_LAST_LOGON(emp) < (time(0) - whole_empire_timeout)) {
			continue;
		}
		
		msg_to_char(ch, "%3d. %s%-30.30s&0  %2d  %2d  %3d  %d\r\n", count, EMPIRE_BANNER(emp), EMPIRE_NAME(emp), get_total_score(emp), EMPIRE_MEMBERS(emp), EMPIRE_GREATNESS(emp), EMPIRE_CITY_TERRITORY(emp) + EMPIRE_OUTSIDE_TERRITORY(emp));
	}
	
	msg_to_char(ch, "List options: -m for more, -a for all, -## for minimum members\r\n");
}


ACMD(do_empire_inventory) {
	char error[MAX_STRING_LENGTH], arg2[MAX_INPUT_LENGTH];
	empire_data *emp;

	argument = any_one_word(argument, arg);
	skip_spaces(&argument);
	strcpy(arg2, argument);
	
	// only immortals may inventory other empires
	if (!*arg || (GET_ACCESS_LEVEL(ch) < LVL_CIMPL && !IS_GRANTED(ch, GRANT_EMPIRES))) {
		emp = GET_LOYALTY(ch);
		if (*arg2) {
			sprintf(buf, "%s %s", arg, arg2);
			strcpy(arg2, buf);
		}
		else {
			strcpy(arg2, arg);
		}
		strcpy(error, "You don't belong to any empire!\r\n");
	}
	else {
		emp = get_empire_by_name(arg);
		if (!emp) {
			emp = GET_LOYALTY(ch);
			if (*arg2) {
				sprintf(buf, "%s %s", arg, arg2);
				strcpy(arg2, buf);
			}
			else {
				strcpy(arg2, arg);
			}

			// in case
			strcpy(error, "You don't belong to any empire!\r\n");
		}
	}
	
	if (emp) {
		show_empire_inventory_to_char(ch, emp, arg2);
	}
	else {
		send_to_char(error, ch);
	}
}


ACMD(do_enroll) {
	void save_char_file_u(struct char_file_u *st);
	struct char_file_u chdata;
	struct empire_territory_data *ter, *next_ter;
	struct empire_npc_data *npc;
	struct empire_storage_data *store, *store2;
	struct empire_city_data *city, *next_city, *temp;
	struct empire_unique_storage *eus;
	struct shipping_data *shipd;
	empire_data *e, *old;
	room_data *room, *next_room;
	int j, old_store;
	char_data *targ = NULL, *victim, *mob;
	bool file = FALSE;
	obj_data *obj;

	if (IS_NPC(ch))
		return;

	if (IS_NPC(ch) || !GET_LOYALTY(ch)) {
		msg_to_char(ch, "You don't belong to any empire.\r\n");
		return;
	}
	
	one_argument(argument, arg);
	e = GET_LOYALTY(ch);

	if (!e)
		msg_to_char(ch, "You don't belong to any empire.\r\n");
	else if (GET_RANK(ch) < EMPIRE_PRIV(e, PRIV_ENROLL)) {
		// don't use has_permission; it would check ownership of the room
		msg_to_char(ch, "You don't have the authority to enroll followers.\r\n");
	}
	else if (!*arg)
		msg_to_char(ch, "Whom did you want to enroll?\r\n");
	else if (!(targ = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else if (IS_NPC(targ))
		msg_to_char(ch, "You can't enroll animals!\r\n");
	else if (ch == targ)
		msg_to_char(ch, "You're already in the empire!\r\n");
	else if (GET_PLEDGE(targ) != EMPIRE_VNUM(e))
		act("$E has not pledged $Mself to your empire.", FALSE, ch, 0, targ, TO_CHAR | TO_SLEEP);
	else if ((old = GET_LOYALTY(targ)) && EMPIRE_LEADER(old) != GET_IDNUM(targ))
		act("$E is already loyal to another empire.", FALSE, ch, 0, targ, TO_CHAR | TO_SLEEP);
	else {
		log_to_empire(e, ELOG_MEMBERS, "%s has been enrolled in the empire", PERS(targ, targ, 1));
		msg_to_char(targ, "You have been enrolled in %s.\r\n", EMPIRE_NAME(e));
		send_config_msg(ch, "ok_string");
		
		GET_LOYALTY(targ) = e;
		GET_EMPIRE_VNUM(targ) = EMPIRE_VNUM(e);
		GET_RANK(targ) = 1;
		GET_PLEDGE(targ) = NOTHING;
		add_lore(targ, LORE_JOIN_EMPIRE, EMPIRE_VNUM(e));
		
		// TODO split this out into a "merge empires" func

		// move data over
		if (old && EMPIRE_LEADER(old) == GET_IDNUM(targ)) {
			eliminate_linkdead_players();
			
			// move members
			for (j = 0; j <= top_of_p_table; j++) {
				// only even bother checking people other than targ
				if (player_table[j].id != GET_IDNUM(targ)) {
					if ((victim = is_playing(player_table[j].id))) {
						if (GET_LOYALTY(victim) == old) {
							msg_to_char(victim, "Your empire has merged with %s.\r\n", EMPIRE_NAME(e));
							add_lore(victim, LORE_JOIN_EMPIRE, EMPIRE_VNUM(e));
							GET_LOYALTY(victim) = e;
							GET_EMPIRE_VNUM(victim) = EMPIRE_VNUM(e);
							GET_RANK(victim) = 1;
							SAVE_CHAR(victim);
						}
					}
					else if ((victim = is_at_menu(player_table[j].id))) {
						// hybrid
						if (GET_LOYALTY(victim) == old) {
							add_lore(victim, LORE_JOIN_EMPIRE, EMPIRE_VNUM(e));
							GET_LOYALTY(victim) = e;
							GET_EMPIRE_VNUM(victim) = EMPIRE_VNUM(e);
							GET_RANK(victim) = 1;
							SAVE_CHAR(victim);
							
							load_char(player_table[j].name, &chdata);
							if (chdata.player_specials_saved.empire == EMPIRE_VNUM(old)) {
								chdata.player_specials_saved.empire = EMPIRE_VNUM(e);
								chdata.player_specials_saved.rank = 1;
								save_char_file_u(&chdata);
							}
						}
					}
					else {
						load_char(player_table[j].name, &chdata);
						if (chdata.player_specials_saved.empire == EMPIRE_VNUM(old)) {
							chdata.player_specials_saved.empire = EMPIRE_VNUM(e);
							chdata.player_specials_saved.rank = 1;
							save_char_file_u(&chdata);
						}
					}
				}
			}
		
			// mobs
			for (mob = character_list; mob; mob = mob->next) {
				if (GET_LOYALTY(mob) == old) {
					GET_LOYALTY(mob) = e;
				}
			}
			
			// objs
			for (obj = object_list; obj; obj = obj->next) {
				if (obj->last_empire_id == EMPIRE_VNUM(old)) {
					obj->last_empire_id = EMPIRE_VNUM(e);
				}
			}

			// storage
			for (store = EMPIRE_STORAGE(old); store; store = store->next) {
				if (!(store2 = find_stored_resource(e, store->island, store->vnum))) {
					CREATE(store2, struct empire_storage_data, 1);
					store2->next = EMPIRE_STORAGE(e);
					EMPIRE_STORAGE(e) = store2;
					store2->vnum = store->vnum;
					store2->island = store->island;
				}

				old_store = store2->amount;
				store2->amount += store->amount;
				// bounds checking
				if (store2->amount < old_store || store2->amount > MAX_STORAGE) {
					store2->amount = MAX_STORAGE;
				}
			}
			
			// shipping: append to end of current empire's list
			if (EMPIRE_SHIPPING_LIST(old)) {
				// find end
				if ((shipd = EMPIRE_SHIPPING_LIST(e))) {
					while (shipd->next) {
						shipd = shipd->next;
					}
					shipd->next = EMPIRE_SHIPPING_LIST(old);
				}
				else {
					EMPIRE_SHIPPING_LIST(e) = EMPIRE_SHIPPING_LIST(old);
				}
				EMPIRE_SHIPPING_LIST(old) = NULL;
			}
			
			// unique storage: append to end of current empire's list
			if (EMPIRE_UNIQUE_STORAGE(old)) {
				// find end
				if ((eus = EMPIRE_UNIQUE_STORAGE(e))) {
					while (eus->next) {
						eus = eus->next;
					}
					eus->next = EMPIRE_UNIQUE_STORAGE(old);
				}
				else {
					EMPIRE_UNIQUE_STORAGE(e) = EMPIRE_UNIQUE_STORAGE(old);
				}
				EMPIRE_UNIQUE_STORAGE(old) = NULL;
			}
			
			// cities
			for (city = EMPIRE_CITY_LIST(old); city; city = next_city) {
				next_city = city->next;
				
				if (city_points_available(e) >= (city->type + 1)) {
					// remove from old empire
					REMOVE_FROM_LIST(city, EMPIRE_CITY_LIST(old), next);
					city->next = NULL;
					
					// add to new empire
					if (EMPIRE_CITY_LIST(e)) {
						temp = EMPIRE_CITY_LIST(e);
						while (temp->next) {
							temp = temp->next;
						}
						temp->next = city;
					}
					else {
						EMPIRE_CITY_LIST(e) = city;
					}
				}
				else {
					// no room for this city
					perform_abandon_city(old, city, FALSE);
				}
			}

			// territory data
			for (ter = EMPIRE_TERRITORY_LIST(old); ter; ter = next_ter) {
				next_ter = ter->next;
				
				// switch npc allegiance
				for (npc = ter->npcs; npc; npc = npc->next) {
					npc->empire_id = EMPIRE_VNUM(e);
				}
				
				// move territory over
				ter->next = EMPIRE_TERRITORY_LIST(e);
				EMPIRE_TERRITORY_LIST(e) = ter;
			}
			
			EMPIRE_TERRITORY_LIST(old) = NULL;
			
			// move territory over
			HASH_ITER(world_hh, world_table, room, next_room) {
				if (ROOM_OWNER(room) == old) {
					ROOM_OWNER(room) = e;
				}
			}
			
			// move coins (use same empire to get them at full value)
			increase_empire_coins(e, e, EMPIRE_COINS(old));
			
			// save now
			if (file) {
				// WARNING: this frees targ
				store_loaded_char(targ);
				file = FALSE;
				targ = NULL;
			}
			else {
				SAVE_CHAR(targ);
			}
			
			// Delete the old empire
			delete_empire(old);
		}
		
		// targ still around when we got this far?
		if (targ) {
			// save now
			if (file) {
				// this frees targ
				store_loaded_char(targ);
				file = FALSE;
				targ = NULL;
			}
			else {
				SAVE_CHAR(targ);
			}
		}
		
		// This will PROPERLY reset wealth and land, plus members and abilities
		reread_empire_tech(GET_LOYALTY(ch));
	}
	
	// clean up if still necessary
	if (file && targ) {
		free_char(targ);
	}
}


ACMD(do_esay) {
	void clear_last_act_message(descriptor_data *desc);
	void add_to_channel_history(descriptor_data *desc, int type, char *message);
	extern bool is_ignoring(char_data *ch, char_data *victim);
	
	descriptor_data *d;
	char_data *tch;
	int level = 0, i;
	empire_data *e;
	bool emote = FALSE;
	char buf[MAX_STRING_LENGTH], lstring[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH], output[MAX_STRING_LENGTH], color[8];

	if (IS_NPC(ch))
		return;

	if (!(e = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You don't belong to any empire.\r\n");
		return;
		}

	if (PLR_FLAGGED(ch, PLR_MUTED)) {
		msg_to_char(ch, "You can't use the empire channel while muted.\r\n");
		return;
		}

	skip_spaces(&argument);

	if (!*argument) {
		msg_to_char(ch, "What would you like to tell your empire?\r\n");
		return;
	}
	if (*argument == '*') {
		argument++;
		emote = TRUE;
	}
	if (*argument == '#') {
		half_chop(argument, arg, buf);
		strcpy(argument, buf);
		for (i = 0; i < strlen(arg); i++)
			if (arg[i] == '_')
				arg[i] = ' ';
		for (i = 0; i < EMPIRE_NUM_RANKS(e); i++)
			if (is_abbrev(arg+1, EMPIRE_RANK(e, i)))
				break;
		if (i < EMPIRE_NUM_RANKS(e))
			level = i;
	}
	if (*argument == '*') {
		argument++;
		emote = TRUE;
	}
	skip_spaces(&argument);

	level++;

	if (level > 1)
		sprintf(lstring, " <%s&0>", EMPIRE_RANK(e, level-1));
	else
		*lstring = '\0';

	/* Since we cut up the string, we have to check again */
	if (!*argument) {
		msg_to_char(ch, "What would you like to tell your empire?\r\n");
		return;
	}

	// NOTE: both modes will leave in 2 '%s' for color codes
	if (emote) {
		sprintf(buf, "%%s[%sEMPIRE%%s%s] $o %s&0", EMPIRE_BANNER(e), lstring, double_percents(argument));
	}
	else {
		sprintf(buf, "%%s[%sEMPIRE%%s $o%s]: %s", EMPIRE_BANNER(e), lstring, double_percents(argument));
	}

	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		// for channel history
		if (ch->desc) {
			clear_last_act_message(ch->desc);
		}

		sprintf(color, "&%c", GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_ESAY) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_ESAY) : '0');
		sprintf(output, buf, color, color);
		
		act(output, FALSE, ch, 0, 0, TO_CHAR | TO_SLEEP | TO_NODARK);
		
		// channel history
		if (ch->desc && ch->desc->last_act_message) {
			// the message was sent via act(), we can retrieve it from the desc
			sprintf(lbuf, "%s", ch->desc->last_act_message);
			add_to_channel_history(ch->desc, CHANNEL_HISTORY_EMPIRE, lbuf);
		}
	}

	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) != CON_PLAYING || !(tch = d->character) || IS_NPC(tch) || is_ignoring(tch, ch) || GET_LOYALTY(tch) != e || GET_RANK(tch) < level || tch == ch)
			continue;
		else {
			clear_last_act_message(d);
			
			sprintf(color, "&%c", GET_CUSTOM_COLOR(tch, CUSTOM_COLOR_ESAY) ? GET_CUSTOM_COLOR(tch, CUSTOM_COLOR_ESAY) : '0');
			sprintf(output, buf, color, color);
			act(output, FALSE, ch, 0, tch, TO_VICT | TO_SLEEP | TO_NODARK);
			
			// channel history
			if (d->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				sprintf(lbuf, "%s", d->last_act_message);
				add_to_channel_history(d, CHANNEL_HISTORY_EMPIRE, lbuf);
			}	
		}
	}
}


ACMD(do_expel) {
	empire_data *e;
	char_data *targ = NULL;
	bool file = FALSE;

	if (IS_NPC(ch) || !(e = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You don't belong to any empire.\r\n");
		return;
	}

	one_argument(argument, arg);

	// it's important not to RETURN from here, as the target may need to be freed later
	if (!e)
		msg_to_char(ch, "You don't belong to any empire.\r\n");
	else if (GET_RANK(ch) != EMPIRE_NUM_RANKS(e))
		msg_to_char(ch, "You don't have the authority to expel followers.\r\n");
	else if (!*arg)
		msg_to_char(ch, "Whom do you wish to expel?\r\n");
	else if (!(targ = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else if (IS_NPC(targ) || GET_LOYALTY(targ) != e)
		msg_to_char(ch, "That person is not a member of this empire.\r\n");
	else if (targ == ch)
		msg_to_char(ch, "You can't expel yourself.\r\n");
	else if (EMPIRE_LEADER(e) == GET_IDNUM(targ))
		msg_to_char(ch, "You can't expel the leader!\r\n");
	else {
		GET_LOYALTY(targ) = NULL;
		GET_EMPIRE_VNUM(targ) = NOTHING;
		add_cooldown(targ, COOLDOWN_LEFT_EMPIRE, 2 * SECS_PER_REAL_HOUR);
		clear_private_owner(GET_IDNUM(targ));

		log_to_empire(e, ELOG_MEMBERS, "%s has been expelled from the empire", PERS(targ, targ, 1));
		send_config_msg(ch, "ok_string");
		msg_to_char(targ, "You have been expelled from the empire.\r\n");
		add_lore(targ, LORE_KICKED_EMPIRE, EMPIRE_VNUM(e));

		// save now
		if (file) {
			store_loaded_char(targ);
			file = FALSE;
		}
		else {
			SAVE_CHAR(targ);
		}

		// do this AFTER the save -- fixes member counts, etc
		reread_empire_tech(e);
	}

	// clean up
	if (file && targ) {
		free_char(targ);
	}
}


ACMD(do_findmaintenance) {
	struct find_territory_node *node_list = NULL, *node, *next_node;
	empire_data *emp = GET_LOYALTY(ch);
	room_data *iter, *next_iter;
	
	// imms can target this
	skip_spaces(&argument);
	if (*argument && (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES))) {
		if (!(emp = get_empire_by_name(argument))) {
			msg_to_char(ch, "Unknown empire.\r\n");
			return;
		}
	}
	
	if (!ch->desc || IS_NPC(ch)) {
		return;
	}
	
	if (!emp) {
		msg_to_char(ch, "You can't use findmaintenance if you are not in an empire.\r\n");
	}
	else {
		HASH_ITER(world_hh, world_table, iter, next_iter) {
			// skip non-map
			if (GET_ROOM_VNUM(iter) >= MAP_SIZE) {
				continue;
			}
			
			// owned by the empire?
			if (ROOM_OWNER(iter) == emp) {
				// needs repair?
				if (BUILDING_DISREPAIR(iter) > 1) {
					CREATE(node, struct find_territory_node, 1);
					node->loc = iter;
					node->count = 1;
					node->next = node_list;
					node_list = node;
				}
			}
		}
		
		if (node_list) {
			node_list = reduce_territory_node_list(node_list);
			strcpy(buf, "Locations needing at least 2 maintenance:\r\n");
			
			// display and free the nodes
			for (node = node_list; node; node = next_node) {
				next_node = node->next;
				sprintf(buf + strlen(buf), "%2d building%s near%s (%*d, %*d) %s\r\n", node->count, (node->count != 1 ? "s" : ""), (node->count == 1 ? " " : ""), X_PRECISION, X_COORD(node->loc), Y_PRECISION, Y_COORD(node->loc), get_room_name(node->loc, FALSE));
				free(node);
			}
			
			node_list = NULL;
			page_string(ch->desc, buf, TRUE);
		}
		else {
			msg_to_char(ch, "No buildings were found that needed at least 2 maintenance.\r\n");
		}
	}
}


/**
* Searches the world for the player's private home.
*
* @param char_data *ch The player.
* @return room_data* The home location, or NULL if none found.
*/
room_data *find_home(char_data *ch) {
	room_data *iter, *next_iter;
	
	if (IS_NPC(ch) || !GET_LOYALTY(ch)) {
		return NULL;
	}
	
	HASH_ITER(world_hh, world_table, iter, next_iter) {
		if (ROOM_PRIVATE_OWNER(iter) == GET_IDNUM(ch)) {
			return iter;
		}
	}
	
	return NULL;
}


ACMD(do_home) {	
	struct empire_territory_data *ter;
	struct empire_npc_data *npc;
	room_data *iter, *next_iter, *home = NULL, *real = HOME_ROOM(IN_ROOM(ch));
	empire_data *emp = GET_LOYALTY(ch);
	obj_data *obj;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	skip_spaces(&argument);
	home = find_home(ch);
	
	if (!*argument) {
		if (!home) {
			msg_to_char(ch, "You have no home set.\r\n");
		}
		else if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
			msg_to_char(ch, "Your home is at: %s (%d, %d)\r\n", get_room_name(home, FALSE), X_COORD(home), Y_COORD(home));
		}
		else {
			msg_to_char(ch, "Your home is at: %s\r\n", get_room_name(home, FALSE));
		}
		
		msg_to_char(ch, "Use 'home set' to claim this room.\r\n");
	}
	else if (!str_cmp(argument, "set")) {
		if (PLR_FLAGGED(ch, PLR_ADVENTURE_SUMMONED)) {
			msg_to_char(ch, "You can't home-set while adventure-summoned.\r\n");
		}
		else if (GET_POS(ch) < POS_STANDING) {
			msg_to_char(ch, "You can't do that right now. You need to be standing.\r\n");
		}
		else if (!GET_LOYALTY(ch) || ROOM_OWNER(real) != GET_LOYALTY(ch)) {
			msg_to_char(ch, "You need to own a building to make it your home.\r\n");
		}
		else if (ROOM_PRIVATE_OWNER(real) == GET_IDNUM(ch)) {
			msg_to_char(ch, "But it's already your home!\r\n");
		}
		else if (ROOM_PRIVATE_OWNER(real) != NOBODY && GET_RANK(ch) < EMPIRE_NUM_RANKS(emp)) {
			msg_to_char(ch, "Someone already owns this home.\r\n");
		}
		else if (!IS_MAP_BUILDING(real) || !IS_COMPLETE(real) || !GET_BUILDING(real) || GET_BLD_CITIZENS(GET_BUILDING(real)) <= 0) {
			msg_to_char(ch, "You can't make this your home.\r\n");
		}
		else if (ROOM_AFF_FLAGGED(real, ROOM_AFF_HAS_INSTANCE)) {
			msg_to_char(ch, "You can't make this your home right now.\r\n");
		}
		else {
			// allow the player to set home here
			clear_private_owner(GET_IDNUM(ch));
			
			// clear out npcs
			// TODO should this be done for interior rooms, too?
			if ((ter = find_territory_entry(emp, real))) {
				while ((npc = ter->npcs)) {
					if (npc->mob) {
						act("$n leaves.", TRUE, npc->mob, NULL, NULL, TO_ROOM);
						GET_EMPIRE_NPC_DATA(npc->mob) = NULL;	// un-link this npc data from the mob, or extract will corrupt memory
						extract_char(npc->mob);
						npc->mob = NULL;
					}
					
					EMPIRE_POPULATION(emp) -= 1;
					
					ter->npcs = npc->next;
					free(npc);
				}
			}
			
			COMPLEX_DATA(real)->private_owner = GET_IDNUM(ch);

			// interior only			
			HASH_ITER(interior_hh, interior_world_table, iter, next_iter) {
				// TODO consider a trigger like RoomUpdate that passes a var like %update% == homeset
				if (HOME_ROOM(iter) == real && BUILDING_VNUM(iter) == RTYPE_BEDROOM) {
					obj_to_room((obj = read_object(o_HOME_CHEST)), iter);
					load_otrigger(obj);
				}
			}
			
			log_to_empire(emp, ELOG_TERRITORY, "%s has made (%d, %d) %s home", PERS(ch, ch, 1), X_COORD(real), Y_COORD(real), HSHR(ch));
			msg_to_char(ch, "You make this your home.\r\n");
		}
	}
	else if (!str_cmp(argument, "clear")) {
		if (ROOM_PRIVATE_OWNER(IN_ROOM(ch)) == NOBODY) {
			msg_to_char(ch, "This isn't anybody's home.\r\n");
		}
		else if (GET_POS(ch) < POS_STANDING) {
			msg_to_char(ch, "You can't do that right now. You need to be standing.\r\n");
		}
		else if (!GET_LOYALTY(ch) || ROOM_OWNER(real) != GET_LOYALTY(ch)) {
			msg_to_char(ch, "You need to own a building to make it your home.\r\n");
		}
		else if (ROOM_PRIVATE_OWNER(real) != GET_IDNUM(ch) && GET_RANK(ch) < EMPIRE_NUM_RANKS(emp)) {
			msg_to_char(ch, "You can't take away somebody's home.\r\n");
		}
		else {
			clear_private_owner(ROOM_PRIVATE_OWNER(IN_ROOM(ch)));
			msg_to_char(ch, "This home's private owner has been cleared.\r\n");
		}
	}
	else if (!str_cmp(argument, "unset")) {
		clear_private_owner(GET_IDNUM(ch));
		msg_to_char(ch, "Your home has been unset.\r\n");
	}
	else {
		msg_to_char(ch, "Usage: home [set | unset | clear]\r\n");
	}
}


ACMD(do_islands) {
	char output[MAX_STRING_LENGTH*2], line[256], emp_arg[MAX_INPUT_LENGTH];
	struct do_islands_data *item, *next_item, *list = NULL;
	struct empire_unique_storage *eus;
	struct empire_territory_data *ter;
	struct empire_storage_data *store;
	struct island_info *isle;
	empire_data *emp;
	room_data *room;
	int id, last_id = -1;
	size_t size, lsize;
	bool overflow = FALSE;
	
	// imms can target empires
	any_one_word(argument, emp_arg);
	if (!*emp_arg || (GET_ACCESS_LEVEL(ch) < LVL_CIMPL && !IS_GRANTED(ch, GRANT_EMPIRES))) {
		emp = GET_LOYALTY(ch);
	}
	else {
		emp = get_empire_by_name(emp_arg);
		if (!emp) {
			msg_to_char(ch, "Unknown empire.\r\n");
			return;
		}
	}
	
	if (IS_NPC(ch) || !ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
		return;
	}
	if (!HAS_ABILITY(ch, ABIL_NAVIGATION)) {
		msg_to_char(ch, "You need to purchase the Navigation ability to do that.\r\n");
		return;
	}
	if (!emp) {
		msg_to_char(ch, "You must be in an empire to do that.\r\n");
		return;
	}
	
	// mark your territory
	for (ter = EMPIRE_TERRITORY_LIST(emp); ter; ter = ter->next) {
		id = GET_ISLAND_ID(ter->room);
		
		if (id != last_id) {
			last_id = id;
			
			if (id != NO_ISLAND) {
				do_islands_has_territory(&list, id);
			}
		}
	}
	
	// compute einv
	for (store = EMPIRE_STORAGE(emp); store; store = store->next) {
		do_islands_add_einv(&list, store->island, store->amount);
	}
	
	// add unique storage
	for (eus = EMPIRE_UNIQUE_STORAGE(emp); eus; eus = eus->next) {
		do_islands_add_einv(&list, eus->island, eus->amount);
	}
	
	// and then build the display while freeing it up
	size = snprintf(output, sizeof(output), "%s%s&0 is on the following islands:\r\n", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
	
	if (!list) {
		size += snprintf(output + size, sizeof(output) - size, " none\r\n");
	}
	
	HASH_ITER(hh, list, item, next_item) {
		isle = get_island(item->id, TRUE);
		room = real_room(isle->center);
		lsize = snprintf(line, sizeof(line), " %s (%d, %d) - ", isle->name, X_COORD(room), Y_COORD(room));
		
		if (item->has_territory) {
			lsize += snprintf(line + lsize, sizeof(line) - lsize, "has territory%s", item->einv_size > 0 ? ", " : "");
		}
		if (item->einv_size > 0) {
			lsize += snprintf(line + lsize, sizeof(line) - lsize, "%d item%s in einventory", item->einv_size, PLURAL(item->einv_size));
		}
		
		if (size + lsize + 3 < sizeof(output)) {
			size += snprintf(output + size, sizeof(output) - size, "%s\r\n", line);
		}
		else {
			overflow = TRUE;
		}
		
		HASH_DEL(list, item);
		free(item);
	}
	
	if (overflow) {
		size += snprintf(output + size, sizeof(output) - size, " and more...\r\n");
	}
	
	page_string(ch->desc, output, TRUE);
}


ACMD(do_tavern) {
	int iter, type = NOTHING, pos, old;
	
	one_argument(argument, arg);
	
	if (*arg) {
		for (iter = 0; type == NOTHING && *tavern_data[iter].name != '\n'; ++iter) {
			if (is_abbrev(arg, tavern_data[iter].name)) {
				type = iter;
			}
		}
	}
	
	if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_TAVERN) || !IS_COMPLETE(IN_ROOM(ch))) {
		show_tavern_status(ch);
		msg_to_char(ch, "You can only change what's being brewed while actually in the tavern.\r\n");
	}
	else if (!GET_LOYALTY(ch) || GET_LOYALTY(ch) != ROOM_OWNER(IN_ROOM(ch))) {
		msg_to_char(ch, "Your empire doesn't own this tavern.\r\n");
	}
	else if (!has_permission(ch, PRIV_WORKFORCE)) {
		msg_to_char(ch, "You need the workforce privilege to change what this tavern is brewing.\r\n");
	}
	else if (!*arg || type == NOTHING) {
		if (type == NOTHING) {
			msg_to_char(ch, "Invalid tavern type. ");	// deliberate lack of CRLF
		}
		else {
			show_tavern_status(ch);
			msg_to_char(ch, "This tavern is currently brewing %s.\r\n", tavern_data[get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_TYPE)].name);
		}
		send_to_char("You can have it make:\r\n", ch);
		for (iter = 0; *tavern_data[iter].name != '\n'; ++iter) {
			msg_to_char(ch, " %s", tavern_data[iter].name);
			for (pos = 0; tavern_data[iter].ingredients[pos] != NOTHING; ++pos) {
				msg_to_char(ch, "%s%s", pos > 0 ? " + " : ": ", get_obj_name_by_proto(tavern_data[iter].ingredients[pos]));
			}
			send_to_char("\r\n", ch);
		}
	}
	else {
		old = get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_TYPE);
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_TYPE, type);
		
		if (extract_tavern_resources(IN_ROOM(ch))) {
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_BREWING_TIME, config_get_int("tavern_brew_time"));
			remove_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_AVAILABLE_TIME);
			msg_to_char(ch, "This tavern will now brew %s.\r\n", tavern_data[type].name);
		}
		else {
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_TYPE, old);
			msg_to_char(ch, "Your empire doesn't have the resources to brew that.\r\n");
		}
	}
}


ACMD(do_tomb) {	
	room_data *tomb = real_room(GET_TOMB_ROOM(ch)), *real = HOME_ROOM(IN_ROOM(ch));
	
	if (IS_NPC(ch)) {
		return;
	}
	
	skip_spaces(&argument);
	
	if (!*argument) {
		if (!tomb) {
			msg_to_char(ch, "You have no tomb set.\r\n");
		}
		else if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
			msg_to_char(ch, "Your tomb is at: %s (%d, %d)\r\n", get_room_name(tomb, FALSE), X_COORD(tomb), Y_COORD(tomb));
		}
		else {
			msg_to_char(ch, "Your tomb is at: %s\r\n", get_room_name(tomb, FALSE));
		}
		
		// additional info
		if (tomb && !can_use_room(ch, tomb, GUESTS_ALLOWED)) {
			msg_to_char(ch, "You no longer have access to that tomb because it's owned by %s.\r\n", ROOM_OWNER(tomb) ? EMPIRE_NAME(ROOM_OWNER(tomb)) : "someone else");
		}
		if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_TOMB) && IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "Use 'tomb set' to change your tomb to this room.\r\n");
		}
	}
	else if (!str_cmp(argument, "set")) {
		if (PLR_FLAGGED(ch, PLR_ADVENTURE_SUMMONED)) {
			msg_to_char(ch, "You can't tomb-set while adventure-summoned.\r\n");
		}
		else if (GET_POS(ch) < POS_STANDING) {
			msg_to_char(ch, "You can't do that right now. You need to be standing.\r\n");
		}
		else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
			msg_to_char(ch, "You need to own a building to make it your tomb.\r\n");
		}
		else if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_TOMB) || !IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't make this place your tomb!\r\n");
		}
		else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE) || ROOM_AFF_FLAGGED(real, ROOM_AFF_HAS_INSTANCE)) {
			msg_to_char(ch, "You can't make this your tomb right now.\r\n");
		}
		else {			
			GET_TOMB_ROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));
			msg_to_char(ch, "You make this your tomb.\r\n");
		}
	}
	else if (!str_cmp(argument, "unset")) {
		if (!tomb) {
			msg_to_char(ch, "You have no tomb set.\r\n");
		}
		else {
			GET_TOMB_ROOM(ch) = NOWHERE;
			msg_to_char(ch, "You no longer have a tomb selected.\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: tomb [set | unset]\r\n");
	}
}


// subcmd == TRADE_EXPORT or TRADE_IMPORT
ACMD(do_import) {
	empire_data *emp = NULL;
	bool imm_access = GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES);
	
	argument = any_one_word(argument, arg);
	
	// optional first arg as empire name
	if (imm_access && *arg && (emp = get_empire_by_name(arg))) {
		argument = any_one_word(argument, arg);
	}
	else if (!IS_NPC(ch)) {
		emp = GET_LOYALTY(ch);
	}
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch) || !ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!emp) {
		msg_to_char(ch, "You must be in an empire to set trade rules.\r\n");
	}
	else if (is_abbrev(arg, "list")) {
		do_import_list(ch, emp, argument, subcmd);
	}
	else if (EMPIRE_IMM_ONLY(emp)) {
		msg_to_char(ch, "Immortal empires cannot trade.\r\n");
	}
	else if (!EMPIRE_HAS_TECH(emp, TECH_TRADE_ROUTES)) {
		msg_to_char(ch, "The empire needs the Trade Routes technology for you to do that.\r\n");
	}
	else if (!imm_access && GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_TRADE)) {
		msg_to_char(ch, "You don't have permission to set trade rules.\r\n");
	}
	else if (is_abbrev(arg, "add")) {
		do_import_add(ch, emp, argument, subcmd);
	}
	else if (is_abbrev(arg, "remove")) {
		do_import_remove(ch, emp, argument, subcmd);
	}
	else if (is_abbrev(arg, "analyze") || is_abbrev(arg, "analysis")) {
		do_import_analysis(ch, emp, argument, subcmd);
	}
	else {
		msg_to_char(ch, "Usage: %s <add | remove | list | analyze>\r\n", trade_type[subcmd]);
	}
}


ACMD(do_inspire) {
	char arg1[MAX_INPUT_LENGTH];
	char_data *vict = NULL;
	int type, cost = 30;
	empire_data *emp = GET_LOYALTY(ch);
	bool any, all = FALSE;
	
	two_arguments(argument, arg, arg1);
	
	if (!str_cmp(arg, "all")) {
		all = TRUE;
		cost *= 2;
	}
	
	if (!can_use_ability(ch, ABIL_INSPIRE, MOVE, cost, NOTHING)) {
		// nope
	}
	else if (!*arg || !*arg1) {
		msg_to_char(ch, "Usage: inspire <name | all> <type>\r\n");
		msg_to_char(ch, "Types:");
		for (type = 0, any = FALSE; *inspire_data[type].name != '\n'; ++type) {
			msg_to_char(ch, "%s%s", (any ? ", " : " "), inspire_data[type].name);
			any = TRUE;
		}
		msg_to_char(ch, "\r\n");
	}
	else if (!all && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (!emp) {
		msg_to_char(ch, "You can only inspire people who are in the same empire as you.\r\n");
	}
	else if ((type = find_inspire(arg1)) == NOTHING) {
		msg_to_char(ch, "That's not a valid way to inspire.\r\n");
	}
	else if (vict && IS_NPC(vict)) {
		msg_to_char(ch, "You can only inspire other players.\r\n");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_INSPIRE)) {
		return;
	}
	else {
		if (SHOULD_APPEAR(ch)) {
			appear(ch);
		}
		
		charge_ability_cost(ch, MOVE, cost, NOTHING, 0, WAIT_ABILITY);
		
		msg_to_char(ch, "You give a powerful speech about %s.\r\n", inspire_data[type].name);
		sprintf(buf, "$n gives a powerful speech about %s.", inspire_data[type].name);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (all) {
			for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = vict->next_in_room) {
				if (ch != vict && !IS_NPC(vict)) {
					perform_inspire(ch, vict, type);
				}
			}
		}
		else if (vict) {
			perform_inspire(ch, vict, type);
		}
	}
}


ACMD(do_pledge) {	
	empire_data *e, *old;

	if (IS_NPC(ch))
		return;

	skip_spaces(&argument);

	if (IS_NPC(ch)) {
		return;
	}
	else if ((old = GET_LOYALTY(ch)) && EMPIRE_LEADER(old) != GET_IDNUM(ch))
		msg_to_char(ch, "You're already a member of an empire.\r\n");
	else if (!(e = get_empire_by_name(argument)))
		msg_to_char(ch, "There is no empire by that name.\r\n");
	else if (GET_LOYALTY(ch) == e) {
		msg_to_char(ch, "You are already a member of that empire. In fact, you seem to be the most forgetful member.\r\n");
	}
	else if (get_cooldown_time(ch, COOLDOWN_LEFT_EMPIRE) > 0) {
		msg_to_char(ch, "You can't pledge again yet.\r\n");
	}
	else if ((IS_GOD(ch) || IS_IMMORTAL(ch)) && !EMPIRE_IMM_ONLY(e))
		msg_to_char(ch, "You may not join an empire.\r\n");
	else if (EMPIRE_IMM_ONLY(e) && !IS_GOD(ch) && !IS_IMMORTAL(ch))
		msg_to_char(ch, "You can't join that empire.\r\n");
	else {
		GET_PLEDGE(ch) = EMPIRE_VNUM(e);
		log_to_empire(e, ELOG_MEMBERS, "%s has offered %s pledge to this empire", PERS(ch, ch, 1), HSHR(ch));
		msg_to_char(ch, "You offer your pledge to %s.\r\n", EMPIRE_NAME(e));
		SAVE_CHAR(ch);
	}
}


ACMD(do_promote) {
	empire_data *e = GET_LOYALTY(ch);
	int to_rank = NOTHING;
	bool file = FALSE;
	char_data *victim;

	if (IS_NPC(ch)) {
		return;
	}

	argument = one_argument(argument, arg);
	skip_spaces(&argument);

	// initial checks
	if (!e) {
		msg_to_char(ch, "You don't belong to any empire.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(e, PRIV_PROMOTE)) {
		// don't use has_permission, it would check the ownership of the room
		msg_to_char(ch, "You can't promote anybody!\r\n");
		return;
	}
	if (!*arg) {
		msg_to_char(ch, "Promote whom?\r\n");
		return;
	}

	// 2nd argument: promote to which level
	if (*argument) {
		if ((to_rank = find_rank_by_name(e, argument)) == NOTHING) {
			msg_to_char(ch, "Invalid rank.\r\n");
			return;
		}
		
		// 1-based not zero-based :-/
		++to_rank;
	}

	// do NOT return from any of these -- let it fall through to the "if (file)" later
	if (!(victim = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else if (victim == ch)
		msg_to_char(ch, "You can't promote yourself!\r\n");
	else if (IS_NPC(victim) || GET_LOYALTY(victim) != e)
		msg_to_char(ch, "That person is not in your empire.\r\n");
	else if ((to_rank != NOTHING ? to_rank : (to_rank = GET_RANK(victim) + 1)) < GET_RANK(victim))
		msg_to_char(ch, "Use demote for that.\r\n");
	else if (to_rank == GET_RANK(victim))
		act("$E is already that rank.", FALSE, ch, 0, victim, TO_CHAR);
	else if (to_rank > EMPIRE_NUM_RANKS(e))
		msg_to_char(ch, "You can't promote someone over the top level.\r\n");
	else if (to_rank >= GET_RANK(ch) && GET_RANK(ch) < EMPIRE_NUM_RANKS(e))
		msg_to_char(ch, "You can't promote someone to that level.\r\n");
	else {
		GET_RANK(victim) = to_rank;

		log_to_empire(e, ELOG_MEMBERS, "%s has been promoted to %s%s!", PERS(victim, victim, 1), EMPIRE_RANK(e, to_rank-1), EMPIRE_BANNER(e));
		send_config_msg(ch, "ok_string");

		// save now
		if (file) {
			store_loaded_char(victim);
			file = FALSE;
		}
		else {
			SAVE_CHAR(victim);
		}
	}
	
	// clean up
	if (file && victim) {
		free_char(victim);
	}
}


ACMD(do_publicize) {
	if (HOME_ROOM(IN_ROOM(ch)) != IN_ROOM(ch))
		msg_to_char(ch, "You can't do that here -- try it in the main room.\r\n");
	else if (!GET_LOYALTY(ch))
		msg_to_char(ch, "You're not in an empire.\r\n");
	else if (GET_LOYALTY(ch) != ROOM_OWNER(IN_ROOM(ch))) {
		msg_to_char(ch, "Your empire doesn't own this area.\r\n");
	}
	else if (!has_permission(ch, PRIV_CLAIM)) {
		msg_to_char(ch, "You don't have permission to do that.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_PUBLIC)) {
		REMOVE_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_PUBLIC);
		REMOVE_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_PUBLIC);
		msg_to_char(ch, "This area is no longer public.\r\n");
	}
	else {
		SET_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_PUBLIC);
		SET_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_PUBLIC);
		msg_to_char(ch, "This area is now public.\r\n");
	}
}


ACMD(do_radiance) {
	struct affected_type *af;
	
	if (affected_by_spell(ch, ATYPE_RADIANCE)) {
		msg_to_char(ch, "You turn off your radiant aura.\r\n");
		affect_from_char(ch, ATYPE_RADIANCE);
	}
	else if (!can_use_ability(ch, ABIL_RADIANCE, NOTHING, 0, NOTHING)) {
		return;
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_RADIANCE)) {
		return;
	}
	else {
		af = create_mod_aff(ATYPE_RADIANCE, -1, APPLY_GREATNESS, 2, ch);
		affect_join(ch, af, 0);
		
		msg_to_char(ch, "You project a radiant aura!\r\n");
		act("$n projects a radiant aura!", TRUE, ch, NULL, NULL, TO_ROOM);
		command_lag(ch, WAIT_ABILITY);
	}
}


/**
* Reclaim action tick
*
* @param char_data *ch
*/
void process_reclaim(char_data *ch) {
	struct empire_political_data *pol;
	empire_data *emp = GET_LOYALTY(ch);
	empire_data *enemy = ROOM_OWNER(IN_ROOM(ch));
	
	if (real_empire(GET_ACTION_VNUM(ch, 0)) != ROOM_OWNER(IN_ROOM(ch))) {
		msg_to_char(ch, "You stop reclaiming as ownership has changed.\r\n");
		GET_ACTION(ch) = ACT_NONE;
	}
	else if (!emp || !enemy || !(pol = find_relation(emp, enemy)) || !IS_SET(pol->type, DIPL_WAR)) {
		msg_to_char(ch, "You stop reclaiming as you are not at war with this empire.\r\n");
		GET_ACTION(ch) = ACT_NONE;
	}
	else if (IS_CITY_CENTER(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't reclaim a city center.\r\n");
		GET_ACTION(ch) = ACT_NONE;
	}
	else if (!can_claim(ch)) {
		msg_to_char(ch, "You stop reclaiming because you can claim no more land.\r\n");
		GET_ACTION(ch) = ACT_NONE;
	}
	else if (--GET_ACTION_TIMER(ch) > 0 && (GET_ACTION_TIMER(ch) % 12) == 0) {
		log_to_empire(enemy, ELOG_HOSTILITY, "An enemy is trying to reclaim (%d, %d)", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
		msg_to_char(ch, "%d minutes remaining to reclaim this acre.\r\n", (GET_ACTION_TIMER(ch) / 12));
	}
	else if (GET_ACTION_TIMER(ch) <= 0) {
		log_to_empire(enemy, ELOG_HOSTILITY, "An enemy has reclaimed (%d, %d)!", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
		msg_to_char(ch, "You have reclaimed this acre for your empire!");

		abandon_room(IN_ROOM(ch));
		claim_room(IN_ROOM(ch), emp);

		/* Transfers wealth, etc */
		save_empire(emp);
		reread_empire_tech(emp);
		if (enemy != emp) {
			reread_empire_tech(enemy);
		}
		GET_ACTION(ch) = ACT_NONE;
	}
}


ACMD(do_reclaim) {	
	struct empire_political_data *pol;
	empire_data *emp, *enemy;
	int x, y, count;
	room_data *to_room;

	if (IS_NPC(ch))
		return;

	emp = GET_LOYALTY(ch);
	enemy = ROOM_OWNER(IN_ROOM(ch));

	if (GET_ACTION(ch) == ACT_RECLAIMING) {
		msg_to_char(ch, "You stop trying to reclaim this acre.\r\n");
		act("$n stops trying to reclaim this acre.", FALSE, ch, NULL, NULL, TO_ROOM);
		GET_ACTION(ch) = ACT_NONE;
	}
	else if (!emp) {
		msg_to_char(ch, "You don't belong to any empire.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're a little busy right now.\r\n");
	}
	else if (emp == enemy) {
		msg_to_char(ch, "Your empire already owns this acre.\r\n");
	}
	else if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_CLAIM)) {
		// this doesn't use has_permission because that would check if the land is owned already
		msg_to_char(ch, "You don't have permission to claim land for the empire.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE)) {
		msg_to_char(ch, "This acre can't be claimed.\r\n");
	}
	else if (IS_CITY_CENTER(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't reclaim a city center.\r\n");
	}
	else if (!enemy) {
		msg_to_char(ch, "This acre isn't claimed.\r\n");
	}
	else if (HOME_ROOM(IN_ROOM(ch)) != IN_ROOM(ch)) {
		msg_to_char(ch, "You must reclaim from the main room of the building.\r\n");
	}
	else if (!can_claim(ch)) {
		msg_to_char(ch, "You can't claim any more land.\r\n");
	}
	else if (!(pol = find_relation(emp, enemy)) || !IS_SET(pol->type, DIPL_WAR)) {
		msg_to_char(ch, "You can only reclaim territory from people you're at war with.\r\n");
	}
	else {
		// secondary validation: Must have 4 claimed tiles adjacent
		count = 0;
		for (x = -1; x <= 1; ++x) {
			for (y = -1; y <= 1; ++y) {
				to_room = real_shift(IN_ROOM(ch), x, y);
				
				if (to_room && ROOM_OWNER(to_room) == emp) {
					++count;
				}
			}
		}
		
		if (count < 4) {
			msg_to_char(ch, "You can only reclaim territory that is adjacent to at least 4 acres you own.\r\n");
		}
		else {
			log_to_empire(enemy, ELOG_HOSTILITY, "An enemy is trying to reclaim (%d, %d)", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
			msg_to_char(ch, "You start to reclaim this acre. It will take 5 minutes.\r\n");
			act("$n starts to reclaim this acre for $s empire!", FALSE, ch, NULL, NULL, TO_ROOM);
			start_action(ch, ACT_RECLAIMING, 12 * SECS_PER_REAL_UPDATE);
			GET_ACTION_VNUM(ch, 0) = ROOM_OWNER(IN_ROOM(ch)) ? EMPIRE_VNUM(ROOM_OWNER(IN_ROOM(ch))) : NOTHING;
		}
	}
}


ACMD(do_reward) {	
	char arg[MAX_INPUT_LENGTH];
	int count, iter;
	char_data *vict;
	bool found;
	
	// count rewards used
	for (count = 0, iter = 0; iter < MAX_REWARDS_PER_DAY; ++iter) {
		if (GET_REWARDED_TODAY(ch, iter) != -1) {
			++count;
		}
	}
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch) || !can_use_ability(ch, ABIL_REWARD, NOTHING, 0, COOLDOWN_REWARD)) {
		// nope
	}
	else if (!*arg) {
		msg_to_char(ch, "Usage: reward <person>\r\n");
		msg_to_char(ch, "You have %d rewards remaining today.\r\n", MAX(0, MAX_REWARDS_PER_DAY - count));
	}
	else if (count >= MAX_REWARDS_PER_DAY) {
		msg_to_char(ch, "You have no more rewards to give out today.\r\n");
	}
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (ch == vict) {
		msg_to_char(ch, "You can't reward yourself.\r\n");
	}
	else if (IS_NPC(vict)) {
		msg_to_char(ch, "You can only reward players, not NPCs.\r\n");
	}
	else if (!GET_LOYALTY(ch) || GET_LOYALTY(vict) != GET_LOYALTY(ch)) {
		msg_to_char(ch, "You can only reward people in your empire.\r\n");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_REWARD)) {
		return;
	}
	else {
		// see if vict was rewarded by me
		found = FALSE;
		for (iter = 0; !found && iter < MAX_REWARDS_PER_DAY; ++iter) {
			if (GET_REWARDED_TODAY(ch, iter) == GET_IDNUM(vict)) {
				found = TRUE;
			}
		}
		
		if (found) {
			act("You have already rewarded $N today.", FALSE, ch, NULL, vict, TO_CHAR);
		}
		else {
			charge_ability_cost(ch, NOTHING, 0, COOLDOWN_REWARD, 30, WAIT_ABILITY);
			
			act("You reward $N with extra bonus experience!", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n rewards you with extra bonus experience!", FALSE, ch, NULL, vict, TO_VICT);
			act("$n rewards $N with extra bonus experience!", TRUE, ch, NULL, vict, TO_NOTVICT);
			
			SAFE_ADD(GET_DAILY_BONUS_EXPERIENCE(vict), 5, 0, UCHAR_MAX, FALSE);
		
			// mark rewarded
			for (iter = 0, found = FALSE; !found && iter < MAX_REWARDS_PER_DAY; ++iter) {
				if (GET_REWARDED_TODAY(ch, iter) == -1) {
					GET_REWARDED_TODAY(ch, iter) = GET_IDNUM(vict);
					found = TRUE;
				}
			}
			SAVE_CHAR(vict);
			SAVE_CHAR(ch);
		
			gain_ability_exp(ch, ABIL_REWARD, 33.4);
		}
	}
}


ACMD(do_roster) {
	extern bool member_is_timed_out_cfu(struct char_file_u *chdata);

	char buf[MAX_STRING_LENGTH * 2], buf1[MAX_STRING_LENGTH * 2], arg[MAX_STRING_LENGTH];
	struct char_file_u chdata;
	int j, days, hours, size;
	empire_data *e;
	char_data *tmp;
	bool timed_out;

	one_word(argument, arg);

	if (!*arg || (GET_ACCESS_LEVEL(ch) < LVL_CIMPL && !IS_GRANTED(ch, GRANT_EMPIRES))) {
		if (!(e = GET_LOYALTY(ch))) {
			msg_to_char(ch, "You don't belong to any empire!\r\n");
			return;
		}
	}
	else if (!(e = get_empire_by_name(arg))) {
		send_to_char("Unknown empire.\r\n", ch);
		return;
	}

	*buf = '\0';
	size = 0;

	for (j = 0; j <= top_of_p_table; j++) {
		load_char((player_table + j)->name, &chdata);
		if (!IS_SET(chdata.char_specials_saved.act, PLR_DELETED)) {
			if (chdata.player_specials_saved.empire == EMPIRE_VNUM(e)) {
				tmp = is_playing(chdata.char_specials_saved.idnum);
			
				timed_out = member_is_timed_out_cfu(&chdata);
				size += snprintf(buf + size, sizeof(buf) - size, "[%d %s] <%s&0> %s%s&0", tmp ? GET_COMPUTED_LEVEL(tmp) : chdata.player_specials_saved.last_known_level, class_data[tmp ? GET_CLASS(tmp) : chdata.player_specials_saved.character_class].name, EMPIRE_RANK(e, (tmp ? GET_RANK(tmp) : chdata.player_specials_saved.rank) - 1), (timed_out ? "&r" : ""), chdata.name);
								
				// online/not
				if (tmp) {
					size += snprintf(buf + size, sizeof(buf) - size, "  - &conline&0%s", IS_AFK(tmp) ? " - &rafk&0" : "");
				}
				else if ((time(0) - chdata.last_logon) < SECS_PER_REAL_DAY) {
					hours = (time(0) - chdata.last_logon) / SECS_PER_REAL_HOUR;
					size += snprintf(buf + size, sizeof(buf) - size, "  - %d hour%s ago%s", hours, PLURAL(hours), (timed_out ? ", &rtimed-out&0" : ""));
				}
				else {	// more than a day
					days = (time(0) - chdata.last_logon) / SECS_PER_REAL_DAY;
					size += snprintf(buf + size, sizeof(buf) - size, "  - %d day%s ago%s", days, PLURAL(days), (timed_out ? ", &rtimed-out&0" : ""));
				}
				
				size += snprintf(buf + size, sizeof(buf) - size, "\r\n");
			}
		}
	}

	snprintf(buf1, sizeof(buf1), "Roster of %s%s&0:\r\n%s", EMPIRE_BANNER(e), EMPIRE_NAME(e), buf);
	page_string(ch->desc, buf1, 1);
}


ACMD(do_territory) {
	struct find_territory_node *node_list = NULL, *node, *next_node;
	empire_data *emp = GET_LOYALTY(ch);
	room_data *iter, *next_iter;
	bool outside_only = TRUE, ok, junk;
	int total, check_x, check_y;
	crop_data *crop = NULL;
	char *remain;
	
	// imms can target an empire, otherwise the only arg is optional sector type
	remain = any_one_word(argument, arg);
	if (*arg && (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES))) {
		if ((emp = get_empire_by_name(arg))) {
			argument = remain;
		}
		else {
			// keep old argument and re-set to own empire
			emp = GET_LOYALTY(ch);
		}
	}
	
	skip_spaces(&argument);
	
	if (!emp) {
		msg_to_char(ch, "You are not in an empire.\r\n");
		return;
	}
	if (!ch->desc || IS_NPC(ch) || !HAS_ABILITY(ch, ABIL_NAVIGATION)) {
		msg_to_char(ch, "You need the Navigation ability to list the coordinates of your territory.\r\n");
		return;
	}
	
	// only if no argument (optional)
	outside_only = *argument ? FALSE : TRUE;
	
	// ready?
	HASH_ITER(world_hh, world_table, iter, next_iter) {
		if (outside_only && GET_ROOM_VNUM(iter) >= MAP_SIZE) {
			continue;
		}
		
		// owned by the empire?
		if (ROOM_OWNER(iter) == emp) {
			if (!outside_only || !is_in_city_for_empire(iter, emp, FALSE, &junk)) {
				// compare request
				if (!*argument) {
					ok = TRUE;
				}
				else if (multi_isname(argument, GET_SECT_NAME(SECT(iter)))) {
					ok = TRUE;
				}
				else if (GET_BUILDING(iter) && multi_isname(argument, GET_BLD_NAME(GET_BUILDING(iter)))) {
					ok = TRUE;
				}
				else if (ROOM_SECT_FLAGGED(iter, SECTF_HAS_CROP_DATA) && (crop = crop_proto(ROOM_CROP_TYPE(iter))) && multi_isname(argument, GET_CROP_NAME(crop))) {
					ok = TRUE;
				}
				else if (multi_isname(argument, get_room_name(iter, FALSE))) {
					ok = TRUE;
				}
				else {
					ok = FALSE;
				}
				
				if (ok) {
					CREATE(node, struct find_territory_node, 1);
					node->loc = iter;
					node->count = 1;
					node->next = node_list;
					node_list = node;
				}
			}
		}
	}
	
	if (node_list) {
		node_list = reduce_territory_node_list(node_list);
	
		if (!*argument) {
			sprintf(buf, "%s%s&0 territory outside of cities:\r\n", EMPIRE_BANNER(emp), EMPIRE_ADJECTIVE(emp));
		}
		else {
			sprintf(buf, "'%s' tiles owned by %s%s&0:\r\n", argument, EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
			CAP(buf);
		}
		
		// display and free the nodes
		total = 0;
		for (node = node_list; node; node = next_node) {
			next_node = node->next;
			total += node->count;
			
			// territory can be off the map (e.g. ships) and get a -1 here
			check_x = X_COORD(node->loc);
			check_y = Y_COORD(node->loc);
			
			if (CHECK_MAP_BOUNDS(check_x, check_y)) {
				sprintf(buf + strlen(buf), "%2d tile%s near%s (%*d, %*d) %s\r\n", node->count, (node->count != 1 ? "s" : ""), (node->count == 1 ? " " : ""), X_PRECISION, check_x, Y_PRECISION, check_y, get_room_name(node->loc, FALSE));
			}
			else {
				sprintf(buf + strlen(buf), "%2d tile%s near%s (unknown) %s\r\n", node->count, (node->count != 1 ? "s" : ""), (node->count == 1 ? " " : ""), get_room_name(node->loc, FALSE));
			}
			free(node);
		}
		
		node_list = NULL;
		sprintf(buf + strlen(buf), "Total: %d\r\n", total);
		page_string(ch->desc, buf, TRUE);
	}
	else {
		msg_to_char(ch, "No matching territory found.\r\n");
	}
}


ACMD(do_unpublicize) {
	empire_data *e;
	room_data *iter, *next_iter;

	if (!(e = GET_LOYALTY(ch)))
		msg_to_char(ch, "You're not in an empire.\r\n");
	else if (GET_RANK(ch) < EMPIRE_NUM_RANKS(e))
		msg_to_char(ch, "You're of insufficient rank to remove all public status for the empire.\r\n");
	else {
		HASH_ITER(world_hh, world_table, iter, next_iter) {
			if (ROOM_AFF_FLAGGED(iter, ROOM_AFF_PUBLIC) && ROOM_OWNER(iter) == e) {
				REMOVE_BIT(ROOM_AFF_FLAGS(iter), ROOM_AFF_PUBLIC);
				REMOVE_BIT(ROOM_BASE_FLAGS(iter), ROOM_AFF_PUBLIC);
			}
		}
		msg_to_char(ch, "All public status for this empire's buildings has been renounced.\r\n");
	}
}


ACMD(do_workforce) {
	void deactivate_workforce(empire_data *emp, int type);
	void deactivate_workforce_room(empire_data *emp, room_data *room);
	
	int iter, type;
	empire_data *emp;
	
	one_argument(argument, arg);

	if (!(emp = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You must be in an empire to set up the workforce.\r\n");
	}
	else if (!EMPIRE_HAS_TECH(emp, TECH_WORKFORCE)) {
		msg_to_char(ch, "Your empire has no workforce.\r\n");
	}
	else if (!*arg) {
		show_workforce_setup_to_char(emp, ch);		
		msg_to_char(ch, "Use 'workforce no-work' to toggle workforce for this room.\r\n");
	}
	else if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_WORKFORCE)) {
		// this doesn't use has_permission because that would check if the current room is owned
		msg_to_char(ch, "You don't have permission to set up the workforce.\r\n");
	}
	else if (is_abbrev(arg, "nowork") || is_abbrev(arg, "no-work")) {
		// special case: toggle no-work on this tile
		if (ROOM_OWNER(HOME_ROOM(IN_ROOM(ch))) != emp) {
			msg_to_char(ch, "Your empire doesn't own this tile anyway.\r\n");
		}
		else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_WORK)) {
			REMOVE_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_NO_WORK);
			REMOVE_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_NO_WORK);
			msg_to_char(ch, "Workforce will now be able to work this tile.\r\n");
		}
		else {
			SET_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_NO_WORK);
			SET_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_NO_WORK);
			msg_to_char(ch, "Workforce will no longer work this tile.\r\n");
			deactivate_workforce_room(emp, IN_ROOM(ch));
		}
	}
	else if (is_abbrev(arg, "where")) {
		show_workforce_where(emp, ch);
	}
	else {
		// find type to toggle
		for (iter = 0, type = NOTHING; iter < NUM_CHORES && type == NOTHING; ++iter) {
			if (is_abbrev(arg, chore_data[iter].name)) {
				type = iter;
			}
		}
		
		if (type == NOTHING) {
			msg_to_char(ch, "Invalid workforce option.\r\n");
		}
		else {
			EMPIRE_CHORE(emp, type) = !EMPIRE_CHORE(emp, type);
			msg_to_char(ch, "You have %s the %s chore for your empire.\r\n", EMPIRE_CHORE(emp, type) ? "enabled" : "disabled", chore_data[type].name);
			
			if (!EMPIRE_CHORE(emp, type)) {
				deactivate_workforce(emp, type);
			}
		}
	}
}


ACMD(do_withdraw) {
	empire_data *emp, *coin_emp;
	int coin_amt;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't withdraw anything.\r\n");
	}
	else if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_VAULT)) {
		msg_to_char(ch, "You can only withdraw coins in a vault.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You must finish building it first.\r\n");
	}
	else if (!(emp = ROOM_OWNER(IN_ROOM(ch)))) {
		msg_to_char(ch, "No empire stores coins here.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY) || !has_permission(ch, PRIV_WITHDRAW)) {
		// real members only
		msg_to_char(ch, "You don't have permission to withdraw coins here.\r\n");
	}
	else if (find_coin_arg(argument, &coin_emp, &coin_amt, TRUE) == argument || coin_amt < 1) {
		msg_to_char(ch, "Usage: withdraw <number> coins\r\n");
	}
	else if (coin_emp != NULL && coin_emp != emp) {
		// player typed a coin type that didn't match -- ignore OTHER because it likely means they typed no empire arg
		msg_to_char(ch, "Only %s coins are stored here.\r\n", EMPIRE_ADJECTIVE(emp));
	}
	else if (EMPIRE_COINS(emp) < coin_amt) {
		msg_to_char(ch, "The empire doesn't have enough coins.\r\n");
	}
	else {
		msg_to_char(ch, "You withdraw %d coin%s.\r\n", coin_amt, (coin_amt != 1 ? "s" : ""));
		sprintf(buf, "$n withdraws %d coin%s.\r\n", coin_amt, (coin_amt != 1 ? "s" : ""));
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		decrease_empire_coins(emp, emp, coin_amt);
		increase_coins(ch, emp, coin_amt);
	}
}
