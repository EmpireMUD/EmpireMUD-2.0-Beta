/* ************************************************************************
*   File: act.highsorcery.c                               EmpireMUD 2.0b5 *
*  Usage: implementation for high sorcery abilities                       *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
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

/**
* Contents:
*   Data
*   Helpers
*   Commands
*   Chants
*   Rituals
*/

// external vars

// external funcs
void scale_item_to_level(obj_data *obj, int level);
extern bool trigger_counterspell(char_data *ch);	// spells.c
void trigger_distrust_from_hostile(char_data *ch, empire_data *emp);	// fight.c

// locals
void send_ritual_messages(char_data *ch, int rit, int pos);


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

// for do_disenchant
// TODO updates for this:
//  - this should be moved to a live config for general disenchant -> possibly global interactions
//  - add the ability to set disenchant results by gear or scaled level
//  - add an obj interaction to allow custom disenchant results
const struct {
	obj_vnum vnum;
	double chance;
} disenchant_data[] = {
	{ o_LIGHTNING_STONE, 5 },
	{ o_BLOODSTONE, 5 },
	{ o_IRIDESCENT_IRIS, 5 },
	{ o_GLOWING_SEASHELL, 5 },

	// this must go last
	{ NOTHING, 0.0 }
};


/**
* Ritual setup function: for rituals that take arguments or must pre-validate
* anything. If your ritual doesn't require this, use start_simple_ritual as
* your setup function.
*
* You must call start_ritual(ch, ritual) and return TRUE at the end of setup.
* If you need to store data, you can use action vnums 1 and 2; 0 will be the
* ritual number. You can only add action vnum data AFTER calling start_ritual.
*
* @param char_data *ch The person performing the ritual.
* @param char *argument The argument (text after the ritual name).
* @param int ritual The position in the ritual_data[] table.
* @return bool Returns TRUE if the ritual started, or FALSE if there was an error.
*/
#define RITUAL_SETUP_FUNC(name)  bool (name)(char_data *ch, char *argument, int ritual)


/**
* Ritual finish function: This is called when the ritual is complete, and
* should contain the actual functionality of the ritual.
*
* @param char_data *ch The player performing the ritual.
* @param int ritual The position in the ritual_data[] table.
*/
#define RITUAL_FINISH_FUNC(name)  void (name)(char_data *ch, int ritual)


// ritual flags
// TODO these are not currently used but should maybe go in structs.h
#define RIT_FLAG  BIT(0)

// misc
#define NO_MESSAGE	{ "\t", "\t" }	// skip a tick (5 seconds)
#define MESSAGE_END  { "\n", "\n" }  // end of sequence (one final tick to get here)

// ritual prototypes
RITUAL_FINISH_FUNC(perform_devastation_ritual);
RITUAL_FINISH_FUNC(perform_phoenix_rite);
RITUAL_FINISH_FUNC(perform_ritual_of_burdens);
RITUAL_FINISH_FUNC(perform_ritual_of_defense);
RITUAL_FINISH_FUNC(perform_ritual_of_detection);
RITUAL_FINISH_FUNC(perform_ritual_of_teleportation);
RITUAL_FINISH_FUNC(perform_sense_life_ritual);
RITUAL_FINISH_FUNC(perform_siege_ritual);
RITUAL_SETUP_FUNC(start_devastation_ritual);
RITUAL_SETUP_FUNC(start_ritual_of_defense);
RITUAL_SETUP_FUNC(start_ritual_of_detection);
RITUAL_SETUP_FUNC(start_ritual_of_teleportation);
RITUAL_SETUP_FUNC(start_siege_ritual);
RITUAL_SETUP_FUNC(start_simple_ritual);

// chant prototypes
RITUAL_FINISH_FUNC(perform_chant_of_druids);
RITUAL_FINISH_FUNC(perform_chant_of_illusions);
RITUAL_FINISH_FUNC(perform_chant_of_nature);
RITUAL_SETUP_FUNC(start_chant_of_druids);
RITUAL_SETUP_FUNC(start_chant_of_illusions);
RITUAL_SETUP_FUNC(start_chant_of_nature);


// SCMD_RITUAL, SCMD_CHANT
const char *ritual_scmd[] = { "ritual", "chant" };
const int ritual_action[] = { ACT_RITUAL, ACT_CHANTING };


// Ritual data
// TODO this could all be moved to live data, but would need a lot of support and a custom editor
struct ritual_data_type {
	char *name;
	int cost;
	any_vnum ability;
	bitvector_t flags;
	int subcmd;
	RITUAL_SETUP_FUNC(*begin);	// function pointer
	RITUAL_FINISH_FUNC(*perform);	// function pointer
	struct ritual_strings strings[24];	// one sent per tick, terminate with MESSAGE_END
} ritual_data[] = {
	// 0: ritual of burdens
	{ "burdens", 5, ABIL_RITUAL_OF_BURDENS, 0, SCMD_RITUAL,
		start_simple_ritual,
		perform_ritual_of_burdens,
		{{ "You whisper your burdens into the air...", "$n whispers $s burdens into the air..." },
		{ "\n", "\n" }
	}},
	
	// 1: ritual of teleportation
	{ "teleportation", 25, ABIL_RITUAL_OF_TELEPORTATION, 0, SCMD_RITUAL,
		start_ritual_of_teleportation,
		perform_ritual_of_teleportation,
		{{ "You wrap yourself up and begin chanting...", "$n wraps $mself up and begins chanting..." },
		{ "There is a bright blue glow all around you...", "$n begins to glow bright blue, like a flame..." },
		MESSAGE_END
	}},
	
	// 2: phoenix rite
	{ "phoenix", 25, ABIL_PHOENIX_RITE, 0, SCMD_RITUAL,
		start_simple_ritual,
		perform_phoenix_rite,
		{{ "You light some candles and begin the Phoenix Rite.", "$n lights some candles." },
		{ "You sit and place the candles in a circle around you.", "$n sits and places the candles in a circle around $mself." },
		NO_MESSAGE,
		{ "You focus on the flame of the candles...", "$n moves back and forth as if in a trance." },
		NO_MESSAGE,
		{ "You reach out to the sides and grab the flame of the candles in your hands!", "$n's hands shoot out to the sides and grab the flames of the candles!" },
		MESSAGE_END
	}},
	
	// 3: ritual of defense
	{ "defense", 15, ABIL_RITUAL_OF_DEFENSE, 0, SCMD_RITUAL,
		start_ritual_of_defense,
		perform_ritual_of_defense,
		{{ "You begin the incantation for the Ritual of Defense.", "\t" },
		{ "You say, 'Empower now this wall of stone...'", "$n says, 'Empower now this wall of stone...'" },
		{ "You say, 'With wisdom of the sages...'", "$n says, 'With wisdom of the sages...'" },
		{ "You say, 'Ward against the foes above...'", "$n says, 'Ward against the foes above...'" },
		{ "You say, 'And strong, withstand the ages!'", "$n says, 'And strong, withstand the ages!'" },
		MESSAGE_END
	}},
	
	// 4: sense life ritual
	{ "senselife", 15, ABIL_SENSE_LIFE_RITUAL, 0, SCMD_RITUAL,
		start_simple_ritual,
		perform_sense_life_ritual,
		{{ "You murmur the words to the Sense Life Ritual...", "$n murmurs some arcane words..." },
		MESSAGE_END
	}},
	
	// 5: ritual of detection
	{ "detection", 15, ABIL_RITUAL_OF_DETECTION, 0, SCMD_RITUAL,
		start_ritual_of_detection,
		perform_ritual_of_detection,
		{{ "You pull a piece of chalk from your pocket and begin drawing circles on the ground.", "$n pulls out a piece of chalk and begins drawing circles on the ground." },
		NO_MESSAGE,
		{ "You make crossed lines through the circles you've drawn.", "$n makes crossed lines through the circles $e has drawn." },
		NO_MESSAGE,
		{ "You begin writing the words 'where', 'find', and 'reveal' on the ground.", "$n begins writing words in the circles on the ground." },
		MESSAGE_END
	}},
	
	// 6: siege ritual
	{ "siege", 30, ABIL_SIEGE_RITUAL, 0, SCMD_RITUAL,
		start_siege_ritual,
		perform_siege_ritual,
		{{ "You draw mana to yourself...", "$n begins drawing mana to $mself..." },
		NO_MESSAGE,
		{ "You concentrate your power into a ball of fire...", "$n concentrates $s power into a fireball..." },
		NO_MESSAGE,
		{ "You summon the full force of your magic...", "$n draws more mana..." },
		NO_MESSAGE,
		{ "You twist and grow the ball of fire in your hands...", "$n twists and grows the ball of fire in $s hands..." },
		NO_MESSAGE,
		MESSAGE_END
	}},
	
	// 7: devastation ritual
	{ "devastation", 15, ABIL_DEVASTATION_RITUAL, 0, SCMD_RITUAL,
		start_devastation_ritual,
		perform_devastation_ritual,
		{{ "You plant your staff hard into the ground and begin focusing mana...", "$n plants $s staff hard into the ground and begins drawing mana toward $mself..." },
		NO_MESSAGE,
		{ "You send out shockwaves of mana from your staff...", "$n's staff sends out shockwaves of mana..." },
		NO_MESSAGE,
		MESSAGE_END
	}},
	
	// 8: chant of druids
	{ "druids", 0, NO_ABIL, 0, SCMD_CHANT,
		start_chant_of_druids,
		perform_chant_of_druids,
		{{ "You start the chant of druids...", "$n starts the chant of druids..." },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		{ "You chant, 'Chant of druids placeholder text.'", "$n chants, 'Chant of druids placeholder text.'" },
		MESSAGE_END
	}},
	
	// 9: chant of nature
	{ "nature", 0, ABIL_CHANT_OF_NATURE, 0, SCMD_CHANT,
		start_chant_of_nature,
		perform_chant_of_nature,
		{{ "You start the chant of nature...", "$n starts the chant of nature..." },
		{ "You chant, 'O ancient Ash, bubbling waters, let life flow anew...'", "$n chants, 'O ancient Ash, bubbling waters, let life flow anew...'" },
		NO_MESSAGE,
		{ "You chant, 'O great Ceres, mother of seeds, rise above the dew...'", "$n chants, 'O great Ceres, mother of seeds, rise above the dew...'" },
		NO_MESSAGE,
		{ "You chant, 'O Viridius, greenest father, god of vines that coil...'", "$n chants, 'O Viridius, greenest father, god of vines that coil...'" },
		NO_MESSAGE,
		{ "You chant, 'O Chloris, Elysian spring, lift us from the soil!'", "$n chants, 'O Chloris, Elysian spring, lift us from the soil!'" },
		NO_MESSAGE,
		MESSAGE_END
	}},
	
	// 10: chant of illusions
	{ "illusions", 0, ABIL_CHANT_OF_ILLUSIONS, 0, SCMD_CHANT,
		start_chant_of_illusions,
		perform_chant_of_illusions,
		{{ "You start the chant of illusions...", "$n starts the chant of illusions..." },
		{ "You chant, 'Chant of illusions placeholder text.'", "$n chants, 'Chant of illusions placeholder text.'" },
		NO_MESSAGE,
		{ "You chant, 'Chant of illusions placeholder text.'", "$n chants, 'Chant of illusions placeholder text.'" },
		NO_MESSAGE,
		{ "You chant, 'Chant of illusions placeholder text.'", "$n chants, 'Chant of illusions placeholder text.'" },
		NO_MESSAGE,
		{ "You chant, 'Chant of illusions placeholder text.'", "$n chants, 'Chant of illusions placeholder text.'" },
		NO_MESSAGE,
		{ "You chant, 'Chant of illusions placeholder text.'", "$n chants, 'Chant of illusions placeholder text.'" },
		NO_MESSAGE,
		{ "You chant, 'Chant of illusions placeholder text.'", "$n chants, 'Chant of illusions placeholder text.'" },
		NO_MESSAGE,
		{ "You chant, 'Chant of illusions placeholder text.'", "$n chants, 'Chant of illusions placeholder text.'" },
		NO_MESSAGE,
		MESSAGE_END
	}},
	
	{ "\n", 0, NO_ABIL, 0, 0, NULL, NULL, { MESSAGE_END } },
};


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* @param char_data *ch The player trying to perform a ritual.
* @param int ritual The position in the ritual_data[] table.
*/
bool can_use_ritual(char_data *ch, int ritual) {
	if (IS_NPC(ch)) {
		return FALSE;
	}
	if (ritual_data[ritual].ability != NO_ABIL && !has_ability(ch, ritual_data[ritual].ability)) {
		return FALSE;
	}
		
	return TRUE;
}


