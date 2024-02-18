/* ************************************************************************
*   File: skills.h                                        EmpireMUD 2.0b5 *
*  Usage: header file for classes, skills, abilities, and combat          *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/**
* Contents:
*   Ability Definitions
*   Skill Definitions
*   Other Definitions
*   Constants
*   Ability Prototypes
*   Class Prototypes
*   Skill Prototypes
*   Inline Skill Functions
*/

 //////////////////////////////////////////////////////////////////////////////
//// ABILITY DEFINITIONS /////////////////////////////////////////////////////

#define GAINS_PER_ABILITY		10	// times you can gain skill from each ability -- TODO: should be a config?


// skill level caps: players stop at these levels and must specialize
#define MAX_SKILL_CAP			100	// top skill level
#define SPECIALTY_SKILL_CAP		75	// secondary skill
#define BASIC_SKILL_CAP			50	// common skills


// ABILF_x: ability flags
#define ABILF_VIOLENT				BIT(0)	// a. hostile ability (can't target self, etc)
#define ABILF_COUNTERSPELLABLE		BIT(1)	// b. can be counterspelled
#define ABILF_TOGGLE				BIT(2)	// c. can be toggled off by re-using (buffs)
#define ABILF_INVISIBLE				BIT(3)	// d. act messages don't show if char can't be seen
#define ABILF_NO_ENGAGE				BIT(4)	// e. won't cause you to enter combat
#define ABILF_RANGED				BIT(5)	// f. allows use in ranged combat
#define ABILF_NO_ANIMAL				BIT(6)	// g. can't be used in animal form
#define ABILF_NO_INVULNERABLE		BIT(7)	// h. can't be used in invulnerable form
#define ABILF_CASTER_ROLE			BIT(8)	// i. bonus if in 'caster' role
#define ABILF_HEALER_ROLE			BIT(9)	// j. bonus if in 'healer' role
#define ABILF_MELEE_ROLE			BIT(10)	// k. bonus if in 'melee' role
#define ABILF_TANK_ROLE				BIT(11)	// l. bonus if in 'tank' role
#define ABILF_RANGED_ONLY			BIT(12)	// m. requires ranged combat
#define ABILF_IGNORE_SUN			BIT(13)	// n. vampire ability ignores sunlight
#define ABILF_UNSCALED_BUFF			BIT(14)	// o. buff does not scale at all (fixed values)
#define ABILF_LIMIT_CROWD_CONTROL	BIT(15)	// p. cancels same buff on others in the room (using affectvnum)
#define ABILF_NOT_IN_COMBAT			BIT(16)	// q. prevents use in combat despite min-position
#define ABILF_ONE_AT_A_TIME			BIT(17)	// r. for some types, prevents them from being used while already active
#define ABILF_OVER_TIME				BIT(18)	// s. takes multiple turns, like a chant or ritual
#define ABILF_SPOKEN				BIT(19)	// t. ability is said out loud (blocked by the SILENT room affect)
#define ABILF_REPEAT_OVER_TIME		BIT(20)	// u. self-repeats an over-time ability if it succeeds
#define ABILF_CUMULATIVE_BUFF		BIT(21)	// v. buff stacks effect
#define ABILF_CUMULATIVE_DURATION	BIT(22)	// w. buff stacks duration
#define ABILF_WEAPON_HIT			BIT(23)	// x. involves a weapon hit (can poison)
#define ABILF_DIFFICULT_ANYWAY		BIT(24)	// y. ignores sleep/can't-see and checks difficulty anyway
#define ABILF_NOT_IN_DARK			BIT(25)	// z. must be able to see
#define ABILF_UNSCALED_PENALTY		BIT(26)	// A. negative buffs are unscaled; positive ones still scale
#define ABILF_STOP_ON_MISS			BIT(27)	// B. attack/damage miss will prevent buff and DoT types
#define ABILF_REDUCED_ON_EXTRA_TARGETS	BIT(28)	// C. for multi-targeting, scale is 50% on targets after the first
#define ABILF_USE_SKILL_BELOW_MAX	BIT(29)	// D. if the player is below maximum on the skill for the ability, limits it to that skill level
#define ABILF_UNREMOVABLE_BUFF		BIT(30)	// E. buff/debuff cannot be cleansed (marks victim as the caster)
#define ABILF_BUFF_SELF_NOT_TARGET	BIT(31)	// F. buff portion of the ability will go on the caster; everything else hits the victim
#define ABILF_STAY_HIDDEN			BIT(32)	// G. will not automatically un-hide
#define ABILF_BUFFS_COMMAND		    BIT(33)	// H. appears on the 'buffs' list

#define ABILITY_ROLE_FLAGS	(ABILF_CASTER_ROLE | ABILF_HEALER_ROLE | ABILF_MELEE_ROLE | ABILF_TANK_ROLE)

