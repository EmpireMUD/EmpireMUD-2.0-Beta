/* ************************************************************************
*   File: utils.h                                         EmpireMUD 2.0b5 *
*  Usage: header file: utility macros and prototypes of utility funcs     *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/**
* Contents:
*   Core Utils
*   Ability Utils
*   Adventure Utils
*   Archetype Utils
*   Augment Utils
*   Bitvector Utils
*   Building Utils
*   Can See Utils
*   Can See Obj Utils
*   Character Utils
*   Class Utils
*   Craft Utils
*   Crop Utils
*   Descriptor Utils
*   Empire Utils
*   Faction Utils
*   Fight Utils
*   Global Utils
*   Map Utils
*   Memory Utils
*   Mobile Utils
*   Morph Utils
*   Object Utils
*   Objval Utils
*   Player Utils
*   Quest Utils
*   Room Utils
*   Room Template Utils
*   Sector Utils
*   Skill Utils
*   Social Utils
*   String Utils
*   Vehicle Utils
*   Const Externs
*   Util Function Protos
*   Miscellaneous Utils
*   Consts for utils.c
*/

 //////////////////////////////////////////////////////////////////////////////
//// CORE UTILS //////////////////////////////////////////////////////////////

/* there could be some strange OS which doesn't have NULL... */
#ifndef NULL
	#define NULL (void *)0
#endif

#if !defined(FALSE)
	#define FALSE 0
#endif

#if !defined(TRUE)
	#define TRUE  (!FALSE)
#endif


/* defines for fseek */
#ifndef SEEK_SET
#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2
#endif


/*
 * NOCRYPT can be defined by an implementor manually in sysdep.h.
 * EMPIRE_CRYPT is a variable that the 'configure' script
 * automatically sets when it determines whether or not the system is
 * capable of encrypting.
 */
#if defined(NOCRYPT) || !defined(EMPIRE_CRYPT)
	#define CRYPT(a, b) (a)
#else
	#define CRYPT(a, b) ((char *) crypt((a),(b)))
#endif


 //////////////////////////////////////////////////////////////////////////////
//// ABILITY UTILS ///////////////////////////////////////////////////////////

#define ABIL_ASSIGNED_SKILL(abil)  ((abil)->assigned_skill)
#define ABIL_FLAGS(abil)  ((abil)->flags)
#define ABIL_MASTERY_ABIL(abil)  ((abil)->mastery_abil)
#define ABIL_NAME(abil)  ((abil)->name)
#define ABIL_SKILL_LEVEL(abil)  ((abil)->skill_level)
#define ABIL_VNUM(abil)  ((abil)->vnum)

// utils
#define ABILITY_FLAGGED(abil, flag)  IS_SET(ABIL_FLAGS(abil), (flag))


 //////////////////////////////////////////////////////////////////////////////
//// ADVENTURE UTILS /////////////////////////////////////////////////////////

#define GET_ADV_VNUM(adv)  ((adv)->vnum)
#define GET_ADV_NAME(adv)  ((adv)->name)
#define GET_ADV_AUTHOR(adv)  ((adv)->author)
#define GET_ADV_DESCRIPTION(adv)  ((adv)->description)
#define GET_ADV_START_VNUM(adv)  ((adv)->start_vnum)
#define GET_ADV_END_VNUM(adv)  ((adv)->end_vnum)
#define GET_ADV_MIN_LEVEL(adv)  ((adv)->min_level)
#define GET_ADV_MAX_LEVEL(adv)  ((adv)->max_level)
#define GET_ADV_MAX_INSTANCES(adv)  ((adv)->max_instances)
#define GET_ADV_RESET_TIME(adv)  ((adv)->reset_time)
#define GET_ADV_FLAGS(adv)  ((adv)->flags)
#define GET_ADV_LINKING(adv)  ((adv)->linking)
#define GET_ADV_PLAYER_LIMIT(adv)  ((adv)->player_limit)
#define GET_ADV_SCRIPTS(adv)  ((adv)->proto_script)

// utils
#define ADVENTURE_FLAGGED(adv, flg)  (IS_SET(GET_ADV_FLAGS(adv), (flg)) ? TRUE : FALSE)
#define INSTANCE_FLAGGED(i, flg)  (IS_SET((i)->flags, (flg)))
#define LINK_FLAGGED(lnkptr, flg)  (IS_SET((lnkptr)->flags, (flg)) ? TRUE : FALSE)


 //////////////////////////////////////////////////////////////////////////////
//// ARCHETYPE UTILS /////////////////////////////////////////////////////////

#define GET_ARCH_ATTRIBUTE(arch, pos)  ((arch)->attributes[(pos)])
#define GET_ARCH_DESC(arch)  ((arch)->description)
#define GET_ARCH_FEMALE_RANK(arch)  ((arch)->female_rank)
#define GET_ARCH_FLAGS(arch)  ((arch)->flags)
#define GET_ARCH_GEAR(arch)  ((arch)->gear)
#define GET_ARCH_LORE(arch)  ((arch)->lore)
#define GET_ARCH_MALE_RANK(arch)  ((arch)->male_rank)
#define GET_ARCH_NAME(arch)  ((arch)->name)
#define GET_ARCH_SKILLS(arch)  ((arch)->skills)
#define GET_ARCH_TYPE(arch)  ((arch)->type)
#define GET_ARCH_VNUM(arch)  ((arch)->vnum)

#define ARCHETYPE_FLAGGED(arch, flag)  IS_SET(GET_ARCH_FLAGS(arch), (flag))


 //////////////////////////////////////////////////////////////////////////////
//// AUGMENT UTILS ///////////////////////////////////////////////////////////

#define GET_AUG_ABILITY(aug)  ((aug)->ability)
#define GET_AUG_APPLIES(aug)  ((aug)->applies)
#define GET_AUG_FLAGS(aug)  ((aug)->flags)
#define GET_AUG_NAME(aug)  ((aug)->name)
#define GET_AUG_RESOURCES(aug)  ((aug)->resources)
#define GET_AUG_REQUIRES_OBJ(aug)  ((aug)->requires_obj)
#define GET_AUG_TYPE(aug)  ((aug)->type)
#define GET_AUG_VNUM(aug)  ((aug)->vnum)
#define GET_AUG_WEAR_FLAGS(aug)  ((aug)->wear_flags)

#define AUGMENT_FLAGGED(aug, flag)  IS_SET(GET_AUG_FLAGS(aug), (flag))


 //////////////////////////////////////////////////////////////////////////////
//// BITVECTOR UTILS /////////////////////////////////////////////////////////

#define IS_SET(flag,bit)  ((flag) & (bit))
#define SET_BIT(var,bit)  ((var) |= (bit))
#define REMOVE_BIT(var,bit)  ((var) &= ~(bit))
#define TOGGLE_BIT(var,bit)  ((var) = (var) ^ (bit))

#define PLR_TOG_CHK(ch,flag)  ((TOGGLE_BIT(PLR_FLAGS(ch), (flag))) & (flag))
#define PRF_TOG_CHK(ch,flag)  ((TOGGLE_BIT(PRF_FLAGS(ch), (flag))) & (flag))


 //////////////////////////////////////////////////////////////////////////////
//// BUILDING UTILS //////////////////////////////////////////////////////////

#define GET_BLD_VNUM(bld)  ((bld)->vnum)
#define GET_BLD_NAME(bld)  ((bld)->name)
#define GET_BLD_TITLE(bld)  ((bld)->title)
#define GET_BLD_ICON(bld)  ((bld)->icon)
#define GET_BLD_COMMANDS(bld)  ((bld)->commands)
#define GET_BLD_DESC(bld)  ((bld)->description)
#define GET_BLD_MAX_DAMAGE(bld)  ((bld)->max_damage)
#define GET_BLD_FAME(bld)  ((bld)->fame)
#define GET_BLD_FLAGS(bld)  ((bld)->flags)
#define GET_BLD_FUNCTIONS(bld)  ((bld)->functions)
#define GET_BLD_UPGRADES_TO(bld)  ((bld)->upgrades_to)
#define GET_BLD_EX_DESCS(bld)  ((bld)->ex_description)
#define GET_BLD_EXTRA_ROOMS(bld)  ((bld)->extra_rooms)
#define GET_BLD_DESIGNATE_FLAGS(bld)  ((bld)->designate_flags)
#define GET_BLD_BASE_AFFECTS(bld)  ((bld)->base_affects)
#define GET_BLD_CITIZENS(bld)  ((bld)->citizens)
#define GET_BLD_MILITARY(bld)  ((bld)->military)
#define GET_BLD_ARTISAN(bld)  ((bld)->artisan_vnum)
#define GET_BLD_SCRIPTS(bld)  ((bld)->proto_script)
#define GET_BLD_SPAWNS(bld)  ((bld)->spawns)
#define GET_BLD_INTERACTIONS(bld)  ((bld)->interactions)
#define GET_BLD_QUEST_LOOKUPS(bld)  ((bld)->quest_lookups)
#define GET_BLD_YEARLY_MAINTENANCE(bld)  ((bld)->yearly_maintenance)


 //////////////////////////////////////////////////////////////////////////////
//// CAN SEE UTILS ///////////////////////////////////////////////////////////

// this all builds up to CAN_SEE
#define LIGHT_OK(sub)  (!AFF_FLAGGED(sub, AFF_BLIND) && CAN_SEE_IN_DARK_ROOM((sub), (IN_ROOM(sub))))
#define INVIS_OK(sub, obj)  ((!AFF_FLAGGED(obj, AFF_INVISIBLE)) && (!AFF_FLAGGED((obj), AFF_HIDE) || AFF_FLAGGED((sub), AFF_SENSE_HIDE)))


#define MORT_CAN_SEE_NO_DARK(sub, obj)  (INVIS_OK(sub, obj))
#define MORT_CAN_SEE_LIGHT(sub, obj)  (LIGHT_OK(sub) || (IN_ROOM(sub) == IN_ROOM(obj) && has_ability((sub), ABIL_PREDATOR_VISION)))
#define MORT_CAN_SEE(sub, obj)  (MORT_CAN_SEE_LIGHT(sub, obj) && MORT_CAN_SEE_NO_DARK(sub, obj))

#define IMM_CAN_SEE(sub, obj)  (MORT_CAN_SEE(sub, obj) || (!IS_NPC(sub) && PRF_FLAGGED(sub, PRF_HOLYLIGHT)))
#define IMM_CAN_SEE_NO_DARK(sub, obj)  (MORT_CAN_SEE_NO_DARK(sub, obj) || (!IS_NPC(sub) && PRF_FLAGGED(sub, PRF_HOLYLIGHT)))

// for things like "who"
#define CAN_SEE_GLOBAL(sub, obj)  (SELF(sub, obj) || (GET_REAL_LEVEL(sub) >= (IS_NPC(obj) ? 0 : GET_INVIS_LEV(obj))))

// Can subject see character "obj"?
#define CAN_SEE_DARK(sub, obj)  (SELF(sub, obj) || (!EXTRACTED(obj) && CAN_SEE_GLOBAL(sub, obj) && IMM_CAN_SEE(sub, obj)))
#define CAN_SEE_NO_DARK(sub, obj)  (SELF(sub, obj) || (!EXTRACTED(obj) && CAN_SEE_GLOBAL(sub, obj) && IMM_CAN_SEE_NO_DARK(sub, obj)))

// The big question...
#define CAN_SEE(sub, obj)  (Global_ignore_dark ? CAN_SEE_NO_DARK(sub, obj) : CAN_SEE_DARK(sub, obj))

#define WIZHIDE_OK(sub, obj)  (!IS_IMMORTAL(obj) || !PRF_FLAGGED((obj), PRF_WIZHIDE) || (!IS_NPC(sub) && GET_ACCESS_LEVEL(sub) >= GET_ACCESS_LEVEL(obj) && PRF_FLAGGED((sub), PRF_HOLYLIGHT)))
#define INCOGNITO_OK(sub, obj)  (!IS_IMMORTAL(obj) || IS_IMMORTAL(sub) || !PRF_FLAGGED((obj), PRF_INCOGNITO))


 //////////////////////////////////////////////////////////////////////////////
//// CAN SEE OBJ UTILS ///////////////////////////////////////////////////////

#define CAN_SEE_OBJ_CARRIER(sub, obj)  ((!obj->carried_by || CAN_SEE(sub, obj->carried_by)) && (!obj->worn_by || CAN_SEE(sub, obj->worn_by)))
#define MORT_CAN_SEE_OBJ(sub, obj)  ((LIGHT_OK(sub) || obj->worn_by == sub || obj->carried_by == sub) && CAN_SEE_OBJ_CARRIER(sub, obj))
#define CAN_SEE_OBJ(sub, obj)  (MORT_CAN_SEE_OBJ(sub, obj) || (!IS_NPC(sub) && PRF_FLAGGED((sub), PRF_HOLYLIGHT)))


 //////////////////////////////////////////////////////////////////////////////
//// CAN SEE VEHICLE UTILS ///////////////////////////////////////////////////////

#define MORT_CAN_SEE_VEHICLE(sub, veh)  (VEH_FLAGGED((veh), VEH_VISIBLE_IN_DARK) || LIGHT_OK(sub) || VEH_DRIVER(veh) == (sub) || VEH_LED_BY(veh) == (sub) || VEH_SITTING_ON(veh) == (sub) || GET_ROOM_VEHICLE(IN_ROOM(sub)) == (veh))
#define CAN_SEE_VEHICLE(sub, veh)  (MORT_CAN_SEE_VEHICLE(sub, veh) || (!IS_NPC(sub) && PRF_FLAGGED((sub), PRF_HOLYLIGHT)))


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER UTILS /////////////////////////////////////////////////////////

// ch: char_data
#define GET_AGE(ch)  ((!IS_NPC(ch) && IS_VAMPIRE(ch)) ? GET_APPARENT_AGE(ch) : age(ch)->year)
#define GET_EQ(ch, i)  ((ch)->equipment[i])
#define GET_REAL_AGE(ch)  (age(ch)->year)
#define IN_ROOM(ch)  ((ch)->in_room)
#define GET_LOYALTY(ch)  ((ch)->loyalty)