// for devastation_ritual
INTERACTION_FUNC(devastate_crop) {	
	crop_data *cp = ROOM_CROP(inter_room);
	obj_data *newobj;
	int num = interaction->quantity;
	
	msg_to_char(ch, "You devastate the %s and collect %s (x%d)!\r\n", GET_CROP_NAME(cp), get_obj_name_by_proto(interaction->vnum), num);
	sprintf(buf, "$n's powerful ritual devastates the %s crops!", GET_CROP_NAME(cp));
	act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
	
	while (num-- > 0) {
		obj_to_char_or_room((newobj = read_object(interaction->vnum, TRUE)), ch);
		scale_item_to_level(newobj, 1);	// minimum level
		load_otrigger(newobj);
	}
	
	return TRUE;
}


// for devastation_ritual
INTERACTION_FUNC(devastate_trees) {
	char buf[MAX_STRING_LENGTH], type[MAX_STRING_LENGTH];
	obj_data *newobj;
	int num;
	
	snprintf(type, sizeof(type), GET_SECT_NAME(SECT(inter_room)));
	strtolower(type);
	
	if (interaction->quantity != 1) {
		snprintf(buf, sizeof(buf), "You devastate the %s and collect %s (x%d)!", type, get_obj_name_by_proto(interaction->vnum), interaction->quantity);
	}
	else {
		snprintf(buf, sizeof(buf), "You devastate the %s and collect %s!", type, get_obj_name_by_proto(interaction->vnum));
	}
	act(buf, FALSE, ch, NULL, NULL, TO_CHAR);
	
	snprintf(buf, sizeof(buf), "$n's powerful ritual devastates the %s!", type);
	act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
	
	for (num = 0; num < interaction->quantity; ++num) {
		obj_to_char_or_room((newobj = read_object(interaction->vnum, TRUE)), ch);
		scale_item_to_level(newobj, 1);	// minimum level
		load_otrigger(newobj);
	}
	
	return TRUE;
}


/**
* @param char_data *ch The enchanter.
* @param int max_scale Optional: The highest scale level it will use (0 = no max)
* @return double The number of scale points available for an enchantment at that level.
*/
double get_enchant_scale_for_char(char_data *ch, int max_scale) {
	extern int get_crafting_level(char_data *ch);
	
	double points_available;
	int level;

	// enchant scale level is whichever is less: obj scale level, or player crafting level
	level = MAX(get_crafting_level(ch), get_approximate_level(ch));
	if (max_scale > 0) {
		level = MIN(max_scale, level);
	}
	points_available = level / 100.0 * config_get_double("enchant_points_at_100");
	return MAX(points_available, 1.0);
}


/**
* This function is called by the action message each update tick, for a person
* who is performing a ritual. That's why it's called that.
*
* @param char_data *ch The person performing the ritual.
*/
void perform_ritual(char_data *ch) {	
	int rit = GET_ACTION_VNUM(ch, 0);
	
	GET_ACTION_TIMER(ch) += 1;
	send_ritual_messages(ch, rit, GET_ACTION_TIMER(ch));
	
	// this triggers the end of the ritual, based on MESSAGE_END
	if (*ritual_data[rit].strings[GET_ACTION_TIMER(ch)].to_char == '\n') {
		GET_ACTION(ch) = ACT_NONE;
		
		if (ritual_data[rit].perform) {
			(ritual_data[rit].perform)(ch, rit);
		}
		else {
			msg_to_char(ch, "You complete the ritual but it doesn't seem to be implemented.\r\n");
		}
	}
}


/**
* Check and send messages for a ritual tick
*
* @param char_data *ch actor
* @param int rit The ritual_data number
* @param int pos Number of ticks into the ritual
*/
void send_ritual_messages(char_data *ch, int rit, int pos) {
	// the very first message (pos==0) must be sent
	int nospam = (pos == 0 ? 0 : TO_SPAMMY);

	if (*ritual_data[rit].strings[pos].to_char != '\t' && *ritual_data[rit].strings[pos].to_char != '\n') {
		act(ritual_data[rit].strings[pos].to_char, FALSE, ch, NULL, NULL, TO_CHAR | nospam);
	}
	if (*ritual_data[rit].strings[pos].to_room != '\t' && *ritual_data[rit].strings[pos].to_room != '\n') {
		act(ritual_data[rit].strings[pos].to_room, FALSE, ch, NULL, NULL, TO_ROOM | nospam);
	}
}


/**
* Sets up the ritual action and sends first message.
*
* @param char_data *ch The player.
* @param int ritual The ritual (position in ritual_data)
*/
void start_ritual(char_data *ch, int ritual) {
	if (!ch || IS_NPC(ch)) {
		return;
	}
	
	// first message
	send_ritual_messages(ch, ritual, 0);
	
	start_action(ch, ritual_action[ritual_data[ritual].subcmd], 0);
	GET_ACTION_VNUM(ch, 0) = ritual;
}


