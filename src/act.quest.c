	/* ************************************************************************
*   File: act.quest.c                                     EmpireMUD 2.0b5 *
*  Usage: commands related to questing                                    *
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
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "skills.h"
#include "dg_scripts.h"
#include "constants.h"

/**
* Contents:
*   Helpers
*   Quest Subcommands
*   Quest Commands
*/

// for quest subcommands
#define QCMD(name)		void (name)(char_data *ch, char *argument)


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

#define QUEST_LEVEL_COLOR(ch, quest)  color_by_difficulty((ch), pick_level_from_range(GET_COMPUTED_LEVEL(ch), QUEST_MIN_LEVEL(quest), QUEST_MAX_LEVEL(quest)))


/**
* Finds all available quests at a character's location. You should use
* free_quest_temp_list() when you're done with this list.
*
* @param char_data *ch The player looking for quests.
* @return struct quest_temp_list* The quest list.
*/
struct quest_temp_list *build_available_quest_list(char_data *ch) {
	struct quest_temp_list *quest_list = NULL;
	vehicle_data *veh;
	char_data *mob;
	obj_data *obj;
	
	// search room
	can_get_quest_from_room(ch, IN_ROOM(ch), &quest_list);
	
	// search in inventory
	DL_FOREACH2(ch->carrying, obj, next_content) {
		can_get_quest_from_obj(ch, obj, &quest_list);
	}
	
	// objs in room
	DL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_content) {
		can_get_quest_from_obj(ch, obj, &quest_list);
	}
	
	// search mobs in room
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), mob, next_in_room) {
		if (IS_NPC(mob) && !EXTRACTED(mob)) {
			can_get_quest_from_mob(ch, mob, &quest_list);
		}
	}
	
	// search vehicles in room
	DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
		if (!VEH_IS_EXTRACTED(veh)) {
			can_get_quest_from_vehicle(ch, veh, &quest_list);
		}
	}
	
	return quest_list;
}


/**
* Colors by level difference between the player and the quest or area.
*
* @param char_data *ch The player.
* @param int The level to compare her to.
*/
const char *color_by_difficulty(char_data *ch, int level) {
	int diff, iter;
	
	const struct { int level; const char *color; } pairs[] = {
		// { level diff, color } order highest (hardest) to lowest (easiest)
		{ 51, "\tr" },
		{ 36, "\tp" },
		{ 26, "\to" },
		{ 16, "\ty" },
		{ -15, "\tg" },
		{ -25, "\tj" },
		{ -35, "\ta" },
		{ -50, "\tc" },
		{ INT_MIN, "\tw" }	// put this last
	};
	
	diff = level;
	SAFE_ADD(diff, -get_approximate_level(ch), INT_MIN, INT_MAX, TRUE);
	for (iter = 0; /* safely terminates itself */; ++iter) {
		if (diff >= pairs[iter].level) {
			return pairs[iter].color;
		}
	}
	
	// count never really get here
	return "\t0";
}


/**
* Completes the quest, messages the player, gives rewards.
*
* @param char_data *ch The player.
* @param struct player_quest *pq The quest to turn in.
* @param empire_data *giver_emp The empire of the quest-giver, if any.
*/
void complete_quest(char_data *ch, struct player_quest *pq, empire_data *giver_emp) {
	quest_data *quest = quest_proto(pq->vnum);
	struct player_completed_quest *pcq;
	any_vnum vnum;
	int level;
	
	if (!quest) {
		// somehow
		drop_quest(ch, pq);
		return;
	}
	
	// take objs if necessary
	if (QUEST_FLAGGED(quest, QST_EXTRACT_TASK_OBJECTS)) {
		extract_required_items(ch, pq->tracker);
	}
	remove_quest_items_by_quest(ch, QUEST_VNUM(quest));
	
	qt_quest_completed(ch, pq->vnum);
	qt_lose_quest(ch, pq->vnum);
	
	msg_to_char(ch, "You have finished %s%s\t0!\r\n%s", QUEST_LEVEL_COLOR(ch, quest), QUEST_NAME(quest), NULLSAFE(QUEST_COMPLETE_MSG(quest)));
	act("$n has finished $t!", TRUE, ch, QUEST_NAME(quest), NULL, TO_ROOM | ACT_STR_OBJ);
	
	// add quest completion
	vnum = pq->vnum;
	HASH_FIND_INT(GET_COMPLETED_QUESTS(ch), &vnum, pcq);
	if (!pcq) {
		CREATE(pcq, struct player_completed_quest, 1);
		pcq->vnum = pq->vnum;
		HASH_ADD_INT(GET_COMPLETED_QUESTS(ch), vnum, pcq);
	}
	pcq->last_completed = time(0);
	pcq->last_instance_id = pq->instance_id;
	pcq->last_adventure = pq->adventure;
	
	// completion time slightly different for dailies
	if (IS_DAILY_QUEST(quest)) {
		pcq->last_completed = data_get_long(DATA_DAILY_CYCLE);
	}
	
	// determine scale level
	level = GET_HIGHEST_KNOWN_LEVEL(ch);
	if (QUEST_MIN_LEVEL(quest) > 0) {
		level = MAX(level, QUEST_MIN_LEVEL(quest));
	}
	if (QUEST_MAX_LEVEL(quest) > 0) {
		level = MIN(level, QUEST_MAX_LEVEL(quest));
	}
	
	// give rewards
	give_quest_rewards(ch, QUEST_REWARDS(quest), level, giver_emp, pq->instance_id);
	
	// remove from player's tracker
	if (pq == global_next_player_quest) {
		global_next_player_quest = global_next_player_quest->next;
	}
	if (pq == global_next_player_quest_2) {
		global_next_player_quest_2 = global_next_player_quest_2->next;
	}
	LL_DELETE(GET_QUESTS(ch), pq);
	pq->next = NULL;	// freed as list
	free_player_quests(pq);
	
	// dailies:
	if (IS_EVENT_DAILY(quest)) {	// event daily
		GET_EVENT_DAILY_QUESTS(ch) += 1;
	
		// fail remaining quests
		if (GET_EVENT_DAILY_QUESTS(ch) >= config_get_int("dailies_per_day") && fail_daily_quests(ch, TRUE)) {
			msg_to_char(ch, "You have hit the daily quest limit for the event and your remaining event dailies expire.\r\n");
		}
	}
	else if (IS_NON_EVENT_DAILY(quest)) {	// non-event daily
		GET_DAILY_QUESTS(ch) += 1;
	
		// fail remaining quests
		if (GET_DAILY_QUESTS(ch) >= config_get_int("dailies_per_day") && fail_daily_quests(ch, FALSE)) {
			msg_to_char(ch, "You have hit the daily quest limit and your remaining daily quests expire.\r\n");
		}
	}
	
	queue_delayed_update(ch, CDU_SAVE);
}