// ABILT_x: ability type flags
#define ABILT_CRAFT				BIT(0)	// a. related to crafting/building
#define ABILT_BUFF				BIT(1)	// b. applies an affect
#define ABILT_DAMAGE			BIT(2)	// c. deals damage
#define ABILT_DOT				BIT(3)	// d. damage over time effect
#define ABILT_PLAYER_TECH		BIT(4)	// e. some player tech feature
#define ABILT_PASSIVE_BUFF		BIT(5)	// f. similar to a buff except always on
#define ABILT_READY_WEAPONS		BIT(6)	// g. use READY-WEAPON data to add to a player's ready list
#define ABILT_COMPANION			BIT(7)	// h. grants companions
#define ABILT_SUMMON_ANY		BIT(8)	// i. player can summon from a list of mobs
#define ABILT_SUMMON_RANDOM		BIT(9)	// j. player can summon a mob at random from a list
#define ABILT_MORPH				BIT(10)	// k. ability has morphs that require it
#define ABILT_AUGMENT			BIT(11)	// l. related to augments/enchants
#define ABILT_CUSTOM			BIT(12)	// m. ability is hard-coded
#define ABILT_CONJURE_OBJECT	BIT(13)	// n. creates 1 or more items
#define ABILT_CONJURE_LIQUID	BIT(14)	// o. puts liquid in a drink container
#define ABILT_CONJURE_VEHICLE	BIT(15)	// p. creates a vehicle in the room
#define ABILT_ROOM_AFFECT		BIT(16)	// q. puts an affect on a room
#define ABILT_PAINT_BUILDING	BIT(17)	// r. applies color to the building
#define ABILT_ACTION			BIT(18)	// s. performs an action (from data list)
#define ABILT_BUILDING_DAMAGE	BIT(19)	// t. blasts a building or vehicle
#define ABILT_TELEPORT			BIT(20)	// u. relocates the player
#define ABILT_RESURRECT			BIT(21)	// v. resurrects a player
#define ABILT_RESOURCE			BIT(22)	// w. used for gathering resources; no built-in properties
#define ABILT_ATTACK			BIT(23)	// x. hits the target with one's own weapon attack
#define ABILT_RESTORE			BIT(24)	// y. restores pools (health, etc)
#define ABILT_MASTERY		    BIT(25)	// z. no inherent functions; just marks it as mastery of another one
#define ABILT_LINK				BIT(26)	// A. only serves to link between two other abilities
#define ABILT_MOVE				BIT(27)	// B. special type of move


// ATAR_x: ability targeting flags
#define ATAR_IGNORE			BIT(0)	// a. ignore target
#define ATAR_CHAR_ROOM		BIT(1)	// b. pc/npc in room
#define ATAR_CHAR_WORLD		BIT(2)	// c. pc/npc in the world
#define ATAR_CHAR_CLOSEST	BIT(3)	// d. closest pc/npc in the world
#define ATAR_FIGHT_SELF		BIT(4)	// e. if fighting and no arg, targets self
#define ATAR_FIGHT_VICT		BIT(5)	// f. if fighting and no arg, targets opponent
#define ATAR_SELF_ONLY		BIT(6)	// g. targets self if no arg, and only allows self
#define ATAR_NOT_SELF		BIT(7)	// h. target is any other char, always use with e.g. TAR_CHAR_ROOM
#define ATAR_OBJ_INV		BIT(8)	// i. object in inventory
#define ATAR_OBJ_ROOM		BIT(9)	// j. object in the room
#define ATAR_OBJ_WORLD		BIT(10)	// k. object in the world
#define ATAR_OBJ_EQUIP		BIT(11)	// l. object held/equipped
#define ATAR_VEH_ROOM		BIT(12)	// m. vehicle in the room
#define ATAR_VEH_WORLD		BIT(13)	// n. vehicle in the world
#define ATAR_ROOM_HERE		BIT(14)	// o. targets the room you're in
#define ATAR_ROOM_ADJACENT	BIT(15)	// p. targets in a direction
#define ATAR_ROOM_EXIT		BIT(16)	// q. targets the outside of the current place
#define ATAR_ROOM_HOME		BIT(17)	// r. targets the player's home room
#define ATAR_ROOM_RANDOM	BIT(18)	// s. targets a random room (combine with a RANGE limit)
#define ATAR_ROOM_CITY		BIT(19)	// t. targets a city from one's own empire
#define ATAR_ROOM_COORDS	BIT(20)	// u. targets a room by coordinates
#define ATAR_ROOM_NOT_HERE	BIT(21)	// v. modifier ensures it's not the current room
#define ATAR_STRING			BIT(22)	// w. string is expected and arg cannot be empty
#define ATAR_ALLIES_MULTI	BIT(23)	// x. all allies
#define ATAR_GROUP_MULTI	BIT(24)	// y. whole group
#define ATAR_ANY_MULTI		BIT(25)	// z. everyone in the room
#define ATAR_ENEMIES_MULTI	BIT(26)	// A. all enemies
#define ATAR_MULTI_CAN_SEE	BIT(27)	// B. modifies multi-targ to only hit visible people
#define ATAR_NOT_ALLY		BIT(28)	// C. cannot target an ally
#define ATAR_NOT_ENEMY		BIT(29)	// D. cannot target an enemy
#define ATAR_DEAD_OK		BIT(30)	// E. can target dead people
#define ATAR_ROOM_RANDOM_CAN_USE	BIT(31)	// F. similar to ROOM-RANDOM but checks use permission

#define CHAR_ATARS			(ATAR_CHAR_ROOM | ATAR_CHAR_WORLD | ATAR_CHAR_CLOSEST | ATAR_SELF_ONLY)
#define MULTI_CHAR_ATARS	(ATAR_ALLIES_MULTI | ATAR_GROUP_MULTI | ATAR_ANY_MULTI | ATAR_ENEMIES_MULTI)
#define OBJ_ATARS			(ATAR_OBJ_INV | ATAR_OBJ_ROOM | ATAR_OBJ_WORLD | ATAR_OBJ_EQUIP)
#define VEH_ATARS			(ATAR_VEH_ROOM | ATAR_VEH_WORLD)
#define ROOM_ATARS			(ATAR_ROOM_HERE | ATAR_ROOM_ADJACENT | ATAR_ROOM_EXIT | ATAR_ROOM_HOME | ATAR_ROOM_RANDOM | ATAR_ROOM_RANDOM_CAN_USE | ATAR_ROOM_CITY | ATAR_ROOM_COORDS)


