/* ************************************************************************
*   File: event.c                                         EmpireMUD 2.0b5 *
*  Usage: timed global events that reward players for participation       *
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
#include "dg_event.h"
#include "dg_scripts.h"
#include "vnums.h"

/**
* Contents:
*   Helpers
*   Leaderboard Helpers
*   Player Event Data
*   Running Events
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   OLC Modules
*   Events Command
*/

// local data
const char *default_event_name = "Unnamed Event";
const char *default_event_description = "This event has no description.\r\n";
const char *default_event_complete_msg = "The event has ended.\r\n";

// external consts
extern const char *event_types[];
extern const char *event_flags[];
extern const char *olc_type_bits[NUM_OLC_TYPES+1];
extern const char *quest_reward_types[];

// external funcs
extern bool delete_quest_reward_from_list(struct quest_reward **list, int type, any_vnum vnum);
extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
extern bool find_quest_reward_in_list(struct quest_reward *list, int type, any_vnum vnum);
extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);

// local protos
EVENT_CANCEL_FUNC(cancel_event_event);
struct player_event_data *get_event_data(char_data *ch, int event_id);
void schedule_event_event(struct event_running_data *erd);
int sort_event_rewards(struct event_reward *a, struct event_reward *b);
void update_player_leaderboard(char_data *ch, struct event_running_data *re, struct player_event_data *ped);


#define EVENT_CMD(name)		void (name)(char_data *ch, char *argument)


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Gets a character's rank in an event. It returns NOTHING if ch is unranked.
*
* @param char_data *ch the player.
* @param struct event_running_data *re The event to check rank for.
* @return int The rank, or NOTHING if not ranked in the event.
*/
int get_event_rank(char_data *ch, struct event_running_data *re) {
	struct event_leaderboard *lb, *next_lb;
	int rank = 0, total_rank = 0, last_points = -1;
	
	bool need_appr = config_get_bool("event_approval");
	
	if (!ch || !re) {
		return NOTHING;
	}
	
	HASH_ITER(hh, re->player_leaderboard, lb, next_lb) {
		// compute rank
		if (!lb->ignore && (lb->approved || !need_appr)) {	// otherwise they don't count toward rank
			++total_rank;
			if (lb->points != last_points) {	// not a tie
				rank = total_rank;
			}
			// else: leave rank where it was (tie)
			last_points = lb->points;
			
			// it me?
			if (lb->id == GET_IDNUM(ch)) {
				return rank;
			}
		}
		else if (lb->id == GET_IDNUM(ch)) {
			// found me but I don't qualify for a rank
			return NOTHING;
		}
	}
	
	// not found
	return NOTHING;
}


/**
* Ends an event that was running.
*
* @param event_data *event
*/
void end_event(struct event_running_data *re) {
	struct player_event_data *ped;
	event_data *event;
	char_data *ch;
	
	// basic safety
	if (!re || !(event = re->event) || re->status != EVTS_RUNNING) {
		return;
	}
	
	// change status
	re->status = EVTS_COMPLETE;
	events_need_save = TRUE;
	
	// update all in-game players so they have their current rank
	LL_FOREACH(character_list, ch) {
		if (IS_NPC(ch) || !(ped = get_event_data(ch, re->id))) {
			continue;	// no work
		}
		
		ped->status = EVTS_COMPLETE;	// but not collected
		ped->rank = get_event_rank(ch, re);
	}
	
	// announce
	syslog(SYS_INFO, LVL_START_IMM, TRUE, "EVENT: [%d] %s (id %d) has ended", EVT_VNUM(event), EVT_NAME(event), re->id);
	log_to_slash_channel_by_name(EVENT_LOG_CHANNEL, NULL, "%s has ended!", EVT_NAME(event));
	
	qt_event_start_stop(EVT_VNUM(event));
	et_event_start_stop(EVT_VNUM(event));
	
	// unschedule event?
	if (re->next_dg_event) {
		dg_event_cancel(re->next_dg_event, cancel_event_event);
		re->next_dg_event = NULL;
	}
}


/**
* Quick way to turn a vnum into a name, safely.
*
* @param any_vnum vnum The event vnum to look up.
* @return char* A name for the vnum, or "UNKNOWN".
*/
char *get_event_name_by_proto(any_vnum vnum) {
	event_data *proto = find_event_by_vnum(vnum);
	return proto ? EVT_NAME(proto) : "UNKNOWN";
}


/**
* Wraps quest_reward_string for use with event rewards.
*
* @param struct event_reward *reward The reward to show.
* @param bool show_vnums If TRUE, adds [1234] at the start of the string.
* @return char* The string display.
*/
char *event_reward_string(struct event_reward *reward, bool show_vnums) {
	extern char *quest_reward_string(struct quest_reward *reward, bool show_vnums);

	struct quest_reward qr;

	// borrow data to use the other reward string func
	qr.type = reward->type;
	qr.vnum = reward->vnum;
	qr.amount = reward->amount;
	
	return quest_reward_string(&qr, show_vnums);
}


/**
* This function will return the 1 currently-running event, if there is only
* one. It can optionally also get the total number of events currently-running.
*
* The reason this function returns NULL if there is more than 1 event running
* is that many functions behave differently when only 1 is running, as a short-
* cut.
*
* @param int *count Optional: A variable to pass back the count of running events.
* @return struct event_running_data* If there is only 1 event running, this returns it. In all other cases, it returns NULL.
*/
struct event_running_data *only_one_running_event(int *count) {
	struct event_running_data *running, *only = NULL;
	int num;
	
	// count or find only event
	num = 0;
	LL_FOREACH(running_events, running) {
		if (running->event && running->status == EVTS_RUNNING) {
			++num;
			only = running;
		}
	}
	
	// store count if requested
	if (count) {
		*count = num;
	}
	
	if (num == 1) {
		return only;	// ONLY if there's only 1
	}
	else {
		return NULL;
	}
}


/**
* Copies entries from one list into another, only if they are not already in
* the to_list.
*
* @param struct event_reward **to_list A pointer to the destination list.
* @param struct event_reward *from_list The list to copy from.
*/
void smart_copy_event_rewards(struct event_reward **to_list, struct event_reward *from_list) {
	struct event_reward *iter, *search, *reward;
	bool found;
	
	LL_FOREACH(from_list, iter) {
		// ensure not already in list
		found = FALSE;
		LL_FOREACH(*to_list, search) {
			if (search->type == iter->type && search->amount == iter->amount && search->vnum == iter->vnum && search->min == iter->max && search->max == iter->max) {
				found = TRUE;
				break;
			}
		}
		
		// add it
		if (!found) {
			CREATE(reward, struct event_reward, 1);
			reward->min = iter->min;
			reward->max = iter->max;
			reward->type = iter->type;
			reward->amount = iter->amount;
			reward->vnum = iter->vnum;
			LL_APPEND(*to_list, reward);
		}
	}
}


/**
* Looks up an event by name or vnum, optionally requiring that the event be
* active right now.
*
* @param char *name The string (name/vnum) to look for.
* @param bool running_only If TRUE, skips events that aren't running.
* @return event_data* The event prototype found, if any.
*/
event_data *smart_find_event(char *name, bool running_only) {
	event_data *iter, *next_iter, *partial = NULL;
	any_vnum find_vnum = (isdigit(*name) ? atoi(name) : NOTHING);
	
	HASH_ITER(hh, event_table, iter, next_iter) {
		if (EVT_FLAGGED(iter, EVTF_IN_DEVELOPMENT)) {
			continue;	// skip in-dev
		}
		if (running_only && !find_running_event_by_vnum(EVT_VNUM(iter))) {
			continue;	// not running
		}
		
		if (find_vnum != NOTHING && find_vnum == EVT_VNUM(iter)) {
			return iter;	// vnum match
		}
		else if (!str_cmp(name, EVT_NAME(iter))) {
			return iter;	// FOUND!
		}
		else if (!partial && is_multiword_abbrev(name, EVT_NAME(iter))) {
			partial = iter;	// partial match
		}
	}
	
	return partial;	// if any
}


/**
* Starts an event running.
*
* @param event_data *event
*/
void start_event(event_data *event) {
	struct event_running_data *re;
	
	// basic safety
	if (!event) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: start_event called with no event");
		return;
	}
	if (find_running_event_by_id(EVT_VNUM(event))) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: start_event called on event that's already running");
		return;
	}
	if (EVT_FLAGGED(event, EVTF_IN_DEVELOPMENT)) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: start_event called with on IN-DEV event");
		return;
	}
	
	// let's a-go
	CREATE(re, struct event_running_data, 1);
	re->id = ++top_event_id;
	re->event = event;
	re->start_time = time(0);	// TODO add a way to delay-start?
	re->status = EVTS_RUNNING;	// ^ would set it to EVTS_NOT_STARTED if so
	
	LL_PREPEND(running_events, re);	// newest always first (must sort if delay-start is added)
	events_need_save = TRUE;
	
	// announce
	syslog(SYS_INFO, LVL_START_IMM, TRUE, "EVENT: [%d] %s has started with event id %d", EVT_VNUM(event), EVT_NAME(event), re->id);
	log_to_slash_channel_by_name(EVENT_LOG_CHANNEL, NULL, "%s has begun!", EVT_NAME(event));
	
	qt_event_start_stop(EVT_VNUM(event));
	et_event_start_stop(EVT_VNUM(event));
	
	schedule_event_event(re);
}


 //////////////////////////////////////////////////////////////////////////////
//// LEADERBOARD HELPERS /////////////////////////////////////////////////////

/**
* Fetches (or creates) a leaderboard entry for the given id. This is used for
* both the empire and player leaderboards.
*
* @param struct event_leaderboard **hash The has to search in.
* @param int id The id of the empire/player.
* @return struct event_leaderboard* The entry for that id.
*/
struct event_leaderboard *get_event_leaderboard_entry(struct event_leaderboard **hash, int id) {
	struct event_leaderboard *el;
	
	HASH_FIND_INT(*hash, &id, el);
	if (!el) {
		CREATE(el, struct event_leaderboard, 1);
		el->id = id;
		HASH_ADD_INT(*hash, id, el);
	}
	
	return el;
}


