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
*   Event Utils
*   Faction Utils
*   Fight Utils
*   Generic Utils
*   Global Utils
*   Map Utils
*   Memory Utils
*   Mobile Utils
*   Morph Utils
*   Object Utils
*   Objval Utils
*   Player Utils
*   Progress Utils
*   Quest Utils
*   Room Utils
*   Room Template Utils
*   Sector Utils
*   Shop Utils
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

#define ABIL_AFFECTS(abil)  ((abil)->affects)
#define ABIL_AFFECT_VNUM(abil)  ((abil)->affect_vnum)
#define ABIL_APPLIES(abil)  ((abil)->applies)
#define ABIL_ASSIGNED_SKILL(abil)  ((abil)->assigned_skill)
#define ABIL_ATTACK_TYPE(abil)  ((abil)->attack_type)
#define ABIL_COMMAND(abil)  ((abil)->command)
#define ABIL_COOLDOWN(abil)  ((abil)->cooldown)
#define ABIL_COOLDOWN_SECS(abil)  ((abil)->cooldown_secs)
#define ABIL_COST(abil)  ((abil)->cost)
#define ABIL_COST_PER_SCALE_POINT(abil)  ((abil)->cost_per_scale_point)
#define ABIL_COST_TYPE(abil)  ((abil)->cost_type)
#define ABIL_CUSTOM_MSGS(abil)  ((abil)->custom_msgs)
#define ABIL_DAMAGE_TYPE(abil)  ((abil)->damage_type)
#define ABIL_DATA(abil)  ((abil)->data)
#define ABIL_DIFFICULTY(abil)  ((abil)->difficulty)
#define ABIL_FLAGS(abil)  ((abil)->flags)
#define ABIL_GAIN_HOOKS(abil)  ((abil)->gain_hooks)
#define ABIL_IMMUNITIES(abil)  ((abil)->immunities)
#define ABIL_LINKED_TRAIT(abil)  ((abil)->linked_trait)
#define ABIL_LONG_DURATION(abil)  ((abil)->long_duration)
#define ABIL_MASTERY_ABIL(abil)  ((abil)->mastery_abil)
#define ABIL_MAX_STACKS(abil)  ((abil)->max_stacks)
#define ABIL_MIN_POS(abil)  ((abil)->min_position)
#define ABIL_NAME(abil)  ((abil)->name)
#define ABIL_REQUIRES_TOOL(abil)  ((abil)->requires_tool)
#define ABIL_SCALE(abil)  ((abil)->scale)
#define ABIL_SHORT_DURATION(abil)  ((abil)->short_duration)
#define ABIL_SKILL_LEVEL(abil)  ((abil)->skill_level)
#define ABIL_TARGETS(abil)  ((abil)->targets)
#define ABIL_TYPES(abil)  ((abil)->types)
#define ABIL_TYPE_LIST(abil)  ((abil)->type_list)
#define ABIL_VNUM(abil)  ((abil)->vnum)
#define ABIL_WAIT_TYPE(abil)  ((abil)->wait_type)


// type-specific data
#define ABIL_AFFECT_VNUM(abil)  ((abil)->affect_vnum)

// utils
#define ABILITY_FLAGGED(abil, flag)  IS_SET(ABIL_FLAGS(abil), (flag))
#define ABIL_IS_CLASS(abil)  ((abil)->is_class)
#define ABIL_IS_PURCHASE(abil)  (ABIL_ASSIGNED_SKILL(abil) != NULL)
#define ABIL_IS_SYNERGY(abil)  ((abil)->is_synergy)
#define SAFE_ABIL_COMMAND(abil)  (ABIL_COMMAND(abil) ? ABIL_COMMAND(abil) : "the ability")


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
#define GET_ADV_TEMPERATURE_TYPE(adv)  ((adv)->temperature_type)

// utils
#define ADVENTURE_FLAGGED(adv, flg)  (IS_SET(GET_ADV_FLAGS(adv), (flg)) ? TRUE : FALSE)
#define LINK_FLAGGED(lnkptr, flg)  (IS_SET((lnkptr)->flags, (flg)) ? TRUE : FALSE)

// instance utils
#define INSTANCE_FLAGGED(i, flg)  (IS_SET(INST_FLAGS(i), (flg)))
#define INST_ADVENTURE(inst)  ((inst)->adventure)
#define INST_CREATED(inst)  ((inst)->created)
#define INST_DIR(inst)  ((inst)->dir)
#define INST_FAKE_LOC(inst)  ((inst)->fake_loc)
#define INST_FLAGS(inst)  ((inst)->flags)
#define INST_ID(inst)  ((inst)->id)
#define INST_LAST_RESET(inst)  ((inst)->last_reset)
#define INST_LEVEL(inst)  ((inst)->level)
#define INST_LOCATION(inst)  ((inst)->location)
#define INST_MOB_COUNTS(inst)  ((inst)->mob_counts)
#define INST_ROOM(inst, num)  ((inst)->room[(num)])
#define INST_ROTATION(inst)  ((inst)->rotation)
#define INST_RULE(inst)  ((inst)->rule)
#define INST_SIZE(inst)  ((inst)->size)
#define INST_START(inst)  ((inst)->start)


 //////////////////////////////////////////////////////////////////////////////
//// ARCHETYPE UTILS /////////////////////////////////////////////////////////

#define GET_ARCH_ATTRIBUTE(arch, pos)  ((arch)->attributes[(pos)])
#define GET_ARCH_DESC(arch)  ((arch)->description)
#define GET_ARCH_FEMALE_RANK(arch)  ((arch)->female_rank)
#define GET_ARCH_FLAGS(arch)  ((arch)->flags)
#define GET_ARCH_GEAR(arch)  ((arch)->gear)
#define GET_ARCH_LANGUAGE(arch)  ((arch)->language)
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
#define IS_SET_STRICT(flag,bit)  (((flag) & (bit)) == (bit))
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
#define GET_BLD_HEIGHT(bld)  ((bld)->height)
#define GET_BLD_EX_DESCS(bld)  ((bld)->ex_description)
#define GET_BLD_EXTRA_ROOMS(bld)  ((bld)->extra_rooms)
#define GET_BLD_DESIGNATE_FLAGS(bld)  ((bld)->designate_flags)
#define GET_BLD_BASE_AFFECTS(bld)  ((bld)->base_affects)
#define GET_BLD_CITIZENS(bld)  ((bld)->citizens)
#define GET_BLD_MILITARY(bld)  ((bld)->military)
#define GET_BLD_ARTISAN(bld)  ((bld)->artisan_vnum)
#define GET_BLD_RELATIONS(bld)  ((bld)->relations)
#define GET_BLD_SCRIPTS(bld)  ((bld)->proto_script)
#define GET_BLD_SPAWNS(bld)  ((bld)->spawns)
#define GET_BLD_INTERACTIONS(bld)  ((bld)->interactions)
#define GET_BLD_QUEST_LOOKUPS(bld)  ((bld)->quest_lookups)
#define GET_BLD_SHOP_LOOKUPS(bld)  ((bld)->shop_lookups)
#define GET_BLD_TEMPERATURE_TYPE(bld)  ((bld)->temperature_type)
#define GET_BLD_YEARLY_MAINTENANCE(bld)  ((bld)->yearly_maintenance)


 //////////////////////////////////////////////////////////////////////////////
//// CAN SEE UTILS ///////////////////////////////////////////////////////////

// this all builds up to CAN_SEE
#define LIGHT_OK(sub)  (!AFF_FLAGGED(sub, AFF_BLIND) && can_see_in_dark_room((sub), (IN_ROOM(sub)), TRUE))
#define INVIS_OK(sub, obj)  ((!AFF_FLAGGED(obj, AFF_INVISIBLE)) && (!AFF_FLAGGED((obj), AFF_HIDE) || AFF_FLAGGED((sub), AFF_SENSE_HIDE)))


#define MORT_CAN_SEE_NO_DARK(sub, obj)  (INVIS_OK(sub, obj))
#define MORT_CAN_SEE_LIGHT(sub, obj)  (LIGHT_OK(sub) || (!AFF_FLAGGED(sub, AFF_BLIND) && IN_ROOM(sub) == IN_ROOM(obj) && (has_player_tech((sub), PTECH_SEE_CHARS_IN_DARK) || (IS_OUTDOORS(sub) && has_player_tech((sub), PTECH_SEE_IN_DARK_OUTDOORS))) && (!MAGIC_DARKNESS(IN_ROOM(sub)) || CAN_SEE_IN_MAGIC_DARKNESS(sub))))
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

#define WIZHIDE_OK(sub, obj)  (!IS_IMMORTAL(obj) || !PRF_FLAGGED((obj), PRF_WIZHIDE) || (!IS_NPC(sub) && GET_ACCESS_LEVEL(sub) >= MIN(LVL_START_IMM, GET_ACCESS_LEVEL(obj)) && PRF_FLAGGED((sub), PRF_HOLYLIGHT)))
#define INCOGNITO_OK(sub, obj)  (!IS_IMMORTAL(obj) || IS_IMMORTAL(sub) || !PRF_FLAGGED((obj), PRF_INCOGNITO))


 //////////////////////////////////////////////////////////////////////////////
//// CAN SEE OBJ UTILS ///////////////////////////////////////////////////////

#define CAN_SEE_OBJ_CARRIER(sub, obj)  ((!obj->carried_by || CAN_SEE(sub, obj->carried_by)) && (!obj->worn_by || CAN_SEE(sub, obj->worn_by)))
#define MORT_CAN_SEE_OBJ(sub, obj)  ((LIGHT_OK(sub) || obj->worn_by == sub || obj->carried_by == sub || (IN_ROOM(sub) == IN_ROOM(obj) && !AFF_FLAGGED((sub), AFF_BLIND) && (has_player_tech((sub), PTECH_SEE_OBJS_IN_DARK) || (IS_OUTDOORS(sub) && has_player_tech((sub), PTECH_SEE_IN_DARK_OUTDOORS))) && (!MAGIC_DARKNESS(IN_ROOM(sub)) || CAN_SEE_IN_MAGIC_DARKNESS(sub)))) && CAN_SEE_OBJ_CARRIER(sub, obj))
#define CAN_SEE_OBJ(sub, obj)  (MORT_CAN_SEE_OBJ(sub, obj) || (!IS_NPC(sub) && PRF_FLAGGED((sub), PRF_HOLYLIGHT)))


 //////////////////////////////////////////////////////////////////////////////
//// CAN SEE VEHICLE UTILS ///////////////////////////////////////////////////////

#define MORT_CAN_SEE_VEHICLE(sub, veh)  (VEH_FLAGGED((veh), VEH_VISIBLE_IN_DARK) || LIGHT_OK(sub) || VEH_DRIVER(veh) == (sub) || VEH_LED_BY(veh) == (sub) || VEH_SITTING_ON(veh) == (sub) || GET_ROOM_VEHICLE(IN_ROOM(sub)) == (veh))
#define CAN_SEE_VEHICLE(sub, veh)  (MORT_CAN_SEE_VEHICLE(sub, veh) || (!IS_NPC(sub) && PRF_FLAGGED((sub), PRF_HOLYLIGHT)))


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER UTILS /////////////////////////////////////////////////////////

// ch: char_data
#define GET_AGE(ch)  (((!IS_NPC(ch) && IS_VAMPIRE(ch)) ? GET_APPARENT_AGE(ch) : age(ch)->year) + GET_AGE_MODIFIER(ch))
#define GET_EQ(ch, i)  ((ch)->equipment[i])
#define GET_REAL_AGE(ch)  (age(ch)->year)
#define IN_ROOM(ch)  ((ch)->in_room)
#define GET_LEADER(ch)  ((ch)->leader)
#define GET_LIGHTS(ch)  ((ch)->lights)
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
#define GET_LOOK_DESC(ch)  ((ch)->player.look_descr)
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
#define GET_BLOOD_UPKEEP(ch)  (GET_EXTRA_ATT(ch, ATT_BLOOD_UPKEEP) + (has_skill_flagged(ch, SKILLF_VAMPIRE) <= 15 ? 1 : 0))
int GET_MAX_BLOOD(char_data *ch);	// this one is different than the other max pools, and max_pools[BLOOD] is not used.
#define GET_HEALTH_DEFICIT(ch)  GET_DEFICIT((ch), HEALTH)
#define GET_MOVE_DEFICIT(ch)  GET_DEFICIT((ch), MOVE)
#define GET_MANA_DEFICIT(ch)  GET_DEFICIT((ch), MANA)
#define GET_BONUS_INVENTORY(ch)  GET_EXTRA_ATT(ch, ATT_BONUS_INVENTORY)
#define GET_COOLING(ch)  GET_EXTRA_ATT(ch, ATT_COOLING)
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
#define GET_AGE_MODIFIER(ch)  GET_EXTRA_ATT(ch, ATT_AGE_MODIFIER)
#define GET_WARMTH(ch)  GET_EXTRA_ATT(ch, ATT_WARMTH)

// ch->char_specials: char_special_data
#define FIGHTING(ch)  ((ch)->char_specials.fighting.victim)
#define FIGHT_MODE(ch)  ((ch)->char_specials.fighting.mode)
#define FIGHT_WAIT(ch)  ((ch)->char_specials.fighting.wait)
#define GET_COMPANION(ch)  ((ch)->char_specials.companion)
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
#define GET_ROPE_VNUM(ch)  ((ch)->char_specials.rope_vnum)
#define GET_SITTING_ON(ch)  ((ch)->char_specials.sitting_on)
#define GET_POS(ch)  ((ch)->char_specials.position)
#define SET_SIZE(ch)  ((ch)->char_specials.size)	// notice "SET_SIZE" -- the simple version of the macro
#define GET_SIZE(ch)  (IS_MORPHED(ch) ? MORPH_SIZE(GET_MORPH(ch)) : SET_SIZE(ch))
#define GET_STORED_EVENTS(ch)  ((ch)->char_specials.stored_events)
#define HUNTING(ch)  ((ch)->char_specials.hunting)
#define IS_CARRYING_N(ch)  ((ch)->char_specials.carry_items)


// definitions
#define AWAKE(ch)  (GET_POS(ch) > POS_SLEEPING || GET_POS(ch) == POS_DEAD)
int CAN_CARRY_N(char_data *ch);	// formerly a macro
#define CAN_CARRY_OBJ(ch,obj)  (FREE_TO_CARRY(obj) || (IS_CARRYING_N(ch) + obj_carry_size(obj)) <= CAN_CARRY_N(ch))
#define CAN_GET_OBJ(ch, obj)  (CAN_WEAR((obj), ITEM_WEAR_TAKE) && CAN_CARRY_OBJ((ch),(obj)) && CAN_SEE_OBJ((ch),(obj)))
#define CAN_RECOGNIZE(ch, vict)  (PRF_FLAGGED(ch, PRF_HOLYLIGHT) || (!AFF_FLAGGED(vict, AFF_NO_SEE_IN_ROOM) && ((GET_LOYALTY(ch) && GET_LOYALTY(ch) == GET_LOYALTY(vict)) || (GROUP(ch) && in_same_group(ch, vict)) || (!CHAR_MORPH_FLAGGED((vict), MORPHF_ANIMAL) && !IS_DISGUISED(vict)))))
#define CAN_RIDE_FLYING_MOUNT(ch)  (has_player_tech((ch), PTECH_RIDING_FLYING))
#define CAN_RIDE_MOUNT(ch, mob)  (MOB_FLAGGED((mob), MOB_MOUNTABLE) && (!AFF_FLAGGED((mob), AFF_FLY) || CAN_RIDE_FLYING_MOUNT(ch)) && (!AFF_FLAGGED((mob), AFF_WATERWALK) || CAN_RIDE_WATERWALK_MOUNT(ch)))
#define CAN_RIDE_WATERWALK_MOUNT(ch)  (has_player_tech((ch), PTECH_RIDING_UPGRADE))
#define CAN_SEE_IN_MAGIC_DARKNESS(ch)  (IS_NPC(ch) ? (get_approximate_level(ch) > 100) : (PRF_FLAGGED((ch), PRF_HOLYLIGHT) || has_ability((ch), ABIL_DARKNESS)))
#define CAN_SPEND_BLOOD(ch)  (!AFF_FLAGGED(ch, AFF_CANT_SPEND_BLOOD))
#define CAST_BY_ID(ch)  (IS_NPC(ch) ? (-1 * GET_MOB_VNUM(ch)) : GET_IDNUM(ch))
#define EFFECTIVELY_FLYING(ch)  (IS_RIDING(ch) ? MOUNT_FLAGGED(ch, MOUNT_FLYING) : AFF_FLAGGED(ch, AFF_FLY))
#define EFFECTIVELY_SWIMMING(ch)  (EFFECTIVELY_FLYING(ch) || HAS_WATERWALK(ch) || (IS_RIDING(ch) && (MOUNT_FLAGGED((ch), MOUNT_AQUATIC) || has_player_tech((ch), PTECH_RIDING_UPGRADE))) || (IS_NPC(ch) ? MOB_FLAGGED((ch), MOB_AQUATIC) : has_player_tech((ch), PTECH_SWIMMING)))
#define FREE_TO_CARRY(obj)  (IS_COINS(obj) || GET_OBJ_REQUIRES_QUEST(obj) != NOTHING)
#define HAS_INFRA(ch)  AFF_FLAGGED(ch, AFF_INFRAVISION)
#define HAS_WATERWALK(ch)  (AFF_FLAGGED((ch), AFF_WATERWALK) || MOUNT_FLAGGED((ch), MOUNT_WATERWALK))
#define IS_HUMAN(ch)  (!IS_VAMPIRE(ch))
#define IS_MAGE(ch)  (IS_NPC(ch) ? MOB_FLAGGED((ch), MOB_CASTER) : (has_skill_flagged((ch), SKILLF_CASTER) > 0))
#define IS_OUTDOORS(ch)  IS_OUTDOOR_TILE(IN_ROOM(ch))
#define IS_SWIMMING(ch)  (WATER_SECT(IN_ROOM(ch)) && !GET_SITTING_ON(ch) && !IS_RIDING(ch) && !EFFECTIVELY_FLYING(ch) && !HAS_WATERWALK(ch))
#define IS_VAMPIRE(ch)  (IS_NPC(ch) ? MOB_FLAGGED((ch), MOB_VAMPIRE) : (has_skill_flagged((ch), SKILLF_VAMPIRE) > 0))
#define NOT_MELEE_RANGE(ch, vict)  ((FIGHTING(ch) && FIGHT_MODE(ch) != FMODE_MELEE) || (FIGHTING(vict) && FIGHT_MODE(vict) != FMODE_MELEE))
#define WOULD_EXECUTE(ch, vict)  (MOB_FLAGGED((vict), MOB_HARD | MOB_GROUP) || (IS_NPC(ch) ? ((GET_LEADER(ch) && !IS_NPC(GET_LEADER(ch))) ? PRF_FLAGGED(GET_LEADER(ch), PRF_AUTOKILL) : (!MOB_FLAGGED((ch), MOB_ANIMAL) || MOB_FLAGGED((ch), MOB_AGGRESSIVE | MOB_HARD | MOB_GROUP))) : PRF_FLAGGED((ch), PRF_AUTOKILL)))

// helpers
#define AFF_FLAGGED(ch, flag)  (IS_SET(AFF_FLAGS(ch), (flag)))
#define EXTRACTED(ch)  (PLR_FLAGGED((ch), PLR_EXTRACTED) || MOB_FLAGGED((ch), MOB_EXTRACTED))
#define GET_NAME(ch)  (IS_NPC(ch) ? GET_SHORT_DESC(ch) : GET_PC_NAME(ch))
#define GET_REAL_LEVEL(ch)  (ch->desc && ch->desc->original ? GET_ACCESS_LEVEL(ch->desc->original) : GET_ACCESS_LEVEL(ch))
#define GET_SEX(ch)  (IS_DISGUISED(ch) ? GET_DISGUISED_SEX(ch) : GET_REAL_SEX(ch))
#define GET_TRACK_ID(ch)  (!IS_NPC(ch) ? GET_IDNUM(ch) : (GET_MOB_VNUM(ch) >= 0 ? GET_MOB_VNUM(ch) : 0))
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
#define GET_CRAFT_REQUIRES_FUNCTION(craft)  ((craft)->requires_function)
#define GET_CRAFT_REQUIRES_TOOL(craft)  ((craft)->requires_tool)
#define GET_CRAFT_RESOURCES(craft)  ((craft)->resources)
#define GET_CRAFT_TIME(craft)  ((craft)->time)
#define GET_CRAFT_TYPE(craft)  ((craft)->type)
#define GET_CRAFT_VNUM(craft)  ((craft)->vnum)