/**
* Command processing for the "summon materials" ability, called via the
* central do_summon command.
*
* @param char_data *ch The person using the command.
* @param char *argument The typed arg.
*/
void summon_materials(char_data *ch, char *argument) {
	void sort_storage(empire_data *emp);
	void read_vault(empire_data *emp);

	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], *objname;
	struct empire_storage_data *store, *next_store;
	int count = 0, total = 1, number, pos;
	empire_data *emp;
	int cost = 2;	// * number of things to summon
	obj_data *proto;
	bool found = FALSE;

	half_chop(argument, arg1, arg2);
	
	if (!(emp = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You can't summon empire materials if you're not in an empire.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_STORAGE)) {
		msg_to_char(ch, "You aren't high enough rank to retrieve from the empire inventory.\r\n");
		return;
	}
	
	if (GET_ISLAND_ID(IN_ROOM(ch)) == NO_ISLAND) {
		msg_to_char(ch, "You can't summon materials here.\r\n");
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_SUMMON_MATERIALS)) {
		return;
	}
	
	// pull out a number if the first arg is a number
	if (*arg1 && is_number(arg1)) {
		total = atoi(arg1);
		if (total < 1) {
			msg_to_char(ch, "You have to summon at least 1.\r\n");
			return;
			}
		strcpy(arg1, arg2);
	}
	else if (*arg1 && *arg2) {
		// re-combine if it wasn't a number
		sprintf(arg1 + strlen(arg1), " %s", arg2);
	}
	
	// arg1 now holds the desired name
	objname = arg1;
	number = get_number(&objname);

	// multiply cost for total, but don't store it
	if (!can_use_ability(ch, ABIL_SUMMON_MATERIALS, MANA, cost * total, NOTHING)) {
		return;
	}

	if (!*objname) {
		msg_to_char(ch, "What material would you like to summon (use einventory to see what you have)?\r\n");
		return;
	}
	
	msg_to_char(ch, "You open a tiny portal to summon materials...\r\n");
	act("$n opens a tiny portal to summon materials...", FALSE, ch, NULL, NULL, TO_ROOM);
	
	// sort first
	sort_storage(emp);

	pos = 0;
	for (store = EMPIRE_STORAGE(emp); !found && store; store = next_store) {
		next_store = store->next;
		
		// island check
		if (store->island != GET_ISLAND_ID(IN_ROOM(ch))) {
			continue;
		}
		
		proto = obj_proto(store->vnum);
		if (proto && multi_isname(objname, GET_OBJ_KEYWORDS(proto)) && (++pos == number)) {
			found = TRUE;
			
			if (stored_item_requires_withdraw(proto)) {
				msg_to_char(ch, "You can't summon materials out of the vault.\r\n");
				return;
			}
			
			while (count < total && store->amount > 0) {
				if (retrieve_resource(ch, emp, store, FALSE)) {
					++count;
				}
				else {
					break;	// no more
				}
			}
		}
	}
	
	if (found && count < total && count > 0) {
		msg_to_char(ch, "There weren't enough, but you managed to summon %d.\r\n", count);
	}
	
	// result messages
	if (!found) {
		msg_to_char(ch, "Nothing like that is stored around here.\r\n");
	}
	else if (count == 0) {
		// they must have gotten an error message
	}
	else {
		// save the empire
		if (found) {
			GET_MANA(ch) -= cost * count;	// charge only the amount retrieved
			EMPIRE_NEEDS_SAVE(emp) = TRUE;
			read_vault(emp);
			gain_ability_exp(ch, ABIL_SUMMON_MATERIALS, 1);
		}
	}
	
	command_lag(ch, WAIT_OTHER);
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////


ACMD(do_collapse) {
	obj_data *portal, *obj, *reverse = NULL;
	room_data *to_room;
	int cost = 15;
	
	one_argument(argument, arg);

	if (!can_use_ability(ch, ABIL_PORTAL_MASTER, MANA, cost, NOTHING)) {
		return;
	}
	
	if (!*arg) {
		msg_to_char(ch, "Collapse which portal?\r\n");
		return;
	}
	
	if (!(portal = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "You don't see a %s here.\r\n", arg);
		return;
	}
	
	if (!IS_PORTAL(portal) || GET_OBJ_TIMER(portal) == UNLIMITED) {
		msg_to_char(ch, "You can't collapse that.\r\n");
		return;
	}
	
	if (!(to_room = real_room(GET_PORTAL_TARGET_VNUM(portal)))) {
		msg_to_char(ch, "You can't collapse that.\r\n");
		return;
	}
	
	// find the reverse portal
	for (obj = ROOM_CONTENTS(to_room); obj && !reverse; obj = obj->next_content) {
		if (GET_PORTAL_TARGET_VNUM(obj) == GET_ROOM_VNUM(IN_ROOM(ch))) {
			reverse = obj;
		}
	}

	// do it
	charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
	
	act("You grab $p and draw it shut!", FALSE, ch, portal, NULL, TO_CHAR);
	act("$n grabs $p and draws it shut!", FALSE, ch, portal, NULL, TO_ROOM);
	
	extract_obj(portal);
	
	if (reverse) {
		if (ROOM_PEOPLE(to_room)) {
			act("$p is collapsed from the other side!", FALSE, ROOM_PEOPLE(to_room), reverse, NULL, TO_CHAR | TO_ROOM);
		}
		extract_obj(reverse);
	}
	
	gain_ability_exp(ch, ABIL_PORTAL_MASTER, 20);
}


ACMD(do_colorburst) {
	char_data *vict = NULL;
	struct affected_type *af;
	int amt;
	
	int costs[] = { 15, 30, 45 };
	int levels[] = { -5, -10, -15 };
	
	if (!can_use_ability(ch, ABIL_COLORBURST, MANA, CHOOSE_BY_ABILITY_LEVEL(costs, ch, ABIL_COLORBURST), COOLDOWN_COLORBURST)) {
		return;
	}
	
	// find target
	one_argument(argument, arg);
	if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
		return;
	}
	if (!*arg && !(vict = FIGHTING(ch))) {
		msg_to_char(ch, "Who would you like to cast that at?\r\n");
		return;
	}
	if (ch == vict) {
		msg_to_char(ch, "You wouldn't want to cast that on yourself.\r\n");
		return;
	}
	
	// check validity
	if (!can_fight(ch, vict)) {
		act("You can't attack $M!", FALSE, ch, NULL, vict, TO_CHAR);
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_COLORBURST)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, CHOOSE_BY_ABILITY_LEVEL(costs, ch, ABIL_COLORBURST), COOLDOWN_COLORBURST, 30, WAIT_COMBAT_SPELL);
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// counterspell??
	if (trigger_counterspell(vict) || AFF_FLAGGED(vict, AFF_IMMUNE_HIGH_SORCERY)) {
		act("You fire a burst of color at $N, but $E deflects it!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n fires a burst of color at you, but it's deflected by your counterspell!", FALSE, ch, NULL, vict, TO_VICT);
		act("$n fires a burst of color at $N, but $E deflects it.", FALSE, ch, NULL, vict, TO_NOTVICT);
	}
	else {
		// succeed
	
		act("You whip your hand forward and fire a burst of color at $N!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n whips $s hand forward and fires a burst of color at you!", FALSE, ch, NULL, vict, TO_VICT);
		act("$n whips $s hand forward and fires a burst of color at $N!", FALSE, ch, NULL, vict, TO_NOTVICT);
		
		amt = CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_COLORBURST) - GET_INTELLIGENCE(ch);
	
		af = create_mod_aff(ATYPE_COLORBURST, 6, APPLY_TO_HIT, amt, ch);
		affect_join(vict, af, 0);

		engage_combat(ch, vict, FALSE);
	}
	
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, ABIL_COLORBURST, 15);
	}
}


ACMD(do_disenchant) {
	struct obj_apply *apply, *next_apply, *temp;
	obj_data *obj, *reward;
	int iter, prc, rnd;
	obj_vnum vnum = NOTHING;
	int cost = 5;
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_DISENCHANT, MANA, cost, NOTHING)) {
		// sends its own messages
	}
	else if (!*arg) {
		msg_to_char(ch, "Disenchant what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You seem to have lost your %s.\r\n", arg);
	}
	else if (ABILITY_TRIGGERS(ch, NULL, obj, ABIL_DISENCHANT)) {
		return;
	}
	else if (!bind_ok(obj, ch)) {
		msg_to_char(ch, "You can't disenchant something that is bound to someone else.\r\n");
	}
	else if (!OBJ_FLAGGED(obj, OBJ_ENCHANTED)) {
		act("$p is not even enchanted.", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else {
		charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
		REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_ENCHANTED | OBJ_SUPERIOR);
		
		for (apply = GET_OBJ_APPLIES(obj); apply; apply = next_apply) {
			next_apply = apply->next;
			if (apply->apply_type == APPLY_TYPE_ENCHANTMENT) {
				REMOVE_FROM_LIST(apply, GET_OBJ_APPLIES(obj), next);
				free(apply);
			}
		}
		
		act("You hold $p over your head and shout 'KA!' as you release the mana bound to it!\r\nThere is a burst of red light from $p and then it fizzles and is disenchanted.", FALSE, ch, obj, NULL, TO_CHAR);
		act("$n shouts 'KA!' and cracks $p, which blasts out red light, and then fizzles.", FALSE, ch, obj, NULL, TO_ROOM);
		gain_ability_exp(ch, ABIL_DISENCHANT, 33.4);
		
		// obj back?
		if (skill_check(ch, ABIL_DISENCHANT, DIFF_MEDIUM)) {
			rnd = number(1, 10000);
			prc = 0;
			for (iter = 0; disenchant_data[iter].vnum != NOTHING && vnum == NOTHING; ++iter) {
				prc += disenchant_data[iter].chance * 100;
				if (rnd <= prc) {
					vnum = disenchant_data[iter].vnum;
				}
			}
		
			if (vnum != NOTHING) {
				reward = read_object(vnum, TRUE);
				obj_to_char(reward, ch);
				act("You manage to weave the freed mana into $p!", FALSE, ch, reward, NULL, TO_CHAR);
				act("$n weaves the freed mana into $p!", TRUE, ch, reward, NULL, TO_ROOM);
				load_otrigger(reward);
			}
		}
	}
}


