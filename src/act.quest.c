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
void add_to_quest_temp_list(struct quest_temp_list **list, quest_data *quest);
extern bool can_get_quest_from_room(char_data *ch, room_data *room, struct quest_temp_list **build_list);
extern bool can_get_quest_from_obj(char_data *ch, obj_data *obj, struct quest_temp_list **build_list);
extern bool can_get_quest_from_mob(char_data *ch, char_data *mob, struct quest_temp_list **build_list);
void free_quest_temp_list(struct quest_temp_list *list);

// local prototypes


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////


 //////////////////////////////////////////////////////////////////////////////
//// QUEST SUBCOMMANDS ///////////////////////////////////////////////////////

QCMD(qcmd_check) {
	struct quest_temp_list *qtl, *quest_list = NULL;
	char buf[MAX_STRING_LENGTH];
	bool any = FALSE;
	char_data *mob;
	obj_data *obj;
	
	// search in inventory
	LL_FOREACH2(ch->carrying, obj, next_content) {
		can_get_quest_from_obj(ch, obj, &quest_list);
	}
	
	// search mobs in room
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), mob, next_in_room) {
		if (IS_NPC(mob)) {
			can_get_quest_from_mob(ch, mob, &quest_list);
		}
	}
	
	// search room room
	can_get_quest_from_room(ch, IN_ROOM(ch), &quest_list);
	
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
		
		msg_to_char(ch, " %s%s\r\n", QUEST_NAME(qtl->quest), buf);
	}
	
	if (any) {
		msg_to_char(ch, "Use 'quest start <name>' to begin a quest.\r\n");
	}
	else {
		msg_to_char(ch, "There are no quests available here.\r\n");
	}
	
	free_quest_temp_list(quest_list);
}


const struct {
	char *command;
	QCMD(*func);
	int min_pos;
} quest_cmd[] = {
	{ "check", qcmd_check, POS_RESTING },
	
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
