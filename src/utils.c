/* ************************************************************************
*   File: utils.c                                         EmpireMUD 2.0b5 *
*  Usage: various internal functions of a utility nature                  *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <math.h>
#include <sys/time.h>

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Basic Utils
*   Empire Utils
*   Empire Trade Utils
*   Empire Diplomacy Utils
*   Empire Permissions Utils
*   File Utils
*   Logging Utils
*   Adventure Utils
*   Interpreter Utils
*   Mobile Utils
*   Object Utils
*   Player Utils
*   Resource Utils
*   Sector Utils
*   String Utils
*   Type Utils
*   World Utils
*   Misc Utils
*/

// external vars
extern const char *pool_types[];

// external funcs
void scale_item_to_level(obj_data *obj, int level);
void send_char_pos(char_data *ch, int dam);

// locals
#define WHITESPACE " \t"	// used by some of the string functions
bool emp_can_use_room(empire_data *emp, room_data *room, int mode);
bool empire_can_claim(empire_data *emp);
bool is_trading_with(empire_data *emp, empire_data *partner);
void score_empires();
void unmark_items_for_char(char_data *ch, bool ground);


 //////////////////////////////////////////////////////////////////////////////
//// BASIC UTILS /////////////////////////////////////////////////////////////


/**
* This is usually called as GET_AGE() from utils.h
*
* @param char_data *ch The character.
* @return struct time_info_data* A time object indicating the player's age.
*/
struct time_info_data *age(char_data *ch) {
	static struct time_info_data player_age;

	player_age = *mud_time_passed(time(0), ch->player.time.birth);

	player_age.year += 17;	/* All players start at 17 */
	player_age.year -= config_get_int("starting_year");

	return (&player_age);
}


/**
* @param room_data *room The location
* @return bool TRUE if any players are in the room
*/
bool any_players_in_room(room_data *room) {
	char_data *ch;
	bool found = FALSE;
	
	for (ch = ROOM_PEOPLE(room); !found && ch; ch = ch->next_in_room) {
		if (!IS_NPC(ch)) {
			found = TRUE;
		}
	}
	
	return found;
}


/**
* Applies bonus traits whose effects are one-time-only.
*
* @param char_data *ch The player to apply the trait to.
* @param bitvector_t trait Any BONUS_x bit.
* @param bool add If TRUE, we are adding the trait; FALSE means removing it;
*/
void apply_bonus_trait(char_data *ch, bitvector_t trait, bool add) {	
	int amt = (add ? 1 : -1);
	
	if (IS_SET(trait, BONUS_EXTRA_DAILY_SKILLS)) {
		GET_DAILY_BONUS_EXPERIENCE(ch) = MAX(0, GET_DAILY_BONUS_EXPERIENCE(ch) + (config_get_int("num_bonus_trait_daily_skills") * amt));
	}
	if (IS_SET(trait, BONUS_STRENGTH)) {
		ch->real_attributes[STRENGTH] = MAX(1, MIN(att_max(ch), ch->real_attributes[STRENGTH] + amt));
	}
	if (IS_SET(trait, BONUS_DEXTERITY)) {
		ch->real_attributes[DEXTERITY] = MAX(1, MIN(att_max(ch), ch->real_attributes[DEXTERITY] + amt));
	}
	if (IS_SET(trait, BONUS_CHARISMA)) {
		ch->real_attributes[CHARISMA] = MAX(1, MIN(att_max(ch), ch->real_attributes[CHARISMA] + amt));
	}
	if (IS_SET(trait, BONUS_GREATNESS)) {
		ch->real_attributes[GREATNESS] = MAX(1, MIN(att_max(ch), ch->real_attributes[GREATNESS] + amt));
	}
	if (IS_SET(trait, BONUS_INTELLIGENCE)) {
		ch->real_attributes[INTELLIGENCE] = MAX(1, MIN(att_max(ch), ch->real_attributes[INTELLIGENCE] + amt));
	}
	if (IS_SET(trait, BONUS_WITS)) {
		ch->real_attributes[WITS] = MAX(1, MIN(att_max(ch), ch->real_attributes[WITS] + amt));
	}
	
	if (IN_ROOM(ch)) {
		affect_total(ch);
	}
}


/**
* If no ch is supplied, this uses the max npc value by default.
*
* @param char_data *ch (Optional) The person to check the max for.
* @return int The maximum attribute level.
*/
int att_max(char_data *ch) {	
	int max = 1;

	if (ch && !IS_NPC(ch)) {
		max = config_get_int("max_player_attribute");
	}
	else {
		max = config_get_int("max_npc_attribute");
	}

	return max;
}


/* Check if making CH follow VICTIM will create an illegal */
/* Follow "Loop/circle"                                    */
bool circle_follow(char_data *ch, char_data *victim) {
	char_data *k;

	for (k = victim; k; k = k->master) {
		if (k == ch) {
			return (TRUE);
		}
	}

	return (FALSE);
}


/**
* @param bitvector_t bitset The bit flags to count.
* @return int The number of flags active in the set.
*/
int count_bits(bitvector_t bitset) {
	bitvector_t set;
	int count = 0;
	
	for (set = bitset; set; set >>= 1) {
		if (set & 1) {
			++count;
		}
	}
	
	return count;
}


/* simulates dice roll */
int dice(int number, int size) {
	unsigned long empire_random();

	int sum = 0;

	if (size <= 0 || number <= 0)
		return (0);

	while (number-- > 0)
		sum += ((empire_random() % size) + 1);

	return (sum);
}


/**
* Diminishing returns formula from lostsouls.org -- the one change I made is
* that it won't return a number of greater magnitude than the original 'val',
* as I noticed that a 15 on a scale of 20 returned 16. The intention is to
* diminish the value, not raise it.
*
* 'scale' is the level at which val is unchanged -- at higher levels, it is
* greatly reduced.
*
* @param double val The base value.
* @param double scale The level after which val diminishes.
* @return double The diminished value.
*/
double diminishing_returns(double val, double scale) {
	// nonsense!
	if (scale == 0) {
		return 0;
	}
	
    if (val < 0)
        return -diminishing_returns(-val, scale);
    double mult = val / scale;
    double trinum = (sqrt(8.0 * mult + 1.0) - 1.0) / 2.0;
    double final = trinum * scale;
    
    if (final > val) {
    	return val;
    }
    
    return final;
}


/**
* @param char_data *ch
* @return bool TRUE if ch has played at least 1 day (or is an npc)
*/
bool has_one_day_playtime(char_data *ch) {
	struct time_info_data playing_time;

	if (IS_NPC(ch)) {
		return TRUE;
	}
	else {
		playing_time = *real_time_passed((time(0) - ch->player.time.logon) + ch->player.time.played, 0);

		return playing_time.day >= 1;
	}
}


/**
* @param char_data *ch The player to check.
* @return int The total number of bonus traits a character deserves.
*/
int num_earned_bonus_traits(char_data *ch) {
	struct time_info_data t;
	int count = 1;	// all players deserve 1
	
	if (IS_NPC(ch)) {
		return 0;
	}
	
	// extra point at 2 days playtime
	t = *real_time_passed((time(0) - ch->player.time.logon) + ch->player.time.played, 0);
	if (t.day >= 2) {
		++count;
	}

	return count;
}


/* creates a random number in interval [from;to] */
int number(int from, int to) {
	unsigned long empire_random();

	// shortcut -paul 12/9/2014
	if (from == to) {
		return from;
	}

	/* error checking in case people call number() incorrectly */
	if (from > to) {
		int tmp = from;
		from = to;
		to = tmp;
	}

	return ((empire_random() % (to - from + 1)) + from);
}


/* Calculate the MUD time passed over the last t2-t1 centuries (secs) */
struct time_info_data *mud_time_passed(time_t t2, time_t t1) {
	long secs;
	static struct time_info_data now;

	secs = (long) (t2 - t1);

	now.hours = (secs / SECS_PER_MUD_HOUR) % 24;	/* 0..23 hours */
	secs -= SECS_PER_MUD_HOUR * now.hours;

	now.day = (secs / SECS_PER_MUD_DAY) % 30;		/* 0..29 days  */
	secs -= SECS_PER_MUD_DAY * now.day;

	now.month = (secs / SECS_PER_MUD_MONTH) % 12;	/* 0..11 months */
	secs -= SECS_PER_MUD_MONTH * now.month;

	now.year = config_get_int("starting_year") + (secs / SECS_PER_MUD_YEAR);		/* 0..XX? years */

	return (&now);
}


/**
* PERS is essentially the $n from act() -- how one character is displayed to
* another.
*
* @param char_data *ch The person being displayed.
* @param char_data *vict The person who is seeing them.
* @param bool real If TRUE, uses real name instead of disguise/morph.
*/
const char *PERS(char_data *ch, char_data *vict, bool real) {
	static char output[MAX_INPUT_LENGTH];

	if (!CAN_SEE(vict, ch)) {
		return "someone";
	}

	if (IS_MORPHED(ch) && !real) {
		return get_morph_desc(ch, FALSE);
	}
	
	if (!real && IS_DISGUISED(ch)) {
		return GET_DISGUISED_NAME(ch);
	}

	strcpy(output, GET_NAME(ch));
	if (!IS_NPC(ch) && GET_LASTNAME(ch)) {
		sprintf(output + strlen(output), " %s", GET_LASTNAME(ch));
	}

	return output;
}


/* Calculate the REAL time passed over the last t2-t1 centuries (secs) */
struct time_info_data *real_time_passed(time_t t2, time_t t1) {
	long secs;
	static struct time_info_data now;

	secs = (long) (t2 - t1);

	now.hours = (secs / SECS_PER_REAL_HOUR) % 24;	/* 0..23 hours */
	secs -= SECS_PER_REAL_HOUR * now.hours;

	now.day = (secs / SECS_PER_REAL_DAY);	/* 0..29 days  */
	/* secs -= SECS_PER_REAL_DAY * now.day; - Not used. */

	now.month = -1;
	now.year = -1;

	return (&now);
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE UTILS ////////////////////////////////////////////////////////////

/**
* Checks all empires for delayed-refresh commands.
*/
void run_delayed_refresh(void) {
	void complete_goal(empire_data *emp, struct empire_goal *goal);
	extern int count_empire_crop_variety(empire_data *emp, int max_needed, int only_island);
	void count_quest_tasks(struct req_data *list, int *complete, int *total);
	
	if (check_delayed_refresh) {
		struct empire_goal *goal, *next_goal;
		int complete, total, crop_var;
		empire_data *emp, *next_emp;
		struct req_data *task;
		
		HASH_ITER(hh, empire_table, emp, next_emp) {
			crop_var = -1;
			
			// DELAY_REFRESH_x: effects of various rereshes
			if (IS_SET(EMPIRE_DELAYED_REFRESH(emp), DELAY_REFRESH_CROP_VARIETY)) {
				HASH_ITER(hh, EMPIRE_GOALS(emp), goal, next_goal) {
					LL_FOREACH(goal->tracker, task) {
						if (task->type == REQ_CROP_VARIETY) {
							if (crop_var == -1) {
								// only look up once per empire
								crop_var = count_empire_crop_variety(emp, task->needed, NO_ISLAND);
							}
							task->current = crop_var;
							TRIGGER_DELAYED_REFRESH(emp, DELAY_REFRESH_GOAL_COMPLETE);
						}
					}
				}
			}
			if (IS_SET(EMPIRE_DELAYED_REFRESH(emp), DELAY_REFRESH_GOAL_COMPLETE)) {
				HASH_ITER(hh, EMPIRE_GOALS(emp), goal, next_goal) {
					count_quest_tasks(goal->tracker, &complete, &total);
					if (complete == total) {
						complete_goal(emp, goal);
					}
				}
			}
			if (IS_SET(EMPIRE_DELAYED_REFRESH(emp), DELAY_REFRESH_MEMBERS)) {
				read_empire_members(emp, FALSE);
			}
			
			// clear this
			EMPIRE_DELAYED_REFRESH(emp) = NOBITS;
		}
		
		check_delayed_refresh = FALSE;
	}
}


/**
* @param empire_data *emp
* @return int the number of members online
*/
int count_members_online(empire_data *emp) {
	descriptor_data *d;
	int count = 0;
	
	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) == CON_PLAYING && d->character && GET_LOYALTY(d->character) == emp) {
			++count;
		}
	}
	
	return count;
}


/**
* @param empire_data *emp An empire
* @return int The number of technologies known by that empire
*/
int count_tech(empire_data *emp) {
	int iter, count = 0;
	
	for (iter = 0; iter < NUM_TECHS; ++iter) {
		if (EMPIRE_HAS_TECH(emp, iter)) {
			++count;
		}
	}
	
	return count;
}


/**
* Gets an empire's total score -- only useful if you've called score_empires()
*
* @param empire_data *emp The empire to check.
* @return int The empire's score.
*/
int get_total_score(empire_data *emp) {
	int total, iter;
	
	if (!emp) {
		return 0;
	}
	
	total = 0;
	for (iter = 0; iter < NUM_SCORES; ++iter) {
		total += EMPIRE_SCORE(emp, iter);
	}
	
	return total;
}


/**
* Trigger a score update and re-sort of the empires.
*
* @param bool force Overrides the time limit on resorting.
*/
void resort_empires(bool force) {
	extern int sort_empires(empire_data *a, empire_data *b);
	
	static time_t last_sort_time = 0;
	static int last_sort_size = 0;

	// prevent constant re-sorting by limiting this to once per minute
	if (force || last_sort_size != HASH_COUNT(empire_table) || last_sort_time + (60 * 1) < time(0)) {
		// better score them first
		score_empires();
		HASH_SORT(empire_table, sort_empires);

		last_sort_size = HASH_COUNT(empire_table);
		last_sort_time = time(0);
	}
}


// score_empires helper type
struct scemp_type {
	int value;
	struct scemp_type *next;	// LL
};


// score_empires sorter
int compare_scemp(struct scemp_type *a, struct scemp_type *b) {
	return a->value - b->value;
}


/**
* score_empires helper adder: inserts a value in order
*
* @param struct scemp_type **list A pointer to the list to add to.
* @param int value The number to add to the list.
*/
static inline void add_scemp(struct scemp_type **list, int value) {
	struct scemp_type *scemp;
	CREATE(scemp, struct scemp_type, 1);
	scemp->value = value;
	LL_INSERT_INORDER(*list, scemp, compare_scemp);
}