// ch->aff_attributes, ch->real_attributes:
#define GET_REAL_ATT(ch, att)  ((ch)->real_attributes[(att)])
#define GET_ATT(ch, att)  ((ch)->aff_attributes[(att)])
#define GET_STRENGTH(ch)  GET_ATT(ch, STRENGTH)
#define GET_DEXTERITY(ch)  GET_ATT(ch, DEXTERITY)
#define GET_CHARISMA(ch)  GET_ATT(ch, CHARISMA)
#define GET_GREATNESS(ch)  GET_ATT(ch, GREATNESS)
#define GET_INTELLIGENCE(ch)  GET_ATT(ch, INTELLIGENCE)
#define GET_WITS(ch)  GET_ATT(ch, WITS)

// ch->player: char_player_data
#define GET_ACCESS_LEVEL(ch)  ((ch)->player.access_level)
#define GET_LONG_DESC(ch)  ((ch)->player.long_descr)
#define GET_LORE(ch)  ((ch)->player.lore)
#define GET_PASSWD(ch)  ((ch)->player.passwd)
#define GET_PC_NAME(ch)  ((ch)->player.name)
#define GET_REAL_NAME(ch)  (GET_NAME(REAL_CHAR(ch)))
#define GET_REAL_SEX(ch)  ((ch)->player.sex)
#define GET_SHORT_DESC(ch)  ((ch)->player.short_descr)

// ch->points: char_point_data
#define GET_CURRENT_POOL(ch, pool)  ((ch)->points.current_pools[(pool)])
#define GET_MAX_POOL(ch, pool)  ((ch)->points.max_pools[(pool)])
#define GET_DEFICIT(ch, pool)  ((ch)->points.deficit[(pool)])
#define GET_EXTRA_ATT(ch, att)  ((ch)->points.extra_attributes[(att)])

// ch->points: specific pools
#define GET_HEALTH(ch)  GET_CURRENT_POOL(ch, HEALTH)
#define GET_MAX_HEALTH(ch)  GET_MAX_POOL(ch, HEALTH)
#define GET_MANA(ch)  GET_CURRENT_POOL(ch, MANA)
#define GET_MAX_MANA(ch)  GET_MAX_POOL(ch, MANA)
#define GET_MOVE(ch)  GET_CURRENT_POOL(ch, MOVE)
#define GET_MAX_MOVE(ch)  GET_MAX_POOL(ch, MOVE)
#define GET_BLOOD(ch)  GET_CURRENT_POOL(ch, BLOOD)
#define GET_BLOOD_UPKEEP(ch)  (GET_EXTRA_ATT(ch, ATT_BLOOD_UPKEEP) + (get_skill_level(ch, SKILL_VAMPIRE) <= 15 ? 1 : 0))
extern int GET_MAX_BLOOD(char_data *ch);	// this one is different than the other max pools, and max_pools[BLOOD] is not used.
#define GET_HEALTH_DEFICIT(ch)  GET_DEFICIT((ch), HEALTH)
#define GET_MOVE_DEFICIT(ch)  GET_DEFICIT((ch), MOVE)
#define GET_MANA_DEFICIT(ch)  GET_DEFICIT((ch), MANA)
#define GET_BONUS_INVENTORY(ch)  GET_EXTRA_ATT(ch, ATT_BONUS_INVENTORY)
#define GET_RESIST_PHYSICAL(ch)  GET_EXTRA_ATT(ch, ATT_RESIST_PHYSICAL)
#define GET_RESIST_MAGICAL(ch)  GET_EXTRA_ATT(ch, ATT_RESIST_MAGICAL)
#define GET_BLOCK(ch)  GET_EXTRA_ATT(ch, ATT_BLOCK)
#define GET_TO_HIT(ch)  GET_EXTRA_ATT(ch, ATT_TO_HIT)
#define GET_DODGE(ch)  GET_EXTRA_ATT(ch, ATT_DODGE)
#define GET_EXTRA_BLOOD(ch)  GET_EXTRA_ATT(ch, ATT_EXTRA_BLOOD)
#define GET_BONUS_PHYSICAL(ch)  GET_EXTRA_ATT(ch, ATT_BONUS_PHYSICAL)
#define GET_BONUS_MAGICAL(ch)  GET_EXTRA_ATT(ch, ATT_BONUS_MAGICAL)
#define GET_BONUS_HEALING(ch)  GET_EXTRA_ATT(ch, ATT_BONUS_HEALING)	// use total_bonus_healing(ch) for most uses
#define GET_HEAL_OVER_TIME(ch)  GET_EXTRA_ATT(ch, ATT_HEAL_OVER_TIME)
#define GET_CRAFTING_BONUS(ch)  GET_EXTRA_ATT(ch, ATT_CRAFTING_BONUS)

// ch->char_specials: char_special_data
#define FIGHTING(ch)  ((ch)->char_specials.fighting.victim)
#define FIGHT_MODE(ch)  ((ch)->char_specials.fighting.mode)
#define FIGHT_WAIT(ch)  ((ch)->char_specials.fighting.wait)
#define GET_DRIVING(ch)  ((ch)->char_specials.driving)
#define GET_EMPIRE_NPC_DATA(ch)  ((ch)->char_specials.empire_npc)
#define GET_FED_ON_BY(ch)  ((ch)->char_specials.fed_on_by)
#define GET_FEEDING_FROM(ch)  ((ch)->char_specials.feeding_from)
#define GET_HEALTH_REGEN(ch)  ((ch)->char_specials.health_regen)
#define GET_IDNUM(ch)  (REAL_CHAR(ch)->char_specials.idnum)
#define GET_LAST_SWING_MAINHAND(ch)  ((ch)->char_specials.fighting.last_swing_mainhand)
#define GET_LAST_SWING_OFFHAND(ch)  ((ch)->char_specials.fighting.last_swing_offhand)
#define GET_LEADING_MOB(ch)  ((ch)->char_specials.leading_mob)
#define GET_LEADING_VEHICLE(ch)  ((ch)->char_specials.leading_vehicle)
#define GET_LED_BY(ch)  ((ch)->char_specials.led_by)
#define GET_MANA_REGEN(ch)  ((ch)->char_specials.mana_regen)
#define GET_MOVE_REGEN(ch)  ((ch)->char_specials.move_regen)
#define GET_SITTING_ON(ch)  ((ch)->char_specials.sitting_on)
#define GET_POS(ch)  ((ch)->char_specials.position)
#define HUNTING(ch)  ((ch)->char_specials.hunting)
#define IS_CARRYING_N(ch)  ((ch)->char_specials.carry_items)


// definitions
#define AWAKE(ch)  (GET_POS(ch) > POS_SLEEPING || GET_POS(ch) == POS_DEAD)
#define CAN_CARRY_N(ch)  (25 + GET_BONUS_INVENTORY(ch) + (HAS_BONUS_TRAIT(ch, BONUS_INVENTORY) ? 5 : 0) + (GET_EQ((ch), WEAR_PACK) ? GET_PACK_CAPACITY(GET_EQ(ch, WEAR_PACK)) : 0))
#define CAN_CARRY_OBJ(ch,obj)  (FREE_TO_CARRY(obj) || (IS_CARRYING_N(ch) + obj_carry_size(obj)) <= CAN_CARRY_N(ch))
#define CAN_GET_OBJ(ch, obj)  (CAN_WEAR((obj), ITEM_WEAR_TAKE) && CAN_CARRY_OBJ((ch),(obj)) && CAN_SEE_OBJ((ch),(obj)))
#define CAN_RECOGNIZE(ch, vict)  (IS_IMMORTAL(ch) || (!AFF_FLAGGED(vict, AFF_NO_SEE_IN_ROOM) && ((GET_LOYALTY(ch) && GET_LOYALTY(ch) == GET_LOYALTY(vict)) || (GROUP(ch) && in_same_group(ch, vict)) || (!CHAR_MORPH_FLAGGED((vict), MORPHF_ANIMAL) && !IS_DISGUISED(vict)))))
#define CAN_RIDE_FLYING_MOUNT(ch)  (has_ability((ch), ABIL_ALL_TERRAIN_RIDING) || has_ability((ch), ABIL_FLY) || has_ability((ch), ABIL_DRAGONRIDING))
#define CAN_SEE_IN_DARK(ch)  (HAS_INFRA(ch) || (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_HOLYLIGHT)))
#define CAN_SEE_IN_DARK_ROOM(ch, room)  ((WOULD_BE_LIGHT_WITHOUT_MAGIC_DARKNESS(room) || (room == IN_ROOM(ch) && (has_ability(ch, ABIL_BY_MOONLIGHT))) || CAN_SEE_IN_DARK(ch)) && (!MAGIC_DARKNESS(room) || CAN_SEE_IN_MAGIC_DARKNESS(ch)))
#define CAN_SEE_IN_MAGIC_DARKNESS(ch)  (IS_NPC(ch) ? (get_approximate_level(ch) > 100) : has_ability((ch), ABIL_DARKNESS))
#define CAN_SPEND_BLOOD(ch)  (!AFF_FLAGGED(ch, AFF_CANT_SPEND_BLOOD))
#define CAST_BY_ID(ch)  (IS_NPC(ch) ? (-1 * GET_MOB_VNUM(ch)) : GET_IDNUM(ch))
#define EFFECTIVELY_FLYING(ch)  (IS_RIDING(ch) ? MOUNT_FLAGGED(ch, MOUNT_FLYING) : AFF_FLAGGED(ch, AFF_FLY))
#define EFFECTIVELY_SWIMMING(ch)  (EFFECTIVELY_FLYING(ch) || (IS_RIDING(ch) && (MOUNT_FLAGGED((ch), MOUNT_AQUATIC) || has_ability((ch), ABIL_ALL_TERRAIN_RIDING))) || (IS_NPC(ch) ? MOB_FLAGGED((ch), MOB_AQUATIC) : has_ability((ch), ABIL_SWIMMING)))
#define FREE_TO_CARRY(obj)  (IS_COINS(obj) || GET_OBJ_REQUIRES_QUEST(obj) != NOTHING)
#define HAS_INFRA(ch)  AFF_FLAGGED(ch, AFF_INFRAVISION)
#define IS_HUMAN(ch)  (!IS_VAMPIRE(ch))
#define IS_MAGE(ch)  (IS_NPC(ch) ? GET_MAX_MANA(ch) > 0 : (get_skill_level((ch), SKILL_NATURAL_MAGIC) > 0 || get_skill_level((ch), SKILL_HIGH_SORCERY) > 0))
#define IS_OUTDOORS(ch)  IS_OUTDOOR_TILE(IN_ROOM(ch))
#define IS_VAMPIRE(ch)  (IS_NPC(ch) ? MOB_FLAGGED((ch), MOB_VAMPIRE) : (get_skill_level((ch), SKILL_VAMPIRE) > 0))
#define WOULD_EXECUTE(ch, vict)  (MOB_FLAGGED((vict), MOB_HARD | MOB_GROUP) || (IS_NPC(ch) ? (((ch)->master && !IS_NPC((ch)->master)) ? PRF_FLAGGED((ch)->master, PRF_AUTOKILL) : (!MOB_FLAGGED((ch), MOB_ANIMAL) || MOB_FLAGGED((ch), MOB_AGGRESSIVE | MOB_HARD | MOB_GROUP))) : PRF_FLAGGED((ch), PRF_AUTOKILL)))

// helpers
#define AFF_FLAGGED(ch, flag)  (IS_SET(AFF_FLAGS(ch), (flag)))
#define EXTRACTED(ch)  (PLR_FLAGGED((ch), PLR_EXTRACTED) || MOB_FLAGGED((ch), MOB_EXTRACTED))
#define GET_NAME(ch)  (IS_NPC(ch) ? GET_SHORT_DESC(ch) : GET_PC_NAME(ch))
#define GET_REAL_LEVEL(ch)  (ch->desc && ch->desc->original ? GET_ACCESS_LEVEL(ch->desc->original) : GET_ACCESS_LEVEL(ch))
#define GET_SEX(ch)  (IS_DISGUISED(ch) ? GET_DISGUISED_SEX(ch) : GET_REAL_SEX(ch))
#define IS_DEAD(ch)  (GET_POS(ch) == POS_DEAD)
#define IS_INJURED(ch, flag)  (IS_SET(INJURY_FLAGS(ch), (flag)))
#define IS_NPC(ch)  (IS_SET(MOB_FLAGS(ch), MOB_ISNPC))
#define REAL_CHAR(ch)  (((ch)->desc && (ch)->desc->original) ? (ch)->desc->original : (ch))
#define REAL_NPC(ch)  (IS_NPC(REAL_CHAR(ch)))

// wait!
#define GET_WAIT_STATE(ch)  ((ch)->wait)


 //////////////////////////////////////////////////////////////////////////////
//// CLASS UTILS /////////////////////////////////////////////////////////////

#define CLASS_VNUM(cls)  ((cls)->vnum)
#define CLASS_NAME(cls)  ((cls)->name)
#define CLASS_ABBREV(cls)  ((cls)->abbrev)
#define CLASS_FLAGS(cls)  ((cls)->flags)
#define CLASS_POOL(cls, type)  ((cls)->pools[type])
#define CLASS_SKILL_REQUIREMENTS(cls)  ((cls)->skill_requirements)
#define CLASS_ABILITIES(cls)  ((cls)->abilities)

#define CLASS_FLAGGED(cls, flag)  IS_SET(CLASS_FLAGS(cls), (flag))


 //////////////////////////////////////////////////////////////////////////////
//// CRAFT UTILS /////////////////////////////////////////////////////////////

#define GET_CRAFT_ABILITY(craft)  ((craft)->ability)
#define GET_CRAFT_BUILD_FACING(craft)  ((craft)->build_facing)
#define GET_CRAFT_BUILD_ON(craft)  ((craft)->build_on)
#define GET_CRAFT_BUILD_TYPE(craft)  ((craft)->build_type)
#define GET_CRAFT_FLAGS(craft)  ((craft)->flags)
#define GET_CRAFT_MIN_LEVEL(craft)  ((craft)->min_level)
#define GET_CRAFT_NAME(craft)  ((craft)->name)
#define GET_CRAFT_OBJECT(craft)  ((craft)->object)
#define GET_CRAFT_QUANTITY(craft)  ((craft)->quantity)
#define GET_CRAFT_REQUIRES_OBJ(craft)  ((craft)->requires_obj)
#define GET_CRAFT_RESOURCES(craft)  ((craft)->resources)
#define GET_CRAFT_TIME(craft)  ((craft)->time)
#define GET_CRAFT_TYPE(craft)  ((craft)->type)
#define GET_CRAFT_VNUM(craft)  ((craft)->vnum)

