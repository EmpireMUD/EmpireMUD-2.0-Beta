/* ************************************************************************
*   File: act.vampire.c                                   EmpireMUD 2.0b1 *
*  Usage: Code related to the Vampire ability and its commands            *
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
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Consts
*   Helpers
*   Commands
*/

// external vars
extern const int universal_wait;

// external funcs
extern bool check_scaling(char_data *mob, char_data *based_on);
extern obj_data *die(char_data *ch, char_data *killer);
void end_morph(char_data *ch);

// locals
bool check_vampire_sun(char_data *ch, bool message);
ACMD(do_bite);
void end_alacrity(char_data *ch);
void end_boost(char_data *ch);
void end_majesty(char_data *ch);
void retract_claws(char_data *ch);
void un_deathshroud(char_data *ch);
void un_mummify(char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// CONSTS //////////////////////////////////////////////////////////////////

// flags for vampire_upkeep[] -- if you add a new one, modify get_blood_upkeep_cost() and cancel_blood_upkeeps()
#define VUP_AFFECT  1	// represents an active affect ATYPE_x
#define VUP_MORPH  2	// represents a MORPH_x state


// The structure of upkeep costs for vampire powers
struct vampire_upkeep_type {
	int type;	// VUP_x
	bitvector_t value;	// depending on the type, a AFF flag, affect id, &c.
	int blood;	// blood cost per hour
	void (*cancel_function) (char_data *ch);	// the function to cancel the power if upkeep runs out
};

const struct vampire_upkeep_type vampire_upkeep[] = {
	{ VUP_MORPH, MORPH_WOLF, 1, end_morph },
	{ VUP_MORPH, MORPH_BAT, 1, end_morph },
	{ VUP_MORPH, MORPH_MIST, 1, end_morph },
	
	// affects are charged per instance of the ATYPE_x flag, so boost may charge twice
	{ VUP_AFFECT, ATYPE_ALACRITY, 3, end_alacrity },
	{ VUP_AFFECT, ATYPE_BOOST, 1, end_boost },
	{ VUP_AFFECT, ATYPE_CLAWS, 2, retract_claws },
	{ VUP_AFFECT, ATYPE_MAJESTY, 3, end_majesty },
	{ VUP_AFFECT, ATYPE_MUMMIFY, 1, un_mummify },
	{ VUP_AFFECT, ATYPE_DEATHSHROUD, 1, un_deathshroud },

	// this goes last
	{ NOTHING, NOTHING, NOTHING, NULL }
};


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Attempts to cancel the vampire powers that require upkeeps, e.g. upon
* death or when out of blood.
*
* @param char_data *ch The vampire paying upkeeps.
*/
void cancel_blood_upkeeps(char_data *ch) {
	int iter;
	bool active = FALSE;
	
	if (!IS_NPC(ch) && IS_VAMPIRE(ch)) {
		for (iter = 0; vampire_upkeep[iter].type != NOTHING; ++iter) {
			active = FALSE;
			
			// see if active
			switch (vampire_upkeep[iter].type) {
				case VUP_AFFECT: {
					active = affected_by_spell(ch, vampire_upkeep[iter].value);
					break;
				}
				case VUP_MORPH: {
					active = (GET_MORPH(ch) == vampire_upkeep[iter].value);
					break;
				}
			}
			
			// did we find it?
			if (active && vampire_upkeep[iter].cancel_function != NULL) {
				(vampire_upkeep[iter].cancel_function)(ch);
			}
		}
	}
}


/**
* Called when the player cancels ACT_SIRING for some reason.
*
* @param char_data *ch The acting player
*/
void cancel_siring(char_data *ch) {
	if (GET_FEEDING_FROM(ch)) {
		do_bite(ch, "", 0, 0);
	}
}


// checks for Blood Fortitude and does skill gain
bool check_blood_fortitude(char_data *ch) {
	if (!IS_NPC(ch) && IS_VAMPIRE(ch) && check_vampire_sun(ch, FALSE) && HAS_ABILITY(ch, ABIL_BLOOD_FORTITUDE)) {
		gain_ability_exp(ch, ABIL_BLOOD_FORTITUDE, 1);
		return TRUE;
	}
	return FALSE;
}


// returns TRUE if the character is a vampire and has the ability; sends its own error
bool check_vampire_ability(char_data *ch, int ability, int cost_pool, int cost_amount, int cooldown_type) {
	if (!IS_VAMPIRE(ch)) {
		send_config_msg(ch, "must_be_vampire");
		return FALSE;
	}
	if (!can_use_ability(ch, ability, cost_pool, cost_amount, cooldown_type)) {
		return FALSE;
	}
	
	return TRUE;
}


/**
* @param char_data *ch the vampire
* @param bool message if TRUE, sends the you-can't-do-that-in-sun
* @return bool TRUE if the ability can proceed, FALSE if sunny
*/
bool check_vampire_sun(char_data *ch, bool message) {
	if (IS_NPC(ch) || HAS_ABILITY(ch, ABIL_DAYWALKING) || IS_GOD(ch) || IS_IMMORTAL(ch) || AFF_FLAGGED(ch, AFF_EARTHMELD) || !check_sunny(IN_ROOM(ch))) {
		// ok -- not sunny
		return TRUE;
	}
	else {
		// sunny
		if (message) {
			msg_to_char(ch, "Your powers don't work in direct sunlight.\r\n");
		}
		return FALSE;
	}
}


// for do_alacrity
void end_alacrity(char_data *ch) {
	if (affected_by_spell(ch, ATYPE_ALACRITY)) {
		affect_from_char(ch, ATYPE_ALACRITY);
		msg_to_char(ch, "Your supernatural alacrity fades.\r\n");
		if (!AFF_FLAGGED(ch, AFF_HASTE)) {
			act("$n seems to slow down.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
	}
}


/**
* Ends all boost affects on a character.
*
* @param char_data *ch The boosted character.
*/
void end_boost(char_data *ch) {
	if (affected_by_spell(ch, ATYPE_BOOST)) {
		msg_to_char(ch, "Your boost fades.\r\n");
		affect_from_char(ch, ATYPE_BOOST);
	}
}


// for do_majesty: shuts off majesty
void end_majesty(char_data *ch) {
	if (AFF_FLAGGED(ch, AFF_MAJESTY)) {
		affects_from_char_by_aff_flag(ch, AFF_MAJESTY);
		if (!AFF_FLAGGED(ch, AFF_MAJESTY)) {
			msg_to_char(ch, "You reduce your supernatural majesty.\r\n");
			act("$n seems less majestic now.", TRUE, ch, 0, 0, TO_ROOM);
		}
	}
}


/**
* Computes the total upkeep for a character based on the vampire_upkeep[]
* struct.
*
* @param char_data *ch The vampire.
* @return int The upkeep blood cost total.
*/
int get_blood_upkeep_cost(char_data *ch) {
	struct affected_type *af;
	int iter;
	int cost = 0;
	
	if (!IS_NPC(ch) && IS_VAMPIRE(ch) && !IS_IMMORTAL(ch)) {
		// low skill vamp upkeep
		if (GET_SKILL(ch, SKILL_VAMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
			cost += 1;
		}
	
		// normal upkeeps from the table
		for (iter = 0; vampire_upkeep[iter].type != NOTHING; ++iter) {
			switch (vampire_upkeep[iter].type) {
				case VUP_AFFECT: {
					for (af = ch->affected; af; af = af->next) {
						if (af->type == vampire_upkeep[iter].value) {
							cost += vampire_upkeep[iter].blood;
						}
					}
					break;
				}
				case VUP_MORPH: {
					if (GET_MORPH(ch) == vampire_upkeep[iter].value) {
						cost += vampire_upkeep[iter].blood;
					}
					break;
				}
			}
		}
	}
	
	return cost;
}


// max blood is set in mobfile for npc, but computed for player
int GET_MAX_BLOOD(char_data *ch) {
	extern const int base_player_pools[NUM_POOLS];
	
	int base = base_player_pools[BLOOD];
	
	if (IS_NPC(ch)) {
		base = ch->points.max_pools[BLOOD];
	}
	else {
		base += GET_EXTRA_BLOOD(ch);

		if (IS_VAMPIRE(ch)) {
			if (IS_SPECIALTY_SKILL(ch, SKILL_VAMPIRE)) {
				base += 50;
			}
			if (IS_CLASS_SKILL(ch, SKILL_VAMPIRE)) {
				base += 50;
			}

			if (HAS_ABILITY(ch, ABIL_ANCIENT_BLOOD)) {
				base *= 2;
			}
		}
	}

	return base;
}


/**
* char_data *ch becomes a vampire
* bool lore if TRUE, also adds lore
*/
void make_vampire(char_data *ch, bool lore) {
	void set_skill(char_data *ch, int skill, int level);
	
	if (!IS_NPC(ch)) {	
		/* set BEFORE set as a vampire! */
		GET_APPARENT_AGE(ch) = GET_AGE(ch);

		SET_BIT(PLR_FLAGS(ch), PLR_VAMPIRE);
	
		if (GET_SKILL(ch, SKILL_VAMPIRE) < 1) {
			set_skill(ch, SKILL_VAMPIRE, 1);
		}

		GET_BLOOD(ch) = 30;
	
		if (lore) {
			add_lore(ch, LORE_START_VAMPIRE, -1);
		}
	
		SAVE_CHAR(ch);
	}
}


// for do_claws
void retract_claws(char_data *ch) {
	if (AFF_FLAGGED(ch, AFF_CLAWS)) {
		affects_from_char_by_aff_flag(ch, AFF_CLAWS);
		if (!AFF_FLAGGED(ch, AFF_CLAWS)) {
			msg_to_char(ch, "Your claws meld back into your fingers!\r\n");
			act("$n's claws meld back into $s fingers!", TRUE, ch, 0, 0, TO_ROOM);
		}
	}
}


// for do_sire
void sire_char(char_data *ch, char_data *victim) {
	GET_FEEDING_FROM(ch) = NULL;
	GET_FED_ON_BY(victim) = NULL;

	act("$N drops limply from your fangs...", FALSE, ch, 0, victim, TO_CHAR);
	act("$N drops limply from $n's fangs...", FALSE, ch, 0, victim, TO_NOTVICT);
	act("You fall limply the ground. In the distance, you think you see a light...", FALSE, ch, 0, victim, TO_VICT);

	if (CAN_GAIN_NEW_SKILLS(victim)) {
		make_vampire(victim, FALSE);
		GET_BLOOD(ch) -= 10;

		act("You tear open your wrist with your fangs and drip blood into $N's mouth!", FALSE, ch, 0, victim, TO_CHAR);
		act("$n tears open $s wrist with $s teeth and drips blood into $N's mouth!", FALSE, ch, 0, victim, TO_NOTVICT);
		msg_to_char(victim, "&rSuddenly, a warm sensation touches your lips and a stream of blood flows down your throat...&0\r\n");
		msg_to_char(victim, "&rAs the blood fills you, a strange sensation covers your body... The light in the distance turns blood-red and a hunger builds within you!&0\r\n");

		msg_to_char(victim, "You sit up quickly, nearly knocking over your sire!\r\n");
		act("$n sits up quickly!", FALSE, victim, 0, 0, TO_ROOM);

		add_lore(victim, LORE_SIRE_VAMPIRE, GET_IDNUM(ch));
		add_lore(ch, LORE_MAKE_VAMPIRE, GET_IDNUM(victim));

		/* Turn off that SIRING action */
		GET_ACTION(ch) = ACT_NONE;

		SAVE_CHAR(ch);
		SAVE_CHAR(victim);
	}
	else {
		// can't gain skills!
		die(victim, ch);	// returns a corpse but we don't need it
	}
}


// sends the message that a sunlight effect is happening
void sun_message(char_data *ch) {
	if (AWAKE(ch)) {
		msg_to_char(ch, "You wince and recoil from the sunlight!\r\n");
		act("$n winces and recoils from the sunlight!", TRUE, ch, 0, 0, TO_ROOM);
	}
	else {
		msg_to_char(ch, "The sun shines down on you.\r\n");
		act("The sun shines down on $n.", TRUE, ch, 0, 0, TO_ROOM);
	}
}


/**
* This comes from a special case of do_eat() subcmd SCMD_TASTE.
* At the start of this function, we're only sure they typed "taste <vict>"
*
* char_data *ch the person doing the tasting
* char_data *vict the person being tasted
*/
void taste_blood(char_data *ch, char_data *vict) {
	if (GET_FEEDING_FROM(ch) && GET_FEEDING_FROM(ch) != vict)
		msg_to_char(ch, "You've already got your fangs in someone else.\r\n");
	else if (GET_ACTION(ch) != ACT_NONE)
		msg_to_char(ch, "You're a bit busy right now.\r\n");
	else if (ch == vict)
		msg_to_char(ch, "It seems redundant to taste your own blood..\r\n");
	else if (!IS_NPC(vict) && AWAKE(vict) && !PRF_FLAGGED(vict, PRF_BOTHERABLE))
		act("You don't have permission to taste $N's blood!", FALSE, ch, 0, vict, TO_CHAR);
	else {
		act("You taste a sample of $N's blood!", FALSE, ch, 0, vict, TO_CHAR);
		act("$n tastes a sample of your blood!", FALSE, ch, 0, vict, TO_VICT);
		act("$n tastes a sample of $N's blood!", FALSE, ch, 0, vict, TO_NOTVICT);

		sprintf(buf, "This is $O, a %s.", IS_VAMPIRE(vict) ? "vampire" : (!IS_NPC(vict) || !MOB_FLAGGED(vict, MOB_ANIMAL)) ? "human" : "animal");
		act(buf, FALSE, ch, 0, vict, TO_CHAR);

		if (GET_BLOOD(vict) != GET_MAX_BLOOD(vict)) {
			sprintf(buf, "$E is missing about %d%% of $S blood.", (GET_MAX_BLOOD(vict) - GET_BLOOD(vict)) * 100 / GET_MAX_BLOOD(vict));
			act(buf, FALSE, ch, 0, vict, TO_CHAR);
			}

		sprintf(buf, "$E is about %d years old.", GET_AGE(vict) + number(-1, 1));
		act(buf, FALSE, ch, 0, vict, TO_CHAR);
		
		WAIT_STATE(ch, universal_wait);
		gain_ability_exp(ch, ABIL_TASTE_BLOOD, 20);
	}
}


/**
* Ends the mummify affect.
*
* @param char_data *ch The mummified person.
*/
void un_mummify(char_data *ch) {
	if (AFF_FLAGGED(ch, AFF_MUMMIFY)) {
		affects_from_char_by_aff_flag(ch, AFF_MUMMIFY);
		if (!AFF_FLAGGED(ch, AFF_MUMMIFY)) {
			msg_to_char(ch, "Your flesh softens and the mummified layers flake off!\r\n");
			act("$n sheds a layer of skin and appears normal again!", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_STANDING;
		}
	}
}


/**
* Ends the deathshroud effect.
*
* @param char_data *ch The deathshrouded player.
*/
void un_deathshroud(char_data *ch) {
	if (AFF_FLAGGED(ch, AFF_DEATHSHROUD)) {
		affects_from_char_by_aff_flag(ch, AFF_DEATHSHROUD);
		if (!AFF_FLAGGED(ch, AFF_DEATHSHROUD)) {
			msg_to_char(ch, "Your flesh returns to normal!\r\n");
			act("$n appears normal again!", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_STANDING;
		}
	}
}


// undo vampirism
void un_vampire(char_data *ch) {
	void clear_char_abilities(char_data *ch, int skill);

	if (!IS_NPC(ch)) {
		add_lore(ch, LORE_PURIFY, -1);
		REMOVE_BIT(PLR_FLAGS(ch), PLR_VAMPIRE);
		GET_BLOOD(ch) = GET_MAX_BLOOD(ch);
		clear_char_abilities(ch, SKILL_VAMPIRE);
		SAVE_CHAR(ch);
	}
}


/**
* Updates the "biting" action, per 5 seconds.
*
* @param char_data *ch The person who might be biting, to update.
*/
void update_biting_char(char_data *ch) {
	void death_log(char_data *ch, char_data *killer, int type);
	
	char_data *victim;
	obj_data *corpse;
	int amount;

	if (!(victim = GET_FEEDING_FROM(ch)))
		return;
	
	check_scaling(victim, ch);

	// permanent cancels
	if (GET_POS(ch) < POS_STANDING || IN_ROOM(victim) != IN_ROOM(ch) || GET_BLOOD(victim) <= 0 || IS_DEAD(ch) || IS_DEAD(victim) || EXTRACTED(ch) || EXTRACTED(victim)) {
		GET_FEEDING_FROM(ch) = NULL;
		GET_FED_ON_BY(victim) = NULL;
		return;
	}
	
	// Transfuse blood -- 10-25 points (pints?) at a time
	amount = MIN(number(10, 25), GET_BLOOD(victim));
	GET_BLOOD(victim) -= amount;
	
	// can gain more
	if (HAS_ABILITY(ch, ABIL_ANCIENT_BLOOD)) {
		amount *= 2;
	}
	GET_BLOOD(ch) = MIN(GET_MAX_BLOOD(ch), GET_BLOOD(ch) + amount);

	if (GET_BLOOD(victim) <= 0 && GET_ACTION(ch) != ACT_SIRING) {
		GET_BLOOD(victim) = 0;

		if (!IS_NPC(victim) && !PRF_FLAGGED(ch, PRF_AUTOKILL)) {
			do_bite(ch, "", 0, 0);
			return;
		}

		act("You pull the last of $N's blood from $S veins, and $E falls limply to the ground!", FALSE, ch, 0, victim, TO_CHAR);
		act("$N falls limply from $n's arms!", FALSE, ch, 0, victim, TO_NOTVICT);
		act("You feel faint as the last of your blood is pulled from your body!", FALSE, ch, 0, victim, TO_VICT);
		
		gain_ability_exp(ch, ABIL_ANCIENT_BLOOD, 15);

		act("$n is dead! R.I.P.", FALSE, victim, 0, 0, TO_ROOM);
		msg_to_char(victim, "You are dead! Sorry...\r\n");
		if (!IS_NPC(victim)) {
			death_log(victim, ch, ATTACK_EXECUTE);
			add_lore(ch, LORE_PLAYER_KILL, GET_IDNUM(victim));
			add_lore(victim, LORE_PLAYER_DEATH, GET_IDNUM(ch));
		}

		GET_FED_ON_BY(victim) = NULL;
		GET_FEEDING_FROM(ch) = NULL;

		corpse = die(victim, ch);
		
		// tag corpse
		if (corpse && !IS_NPC(ch)) {
			corpse->last_owner_id = GET_IDNUM(ch);
			corpse->last_empire_id = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING;
		}
	}
	else {
		act("You shudder with ecstasy as you feed from $N!", FALSE, ch, 0, victim, TO_CHAR);
		act("$n shudders with ecstasy as $e feeds from $N!", FALSE, ch, 0, victim, TO_NOTVICT);
		act("A surge of ecstasy fills your body as $n feeds from your veins!", FALSE, ch, 0, victim, TO_VICT);
		
		if ((GET_BLOOD(victim) * 100 / GET_MAX_BLOOD(victim)) < 20) {
			act("...$e sways from lack of blood.", FALSE, victim, NULL, NULL, TO_ROOM);
		}
		
		if (HAS_ABILITY(ch, ABIL_ANCIENT_BLOOD)) {
			gain_ability_exp(ch, ABIL_ANCIENT_BLOOD, 1);
		}
	}
	
	gain_ability_exp(ch, ABIL_SANGUINE_RESTORATION, 2);
	gain_ability_exp(ch, ABIL_UNNATURAL_THIRST, 2);
	if (GET_SKILL(ch, SKILL_VAMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
		gain_skill_exp(ch, SKILL_VAMPIRE, 5);
	}
}


void update_vampire_sun(char_data *ch) {
	bool found = FALSE;
	
	// only applies if vampire and not an NPC
	if (IS_NPC(ch) || !IS_VAMPIRE(ch) || check_vampire_sun(ch, FALSE)) {
		return;
	}
	
	if (affected_by_spell(ch, ATYPE_ALACRITY)) {
		if (!found) {
			sun_message(ch);
		}
		found = TRUE;
		end_alacrity(ch);
	}
	
	if (affected_by_spell(ch, ATYPE_CLAWS)) {
		if (!found) {
			sun_message(ch);
		}
		found = TRUE;
		retract_claws(ch);
	}
	
	if (affected_by_spell(ch, ATYPE_MAJESTY)) {
		if (!found) {
			sun_message(ch);
		}
		found = TRUE;
		end_majesty(ch);
	}
	
	// revert vampire morphs
	if (GET_MORPH(ch) != MORPH_NONE && MORPH_FLAGGED(ch, MORPH_FLAG_VAMPIRE_ONLY)) {
		if (!found) {
			sun_message(ch);
		}
		
		// store morph name
		sprintf(buf, "%s lurches and reverts into $n!", PERS(ch, ch, 0));

		perform_morph(ch, MORPH_NONE);

		act(buf, TRUE, ch, 0, 0, TO_ROOM);
		msg_to_char(ch, "You revert to your natural form!\r\n");
	}

	if (affected_by_spell(ch, ATYPE_BOOST)) {
		if (!found) {
			sun_message(ch);
		}
		found = TRUE;
		affect_from_char(ch, ATYPE_BOOST);
	}
	
	// lastly
	if (found) {
		WAIT_STATE(ch, universal_wait);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_alacrity) {
	struct affected_type *af;
	int cost = 10;
	
	if (affected_by_spell(ch, ATYPE_ALACRITY)) {
		end_alacrity(ch);
		WAIT_STATE(ch, universal_wait);
		return;
	}
	
	if (!check_vampire_ability(ch, ABIL_ALACRITY, BLOOD, cost, NOTHING)) {
		return;
	}
	if (!check_vampire_sun(ch, TRUE)) {
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_ALACRITY)) {
		return;
	}

	msg_to_char(ch, "You focus your blood into your lithe muscles...\r\n");
	if (!AFF_FLAGGED(ch, AFF_HASTE)) {
		msg_to_char(ch, "You speed up immensely!\r\n");
		act("$n seems to speed up!", TRUE, ch, 0, 0, TO_ROOM);
	}
	
	af = create_flag_aff(ATYPE_ALACRITY, UNLIMITED, AFF_HASTE);
	affect_join(ch, af, 0);

	charge_ability_cost(ch, BLOOD, cost, NOTHING, 0);
	gain_ability_exp(ch, ABIL_ALACRITY, 20);
}


ACMD(do_bite) {
	extern int get_to_hit(char_data *ch, bool off_hand);
	extern int get_dodge_modifier(char_data *ch);

	char_data *victim, *ch_iter;
	int hit_chance, success;
	bool found;

	one_argument(argument, arg);

	if (GET_FEEDING_FROM(ch)) {
		msg_to_char(ch, "You stop feeding.\r\n");
		act("$n stops feeding from $N.", FALSE, ch, 0, GET_FEEDING_FROM(ch), TO_ROOM);
		GET_FED_ON_BY(GET_FEEDING_FROM(ch)) = NULL;
		GET_FEEDING_FROM(ch) = NULL;
	}
	else if (!IS_VAMPIRE(ch)) {
		send_config_msg(ch, "must_be_vampire");
	}
	else if (IS_NPC(ch)) {
		msg_to_char(ch, "Nope.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_SIRING && GET_ACTION(ch) != ACT_NONE)
		msg_to_char(ch, "You're a bit busy right now.\r\n");
	else if (!*arg)
		msg_to_char(ch, "Bite whom?\r\n");
	else if (subcmd ? (!(victim = get_player_vis(ch, arg, FIND_CHAR_ROOM))) : (!(victim = get_char_vis(ch, arg, FIND_CHAR_ROOM))))
		send_config_msg(ch, "no_person");
	else if (ch == victim)
		msg_to_char(ch, "That seems a bit redundant..\r\n");
	else if (GET_FED_ON_BY(victim))
		msg_to_char(ch, "Sorry, somebody beat you to it.\r\n");
	else if (IS_GOD(victim) || IS_IMMORTAL(victim))
		msg_to_char(ch, "You can't bite an immortal!\r\n");
	else if ((IS_NPC(victim) || !PRF_FLAGGED(victim, PRF_BOTHERABLE)) && !can_fight(ch, victim))
		act("You can't attack $N!", FALSE, ch, 0, victim, TO_CHAR);
	else if (check_scaling(victim, ch) && !PRF_FLAGGED(victim, PRF_BOTHERABLE) && AWAKE(victim) && GET_HEALTH(victim) > MAX(5, GET_MAX_HEALTH(victim)/20)) {
		// must scale before checking health ^
		msg_to_char(ch, "You can only bite people who are vulnerable or low on health.\r\n");
	}
	else {
		// final check: is anybody other than victim fighting me
		found = FALSE;
		for (ch_iter = ROOM_PEOPLE(IN_ROOM(ch)); ch_iter && !found; ch_iter = ch_iter->next_in_room) {
			if (ch_iter != victim && FIGHTING(ch_iter) == ch) {
				found = TRUE;
			}
		}
		
		if (found) {
			msg_to_char(ch, "You can't do that while someone else is attacking you!\r\n");
		}
		else {
			// SUCCESS
			if (SHOULD_APPEAR(ch))
				appear(ch);

			/* if the person isn't biteable, gotta roll! */
			if ((IS_NPC(victim) || !PRF_FLAGGED(victim, PRF_BOTHERABLE)) && AWAKE(victim)) {
				// determine hit
				hit_chance = get_to_hit(ch, FALSE);
	
				// evasion
				if (CAN_SEE(victim, ch)) {
					hit_chance -= get_dodge_modifier(victim);
				}

				success = !AWAKE(victim) || (hit_chance >= number(1, 100));

				if (!success && !MOB_FLAGGED(victim, MOB_ANIMAL)) {
					act("You lunge at $N, but $E dodges you!", FALSE, ch, 0, victim, TO_CHAR);
					act("$n lunges at $N, but $E dodges it!", FALSE, ch, 0, victim, TO_NOTVICT);
					act("$n lunges at you, but you dodge $m!", FALSE, ch, 0, victim, TO_VICT);
					WAIT_STATE(ch, universal_wait);
					if (!FIGHTING(victim)) {
						hit(victim, ch, GET_EQ(victim, WEAR_WIELD), FALSE);
					}
					return;
				}
			}

			/* All-clear */
			GET_FEEDING_FROM(ch) = victim;
			GET_FED_ON_BY(victim) = ch;
		
			stop_fighting(ch);
			stop_fighting(victim);

			if (!IS_NPC(victim) && PRF_FLAGGED(victim, PRF_BOTHERABLE)) {
				act("You grasp $N's wrist and bite into it.", FALSE, ch, 0, victim, TO_CHAR);
				act("$n grasps $N's wrist and bites into it.", FALSE, ch, 0, victim, TO_NOTVICT);
				act("$n grasps your wrist and bites into it.", FALSE, ch, 0, victim, TO_VICT);
			}
			else {
				act("You lunge at $N, grasping onto $S neck and biting into it vigorously!", FALSE, ch, 0, victim, TO_CHAR);
				act("$n lunges at $N, grasping onto $S neck and biting into it vigorously!", FALSE, ch, 0, victim, TO_NOTVICT);
				act("$n lunges at you... $e grasps onto your neck and bites into it!", FALSE, ch, 0, victim, TO_VICT | TO_SLEEP);
			}
		}
	}
}


ACMD(do_bloodsword) {
	obj_data *obj;
	int cost = 20;
	int vnum;

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use bloodsword.\r\n");
		return;
	}
	if (!check_vampire_ability(ch, ABIL_BLOODSWORD, BLOOD, cost, NOTHING)) {
		return;
	}
	if (!check_vampire_sun(ch, TRUE)) {
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_BLOODSWORD)) {
		return;
	}
	
	// attempt to remove existing wield
	if (GET_EQ(ch, WEAR_WIELD)) {
		perform_remove(ch, WEAR_WIELD, FALSE, FALSE);
		
		// did it work? if not, player got an error
		if (GET_EQ(ch, WEAR_WIELD)) {
			return;
		}
	}
	
	// determine which version
	if (get_ability_level(ch, ABIL_BLOODSWORD) >= 100) {
		vnum = o_BLOODSWORD_LEGENDARY;
	}
	else if (get_ability_level(ch, ABIL_BLOODSWORD) >= 90) {
		vnum = o_BLOODSWORD_HIGH;
	}
	else if (get_ability_level(ch, ABIL_BLOODSWORD) >= 50) {
		vnum = o_BLOODSWORD_MEDIUM;
	}
	else {
		vnum = o_BLOODSWORD_LOW;
	}
	
	charge_ability_cost(ch, BLOOD, cost, NOTHING, 0);
	obj = read_object(vnum);
	
	act("You drain blood from your wrist and mold it into $p.", FALSE, ch, obj, NULL, TO_CHAR);
	act("$n twists and molds $s own blood into $p.", TRUE, ch, obj, NULL, TO_ROOM);
	
	equip_char(ch, obj, WEAR_WIELD);
	
	gain_ability_exp(ch, ABIL_BLOODSWORD, 20);
	load_otrigger(obj);
}


ACMD(do_boost) {
	struct affected_type *af;
	int cost = 10;

	one_argument(argument, arg);

	if (!check_vampire_ability(ch, ABIL_BOOST, BLOOD, cost, NOTHING)) {
		return;
	}
	else if (!check_vampire_sun(ch, TRUE)) {
		return;
	}
	else if (!*arg) {
		msg_to_char(ch, "Which attribute do you wish to boost (strength or dexterity), or 'end' to cancel boosts?\r\n");
	}
	
	else if (is_abbrev(arg, "end")) {
		if (affected_by_spell(ch, ATYPE_BOOST)) {
			end_boost(ch);
		}
		else {
			msg_to_char(ch, "Your attributes aren't boosted!\r\n");
		}
	}
	
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_BOOST)) {
		return;
	}

	/* Strength */
	else if (is_abbrev(arg, "strength")) {
		if (GET_STRENGTH(ch) >= att_max(ch)) {
			msg_to_char(ch, "Your strength is already at maximum!\r\n");
		}
		else if (affected_by_spell_and_apply(ch, ATYPE_BOOST, APPLY_STRENGTH)) {
			msg_to_char(ch, "Your strength is already boosted!\r\n");
		}
		else {
			af = create_mod_aff(ATYPE_BOOST, 3 MUD_HOURS, APPLY_STRENGTH, 1 + (skill_check(ch, ABIL_BOOST, DIFF_HARD) ? 1 : 0));
			affect_join(ch, af, AVG_DURATION | ADD_MODIFIER);

			charge_ability_cost(ch, BLOOD, cost, NOTHING, 0);

			msg_to_char(ch, "You force blood into your muscles, boosting your strength!\r\n");
			gain_ability_exp(ch, ABIL_BOOST, 20);
		}
	}

	/* Dexterity */
	else if (is_abbrev(arg, "dexterity")) {
		if (GET_DEXTERITY(ch) >= att_max(ch)) {
			msg_to_char(ch, "Your dexterity is already at maximum!\r\n");
		}
		else if (affected_by_spell_and_apply(ch, ATYPE_BOOST, APPLY_DEXTERITY)) {
			msg_to_char(ch, "Your dexterity is already boosted!\r\n");
		}
		else {
			af = create_mod_aff(ATYPE_BOOST, 3 MUD_HOURS, APPLY_DEXTERITY, 1 + (skill_check(ch, ABIL_BOOST, DIFF_HARD) ? 1 : 0));
			affect_join(ch, af, AVG_DURATION | ADD_MODIFIER);

			charge_ability_cost(ch, BLOOD, cost, NOTHING, 0);

			msg_to_char(ch, "You focus your blood, increasing your dexterity!\r\n");
			gain_ability_exp(ch, ABIL_BOOST, 20);
		}
	}
	else {
		msg_to_char(ch, "Would you like to increase your strength or dexterity?\r\n");
	}
}


ACMD(do_claws) {
	struct affected_type *af;
	int cost = 10;
	
	if (affected_by_spell(ch, ATYPE_CLAWS)) {
		retract_claws(ch);
		WAIT_STATE(ch, universal_wait);
		return;
	}
	
	if (!check_vampire_ability(ch, ABIL_CLAWS, BLOOD, cost, NOTHING)) {
		return;
	}
	if (!check_vampire_sun(ch, TRUE)) {
		return;
	}
	if (MORPH_FLAGGED(ch, MORPH_FLAG_NO_CLAWS)) {
		msg_to_char(ch, "You can't grow claws in this form!\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_CLAWS)) {
		return;
	}

	// attempt to remove existing wield
	if (GET_EQ(ch, WEAR_WIELD)) {
		perform_remove(ch, WEAR_WIELD, FALSE, FALSE);
		
		// did it work? if not, player got an error
		if (GET_EQ(ch, WEAR_WIELD)) {
			return;
		}
	}

	msg_to_char(ch, "You focus your blood into your hands...\r\n");
	msg_to_char(ch, "Your fingers grow into grotesque claws!\r\n");
	act("$n's fingers grow into giant claws!", TRUE, ch, 0, 0, TO_ROOM);
	
	af = create_flag_aff(ATYPE_CLAWS, UNLIMITED, AFF_CLAWS);
	affect_join(ch, af, 0);

	charge_ability_cost(ch, BLOOD, cost, NOTHING, 0);
	gain_ability_exp(ch, ABIL_CLAWS, 20);
}


ACMD(do_command) {
	ACMD(do_say);
	char_data *victim;
	char *to_do;
	bool un_charm;
	
	int level_threshold = 25;

	argument = two_arguments(argument, arg, buf);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use Command.\r\n");
	}
	else if (!check_vampire_ability(ch, ABIL_COMMAND, NOTHING, 0, NOTHING)) {
		return;
	}
	else if (!check_vampire_sun(ch, TRUE)) {
		return;
	}
	else if (!*arg || !*buf)
		msg_to_char(ch, "Force whom to do what command?\r\n");
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (ch == victim)
		msg_to_char(ch, "That seems rather pointless, doesn't it?\r\n");
	else if (!IS_NPC(victim)) {
		msg_to_char(ch, "You can only command NPCs.\r\n");
	}
	else if (!MOB_FLAGGED(victim, MOB_HUMAN))
		msg_to_char(ch, "You can only give commands to humans.\r\n");
	else if (MOB_FLAGGED(victim, MOB_HARD | MOB_GROUP)) {
		act("You can't command $N.", FALSE, ch, NULL, victim, TO_CHAR);
	}
	else if (IS_VAMPIRE(victim) && (IS_NPC(victim) || GET_SKILL(victim, SKILL_VAMPIRE) > GET_SKILL(ch, SKILL_VAMPIRE)))
		msg_to_char(ch, "You cannot force your will upon those of more powerful blood.\r\n");
	else if (get_approximate_level(ch) - get_approximate_level(victim) < level_threshold) {
		msg_to_char(ch, "Your victim is too powerful.\r\n");
	}
	else if (!AWAKE(victim))
		msg_to_char(ch, "Your victim will need to be awake to understand you.\r\n");
	else if (*argument && !str_str(argument, buf))
		msg_to_char(ch, "If you wish to include your orders in a sentence, you must include the word in that sentence.\r\nFormat: command <target> <command> [sentence including command]\r\n");
	else if (!CAN_SEE(victim, ch))
		act("How do you intend to command $M if $E can't even see you?", FALSE, ch, 0, victim, TO_CHAR);
	else if (ABILITY_TRIGGERS(ch, victim, NULL, ABIL_COMMAND)) {
		return;
	}
	else {
		/* do_say corrodes buf */
		to_do = str_dup(buf);

		/* The command is spoken.. */
		if (!*argument)
			strcpy(argument, buf);
		do_say(ch, argument, 0, 0);
		
		gain_ability_exp(ch, ABIL_COMMAND, 33.4);

		if (skill_check(ch, ABIL_COMMAND, DIFF_MEDIUM) && !AFF_FLAGGED(victim, AFF_IMMUNE_VAMPIRE)) {
			un_charm = AFF_FLAGGED(victim, AFF_CHARM) ? FALSE : TRUE;
			SET_BIT(AFF_FLAGS(victim), AFF_CHARM);
			
			// do
			command_interpreter(victim, to_do);
			
			if (un_charm && !EXTRACTED(victim)) {
				REMOVE_BIT(AFF_FLAGS(victim), AFF_CHARM);
			}
		}
		
		WAIT_STATE(ch, universal_wait);
	}
}


ACMD(do_deathshroud) {
	struct affected_type *af;
	int cost = 20;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot deathshroud.\r\n");
	}
	else if (GET_POS(ch) < POS_SLEEPING) {
		msg_to_char(ch, "You can't do that right now.\r\n");
	}
	else if (affected_by_spell(ch, ATYPE_DEATHSHROUD)) {
		un_deathshroud(ch);
		WAIT_STATE(ch, universal_wait);
	}
	else if (!check_vampire_ability(ch, ABIL_DEATHSHROUD, BLOOD, cost, NOTHING)) {
		return;
	}
	else if (!check_vampire_sun(ch, TRUE)) {
		return;
	}
	else if (FIGHTING(ch) || GET_POS(ch) < POS_RESTING) {
		msg_to_char(ch, "You can't do that right now.\r\n");
	}
	else if (GET_POS(ch) == POS_FIGHTING)
		msg_to_char(ch, "You can't use deathshroud while fighting!\r\n");
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_DEATHSHROUD)) {
		return;
	}
	else {
		msg_to_char(ch, "You fall to the ground, dead!\r\n");
		act("$n falls to the ground, dead!", TRUE, ch, 0, 0, TO_ROOM);

		af = create_flag_aff(ATYPE_DEATHSHROUD, UNLIMITED, AFF_DEATHSHROUD);
		affect_join(ch, af, 0);

		GET_POS(ch) = POS_SLEEPING;
		charge_ability_cost(ch, BLOOD, cost, NOTHING, 0);
		gain_ability_exp(ch, ABIL_DEATHSHROUD, 50);
	}
}


ACMD(do_feed) {
	char_data *victim;
	int amt = 0;

	two_arguments(argument, arg, buf);

	if (!IS_VAMPIRE(ch)) {
		msg_to_char(ch, "You need to be a vampire to feed blood to others.\r\n");
		return;
	}
	else if (!*arg || !*buf)
		msg_to_char(ch, "Feed whom how much blood?\r\n");
	else if ((amt = atoi(buf)) <= 0)
		msg_to_char(ch, "Feed how much blood?\r\n");
	else if (amt > GET_BLOOD(ch) - 1)
		msg_to_char(ch, "You can't give THAT much blood.\r\n");
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (IS_NPC(victim) || !PRF_FLAGGED(victim, PRF_BOTHERABLE))
		act("$E refuses your vitae.", FALSE, ch, 0, victim, TO_CHAR);
	else {
		act("You slice your wrist open and feed $N some blood!", FALSE, ch, 0, victim, TO_CHAR);
		act("$n slices $s wrist open and feeds $N some blood!", TRUE, ch, 0, victim, TO_NOTVICT);
		act("$n slices $s wrist open and feeds you some blood from the cut!", FALSE, ch, 0, victim, TO_VICT);

		// mve the blood
		GET_BLOOD(ch) -= amt;
		GET_BLOOD(victim) = MIN(GET_MAX_BLOOD(victim), GET_BLOOD(victim) + amt);
	}
}


ACMD(do_majesty) {
	struct affected_type *af;
	
	if (affected_by_spell(ch, ATYPE_MAJESTY)) {
		end_majesty(ch);
	}
	else if (!check_vampire_ability(ch, ABIL_MAJESTY, NOTHING, 0, NOTHING)) {
		return;
	}
	else if (!check_vampire_sun(ch, TRUE)) {
		return;
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_MAJESTY)) {
		return;
	}
	else {
		msg_to_char(ch, "You create a sense of supernatural majesty about yourself.\r\n");
		act("$n glows majestically.", TRUE, ch, 0, 0, TO_ROOM);

		af = create_flag_aff(ATYPE_MAJESTY, UNLIMITED, AFF_MAJESTY);
		affect_join(ch, af, 0);
	}
	
	WAIT_STATE(ch, universal_wait);
}


ACMD(do_mummify) {
	int cost = 20;
	struct affected_type *af;

	if (GET_POS(ch) < POS_SLEEPING) {
		msg_to_char(ch, "You can't do that right now.\r\n");
	}
	else if (affected_by_spell(ch, ATYPE_MUMMIFY)) {
		un_mummify(ch);
		WAIT_STATE(ch, universal_wait);
	}
	else if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot mummify.\r\n");
	}
	else if (!check_vampire_ability(ch, ABIL_MUMMIFY, BLOOD, cost, NOTHING)) {
		return;
	}
	else if (!check_vampire_sun(ch, TRUE)) {
		return;
	}
	else if (FIGHTING(ch) || GET_POS(ch) < POS_RESTING) {
		msg_to_char(ch, "You can't do that right now.\r\n");
	}
	else if (GET_POS(ch) == POS_FIGHTING)
		msg_to_char(ch, "You can't mummify yourself while fighting!\r\n");
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_MUMMIFY)) {
		return;
	}
	else {
		msg_to_char(ch, "Your flesh hardens as you mummify yourself!\r\n");
		act("$n's flesh hardens and $e falls to the ground!", TRUE, ch, 0, 0, TO_ROOM);
		GET_POS(ch) = POS_SLEEPING;
		charge_ability_cost(ch, BLOOD, cost, NOTHING, 0);

		af = create_aff(ATYPE_MUMMIFY, -1, APPLY_NONE, 0, AFF_IMMUNE_PHYSICAL | AFF_MUMMIFY | AFF_NO_ATTACK);
		affect_join(ch, af, 0);
		
		gain_ability_exp(ch, ABIL_MUMMIFY, 50);
	}
}


