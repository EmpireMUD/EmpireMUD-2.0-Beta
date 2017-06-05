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
#define ZEROES_REQUIRED_FOR_BONUS_SKILLS  2  // if you have this many skills at zero, you get BONUS_SPECIALTY_SKILLS_ALLOWED

#define NUM_CLASS_SKILLS_ALLOWED  2	// skills > SPECIALTY_SKILL_CAP
#define NUM_SPECIALTY_SKILLS_ALLOWED  3  // skills > BASIC_SKILL_CAP
#define BONUS_SPECIALTY_SKILLS_ALLOWED  1  // extra skills that can be > BASIC_SKILL_CAP if you meet ZEROES_REQUIRED_FOR_BONUS_SKILLS

// specific skill checks
#define CHECK_MAJESTY(ch)  (AFF_FLAGGED((ch), AFF_MAJESTY) && number(0, GET_CHARISMA(ch)))


// protos
void add_ability(char_data *ch, ability_data *abil, bool reset_levels);
void adjust_abilities_to_empire(char_data *ch, empire_data *emp, bool add);
extern bool can_gain_exp_from(char_data *ch, char_data *vict);
extern bool can_use_ability(char_data *ch, any_vnum ability, int cost_pool, int cost_amount, int cooldown_type);
void charge_ability_cost(char_data *ch, int cost_pool, int cost_amount, int cooldown_type, int cooldown_time, int wait_type);
extern bool check_solo_role(char_data *ch);
void gain_ability_exp(char_data *ch, any_vnum ability, double amount);
extern bool gain_skill(char_data *ch, skill_data *skill, int amount);
extern bool gain_skill_exp(char_data *ch, any_vnum skill_vnum, double amount);
extern struct player_ability_data *get_ability_data(char_data *ch, any_vnum abil_id, bool add_if_missing);
extern int get_ability_level(char_data *ch, any_vnum ability);
extern int get_ability_points_available_for_char(char_data *ch, any_vnum skill);
extern int get_approximate_level(char_data *ch);
extern struct player_skill_data *get_skill_data(char_data *ch, any_vnum vnum, bool add_if_missing);
void mark_level_gained_from_ability(char_data *ch, ability_data *abil);
void remove_ability(char_data *ch, ability_data *abil, bool reset_levels);
void set_skill(char_data *ch, any_vnum skill, int level);
extern bool skill_check(char_data *ch, any_vnum ability, int difficulty);


// skill_check difficulties
#define DIFF_EASY  0
#define DIFF_MEDIUM  1
#define DIFF_HARD  2
#define DIFF_RARELY  3
#define NUM_DIFF_TYPES  4


// SKILL_x: skill vnums
#define SKILL_BATTLE  0
#define SKILL_EMPIRE  1
#define SKILL_HIGH_SORCERY  2
#define SKILL_NATURAL_MAGIC  3
#define SKILL_STEALTH  4
#define SKILL_SURVIVAL  5
#define SKILL_TRADE  6
#define SKILL_VAMPIRE  7


// for ability definitions
#define NO_PREREQ  NO_ABIL


// combo classes (class_data)
#define CLASS_NONE  0	// RESERVED
#define CLASS_DUKE  1
#define CLASS_BATTLEMAGE  2
#define CLASS_WEREWOLF  3
#define CLASS_ASSASSIN  4
#define CLASS_BARBARIAN  5
#define CLASS_STEELSMITH  6
#define CLASS_REAPER  7
#define CLASS_EXARCH  8
#define CLASS_LUMINARY  9
#define CLASS_POWERBROKER  10
#define CLASS_ELDER  11
#define CLASS_GUILDSMAN  12
#define CLASS_VAMPIRE_EMPIRE  13
#define CLASS_ARCHMAGE  14
#define CLASS_OCCULTIST  15
#define CLASS_THEURGE  16
#define CLASS_ARTIFICER  17
#define CLASS_LICH  18
#define CLASS_ORACLE  19
#define CLASS_MYSTIC  20
#define CLASS_ALCHEMIST  21
#define CLASS_NECROMANCER  22
#define CLASS_BANDIT  23
#define CLASS_SMUGGLER  24
#define CLASS_SHADE  25
#define CLASS_TINKER  26
#define CLASS_WIGHT  27
#define CLASS_ANTIQUARIAN  28