#define CRAFT_FLAGGED(cr, flg)  (IS_SET(GET_CRAFT_FLAGS(cr), (flg)) ? TRUE : FALSE)


 //////////////////////////////////////////////////////////////////////////////
//// CROP UTILS //////////////////////////////////////////////////////////////

#define GET_CROP_CLIMATE(crop)  ((crop)->climate)
#define GET_CROP_FLAGS(crop)  ((crop)->flags)
#define GET_CROP_ICONS(crop)  ((crop)->icons)
#define GET_CROP_INTERACTIONS(crop)  ((crop)->interactions)
#define GET_CROP_MAPOUT(crop)  ((crop)->mapout)
#define GET_CROP_NAME(crop)  ((crop)->name)
#define GET_CROP_SPAWNS(crop)  ((crop)->spawns)
#define GET_CROP_TITLE(crop)  ((crop)->title)
#define GET_CROP_VNUM(crop)  ((crop)->vnum)
#define GET_CROP_X_MAX(crop)  ((crop)->x_max)
#define GET_CROP_X_MIN(crop)  ((crop)->x_min)
#define GET_CROP_Y_MAX(crop)  ((crop)->y_max)
#define GET_CROP_Y_MIN(crop)  ((crop)->y_min)

// helpers
#define CROP_FLAGGED(crp, flg)  (IS_SET(GET_CROP_FLAGS(crp), (flg)))


 //////////////////////////////////////////////////////////////////////////////
//// DESCRIPTOR UTILS ////////////////////////////////////////////////////////

// basic
#define STATE(d)  ((d)->connected)
#define SUBMENU(d)  ((d)->submenu)


// OLC_x: olc getters
#define GET_OLC_TYPE(desc)  ((desc)->olc_type)
#define GET_OLC_VNUM(desc)  ((desc)->olc_vnum)
#define GET_OLC_STORAGE(desc)  ((desc)->olc_storage)
#define GET_OLC_ABILITY(desc)  ((desc)->olc_ability)
#define GET_OLC_ADVENTURE(desc)  ((desc)->olc_adventure)
#define GET_OLC_ARCHETYPE(desc)  ((desc)->olc_archetype)
#define GET_OLC_AUGMENT(desc)  ((desc)->olc_augment)
#define GET_OLC_BOOK(desc)  ((desc)->olc_book)
#define GET_OLC_BUILDING(desc)  ((desc)->olc_building)
#define GET_OLC_CLASS(desc)  ((desc)->olc_class)
#define GET_OLC_CRAFT(desc)  ((desc)->olc_craft)
#define GET_OLC_CROP(desc)  ((desc)->olc_crop)
#define GET_OLC_FACTION(desc)  ((desc)->olc_faction)
#define GET_OLC_GLOBAL(desc)  ((desc)->olc_global)
#define GET_OLC_MOBILE(desc)  ((desc)->olc_mobile)
#define GET_OLC_MORPH(desc)  ((desc)->olc_morph)
#define GET_OLC_OBJECT(desc)  ((desc)->olc_object)
#define GET_OLC_QUEST(desc)  ((desc)->olc_quest)
#define GET_OLC_ROOM_TEMPLATE(desc)  ((desc)->olc_room_template)
#define GET_OLC_SECTOR(desc)  ((desc)->olc_sector)
#define GET_OLC_SKILL(desc)  ((desc)->olc_skill)
#define GET_OLC_SOCIAL(desc)  ((desc)->olc_social)
#define GET_OLC_TRIGGER(desc)  ((desc)->olc_trigger)
#define GET_OLC_VEHICLE(desc)  ((desc)->olc_vehicle)


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE UTILS ////////////////////////////////////////////////////////////

#define EMPIRE_VNUM(emp)  ((emp)->vnum)
#define EMPIRE_LEADER(emp)  ((emp)->leader)
#define EMPIRE_CREATE_TIME(emp)  ((emp)->create_time)
#define EMPIRE_NAME(emp)  ((emp)->name)
#define EMPIRE_ADJECTIVE(emp)  ((emp)->adjective)
#define EMPIRE_BANNER(emp)  ((emp)->banner)
#define EMPIRE_BANNER_HAS_UNDERLINE(emp)  ((emp)->banner_has_underline)
#define EMPIRE_NUM_RANKS(emp)  ((emp)->num_ranks)
#define EMPIRE_RANK(emp, num)  ((emp)->rank[(num)])
#define EMPIRE_FRONTIER_TRAITS(emp)  ((emp)->frontier_traits)
#define EMPIRE_COINS(emp)  ((emp)->coins)
#define EMPIRE_PRIV(emp, num)  ((emp)->priv[(num)])
#define EMPIRE_DESCRIPTION(emp)  ((emp)->description)
#define EMPIRE_DIPLOMACY(emp)  ((emp)->diplomacy)
#define EMPIRE_STORAGE(emp)  ((emp)->store)
#define EMPIRE_TRADE(emp)  ((emp)->trade)
#define EMPIRE_LOGS(emp)  ((emp)->logs)
#define EMPIRE_TERRITORY_LIST(emp)  ((emp)->territory_list)
#define EMPIRE_CITY_LIST(emp)  ((emp)->city_list)
#define EMPIRE_CITY_TERRITORY(emp)  ((emp)->city_terr)
#define EMPIRE_OUTSIDE_TERRITORY(emp)  ((emp)->outside_terr)
#define EMPIRE_WEALTH(emp)  ((emp)->wealth)
#define EMPIRE_POPULATION(emp)  ((emp)->population)
#define EMPIRE_MILITARY(emp)  ((emp)->military)
#define EMPIRE_MOTD(emp)  ((emp)->motd)
#define EMPIRE_NEEDS_SAVE(emp)  ((emp)->needs_save)
#define EMPIRE_GREATNESS(emp)  ((emp)->greatness)
#define EMPIRE_TECH(emp, num)  ((emp)->tech[(num)])
#define EMPIRE_MEMBERS(emp)  ((emp)->members)
#define EMPIRE_TOTAL_MEMBER_COUNT(emp)  ((emp)->total_member_count)
#define EMPIRE_TOTAL_PLAYTIME(emp)  ((emp)->total_playtime)
#define EMPIRE_IMM_ONLY(emp)  ((emp)->imm_only)
#define EMPIRE_FAME(emp)  ((emp)->fame)
#define EMPIRE_LAST_LOGON(emp)  ((emp)->last_logon)
#define EMPIRE_SCORE(emp, num)  ((emp)->scores[(num)])
#define EMPIRE_SHIPPING_LIST(emp)  ((emp)->shipping_list)
#define EMPIRE_SORT_VALUE(emp)  ((emp)->sort_value)
#define EMPIRE_UNIQUE_STORAGE(emp)  ((emp)->unique_store)
#define EMPIRE_WORKFORCE_TRACKER(emp)  ((emp)->ewt_tracker)
#define EMPIRE_ISLANDS(emp)  ((emp)->islands)
#define EMPIRE_TOP_SHIPPING_ID(emp)  ((emp)->top_shipping_id)

// helpers
#define EMPIRE_HAS_TECH(emp, num)  (EMPIRE_TECH((emp), (num)) > 0)
#define EMPIRE_IS_TIMED_OUT(emp)  (EMPIRE_LAST_LOGON(emp) + (config_get_int("whole_empire_timeout") * SECS_PER_REAL_DAY) < time(0))
#define GET_TOTAL_WEALTH(emp)  (EMPIRE_WEALTH(emp) + (EMPIRE_COINS(emp) * COIN_VALUE))
#define EXPLICIT_BANNER_TERMINATOR(emp)  (EMPIRE_BANNER_HAS_UNDERLINE(emp) ? "\t0" : "")

// definitions
#define SAME_EMPIRE(ch, vict)  (!IS_NPC(ch) && !IS_NPC(vict) && GET_LOYALTY(ch) != NULL && GET_LOYALTY(ch) == GET_LOYALTY(vict))

// only some types of tiles are kept in the shortlist of territories -- particularly ones with associated chores
#define BELONGS_IN_TERRITORY_LIST(room)  (IS_ANY_BUILDING(room) || COMPLEX_DATA(room) || ROOM_SECT_FLAGGED(room, SECTF_CHORE))
#define COUNTS_AS_TERRITORY(room)  (HOME_ROOM(room) == (room) && !GET_ROOM_VEHICLE(room))
#define LARGE_CITY_RADIUS(room)  (ROOM_BLD_FLAGGED((room), BLD_LARGE_CITY_RADIUS) || ROOM_SECT_FLAGGED((room), SECTF_LARGE_CITY_RADIUS))


 //////////////////////////////////////////////////////////////////////////////
//// FACTION UTILS ///////////////////////////////////////////////////////////

#define FCT_VNUM(fct)  ((fct)->vnum)
#define FCT_DESCRIPTION(fct)  ((fct)->description)
#define FCT_FLAGS(fct)  ((fct)->flags)
#define FCT_MAX_REP(fct)  ((fct)->max_rep)
#define FCT_MIN_REP(fct)  ((fct)->min_rep)
#define FCT_NAME(fct)  ((fct)->name)
#define FCT_RELATIONS(fct)  ((fct)->relations)
#define FCT_STARTING_REP(fct)  ((fct)->starting_rep)

// helpers
#define FACTION_FLAGGED(fct, flag)  IS_SET(FCT_FLAGS(fct), (flag))


 //////////////////////////////////////////////////////////////////////////////
//// FIGHT UTILS /////////////////////////////////////////////////////////////

#define SHOULD_APPEAR(ch)  AFF_FLAGGED(ch, AFF_HIDE | AFF_INVISIBLE)


 //////////////////////////////////////////////////////////////////////////////
//// GLOBAL UTILS ////////////////////////////////////////////////////////////

#define GET_GLOBAL_VNUM(glb)  ((glb)->vnum)
#define GET_GLOBAL_NAME(glb)  ((glb)->name)
#define GET_GLOBAL_TYPE(glb)  ((glb)->type)
#define GET_GLOBAL_FLAGS(glb)  ((glb)->flags)
#define GET_GLOBAL_GEAR(glb)  ((glb)->gear)
#define GET_GLOBAL_PERCENT(glb)  ((glb)->percent)
#define GET_GLOBAL_ABILITY(glb)  ((glb)->ability)
#define GET_GLOBAL_TYPE_EXCLUDE(glb)  ((glb)->type_exclude)
#define GET_GLOBAL_TYPE_FLAGS(glb)  ((glb)->type_flags)
#define GET_GLOBAL_MIN_LEVEL(glb)  ((glb)->min_level)
#define GET_GLOBAL_MAX_LEVEL(glb)  ((glb)->max_level)
#define GET_GLOBAL_VAL(glb, pos)  ((glb)->value[(pos)])
#define GET_GLOBAL_INTERACTIONS(glb)  ((glb)->interactions)

// global value types
#define GLB_VAL_MAX_MINE_SIZE  0	// which global value is used for max mine size


 //////////////////////////////////////////////////////////////////////////////
//// MAP UTILS ///////////////////////////////////////////////////////////////

// returns TRUE only if both x and y are in the bounds of the map
#define CHECK_MAP_BOUNDS(x, y)  ((x) >= 0 && (x) < MAP_WIDTH && (y) >= 0 && (y) < MAP_HEIGHT)

// for getting coordinates by vnum
#define MAP_X_COORD(vnum)  ((vnum) % MAP_WIDTH)
#define MAP_Y_COORD(vnum)  (int)((vnum) / MAP_WIDTH)

// flat coords ASSUME room is on the map -- otherwise use the X_COORD/Y_COORD
#define FLAT_X_COORD(room)  MAP_X_COORD(GET_ROOM_VNUM(room))
#define FLAT_Y_COORD(room)  MAP_Y_COORD(GET_ROOM_VNUM(room))

extern int X_COORD(room_data *room);	// formerly #define X_COORD(room)  FLAT_X_COORD(get_map_location_for(room))
extern int Y_COORD(room_data *room);	// formerly #define Y_COORD(room)  FLAT_Y_COORD(get_map_location_for(room))

// wrap x/y "around the edge"
#define WRAP_X_COORD(x)  (WRAP_X ? (((x) < 0) ? ((x) + MAP_WIDTH) : (((x) >= MAP_WIDTH) ? ((x) - MAP_WIDTH) : (x))) : MAX(0, MIN(MAP_WIDTH-1, (x))))
#define WRAP_Y_COORD(y)  (WRAP_Y ? (((y) < 0) ? ((y) + MAP_HEIGHT) : (((y) >= MAP_HEIGHT) ? ((y) - MAP_HEIGHT) : (y))) : MAX(0, MIN(MAP_HEIGHT-1, (y))))


 //////////////////////////////////////////////////////////////////////////////
//// MEMORY UTILS ////////////////////////////////////////////////////////////

#define CREATE(result, type, number)  do {\
	if ((number) * sizeof(type) <= 0)	\
		log("SYSERR: Zero bytes or less requested at %s:%d.", __FILE__, __LINE__);	\
	if (!((result) = (type *) calloc ((number), sizeof(type))))	\
		{ log("SYSERR: malloc failure: %s: %d", __FILE__, __LINE__); abort(); } } while(0)


#define RECREATE(result, type, number) do {\
  if (!((result) = (type *) realloc ((result), sizeof(type) * (number))))\
		{ log("SYSERR: realloc failure: %s: %d", __FILE__, __LINE__); abort(); } } while(0)

/*
 * the source previously used the same code in many places to remove an item
 * from a list: if it's the list head, change the head, else traverse the
 * list looking for the item before the one to be removed.  Now, we have a
 * macro to do this.  To use, just make sure that there is a variable 'temp'
 * declared as the same type as the list to be manipulated.
 */
#define REMOVE_FROM_LIST(item, head, next)	\
   if ((item) == (head))		\
      head = (item)->next;		\
   else {				\
      temp = head;			\
      while (temp && (temp->next != (item))) \
	 temp = temp->next;		\
      if (temp)				\
         temp->next = (item)->next;	\
   }


 //////////////////////////////////////////////////////////////////////////////
//// MOBILE UTILS ////////////////////////////////////////////////////////////

// ch->char_specials: char_special_data
#define MOB_FLAGS(ch)  ((ch)->char_specials.act)