ACMD(do_regenerate) {
	#define REGEN_HEALTH	0
	#define REGEN_MANA		1
	#define REGEN_MOVE		2
	
	#define REGEN_PER_BLOOD_AT_50	1
	#define REGEN_PER_BLOOD_AT_75	1.5
	#define REGEN_PER_BLOOD_AT_100	2.5

	int cost = 10, mode, amount = -1;
	char arg2[MAX_INPUT_LENGTH];
	double per;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use regenerate.\r\n");
		return;
	}
	
	two_arguments(argument, arg, arg2);
	// allow args in either order
	if (is_number(arg)) {
		strcpy(buf, arg2);
		strcpy(arg2, arg);
		strcpy(arg, buf);
	}
	
	// basic checks
	
	if (!check_vampire_ability(ch, ABIL_REGENERATE, NOTHING, 0, NOTHING)) {
		return;
	}
	if (!check_vampire_sun(ch, TRUE)) {
		return;
	}
	if (!CAN_SPEND_BLOOD(ch)) {
		msg_to_char(ch, "Your blood is inert, you can't do that!\r\n");
		return;
	}
	// can't heal the dead
	if (IS_DEAD(ch)) {
		msg_to_char(ch, "You can't regenerate from death.\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_REGENERATE)) {
		return;
	}

	// detect type
	if (!*arg || is_abbrev(arg, "health") || is_abbrev(arg, "hitpoints")) {
		if (GET_HEALTH(ch) >= GET_MAX_HEALTH(ch)) {
			msg_to_char(ch, "You don't seem to be injured.\r\n");
			return;
		}
		
		mode = REGEN_HEALTH;
	}
	else if (is_abbrev(arg, "mana") || is_abbrev(arg, "magic")) {
		if (GET_MANA(ch) >= GET_MAX_MANA(ch)) {
			msg_to_char(ch, "You don't seem to need mana.\r\n");
			return;
		}
		
		mode = REGEN_MANA;
	}
	else if (is_abbrev(arg, "moves") || is_abbrev(arg, "stamina") || is_abbrev(arg, "movement")) {
		if (GET_MOVE(ch) >= GET_MAX_MOVE(ch)) {
			msg_to_char(ch, "You don't seem to need movement points.\r\n");
			return;
		}
		
		mode = REGEN_MOVE;
	}
	else {
		msg_to_char(ch, "Regenerate health, moves, or mana?\r\n");
		return;
	}

	// determine amount/cost
	if (*arg2 && (amount = atoi(arg2)) <= 0) {
		// don't save amount in this case; detect it
		amount = -1;
	}

	// determine actual amount to heal
	per = get_ability_level(ch, ABIL_REGENERATE) <= 50 ? REGEN_PER_BLOOD_AT_50 : (get_ability_level(ch, ABIL_REGENERATE) <= 75 ? REGEN_PER_BLOOD_AT_75 : REGEN_PER_BLOOD_AT_100);
	if (amount == -1) {
		// default value -- spend 10 blood
		amount = per * 10;
	}
	else {
		amount = per * ((amount / per) + 1);
	}
	
	// limit on how much they needi
	switch (mode) {
		case REGEN_HEALTH: {
			amount = MIN(amount, GET_MAX_HEALTH(ch) - GET_HEALTH(ch));
			break;
		}
		case REGEN_MOVE: {
			amount = MIN(amount, GET_MAX_MOVE(ch) - GET_MOVE(ch));
			break;
		}
		case REGEN_MANA: {
			amount = MIN(amount, GET_MAX_MANA(ch) - GET_MANA(ch));
			break;
		}
	}
	
	// final cost
	cost = amount / per;

	if (GET_BLOOD(ch) < cost + 1) {
		msg_to_char(ch, "You need %d blood to regenerate.\r\n", cost);
		return;
	}
	
	charge_ability_cost(ch, BLOOD, cost, NOTHING, 0);
	
	if (skill_check(ch, ABIL_REGENERATE, DIFF_EASY)) {
		switch (mode) {
			case REGEN_HEALTH: {
				msg_to_char(ch, "You focus your blood into regeneration.\r\n");
				act("$n's wounds seem to seal themselves.", TRUE, ch, NULL, NULL, TO_ROOM);
				heal(ch, ch, amount);
				break;
			}
			case REGEN_MANA: {
				msg_to_char(ch, "You draw out the mystical energy from your blood.\r\n");
				act("$n's skin flushes red.", TRUE, ch, NULL, NULL, TO_ROOM);
				GET_MANA(ch) = MIN(GET_MAX_MANA(ch), GET_MANA(ch) + amount);
				break;
			}
			case REGEN_MOVE: {
				msg_to_char(ch, "You focus your blood into your sore muscles.\r\n");
				act("$n seems envigorated.", TRUE, ch, NULL, NULL, TO_ROOM);
				GET_MOVE(ch) = MIN(GET_MAX_MOVE(ch), GET_MOVE(ch) + amount);
				break;
			}
		}
	}
	else {
		msg_to_char(ch, "You focus your blood but fail to regenerate yourself.\r\n");
	}
	
	gain_ability_exp(ch, ABIL_REGENERATE, 15);
	WAIT_STATE(ch, universal_wait);
}


