/* ************************************************************************
*   File: olc.h                                           EmpireMUD 2.0b5 *
*  Usage: On-Line Creation header file                                    *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/*
 * EmpireMUD OLC Code
 */


#define MAX_VNUM  999999	// highest allowed vnum for olc
#define START_PLAYER_BOOKS  (MAX_VNUM+1)	// start of vnums for player-written books


// olc controls
#define LVL_BUILDER  LVL_START_IMM	// level at which olc commands are available
#define LVL_UNRESTRICTED_BUILDER  LVL_CIMPL	// level at which there are no vnum restrictions on olc


#define OLC_MODULE(name)	void (name)(char_data *ch, int type, char *argument)


// OLC_x types -- these are bits because some things operate on more than one type
#define OLC_CRAFT  BIT(0)
#define OLC_MOBILE  BIT(1)
#define OLC_OBJECT  BIT(2)
#define OLC_MAP  BIT(3)
#define OLC_BUILDING  BIT(4)
#define OLC_TRIGGER  BIT(5)
#define OLC_CROP  BIT(6)
#define OLC_SECTOR  BIT(7)
#define OLC_ADVENTURE  BIT(8)
#define OLC_ROOM_TEMPLATE  BIT(9)
#define OLC_GLOBAL  BIT(10)
#define OLC_BOOK  BIT(11)
#define OLC_AUGMENT  BIT(12)
#define OLC_ARCHETYPE  BIT(13)
#define OLC_ABILITY  BIT(14)
#define OLC_CLASS  BIT(15)
#define OLC_SKILL  BIT(16)
#define OLC_VEHICLE  BIT(17)
#define OLC_MORPH  BIT(18)
#define OLC_QUEST  BIT(19)
#define OLC_SOCIAL  BIT(20)
#define OLC_FACTION  BIT(21)
#define OLC_GENERIC  BIT(22)
#define OLC_SHOP  BIT(23)
#define OLC_PROGRESS  BIT(24)
#define OLC_EVENT  BIT(25)
#define OLC_ATTACK  BIT(26)
#define NUM_OLC_TYPES  27


// olc command flags
#define OLC_CF_EDITOR  BIT(0)	// requires an active edit
#define OLC_CF_NO_ABBREV  BIT(1)	// does not allow command abbrevs
#define OLC_CF_MAP_EDIT  BIT(2)	// needs OLC_MFLAG_MAP_EDIT


// olc flags for players -- OLC_FLAGGED(ch, OLC_FLAG_x)
#define OLC_FLAG_ALL_VNUMS  BIT(0)	// access to any vnum
#define OLC_FLAG_MAP_EDIT  BIT(1)	// can edit map
#define OLC_FLAG_CLEAR_IN_DEV  BIT(2)	// can remove the in-development
#define OLC_FLAG_NO_CRAFT  BIT(3)	// cannot edit crafts
#define OLC_FLAG_NO_MOBILE  BIT(4)	// cannot edit mobs
#define OLC_FLAG_NO_OBJECT  BIT(5)	// cannot edit objects
#define OLC_FLAG_NO_BUILDING  BIT(6)	// cannot edit buildings
#define OLC_FLAG_NO_SECTORS  BIT(7)	// cannot edit sectors
#define OLC_FLAG_NO_CROP  BIT(8)	// cannot edit crops
#define OLC_FLAG_NO_TRIGGER  BIT(9)	// cannot edit triggers
#define OLC_FLAG_NO_ADVENTURE  BIT(10)	// cannot edit adventures
#define OLC_FLAG_NO_ROOM_TEMPLATE  BIT(11)	// cannot edit room templates
#define OLC_FLAG_NO_GLOBAL  BIT(12)	// cannot edit globals
#define OLC_FLAG_NO_AUGMENT  BIT(13)	// cannot edit augs
#define OLC_FLAG_NO_ARCHETYPE  BIT(14)	// cannot edit archetypes
#define OLC_FLAG_NO_ABILITIES  BIT(15)	// CAN edit abilities
#define OLC_FLAG_NO_CLASSES  BIT(16)	// CAN edit classes
#define OLC_FLAG_NO_SKILLS  BIT(17)	// CAN edit skills
#define OLC_FLAG_NO_VEHICLES  BIT(18)	// cannot edit vehicles
#define OLC_FLAG_NO_MORPHS  BIT(19)	// cannot edit morphs
#define OLC_FLAG_NO_QUESTS  BIT(20)	// cannot edit quests
#define OLC_FLAG_NO_SOCIALS  BIT(21)	// cannot edit socials
#define OLC_FLAG_NO_FACTIONS  BIT(22)	// cannot edit factions
#define OLC_FLAG_NO_GENERICS  BIT(23)	// cannot edit generics
#define OLC_FLAG_NO_SHOPS  BIT(24)	// cannot edit shops
#define OLC_FLAG_NO_PROGRESS  BIT(25)	// cannot edit progress
#define OLC_FLAG_NO_EVENTS  BIT(26)	// cannot edit events
#define OLC_FLAG_NO_ATTACKS  BIT(27)	// cannot edit attacks
#define OLC_FLAG_REFRESH_COMPANIONS  BIT(28)	// can refreshcompanions


