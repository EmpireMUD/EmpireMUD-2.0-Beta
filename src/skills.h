/* ************************************************************************
*   File: skills.h                                        EmpireMUD 2.0b5 *
*  Usage: header file for classes, skills, and combat                     *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define EMPIRE_CHORE_SKILL_CAP  15  // how high you can gain some skills without using a yellow ability

#define GAINS_PER_ABILITY  10	// times you can gain skill from each ability

// skill gain caps
#define CLASS_SKILL_CAP  100  // class skills
#define SPECIALTY_SKILL_CAP  75  // accessory skill
#define BASIC_SKILL_CAP  50  // common skills
#define IS_ANY_SKILL_CAP(ch, skill)  (get_skill_level((ch), (skill)) == SKILL_MAX_LEVEL(find_skill_by_vnum(skill)) || get_skill_level((ch), (skill)) == SPECIALTY_SKILL_CAP || get_skill_level((ch), (skill)) == BASIC_SKILL_CAP || (get_skill_level((ch), (skill)) == 0 && !CAN_GAIN_NEW_SKILLS(ch)))
#define NEXT_CAP_LEVEL(ch, skill)  (get_skill_level((ch), (skill)) <= BASIC_SKILL_CAP ? BASIC_SKILL_CAP : (get_skill_level((ch), (skill)) <= SPECIALTY_SKILL_CAP ? SPECIALTY_SKILL_CAP : (CLASS_SKILL_CAP)))

// skill > basic level
#define IS_SPECIALTY_SKILL(ch, skill)	((IS_NPC(ch) ? get_approximate_level(ch) : get_skill_level((ch), (skill))) > BASIC_SKILL_CAP)
#define IS_SPECIALTY_ABILITY(ch, abil)	(get_ability_level((ch), (abil)) > BASIC_SKILL_CAP)

// skill > specialty level
#define IS_CLASS_SKILL(ch, skill)	((IS_NPC(ch) ? get_approximate_level(ch) : get_skill_level((ch), (skill))) > SPECIALTY_SKILL_CAP)
#define IS_CLASS_ABILITY(ch, abil)	(get_ability_level((ch), (abil)) > SPECIALTY_SKILL_CAP)

#define CHOOSE_BY_SKILL_LEVEL(arr, ch, skill)	(IS_CLASS_SKILL((ch), (skill)) ? (arr)[2] : (IS_SPECIALTY_SKILL((ch), (skill)) ? (arr)[1] : (arr)[0]))
#define CHOOSE_BY_ABILITY_LEVEL(arr, ch, abil)	(IS_CLASS_ABILITY((ch), (abil)) ? (arr)[2] : (IS_SPECIALTY_ABILITY((ch), (abil)) ? (arr)[1] : (arr)[0]))

// TODO move some of this to a config
#define ZEROES_REQUIRED_FOR_BONUS_SKILLS  1  // if you have this many BASIC skills at zero, you get BONUS_SPECIALTY_SKILLS_ALLOWED

#define NUM_CLASS_SKILLS_ALLOWED  2	// skills > SPECIALTY_SKILL_CAP
#define NUM_SPECIALTY_SKILLS_ALLOWED  3  // skills > BASIC_SKILL_CAP
#define BONUS_SPECIALTY_SKILLS_ALLOWED  1  // extra skills that can be > BASIC_SKILL_CAP if you meet ZEROES_REQUIRED_FOR_BONUS_SKILLS

// specific skill checks
#define CHECK_MAJESTY(ch)  (AFF_FLAGGED((ch), AFF_MAJESTY) && number(0, GET_CHARISMA(ch)))

// ability utils
#define PREP_ABIL(name)  void (name)(char_data *ch, ability_data *abil, int level, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, struct ability_exec *data)
#define DO_ABIL(name)  void (name)(char_data *ch, ability_data *abil, int level, char_data *vict, obj_data *ovict, vehicle_data *vvict, room_data *room_targ, struct ability_exec *data)

#define call_do_abil(name)  (name)(ch, abil, level, vict, ovict, vvict, room_targ, data)
#define call_prep_abil(name)  (name)(ch, abil, level, vict, ovict, vvict, room_targ, data)


// class.c prototypes
void assign_class_abilities(char_data *ch, class_data *cls, int role);
bool remove_vnum_from_class_abilities(struct class_ability **list, any_vnum vnum);
void update_class(char_data *ch);

// skills.c prototypes
// TODO sort this
void add_ability(char_data *ch, ability_data *abil, bool reset_levels);
void add_ability_by_set(char_data *ch, ability_data *abil, int skill_set, bool reset_levels);
void adjust_abilities_to_empire(char_data *ch, empire_data *emp, bool add);
bool can_gain_exp_from(char_data *ch, char_data *vict);
bool can_use_ability(char_data *ch, any_vnum ability, int cost_pool, int cost_amount, int cooldown_type);
bool can_wear_item(char_data *ch, obj_data *item, bool send_messages);
void charge_ability_cost(char_data *ch, int cost_pool, int cost_amount, int cooldown_type, int cooldown_time, int wait_type);
void check_ability_levels(char_data *ch, any_vnum skill);
bool check_can_gain_skill(char_data *ch, any_vnum skill_vnum);
void check_skill_sell(char_data *ch, ability_data *abil);
bool check_solo_role(char_data *ch);
void clear_char_abilities(char_data *ch, any_vnum skill);
int compute_bonus_exp_per_day(char_data *ch);
void empire_player_tech_skillup(empire_data *emp, int tech, double amount);
void empire_skillup(empire_data *emp, any_vnum ability, double amount);
void gain_ability_exp(char_data *ch, any_vnum ability, double amount);
void gain_player_tech_exp(char_data *ch, int tech, double amount);
bool gain_skill(char_data *ch, skill_data *skill, int amount, ability_data *from_abil);
bool gain_skill_exp(char_data *ch, any_vnum skill_vnum, double amount, ability_data *from_abil);
struct player_ability_data *get_ability_data(char_data *ch, any_vnum abil_id, bool add_if_missing);
int get_ability_level(char_data *ch, any_vnum ability);
int get_ability_points_available_for_char(char_data *ch, any_vnum skill);
int get_approximate_level(char_data *ch);
int get_attack_type_by_name(char *name);
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

// spells.c prototypes
bool trigger_counterspell(char_data *ch);


// DIFF_x: skill_check difficulties
#define DIFF_TRIVIAL  0
#define DIFF_EASY  1
#define DIFF_MEDIUM  2
#define DIFF_HARD  3
#define DIFF_RARELY  4
#define NUM_DIFF_TYPES  5


// SKILL_x: skill vnums
// Skill vnums no longer appear in the code -- all skills are configured using
// the .skill editor in-game.


// for ability definitions
#define NO_PREREQ  NO_ABIL


// ABIL_x: ability vnums
// #define ABIL_GIFT_OF_NATURE  0
// #define ABIL_ANCIENT_BLOOD  2
#define ABIL_BACKSTAB  3
// #define ABIL_WOLF_FORM  4
#define ABIL_NULL_MANA  5
// #define ABIL_READY_BLOOD_WEAPONS  6
#define ABIL_BOOST  7
#define ABIL_CLAWS  8
#define ABIL_VAMP_COMMAND  9
// #define ABIL_DARKNESS  11
#define ABIL_DEATHSHROUD  12
#define ABIL_DISGUISE  13
#define ABIL_EARTHMELD  14
#define ABIL_WORM  16
#define ABIL_SEARCH  18
// #define ABIL_DAYWALKING  19
#define ABIL_HIDE  20
// #define ABIL_HORRID_FORM  21
#define ABIL_DISPEL  22
// #define ABIL_MIST_FORM  24
#define ABIL_MUMMIFY  25
#define ABIL_REGENERATE  27
// #define ABIL_SUMMON_THUG  28
#define ABIL_SNEAK  29
#define ABIL_SOLAR_POWER  30
#define ABIL_SOULSIGHT  31
// #define ABIL_SUMMON_ANIMALS  34
// #define ABIL_SUMMON_HUMANS  35
#define ABIL_HEARTSTOP  36
#define ABIL_TASTE_BLOOD  37
#define ABIL_BLOOD_FORTITUDE  38
// formerly: #define ABIL_LOCKS  41
// formerly: #define ABIL_ROADS  49
// #define ABIL_STAMINA  66
#define ABIL_FISH  72
#define ABIL_TRACK  73
#define ABIL_RESIST_POISON  77
#define ABIL_PATHFINDING  83
// #define ABIL_REFLEXES  84
// #define ABIL_QUICK_BLOCK  86
#define ABIL_REFORGE  87
// #define ABIL_ENDURANCE  92
#define ABIL_RESCUE  93
#define ABIL_DISARM  94
#define ABIL_SPARRING  95
#define ABIL_KICK  96
#define ABIL_BASH  97
#define ABIL_CUT_DEEP  98
#define ABIL_BIG_GAME_HUNTER  99
#define ABIL_QUICK_DRAW  102
#define ABIL_FIRSTAID  103
#define ABIL_FLEET  104
#define ABIL_FINESSE  105
#define ABIL_STUNNING_BLOW  106
#define ABIL_VEINTAP  107
#define ABIL_HEAL  109
#define ABIL_HEAL_FRIEND  110
#define ABIL_HEAL_PARTY  111
#define ABIL_REJUVENATE  114
#define ABIL_PURIFY  115
#define ABIL_CLEANSE  116
// #define ABIL_READY_FIREBALL  117
#define ABIL_COUNTERSPELL  120
#define ABIL_ENTANGLE  121
// #define ABIL_FAMILIAR  122
#define ABIL_SKYBRAND  125
// #define ABIL_CHANT_OF_NATURE  126
// formerly: #define ABIL_REWARD  127
// formerly: #define ABIL_SUMMON_GUARDS  128
// formerly: #define ABIL_PROSPECT  130
// formerly: #define ABIL_WORKFORCE  131
// formerly: #define ABIL_RARE_METALS  133
// formerly: #define ABIL_COMMERCE  134
// formerly: #define ABIL_PROMINENCE  135
#define ABIL_INSPIRE  136
// #define ABIL_SUMMON_BODYGUARD  138
// formerly: #define ABIL_BARDE  139
// formerly: #define ABIL_CITY_LIGHTS  140
#define ABIL_STEAL  143
#define ABIL_ESCAPE  146
#define ABIL_CONCEALMENT  148
// #define ABIL_SHADOWSTEP  153
#define ABIL_JAB  155
#define ABIL_SAP  157
#define ABIL_DAGGER_MASTERY  158
#define ABIL_PRICK  161
// #define ABIL_BAT_FORM  162
// #define ABIL_RITUAL_OF_BURDENS  163
#define ABIL_MANASHIELD  165
// #define ABIL_SUMMON_SWIFT  168
// #define ABIL_SUMMON_MATERIALS  169
#define ABIL_STAFF_MASTERY  171
// #define ABIL_SIEGE_RITUAL  172
// #define ABIL_COLORBURST  173
#define ABIL_ENERVATE  174
// #define ABIL_SLOW  175
#define ABIL_SIPHON  176
#define ABIL_MIRRORIMAGE  177
// #define ABIL_PHOENIX_RITE  179
#define ABIL_DISENCHANT  180
#define ABIL_VIGOR  183
// #define ABIL_RITUAL_OF_DEFENSE  186
// #define ABIL_RITUAL_OF_TELEPORTATION  187
#define ABIL_PORTAL_MAGIC  189
#define ABIL_PORTAL_MASTER  190
// #define ABIL_DEVASTATION_RITUAL  191
// #define ABIL_SENSE_LIFE_RITUAL  192
// #define ABIL_RITUAL_OF_DETECTION  193
// #define ABIL_BASIC_CRAFTS  198	// no longer used in code
// formerly: #define ABIL_SKILLED_LABOR  201
#define ABIL_MASTER_SURVIVALIST  205
// formerly: #define ABIL_TUNNEL  206
// #define ABIL_ARCANE_POWER  207
#define ABIL_OUTRAGE  209
// #define ABIL_DREAD_BLOOD_FORM  211
// #define ABIL_SAVAGE_WEREWOLF_FORM  212
// #define ABIL_TOWERING_WEREWOLF_FORM  213
// #define ABIL_SAGE_WEREWOLF_FORM  214
// #define ABIL_ANIMAL_FORMS  215
#define ABIL_REFASHION  216
// formerly: #define ABIL_TRADE_ROUTES  217
#define ABIL_RESURRECT  219
#define ABIL_MOONRISE  220
#define ABIL_SANGUINE_RESTORATION  226
#define ABIL_NOBLE_BEARING  228
#define ABIL_BLOODSWEAT  229
#define ABIL_SHADOW_KICK  231
#define ABIL_STAGGER_JAB  232
#define ABIL_SHADOWCAGE  233
#define ABIL_HOWL  234
#define ABIL_CRUCIAL_JAB  235
#define ABIL_DIVERSION  236
#define ABIL_SHADOW_JAB  237
#define ABIL_ANCESTRAL_HEALING  239
#define ABIL_CONFER  240
// #define ABIL_EXARCH_CRAFTS  248
// #define ABIL_GRIFFIN  250
// #define ABIL_DIRE_WOLF  251
// #define ABIL_MOON_RABBIT  252
// #define ABIL_SPIRIT_WOLF  253
// #define ABIL_MANTICORE  254
// #define ABIL_PHOENIX  255
// #define ABIL_SCORPION_SHADOW  256
// #define ABIL_OWL_SHADOW  257
// #define ABIL_BASILISK  258
// #define ABIL_SALAMANDER  259
// #define ABIL_SKELETAL_HULK  260
// #define ABIL_BANSHEE  261
#define ABIL_HONE  262
// #define ABIL_CHANT_OF_ILLUSIONS  265
// #define ABIL_ASTRAL_WEREWOLF_FORM  267
#define ABIL_WHISPERSTRIDE 268
// #define ABIL_WEREWOLF_FORM  269
#define ABIL_STABLEMASTER  272
// formerly: #define ABIL_ERODE  280
// formerly: #define ABIL_SCOUR  281
#define ABIL_SHADOWLASH  282
#define ABIL_EVASION  286
#define ABIL_WEAPON_PROFICIENCY  287
#define ABIL_BITE  292
#define ABIL_KITE  294
#define ABIL_BOWMASTER  295
#define ABIL_TRICK_SHOTS  296
#define ABIL_CHARGE  297


// TYPE_x: WEAPON ATTACK TYPES
#define TYPE_UNDEFINED  -1
#define TYPE_RESERVED  0
#define TYPE_SLASH  1
#define TYPE_SLICE  2
#define TYPE_JAB  3
#define TYPE_STAB  4
#define TYPE_POUND  5
#define TYPE_HAMMER  6
#define TYPE_WHIP  7
#define TYPE_PICK  8
#define TYPE_BITE  9	// Animal-only
#define TYPE_CLAW  10	// Animal-only
#define TYPE_KICK  11	// Animal-only
#define TYPE_FIRE  12
#define TYPE_VAMPIRE_CLAWS  13
#define TYPE_CRUSH  14	// animal-only
#define TYPE_HIT  15	// default physical
#define TYPE_MAGIC_FIRE  16	// disarmable fire blast
#define TYPE_LIGHTNING_STAFF  17
#define TYPE_BURN_STAFF  18
#define TYPE_AGONY_STAFF  19
#define TYPE_MAGIC_FROST  20
#define TYPE_MAGIC_SHOCK  21
#define TYPE_MAGIC_LIGHT  22
#define TYPE_STING  23	// animal
#define TYPE_SWIPE  24	// animal
#define TYPE_TAIL_SWIPE  25	// animal
#define TYPE_PECK  26	// animal
#define TYPE_GORE  27	// animal
#define TYPE_MANA_BLAST  28	// default magical
#define TYPE_BOW  29	// shoot / bow
#define TYPE_CROSSBOW  30	// shoot / crossbow
#define TYPE_PISTOL  31	// shoot / pistol
#define TYPE_MUSKET  32	// shoot / musket
#define TYPE_FIRE_BREATH  33	// non-disarmable fire blast
#define TYPE_SLING  34	// shoot / sling
#define TYPE_SPEAR_THROWER  35	// shoot / spear-thrower
#define TYPE_ANIMAL_WHIP  36	// non-disarmable whip variant

#define NUM_ATTACK_TYPES  37	// total

// helpfulment
#define IS_WEAPON_TYPE(type) (((type) >= TYPE_RESERVED) && ((type) < TYPE_SUFFERING))
#define IS_MAGIC_ATTACK(type)  (attack_hit_info[(type)].damage_type == DAM_MAGICAL)


/* new attack types can be added here - up to TYPE_SUFFERING */
#define TYPE_SUFFERING				50