ACMD(do_sire) {
	char_data *victim;

	one_argument(argument, arg);

	if (!IS_VAMPIRE(ch)) {
		send_config_msg(ch, "must_be_vampire");
	}
	else if (IS_NPC(ch)) {
		msg_to_char(ch, "Nope.\r\n");
	}
	else if (!*arg)
		msg_to_char(ch, "Sire whom?\r\n");
	else if (GET_ACTION(ch) != ACT_NONE)
		msg_to_char(ch, "You're too busy to do that.\r\n");
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (IS_NPC(victim))
		msg_to_char(ch, "You can't sire an NPC.\r\n");
	else if (IS_VAMPIRE(victim))
		msg_to_char(ch, "You can't sire a vampire!\r\n");
	else if (IS_GOD(victim) || IS_IMMORTAL(victim))
		msg_to_char(ch, "You can't sire a deity!\r\n");
	else {
		sprintf(buf, "%s", GET_NAME(victim));
		do_bite(ch, buf, 0, 1);
		
		if (GET_FEEDING_FROM(ch)) {
			start_action(ch, ACT_SIRING, -1, 0);
		}
	}
}


ACMD(do_soulmask) {
	struct affected_type *af;

	if (affected_by_spell(ch, ATYPE_SOULMASK)) {
		msg_to_char(ch, "You turn off your soulmask.\r\n");
		affect_from_char(ch, ATYPE_SOULMASK);
		WAIT_STATE(ch, universal_wait);
		return;
	}
	else if (!check_vampire_ability(ch, ABIL_SOULMASK, NOTHING, 0, NOTHING)) {
		return;
	}
	else if (!check_vampire_sun(ch, TRUE)) {
		return;
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_SOULMASK)) {
		return;
	}
	else {
		if (skill_check(ch, ABIL_SOULMASK, DIFF_EASY)) {
			af = create_flag_aff(ATYPE_SOULMASK, UNLIMITED, AFF_SOULMASK);
			affect_join(ch, af, 0);
		}
		msg_to_char(ch, "You conceal your magical aura!\r\n");
		
		gain_ability_exp(ch, ABIL_SOULMASK, 33.4);
		WAIT_STATE(ch, universal_wait);
	}
}


