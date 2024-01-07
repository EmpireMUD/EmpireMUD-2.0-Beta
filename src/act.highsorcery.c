/* ************************************************************************
*   File: act.highsorcery.c                               EmpireMUD 2.0b5 *
*  Usage: implementation for high sorcery abilities                       *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"
#include "constants.h"

/**
* Contents:
*   Helpers
*   Commands
*/


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_mirrorimage) {
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], *tmp;
	char_data *mob, *other;
	obj_data *wield;
	int cost = GET_MAX_MANA(ch) / 5;
	mob_vnum vnum = MIRROR_IMAGE_MOB;
	struct custom_message *ocm;
	bool found;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use mirrorimage.\r\n");
		return;
	}
	
	if (!can_use_ability(ch, ABIL_MIRRORIMAGE, MANA, cost, COOLDOWN_MIRRORIMAGE)) {
		return;
	}
	
	// limit 1
	found = FALSE;
	DL_FOREACH(character_list, other) {
		if (ch != other && IS_NPC(other) && GET_MOB_VNUM(other) == vnum && GET_LEADER(other) == ch) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		msg_to_char(ch, "You can't summon a second mirror image.\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, NULL, ABIL_MIRRORIMAGE)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, COOLDOWN_MIRRORIMAGE, 5 * SECS_PER_REAL_MIN, WAIT_COMBAT_SPELL);
	mob = read_mobile(vnum, TRUE);
	
	// scale mob to the summoner -- so it won't change its attributes later
	scale_mob_as_companion(mob, ch, 0);
	
	char_to_room(mob, IN_ROOM(ch));
	
	// restrings
	GET_PC_NAME(mob) = str_dup(PERS(ch, ch, FALSE));
	GET_SHORT_DESC(mob) = str_dup(GET_PC_NAME(mob));
	change_sex(mob, GET_SEX(ch));	// need this for some desc stuff
	
	// longdesc is more complicated
	if (GET_MORPH(ch)) {
		sprintf(buf, "%s\r\n", get_morph_desc(ch, TRUE));
	}
	else if ((ocm = pick_custom_longdesc(ch))) {
		sprintf(buf, "%s\r\n", ocm->msg);
		
		// must process $n, $s, $e, $m
		if (strstr(buf, "$n")) {
			tmp = str_replace("$n", GET_SHORT_DESC(mob), buf);
			strcpy(buf, tmp);
			free(tmp);
		}
		if (strstr(buf, "$s")) {
			tmp = str_replace("$s", HSHR(mob), buf);
			strcpy(buf, tmp);
			free(tmp);
		}
		if (strstr(buf, "$e")) {
			tmp = str_replace("$e", HSSH(mob), buf);
			strcpy(buf, tmp);
			free(tmp);
		}
		if (strstr(buf, "$m")) {
			tmp = str_replace("$m", HMHR(mob), buf);
			strcpy(buf, tmp);
			free(tmp);
		}

	}
	else {
		sprintf(buf, "%s is standing here.\r\n", GET_SHORT_DESC(mob));
	}
	*buf = UPPER(*buf);
	
	// attach rank if needed
	if (GET_LOYALTY(ch)) {
		sprintf(buf2, "<%s&0&y> %s", EMPIRE_RANK(GET_LOYALTY(ch), GET_RANK(ch)-1), buf);
		GET_LONG_DESC(mob) = str_dup(buf2);
	}
	else {
		GET_LONG_DESC(mob) = str_dup(buf);
	}
	
	// stats
	
	// inherit scaled mob health
	// mob->points.max_pools[HEALTH] = get_approximate_level(ch) * level_health_mod;
	// mob->points.current_pools[HEALTH] = mob->points.max_pools[HEALTH];
	mob->points.max_pools[MOVE] = GET_MAX_MOVE(ch);
	mob->points.current_pools[MOVE] = GET_MOVE(ch);
	mob->points.max_pools[MANA] = GET_MAX_MANA(ch);
	mob->points.current_pools[MANA] = GET_MANA(ch);
	mob->points.max_pools[BLOOD] = 10;	// not meant to be a blood source
	mob->points.current_pools[BLOOD] = 10;
	
	// mimic weapon
	wield = GET_EQ(ch, WEAR_WIELD);
	MOB_ATTACK_TYPE(mob) = wield ? GET_WEAPON_TYPE(wield) : config_get_int("default_physical_attack");
	MOB_DAMAGE(mob) = 3;	// deliberately low (it will miss anyway)
	
	// mirrors are no good at hitting or dodging
	mob->mob_specials.to_hit = 0;
	mob->mob_specials.to_dodge = 0;
	
	mob->real_attributes[STRENGTH] = GET_STRENGTH(ch);
	mob->real_attributes[DEXTERITY] = GET_DEXTERITY(ch);
	mob->real_attributes[CHARISMA] = GET_CHARISMA(ch);
	mob->real_attributes[GREATNESS] = GET_GREATNESS(ch);
	mob->real_attributes[INTELLIGENCE] = GET_INTELLIGENCE(ch);
	mob->real_attributes[WITS] = GET_WITS(ch);

	SET_BIT(AFF_FLAGS(mob), AFF_CHARM);
	set_mob_flags(mob, MOB_NO_RESCALE);
	affect_total(mob);
	
	act("You create a mirror image to distract your foes!", FALSE, ch, NULL, NULL, TO_CHAR);
	
	// switch at least 1 thing that's hitting ch
	found = FALSE;
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), other, next_in_room) {
		if (FIGHTING(other) == ch) {
			if (!found || !number(0, 1)) {
				found = TRUE;
				FIGHTING(other) = mob;
			}
		}
		else if (other != ch) {	// only people not hitting ch see the split
			act("$n suddenly splits in two!", TRUE, ch, NULL, other, TO_VICT);
		}
	}
	
	if (FIGHTING(ch)) {
		set_fighting(mob, FIGHTING(ch), FIGHT_MODE(ch));
	}
	
	add_follower(mob, ch, FALSE);
	gain_ability_exp(ch, ABIL_MIRRORIMAGE, 15);
	run_ability_hooks(ch, AHOOK_ABILITY, ABIL_MIRRORIMAGE, 0, mob, NULL, NULL, NULL, NOBITS);
	
	load_mtrigger(mob);
}