// Non-weapon attacks -- these are defined starting from TYPE_SUFFERING
// but they actually correspond to numbers in the combat message file, so
// it's not necessarily quick to convert them.
// TODO: need to move attack messages into an in-game editor + many of these could be messages on their respective abilities
#define ATTACK_GUARD_TOWER		(TYPE_SUFFERING + 1)
#define ATTACK_EXECUTE			(TYPE_SUFFERING + 2)
#define ATTACK_ARROW			(TYPE_SUFFERING + 3)
#define ATTACK_KICK				(TYPE_SUFFERING + 4)
#define ATTACK_BASH				(TYPE_SUFFERING + 5)
#define ATTACK_SUNBURN			(TYPE_SUFFERING + 6)
#define ATTACK_POISON			(TYPE_SUFFERING + 7)
	#define ATTACK_CREO_IGNEM		(TYPE_SUFFERING + 8)
#define ATTACK_VAMPIRE_BITE		(TYPE_SUFFERING + 9)
#define ATTACK_LIGHTNINGBOLT	(TYPE_SUFFERING + 10)	// 60
#define ATTACK_PHYSICAL_DOT		(TYPE_SUFFERING + 11)
#define ATTACK_BACKSTAB			(TYPE_SUFFERING + 12)
#define ATTACK_SUNSHOCK			(TYPE_SUFFERING + 13)
#define ATTACK_MAGICAL_DOT		(TYPE_SUFFERING + 14)
#define ATTACK_FIRE_DOT			(TYPE_SUFFERING + 15)
#define ATTACK_POISON_DOT		(TYPE_SUFFERING + 16)
#define ATTACK_ABLATE			(TYPE_SUFFERING + 17)
#define ATTACK_ACIDBLAST		(TYPE_SUFFERING + 18)
#define ATTACK_ARCLIGHT			(TYPE_SUFFERING + 19)
#define ATTACK_ASTRALCLAW		(TYPE_SUFFERING + 20)	// 70
#define ATTACK_CHRONOBLAST		(TYPE_SUFFERING + 21)
#define ATTACK_DEATHTOUCH		(TYPE_SUFFERING + 22)
#define ATTACK_DISPIRIT			(TYPE_SUFFERING + 23)
#define ATTACK_ERODE			(TYPE_SUFFERING + 24)	// currently unused
#define ATTACK_SCOUR			(TYPE_SUFFERING + 25)	// currently unused
#define ATTACK_SHADOWLASH		(TYPE_SUFFERING + 26)
#define ATTACK_SOULCHAIN		(TYPE_SUFFERING + 27)
#define ATTACK_STARSTRIKE		(TYPE_SUFFERING + 28)
#define ATTACK_THORNLASH		(TYPE_SUFFERING + 29)

