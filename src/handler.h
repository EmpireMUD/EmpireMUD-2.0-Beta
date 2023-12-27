/* ************************************************************************
*   File: handler.h                                       EmpireMUD 2.0b5 *
*  Usage: header file: prototypes of handling and utility functions       *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#define FIND_VEHICLE_WORLD	BIT(9)
#define FIND_NPC_ONLY		BIT(10)	// ignores players


// for match_char_name()
#define MATCH_GLOBAL  BIT(0)	// ignores dark/blind
#define MATCH_IN_ROOM  BIT(1)	// specifically checks for things that only matter in-room


// for the interaction handlers (returns TRUE if the character performs the interaction; FALSE if it aborts)
#define INTERACTION_FUNC(name)	bool (name)(char_data *ch, struct interaction_item *interaction, room_data *inter_room, char_data *inter_mob, obj_data *inter_item, vehicle_data *inter_veh)


// global function types -- for run_globals
#define GLB_VALIDATOR(name)		bool (name)(struct global_data *glb, char_data *ch, void *other_data)
#define GLB_FUNCTION(name)		bool (name)(struct global_data *glb, char_data *ch, void *other_data)


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
void affect_from_room_by_caster(room_data *room, any_vnum type, char_data *caster, bool show_msg);
void affect_join(char_data *ch, struct affected_type *af, int flags);
void affect_modify(char_data *ch, byte loc, sh_int mod, bitvector_t bitv, bool add);
void affect_remove(char_data *ch, struct affected_type *af);
void affect_remove_room(room_data *room, struct affected_type *af);
void affect_to_char_silent(char_data *ch, struct affected_type *af);
void affect_to_char(char_data *ch, struct affected_type *af);
void affect_to_room(room_data *room, struct affected_type *af);
void affect_total(char_data *ch);
void affect_total_room(room_data *room);
bool affected_by_spell(char_data *ch, any_vnum type);
bool affected_by_spell_and_apply(char_data *ch, any_vnum type, int apply, bitvector_t aff_flag);
bool affected_by_spell_from_caster(char_data *ch, any_vnum type, char_data *caster);
struct affected_type *create_aff(any_vnum type, int duration, int location, int modifier, bitvector_t bitvector, char_data *cast_by);
void apply_dot_effect(char_data *ch, any_vnum type, int seconds_duration, sh_int damage_type, sh_int damage, sh_int max_stack, char_data *cast_by);
void cancel_permanent_affects_room(room_data *room);
void dot_remove(char_data *ch, struct over_time_effect_type *dot);
void free_freeable_dots();
void remove_first_aff_flag_from_char(char_data *ch, bitvector_t aff_flag, bool show_msg);
bool room_affected_by_spell(room_data *room, any_vnum type);
bool room_affected_by_spell_from_caster(room_data *room, any_vnum type, char_data *caster);
void schedule_affect_expire(char_data *ch, struct affected_type *af);
void schedule_dot_update(char_data *ch, struct over_time_effect_type *dot);
void show_wear_off_msg(char_data *ch, int atype);

// affect shortcut macros
#define create_flag_aff(type, duration, bit, cast_by)  create_aff((type), (duration), APPLY_NONE, 0, (bit), (cast_by))
#define create_mod_aff(type, duration, loc, mod, cast_by)  create_aff((type), (duration), (loc), (mod), 0, (cast_by))

// character handlers
void add_learned_craft_empire(empire_data *emp, any_vnum vnum);
void die_follower(char_data *ch);
void extract_char(char_data *ch);
void extract_char_final(char_data *ch);
void extract_pending_chars();
bool has_learned_craft(char_data *ch, any_vnum vnum);
bool match_char_name(char_data *ch, char_data *target, char *name, bitvector_t flags, bool *was_exact);
bool perform_idle_out(char_data *ch);

// character location handlers
void char_from_room(char_data *ch);
void char_to_room(char_data *ch, room_data *room);

// character targeting handlers
char_data *find_closest_char(char_data *ch, char *arg, bool pc);
char_data *find_mob_in_room_by_vnum(room_data *room, mob_vnum vnum);
char_data *find_mortal_in_room(room_data *room);
char_data *get_char_by_obj(obj_data *obj, char *name);
char_data *get_char_by_room(room_data *room, char *name);
char_data *get_char_by_vehicle(vehicle_data *veh, char *name);
char_data *get_char_in_room(room_data *room, char *name);
char_data *get_char_room(char *name, room_data *room);
char_data *get_char_room_vis(char_data *ch, char *name, int *number);
char_data *get_char_vis(char_data *ch, char *name, int *number, bitvector_t where);
char_data *get_player_vis(char_data *ch, char *name, bitvector_t flags);
char_data *get_char_world(char *name, int *number);
char_data *get_player_world(char *name, int *number);

// coin handlers
bool can_afford_coins(char_data *ch, empire_data *type, int amount);
void charge_coins(char_data *ch, empire_data *type, int amount, struct resource_data **build_used_list, char *build_string);
void cleanup_all_coins();
void cleanup_coins(char_data *ch);
void coin_string(struct coin_data *list, char *storage);
int count_total_coins_as(char_data *ch, empire_data *type);
obj_data *create_money(empire_data *type, int amount);
#define decrease_coins(ch, emp, amount)  increase_coins(ch, emp, -1 * amount)
double exchange_coin_value(double amount, empire_data *convert_from, empire_data *convert_to);
double exchange_rate(empire_data *from, empire_data *to);
char *find_coin_arg(char *input, empire_data **emp_found, int *amount_found, bool require_coin_type, bool assume_coins, bool *gave_coin_type);
struct coin_data *find_coin_entry(struct coin_data *list, empire_data *emp);
int increase_coins(char_data *ch, empire_data *emp, int amount);
const char *money_amount(empire_data *type, int amount);
const char *money_desc(empire_data *type, int amount);
int total_coins(char_data *ch);

// companion handlers
struct companion_data *add_companion(char_data *ch, any_vnum vnum, any_vnum from_abil);
void add_companion_mod(struct companion_data *companion, int type, int num, char *str);
void add_companion_var(char_data *mob, char *name, char *value, int id);
struct companion_mod *get_companion_mod_by_type(struct companion_data *cd, int type);
struct companion_data *has_companion(char_data *ch, any_vnum vnum);
void remove_companion(char_data *ch, any_vnum vnum);
void remove_companion_mod(struct companion_data **companion, int type);
void remove_companion_var(char_data *mob, char *name, int context);
void reread_companion_trigs(char_data *mob);

// cooldown handlers
void add_cooldown(char_data *ch, any_vnum type, int seconds_duration);
void free_cooldown(struct cooldown_data *cool);
int get_cooldown_time(char_data *ch, any_vnum type);
void remove_cooldown(char_data *ch, struct cooldown_data *cool);
void remove_cooldown_by_type(char_data *ch, any_vnum type);

// currency handlers
int add_currency(char_data *ch, any_vnum vnum, int amount);
int get_currency(char_data *ch, any_vnum vnum);

// empire handlers
void abandon_room(room_data *room);
void claim_room(room_data *room, empire_data *emp);
int count_dropped_items(empire_data *emp, obj_vnum vnum);
struct empire_political_data *create_relation(empire_data *a, empire_data *b);
int find_rank_by_name(empire_data *emp, char *name);
struct empire_political_data *find_relation(empire_data *from, empire_data *to);
struct empire_territory_data *find_territory_entry(empire_data *emp, room_data *room);
struct empire_trade_data *find_trade_entry(empire_data *emp, int type, obj_vnum vnum);
struct empire_production_total *get_production_total_entry(empire_data *emp, any_vnum vnum);
int increase_empire_coins(empire_data *emp_gaining, empire_data *coin_empire, double amount);
#define decrease_empire_coins(emp_gaining, coin_empire, amount)  increase_empire_coins((emp_gaining), (coin_empire), -1 * (amount))
void perform_abandon_room(room_data *room);
void perform_abandon_vehicle(vehicle_data *veh);
void perform_claim_room(room_data *room, empire_data *emp);
void perform_claim_vehicle(vehicle_data *veh, empire_data *emp);
void read_vault(empire_data *emp);
void refresh_empire_dropped_items(empire_data *only_emp);
void remove_homeless_citizen(empire_data *emp, struct empire_homeless_citizen *ehc);
void remove_learned_craft_empire(empire_data *emp, any_vnum vnum, bool full_remove);
int sort_empire_production_totals(struct empire_production_total *a, struct empire_production_total *b);

// empire production total handlers
void add_production_total(empire_data *emp, obj_vnum vnum, int amount);
void add_production_total_for_tag_list(struct mob_tag *list, obj_vnum vnum, int amount);
int get_production_total(empire_data *emp, obj_vnum vnum);
int get_production_total_component(empire_data *emp, any_vnum cmp_vnum);
void mark_production_trade(empire_data *emp, obj_vnum vnum, int imported, int exported);

// empire needs handlers
void add_empire_needs(empire_data *emp, int island, int type, int amount);
struct empire_needs *get_empire_needs(empire_data *emp, int island, int type);
bool empire_has_needs_status(empire_data *emp, int island, int type, bitvector_t status);

// empire npc handlers
void kill_empire_npc(char_data *ch);

// empire targeting handlers
struct empire_city_data *find_city(empire_data *emp, room_data *loc);
struct empire_city_data *find_city_entry(empire_data *emp, room_data *location);
struct empire_city_data *find_city_by_name(empire_data *emp, char *name);
struct empire_city_data *find_closest_city(empire_data *emp, room_data *loc);
empire_data *get_empire_by_name(char *name);

// follow handlers
void add_follower(char_data *ch, char_data *leader, bool msg);
void stop_follower(char_data *ch);

// global handlers
bool run_globals(int glb_type, GLB_FUNCTION(*func), bool allow_many, bitvector_t type_flags, char_data *ch, adv_data *adv, int level, GLB_VALIDATOR(*validator), void *other_data);

// group handlers
int count_group_members(struct group_data *group);
struct group_data *create_group(char_data *leader);
void free_group(struct group_data *group);
bool in_same_group(char_data *ch, char_data *vict);
void join_group(char_data *ch, struct group_data *group);
void leave_group(char_data *ch);

// help file handlers
struct help_index_element *find_help_entry(int level, const char *word);

// interaction handlers
bool can_interact_room(room_data *room, int type);
bool check_exclusion_set(struct interact_exclusion_data **set, char code, double percent);
struct interact_exclusion_data *find_exclusion_data(struct interact_exclusion_data **set, char code);
void free_exclusion_data(struct interact_exclusion_data *list);
int get_interaction_depletion(char_data *ch, empire_data *emp, struct interaction_item *list, int interaction_type, bool require_storable);
int get_interaction_depletion_room(char_data *ch, empire_data *emp, room_data *room, int interaction_type, bool require_storable);
bool has_interaction(struct interaction_item *list, int type);
bool meets_interaction_restrictions(struct interact_restriction *list, char_data *ch, empire_data *emp, char_data *inter_mob, obj_data *inter_item);
bool run_global_mob_interactions(char_data *ch, char_data *mob, int type, INTERACTION_FUNC(*func));
bool run_global_obj_interactions(char_data *ch, obj_data *obj, int type, INTERACTION_FUNC(*func));
bool run_interactions(char_data *ch, struct interaction_item *run_list, int type, room_data *inter_room, char_data *inter_mob, obj_data *inter_item, vehicle_data *inter_veh, INTERACTION_FUNC(*func));
bool run_room_interactions(char_data *ch, room_data *room, int type, vehicle_data *inter_veh, int access_type, INTERACTION_FUNC(*func));

// lore handlers
void add_lore(char_data *ch, int type, const char *str, ...) __attribute__((format(printf, 3, 4)));
void remove_lore(char_data *ch, int type);
void remove_recent_lore(char_data *ch, int type);

// mob tagging handlers
bool find_id_in_tag_list(int id, struct mob_tag *list);
void expand_mob_tags(char_data *mob);
void free_mob_tags(struct mob_tag **list);
void tag_mob(char_data *mob, char_data *player);

// mount handlers
void add_mount(char_data *ch, mob_vnum vnum, bitvector_t flags);
bitvector_t get_mount_flags_by_mob(char_data *mob);
struct mount_data *find_mount_data(char_data *ch, mob_vnum vnum);
void perform_dismount(char_data *ch);
void perform_mount(char_data *ch, char_data *mount);

// object handlers
void add_to_object_list(obj_data *obj);
void apply_obj_light(obj_data *obj, bool add);
struct obj_binding *copy_obj_bindings(struct obj_binding *from);
obj_data *copy_warehouse_obj(obj_data *input);
void empty_obj_before_extract(obj_data *obj);
void extract_obj(obj_data *obj);
room_data *find_room_obj_saves_in(obj_data *obj);
obj_data *fresh_copy_obj(obj_data *obj, int scale_level, bool keep_strings, bool keep_augments);
bool identical_bindings(obj_data *obj_a, obj_data *obj_b);
bool objs_are_identical(obj_data *obj_a, obj_data *obj_b);
void remove_from_object_list(obj_data *obj);

// object binding handlers
void bind_obj_to_group(obj_data *obj, struct group_data *group);
void bind_obj_to_player(obj_data *obj, char_data *ch);
void bind_obj_to_tag_list(obj_data *obj, struct mob_tag *list);
bool bind_ok(obj_data *obj, char_data *ch);
bool bind_ok_idnum(obj_data *obj, int idnum);
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
obj_data *unequip_char(char_data *ch, int pos);
obj_data *unequip_char_to_inventory(char_data *ch, int pos);
obj_data *unequip_char_to_room(char_data *ch, int pos);

// custom message handlers
struct custom_message *copy_custom_messages(struct custom_message *from);
int count_custom_messages(struct custom_message *list, int type);
void free_custom_messages(struct custom_message *mes);
char *get_custom_message(struct custom_message *list, int type);
char *get_custom_message_pos(struct custom_message *list, int type, int pos);
int get_custom_message_random_pos_number(struct custom_message *list, int type);
bool has_custom_message(struct custom_message *list, int type);
bool has_custom_message_pos(struct custom_message *list, int type, int pos);

// custom message helpers
#define mob_get_custom_message(mob, type)  get_custom_message(MOB_CUSTOM_MSGS(mob), type)
#define mob_has_custom_message(mob, type)  has_custom_message(MOB_CUSTOM_MSGS(mob), type)
#define obj_get_custom_message(obj, type)  get_custom_message(GET_OBJ_CUSTOM_MSGS(obj), type)
#define obj_has_custom_message(obj, type)  has_custom_message(GET_OBJ_CUSTOM_MSGS(obj), type)
#define abil_get_custom_message(abil, type)  get_custom_message(ABIL_CUSTOM_MSGS(abil), type)
#define abil_has_custom_message(abil, type)  has_custom_message(ABIL_CUSTOM_MSGS(abil), type)
#define veh_get_custom_message(veh, type)  get_custom_message(VEH_CUSTOM_MSGS(veh), type)
#define veh_has_custom_message(veh, type)  has_custom_message(VEH_CUSTOM_MSGS(veh), type)

// object targeting handlers
obj_data *get_component_in_list(any_vnum cmp_vnum, obj_data *list, bool *kept);
obj_data *get_obj_by_char_share(char_data *ch, char *arg);
obj_data *get_obj_by_obj(obj_data *obj, char *name);
obj_data *get_obj_by_room(room_data *room, char *name);
obj_data *get_obj_by_vehicle(vehicle_data *veh, char *name);
obj_data *get_obj_in_list_num(int num, obj_data *list);
obj_data *get_obj_in_list_vnum(obj_vnum vnum, obj_data *list);
obj_data *get_obj_in_list_vis(char_data *ch, char *name, int *number, obj_data *list);
obj_data *get_obj_in_list_vis_prefer_interaction(char_data *ch, char *name, int *number, obj_data *list, int interact_type);
obj_data *get_obj_in_list_vis_prefer_type(char_data *ch, char *name, int *number, obj_data *list, int obj_type);
obj_data *get_obj_in_room(room_data *room, char *name);
int get_obj_pos_in_equip_vis(char_data *ch, char *arg, int *number, obj_data *equipment[]);
obj_vnum get_obj_vnum_by_name(char *name, bool storable_only);
obj_data *get_obj_vis(char_data *ch, char *name, int *number);
obj_data *get_obj_in_equip_vis(char_data *ch, char *arg, int *number, obj_data *equipment[], int *pos);
obj_data *get_obj_world(char *name, int *number);

// offer handlers
struct offer_data *add_offer(char_data *ch, char_data *from, int type, int data);
void remove_offers_by_type(char_data *ch, int type);

// player list handlers
void add_learned_craft(char_data *ch, any_vnum vnum);
void add_minipet(char_data *ch, any_vnum vnum);
void clean_offers(char_data *ch);
bool has_minipet(char_data *ch, any_vnum vnum);
void remove_learned_craft(char_data *ch, any_vnum vnum);
void remove_minipet(char_data *ch, any_vnum vnum);

// player tech handlers
void add_player_tech(char_data *ch, any_vnum abil, int tech);
bool has_player_tech(char_data *ch, int tech);
void remove_player_tech(char_data *ch, any_vnum abil);
bool run_ability_triggers_by_player_tech(char_data *ch, int tech, char_data *cvict, obj_data *ovict);

// requirement handlers
struct req_data *copy_requirements(struct req_data *from);
bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
void extract_required_items(char_data *ch, struct req_data *list);
bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);
void free_requirements(struct req_data *list);
bool meets_requirements(char_data *ch, struct req_data *list, struct instance_data *instance);
char *requirement_string(struct req_data *task, bool show_vnums, bool allow_custom);

// resource handlers
bool remove_thing_from_resource_list(struct resource_data **list, int type, any_vnum vnum);

// resource depletion handlers
void add_depletion(room_data *room, int type, bool multiple);
void clear_depletions(room_data *room);
#define add_vehicle_depletion(veh, type, multiple)  do { perform_add_depletion(&VEH_DEPLETION(veh), (type), (multiple)); request_vehicle_save_in_world(veh); } while (0)
int get_depletion_amount(struct depletion_data *list, int type);
#define get_depletion(room, type)  get_depletion_amount(ROOM_DEPLETION(room), (type))
#define get_vehicle_depletion(veh, type)  get_depletion_amount(VEH_DEPLETION(veh), (type))
void perform_add_depletion(struct depletion_data **list, int type, bool multiple);
bool remove_depletion_from_list(struct depletion_data **list, int type);
void remove_depletion(room_data *room, int type);
void set_depletion(struct depletion_data **list, int type, int value);
#define set_room_depletion(room, type, amount) {	\
		if (room && SHARED_DATA(room) != &ocean_shared_data) {	\
			set_depletion(&ROOM_DEPLETION(room), (type), (amount));	\
			request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);	\
		}	\
	}
#define set_vehicle_depletion(veh, type, amount) {	\
		if (veh) {	\
			set_depletion(&VEH_DEPLETION(veh), (type), (amount));	\
			request_vehicle_save_in_world(veh);	\
		}	\
	}

// room handlers
void attach_building_to_room(bld_data *bld, room_data *room, bool with_triggers);
void attach_template_to_room(room_template *rmt, room_data *room);
void detach_building_from_room(room_data *room);
void reset_light_count(room_data *room);

// room extra data handlers
void add_to_extra_data(struct room_extra_data **list, int type, int add_value);
struct room_extra_data *find_extra_data(struct room_extra_data *list, int type);
void free_extra_data(struct room_extra_data **hash);
int get_extra_data(struct room_extra_data *list, int type);
void multiply_extra_data(struct room_extra_data **list, int type, double multiplier);
void remove_extra_data(struct room_extra_data **list, int type);
void set_extra_data(struct room_extra_data **list, int type, int value);

// room extra data helpers (backwards-compatibility and shortcuts)
#define add_to_room_extra_data(room, type, add_value)  do { add_to_extra_data(&ROOM_EXTRA_DATA(room), (type), (add_value)); request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM); } while (0)
#define find_room_extra_data(room, type)  find_extra_data(ROOM_EXTRA_DATA(room), type)
#define get_room_extra_data(room, type)  get_extra_data(ROOM_EXTRA_DATA(room), type)
#define multiply_room_extra_data(room, type, multiplier)  do { multiply_extra_data(&ROOM_EXTRA_DATA(room), (type), (multiplier)); request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM); } while (0)
#define remove_room_extra_data(room, type)  do { remove_extra_data(&ROOM_EXTRA_DATA(room), (type)); request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM); } while (0)
#define set_room_extra_data(room, type, value)  do { set_extra_data(&ROOM_EXTRA_DATA(room), (type), (value)); request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM); } while (0)

// vehicle extra data helpers
#define add_to_vehicle_extra_data(veh, type, add_value)  do { add_to_extra_data(&VEH_EXTRA_DATA(veh), type, add_value); request_vehicle_save_in_world(veh); } while (0)
#define find_vehicle_extra_data(veh, type)  find_extra_data(VEH_EXTRA_DATA(veh), type)
#define get_vehicle_extra_data(veh, type)  get_extra_data(VEH_EXTRA_DATA(veh), type)
#define multiply_vehicle_extra_data(veh, type, multiplier)  do { multiply_extra_data(&VEH_EXTRA_DATA(veh), type, multiplier); request_vehicle_save_in_world(veh); } while (0)
#define remove_vehicle_extra_data(veh, type)  do { remove_extra_data(&VEH_EXTRA_DATA(veh), type); request_vehicle_save_in_world(veh); } while (0)
#define set_vehicle_extra_data(veh, type, value)  do { set_extra_data(&VEH_EXTRA_DATA(veh), type, value); request_vehicle_save_in_world(veh); } while (0)

// room targeting handlers
room_data *find_target_room(char_data *ch, char *rawroomstr);
room_data *parse_room_from_coords(char *string);

// sector handlers
bool check_evolution_percent(struct evolution_data *evo);
struct evolution_data *get_evolution_by_type(sector_data *st, int type);
bool has_evolution_type(sector_data *st, int type);
sector_data *reverse_lookup_evolution_for_sector(sector_data *in_sect, int evo_type);

// storage handlers
struct empire_storage_data *add_to_empire_storage(empire_data *emp, int island, obj_vnum vnum, int amount);
bool charge_stored_component(empire_data *emp, int island, any_vnum cmp_vnum, int amount, bool use_kept, bool basic_only, struct resource_data **build_used_list);
bool charge_stored_resource(empire_data *emp, int island, obj_vnum vnum, int amount);
bool check_home_store_cap(char_data *ch, obj_data *obj, bool message, bool *capped);
bool delete_stored_resource(empire_data *emp, obj_vnum vnum);
bool empire_can_afford_component(empire_data *emp, int island, any_vnum cmp_vnum, int amount, bool include_kept, bool basic_only);
struct empire_storage_data *find_island_storage_by_keywords(empire_data *emp, int island_id, char *keywords);
struct empire_storage_data *find_stored_resource(empire_data *emp, int island, obj_vnum vnum);
int get_total_stored_count(empire_data *emp, obj_vnum vnum, bool count_secondary);
bool obj_can_be_stored(obj_data *obj, room_data *loc, empire_data *by_emp, bool retrieval_mode);
#define obj_can_be_retrieved(obj, loc, by_emp)  obj_can_be_stored((obj), (loc), (by_emp), TRUE)
bool retrieve_resource(char_data *ch, empire_data *emp, struct empire_storage_data *store, bool stolen);
int store_resource(char_data *ch, empire_data *emp, obj_data *obj);
bool stored_item_requires_withdraw(obj_data *obj);

// targeting handlers
int find_all_dots(char *arg);
bitvector_t generic_find(char *arg, int *number, bitvector_t bitvector, char_data *ch, char_data **tar_ch, obj_data **tar_obj, vehicle_data **tar_veh);
int get_number(char **name);

// trading post handlers
void expire_trading_post_item(struct trading_post_data *tpd);

// unique storage handlers
bool delete_unique_storage_by_vnum(struct empire_unique_storage **list, obj_vnum vnum);
struct empire_unique_storage *find_eus_entry(obj_data *obj, struct empire_unique_storage *list, room_data *location);
void store_unique_item(char_data *ch, struct empire_unique_storage **to_list, obj_data *obj, empire_data *save_emp, room_data *room, bool *full);

// vehicle handlers
void extract_pending_vehicles();
void extract_vehicle(vehicle_data *veh);
void sit_on_vehicle(char_data *ch, vehicle_data *veh);
void unseat_char_from_vehicle(char_data *ch);
void vehicle_from_room(vehicle_data *veh);
void vehicle_to_room(vehicle_data *veh, room_data *room);

// vehicle targeting handlers
vehicle_data *get_vehicle(char *name);
vehicle_data *get_vehicle_by_obj(obj_data *obj, char *name);
vehicle_data *get_vehicle_by_vehicle(vehicle_data *veh, char *name);
vehicle_data *get_vehicle_in_target_room_vis(char_data *ch, room_data *room, char *name, int *number);
vehicle_data *get_vehicle_near_obj(obj_data *obj, char *name);
vehicle_data *get_vehicle_near_vehicle(vehicle_data *veh, char *name);
#define get_vehicle_in_room_vis(ch, name, number)  get_vehicle_in_target_room_vis((ch), IN_ROOM(ch), (name), (number))
vehicle_data *get_vehicle_vis(char_data *ch, char *name, int *number);
vehicle_data *get_vehicle_room(room_data *room, char *name, int *number);
vehicle_data *get_vehicle_world(char *name, int *number);
vehicle_data *get_vehicle_world_vis(char_data *ch, char *name, int *number);

// world handlers
struct room_direction_data *find_exit(room_data *room, int dir);
int get_direction_for_char(char_data *ch, int dir);
room_data *get_room(room_data *ref, char *name);
room_data *obj_room(obj_data *obj);
int parse_direction(char_data *ch, char *dir);
void schedule_room_affect_expire(room_data *room, struct affected_type *af);


 //////////////////////////////////////////////////////////////////////////////
//// handlers from other files ///////////////////////////////////////////////

// act.item.c
int perform_drop(char_data *ch, obj_data *obj, byte mode, const char *sname);
obj_data *perform_remove(char_data *ch, int pos);

// books.c
book_data *book_proto(book_vnum vnum);
book_data *find_book_by_author(char *argument, int idnum);
book_data *find_book_in_library(char *argument, room_data *room);

// config.c
bitvector_t config_get_bitvector(char *key);
bool config_get_bool(char *key);
double config_get_double(char *key);
int config_get_int(char *key);
int *config_get_int_array(char *key, int *array_size);
const char *config_get_string(char *key);

// faction.c
int compare_reptuation(int rep_a, int rep_b);
void gain_reputation(char_data *ch, any_vnum vnum, int amount, bool is_kill, bool cascade);
struct player_faction_data *get_reputation(char_data *ch, any_vnum vnum, bool create);
int get_reputation_by_name(char *name);
int get_reputation_value(char_data *ch, any_vnum vnum);
bool has_reputation(char_data *ch, any_vnum faction, int rep);
int rep_const_to_index(int rep_const);
void set_reputation(char_data *ch, any_vnum vnum, int rep);

// fight.c
void appear(char_data *ch);
bool can_fight(char_data *ch, char_data *victim);
int damage(char_data *ch, char_data *victim, int dam, int attacktype, byte damtype, attack_message_data *custom_fight_messages);
obj_data *die(char_data *ch, char_data *killer);
void engage_combat(char_data *ch, char_data *vict, bool melee);
void heal(char_data *ch, char_data *vict, int amount);
int hit(char_data *ch, char_data *victim, obj_data *weapon, bool combat_round);
bool is_fighting(char_data *ch);
void set_fighting(char_data *ch, char_data *victim, byte mode);
void stop_fighting(char_data *ch);
void update_pos(char_data *victim);

// instance.c
void add_instance_mob(struct instance_data *inst, mob_vnum vnum);
struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom, bool allow_fake_loc);
struct instance_data *real_instance(any_vnum instance_id);
void subtract_instance_mob(struct instance_data *inst, mob_vnum vnum);

// limits.c
INTERACTION_FUNC(consumes_or_decays_interact);
int limit_crowd_control(char_data *victim, int atype);

// morph.c
void perform_morph(char_data *ch, morph_data *morph);

// objsave.c

// progress.c
void cancel_empire_goal(empire_data *emp, struct empire_goal *goal);
struct empire_goal *get_current_goal(empire_data *emp, any_vnum vnum);
bool empire_has_completed_goal(empire_data *emp, any_vnum vnum);
time_t when_empire_completed_goal(empire_data *emp, any_vnum vnum);


/**
* This crash-saves all players in the game.
*/
void extract_all_items(char_data *ch);