// ABILEDIT_x: Which parts of the ability menu and stats to show
#define ABILEDIT_IMMUNITIES		BIT(0)
#define ABILEDIT_COMMAND		BIT(1)
#define ABILEDIT_TARGETS		BIT(2)
#define ABILEDIT_COST			BIT(3)
#define ABILEDIT_MIN_POS		BIT(4)
#define ABILEDIT_WAIT			BIT(5)
#define ABILEDIT_DURATION		BIT(6)
#define ABILEDIT_AFFECTS		BIT(7)
#define ABILEDIT_APPLIES		BIT(8)
#define ABILEDIT_AFFECT_VNUM	BIT(9)
#define ABILEDIT_ATTACK_TYPE	BIT(10)
#define ABILEDIT_MAX_STACKS		BIT(11)
#define ABILEDIT_INTERACTIONS	BIT(12)
#define ABILEDIT_COOLDOWN		BIT(13)
#define ABILEDIT_DIFFICULTY		BIT(14)
#define ABILEDIT_TOOL			BIT(15)
#define ABILEDIT_DAMAGE_TYPE	BIT(16)
#define ABILEDIT_COST_PER_AMOUNT	BIT(17)
#define ABILEDIT_COST_PER_TARGET	BIT(18)
#define ABILEDIT_POOL_TYPE		BIT(19)
#define ABILEDIT_MOVE_TYPE		BIT(20)


// TRIG_ARG_x: for trigger editing
#define TRIG_ARG_PERCENT  BIT(0)
#define TRIG_ARG_COMMAND  BIT(1)
#define TRIG_ARG_PHRASE_OR_WORDLIST  BIT(2)	// 0 = phrase, 1 = wordlist
#define TRIG_ARG_COST  BIT(3)
#define TRIG_ARG_OBJ_WHERE  BIT(4)


// these cause the color change on olc labels
#define OLC_LABEL_CHANGED  (PRF_FLAGGED(ch, PRF_SCREEN_READER) ? "\tg*" : "\tg")
#define OLC_LABEL_UNCHANGED  "\ty"
#define OLC_LABEL_STR(cur, dflt)  ((cur && strcmp(cur, dflt)) ? OLC_LABEL_CHANGED : OLC_LABEL_UNCHANGED)
#define OLC_LABEL_PTR(ptr)  (ptr ? OLC_LABEL_CHANGED : OLC_LABEL_UNCHANGED)
#define OLC_LABEL_VAL(val, dflt)  (val != dflt ? OLC_LABEL_CHANGED : OLC_LABEL_UNCHANGED)


// subcommands for olc
struct olc_command_data {
	char *command;
	OLC_MODULE(*func);
	int valid_types;	// OLC_ type const
	int flags;	// OLC_CF_
};

// for parse_quest_reward_vnum:
#define PARSE_QRV_FAILED  -999


// olc.c helpers
bool can_start_olc_edit(char_data *ch, int type, any_vnum vnum);
void show_icons_display(char_data *ch, struct icon_data *list, bool send_output);
void show_interaction_display(char_data *ch, struct interaction_item *list, bool send_output);
void show_resource_display(char_data *ch, struct resource_data *list, bool send_output);
void show_script_display(char_data *ch, struct trig_proto_list *list, bool send_output);
int find_olc_type(char *name);
bool interactions_are_identical(struct interaction_item *a, struct interaction_item *b);
char *one_icon_display(char *icon, char *base_color);
bool player_can_olc_edit(char_data *ch, int type, any_vnum vnum);
int which_olc_command(char_data *ch, char *command, bitvector_t olc_type);