#define NUM_CLASSES  29	// total


// ABIL_x: ability vnums
#define ABIL_GIFT_OF_NATURE  0
#define ABIL_FLY  1
#define ABIL_ANCIENT_BLOOD  2
#define ABIL_BACKSTAB  3
#define ABIL_WOLF_FORM  4
#define ABIL_NULL_MANA  5
#define ABIL_READY_BLOOD_WEAPONS  6
#define ABIL_BOOST  7
#define ABIL_CLAWS  8
#define ABIL_COMMAND  9
#define ABIL_TOUCH_OF_FLAME  10
#define ABIL_DARKNESS  11
#define ABIL_DEATHSHROUD  12
#define ABIL_DISGUISE  13
#define ABIL_EARTHMELD  14
#define ABIL_EARTHARMOR  15
#define ABIL_WORM  16
#define ABIL_TERRIFY  17
#define ABIL_SEARCH  18
#define ABIL_DAYWALKING  19
#define ABIL_HIDE  20
#define ABIL_HORRID_FORM  21
#define ABIL_DISPEL  22
#define ABIL_MAJESTY  23
#define ABIL_MIST_FORM  24
#define ABIL_MUMMIFY  25
#define ABIL_TWO_HANDED_WEAPONS  26
#define ABIL_REGENERATE  27
#define ABIL_SUMMON_THUG  28
#define ABIL_SNEAK  29
#define ABIL_SOLAR_POWER  30
#define ABIL_SOULSIGHT  31
#define ABIL_SOULMASK  32
#define ABIL_ALACRITY  33
#define ABIL_SUMMON_ANIMALS  34
#define ABIL_SUMMON_HUMANS  35
#define ABIL_HEARTSTOP  36
#define ABIL_TASTE_BLOOD  37
#define ABIL_BLOOD_FORTITUDE  38
#define ABIL_ARTISANS  39
#define ABIL_HOUSING  40
#define ABIL_LOCKS  41
#define ABIL_LUXURY  42
#define ABIL_MONUMENTS  43
#define ABIL_RAISE_ARMIES  44
#define ABIL_SEAFARING  45
#define ABIL_HAVENS  46
#define ABIL_ADVANCED_TACTICS  47
#define ABIL_GRAND_MONUMENTS  48
#define ABIL_ROADS  49
#define ABIL_ADVANCED_SHIPS  50
#define ABIL_FORGE  51
#define ABIL_WEAPONCRAFTING  52
#define ABIL_ARMORSMITHING  53
#define ABIL_JEWELRY  54
#define ABIL_SEWING  55
#define ABIL_LEATHERWORKING  56
#define ABIL_POTTERY  57
#define ABIL_WOODWORKING  58
#define ABIL_ADVANCED_WOODWORKING  59
#define ABIL_SIEGEWORKS  60
#define ABIL_SHIPBUILDING  61
#define ABIL_SWIMMING  62
#define ABIL_FORAGE  63
#define ABIL_RIDE  64
#define ABIL_ALL_TERRAIN_RIDING  65
#define ABIL_STAMINA  66
#define ABIL_BY_MOONLIGHT  67
#define ABIL_NIGHTSIGHT  68
#define ABIL_FINDER  69
#define ABIL_FIND_HERBS  70
#define ABIL_HUNT  71
#define ABIL_FISH  72
#define ABIL_TRACK  73
#define ABIL_MASTER_TRACKER  74
#define ABIL_SATED_THIRST  75
#define ABIL_NO_TRACE  76
#define ABIL_RESIST_POISON  77
#define ABIL_POISON_IMMUNITY  78
#define ABIL_FIND_SHELTER  79
#define ABIL_NAVIGATION  80
#define ABIL_BUTCHER  81
#define ABIL_MOUNTAIN_CLIMBING  82
#define ABIL_PATHFINDING  83
#define ABIL_REFLEXES  84
#define ABIL_SHIELD_BLOCK  85
#define ABIL_QUICK_BLOCK  86
#define ABIL_REFORGE  87
#define ABIL_BLOCK_ARROWS  88
#define ABIL_LIGHT_ARMOR  89
#define ABIL_MEDIUM_ARMOR  90
#define ABIL_HEAVY_ARMOR  91
#define ABIL_ENDURANCE  92
#define ABIL_RESCUE  93
#define ABIL_DISARM  94
#define ABIL_SPARRING  95
#define ABIL_KICK  96
#define ABIL_BASH  97
#define ABIL_CUT_DEEP  98
#define ABIL_BIG_GAME_HUNTER  99
#define ABIL_MAGE_ARMOR  100
#define ABIL_ARCHERY  101
#define ABIL_QUICK_DRAW  102
#define ABIL_FIRSTAID  103
#define ABIL_FLEET  104
#define ABIL_FINESSE  105
#define ABIL_STUNNING_BLOW  106
#define ABIL_VEINTAP  107
#define ABIL_WEAKEN  108
#define ABIL_HEAL  109
#define ABIL_HEAL_FRIEND  110
#define ABIL_HEAL_PARTY  111
#define ABIL_HEALING_POTIONS  112
#define ABIL_HEALING_ELIXIRS  113
#define ABIL_REJUVENATE  114
#define ABIL_PURIFY  115
#define ABIL_CLEANSE  116
#define ABIL_READY_FIREBALL  117
#define ABIL_LIGHTNINGBOLT  118
#define ABIL_HASTEN  119
#define ABIL_COUNTERSPELL  120
#define ABIL_ENTANGLE  121
#define ABIL_FAMILIAR  122
#define ABIL_NATURE_POTIONS  123
#define ABIL_WRATH_OF_NATURE_POTIONS  124
#define ABIL_SKYBRAND  125
#define ABIL_CHANT_OF_NATURE  126
#define ABIL_REWARD  127
#define ABIL_SUMMON_GUARDS  128
#define ABIL_RADIANCE  129
#define ABIL_PROSPECT  130
#define ABIL_WORKFORCE  131
#define ABIL_DEEP_MINES  132
#define ABIL_RARE_METALS  133
#define ABIL_COMMERCE  134
#define ABIL_PROMINENCE  135
#define ABIL_INSPIRE  136
#define ABIL_CUSTOMIZE_BUILDING  137
#define ABIL_SUMMON_BODYGUARD  138
#define ABIL_BARDE  139
#define ABIL_CITY_LIGHTS  140
#define ABIL_GREAT_WALLS  141
#define ABIL_PICKPOCKET  142
#define ABIL_STEAL  143
#define ABIL_INFILTRATE  144
#define ABIL_IMPROVED_INFILTRATE  145
#define ABIL_ESCAPE  146
#define ABIL_APPRAISAL  147
#define ABIL_CONCEALMENT  148
#define ABIL_SECRET_CACHE  149
#define ABIL_CLING_TO_SHADOW  150
#define ABIL_UNSEEN_PASSING  151
#define ABIL_CLOAK_OF_DARKNESS  152
#define ABIL_SHADOWSTEP  153
#define ABIL_VAULTCRACKING  154
#define ABIL_JAB  155
#define ABIL_BLIND  156
#define ABIL_SAP  157
#define ABIL_DAGGER_MASTERY  158
#define ABIL_POISONS  159
#define ABIL_DEADLY_POISONS  160
#define ABIL_PRICK  161
#define ABIL_BAT_FORM  162
#define ABIL_RITUAL_OF_BURDENS  163
#define ABIL_STAFFMAKING  164
#define ABIL_MANASHIELD  165
#define ABIL_FORESIGHT  166
#define ABIL_ENCHANT_WEAPONS  167
#define ABIL_SUMMON_SWIFT  168
#define ABIL_SUMMON_MATERIALS  169
#define ABIL_POWERFUL_STAVES  170
#define ABIL_STAFF_MASTERY  171
#define ABIL_SIEGE_RITUAL  172
#define ABIL_COLORBURST  173
#define ABIL_ENERVATE  174
#define ABIL_SLOW  175
#define ABIL_SIPHON  176
#define ABIL_MIRRORIMAGE  177
#define ABIL_SUNSHOCK  178
#define ABIL_PHOENIX_RITE  179
#define ABIL_DISENCHANT  180
#define ABIL_GREATER_ENCHANTMENTS  181
#define ABIL_DANGEROUS_LEATHERS  182
#define ABIL_VIGOR  183
#define ABIL_ENCHANT_ARMOR  184
#define ABIL_ENCHANT_TOOLS  185
#define ABIL_RITUAL_OF_DEFENSE  186
#define ABIL_RITUAL_OF_TELEPORTATION  187
#define ABIL_CITY_TELEPORTATION  188
#define ABIL_PORTAL_MAGIC  189
#define ABIL_PORTAL_MASTER  190
#define ABIL_DEVASTATION_RITUAL  191
#define ABIL_SENSE_LIFE_RITUAL  192
#define ABIL_RITUAL_OF_DETECTION  193
#define ABIL_MASTER_TAILOR  194
#define ABIL_MASTER_BLACKSMITH  195
#define ABIL_DEADLY_WEAPONS  196
#define ABIL_IMPERIAL_ARMORS  197
#define ABIL_BASIC_CRAFTS  198
#define ABIL_MAGICAL_VESTMENTS  199
#define ABIL_HERB_GARDENS  200
#define ABIL_SKILLED_LABOR  201
#define ABIL_FINE_POTTERY  202
#define ABIL_MASTER_CRAFTSMAN  203
#define ABIL_MASTER_FARMER  204
#define ABIL_MASTER_SURVIVALIST  205
#define ABIL_TUNNEL  206
#define ABIL_ARCANE_POWER  207
#define ABIL_SWAMP_ENGINEERING  208
#define ABIL_OUTRAGE  209
#define ABIL_UNNATURAL_THIRST  210
#define ABIL_DREAD_BLOOD_FORM  211
#define ABIL_SAVAGE_WEREWOLF_FORM  212
#define ABIL_TOWERING_WEREWOLF_FORM  213
#define ABIL_SAGE_WEREWOLF_FORM  214
#define ABIL_ANIMAL_FORMS  215
#define ABIL_REFASHION  216
#define ABIL_TRADE_ROUTES  217
#define ABIL_DUAL_WIELD  218
#define ABIL_RESURRECT  219
#define ABIL_MOONRISE  220
#define ABIL_IRON_BLADES  221
#define ABIL_STURDY_ARMORS  222
#define ABIL_RAWHIDE_STITCHING  223
#define ABIL_MAGIC_ATTIRE  224
#define ABIL_PREDATOR_VISION  225
#define ABIL_SANGUINE_RESTORATION  226
#define ABIL_WARD_AGAINST_MAGIC  227
#define ABIL_NOBLE_BEARING  228
#define ABIL_BLOODSWEAT  229
#define ABIL_DRAGONRIDING  230
#define ABIL_SHADOW_KICK  231
#define ABIL_STAGGER_JAB  232
#define ABIL_SHADOWCAGE  233
#define ABIL_HOWL  234
#define ABIL_CRUCIAL_JAB  235
#define ABIL_DIVERSION  236
#define ABIL_SHADOW_JAB  237
#define ABIL_FASTCASTING  238
#define ABIL_ANCESTRAL_HEALING  239
#define ABIL_CONFER  240
#define ABIL_STEELSMITH_CRAFTS  241
#define ABIL_GUILDSMAN_CRAFTS  242
#define ABIL_ARTIFICER_CRAFTS  243
#define ABIL_ALCHEMIST_CRAFTS  244
#define ABIL_SMUGGLER_CRAFTS  245
#define ABIL_TINKER_CRAFTS  246
#define ABIL_ANTIQUARIAN_CRAFTS  247
#define ABIL_EXARCH_CRAFTS  248
#define ABIL_WORKFORCE_SAWING  249
#define ABIL_GRIFFIN  250
#define ABIL_DIRE_WOLF  251
#define ABIL_MOON_RABBIT  252
#define ABIL_SPIRIT_WOLF  253
#define ABIL_MANTICORE  254
#define ABIL_PHOENIX  255
#define ABIL_SCORPION_SHADOW  256
#define ABIL_OWL_SHADOW  257
#define ABIL_BASILISK  258
#define ABIL_SALAMANDER  259
#define ABIL_SKELETAL_HULK  260
#define ABIL_BANSHEE  261
#define ABIL_HONE  262
#define ABIL_HEALING_BOOST  263
#define ABIL_EXQUISITE_WOODWORKING  264
#define ABIL_CHANT_OF_ILLUSIONS  265
#define ABIL_ELDER_CRAFTS  266
#define ABIL_ASTRAL_WEREWOLF_FORM  267
#define ABIL_WHISPERSTRIDE 268
#define ABIL_WEREWOLF_FORM  269
#define ABIL_FAMILY_RECIPES  270
#define ABIL_GOURMET_CHEF  271
#define ABIL_STABLEMASTER  272
#define ABIL_ABLATE  273
#define ABIL_ACIDBLAST  274
#define ABIL_ARCLIGHT  275
#define ABIL_ASTRALCLAW  276
#define ABIL_CHRONOBLAST  277
#define ABIL_DEATHTOUCH  278
#define ABIL_DISPIRIT  279
#define ABIL_ERODE  280
#define ABIL_SCOUR  281
#define ABIL_SHADOWLASH  282
#define ABIL_SOULCHAIN  283
#define ABIL_STARSTRIKE  284
#define ABIL_THORNLASH  285
#define ABIL_EVASION  286
#define ABIL_WEAPON_PROFICIENCY  287
#define ABIL_PRIMITIVE_CRAFTS  288
#define ABIL_BASIC_BUILDINGS  289
#define ABIL_CHORES  290
#define ABIL_SCAVENGING  291
#define ABIL_BITE  292
#define ABIL_COOK  293


