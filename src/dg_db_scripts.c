/* ************************************************************************
*   File: dg_db_scripts.c                                 EmpireMUD 2.0b2 *
*  Usage: Contains routines to handle db functions for scripts and trigs  *
*                                                                         *
*  DG Scripts code by egreen, 1996/09/30 21:27:54, revision 3.7           *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "dg_scripts.h"
#include "utils.h"
#include "db.h"
#include "handler.h"
#include "dg_event.h"
#include "comm.h"
#include "olc.h"

void trig_data_copy(trig_data *this_data, const trig_data *trg);
void free_trigger(trig_data *trig);

extern void half_chop(char *string, char *arg1, char *arg2);

/**
* Uses strtok() to compile a list of trigger commands.
*
* @param char *input The raw trigger script.
* @return struc cmdlist_element* The compiled command list.
*/
struct cmdlist_element *compile_command_list(char *input) {
	struct cmdlist_element *list, *cmd;
	
	CREATE(list, struct cmdlist_element, 1);
	if (input) {
		char *t = strtok(input, "\n\r"); /* strtok returns NULL if str is "\r\n" */
		if (t)
			list->cmd = strdup(t);
		else
			list->cmd = strdup("* No script");

		cmd = list;
		while ((input = strtok(NULL, "\n\r"))) {
			CREATE(cmd->next, struct cmdlist_element, 1);
			cmd = cmd->next;
			cmd->cmd = strdup(input);
		}
	}
	else {
		list->cmd = strdup("* No Script");
	}

	return list;
}


void parse_trigger(FILE *trig_f, int nr) {
	void add_trigger_to_table(trig_data *trig);

	int t[2], k, attach_type;
	char line[256], *cmds, *s, flags[256], errors[MAX_INPUT_LENGTH];
	struct cmdlist_element *cle;
	trig_data *trig;

	CREATE(trig, trig_data, 1);
	trig->vnum = nr;
	add_trigger_to_table(trig);

	snprintf(errors, sizeof(errors), "trig vnum %d", nr);

	trig->name = fread_string(trig_f, errors);

	get_line(trig_f, line);
	k = sscanf(line, "%d %s %d", &attach_type, flags, t);
	trig->attach_type = (byte)attach_type;
	trig->trigger_type = asciiflag_conv(flags);
	trig->narg = (k == 3) ? t[0] : 0;

	trig->arglist = fread_string(trig_f, errors);

	cmds = s = fread_string(trig_f, errors);

	CREATE(trig->cmdlist, struct cmdlist_element, 1);
	trig->cmdlist->cmd = strdup(strtok(s, "\n\r"));
	cle = trig->cmdlist;

	while ((s = strtok(NULL, "\n\r"))) {
		CREATE(cle->next, struct cmdlist_element, 1);
		cle = cle->next;
		cle->cmd = strdup(s);
	}

	free(cmds);
}


/**
* create a new trigger from a prototype.
*
* @param trig_vnum vnum Trigger to instantiate
* @return trig_data* The trigger, or NULL if it doesn't exit
*/
trig_data *read_trigger(trig_vnum vnum) {
	trig_data *proto, *trig;

	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	if ((proto = real_trigger(vnum)) == NULL) {
		return NULL;
	}

	CREATE(trig, trig_data, 1);
	trig_data_copy(trig, proto);

	return trig;
}


void trig_data_init(trig_data *this_data) {
	this_data->vnum = NOTHING;
	this_data->data_type = 0;
	this_data->name = NULL;
	this_data->trigger_type = 0;
	this_data->cmdlist = NULL;
	this_data->curr_state = NULL;
	this_data->narg = 0;
	this_data->arglist = NULL;
	this_data->depth = 0;
	this_data->wait_event = NULL;
	this_data->purged = FALSE;
	this_data->var_list = NULL;

	this_data->next = NULL;  
}


