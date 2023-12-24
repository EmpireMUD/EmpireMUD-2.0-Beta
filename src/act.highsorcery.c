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
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_MIRRORIMAGE)) {
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
	MOB_ATTACK_TYPE(mob) = wield ? GET_WEAPON_TYPE(wield) : TYPE_HIT;
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


ACMD(do_vigor) {
	struct affected_type *af;
	char_data *vict = ch, *iter;
	int gain;
	bool fighting;
	
	// gain levels:		basic, specialty, class skill
	int costs[] = { 15, 25, 40 };
	int move_gain[] = { 30, 65, 125 };
	int bonus_per_intelligence = 5;
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_VIGOR, MANA, CHOOSE_BY_ABILITY_LEVEL(costs, ch, ABIL_VIGOR), NOTHING)) {
		return;
	}
	else if (*arg && !(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_VIGOR)) {
		return;
	}
	else if (GET_MOVE(vict) >= GET_MAX_MOVE(vict)) {
		if (ch == vict) {
			msg_to_char(ch, "You already have full movement points.\r\n");
		}
		else {
			act("$E already has full movement points.", FALSE, ch, NULL, vict, TO_CHAR);
		}
	}
	else if (affected_by_spell(vict, ATYPE_VIGOR)) {
		if (ch == vict) {
			msg_to_char(ch, "You can't cast vigor on yourself again until the effects of the last one have worn off.\r\n");
		}
		else {
			act("$E has had vigor cast on $M too recently to do it again.", FALSE, ch, NULL, vict, TO_CHAR);
		}
	}
	else {
		charge_ability_cost(ch, MANA, CHOOSE_BY_ABILITY_LEVEL(costs, ch, ABIL_VIGOR), NOTHING, 0, WAIT_SPELL);
		
		if (ch == vict) {
			msg_to_char(ch, "You focus your thoughts and say the word 'maktso', and you feel a burst of vigor!\r\n");
			act("$n closes $s eyes and says the word 'maktso', and $e seems suddenly refreshed.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			act("You focus your thoughts and say the word 'maktso', and $N suddenly seems refreshed.", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n closes $s eyes and says the word 'maktso', and you feel a sudden burst of vigor!", FALSE, ch, NULL, vict, TO_VICT);
			act("$n closes $s eyes and says the word 'maktso', and $N suddenly seems refreshed.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}
		
		// check if vict is in combat
		fighting = (FIGHTING(vict) != NULL);
		if (!fighting) {
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_in_room) {
				if (FIGHTING(iter) == vict) {
					fighting = TRUE;
					break;
				}
			}
		}

		gain = CHOOSE_BY_ABILITY_LEVEL(move_gain, ch, ABIL_VIGOR) + (bonus_per_intelligence * GET_INTELLIGENCE(ch));
		
		// bonus if not in combat
		if (!fighting) {
			gain *= 2;
		}
		
		set_move(vict, GET_MOVE(vict) + gain);
		
		// the cast_by on this is vict himself, because it is a penalty and this will block cleanse
		af = create_mod_aff(ATYPE_VIGOR, 75, APPLY_MOVE_REGEN, -5, vict);
		affect_join(vict, af, 0);
		
		gain_ability_exp(ch, ABIL_VIGOR, 33.4);

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
		
		run_ability_hooks(ch, AHOOK_ABILITY, ABIL_VIGOR, 0, vict, NULL, NULL, NULL, NOBITS);
	}
}