// cooldowns -- see COOLDOWN_x in constants.c
#define COOLDOWN_RESERVED  0	// do not change
#define COOLDOWN_DEATH_RESPAWN  1
#define COOLDOWN_LEFT_EMPIRE  2
#define COOLDOWN_HOSTILE_FLAG  3
#define COOLDOWN_PVP_FLAG  4
#define COOLDOWN_PVP_QUIT_TIMER  5
#define COOLDOWN_MILK  6
#define COOLDOWN_SHEAR  7
#define COOLDOWN_DISARM  8
#define COOLDOWN_OUTRAGE  9
#define COOLDOWN_RESCUE  10
#define COOLDOWN_KICK  11
#define COOLDOWN_BASH  12
#define COOLDOWN_COLORBURST  13
#define COOLDOWN_ENERVATE  14
#define COOLDOWN_SLOW  15
#define COOLDOWN_SIPHON  16
#define COOLDOWN_MIRRORIMAGE  17
#define COOLDOWN_SUNSHOCK  18
#define COOLDOWN_TELEPORT_HOME  19
#define COOLDOWN_TELEPORT_CITY  20
#define COOLDOWN_REJUVENATE  21
#define COOLDOWN_CLEANSE  22
#define COOLDOWN_LIGHTNINGBOLT  23
#define COOLDOWN_SKYBRAND  24
#define COOLDOWN_ENTANGLE  25
#define COOLDOWN_HEARTSTOP  26
#define COOLDOWN_SUMMON_HUMANS  27
#define COOLDOWN_SUMMON_ANIMALS  28
#define COOLDOWN_SUMMON_GUARDS  29
#define COOLDOWN_SUMMON_BODYGUARD  30
#define COOLDOWN_SUMMON_THUG  31
#define COOLDOWN_SUMMON_SWIFT  32
#define COOLDOWN_REWARD  33
#define COOLDOWN_SEARCH  34
#define COOLDOWN_TERRIFY  35
#define COOLDOWN_DARKNESS  36
#define COOLDOWN_SHADOWSTEP  37
#define COOLDOWN_BACKSTAB  38
#define COOLDOWN_JAB  39
#define COOLDOWN_BLIND  40
#define COOLDOWN_SAP  41
#define COOLDOWN_PRICK  42
#define COOLDOWN_WEAKEN  43
#define COOLDOWN_MOONRISE  44
#define COOLDOWN_ALTERNATE  45
#define COOLDOWN_DISPEL  46
#define COOLDOWN_BLOODSWEAT  47
#define COOLDOWN_EARTHMELD  48
#define COOLDOWN_SHADOWCAGE  49
#define COOLDOWN_HOWL  50
#define COOLDOWN_DIVERSION  51
#define COOLDOWN_ROGUE_FLAG  52
#define COOLDOWN_PORTAL_SICKNESS  53
#define COOLDOWN_WHISPERSTRIDE  54
#define COOLDOWN_ABLATE  55
#define COOLDOWN_ACIDBLAST  56
#define COOLDOWN_ARCLIGHT  57
#define COOLDOWN_ASTRALCLAW  58
#define COOLDOWN_CHRONOBLAST  59
#define COOLDOWN_DEATHTOUCH  60
#define COOLDOWN_DISPIRIT  61
#define COOLDOWN_ERODE  62
#define COOLDOWN_SCOUR  63
#define COOLDOWN_SHADOWLASH  64
#define COOLDOWN_SOULCHAIN  65
#define COOLDOWN_STARSTRIKE  66
#define COOLDOWN_THORNLASH  67