// ABIL_ACTION_x: ability actions
#define ABIL_ACTION_DETECT_HIDE				0	// finds hidden characters
#define ABIL_ACTION_DETECT_EARTHMELD		1	// finds earthmelded characters
#define ABIL_ACTION_DETECT_PLAYERS_AROUND	2	// finds other players in/near the city
#define ABIL_ACTION_DETECT_ADVENTURES_AROUND	3	// finds detectable adventures near the player
#define ABIL_ACTION_DEVASTATE_AREA			4	// collects a chop or crop resource within 3 tiles
#define ABIL_ACTION_MAGIC_GROWTH			5	// triggers a magic growth evolution
#define ABIL_ACTION_CLOSE_PORTAL			6	// if item target is portal, closes it
#define ABIL_ACTION_APPLY_POISON			7	// applies your current poison to the target
#define ABIL_ACTION_REMOVE_PHYSICAL_DOTS	8	// removes physical-type DoT effects
#define ABIL_ACTION_REMOVE_MAGICAL_DOTS		9	// removes magical-type DoT effects
#define ABIL_ACTION_REMOVE_FIRE_DOTS		10	// removes fire-type DoT effects
#define ABIL_ACTION_REMOVE_POISON_DOTS		11	// removes poison-type DoT effects
#define ABIL_ACTION_REMOVE_ALL_DOTS			12	// removes all types or DoT effects
#define ABIL_ACTION_REMOVE_DEBUFFS			13	// removes debuffs
#define ABIL_ACTION_REMOVE_DRUNK			14	// removes drunk effects
#define ABIL_ACTION_TAUNT					15	// taunts the target to hit you
#define ABIL_ACTION_RESCUE_ONE				16	// rescues the target
#define ABIL_ACTION_RESCUE_ALL				17	// rescues the target
#define ABIL_ACTION_HIDE					18	// attempts a hide
#define ABIL_ACTION_PUT_TO_SLEEP			19	// target is put to sleep
#define ABIL_ACTION_DISENCHANT_OBJ			20	// disenchants an item
#define ABIL_ACTION_PURIFY					21	// removes skills with REMOVED-BY-PURIFY
#define ABIL_ACTION_CLOSE_TO_MELEE			22	// immediately drops from ranged combat to melee
#define ABIL_ACTION_SET_FIGHTING_TARGET		23	// ensures ch is hitting vict
#define ABIL_ACTION_PUSH_BACK_TO_RANGED_COMBAT	24	// for 'kite', pushes a character away from the user
#define ABIL_ACTION_LOOK_AT_ROOM			25	// forces a 'look'


// ABIL_CUSTOM_x: custom message types
#define ABIL_CUSTOM_SELF_TO_CHAR			0
#define ABIL_CUSTOM_SELF_TO_ROOM			1
#define ABIL_CUSTOM_TARGETED_TO_CHAR		2
#define ABIL_CUSTOM_TARGETED_TO_VICT		3
#define ABIL_CUSTOM_TARGETED_TO_ROOM		4
#define ABIL_CUSTOM_COUNTERSPELL_TO_CHAR	5
#define ABIL_CUSTOM_COUNTERSPELL_TO_VICT	6
#define ABIL_CUSTOM_COUNTERSPELL_TO_ROOM	7
#define ABIL_CUSTOM_FAIL_SELF_TO_CHAR		8
#define ABIL_CUSTOM_FAIL_SELF_TO_ROOM		9
#define ABIL_CUSTOM_FAIL_TARGETED_TO_CHAR	10
#define ABIL_CUSTOM_FAIL_TARGETED_TO_VICT	11
#define ABIL_CUSTOM_FAIL_TARGETED_TO_ROOM	12
#define ABIL_CUSTOM_PRE_SELF_TO_CHAR		13
#define ABIL_CUSTOM_PRE_SELF_TO_ROOM		14
#define ABIL_CUSTOM_PRE_TARGETED_TO_CHAR	15
#define ABIL_CUSTOM_PRE_TARGETED_TO_VICT	16
#define ABIL_CUSTOM_PRE_TARGETED_TO_ROOM	17
#define ABIL_CUSTOM_PER_VEH_TO_CHAR			18
#define ABIL_CUSTOM_PER_VEH_TO_ROOM			19
#define ABIL_CUSTOM_PER_ITEM_TO_CHAR		20
#define ABIL_CUSTOM_PER_ITEM_TO_ROOM		21
#define ABIL_CUSTOM_OVER_TIME_LONGDESC		22
#define ABIL_CUSTOM_TOGGLE_TO_CHAR			23
#define ABIL_CUSTOM_TOGGLE_TO_ROOM			24
#define ABIL_CUSTOM_PER_CHAR_TO_CHAR		25
#define ABIL_CUSTOM_PER_CHAR_TO_VICT		26
#define ABIL_CUSTOM_PER_CHAR_TO_ROOM		27
#define ABIL_CUSTOM_SPEC_TO_CHAR			28
#define ABIL_CUSTOM_SPEC_TO_VICT			29
#define ABIL_CUSTOM_SPEC_TO_ROOM			30
#define ABIL_CUSTOM_NO_ARGUMENT				31
#define ABIL_CUSTOM_OVER_TIME_SELF_TO_CHAR	32
#define ABIL_CUSTOM_OVER_TIME_SELF_TO_ROOM	33
#define ABIL_CUSTOM_OVER_TIME_TARG_TO_CHAR	34
#define ABIL_CUSTOM_OVER_TIME_TARG_TO_VICT	35
#define ABIL_CUSTOM_OVER_TIME_TARG_TO_ROOM	36
#define ABIL_CUSTOM_IMMUNE_SELF_TO_CHAR		37
#define ABIL_CUSTOM_IMMUNE_TARG_TO_CHAR		38
#define ABIL_CUSTOM_NO_TARGET				39
#define ABIL_CUSTOM_SELF_ONE_AT_AT_TIME		40
#define ABIL_CUSTOM_TARG_ONE_AT_AT_TIME		41


// ABIL_EFFECT_x: things that happen when an ability is used
#define ABIL_EFFECT_DISMOUNT				0	// player is dismounted
#define ABIL_EFFECT_DISTRUST_FROM_HOSTILE	1	// causes a distrust state with the target