// olc auditor functions
bool audit_ability(ability_data *abil, char_data *ch);
bool audit_adventure(adv_data *adv, char_data *ch, bool only_one);
bool audit_archetype(archetype_data *arch, char_data *ch);
bool audit_attack_message(attack_message_data *amd, char_data *ch);
bool audit_augment(augment_data *aug, char_data *ch);
bool audit_building(bld_data *bld, char_data *ch);
bool audit_class(class_data *cls, char_data *ch);
bool audit_craft(craft_data *craft, char_data *ch);
bool audit_crop(crop_data *cp, char_data *ch);
bool audit_event(event_data *event, char_data *ch);
bool audit_faction(faction_data *fct, char_data *ch);
bool audit_generic(generic_data *gen, char_data *ch);
bool audit_global(struct global_data *global, char_data *ch);
bool audit_mobile(char_data *mob, char_data *ch);
bool audit_morph(morph_data *morph, char_data *ch);
bool audit_object(obj_data *obj, char_data *ch);
bool audit_progress(progress_data *prg, char_data *ch);
bool audit_quest(quest_data *quest, char_data *ch);
bool audit_room_template(room_template *rmt, char_data *ch);
bool audit_sector(sector_data *sect, char_data *ch);
bool audit_shop(shop_data *shop, char_data *ch);
bool audit_skill(skill_data *skill, char_data *ch);
bool audit_social(social_data *soc, char_data *ch);
bool audit_trigger(trig_data *trig, char_data *ch);
bool audit_vehicle(vehicle_data *veh, char_data *ch);

// supplemental auditors
bool audit_extra_descs(any_vnum vnum, struct extra_descr_data *list, char_data *ch);
bool audit_interactions(any_vnum vnum, struct interaction_item *list, int attach_type, char_data *ch);
bool audit_spawns(any_vnum vnum, struct spawn_info *list, char_data *ch);

// olc copiers
struct archetype_gear *copy_archetype_gear(struct archetype_gear *input);
struct bld_relation *copy_bld_relations(struct bld_relation *input_list);
struct extra_descr_data *copy_extra_descs(struct extra_descr_data *list);
struct icon_data *copy_icon_set(struct icon_data *input_list);
struct interaction_item *copy_interaction_list(struct interaction_item *input_list);
struct spawn_info *copy_spawn_list(struct spawn_info *input_list);
void smart_copy_bld_relations(struct bld_relation **to_list, struct bld_relation *from_list);
void smart_copy_icons(struct icon_data **addto, struct icon_data *input);
void smart_copy_interactions(struct interaction_item **addto, struct interaction_item *input);
void smart_copy_requirements(struct req_data **to_list, struct req_data *from_list);
void smart_copy_scripts(struct trig_proto_list **addto, struct trig_proto_list *input);
void smart_copy_spawns(struct spawn_info **addto, struct spawn_info *input);
void smart_copy_template_spawns(struct adventure_spawn **addto, struct adventure_spawn *input);

// olc editors
void setup_extra_desc_editor(char_data *ch, struct extra_descr_data *ex);

// olc list-deleters
bool delete_bld_relation_by_vnum(struct bld_relation **list, int type, bld_vnum vnum);
bool delete_event_reward_from_list(struct event_reward **list, int type, any_vnum vnum);
bool delete_from_interaction_list(struct interaction_item **list, int vnum_type, any_vnum vnum);
bool delete_from_interaction_restrictions(struct interaction_item **list, int type, any_vnum vnum);
bool delete_link_rule_by_portal(struct adventure_link_rule **list, obj_vnum portal_vnum);
bool delete_link_rule_by_type_value(struct adventure_link_rule **list, int type, any_vnum value);
bool delete_from_spawn_template_list(struct adventure_spawn **list, int spawn_type, mob_vnum vnum);
bool delete_shop_item_from_list(struct shop_item **list, any_vnum vnum);
bool remove_vnum_from_class_skill_reqs(struct class_skill_req **list, any_vnum vnum);

// olc processors/parsers
void archedit_process_gear(char_data *ch, char *argument, struct archetype_gear **list);
void olc_process_applies(char_data *ch, char *argument, struct apply_data **list);
void olc_process_custom_messages(char_data *ch, char *argument, struct custom_message **list, const char **type_names, const char *type_help_string);
double olc_process_double(char_data *ch, char *argument, char *name, char *command, double min, double max, double old_value);
bitvector_t olc_process_flag(char_data *ch, char *argument, char *name, char *command, const char **flag_names, bitvector_t existing_bits);
int olc_process_number(char_data *ch, char *argument, char *name, char *command, int min, int max, int old_value);
void olc_process_string(char_data *ch, char *argument, const char *name, char **save_point);
int olc_process_type(char_data *ch, char *argument, char *name, char *command, const char **type_names, int old_value);
void olc_process_extra_desc(char_data *ch, char *argument, struct extra_descr_data **list);
void olc_process_icons(char_data *ch, char *argument, struct icon_data **list);
void olc_process_interactions(char_data *ch, char *argument, struct interaction_item **list, int attach_type);
void olc_process_relations(char_data *ch, char *argument, struct bld_relation **list);
void olc_process_requirements(char_data *ch, char *argument, struct req_data **list, char *command, bool allow_tracker_types);
void olc_process_resources(char_data *ch, char *argument, struct resource_data **list);
void olc_process_spawns(char_data *ch, char *argument, struct spawn_info **list);
void olc_process_script(char_data *ch, char *argument, struct trig_proto_list **list, int trigger_attach);
any_vnum parse_quest_reward_vnum(char_data *ch, int type, char *vnum_arg, char *prev_arg);
void qedit_process_quest_givers(char_data *ch, char *argument, struct quest_giver **list, char *command);