/**
* This function assigns scores to all empires so they can be ranked on the
* empire list.
*/
void score_empires(void) {
	extern double empire_score_average[NUM_SCORES];
	extern const double score_levels[];
	
	struct scemp_type *scemp, *next_scemp, *lists[NUM_SCORES];
	int iter, pos, median, num_emps = 0;
	struct empire_storage_data *store, *next_store;
	struct empire_island *isle, *next_isle;
	empire_data *emp, *next_emp;
	long long num;
	
	int time_to_empire_emptiness = config_get_int("time_to_empire_emptiness") * SECS_PER_REAL_WEEK;

	// clear data	
	for (iter = 0; iter < NUM_SCORES; ++iter) {
		lists[iter] = NULL;
		empire_score_average[iter] = 0;
	}
	
	// clear scores first
	HASH_ITER(hh, empire_table, emp, next_emp) {
		for (iter = 0; iter < NUM_SCORES; ++iter) {
			EMPIRE_SCORE(emp, iter) = 0;
		}
		EMPIRE_SORT_VALUE(emp) = 0;
	}
	
	#define SCORE_SKIP_EMPIRE(ee)  (EMPIRE_IMM_ONLY(ee) || EMPIRE_MEMBERS(ee) == 0 || EMPIRE_LAST_LOGON(ee) + time_to_empire_emptiness < time(0))

	// build data
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (SCORE_SKIP_EMPIRE(emp)) {
			continue;
		}
		
		// found one
		++num_emps;
		
		// detect scores -- and store later for easy comparison
		add_scemp(&lists[SCORE_WEALTH], GET_TOTAL_WEALTH(emp));
		EMPIRE_SCORE(emp, SCORE_WEALTH) = GET_TOTAL_WEALTH(emp);
		
		add_scemp(&lists[SCORE_TERRITORY], (num = land_can_claim(emp, TER_TOTAL)));
		EMPIRE_SCORE(emp, SCORE_TERRITORY) = num;
		
		add_scemp(&lists[SCORE_MEMBERS], EMPIRE_MEMBERS(emp));
		EMPIRE_SCORE(emp, SCORE_MEMBERS) = EMPIRE_MEMBERS(emp);
		
		add_scemp(&lists[SCORE_COMMUNITY], (num = EMPIRE_PROGRESS_POINTS(emp, PROGRESS_COMMUNITY)));
		EMPIRE_SCORE(emp, SCORE_COMMUNITY) = num;
		
		num = 0;
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
			HASH_ITER(hh, isle->store, store, next_store) {
				SAFE_ADD(num, store->amount, 0, MAX_INT, FALSE);
			}
		}
		num /= 1000;	// for sanity of number size
		add_scemp(&lists[SCORE_INVENTORY], num);
		EMPIRE_SCORE(emp, SCORE_INVENTORY) = num;
		
		add_scemp(&lists[SCORE_GREATNESS], EMPIRE_GREATNESS(emp));
		EMPIRE_SCORE(emp, SCORE_GREATNESS) = EMPIRE_GREATNESS(emp);
		
		add_scemp(&lists[SCORE_INDUSTRY], (num = EMPIRE_PROGRESS_POINTS(emp, PROGRESS_INDUSTRY)));
		EMPIRE_SCORE(emp, SCORE_INDUSTRY) = num;
		
		add_scemp(&lists[SCORE_PRESTIGE], (num = EMPIRE_PROGRESS_POINTS(emp, PROGRESS_PRESTIGE)));
		EMPIRE_SCORE(emp, SCORE_PRESTIGE) = num;
		
		add_scemp(&lists[SCORE_DEFENSE], (num = EMPIRE_PROGRESS_POINTS(emp, PROGRESS_DEFENSE)));
		EMPIRE_SCORE(emp, SCORE_DEFENSE) = num;
		
		add_scemp(&lists[SCORE_PLAYTIME], EMPIRE_TOTAL_PLAYTIME(emp));
		EMPIRE_SCORE(emp, SCORE_PLAYTIME) = EMPIRE_TOTAL_PLAYTIME(emp);
	}
	
	// determine medians and free lists
	median = num_emps / 2 + 1;
	median = MIN(num_emps, median);
	for (iter = 0; iter < NUM_SCORES; ++iter) {
		pos = 0;
		LL_FOREACH_SAFE(lists[iter], scemp, next_scemp) {
			if (pos++ == median) {
				empire_score_average[iter] = scemp->value;
			}
			free(scemp);
		}
	}
	
	// apply scores to empires based on how they fared
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (SCORE_SKIP_EMPIRE(emp)) {
			continue;
		}
			
		for (iter = 0; iter < NUM_SCORES; ++iter) {
			num = 0;
			
			// score_levels[] terminates with a -1 but I don't trust doubles enough to do anything other than >= 0 here
			if (EMPIRE_SCORE(emp, iter) > 0) {
				for (pos = 0; score_levels[pos] >= 0; ++pos) {
					if (EMPIRE_SCORE(emp, iter) >= (empire_score_average[iter] * score_levels[pos])) {
						++num;
					}
				}
			}
			
			// assign permanent score
			EMPIRE_SORT_VALUE(emp) += EMPIRE_SCORE(emp, iter);
			EMPIRE_SCORE(emp, iter) = num;
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE TRADE UTILS //////////////////////////////////////////////////////


// helper type for short list of empires
struct partner_list_type {
	empire_data *emp;
	double rate;
	
	struct partner_list_type *next;	// doubly-linked list
	struct partner_list_type *prev;
};


// helper type for cost sorting
struct import_pair_type {
	empire_data *emp;	// trading partner
	int amount;	// how many (up to the amt we want)
	double cost;	// cost each
	double rate;	// the rate we paid
	
	struct import_pair_type *next;	// doubly-linked list
	struct import_pair_type *prev;
};


// simple sorter for cost of partners (ascending order of cost)
int sort_import_partners(struct import_pair_type *a, struct import_pair_type *b) {
	return (a->cost > b->cost) - (a->cost < b->cost);
}


/**
* Attempts to import goods into the empire from trading partners. We have
* already checked TECH_TRADE_ROUTES before this, and that the empire is not
* timed out.
*
* @param empire_data *emp The empire to import to.
* @return bool TRUE if any items moved
*/
bool process_import_one(empire_data *emp) {
	extern int get_main_island(empire_data *emp);
	
	struct partner_list_type *plt, *next_plt, *partner_list = NULL;
	struct import_pair_type *pair, *next_pair, *pair_list;
	int my_amt, their_amt, trade_amt, found_island = NO_ISLAND;
	struct empire_trade_data *trade, *p_trade;
	empire_data *partner, *next_partner;
	int limit = config_get_int("imports_per_day");
	bool any = FALSE;
	obj_data *orn;
	double cost;
	
	// find trading partners
	HASH_ITER(hh, empire_table, partner, next_partner) {
		if (is_trading_with(emp, partner)) {
			CREATE(plt, struct partner_list_type, 1);
			plt->emp = partner;
			plt->rate = exchange_rate(emp, partner);
			DL_APPEND(partner_list, plt);	// NOTE: reverses order of the partner list
		}
	}
	
	// find items to trade
	LL_FOREACH(EMPIRE_TRADE(emp), trade) {
		if (limit <= 0) {
			break;	// done!
		}
		
		if (trade->type != TRADE_IMPORT) {
			continue;	// not an import
		}
		if ((my_amt = get_total_stored_count(emp, trade->vnum, TRUE)) >= trade->limit) {
			continue;	// don't need any of it
		}
		
		// build a list of trading partners who have it
		pair_list = NULL;
		DL_FOREACH(partner_list, plt) {
			if (!(p_trade = find_trade_entry(plt->emp, TRADE_EXPORT, trade->vnum))) {
				continue;	// not trading this item
			}
			if ((p_trade->cost * (1.0/plt->rate)) > trade->cost) {
				continue;	// too expensive
			}
			if ((their_amt = get_total_stored_count(plt->emp, trade->vnum, FALSE)) <= p_trade->limit) {
				continue;	// they don't have enough (don't count shipping -- it's not tradable)
			}
			
			// compute real amounts:
			their_amt -= p_trade->limit;	// how much they have available to trade
			their_amt = MIN((trade->limit - my_amt), their_amt);	// how many we actually need
			
			// seems valid!
			if (their_amt > 0) {
				CREATE(pair, struct import_pair_type, 1);
				pair->emp = plt->emp;
				pair->amount = their_amt;
				pair->rate = plt->rate;
				pair->cost = p_trade->cost * (1.0/plt->rate);
				DL_APPEND(pair_list, pair);
			}
		}
		
		// anything to trade?
		if (!pair_list) {
			continue;
		}
		
		// sort the list
		DL_SORT(pair_list, sort_import_partners);
		
		// attempt to trade up to the limit
		DL_FOREACH(pair_list, pair) {
			if (limit <= 0) {
				break;	// done!
			}
			
			// amount to actually trade now:
			trade_amt = trade->limit - my_amt;	// amount we need
			trade_amt = MIN(trade_amt, pair->amount);	// amount available
			trade_amt = MIN(trade_amt, limit);	// amount we can import
			if (trade_amt < 1) {
				break; // don't neeed anymore?
			}
			
			// compute cost for this trade (pair->cost is already rate-exchanged)
			cost = trade_amt * pair->cost;
			
			// can we afford it?
			if (EMPIRE_COINS(emp) < cost) {
				trade_amt = EMPIRE_COINS(emp) / pair->cost;	// reduce to how many we can afford
				cost = trade_amt * pair->cost;
				if (trade_amt < 1) {
					continue;	// can't afford any
				}
			}
			
			// only store it if we did find an island to store to
			if (found_island != NO_ISLAND || (found_island = get_main_island(emp)) != NO_ISLAND) {
				// items
				add_to_empire_storage(emp, found_island, trade->vnum, trade_amt);
				charge_stored_resource(pair->emp, ANY_ISLAND, trade->vnum, trade_amt);
				
				// mark gather trackers
				add_production_total(emp, trade->vnum, trade_amt);
				mark_production_trade(emp, trade->vnum, trade_amt, 0);
				mark_production_trade(pair->emp, trade->vnum, 0, trade_amt);
				
				// money
				decrease_empire_coins(emp, emp, cost);
				increase_empire_coins(pair->emp, pair->emp, cost * pair->rate);
				
				// update limit
				limit -= trade_amt;
				any = TRUE;
				
				// log
				orn = obj_proto(trade->vnum);
				log_to_empire(emp, ELOG_TRADE, "Imported %s x%d from %s for %.1f coins", GET_OBJ_SHORT_DESC(orn), trade_amt, EMPIRE_NAME(pair->emp), cost);
				log_to_empire(pair->emp, ELOG_TRADE, "Exported %s x%d to %s for %.1f coins", GET_OBJ_SHORT_DESC(orn), trade_amt, EMPIRE_NAME(emp), cost * pair->rate);
			}
		}
		
		// free this partner list
		DL_FOREACH_SAFE(pair_list, pair, next_pair) {
			DL_DELETE(pair_list, pair);
			free(pair);
		}
	}
	
	// cleaup
	DL_FOREACH_SAFE(partner_list, plt, next_plt) {
		DL_DELETE(partner_list, plt);
		free(plt);
	}
	
	return any;
}


// runs daily imports
void process_imports(void) {
	void read_vault(empire_data *emp);
	
	empire_data *emp, *next_emp;
	int amount;
	
	int time_to_empire_emptiness = config_get_int("time_to_empire_emptiness") * SECS_PER_REAL_WEEK;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_IMM_ONLY(emp)) {
			continue;
		}
		if (!EMPIRE_HAS_TECH(emp, TECH_TRADE_ROUTES)) {
			continue;
		}
		if (EMPIRE_LAST_LOGON(emp) + time_to_empire_emptiness < time(0)) {
			continue;
		}
		
		// ok go
		amount = process_import_one(emp);
		
		if (amount > 0) {
			read_vault(emp);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE DIPLOMACY UTILS //////////////////////////////////////////////////

/**
* This function prevents a player from building/claiming a location while
* at war with a person whose city it would be within.
*
* @param char_data *ch The person trying to build/claim.
* @param room_data *loc The map location.
* @return bool TRUE if it's okay to build/claim; FALSE if not.
*/
bool can_build_or_claim_at_war(char_data *ch, room_data *loc) {
	struct empire_political_data *pol;
	empire_data *enemy;
	bool junk;
	
	// if they aren't at war, this doesn't apply
	if (!ch || !is_at_war(GET_LOYALTY(ch))) {
		return TRUE;
	}
	
	// if they already own it, it's ok
	if (GET_LOYALTY(ch) && ROOM_OWNER(IN_ROOM(ch)) == GET_LOYALTY(ch)) {
		return TRUE;
	}
	
	// if it's in one of their OWN cities, it's ok
	if (GET_LOYALTY(ch) && is_in_city_for_empire(loc, GET_LOYALTY(ch), TRUE, &junk)) {
		return TRUE;
	}
	
	// find who they are at war with
	for (pol = EMPIRE_DIPLOMACY(GET_LOYALTY(ch)); pol; pol = pol->next) {
		if (IS_SET(pol->type, DIPL_WAR)) {
			// not good if they are trying to build in a location owned by the other player while at war
			if ((enemy = real_empire(pol->id)) && is_in_city_for_empire(loc, enemy, TRUE, &junk)) {
				return FALSE;
			}
		}
	}
	
	return TRUE;
}


/**
* Similar to has_relationship except it takes two characters, and attempts to
* check their player masters first. It also considers npc affinities, if there
* is no player master.
*
* @param char_data *ch_a One character.
* @param char_data *ch_b Another character.
* @param bitvector_t dipl_bits Any DIPL_ flags.
* @return bool TRUE if the characters have the requested diplomacy.
*/
bool char_has_relationship(char_data *ch_a, char_data *ch_b, bitvector_t dipl_bits) {
	char_data *top_a, *top_b;
	empire_data *emp_a, *emp_b;
	
	if (!ch_a || !ch_b || ch_a == ch_b) {
		return FALSE;
	}
	
	// find a player for each
	top_a = ch_a;
	while (IS_NPC(top_a) && top_a->master) {
		top_a = top_a->master;
	}
	top_b = ch_b;
	while (IS_NPC(top_b) && top_b->master) {
		top_b = top_b->master;
	}
	
	// look up 
	emp_a = GET_LOYALTY(top_a);
	emp_b = GET_LOYALTY(top_b);
	
	if (!emp_a || !emp_b) {
		return FALSE;
	}
	
	return has_relationship(emp_a, emp_b, dipl_bits);
}


/**
* @param empire_data *emp empire A
* @param empire_data *enemy empire B
* @return bool TRUE if emp is nonaggression/ally/trade/peace
*/
bool empire_is_friendly(empire_data *emp, empire_data *enemy) {
	struct empire_political_data *pol;
	
	if (emp && enemy) {
		if ((pol = find_relation(emp, enemy))) {
			if (IS_SET(pol->type, DIPL_ALLIED | DIPL_NONAGGR | DIPL_TRADE | DIPL_PEACE)) {
				return TRUE;
			}
		}
	}

	return FALSE;
}


/**
* @param empire_data *emp empire A
* @param empire_data *enemy empire B
* @param room_data *loc The location to check (optional; pass NULL to ignore)
* @return bool TRUE if emp is at war/distrust
*/
bool empire_is_hostile(empire_data *emp, empire_data *enemy, room_data *loc) {
	struct empire_political_data *pol;
	bool distrustful = FALSE;
	
	if (emp == enemy || !emp) {
		return FALSE;
	}
	
	if ((!loc && IS_SET(EMPIRE_FRONTIER_TRAITS(emp), ETRAIT_DISTRUSTFUL)) || has_empire_trait(emp, loc, ETRAIT_DISTRUSTFUL)) {
		distrustful = TRUE;
	}
	
	if (emp && enemy) {
		if ((pol = find_relation(emp, enemy))) {
			if (IS_SET(pol->type, DIPL_WAR | DIPL_DISTRUST) || (distrustful && !empire_is_friendly(emp, enemy))) {
				return TRUE;
			}
		}
		if ((pol = find_relation(enemy, emp))) {	// the reverse
			if (IS_SET(pol->type, DIPL_THIEVERY) && (distrustful && !empire_is_friendly(enemy, emp))) {
				return TRUE;
			}
		}
	}

	return (distrustful && !empire_is_friendly(emp, enemy));
}


/**
* Clears out old diplomacy to keep things clean and simple.
*/
void expire_old_politics(void) {
	empire_data *emp, *next_emp, *other;
	struct empire_political_data *pol, *next_pol, *find_pol;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (!EMPIRE_IS_TIMED_OUT(emp) || EMPIRE_MEMBERS(emp) > 0 || EMPIRE_TERRITORY(emp, TER_TOTAL) > 0) {
			continue;
		}
		
		LL_FOREACH_SAFE(EMPIRE_DIPLOMACY(emp), pol, next_pol) {
			if ((other = real_empire(pol->id)) && other != emp) {
				if (pol->type != NOBITS) {
					log_to_empire(emp, ELOG_DIPLOMACY, "Diplomatic relations with %s have ended because your empire is in ruins", EMPIRE_NAME(other));
					log_to_empire(other, ELOG_DIPLOMACY, "Diplomatic relations with %s have ended because that empire is in ruins", EMPIRE_NAME(emp));
				}
				if ((find_pol = find_relation(other, emp))) {
					LL_DELETE(EMPIRE_DIPLOMACY(other), find_pol);
					free(find_pol);
				}
				EMPIRE_NEEDS_SAVE(other) = TRUE;
			}
			
			LL_DELETE(EMPIRE_DIPLOMACY(emp), pol);
			free(pol);
			
			EMPIRE_NEEDS_SAVE(emp) = TRUE;
		}
	}
}


/**
* Determines if an empire has a trait set at a certain location. It auto-
* matically detects if the location is covered by a city's traits or the
* empire's frontier traits.
*
* @param empire_data *emp The empire to check.
* @param room_data *loc The location to check (optional; pass NULL to ignore).
* @param bitvector_t ETRAIT_x flag(s).
* @return bool TRUE if the empire has that (those) trait(s) at loc.
*/
bool has_empire_trait(empire_data *emp, room_data *loc, bitvector_t trait) {
	extern struct city_metadata_type city_type[];
	
	struct empire_city_data *city;
	bitvector_t set = NOBITS;
	bool near_city = FALSE;
	
	double outskirts_mod = config_get_double("outskirts_modifier");
	
	// short-circuit
	if (!emp) {
		return FALSE;
	}
	
	if (loc) {	// see if it's near enough to any cities
		LL_FOREACH(EMPIRE_CITY_LIST(emp), city) {
			if (compute_distance(loc, city->location) <= city_type[city->type].radius * outskirts_mod) {
				near_city = TRUE;
				break;	// only need 1
			}
		}
	}
	
	set = near_city ? city->traits : EMPIRE_FRONTIER_TRAITS(emp);
	
	return (IS_SET(set, trait) ? TRUE : FALSE);
}


/**
* @param empire_data *emp one empire
* @param empire_data *fremp other empire
* @param bitvector_t diplomacy DIPL_
* @return TRUE if emp and fremp have the given diplomacy with each other
*/
bool has_relationship(empire_data *emp, empire_data *fremp, bitvector_t diplomacy) {
	struct empire_political_data *pol;
	
	if (emp && fremp) {
		if ((pol = find_relation(emp, fremp))) {
			return IS_SET(pol->type, diplomacy) ? TRUE : FALSE;
		}
	}
	
	return FALSE;
}


/**
* Determines if an empire is currently participating in any wars.
*
* @param empire_data *emp The empire to check.
* @return bool TRUE if the empire is in at least one war.
*/
bool is_at_war(empire_data *emp) {
	struct empire_political_data *pol;
	
	if (!emp) {
		return FALSE;
	}
	
	for (pol = EMPIRE_DIPLOMACY(emp); pol; pol = pol->next) {
		if (IS_SET(pol->type, DIPL_WAR)) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* Determine if 2 empires are capable of trading with each other. This returns
* false if either is unable to trade for any reason.
*
* @param empire_data *emp The first empire.
* @param empire_data *partner The other empire. (argument order does not matter)
* @return bool TRUE if the two empires are trading and able to trade.
*/
bool is_trading_with(empire_data *emp, empire_data *partner) {
	int time_to_empire_emptiness = config_get_int("time_to_empire_emptiness") * SECS_PER_REAL_WEEK;
	
	// no self-trades or invalid empires
	if (emp == partner || !emp || !partner) {
		return FALSE;
	}
	// neither can be imm-only
	if (EMPIRE_IMM_ONLY(emp) || EMPIRE_IMM_ONLY(partner)) {
		return FALSE;
	}
	// both must have trade routes
	if (!EMPIRE_HAS_TECH(emp, TECH_TRADE_ROUTES) || !EMPIRE_HAS_TECH(partner, TECH_TRADE_ROUTES)) {
		return FALSE;
	}
	// neither can have hit the emptiness point
	if (EMPIRE_LAST_LOGON(emp) + time_to_empire_emptiness < time(0) || EMPIRE_LAST_LOGON(partner) + time_to_empire_emptiness < time(0)) {
		return FALSE;
	}
	// they need to have trade relations
	if (!has_relationship(emp, partner, DIPL_TRADE)) {
		return FALSE;
	}
	
	// ok!
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE PERMISSIONS UTILS ////////////////////////////////////////////////

/**
* Determines if a character can legally claim land.
*
* @param char_data *ch The player.
* @return bool TRUE if the player is allowed to claim.
*/
bool can_claim(char_data *ch) {
	if (IS_NPC(ch)) {
		return FALSE;	// npcs never claim
	}
	if (GET_LOYALTY(ch) && GET_RANK(ch) < EMPIRE_PRIV(GET_LOYALTY(ch), PRIV_CLAIM)) {
		return FALSE;	// rank too low
	}
	
	// and check the empire itself
	return empire_can_claim(GET_LOYALTY(ch));
}


/**
* Determines if a character an use a room (chop, withdraw, etc).
* Unclaimable rooms are ok for GUESTS_ALLOWED.
*
* @param char_data *ch The character.
* @param room_data *room The location.
* @param int mode -- GUESTS_ALLOWED, MEMBERS_AND_ALLIES, MEMBERS_ONLY
* @return bool TRUE if ch can use room, FALSE otherwise
*/
bool can_use_room(char_data *ch, room_data *room, int mode) {
	room_data *homeroom = HOME_ROOM(room);

	// no owner?
	if (!ROOM_OWNER(homeroom)) {
		return TRUE;
	}
	// empire ownership
	if (ROOM_OWNER(homeroom) == GET_LOYALTY(ch)) {
		// private room?
		if (ROOM_PRIVATE_OWNER(homeroom) != NOBODY && ROOM_PRIVATE_OWNER(homeroom) != GET_IDNUM(ch) && GET_RANK(ch) < EMPIRE_NUM_RANKS(ROOM_OWNER(homeroom))) {
			return FALSE;
		}
	}
	// public + guest + hostile + no-empire exclusion
	if (ROOM_AFF_FLAGGED(homeroom, ROOM_AFF_PUBLIC) && mode == GUESTS_ALLOWED && !GET_LOYALTY(ch) && IS_HOSTILE(ch)) {
		return FALSE;
	}
	
	// otherwise it's just whether ch's empire can use it
	return emp_can_use_room(GET_LOYALTY(ch), room, mode);
}


/**
* Determines if an empire can use a room (e.g. send a ship through it).
* Unclaimable rooms are ok for GUESTS_ALLOWED.
*
* @param empire_data *emp The empire trying to use it.
* @param room_data *room The location.
* @param int mode -- GUESTS_ALLOWED, MEMBERS_AND_ALLIES, MEMBERS_ONLY
* @return bool TRUE if emp can use room, FALSE otherwise
*/
bool emp_can_use_room(empire_data *emp, room_data *room, int mode) {
	room_data *homeroom = HOME_ROOM(room);

	// unclaimable always denies MEMBERS_x
	if (mode != GUESTS_ALLOWED && ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE)) {
		return FALSE;
	}
	// no owner?
	if (!ROOM_OWNER(homeroom)) {
		return TRUE;
	}
	// public + guests (not at war or hostile)
	if (ROOM_AFF_FLAGGED(homeroom, ROOM_AFF_PUBLIC) && mode == GUESTS_ALLOWED && !has_relationship(emp, ROOM_OWNER(homeroom), DIPL_WAR)) {
		return TRUE;
	}
	// empire ownership
	if (ROOM_OWNER(homeroom) == emp) {
		return TRUE;
	}
	// check allies if not a private room
	if (mode != MEMBERS_ONLY && ROOM_PRIVATE_OWNER(homeroom) == NOBODY && has_relationship(ROOM_OWNER(homeroom), emp, DIPL_ALLIED)) {
		return TRUE;
	}
	
	// newp
	return FALSE;
}


/**
* Determines if an empire can use a vehicle.
*
* @param empire_data *emp The empire trying to use it -- MAY be null.
* @param vehicle_data *veh The vehicle it's trying to use.
* @param int mode -- GUESTS_ALLOWED, MEMBERS_AND_ALLIES, MEMBERS_ONLY
* @return bool TRUE if emp can use veh, FALSE otherwise
*/
bool emp_can_use_vehicle(empire_data *emp, vehicle_data *veh, int mode) {
	room_data *interior = VEH_INTERIOR_HOME_ROOM(veh);	// if any
	
	// no owner?
	if (!VEH_OWNER(veh)) {
		return TRUE;
	}
	// empire ownership
	if (VEH_OWNER(veh) == emp) {
		return TRUE;
	}
	// public + guests
	if (interior && ROOM_AFF_FLAGGED(interior, ROOM_AFF_PUBLIC) && mode == GUESTS_ALLOWED) {
		return TRUE;
	}
	// check allies
	if (mode != MEMBERS_ONLY && emp && has_relationship(VEH_OWNER(veh), emp, DIPL_ALLIED)) {
		return TRUE;
	}
	
	// newp
	return FALSE;
}


/**
* Determines if an empire can currently claim land.
*
* @param empire_data *emp The empire.
* @return bool TRUE if the empire is allowed to claim.
*/
bool empire_can_claim(empire_data *emp) {
	if (!emp) {	// theoretically, no empire == you can found one...
		return TRUE;
	}
	if (EMPIRE_TERRITORY(emp, TER_TOTAL) >= land_can_claim(emp, TER_TOTAL)) {
		return FALSE;	// too much territory
	}
	
	// seems ok
	return TRUE;
}


/**
* Checks the room to see if ch has permission.
*
* @param char_data *ch
* @param int type PRIV_
* @param room_data *loc Optional: For permission checks that only matter on claimed tiles. (Pass NULL if location doesn't matter.)
* @return bool TRUE if it's ok
*/
bool has_permission(char_data *ch, int type, room_data *loc) {
	empire_data *loc_emp = loc ? ROOM_OWNER(HOME_ROOM(loc)) : NULL;
	
	if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES)) {
		return FALSE;
	}
	else if (loc_emp && GET_LOYALTY(ch) == loc_emp && GET_RANK(ch) < EMPIRE_PRIV(loc_emp, type)) {
		// for empire members only
		return FALSE;
	}
	else if (loc_emp && GET_LOYALTY(ch) != loc_emp && EMPIRE_PRIV(loc_emp, type) > 1) {
		// allies can't use things that are above rank 1 in the owner's empire
		return FALSE;
	}
	else if (!loc && GET_LOYALTY(ch) && GET_RANK(ch) < EMPIRE_PRIV(GET_LOYALTY(ch), type)) {
		// privileges too low on global check
		return FALSE;
	}
	
	return TRUE;
}


/**
* This determines if the character's current location has the tech available.
* It takes the location into account, not just the tech flags.
*
* @param char_data *ch acting character, although this is location-based
* @param int tech TECH_ id
* @return TRUE if successful, FALSE if not (and sends its own error message to ch)
*/
bool has_tech_available(char_data *ch, int tech) {
	extern const char *techs[];

	if (!ROOM_OWNER(IN_ROOM(ch))) {
		msg_to_char(ch, "In order to do that you need to be in the territory of an empire with %s.\r\n", techs[tech]);
		return FALSE;
	}
	else if (!has_tech_available_room(IN_ROOM(ch), tech)) {
		msg_to_char(ch, "In order to do that you need to be in the territory of an empire with %s on this island.\r\n", techs[tech]);
		return FALSE;
	}
	else {
		return TRUE;
	}
}


/**
* This determines if the given location has the tech available.
* It takes the location into account, not just the tech flags.
*
* @param room_data *room The location to check.
* @param int tech TECH_ id
* @return TRUE if successful, FALSE if not (and sends its own error message to ch)
*/
bool has_tech_available_room(room_data *room, int tech) {
	extern const int techs_requiring_same_island[];
	
	empire_data *emp = ROOM_OWNER(room);
	bool requires_island = FALSE;
	struct empire_island *isle;
	int id, iter;
	
	if (!emp) {
		return FALSE;
	}
	
	// see if it requires island
	for (iter = 0; techs_requiring_same_island[iter] != NOTHING; ++iter) {
		if (tech == techs_requiring_same_island[iter]) {
			requires_island = TRUE;
			break;
		}
	}
	
	// easy way out
	if (!requires_island && EMPIRE_HAS_TECH(emp, tech)) {
		return TRUE;
	}
	
	// check the island
	id = GET_ISLAND_ID(room);
	if (id != NO_ISLAND && (isle = get_empire_island(emp, id))) {
		return (isle->tech[tech] > 0);
	}
	
	// nope
	return FALSE;
}


/**
* Calculates the total claimable land for an empire.
*
* @param empire_data *emp An empire number.
* @param int ter_type Any TER_ to determine how much territory an empire can claim of that type.
* @return int The total claimable land.
*/
int land_can_claim(empire_data *emp, int ter_type) {
	int cur, from_wealth, out_t = 0, fron_t = 0, total = 0, min_cap = 0;
	double outskirts_mod = config_get_double("land_outside_city_modifier");
	double frontier_mod = config_get_double("land_frontier_modifier");
	int frontier_timeout = config_get_int("frontier_timeout");
	
	if (!emp) {
		return 0;
	}
	if (ter_type == TER_FRONTIER && frontier_timeout > 0 && (EMPIRE_LAST_LOGON(emp) + (frontier_timeout * SECS_PER_REAL_DAY)) < time(0)) {
		return 0;	// no frontier territory if gone longer than this
	}
	
	// so long as there's at least 1 active member, they get the min cap
	if (EMPIRE_MEMBERS(emp) > 0) {
		min_cap = config_get_int("land_min_cap");
	}
	
	// basics
	total += EMPIRE_GREATNESS(emp) * (config_get_int("land_per_greatness") + EMPIRE_ATTRIBUTE(emp, EATT_TERRITORY_PER_GREATNESS)) + EMPIRE_ATTRIBUTE(emp, EATT_BONUS_TERRITORY);
	
	if (EMPIRE_ATTRIBUTE(emp, EATT_TERRITORY_PER_100_WEALTH) > 0) {
		from_wealth = GET_TOTAL_WEALTH(emp) / 100.0 * EMPIRE_ATTRIBUTE(emp, EATT_TERRITORY_PER_100_WEALTH);
		
		// limited to 3x non-wealth territory
		from_wealth = MIN(from_wealth, total * 3);
		
		// for a total of 4x
		total += from_wealth;
	}
	
	// determine specific caps and apply minimum
	total = MAX(total, min_cap);
	if (ter_type == TER_TOTAL || ter_type == TER_CITY) {
		return total;	// shortcut -- no further work
	}
	
	out_t = total * (outskirts_mod + frontier_mod);
	out_t = MAX(out_t, min_cap);
	fron_t = total * frontier_mod;
	fron_t = MAX(fron_t, min_cap);
	
	// check cascading categories
	if (EMPIRE_TERRITORY(emp, TER_CITY) > (total - out_t)) {
		out_t -= (EMPIRE_TERRITORY(emp, TER_CITY) - (total - out_t));
		out_t = MAX(0, out_t);
	}
	if (EMPIRE_TERRITORY(emp, TER_CITY) + EMPIRE_TERRITORY(emp, TER_OUTSKIRTS) > (total - fron_t)) {
		fron_t -= (EMPIRE_TERRITORY(emp, TER_CITY) + EMPIRE_TERRITORY(emp, TER_OUTSKIRTS) - (total - fron_t));
		fron_t = MAX(0, fron_t);
	}
	if (EMPIRE_TERRITORY(emp, TER_OUTSKIRTS) > out_t - fron_t) {
		fron_t -= (EMPIRE_TERRITORY(emp, TER_OUTSKIRTS) - (out_t - fron_t));
		fron_t = MAX(0, fron_t);
	}
	
	switch (ter_type) {
		case TER_OUTSKIRTS: {
			// subtract out currently-used frontier, as that territory is shared with outskirts
			cur = MIN(fron_t, EMPIRE_TERRITORY(emp, TER_FRONTIER));
			out_t -= cur;
			
			return out_t;
		}
		case TER_FRONTIER: {
			return fron_t;
		}
		// default: no changes
		default: {
			return total;
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// FILE UTILS //////////////////////////////////////////////////////////////

/**
* Gets the filename/path for various name-divided file directories.
*
* @param char *orig_name The player name.
* @param char *filename A variable to write the filename to.
* @return int 1=success, 0=fail
*/
int get_filename(char *orig_name, char *filename, int mode) {
	const char *prefix, *middle, *suffix;
	char name[64], *ptr;

	if (orig_name == NULL || *orig_name == '\0' || filename == NULL) {
		log("SYSERR: NULL pointer or empty string passed to get_filename(), %p or %p.", orig_name, filename);
		return (0);
	}

	switch (mode) {
		case PLR_FILE: {
			prefix = LIB_PLAYERS;
			suffix = SUF_PLR;
			break;
		}
		case DELAYED_FILE: {
			prefix = LIB_PLAYERS;
			suffix = SUF_DELAY;
			break;
		}
		default: {
			return (0);
		}
	}

	strcpy(name, orig_name);
	for (ptr = name; *ptr; ptr++)
		*ptr = LOWER(*ptr);

	if (LOWER(*name) <= 'e')
		middle = "A-E";
	else if (LOWER(*name) <= 'j')
		middle = "F-J";
	else if (LOWER(*name) <= 'o')
		middle = "K-O";
	else if (LOWER(*name) <= 't')
		middle = "P-T";
	else if (LOWER(*name) <= 'z')
		middle = "U-Z";
	else
		middle = "ZZZ";

	/* If your compiler gives you shit about <= '', use this switch:
		switch (LOWER(*name)) {
			case 'a':  case 'b':  case 'c':  case 'd':  case 'e':
				middle = "A-E";
				break;
			case 'f':  case 'g':  case 'h':  case 'i':  case 'j':
				middle = "F-J";
				break;
			case 'k':  case 'l':  case 'm':  case 'n':  case 'o':
				middle = "K-O";
				break;
			case 'p':  case 'q':  case 'r':  case 's':  case 't':
				middle = "P-T";
				break;
			case 'u':  case 'v':  case 'w':  case 'x':  case 'y':  case 'z':
				middle = "U-Z";
				break;
			default:
				middle = "ZZZ";
				break;
		}
	*/

	sprintf(filename, "%s%s/%s.%s", prefix, middle, name, suffix);
	return (1);
}


/*
 * get_line reads the next non-blank line off of the input stream.
 * The newline character is removed from the input.  Lines which begin
 * with '*' are considered to be comments.
 *
 * Returns the number of lines advanced in the file.
 */
int get_line(FILE *fl, char *buf) {
	char temp[256];
	int lines = 0;

	do {
		fgets(temp, 256, fl);
		if (feof(fl)) {
			return (0);
		}
		lines++;
	} while (*temp == '*' || *temp == '\n');
	
	if (temp[strlen(temp) - 1] == '\n') {
		temp[strlen(temp) - 1] = '\0';
	}
	strcpy(buf, temp);
	return (lines);
}


/* the "touch" command, essentially. */
int touch(const char *path) {
	FILE *fl;

	if (!(fl = fopen(path, "a"))) {
		log("SYSERR: %s: %s", path, strerror(errno));
		return (-1);
	}
	else {
		fclose(fl);
		return (0);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// LOGGING UTILS ///////////////////////////////////////////////////////////


/*
 * New variable argument log() function.  Works the same as the old for
 * previously written code but is very nice for new code.
 */
void basic_mud_log(const char *format, ...) {
	va_list args;
	time_t ct = time(0);
	char *time_s = asctime(localtime(&ct));

	if (logfile == NULL) {
		puts("SYSERR: Using log() before stream was initialized!");
	}
	if (format == NULL) {
		format = "SYSERR: log() received a NULL format.";
	}

	time_s[strlen(time_s) - 1] = '\0';

	fprintf(logfile, "%-20.20s :: ", time_s + 4);

	va_start(args, format);
	vfprintf(logfile, format, args);
	va_end(args);

	fprintf(logfile, "\n");
	fflush(logfile);
}


/**
* Shows a log to players in-game and also stores it to file, except for the
* ELOG_NONE/ELOG_LOGINS types, which are displayed but not stored.
*
* @param empire_data *emp Which empire to log to.
* @param int type ELOG_
* @param const char *str The va-arg format ...
*/
void log_to_empire(empire_data *emp, int type, const char *str, ...) {
	extern const bool show_empire_log_type[];
	
	struct empire_log_data *elog, *temp;
	char output[MAX_STRING_LENGTH];
	descriptor_data *i;
	va_list tArgList;

	if (!str || !*str) {
		return;
	}

	va_start(tArgList, str);
	vsprintf(output, str, tArgList);
	
	// save to file?
	if (type != ELOG_NONE && type != ELOG_LOGINS) {
		CREATE(elog, struct empire_log_data, 1);
		elog->type = type;
		elog->timestamp = time(0);
		elog->string = str_dup(output);
		elog->next = NULL;
		
		// append to end
		if ((temp = EMPIRE_LOGS(emp))) {
			while (temp->next) {
				temp = temp->next;
			}
			temp->next = elog;
		}
		else {
			EMPIRE_LOGS(emp) = elog;
		}
		
		EMPIRE_NEEDS_LOGS_SAVE(emp) = TRUE;
	}
	
	// show to players
	if (show_empire_log_type[type] == TRUE) {
		for (i = descriptor_list; i; i = i->next) {
			if (STATE(i) != CON_PLAYING || IS_NPC(i->character))
				continue;
			if (GET_LOYALTY(i->character) != emp)
				continue;

			stack_msg_to_desc(i, "%s[ %s ]&0\r\n", EMPIRE_BANNER(emp), output);
		}
	}
	
	va_end(tArgList);
}


void mortlog(const char *str, ...) {
	char output[MAX_STRING_LENGTH];
	descriptor_data *i;
	va_list tArgList;

	if (!str) {
		return;
	}

	va_start(tArgList, str);
	vsprintf(output, str, tArgList);

	for (i = descriptor_list; i; i = i->next) {
		if (STATE(i) == CON_PLAYING && i->character && PRF_FLAGGED(i->character, PRF_MORTLOG)) {
			stack_msg_to_desc(i, "&c[ %s ]&0\r\n", output);
		}
	}
	va_end(tArgList);
}


/**
* This creates a consistent room identifier for logs.
*
* @param room_data *room The location to log.
* @return char* The compiled string.
*/
char *room_log_identifier(room_data *room) {
	extern char *get_room_name(room_data *room, bool color);
	
	static char val[MAX_STRING_LENGTH];
	
	sprintf(val, "[%d] %s (%d, %d)", GET_ROOM_VNUM(room), get_room_name(room, FALSE), X_COORD(room), Y_COORD(room));
	return val;
}


/**
* This is the main syslog function (mudlog in CircleMUD). It logs to all
* immortals who are listening.
*
* @param bitvector_t type Any SYS_ type
* @param int level The minimum level to see the log.
* @param bool file If TRUE, also outputs to the mud's log file.
* @param const char *str The log string.
* @param ... printf-style args for str.
*/
void syslog(bitvector_t type, int level, bool file, const char *str, ...) {
	char output[MAX_STRING_LENGTH];
	descriptor_data *i;
	va_list tArgList;

	if (!str) {
		return;
	}

	va_start(tArgList, str);
	vsprintf(output, str, tArgList);

	if (file) {
		log("%s", output);
	}
	
	level = MAX(level, LVL_START_IMM);

	for (i = descriptor_list; i; i = i->next) {
		if (STATE(i) == CON_PLAYING && i->character && !IS_NPC(i->character) && GET_ACCESS_LEVEL(i->character) >= level) {
			if (IS_SET(SYSLOG_FLAGS(REAL_CHAR(i->character)), type)) {
				if (level > LVL_START_IMM) {
					stack_msg_to_desc(i, "&g[ (i%d) %s ]&0\r\n", level, output);
				}
				else {
					stack_msg_to_desc(i, "&g[ %s ]&0\r\n", output);
				}
			}
		}
	}
	
	va_end(tArgList);
}


 //////////////////////////////////////////////////////////////////////////////
//// ADVENTURE UTILS /////////////////////////////////////////////////////////

/**
* Looks for the adventure that contains this in its vnum range.
* 
* @param rmt_vnum vnum The vnum to look up.
* @return adv_data* An adventure, or NULL if no match.
*/
adv_data *get_adventure_for_vnum(rmt_vnum vnum) {
	adv_data *iter, *next_iter;
	
	HASH_ITER(hh, adventure_table, iter, next_iter) {
		if (vnum >= GET_ADV_START_VNUM(iter) && vnum <= GET_ADV_END_VNUM(iter)) {
			return iter;
		}
	}
	
	return NULL;
}


 //////////////////////////////////////////////////////////////////////////////
//// INTERPRETER UTILS ///////////////////////////////////////////////////////


/* same as one_argument except that it doesn't ignore fill words */
char *any_one_arg(char *argument, char *first_arg) {
	skip_spaces(&argument);

	while (*argument && !isspace(*argument)) {
		*(first_arg++) = *argument;
		argument++;
	}

	*first_arg = '\0';

	return (argument);
}


/* same as any_one_arg except that it takes "quoted strings" and (coord, pairs) as one arg */
char *any_one_word(char *argument, char *first_arg) {
	skip_spaces(&argument);

	if (*argument == '\"') {
		++argument;
		while (*argument && *argument != '\"') {
			*(first_arg++) = *argument;
			++argument;
		}
		if (*argument) {
			++argument;
		}
	}
	else if (*argument == '(') {
		++argument;
		while (*argument && *argument != ')') {
			*(first_arg++) = *argument;
			++argument;
		}
		if (*argument) {
			++argument;
		}
	
	}
	else {
		// copy to first space
		while (*argument && !isspace(*argument)) {
			*(first_arg++) = *argument;
			++argument;
		}
	}

	*first_arg = '\0';

	return (argument);
}


/**
* Function to separate a string into two comma-separated args. It is otherwise
* similar to half_chop().
*
* @param char *string The input string, e.g. "15, 2"
* @param char *arg1 A buffer to save the first argument to, or "" if there is none
* @param char *arg2 A buffer to save the second argument to, or ""
*/
void comma_args(char *string, char *arg1, char *arg2) {
	char *comma;
	int len;
	
	skip_spaces(&string);

	if ((comma = strchr(string, ',')) != NULL) {
		// trailing whitespace
		while (comma > string && isspace(*(comma-1))) {
			--comma;
		}
		len = comma - string;
		
		// arg1
		strncpy(arg1, string, len);
		arg1[len] = '\0';
		
		// arg2
		while (isspace(*comma) || *comma == ',') {
			++comma;
		}
		strcpy(arg2, comma);
	}
	else {
		// no comma
		strcpy(arg1, string);
		*arg2 = '\0';
	}
}


/**
* @param char *argument The argument to test.
* @return int TRUE if the argument is in the fill_words[] list.
*/
int fill_word(char *argument) {
	extern const char *fill_words[];
	return (search_block(argument, fill_words, TRUE) != NOTHING);
}


/* return first space-delimited token in arg1; remainder of string in arg2 */
void half_chop(char *string, char *arg1, char *arg2) {
	char *temp;

	temp = any_one_arg(string, arg1);
	skip_spaces(&temp);
	strcpy(arg2, temp);
}


/*
 * determine if a given string is an abbreviation of another
 * (now works symmetrically -- JE 7/25/94)
 *
 * that was dumb.  it shouldn't be symmetrical.  JE 5/1/95
 * 
 * returnss 1 if arg1 is an abbreviation of arg2
 */
int is_abbrev(const char *arg1, const char *arg2) {
	if (!*arg1)
		return (0);

	for (; *arg1 && *arg2; arg1++, arg2++)
		if (LOWER(*arg1) != LOWER(*arg2))
			return (0);

	if (!*arg1)
		return (1);
	else
		return (0);
}


/**
* similar to is_abbrev() but compares abbreviations of multiple words, e.g.
* "cl br" would be a multi-abbrev of "clay bricks".
*
* @param const char *arg player input
* @param const char *phrase phrase to match
* @return bool TRUE if words in arg are an abbreviation of phrase, FALSE otherwise
*/
bool is_multiword_abbrev(const char *arg, const char *phrase) {
	char *argptr, *phraseptr;
	char argword[256], phraseword[256];
	char argcpy[MAX_INPUT_LENGTH], phrasecpy[MAX_INPUT_LENGTH];
	bool ok;

	if (is_abbrev(arg, phrase)) {
		return TRUE;
	}
	else {
		strcpy(phrasecpy, phrase);
		strcpy(argcpy, arg);
		
		argptr = any_one_arg(argcpy, argword);
		phraseptr = any_one_arg(phrasecpy, phraseword);
		
		ok = TRUE;
		while (*argword && *phraseword && ok) {
			if (!is_abbrev(argword, phraseword)) {
				ok = FALSE;
			}
			argptr = any_one_arg(argptr, argword);
			phraseptr = any_one_arg(phraseptr, phraseword);
		}
		
		// if anything is left in argword, there was another word that didn't match
		return (ok && !*argword);
	}
}
 

/**
* @param const char *str The string to check.
* @return int TRUE if str is a number, FALSE if it contains anything else.
*/
int is_number(const char *str) {
	if (!*str) {
		return (0);
	}
	
	// allow leading negative
	if (*str == '-') {
		str++;
	}
	
	while (*str)
		if (!isdigit(*(str++)))
			return (0);

	return (1);
}


/*
 * copy the first non-fill-word, space-delimited argument of 'argument'
 * to 'first_arg'; return a pointer to the remainder of the string.
 */
char *one_argument(char *argument, char *first_arg) {
	char *begin = first_arg;

	if (!argument) {
		log("SYSERR: one_argument received a NULL pointer!");
		*first_arg = '\0';
		return (NULL);
	}

	do {
		skip_spaces(&argument);

		first_arg = begin;
		while (*argument && !isspace(*argument)) {
			*(first_arg++) = *argument;
			argument++;
		}

		*first_arg = '\0';
	} while (fill_word(begin));

	return (argument);
}


/*
 * one_word is like one_argument, except that words in quotes ("") or parens
 * () are considered one word.
 */
char *one_word(char *argument, char *first_arg) {
	char *begin = first_arg;

	do {
		skip_spaces(&argument);

		first_arg = begin;

		if (*argument == '\"') {
			argument++;
			while (*argument && *argument != '\"') {
				*(first_arg++) = *argument;
				argument++;
			}
			argument++;
		}
		else if (*argument == '(') {
			argument++;
			while (*argument && *argument != ')') {
				*(first_arg++) = *argument;
				argument++;
			}
			argument++;
		}
		else {
			while (*argument && !isspace(*argument)) {
				*(first_arg++) = *argument;
				argument++;
			}
		}

		*first_arg = '\0';
	} while (fill_word(begin));

	return (argument);
}


/**
* @param char *argument The argument to test.
* @return int TRUE if the argument is in the reserved_words[] list.
*/
int reserved_word(char *argument) {
	extern const char *reserved_words[];
	return (search_block(argument, reserved_words, TRUE) != NOTHING);
}


/**
* searches an array of strings for a target string.  "exact" can be
* 0 or non-0, depending on whether or not the match must be exact for
* it to be returned.  Returns NOTHING if not found; 0..n otherwise.  Array
* must be terminated with a '\n' so it knows to stop searching.
*
* As of b5.57, This function will prefer exact matches over partial matches,
* even without 'exact'.
*
* @param char *arg The input.
* @param const char **list A "\n"-terminated name list.
* @param int exact 0 = abbrevs, 1 = full match
*/
int search_block(char *arg, const char **list, int exact) {
	register int i, l, part = NOTHING;

	if (!*arg) {
		return NOTHING;	// shortcut
	}

	l = strlen(arg);

	for (i = 0; **(list + i) != '\n'; i++) {
		if (!str_cmp(arg, *(list + i))) {
			return i;	// exact or otherwise
		}
		else if (!exact && part == NOTHING && !strn_cmp(arg, *(list + i), l)) {
			part = i;	// found partial but keep searching
		}
	}

	return part;	// if any
}


/*
 * Function to skip over the leading spaces of a string.
 */
void skip_spaces(char **string) {
	for (; **string && isspace(**string); (*string)++);
}


/*
 * Same as one_argument except that it takes two args and returns the rest;
 * ignores fill words
 */
char *two_arguments(char *argument, char *first_arg, char *second_arg) {
	return (one_argument(one_argument(argument, first_arg), second_arg));
}


 //////////////////////////////////////////////////////////////////////////////
//// MOBILE UTILS ////////////////////////////////////////////////////////////

/**
* Despawns all familiars and charmies a player has. This is usually called upon
* player death.
*
* @param char_data *ch The person whose followers to despawn.
*/
void despawn_charmies(char_data *ch) {
	char_data *iter, *next_iter;
	
	for (iter = character_list; iter; iter = next_iter) {
		next_iter = iter->next;
		
		if (IS_NPC(iter) && iter->master == ch) {
			if (MOB_FLAGGED(iter, MOB_FAMILIAR) || AFF_FLAGGED(iter, AFF_CHARM)) {
				act("$n leaves.", TRUE, iter, NULL, NULL, TO_ROOM);
				extract_char(iter);
			}
		}
	}
}


/**
* Quick way to turn a vnum into a name, safely.
*
* @param mob_vnum vnum The vnum to look up.
* @return char* A name for the vnum, or "UNKNOWN".
*/
char *get_mob_name_by_proto(mob_vnum vnum) {
	char_data *proto = mob_proto(vnum);
	char *unk = "UNKNOWN";
	
	return proto ? GET_SHORT_DESC(proto) : unk;
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT UTILS ////////////////////////////////////////////////////////////

/**
* Quick way to turn a vnum into a name, safely.
*
* @param obj_vnum vnum The vnum to look up.
* @return char* A name for the vnum, or "UNKNOWN".
*/
char *get_obj_name_by_proto(obj_vnum vnum) {
	obj_data *proto = obj_proto(vnum);
	char *unk = "UNKNOWN";
	
	return proto ? GET_OBJ_SHORT_DESC(proto) : unk;
}


/**
* @param obj_data *obj Any object
* @return obj_data *the "top object" -- the outer-most container (or obj if it wasn't in one)
*/
obj_data *get_top_object(obj_data *obj) {
	obj_data *top = obj;
	
	while (top->in_obj) {
		top = top->in_obj;
	}
	
	return top;
}


/**
* Determines a rudimentary comparative value for an item
*
* @param obj_data *obj the item to compare
* @return double a score
*/
double rate_item(obj_data *obj) {
	extern double get_base_dps(obj_data *weapon);
	extern const double apply_values[];
	
	struct obj_apply *apply;
	double score = 0;
	
	// basic apply score
	for (apply = GET_OBJ_APPLIES(obj); apply; apply = apply->next) {
		score += apply->modifier * apply_values[(int) apply->location];
	}
	
	// score based on type
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_WEAPON: {
			score += get_base_dps(obj);
			break;
		}
		case ITEM_DRINKCON: {
			score += GET_DRINK_CONTAINER_CAPACITY(obj) / config_get_double("scale_drink_capacity");
			break;
		}
		case ITEM_COINS: {
			score += GET_COINS_AMOUNT(obj) / config_get_double("scale_coin_amount");
			break;
		}
		case ITEM_MISSILE_WEAPON: {
			score += get_base_dps(obj);
			break;
		}
		case ITEM_AMMO: {
			score += GET_AMMO_DAMAGE_BONUS(obj);
			break;
		}
		case ITEM_PACK: {
			score += GET_PACK_CAPACITY(obj) / config_get_double("scale_pack_size");
			break;
		}	
	}

	return score;
}


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER UTILS ////////////////////////////////////////////////////////////

/**
* Gives a character the appropriate amount of command lag (wait time).
*
* @param char_data *ch The person to give command lag to.
* @param int wait_type A WAIT_ const to help determine wait time.
*/
void command_lag(char_data *ch, int wait_type) {
	extern const int universal_wait;
	
	double val;
	int wait;
	
	// short-circuit
	if (wait_type == WAIT_NONE) {
		return;
	}
	
	// base
	wait = universal_wait;
	
	switch (wait_type) {
		case WAIT_SPELL: {	// spells (but not combat spells)
			if (has_player_tech(ch, PTECH_FASTCASTING)) {
				val = 0.3333 * GET_WITS(ch);
				wait -= MAX(0, val) RL_SEC;
				
				// ensure minimum of 0.5 seconds
				wait = MAX(wait, 0.5 RL_SEC);
			}
			break;
		}
		case WAIT_MOVEMENT: {	// normal movement (special handling)
			if (AFF_FLAGGED(ch, AFF_SLOW)) {
				wait = 1 RL_SEC;
			}
			else if (IS_RIDING(ch) || IS_ROAD(IN_ROOM(ch))) {
				wait = 0;	// no wait on riding/road
			}
			else if (IS_OUTDOORS(ch)) {
				if (HAS_BONUS_TRAIT(ch, BONUS_FASTER)) {
					wait = 0.25 RL_SEC;
				}
				else {
					wait = 0.5 RL_SEC;
				}
			}
			else {
				wait = 0;	// indoors
			}
			break;
		}
	}
	
	if (GET_WAIT_STATE(ch) < wait) {
		GET_WAIT_STATE(ch) = wait;
	}
}


/**
* Calculates a player's gear level, based on what they have equipped.
*
* @param char_data *ch The player to set gear level for.
*/
void determine_gear_level(char_data *ch) {
	extern const struct wear_data_type wear_data[NUM_WEARS];

	double total, slots;
	int avg, level, pos;
	
	// sanity check: we really have no work to do for mobs
	if (IS_NPC(ch)) {
		return;
	}
	
	level = total = slots = 0;	// init
	
	for (pos = 0; pos < NUM_WEARS; ++pos) {
		// some items count as more or less than 1 slot
		slots += wear_data[pos].gear_level_mod;
		
		if (GET_EQ(ch, pos) && wear_data[pos].gear_level_mod > 0) {
			total += GET_OBJ_CURRENT_SCALE_LEVEL(GET_EQ(ch, pos)) * wear_data[pos].gear_level_mod;
			
			// bonuses
			if (OBJ_FLAGGED(GET_EQ(ch, pos), OBJ_SUPERIOR)) {
				total += 10 * wear_data[pos].gear_level_mod;
			}
			if (OBJ_FLAGGED(GET_EQ(ch, pos), OBJ_ENCHANTED)) {
				total += 10 * wear_data[pos].gear_level_mod;
			}
			if (OBJ_FLAGGED(GET_EQ(ch, pos), OBJ_HARD_DROP)) {
				total += 10 * wear_data[pos].gear_level_mod;
			}
			if (OBJ_FLAGGED(GET_EQ(ch, pos), OBJ_GROUP_DROP)) {
				total += 10 * wear_data[pos].gear_level_mod;
			}
		}
	}
	
	// determine average gear level of the player's slots
	avg = round(total / slots);
	
	// player's gear level (which will be added to skill level) is:
	level = avg + 50 - 100;	// 50 higher than the average scaled level of their gear, -100 to compensate for skill level
	
	GET_GEAR_LEVEL(ch) = MAX(level, 0);
}


/**
* Finds an attribute by name, preferring exact matches to partial matches.
*
* @param char *name The name to look up.
* @return int An attribute constant (STRENGTH) or -1 for no-match.
*/
int get_attribute_by_name(char *name) {
	extern struct attribute_data_type attributes[NUM_ATTRIBUTES];
	int iter, partial = -1;
	
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		if (!str_cmp(name, attributes[iter].name)) {
			return iter;	// exact matcg;
		}
		else if (is_abbrev(name, attributes[iter].name)) {
			partial = iter;
		}
	}
	
	// didn't find exact...
	return partial;
}


/**
* Picks a level based on a min/max and base.
*
* @param int level The base level (usually player's level).
* @param int min Optional: A minimum level to pick (0 for no-minimum).
* @param int max Optional: A maximum level to pick (0 for no-maximum).
* @return int The selected level.
*/
int pick_level_from_range(int level, int min, int max) {
	if (min > 0) {
		level = MAX(level, min);
	}
	if (max > 0) {
		level = MIN(level, max);
	}
	return level;
}


/**
* Raises a person from sleeping+ to standing (or fighting) if possible.
* 
* @param char_data *ch The person to try to wake/stand.
* @return bool TRUE if the character ended up standing (>= fighting), FALSE if not.
*/
bool wake_and_stand(char_data *ch) {
	void do_unseat_from_vehicle(char_data *ch);
	char buf[MAX_STRING_LENGTH];
	bool was_sleeping = FALSE;
	
	switch (GET_POS(ch)) {
		case POS_SLEEPING: {
			was_sleeping = TRUE;
			// no break -- drop through
		}
		case POS_RESTING:
		case POS_SITTING: {
			do_unseat_from_vehicle(ch);
			GET_POS(ch) = POS_STANDING;
			msg_to_char(ch, "You %sget up.\r\n", (was_sleeping ? "awaken and " : ""));
			snprintf(buf, sizeof(buf), "$n %sgets up.", (was_sleeping ? "awakens and " : ""));
			act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
			// no break -- drop through
		}
		case POS_FIGHTING:
		case POS_STANDING: {
			// at this point definitely standing, or close enough
			return TRUE;
		}
		default: {
			// can't do anything with any other pos
			return FALSE;
		}
	}
	
	return FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// RESOURCE UTILS //////////////////////////////////////////////////////////

/**
* Combines some resource data into an existing resource list, combining if
* possible.
*
* @param struct resource_data **list The list to add to.
* @param int type The RES_ type.
* @param any_vnum vnum The appropriate vnum for 'type'.
* @param int amount How many/much of that thing.
* @param int misc For types that use the misc field.
*/
void add_to_resource_list(struct resource_data **list, int type, any_vnum vnum, int amount, int misc) {
	struct resource_data *res;
	bool found = FALSE;
	
	// prefer to combine
	LL_FOREACH(*list, res) {
		if (res->type == type && res->vnum == vnum && res->misc == misc) {
			SAFE_ADD(res->amount, amount, INT_MIN, INT_MAX, FALSE);
			found = TRUE;
		}
	}
	
	// append if not found
	if (!found) {
		CREATE(res, struct resource_data, 1);
		res->type = type;
		res->vnum = vnum;
		res->amount = amount;
		res->misc = misc;
		
		LL_APPEND(*list, res);
	}
}


/**
* Applies a previously found resource_data (and sometimes matching obj) to a
* building or other one-at-a-time craft. This is called using the resource and
* found object from get_next_resource().
*
* @param char_data *ch The player doing the work.
* @param struct resource_data *res The resource entry they are applying (may be freed!).
* @param struct resource_data **list A pointer to the resource list they are working on (res may be removed from it).
* @param obj_data *use_obj Optional: If an item was found, pass it here -- it will probably be extracted.
* @param int msg_type APPLY_RES_BUILD, etc -- for how to message.
* @param vehicle_data *crafting_veh Optional: used for APPLY_RES_CRAFT messages.
* @param struct resource_data **build_used_list Optional: If you need to track the actual resources used, pass a pointer to that list.
*/
void apply_resource(char_data *ch, struct resource_data *res, struct resource_data **list, obj_data *use_obj, int msg_type, vehicle_data *crafting_veh, struct resource_data **build_used_list) {
	bool messaged_char = FALSE, messaged_room = FALSE;
	char buf[MAX_STRING_LENGTH];
	int amt;
	
	if (!ch || !res) {
		return;	// nothing to do
	}
	
	// APPLY_RES_x: send custom messages (only) now
	if (use_obj) {
		switch (msg_type) {
			case APPLY_RES_BUILD: {
				if (!messaged_char && obj_has_custom_message(use_obj, OBJ_CUSTOM_BUILD_TO_CHAR)) {
					act(obj_get_custom_message(use_obj, OBJ_CUSTOM_BUILD_TO_CHAR), FALSE, ch, use_obj, NULL, TO_CHAR | TO_SPAMMY);
					messaged_char = TRUE;
				}		
				if (!messaged_room && obj_has_custom_message(use_obj, OBJ_CUSTOM_BUILD_TO_ROOM)) {
					act(obj_get_custom_message(use_obj, OBJ_CUSTOM_BUILD_TO_ROOM), FALSE, ch, use_obj, NULL, TO_ROOM | TO_SPAMMY);
					messaged_room = TRUE;
				}
				break;
			}
			case APPLY_RES_CRAFT: {
				if (!messaged_char && obj_has_custom_message(use_obj, OBJ_CUSTOM_CRAFT_TO_CHAR)) {
					act(obj_get_custom_message(use_obj, OBJ_CUSTOM_CRAFT_TO_CHAR), FALSE, ch, use_obj, crafting_veh, TO_CHAR | TO_SPAMMY);
					messaged_char = TRUE;
				}
				if (!messaged_room && obj_has_custom_message(use_obj, OBJ_CUSTOM_CRAFT_TO_ROOM)) {
					act(obj_get_custom_message(use_obj, OBJ_CUSTOM_CRAFT_TO_ROOM), FALSE, ch, use_obj, crafting_veh, TO_ROOM | TO_SPAMMY);
					messaged_room = TRUE;
				}
				break;
			}
			case APPLY_RES_REPAIR: {
				if (!messaged_char) {
					act("You use $p to repair $V.", FALSE, ch, use_obj, crafting_veh, TO_CHAR | TO_SPAMMY);
					messaged_char = TRUE;
				}
				if (!messaged_room) {
					act("$n uses $p to repair $V.", FALSE, ch, use_obj, crafting_veh, TO_ROOM | TO_SPAMMY);
					messaged_room = TRUE;
				}
				break;
			}
		}
	}
	
	// RES_x: Remove one resource based on type.
	switch (res->type) {
		case RES_OBJECT:	// obj/component are just simple objs
		case RES_COMPONENT: {
			// APPLY_RES_x: generic messaging if not previously sent
			switch (msg_type) {
				case APPLY_RES_BUILD: {
					if (!messaged_char) {
						act("You carefully place $p in the structure.", FALSE, ch, use_obj, NULL, TO_CHAR | TO_SPAMMY);
					}
					if (!messaged_room) {
						act("$n places $p carefully in the structure.", FALSE, ch, use_obj, NULL, TO_ROOM | TO_SPAMMY);
					}
					break;
				}
				case APPLY_RES_CRAFT: {
					if (!messaged_char) {
						act("You place $p onto $V.", FALSE, ch, use_obj, crafting_veh, TO_CHAR | TO_SPAMMY);
					}
					if (!messaged_room) {
						act("$n places $p onto $V.", FALSE, ch, use_obj, crafting_veh, TO_ROOM | TO_SPAMMY);
					}
					break;
				}
				// already messaged APPLY_RES_REPAIR for this type
			}
			
			res->amount -= 1;	// only pay 1 at a time
			if (use_obj) {
				if (build_used_list) {
					add_to_resource_list(build_used_list, RES_OBJECT, GET_OBJ_VNUM(use_obj), 1, GET_OBJ_CURRENT_SCALE_LEVEL(use_obj));
				}
				empty_obj_before_extract(use_obj);
				extract_obj(use_obj);
			}
			break;
		}
		case RES_LIQUID: {
			// APPLY_RES_x: generic messaging if not previously sent
			switch (msg_type) {
				case APPLY_RES_BUILD: {
					if (!messaged_char) {
						act("You carefully pour $p into the structure.", FALSE, ch, use_obj, NULL, TO_CHAR | TO_SPAMMY);
					}
					if (!messaged_room) {
						act("$n pours $p carefully into the structure.", FALSE, ch, use_obj, NULL, TO_ROOM | TO_SPAMMY);
					}
					break;
				}
				case APPLY_RES_CRAFT: {
					if (!messaged_char) {
						act("You pour $p into $V.", FALSE, ch, use_obj, crafting_veh, TO_CHAR | TO_SPAMMY);
					}
					if (!messaged_room) {
						act("$n pours $p into $V.", FALSE, ch, use_obj, crafting_veh, TO_ROOM | TO_SPAMMY);
					}
					break;
				}
				// already messaged APPLY_RES_REPAIR for this type
			}
			
			if (use_obj) {
				amt = GET_DRINK_CONTAINER_CONTENTS(use_obj);
				amt = MIN(res->amount, amt);
				
				res->amount -= amt;	// possible partial payment
				GET_OBJ_VAL(use_obj, VAL_DRINK_CONTAINER_CONTENTS) -= amt;
				if (GET_DRINK_CONTAINER_CONTENTS(use_obj) == 0) {
					GET_OBJ_VAL(use_obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
				}
				
				if (build_used_list) {
					add_to_resource_list(build_used_list, RES_LIQUID, res->vnum, amt, 0);
				}
			}
			break;
		}
		case RES_COINS: {
			empire_data *coin_emp = real_empire(res->vnum);
			
			if (!messaged_char && msg_type != APPLY_RES_SILENT) {
				snprintf(buf, sizeof(buf), "You spend %s.", money_amount(coin_emp, res->amount));
				act(buf, FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
			}
			if (!messaged_room && msg_type != APPLY_RES_SILENT) {
				snprintf(buf, sizeof(buf), "$n spends %s.", money_amount(coin_emp, res->amount));
				act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
			}
			
			charge_coins(ch, coin_emp, res->amount, build_used_list);
			res->amount = 0;	// cost paid in full
			break;
		}
		case RES_CURRENCY: {
			if (!messaged_char && msg_type != APPLY_RES_SILENT) {
				snprintf(buf, sizeof(buf), "You spend %d %s.", res->amount, get_generic_string_by_vnum(res->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(res->amount)));
				act(buf, FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
			}
			if (!messaged_room && msg_type != APPLY_RES_SILENT) {
				snprintf(buf, sizeof(buf), "$n spends %d %s.", res->amount, get_generic_string_by_vnum(res->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(res->amount)));
				act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
			}
			add_currency(ch, res->vnum, -(res->amount));
			res->amount = 0;	// paid all at once
			break;
		}
		case RES_POOL: {
			if (!messaged_char && msg_type != APPLY_RES_SILENT) {
				snprintf(buf, sizeof(buf), "You spend %d %s point%s.", res->amount, pool_types[res->vnum], PLURAL(res->amount));
				act(buf, FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
			}
			if (!messaged_room && msg_type != APPLY_RES_SILENT) {
				snprintf(buf, sizeof(buf), "$n spends %s %s point%s.", (res->amount != 1) ? "some" : "a", pool_types[res->vnum], PLURAL(res->amount));
				act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
			}
			
			if (build_used_list) {
				add_to_resource_list(build_used_list, RES_POOL, res->vnum, res->amount, 0);
			}
			
			GET_CURRENT_POOL(ch, res->vnum) -= res->amount;
			GET_CURRENT_POOL(ch, res->vnum) = MAX(0, GET_CURRENT_POOL(ch, res->vnum));
			
			if (res->vnum == HEALTH) {
				update_pos(ch);
				send_char_pos(ch, 0);
			}
			
			res->amount = 0;	// cost paid in full
			break;
		}
		case RES_ACTION: {
			generic_data *gen = find_generic(res->vnum, GENERIC_ACTION);
			
			switch (msg_type) {
				case APPLY_RES_BUILD: {
					if (!messaged_char && gen && GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR)) {
						act(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR), FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
					}
					if (!messaged_room && gen && GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM)) {
						act(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM), FALSE, ch, use_obj, NULL, TO_ROOM | TO_SPAMMY);
					}
					break;
				}
				case APPLY_RES_CRAFT: {
					if (!messaged_char && gen && GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR)) {
						act(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR), FALSE, ch, NULL, crafting_veh, TO_CHAR | TO_SPAMMY);
					}
					if (!messaged_room && gen && GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM)) {
						act(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM), FALSE, ch, NULL, crafting_veh, TO_ROOM | TO_SPAMMY);
					}
					break;
				}
				case APPLY_RES_REPAIR: {
					if (!messaged_char && gen && GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR)) {
						act(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR), FALSE, ch, NULL, crafting_veh, TO_CHAR | TO_SPAMMY);
					}
					if (!messaged_room && gen && GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM)) {
						act(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM), FALSE, ch, NULL, crafting_veh, TO_ROOM | TO_SPAMMY);
					}
					break;
				}
			}
			
			res->amount -= 1;	// only 1 at a time
			break;
		}
	}
	
	// now possibly remove the resource, if it hit zero
	if (res->amount <= 0) {
		if (list) {
			LL_DELETE(*list, res);
		}
		free(res);
	}
}


