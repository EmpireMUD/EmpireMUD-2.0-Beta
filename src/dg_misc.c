/* ************************************************************************
*   File: dg_misc.c                                       EmpireMUD 2.0b3 *
*  Usage: contains general functions for script usage.                    *
*                                                                         *
*  DG Scripts code had no header or author info in this file              *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <math.h>

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "dg_scripts.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "dg_event.h"
#include "db.h"
#include "skills.h"

extern obj_data *die(char_data *ch, char_data *killer);

/* external vars */
extern const char *apply_types[];
extern const char *affected_bits[];



/* cast a spell; can be called by mobiles, objects and rooms, and no   */
/* level check is required. Note that mobs should generally use the    */
/* normal 'cast' command (which must be patched to allow mobs to cast  */
/* spells) as the spell system is designed to have a character caster, */
/* and this cast routine may overlook certain issues.                  */
/* LIMITATION: a target MUST exist for the spell unless the spell is   */
/* set to TAR_IGNORE. Also, group spells are not permitted             */
/* code borrowed from do_cast() */
void do_dg_cast(void *go, struct script_data *sc, trig_data *trig, int type, char *cmd) {
/*
	char_data *caster = NULL, tch = NULL;
	obj_data *tobj = NULL;
	room_data *caster_room = NULL;
	char *s, *t;
	int spellnum, target = 0;
	char buf2[MAX_STRING_LENGTH], orig_cmd[MAX_INPUT_LENGTH];

	// need to get the caster or the room of the temporary caster
	switch (type) {
		case MOB_TRIGGER:
			caster = (char_data*)go;
			break;
		case WLD_TRIGGER:
			caster_room = (room_data*)go;
			break;
		case OBJ_TRIGGER:
			caster_room = dg_room_of_obj((obj_data*)go);
			if (!caster_room) {
				script_log("dg_do_cast: unknown room for object-caster!");
				return;
			}
			break;
		default:
			script_log("dg_do_cast: unknown trigger type!");
			return;
	}

	strcpy(orig_cmd, cmd);
	// get: blank, spell name, target name
	s = strtok(cmd, "'");
	if (s == NULL) {
		script_log("Trigger: %s, VNum %d. dg_cast needs spell name.", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
		return;
	}
	s = strtok(NULL, "'");
	if (s == NULL) {
		script_log("Trigger: %s, VNum %d. dg_cast needs spell name in `'s.", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
		return;
	}
	t = strtok(NULL, "\0");

	// spellnum = search_block(s, spells, 0);
	spellnum = find_skill_num(s);
	if ((spellnum < 1) || (spellnum > MAX_SPELLS)) {
		script_log("Trigger: %s, VNum %d. dg_cast: invalid spell name (%s)",
		GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), orig_cmd);
		return;
	}

	// Find the target
	if (t != NULL) {
		one_argument(strcpy(buf2, t), t);
		skip_spaces(&t);
	}
	if (IS_SET(SINFO.targets, TAR_IGNORE)) {
		target = TRUE;
	}
	else if (t != NULL && *t) {
		if (!target && (IS_SET(SINFO.targets, TAR_CHAR_ROOM) || IS_SET(SINFO.targets, TAR_CHAR_WORLD))) {
			if ((tch = get_char(t)) != NULL)
				target = TRUE; 
		}

		if (!target && (IS_SET(SINFO.targets, TAR_OBJ_INV) || IS_SET(SINFO.targets, TAR_OBJ_EQUIP) || IS_SET(SINFO.targets, TAR_OBJ_ROOM) || IS_SET(SINFO.targets, TAR_OBJ_WORLD))) {
			if ((tobj = get_obj(t)) != NULL)
				target = TRUE; 
		}

		if (!target) {
			script_log("Trigger: %s, VNum %d. dg_cast: target not found (%s)", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), orig_cmd);
			return;
		}
	}

	if (IS_SET(SINFO.routines, MAG_GROUPS)) {
		script_log("Trigger: %s, VNum %d. dg_cast: group spells not permitted (%s)", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), orig_cmd);
		return;
	}

	if (!caster) {
		caster = read_mobile(DG_CASTER_PROXY, TRUE);
		if (!caster) {
			script_log("dg_cast: Cannot load the caster mob!");
			return;
		}
		// set the caster's name to that of the object, or the gods....
		if (type == OBJ_TRIGGER)
			caster->player.short_descr = strdup(GET_OBJ_SHORT_DESC((obj_data*)go));
		else if (type == WLD_TRIGGER)
			caster->player.short_descr = strdup("The gods");
		caster->next_in_room = caster_room->people;
		caster_room->people = caster;
		IN_ROOM(caster) = caster_room;
		//call_magic(caster, tch, tobj, spellnum, DG_SPELL_LEVEL, CAST_SPELL);
		extract_char(caster);
	}
	else {
		//call_magic(caster, tch, tobj, spellnum, GET_ACCESS_LEVEL(caster), CAST_SPELL);
	}
	*/
}