// olc save functions
void save_olc_building(descriptor_data *desc);
void save_olc_craft(descriptor_data *desc);
void save_olc_vehicle(descriptor_data *desc);

// olc setup functions
ability_data *setup_olc_ability(ability_data *input);
adv_data *setup_olc_adventure(adv_data *input);
archetype_data *setup_olc_archetype(archetype_data *input);
attack_message_data *setup_olc_attack_message(attack_message_data *input);
augment_data *setup_olc_augment(augment_data *input);
book_data *setup_olc_book(book_data *input);
bld_data *setup_olc_building(bld_data *input);
class_data *setup_olc_class(class_data *input);
craft_data *setup_olc_craft(craft_data *input);
crop_data *setup_olc_crop(crop_data *input);
event_data *setup_olc_event(event_data *input);
faction_data *setup_olc_faction(faction_data *input);
generic_data *setup_olc_generic(generic_data *input);
struct global_data *setup_olc_global(struct global_data *input);
char_data *setup_olc_mobile(char_data *input);
morph_data *setup_olc_morph(morph_data *input);
obj_data *setup_olc_object(obj_data *input);
progress_data *setup_olc_progress(progress_data *input);
quest_data *setup_olc_quest(quest_data *input);
room_template *setup_olc_room_template(room_template *input);
sector_data *setup_olc_sector(sector_data *input);
shop_data *setup_olc_shop(shop_data *input);
skill_data *setup_olc_skill(skill_data *input);
social_data *setup_olc_social(social_data *input);
trig_data *setup_olc_trigger(struct trig_data *input, char **cmdlist_storage);
vehicle_data *setup_olc_vehicle(vehicle_data *input);

// main olc displays
void olc_show_ability(char_data *ch);
void olc_show_adventure(char_data *ch);
void olc_show_archetype(char_data *ch);
void olc_show_attack_message(char_data *ch);
void olc_show_augment(char_data *ch);
void olc_show_building(char_data *ch);
void olc_show_class(char_data *ch);
void olc_show_craft(char_data *ch);
void olc_show_crop(char_data *ch);
void olc_show_event(char_data *ch);
void olc_show_faction(char_data *ch);
void olc_show_generic(char_data *ch);
void olc_show_global(char_data *ch);
void olc_show_mobile(char_data *ch);
void olc_show_morph(char_data *ch);
void olc_show_object(char_data *ch);
void olc_show_progress(char_data *ch);
void olc_show_quest(char_data *ch);
void olc_show_room_template(char_data *ch);
void olc_show_sector(char_data *ch);
void olc_show_shop(char_data *ch);
void olc_show_skill(char_data *ch);
void olc_show_social(char_data *ch);
void olc_show_trigger(char_data *ch);
void olc_show_vehicle(char_data *ch);

// olc display parts
void get_generic_relation_display(struct generic_relation *list, bool show_vnums, char *save_buf, char *prefix);
char *get_interaction_restriction_display(struct interact_restriction *list, bool whole_list);
void show_adventure_linking_display(char_data *ch, struct adventure_link_rule *list, bool send_output);
void show_archetype_gear_display(char_data *ch, struct archetype_gear *list, bool send_output);
void show_bld_relations_display(char_data *ch, struct bld_relation *list, bool send_output);
void show_extra_desc_display(char_data *ch, struct extra_descr_data *list, bool send_output);
void show_evolution_display(char_data *ch, struct evolution_data *list, bool send_output);
void show_exit_template_display(char_data *ch, struct exit_template *list, bool send_output);
void show_progress_perks_display(char_data *ch, struct progress_perk *list, bool show_vnums, bool send_output);
void show_quest_giver_display(char_data *ch, struct quest_giver *list, bool send_output);
void show_requirement_display(char_data *ch, struct req_data *list, bool send_output);
void show_template_spawns_display(char_data *ch, struct adventure_spawn *list, bool send_output);