ACMD(do_dispel) {
	struct over_time_effect_type *dot, *next_dot;
	char_data *vict = ch;
	int cost = 30;
	bool found;
	
	one_argument(argument, arg);
	
	
	if (!can_use_ability(ch, ABIL_DISPEL, MANA, cost, COOLDOWN_DISPEL)) {
		return;
	}
	else if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_DISPEL)) {
		return;
	}
	else {
		// verify they have dots
		found = FALSE;
		for (dot = vict->over_time_effects; dot && !found; dot = dot->next) {
			if (dot->damage_type == DAM_MAGICAL || dot->damage_type == DAM_FIRE) {
				found = TRUE;
			}
		}
		if (!found) {
			if (ch == vict) {
				msg_to_char(ch, "You aren't afflicted by anything that can be dispelled.\r\n");
			}
			else {
				act("$E isn't afflicted by anything that can be dispelled.", FALSE, ch, NULL, vict, TO_CHAR);
			}
			return;
		}
	
		charge_ability_cost(ch, MANA, cost, COOLDOWN_DISPEL, 9, WAIT_SPELL);
		
		if (ch == vict) {
			msg_to_char(ch, "You shout 'KA!' and dispel your afflictions.\r\n");
			act("$n shouts 'KA!' and dispels $s afflictions.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			act("You shout 'KA!' and dispel $N's afflictions.", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n shouts 'KA!' and dispels your afflictions.", FALSE, ch, NULL, vict, TO_VICT);
			act("$n shouts 'KA!' and dispels $N's afflictions.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}

		// remove DoTs
		for (dot = vict->over_time_effects; dot; dot = next_dot) {
			next_dot = dot->next;

			if (dot->damage_type == DAM_MAGICAL || dot->damage_type == DAM_FIRE) {
				dot_remove(vict, dot);
			}
		}
		
		if (can_gain_exp_from(ch, vict)) {
			gain_ability_exp(ch, ABIL_DISPEL, 33.4);
		}

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
	}
}


ACMD(do_enervate) {
	char_data *vict = NULL;
	struct affected_type *af, *af2;
	int cost = 20;
	
	int levels[] = { 1, 1, 3 };
		
	if (!can_use_ability(ch, ABIL_ENERVATE, MANA, cost, COOLDOWN_ENERVATE)) {
		return;
	}
	
	// find target
	one_argument(argument, arg);
	if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
		return;
	}
	if (!*arg && !(vict = FIGHTING(ch))) {
		msg_to_char(ch, "Who would you like to cast that at?\r\n");
		return;
	}
	if (ch == vict) {
		msg_to_char(ch, "You wouldn't want to cast that on yourself.\r\n");
		return;
	}
	
	// check validity
	if (!can_fight(ch, vict)) {
		act("You can't attack $M!", FALSE, ch, NULL, vict, TO_CHAR);
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_ENERVATE)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, COOLDOWN_ENERVATE, SECS_PER_MUD_HOUR, WAIT_COMBAT_SPELL);
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// counterspell??
	if (trigger_counterspell(vict) || AFF_FLAGGED(vict, AFF_IMMUNE_HIGH_SORCERY)) {
		act("You attempt to hex $N with enervate, but it fails!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n attempts to hex you with enervate, but it's deflected by your counterspell!", FALSE, ch, NULL, vict, TO_VICT);
		act("$n attempts to hex $N with enervate, but it fails!", FALSE, ch, NULL, vict, TO_NOTVICT);
	}
	else {
		// succeed
	
		act("$N starts to glow red as you shout the enervate hex at $M! You feel your own stamina grow as you drain $S.", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n shouts somthing at you... The world takes on a reddish hue and you feel your stamina drain.", FALSE, ch, NULL, vict, TO_VICT);
		act("$n shouts some kind of hex at $N, who starts to glow red and seems weakened!", FALSE, ch, NULL, vict, TO_NOTVICT);
	
		af = create_mod_aff(ATYPE_ENERVATE, 1 MUD_HOURS, APPLY_MOVE_REGEN, -1 * GET_INTELLIGENCE(ch) / 2, ch);
		affect_join(vict, af, 0);
		af2 = create_mod_aff(ATYPE_ENERVATE_GAIN, 1 MUD_HOURS, APPLY_MOVE_REGEN, CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_ENERVATE), ch);
		affect_join(ch, af2, 0);

		engage_combat(ch, vict, FALSE);
	}
	
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, ABIL_ENERVATE, 15);
	}
}


ACMD(do_foresight) {
	struct affected_type *af;
	char_data *vict = ch;
	int amt, cost = 30;
	
	int levels[] = { 5, 10, 15 };
	
	one_argument(argument, arg);
	
	if (!can_use_ability(ch, ABIL_FORESIGHT, MANA, cost, NOTHING)) {
		return;
	}
	else if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_FORESIGHT)) {
		return;
	}
	else {
		charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
		
		if (ch == vict) {
			msg_to_char(ch, "You pull out a grease pencil and mark each of your eyelids with an X...\r\nYou feel the gift of foresight!\r\n");
			act("$n pulls out a grease pencil and marks each of $s eyelids with an X.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			act("You pull out a grease pencil and mark each of $N's eyelids with an X, giving $M the gift of foresight.", FALSE, ch, NULL, vict, TO_CHAR);
			act("$n marks each of your eyelids using a grease pencil...\r\nYou feel the gift of foresight!", FALSE, ch, NULL, vict, TO_VICT);
			act("$n pulls out a grease pencil and marks each of $N's eyelids with an X.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}
		
		amt = CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_FORESIGHT) + GET_INTELLIGENCE(ch);
		
		af = create_mod_aff(ATYPE_FORESIGHT, 12 MUD_HOURS, APPLY_DODGE, amt, ch);
		affect_join(vict, af, 0);
		
		gain_ability_exp(ch, ABIL_FORESIGHT, 15);

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
	}
}


ACMD(do_manashield) {
	struct affected_type *af1, *af2, *af3;
	int cost = 25;
	int amt;
	
	int levels[] = { 2, 4, 6 };

	if (affected_by_spell(ch, ATYPE_MANASHIELD)) {
		msg_to_char(ch, "You wipe the symbols off your arm and cancel your mana shield.\r\n");
		act("$n wipes the arcane symbols off $s arm.", TRUE, ch, NULL, NULL, TO_ROOM);
		affect_from_char(ch, ATYPE_MANASHIELD, FALSE);
	}
	else if (!can_use_ability(ch, ABIL_MANASHIELD, MANA, cost, NOTHING)) {
		return;
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_MANASHIELD)) {
		return;
	}
	else {
		charge_ability_cost(ch, MANA, cost, NOTHING, 0, WAIT_SPELL);
		
		msg_to_char(ch, "You pull out a grease pencil and draw a series of arcane symbols down your left arm...\r\nYou feel yourself shielded by your mana!\r\n");
		act("$n pulls out a grease pencil and draws a series of arcane symbols down $s left arm.", TRUE, ch, NULL, NULL, TO_ROOM);
		
		amt = CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_MANASHIELD) + (GET_INTELLIGENCE(ch) / 3);
		
		af1 = create_mod_aff(ATYPE_MANASHIELD, 24 MUD_HOURS, APPLY_MANA, -25, ch);
		af2 = create_mod_aff(ATYPE_MANASHIELD, 24 MUD_HOURS, APPLY_RESIST_PHYSICAL, amt, ch);
		af3 = create_mod_aff(ATYPE_MANASHIELD, 24 MUD_HOURS, APPLY_RESIST_MAGICAL, amt, ch);
		affect_join(ch, af1, 0);
		affect_join(ch, af2, 0);
		affect_join(ch, af3, 0);
		
		// possible to go negative here
		GET_MANA(ch) = MAX(0, GET_MANA(ch));
	}
}