/* WEAPON ATTACK TYPES */
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
#define TYPE_MAGIC_FIRE  16
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

#define NUM_ATTACK_TYPES  29	// total

// helpfulment
#define IS_WEAPON_TYPE(type) (((type) >= TYPE_RESERVED) && ((type) < TYPE_SUFFERING))
#define IS_MAGIC_ATTACK(type)  (attack_hit_info[(type)].damage_type == DAM_MAGICAL)


/* new attack types can be added here - up to TYPE_SUFFERING */
#define TYPE_SUFFERING				50


// Non-weapon attacks -- these are defined starting from TYPE_SUFFERING
// but they actually correspond to numbers in the combat message file, so
// it's not necessarily quick to convert them.
#define ATTACK_GUARD_TOWER		(TYPE_SUFFERING + 1)
#define ATTACK_EXECUTE			(TYPE_SUFFERING + 2)
#define ATTACK_ARROW			(TYPE_SUFFERING + 3)
#define ATTACK_KICK				(TYPE_SUFFERING + 4)
#define ATTACK_BASH				(TYPE_SUFFERING + 5)
#define ATTACK_SUNBURN			(TYPE_SUFFERING + 6)
#define ATTACK_POISON			(TYPE_SUFFERING + 7)
#define ATTACK_CREO_IGNEM		(TYPE_SUFFERING + 8)
	#define ATTACK_UNUSED			(TYPE_SUFFERING + 9)
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
#define ATTACK_ERODE			(TYPE_SUFFERING + 24)
#define ATTACK_SCOUR			(TYPE_SUFFERING + 25)
#define ATTACK_SHADOWLASH		(TYPE_SUFFERING + 26)
#define ATTACK_SOULCHAIN		(TYPE_SUFFERING + 27)
#define ATTACK_STARSTRIKE		(TYPE_SUFFERING + 28)
#define ATTACK_THORNLASH		(TYPE_SUFFERING + 29)