#define CRAFT_FLAGGED(cr, flg)  (IS_SET(GET_CRAFT_FLAGS(cr), (flg)) ? TRUE : FALSE)
#define CRAFT_IS_BUILDING(craft)  ((GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD || CRAFT_FLAGGED(craft, CRAFT_BUILDING)) && !CRAFT_IS_VEHICLE(craft))
#define CRAFT_IS_VEHICLE(craft)  CRAFT_FLAGGED((craft), CRAFT_VEHICLE)	// vehicle overrides building


 //////////////////////////////////////////////////////////////////////////////
//// CROP UTILS //////////////////////////////////////////////////////////////

#define GET_CROP_CLIMATE(crop)  ((crop)->climate)
#define GET_CROP_EX_DESCS(crop)  ((crop)->ex_description)
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
#define MATCH_CROP_SECTOR_CLIMATE(crop, climate)  (!GET_CROP_CLIMATE(crop) || (GET_CROP_CLIMATE(crop) & (climate)) == GET_CROP_CLIMATE(crop) || (CROP_FLAGGED((crop), CROPF_ANY_LISTED_CLIMATE) && (GET_CROP_CLIMATE(crop) & (climate))))


 //////////////////////////////////////////////////////////////////////////////
//// DESCRIPTOR UTILS ////////////////////////////////////////////////////////

// basic
#define STATE(d)  ((d)->connected)
#define SUBMENU(d)  ((d)->submenu)


// OLC_x: olc getters
#define GET_OLC_TYPE(desc)  ((desc)->olc_type)
#define GET_OLC_VNUM(desc)  ((desc)->olc_vnum)
#define GET_OLC_STORAGE(desc)  ((desc)->olc_storage)
#define GET_OLC_SHOW_TREE(desc)  ((desc)->olc_show_tree)
#define GET_OLC_SHOW_SYNERGIES(desc)  ((desc)->olc_show_synergies)

#define GET_OLC_ABILITY(desc)  ((desc)->olc_ability)
#define GET_OLC_ADVENTURE(desc)  ((desc)->olc_adventure)
#define GET_OLC_ARCHETYPE(desc)  ((desc)->olc_archetype)
#define GET_OLC_AUGMENT(desc)  ((desc)->olc_augment)
#define GET_OLC_BOOK(desc)  ((desc)->olc_book)
#define GET_OLC_BUILDING(desc)  ((desc)->olc_building)
#define GET_OLC_CLASS(desc)  ((desc)->olc_class)
#define GET_OLC_CRAFT(desc)  ((desc)->olc_craft)
#define GET_OLC_CROP(desc)  ((desc)->olc_crop)
#define GET_OLC_EVENT(desc)  ((desc)->olc_event)
#define GET_OLC_FACTION(desc)  ((desc)->olc_faction)
#define GET_OLC_GENERIC(desc)  ((desc)->olc_generic)
#define GET_OLC_GLOBAL(desc)  ((desc)->olc_global)
#define GET_OLC_MOBILE(desc)  ((desc)->olc_mobile)
#define GET_OLC_MORPH(desc)  ((desc)->olc_morph)
#define GET_OLC_OBJECT(desc)  ((desc)->olc_object)
#define GET_OLC_PROGRESS(desc)  ((desc)->olc_progress)
#define GET_OLC_QUEST(desc)  ((desc)->olc_quest)
#define GET_OLC_ROOM_TEMPLATE(desc)  ((desc)->olc_room_template)
#define GET_OLC_SECTOR(desc)  ((desc)->olc_sector)
#define GET_OLC_SHOP(desc)  ((desc)->olc_shop)
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
#define EMPIRE_ADMIN_FLAGS(emp)  ((emp)->admin_flags)
#define EMPIRE_ATTRIBUTE(emp, att)  ((emp)->attributes[(att)])
#define EMPIRE_BANNER(emp)  ((emp)->banner)
#define EMPIRE_BANNER_HAS_UNDERLINE(emp)  ((emp)->banner_has_underline)
#define EMPIRE_BASE_TECH(emp, num)  ((emp)->base_tech[(num)])
#define EMPIRE_CITY_OVERAGE_WARNING_TIME(emp)  ((emp)->city_overage_warning_time)
#define EMPIRE_DELAYED_REFRESH(emp)  ((emp)->delayed_refresh)
#define EMPIRE_DROPPED_ITEMS(emp)  ((emp)->dropped_items)
#define EMPIRE_NUM_RANKS(emp)  ((emp)->num_ranks)
#define EMPIRE_RANK(emp, num)  ((emp)->rank[(num)])
#define EMPIRE_FRONTIER_TRAITS(emp)  ((emp)->frontier_traits)
#define EMPIRE_HOMELESS_CITIZENS(emp)  ((emp)->homeless)
#define EMPIRE_COINS(emp)  ((emp)->coins)
#define EMPIRE_COMPLETED_GOALS(emp)  ((emp)->completed_goals)
#define EMPIRE_PRIV(emp, num)  ((emp)->priv[(num)])
#define EMPIRE_DELAYS(emp)  ((emp)->delays)
#define EMPIRE_DESCRIPTION(emp)  ((emp)->description)
#define EMPIRE_DIPLOMACY(emp)  ((emp)->diplomacy)
#define EMPIRE_GOALS(emp)  ((emp)->goals)
#define EMPIRE_TRADE(emp)  ((emp)->trade)
#define EMPIRE_LOGS(emp)  ((emp)->logs)
#define EMPIRE_TERRITORY_LIST(emp)  ((emp)->territory_list)
#define EMPIRE_CITY_LIST(emp)  ((emp)->city_list)
#define EMPIRE_TERRITORY(emp, type)  ((emp)->territory[(type)])
#define EMPIRE_WEALTH(emp)  ((emp)->wealth)
#define EMPIRE_POPULATION(emp)  ((emp)->population)
#define EMPIRE_LANGUAGES(emp)  ((emp)->languages)
#define EMPIRE_LEARNED_CRAFTS(emp)  ((emp)->learned_crafts)
#define EMPIRE_MAPOUT_TOKEN(emp)  ((emp)->mapout_token)
#define EMPIRE_MEMBER_ACCOUNTS(emp)  ((emp)->member_accounts)
#define EMPIRE_MILITARY(emp)  ((emp)->military)
#define EMPIRE_MAX_LEVEL(emp)  ((emp)->max_level)
#define EMPIRE_MIN_LEVEL(emp)  ((emp)->min_level)
#define EMPIRE_MOTD(emp)  ((emp)->motd)
#define EMPIRE_NEEDS_SAVE(emp)  ((emp)->needs_save)
#define EMPIRE_NEEDS_LOGS_SAVE(emp)  ((emp)->needs_logs_save)
#define EMPIRE_NEEDS_STORAGE_SAVE(emp)  ((emp)->needs_storage_save)
#define EMPIRE_NEXT_TIMEOUT(emp)  ((emp)->next_timeout)
#define EMPIRE_PLAYTIME_TRACKER(emp)  ((emp)->playtime_tracker)
#define EMPIRE_PRODUCTION_LIMITS(emp)  ((emp)->production_limits)
#define EMPIRE_PRODUCTION_LOGS(emp)  ((emp)->production_logs)
#define EMPIRE_PRODUCTION_TOTALS(emp)  ((emp)->production_totals)
#define EMPIRE_PROGRESS_POINTS(emp, type)  ((emp)->progress_points[(type)])
#define EMPIRE_PROGRESS_POOL(emp)  EMPIRE_ATTRIBUTE((emp), EATT_PROGRESS_POOL)
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
#define EMPIRE_THEFT_LOGS(emp)  ((emp)->theft_logs)
#define EMPIRE_UNIQUE_STORAGE(emp)  ((emp)->unique_store)
#define EMPIRE_WORKFORCE_TRACKER(emp)  ((emp)->ewt_tracker)
#define EMPIRE_ISLANDS(emp)  ((emp)->islands)
#define EMPIRE_TOP_SHIPPING_ID(emp)  ((emp)->top_shipping_id)
#define EMPIRE_OFFENSES(emp)  ((emp)->offenses)
#define EMPIRE_WORKFORCE_LOG(emp)  ((emp)->wf_log)
#define EMPIRE_WORKFORCE_WHERE_LOG(emp)  ((emp)->wf_where_log)

// helpers
#define EMPIRE_ADMIN_FLAGGED(emp, flag)  IS_SET(EMPIRE_ADMIN_FLAGS(emp), (flag))
#define EMPIRE_HAS_TECH(emp, num)  (EMPIRE_TECH((emp), (num)) > 0)
#define EMPIRE_IS_TIMED_OUT(emp)  (EMPIRE_LAST_LOGON(emp) + (config_get_int("whole_empire_timeout") * SECS_PER_REAL_DAY) < time(0))
#define GET_TOTAL_WEALTH(emp)  (EMPIRE_WEALTH(emp) + (EMPIRE_COINS(emp) * COIN_VALUE))
#define EXPLICIT_BANNER_TERMINATOR(emp)  (EMPIRE_BANNER_HAS_UNDERLINE(emp) ? "\t0" : "")
#define OUTSKIRTS_CLAIMS_AVAILABLE(emp)  (land_can_claim((emp), TER_OUTSKIRTS) + land_can_claim((emp), TER_FRONTIER) - EMPIRE_TERRITORY((emp), TER_FRONTIER))
#define TRIGGER_DELAYED_REFRESH(emp, flag)  { SET_BIT(EMPIRE_DELAYED_REFRESH(emp), (flag)); check_empire_refresh = TRUE; }

// definitions
#define OFFENSE_HAS_WEIGHT(off)  (!IS_SET((off)->flags, OFF_AVENGED | OFF_WAR))
#define SAME_EMPIRE(ch, vict)  (!IS_NPC(ch) && !IS_NPC(vict) && GET_LOYALTY(ch) != NULL && GET_LOYALTY(ch) == GET_LOYALTY(vict))

// only some types of tiles are kept in the shortlist of territories -- particularly ones with associated chores
#define BELONGS_IN_TERRITORY_LIST(room)  (IS_ANY_BUILDING(room) || COMPLEX_DATA(room) || ROOM_SECT_FLAGGED(room, SECTF_CHORE))
#define COUNTS_AS_TERRITORY(room)  (HOME_ROOM(room) == (room) && !GET_ROOM_VEHICLE(room))
#define LARGE_CITY_RADIUS(room)  (ROOM_BLD_FLAGGED((room), BLD_LARGE_CITY_RADIUS) || ROOM_SECT_FLAGGED((room), SECTF_LARGE_CITY_RADIUS))

// dismantle privilege: does NOT actually check owner -- which should be checked BEFORE this
#define HAS_DISMANTLE_PRIV_FOR_VEHICLE(ch, veh)  (!VEH_OWNER(veh) || !GET_LOYALTY(ch) || GET_RANK(ch) >= EMPIRE_PRIV(GET_LOYALTY(ch), PRIV_DISMANTLE) || get_vehicle_extra_data((veh), ROOM_EXTRA_ORIGINAL_BUILDER) == GET_ACCOUNT(ch)->id)
#define HAS_DISMANTLE_PRIV_FOR_BUILDING(ch, room)  (can_use_room(ch, (room), MEMBERS_ONLY) && (has_permission(ch, PRIV_DISMANTLE, (room)) || get_room_extra_data((room), ROOM_EXTRA_ORIGINAL_BUILDER) == GET_ACCOUNT(ch)->id))

// deprecated
#define EMPIRE_CITY_TERRITORY(emp)  EMPIRE_TERRITORY(emp, TER_CITY)
#define EMPIRE_OUTSIDE_TERRITORY(emp)  EMPIRE_TERRITORY(emp, TER_OUTSKIRTS)


 //////////////////////////////////////////////////////////////////////////////
//// EVENT UTILS /////////////////////////////////////////////////////////////

#define EVT_VNUM(evt)  ((evt)->vnum)
#define EVT_COMPLETE_MSG(evt)  ((evt)->complete_msg)
#define EVT_DESCRIPTION(evt)  ((evt)->description)
#define EVT_DURATION(evt)  ((evt)->duration)
#define EVT_FLAGS(evt)  ((evt)->flags)
#define EVT_MAX_LEVEL(evt)  ((evt)->max_level)
#define EVT_MAX_POINTS(evt)  ((evt)->max_points)
#define EVT_MIN_LEVEL(evt)  ((evt)->min_level)
#define EVT_NAME(evt)  ((evt)->name)
#define EVT_NOTES(evt)  ((evt)->notes)
#define EVT_RANK_REWARDS(evt)  ((evt)->rank_rewards)
#define EVT_REPEATS_AFTER(evt)  ((evt)->repeats_after)
#define EVT_THRESHOLD_REWARDS(evt)  ((evt)->threshold_rewards)
#define EVT_TYPE(evt)  ((evt)->type)
#define EVT_VERSION(evt)  ((evt)->version)

// helpers
#define EVT_FLAGGED(evt, fl)  IS_SET(EVT_FLAGS(evt), (fl))


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
//// GENERIC UTILS ///////////////////////////////////////////////////////////

#define GEN_VNUM(gen)  ((gen)->vnum)
#define GEN_COMPUTED_RELATIONS(gen)  ((gen)->computed_relations)
#define GEN_FLAGS(gen)  ((gen)->flags)
#define GEN_NAME(gen)  ((gen)->name)
#define GEN_RELATIONS(gen)  ((gen)->relations)
#define GEN_STRING(gen, pos)  ((gen)->string[(pos)])
#define GEN_TYPE(gen)  ((gen)->type)
#define GEN_VALUE(gen, pos)  ((gen)->value[(pos)])


// helpers
#define GEN_FLAGGED(gen, flag)  IS_SET(GEN_FLAGS(gen), (flag))


// GENERIC_x: value definitions and getters

// GENERIC_LIQUID
#define GVAL_LIQUID_DRUNK  0
#define GVAL_LIQUID_FULL  1
#define GVAL_LIQUID_THIRST  2
#define GVAL_LIQUID_FLAGS  3
#define GSTR_LIQUID_NAME  0
#define GSTR_LIQUID_COLOR  1
#define GET_LIQUID_NAME(gen)  (GEN_TYPE(gen) == GENERIC_LIQUID ? GEN_STRING((gen), GSTR_LIQUID_NAME) : "")
#define GET_LIQUID_COLOR(gen)  (GEN_TYPE(gen) == GENERIC_LIQUID ? GEN_STRING((gen), GSTR_LIQUID_COLOR) : "")
#define GET_LIQUID_DRUNK(gen)  (GEN_TYPE(gen) == GENERIC_LIQUID ? GEN_VALUE((gen), GVAL_LIQUID_DRUNK) : 0)
#define GET_LIQUID_FULL(gen)  (GEN_TYPE(gen) == GENERIC_LIQUID ? GEN_VALUE((gen), GVAL_LIQUID_FULL) : 0)
#define GET_LIQUID_THIRST(gen)  (GEN_TYPE(gen) == GENERIC_LIQUID ? GEN_VALUE((gen), GVAL_LIQUID_THIRST) : 0)
#define GET_LIQUID_FLAGS(gen)  (GEN_TYPE(gen) == GENERIC_LIQUID ? GEN_VALUE((gen), GVAL_LIQUID_FLAGS) : 0)

// GENERIC_ACTION
#define GSTR_ACTION_BUILD_TO_CHAR  0
#define GSTR_ACTION_BUILD_TO_ROOM  1
#define GSTR_ACTION_CRAFT_TO_CHAR  2
#define GSTR_ACTION_CRAFT_TO_ROOM  3
#define GSTR_ACTION_REPAIR_TO_CHAR  4
#define GSTR_ACTION_REPAIR_TO_ROOM  5

// GENERIC_COOLDOWN
#define GSTR_COOLDOWN_WEAR_OFF  0
#define GET_COOLDOWN_WEAR_OFF(gen)  (GEN_TYPE(gen) == GENERIC_COOLDOWN ? GEN_STRING((gen), GSTR_COOLDOWN_WEAR_OFF) : NULL)

// GENERIC_AFFECT
#define GSTR_AFFECT_WEAR_OFF_TO_CHAR  0
#define GSTR_AFFECT_WEAR_OFF_TO_ROOM  1
#define GSTR_AFFECT_APPLY_TO_CHAR  2
#define GSTR_AFFECT_APPLY_TO_ROOM  3
#define GSTR_AFFECT_LOOK_AT_CHAR  4
#define GSTR_AFFECT_LOOK_AT_ROOM  5
#define GSTR_AFFECT_DOT_TO_CHAR  6
#define GSTR_AFFECT_DOT_TO_ROOM  7
#define GSTR_AFFECT_DEATH_TO_CHAR  8
#define GSTR_AFFECT_DEATH_TO_ROOM  9
#define GET_AFFECT_WEAR_OFF_TO_CHAR(gen)  (GEN_TYPE(gen) == GENERIC_AFFECT ? GEN_STRING((gen), GSTR_AFFECT_WEAR_OFF_TO_CHAR) : NULL)
#define GET_AFFECT_WEAR_OFF_TO_ROOM(gen)  (GEN_TYPE(gen) == GENERIC_AFFECT ? GEN_STRING((gen), GSTR_AFFECT_WEAR_OFF_TO_ROOM) : NULL)
#define GET_AFFECT_APPLY_TO_CHAR(gen)  (GEN_TYPE(gen) == GENERIC_AFFECT ? GEN_STRING((gen), GSTR_AFFECT_APPLY_TO_CHAR) : NULL)
#define GET_AFFECT_APPLY_TO_ROOM(gen)  (GEN_TYPE(gen) == GENERIC_AFFECT ? GEN_STRING((gen), GSTR_AFFECT_APPLY_TO_ROOM) : NULL)
#define GET_AFFECT_LOOK_AT_CHAR(gen)  (GEN_TYPE(gen) == GENERIC_AFFECT ? GEN_STRING((gen), GSTR_AFFECT_LOOK_AT_CHAR) : NULL)
#define GET_AFFECT_LOOK_AT_ROOM(gen)  (GEN_TYPE(gen) == GENERIC_AFFECT ? GEN_STRING((gen), GSTR_AFFECT_LOOK_AT_ROOM) : NULL)
#define GET_AFFECT_DOT_TO_CHAR(gen)  (GEN_TYPE(gen) == GENERIC_AFFECT ? GEN_STRING((gen), GSTR_AFFECT_DOT_TO_CHAR) : NULL)
#define GET_AFFECT_DOT_TO_ROOM(gen)  (GEN_TYPE(gen) == GENERIC_AFFECT ? GEN_STRING((gen), GSTR_AFFECT_DOT_TO_ROOM) : NULL)
#define GET_AFFECT_DEATH_TO_CHAR(gen)  (GEN_TYPE(gen) == GENERIC_AFFECT ? GEN_STRING((gen), GSTR_AFFECT_DEATH_TO_CHAR) : NULL)
#define GET_AFFECT_DEATH_TO_ROOM(gen)  (GEN_TYPE(gen) == GENERIC_AFFECT ? GEN_STRING((gen), GSTR_AFFECT_DEATH_TO_ROOM) : NULL)

// GENERIC_CURRENCY
#define GSTR_CURRENCY_SINGULAR  0
#define GSTR_CURRENCY_PLURAL  1
#define GET_CURRENCY_SINGULAR(gen)  (GEN_TYPE(gen) == GENERIC_CURRENCY ? GEN_STRING((gen), GSTR_CURRENCY_SINGULAR) : NULL)
#define GET_CURRENCY_PLURAL(gen)  (GEN_TYPE(gen) == GENERIC_CURRENCY ? GEN_STRING((gen), GSTR_CURRENCY_PLURAL) : NULL)
#define WHICH_CURRENCY(amt)  (((amt) == 1) ? GSTR_CURRENCY_SINGULAR : GSTR_CURRENCY_PLURAL)

// GENERIC_COMPONENT
#define GSTR_COMPONENT_PLURAL  0
#define GVAL_OBJ_VNUM  1
#define GET_COMPONENT_OBJ_VNUM(gen)  (GEN_TYPE(gen) == GENERIC_COMPONENT ? GEN_VALUE((gen), GVAL_OBJ_VNUM) : NOTHING)
#define GET_COMPONENT_PLURAL(gen)  (GEN_TYPE(gen) == GENERIC_COMPONENT ? GEN_STRING((gen), GSTR_COMPONENT_PLURAL) : NULL)
#define GET_COMPONENT_SINGULAR(gen)  (GEN_TYPE(gen) == GENERIC_COMPONENT ? GEN_NAME(gen) : NULL)