ACMD(do_mirrorimage) {
	extern char_data *has_familiar(char_data *ch);
	extern struct custom_message *pick_custom_longdesc(char_data *ch);
	void scale_mob_as_familiar(char_data *mob, char_data *master);
	
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
	LL_FOREACH(character_list, other) {
		if (ch != other && IS_NPC(other) && GET_MOB_VNUM(other) == vnum && other->master == ch) {
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
	scale_mob_as_familiar(mob, ch);
	
	char_to_room(mob, IN_ROOM(ch));
	
	// restrings
	GET_PC_NAME(mob) = str_dup(PERS(ch, ch, FALSE));
	GET_SHORT_DESC(mob) = str_dup(GET_PC_NAME(mob));
	GET_REAL_SEX(mob) = GET_SEX(ch);	// need this for some desc stuff
	
	// longdesc is more complicated
	if (GET_MORPH(ch)) {
		sprintf(buf, "%s\r\n", MORPH_LONG_DESC(GET_MORPH(ch)));
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
	SET_BIT(MOB_FLAGS(mob), MOB_NO_RESCALE);
	affect_total(mob);
	
	act("You create a mirror image to distract your foes!", FALSE, ch, NULL, NULL, TO_CHAR);
	
	// switch at least 1 thing that's hitting ch
	found = FALSE;
	for (other = ROOM_PEOPLE(IN_ROOM(ch)); other; other = other->next_in_room) {
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
	
	load_mtrigger(mob);
}


ACMD(do_ritual) {
	char arg2[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	int iter, rit = NOTHING;
	bool found, result = FALSE;
	
	half_chop(argument, arg, arg2);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot perform %ss.\r\n", ritual_scmd[subcmd]);
		return;
	}
	
	// just ending
	if (GET_ACTION(ch) == ACT_RITUAL || GET_ACTION(ch) == ACT_CHANTING) {
		msg_to_char(ch, "You stop the %s.\r\n", ritual_scmd[subcmd]);
		sprintf(buf, "$n stops the %s.", ritual_scmd[subcmd]);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		cancel_action(ch);
		return;
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You are already busy doing something else.\r\n");
		return;
	}
	
	// list rituals
	if (!*arg) {
		msg_to_char(ch, "You can perform the following %ss:", ritual_scmd[subcmd]);
		
		found = FALSE;
		for (iter = 0; *ritual_data[iter].name != '\n'; ++iter) {
			if (ritual_data[iter].subcmd == subcmd && can_use_ritual(ch, iter)) {
				msg_to_char(ch, "%s%s", (found ? ", " : " "), ritual_data[iter].name);
				found = TRUE;
			}
		}
		
		if (found) {
			msg_to_char(ch, "\r\n");
		}
		else {
			msg_to_char(ch, " none\r\n");
		}
		return;
	}
	
	// find ritual
	found = FALSE;
	for (iter = 0; *ritual_data[iter].name != '\n' && rit == NOTHING; ++iter) {
		if (ritual_data[iter].subcmd == subcmd && can_use_ritual(ch, iter) && is_abbrev(arg, ritual_data[iter].name)) {
			found = TRUE;
			rit = iter;
		}
	}
	
	if (!found) {
		msg_to_char(ch, "You don't know that %s.\r\n", ritual_scmd[subcmd]);
		return;
	}
	
	// triggers?
	if (ritual_data[rit].ability != NO_ABIL && ABILITY_TRIGGERS(ch, NULL, NULL, ritual_data[rit].ability)) {
		return;
	}
	
	if (GET_MANA(ch) < ritual_data[rit].cost) {
		msg_to_char(ch, "You need %d mana to perform that %s.\r\n", ritual_data[rit].cost, ritual_scmd[subcmd]);
		return;
	}
	
	if (ritual_data[rit].begin) {
		result = (ritual_data[rit].begin)(ch, arg2, rit);
	}
	else {
		msg_to_char(ch, "That %s is not implemented.\r\n", ritual_scmd[subcmd]);
	}
	
	if (result) {
		GET_MANA(ch) -= ritual_data[rit].cost;
	}
}


ACMD(do_siphon) {
	char_data *vict = NULL;
	struct affected_type *af;
	int cost = 10;
	
	int levels[] = { 2, 5, 9 };
		
	if (!can_use_ability(ch, ABIL_SIPHON, MANA, cost, COOLDOWN_SIPHON)) {
		return;
	}
	if (get_cooldown_time(ch, COOLDOWN_SIPHON) > 0) {
		msg_to_char(ch, "Siphon is still on cooldown.\r\n");
		return;
	}
	
	// find target
	one_argument(argument, arg);
	if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
		return;
	}
	if (!*arg && !(vict = FIGHTING(ch))) {
		msg_to_char(ch, "Who would you like to cast that at?\r\n");
		return;
	}
	if (ch == vict) {
		msg_to_char(ch, "You wouldn't want to cast that on yourself.\r\n");
		return;
	}
	
	// check validity
	if (!can_fight(ch, vict)) {
		act("You can't attack $M!", FALSE, ch, NULL, vict, TO_CHAR);
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_SIPHON)) {
		return;
	}
	
	if (GET_MANA(vict) <= 0) {
		msg_to_char(ch, "Your target has no mana to siphon.\r\n");
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, COOLDOWN_SIPHON, 20, WAIT_COMBAT_SPELL);
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// counterspell??
	if (trigger_counterspell(vict) || AFF_FLAGGED(vict, AFF_IMMUNE_HIGH_SORCERY)) {
		act("You try to siphon mana from $N, but are deflected by a counterspell!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n tries to siphon mana from you, but it's deflected by your counterspell!", FALSE, ch, NULL, vict, TO_VICT);
		act("$n tries to siphon mana from $N, but it fails!", FALSE, ch, NULL, vict, TO_NOTVICT);
	}
	else {
		// succeed
	
		act("$N starts to glow violet as you shout the mana siphon hex at $M! You feel your own mana grow as you drain $S.", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n shouts something at you... The world takes on a violet glow and you feel your mana siphoned away.", FALSE, ch, NULL, vict, TO_VICT);
		act("$n shouts some kind of hex at $N, who starts to glow violet as mana flows away from $S skin!", FALSE, ch, NULL, vict, TO_NOTVICT);

		af = create_mod_aff(ATYPE_SIPHON, 4, APPLY_MANA_REGEN, CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_SIPHON), ch);
		affect_join(ch, af, 0);
		
		af = create_mod_aff(ATYPE_SIPHON_DRAIN, 4, APPLY_MANA_REGEN, -1 * CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_SIPHON), ch);
		affect_join(vict, af, 0);

		engage_combat(ch, vict, FALSE);
	}
	
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, ABIL_SIPHON, 15);
	}
}


ACMD(do_slow) {
	char_data *vict = NULL;
	struct affected_type *af;
	int cost = 20;
	
	int levels[] = { 0.5 MUD_HOURS, 0.5 MUD_HOURS, 1 MUD_HOURS };
		
	if (!can_use_ability(ch, ABIL_SLOW, MANA, cost, COOLDOWN_SLOW)) {
		return;
	}
	
	// find target
	one_argument(argument, arg);
	if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
		return;
	}
	if (!*arg && !(vict = FIGHTING(ch))) {
		msg_to_char(ch, "Who would you like to cast that at?\r\n");
		return;
	}
	if (ch == vict) {
		msg_to_char(ch, "You wouldn't want to cast that on yourself.\r\n");
		return;
	}
	
	// check validity
	if (!can_fight(ch, vict)) {
		act("You can't attack $M!", FALSE, ch, NULL, vict, TO_CHAR);
		return;
	}
	
	if (ABILITY_TRIGGERS(ch, vict, NULL, ABIL_SLOW)) {
		return;
	}
	
	charge_ability_cost(ch, MANA, cost, COOLDOWN_SLOW, 75, WAIT_COMBAT_SPELL);
	
	if (SHOULD_APPEAR(ch)) {
		appear(ch);
	}
	
	// counterspell??
	if (trigger_counterspell(vict) || AFF_FLAGGED(vict, AFF_IMMUNE_HIGH_SORCERY)) {
		act("You try to use a slow hex on $N, but $E deflects it!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n tries to hex you, but it's deflected by your counterspell!", FALSE, ch, NULL, vict, TO_VICT);
		act("$n tries to hex $N, but $E deflects it.", FALSE, ch, NULL, vict, TO_NOTVICT);
	}
	else {
		// succeed
	
		act("$N grows lethargic and starts to glow gray as you shout the slow hex at $M!", FALSE, ch, NULL, vict, TO_CHAR);
		act("$n shouts something at you... The world takes on a gray tone and you more lethargic.", FALSE, ch, NULL, vict, TO_VICT);
		act("$n shouts some kind of hex at $N, who starts to move sluggishly and starts to glow gray!", FALSE, ch, NULL, vict, TO_NOTVICT);
	
		af = create_flag_aff(ATYPE_SLOW, CHOOSE_BY_ABILITY_LEVEL(levels, ch, ABIL_SLOW), AFF_SLOW, ch);
		affect_join(vict, af, 0);

		engage_combat(ch, vict, FALSE);
	}
	
	if (can_gain_exp_from(ch, vict)) {
		gain_ability_exp(ch, ABIL_SLOW, 15);
	}
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
	else if (*arg && !(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
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
			act("$n closes $s eyes and says the word 'matkso', and you feel a sudden burst of vigor!", FALSE, ch, NULL, vict, TO_VICT);
			act("$n closes $s eyes and says the word 'matkso', and $N suddenly seems refreshed.", FALSE, ch, NULL, vict, TO_NOTVICT);
		}
		
		// check if vict is in combat
		fighting = (FIGHTING(vict) != NULL);
		for (iter = ROOM_PEOPLE(IN_ROOM(ch)); iter && !fighting; iter = iter->next_in_room) {
			if (FIGHTING(iter) == vict) {
				fighting = TRUE;
			}
		}

		gain = CHOOSE_BY_ABILITY_LEVEL(move_gain, ch, ABIL_VIGOR) + (bonus_per_intelligence * GET_INTELLIGENCE(ch));
		
		// bonus if not in combat
		if (!fighting) {
			gain *= 2;
		}
		
		GET_MOVE(vict) = MIN(GET_MAX_MOVE(vict), GET_MOVE(vict) + gain);
		
		// the cast_by on this is vict himself, because it is a penalty and this will block cleanse
		af = create_mod_aff(ATYPE_VIGOR, 1 MUD_HOURS, APPLY_MOVE_REGEN, -5, vict);
		affect_join(vict, af, 0);
		
		gain_ability_exp(ch, ABIL_VIGOR, 33.4);

		if (FIGHTING(vict) && !FIGHTING(ch)) {
			engage_combat(ch, FIGHTING(vict), FALSE);
		}
	}
}


 ///////////////////////////////////////////////////////////////////////////////