// ADL_x: for ability_data_list (these are bit flags because one ability may have multiple types)
#define ADL_PLAYER_TECH		BIT(0)	// vnum will be PTECH_ types
#define ADL_EFFECT			BIT(1)	// an ABIL_EFFECT_ that happens when the ability is used
#define ADL_READY_WEAPON	BIT(2)	// adds to the "ready"
#define ADL_SUMMON_MOB		BIT(3)	// adds a mob to companions/summon-any/summon-random abilities (depending on skill type)
#define ADL_LIMITATION		BIT(4)	// special checks on the ability
#define ADL_PAINT_COLOR		BIT(5)	// for paint-building, etc
#define ADL_ACTION			BIT(6)	// has a specific action when successful
#define ADL_RANGE			BIT(7)	// some abilities can control range
#define ADL_PARENT			BIT(8)	// indicates this ability comes free with that one
#define ADL_SUPERCEDED_BY	BIT(9)	// if player has that ability, use it instead of this one


// AGH_x: ability gain hooks
#define AGH_ONLY_WHEN_AFFECTED	BIT(0)	// modifies other types: only when affected by this abil
#define AGH_MELEE				BIT(1)	// gains when actor hits in melee
#define AGH_RANGED				BIT(2)	// gains when actor hits at range
#define AGH_DODGE				BIT(3)	// gains when actor dodges
#define AGH_BLOCK				BIT(4)	// gains when actor blocks
#define AGH_TAKE_DAMAGE			BIT(5)	// gains when hit in melee
#define AGH_PASSIVE_FREQUENT	BIT(6)	// gains every "real update"
#define AGH_PASSIVE_HOURLY		BIT(7)	// gains every game hour
#define AGH_ONLY_DARK			BIT(8)	// only gains if it's dark
#define AGH_ONLY_LIGHT			BIT(9)	// only gains if it's light
#define AGH_ONLY_VS_ANIMAL		BIT(10)	// only if the target was an animal
#define AGH_VAMPIRE_FEEDING		BIT(11)	// gains when feeding
#define AGH_MOVING				BIT(12)	// gain when moving
#define AGH_ONLY_USING_READY_WEAPON		BIT(13)	// only gains if a ready-weapon is equipped
#define AGH_ONLY_USING_COMPANION		BIT(14)	// only if the player is using a companion from this ability
#define AGH_NOT_WHILE_ASLEEP	BIT(15)	// prevent gains while sleeping (or dying)
#define AGH_DYING				BIT(16)	// called when the player dies (before auto-resurrect)
#define AGH_DO_HEAL				BIT(17)	// called when the player heals someone
#define AGH_ONLY_INDOORS		BIT(18)	// only fires if indoors
#define AGH_ONLY_OUTDOORS		BIT(19)	// only fires if outdoors


// AHOOK_x: Ability hooks (things that cause an ability to run itself)
#define AHOOK_ABILITY			BIT(0)	// after another ability finishes
#define AHOOK_ATTACK			BIT(1)	// any attack
#define AHOOK_ATTACK_TYPE		BIT(2)	// specific attack type
#define AHOOK_DAMAGE_TYPE		BIT(3)	// specific damage type
#define AHOOK_KILL				BIT(4)	// any kill
#define AHOOK_MELEE_ATTACK		BIT(5)	// after any melee attack
#define AHOOK_RANGED_ATTACK		BIT(6)	// after any ranged attack
#define AHOOK_WEAPON_TYPE		BIT(7)	// blunt/sharp/etc
#define AHOOK_DYING				BIT(8)	// right before death
#define AHOOK_RESPAWN			BIT(9)	// character respawned
#define AHOOK_RESURRECT			BIT(10)	// character was resurrected
#define AHOOK_DAMAGE_ANY		BIT(11)	// any type of damage
#define AHOOK_ATTACK_MAGE		BIT(12)	// attack against a mage character
#define AHOOK_ATTACK_VAMPIRE	BIT(13)	// attack against a vampire character

#define COMBAT_AHOOKS	(AHOOK_ATTACK | AHOOK_ATTACK_TYPE | AHOOK_DAMAGE_TYPE | AHOOK_KILL | AHOOK_MELEE_ATTACK | AHOOK_RANGED_ATTACK | AHOOK_WEAPON_TYPE | AHOOK_DAMAGE_ANY | AHOOK_ATTACK_MAGE | AHOOK_ATTACK_VAMPIRE)


// ABIL_MOVE_x: Move ability types
#define ABIL_MOVE_NORMAL		0	// just moves them (can combine with affects)
#define ABIL_MOVE_EARTHMELD		1	// performs an earthmeld move


