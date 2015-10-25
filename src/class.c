/* ************************************************************************
*   File: class.c                                         EmpireMUD 2.0b3 *
*  Usage: code related to classes                                         *
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

/**
* Contents:
*   Configs
*   Helpers
*   Commands
*/


 /////////////////////////////////////////////////////////////////////////////
//// CONFIGS ////////////////////////////////////////////////////////////////


#define ROLE_ABIL_END	-1
#define ROLE_LIST_END	{ ROLE_NONE, { ROLE_ABIL_END } }

// CLASS_x
const struct class_data_type class_data[NUM_CLASSES] = {
	// name, abbrev, { 2-skill combo },  health, move, mana, blood(unused)
	{ "Unclassed", "Uncl", { NO_SKILL, NO_SKILL },  { 50, 100, 50, 0 },
		{ ROLE_LIST_END }
	},
	{ "Duke", "Duke", { SKILL_BATTLE, SKILL_EMPIRE },  { 300, 200, 100, 0 },
		{
			{ ROLE_TANK, { ABIL_DRAGONRIDING, ABIL_NOBLE_BEARING, ROLE_ABIL_END } },
			{ ROLE_MELEE, { ABIL_DRAGONRIDING, ABIL_TWO_HANDED_WEAPONS, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_DRAGONRIDING, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Battlemage", "BtMg", { SKILL_BATTLE, SKILL_HIGH_SORCERY },  { 200, 200, 200, 0 },
		{ ROLE_LIST_END }
	},
	{ "Werewolf", "Wwlf", { SKILL_BATTLE, SKILL_NATURAL_MAGIC },  { 200, 200, 200, 0 },
		{
			{ ROLE_TANK, { ABIL_RESURRECT, ABIL_SPIRIT_WOLF, ABIL_TOWERING_WEREWOLF_FORM, ROLE_ABIL_END } },
			{ ROLE_MELEE, { ABIL_DIRE_WOLF, ABIL_RESURRECT, ABIL_SAVAGE_WEREWOLF_FORM, ROLE_ABIL_END } },
			{ ROLE_HEALER, { ABIL_CONFER, ABIL_FASTCASTING, ABIL_MOON_RABBIT, ABIL_RESURRECT, ABIL_SAGE_WEREWOLF_FORM, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Assassin", "Assn", { SKILL_BATTLE, SKILL_STEALTH },  { 200, 300, 100, 0 },
		{
			{ ROLE_MELEE, { ABIL_DUAL_WIELD, ABIL_SHADOW_KICK, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Barbarian", "Brbn", { SKILL_BATTLE, SKILL_SURVIVAL },  { 300, 200, 100, 0 },
		{
			{ ROLE_TANK, { ABIL_WARD_AGAINST_MAGIC, ROLE_ABIL_END } },
			{ ROLE_MELEE, { ABIL_TWO_HANDED_WEAPONS, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Steelsmith", "Smth", { SKILL_BATTLE, SKILL_TRADE },  { 300, 200, 100, 0 },
		{
			{ ROLE_MELEE, { ABIL_STEELSMITH_CRAFTS, ABIL_TWO_HANDED_WEAPONS, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_STEELSMITH_CRAFTS, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Reaper", "Reap", { SKILL_BATTLE, SKILL_VAMPIRE },  { 300, 200, 100, 0 },
		{
			{ ROLE_TANK, { ABIL_BLOOD_FORTITUDE, ABIL_BLOODSWEAT, ABIL_WARD_AGAINST_MAGIC, ROLE_ABIL_END } },
			{ ROLE_MELEE, { ABIL_HORRID_FORM, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Exarch", "Exrc", { SKILL_EMPIRE, SKILL_HIGH_SORCERY },  { 150, 150, 300, 0 },
		{
			{ ROLE_CASTER, { ABIL_DRAGONRIDING, ABIL_EXARCH_CRAFTS, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_DRAGONRIDING, ABIL_EXARCH_CRAFTS, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Luminary", "Lmny", { SKILL_EMPIRE, SKILL_NATURAL_MAGIC },  { 150, 150, 300, 0 },
		{
			{ ROLE_CASTER, { ABIL_DRAGONRIDING, ABIL_GRIFFIN, ABIL_MANTICORE, ABIL_RESURRECT, ROLE_ABIL_END } },
			{ ROLE_HEALER, { ABIL_ANCESTRAL_HEALING, ABIL_DRAGONRIDING, ABIL_FASTCASTING, ABIL_GRIFFIN, ABIL_MANTICORE, ABIL_RESURRECT, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_DRAGONRIDING, ABIL_GRIFFIN, ABIL_MANTICORE, ABIL_RESURRECT, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Powerbroker", "Pwbk", { SKILL_EMPIRE, SKILL_STEALTH },  { 200, 300, 100, 0 },
		{
			{ ROLE_MELEE, { ABIL_DRAGONRIDING, ABIL_STAGGER_JAB, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_DRAGONRIDING, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Elder", "Eldr", { SKILL_EMPIRE, SKILL_SURVIVAL },  { 200, 200, 200, 0 },
		{
			{ ROLE_MELEE, { ABIL_DRAGONRIDING, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_DRAGONRIDING, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Guildsman", "Gdsm", { SKILL_EMPIRE, SKILL_TRADE },  { 200, 200, 200, 0 },
		{
			{ ROLE_UTILITY, { ABIL_DRAGONRIDING, ABIL_GUILDSMAN_CRAFTS, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Ancient", "Anct", { SKILL_EMPIRE, SKILL_VAMPIRE },  { 200, 200, 200, 0 },
		{
			{ ROLE_MELEE, { ABIL_DRAGONRIDING, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_DRAGONRIDING, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Archmage", "Achm", { SKILL_HIGH_SORCERY, SKILL_NATURAL_MAGIC },  { 150, 150, 300, 0 },
		{
			{ ROLE_CASTER, { ABIL_RESURRECT, ROLE_ABIL_END } },
			{ ROLE_HEALER, { ABIL_FASTCASTING, ABIL_PHOENIX, ABIL_RESURRECT, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Occultist", "Oclt", { SKILL_HIGH_SORCERY, SKILL_STEALTH },  { 100, 250, 250, 0 },
		{
			{ ROLE_MELEE, { ABIL_SHADOWCAGE, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Theurge", "Thrg", { SKILL_HIGH_SORCERY, SKILL_SURVIVAL },  { 200, 100, 300, 0 },
		{
			{ ROLE_UTILITY, { ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Artificer", "Artf", { SKILL_HIGH_SORCERY, SKILL_TRADE },  { 150, 150, 300, 0 },
		{
			{ ROLE_CASTER, { ABIL_ARTIFICER_CRAFTS, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_ARTIFICER_CRAFTS, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Lich", "Lich", { SKILL_HIGH_SORCERY, SKILL_VAMPIRE },  { 150, 150, 300, 0 },
		{
			{ ROLE_CASTER, { ABIL_DREAD_BLOOD_FORM, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Shadow Wolf", "ShWf", { SKILL_NATURAL_MAGIC, SKILL_STEALTH },  { 100, 200, 300, 0 },
		{
			{ ROLE_MELEE, { ABIL_HOWL, ABIL_MOONRISE, ABIL_SAVAGE_WEREWOLF_FORM, ABIL_SCORPION_SHADOW, ROLE_ABIL_END } },
			{ ROLE_HEALER, { ABIL_FASTCASTING, ABIL_MOONRISE, ABIL_OWL_SHADOW, ABIL_SAGE_WEREWOLF_FORM, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Feral", "Ferl", { SKILL_NATURAL_MAGIC, SKILL_SURVIVAL },  { 150, 150, 300, 0 },
		{
			{ ROLE_TANK, { ABIL_BASILISK, ABIL_RESURRECT, ABIL_TOWERING_WEREWOLF_FORM, ROLE_ABIL_END } },
			{ ROLE_HEALER, { ABIL_BASILISK, ABIL_FASTCASTING, ABIL_RESURRECT, ABIL_SAGE_WEREWOLF_FORM, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Alchemist", "Alch", { SKILL_NATURAL_MAGIC, SKILL_TRADE },  { 150, 150, 300, 0 },
		{
			{ ROLE_HEALER, { ABIL_ALCHEMIST_CRAFTS, ABIL_FASTCASTING, ABIL_RESURRECT, ABIL_SALAMANDER, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_ALCHEMIST_CRAFTS, ABIL_RESURRECT, ABIL_SALAMANDER, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Necromancer", "Necr", { SKILL_NATURAL_MAGIC, SKILL_VAMPIRE },  { 150, 150, 300, 0 },
		{
			{ ROLE_CASTER, { ABIL_DREAD_BLOOD_FORM, ABIL_RESURRECT, ABIL_SKELETAL_HULK, ROLE_ABIL_END } },
			{ ROLE_HEALER, { ABIL_BANSHEE, ABIL_FASTCASTING, ABIL_RESURRECT, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Bandit", "Bndt", { SKILL_STEALTH, SKILL_SURVIVAL },  { 250, 250, 100, 0 },
		{
			{ ROLE_MELEE, { ABIL_CRUCIAL_JAB, ABIL_DUAL_WIELD, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Smuggler", "Smgl", { SKILL_STEALTH, SKILL_TRADE },  { 200, 300, 100, 0 },
		{
			{ ROLE_MELEE, { ABIL_DIVERSION, ABIL_DUAL_WIELD, ABIL_SMUGGLER_CRAFTS, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_SMUGGLER_CRAFTS, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Shade", "Shde", { SKILL_STEALTH, SKILL_VAMPIRE },  { 200, 300, 100, 0 },
		{
			{ ROLE_MELEE, { ABIL_HORRID_FORM, ABIL_SHADOW_JAB, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Tinker", "Tnkr", { SKILL_SURVIVAL, SKILL_TRADE },  { 250, 250, 100, 0 },
		{
			{ ROLE_MELEE, { ABIL_TINKER_CRAFTS, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_TINKER_CRAFTS, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Wight", "Wght", { SKILL_SURVIVAL, SKILL_VAMPIRE },  { 300, 200, 100, 0 },
		{
			{ ROLE_MELEE, { ABIL_HORRID_FORM, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	},
	{ "Antiquarian", "Antq", { SKILL_TRADE, SKILL_VAMPIRE },  { 200, 200, 200, 0 },
		{
			{ ROLE_MELEE, { ABIL_ANTIQUARIAN_CRAFTS, ROLE_ABIL_END } },
			{ ROLE_UTILITY, { ABIL_ANTIQUARIAN_CRAFTS, ROLE_ABIL_END } },
			ROLE_LIST_END
		}
	}
};


// ROLE_x (1/2): role names
const char *class_role[NUM_ROLES] = {
	"none",
	"Tank",
	"Melee",
	"Caster",
	"Healer",
	"Utility"
};


// ROLE_x (2/2): role colors for who list
const char *class_role_color[NUM_ROLES] = {
	"\t0",
	"\ty",	// tank
	"\tr",	// melee
	"\ta",	// caster
	"\tj",	// healer
	"\tm"	// utility
};


 /////////////////////////////////////////////////////////////////////////////
//// HELPERS ////////////////////////////////////////////////////////////////

/**
* Checks that the player has the correct set of class abilities, and removes
* any they shouldn't have. This function ignores immortals.
*
* @param char_data *ch The player to check.
* @param int class Any CLASS_X const, or NOTHING to detect from the player.
* @param int roole Any ROLE_X const, or NOTHING to detect from the player.
*/
void assign_class_abilities(char_data *ch, int class, int role) {
	void check_skill_sell(char_data *ch, int abil);
	
	int abil, role_iter, iter;
	bool has;

	// simple sanity
	if (IS_NPC(ch) || IS_IMMORTAL(ch)) {
		return;
	}
	
	// verify we have a class and role (defaults)
	if (class == NOTHING) {
		class = GET_CLASS(ch);
	}
	if (role == NOTHING) {
		role = GET_CLASS_ROLE(ch);
	}
	
	// check all abilities
	for (abil = 0; abil < NUM_ABILITIES; ++abil) {
		// class abils only
		if (ability_data[abil].parent_skill != NO_SKILL) {
			continue;
		}
		
		// determine if the player's class/role has this abil
		has = FALSE;
		for (role_iter = 0; !has && class_data[class].role[role_iter].role != ROLE_NONE; ++role_iter) {
			if (class_data[class].role[role_iter].role != role) {
				continue;
			}
			
			// found role -- check its abils
			for (iter = 0; !has && class_data[class].role[role_iter].ability[iter] != ROLE_ABIL_END; ++iter) {
				if (class_data[class].role[role_iter].ability[iter] == abil) {
					has = TRUE;	// class/role DOES have this ability
				}
			}
		}
		
		// remove any they shouldn't have
		if (ch->player_specials->abilities[abil].purchased && !has) {
			ch->player_specials->abilities[abil].purchased = FALSE;
			check_skill_sell(ch, abil);
		}
		// add if needed
		if (has) {
			ch->player_specials->abilities[abil].purchased = TRUE;
		}
	}
}


/**
* This function updates a player's class and skill levelability data based
* on current skill levels.
*
* @param char_data *ch the player
*/
void update_class(char_data *ch) {
	#define NUM_BEST  3
	#define IGNORE_BOTTOM_SKILL_POINTS  30	// amount newbies should start with
	#define BEST_SUM_REQUIRED_FOR_100  (2 * CLASS_SKILL_CAP + SPECIALTY_SKILL_CAP)
	
	int iter, skl, class = CLASS_NONE, at_zero = 0, over_basic = 0, over_specialty = 0;
	int best[NUM_BEST], best_level[NUM_BEST], best_iter, best_sub, best_total;
	int total_class_skill, total_skill;
	int old_class, old_level;
	bool ok;
	
	if (!IS_NPC(ch)) {
		total_skill = 0;
		for (best_iter = 0; best_iter < NUM_BEST; ++best_iter) {
			best[best_iter] = NO_SKILL;
			best_level[best_iter] = 0;
		}
		
		// find skill counts
		for (iter = 0; iter < NUM_SKILLS; ++iter) {
			total_skill += GET_SKILL(ch, iter);
			
			if (GET_SKILL(ch, iter) == 0) {
				++at_zero;
			}
			if (GET_SKILL(ch, iter) > BASIC_SKILL_CAP) {
				++over_basic;
			}
			if (GET_SKILL(ch, iter) > SPECIALTY_SKILL_CAP) {
				++over_specialty;
			}
			
			// update best
			for (best_iter = 0; best_iter < NUM_BEST; ++best_iter) {
				// new best
				if (GET_SKILL(ch, iter) > best_level[best_iter]) {
					// move down the other best first
					for (best_sub = NUM_BEST - 1; best_sub > best_iter; --best_sub) {
						best[best_sub] = best[best_sub-1];
						best_level[best_sub] = best_level[best_sub-1];
					}
					
					// store this one
					best[best_iter] = iter;
					best_level[best_iter] = GET_SKILL(ch, iter);
					
					// ONLY update the first matching best
					break;
				}
			}
		}
		
		// set up skill limits:
		
		// can still gain new skills (gain from 0) if either you have more zeroes than required for bonus skills, or you're not using bonus skills
		CAN_GAIN_NEW_SKILLS(ch) = (at_zero > ZEROES_REQUIRED_FOR_BONUS_SKILLS) || (over_basic <= NUM_SPECIALTY_SKILLS_ALLOWED);

		// you qualify for bonus skills so long as you have enough skills at zero
		CAN_GET_BONUS_SKILLS(ch) = (at_zero >= ZEROES_REQUIRED_FOR_BONUS_SKILLS);
		
		old_class = GET_CLASS(ch);
		old_level = GET_SKILL_LEVEL(ch);
	
		// determine class
		for (iter = 0; class == CLASS_NONE && iter < NUM_CLASSES; ++iter) {
			// check skills
			ok = TRUE;
			total_class_skill = 0;
			for (skl = 0; skl < SKILLS_PER_CLASS && ok; ++skl) {
				if (class_data[iter].skills[skl] != NO_SKILL && GET_SKILL(ch, class_data[iter].skills[skl]) > SPECIALTY_SKILL_CAP) {
					total_class_skill += GET_SKILL(ch, class_data[iter].skills[skl]);
				}
				else {
					ok = FALSE;
				}
			}
			
			if (ok) {
				class = iter;
			}
		}
		
		// clear role?
		if (class != old_class) {
			GET_CLASS_ROLE(ch) = ROLE_NONE;
		}
		
		GET_CLASS(ch) = class;
		assign_class_abilities(ch, NOTHING, NOTHING);
				
		// total up best X skills
		best_total = 0;
		for (best_iter = 0; best_iter < NUM_BEST; ++best_iter) {
			best_total += best_level[best_iter];
		}
		
		// set level
		GET_SKILL_LEVEL(ch) = (best_total - IGNORE_BOTTOM_SKILL_POINTS) * 100 / MAX(1, BEST_SUM_REQUIRED_FOR_100 - IGNORE_BOTTOM_SKILL_POINTS);
		GET_SKILL_LEVEL(ch) = MIN(CLASS_SKILL_CAP, MAX(1, GET_SKILL_LEVEL(ch)));
		
		// set progression (% of the way from 75 to 100)
		if (class != CLASS_NONE && GET_SKILL_LEVEL(ch) >= SPECIALTY_SKILL_CAP) {
			// class progression level based on % of the way
			GET_CLASS_PROGRESSION(ch) = (GET_SKILL_LEVEL(ch) - SPECIALTY_SKILL_CAP) * 100 / (CLASS_SKILL_CAP - SPECIALTY_SKILL_CAP);
		}
		else {
			GET_CLASS_PROGRESSION(ch) = 0;
		}
		
		if (GET_CLASS(ch) != old_class || GET_SKILL_LEVEL(ch) != old_level) {
			affect_total(ch);
		}
	}
}


 /////////////////////////////////////////////////////////////////////////////
//// COMMANDS ///////////////////////////////////////////////////////////////


ACMD(do_class) {
	void resort_empires();
	
	char arg2[MAX_INPUT_LENGTH];
	int iter, ab_iter, found, abil;
	empire_data *emp = GET_LOYALTY(ch);
	bool comma, ok;
	
	two_arguments(argument, arg, arg2);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "You have no class!\r\n");
	}
	else if (*arg && !str_cmp(arg, "role")) {
		// Handle role selection or display
		
		if (GET_SKILL_LEVEL(ch) < CLASS_SKILL_CAP) {
			msg_to_char(ch, "You can't set a class role until you hit skill level %d.\r\n", CLASS_SKILL_CAP);
		}
		else if (!*arg2) {
			msg_to_char(ch, "Your class role is currently set to: %s.\r\n", class_role[(int) GET_CLASS_ROLE(ch)]);
		}
		else if (GET_POS(ch) < POS_STANDING) {
			msg_to_char(ch, "You can't change your class role right now!\r\n");
		}
		else {
			found = NOTHING;
			for (iter = 0; iter < NUM_ROLES && found == NOTHING; ++iter) {
				if (is_abbrev(arg2, class_role[iter])) {
					found = iter;
				}
			}
			
			if (found == NOTHING) {
				msg_to_char(ch, "Unknown role '%s'.\r\n", arg2);
			}
			else {
				// make sure the choice applies to this class
				ok = FALSE;
				for (iter = 0; class_data[GET_CLASS(ch)].role[iter].role != ROLE_NONE && !ok; ++iter) {
					if (found == class_data[GET_CLASS(ch)].role[iter].role) {
						ok = TRUE;
					}
				}
				
				if (!ok) {
					msg_to_char(ch, "That role is not available for this class.\r\n");
				}
				else {
					// remove old abilities
					if (emp) {
						adjust_abilities_to_empire(ch, emp, FALSE);
					}
					
					// change role
					GET_CLASS_ROLE(ch) = found;
					
					// add new abilities
					assign_class_abilities(ch, NOTHING, NOTHING);
					if (emp) {
						adjust_abilities_to_empire(ch, emp, TRUE);
						resort_empires();
					}
					
					msg_to_char(ch, "Your class role is now: %s.\r\n", class_role[(int) GET_CLASS_ROLE(ch)]);
				}
			}
		}
	}
	else if (*arg) {
		msg_to_char(ch, "Invalid class command.\r\n");
	}
	else {
		// Display class info
		
		if (GET_CLASS(ch) == CLASS_NONE) {
			msg_to_char(ch, "You don't have a class. You can earn your class by raising two skills to 76 or higher.\r\n");
		}
		else {
			msg_to_char(ch, "%s\r\nClass: %s%s (%s)\t0 %d/%d/%d\r\n", PERS(ch, ch, TRUE), class_role_color[GET_CLASS_ROLE(ch)], class_data[GET_CLASS(ch)].name, class_role[(int) GET_CLASS_ROLE(ch)], GET_SKILL_LEVEL(ch), GET_GEAR_LEVEL(ch), GET_COMPUTED_LEVEL(ch));
			
			// only show roles if there are any
			if (class_data[GET_CLASS(ch)].role[0].role != ROLE_NONE) {
				msg_to_char(ch, " Available class roles:\r\n");
				
				for (iter = 0; class_data[GET_CLASS(ch)].role[iter].role != ROLE_NONE; ++iter) {
					msg_to_char(ch, "  %s%s\t0%s: ", class_role_color[class_data[GET_CLASS(ch)].role[iter].role], class_role[class_data[GET_CLASS(ch)].role[iter].role], (GET_CLASS_ROLE(ch) == class_data[GET_CLASS(ch)].role[iter].role) ? " (current)" : "");
					
					for (ab_iter = 0, comma = FALSE; class_data[GET_CLASS(ch)].role[iter].ability[ab_iter] != ROLE_ABIL_END; ++ab_iter, comma = TRUE) {
						abil = class_data[GET_CLASS(ch)].role[iter].ability[ab_iter];
						msg_to_char(ch, "%s%s%s&0", (comma ? ", " : ""), (HAS_ABILITY(ch, abil) ? "&g" : ""), ability_data[abil].name);
					}
					msg_to_char(ch, "\r\n");
				}
			}
		}
	}
}