//// CHANTS ///////////////////////////////////////////////////////////////////

RITUAL_SETUP_FUNC(start_chant_of_druids) {
	if (!HAS_FUNCTION(IN_ROOM(ch), FNC_HENGE)) {
		msg_to_char(ch, "You can't perform the chant of druids unless you are at a henge.\r\n");
		return FALSE;
	}
	if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it.\r\n");
		return FALSE;
	}
	if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to finish it before you can perform the chant of druids.\r\n");
		return FALSE;
	}
	
	// OK
	start_ritual(ch, ritual);
	return TRUE;
}

RITUAL_FINISH_FUNC(perform_chant_of_druids) {
	if (CAN_GAIN_NEW_SKILLS(ch) && get_skill_level(ch, SKILL_NATURAL_MAGIC) == 0 && noskill_ok(ch, SKILL_NATURAL_MAGIC) && room_has_function_and_city_ok(IN_ROOM(ch), FNC_HENGE)) {
		msg_to_char(ch, "&gAs you finish the chant, you begin to see the weave of mana through nature...&0\r\n");
		set_skill(ch, SKILL_NATURAL_MAGIC, 1);
		SAVE_CHAR(ch);
	}
	else {
		msg_to_char(ch, "You have finished the chant.\r\n");
	}
}


RITUAL_SETUP_FUNC(start_chant_of_illusions) {
	static struct resource_data *illusion_res = NULL;
	if (!illusion_res) {
		add_to_resource_list(&illusion_res, RES_OBJECT, o_NOCTURNIUM_SPIKE, 1, 0);
		add_to_resource_list(&illusion_res, RES_OBJECT, o_IRIDESCENT_IRIS, 1, 0);
	}
	
	if (!IS_ROAD(IN_ROOM(ch)) || !IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't perform the chant of illusions here.\r\n");
		return FALSE;
	}
	if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES)) {
		msg_to_char(ch, "You don't have permission to perform that chant here.\r\n");
		return FALSE;
	}
	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_CHAMELEON)) {
		msg_to_char(ch, "This road is already cloaked in illusion.\r\n");
		return FALSE;
	}
	if (!has_resources(ch, illusion_res, TRUE, TRUE)) {
		// message sent by has_resources
		return FALSE;
	}
	
	// OK
	extract_resources(ch, illusion_res, TRUE, NULL);
	start_ritual(ch, ritual);
	return TRUE;
}

RITUAL_FINISH_FUNC(perform_chant_of_illusions) {
	if (!IS_ROAD(IN_ROOM(ch)) || !IS_COMPLETE(IN_ROOM(ch)) || !can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES)) {
		msg_to_char(ch, "You finish the chant, but it has no effect.\r\n");
		return;
	}
	
	SET_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_CHAMELEON);
	SET_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_CHAMELEON);
	msg_to_char(ch, "As you finish the chant, the road is cloaked in illusion!\r\n");
}


RITUAL_SETUP_FUNC(start_chant_of_nature) {
	if (!IS_APPROVED(ch) && config_get_bool("terraform_approval")) {
		send_config_msg(ch, "need_approval_string");
		return FALSE;
	}
	
	start_ritual(ch, ritual);
	return TRUE;
}

RITUAL_FINISH_FUNC(perform_chant_of_nature) {
	sector_data *new_sect, *preserve;
	struct evolution_data *evo;
	
	// percentage is checked in the evolution data
	if ((evo = get_evolution_by_type(SECT(IN_ROOM(ch)), EVO_MAGIC_GROWTH))) {
		new_sect = sector_proto(evo->becomes);
		preserve = (BASE_SECT(IN_ROOM(ch)) != SECT(IN_ROOM(ch))) ? BASE_SECT(IN_ROOM(ch)) : NULL;
		
		// messaging based on whether or not it's choppable
		if (new_sect && has_evolution_type(new_sect, EVO_CHOPPED_DOWN)) {
			msg_to_char(ch, "As you chant, a mighty tree springs from the ground!\r\n");
			act("As $n chants, a mighty tree springs from the ground!", FALSE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			msg_to_char(ch, "As you chant, the plants around you grow with amazing speed!\r\n");
			act("As $n chants, the plants around $m grow with amazing speed!", FALSE, ch, NULL, NULL, TO_ROOM);
		}
		
		change_terrain(IN_ROOM(ch), evo->becomes);
		if (preserve) {
			change_base_sector(IN_ROOM(ch), preserve);
		}
		
		remove_depletion(IN_ROOM(ch), DPLTN_PICK);
		remove_depletion(IN_ROOM(ch), DPLTN_FORAGE);
		
		gain_ability_exp(ch, ABIL_CHANT_OF_NATURE, 20);
	}
	else {
		gain_ability_exp(ch, ABIL_CHANT_OF_NATURE, 0.5);
	}
	
	// restart it automatically
	start_ritual(ch, ritual);
}


 ///////////////////////////////////////////////////////////////////////////////
//// RITUALS //////////////////////////////////////////////////////////////////


// used for rituals with no start requirements
RITUAL_SETUP_FUNC(start_simple_ritual) {
	start_ritual(ch, ritual);
	return TRUE;
}


RITUAL_FINISH_FUNC(perform_ritual_of_burdens) {
	struct affected_type *af;
	
	int burden_levels[] = { 6, 12, 18 };
	
	msg_to_char(ch, "You feel the weight of the world lift from your shoulders!\r\n");
	act("$n seems uplifted!", FALSE, ch, NULL, NULL, TO_ROOM);
	
	af = create_mod_aff(ATYPE_UNBURDENED, 24 MUD_HOURS, APPLY_INVENTORY, CHOOSE_BY_ABILITY_LEVEL(burden_levels, ch, ABIL_RITUAL_OF_BURDENS), ch);
	affect_join(ch, af, 0);	
	
	gain_ability_exp(ch, ABIL_RITUAL_OF_BURDENS, 25);
}


RITUAL_SETUP_FUNC(start_ritual_of_teleportation) {
	room_data *room, *next_room, *to_room = NULL, *map;
	struct empire_city_data *city;
	int subtype = NOWHERE;
	bool wait;
	
	if (!can_teleport_to(ch, IN_ROOM(ch), TRUE) || RMT_FLAGGED(IN_ROOM(ch), RMT_NO_TELEPORT) || ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_TELEPORT)) {
		msg_to_char(ch, "You can't teleport out of here.\r\n");
		return FALSE;
	}
	
	if (!*argument) {
		// random!
		subtype = NOWHERE;
	}
	else if (!str_cmp(argument, "home")) {
		HASH_ITER(hh, world_table, room, next_room) {
			if (ROOM_PRIVATE_OWNER(room) == GET_IDNUM(ch)) {
				to_room = room;
				break;
			}
		}

		if (!to_room) {
			msg_to_char(ch, "You have no home to which you could teleport.\r\n");
			return FALSE;
		}
		else if (get_cooldown_time(ch, COOLDOWN_TELEPORT_HOME) > 0) {
			msg_to_char(ch, "Your home teleportation is still on cooldown.\r\n");
			return FALSE;
		}
		else if (!(map = get_map_location_for(to_room))) {
			msg_to_char(ch, "You can't teleport home right now.\r\n");
			return FALSE;
		}
		else if (!can_use_room(ch, map, GUESTS_ALLOWED)) {
			msg_to_char(ch, "You can't teleport home because your home is somewhere you don't have permission to be.\r\n");
			return FALSE;
		}
		else if (!can_teleport_to(ch, map, FALSE)) {
			msg_to_char(ch, "You can't teleport home right now.\r\n");
			return FALSE;
		}
		else {
			subtype = GET_ROOM_VNUM(to_room);
		}
	}
	else if (has_ability(ch, ABIL_CITY_TELEPORTATION) && (city = find_city_by_name(GET_LOYALTY(ch), argument))) {
		subtype = GET_ROOM_VNUM(city->location);
		
		if (get_cooldown_time(ch, COOLDOWN_TELEPORT_CITY) > 0) {
			msg_to_char(ch, "Your city teleportation is still on cooldown.\r\n");
			return FALSE;
		}
		if (!is_in_city_for_empire(city->location, GET_LOYALTY(ch), TRUE, &wait)) {
			msg_to_char(ch, "That city was founded too recently to teleport to it.\r\n");
			return FALSE;
		}
	}
	else if (find_city_by_name(GET_LOYALTY(ch), argument)) {
		msg_to_char(ch, "You need to purchase the City Teleportation ability to do that.\r\n");
		return FALSE;
	}
	else {
		msg_to_char(ch, "That's not a valid place to teleport.\r\n");
		return FALSE;
	}
	
	start_ritual(ch, ritual);
	GET_ACTION_VNUM(ch, 1) = subtype;	// vnum 0 is ritual id
	return TRUE;
}