// ABIL_LIMIT_x: Limitations when trying to use an ability
#define ABIL_LIMIT_ON_BARRIER				0	// must be on a barrier
#define ABIL_LIMIT_OWN_TILE					1	// must own the tile
#define ABIL_LIMIT_CAN_USE_GUEST			2	// guest permission on tile
#define ABIL_LIMIT_CAN_USE_ALLY				3	// ally permission on tile
#define ABIL_LIMIT_CAN_USE_MEMBER			4	// member permission on tile
#define ABIL_LIMIT_ON_ROAD					5	// must be a road
#define ABIL_LIMIT_PAINTABLE_BUILDING		6	// building can be painted by a player
#define ABIL_LIMIT_IN_CITY					7	// must be in a city
#define ABIL_LIMIT_HAVE_EMPIRE				8	// must be a member of an empire
#define ABIL_LIMIT_INDOORS					9	// must be inside
#define ABIL_LIMIT_OUTDOORS					10	// must be outdoors (includes in adventures)
#define ABIL_LIMIT_ON_MAP					11	// won't work on interiors or adventures, but may include map buildings
#define ABIL_LIMIT_TERRAFORM_APPROVAL		12	// check player approval
#define ABIL_LIMIT_VALID_SIEGE_TARGET		13	// validate for siege damage
#define ABIL_LIMIT_NOT_DISTRACTED			14	// player must not have DISTRACTED aff
#define ABIL_LIMIT_NOT_IMMOBILIZED			15	// player must not have IMMOBILIZED aff
#define ABIL_LIMIT_CAN_TELEPORT_HERE		16	// player can teleport here
#define ABIL_LIMIT_WITHIN_RANGE				17	// enforces RANGE data on target
#define ABIL_LIMIT_NOT_GOD_TARGET			18	// cannot target gods/immortal
#define ABIL_LIMIT_GUEST_PERMISSION_AT_TARGET		19	// must have guest permission at the target location
#define ABIL_LIMIT_ALLY_PERMISSION_AT_TARGET		20	// must have ally permission at the target location
#define ABIL_LIMIT_MEMBER_PERMISSION_AT_TARGET		21	// must have member permission at the target location
#define ABIL_LIMIT_CAN_TELEPORT_TARGET				22	// target room is teleport-able
#define ABIL_LIMIT_TARGET_NOT_FOREIGN_EMPIRE_NPC	23	// check loyalty of target and forbid npcs from other empires
#define ABIL_LIMIT_NOT_HERE					24	// cannot target own room
#define ABIL_LIMIT_CHECK_CITY_FOUND_TIME	25	// check founded-too-recently
#define ABIL_LIMIT_ITEM_TYPE				26	// requires a specific item type
#define ABIL_LIMIT_WIELD_ANY_WEAPON			27	// checks for a weapon in the wield slot -- and not disarmed
#define ABIL_LIMIT_WIELD_ATTACK_TYPE		28	// wielding a weapon with a given attack
#define ABIL_LIMIT_WIELD_WEAPON_TYPE		29	// wielding a weapon with a given damage type
#define ABIL_LIMIT_NOT_BEING_ATTACKED		30	// no characters hitting actor
#define ABIL_LIMIT_DISARMABLE_TARGET		31	// target must be disarmable
#define ABIL_LIMIT_TARGET_HAS_MANA			32	// target must have a mana pool (non-caster mobs don't count)
#define ABIL_LIMIT_USING_ANY_POISON			33	// must have poison available
#define ABIL_LIMIT_TARGET_HAS_DOT_TYPE		34	// must be afflicted by a specific DoT type
#define ABIL_LIMIT_TARGET_HAS_ANY_DOT		35	// must be afflicted by a DoT
#define ABIL_LIMIT_TARGET_BEING_ATTACKED	36	// target has someone hitting them
#define ABIL_LIMIT_IN_ROLE					37	// char must be in a certain role (or solo)
#define ABIL_LIMIT_NO_WITNESSES				38	// nobody is watching
#define ABIL_LIMIT_NO_WITNESSES_HIDE		39	// nobody is watching, unless you're good at hiding
#define ABIL_LIMIT_CHECK_OBJ_BINDING		40	// ensures item is bound to actor or no one
#define ABIL_LIMIT_OBJ_FLAGGED				41	// checks for object flag present
#define ABIL_LIMIT_OBJ_NOT_FLAGGED			42	// checks for object flag absent
#define ABIL_LIMIT_TARGET_IS_VAMPIRE		43	// requires that the target be a vampire
#define ABIL_LIMIT_CAN_PURIFY_TARGET		44	// target can be purified
#define ABIL_LIMIT_IN_COMBAT				45	// must be in combat
#define ABIL_LIMIT_NOT_BEING_ATTACKED_MELEE	46	// no characters hitting actor in melee
#define ABIL_LIMIT_NOT_BEING_ATTACKED_MOBILE_MELEE	47	// no characters hitting actor in melee (except immobilized ones)
#define ABIL_LIMIT_NOT_AFFECTED_BY			48	// user not affected by aff flag
#define ABIL_LIMIT_TARGET_NOT_AFFECTED_BY	49	// target is not affected by aff flag
#define ABIL_LIMIT_NOT_LEADING_MOB			50	// user isn't leading a mob
#define ABIL_LIMIT_NOT_LEADING_VEHICLE		51	// user isn't leading a vehicle
#define ABIL_LIMIT_IS_AFFECTED_BY			52	// user is affected by an aff flag
#define ABIL_LIMIT_TARGET_IS_AFFECTED_BY	53	// target is affected by an aff flag
#define ABIL_LIMIT_TARGET_IS_HUMAN			54	// target must be human (mob or npc)


// ABLIM_x: data needed for ability limits:
#define ABLIM_NOTHING		0
#define ABLIM_ITEM_TYPE		1	// requires an ITEM_ const
#define ABLIM_ATTACK_TYPE	2	// requires an attack type
#define ABLIM_WEAPON_TYPE	3	// requires an weapon type (blunt, sharp, magic)
#define ABLIM_DAMAGE_TYPE	4	// requires a damage type (physical, magical, etc)
#define ABLIM_ROLE			5	// requires a role
#define ABLIM_OBJ_FLAG		6	// requires object flag
#define ABLIM_AFF_FLAG		7	// requires affect flag


// RUN_ABIL_x: modes for activating abilities
#define RUN_ABIL_NORMAL		BIT(0)	// normal command activation
#define RUN_ABIL_OVER_TIME	BIT(1)	// an over-time ability running its final code
#define RUN_ABIL_HOOKED		BIT(2)	// called from a hook (triggered) ability
#define RUN_ABIL_MULTI		BIT(3)	// running with multiple targets


 //////////////////////////////////////////////////////////////////////////////
//// CLASS DEFINITIONS ///////////////////////////////////////////////////////

// CLASSF_x: class flags
#define CLASSF_IN_DEVELOPMENT  BIT(0)	// a. not available to players


 //////////////////////////////////////////////////////////////////////////////
//// SKILL DEFINITIONS ///////////////////////////////////////////////////////

// SKILLF_x: skill flags
#define SKILLF_IN_DEVELOPMENT  BIT(0)	// a. not live, won't show up on skill lists
#define SKILLF_BASIC  BIT(1)	// b. always shows in the list
#define SKILLF_NO_SPECIALIZE  BIT(2)	// c. players must pass 50/75 via script/quest
#define SKILLF_VAMPIRE  BIT(3)	// d. players with this skill are considered vampires
#define SKILLF_CASTER  BIT(4)	// e. players with this skill are considered spellcasters/mages
#define SKILLF_REMOVED_BY_PURIFY  BIT(5)	// f. lose this skill if hit by the 'purify' spell


 //////////////////////////////////////////////////////////////////////////////