/**
* The name of a component with its flags.
*
* @param int type CMP_ component type.
* @param bitvector_t flags CMPF_ component flags.
*/
char *component_string(int type, bitvector_t flags) {
	extern const char *component_flags[];
	extern const char *component_types[];
	
	char mods[MAX_STRING_LENGTH];
	static char output[256];
	
	if (flags) {
		prettier_sprintbit(flags, component_flags, mods);
		strcat(mods, " ");
	}
	else {
		*mods = '\0';
	}
	snprintf(output, sizeof(output), "%s%s", mods, component_types[type]);
	return output;
}


/**
* Extract resources from the list, hopefully having checked has_resources, as
* this function does not error if it runs out -- it just keeps extracting
* until it's out of items, or hits its required limit.
*
* This function always takes from inventory first, and ground second.
*
* @param char_data *ch The person whose resources to take.
* @param struct resource_data *list Any resource list.
* @param bool ground If TRUE, will also take resources off the ground.
* @param struct resource_data **build_used_list Optional: if not NULL, will build a resource list of the specifc things extracted.
*/
void extract_resources(char_data *ch, struct resource_data *list, bool ground, struct resource_data **build_used_list) {
	int diff, liter, remaining, cycle;
	obj_data *obj, *next_obj;
	struct resource_data *res;
	
	// This is done in 2 phases (to ensure specific objs are used before components):
	#define EXRES_OBJS  (cycle == 0)	// check/mark specific objs
	#define EXRES_OTHER  (cycle == 1)	// check/mark components
	#define NUM_EXRES_CYCLES  2
	
	for (cycle = 0; cycle < NUM_EXRES_CYCLES; ++cycle) {
		LL_FOREACH(list, res) {
			// only RES_OBJECT is checked in the first cycle
			if (EXRES_OBJS && res->type != RES_OBJECT) {
				continue;
			}
			else if (EXRES_OTHER && res->type == RES_OBJECT) {
				continue;
			}
		
			// RES_x: extract resources by type
			switch (res->type) {
				case RES_OBJECT:
				case RES_COMPONENT:
				case RES_LIQUID: {	// these 3 types check objects
					obj_data *search_list[2];
					search_list[0] = ch->carrying;
					search_list[1] = ground ? ROOM_CONTENTS(IN_ROOM(ch)) : NULL;
				
					remaining = res->amount;
				
					// up to two places to search
					for (liter = 0; liter < 2 && remaining > 0; ++liter) {
						LL_FOREACH_SAFE2(search_list[liter], obj, next_obj, next_content) {
							// skip keeps
							if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
								continue;
							}
							if (!CAN_SEE_OBJ(ch, obj)) {
								continue;
							}
						
							// RES_x: just types that need objects
							switch (res->type) {
								case RES_OBJECT: {
									if (GET_OBJ_VNUM(obj) == res->vnum) {
										if (build_used_list) {
											add_to_resource_list(build_used_list, RES_OBJECT, GET_OBJ_VNUM(obj), 1, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
										}
									
										--remaining;
										extract_obj(obj);
									}
									break;
								}
								case RES_COMPONENT: {
									// require full match on cmp flags
									if (GET_OBJ_CMP_TYPE(obj) == res->vnum && (GET_OBJ_CMP_FLAGS(obj) & res->misc) == res->misc) {
										if (build_used_list) {
											add_to_resource_list(build_used_list, RES_OBJECT, GET_OBJ_VNUM(obj), 1, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
										}
									
										--remaining;
										extract_obj(obj);
									}
									break;
								}
								case RES_LIQUID: {
									if (IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == res->vnum) {
										diff = MIN(remaining, GET_DRINK_CONTAINER_CONTENTS(obj));
										remaining -= diff;
										GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) -= diff;
									
										if (GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) == 0) {
											GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
										}
									
										if (build_used_list) {
											add_to_resource_list(build_used_list, RES_LIQUID, res->vnum, diff, 0);
										}
									}
									break;
								}
							}
						
							// ok to break out early if we found enough
							if (remaining <= 0) {
								break;
							}
						}
					}
					break;
				}
				case RES_COINS: {
					charge_coins(ch, real_empire(res->vnum), res->amount, build_used_list);
					break;
				}
				case RES_CURRENCY: {
					add_currency(ch, res->vnum, -(res->amount));
					break;
				}
				case RES_POOL: {
					if (build_used_list) {
						add_to_resource_list(build_used_list, RES_POOL, res->vnum, res->amount, 0);
					}
				
					GET_CURRENT_POOL(ch, res->vnum) -= res->amount;
					GET_CURRENT_POOL(ch, res->vnum) = MAX(0, GET_CURRENT_POOL(ch, res->vnum));
				
					if (res->vnum == HEALTH) {
						update_pos(ch);
						send_char_pos(ch, 0);
					}
					break;
				}
				case RES_ACTION: {
					// nothing to do
					break;
				}
			}
		}
	}
}