void trig_data_copy(trig_data *this_data, const trig_data *trg) {
	trig_data_init(this_data);

	this_data->vnum = trg->vnum;
	this_data->attach_type = trg->attach_type;
	this_data->data_type = trg->data_type;
	if (trg->name) 
		this_data->name = strdup(trg->name);
	else {
		this_data->name = strdup("unnamed trigger");
		log("Trigger with no name! (%d)", trg->vnum);
	}
	this_data->trigger_type = trg->trigger_type;
	this_data->cmdlist = trg->cmdlist;
	this_data->narg = trg->narg;
	if (trg->arglist)
		this_data->arglist = strdup(trg->arglist);
}

/* for mobs and rooms: */
void dg_read_trigger(char *line, void *proto, int type) {
	char junk[8];
	int vnum, count;
	char_data *mob;
	room_data *room;
	room_template *rmt;
	adv_data *adv;
	trig_data *trproto;
	struct trig_proto_list *trg_proto, *new_trg;

	count = sscanf(line, "%s %d", junk, &vnum);

	if (count != 2) {
		syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: Error assigning trigger! - Line was\n  %s", line);
		return;
	}

	trproto = real_trigger(vnum);
	if (!trproto) {
		switch(type) {
			case MOB_TRIGGER:
				syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: dg_read_trigger: Trigger vnum #%d asked for but non-existant! (mob: %s - %d)", vnum, GET_NAME((char_data*)proto), GET_MOB_VNUM((char_data*)proto));
				break;
			case WLD_TRIGGER:
				syslog(SYS_ERROR, LVL_BUILDER, TRUE,  "SYSERR: dg_read_trigger: Trigger vnum #%d asked for but non-existant! (room:%d)", vnum, GET_ROOM_VNUM(((room_data*)proto)));
				break;
			case RMT_TRIGGER:
				syslog(SYS_ERROR, LVL_BUILDER, TRUE,  "SYSERR: dg_read_trigger: Trigger vnum #%d asked for but non-existant! (room:%d)", vnum, ((room_template*)proto)->vnum);
				break;
			case ADV_TRIGGER:
				syslog(SYS_ERROR, LVL_BUILDER, TRUE,  "SYSERR: dg_read_trigger: Trigger vnum #%d asked for but non-existant! (room:%d)", vnum, ((adv_data*)proto)->vnum);
				break;
			default:
				syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: dg_read_trigger: Trigger vnum #%d asked for but non-existant! (?)", vnum);
				break;
		}
		return;
	}

	switch(type) {
		case ADV_TRIGGER: {
			CREATE(new_trg, struct trig_proto_list, 1);
			new_trg->vnum = vnum;
			new_trg->next = NULL;
			adv = (adv_data*)proto;
			trg_proto = GET_ADV_SCRIPTS(adv);
			if (!trg_proto) {
				GET_ADV_SCRIPTS(adv) = trg_proto = new_trg;
			}
			else {
				while (trg_proto->next) {
					trg_proto = trg_proto->next;
				}
				trg_proto->next = new_trg;
			}
			break;
		}
		case MOB_TRIGGER:
			CREATE(new_trg, struct trig_proto_list, 1);
			new_trg->vnum = vnum;
			new_trg->next = NULL;

			mob = (char_data*)proto;
			trg_proto = mob->proto_script;
			if (!trg_proto) {
				mob->proto_script = trg_proto = new_trg;
			}
			else {
				while (trg_proto->next) 
					trg_proto = trg_proto->next;
				trg_proto->next = new_trg;
			}
			break;
		case RMT_TRIGGER: {
			CREATE(new_trg, struct trig_proto_list, 1);
			new_trg->vnum = vnum;
			new_trg->next = NULL;
			rmt = (room_template*)proto;
			trg_proto = GET_RMT_SCRIPTS(rmt);
			if (!trg_proto) {
				GET_RMT_SCRIPTS(rmt) = trg_proto = new_trg;
			}
			else {
				while (trg_proto->next) {
					trg_proto = trg_proto->next;
				}
				trg_proto->next = new_trg;
			}
			break;
		}
		case WLD_TRIGGER:
			CREATE(new_trg, struct trig_proto_list, 1);
			new_trg->vnum = vnum;
			new_trg->next = NULL;
			room = (room_data*)proto;
			trg_proto = room->proto_script;
			if (!trg_proto) {
				room->proto_script = trg_proto = new_trg;
			}
			else {
				while (trg_proto->next)
					trg_proto = trg_proto->next;
				trg_proto->next = new_trg;
			}

			if (trproto) {
				if (!(room->script))
					CREATE(room->script, struct script_data, 1);
				add_trigger(SCRIPT(room), read_trigger(vnum), -1);
			}
			else {
				syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: non-existant trigger #%d assigned to room #%d", vnum, GET_ROOM_VNUM(room));
			}
			break;
		default:
			syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: Trigger vnum #%d assigned to non-mob/obj/room", vnum);
	}
}