#define TOTAL_ATTACK_TYPES		(TYPE_SUFFERING + 30)


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


// RESCUE_x: Rescue message types
#define RESCUE_NO_MSG  0
#define RESCUE_RESCUE  1	// traditional rescue message
#define RESCUE_FOCUS  2		// mob changes focus


// SPD_x: speeds for attack_hit_info.speed
#define SPD_FAST  0
#define SPD_NORMAL  1
#define SPD_SLOW  2
#define NUM_SPEEDS  3


// WEAPON_x: Weapon types
#define WEAPON_BLUNT  0
#define WEAPON_SHARP  1
#define WEAPON_MAGIC  2


// TYPE_ Attacktypes with grammar
struct attack_hit_type {
	const char *name;
	const char *first_pers;	// You "slash"
	const char *third_pers;	// $n "slashes"
	const char *noun;	// ... with your "swing"
	double speed[NUM_SPEEDS];	// { fast, normal, slow }
	int weapon_type;	// WEAPON_ type
	int damage_type;	// DAM_ type
	bool disarmable;	// whether or not disarm works
};


// passes data throughout an ability call
struct ability_exec {
	ability_data *abil;	// which ability (pointer)
	bool stop;	// indicates no further types should process
	bool success;	// indicates the player should be charged
	bool sent_any_msg;	// if TRUE, it sent at least one message to the actor
	bool no_msg;	// indicates you shouldn't send messages
	bool matching_role;	// if FALSE, no bonuses from matching role
	int cost;	// for types that raise the cost later
	bool should_charge_cost;	// if TRUE, will charge regardless of success
	
	struct ability_exec_type *types;	// LL of type data
};


// preliminary data from ability setup
struct ability_exec_type {
	bitvector_t type;
	double scale_points;
	struct ability_exec_type *next;
};


// constants.c externs that need skills.h
extern const char *armor_types[NUM_ARMOR_TYPES+1];
extern const double armor_scale_bonus[NUM_ARMOR_TYPES];
extern struct attack_hit_type attack_hit_info[NUM_ATTACK_TYPES];
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
