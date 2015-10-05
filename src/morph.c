/* ************************************************************************
*   File: morph.c                                         EmpireMUD 2.0b2 *
*  Usage: morph-related code                                              *
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
#include "db.h"
#include "skills.h"
#include "handler.h"
#include "interpreter.h"
#include "dg_scripts.h"

/**
* Contents:
*   Data
*   Helpers
*   Commands
*/


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

// validates morphs
#define MORPH_OK(ch, morph)  ( \
	(morph_data[(morph)].ability == NO_ABIL || HAS_ABILITY((ch), morph_data[(morph)].ability)) && \
	(!IS_SET(morph_data[(morph)].morph_flags, MORPH_FLAG_VAMPIRE_ONLY) || IS_VAMPIRE(ch)) \
)


/* The main data structure */
struct morph_data_structure {
	char *name;	// for "morph <form>" -- "\n" to not be able to manually morph to this form
	int ability;	// required ability, or NO_ABIL for none
	int normal_cost[NUM_POOLS];	// cost when out of combat (slow morph)
	int combat_cost[NUM_POOLS];	// cost when in combat
	
	byte attribute_bonus[NUM_ATTRIBUTES];	// attributes +/- this number
	int pool_mod[NUM_POOLS];	// health (etc) pools modified by this percent
	
	int attack_type, aff_bits;	// TYPE_x, AFF_x
	bitvector_t morph_flags;	// MORPH_FLAG_x
	char *short_desc;	// "a black bat"
	char *extra_desc;	// the description for "look <person>"
};