/* modify an affection on the target. affections can be of the AFF_x  */
/* variety or APPLY_x type. APPLY_x's have an integer value for them  */
/* while AFF_x's have boolean values. In any case, the duration MUST  */
/* be non-zero.                                                       */
/* usage:  apply <target> <property> <value> <duration seconds>       */
#define APPLY_TYPE	1
#define AFFECT_TYPE	2
void do_dg_affect(void *go, struct script_data *sc, trig_data *trig, int script_type, char *cmd) {
	char_data *ch = NULL;
	int value = 0, duration = 0;
	char junk[MAX_INPUT_LENGTH]; /* will be set to "dg_affect" */
	char charname[MAX_INPUT_LENGTH], property[MAX_INPUT_LENGTH];
	char value_p[MAX_INPUT_LENGTH], duration_p[MAX_INPUT_LENGTH];
	bitvector_t i = 0, type = 0;
	struct affected_type af;

	half_chop(cmd, junk, cmd);
	half_chop(cmd, charname, cmd);
	half_chop(cmd, property, cmd);
	half_chop(cmd, value_p, duration_p);

	/* make sure all parameters are present */
	if (!*charname || !*property || !*value_p || !*duration_p) {
		script_log("Trigger: %s, VNum %d. dg_affect usage: <target> <property> <value> <duration>", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
		return;
	}

	value = atoi(value_p);
	duration = atoi(duration_p);
	if (duration <= 0) {
		script_log("Trigger: %s, VNum %d. dg_affect: need positive duration!", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
		script_log("Line was: dg_affect %s %s %s %s (%d)", charname, property, value_p, duration_p, duration);
		return;
	}

	/* find the property -- first search apply_types */
	if ((i = search_block(property, apply_types, TRUE)) != NOTHING) {
		type = APPLY_TYPE;
	}

	if (!type) { /* search affect_types now */
		if ((i = search_block(property, affected_bits, TRUE)) != NOTHING) {
			type = AFFECT_TYPE;
		}
	}

	if (!type) { /* property not found */
		script_log("Trigger: %s, VNum %d. dg_affect: unknown property '%s'!", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), property);
		return;
	}

	/* locate the target */
	ch = get_char(charname);
	if (!ch) {
		script_log("Trigger: %s, VNum %d. dg_affect: cannot locate target!", GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig));
		return;
	}

	if (!str_cmp(value_p, "off")) {
		if (type == APPLY_TYPE) {
			affect_from_char_by_apply(ch, ATYPE_DG_AFFECT, i);
		}
		else {
			affect_from_char_by_bitvector(ch, ATYPE_DG_AFFECT, BIT(i));
		}
		return;
	}

	/* add the affect */
	af.type = ATYPE_DG_AFFECT;
	af.cast_by = 0;	// TODO implement this
	af.duration = ceil((double)duration / SECS_PER_REAL_UPDATE);
	af.modifier = value;

	if (type == APPLY_TYPE) {
		af.location = i;
		af.bitvector = NOBITS;
	}
	else {
		af.location = APPLY_NONE;
		af.bitvector = BIT(i);
	}

	affect_to_char(ch, &af);
}


/**
* Do the actual work for the %terracrop% commands, once everything has been
* validated.
*
* @param room_data *target The room to change.
* @param crop_data *cp The crop to change it to.
*/
void do_dg_terracrop(room_data *target, crop_data *cp) {
	sector_data *sect;
	empire_data *emp;
	
	if (!target || !cp) {
		return;
	}
	
	emp = ROOM_OWNER(target);
	
	if (!(sect = find_first_matching_sector(SECTF_CROP, NOBITS))) {
		// no crop sects?
		return;
	}
	else {
		change_terrain(target, GET_SECT_VNUM(sect));
		set_crop_type(target, cp);
		
		remove_depletion(target, DPLTN_PICK);
		remove_depletion(target, DPLTN_FORAGE);
	}
	
	if (emp) {
		read_empire_territory(emp);
	}
}


/**
* Do the actual work for the %terraform% commands, once everything has been
* validated.
*
* @param room_data *target The room to change.
* @param sector_data *sect The sector to change it to.
*/
void do_dg_terraform(room_data *target, sector_data *sect) {
	sector_data *old_sect;
	empire_data *emp;
	
	if (!target || !sect) {
		return;
	}
	
	old_sect = BASE_SECT(target);
	emp = ROOM_OWNER(target);
	
	change_terrain(target, GET_SECT_VNUM(sect));
	
	// preserve old original sect for roads -- TODO this is a special-case
	if (IS_ROAD(target)) {
		change_base_sector(target, old_sect);
	}

	if (emp) {
		read_empire_territory(emp);
	}
}