//// OTHER DEFINITIONS ///////////////////////////////////////////////////////


// for restoration abilities: pool type can be any
#define ANY_POOL	-1


// ARMOR_x: armor types
#define ARMOR_MAGE  0
#define ARMOR_LIGHT  1
#define ARMOR_MEDIUM  2
#define ARMOR_HEAVY  3
#define NUM_ARMOR_TYPES  4


// DAM_x: Damage types
#define DAM_PHYSICAL  0
#define DAM_MAGICAL  1
#define DAM_FIRE  2
#define DAM_POISON  3
#define DAM_DIRECT  4


// DIFF_x: skill_check difficulties
#define DIFF_TRIVIAL  0
#define DIFF_EASY  1
#define DIFF_MEDIUM  2
#define DIFF_HARD  3
#define DIFF_RARELY  4
#define NUM_DIFF_TYPES  5


// FMODE_x: Fight modes
#define FMODE_MELEE  0	// Hand-to-hand combat
#define FMODE_MISSILE  1	// Ranged combat
#define FMODE_WAITING  2	// Fighting someone in ranged combat


// RESCUE_x: Rescue message types
#define RESCUE_NO_MSG  0
#define RESCUE_RESCUE  1	// traditional rescue message
#define RESCUE_FOCUS  2		// mob changes focus


// ROLE_x: group roles for classes
#define ROLE_NONE  0	// placeholder
#define ROLE_TANK  1
#define ROLE_MELEE  2
#define ROLE_CASTER  3
#define ROLE_HEALER  4
#define ROLE_UTILITY  5
#define ROLE_SOLO  6
#define NUM_ROLES  7


// SIEGE_x: types for besiege
#define SIEGE_PHYSICAL  0
#define SIEGE_MAGICAL  1
#define SIEGE_BURNING  2


 //////////////////////////////////////////////////////////////////////////////
//// CONSTANTS AND VNUMS /////////////////////////////////////////////////////

// SKILL_x: skill Skill vnums no longer appear in the code -- all skills are
// configured using the .skill editor in-game and any inherent properties are
// now controlled by skill flags.


// for ability definitions
#define NO_PREREQ  NO_ABIL


// ABIL_x: ability vnums
#define ABIL_BOOST  7
#define ABIL_VAMP_COMMAND  9
#define ABIL_DISGUISE  13
#define ABIL_EARTHMELD  14
#define ABIL_INSPIRE  136
#define ABIL_DAGGER_MASTERY  158
#define ABIL_STAFF_MASTERY  171
#define ABIL_MIRRORIMAGE  177
#define ABIL_CONFER  240
#define ABIL_BOWMASTER  295


// ATTACK_x: WEAPON ATTACK TYPES (now data-driven and handled by the .attack editor)
// As of b5.166 these are edited with the .attack editor and do not need code
// consts unless they are special-cased in code.
#define ATTACK_UNDEFINED	-1
#define ATTACK_RESERVED		0
#define ATTACK_STAB			4	// used by dagger mastery -- TODO convert weapon masteries

// Special attacks (not available on weapons): these correspond to attack vnums
#define ATTACK_SUFFERING		50	// used when players die from their wounds
#define ATTACK_GUARD_TOWER		51
#define ATTACK_EXECUTE			52	// used for death logging instead of attack type when executing
#define ATTACK_VAMPIRE_BITE		59	// bite command
#define ATTACK_PHYSICAL_DOT		61
#define ATTACK_BACKSTAB			62
#define ATTACK_MAGICAL_DOT		64
#define ATTACK_FIRE_DOT			65
#define ATTACK_POISON_DOT		66


// constants.c externs that need skills.h
extern const char *armor_types[NUM_ARMOR_TYPES+1];
extern const double armor_scale_bonus[NUM_ARMOR_TYPES];
extern double skill_check_difficulty_modifier[NUM_DIFF_TYPES];


 //////////////////////////////////////////////////////////////////////////////
//// ABILITY PROTOTYPES //////////////////////////////////////////////////////

extern ability_data *ability_table;
extern ability_data *sorted_abilities;

#define ability_proto  find_ability_by_vnum	// why don't all the types have "type_proto()" functions?
void free_ability(ability_data *abil);
void free_trait_hooks(struct ability_trait_hook **hash);
char *get_ability_name_by_vnum(any_vnum vnum);
void remove_ability_from_table(ability_data *abil);

// ability lookups
ability_data *find_ability(char *argument);
ability_data *find_ability_by_name_exact(char *name, bool allow_abbrev);
#define find_ability_by_name(name)  find_ability_by_name_exact(name, TRUE)
ability_data *find_ability_by_vnum(any_vnum vnum);
ability_data *find_ability_on_skill(char *name, skill_data *skill);
ability_data *find_player_ability_by_tech(char_data *ch, int ptech);
ability_data *has_buff_ability_by_affect_and_affect_vnum(char_data *ch, bitvector_t aff_flag, any_vnum aff_vnum);

// ability macros
#define PREP_ABIL(name)  void (name)(char_data *ch, ability_data *abil, char *argument, int level, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, struct ability_exec *data)
#define DO_ABIL(name)  void (name)(char_data *ch, ability_data *abil, char *argument, int level, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, struct ability_exec *data)
#define call_prep_abil(name)  (name)(ch, abil, argument, level, vict, ovict, vvict, room_targ, data)
#define call_do_abil(name)  (name)(ch, abil, argument, level, vict, ovict, vvict, room_targ, data)