const struct morph_data_structure morph_data[] = {
	/* Notes on adding forms:
	 *  1. add a MORPH_x definition for your morph
	 *  2. add an entry to this table (the extra desc should NOT end in \r\n)
	 *  3. find a place to cause the morph
	 * It's that easy.
	 */

	// normal form (no morph -- used mainly for morphing back)
	{ "normal", NO_ABIL,	// morph command arg, ability required
		{ 0, 0, 0, 0 },	// cost when not in combat (health, move, mana, blood)
		{ 0, 0, 0, 0 },	// cost when fighting (health, move, mana, blood)
		{ 0, 0, 0, 0, 0, 0 },	// attribute mods (+/- this number)
		{ 100, 100, 100, 100 },	// health/move/mana/blood pools modified by this percent
		TYPE_HIT, 0,	// attack type, aff flags
		0,	// morph flags
		"",	// short desc
		""	// extra desc
	},

	// bat form
	{ "bat", ABIL_BAT_FORM,
		{ 0, 0, 0, 50 },
		{ 0, 0, 0, 100 },
		{ -2, 0, 0, 0, 0, 0 },
		{ 25, 200, 100, 100 },
		TYPE_BITE, AFF_FLY,
		MORPH_FLAG_ANIMAL | MORPH_FLAG_NO_CLAWS | MORPH_FLAG_VAMPIRE_ONLY,
		"a black bat",
		"   The bat is small and awkward as it flaps its wings around. It flitters\r\naround you menacingly, but keeps its distance. Though its size is not\r\nthreatening, the bat seems somehow capable of more."
	},

	// wolf form
	{ "wolf", ABIL_WOLF_FORM,
		{ 0, 0, 0, 20 },
		{ 0, 0, 0, 40 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 50, 125, 100, 100 },
		TYPE_CLAW, 0,
		MORPH_FLAG_ANIMAL | MORPH_FLAG_VAMPIRE_ONLY,
		"a brown wolf",
		"   As you look at the wolf, you first see $s cold, black eyes which stare you\r\ndown harshly. Larger than the other wolves which roam the plains, $s this\r\ncreature frightens you deeply. Wolves of this stature are known to be\r\nvicious, and you feel like running."
	},

	// horrid form
	{ "horrid form", ABIL_HORRID_FORM,
		{ 0, 0, 0, 30 },
		{ 0, 0, 0, 60 },
		{ 2, 0, 0, 0, 0, 0 },
		{ 150, 150, 25, 100 },
		TYPE_CLAW, 0,
		MORPH_FLAG_VAMPIRE_ONLY,
		"a horrid monstrosity",
		"   The horrid monstrosity is huge! At well over eight feet tall, with greenish-\r\ngray flesh, it's like nothing you've ever seen! Its arms are long, thin, and\r\nedged black nails. Its face is something out of a nightmare. A row of spines\r\nrun down its back, and bony plates protect its skin!"
	},

	// dread blood form
	{ "dread blood", ABIL_DREAD_BLOOD_FORM,
		{ 0, 0, 0, 30 },
		{ 0, 0, 0, 60 },
		{ 0, 0, 0, 0, 2, 0 },
		{ 50, 100, 150, 100 },
		TYPE_HIT, 0,
		MORPH_FLAG_VAMPIRE_ONLY,
		"the dread blood",
		"   What stands before you is a twisting, swirling, oozing mass of red blood.\r\nIts shape resembles a man, but its fanged, gaping maw is too large. Its limbs\r\nare too long. And when you look upon it, you're barely certain what's real and\r\nwhat exists only in your mind."
	},

	// mist form
	{ "mist", ABIL_MIST_FORM,
		{ 0, 0, 0, 20 },
		{ 0, 0, 0, 40 },
		{ -5, 0, 0, 0, 0, 0 },
		{ 10, 200, 100, 100 },
		TYPE_UNDEFINED, AFF_NO_ATTACK | AFF_IMMUNE_PHYSICAL,
		MORPH_FLAG_ANIMAL | MORPH_FLAG_VAMPIRE_ONLY,
		"a cloud of swirling mist",
		"   The mist swirls around, periodically taking form and then swirling away\r\nagain. It floats around you, chilling your flesh, then covers the ground,\r\nfeeling the terrain. It swirls up again, and again seems to take some shape."
	},

	// savage werewolf form
	{ "savage werewolf", ABIL_SAVAGE_WEREWOLF_FORM,
		{ 0, 50, 0, 0 },
		{ 0, 100, 0, 0 },
		{ 2, 0, 0, 0, 0, 0 },
		{ 100, 200, 25, 100 },
		TYPE_CLAW, 0,
		0,
		"a savage werewolf",
		"   The werewolf stands over you, snarling, baring every one of its long\r\ncanine teeth. It stands on two legs, more like a man than a wolf, and yet\r\nthere is something disturbingly bestial about it. You can't bring yourself\r\nto stop looking at it, for fear that turning away would spell your doom."
	},

	// towering werewolf form
	{ "towering werewolf", ABIL_TOWERING_WEREWOLF_FORM,
		{ 0, 50, 0, 0 },
		{ 0, 100, 0, 0 },
		{ 1, 3, 0, 0, 0, -1 },
		{ 200, 125, 20, 100 },
		TYPE_CLAW, 0,
		0,
		"a towering werewolf",
		"   You can scarcely avoid the shadow of this massive wolf-man, which towers\r\nover you and everything else. Its massive, powerful muscles are etched with\r\nthe scars of dozens of battles -- or hundreds -- and you wonder how many\r\nchildren this creature has orphaned on its quest for blood."
	},

	// sage werewolf form
	{ "sage werewolf", ABIL_SAGE_WEREWOLF_FORM,
		{ 0, 50, 0, 0 },
		{ 0, 100, 0, 0 },
		{ 1, 0, 0, 0, 2, 0 },
		{ 50, 100, 150, 100 },
		TYPE_CLAW, 0,
		0,
		"a sage werewolf",
		"   Though you have seen other werewolves, this one looks somehow smaller and\r\ngentler. It isn't the size of an ox cart, and its arms are not muscled to the\r\npoint of straining its armor. Instead, it has a serene look about it, as if it\r\nis in tune with the nature around it."
	},

	// deer form
	{ "deer", ABIL_ANIMAL_FORMS,
		{ 0, 0, 25, 0 },
		{ 0, 0, 50, 0 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 50, 125, 100, 100 },
		TYPE_BITE, 0,
		MORPH_FLAG_ANIMAL | MORPH_FLAG_TEMPERATE_AFFINITY,
		"a deer",
		"   The deer eyes you warily, apparently unsure of whether it should hold\r\nperfectly still, or turn and bolt. Its big, brown eyes never seem to move, and\r\nyet you're certain they are trained upon you."
	},

	// ostrich form
	{ "ostrich", ABIL_ANIMAL_FORMS,
		{ 0, 0, 25, 0 },
		{ 0, 0, 50, 0 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 50, 125, 100, 100 },
		TYPE_BITE, 0,
		MORPH_FLAG_ANIMAL | MORPH_FLAG_ARID_AFFINITY,
		"an ostrich",
		"   The ostrich notices you looking at it, and stops moving. It slowly raises\r\nits head until its eyes are above yours, and then somehow widens its giant\r\neyes further. It raises the backs of its wings up, making its body appear to\r\ndouble in size, and then stands motionless until you pass."
	},

	// tapir form
	{ "tapir", ABIL_ANIMAL_FORMS,
		{ 0, 0, 25, 0 },
		{ 0, 0, 50, 0 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 50, 125, 100, 100 },
		TYPE_BITE, 0,
		MORPH_FLAG_ANIMAL | MORPH_FLAG_TROPICAL_AFFINITY,
		"a tapir",
		"   The tapir roots around at the ground, largely ignoring you, although you\r\ncan't help but notice its eyes never seem to look away from yours. In fact,\r\nthe more you watch the tapir, the more certain you are that it's actually\r\nwatching you."
	},

	// This must go last
	{ "\n", NO_ABIL,
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0 }, 
		{ 100, 100, 100, 100 },
		0, 0,
		0,
		"",
		""
	}
};


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
*
* @param room_data *location Where the morph affinity is happening.
* @param int morph Which MORPH_x the person is trying to morph into.
* @return bool TRUE if the morph passes the affinities check.
*/
bool morph_affinity_ok(room_data *location, int morph) {
	int climate = NOTHING;
	crop_data *cp;
	bool ok = TRUE;
	
	if (ROOM_SECT_FLAGGED(location, SECTF_HAS_CROP_DATA) && (cp = crop_proto(ROOM_CROP_TYPE(location)))) {
		climate = GET_CROP_CLIMATE(cp);
	}
	else {
		climate = GET_SECT_CLIMATE(SECT(location));
	}
	
	if (IS_SET(morph_data[(morph)].morph_flags, MORPH_FLAG_TEMPERATE_AFFINITY) && climate != CLIMATE_TEMPERATE) {
		ok = FALSE;
	}
	if (IS_SET(morph_data[(morph)].morph_flags, MORPH_FLAG_ARID_AFFINITY) && climate != CLIMATE_ARID) {
		ok = FALSE;
	}
	if (IS_SET(morph_data[(morph)].morph_flags, MORPH_FLAG_TROPICAL_AFFINITY) && climate != CLIMATE_TROPICAL) {
		ok = FALSE;
	}
	
	return ok;
}