/**
* Finds the next resource a character has available, from a list. This can be
* used to get the next <thing> for any one-at-a-time building task, including
* vehicles. This function returns the resource_data pointer, and will bind the
* object it found (if applicable to the resource type), but it doesn't extract
* or free anything.
*
* Apply the found resource using apply_resource().
*
* @param char_data *ch The person who is doing the building.
* @param struct resource_data *list The resource list he's working on.
* @param bool ground TRUE if the character can use items off the ground (in addition to inventory).
* @param bool left2right If TRUE, requires the leftmost resource first (blocks progress).
* @param obj_data **found_obj A variable to bind the matching item to, for resources that require items.
*/
struct resource_data *get_next_resource(char_data *ch, struct resource_data *list, bool ground, bool left2right, obj_data **found_obj) {
	struct resource_data *res;
	int liter, amt;
	obj_data *obj;
	
	*found_obj = NULL;
	
	LL_FOREACH(list, res) {
		// RES_x: find first resource by type
		switch (res->type) {
			case RES_OBJECT:
			case RES_COMPONENT:
			case RES_LIQUID: {	// these 3 types check objects
				obj_data *search_list[2];
				search_list[0] = ch->carrying;
				search_list[1] = ground ? ROOM_CONTENTS(IN_ROOM(ch)) : NULL;
				
				// up to two places to search
				for (liter = 0; liter < 2; ++liter) {
					LL_FOREACH2(search_list[liter], obj, next_content) {
						// skip keeps
						if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
							continue;
						}
						if (!CAN_SEE_OBJ(ch, obj)) {
							continue;
						}
						
						// RES_x: just types that need objects
						switch (res->type) {
							case RES_OBJECT: {
								if (GET_OBJ_VNUM(obj) == res->vnum) {
									// got one!
									*found_obj = obj;
									return res;
								}
								break;
							}
							case RES_COMPONENT: {
								// require full match on cmp flags
								if (GET_OBJ_CMP_TYPE(obj) == res->vnum && (GET_OBJ_CMP_FLAGS(obj) & res->misc) == res->misc) {
									// got one!
									*found_obj = obj;
									return res;
								}
								break;
							}
							case RES_LIQUID: {
								if (IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == res->vnum && GET_DRINK_CONTAINER_CONTENTS(obj) > 0) {
									// got one!
									*found_obj = obj;
									return res;
								}
								break;
							}
						}
					}
				}
				break;
			}
			case RES_COINS: {
				if (can_afford_coins(ch, real_empire(res->vnum), res->amount)) {
					// success!
					return res;
				}
				break;
			}
			case RES_CURRENCY: {
				if (get_currency(ch, res->vnum) >= res->amount) {
					return res;
				}
				break;
			}
			case RES_POOL: {
				// special rule: require that blood or health costs not reduce player below 1
				amt = res->amount + ((res->vnum == HEALTH || res->vnum == BLOOD) ? 1 : 0);
	
				// more player checks
				if (GET_CURRENT_POOL(ch, res->vnum) >= amt) {
					// success!
					return res;
				}
				break;
			}
			case RES_ACTION: {
				// always good
				return res;
			}
		}
		
