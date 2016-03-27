/* ************************************************************************
*   File: act.quest.c                                     EmpireMUD 2.0b3 *
*  Usage: commands related to questing                                    *
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
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "skills.h"
#include "dg_scripts.h"

/**
* Contents:
*   Helpers
*   Quest Subcommands
*   Quest Commands
*/

// helpful
#define QCMD(name)		void (name)(char_data *ch, char *argument)

// external vars

// external funcs
extern bool can_get_quest_from_room(char_data *ch, room_data *room, struct quest_temp_list **build_list);
extern bool can_get_quest_from_obj(char_data *ch, obj_data *obj, struct quest_temp_list **build_list);
extern bool can_get_quest_from_mob(char_data *ch, char_data *mob, struct quest_temp_list **build_list);
extern bool char_meets_prereqs(char_data *ch, quest_data *quest, struct instance_data *instance);
extern struct quest_task *copy_quest_tasks(struct quest_task *from);
void free_player_quests(struct player_quest *list);
void free_quest_temp_list(struct quest_temp_list *list);
extern struct instance_data *get_instance_by_id(any_vnum instance_id);
extern struct player_quest *is_on_quest(char_data *ch, any_vnum quest);
extern char *quest_task_string(struct quest_task *task, bool show_vnums);
void refresh_one_quest_tracker(char_data *ch, struct player_quest *pq);
void scale_item_to_level(obj_data *obj, int level);

// local prototypes
void drop_quest(char_data *ch, struct player_quest *pq);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Finds all available quests at a character's location. You should use
* free_quest_temp_list() when you're done with this list.
*
* @param char_data *ch The player looking for quests.
* @return struct quest_temp_list* The quest list.
*/
struct quest_temp_list *build_available_quest_list(char_data *ch) {
	struct quest_temp_list *quest_list = NULL;
	char_data *mob;
	obj_data *obj;
	
	// search in inventory
	LL_FOREACH2(ch->carrying, obj, next_content) {
		can_get_quest_from_obj(ch, obj, &quest_list);
	}
	
	// objs in room
	LL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_content) {
		can_get_quest_from_obj(ch, obj, &quest_list);
	}
	
	// search mobs in room
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), mob, next_in_room) {
		if (IS_NPC(mob)) {
			can_get_quest_from_mob(ch, mob, &quest_list);
		}
	}
	
	return quest_list;
}