// abilities.c prototypes
void add_ability_gain_hook(char_data *ch, ability_data *abil);
void add_all_gain_hooks(char_data *ch);
void apply_ability_techs_to_player(char_data *ch, ability_data *abil);
void apply_all_ability_techs(char_data *ch);
void apply_one_passive_buff(char_data *ch, ability_data *abil);
bool check_ability(char_data *ch, char *string, bool exact);
void check_trait_hooks(char_data *ch);
bool delete_misc_from_ability_data_list(ability_data *abil, int type, any_vnum vnum, int misc);
bool delete_from_ability_data_list(ability_data *abil, int type, any_vnum vnum);
ability_data *find_ability_on_skill(char *name, skill_data *skill);
struct ability_data_list *find_ability_data_entry_for(ability_data *abil, int type, any_vnum vnum);
struct ability_data_list *find_ability_data_entry_for_misc(ability_data *abil, int type, any_vnum vnum, int misc);
void get_ability_type_display(struct ability_type *list, char *save_buffer, bool for_players);
int get_player_level_for_ability(char_data *ch, any_vnum abil_vnum);
bool has_ability_data_any(ability_data *abil, int type);
bool has_ability_hook(ability_data *abil, bitvector_t hook_type, int hook_value);
char_data *load_companion_mob(char_data *leader, struct companion_data *cd);
void perform_ability_command(char_data *ch, ability_data *abil, char *argument);
void read_ability_requirements();
void redetect_ability_targets_on_login(char_data *ch);
void refresh_passive_buffs(char_data *ch);
void remove_passive_buff(char_data *ch, struct affected_type *aff);
void remove_passive_buff_by_ability(char_data *ch, any_vnum abil);
void run_ability_gain_hooks(char_data *ch, char_data *opponent, bitvector_t trigger);
void run_ability_hooks(char_data *ch, bitvector_t hook_type, any_vnum hook_value, int level, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, bitvector_t multi_targ);
void run_ability_hooks_by_player_tech(char_data *ch, int tech, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ);
void send_ability_fail_messages(char_data *ch, char_data *vict, obj_data *ovict, ability_data *abil, struct ability_exec *data);
void send_ability_special_messages(char_data *ch, char_data *vict, obj_data *ovict, ability_data *abil, struct ability_exec *data, char **replace, int replace_count);
void send_pre_ability_messages(char_data *ch, char_data *vict, obj_data *ovict, ability_data *abil, struct ability_exec *data);
void setup_ability_companions(char_data *ch);
void show_ability_info(char_data *ch, ability_data *abil, ability_data *parent, char *outbuf, int sizeof_outbuf);


 //////////////////////////////////////////////////////////////////////////////
//// CLASS PROTOTYPES ////////////////////////////////////////////////////////

extern class_data *class_table;
extern class_data *sorted_classes;

class_data *find_class(char *argument);
class_data *find_class_by_name(char *name);
class_data *find_class_by_vnum(any_vnum vnum);
void free_class(class_data *cls);
void remove_class_from_table(class_data *cls);

// class.c prototypes
void assign_class_and_extra_abilities(char_data *ch, class_data *cls, int role);
bool is_class_ability(ability_data *abil);
bool remove_vnum_from_class_abilities(struct class_ability **list, any_vnum vnum);
void update_class_and_abilities(char_data *ch);


// skill data
extern skill_data *skill_table;
extern skill_data *sorted_skills;

// skills.c data prototypes
skill_data *find_skill(char *argument);
skill_data *find_skill_by_name_exact(char *name, bool allow_abbrev);
#define find_skill_by_name(name)  find_skill_by_name_exact(name, TRUE)
skill_data *find_skill_by_vnum(any_vnum vnum);
void free_skill(skill_data *skill);
char *get_skill_abbrev_by_vnum(any_vnum vnum);
char *get_skill_name_by_vnum(any_vnum vnum);
void remove_skill_from_table(skill_data *skill);

// skills.c prototypes
// TODO sort this
char *ability_color(char_data *ch, ability_data *abil);
void add_ability(char_data *ch, ability_data *abil, bool reset_levels);
void add_ability_by_set(char_data *ch, ability_data *abil, int skill_set, bool reset_levels);
bool can_gain_exp_from(char_data *ch, char_data *vict);
bool can_gain_skill_from(char_data *ch, ability_data *abil);
bool can_use_ability(char_data *ch, any_vnum ability, int cost_pool, int cost_amount, int cooldown_type);
bool can_wear_item(char_data *ch, obj_data *item, bool send_messages);
void charge_ability_cost(char_data *ch, int cost_pool, int cost_amount, int cooldown_type, int cooldown_time, int wait_type);
void check_ability_levels(char_data *ch, any_vnum skill);
bool check_can_gain_skill(char_data *ch, any_vnum skill_vnum);
void check_skill_sell(char_data *ch, ability_data *abil);
bool check_solo_role(char_data *ch);
void clear_char_abilities(char_data *ch, any_vnum skill);
int compute_bonus_exp_per_day(char_data *ch);
bool difficulty_check(int level, int difficulty);
void empire_player_tech_skillup(empire_data *emp, int tech, double amount);
void empire_skillup(empire_data *emp, any_vnum ability, double amount);
skill_data *find_assigned_skill(ability_data *abil, char_data *ch);
void gain_ability_exp(char_data *ch, any_vnum ability, double amount);
void gain_player_tech_exp(char_data *ch, int tech, double amount);
bool gain_skill(char_data *ch, skill_data *skill, int amount, ability_data *from_abil);
bool gain_skill_exp(char_data *ch, any_vnum skill_vnum, double amount, ability_data *from_abil);
struct player_ability_data *get_ability_data(char_data *ch, any_vnum abil_id, bool add_if_missing);
int get_ability_skill_level(char_data *ch, any_vnum ability);
int get_ability_points_available_for_char(char_data *ch, any_vnum skill);
int get_approximate_level(char_data *ch);
struct player_skill_data *get_skill_data(char_data *ch, any_vnum vnum, bool add_if_missing);
void give_level_zero_abilities(char_data *ch);
bool has_cooking_fire(char_data *ch);
int has_skill_flagged(char_data *ch, bitvector_t skill_flag);
void mark_level_gained_from_ability(char_data *ch, ability_data *abil);
void perform_npc_tie(char_data *ch, char_data *victim, int subcmd);
void perform_swap_skill_sets(char_data *ch);
void remove_ability(char_data *ch, ability_data *abil, bool reset_levels);
void remove_ability_by_set(char_data *ch, ability_data *abil, int skill_set, bool reset_levels);
bool remove_ability_from_synergy_abilities(struct synergy_ability **list, any_vnum abil_vnum);
bool remove_skills_by_flag(char_data *ch, bitvector_t skill_flag);
bool remove_vnum_from_skill_abilities(struct skill_ability **list, any_vnum vnum);
void set_skill(char_data *ch, any_vnum skill, int level);
bool skill_check(char_data *ch, any_vnum ability, int difficulty);
bool player_tech_skill_check(char_data *ch, int tech, int difficulty);
bool player_tech_skill_check_by_ability_difficulty(char_data *ch, int tech);


 //////////////////////////////////////////////////////////////////////////////