/**
* This function gets the short description or extra description for a morphed
* player.
*
* @param char_data *ch The player character.
* @param byte type Either MORPH_STRING_NAME or MORPH_STRING_DESC
* @return char* The string.
*/
char *morph_string(char_data *ch, byte type) {
	if (!IS_NPC(ch) && GET_MORPH(ch) > MORPH_NONE && GET_MORPH(ch) < NUM_MORPHS) {
		if (type == MORPH_STRING_NAME) {
			return morph_data[(int) GET_MORPH(ch)].short_desc;
		}
		else {
			return morph_data[(int) GET_MORPH(ch)].extra_desc;
		}
	}

	/* Oops */
	if (type == MORPH_STRING_NAME) {
		return "a morphed creature";
	}
	else {
		return "There is no string provided for this morph.\r\n";
	}
}


/**
* @param char_data *ch The player to check
* @return int Any of the attack types (TYPE_x), defaulting to TYPE_HIT.
*/
int get_morph_attack_type(char_data *ch) {
	int type = TYPE_HIT;
	
	if (!IS_NPC(ch) && GET_MORPH(ch) > MORPH_NONE && GET_MORPH(ch) < NUM_MORPHS) {
		if (morph_data[(int) GET_MORPH(ch)].attack_type != TYPE_UNDEFINED) {
			type = morph_data[(int) GET_MORPH(ch)].attack_type;
		}
	}

	return type;
}