// olc helpers
const char *get_interaction_target(int type, any_vnum vnum);

// word count: core functions
int wordcount_custom_messages(struct custom_message *list);
int wordcount_extra_descriptions(struct extra_descr_data *list);
int wordcount_requirements(struct req_data *list);
int wordcount_string(const char *string);

// word count: types
int wordcount_ability(ability_data *abil);
int wordcount_adventure(struct adventure_data *adv);
int wordcount_archetype(archetype_data *arch);
int wordcount_attack_message(attack_message_data *amd);
int wordcount_augment(augment_data *aug);
int wordcount_book(book_data *book);
int wordcount_building(bld_data *bld);
int wordcount_class(class_data *cls);
int wordcount_craft(craft_data *craft);
int wordcount_crop(crop_data *crop);
int wordcount_event(event_data *evt);
int wordcount_faction(faction_data *fct);
int wordcount_generic(generic_data *gen);
int wordcount_global(struct global_data *glb);
int wordcount_mobile(char_data *ch);
int wordcount_morph(morph_data *mph);
int wordcount_object(obj_data *obj);
int wordcount_progress(progress_data *prg);
int wordcount_quest(quest_data *quest);
int wordcount_room_template(room_template *rmt);
int wordcount_sector(sector_data *sect);
int wordcount_shop(shop_data *shop);
int wordcount_skill(skill_data *skill);
int wordcount_social(social_data *soc);
int wordcount_trigger(trig_data *trig);
int wordcount_vehicle(vehicle_data *veh);

// helpers from other systems
bool find_event_reward_in_list(struct event_reward *list, int type, any_vnum vnum);
bool find_interaction_restriction_in_list(struct interaction_item *list, int type, any_vnum vnum);
bool find_shop_item_in_list(struct shop_item *list, any_vnum vnum);


// fullsearch helpers

#define FULLSEARCH_BOOL(string, var)	\
	else if (is_abbrev(type_arg, "-"string)) {	\
		var = TRUE;	\
	}

#define FULLSEARCH_CHAR(string, var)	\
	else if (is_abbrev(type_arg, "-"string)) {	\
		argument = any_one_word(argument, val_arg);	\
		if (!(var = *val_arg)) {	\
			msg_to_char(ch, "Invalid %s '%s'.\r\n", string, val_arg);	\
			return;	\
		}	\
	}

#define FULLSEARCH_DOUBLE(string, var, min, max)	\
	else if (is_abbrev(type_arg, "-"string)) {	\
		argument = any_one_word(argument, val_arg);	\
		if (!isdigit(*val_arg) || (var = atof(val_arg)) < min || var > max) {	\
			msg_to_char(ch, "Invalid %s '%s'.\r\n", string, val_arg);	\
			return;	\
		}	\
	}

#define FULLSEARCH_INT(string, var, min, max)	\
	else if (is_abbrev(type_arg, "-"string)) {	\
		argument = any_one_word(argument, val_arg);	\
		if (!isdigit(*val_arg) || (var = atoi(val_arg)) < min || var > max) {	\
			msg_to_char(ch, "Invalid %s '%s'.\r\n", string, val_arg);	\
			return;	\
		}	\
	}

#define FULLSEARCH_FLAGS(string, var, names)	\
	else if (is_abbrev(type_arg, "-"string)) {	\
		int lookup;	\
		argument = any_one_word(argument, val_arg);	\
		if ((lookup = search_block(val_arg, names, FALSE)) != NOTHING) {	\
			var |= BIT(lookup);	\
		}	\
		else {	\
			msg_to_char(ch, "Invalid %s '%s'.\r\n", string, val_arg);	\
			return;	\
		}	\
	}

#define FULLSEARCH_FUNC(string, var, func)	\
	else if (is_abbrev(type_arg, "-"string)) {	\
		argument = any_one_word(argument, val_arg);	\
		if (!(var = (func))) {	\
			msg_to_char(ch, "Invalid %s '%s'.\r\n", string, val_arg);	\
			return;	\
		}	\
	}

#define FULLSEARCH_LIST(string, var, names)	\
	else if (is_abbrev(type_arg, "-"string)) {	\
		argument = any_one_word(argument, val_arg);	\
		if ((var = search_block(val_arg, names, FALSE)) == NOTHING) {	\
			msg_to_char(ch, "Invalid %s '%s'.\r\n", string, val_arg);	\
			return;	\
		}	\
	}

#define FULLSEARCH_STRING(string, var)	\
	else if (is_abbrev(type_arg, "-"string)) {	\
		argument = any_one_word(argument, val_arg);	\
		strcpy(var, val_arg);	\
	}
