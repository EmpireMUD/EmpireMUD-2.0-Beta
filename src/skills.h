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

#define GAINS_PER_ABILITY		10	// times you can gain skill from each ability -- TODO: should be a config?

// skill level caps: players stop at these levels and must specialize
#define MAX_SKILL_CAP			100	// top skill level
#define SPECIALTY_SKILL_CAP		75	// secondary skill
#define BASIC_SKILL_CAP			50	// common skills


// specific skill checks
#define CHECK_MAJESTY(ch)  (AFF_FLAGGED((ch), AFF_MAJESTY) && number(0, GET_CHARISMA(ch)))


 //////////////////////////////////////////////////////////////////////////////
//// CONSTANTS ///////////////////////////////////////////////////////////////

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
#define ABIL_DAGGER_MASTERY  158
#define ABIL_STAFF_MASTERY  171
#define ABIL_MIRRORIMAGE  177
#define ABIL_CONFER  240
#define ABIL_HONE  262
#define ABIL_KITE  294
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


// DAM_x: Damage types
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
//// ABILITIES ///////////////////////////////////////////////////////////////

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
//// CLASSES /////////////////////////////////////////////////////////////////

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
//// SPELLLS /////////////////////////////////////////////////////////////////

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