// ch->mob_specials: mob_special_data
#define GET_CURRENT_SCALE_LEVEL(ch)  ((ch)->mob_specials.current_scale_level)
#define GET_MAX_SCALE_LEVEL(ch)  ((ch)->mob_specials.max_scale_level)
#define GET_MOB_VNUM(mob)  (IS_NPC(mob) ? (mob)->vnum : NOTHING)
#define GET_MIN_SCALE_LEVEL(ch)  ((ch)->mob_specials.min_scale_level)
#define MOB_NAME_SET(ch)  ((ch)->mob_specials.name_set)
#define MOB_ATTACK_TYPE(ch)  ((ch)->mob_specials.attack_type)
#define MOB_CUSTOM_MSGS(ch)  ((ch)->mob_specials.custom_msgs)
#define MOB_DAMAGE(ch)  ((ch)->mob_specials.damage)
#define MOB_DYNAMIC_NAME(ch)  ((ch)->mob_specials.dynamic_name)
#define MOB_DYNAMIC_SEX(ch)  ((ch)->mob_specials.dynamic_sex)
#define MOB_FACTION(ch)  ((ch)->mob_specials.faction)
#define MOB_INSTANCE_ID(ch)  ((ch)->mob_specials.instance_id)
#define MOB_MOVE_TYPE(ch)  ((ch)->mob_specials.move_type)
#define MOB_PURSUIT(ch)  ((ch)->mob_specials.pursuit)
#define MOB_PURSUIT_LEASH_LOC(ch)  ((ch)->mob_specials.pursuit_leash_loc)
#define MOB_TAGGED_BY(ch)  ((ch)->mob_specials.tagged_by)
#define MOB_SPAWN_TIME(ch)  ((ch)->mob_specials.spawn_time)
#define MOB_TO_DODGE(ch)  ((ch)->mob_specials.to_dodge)
#define MOB_TO_HIT(ch)  ((ch)->mob_specials.to_hit)
#define MOB_QUEST_LOOKUPS(ch)  ((ch)->quest_lookups)

// helpers
#define IS_MOB(ch)  (IS_NPC(ch) && GET_MOB_VNUM(ch) != NOTHING)
#define IS_TAGGED_BY(mob, player)  (IS_NPC(mob) && !IS_NPC(player) && find_id_in_tag_list(GET_IDNUM(player), MOB_TAGGED_BY(mob)))
#define MOB_FLAGGED(ch, flag)  (IS_NPC(ch) && IS_SET(MOB_FLAGS(ch), (flag)))


 //////////////////////////////////////////////////////////////////////////////
//// MORPH UTILS /////////////////////////////////////////////////////////////

#define MORPH_ABILITY(mph)  ((mph)->ability)
#define MORPH_AFFECTS(mph)  ((mph)->affects)
#define MORPH_APPLIES(mph)  ((mph)->applies)
#define MORPH_ATTACK_TYPE(mph)  ((mph)->attack_type)
#define MORPH_COST(mph)  ((mph)->cost)
#define MORPH_COST_TYPE(mph)  ((mph)->cost_type)
#define MORPH_FLAGS(mph)  ((mph)->flags)
#define MORPH_KEYWORDS(mph)  ((mph)->keywords)
#define MORPH_LONG_DESC(mph)  ((mph)->long_desc)
#define MORPH_MAX_SCALE(mph)  ((mph)->max_scale)
#define MORPH_MOVE_TYPE(mph)  ((mph)->move_type)
#define MORPH_REQUIRES_OBJ(mph)  ((mph)->requires_obj)
#define MORPH_SHORT_DESC(mph)  ((mph)->short_desc)
#define MORPH_VNUM(mph)  ((mph)->vnum)

// helpers
#define MORPH_FLAGGED(mph, flg)  IS_SET(MORPH_FLAGS(mph), (flg))
#define CHAR_MORPH_FLAGGED(ch, flg)  (IS_MORPHED(ch) ? MORPH_FLAGGED(GET_MORPH(ch), (flg)) : FALSE)


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT UTILS ////////////////////////////////////////////////////////////

// primary attributes
#define GET_OBJ_APPLIES(obj)  ((obj)->applies)
#define GET_AUTOSTORE_TIMER(obj)  ((obj)->autostore_timer)
#define GET_OBJ_ACTION_DESC(obj)  ((obj)->action_description)
#define GET_OBJ_AFF_FLAGS(obj)  ((obj)->obj_flags.bitvector)
#define GET_OBJ_CARRYING_N(obj)  ((obj)->obj_flags.carrying_n)
#define GET_OBJ_CMP_TYPE(obj)  ((obj)->obj_flags.cmp_type)
#define GET_OBJ_CMP_FLAGS(obj)  ((obj)->obj_flags.cmp_flags)
#define GET_OBJ_CURRENT_SCALE_LEVEL(obj)  ((obj)->obj_flags.current_scale_level)
#define GET_OBJ_EXTRA(obj)  ((obj)->obj_flags.extra_flags)
#define GET_OBJ_KEYWORDS(obj)  ((obj)->name)
#define GET_OBJ_LONG_DESC(obj)  ((obj)->description)
#define GET_OBJ_MATERIAL(obj)  ((obj)->obj_flags.material)
#define GET_OBJ_MAX_SCALE_LEVEL(obj)  ((obj)->obj_flags.max_scale_level)
#define GET_OBJ_MIN_SCALE_LEVEL(obj)  ((obj)->obj_flags.min_scale_level)
#define GET_OBJ_QUEST_LOOKUPS(obj)  ((obj)->quest_lookups)
#define GET_OBJ_REQUIRES_QUEST(obj)  ((obj)->obj_flags.requires_quest)
#define GET_OBJ_SHORT_DESC(obj)  ((obj)->short_description)
#define GET_OBJ_TIMER(obj)  ((obj)->obj_flags.timer)
#define GET_OBJ_TYPE(obj)  ((obj)->obj_flags.type_flag)
#define GET_OBJ_VAL(obj, val)  ((obj)->obj_flags.value[(val)])
#define GET_OBJ_WEAR(obj)  ((obj)->obj_flags.wear_flags)
#define GET_STOLEN_TIMER(obj)  ((obj)->stolen_timer)
#define GET_STOLEN_FROM(obj)  ((obj)->stolen_from)
#define LAST_OWNER_ID(obj)  ((obj)->last_owner_id)
#define OBJ_BOUND_TO(obj)  ((obj)->bound_to)
#define OBJ_VERSION(obj)  ((obj)->version)

// compound attributes
#define GET_OBJ_DESC(obj, ch, mode)  get_obj_desc((obj), (ch), (mode))
#define GET_OBJ_VNUM(obj)  ((obj)->vnum)

// definitions
#define IS_BLOOD_WEAPON(obj)  (GET_OBJ_VNUM(obj) == o_BLOODSWORD || GET_OBJ_VNUM(obj) == o_BLOODSTAFF || GET_OBJ_VNUM(obj) == o_BLOODSPEAR || GET_OBJ_VNUM(obj) == o_BLOODSKEAN || GET_OBJ_VNUM(obj) == o_BLOODMACE)
#define IS_STOLEN(obj)  (GET_STOLEN_TIMER(obj) > 0 && (config_get_int("stolen_object_timer") * SECS_PER_REAL_MIN) + GET_STOLEN_TIMER(obj) > time(0))

// helpers
#define OBJ_FLAGGED(obj, flag)  (IS_SET(GET_OBJ_EXTRA(obj), (flag)))
#define OBJ_CMP_FLAGGED(obj, flg)  IS_SET(GET_OBJ_CMP_FLAGS(obj), (flg))
#define OBJS(obj, vict)  (CAN_SEE_OBJ((vict), (obj)) ? GET_OBJ_DESC((obj), vict, 1) : "something")
#define OBJVAL_FLAGGED(obj, flag)  (IS_SET(GET_OBJ_VAL((obj), 1), (flag)))
#define CAN_WEAR(obj, part)  (IS_SET(GET_OBJ_WEAR(obj), (part)))

// for stacking, sotring, etc
#define OBJ_CAN_STACK(obj)  (GET_OBJ_TYPE(obj) != ITEM_CONTAINER && !OBJ_FLAGGED((obj), OBJ_ENCHANTED) && !IS_ARROW(obj))
#define OBJ_CAN_STORE(obj)  ((obj)->storage && !OBJ_BOUND_TO(obj) && !OBJ_FLAGGED((obj), OBJ_SUPERIOR | OBJ_ENCHANTED) && !IS_STOLEN(obj))
#define UNIQUE_OBJ_CAN_STORE(obj)  (!OBJ_BOUND_TO(obj) && !OBJ_CAN_STORE(obj) && !OBJ_FLAGGED((obj), OBJ_JUNK) && GET_OBJ_TIMER(obj) == UNLIMITED && !IS_STOLEN(obj) && GET_OBJ_REQUIRES_QUEST(obj) == NOTHING && !IS_STOLEN(obj))
#define OBJ_STACK_FLAGS  (OBJ_SUPERIOR | OBJ_KEEP)
#define OBJS_ARE_SAME(o1, o2)  (GET_OBJ_VNUM(o1) == GET_OBJ_VNUM(o2) && ((GET_OBJ_EXTRA(o1) & OBJ_STACK_FLAGS) == (GET_OBJ_EXTRA(o2) & OBJ_STACK_FLAGS)) && (!IS_DRINK_CONTAINER(o1) || GET_DRINK_CONTAINER_TYPE(o1) == GET_DRINK_CONTAINER_TYPE(o2)) && (IS_STOLEN(o1) == IS_STOLEN(o2)))


 //////////////////////////////////////////////////////////////////////////////
//// OBJVAL UTILS ////////////////////////////////////////////////////////////

// ITEM_POTION
#define IS_POTION(obj)  (GET_OBJ_TYPE(obj) == ITEM_POTION)
#define VAL_POTION_TYPE  0
#define VAL_POTION_SCALE  1
#define GET_POTION_TYPE(obj)  (IS_POTION(obj) ? GET_OBJ_VAL((obj), VAL_POTION_TYPE) : NOTHING)
#define GET_POTION_SCALE(obj)  (IS_POTION(obj) ? GET_OBJ_VAL((obj), VAL_POTION_SCALE) : 0)

// ITEM_POISON
#define IS_POISON(obj)  (GET_OBJ_TYPE(obj) == ITEM_POISON)
#define VAL_POISON_TYPE  0
#define VAL_POISON_CHARGES  1
#define GET_POISON_TYPE(obj)  (IS_POISON(obj) ? GET_OBJ_VAL((obj), VAL_POISON_TYPE) : NOTHING)
#define GET_POISON_CHARGES(obj)  (IS_POISON(obj) ? GET_OBJ_VAL((obj), VAL_POISON_CHARGES) : 0)

// ITEM_WEAPON
#define IS_WEAPON(obj)  (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
#define VAL_WEAPON_DAMAGE_BONUS  1
#define VAL_WEAPON_TYPE  2
#define GET_WEAPON_DAMAGE_BONUS(obj)  (IS_WEAPON(obj) ? GET_OBJ_VAL((obj), VAL_WEAPON_DAMAGE_BONUS) : 0)
#define GET_WEAPON_TYPE(obj)  (IS_WEAPON(obj) ? GET_OBJ_VAL((obj), VAL_WEAPON_TYPE) : TYPE_UNDEFINED)

#define IS_ANY_WEAPON(obj)  (IS_WEAPON(obj) || IS_MISSILE_WEAPON(obj))
#define IS_STAFF(obj)  (GET_WEAPON_TYPE(obj) == TYPE_LIGHTNING_STAFF || GET_WEAPON_TYPE(obj) == TYPE_BURN_STAFF || GET_WEAPON_TYPE(obj) == TYPE_AGONY_STAFF || OBJ_FLAGGED((obj), OBJ_STAFF))

// ITEM_ARMOR subtype
#define IS_ARMOR(obj)  (GET_OBJ_TYPE(obj) == ITEM_ARMOR)
#define VAL_ARMOR_TYPE  0
#define GET_ARMOR_TYPE(obj)  (IS_ARMOR(obj) ? GET_OBJ_VAL((obj), VAL_ARMOR_TYPE) : NOTHING)

// ITEM_WORN subtype
#define IS_WORN_TYPE(obj)  (GET_OBJ_TYPE(obj) == ITEM_WORN)

// ITEM_OTHER
// ITEM_CONTAINER
#define IS_CONTAINER(obj)  (GET_OBJ_TYPE(obj) == ITEM_CONTAINER)
#define VAL_CONTAINER_MAX_CONTENTS  0
#define VAL_CONTAINER_FLAGS  1
#define GET_MAX_CONTAINER_CONTENTS(obj)  (IS_CONTAINER(obj) ? GET_OBJ_VAL((obj), VAL_CONTAINER_MAX_CONTENTS) : 0)
#define GET_CONTAINER_FLAGS(obj)  (IS_CONTAINER(obj) ? GET_OBJ_VAL((obj), VAL_CONTAINER_FLAGS) : 0)

// ITEM_DRINKCON
#define IS_DRINK_CONTAINER(obj)  (GET_OBJ_TYPE(obj) == ITEM_DRINKCON)
#define VAL_DRINK_CONTAINER_CAPACITY  0
#define VAL_DRINK_CONTAINER_CONTENTS  1
#define VAL_DRINK_CONTAINER_TYPE  2
#define GET_DRINK_CONTAINER_CAPACITY(obj)  (IS_DRINK_CONTAINER(obj) ? GET_OBJ_VAL((obj), VAL_DRINK_CONTAINER_CAPACITY) : 0)
#define GET_DRINK_CONTAINER_CONTENTS(obj)  (IS_DRINK_CONTAINER(obj) ? GET_OBJ_VAL((obj), VAL_DRINK_CONTAINER_CONTENTS) : 0)
#define GET_DRINK_CONTAINER_TYPE(obj)  (IS_DRINK_CONTAINER(obj) ? GET_OBJ_VAL((obj), VAL_DRINK_CONTAINER_TYPE) : NOTHING)

// ITEM_FOOD
#define IS_FOOD(obj)  (GET_OBJ_TYPE(obj) == ITEM_FOOD)
#define VAL_FOOD_HOURS_OF_FULLNESS  0
#define VAL_FOOD_CROP_TYPE  1
#define GET_FOOD_HOURS_OF_FULLNESS(obj)  (IS_FOOD(obj) ? GET_OBJ_VAL((obj), VAL_FOOD_HOURS_OF_FULLNESS) : 0)
#define IS_PLANTABLE_FOOD(obj)  (IS_FOOD(obj) && OBJ_FLAGGED((obj), OBJ_PLANTABLE))
#define GET_FOOD_CROP_TYPE(obj)  (IS_PLANTABLE_FOOD(obj) ? GET_OBJ_VAL((obj), VAL_FOOD_CROP_TYPE) : NOTHING)