// GENERIC_MOON
#define GVAL_MOON_CYCLE  0
#define GET_MOON_CYCLE(gen)  GEN_VALUE((gen), GVAL_MOON_CYCLE)
#define GET_MOON_CYCLE_DAYS(gen)  (GET_MOON_CYCLE(gen) / 100.0)


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
#define GET_GLOBAL_SPARE_BITS(glb)  ((glb)->spare_bits)
#define GET_GLOBAL_SPAWNS(glb)  ((glb)->spawns)

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

int X_COORD(room_data *room);	// formerly #define X_COORD(room)  FLAT_X_COORD(get_map_location_for(room))
int Y_COORD(room_data *room);	// formerly #define Y_COORD(room)  FLAT_Y_COORD(get_map_location_for(room))

// wrap x/y "around the edge"
#define WRAP_X_COORD(x)  (WRAP_X ? (((x) < 0) ? ((x) + MAP_WIDTH) : (((x) >= MAP_WIDTH) ? ((x) - MAP_WIDTH) : (x))) : MAX(0, MIN(MAP_WIDTH-1, (x))))
#define WRAP_Y_COORD(y)  (WRAP_Y ? (((y) < 0) ? ((y) + MAP_HEIGHT) : (((y) >= MAP_HEIGHT) ? ((y) - MAP_HEIGHT) : (y))) : MAX(0, MIN(MAP_HEIGHT-1, (y))))

#define HAS_SHARED_DATA_TO_SAVE(map)  ((map)->shared != &ocean_shared_data && ((map)->shared->icon || (map)->shared->name || (map)->shared->description || (map)->shared->depletion || (map)->shared->tracks || (map)->shared->extra_data))
#define request_mapout_update(vnum)  add_vnum_hash(&mapout_update_requests, (vnum), 1)


 //////////////////////////////////////////////////////////////////////////////
//// MEMORY UTILS ////////////////////////////////////////////////////////////

#define CREATE(result, type, number)  do {\
	if ((number) * sizeof(type) <= 0)	\
		log("SYSERR: Zero bytes or less requested at %s:%d.", __FILE__, __LINE__);	\
	if (!((result) = (type *) calloc ((number), sizeof(type))))	\
		{ log("SYSERR: malloc failure: %s: %d", __FILE__, __LINE__); abort(); } } while(0)


#define RECREATE(result, type, number) do {\
  if (!((result) = (type *) realloc ((result), sizeof(type) * (number))))\
		{ log("SYSERR: realloc failure: %s: %d", __FILE__, __LINE__); abort(); } } while (0)


// b5.110 replaced this with LL_DELETE everywhere in the code, for consistency, but it is provided here for compatibility
#define REMOVE_FROM_LIST(item, head, next)  LL_DELETE2(head, item, next)


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
#define MOB_LANGUAGE(ch)  ((ch)->mob_specials.language)
#define MOB_MOVE_TYPE(ch)  ((ch)->mob_specials.move_type)
#define MOB_PURSUIT(ch)  ((ch)->mob_specials.pursuit)
#define MOB_PURSUIT_LEASH_LOC(ch)  ((ch)->mob_specials.pursuit_leash_loc)
#define MOB_TAGGED_BY(ch)  ((ch)->mob_specials.tagged_by)
#define MOB_SPAWN_TIME(ch)  ((ch)->mob_specials.spawn_time)
#define MOB_TO_DODGE(ch)  ((ch)->mob_specials.to_dodge)
#define MOB_TO_HIT(ch)  ((ch)->mob_specials.to_hit)
#define MOB_QUEST_LOOKUPS(ch)  ((ch)->quest_lookups)
#define MOB_SHOP_LOOKUPS(ch)  ((ch)->shop_lookups)

// helpers
#define IS_MOB(ch)  (IS_NPC(ch) && GET_MOB_VNUM(ch) != NOTHING)
#define IS_TAGGED_BY(mob, player)  (IS_NPC(mob) && !IS_NPC(player) && find_id_in_tag_list(GET_IDNUM(player), MOB_TAGGED_BY(mob)))
#define MOB_FLAGGED(ch, flag)  (IS_NPC(ch) && IS_SET(MOB_FLAGS(ch), (flag)))
#define MOB_SAVES_TO_ROOM(mob)  (IS_NPC(mob) && GET_MOB_VNUM(mob) != NOTHING && !MOB_FLAGGED((mob), MOB_EMPIRE) && !GET_COMPANION(mob))


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
#define MORPH_LOOK_DESC(mph)  ((mph)->look_desc)
#define MORPH_MAX_SCALE(mph)  ((mph)->max_scale)
#define MORPH_MOVE_TYPE(mph)  ((mph)->move_type)
#define MORPH_REQUIRES_OBJ(mph)  ((mph)->requires_obj)
#define MORPH_SHORT_DESC(mph)  ((mph)->short_desc)
#define MORPH_SIZE(mph)  ((mph)->size)
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
#define GET_OBJ_CURRENT_SCALE_LEVEL(obj)  ((obj)->obj_flags.current_scale_level)
#define GET_OBJ_EQ_SETS(obj)  ((obj)->eq_sets)
#define GET_OBJ_EXTRA(obj)  ((obj)->obj_flags.extra_flags)
#define GET_OBJ_KEYWORDS(obj)  ((obj)->name)
#define GET_OBJ_LONG_DESC(obj)  ((obj)->description)
#define GET_OBJ_SHORT_DESC(obj)  ((obj)->short_description)
#define GET_OBJ_STORED_EVENTS(obj)  ((obj)->stored_events)
#define GET_OBJ_TIMER(obj)  ((obj)->obj_flags.timer)
#define GET_OBJ_VAL(obj, val)  ((obj)->obj_flags.value[(val)])
#define GET_OBJ_WEAR(obj)  ((obj)->obj_flags.wear_flags)
#define GET_STOLEN_TIMER(obj)  ((obj)->stolen_timer)
#define GET_STOLEN_FROM(obj)  ((obj)->stolen_from)
#define LAST_OWNER_ID(obj)  ((obj)->last_owner_id)
#define OBJ_BOUND_TO(obj)  ((obj)->bound_to)
#define OBJ_VERSION(obj)  ((obj)->version)

// prototype data: these are not writable to prevent accidental changes; prototype data is immutable
#define GET_OBJ_COMPONENT(obj)  ((obj)->proto_data ? (obj)->proto_data->component : NOTHING)
#define GET_OBJ_CUSTOM_MSGS(obj)  ((obj)->proto_data ? (obj)->proto_data->custom_msgs : NULL)
#define GET_OBJ_EX_DESCS(obj)  ((obj)->proto_data ? (obj)->proto_data->ex_description : NULL)
#define GET_OBJ_INTERACTIONS(obj)  ((obj)->proto_data ? (obj)->proto_data->interactions : NULL)
#define GET_OBJ_MATERIAL(obj)  ((obj)->proto_data ? (obj)->proto_data->material : 0)
#define GET_OBJ_MAX_SCALE_LEVEL(obj)  ((obj)->proto_data ? (obj)->proto_data->max_scale_level : 0)
#define GET_OBJ_MIN_SCALE_LEVEL(obj)  ((obj)->proto_data ? (obj)->proto_data->min_scale_level : 0)
#define GET_OBJ_QUEST_LOOKUPS(obj)  ((obj)->proto_data ? (obj)->proto_data->quest_lookups : NULL)
#define GET_OBJ_REQUIRES_QUEST(obj)  ((obj)->proto_data ? (obj)->proto_data->requires_quest : NOTHING)
#define GET_OBJ_REQUIRES_TOOL(obj)  ((obj)->proto_data ? (obj)->proto_data->requires_tool : NOTHING)
#define GET_OBJ_SHOP_LOOKUPS(obj)  ((obj)->proto_data ? (obj)->proto_data->shop_lookups : NULL)
#define GET_OBJ_STORAGE(obj)  ((obj)->proto_data ? (obj)->proto_data->storage : NULL)
#define GET_OBJ_TOOL_FLAGS(obj)  ((obj)->proto_data ? (obj)->proto_data->tool_flags : NOBITS)
#define GET_OBJ_TYPE(obj)  ((obj)->proto_data ? (obj)->proto_data->type_flag : ITEM_UNDEFINED)

// compound attributes
#define GET_OBJ_DESC(obj, ch, mode)  get_obj_desc((obj), (ch), (mode))
#define GET_OBJ_VNUM(obj)  ((obj)->vnum)

// definitions
#define IS_STOLEN(obj)  (GET_STOLEN_TIMER(obj) > 0 && (config_get_int("stolen_object_timer") * SECS_PER_REAL_MIN) + GET_STOLEN_TIMER(obj) > time(0))

// helpers
#define OBJ_FLAGGED(obj, flag)  (IS_SET(GET_OBJ_EXTRA(obj), (flag)))
#define OBJS(obj, vict)  (CAN_SEE_OBJ((vict), (obj)) ? GET_OBJ_DESC((obj), vict, 1) : "something")
#define OBJVAL_FLAGGED(obj, flag)  (IS_SET(GET_OBJ_VAL((obj), 1), (flag)))
#define CAN_WEAR(obj, part)  (IS_SET(GET_OBJ_WEAR(obj), (part)))
#define TOOL_FLAGGED(obj, flag)  IS_SET(GET_OBJ_TOOL_FLAGS(obj), (flag))
#define WORN_OR_CARRIED_BY(obj, ch)  ((obj)->worn_by == (ch) || (obj)->carried_by == (ch))

// for stacking, sotring, etc
#define OBJ_CAN_STACK(obj)  (GET_OBJ_TYPE(obj) != ITEM_CONTAINER && !IS_AMMO(obj))
#define OBJ_CAN_STORE(obj)  (GET_OBJ_STORAGE(obj) && GET_OBJ_REQUIRES_QUEST(obj) == NOTHING && !OBJ_BOUND_TO(obj) && !OBJ_FLAGGED((obj), OBJ_NO_STORE | OBJ_SUPERIOR | OBJ_ENCHANTED) && !IS_STOLEN(obj))
#define UNIQUE_OBJ_CAN_STORE(obj, allow_bound)  ((allow_bound || (!OBJ_BOUND_TO(obj) && !OBJ_FLAGGED((obj), OBJ_BIND_ON_PICKUP))) && !OBJ_CAN_STORE(obj) && !OBJ_FLAGGED((obj), OBJ_NO_STORE | OBJ_JUNK) && GET_OBJ_TIMER(obj) == UNLIMITED && GET_OBJ_REQUIRES_QUEST(obj) == NOTHING && !IS_STOLEN(obj))
#define OBJ_STACK_FLAGS  (OBJ_SUPERIOR | OBJ_KEEP | OBJ_ENCHANTED | OBJ_HARD_DROP | OBJ_GROUP_DROP | OBJ_BIND_ON_EQUIP | OBJ_BIND_ON_PICKUP)
#define OBJS_ARE_SAME(o1, o2)  (GET_OBJ_VNUM(o1) == GET_OBJ_VNUM(o2) && GET_OBJ_CURRENT_SCALE_LEVEL(o1) == GET_OBJ_CURRENT_SCALE_LEVEL(o2) && ((GET_OBJ_EXTRA(o1) & OBJ_STACK_FLAGS) == (GET_OBJ_EXTRA(o2) & OBJ_STACK_FLAGS)) && (GET_OBJ_SHORT_DESC(o1) == GET_OBJ_SHORT_DESC(o2) || !strcmp(GET_OBJ_SHORT_DESC(o1), GET_OBJ_SHORT_DESC(o2))) && (GET_OBJ_LONG_DESC(o1) == GET_OBJ_LONG_DESC(o2) || !strcmp(GET_OBJ_LONG_DESC(o1), GET_OBJ_LONG_DESC(o2))) && (!IS_DRINK_CONTAINER(o1) || GET_DRINK_CONTAINER_TYPE(o1) == GET_DRINK_CONTAINER_TYPE(o2)) && (!IS_BOOK(o1) || !IS_BOOK(o2) || GET_BOOK_ID(o1) == GET_BOOK_ID(o2)) && (!IS_AMMO(o1) || !IS_AMMO(o2) || GET_AMMO_QUANTITY(o1) == GET_AMMO_QUANTITY(o2)) && (!IS_LIGHT(o1) || !IS_LIGHT(o2) || GET_LIGHT_IS_LIT(o1) == GET_LIGHT_IS_LIT(o2) || GET_LIGHT_HOURS_REMAINING(o1) == GET_LIGHT_HOURS_REMAINING(o2) || (GET_LIGHT_HOURS_REMAINING(o1) > 0 && GET_LIGHT_HOURS_REMAINING(o2) > 0)) && (IS_STOLEN(o1) == IS_STOLEN(o2)) && identical_bindings((o1),(o2)))


 //////////////////////////////////////////////////////////////////////////////
//// OBJVAL UTILS ////////////////////////////////////////////////////////////

// ITEM_x: getters based on object type

// ITEM_POTION
#define IS_POTION(obj)  (GET_OBJ_TYPE(obj) == ITEM_POTION)
#define VAL_POTION_COOLDOWN_TYPE  0
#define VAL_POTION_COOLDOWN_TIME  1
#define VAL_POTION_AFFECT  2
#define GET_POTION_COOLDOWN_TYPE(obj)  (IS_POTION(obj) ? GET_OBJ_VAL((obj), VAL_POTION_COOLDOWN_TYPE) : NOTHING)
#define GET_POTION_COOLDOWN_TIME(obj)  (IS_POTION(obj) ? GET_OBJ_VAL((obj), VAL_POTION_COOLDOWN_TIME) : NOTHING)
#define GET_POTION_AFFECT(obj)  (IS_POTION(obj) ? GET_OBJ_VAL((obj), VAL_POTION_AFFECT) : 0)

// ITEM_POISON
#define IS_POISON(obj)  (GET_OBJ_TYPE(obj) == ITEM_POISON)
#define VAL_POISON_CHARGES  1
#define VAL_POISON_AFFECT  2
#define GET_POISON_CHARGES(obj)  (IS_POISON(obj) ? GET_OBJ_VAL((obj), VAL_POISON_CHARGES) : 0)
#define GET_POISON_AFFECT(obj)  (IS_POISON(obj) ? GET_OBJ_VAL((obj), VAL_POISON_AFFECT) : NOTHING)

// ITEM_RECIPE
#define IS_RECIPE(obj)  (GET_OBJ_TYPE(obj) == ITEM_RECIPE)
#define VAL_RECIPE_VNUM  0
#define GET_RECIPE_VNUM(obj)  (IS_RECIPE(obj) ? GET_OBJ_VAL((obj), VAL_RECIPE_VNUM) : NOTHING)


// ITEM_WEAPON
#define IS_WEAPON(obj)  (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
#define VAL_WEAPON_DAMAGE_BONUS  1
#define VAL_WEAPON_TYPE  2
#define GET_WEAPON_DAMAGE_BONUS(obj)  (IS_WEAPON(obj) ? GET_OBJ_VAL((obj), VAL_WEAPON_DAMAGE_BONUS) : 0)
#define GET_WEAPON_TYPE(obj)  (IS_WEAPON(obj) ? GET_OBJ_VAL((obj), VAL_WEAPON_TYPE) : TYPE_UNDEFINED)

#define IS_ANY_WEAPON(obj)  (IS_WEAPON(obj) || IS_MISSILE_WEAPON(obj))
#define IS_STAFF(obj)  (GET_WEAPON_TYPE(obj) == TYPE_LIGHTNING_STAFF || GET_WEAPON_TYPE(obj) == TYPE_BURN_STAFF || GET_WEAPON_TYPE(obj) == TYPE_AGONY_STAFF || TOOL_FLAGGED((obj), TOOL_STAFF))

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
#define VAL_CORPSE_SIZE  1
#define VAL_CORPSE_FLAGS  2
#define IS_NPC_CORPSE(obj)  (IS_CORPSE(obj) && GET_OBJ_VAL((obj), VAL_CORPSE_IDNUM) >= 0)
#define IS_PC_CORPSE(obj)  (IS_CORPSE(obj) && GET_OBJ_VAL((obj), VAL_CORPSE_IDNUM) < 0)
#define GET_CORPSE_NPC_VNUM(obj)  (IS_NPC_CORPSE(obj) ? GET_OBJ_VAL((obj), VAL_CORPSE_IDNUM) : NOBODY)
#define GET_CORPSE_PC_ID(obj)  (IS_PC_CORPSE(obj) ? (-1 * GET_OBJ_VAL((obj), VAL_CORPSE_IDNUM)) : NOBODY)
#define GET_CORPSE_FLAGS(obj)  (IS_CORPSE(obj) ? GET_OBJ_VAL((obj), VAL_CORPSE_FLAGS) : 0)
#define IS_CORPSE_FLAG_SET(obj, flag)  (GET_CORPSE_FLAGS(obj) & (flag))
#define GET_CORPSE_SIZE(obj)  (IS_CORPSE(obj) ? GET_OBJ_VAL((obj), VAL_CORPSE_SIZE) : SIZE_NORMAL)

// ITEM_COINS
#define IS_COINS(obj)  (GET_OBJ_TYPE(obj) == ITEM_COINS)
#define VAL_COINS_AMOUNT  0
#define VAL_COINS_EMPIRE_ID  1
#define GET_COINS_AMOUNT(obj)  (IS_COINS(obj) ? GET_OBJ_VAL((obj), VAL_COINS_AMOUNT) : 0)
#define GET_COINS_EMPIRE_ID(obj)  (IS_COINS(obj) ? GET_OBJ_VAL((obj), VAL_COINS_EMPIRE_ID) : 0)

// ITEM_CURRENCY
#define IS_CURRENCY(obj)  (GET_OBJ_TYPE(obj) == ITEM_CURRENCY)
#define VAL_CURRENCY_AMOUNT  0
#define VAL_CURRENCY_VNUM  1
#define GET_CURRENCY_AMOUNT(obj)  (IS_CURRENCY(obj) ? GET_OBJ_VAL((obj), VAL_CURRENCY_AMOUNT) : 0)
#define GET_CURRENCY_VNUM(obj)  (IS_CURRENCY(obj) ? GET_OBJ_VAL((obj), VAL_CURRENCY_VNUM) : 0)

// ITEM_MAIL

// ITEM_WEALTH
#define IS_WEALTH_ITEM(obj)  (GET_OBJ_TYPE(obj) == ITEM_WEALTH)
#define VAL_WEALTH_VALUE  0
#define VAL_WEALTH_MINT_FLAGS  1
#define GET_WEALTH_VALUE(obj)  (IS_WEALTH_ITEM(obj) ? GET_OBJ_VAL((obj), VAL_WEALTH_VALUE) : 0)
#define GET_WEALTH_MINT_FLAGS(obj)  (IS_WEALTH_ITEM(obj) ? GET_OBJ_VAL((obj), VAL_WEALTH_MINT_FLAGS) : NOBITS)
#define IS_MINT_FLAGGED(obj, flag)  IS_SET(GET_WEALTH_MINT_FLAGS(obj), (flag))

// ITEM_MISSILE_WEAPON
#define IS_MISSILE_WEAPON(obj)  (GET_OBJ_TYPE(obj) == ITEM_MISSILE_WEAPON)
#define VAL_MISSILE_WEAPON_TYPE  0	// attack type
#define VAL_MISSILE_WEAPON_DAMAGE  1
#define VAL_MISSILE_WEAPON_AMMO_TYPE  2
#define GET_MISSILE_WEAPON_TYPE(obj)  (IS_MISSILE_WEAPON(obj) ? GET_OBJ_VAL((obj), VAL_MISSILE_WEAPON_TYPE) : 0)
#define GET_MISSILE_WEAPON_DAMAGE(obj)  (IS_MISSILE_WEAPON(obj) ? GET_OBJ_VAL((obj), VAL_MISSILE_WEAPON_DAMAGE) : 0)
#define GET_MISSILE_WEAPON_AMMO_TYPE(obj)  (IS_MISSILE_WEAPON(obj) ? GET_OBJ_VAL((obj), VAL_MISSILE_WEAPON_AMMO_TYPE) : NOTHING)