/**
* Returns the complete/total numbers for the quest tasks the player is
* closest to completing. If a quest has multiple "or" conditions, it picks
* the best one.
*
* This function also works on empire progression goals, and any other list
* of tracked requirements.
* 
* @param struct req_data *list The list of requirements to check.
* @param int *complete A variable to store the # of complete tasks.
* @param int *total A vartiable to store the total # of tasks.
*/
void count_quest_tasks(struct req_data *list, int *complete, int *total) {
	// helper struct
	struct count_quest_data {
		int group;	// converted from "char"
		int complete;	// tasks considered done
		int total;	// total tasks in the group
		UT_hash_handle hh;
	};
	
	struct count_quest_data *cqd, *next_cqd, *cqd_list = NULL;
	int group, best_total, best_complete, ungroup_total, ungroup_complete;
	struct req_data *task;
	bool done = FALSE;
	
	if (!list) {	// shortcut for no tasks
		*complete = *total = 1;
		return;
	}
	
	// prepare data
	*complete = *total = 0;
	ungroup_total = ungroup_complete = 0;
	
	LL_FOREACH(list, task) {
		if (!task->group) {	// ungrouped "or" tasks
			if (task->current >= task->needed) {
				// found a complete "or"
				if (requirement_amt_type[task->type] == REQ_AMT_NUMBER) {
					*complete = *total = task->needed;
				}
				else {
					// just pass/fail
					*complete = *total = 1;
				}
				done = TRUE;
				break;
			}
			else if (task->current > ungroup_complete && requirement_amt_type[task->type] == REQ_AMT_NUMBER) {
				ungroup_complete = task->current;
				ungroup_total = task->needed;
			}
			else if (!ungroup_total) {
				ungroup_total = (requirement_amt_type[task->type] == REQ_AMT_NUMBER) ? task->needed : 1;
			}
		}
		else {	// grouped tasks
			// find or create group tracker
			group = (int) task->group;
			HASH_FIND_INT(cqd_list, &group, cqd);
			if (!cqd) {
				CREATE(cqd, struct count_quest_data, 1);
				cqd->group = group;
				HASH_ADD_INT(cqd_list, group, cqd);
			}
			
			cqd->total += 1;
			if (task->current >= task->needed) {
				cqd->complete += 1;
			}
		}
	}
	
	// tally data
	best_complete = ungroup_complete;	// start this lower than best_total
	best_total = MAX(1, ungroup_total);	// ensure this is not zero
	
	HASH_ITER(hh, cqd_list, cqd, next_cqd) {
		if (!done) {	// only bother with this part if we didn't find one
			if (cqd->complete >= cqd->total && best_complete < best_total) {
				// found first completion
				best_complete = cqd->complete;
				best_total = cqd->total;
			}
			else if ((cqd->complete * 100 / cqd->total) >= (best_complete * 100 / best_total)) {
				// found better completion
				best_complete = cqd->complete;
				best_total = cqd->total;
			}
		}
		
		// clean up data
		HASH_DEL(cqd_list, cqd);
		free(cqd);
	}
	
	// update data only if we didn't already finish
	if (!done) {
		*complete = MAX(0, best_complete);
		*total = MAX(1, best_total);
	}
}


/**
* Cancels all daily quests a player is on.
*
* @param char_data *ch The player to fail.
* @param bool event if TRUE, cancels event dailes; if FALSE cancels non-event dailies
* @return bool TRUE if any quests were failed, FALSE if there were none.
*/
bool fail_daily_quests(char_data *ch, bool event) {
	struct player_quest *pq;
	quest_data *quest;
	int found = 0;
	
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	LL_FOREACH_SAFE(GET_QUESTS(ch), pq, global_next_player_quest_2) {
		if (!(quest = quest_proto(pq->vnum))) {	// somehow?
			continue;
		}
		if (!IS_DAILY_QUEST(quest)) {	// not a daily
			continue;
		}
		if (event && !IS_EVENT_QUEST(quest)) {	// not event
			continue;
		}
		if (!event && IS_EVENT_QUEST(quest)) {	// event
			continue;
		}
		
		drop_quest(ch, pq);
		++found;
	}
	global_next_player_quest_2 = NULL;
	
	return (found > 0);
}