// ITEM_BOARD
// ITEM_CORPSE
#define IS_CORPSE(obj)  (GET_OBJ_TYPE(obj) == ITEM_CORPSE)
#define VAL_CORPSE_IDNUM  0
#define VAL_CORPSE_FLAGS  2
#define IS_NPC_CORPSE(obj)  (IS_CORPSE(obj) && GET_OBJ_VAL((obj), VAL_CORPSE_IDNUM) >= 0)
#define IS_PC_CORPSE(obj)  (IS_CORPSE(obj) && GET_OBJ_VAL((obj), VAL_CORPSE_IDNUM) < 0)
#define GET_CORPSE_NPC_VNUM(obj)  (IS_NPC_CORPSE(obj) ? GET_OBJ_VAL((obj), VAL_CORPSE_IDNUM) : NOBODY)
#define GET_CORPSE_PC_ID(obj)  (IS_PC_CORPSE(obj) ? (-1 * GET_OBJ_VAL((obj), VAL_CORPSE_IDNUM)) : NOBODY)
#define GET_CORPSE_FLAGS(obj)  (IS_CORPSE(obj) ? GET_OBJ_VAL((obj), VAL_CORPSE_FLAGS) : 0)
#define IS_CORPSE_FLAG_SET(obj, flag)  (GET_CORPSE_FLAGS(obj) & (flag))

// ITEM_COINS
#define IS_COINS(obj)  (GET_OBJ_TYPE(obj) == ITEM_COINS)
#define VAL_COINS_AMOUNT  0
#define VAL_COINS_EMPIRE_ID  1
#define GET_COINS_AMOUNT(obj)  (IS_COINS(obj) ? GET_OBJ_VAL((obj), VAL_COINS_AMOUNT) : 0)
#define GET_COINS_EMPIRE_ID(obj)  (IS_COINS(obj) ? GET_OBJ_VAL((obj), VAL_COINS_EMPIRE_ID) : 0)

// ITEM_MAIL

// ITEM_WEALTH
#define IS_WEALTH_ITEM(obj)  (GET_OBJ_TYPE(obj) == ITEM_WEALTH)
#define VAL_WEALTH_VALUE  0
#define VAL_WEALTH_AUTOMINT  1
#define GET_WEALTH_VALUE(obj)  (IS_WEALTH_ITEM(obj) ? GET_OBJ_VAL((obj), VAL_WEALTH_VALUE) : 0)
#define GET_WEALTH_AUTOMINT(obj)  (IS_WEALTH_ITEM(obj) ? GET_OBJ_VAL((obj), VAL_WEALTH_AUTOMINT) : 0)

// ITEM_MISSILE_WEAPON
#define IS_MISSILE_WEAPON(obj)  (GET_OBJ_TYPE(obj) == ITEM_MISSILE_WEAPON)
#define VAL_MISSILE_WEAPON_SPEED  0
#define VAL_MISSILE_WEAPON_DAMAGE  1
#define VAL_MISSILE_WEAPON_TYPE  2
#define GET_MISSILE_WEAPON_SPEED(obj)  (IS_MISSILE_WEAPON(obj) ? GET_OBJ_VAL((obj), VAL_MISSILE_WEAPON_SPEED) : 0)
#define GET_MISSILE_WEAPON_DAMAGE(obj)  (IS_MISSILE_WEAPON(obj) ? GET_OBJ_VAL((obj), VAL_MISSILE_WEAPON_DAMAGE) : 0)
#define GET_MISSILE_WEAPON_TYPE(obj)  (IS_MISSILE_WEAPON(obj) ? GET_OBJ_VAL((obj), VAL_MISSILE_WEAPON_TYPE) : NOTHING)

// ITEM_ARROW
#define IS_ARROW(obj)  (GET_OBJ_TYPE(obj) == ITEM_ARROW)
#define VAL_ARROW_QUANTITY  0
#define VAL_ARROW_DAMAGE_BONUS  1
#define VAL_ARROW_TYPE  2
#define GET_ARROW_DAMAGE_BONUS(obj)  (IS_ARROW(obj) ? GET_OBJ_VAL((obj), VAL_ARROW_DAMAGE_BONUS) : 0)
#define GET_ARROW_TYPE(obj)  (IS_ARROW(obj) ? GET_OBJ_VAL((obj), VAL_ARROW_TYPE) : NOTHING)
#define GET_ARROW_QUANTITY(obj)  (IS_ARROW(obj) ? GET_OBJ_VAL((obj), VAL_ARROW_QUANTITY) : 0)

// ITEM_INSTRUMENT
#define IS_INSTRUMENT(obj)  (GET_OBJ_TYPE(obj) == ITEM_INSTRUMENT)

// ITEM_SHIELD
#define IS_SHIELD(obj)  (GET_OBJ_TYPE(obj) == ITEM_SHIELD)

// ITEM_PORTAL
#define IS_PORTAL(obj)  (GET_OBJ_TYPE(obj) == ITEM_PORTAL)
#define VAL_PORTAL_TARGET_VNUM  0
#define GET_PORTAL_TARGET_VNUM(obj)  (IS_PORTAL(obj) ? GET_OBJ_VAL((obj), VAL_PORTAL_TARGET_VNUM) : NOWHERE)

// ITEM_PACK
#define IS_PACK(obj)  (GET_OBJ_TYPE(obj) == ITEM_PACK)
#define VAL_PACK_CAPACITY  0
#define GET_PACK_CAPACITY(obj)  (IS_PACK(obj) ? GET_OBJ_VAL((obj), VAL_PACK_CAPACITY) : 0)

// ITEM_BOOK
#define IS_BOOK(obj)  (GET_OBJ_TYPE(obj) == ITEM_BOOK)
#define VAL_BOOK_ID  0
#define GET_BOOK_ID(obj)  (IS_BOOK(obj) ? GET_OBJ_VAL((obj), VAL_BOOK_ID) : 0)


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER UTILS ////////////////////////////////////////////////////////////

// ch->char_specials: char_special_data
#define AFF_FLAGS(ch)  ((ch)->char_specials.affected_by)
#define INJURY_FLAGS(ch)  ((ch)->char_specials.injuries)
#define PLR_FLAGS(ch)  (REAL_CHAR(ch)->char_specials.act)
#define SYSLOG_FLAGS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->syslogs))
#define GET_MORPH(ch)  ((ch)->char_specials.morph)

// ch->player_specials: player_special_data
#define CAN_GAIN_NEW_SKILLS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->can_gain_new_skills))
#define CAN_GET_BONUS_SKILLS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->can_get_bonus_skills))
#define CREATION_ARCHETYPE(ch, pos)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->creation_archetype[pos]))
#define GET_ABILITY_HASH(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->ability_hash))
#define GET_ACCOUNT(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->account))
#define GET_ACTION(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action))
#define GET_ACTION_CYCLE(ch) CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action_cycle))
#define GET_ACTION_ROOM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action_room))
#define GET_ACTION_TIMER(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action_timer))
#define GET_ACTION_VNUM(ch, n)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action_vnum[(n)]))
#define GET_ACTION_RESOURCES(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action_resources))
#define GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->adventure_summon_return_location))
#define GET_ADVENTURE_SUMMON_RETURN_MAP(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->adventure_summon_return_map))
#define GET_ALIASES(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->aliases))
#define GET_APPARENT_AGE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->apparent_age))
#define GET_BAD_PWS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->bad_pws))
#define GET_BONUS_TRAITS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->bonus_traits))
#define GET_CLASS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->character_class))
#define GET_CLASS_PROGRESSION(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->class_progression))
#define GET_CLASS_ROLE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->class_role))
#define GET_COMBAT_METERS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->meters))
#define GET_COMPLETED_QUESTS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->completed_quests))
#define GET_COMPUTED_LEVEL(ch)  (GET_SKILL_LEVEL(ch) + GET_GEAR_LEVEL(ch))
#define GET_COND(ch, i)  CHECK_PLAYER_SPECIAL(REAL_CHAR(ch), (REAL_CHAR(ch)->player_specials->conditions[(i)]))
#define GET_CONFUSED_DIR(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->confused_dir))
#define GET_CREATION_ALT_ID(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->create_alt_id))
#define GET_CREATION_HOST(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->creation_host))
#define GET_CURRENT_SKILL_SET(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->current_skill_set))
#define GET_CUSTOM_COLOR(ch, pos)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->custom_colors[(pos)]))
#define GET_DAILY_BONUS_EXPERIENCE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->daily_bonus_experience))
#define GET_DAILY_CYCLE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->daily_cycle))
#define GET_DAILY_QUESTS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->daily_quests))
#define GET_DISGUISED_NAME(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->disguised_name))
#define GET_DISGUISED_SEX(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->disguised_sex))
#define GET_EXP_TODAY(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->exp_today))
#define GET_FACTIONS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->factions))
#define GET_FIGHT_MESSAGES(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->fight_messages))
#define GET_FIGHT_PROMPT(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->fight_prompt))
#define GET_GEAR_LEVEL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->gear_level))
#define GET_GRANT_FLAGS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->grants))
#define GET_GROUP_INVITE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->group_invite_by))
#define GET_HIGHEST_KNOWN_LEVEL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->highest_known_level))
#define GET_HISTORY(ch, type)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->channel_history[(type)]))
#define GET_IGNORE_LIST(ch, pos)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->ignore_list[(pos)]))
#define GET_IMMORTAL_LEVEL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->immortal_level))
#define GET_INVIS_LEV(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->invis_level))
#define GET_LARGEST_INVENTORY(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->largest_inventory))
#define GET_LASTNAME(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->lastname))
#define GET_LAST_CORPSE_ID(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_corpse_id))
#define GET_LAST_DEATH_TIME(ch)  CHECK_PLAYER_SPECIAL(REAL_CHAR(ch), (REAL_CHAR(ch)->player_specials->last_death_time))
#define GET_LAST_DIR(ch)  CHECK_PLAYER_SPECIAL(REAL_CHAR(ch), (REAL_CHAR(ch)->player_specials->last_direction))
#define GET_LAST_KNOWN_LEVEL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_known_level))
#define GET_LAST_ROOM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_room))
#define GET_LAST_TELL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_tell))
#define GET_LAST_TIP(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_tip))
#define GET_LOADROOM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->load_room))
#define GET_LOAD_ROOM_CHECK(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->load_room_check))
#define GET_MAIL_PENDING(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->mail_pending))
#define GET_MAPSIZE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->mapsize))
#define GET_MARK_LOCATION(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->marked_location))
#define GET_MOUNT_FLAGS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->mount_flags))
#define GET_MOUNT_LIST(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->mount_list))
#define GET_MOUNT_VNUM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->mount_vnum))
#define GET_MOVE_TIME(ch, pos)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->move_time[(pos)]))
#define GET_OFFERS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->offers))
#define GET_OLC_FLAGS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->olc_flags))
#define GET_OLC_MAX_VNUM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->olc_max_vnum))
#define GET_OLC_MIN_VNUM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->olc_min_vnum))
#define GET_PLAYER_COINS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->coins))
#define GET_PLEDGE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->pledge))
#define GET_PROMO_ID(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->promo_id))
#define GET_PROMPT(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->prompt))
#define GET_QUESTS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->quests))
#define GET_RANK(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->rank))
#define GET_RECENT_DEATH_COUNT(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->recent_death_count))
#define GET_REFERRED_BY(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->referred_by))
#define GET_RESOURCE(ch, i)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->resources[i]))
#define GET_REWARDED_TODAY(ch, pos)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->rewarded_today[(pos)]))
#define GET_SKILL_HASH(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->skill_hash))
#define GET_SKILL_LEVEL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->skill_level))
#define GET_SLASH_CHANNELS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->slash_channels))
#define GET_SLASH_HISTORY(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->slash_history))
#define GET_TEMPORARY_ACCOUNT_ID(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->temporary_account_id))
#define GET_TITLE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->title))
#define GET_TOMB_ROOM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->tomb_room))
#define IS_DISGUISED(ch)  (!IS_NPC(ch) && PLR_FLAGGED((ch), PLR_DISGUISED))
#define LOAD_SLASH_CHANNELS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->load_slash_channels))
#define NEEDS_DELAYED_LOAD(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->needs_delayed_load))
#define POOFIN(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->poofin))
#define POOFOUT(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->poofout))
#define PRF_FLAGS(ch)  CHECK_PLAYER_SPECIAL(REAL_CHAR(ch), (REAL_CHAR(ch)->player_specials->pref))
#define REBOOT_CONF(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->reboot_conf))
#define REREAD_EMPIRE_TECH_ON_LOGIN(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->reread_empire_tech_on_login))
#define RESTORE_ON_LOGIN(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->restore_on_login))
#define USING_POISON(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->using_poison))

// helpers
#define ACCOUNT_FLAGGED(ch, flag)  (!IS_NPC(ch) && GET_ACCOUNT(ch) && IS_SET(GET_ACCOUNT(ch)->flags, (flag)))
#define HAS_BONUS_TRAIT(ch, flag)  (!IS_NPC(ch) && IS_SET(GET_BONUS_TRAITS(ch), (flag)))
#define IS_AFK(ch)  (!IS_NPC(ch) && (PRF_FLAGGED((ch), PRF_AFK) || ((ch)->char_specials.timer * SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN) >= 10))
#define IS_GRANTED(ch, flag)  (!IS_NPC(ch) && IS_SET(GET_GRANT_FLAGS(ch), (flag)))
#define IS_MORPHED(ch)  (GET_MORPH(ch) != NULL)
#define IS_PVP_FLAGGED(ch)  (!IS_NPC(ch) && (PRF_FLAGGED((ch), PRF_ALLOW_PVP) || get_cooldown_time((ch), COOLDOWN_PVP_FLAG) > 0))
#define MOUNT_FLAGGED(ch, flag)  (!IS_NPC(ch) && IS_SET(GET_MOUNT_FLAGS(ch), (flag)))
#define NOHASSLE(ch)  (!IS_NPC(ch) && IS_IMMORTAL(ch) && PRF_FLAGGED((ch), PRF_NOHASSLE))
#define PLR_FLAGGED(ch, flag)  (!REAL_NPC(ch) && IS_SET(PLR_FLAGS(ch), (flag)))
#define PRF_FLAGGED(ch, flag)  (!REAL_NPC(ch) && IS_SET(PRF_FLAGS(ch), (flag)))
#define OLC_FLAGGED(ch, flag)  (!IS_NPC(ch) && IS_SET(GET_OLC_FLAGS(ch), (flag)))
#define SAVE_ACCOUNT(acct)  save_library_file_for_vnum(DB_BOOT_ACCT, (acct)->id)
#define SHOW_CLASS_ABBREV(ch)  ((!IS_NPC(ch) && GET_CLASS(ch)) ? CLASS_ABBREV(GET_CLASS(ch)) : config_get_string("default_class_abbrev"))
#define SHOW_CLASS_NAME(ch)  ((!IS_NPC(ch) && GET_CLASS(ch)) ? CLASS_NAME(GET_CLASS(ch)) : config_get_string("default_class_name"))
#define SHOW_FIGHT_MESSAGES(ch, bit)  (!IS_NPC(ch) && IS_SET(GET_FIGHT_MESSAGES(ch), (bit)))

