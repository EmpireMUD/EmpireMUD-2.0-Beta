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
void free_quest_temp_list(struct quest_temp_list *list);
extern struct instance_data *get_instance_by_id(any_vnum instance_id);
extern struct player_quest *is_on_quest(char_data *ch, any_vnum quest);

// local prototypes


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
* Begins a quest for a player, and sends the start messages.
*
* @param char_data *ch The person starting a quest.
* @param quest_data *qst The quest to start.
* @param struct instance_data *inst The associated instance, if any.
*/
void start_quest(char_data *ch, quest_data *qst, struct instance_data *inst) {
	void refresh_one_quest_tracker(char_data *ch, struct player_quest *pq);
	
	char buf[MAX_STRING_LENGTH];
	struct player_quest *pq;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	// pre-reqs are already checked for us
	msg_to_char(ch, "You start %s:\r\n%s", QUEST_NAME(qst), NULLSAFE(QUEST_DESCRIPTION(qst)));
	snprintf(buf, sizeof(buf), "$n starts %s.", QUEST_NAME(qst));
	act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
	
	// set up data
	CREATE(pq, struct player_quest, 1);
	pq->vnum = QUEST_VNUM(qst);
	pq->start_time = time(0);
	pq->instance_id = inst ? inst->id : NOTHING;
	pq->adventure = inst ? GET_ADV_VNUM(inst->adventure) : NOTHING;
	pq->tracker = copy_quest_tasks(QUEST_TASKS(qst));
	
	LL_PREPEND(GET_QUESTS(ch), pq);
	refresh_one_quest_tracker(ch, pq);
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


QCMD(qcmd_list) {
	char buf[MAX_STRING_LENGTH];
	struct player_quest *pq;
	struct quest_task *task;
	int count, total;
	size_t size;
	
	if (!GET_QUESTS(ch)) {
		msg_to_char(ch, "You aren't on any quests.\r\n");
		return;
	}
	
	size = snprintf(buf, sizeof(buf), "Your quests:\r\n");
	LL_FOREACH(GET_QUESTS(ch), pq) {
		// count completion
		count = total = 0;
		LL_FOREACH(pq->tracker, task) {
			++total;
			if (task->current >= task->needed) {
				++count;
			}
		}
		
		// display
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


const struct {
	char *command;
	QCMD(*func);
	int min_pos;
} quest_cmd[] = {
	{ "check", qcmd_check, POS_RESTING },
	// drop
	// finish
	// info
	{ "list", qcmd_list, POS_DEAD },
	{ "start", qcmd_start, POS_STANDING },
	
	// this goes last
	{ "\n", NULL, POS_DEAD }
};


 //////////////////////////////////////////////////////////////////////////////
//// QUEST COMMANDS //////////////////////////////////////////////////////////

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