/**
* This checks whether a player's current morph form has any morph flags set.
*
* @param char_data *ch The player character to check.
* @param bitvector_t flag The flag(s) to check.
* @return TRUE if the character is morphed and has the morph flag.
*/
bool MORPH_FLAGGED(char_data *ch, bitvector_t flag) {
	bool flagged = FALSE;
	
	if (!IS_NPC(ch)) {
		if (GET_MORPH(ch) > MORPH_NONE && GET_MORPH(ch) < NUM_MORPHS) {
			flagged = IS_SET(morph_data[(int) GET_MORPH(ch)].morph_flags, flag) ? TRUE : FALSE;
		}
	}
	
	return flagged;
}


/**
* Morphs a player back to normal. This function takes just 1 arg so it can
* be passed e.g. as a function pointer in vampire blood upkeep.
*
* @param char_data *ch The morph-ed one.
*/
void end_morph(char_data *ch) {
	if (GET_MORPH(ch) != MORPH_NONE) {
		perform_morph(ch, MORPH_NONE);
	}
}


/* return the modifier to a certain APPLY_x */
int morph_modifier(char_data *ch, byte loc) {
	int mod = 0;
	double prc;
	
	switch (loc) {
		case APPLY_STRENGTH: {
			mod = morph_data[GET_MORPH(ch)].attribute_bonus[STRENGTH];
			break;
		}
		case APPLY_DEXTERITY: {
			mod = morph_data[GET_MORPH(ch)].attribute_bonus[DEXTERITY];
			break;
		}
		case APPLY_CHARISMA: {
			mod = morph_data[GET_MORPH(ch)].attribute_bonus[CHARISMA];
			break;
		}
		case APPLY_GREATNESS: {
			mod = morph_data[GET_MORPH(ch)].attribute_bonus[GREATNESS];
			break;
		}
		case APPLY_INTELLIGENCE: {
			mod = morph_data[GET_MORPH(ch)].attribute_bonus[INTELLIGENCE];
			break;
		}
		case APPLY_WITS: {
			mod = morph_data[GET_MORPH(ch)].attribute_bonus[WITS];
			break;
		}
		case APPLY_HEALTH: {
			prc = morph_data[GET_MORPH(ch)].pool_mod[HEALTH] / 100.0;
			mod = (GET_MAX_HEALTH(ch) * prc) - GET_MAX_HEALTH(ch);
			break;
		}
		case APPLY_MOVE: {
			prc = morph_data[GET_MORPH(ch)].pool_mod[MOVE] / 100.0;
			mod = (GET_MAX_MOVE(ch) * prc) - GET_MAX_MOVE(ch);
			break;
		}
		case APPLY_MANA: {
			prc = morph_data[GET_MORPH(ch)].pool_mod[MANA] / 100.0;
			mod = (GET_MAX_MANA(ch) * prc) - GET_MAX_MANA(ch);
			break;
		}
	}

	return mod;
}