#define TOTAL_ATTACK_TYPES		(TYPE_SUFFERING + 30)


// SIEGE_x types for besiege
#define SIEGE_PHYSICAL  0
#define SIEGE_MAGICAL  1
#define SIEGE_BURNING  2


// ATYPE_x: Affect types
#define ATYPE_RESERVED  0
#define ATYPE_FLY  1
#define ATYPE_ENTRANCEMENT  2
#define ATYPE_DARKNESS  3
#define ATYPE_POISON  4
#define ATYPE_BOOST  5
#define ATYPE_CUT_DEEP  6
#define ATYPE_SAP  7
#define ATYPE_MANASHIELD  8
#define ATYPE_FORESIGHT  9
#define ATYPE_EARTHMELD  10
#define ATYPE_MUMMIFY  11
#define ATYPE_EARTHARMOR  12
#define ATYPE_BESTOW_VIGOR  13	// NOTE: not used
#define ATYPE_WEAKEN  14
#define ATYPE_COLORBURST  15
#define ATYPE_HEARTSTOP  16
#define ATYPE_PHOENIX_RITE  17
#define ATYPE_DISARM  18
#define ATYPE_SHOCKED  19
#define ATYPE_SKYBRAND  20
#define ATYPE_COUNTERSPELL  21
#define ATYPE_HASTEN  22
#define ATYPE_REJUVENATE  23
#define ATYPE_ENTANGLE  24
#define ATYPE_RADIANCE  25
#define ATYPE_INSPIRE  26
#define ATYPE_JABBED  27
#define ATYPE_BLIND  28
#define ATYPE_REGEN_POTION  29
#define ATYPE_NATURE_POTION  30
#define ATYPE_VIGOR  31
#define ATYPE_ENERVATE  32
#define ATYPE_ENERVATE_GAIN  33
#define ATYPE_SIPHON  34
#define ATYPE_SLOW  35
#define ATYPE_SUNSHOCK  36
#define ATYPE_TRIPPING  37
#define ATYPE_SIPHON_DRAIN  38
#define ATYPE_DG_AFFECT  39
#define ATYPE_CLAWS  40
#define ATYPE_DEATHSHROUD  41
#define ATYPE_SOULMASK  42
#define ATYPE_MAJESTY  43
#define ATYPE_ALACRITY  44
#define ATYPE_NIGHTSIGHT  45
#define ATYPE_DEATH_PENALTY  46
#define ATYPE_BASH  47
#define ATYPE_TERRIFY  48
#define ATYPE_STUNNING_BLOW  49
#define ATYPE_STUN_IMMUNITY  50
#define ATYPE_WAR_DELAY  51
#define ATYPE_UNBURDENED  52
#define ATYPE_SHADOW_KICK  53
#define ATYPE_STAGGER_JAB  54
#define ATYPE_SHADOWCAGE  55
#define ATYPE_HOWL  56
#define ATYPE_CRUCIAL_JAB  57
#define ATYPE_DIVERSION  58
#define ATYPE_SHADOW_JAB  59
#define ATYPE_CONFER  60
#define ATYPE_CONFERRED  61
#define ATYPE_MORPH  62
#define ATYPE_WHISPERSTRIDE  63
#define ATYPE_WELL_FED  64
#define ATYPE_ABLATE  65
#define ATYPE_ACIDBLAST  66
#define ATYPE_ASTRALCLAW  67
#define ATYPE_CHRONOBLAST  68
#define ATYPE_DISPIRIT  69
#define ATYPE_ERODE  70
#define ATYPE_SCOUR  71
#define ATYPE_SHADOWLASH_BLIND  72
#define ATYPE_SHADOWLASH_DOT  73
#define ATYPE_SOULCHAIN  74
#define ATYPE_THORNLASH  75
#define ATYPE_ARROW_TO_THE_KNEE  76
#define ATYPE_HOSTILE_DELAY  77
#define ATYPE_NATURE_BURN  78