/**
* Matches an argument to a quest the player has completed.
*
* @param char_data *ch The player.
* @param char *argument The text they typed (will use a multi-keyword lookup and prefer exact matches).
* @return quest_data* The matching quest, or NULL if none.
*/
quest_data *find_completed_quest_by_name(char_data *ch, char *argument) {
	bool had_number;
	int number;
	struct player_completed_quest *pcq, *next_pcq;
	quest_data *quest, *abbrev = NULL;
	
	if (IS_NPC(ch)) {
		return NULL;	// no quests for mobs
	}
	
	had_number = isdigit(*argument) ? TRUE : FALSE;
	number = get_number(&argument);
	
	HASH_ITER(hh, GET_COMPLETED_QUESTS(ch), pcq, next_pcq) {
		if (!(quest = quest_proto(pcq->vnum))) {
			continue;
		}
		
		if (!str_cmp(argument, QUEST_NAME(quest)) && --number <= 0) {
			// exact match
			return quest;
		}
		else if (multi_isname(argument, QUEST_NAME(quest)) && ((had_number && --number <= 0) || (!had_number && !abbrev))) {
			abbrev = quest;
			
			if (had_number && number <= 0) {
				// if they requested it by number, just return it now
				return abbrev;
			}
		}
	}
	
	return abbrev;	// if any
}


/**
* Matches a quest a character can see where he is (one he is on or one avail-
* able in the room). This always prefers exact matches.
*
* @param char_data *ch The player.
* @param char *argument The quest name they typed.
* @param bool check_char Looks at character's quests.
* @param bool check_room Looks at stuff in the room.
* @param struct instance_data **find_inst If it finds an instance related to the quest, it binds it here (may be NULL even with a valid quest).
*/
quest_data *find_local_quest_by_name(char_data *ch, char *argument, bool check_char, bool check_room, struct instance_data **find_inst) {
	quest_data *quest, *abbrev = NULL;
	struct instance_data *inst, *abbrev_inst = NULL;
	
	*find_inst = NULL;
	
	if (check_char) {
		struct player_quest *pq;
		LL_FOREACH(GET_QUESTS(ch), pq) {
			if (!(quest = quest_proto(pq->vnum))) {
				continue;
			}
			
			if (!str_cmp(argument, QUEST_NAME(quest))) {
				// exact match
				*find_inst = get_instance_by_id(pq->instance_id);
				return quest;
			}
			else if (!abbrev && multi_isname(argument, QUEST_NAME(quest))) {
				abbrev = quest;
				abbrev_inst = get_instance_by_id(pq->instance_id);
			}
		}
	}
	
	if (check_room) {
		struct quest_temp_list *qtl, *quest_list;
		
		quest_list = build_available_quest_list(ch);
		LL_FOREACH(quest_list, qtl) {
			quest = qtl->quest;
			inst = qtl->instance;
			
			if (!str_cmp(argument, QUEST_NAME(quest))) {
				// exact match
				free_quest_temp_list(quest_list);
				*find_inst = inst;
				return quest;
			}
			else if (!abbrev && multi_isname(argument, QUEST_NAME(quest))) {
				abbrev = quest;
				abbrev_inst = inst;
			}
		}
		
		free_quest_temp_list(quest_list);
	}
	
	// no exact matches
	*find_inst = abbrev_inst;
	return abbrev;	// if any
}


/**
* Attempts to find an instance id to match a shared quest. This is not 100%
* reliable but should be good enough.
*
* @param char_data *ch The player who needs to match the instance.
* @param any_vnum quest_vnum Which quest to find an instance for.
* @return struct instance_data* The found instance, if any (or NULL).
*/
struct instance_data *find_matching_instance_for_shared_quest(char_data *ch, any_vnum quest_vnum) {
	struct instance_data *inst = NULL;
	struct group_member_data *mem;
	struct player_quest *pq;
	
	if (IS_NPC(ch) || !GROUP(ch)) {
		return NULL;
	}
	
	LL_FOREACH(GROUP(ch)->members, mem) {
		if (!(pq = is_on_quest(mem->member, quest_vnum))) {
			continue;	// not on the quest
		}
		if (pq->instance_id == NOTHING) {
			continue;	// no instance
		}
		
		inst = get_instance_by_id(pq->instance_id);
		
		// found one?
		if (inst) {
			return inst;
		}
	}
	
	return inst;
}


/**
* Takes a character off of a quest.
*
* @param char_data *ch The player.
* @param sturct player_quest *pq The player's quest entry to drop.
*/
void drop_quest(char_data *ch, struct player_quest *pq) {
	if (IS_NPC(ch) || !pq) {
		return;
	}
	
	qt_lose_quest(ch, pq->vnum);
	remove_quest_items_by_quest(ch, pq->vnum);
	
	if (pq == global_next_player_quest) {
		global_next_player_quest = global_next_player_quest->next;
	}
	if (pq == global_next_player_quest_2) {
		global_next_player_quest_2 = global_next_player_quest_2->next;
	}
	LL_DELETE(GET_QUESTS(ch), pq);
	pq->next = NULL;	// freed as list
	free_player_quests(pq);
}


/**
* Shown on some quest commands to indicate number of dailies remaining.
*
* @param char_data *ch The person whose count to show.
* @return char* A string to display.
*/
char *show_daily_quest_line(char_data *ch) {
	static char output[MAX_STRING_LENGTH];
	char event_part[256];
	int amount, event_amount = 0, event_count = 0, size = 0;
	
	*output = '\0';
	
	// prepare event dailies
	only_one_running_event(&event_count);
	if (event_count > 0) {
		event_amount = IS_NPC(ch) ? 0 : (config_get_int("dailies_per_day") - GET_EVENT_DAILY_QUESTS(ch));
		if (event_amount > 0) {
			safe_snprintf(event_part, sizeof(event_part), " and %d more event %s", event_amount, event_amount != 1 ? "dailies" : "daily");
		}
		else {
			safe_snprintf(event_part, sizeof(event_part), " and no more event dailies");
		}
	}
	else {
		// not running
		*event_part = '\0';
	}
	
	// non-event dailies (and messaging)
	amount = IS_NPC(ch) ? 0 : (config_get_int("dailies_per_day") - GET_DAILY_QUESTS(ch));
	if (amount > 0) {
		size += snprintf(output + size, sizeof(output) - size, "You can complete %d more daily quest%s%s today.", amount, PLURAL(amount), event_part);
	}
	else if (event_count > 0 && event_amount > 0) {
		size += snprintf(output + size, sizeof(output) - size, "You can complete no more daily quests%s today.", event_part);
	}
	else if (event_count > 0 && event_amount == 0) {
		size += snprintf(output + size, sizeof(output) - size, "You have completed all your daily quests and event dailies today.");
	}
	else {
		size += snprintf(output + size, sizeof(output) - size, "You have completed all your daily quests today.");
	}
	
	return output;
}