/**
* Completes the quest, messages the player, gives rewards.
*
* @param char_data *ch The player.
* @param struct player_quest *pq The quest to turn in.
* @param empire_data *giver_emp The empire of the quest-giver, if any.
*/
void complete_quest(char_data *ch, struct player_quest *pq, empire_data *giver_emp) {
	void clear_char_abilities(char_data *ch, any_vnum skill);
	
	quest_data *quest = quest_proto(pq->vnum);
	struct player_completed_quest *pcq;
	char buf[MAX_STRING_LENGTH];
	struct quest_reward *reward;
	any_vnum vnum;
	int level;
	
	if (!quest) {
		// somehow
		drop_quest(ch, pq);
		return;
	}
	
	qt_quest_completed(ch, pq->vnum);
	qt_lose_quest(ch, pq->vnum);
	
	msg_to_char(ch, "You have finished %s!\r\n%s", QUEST_NAME(quest), NULLSAFE(QUEST_COMPLETE_MSG(quest)));
	act("$n has finished $t!", TRUE, ch, QUEST_NAME(quest), NULL, TO_ROOM);
	
	// take objs if necessary
	if (QUEST_FLAGGED(quest, QST_EXTRACT_TASK_OBJECTS)) {
		struct resource_data *res = NULL;
		struct quest_task *task;
		
		LL_FOREACH(pq->tracker, task) {
			// QT_x: types that are extractable
			switch (task->type) {
				case QT_GET_COMPONENT: {
					add_to_resource_list(&res, RES_COMPONENT, task->vnum, task->needed, task->misc);
					break;
				}
				case QT_GET_OBJECT: {
					add_to_resource_list(&res, RES_OBJECT, task->vnum, task->needed, 0);
					break;
				}
			}
		}
		
		if (res) {
			extract_resources(ch, res, FALSE, NULL);
			free_resource_list(res);
		}
	}
	
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
	
	// remove from player's tracker
	LL_DELETE(GET_QUESTS(ch), pq);
	pq->next = NULL;
	free_player_quests(pq);
	
	// determine scale level
	level = get_approximate_level(ch);
	if (QUEST_MIN_LEVEL(quest) > 0) {
		level = MAX(level, QUEST_MIN_LEVEL(quest));
	}
	if (QUEST_MAX_LEVEL(quest) > 0) {
		level = MIN(level, QUEST_MAX_LEVEL(quest));
	}
	
	// give rewards
	LL_FOREACH(QUEST_REWARDS(quest), reward) {
		// QR_x: reward the rewards
		switch (reward->type) {
			case QR_BONUS_EXP: {
				msg_to_char(ch, "You gain %d bonus experience point%s!\r\n", reward->amount, PLURAL(reward->amount));
				SAFE_ADD(GET_DAILY_BONUS_EXPERIENCE(ch), reward->amount, 0, UCHAR_MAX, FALSE);
				break;
			}
			case QR_COINS: {
				empire_data *coin_emp = (reward->vnum == OTHER_COIN ? NULL : giver_emp);
				msg_to_char(ch, "You receive %s!\r\n", money_amount(coin_emp, reward->amount));
				increase_coins(ch, coin_emp, reward->amount);
				break;
			}
			case QR_OBJECT: {
				obj_data *obj = NULL;
				int iter;
				for (iter = 0; iter < reward->amount; ++iter) {
					obj = read_object(reward->vnum, TRUE);
					scale_item_to_level(obj, level);
					obj_to_char(obj, ch);
					load_otrigger(obj);
				}
				
				if (reward->amount > 0) {
					snprintf(buf, sizeof(buf), "You receive $p (x%d)!", reward->amount);
				}
				else {
					snprintf(buf, sizeof(buf), "You receive $p!");
				}
				
				if (obj) {
					act(buf, FALSE, ch, obj, NULL, TO_CHAR);
				}
				break;
			}
			case QR_SET_SKILL: {
				int val = MAX(0, MIN(CLASS_SKILL_CAP, reward->amount));
				bool loss;
				
				loss = (val < get_skill_level(ch, reward->vnum));
				set_skill(ch, reward->vnum, val);
				
				msg_to_char(ch, "Your %s is now level %d!\r\n", get_skill_name_by_vnum(reward->vnum), val);
				
				if (loss) {
					clear_char_abilities(ch, reward->vnum);
				}
				
				break;
			}
			case QR_SKILL_EXP: {
				msg_to_char(ch, "You gain %s skill experience!\r\n", get_skill_name_by_vnum(reward->vnum));
				gain_skill_exp(ch, reward->vnum, reward->amount);
				break;
			}
			case QR_SKILL_LEVELS: {
				int val;
				bool loss;
				
				val = get_skill_level(ch, reward->vnum) + reward->amount;
				val = MAX(0, MIN(CLASS_SKILL_CAP, val));
				loss = (val < get_skill_level(ch, reward->vnum));
				set_skill(ch, reward->vnum, val);
				
				msg_to_char(ch, "Your %s is now level %d!\r\n", get_skill_name_by_vnum(reward->vnum), val);
				
				if (loss) {
					clear_char_abilities(ch, reward->vnum);
				}
				break;
			}
		}
	}
	
	// TODO remove quest items}
}