/* Update the stats on a morph */
void update_morph_stats(char_data *ch, bool add) {
	if (!IS_NPC(ch) && GET_MORPH(ch) != MORPH_NONE) {
		if (morph_data[GET_MORPH(ch)].aff_bits) {
			affect_modify(ch, 0, 0, morph_data[GET_MORPH(ch)].aff_bits, add);
		}

		affect_modify(ch, APPLY_STRENGTH, morph_modifier(ch, APPLY_STRENGTH), 0, add);
		affect_modify(ch, APPLY_DEXTERITY, morph_modifier(ch, APPLY_DEXTERITY), 0, add);
		affect_modify(ch, APPLY_CHARISMA, morph_modifier(ch, APPLY_CHARISMA), 0, add);
		affect_modify(ch, APPLY_GREATNESS, morph_modifier(ch, APPLY_GREATNESS), 0, add);
		affect_modify(ch, APPLY_INTELLIGENCE, morph_modifier(ch, APPLY_INTELLIGENCE), 0, add);
		affect_modify(ch, APPLY_WITS, morph_modifier(ch, APPLY_WITS), 0, add);
		affect_modify(ch, APPLY_HEALTH, morph_modifier(ch, APPLY_HEALTH), 0, add);
		affect_modify(ch, APPLY_MOVE, morph_modifier(ch, APPLY_MOVE), 0, add);
		affect_modify(ch, APPLY_MANA, morph_modifier(ch, APPLY_MANA), 0, add);
	}
}


/* cause the morph to happen: universal thru this function to prevent errors */
void perform_morph(char_data *ch, ubyte form) {
	ACMD(do_dismount);
	double move_mod, health_mod, mana_mod;

	if (IS_NPC(ch)) {
		return;
	}

	if (IS_RIDING(ch) && form != MORPH_NONE) {
		do_dismount(ch, "", 0, 0);
	}

	// read current pools
	health_mod = (double) GET_HEALTH(ch) / GET_MAX_HEALTH(ch);
	move_mod = (double) GET_MOVE(ch) / GET_MAX_MOVE(ch);
	mana_mod = (double) GET_MANA(ch) / GET_MAX_MANA(ch);

	/* Remove his morph bonuses from his current form */
	update_morph_stats(ch, FALSE);

	/* Set the new form */
	GET_MORPH(ch) = form;

	/* Add new morph bonuses */
	update_morph_stats(ch, TRUE);
	
	// this fixes all the things
	affect_total(ch);

	// set new pools
	GET_HEALTH(ch) = (sh_int) (GET_MAX_HEALTH(ch) * health_mod);
	GET_MOVE(ch) = (sh_int) (GET_MAX_MOVE(ch) * move_mod);
	GET_MANA(ch) = (sh_int) (GET_MAX_MANA(ch) * mana_mod);

	/* check for claws */
	if (MORPH_FLAGGED(ch, MORPH_FLAG_NO_CLAWS) && AFF_FLAGGED(ch, AFF_CLAWS)) {
		affects_from_char_by_aff_flag(ch, AFF_CLAWS);
	}
}


