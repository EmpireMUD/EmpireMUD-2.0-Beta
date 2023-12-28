/* ************************************************************************
*   File: skills.h                                        EmpireMUD 2.0b5 *
*  Usage: header file for classes, skills, and combat                     *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define EMPIRE_CHORE_SKILL_CAP  15  // how high you can gain some skills without using a yellow ability

#define GAINS_PER_ABILITY  10	// times you can gain skill from each ability -- TODO: should be a config?

// skill gain caps
#define CLASS_SKILL_CAP  100  // class skills -- TODO: rename MAX_SKILL_CAP ?
#define SPECIALTY_SKILL_CAP  75  // accessory skill
#define BASIC_SKILL_CAP  50  // common skills
#define IS_ANY_SKILL_CAP(ch, skill)  (get_skill_level((ch), (skill)) == SKILL_MAX_LEVEL(find_skill_by_vnum(skill)) || get_skill_level((ch), (skill)) == SPECIALTY_SKILL_CAP || get_skill_level((ch), (skill)) == BASIC_SKILL_CAP || (get_skill_level((ch), (skill)) == 0 && !CAN_GAIN_NEW_SKILLS(ch)))
#define NEXT_CAP_LEVEL(ch, skill)  (get_skill_level((ch), (skill)) <= BASIC_SKILL_CAP ? BASIC_SKILL_CAP : (get_skill_level((ch), (skill)) <= SPECIALTY_SKILL_CAP ? SPECIALTY_SKILL_CAP : (CLASS_SKILL_CAP)))

// skill > basic level
#define IS_SPECIALTY_ABILITY(ch, abil)	(get_ability_skill_level((ch), (abil)) > BASIC_SKILL_CAP)

// skill > specialty level
#define IS_CLASS_ABILITY(ch, abil)	(get_ability_skill_level((ch), (abil)) > SPECIALTY_SKILL_CAP)

#define CHOOSE_BY_ABILITY_LEVEL(arr, ch, abil)	(IS_CLASS_ABILITY((ch), (abil)) ? (arr)[2] : (IS_SPECIALTY_ABILITY((ch), (abil)) ? (arr)[1] : (arr)[0]))

// TODO move some of this to a config
#define ZEROES_REQUIRED_FOR_BONUS_SKILLS  1  // if you have this many BASIC skills at zero, you get BONUS_SPECIALTY_SKILLS_ALLOWED

#define NUM_CLASS_SKILLS_ALLOWED  2	// skills > SPECIALTY_SKILL_CAP
#define NUM_SPECIALTY_SKILLS_ALLOWED  3  // skills > BASIC_SKILL_CAP
#define BONUS_SPECIALTY_SKILLS_ALLOWED  1  // extra skills that can be > BASIC_SKILL_CAP if you meet ZEROES_REQUIRED_FOR_BONUS_SKILLS

// specific skill checks
#define CHECK_MAJESTY(ch)  (AFF_FLAGGED((ch), AFF_MAJESTY) && number(0, GET_CHARISMA(ch)))

// ability utils
#define PREP_ABIL(name)  void (name)(char_data *ch, ability_data *abil, char *argument, int level, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, struct ability_exec *data)
#define DO_ABIL(name)  void (name)(char_data *ch, ability_data *abil, char *argument, int level, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, struct ability_exec *data)

#define call_prep_abil(name)  (name)(ch, abil, argument, level, vict, ovict, vvict, room_targ, data)
#define call_do_abil(name)  (name)(ch, abil, argument, level, vict, ovict, vvict, room_targ, data)


// abilities.c prototypes
int get_ability_duration(char_data *ch, ability_data *abil);


// class.c prototypes
void assign_class_abilities(char_data *ch, class_data *cls, int role);
bool is_class_ability(ability_data *abil);
bool remove_vnum_from_class_abilities(struct class_ability **list, any_vnum vnum);
void update_class_and_abilities(char_data *ch);


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


// spells.c prototypes
bool trigger_counterspell(char_data *ch, char_data *triggered_by);


// DIFF_x: skill_check difficulties
#define DIFF_TRIVIAL  0
#define DIFF_EASY  1
#define DIFF_MEDIUM  2
#define DIFF_HARD  3
#define DIFF_RARELY  4
#define NUM_DIFF_TYPES  5


// SKILL_x: skill Skill vnums no longer appear in the code -- all skills are
// configured using the .skill editor in-game and any inherent properties are
// now controlled by skill flags.


// for ability definitions
#define NO_PREREQ  NO_ABIL


// ABIL_x: ability vnums
#define ABIL_NULL_MANA  5
#define ABIL_BOOST  7
#define ABIL_VAMP_COMMAND  9
#define ABIL_DISGUISE  13
#define ABIL_EARTHMELD  14
#define ABIL_WORM  16
#define ABIL_SNEAK  29
#define ABIL_RESCUE  93
#define ABIL_VEINTAP  107
#define ABIL_INSPIRE  136
#define ABIL_STEAL  143
#define ABIL_DAGGER_MASTERY  158
#define ABIL_STAFF_MASTERY  171
#define ABIL_MIRRORIMAGE  177
#define ABIL_CONFER  240
#define ABIL_HONE  262
#define ABIL_KITE  294
#define ABIL_BOWMASTER  295
#define ABIL_CHARGE  297


// TYPE_x: WEAPON ATTACK TYPES
// As of b5.166 these are edited with the .attack editor and do not need code
// consts unless they are special-cased in code.
#define TYPE_UNDEFINED  -1	// TODO rename these from TYPE_ to ATTACK_
#define TYPE_RESERVED  0
#define TYPE_STAB  4	// used by dagger mastery -- TODO convert weapon masteries
#define TYPE_HIT  15	// default physical -- TODO: should be a config
#define TYPE_MANA_BLAST  28	// default magical -- TODO: should be a config

// Special attacks (not available on weapons): these correspond to attack vnums
#define TYPE_SUFFERING			50	// used when players die from their wounds
#define ATTACK_GUARD_TOWER		51
#define ATTACK_EXECUTE			52	// used for death logging instead of attack type when executing
#define ATTACK_VAMPIRE_BITE		59	// bite command
#define ATTACK_PHYSICAL_DOT		61
#define ATTACK_BACKSTAB			62
#define ATTACK_MAGICAL_DOT		64
#define ATTACK_FIRE_DOT			65
#define ATTACK_POISON_DOT		66


// SIEGE_x: types for besiege
#define SIEGE_PHYSICAL  0
#define SIEGE_MAGICAL  1
#define SIEGE_BURNING  2


// ARMOR_x: armor types
#define ARMOR_MAGE  0
#define ARMOR_LIGHT  1
#define ARMOR_MEDIUM  2
#define ARMOR_HEAVY  3
#define NUM_ARMOR_TYPES  4


/* DAM_x: Damage types */
#define DAM_PHYSICAL  0
#define DAM_MAGICAL  1
#define DAM_FIRE  2
#define DAM_POISON  3
#define DAM_DIRECT  4


// FMODE_x: Fight modes
#define FMODE_MELEE  0	// Hand-to-hand combat
#define FMODE_MISSILE  1	// Ranged combat
#define FMODE_WAITING  2	// Fighting someone in ranged combat


// for restoration abilities: pool type can be any
#define ANY_POOL	-1


// RESCUE_x: Rescue message types
#define RESCUE_NO_MSG  0
#define RESCUE_RESCUE  1	// traditional rescue message
#define RESCUE_FOCUS  2		// mob changes focus


// constants.c externs that need skills.h
extern const char *armor_types[NUM_ARMOR_TYPES+1];
extern const double armor_scale_bonus[NUM_ARMOR_TYPES];
extern double skill_check_difficulty_modifier[NUM_DIFF_TYPES];


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