ACMD(do_veintap) {
	obj_data *container;
	int amt = 0;

	argument = two_arguments(argument, buf, buf1);

	/*
	 * buf = amount
	 * buf1 = countainer
	 */

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot veintap.\r\n");
	}
	else if (!check_vampire_ability(ch, ABIL_VEINTAP, NOTHING, 0, NOTHING)) {
		return;
	}
	else if (!*buf || !*buf1)
		msg_to_char(ch, "Usage: veintap <amount> <container>\r\n");
	else if (!IS_VAMPIRE(ch))
		msg_to_char(ch, "Only vampire blood may be stored with blood essence.\r\n");
	else if ((amt = atoi(buf)) <= 0)
		msg_to_char(ch, "Drain how much blood?\r\n");
	else if (GET_BLOOD(ch) < amt + 1)
		msg_to_char(ch, "You can't drain THAT much!\r\n");
	else if (!(container = get_obj_in_list_vis(ch, buf1, ch->carrying)))
		msg_to_char(ch, "You don't seem to have a %s.\r\n", buf1);
	else if (GET_OBJ_TYPE(container) != ITEM_DRINKCON)
		msg_to_char(ch, "You can't drain blood into that!\r\n");
	else if (GET_DRINK_CONTAINER_CONTENTS(container) > 0 && GET_DRINK_CONTAINER_TYPE(container) != LIQ_BLOOD)
		act("$p already contains something else!", FALSE, ch, container, 0, TO_CHAR);
	else if (GET_DRINK_CONTAINER_CONTENTS(container) >= GET_DRINK_CONTAINER_CAPACITY(container))
		act("$p is already full.", FALSE, ch, container, 0, TO_CHAR);
	else if (ABILITY_TRIGGERS(ch, NULL, container, ABIL_VEINTAP)) {
		return;
	}
	else {
		act("You drain some of your blood into $p!", FALSE, ch, container, 0, TO_CHAR);
		act("$n drains some of $s blood into $p!", TRUE, ch, container, 0, TO_ROOM);

		amt = MIN(amt, GET_DRINK_CONTAINER_CAPACITY(container) - GET_DRINK_CONTAINER_CONTENTS(container));

		charge_ability_cost(ch, BLOOD, amt, NOTHING, 0);
		GET_OBJ_VAL(container, VAL_DRINK_CONTAINER_CONTENTS) += amt;
		GET_OBJ_VAL(container, VAL_DRINK_CONTAINER_TYPE) = LIQ_BLOOD;
		
		gain_ability_exp(ch, ABIL_VEINTAP, 33.4);
	}
}