/**
* This is called by the morphing action, as well as the instamorph in do_morph.
* It sends messages and finishes the morph.
*
* @param char_data *ch the morpher
* @param int morph_to any MORPH_x const
*/
void finish_morphing(char_data *ch, int morph_to) {
	void undisguise(char_data *ch);

	char lbuf[MAX_STRING_LENGTH];
	
	// can't be disguised while morphed
	if (IS_DISGUISED(ch) && morph_to != MORPH_NONE) {
		undisguise(ch);
	}
	
	sprintf(lbuf, "%s has become $n!", PERS(ch, ch, FALSE));

	perform_morph(ch, morph_to);

	act(lbuf, TRUE, ch, 0, 0, TO_ROOM);
	act("You have become $n!", FALSE, ch, 0, 0, TO_CHAR);
	
	if (morph_data[morph_to].ability != NOTHING) {
		gain_ability_exp(ch, morph_data[morph_to].ability, 33.4);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_morph) {
	extern bool check_vampire_sun(char_data *ch, bool message);
	
	int morph_to = NOTHING;
	int cost[NUM_POOLS];
	int iter;
	bool found, fighting;
	
	// safety first
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't morph.\r\n");
		return;
	}
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "You know the following morphs: ");
		found = FALSE;
		
		for (iter = 0; iter < NUM_MORPHS; ++iter) {
			if (*morph_data[iter].name != '\n' && MORPH_OK(ch, iter)) {
				msg_to_char(ch, "%s%s", (found ? ", " : ""), morph_data[iter].name);
				found = TRUE;
			}
		}
		
		msg_to_char(ch, "\r\n");
		return;
	}
	
	// find target morph
	for (iter = 0; iter < NUM_MORPHS && morph_to == NOTHING; ++iter) {
		// match name
		if (*morph_data[iter].name != '\n' && is_abbrev(arg, morph_data[iter].name)) {
			if (MORPH_OK(ch, iter)) {
				morph_to = iter;
			}
		}
	}
	
	if (morph_to == NOTHING) {
		msg_to_char(ch, "You don't know any such morph.\r\n");
		return;
	}

	// set up costs costs -- health and blood require 1 more than the morph cost, as you'd die from hitting 0
	fighting = (FIGHTING(ch) || GET_POS(ch) == POS_FIGHTING);
	for (iter = 0; iter < NUM_POOLS; ++iter) {
		cost[iter] = (fighting ? morph_data[morph_to].combat_cost[iter] : morph_data[morph_to].normal_cost[iter]);
	}
	
	if (IS_SET(morph_data[morph_to].morph_flags, MORPH_FLAG_VAMPIRE_ONLY) && !check_vampire_sun(ch, TRUE)) {
		// sends own sun message
		return;
	}
	else if (GET_MORPH(ch) == morph_to) {
		msg_to_char(ch, "You are already in that form.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're too busy to morph now!\r\n");
	}
	else if (!morph_affinity_ok(IN_ROOM(ch), morph_to)) {
		msg_to_char(ch, "You can't morph into that here.\r\n");
	}
	else if (cost[HEALTH] > 0 && GET_HEALTH(ch) < cost[HEALTH] + 1) {
		msg_to_char(ch, "You need %d health points to morph.\r\n", cost[HEALTH]);
	}
	else if (cost[MOVE] > 0 && GET_MOVE(ch) < cost[MOVE]) {
		msg_to_char(ch, "You need %d move points to morph.\r\n", cost[MOVE]);
	}
	else if (cost[MANA] > 0 && GET_MANA(ch) < cost[MANA]) {
		msg_to_char(ch, "You need %d mana points to morph.\r\n", cost[MANA]);
	}
	else if (cost[BLOOD] > 0 && GET_BLOOD(ch) < cost[BLOOD] + 1) {
		msg_to_char(ch, "You need %d blood points to morph.\r\n", cost[BLOOD]);
	}
	else if (cost[BLOOD] > 0 && !CAN_SPEND_BLOOD(ch)) {
		msg_to_char(ch, "Your blood is inert, you can't do that!\r\n");
	}
	else if (morph_data[morph_to].ability != NO_ABIL && ABILITY_TRIGGERS(ch, NULL, NULL, morph_data[morph_to].ability)) {
		return;
	}
	else {
		// charge costs
		for (iter = 0; iter < NUM_POOLS; ++iter) {
			GET_CURRENT_POOL(ch, iter) -= cost[iter];
		}
		
		if (fighting) {
			// insta-morph!
			finish_morphing(ch, morph_to);
			command_lag(ch, WAIT_OTHER);
		}
		else {
			start_action(ch, ACT_MORPHING, config_get_int("morph_timer"));
			GET_ACTION_VNUM(ch, 0) = morph_to;
			msg_to_char(ch, "You begin to transform!\r\n");
		}
	}
}