// definitions
#define IN_HOSTILE_TERRITORY(ch)  (!IS_NPC(ch) && !IS_IMMORTAL(ch) && ROOM_OWNER(IN_ROOM(ch)) && ROOM_OWNER(IN_ROOM(ch)) != GET_LOYALTY(ch) && (IS_HOSTILE(ch) || empire_is_hostile(ROOM_OWNER(IN_ROOM(ch)), GET_LOYALTY(ch), IN_ROOM(ch))))
#define IS_APPROVED(ch)  (IS_NPC(ch) || PLR_FLAGGED(ch, PLR_APPROVED) || ACCOUNT_FLAGGED(ch, ACCT_APPROVED))
#define IS_HOSTILE(ch)  (!IS_NPC(ch) && (get_cooldown_time((ch), COOLDOWN_HOSTILE_FLAG) > 0 || get_cooldown_time((ch), COOLDOWN_ROGUE_FLAG) > 0))
#define IS_HUNGRY(ch)  (GET_COND(ch, FULL) >= 360 && !has_ability(ch, ABIL_UNNATURAL_THIRST))
#define IS_DRUNK(ch)  (GET_COND(ch, DRUNK) >= 360)
#define IS_GOD(ch)  (GET_ACCESS_LEVEL(ch) == LVL_GOD)
#define IS_IMMORTAL(ch)  (GET_ACCESS_LEVEL(ch) >= LVL_START_IMM)
#define IS_RIDING(ch)  (!IS_NPC(ch) && GET_MOUNT_VNUM(ch) != NOTHING && MOUNT_FLAGGED(ch, MOUNT_RIDING))
#define IS_THIRSTY(ch)  (GET_COND(ch, THIRST) >= 360 && !has_ability(ch, ABIL_UNNATURAL_THIRST) && !has_ability(ch, ABIL_SATED_THIRST))
#define IS_BLOOD_STARVED(ch)  (IS_VAMPIRE(ch) && GET_BLOOD(ch) <= config_get_int("blood_starvation_level"))

// for act() and act-like things (requires to_sleeping and is_spammy set to true/false)
#define SENDOK(ch)  (((ch)->desc || SCRIPT_CHECK((ch), MTRIG_ACT)) && (to_sleeping || AWAKE(ch)) && (!PRF_FLAGGED(ch, PRF_NOSPAM) || !is_spammy))


 //////////////////////////////////////////////////////////////////////////////
//// QUEST UTILS /////////////////////////////////////////////////////////////

#define QUEST_VNUM(quest)  ((quest)->vnum)
#define QUEST_COMPLETE_MSG(quest)  ((quest)->complete_msg)
#define QUEST_DAILY_ACTIVE(quest)  ((quest)->daily_active)
#define QUEST_DAILY_CYCLE(quest)  ((quest)->daily_cycle)
#define QUEST_DESCRIPTION(quest)  ((quest)->description)
#define QUEST_ENDS_AT(quest)  ((quest)->ends_at)
#define QUEST_FLAGS(quest)  ((quest)->flags)
#define QUEST_MAX_LEVEL(quest)  ((quest)->max_level)
#define QUEST_MIN_LEVEL(quest)  ((quest)->min_level)
#define QUEST_NAME(quest)  ((quest)->name)
#define QUEST_PREREQS(quest)  ((quest)->prereqs)
#define QUEST_REPEATABLE_AFTER(quest)  ((quest)->repeatable_after)
#define QUEST_REWARDS(quest)  ((quest)->rewards)
#define QUEST_SCRIPTS(quest)  ((quest)->proto_script)
#define QUEST_STARTS_AT(quest)  ((quest)->starts_at)
#define QUEST_TASKS(quest)  ((quest)->tasks)
#define QUEST_VERSION(quest)  ((quest)->version)

#define QUEST_FLAGGED(quest, flg)  IS_SET(QUEST_FLAGS(quest), (flg))


 //////////////////////////////////////////////////////////////////////////////
//// ROOM UTILS //////////////////////////////////////////////////////////////

// basic room data
#define GET_ROOM_VNUM(room)  ((room)->vnum)
#define ROOM_AFF_FLAGS(room)  ((room)->affects)
#define ROOM_AFFECTS(room)  ((room)->af)
#define ROOM_BASE_FLAGS(room)  ((room)->base_affects)
#define ROOM_CONTENTS(room)  ((room)->contents)
#define ROOM_CROP(room)  ((room)->crop_type)
#define ROOM_DEPLETION(room)  ((room)->depletion)
#define ROOM_LAST_SPAWN_TIME(room)  ((room)->last_spawn_time)
#define ROOM_LIGHTS(room)  ((room)->light)
#define BASE_SECT(room)  ((room)->base_sector)
#define ROOM_OWNER(room)  ((room)->owner)
#define ROOM_PEOPLE(room)  ((room)->people)
#define ROOM_TRACKS(room)  ((room)->tracks)
#define ROOM_VEHICLES(room)  ((room)->vehicles)
#define SECT(room)  ((room)->sector_type)
#define GET_EXITS_HERE(room)  ((room)->exits_here)


// room->complex data
#define COMPLEX_DATA(room)  ((room)->complex)
#define GET_BUILDING(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->bld_ptr : NULL)
#define GET_ROOM_TEMPLATE(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->rmt_ptr : NULL)
#define IN_VEHICLE_IN_ROOM(room)  (GET_ROOM_VEHICLE(room) ? IN_ROOM(GET_ROOM_VEHICLE(room)) : room)
#define BUILDING_BURNING(room)  (COMPLEX_DATA(HOME_ROOM(room)) ? COMPLEX_DATA(HOME_ROOM(room))->burning : 0)
#define BUILDING_DAMAGE(room)  (COMPLEX_DATA(HOME_ROOM(room)) ? COMPLEX_DATA(HOME_ROOM(room))->damage : 0)
#define BUILDING_ENTRANCE(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->entrance : NO_DIR)
#define BUILDING_RESOURCES(room)  (COMPLEX_DATA(room) ? GET_BUILDING_RESOURCES(room) : NULL)
#define GET_ROOM_VEHICLE(room)  (COMPLEX_DATA(HOME_ROOM(room)) ? COMPLEX_DATA(HOME_ROOM(room))->vehicle : NULL)
#define GET_BUILDING_RESOURCES(room)  (COMPLEX_DATA(room)->to_build)
#define GET_BUILT_WITH(room)  (COMPLEX_DATA(room)->built_with)
#define GET_INSIDE_ROOMS(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->inside_rooms : 0)
#define HOME_ROOM(room)  ((COMPLEX_DATA(room) && COMPLEX_DATA(room)->home_room) ? COMPLEX_DATA(room)->home_room : (room))
#define IS_COMPLETE(room)  (!IS_INCOMPLETE(room) && !IS_DISMANTLING(room))
#define ROOM_PATRON(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->patron : NOBODY)
#define ROOM_PRIVATE_OWNER(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->private_owner : NOBODY)
#define ROOM_INSTANCE(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->instance : NULL)


// exits
#define CAN_GO(ch, ex) (ex->room_ptr && !EXIT_FLAGGED(ex, EX_CLOSED))
#define EXIT_FLAGGED(exit, flag)  (IS_SET((exit)->exit_info, (flag)))

// building data by room
#define BLD_MAX_ROOMS(room)  (GET_BUILDING(HOME_ROOM(room)) ? GET_BLD_EXTRA_ROOMS(GET_BUILDING(HOME_ROOM(room))) : 0)
#define BLD_BASE_AFFECTS(room)  (GET_BUILDING(HOME_ROOM(room)) ? GET_BLD_BASE_AFFECTS(GET_BUILDING(HOME_ROOM(room))) : NOBITS)

// customs
#define ROOM_CUSTOM_NAME(room)  ((room)->name)
#define ROOM_CUSTOM_ICON(room)  ((room)->icon)
#define ROOM_CUSTOM_DESCRIPTION(room)  ((room)->description)

// definitions
#define BLD_ALLOWS_MOUNTS(room)  (ROOM_IS_CLOSED(room) ? (ROOM_BLD_FLAGGED((room), BLD_ALLOW_MOUNTS | BLD_OPEN) || RMT_FLAGGED((room), RMT_OUTDOOR)) : TRUE)
#define CAN_CHOP_ROOM(room)  (has_evolution_type(SECT(room), EVO_CHOPPED_DOWN) || CAN_INTERACT_ROOM((room), INTERACT_CHOP) || (ROOM_SECT_FLAGGED((room), SECTF_CROP) && ROOM_CROP_FLAGGED((room), CROPF_IS_ORCHARD)))
#define DEPLETION_LIMIT(room)  (ROOM_BLD_FLAGGED((room), BLD_HIGH_DEPLETION) ? config_get_int("high_depletion") : config_get_int("common_depletion"))
#define HAS_MINOR_DISREPAIR(room)  (HOME_ROOM(room) == room && GET_BUILDING(room) && BUILDING_DAMAGE(room) > 0 && (BUILDING_DAMAGE(room) >= (GET_BLD_MAX_DAMAGE(GET_BUILDING(room)) * config_get_int("disrepair_minor") / 100)))
#define HAS_MAJOR_DISREPAIR(room)  (HOME_ROOM(room) == room && GET_BUILDING(room) && BUILDING_DAMAGE(room) > 0 && (BUILDING_DAMAGE(room) >= (GET_BLD_MAX_DAMAGE(GET_BUILDING(room)) * config_get_int("disrepair_major") / 100)))
#define IS_CITY_CENTER(room)  (BUILDING_VNUM(room) == BUILDING_CITY_CENTER)
#define IS_DARK(room)  (MAGIC_DARKNESS(room) || (!IS_ANY_BUILDING(room) && ROOM_LIGHTS(room) == 0 && (!ROOM_OWNER(room) || !EMPIRE_HAS_TECH(ROOM_OWNER(room), TECH_CITY_LIGHTS)) && !RMT_FLAGGED((room), RMT_LIGHT) && (weather_info.sunlight == SUN_DARK || RMT_FLAGGED((room), RMT_DARK))))
#define IS_LIGHT(room)  (!MAGIC_DARKNESS(room) && WOULD_BE_LIGHT_WITHOUT_MAGIC_DARKNESS(room))
#define IS_REAL_LIGHT(room)  (!MAGIC_DARKNESS(room) && (!IS_DARK(room) || RMT_FLAGGED((room), RMT_LIGHT) || IS_INSIDE(room) || (ROOM_OWNER(room) && IS_ANY_BUILDING(room))))
#define IS_RUINS(room)  (BUILDING_VNUM(room) == BUILDING_RUINS_OPEN || BUILDING_VNUM(room) == BUILDING_RUINS_CLOSED || BUILDING_VNUM(room) == BUILDING_RUINS_FLOODED)	// TODO: some new designation for ruins and a more procedural way to set it up or configure it in-game?
#define ISLAND_FLAGGED(room, flag)  ((GET_ISLAND_ID(room) != NO_ISLAND) ? IS_SET(get_island(GET_ISLAND_ID(room), TRUE)->flags, (flag)) : FALSE)
#define MAGIC_DARKNESS(room)  (ROOM_AFF_FLAGGED((room), ROOM_AFF_DARK))
#define ROOM_CAN_MINE(room)  (ROOM_SECT_FLAGGED((room), SECTF_CAN_MINE) || HAS_FUNCTION((room), FNC_MINE) || (IS_ROAD(room) && SECT_FLAGGED(BASE_SECT(room), SECTF_CAN_MINE)))
#define ROOM_IS_CLOSED(room)  (IS_INSIDE(room) || IS_ADVENTURE_ROOM(room) || (IS_ANY_BUILDING(room) && !ROOM_BLD_FLAGGED(room, BLD_OPEN) && (IS_COMPLETE(room) || ROOM_BLD_FLAGGED(room, BLD_CLOSED))))
#define SHOW_PEOPLE_IN_ROOM(room)  (!ROOM_IS_CLOSED(room) && !ROOM_SECT_FLAGGED(room, SECTF_OBSCURE_VISION))
#define WOULD_BE_LIGHT_WITHOUT_MAGIC_DARKNESS(room)  (!IS_DARK(room) || RMT_FLAGGED((room), RMT_LIGHT) || adjacent_room_is_light(room) || IS_ANY_BUILDING(room))

// interaction checks (leading up to CAN_INTERACT_ROOM)
#define BLD_CAN_INTERACT_ROOM(room, type)  (GET_BUILDING(room) && has_interaction(GET_BLD_INTERACTIONS(GET_BUILDING(room)), (type)))
#define CROP_CAN_INTERACT_ROOM(room, type)  (ROOM_SECT_FLAGGED(room, SECTF_CROP) && ROOM_CROP(room) && has_interaction(GET_CROP_INTERACTIONS(ROOM_CROP(room)), (type)))
#define RMT_CAN_INTERACT_ROOM(room, type)  (GET_ROOM_TEMPLATE(room) && has_interaction(GET_RMT_INTERACTIONS(GET_ROOM_TEMPLATE(room)), (type)))
#define SECT_CAN_INTERACT_ROOM(room, type)  has_interaction(GET_SECT_INTERACTIONS(SECT(room)), (type))
#define CAN_INTERACT_ROOM(room, type)  (SECT_CAN_INTERACT_ROOM((room), (type)) || BLD_CAN_INTERACT_ROOM((room), (type)) || RMT_CAN_INTERACT_ROOM((room), (type)) || CROP_CAN_INTERACT_ROOM((room), (type)))

