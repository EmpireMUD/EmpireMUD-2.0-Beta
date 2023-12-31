/* ************************************************************************
*   File: constants.h                                     EmpireMUD 2.0b5 *
*  Usage: Externs for the many consts in constants.c                      *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

// empiremud constants
extern const char *level_names[][2];
extern const int num_of_reboot_strings;
extern const char *reboot_strings[];
extern const char *reboot_type[];
extern const char *version;

// ability constants
extern const char *ability_actions[];
extern const char *ability_flags[];
extern const char *ability_flag_notes[];
extern const char *ability_type_flags[];
extern const char *ability_type_notes[];
extern const char *ability_target_flags[];
extern const char *ability_custom_types[];
extern const char *ability_custom_type_help;
extern const char *ability_data_types[];
extern const char *ability_effects[];
extern const char *ability_gain_hooks[];
extern const char *ability_hook_types[];
extern const char *ability_move_types[];
extern const char *ability_limitations[];
extern const int ability_limitation_misc[];
extern const char *conjure_words[];

// adventure constants
extern const char *adventure_flags[];
extern const char *adventure_link_flags[];
extern const char *adventure_link_types[];
extern const bool adventure_link_is_location_rule[];
extern const char *adventure_spawn_types[];
extern const char *instance_flags[];
extern const char *room_template_flags[];

// archetype constants
extern const char *archetype_flags[];
extern const char *archetype_types[];
extern const struct archetype_menu_type archetype_menu[];

// attack message constants
extern const char *attack_message_flags[];
extern const char *attack_speed_types[];
extern const char *weapon_types[];

// augment constants
extern const char *augment_types[];
extern const struct augment_type_data augment_info[];
extern const char *augment_flags[];

// class constants
extern const char *class_flags[];

// player constants
extern const char *account_flags[];
extern const char *bonus_bits[];
extern const char *bonus_bit_descriptions[];
extern const char *condition_types[];
extern const char *custom_color_types[];
extern const char *extra_attribute_types[];
extern const char *combat_message_types[];
extern const char *grant_bits[];
extern const char *informative_view_bits[];
extern const char *mount_flags[];
extern const char *player_bits[];
extern const char *preference_bits[];
extern const char *class_role[];
extern const char *class_role_color[];
extern const struct toggle_data_type toggle_data[];
extern const char *connected_types[];
extern const char *player_tech_types[];
extern const char *status_message_types[];
extern const char *syslog_types[];

// direction and room constants
extern const char *dirs[];
extern const char *alt_dirs[];
extern const char *from_dir[];
extern const int shift_dir[][2];
extern const bool can_designate_dir[NUM_OF_DIRS];
extern const bool can_designate_dir_vehicle[NUM_OF_DIRS];
extern const bool can_flee_dir[NUM_OF_DIRS];
extern const bool is_flat_dir[NUM_OF_DIRS];
extern const char *exit_bits[];
extern const int rev_dir[NUM_OF_DIRS];
extern const int confused_dirs[NUM_2D_DIRS][2][NUM_OF_DIRS];
extern const int how_to_show_map[NUM_SIMPLE_DIRS][2];
extern const int show_map_y_first[NUM_SIMPLE_DIRS];

// character constants
extern const char *affected_bits[];
extern const char *affected_bits_consider[];
extern const bool aff_is_bad[];
extern const char *health_levels[];
extern const char *move_levels[];
extern const char *mana_levels[];
extern const char *blood_levels[];
extern const char *genders[];
extern const char *position_types[];
extern const char *position_commands[];
extern const int regen_by_pos[];
extern const char *injury_bits[];
extern const char *apply_type_names[];
extern const bool apply_type_from_player[];
extern const char *apply_types[];
extern const double apply_values[];
extern const int apply_attribute[];
extern const bool apply_never_scales[];
extern const struct attribute_data_type attributes[NUM_ATTRIBUTES];
extern const int attribute_display_order[NUM_ATTRIBUTES];
extern const char *pool_types[];
extern const char *pool_abbrevs[];
extern const char *size_types[];
extern const struct character_size_data size_data[];

// craft recipe constants
extern const char *craft_flags[];
extern const char *craft_flag_for_info[];
extern const char *craft_types[];

// empire constants
extern const struct city_metadata_type city_type[];
extern const char *diplomacy_flags[];
extern const struct offense_info_type offense_info[NUM_OFFENSES];
extern const char *empire_log_types[];
extern const bool show_empire_log_type[];
extern const bool empire_log_request_only[];
extern const char *empire_admin_flags[];
extern const char *empire_attributes[];
extern const char *empire_needs_types[];
extern const char *empire_needs_status[];
extern const char *unique_storage_flags[];
extern const char *offense_flags[];
extern const char *techs[];
extern const char *empire_trait_types[];
extern const char *priv[];
extern const char *score_type[];
extern const char *trade_type[];
extern const char *trade_mostleast[];
extern const char *trade_overunder[];
extern const char *wf_problem_types[];

// event constants
extern const char *event_types[];
extern const char *event_flags[];
extern const char *event_status[];

// faction constants
extern const char *faction_flags[];
extern const char *relationship_flags[];
extern const char *relationship_descs[];
extern const struct faction_reputation_type reputation_levels[];

// generic constants
extern const char *generic_types[];
extern const bool generic_types_uses_in_dev[];
extern const char *generic_flags[];
extern const char *language_types[];
extern const char *liquid_flags[];

// mob constants
extern const char *action_bits[];
extern const char *mob_custom_types[];
extern const char *mob_custom_type_help;
extern const char *mob_move_types[];
extern const char *name_sets[];

// moon constants
extern const char *moon_phases[];
extern const char *moon_phases_long[];
extern const int moon_phase_brightness[NUM_PHASES];
extern const char *moon_positions[];

// item constants
extern const char *wear_keywords[];
extern const struct wear_data_type wear_data[NUM_WEARS];
extern const char *item_types[];
extern const char *wear_bits[];
extern const int wear_significance[];
extern const int item_wear_to_wear[];
extern const char *extra_bits[];
extern const char *extra_bits_inv_flags[];
extern const double obj_flag_scaling_bonus[];
extern const struct material_data materials[NUM_MATERIALS];
extern const char *container_bits[];
extern const char *corpse_flags[];
extern const char *fullness[];
extern const char *item_rename_keywords[];
extern const char *light_flags[];
extern const char *light_flags_for_identify[];
extern const char *mint_flags[];
extern const char *mint_flags_for_identify[];
extern const char *paint_colors[];
extern const char *paint_names[];
extern const char *resource_types[];
extern const char *storage_bits[];
extern const char *tool_flags[];
extern const char *obj_custom_types[];
extern const char *obj_custom_type_help;
extern const double basic_speed;

// olc constants
extern const char *olc_flag_bits[];
extern const char *olc_type_bits[];

// progress constants
extern const char *progress_types[];
extern const char *progress_flags[];
extern const char *progress_perk_types[];

// quest constants
extern const char *quest_flags[];
extern const char *quest_giver_types[];
extern const char *quest_reward_types[];

// room/world constants
extern const char *bld_on_flags[];
extern const bitvector_t bld_on_flags_order[];
extern const char *bld_flags[];
extern const char *bld_relationship_types[];
extern const int bld_relationship_vnum_types[];
extern const char *climate_flags[];
extern const struct climate_temperature_t climate_temperature[];
extern const bitvector_t climate_flags_order[];
extern const bool climate_ruins_vehicle_slowly[][2];
extern const char *crop_flags[];
extern const char *depletion_type[];
extern const char *designate_flags[];
extern const char *evo_types[];
extern const int evo_val_types[NUM_EVOS];
extern const bool evo_is_over_time[];
extern const char *function_flags[];
extern const char *function_flags_long[];
extern const char *island_bits[];
extern const char *mapout_color_names[];
extern const char mapout_color_tokens[];
extern const char banner_to_mapout_token[][2];
extern const char *road_types[];
extern const char *room_aff_bits[];
extern const char *room_extra_types[];
extern const char *sector_flags[];
extern const char *spawn_flags[];
extern const char *spawn_flags_short[];
extern const char *seasons[];
extern const char *icon_types[];
extern const int season_temperature[];
extern const char *sun_types[];
extern const int sun_temperature[];
extern const char *temperature_types[];
extern const char *weather_types[];

// for the second dimension of: climate_ruins_vehicle_slowly[climate][when]
#define CRVS_WHEN_GAINING  0
#define CRVS_WHEN_LOSING  1

// shop constants
extern const char *shop_flags[];

// skill constants
extern const char *damage_types[];
extern const int damage_type_to_dot_attack[];
extern const char *skill_check_difficulty[];
extern const char *skill_flags[];

// social constants
extern const char *social_flags[];
extern const char *social_message_types[NUM_SOCM_MESSAGES][2];

// trigger constants
extern const char *trig_types[];
extern const bitvector_t mtrig_argument_types[];
extern const char *otrig_types[];
extern const bitvector_t otrig_argument_types[];
extern const char *vtrig_types[];
extern const bitvector_t vtrig_argument_types[];
extern const char *wtrig_types[];
extern const bitvector_t wtrig_argument_types[];
extern const char *trig_attach_types[];
extern const char **trig_attach_type_list[];
extern const bitvector_t *trig_argument_type_list[];

// misc constants
extern const char *automessage_types[];
extern const char *fill_words[];
extern const char *global_types[];
extern const char *global_flags[];
extern const char *interact_types[];
extern const int interact_attach_types[NUM_INTERACTS];
extern const byte interact_vnum_types[NUM_INTERACTS];
extern const bool interact_one_at_a_time[NUM_INTERACTS];
extern const char *interact_restriction_types[];
extern const char *morph_flags[];
extern const char *requirement_types[];
extern const bool requirement_amt_type[];
extern const bool requirement_needs_tracker[];
extern const char *reserved_words[];
extern const char *weekdays[];
extern const char *month_name[];
extern const char *offon_types[];
extern const char *shutdown_types[];
extern const char *vehicle_flags[];
extern const char *identify_vehicle_flags[];
extern const char *veh_custom_types[];
extern const char *vehicle_speed_types[];
extern const char *wait_types[];


// act.action.c
extern const struct action_data_struct action_data[];

// act.empire.c
extern const struct tavern_data_type tavern_data[];

// act.movement.c
extern const char *cmd_door[];

// act.trade.c
extern const struct gen_craft_data_t gen_craft_data[];

// config.c
extern const int base_hit_chance;
extern const int base_player_pools[NUM_POOLS];
extern const char *book_name_list[];
extern struct file_lookup_struct file_lookup[];
extern const double hit_per_dex;
extern const int primary_attributes[];
extern struct promo_code_list promo_codes[];
extern const int round_level_scaling_to_nearest;
extern const double score_levels[];
extern const int techs_requiring_same_island[];
extern struct text_file_data_type text_file_data[NUM_TEXT_FILE_STRINGS];
extern const int universal_wait;