// ITEM_AMMO
#define IS_AMMO(obj)  (GET_OBJ_TYPE(obj) == ITEM_AMMO)
#define VAL_AMMO_QUANTITY  0
#define VAL_AMMO_DAMAGE_BONUS  1
#define VAL_AMMO_TYPE  2
#define GET_AMMO_DAMAGE_BONUS(obj)  (IS_AMMO(obj) ? GET_OBJ_VAL((obj), VAL_AMMO_DAMAGE_BONUS) : 0)
#define GET_AMMO_TYPE(obj)  (IS_AMMO(obj) ? GET_OBJ_VAL((obj), VAL_AMMO_TYPE) : NOTHING)
#define GET_AMMO_QUANTITY(obj)  (IS_AMMO(obj) ? GET_OBJ_VAL((obj), VAL_AMMO_QUANTITY) : 0)

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

// ITEM_PAINT
#define IS_PAINT(obj)  (GET_OBJ_TYPE(obj) == ITEM_PAINT)
#define VAL_PAINT_COLOR  0
#define GET_PAINT_COLOR(obj)  (IS_PAINT(obj) ? GET_OBJ_VAL((obj), VAL_PAINT_COLOR) : 0)

// ITEM_LIGHTER
#define IS_LIGHTER(obj)  (GET_OBJ_TYPE(obj) == ITEM_LIGHTER)
#define VAL_LIGHTER_USES  0
#define GET_LIGHTER_USES(obj)  (IS_LIGHTER(obj) ? GET_OBJ_VAL((obj), VAL_LIGHTER_USES) : 0)

// ITEM_MINIPET
#define IS_MINIPET(obj)  (GET_OBJ_TYPE(obj) == ITEM_MINIPET)
#define VAL_MINIPET_VNUM  0
#define GET_MINIPET_VNUM(obj)  (IS_MINIPET(obj) ? GET_OBJ_VAL((obj), VAL_MINIPET_VNUM) : NOTHING)

// ITEM_LIGHT
#define IS_LIGHT(obj)  (GET_OBJ_TYPE(obj) == ITEM_LIGHT)
#define VAL_LIGHT_HOURS_REMAINING  0
#define VAL_LIGHT_IS_LIT  1
#define VAL_LIGHT_FLAGS  2
#define GET_LIGHT_HOURS_REMAINING(obj)  (IS_LIGHT(obj) ? GET_OBJ_VAL((obj), VAL_LIGHT_HOURS_REMAINING) : 0)
#define GET_LIGHT_IS_LIT(obj)  (IS_LIGHT(obj) ? GET_OBJ_VAL((obj), VAL_LIGHT_IS_LIT) : FALSE)
#define GET_LIGHT_FLAGS(obj)  (IS_LIGHT(obj) ? GET_OBJ_VAL((obj), VAL_LIGHT_FLAGS) : NOBITS)
#define CAN_LIGHT_OBJ(obj)  ((IS_LIGHT(obj) && !GET_LIGHT_IS_LIT(obj) && GET_LIGHT_HOURS_REMAINING(obj) != 0) || has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_LIGHT))
#define LIGHT_FLAGGED(obj, flag)  IS_SET(GET_LIGHT_FLAGS(obj), (flag))
#define LIGHT_IS_LIT(obj)  (OBJ_FLAGGED((obj), OBJ_LIGHT) || (GET_LIGHT_IS_LIT(obj) && GET_LIGHT_HOURS_REMAINING(obj) != 0))


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER UTILS ////////////////////////////////////////////////////////////

// ch->char_specials: char_special_data
#define AFF_FLAGS(ch)  ((ch)->char_specials.affected_by)
#define INJURY_FLAGS(ch)  ((ch)->char_specials.injuries)
#define PLR_FLAGS(ch)  (REAL_CHAR(ch)->char_specials.act)
#define SYSLOG_FLAGS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->syslogs))
#define GET_LAST_DIR(ch)  ((ch)->char_specials.last_direction)
#define GET_MORPH(ch)  ((ch)->char_specials.morph)

// ch->player_specials: player_special_data
#define AFFECTS_CONVERTED(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->affects_converted))
#define CAN_GAIN_NEW_SKILLS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->can_gain_new_skills))
#define CAN_GET_BONUS_SKILLS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->can_get_bonus_skills))
#define CREATION_ARCHETYPE(ch, pos)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->creation_archetype[pos]))
#define DONT_SAVE_DELAY(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->dont_save_delay))
#define GET_ABILITY_HASH(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->ability_hash))
#define GET_ABILITY_GAIN_HOOKS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->gain_hooks))
#define GET_ACCOUNT(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->account))
#define GET_ACTION(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action))
#define GET_ACTION_CYCLE(ch) CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action_cycle))
#define GET_ACTION_ROOM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action_room))
#define GET_ACTION_TIMER(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action_timer))
#define GET_ACTION_VNUM(ch, n)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action_vnum[(n)]))
#define GET_ACTION_RESOURCES(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->action_resources))
#define GET_ADVENTURE_SUMMON_INSTANCE_ID(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->adventure_summon_instance_id))
#define GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->adventure_summon_return_location))
#define GET_ADVENTURE_SUMMON_RETURN_MAP(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->adventure_summon_return_map))
#define GET_ALIASES(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->aliases))
#define GET_APPARENT_AGE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->apparent_age))
#define GET_AUTOMESSAGES(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->automessages))
#define GET_BAD_PWS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->bad_pws))
#define GET_BECKONED_BY(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->beckoned_by))
#define GET_BONUS_TRAITS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->bonus_traits))
#define GET_CLASS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->character_class))
#define GET_CLASS_PROGRESSION(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->class_progression))
#define GET_CLASS_ROLE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->class_role))
#define GET_COMBAT_METERS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->meters))
#define GET_COMPANIONS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->companions))
#define GET_COMPLETED_QUESTS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->completed_quests))
#define GET_COMPUTED_LEVEL(ch)  (GET_SKILL_LEVEL(ch) + GET_GEAR_LEVEL(ch))
#define GET_COND(ch, i)  CHECK_PLAYER_SPECIAL(REAL_CHAR(ch), (REAL_CHAR(ch)->player_specials->conditions[(i)]))
#define GET_CONFUSED_DIR(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->confused_dir))
#define GET_CREATION_ALT_ID(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->create_alt_id))
#define GET_CREATION_HOST(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->creation_host))
#define GET_CURRENCIES(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->currencies))
#define GET_CURRENT_LASTNAME(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->current_lastname))
#define GET_CURRENT_SKILL_SET(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->current_skill_set))
#define GET_CUSTOM_COLOR(ch, pos)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->custom_colors[(pos)]))
#define GET_DAILY_BONUS_EXPERIENCE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->daily_bonus_experience))
#define GET_DAILY_CYCLE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->daily_cycle))
#define GET_DAILY_QUESTS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->daily_quests))
#define GET_DISGUISED_NAME(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->disguised_name))
#define GET_DISGUISED_SEX(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->disguised_sex))
#define GET_EQ_SETS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->eq_sets))
#define GET_EVENT_DAILY_QUESTS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->event_daily_quests))
#define GET_EVENT_DATA(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->event_data))
#define GET_EXP_TODAY(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->exp_today))
#define GET_FACTIONS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->factions))
#define GET_FIGHT_MESSAGES(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->fight_messages))
#define GET_FIGHT_PROMPT(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->fight_prompt))
#define GET_GEAR_LEVEL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->gear_level))
#define GET_GRANT_FLAGS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->grants))
#define GET_GROUP_INVITE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->group_invite_by))
#define GET_HIGHEST_KNOWN_LEVEL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->highest_known_level))
#define GET_HISTORY(ch, type)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->channel_history[(type)]))
#define GET_HOME_STORAGE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->home_storage))
#define GET_IDLE_SECONDS(ch)  CHECK_PLAYER_SPECIAL(REAL_CHAR(ch), (REAL_CHAR(ch)->player_specials->idle_seconds))
#define GET_IGNORE_LIST(ch, pos)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->ignore_list[(pos)]))
#define GET_IMMORTAL_LEVEL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->immortal_level))
#define GET_INFORMATIVE_FLAGS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->informative_flags))
#define GET_INVIS_LEV(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->invis_level))
#define GET_LANGUAGES(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->languages))
#define GET_LARGEST_INVENTORY(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->largest_inventory))
#define GET_LAST_AFF_WEAR_OFF_VNUM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_aff_wear_off_vnum))
#define GET_LAST_AFF_WEAR_OFF_TIME(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_aff_wear_off_time))
#define GET_LAST_COLD_TIME(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_cold_time))
#define GET_LAST_COMPANION(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_companion))
#define GET_LAST_COND_MESSAGE_TIME(ch, cond)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_cond_message_time[(cond)]))
#define GET_LAST_CORPSE_ID(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_corpse_id))
#define GET_LAST_DEATH_TIME(ch)  CHECK_PLAYER_SPECIAL(REAL_CHAR(ch), (REAL_CHAR(ch)->player_specials->last_death_time))
#define GET_LAST_GOAL_CHECK(ch)  CHECK_PLAYER_SPECIAL(REAL_CHAR(ch), (REAL_CHAR(ch)->player_specials->last_goal_check))
#define GET_LAST_HOME_SET_TIME(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_home_set_time))
#define GET_LAST_KNOWN_LEVEL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_known_level))
#define GET_LAST_LOOK_SUN(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_look_sun))
#define GET_LAST_MESSAGED_TEMPERATURE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_messaged_temperature))
#define GET_LAST_OFFENSE_SEEN(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_offense_seen))
#define GET_LAST_ROOM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_room))
#define GET_LAST_TELL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_tell))
#define GET_LAST_TIP(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_tip))
#define GET_LASTNAME_LIST(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->lastname_list))
#define GET_LAST_VEHICLE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_vehicle))
#define GET_LAST_WARM_TIME(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->last_warm_time))
#define GET_LEARNED_CRAFTS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->learned_crafts))
#define GET_LOADROOM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->load_room))
#define GET_LOAD_ROOM_CHECK(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->load_room_check))
#define GET_MAIL_PENDING(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->mail_pending))
#define GET_MAP_MEMORY(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->map_memory))
#define GET_MAP_MEMORY_COUNT(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->map_memory_count))
#define GET_MAP_MEMORY_LOADED(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->map_memory_loaded))
#define GET_MAP_MEMORY_NEEDS_SAVE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->map_memory_needs_save))
#define GET_MAPSIZE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->mapsize))
#define GET_MARK_LOCATION(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->marked_location))
#define GET_MINIPETS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->minipets))
#define GET_MOUNT_FLAGS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->mount_flags))
#define GET_MOUNT_LIST(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->mount_list))
#define GET_MOUNT_VNUM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->mount_vnum))
#define GET_MOVE_TIME(ch, pos)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->move_time[(pos)]))
#define GET_MOVEMENT_STRING(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->movement_string))
#define GET_OFFERS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->offers))
#define GET_OLC_FLAGS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->olc_flags))
#define GET_OLC_MAX_VNUM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->olc_max_vnum))
#define GET_OLC_MIN_VNUM(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->olc_min_vnum))
#define GET_PASSIVE_BUFFS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->passive_buffs))
#define GET_PERSONAL_LASTNAME(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->personal_lastname))
#define GET_PLAYER_COINS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->coins))
#define GET_PLEDGE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->pledge))
#define GET_PROMO_ID(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->promo_id))
#define GET_PROMPT(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->prompt))
#define GET_QUESTS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->quests))
#define GET_RANK(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->rank))
#define GET_RECENT_DEATH_COUNT(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->recent_death_count))
#define GET_REFERRED_BY(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->referred_by))
#define GET_RESOURCE(ch, i)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->resources[i]))
#define GET_SKILL_HASH(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->skill_hash))
#define GET_SKILL_LEVEL(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->skill_level))
#define GET_SLASH_CHANNELS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->slash_channels))
#define GET_SPEAKING(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->speaking))
#define GET_STATUS_MESSAGES(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->status_messages))
#define GET_TECHS(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->techs))
#define GET_TEMPERATURE(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->temperature))
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
#define USING_AMMO(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->using_ammo))
#define USING_POISON(ch)  CHECK_PLAYER_SPECIAL((ch), ((ch)->player_specials->using_poison))

// helpers
#define ACCOUNT_FLAGGED(ch, flag)  (!IS_NPC(ch) && GET_ACCOUNT(ch) && IS_SET(GET_ACCOUNT(ch)->flags, (flag)))
#define CUSTOM_COLOR_CHAR(ch, which)  ((!IS_NPC(ch) && GET_CUSTOM_COLOR((ch), (which))) ? GET_CUSTOM_COLOR((ch), (which)) : '0')
#define HAS_BONUS_TRAIT(ch, flag)  (!IS_NPC(ch) && IS_SET(GET_BONUS_TRAITS(ch), (flag)))
#define HAS_NEW_OFFENSES(ch) (!IS_NPC(ch) && GET_LOYALTY(ch) && EMPIRE_OFFENSES(GET_LOYALTY(ch)) && EMPIRE_OFFENSES(GET_LOYALTY(ch))->timestamp > GET_LAST_OFFENSE_SEEN(ch))
#define INFORMATIVE_FLAGGED(ch, flag)  (!IS_NPC(ch) && IS_SET(GET_INFORMATIVE_FLAGS(ch), (flag)))
#define IS_AFK(ch)  (!IS_NPC(ch) && (PRF_FLAGGED((ch), PRF_AFK) || (GET_IDLE_SECONDS(ch) / SECS_PER_REAL_MIN) >= 5))
#define IS_GRANTED(ch, flag)  (!IS_NPC(ch) && IS_SET(GET_GRANT_FLAGS(ch), (flag)))
#define IS_MORPHED(ch)  (GET_MORPH(ch) != NULL)
#define IS_PVP_FLAGGED(ch)  (!IS_NPC(ch) && (PRF_FLAGGED((ch), PRF_ALLOW_PVP) || get_cooldown_time((ch), COOLDOWN_PVP_FLAG) > 0))
#define MOUNT_FLAGGED(ch, flag)  (!IS_NPC(ch) && IS_SET(GET_MOUNT_FLAGS(ch), (flag)))
#define NOHASSLE(ch)  (!IS_NPC(ch) && IS_IMMORTAL(ch) && PRF_FLAGGED((ch), PRF_NOHASSLE))
#define PLR_FLAGGED(ch, flag)  (!REAL_NPC(ch) && IS_SET(PLR_FLAGS(ch), (flag)))
#define PRF_FLAGGED(ch, flag)  (!REAL_NPC(ch) && IS_SET(PRF_FLAGS(ch), (flag)))
#define OLC_FLAGGED(ch, flag)  (!IS_NPC(ch) && IS_SET(GET_OLC_FLAGS(ch), (flag)))
#define RESET_LAST_MESSAGED_TEMPERATURE(ch)  { if (!IS_NPC(ch)) { GET_LAST_MESSAGED_TEMPERATURE(ch) = get_room_temperature(IN_ROOM(ch)); } }
#define SAVE_ACCOUNT(acct)  save_library_file_for_vnum(DB_BOOT_ACCT, (acct)->id)
#define SHOW_CLASS_NAME(ch)  ((!IS_NPC(ch) && GET_CLASS(ch)) ? CLASS_NAME(GET_CLASS(ch)) : config_get_string("default_class_name"))
#define SHOW_FIGHT_MESSAGES(ch, bit)  (!IS_NPC(ch) && IS_SET(GET_FIGHT_MESSAGES(ch), (bit)))
#define SHOW_STATUS_MESSAGES(ch, bit)  (!IS_NPC(ch) && IS_SET(GET_STATUS_MESSAGES(ch), (bit)))

// definitions
#define HAS_CLOCK(ch)  (HAS_BONUS_TRAIT((ch), BONUS_CLOCK) || has_player_tech((ch), PTECH_CLOCK))
#define HAS_NAVIGATION(ch)  has_player_tech(ch, PTECH_NAVIGATION)
#define IN_HOSTILE_TERRITORY(ch)  (!IS_NPC(ch) && !IS_IMMORTAL(ch) && ROOM_OWNER(IN_ROOM(ch)) && ROOM_OWNER(IN_ROOM(ch)) != GET_LOYALTY(ch) && (IS_HOSTILE(ch) || empire_is_hostile(ROOM_OWNER(IN_ROOM(ch)), GET_LOYALTY(ch), IN_ROOM(ch))))
#define IS_APPROVED(ch)  (IS_NPC(ch) || PLR_FLAGGED(ch, PLR_APPROVED) || ACCOUNT_FLAGGED(ch, ACCT_APPROVED))
#define IS_HOSTILE(ch)  (!IS_NPC(ch) && (get_cooldown_time((ch), COOLDOWN_HOSTILE_FLAG) > 0 || get_cooldown_time((ch), COOLDOWN_ROGUE_FLAG) > 0))
#define IS_HUNGRY(ch)  (GET_COND((ch), FULL) >= (REAL_UPDATES_PER_MUD_HOUR * 24) && !HAS_BONUS_TRAIT((ch), BONUS_NO_HUNGER) && !has_player_tech((ch), PTECH_NO_HUNGER))
#define IS_DRUNK(ch)  (GET_COND(ch, DRUNK) >= (REAL_UPDATES_PER_MUD_HOUR * 24))
#define IS_GOD(ch)  (GET_ACCESS_LEVEL(ch) == LVL_GOD)
#define IS_IMMORTAL(ch)  (GET_ACCESS_LEVEL(ch) >= LVL_START_IMM)
#define IS_RIDING(ch)  (!IS_NPC(ch) && GET_MOUNT_VNUM(ch) != NOTHING && MOUNT_FLAGGED(ch, MOUNT_RIDING))
#define IS_THIRSTY(ch)  (GET_COND((ch), THIRST) >= (REAL_UPDATES_PER_MUD_HOUR * 24) && !HAS_BONUS_TRAIT((ch), BONUS_NO_THIRST) && !has_player_tech((ch), PTECH_NO_THIRST))
#define IS_BLOOD_STARVED(ch)  (IS_VAMPIRE(ch) && GET_BLOOD(ch) <= config_get_int("blood_starvation_level"))

// for act() and act-like things (requires to_sleeping and is_spammy set to true/false)
#define SENDOK(ch)  (((ch)->desc || SCRIPT_CHECK((ch), MTRIG_ACT)) && (to_sleeping || AWAKE(ch)) && (!is_spammy || !PRF_FLAGGED((ch), PRF_NOSPAM)) && (!is_animal_move || IS_NPC(ch) || SHOW_STATUS_MESSAGES((ch), SM_ANIMAL_MOVEMENT)))


 //////////////////////////////////////////////////////////////////////////////
//// PROGRESS UTILS //////////////////////////////////////////////////////////

#define PRG_VNUM(prg)  ((prg)->vnum)
#define PRG_COST(prg)  ((prg)->cost)
#define PRG_DESCRIPTION(prg)  ((prg)->description)
#define PRG_FLAGS(prg)  ((prg)->flags)
#define PRG_NAME(prg)  ((prg)->name)
#define PRG_PERKS(prg)  ((prg)->perks)
#define PRG_PREREQS(prg)  ((prg)->prereqs)
#define PRG_TASKS(prg)  ((prg)->tasks)
#define PRG_TYPE(prg)  ((prg)->type)
#define PRG_VALUE(prg)  ((prg)->value)
#define PRG_VERSION(prg)  ((prg)->version)

#define PRG_FLAGGED(prg, flg)  IS_SET(PRG_FLAGS(prg), (flg))


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

#define IS_DAILY_QUEST(quest) (QUEST_FLAGGED((quest), QST_DAILY))
#define IS_EVENT_QUEST(quest) (QUEST_FLAGGED((quest), QST_EVENT))
#define IS_EVENT_DAILY(quest) (QUEST_FLAGGED((quest), QST_DAILY) && QUEST_FLAGGED((quest), QST_EVENT))
#define IS_NON_EVENT_DAILY(quest) (QUEST_FLAGGED((quest), QST_DAILY) && !QUEST_FLAGGED((quest), QST_EVENT))


 //////////////////////////////////////////////////////////////////////////////
//// ROOM UTILS //////////////////////////////////////////////////////////////

// basic room data
#define GET_ROOM_VNUM(room)  ((room)->vnum)
#define ROOM_AFFECTS(room)  ((room)->af)
#define ROOM_CONTENTS(room)  ((room)->contents)
#define ROOM_CROP(room)  ((room)->crop_type)
#define ROOM_LAST_SPAWN_TIME(room)  ((room)->last_spawn_time)
#define ROOM_LIGHTS(room)  ((room)->light)
#define BASE_SECT(room)  ((room)->base_sector)
#define ROOM_OWNER(room)  ((room)->owner)
#define ROOM_PATHFIND_KEY(room)  ((room)->pathfind_key)
#define ROOM_PEOPLE(room)  ((room)->people)
#define ROOM_UNLOAD_EVENT(room)  ((room)->unload_event)
#define ROOM_VEHICLES(room)  ((room)->vehicles)
#define SECT(room)  ((room)->sector_type)
#define GET_EXITS_HERE(room)  ((room)->exits_here)
#define GET_MAP_LOC(room)  ((room)->map_loc)


