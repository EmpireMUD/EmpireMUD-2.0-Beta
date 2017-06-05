/* ************************************************************************
*   File: act.empire.c                                    EmpireMUD 2.0b5 *
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
*   Diplomacy Helpers
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
void deactivate_workforce(empire_data *emp, int island_id, int type);
void deactivate_workforce_room(empire_data *emp, room_data *room);
void eliminate_linkdead_players();
extern int get_total_score(empire_data *emp);
extern char *get_room_name(room_data *room, bool color);
extern bool is_trading_with(empire_data *emp, empire_data *partner);
extern bitvector_t olc_process_flag(char_data *ch, char *argument, char *name, char *command, const char **flag_names, bitvector_t existing_bits);
void identify_obj_to_char(obj_data *obj, char_data *ch);

// locals
bool is_affiliated_island(empire_data *emp, int island_id);
void perform_abandon_city(empire_data *emp, struct empire_city_data *city, bool full_abandon);
void set_workforce_limit(empire_data *emp, int island_id, int chore, int limit);
void set_workforce_limit_all(empire_data *emp, int chore, int limit);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Updates all shipping ids (run before converting vehicle ownership) and moves
* shipping data to the new empire. Call this during an empire merge.
*
* @param empire_data *old_emp
* @param empire_data *new_emp
*/
void convert_empire_shipping(empire_data *old_emp, empire_data *new_emp) {
	extern int find_free_shipping_id(empire_data *emp);
	
	struct shipping_data *sd, *next_sd;
	vehicle_data *veh;
	int old_id, new_id;
	
	LL_FOREACH(vehicle_list, veh) {
		if (VEH_OWNER(veh) != old_emp || VEH_SHIPPING_ID(veh) == -1) {
			continue;
		}
		
		old_id = VEH_SHIPPING_ID(veh);
		new_id = find_free_shipping_id(new_emp);
		
		LL_FOREACH_SAFE(EMPIRE_SHIPPING_LIST(old_emp), sd, next_sd) {
			if (sd->shipping_id == old_id) {
				sd->shipping_id = new_id;
			}
		}
		
		VEH_SHIPPING_ID(veh) = new_id;
	}
	
	// move all shipping entries over
	if ((sd = EMPIRE_SHIPPING_LIST(new_emp))) {
		// append to end
		while (sd->next) {
			sd = sd->next;
		}
		sd->next = EMPIRE_SHIPPING_LIST(old_emp);
	}
	else {
		EMPIRE_SHIPPING_LIST(new_emp) = EMPIRE_SHIPPING_LIST(old_emp);
	}
	
	EMPIRE_SHIPPING_LIST(old_emp) = NULL;
}


/**
* Copies workforce limits from target island into the island the character is currently in.
*
* @param char_data *ch The person requesting the copy.
* @param struct island_info *island Which island to copy the limits from.
*/
void copy_workforce_limits_into_current_island(char_data *ch, struct island_info *from_island) {
	struct empire_island *source_isle = NULL;
	struct island_info *ch_current_island = NULL;
	int iter;
	empire_data *emp = GET_LOYALTY(ch); //This method should only be called if ch belongs to an empire, so this should never return null here.
	
	ch_current_island = get_island(GET_ISLAND_ID(IN_ROOM(ch)), false);
	
	//Error validation
	if (!ch_current_island || ch_current_island->id == NO_ISLAND) {
		msg_to_char(ch, "You are not currently on any island.\r\n");
		return;
	}
	if ( !is_affiliated_island(emp,from_island->id) ) {
		msg_to_char(ch, "Your empire has no affiliation with source island \"%s\".\r\n", get_island_name_for(from_island->id, ch));
		return;
	}
	if ( from_island->id == ch_current_island->id ) {
		msg_to_char(ch, "Your source island can't be the same as your current island.\r\n");
		return;
	}
	
	source_isle = get_empire_island(emp,from_island->id);
	
	//Things went fine? Let's copy!
	for (iter = 0; iter < NUM_CHORES; ++iter) {
		set_workforce_limit(emp, ch_current_island->id, iter, source_isle->workforce_limit[iter]);
		if (source_isle->workforce_limit[iter] == 0) {
			deactivate_workforce(emp, ch_current_island->id, iter);
		}
	}
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
	
	msg_to_char(ch, "Successfully copied workforce limits from %s to %s.\r\n", get_island_name_for(from_island->id, ch), get_island_name_for(ch_current_island->id, ch));
}


/**
* Sub-processor for do_customize: customize island name <name | none>
*
* @param char_data *ch The person trying to customize the island.
* @param char *argument The arguments.
*/
void do_customize_island(char_data *ch, char *argument) {
	extern bool island_has_default_name(struct island_info *island);
	void save_island_table();
	
	char type_arg[MAX_INPUT_LENGTH];
	struct empire_city_data *city;
	struct empire_island *eisle;
	struct island_info *island;
	bool alnum, has_city = FALSE;
	int iter;
	
	const char *allowed_special = " '-";	// chars allowed in island names
	
	// check cities ahead of time
	if (GET_LOYALTY(ch)) {
		LL_FOREACH(EMPIRE_CITY_LIST(GET_LOYALTY(ch)), city) {
			if (GET_ISLAND_ID(city->location) == GET_ISLAND_ID(IN_ROOM(ch)) && (get_room_extra_data(city->location, ROOM_EXTRA_FOUND_TIME) + (config_get_int("minutes_to_full_city") * SECS_PER_REAL_MIN) < time(0))) {
				has_city = TRUE;
				break;
			}
		}
	}
	
	argument = one_argument(argument, type_arg);
	skip_spaces(&argument);
	
	if (!ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!GET_LOYALTY(ch)) {
		msg_to_char(ch, "You must be part of an empire to customize an island.\r\n");;
	}
	else if (!has_permission(ch, PRIV_CUSTOMIZE)) {
		msg_to_char(ch, "You don't have permission to customize anything.\r\n");
	}
	else if (GET_ISLAND_ID(IN_ROOM(ch)) == NO_ISLAND || !(island = get_island(GET_ISLAND_ID(IN_ROOM(ch)), TRUE)) || !(eisle = get_empire_island(GET_LOYALTY(ch), GET_ISLAND_ID(IN_ROOM(ch))))) {
		msg_to_char(ch, "You can't do that here.\r\n");
	}
	else if (IS_SET(island->flags, ISLE_NO_CUSTOMIZE)) {
		msg_to_char(ch, "This island cannot be customized.\r\n");
	}
	else if (!has_city) {
		msg_to_char(ch, "Your empire needs an established city on this island in order to customize it.\r\n");
	}
	else if (!*type_arg || !*argument) {
		msg_to_char(ch, "Usage: customize island <name> <value | none>\r\n");
	}
	
	// types:
	else if (is_abbrev(type_arg, "name")) {
		// check for alphanumeric
		alnum = TRUE;
		for (iter = 0; iter < strlen(argument); ++iter) {
			if (!isalpha(argument[iter]) && !isdigit(argument[iter]) && !strchr(allowed_special, argument[iter])) {
				alnum = FALSE;
			}
		}
		
		if (!*argument) {
			msg_to_char(ch, "What would you like to name this island (or \"none\")?\r\n");
		}
		else if (!str_cmp(argument, "none")) {
			if (!eisle->name) {
				msg_to_char(ch, "Your empire has no custom name for this island.\r\n");
			}
			else {
				log_to_empire(GET_LOYALTY(ch), ELOG_TERRITORY, "%s has removed the custom name for %s (%s)", PERS(ch, ch, TRUE), eisle->name, island->name);
				msg_to_char(ch, "This island no longer has a custom name.\r\n");
				
				free(eisle->name);
				eisle->name = NULL;
				EMPIRE_NEEDS_SAVE(GET_LOYALTY(ch)) = TRUE;
			}
		}
		else if (color_code_length(argument) > 0) {
			msg_to_char(ch, "You cannot use color codes in custom names.\r\n");
		}
		else if (strlen(argument) > 30) {
			msg_to_char(ch, "That name is too long. Island names may not be over 30 characters.\r\n");
		}
		else if (!alnum) {
			msg_to_char(ch, "Island names must be alphanumeric.\r\n");
		}
		else {
			log_to_empire(GET_LOYALTY(ch), ELOG_TERRITORY, "%s has given %s the custom name of %s", PERS(ch, ch, TRUE), get_island_name_for(island->id, ch), argument);
			msg_to_char(ch, "It is now called \"%s\".\r\n", argument);
			
			if (eisle->name) {
				free(eisle->name);
			}
			eisle->name = str_dup(CAP(trim(argument)));
			EMPIRE_NEEDS_SAVE(GET_LOYALTY(ch)) = TRUE;
			
			// need to apply global name?
			if (island_has_default_name(island)) {
				syslog(SYS_INFO, GET_INVIS_LEV(ch), TRUE, "%s has renamed island %d (%s) to %s", GET_NAME(ch), island->id, NULLSAFE(island->name), eisle->name);
				
				if (island->name) {
					free(island->name);
				}
				island->name = str_dup(eisle->name);
				save_island_table();
			}
		}
	}
	else {
		msg_to_char(ch, "You can customize: name\r\n");
	}
}


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
* Copies workforce limits from target island into the island the character is currently in.
*
* @param empire_data *emp The empire to test the island against.
* @param struct empire_island *isle Which island check empire relevancy.
* @return bool True if the empire is affiliated to that island or false if not.
*/
bool is_affiliated_island(empire_data *emp, int island_id) {
	struct empire_island *isle;
	struct empire_unique_storage *eus;
	struct empire_storage_data *store;
	
	//Grab the empire_isle information.
	isle = get_empire_island(emp,island_id);
	
	//Check if the empire has claimed tiles in the island.
	if ( isle->city_terr > 0 || isle->outside_terr > 0) {
		return true;
	}
	
	//Check if the empire has at least an item in there.
	for (store = EMPIRE_STORAGE(emp); store; store = store->next) {
		if (isle->island == store->island) {
			return true;
		}
	}
	
	//Check unique storage too
	for (eus = EMPIRE_UNIQUE_STORAGE(emp); eus; eus = eus->next) {
		if (isle->island == eus->island) {
			return true;
		}
	}
	
	return false;
}

/**
* Sets the workforce limit on one island.
* 0: Do not work
* -1: Use natural limit
* >0: How much to produce before stopping
*
* @param empire_data *emp The empire.
* @param int island_id Which island we're on.
* @param int chore Which CHORE_ type.
* @param int limit The workforce limit to set.
*/
void set_workforce_limit(empire_data *emp, int island_id, int chore, int limit) {
	struct empire_island *isle;
	
	// sanity
	if (!emp || island_id == NO_ISLAND || chore < 0 || chore >= NUM_CHORES) {
		return;
	}
	if (!(isle = get_empire_island(emp, island_id))) {
		return;
	}
	
	isle->workforce_limit[chore] = limit;
}