void dg_obj_trigger(char *line, obj_data *obj) {
	char junk[8];
	int vnum, count;
	trig_data *trproto;
	struct trig_proto_list *trg_proto, *new_trg;

	count = sscanf(line, "%s %d", junk, &vnum);

	if (count != 2) {
		syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: dg_obj_trigger() : Error assigning trigger! - Line was:\n  %s", line);
		return;
	}

	trproto = real_trigger(vnum);
	if (!trproto) {
		syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: Trigger vnum #%d asked for but non-existant! (Object: %s - %d)", vnum, GET_OBJ_SHORT_DESC(obj), GET_OBJ_VNUM(obj));
		return;
	}

	CREATE(new_trg, struct trig_proto_list, 1);
	new_trg->vnum = vnum;
	new_trg->next = NULL;

	trg_proto = obj->proto_script;
	if (!trg_proto) {
		obj->proto_script = trg_proto = new_trg;
	}
	else {
		while (trg_proto->next)
			trg_proto = trg_proto->next;
		trg_proto->next = new_trg;
	}
}

void assign_triggers(void *i, int type) {
	char_data *mob = NULL;
	obj_data *obj = NULL;
	room_data *room = NULL;
	trig_data *trproto;
	struct trig_proto_list *trg_proto;

	switch (type) {
		case MOB_TRIGGER:
			mob = (char_data*)i;
			trg_proto = mob->proto_script;
			while (trg_proto) {
				trproto = real_trigger(trg_proto->vnum);
				if (!trproto) {
					syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: trigger #%d non-existant, for mob #%d", trg_proto->vnum, GET_MOB_VNUM(mob));
				}
				else {
					if (!SCRIPT(mob))
						CREATE(SCRIPT(mob), struct script_data, 1);
					add_trigger(SCRIPT(mob), read_trigger(trg_proto->vnum), -1);
				}
				trg_proto = trg_proto->next;
			}
			break;
		case OBJ_TRIGGER:
			obj = (obj_data*)i;
			trg_proto = obj->proto_script;
			while (trg_proto) {
				trproto = real_trigger(trg_proto->vnum);
				if (!trproto) {
					log("SYSERR: trigger #%d non-existant, for obj #%d", trg_proto->vnum, obj->vnum);
				}
				else {
					if (!SCRIPT(obj))
						CREATE(SCRIPT(obj), struct script_data, 1);
					add_trigger(SCRIPT(obj), read_trigger(trg_proto->vnum), -1);
				}
				trg_proto = trg_proto->next;
			}
			break;
		case WLD_TRIGGER:
			room = (room_data*)i;
			trg_proto = room->proto_script;
			while (trg_proto) {
				trproto = real_trigger(trg_proto->vnum);
				if (!trproto) {
					syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: trigger #%d non-existant, for room #%d",
					trg_proto->vnum, GET_ROOM_VNUM(room));
				}
				else {
					if (!SCRIPT(room))
						CREATE(SCRIPT(room), struct script_data, 1);
					add_trigger(SCRIPT(room), read_trigger(trg_proto->vnum), -1);
				}
				trg_proto = trg_proto->next;
			}
			break;
		default:
			syslog(SYS_ERROR, LVL_BUILDER, TRUE, "SYSERR: unknown type for assign_triggers()");
			break;
	}
}