// room->complex data
#define COMPLEX_DATA(room)  ((room)->complex)
#define GET_BUILDING(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->bld_ptr : NULL)
#define GET_ROOM_TEMPLATE(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->rmt_ptr : NULL)
#define IN_VEHICLE_IN_ROOM(room)  (GET_ROOM_VEHICLE(room) ? IN_ROOM(GET_ROOM_VEHICLE(room)) : room)
#define BUILDING_BURN_DOWN_TIME(room)  (COMPLEX_DATA(HOME_ROOM(room)) ? COMPLEX_DATA(HOME_ROOM(room))->burn_down_time : 0)
#define BUILDING_DAMAGE(room)  (COMPLEX_DATA(HOME_ROOM(room)) ? COMPLEX_DATA(HOME_ROOM(room))->damage : 0)
#define BUILDING_ENTRANCE(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->entrance : NO_DIR)
#define BUILDING_RESOURCES(room)  (COMPLEX_DATA(room) ? GET_BUILDING_RESOURCES(room) : NULL)
#define GET_ROOM_VEHICLE(room)  (COMPLEX_DATA(HOME_ROOM(room)) ? COMPLEX_DATA(HOME_ROOM(room))->vehicle : NULL)
#define GET_BUILDING_RESOURCES(room)  (COMPLEX_DATA(room)->to_build)
#define GET_BUILT_WITH(room)  (COMPLEX_DATA(room)->built_with)
#define GET_INSIDE_ROOMS(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->inside_rooms : 0)
#define HOME_ROOM(room)  ((COMPLEX_DATA(room) && COMPLEX_DATA(room)->home_room) ? COMPLEX_DATA(room)->home_room : (room))
#define IS_BURNING(room)  (BUILDING_BURN_DOWN_TIME(room) > 0)
#define IS_COMPLETE(room)  (!IS_INCOMPLETE(room) && !IS_DISMANTLING(room))
#define ROOM_PAINT_COLOR(room)  get_room_extra_data((room), ROOM_EXTRA_PAINT_COLOR)
#define ROOM_PATRON(room)  get_room_extra_data((room), ROOM_EXTRA_DEDICATE_ID)
#define ROOM_PRIVATE_OWNER(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->private_owner : NOBODY)
#define ROOM_INSTANCE(room)  (COMPLEX_DATA(room) ? COMPLEX_DATA(room)->instance : NULL)


// shared data
#define SHARED_DATA(room)  ((room)->shared)
#define ROOM_AFF_FLAGS(room)  (SHARED_DATA(room)->affects)
#define ROOM_BASE_FLAGS(room)  (SHARED_DATA(room)->base_affects)
#define ROOM_CUSTOM_DESCRIPTION(room)  (SHARED_DATA(room)->description)
#define ROOM_CUSTOM_ICON(room)  (SHARED_DATA(room)->icon)
#define ROOM_CUSTOM_NAME(room)  (SHARED_DATA(room)->name)
#define ROOM_DEPLETION(room)  (SHARED_DATA(room)->depletion)
#define ROOM_EXTRA_DATA(room)  (SHARED_DATA(room)->extra_data)
#define ROOM_HEIGHT(room)  (SHARED_DATA(room)->height)
#define ROOM_TRACKS(room)  (SHARED_DATA(room)->tracks)
#define GET_ISLAND(room)  (SHARED_DATA(room)->island_ptr)
#define GET_ISLAND_ID(room)  (SHARED_DATA(room)->island_id)


// exits
#define CAN_GO(ch, ex) (ex->room_ptr && !EXIT_FLAGGED(ex, EX_CLOSED))
#define EXIT_FLAGGED(exit, flag)  (IS_SET((exit)->exit_info, (flag)))

// building data by room
#define BLD_MAX_ROOMS(room)  (GET_BUILDING(HOME_ROOM(room)) ? GET_BLD_EXTRA_ROOMS(GET_BUILDING(HOME_ROOM(room))) : 0)
#define BLD_BASE_AFFECTS(room)  (GET_BUILDING(HOME_ROOM(room)) ? GET_BLD_BASE_AFFECTS(GET_BUILDING(HOME_ROOM(room))) : NOBITS)

// definitions
#define BLD_ALLOWS_MOUNTS(room)  (ROOM_IS_CLOSED(room) ? (ROOM_BLD_FLAGGED((room), BLD_ALLOW_MOUNTS | BLD_OPEN) || RMT_FLAGGED((room), RMT_OUTDOOR)) : TRUE)
#define CAN_CHOP_ROOM(room)  (has_evolution_type(SECT(room), EVO_CHOPPED_DOWN) || can_interact_room((room), INTERACT_CHOP) || (ROOM_SECT_FLAGGED((room), SECTF_CROP) && ROOM_CROP_FLAGGED((room), CROPF_IS_ORCHARD)))
#define DEPLETION_LIMIT(room)  (ROOM_BLD_FLAGGED((room), BLD_HIGH_DEPLETION) ? config_get_int("high_depletion") : config_get_int("common_depletion"))
#define HAS_MINOR_DISREPAIR(room)  (HOME_ROOM(room) == room && GET_BUILDING(room) && BUILDING_DAMAGE(room) > 0 && (BUILDING_DAMAGE(room) >= (GET_BLD_MAX_DAMAGE(GET_BUILDING(room)) * config_get_int("disrepair_minor") / 100)))
#define HAS_MAJOR_DISREPAIR(room)  (HOME_ROOM(room) == room && GET_BUILDING(room) && BUILDING_DAMAGE(room) > 0 && (BUILDING_DAMAGE(room) >= (GET_BLD_MAX_DAMAGE(GET_BUILDING(room)) * config_get_int("disrepair_major") / 100)))
#define IS_CITY_CENTER(room)  (BUILDING_VNUM(room) == BUILDING_CITY_CENTER)
#define ISLAND_FLAGGED(room, flag)  (GET_ISLAND(room) ? IS_SET(GET_ISLAND(room)->flags, (flag)) : FALSE)
#define MAGIC_DARKNESS(room)  (ROOM_AFF_FLAGGED((room), ROOM_AFF_DARK))
#define NO_LOCATION(room)  (RMT_FLAGGED(room, RMT_NO_LOCATION) || RMT_FLAGGED(IN_VEHICLE_IN_ROOM(room), RMT_NO_LOCATION))
#define ROOM_CAN_EXIT(room)  (ROOM_BLD_FLAGGED((room), BLD_EXIT) || (GET_ROOM_VEHICLE(room) && room == HOME_ROOM(room)))
#define ROOM_CAN_MINE(room)  (ROOM_SECT_FLAGGED((room), SECTF_CAN_MINE) || room_has_function_and_city_ok(ROOM_OWNER(room), (room), FNC_MINE) || (IS_ROAD(room) && SECT_FLAGGED(BASE_SECT(room), SECTF_CAN_MINE)))
#define ROOM_IS_CLOSED(room)  (IS_INSIDE(room) || IS_ADVENTURE_ROOM(room) || (IS_ANY_BUILDING(room) && !ROOM_BLD_FLAGGED(room, BLD_OPEN) && (IS_COMPLETE(room) || ROOM_BLD_FLAGGED(room, BLD_CLOSED))))
#define ROOM_IS_UPGRADED(room)  ((IS_COMPLETE(room) && HAS_FUNCTION((room), FNC_UPGRADED)) || (IS_COMPLETE(HOME_ROOM(room)) && HAS_FUNCTION(HOME_ROOM(room), FNC_UPGRADED)) || (GET_ROOM_VEHICLE(room) && IS_SET(VEH_FUNCTIONS(GET_ROOM_VEHICLE(room)), FNC_UPGRADED)))
#define ROOM_NEEDS_PACK_SAVE(room)  (ROOM_CONTENTS(room) || ROOM_VEHICLES(room))
#define SHOW_PEOPLE_IN_ROOM(room)  (!ROOM_IS_CLOSED(room) && !ROOM_SECT_FLAGGED(room, SECTF_OBSCURE_VISION))

// interaction checks (by type)
#define BLD_CAN_INTERACT_ROOM(room, type)  (GET_BUILDING(room) && has_interaction(GET_BLD_INTERACTIONS(GET_BUILDING(room)), (type)))
#define CROP_CAN_INTERACT_ROOM(room, type)  (ROOM_SECT_FLAGGED(room, SECTF_CROP) && ROOM_CROP(room) && has_interaction(GET_CROP_INTERACTIONS(ROOM_CROP(room)), (type)))
#define RMT_CAN_INTERACT_ROOM(room, type)  (GET_ROOM_TEMPLATE(room) && has_interaction(GET_RMT_INTERACTIONS(GET_ROOM_TEMPLATE(room)), (type)))
#define SECT_CAN_INTERACT_ROOM(room, type)  has_interaction(GET_SECT_INTERACTIONS(SECT(room)), (type))
// room interactions including vehicles: bool can_interact_room(room, type)
// if you don't want to check vehicles, you can use this macro:
#define CAN_INTERACT_ROOM_NO_VEH(room, type)  (SECT_CAN_INTERACT_ROOM((room), (type)) || BLD_CAN_INTERACT_ROOM((room), (type)) || RMT_CAN_INTERACT_ROOM((room), (type)) || CROP_CAN_INTERACT_ROOM((room), (type)))

// fundamental types
#define BUILDING_VNUM(room)  (GET_BUILDING(room) ? GET_BLD_VNUM(GET_BUILDING(room)) : NOTHING)
#define ROOM_TEMPLATE_VNUM(room)  (GET_ROOM_TEMPLATE(room) ? GET_RMT_VNUM(GET_ROOM_TEMPLATE(room)) : NOTHING)

// helpers
#define BLD_FLAGGED(bld, flag)  IS_SET(GET_BLD_FLAGS(bld), (flag))
#define BLD_DESIGNATE_FLAGGED(room, flag)  (GET_BUILDING(HOME_ROOM(room)) && IS_SET(GET_BLD_DESIGNATE_FLAGS(GET_BUILDING(HOME_ROOM(room))), (flag)))
#define CHECK_CHAMELEON(from_room, to_room)  (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_CHAMELEON) && IS_COMPLETE(to_room) && compute_distance(from_room, to_room) >= 2)
#define RMT_FLAGGED(room, flag)  (GET_ROOM_TEMPLATE(room) && IS_SET(GET_RMT_FLAGS(GET_ROOM_TEMPLATE(room)), (flag)))
#define ROOM_AFF_FLAGGED(r, flag)  (IS_SET(ROOM_AFF_FLAGS(r), (flag)))
#define ROOM_BLD_FLAGGED(room, flag)  (GET_BUILDING(room) && IS_SET(GET_BLD_FLAGS(GET_BUILDING(room)), (flag)))
#define ROOM_CROP_FLAGGED(room, flg)  (ROOM_SECT_FLAGGED((room), SECTF_HAS_CROP_DATA) && ROOM_CROP(room) && CROP_FLAGGED(ROOM_CROP(room), (flg)))
#define ROOM_SECT_FLAGGED(room, flg)  SECT_FLAGGED(SECT(room), (flg))
#define SHIFT_CHAR_DIR(ch, room, dir)  SHIFT_DIR((room), confused_dirs[get_north_for_char(ch)][0][(dir)])
#define SHIFT_DIR(room, dir)  real_shift((room), shift_dir[(dir)][0], shift_dir[(dir)][1])

// use 'room_has_function_and_city_ok' instead of HAS_FUNCTION for almost all uses; HAS_FUNCTION does not detect vehicles or check city status
#define HAS_FUNCTION(room, flag)  ((GET_BUILDING(room) && IS_SET(GET_BLD_FUNCTIONS(GET_BUILDING(room)), (flag))) || (GET_ROOM_TEMPLATE(room) && IS_SET(GET_RMT_FUNCTIONS(GET_ROOM_TEMPLATE(room)), (flag))))

// room types
#define CAN_LOOK_OUT(room)  (ROOM_BLD_FLAGGED((room), BLD_LOOK_OUT) || RMT_FLAGGED((room), RMT_LOOK_OUT))
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


/**
* Determine season by room location. This was a short macro but needed coord-
* safety.
*
* @param room_data *room The room to get the current season for.
* @return int A TILESET_ const (default: TILESET_SPRING).
*/
static inline int GET_SEASON(room_data *room) {
	extern byte y_coord_to_season[MAP_HEIGHT];
	int coord;
	
	if (room && (coord = Y_COORD(room)) >= 0 && coord < MAP_HEIGHT) {
		return y_coord_to_season[coord];
	}
	else {
		return TILESET_SPRING;	// default
	}
}


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
#define GET_RMT_SHOP_LOOKUPS(rmt)  ((rmt)->shop_lookups)
#define GET_RMT_SCRIPTS(rmt)  ((rmt)->proto_script)
#define GET_RMT_SUBZONE(rmt)  ((rmt)->subzone)
#define GET_RMT_TEMPERATURE_TYPE(rmt)  ((rmt)->temperature_type)


 //////////////////////////////////////////////////////////////////////////////
//// SECTOR UTILS ////////////////////////////////////////////////////////////

#define GET_SECT_BUILD_FLAGS(sect)  ((sect)->build_flags)
#define GET_SECT_CLIMATE(sect)  ((sect)->climate)
#define GET_SECT_COMMANDS(sect)  ((sect)->commands)
#define GET_SECT_EVOS(sect)  ((sect)->evolution)
#define GET_SECT_EX_DESCS(sect)  ((sect)->ex_description)
#define GET_SECT_FLAGS(sect)  ((sect)->flags)
#define GET_SECT_ICONS(sect)  ((sect)->icons)
#define GET_SECT_INTERACTIONS(sect)  ((sect)->interactions)
#define GET_SECT_MAPOUT(sect)  ((sect)->mapout)
#define GET_SECT_MOVE_LOSS(sect)  ((sect)->movement_loss)
#define GET_SECT_NAME(sect)  ((sect)->name)
#define GET_SECT_NOTES(sect)  ((sect)->notes)
#define GET_SECT_ROADSIDE_ICON(sect)  ((sect)->roadside_icon)
#define GET_SECT_SPAWNS(sect)  ((sect)->spawns)
#define GET_SECT_TITLE(sect)  ((sect)->title)
#define GET_SECT_VNUM(sect)  ((sect)->vnum)

// utils
#define SECT_FLAGGED(sct, flg)  (IS_SET(GET_SECT_FLAGS(sct), (flg)))


 //////////////////////////////////////////////////////////////////////////////
//// SHOP UTILS //////////////////////////////////////////////////////////////

#define SHOP_VNUM(shop)  ((shop)->vnum)
#define SHOP_ALLEGIANCE(shop)  ((shop)->allegiance)
#define SHOP_CLOSE_TIME(shop)  ((shop)->close_time)
#define SHOP_FLAGS(shop)  ((shop)->flags)
#define SHOP_ITEMS(shop)  ((shop)->items)
#define SHOP_LOCATIONS(shop)  ((shop)->locations)
#define SHOP_NAME(shop)  ((shop)->name)
#define SHOP_OPEN_TIME(shop)  ((shop)->open_time)

// helpers
#define SHOP_FLAGGED(shop, flag)  IS_SET(SHOP_FLAGS(shop), (flag))


 //////////////////////////////////////////////////////////////////////////////
//// SKILL UTILS /////////////////////////////////////////////////////////////

#define SKILL_ABBREV(skill)  ((skill)->abbrev)
#define SKILL_ABILITIES(skill)  ((skill)->abilities)
#define SKILL_DESC(skill)  ((skill)->desc)
#define SKILL_FLAGS(skill)  ((skill)->flags)
#define SKILL_MAX_LEVEL(skill)  ((skill)->max_level)
#define SKILL_MIN_DROP_LEVEL(skill)  ((skill)->min_drop_level)
#define SKILL_NAME(skill)  ((skill)->name)
#define SKILL_SYNERGIES(skill)  ((skill)->synergies)
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
#define VEH_BUILT_WITH(veh)  ((veh)->built_with)
#define VEH_CARRYING_N(veh)  ((veh)->carrying_n)
#define VEH_CONSTRUCTION_ID(veh)  ((veh)->construction_id)
#define VEH_CONTAINS(veh)  ((veh)->contains)
#define VEH_DEPLETION(veh)  ((veh)->depletion)
#define VEH_DRIVER(veh)  ((veh)->driver)
#define VEH_EXTRA_DATA(veh)  ((veh)->extra_data)
#define VEH_FLAGS(veh)  ((veh)->flags)
#define VEH_HEALTH(veh)  ((veh)->health)
#define VEH_ICON(veh)  ((veh)->icon)
#define VEH_INSIDE_ROOMS(veh)  ((veh)->inside_rooms)
#define VEH_INSTANCE_ID(veh)  ((veh)->instance_id)
#define VEH_INTERIOR_HOME_ROOM(veh)  ((veh)->interior_home_room)
#define VEH_KEYWORDS(veh)  ((veh)->keywords)
#define VEH_LAST_FIRE_TIME(veh)  ((veh)->last_fire_time)
#define VEH_LAST_MOVE_TIME(veh)  ((veh)->last_move_time)
#define VEH_LED_BY(veh)  ((veh)->led_by)
#define VEH_LONG_DESC(veh)  ((veh)->long_desc)
#define VEH_LOOK_DESC(veh)  ((veh)->look_desc)
#define VEH_NEEDS_RESOURCES(veh)  ((veh)->needs_resources)
#define VEH_OWNER(veh)  ((veh)->owner)
#define VEH_QUEST_LOOKUPS(veh)  ((veh)->quest_lookups)
#define VEH_ROOM_AFFECTS(veh)  ((veh)->room_affects)
#define VEH_ROOM_LIST(veh)  ((veh)->room_list)
#define VEH_SCALE_LEVEL(veh)  ((veh)->scale_level)
#define VEH_SHIPPING_ID(veh)  ((veh)->shipping_id)
#define VEH_SHOP_LOOKUPS(veh)  ((veh)->shop_lookups)
#define VEH_SHORT_DESC(veh)  ((veh)->short_desc)
#define VEH_SITTING_ON(veh)  ((veh)->sitting_on)
#define VEH_VNUM(veh)  ((veh)->vnum)

// attribute (non-instanced) data
#define VEH_ANIMALS_REQUIRED(veh)  ((veh)->attributes->animals_required)
#define VEH_ARTISAN(veh)  NOTHING	// for future use
#define VEH_CAPACITY(veh)  ((veh)->attributes->capacity)
#define VEH_CUSTOM_MSGS(veh)  ((veh)->attributes->custom_msgs)
#define VEH_DESIGNATE_FLAGS(veh)  ((veh)->attributes->designate_flags)
#define VEH_EX_DESCS(veh)  ((veh)->attributes->ex_description)
#define VEH_FAME(veh)  ((veh)->attributes->fame)
#define VEH_FORBID_CLIMATE(veh)  ((veh)->attributes->forbid_climate)
#define VEH_FUNCTIONS(veh)  ((veh)->attributes->functions)
#define VEH_HEIGHT(veh)  ((veh)->attributes->height)
#define VEH_INTERACTIONS(veh)  ((veh)->attributes->interactions)
#define VEH_INTERIOR_ROOM_VNUM(veh)  ((veh)->attributes->interior_room_vnum)
#define VEH_MAX_HEALTH(veh)  ((veh)->attributes->maxhealth)
#define VEH_MAX_ROOMS(veh)  ((veh)->attributes->max_rooms)
#define VEH_MAX_SCALE_LEVEL(veh)  ((veh)->attributes->max_scale_level)
#define VEH_MILITARY(veh)  ((veh)->attributes->military)
#define VEH_MIN_SCALE_LEVEL(veh)  ((veh)->attributes->min_scale_level)
#define VEH_MOVE_TYPE(veh)  ((veh)->attributes->move_type)
#define VEH_RELATIONS(veh)  ((veh)->attributes->relations)
#define VEH_REQUIRES_CLIMATE(veh)  ((veh)->attributes->requires_climate)
#define VEH_YEARLY_MAINTENANCE(veh)  ((veh)->attributes->yearly_maintenance)
#define VEH_SIZE(veh)  ((veh)->attributes->size)
#define VEH_SPAWNS(veh)  ((veh)->attributes->spawns)
#define VEH_SPEED_BONUSES(veh)  ((veh)->attributes->veh_move_speed)