/**
* Basic quest-info display for the player.
*
* @param char_data *ch The player.
* @param quest_data *qst The quest to show.
*/
void show_quest_info(char_data *ch, quest_data *qst) {
	char buf[MAX_STRING_LENGTH], *buf2, vstr[128];
	struct quest_giver *giver;
	struct player_quest *pq;
	struct player_completed_quest *pcq;
	int complete, total;
	struct string_hash *str_iter, *next_str, *str_hash = NULL;

	pcq = has_completed_quest(ch, QUEST_VNUM(qst), NOTHING);
	
	if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
		sprintf(vstr, "[%5d] ", QUEST_VNUM(qst));
	}
	else {
		*vstr = '\0';
	}
	
	pq = is_on_quest(ch, QUEST_VNUM(qst));
	
	// title
	if (pq) {
		count_quest_tasks(pq->tracker, &complete, &total);
		build_page_display(ch, "%s%s%s\t0 (%d/%d task%s)", vstr, QUEST_LEVEL_COLOR(ch, qst), QUEST_NAME(qst), complete, total, PLURAL(total));
	}
	else if (pcq) {
		build_page_display(ch, "%s%s%s\t0 (completed)", vstr, QUEST_LEVEL_COLOR(ch, qst), QUEST_NAME(qst));
	}
	else {
		build_page_display(ch, "%s%s%s\t0 (not on quest)", vstr, QUEST_LEVEL_COLOR(ch, qst), QUEST_NAME(qst));
	}
	
	build_page_display_str(ch, NULLSAFE(QUEST_DESCRIPTION(qst)));
	
	// tracker
	if (pq) {
		build_page_display_str(ch, "Quest Tracker:");
		show_tracker_display(ch, pq->tracker, FALSE);
	}
	
	// show quest giver: use a string hash to remove duplicates
	LL_FOREACH(QUEST_ENDS_AT(qst), giver) {
		if (giver->type != QG_TRIGGER) {
			add_string_hash(&str_hash, quest_giver_string(giver, FALSE), 1);
		}
	}
	
	// build string
	*buf = '\0';
	HASH_ITER(hh, str_hash, str_iter, next_str) {
		sprintf(buf + strlen(buf), "%s%s", (*buf ? "; " : ""), str_iter->str);
	}
	free_string_hash(&str_hash);
	
	// show string?
	if (*buf) {
		if (strstr(buf, "#e") || strstr(buf, "#n") || strstr(buf, "#a")) {
			// #n
			buf2 = str_replace("#n", "<name>", buf);
			strcpy(buf, buf2);
			free(buf2);
			// #e
			buf2 = str_replace("#e", "<empire>", buf);
			strcpy(buf, buf2);
			free(buf2);
			// #a
			buf2 = str_replace("#a", "<empire>", buf);
			strcpy(buf, buf2);
			free(buf2);
		}
		
		build_page_display(ch, "Turn in at: %s%s", buf, QUEST_FLAGGED(qst, QST_IN_CITY_ONLY) ? " (in-city only)" : "");
	}
	
	if (QUEST_FLAGGED(qst, QST_GROUP_COMPLETION)) {
		build_page_display_str(ch, "Group completion: This quest will auto-complete if any member of your group completes it while you're present.");
	}
	
	// completed AND not on it again?
	if (pcq && !pq) {
		build_page_display(ch, "--\r\n%s", NULLSAFE(QUEST_COMPLETE_MSG(qst)));
		if (QUEST_REWARDS(qst)) {
			build_page_display_str(ch, "Quest Rewards:");
			show_quest_reward_display(ch, QUEST_REWARDS(qst), FALSE, FALSE);
		}
	}
	
	send_page_display(ch);
}