RITUAL_FINISH_FUNC(perform_ritual_of_teleportation) {
	void cancel_adventure_summon(char_data *ch);
	
	room_data *to_room, *rand_room, *map;
	int tries, rand_x, rand_y;
	bool random;
	
	to_room = real_room(GET_ACTION_VNUM(ch, 1));
	random = to_room ? FALSE : TRUE;
	
	// if there's no room, find a where
	tries = 0;
	while (!to_room && tries < 100) {
		// randomport!
		rand_x = number(0, 1) ? number(2, 7) : number(-7, -2);
		rand_y = number(0, 1) ? number(2, 7) : number(-7, -2);
		rand_room = real_shift(HOME_ROOM(IN_ROOM(ch)), rand_x, rand_y);
		
		// found outdoors!
		if (rand_room && !ROOM_IS_CLOSED(rand_room) && can_teleport_to(ch, rand_room, TRUE)) {
			to_room = rand_room;
		}
		++tries;
	}
	
	if (!to_room || !can_teleport_to(ch, to_room, TRUE) || !(map = get_map_location_for(to_room))) {
		msg_to_char(ch, "Teleportation failed: you couldn't find a safe place to teleport.\r\n");
	}
	else if (!can_teleport_to(ch, map, FALSE)) {
		msg_to_char(ch, "Teleportation failed: you can't seem to teleport there right now.\r\n");
	}
	else {
		act("$n vanishes in a brilliant flash of light!", FALSE, ch, NULL, NULL, TO_ROOM);
		char_to_room(ch, to_room);
		qt_visit_room(ch, IN_ROOM(ch));
		look_at_room_by_loc(ch, IN_ROOM(ch), NOBITS);
		act("$n appears with a brilliant flash of light!", FALSE, ch, NULL, NULL, TO_ROOM);
	
		// reset this in case they teleport onto a wall.
		GET_LAST_DIR(ch) = NO_DIR;
		
		// any existing adventure summon location is no longer valid after a voluntary teleport
		if (!random) {	// except random teleport
			cancel_adventure_summon(ch);
		}

		// trigger block	
		enter_wtrigger(IN_ROOM(ch), ch, NO_DIR);
		entry_memory_mtrigger(ch);
		greet_mtrigger(ch, NO_DIR);
		greet_memory_mtrigger(ch);
		greet_vtrigger(ch, NO_DIR);
	
		gain_ability_exp(ch, ABIL_RITUAL_OF_TELEPORTATION, 50);
	
		if (IS_CITY_CENTER(IN_ROOM(ch))) {
			gain_ability_exp(ch, ABIL_CITY_TELEPORTATION, 50);
			add_cooldown(ch, COOLDOWN_TELEPORT_CITY, 15 * SECS_PER_REAL_MIN);
		}
		else if (ROOM_PRIVATE_OWNER(IN_ROOM(ch)) == GET_IDNUM(ch)) {
			add_cooldown(ch, COOLDOWN_TELEPORT_HOME, 15 * SECS_PER_REAL_MIN);
		}
	}
}


RITUAL_FINISH_FUNC(perform_phoenix_rite) {
	struct affected_type *af;
	
	msg_to_char(ch, "The flames on the remaining candles shoot toward you and form the crest of the Phoenix!\r\n");
	act("The flames on the remaining candles shoot toward $n and form a huge fiery bird around $m!", FALSE, ch, NULL, NULL, TO_ROOM);

	af = create_mod_aff(ATYPE_PHOENIX_RITE, UNLIMITED, APPLY_NONE, 0, ch);
	affect_join(ch, af, 0);

	gain_ability_exp(ch, ABIL_PHOENIX_RITE, 10);
}


RITUAL_SETUP_FUNC(start_ritual_of_defense) {
	static struct resource_data *defense_res = NULL;
	bool found = FALSE;
	
	if (!defense_res) {
		add_to_resource_list(&defense_res, RES_OBJECT, o_IMPERIUM_SPIKE, 1, 0);
		add_to_resource_list(&defense_res, RES_OBJECT, o_BLOODSTONE, 1, 0);
	}
	
	// valid sects
	if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_BARRIER)) {
		found = TRUE;
	}

	if (!found) {
		msg_to_char(ch, "You can't perform the Ritual of Defense here.\r\n");
		return FALSE;
	}
	
	if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to finish it before you can cast Ritual of Defense.\r\n");
		return FALSE;
	}
	
	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_FLY)) {
		msg_to_char(ch, "The ritual has already been performed here.\r\n");
		return FALSE;
	}
	
	if (!has_resources(ch, defense_res, TRUE, TRUE)) {
		// message sent by has_resources
		return FALSE;
	}
	
	// OK: take resources
	extract_resources(ch, defense_res, TRUE, NULL);
	start_ritual(ch, ritual);
	return TRUE;
}


RITUAL_FINISH_FUNC(perform_ritual_of_defense) {
	msg_to_char(ch, "You finish the ritual and the walls take on a strange magenta glow!\r\n");
	act("$n finishes the ritual and the walls take on a strange magenta glow!", FALSE, ch, NULL, NULL, TO_ROOM);
	if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_FLY)) {
		gain_ability_exp(ch, ABIL_RITUAL_OF_DEFENSE, 25);
	}
	SET_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_NO_FLY);
	SET_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_NO_FLY);
}


RITUAL_FINISH_FUNC(perform_sense_life_ritual) {
	char_data *targ;
	bool found, earthmeld;
	
	msg_to_char(ch, "You finish the ritual and your eyes are opened...\r\n");
	act("$n finishes the ritual and $s eyes flash a bright white.", FALSE, ch, NULL, NULL, TO_ROOM);
	
	found = earthmeld = FALSE;
	for (targ = ROOM_PEOPLE(IN_ROOM(ch)); targ; targ = targ->next_in_room) {
		if (targ == ch) {
			continue;
		}
		
		if (AFF_FLAGGED(targ, AFF_HIDE)) {
			// hidden target
			SET_BIT(AFF_FLAGS(ch), AFF_SENSE_HIDE);

			if (CAN_SEE(ch, targ)) {
				act("You sense $N hiding here!", FALSE, ch, 0, targ, TO_CHAR);
				msg_to_char(targ, "You are discovered!\r\n");
				REMOVE_BIT(AFF_FLAGS(targ), AFF_HIDE);
				affects_from_char_by_aff_flag(targ, AFF_HIDE, FALSE);
				found = TRUE;
			}

			REMOVE_BIT(AFF_FLAGS(ch), AFF_SENSE_HIDE);
		}
		else if (!earthmeld && AFF_FLAGGED(targ, AFF_EARTHMELD)) {
			// earthmelded targets (only do once)
			if (skill_check(ch, ABIL_SEARCH, DIFF_HARD) && CAN_SEE(ch, targ)) {
				act("You sense someone earthmelded here.", FALSE, ch, NULL, NULL, TO_CHAR);
				found = earthmeld = TRUE;
			}
		}
	}
	
	if (!found) {
		msg_to_char(ch, "You sense no unseen presence.\r\n");
	}
	
	gain_ability_exp(ch, ABIL_SENSE_LIFE_RITUAL, 15);
}


RITUAL_SETUP_FUNC(start_ritual_of_detection) {
	bool wait;
	
	if (!GET_LOYALTY(ch)) {
		msg_to_char(ch, "You must be a member of an empire to do this.\r\n");
		return FALSE;
	}
	if (!is_in_city_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), TRUE, &wait)) {
		msg_to_char(ch, "You can only use the Ritual of Detection in one of your own cities%s.\r\n", wait ? " (this city was founded too recently)" : "");
		return FALSE;
	}

	start_ritual(ch, ritual);
	return TRUE;
}


RITUAL_FINISH_FUNC(perform_ritual_of_detection) {
	extern char *get_room_name(room_data *room, bool color);
	
	struct empire_city_data *city;
	descriptor_data *d;
	char_data *targ;
	bool found, wait;
	
	if (!GET_LOYALTY(ch)) {
		msg_to_char(ch, "The ritual fails as you aren't in any empire.\r\n");
	}
	else if (!is_in_city_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), TRUE, &wait) || !(city = find_city(GET_LOYALTY(ch), IN_ROOM(ch)))) {
		msg_to_char(ch, "The ritual fails as you aren't in one of your cities%s.\r\n", wait ? " (this city was founded too recently)" : "");
	}
	else {
		msg_to_char(ch, "You complete the Ritual of Detection...\r\n");
	
		found = FALSE;
		for (d = descriptor_list; d; d = d->next) {
			if (STATE(d) == CON_PLAYING && (targ = d->character) && targ != ch && !IS_NPC(targ) && !IS_IMMORTAL(targ)) {
				if (find_city(GET_LOYALTY(ch), IN_ROOM(targ)) == city) {
					found = TRUE;
					msg_to_char(ch, "You sense %s at (%d, %d) %s\r\n", PERS(targ, targ, FALSE), X_COORD(IN_ROOM(targ)), Y_COORD(IN_ROOM(targ)), get_room_name(IN_ROOM(targ), FALSE));
				}
			}
		}
		
		if (!found) {
			msg_to_char(ch, "You sense no other players in the city.\r\n");
		}
		gain_ability_exp(ch, ABIL_RITUAL_OF_DETECTION, 15);
	}				
}