// helpers
#define IN_OR_ON(veh)		(VEH_FLAGGED((veh), VEH_IN) ? "in" : "on")
#define VEH_CLAIMS_WITH_ROOM(veh)  (VEH_FLAGGED((veh), VEH_BUILDING) && !VEH_FLAGGED((veh), MOVABLE_VEH_FLAGS | VEH_NO_CLAIM))
#define VEH_IS_EXTRACTED(veh)  VEH_FLAGGED((veh), VEH_EXTRACTED)
#define VEH_IS_VISIBLE_ON_MAPOUT(veh)  (VEH_FLAGGED((veh), VEH_BUILDING) && VEH_ICON(veh) && VEH_SIZE(veh) > 0 && !VEH_FLAGGED((veh), VEH_CHAMELEON))
#define VEH_FLAGGED(veh, flag)  IS_SET(VEH_FLAGS(veh), (flag))
#define VEH_HAS_MINOR_DISREPAIR(veh)  (VEH_HEALTH(veh) < VEH_MAX_HEALTH(veh) && (VEH_HEALTH(veh) <= (VEH_MAX_HEALTH(veh) * config_get_int("disrepair_minor") / 100)))
#define VEH_HAS_MAJOR_DISREPAIR(veh)  (VEH_HEALTH(veh) < VEH_MAX_HEALTH(veh) && (VEH_HEALTH(veh) <= (VEH_MAX_HEALTH(veh) * config_get_int("disrepair_major") / 100)))
#define VEH_IS_COMPLETE(veh)  (!VEH_NEEDS_RESOURCES(veh) || !VEH_FLAGGED((veh), VEH_INCOMPLETE | VEH_DISMANTLING | VEH_EXTRACTED))
#define VEH_IS_DISMANTLING(veh)  (VEH_FLAGGED((veh), VEH_DISMANTLING) ? TRUE : FALSE)
#define VEH_OR_BLD(veh)  (VEH_FLAGGED((veh), VEH_BUILDING) ? "building" : "vehicle")
#define VEH_PAINT_COLOR(veh)  get_vehicle_extra_data((veh), ROOM_EXTRA_PAINT_COLOR)
#define VEH_PATRON(veh)  get_vehicle_extra_data((veh), ROOM_EXTRA_DEDICATE_ID)
#define VEH_PROVIDES_LIGHT(veh)  (VEH_FLAGGED((veh), VEH_BUILDING) && (VEH_OWNER(veh) || (IN_ROOM(veh) && ROOM_AFF_FLAGGED(IN_ROOM(veh), ROOM_AFF_UNCLAIMABLE))))


 //////////////////////////////////////////////////////////////////////////////
//// CONST EXTERNS ///////////////////////////////////////////////////////////

extern FILE *logfile;	// comm.c
extern struct weather_data weather_info;	// db.c


 //////////////////////////////////////////////////////////////////////////////
//// UTIL FUNCTION PROTOS ////////////////////////////////////////////////////

// used by workforce.c to validate crafts
#define CHORE_GEN_CRAFT_VALIDATOR(name)  bool (name)(empire_data *emp, room_data *room, vehicle_data *veh, int chore, craft_data *craft)

/**
* Validates a location for the pathfinding system.
*
* This must be implemented for both 'room' and 'map', as only 1 of those is set
* each time it's called.
*
* @param room_data *room Optional: An interior room (if this is NULL, map will be set instead).
* @param struct map_data *map Optional: A map room if outdoors (if this is NULL, room will be set instead).
* @param char_data *ch Optional: Player trying to find the path (may be NULL).
* @param vehicle_data *veh Optional: Vehicle trying to find the paath (may be NULL).
* @param struct pathfind_controller *controller The pathfinding controller and all its data.
* @return bool TRUE if the room/map is ok, FALSE if not.
*/
#define PATHFIND_VALIDATOR(name)  bool (name)(room_data *room, struct map_data *map, char_data *ch, vehicle_data *veh, struct pathfind_controller *controller)

// for the easy-update system
#define PLAYER_UPDATE_FUNC(name)  void (name)(char_data *ch, bool is_file)

// log information
#define log  basic_mud_log

// string/vnum hash tools from utils.c
void add_pair_hash(struct pair_hash **hash, int id, int value);
void add_string_hash(struct string_hash **hash, const char *string, int count);
void add_vnum_hash(struct vnum_hash **hash, any_vnum vnum, int count);
struct pair_hash *find_in_pair_hash(struct pair_hash *hash, int id);
struct string_hash *find_in_string_hash(struct string_hash *hash, const char *string);
struct vnum_hash *find_in_vnum_hash(struct vnum_hash *hash, any_vnum vnum);
void free_pair_hash(struct pair_hash **hash);
void free_string_hash(struct string_hash **hash);
void free_vnum_hash(struct vnum_hash **hash);
int sort_string_hash(struct string_hash *a, struct string_hash *b);
void string_hash_to_string(struct string_hash *str_hash, char *to_string, size_t string_size, bool show_count, bool use_commas, bool use_and);

// adventure functions from utils.c
adv_data *get_adventure_for_vnum(rmt_vnum vnum);

// apply/attribute functions from utils.c
int get_attribute_by_apply(char_data *ch, int apply_type);
int get_attribute_by_name(char *name);

// basic functions from utils.c
bool any_players_in_room(room_data *room);
const char *PERS(char_data *ch, char_data *vict, bool real);
double diminishing_returns(double val, double scale);
int att_max(char_data *ch);
int count_bits(bitvector_t bitset);
int dice(int number, int size);
int number(int from, int to);
struct time_info_data *age(char_data *ch);
struct time_info_data *mud_time_passed(time_t t2, time_t t1);
struct time_info_data *real_time_passed(time_t t2, time_t t1);

// building functions from utils.c
bld_data *get_building_by_name(char *name, bool room_only);

// character functions from utils.c
void change_keywords(char_data *ch, char *str);
void change_long_desc(char_data *ch, char *str);
void change_look_desc(char_data *ch, char *str, bool format);
void change_look_desc_append(char_data *ch, char *str, bool format);
void change_sex(char_data *ch, int sex);
void change_short_desc(char_data *ch, char *str);
bool circle_follow(char_data *ch, char_data *victim);

// component functions from utils.c
generic_data *find_generic_component(char *name);
bool is_basic_component(obj_data *obj);
bool is_component(obj_data *obj, generic_data *cmp);
#define is_component_vnum(obj, vnum)  is_component((obj), real_generic(vnum))

// crop functions from utils.c
crop_data *get_crop_by_name(char *name);

// delayed refresh functions from utils.c
void run_delayed_refresh();

// empire utils from utils.c
bool can_claim(char_data *ch);
int count_members_online(empire_data *emp);
int count_tech(empire_data *emp);
bool empire_can_claim(empire_data *emp);
int get_total_score(empire_data *emp);
bool ignore_distrustful_due_to_start_loc(room_data *loc);
bool is_trading_with(empire_data *emp, empire_data *partner);
void process_imports();
void resort_empires(bool force);

// empire diplomacy utils from utils.c
bool char_has_relationship(char_data *ch_a, char_data *ch_b, bitvector_t dipl_bits);
bool empire_is_friendly(empire_data *emp, empire_data *enemy);
bool empire_is_hostile(empire_data *emp, empire_data *enemy, room_data *loc);
bool has_empire_trait(empire_data *emp, room_data *loc, bitvector_t trait);
bool has_relationship(empire_data *emp, empire_data *fremp, bitvector_t diplomacy);

// interpreter utils from utils.c
char *any_one_arg(char *argument, char *first_arg);
char *any_one_word(char *argument, char *first_arg);
void chop_last_arg(char *string, char *most_args, char *last_arg);
void comma_args(char *string, char *arg1, char *arg2);
int fill_word(char *argument);
void half_chop(char *string, char *arg1, char *arg2);
int is_abbrev(const char *arg1, const char *arg2);
bool is_multiword_abbrev(const char *arg, const char *phrase);
int is_number(const char *str);
char *one_argument(char *argument, char *first_arg);
char *one_word(char *argument, char *first_arg);
int reserved_word(char *argument);
int search_block(char *arg, const char **list, int exact);
void skip_spaces(char **string);
char *two_arguments(char *argument, char *first_arg, char *second_arg);
void ucwords(char *string);

// permission utils from utils.c
bool can_build_or_claim_at_war(char_data *ch, room_data *loc);
bool can_use_room(char_data *ch, room_data *room, int mode);
bool emp_can_use_room(empire_data *emp, room_data *room, int mode);
bool emp_can_use_vehicle(empire_data *emp, vehicle_data *veh, int mode);
#define can_use_vehicle(ch, veh, mode)  (IS_IMMORTAL(ch) || (emp_can_use_vehicle(GET_LOYALTY(ch), (veh), (mode)) && (!VEH_INTERIOR_HOME_ROOM(veh) || can_use_room((ch), VEH_INTERIOR_HOME_ROOM(veh), (mode)))))
bool has_permission(char_data *ch, int type, room_data *loc);
bool has_tech_available(char_data *ch, int tech);
bool has_tech_available_room(room_data *room, int tech);
bool is_at_war(empire_data *emp);
int land_can_claim(empire_data *emp, int ter_type);

// portal utils from utils.c
obj_data *find_portal_in_room_targetting(room_data *room, room_vnum to_room);

// file utilities from utils.c
int get_filename(char *orig_name, char *filename, int mode);
int get_line(FILE *fl, char *buf);
int touch(const char *path);