ACMD(do_weaken) {
	struct affected_type *af;
	char_data *victim;
	int cost = 50;

	one_argument(argument, arg);

	if (!check_vampire_ability(ch, ABIL_WEAKEN, BLOOD, cost, COOLDOWN_WEAKEN)) {
		return;
	}
	else if (!check_vampire_sun(ch, TRUE)) {
		return;
	}
	else if (!*arg && !FIGHTING(ch))
		msg_to_char(ch, "Inflict weakness upon whom?\r\n");
	else if (!(victim = get_char_vis(ch, arg, FIND_CHAR_ROOM)) && !(victim = FIGHTING(ch)))
		send_config_msg(ch, "no_person");
	else if (ch == victim)
		msg_to_char(ch, "You wouldn't want to do that to yourself...\r\n");
	else if (IS_GOD(victim) || IS_IMMORTAL(victim))
		msg_to_char(ch, "You cannot use this power on so godly a target!\r\n");
	else if (affected_by_spell(victim, ATYPE_WEAKEN) || GET_STRENGTH(victim) <= 1)
		act("$E is already weak!", FALSE, ch, 0, victim, TO_CHAR);
	else if (!can_fight(ch, victim))
		act("You can't attack $M!", FALSE, ch, 0, victim, TO_CHAR);
	else if (ABILITY_TRIGGERS(ch, victim, NULL, ABIL_WEAKEN)) {
		return;
	}
	else {
		if (SHOULD_APPEAR(ch)) {
			appear(ch);
		}

		charge_ability_cost(ch, BLOOD, cost, COOLDOWN_WEAKEN, SECS_PER_MUD_HOUR);

		act("You spread your dead blood on $N...", FALSE, ch, 0, victim, TO_CHAR);
		act("$n spreads $s cold, dead blood on you...", FALSE, ch, 0, victim, TO_VICT);
		act("$n spreads some of $s blood on $N...", TRUE, ch, 0, victim, TO_NOTVICT);

		if (!AFF_FLAGGED(victim, AFF_IMMUNE_VAMPIRE)) {
			af = create_mod_aff(ATYPE_WEAKEN, 1 MUD_HOURS, APPLY_STRENGTH, -1 * MIN(2, GET_STRENGTH(victim)-1));
			affect_join(victim, af, 0);
			msg_to_char(victim, "You feel weak!\r\n");
			act("$n hunches over in pain!", TRUE, victim, 0, 0, TO_ROOM);
		}
		
		engage_combat(ch, victim, TRUE);

		gain_ability_exp(ch, ABIL_WEAKEN, 15);
	}
}