void send_char_pos(char_data *ch, int dam) {
	switch (GET_POS(ch)) {
		case POS_MORTALLYW:
			act("$n is mortally wounded, and will die soon, if not aided.", TRUE, ch, 0, 0, TO_ROOM);
			msg_to_char(ch, "You are mortally wounded, and will die soon, if not aided.\r\n");
			break;
		case POS_INCAP:
			act("$n is incapacitated and will slowly die, if not aided.", TRUE, ch, 0, 0, TO_ROOM);
			msg_to_char(ch, "You are incapacitated and will slowly die, if not aided.\r\n");
			break;
		case POS_STUNNED:
			act("$n is stunned, but will probably regain consciousness again.", TRUE, ch, 0, 0, TO_ROOM);
			msg_to_char(ch, "You're stunned, but will probably regain consciousness again.\r\n");
			break;
		case POS_DEAD:
			act("$n is dead!  R.I.P.", FALSE, ch, 0, 0, TO_ROOM);
			send_to_char("You are dead!  Sorry...\r\n", ch);
			break;
		default:                        /* >= POSITION SLEEPING */
			if (dam > (GET_MAX_HEALTH(ch) / 4))
				act("That really did HURT!", FALSE, ch, 0, 0, TO_CHAR);
			if (GET_HEALTH(ch) < (GET_MAX_HEALTH(ch) / 4))
				msg_to_char(ch, "&rYou wish that your wounds would stop BLEEDING so much!&0\r\n");
			break;
	}
}


/* Used throughout the xxxcmds.c files for checking if a char
* can be targetted 
* - allow_gods is false when called by %force%, for instance,
* while true for %teleport%.  -- Welcor 
*/
int valid_dg_target(char_data *ch, int bitvector) {
	if (IS_NPC(ch))
		return TRUE;  /* all npcs are allowed as targets */
	else if (GET_ACCESS_LEVEL(ch) < LVL_START_IMM || !NOHASSLE(ch))
		return TRUE;  /* as well as all mortals */
	else if (!IS_SET(bitvector, DG_ALLOW_GODS) && GET_ACCESS_LEVEL(ch) >= LVL_CIMPL) 
		return FALSE; /* but not always the highest gods */
	else if (GET_INVIS_LEV(ch) >= LVL_START_IMM) {
		// skip invisible immortals
		return FALSE;
	}
	else {
		// lower-level gods who are visible
		return TRUE;
	}
}


/**
* Deals damage to a character based on scaled level and modifier.
*
* @param char_data *vict The poor sod who's taking damage.
* @param char_data *killer Optional: The person to credit with the kill.
* @param int level The level to scale damage to.
* @param int dam_type The DAM_x type of damage.
* @param double modifier Percent to multiply scaled damage by (to make it lower or higher).
*/
void script_damage(char_data *vict, char_data *killer, int level, int dam_type, double modifier) {
	void death_log(char_data *ch, char_data *killer, int type);
	extern char *get_room_name(room_data *room, bool color);
	extern int reduce_damage_from_skills(int dam, char_data *victim, char_data *attacker, int damtype);
	
	double dam;
	
	// no point damaging the dead
	if (IS_DEAD(vict)) {
		return;
	}
	
	if (IS_IMMORTAL(vict) && (modifier > 0)) {
		msg_to_char(vict, "Being the cool immortal you are, you sidestep a trap, obviously placed to kill you.\r\n");
		return;
	}
	
	dam = level / 8.0;
	dam *= modifier;
	
	// guarantee at least 1
	if (modifier > 0) {
		dam = reduce_damage_from_skills(dam, vict, killer, dam_type);	// resistance, etc
		dam = MAX(1, dam);
	}
	else if (modifier < 0) {
		// healing
		dam = MIN(-1, dam);
	}
	
	GET_HEALTH(vict) -= dam;
	GET_HEALTH(vict) = MIN(GET_HEALTH(vict), GET_MAX_HEALTH(vict));

	update_pos(vict);
	send_char_pos(vict, dam);

	if (GET_POS(vict) == POS_DEAD) {
		if (!IS_NPC(vict)) {
			syslog(SYS_DEATH, 0, TRUE, "%s killed by script at %s", GET_NAME(vict), get_room_name(IN_ROOM(vict), FALSE));
			death_log(vict, vict, TYPE_SUFFERING);
		}
		die(vict, killer ? killer : vict);
	}
}  


/**
* This variant of script_damage adds a scaled damage-over-time effect to the
* victim.
*
* @param char_data *vict The person receiving the DoT.
* @param int level The level to scale damage to.
* @param int dam_type A DAM_x type.
* @param double modifier An amount to modify the damage by (1.0 = full damage).
* @param int dur_seconds The requested duration, in seconds.
* @param int max_stacks Number of times this DoT can stack (minimum/default 1).
* @param char_data *cast_by The caster, if any, for tracking on the effect (may be NULL).
*/
void script_damage_over_time(char_data *vict, int level, int dam_type, double modifier, int dur_seconds, int max_stacks, char_data *cast_by) {
	double dam;
	
	if (modifier <= 0 || dur_seconds <= 0) {
		return;
	}
	
	if (IS_IMMORTAL(vict) && (modifier > 0)) {
		msg_to_char(vict, "Being the cool immortal you are, you sidestep a trap, obviously placed to kill you.\r\n");
		return;
	}
	
	dam = level / 20.0;
	dam *= modifier;
	
	// guarantee at least 1
	if (modifier > 0) {
		dam = MAX(1, dam);
	}

	// add the affect
	apply_dot_effect(vict, ATYPE_DG_AFFECT, ceil((double)dur_seconds / SECS_PER_REAL_UPDATE), dam_type, (int) dam, MAX(1, max_stacks), cast_by);
}