/**
* Sets the workforce limit for all islands an empire controls.
*
* @param empire_data *emp The empire.
* @param int chore Which CHORE_ type.
* @param int limit The workforce limit to set.
*/
void set_workforce_limit_all(empire_data *emp, int chore, int limit) {
	struct empire_island *isle, *next_isle;
	
	// sanity
	if (!emp || chore < 0 || chore >= NUM_CHORES) {
		return;
	}
	
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		// update it if it's populated OR it already has chore data
		if (isle->population > 0 || isle->workforce_limit[chore]) {
			isle->workforce_limit[chore] = limit;
		}
	}
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
	player_index_data *index;
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
		{ DIPL_WAR, "battle", "At war with", FALSE },
		{ DIPL_TRADE, "trade", "Trade relations with", TRUE },
		
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
	
	msg_to_char(ch, "%s%s&0%s, led by %s\r\n", EMPIRE_BANNER(e), EMPIRE_NAME(e), line, (index = find_player_index_by_idnum(EMPIRE_LEADER(e))) ? index->fullname : "(Unknown)");
	
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
	msg_to_char(ch, "Wealth: %.1f coin%s (at %d%%), %d treasure (%d total)\r\n", EMPIRE_COINS(e), (EMPIRE_COINS(e) != 1.0 ? "s" : ""), (int)(COIN_VALUE * 100), EMPIRE_WEALTH(e), (int) GET_TOTAL_WEALTH(e));
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
	
	if (is_own_empire || !EMPIRE_IS_TIMED_OUT(e)) {
		// display political information by diplomacy type
		for (iter = 0; diplomacy_display[iter].type != NOTHING; ++iter) {
			if (!diplomacy_display[iter].offers_only) {
				found = FALSE;
				for (emp_pol = EMPIRE_DIPLOMACY(e); emp_pol; emp_pol = emp_pol->next) {
					if (IS_SET(emp_pol->type, diplomacy_display[iter].type) && (other = real_empire(emp_pol->id)) && !EMPIRE_IS_TIMED_OUT(other)) {
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

		// Show things that are marked offer_only but have no other relation
		for (iter = 0; diplomacy_display[iter].type != NOTHING; ++iter) {
			if (!diplomacy_display[iter].offers_only) {
				continue;
			}
			
			found = FALSE;
			for (emp_pol = EMPIRE_DIPLOMACY(e); emp_pol; emp_pol = emp_pol->next) {
				if (emp_pol->type == diplomacy_display[iter].type && (other = real_empire(emp_pol->id)) && !EMPIRE_IS_TIMED_OUT(other)) {
					if (!found) {
						msg_to_char(ch, "%s ", diplomacy_display[iter].text);
					}
			
					msg_to_char(ch, "%s%s%s&0", (found ? ", " : ""), EMPIRE_BANNER(other), EMPIRE_NAME(other));
					found = TRUE;
				}
			}
		
			if (found) {
				msg_to_char(ch, ".\r\n");
			}
		}
		
		// now show any open offers
		for (iter = 0; diplomacy_display[iter].type != NOTHING; ++iter) {
			found = FALSE;
			for (emp_pol = EMPIRE_DIPLOMACY(e); emp_pol; emp_pol = emp_pol->next) {
				// only show offers to members
				if (is_own_empire || (GET_LOYALTY(ch) && EMPIRE_VNUM(GET_LOYALTY(ch)) == emp_pol->id)) {
					if (IS_SET(emp_pol->offer, diplomacy_display[iter].type) && (other = real_empire(emp_pol->id)) && !EMPIRE_IS_TIMED_OUT(other)) {
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
* called by do_empire_identify to show eidentify
*
* @param char_data *ch The player requesting the einv.
* @param empire_data *emp The empire whose inventory item to identify.
* @param char *argument The requested inventory item.
*/
static void show_empire_identify_to_char(char_data *ch, empire_data *emp, char *argument) {
	//Helper structure.
	struct eid_per_island {
		int island;
		int quantity;
		UT_hash_handle hh;
	};
	
	struct empire_storage_data *store;
	struct eid_per_island *eid_pi, *eid_pi_next, *eid_pi_list = NULL;
	obj_data *proto = NULL;
	
	
	if (!ch || !ch->desc) {
		return;
	}
	
	if (!*argument){
		msg_to_char(ch, "Which stored item would you like to identify?\r\n");
		return;
	}
	
	for (store = EMPIRE_STORAGE(emp); store; store = store->next) {
		//If there isn't an item proto yet, the first item that matches the given argument will become the item used for the rest of the loop.
		if ( !proto ) {
			if (!multi_isname(argument, GET_OBJ_KEYWORDS(obj_proto(store->vnum)))) {
				continue;
			} else {
				proto = obj_proto(store->vnum);
			}
		}else if ( proto->vnum != store->vnum){
			continue;
		}
		
		//We have a match.
		CREATE(eid_pi, struct eid_per_island, 1);
		eid_pi->island = store->island;
		eid_pi->quantity = store->amount;
		HASH_ADD_INT(eid_pi_list, island, eid_pi);
		
	}
	if ( !proto ) {
		msg_to_char(ch, "This empire has no item by that name.\r\n");
		return;
	}
	
	identify_obj_to_char(proto, ch);
	
	msg_to_char(ch, "Storage list: \r\n");
	HASH_ITER(hh, eid_pi_list, eid_pi, eid_pi_next) {
		msg_to_char(ch, "(%4d) %s\r\n", eid_pi->quantity, get_island_name_for(eid_pi->island, ch));
		// Cleaning as we use the hash.
		HASH_DEL(eid_pi_list, eid_pi);
		free(eid_pi);
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
		
		HASH_FIND_INT(list, &vnum, einv);
		
		if (einv) {	// have this?
			einv->total += shipd->amount;
		}
		else if (all) {	// add an entry
			CREATE(einv, struct einv_type, 1);
			einv->vnum = vnum;
			einv->total = shipd->amount;
			HASH_ADD_INT(list, vnum, einv);
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
* Shows current workforce settings for one chore, to a character.
*
* @param empire_data *emp The empire whose settings to show.
* @param char_data *ch The person to show it to.
* @param int chore Which CHORE_ to show.
*/
void show_detailed_workforce_setup_to_char(empire_data *emp, char_data *ch, int chore) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	struct empire_island *isle, *next_isle;
	struct island_info *island;
	size_t size;
	bool found;
	
	if (!emp || chore < 0 || chore >= NUM_CHORES) {
		msg_to_char(ch, "No workforce is set up.\r\n");
	}
	
	size = snprintf(buf, sizeof(buf), "%s workforce setup for %s%s&0:\r\n", chore_data[chore].name, EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
	CAP(buf);
	
	found = FALSE;
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		// skip island if nothing to show
		if (isle->population <= 0 && isle->workforce_limit[chore] == 0) {
			continue;
		}
		
		// look up island data (for name)
		if (!(island = get_island(isle->island, TRUE))) {
			continue;
		}
		
		if (isle->workforce_limit[chore] == WORKFORCE_UNLIMITED) {
			snprintf(part, sizeof(part), "%s: &con&0%s", get_island_name_for(island->id, ch), isle->population <= 0 ? " (no citizens)" : "");
		}
		else if (isle->workforce_limit[chore] == 0) {
			snprintf(part, sizeof(part), "%s: &yoff&0%s", get_island_name_for(island->id, ch), isle->population <= 0 ? " (no citizens)" : "");
		}
		else {
			snprintf(part, sizeof(part), "%s: &climit %d&0%s", get_island_name_for(island->id, ch), isle->workforce_limit[chore], isle->population <= 0 ? " (no citizens)" : "");
		}
		
		if (size + strlen(part) + 3 < sizeof(buf)) {
			size += snprintf(buf + size, sizeof(buf) - size, " %s\r\n", part);
		}
		
		found = TRUE;
	}
	
	if (!found) {
		size += snprintf(buf + size, sizeof(buf) - size, " no islands found\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Finds workforce mobs belonging to an empire and reports on how many there
* are.
*
* @param empire_data *emp The empire to check.
* @param char_data *to The person to show the info to.
* @param bool here If it should filter workforce in the same island.
*/
void show_workforce_where(empire_data *emp, char_data *to, bool here) {
	// helper data type
	struct workforce_count_type {
		int chore;
		int count;
		UT_hash_handle hh;
	};

	struct workforce_count_type *find, *wct, *next_wct, *counts = NULL;
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	char_data *ch_iter;
	int chore, iter, requesters_island;
	size_t size;
	
	if (!emp) {
		msg_to_char(to, "No empire workforce found.\r\n");
		return;
	}
	
	requesters_island = GET_ISLAND_ID(IN_ROOM(to));
	
	// count up workforce mobs
	for (ch_iter = character_list; ch_iter; ch_iter = ch_iter->next) {
		if (!IS_NPC(ch_iter) || GET_LOYALTY(ch_iter) != emp) {
			continue;
		}
		
		if ( here && requesters_island != GET_ISLAND_ID(IN_ROOM(ch_iter))) {
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
	struct empire_island *isle, *next_isle;
	char part[MAX_STRING_LENGTH];
	int iter, on, off, size;
	
	if (!emp) {
		msg_to_char(ch, "No workforce is set up.\r\n");
	}
	
	msg_to_char(ch, "Workforce setup for %s%s&0:\r\n", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
	
	for (iter = 0; iter < NUM_CHORES; ++iter) {
		// determine if any/all islands have it on
		on = off = 0;
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
			// only count islands with territory
			if (isle->city_terr == 0 && isle->outside_terr == 0) {
				continue;
			}
			
			if (isle->workforce_limit[iter] == 0) {
				++off;
			}
			else {
				++on;
			}
		}
		
		snprintf(part, sizeof(part), "%s: %s", chore_data[iter].name, (on == 0) ? "&yoff&0" : ((off == 0) ? "&con&0" : "&mpart&0"));
		size = 24 + color_code_length(part);
		msg_to_char(ch, " %-*.*s%s", size, size, part, (PRF_FLAGGED(ch, PRF_SCREEN_READER) || !((iter+1)%3)) ? "\r\n" : " ");
	}
	if (iter % 3 && !PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
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
	
	read_empire_territory(emp, FALSE);
	save_empire(emp);
}


/**
* Checks if a building has the IN-CITY-ONLY flag and whether or not the room
* is in a city. For interior rooms inside a building, it ALSO checks the main
* building.
*
* @param room_data *room The room to check.
* @param bool check_wait If TRUE, requires that the city is over an hour old (or as-configured).
* @return bool TRUE if the room is okay to use, FALSE if it failed this check.
*/
bool check_in_city_requirement(room_data *room, bool check_wait) {
	room_data *home = HOME_ROOM(room);
	bool junk;
	
	if (!ROOM_BLD_FLAGGED(room, BLD_IN_CITY_ONLY) && !ROOM_BLD_FLAGGED(home, BLD_IN_CITY_ONLY)) {
		return TRUE;
	}
	if (ROOM_OWNER(room) && is_in_city_for_empire(room, ROOM_OWNER(room), check_wait, &junk)) {
		return TRUE;
	}
	
	return FALSE;
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
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}


void claim_city(char_data *ch, char *argument) {
	char arg1[MAX_INPUT_LENGTH];
	empire_data *emp = GET_LOYALTY(ch);
	struct empire_city_data *city;
	int x, y, radius;
	room_data *iter, *next_iter, *to_room, *center, *home;
	bool found = FALSE, all = FALSE;
	int len;
	
	// look for the "all" at the end
	len = strlen(argument);
	if (!str_cmp(argument + len - 4, " all")) {
		argument[len - 4] = '\0';
		all = TRUE;
	}
	else if (!str_cmp(argument + len - 5, " -all")) {
		argument[len - 5] = '\0';
		all = TRUE;
	}
	else if (!str_cmp(argument + len - 3, " -a")) {
		argument[len - 3] = '\0';
		all = TRUE;
	}
	
	// remove trailing spaces
	while (argument[strlen(argument)-1] == ' ') {
		argument[strlen(argument)-1] = '\0';
	}
	
	any_one_word(argument, arg1);	// city name
	
	// check legality
	if (!emp) {
		msg_to_char(ch, "You aren't even in an empire.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_CLAIM)) {
		msg_to_char(ch, "You don't have permission to claim territory.\r\n");
		return;
	}
	if (!*arg1) {
		msg_to_char(ch, "Usage: city claim <name> [all]\r\n");
		return;
	}
	if (!(city = find_city_by_name(emp, arg1))) {
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
			
			if (!to_room || compute_distance(center, to_room) > radius) {
				continue;
			}
			if (ROOM_OWNER(to_room) || ROOM_AFF_FLAGGED(to_room, ROOM_AFF_HAS_INSTANCE)) {
				continue;
			}
			if (ROOM_SECT_FLAGGED(to_room, SECTF_NO_CLAIM)) {
				continue;
			}
			
			// ok...
			if (all || (SECT(to_room) != BASE_SECT(to_room))) {
				found = TRUE;
				claim_room(to_room, emp);
			}
		}
	}
	
	if (found) {
		// update the inside (interior rooms only)
		for (iter = interior_room_list; iter; iter = next_iter) {
			next_iter = iter->next_interior;
			
			home = HOME_ROOM(iter);
			if (home != iter && ROOM_OWNER(home) == emp) {
				claim_room(iter, emp);
			}
		}
				
		msg_to_char(ch, "You have claimed all %s in the %s of %s.\r\n", all ? "tiles" : "unclaimed structures", city_type[city->type].name, city->name);
	}
	else {
		msg_to_char(ch, "The %s of %s has no unclaimed %s.\r\n", city_type[city->type].name, city->name, all ? "tiles" : "structures");
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
	
	read_empire_territory(emp, FALSE);
	save_empire(emp);
}


void found_city(char_data *ch, char *argument) {
	extern struct empire_city_data *create_city_entry(empire_data *emp, char *name, room_data *location, int type);
	void stop_room_action(room_data *room, int action, int chore);
	extern int num_of_start_locs;
	extern int *start_locs;
	
	empire_data *emp = get_or_create_empire(ch);
	empire_data *emp_iter, *next_emp;
	struct island_info *isle;
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
	if ((isle = get_island(GET_ISLAND_ID(IN_ROOM(ch)), TRUE)) && IS_SET(isle->flags, ISLE_NEWBIE) && !config_get_bool("cities_on_newbie_islands")) {
		msg_to_char(ch, "You can't found a city on a newbie island.\r\n");
		return;
	}
	if (ROOM_IS_CLOSED(IN_ROOM(ch)) || COMPLEX_DATA(IN_ROOM(ch)) || WATER_SECT(IN_ROOM(ch)) || ROOM_SECT_FLAGGED(IN_ROOM(ch), nocity_flags)) {
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
	if (color_code_length(argument) > 0) {
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
	
	if (has_ability(ch, ABIL_NAVIGATION)) {
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
	
	read_empire_territory(emp, FALSE);
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
	
	*too_soon = FALSE;
	
	if (!emp) {
		return FALSE;
	}
	
	// secondary territory counts as in-city
	if (ROOM_BLD_FLAGGED(loc, BLD_SECONDARY_TERRITORY)) {
		return TRUE;
	}
	
	for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
		dist = compute_distance(loc, city->location);
		
		if (dist <= city_type[city->type].radius || (LARGE_CITY_RADIUS(loc) && dist <= (3 * city_type[city->type].radius))) {
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
		msg_to_char(ch, "%d. (%*d, %*d) %s, on %s (%s/%d), traits: %s%s\r\n", ++count, X_PRECISION, X_COORD(rl), Y_PRECISION, Y_COORD(rl), city->name, get_island_name_for(isle->id, ch), city_type[city->type].name, city_type[city->type].radius, buf, pending ? " &r(new)&0" : "");
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
	if (color_code_length(newname) > 0) {
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
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
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
	read_empire_territory(emp, FALSE);
	save_empire(emp);
}


 //////////////////////////////////////////////////////////////////////////////
//// DIPLOMACY HELPERS ///////////////////////////////////////////////////////

#define POL_FLAGGED(pol, flg)  (IS_SET((pol)->type, (flg)) == (flg))
#define POL_OFFERED(pol, flg)  (IS_SET((pol)->offer, (flg)) == (flg))

#define DIPF_REQUIRE_PRESENCE  BIT(0)	// Only works if the other side is online.
#define DIPF_UNILATERAL  BIT(1)	// No need to offer first.
#define DIPF_WAR_COST  BIT(2)	// Empire must pay to do this.
#define DIPF_NOT_MUTUAL_WAR  BIT(3)	// Won't work if mutual_war_only is set
#define DIPF_ALLOW_FROM_NEUTRAL  BIT(4)	// Can be used if no relations at all

struct diplomacy_type {
	char *keywords;	// keyword list (first word is displayed to players, any word matches)
	bitvector_t add_bits;	// DIPL_ represented by this
	bitvector_t remove_bits;	// DIPL_ to be removed
	bitvector_t requires_bits;	// DIPL_ required to use this
	bitvector_t flags;	// DIPF_ flags for do_diplomacy
	char *desc;	// short explanation
} diplo_option[] = {
	{ "peace", DIPL_PEACE, ALL_DIPLS_EXCEPT(DIPL_TRADE), DIPL_WAR | DIPL_DISTRUST | DIPL_TRUCE, DIPF_ALLOW_FROM_NEUTRAL, "end a war or state of distrust" },
	{ "truce", DIPL_TRUCE, ALL_DIPLS_EXCEPT(DIPL_TRADE), DIPL_WAR | DIPL_DISTRUST, NOBITS, "end a war without declaring peace" },
	
	{ "alliance ally", DIPL_ALLIED, ALL_DIPLS_EXCEPT(DIPL_TRADE), DIPL_NONAGGR, NOBITS, "propose or accept a full alliance" },
	{ "nonaggression pact", DIPL_NONAGGR, ALL_DIPLS_EXCEPT(DIPL_TRADE), DIPL_TRUCE | DIPL_PEACE, DIPF_ALLOW_FROM_NEUTRAL, "propose or accept a pact of non-aggression" },
	{ "trade trading", DIPL_TRADE, NOBITS, NOBITS, DIPF_ALLOW_FROM_NEUTRAL, "propose or accept a trade agreement" },
	{ "distrust", DIPL_DISTRUST, ALL_DIPLS, NOBITS, DIPF_UNILATERAL, "declare that your empire distrusts, but is not at war with, another" },
	
	{ "war", DIPL_WAR, ALL_DIPLS, NOBITS, DIPF_UNILATERAL | DIPF_WAR_COST | DIPF_REQUIRE_PRESENCE | DIPF_NOT_MUTUAL_WAR, "declare war on an empire!" },
	{ "battle", DIPL_WAR, ALL_DIPLS, NOBITS, DIPF_REQUIRE_PRESENCE | DIPF_ALLOW_FROM_NEUTRAL, "suggest a friendly war" },
	
	{ "\n", NOBITS, NOBITS, NOBITS, NOBITS }	// this goes last
};


/**
* Matches input text to a diplomacy option.
*
* @param char *arg The typed-in arg (may be 1 or more keywords).
* @return int The diplo_option[] position, or NOTHING if no match.
*/
int find_diplomacy_option(char *arg) {
	int iter;
	for (iter = 0; *(diplo_option[iter].keywords) != '\n'; ++iter) {
		if (multi_isname(arg, diplo_option[iter].keywords)) {
			return iter;
		}
	}
	return NOTHING;
}


 //////////////////////////////////////////////////////////////////////////////
//// EFIND HELPERS ///////////////////////////////////////////////////////////

// for better efind
struct efind_group {
	room_data *location;	// where
	obj_data *obj;	// 1st object for this set (used for names, etc)
	vehicle_data *veh;	// 1st vehicle for this set
	int count;	// how many found
	bool stackable;	// whether or not this can stack
	
	struct efind_group *next;
};


/**
* simple increment/add function for managing efind groups -- supports obj or
* vehicle (not both in 1 call).
*
* @param struct efind_group **list The list to add to/update.
* @param obj_data *obj The object to add (optional; use NULL if none).
* @param vehicle_data *veh The vehicle to add (optional; use NULL if none).
* @param room_data *location Where it is.
*/
void add_obj_to_efind(struct efind_group **list, obj_data *obj, vehicle_data *veh, room_data *location) {
	struct efind_group *eg, *temp;
	bool found = FALSE;
	
	// need 1 or the other
	if (!obj && !veh) {
		return;
	}
	
	if (obj && OBJ_CAN_STACK(obj)) {
		LL_FOREACH(*list, eg) {
			if (eg->location == location && eg->stackable && GET_OBJ_VNUM(eg->obj) == GET_OBJ_VNUM(obj)) {
				eg->count += 1;
				found = TRUE;
				break;
			}
		}
	}
	if (veh) {
		LL_FOREACH(*list, eg) {
			if (eg->location != location) {
				continue;
			}
			if (!eg->veh || VEH_VNUM(eg->veh) != VEH_VNUM(veh)) {
				continue;
			}
			if (VEH_SHORT_DESC(eg->veh) != VEH_SHORT_DESC(veh)) {
				continue;
			}
			
			eg->count += 1;
			found = TRUE;
			break;
		}
	}
	
	if (!found) {
		CREATE(eg, struct efind_group, 1);
		
		eg->location = location;
		eg->obj = obj;
		eg->veh = veh;
		eg->count = 1;
		eg->stackable = obj ? OBJ_CAN_STACK(obj) : FALSE;	// not used for vehicle
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
	double cost;
	int limit;
	obj_vnum vnum = NOTHING;
	obj_data *obj;
	
	// 3 args
	argument = one_argument(argument, cost_arg);
	argument = one_argument(argument, limit_arg);
	skip_spaces(&argument);	// remaining arg is item name
	
	// for later
	cost = floor(atof(cost_arg) * 10.0) / 10.0;	// round to 0.1f
	limit = atoi(limit_arg);
	
	if (!*cost_arg || !*limit_arg || !*argument) {
		msg_to_char(ch, "Usage: %s add <%scost> <%s limit> <item name>\r\n", trade_type[subcmd], trade_mostleast[subcmd], trade_overunder[subcmd]);
	}
	else if (!isdigit(*cost_arg) && *cost_arg != '.') {
		msg_to_char(ch, "Cost must be a number, '%s' given.\r\n", cost_arg);
	}
	else if (!isdigit(*limit_arg)) {
		msg_to_char(ch, "Limit must be a number, '%s' given.\r\n", limit_arg);
	}
	else if (cost < 0.1 || cost > MAX_COIN) {
		msg_to_char(ch, "That cost must be between 0.1 and %d.\r\n", MAX_COIN);
	}
	else if (limit < 0 || limit > MAX_COIN) {
		// max coin is a safe limit here
		msg_to_char(ch, "That limit is out of bounds.\r\n");
	}
	else if (((obj = get_obj_in_list_vis(ch, argument, ch->carrying)) || (obj = get_obj_in_list_vis(ch, argument, ROOM_CONTENTS(IN_ROOM(ch))))) && ((vnum = GET_OBJ_VNUM(obj)) == NOTHING || !obj->storage)) {
		// targeting an item in room/inventory
		act("$p can't be traded.", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else if (vnum == NOTHING && (vnum = get_obj_vnum_by_name(argument, TRUE)) == NOTHING) {
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
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
		
		msg_to_char(ch, "%s now %ss '%s' costing %s%.1f, when %s %d.\r\n", EMPIRE_NAME(emp), trade_type[subcmd], get_obj_name_by_proto(vnum), trade_mostleast[subcmd], cost, trade_overunder[subcmd], limit);
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
	else if (vnum == NOTHING && (vnum = get_obj_vnum_by_name(argument, TRUE)) == NOTHING) {
		msg_to_char(ch, "Unknown item '%s'.\r\n", argument);
	}
	else if (!(trade = find_trade_entry(emp, subcmd, vnum))) {
		msg_to_char(ch, "The empire doesn't appear to be %sing '%s'.\r\n", trade_type[subcmd], get_obj_name_by_proto(vnum));
	}
	else {
		REMOVE_FROM_LIST(trade, EMPIRE_TRADE(emp), next);
		free(trade);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
		
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
			if (rate != 1.0) {
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
			
			snprintf(line, sizeof(line), " &c%s&0 for %s&y%.1f%s&0 coin%s%s%s\r\n", GET_OBJ_SHORT_DESC(proto), trade_mostleast[use_type], trade->cost, coin_conv, (trade->cost != 1.0 ? "s" : ""), over_part, indicator);
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
	else if (((obj = get_obj_in_list_vis(ch, argument, ch->carrying)) || (obj = get_obj_in_list_vis(ch, argument, ROOM_CONTENTS(IN_ROOM(ch))))) && ((vnum = GET_OBJ_VNUM(obj)) == NOTHING || !obj->storage)) {
		// targeting an item in room/inventory
		act("$p can't be traded.", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else if (vnum == NOTHING && (vnum = get_obj_vnum_by_name(argument, TRUE)) == NOTHING) {
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
			if (rate != 1.0) {
				snprintf(coin_conv, sizeof(coin_conv), " (%.1f)", trade->cost * rate);
			}
			else {
				*coin_conv = '\0';
			}
			
			sprintf(line, " %s (%d%%) is %sing it at &y%.1f%s coin%s&0%s%s%s\r\n", EMPIRE_NAME(iter), (int)(100 * rate), trade_type[find_type], trade->cost, coin_conv, (trade->cost != 1.0 ? "s" : ""), ((find_type != TRADE_EXPORT || has_avail) ? "" : " (none available)"), ((find_type != TRADE_IMPORT || is_buying) ? "" : " (full)"), (is_trading_with(emp, iter) ? "" : " (not trading)"));
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
	
	affect_from_char(vict, ATYPE_INSPIRE, FALSE);
	
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
	int territory;
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
* @param int amount How much territory on the island.
*/
void do_islands_has_territory(struct do_islands_data **list, int island_id, int amount) {
	struct do_islands_data *isle;
	
	HASH_FIND_INT(*list, &island_id, isle);
	if (!isle) {
		CREATE(isle, struct do_islands_data, 1);
		isle->id = island_id;
		HASH_ADD_INT(*list, id, isle);
	}
	SAFE_ADD(isle->territory, amount, INT_MIN, INT_MAX, TRUE);
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
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
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
		if (room_has_function_and_city_ok(ter->room, FNC_TAVERN)) {
			found = TRUE;
			
			if (has_ability(ch, ABIL_NAVIGATION)) {
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
	vehicle_data *veh;
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
			else if (GET_BUILDING(room) && multi_isname(argument, GET_BLD_NAME(GET_BUILDING(room))) && !CHECK_CHAMELEON(map, room)) {
				ok = TRUE;
			}
			else if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && (crop = ROOM_CROP(room)) && multi_isname(argument, GET_CROP_NAME(crop))) {
				ok = TRUE;
			}
			else if (multi_isname(argument, get_room_name(room, FALSE)) && !CHECK_CHAMELEON(map, room)) {
				ok = TRUE;
			}
			else {
				// try finding a matching vehicle visible in the tile
				LL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
					if (!VEH_ICON(veh) || !VEH_IS_COMPLETE(veh)) {
						continue;
					}
					if (!CAN_SEE_VEHICLE(ch, veh)) {
						continue;
					}
					if (!multi_isname(argument, VEH_KEYWORDS(veh))) {
						continue;
					}
					
					// found a vehicle match (only need 1)
					ok = TRUE;
					break;
				}
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
			
			if (CHECK_MAP_BOUNDS(check_x, check_y) && has_ability(ch, ABIL_NAVIGATION)) {
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

/**
* Command sub-processor for abandoning a room.
*
* @param char_data *ch The player trying to abandon.
* @param room_data *room The room to abandon.
*/
void do_abandon_room(char_data *ch, room_data *room) {
	if (!ROOM_OWNER(room) || ROOM_OWNER(room) != GET_LOYALTY(ch)) {
		msg_to_char(ch, "You don't even own the area.\r\n");
	}
	else if (IS_CITY_CENTER(room)) {
		msg_to_char(ch, "You can't abandon a city center that way -- use \"city abandon\".\r\n");
	}
	else if (HOME_ROOM(room) != room) {
		msg_to_char(ch, "Just abandon the main room.\r\n");
	}
	else {
		if (room != IN_ROOM(ch)) {
			if (has_ability(ch, ABIL_NAVIGATION)) {
				msg_to_char(ch, "(%d, %d) %s abandoned.\r\n", X_COORD(room), Y_COORD(room), get_room_name(room, FALSE));
			}
			else {
				msg_to_char(ch, "%s abandoned.\r\n", get_room_name(room, FALSE));
			}
			if (ROOM_PEOPLE(room)) {
				act("$N abandons $S claim to this area.", FALSE, ROOM_PEOPLE(room), NULL, ch, TO_CHAR | TO_ROOM);
			}
		}
		else {
			msg_to_char(ch, "Territory abandoned.\r\n");
			act("$n abandons $s claim to this area.", FALSE, ch, NULL, NULL, TO_ROOM);
		}
		
		abandon_room(room);
	}
}


/**
* Command sub-processor for abandoning a vehicle.
*
* @param char_data *ch The player trying to abandon.
* @param vehicle_data *veh The vehicle to abandon.
*/
void do_abandon_vehicle(char_data *ch, vehicle_data *veh) {
	empire_data *emp = VEH_OWNER(veh);
	
	if (!emp || VEH_OWNER(veh) != GET_LOYALTY(ch)) {
		msg_to_char(ch, "You don't even own that.\r\n");
	}
	else {
		act("You abandon $V.", FALSE, ch, NULL, veh, TO_CHAR);
		act("$n abandons $V.", FALSE, ch, NULL, veh, TO_ROOM);
		VEH_OWNER(veh) = NULL;
		
		if (VEH_INTERIOR_HOME_ROOM(veh)) {
			abandon_room(VEH_INTERIOR_HOME_ROOM(veh));
		}
		
		if (VEH_IS_COMPLETE(veh)) {
			qt_empire_players(emp, qt_lose_vehicle, VEH_VNUM(veh));
		}
	}
}


ACMD(do_abandon) {
	char arg[MAX_INPUT_LENGTH];
	vehicle_data *veh;
	room_data *room = IN_ROOM(ch);

	if (IS_NPC(ch)) {
		return;
	}
	
	one_word(argument, arg);
	
	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (FIGHTING(ch)) {
		msg_to_char(ch, "You're too busy fighting!\r\n");
	}
	else if (!GET_LOYALTY(ch)) {
		msg_to_char(ch, "You're not part of an empire.\r\n");
	}
	else if (GET_RANK(ch) < EMPIRE_PRIV(GET_LOYALTY(ch), PRIV_CEDE)) {
		// this doesn't use has_permission because that would check if the land is owned already
		msg_to_char(ch, "You don't have permission to abandon.\r\n");
	}
	else if (*arg && (veh = get_vehicle_in_room_vis(ch, arg))) {
		do_abandon_vehicle(ch, veh);
	}
	else if (*arg && !(room = find_target_room(ch, arg))) {
		// sends own error
	}
	else if (!(room = HOME_ROOM(room))) {
		msg_to_char(ch, "You can't abandon that!\r\n");
	}
	else if (GET_ROOM_VEHICLE(room)) {
		do_abandon_vehicle(ch, GET_ROOM_VEHICLE(room));
	}
	else {
		do_abandon_room(ch, room);
	}
}


ACMD(do_barde) {
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	static struct resource_data *res = NULL;
	struct interact_exclusion_data *excl = NULL;
	struct interaction_item *interact;
	char_data *mob, *newmob = NULL;
	bool found;
	double prc;
	int num;
	
	if (!res) {
		add_to_resource_list(&res, RES_COMPONENT, CMP_METAL, 10, CMPF_COMMON);
	}
	
	one_argument(argument, arg);

	if (!can_use_ability(ch, ABIL_BARDE, NOTHING, 0, NOTHING)) {
		// nope
	}
	else if (!HAS_FUNCTION(IN_ROOM(ch), FNC_STABLE) || !IS_COMPLETE(IN_ROOM(ch)))
		msg_to_char(ch, "You must barde animals in the stable.\r\n");
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED))
		msg_to_char(ch, "You don't have permission to barde animals here.\r\n");
	else if (!*arg)
		msg_to_char(ch, "Which animal would you like to barde?\r\n");
	else if (!(mob = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (!IS_NPC(mob)) {
		act("You can't barde $N!", FALSE, ch, 0, mob, TO_CHAR);
	}
	else if (GET_LED_BY(mob)) {
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
					newmob = read_mobile(interact->vnum, TRUE);
					setup_generic_npc(newmob, GET_LOYALTY(mob), MOB_DYNAMIC_NAME(mob), MOB_DYNAMIC_SEX(mob));
					char_to_room(newmob, IN_ROOM(ch));
					MOB_INSTANCE_ID(newmob) = MOB_INSTANCE_ID(mob);
					if (MOB_INSTANCE_ID(newmob) != NOTHING) {
						add_instance_mob(real_instance(MOB_INSTANCE_ID(newmob)), GET_MOB_VNUM(newmob));
					}
		
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
				extract_resources(ch, res, TRUE, NULL);
			}
		}
		else {
			act("You can't barde $N!", FALSE, ch, NULL, mob, TO_CHAR);
		}
		
		free_exclusion_data(excl);
	}
}


ACMD(do_cede) {
	empire_data *e = GET_LOYALTY(ch), *f;
	room_data *room, *iter, *next_iter;
	char_data *targ;
	bool junk;

	if (IS_NPC(ch))
		return;
	
	room = HOME_ROOM(IN_ROOM(ch));
	argument = one_argument(argument, arg);
	skip_spaces(&argument);

	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!e)
		msg_to_char(ch, "You own no territory.\r\n");
	else if (!*arg)
		msg_to_char(ch, "Usage: cede <person> (x, y)\r\n");
	else if (!(targ = get_player_vis(ch, arg, FIND_CHAR_WORLD | FIND_NO_DARK)))
		send_config_msg(ch, "no_person");
	else if (IS_NPC(targ))
		msg_to_char(ch, "You can't cede land to NPCs!\r\n");
	else if (ch == targ)
		msg_to_char(ch, "You can't cede land to yourself!\r\n");
	else if (!PRF_FLAGGED(targ, PRF_BOTHERABLE)) {
		msg_to_char(ch, "You can't cede land to someone with 'bother' toggled off.\r\n");
	}
	else if (*argument && strchr(argument, ',')) {
		msg_to_char(ch, "You can only cede the land you're standing on.\r\n");
	}
	else if (GET_ROOM_VEHICLE(room)) {
		msg_to_char(ch, "You can't cede the inside of a vehicle.\r\n");
	}
	else if (GET_RANK(ch) < EMPIRE_PRIV(e, PRIV_CEDE)) {
		// don't use has_permission here because it would check permits on the room you're in
		msg_to_char(ch, "You don't have permission to cede.\r\n");
	}
	else if (ROOM_OWNER(room) != e)
		msg_to_char(ch, "You don't even own this location.\r\n");
	else if (ROOM_PRIVATE_OWNER(room) != NOBODY) {
		msg_to_char(ch, "You can't cede a private house.\r\n");
	}
	else if (IS_CITY_CENTER(room)) {
		msg_to_char(ch, "You can't cede a city center.\r\n");
	}
	else if ((f = get_or_create_empire(targ)) == NULL)
		msg_to_char(ch, "You can't seem to cede land to %s.\r\n", REAL_HMHR(targ));
	else if (f == e)
		msg_to_char(ch, "You can't cede land to your own empire!\r\n");
	else if (EMPIRE_CITY_TERRITORY(f) + EMPIRE_OUTSIDE_TERRITORY(f) >= land_can_claim(f, FALSE))
		msg_to_char(ch, "You can't cede land to %s, %s empire can't own any more land.\r\n", REAL_HMHR(targ), REAL_HSHR(targ));
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
		for (iter = interior_room_list; iter; iter = next_iter) {
			next_iter = iter->next_interior;
			
			if (HOME_ROOM(iter) == room) {
				set_room_extra_data(iter, ROOM_EXTRA_CEDED, 1);
			}
		}
	}
}


ACMD(do_city) {
	char arg1[MAX_INPUT_LENGTH];

	half_chop(argument, arg, arg1);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: city <list | found | upgrade | downgrade | claim | abandon | rename | traits>\r\n");
	}
	else if (is_abbrev(arg, "list")) {
		list_cities(ch, arg1);
	}
	// all other options require manage-empire
	else if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
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
		msg_to_char(ch, "Usage: city <list | found | upgrade | downgrade | claim | abandon | rename | traits>\r\n");
	}
}


/**
* Processes a "claim" targeting a room.
*
* @param char_data *ch The player trying to claim.
* @param room_data *room The room he's trying to claim.
*/
void do_claim_room(char_data *ch, room_data *room) {
	empire_data *emp = get_or_create_empire(ch);
	bool junk;
	
	if (!emp) {
		msg_to_char(ch, "You don't belong to any empre.\r\n");
	}
	else if (ROOM_OWNER(room) == emp) {
		msg_to_char(ch, "Your empire already owns this area.\r\n");
	}
	else if (IS_CITY_CENTER(room)) {
		msg_to_char(ch, "You can't claim a city center.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(room, SECTF_NO_CLAIM) || ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE | ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "The tile can't be claimed.\r\n");
	}
	else if (ROOM_OWNER(room) != NULL) {
		msg_to_char(ch, "The area is already claimed.\r\n");
	}
	else if (HOME_ROOM(room) != room) {
		msg_to_char(ch, "Just claim the entrance room.\r\n");
	}
	else if (!can_claim(ch)) {
		msg_to_char(ch, "You can't claim any more land.\r\n");
	}
	else if (!can_build_or_claim_at_war(ch, room)) {
		msg_to_char(ch, "You can't claim while at war with the empire that controls this area.\r\n");
	}
	else if (!is_in_city_for_empire(room, emp, FALSE, &junk) && EMPIRE_OUTSIDE_TERRITORY(emp) >= land_can_claim(emp, TRUE)) {
		msg_to_char(ch, "You can't claim the area because you're over the 20%% of your territory that can be outside of cities.\r\n");
	}
	else {
		send_config_msg(ch, "ok_string");
		if (room == IN_ROOM(ch)) {
			act("$n stakes a claim to this area.", FALSE, ch, NULL, NULL, TO_ROOM);
		}
		else if (ROOM_PEOPLE(room)) {
			act("$N stakes a claim to this area.", FALSE, ROOM_PEOPLE(room), NULL, ch, TO_CHAR | TO_ROOM);
		}
		claim_room(room, emp);
	}
}


/**
* Processes a "claim" targeting a vehicle.
*
* @param char_data *ch The player trying to claim.
* @param vehicle_data *veh The vehicle he's trying to claim.
*/
void do_claim_vehicle(char_data *ch, vehicle_data *veh) {
	empire_data *emp = get_or_create_empire(ch);
	
	if (!emp) {
		msg_to_char(ch, "You don't belong to any empre.\r\n");
	}
	else if (VEH_OWNER(veh) == emp) {
		msg_to_char(ch, "Your empire already owns that.\r\n");
	}
	else if (VEH_OWNER(veh)) {
		msg_to_char(ch, "Someone else already owns that.\r\n");
	}
	else {
		send_config_msg(ch, "ok_string");
		act("$n claims $V.", FALSE, ch, NULL, veh, TO_ROOM);
		VEH_OWNER(veh) = emp;
		VEH_SHIPPING_ID(veh) = -1;
		
		if (VEH_INTERIOR_HOME_ROOM(veh)) {
			if (ROOM_OWNER(VEH_INTERIOR_HOME_ROOM(veh))) {
				abandon_room(VEH_INTERIOR_HOME_ROOM(veh));
			}
			claim_room(VEH_INTERIOR_HOME_ROOM(veh), emp);
		}
		
		if (VEH_IS_COMPLETE(veh)) {
			qt_empire_players(emp, qt_gain_vehicle, VEH_VNUM(veh));
		}
	}
}


ACMD(do_claim) {
	char arg[MAX_INPUT_LENGTH];
	vehicle_data *veh;

	if (IS_NPC(ch)) {
		return;
	}
	
	one_argument(argument, arg);
	
	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (FIGHTING(ch)) {
		msg_to_char(ch, "You're too busy fighting!\r\n");
	}
	else if (!get_or_create_empire(ch)) {
		msg_to_char(ch, "You don't belong to any empire.\r\n");
	}
	else if (GET_RANK(ch) < EMPIRE_PRIV(GET_LOYALTY(ch), PRIV_CLAIM)) {
		// this doesn't use has_permission because that would check if the land is owned already
		msg_to_char(ch, "You don't have permission to claim for the empire.\r\n");
	}
	else if (*arg && (veh = get_vehicle_in_room_vis(ch, arg))) {
		do_claim_vehicle(ch, veh);
	}
	else if (*arg) {
		msg_to_char(ch, "You don't see that to claim.\r\n");
	}
	else if (GET_ROOM_VEHICLE(IN_ROOM(ch))) {
		do_claim_vehicle(ch, GET_ROOM_VEHICLE(IN_ROOM(ch)));
	}
	else {
		do_claim_room(ch, IN_ROOM(ch));
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
		add_cooldown(ch, COOLDOWN_LEFT_EMPIRE, 2 * SECS_PER_REAL_HOUR);
		SAVE_CHAR(ch);
		
		log_to_empire(e, ELOG_MEMBERS, "%s has defected from the empire", PERS(ch, ch, 1));
		msg_to_char(ch, "You defect from the empire!\r\n");
		
		remove_lore(ch, LORE_PROMOTED);
		add_lore(ch, LORE_DEFECT_EMPIRE, "Defected from %s%s&0", EMPIRE_BANNER(e), EMPIRE_NAME(e));
		
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

	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
		return;
	}
	else if (!e) {
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
		remove_lore(victim, LORE_PROMOTED);	// only save most recent
		add_lore(victim, LORE_PROMOTED, "Became %s&0", EMPIRE_RANK(e, to_rank-1));

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
	else if (!HAS_FUNCTION(IN_ROOM(ch), FNC_VAULT)) {
		msg_to_char(ch, "You can only deposit coins in a vault.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You must finish building it first.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it.\r\n");
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
	else if ((rate * coin_amt) < 0.1) {
		msg_to_char(ch, "At this exchange rate, that many coins wouldn't be worth anything.\r\n");
	}
	else {
		msg_to_char(ch, "You deposit %s.\r\n", money_amount(coin_emp, coin_amt));
		sprintf(buf, "$n deposits %s.", money_amount(coin_emp, coin_amt));
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		
		increase_empire_coins(emp, coin_emp, coin_amt);
		decrease_coins(ch, coin_emp, coin_amt);
	}
}


ACMD(do_diplomacy) {
	char type_arg[MAX_INPUT_LENGTH], emp_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH], ch_log[MAX_STRING_LENGTH], vict_log[MAX_STRING_LENGTH];
	struct empire_political_data *ch_pol = NULL, *vict_pol = NULL;
	empire_data *ch_emp, *vict_emp;
	int iter, type, war_cost = 0;
	bool cancel = FALSE;
	
	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
		return;
	}
	
	half_chop(argument, type_arg, emp_arg);
	
	// check for optional cancel arg
	if (!str_cmp(type_arg, "cancel")) {
		cancel = TRUE;
		strcpy(buf, emp_arg);
		half_chop(buf, type_arg, emp_arg);
	}
	
	if (!*type_arg) {
		msg_to_char(ch, "Usage: diplomacy <option> <empire>\r\n");
		msg_to_char(ch, "       diplomacy cancel <option> <empire>\r\n");
		msg_to_char(ch, "Options:\r\n");
		
		for (iter = 0; *(diplo_option[iter].keywords) != '\n'; ++iter) {
			msg_to_char(ch, "  \ty%s\t0 - %s\r\n", fname(diplo_option[iter].keywords), diplo_option[iter].desc);
		}
	}
	else if (!(ch_emp = GET_LOYALTY(ch)) || IS_NPC(ch)) {
		msg_to_char(ch, "You can't engage in diplomacy if you're not a member of an empire.\r\n");
	}
	
	// own empire validation
	else if (EMPIRE_IMM_ONLY(ch_emp)) {
		msg_to_char(ch, "Empires belonging to immortals cannot engage in diplomacy.\r\n");
	}
	else if (GET_RANK(ch) < EMPIRE_PRIV(ch_emp, PRIV_DIPLOMACY)) {
		// don't use has_permission, it would check the ownership of the room
		msg_to_char(ch, "You don't have the authority to make diplomatic relations.\r\n");
	}
	
	// option validation
	else if ((type = find_diplomacy_option(type_arg)) == NOTHING) {
		msg_to_char(ch, "Unknown option '%s'.\r\n", type_arg);
	}
	else if (IS_SET(diplo_option[type].flags, DIPF_NOT_MUTUAL_WAR) && config_get_bool("mutual_war_only")) {
		msg_to_char(ch, "This EmpireMUD does not allow you to unilaterally declare %s.\r\n", fname(diplo_option[type].keywords));
	}
	
	// empire validation
	else if (!*emp_arg) {
		if (cancel) {
			msg_to_char(ch, "With which empire would you like to cancel your %s offer?\r\n", fname(diplo_option[type].keywords));
		}
		else {
			msg_to_char(ch, "Which empire would you like to offer %s?\r\n", fname(diplo_option[type].keywords));
		}
	}
	else if (!(vict_emp = get_empire_by_name(emp_arg))) {
		msg_to_char(ch, "Unknown empire '%s'.\r\n", emp_arg);
	}
	else if (EMPIRE_IMM_ONLY(vict_emp)) {
		msg_to_char(ch, "Empires belonging to immortals cannot engage in diplomacy.\r\n");
	}
	else if (ch_emp == vict_emp) {
		msg_to_char(ch, "You can't engage in diplomacy with your own empire!\r\n");
	}
	
	// cancel? (has its own logic not based on current relations)
	else if (cancel) {
		if (!(ch_pol = find_relation(ch_emp, vict_emp)) || !POL_OFFERED(ch_pol, diplo_option[type].add_bits)) {
			msg_to_char(ch, "You haven't offered that to %s.\r\n", EMPIRE_NAME(vict_emp));
		}
		else {
			REMOVE_BIT(ch_pol->offer, diplo_option[type].add_bits);
			log_to_empire(ch_emp, ELOG_DIPLOMACY, "The offer of %s to %s has been canceled", fname(diplo_option[type].keywords), EMPIRE_NAME(vict_emp));
			log_to_empire(vict_emp, ELOG_DIPLOMACY, "%s has withdrawn the offer of %s", EMPIRE_NAME(ch_emp), fname(diplo_option[type].keywords));
			msg_to_char(ch, "You have withdrawn the offer of %s to %s.\r\n", fname(diplo_option[type].keywords), EMPIRE_NAME(vict_emp));
			EMPIRE_NEEDS_SAVE(ch_emp) = TRUE;
		}
		return;
	}
	
	// relationship validation
	else if ((ch_pol = find_relation(ch_emp, vict_emp)) && POL_FLAGGED(ch_pol, DIPL_WAR) && !IS_SET(diplo_option[type].requires_bits, DIPL_WAR)) {
		msg_to_char(ch, "You can't do that while you're at war.\r\n");
	}
	else if (ch_pol && POL_FLAGGED(ch_pol, diplo_option[type].add_bits)) {
		msg_to_char(ch, "You already have that relationship with %s.\r\n", EMPIRE_NAME(vict_emp));
	}
	else if (diplo_option[type].requires_bits && (!IS_SET(diplo_option[type].flags, DIPF_ALLOW_FROM_NEUTRAL) || (ch_pol && IS_SET(ch_pol->type, CORE_DIPLS))) && (!ch_pol || !IS_SET(ch_pol->type, diplo_option[type].requires_bits))) {
		msg_to_char(ch, "You can't do that with your current diplomatic relations.\r\n");
	}
	else if (IS_SET(diplo_option[type].flags, DIPF_REQUIRE_PRESENCE) && count_members_online(vict_emp) == 0) {
		msg_to_char(ch, "You can't do that when no members of %s are online.\r\n", EMPIRE_NAME(vict_emp));
	}
	else if (IS_SET(diplo_option[type].flags, DIPF_WAR_COST) && EMPIRE_COINS(ch_emp) < (war_cost = get_war_cost(ch_emp, vict_emp))) {
		msg_to_char(ch, "The empire requires %d coin%s in the vault in order to finance the war with %s!\r\n", war_cost, PLURAL(war_cost), EMPIRE_NAME(vict_emp));
	}
	else if (ch_pol && POL_OFFERED(ch_pol, diplo_option[type].add_bits)) {
		msg_to_char(ch, "Your empire has already made that offer.\r\n");
	}
	
	// ready to go
	else {
		if (!ch_pol) {
			ch_pol = create_relation(ch_emp, vict_emp);
		}
		if (!(vict_pol = find_relation(vict_emp, ch_emp))) {
			vict_pol = create_relation(vict_emp, ch_emp);
		}
		
		if (war_cost > 0) {
			EMPIRE_COINS(ch_emp) -= war_cost;
		}
		
		*ch_log = '\0';	// leave trailing punctuation off of ch_log (for war cost)
		*vict_log = '\0';
		
		if (IS_SET(diplo_option[type].flags, DIPF_UNILATERAL)) {
			// demand
			ch_pol->start_time = vict_pol->start_time = time(0);
			REMOVE_BIT(ch_pol->type, diplo_option[type].remove_bits);
			REMOVE_BIT(ch_pol->offer, diplo_option[type].add_bits | diplo_option[type].remove_bits);
			REMOVE_BIT(vict_pol->type, diplo_option[type].remove_bits);
			REMOVE_BIT(vict_pol->offer, diplo_option[type].add_bits | diplo_option[type].remove_bits);
			
			SET_BIT(ch_pol->type, diplo_option[type].add_bits);
			SET_BIT(vict_pol->type, diplo_option[type].add_bits);
			
			snprintf(ch_log, sizeof(ch_log), "%s has been declared with %s", fname(diplo_option[type].keywords), EMPIRE_NAME(vict_emp));
			snprintf(vict_log, sizeof(vict_log), "%s has declared %s!", EMPIRE_NAME(ch_emp), fname(diplo_option[type].keywords));
			syslog(SYS_EMPIRE, 0, TRUE, "DIPL: %s (%s) has declared %s with %s", EMPIRE_NAME(ch_emp), GET_NAME(ch), fname(diplo_option[type].keywords), EMPIRE_NAME(vict_emp));
			msg_to_char(ch, "You have declared %s with %s!\r\n", fname(diplo_option[type].keywords), EMPIRE_NAME(vict_emp));
		}
		else if (POL_OFFERED(vict_pol, diplo_option[type].add_bits)) {
			// accept
			ch_pol->start_time = vict_pol->start_time = time(0);
			REMOVE_BIT(ch_pol->type, diplo_option[type].remove_bits);
			REMOVE_BIT(ch_pol->offer, diplo_option[type].add_bits | diplo_option[type].remove_bits);
			REMOVE_BIT(vict_pol->type, diplo_option[type].remove_bits);
			REMOVE_BIT(vict_pol->offer, diplo_option[type].add_bits | diplo_option[type].remove_bits);
			
			SET_BIT(ch_pol->type, diplo_option[type].add_bits);
			SET_BIT(vict_pol->type, diplo_option[type].add_bits);
			
			snprintf(ch_log, sizeof(ch_log), "%s has been accepted with %s", fname(diplo_option[type].keywords), EMPIRE_NAME(vict_emp));
			snprintf(vict_log, sizeof(vict_log), "%s has accepted %s!", EMPIRE_NAME(ch_emp), fname(diplo_option[type].keywords));
			syslog(SYS_EMPIRE, 0, TRUE, "DIPL: %s (%s) has accepted %s with %s", EMPIRE_NAME(ch_emp), GET_NAME(ch), fname(diplo_option[type].keywords), EMPIRE_NAME(vict_emp));
			msg_to_char(ch, "You have accepted %s with %s!\r\n", fname(diplo_option[type].keywords), EMPIRE_NAME(vict_emp));
		}
		else {
			// offer
			// don't set start time
			REMOVE_BIT(ch_pol->offer, diplo_option[type].remove_bits);
			REMOVE_BIT(vict_pol->offer, diplo_option[type].remove_bits);
			
			SET_BIT(ch_pol->offer, diplo_option[type].add_bits);
			
			snprintf(ch_log, sizeof(ch_log), "The empire has offered %s to %s", fname(diplo_option[type].keywords), EMPIRE_NAME(vict_emp));
			snprintf(vict_log, sizeof(vict_log), "%s offers %s to the empire", EMPIRE_NAME(ch_emp), fname(diplo_option[type].keywords));
			msg_to_char(ch, "You have offered %s to %s!\r\n", fname(diplo_option[type].keywords), EMPIRE_NAME(vict_emp));	
		}
		
		// logs
		if (*ch_log) {
			if (war_cost > 0) {
				log_to_empire(ch_emp, ELOG_DIPLOMACY, "%s for %d coin%s!", CAP(ch_log), war_cost, PLURAL(war_cost));
			}
			else {
				log_to_empire(ch_emp, ELOG_DIPLOMACY, "%s!", CAP(ch_log));
			}
		}
		if (*vict_log) {
			log_to_empire(vict_emp, ELOG_DIPLOMACY, "%s", CAP(vict_log));
		}
		
		EMPIRE_NEEDS_SAVE(ch_emp) = TRUE;
		EMPIRE_NEEDS_SAVE(vict_emp) = TRUE;
	}
}


ACMD(do_efind) {
	extern char *get_vehicle_short_desc(vehicle_data *veh, char_data *to);
	
	char buf[MAX_STRING_LENGTH*2];
	obj_data *obj;
	empire_data *emp;
	int check_x, check_y, total;
	bool all = FALSE;
	room_data *last_rm, *iter, *next_iter;
	struct efind_group *eg, *next_eg, *list = NULL;
	vehicle_data *veh;
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
		HASH_ITER(hh, world_table, iter, next_iter) {
			if (ROOM_OWNER(iter) == emp) {			
				for (obj = ROOM_CONTENTS(iter); obj; obj = obj->next_content) {
					if ((all && CAN_WEAR(obj, ITEM_WEAR_TAKE)) || (!all && isname(arg, obj->name))) {
						add_obj_to_efind(&list, obj, NULL, iter);
						++total;
					}
				}
			}
		}
		
		// next, vehicles
		LL_FOREACH(vehicle_list, veh) {
			if (!IN_ROOM(veh)) {
				continue;
			}
			if (VEH_OWNER(veh) != emp && (VEH_OWNER(veh) != NULL || ROOM_OWNER(IN_ROOM(veh)) != emp)) {
				continue;
			}
			if (!all && !isname(arg, VEH_KEYWORDS(veh))) {
				continue;
			}
			
			add_obj_to_efind(&list, NULL, veh, IN_ROOM(veh));
			++total;
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
					if (has_ability(ch, ABIL_NAVIGATION)) {
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
				
				if (eg->obj) {
					size += snprintf(buf + size, sizeof(buf) - size, "%s", get_obj_desc(eg->obj, ch, OBJ_DESC_SHORT));
				}
				else if (eg->veh) {
					size += snprintf(buf + size, sizeof(buf) - size, "%s", get_vehicle_short_desc(eg->veh, ch));
				}
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
	char output[MAX_STRING_LENGTH*2], line[MAX_STRING_LENGTH];
	empire_data *e, *emp, *next_emp;
	char_data *vict = NULL;
	int min = 1, count;
	bool more = FALSE, all = FALSE, file = FALSE;
	char title[80];
	size_t lsize, size;

	skip_spaces(&argument);

	if (!ch->desc) {
		return;
	}

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

	size = snprintf(output, sizeof(output), "%-35.35s  Sc  Mm  Grt  Territory\r\n", title);

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
		if (!all && EMPIRE_IS_TIMED_OUT(emp)) {
			continue;
		}
		
		lsize = snprintf(line, sizeof(line), "%3d. %s%-30.30s&0  %2d  %2d  %3d  %d\r\n", count, EMPIRE_BANNER(emp), EMPIRE_NAME(emp), get_total_score(emp), EMPIRE_MEMBERS(emp), EMPIRE_GREATNESS(emp), EMPIRE_CITY_TERRITORY(emp) + EMPIRE_OUTSIDE_TERRITORY(emp));
		
		// append if room
		if (size + lsize < sizeof(output)) {
			size += lsize;
			strcat(output, line);
		}
	}
	
	lsize = snprintf(line, sizeof(line), "List options: -m for more, -a for all, -## for minimum members\r\n");
	if (size + lsize < sizeof(output)) {
		size += lsize;
		strcat(output, line);
	}
	
	page_string(ch->desc, output, TRUE);
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
		if ( subcmd == SCMD_EINVENTORY ) {
			show_empire_inventory_to_char(ch, emp, arg2);
		} else {
			show_empire_identify_to_char(ch, emp, arg2);
		}
	}
	else {
		send_to_char(error, ch);
	}
}


ACMD(do_enroll) {
	struct empire_island *from_isle, *next_isle, *isle;
	struct empire_territory_data *ter, *next_ter;
	struct empire_npc_data *npc;
	struct empire_storage_data *store, *store2;
	struct empire_city_data *city, *next_city, *temp;
	player_index_data *index, *next_index;
	struct empire_unique_storage *eus;
	struct vehicle_attached_mob *vam;
	vehicle_data *veh, *next_veh;
	empire_data *e, *old;
	room_data *room, *next_room;
	int old_store, iter;
	char_data *targ = NULL, *victim, *mob;
	bool all_zero, file = FALSE, sub_file = FALSE;
	obj_data *obj;

	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
		return;
	}
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
	else if (!IS_APPROVED(targ) && config_get_bool("join_empire_approval")) {
		act("$E needs to be approved to play before $E can join your empire.", FALSE, ch, NULL, targ, TO_CHAR | TO_SLEEP);
	}
	else {
		log_to_empire(e, ELOG_MEMBERS, "%s has been enrolled in the empire", PERS(targ, targ, 1));
		msg_to_char(targ, "You have been enrolled in %s.\r\n", EMPIRE_NAME(e));
		send_config_msg(ch, "ok_string");
		
		GET_LOYALTY(targ) = e;
		GET_RANK(targ) = 1;
		GET_PLEDGE(targ) = NOTHING;
		
		remove_lore(targ, LORE_PROMOTED);
		add_lore(targ, LORE_JOIN_EMPIRE, "Honorably accepted into %s%s&0", EMPIRE_BANNER(e), EMPIRE_NAME(e));
		
		SAVE_CHAR(targ);
		
		// TODO split this out into a "merge empires" func

		// move data over
		if (old && EMPIRE_LEADER(old) == GET_IDNUM(targ)) {
			// attempt to estimate the new member count so cities and territory transfer correctly
			// note: may over-estimate if some players already had alts in both empires
			EMPIRE_MEMBERS(e) += EMPIRE_MEMBERS(old);
			
			// move members
			HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
				// find only members of the old empire (other than targ)
				if (index->idnum == GET_IDNUM(targ) || index->loyalty != old) {
					continue;
				}
				
				if ((victim = is_playing(index->idnum)) || (victim = is_at_menu(index->idnum))) {
					if (IN_ROOM(victim)) {
						msg_to_char(victim, "Your empire has merged with %s.\r\n", EMPIRE_NAME(e));
					}
					remove_lore(victim, LORE_PROMOTED);
					add_lore(victim, LORE_JOIN_EMPIRE, "Empire merged into %s%s&0", EMPIRE_BANNER(e), EMPIRE_NAME(e));
					GET_LOYALTY(victim) = e;
					GET_RANK(victim) = 1;
					update_player_index(index, victim);
					SAVE_CHAR(victim);
				}
				else if ((victim = find_or_load_player(index->name, &sub_file))) {
					remove_lore(victim, LORE_PROMOTED);
					add_lore(victim, LORE_JOIN_EMPIRE, "Empire merged into %s%s&0", EMPIRE_BANNER(e), EMPIRE_NAME(e));
					GET_LOYALTY(victim) = e;
					GET_RANK(victim) = 1;
					update_player_index(index, victim);
					if (sub_file && victim != targ) {
						store_loaded_char(victim);
					}
					else {
						SAVE_CHAR(victim);
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
			
			// workforce: attempt a smart-copy
			HASH_ITER(hh, EMPIRE_ISLANDS(old), from_isle, next_isle) {
				isle = get_empire_island(e, from_isle->island);
				
				all_zero = TRUE;
				for (iter = 0; iter < NUM_CHORES && all_zero; ++iter) {
					if (isle->workforce_limit[iter] != 0) {
						all_zero = FALSE;
					}
				}
				
				if (all_zero) {	// safe to copy (no previous settings)
					for (iter = 0; iter < NUM_CHORES; ++iter) {
						isle->workforce_limit[iter] = from_isle->workforce_limit[iter];
					}
				}
			}
			
			// convert shipping (before doing vehicles)
			convert_empire_shipping(old, e);
			
			// vehicles
			LL_FOREACH_SAFE2(vehicle_list, veh, next_veh, next) {
				if (VEH_OWNER(veh) == old) {
					VEH_OWNER(veh) = e;
				}
				LL_FOREACH(VEH_ANIMALS(veh), vam) {
					if (vam->empire == EMPIRE_VNUM(old)) {
						vam->empire = EMPIRE_VNUM(e);
					}
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
			HASH_ITER(hh, world_table, room, next_room) {
				if (ROOM_OWNER(room) == old) {
					abandon_room(room);
					claim_room(room, e);
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
	void add_to_channel_history(char_data *ch, int type, char *message);
	extern bool is_ignoring(char_data *ch, char_data *victim);
	
	descriptor_data *d;
	char_data *tch;
	int level = 0, i;
	empire_data *e;
	bool emote = FALSE, extra_color = FALSE;
	char buf[MAX_STRING_LENGTH], lstring[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH], output[MAX_STRING_LENGTH], color[8];

	if (IS_NPC(ch))
		return;

	if (!(e = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You don't belong to any empire.\r\n");
		return;
		}

	if (ACCOUNT_FLAGGED(ch, ACCT_MUTED)) {
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
		for (i = 0; i < strlen(arg); i++) {
			if (arg[i] == '_') {
				arg[i] = ' ';
			}
		}
		i = find_rank_by_name(e, arg+1);
		if (i != NOTHING) {
			level = i;
		}
	}
	if (*argument == '*') {
		argument++;
		emote = TRUE;
	}
	skip_spaces(&argument);

	level++;	// 1-based, not 0-based

	if (level > 1) {
		sprintf(lstring, " <%s%%s>", EMPIRE_RANK(e, level-1));
		extra_color = TRUE;
	}
	else {
		*lstring = '\0';
	}

	/* Since we cut up the string, we have to check again */
	if (!*argument) {
		msg_to_char(ch, "What would you like to tell your empire?\r\n");
		return;
	}

	// NOTE: both modes will leave in 2 '%s' for color codes
	if (emote) {
		sprintf(buf, "%%s[%sEMPIRE%%s%s] $o %s\t0", EMPIRE_BANNER(e), lstring, double_percents(argument));
	}
	else {
		sprintf(buf, "%%s[%sEMPIRE%%s $o%s]: %s\t0", EMPIRE_BANNER(e), lstring, double_percents(argument));
	}

	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		// for channel history
		if (ch->desc) {
			clear_last_act_message(ch->desc);
		}

		sprintf(color, "%s\t%c", EXPLICIT_BANNER_TERMINATOR(e), GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_ESAY) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_ESAY) : '0');
		if (extra_color) {
			sprintf(output, buf, color, color, color);
		}
		else {
			sprintf(output, buf, color, color);
		}
		
		act(output, FALSE, ch, 0, 0, TO_CHAR | TO_SLEEP | TO_NODARK);
		
		// channel history
		if (ch->desc && ch->desc->last_act_message) {
			// the message was sent via act(), we can retrieve it from the desc
			sprintf(lbuf, "%s", ch->desc->last_act_message);
			add_to_channel_history(ch, CHANNEL_HISTORY_EMPIRE, lbuf);
		}
	}

	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) != CON_PLAYING || !(tch = d->character) || IS_NPC(tch) || is_ignoring(tch, ch) || GET_LOYALTY(tch) != e || GET_RANK(tch) < level || tch == ch)
			continue;
		else {
			clear_last_act_message(d);
			
			sprintf(color, "%s\t%c", EXPLICIT_BANNER_TERMINATOR(e), GET_CUSTOM_COLOR(tch, CUSTOM_COLOR_ESAY) ? GET_CUSTOM_COLOR(tch, CUSTOM_COLOR_ESAY) : '0');
			if (extra_color) {
				sprintf(output, buf, color, color, color);
			}
			else {
				sprintf(output, buf, color, color);
			}
			act(output, FALSE, ch, 0, tch, TO_VICT | TO_SLEEP | TO_NODARK);
			
			// channel history
			if (d->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				sprintf(lbuf, "%s", d->last_act_message);
				add_to_channel_history(tch, CHANNEL_HISTORY_EMPIRE, lbuf);
			}	
		}
	}
}


ACMD(do_estats) {
	empire_data *emp = GET_LOYALTY(ch);
	
	skip_spaces(&argument);
	if (*argument && !(emp = get_empire_by_name(argument))) {
		msg_to_char(ch, "Unknown empire.\r\n");
		return;
	}
	if (!*argument && !emp) {
		msg_to_char(ch, "You are not in an empire.\r\n");
		return;
	}
	
	// show the empire:
	
	// name
	msg_to_char(ch, "%s%s&0", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
	if (strcmp(EMPIRE_ADJECTIVE(emp), EMPIRE_NAME(emp))) {
		msg_to_char(ch, " (%s&0)", EMPIRE_ADJECTIVE(emp));
	}
	msg_to_char(ch, "\r\n");
	
	// stats
	msg_to_char(ch, "Population: %d player%s, %d citizen%s, %d military\r\n", EMPIRE_MEMBERS(emp), PLURAL(EMPIRE_MEMBERS(emp)), EMPIRE_POPULATION(emp), PLURAL(EMPIRE_POPULATION(emp)), EMPIRE_MILITARY(emp));
	msg_to_char(ch, "Territory: %d in cities, %d/%d outside (%d/%d total)\r\n", EMPIRE_CITY_TERRITORY(emp), EMPIRE_OUTSIDE_TERRITORY(emp), land_can_claim(emp, TRUE), EMPIRE_CITY_TERRITORY(emp) + EMPIRE_OUTSIDE_TERRITORY(emp), land_can_claim(emp, FALSE));
	msg_to_char(ch, "Wealth: %.1f coin%s (at %d%%), %d treasure (%d total)\r\n", EMPIRE_COINS(emp), PLURAL(EMPIRE_COINS(emp)), (int)(COIN_VALUE * 100), EMPIRE_WEALTH(emp), (int) GET_TOTAL_WEALTH(emp));
	msg_to_char(ch, "Fame: %d, Greatness: %d\r\n", EMPIRE_FAME(emp), EMPIRE_GREATNESS(emp));
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
	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!e)
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
		add_cooldown(targ, COOLDOWN_LEFT_EMPIRE, 2 * SECS_PER_REAL_HOUR);
		clear_private_owner(GET_IDNUM(targ));

		log_to_empire(e, ELOG_MEMBERS, "%s has been expelled from the empire", PERS(targ, targ, 1));
		send_config_msg(ch, "ok_string");
		msg_to_char(targ, "You have been expelled from the empire.\r\n");
		
		remove_lore(targ, LORE_PROMOTED);
		remove_lore(targ, LORE_JOIN_EMPIRE);
		add_lore(targ, LORE_KICKED_EMPIRE, "Dishonorably discharged from %s%s&0", EMPIRE_BANNER(e), EMPIRE_NAME(e));

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
	extern struct resource_data *combine_resources(struct resource_data *combine_a, struct resource_data *combine_b);
	
	char arg[MAX_INPUT_LENGTH], partial[MAX_STRING_LENGTH], temp[MAX_INPUT_LENGTH], *ptr;
	struct find_territory_node *node_list = NULL, *node, *next_node;
	struct resource_data *old_res, *total_list = NULL;
	struct island_info *find_island = NULL;
	empire_data *emp = GET_LOYALTY(ch);
	struct empire_territory_data *ter;
	room_data *find_room = NULL;
	int total = 0;
	
	if (!ch->desc || IS_NPC(ch)) {
		return;
	}
	
	skip_spaces(&argument);
	
	// imms can target this
	if (*argument && (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES))) {
		strcpy(temp, argument);
		ptr = any_one_word(temp, arg);
		
		if ((emp = get_empire_by_name(arg))) {
			argument = ptr;
			skip_spaces(&argument);
		}
		else {
			// wasn't an empire
			emp = GET_LOYALTY(ch);
		}
	}
	
	if (!emp) {
		msg_to_char(ch, "You can't use findmaintenance if you are not in an empire.\r\n");
		return;
	}
	
	// anything else in the argument?
	if (*argument == '"' || *argument == '(') {
		any_one_word(argument, arg);
	}
	else {
		strcpy(arg, argument);
	}
	
	if (*arg) {
		if (!(find_island = get_island_by_name(ch, arg)) && !(find_room = find_target_room(NULL, arg))) {
			msg_to_char(ch, "Unknown location: %s.\r\n", arg);
			return;
		}
		else if (find_room && ROOM_OWNER(find_room) != emp) {
			msg_to_char(ch, "The empire does not own that location.\r\n");
			return;
		}
		else if (find_room && !BUILDING_DAMAGE(find_room) && (!IS_COMPLETE(find_room) || !BUILDING_RESOURCES(find_room))) {
			msg_to_char(ch, "That location needs no maintenance.\r\n");
			return;
		}
	}
	
	// player is only checking one room (pre-validated)
	if (find_room) {
		show_resource_list(BUILDING_RESOURCES(find_room), partial);
		msg_to_char(ch, "Maintenance needed for %s (%d, %d): %s\r\n", get_room_name(find_room, FALSE), X_COORD(find_room), Y_COORD(find_room), partial);
		return;
	}
	
	// check all the territory
	LL_FOREACH(EMPIRE_TERRITORY_LIST(emp), ter) {
		// validate the tile
		if (GET_ROOM_VNUM(ter->room) >= MAP_SIZE) {
			continue;
		}
		if ((!IS_COMPLETE(ter->room) || !BUILDING_RESOURCES(ter->room)) && !HAS_MINOR_DISREPAIR(ter->room)) {
			continue;
		}
		if (find_island && GET_ISLAND_ID(ter->room) != find_island->id) {
			continue;
		}
		
		// validated
		++total;
		
		// ok, what are we looking for (resource list or node list?)
		if (find_island) {
			old_res = total_list;
			total_list = combine_resources(total_list, BUILDING_RESOURCES(ter->room));
			free_resource_list(old_res);
		}
		else {
			CREATE(node, struct find_territory_node, 1);
			node->loc = ter->room;
			node->count = 1;
			node->next = node_list;
			node_list = node;
		}
	}
	
	// okay, now what to display
	if (find_island) {
		show_resource_list(total_list, partial);
		msg_to_char(ch, "Maintenance needed for %d building%s on %s: %s\r\n", total, PLURAL(total), find_island->name, total_list ? partial : "none");
		free_resource_list(total_list);
	}
	else if (node_list) {
		node_list = reduce_territory_node_list(node_list);
		sprintf(buf, "%d location%s needing maintenance:\r\n", total, PLURAL(total));
		
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
		msg_to_char(ch, "No buildings were found that needed maintenance.\r\n");
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
	
	HASH_ITER(hh, world_table, iter, next_iter) {
		if (ROOM_PRIVATE_OWNER(iter) == GET_IDNUM(ch)) {
			return iter;
		}
	}
	
	return NULL;
}


ACMD(do_home) {
	void delete_territory_npc(struct empire_territory_data *ter, struct empire_npc_data *npc);
	
	struct empire_territory_data *ter;
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
		else if (has_ability(ch, ABIL_NAVIGATION)) {
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
		else if (!has_permission(ch, PRIV_HOMES)) {	// after the has-owner check because otherwise the error is misleading
			msg_to_char(ch, "You aren't high enough rank to set a home.\r\n");
		}
		else if (!IS_COMPLETE(real) || !GET_BUILDING(real) || GET_BLD_CITIZENS(GET_BUILDING(real)) <= 0) {
			msg_to_char(ch, "You can't make this your home.\r\n");
		}
		else if (ROOM_AFF_FLAGGED(real, ROOM_AFF_HAS_INSTANCE)) {
			msg_to_char(ch, "You can't make this your home right now.\r\n");
		}
		else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
			msg_to_char(ch, "You can't make this your home because it's not in a city.\r\n");
		}
		else {
			// if someone is overriding it
			if (ROOM_PRIVATE_OWNER(real) != NOBODY) {
				clear_private_owner(ROOM_PRIVATE_OWNER(real));
			}
			
			// allow the player to set home here
			clear_private_owner(GET_IDNUM(ch));
			
			// clear out npcs
			// TODO should this be done for interior rooms, too?
			if ((ter = find_territory_entry(emp, real))) {
				while (ter->npcs) {
					delete_territory_npc(ter, ter->npcs);
				}
			}
			
			COMPLEX_DATA(real)->private_owner = GET_IDNUM(ch);

			// interior only
			for (iter = interior_room_list; iter; iter = next_iter) {
				next_iter = iter->next_interior;
				
				// TODO consider a trigger like RoomUpdate that passes a var like %update% == homeset
				if (HOME_ROOM(iter) == real && BUILDING_VNUM(iter) == RTYPE_BEDROOM) {
					obj_to_room((obj = read_object(o_HOME_CHEST, TRUE)), iter);
					load_otrigger(obj);
				}
			}
			
			log_to_empire(emp, ELOG_TERRITORY, "%s has made (%d, %d) %s home", PERS(ch, ch, 1), X_COORD(real), Y_COORD(real), REAL_HSHR(ch));
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
	struct empire_island *eisle, *next_eisle;
	struct empire_unique_storage *eus;
	struct empire_storage_data *store;
	struct island_info *isle;
	empire_data *emp;
	room_data *room;
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
	if (!has_ability(ch, ABIL_NAVIGATION)) {
		msg_to_char(ch, "You need to purchase the Navigation ability to do that.\r\n");
		return;
	}
	if (!emp) {
		msg_to_char(ch, "You must be in an empire to do that.\r\n");
		return;
	}
	
	// mark your territory
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), eisle, next_eisle) {
		if (eisle->city_terr + eisle->outside_terr > 0) {
			do_islands_has_territory(&list, eisle->island, eisle->city_terr + eisle->outside_terr);
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
		if (item->id == NO_ISLAND) {
			continue;	// skip
		}
		
		isle = get_island(item->id, TRUE);
		room = real_room(isle->center);
		lsize = snprintf(line, sizeof(line), " %s (%d, %d) - ", get_island_name_for(isle->id, ch), X_COORD(room), Y_COORD(room));
		
		if (item->territory > 0) {
			lsize += snprintf(line + lsize, sizeof(line) - lsize, "%d territory%s", item->territory, item->einv_size > 0 ? ", " : "");
		}
		if (item->einv_size > 0) {
			lsize += snprintf(line + lsize, sizeof(line) - lsize, "%d einventory", item->einv_size);
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
	
	if (!HAS_FUNCTION(IN_ROOM(ch), FNC_TAVERN) || !IS_COMPLETE(IN_ROOM(ch))) {
		show_tavern_status(ch);
		msg_to_char(ch, "You can only change what's being brewed while actually in the tavern.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it.\r\n");
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
		else if (has_ability(ch, ABIL_NAVIGATION)) {
			msg_to_char(ch, "Your tomb is at: %s (%d, %d)\r\n", get_room_name(tomb, FALSE), X_COORD(tomb), Y_COORD(tomb));
		}
		else {
			msg_to_char(ch, "Your tomb is at: %s\r\n", get_room_name(tomb, FALSE));
		}
		
		// additional info
		if (tomb && !can_use_room(ch, tomb, GUESTS_ALLOWED)) {
			msg_to_char(ch, "You no longer have access to that tomb because it's owned by %s.\r\n", ROOM_OWNER(tomb) ? EMPIRE_NAME(ROOM_OWNER(tomb)) : "someone else");
		}
		if (room_has_function_and_city_ok(IN_ROOM(ch), FNC_TOMB)) {
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
		else if (GET_ROOM_VEHICLE(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't set your tomb in a vehicle.\r\n");
		}
		else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
			msg_to_char(ch, "You need to own a building to make it your tomb.\r\n");
		}
		else if (!HAS_FUNCTION(IN_ROOM(ch), FNC_TOMB) || !IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't make this place your tomb!\r\n");
		}
		else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE) || ROOM_AFF_FLAGGED(real, ROOM_AFF_HAS_INSTANCE)) {
			msg_to_char(ch, "You can't make this your tomb right now.\r\n");
		}
		else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
			msg_to_char(ch, "You can't make this your tomb because it's not in a city.\r\n");
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
	else if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
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
	else if (is_abbrev(arg, "analyze") || is_abbrev(arg, "analysis")) {
		do_import_analysis(ch, emp, argument, subcmd);
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
	else if (!IS_APPROVED(ch) && config_get_bool("join_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
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
		log_to_empire(e, ELOG_MEMBERS, "%s has offered %s pledge to this empire", PERS(ch, ch, 1), REAL_HSHR(ch));
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
	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
		return;
	}
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
		remove_recent_lore(victim, LORE_PROMOTED);	// only save most recent
		add_lore(victim, LORE_PROMOTED, "Promoted to %s&0", EMPIRE_RANK(e, to_rank-1));

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
	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (HOME_ROOM(IN_ROOM(ch)) != IN_ROOM(ch))
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
		affect_from_char(ch, ATYPE_RADIANCE, FALSE);
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
	else if (ch == vict || GET_ACCOUNT(ch) == GET_ACCOUNT(vict)) {
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
	extern bool member_is_timed_out_ch(char_data *ch);
	extern const char *class_role[];
	extern const char *class_role_color[];

	char buf[MAX_STRING_LENGTH * 2], buf1[MAX_STRING_LENGTH * 2], arg[MAX_INPUT_LENGTH];
	player_index_data *index, *next_index;
	empire_data *e = GET_LOYALTY(ch);
	bool timed_out, is_file = FALSE;
	int days, hours, size;
	char_data *member;
	bool all = FALSE;
	
	// imm usage: roster ["empire"] [all]
	// mortal usage: roster [all]
	
	while (*argument) {
		argument = any_one_word(argument, arg);
		
		if (!str_cmp(arg, "all") || is_abbrev(arg, "-all")) {
			all = TRUE;
		}
		else if (*arg && (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES))) {
			if (!(e = get_empire_by_name(arg))) {
				msg_to_char(ch, "Unknown empire.\r\n");
				return;
			}
		}
		else if (*arg) {
			msg_to_char(ch, "Usage: roster %s[all]\r\n", (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES)) ? "[\"empire\"] " : "");
			return;
		}
	}
	
	if (!e) {
		msg_to_char(ch, "You must be a member of an empire to do that.\r\n");
		return;
	}
	
	*buf = '\0';
	size = 0;
	
	HASH_ITER(name_hh, player_table_by_name, index, next_index) {
		if (index->loyalty != e) {
			continue;
		}
		
		// load member
		member = find_or_load_player(index->name, &is_file);
		if (!member) {
			continue;
		}
		
		timed_out = member_is_timed_out_ch(member);
		if (timed_out && !all) {
			continue;
		}
		
		// display:
		if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
			size += snprintf(buf + size, sizeof(buf) - size, "[%d %s %s] <%s&0> %s%s&0", !is_file ? GET_COMPUTED_LEVEL(member) : GET_LAST_KNOWN_LEVEL(member), SHOW_CLASS_NAME(member), class_role[GET_CLASS_ROLE(member)], EMPIRE_RANK(e, GET_RANK(member) - 1), (timed_out ? "&r" : ""), PERS(member, member, TRUE));
		}
		else {	// not screenreader
			size += snprintf(buf + size, sizeof(buf) - size, "[%d %s%s\t0] <%s&0> %s%s&0", !is_file ? GET_COMPUTED_LEVEL(member) : GET_LAST_KNOWN_LEVEL(member), class_role_color[GET_CLASS_ROLE(member)], SHOW_CLASS_NAME(member), EMPIRE_RANK(e, GET_RANK(member) - 1), (timed_out ? "&r" : ""), PERS(member, member, TRUE));
		}
						
		// online/not
		if (!is_file) {
			size += snprintf(buf + size, sizeof(buf) - size, "  - &conline&0%s", IS_AFK(member) ? " - &rafk&0" : "");
		}
		else if ((time(0) - member->prev_logon) < SECS_PER_REAL_DAY) {
			hours = (time(0) - member->prev_logon) / SECS_PER_REAL_HOUR;
			size += snprintf(buf + size, sizeof(buf) - size, "  - %d hour%s ago%s", hours, PLURAL(hours), (timed_out ? ", &rtimed-out&0" : ""));
		}
		else {	// more than a day
			days = (time(0) - member->prev_logon) / SECS_PER_REAL_DAY;
			size += snprintf(buf + size, sizeof(buf) - size, "  - %d day%s ago%s", days, PLURAL(days), (timed_out ? ", &rtimed-out&0" : ""));
		}
		
		size += snprintf(buf + size, sizeof(buf) - size, "\r\n");
		
		if (member && is_file) {
			free_char(member);
		}
	}

	snprintf(buf1, sizeof(buf1), "Roster of %s%s&0:\r\n%s", EMPIRE_BANNER(e), EMPIRE_NAME(e), buf);
	page_string(ch->desc, buf1, TRUE);
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
	if (!ch->desc || IS_NPC(ch) || !has_ability(ch, ABIL_NAVIGATION)) {
		msg_to_char(ch, "You need the Navigation ability to list the coordinates of your territory.\r\n");
		return;
	}
	
	// only if no argument (optional)
	outside_only = *argument ? FALSE : TRUE;
	
	// ready?
	HASH_ITER(hh, world_table, iter, next_iter) {
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
				else if (ROOM_SECT_FLAGGED(iter, SECTF_HAS_CROP_DATA) && (crop = ROOM_CROP(iter)) && multi_isname(argument, GET_CROP_NAME(crop))) {
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

	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!(e = GET_LOYALTY(ch)))
		msg_to_char(ch, "You're not in an empire.\r\n");
	else if (GET_RANK(ch) < EMPIRE_NUM_RANKS(e))
		msg_to_char(ch, "You're of insufficient rank to remove all public status for the empire.\r\n");
	else {
		HASH_ITER(hh, world_table, iter, next_iter) {
			if (ROOM_AFF_FLAGGED(iter, ROOM_AFF_PUBLIC) && ROOM_OWNER(iter) == e) {
				REMOVE_BIT(ROOM_AFF_FLAGS(iter), ROOM_AFF_PUBLIC);
				REMOVE_BIT(ROOM_BASE_FLAGS(iter), ROOM_AFF_PUBLIC);
			}
		}
		msg_to_char(ch, "All public status for this empire's buildings has been renounced.\r\n");
	}
}


ACMD(do_workforce) {
	char arg[MAX_INPUT_LENGTH], lim_arg[MAX_INPUT_LENGTH], name[MAX_STRING_LENGTH], local_arg[MAX_INPUT_LENGTH], island_arg[MAX_INPUT_LENGTH];
	struct island_info *island = NULL;
	bool all = FALSE, here = FALSE;
	int iter, type, limit = 0;
	empire_data *emp;
	
	argument = any_one_arg(argument, arg);

	if (!(emp = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You must be in an empire to set up the workforce.\r\n");
	}
	else if (IS_NPC(ch) || !ch->desc) {
		msg_to_char(ch, "You can't set up the workforce right now.\r\n");
	}
	else if (!EMPIRE_HAS_TECH(emp, TECH_WORKFORCE)) {
		msg_to_char(ch, "Your empire has no workforce.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Usage: workforce [chore] [on | off | <limit>] [island name | all]\r\n");
		show_workforce_setup_to_char(emp, ch);
	}
	else if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (is_abbrev(arg, "where")) {
		argument = any_one_arg(argument, local_arg);
		if ( is_abbrev(local_arg, "here") ){
			here = true;
		}
		show_workforce_where(emp, ch, here);
	}
	// everything below requires privileges
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
	else if (is_abbrev(arg, "copy")) {
		// process remaining args (island name may have quotes)
		argument = any_one_word(argument, island_arg);
		
		if (!*island_arg) {
			msg_to_char(ch, "Usage: workforce copy <from island>\r\n");
			return;
		}
		if (!(island = get_island_by_name(ch, island_arg)) && !(island = get_island_by_coords(island_arg))) {
			msg_to_char(ch, "Unknown island \"%s\".\r\n", island_arg);
			return;
		}
		
		copy_workforce_limits_into_current_island(ch,island);
	}
	else {	// <chore>: show/change type
		// find chore
		for (iter = 0, type = NOTHING; iter < NUM_CHORES && type == NOTHING; ++iter) {
			if (is_abbrev(arg, chore_data[iter].name)) {
				type = iter;
			}
		}
		if (type == NOTHING) {
			msg_to_char(ch, "Invalid workforce option.\r\n");
			return;
		}
		
		// process remaining args (island name may have quotes)
		argument = any_one_arg(argument, lim_arg);
		any_one_word(argument, island_arg);
		
		// limit arg
		if (!*lim_arg) {
			show_detailed_workforce_setup_to_char(emp, ch, type);
			return;
		}
		else if (!str_cmp(lim_arg, "on")) {
			limit = WORKFORCE_UNLIMITED;
		}
		else if (!str_cmp(lim_arg, "off")) {
			limit = 0;
		}
		else if (isdigit(*lim_arg)) {
			limit = atoi(lim_arg);
			if (limit < 0) {
				// caused by absurdly large limits rolling over
				msg_to_char(ch, "Invalid limit.\r\n");
				return;
			}
		}
		else {
			msg_to_char(ch, "Invalid setting (must be on, off, or a limit number).\r\n");
			return;
		}
		
		// island arg
		if (!*island_arg) {
			if (GET_ISLAND_ID(IN_ROOM(ch)) == NO_ISLAND) {
				msg_to_char(ch, "You can't set local workforce options when you're not on any island.\r\n");
				return;
			}
			else {
				here = TRUE;
			}
		}
		else if (!str_cmp(island_arg, "all")) {
			all = TRUE;
		}
		else if (!(island = get_island_by_name(ch, island_arg)) && !(island = get_island_by_coords(island_arg))) {
			msg_to_char(ch, "Unknown island \"%s\".\r\n", island_arg);
			return;
		}
		
		// ok, set workforce
		*name = '\0';
		if (all) {
			set_workforce_limit_all(emp, type, limit);
			strcpy(name, "all islands");
		}
		else if (here) {
			set_workforce_limit(emp, GET_ISLAND_ID(IN_ROOM(ch)), type, limit);
			strcpy(name, "this island");
		}
		else if (island) {
			set_workforce_limit(emp, island->id, type, limit);
			snprintf(name, sizeof(name), "%s", get_island_name_for(island->id, ch));
		}
		else {
			msg_to_char(ch, "No workforce to set for that.\r\n");
			return;
		}
		
		msg_to_char(ch, "You have %s the %s chore for %s.\r\n", (limit == 0) ? "disabled" : ((limit == WORKFORCE_UNLIMITED) ? "enabled" : "set the limit for"), chore_data[type].name, name);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
		
		if (limit == 0) {
			deactivate_workforce(emp, all ? NO_ISLAND : (here ? GET_ISLAND_ID(IN_ROOM(ch)) : island->id), type);
		}
	}
}


ACMD(do_withdraw) {
	empire_data *emp, *coin_emp;
	int coin_amt;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't withdraw anything.\r\n");
	}
	else if (!HAS_FUNCTION(IN_ROOM(ch), FNC_VAULT)) {
		msg_to_char(ch, "You can only withdraw coins in a vault.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You must finish building it first.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it.\r\n");
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
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}