// logging functions from utils.c
char *room_log_identifier(room_data *room);
void basic_mud_log(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
void log_to_empire(empire_data *emp, int type, const char *str, ...) __attribute__((format(printf, 3, 4)));
void mortlog(const char *str, ...) __attribute__((format(printf, 1, 2)));
void syslog(bitvector_t type, int level, bool file, const char *str, ...) __attribute__((format(printf, 4, 5)));

// mobile functions from utils.c
char *get_mob_name_by_proto(mob_vnum vnum, bool replace_placeholders);

// object functions from utils.c
int count_objs_by_vnum(obj_vnum vnum, obj_data *list);
char *get_obj_name_by_proto(obj_vnum vnum);
obj_data *get_top_object(obj_data *obj);
double rate_item(obj_data *obj);

// player functions from utils.c
void apply_bonus_trait(char_data *ch, bitvector_t trait, bool add);
bool can_see_in_dark_room(char_data *ch, room_data *room, bool count_adjacent_light);
void command_lag(char_data *ch, int wait_type);
void despawn_charmies(char_data *ch, any_vnum only_vnum);
void determine_gear_level(char_data *ch);
room_data *find_load_room(char_data *ch);
room_data *find_starting_location();
int get_view_height(char_data *ch, room_data *from_room);
bool has_one_day_playtime(char_data *ch);
int num_earned_bonus_traits(char_data *ch);
int pick_level_from_range(int level, int min, int max);
void relocate_players(room_data *room, room_data *to_room);
void schedule_check_leading_event(char_data *ch);
void update_all_players(char_data *to_message, PLAYER_UPDATE_FUNC(*func));
bool wake_and_stand(char_data *ch);

// resource functions from utils.c
void add_to_resource_list(struct resource_data **list, int type, any_vnum vnum, int amount, int misc);
void apply_resource(char_data *ch, struct resource_data *res, struct resource_data **list, obj_data *use_obj, int msg_type, vehicle_data *crafting_veh, struct resource_data **build_used_list);
void extract_resources(char_data *ch, struct resource_data *list, bool ground, struct resource_data **build_used_list);
struct resource_data *get_next_resource(char_data *ch, struct resource_data *list, bool ground, bool left2right, obj_data **found_obj);
char *get_resource_name(struct resource_data *res);
void give_resources(char_data *ch, struct resource_data *list, bool split);
bool has_resources(char_data *ch, struct resource_data *list, bool ground, bool send_msgs, char *msg_prefix);
void reduce_dismantle_resources(int damage, int max_health, struct resource_data **list);
void show_resource_list(struct resource_data *list, char *save_buffer);

// sector functions from utils.c
sector_data *get_sect_by_name(char *name);

// string functions from utils.c
bitvector_t asciiflag_conv(char *flag);
char *bitv_to_alpha(bitvector_t flags);
char *delete_doubledollar(char *string);
const char *double_percents(const char *string);
bool has_keyword(char *string, const char *list[], bool exact);
bool isname(const char *str, const char *namelist);
char *level_range_string(int min, int max, int current);
bool multi_isname(const char *arg, const char *namelist);
char *CAP(char *txt);
char *fname(const char *namelist);
void replace_question_color(char *input, char *color, char *output);
char *reverse_strstr(char *haystack, char *needle);
bool search_custom_messages(char *keywords, struct custom_message *list);
bool search_extra_descs(char *keywords, struct extra_descr_data *list);
char *shared_by(obj_data *obj, char_data *ch);
char *show_color_codes(char *string);
char *str_dup(const char *source);
char *str_replace(const char *search, const char *replace, const char *subject);
char *str_str(const char *cs, const char *ct);
char *strip_color(char *input);
char *stripcr(char *dest, const char *src);
void strip_crlf(char *buffer);
char *strtolower(char *str);
char *strtoupper(char *str);
int color_code_length(const char *str);
#define color_strlen(str)  (strlen(str) - color_code_length(str))
int count_icon_codes(char *string);
bool strchrstr(const char *haystack, const char *needles);
int str_cmp(const char *arg1, const char *arg2);
int strn_cmp(const char *arg1, const char *arg2, int n);
void ordered_sprintbit(bitvector_t bitvector, const char *names[], const bitvector_t order[], bool commas, char *result);
void prettier_sprintbit(bitvector_t bitvector, const char *names[], char *result);
void prune_crlf(char *txt);
const char *skip_filler(const char *string);
void sprintbit(bitvector_t vektor, const char *names[], char *result, bool space);
void sprinttype(int type, const char *names[], char *result, size_t max_result_size, char *error_value);
char *time_length_string(int seconds);
char *trim(char *string);

// world functions in utils.c
bool check_sunny(room_data *room);
char *coord_display(char_data *ch, int x, int y, bool fixed_width);
#define coord_display_room(ch, room, fixed_width)  coord_display((ch), (room && !NO_LOCATION(room)) ? X_COORD(room) : NOWHERE, (room && !NO_LOCATION(room)) ? Y_COORD(room) : NOWHERE, (fixed_width))
int count_flagged_sect_between(bitvector_t sectf_bits, room_data *start, room_data *end, bool check_base_sect);
double compute_map_distance(int x1, int y1, int x2, int y2);
#define compute_distance(from, to)  ((int)compute_map_distance(X_COORD(from), Y_COORD(from), X_COORD(to), Y_COORD(to)))
int count_adjacent_sectors(room_data *room, sector_vnum sect, bool count_original_sect);
int distance_to_nearest_player(room_data *room);
bool find_flagged_sect_within_distance_from_char(char_data *ch, bitvector_t with_flags, bitvector_t without_flags, int distance);
bool find_flagged_sect_within_distance_from_room(room_data *room, bitvector_t with_flags, bitvector_t without_flags, int distance);
room_data *find_other_starting_location(room_data *current_room);
bool find_sect_within_distance_from_char(char_data *ch, sector_vnum sect, int distance);
bool find_sect_within_distance_from_room(room_data *room, sector_vnum sect, int distance);
bitvector_t get_climate(room_data *room);
bool get_coord_shift(int start_x, int start_y, int x_shift, int y_shift, int *new_x, int *new_y);
int get_direction_to(room_data *from, room_data *to);
room_data *get_map_location_for(room_data *room);
char *get_partial_direction_to(char_data *ch, room_data *from, room_data *to, bool abbrev);
int get_room_blocking_height(room_data *room, bool *blocking_vehicle);
bool is_deep_mine(room_data *room);
void lock_icon(room_data *room, struct icon_data *use_icon);
void lock_icon_map(struct map_data *loc, struct icon_data *use_icon);
room_data *real_shift(room_data *origin, int x_shift, int y_shift);
bool room_is_light(room_data *room, bool count_adjacent_light);
room_data *straight_line(room_data *origin, room_data *destination, int iter);
sector_data *find_first_matching_sector(bitvector_t with_flags, bitvector_t without_flags, bitvector_t prefer_climate);

// misc functions from utils.c
char *simple_time_since(time_t when);
unsigned long long microtime(void);
bool room_has_function_and_city_ok(empire_data *for_emp, room_data *room, bitvector_t fnc_flag);
bool vehicle_has_function_and_city_ok(vehicle_data *veh, bitvector_t fnc_flag);


// abilities.c
void ability_fail_message(char_data *ch, char_data *vict, ability_data *abil);
void add_ability_gain_hook(char_data *ch, ability_data *abil);
void apply_ability_techs_to_player(char_data *ch, ability_data *abil);
void apply_one_passive_buff(char_data *ch, ability_data *abil);
bool check_ability(char_data *ch, char *string, bool exact);
ability_data *find_ability_on_skill(char *name, skill_data *skill);
ability_data *find_player_ability_by_tech(char_data *ch, int ptech);
void get_ability_type_display(struct ability_type *list, char *save_buffer, bool for_players);
int get_player_level_for_ability(char_data *ch, any_vnum abil_vnum);
bool is_class_ability(ability_data *abil);
char_data *load_companion_mob(char_data *leader, struct companion_data *cd);
void pre_ability_message(char_data *ch, char_data *vict, ability_data *abil);
void read_ability_requirements();
void refresh_passive_buffs(char_data *ch);
void remove_passive_buff(char_data *ch, struct affected_type *aff);
void remove_passive_buff_by_ability(char_data *ch, any_vnum abil);
void run_ability_gain_hooks(char_data *ch, char_data *opponent, bitvector_t trigger);
void setup_ability_companions(char_data *ch);

// act.action.c
bool action_flagged(char_data *ch, bitvector_t actf);
void cancel_action(char_data *ch);
void do_burn_area(char_data *ch);
obj_data *has_tool(char_data *ch, bitvector_t flags);
obj_data *has_all_tools(char_data *ch, bitvector_t flags);
void process_build_action(char_data *ch);
void start_action(char_data *ch, int type, int timer);
void stop_room_action(room_data *room, int action);

// act.battle.c
void perform_rescue(char_data *ch, char_data *vict, char_data *from, int msg);

// act.comm.c
void add_to_channel_history(char_data *ch, int type, char_data *speaker, char *message);
struct slash_channel *create_slash_channel(char *name);
struct player_slash_channel *find_on_slash_channel(char_data *ch, int id);
struct slash_channel *find_slash_channel_by_id(int id);
struct slash_channel *find_slash_channel_by_name(char *name, bool exact);
bool is_ignoring(char_data *ch, char_data *victim);
void log_to_slash_channel_by_name(char *chan_name, char_data *ignorable_person, const char *messg, ...);

// act.empire.c
bool check_in_city_requirement(room_data *room, bool check_wait);
void do_burn_building(char_data *ch, room_data *room, obj_data *lighter);
void do_customize_island(char_data *ch, char *argument);
int get_territory_type_for_empire(room_data *loc, empire_data *emp, bool check_wait, bool *city_too_soon, bool *using_large_radius);
#define is_in_city_for_empire(loc, emp, check_wait, city_too_soon)  (get_territory_type_for_empire((loc), (emp), (check_wait), (city_too_soon), NULL) == TER_CITY)	// backwards-compatibility
void perform_abandon_city(empire_data *emp, struct empire_city_data *city, bool full_abandon);
void scan_for_tile(char_data *ch, char *argument, int max_dist, bitvector_t only_in_dirs);
void set_workforce_limit(empire_data *emp, int island_id, int chore, int limit);
void set_workforce_limit_all(empire_data *emp, int chore, int limit);
void show_workforce_setup_to_char(empire_data *emp, char_data *ch);

// act.highsorcery.c
double get_enchant_scale_for_char(char_data *ch, int max_scale);
void summon_materials(char_data *ch, char *argument);

// act.immortal.c
void perform_autostore(obj_data *obj, empire_data *emp, int island);
void perform_immort_vis(char_data *ch);
void show_spawn_summary_to_char(char_data *ch, struct spawn_info *list);

// act.informative.c
void diag_char_to_char(char_data *i, char_data *ch);
void display_attributes(char_data *ch, char_data *to);
void display_tip_to_char(char_data *ch);
char *find_exdesc_for_char(char_data *ch, char *word, int *number, obj_data **found_obj, vehicle_data **found_veh, room_data **found_room);
char *get_obj_desc(obj_data *obj, char_data *ch, int mode);
void get_player_skill_string(char_data *ch, char *buffer, bool abbrev);
void list_char_to_char(char_data *list, char_data *ch);
void list_obj_to_char(obj_data *list, char_data *ch, int mode, int show);
void list_vehicles_to_char(vehicle_data *list, char_data *ch, bool large_only, vehicle_data *exclude);
char *obj_color_by_quality(obj_data *obj, char_data *ch);
char *obj_desc_for_char(obj_data *obj, char_data *ch, int mode);
struct custom_message *pick_custom_longdesc(char_data *ch);
void show_character_affects(char_data *ch, char_data *to);
bool show_local_einv(char_data *ch, room_data *room, bool thief_mode);

// act.item.c
bool can_take_obj(char_data *ch, obj_data *obj);
int count_objs_in_room(room_data *room);
void deliver_shipment(empire_data *emp, struct shipping_data *shipd);
bool douse_light(obj_data *obj);
room_data *find_docks(empire_data *emp, int island_id);
int find_free_shipping_id(empire_data *emp);
obj_data *find_lighter_in_list(obj_data *list, bool *had_keep);
bool get_check_money(char_data *ch, obj_data *obj);
void identify_obj_to_char(obj_data *obj, char_data *ch, bool simple);
int obj_carry_size(obj_data *obj);
void process_shipping();
void remove_armor_by_type(char_data *ch, int armor_type);
void remove_honed_gear(char_data *ch);
void sail_shipment(empire_data *emp, vehicle_data *boat);
void scale_item_to_level(obj_data *obj, int level);
bool ship_is_empty(vehicle_data *ship);
bool use_hour_of_light(obj_data *obj, bool messages);
bool used_lighter(char_data *ch, obj_data *obj);

// act.movement.c
bool can_enter_portal(char_data *ch, obj_data *portal, bool allow_infiltrate, bool skip_permissions);
void char_through_portal(char_data *ch, obj_data *portal, bool following);
bool check_stop_flying(char_data *ch);
void clear_recent_moves(char_data *ch);
int count_recent_moves(char_data *ch);
obj_data *find_back_portal(room_data *in_room, room_data *from_room, obj_data *fallback);
room_data *get_exit_room(room_data *from_room);
int get_north_for_char(char_data *ch);
void give_portal_sickness(char_data *ch, obj_data *portal, room_data *from, room_data *to);
bool parse_next_dir_from_string(char_data *ch, char *string, int *dir, int *dist, bool send_error);
int perform_move(char_data *ch, int dir, room_data *to_room, bitvector_t flags);
void skip_run_filler(char **string);
bool validate_vehicle_move(char_data *ch, vehicle_data *veh, room_data *to_room);

// act.naturalmagic.c
bool despawn_companion(char_data *ch, mob_vnum vnum);
int total_bonus_healing(char_data *ch);
void un_earthmeld(char_data *ch);

// act.other.c
void adventure_summon(char_data *ch, char *argument);
void adventure_unsummon(char_data *ch);
void cancel_adventure_summon(char_data *ch);
bool dismiss_any_minipet(char_data *ch);
void do_douse_room(char_data *ch, room_data *room, obj_data *cont);

// act.quest.c
const char *color_by_difficulty(char_data *ch, int level);
void count_quest_tasks(struct req_data *list, int *complete, int *total);
void drop_quest(char_data *ch, struct player_quest *pq);
bool fail_daily_quests(char_data *ch, bool event);
struct instance_data *find_matching_instance_for_shared_quest(char_data *ch, any_vnum quest_vnum);
void show_quest_tracker(char_data *ch, struct player_quest *pq);
void start_quest(char_data *ch, quest_data *qst, struct instance_data *inst);

// act.social.c
bool check_social(char_data *ch, char *string, bool exact);
social_data *find_social(char_data *ch, char *name, bool exact);
void perform_social(char_data *ch, social_data *soc, char *argument);

// act.stealth.c
int apply_poison(char_data *ch, char_data *vict);
bool can_infiltrate(char_data *ch, empire_data *emp);
bool can_steal(char_data *ch, empire_data *emp);
obj_data *find_poison_by_vnum(obj_data *list, any_vnum vnum);
void trigger_distrust_from_stealth(char_data *ch, empire_data *emp);
void undisguise(char_data *ch);
bool valid_unseen_passing(room_data *room);

// act.survival.c
bool valid_no_trace(room_data *room);

// act.trade.c
void cancel_gen_craft(char_data *ch);
bool check_can_craft(char_data *ch, craft_data *type, bool continuing);
bool find_and_bind(char_data *ch, obj_vnum vnum);
int get_craft_scale_level(char_data *ch, craft_data *craft);
int get_crafting_level(char_data *ch);
obj_data *has_required_obj_for_craft(char_data *ch, obj_vnum vnum);

// act.vampire.c
bool cancel_biting(char_data *ch);
void cancel_blood_upkeeps(char_data *ch);
bool check_blood_fortitude(char_data *ch, bool can_gain_skill);
void check_un_vampire(char_data *ch, bool remove_vampire_skills);
bool check_vampire_sun(char_data *ch, bool message);
void make_vampire(char_data *ch, bool lore, any_vnum skill_vnum);
void retract_claws(char_data *ch);
void sire_char(char_data *ch, char_data *victim);
void start_drinking_blood(char_data *ch, char_data *victim);
bool starving_vampire_aggro(char_data *ch);
void taste_blood(char_data *ch, char_data *vict);
void un_deathshroud(char_data *ch);
void un_mummify(char_data *ch);
void update_biting_char(char_data *ch);
void update_vampire_sun(char_data *ch);
bool vampire_kill_feeding_target(char_data *ch, char *argument);

// act.vehicles.c
void do_customize_vehicle(char_data *ch, char *argument);
void do_douse_vehicle(char_data *ch, vehicle_data *veh, obj_data *cont);
void do_light_vehicle(char_data *ch, vehicle_data *veh, obj_data *flint);
void do_sit_on_vehicle(char_data *ch, char *argument, int pos);
void do_unseat_from_vehicle(char_data *ch);
bool find_siege_target_for_vehicle(char_data *ch, vehicle_data *veh, char *arg, room_data **room_targ, int *dir, vehicle_data **veh_targ);
vehicle_data *get_current_piloted_vehicle(char_data *ch);
room_data *get_shipping_target(char_data *ch, char *argument, bool *targeted_island);
bool move_vehicle(char_data *ch, vehicle_data *veh, int dir, int subcmd);
bool validate_sit_on_vehicle(char_data *ch, vehicle_data *veh, int pos, bool message);

// augments.c
bool validate_augment_target(char_data *ch, obj_data *obj, augment_data *aug, bool send_messages);

// ban.c
int isbanned(char *hostname);

// bookedit.c
void olc_show_book(char_data *ch);

// books.c
void add_book_author(int idnum);
void read_book(char_data *ch, obj_data *obj);
void save_author_books(int idnum);
void save_author_index();

// building.c
bool can_build_on(room_data *room, bitvector_t flags);
bool check_build_location_and_dir(char_data *ch, room_data *room, craft_data *type, int dir, bool is_upgrade, bool *bld_is_closed, bool *bld_needs_reverse);
struct resource_data *combine_resources(struct resource_data *combine_a, struct resource_data *combine_b);
void complete_building(room_data *room);
void do_customize_room(char_data *ch, char *argument);
craft_data *find_building_list_entry(room_data *room, byte type);
void finish_building(char_data *ch, room_data *room);
void finish_dismantle(char_data *ch, room_data *room);
void finish_maintenance(char_data *ch, room_data *room);
void herd_animals_out(room_data *location);
bool is_entrance(room_data *room);
void process_build(char_data *ch, room_data *room, int act_type);
void process_dismantling(char_data *ch, room_data *room);
void perform_force_upgrades();
void remove_like_component_from_built_with(struct resource_data **built_with, any_vnum component);
void remove_like_item_from_built_with(struct resource_data **built_with, obj_data *obj);
void special_building_setup(char_data *ch, room_data *room);
void special_vehicle_setup(char_data *ch, vehicle_data *veh);
void start_dismantle_building(room_data *loc);

// dg_wldcmd.c
int get_room_scale_level(room_data *room, char_data *targ);

// eedit.c
bool check_banner_color_string(char *str, bool allow_neutral_color, bool allow_underline);
bool check_unique_empire_name(empire_data *for_emp, char *name);
char empire_banner_to_mapout_token(const char *banner);
bool valid_rank_name(char_data *ch, char *newname);

// event.c
void delete_player_from_running_events(char_data *ch);
int gain_event_points(char_data *ch, any_vnum event_vnum, int points);
struct player_event_data *get_event_data(char_data *ch, int event_id);
bool has_uncollected_event_rewards(char_data *ch);
struct event_running_data *only_one_running_event(int *count);

// faction.c
const char *get_faction_name_by_vnum(any_vnum vnum);
const char *get_reputation_name(int type);
void update_reputations(char_data *ch);

// fight.c
void besiege_room(char_data *attacker, room_data *to_room, int damage, vehicle_data *by_vehicle);
bool besiege_vehicle(char_data *attacker, vehicle_data *veh, int damage, int siege_type, vehicle_data *by_vehicle);
void check_combat_end(char_data *ch);
void check_combat_start(char_data *ch);
bool check_hit_vs_dodge(char_data *attacker, char_data *victim, bool off_hand);
void death_log(char_data *ch, char_data *killer, int type);
void death_restore(char_data *ch);
double get_base_dps(obj_data *weapon);
int get_block_rating(char_data *ch, bool can_gain_skill);
double get_combat_speed(char_data *ch, int pos);
int get_dodge_modifier(char_data *ch, char_data *attacker, bool can_gain_skill);
int get_to_hit(char_data *ch, char_data *victim, bool off_hand, bool can_gain_skill);
time_t get_last_killed_by_empire(char_data *ch, empire_data *emp);
double get_weapon_speed(obj_data *weapon);
bool is_fight_ally(char_data *ch, char_data *frenemy);
bool is_fight_enemy(char_data *ch, char_data *frenemy);
void out_of_blood(char_data *ch);
void perform_execute(char_data *ch, char_data *victim, int attacktype, int damtype);
void perform_resurrection(char_data *ch, char_data *rez_by, room_data *loc, any_vnum ability);
obj_data *player_death(char_data *ch);
int reduce_damage_from_skills(int dam, char_data *victim, char_data *attacker, int damtype);
void reset_combat_meters(char_data *ch);
void trigger_distrust_from_hostile(char_data *ch, empire_data *emp);
bool validate_siege_target_room(char_data *ch, vehicle_data *veh, room_data *to_room);
bool validate_siege_target_vehicle(char_data *ch, vehicle_data *veh, vehicle_data *target);

// fight.c combat meters
void combat_meter_damage_dealt(char_data *ch, int amt);
void combat_meter_damage_taken(char_data *ch, int amt);
void combat_meter_heal_dealt(char_data *ch, int amt);
void combat_meter_heal_taken(char_data *ch, int amt);

// generic.c
bool has_generic_relation(struct generic_relation *list, any_vnum vnum);
bool liquid_flagged(any_vnum generic_liquid_vnum, bitvector_t flag);

// instance.c
int adjusted_instance_limit(adv_data *adv);
bool can_enter_instance(char_data *ch, struct instance_data *inst);
bool can_instance(adv_data *adv);
void check_instance_is_loaded(struct instance_data *inst);
int count_instances(adv_data *adv);
int delete_all_instances(adv_data *adv);
void delete_instance(struct instance_data *inst, bool run_cleanup);
void empty_instance_vehicle(struct instance_data *inst, vehicle_data *veh, room_data *to_room);
room_data *find_nearest_adventure(room_data *from, rmt_vnum vnum);
room_data *find_nearest_rmt(room_data *from, rmt_vnum vnum);
room_data *find_room_template_in_instance(struct instance_data *inst, rmt_vnum vnum);
void generate_adventure_instances();
struct instance_data *get_instance_by_mob(char_data *mob);
struct instance_data *get_instance_for_script(int go_type, void *go);
void get_scale_constraints(room_data *room, char_data *mob, int *scale_level, int *min, int *max);
void instance_obj_setup(struct instance_data *inst, obj_data *obj);
int lock_instance_level(room_data *room, int level);
void mark_instance_completed(struct instance_data *inst);
void prune_instances();
void remove_instance_fake_loc(struct instance_data *inst);
void reset_instance(struct instance_data *inst);
bool same_subzone(room_data *a, room_data *b);
void scale_instance_to_level(struct instance_data *inst, int level);
void set_instance_fake_loc(struct instance_data *inst, room_data *loc);
void unlink_instance_entrance(room_data *room, struct instance_data *inst, bool run_cleanup);

// limits.c
bool can_teleport_to(char_data *ch, room_data *loc, bool check_owner);
bool check_autostore(obj_data *obj, bool force, empire_data *override_emp);
void check_daily_cycle_reset(char_data *ch);
void check_pointless_fight(char_data *mob);
void check_ruined_cities();
void gain_condition(char_data *ch, int condition, int value);
int health_gain(char_data *ch, bool info_only);
int mana_gain(char_data *ch, bool info_only);
int move_gain(char_data *ch, bool info_only);
void schedule_all_obj_timers(char_data *ch);
void schedule_heal_over_time(char_data *ch);
void schedule_obj_autostore_check(obj_data *obj, long new_autostore_timer);
void schedule_obj_timer_update(obj_data *obj, bool override);
void update_empire_needs(empire_data *emp, struct empire_island *eisle, struct empire_needs *needs);

// mapview.c
bool adjacent_room_is_light(room_data *room);
int distance_can_see_in_dark(char_data *ch);
struct icon_data *get_icon_from_set(struct icon_data *set, int type);
int get_map_radius(char_data *ch);
char *get_mine_type_name(room_data *room);
char *get_room_description(room_data *room);
char *get_room_name(room_data *room, bool color);
void look_at_room_by_loc(char_data *ch, room_data *room, bitvector_t options);
#define look_at_room(ch)  look_at_room_by_loc((ch), IN_ROOM(ch), NOBITS)
void look_in_direction(char_data *ch, int dir);

#define HOLYLIGHT_OR_TILE_OWNER(ch, room)  (PRF_FLAGGED((ch), PRF_HOLYLIGHT) || !ROOM_OWNER(room) || (GET_LOYALTY(ch) && GET_LOYALTY(ch) == ROOM_OWNER(room)))
#define HOLYLIGHT_OR_VEH_OWNER(ch, veh)  (PRF_FLAGGED((ch), PRF_HOLYLIGHT) || !VEH_OWNER(veh) || (GET_LOYALTY(ch) && GET_LOYALTY(ch) == VEH_OWNER(veh)))

void get_informative_string(char_data *ch, char *buffer, bool dismantling, bool unfinished, bool major_disrepair, bool minor_disrepair, int mine_view, bool public, bool no_work, bool no_abandon, bool no_dismantle, bool chameleon);
#define INFORMATIVE_MINE_VALUE(ch, room)  ((get_room_extra_data((room), ROOM_EXTRA_MINE_GLB_VNUM) > 0) ? ((PRF_FLAGGED((ch), PRF_HOLYLIGHT) || room_has_function_and_city_ok(GET_LOYALTY(ch), (room), FNC_MINE) || (GET_LOYALTY(ch) && get_room_extra_data((room), ROOM_EXTRA_PROSPECT_EMPIRE) == EMPIRE_VNUM(GET_LOYALTY(ch)))) ? (get_room_extra_data((room), ROOM_EXTRA_MINE_AMOUNT) > 0 ? 1 : -1) : 0) : 0)
#define get_informative_tile_string(ch, room, buffer)  get_informative_string((ch), (buffer), (HOLYLIGHT_OR_TILE_OWNER((ch), (room)) && IS_DISMANTLING(room)) ? TRUE : FALSE, HOLYLIGHT_OR_TILE_OWNER((ch), (room)) ? !IS_COMPLETE(room) : FALSE, HAS_MAJOR_DISREPAIR(room), HAS_MINOR_DISREPAIR(room), INFORMATIVE_MINE_VALUE(ch, room), ROOM_AFF_FLAGGED((room), ROOM_AFF_PUBLIC) ? TRUE : FALSE, (HOLYLIGHT_OR_TILE_OWNER((ch), (room)) && ROOM_AFF_FLAGGED((room), ROOM_AFF_NO_WORK)) ? TRUE : FALSE, (HOLYLIGHT_OR_TILE_OWNER((ch), (room)) && ROOM_AFF_FLAGGED((room), ROOM_AFF_NO_ABANDON)) ? TRUE : FALSE, (HOLYLIGHT_OR_TILE_OWNER((ch), (room)) && ROOM_AFF_FLAGGED((room), ROOM_AFF_NO_DISMANTLE)) ? TRUE : FALSE, ((ch) && IS_IMMORTAL(ch) && ROOM_AFF_FLAGGED((room), ROOM_AFF_CHAMELEON) && IS_COMPLETE(room)))
#define get_informative_vehicle_string(ch, veh, buffer)  get_informative_string((ch), (buffer), (HOLYLIGHT_OR_VEH_OWNER((ch), (veh)) && VEH_IS_DISMANTLING(veh)) ? TRUE : FALSE, HOLYLIGHT_OR_VEH_OWNER((ch), (veh)) ? (!VEH_IS_COMPLETE(veh) && !VEH_IS_DISMANTLING(veh)) : FALSE, VEH_HAS_MAJOR_DISREPAIR(veh), VEH_HAS_MINOR_DISREPAIR(veh), INFORMATIVE_MINE_VALUE(ch, IN_ROOM(veh)), FALSE, (HOLYLIGHT_OR_VEH_OWNER((ch), (veh)) && VEH_FLAGGED((veh), VEH_PLAYER_NO_WORK)) || (VEH_CLAIMS_WITH_ROOM(veh) && HOLYLIGHT_OR_TILE_OWNER((ch), IN_ROOM(veh)) && ROOM_AFF_FLAGGED(IN_ROOM(veh), ROOM_AFF_NO_WORK)), FALSE /*HOLYLIGHT_OR_VEH_OWNER((ch), (veh)) && VEH_FLAGGED((veh), VEH_PLAYER_NO_ABANDON)*/, (HOLYLIGHT_OR_VEH_OWNER((ch), (veh)) && VEH_FLAGGED((veh), VEH_PLAYER_NO_DISMANTLE)) ? TRUE : FALSE, (ch) && IS_IMMORTAL(ch) && (ROOM_AFF_FLAGGED(IN_ROOM(veh), ROOM_AFF_CHAMELEON) || VEH_FLAGGED(veh, VEH_CHAMELEON)) && IS_COMPLETE(IN_ROOM(veh)))

char *get_informative_color(char_data *ch, bool dismantling, bool unfinished, bool major_disrepair, bool minor_disrepair, int mine_view, bool public, bool no_work, bool no_abandon, bool no_dismantle, bool chameleon);	
#define simple_distance(x, y, a, b)		((x - a) * (x - a) + (y - b) * (y - b))
#define get_informative_color_room(ch, room)  get_informative_color((ch), (HOLYLIGHT_OR_TILE_OWNER((ch), (room)) && IS_DISMANTLING(room)) ? TRUE : FALSE, HOLYLIGHT_OR_TILE_OWNER((ch), (room)) ? !IS_COMPLETE(room) : FALSE, HAS_MAJOR_DISREPAIR(room), HAS_MINOR_DISREPAIR(room), INFORMATIVE_MINE_VALUE(ch, room), ROOM_AFF_FLAGGED(room, ROOM_AFF_PUBLIC) ? TRUE : FALSE, (HOLYLIGHT_OR_TILE_OWNER((ch), (room)) && ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_WORK)) ? TRUE : FALSE, (HOLYLIGHT_OR_TILE_OWNER((ch), (room)) && ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_ABANDON)) ? TRUE : FALSE, (HOLYLIGHT_OR_TILE_OWNER((ch), (room)) && ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_DISMANTLE)) ? TRUE : FALSE, ch && IS_IMMORTAL(ch) && ROOM_AFF_FLAGGED(room, ROOM_AFF_CHAMELEON) && IS_COMPLETE(room) && simple_distance(X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)), X_COORD(room), Y_COORD(room)) > 2)
#define get_informative_color_veh(ch, veh)  get_informative_color((ch), (HOLYLIGHT_OR_VEH_OWNER((ch), (veh)) && VEH_IS_DISMANTLING(veh)) ? TRUE : FALSE, (HOLYLIGHT_OR_VEH_OWNER((ch), (veh)) && !VEH_IS_COMPLETE(veh) && !VEH_IS_DISMANTLING(veh)), VEH_HAS_MAJOR_DISREPAIR(veh), VEH_HAS_MINOR_DISREPAIR(veh), INFORMATIVE_MINE_VALUE(ch, IN_ROOM(veh)), FALSE /* public */, (HOLYLIGHT_OR_VEH_OWNER((ch), (veh)) && VEH_FLAGGED((veh), VEH_PLAYER_NO_WORK)) || (HOLYLIGHT_OR_TILE_OWNER((ch), IN_ROOM(veh)) && VEH_CLAIMS_WITH_ROOM(veh) && ROOM_AFF_FLAGGED(IN_ROOM(veh), ROOM_AFF_NO_WORK)), FALSE /* no-abandon */, (HOLYLIGHT_OR_VEH_OWNER((ch), (veh)) && VEH_FLAGGED((veh), VEH_PLAYER_NO_DISMANTLE)) ? TRUE : FALSE, (ch) && IS_IMMORTAL(ch) && (ROOM_AFF_FLAGGED(IN_ROOM(veh), ROOM_AFF_CHAMELEON) || VEH_FLAGGED(veh, VEH_CHAMELEON)) && IS_COMPLETE(IN_ROOM(veh)) && simple_distance(X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)), X_COORD(IN_ROOM(veh)), Y_COORD(IN_ROOM(veh))) > 2)

// mobact.c
void add_pursuit(char_data *ch, char_data *target);
bool check_reset_mob(char_data *ch, bool force);
bool check_scaling(char_data *mob, char_data *attacker);
void check_scavengers(room_data *room);
void check_scheduled_events_mob(char_data *mob);
void despawn_mob(char_data *ch);
int determine_best_scale_level(char_data *ch, bool check_group);
void end_pursuit(char_data *ch, char_data *target);
struct generic_name_data *get_generic_name_list(int name_set, int sex);
struct generic_name_data *get_best_name_list(int name_set, int sex);
int mob_coins(char_data *mob);
void random_encounter(char_data *ch);
void reschedule_all_despawns();
void scale_mob_as_companion(char_data *mob, char_data *leader, int use_level);
void scale_mob_for_character(char_data *mob, char_data *ch);
void scale_mob_to_level(char_data *mob, int level);
void schedule_aggro_event(char_data *ch);
void schedule_despawn_event(char_data *mob);
void schedule_mob_move_event(char_data *ch, bool randomize);
void schedule_movement_events(char_data *ch);
void schedule_reset_mob(char_data *ch);
void schedule_pursuit_event(char_data *ch);
void schedule_scavenge_event(char_data *ch, bool randomize);
void set_mob_spawn_time(char_data *mob, long when);
void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
void spawn_mobs_from_center(room_data *center);
bool try_mobile_movement(char_data *ch);
bool validate_spawn_location(room_data *room, bitvector_t spawn_flags, int x_coord, int y_coord, bool in_city);

// modify.c
void format_text(char **ptr_string, int mode, descriptor_data *d, unsigned int maxlen);
void show_string(descriptor_data *d, char *input);

// morph.c
void check_morph_ability(char_data *ch);
void end_morph(char_data *ch);
const char *get_morph_desc(char_data *ch, bool long_desc_if_true);
morph_data *find_morph_by_name(char_data *ch, char *name);
void finish_morphing(char_data *ch, morph_data *morph);
bool morph_affinity_ok(room_data *location, morph_data *morph);

// olc.c
bool validate_icon(char *icon);

// olc.building.c
bool bld_has_relation(bld_data *bld, int type, bld_vnum vnum);
bool veh_has_relation(vehicle_data *veh, int type, any_vnum vnum);
int count_bld_relations(bld_data *bld, int type);
char *get_bld_name_by_proto(bld_vnum vnum);

// olc.roomtemplate.c
bool rmt_has_exit(room_template *rmt, int dir);

// pathfind.c
ubyte get_pathfind_key(void);
char *get_pathfind_string(room_data *start, room_data *end, char_data *ch, vehicle_data *veh, PATHFIND_VALIDATOR(*validator));
PATHFIND_VALIDATOR(pathfind_ocean);
PATHFIND_VALIDATOR(pathfind_pilot);
PATHFIND_VALIDATOR(pathfind_road);

// progress.c
void check_for_eligible_goals(empire_data *emp);
void check_progress_refresh();
int count_diplomacy(empire_data *emp, bitvector_t dip_flags);
bool empire_meets_goal_prereqs(empire_data *emp, progress_data *prg);
bool delete_progress_perk_from_list(struct progress_perk **list, int type, int value);
progress_data *find_current_progress_goal_by_name(empire_data *emp, char *name);
progress_data *find_progress_goal_by_name(char *name);
bool find_progress_perk_in_list(struct progress_perk *list, int type, int value);
progress_data *find_purchasable_goal_by_name(empire_data *emp, char *name);
void full_reset_empire_progress(empire_data *only_emp);
void purchase_goal(empire_data *emp, progress_data *prg, char_data *purchased_by);
void refresh_empire_goals(empire_data *emp, any_vnum only_vnum);
void refresh_one_goal_tracker(empire_data *emp, struct empire_goal *goal);
void remove_completed_goal(empire_data *emp, any_vnum vnum);
void script_reward_goal(empire_data *emp, progress_data *prg);
struct empire_goal *start_empire_goal(empire_data *e, progress_data *prg);

// quest.c
bool can_get_quest_from_mob(char_data *ch, char_data *mob, struct quest_temp_list **build_list);
bool can_get_quest_from_obj(char_data *ch, obj_data *obj, struct quest_temp_list **build_list);
bool can_get_quest_from_room(char_data *ch, room_data *room, struct quest_temp_list **build_list);
bool can_get_quest_from_vehicle(char_data *ch, vehicle_data *veh, struct quest_temp_list **build_list);
bool can_turn_in_quest_at(char_data *ch, room_data *loc, quest_data *quest, empire_data **giver_emp);
bool can_turn_quest_in_to_mob(char_data *ch, char_data *mob, struct quest_temp_list **build_list);
bool can_turn_quest_in_to_obj(char_data *ch, obj_data *obj, struct quest_temp_list **build_list);
bool can_turn_quest_in_to_room(char_data *ch, room_data *room, struct quest_temp_list **build_list);
bool can_turn_quest_in_to_vehicle(char_data *ch, vehicle_data *veh, struct quest_temp_list **build_list);
bool char_meets_prereqs(char_data *ch, quest_data *quest, struct instance_data *instance);
int count_crop_variety_in_list(obj_data *list);
int count_owned_buildings(empire_data *emp, bld_vnum vnum);
int count_owned_buildings_by_function(empire_data *emp, bitvector_t flags);
int count_owned_homes(empire_data *emp);
int count_owned_sector(empire_data *emp, sector_vnum vnum);
int count_owned_vehicles(empire_data *emp, any_vnum vnum);
int count_owned_vehicles_by_flags(empire_data *emp, bitvector_t flags);
int count_owned_vehicles_by_function(empire_data *emp, bitvector_t funcs);
bool delete_quest_giver_from_list(struct quest_giver **list, int type, any_vnum vnum);
bool delete_quest_reward_from_list(struct quest_reward **list, int type, any_vnum vnum);
void expire_instance_quests(struct instance_data *inst);
void extract_crop_variety(char_data *ch, int amount);
bool find_quest_giver_in_list(struct quest_giver *list, int type, any_vnum vnum);
bool find_quest_reward_in_list(struct quest_reward *list, int type, any_vnum vnum);
char *get_quest_name_by_proto(any_vnum vnum);
void get_quest_reward_display(struct quest_reward *list, char *save_buffer, bool show_vnums);
void get_tracker_display(struct req_data *tracker, char *save_buffer);
void give_quest_rewards(char_data *ch, struct quest_reward *list, int reward_level, empire_data *quest_giver_emp, int instance_id);
struct player_completed_quest *has_completed_quest(char_data *ch, any_vnum quest, int instance_id);
struct player_quest *is_on_quest(char_data *ch, any_vnum quest);
struct player_quest *is_on_quest_by_name(char_data *ch, char *argument);
char *quest_giver_string(struct quest_giver *giver, bool show_vnums);
char *quest_reward_string(struct quest_reward *reward, bool show_vnums);
void refresh_all_quests(char_data *ch);
void refresh_one_quest_tracker(char_data *ch, struct player_quest *pq);
void remove_quest_items(char_data *ch);
void remove_quest_items_by_quest(char_data *ch, any_vnum vnum);
void setup_daily_quest_cycles(int only_cycle);
int sort_completed_quests_by_timestamp(struct player_completed_quest *a, struct player_completed_quest *b);

void qt_change_ability(char_data *ch, any_vnum abil);
void qt_change_level(char_data *ch, int level);
void qt_change_production_total(char_data *ch, any_vnum vnum, int amount);
void qt_change_reputation(char_data *ch, any_vnum faction);
void qt_change_skill_level(char_data *ch, any_vnum skl);
void qt_drop_obj(char_data *ch, obj_data *obj);
void qt_empire_cities(char_data *ch, any_vnum amount);
void qt_empire_diplomacy(char_data *ch, any_vnum amount);
void qt_empire_greatness(char_data *ch, any_vnum amount);
void qt_empire_players(empire_data *emp, void (*func)(char_data *ch, any_vnum vnum), any_vnum vnum);
void qt_empire_players_vehicle(empire_data *emp, void (*func)(char_data *ch, vehicle_data *veh), vehicle_data *veh);
void qt_gain_building(char_data *ch, any_vnum vnum);
void qt_gain_tile_sector(char_data *ch, sector_vnum vnum);
void qt_change_coins(char_data *ch);
void qt_change_currency(char_data *ch, any_vnum vnum, int total);
void qt_change_language(char_data *ch, any_vnum vnum, int level);
void qt_empire_wealth(char_data *ch, any_vnum amount);
void qt_event_start_stop(any_vnum event_vnum);
void qt_gain_vehicle(char_data *ch, vehicle_data *veh);
void qt_get_obj(char_data *ch, obj_data *obj);
void qt_keep_obj(char_data *ch, obj_data *obj, bool true_for_keep);
void qt_kill_mob(char_data *ch, char_data *mob);
void qt_lose_building(char_data *ch, any_vnum vnum);
void qt_lose_quest(char_data *ch, any_vnum vnum);
void qt_lose_tile_sector(char_data *ch, sector_vnum vnum);
void qt_lose_vehicle(char_data *ch, vehicle_data *veh);
void qt_quest_completed(char_data *ch, any_vnum vnum);
void qt_remove_obj(char_data *ch, obj_data *obj);
void qt_start_quest(char_data *ch, any_vnum vnum);
void qt_triggered_task(char_data *ch, any_vnum vnum, int specific_val);
void qt_untrigger_task(char_data *ch, any_vnum vnum, bool remove_all);
void qt_visit_room(char_data *ch, room_data *room);
void qt_wear_obj(char_data *ch, obj_data *obj);

// progress.c
void complete_goal(empire_data *emp, struct empire_goal *goal);
int count_empire_crop_variety(empire_data *emp, int max_needed, int only_island);

void et_change_cities(empire_data *emp);
void et_change_coins(empire_data *emp, int amount);
void et_change_diplomacy(empire_data *emp);
void et_change_greatness(empire_data *emp);
void et_event_start_stop(any_vnum event_vnum);
void et_gain_building(empire_data *emp, any_vnum vnum);
void et_gain_tile_sector(empire_data *emp, sector_vnum vnum);
void et_gain_vehicle(empire_data *emp, vehicle_data *veh);
void et_change_production_total(empire_data *emp, obj_vnum vnum, int amount);
void et_get_obj(empire_data *emp, obj_data *obj, int amount, int new_total);
void et_lose_building(empire_data *emp, any_vnum vnum);
void et_lose_tile_sector(empire_data *emp, sector_vnum vnum);
void et_lose_vehicle(empire_data *emp, vehicle_data *veh);

// random.c
unsigned long empire_random();

// shop.c
struct shop_temp_list *build_available_shop_list(char_data *ch);
bool find_currency_in_shop_item_list(struct shop_item *list, any_vnum vnum);
void free_shop_temp_list(struct shop_temp_list *list);

// social.c
bool validate_social_requirements(char_data *ch, social_data *soc);

// statistics.c
void display_statistics_to_char(char_data *ch);
void mudstats_empires(char_data *ch, char *argument);
int stats_get_building_count(bld_data *bdg);
int stats_get_crop_count(crop_data *cp);
int stats_get_sector_count(sector_data *sect);
void update_players_online_stats();
void update_world_count();

// vehicles.c
void add_room_to_vehicle(room_data *room, vehicle_data *veh);
bool check_vehicle_climate_change(vehicle_data *veh, bool immediate_only);
void complete_vehicle(vehicle_data *veh);
int count_harnessed_animals(vehicle_data *veh);
int count_players_in_vehicle(vehicle_data *veh, bool ignore_invis_imms);
int count_building_vehicles_in_room(room_data *room, empire_data *only_owner);
void Crash_save_vehicles(vehicle_data *veh, FILE *fl);
bool decay_one_vehicle(vehicle_data *veh, char *message);
void delete_vehicle_interior(vehicle_data *veh);
void empty_vehicle(vehicle_data *veh, room_data *to_room);
craft_data *find_craft_for_vehicle(vehicle_data *veh);
vehicle_data *find_dismantling_vehicle_in_room(room_data *room, int with_id);
struct vehicle_attached_mob *find_harnessed_mob_by_name(vehicle_data *veh, char *name);
vehicle_data *find_vehicle_in_room_with_interior(room_data *room, room_vnum interior_room);
void finish_dismantle_vehicle(char_data *ch, vehicle_data *veh);
void fully_empty_vehicle(vehicle_data *veh, room_data *to_room);
int get_new_vehicle_construction_id();
room_data *get_vehicle_interior(vehicle_data *veh);
char *get_vehicle_name_by_proto(obj_vnum vnum);
char *get_vehicle_short_desc(vehicle_data *veh, char_data *to);
void harness_mob_to_vehicle(char_data *mob, vehicle_data *veh);
char *list_harnessed_mobs(vehicle_data *veh);
void look_at_vehicle(vehicle_data *veh, char_data *ch);
void process_dismantle_vehicle(char_data *ch);
void remove_room_from_vehicle(room_data *room, vehicle_data *veh);
void ruin_vehicle(vehicle_data *veh, char *message);
void scale_vehicle_to_level(vehicle_data *veh, int level);
void start_dismantle_vehicle(vehicle_data *veh);
void start_vehicle_burning(vehicle_data *veh);
int total_small_vehicles_in_room(room_data *room);
int total_vehicle_size_in_room(room_data *room);
char_data *unharness_mob_from_vehicle(struct vehicle_attached_mob *vam, vehicle_data *veh);
vehicle_data *unstore_vehicle_from_file(FILE *fl, any_vnum vnum, char *error_str);
void update_vehicle_island_and_loc(vehicle_data *veh, room_data *loc);
bool vehicle_allows_climate(vehicle_data *veh, room_data *room, bool *allow_slow_ruin);
bool vehicle_is_chameleon(vehicle_data *veh, room_data *from);

// weather.c moons
int compute_night_light_radius(room_data *room);
void determine_seasons();
moon_phase_t get_moon_phase(double cycle_days);
moon_pos_t get_moon_position(moon_phase_t phase, int hour);
bool look_at_moon(char_data *ch, char *name, int *number);
void show_visible_moons(char_data *ch);

// weather.c temperature
int calculate_temperature(int temp_type, bitvector_t climates, int season, int sun);
void check_temperature_penalties(char_data *ch);
int get_relative_temperature(char_data *ch);
int get_room_temperature(room_data *room);
int get_temperature_type(room_data *room);
void reset_player_temperature(char_data *ch);
const char *temperature_to_string(int temperature);
void update_player_temperature(char_data *ch);
int warm_player_from_liquid(char_data *ch, int hours_drank, any_vnum liquid);

// weather.c time
double get_hours_of_sun(room_data *room);
struct time_info_data get_local_time(room_data *room);
int get_sun_status(room_data *room);
int get_zenith_days_from_solstice(room_data *room);
bool is_zenith_day(room_data *room);

// weather.c weather
void reset_weather();

// workforce.c
void add_workforce_production_log(empire_data *emp, int type, any_vnum vnum, int amount);
void deactivate_workforce(empire_data *emp, int island_id, int type);
void deactivate_workforce_island(empire_data *emp, int island_id);
void deactivate_workforce_room(empire_data *emp, room_data *room);
char_data *find_chore_worker_in_room(empire_data *emp, room_data *room, vehicle_data *veh, mob_vnum vnum);
struct empire_npc_data *find_free_npc_for_chore(empire_data *emp, room_data *loc);
void free_workforce_where_log(struct workforce_where_log **to_free);
int *get_ordered_chores();
void log_workforce_where(empire_data *emp, char_data *mob, int chore);
void remove_from_workforce_where_log(empire_data *emp, char_data *mob);
void set_workforce_production_limit(empire_data *emp, any_vnum vnum, int amount);
void sort_einv_for_empire(empire_data *emp);


 //////////////////////////////////////////////////////////////////////////////
//// MISCELLANEOUS UTILS /////////////////////////////////////////////////////

// unique int for each 'daily cycle' (used for experience, scripting, etc)
#define DAILY_CYCLE_DAY  (1 + (long)((data_get_long(DATA_DAILY_CYCLE) - data_get_long(DATA_WORLD_START)) / SECS_PER_REAL_DAY))

// Group related defines
#define GROUP(ch)  (ch->group)
#define GROUP_LEADER(group)  (group->leader)
#define GROUP_FLAGS(group)  (group->group_flags)

// handy
#define SELF(sub, obj)  ((sub) == (obj))

// this is sort of a config
#define HAPPY_COLOR(cur, base)  ((cur) < (base) ? "&r" : ((cur) > (base) ? "&g" : "&0"))


// supplementary math
#define ABSOLUTE(x)  (((x) < 0) ? ((x) * -1) : (x))


// time: converts 0-23 to 1-12am, 1-12pm
#define TIME_TO_12H(time)  ((time) > 12 ? (time) - 12 : ((time) == 0 ? 12 : (time)))
#define AM_PM(time)  (time < 12 ? "am" : "pm")

// time helpers
#define DAY_OF_YEAR(timeinfo)  ((timeinfo).month * 30 + (timeinfo).day + 1)
#define PERCENT_THROUGH_CURRENT_HOUR  (((main_game_pulse / PASSES_PER_SEC) % SECS_PER_MUD_HOUR) / (double)SECS_PER_MUD_HOUR)


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
		else if ((field) < (min)) {	\
			(field) = (min);	\
		}	\
		else if ((field) > (max)) {	\
			(field) = (max);	\
		}	\
	} while (0)