/**
* Begins a quest for a player, and sends the start messages.
*
* @param char_data *ch The person starting a quest.
* @param quest_data *qst The quest to start.
* @param struct instance_data *inst The associated instance, if any.
*/
void start_quest(char_data *ch, quest_data *qst, struct instance_data *inst) {
	char buf[MAX_STRING_LENGTH];
	struct player_quest *pq;
	int count, total;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	// triggers
	if (!check_start_quest_trigger(ch, qst, inst)) {
		return;
	}
	
	// pre-reqs are already checked for us
	msg_to_char(ch, "You start %s%s\t0:\r\n%s", QUEST_LEVEL_COLOR(ch, qst), QUEST_NAME(qst), NULLSAFE(QUEST_DESCRIPTION(qst)));
	safe_snprintf(buf, sizeof(buf), "$n starts %s.", QUEST_NAME(qst));
	act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
	
	// set up data
	CREATE(pq, struct player_quest, 1);
	pq->vnum = QUEST_VNUM(qst);
	pq->version = QUEST_VERSION(qst);
	pq->start_time = time(0);
	pq->instance_id = inst ? INST_ID(inst) : NOTHING;
	pq->adventure = inst ? GET_ADV_VNUM(INST_ADVENTURE(inst)) : NOTHING;
	pq->tracker = copy_requirements(QUEST_TASKS(qst));
	
	LL_PREPEND(GET_QUESTS(ch), pq);
	refresh_one_quest_tracker(ch, pq);
	qt_start_quest(ch, QUEST_VNUM(qst));
	
	count_quest_tasks(pq->tracker, &count, &total);
	if (count == total) {
		msg_to_char(ch, "You already meet the requirements for %s%s\t0.\r\n", QUEST_LEVEL_COLOR(ch, qst), QUEST_NAME(qst));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// QUEST SUBCOMMANDS ///////////////////////////////////////////////////////

QCMD(qcmd_completed) {
	struct player_completed_quest *pcq, *next_pcq;
	
	// sort now
	HASH_SORT(GET_COMPLETED_QUESTS(ch), sort_completed_quests_by_timestamp);
	
	build_page_display(ch, "Completed quests:");
	HASH_ITER(hh, GET_COMPLETED_QUESTS(ch), pcq, next_pcq) {
		build_page_display(ch, " %s", get_quest_name_by_proto(pcq->vnum));
	}
	
	send_page_display(ch);
}


QCMD(qcmd_drop) {
	struct instance_data *inst;
	struct player_quest *pq;
	quest_data *qst;
	
	if (!*argument) {
		msg_to_char(ch, "Drop which quest?\r\n");
	}
	else if (!(qst = find_local_quest_by_name(ch, argument, TRUE, FALSE, &inst)) || !(pq = is_on_quest(ch, QUEST_VNUM(qst)))) {
		msg_to_char(ch, "You don't seem to be on a quest called '%s'.\r\n", argument);
	}
	else {
		msg_to_char(ch, "You drop %s%s\t0.\r\n", QUEST_LEVEL_COLOR(ch, qst), QUEST_NAME(qst));
		drop_quest(ch, pq);
		queue_delayed_update(ch, CDU_SAVE);
	}
}


/**
* Attempts to turn in a single quest.
*
* @param char_data *ch The player.
* @param struct player_quest *pq The quest to attempt to finish.
* @param bool show_errors If FALSE, runs silently (e.g. trying to turn in all).
*/
bool qcmd_finish_one(char_data *ch, struct player_quest *pq, bool show_errors) {
	quest_data *quest = quest_proto(pq->vnum);
	struct group_member_data *mem;
	empire_data *giver_emp = NULL;
	int complete, total;
	
	if (!quest) {
		if (show_errors) {
			msg_to_char(ch, "Unknown quest %d.\r\n", pq->vnum);
		}
		return FALSE;
	}
	
	// preliminary completeness check
	count_quest_tasks(pq->tracker, &complete, &total);
	if (complete < total) {
		if (show_errors) {
			msg_to_char(ch, "That quest is not complete.\r\n");
		}
		return FALSE;
	}
	
	// 2nd checks: completability
	if (IS_NON_EVENT_DAILY(quest) && GET_DAILY_QUESTS(ch) >= config_get_int("dailies_per_day")) {
		if (show_errors) {
			msg_to_char(ch, "You can't finish any more daily quests today.\r\n");
		}
		return FALSE;
	}
	if (IS_EVENT_DAILY(quest) && GET_EVENT_DAILY_QUESTS(ch) >= config_get_int("dailies_per_day")) {
		if (show_errors) {
			msg_to_char(ch, "You can't finish any more event dailies today.\r\n");
		}
		return FALSE;
	}
	if (!can_turn_in_quest_at(ch, IN_ROOM(ch), quest, &giver_emp)) {
		if (show_errors) {
			msg_to_char(ch, "You can't turn that quest in here.\r\n");
		}
		return FALSE;
	}
	
	// 3rd check after refreshing tasks
	refresh_one_quest_tracker(ch, pq);
	count_quest_tasks(pq->tracker, &complete, &total);
	if (complete < total) {
		if (show_errors) {
			msg_to_char(ch, "That quest is not complete.\r\n");
		}
		return FALSE;
	}
	
	// triggers?
	if (!check_finish_quest_trigger(ch, quest, get_instance_by_id(pq->instance_id))) {
		return FALSE;
	}
	
	// SUCCESS
	complete_quest(ch, pq, giver_emp);
	
	// group completion
	if (QUEST_FLAGGED(quest, QST_GROUP_COMPLETION) && GROUP(ch)) {
		LL_FOREACH(GROUP(ch)->members, mem) {
			if (mem->member != ch && !IS_NPC(mem->member) && IN_ROOM(mem->member) == IN_ROOM(ch) && (pq = is_on_quest(mem->member, QUEST_VNUM(quest)))) {
				if (check_finish_quest_trigger(mem->member, quest, get_instance_by_id(pq->instance_id))) {
					complete_quest(mem->member, pq, giver_emp);
				}
			}
		}
	}
	
	return TRUE;
}


QCMD(qcmd_finish) {
	struct player_quest *pq;
	struct instance_data *inst = NULL;
	quest_data *qst;
	bool all, any;
	
	all = !str_cmp(argument, "all");
	
	if (CHAR_MORPH_FLAGGED(ch, MORPHF_ANIMAL) || AFF_FLAGGED(ch, AFF_NO_ATTACK)) {
		msg_to_char(ch, "You can't start or finish quests in this form.\r\n");
	}
	else if (IS_DISGUISED(ch)) {
		msg_to_char(ch, "You can't start or finish quests while disguised.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Finish which quest (use 'quest list' to see quests you're on)?\r\n");
	}
	else if (!all && (!(qst = find_local_quest_by_name(ch, argument, TRUE, FALSE, &inst)) || !(pq = is_on_quest(ch, QUEST_VNUM(qst))))) {
		msg_to_char(ch, "You don't seem to be on a quest called '%s'.\r\n", argument);
	}
	else {
		// do it
		if (all) {
			any = FALSE;
			LL_FOREACH_SAFE(GET_QUESTS(ch), pq, global_next_player_quest) {
				any |= qcmd_finish_one(ch, pq, FALSE);
			}
			global_next_player_quest = NULL;
			if (any) {
				queue_delayed_update(ch, CDU_SAVE);
			}
			else {
				msg_to_char(ch, "You don't have any quests to turn in here.\r\n");
			}
		}
		else {
			any = qcmd_finish_one(ch, pq, TRUE);
			if (any) {
				queue_delayed_update(ch, CDU_SAVE);
			}
		}
	}
}


QCMD(qcmd_group) {
	char line[MAX_STRING_LENGTH];
	struct group_member_data *mem;
	struct player_quest *pq, *fq;
	quest_data *proto;
	char_data *friend;
	int count, total;
	bool any, have;
	
	if (!GROUP(ch)) {
		msg_to_char(ch, "You are not in a group.\r\n");
		return;
	}
	if (!GET_QUESTS(ch)) {
		msg_to_char(ch, "You aren't on any quests.\r\n");
		return;
	}
	
	build_page_display(ch, "Quests in common with your group:");
	have = FALSE;
	LL_FOREACH(GET_QUESTS(ch), pq) {
		any = FALSE;
		*line = '\0';
		
		// find group members on quest
		LL_FOREACH(GROUP(ch)->members, mem) {
			friend = mem->member;
			
			if (!IS_NPC(friend) && friend != ch && (fq = is_on_quest(friend, pq->vnum))) {
				count_quest_tasks(fq->tracker, &count, &total);
				safe_snprintf(line + strlen(line), sizeof(line) - strlen(line), "%s%s (%d/%d)", (any ? ", " : ""), GET_NAME(friend), count, total);
				any = TRUE;
			}
		}
		
		if (any && *line) {
			have = TRUE;
			if ((proto = quest_proto(pq->vnum))) {
				build_page_display(ch, " %s%s\t0: %s", QUEST_LEVEL_COLOR(ch, proto), QUEST_NAME(proto), line);
			}
		}
	}
	
	if (!have) {
		build_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


QCMD(qcmd_info) {
	struct instance_data *inst;
	quest_data *qst;
	
	if (!ch->desc) {
		// can't see it anyway
	}
	else if (!*argument) {
		msg_to_char(ch, "Get info on which quest? You can use 'quest list' to list quests you're on, or\r\n'quest start' to see ones available here.\r\n");
	}
	else if (!(qst = find_local_quest_by_name(ch, argument, TRUE, TRUE, &inst)) && !(qst = find_completed_quest_by_name(ch, argument))) {
		msg_to_char(ch, "You don't see a quest called '%s' here.\r\n", argument);
	}
	else {
		show_quest_info(ch, qst);
	}
}


QCMD(qcmd_list) {
	char vstr[128], typestr[128];
	struct quest_temp_list *quest_list = NULL, *qtl;
	struct player_quest *pq;
	quest_data *proto;
	int count, total;
	
	if (!GET_QUESTS(ch)) {
		msg_to_char(ch, "You aren't on any quests.\r\n");
		msg_to_char(ch, "%s\r\n", show_daily_quest_line(ch));
		
		if ((quest_list = build_available_quest_list(ch))) {
			msg_to_char(ch, "Try typing 'start' to see a list of available quests here.\r\n");
			free_quest_temp_list(quest_list);
		}
		return;
	}
	
	build_page_display(ch, "Your quests:");
	LL_FOREACH(GET_QUESTS(ch), pq) {
		count_quest_tasks(pq->tracker, &count, &total);
		if ((proto = quest_proto(pq->vnum))) {
			if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
				sprintf(vstr, "[%5d] ", QUEST_VNUM(proto));
			}
			else {
				*vstr = '\0';
			}
			
			if (IS_EVENT_DAILY(proto)) {
				safe_snprintf(typestr, sizeof(typestr), "; event daily");
			}
			else if (IS_DAILY_QUEST(proto)) {
				safe_snprintf(typestr, sizeof(typestr), "; daily");
			}
			else if (IS_EVENT_QUEST(proto)) {
				safe_snprintf(typestr, sizeof(typestr), "; event");
			}
			else {
				*typestr = '\0';
			}
			
			build_page_display(ch, "  %s%s%s\t0 (%d/%d task%s%s)", vstr, QUEST_LEVEL_COLOR(ch, proto), QUEST_NAME(proto), count, total, PLURAL(total), typestr);
		}
	}
	
	// show dailies status, too
	build_page_display_str(ch, show_daily_quest_line(ch));
	
	// any quests available here?
	quest_list = build_available_quest_list(ch);
	if (quest_list) {
		count = 0;
		LL_COUNT(quest_list, qtl, count);
		build_page_display(ch, "There are %d quest%s available here%s.", count, PLURAL(count), PRF_FLAGGED(ch, PRF_NO_TUTORIALS) ? "" : " (type 'start' to see them)");
	}
	free_quest_temp_list(quest_list);
	
	send_page_display(ch);
}


QCMD(qcmd_share) {
	struct group_member_data *mem;
	char buf[MAX_STRING_LENGTH];
	struct instance_data *inst;
	struct player_quest *pq;
	bool any, same_room;
	char_data *friend;
	quest_data *qst;
	
	skip_spaces(&argument);
	
	if (!GROUP(ch)) {
		msg_to_char(ch, "You must be in a group to share quests.\r\n");
		return;
	}
	if (!(qst = find_local_quest_by_name(ch, argument, TRUE, FALSE, &inst)) || !(pq = is_on_quest(ch, QUEST_VNUM(qst)))) {
		msg_to_char(ch, "You don't seem to be on a quest called '%s'.\r\n", argument);
		return;
	}
	
	// look up instance (re-use the 'inst' var, which was junk above)
	// we need the same instance for the other players here
	inst = get_instance_by_id(pq->instance_id);
	if (inst && GET_ADV_VNUM(INST_ADVENTURE(inst)) != pq->adventure) {
		inst = NULL;
	}
	
	// try to share it
	any = same_room = FALSE;
	LL_FOREACH(GROUP(ch)->members, mem) {
		friend = mem->member;
		if (IS_NPC(friend) || is_on_quest(friend, QUEST_VNUM(qst))) {
			continue;
		}
		if (!char_meets_prereqs(friend, qst, inst)) {
			continue;
		}
		
		// character qualifies... but are they in the same room?
		if (IN_ROOM(ch) != IN_ROOM(friend)) {
			same_room = TRUE;
			continue;
		}
		
		any = TRUE;
		add_offer(friend, ch, OFFER_QUEST, pq->vnum);
		act("You offer to share '$t' with $N.", FALSE, ch, QUEST_NAME(qst), friend, TO_CHAR | ACT_STR_OBJ);
		safe_snprintf(buf, sizeof(buf), "$o offers to share the quest %s%s\t0 with you (use 'accept/reject quest').", QUEST_LEVEL_COLOR(friend, qst), QUEST_NAME(qst));
		act(buf, FALSE, ch, QUEST_NAME(qst), friend, TO_VICT | ACT_STR_OBJ);
	}
	
	if (!any && same_room) {
		msg_to_char(ch, "You can only share quests with group members in the same room as you.\r\n");
	}
	else if (!any) {
		msg_to_char(ch, "Nobody in your group can accept that quest.\r\n");
	}
}


QCMD(qcmd_start) {
	struct quest_temp_list *qtl, *quest_list = NULL;
	struct instance_data *inst = NULL;
	char buf[MAX_STRING_LENGTH], vstr[128], typestr[128];
	quest_data *qst;
	bool any, level_fail = FALSE;
	
	if (!*argument) {	// no-arg: just list them
		// find quests
		quest_list = build_available_quest_list(ch);
		any = FALSE;
	
		// now show any quests available
		LL_FOREACH(quest_list, qtl) {
			// show it
			if (!any) {
				msg_to_char(ch, "Quests available here:\r\n");
				any = TRUE;
			}
			
			if (QUEST_MIN_LEVEL(qtl->quest) > 0 || QUEST_MAX_LEVEL(qtl->quest) > 0) {
				sprintf(buf, " (%s)", level_range_string(QUEST_MIN_LEVEL(qtl->quest), QUEST_MAX_LEVEL(qtl->quest), 0));
			}
			else {
				*buf = '\0';
			}
			
			if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
				sprintf(vstr, "[%5d] ", QUEST_VNUM(qtl->quest));
			}
			else {
				*vstr = '\0';
			}
			
			if (IS_EVENT_DAILY(qtl->quest)) {
				safe_snprintf(typestr, sizeof(typestr), " (event daily)");
			}
			else if (IS_DAILY_QUEST(qtl->quest)) {
				safe_snprintf(typestr, sizeof(typestr), " (daily)");
			}
			else if (IS_EVENT_QUEST(qtl->quest)) {
				safe_snprintf(typestr, sizeof(typestr), " (event)");
			}
			else {
				*typestr = '\0';
			}
			
			msg_to_char(ch, "  %s%s%s%s%s\t0\r\n", vstr, QUEST_LEVEL_COLOR(ch, qtl->quest), QUEST_NAME(qtl->quest), buf, typestr);
		}
	
		if (!any) {
			msg_to_char(ch, "There are no quests available here.\r\n");
		}
	
		free_quest_temp_list(quest_list);
		
		// show dailies status, too
		msg_to_char(ch, "%s\r\n", show_daily_quest_line(ch));
	}
	else if (GET_POS(ch) < POS_STANDING) {
		// anything other than a list requires standing
		msg_to_char(ch, "You must %s to start a quest.\r\n", FIGHTING(ch) ? "finish fighting" : "be standing");
	}
	else if (CHAR_MORPH_FLAGGED(ch, MORPHF_ANIMAL) || AFF_FLAGGED(ch, AFF_NO_ATTACK)) {
		msg_to_char(ch, "You can't start or finish quests in this form.\r\n");
	}
	else if (IS_DISGUISED(ch)) {
		msg_to_char(ch, "You can't start or finish quests while disguised.\r\n");
	}
	else if (!str_cmp(argument, "all")) {
		// start all quests
		quest_list = build_available_quest_list(ch);
		any = FALSE;
		LL_FOREACH(quest_list, qtl) {
			if (get_approximate_level(ch) + 50 < QUEST_MIN_LEVEL(qtl->quest)) {
				level_fail = TRUE;
				continue;	// must validate level
			}
			if (IS_NON_EVENT_DAILY(qtl->quest) && GET_DAILY_QUESTS(ch) >= config_get_int("dailies_per_day")) {
				continue;	// too many dailies
			}
			if (IS_EVENT_DAILY(qtl->quest) && GET_EVENT_DAILY_QUESTS(ch) >= config_get_int("dailies_per_day")) {
				continue;	// too many event dailies
			}
			
			// must re-check prereqs
			if (!is_on_quest(ch, QUEST_VNUM(qtl->quest)) && char_meets_prereqs(ch, qtl->quest, qtl->instance)) {
				if (any) {
					// spacing
					msg_to_char(ch, "\r\n");
				}
				start_quest(ch, qtl->quest, qtl->instance);
				any = TRUE;
			}
		}
		
		if (!any) {
			msg_to_char(ch, "There are no quests%s you can start here.\r\n", (level_fail ? " in your level range that" : ""));
		}
		
		free_quest_temp_list(quest_list);
	}
	else if (!(qst = find_local_quest_by_name(ch, argument, FALSE, TRUE, &inst))) {
		if (is_on_quest_by_name(ch, argument)) {
			msg_to_char(ch, "You are already on that quest.\r\n");
		}
		else {
			msg_to_char(ch, "You don't see that quest here.\r\n");
			qcmd_start(ch, "");	// list quests available here
		}
	}
	else if (IS_NON_EVENT_DAILY(qst) && GET_DAILY_QUESTS(ch) >= config_get_int("dailies_per_day")) {
		msg_to_char(ch, "You can't start any more daily quests today.\r\n");
	}
	else if (IS_EVENT_DAILY(qst) && GET_EVENT_DAILY_QUESTS(ch) >= config_get_int("dailies_per_day")) {
		msg_to_char(ch, "You can't start any more event dailies today.\r\n");
	}
	else if (get_approximate_level(ch) + 50 < QUEST_MIN_LEVEL(qst)) {
		msg_to_char(ch, "You can't start that quest because it's more than 50 levels above you.\r\n");
	}
	else {
		start_quest(ch, qst, inst);
	}
}


QCMD(qcmd_tracker) {
	struct instance_data *inst;
	struct player_quest *pq;
	quest_data *qst;
	
	if (!*argument) {
		msg_to_char(ch, "Show the tracker for which quest?\r\n");
	}
	else if (!(qst = find_local_quest_by_name(ch, argument, TRUE, FALSE, &inst)) || !(pq = is_on_quest(ch, QUEST_VNUM(qst)))) {
		msg_to_char(ch, "You don't seem to be on a quest called '%s' here.\r\n", argument);
	}
	else {
		build_page_display(ch, "%s Tracker:", QUEST_NAME(qst));
		show_tracker_display(ch, pq->tracker, FALSE);
		send_page_display(ch);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// QUEST COMMAND ///////////////////////////////////////////////////////////

// quest command configs:
const struct { char *command; QCMD(*func); int min_pos; } quest_cmd[] = {
	// command, function, min pos
	{ "completed", qcmd_completed, POS_DEAD },
	{ "drop", qcmd_drop, POS_DEAD },
	{ "group", qcmd_group, POS_DEAD },
	{ "finish", qcmd_finish, POS_STANDING },
	{ "info", qcmd_info, POS_DEAD },
	{ "list", qcmd_list, POS_DEAD },
	{ "start", qcmd_start, POS_RESTING },
	{ "share", qcmd_share, POS_DEAD },
	{ "tracker", qcmd_tracker, POS_DEAD },
	
	// for historical reasons; 'check' just calls 'start now
	{ "check", qcmd_start, POS_RESTING },
	
	// this goes last
	{ "\n", NULL, POS_DEAD }
};


ACMD(do_quest) {
	char buf[MAX_STRING_LENGTH], cmd_arg[MAX_INPUT_LENGTH];
	char *arg_ptr;
	struct instance_data *inst;
	quest_data *qst;
	int iter, type;
	bool found;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs don't quest.\r\n");
		return;
	}
	if (!IS_APPROVED(ch) && config_get_bool("quest_approval")) {
		send_config_msg(ch, "need_approval_string");
		return;
	}

	// grab first arg as command
	skip_spaces(&argument);
	arg_ptr = any_one_arg(argument, cmd_arg);
	skip_spaces(&arg_ptr);
	if (!*cmd_arg) {
		if (!PRF_FLAGGED(ch, PRF_NO_TUTORIALS)) {
			msg_to_char(ch, "Available options: ");
			found = FALSE;
			for (iter = 0; *quest_cmd[iter].command != '\n'; ++iter) {
				msg_to_char(ch, "%s%s", (found ? ", " : ""), quest_cmd[iter].command);
				found = TRUE;
			}
			msg_to_char(ch, "\r\n");
		}
		qcmd_list(ch, "");
		return;
	}
	
	// look up command
	type = -1;
	for (iter = 0; *quest_cmd[iter].command != '\n'; ++iter) {
		if (is_abbrev(cmd_arg, quest_cmd[iter].command)) {
			type = iter;
			break;
		}
	}
	if (type == -1) {
		if ((qst = find_local_quest_by_name(ch, argument, TRUE, TRUE, &inst)) || (qst = find_completed_quest_by_name(ch, argument))) {
			// quest named: pass through to quest-info
			show_quest_info(ch, qst);
		}
		else {
			msg_to_char(ch, "You're not on a quest called '%s'.\r\n", argument);
		}
		return;
	}
	
	// check pos
	if (GET_POS(ch) < quest_cmd[type].min_pos) {
		if (FIGHTING(ch) || GET_POS(ch) == POS_FIGHTING) {
			msg_to_char(ch, "You can't do that while fighting!\r\n");
		}
		else {
			strcpy(buf, position_types[quest_cmd[type].min_pos]);
			*buf = LOWER(*buf);	// they are Capitalized
			msg_to_char(ch, "You need to at least be %s.\r\n", buf);
		}
		return;
	}
	
	// check func
	if (!quest_cmd[type].func) {
		msg_to_char(ch, "Quest %s is not implemented.\r\n", quest_cmd[type].command);
		return;
	}
	
	// pass on to subcommand
	(quest_cmd[type].func)(ch, arg_ptr);
}


ACMD(do_finish) {
	// pass-thru to "quest finish <args>"
	char temp[MAX_INPUT_LENGTH];
	skip_spaces(&argument);
	safe_snprintf(temp, sizeof(temp), "finish %s", argument);
	do_quest(ch, temp, 0, 0);
}


ACMD(do_start) {
	// pass-thru to "quest start <args>"
	char temp[MAX_INPUT_LENGTH];
	skip_spaces(&argument);
	safe_snprintf(temp, sizeof(temp), "start %s", argument);
	do_quest(ch, temp, 0, 0);
}