RITUAL_SETUP_FUNC(start_siege_ritual) {
	extern bool find_siege_target_for_vehicle(char_data *ch, vehicle_data *veh, char *arg, room_data **room_targ, int *dir, vehicle_data **veh_targ);
	
	vehicle_data *veh_targ;
	room_data *room_targ;
	int dir;
	
	if (!*argument) {
		msg_to_char(ch, "Besiege which direction or vehicle?\r\n");
	}
	else if (!find_siege_target_for_vehicle(ch, NULL, argument, &room_targ, &dir, &veh_targ)) {
		// sends own messages
	}
	else {
		// ready:

		// trigger hostile immediately so they are attackable
		if (room_targ && ROOM_OWNER(room_targ) && ROOM_OWNER(room_targ) != GET_LOYALTY(ch)) {
			trigger_distrust_from_hostile(ch, ROOM_OWNER(room_targ));
		}
		if (veh_targ && VEH_OWNER(veh_targ) && VEH_OWNER(veh_targ) != GET_LOYALTY(ch)) {
			trigger_distrust_from_hostile(ch, VEH_OWNER(veh_targ));
		}
	
		start_ritual(ch, ritual);
		// action 0 is ritual #
		GET_ACTION_VNUM(ch, 1) = room_targ ? GET_ROOM_VNUM(room_targ) : NOTHING;
		GET_ACTION_VNUM(ch, 2) = veh_targ ? veh_script_id(veh_targ) : NOTHING;
		return TRUE;
	}
	
	return FALSE;
}


RITUAL_FINISH_FUNC(perform_siege_ritual) {
	void besiege_room(room_data *to_room, int damage);
	bool besiege_vehicle(vehicle_data *veh, int damage, int siege_type);
	extern vehicle_data *find_vehicle(int n);
	extern bool validate_siege_target_room(char_data *ch, vehicle_data *veh, room_data *to_room);
	extern bool validate_siege_target_vehicle(char_data *ch, vehicle_data *veh, vehicle_data *target);
	
	vehicle_data *veh_targ = NULL;
	room_data *room_targ = NULL;
	sector_data *secttype;
	int dam;
	
	// get the target back
	if (GET_ACTION_VNUM(ch, 1) != NOTHING) {
		room_targ = real_room(GET_ACTION_VNUM(ch, 1));
	}
	else if (GET_ACTION_VNUM(ch, 2) != NOTHING) {
		veh_targ = find_vehicle(GET_ACTION_VNUM(ch, 2));
		if (veh_targ && IN_ROOM(veh_targ) != IN_ROOM(ch)) {
			// must not really be valid
			veh_targ = NULL;
		}
	}
	
	if (!room_targ && !veh_targ) {
		msg_to_char(ch, "You don't seem to have a valid target, and the ritual fails.\r\n");
	}
	else if (room_targ && !validate_siege_target_room(ch, NULL, room_targ)) {
		// sends own message
	}
	else if (veh_targ && !validate_siege_target_vehicle(ch, NULL, veh_targ)) {
		// sends own message
	}
	else {
		if (SHOULD_APPEAR(ch)) {
			appear(ch);
		}
		
		dam = 1 + GET_INTELLIGENCE(ch);
		
		msg_to_char(ch, "You shoot one powerful fireball...\r\n");
		act("$n shoots one powerful fireball...", FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (room_targ) {
			secttype = SECT(room_targ);
			
			if (ROOM_OWNER(room_targ) && GET_LOYALTY(ch) != ROOM_OWNER(room_targ)) {
				trigger_distrust_from_hostile(ch, ROOM_OWNER(room_targ));
			}
			
			besiege_room(room_targ, dam);
			
			if (SECT(room_targ) != secttype) {
				msg_to_char(ch, "It is destroyed!\r\n");
				act("$n's target is destroyed!", FALSE, ch, NULL, NULL, TO_ROOM);
			}
		}
		else if (veh_targ) {
			if (VEH_OWNER(veh_targ) && GET_LOYALTY(ch) != VEH_OWNER(veh_targ)) {
				trigger_distrust_from_hostile(ch, VEH_OWNER(veh_targ));
			}
			
			besiege_vehicle(veh_targ, dam, SIEGE_MAGICAL);
		}
		
		gain_ability_exp(ch, ABIL_SIEGE_RITUAL, 33.4);
	}
}


RITUAL_SETUP_FUNC(start_devastation_ritual) {
	if (IS_ADVENTURE_ROOM(IN_ROOM(ch)) || !IS_OUTDOORS(ch)) {
		msg_to_char(ch, "You need to be outdoors to do perform the Devastation Ritual.\r\n");
		return FALSE;
	}
	if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to devastate here.\r\n");
		return FALSE;
	}

	start_ritual(ch, ritual);
	return TRUE;
}


RITUAL_FINISH_FUNC(perform_devastation_ritual) {
	extern void change_chop_territory(room_data *room);
	extern const sector_vnum climate_default_sector[NUM_CLIMATES];
	
	room_data *rand_room, *to_room = NULL;
	crop_data *cp;
	int dist, iter;
	int x, y;
	
	#define CAN_DEVASTATE(room)  ((ROOM_SECT_FLAGGED((room), SECTF_HAS_CROP_DATA) || (CAN_CHOP_ROOM(room) && get_depletion((room), DPLTN_CHOP) < config_get_int("chop_depletion"))) && !ROOM_AFF_FLAGGED((room), ROOM_AFF_HAS_INSTANCE))
	#define DEVASTATE_RANGE  3	// tiles

	// check this room
	if (CAN_DEVASTATE(IN_ROOM(ch)) && can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		to_room = IN_ROOM(ch);
	}
	
	// check surrounding rooms in star patter
	for (dist = 1; !to_room && dist <= DEVASTATE_RANGE; ++dist) {
		for (iter = 0; !to_room && iter < NUM_2D_DIRS; ++iter) {
			rand_room = real_shift(IN_ROOM(ch), shift_dir[iter][0] * dist, shift_dir[iter][1] * dist);
			if (rand_room && CAN_DEVASTATE(rand_room) && compute_distance(IN_ROOM(ch), rand_room) <= DEVASTATE_RANGE && can_use_room(ch, rand_room, MEMBERS_ONLY)) {
				to_room = rand_room;
			}
		}
	}
	
	// check max radius
	for (x = -DEVASTATE_RANGE; !to_room && x <= DEVASTATE_RANGE; ++x) {
		for (y = -DEVASTATE_RANGE; !to_room && y <= DEVASTATE_RANGE; ++y) {
			rand_room = real_shift(IN_ROOM(ch), x, y);
			if (rand_room && CAN_DEVASTATE(rand_room) && compute_distance(IN_ROOM(ch), rand_room) <= DEVASTATE_RANGE && can_use_room(ch, rand_room, MEMBERS_ONLY)) {
				to_room = rand_room;
			}
		}
	}
	
	// SUCCESS: distribute resources
	if (to_room) {
		if (ROOM_SECT_FLAGGED(to_room, SECTF_CROP) && (cp = ROOM_CROP(to_room)) && has_interaction(GET_CROP_INTERACTIONS(cp), INTERACT_HARVEST)) {
			run_room_interactions(ch, to_room, INTERACT_HARVEST, devastate_crop);
			run_room_interactions(ch, to_room, INTERACT_CHOP, devastate_trees);
			
			// check for original sect, which may have been stored
			if (BASE_SECT(to_room) != SECT(to_room)) {
				change_terrain(to_room, GET_SECT_VNUM(BASE_SECT(to_room)));
			}
			else {
				// fallback sect
				change_terrain(to_room, climate_default_sector[GET_CROP_CLIMATE(cp)]);
			}
		}
		else if (CAN_CHOP_ROOM(to_room) && get_depletion(to_room, DPLTN_CHOP) < config_get_int("chop_depletion")) {
			run_room_interactions(ch, to_room, INTERACT_CHOP, devastate_trees);
			change_chop_territory(to_room);
		}
		else if (ROOM_SECT_FLAGGED(to_room, SECTF_HAS_CROP_DATA) && (cp = ROOM_CROP(to_room))) {
			msg_to_char(ch, "You devastate the seeded field!\r\n");
			act("$n's powerful ritual devastates the seeded field!", FALSE, ch, NULL, NULL, TO_ROOM);
			
			// check for original sect, which may have been stored
			if (BASE_SECT(to_room) != SECT(to_room)) {
				change_terrain(to_room, GET_SECT_VNUM(BASE_SECT(to_room)));
			}
			else {
				// fallback sect
				change_terrain(to_room, climate_default_sector[GET_CROP_CLIMATE(cp)]);
			}
		}
		else {
			msg_to_char(ch, "The Devastation Ritual has failed.\r\n");
			return;
		}
	
		gain_ability_exp(ch, ABIL_DEVASTATION_RITUAL, 10);
		
		// auto-repeat
		if (GET_MANA(ch) >= ritual_data[ritual].cost) {
			GET_MANA(ch) -= ritual_data[ritual].cost;
			GET_ACTION(ch) = ACT_RITUAL;
			GET_ACTION_TIMER(ch) = 0;
			// other variables are still set up
		}
		else {
			msg_to_char(ch, "You do not have enough mana to continue the ritual.\r\n");
		}
	}
	else {
		msg_to_char(ch, "The Devastation Ritual is complete.\r\n");
	}
}
