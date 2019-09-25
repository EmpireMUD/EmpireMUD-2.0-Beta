/* ************************************************************************
*   File: handler.h                                       EmpireMUD 2.0b5 *
*  Usage: header file: prototypes of handling and utility functions       *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

 //////////////////////////////////////////////////////////////////////////////
//// HANDLER CONSTS //////////////////////////////////////////////////////////

// for affect_join()
#define ADD_DURATION	BIT(0)
#define AVG_DURATION	BIT(1)
#define ADD_MODIFIER	BIT(2)
#define AVG_MODIFIER	BIT(3)
#define SILENT_AFF		BIT(4)	// prevents messaging


// for find_all_dots
#define FIND_INDIV  0
#define FIND_ALL  1
#define FIND_ALLDOT  2


// for generic_find()
#define FIND_CHAR_ROOM		BIT(0)
#define FIND_CHAR_WORLD		BIT(1)
#define FIND_OBJ_INV		BIT(2)
#define FIND_OBJ_ROOM		BIT(3)
#define FIND_OBJ_WORLD		BIT(4)
#define FIND_OBJ_EQUIP		BIT(5)
#define FIND_NO_DARK		BIT(6)	// ignores light
#define FIND_VEHICLE_ROOM	BIT(7)
#define FIND_VEHICLE_INSIDE	BIT(8)
#define FIND_NPC_ONLY		BIT(9)	// ignores players


// for match_char_name()
#define MATCH_GLOBAL  BIT(0)	// ignores dark/blind
#define MATCH_IN_ROOM  BIT(1)	// specifically checks for things that only matter in-room


// for the interaction handlers (returns TRUE if the character performs the interaction; FALSE if it aborts)
#define INTERACTION_FUNC(name)	bool (name)(char_data *ch, struct interaction_item *interaction, room_data *inter_room, char_data *inter_mob, obj_data *inter_item)


 //////////////////////////////////////////////////////////////////////////////
//// HANDLER MACROS //////////////////////////////////////////////////////////

#define MATCH_ITEM_NAME(str, obj)  (isname((str), GET_OBJ_KEYWORDS(obj)) || (IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_CONTENTS(obj) > 0 && isname((str), get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_NAME))))


 //////////////////////////////////////////////////////////////////////////////
//// handler.c protos ////////////////////////////////////////////////////////

// affect handlers
void affect_from_char(char_data *ch, any_vnum type, bool show_msg);
void affect_from_char_by_apply(char_data *ch, any_vnum type, int apply, bool show_msg);
void affect_from_char_by_bitvector(char_data *ch, any_vnum type, bitvector_t bits, bool show_msg);
void affect_from_char_by_caster(char_data *ch, any_vnum type, char_data *caster, bool show_msg);
void affects_from_char_by_aff_flag(char_data *ch, bitvector_t aff_flag, bool show_msg);
void affect_from_room(room_data *room, any_vnum type);
void affect_from_room_by_bitvector(room_data *room, any_vnum type, bitvector_t bits, bool show_msg);
void affect_join(char_data *ch, struct affected_type *af, int flags);
void affect_modify(char_data *ch, byte loc, sh_int mod, bitvector_t bitv, bool add);
void affect_remove(char_data *ch, struct affected_type *af);
void affect_remove_room(room_data *room, struct affected_type *af);
void affect_to_char_silent(char_data *ch, struct affected_type *af);
void affect_to_char(char_data *ch, struct affected_type *af);
void affect_to_room(room_data *room, struct affected_type *af);
void affect_total(char_data *ch);
void affect_total_room(room_data *room);
extern bool affected_by_spell(char_data *ch, any_vnum type);
extern bool affected_by_spell_and_apply(char_data *ch, any_vnum type, int apply);
bool affected_by_spell_from_caster(char_data *ch, any_vnum type, char_data *caster);
extern struct affected_type *create_aff(any_vnum type, int duration, int location, int modifier, bitvector_t bitvector, char_data *cast_by);
void apply_dot_effect(char_data *ch, any_vnum type, sh_int duration, sh_int damage_type, sh_int damage, sh_int max_stack, char_data *cast_by);
void dot_remove(char_data *ch, struct over_time_effect_type *dot);
extern bool room_affected_by_spell(room_data *room, any_vnum type);
void show_wear_off_msg(char_data *ch, int atype);

// affect shortcut macros
#define create_flag_aff(type, duration, bit, cast_by)  create_aff((type), (duration), APPLY_NONE, 0, (bit), (cast_by))
#define create_mod_aff(type, duration, loc, mod, cast_by)  create_aff((type), (duration), (loc), (mod), 0, (cast_by))

// character handlers
void extract_char(char_data *ch);
void extract_char_final(char_data *ch);
extern bool match_char_name(char_data *ch, char_data *target, char *name, bitvector_t flags);
void perform_idle_out(char_data *ch);

// character location handlers
void char_from_room(char_data *ch);
void char_to_room(char_data *ch, room_data *room);

// character targeting handlers
extern char_data *find_closest_char(char_data *ch, char *arg, bool pc);
extern char_data *find_mob_in_room_by_vnum(room_data *room, mob_vnum vnum);
extern char_data *find_mortal_in_room(room_data *room);
extern char_data *get_char_room(char *name, room_data *room);
extern char_data *get_char_room_vis(char_data *ch, char *name);
extern char_data *get_char_vis(char_data *ch, char *name, bitvector_t where);
extern char_data *get_player_vis(char_data *ch, char *name, bitvector_t flags);
extern char_data *get_char_world(char *name);

// coin handlers
extern bool can_afford_coins(char_data *ch, empire_data *type, int amount);
void charge_coins(char_data *ch, empire_data *type, int amount, struct resource_data **build_used_list);
void cleanup_all_coins();
void cleanup_coins(char_data *ch);
void coin_string(struct coin_data *list, char *storage);
extern int count_total_coins_as(char_data *ch, empire_data *type);
extern obj_data *create_money(empire_data *type, int amount);
#define decrease_coins(ch, emp, amount)  increase_coins(ch, emp, -1 * amount)
extern double exchange_coin_value(double amount, empire_data *convert_from, empire_data *convert_to);
double exchange_rate(empire_data *from, empire_data *to);
extern char *find_coin_arg(char *input, empire_data **emp_found, int *amount_found, bool assume_coins, bool *gave_coin_type);
extern struct coin_data *find_coin_entry(struct coin_data *list, empire_data *emp);
extern int increase_coins(char_data *ch, empire_data *emp, int amount);
extern const char *money_amount(empire_data *type, int amount);
extern const char *money_desc(empire_data *type, int amount);
extern int total_coins(char_data *ch);

// cooldown handlers
void add_cooldown(char_data *ch, any_vnum type, int seconds_duration);
extern int get_cooldown_time(char_data *ch, any_vnum type);
void remove_cooldown(char_data *ch, struct cooldown_data *cool);
void remove_cooldown_by_type(char_data *ch, any_vnum type);

// currency handlers
extern int add_currency(char_data *ch, any_vnum vnum, int amount);
extern int get_currency(char_data *ch, any_vnum vnum);

// empire handlers
void abandon_room(room_data *room);
void claim_room(room_data *room, empire_data *emp);
extern struct empire_political_data *create_relation(empire_data *a, empire_data *b);
extern int find_rank_by_name(empire_data *emp, char *name);
extern struct empire_political_data *find_relation(empire_data *from, empire_data *to);
extern struct empire_territory_data *find_territory_entry(empire_data *emp, room_data *room);
struct empire_trade_data *find_trade_entry(empire_data *emp, int type, obj_vnum vnum);
extern int increase_empire_coins(empire_data *emp_gaining, empire_data *coin_empire, double amount);
#define decrease_empire_coins(emp_gaining, coin_empire, amount)  increase_empire_coins((emp_gaining), (coin_empire), -1 * (amount))
void perform_abandon_room(room_data *room);
void perform_claim_room(room_data *room, empire_data *emp);

// empire production total handlers
void add_production_total(empire_data *emp, obj_vnum vnum, int amount);
void add_production_total_for_tag_list(struct mob_tag *list, obj_vnum vnum, int amount);
extern int get_production_total(empire_data *emp, obj_vnum vnum);
extern int get_production_total_component(empire_data *emp, int cmp_type, bitvector_t cmp_flags);
void mark_production_trade(empire_data *emp, obj_vnum vnum, int imported, int exported);

// empire needs handlers
void add_empire_needs(empire_data *emp, int island, int type, int amount);
extern struct empire_needs *get_empire_needs(empire_data *emp, int island, int type);
extern bool empire_has_needs_status(empire_data *emp, int island, int type, bitvector_t status);

// empire targeting handlers
extern struct empire_city_data *find_city(empire_data *emp, room_data *loc);
extern struct empire_city_data *find_city_entry(empire_data *emp, room_data *location);
extern struct empire_city_data *find_city_by_name(empire_data *emp, char *name);
extern struct empire_city_data *find_closest_city(empire_data *emp, room_data *loc);
extern empire_data *get_empire_by_name(char *name);

// follow handlers
void add_follower(char_data *ch, char_data *leader, bool msg);
void stop_follower(char_data *ch);

// group handlers
extern int count_group_members(struct group_data *group);
extern struct group_data *create_group(char_data *leader);
void free_group(struct group_data *group);
extern bool in_same_group(char_data *ch, char_data *vict);
void join_group(char_data *ch, struct group_data *group);
void leave_group(char_data *ch);

// interaction handlers
extern bool can_interact_room(room_data *room, int type);
extern bool check_exclusion_set(struct interact_exclusion_data **set, char code, double percent);
extern struct interact_exclusion_data *find_exclusion_data(struct interact_exclusion_data **set, char code);
void free_exclusion_data(struct interact_exclusion_data *list);
extern bool has_interaction(struct interaction_item *list, int type);
extern bool meets_interaction_restrictions(struct interact_restriction *list, char_data *ch, empire_data *emp);
extern bool run_global_mob_interactions(char_data *ch, char_data *mob, int type, INTERACTION_FUNC(*func));
extern bool run_interactions(char_data *ch, struct interaction_item *run_list, int type, room_data *inter_room, char_data *inter_mob, obj_data *inter_item, INTERACTION_FUNC(*func));
extern bool run_room_interactions(char_data *ch, room_data *room, int type, INTERACTION_FUNC(*func));

// lore handlers
void add_lore(char_data *ch, int type, const char *str, ...) __attribute__((format(printf, 3, 4)));
void remove_lore(char_data *ch, int type);
void remove_recent_lore(char_data *ch, int type);

// mob tagging handlers
extern bool find_id_in_tag_list(int id, struct mob_tag *list);
void expand_mob_tags(char_data *mob);
void free_mob_tags(struct mob_tag **list);
void tag_mob(char_data *mob, char_data *player);

// mount handlers
void add_mount(char_data *ch, mob_vnum vnum, bitvector_t flags);
extern bitvector_t get_mount_flags_by_mob(char_data *mob);
extern struct mount_data *find_mount_data(char_data *ch, mob_vnum vnum);
void perform_dismount(char_data *ch);
void perform_mount(char_data *ch, char_data *mount);

// object handlers
void add_to_object_list(obj_data *obj);
extern obj_data *copy_warehouse_obj(obj_data *input);
void empty_obj_before_extract(obj_data *obj);
void extract_obj(obj_data *obj);
extern obj_data *fresh_copy_obj(obj_data *obj, int scale_level);
extern bool objs_are_identical(obj_data *obj_a, obj_data *obj_b);
extern bool parse_component(char *str, int *type, bitvector_t *flags);
void remove_from_object_list(obj_data *obj);

// object binding handlers
void bind_obj_to_group(obj_data *obj, struct group_data *group);
void bind_obj_to_player(obj_data *obj, char_data *ch);
void bind_obj_to_tag_list(obj_data *obj, struct mob_tag *list);
extern bool bind_ok(obj_data *obj, char_data *ch);
void reduce_obj_binding(obj_data *obj, char_data *player);

// object location handlers
void check_obj_in_void(obj_data *obj);
void equip_char(char_data *ch, obj_data *obj, int pos);
void obj_from_char(obj_data *object);
void obj_from_obj(obj_data *obj);
void obj_from_room(obj_data *object);
void obj_from_vehicle(obj_data *obj);
void object_list_no_owner(obj_data *list);
void obj_to_char(obj_data *object, char_data *ch);
void obj_to_char_if_okay(obj_data *obj, char_data *ch);
void obj_to_char_or_room(obj_data *obj, char_data *ch);
void obj_to_obj(obj_data *obj, obj_data *obj_to);
void obj_to_room(obj_data *object, room_data *room);
void obj_to_vehicle(obj_data *object, vehicle_data *veh);
void swap_obj_for_obj(obj_data *old, obj_data *new);
extern obj_data *unequip_char(char_data *ch, int pos);
extern obj_data *unequip_char_to_inventory(char_data *ch, int pos);
extern obj_data *unequip_char_to_room(char_data *ch, int pos);

// custom message handlers
extern struct custom_message *copy_custom_messages(struct custom_message *from);
void free_custom_messages(struct custom_message *mes);
extern char *get_custom_message(struct custom_message *list, int type);
extern bool has_custom_message(struct custom_message *list, int type);

// custom message helpers
#define obj_get_custom_message(obj, type)  get_custom_message(obj->custom_msgs, type)
#define obj_has_custom_message(obj, type)  has_custom_message(obj->custom_msgs, type)
#define abil_get_custom_message(abil, type)  get_custom_message(ABIL_CUSTOM_MSGS(abil), type)
#define abil_has_custom_message(abil, type)  has_custom_message(ABIL_CUSTOM_MSGS(abil), type)

// object targeting handlers
extern obj_data *get_component_in_list(int cmp_type, bitvector_t cmp_flags, obj_data *list, bool *kept);
extern obj_data *get_obj_by_char_share(char_data *ch, char *arg);
extern obj_data *get_obj_in_equip_vis(char_data *ch, char *arg, obj_data *equipment[]);
extern obj_data *get_obj_in_list_num(int num, obj_data *list);
extern obj_data *get_obj_in_list_vnum(obj_vnum vnum, obj_data *list);
extern obj_data *get_obj_in_list_vis(char_data *ch, char *name, obj_data *list);
extern int get_obj_pos_in_equip_vis(char_data *ch, char *arg, obj_data *equipment[]);
extern obj_vnum get_obj_vnum_by_name(char *name, bool storable_only);
extern obj_data *get_obj_vis(char_data *ch, char *name);
extern obj_data *get_object_in_equip_vis(char_data *ch, char *arg, obj_data *equipment[], int *pos);
extern obj_data *get_obj_world(char *name);

// offer handlers
extern struct offer_data *add_offer(char_data *ch, char_data *from, int type, int data);
void remove_offers_by_type(char_data *ch, int type);

// player tech handlers
void add_player_tech(char_data *ch, any_vnum abil, int tech);
extern bool has_player_tech(char_data *ch, int tech);
void remove_player_tech(char_data *ch, any_vnum abil);
extern bool run_ability_triggers_by_player_tech(char_data *ch, int tech, char_data *cvict, obj_data *ovict);

// requirement handlers
void free_requirements(struct req_data *list);

// resource depletion handlers
void add_depletion(room_data *room, int type, bool multiple);
extern int get_depletion(room_data *room, int type);
void remove_depletion_from_list(struct depletion_data **list, int type);
void remove_depletion(room_data *room, int type);
void set_depletion(room_data *room, int type, int value);

// room handlers
void attach_building_to_room(bld_data *bld, room_data *room, bool with_triggers);
void attach_template_to_room(room_template *rmt, room_data *room);
void detach_building_from_room(room_data *room);

// room extra data handlers
void add_to_extra_data(struct room_extra_data **list, int type, int add_value);
extern struct room_extra_data *find_extra_data(struct room_extra_data *list, int type);
extern int get_extra_data(struct room_extra_data *list, int type);
void multiply_extra_data(struct room_extra_data **list, int type, double multiplier);
void remove_extra_data(struct room_extra_data **list, int type);
void set_extra_data(struct room_extra_data **list, int type, int value);

// room extra data helpers (backwards-compatibility and shortcuts)
#define add_to_room_extra_data(room, type, add_value)  add_to_extra_data(&ROOM_EXTRA_DATA(room), type, add_value)
#define find_room_extra_data(room, type)  find_extra_data(ROOM_EXTRA_DATA(room), type)
#define get_room_extra_data(room, type)  get_extra_data(ROOM_EXTRA_DATA(room), type)
#define multiply_room_extra_data(room, type, multiplier)  multiply_extra_data(&ROOM_EXTRA_DATA(room), type, multiplier);
#define remove_room_extra_data(room, type)  remove_extra_data(&ROOM_EXTRA_DATA(room), type)
#define set_room_extra_data(room, type, value)  set_extra_data(&ROOM_EXTRA_DATA(room), type, value)

// room targeting handlers
extern room_data *find_target_room(char_data *ch, char *rawroomstr);

// sector handlers
extern bool check_evolution_percent(struct evolution_data *evo);
extern struct evolution_data *get_evolution_by_type(sector_data *st, int type);
extern bool has_evolution_type(sector_data *st, int type);
sector_data *reverse_lookup_evolution_for_sector(sector_data *in_sect, int evo_type);

// storage handlers
void add_to_empire_storage(empire_data *emp, int island, obj_vnum vnum, int amount);
extern bool charge_stored_component(empire_data *emp, int island, int cmp_type, int cmp_flags, int amount, bool use_kept, struct resource_data **build_used_list);
extern bool charge_stored_resource(empire_data *emp, int island, obj_vnum vnum, int amount);
extern bool delete_stored_resource(empire_data *emp, obj_vnum vnum);
extern bool empire_can_afford_component(empire_data *emp, int island, int cmp_type, int cmp_flags, int amount, bool include_kept);
extern struct empire_storage_data *find_island_storage_by_keywords(empire_data *emp, int island_id, char *keywords);
extern struct empire_storage_data *find_stored_resource(empire_data *emp, int island, obj_vnum vnum);
extern int get_total_stored_count(empire_data *emp, obj_vnum vnum, bool count_shipping);
extern bool obj_can_be_stored(obj_data *obj, room_data *loc, bool retrieval_mode);
extern bool obj_can_be_retrieved(obj_data *obj, room_data *loc);
extern bool retrieve_resource(char_data *ch, empire_data *emp, struct empire_storage_data *store, bool stolen);
extern int store_resource(char_data *ch, empire_data *emp, obj_data *obj);
extern bool stored_item_requires_withdraw(obj_data *obj);

// targeting handlers
extern int find_all_dots(char *arg);
extern int generic_find(char *arg, bitvector_t bitvector, char_data *ch, char_data **tar_ch, obj_data **tar_obj, vehicle_data **tar_veh);
extern int get_number(char **name);

// unique storage handlers
void add_eus_entry(struct empire_unique_storage *eus, empire_data *emp);
extern bool delete_unique_storage_by_vnum(empire_data *emp, obj_vnum vnum);
extern struct empire_unique_storage *find_eus_entry(obj_data *obj, empire_data *emp, room_data *location);
void remove_eus_entry(struct empire_unique_storage *eus, empire_data *emp);
void store_unique_item(char_data *ch, obj_data *obj, empire_data *emp, room_data *room, bool *full);

// vehicle handlers
void extract_vehicle(vehicle_data *veh);
void sit_on_vehicle(char_data *ch, vehicle_data *veh);
void unseat_char_from_vehicle(char_data *ch);
void vehicle_from_room(vehicle_data *veh);
void vehicle_to_room(vehicle_data *veh, room_data *room);

// vehicle targeting handlers
vehicle_data *get_vehicle_in_target_room_vis(char_data *ch, room_data *room, char *name);
#define get_vehicle_in_room_vis(ch, name)  get_vehicle_in_target_room_vis((ch), IN_ROOM(ch), (name))
extern vehicle_data *get_vehicle_vis(char_data *ch, char *name);
extern vehicle_data *get_vehicle_room(room_data *room, char *name);
extern vehicle_data *get_vehicle_world(char *name);

// world handlers
extern struct room_direction_data *find_exit(room_data *room, int dir);
extern int get_direction_for_char(char_data *ch, int dir);
extern int parse_direction(char_data *ch, char *dir);


 //////////////////////////////////////////////////////////////////////////////
//// handlers from other files ///////////////////////////////////////////////

// act.item.c
extern obj_data *perform_remove(char_data *ch, int pos);

// books.c
extern book_data *book_proto(book_vnum vnum);
extern book_data *find_book_by_author(char *argument, int idnum);
extern book_data *find_book_in_library(char *argument, room_data *room);

// config.c
extern bitvector_t config_get_bitvector(char *key);
extern bool config_get_bool(char *key);
extern double config_get_double(char *key);
extern int config_get_int(char *key);
extern int *config_get_int_array(char *key, int *array_size);
extern const char *config_get_string(char *key);

// faction.c
extern int compare_reptuation(int rep_a, int rep_b);
void gain_reputation(char_data *ch, any_vnum vnum, int amount, bool is_kill, bool cascade);
extern struct player_faction_data *get_reputation(char_data *ch, any_vnum vnum, bool create);
extern int get_reputation_by_name(char *name);
extern int get_reputation_value(char_data *ch, any_vnum vnum);
extern bool has_reputation(char_data *ch, any_vnum faction, int rep);
extern int rep_const_to_index(int rep_const);
void set_reputation(char_data *ch, any_vnum vnum, int rep);

// fight.c
void appear(char_data *ch);
extern bool can_fight(char_data *ch, char_data *victim);
extern int damage(char_data *ch, char_data *victim, int dam, int attacktype, byte damtype);
void engage_combat(char_data *ch, char_data *vict, bool melee);
void heal(char_data *ch, char_data *vict, int amount);
extern int hit(char_data *ch, char_data *victim, obj_data *weapon, bool normal_round);
extern bool is_fighting(char_data *ch);
void set_fighting(char_data *ch, char_data *victim, byte mode);
void stop_fighting(char_data *ch);
void update_pos(char_data *victim);

// instance.c
void add_instance_mob(struct instance_data *inst, mob_vnum vnum);
extern struct instance_data *real_instance(any_vnum instance_id);
void subtract_instance_mob(struct instance_data *inst, mob_vnum vnum);

// limits.c
extern int limit_crowd_control(char_data *victim, int atype);

// morph.c
void perform_morph(char_data *ch, morph_data *morph);

// objsave.c

// progress.c
void cancel_empire_goal(empire_data *emp, struct empire_goal *goal);
extern struct empire_goal *get_current_goal(empire_data *emp, any_vnum vnum);
extern bool empire_has_completed_goal(empire_data *emp, any_vnum vnum);
extern time_t when_empire_completed_goal(empire_data *emp, any_vnum vnum);


/**
* This crash-saves all players in the game.
*/
void extract_all_items(char_data *ch);