// bounds-safe adding: doubles
#define SAFE_ADD_DOUBLE(field, amount, min, max, warn)  do { \
		long double _safe_add_old_val = (field);	\
		field += (amount);	\
		if ((amount) > 0.0 && (field) < _safe_add_old_val) {	\
			(field) = (max);	\
			if (warn) {	\
				log("SYSERR: SAFE_ADD_DOUBLE out of bounds at %s:%d.", __FILE__, __LINE__);	\
			}	\
		}	\
		else if ((amount) < 0.0 && (field) > _safe_add_old_val) {	\
			(field) = (min);	\
			if (warn) {	\
				log("SYSERR: SAFE_ADD_DOUBLE out of bounds at %s:%d.", __FILE__, __LINE__);	\
			}	\
		}	\
		else if ((field) < (min)) {	\
			(field) = (min);	\
		}	\
		else if ((field) > (max)) {	\
			(field) = (max);	\
		}	\
	} while (0)


// shortcudt for messaging
#define send_config_msg(ch, conf)  msg_to_char((ch), "%s\r\n", config_get_string(conf))


 //////////////////////////////////////////////////////////////////////////////
//// CONSTS FOR UTILS.C //////////////////////////////////////////////////////

// get_filename()
#define PLR_FILE  0
#define DELAYED_FILE  1
#define DELETED_PLR_FILE  2
#define DELETED_DELAYED_FILE  3
#define MAP_MEMORY_FILE  4


// APPLY_RES_x: messaging for the apply_resource() function
#define APPLY_RES_SILENT  0	// send no messages
#define APPLY_RES_BUILD  1	// send build/maintain message
#define APPLY_RES_CRAFT  2	// send craft message
#define APPLY_RES_REPAIR  3	// vehicle repairing
#define NUM_APPLY_RES_TYPES  4