#define NUM_ATYPES  79	// total number, for bounds checking


// armor types
#define ARMOR_MAGE  0
#define ARMOR_LIGHT  1
#define ARMOR_MEDIUM  2
#define ARMOR_HEAVY  3
#define NUM_ARMOR_TYPES  4


/* Damage types */
#define DAM_PHYSICAL  0
#define DAM_MAGICAL  1
#define DAM_FIRE  2
#define DAM_POISON  3
#define DAM_DIRECT  4


// Fight modes
#define FMODE_MELEE  0	// Hand-to-hand combat
#define FMODE_MISSILE  1	// Ranged combat
#define FMODE_WAITING  2	// Fighting someone in ranged combat


// speeds for attack_hit_info.speed
#define SPD_FAST  0
#define SPD_NORMAL  1
#define SPD_SLOW  2
#define NUM_SPEEDS  3


// Weapon types
#define WEAPON_BLUNT  0
#define WEAPON_SHARP  1
#define WEAPON_MAGIC  2


// TYPE_x Attacktypes with grammar
struct attack_hit_type {
	const char *name;
	const char *singular;	// You "slash"
	const char *plural;	// $n "slashes"
	double speed[NUM_SPEEDS];	// { fast, normal, slow }
	int weapon_type;	// WEAPON_ type
	int damage_type;	// DAM_ type
	bool disarmable;	// whether or not disarm works
};


// skill and ability data
extern struct attack_hit_type attack_hit_info[NUM_ATTACK_TYPES];
extern const double missile_weapon_speed[];


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