// fundamental types
#define BUILDING_VNUM(room)  (GET_BUILDING(room) ? GET_BLD_VNUM(GET_BUILDING(room)) : NOTHING)
#define ROOM_TEMPLATE_VNUM(room)  (GET_ROOM_TEMPLATE(room) ? GET_RMT_VNUM(GET_ROOM_TEMPLATE(room)) : NOTHING)

// helpers
#define BLD_DESIGNATE_FLAGGED(room, flag)  (GET_BUILDING(HOME_ROOM(room)) && IS_SET(GET_BLD_DESIGNATE_FLAGS(GET_BUILDING(HOME_ROOM(room))), (flag)))
#define CHECK_CHAMELEON(from_room, to_room)  (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_CHAMELEON) && IS_COMPLETE(to_room) && compute_distance(from_room, to_room) >= 2)
#define HAS_FUNCTION(room, flag)  ((GET_BUILDING(room) && IS_SET(GET_BLD_FUNCTIONS(GET_BUILDING(room)), (flag))) || (GET_ROOM_TEMPLATE(room) && IS_SET(GET_RMT_FUNCTIONS(GET_ROOM_TEMPLATE(room)), (flag))))
#define RMT_FLAGGED(room, flag)  (GET_ROOM_TEMPLATE(room) && IS_SET(GET_RMT_FLAGS(GET_ROOM_TEMPLATE(room)), (flag)))
#define ROOM_AFF_FLAGGED(r, flag)  (IS_SET(ROOM_AFF_FLAGS(r), (flag)))
#define ROOM_BLD_FLAGGED(room, flag)  (GET_BUILDING(room) && IS_SET(GET_BLD_FLAGS(GET_BUILDING(room)), (flag)))
#define ROOM_CROP_FLAGGED(room, flg)  (ROOM_SECT_FLAGGED((room), SECTF_HAS_CROP_DATA) && ROOM_CROP(room) && CROP_FLAGGED(ROOM_CROP(room), (flg)))
#define ROOM_SECT_FLAGGED(room, flg)  SECT_FLAGGED(SECT(room), (flg))
#define SHIFT_CHAR_DIR(ch, room, dir)  SHIFT_DIR((room), confused_dirs[get_north_for_char(ch)][0][(dir)])
#define SHIFT_DIR(room, dir)  real_shift((room), shift_dir[(dir)][0], shift_dir[(dir)][1])

// island info
extern int GET_ISLAND_ID(room_data *room);	// formerly #define GET_ISLAND_ID(room)  (get_map_location_for(room)->island)
void SET_ISLAND_ID(room_data *room, int island);	// formerly a #define and a room_data property

// room types
#define IS_ADVENTURE_ROOM(room)  ROOM_SECT_FLAGGED((room), SECTF_ADVENTURE)
#define IS_ANY_BUILDING(room)  ROOM_SECT_FLAGGED((room), SECTF_MAP_BUILDING | SECTF_INSIDE)
#define IS_DISMANTLING(room)  (ROOM_AFF_FLAGGED((room), ROOM_AFF_DISMANTLING))
#define IS_INCOMPLETE(room)  (ROOM_AFF_FLAGGED((room), ROOM_AFF_INCOMPLETE))
#define IS_INSIDE(room)  ROOM_SECT_FLAGGED((room), SECTF_INSIDE)
#define IS_MAP_BUILDING(room)  ROOM_SECT_FLAGGED((room), SECTF_MAP_BUILDING)
#define IS_OUTDOOR_TILE(room)  (RMT_FLAGGED((room), RMT_OUTDOOR) || (!IS_ADVENTURE_ROOM(room) && (!IS_ANY_BUILDING(room) || (IS_MAP_BUILDING(room) && ROOM_BLD_FLAGGED((room), BLD_OPEN)))))
#define IS_PUBLIC(room)  (ROOM_AFF_FLAGGED((room), ROOM_AFF_PUBLIC))
#define IS_ROAD(room)  ROOM_SECT_FLAGGED((room), SECTF_IS_ROAD)
#define IS_UNCLAIMABLE(room)  (ROOM_AFF_FLAGGED((room), ROOM_AFF_UNCLAIMABLE))
#define IS_WATER_BUILDING(room)  (ROOM_BLD_FLAGGED((room), BLD_SAIL))
#define WATER_SECT(room)		(ROOM_SECT_FLAGGED((room), SECTF_FRESH_WATER | SECTF_OCEAN) || ROOM_BLD_FLAGGED((room), BLD_NEED_BOAT) || RMT_FLAGGED((room), RMT_NEED_BOAT) || (IS_WATER_BUILDING(room) && !IS_COMPLETE(room) && SECT_FLAGGED(BASE_SECT(room), SECTF_FRESH_WATER | SECTF_OCEAN)))
#define DEEP_WATER_SECT(room)	(ROOM_SECT_FLAGGED((room), SECTF_OCEAN))


 //////////////////////////////////////////////////////////////////////////////
//// ROOM TEMPLATE UTILS /////////////////////////////////////////////////////

#define GET_RMT_VNUM(rmt)  ((rmt)->vnum)
#define GET_RMT_TITLE(rmt)  ((rmt)->title)
#define GET_RMT_DESC(rmt)  ((rmt)->description)
#define GET_RMT_FLAGS(rmt)  ((rmt)->flags)
#define GET_RMT_FUNCTIONS(rmt)  ((rmt)->functions)
#define GET_RMT_BASE_AFFECTS(rmt)  ((rmt)->base_affects)
#define GET_RMT_SPAWNS(rmt)  ((rmt)->spawns)
#define GET_RMT_EX_DESCS(rmt)  ((rmt)->ex_description)
#define GET_RMT_EXITS(rmt)  ((rmt)->exits)
#define GET_RMT_INTERACTIONS(rmt)  ((rmt)->interactions)
#define GET_RMT_QUEST_LOOKUPS(rmt)  ((rmt)->quest_lookups)
#define GET_RMT_SCRIPTS(rmt)  ((rmt)->proto_script)


 //////////////////////////////////////////////////////////////////////////////
//// SECTOR UTILS ////////////////////////////////////////////////////////////

#define GET_SECT_BUILD_FLAGS(sect)  ((sect)->build_flags)
#define GET_SECT_CLIMATE(sect)  ((sect)->climate)
#define GET_SECT_COMMANDS(sect)  ((sect)->commands)
#define GET_SECT_EVOS(sect)  ((sect)->evolution)
#define GET_SECT_FLAGS(sect)  ((sect)->flags)
#define GET_SECT_ICONS(sect)  ((sect)->icons)
#define GET_SECT_INTERACTIONS(sect)  ((sect)->interactions)
#define GET_SECT_MAPOUT(sect)  ((sect)->mapout)
#define GET_SECT_MOVE_LOSS(sect)  ((sect)->movement_loss)
#define GET_SECT_NAME(sect)  ((sect)->name)
#define GET_SECT_ROADSIDE_ICON(sect)  ((sect)->roadside_icon)
#define GET_SECT_SPAWNS(sect)  ((sect)->spawns)
#define GET_SECT_TITLE(sect)  ((sect)->title)
#define GET_SECT_VNUM(sect)  ((sect)->vnum)

// utils
#define SECT_FLAGGED(sct, flg)  (IS_SET(GET_SECT_FLAGS(sct), (flg)))


 //////////////////////////////////////////////////////////////////////////////
//// SKILL UTILS /////////////////////////////////////////////////////////////

#define SKILL_ABBREV(skill)  ((skill)->abbrev)
#define SKILL_ABILITIES(skill)  ((skill)->abilities)
#define SKILL_DESC(skill)  ((skill)->desc)
#define SKILL_FLAGS(skill)  ((skill)->flags)
#define SKILL_MAX_LEVEL(skill)  ((skill)->max_level)
#define SKILL_MIN_DROP_LEVEL(skill)  ((skill)->min_drop_level)
#define SKILL_NAME(skill)  ((skill)->name)
#define SKILL_VNUM(skill)  ((skill)->vnum)

// utils
#define SKILL_FLAGGED(skill, flag)  IS_SET(SKILL_FLAGS(skill), (flag))


 //////////////////////////////////////////////////////////////////////////////
//// SOCIAL UTILS ////////////////////////////////////////////////////////////

#define SOC_COMMAND(soc)  ((soc)->command)
#define SOC_FLAGS(soc)  ((soc)->flags)
#define SOC_MESSAGE(soc, num)  ((soc)->message[num])
#define SOC_MIN_CHAR_POS(soc)  ((soc)->min_char_position)
#define SOC_MIN_VICT_POS(soc)  ((soc)->min_victim_position)
#define SOC_NAME(soc)  ((soc)->name)
#define SOC_REQUIREMENTS(soc)  ((soc)->requirements)
#define SOC_VNUM(soc)  ((soc)->vnum)

// definitions
#define SOCIAL_FLAGGED(soc, flag)  IS_SET(SOC_FLAGS(soc), (flag))
#define SOC_HIDDEN(soc)  (SOCIAL_FLAGGED((soc), SOC_HIDE_IF_INVIS))
#define SOC_IS_TARGETABLE(soc)  (SOC_MESSAGE((soc), SOCM_TARGETED_TO_CHAR) != NULL)


 //////////////////////////////////////////////////////////////////////////////
//// STRING UTILS ////////////////////////////////////////////////////////////

#define HSHR(ch)  (MOB_FLAGGED((ch), MOB_PLURAL) ? "their" : ((GET_SEX(ch) && !CHAR_MORPH_FLAGGED(ch, MORPHF_GENDER_NEUTRAL)) ? (GET_SEX(ch) == SEX_MALE ? "his":"her") :"its"))
#define HSSH(ch)  (MOB_FLAGGED((ch), MOB_PLURAL) ? "they" : ((GET_SEX(ch) && !CHAR_MORPH_FLAGGED(ch, MORPHF_GENDER_NEUTRAL)) ? (GET_SEX(ch) == SEX_MALE ? "he" :"she") : "it"))
#define HMHR(ch)  (MOB_FLAGGED((ch), MOB_PLURAL) ? "them" : ((GET_SEX(ch) && !CHAR_MORPH_FLAGGED(ch, MORPHF_GENDER_NEUTRAL)) ? (GET_SEX(ch) == SEX_MALE ? "him":"her") : "it"))

#define REAL_HSHR(ch)  (MOB_FLAGGED((ch), MOB_PLURAL) ? "their" : (GET_REAL_SEX(ch) ? (GET_REAL_SEX(ch) == SEX_MALE ? "his":"her") :"its"))
#define REAL_HSSH(ch)  (MOB_FLAGGED((ch), MOB_PLURAL) ? "they" : (GET_REAL_SEX(ch) ? (GET_REAL_SEX(ch) == SEX_MALE ? "he" :"she") : "it"))
#define REAL_HMHR(ch)  (MOB_FLAGGED((ch), MOB_PLURAL) ? "them" : (GET_REAL_SEX(ch) ? (GET_REAL_SEX(ch) == SEX_MALE ? "him":"her") : "it"))

#define AN(string)  (strchr("aeiouAEIOU", *string) ? "an" : "a")
#define SANA(obj)  (strchr("aeiouyAEIOUY", *(obj)->name) ? "an" : "a")
#define ANA(obj)  (strchr("aeiouyAEIOUY", *(obj)->name) ? "An" : "A")

#define LOWER(c)  (((c)>='A'  && (c) <= 'Z') ? ((c)+('a'-'A')) : (c))
#define UPPER(c)  (((c)>='a'  && (c) <= 'z') ? ((c)+('A'-'a')) : (c) )

#define ISNEWL(ch) ((ch) == '\n' || (ch) == '\r') 
#define NULLSAFE(foo)  ((foo) ? (foo) : "")

#define PLURAL(num)  ((num) != 1 ? "s" : "")

#define YESNO(a) ((a) ? "YES" : "NO")
#define ONOFF(a) ((a) ? "ON" : "OFF")


 //////////////////////////////////////////////////////////////////////////////
//// VEHICLE UTILS ///////////////////////////////////////////////////////////

// basic data
#define VEH_ANIMALS(veh)  ((veh)->animals)
#define VEH_CARRYING_N(veh)  ((veh)->carrying_n)
#define VEH_CONTAINS(veh)  ((veh)->contains)
#define VEH_DRIVER(veh)  ((veh)->driver)
#define VEH_FLAGS(veh)  ((veh)->flags)
#define VEH_HEALTH(veh)  ((veh)->health)
#define VEH_ICON(veh)  ((veh)->icon)
#define VEH_INSIDE_ROOMS(veh)  ((veh)->inside_rooms)
#define VEH_INTERIOR_HOME_ROOM(veh)  ((veh)->interior_home_room)
#define VEH_KEYWORDS(veh)  ((veh)->keywords)
#define VEH_LAST_FIRE_TIME(veh)  ((veh)->last_fire_time)
#define VEH_LAST_MOVE_TIME(veh)  ((veh)->last_move_time)
#define VEH_LED_BY(veh)  ((veh)->led_by)
#define VEH_LONG_DESC(veh)  ((veh)->long_desc)
#define VEH_LOOK_DESC(veh)  ((veh)->look_desc)
#define VEH_NEEDS_RESOURCES(veh)  ((veh)->needs_resources)
#define VEH_OWNER(veh)  ((veh)->owner)
#define VEH_ROOM_LIST(veh)  ((veh)->room_list)
#define VEH_SCALE_LEVEL(veh)  ((veh)->scale_level)
#define VEH_SHIPPING_ID(veh)  ((veh)->shipping_id)
#define VEH_SHORT_DESC(veh)  ((veh)->short_desc)
#define VEH_SITTING_ON(veh)  ((veh)->sitting_on)
#define VEH_VNUM(veh)  ((veh)->vnum)

// attribute (non-instanced) data
#define VEH_ANIMALS_REQUIRED(veh)  ((veh)->attributes->animals_required)
#define VEH_CAPACITY(veh)  ((veh)->attributes->capacity)
#define VEH_DESIGNATE_FLAGS(veh)  ((veh)->attributes->designate_flags)
#define VEH_INTERIOR_ROOM_VNUM(veh)  ((veh)->attributes->interior_room_vnum)
#define VEH_MAX_HEALTH(veh)  ((veh)->attributes->maxhealth)
#define VEH_MAX_ROOMS(veh)  ((veh)->attributes->max_rooms)
#define VEH_MAX_SCALE_LEVEL(veh)  ((veh)->attributes->max_scale_level)
#define VEH_MIN_SCALE_LEVEL(veh)  ((veh)->attributes->min_scale_level)
#define VEH_MOVE_TYPE(veh)  ((veh)->attributes->move_type)
#define VEH_YEARLY_MAINTENANCE(veh)  ((veh)->attributes->yearly_maintenance)

