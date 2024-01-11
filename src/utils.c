/* ************************************************************************
*   File: utils.c                                         EmpireMUD 2.0b5 *
*  Usage: various internal functions of a utility nature                  *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#include "dg_event.h"
#include "dg_scripts.h"
#include "constants.h"

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
*   Component Utils
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

// local prototypes
void score_empires();
void unmark_items_for_char(char_data *ch, bool ground);

#define WHITESPACE " \t"	// used by some of the string functions


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
	
	DL_FOREACH2(ROOM_PEOPLE(room), ch, next_in_room) {
		if (!IS_NPC(ch)) {
			found = TRUE;
			break;
		}
	}
	
	return found;
}


/**
* Applies bonus traits whose effects are one-time-only.
*
* @param char_data *ch The player to apply the trait to.
* @param bitvector_t trait Any BONUS_ bit.
* @param bool add If TRUE, we are adding the trait; FALSE means removing it;
*/
void apply_bonus_trait(char_data *ch, bitvector_t trait, bool add) {	
	int amt = (add ? 1 : -1);
	
	if (IS_SET(trait, BONUS_EXTRA_DAILY_SKILLS)) {
		GET_DAILY_BONUS_EXPERIENCE(ch) = MAX(0, GET_DAILY_BONUS_EXPERIENCE(ch) + (config_get_int("num_bonus_trait_daily_skills") * amt));
		update_MSDP_bonus_exp(ch, UPDATE_SOON);
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

	for (k = victim; k; k = GET_LEADER(k)) {
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
	int hours;
	int count = 0;	// number of traits to give
	
	if (IS_NPC(ch)) {
		return 0;
	}
	
	// extra point at 2 days playtime
	t = *real_time_passed((time(0) - ch->player.time.logon) + ch->player.time.played, 0);
	hours = t.day * 24 + t.hours;
	if (hours >= config_get_int("hours_to_first_bonus_trait")) {
		++count;
	}
	if (hours >= config_get_int("hours_to_second_bonus_trait")) {
		++count;
	}

	return count;
}


/* creates a random number in interval [from;to] */
int number(int from, int to) {
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
	if (!IS_NPC(ch) && GET_CURRENT_LASTNAME(ch)) {
		sprintf(output + strlen(output), " %s", GET_CURRENT_LASTNAME(ch));
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
* Cancels a requested refresh on 1 or more empires.
*
* @param empire_data *only_emp Optional: Only remove from one empire (default: NULL = all)
* @param bitvector_t refresh_flag The DELAY_REFRESH_ flag(s) to remove.
*/
void clear_delayed_empire_refresh(empire_data *only_emp, bitvector_t refresh_flag) {
	empire_data *emp, *next_emp;
	
	if (only_emp) {
		REMOVE_BIT(EMPIRE_DELAYED_REFRESH(only_emp), refresh_flag);
	}
	else {
		HASH_ITER(hh, empire_table, emp, next_emp) {
			REMOVE_BIT(EMPIRE_DELAYED_REFRESH(emp), refresh_flag);
		}
	}
}


/**
* Checks players and empires for delayed-refresh commands.
*/
void run_delayed_refresh(void) {
	struct char_delayed_update *cdu, *next_cdu;
	
	// player portion
	HASH_ITER(hh, char_delayed_update_list, cdu, next_cdu) {
		// CDU_x: functionality for delayed update types
		if (IS_SET(cdu->type, CDU_TRAIT_HOOKS)) {
			check_trait_hooks(cdu->ch);
			REMOVE_BIT(cdu->type, CDU_TRAIT_HOOKS);
		}
		if (IS_SET(cdu->type, CDU_PASSIVE_BUFFS)) {
			refresh_passive_buffs(cdu->ch);
			REMOVE_BIT(cdu->type, CDU_PASSIVE_BUFFS);
		}
		
		// MSDP sections
		if (IS_SET(cdu->type, CDU_MSDP_AFFECTS)) {
			update_MSDP_affects(cdu->ch, UPDATE_SOON);
			REMOVE_BIT(cdu->type, CDU_MSDP_AFFECTS);
		}
		if (IS_SET(cdu->type, CDU_MSDP_ATTRIBUTES)) {
			update_MSDP_attributes(cdu->ch, UPDATE_SOON);
			REMOVE_BIT(cdu->type, CDU_MSDP_ATTRIBUTES);
		}
		if (IS_SET(cdu->type, CDU_MSDP_COOLDOWNS)) {
			update_MSDP_cooldowns(cdu->ch, UPDATE_SOON);
			REMOVE_BIT(cdu->type, CDU_MSDP_COOLDOWNS);
		}
		if (IS_SET(cdu->type, CDU_MSDP_DOTS)) {
			update_MSDP_dots(cdu->ch, UPDATE_SOON);
			REMOVE_BIT(cdu->type, CDU_MSDP_DOTS);
		}
		if (IS_SET(cdu->type, CDU_MSDP_EMPIRE_ALL)) {
			update_MSDP_empire_data(cdu->ch, UPDATE_SOON);
			REMOVE_BIT(cdu->type, CDU_MSDP_EMPIRE_ALL);
		}
		if (IS_SET(cdu->type, CDU_MSDP_EMPIRE_CLAIMS)) {
			update_MSDP_empire_claims(cdu->ch, UPDATE_SOON);
			REMOVE_BIT(cdu->type, CDU_MSDP_EMPIRE_CLAIMS);
		}
		if (IS_SET(cdu->type, CDU_MSDP_SKILLS)) {
			update_MSDP_skills(cdu->ch, UPDATE_SOON);
			REMOVE_BIT(cdu->type, CDU_MSDP_SKILLS);
		}
		
		// do this after the MSDP section
		if (IS_SET(cdu->type, CDU_MSDP_SEND_UPDATES)) {
			if (cdu->ch->desc) {
				MSDPUpdate(cdu->ch->desc);
			}
			REMOVE_BIT(cdu->type, CDU_MSDP_SEND_UPDATES);
		}
		
		// do this one last (anything above may be save-able)
		if (IS_SET(cdu->type, CDU_SAVE)) {
			SAVE_CHAR(cdu->ch);
			REMOVE_BIT(cdu->type, CDU_SAVE);
		}
		
		// remove only if no bits remain (sometimes more bits get added while running)
		if (!cdu->type) {
			HASH_DEL(char_delayed_update_list, cdu);
			free(cdu);
		}
	}

	
	// empire portion
	if (check_empire_refresh) {
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
			if (IS_SET(EMPIRE_DELAYED_REFRESH(emp), DELAY_REFRESH_GREATNESS)) {
				update_empire_members_and_greatness(emp);
			}
			
			// clear this
			EMPIRE_DELAYED_REFRESH(emp) = NOBITS;
		}
		
		check_empire_refresh = FALSE;
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
				if (store->amount > 0) {
					SAFE_ADD(num, store->amount, 0, MAX_INT, FALSE);
				}
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
	struct partner_list_type *plt, *next_plt, *partner_list = NULL;
	struct import_pair_type *pair, *next_pair, *pair_list;
	int my_amt, their_amt, trade_amt, found_island = NO_ISLAND;
	struct empire_trade_data *trade, *p_trade;
	empire_data *partner, *next_partner;
	int limit = config_get_int("imports_per_day");
	bool any = FALSE;
	obj_data *orn;
	double cost, gain;
	
	// round to %.001f
	#define IMPORT_ROUND(amt)  (round((amt) * 1000.0) / 1000.0)
	
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
			if (IMPORT_ROUND(p_trade->cost * (1.0/plt->rate)) > trade->cost) {
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
				pair->cost = IMPORT_ROUND(p_trade->cost * (1.0/plt->rate));
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
			
			// round off to 0.1 now (up)
			cost = ceil(cost * 10.0) / 10.0;
			
			// can we afford it?
			if (EMPIRE_COINS(emp) < cost) {
				trade_amt = EMPIRE_COINS(emp) / pair->cost;	// reduce to how many we can afford
				cost = ceil(trade_amt * pair->cost * 10.0) / 10.0;
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
				gain = cost * pair->rate;
				increase_empire_coins(pair->emp, pair->emp, gain);
				
				// update limit
				limit -= trade_amt;
				my_amt += trade_amt;
				any = TRUE;
				
				// log
				orn = obj_proto(trade->vnum);
				log_to_empire(emp, ELOG_TRADE, "Imported %s x%d from %s for %.1f coins", GET_OBJ_SHORT_DESC(orn), trade_amt, EMPIRE_NAME(pair->emp), cost);
				log_to_empire(pair->emp, ELOG_TRADE, "Exported %s x%d to %s for %.1f coins", GET_OBJ_SHORT_DESC(orn), trade_amt, EMPIRE_NAME(emp), gain);
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
* is no player leader.
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
	while (IS_NPC(top_a) && GET_LEADER(top_a)) {
		top_a = GET_LEADER(top_a);
	}
	top_b = ch_b;
	while (IS_NPC(top_b) && GET_LEADER(top_b)) {
		top_b = GET_LEADER(top_b);
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
	
	if (!loc || !ignore_distrustful_due_to_start_loc(loc)) {
		if ((!loc && IS_SET(EMPIRE_FRONTIER_TRAITS(emp), ETRAIT_DISTRUSTFUL)) || has_empire_trait(emp, loc, ETRAIT_DISTRUSTFUL)) {
			distrustful = TRUE;
		}
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
* This function checks the "min_distrustful_distance" config, which is the
* distance from a starting location where the distrustful flag stops working.
* Anybody closer is proteted from distrustful-based attacks.
*
* @param room_data *loc The location to test.
* @return bool TRUE if this location is too close to a starting location; FALSE if not.
*/
bool ignore_distrustful_due_to_start_loc(room_data *loc) {;
	int safe_distance = config_get_int("min_distrustful_distance");
	int iter;
	
	// short-circuit if this feature is turned off
	if (safe_distance == 0) {
		return FALSE;
	}
	
	for (iter = 0; iter <= highest_start_loc_index; ++iter) {
		if (compute_distance(loc, real_room(start_locs[iter])) < safe_distance) {
			return TRUE;
		}
	}
	
	return FALSE;	// didn't find any start locs in range
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
	
	if (mode == NOTHING) {
		return TRUE;	// nothing asked, nothing checked
	}
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
	if (mode == NOTHING) {
		return TRUE;	// nothing asked, nothing checked
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
	
	if (mode == NOTHING) {
		return TRUE;	// nothing asked, nothing checked
	}
	// no owner?
	if (!VEH_OWNER(veh)) {
		return TRUE;
	}
	// empire ownership
	if (VEH_OWNER(veh) == emp) {
		return TRUE;
	}
	// public + guests: use interior room to determine publicness
	if (interior && ROOM_AFF_FLAGGED(interior, ROOM_AFF_PUBLIC) && mode == GUESTS_ALLOWED) {
		return TRUE;
	}
	// no interior + guests: use the tile it's on, if the owner is the same as the vehicle
	if (!interior && IN_ROOM(veh) && ROOM_OWNER(IN_ROOM(veh)) == VEH_OWNER(veh) && ROOM_AFF_FLAGGED(IN_ROOM(veh), ROOM_AFF_PUBLIC) && mode == GUESTS_ALLOWED) {
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
* @param int mode PLR_FILE, DELAYED_FILE, MAP_MEMORY_FILE, DELETED_PLR_FILE, DELETED_DELAYED_FILE
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
		case MAP_MEMORY_FILE: {
			prefix = LIB_PLAYERS;
			suffix = SUF_MEMORY;
			break;
		}
		case DELETED_PLR_FILE: {
			prefix = LIB_PLAYERS_DELETED;
			suffix = SUF_PLR;
			break;
		}
		case DELETED_DELAYED_FILE: {
			prefix = LIB_PLAYERS_DELETED;
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
	struct empire_log_data *elog;
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
		DL_APPEND(EMPIRE_LOGS(emp), elog);
		EMPIRE_NEEDS_LOGS_SAVE(emp) = TRUE;
	}
	
	// show to players
	if (show_empire_log_type[type] == TRUE) {
		for (i = descriptor_list; i; i = i->next) {
			if (STATE(i) != CON_PLAYING || IS_NPC(i->character)) {
				continue;
			}
			if (GET_LOYALTY(i->character) != emp) {
				continue;	// wrong empire
			}
			if (!SHOW_STATUS_MESSAGES(i->character, SM_EMPIRE_LOGS)) {
				continue;	// elogs off
			}

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
		if (STATE(i) == CON_PLAYING && i->character && SHOW_STATUS_MESSAGES(i->character, SM_MORTLOG)) {
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
//// COMPONENT UTILS /////////////////////////////////////////////////////////

/**
* Finds a generic component by name or vnum.
*
* @param char *name The name to search.
* @return generic_data* The generic component, or NULL if it doesn't exist.
*/
generic_data *find_generic_component(char *name) {
	generic_data *gen, *next_gen, *abbrev = NULL;
	
	skip_spaces(&name);
	
	if (is_number(name)) {
		return find_generic(atoi(name), GENERIC_COMPONENT);
	}
	
	// not number
	HASH_ITER(hh, generic_table, gen, next_gen) {
		if (GEN_TYPE(gen) != GENERIC_COMPONENT) {
			continue;
		}
		
		if (!str_cmp(name, GEN_NAME(gen)) || (GET_COMPONENT_PLURAL(gen) && !str_cmp(name, GET_COMPONENT_PLURAL(gen)))) {
			return gen;	// exact match
		}
		else if (!abbrev && (multi_isname(name, GEN_NAME(gen)) || (GET_COMPONENT_PLURAL(gen) && multi_isname(name, GET_COMPONENT_PLURAL(gen))))) {
			abbrev = gen;	// partial match
		}
	}
	
	return abbrev;	// if any
}


/**
* Determines if an object is a 'basic' type of component -- that is, it is a
* generic component and that component is marked GEN_BASIC.
*
* @param obj_data *obj The object to test.
* @return bool TRUE if obj is a component and that component is basic; FALSE otherwise.
*/
bool is_basic_component(obj_data *obj) {
	generic_data *gen;
	if (obj && GET_OBJ_COMPONENT(obj) != NOTHING && (gen = real_generic(GET_OBJ_COMPONENT(obj))) && GEN_TYPE(gen) == GENERIC_COMPONENT && GEN_FLAGGED(gen, GEN_BASIC)) {
		return TRUE;
	}
	return FALSE;	// all other cases
}


/**
* Determines if an objext is a certain type of generic component -- or any
* of its related types. It does not indicate WHICH of those types it is.
*
* @param obj_data *obj The object to check.
* @param generic_data *cmp The generic component to compare it to.
* @return bool TRUE if obj matches cmp (or one of its related types); FALSE if not
*/
bool is_component(obj_data *obj, generic_data *cmp) {
	generic_data *my_cmp;
	
	if (!obj || !cmp || GEN_TYPE(cmp) != GENERIC_COMPONENT) {
		return FALSE;	// basic sanity
	}
	if (GET_OBJ_COMPONENT(obj) == GEN_VNUM(cmp)) {
		return TRUE;	// exact match = SUCCESS
	}
	if (GET_OBJ_COMPONENT(obj) == NOTHING || !(my_cmp = real_generic(GET_OBJ_COMPONENT(obj)))) {
		return FALSE;	// not a component at all = fail
	}
	
	// otherwise check dependent types
	return has_generic_relation(GEN_COMPUTED_RELATIONS(my_cmp), GEN_VNUM(cmp));
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
* Breaks a string into two parts: the last word, and the rest of the words.
*
* @param char *string The input string.
* @param char *most_args All the words in 'string' except the last one (empty if string was empty).
* @param char *last_arg The last word in 'string' IF it had more than one.
*/
void chop_last_arg(char *string, char *most_args, char *last_arg) {
	int end_pos;
	
	*most_args = *last_arg = '\0';
	
	if (!string || !*string) {
		return;	// no work
	}
	
	// first trim trailing spaces
	for (end_pos = strlen(string) - 1; end_pos >= 0 && isspace(string[end_pos]); --end_pos) {
		string[end_pos] = '\0';
	}
	
	// end_pos is still the end of the string: now rewind to look for previous space
	for (; end_pos >= 0 && !isspace(string[end_pos]); --end_pos);
	
	// did we find it?
	if (end_pos > 0) {
		strcpy(last_arg, string + end_pos + 1);
		strncpy(most_args, string, end_pos);
		most_args[end_pos] = '\0';
		
		// then trim trailing spaces (in case of multiple spaces between args)
		for (--end_pos; end_pos >= 0 && isspace(most_args[end_pos]); --end_pos) {
			most_args[end_pos] = '\0';
		}
	}
	else {	// didn't find a space: probably only 1 arg
		strcpy(most_args, string);
	}
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
	
	if (!string) {
		*arg1 = *arg2 = '\0';
		return;
	}
	
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
* This is similar to one_word() in that it checks for quotes around the args,
* but different in that unquoted text will return the whole argument, not just
* the first word.
*
* @param char *argument The incoming argument(s).
* @param char *found_arg The variable where the detected argument will be stored.
* @return char* A pointer to whatever remains of argument.
*/
char *quoted_arg_or_all(char *argument, char *found_arg) {
	skip_spaces(&argument);
	if (*argument == '\"') {
		// found quote: pull whole thing
		argument++;
		while (*argument && *argument != '\"') {
			*(found_arg++) = *argument;
			argument++;
		}
		*found_arg = '\0';
	}
	else {
		// no quote: pull the entire arg
		strcpy(found_arg, argument);
		
		// and advance argument to the very end
		while (*argument) {
			++argument;
		}
	}
	
	return (argument);
}


/**
* @param char *argument The argument to test.
* @return int TRUE if the argument is in the reserved_words[] list.
*/
int reserved_word(char *argument) {
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

	for (i = 0; **(list + i) != '\n'; ++i) {
		if (!str_cmp(arg, *(list + i))) {
			return i;	// exact or otherwise
		}
		else if (!exact && part == NOTHING && !strn_cmp(arg, *(list + i), l)) {
			part = i;	// found partial but keep searching
		}
	}

	return part;	// if any
}


/**
* Variant of search_block that uses multi_isname for matches. This simpilfies
* typing some text, especially in OLC. It still prefers exact matches.
*
* @param char *arg The input.
* @param const char **list A "\n"-terminated name list.
* @return int The entry in the list, or NOTHING if not found.
*/
int search_block_multi_isname(char *arg, const char **list) {
	int iter, partial = NOTHING;
	
	for (iter = 0; **(list + iter) != '\n'; ++iter) {
		if (!str_cmp(arg, *(list + iter))) {
			return iter;	// exact match
		}
		else if (partial == NOTHING && multi_isname(arg, *(list + iter))) {
			partial = iter;
		}
	}
	
	return partial;	// if any
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
* Processes a change to a mob's keywords. This may also update it in
* additional places, such as stored companion data.
*
* @parma char_data *ch The mob.
* @param char *str The new keywords (will be copied). Pass NULL to set it back to the prototype.
*/
void change_keywords(char_data *ch, char *str) {
	struct companion_data *cd;
	char_data *proto;
	
	if (!ch || !IS_NPC(ch)) {
		return;	// no work
	}
	
	proto = mob_proto(GET_MOB_VNUM(ch));
	
	// the change
	if (str && *str) {
		ch->customized = TRUE;
		if (!proto || GET_PC_NAME(ch) != GET_PC_NAME(proto)) {
			free(GET_PC_NAME(ch));
		}
		GET_PC_NAME(ch) = str_dup(str);
		
		// update companion data
		if (GET_COMPANION(ch) && (cd = has_companion(GET_COMPANION(ch), GET_MOB_VNUM(ch)))) {
			add_companion_mod(cd, CMOD_KEYWORDS, 0, str);
			queue_delayed_update(GET_COMPANION(ch), CDU_SAVE);
		}
	}
	else if (proto) {	// reset to proto
		if (GET_PC_NAME(ch) && GET_PC_NAME(ch) != GET_PC_NAME(proto)) {
			free(GET_PC_NAME(ch));
		}
		GET_PC_NAME(ch) = GET_PC_NAME(proto);
		
		// delete companion data
		if (GET_COMPANION(ch) && (cd = has_companion(GET_COMPANION(ch), GET_MOB_VNUM(ch)))) {
			remove_companion_mod(&cd, CMOD_KEYWORDS);
			queue_delayed_update(GET_COMPANION(ch), CDU_SAVE);
		}
	}
	
	request_char_save_in_world(ch);
}


/**
* Processes a change to a mob's long desc. This may also update it in
* additional places, such as stored companion data.
*
* @parma char_data *ch The mob.
* @param char *str The new long desc (will be copied). Pass NULL to set it back to the prototype.
*/
void change_long_desc(char_data *ch, char *str) {
	struct companion_data *cd;
	char_data *proto;
	
	if (!ch || !IS_NPC(ch)) {
		return;	// no work
	}
	
	proto = mob_proto(GET_MOB_VNUM(ch));
	
	// the change
	if (str && *str) {
		ch->customized = TRUE;
		if (!proto || GET_LONG_DESC(ch) != GET_LONG_DESC(proto)) {
			free(GET_LONG_DESC(ch));
		}
		GET_LONG_DESC(ch) = str_dup(str);
		
		// update companion data
		if (GET_COMPANION(ch) && (cd = has_companion(GET_COMPANION(ch), GET_MOB_VNUM(ch)))) {
			add_companion_mod(cd, CMOD_LONG_DESC, 0, str);
			queue_delayed_update(GET_COMPANION(ch), CDU_SAVE);
		}
	}
	else if (proto) {	// reset to proto
		if (GET_LONG_DESC(ch) && GET_LONG_DESC(ch) != GET_LONG_DESC(proto)) {
			free(GET_LONG_DESC(ch));
		}
		GET_LONG_DESC(ch) = GET_LONG_DESC(proto);
		
		// delete companion data
		if (GET_COMPANION(ch) && (cd = has_companion(GET_COMPANION(ch), GET_MOB_VNUM(ch)))) {
			remove_companion_mod(&cd, CMOD_LONG_DESC);
			queue_delayed_update(GET_COMPANION(ch), CDU_SAVE);
		}
	}
	
	request_char_save_in_world(ch);
}


/**
* Processes a change to a mob's look desc. This may also update it in
* additional places, such as stored companion data.
*
* @parma char_data *ch The mob.
* @param char *str The new look desc (will be copied). Pass NULL to set it back to the prototype.
* @param bool format If TRUE, formats the whole description as a paragraph.
*/
void change_look_desc(char_data *ch, char *str, bool format) {
	struct companion_data *cd;
	char_data *proto;
	
	if (!ch || !IS_NPC(ch)) {
		return;	// no work
	}
	
	proto = mob_proto(GET_MOB_VNUM(ch));
	
	// the change
	if (str && *str) {
		ch->customized = TRUE;
		if (!proto || GET_LOOK_DESC(ch) != GET_LOOK_DESC(proto)) {
			free(GET_LOOK_DESC(ch));
		}
		
		// check for trailing \r\n
		if (str[strlen(str)-1] == '\n') {
			GET_LOOK_DESC(ch) = str_dup(str);
		}
		else {
			char temp[MAX_STRING_LENGTH];
			snprintf(temp, sizeof(temp), "%s\r\n", str);
			GET_LOOK_DESC(ch) = str_dup(temp);
		}
		
		if (format) {
			format_text(&GET_LOOK_DESC(ch), (strlen(GET_LOOK_DESC(ch)) > 80 ? FORMAT_INDENT : 0), NULL, MAX_STRING_LENGTH);
		}
		
		// update companion data
		if (GET_COMPANION(ch) && (cd = has_companion(GET_COMPANION(ch), GET_MOB_VNUM(ch)))) {
			add_companion_mod(cd, CMOD_LOOK_DESC, 0, str);
			queue_delayed_update(GET_COMPANION(ch), CDU_SAVE);
		}
	}
	else if (proto) {	// reset to proto
		if (GET_LOOK_DESC(ch) && GET_LOOK_DESC(ch) != GET_LOOK_DESC(proto)) {
			free(GET_LOOK_DESC(ch));
		}
		GET_LOOK_DESC(ch) = GET_LOOK_DESC(proto);
		
		// delete companion data
		if (GET_COMPANION(ch) && (cd = has_companion(GET_COMPANION(ch), GET_MOB_VNUM(ch)))) {
			remove_companion_mod(&cd, CMOD_LOOK_DESC);
			queue_delayed_update(GET_COMPANION(ch), CDU_SAVE);
		}
	}
	
	request_char_save_in_world(ch);
}


/**
* Processes a change to a mob's look desc -- it will append text to the
* existing text. This may also update it in additional places, such as stored
* companion data.
*
* @parma char_data *ch The mob.
* @param char *str The new look desc to append (text will be copied).
* @param bool format If TRUE, formats the whole description as a paragraph.
*/
void change_look_desc_append(char_data *ch, char *str, bool format) {
	char temp[MAX_STRING_LENGTH];
	struct companion_data *cd;
	char_data *proto;
	
	if (!ch || !IS_NPC(ch) || !str || !*str) {
		return;	// no work
	}
	
	proto = mob_proto(GET_MOB_VNUM(ch));
	
	// the change
	if (str && *str) {
		snprintf(temp, sizeof(temp), "%s%s%s", NULLSAFE(GET_LOOK_DESC(ch)), NULLSAFE(str), (str[strlen(str)-1] == '\n' ? "" : "\r\n"));

		ch->customized = TRUE;
		if (!proto || GET_LOOK_DESC(ch) != GET_LOOK_DESC(proto)) {
			free(GET_LOOK_DESC(ch));
		}
		GET_LOOK_DESC(ch) = str_dup(temp);
		
		if (format) {
			format_text(&GET_LOOK_DESC(ch), (strlen(GET_LOOK_DESC(ch)) > 80 ? FORMAT_INDENT : 0), NULL, MAX_STRING_LENGTH);
		}
		
		// update companion data
		if (GET_COMPANION(ch) && (cd = has_companion(GET_COMPANION(ch), GET_MOB_VNUM(ch)))) {
			add_companion_mod(cd, CMOD_LOOK_DESC, 0, str);
			queue_delayed_update(GET_COMPANION(ch), CDU_SAVE);
		}
	}
	
	request_char_save_in_world(ch);
}


/**
* Processes a change to a character/mob's sex. This may also update it in
* additional places, such as stored companion data.
*
* @param char_data *ch The player or NPC to change.
* @param int sex The new sex.
*/
void change_sex(char_data *ch, int sex) {
	struct companion_data *cd;
	
	if (!ch || sex < 0 || sex >= NUM_GENDERS) {
		return;	// no work
	}
	
	// the change
	if (sex != GET_REAL_SEX(ch)) {
		ch->customized = TRUE;
	}
	GET_REAL_SEX(ch) = sex;
	
	// update companion data
	if (IS_NPC(ch) && GET_COMPANION(ch) && (cd = has_companion(GET_COMPANION(ch), GET_MOB_VNUM(ch)))) {
		add_companion_mod(cd, CMOD_SEX, sex, NULL);
		queue_delayed_update(GET_COMPANION(ch), CDU_SAVE);
	}
	
	// update msdp
	update_MSDP_gender(ch, UPDATE_SOON);
	request_char_save_in_world(ch);
}


/**
* Processes a change to a mob's short desc. This may also update it in
* additional places, such as stored companion data.
*
* @parma char_data *ch The mob.
* @param char *str The new short desc (will be copied). Pass NULL to set it back to the prototype.
*/
void change_short_desc(char_data *ch, char *str) {
	struct companion_data *cd;
	char_data *proto;
	
	if (!ch || !IS_NPC(ch)) {
		return;	// no work
	}
	
	proto = mob_proto(GET_MOB_VNUM(ch));
	
	// the change
	if (str && *str) {
		ch->customized = TRUE;
		if (!proto || GET_SHORT_DESC(ch) != GET_SHORT_DESC(proto)) {
			free(GET_SHORT_DESC(ch));
		}
		GET_SHORT_DESC(ch) = str_dup(str);
		
		// update companion data
		if (GET_COMPANION(ch) && (cd = has_companion(GET_COMPANION(ch), GET_MOB_VNUM(ch)))) {
			add_companion_mod(cd, CMOD_SHORT_DESC, 0, str);
			queue_delayed_update(GET_COMPANION(ch), CDU_SAVE);
		}
	}
	else if (proto) {	// reset to proto
		if (GET_SHORT_DESC(ch) && GET_SHORT_DESC(ch) != GET_SHORT_DESC(proto)) {
			free(GET_SHORT_DESC(ch));
		}
		GET_SHORT_DESC(ch) = GET_SHORT_DESC(proto);
		
		// delete companion data
		if (GET_COMPANION(ch) && (cd = has_companion(GET_COMPANION(ch), GET_MOB_VNUM(ch)))) {
			remove_companion_mod(&cd, CMOD_SHORT_DESC);
			queue_delayed_update(GET_COMPANION(ch), CDU_SAVE);
		}
	}
	
	request_char_save_in_world(ch);
}


/**
* Despawns all companionss and charmies a player has. This is usually called
* upon player death.
*
* @param char_data *ch The person whose followers to despawn.
* @param any_vnum only_vnum Optional: Only despawn a specific mob vnum (pass NOTHING for all charmies).
*/
void despawn_charmies(char_data *ch, any_vnum only_vnum) {
	char_data *iter, *next_iter;
	
	DL_FOREACH_SAFE(character_list, iter, next_iter) {
		if (IS_NPC(iter) && GET_LEADER(iter) == ch && (only_vnum == NOTHING || GET_MOB_VNUM(iter) == only_vnum)) {
			if (GET_COMPANION(iter) == ch || AFF_FLAGGED(iter, AFF_CHARM)) {
				if (!AFF_FLAGGED(iter, AFF_HIDDEN | AFF_NO_SEE_IN_ROOM)) {
					act("$n leaves.", TRUE, iter, NULL, NULL, TO_ROOM);
				}
				extract_char(iter);
			}
		}
	}
}


/**
* Fetches the current attribute from a character based on an APPLY_ type. For
* example, APPLY_STRENGTH fetches the character's strength.
*
* @param char_data *ch The character.
* @param int apply_type Any APPLY_ const.
* @return int The character's value for that attributes.
*/
int get_attribute_by_apply(char_data *ch, int apply_type) {
	if (!ch) {
		return 0;	// shortcut/safety
	}
	
	// APPLY_x
	switch (apply_type) {
		case APPLY_STRENGTH: {
			return GET_STRENGTH(ch);
		}
		case APPLY_DEXTERITY: {
			return GET_DEXTERITY(ch);
		}
		case APPLY_HEALTH_REGEN: {
			return health_gain(ch, TRUE);
		}
		case APPLY_CHARISMA: {
			return GET_CHARISMA(ch);
		}
		case APPLY_GREATNESS: {
			return GET_GREATNESS(ch);
		}
		case APPLY_MOVE_REGEN: {
			return move_gain(ch, TRUE);
		}
		case APPLY_MANA_REGEN: {
			return mana_gain(ch, TRUE);
		}
		case APPLY_INTELLIGENCE: {
			return GET_INTELLIGENCE(ch);
		}
		case APPLY_WITS: {
			return GET_WITS(ch);
		}
		case APPLY_AGE: {
			return GET_AGE(ch);
		}
		case APPLY_MOVE: {
			return GET_MAX_MOVE(ch);
		}
		case APPLY_RESIST_PHYSICAL: {
			return GET_RESIST_PHYSICAL(ch);
		}
		case APPLY_BLOCK: {
			return get_block_rating(ch, FALSE);
		}
		case APPLY_HEAL_OVER_TIME: {
			return GET_HEAL_OVER_TIME(ch);
		}
		case APPLY_HEALTH: {
			return GET_MAX_HEALTH(ch);
		}
		case APPLY_MANA: {
			return GET_MAX_MANA(ch);
		}
		case APPLY_TO_HIT: {
			return get_to_hit(ch, NULL, FALSE, FALSE);
		}
		case APPLY_DODGE: {
			return get_dodge_modifier(ch, NULL, FALSE);
		}
		case APPLY_INVENTORY: {
			return CAN_CARRY_N(ch);
		}
		case APPLY_BLOOD: {
			return GET_MAX_BLOOD(ch);
		}
		case APPLY_BONUS_PHYSICAL: {
			return GET_BONUS_PHYSICAL(ch);
		}
		case APPLY_BONUS_MAGICAL: {
			return GET_BONUS_MAGICAL(ch);
		}
		case APPLY_BONUS_HEALING: {
			return GET_BONUS_HEALING(ch);
		}
		case APPLY_RESIST_MAGICAL: {
			return GET_RESIST_MAGICAL(ch);
		}
		case APPLY_CRAFTING: {
			return get_crafting_level(ch);
		}
		case APPLY_BLOOD_UPKEEP: {
			return GET_BLOOD_UPKEEP(ch);
		}
		case APPLY_NIGHT_VISION: {
			return GET_EXTRA_ATT(ch, ATT_NIGHT_VISION);
		}
		case APPLY_NEARBY_RANGE: {
			return GET_EXTRA_ATT(ch, ATT_NEARBY_RANGE);
		}
		case APPLY_WHERE_RANGE: {
			return GET_EXTRA_ATT(ch, ATT_WHERE_RANGE);
		}
		case APPLY_WARMTH: {
			return GET_EXTRA_ATT(ch, ATT_WARMTH);
		}
		case APPLY_COOLING: {
			return GET_EXTRA_ATT(ch, ATT_COOLING);
		}
	}
	return 0;	// if we got this far
}


/**
* Quick way to turn a vnum into a name, safely.
*
* @param mob_vnum vnum The vnum to look up.
* @param bool replace_placeholders If TRUE, will replace #n, #e, and #a with <name> etc
* @return char* A name for the vnum, or "UNKNOWN".
*/
char *get_mob_name_by_proto(mob_vnum vnum, bool replace_placeholders) {
	static char output[MAX_STRING_LENGTH];
	char_data *proto = mob_proto(vnum);
	char *unk = "UNKNOWN";
	char *tmp;
	
	if (!replace_placeholders || !proto || !strchr(GET_SHORT_DESC(proto), '#')) {
		// short way out
		return proto ? GET_SHORT_DESC(proto) : unk;
	}
	else {
		strcpy(output, GET_SHORT_DESC(proto));
		
		// #n name
		if (strstr(GET_SHORT_DESC(proto), "#n")) {
			tmp = str_replace("#n", "<name>", output);
			strcpy(output, tmp);
			free(tmp);
		}
		// #e empire name
		if (strstr(GET_SHORT_DESC(proto), "#e")) {
			tmp = str_replace("#e", "<empire>", output);
			strcpy(output, tmp);
			free(tmp);
		}
		// #a empire adjective
		if (strstr(GET_SHORT_DESC(proto), "#a")) {
			tmp = str_replace("#a", "<empire>", output);
			strcpy(output, tmp);
			free(tmp);
		}
		
		return output;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT UTILS ////////////////////////////////////////////////////////////

/**
* Count how many of a vnum there are in a set of objs.
*
* @param obj_vnum vnum The object to look for/count.
* @param obj_Data *list One or more objects in a "next_content" list.
*/
int count_objs_by_vnum(obj_vnum vnum, obj_data *list) {
	int count = 0;
	
	if (!list) {
		return 0;
	}
	if (GET_OBJ_VNUM(list) == vnum) {
		++count;
	}
	if (list->contains) {
		count += count_objs_by_vnum(vnum, list->contains);
	}
	if (list->next_content) {
		count += count_objs_by_vnum(vnum, list->next_content);
	}
	return count;
}


/**
* Finds a portal in the room with a given destination.
*
* @param room_data *room The room to start in.
* @param room_vnum to_room The destination to look for.
* @return obj_data* The portal that leads there, if any. Otherwise, NULL.
*/
obj_data *find_portal_in_room_targetting(room_data *room, room_vnum to_room) {
	obj_data *obj;
	
	DL_FOREACH2(ROOM_CONTENTS(room), obj, next_content) {
		if (IS_PORTAL(obj) && GET_PORTAL_TARGET_VNUM(obj) == to_room) {
			return obj;
		}
	}
	
	return NULL;
}


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
* Determines if an effect appears to be bad for you or not.
*
* @param struct affected_type *aff Which aff.
* @return bool TRUE if it's a good aff, FALSE if it's a bad one.
*/
bool affect_is_beneficial(struct affected_type *aff) {
	bitvector_t bitv;
	int pos;
	
	if (aff->location != APPLY_NONE && (apply_values[(int) aff->location] == 0.0 || aff->modifier < 0)) {
		return FALSE;	// bad apply
	}
	if ((bitv = aff->bitvector) != NOBITS) {
		// check each bit
		for (pos = 0; bitv; ++pos, bitv >>= 1) {
			if (IS_SET(bitv, BIT(0)) && aff_is_bad[pos]) {
				return FALSE;
			}
		}
	}
	
	// otherwise default to TRUE
	return TRUE;
}


/**
* If the character chose bonus health, moves, or mana during creation, adds or
* removes those now.
*
* TODO would like to replace this with special abilities, through the passive
* buff ability engine.
*
* @param char_data *ch The player.
* @param bool add If TRUE, adds the bonus. If FALSE, removes it.
*/
void apply_bonus_pools(char_data *ch, bool add) {
	if (!IS_NPC(ch)) {
		if (HAS_BONUS_TRAIT(ch, BONUS_HEALTH)) {
			affect_modify(ch, APPLY_HEALTH, (config_get_int("pool_bonus_amount") * (1 + (GET_HIGHEST_KNOWN_LEVEL(ch) / 25))), NOBITS, add);
		}
		if (HAS_BONUS_TRAIT(ch, BONUS_MOVES)) {
			affect_modify(ch, APPLY_MOVE, (config_get_int("pool_bonus_amount") * (1 + (GET_HIGHEST_KNOWN_LEVEL(ch) / 25))), NOBITS, add);
		}
		if (HAS_BONUS_TRAIT(ch, BONUS_MANA)) {
			affect_modify(ch, APPLY_MANA, (config_get_int("pool_bonus_amount") * (1 + (GET_HIGHEST_KNOWN_LEVEL(ch) / 25))), NOBITS, add);
		}
	}
}


/**
* Player's carry limit. This was formerly a macro.
*
* @param char_data *ch The player/character.
* @return int The maximum number of items.
*/
int CAN_CARRY_N(char_data *ch) {
	int bonus;
	int total = 25;	// TODO: should this be a configurable base
	
	// contribution from gear
	total += GET_BONUS_INVENTORY(ch);
	if (GET_EQ(ch, WEAR_PACK)) {
		total += GET_PACK_CAPACITY(GET_EQ(ch, WEAR_PACK));
	}
	
	// players only:
	if (!IS_NPC(ch)) {
		// player's bonus trait
		if (HAS_BONUS_TRAIT(ch, BONUS_INVENTORY)) {
			bonus = GET_HIGHEST_KNOWN_LEVEL(ch) / 10;
			total += MAX(5, bonus);
		}
	}
	
	return total;
}


/**
* Determines if a character can see in a dark room. This replaces a shirt-load
* of macros that were increasingly hard to read.
*
* @param char_data *ch The character.
* @param room_data *room The room they're trying to see in (which may be light).
* @param bool count_adjacent_light If TRUE, light cascades from adjacent tiles.
* @return bool TRUE if the character can see in dark here (or the room is light); FALSE if the character cannot see due to darkness.
*/
bool can_see_in_dark_room(char_data *ch, room_data *room, bool count_adjacent_light) {
	// magic darkness overrides everything
	if (MAGIC_DARKNESS(room) && !CAN_SEE_IN_MAGIC_DARKNESS(ch)) {
		return FALSE;	// magic dark
	}
	
	// see-in-dark abilities
	if (HAS_INFRA(ch) || (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_HOLYLIGHT))) {
		return TRUE;	// can see
	}
	
	// check if the room is actually light
	if (room_is_light(room, count_adjacent_light, CAN_SEE_IN_MAGIC_DARKNESS(ch))) {
		return TRUE;
	}
	
	// reasons the character can see
	if (room == IN_ROOM(ch) && (has_player_tech((ch), PTECH_SEE_CHARS_IN_DARK) || has_player_tech((ch), PTECH_SEE_OBJS_IN_DARK))) {
		return TRUE;	// can see in own room due to see-*-in-dark
	}
	if (room == IN_ROOM(ch) && IS_OUTDOORS(ch) && has_player_tech((ch), PTECH_SEE_IN_DARK_OUTDOORS)) {
		return TRUE;	// can see in own room outdoors
	}
	
	// all other cases:
	return FALSE;
}


// checks stuff right after movement
EVENTFUNC(check_leading_event) {
	struct char_event_data *data = (struct char_event_data*)event_obj;
	char_data *ch = data->character;
	
	// will not be re-using this
	delete_stored_event(&GET_STORED_EVENTS(ch), SEV_CHECK_LEADING);
	free(data);
	
	if (!IN_ROOM(ch)) {
		return 0;	// never re-enqueue
	}
	
	// NOTE: if you add conditions here, check _QUALIFY_CHECK_LEADING(ch) too
	if (GET_LEADING_VEHICLE(ch) && IN_ROOM(ch) != IN_ROOM(GET_LEADING_VEHICLE(ch))) {
		act("You have lost $V and stop leading it.", FALSE, ch, NULL, GET_LEADING_VEHICLE(ch), TO_CHAR | ACT_VEH_VICT);
		VEH_LED_BY(GET_LEADING_VEHICLE(ch)) = NULL;
		GET_LEADING_VEHICLE(ch) = NULL;
	}
	if (GET_LEADING_MOB(ch) && IN_ROOM(ch) != IN_ROOM(GET_LEADING_MOB(ch))) {
		act("You have lost $N and stop leading $M.", FALSE, ch, NULL, GET_LEADING_MOB(ch), TO_CHAR);
		GET_LED_BY(GET_LEADING_MOB(ch)) = NULL;
		GET_LEADING_MOB(ch) = NULL;
	}
	if (GET_SITTING_ON(ch)) {
		// things that cancel sitting-on:
		if (IN_ROOM(ch) != IN_ROOM(GET_SITTING_ON(ch)) || (GET_POS(ch) != POS_SITTING && GET_POS(ch) != POS_RESTING && GET_POS(ch) != POS_SLEEPING) || IS_RIDING(ch) || GET_LEADING_MOB(ch) || GET_LEADING_VEHICLE(ch)) {
			do_unseat_from_vehicle(ch);
		}
	}
	
	return 0;	// never re-enqueue
}


/**
* Gives a character the appropriate amount of command lag (wait time).
*
* @param char_data *ch The person to give command lag to.
* @param int wait_type A WAIT_ const to help determine wait time.
*/
void command_lag(char_data *ch, int wait_type) {
	double val;
	int wait;
	
	// short-circuit
	if (wait_type == WAIT_NONE) {
		return;
	}
	
	// base
	wait = universal_wait;
	
	// WAIT_x: functionality for wait states
	switch (wait_type) {
		case WAIT_SPELL: {	// spells (but not combat spells)
			if (has_player_tech(ch, PTECH_FASTCASTING)) {
				val = 0.3333 * GET_WITS(ch);
				wait -= MAX(0, val) RL_SEC;
				
				// ensure minimum of 0.5 seconds
				wait = MAX(wait, 0.5 RL_SEC);
				
				gain_player_tech_exp(ch, PTECH_FASTCASTING, 5);
			}
			break;
		}
		case WAIT_MOVEMENT: {	// normal movement (special handling)
			if (IS_SLOWED(ch)) {
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
		case WAIT_LONG: {
			wait = 2 RL_SEC;
			break;
		}
		case WAIT_VERY_LONG: {
			wait = 4 RL_SEC;
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
	double total, slots;
	int avg, level, pos, old;
	
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
	
	old = GET_GEAR_LEVEL(ch);
	GET_GEAR_LEVEL(ch) = MAX(level, 0);
	
	if (old != GET_GEAR_LEVEL(ch)) {
		update_MSDP_level(ch, UPDATE_SOON);
		queue_delayed_update(ch, CDU_PASSIVE_BUFFS);
	}
}


/**
* Finds an attribute by name, preferring exact matches to partial matches.
*
* @param char *name The name to look up.
* @return int An attribute constant (STRENGTH) or -1 for no-match.
*/
int get_attribute_by_name(char *name) {
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
* Determines what effective terrain height a player has when looking at the
* map, for the purpose of determining what terrain is blocked.
*
* @param char_data *ch The person.
* @param room_data *from_room Optional: What room the person is looking at (if NULL, assumes it's the room they're in).
* @return int The person's view height.
*/
int get_view_height(char_data *ch, room_data *from_room) {
	room_data *home_room;
	vehicle_data *veh;
	int height = 0, best_veh = 0;
	
	if (!from_room) {
		from_room = IN_ROOM(ch);
	}
	
	// room modifiers
	if (from_room) {
		home_room = HOME_ROOM(from_room);
		
		// ignore negative heights: these are used to track water flow
		height += MAX(0, ROOM_HEIGHT(home_room));
		
		if (GET_BUILDING(home_room) && IS_COMPLETE(home_room)) {
			height += GET_BLD_HEIGHT(GET_BUILDING(home_room));
		}
		
		// if in a vehicle, apply it here
		if (GET_ROOM_VEHICLE(from_room)) {
			height += VEH_HEIGHT(GET_ROOM_VEHICLE(from_room));
		}
	}
	
	// character modifiers
	if (from_room == IN_ROOM(ch) && EFFECTIVELY_FLYING(ch)) {
		++height;
	}
	if (from_room == IN_ROOM(ch) && HAS_BONUS_TRAIT(ch, BONUS_VIEW_HEIGHT)) {
		++height;
	}
	
	// possible vehicle modifiers
	if (GET_SITTING_ON(ch)) {
		// prefer sitting-on
		height += VEH_HEIGHT(GET_SITTING_ON(ch));
	}
	else if (from_room) {
		// if not sitting-on, look for a vehicle in the room that has no sit/inside
		DL_FOREACH2(ROOM_VEHICLES(from_room), veh, next_in_room) {
			if (!VEH_FLAGGED(veh, VEH_SIT | VEH_SLEEP) && VEH_INTERIOR_ROOM_VNUM(veh) == NOWHERE && VEH_IS_COMPLETE(veh)) {
				best_veh = MAX(best_veh, VEH_HEIGHT(veh));
			}
		}
		
		height += best_veh;
	}
	
	return height;
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
* Schedules an event to check things a person is leading/sitting on after
* moving, if needed.
*
* @param char_data *ch The person who moved.
*/
void schedule_check_leading_event(char_data *ch) {
	struct char_event_data *data;
	struct dg_event *ev;
	
	// things that need this function
	#define _QUALIFY_CHECK_LEADING(ch)  (GET_LEADING_VEHICLE(ch) || GET_LEADING_MOB(ch) || GET_SITTING_ON(ch))
	
	if (ch && _QUALIFY_CHECK_LEADING(ch) && !find_stored_event(GET_STORED_EVENTS(ch), SEV_CHECK_LEADING)) {
		CREATE(data, struct char_event_data, 1);
		data->character = ch;
		
		ev = dg_event_create(check_leading_event, data, 1 RL_SEC);
		add_stored_event(&GET_STORED_EVENTS(ch), SEV_CHECK_LEADING, ev);
	}
}


/**
* Raises a person from sleeping+ to standing (or fighting) if possible.
* 
* @param char_data *ch The person to try to wake/stand.
* @return bool TRUE if the character ended up standing (>= fighting), FALSE if not.
*/
bool wake_and_stand(char_data *ch) {
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
					act(obj_get_custom_message(use_obj, OBJ_CUSTOM_CRAFT_TO_CHAR), FALSE, ch, use_obj, crafting_veh, TO_CHAR | TO_SPAMMY | ACT_VEH_VICT);
					messaged_char = TRUE;
				}
				if (!messaged_room && obj_has_custom_message(use_obj, OBJ_CUSTOM_CRAFT_TO_ROOM)) {
					act(obj_get_custom_message(use_obj, OBJ_CUSTOM_CRAFT_TO_ROOM), FALSE, ch, use_obj, crafting_veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
					messaged_room = TRUE;
				}
				break;
			}
			case APPLY_RES_REPAIR: {
				if (!messaged_char) {
					act("You use $p to repair $V.", FALSE, ch, use_obj, crafting_veh, TO_CHAR | TO_SPAMMY | ACT_VEH_VICT);
					messaged_char = TRUE;
				}
				if (!messaged_room) {
					act("$n uses $p to repair $V.", FALSE, ch, use_obj, crafting_veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
					messaged_room = TRUE;
				}
				break;
			}
		}
	}
	
	// RES_x: Remove one resource based on type.
	switch (res->type) {
		case RES_OBJECT:	// obj/component are just simple objs, as are tools
		case RES_COMPONENT:
		case RES_TOOL: {
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
						act("You place $p onto $V.", FALSE, ch, use_obj, crafting_veh, TO_CHAR | TO_SPAMMY | ACT_VEH_VICT);
					}
					if (!messaged_room) {
						act("$n places $p onto $V.", FALSE, ch, use_obj, crafting_veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
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
						act("You pour $p into $V.", FALSE, ch, use_obj, crafting_veh, TO_CHAR | TO_SPAMMY | ACT_VEH_VICT);
					}
					if (!messaged_room) {
						act("$n pours $p into $V.", FALSE, ch, use_obj, crafting_veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
					}
					break;
				}
				// already messaged APPLY_RES_REPAIR for this type
			}
			
			if (use_obj) {
				amt = GET_DRINK_CONTAINER_CONTENTS(use_obj);
				amt = MIN(res->amount, amt);
				
				res->amount -= amt;	// possible partial payment
				set_obj_val(use_obj, VAL_DRINK_CONTAINER_CONTENTS, GET_DRINK_CONTAINER_CONTENTS(use_obj) - amt);
				if (GET_DRINK_CONTAINER_CONTENTS(use_obj) == 0) {
					set_obj_val(use_obj, VAL_DRINK_CONTAINER_TYPE, LIQ_WATER);
				}
				
				if (build_used_list) {
					add_to_resource_list(build_used_list, RES_LIQUID, res->vnum, amt, 0);
				}
			}
			break;
		}
		case RES_COINS: {
			empire_data *coin_emp = real_empire(res->vnum);
			char buf2[MAX_STRING_LENGTH];
			*buf2 = '\0';
			
			charge_coins(ch, coin_emp, res->amount, build_used_list, msg_type == APPLY_RES_SILENT ? NULL : buf2);
			
			if (!messaged_char && msg_type != APPLY_RES_SILENT) {
				snprintf(buf, sizeof(buf), "You spend %s.", buf2);
				act(buf, FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
			}
			if (!messaged_room && msg_type != APPLY_RES_SILENT) {
				snprintf(buf, sizeof(buf), "$n spends %s.", buf2);
				act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
			}
			
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
			
			set_current_pool(ch, res->vnum, GET_CURRENT_POOL(ch, res->vnum) - res->amount);
			if (GET_CURRENT_POOL(ch, res->vnum) < 0) {
				set_current_pool(ch, res->vnum, 0);
			}
			
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
						act(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR), FALSE, ch, NULL, crafting_veh, TO_CHAR | TO_SPAMMY | ACT_VEH_VICT);
					}
					if (!messaged_room && gen && GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM)) {
						act(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM), FALSE, ch, NULL, crafting_veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
					}
					break;
				}
				case APPLY_RES_REPAIR: {
					if (!messaged_char && gen && GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR)) {
						act(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR), FALSE, ch, NULL, crafting_veh, TO_CHAR | TO_SPAMMY | ACT_VEH_VICT);
					}
					if (!messaged_room && gen && GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM)) {
						act(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM), FALSE, ch, NULL, crafting_veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
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
* Extract resources from the list, hopefully having checked has_resources, as
* this function does not error if it runs out -- it just keeps extracting
* until it's out of items, or hits its required limit.
*
* This function always takes from inventory first, and ground second.
*
* For components, this prefers an exact match, then a basic match, then any
* extended/related component.
*
* @param char_data *ch The person whose resources to take.
* @param struct resource_data *list Any resource list.
* @param bool ground If TRUE, will also take resources off the ground.
* @param struct resource_data **build_used_list Optional: if not NULL, will build a resource list of the specifc things extracted.
*/
void extract_resources(char_data *ch, struct resource_data *list, bool ground, struct resource_data **build_used_list) {
	int diff, liter, cycle;
	obj_data *obj, *next_obj;
	struct resource_data *res, *list_copy;
	
	// This is done in 3 phases (to ensure specific objs are used before components):
	#define EXRES_OBJS  (cycle == 0)	// check/mark specific objs and matching components
	#define EXRES_BASIC  (cycle == 1)	// check/mark basic components
	#define EXRES_OTHER  (cycle == 2)	// check/mark remaining components
	#define NUM_EXRES_CYCLES  3
	
	// we make a copy of the list and delete as we go
	list_copy = copy_resource_list(list);
	
	for (cycle = 0; cycle < NUM_EXRES_CYCLES; ++cycle) {
		LL_FOREACH(list_copy, res) {
			if (res->amount < 1) {
				continue;	// resource done
			}
			else if (EXRES_OBJS && res->type != RES_OBJECT && res->type != RES_COMPONENT) {
				// only RES_OBJECT and RES_COMPONENT (exact matches) are checked in the first cycle
				continue;
			}
			else if (!EXRES_OBJS && res->type == RES_OBJECT) {
				continue;	// skip objs except on the first cycle
			}
		
			// RES_x: extract resources by type
			switch (res->type) {
				case RES_OBJECT:
				case RES_COMPONENT:
				case RES_LIQUID:
				case RES_TOOL: {	// these 4 types check objects
					obj_data *search_list[2];
					search_list[0] = ch->carrying;	// rebuild list each time... ch->carrying can change
					search_list[1] = ground ? ROOM_CONTENTS(IN_ROOM(ch)) : NULL;
				
					// up to two places to search
					for (liter = 0; liter < 2 && res->amount > 0; ++liter) {
						DL_FOREACH_SAFE2(search_list[liter], obj, next_obj, next_content) {
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
									
										--res->amount;
										extract_obj(obj);
									}
									break;
								}
								case RES_COMPONENT: {
									// 1st pass only takes exact matches; 2nd pass takes BASIC extended-matches; 3rd pass takes any extended match
									if ((EXRES_OBJS && GET_OBJ_COMPONENT(obj) == res->vnum) || (is_component_vnum(obj, res->vnum) &&  ((EXRES_BASIC && is_basic_component(obj)) || EXRES_OTHER))) {
										if (build_used_list) {
											add_to_resource_list(build_used_list, RES_OBJECT, GET_OBJ_VNUM(obj), 1, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
										}
									
										--res->amount;
										extract_obj(obj);
									}
									break;
								}
								case RES_LIQUID: {
									if (IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == res->vnum) {
										diff = MIN(res->amount, GET_DRINK_CONTAINER_CONTENTS(obj));
										res->amount -= diff;
										set_obj_val(obj, VAL_DRINK_CONTAINER_CONTENTS, GET_DRINK_CONTAINER_CONTENTS(obj) - diff);
									
										if (GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) == 0) {
											set_obj_val(obj, VAL_DRINK_CONTAINER_TYPE, LIQ_WATER);
										}
									
										if (build_used_list) {
											add_to_resource_list(build_used_list, RES_LIQUID, res->vnum, diff, 0);
										}
									}
									break;
								}
								case RES_TOOL: {
									if (TOOL_FLAGGED(obj, res->vnum)) {
										if (build_used_list) {
											add_to_resource_list(build_used_list, RES_OBJECT, GET_OBJ_VNUM(obj), 1, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
										}
									
										--res->amount;
										extract_obj(obj);
									}
									break;
								}
							}
						
							// ok to break out early if we found enough
							if (res->amount <= 0) {
								break;
							}
						}
					}
					break;
				}
				case RES_COINS: {
					charge_coins(ch, real_empire(res->vnum), res->amount, build_used_list, NULL);
					res->amount = 0;	// got full amount
					break;
				}
				case RES_CURRENCY: {
					add_currency(ch, res->vnum, -(res->amount));
					res->amount = 0;	// got full amount
					break;
				}
				case RES_POOL: {
					if (build_used_list) {
						add_to_resource_list(build_used_list, RES_POOL, res->vnum, res->amount, 0);
					}
				
					set_current_pool(ch, res->vnum, GET_CURRENT_POOL(ch, res->vnum) - res->amount);
					if (GET_CURRENT_POOL(ch, res->vnum) < 0) {
						set_current_pool(ch, res->vnum, 0);
					}
					res->amount = 0;	// got full amount
				
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
	
	free_resource_list(list_copy);
}


/**
* Finds the next resource a character has available, from a list. This can be
* used to get the next <thing> for any one-at-a-time building task, including
* vehicles. This function returns the resource_data pointer, and will bind the
* object it found (if applicable to the resource type), but it doesn't extract
* or free anything.
*
* For components, this prefers an exact match, then a basic match, then any
* extended/related component.
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
	struct resource_data *res, *basic_res = NULL, *extended_res = NULL;
	obj_data *obj, *basic_obj = NULL, *extended_obj = NULL;
	int liter, amt;
	
	*found_obj = NULL;
	
	LL_FOREACH(list, res) {
		// RES_x: find first resource by type
		switch (res->type) {
			case RES_OBJECT:
			case RES_COMPONENT:
			case RES_LIQUID:
			case RES_TOOL: {	// these 4 types check objects
				obj_data *search_list[2];
				search_list[0] = ch->carrying;
				search_list[1] = ground ? ROOM_CONTENTS(IN_ROOM(ch)) : NULL;
				
				// up to two places to search
				for (liter = 0; liter < 2; ++liter) {
					DL_FOREACH2(search_list[liter], obj, next_content) {
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
								if (GET_OBJ_COMPONENT(obj) == res->vnum) {
									// got one! exact match means we're done
									*found_obj = obj;
									return res;
								}
								else if (!basic_obj && is_component_vnum(obj, res->vnum)) {
									if (is_basic_component(obj)) {
										// fall back to this one if no exact match
										basic_obj = obj;
										basic_res = res;
									}
									else if (!extended_obj) {
										// fall back to this one if no basic match
										extended_obj = obj;
										extended_res = res;
									}
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
							case RES_TOOL: {
								if (TOOL_FLAGGED(obj, res->vnum)) {
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
	
	// no exact matches...
	if (basic_obj && basic_res) {
		*found_obj = basic_obj;
		return basic_res;
	}
	else if (extended_obj && extended_res) {
		*found_obj = extended_obj;
		return extended_res;
	}
	else {
		return NULL;	// found nothing
	}
}


/**
* Gets string output describing one "resource" entry, for use in lists.
*
* @param sturct resource_data *res The resource to display.
* @return char* A short string including quantity and type.
*/
char *get_resource_name(struct resource_data *res) {
	static char output[MAX_STRING_LENGTH];
	char buf[MAX_STRING_LENGTH];
	
	*output = '\0';
	
	// RES_x: resource type determines display
	switch (res->type) {
		case RES_OBJECT: {
			snprintf(output, sizeof(output), "%dx %s", res->amount, skip_filler(get_obj_name_by_proto(res->vnum)));
			break;
		}
		case RES_COMPONENT: {
			snprintf(output, sizeof(output), "%dx (%s)", res->amount, res->amount == 1 ? get_generic_name_by_vnum(res->vnum) : get_generic_string_by_vnum(res->vnum, GENERIC_COMPONENT, GSTR_COMPONENT_PLURAL));
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
		case RES_TOOL: {
			prettier_sprintbit(res->vnum, tool_flags, buf);
			snprintf(output, sizeof(output), "%dx %s (tool%s)", res->amount, buf, PLURAL(res->amount));
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
			case RES_ACTION:
			case RES_TOOL: {
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
					
					if (load_otrigger(obj) && obj->carried_by) {
						get_otrigger(obj, obj->carried_by, FALSE);
					}
				}
				break;
			}
			case RES_LIQUID: {
				obj_data *search_list[2] = { ch->carrying, ROOM_CONTENTS(IN_ROOM(ch)) };
				remaining = res->amount / (split ? 2 : 1);
				last = FALSE;	// cause next obj to refund
				
				// up to two places to search for containers to fill
				for (liter = 0; liter < 2 && remaining > 0; ++liter) {
					DL_FOREACH2(search_list[liter], obj, next_content) {
						if (IS_DRINK_CONTAINER(obj) && (GET_DRINK_CONTAINER_TYPE(obj) == res->vnum || GET_DRINK_CONTAINER_CONTENTS(obj) == 0)) {
							diff = GET_DRINK_CONTAINER_CAPACITY(obj) - GET_DRINK_CONTAINER_CONTENTS(obj);
							diff = MIN(remaining, diff);
							remaining -= diff;
							set_obj_val(obj, VAL_DRINK_CONTAINER_CONTENTS, GET_DRINK_CONTAINER_CONTENTS(obj) + diff);
							
							// set type in case container was empty
							if (GET_DRINK_CONTAINER_CONTENTS(obj) > 0) {
								set_obj_val(obj, VAL_DRINK_CONTAINER_TYPE, res->vnum);
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
				set_current_pool(ch, res->vnum, GET_CURRENT_POOL(ch, res->vnum) + (res->amount / (split ? 2 : 1)));
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
* Removes some resources from a resource list when dismantling starts, based on
* the material type and the damage to the building.
*
* @param int damage The current amount of damage.
* @param int max_health The maximum health of the building/vehicle.
* @param struct resource_data **list The list to halve.
*/
void reduce_dismantle_resources(int damage, int max_health, struct resource_data **list) {
	struct resource_data *res, *next_res;
	int iter, count, total, remaining;
	double damage_prc, prc_to_keep;
	obj_data *proto;
	
	// determine how damaged the building it -- 1.0+ is VERY damaged, 0 is not damaged at all
	damage_prc = ((double)damage / MAX(1.0, (double)max_health));
	damage_prc = MIN(1.0, damage_prc);	// reduce to 0-1.0 range (1.0 is fully damaged)
	
	// we keep a percent of the items based on how damaged it is, in the 20-90% range
	prc_to_keep = (1.0 - damage_prc) * 0.7 + 0.2;
	
	// count these to determine how much we removed
	total = remaining = 0;
	
	// first round: only lower res->amount, do not delete res here
	LL_FOREACH(*list, res) {
		total += res->amount;	// number of resources in the list
		
		// random chance of decay for items based on material type
		if (res->type == RES_OBJECT && (proto = obj_proto(res->vnum))) {
			for (iter = 0; iter < res->amount; ++iter) {
				if (number(1, 10000) > materials[GET_OBJ_MATERIAL(proto)].chance_to_dismantle * 100) {
					res->amount -= 1;
				}
			}
			remaining += res->amount;	// how much survived this round
		}
		else {	// survives this round
			remaining += res->amount;
		}
	}
	
	// ensure no 0
	total = MAX(1, total);
	
	// second round (if we didn't reduce enough)
	count = 0;
	while (count < 150 && (((double) remaining) / total) > prc_to_keep) {
		++count;	// prevents infinite loop
		
		LL_FOREACH(*list, res) {
			if (res->amount > 0 && !number(0, 1)) {
				res->amount -= 1;
				--remaining;
			}
			
			if (((double) remaining) / total <= prc_to_keep) {
				break;	// enough
			}
		}
	}
	
	// check for no remaining resources and delete res entries if so
	LL_FOREACH_SAFE(*list, res, next_res) {
		if (res->amount <= 0) {
			LL_DELETE(*list, res);
			free(res);
		}
	}
}


/**
* Find out if a person has resources available.
*
* For components, this prefers an exact match, then a basic match, then any
* extended/related component. This prevents an item from blocking another item
* and resulting in "you don't have enough".
*
* @param char_data *ch The person whose resources to check.
* @param struct resource_data *list Any resource list.
* @param bool ground If TRUE, will also count resources on the ground.
* @param bool send_msgs If TRUE, will alert the character as to what they need. FALSE runs silently.
* @param char *msg_prefix Optional: If provided AND send_msgs is TRUE, prepends this to any error message as "msg_prefix: You need..." (may be NULL)
*/
bool has_resources(char_data *ch, struct resource_data *list, bool ground, bool send_msgs, char *msg_prefix) {
	char buf[MAX_STRING_LENGTH], prefix[256];
	int amt, liter, cycle;
	struct resource_data *res, *list_copy;
	bool ok = TRUE;
	obj_data *obj;
	
	unmark_items_for_char(ch, ground);
	
	// work from a copy of the list
	list_copy = copy_resource_list(list);

	// This is done in 2 phases (to ensure specific objs are used before components):
	#define HASRES_OBJS  (cycle == 0)	// check/mark specific objs and exact components
	#define HASRES_BASIC  (cycle == 1)	// basic components
	#define HASRES_OTHER  (cycle == 2)	// check/mark exteded components
	#define NUM_HASRES_CYCLES  3
	
	for (cycle = 0; cycle < NUM_HASRES_CYCLES; ++cycle) {
		LL_FOREACH(list_copy, res) {
			if (res->amount < 1) {
				continue;	// resource done
			}
			else if (HASRES_OBJS && res->type != RES_OBJECT && res->type != RES_COMPONENT) {
				// only RES_OBJECT and RES_COMPONENT (exact matches) are checked in the first cycle
				continue;
			}
			else if (!HASRES_OBJS && res->type == RES_OBJECT) {
				continue;
			}
		
			// RES_x: check resources by type
			switch (res->type) {
				case RES_OBJECT:
				case RES_COMPONENT:
				case RES_LIQUID:
				case RES_TOOL: {	// these 4 types check objects
					obj_data *search_list[2];
					search_list[0] = ch->carrying;
					search_list[1] = ground ? ROOM_CONTENTS(IN_ROOM(ch)) : NULL;
				
					// now search the list(s)
					for (liter = 0; liter < 2 && res->amount > 0; ++liter) {
						DL_FOREACH2(search_list[liter], obj, next_content) {
							if (obj->search_mark) {
								continue;	// skip already-used items
							}
							else if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
								continue;	// skip keeps
							}
							else if (!CAN_SEE_OBJ(ch, obj)) {
								continue;	// skip can't-see
							}
						
							// RES_x: just types that need objects
							switch (res->type) {
								case RES_OBJECT: {
									if (GET_OBJ_VNUM(obj) == res->vnum) {
										--res->amount;
										obj->search_mark = TRUE;
									}
									break;
								}
								case RES_COMPONENT: {
									if (GET_OBJ_COMPONENT(obj) == res->vnum || is_component_vnum(obj, res->vnum)) {
										--res->amount;
										obj->search_mark = TRUE;
									}
									break;
								}
								case RES_LIQUID: {
									if (IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == res->vnum) {
										// reduce amount by the volume of liquid
										res->amount -= GET_DRINK_CONTAINER_CONTENTS(obj);
										obj->search_mark = TRUE;
									}
									break;
								}
								case RES_TOOL: {
									if (TOOL_FLAGGED(obj, res->vnum)) {
										--res->amount;
										obj->search_mark = TRUE;
									}
									break;
								}
							}
						
							// ok to break out early if we found enough
							if (res->amount < 1) {
								break;
							}
						}
					}
					
					break;
				}
				case RES_COINS: {
					empire_data *coin_emp = real_empire(res->vnum);
					if (can_afford_coins(ch, coin_emp, res->amount)) {
						res->amount = 0;
					}
					break;
				}
				case RES_CURRENCY: {
					res->amount -= get_currency(ch, res->vnum);
					break;
				}
				case RES_POOL: {
					// special rule: require that blood or health costs not reduce player below 1
					amt = res->amount + ((res->vnum == HEALTH || res->vnum == BLOOD) ? 1 : 0);
					res->amount -= MAX(amt, GET_CURRENT_POOL(ch, res->vnum));
					break;
				}
				case RES_ACTION: {
					res->amount = 0;
					// always has these
					break;
				}
			}
		}
	}
	
	// now check ok and message for anything missing
	LL_FOREACH(list_copy, res) {
		if (res->amount < 1) {
			continue;	// had enough
		}
		
		// RES_x: messaging for types the player is missing
		if (send_msgs) {
			// prepare prefix, if any
			if (msg_prefix && *msg_prefix) {
				snprintf(prefix, sizeof(prefix), "%s: You need", msg_prefix);
				CAP(prefix);
			}
			else {
				snprintf(prefix, sizeof(prefix), "You need");
			}
			
			switch (res->type) {
				case RES_OBJECT: {
					msg_to_char(ch, "%s %d more of %s", (ok ? prefix : ","), res->amount, skip_filler(get_obj_name_by_proto(res->vnum)));
					break;
				}
				case RES_COMPONENT: {
					msg_to_char(ch, "%s %d more (%s)", (ok ? prefix : ","), res->amount, res->amount == 1 ? get_generic_name_by_vnum(res->vnum) : get_generic_string_by_vnum(res->vnum, GENERIC_COMPONENT, GSTR_COMPONENT_PLURAL));
					break;
				}
				case RES_LIQUID: {
					msg_to_char(ch, "%s %d more unit%s of %s", (ok ? prefix : ","), res->amount, PLURAL(res->amount), get_generic_string_by_vnum(res->vnum, GENERIC_LIQUID, GSTR_LIQUID_NAME));
					break;
				}
				case RES_TOOL: {
					prettier_sprintbit(res->vnum, tool_flags, buf);
					msg_to_char(ch, "%s %d more %s (tool%s)", (ok ? prefix : ","), res->amount, buf, PLURAL(res->amount));
					break;
				}
				case RES_COINS: {
					empire_data *coin_emp = real_empire(res->vnum);
					msg_to_char(ch, "%s %s", (ok ? prefix : ","), money_amount(coin_emp, res->amount));
					break;
				}
				case RES_CURRENCY: {
					msg_to_char(ch, "%s %d more %s", (ok ? prefix : ","), res->amount, get_generic_string_by_vnum(res->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(res->amount)));
					break;
				}
				case RES_POOL: {
					// special rule: require that blood or health costs not reduce player below 1
					amt = res->amount + ((res->vnum == HEALTH || res->vnum == BLOOD) ? 1 : 0);
					msg_to_char(ch, "%s %d more %s point%s", (ok ? prefix : ","), amt, pool_types[res->vnum], PLURAL(amt));
					break;
				}
			}
		}
		
		ok = FALSE;	// at least 1 thing missing
	}
	
	if (!ok && send_msgs) {
		send_to_char(".\r\n", ch);
	}
	
	free_resource_list(list_copy);
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
		DL_FOREACH2(search_list[iter], obj, next_content) {
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
* @param bitvector_t prefer_climate Will attempt to find a good match for this climate (but will return without it if not).
* @return sector_data* A sector, or NULL if none matches.
*/
sector_data *find_first_matching_sector(bitvector_t with_flags, bitvector_t without_flags, bitvector_t prefer_climate) {
	sector_data *sect, *next_sect, *low_climate = NULL, *no_climate = NULL;
	
	HASH_ITER(hh, sector_table, sect, next_sect) {
		if (with_flags && !SECT_FLAGGED(sect, with_flags)) {
			continue;
		}
		if (without_flags && SECT_FLAGGED(sect, without_flags)) {
			continue;
		}
		
		// ok it matches flags, now check climate
		if (!prefer_climate || (GET_SECT_CLIMATE(sect) & prefer_climate) == prefer_climate) {
			return sect;	// perfect match
		}
		else if (!low_climate && prefer_climate && (GET_SECT_CLIMATE(sect) & prefer_climate)) {
			low_climate = sect;	// missing some climate flags
		}
		else if (!no_climate) {
			no_climate = sect;	// missing all climate flags
		}
	}
	
	// if we got here, there was no perfect match
	return low_climate ? low_climate : no_climate;
}


 //////////////////////////////////////////////////////////////////////////////
//// STRING UTILS ////////////////////////////////////////////////////////////

/**
* Determines if any word in str is an abbrev for any keyword in namelist,
* separately -- even if the other words in str are not.
*
* @param const char *str The search arguments, e.g. "hug blue"
* @param const char *namelist The keyword list, e.g. "bear white huge"
* @return bool TRUE if any word in str is an abbrev for any keyword in namelist, even if the others aren't.
*/
bool any_isname(const char *str, const char *namelist) {
	char *newlist, *curtok, word[MAX_INPUT_LENGTH];
	const char *ptr;
	bool found = FALSE;
	
	if (!*str || !*namelist) {
		return FALSE;	// shortcut
	}
	if (!str_cmp(str, namelist)) {
		return TRUE;	// the easy way
	}

	newlist = strdup(namelist);

	// for each word in newlist
	for (curtok = strtok(newlist, WHITESPACE); curtok && !found; curtok = strtok(NULL, WHITESPACE)) {
		if (curtok) {
			ptr = str;
			while (*ptr && !found) {
				ptr = any_one_arg((char*)ptr, word);
				if (*word && is_abbrev(word, curtok)) {
					found = TRUE;
				}
			}
		}
	}

	free(newlist);
	return found;
}


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
* Converts a number of seconds (or minutes) from a large number to a 0:00
* format (or 0:00:00 or even 0:00:00:00).
*
* @param int seconds How long the time is.
* @param bool minutes_instead if TRUE, the 'seconds' field is actually minutes and the time returned will be minutes as well.
* @param char *unlimited_str If the time can be -1/UNLIMITED, returns this string instead (may be NULL).
* @return char* The time string formatted with colons, or the "unlimited_str" if seconds was -1/UNLIMITED
*/
char *colon_time(int seconds, bool minutes_instead, char *unlimited_str) {
	static char output[256];
	int minutes, hours, days;
	
	// some things using this function allow UNLIMITED as a value
	if (seconds == UNLIMITED) {
		return unlimited_str ? unlimited_str : "unlimited";
	}
	
	// multiplier if the unit is minutes
	if (minutes_instead) {
		seconds *= SECS_PER_REAL_MIN;
	}
	
	// a negative here would be meaningless
	seconds = ABSOLUTE(seconds);
	
	// compute
	days = seconds / SECS_PER_REAL_DAY;
	seconds %= SECS_PER_REAL_DAY;
	
	hours = seconds / SECS_PER_REAL_HOUR;
	seconds %= SECS_PER_REAL_HOUR;
	
	minutes = seconds / SECS_PER_REAL_MIN;
	seconds %= SECS_PER_REAL_MIN;
	
	if (days > 0) {
		snprintf(output, sizeof(output), "%d:%02d:%02d:%02d", days, hours, minutes, seconds);
	}
	else if (hours > 0) {
		snprintf(output, sizeof(output), "%d:%02d:%02d", hours, minutes, seconds);
	}
	else {
		snprintf(output, sizeof(output), "%d:%02d", minutes, seconds);
	}
	
	// if we started with minutes, shave the seconds off
	if (minutes_instead && strlen(output) >= 3) {
		output[strlen(output) - 3] = '\0';
	}
	
	return output;
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
* Doubles the ampersands in a 4-char icon and returns the result. This is
* mainly for loading stored map memories.
*
* @param char *icon The incoming 4-char icon.
* @return char* The modified icon, or the original if there were no ampersands.
*/
char *double_map_ampersands(char *icon) {
	static char output[32];
	int iter, pos;
	
	// shortcut: easy in, easy out
	if (!strchr(icon, COLOUR_CHAR)) {
		return icon;
	}
	
	// copy while doubling
	for (iter = 0, pos = 0; icon[iter] && pos < 30; ++iter) {
		if (icon[iter] == COLOUR_CHAR) {
			// double the ampersand
			output[pos++] = COLOUR_CHAR;
		}
		// and copy the char
		output[pos++] = icon[iter];
	}
	
	// and terminate
	output[pos++] = '\0';
	
	return output;
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

	for (point = holder; *namelist && !isspace(*namelist); namelist++, point++) {
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
	
	if (!*str || !*namelist || (*str == UID_CHAR && isdigit(*(str+1)))) {
		return FALSE;	// shortcut
	}

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
* Variant of isname that also deterines if it used an abbreviation.
*
* @param const char *str The search argument, e.g. "be"
* @param const char *namelist The keyword list, e.g. "bear white huge"
* @param bool *was_exact Optional: Will be set to TRUE if an exact keyword was typed instead of an abbrev. (Pass NULL to skip this.)
* @return bool TRUE if str is an abbrev for any keyword in namelist
*/
bool isname_check_exact(const char *str, const char *namelist, bool *was_exact) {
	char *newlist, *curtok;
	bool found = FALSE;
	
	if (was_exact) {
		// initialize
		*was_exact = FALSE;
	}
	
	if (!*str || !*namelist || (*str == UID_CHAR && isdigit(*(str+1)))) {
		return FALSE;	// shortcut
	}

	/* the easy way */
	if (!str_cmp(str, namelist)) {
		if (was_exact) {
			*was_exact = TRUE;
		}
		return TRUE;
	}

	newlist = strdup(namelist);

	for (curtok = strtok(newlist, WHITESPACE); curtok && (!found || (was_exact && !*was_exact)); curtok = strtok(NULL, WHITESPACE)) {
		if (curtok && was_exact && !str_cmp(str, curtok)) {
			*was_exact = TRUE;
			found = TRUE;
		}
		else if (curtok && is_abbrev(str, curtok)) {
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
	
	if (!*arg || !namelist || !*namelist) {
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
	
	ok = *argword ? TRUE : FALSE;
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


/*
 * Like sprintbit, this turns a bitvector and a list of names into a string.
 * However, this one also takes an orderering array -- the bits will be
 * displayed in the order of that array (for each bit present) and any bits
 * not in that array will not be displayed.
 *
 * If you don't have a 'const' array, just cast it as such.  It's safer
 * to cast a non-const array as const than to cast a const one as non-const.
 * Doesn't really matter since this function doesn't change the array though.
 * 
 * @param bitvector_t bitvector The bits to display.
 * @param const char *names[] The names for each bit (terminated with "\n" at the end of the array).
 * @param const bitvector_t order[] The order to display the bits in (terminated with 0/NOBITS at the end).
 * @param bool commas If TRUE, comma-separates the bits.
 * @param char *result A string to store the output in (ideally MAX_STRING_LENGTH).
 */
void ordered_sprintbit(bitvector_t bitvector, const char *names[], const bitvector_t order[], bool commas, char *result) {
	int iter, pos, n_len = 0;
	bitvector_t temp;
	
	// determine lengths of the name array (safe max of 128 is double the largest possible number of bits)
	// - this is for array access safety if they are not the same length or if bits fall outside those arrays
	for (iter = 0; n_len == 0 && iter < 128; ++iter) {
		if (*names[iter] == '\n') {
			n_len = iter;
		}
	}
	
	// init string
	*result = '\0';
	
	// iterate over order array
	for (iter = 0; order[iter] != NOBITS; ++iter) {
		if (IS_SET(bitvector, order[iter])) {
			// determine bit pos and append flag name
			temp = order[iter];
			for (pos = 0; temp; ++pos, temp >>= 1) {
				if (IS_SET(temp, 1)) {
					sprintf(result + strlen(result), "%s%s", (*result ? (commas ? ", " : " ") : ""), (pos < n_len ? names[pos] : "UNDEFINED"));
				}
			}
		}
	}
	
	if (!*result) {
		strcpy(result, "NOBITS");
	}
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
		if (input[ipos] == COLOUR_CHAR && input[ipos+1] == '?') {
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


/**
* This function gets just the portion of a string after removing any initial
* word that was on the conjure_words[] list.
*
* If you want to save the result somewhere, you should str_dup() it
*
* @param const char *string The original string.
* @param const char **wordlist The list of words to skip.
* @param bool also_skip_filler If TRUE, also skips fill words and reserved words.
*/
const char *skip_wordlist(const char *string, const char **wordlist, bool also_skip_filler) {
	static char remainder[MAX_STRING_LENGTH];
	char temp[MAX_STRING_LENGTH];
	char *ptr, *ot;
	int pos = 0;
	
	*remainder = '\0';
	
	if (!string || !wordlist) {
		log("SYSERR: skip_wordlist received a NULL %s pointer.", string ? "string" : "wordlist");
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
	} while (search_block(temp, wordlist, TRUE) != NOTHING && (also_skip_filler || fill_word(temp) || reserved_word(temp)));
	
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


/**
* Safely gets a value "\n"-terminated array of names. This prevents out-of-
* bounds issues on the namelist.
*
* @param int type The position in the name list.
* @param const char *names[] The name list to use (must have "\n" as its last entry).
* @param char *result A string variable to save the result to.
* @param size_t max_result_size The size of the result buffer (prevents overflows).
* @param char *error_value If 'type' is out of bounds, uses this string.
*/
void sprinttype(int type, const char *names[], char *result, size_t max_result_size, char *error_value) {
	int nr = 0;
	
	while (type && *names[nr] != '\n') {
		--type;
		++nr;
	}
	
	if (*names[nr] != '\n') {
		snprintf(result, max_result_size, "%s", names[nr]);
	}
	else {
		snprintf(result, max_result_size, "%s", NULLSAFE(error_value));
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
		if (input[iter] == COLOUR_CHAR) {
			if (input[iter+1] == COLOUR_CHAR) {
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
			if (input[iter+1] == COLOUR_CHAR || input[iter+1] == '\t') {
				// double \t: copy both
				lbuf[pos++] = input[iter];
				lbuf[pos++] = input[++iter];
			}
			else if (input[iter+1] == '[') {
				// skip to the end of the color code
				for (++iter; input[iter] && input[iter] != ']'; ++iter);
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
		++str;
		++ptr;
		while (*ptr == '\r') {
			++ptr;
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
* @param const char *cs The string to search.
* @param const char *ct The string to search for...
* @return char* A pointer to the substring within cs, or NULL if not found.
*/
char *str_str(const char *cs, const char *ct) {
	const char *s, *t;

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
			return (char*)s;
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


/**
* Un-doubles ampersands in 4-char map icons and returns the result. This is
* used for writing map memory, which does not have room for doubled ampersands.
*
* @param char *icon The incoming icon, which may have doubled ampersands.
* @return char* The modified icon with any doubled ampersands reduced to one, or the original icon if there were no ampersands.
*/
char *undouble_map_ampersands(char *icon) {
	static char output[32];
	int iter, pos;
	
	// shortcut: easy in, easy out
	if (!strchr(icon, COLOUR_CHAR)) {
		return icon;
	}
	
	// copy while doubling
	for (iter = 0, pos = 0; icon[iter] && pos < 30; ++iter) {
		if (icon[iter] == COLOUR_CHAR && icon[iter+1] == COLOUR_CHAR) {
			++iter;	// skip 1
		}
		// and copy the char
		output[pos++] = icon[iter];
	}
	
	// and terminate
	output[pos++] = '\0';
	
	return output;
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
	return (sun_sect && get_sun_status(room) != SUN_DARK && !ROOM_AFF_FLAGGED(room, ROOM_AFF_MAGIC_DARKNESS));
}


/**
* Gets linear distance between two map locations (accounting for wrapping).
*
* @param int x1 first
* @param int y1  coordinate
* @param int x2 second
* @param int y2  coordinate
* @return double distance
*/
double compute_map_distance(int x1, int y1, int x2, int y2) {
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
	return sqrt(dist);
}


/**
* Gets coordinates IF the player has Navigation, and an empty string if not.
* The coordinates will include a leading space, like " (x, y)" if present. It
* may also return " (unknown)" if (x,y) are not on the map.
*
* @param char_data *ch Optional: The person to check for Navigation.
* @param int x The X-coordinate to show.
* @param int y The Y-coordinate to show.
* @param bool fixed_width If TRUE, spaces the coordinates for display in a vertical column.
* @return char* The string containing " (x, y)" or "", depending on the Navigation ability.
*/
char *coord_display(char_data *ch, int x, int y, bool fixed_width) {
	static char output[80];
	
	if (ch && (IS_NPC(ch) || !HAS_NAVIGATION(ch))) {
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
* @param bitvector_t sectf_bits SECTF_ flag(s) to look for.
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
* Determines the full climate type of a given room. This should be used instead
* of GET_SECT_CLIMATE() in most cases because it checks additional fields.
*
* @param room_data *room The room to check climate for.
* @return bitvector_t The full set of CLIM_ flags for the room.
*/
bitvector_t get_climate(room_data *room) {
	bitvector_t flags = GET_SECT_CLIMATE(SECT(room));
	room_data *home = HOME_ROOM(room);
	
	// base sect?
	if (ROOM_SECT_FLAGGED(room, SECTF_INHERIT_BASE_CLIMATE)) {
		flags |= GET_SECT_CLIMATE(BASE_SECT(room));
		
		// home room -- only if checking base sect
		if (home != room) {
			flags |= GET_SECT_CLIMATE(SECT(home));
			
			// and base of the home room?
			if (ROOM_SECT_FLAGGED(home, SECTF_INHERIT_BASE_CLIMATE)) {
				flags |= GET_SECT_CLIMATE(BASE_SECT(home));
			}
		}
	}
	
	// should this check for TEMPERATURE_USE_CLIMATE too? I think "maybe not" -pc
	
	return flags;
}


/**
* Determines the maximum depletion amount the interactions in a given room
* allow.
*
* @param room_data *room Which room (to find interactions in).
* @param int depletion_type Which depletion we're looking for.
* @return int Detected deletion cap if any; -1 if not detected.
*/
int get_depletion_max(room_data *room, int depletion_type) {
	int iter, list_size, max, best = -1;
	struct interaction_item *interact, *list[4] = { NULL, NULL, NULL, NULL };
	
	if (!room) {
		return best;
	}
	
	// build lists of interactions to check
	list_size = 0;
	list[list_size++] = GET_SECT_INTERACTIONS(SECT(room));
	if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && ROOM_CROP(room)) {
		list[list_size++] = GET_CROP_INTERACTIONS(ROOM_CROP(room));
	}
	if (GET_BUILDING(room) && IS_COMPLETE(room)) {
		list[list_size++] = GET_BLD_INTERACTIONS(GET_BUILDING(room));
	}
	if (GET_ROOM_TEMPLATE(room)) {
		list[list_size++] = GET_RMT_INTERACTIONS(GET_ROOM_TEMPLATE(room));
	}
	
	// check all lists
	for (iter = 0; iter < list_size; ++iter) {
		LL_FOREACH(list[iter], interact) {
			if (determine_depletion_type(interact) == depletion_type) {
				max = interact_data[interact->type].one_at_a_time ? interact->quantity : DEPLETION_LIMIT(room);
				best = MAX(best, max);
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
* Determines the visible height of a map room, as blocked by whatever is on
* it (building; tallest vehicle).
*
* @param room_data *room The map room.
* @param bool *blocking_vehicle Optional: Will bind TRUE to this if there's a vehicle blocking at that height.
* @return int The effective height of the tile.
*/
int get_room_blocking_height(room_data *room, bool *blocking_vehicle) {
	vehicle_data *veh;
	int height = 0, best_veh = 0;
	
	if (blocking_vehicle) {
		// initialize
		*blocking_vehicle = FALSE;
	}
	
	if (!room) {
		return 0;
	}
	
	// ignore negative heights: these are used to track water flow
	height += MAX(0, ROOM_HEIGHT(room));
	
	if (GET_BUILDING(room) && IS_COMPLETE(room)) {
		height += GET_BLD_HEIGHT(GET_BUILDING(room));
	}
	
	// look for tallest vehicle if room is open
	if (!ROOM_IS_CLOSED(room)) {
		DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
			if (VEH_IS_COMPLETE(veh) && VEH_FLAGGED(veh, VEH_OBSCURE_VISION)) {
				best_veh = MAX(best_veh, VEH_HEIGHT(veh));
				if (blocking_vehicle) {
					*blocking_vehicle = TRUE;
				}
			}
		}
		height += best_veh;
	}
	
	return height;
}


/**
* Find an optimal place to start upon new login or death.
*
* @param char_data *ch The player to find a loadroom for.
* @return room_data* The location of a place to start.
*/
room_data *find_load_room(char_data *ch) {
	struct empire_territory_data *ter, *next_ter;
	room_data *rl, *rl_last_room, *found, *map;
	int num_found = 0;
	sh_int island;
	bool veh_ok;
	
	// preferred graveyard?
	if (!IS_NPC(ch) && (rl = real_room(GET_TOMB_ROOM(ch))) && room_has_function_and_city_ok(GET_LOYALTY(ch), rl, FNC_TOMB) && can_use_room(ch, rl, GUESTS_ALLOWED) && !IS_BURNING(rl)) {
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
			if (room_has_function_and_city_ok(GET_LOYALTY(ch), ter->room, FNC_TOMB) && IS_COMPLETE(ter->room) && GET_ISLAND_ID(ter->room) == island && !IS_BURNING(ter->room)) {
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
	return find_starting_location(real_room(GET_LAST_ROOM(ch)));
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
* This determines if room is close enough to a sect with certain flags. It also
* checks the base sector, e.g. if a road or bridge is over the flagged sect.
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
	
	if (!(real = (GET_MAP_LOC(room) ? real_room(GET_MAP_LOC(room)->vnum) : NULL))) {	// no map location
		return FALSE;
	}
	
	for (x = -1 * distance; x <= distance; ++x) {
		for (y = -1 * distance; y <= distance; ++y) {
			if (!(shift = real_shift(real, x, y))) {
				continue;	// no room
			}
			if (with_flags && !ROOM_SECT_FLAGGED(shift, with_flags) && !IS_SET(GET_SECT_FLAGS(BASE_SECT(shift)), with_flags)) {
				continue;	// missing with-flags
			}
			if (without_flags && (ROOM_SECT_FLAGGED(shift, without_flags) || IS_SET(GET_SECT_FLAGS(BASE_SECT(shift)), without_flags))) {
				continue;
			}
			if (compute_distance(room, shift) > distance) {
				continue;
			}
			
			// found!
			return TRUE;
		}
	}
	
	return FALSE; // not found
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
* Find a starting location near a given room, or a random starting location.
*
* @param room_data *near_room Optional: Gets the start loc closest to here if provided, or a random one if NULL.
* @return room_data* A random starting location.
*/
room_data *find_starting_location(room_data *near_room) {
	int iter, dist, best, best_dist;
	
	if (highest_start_loc_index < 0) {
		return NULL;
	}
	
	if (near_room) {
		// find nearest
		best = 0;
		best_dist = -1;
		for (iter = 0; iter <= highest_start_loc_index; ++iter) {
			dist = compute_distance(near_room, real_room(start_locs[iter]));
			if (best_dist == -1 || dist < best_dist) {
				best = iter;
				best_dist = dist;
			}
		}
		
		return real_room(start_locs[best]);
	}
	else {
		// random
		return real_room(start_locs[number(0, highest_start_loc_index)]);
	}
}


/**
 * Attempt to find a random starting location, other than the one you're currently in
 *
 * @return room_data* A random starting location that's less likely to be your current one
 */
room_data *find_other_starting_location(room_data *current_room) {
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
* This gets the direction on a 16-direction circle that includes partial
* directions like east-northeast (ene). This helps point players in the
* correct direction. Since these are not legitimate game directions, they are
* returned as strings not intergers.
*
* @param char_data *ch The player viewing it, for purposes of map rotation (Optional: may be NULL).
* @param room_data *from The origin point.
* @param room_data *to The desitination point.
* @param bool abbrev If TRUE, gets e.g. "ene". If FALSE, gets e.g. "east-northeast".
* @return char* The string for the direction. May be an empty string if to == from.
*/
const char *get_partial_direction_to(char_data *ch, room_data *from, room_data *to, bool abbrev) {
	room_data *origin = HOME_ROOM(from), *dest = HOME_ROOM(to);
	int from_x = X_COORD(origin), from_y = Y_COORD(origin);
	int to_x = X_COORD(dest), to_y = Y_COORD(dest);
	int x_diff = to_x - from_x, y_diff = to_y - from_y;
	int iter;
	double radians, slope, degrees;
	
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
		return "";
	}
	else if (x_diff == 0) {
		degrees = (y_diff > 0 ? 90 : 270);
	}
	else {
		slope = (double) y_diff / (double) x_diff;
		radians = atan(slope);
		
		// convert to degrees and adjust for direction
		degrees = 180 * radians / M_PI;
		if (x_diff < 0) {
			// quadrant II or III: add 180
			degrees += 180;
		}
		else if (y_diff < 0) {
			// quadrant IV: add 360
			degrees += 360;
		}
		else {	// (y_diff > 0)
			// quadrant I: no adjustment
		}
	}
	
	// rotate for character
	if (ch) {
		degrees += get_north_for_char(ch) * 90.0;
		if (degrees >= 360.0) {
			degrees -= 360.0;
		}
	}
	
	// convert 0 to 360 to help with the detection below
	if (degrees <= .001) {
		degrees = 360.0;
	}
	
	// each dir is 22.5 degrees of the circle (11.25 each way)
	// so we remove that first 11.25 and do East (0 degrees) at the end...
	for (iter = 0; *partial_dirs[iter][1] != '\n'; ++iter) {
		if ((degrees - 11.25) < ((iter + 1) * 22.5)) {
			// found!
			return partial_dirs[iter][(abbrev ? 1 : 0)];
		}
	}
	
	// if we got here, it's only because it's the last direction: east
	return partial_dirs[iter][(abbrev ? 1 : 0)];
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
	struct icon_data *icon;
	char buffer[256];
	char *temp;

	// don't do it if a custom icon is set (or no room provided)
	if (!room || ROOM_CUSTOM_ICON(room)) {
		return;
	}
	if (SHARED_DATA(room) == &ocean_shared_data) {
		return;	// never on the ocean
	}

	if (!(icon = use_icon)) {
		icon = get_icon_from_set(GET_SECT_ICONS(SECT(room)), GET_SEASON(room));
	}
	
	// did we find one
	if (icon) {
		// prepend color (it's not automatically there)
		snprintf(buffer, sizeof(buffer), "%s%s", icon->color, icon->icon);
	
		// check for variable colors that must be stored
		if (strstr(buffer, "&?")) {
			temp = str_replace("&?", icon->color, buffer);
			set_room_custom_icon(room, temp);
			free(temp);
		}
		else {
			set_room_custom_icon(room, buffer);
		}
	}
	// else nothing to do
}


/**
* Variant of lock_icon when a room is not available. Only works if there isn't
* a custom icon yet.
*
* @param struct map_data *loc The location to lock.
* @param struct icon_data *use_icon Optional: Force it to use this icon (may be NULL).
*/
void lock_icon_map(struct map_data *loc, struct icon_data *use_icon) {
	struct icon_data *icon;
	char buffer[256];
	
	// safety first
	if (!loc || loc->shared->icon) {
		return;	// don't do it if a custom icon is set (or no location provided)
	}
	if (loc->shared == &ocean_shared_data) {
		return;	// never on the ocean
	}

	if (!(icon = use_icon)) {
		icon = get_icon_from_set(GET_SECT_ICONS(loc->sector_type), y_coord_to_season[MAP_Y_COORD(loc->vnum)]);
	}
	
	// did we find one
	if (icon) {
		if (loc->shared->icon) {
			free(loc->shared->icon);
		}
		
		// prepend color code
		snprintf(buffer, sizeof(buffer), "%s%s", icon->color, icon->icon);
	
		// finally, check for variable colors that must be stored
		if (strstr(buffer, "&?")) {
			// str_replace allocates a new string
			loc->shared->icon = str_replace("&?", icon->color, buffer);
		}
		else {
			loc->shared->icon = str_dup(buffer);
		}
		
		request_world_save(loc->vnum, WSAVE_ROOM);
	}
	// if no icon, no work
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
	
	DL_FOREACH_SAFE2(ROOM_PEOPLE(room), ch, next_ch, next_in_room) {
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
			enter_triggers(ch, NO_DIR, "system", FALSE);
			qt_visit_room(ch, IN_ROOM(ch));
			GET_LAST_DIR(ch) = NO_DIR;
			look_at_room(ch);
			act("$n arrives.", TRUE, ch, NULL, NULL, TO_ROOM);
			greet_triggers(ch, NO_DIR, "system", FALSE);
			msdp_update_room(ch);
		}
	}
}


/**
* Determines if any room is light. This replaces the IS_LIGHT/IS_REAL_LIGHT
* macro family.
*
* @param room_data *room The room to check (which may be light).
* @param bool count_adjacent_light If TRUE, light cascades from adjacent tiles.
* @param bool ignore_magic_darkness If TRUE, ignores ROOM_AFF_MAGIC_DARKNESS -- presumably because you already checked it.
* @return bool TRUE if the room is light, FALSE if not.
*/
bool room_is_light(room_data *room, bool count_adjacent_light, bool ignore_magic_darkness) {
	if (!ignore_magic_darkness && MAGIC_DARKNESS(room)) {
		return FALSE;	// always dark
	}
	
	// things that make the room light
	if (ROOM_LIGHTS(room) > 0 || RMT_FLAGGED(room, RMT_LIGHT)) {
		return TRUE;	// not dark: has a light source
	}
	if (IS_ANY_BUILDING(room) && (ROOM_OWNER(room) || ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE))) {
		return TRUE;	// not dark: claimed (or unclaimable) building
	}
	if (!RMT_FLAGGED(room, RMT_DARK) && get_sun_status(room) != SUN_DARK) {
		return TRUE;	// not dark: it isn't dark outside
	}
	if (ROOM_OWNER(room) && EMPIRE_HAS_TECH(ROOM_OWNER(room), TECH_CITY_LIGHTS) && get_territory_type_for_empire(room, ROOM_OWNER(room), FALSE, NULL, NULL) != TER_FRONTIER) {
		return TRUE;	// not dark: city lights
	}
	if (count_adjacent_light && adjacent_room_is_light(room, ignore_magic_darkness)) {
		return TRUE;	// not dark: adjacent room is light
	}
	
	// otherwise: it's dark
	return FALSE;
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
	
	// to draw better lines, this uses "y as a function of x" for low slopes and "x as a function of y" for high slopes
	if (ABSOLUTE(dy) <= ABSOLUTE(dx)) {
		// y as a function of x
		slope = dy / dx;
	
		// moving left
		if (dx < 0) {
			iter *= -1;
		}
	
		new_x = x1 + iter;
		new_y = y1 + round(slope * (double)iter);
	}
	else {
		// x as a function of y
		slope = dx / dy;
	
		// moving down
		if (dy < 0) {
			iter *= -1;
		}
	
		new_y = y1 + iter;
		new_x = x1 + round(slope * (double)iter);
	}
	
	// bounds check
	new_x = WRAP_X_COORD(new_x);
	new_y = WRAP_Y_COORD(new_y);
	
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
	if (room && GET_MAP_LOC(room)) {
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
	if (room && GET_MAP_LOC(room)) {
		return MAP_Y_COORD(GET_MAP_LOC(room)->vnum);
	}
	else {
		return -1;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MISC UTILS //////////////////////////////////////////////////////////////

/**
* Adds an entry to a pair-hash. This is a general tool for hashing simple pairs
* of numbers. IDs are unique (it will overwrite a duplicate). Example:
*   struct pair_hash *my_hash = NULL;	// initialize to null
*	add_pair_hash(&my_hash, 123, 321);	// add item {123, 321}
*   HASH_ITER(hh, my_hash, iter, next) { ... }	// use hash
*   free_pair_hash(&my_hash);	// free when done
*
* @param struct pair_hash **hash A pointer to the hash we're adding to.
* @param int int The id to add to the hash (first part of the pair).
* @param int value The value (second part of the pair).
*/
void add_pair_hash(struct pair_hash **hash, int id, int value) {
	struct pair_hash *item = NULL;
	
	if (hash) {
		HASH_FIND_INT(*hash, &id, item);
		if (!item) {
			CREATE(item, struct pair_hash, 1);
			item->id = id;
			HASH_ADD_INT(*hash, id, item);
		}
		
		item->value = value;
	}
}


/**
* Adds an entry to a string hash. This is a general tool for listing/counting
* unique names/things. Example:
*   struct string_hash *my_hash = NULL;	// initialize to null
*	add_string_hash(&my_hash, string, 1);	// add items
*   HASH_ITER(hh, my_hash, iter, next) { ... }	// use hash
*   free_string_hash(&my_hash);	// free when done
*
* @param struct string_hash **hash A pointer to the hash we're adding to.
* @param const char *string The string to add, if unique (will be copied for this).
* @param int count How many to add (usually 1, but you can add any amount including a negative).
*/
void add_string_hash(struct string_hash **hash, const char *string, int count) {
	struct string_hash *item = NULL;
	
	if (hash && string) {
		HASH_FIND_STR(*hash, string, item);
		if (!item) {
			CREATE(item, struct string_hash, 1);
			item->str = str_dup(string);
			HASH_ADD_STR(*hash, str, item);
		}
		
		SAFE_ADD(item->count, count, INT_MIN, INT_MAX, FALSE);
	}
}


/**
* Adds an entry to a vnum hash. This is a general tool for listing/counting
* unique vnums. Example:
*   struct vnum_hash *my_hash = NULL;	// initialize to null
*	add_vnum_hash(&my_hash, 123, 1);	// add items
*   HASH_ITER(hh, my_hash, iter, next) { ... }	// use hash
*   free_vnum_hash(&my_hash);	// free when done
*
* @param struct vnum_hash **hash A pointer to the hash we're adding to.
* @param any_vnum vnum The vnum to add.
* @param int count How many to add (usually 1, but you can add any amount including a negative).
*/
void add_vnum_hash(struct vnum_hash **hash, any_vnum vnum, int count) {
	struct vnum_hash *item = NULL;
	
	if (hash) {
		HASH_FIND_INT(*hash, &vnum, item);
		if (!item) {
			CREATE(item, struct vnum_hash, 1);
			item->vnum = vnum;
			HASH_ADD_INT(*hash, vnum, item);
		}
		
		SAFE_ADD(item->count, count, INT_MIN, INT_MAX, FALSE);
	}
}


/**
* Finds an entry in a pair hash by id.
*
* @param struct pair_hash *hash The hash to search.
* @param int id The id to look for.
* @return struct pair_hash* The entry in the hash, if it exists, or else NULL.
*/
struct pair_hash *find_in_pair_hash(struct pair_hash *hash, int id) {
	struct pair_hash *item = NULL;
	HASH_FIND_INT(hash, &id, item);
	return item;
}


/**
* Finds an entry in a string hash by string match.
*
* @param struct string_hash *hash The hash to search.
* @param const char *string The string to look for.
* @return struct string_hash* The entry in the hash, if it exists, or else NULL.
*/
struct string_hash *find_in_string_hash(struct string_hash *hash, const char *string) {
	struct string_hash *item = NULL;
	HASH_FIND_STR(hash, string, item);
	return item;
}


/**
* Finds an entry in a vnum hash by vnum.
*
* @param struct vnum_hash *hash The hash to search.
* @param any_vnum vnum The vnum to look for.
* @return struct vnum_hash* The entry in the hash, if it exists, or else NULL.
*/
struct vnum_hash *find_in_vnum_hash(struct vnum_hash *hash, any_vnum vnum) {
	struct vnum_hash *item = NULL;
	HASH_FIND_INT(hash, &vnum, item);
	return item;
}


/**
* Frees a pair_hash when you're done with it.
*
* @param struct pair_hash **hash The pair hash to free.
*/
void free_pair_hash(struct pair_hash **hash) {
	struct pair_hash *iter, *next;
	
	if (hash) {
		HASH_ITER(hh, *hash, iter, next) {
			HASH_DEL(*hash, iter);
			free(iter);
		}
	}
}


/**
* Frees a string_hash when you're done with it.
*
* @param struct string_hash **hash The string hash to free.
*/
void free_string_hash(struct string_hash **hash) {
	struct string_hash *iter, *next;
	
	if (hash) {
		HASH_ITER(hh, *hash, iter, next) {
			HASH_DEL(*hash, iter);
			
			if (iter->str) {
				free(iter->str);
			}
			free(iter);
		}
	}
}


/**
* Frees a vnum_hash when you're done with it.
*
* @param struct vnum_hash **hash The vnum hash to free.
*/
void free_vnum_hash(struct vnum_hash **hash) {
	struct vnum_hash *iter, *next;
	
	if (hash) {
		HASH_ITER(hh, *hash, iter, next) {
			HASH_DEL(*hash, iter);
			free(iter);
		}
	}
}


// easy alpha sorter
int sort_string_hash(struct string_hash *a, struct string_hash *b) {
	return strcmp(a->str, b->str);
}


/**
* Builds a string from the contents of a string_hash. This does NOT free the
* hash in the process.
*
* @param struct string_hash *str_hash The string hash to convert to a string.
* @param char *to_string The buffer to save it to.
* @param size_t string_size Limit for to_string, to prevent buffer overruns.
* @param bool show_count If TRUE, shows (x123) after anything with a count over 1; if FALSE never shows the number.
* @param bool use_commas If TRUE, puts commas between entries; if FALSE only shows spaces.
* @param bool use_and If TRUE, puts an "and" before the last entry; if FALSE, just has a comma (or space if no commas).
*/
void string_hash_to_string(struct string_hash *str_hash, char *to_string, size_t string_size, bool show_count, bool use_commas, bool use_and) {
	struct string_hash *str_iter, *next_str;
	char entry[MAX_STRING_LENGTH];
	size_t entry_size, cur_size;
	
	if (!to_string) {
		return;	// baffling
	}
	
	*to_string = '\0';
	cur_size = 0;
	
	if (!str_hash) {
		return;	// nothing else to do
	}
	
	HASH_ITER(hh, str_hash, str_iter, next_str) {
		// build one entry
		*entry = '\0';
		entry_size = 0;
		
		// start: leading and?
		if (use_and && !next_str && str_iter != str_hash) {
			// last entry and not the first entry
			entry_size += snprintf(entry + entry_size, sizeof(entry) - entry_size, "and ");
		}
		
		// name
		entry_size += snprintf(entry + entry_size, sizeof(entry) - entry_size, "%s", str_iter->str);
		
		// count?
		if (str_iter->count > 1 && show_count) {
			entry_size += snprintf(entry + entry_size, sizeof(entry) - entry_size, " (x%d)", str_iter->count);
		}
		
		// trailing comma?
		if (use_commas && next_str && (!use_and || HASH_COUNT(str_hash) > 2)) {
			if (entry_size + 2 < sizeof(entry)) {
				strcat(entry, ", ");
				entry_size += 2;
			}
		}
		else if (next_str) {
			if (entry_size + 1 < sizeof(entry)) {
				strcat(entry, " ");
				++entry_size;
			}
		}
		
		// now append?
		if (cur_size + entry_size < string_size - 12) {
			strcat(to_string, entry);
			cur_size += entry_size;
		}
		else {
			cur_size += snprintf(to_string + cur_size, string_size - cur_size, "**OVERFLOW**");
			break;
		}
	}
}


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
	if ((diff > 0 && parts < 2) || (diff == 0 && parts == 0)) {
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
* in-city requirements. (If the room does not have FNC_IN_CITY_ONLY, this only
* checks the function.) Note that the UPGRADED flag is checked on its own
* inside this function, and having that flag on the home-room of a building
* will apply it to everything inside.
*
* Note: If a player has no empire, they generally pass the for_emp part of this
* test and you should still check use-permission separately, too.
*
* @param empire_data *for_emp Optional: Which empire wants to use the functions. If NULL, no empire/owner check is made.
* @param room_data *room The room to check.
* @param bitvector_t fnc_flag Any FNC_ flag -- only requires 1 of these (except UPGRADED, which is always required in addition to one of the other flags, if present).
* @return bool TRUE if the room has the function and passed the city check, FALSE if not.
*/
bool room_has_function_and_city_ok(empire_data *for_emp, room_data *room, bitvector_t fnc_flag) {
	vehicle_data *veh;
	bool junk, upgraded;
	
	// are we checking for upgraded?
	upgraded = IS_SET(fnc_flag, FNC_UPGRADED);
	fnc_flag &= ~FNC_UPGRADED;
	
	// check vehicles first
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (upgraded && !IS_SET(VEH_FUNCTIONS(veh), FNC_UPGRADED)) {
			continue;
		}
		if (!IS_SET(VEH_FUNCTIONS(veh), fnc_flag)) {
			continue;	// no function
		}
		if (!VEH_IS_COMPLETE(veh) || VEH_HEALTH(veh) < 1) {
			continue;
		}
		if (for_emp && VEH_OWNER(veh) && VEH_OWNER(veh) != for_emp && !emp_can_use_vehicle(for_emp, veh, GUESTS_ALLOWED)) {
			continue;
		}
		if (VEH_FLAGGED(veh, MOVABLE_VEH_FLAGS) && IS_SET(fnc_flag, IMMOBILE_FNCS)) {
			continue;	// exclude certain functions on movable vehicles (functions that require room data)
		}
		if (IS_SET(VEH_FUNCTIONS(veh), FNC_IN_CITY_ONLY) && (!ROOM_OWNER(room) || get_territory_type_for_empire(room, ROOM_OWNER(room), TRUE, &junk, NULL) != TER_CITY)) {
			continue;	// not in-city but needs it
		}
		
		return TRUE;	// vehicle allows it
	}
	
	// otherwise check the room itself
	if (upgraded && (!HAS_FUNCTION(room, FNC_UPGRADED) || !IS_COMPLETE(room)) && (!HAS_FUNCTION(HOME_ROOM(room), FNC_UPGRADED) || !IS_COMPLETE(HOME_ROOM(room)))) {
		return FALSE;
	}
	if (!HAS_FUNCTION(room, fnc_flag) || !IS_COMPLETE(room)) {
		return FALSE;
	}
	if (for_emp && ROOM_OWNER(room) && ROOM_OWNER(room) != for_emp && !emp_can_use_room(for_emp, room, GUESTS_ALLOWED)) {
		return FALSE;	// ownership failed
	}
	if (!check_in_city_requirement(room, TRUE)) {
		return FALSE;
	}
	
	// oh okay
	return TRUE;
}


/**
* Determines if a vehicle both has a function flag, and passes any necessary
* in-city requirements. (If it does not have FNC_IN_CITY_ONLY, this only
* checks the function.)
*
* A vehicle that is not owned cannot be in-city. The vehicle must be in the
* radius of its owner's city, and not on land claimed by another empire.
*
* @param vehicle_data *veh The vehicle to test.
* @param bitvector_t fnc_flag Any FNC_ flag.
* @return bool TRUE if the vehicle has the function and passed the city check, FALSE if not.
*/
bool vehicle_has_function_and_city_ok(vehicle_data *veh, bitvector_t fnc_flag) {
	empire_data *emp = VEH_OWNER(veh);
	room_data *room = IN_ROOM(veh);
	bool junk;
	
	if (!VEH_IS_COMPLETE(veh) || VEH_HEALTH(veh) < 1 || !IS_SET(VEH_FUNCTIONS(veh), fnc_flag)) {
		return FALSE;	// no function
	}
	if (VEH_FLAGGED(veh, MOVABLE_VEH_FLAGS) && IS_SET(fnc_flag, IMMOBILE_FNCS)) {
		return FALSE;	// exclude certain functions on movable vehicles (functions that require room data)
	}
	
	// for checks that do require in-city
	if (IS_SET(VEH_FUNCTIONS(veh), FNC_IN_CITY_ONLY)) {
		if (!emp) {
			return FALSE;	// no owner = cannot be in-city
		}
		if (ROOM_OWNER(room) && ROOM_OWNER(room) != emp) {
			return FALSE;	// someone else's claim is not in our city
		}
		if (get_territory_type_for_empire(room, emp, TRUE, &junk, NULL) != TER_CITY) {
			return FALSE;	// not in-city for us either
		}
	}
	
	return TRUE;	// if we made it this far
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
	DL_FOREACH2(player_character_list, ch, next_plr) {
		if (!ch->desc) {
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
			queue_delayed_update(ch, CDU_SAVE);
		}
	}
}