/**
* @param struct player_quest *pq The player-quest entry to count.
* @param int *complete A variable to store the # of complete tasks.
* @param int *total A vartiable to store the total # of tasks.
*/
void count_quest_tasks(struct player_quest *pq, int *complete, int *total) {
	struct quest_task *task;
	
	*complete = *total = 0;
	LL_FOREACH(pq->tracker, task) {
		++*total;
		if (task->current >= task->needed) {
			++*complete;
		}
	}
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
			else if (!abbrev && is_multiword_abbrev(argument, QUEST_NAME(quest))) {
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
			else if (!abbrev && is_multiword_abbrev(argument, QUEST_NAME(quest))) {
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
	
	LL_DELETE(GET_QUESTS(ch), pq);
	pq->next = NULL;
	free_player_quests(pq);
	
	// TODO remove quest items
}


/**
* @param char_data *ch The person to show to.
* @param struct player_quest *pq The quest to show the tracker for.
*/
void show_quest_tracker(char_data *ch, struct player_quest *pq) {
	extern const bool quest_tracker_has_amount[];
	
	char buf[MAX_STRING_LENGTH];
	struct quest_task *task;
	
	msg_to_char(ch, "Quest Tracker:\r\n");
	
	LL_FOREACH(pq->tracker, task) {
		if (quest_tracker_has_amount[task->type]) {
			sprintf(buf, " (%d/%d)", MIN(task->current, task->needed), task->needed);
		}
		else {
			*buf = '\0';
		}
		msg_to_char(ch, "  %s%s\r\n", quest_task_string(task, FALSE), buf);
	}
}


/**
* Begins a quest for a player, and sends the start messages.
*
* @param char_data *ch The person starting a quest.
* @param quest_data *qst The quest to start.
* @param struct instance_data *inst The associated instance, if any.
*/
void start_quest(char_data *ch, quest_data *qst, struct instance_data *inst) {
	extern int check_start_quest_trigger(char_data *actor, quest_data *quest);
	
	char buf[MAX_STRING_LENGTH];
	struct player_quest *pq;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	// triggers
	if (!check_start_quest_trigger(ch, qst)) {
		return;
	}
	
	// pre-reqs are already checked for us
	msg_to_char(ch, "You start %s:\r\n%s", QUEST_NAME(qst), NULLSAFE(QUEST_DESCRIPTION(qst)));
	snprintf(buf, sizeof(buf), "$n starts %s.", QUEST_NAME(qst));
	act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
	
	// set up data
	CREATE(pq, struct player_quest, 1);
	pq->vnum = QUEST_VNUM(qst);
	pq->version = QUEST_VERSION(qst);
	pq->start_time = time(0);
	pq->instance_id = inst ? inst->id : NOTHING;
	pq->adventure = inst ? GET_ADV_VNUM(inst->adventure) : NOTHING;
	pq->tracker = copy_quest_tasks(QUEST_TASKS(qst));
	
	LL_PREPEND(GET_QUESTS(ch), pq);
	refresh_one_quest_tracker(ch, pq);
	
	qt_start_quest(ch, QUEST_VNUM(qst));
}


 //////////////////////////////////////////////////////////////////////////////
//// QUEST SUBCOMMANDS ///////////////////////////////////////////////////////

QCMD(qcmd_check) {
	struct quest_temp_list *qtl, *quest_list = NULL;
	char buf[MAX_STRING_LENGTH];
	bool any = FALSE;
	
	// find quests
	quest_list = build_available_quest_list(ch);
	
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
		
		msg_to_char(ch, "  %s%s\r\n", QUEST_NAME(qtl->quest), buf);
	}
	
	if (!any) {
		msg_to_char(ch, "There are no quests available here.\r\n");
	}
	
	free_quest_temp_list(quest_list);
}


QCMD(qcmd_completed) {
	char buf[MAX_STRING_LENGTH];
	struct player_completed_quest *pcq, *next_pcq;
	size_t size;
	
	size = snprintf(buf, sizeof(buf), "Completed quests:\r\n");
	HASH_ITER(hh, GET_COMPLETED_QUESTS(ch), pcq, next_pcq) {
		size += snprintf(buf + size, sizeof(buf) - size, "  %s\r\n", get_quest_name_by_proto(pcq->vnum));
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
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
		msg_to_char(ch, "You drop %s.\r\n", QUEST_NAME(qst));
		drop_quest(ch, pq);
		SAVE_CHAR(ch);
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
	extern bool can_turn_in_quest_at(char_data *ch, room_data *loc, quest_data *quest, empire_data **giver_emp);
	extern int check_finish_quest_trigger(char_data *actor, quest_data *quest);
	
	quest_data *quest = quest_proto(pq->vnum);
	empire_data *giver_emp = NULL;
	int complete, total;
	
	if (!quest) {
		if (show_errors) {
			msg_to_char(ch, "Unknown quest %d.\r\n", pq->vnum);
		}
		return FALSE;
	}
	if (!can_turn_in_quest_at(ch, IN_ROOM(ch), quest, &giver_emp)) {
		if (show_errors) {
			msg_to_char(ch, "You can't turn that quest in here.\r\n");
		}
		return FALSE;
	}
	
	
	// preliminary completeness check
	count_quest_tasks(pq, &complete, &total);
	if (complete < total) {
		if (show_errors) {
			msg_to_char(ch, "That quest is not complete.\r\n");
		}
		return FALSE;
	}
	
	// 2nd check after refreshing tasks
	refresh_one_quest_tracker(ch, pq);
	count_quest_tasks(pq, &complete, &total);
	if (complete < total) {
		if (show_errors) {
			msg_to_char(ch, "That quest is not complete.\r\n");
		}
		return FALSE;
	}
	
	// triggers?
	if (!check_finish_quest_trigger(ch, quest)) {
		return FALSE;
	}
	
	// SUCCESS
	complete_quest(ch, pq, giver_emp);
	return TRUE;
}


QCMD(qcmd_finish) {
	struct player_quest *pq, *next_pq;
	struct instance_data *inst;
	quest_data *qst;
	bool all, any;
	
	all = !str_cmp(argument, "all");
	
	if (!*argument) {
		msg_to_char(ch, "Finish which quest?\r\n");
	}
	else if (!all && (!(qst = find_local_quest_by_name(ch, argument, TRUE, FALSE, &inst)) || !(pq = is_on_quest(ch, QUEST_VNUM(qst))))) {
		msg_to_char(ch, "You don't seem to be on a quest called '%s'.\r\n", argument);
	}
	else {
		// do it
		if (all) {
			any = FALSE;
			LL_FOREACH_SAFE(GET_QUESTS(ch), pq, next_pq) {
				
				any |= qcmd_finish_one(ch, pq, FALSE);
			}
			if (any) {
				SAVE_CHAR(ch);
			}
			else {
				msg_to_char(ch, "You don't have any quests to turn in here.\r\n");
			}
		}
		else {
			any = qcmd_finish_one(ch, pq, TRUE);
			if (any) {
				SAVE_CHAR(ch);
			}
		}
	}
}


QCMD(qcmd_info) {
	struct instance_data *inst;
	struct player_quest *pq;
	int complete, total;
	quest_data *qst;
	
	if (!*argument) {
		msg_to_char(ch, "Get info on which quest?\r\n");
	}
	else if (!(qst = find_local_quest_by_name(ch, argument, TRUE, TRUE, &inst))) {
		msg_to_char(ch, "You don't see a quest called '%s' here.\r\n", argument);
	}
	else {
		pq = is_on_quest(ch, QUEST_VNUM(qst));
		
		// title
		if (pq) {
			count_quest_tasks(pq, &complete, &total);
			msg_to_char(ch, "%s (%d/%d)\r\n", QUEST_NAME(qst), complete, total);
		}
		else {
			msg_to_char(ch, "%s (not on quest)\r\n", QUEST_NAME(qst));
		}
		
		send_to_char(NULLSAFE(QUEST_DESCRIPTION(qst)), ch);
		
		// tracker
		if (pq) {
			show_quest_tracker(ch, pq);
		}
	}
}


QCMD(qcmd_list) {
	char buf[MAX_STRING_LENGTH];
	struct player_quest *pq;
	int count, total;
	size_t size;
	
	if (!GET_QUESTS(ch)) {
		msg_to_char(ch, "You aren't on any quests.\r\n");
		return;
	}
	
	size = snprintf(buf, sizeof(buf), "Your quests:\r\n");
	LL_FOREACH(GET_QUESTS(ch), pq) {
		count_quest_tasks(pq, &count, &total);
		size += snprintf(buf + size, sizeof(buf) - size, "  %s (%d/%d)\r\n", get_quest_name_by_proto(pq->vnum), count, total);
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


QCMD(qcmd_start) {
	struct quest_temp_list *qtl, *quest_list;
	struct instance_data *inst = NULL;
	quest_data *qst;
	bool any;
	
	if (!*argument) {
		msg_to_char(ch, "Start which quest?\r\n");
	}
	else if (!str_cmp(argument, "all")) {
		// start all quests
		quest_list = build_available_quest_list(ch);
		any = FALSE;
		LL_FOREACH(quest_list, qtl) {
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
			msg_to_char(ch, "There are no quests you can start here.\r\n");
		}
	}
	else if (!(qst = find_local_quest_by_name(ch, argument, FALSE, TRUE, &inst))) {
		msg_to_char(ch, "You don't see that quest here.\r\n");
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
		show_quest_tracker(ch, pq);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// QUEST COMMAND ///////////////////////////////////////////////////////////

// quest command configs:
const struct { char *command; QCMD(*func); int min_pos; } quest_cmd[] = {
	// command, function, min pos
	{ "check", qcmd_check, POS_RESTING },
	{ "completed", qcmd_completed, POS_DEAD },
	{ "drop", qcmd_drop, POS_DEAD },
	{ "finish", qcmd_finish, POS_STANDING },
	{ "info", qcmd_info, POS_DEAD },
	{ "list", qcmd_list, POS_DEAD },
	// { "share", qcmd_share, POS_DEAD },
	{ "start", qcmd_start, POS_STANDING },
	{ "tracker", qcmd_tracker, POS_DEAD },
	
	// this goes last
	{ "\n", NULL, POS_DEAD }
};


ACMD(do_quest) {
	extern const char *position_types[];
	
	char buf[MAX_STRING_LENGTH], cmd_arg[MAX_INPUT_LENGTH];
	int iter, type;
	bool found;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs don't quest.\r\n");
		return;
	}

	// grab first arg as command
	argument = any_one_arg(argument, cmd_arg);
	skip_spaces(&argument);
	if (!*cmd_arg) {
		msg_to_char(ch, "Available options: ");
		found = FALSE;
		for (iter = 0; *quest_cmd[iter].command != '\n'; ++iter) {
			msg_to_char(ch, "%s%s", (found ? ", " : ""), quest_cmd[iter].command);
			found = TRUE;
		}
		msg_to_char(ch, "\r\n");
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
		msg_to_char(ch, "Unknown quest command '%s'.\r\n", cmd_arg);
		return;
	}
	
	// check pos
	if (GET_POS(ch) < quest_cmd[type].min_pos) {
		strcpy(buf, position_types[quest_cmd[type].min_pos]);
		*buf = LOWER(*buf);	// they are Capitalized
		msg_to_char(ch, "You need to at least be %s.\r\n", buf);
		return;
	}
	
	// check func
	if (!quest_cmd[type].func) {
		msg_to_char(ch, "Quest %s is not implemented.\r\n", quest_cmd[type].command);
		return;
	}
	
	// pass on to subcommand
	(quest_cmd[type].func)(ch, argument);
}