/**
* Frees a whole leaderboard.
*
* @param struct event_leaderboard *hash The leaderboard hash to free.
*/
void free_event_leaderboard(struct event_leaderboard *hash) {
	struct event_leaderboard *el, *next_el;
	
	HASH_ITER(hh, hash, el, next_el) {
		HASH_DEL(hash, el);
		free(el);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER EVENT DATA ///////////////////////////////////////////////////////

/**
* Called on login to make sure all of a player's event data is valid.
*
* @param char_data *ch The character to check.
*/
void check_player_events(char_data *ch) {
	struct player_event_data *ped, *next_ped;
	struct event_running_data *running;
	
	if (IS_NPC(ch)) {
		return;	// safety first
	}
	
	HASH_ITER(hh, GET_EVENT_DATA(ch), ped, next_ped) {
		// event does not exist, or has an invalid event id
		if (!ped->event || ped->id > top_event_id) {
			HASH_DEL(GET_EVENT_DATA(ch), ped);
			free(ped);
			continue;
		}
		
		// check status
		if ((running = find_running_event_by_id(ped->id))) {
			if (running->status == EVTS_COMPLETE && ped->status == EVTS_RUNNING) {
				ped->status = running->status;	// copy event status
				ped->rank = get_event_rank(ch, running);
			}
			else if (running->status == EVTS_RUNNING && ped->status == EVTS_NOT_STARTED) {
				ped->status = running->status;	// just copy this status
			}
			else if (ped->status == EVTS_RUNNING) {
				// any_running = TRUE;
			}
		}
	}
}


/**
* Creates a player_event_data entry and returns it. Or, if one already exists,
* returns that. Either way, you're guaranteed an entry, if your input data is
* good. If you're only trying to fetch existing data, use get_event_data()
* instead.
*
* @param char_data *ch The player character to create (or get) data for.
* @param int event_id The event's unique id.
* @param any_vnum event_vnum Which event the entry is for.
* @return struct player_event_data* The event entry, if possible.
*/
struct player_event_data *create_event_data(char_data *ch, int event_id, any_vnum event_vnum) {
	struct player_event_data *ped = NULL;
	struct event_running_data *running;
	
	if (IS_NPC(ch)) {
		return NULL;	// sanity
	}
	
	HASH_FIND_INT(GET_EVENT_DATA(ch), &event_id, ped);
	if (!ped) {
		CREATE(ped, struct player_event_data, 1);
		ped->id = event_id;
		ped->event = find_event_by_vnum(event_vnum);
		
		if ((running = find_running_event_by_id(event_id))) {
			ped->timestamp = running->start_time;
			ped->status = running->status;
		}
		else {	// backup
			ped->timestamp = time(0);
			ped->status = EVTS_NOT_STARTED;
		}
		
		HASH_ADD_INT(GET_EVENT_DATA(ch), id, ped);
	}
	return ped;
}


/**
* Frees a set of player_event_data, e.g. when the player is being freed.
*
* @param struct player_event_data *hash The event data to free.
*/
void free_player_event_data(struct player_event_data *hash) {
	struct player_event_data *el, *next_el;
	
	HASH_ITER(hh, hash, el, next_el) {
		HASH_DEL(hash, el);
		free(el);
	}
}


/**
* Gains (or loses) a number of points for a currently-running event, by vnum.
* This function fails silently if there is no matching event running, as it's
* possible for quests/scripts to try to grant points without it.
*
* Players receive a message when this happens.
*
* @param char_data *ch The person gaining points.
* @param any_vnum event_vnum Which event.
* @param int points How many points to gain/lose.
* @return int The new point total.
*/
int gain_event_points(char_data *ch, any_vnum event_vnum, int points) {
	struct event_running_data *running;
	struct player_event_data *ped;
	
	if (!ch || !points) {
		return 0;	// no work
	}
	
	if (!(running = find_running_event_by_vnum(event_vnum)) || running->status != EVTS_RUNNING) {
		return 0;	// no running event
	}
	if (!(ped = create_event_data(ch, running->id, event_vnum))) {
		return 0;	// cannot get/create an event data entry
	}
	
	if (GET_HIGHEST_KNOWN_LEVEL(ch) > ped->level) {
		ped->level = GET_HIGHEST_KNOWN_LEVEL(ch);
	}
	SAFE_ADD(ped->points, points, 0, INT_MAX, FALSE);
	update_player_leaderboard(ch, running, ped);
	
	if (points > 0) {
		msg_to_char(ch, "\tyYou gain %d point%s for '%s'! You now have %d point%s.\t0\r\n", points, PLURAL(points), running->event ? EVT_NAME(running->event) : "Unknown Event", ped->points, PLURAL(ped->points));
	}
	else if (points < 0) {
		msg_to_char(ch, "\tyYou lose %d point%s for '%s'! You now have %d point%s.\t0\r\n", ABSOLUTE(points), PLURAL(ABSOLUTE(points)), running->event ? EVT_NAME(running->event) : "Unknown Event", ped->points, PLURAL(ped->points));
	}
	return ped->points;
}


/**
* Gets the event data entry for a player, if it exists. Use create_event_data
* if you need to guarantee it exists.
*
* @param char_data *ch The player character to get data for.
* @param int event_id The event's unique id (not vnum).
* @return struct player_event_data* The event entry, if it exists.
*/
struct player_event_data *get_event_data(char_data *ch, int event_id) {
	struct player_event_data *ped = NULL;
	
	if (IS_NPC(ch)) {
		return NULL;	// sanity
	}
	
	HASH_FIND_INT(GET_EVENT_DATA(ch), &event_id, ped);
	return ped;	// if any
}


/**
* Sets the point total for a currently-running event, by vnum.
* This function fails silently if there is no matching event running, as it's
* possible for quests/scripts to try to grant points without it.
*
* Players do not receive a message for this.
*
* @param char_data *ch The person whose points are changing.
* @param any_vnum event_vnum Which event.
* @param int points How many points to have.
*/
void set_event_points(char_data *ch, any_vnum event_vnum, int points) {
	struct event_running_data *running;
	struct player_event_data *ped;
	
	if (!ch) {
		return;	// no work
	}
	
	if (!(running = find_running_event_by_vnum(event_vnum))) {
		return;	// no running event
	}
	if (!(ped = create_event_data(ch, running->id, event_vnum))) {
		return;	// cannot get/create an event data entry
	}
	
	if (GET_HIGHEST_KNOWN_LEVEL(ch) > ped->level) {
		ped->level = GET_HIGHEST_KNOWN_LEVEL(ch);
	}
	ped->points = points;
	update_player_leaderboard(ch, running, ped);
}


// Simple vnum sorter for event leaderboards
int sort_leaderboard(struct event_leaderboard *a, struct event_leaderboard *b) {
	return b->points - a->points;
}


/**
* Checks and updates the player leaderboard, when someone's score changes.
* This only works on events that are still running, and is called by
* set/gain_event_points().
*
* This does not update rank.
*
* @param char_data *ch The person whose points have changed.
* @param struct event_running_data *re The data for the running-event.
* @param struct player_event_data *ped The player's event data for it.
*/
void update_player_leaderboard(char_data *ch, struct event_running_data *re, struct player_event_data *ped) {
	struct event_leaderboard *lb;
	
	if (!ch || !re || !ped) {
		return;	// no work
	}
	if (re->status != EVTS_RUNNING) {
		return;	// leaderboard cannot be updated after event ends
	}
	
	if ((lb = get_event_leaderboard_entry(&re->player_leaderboard, GET_IDNUM(ch))) && lb->points != ped->points) {
		lb->points = ped->points;
		lb->approved = IS_APPROVED(ch);
		lb->ignore |= IS_IMMORTAL(ch);	// reasons to ignore this result
		HASH_SORT(re->player_leaderboard, sort_leaderboard);
		events_need_save = TRUE;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// RUNNING EVENTS //////////////////////////////////////////////////////////

/**
* Cancels an event without rewarding anybody.
*
* @param struct event_running_data *re The event to cancel.
*/
void cancel_running_event(struct event_running_data *re) {
	struct player_event_data *ped;
	char_data *chiter;
	int id;
	
	if (!re) {
		return;
	}
	
	// announce
	if (re->event) {
		syslog(SYS_INFO, LVL_START_IMM, TRUE, "EVENT: [%d] %s (id %d) has canceled", EVT_VNUM(re->event), EVT_NAME(re->event), re->id);
		log_to_slash_channel_by_name(EVENT_LOG_CHANNEL, NULL, "%s has been canceled", EVT_NAME(re->event));
	}
	
	// look for people with event data and remove it
	id = re->id;
	LL_FOREACH(character_list, chiter) {
		if (IS_NPC(chiter)) {
			continue;
		}
		
		HASH_FIND_INT(GET_EVENT_DATA(chiter), &id, ped);
		if (ped) {
			HASH_DEL(GET_EVENT_DATA(chiter), ped);
			free(ped);
		}
	}
	
	// remove
	LL_DELETE(running_events, re);
	
	if (re->event) {
		qt_event_start_stop(EVT_VNUM(re->event));
		et_event_start_stop(EVT_VNUM(re->event));
	}
	
	// unschedule event?
	if (re->next_dg_event) {
		dg_event_cancel(re->next_dg_event, cancel_event_event);
		re->next_dg_event = NULL;
	}
	
	// and free
	// free_event_leaderboard(re->empire_leaderboard);
	free_event_leaderboard(re->player_leaderboard);
	free(re);
	
	// mark file for save
	events_need_save = TRUE;
}


// frees memory when an event_data event is canceled
EVENT_CANCEL_FUNC(cancel_event_event) {
	struct event_event_data *data = (struct event_event_data *)event_obj;
	free(data);
}


// sends an alert that an event is ending soon
EVENTFUNC(check_event_announce) {
	struct event_event_data *data = (struct event_event_data *)event_obj;
	struct event_running_data *erd = data->running;
	int when;
	
	if (!erd) {	// somehow
		log("SYSERR: check_event_announce called with null event");
		free(data);
		return 0;
	}
	
	// clear this now
	erd->next_dg_event = NULL;
	
	if (erd->event) {
		when = erd->start_time + (EVT_DURATION(erd->event) * SECS_PER_REAL_MIN) - time(0);
		if (when > 0) {
			log_to_slash_channel_by_name(EVENT_LOG_CHANNEL, NULL, "%s will end in %s", EVT_NAME(erd->event), time_length_string(when));
		}
	}
	
	schedule_event_event(erd);	// schedule the next one
	
	return 0;	// do not reenqueue this one
}


// checks if an event should have ended by now
EVENTFUNC(check_event_end) {
	struct event_event_data *data = (struct event_event_data *)event_obj;
	struct event_running_data *erd = data->running;
	
	if (!erd) {	// somehow
		log("SYSERR: check_event_end called with null event");
		free(data);
		return 0;
	}
	
	free(data);
	erd->next_dg_event = NULL;
	end_event(erd);
	return 0;	// do not reenqueue
}


/**
* Finds either the current running version of the event (by vnum) or the last
* time it was run.
*
* @param any_vnum event_vnum Which event we're looking for.
* @return struct event_running_data* The event_running_data entry or NULL if none found.
*/
struct event_running_data *find_last_event_by_vnum(any_vnum event_vnum) {
	struct event_running_data *re, *found = NULL;
	time_t last_time = 0;
	
	LL_FOREACH(running_events, re) {
		if (!re->event || EVT_VNUM(re->event) != event_vnum) {
			continue;	// not what we're looking for
		}
		
		// ok:
		if (re->status == EVTS_RUNNING) {
			return re;	// if we find one running, it's always the right one
		}
		else if (last_time == 0 || re->start_time > last_time) {
			found = re;
			last_time = re->start_time;
		}
	}
	return found;	// if any
}


/**
* Pulls up the entry for a running event by id. This will find it regardless
* of 'status'.
*
* @param int id The id of the event to look up.
* @return struct event_running_data* The event_running_data entry or NULL if none found.
*/
struct event_running_data *find_running_event_by_id(int id) {
	struct event_running_data *re;
	LL_FOREACH(running_events, re) {
		if (re->id == id) {
			return re;
		}
	}
	return NULL;
}


/**
* Pulls up the entry for a running event by event vnum. Hopefully this only
* ever finds one. This will only find events that are in the RUNNING status.
*
* @param any_vnum event_vnum Which event we're looking for.
* @return struct event_running_data* The event_running_data entry or NULL if none found.
*/
struct event_running_data *find_running_event_by_vnum(any_vnum event_vnum) {
	struct event_running_data *re;
	LL_FOREACH(running_events, re) {
		if (re->event && EVT_VNUM(re->event) == event_vnum && re->status == EVTS_RUNNING) {
			return re;
		}
	}
	return NULL;
}


/**
* Load one running event, from the running event file.
*
* @param FILE *fl The open read file.
* @param int id The event id.
* @return struct event_running_data* The event.
*/
struct event_running_data *load_one_running_event(FILE *fl, int id) {
	char errstr[MAX_STRING_LENGTH], line[256];
	struct event_leaderboard *lb;
	struct event_running_data *re;
	int int_in[4];
	long long_in;
	
	snprintf(errstr, sizeof(errstr), "running event %d", id);
	
	// find or create/add
	if ((re = find_running_event_by_id(id))) {
		log("Found duplicate running event id: %d", id);
		// but load it anyway
	}
	else {
		CREATE(re, struct event_running_data, 1);
		re->id = id;
		LL_APPEND(running_events, re);
	}
	
	// line 1: vnum start-time status
	if (!get_line(fl, line) || sscanf(line, "%d %ld %d", &int_in[0], &long_in, &int_in[1]) != 3) {
		log("SYSERR: Format error in line 1 of %s", errstr);
		exit(1);
	}
	
	re->event = find_event_by_vnum(int_in[0]);	// warning: may be NULL / audit later
	re->start_time = long_in;
	re->status = int_in[1];
	
	// line 2: misc/unused
	if (!get_line(fl, line) || sscanf(line, "%d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
		log("SYSERR: Format error in line 2 of %s", errstr);
		exit(1);
	}
	
	// int_in[0-3] not currently used
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in instance, expecting alphabetic flags, got: %s", line);
			exit(1);
		}
		switch (*line) {
			case 'E': {	// empire leaderboard
				if (sscanf(line, "E %d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
					log("SYSERR: Format error in E line of %s: %s", errstr, line);
					// not fatal
				}
				else {
					// not currently used
					// lb = get_event_leaderboard_entry(&re->empire_leaderboard, int_in[0]);
					// lb->points = int_in[1];
					// lb->approved = int_in[2] ? 1 : 0;
					// lb->ignore = int_in[3] ? 1 : 0;
				}
				break;
			}
			case 'P': {	// player leaderboard
				if (sscanf(line, "P %d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
					log("SYSERR: Format error in E line of %s: %s", errstr, line);
					// not fatal
				}
				else {
					lb = get_event_leaderboard_entry(&re->player_leaderboard, int_in[0]);
					lb->points = int_in[1];
					lb->approved = int_in[2] ? 1 : 0;
					lb->ignore = int_in[3] ? 1 : 0;
				}
				break;
			}
			case 'S': {
				// done
				return re;
			}
		}
	}
	
	return re;
}


/**
* Loads the running events file, if there is any.
*/
void load_running_events_file(void) {
	char line[256];
	FILE *fl;
	
	if (!(fl = fopen(RUNNING_EVENTS_FILE, "r"))) {
		// log("SYSERR: Unable to load running events from file.");
		// there just isn't a file -- that's ok, we'll generate new data
		return;
	}
	
	// header info
	if (!get_line(fl, line)) {
		log("WARNING: Running events file is empty.");
		fclose(fl);
		return;
	}
	top_event_id = atoi(line);
	
	// load #-marked entries
	while (get_line(fl, line)) {
		if (*line == '#') {
			load_one_running_event(fl, atoi(line+1));
		}
		else if (*line == '$') {
			// done;
			break;
		}
		else {
			// junk data?
		}
	}
	
	fclose(fl);
}


/**
* Schedules the end of an event, or its next notification.
*
* @param struct event_running_data *erd The running event.
*/
void schedule_event_event(struct event_running_data *erd) {
	struct event_event_data *data;
	time_t end, left;
	
	if (!erd) {
		return;
	}
	
	// cancel any old event
	if (erd->next_dg_event) {
		dg_event_cancel(erd->next_dg_event, cancel_event_event);
		erd->next_dg_event = NULL;
	}
	
	if (!erd->event) {
		return;	// can't really function without an event
	}
	
	// start the new one
	CREATE(data, struct event_event_data, 1);
	data->running = erd;
	
	end = erd->start_time + (EVT_DURATION(erd->event) * SECS_PER_REAL_MIN);
	left = end - time(0);
	
	// announce at...
	if (left > 5 * SECS_PER_REAL_MIN + 1) {
		erd->next_dg_event = dg_event_create(check_event_announce, (void*)data, (left - (5 * SECS_PER_REAL_MIN)) RL_SEC);
	}
	else if (left > 1 * SECS_PER_REAL_MIN + 1) {
		erd->next_dg_event = dg_event_create(check_event_announce, (void*)data, (left - (1 * SECS_PER_REAL_MIN)) RL_SEC);
	}
	else if (left > 30 + 1) {
		erd->next_dg_event = dg_event_create(check_event_announce, (void*)data, (left - 30) RL_SEC);
	}
	else if (left > 0) {	// event almost over
		erd->next_dg_event = dg_event_create(check_event_end, (void*)data, left RL_SEC);
	}
	else {	// event is already over
		free(data);
		end_event(erd);
	}
}


/**
* Audits all the currently-running events, on startup.
*/
void verify_running_events(void) {
	struct event_running_data *re, *next_re;
	bool bad;
	
	LL_FOREACH_SAFE(running_events, re, next_re) {
		bad = FALSE;
		
		if (!re->event) {
			bad = TRUE;	// no event??
		}
		else if (EVT_FLAGGED(re->event, EVTF_IN_DEVELOPMENT)) {
			bad = TRUE;	// event is in-dev
		}
		
		// oops:
		if (bad) {
			log("verify_running_events: Canceling event %d (%s)", re->id, re->event ? EVT_NAME(re->event) : "invalid event");
			cancel_running_event(re);
		}
		else if (re->status == EVTS_RUNNING) {
			// schedule its next event
			schedule_event_event(re);
		}
	}
}


/**
* Writes the running_events table to file. This is called whenever
* events_need_save is TRUE.
*/
void write_running_events_file(void) {
	struct event_leaderboard *lb, *next_lb;
	struct event_running_data *re;
	FILE *fl;
	
	if (!(fl = fopen(RUNNING_EVENTS_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to write %s", RUNNING_EVENTS_FILE TEMP_SUFFIX);
		return;
	}
	
	// header info
	fprintf(fl, "%d\n", top_event_id);
	
	LL_FOREACH(running_events, re) {
		if (!re->event) {
			continue;	// can't save if no event data
		}
		
		fprintf(fl, "#%d\n", re->id);
		fprintf(fl, "%d %ld %d\n", EVT_VNUM(re->event), re->start_time, re->status);
		fprintf(fl, "0 0 0 0\n");	// numeric line for future use
		
		// E: empire leaderboard
		/* not currently used
		HASH_ITER(hh, re->empire_leaderboard, lb, next_lb) {
			fprintf(fl, "E %d %d %d %d\n", lb->id, lb->points, lb->approved, lb->ignore);
		}
		*/
		
		// P: player leaderboard
		HASH_ITER(hh, re->player_leaderboard, lb, next_lb) {
			fprintf(fl, "P %d %d %d %d\n", lb->id, lb->points, lb->approved, lb->ignore);
		}
		
		fprintf(fl, "S\n");
	}
	
	// end
	fprintf(fl, "$\n");
	fclose(fl);
	
	rename(RUNNING_EVENTS_FILE TEMP_SUFFIX, RUNNING_EVENTS_FILE);
	
	// and prevent re-saving
	events_need_save = FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common event problems and reports them to ch.
*
* @param event_data *event The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_event(event_data *event, char_data *ch) {
	bool problem = FALSE;
	
	if (EVT_FLAGGED(event, EVTF_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, EVT_VNUM(event), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!EVT_NAME(event) || !*EVT_NAME(event) || !str_cmp(EVT_NAME(event), default_event_name)) {
		olc_audit_msg(ch, EVT_VNUM(event), "Name not set");
		problem = TRUE;
	}
	if (!isupper(*EVT_NAME(event))) {
		olc_audit_msg(ch, EVT_VNUM(event), "Name not capitalized");
		problem = TRUE;
	}
	if (!EVT_DESCRIPTION(event) || !*EVT_DESCRIPTION(event) || !str_cmp(EVT_DESCRIPTION(event), default_event_description)) {
		olc_audit_msg(ch, EVT_VNUM(event), "Description not set");
		problem = TRUE;
	}
	if (!EVT_COMPLETE_MSG(event) || !*EVT_COMPLETE_MSG(event) || !str_cmp(EVT_COMPLETE_MSG(event), default_event_complete_msg)) {
		olc_audit_msg(ch, EVT_VNUM(event), "Complete message not set");
		problem = TRUE;
	}
	
	if (EVT_MIN_LEVEL(event) > EVT_MAX_LEVEL(event) && EVT_MAX_LEVEL(event) != 0) {
		olc_audit_msg(ch, EVT_VNUM(event), "Min level higher than max level");
		problem = TRUE;
	}
	
	if (EVT_DURATION(event) < 1) {
		olc_audit_msg(ch, EVT_VNUM(event), "Invalid duration");
		problem = TRUE;
	}
	if (EVT_DURATION(event) < 30) {
		olc_audit_msg(ch, EVT_VNUM(event), "Duration is very low");
		problem = TRUE;
	}
	
	if (!EVT_RANK_REWARDS(event)) {
		olc_audit_msg(ch, EVT_VNUM(event), "No rank rewards set");
		problem = TRUE;
	}
	if (!EVT_THRESHOLD_REWARDS(event)) {
		olc_audit_msg(ch, EVT_VNUM(event), "No threshold rewards set");
		problem = TRUE;
	}
	
	return problem;
}


/**
* Deletes entries by type+vnum.
*
* @param struct event_reward **list A pointer to the list to delete from.
* @param int type QG_ type.
* @param any_vnum vnum The vnum to remove.
* @return bool TRUE if the type+vnum was removed from the list. FALSE if not.
*/
bool delete_event_reward_from_list(struct event_reward **list, int type, any_vnum vnum) {
	struct event_reward *iter, *next_iter;
	bool any = FALSE;
	
	LL_FOREACH_SAFE(*list, iter, next_iter) {
		if (iter->type == type && iter->vnum == vnum) {
			any = TRUE;
			LL_DELETE(*list, iter);
			free(iter);
		}
	}
	
	return any;
}


/**
* @param struct event_reward *list A list to search.
* @param int type REQ_ type.
* @param any_vnum vnum The vnum to look for.
* @return bool TRUE if the type+vnum is in the list. FALSE if not.
*/
bool find_event_reward_in_list(struct event_reward *list, int type, any_vnum vnum) {
	struct event_reward *iter;
	LL_FOREACH(list, iter) {
		if (iter->type == type && iter->vnum == vnum) {
			return TRUE;
		}
	}
	return FALSE;
}


/**
* For the .list command.
*
* @param event_data *event The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_event(event_data *event, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s", EVT_VNUM(event), EVT_NAME(event));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", EVT_VNUM(event), EVT_NAME(event));
	}
		
	return output;
}


/**
* Searches for all uses of an event and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The event vnum.
*/
void olc_search_event(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	event_data *event = find_event_by_vnum(vnum);
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	social_data *soc, *next_soc;
	event_data *ev, *next_ev;
	int size, found;
	bool any;
	
	if (!event) {
		msg_to_char(ch, "There is no event %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of event %d (%s):\r\n", vnum, EVT_NAME(event));
	
	// other events
	HASH_ITER(hh, event_table, ev, next_ev) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QR_x: event rewards
		any = find_event_reward_in_list(EVT_RANK_REWARDS(ev), QR_EVENT_POINTS, vnum);
		any |= find_event_reward_in_list(EVT_THRESHOLD_REWARDS(ev), QR_EVENT_POINTS, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "EVT [%5d] %s\r\n", EVT_VNUM(ev), EVT_NAME(ev));
		}
	}
	
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_EVENT_RUNNING, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_EVENT_NOT_RUNNING, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "PRG [%5d] %s\r\n", PRG_VNUM(prg), PRG_NAME(prg));
		}
	}
	
	// quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QR_x, REQ_x: quest types
		any = find_requirement_in_list(QUEST_TASKS(quest), REQ_EVENT_RUNNING, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_EVENT_RUNNING, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_EVENT_NOT_RUNNING, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_EVENT_NOT_RUNNING, vnum);
		
		any |= find_quest_reward_in_list(QUEST_REWARDS(quest), QR_EVENT_POINTS, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// on socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: quest types
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_EVENT_RUNNING, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_EVENT_NOT_RUNNING, vnum);
		
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


// Simple vnum sorter for the event hash
int sort_events(event_data *a, event_data *b) {
	return EVT_VNUM(a) - EVT_VNUM(b);
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* @param any_vnum vnum Any event vnum
* @return event_data* The event, or NULL if it doesn't exist
*/
event_data *find_event_by_vnum(any_vnum vnum) {
	event_data *event;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(event_table, &vnum, event);
	return event;
}


/**
* Puts an event into the hash table.
*
* @param event_data *event The event data to add to the table.
*/
void add_event_to_table(event_data *event) {
	event_data *find;
	any_vnum vnum;
	
	if (event) {
		vnum = EVT_VNUM(event);
		HASH_FIND_INT(event_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(event_table, vnum, event);
			HASH_SORT(event_table, sort_events);
		}
	}
}


/**
* Removes an event from the hash table.
*
* @param event_data *event The event data to remove from the table.
*/
void remove_event_from_table(event_data *event) {
	HASH_DEL(event_table, event);
}


/**
* Initializes a new event. This clears all memory for it, so set the vnum
* AFTER.
*
* @param event_data *event The event to initialize.
*/
void clear_event(event_data *event) {
	memset((char *) event, 0, sizeof(event_data));
	
	EVT_VNUM(event) = NOTHING;
	EVT_REPEATS_AFTER(event) = NOT_REPEATABLE;
	EVT_DURATION(event) = 1;
}


/**
* @param struct event_reward *from The list to copy.
* @return struct event_reward* The copy of the list.
*/
struct event_reward *copy_event_rewards(struct event_reward *from) {
	struct event_reward *el, *iter, *list = NULL, *end = NULL;
	
	LL_FOREACH(from, iter) {
		CREATE(el, struct event_reward, 1);
		*el = *iter;
		el->next = NULL;
		
		if (end) {
			end->next = el;
		}
		else {
			list = el;
		}
		end = el;
	}
	
	return list;
}


/**
* @param struct event_reward *list The list to free.
*/
void free_event_rewards(struct event_reward *list) {
	struct event_reward *iter, *next_iter;
	LL_FOREACH_SAFE(list, iter, next_iter) {
		free(iter);
	}
}


/**
* frees up memory for an event data.
*
* See also: olc_delete_event
*
* @param event_data *event The event data to free.
*/
void free_event(event_data *event) {
	event_data *proto = find_event_by_vnum(EVT_VNUM(event));
	
	// strings
	if (EVT_NAME(event) && (!proto || EVT_NAME(event) != EVT_NAME(proto))) {
		free(EVT_NAME(event));
	}
	if (EVT_DESCRIPTION(event) && (!proto || EVT_DESCRIPTION(event) != EVT_DESCRIPTION(proto))) {
		free(EVT_DESCRIPTION(event));
	}
	if (EVT_COMPLETE_MSG(event) && (!proto || EVT_COMPLETE_MSG(event) != EVT_COMPLETE_MSG(proto))) {
		free(EVT_COMPLETE_MSG(event));
	}
	if (EVT_NOTES(event) && (!proto || EVT_NOTES(event) != EVT_NOTES(proto))) {
		free(EVT_NOTES(event));
	}
	
	// pointers
	if (EVT_RANK_REWARDS(event) && (!proto || EVT_RANK_REWARDS(event) != EVT_RANK_REWARDS(proto))) {
		free_event_rewards(EVT_RANK_REWARDS(event));
	}
	if (EVT_THRESHOLD_REWARDS(event) && (!proto || EVT_THRESHOLD_REWARDS(event) != EVT_THRESHOLD_REWARDS(proto))) {
		free_event_rewards(EVT_THRESHOLD_REWARDS(event));
	}
	
	free(event);
}


/**
* Parses an event reward, saved as:
*
* R min max type vnum amount
* T min max type vnum amount
*
* @param char *line The reward line as read from the file.
* @param struct event_reward **list The list to append to.
* @param char *error_str How to report if there is an error.
*/
void parse_event_reward(char *line, struct event_reward **list, char *error_str) {
	struct event_reward *reward;
	int min, max, type, amount;
	any_vnum vnum;
	char key;
	
	if (!line || !*line || !list) {
		log("SYSERR: data error in event reward line of %s: %s", error_str ? error_str : "UNKNOWN", NULLSAFE(line));
		exit(1);
	}
	if (sscanf(line, "%c %d %d %d %d %d", &key, &min, &max, &type, &vnum, &amount) != 6) {
		log("SYSERR: format error in event reward line of %s", error_str ? error_str : "UNKNOWN");
		exit(1);
	}
	
	CREATE(reward, struct event_reward, 1);
	reward->min = min;
	reward->max = max;
	reward->type = type;
	reward->vnum = vnum;
	reward->amount = amount;
	
	LL_APPEND(*list, reward);
}


/**
* Read one event from file.
*
* @param FILE *fl The open .evt file
* @param any_vnum vnum The event vnum
*/
void parse_event(FILE *fl, any_vnum vnum) {
	void parse_requirement(FILE *fl, struct req_data **list, char *error_str);
	
	char line[256], error[256], str_in[256];
	event_data *event, *find;
	int int_in[6];
	
	CREATE(event, event_data, 1);
	clear_event(event);
	EVT_VNUM(event) = vnum;
	
	HASH_FIND_INT(event_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate event vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_event_to_table(event);
		
	// for error messages
	sprintf(error, "event vnum %d", vnum);
	
	// lines 1-4: strings
	EVT_NAME(event) = fread_string(fl, error);
	EVT_DESCRIPTION(event) = fread_string(fl, error);
	EVT_COMPLETE_MSG(event) = fread_string(fl, error);
	EVT_NOTES(event) = fread_string(fl, error);
	
	// 5. version type flags min max duration repeats
	if (!get_line(fl, line) || sscanf(line, "%d %d %s %d %d %d %d", &int_in[0], &int_in[1], str_in, &int_in[2], &int_in[3], &int_in[4], &int_in[5]) != 7) {
		log("SYSERR: Format error in line 4 of %s", error);
		exit(1);
	}
	
	EVT_VERSION(event) = int_in[0];
	EVT_TYPE(event) = int_in[1];
	EVT_FLAGS(event) = asciiflag_conv(str_in);
	EVT_MIN_LEVEL(event) = int_in[2];
	EVT_MAX_LEVEL(event) = int_in[3];
	EVT_DURATION(event) = int_in[4];
	EVT_REPEATS_AFTER(event) = int_in[5];
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'R': {	// rank rewards
				parse_event_reward(line, &EVT_RANK_REWARDS(event), error);
				break;
			}
			case 'T': {	// threshold rewards
				parse_event_reward(line, &EVT_THRESHOLD_REWARDS(event), error);
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


/**
* Handler for the .rankrewards and .thresholdrewards editors. The only
* difference between the two is that rank requires a min-max (rank range) while
* threshold only has a min (number of points).
*
* @param char_data *ch The person editing.
* @param char *argument Any args.
* @param struct event_reward **list Pointer to which list of rewards to work with.
* @param char *cmd Which command is shown in strings (what they typed to use this editor).
* @bool rank If TRUE, requires min-max rank. FALSE is threshold mode and only needs min points.
*/
void process_evedit_rewards(char_data *ch, char *argument, struct event_reward **list, char *cmd, bool rank) {
	extern any_vnum parse_quest_reward_vnum(char_data *ch, int type, char *vnum_arg, char *prev_arg);
	
	char cmd_arg[MAX_INPUT_LENGTH], field_arg[MAX_INPUT_LENGTH];
	char min_arg[MAX_INPUT_LENGTH], max_arg[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH];
	char vnum_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	struct event_reward *reward, *iter, *change, *copyfrom;
	int findtype, num = 1, stype, temp, min, max = 0;
	any_vnum vnum;
	bool found;
	char *pos;
	
	argument = any_one_arg(argument, cmd_arg);	// add/remove/change/copy
	
	if (is_abbrev(cmd_arg, "copy")) {
		// usage: rewards copy <from type> <from vnum>
		argument = any_one_arg(argument, type_arg);	// just "event" for now
		argument = any_one_arg(argument, vnum_arg);	// any vnum for that type
		
		if (!*type_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: %s copy <from type> <from vnum>\r\n", cmd);
		}
		else if ((findtype = find_olc_type(type_arg)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", type_arg);
		}
		else if (!isdigit(*vnum_arg)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy from which %s?\r\n", buf);
		}
		else if ((vnum = atoi(vnum_arg)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			copyfrom = NULL;
			
			switch (findtype) {
				case OLC_EVENT: {
					event_data *from_event = find_event_by_vnum(vnum);
					if (from_event) {
						copyfrom = rank ? EVT_RANK_REWARDS(from_event) : EVT_THRESHOLD_REWARDS(from_event);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy rewards from %ss.\r\n", buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, vnum_arg);
			}
			else {
				smart_copy_event_rewards(list, copyfrom);
				msg_to_char(ch, "Copied rewards from %s %d.\r\n", buf, vnum);
			}
		}
	}	// end 'copy'
	else if (is_abbrev(cmd_arg, "remove")) {
		// usage: rewards remove <number | all>
		skip_spaces(&argument);	// only arg is number
		
		if (!*argument) {
			msg_to_char(ch, "Remove which %s (number)?\r\n", cmd);
		}
		else if (!str_cmp(argument, "all")) {
			free_event_rewards(*list);
			*list = NULL;
			msg_to_char(ch, "You remove all the %s.\r\n", cmd);
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid reward number.\r\n");
		}
		else {
			found = FALSE;
			LL_FOREACH(*list, iter) {
				if (--num == 0) {
					found = TRUE;
					
					if (iter->vnum > 0) {
						msg_to_char(ch, "You remove the %s for %dx %s %d.\r\n", cmd, iter->amount, quest_reward_types[iter->type], iter->vnum);
					}
					else {
						msg_to_char(ch, "You remove the %s for %dx %s.\r\n", cmd, iter->amount, quest_reward_types[iter->type]);
					}
					LL_DELETE(*list, iter);
					free(iter);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid %s number.\r\n", cmd);
			}
		}
	}	// end 'remove'
	else if (is_abbrev(cmd_arg, "add")) {
		// usage: rewards add <min> [max] <type> <amount> <vnum/type>
		argument = any_one_arg(argument, min_arg);
		if (rank) {
			// check for optional "min-max" with a dash
			if ((pos = strchr(min_arg, '-'))) {
				*pos = '\0';
				strcpy(max_arg, pos+1);
			}
			else {
				argument = any_one_arg(argument, max_arg);
			}
		}
		argument = any_one_arg(argument, type_arg);
		argument = any_one_arg(argument, num_arg);
		argument = any_one_word(argument, vnum_arg);
		
		if (!*min_arg || (rank && !*max_arg) || !*type_arg || !*num_arg || !isdigit(*num_arg)) {
			msg_to_char(ch, "Usage: %s add %s <type> <amount> <vnum/type>\r\n", cmd, rank ? "<min rank> <max rank>" : "<min threshold>");
		}
		else if (!isdigit(*min_arg) || (min = atoi(min_arg)) < 1) {
			msg_to_char(ch, "Minimum %s must be a number greater than 0.\r\n", rank ? "rank" : "threshold");
		}
		else if (rank && (!isdigit(*max_arg) || (max = atoi(max_arg)) < 1)) {
			msg_to_char(ch, "Maximum %s must be a number greater than 0.\r\n", rank ? "rank" : "threshold");
		}
		else if ((stype = search_block(type_arg, quest_reward_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", type_arg);
		}
		else if ((num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid amount '%s'.\r\n", num_arg);
		}
		else if ((vnum = parse_quest_reward_vnum(ch, stype, vnum_arg, num_arg)) == NOTHING) {
			// this should have sent its own message
		}
		else {	// ok!
			// slight sanity: ensure max > min
			if (min > max && max != 0) {
				temp = min;
				min = max;
				max = temp;
			}
			
			// success
			CREATE(reward, struct event_reward, 1);
			reward->min = min;
			reward->max = max;
			reward->type = stype;
			reward->amount = num;
			reward->vnum = vnum;
			
			LL_PREPEND(*list, reward);
			LL_SORT(*list, sort_event_rewards);
			
			if (rank) {
				sprintf(buf, "ranks %d-%d", min, max);
			}
			else {
				sprintf(buf, "%d points", min);
			}
			
			msg_to_char(ch, "You add %s %s reward for %s: %s\r\n", AN(quest_reward_types[stype]), quest_reward_types[stype], buf, event_reward_string(reward, TRUE));
		}
	}	// end 'add'
	else if (is_abbrev(cmd_arg, "change")) {
		// usage: rewards change <number> <min | max | amount | vnum> <value>
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		argument = any_one_word(argument, vnum_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*field_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: %s change <number> <min | max | amount | vnum> <value>\r\n", cmd);
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		LL_FOREACH(*list, iter) {
			if (--num == 0) {
				change = iter;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid reward number.\r\n");
		}
		else if (is_abbrev(field_arg, "minimum")) {
			if (!isdigit(*vnum_arg) || (min = atoi(vnum_arg)) < 1) {
				msg_to_char(ch, "Invalid minimum '%s'.\r\n", vnum_arg);
				return;
			}
			else {
				change->min = min;
				LL_SORT(*list, sort_event_rewards);
				msg_to_char(ch, "You change the minimum %s to: %d\r\n", rank ? "rank" : "threshold", min);
			}
		}
		else if (is_abbrev(field_arg, "maximum")) {
			if (!rank) {
				msg_to_char(ch, "You can't set the maximum on this type of reward.\r\n");
				return;
			}
			else if (!isdigit(*vnum_arg) || (max = atoi(vnum_arg)) < 1) {
				msg_to_char(ch, "Invalid maximum '%s'.\r\n", vnum_arg);
				return;
			}
			else {
				change->max = max;
				LL_SORT(*list, sort_event_rewards);
				msg_to_char(ch, "You change the maximum %s to: %d\r\n", rank ? "rank" : "threshold", max);
			}
		}
		else if (is_abbrev(field_arg, "amount") || is_abbrev(field_arg, "quantity")) {
			if (!isdigit(*vnum_arg) || (num = atoi(vnum_arg)) < 0) {
				msg_to_char(ch, "Invalid amount '%s'.\r\n", vnum_arg);
				return;
			}
			else {
				change->amount = num;
				msg_to_char(ch, "You change reward %d to: %s\r\n", atoi(num_arg), event_reward_string(change, TRUE));
			}
		}
		else if (is_abbrev(field_arg, "vnum")) {
			if ((vnum = parse_quest_reward_vnum(ch, change->type, vnum_arg, NULL)) == NOTHING) {
				// sends own error
			}
			else {
				change->vnum = vnum;
				msg_to_char(ch, "Changed reward %d to: %s\r\n", atoi(num_arg), event_reward_string(change, TRUE));
			}
		}
		else {
			msg_to_char(ch, "You can only change the amount or vnum.\r\n");
		}
	}	// end 'change'
	else if (is_abbrev(cmd_arg, "move")) {
		struct event_reward *to_move, *prev, *a, *b, *a_next, *b_next, iitem;
		bool up;
		
		// usage: rewards move <number> <up | down>
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		up = is_abbrev(field_arg, "up");
		
		if (!*num_arg || !*field_arg) {
			msg_to_char(ch, "Usage: %s move <number> <up | down>\r\n", cmd);
		}
		else if (!isdigit(*num_arg) || (num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid reward number.\r\n");
		}
		else if (!is_abbrev(field_arg, "up") && !is_abbrev(field_arg, "down")) {
			msg_to_char(ch, "You must specify whether you're moving it up or down in the list.\r\n");
		}
		else if (up && num == 1) {
			msg_to_char(ch, "You can't move it up; it's already at the top of the list.\r\n");
		}
		else {
			// find the one to move
			to_move = prev = NULL;
			for (reward = *list; reward && !to_move; reward = reward->next) {
				if (--num == 0) {
					to_move = reward;
				}
				else {
					// store for next iteration
					prev = reward;
				}
			}
			
			if (!to_move) {
				msg_to_char(ch, "Invalid reward number.\r\n");
			}
			else if (!up && !to_move->next) {
				msg_to_char(ch, "You can't move it down; it's already at the bottom of the list.\r\n");
			}
			else {
				// SUCCESS: "move" them by swapping data
				if (up) {
					a = prev;
					b = to_move;
				}
				else {
					a = to_move;
					b = to_move->next;
				}
				
				// store next pointers
				a_next = a->next;
				b_next = b->next;
				
				// swap data
				iitem = *a;
				*a = *b;
				*b = iitem;
				
				// restore next pointers
				a->next = a_next;
				b->next = b_next;
				
				// message: re-atoi(num_arg) because we destroyed num finding our target
				LL_SORT(*list, sort_event_rewards);
				msg_to_char(ch, "You move reward %d %s.\r\n", atoi(num_arg), (up ? "up" : "down"));
			}
		}
	}	// end 'move'
	else {
		msg_to_char(ch, "Usage: %s add %s <type> <amount> <vnum/type>\r\n", cmd, rank ? "<min rank> <max rank>" : "<min threshold>");
		msg_to_char(ch, "Usage: %s change <number> <min | max | vnum | amount> <value>\r\n", cmd);
		msg_to_char(ch, "Usage: %s copy <from type> <from vnum>\r\n", cmd);
		msg_to_char(ch, "Usage: %s remove <number | all>\r\n", cmd);
		msg_to_char(ch, "Usage: %s move <number> <up | down>\r\n", cmd);
	}
}


// simple sorter for event rewards
int sort_event_rewards(struct event_reward *a, struct event_reward *b) {
	if (a->max != b->max) {
		return a->max - b->max;
	}
	else {
		return a->min - b->min;
	}
}


// writes entries in the event index
void write_event_index(FILE *fl) {
	event_data *event, *next_event;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, event_table, event, next_event) {
		// determine "zone number" by vnum
		this = (int)(EVT_VNUM(event) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, EVT_SUFFIX);
			last = this;
		}
	}
}


/**
* Writes a list of 'event_reward' to a data file.
*
* @param FILE *fl The file, open for writing.
* @param char letter The tag letter.
* @param struct event_reward *list The list to write.
*/
void write_event_rewards_to_file(FILE *fl, char letter, struct event_reward *list) {
	struct event_reward *iter;
	LL_FOREACH(list, iter) {
		fprintf(fl, "%c %d %d %d %d %d\n", letter, iter->min, iter->max, iter->type, iter->vnum, iter->amount);
	}
}


/**
* Outputs one event item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param event_data *event The thing to save.
*/
void write_event_to_file(FILE *fl, event_data *event) {
	char temp[MAX_STRING_LENGTH];
	
	if (!fl || !event) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_event_to_file called without %s", !fl ? "file" : "event");
		return;
	}
	
	fprintf(fl, "#%d\n", EVT_VNUM(event));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(EVT_NAME(event)));
	
	// 2. desc
	strcpy(temp, NULLSAFE(EVT_DESCRIPTION(event)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// 3. complete msg
	strcpy(temp, NULLSAFE(EVT_COMPLETE_MSG(event)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// 4. notes
	strcpy(temp, NULLSAFE(EVT_NOTES(event)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// 5. version type flags min max duration repeats
	fprintf(fl, "%d %d %s %d %d %d %d\n", EVT_VERSION(event), EVT_TYPE(event), bitv_to_alpha(EVT_FLAGS(event)), EVT_MIN_LEVEL(event), EVT_MAX_LEVEL(event), EVT_DURATION(event), EVT_REPEATS_AFTER(event));
	
	// R. rank rewards
	write_event_rewards_to_file(fl, 'R', EVT_RANK_REWARDS(event));
	
	// R. rank thresholds
	write_event_rewards_to_file(fl, 'T', EVT_THRESHOLD_REWARDS(event));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////


/**
* Creates a new event entry.
* 
* @param any_vnum vnum The number to create.
* @return event_data* The new event's prototype.
*/
event_data *create_event_table_entry(any_vnum vnum) {
	event_data *event;
	
	// sanity
	if (find_event_by_vnum(vnum)) {
		log("SYSERR: Attempting to insert event at existing vnum %d", vnum);
		return find_event_by_vnum(vnum);
	}
	
	CREATE(event, event_data, 1);
	clear_event(event);
	EVT_VNUM(event) = vnum;
	EVT_NAME(event) = str_dup(default_event_name);
	EVT_DESCRIPTION(event) = str_dup(default_event_description);
	EVT_COMPLETE_MSG(event) = str_dup(default_event_complete_msg);
	EVT_FLAGS(event) = EVTF_IN_DEVELOPMENT;
	add_event_to_table(event);

	// save index and event file now
	save_index(DB_BOOT_EVT);
	save_library_file_for_vnum(DB_BOOT_EVT, vnum);

	return event;
}


/**
* WARNING: This function actually deletes an event.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_event(char_data *ch, any_vnum vnum) {
	struct event_running_data *running, *next_running;
	struct player_event_data *ped, *next_ped;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	social_data *soc, *next_soc;
	event_data *ev, *next_ev;
	descriptor_data *desc;
	char_data *chiter;
	event_data *event;
	bool found;
	
	if (!(event = find_event_by_vnum(vnum))) {
		msg_to_char(ch, "There is no such event %d.\r\n", vnum);
		return;
	}
	
	// end the event, if running -- BEFORE removing from the hash table
	while ((running = find_running_event_by_vnum(vnum))) {
		cancel_running_event(running);
	}
	
	// remove it from the hash table now
	remove_event_from_table(event);
	
	// look for people with event data and remove it
	LL_FOREACH(character_list, chiter) {
		if (IS_NPC(chiter)) {
			continue;
		}
		
		HASH_ITER(hh, GET_EVENT_DATA(chiter), ped, next_ped) {
			if (!ped->event || EVT_VNUM(ped->event) == vnum) {
				HASH_DEL(GET_EVENT_DATA(chiter), ped);
				free(ped);
			}
		}
	}
	
	// delete all old running-events with this vnum
	LL_FOREACH_SAFE(running_events, running, next_running) {
		if (!running->event || EVT_VNUM(running->event) == vnum) {
			LL_DELETE(running_events, running);
			// free_event_leaderboard(running->empire_leaderboard);
			free_event_leaderboard(running->player_leaderboard);
			free(running);
		}
	}
	
	// save index and event file now
	save_index(DB_BOOT_EVT);
	save_library_file_for_vnum(DB_BOOT_EVT, vnum);
	
	// update other events
	HASH_ITER(hh, event_table, ev, next_ev) {
		// QR_x: event reward types
		found = delete_event_reward_from_list(&EVT_RANK_REWARDS(ev), QR_EVENT_POINTS, vnum);
		found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(ev), QR_EVENT_POINTS, vnum);
		
		if (found) {
			// SET_BIT(EVT_FLAGS(ev), EVTF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_EVT, EVT_VNUM(ev));
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		// REQ_x:
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_EVENT_RUNNING, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_EVENT_NOT_RUNNING, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// update quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		// REQ_x, QR_x: quest types
		found = delete_requirement_from_list(&QUEST_TASKS(quest), REQ_EVENT_RUNNING, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_EVENT_RUNNING, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_EVENT_NOT_RUNNING, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_EVENT_NOT_RUNNING, vnum);
		
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(quest), QR_EVENT_POINTS, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		// REQ_x: quest types
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_EVENT_RUNNING, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_EVENT_NOT_RUNNING, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// remove from from active editors
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (GET_OLC_EVENT(desc)) {
			// QR_x: event reward types
			found = delete_event_reward_from_list(&EVT_RANK_REWARDS(GET_OLC_EVENT(desc)), QR_EVENT_POINTS, vnum);
			found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(GET_OLC_EVENT(desc)), QR_EVENT_POINTS, vnum);
		
			if (found) {
				// SET_BIT(EVT_FLAGS(GET_OLC_EVENT(desc)), EVTF_IN_DEVELOPMENT);
				msg_to_desc(desc, "An event used as a reward by the event you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_PROGRESS(desc)) {
			// REQ_x:
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_EVENT_RUNNING, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_EVENT_NOT_RUNNING, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "An event used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			// REQ_x, QR_x: quest types
			found = delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_EVENT_RUNNING, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_EVENT_NOT_RUNNING, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_EVENT_RUNNING, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_EVENT_NOT_RUNNING, vnum);
			
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_EVENT_POINTS, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "An event used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			// REQ_x: quest types
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_EVENT_RUNNING, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_EVENT_NOT_RUNNING, vnum);
			
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "An event required by the social you are editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted event %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Event %d deleted.\r\n", vnum);
	
	free_event(event);
}


/**
* Function to save a player's changes to an event (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_event(descriptor_data *desc) {
	struct event_running_data *running;
	event_data *proto, *event = GET_OLC_EVENT(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	bool cancel = FALSE;
	UT_hash_handle hh;
	
	// have a place to save it?
	if (!(proto = find_event_by_vnum(vnum))) {
		proto = create_event_table_entry(vnum);
	}
	
	// event needs canceling?
	if (EVT_FLAGGED(event, EVTF_IN_DEVELOPMENT) && !EVT_FLAGGED(proto, EVTF_IN_DEVELOPMENT)) {
		cancel = TRUE;
	}
	
	// free prototype strings and pointers
	if (EVT_NAME(proto)) {
		free(EVT_NAME(proto));
	}
	if (EVT_DESCRIPTION(proto)) {
		free(EVT_DESCRIPTION(proto));
	}
	if (EVT_COMPLETE_MSG(proto)) {
		free(EVT_COMPLETE_MSG(proto));
	}
	if (EVT_NOTES(proto)) {
		free(EVT_NOTES(proto));
	}
	free_event_rewards(EVT_RANK_REWARDS(proto));
	free_event_rewards(EVT_THRESHOLD_REWARDS(proto));
	
	// sanity
	if (!EVT_NAME(event) || !*EVT_NAME(event)) {
		if (EVT_NAME(event)) {
			free(EVT_NAME(event));
		}
		EVT_NAME(event) = str_dup(default_event_name);
	}
	if (!EVT_DESCRIPTION(event) || !*EVT_DESCRIPTION(event)) {
		if (EVT_DESCRIPTION(event)) {
			free(EVT_DESCRIPTION(event));
		}
		EVT_DESCRIPTION(event) = str_dup(default_event_description);
	}
	if (!EVT_COMPLETE_MSG(event) || !*EVT_COMPLETE_MSG(event)) {
		if (EVT_COMPLETE_MSG(event)) {
			free(EVT_COMPLETE_MSG(event));
		}
		EVT_COMPLETE_MSG(event) = str_dup(default_event_complete_msg);
	}
	if (EVT_NOTES(event) && !*EVT_NOTES(event)) {
		free(EVT_NOTES(event));
		EVT_NOTES(event) = NULL;
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *event;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_EVT, vnum);
	
	// update any running events
	if ((running = find_running_event_by_vnum(vnum))) {
		if (cancel) {
			cancel_running_event(running);
		}
		else {	// not canceling but still may have changed: this will cancel any current end-schedule and set a new one
			schedule_event_event(running);
		}
	}
}


/**
* Creates a copy of a event, or clears a new one, for editing.
* 
* @param event_data *input The event to copy, or NULL to make a new one.
* @return event_data* The copied event.
*/
event_data *setup_olc_event(event_data *input) {
	extern struct apply_data *copy_apply_list(struct apply_data *input);
	
	event_data *new;
	
	CREATE(new, event_data, 1);
	clear_event(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		EVT_NAME(new) = EVT_NAME(input) ? str_dup(EVT_NAME(input)) : NULL;
		EVT_DESCRIPTION(new) = EVT_DESCRIPTION(input) ? str_dup(EVT_DESCRIPTION(input)) : NULL;
		EVT_COMPLETE_MSG(new) = EVT_COMPLETE_MSG(input) ? str_dup(EVT_COMPLETE_MSG(input)) : NULL;
		EVT_NOTES(new) = EVT_NOTES(input) ? str_dup(EVT_NOTES(input)) : NULL;
		
		EVT_RANK_REWARDS(new) = copy_event_rewards(EVT_RANK_REWARDS(input));
		EVT_THRESHOLD_REWARDS(new) = copy_event_rewards(EVT_THRESHOLD_REWARDS(input));
		
		// update version number
		EVT_VERSION(new) += 1;
	}
	else {
		// brand new: some defaults
		EVT_NAME(new) = str_dup(default_event_name);
		EVT_DESCRIPTION(new) = str_dup(default_event_description);
		EVT_COMPLETE_MSG(new) = str_dup(default_event_complete_msg);
		EVT_FLAGS(new) = EVTF_IN_DEVELOPMENT;
		EVT_VERSION(new) = 1;
	}
	
	// done
	return new;	
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* Gets the display for a set of event rewards.
*
* @param struct event_reward *list Pointer to the start of a list of event rewards.
* @param char *save_buffer A buffer to store the result to.
*/
void get_event_reward_display(struct event_reward *list, char *save_buffer) {
	char buf[MAX_STRING_LENGTH];
	struct event_reward *reward;
	int count = 0;
	
	*save_buffer = '\0';
	LL_FOREACH(list, reward) {
		if (reward->max > 0) {
			sprintf(buf, "Ranks %d-%d", reward->min, reward->max);
		}
		else {
			sprintf(buf, "%d points", reward->min);
		}
		sprintf(save_buffer + strlen(save_buffer), "%2d. %s: %s: %s\r\n", ++count, buf, quest_reward_types[reward->type], event_reward_string(reward, TRUE));
	}
	
	// empty list not shown
}


/**
* For vstat.
*
* @param char_data *ch The player requesting stats.
* @param event_data *event The event to display.
*/
void do_stat_event(char_data *ch, event_data *event) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	size_t size;
	
	if (!event) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \tc%s\t0\r\n", EVT_VNUM(event), EVT_NAME(event));
	size += snprintf(buf + size, sizeof(buf) - size, "%s", EVT_DESCRIPTION(event));
	size += snprintf(buf + size, sizeof(buf) - size, "-------------------------------------------------\r\n");
	size += snprintf(buf + size, sizeof(buf) - size, "%s", EVT_COMPLETE_MSG(event));
	
	if (EVT_NOTES(event)) {
		size += snprintf(buf + size, sizeof(buf) - size, "- Notes -----------------------------------------\r\n");
		size += snprintf(buf + size, sizeof(buf) - size, "%s", EVT_NOTES(event));
	}
	
	sprintbit(EVT_FLAGS(event), event_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);	
	
	if (EVT_REPEATS_AFTER(event) == NOT_REPEATABLE) {
		strcpy(part, "never");
	}
	else if (EVT_REPEATS_AFTER(event) == 0) {
		strcpy(part, "never");	// 0 is not a valid repeat time, unlike quests
	}
	else {
		sprintf(part, "%d minutes (%d:%02d:%02d)", EVT_REPEATS_AFTER(event), (EVT_REPEATS_AFTER(event) / (60 * 24)), ((EVT_REPEATS_AFTER(event) % (60 * 24)) / 60), ((EVT_REPEATS_AFTER(event) % (60 * 24)) % 60));
	}
	size += snprintf(buf + size, sizeof(buf) - size, "Level limits: [\tc%s\t0], Duration: [\tc%d minutes (%d:%02d:%02d)\t0], Repeatable: [\tc%s\t0]\r\n", level_range_string(EVT_MIN_LEVEL(event), EVT_MAX_LEVEL(event), 0), EVT_DURATION(event), (EVT_DURATION(event) / (60 * 24)), ((EVT_DURATION(event) % (60 * 24)) / 60), ((EVT_DURATION(event) % (60 * 24)) % 60), part);
	
	get_event_reward_display(EVT_RANK_REWARDS(event), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Rank Rewards:\r\n%s", *part ? part : " none\r\n");
	
	get_event_reward_display(EVT_THRESHOLD_REWARDS(event), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Threshold Rewards:\r\n%s", *part ? part : " none\r\n");
	
	page_string(ch->desc, buf, TRUE);
}


/**
* This is the main recipe display for event OLC. It displays the user's
* currently-edited event.
*
* @param char_data *ch The person who is editing an event and will see its display.
*/
void olc_show_event(char_data *ch) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	
	if (!event) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !find_event_by_vnum(EVT_VNUM(event)) ? "new event" : get_event_name_by_proto(EVT_VNUM(event)));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(EVT_NAME(event), default_event_name), NULLSAFE(EVT_NAME(event)));
	sprintf(buf + strlen(buf), "<%sdescription\t0>\r\n%s", OLC_LABEL_STR(EVT_DESCRIPTION(event), default_event_description), NULLSAFE(EVT_DESCRIPTION(event)));
	sprintf(buf + strlen(buf), "<%scompletemessage\t0>\r\n%s", OLC_LABEL_STR(EVT_COMPLETE_MSG(event), default_event_complete_msg), NULLSAFE(EVT_COMPLETE_MSG(event)));
	
	sprintbit(EVT_FLAGS(event), event_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT), lbuf);
	
	if (EVT_MIN_LEVEL(event) > 0) {
		sprintf(buf + strlen(buf), "<%sminlevel\t0> %d\r\n", OLC_LABEL_CHANGED, EVT_MIN_LEVEL(event));
	}
	else {
		sprintf(buf + strlen(buf), "<%sminlevel\t0> none\r\n", OLC_LABEL_UNCHANGED);
	}
	if (EVT_MAX_LEVEL(event) > 0) {
		sprintf(buf + strlen(buf), "<%smaxlevel\t0> %d\r\n", OLC_LABEL_CHANGED, EVT_MAX_LEVEL(event));
	}
	else {
		sprintf(buf + strlen(buf), "<%smaxlevel\t0> none\r\n", OLC_LABEL_UNCHANGED);
	}
	
	sprintf(buf + strlen(buf), "<%sduration\t0> %d minutes (%d:%02d:%02d)\r\n", OLC_LABEL_VAL(EVT_DURATION(event), 0), EVT_DURATION(event), (EVT_DURATION(event) / (60 * 24)), ((EVT_DURATION(event) % (60 * 24)) / 60), ((EVT_DURATION(event) % (60 * 24)) % 60));
	
	if (EVT_REPEATS_AFTER(event) == NOT_REPEATABLE || EVT_REPEATS_AFTER(event) == 0) {
		sprintf(buf + strlen(buf), "<%srepeat\t0> never\r\n", OLC_LABEL_VAL(EVT_REPEATS_AFTER(event), NOT_REPEATABLE));
	}
	else if (EVT_REPEATS_AFTER(event) > 0) {
		sprintf(buf + strlen(buf), "<%srepeat\t0> %d minutes (%d:%02d:%02d)\r\n", OLC_LABEL_VAL(EVT_REPEATS_AFTER(event), 0), EVT_REPEATS_AFTER(event), (EVT_REPEATS_AFTER(event) / (60 * 24)), ((EVT_REPEATS_AFTER(event) % (60 * 24)) / 60), ((EVT_REPEATS_AFTER(event) % (60 * 24)) % 60));
	}
	
	get_event_reward_display(EVT_RANK_REWARDS(event), lbuf);
	sprintf(buf + strlen(buf), "Rank rewards: <%srank\t0>\r\n%s", OLC_LABEL_PTR(EVT_RANK_REWARDS(event)), lbuf);
	
	get_event_reward_display(EVT_THRESHOLD_REWARDS(event), lbuf);
	sprintf(buf + strlen(buf), "Threshold rewards: <%sthreshold\t0>\r\n%s", OLC_LABEL_PTR(EVT_THRESHOLD_REWARDS(event)), lbuf);
	
	sprintf(buf + strlen(buf), "<%snotes\t0>\r\n%s", OLC_LABEL_PTR(EVT_NOTES(event)), NULLSAFE(EVT_NOTES(event)));
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Basic event detail page for players. This includes the player's score/rank
* if applicable.
*
* @param char_data *ch The person to show to (also includes some of their stats).
* @param event_data *event The event prototype.
*/
void show_event_detail(char_data *ch, event_data *event) {
	// bool full_access = (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EVENTS));
	struct event_running_data *running = find_last_event_by_vnum(EVT_VNUM(event));
	char vnum[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	struct player_event_data *ped = NULL;
	int diff, rank = 0;
	
	// load this for later
	if (running) {
		ped = get_event_data(ch, running->id);
		rank = get_event_rank(ch, running);
	}
	
	// vnum portion
	if (IS_IMMORTAL(ch)) {
		snprintf(vnum, sizeof(vnum), "[%d] ", EVT_VNUM(event));
	}
	else {
		*vnum = '\0';
	}
	
	// level portion
	if (EVT_MIN_LEVEL(event) == EVT_MAX_LEVEL(event) && EVT_MIN_LEVEL(event) == 0) {
		snprintf(part, sizeof(part), " (all levels)");
	}
	else {
		snprintf(part, sizeof(part), " (level %s)", level_range_string(EVT_MIN_LEVEL(event), EVT_MAX_LEVEL(event), 0));
	}
	
	// header
	msg_to_char(ch, "%s\tc%s\t0%s\r\n", vnum, EVT_NAME(event), part);
	
	if (EVT_FLAGGED(event, EVTF_IN_DEVELOPMENT)) {
		msg_to_char(ch, "\trThis event is in-development. You cannot play it or collect rewards.\t0\r\n");
	}
	
	// desc?
	if (!running || running->status == EVTS_RUNNING || running->status == EVTS_NOT_STARTED) {
		// show desc while running
		msg_to_char(ch, "%s", EVT_DESCRIPTION(event));
	}
	else if (ped && (rank > 0 || ped->points > 0) && (running->start_time + (EVT_DURATION(event) * SECS_PER_REAL_MIN) + SECS_PER_REAL_WEEK) > time(0)) {
		// show complete message -- if it's ended AND they participated
		msg_to_char(ch, "%s", EVT_COMPLETE_MSG(event));
	}
	else {
		// show desc in all other cases
		msg_to_char(ch, "%s", EVT_DESCRIPTION(event));
	}
	
	if (running) {
		switch (running->status) {
			case EVTS_RUNNING: {	// show time remaining
				msg_to_char(ch, "\tCStatus: Running (ends in %s)\t0\r\n", time_length_string(running->start_time + (EVT_DURATION(event) * SECS_PER_REAL_MIN) - time(0)));
				break;
			}
			case EVTS_COMPLETE: {	// show time since end
				diff = running->start_time + (EVT_DURATION(event) * SECS_PER_REAL_MIN) - time(0);
				if (diff < 0) {
					msg_to_char(ch, "\tcStatus: Ended (%s ago)\t0\r\n", time_length_string(diff));
				}
				else {	// end is still in the future?
					msg_to_char(ch, "\tcStatus: Ended recently\t0\r\n");
				}
				break;
			}
			// no other status shown
		}
		
		if (ped) {
			if (rank > 0) {
				msg_to_char(ch, "Rank: %d (%d point%s)\r\n", rank, ped->points, PLURAL(ped->points));
			}
			else {
				msg_to_char(ch, "Rank: unranked (%d point%s)\r\n", ped->points, PLURAL(ped->points));
			}
		}
		else {
			msg_to_char(ch, "Rank: unranked (0 points)\r\n");
		}
	}
}


/**
* Displays the current leaderboard for any event that still has 'running' data.
*
* @param char_data *ch The person who's looking at the data.
* @param struct event_running_data *re The event.
*/
void show_event_leaderboard(char_data *ch, struct event_running_data *re) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], part[256];
	struct event_leaderboard *lb, *next_lb;
	player_index_data *index;
	int rank = 0, total_rank = 0, last_points = -1;
	size_t size;
	
	bool need_approval = config_get_bool("event_approval");
	
	// basic sanitation
	if (!re || !re->event || !ch->desc) {
		msg_to_char(ch, "Unable to display leaderboard.\r\n");
		return;
	}
	
	size = snprintf(buf, sizeof(buf), "Leaderboard for %s:\r\n", EVT_NAME(re->event));
	HASH_ITER(hh, re->player_leaderboard, lb, next_lb) {
		if (lb->ignore || (need_approval && !lb->approved)) {
			strcpy(part, " *");
		}
		else {
			++total_rank;
			if (lb->points != last_points) {	// not a tie
				rank = total_rank;
			}
			// else: leave rank where it was (tie)
			last_points = lb->points;
			
			sprintf(part, "%2d", rank);
		}
		
		index = find_player_index_by_idnum(lb->id);
		snprintf(line, sizeof(line), "%s. %s (%d)\r\n", part, index ? index->fullname : "???", lb->points);
		
		if (size + strlen(line) < sizeof(buf)) {
			strcat(buf, line);
			size += strlen(line);
		}
		else {
			snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
			break;
		}
	}
	
	if (!re->player_leaderboard) {
		// always room left in 'buf' in this case
		size += snprintf(buf + size, sizeof(buf) - size, " no entries\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}



/**
* Displays the rewards for any event that still has 'running' data. This will
* show ones the player has already collected as such.
*
* @param char_data *ch The person who's looking at the data.
* @param struct event_running_data *re The event.
*/
void show_event_rewards(char_data *ch, struct event_running_data *re) {
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH];
	struct player_event_data *ped;
	struct event_reward *reward;
	bool collect, done;
	size_t size;
	
	// basic sanitation
	if (!re || !re->event || !ch->desc) {
		msg_to_char(ch, "Unable to view rewards.\r\n");
		return;
	}
	
	ped = get_event_data(ch, re->id);
	size = 0;
	*buf = '\0';
	
	// THRESHOLD
	size += snprintf(buf + size, sizeof(buf) - size, "Threshold rewards for %s:\r\n", EVT_NAME(re->event));
	LL_FOREACH(EVT_THRESHOLD_REWARDS(re->event), reward) {
		collect = (ped && ped->points >= reward->min);
		done = (ped && ped->collected_points >= reward->min);
		snprintf(line, sizeof(line), "%s%*d pt%s: %s%s\t0\r\n", done ? "\tc" : (collect ? "\tg" : ""), reward->min == 1 ? 4 : 3, reward->min, PLURAL(reward->min), event_reward_string(reward, IS_IMMORTAL(ch)), done ? " (collected)" : (collect ? " (pending)" : ""));
		
		if (size + strlen(line) < sizeof(buf)) {
			strcat(buf, line);
			size += strlen(line);
		}
		else {
			snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
			break;
		}
	}
	if (!EVT_THRESHOLD_REWARDS(re->event)) {
		size += snprintf(buf + size, sizeof(buf) - size, " no threshold rewards\r\n");
	}
	
	// RANK
	size += snprintf(buf + size, sizeof(buf) - size, "\r\nRank rewards for %s:\r\n", EVT_NAME(re->event));
	LL_FOREACH(EVT_RANK_REWARDS(re->event), reward) {
		collect = (ped && ped->rank >= reward->min && ped->rank <= reward->max);
		done = (ped && ped->status == EVTS_COLLECTED);
		snprintf(line, sizeof(line), "%s %d-%d: %s%s\t0\r\n", (collect && done) ? "\tc" : (collect ? "\tg" : ""), reward->min, reward->max, event_reward_string(reward, IS_IMMORTAL(ch)), (collect && done) ? " (collected)" : (collect ? " (pending)" : ""));
		
		if (size + strlen(line) < sizeof(buf)) {
			strcat(buf, line);
			size += strlen(line);
		}
		else {
			snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
			break;
		}
	}
	if (!EVT_RANK_REWARDS(re->event)) {
		size += snprintf(buf + size, sizeof(buf) - size, " no rank rewards\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* What to show if someone types 'events' with no argument. This shows:
* - a list of events, if more than 1 are running
* - the event detail if only 1 is running
* - a status message if none are running
*
* @param char_data *ch The person who typed it.
*/
void show_events_no_arg(char_data *ch) {
	struct event_running_data *running, *only;
	struct player_event_data *ped;
	char part[MAX_STRING_LENGTH];
	int count, rank, when;
	
	// fetch count and optional only-running-event
	only = only_one_running_event(&count);
	
	if (count == 0) {
		msg_to_char(ch, "No events are running right now (see HELP EVENTS for more options).\r\n");
	}
	else if (count == 1 && only) {
		show_event_detail(ch, only->event);
		msg_to_char(ch, "(See HELP EVENTS for more options)\r\n");
	}
	else {	// more than 1
		msg_to_char(ch, "Current events (see HELP EVENTS for more options):\r\n");
		LL_FOREACH(running_events, running) {
			if (!running->event) {
				continue;
			}
			if (running->status != EVTS_RUNNING) {
				continue;
			}
			
			// level portion
			if (EVT_MIN_LEVEL(running->event) == EVT_MAX_LEVEL(running->event) && EVT_MIN_LEVEL(running->event) == 0) {
				snprintf(part, sizeof(part), " (all levels)");
			}
			else {
				snprintf(part, sizeof(part), " (level %s)", level_range_string(EVT_MIN_LEVEL(running->event), EVT_MAX_LEVEL(running->event), 0));
			}
			
			if (IS_IMMORTAL(ch)) {	// imms see id prefix
				msg_to_char(ch, "%d.", running->id);
			}
			
			msg_to_char(ch, " %s%s", EVT_NAME(running->event), part);
			
			if ((ped = get_event_data(ch, running->id))) {
				if ((rank = get_event_rank(ch, running)) > 0) {
					sprintf(part, "rank %d", rank);
				}
				else {
					strcpy(part, "unranked");
				}
				msg_to_char(ch, " (%d point%s, %s)", ped->points, PLURAL(ped->points), part);
			}
			
			when = running->start_time + (EVT_DURATION(running->event) * SECS_PER_REAL_MIN) - time(0);
			if (when > 0) {
				msg_to_char(ch, ", ends in %s", time_length_string(when));
			}
			
			msg_to_char(ch, "\r\n");
		}
	}
}


/**
* Searches the event db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_event(char *searchname, char_data *ch) {
	event_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, event_table, iter, next_iter) {
		if (multi_isname(searchname, EVT_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, EVT_VNUM(iter), EVT_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(evedit_completemessage) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "completion message for %s", EVT_NAME(event));
		start_string_editor(ch->desc, buf, &EVT_COMPLETE_MSG(event), MAX_ITEM_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(evedit_description) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", EVT_NAME(event));
		start_string_editor(ch->desc, buf, &EVT_DESCRIPTION(event), MAX_ITEM_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(evedit_duration) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	
	if (!*argument) {
		msg_to_char(ch, "Set the duration to how many minutes?\r\n");
	}
	else if (isdigit(*argument)) {
		EVT_DURATION(event) = olc_process_number(ch, argument, "duration", "duration", 1, MAX_INT, EVT_DURATION(event));
		msg_to_char(ch, "It now runs for %d minutes (%d:%02d:%02d).\r\n", EVT_DURATION(event), (EVT_DURATION(event) / (60 * 24)), ((EVT_DURATION(event) % (60 * 24)) / 60), ((EVT_DURATION(event) % (60 * 24)) % 60));
	}
	else {
		msg_to_char(ch, "Invalid duration.\r\n");
	}
}


OLC_MODULE(evedit_flags) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	bool had_indev = IS_SET(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	EVT_FLAGS(event) = olc_process_flag(ch, argument, "event", "flags", event_flags, EVT_FLAGS(event));
	
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !IS_SET(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT);
	}
}


OLC_MODULE(evedit_name) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	olc_process_string(ch, argument, "name", &EVT_NAME(event));
}


OLC_MODULE(evedit_maxlevel) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	EVT_MAX_LEVEL(event) = olc_process_number(ch, argument, "maximum level", "maxlevel", 0, MAX_INT, EVT_MAX_LEVEL(event));
}


OLC_MODULE(evedit_minlevel) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	EVT_MIN_LEVEL(event) = olc_process_number(ch, argument, "minimum level", "minlevel", 0, MAX_INT, EVT_MIN_LEVEL(event));
}


OLC_MODULE(evedit_notes) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "notes for %s", EVT_NAME(event));
		start_string_editor(ch->desc, buf, &EVT_NOTES(event), MAX_ITEM_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(evedit_rankrewards) {
	process_evedit_rewards(ch, argument, &EVT_RANK_REWARDS(GET_OLC_EVENT(ch->desc)), "rankrewards", TRUE);
}


OLC_MODULE(evedit_repeat) {
	event_data *event = GET_OLC_EVENT(ch->desc);
	
	if (!*argument) {
		msg_to_char(ch, "Set the repeat interval to how many minutes (or never)?\r\n");
	}
	else if (is_abbrev(argument, "never") || is_abbrev(argument, "none")) {
		EVT_REPEATS_AFTER(event) = NOT_REPEATABLE;
		msg_to_char(ch, "It is now non-repeatable.\r\n");
	}
	else if (isdigit(*argument)) {
		EVT_REPEATS_AFTER(event) = olc_process_number(ch, argument, "repeats after", "repeat", 0, MAX_INT, EVT_REPEATS_AFTER(event));
		msg_to_char(ch, "It now repeats after %d minutes (%d:%02d:%02d).\r\n", EVT_REPEATS_AFTER(event), (EVT_REPEATS_AFTER(event) / (60 * 24)), ((EVT_REPEATS_AFTER(event) % (60 * 24)) / 60), ((EVT_REPEATS_AFTER(event) % (60 * 24)) % 60));
	}
	else {
		msg_to_char(ch, "Invalid repeat interval.\r\n");
	}
}


OLC_MODULE(evedit_thresholdrewards) {
	process_evedit_rewards(ch, argument, &EVT_THRESHOLD_REWARDS(GET_OLC_EVENT(ch->desc)), "thresholdrewards", FALSE);
}


 //////////////////////////////////////////////////////////////////////////////
//// EVENTS COMMAND //////////////////////////////////////////////////////////

EVENT_CMD(evcmd_cancel);
EVENT_CMD(evcmd_collect);
EVENT_CMD(evcmd_end);
EVENT_CMD(evcmd_leaderboard);
EVENT_CMD(evcmd_recent);
EVENT_CMD(evcmd_rewards);
EVENT_CMD(evcmd_start);

// subcommand struct for 'events'
const struct {
	char *command;
	EVENT_CMD(*func);
	int min_level;
	bitvector_t grant_flag;
} event_cmd[] = {
	{ "collect", evcmd_collect, 0, NO_GRANTS },
	{ "leaderboard", evcmd_leaderboard, 0, NO_GRANTS },
	{ "lb", evcmd_leaderboard, 0, NO_GRANTS },
	{ "recent", evcmd_recent, 0, NO_GRANTS },
	{ "rewards", evcmd_rewards, 0, NO_GRANTS },
	
	// immortal commands
	{ "cancel", evcmd_cancel, LVL_CIMPL, GRANT_EVENTS },
	{ "end", evcmd_end, LVL_CIMPL, GRANT_EVENTS },
	{ "start", evcmd_start, LVL_CIMPL, GRANT_EVENTS },
	
	// this goes last
	{ "\n", NULL, 0, NOBITS }
};


ACMD(do_events) {
	char *argptr, arg[MAX_INPUT_LENGTH];
	int iter, type = NOTHING;
	event_data *event;
	
	skip_spaces(&argument);
	argptr = any_one_arg(argument, arg);
	skip_spaces(&argptr);
	
	// find type?
	for (iter = 0; *event_cmd[iter].command != '\n' && type == NOTHING; ++iter) {
		if (GET_ACCESS_LEVEL(ch) < event_cmd[iter].min_level && (!event_cmd[iter].grant_flag || !IS_GRANTED(ch, event_cmd[iter].grant_flag))) {
			continue;	// can't do
		}
		
		// ok?
		if (is_abbrev(arg, event_cmd[iter].command)) {
			type = iter;
		}
	}
	
	// are they looking up an event instead?
	if (*arg && type == NOTHING && (event = smart_find_event(argument, FALSE))) {
		show_event_detail(ch, event);
		return;
	}
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't participate in events.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("event_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!*arg) {
		show_events_no_arg(ch);
	}
	else if (type == NOTHING || !event_cmd[type].func) {
		msg_to_char(ch, "Invalid event command. See HELP EVENTS for more info.\r\n");
	}
	else {
		// pass to child function
		(event_cmd[type].func)(ch, argptr);
	}
}


// cancels a current event (no rewards)
EVENT_CMD(evcmd_cancel) {
	struct event_running_data *re = NULL;
	event_data *event = NULL;
	
	if (!*argument) {
		msg_to_char(ch, "Cancel which event (id or name)?\r\n");
	}
	else if (is_number(argument) && !(re = find_running_event_by_id(atoi(argument)))) {
		msg_to_char(ch, "Unknown event id '%s'.\r\n", argument);
	}
	else if (!re && !(event = smart_find_event(argument, TRUE))) {
		msg_to_char(ch, "Unable to find a running event called '%s'.\r\n", argument);
	}
	else {
		// quick data check
		if (event && !re) {
			re = find_running_event_by_vnum(EVT_VNUM(event));
		}
		else if (re && !event) {
			event = re->event;
		}
		
		if (!re || !event) {
			msg_to_char(ch, "Unable to find event to cancel.\r\n");
			return;
		}
		
		// ok cancel it
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has canceled event: %d %s (id %d)", GET_NAME(ch), EVT_VNUM(event), EVT_NAME(event), re->id);
		send_config_msg(ch, "ok_string");
		cancel_running_event(re);
	}
}


// collects all pending rewards
EVENT_CMD(evcmd_collect) {
	void give_quest_rewards(char_data *ch, struct quest_reward *list, int reward_level, empire_data *quest_giver_emp, int instance_id);
	
	int level;
	struct player_event_data *ped, *next_ped;
	struct event_reward *reward;
	struct quest_reward qr;
	bool any, header;
	
	// qr is used for sharing reward handling with quests
	qr.next = NULL;
	
	// bad arg check
	if (*argument && !str_cmp(argument, "confirm")) {
		// player explicitly confirms (code continues)
	}
	else if (*argument) {
		msg_to_char(ch, "You can't specify what to collect. Just use 'event collect' to collect all your rewards.\r\n");
		return;
	}
	else if (GET_LOYALTY(ch) && ROOM_OWNER(IN_ROOM(ch)) != GET_LOYALTY(ch)) {
		// no arg and not in territory
		msg_to_char(ch, "You should only collect rewards in your own territory, in case they overfill your inventory.\r\n");
		msg_to_char(ch, "Type 'event collect confirm' to override this.\r\n");
		return;
	}
	
	any = FALSE;
	
	// check player's event data
	HASH_ITER(hh, GET_EVENT_DATA(ch), ped, next_ped) {
		if (!ped->event || EVT_FLAGGED(ped->event, EVTF_IN_DEVELOPMENT)) {
			continue;	// no work if no event or in-dev
		}
		if (ped->status == EVTS_COLLECTED) {
			continue;	// no work for fully-collected events
		}
		
		// determine reward level (if no level was recorded, use their current one
		level = ped->level ? ped->level : GET_HIGHEST_KNOWN_LEVEL(ch);
		if (EVT_MIN_LEVEL(ped->event) > 0) {
			level = MAX(level, EVT_MIN_LEVEL(ped->event));
		}
		if (EVT_MAX_LEVEL(ped->event) > 0) {
			level = MIN(level, EVT_MAX_LEVEL(ped->event));
		}
		
		// threshold rewards: thresholds are ALWAYS lowest-to-highest in the list
		// players can claim thresholds at any time, before the event ends
		header = FALSE;
		LL_FOREACH(EVT_THRESHOLD_REWARDS(ped->event), reward) {
			if (ped->points >= reward->min && ped->collected_points < reward->min) {
				// show header
				if (!header) {
					header = TRUE,
					msg_to_char(ch, "You collect threshold rewards for %s (%d point%s, level %d):\r\n", EVT_NAME(ped->event), ped->points, PLURAL(ped->points), level);
				}
				
				ped->collected_points = reward->min;	// update highest collection
				
				// copy helper data to use quest rewards
				qr.type = reward->type;
				qr.vnum = reward->vnum;
				qr.amount = reward->amount;
				
				give_quest_rewards(ch, &qr, level, GET_LOYALTY(ch), 0);
				any = TRUE;
			}
		}
		
		// rank rewards: look for events in the 'complete' state
		if (ped->status == EVTS_COMPLETE) {
			ped->status = EVTS_COLLECTED;	// update status now
			
			header = FALSE;
			LL_FOREACH(EVT_RANK_REWARDS(ped->event), reward) {
				if (ped->rank >= reward->min && ped->rank <= reward->max) {
					// show header
					if (!header) {
						header = TRUE,
						msg_to_char(ch, "You collect rank rewards for %s (rank %d, level %d):\r\n", EVT_NAME(ped->event), ped->rank, level);
					}
					
					// copy helper data to use quest rewards
					qr.type = reward->type;
					qr.vnum = reward->vnum;
					qr.amount = reward->amount;
				
					give_quest_rewards(ch, &qr, level, GET_LOYALTY(ch), 0);
					any = TRUE;
				}
			}
		}
	}
	
	if (any) {
		SAVE_CHAR(ch);
	}
	else {
		msg_to_char(ch, "You have no pending rewards to collect.\r\n");
	}
}


// ends a current event (with rewards)
EVENT_CMD(evcmd_end) {
	struct event_running_data *re = NULL;
	event_data *event = NULL;
	
	if (!*argument) {
		msg_to_char(ch, "End which event (id or name)?\r\n");
	}
	else if (is_number(argument) && !(re = find_running_event_by_id(atoi(argument)))) {
		msg_to_char(ch, "Unknown event id '%s'.\r\n", argument);
	}
	else if (!re && !(event = smart_find_event(argument, TRUE))) {
		msg_to_char(ch, "Unable to find a running event called '%s'.\r\n", argument);
	}
	else {
		// quick data check
		if (event && !re) {
			re = find_running_event_by_vnum(EVT_VNUM(event));
		}
		else if (re && !event) {
			event = re->event;
		}
		
		if (!re || !event) {
			msg_to_char(ch, "Unable to find event to end.\r\n");
			return;
		}
		
		// ok end it
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has ended event: %d %s (id %d)", GET_NAME(ch), EVT_VNUM(event), EVT_NAME(event), re->id);
		send_config_msg(ch, "ok_string");
		end_event(re);
	}
}


// displays a leaderboard for an event
EVENT_CMD(evcmd_leaderboard) {
	struct event_running_data *re, *only;
	event_data *event;
	
	// if only 1 event is running, show it if no-args
	only = only_one_running_event(NULL);
	
	if (!*argument && only) {
		show_event_leaderboard(ch, only);
		return;
	}
	else if (!*argument) {
		msg_to_char(ch, "Show leaderboard for which event?\r\n");
		return;
	}
	
	// targeting: imms may target by event id
	if (IS_IMMORTAL(ch) && isdigit(*argument) && (re = find_running_event_by_id(atoi(argument)))) {
		event = re->event;
		// this is a valid case; all other targeting below happens otherwise:
	}
	else if (!(event = smart_find_event(argument, FALSE))) {
		msg_to_char(ch, "Unknown event '%s'.\r\n", argument);
		return;
	}
	else if (!(re = find_last_event_by_vnum(EVT_VNUM(event)))) {
		msg_to_char(ch, "There is no leaderboard for that event.\r\n");
		return;
	}
	
	// success
	show_event_leaderboard(ch, re);
}


// shows recent events
EVENT_CMD(evcmd_recent) {
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	struct event_running_data *running;
	struct player_event_data *ped;
	int rank, when, count = 0;
	size_t size, lsize;
	char *ptr;
	
	size = snprintf(buf, sizeof(buf), "Recent events (see HELP EVENTS for more options):\r\n");
	LL_FOREACH(running_events, running) {
		if (!running->event) {
			continue;	// can't show one with no event
		}
		if (running->status != EVTS_COMPLETE) {
			continue;	// only show finished events
		}
		
		++count;	// any
		
		// level portion
		if (EVT_MIN_LEVEL(running->event) == EVT_MAX_LEVEL(running->event) && EVT_MIN_LEVEL(running->event) == 0) {
			snprintf(part, sizeof(part), " (all levels)");
		}
		else {
			snprintf(part, sizeof(part), " (level %s)", level_range_string(EVT_MIN_LEVEL(running->event), EVT_MAX_LEVEL(running->event), 0));
		}
		
		lsize = 0;
		*line = '\0';
		
		if (IS_IMMORTAL(ch)) {	// imms see id prefix
			lsize += snprintf(line + lsize, sizeof(line) - lsize, "%d.", running->id);
		}
		
		lsize += snprintf(line + lsize, sizeof(line) - lsize, " %s%s", EVT_NAME(running->event), part);
		
		if ((ped = get_event_data(ch, running->id))) {
			if ((rank = get_event_rank(ch, running)) > 0) {
				sprintf(part, "rank %d", rank);
			}
			else {
				strcpy(part, "unranked");
			}
			lsize += snprintf(line + lsize, sizeof(line) - lsize, " (%d point%s, %s)", ped->points, PLURAL(ped->points), part);
		}
		
		when = running->start_time + (EVT_DURATION(running->event) * SECS_PER_REAL_MIN);
		if (when - time(0) < 0) {
			ptr = simple_time_since(when);
			skip_spaces(&ptr);
			lsize += snprintf(line + lsize, sizeof(line) - lsize, ", ended %s ago", ptr);
		}
		
		if (lsize < sizeof(line) - 2) {
			strcat(line, "\r\n");
			lsize += 2;
		}
		
		// append
		if (lsize + size < sizeof(buf)) {
			strcat(buf, line);
			size += lsize;
		}
		else {
			size += snprintf(buf + size, sizeof(buf) - size, "**OVERFLOW**\r\n");
		}
	}
	
	if (!count) {
		strcat(buf, " none\r\n");	// always room in buf if !count
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


// shows all rewards for an event
EVENT_CMD(evcmd_rewards) {
	struct event_running_data *re, *only;
	event_data *event;
	
	// if only 1 event is running, show it if no-args
	only = only_one_running_event(NULL);
	
	if (!*argument && only) {
		show_event_rewards(ch, only);
		return;
	}
	else if (!*argument) {
		msg_to_char(ch, "View rewards for which event?\r\n");
		return;
	}
	
	// targeting: imms may target by event id
	if (IS_IMMORTAL(ch) && isdigit(*argument) && (re = find_running_event_by_id(atoi(argument)))) {
		event = re->event;
		// this is a valid case; all other targeting below happens otherwise:
	}
	else if (!(event = smart_find_event(argument, FALSE))) {
		msg_to_char(ch, "Unknown event '%s'.\r\n", argument);
		return;
	}
	else if (!(re = find_last_event_by_vnum(EVT_VNUM(event)))) {
		msg_to_char(ch, "Unable to view rewards for that event.\r\n");
		return;
	}
	
	// success
	show_event_rewards(ch, re);
}


// starts an event
EVENT_CMD(evcmd_start) {
	event_data *event;
	
	if (!*argument) {
		msg_to_char(ch, "Start what event?\r\n");
	}
	else if (!(event = smart_find_event(argument, FALSE))) {
		msg_to_char(ch, "Unknown event '%s'.\r\n", argument);
	}
	else if (find_running_event_by_vnum(EVT_VNUM(event))) {
		msg_to_char(ch, "That event is already running.\r\n");
	}
	else if (EVT_FLAGGED(event, EVTF_IN_DEVELOPMENT)) {
		msg_to_char(ch, "You cannot start that event because it's flagged as in-development.\r\n");
	}
	else {
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has started event: %d %s", GET_NAME(ch), EVT_VNUM(event), EVT_NAME(event));
		send_config_msg(ch, "ok_string");
		start_event(event);
	}
}