		// if they requested left-to-right, we never pass the first resource in the list
		if (left2right) {
			break;
		}
	}
	
	// found nothing
	return NULL;
}


/**
* Gets string output describing one "resource" entry, for use in lists.
*
* @param sturct resource_data *res The resource to display.
* @return char* A short string including quantity and type.
*/
char *get_resource_name(struct resource_data *res) {	
	static char output[MAX_STRING_LENGTH];
	
	*output = '\0';
	
	// RES_x: resource type determines display
	switch (res->type) {
		case RES_OBJECT: {
			snprintf(output, sizeof(output), "%dx %s", res->amount, skip_filler(get_obj_name_by_proto(res->vnum)));
			break;
		}
		case RES_COMPONENT: {
			snprintf(output, sizeof(output), "%dx (%s)", res->amount, component_string(res->vnum, res->misc));
			break;
		}
		case RES_LIQUID: {
			snprintf(output, sizeof(output), "%d unit%s of %s", res->amount, PLURAL(res->amount), get_generic_string_by_vnum(res->vnum, GENERIC_LIQUID, GSTR_LIQUID_NAME));
			break;
		}
		case RES_COINS: {
			snprintf(output, sizeof(output), "%s", money_amount(real_empire(res->vnum), res->amount));
			break;
		}
		case RES_CURRENCY: {
			snprintf(output, sizeof(output), "%d %s", res->amount, get_generic_string_by_vnum(res->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(res->amount)));
			break;
		}
		case RES_POOL: {
			snprintf(output, sizeof(output), "%d %s point%s", res->amount, pool_types[res->vnum], PLURAL(res->amount));
			break;
		}
		case RES_ACTION: {
			snprintf(output, sizeof(output), "%dx [%s]", res->amount, get_generic_name_by_vnum(res->vnum));
			break;
		}
		default: {
			snprintf(output, sizeof(output), "[unknown resource %d]", res->type);
			break;
		}
	}
	
	return output;
}


/**
* Give resources from a resource list to a player.
*
* @param char_data *ch The target player.
* @param struct resource_data *list Any resource list.
* @param bool split If TRUE, gives back only half.
*/
void give_resources(char_data *ch, struct resource_data *list, bool split) {
	int diff, liter, remaining;
	struct resource_data *res;
	bool last = FALSE;
	obj_data *obj;
	
	LL_FOREACH(list, res) {
		// RES_x: give resources by type
		switch (res->type) {
			case RES_COMPONENT:
			case RES_ACTION: {
				// nothing to do -- we can't refund these
				break;
			}
			case RES_OBJECT: {
				for (remaining = res->amount; remaining > 0; --remaining) {
					// skip every-other-thing
					if (last && split) {
						last = FALSE;
						continue;
					}
					last = TRUE;
					
					// replace the obj
					obj = read_object(res->vnum, TRUE);
					
					// scale item to "misc" level if applicable (will be 0/minimum if not)
					scale_item_to_level(obj, res->misc);
					
					if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
						obj_to_char(obj, ch);
					}
					else {
						obj_to_room(obj, IN_ROOM(ch));
					}
					
					load_otrigger(obj);
				}
				break;
			}
			case RES_LIQUID: {
				obj_data *search_list[2] = { ch->carrying, ROOM_CONTENTS(IN_ROOM(ch)) };
				remaining = res->amount / (split ? 2 : 1);
				last = FALSE;	// cause next obj to refund
				
				// up to two places to search for containers to fill
				for (liter = 0; liter < 2 && remaining > 0; ++liter) {
					LL_FOREACH2(search_list[liter], obj, next_content) {
						if (IS_DRINK_CONTAINER(obj) && (GET_DRINK_CONTAINER_TYPE(obj) == res->vnum || GET_DRINK_CONTAINER_CONTENTS(obj) == 0)) {
							diff = GET_DRINK_CONTAINER_CAPACITY(obj) - GET_DRINK_CONTAINER_CONTENTS(obj);
							diff = MIN(remaining, diff);
							remaining -= diff;
							GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) += diff;
							
							// set type in case container was empty
							if (GET_DRINK_CONTAINER_CONTENTS(obj) > 0) {
								GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = res->vnum;
							}
						}
						
						// ok to break out early if we gave enough
						if (remaining <= 0) {
							break;
						}
					}
				}
				break;
			}
			case RES_COINS: {
				increase_coins(ch, real_empire(res->vnum), res->amount / (split ? 2 : 1));
				last = FALSE;	// cause next obj to refund
				break;
			}
			case RES_CURRENCY: {
				add_currency(ch, res->vnum, res->amount);
				last = FALSE;	// cause next obj to refund
				break;
			}
			case RES_POOL: {
				GET_CURRENT_POOL(ch, res->vnum) += res->amount / (split ? 2 : 1);
				GET_CURRENT_POOL(ch, res->vnum) = MIN(GET_MAX_POOL(ch, res->vnum), GET_CURRENT_POOL(ch, res->vnum));
				if (GET_HEALTH(ch) > 0 && GET_POS(ch) <= POS_STUNNED) {
					GET_POS(ch) = POS_RESTING;
				}
				last = FALSE;	// cause next obj to refund
				break;
			}
		}
	}
}


/**
* Cuts a list of resources in half, for things that only return half. Empty
* entries (half=0) will be removed.
*
* @param struct resource_data **list The list to halve.
* @param bool remove_nonrefundables If TRUE, removes entries that cannot be refunded.
*/
void halve_resource_list(struct resource_data **list, bool remove_nonrefundables) {
	struct resource_data *res, *next_res;
	bool odd = FALSE;
	
	LL_FOREACH_SAFE(*list, res, next_res) {
		if (odd && (res->amount % 2) != 0) {
			// a previous one was odd (round up this time)
			res->amount = (res->amount + 1) / 2;
			odd = FALSE;
		}
		else if (odd) {
			// we have a stored odd but this one isn't odd too, so we keep the stored odd
			res->amount /= 2;
		}
		else {
			// no stored odd -- do a normal round-down divide
			odd = (res->amount % 2);
			res->amount /= 2;
		}
		
		// check for no remaining resource
		if (res->amount <= 0) {
			LL_DELETE(*list, res);
			free(res);
		}
	}
}


/**
* Find out if a person has resources available.
*
* @param char_data *ch The person whose resources to check.
* @param struct resource_data *list Any resource list.
* @param bool ground If TRUE, will also count resources on the ground.
* @param bool send_msgs If TRUE, will alert the character as to what they need. FALSE runs silently.
*/
bool has_resources(char_data *ch, struct resource_data *list, bool ground, bool send_msgs) {
	char buf[MAX_STRING_LENGTH];
	int total, amt, liter, cycle;
	struct resource_data *res;
	bool ok = TRUE;
	obj_data *obj;
	
	unmark_items_for_char(ch, ground);

	// This is done in 2 phases (to ensure specific objs are used before components):
	#define HASRES_OBJS  (cycle == 0)	// check/mark specific objs
	#define HASRES_OTHER  (cycle == 1)	// check/mark components
	#define NUM_HASRES_CYCLES  2
	
	for (cycle = 0; cycle < NUM_HASRES_CYCLES; ++cycle) {
		LL_FOREACH(list, res) {
			// only RES_OBJECT is checked in the first cycle
			if (HASRES_OBJS && res->type != RES_OBJECT) {
				continue;
			}
			else if (HASRES_OTHER && res->type == RES_OBJECT) {
				continue;
			}
			
			total = 0;
		
			// RES_x: check resources by type
			switch (res->type) {
				case RES_OBJECT:
				case RES_COMPONENT:
				case RES_LIQUID: {	// these 3 types check objects
					obj_data *search_list[2];
					search_list[0] = ch->carrying;
					search_list[1] = ground ? ROOM_CONTENTS(IN_ROOM(ch)) : NULL;
				
					// now search the list(s)
					for (liter = 0; liter < 2 && total < res->amount; ++liter) {
						LL_FOREACH2(search_list[liter], obj, next_content) {
							// skip already-used items
							if (obj->search_mark) {
								continue;
							}
							// skip keeps
							if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
								continue;
							}
							if (!CAN_SEE_OBJ(ch, obj)) {
								continue;
							}
						
							// RES_x: just types that need objects
							switch (res->type) {
								case RES_OBJECT: {
									if (GET_OBJ_VNUM(obj) == res->vnum) {
										++total;
										obj->search_mark = TRUE;
									}
									break;
								}
								case RES_COMPONENT: {
									// require full match on cmp flags
									if (GET_OBJ_CMP_TYPE(obj) == res->vnum && (GET_OBJ_CMP_FLAGS(obj) & res->misc) == res->misc) {
										++total;
										obj->search_mark = TRUE;
									}
									break;
								}
								case RES_LIQUID: {
									if (IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == res->vnum) {
										// add the volume of liquid
										total += GET_DRINK_CONTAINER_CONTENTS(obj);
										obj->search_mark = TRUE;
									}
									break;
								}
							}
						
							// ok to break out early if we found enough
							if (total >= res->amount) {
								break;
							}
						}
					}

					if (total < res->amount) {
						if (send_msgs) {
							// RES_x: just types that need objects
							switch (res->type) {
								case RES_OBJECT: {
									msg_to_char(ch, "%s %d more of %s", (ok ? "You need" : ","), res->amount - total, skip_filler(get_obj_name_by_proto(res->vnum)));
									break;
								}
								case RES_COMPONENT: {
									msg_to_char(ch, "%s %d more (%s)", (ok ? "You need" : ","), res->amount - total, component_string(res->vnum, res->misc));
									break;
								}
								case RES_LIQUID: {
									msg_to_char(ch, "%s %d more unit%s of %s", (ok ? "You need" : ","), res->amount - total, PLURAL(res->amount - total), get_generic_string_by_vnum(res->vnum, GENERIC_LIQUID, GSTR_LIQUID_NAME));
									break;
								}
							}
						}
						ok = FALSE;
					}
					break;
				}
				case RES_COINS: {
					empire_data *coin_emp = real_empire(res->vnum);
					if (!can_afford_coins(ch, coin_emp, res->amount)) {
						if (send_msgs) {
							msg_to_char(ch, "%s %s", (ok ? "You need" : ","), money_amount(coin_emp, res->amount));
						}
						ok = FALSE;
					}
					break;
				}
				case RES_CURRENCY: {
					if (get_currency(ch, res->vnum) < res->amount) {
						snprintf(buf, sizeof(buf), "You need %d %s.", res->amount, get_generic_string_by_vnum(res->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(res->amount)));
						ok = FALSE;
					}
					break;
				}
				case RES_POOL: {
					// special rule: require that blood or health costs not reduce player below 1
					amt = res->amount + ((res->vnum == HEALTH || res->vnum == BLOOD) ? 1 : 0);
	
					// more player checks
					if (amt >= 0 && GET_CURRENT_POOL(ch, res->vnum) < amt) {
						if (send_msgs) {
							msg_to_char(ch, "%s %d more %s point%s", (ok ? "You need" : ","), amt - GET_CURRENT_POOL(ch, res->vnum), pool_types[res->vnum], PLURAL(amt - GET_CURRENT_POOL(ch, res->vnum)));
						}
						ok = FALSE;
					}
					break;
				}
				case RES_ACTION: {
					// always has these
					break;
				}
			}
		}
	}
	
	if (!ok && send_msgs) {
		send_to_char(".\r\n", ch);
	}
	
	return ok;
}