//// SPELL PROTOTYPES ////////////////////////////////////////////////////////

bool has_majesty_immunity(char_data *ch, char_data *attacker);
bool trigger_counterspell(char_data *ch, char_data *triggered_by);


 //////////////////////////////////////////////////////////////////////////////
//// INLINE SKILL FUNCTIONS //////////////////////////////////////////////////

/**
* @param char_data *ch A player.
* @param any_vnum skill Any valid skill number.
* @return double The player's experience in that skill.
*/
static inline double get_skill_exp(char_data *ch, any_vnum skill) {
	struct player_skill_data *sk = get_skill_data(ch, skill, 0);
	return sk ? sk->exp : 0.0;
}


/**
* @param char_data *ch A player.
* @param any_vnum skill Any valid skill number.
* @return int The player's level in that skill.
*/
static inline int get_skill_level(char_data *ch, any_vnum skill) {
	struct player_skill_data *sk = get_skill_data(ch, skill, 0);
	return sk ? sk->level : 0;
}


/**
* @param char_data *ch A player.
* @param any_vnum skill Any valid skill number.
* @return int The number of skill resets available.
*/
static inline int get_skill_resets(char_data *ch, any_vnum skill) {
	struct player_skill_data *sk = get_skill_data(ch, skill, 0);
	return sk ? sk->resets : 0;
}


/**
* @param char_data *ch The player to check.
* @param any_vnum abil_id Any valid ability.
* @param int skill_set Which skill set number (0..NUM_SKILL_SETS-1).
* @return bool TRUE if the player has the ability; FALSE if not.
*/
static inline bool has_ability_in_set(char_data *ch, any_vnum abil_id, int skill_set) {
	struct player_ability_data *data;
	
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	data = get_ability_data(ch, abil_id, 0);
	return data && data->purchased[skill_set];
}


/**
* Look up skill in the player's current set.
*
* @param char_data *ch The player to check.
* @param any_vnum abil_id Any valid ability.
* @return bool TRUE if the player has the ability; FALSE if not.
*/
static inline bool has_ability(char_data *ch, any_vnum abil_id) {
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	// GET_CURRENT_SKILL_SET(ch) not available here
	return has_ability_in_set(ch, abil_id, GET_CURRENT_SKILL_SET(ch));
}


/**
* @param char_data *ch The player to check.
* @param ability_data *abil Any valid ability.
* @return int The number of levels gained from that ability.
*/
static inline int levels_gained_from_ability(char_data *ch, ability_data *abil) {
	struct player_ability_data *data = get_ability_data(ch, abil->vnum, 0);
	return data ? data->levels_gained : 0;
}


/**
* @param char_data *ch A player.
* @param any_vnum skill Any valid skill number.
* @return bool TRUE if a player can gain skills, FALSE if not.
*/
static inline bool noskill_ok(char_data *ch, any_vnum skill) {
	struct player_skill_data *sk = get_skill_data(ch, skill, 0);
	return sk ? !sk->noskill : 1;
}


/**
* Tests if the character is at any of the skill cap levels (50, 75, 100) for
* a given skill.
*
* Formerly a macro.
*
* @param char_data *ch A player.
* @param any_vnum skill Which skill to test.
* @return bool TRUE if the character is at a cap; FALSE if not.
*/
static inline bool IS_ANY_SKILL_CAP(char_data *ch, any_vnum skill) {
	int level = get_skill_level(ch, skill);
	skill_data *sk = find_skill_by_vnum(skill);
	
	if (sk && level >= SKILL_MAX_LEVEL(sk)) {
		return TRUE;
	}
	else if (level == MAX_SKILL_CAP || level == SPECIALTY_SKILL_CAP || level == BASIC_SKILL_CAP) {
		return TRUE;
	}
	else if (level == 0 && !CAN_GAIN_NEW_SKILLS(ch)) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}


/**
* Determines what the next "cap level" is for a player on a given skill. These
* are level 50, 75, or 100 and may be limited if the skill has a low maximum.
* A character who is at one of those caps will return their current level.
*
* Formerly a macro.
*
* @param char_data *ch A player.
* @param any_vnum skill Which skill to check the next cap for.
* @return int The level of the next cap level.
*/
static inline int NEXT_CAP_LEVEL(char_data *ch, any_vnum skill) {
	int level = get_skill_level(ch, skill);
	skill_data *sk = find_skill_by_vnum(skill);
	
	if (level <= BASIC_SKILL_CAP && (!sk || BASIC_SKILL_CAP < SKILL_MAX_LEVEL(sk))) {
		return BASIC_SKILL_CAP;
	}
	else if (level <= SPECIALTY_SKILL_CAP && (!sk || SPECIALTY_SKILL_CAP < SKILL_MAX_LEVEL(sk))) {
		return SPECIALTY_SKILL_CAP;
	}
	else if (sk && level <= SKILL_MAX_LEVEL(sk)) {
		return SKILL_MAX_LEVEL(sk);
	}
	else {
		return MAX_SKILL_CAP;
	}
}