// helpers
#define IN_OR_ON(veh)		(VEH_FLAGGED((veh), VEH_IN) ? "in" : "on")
#define VEH_FLAGGED(veh, flag)  IS_SET(VEH_FLAGS(veh), (flag))
#define VEH_IS_COMPLETE(veh)  (!VEH_NEEDS_RESOURCES(veh) || !VEH_FLAGGED(veh, VEH_INCOMPLETE))


 //////////////////////////////////////////////////////////////////////////////
//// CONST EXTERNS ///////////////////////////////////////////////////////////

extern FILE *logfile;	// comm.c
extern const int shift_dir[][2];	// constants.c
extern struct weather_data weather_info;	// db.c


 //////////////////////////////////////////////////////////////////////////////
//// UTIL FUNCTION PROTOS ////////////////////////////////////////////////////

// log information
#define log  basic_mud_log

// basic functions from utils.c
extern bool any_players_in_room(room_data *room);
extern char *PERS(char_data *ch, char_data *vict, bool real);
extern double diminishing_returns(double val, double scale);
extern int att_max(char_data *ch);
extern int count_bits(bitvector_t bitset);
extern int dice(int number, int size);
extern int number(int from, int to);
extern struct time_info_data *age(char_data *ch);
extern struct time_info_data *mud_time_passed(time_t t2, time_t t1);
extern struct time_info_data *real_time_passed(time_t t2, time_t t1);

// empire utils from utils.c
extern int count_members_online(empire_data *emp);
extern int count_tech(empire_data *emp);

// empire diplomacy utils from utils.c
extern bool char_has_relationship(char_data *ch_a, char_data *ch_b, bitvector_t dipl_bits);
extern bool empire_is_friendly(empire_data *emp, empire_data *enemy);
extern bool empire_is_hostile(empire_data *emp, empire_data *enemy, room_data *loc);
extern bool has_empire_trait(empire_data *emp, room_data *loc, bitvector_t trait);
extern bool has_relationship(empire_data *emp, empire_data *fremp, bitvector_t diplomacy);

// interpreter utils from utils.c
extern char *any_one_arg(char *argument, char *first_arg);
extern char *any_one_word(char *argument, char *first_arg);
void comma_args(char *string, char *arg1, char *arg2);
extern int fill_word(char *argument);
void half_chop(char *string, char *arg1, char *arg2);
extern int is_abbrev(const char *arg1, const char *arg2);
extern bool is_multiword_abbrev(const char *arg, const char *phrase);
extern int is_number(const char *str);
extern char *one_argument(char *argument, char *first_arg);
extern char *one_word(char *argument, char *first_arg);
extern int reserved_word(char *argument);
extern int search_block(char *arg, const char **list, int exact);
void skip_spaces(char **string);
extern char *two_arguments(char *argument, char *first_arg, char *second_arg);
void ucwords(char *string);

// permission utils from utils.c
extern bool can_build_or_claim_at_war(char_data *ch, room_data *loc);
extern bool can_use_room(char_data *ch, room_data *room, int mode);
extern bool emp_can_use_room(empire_data *emp, room_data *room, int mode);
extern bool emp_can_use_vehicle(empire_data *emp, vehicle_data *veh, int mode);
#define can_use_vehicle(ch, veh, mode)  (IS_IMMORTAL(ch) || emp_can_use_vehicle(GET_LOYALTY(ch), (veh), (mode)))
extern bool has_permission(char_data *ch, int type);
extern bool has_tech_available(char_data *ch, int tech);
extern bool has_tech_available_room(room_data *room, int tech);
extern bool is_at_war(empire_data *emp);
extern int land_can_claim(empire_data *emp, bool outside_only);

// file utilities from utils.c
extern int get_filename(char *orig_name, char *filename, int mode);
extern int get_line(FILE *fl, char *buf);
extern int touch(const char *path);

// logging functions from utils.c
extern char *room_log_identifier(room_data *room);
void basic_mud_log(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
void log_to_empire(empire_data *emp, int type, const char *str, ...) __attribute__((format(printf, 3, 4)));
void mortlog(const char *str, ...) __attribute__((format(printf, 1, 2)));
void syslog(bitvector_t type, int level, bool file, const char *str, ...) __attribute__((format(printf, 4, 5)));

// mobile functions from utils.c
extern char *get_mob_name_by_proto(mob_vnum vnum);

// object functions from utils.c
extern char *get_obj_name_by_proto(obj_vnum vnum);
extern obj_data *get_top_object(obj_data *obj);
extern double rate_item(obj_data *obj);

// player functions from utils.c
void command_lag(char_data *ch, int wait_type);
void determine_gear_level(char_data *ch);
extern int pick_level_from_range(int level, int min, int max);
extern bool wake_and_stand(char_data *ch);

// resource functions from utils.c
void add_to_resource_list(struct resource_data **list, int type, any_vnum vnum, int amount, int misc);
void apply_resource(char_data *ch, struct resource_data *res, struct resource_data **list, obj_data *use_obj, int msg_type, vehicle_data *crafting_veh, struct resource_data **build_used_list);
extern char *component_string(int type, bitvector_t flags);
void extract_resources(char_data *ch, struct resource_data *list, bool ground, struct resource_data **build_used_list);
extern struct resource_data *get_next_resource(char_data *ch, struct resource_data *list, bool ground, bool left2right, obj_data **found_obj);
extern char *get_resource_name(struct resource_data *res);
void give_resources(char_data *ch, struct resource_data *list, bool split);
void halve_resource_list(struct resource_data **list, bool remove_nonrefundables);
extern bool has_resources(char_data *ch, struct resource_data *list, bool ground, bool send_msgs);
void show_resource_list(struct resource_data *list, char *save_buffer);

// string functions from utils.c
extern bitvector_t asciiflag_conv(char *flag);
extern char *bitv_to_alpha(bitvector_t flags);
extern char *delete_doubledollar(char *string);
extern const char *double_percents(const char *string);
extern bool has_keyword(char *string, const char *list[], bool exact);
extern bool isname(const char *str, const char *namelist);
extern char *level_range_string(int min, int max, int current);
extern bool multi_isname(const char *arg, const char *namelist);
extern char *CAP(char *txt);
extern char *fname(const char *namelist);
extern char *reverse_strstr(char *haystack, char *needle);
extern char *str_dup(const char *source);
extern char *str_replace(char *search, char *replace, char *subject);
extern char *str_str(char *cs, char *ct);
extern char *strip_color(char *input);
void strip_crlf(char *buffer);
extern char *strtolower(char *str);
extern char *strtoupper(char *str);
extern int color_code_length(const char *str);
#define color_strlen(str)  (strlen(str) - color_code_length(str))
extern int count_icon_codes(char *string);
extern bool strchrstr(const char *haystack, const char *needles);
extern int str_cmp(const char *arg1, const char *arg2);
extern int strn_cmp(const char *arg1, const char *arg2, int n);
void prettier_sprintbit(bitvector_t bitvector, const char *names[], char *result);
void prune_crlf(char *txt);
extern const char *skip_filler(char *string);
void sprintbit(bitvector_t vektor, const char *names[], char *result, bool space);
void sprinttype(int type, const char *names[], char *result);
extern char *trim(char *string);

// world functions in utils.c
extern bool check_sunny(room_data *room);
extern bool find_flagged_sect_within_distance_from_char(char_data *ch, bitvector_t with_flags, bitvector_t without_flags, int distance);
extern bool find_flagged_sect_within_distance_from_room(room_data *room, bitvector_t with_flags, bitvector_t without_flags, int distance);
extern bool find_sect_within_distance_from_char(char_data *ch, sector_vnum sect, int distance);
extern bool find_sect_within_distance_from_room(room_data *room, sector_vnum sect, int distance);
extern int compute_map_distance(int x1, int y1, int x2, int y2);
#define compute_distance(from, to)  compute_map_distance(X_COORD(from), Y_COORD(from), X_COORD(to), Y_COORD(to))
extern int count_adjacent_sectors(room_data *room, sector_vnum sect, bool count_original_sect);
extern int distance_to_nearest_player(room_data *room);
extern bool get_coord_shift(int start_x, int start_y, int x_shift, int y_shift, int *new_x, int *new_y);
extern int get_direction_to(room_data *from, room_data *to);
extern room_data *get_map_location_for(room_data *room);
extern room_data *real_shift(room_data *origin, int x_shift, int y_shift);
extern room_data *straight_line(room_data *origin, room_data *destination, int iter);
extern sector_data *find_first_matching_sector(bitvector_t with_flags, bitvector_t without_flags);

// misc functions from utils.c
extern unsigned long long microtime(void);
extern bool room_has_function_and_city_ok(room_data *room, bitvector_t fnc_flag);

// utils from act.action.c
void cancel_action(char_data *ch);
void start_action(char_data *ch, int type, int timer);

// utils from act.empire.c
extern bool check_in_city_requirement(room_data *room, bool check_wait);
extern bool is_in_city_for_empire(room_data *loc, empire_data *emp, bool check_wait, bool *too_soon);

// utils from act.informative.c
extern char *get_obj_desc(obj_data *obj, char_data *ch, int mode);

// utils from act.item.c
extern int obj_carry_size(obj_data *obj);

// utils from faction.c
extern const char *get_faction_name_by_vnum(any_vnum vnum);
extern const char *get_reputation_name(int type);

// utils from limits.c
extern bool can_teleport_to(char_data *ch, room_data *loc, bool check_owner);
void gain_condition(char_data *ch, int condition, int value);

// utils from mapview.c
extern bool adjacent_room_is_light(room_data *room);
void look_at_room_by_loc(char_data *ch, room_data *room, bitvector_t options);
#define look_at_room(ch)  look_at_room_by_loc((ch), IN_ROOM(ch), NOBITS)

// utils from quest.c
extern char *get_quest_name_by_proto(any_vnum vnum);
void qt_change_ability(char_data *ch, any_vnum abil);
void qt_change_reputation(char_data *ch, any_vnum faction);
void qt_change_skill_level(char_data *ch, any_vnum skl);
void qt_drop_obj(char_data *ch, obj_data *obj);
void qt_empire_players(empire_data *emp, void (*func)(char_data *ch, any_vnum vnum), any_vnum vnum);
void qt_gain_building(char_data *ch, any_vnum vnum);
void qt_gain_vehicle(char_data *ch, any_vnum vnum);
void qt_get_obj(char_data *ch, obj_data *obj);
void qt_kill_mob(char_data *ch, char_data *mob);
void qt_lose_building(char_data *ch, any_vnum vnum);
void qt_lose_quest(char_data *ch, any_vnum vnum);
void qt_lose_vehicle(char_data *ch, any_vnum vnum);
void qt_quest_completed(char_data *ch, any_vnum vnum);
void qt_remove_obj(char_data *ch, obj_data *obj);
void qt_start_quest(char_data *ch, any_vnum vnum);
void qt_triggered_task(char_data *ch, any_vnum vnum);
void qt_untrigger_task(char_data *ch, any_vnum vnum);
void qt_visit_room(char_data *ch, room_data *room);
void qt_wear_obj(char_data *ch, obj_data *obj);

// utils from vehicles.c
extern char *get_vehicle_name_by_proto(obj_vnum vnum);


 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS UTILS /////////////////////////////////////////////////////

// for the easy-update system
#define PLAYER_UPDATE_FUNC(name)  void (name)(char_data *ch, bool is_file)

// Group related defines
#define GROUP(ch)  (ch->group)
#define GROUP_LEADER(group)  (group->leader)
#define GROUP_FLAGS(group)  (group->group_flags)

// handy
#define SELF(sub, obj)  ((sub) == (obj))

// this is sort of a config
#define HAPPY_COLOR(cur, base)  ((cur) < (base) ? "&r" : ((cur) > (base) ? "&g" : "&0"))


// supplementary math
#define ABSOLUTE(x)  ((x < 0) ? ((x) * -1) : (x))


/* undefine MAX and MIN so that our macros are used instead */
#ifdef MAX
#undef MAX
#endif
#define MAX(a, b)  ((a) > (b) ? (a) : (b))

#ifdef MIN
#undef MIN
#endif
#define MIN(a, b)  ((a) < (b) ? (a) : (b))


/* 
 * npc safeguard:
 * Accessing player specific data structures on a mobile is a very bad thing
 * to do.  Consider that changing these variables for a single mob will change
 * it for every other single mob in the game.  If we didn't specifically check
 * for it, 'wimpy' would be an extremely bad thing for a mob to do, as an
 * example.  If you really couldn't care less, change this to a '#if 0'.
 */
#if 1
	extern struct player_special_data dummy_mob;
	#define CHECK_PLAYER_SPECIAL(ch, var)  (*(((ch)->player_specials == &dummy_mob) ? (log("SYSERR: Mob using '"#var"' at %s:%d.", __FILE__, __LINE__), &(var)) : &(var)))
#else
	#define CHECK_PLAYER_SPECIAL(ch, var)  (var)
#endif


// bounds-safe adding
#define SAFE_ADD(field, amount, min, max, warn)  do { \
		long long _safe_add_old_val = (field);	\
		field += (amount);	\
		if ((amount) > 0 && (field) < _safe_add_old_val) {	\
			(field) = (max);	\
			if (warn) {	\
				log("SYSERR: SAFE_ADD out of bounds at %s:%d.", __FILE__, __LINE__);	\
			}	\
		}	\
		else if ((amount) < 0 && (field) > _safe_add_old_val) {	\
			(field) = (min);	\
			if (warn) {	\
				log("SYSERR: SAFE_ADD out of bounds at %s:%d.", __FILE__, __LINE__);	\
			}	\
		}	\
	} while (0)


// shortcudt for messaging
#define send_config_msg(ch, conf)  msg_to_char((ch), "%s\r\n", config_get_string(conf))


 //////////////////////////////////////////////////////////////////////////////
//// CONSTS FOR UTILS.C //////////////////////////////////////////////////////

// get_filename()
#define PLR_FILE  0
#define DELAYED_FILE  1


// APPLY_RES_x: messaging for the apply_resource() function
#define APPLY_RES_SILENT  0	// send no messages
#define APPLY_RES_BUILD  1	// send build/maintain message
#define APPLY_RES_CRAFT  2	// send craft message
#define APPLY_RES_REPAIR  3	// vehicle repairing
#define NUM_APPLY_RES_TYPES  4