/**
* Gets the resources-needed list for display to mortals.
*
* @param struct resource_data *list The list to show.
* @param char *save_buffer A string to write the output to.
*/
void show_resource_list(struct resource_data *list, char *save_buffer) {
	struct resource_data *res;
	bool found = FALSE;
	
	*save_buffer = '\0';
	
	LL_FOREACH(list, res) {
		sprintf(save_buffer + strlen(save_buffer), "%s%s", (found ? ", " : ""), get_resource_name(res));
		found = TRUE;
	}
	
	if (!*save_buffer) {
		strcpy(save_buffer, "nothing");
	}
}


/**
* Removes the XX mark on items that will be used for functions that search for
* a list of items.
*
* @param char_data *ch The person who's searching lists of items (e.g. inventory).
* @param bool ground If TRUE, also hits items on the ground.
*/
void unmark_items_for_char(char_data *ch, bool ground) {
	obj_data *search_list[2];
	obj_data *obj;
	int iter;
	
	// 2 places to look
	search_list[0] = ch->carrying;
	search_list[1] = ground ? ROOM_CONTENTS(IN_ROOM(ch)) : NULL;
	
	for (iter = 0; iter < 2; ++iter) {
		LL_FOREACH2(search_list[iter], obj, next_content) {
			obj->search_mark = FALSE;
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// SECTOR UTILS ////////////////////////////////////////////////////////////

/**
* This function finds a sector that has (or doesn't have) certain flags, and
* returns the first matching sector.
*
* @param bitvector_t with_flags Find a sect that has these flags.
* @param bitvector_t without_flags Find a sect that doesn't have these flags.
* @return sector_data* A sector, or NULL if none matches.
*/
sector_data *find_first_matching_sector(bitvector_t with_flags, bitvector_t without_flags) {
	sector_data *sect, *next_sect;
	
	HASH_ITER(hh, sector_table, sect, next_sect) {
		if ((with_flags == NOBITS || SECT_FLAGGED(sect, with_flags)) && (without_flags == NOBITS || !SECT_FLAGGED(sect, without_flags))) {
			return sect;
		}
	}
	
	return NULL;
}


 //////////////////////////////////////////////////////////////////////////////
//// STRING UTILS ////////////////////////////////////////////////////////////

/**
* This converts data file entries into bitvectors, where they may be written
* as "abdo" in the file, or as a number.
*
* - a-z are bits 1-26
* - A-Z are bits 27-52
* - !"#$%&'()*=, are bits 53-64
*
* @param char *flag The input string.
* @return bitvector_t The bitvector.
*/
bitvector_t asciiflag_conv(char *flag) {
	bitvector_t flags = 0;
	bool is_number = TRUE;
	char *p;

	for (p = flag; *p; ++p) {
		// skip numbers
		if (isdigit(*p)) {
			continue;
		}
		
		is_number = FALSE;
		
		if (islower(*p)) {
			flags |= BIT(*p - 'a');
		}
		else if (isupper(*p)) {
			flags |= BIT(26 + (*p - 'A'));
		}
		else {
			flags |= BIT(52 + (*p - '!'));
		}
	}

	if (is_number) {
		flags = strtoull(flag, NULL, 10);
	}

	return (flags);
}


/*
 * Given a string, change all instances of double dollar signs ($$) to
 * single dollar signs ($).  When strings come in, all $'s are changed
 * to $$'s to avoid having users be able to crash the system if the
 * inputted string is eventually sent to act().  If you are using user
 * input to produce screen output AND YOU ARE SURE IT WILL NOT BE SENT
 * THROUGH THE act() FUNCTION (i.e., do_gecho, do_title, but NOT do_say),
 * you can call delete_doubledollar() to make the output look correct.
 *
 * Modifies the string in-place.
 */
char *delete_doubledollar(char *string) {
	char *read, *write;

	/* If the string has no dollar signs, return immediately */
	if ((write = strchr(string, '$')) == NULL)
		return (string);

	/* Start from the location of the first dollar sign */
	read = write;

	while (*read)   /* Until we reach the end of the string... */
		if ((*(write++) = *(read++)) == '$') /* copy one char */
			if (*read == '$')
				read++; /* skip if we saw 2 $'s in a row */

	*write = '\0';

	return (string);
}


/**
* This converts bitvector flags into the human-readable sequence used in db
* files, e.g. "adoO", where each letter represents a bit starting with a=1.
* If there are no bits, it returns the string "0".
*
* - bits 1-26 are lowercase a-z
* - bits 27-52 are uppercase A-Z
* - bits 53-64 are !"#$%&'()*=,
*
* @param bitvector_t flags The bitmask to convert to alpha.
* @return char* The resulting string.
*/
char *bitv_to_alpha(bitvector_t flags) {
	static char output[65];
	int iter, pos;
	
	pos = 0;
	for (iter = 0; flags && iter < 64; ++iter) {
		if (IS_SET(flags, BIT(iter))) {
			if (iter < 26) {
				output[pos++] = ('a' + iter);
			}
			else if (iter < 52) {
				output[pos++] = ('A' + (iter - 26));
			}
			else if (iter < 64) {
				output[pos++] = ('!' + (iter - 52));
			}
		}
		
		// remove so we exhaust flags
		REMOVE_BIT(flags, BIT(iter));
	}
	
	// empty string?
	if (pos == 0) {
		output[pos++] = '0';
	}
	
	// terminate it like Ahnold
	output[pos] = '\0';
	
	return output;
}


char *CAP(char *txt) {
	*txt = UPPER(*txt);
	return (txt);
}


/**
* Counts the number of chars in a string that are color codes and will be
* invisible to the player.
*
* @param const char *str The string to count.
* @return int The length of color codes.
*/
int color_code_length(const char *str) {
	const char *ptr;
	int len = 0;
	
	for (ptr = str; *ptr; ++ptr) {
		if (*ptr == '\t') {
			if (*(ptr+1) == '\t' || *(ptr+1) == COLOUR_CHAR) {	// && = &
				++ptr;
				++len;	// only 1 char counts as a color code
			}
			else if (*(ptr+1) == '[') {
				++len;	// 1 for the &
				if (UPPER(*(ptr+2)) != 'U') {
					++len;	// we skip 1 len if there is a U because 1 char will be visible
				}
				for (++ptr; *ptr != ']'; ++ptr) {	// count chars in *[..]
					++len;
				}
			}
			else {	// assume 2-wide color char
				++ptr;
				len += 2;
			}
		}
		else if (*ptr == COLOUR_CHAR) {
			if (*(ptr+1) == COLOUR_CHAR) {	// && = &
				++ptr;
				++len;	// only 1 char counts as a color code, the other is removed
			}
			else if (*(ptr+1) == '[' && config_get_bool("allow_extended_color_codes")) {
				++len;	// 1 for the &
				if (UPPER(*(ptr+2)) != 'U') {
					++len;	// we skip 1 len if there is a U because 1 char will be visible
				}
				for (++ptr; *ptr != ']'; ++ptr) {	// count chars in &[..]
					++len;
				}
			}
			else {	// assume 2-wide color char
				++ptr;
				len += 2;
			}
		}
		// else not a color code
	}
	
	return len;
}


/**
* Counts the number of icon codes (@) in a string.
*
* @param char *string The input string.
* @return int the number of @ codes.
*/
int count_icon_codes(char *string) {
	const char *icon_codes = ".ewuUvV";
	
	int iter, count = 0, len = strlen(string);
	for (iter = 0; iter < len - 1; ++iter) {
		if (string[iter] == '@' && strchr(icon_codes, string[iter+1])) {
			++count;
			++iter;	// advance past the color code
		}
	}
	
	return count;
}


/**
* @param const char *string A string that might contain stray % signs.
* @return const char* A string with % signs doubled.
*/
const char *double_percents(const char *string) {
	static char output[MAX_STRING_LENGTH*2];
	int iter, pos;
	
	// no work
	if (!strchr(string, '%')) {
		return string;
	}
	
	for (iter = 0, pos = 0; string[iter] && pos < (MAX_STRING_LENGTH*2); ++iter) {
		output[pos++] = string[iter];
		
		// double %
		if (string[iter] == '%' && pos < (MAX_STRING_LENGTH*2)) {
			output[pos++] = '%';
		}
	}
	
	// terminate/ensure terminator
	output[MIN(pos, (MAX_STRING_LENGTH*2)-1)] = '\0';
	return (const char*)output;
}


/**
* @param const char *namelist A keyword list, e.g. "bear white huge"
* @return char* The first name in the list, e.g. "bear"
*/
char *fname(const char *namelist) {
	static char holder[30];
	register char *point;

	for (point = holder; isalpha(*namelist); namelist++, point++) {
		*point = *namelist;
	}

	*point = '\0';

	return (holder);
}


/**
* Determines if a keyword string contains at least one keyword from a list.
*
* @param char *string The string of keywords entered by a player.
* @param const char *list[] A set of words, terminated by a "\n".
* @param bool exact Does not allow abbrevs unless FALSE.
* @return bool TRUE if at least one keyword from the list appears.
*/
bool has_keyword(char *string, const char *list[], bool exact) {
	char word[MAX_INPUT_LENGTH], *ptr;
	ptr = string;
	int iter;
	
	while ((ptr = any_one_arg(ptr, word)) && *word) {
		for (iter = 0; *list[iter] != '\n'; ++iter) {
			if (!str_cmp(word, list[iter]) || (!exact && is_abbrev(word, list[iter]))) {
				return TRUE;	// found 1
			}
		}
	}
	
	return FALSE;
}


/**
* Determines if str is an abbrev for any keyword in namelist.
*
* @param const char *str The search argument, e.g. "be"
* @param const char *namelist The keyword list, e.g. "bear white huge"
* @return bool TRUE if str is an abbrev for any keyword in namelist
*/
bool isname(const char *str, const char *namelist) {
	char *newlist, *curtok;
	bool found = FALSE;

	/* the easy way */
	if (!str_cmp(str, namelist)) {
		return TRUE;
	}

	newlist = strdup(namelist);

	for (curtok = strtok(newlist, WHITESPACE); curtok && !found; curtok = strtok(NULL, WHITESPACE)) {
		if (curtok && is_abbrev(str, curtok)) {
			found = TRUE;
		}
	}

	free(newlist);
	return found;
}


/**
* Makes a level range look more elegant. You can omit current.
*
* @param int min The low end of the range (0 for none).
* @param int max The high end of the range (0 for no max).
* @param int current If it's already scaled, one level (0 for none).
* @return char* The string.
*/
char *level_range_string(int min, int max, int current) {
	static char output[65];
	
	if (current > 0) {
		snprintf(output, sizeof(output), "%d", current);
	}
	else if (min == max) {	// could also be "0" here
		snprintf(output, sizeof(output), "%d", min);
	}
	else if (min > 0 && max > 0) {
		snprintf(output, sizeof(output), "%d-%d", min, max);
	}
	else if (min > 0) {
		snprintf(output, sizeof(output), "%d+", min);
	}
	else if (max > 0) {
		snprintf(output, sizeof(output), "1-%d", max);
	}
	else {
		snprintf(output, sizeof(output), "0");
	}
	
	return output;
}


/**
* This works like isname but checks to see if EVERY word in arg is an abbrev
* of some word in namelist. E.g. "cl br" is an abbrev of "bricks pile clay",
* the namelist from "a pile of clay bricks".
*
* @param const char *arg The user input
* @param const char *namelist The list of names to check
* @return bool TRUE if every word in arg is a valid name from namelist, otherwise FALSE
*/
bool multi_isname(const char *arg, const char *namelist) {
	char argcpy[MAX_INPUT_LENGTH], argword[256];
	char *ptr;
	bool ok;
	
	if (!namelist || !*namelist) {
		return FALSE;	// shortcut
	}

	/* the easy way */
	if (!str_cmp(arg, namelist)) {
		return TRUE;
	}
	
	strcpy(argcpy, arg);
	ptr = argcpy;
	do {
		ptr = any_one_arg(ptr, argword);
	} while (fill_word(argword));
	
	ok = TRUE;
	while (*argword && ok) {
		if (!isname(argword, namelist)) {
			ok = FALSE;
		}
		do {	// skip fill words like "of" so "bunch of apples" also matches "bunch apples"	
			ptr = any_one_arg(ptr, argword);
		} while (fill_word(argword));
	}
	
	return ok;
}


/**
* This does the same thing as sprintbit but looks nicer for players.
*/
void prettier_sprintbit(bitvector_t bitvector, const char *names[], char *result) {
	long nr;
	bool found = FALSE;

	*result = '\0';

	for (nr = 0; bitvector; bitvector >>= 1) {
		if (IS_SET(bitvector, 1)) {
			if (*names[nr] && *names[nr] != '\n') {
				sprintf(result + strlen(result), "%s%s", (found ? ", " : ""), names[nr]);
				found = TRUE;
			}
			else if (*names[nr]) {
				sprintf(result + strlen(result), "%sUNDEFINED", (found ? ", " : ""));
				found = TRUE;
			}
		}
		if (*names[nr] != '\n') {
			nr++;
		}
	}

	if (!*result) {
		strcpy(result, "none");
	}
}


/*
 * Strips \r\n from end of string.
 */
void prune_crlf(char *txt) {
	int i = strlen(txt) - 1;

	while (txt[i] == '\n' || txt[i] == '\r') {
		txt[i--] = '\0';
	}
}


/**
* Replaces a &? with the replacement color of your choice.
*
* @param char *input The incoming string containing 0 or more '&?'.
* @param char *color The string to replace '&?' with.
* @param char *output A buffer to store the result to.
*/
void replace_question_color(char *input, char *color, char *output) {
	int ipos, opos;
	*output = '\0';
	
	for (ipos = 0, opos = 0; ipos < strlen(input); ++ipos) {
		if (input[ipos] == '&' && input[ipos+1] == '?') {
			// copy replacement color instead
			strcpy(output + opos, color);
			opos += strlen(color);
			
			// skip past &?
			++ipos;
		}
		else {
			output[opos++] = input[ipos];
		}
	}
	
	output[opos++] = '\0';
}


/**
* Finds the last occurrence of needle in haystack.
*
* @param const char *haystack The string to search.
* @param const char *needle The thing to search for.
* @return char* The last occurrence of needle in haystack, or NULL if it does not occur.
*/
char *reverse_strstr(char *haystack, char *needle) {
	char *found = NULL, *next = haystack;
	
	if (!haystack || !needle || !*haystack) {
		return NULL;
	}
	
	while ((next = strstr(next, needle))) {
		found = next;
		++next;
	}
	
	return found;
}


/**
* Looks for certain keywords in a set of custom messages. All given keywords
* must appear in 1 of the custom messages to be valid.
*
* @param char *keywords The word(s) we are looking for.
* @param struct custom_message *list The list of custom messages to search.
* @return bool TRUE if the keywords were found, FALSE if not.
*/
bool search_custom_messages(char *keywords, struct custom_message *list) {
	struct custom_message *iter;
	
	LL_FOREACH(list, iter) {
		if (iter->msg && multi_isname(keywords, iter->msg)) {
			return TRUE;
		}
	}
	
	return FALSE;	// not found
}


/**
* Looks for certain keywords in a set of extra descriptions. All given keywords
* must appear in 1 of the descriptions or its keywords to be valid.
*
* @param char *keywords The word(s) we are looking for.
* @param struct extra_descr_data *list The list of extra descs to search.
* @return bool TRUE if the keywords were found, FALSE if not.
*/
bool search_extra_descs(char *keywords, struct extra_descr_data *list) {
	struct extra_descr_data *iter;
	
	LL_FOREACH(list, iter) {
		if (iter->keyword && multi_isname(keywords, iter->keyword)) {
			return TRUE;
		}
		if (iter->description && multi_isname(keywords, iter->description)) {
			return TRUE;
		}
	}
	
	return FALSE;	// not found
}


/**
* Doubles the & in a string so that color codes are displayed to the user.
*
* @param char *string The input string.
* @return char* The string with color codes shown.
*/
char *show_color_codes(char *string) {
	static char value[MAX_STRING_LENGTH * 3];	// reserve extra space
	char *ptr;
	
	ptr = str_replace("&", "\t&", string);
	strncpy(value, ptr, sizeof(value));
	value[sizeof(value)-1] = '\0';	// safety
	free(ptr);
	
	return value;
}


/**
* This function gets just the portion of a string after any fill words or
* reserved words -- used especially to simplify "an object" to "object".
*
* If you want to save the result somewhere, you should str_dup() it
*
* @param const char *string The original string.
*/
const char *skip_filler(const char *string) {	
	static char remainder[MAX_STRING_LENGTH];
	char temp[MAX_STRING_LENGTH];
	char *ptr, *ot;
	int pos = 0;
	
	*remainder = '\0';
	
	if (!string) {
		log("SYSERR: skip_filler received a NULL pointer.");
		return (const char *)remainder;
	}
	
	do {
		string += pos;
		ot = (char*)string;	// just to skip_spaces on it, which won't modify the string
		skip_spaces(&ot);
		string = ot;
		
		if ((ptr = strchr(string, ' '))) {
			pos = ptr - string;
		}
		else {
			pos = 0;
		}
		
		if (pos > 0) {
			strncpy(temp, string, pos);
		}
		temp[pos] = '\0';
	} while (fill_word(temp) || reserved_word(temp));
	
	// when we get here, string is a pointer to the start of the real name
	strcpy(remainder, string);
	return (const char *)remainder;
}


/*
 * If you don't have a 'const' array, just cast it as such.  It's safer
 * to cast a non-const array as const than to cast a const one as non-const.
 * Doesn't really matter since this function doesn't change the array though.
 */
void sprintbit(bitvector_t bitvector, const char *names[], char *result, bool space) {
	long nr;

	*result = '\0';

	for (nr = 0; bitvector; bitvector >>= 1) {
		if (IS_SET(bitvector, 1)) {
			if (*names[nr] != '\n') {
				strcat(result, names[nr]);
				if (space && *names[nr]) {
					strcat(result, " ");
				}
			}
			else {
				strcat(result, "UNDEFINED ");
			}
		}
		if (*names[nr] != '\n') {
			nr++;
		}
	}

	if (!*result && space) {
		strcpy(result, "NOBITS ");
	}
}


void sprinttype(int type, const char *names[], char *result) {
	int nr = 0;

	while (type && *names[nr] != '\n') {
		type--;
		nr++;
	}

	if (*names[nr] != '\n') {
		strcpy(result, names[nr]);
	}
	else {
		strcpy(result, "UNDEFINED");
	}
}


/**
* Similar to strchr except the 2nd argument is a string.
*
* @param const char *haystack The string to search.
* @param const char *needles A set of characters to search for.
* @return bool TRUE if any character from needles exists in haystack.
*/
bool strchrstr(const char *haystack, const char *needles) {
	int iter;
	
	for (iter = 0; needles[iter] != '\0'; ++iter) {
		if (strchr(haystack, needles[iter])) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/*
 * str_cmp: a case-insensitive version of strcmp().
 * Returns: 0 if equal, > 0 if arg1 > arg2, or < 0 if arg1 < arg2.
 *
 * Scan until strings are found different or we reach the end of both.
 */
int str_cmp(const char *arg1, const char *arg2) {
	int chk, i;

	if (arg1 == NULL || arg2 == NULL) {
		log("SYSERR: str_cmp() passed a NULL pointer, %p or %p.", arg1, arg2);
		return (0);
	}

	for (i = 0; arg1[i] || arg2[i]; i++) {
		if ((chk = LOWER(arg1[i]) - LOWER(arg2[i])) != 0) {
			return (chk);	/* not equal */
		}
	}

	return (0);
}


/* Create a duplicate of a string */
char *str_dup(const char *source) {
	char *new_z;

	CREATE(new_z, char, strlen(source) + 1);
	return (strcpy(new_z, source));
}


/**
* Capitalizes the first letter of each word.
*
* @param char *string The string to ucwords on.
*/
void ucwords(char *string) {
	int iter, len = strlen(string);
	
	for (iter = 0; iter < len; ++iter) {
		if (iter == 0 || string[iter-1] == ' ') {
			string[iter] = UPPER(string[iter]);
		}
	}
}


/**
* Generic string replacement function: returns a memory-allocated char* with
* the resulting string.
*
* @param const char *search The search string ('foo').
* @param const char *replace The replacement string ('bar').
* @param const char *subject The string to alter ('foodefoo').
* @return char* A pointer to a new string with the replacements ('bardebar').
*/
char *str_replace(const char *search, const char *replace, const char *subject) {
	static char output[MAX_STRING_LENGTH];
	int spos, opos, slen, rlen;
	
	// nothing / don't bother
	if (!*subject || !*search || !strstr(subject, search)) {
		return str_dup(subject);
	}
	
	slen = strlen(search);
	rlen = strlen(replace);
	
	for (spos = 0, opos = 0; spos < strlen(subject) && opos < MAX_STRING_LENGTH; ) {
		if (!strncmp(subject + spos, search, slen)) {
			// found a search match at this position
			strncpy(output + opos, replace, MAX_STRING_LENGTH - opos);
			opos += rlen;
			spos += slen;
		}
		else {
			output[opos++] = subject[spos++];
		}
	}
	
	// safety terminations:
	if (opos < MAX_STRING_LENGTH) {
		output[opos++] = '\0';
	}
	else {
		output[MAX_STRING_LENGTH-1] = '\0';
	}
	
	// allocate enough memory to return
	return str_dup(output);
}


/**
* Strips out the color codes and returns the string.
*
* @param char *input
* @return char *result (no memory allocated, so str_dup if you want to keep it)
*/
char *strip_color(char *input) {
	static char lbuf[MAX_STRING_LENGTH];
	char *ptr;
	int iter, pos;

	for (iter = 0, pos = 0; pos < (MAX_STRING_LENGTH-1) && iter < strlen(input); ++iter) {
		if (input[iter] == '&') {
			if (input[iter+1] == '&') {
				// double &: copy both
				lbuf[pos++] = input[iter];
				lbuf[pos++] = input[++iter];
			}
			else {
				// single &: skip next char (color code)
				++iter;
			}
		}
		else if (input[iter] == '\t') {
			if (input[iter+1] == '&' || input[iter+1] == '\t') {
				// double \t: copy both
				lbuf[pos++] = input[iter];
				lbuf[pos++] = input[++iter];
			}
			else {
				// single \t: skip next char (color code)
				++iter;
			}
		}
		else {
			// normal char: copy it
			lbuf[pos++] = input[iter];
		}
	}
	
	lbuf[pos++] = '\0';
	ptr = lbuf;
	
	return ptr;
}


/* Removes the \r from \r\n to prevent Windows clients from seeing extra linebreaks in descs */
void strip_crlf(char *buffer) {
	register char *ptr, *str;

	ptr = buffer;
	str = ptr;

	while ((*str = *ptr)) {
		str++;
		ptr++;
		if (*ptr == '\r') {
			ptr++;
		}
	}
}


/* strips \r's from line */
char *stripcr(char *dest, const char *src) {
	int i, length;
	char *temp;

	if (!dest || !src) return NULL;
	temp = &dest[0];
	length = strlen(src);
	for (i = 0; *src && (i < length); i++, src++)
		if (*src != '\r') *(temp++) = *src;
	*temp = '\0';
	return dest;
}


/**
* Converts a string to all-lowercase. The orignal string is modified by this,
* and a reference to it is returned.
*
* @param char *str The string to lower.
* @return char* A pointer to the string.
*/
char *strtolower(char *str) {
	char *ptr;
	
	for (ptr = str; *ptr; ++ptr) {
		*ptr = LOWER(*ptr);
	}
	
	return str;
}


/**
* Converts a string to all-uppercase. The orignal string is modified by this,
* and a reference to it is returned.
*
* @param char *str The string to uppercase.
* @return char* A pointer to the string.
*/
char *strtoupper(char *str) {
	char *ptr;
	
	for (ptr = str; *ptr; ++ptr) {
		*ptr = UPPER(*ptr);
	}
	
	return str;
}


/*
 * strn_cmp: a case-insensitive version of strncmp().
 * Returns: 0 if equal, > 0 if arg1 > arg2, or < 0 if arg1 < arg2.
 *
 * Scan until strings are found different, the end of both, or n is reached.
 */
int strn_cmp(const char *arg1, const char *arg2, int n) {
	int chk, i;

	if (arg1 == NULL || arg2 == NULL) {
		log("SYSERR: strn_cmp() passed a NULL pointer, %p or %p.", arg1, arg2);
		return (0);
	}

	for (i = 0; (arg1[i] || arg2[i]) && (n > 0); i++, n--) {
		if ((chk = LOWER(arg1[i]) - LOWER(arg2[i])) != 0) {
			return (chk);	/* not equal */
		}
	}

	return (0);
}


/*
* Version of str_str from dg_scripts -- better than the base CircleMUD version.
*
* @param char *cs The string to search.
* @param char *ct The string to search for...
* @return char* A pointer to the substring within cs, or NULL if not found.
*/
char *str_str(char *cs, char *ct) {
	char *s, *t;

	if (!cs || !ct || !*ct)
		return NULL;

	while (*cs) {
		t = ct;

		while (*cs && (LOWER(*cs) != LOWER(*t)))
			cs++;

		s = cs;

		while (*t && *cs && (LOWER(*cs) == LOWER(*t))) {
			t++;
			cs++;
		}

		if (!*t)
			return s;
	}

	return NULL;
}


/**
* Takes a number of seconds and returns a string like "5 days, 1 hour, 
* 30 minutes, 27 seconds".
*
* @param int seconds Amount of time.
* @return char* The string describing the time.
*/
char *time_length_string(int seconds) {
	static char output[MAX_STRING_LENGTH];
	bool any = FALSE;
	int left, amt;
	
	*output = '\0';
	left = ABSOLUTE(seconds);	// sometimes we get negative seconds for times in the future?
	
	if ((amt = (left / SECS_PER_REAL_DAY)) > 0) {
		sprintf(output + strlen(output), "%s%d day%s", (any ? ", " : ""), amt, PLURAL(amt));
		left -= (amt * SECS_PER_REAL_DAY);
		any = TRUE;
	}
	if ((amt = (left / SECS_PER_REAL_HOUR)) > 0) {
		sprintf(output + strlen(output), "%s%d hour%s", (any ? ", " : ""), amt, PLURAL(amt));
		left -= (amt * SECS_PER_REAL_HOUR);
		any = TRUE;
	}
	if ((amt = (left / SECS_PER_REAL_MIN)) > 0) {
		sprintf(output + strlen(output), "%s%d minute%s", (any ? ", " : ""), amt, PLURAL(amt));
		left -= (amt * SECS_PER_REAL_MIN);
		any = TRUE;
	}
	if (left > 0) {
		sprintf(output + strlen(output), "%s%d second%s", (any ? ", " : ""), left, PLURAL(left));
		any = TRUE;
	}
	
	if (!*output) {
		strcpy(output, "0 seconds");
	}
	
	return output;
}


/**
* Removes spaces (' ') from the end of a string, and returns a pointer to the
* first non-space character.
*
* @param char *string The string to trim (will lose spaces at the end).
* @return char* A pointer to the first non-space character in the string.
*/
char *trim(char *string) {
	char *ptr = string;
	int len;
	
	if (!string) {
		return NULL;
	}
	
	// trim start
	while (*ptr == ' ') {
		++ptr;
	}
	
	// trim end
	while ((len = strlen(ptr)) > 0 && ptr[len-1] == ' ') {
		ptr[len-1] = '\0';
	}
	
	return ptr;
}


 //////////////////////////////////////////////////////////////////////////////
//// TYPE UTILS //////////////////////////////////////////////////////////////

/**
* @param char *name Building to find by name.
* @param bool room_only If TRUE, only matches designatable rooms.
* @return bld_data *The matching building entry (BUILDING_x const) or NULL.
*/
bld_data *get_building_by_name(char *name, bool room_only) {
	bld_data *iter, *next_iter, *partial = NULL;
	
	HASH_ITER(hh, building_table, iter, next_iter) {
		if (room_only && !IS_SET(GET_BLD_FLAGS(iter), BLD_ROOM)) {
			continue;
		}
		if (!str_cmp(name, GET_BLD_NAME(iter))) {
			return iter;	// exact match
		}
		else if (!partial && is_abbrev(name, GET_BLD_NAME(iter))) {
			partial = iter;
		}
	}
	
	return partial;	// if any
}


/**
* Find a crop by name; prefers exact match over abbrev.
*
* @param char *name Crop to find by name.
* @return crop_data* The matching crop prototype or NULL.
*/
crop_data *get_crop_by_name(char *name) {
	crop_data *iter, *next_iter, *abbrev_match = NULL;
	
	HASH_ITER(hh, crop_table, iter, next_iter) {
		if (!str_cmp(name, GET_CROP_NAME(iter))) {
			return iter;
		}
		if (!abbrev_match && is_abbrev(name, GET_CROP_NAME(iter))) {
			abbrev_match = iter;
		}
	}
	
	return abbrev_match;	// if any
}


/**
* Find a sector by name; prefers exact match over abbrev.
*
* @param char *name Sector to find by name.
* @return sector_data* The matching sector or NULL.
*/
sector_data *get_sect_by_name(char *name) {
	sector_data *sect, *next_sect, *abbrev_match = NULL;

	HASH_ITER(hh, sector_table, sect, next_sect) {
		if (!str_cmp(name, GET_SECT_NAME(sect))) {
			// exact match
			return sect;
		}
		if (!abbrev_match && is_abbrev(name, GET_SECT_NAME(sect))) {
			abbrev_match = sect;
		}
	}
	
	return abbrev_match;	// if any
}


 //////////////////////////////////////////////////////////////////////////////
//// WORLD UTILS /////////////////////////////////////////////////////////////


/**
* @param room_data *room Room to check.
* @return bool TRUE if it's sunny there.
*/
bool check_sunny(room_data *room) {
	bool sun_sect = !ROOM_IS_CLOSED(room) || (IS_ADVENTURE_ROOM(room) && RMT_FLAGGED(room, RMT_OUTDOOR));
	return (sun_sect && weather_info.sunlight != SUN_DARK && !ROOM_AFF_FLAGGED(room, ROOM_AFF_DARK));
}


/**
* Gets linear distance between two map locations (accounting for wrapping).
*
* @param int x1 first
* @param int y1  coordinate
* @param int x2 second
* @param int y2  coordinate
* @return int distance
*/
int compute_map_distance(int x1, int y1, int x2, int y2) {
	int dx = x1 - x2;
	int dy = y1 - y2;
	int dist;
	
	// short circuit on same-room
	if (x1 == x2 && y1 == y2) {
		return 0;
	}
	
	// infinite distance if they are in an unknown location
	if (x1 < 0 || x2 < 0 || y1 < 0 || y2 < 0) {
		return MAP_SIZE;	// maximum distance
	}

	// adjust for edges
	if (WRAP_X) {
		if (dx < (-1 * MAP_WIDTH / 2)) {
			dx += MAP_WIDTH;
		}
		if (dx > (MAP_WIDTH / 2)) {
			dx -= MAP_WIDTH;
		}
	}
	if (WRAP_Y) {
		if (dy < (-1 * MAP_HEIGHT / 2)) {
			dy += MAP_HEIGHT;
		}
		if (dy > (MAP_HEIGHT / 2)) {
			dy -= MAP_HEIGHT;
		}
	}
	
	dist = (dx * dx + dy * dy);
	dist = (int) sqrt(dist);
	
	return dist;
}


/**
* Gets coordinates IF the player has Navigation, and an empty string if not.
* The coordinates will include a leading space, like " (x, y)" if present. It
* may also return " (unknown)" if (x,y) are not on the map.
*
* @param char_data *ch The person to check for Navigation.
* @param int x The X-coordinate to show.
* @param int y The Y-coordinate to show.
* @param bool fixed_width If TRUE, spaces the coordinates for display in a vertical column.
* @return char* The string containing " (x, y)" or "", depending on the Navigation ability.
*/
char *coord_display(char_data *ch, int x, int y, bool fixed_width) {
	static char output[80];
	
	if (!ch || IS_NPC(ch) || !HAS_NAVIGATION(ch)) {
		*output = '\0';
	}
	else if (fixed_width) {
		if (CHECK_MAP_BOUNDS(x, y)) {
			snprintf(output, sizeof(output), " (%*d, %*d)", X_PRECISION, x, Y_PRECISION, y);
		}
		else {
			snprintf(output, sizeof(output), " (%*.*s)", X_PRECISION + Y_PRECISION + 2, X_PRECISION + Y_PRECISION + 2, "unknown");
		}
	}
	else if (CHECK_MAP_BOUNDS(x, y)) {
		snprintf(output, sizeof(output), " (%d, %d)", x, y);
	}
	else {
		strcpy(output, " (unknown)");	// strcpy ok: known width
	}
	
	return output;
}


/**
* Counts how many adjacent tiles have the given sector type...
*
* @param room_data *room The location to check.
* @param sector_vnum The sector vnum to find.
* @param bool count_original_sect If TRUE, also checks BASE_SECT
* @return int The number of matching adjacent tiles.
*/
int count_adjacent_sectors(room_data *room, sector_vnum sect, bool count_original_sect) {
	sector_data *rl_sect = sector_proto(sect);
	int iter, count = 0;
	room_data *to_room;
	
	// we only care about its map room
	room = HOME_ROOM(room);
	
	for (iter = 0; iter < NUM_2D_DIRS; ++iter) {
		to_room = real_shift(room, shift_dir[iter][0], shift_dir[iter][1]);
		
		if (to_room && (SECT(to_room) == rl_sect || (count_original_sect && BASE_SECT(to_room) == rl_sect))) {
			++count;
		}
	}
	
	return count;
}


/**
* Counts how many times a sector appears between two locations, NOT counting
* the start/end locations.
*
* @param bitvector_t sectf_bits SECTF_x flag(s) to look for.
* @param room_data *start Tile to start at (not inclusive).
* @param room_data *end Tile to end at (not inclusive).
* @param bool check_base_sect If TRUE, also looks for those flags on the base sector.
* @return int The number of matching sectors.
*/
int count_flagged_sect_between(bitvector_t sectf_bits, room_data *start, room_data *end, bool check_base_sect) {
	room_data *room;
	int dist, iter, count = 0;
	
	// no work
	if (!start || !end || start == end || !sectf_bits) {
		return count;
	}
	
	dist = compute_distance(start, end);	// for safety-checking
	
	for (iter = 1, room = straight_line(start, end, iter); iter <= dist && room && room != end; ++iter, room = straight_line(start, end, iter)) {
		if (SECT_FLAGGED(SECT(room), sectf_bits) || (check_base_sect && SECT_FLAGGED(BASE_SECT(room), sectf_bits))) {
			++count;
		}
	}
	
	return count;
}


/**
* Computes the distance to the nearest connected player.
*
* @param room_data *room Origin spot
* @return int Distance to the nearest player (> MAP_SIZE if no players)
*/
int distance_to_nearest_player(room_data *room) {
	descriptor_data *d_iter;
	char_data *vict;
	int dist, best = MAP_SIZE * 2;
	
	for (d_iter = descriptor_list; d_iter; d_iter = d_iter->next) {
		if ((vict = d_iter->character) && IN_ROOM(vict)) {
			dist = compute_distance(room, IN_ROOM(vict));
			if (dist < best) {
				best = dist;
			}
		}
	}
	
	return best;
}


/**
* This finds the ultimate map point for a given room, resolving any number of
* layers of boats and home rooms.
*
* @param room_data *room Any room in the game.
* @return room_data* A location on the map, or NULL if there is no map location.
*/
room_data *get_map_location_for(room_data *room) {
	room_data *working, *last;
	vehicle_data *veh;
	
	if (!room) {
		return NULL;
	}
	else if (GET_ROOM_VNUM(room) < MAP_SIZE) {
		// shortcut
		return room;
	}
	else if (GET_ROOM_VNUM(HOME_ROOM(room)) >= MAP_SIZE && IN_VEHICLE_IN_ROOM(room) == room && !IS_ADVENTURE_ROOM(HOME_ROOM(room))) {
		// no home room on the map and not in a vehicle?
		return NULL;
	}
	
	working = room;
	do {
		last = working;
		
		// instance resolution: find instance home
		do {
			last = working;
			if (COMPLEX_DATA(working) && COMPLEX_DATA(working)->instance && INST_FAKE_LOC(COMPLEX_DATA(working)->instance)) {
				working = INST_FAKE_LOC(COMPLEX_DATA(working)->instance);
			}
		} while (last != working);
		
		// vehicle resolution: find top vehicle->in_room: this is similar to GET_ROOM_VEHICLE()/IN_VEHICLE_IN_ROOM()
		do {
			last = working;
			veh = (COMPLEX_DATA(working) ? COMPLEX_DATA(working)->vehicle : NULL);
		
			if (veh && IN_ROOM(veh)) {
				working = IN_ROOM(veh);
			}
		} while (last != working);
		
		// home resolution: similar to HOME_ROOM()
		if (COMPLEX_DATA(working) && COMPLEX_DATA(working)->home_room) {
			working = COMPLEX_DATA(working)->home_room;
		}
	} while (COMPLEX_DATA(working) && GET_ROOM_VNUM(working) >= MAP_SIZE && last != working);
	
	if (GET_ROOM_VNUM(working) < MAP_SIZE) {
		return working;
	}
	else {
		return NULL;	// found a location not on the map
	}
}


/**
* Find an optimal place to start upon new login or death.
*
* @param char_data *ch The player to find a loadroom for.
* @return room_data* The location of a place to start.
*/
room_data *find_load_room(char_data *ch) {
	extern room_data *find_starting_location();
	
	struct empire_territory_data *ter, *next_ter;
	room_data *rl, *rl_last_room, *found, *map;
	int num_found = 0;
	sh_int island;
	bool veh_ok;
	
	// preferred graveyard?
	if (!IS_NPC(ch) && (rl = real_room(GET_TOMB_ROOM(ch))) && room_has_function_and_city_ok(rl, FNC_TOMB) && can_use_room(ch, rl, GUESTS_ALLOWED) && !IS_BURNING(rl)) {
		// check that we're not somewhere illegal (vehicle in enemy territory)
		veh_ok = GET_MAP_LOC(rl) && (map = real_room(GET_MAP_LOC(rl)->vnum)) && can_use_room(ch, map, GUESTS_ALLOWED);
		
		// does not require last room but if there is one, it must be the same island
		rl_last_room = real_room(GET_LAST_ROOM(ch));
		if (veh_ok && (!rl_last_room || GET_ISLAND(rl) == GET_ISLAND(rl_last_room))) {
			return rl;
		}
	}
	
	// first: look for graveyard
	if (!IS_NPC(ch) && (rl = real_room(GET_LAST_ROOM(ch))) && GET_LOYALTY(ch)) {
		island = GET_ISLAND_ID(rl);
		found = NULL;
		HASH_ITER(hh, EMPIRE_TERRITORY_LIST(GET_LOYALTY(ch)), ter, next_ter) {
			if (room_has_function_and_city_ok(ter->room, FNC_TOMB) && IS_COMPLETE(ter->room) && GET_ISLAND_ID(ter->room) == island && !IS_BURNING(ter->room)) {
				// pick at random if more than 1
				if (!number(0, num_found++) || !found) {
					found = ter->room;
				}
			}
		}
		
		if (found) {
			return found;
		}
	}
	
	// still here?
	return find_starting_location();
}


/**
* This determines if ch is close enough to a sect with certain flags.
*
* @param char_data *ch
* @param bitvector_t with_flags The bits that the sect MUST have (returns true if any 1 is set).
* @param bitvector_t without_flags The bits that must NOT be set (returns true if NONE are set).
* @param int distance how far away to check
* @return bool TRUE if the sect is found
*/
bool find_flagged_sect_within_distance_from_char(char_data *ch, bitvector_t with_flags, bitvector_t without_flags, int distance) {
	bool found = FALSE;
	
	if (!IS_ADVENTURE_ROOM(IN_ROOM(ch)) && IS_OUTDOORS(ch)) {
		found = find_flagged_sect_within_distance_from_room(IN_ROOM(ch), with_flags, without_flags, distance);
	}
	
	return found;
}


/**
* This determines if room is close enough to a sect with certain flags.
*
* @param room_data *room The location to check.
* @param bitvector_t with_flags The bits that the sect MUST have (returns true if any 1 is set).
* @param bitvector_t without_flags The bits that must NOT be set (returns true if NONE are set).
* @param int distance how far away to check
* @return bool TRUE if the sect is found
*/
bool find_flagged_sect_within_distance_from_room(room_data *room, bitvector_t with_flags, bitvector_t without_flags, int distance) {
	int x, y;
	room_data *shift, *real;
	bool found = FALSE;
	
	if (!(real = (GET_MAP_LOC(room) ? real_room(GET_MAP_LOC(room)->vnum) : NULL))) {	// no map location
		return FALSE;
	}
	
	for (x = -1 * distance; x <= distance && !found; ++x) {
		for (y = -1 * distance; y <= distance && !found; ++y) {
			shift = real_shift(real, x, y);
			if (shift && (with_flags == NOBITS || ROOM_SECT_FLAGGED(shift, with_flags)) && (without_flags == NOBITS || !ROOM_SECT_FLAGGED(shift, without_flags))) {
				if (compute_distance(room, shift) <= distance) {
					found = TRUE;
				}
			}
		}
	}
	
	return found;
}


/**
* This determines if ch is close enough to a given sect.
*
* @param char_data *ch
* @param sector_vnum sect Sector vnum
* @param int distance how far away to check
* @return bool TRUE if the sect is found
*/
bool find_sect_within_distance_from_char(char_data *ch, sector_vnum sect, int distance) {
	bool found = FALSE;
	
	if (!IS_ADVENTURE_ROOM(IN_ROOM(ch)) && IS_OUTDOORS(ch)) {
		found = find_sect_within_distance_from_room(IN_ROOM(ch), sect, distance);
	}
	
	return found;
}


/**
* This determines if room is close enough to a given sect.
*
* @param room_data *room
* @param sector_vnum sect Sector vnum
* @param int distance how far away to check
* @return bool TRUE if the sect is found
*/
bool find_sect_within_distance_from_room(room_data *room, sector_vnum sect, int distance) {
	room_data *real;
	sector_data *find = sector_proto(sect);
	bool found = FALSE;
	room_data *shift;
	int x, y;
	
	if (!(real = (GET_MAP_LOC(room) ? real_room(GET_MAP_LOC(room)->vnum) : NULL))) {	// no map location
		return FALSE;
	}
	
	for (x = -1 * distance; x <= distance && !found; ++x) {
		for (y = -1 * distance; y <= distance && !found; ++y) {
			shift = real_shift(real, x, y);
			if (shift && SECT(shift) == find && compute_distance(real, shift) <= distance) {
				found = TRUE;
			}
		}
	}
	
	return found;
}


/**
* find a random starting location
*
* @return room_data* A random starting location.
*/
room_data *find_starting_location() {
	extern int highest_start_loc_index;
	extern int *start_locs;
	
	if (highest_start_loc_index < 0) {
		return NULL;
	}

	return (real_room(start_locs[number(0, highest_start_loc_index)]));
}


/**
 * Attempt to find a random starting location, other than the one you're currently in
 *
 * @return room_data* A random starting location that's less likely to be your current one
 */
room_data *find_other_starting_location(room_data *current_room) {
	extern int highest_start_loc_index;
	extern int *start_locs;
	int start_loc_index;
	
	if (highest_start_loc_index < 0) {
		return NULL;
	}
	
	// Select a random start_loc_index.
	start_loc_index = number(0, highest_start_loc_index);
	
	// If the selected index is the user's current room's index:
	if (start_locs[start_loc_index] == GET_ROOM_VNUM(current_room)) {
		// Increment by one, and if it's out of bounds, wrap back to 0.
		if (++start_loc_index > highest_start_loc_index) {
			start_loc_index = 0;
		}
	}
	
	// We return a room no matter what, even if it's the one they're in already.
	return (real_room(start_locs[start_loc_index]));
}


/**
* This function takes a set of coordinates and finds another location in
* relation to them. It checks the wrapping boundaries of the map in the
* process.
*
* @param int start_x The initial X coordinate.
* @param int start_y The initial Y coordinate.
* @param int x_shift How much to shift X by (+/-).
* @param int y_shift How much to shift Y by (+/-).
* @param int *new_x A variable to bind the new X coord to.
* @param int *new_y A variable to bind the new Y coord to.
* @return bool TRUE if a valid location was found; FALSE if it's off the map.
*/
bool get_coord_shift(int start_x, int start_y, int x_shift, int y_shift, int *new_x, int *new_y) {
	// clear these
	*new_x = -1;
	*new_y = -1;
	
	if (start_x < 0 || start_x >= MAP_WIDTH || start_y < 0 || start_y >= MAP_HEIGHT) {
		// bad location
		return FALSE;
	}
	
	// process x
	start_x += x_shift;
	if (start_x < 0) {
		if (WRAP_X) {
			start_x += MAP_WIDTH;
		}
		else {
			return FALSE;	// off the map
		}
	}
	else if (start_x >= MAP_WIDTH) {
		if (WRAP_X) {
			start_x -= MAP_WIDTH;
		}
		else {
			return FALSE;	// off the map
		}
	}
	
	// process y
	start_y += y_shift;
	if (start_y < 0) {
		if (WRAP_Y) {
			start_y += MAP_HEIGHT;
		}
		else {
			return FALSE;	// off the map
		}
	}
	else if (start_y >= MAP_HEIGHT) {
		if (WRAP_Y) {
			start_y -= MAP_HEIGHT;
		}
		else {
			return FALSE;	// off the map
		}
	}
	
	// found a valid location
	*new_x = start_x;
	*new_y = start_y;
	return TRUE;
}


/**
* This function determines the approximate direction between two points on the
* map.
*
* @param room_data *from The origin point.
* @param room_data *to The desitination point.
* @return int Any direction const (NORTHEAST), or NO_DIR for no direction.
*/
int get_direction_to(room_data *from, room_data *to) {
	room_data *origin = HOME_ROOM(from), *dest = HOME_ROOM(to);
	int from_x = X_COORD(origin), from_y = Y_COORD(origin);
	int to_x = X_COORD(dest), to_y = Y_COORD(dest);
	int x_diff = to_x - from_x, y_diff = to_y - from_y;
	int dir = NO_DIR;
	
	// adjust for edges
	if (WRAP_X) {
		if (x_diff < (-1 * MAP_WIDTH / 2)) {
			x_diff += MAP_WIDTH;
		}
		if (x_diff > (MAP_WIDTH / 2)) {
			x_diff -= MAP_WIDTH;
		}
	}
	if (WRAP_Y) {
		if (y_diff < (-1 * MAP_HEIGHT / 2)) {
			y_diff += MAP_HEIGHT;
		}
		if (y_diff > (MAP_HEIGHT / 2)) {
			y_diff -= MAP_HEIGHT;
		}
	}

	// tolerance: 1 e/w per 5 north would still count as "north"
	
	if (x_diff == 0 && y_diff == 0) {
		dir = NO_DIR;
	}
	else if (ABSOLUTE(y_diff/5) >= ABSOLUTE(x_diff)) {
		dir = (y_diff > 0) ? NORTH : SOUTH;
	}
	else if (ABSOLUTE(x_diff/5) >= ABSOLUTE(y_diff)) {
		dir = (x_diff > 0) ? EAST : WEST;
	}
	else if (x_diff > 0 && y_diff > 0) {
		dir = NORTHEAST;
	}
	else if (x_diff > 0 && 0 > y_diff) {
		dir = SOUTHEAST;
	}
	else if (0 > x_diff && y_diff > 0) {
		dir = NORTHWEST;
	}
	else if (0 > x_diff && 0 > y_diff) {
		dir = SOUTHWEST;
	}
	
	return dir;
}


/**
* @param room_data *room A room that has existing mine data
* @return TRUE if the room has a deep mine set up
*/
bool is_deep_mine(room_data *room) {
	struct global_data *glb = global_proto(get_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM));	
	return glb ? (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) > GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE)) : FALSE;
}


/**
* Locks in a random tile icon by assigning it as a custom icon. This only works
* if the room doesn't already have a custom icon.
*
* @param room_data *room The room to set this on.
* @param struct icon_data *use_icon Optional: Force it to use this icon (may be NULL).
*/
void lock_icon(room_data *room, struct icon_data *use_icon) {
	extern struct icon_data *get_icon_from_set(struct icon_data *set, int type);
	extern int pick_season(room_data *room);
	
	struct icon_data *icon;
	int season;

	// don't do it if a custom icon is set
	if (ROOM_CUSTOM_ICON(room)) {
		return;
	}

	if (!(icon = use_icon)) {
		season = pick_season(room);
		icon = get_icon_from_set(GET_SECT_ICONS(SECT(room)), season);
	}
	ROOM_CUSTOM_ICON(room) = str_dup(icon->icon);
}


/**
* The main function for finding one map location starting from another location
* that is either on the map, or can be resolved to the map (e.g. a home room).
* This function returns NULL if there is no valid shift.
*
* @param room_data *origin The start location
* @param int x_shift How far to move east/west
* @param int y_shift How far to move north/south
* @return room_data* The new location on the map, or NULL if the location would be off the map
*/
room_data *real_shift(room_data *origin, int x_shift, int y_shift) {
	int x_coord, y_coord;
	room_data *map;
	
	// sanity?
	if (!origin) {
		return NULL;
	}
	
	map = (GET_MAP_LOC(origin) ? real_room(GET_MAP_LOC(origin)->vnum) : NULL);
	
	// are we somehow not on the map? if not, don't shift
	if (!map || GET_ROOM_VNUM(map) >= MAP_SIZE) {
		return NULL;
	}
	
	if (get_coord_shift(FLAT_X_COORD(map), FLAT_Y_COORD(map), x_shift, y_shift, &x_coord, &y_coord)) {
		return real_room((y_coord * MAP_WIDTH) + x_coord);
	}
	return NULL;
}


/**
* Removes all players from a room.
*
* @param room_data *room The room to clear of players.
* @param room_data *to_room Optional: A place to send them (or NULL for auto-detect).
*/
void relocate_players(room_data *room, room_data *to_room) {
	char_data *ch, *next_ch;
	room_data *target;
	
	// sanity
	if (to_room == room) {
		to_room = NULL;
	}
	
	for (ch = ROOM_PEOPLE(room); ch; ch = next_ch) {
		next_ch = ch->next_in_room;
	
		if (!IS_NPC(ch)) {
			if (!(target = to_room)) {
				target = find_load_room(ch);
			}
			// absolute chaos
			if (target == room) {
				// abort!
				return;
			}
			
			char_to_room(ch, target);
			qt_visit_room(ch, IN_ROOM(ch));
			GET_LAST_DIR(ch) = NO_DIR;
			look_at_room(ch);
			act("$n appears in the middle of the room!", TRUE, ch, NULL, NULL, TO_ROOM);
			enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
			msdp_update_room(ch);
		}
	}
}


/**
* This approximates a straight line on the map, where "iter" is how far
* along the line.
*
* @param room_data *origin -- start of the line
* @param room_data *destination -- start of 
* @return room_data* The resulting position, or NULL if it's off the map.
*/
room_data *straight_line(room_data *origin, room_data *destination, int iter) {
	int x1, x2, y1, y2, new_x, new_y;
	double slope, dx, dy;
	room_vnum new_loc;
	
	x1 = X_COORD(origin);
	y1 = Y_COORD(origin);
	
	x2 = X_COORD(destination);
	y2 = Y_COORD(destination);
	
	dx = x2 - x1;
	dy = y2 - y1;
	
	slope = dy / dx;
	
	// moving left
	if (dx < 0) {
		iter *= -1;
	}
	
	new_x = x1 + iter;
	new_y = y1 + round(slope * (double)iter);
	
	// bounds check
	if (WRAP_X) {
		new_x = WRAP_X_COORD(new_x);
	}
	else {
		return NULL;
	}
	if (WRAP_Y) {
		new_y = WRAP_Y_COORD(new_y);
	}
	else {
		return NULL;
	}
	
	// new position as a vnum
	new_loc = new_y * MAP_WIDTH + new_x;
	if (new_loc < MAP_SIZE) {
		return real_room(new_loc);
	}
	else {
		// straight line should always stay on the map
		return NULL;
	}
}


/**
* Gets the x-coordinate for a room, based on its map location. This used to be
* a macro, but it needs to ensure there IS a map location.
*
* @param room_data *room The room to get the x-coordinate for.
* @return int The x-coordinate, or -1 if none.
*/
int X_COORD(room_data *room) {
	if (GET_MAP_LOC(room)) {
		return MAP_X_COORD(GET_MAP_LOC(room)->vnum);
	}
	else {
		return -1;
	}
}


/**
* Gets the y-coordinate for a room, based on its map location. This used to be
* a macro, but it needs to ensure there IS a map location.
*
* @param room_data *room The room to get the y-coordinate for.
* @return int The y-coordinate, or -1 if none.
*/
int Y_COORD(room_data *room) {
	if (GET_MAP_LOC(room)) {
		return MAP_Y_COORD(GET_MAP_LOC(room)->vnum);
	}
	else {
		return -1;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MISC UTILS //////////////////////////////////////////////////////////////

/**
* Gets a string fragment if an obj is shared (by someone other than ch). This
* is used by enchanting and some other commands, in their success strings. It
* has an $N in the string, so pass obj->worn_by as the 2nd char in your act()
* message.
*
* @param obj_data *obj The obj that might be shared.
* @param char_data *ch The person using the ablitiy, who will be ignored for this message.
* @return char* Either an empty string, or " (shared by $N)".
*/
char *shared_by(obj_data *obj, char_data *ch) {
	if (obj->worn_on == WEAR_SHARE && obj->worn_by && (!ch || obj->worn_by != ch)) {
		return " (shared by $N)";
	}
	else {
		return "";
	}
}


/**
* Simple, short display for number of days/hours/minutes/seconds since an
* event. It only shows the largest 2 of those groups, so something 26 hours ago
* is '1d2h' and something 100 seconds ago is '1m40s'.
*
* @param time_t when The timestamp.
* @return char* The short string.
*/
char *simple_time_since(time_t when) {
	static char output[80];
	int diff, parts;
	
	parts = 0;
	diff = time(0) - when;
	*output = '\0';
	
	if (diff > SECS_PER_REAL_YEAR && parts < 2) {
		sprintf(output + strlen(output), "%*dy", parts ? 1 : 2, (int)(diff / SECS_PER_REAL_YEAR));
		diff %= SECS_PER_REAL_YEAR;
		++parts;
	}
	if (diff > SECS_PER_REAL_WEEK && parts < 2) {
		sprintf(output + strlen(output), "%*dw", parts ? 1 : 2, (int)(diff / SECS_PER_REAL_WEEK));
		diff %= SECS_PER_REAL_WEEK;
		++parts;
	}
	if (diff > SECS_PER_REAL_DAY && parts < 2) {
		sprintf(output + strlen(output), "%*dd", parts ? 1 : 2, (int)(diff / SECS_PER_REAL_DAY));
		diff %= SECS_PER_REAL_DAY;
		++parts;
	}
	if (diff > SECS_PER_REAL_HOUR && parts < 2) {
		sprintf(output + strlen(output), "%*dh", parts ? 1 : 2, (int)(diff / SECS_PER_REAL_HOUR));
		diff %= SECS_PER_REAL_HOUR;
		++parts;
	}
	if (diff > SECS_PER_REAL_MIN && parts < 2) {
		sprintf(output + strlen(output), "%*dm", parts ? 1 : 2, (int)(diff / SECS_PER_REAL_MIN));
		diff %= SECS_PER_REAL_MIN;
		++parts;
	}
	if (diff > 0 && parts < 2) {
		sprintf(output + strlen(output), "%*ds", parts ? 1 : 2, diff);
		++parts;
	}
	
	return output;
}


/**
* @return unsigned long long The current timestamp as microtime (1 million per second)
*/
unsigned long long microtime(void) {
	struct timeval time;
	
	gettimeofday(&time, NULL);
	return ((unsigned long long)time.tv_sec * 1000000) + time.tv_usec;
}


/**
* Determines if a room both has a function flag, and passes any necessary
* in-city requirements. (If the room does not have BLD_IN_CITY_ONLY, this only
* checks the function.)
*
* @param room_data *room The room to check.
* @param bitvector_t fnc_flag Any FNC_ flag.
* @return bool TRUE if the room has the function and passed the city check, FALSE if not.
*/
bool room_has_function_and_city_ok(room_data *room, bitvector_t fnc_flag) {
	vehicle_data *veh;
	bool junk;
	
	// check vehicles first
	LL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (!VEH_IS_COMPLETE(veh)) {
			continue;
		}
		if (VEH_FLAGGED(veh, MOVABLE_VEH_FLAGS) && IS_SET(fnc_flag, IMMOBILE_FNCS)) {
			continue;	// exclude certain functions on movable vehicles (functions that require room data)
		}
		
		if (IS_SET(VEH_FUNCTIONS(veh), fnc_flag)) {
			if (!IS_SET(VEH_FUNCTIONS(veh), FNC_IN_CITY_ONLY) || (ROOM_OWNER(room) && get_territory_type_for_empire(room, ROOM_OWNER(room), TRUE, &junk) == TER_CITY)) {
				return TRUE;	// vehicle allows it
			}
		}
	}
	
	// otherwise check the room itself
	if (!HAS_FUNCTION(room, fnc_flag) || !IS_COMPLETE(room)) {
		return FALSE;
	}
	if (!check_in_city_requirement(room, TRUE)) {
		return FALSE;
	}
	
	// oh okay
	return TRUE;
}


/**
* This re-usable function is for making tweaks to all players, for example
* removing a player flag that's no longer used. It loads all players who aren't
* already in-game.
*
* Put your desired updates into update_one_player(). The players will be
* loaded and saved automatically.
*
* @param char_data *to_message Optional: a character to send error messages to, if this is called from a command.
* @param PLAYER_UPDATE_FUNC(*func)  A function pointer for the function to run on each player.
*/
void update_all_players(char_data *to_message, PLAYER_UPDATE_FUNC(*func)) {
	char buf[MAX_STRING_LENGTH];
	player_index_data *index, *next_index;
	descriptor_data *desc;
	char_data *ch;
	bool is_file;
	
	// verify there are no characters at menus
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (STATE(desc) != CON_PLAYING && desc->character != NULL) {
			strcpy(buf, "update_all_players: Unable to update because of connection(s) at the menu. Try again later.");
			if (to_message) {
				msg_to_char(to_message, "%s\r\n", buf);
			}
			else {
				log("SYSERR: %s", buf);
			}
			// abort!
			return;
		}
	}
	
	// verify there are no disconnected players characters in-game, which might not be saved
	for (ch = character_list; ch; ch = ch->next) {
		if (!IS_NPC(ch) && !ch->desc) {
			sprintf(buf, "update_all_players: Unable to update because of linkdead player (%s). Try again later.", GET_NAME(ch));
			if (to_message) {
				msg_to_char(to_message, "%s\r\n", buf);
			}
			else {
				log("SYSERR: %s", buf);
			}
			// abort!
			return;
		}
	}

	// ok, ready to roll
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
		if (!(ch = find_or_load_player(index->name, &is_file))) {
			continue;
		}
		
		if (func) {
			(func)(ch, is_file);
		}
		
		// save
		if (is_file) {
			store_loaded_char(ch);
			is_file = FALSE;
			ch = NULL;
		}
		else {
			SAVE_CHAR(ch);
		}
	}
}
