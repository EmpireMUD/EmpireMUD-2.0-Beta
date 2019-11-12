/* ************************************************************************
*   File: olc.c                                           EmpireMUD 2.0b5 *
*  Usage: On-Line Creation at player level                                *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */
#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "comm.h"
#include "olc.h"
#include "dg_scripts.h"

/**
* Contents:
*   Core Command
*   Common Modules
*   Common Displays
*   Auditors
*   Helpers
*/

// local modules
OLC_MODULE(olc_abort);
OLC_MODULE(olc_audit);
OLC_MODULE(olc_copy);
OLC_MODULE(olc_delete);
OLC_MODULE(olc_display);
OLC_MODULE(olc_edit);
OLC_MODULE(olc_fullsearch);
OLC_MODULE(olc_free);
OLC_MODULE(olc_list);
OLC_MODULE(olc_removeindev);
OLC_MODULE(olc_save);
OLC_MODULE(olc_search);
OLC_MODULE(olc_set_flags);
OLC_MODULE(olc_set_max_vnum);
OLC_MODULE(olc_set_min_vnum);

// ability modules
OLC_MODULE(abiledit_affects);
OLC_MODULE(abiledit_affectvnum);
OLC_MODULE(abiledit_apply);
OLC_MODULE(abiledit_attacktype);
OLC_MODULE(abiledit_cdtime);
OLC_MODULE(abiledit_command);
OLC_MODULE(abiledit_cooldown);
OLC_MODULE(abiledit_cost);
OLC_MODULE(abiledit_costperscalepoint);
OLC_MODULE(abiledit_costtype);
OLC_MODULE(abiledit_custom);
OLC_MODULE(abiledit_damagetype);
OLC_MODULE(abiledit_data);
OLC_MODULE(abiledit_difficulty);
OLC_MODULE(abiledit_flags);
OLC_MODULE(abiledit_gainhooks);
OLC_MODULE(abiledit_immunities);
OLC_MODULE(abiledit_linkedtrait);
OLC_MODULE(abiledit_longduration);
OLC_MODULE(abiledit_masteryability);
OLC_MODULE(abiledit_maxstacks);
OLC_MODULE(abiledit_minposition);
OLC_MODULE(abiledit_name);
OLC_MODULE(abiledit_scale);
OLC_MODULE(abiledit_shortduration);
OLC_MODULE(abiledit_targets);
OLC_MODULE(abiledit_types);
OLC_MODULE(abiledit_waittype);

// adventure zone modules
OLC_MODULE(advedit_author);
OLC_MODULE(advedit_cascade);
OLC_MODULE(advedit_description);
OLC_MODULE(advedit_endvnum);
OLC_MODULE(advedit_flags);
OLC_MODULE(advedit_limit);
OLC_MODULE(advedit_linking);
OLC_MODULE(advedit_maxlevel);
OLC_MODULE(advedit_minlevel);
OLC_MODULE(advedit_name);
OLC_MODULE(advedit_playerlimit);
OLC_MODULE(advedit_reset);
OLC_MODULE(advedit_script);
OLC_MODULE(advedit_startvnum);
OLC_MODULE(advedit_uncascade);

// archetype modules
OLC_MODULE(archedit_attribute);
OLC_MODULE(archedit_description);
OLC_MODULE(archedit_femalerank);
OLC_MODULE(archedit_flags);
OLC_MODULE(archedit_gear);
OLC_MODULE(archedit_lore);
OLC_MODULE(archedit_malerank);
OLC_MODULE(archedit_name);
OLC_MODULE(archedit_skill);
OLC_MODULE(archedit_type);

// augment modules
OLC_MODULE(augedit_ability);
OLC_MODULE(augedit_apply);
OLC_MODULE(augedit_flags);
OLC_MODULE(augedit_name);
OLC_MODULE(augedit_requiresobject);
OLC_MODULE(augedit_resource);
OLC_MODULE(augedit_type);
OLC_MODULE(augedit_wear);

// book modules
OLC_MODULE(booked_author);
OLC_MODULE(booked_byline);
OLC_MODULE(booked_item_description);
OLC_MODULE(booked_item_name);
OLC_MODULE(booked_license);
OLC_MODULE(booked_paragraphs);
OLC_MODULE(booked_title);

// building modules
OLC_MODULE(bedit_affects);
OLC_MODULE(bedit_artisan);
OLC_MODULE(bedit_citizens);
OLC_MODULE(bedit_commands);
OLC_MODULE(bedit_description);
OLC_MODULE(bedit_designate);
OLC_MODULE(bedit_extra_desc);
OLC_MODULE(bedit_extrarooms);
OLC_MODULE(bedit_fame);
OLC_MODULE(bedit_flags);
OLC_MODULE(bedit_functions);
OLC_MODULE(bedit_hitpoints);
OLC_MODULE(bedit_icon);
OLC_MODULE(bedit_interaction);
OLC_MODULE(bedit_military);
OLC_MODULE(bedit_name);
OLC_MODULE(bedit_relations);
OLC_MODULE(bedit_resource);
OLC_MODULE(bedit_script);
OLC_MODULE(bedit_spawns);
OLC_MODULE(bedit_title);

// class modules
OLC_MODULE(classedit_abbrev);
OLC_MODULE(classedit_flags);
OLC_MODULE(classedit_maxhealth);
OLC_MODULE(classedit_maxmana);
OLC_MODULE(classedit_maxmoves);
OLC_MODULE(classedit_name);
OLC_MODULE(classedit_requires);
OLC_MODULE(classedit_role);

// craft modules
OLC_MODULE(cedit_ability);
OLC_MODULE(cedit_buildfacing);
OLC_MODULE(cedit_buildon);
OLC_MODULE(cedit_builds);
OLC_MODULE(cedit_creates);
OLC_MODULE(cedit_flags);
OLC_MODULE(cedit_levelrequired);
OLC_MODULE(cedit_liquid);
OLC_MODULE(cedit_name);
OLC_MODULE(cedit_quantity);
OLC_MODULE(cedit_requiresobject);
OLC_MODULE(cedit_resource);
OLC_MODULE(cedit_time);
OLC_MODULE(cedit_type);
OLC_MODULE(cedit_volume);

// crop modules
OLC_MODULE(cropedit_climate);
OLC_MODULE(cropedit_flags);
OLC_MODULE(cropedit_icons);
OLC_MODULE(cropedit_interaction);
OLC_MODULE(cropedit_mapout);
OLC_MODULE(cropedit_name);
OLC_MODULE(cropedit_spawns);
OLC_MODULE(cropedit_title);
OLC_MODULE(cropedit_xmax);
OLC_MODULE(cropedit_xmin);
OLC_MODULE(cropedit_ymax);
OLC_MODULE(cropedit_ymin);

// event modules
OLC_MODULE(evedit_completemessage);
OLC_MODULE(evedit_description);
OLC_MODULE(evedit_duration);
OLC_MODULE(evedit_flags);
OLC_MODULE(evedit_name);
OLC_MODULE(evedit_maxlevel);
OLC_MODULE(evedit_minlevel);
OLC_MODULE(evedit_notes);
OLC_MODULE(evedit_rankrewards);
OLC_MODULE(evedit_repeat);
OLC_MODULE(evedit_thresholdrewards);

// faction modules
OLC_MODULE(fedit_description);
OLC_MODULE(fedit_flags);
OLC_MODULE(fedit_matchrelations);
OLC_MODULE(fedit_maxreputation);
OLC_MODULE(fedit_minreputation);
OLC_MODULE(fedit_name);
OLC_MODULE(fedit_relation);
OLC_MODULE(fedit_startingreputation);

// generic modules
OLC_MODULE(genedit_flags);
OLC_MODULE(genedit_name);
OLC_MODULE(genedit_type);
OLC_MODULE(genedit_color);
OLC_MODULE(genedit_drunk);
OLC_MODULE(genedit_hunger);
OLC_MODULE(genedit_liquid);
OLC_MODULE(genedit_thirst);
OLC_MODULE(genedit_apply2char);
OLC_MODULE(genedit_apply2room);
OLC_MODULE(genedit_build2char);
OLC_MODULE(genedit_build2room);
OLC_MODULE(genedit_craft2char);
OLC_MODULE(genedit_craft2room);
OLC_MODULE(genedit_repair2char);
OLC_MODULE(genedit_repair2room);
OLC_MODULE(genedit_quick_cooldown);
OLC_MODULE(genedit_standardwearoff);
OLC_MODULE(genedit_wearoff);
OLC_MODULE(genedit_wearoff2room);
OLC_MODULE(genedit_plural);
OLC_MODULE(genedit_singular);

// global modules
OLC_MODULE(gedit_ability);
OLC_MODULE(gedit_capacity);
OLC_MODULE(gedit_flags);
OLC_MODULE(gedit_gear);
OLC_MODULE(gedit_interaction);
OLC_MODULE(gedit_maxlevel);
OLC_MODULE(gedit_minlevel);
OLC_MODULE(gedit_mobexclude);
OLC_MODULE(gedit_mobflags);
OLC_MODULE(gedit_name);
OLC_MODULE(gedit_percent);
OLC_MODULE(gedit_sectorexclude);
OLC_MODULE(gedit_sectorflags);
OLC_MODULE(gedit_type);

// mob edit modules
OLC_MODULE(medit_affects);
OLC_MODULE(medit_allegiance);
OLC_MODULE(medit_attack);
OLC_MODULE(medit_custom);
OLC_MODULE(medit_flags);
OLC_MODULE(medit_interaction);
OLC_MODULE(medit_keywords);
OLC_MODULE(medit_longdescription);
OLC_MODULE(medit_lookdescription);
OLC_MODULE(medit_maxlevel);
OLC_MODULE(medit_minlevel);
OLC_MODULE(medit_movetype);
OLC_MODULE(medit_nameset);
OLC_MODULE(medit_script);
OLC_MODULE(medit_sex);
OLC_MODULE(medit_size);
OLC_MODULE(medit_short_description);

// map modules
OLC_MODULE(mapedit_build);
OLC_MODULE(mapedit_complete_room);
OLC_MODULE(mapedit_decay);
OLC_MODULE(mapedit_decustomize);
OLC_MODULE(mapedit_delete_exit);
OLC_MODULE(mapedit_delete_room);
OLC_MODULE(mapedit_exits);
OLC_MODULE(mapedit_grow);
OLC_MODULE(mapedit_icon);
OLC_MODULE(mapedit_maintain);
OLC_MODULE(mapedit_naturalize);
OLC_MODULE(mapedit_pass_walls);
OLC_MODULE(mapedit_populate);
OLC_MODULE(mapedit_remember);
OLC_MODULE(mapedit_room_description);
OLC_MODULE(mapedit_room_name);
OLC_MODULE(mapedit_roomtype);
OLC_MODULE(mapedit_ruin);
OLC_MODULE(mapedit_terrain);
OLC_MODULE(mapedit_unclaimable);

// morph modules
OLC_MODULE(morphedit_ability);
OLC_MODULE(morphedit_affects);
OLC_MODULE(morphedit_apply);
OLC_MODULE(morphedit_attack);
OLC_MODULE(morphedit_cost);
OLC_MODULE(morphedit_costtype);
OLC_MODULE(morphedit_flags);
OLC_MODULE(morphedit_keywords);
OLC_MODULE(morphedit_longdesc);
OLC_MODULE(morphedit_lookdescription);
OLC_MODULE(morphedit_maxlevel);
OLC_MODULE(morphedit_movetype);
OLC_MODULE(morphedit_requiresobject);
OLC_MODULE(morphedit_shortdesc);
OLC_MODULE(morphedit_size);

// object modules
OLC_MODULE(oedit_action_desc);
OLC_MODULE(oedit_affects);
OLC_MODULE(oedit_affecttype);
OLC_MODULE(oedit_apply);
OLC_MODULE(oedit_armortype);
OLC_MODULE(oedit_ammotype);
OLC_MODULE(oedit_automint);
OLC_MODULE(oedit_capacity);
OLC_MODULE(oedit_cdtime);
OLC_MODULE(oedit_charges);
OLC_MODULE(oedit_coinamount);
OLC_MODULE(oedit_compflags);
OLC_MODULE(oedit_component);
OLC_MODULE(oedit_containerflags);
OLC_MODULE(oedit_contents);
OLC_MODULE(oedit_cooldown);
OLC_MODULE(oedit_corpseof);
OLC_MODULE(oedit_currency);
OLC_MODULE(oedit_custom);
OLC_MODULE(oedit_damage);
OLC_MODULE(oedit_extra_desc);
OLC_MODULE(oedit_flags);
OLC_MODULE(oedit_fullness);
OLC_MODULE(oedit_interaction);
OLC_MODULE(oedit_keywords);
OLC_MODULE(oedit_liquid);
OLC_MODULE(oedit_long_desc);
OLC_MODULE(oedit_material);
OLC_MODULE(oedit_maxlevel);
OLC_MODULE(oedit_minipet);
OLC_MODULE(oedit_minlevel);
OLC_MODULE(oedit_paint);
OLC_MODULE(oedit_plants);
OLC_MODULE(oedit_quantity);
OLC_MODULE(oedit_quick_recipe);
OLC_MODULE(oedit_recipe);
OLC_MODULE(oedit_requiresquest);
OLC_MODULE(oedit_roomvnum);
OLC_MODULE(oedit_script);
OLC_MODULE(oedit_short_description);
OLC_MODULE(oedit_size);
OLC_MODULE(oedit_storage);
OLC_MODULE(oedit_text);
OLC_MODULE(oedit_timer);
OLC_MODULE(oedit_type);
OLC_MODULE(oedit_uses);
OLC_MODULE(oedit_value0);
OLC_MODULE(oedit_value1);
OLC_MODULE(oedit_value2);
OLC_MODULE(oedit_wealth);
OLC_MODULE(oedit_weapontype);
OLC_MODULE(oedit_wear);

// progression modules
OLC_MODULE(progedit_cost);
OLC_MODULE(progedit_description);
OLC_MODULE(progedit_flags);
OLC_MODULE(progedit_name);
OLC_MODULE(progedit_perks);
OLC_MODULE(progedit_prereqs);
OLC_MODULE(progedit_tasks);
OLC_MODULE(progedit_type);
OLC_MODULE(progedit_value);

// quests
OLC_MODULE(qedit_completemessage);
OLC_MODULE(qedit_dailycycle);
OLC_MODULE(qedit_description);
OLC_MODULE(qedit_ends);
OLC_MODULE(qedit_flags);
OLC_MODULE(qedit_name);
OLC_MODULE(qedit_maxlevel);
OLC_MODULE(qedit_minlevel);
OLC_MODULE(qedit_prereqs);
OLC_MODULE(qedit_repeat);
OLC_MODULE(qedit_rewards);
OLC_MODULE(qedit_script);
OLC_MODULE(qedit_starts);
OLC_MODULE(qedit_tasks);

// room template
OLC_MODULE(rmedit_affects);
OLC_MODULE(rmedit_description);
OLC_MODULE(rmedit_exit);
OLC_MODULE(rmedit_extra_desc);
OLC_MODULE(rmedit_interaction);
OLC_MODULE(rmedit_flags);
OLC_MODULE(rmedit_functions);
OLC_MODULE(rmedit_matchexits);
OLC_MODULE(rmedit_title);
OLC_MODULE(rmedit_script);
OLC_MODULE(rmedit_spawns);

// sector modules
OLC_MODULE(sectedit_buildflags);
OLC_MODULE(sectedit_climate);
OLC_MODULE(sectedit_commands);
OLC_MODULE(sectedit_evolution);
OLC_MODULE(sectedit_flags);
OLC_MODULE(sectedit_icons);
OLC_MODULE(sectedit_interaction);
OLC_MODULE(sectedit_mapout);
OLC_MODULE(sectedit_movecost);
OLC_MODULE(sectedit_name);
OLC_MODULE(sectedit_roadsideicon);
OLC_MODULE(sectedit_spawns);
OLC_MODULE(sectedit_title);

// shops
OLC_MODULE(shopedit_allegiance);
OLC_MODULE(shopedit_closes);
OLC_MODULE(shopedit_flags);
OLC_MODULE(shopedit_items);
OLC_MODULE(shopedit_locations);
OLC_MODULE(shopedit_name);
OLC_MODULE(shopedit_opens);

// skill modules
OLC_MODULE(skilledit_abbrev);
OLC_MODULE(skilledit_description);
OLC_MODULE(skilledit_flags);
OLC_MODULE(skilledit_maxlevel);
OLC_MODULE(skilledit_mindrop);
OLC_MODULE(skilledit_name);
OLC_MODULE(skilledit_showsynergies);
OLC_MODULE(skilledit_showtree);
OLC_MODULE(skilledit_synergy);
OLC_MODULE(skilledit_tree);

// social modules
OLC_MODULE(socedit_charposition);
OLC_MODULE(socedit_command);
OLC_MODULE(socedit_flags);
OLC_MODULE(socedit_name);
OLC_MODULE(socedit_requirements);
OLC_MODULE(socedit_targetposition);
OLC_MODULE(socedit_n2char);
OLC_MODULE(socedit_n2other);
OLC_MODULE(socedit_s2char);
OLC_MODULE(socedit_s2other);
OLC_MODULE(socedit_t2char);
OLC_MODULE(socedit_t2vict);
OLC_MODULE(socedit_t2other);
OLC_MODULE(socedit_tnotfound);

// trigger modules
OLC_MODULE(tedit_argtype);
OLC_MODULE(tedit_attaches);
OLC_MODULE(tedit_commands);
OLC_MODULE(tedit_costs);
OLC_MODULE(tedit_location);
OLC_MODULE(tedit_name);
OLC_MODULE(tedit_percent);
OLC_MODULE(tedit_string);
OLC_MODULE(tedit_types);

// vehicle modules
OLC_MODULE(vedit_animalsrequired);
OLC_MODULE(vedit_capacity);
OLC_MODULE(vedit_designate);
OLC_MODULE(vedit_extra_desc);
OLC_MODULE(vedit_extrarooms);
OLC_MODULE(vedit_fame);
OLC_MODULE(vedit_flags);
OLC_MODULE(vedit_functions);
OLC_MODULE(vedit_hitpoints);
OLC_MODULE(vedit_icon);
OLC_MODULE(vedit_interaction);
OLC_MODULE(vedit_interiorroom);
OLC_MODULE(vedit_keywords);
OLC_MODULE(vedit_longdescription);
OLC_MODULE(vedit_lookdescription);
OLC_MODULE(vedit_maxlevel);
OLC_MODULE(vedit_military);
OLC_MODULE(vedit_minlevel);
OLC_MODULE(vedit_movetype);
OLC_MODULE(vedit_resource);
OLC_MODULE(vedit_script);
OLC_MODULE(vedit_shortdescription);
OLC_MODULE(vedit_spawns);
OLC_MODULE(vedit_speed);


// externs
extern const char *component_flags[];
extern const char *component_types[];
extern const char *interact_types[];
extern const int interact_attach_types[NUM_INTERACTS];
extern const byte interact_vnum_types[NUM_INTERACTS];
extern const char *olc_flag_bits[];
extern const char *olc_type_bits[NUM_OLC_TYPES+1];
extern const char *pool_types[];
extern const char *player_tech_types[];
extern const bool requirement_amt_type[];
extern const char *requirement_types[];
extern const char *resource_types[];
extern const char *techs[];

// external functions
void replace_question_color(char *input, char *color, char *output);
extern char *requirement_string(struct req_data *req, bool show_vnums);
extern char *show_color_codes(char *string);
void sort_icon_set(struct icon_data **list);
void sort_interactions(struct interaction_item **list);
extern int sort_requirements_by_group(struct req_data *a, struct req_data *b);
extern bool valid_room_template_vnum(rmt_vnum vnum);

// locals
bool can_start_olc_edit(char_data *ch, int type, any_vnum vnum);
void smart_copy_requirements(struct req_data **to_list, struct req_data *from_list);

// prototypes: auditors
extern bool audit_ability(ability_data *abil, char_data *ch);
extern bool audit_adventure(adv_data *adv, char_data *ch, bool only_one);
extern bool audit_archetype(archetype_data *arch, char_data *ch);
extern bool audit_augment(augment_data *aug, char_data *ch);
extern bool audit_building(bld_data *bld, char_data *ch);
extern bool audit_class(class_data *cls, char_data *ch);
extern bool audit_craft(craft_data *craft, char_data *ch);
extern bool audit_crop(crop_data *cp, char_data *ch);
extern bool audit_event(event_data *event, char_data *ch);
extern bool audit_faction(faction_data *fct, char_data *ch);
extern bool audit_generic(generic_data *gen, char_data *ch);
extern bool audit_global(struct global_data *global, char_data *ch);
extern bool audit_mobile(char_data *mob, char_data *ch);
extern bool audit_morph(morph_data *morph, char_data *ch);
extern bool audit_object(obj_data *obj, char_data *ch);
extern bool audit_progress(progress_data *prg, char_data *ch);
extern bool audit_quest(quest_data *quest, char_data *ch);
extern bool audit_room_template(room_template *rmt, char_data *ch);
extern bool audit_sector(sector_data *sect, char_data *ch);
extern bool audit_shop(shop_data *shop, char_data *ch);
extern bool audit_skill(skill_data *skill, char_data *ch);
extern bool audit_social(social_data *soc, char_data *ch);
extern bool audit_trigger(trig_data *trig, char_data *ch);
extern bool audit_vehicle(vehicle_data *veh, char_data *ch);

// prototypes: show
void olc_show_ability(char_data *ch);
void olc_show_adventure(char_data *ch);
void olc_show_archetype(char_data *ch);
void olc_show_augment(char_data *ch);
void olc_show_book(char_data *ch);
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

// prototypes: setup
extern ability_data *setup_olc_ability(ability_data *input);
extern adv_data *setup_olc_adventure(adv_data *input);
extern archetype_data *setup_olc_archetype(archetype_data *input);
extern augment_data *setup_olc_augment(augment_data *input);
extern book_data *setup_olc_book(book_data *input);
extern bld_data *setup_olc_building(bld_data *input);
extern class_data *setup_olc_class(class_data *input);
extern craft_data *setup_olc_craft(craft_data *input);
extern crop_data *setup_olc_crop(crop_data *input);
extern event_data *setup_olc_event(event_data *input);
extern faction_data *setup_olc_faction(faction_data *input);
extern generic_data *setup_olc_generic(generic_data *input);
extern struct global_data *setup_olc_global(struct global_data *input);
extern char_data *setup_olc_mobile(char_data *input);
extern morph_data *setup_olc_morph(morph_data *input);
extern obj_data *setup_olc_object(obj_data *input);
extern progress_data *setup_olc_progress(progress_data *input);
extern quest_data *setup_olc_quest(quest_data *input);
extern room_template *setup_olc_room_template(room_template *input);
extern sector_data *setup_olc_sector(sector_data *input);
extern shop_data *setup_olc_shop(shop_data *input);
extern skill_data *setup_olc_skill(skill_data *input);
extern social_data *setup_olc_social(social_data *input);
extern struct trig_data *setup_olc_trigger(struct trig_data *input, char **cmdlist_storage);
extern vehicle_data *setup_olc_vehicle(vehicle_data *input);

// prototypes: other
extern bool validate_icon(char *icon);


// master olc command structure
const struct olc_command_data olc_data[] = {
	// OLC_x: main commands
	{ "abort", olc_abort, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_EVENT | OLC_FACTION | OLC_GENERIC | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_PROGRESS | OLC_QUEST | OLC_SECTOR | OLC_SHOP | OLC_SKILL | OLC_SOCIAL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, OLC_CF_EDITOR | OLC_CF_NO_ABBREV },
	{ "audit", olc_audit, OLC_ABILITY | OLC_ADVENTURE | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_EVENT | OLC_FACTION | OLC_GENERIC | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_PROGRESS | OLC_QUEST | OLC_ROOM_TEMPLATE | OLC_SHOP | OLC_SKILL | OLC_SOCIAL | OLC_TRIGGER | OLC_VEHICLE, NOBITS },
	{ "copy", olc_copy, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_EVENT | OLC_FACTION | OLC_GENERIC | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_PROGRESS | OLC_QUEST | OLC_SECTOR | OLC_SHOP | OLC_SKILL | OLC_SOCIAL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, NOBITS },
	{ "delete", olc_delete, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_EVENT | OLC_FACTION | OLC_GENERIC | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_PROGRESS | OLC_QUEST | OLC_SECTOR | OLC_SHOP | OLC_SKILL | OLC_SOCIAL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, OLC_CF_NO_ABBREV },
	// "display" command uses the shortcut "." or "olc" with no args, and is in the do_olc function
	{ "edit", olc_edit, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_EVENT | OLC_FACTION | OLC_GENERIC | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_PROGRESS | OLC_QUEST | OLC_SECTOR | OLC_SHOP | OLC_SKILL | OLC_SOCIAL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, NOBITS },
	{ "free", olc_free, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_EVENT | OLC_FACTION | OLC_GENERIC | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_PROGRESS | OLC_QUEST | OLC_SECTOR | OLC_SHOP | OLC_SKILL | OLC_SOCIAL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, NOBITS },
	{ "list", olc_list, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_EVENT | OLC_FACTION | OLC_GENERIC | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_PROGRESS | OLC_QUEST | OLC_SECTOR | OLC_SHOP | OLC_SKILL | OLC_SOCIAL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, NOBITS },
	{ "save", olc_save, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_EVENT | OLC_FACTION | OLC_GENERIC | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_PROGRESS | OLC_QUEST | OLC_SECTOR | OLC_SHOP | OLC_SKILL | OLC_SOCIAL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, OLC_CF_EDITOR | OLC_CF_NO_ABBREV },
	{ "search", olc_search, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_EVENT | OLC_FACTION | OLC_GENERIC | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_PROGRESS | OLC_QUEST | OLC_SECTOR | OLC_SHOP | OLC_SKILL | OLC_SOCIAL | OLC_TRIGGER | OLC_ROOM_TEMPLATE | OLC_VEHICLE, NOBITS },
	
	// admin
	{ "removeindev", olc_removeindev, NOBITS, NOBITS },
	{ "setflags", olc_set_flags, NOBITS, NOBITS },
	{ "setminvnum", olc_set_min_vnum, NOBITS, NOBITS },
	{ "setmaxvnum", olc_set_max_vnum, NOBITS, NOBITS },
	
	// ability commands
	{ "affects", abiledit_affects, OLC_ABILITY, OLC_CF_EDITOR },
	{ "affectvnum", abiledit_affectvnum, OLC_ABILITY, OLC_CF_EDITOR },
	{ "apply", abiledit_apply, OLC_ABILITY, OLC_CF_EDITOR },
	{ "attacktype", abiledit_attacktype, OLC_ABILITY, OLC_CF_EDITOR },
	{ "cdtime", abiledit_cdtime, OLC_ABILITY, OLC_CF_EDITOR },
	{ "command", abiledit_command, OLC_ABILITY, OLC_CF_EDITOR },
	{ "cooldown", abiledit_cooldown, OLC_ABILITY, OLC_CF_EDITOR },
	{ "cost", abiledit_cost, OLC_ABILITY, OLC_CF_EDITOR },
	{ "costperscalepoint", abiledit_costperscalepoint, OLC_ABILITY, OLC_CF_EDITOR },
	{ "costtype", abiledit_costtype, OLC_ABILITY, OLC_CF_EDITOR },
	{ "custom", abiledit_custom, OLC_ABILITY, OLC_CF_EDITOR },
	{ "damagetype", abiledit_damagetype, OLC_ABILITY, OLC_CF_EDITOR },
	{ "data", abiledit_data, OLC_ABILITY, OLC_CF_EDITOR },
	{ "difficulty", abiledit_difficulty, OLC_ABILITY, OLC_CF_EDITOR },
	{ "flags", abiledit_flags, OLC_ABILITY, OLC_CF_EDITOR },
	{ "gainhooks", abiledit_gainhooks, OLC_ABILITY, OLC_CF_EDITOR },
	{ "immunities", abiledit_immunities, OLC_ABILITY, OLC_CF_EDITOR },
	{ "linkedtrait", abiledit_linkedtrait, OLC_ABILITY, OLC_CF_EDITOR },
	{ "longduration", abiledit_longduration, OLC_ABILITY, OLC_CF_EDITOR },
	{ "masteryability", abiledit_masteryability, OLC_ABILITY, OLC_CF_EDITOR },
	{ "maxstacks", abiledit_maxstacks, OLC_ABILITY, OLC_CF_EDITOR },
	{ "minposition", abiledit_minposition, OLC_ABILITY, OLC_CF_EDITOR },
	{ "name", abiledit_name, OLC_ABILITY, OLC_CF_EDITOR },
	{ "scale", abiledit_scale, OLC_ABILITY, OLC_CF_EDITOR },
	{ "shortduration", abiledit_shortduration, OLC_ABILITY, OLC_CF_EDITOR },
	{ "targets", abiledit_targets, OLC_ABILITY, OLC_CF_EDITOR },
	{ "types", abiledit_types, OLC_ABILITY, OLC_CF_EDITOR },
	{ "waittype", abiledit_waittype, OLC_ABILITY, OLC_CF_EDITOR },
	{ "name", abiledit_name, OLC_ABILITY, OLC_CF_EDITOR },
	
	// adventure zones
	{ "author", advedit_author, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "description", advedit_description, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "endvnum", advedit_endvnum, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "flags", advedit_flags, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "limit", advedit_limit, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "linking", advedit_linking, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "maxlevel", advedit_maxlevel, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "minlevel", advedit_minlevel, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "name", advedit_name, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "playerlimit", advedit_playerlimit, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "reset", advedit_reset, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "script", advedit_script, OLC_ADVENTURE, OLC_CF_EDITOR },
	{ "startvnum", advedit_startvnum, OLC_ADVENTURE, OLC_CF_EDITOR },
	
	// adventures: special
	{ "cascade", advedit_cascade, OLC_ADVENTURE, NOBITS },
	{ "uncascade", advedit_uncascade, OLC_ADVENTURE, NOBITS },
	
	// archetypes
	{ "attribute", archedit_attribute, OLC_ARCHETYPE, OLC_CF_EDITOR },
	{ "description", archedit_description, OLC_ARCHETYPE, OLC_CF_EDITOR },
	{ "femalerank", archedit_femalerank, OLC_ARCHETYPE, OLC_CF_EDITOR },
	{ "flags", archedit_flags, OLC_ARCHETYPE, OLC_CF_EDITOR },
	{ "gear", archedit_gear, OLC_ARCHETYPE, OLC_CF_EDITOR },
	{ "lore", archedit_lore, OLC_ARCHETYPE, OLC_CF_EDITOR },
	{ "malerank", archedit_malerank, OLC_ARCHETYPE, OLC_CF_EDITOR },
	{ "name", archedit_name, OLC_ARCHETYPE, OLC_CF_EDITOR },
	{ "startingskill", archedit_skill, OLC_ARCHETYPE, OLC_CF_EDITOR },
	{ "type", archedit_type, OLC_ARCHETYPE, OLC_CF_EDITOR },
	
	// augments
	{ "apply", augedit_apply, OLC_AUGMENT, OLC_CF_EDITOR },
	{ "flags", augedit_flags, OLC_AUGMENT, OLC_CF_EDITOR },
	{ "name", augedit_name, OLC_AUGMENT, OLC_CF_EDITOR },
	{ "requiresability", augedit_ability, OLC_AUGMENT, OLC_CF_EDITOR },
	{ "requiresobject", augedit_requiresobject, OLC_AUGMENT, OLC_CF_EDITOR },
	{ "resource", augedit_resource, OLC_AUGMENT, OLC_CF_EDITOR },
	{ "type", augedit_type, OLC_AUGMENT, OLC_CF_EDITOR },
	{ "wear", augedit_wear, OLC_AUGMENT, OLC_CF_EDITOR },
	{ "where", augedit_wear, OLC_AUGMENT, OLC_CF_EDITOR },	// where to wear it
	
	// books
	{ "author", booked_author, OLC_BOOK, OLC_CF_EDITOR },
	{ "byline", booked_byline, OLC_BOOK, OLC_CF_EDITOR },
	{ "description", booked_item_description, OLC_BOOK, OLC_CF_EDITOR },
	{ "item", booked_item_name, OLC_BOOK, OLC_CF_EDITOR },
	{ "license", booked_license, OLC_BOOK, OLC_CF_EDITOR },
	{ "paragraphs", booked_paragraphs, OLC_BOOK, OLC_CF_EDITOR },
	{ "title", booked_title, OLC_BOOK, OLC_CF_EDITOR },
	
	// building commands
	{ "affects", bedit_affects, OLC_BUILDING, OLC_CF_EDITOR },
	{ "artisan", bedit_artisan, OLC_BUILDING, OLC_CF_EDITOR },
	{ "citizens", bedit_citizens, OLC_BUILDING, OLC_CF_EDITOR },
	{ "commands", bedit_commands, OLC_BUILDING, OLC_CF_EDITOR },
	{ "description", bedit_description, OLC_BUILDING, OLC_CF_EDITOR },
	{ "designate", bedit_designate, OLC_BUILDING, OLC_CF_EDITOR },
	{ "extra", bedit_extra_desc, OLC_BUILDING, OLC_CF_EDITOR },
	{ "fame", bedit_fame, OLC_BUILDING, OLC_CF_EDITOR },
	{ "flags", bedit_flags, OLC_BUILDING, OLC_CF_EDITOR },
	{ "functions", bedit_functions, OLC_BUILDING, OLC_CF_EDITOR },
	{ "hitpoints", bedit_hitpoints, OLC_BUILDING, OLC_CF_EDITOR },
	{ "icon", bedit_icon, OLC_BUILDING, OLC_CF_EDITOR },
	{ "interaction", bedit_interaction, OLC_BUILDING, OLC_CF_EDITOR },
	{ "military", bedit_military, OLC_BUILDING, OLC_CF_EDITOR },
	{ "name", bedit_name, OLC_BUILDING, OLC_CF_EDITOR },
	{ "relations", bedit_relations, OLC_BUILDING, OLC_CF_EDITOR },
	{ "resources", bedit_resource, OLC_BUILDING, OLC_CF_EDITOR },
	{ "rooms", bedit_extrarooms, OLC_BUILDING, OLC_CF_EDITOR },
	{ "script", bedit_script, OLC_BUILDING, OLC_CF_EDITOR },
	{ "spawns", bedit_spawns, OLC_BUILDING, OLC_CF_EDITOR },
	{ "title", bedit_title, OLC_BUILDING, OLC_CF_EDITOR },
	
	// class commands
	{ "abbrev", classedit_abbrev, OLC_CLASS, OLC_CF_EDITOR },
	{ "flags", classedit_flags, OLC_CLASS, OLC_CF_EDITOR },
	{ "name", classedit_name, OLC_CLASS, OLC_CF_EDITOR },
	{ "maxhealth", classedit_maxhealth, OLC_CLASS, OLC_CF_EDITOR },
	{ "maxmana", classedit_maxmana, OLC_CLASS, OLC_CF_EDITOR },
	{ "maxmoves", classedit_maxmoves, OLC_CLASS, OLC_CF_EDITOR },
	{ "requires", classedit_requires, OLC_CLASS, OLC_CF_EDITOR },
	{ "role", classedit_role, OLC_CLASS, OLC_CF_EDITOR },
	
	// craft commands
	{ "builds", cedit_builds, OLC_CRAFT, OLC_CF_EDITOR },
	{ "buildfacing", cedit_buildfacing, OLC_CRAFT, OLC_CF_EDITOR },
	{ "buildon", cedit_buildon, OLC_CRAFT, OLC_CF_EDITOR },
	{ "creates", cedit_creates, OLC_CRAFT, OLC_CF_EDITOR },
	{ "flags", cedit_flags, OLC_CRAFT, OLC_CF_EDITOR },
	{ "levelrequired", cedit_levelrequired, OLC_CRAFT, OLC_CF_EDITOR },
	{ "liquid", cedit_liquid, OLC_CRAFT, OLC_CF_EDITOR },
	{ "name", cedit_name, OLC_CRAFT, OLC_CF_EDITOR },
	{ "quantity", cedit_quantity, OLC_CRAFT, OLC_CF_EDITOR },
	{ "requiresability", cedit_ability, OLC_CRAFT, OLC_CF_EDITOR },
	{ "requiresobject", cedit_requiresobject, OLC_CRAFT, OLC_CF_EDITOR },
	{ "resource", cedit_resource, OLC_CRAFT, OLC_CF_EDITOR },
	{ "time", cedit_time, OLC_CRAFT, OLC_CF_EDITOR },
	{ "type", cedit_type, OLC_CRAFT, OLC_CF_EDITOR },
	{ "volume", cedit_volume, OLC_CRAFT, OLC_CF_EDITOR },
	
	// crop commands
	{ "climate", cropedit_climate, OLC_CROP, OLC_CF_EDITOR },
	{ "flags", cropedit_flags, OLC_CROP, OLC_CF_EDITOR },
	{ "icons", cropedit_icons, OLC_CROP, OLC_CF_EDITOR },
	{ "interaction", cropedit_interaction, OLC_CROP, OLC_CF_EDITOR },
	{ "mapout", cropedit_mapout, OLC_CROP, OLC_CF_EDITOR },
	{ "name", cropedit_name, OLC_CROP, OLC_CF_EDITOR },
	{ "spawns", cropedit_spawns, OLC_CROP, OLC_CF_EDITOR },
	{ "title", cropedit_title, OLC_CROP, OLC_CF_EDITOR },
	{ "xmax", cropedit_xmax, OLC_CROP, OLC_CF_EDITOR },
	{ "xmin", cropedit_xmin, OLC_CROP, OLC_CF_EDITOR },
	{ "ymax", cropedit_ymax, OLC_CROP, OLC_CF_EDITOR },
	{ "ymin", cropedit_ymin, OLC_CROP, OLC_CF_EDITOR },
	
	// event commands
	{ "completemessage", evedit_completemessage, OLC_EVENT, OLC_CF_EDITOR },
	{ "description", evedit_description, OLC_EVENT, OLC_CF_EDITOR },
	{ "duration", evedit_duration, OLC_EVENT, OLC_CF_EDITOR },
	{ "flags", evedit_flags, OLC_EVENT, OLC_CF_EDITOR },
	{ "name", evedit_name, OLC_EVENT, OLC_CF_EDITOR },
	{ "maxlevel", evedit_maxlevel, OLC_EVENT, OLC_CF_EDITOR },
	{ "minlevel", evedit_minlevel, OLC_EVENT, OLC_CF_EDITOR },
	{ "notes", evedit_notes, OLC_EVENT, OLC_CF_EDITOR },
	{ "ranks", evedit_rankrewards, OLC_EVENT, OLC_CF_EDITOR },
	{ "rankrewards", evedit_rankrewards, OLC_EVENT, OLC_CF_EDITOR },
	{ "repeat", evedit_repeat, OLC_EVENT, OLC_CF_EDITOR },
	{ "thresholds", evedit_thresholdrewards, OLC_EVENT, OLC_CF_EDITOR },
	{ "thresholdrewards", evedit_thresholdrewards, OLC_EVENT, OLC_CF_EDITOR },
	
	// faction commands
	{ "description", fedit_description, OLC_FACTION, OLC_CF_EDITOR },
	{ "flags", fedit_flags, OLC_FACTION, OLC_CF_EDITOR },
	{ "matchrelations", fedit_matchrelations, OLC_FACTION, OLC_CF_EDITOR },
	{ "maxreputation", fedit_maxreputation, OLC_FACTION, OLC_CF_EDITOR },
	{ "minreputation", fedit_minreputation, OLC_FACTION, OLC_CF_EDITOR },
	{ "name", fedit_name, OLC_FACTION, OLC_CF_EDITOR },
	{ "relationship", fedit_relation, OLC_FACTION, OLC_CF_EDITOR },
	{ "startingreputation", fedit_startingreputation, OLC_FACTION, OLC_CF_EDITOR },
	
	// generic commands
	{ "flags", genedit_flags, OLC_GENERIC, OLC_CF_EDITOR },
	{ "name", genedit_name, OLC_GENERIC, OLC_CF_EDITOR },
	{ "type", genedit_type, OLC_GENERIC, OLC_CF_EDITOR },
	// generic: actions
	{ "build2char", genedit_build2char, OLC_GENERIC, OLC_CF_EDITOR },
	{ "build2room", genedit_build2room, OLC_GENERIC, OLC_CF_EDITOR },
	{ "craft2char", genedit_craft2char, OLC_GENERIC, OLC_CF_EDITOR },
	{ "craft2room", genedit_craft2room, OLC_GENERIC, OLC_CF_EDITOR },
	{ "repair2char", genedit_repair2char, OLC_GENERIC, OLC_CF_EDITOR },
	{ "repair2room", genedit_repair2room, OLC_GENERIC, OLC_CF_EDITOR },
	// generic: cooldowns
	{ "apply2char", genedit_apply2char, OLC_GENERIC, OLC_CF_EDITOR },
	{ "apply2room", genedit_apply2room, OLC_GENERIC, OLC_CF_EDITOR },
	{ "quickcooldown", genedit_quick_cooldown, OLC_GENERIC, NOBITS },
	{ "standardwearoff", genedit_standardwearoff, OLC_GENERIC, OLC_CF_EDITOR },
	{ "wearoff", genedit_wearoff, OLC_GENERIC, OLC_CF_EDITOR },
	{ "wearoff2room", genedit_wearoff2room, OLC_GENERIC, OLC_CF_EDITOR },
	// generic: liquids
	{ "color", genedit_color, OLC_GENERIC, OLC_CF_EDITOR },
	{ "drunk", genedit_drunk, OLC_GENERIC, OLC_CF_EDITOR },
	{ "hunger", genedit_hunger, OLC_GENERIC, OLC_CF_EDITOR },
	{ "liquid", genedit_liquid, OLC_GENERIC, OLC_CF_EDITOR },
	{ "thirst", genedit_thirst, OLC_GENERIC, OLC_CF_EDITOR },
	// generic: currency
	{ "plural", genedit_plural, OLC_GENERIC, OLC_CF_EDITOR },
	{ "singular", genedit_singular, OLC_GENERIC, OLC_CF_EDITOR },
	
	// globals commands
	{ "capacity", gedit_capacity, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "flags", gedit_flags, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "gear", gedit_gear, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "interaction", gedit_interaction, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "maxlevel", gedit_maxlevel, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "minlevel", gedit_minlevel, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "mobexclude", gedit_mobexclude, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "mobflags", gedit_mobflags, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "name", gedit_name, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "percent", gedit_percent, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "requiresability", gedit_ability, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "sectorexclude", gedit_sectorexclude, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "sectorflags", gedit_sectorflags, OLC_GLOBAL, OLC_CF_EDITOR },
	{ "type", gedit_type, OLC_GLOBAL, OLC_CF_EDITOR },
	
	// mob commands
	{ "affects", medit_affects, OLC_MOBILE, OLC_CF_EDITOR },
	{ "allegiance", medit_allegiance, OLC_MOBILE, OLC_CF_EDITOR },
	{ "attack", medit_attack, OLC_MOBILE, OLC_CF_EDITOR },
	{ "custom", medit_custom, OLC_MOBILE, OLC_CF_EDITOR },
	{ "flags", medit_flags, OLC_MOBILE, OLC_CF_EDITOR },
	{ "interaction", medit_interaction, OLC_MOBILE, OLC_CF_EDITOR },
	{ "keywords", medit_keywords, OLC_MOBILE, OLC_CF_EDITOR },
	{ "longdescription", medit_longdescription, OLC_MOBILE, OLC_CF_EDITOR },
	{ "lookdescription", medit_lookdescription, OLC_MOBILE, OLC_CF_EDITOR },
	{ "maxlevel", medit_maxlevel, OLC_MOBILE, OLC_CF_EDITOR },
	{ "minlevel", medit_minlevel, OLC_MOBILE, OLC_CF_EDITOR },
	{ "movetype", medit_movetype, OLC_MOBILE, OLC_CF_EDITOR },
	{ "nameset", medit_nameset, OLC_MOBILE, OLC_CF_EDITOR },
	{ "script", medit_script, OLC_MOBILE, OLC_CF_EDITOR },
	{ "sex", medit_sex, OLC_MOBILE, OLC_CF_EDITOR },
	{ "shortdescription", medit_short_description, OLC_MOBILE, OLC_CF_EDITOR },
	{ "size", medit_size, OLC_MOBILE, OLC_CF_EDITOR },
	
	// map commands
	{ "build", mapedit_build, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "complete", mapedit_complete_room, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "decay", mapedit_decay, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "decustomize", mapedit_decustomize, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "deleteexit", mapedit_delete_exit, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "deleteroom", mapedit_delete_room, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "description", mapedit_room_description, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "exit", mapedit_exits, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "grow", mapedit_grow, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "icon", mapedit_icon, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "maintain", mapedit_maintain, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "name", mapedit_room_name, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "naturalize", mapedit_naturalize, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "passwalls", mapedit_pass_walls, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "populate", mapedit_populate, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "remember", mapedit_remember, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "roomtype", mapedit_roomtype, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "ruin", mapedit_ruin, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "terrain", mapedit_terrain, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "unclaimable", mapedit_unclaimable, OLC_MAP, OLC_CF_MAP_EDIT },
	
	// morph commands
	{ "apply", morphedit_apply, OLC_MORPH, OLC_CF_EDITOR },
	{ "affects", morphedit_affects, OLC_MORPH, OLC_CF_EDITOR },
	{ "attack", morphedit_attack, OLC_MORPH, OLC_CF_EDITOR },
	{ "cost", morphedit_cost, OLC_MORPH, OLC_CF_EDITOR },
	{ "costtype", morphedit_costtype, OLC_MORPH, OLC_CF_EDITOR },
	{ "flags", morphedit_flags, OLC_MORPH, OLC_CF_EDITOR },
	{ "keywords", morphedit_keywords, OLC_MORPH, OLC_CF_EDITOR },
	{ "longdescription", morphedit_longdesc, OLC_MORPH, OLC_CF_EDITOR },
	{ "lookdescription", morphedit_lookdescription, OLC_MORPH, OLC_CF_EDITOR },
	{ "maxlevel", morphedit_maxlevel, OLC_MORPH, OLC_CF_EDITOR },
	{ "movetype", morphedit_movetype, OLC_MORPH, OLC_CF_EDITOR },
	{ "requiresability", morphedit_ability, OLC_MORPH, OLC_CF_EDITOR },
	{ "requiresobject", morphedit_requiresobject, OLC_MORPH, OLC_CF_EDITOR },
	{ "shortdescription", morphedit_shortdesc, OLC_MORPH, OLC_CF_EDITOR },
	{ "size", morphedit_size, OLC_MORPH, OLC_CF_EDITOR },
	
	// object commands
	{ "affects", oedit_affects, OLC_OBJECT, OLC_CF_EDITOR },
	{ "affecttype", oedit_affecttype, OLC_OBJECT, OLC_CF_EDITOR },
	{ "apply", oedit_apply, OLC_OBJECT, OLC_CF_EDITOR },
	{ "armortype", oedit_armortype, OLC_OBJECT, OLC_CF_EDITOR },
	{ "ammotype", oedit_ammotype, OLC_OBJECT, OLC_CF_EDITOR },
	{ "automint", oedit_automint, OLC_OBJECT, OLC_CF_EDITOR },
	{ "capacity", oedit_capacity, OLC_OBJECT, OLC_CF_EDITOR },
	{ "cdtime", oedit_cdtime, OLC_OBJECT, OLC_CF_EDITOR },
	{ "charges", oedit_charges, OLC_OBJECT, OLC_CF_EDITOR },
	{ "coinamount", oedit_coinamount, OLC_OBJECT, OLC_CF_EDITOR },
	{ "component", oedit_component, OLC_OBJECT, OLC_CF_EDITOR },	// deliberately before "compflags"
	{ "compflags", oedit_compflags, OLC_OBJECT, OLC_CF_EDITOR },
	{ "containerflags", oedit_containerflags, OLC_OBJECT, OLC_CF_EDITOR },
	{ "contents", oedit_contents, OLC_OBJECT, OLC_CF_EDITOR },
	{ "cooldown", oedit_cooldown, OLC_OBJECT, OLC_CF_EDITOR },
	{ "corpseof", oedit_corpseof, OLC_OBJECT, OLC_CF_EDITOR },
	{ "currency", oedit_currency, OLC_OBJECT, OLC_CF_EDITOR },
	{ "custom", oedit_custom, OLC_OBJECT, OLC_CF_EDITOR },
	{ "damage", oedit_damage, OLC_OBJECT, OLC_CF_EDITOR },
	{ "extra", oedit_extra_desc, OLC_OBJECT, OLC_CF_EDITOR },
	{ "flags", oedit_flags, OLC_OBJECT, OLC_CF_EDITOR },
	{ "fullness", oedit_fullness, OLC_OBJECT, OLC_CF_EDITOR },
	{ "interaction", oedit_interaction, OLC_OBJECT, OLC_CF_EDITOR },
	{ "keywords", oedit_keywords, OLC_OBJECT, OLC_CF_EDITOR },
	{ "liquid", oedit_liquid, OLC_OBJECT, OLC_CF_EDITOR },
	{ "longdescription", oedit_long_desc, OLC_OBJECT, OLC_CF_EDITOR },
	{ "lookdescription", oedit_action_desc, OLC_OBJECT, OLC_CF_EDITOR },
	{ "material", oedit_material, OLC_OBJECT, OLC_CF_EDITOR },
	{ "maxlevel", oedit_maxlevel, OLC_OBJECT, OLC_CF_EDITOR },
	{ "minlevel", oedit_minlevel, OLC_OBJECT, OLC_CF_EDITOR },
	{ "minipet", oedit_minipet, OLC_OBJECT, OLC_CF_EDITOR },
	{ "paint", oedit_paint, OLC_OBJECT, OLC_CF_EDITOR },
	{ "plants", oedit_plants, OLC_OBJECT, OLC_CF_EDITOR },
	{ "quantity", oedit_quantity, OLC_OBJECT, OLC_CF_EDITOR },
	{ "recipe", oedit_recipe, OLC_OBJECT, OLC_CF_EDITOR },
	{ "requiresquest", oedit_requiresquest, OLC_OBJECT, OLC_CF_EDITOR },
	{ "roomvnum", oedit_roomvnum, OLC_OBJECT, OLC_CF_EDITOR },
	{ "script", oedit_script, OLC_OBJECT, OLC_CF_EDITOR },
	{ "shortdescription", oedit_short_description, OLC_OBJECT, OLC_CF_EDITOR },
	{ "size", oedit_size, OLC_OBJECT, OLC_CF_EDITOR },
	{ "storage", oedit_storage, OLC_OBJECT, OLC_CF_EDITOR },
	{ "store", oedit_storage, OLC_OBJECT, OLC_CF_EDITOR },
	{ "timer", oedit_timer, OLC_OBJECT, OLC_CF_EDITOR },
	{ "text", oedit_text, OLC_OBJECT, OLC_CF_EDITOR },
	{ "type", oedit_type, OLC_OBJECT, OLC_CF_EDITOR },
	{ "uses", oedit_uses, OLC_OBJECT, OLC_CF_EDITOR },
	{ "value0", oedit_value0, OLC_OBJECT, OLC_CF_EDITOR },
	{ "value1", oedit_value1, OLC_OBJECT, OLC_CF_EDITOR },
	{ "value2", oedit_value2, OLC_OBJECT, OLC_CF_EDITOR },
	{ "wealth", oedit_wealth, OLC_OBJECT, OLC_CF_EDITOR },
	{ "weapontype", oedit_weapontype, OLC_OBJECT, OLC_CF_EDITOR },
	{ "wear", oedit_wear, OLC_OBJECT, OLC_CF_EDITOR },
	// oedit: special
	{ "quickrecipe", oedit_quick_recipe, OLC_OBJECT, NOBITS },
	
	// progression commands
	{ "cost", progedit_cost, OLC_PROGRESS, OLC_CF_EDITOR },
	{ "description", progedit_description, OLC_PROGRESS, OLC_CF_EDITOR },
	{ "flags", progedit_flags, OLC_PROGRESS, OLC_CF_EDITOR },
	{ "name", progedit_name, OLC_PROGRESS, OLC_CF_EDITOR },
	{ "perks", progedit_perks, OLC_PROGRESS, OLC_CF_EDITOR },
	{ "prereqs", progedit_prereqs, OLC_PROGRESS, OLC_CF_EDITOR },
	{ "tasks", progedit_tasks, OLC_PROGRESS, OLC_CF_EDITOR },
	{ "type", progedit_type, OLC_PROGRESS, OLC_CF_EDITOR },
	{ "value", progedit_value, OLC_PROGRESS, OLC_CF_EDITOR },
	
	// quest commands
	{ "completemessage", qedit_completemessage, OLC_QUEST, OLC_CF_EDITOR },
	{ "dailycycle", qedit_dailycycle, OLC_QUEST, OLC_CF_EDITOR },
	{ "description", qedit_description, OLC_QUEST, OLC_CF_EDITOR },
	{ "ends", qedit_ends, OLC_QUEST, OLC_CF_EDITOR },
	{ "flags", qedit_flags, OLC_QUEST, OLC_CF_EDITOR },
	{ "name", qedit_name, OLC_QUEST, OLC_CF_EDITOR },
	{ "maxlevel", qedit_maxlevel, OLC_QUEST, OLC_CF_EDITOR },
	{ "minlevel", qedit_minlevel, OLC_QUEST, OLC_CF_EDITOR },
	{ "prereqs", qedit_prereqs, OLC_QUEST, OLC_CF_EDITOR },
	{ "repeat", qedit_repeat, OLC_QUEST, OLC_CF_EDITOR },
	{ "rewards", qedit_rewards, OLC_QUEST, OLC_CF_EDITOR },
	{ "script", qedit_script, OLC_QUEST, OLC_CF_EDITOR },
	{ "starts", qedit_starts, OLC_QUEST, OLC_CF_EDITOR },
	{ "tasks", qedit_tasks, OLC_QUEST, OLC_CF_EDITOR },
	
	// room template commands
	{ "affects", rmedit_affects, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "description", rmedit_description, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "exit", rmedit_exit, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "extra", rmedit_extra_desc, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "interaction", rmedit_interaction, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "flags", rmedit_flags, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "functions", rmedit_functions, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "matchexits", rmedit_matchexits, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "title", rmedit_title, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "script", rmedit_script, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "spawns", rmedit_spawns, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	
	// sector commands	
	{ "buildflags", sectedit_buildflags, OLC_SECTOR, OLC_CF_EDITOR },
	{ "climate", sectedit_climate, OLC_SECTOR, OLC_CF_EDITOR },
	{ "commands", sectedit_commands, OLC_SECTOR, OLC_CF_EDITOR },
	{ "evolution", sectedit_evolution, OLC_SECTOR, OLC_CF_EDITOR },
	{ "flags", sectedit_flags, OLC_SECTOR, OLC_CF_EDITOR },
	{ "icons", sectedit_icons, OLC_SECTOR, OLC_CF_EDITOR },
	{ "interaction", sectedit_interaction, OLC_SECTOR, OLC_CF_EDITOR },
	{ "mapout", sectedit_mapout, OLC_SECTOR, OLC_CF_EDITOR },
	{ "movecost", sectedit_movecost, OLC_SECTOR, OLC_CF_EDITOR },
	{ "name", sectedit_name, OLC_SECTOR, OLC_CF_EDITOR },
	{ "roadsideicon", sectedit_roadsideicon, OLC_SECTOR, OLC_CF_EDITOR },
	{ "spawns", sectedit_spawns, OLC_SECTOR, OLC_CF_EDITOR },
	{ "title", sectedit_title, OLC_SECTOR, OLC_CF_EDITOR },
	
	// shop commands
	{ "allegiance", shopedit_allegiance, OLC_SHOP, OLC_CF_EDITOR },
	{ "closes", shopedit_closes, OLC_SHOP, OLC_CF_EDITOR },
	{ "flags", shopedit_flags, OLC_SHOP, OLC_CF_EDITOR },
	{ "items", shopedit_items, OLC_SHOP, OLC_CF_EDITOR },
	{ "locations", shopedit_locations, OLC_SHOP, OLC_CF_EDITOR },
	{ "name", shopedit_name, OLC_SHOP, OLC_CF_EDITOR },
	{ "opens", shopedit_opens, OLC_SHOP, OLC_CF_EDITOR },
	
	// skill commands
	{ "abbrev", skilledit_abbrev, OLC_SKILL, OLC_CF_EDITOR },
	{ "description", skilledit_description, OLC_SKILL, OLC_CF_EDITOR },
	{ "flags", skilledit_flags, OLC_SKILL, OLC_CF_EDITOR },
	{ "maxlevel", skilledit_maxlevel, OLC_SKILL, OLC_CF_EDITOR },
	{ "mindrop", skilledit_mindrop, OLC_SKILL, OLC_CF_EDITOR },
	{ "name", skilledit_name, OLC_SKILL, OLC_CF_EDITOR },
	{ "synergy", skilledit_synergy, OLC_SKILL, OLC_CF_EDITOR },
	{ "tree", skilledit_tree, OLC_SKILL, OLC_CF_EDITOR },
	{ "showtree", skilledit_showtree, OLC_SKILL, OLC_CF_EDITOR },
	{ "showsynergies", skilledit_showsynergies, OLC_SKILL, OLC_CF_EDITOR },
	
	// social commands
	{ "charposition", socedit_charposition, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "command", socedit_command, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "flags", socedit_flags, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "name", socedit_name, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "requirements", socedit_requirements, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "targetposition", socedit_targetposition, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "n2character", socedit_n2char, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "n2others", socedit_n2other, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "s2character", socedit_s2char, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "s2others", socedit_s2other, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "t2character", socedit_t2char, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "t2victim", socedit_t2vict, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "t2others", socedit_t2other, OLC_SOCIAL, OLC_CF_EDITOR },
	{ "tnotfound", socedit_tnotfound, OLC_SOCIAL, OLC_CF_EDITOR },
	
	// trigger commands
	{ "argtype", tedit_argtype, OLC_TRIGGER, OLC_CF_EDITOR },
	{ "attaches", tedit_attaches, OLC_TRIGGER, OLC_CF_EDITOR },
	{ "commands", tedit_commands, OLC_TRIGGER, OLC_CF_EDITOR },
	{ "costs", tedit_costs, OLC_TRIGGER, OLC_CF_EDITOR },
	{ "location", tedit_location, OLC_TRIGGER, OLC_CF_EDITOR },
	{ "name", tedit_name, OLC_TRIGGER, OLC_CF_EDITOR },
	{ "percent", tedit_percent, OLC_TRIGGER, OLC_CF_EDITOR },
	{ "string", tedit_string, OLC_TRIGGER, OLC_CF_EDITOR },
	{ "types", tedit_types, OLC_TRIGGER, OLC_CF_EDITOR },
	
	// vehicle commands
	{ "animalsrequired", vedit_animalsrequired, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "capacity", vedit_capacity, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "designate", vedit_designate, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "extra", vedit_extra_desc, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "extrarooms", vedit_extrarooms, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "fame", vedit_fame, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "flags", vedit_flags, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "functions", vedit_functions, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "hitpoints", vedit_hitpoints, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "icon", vedit_icon, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "interaction", vedit_interaction, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "interiorroom", vedit_interiorroom, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "keywords", vedit_keywords, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "longdescription", vedit_longdescription, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "lookdescription", vedit_lookdescription, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "maxlevel", vedit_maxlevel, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "military", vedit_military, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "minlevel", vedit_minlevel, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "movetype", vedit_movetype, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "resource", vedit_resource, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "script", vedit_script, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "shortdescription", vedit_shortdescription, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "spawns", vedit_spawns, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "speed", vedit_speed, OLC_VEHICLE, OLC_CF_EDITOR },
	
	
	// misc commands that should not take precedence over editor commands
	{ "fullsearch", olc_fullsearch, OLC_ABILITY | OLC_BUILDING | OLC_CROP | OLC_MOBILE |  OLC_OBJECT | OLC_PROGRESS | OLC_TRIGGER | OLC_VEHICLE, NOBITS },
	
	// this goes last
	{ "\n", NULL, NOBITS, NOBITS }
};


 //////////////////////////////////////////////////////////////////////////////
//// CORE COMMAND ////////////////////////////////////////////////////////////

// Usage: olc [type] [command] [args]
ACMD(do_olc) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], notype[MAX_INPUT_LENGTH];
	char *arg_ptr;
	int pos, type;
	int iter;
	
	// nope.
	if (!ch->desc) {
		return;
	}
	
	// split the argument into: arg1 arg2 arg3... notype == "arg2 arg3"
	half_chop(argument, arg1, notype);
	half_chop(notype, arg2, arg3);
	
	// arg1: could be a type
	type = find_olc_type(arg1);

	// arg1: if it wasn't a type, move the args over
	if (!type) {
		// move the notype arg into arg3, as we'll now be using it
		strcpy(arg3, notype);
		
		// move arg1 into arg2
		strcpy(arg2, arg1);
		
		// and pull type from the character
		type = GET_OLC_TYPE(ch->desc);
	}
	
	// arg2: should be a command
	pos = NOTHING;
	for (iter = 0; *olc_data[iter].command != '\n' && pos == NOTHING; ++iter) {
		if (olc_data[iter].valid_types == 0 || IS_SET(olc_data[iter].valid_types, type)) {
			if (!IS_SET(olc_data[iter].flags, OLC_CF_MAP_EDIT) || GET_ACCESS_LEVEL(ch) >= LVL_UNRESTRICTED_BUILDER || OLC_FLAGGED(ch, OLC_FLAG_MAP_EDIT)) {
				if (!IS_SET(olc_data[iter].flags, OLC_CF_EDITOR) || IS_SET(olc_data[iter].valid_types, GET_OLC_TYPE(ch->desc))) {
					if (!str_cmp(arg2, olc_data[iter].command) || (!IS_SET(olc_data[iter].flags, OLC_CF_NO_ABBREV) && is_abbrev(arg2, olc_data[iter].command))) {
						pos = iter;
					}
				}
			}
		}
	}

	// now: type and pos are set; arg3 is the remaining argument
	if (pos == NOTHING) {
		if (*arg2) {
			// command provided but not found
			msg_to_char(ch, "Unknown OLC command '%s'.\r\n", arg2);
		}
		else if (GET_OLC_TYPE(ch->desc) != 0) {
			// open editor? just use the display command
			olc_display(ch, 0, "");
		}
		else {
			// not using the editor -- show usage
			msg_to_char(ch, "Usage: olc <type> <command>\r\n");
			
			// list commands only if they specified a type
			if (type != 0) {
				*buf = '\0';
				for (iter = 0; *olc_data[iter].command != '\n'; ++iter) {
					if (IS_SET(olc_data[iter].valid_types, type) && !IS_SET(olc_data[iter].flags, OLC_CF_EDITOR)) {
						if (!IS_SET(olc_data[iter].flags, OLC_CF_MAP_EDIT) || GET_ACCESS_LEVEL(ch) >= LVL_UNRESTRICTED_BUILDER || OLC_FLAGGED(ch, OLC_FLAG_MAP_EDIT)) {
							sprintf(buf + strlen(buf), "%s%s", (*buf ? ", " : ""), olc_data[iter].command);
						}
					}
				}
				
				if (*buf) {
					msg_to_char(ch, "Valid commands: %s\r\n", buf);
				}
			}
		}
	}
	else {
		// pos != NOTHING: a command was entered
		arg_ptr = arg3;
		skip_spaces(&arg_ptr);
		((*olc_data[pos].func)(ch, type, arg_ptr));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMON MODULES //////////////////////////////////////////////////////////

OLC_MODULE(olc_abort) {
	char typename[42];
	
	sprintbit(GET_OLC_TYPE(ch->desc), olc_type_bits, typename, FALSE);
	
	if (GET_OLC_TYPE(ch->desc) == 0 || GET_OLC_VNUM(ch->desc) == NOTHING) {
		msg_to_char(ch, "You aren't editing anything.\r\n");
	}
	else if (ch->desc->str) {
		msg_to_char(ch, "Close your text editor (&y,/h&0) before aborting an olc editor.\r\n");
	}
	else {
		// OLC_x:
		switch (GET_OLC_TYPE(ch->desc)) {
			case OLC_ABILITY: {
				free_ability(GET_OLC_ABILITY(ch->desc));
				GET_OLC_ABILITY(ch->desc) = NULL;
				break;
			}
			case OLC_ADVENTURE: {
				free_adventure(GET_OLC_ADVENTURE(ch->desc));
				GET_OLC_ADVENTURE(ch->desc) = NULL;
				break;
			}
			case OLC_ARCHETYPE: {
				free_archetype(GET_OLC_ARCHETYPE(ch->desc));
				GET_OLC_ARCHETYPE(ch->desc) = NULL;
				break;
			}
			case OLC_AUGMENT: {
				free_augment(GET_OLC_AUGMENT(ch->desc));
				GET_OLC_AUGMENT(ch->desc) = NULL;
				break;
			}
			case OLC_BOOK: {
				free_book(GET_OLC_BOOK(ch->desc));
				GET_OLC_BOOK(ch->desc) = NULL;
				break;
			}
			case OLC_BUILDING: {
				free_building(GET_OLC_BUILDING(ch->desc));
				GET_OLC_BUILDING(ch->desc) = NULL;
				break;
			}
			case OLC_CLASS: {
				free_class(GET_OLC_CLASS(ch->desc));
				GET_OLC_CLASS(ch->desc) = NULL;
				break;
			}
			case OLC_CRAFT: {
				free_craft(GET_OLC_CRAFT(ch->desc));
				GET_OLC_CRAFT(ch->desc) = NULL;
				break;
			}
			case OLC_CROP: {
				free_crop(GET_OLC_CROP(ch->desc));
				GET_OLC_CROP(ch->desc) = NULL;
				break;
			}
			case OLC_EVENT: {
				free_event(GET_OLC_EVENT(ch->desc));
				GET_OLC_EVENT(ch->desc) = NULL;
				break;
			}
			case OLC_FACTION: {
				free_faction(GET_OLC_FACTION(ch->desc));
				GET_OLC_FACTION(ch->desc) = NULL;
				break;
			}
			case OLC_GENERIC: {
				free_generic(GET_OLC_GENERIC(ch->desc));
				GET_OLC_GENERIC(ch->desc) = NULL;
				break;
			}
			case OLC_GLOBAL: {
				free_global(GET_OLC_GLOBAL(ch->desc));
				GET_OLC_GLOBAL(ch->desc) = NULL;
				break;
			}
			case OLC_MOBILE: {
				free_char(GET_OLC_MOBILE(ch->desc));
				GET_OLC_MOBILE(ch->desc) = NULL;
				break;
			}
			case OLC_MORPH: {
				free_morph(GET_OLC_MORPH(ch->desc));
				GET_OLC_MORPH(ch->desc) = NULL;
				break;
			}
			case OLC_OBJECT: {
				free_obj(GET_OLC_OBJECT(ch->desc));
				GET_OLC_OBJECT(ch->desc) = NULL;
				break;
			}
			case OLC_PROGRESS: {
				free_progress(GET_OLC_PROGRESS(ch->desc));
				GET_OLC_PROGRESS(ch->desc) = NULL;
				break;
			}
			case OLC_QUEST: {
				free_quest(GET_OLC_QUEST(ch->desc));
				GET_OLC_QUEST(ch->desc) = NULL;
				break;
			}
			case OLC_ROOM_TEMPLATE: {
				free_room_template(GET_OLC_ROOM_TEMPLATE(ch->desc));
				GET_OLC_ROOM_TEMPLATE(ch->desc) = NULL;
				break;
			}
			case OLC_SECTOR: {
				free_sector(GET_OLC_SECTOR(ch->desc));
				GET_OLC_SECTOR(ch->desc) = NULL;
				break;
			}
			case OLC_SHOP: {
				free_shop(GET_OLC_SHOP(ch->desc));
				GET_OLC_SHOP(ch->desc) = NULL;
				break;
			}
			case OLC_SKILL: {
				free_skill(GET_OLC_SKILL(ch->desc));
				GET_OLC_SKILL(ch->desc) = NULL;
				break;
			}
			case OLC_SOCIAL: {
				free_social(GET_OLC_SOCIAL(ch->desc));
				GET_OLC_SOCIAL(ch->desc) = NULL;
				break;
			}
			case OLC_TRIGGER: {
				free_trigger(GET_OLC_TRIGGER(ch->desc));
				GET_OLC_TRIGGER(ch->desc) = NULL;
				if (GET_OLC_STORAGE(ch->desc)) {
					free(GET_OLC_STORAGE(ch->desc));
					GET_OLC_STORAGE(ch->desc) = NULL;
				}
				break;
			}
			case OLC_VEHICLE: {
				free_vehicle(GET_OLC_VEHICLE(ch->desc));
				GET_OLC_VEHICLE(ch->desc) = NULL;
				break;
			}
			default: {
				msg_to_char(ch, "Error saving: unknown save type %s.\r\n", typename);
				break;
			}
		}
		
		msg_to_char(ch, "You abort your changes to %s %d and close the editor.\r\n", typename, GET_OLC_VNUM(ch->desc));
		GET_OLC_TYPE(ch->desc) = 0;
		GET_OLC_VNUM(ch->desc) = NOTHING;
	}
}


// Usage: olc mob audit <from vnum> [to vnum]
OLC_MODULE(olc_audit) {
	char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	any_vnum from_vnum = NOTHING, to_vnum = NOTHING, iter;
	bool found = FALSE;
	
	// allow - or :
	for (iter = 0; iter < strlen(argument); ++iter) {
		if (argument[iter] == '-' || argument[iter] == ':') {
			argument[iter] = ' ';
		}
	}
	half_chop(argument, arg, arg2);
	
	if (!*arg || !isdigit(*arg) || (*arg2 && !isdigit(*arg2))) {
		msg_to_char(ch, "Usage: audit <from vnum> [to vnum]\r\n");
	}
	else if ((from_vnum = atoi(arg)) < 0 || (to_vnum = atoi(arg2)) < 0 ) {
		msg_to_char(ch, "Invalid vnum range.\r\n");
	}
	else {
		// 2nd vnum optional
		if (!*arg2) {
			to_vnum = from_vnum;
		}
		
		// OLC_x: auditors
		switch (type) {
			case OLC_ABILITY: {
				ability_data *abil, *next_abil;
				HASH_ITER(hh, ability_table, abil, next_abil) {
					if (ABIL_VNUM(abil) >= from_vnum && ABIL_VNUM(abil) <= to_vnum) {
						found |= audit_ability(abil, ch);
					}
				}
				break;
			}
			case OLC_ADVENTURE: {
				adv_data *adv, *next_adv;
				HASH_ITER(hh, adventure_table, adv, next_adv) {
					if (GET_ADV_VNUM(adv) >= from_vnum && GET_ADV_VNUM(adv) <= to_vnum) {
						found |= audit_adventure(adv, ch, (from_vnum == to_vnum));
					}
				}
				break;
			}
			case OLC_ARCHETYPE: {
				archetype_data *arch, *next_arch;
				HASH_ITER(hh, archetype_table, arch, next_arch) {
					if (GET_ARCH_VNUM(arch) >= from_vnum && GET_ARCH_VNUM(arch) <= to_vnum) {
						found |= audit_archetype(arch, ch);
					}
				}
				break;
			}
			case OLC_AUGMENT: {
				augment_data *aug, *next_aug;
				HASH_ITER(hh, augment_table, aug, next_aug) {
					if (GET_AUG_VNUM(aug) >= from_vnum && GET_AUG_VNUM(aug) <= to_vnum) {
						found |= audit_augment(aug, ch);
					}
				}
				break;
			}
			case OLC_BUILDING: {
				bld_data *bld, *next_bld;
				HASH_ITER(hh, building_table, bld, next_bld) {
					if (GET_BLD_VNUM(bld) >= from_vnum && GET_BLD_VNUM(bld) <= to_vnum) {
						found |= audit_building(bld, ch);
					}
				}
				break;
			}
			case OLC_CLASS: {
				class_data *cls, *next_cls;
				HASH_ITER(hh, class_table, cls, next_cls) {
					if (CLASS_VNUM(cls) >= from_vnum && CLASS_VNUM(cls) <= to_vnum) {
						found |= audit_class(cls, ch);
					}
				}
				break;
			}
			case OLC_CRAFT: {
				craft_data *craft, *next_craft;
				HASH_ITER(hh, craft_table, craft, next_craft) {
					if (GET_CRAFT_VNUM(craft) >= from_vnum && GET_CRAFT_VNUM(craft) <= to_vnum) {
						found |= audit_craft(craft, ch);
					}
				}
				break;
			}
			case OLC_CROP: {
				crop_data *cp, *next_cp;
				HASH_ITER(hh, crop_table, cp, next_cp) {
					if (GET_CROP_VNUM(cp) >= from_vnum && GET_CROP_VNUM(cp) <= to_vnum) {
						found |= audit_crop(cp, ch);
					}
				}
				break;
			}
			case OLC_EVENT: {
				event_data *event, *next_event;
				HASH_ITER(hh, event_table, event, next_event) {
					if (EVT_VNUM(event) >= from_vnum && EVT_VNUM(event) <= to_vnum) {
						found |= audit_event(event, ch);
					}
				}
				break;
			}
			case OLC_FACTION: {
				faction_data *fct, *next_fct;
				HASH_ITER(hh, faction_table, fct, next_fct) {
					if (FCT_VNUM(fct) >= from_vnum && FCT_VNUM(fct) <= to_vnum) {
						found |= audit_faction(fct, ch);
					}
				}
				break;
			}
			case OLC_GENERIC: {
				generic_data *gen, *next_gen;
				HASH_ITER(hh, generic_table, gen, next_gen) {
					if (GEN_VNUM(gen) >= from_vnum && GEN_VNUM(gen) <= to_vnum) {
						found |= audit_generic(gen, ch);
					}
				}
				break;
			}
			case OLC_GLOBAL: {
				struct global_data *glb, *next_glb;
				HASH_ITER(hh, globals_table, glb, next_glb) {
					if (GET_GLOBAL_VNUM(glb) >= from_vnum && GET_GLOBAL_VNUM(glb) <= to_vnum) {
						found |= audit_global(glb, ch);
					}
				}
				break;
			}
			case OLC_MOBILE: {
				char_data *mob, *next_mob;
				HASH_ITER(hh, mobile_table, mob, next_mob) {
					if (GET_MOB_VNUM(mob) >= from_vnum && GET_MOB_VNUM(mob) <= to_vnum) {
						found |= audit_mobile(mob, ch);
					}
				}
				break;
			}
			case OLC_MORPH: {
				morph_data *morph, *next_morph;
				HASH_ITER(hh, morph_table, morph, next_morph) {
					if (MORPH_VNUM(morph) >= from_vnum && MORPH_VNUM(morph) <= to_vnum) {
						found |= audit_morph(morph, ch);
					}
				}
				break;
			}
			case OLC_OBJECT: {
				obj_data *obj, *next_obj;
				HASH_ITER(hh, object_table, obj, next_obj) {
					if (GET_OBJ_VNUM(obj) >= from_vnum && GET_OBJ_VNUM(obj) <= to_vnum) {
						found |= audit_object(obj, ch);
					}
				}
				break;
			}
			case OLC_PROGRESS: {
				progress_data *prg, *next_prg;
				HASH_ITER(hh, progress_table, prg, next_prg) {
					if (PRG_VNUM(prg) >= from_vnum && PRG_VNUM(prg) <= to_vnum) {
						found |= audit_progress(prg, ch);
					}
				}
				break;
			}
			case OLC_QUEST: {
				quest_data *quest, *next_quest;
				HASH_ITER(hh, quest_table, quest, next_quest) {
					if (QUEST_VNUM(quest) >= from_vnum && QUEST_VNUM(quest) <= to_vnum) {
						found |= audit_quest(quest, ch);
					}
				}
				break;
			}
			case OLC_ROOM_TEMPLATE: {
				room_template *rmt, *next_rmt;
				HASH_ITER(hh, room_template_table, rmt, next_rmt) {
					if (GET_RMT_VNUM(rmt) >= from_vnum && GET_RMT_VNUM(rmt) <= to_vnum) {
						found |= audit_room_template(rmt, ch);
					}
				}
				break;
			}
			case OLC_SECTOR: {
				sector_data *sect, *next_sect;
				HASH_ITER(hh, sector_table, sect, next_sect) {
					if (GET_SECT_VNUM(sect) >= from_vnum && GET_SECT_VNUM(sect) <= to_vnum) {
						found |= audit_sector(sect, ch);
					}
				}
				break;
			}
			case OLC_SHOP: {
				shop_data *shop, *next_shop;
				HASH_ITER(hh, shop_table, shop, next_shop) {
					if (SHOP_VNUM(shop) >= from_vnum && SHOP_VNUM(shop) <= to_vnum) {
						found |= audit_shop(shop, ch);
					}
				}
				break;
			}
			case OLC_SKILL: {
				skill_data *skill, *next_skill;
				HASH_ITER(hh, skill_table, skill, next_skill) {
					if (SKILL_VNUM(skill) >= from_vnum && SKILL_VNUM(skill) <= to_vnum) {
						found |= audit_skill(skill, ch);
					}
				}
				break;
			}
			case OLC_SOCIAL: {
				social_data *soc, *next_soc;
				HASH_ITER(hh, social_table, soc, next_soc) {
					if (SOC_VNUM(soc) >= from_vnum && SOC_VNUM(soc) <= to_vnum) {
						found |= audit_social(soc, ch);
					}
				}
				break;
			}
			case OLC_TRIGGER: {
				trig_data *trig, *next_trig;
				HASH_ITER(hh, trigger_table, trig, next_trig) {
					if (GET_TRIG_VNUM(trig) >= from_vnum && GET_TRIG_VNUM(trig) <= to_vnum) {
						found |= audit_trigger(trig, ch);
					}
				}
				break;
			}
			case OLC_VEHICLE: {
				vehicle_data *veh, *next_veh;
				HASH_ITER(hh, vehicle_table, veh, next_veh) {
					if (VEH_VNUM(veh) >= from_vnum && VEH_VNUM(veh) <= to_vnum) {
						found |= audit_vehicle(veh, ch);
					}
				}
				break;
			}
			default: {
				msg_to_char(ch, "OLC auditing isn't available for that type.\r\n");
				return;
			}
		}
		if (!found) {
			msg_to_char(ch, "No problems found in that range.\r\n");
		}
	}
}


OLC_MODULE(olc_copy) {
	char arg2[MAX_INPUT_LENGTH];
	descriptor_data *desc;
	char typename[42];
	bool found, exists, ok = TRUE;
	any_vnum vnum, from_vnum;
	
	sprintbit(GET_OLC_TYPE(ch->desc) != 0 ? GET_OLC_TYPE(ch->desc) : type, olc_type_bits, typename, FALSE);
	
	half_chop(argument, arg, arg2);
	
	// check that they're not already editing something
	if (GET_OLC_VNUM(ch->desc) != NOTHING) {
		msg_to_char(ch, "You are already editing %s %s.\r\n", AN(typename), typename);
		return;
	}
	
	// argument checks
	if (!*arg || !*arg2) {
		msg_to_char(ch, "Usage: olc %s copy <from vnum> <to vnum>\r\n", typename);
		return;
	}
	if (!isdigit(*arg) || (from_vnum = atoi(arg)) < 0 || from_vnum > MAX_VNUM) {
		msg_to_char(ch, "You must pick a valid %s vnum between 0 and %d.\r\n", typename, MAX_VNUM);
		return;
	}
	if (!isdigit(*arg2) || (vnum = atoi(arg2)) < 0 || vnum > MAX_VNUM) {
		msg_to_char(ch, "You must pick a valid %s vnum between 0 and %d.\r\n", typename, MAX_VNUM);
		return;
	}
	
	found = FALSE;
	exists = FALSE;
	// OLC_x:
	switch (type) {
		case OLC_ABILITY: {
			found = (find_ability_by_vnum(vnum) != NULL);
			exists = (find_ability_by_vnum(from_vnum) != NULL);
			break;
		}
		case OLC_ADVENTURE: {
			found = (adventure_proto(vnum) != NULL);
			exists = (adventure_proto(from_vnum) != NULL);
			break;
		}
		case OLC_ARCHETYPE: {
			found = (archetype_proto(vnum) != NULL);
			exists = (archetype_proto(from_vnum) != NULL);
			break;
		}
		case OLC_AUGMENT: {
			found = (augment_proto(vnum) != NULL);
			exists = (augment_proto(from_vnum) != NULL);
			break;
		}
		case OLC_BOOK: {
			found = (book_proto(vnum) != NULL);
			exists = (book_proto(from_vnum) != NULL);
			break;
		}
		case OLC_BUILDING: {
			found = (building_proto(vnum) != NULL);
			exists = (building_proto(from_vnum) != NULL);
			break;
		}
		case OLC_CLASS: {
			found = (find_class_by_vnum(vnum) != NULL);
			exists = (find_class_by_vnum(from_vnum) != NULL);
			break;
		}
		case OLC_CRAFT: {
			found = (craft_proto(vnum) != NULL);
			exists = (craft_proto(from_vnum) != NULL);
			break;
		}
		case OLC_CROP: {
			found = (crop_proto(vnum) != NULL);
			exists = (crop_proto(from_vnum) != NULL);
			break;
		}
		case OLC_EVENT: {
			found = (find_event_by_vnum(vnum) != NULL);
			exists = (find_event_by_vnum(from_vnum) != NULL);
			break;
		}
		case OLC_FACTION: {
			found = (find_faction_by_vnum(vnum) != NULL);
			exists = (find_faction_by_vnum(from_vnum) != NULL);
			break;
		}
		case OLC_GENERIC: {
			found = (real_generic(vnum) != NULL);
			exists = (real_generic(from_vnum) != NULL);
			break;
		}
		case OLC_GLOBAL: {
			found = (global_proto(vnum) != NULL);
			exists = (global_proto(from_vnum) != NULL);
			break;
		}
		case OLC_MOBILE: {
			found = (mob_proto(vnum) != NULL);
			exists = (mob_proto(from_vnum) != NULL);
			break;
		}
		case OLC_MORPH: {
			found = (morph_proto(vnum) != NULL);
			exists = (morph_proto(from_vnum) != NULL);
			break;
		}
		case OLC_OBJECT: {
			found = (obj_proto(vnum) != NULL);
			exists = (obj_proto(from_vnum) != NULL);
			break;
		}
		case OLC_PROGRESS: {
			found = (real_progress(vnum) != NULL);
			exists = (real_progress(from_vnum) != NULL);
			break;
		}
		case OLC_QUEST: {
			found = (quest_proto(vnum) != NULL);
			exists = (quest_proto(from_vnum) != NULL);
			break;
		}
		case OLC_ROOM_TEMPLATE: {
			found = (room_template_proto(vnum) != NULL);
			exists = (room_template_proto(from_vnum) != NULL);
			break;
		}
		case OLC_SECTOR: {
			found = (sector_proto(vnum) != NULL);
			exists = (sector_proto(from_vnum) != NULL);
			break;
		}
		case OLC_SHOP: {
			found = (real_shop(vnum) != NULL);
			exists = (real_shop(from_vnum) != NULL);
			break;
		}
		case OLC_SKILL: {
			found = (find_skill_by_vnum(vnum) != NULL);
			exists = (find_skill_by_vnum(from_vnum) != NULL);
			break;
		}
		case OLC_SOCIAL: {
			found = (social_proto(vnum) != NULL);
			exists = (social_proto(from_vnum) != NULL);
			break;
		}
		case OLC_TRIGGER: {
			found = (real_trigger(vnum) != NULL);
			exists = (real_trigger(from_vnum) != NULL);
			break;
		}
		case OLC_VEHICLE: {
			found = (vehicle_proto(vnum) != NULL);
			exists = (vehicle_proto(from_vnum) != NULL);
			break;
		}
	}
	
	// copying something that exists?
	if (!exists) {
		msg_to_char(ch, "No such %s %d to copy.\r\n", typename, from_vnum);
		return;
	}
	
	// found one at the new vnum?
	if (found) {
		msg_to_char(ch, "%s %d already exists.\r\n", typename, vnum);
		return;
	}
	
	if (!player_can_olc_edit(ch, type, vnum)) {
		msg_to_char(ch, "You don't have permission to edit that vnum.\r\n");
		return;
	}
	
	if (type == OLC_ROOM_TEMPLATE && !valid_room_template_vnum(vnum)) {
		msg_to_char(ch, "Invalid room template: may be outside any adventure zone.\r\n");
		return;
	}
	
	// make sure nobody else is already editing it
	found = FALSE;
	for (desc = descriptor_list; desc && !found; desc = desc->next) {
		if (GET_OLC_TYPE(desc) == type && GET_OLC_VNUM(desc) == vnum) {
			found = TRUE;
		}
	}
	
	if (found) {
		msg_to_char(ch, "Someone else is already editing %s %d.\r\n", typename, vnum);
		return;
	}
	
	// SUCCESS
	msg_to_char(ch, "You copy %s %d to vnum %d and begin editing:\r\n", typename, from_vnum, vnum);
	GET_OLC_TYPE(ch->desc) = type;
	GET_OLC_VNUM(ch->desc) = vnum;
	
	// OLC_x: setup
	switch (type) {
		case OLC_ABILITY: {
			GET_OLC_ABILITY(ch->desc) = setup_olc_ability(find_ability_by_vnum(from_vnum));
			GET_OLC_ABILITY(ch->desc)->vnum = vnum;
			olc_show_ability(ch);
			break;
		}
		case OLC_ADVENTURE: {
			GET_OLC_ADVENTURE(ch->desc) = setup_olc_adventure(adventure_proto(from_vnum));
			GET_OLC_ADVENTURE(ch->desc)->vnum = vnum;
			SET_BIT(GET_OLC_ADVENTURE(ch->desc)->flags, ADV_IN_DEVELOPMENT);	// ensure flag
			olc_show_adventure(ch);
			break;
		}
		case OLC_ARCHETYPE: {
			GET_OLC_ARCHETYPE(ch->desc) = setup_olc_archetype(archetype_proto(from_vnum));
			GET_OLC_ARCHETYPE(ch->desc)->vnum = vnum;
			SET_BIT(GET_ARCH_FLAGS(GET_OLC_ARCHETYPE(ch->desc)), ARCH_IN_DEVELOPMENT);	// ensure flag
			olc_show_archetype(ch);
			break;
		}
		case OLC_AUGMENT: {
			GET_OLC_AUGMENT(ch->desc) = setup_olc_augment(augment_proto(from_vnum));
			GET_OLC_AUGMENT(ch->desc)->vnum = vnum;
			SET_BIT(GET_AUG_FLAGS(GET_OLC_AUGMENT(ch->desc)), AUG_IN_DEVELOPMENT);	// ensure flag
			olc_show_augment(ch);
			break;
		}
		case OLC_BOOK: {
			GET_OLC_BOOK(ch->desc) = setup_olc_book(book_proto(from_vnum));
			GET_OLC_BOOK(ch->desc)->vnum = vnum;
			olc_show_book(ch);
			break;
		}
		case OLC_BUILDING: {
			GET_OLC_BUILDING(ch->desc) = setup_olc_building(building_proto(from_vnum));
			GET_OLC_BUILDING(ch->desc)->vnum = vnum;
			olc_show_building(ch);
			break;
		}
		case OLC_CLASS: {
			GET_OLC_CLASS(ch->desc) = setup_olc_class(find_class_by_vnum(from_vnum));
			GET_OLC_CLASS(ch->desc)->vnum = vnum;
			SET_BIT(GET_OLC_CLASS(ch->desc)->flags, CLASSF_IN_DEVELOPMENT);	// ensure flag
			olc_show_class(ch);
			break;
		}
		case OLC_CRAFT: {
			GET_OLC_CRAFT(ch->desc) = setup_olc_craft(craft_proto(from_vnum));
			GET_OLC_CRAFT(ch->desc)->vnum = vnum;
			SET_BIT(GET_OLC_CRAFT(ch->desc)->flags, CRAFT_IN_DEVELOPMENT);	// ensure flag			
			olc_show_craft(ch);
			break;
		}
		case OLC_CROP: {
			GET_OLC_CROP(ch->desc) = setup_olc_crop(crop_proto(from_vnum));
			GET_OLC_CROP(ch->desc)->vnum = vnum;
			SET_BIT(GET_OLC_CROP(ch->desc)->flags, CROPF_NOT_WILD);	// ensure flag
			olc_show_crop(ch);
			break;
		}
		case OLC_EVENT: {
			GET_OLC_EVENT(ch->desc) = setup_olc_event(find_event_by_vnum(from_vnum));
			GET_OLC_EVENT(ch->desc)->vnum = vnum;
			SET_BIT(EVT_FLAGS(GET_OLC_EVENT(ch->desc)), EVTF_IN_DEVELOPMENT);	// ensure flag
			olc_show_event(ch);
			break;
		}
		case OLC_FACTION: {
			GET_OLC_FACTION(ch->desc) = setup_olc_faction(find_faction_by_vnum(from_vnum));
			GET_OLC_FACTION(ch->desc)->vnum = vnum;
			SET_BIT(FCT_FLAGS(GET_OLC_FACTION(ch->desc)), FCT_IN_DEVELOPMENT);	// ensure flag
			olc_show_faction(ch);
			break;
		}
		case OLC_GENERIC: {
			GET_OLC_GENERIC(ch->desc) = setup_olc_generic(real_generic(from_vnum));
			GET_OLC_GENERIC(ch->desc)->vnum = vnum;
			olc_show_generic(ch);
			break;
		}
		case OLC_GLOBAL: {
			GET_OLC_GLOBAL(ch->desc) = setup_olc_global(global_proto(from_vnum));
			GET_OLC_GLOBAL(ch->desc)->vnum = vnum;
			SET_BIT(GET_GLOBAL_FLAGS(GET_OLC_GLOBAL(ch->desc)), GLB_FLAG_IN_DEVELOPMENT);	// ensure flag
			olc_show_global(ch);
			break;
		}
		case OLC_MOBILE: {
			// copy over
			GET_OLC_MOBILE(ch->desc) = setup_olc_mobile(mob_proto(from_vnum));
			GET_OLC_MOBILE(ch->desc)->vnum = vnum;
			olc_show_mobile(ch);
			break;
		}
		case OLC_MORPH: {
			GET_OLC_MORPH(ch->desc) = setup_olc_morph(morph_proto(from_vnum));
			GET_OLC_MORPH(ch->desc)->vnum = vnum;
			SET_BIT(MORPH_FLAGS(GET_OLC_MORPH(ch->desc)), MORPHF_IN_DEVELOPMENT);	// ensure flag
			olc_show_morph(ch);
			break;
		}
		case OLC_OBJECT: {
			// copy over the from-vnum
			GET_OLC_OBJECT(ch->desc) = setup_olc_object(obj_proto(from_vnum));
			GET_OLC_OBJECT(ch->desc)->vnum = vnum;
			olc_show_object(ch);
			break;
		}
		case OLC_PROGRESS: {
			GET_OLC_PROGRESS(ch->desc) = setup_olc_progress(real_progress(from_vnum));
			GET_OLC_PROGRESS(ch->desc)->vnum = vnum;
			SET_BIT(PRG_FLAGS(GET_OLC_PROGRESS(ch->desc)), PRG_IN_DEVELOPMENT);	// ensure flag
			olc_show_progress(ch);
			break;
		}
		case OLC_QUEST: {
			GET_OLC_QUEST(ch->desc) = setup_olc_quest(quest_proto(from_vnum));
			GET_OLC_QUEST(ch->desc)->vnum = vnum;
			SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(ch->desc)), QST_IN_DEVELOPMENT);	// ensure flag
			olc_show_quest(ch);
			break;
		}
		case OLC_ROOM_TEMPLATE: {
			GET_OLC_ROOM_TEMPLATE(ch->desc) = setup_olc_room_template(room_template_proto(from_vnum));
			GET_OLC_ROOM_TEMPLATE(ch->desc)->vnum = vnum;			
			olc_show_room_template(ch);
			break;
		}
		case OLC_SECTOR: {
			GET_OLC_SECTOR(ch->desc) = setup_olc_sector(sector_proto(from_vnum));
			GET_OLC_SECTOR(ch->desc)->vnum = vnum;
			olc_show_sector(ch);
			break;
		}
		case OLC_SHOP: {
			GET_OLC_SHOP(ch->desc) = setup_olc_shop(real_shop(from_vnum));
			GET_OLC_SHOP(ch->desc)->vnum = vnum;
			SET_BIT(SHOP_FLAGS(GET_OLC_SHOP(ch->desc)), SHOP_IN_DEVELOPMENT);	// ensure flag
			olc_show_shop(ch);
			break;
		}
		case OLC_SKILL: {
			GET_OLC_SKILL(ch->desc) = setup_olc_skill(find_skill_by_vnum(from_vnum));
			GET_OLC_SKILL(ch->desc)->vnum = vnum;
			SET_BIT(GET_OLC_SKILL(ch->desc)->flags, SKILLF_IN_DEVELOPMENT);	// ensure flag
			olc_show_skill(ch);
			break;
		}
		case OLC_SOCIAL: {
			GET_OLC_SOCIAL(ch->desc) = setup_olc_social(social_proto(from_vnum));
			GET_OLC_SOCIAL(ch->desc)->vnum = vnum;
			SET_BIT(GET_OLC_SOCIAL(ch->desc)->flags, SOC_IN_DEVELOPMENT);	// ensure flag
			olc_show_social(ch);
			break;
		}
		case OLC_TRIGGER: {
			GET_OLC_TRIGGER(ch->desc) = setup_olc_trigger(real_trigger(from_vnum), &GET_OLC_STORAGE(ch->desc));
			GET_OLC_TRIGGER(ch->desc)->vnum = vnum;
			olc_show_trigger(ch);
			break;
		}
		case OLC_VEHICLE: {
			GET_OLC_VEHICLE(ch->desc) = setup_olc_vehicle(vehicle_proto(from_vnum));
			GET_OLC_VEHICLE(ch->desc)->vnum = vnum;
			olc_show_vehicle(ch);
			break;
		}
		default: {
			ok = FALSE;
			break;
		}
	}
	
	// oops?
	if (!ok) {
		GET_OLC_TYPE(ch->desc) = 0;
		GET_OLC_VNUM(ch->desc) = NOTHING;
	}
}


OLC_MODULE(olc_delete) {
	void olc_delete_ability(char_data *ch, any_vnum vnum);
	void olc_delete_adventure(char_data *ch, adv_vnum vnum);
	void olc_delete_archetype(char_data *ch, any_vnum vnum);
	void olc_delete_augment(char_data *ch, any_vnum vnum);
	void olc_delete_book(char_data *ch, book_vnum vnum);
	void olc_delete_building(char_data *ch, bld_vnum vnum);
	void olc_delete_class(char_data *ch, any_vnum vnum);
	void olc_delete_craft(char_data *ch, craft_vnum vnum);
	void olc_delete_crop(char_data *ch, crop_vnum vnum);
	void olc_delete_global(char_data *ch, any_vnum vnum);
	void olc_delete_mobile(char_data *ch, mob_vnum vnum);
	void olc_delete_morph(char_data *ch, any_vnum vnum);
	void olc_delete_object(char_data *ch, obj_vnum vnum);
	void olc_delete_room_template(char_data *ch, rmt_vnum vnum);
	void olc_delete_sector(char_data *ch, sector_vnum vnum);
	void olc_delete_skill(char_data *ch, any_vnum vnum);
	void olc_delete_social(char_data *ch, any_vnum vnum);
	void olc_delete_trigger(char_data *ch, trig_vnum vnum);
	void olc_delete_vehicle(char_data *ch, any_vnum vnum);
	
	descriptor_data *desc;
	char typename[42];
	bool found;
	any_vnum vnum;
	
	sprintbit(type, olc_type_bits, typename, FALSE);
	
	if (!*argument) {
		msg_to_char(ch, "Delete which %s (vnum)?\r\n", typename);
		return;
	}
	if (!isdigit(*argument) || (vnum = atoi(argument)) < 0 || vnum > MAX_VNUM) {
		msg_to_char(ch, "You must pick a valid %s vnum between 0 and %d.\r\n", typename, MAX_VNUM);
		return;
	}
	
	if (!player_can_olc_edit(ch, type, vnum)) {
		msg_to_char(ch, "You don't have permission to delete that vnum.\r\n");
		return;
	}
	
	// make sure nobody else is editing it
	found = FALSE;
	for (desc = descriptor_list; desc && !found; desc = desc->next) {
		if (GET_OLC_TYPE(desc) == type && GET_OLC_VNUM(desc) == vnum) {
			found = TRUE;
		}
	}
	
	if (found) {
		msg_to_char(ch, "Someone else is currently editing that %s.\r\n", typename);
		return;
	}
	
	// OLC_x: success by type
	switch (type) {
		case OLC_ABILITY: {
			olc_delete_ability(ch, vnum);
			break;
		}
		case OLC_ADVENTURE: {
			olc_delete_adventure(ch, vnum);
			break;
		}
		case OLC_ARCHETYPE: {
			olc_delete_archetype(ch, vnum);
			break;
		}
		case OLC_AUGMENT: {
			olc_delete_augment(ch, vnum);
			break;
		}
		case OLC_BOOK: {
			olc_delete_book(ch, vnum);
			break;
		}
		case OLC_BUILDING: {
			olc_delete_building(ch, vnum);
			break;
		}
		case OLC_CLASS: {
			olc_delete_class(ch, vnum);
			break;
		}
		case OLC_CRAFT: {
			olc_delete_craft(ch, vnum);
			break;
		}
		case OLC_CROP: {
			olc_delete_crop(ch, vnum);
			break;
		}
		case OLC_EVENT: {
			void olc_delete_event(char_data *ch, any_vnum vnum);
			olc_delete_event(ch, vnum);
			break;
		}
		case OLC_FACTION: {
			void olc_delete_faction(char_data *ch, any_vnum vnum);
			olc_delete_faction(ch, vnum);
			break;
		}
		case OLC_GENERIC: {
			void olc_delete_generic(char_data *ch, any_vnum vnum);
			olc_delete_generic(ch, vnum);
			break;
		}
		case OLC_GLOBAL: {
			olc_delete_global(ch, vnum);
			break;
		}
		case OLC_MOBILE: {
			olc_delete_mobile(ch, vnum);
			break;
		}
		case OLC_MORPH: {
			olc_delete_morph(ch, vnum);
			break;
		}
		case OLC_OBJECT: {
			olc_delete_object(ch, vnum);
			break;
		}
		case OLC_PROGRESS: {
			void olc_delete_progress(char_data *ch, any_vnum vnum);
			olc_delete_progress(ch, vnum);
			break;
		}
		case OLC_QUEST: {
			void olc_delete_quest(char_data *ch, any_vnum vnum);
			olc_delete_quest(ch, vnum);
			break;
		}
		case OLC_ROOM_TEMPLATE: {
			olc_delete_room_template(ch, vnum);
			break;
		}
		case OLC_SECTOR: {
			olc_delete_sector(ch, vnum);
			break;
		}
		case OLC_SHOP: {
			void olc_delete_shop(char_data *ch, any_vnum vnum);
			olc_delete_shop(ch, vnum);
			break;
		}
		case OLC_SKILL: {
			olc_delete_skill(ch, vnum);
			break;
		}
		case OLC_SOCIAL: {
			olc_delete_social(ch, vnum);
			break;
		}
		case OLC_TRIGGER: {
			olc_delete_trigger(ch, vnum);
			break;
		}
		case OLC_VEHICLE: {
			olc_delete_vehicle(ch, vnum);
			break;
		}
		default: {
			msg_to_char(ch, "You can't delete that.\r\n");
			break;
		}
	}
}


OLC_MODULE(olc_display) {
	// OLC_x:
	switch (GET_OLC_TYPE(ch->desc)) {
		case OLC_ABILITY: {
			olc_show_ability(ch);
			break;
		}
		case OLC_ADVENTURE: {
			olc_show_adventure(ch);
			break;
		}
		case OLC_ARCHETYPE: {
			olc_show_archetype(ch);
			break;
		}
		case OLC_AUGMENT: {
			olc_show_augment(ch);
			break;
		}
		case OLC_BOOK: {
			olc_show_book(ch);
			break;
		}
		case OLC_BUILDING: {
			olc_show_building(ch);
			break;
		}
		case OLC_CLASS: {
			olc_show_class(ch);
			break;
		}
		case OLC_CRAFT: {
			olc_show_craft(ch);
			break;
		}
		case OLC_CROP: {
			olc_show_crop(ch);
			break;
		}
		case OLC_EVENT: {
			olc_show_event(ch);
			break;
		}
		case OLC_FACTION: {
			olc_show_faction(ch);
			break;
		}
		case OLC_GENERIC: {
			olc_show_generic(ch);
			break;
		}
		case OLC_GLOBAL: {
			olc_show_global(ch);
			break;
		}
		case OLC_MOBILE: {
			olc_show_mobile(ch);
			break;
		}
		case OLC_MORPH: {
			olc_show_morph(ch);
			break;
		}
		case OLC_OBJECT: {
			olc_show_object(ch);
			break;
		}
		case OLC_PROGRESS: {
			olc_show_progress(ch);
			break;
		}
		case OLC_QUEST: {
			olc_show_quest(ch);
			break;
		}
		case OLC_ROOM_TEMPLATE: {
			olc_show_room_template(ch);
			break;
		}
		case OLC_SECTOR: {
			olc_show_sector(ch);
			break;
		}
		case OLC_SHOP: {
			olc_show_shop(ch);
			break;
		}
		case OLC_SKILL: {
			olc_show_skill(ch);
			break;
		}
		case OLC_SOCIAL: {
			olc_show_social(ch);
			break;
		}
		case OLC_TRIGGER: {
			olc_show_trigger(ch);
			break;
		}
		case OLC_VEHICLE: {
			olc_show_vehicle(ch);
			break;
		}
		default: {
			msg_to_char(ch, "You aren't currently editing anything.\r\n");
			break;
		}
	}
}


OLC_MODULE(olc_edit) {	
	char typename[42];
	bool ok = TRUE;
	any_vnum vnum;
	
	sprintbit(GET_OLC_TYPE(ch->desc) != 0 ? GET_OLC_TYPE(ch->desc) : type, olc_type_bits, typename, FALSE);
	
	if (!*argument) {
		msg_to_char(ch, "Edit which %s (vnum)?\r\n", typename);
		return;
	}
	if (!isdigit(*argument) || (vnum = atoi(argument)) < 0 || vnum > MAX_VNUM) {
		msg_to_char(ch, "You must pick a valid %s vnum between 0 and %d.\r\n", typename, MAX_VNUM);
		return;
	}
	if (!can_start_olc_edit(ch, type, vnum)) {
		return;	// sends own message
	}
	
	// SUCCESS
	msg_to_char(ch, "You begin editing %s %d:\r\n", typename, vnum);
	GET_OLC_TYPE(ch->desc) = type;
	GET_OLC_VNUM(ch->desc) = vnum;
	
	// OLC_x: setup
	switch (type) {
		case OLC_ABILITY: {
			// this sets up either new or existing automatically
			GET_OLC_ABILITY(ch->desc) = setup_olc_ability(find_ability_by_vnum(vnum));
			GET_OLC_ABILITY(ch->desc)->vnum = vnum;
			olc_show_ability(ch);
			break;
		}
		case OLC_ADVENTURE: {
			// this sets up either new or existing automatically
			GET_OLC_ADVENTURE(ch->desc) = setup_olc_adventure(adventure_proto(vnum));
			GET_OLC_ADVENTURE(ch->desc)->vnum = vnum;
			olc_show_adventure(ch);
			break;
		}
		case OLC_ARCHETYPE: {
			// this will set up from existing OR new automatically
			GET_OLC_ARCHETYPE(ch->desc) = setup_olc_archetype(archetype_proto(vnum));
			GET_OLC_ARCHETYPE(ch->desc)->vnum = vnum;			
			olc_show_archetype(ch);
			break;
		}
		case OLC_AUGMENT: {
			// this will set up from existing OR new automatically
			GET_OLC_AUGMENT(ch->desc) = setup_olc_augment(augment_proto(vnum));
			GET_OLC_AUGMENT(ch->desc)->vnum = vnum;			
			olc_show_augment(ch);
			break;
		}
		case OLC_BOOK: {
			// this sets up either new or existing automatically
			GET_OLC_BOOK(ch->desc) = setup_olc_book(book_proto(vnum));
			GET_OLC_BOOK(ch->desc)->vnum = vnum;			
			olc_show_book(ch);
			break;
		}
		case OLC_BUILDING: {
			// this sets up either new or existing automatically
			GET_OLC_BUILDING(ch->desc) = setup_olc_building(building_proto(vnum));
			GET_OLC_BUILDING(ch->desc)->vnum = vnum;			
			olc_show_building(ch);
			break;
		}
		case OLC_CLASS: {
			// this sets up either new or existing automatically
			GET_OLC_CLASS(ch->desc) = setup_olc_class(find_class_by_vnum(vnum));
			GET_OLC_CLASS(ch->desc)->vnum = vnum;
			olc_show_class(ch);
			break;
		}
		case OLC_CRAFT: {
			// this sets up either new or existing automatically
			GET_OLC_CRAFT(ch->desc) = setup_olc_craft(craft_proto(vnum));
			GET_OLC_CRAFT(ch->desc)->vnum = vnum;			
			olc_show_craft(ch);
			break;
		}
		case OLC_CROP: {
			// this will set up from existing OR new automatically based on crop_proto
			GET_OLC_CROP(ch->desc) = setup_olc_crop(crop_proto(vnum));
			GET_OLC_CROP(ch->desc)->vnum = vnum;			
			olc_show_crop(ch);
			break;
		}
		case OLC_EVENT: {
			// this will set up from existing OR new automatically based on find_event_by_vnum
			GET_OLC_EVENT(ch->desc) = setup_olc_event(find_event_by_vnum(vnum));
			GET_OLC_EVENT(ch->desc)->vnum = vnum;			
			olc_show_event(ch);
			break;
		}
		case OLC_FACTION: {
			// this will set up from existing OR new automatically based on find_faction_by_vnum
			GET_OLC_FACTION(ch->desc) = setup_olc_faction(find_faction_by_vnum(vnum));
			GET_OLC_FACTION(ch->desc)->vnum = vnum;			
			olc_show_faction(ch);
			break;
		}
		case OLC_GENERIC: {
			// this will set up from existing OR new automatically based on real_generic
			GET_OLC_GENERIC(ch->desc) = setup_olc_generic(real_generic(vnum));
			GET_OLC_GENERIC(ch->desc)->vnum = vnum;			
			olc_show_generic(ch);
			break;
		}
		case OLC_GLOBAL: {
			// this will set up from existing OR new automatically based on global_proto
			GET_OLC_GLOBAL(ch->desc) = setup_olc_global(global_proto(vnum));
			GET_OLC_GLOBAL(ch->desc)->vnum = vnum;			
			olc_show_global(ch);
			break;
		}
		case OLC_MOBILE: {
			// this will set up from existing OR new automatically
			GET_OLC_MOBILE(ch->desc) = setup_olc_mobile(mob_proto(vnum));
			GET_OLC_MOBILE(ch->desc)->vnum = vnum;
			olc_show_mobile(ch);
			break;
		}
		case OLC_MORPH: {
			// this will set up from existing OR new automatically
			GET_OLC_MORPH(ch->desc) = setup_olc_morph(morph_proto(vnum));
			GET_OLC_MORPH(ch->desc)->vnum = vnum;			
			olc_show_morph(ch);
			break;
		}
		case OLC_OBJECT: {
			// this will set up from existing OR new automatically
			GET_OLC_OBJECT(ch->desc) = setup_olc_object(obj_proto(vnum));
			GET_OLC_OBJECT(ch->desc)->vnum = vnum;
			olc_show_object(ch);
			break;
		}
		case OLC_PROGRESS: {
			// this will set up from existing OR new automatically
			GET_OLC_PROGRESS(ch->desc) = setup_olc_progress(real_progress(vnum));
			GET_OLC_PROGRESS(ch->desc)->vnum = vnum;			
			olc_show_progress(ch);
			break;
		}
		case OLC_QUEST: {
			// this will set up from existing OR new automatically
			GET_OLC_QUEST(ch->desc) = setup_olc_quest(quest_proto(vnum));
			GET_OLC_QUEST(ch->desc)->vnum = vnum;			
			olc_show_quest(ch);
			break;
		}
		case OLC_ROOM_TEMPLATE: {
			// this will set up from existing OR new automatically
			GET_OLC_ROOM_TEMPLATE(ch->desc) = setup_olc_room_template(room_template_proto(vnum));
			GET_OLC_ROOM_TEMPLATE(ch->desc)->vnum = vnum;
			olc_show_room_template(ch);
			break;
		}
		case OLC_SECTOR: {
			// this will set up from existing OR new automatically based on sector_proto
			GET_OLC_SECTOR(ch->desc) = setup_olc_sector(sector_proto(vnum));
			GET_OLC_SECTOR(ch->desc)->vnum = vnum;
			olc_show_sector(ch);
			break;
		}
		case OLC_SHOP: {
			// this will set up from existing OR new automatically based on real_shop
			GET_OLC_SHOP(ch->desc) = setup_olc_shop(real_shop(vnum));
			GET_OLC_SHOP(ch->desc)->vnum = vnum;			
			olc_show_shop(ch);
			break;
		}
		case OLC_SKILL: {
			// this sets up either new or existing automatically
			GET_OLC_SKILL(ch->desc) = setup_olc_skill(find_skill_by_vnum(vnum));
			GET_OLC_SKILL(ch->desc)->vnum = vnum;
			olc_show_skill(ch);
			break;
		}
		case OLC_SOCIAL: {
			// this sets up either new or existing automatically
			GET_OLC_SOCIAL(ch->desc) = setup_olc_social(social_proto(vnum));
			GET_OLC_SOCIAL(ch->desc)->vnum = vnum;
			olc_show_social(ch);
			break;
		}
		case OLC_TRIGGER: {
			// this will set up from existing OR new automatically
			GET_OLC_TRIGGER(ch->desc) = setup_olc_trigger(real_trigger(vnum), &GET_OLC_STORAGE(ch->desc));
			GET_OLC_TRIGGER(ch->desc)->vnum = vnum;			
			olc_show_trigger(ch);
			break;
		}
		case OLC_VEHICLE: {
			// this will set up from existing OR new automatically
			GET_OLC_VEHICLE(ch->desc) = setup_olc_vehicle(vehicle_proto(vnum));
			GET_OLC_VEHICLE(ch->desc)->vnum = vnum;			
			olc_show_vehicle(ch);
			break;
		}
		default: {
			ok = FALSE;
			break;
		}
	}
	
	// validation?
	if (!ok) {
		GET_OLC_TYPE(ch->desc) = 0;
		GET_OLC_VNUM(ch->desc) = NOTHING;
	}
}


// Usage: olc mob free <from vnum> [to vnum]
OLC_MODULE(olc_free) {
	char buf2[MAX_STRING_LENGTH], arg2[MAX_INPUT_LENGTH];
	any_vnum from_vnum = NOTHING, to_vnum = NOTHING, iter;
	bool free = FALSE;
	
	// allow - or :
	for (iter = 0; iter < strlen(argument); ++iter) {
		if (argument[iter] == '-' || argument[iter] == ':') {
			argument[iter] = ' ';
		}
	}
	half_chop(argument, arg, arg2);
	
	if (!*arg || !isdigit(*arg) || (*arg2 && !isdigit(*arg2))) {
		msg_to_char(ch, "Usage: free <from vnum> [to vnum]\r\n");
	}
	else if ((from_vnum = atoi(arg)) < 0 || (to_vnum = atoi(arg2)) < 0 ) {
		msg_to_char(ch, "Invalid vnum range.\r\n");
	}
	else {
		// 2nd vnum optional
		if (!*arg2) {
			to_vnum = MAX_VNUM;
		}
		
		// iterate over range finding a vnum that's not used
		for (iter = from_vnum; iter <= to_vnum && !free; ++iter) {
			// OLC_x:
			switch (type) {
				case OLC_ABILITY: {
					free = (find_ability_by_vnum(iter) == NULL);
					break;
				}
				case OLC_ADVENTURE: {
					free = (adventure_proto(iter) == NULL);
					break;
				}
				case OLC_ARCHETYPE: {
					free = (archetype_proto(iter) == NULL);
					break;
				}
				case OLC_AUGMENT: {
					free = (augment_proto(iter) == NULL);
					break;
				}
				case OLC_BOOK: {
					free = (book_proto(iter) == NULL);
					break;
				}
				case OLC_BUILDING: {
					free = (building_proto(iter) == NULL);
					break;
				}
				case OLC_CLASS: {
					free = (find_class_by_vnum(iter) == NULL);
					break;
				}
				case OLC_CRAFT: {
					free = (craft_proto(iter) == NULL);
					break;
				}
				case OLC_CROP: {
					free = (crop_proto(iter) == NULL);
					break;
				}
				case OLC_EVENT: {
					free = (find_event_by_vnum(iter) == NULL);
					break;
				}
				case OLC_FACTION: {
					free = (find_faction_by_vnum(iter) == NULL);
					break;
				}
				case OLC_GENERIC: {
					free = (real_generic(iter) == NULL);
					break;
				}
				case OLC_GLOBAL: {
					free = (global_proto(iter) == NULL);
					break;
				}
				case OLC_MOBILE: {
					free = (mob_proto(iter) == NULL);
					break;
				}
				case OLC_MORPH: {
					free = (morph_proto(iter) == NULL);
					break;
				}
				case OLC_OBJECT: {
					free = (obj_proto(iter) == NULL);
					break;
				}
				case OLC_PROGRESS: {
					free = (real_progress(iter) == NULL);
					break;
				}
				case OLC_QUEST: {
					free = (quest_proto(iter) == NULL);
					break;
				}
				case OLC_ROOM_TEMPLATE: {
					free = (room_template_proto(iter) == NULL);
					break;
				}
				case OLC_SECTOR: {
					free = (sector_proto(iter) == NULL);
					break;
				}
				case OLC_SHOP: {
					free = (real_shop(iter) == NULL);
					break;
				}
				case OLC_SKILL: {
					free = (find_skill_by_vnum(iter) == NULL);
					break;
				}
				case OLC_SOCIAL: {
					free = (social_proto(iter) == NULL);
					break;
				}
				case OLC_TRIGGER: {
					free = (real_trigger(iter) == NULL);
					break;
				}
				case OLC_VEHICLE: {
					free = (vehicle_proto(iter) == NULL);
					break;
				}
				default: {
					msg_to_char(ch, "This command isn't implemented correctly for that type.\r\n");
					return;
				}
			}
			
			if (free) {
				sprintbit(type, olc_type_bits, buf2, FALSE);
				msg_to_char(ch, "First free %s vnum: %d\r\n", buf2, iter);
				
				// done
				return;
			}
		}
		
		msg_to_char(ch, "No free vnums found in that range.\r\n");
	}
}


OLC_MODULE(olc_fullsearch) {
	skip_spaces(&argument);
	
	// OLC_x:
	switch (type) {
		case OLC_ABILITY: {
			void olc_fullsearch_abil(char_data *ch, char *argument);
			olc_fullsearch_abil(ch, argument);
			break;
		}
		case OLC_BUILDING: {
			void olc_fullsearch_building(char_data *ch, char *argument);
			olc_fullsearch_building(ch, argument);
			break;
		}
		case OLC_CROP: {
			void olc_fullsearch_crop(char_data *ch, char *argument);
			olc_fullsearch_crop(ch, argument);
			break;
		}
		case OLC_MOBILE: {
			void olc_fullsearch_mob(char_data *ch, char *argument);
			olc_fullsearch_mob(ch, argument);
			break;
		}
		case OLC_OBJECT: {
			void olc_fullsearch_obj(char_data *ch, char *argument);
			olc_fullsearch_obj(ch, argument);
			break;
		}
		case OLC_PROGRESS: {
			void olc_fullsearch_progress(char_data *ch, char *argument);
			olc_fullsearch_progress(ch, argument);
			break;
		}
		case OLC_TRIGGER: {
			void olc_fullsearch_trigger(char_data *ch, char *argument);
			olc_fullsearch_trigger(ch, argument);
			break;
		}
		case OLC_VEHICLE: {
			void olc_fullsearch_vehicle(char_data *ch, char *argument);
			olc_fullsearch_vehicle(ch, argument);
			break;
		}
		default: {
			msg_to_char(ch, "It doesn't seem to be implemented for that type.\r\n");
			break;
		}
	}
}


// Usage: olc mob list <from vnum> [to vnum]
OLC_MODULE(olc_list) {
	char buf[MAX_STRING_LENGTH*2], buf1[MAX_STRING_LENGTH*2], buf2[MAX_STRING_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	any_vnum from_vnum = NOTHING, to_vnum = NOTHING, iter;
	bool found_from = FALSE, found_to = FALSE;
	bool show_details = FALSE;
	int count = 0, len;
	
	skip_spaces(&argument);
	
	// allow - or : before the first space
	for (iter = 0; iter < strlen(argument); ++iter) {
		if (argument[iter] == '-' || argument[iter] == ':') {
			argument[iter] = ' ';
		}
		else if (argument[iter] == ' ') {
			break;
		}
	}
	
	// process argument
	while (*argument) {
		argument = any_one_arg(argument, arg);
		
		// first argument must be a "from" vnum
		if (!found_from) {
			if (!isdigit(*arg)) {
				msg_to_char(ch, "Invalid vnum.\r\n");
				return;
			}
			from_vnum = atoi(arg);
			found_from = TRUE;
			continue;
		}
		
		if (!found_to && isdigit(*arg)) {
			to_vnum = atoi(arg);
			found_to = TRUE;
			continue;
		}
		
		// other types of args:
		if ((!strn_cmp(arg, "-d", 2) && is_abbrev(arg, "-details")) || (!strn_cmp(arg, "--d", 3) && is_abbrev(arg, "--details"))) {
			show_details = TRUE;
			continue;
		}
		
		// reached an error
		msg_to_char(ch, "Usage: list <from vnum> [to vnum] [-d]\r\n");
		return;
	}
	
	if (!found_from) {	// no useful args
		msg_to_char(ch, "Usage: list <from vnum> [to vnum] [-d]\r\n");
	}
	else if (from_vnum < 0) {
		msg_to_char(ch, "Invalid vnum range.\r\n");
	}
	else {
		// 2nd vnum optional
		if (to_vnum == NOTHING) {
			to_vnum = from_vnum;
		}
	
		*buf = '\0';
		len = 0;
		
		// OLC_x:
		switch (type) {
			case OLC_ABILITY: {
				extern char *list_one_ability(ability_data *abil, bool detail);
				ability_data *abil, *next_abil;
				HASH_ITER(hh, ability_table, abil, next_abil) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (ABIL_VNUM(abil) >= from_vnum && ABIL_VNUM(abil) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_ability(abil, show_details));
					}
				}
				break;
			}
			case OLC_ADVENTURE: {
				extern char *list_one_adventure(adv_data *adv, bool detail);
				adv_data *adv, *next_adv;
				HASH_ITER(hh, adventure_table, adv, next_adv) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_ADV_VNUM(adv) >= from_vnum && GET_ADV_VNUM(adv) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_adventure(adv, show_details));
					}
				}
				break;
			}
			case OLC_ARCHETYPE: {
				extern char *list_one_archetype(archetype_data *arch, bool detail);
				struct archetype_data *arch, *next_arch;
				HASH_ITER(hh, archetype_table, arch, next_arch) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_ARCH_VNUM(arch) >= from_vnum && GET_ARCH_VNUM(arch) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_archetype(arch, show_details));
					}
				}
				break;
			}
			case OLC_AUGMENT: {
				extern char *list_one_augment(augment_data *aug, bool detail);
				struct augment_data *aug, *next_aug;
				HASH_ITER(hh, augment_table, aug, next_aug) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_AUG_VNUM(aug) >= from_vnum && GET_AUG_VNUM(aug) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_augment(aug, show_details));
					}
				}
				break;
			}
			case OLC_BOOK: {
				extern char *list_one_book(book_data *book, bool detail);
				book_data *book, *next_book;
				HASH_ITER(hh, book_table, book, next_book) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (book->vnum >= from_vnum && book->vnum <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_book(book, show_details));
					}
				}
				break;
			}
			case OLC_BUILDING: {
				extern char *list_one_building(bld_data *bld, bool detail);
				bld_data *bld, *next_bld;
				HASH_ITER(hh, building_table, bld, next_bld) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_BLD_VNUM(bld) >= from_vnum && GET_BLD_VNUM(bld) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_building(bld, show_details));
					}
				}
				break;
			}
			case OLC_CLASS: {
				extern char *list_one_class(class_data *cls, bool detail);
				class_data *cls, *next_cls;
				HASH_ITER(hh, class_table, cls, next_cls) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (CLASS_VNUM(cls) >= from_vnum && CLASS_VNUM(cls) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_class(cls, show_details));
					}
				}
				break;
			}
			case OLC_CRAFT: {
				extern char *list_one_craft(craft_data *craft, bool detail);
				craft_data *craft, *next_craft;
				HASH_ITER(hh, craft_table, craft, next_craft) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_CRAFT_VNUM(craft) >= from_vnum && GET_CRAFT_VNUM(craft) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_craft(craft, show_details));
					}
				}
				break;
			}
			case OLC_CROP: {
				extern char *list_one_crop(crop_data *crop, bool detail);
				crop_data *crop, *next_crop;
				HASH_ITER(hh, crop_table, crop, next_crop) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_CROP_VNUM(crop) >= from_vnum && GET_CROP_VNUM(crop) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_crop(crop, show_details));
					}
				}
				break;
			}
			case OLC_EVENT: {
				extern char *list_one_event(event_data *event, bool detail);
				event_data *event, *next_event;
				HASH_ITER(hh, event_table, event, next_event) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (EVT_VNUM(event) >= from_vnum && EVT_VNUM(event) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_event(event, show_details));
					}
				}
				break;
			}
			case OLC_FACTION: {
				extern char *list_one_faction(faction_data *fct, bool detail);
				faction_data *fct, *next_fct;
				HASH_ITER(hh, faction_table, fct, next_fct) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (FCT_VNUM(fct) >= from_vnum && FCT_VNUM(fct) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_faction(fct, show_details));
					}
				}
				break;
			}
			case OLC_GENERIC: {
				extern char *list_one_generic(generic_data *gen, bool detail);
				generic_data *gen, *next_gen;
				HASH_ITER(hh, generic_table, gen, next_gen) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GEN_VNUM(gen) >= from_vnum && GEN_VNUM(gen) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_generic(gen, show_details));
					}
				}
				break;
			}
			case OLC_GLOBAL: {
				extern char *list_one_global(struct global_data *glb, bool detail);
				struct global_data *glb, *next_glb;
				HASH_ITER(hh, globals_table, glb, next_glb) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_GLOBAL_VNUM(glb) >= from_vnum && GET_GLOBAL_VNUM(glb) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_global(glb, show_details));
					}
				}
				break;
			}
			case OLC_MOBILE: {
				extern char *list_one_mobile(char_data *mob, bool detail);
				char_data *mob, *next_mob;
				HASH_ITER(hh, mobile_table, mob, next_mob) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_MOB_VNUM(mob) >= from_vnum && GET_MOB_VNUM(mob) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_mobile(mob, show_details));
					}
				}
				break;
			}
			case OLC_MORPH: {
				extern char *list_one_morph(morph_data *morph, bool detail);
				morph_data *morph, *next_morph;
				HASH_ITER(hh, morph_table, morph, next_morph) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (MORPH_VNUM(morph) >= from_vnum && MORPH_VNUM(morph) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_morph(morph, show_details));
					}
				}
				break;
			}
			case OLC_OBJECT: {
				extern char *list_one_object(obj_data *obj, bool detail);
				obj_data *obj, *next_obj;
				HASH_ITER(hh, object_table, obj, next_obj) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_OBJ_VNUM(obj) >= from_vnum && GET_OBJ_VNUM(obj) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_object(obj, show_details));
					}
				}
				break;
			}
			case OLC_PROGRESS: {
				extern char *list_one_progress(progress_data *prg, bool detail);
				progress_data *prg, *next_prg;
				HASH_ITER(hh, progress_table, prg, next_prg) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (PRG_VNUM(prg) >= from_vnum && PRG_VNUM(prg) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_progress(prg, show_details));
					}
				}
				break;
			}
			case OLC_QUEST: {
				extern char *list_one_quest(quest_data *quest, bool detail);
				quest_data *quest, *next_quest;
				HASH_ITER(hh, quest_table, quest, next_quest) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (QUEST_VNUM(quest) >= from_vnum && QUEST_VNUM(quest) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_quest(quest, show_details));
					}
				}
				break;
			}
			case OLC_ROOM_TEMPLATE: {
				extern char *list_one_room_template(room_template *rmt, bool detail);
				room_template *rmt, *next_rmt;
				HASH_ITER(hh, room_template_table, rmt, next_rmt) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_RMT_VNUM(rmt) >= from_vnum && GET_RMT_VNUM(rmt) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_room_template(rmt, show_details));
					}
				}
				break;
			}
			case OLC_SECTOR: {
				extern char *list_one_sector(sector_data *sect, bool detail);
				sector_data *sect, *next_sect;
				HASH_ITER(hh, sector_table, sect, next_sect) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_SECT_VNUM(sect) >= from_vnum && GET_SECT_VNUM(sect) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_sector(sect, show_details));
					}
				}
				break;
			}
			case OLC_SHOP: {
				extern char *list_one_shop(shop_data *shop, bool detail);
				shop_data *shop, *next_shop;
				HASH_ITER(hh, shop_table, shop, next_shop) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (SHOP_VNUM(shop) >= from_vnum && SHOP_VNUM(shop) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_shop(shop, show_details));
					}
				}
				break;
			}
			case OLC_SKILL: {
				extern char *list_one_skill(skill_data *skill, bool detail);
				skill_data *skill, *next_skill;
				HASH_ITER(hh, skill_table, skill, next_skill) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (SKILL_VNUM(skill) >= from_vnum && SKILL_VNUM(skill) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_skill(skill, show_details));
					}
				}
				break;
			}
			case OLC_SOCIAL: {
				extern char *list_one_social(social_data *soc, bool detail);
				social_data *soc, *next_soc;
				HASH_ITER(hh, social_table, soc, next_soc) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (SOC_VNUM(soc) >= from_vnum && SOC_VNUM(soc) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_social(soc, show_details));
					}
				}
				break;
			}
			case OLC_TRIGGER: {
				extern char *list_one_trigger(trig_data *trig, bool detail);
				trig_data *trig, *next_trig;
				HASH_ITER(hh, trigger_table, trig, next_trig) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_TRIG_VNUM(trig) >= from_vnum && GET_TRIG_VNUM(trig) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_trigger(trig, show_details));
					}
				}
				break;
			}
			case OLC_VEHICLE: {
				extern char *list_one_vehicle(vehicle_data *veh, bool detail);
				vehicle_data *veh, *next_veh;
				HASH_ITER(hh, vehicle_table, veh, next_veh) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (VEH_VNUM(veh) >= from_vnum && VEH_VNUM(veh) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "%s\r\n", list_one_vehicle(veh, show_details));
					}
				}
				break;
			}
		}
		
		sprintbit(type, olc_type_bits, buf2, FALSE);
		if (*buf) {
			snprintf(buf1, sizeof(buf1), "Found %d %s%s:\r\n%s", count, buf2, (count != 1 ? "s" : ""), buf);
			page_string(ch->desc, buf1, TRUE);
		}
		else {
			msg_to_char(ch, "Found no %ss in that range.\r\n", buf2);
		}
	}
}


OLC_MODULE(olc_removeindev) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	any_vnum from = NOTHING, to = NOTHING;
	bool use_adv = FALSE, any = FALSE;
	struct global_data *glb, *next_glb;
	archetype_data *arch, *next_arch;
	morph_data *morph, *next_morph;
	craft_data *craft, *next_craft;
	quest_data *quest, *next_quest;
	skill_data *skill, *next_skill;
	event_data *event, *next_event;
	progress_data *prg, *next_prg;
	faction_data *fct, *next_fct;
	social_data *soc, *next_soc;
	augment_data *aug, *next_aug;
	shop_data *shop, *next_shop;
	class_data *cls, *next_cls;
	adv_data *adv = NULL;
	descriptor_data *desc;
	any_vnum check_vnum;
	int iter;
	
	// allow - or : as vnum separators
	for (iter = 0; iter < strlen(argument); ++iter) {
		if (argument[iter] == '-' || argument[iter] == ':') {
			argument[iter] = ' ';
		}
	}
	
	// 2nd arg optional
	two_arguments(argument, arg1, arg2);
	use_adv = (*arg2 ? FALSE : TRUE);
	
	if (GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !IS_GRANTED(ch, GRANT_OLC_CONTROLS) && !OLC_FLAGGED(ch, OLC_FLAG_ALL_VNUMS)) {
		msg_to_char(ch, "You must be level %d to do that.\r\n", LVL_UNRESTRICTED_BUILDER);
	}
	else if (!*arg1 || !isdigit(*arg1) || (*arg2 && !isdigit(*arg2))) {
		msg_to_char(ch, "Usage: .removeindev <adventure vnum | vnum range>\r\n");
	}
	else if ((from = atoi(arg1)) < 0) {
		msg_to_char(ch, "Invalid vnum '%s'.\r\n", arg1);
	}
	else if (*arg2 && (to = atoi(arg2)) < 0) {
		msg_to_char(ch, "Invalid vnum '%s'.\r\n", arg2);
	}
	else if (use_adv && !(adv = adventure_proto(from))) {
		msg_to_char(ch, "Unknown adventure vnum '%s'.\r\n", arg1);
	}
	else if (use_adv && !player_can_olc_edit(ch, OLC_ADVENTURE, from)) {
		msg_to_char(ch, "You don't have permission to edit that adventure.\r\n");
	}
	else if (!use_adv && !player_can_olc_edit(ch, OLC_CRAFT, from) && !player_can_olc_edit(ch, OLC_CRAFT, to)) {
		msg_to_char(ch, "You don't have permission to edit that vnum range.\r\n");
	}
	else {
		any = FALSE;
		
		if (use_adv && adv && ADVENTURE_FLAGGED(adv, ADV_IN_DEVELOPMENT) && player_can_olc_edit(ch, OLC_ADVENTURE, GET_ADV_VNUM(adv))) {
			REMOVE_BIT(GET_ADV_FLAGS(adv), ADV_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_ADV, GET_ADV_VNUM(adv));
			msg_to_char(ch, "Removed IN-DEV flag from adventure [%d] %s.\r\n", GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
			any = TRUE;
		}
		
		// prepare to iterate
		if (use_adv && adv) {
			from = GET_ADV_START_VNUM(adv);
			to = GET_ADV_END_VNUM(adv);
		}
		
		HASH_ITER(hh, class_table, cls, next_cls) {
			if (CLASS_VNUM(cls) < from || CLASS_VNUM(cls) > to) {
				continue;
			}
			if (!CLASS_FLAGGED(cls, CLASSF_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_CLASS, CLASS_VNUM(cls))) {
				continue;
			}
			
			REMOVE_BIT(CLASS_FLAGS(cls), CLASSF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_CLASS, CLASS_VNUM(cls));
			msg_to_char(ch, "Removed IN-DEV flag from class [%d] %s.\r\n", CLASS_VNUM(cls), CLASS_NAME(cls));
			any = TRUE;
		}
		
		HASH_ITER(hh, craft_table, craft, next_craft) {
			if (GET_CRAFT_VNUM(craft) < from || GET_CRAFT_VNUM(craft) > to) {
				continue;
			}
			if (!CRAFT_FLAGGED(craft, CRAFT_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_CRAFT, GET_CRAFT_VNUM(craft))) {
				continue;
			}
			
			REMOVE_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
			msg_to_char(ch, "Removed IN-DEV flag from craft [%d] %s.\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
			any = TRUE;
		}
		
		HASH_ITER(hh, event_table, event, next_event) {
			if (EVT_VNUM(event) < from || EVT_VNUM(event) > to) {
				continue;
			}
			if (!IS_SET(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_EVENT, EVT_VNUM(event))) {
				continue;
			}
			
			REMOVE_BIT(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_EVT, EVT_VNUM(event));
			msg_to_char(ch, "Removed IN-DEV flag from event [%d] %s.\r\n", EVT_VNUM(event), EVT_NAME(event));
			any = TRUE;
		}
		
		HASH_ITER(hh, faction_table, fct, next_fct) {
			if (FCT_VNUM(fct) < from || FCT_VNUM(fct) > to) {
				continue;
			}
			if (!IS_SET(FCT_FLAGS(fct), FCT_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_FACTION, FCT_VNUM(fct))) {
				continue;
			}
			
			REMOVE_BIT(FCT_FLAGS(fct), FCT_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_FCT, FCT_VNUM(fct));
			msg_to_char(ch, "Removed IN-DEV flag from faction [%d] %s.\r\n", FCT_VNUM(fct), FCT_NAME(fct));
			any = TRUE;
		}
		
		HASH_ITER(hh, globals_table, glb, next_glb) {
			if (GET_GLOBAL_VNUM(glb) < from || GET_GLOBAL_VNUM(glb) > to) {
				continue;
			}
			if (!IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_GLOBAL, GET_GLOBAL_VNUM(glb))) {
				continue;
			}
			
			REMOVE_BIT(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_GLB, GET_GLOBAL_VNUM(glb));
			msg_to_char(ch, "Removed IN-DEV flag from global [%d] %s.\r\n", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb));
			any = TRUE;
		}
		
		HASH_ITER(hh, archetype_table, arch, next_arch) {
			if (GET_ARCH_VNUM(arch) < from || GET_ARCH_VNUM(arch) > to) {
				continue;
			}
			if (!ARCHETYPE_FLAGGED(arch, ARCH_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_ARCHETYPE, GET_ARCH_VNUM(arch))) {
				continue;
			}
			
			REMOVE_BIT(GET_ARCH_FLAGS(arch), ARCH_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_ARCH, GET_ARCH_VNUM(arch));
			msg_to_char(ch, "Removed IN-DEV flag from archetype [%d] %s.\r\n", GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch));
			any = TRUE;
		}
		
		HASH_ITER(hh, augment_table, aug, next_aug) {
			if (GET_AUG_VNUM(aug) < from || GET_AUG_VNUM(aug) > to) {
				continue;
			}
			if (!AUGMENT_FLAGGED(aug, AUG_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_AUGMENT, GET_AUG_VNUM(aug))) {
				continue;
			}
			
			REMOVE_BIT(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_AUG, GET_AUG_VNUM(aug));
			msg_to_char(ch, "Removed IN-DEV flag from augment [%d] %s.\r\n", GET_AUG_VNUM(aug), GET_AUG_NAME(aug));
			any = TRUE;
		}
		
		HASH_ITER(hh, morph_table, morph, next_morph) {
			if (MORPH_VNUM(morph) < from || MORPH_VNUM(morph) > to) {
				continue;
			}
			if (!MORPH_FLAGGED(morph, MORPHF_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_MORPH, MORPH_VNUM(morph))) {
				continue;
			}
			
			REMOVE_BIT(MORPH_FLAGS(morph), MORPHF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_MORPH, MORPH_VNUM(morph));
			msg_to_char(ch, "Removed IN-DEV flag from morph [%d] %s.\r\n", MORPH_VNUM(morph), MORPH_SHORT_DESC(morph));
			any = TRUE;
		}
		
		HASH_ITER(hh, progress_table, prg, next_prg) {
			if (PRG_VNUM(prg) < from || PRG_VNUM(prg) > to) {
				continue;
			}
			if (!PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_PROGRESS, PRG_VNUM(prg))) {
				continue;
			}
			
			REMOVE_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			msg_to_char(ch, "Removed IN-DEV flag from progress goal [%d] %s.\r\n", PRG_VNUM(prg), PRG_NAME(prg));
			any = TRUE;
		}
		
		HASH_ITER(hh, quest_table, quest, next_quest) {
			if (QUEST_VNUM(quest) < from || QUEST_VNUM(quest) > to) {
				continue;
			}
			if (!QUEST_FLAGGED(quest, QST_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_QUEST, QUEST_VNUM(quest))) {
				continue;
			}
			
			REMOVE_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
			msg_to_char(ch, "Removed IN-DEV flag from quest [%d] %s.\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
			any = TRUE;
		}
		
		HASH_ITER(hh, shop_table, shop, next_shop) {
			if (SHOP_VNUM(shop) < from || SHOP_VNUM(shop) > to) {
				continue;
			}
			if (!IS_SET(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_SHOP, SHOP_VNUM(shop))) {
				continue;
			}
			
			REMOVE_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SHOP, SHOP_VNUM(shop));
			msg_to_char(ch, "Removed IN-DEV flag from shop [%d] %s.\r\n", SHOP_VNUM(shop), SHOP_NAME(shop));
			any = TRUE;
		}
		
		HASH_ITER(hh, skill_table, skill, next_skill) {
			if (SKILL_VNUM(skill) < from || SKILL_VNUM(skill) > to) {
				continue;
			}
			if (!SKILL_FLAGGED(skill, SKILLF_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_SKILL, SKILL_VNUM(skill))) {
				continue;
			}
			
			REMOVE_BIT(SKILL_FLAGS(skill), SKILLF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SKILL, SKILL_VNUM(skill));
			msg_to_char(ch, "Removed IN-DEV flag from skill [%d] %s.\r\n", SKILL_VNUM(skill), SKILL_NAME(skill));
			any = TRUE;
		}
		
		HASH_ITER(hh, social_table, soc, next_soc) {
			if (SOC_VNUM(soc) < from || SOC_VNUM(soc) > to) {
				continue;
			}
			if (!SOCIAL_FLAGGED(soc, SOC_IN_DEVELOPMENT)) {
				continue;
			}
			if (!player_can_olc_edit(ch, OLC_SOCIAL, SOC_VNUM(soc))) {
				continue;
			}
			
			REMOVE_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
			msg_to_char(ch, "Removed IN-DEV flag from social [%d] %s.\r\n", SOC_VNUM(soc), SOC_NAME(soc));
			any = TRUE;
		}
		
		// check for people editing in that vnum range
		if (any) {
			LL_FOREACH(descriptor_list, desc) {
				if (!desc->character) {
					continue;
				}
				
				check_vnum = NOTHING;
				if (GET_OLC_CLASS(desc) && CLASS_FLAGGED(GET_OLC_CLASS(desc), CLASSF_IN_DEVELOPMENT)) {
					check_vnum = CLASS_VNUM(GET_OLC_CLASS(desc));
				}
				if (GET_OLC_CRAFT(desc) && CRAFT_FLAGGED(GET_OLC_CRAFT(desc), CRAFT_IN_DEVELOPMENT)) {
					check_vnum = GET_CRAFT_VNUM(GET_OLC_CRAFT(desc));
				}
				if (GET_OLC_EVENT(desc) && EVT_FLAGGED(GET_OLC_EVENT(desc), EVTF_IN_DEVELOPMENT)) {
					check_vnum = EVT_VNUM(GET_OLC_EVENT(desc));
				}
				if (GET_OLC_FACTION(desc) && FACTION_FLAGGED(GET_OLC_FACTION(desc), FCT_IN_DEVELOPMENT)) {
					check_vnum = FCT_VNUM(GET_OLC_FACTION(desc));
				}
				if (GET_OLC_GLOBAL(desc) && IS_SET(GET_GLOBAL_FLAGS(GET_OLC_GLOBAL(desc)), GLB_FLAG_IN_DEVELOPMENT)) {
					check_vnum = GET_GLOBAL_VNUM(GET_OLC_GLOBAL(desc));
				}
				if (GET_OLC_ARCHETYPE(desc) && ARCHETYPE_FLAGGED(GET_OLC_ARCHETYPE(desc), ARCH_IN_DEVELOPMENT)) {
					check_vnum = GET_ARCH_VNUM(GET_OLC_ARCHETYPE(desc));
				}
				if (GET_OLC_AUGMENT(desc) && AUGMENT_FLAGGED(GET_OLC_AUGMENT(desc), AUG_IN_DEVELOPMENT)) {
					check_vnum = GET_AUG_VNUM(GET_OLC_AUGMENT(desc));
				}
				if (GET_OLC_MORPH(desc) && MORPH_FLAGGED(GET_OLC_MORPH(desc), MORPHF_IN_DEVELOPMENT)) {
					check_vnum = MORPH_VNUM(GET_OLC_MORPH(desc));
				}
				if (GET_OLC_QUEST(desc) && QUEST_FLAGGED(GET_OLC_QUEST(desc), QST_IN_DEVELOPMENT)) {
					check_vnum = QUEST_VNUM(GET_OLC_QUEST(desc));
				}
				if (GET_OLC_SKILL(desc) && SKILL_FLAGGED(GET_OLC_SKILL(desc), SKILLF_IN_DEVELOPMENT)) {
					check_vnum = SKILL_VNUM(GET_OLC_SKILL(desc));
				}
				if (GET_OLC_SOCIAL(desc) && SOCIAL_FLAGGED(GET_OLC_SOCIAL(desc), SOC_IN_DEVELOPMENT)) {
					check_vnum = SOC_VNUM(GET_OLC_SOCIAL(desc));
				}
				
				// is it in range?
				if (check_vnum >= from && check_vnum <= to) {
					msg_to_char(ch, "Warning: %s is editing something in that range with an IN-DEV flag.\r\n", GET_NAME(desc->character));
				}
			}
		}
		
		if (!any) {
			msg_to_char(ch, "No in-development flags to remove.\r\n");
		}
		else if (use_adv && adv) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s removed in-development flags for adventure [%d] %s", GET_NAME(ch), GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
		}
		else {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s removed in-development flags for vnums %d-%d", GET_NAME(ch), from, to);
		}
	}
}


OLC_MODULE(olc_save) {
	char typename[42];
	
	sprintbit(GET_OLC_TYPE(ch->desc), olc_type_bits, typename, FALSE);
	
	if (GET_OLC_TYPE(ch->desc) == 0 || GET_OLC_VNUM(ch->desc) == NOTHING) {
		msg_to_char(ch, "You aren't editing anything.\r\n");
	}
	else if (!player_can_olc_edit(ch, GET_OLC_TYPE(ch->desc), GET_OLC_VNUM(ch->desc))) {
		msg_to_char(ch, "You don't have permission to save that anymore.\r\n");
	}
	else if (ch->desc->str) {
		msg_to_char(ch, "Close your text editor (&y,/h&0) before saving an olc editor.\r\n");
	}
	else {
		msg_to_char(ch, "Saving %s %d...\r\n", typename, GET_OLC_VNUM(ch->desc));
		
		// OLC_x:
		switch (GET_OLC_TYPE(ch->desc)) {
			case OLC_ABILITY: {
				void save_olc_ability(descriptor_data *desc);
				save_olc_ability(ch->desc);
				audit_ability(GET_OLC_ABILITY(ch->desc), ch);
				free_ability(GET_OLC_ABILITY(ch->desc));
				GET_OLC_ABILITY(ch->desc) = NULL;
				break;
			}
			case OLC_ADVENTURE: {
				void save_olc_adventure(descriptor_data *desc);
				save_olc_adventure(ch->desc);
				audit_adventure(GET_OLC_ADVENTURE(ch->desc), ch, FALSE);
				free_adventure(GET_OLC_ADVENTURE(ch->desc));
				GET_OLC_ADVENTURE(ch->desc) = NULL;
				break;
			}
			case OLC_ARCHETYPE: {
				void save_olc_archetype(descriptor_data *desc);
				save_olc_archetype(ch->desc);
				audit_archetype(GET_OLC_ARCHETYPE(ch->desc), ch);
				free_archetype(GET_OLC_ARCHETYPE(ch->desc));
				GET_OLC_ARCHETYPE(ch->desc) = NULL;
				break;
			}
			case OLC_AUGMENT: {
				void save_olc_augment(descriptor_data *desc);
				save_olc_augment(ch->desc);
				audit_augment(GET_OLC_AUGMENT(ch->desc), ch);
				free_augment(GET_OLC_AUGMENT(ch->desc));
				GET_OLC_AUGMENT(ch->desc) = NULL;
				break;
			}
			case OLC_BOOK: {
				void save_olc_book(descriptor_data *desc);
				save_olc_book(ch->desc);
				// audit_book(GET_OLC_BOOK(ch->desc), ch);
				free_book(GET_OLC_BOOK(ch->desc));
				GET_OLC_BOOK(ch->desc) = NULL;
				break;
			}
			case OLC_BUILDING: {
				void save_olc_building(descriptor_data *desc);
				save_olc_building(ch->desc);
				audit_building(GET_OLC_BUILDING(ch->desc), ch);
				free_building(GET_OLC_BUILDING(ch->desc));
				GET_OLC_BUILDING(ch->desc) = NULL;
				break;
			}
			case OLC_CLASS: {
				void save_olc_class(descriptor_data *desc);
				save_olc_class(ch->desc);
				audit_class(GET_OLC_CLASS(ch->desc), ch);
				free_class(GET_OLC_CLASS(ch->desc));
				GET_OLC_CLASS(ch->desc) = NULL;
				break;
			}
			case OLC_CRAFT: {
				void save_olc_craft(descriptor_data *desc);
				save_olc_craft(ch->desc);
				audit_craft(GET_OLC_CRAFT(ch->desc), ch);
				free_craft(GET_OLC_CRAFT(ch->desc));
				GET_OLC_CRAFT(ch->desc) = NULL;
				break;
			}
			case OLC_CROP: {
				void save_olc_crop(descriptor_data *desc);
				save_olc_crop(ch->desc);
				audit_crop(GET_OLC_CROP(ch->desc), ch);
				free_crop(GET_OLC_CROP(ch->desc));
				GET_OLC_CROP(ch->desc) = NULL;
				break;
			}
			case OLC_EVENT: {
				void save_olc_event(descriptor_data *desc);
				save_olc_event(ch->desc);
				audit_event(GET_OLC_EVENT(ch->desc), ch);
				free_event(GET_OLC_EVENT(ch->desc));
				GET_OLC_EVENT(ch->desc) = NULL;
				break;
			}
			case OLC_FACTION: {
				void save_olc_faction(descriptor_data *desc);
				save_olc_faction(ch->desc);
				audit_faction(GET_OLC_FACTION(ch->desc), ch);
				free_faction(GET_OLC_FACTION(ch->desc));
				GET_OLC_FACTION(ch->desc) = NULL;
				break;
			}
			case OLC_GENERIC: {
				void save_olc_generic(descriptor_data *desc);
				save_olc_generic(ch->desc);
				audit_generic(GET_OLC_GENERIC(ch->desc), ch);
				free_generic(GET_OLC_GENERIC(ch->desc));
				GET_OLC_GENERIC(ch->desc) = NULL;
				break;
			}
			case OLC_GLOBAL: {
				void save_olc_global(descriptor_data *desc);
				save_olc_global(ch->desc);
				audit_global(GET_OLC_GLOBAL(ch->desc), ch);
				free_global(GET_OLC_GLOBAL(ch->desc));
				GET_OLC_GLOBAL(ch->desc) = NULL;
				break;
			}
			case OLC_MOBILE: {
				void save_olc_mobile(descriptor_data *desc);
				save_olc_mobile(ch->desc);
				audit_mobile(GET_OLC_MOBILE(ch->desc), ch);
				free_char(GET_OLC_MOBILE(ch->desc));
				GET_OLC_MOBILE(ch->desc) = NULL;
				break;
			}
			case OLC_MORPH: {
				void save_olc_morph(descriptor_data *desc);
				save_olc_morph(ch->desc);
				audit_morph(GET_OLC_MORPH(ch->desc), ch);
				free_morph(GET_OLC_MORPH(ch->desc));
				GET_OLC_MORPH(ch->desc) = NULL;
				break;
			}
			case OLC_OBJECT: {
				void save_olc_object(descriptor_data *desc);
				save_olc_object(ch->desc);
				audit_object(GET_OLC_OBJECT(ch->desc), ch);
				free_obj(GET_OLC_OBJECT(ch->desc));
				GET_OLC_OBJECT(ch->desc) = NULL;
				break;
			}
			case OLC_PROGRESS: {
				void save_olc_progress(descriptor_data *desc);
				save_olc_progress(ch->desc);
				audit_progress(GET_OLC_PROGRESS(ch->desc), ch);
				free_progress(GET_OLC_PROGRESS(ch->desc));
				GET_OLC_PROGRESS(ch->desc) = NULL;
				break;
			}
			case OLC_QUEST: {
				void save_olc_quest(descriptor_data *desc);
				save_olc_quest(ch->desc);
				audit_quest(GET_OLC_QUEST(ch->desc), ch);
				free_quest(GET_OLC_QUEST(ch->desc));
				GET_OLC_QUEST(ch->desc) = NULL;
				break;
			}
			case OLC_ROOM_TEMPLATE: {
				void save_olc_room_template(descriptor_data *desc);
				save_olc_room_template(ch->desc);
				audit_room_template(GET_OLC_ROOM_TEMPLATE(ch->desc), ch);
				free_room_template(GET_OLC_ROOM_TEMPLATE(ch->desc));
				GET_OLC_ROOM_TEMPLATE(ch->desc) = NULL;
				break;
			}
			case OLC_SECTOR: {
				void save_olc_sector(descriptor_data *desc);
				save_olc_sector(ch->desc);
				audit_sector(GET_OLC_SECTOR(ch->desc), ch);
				free_sector(GET_OLC_SECTOR(ch->desc));
				GET_OLC_SECTOR(ch->desc) = NULL;
				break;
			}
			case OLC_SHOP: {
				void save_olc_shop(descriptor_data *desc);
				save_olc_shop(ch->desc);
				audit_shop(GET_OLC_SHOP(ch->desc), ch);
				free_shop(GET_OLC_SHOP(ch->desc));
				GET_OLC_SHOP(ch->desc) = NULL;
				break;
			}
			case OLC_SKILL: {
				void save_olc_skill(descriptor_data *desc);
				save_olc_skill(ch->desc);
				audit_skill(GET_OLC_SKILL(ch->desc), ch);
				free_skill(GET_OLC_SKILL(ch->desc));
				GET_OLC_SKILL(ch->desc) = NULL;
				break;
			}
			case OLC_SOCIAL: {
				void save_olc_social(descriptor_data *desc);
				save_olc_social(ch->desc);
				audit_social(GET_OLC_SOCIAL(ch->desc), ch);
				free_social(GET_OLC_SOCIAL(ch->desc));
				GET_OLC_SOCIAL(ch->desc) = NULL;
				break;
			}
			case OLC_TRIGGER: {
				void save_olc_trigger(descriptor_data *desc, char *script_text);
				save_olc_trigger(ch->desc, GET_OLC_STORAGE(ch->desc));
				audit_trigger(GET_OLC_TRIGGER(ch->desc), ch);
				free_trigger(GET_OLC_TRIGGER(ch->desc));
				GET_OLC_TRIGGER(ch->desc) = NULL;
				if (GET_OLC_STORAGE(ch->desc)) {
					free(GET_OLC_STORAGE(ch->desc));
					GET_OLC_STORAGE(ch->desc) = NULL;
				}
				break;
			}
			case OLC_VEHICLE: {
				void save_olc_vehicle(descriptor_data *desc);
				save_olc_vehicle(ch->desc);
				audit_vehicle(GET_OLC_VEHICLE(ch->desc), ch);
				free_vehicle(GET_OLC_VEHICLE(ch->desc));
				GET_OLC_VEHICLE(ch->desc) = NULL;
				break;
			}
			default: {
				msg_to_char(ch, "Error saving: unknown save type %s.\r\n", typename);
				break;
			}
		}
		
		// log and cleanup
		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has edited %s %d", GET_NAME(ch), typename, GET_OLC_VNUM(ch->desc));
		GET_OLC_TYPE(ch->desc) = 0;
		GET_OLC_VNUM(ch->desc) = NOTHING;
	}
}


OLC_MODULE(olc_search) {
	void olc_search_ability(char_data *ch, any_vnum vnum);
	void olc_search_archetype(char_data *ch, any_vnum vnum);
	void olc_search_augment(char_data *ch, any_vnum vnum);
	void olc_search_building(char_data *ch, bld_vnum vnum);
	void olc_search_class(char_data *ch, any_vnum vnum);
	void olc_search_craft(char_data *ch, craft_vnum vnum);
	void olc_search_crop(char_data *ch, crop_vnum vnum);
	void olc_search_global(char_data *ch, any_vnum vnum);
	void olc_search_mob(char_data *ch, mob_vnum vnum);
	void olc_search_morph(char_data *ch, any_vnum vnum);
	void olc_search_obj(char_data *ch, obj_vnum vnum);
	void olc_search_room_template(char_data *ch, rmt_vnum vnum);
	void olc_search_sector(char_data *ch, sector_vnum vnum);
	void olc_search_skill(char_data *ch, any_vnum vnum);
	void olc_search_social(char_data *ch, any_vnum vnum);
	void olc_search_trigger(char_data *ch, trig_vnum vnum);
	void olc_search_vehicle(char_data *ch, any_vnum vnum);

	any_vnum vnum = NOTHING;
	
	skip_spaces(&argument);
	
	if (!*argument || !isdigit(*argument)) {
		msg_to_char(ch, "Usage: search <vnum>\r\n");
	}
	else if ((vnum = atoi(argument)) < 0) {
		msg_to_char(ch, "Invalid vnum '%s'.\r\n", argument);
	}
	else {
		// OLC_x:
		switch (type) {
			case OLC_ABILITY: {
				olc_search_ability(ch, vnum);
				break;
			}
			case OLC_ARCHETYPE: {
				olc_search_archetype(ch, vnum);
				break;
			}
			case OLC_AUGMENT: {
				olc_search_augment(ch, vnum);
				break;
			}
			case OLC_BUILDING: {
				olc_search_building(ch, vnum);
				break;
			}
			case OLC_CLASS: {
				olc_search_class(ch, vnum);
				break;
			}
			case OLC_CRAFT: {
				olc_search_craft(ch, vnum);
				break;
			}
			case OLC_CROP: {
				olc_search_crop(ch, vnum);
				break;
			}
			case OLC_EVENT: {
				void olc_search_event(char_data *ch, any_vnum vnum);
				olc_search_event(ch, vnum);
				break;
			}
			case OLC_FACTION: {
				void olc_search_faction(char_data *ch, any_vnum vnum);
				olc_search_faction(ch, vnum);
				break;
			}
			case OLC_GENERIC: {
				void olc_search_generic(char_data *ch, any_vnum vnum);
				olc_search_generic(ch, vnum);
				break;
			}
			case OLC_GLOBAL: {
				olc_search_global(ch, vnum);
				break;
			}
			case OLC_MOBILE: {
				olc_search_mob(ch, vnum);
				break;
			}
			case OLC_MORPH: {
				olc_search_morph(ch, vnum);
				break;
			}
			case OLC_OBJECT: {
				olc_search_obj(ch, vnum);
				break;
			}
			case OLC_PROGRESS: {
				void olc_search_progress(char_data *ch, any_vnum vnum);
				olc_search_progress(ch, vnum);
				break;
			}
			case OLC_QUEST: {
				void olc_search_quest(char_data *ch, any_vnum vnum);
				olc_search_quest(ch, vnum);
				break;
			}
			case OLC_ROOM_TEMPLATE: {
				olc_search_room_template(ch, vnum);
				break;
			}
			case OLC_SECTOR: {
				olc_search_sector(ch, vnum);
				break;
			}
			case OLC_SHOP: {
				void olc_search_shop(char_data *ch, any_vnum vnum);
				olc_search_shop(ch, vnum);
				break;
			}
			case OLC_SKILL: {
				olc_search_skill(ch, vnum);
				break;
			}
			case OLC_SOCIAL: {
				olc_search_social(ch, vnum);
				break;
			}
			case OLC_TRIGGER: {
				olc_search_trigger(ch, vnum);
				break;
			}
			case OLC_VEHICLE: {
				olc_search_vehicle(ch, vnum);
				break;
			}
			default: {
				msg_to_char(ch, "It doesn't seem to be implemented for that type.\r\n");
				break;
			}
		}
	}
}


OLC_MODULE(olc_set_flags) {
	char_data *vict = NULL;
	bool file = FALSE;
	bitvector_t old;
	
	argument = one_argument(argument, arg);
	skip_spaces(&argument);
	
	if (GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !IS_GRANTED(ch, GRANT_OLC_CONTROLS)) {
		msg_to_char(ch, "You must be level %d to do that.\r\n", LVL_UNRESTRICTED_BUILDER);
	}
	else if (!*arg) {
		msg_to_char(ch, "Set whose olc flags?\r\n");
	}
	else if (!(vict = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else if (GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "You can't set that on a person of such level.\r\n");
	}
	else {
		old = GET_OLC_FLAGS(vict);
		GET_OLC_FLAGS(vict) = olc_process_flag(ch, argument, "olc", "setflags", olc_flag_bits, GET_OLC_FLAGS(vict));
		
		// they didn't necessarily make any changes
		if (GET_OLC_FLAGS(vict) != old) {
			sprintbit(GET_OLC_FLAGS(vict), olc_flag_bits, buf, TRUE);
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has set olc flags for %s to: %s", GET_NAME(ch), GET_NAME(vict), buf);
			msg_to_char(ch, "%s saved.\r\n", GET_NAME(vict));
		
			if (file) {
				store_loaded_char(vict);
				file = FALSE;
				vict = NULL;
			}
			else {
				SAVE_CHAR(vict);
			}
		}
	}
	
	if (vict && file) {
		free_char(vict);
	}
}

OLC_MODULE(olc_set_max_vnum) {
	char_data *vict = NULL;
	bool file = FALSE;
	
	argument = one_argument(argument, arg);
	skip_spaces(&argument);
	
	if (GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !IS_GRANTED(ch, GRANT_OLC_CONTROLS)) {
		msg_to_char(ch, "You must be level %d to do that.\r\n", LVL_UNRESTRICTED_BUILDER);
	}
	else if (!*arg || !*argument) {
		msg_to_char(ch, "Usage: .setmaxvnum <player> <vnum>\r\n");
	}
	else if (!(vict = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else if (GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "You can't set that on a person of such level.\r\n");
	}
	else {
		GET_OLC_MAX_VNUM(vict) = olc_process_number(ch, argument, "max vnum", "setmaxvnum", 0, MAX_INT, GET_OLC_MAX_VNUM(vict));
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has set max olc vnum for %s to %d", GET_NAME(ch), GET_NAME(vict), GET_OLC_MAX_VNUM(vict));
		msg_to_char(ch, "%s saved.\r\n", GET_NAME(vict));
		
		if (file) {
			store_loaded_char(vict);
			file = FALSE;
			vict = NULL;
		}
		else {
			SAVE_CHAR(vict);
		}
	}
	
	if (vict && file) {
		free_char(vict);
	}
}

OLC_MODULE(olc_set_min_vnum) {
	char_data *vict = NULL;
	bool file = FALSE;
	
	argument = one_argument(argument, arg);
	skip_spaces(&argument);
	
	if (GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !IS_GRANTED(ch, GRANT_OLC_CONTROLS)) {
		msg_to_char(ch, "You must be level %d to do that.\r\n", LVL_UNRESTRICTED_BUILDER);
	}
	else if (!*arg || !*argument) {
		msg_to_char(ch, "Usage: .setminvnum <player> <vnum>\r\n");
	}
	else if (!(vict = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else if (GET_ACCESS_LEVEL(vict) >= GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "You can't set that on a person of such level.\r\n");
	}
	else {
		GET_OLC_MIN_VNUM(vict) = olc_process_number(ch, argument, "min vnum", "setminvnum", 0, MAX_INT, GET_OLC_MIN_VNUM(vict));
		syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "GC: %s has set min olc vnum for %s to %d", GET_NAME(ch), GET_NAME(vict), GET_OLC_MIN_VNUM(vict));
		msg_to_char(ch, "%s saved.\r\n", GET_NAME(vict));
		
		if (file) {
			store_loaded_char(vict);
			vict = NULL;
			file = FALSE;
		}
		else {
			SAVE_CHAR(vict);
		}
	}
	
	if (vict && file) {
		free_char(vict);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMON DISPLAYS /////////////////////////////////////////////////////////

/**
* Displays the evolution data from a given list.
*
* @param struct evolution_data *list Pointer to the start of a list of evos.
* @param char *save_buffer A buffer to store the result to.
*/
void get_evolution_display(struct evolution_data *list, char *save_buffer) {
	extern const char *evo_types[];
	extern const int evo_val_types[NUM_EVOS];
	
	char lbuf[MAX_STRING_LENGTH];
	struct evolution_data *evo;
	int count = 0;
	
	*save_buffer = '\0';
	
	for (evo = list; evo; evo = evo->next) {
		switch (evo_val_types[evo->type]) {
			case EVO_VAL_SECTOR: {
				sprintf(lbuf, " [%s (%d)]", GET_SECT_NAME(sector_proto(evo->value)), evo->value);
				break;
			}
			case EVO_VAL_NUMBER: {
				sprintf(lbuf, " [%d]", evo->value);
				break;
			}
			default: {
				*lbuf = '\0';
				break;
			}
		}
		sprintf(save_buffer + strlen(save_buffer), " %d. %s%s %.2f%% becomes %s (%d)\r\n", ++count, evo_types[evo->type], lbuf, evo->percent, GET_SECT_NAME(sector_proto(evo->becomes)), evo->becomes);
	}
	if (!list) {
		sprintf(save_buffer + strlen(save_buffer), " none\r\n");
	}
}


/**
* Displays the extra descs from a given list.
*
* @param struct extra_descr_data *list Pointer to the start of a list of decriptions.
* @param char *save_buffer A buffer to store the result to.
*/
void get_extra_desc_display(struct extra_descr_data *list, char *save_buffer) {
	struct extra_descr_data *ex;
	int count = 0;
	
	*save_buffer = '\0';
	for (ex = list; ex; ex = ex->next) {
		sprintf(save_buffer + strlen(save_buffer), " &y%d&0. %s\r\n%s", ++count, (ex->keyword ? ex->keyword : "(null)"), (ex->description ? ex->description : "(null)\n"));
	}
	if (count == 0) {
		strcat(save_buffer, " none\r\n");
	}
}


/**
* Gets the display for a set of icons.
*
* @param struct icon_data *list Pointer to the start of a list of icons.
* @param char *save_buffer A buffer to store the result to.
*/
void get_icons_display(struct icon_data *list, char *save_buffer) {
	extern const char *icon_types[];
	
	char lbuf[MAX_INPUT_LENGTH], ibuf[MAX_INPUT_LENGTH], line[MAX_INPUT_LENGTH];
	struct icon_data *icon;
	int size, count = 0;

	*save_buffer = '\0';
	
	for (icon = list; icon; icon = icon->next) {
		// have to copy one of the show_color_codes() because it won't work correctly if it appears twice in the same line
		replace_question_color(icon->icon, icon->color, ibuf);
		strcpy(lbuf, show_color_codes(icon->icon));
		sprintf(line, " %2d. %s: %s%s&0  %s%s&0 %s", ++count, icon_types[icon->type], icon->color, ibuf, icon->color, show_color_codes(icon->color), lbuf);
		
		// format column despite variable width of color codes
		size = 34 + color_code_length(line);
		sprintf(lbuf, "%%-%d.%ds%%s", size, size);
		sprintf(save_buffer + strlen(save_buffer), lbuf, line, !(count % 2) ? "\r\n" : "");
	}
	
	if (count % 2) {
		strcat(save_buffer, "\r\n");
	}
}


/**
* Gets the text for a single interaction restriction, or for a full list.
*
* @param struct interact_restriction *list The restriction or list to show.
* @param bool whole_list If TRUE, displays the whole list.
* @return char* The text to display.
*/
char *get_interaction_restriction_display(struct interact_restriction *list, bool whole_list) {
	static char output[MAX_STRING_LENGTH];
	struct interact_restriction *res;
	char line[256];
	size_t size;
	
	*output = '\0';
	size = 0;
	
	LL_FOREACH(list, res) {
		// INTERACT_RESTRICT_x
		switch(res->type) {
			case INTERACT_RESTRICT_ABILITY: {
				snprintf(line, sizeof(line), "Ability: %s", get_ability_name_by_vnum(res->vnum));
				break;
			}
			case INTERACT_RESTRICT_PTECH: {
				snprintf(line, sizeof(line), "PTech: %s", player_tech_types[res->vnum]);
				break;
			}
			case INTERACT_RESTRICT_TECH: {
				snprintf(line, sizeof(line), "Tech: %s", techs[res->vnum]);
				break;
			}
			default: {
				snprintf(line, sizeof(line), "Unknown %d:%d", res->type, res->vnum);
				break;
			}
		}
		
		// append
		if (strlen(line) + size + 2 < sizeof(output)) {
			size += snprintf(output + size, sizeof(output) - size, "%s%s", size > 0 ? ", " : "", line);
		}
		else {
			size += snprintf(output + size, sizeof(output) - size, "OVERFLOW");
			break;
		}
		
		if (!whole_list) {
			break;
		}
	}
	
	return output;
}


/**
* Displays the interactions data from a given list.
*
* @param struct interaction_item *list Pointer to the start of a list of interactions.
* @param char *save_buffer A buffer to store the result to.
*/
void get_interaction_display(struct interaction_item *list, char *save_buffer) {
	extern const char *interact_types[];
	extern const byte interact_vnum_types[NUM_INTERACTS];

	struct interaction_item *interact;
	char lbuf[MAX_STRING_LENGTH];
	int count = 0;
	
	*save_buffer = '\0';
	
	for (interact = list; interact; interact = interact->next) {
		if (interact_vnum_types[interact->type] == TYPE_MOB) {
			strcpy(lbuf, skip_filler(get_mob_name_by_proto(interact->vnum)));
		}
		else {
			strcpy(lbuf, skip_filler(get_obj_name_by_proto(interact->vnum)));
		}
		sprintf(save_buffer + strlen(save_buffer), "%2d. %s: %dx %s (%d) %.2f%%", ++count, interact_types[interact->type], interact->quantity, lbuf, interact->vnum, interact->percent);
		if (isalpha(interact->exclusion_code)) {
			sprintf(save_buffer + strlen(save_buffer), " (%c)", interact->exclusion_code);
		}
		if (interact->restrictions) {
			sprintf(save_buffer + strlen(save_buffer), " (requires: %s)", get_interaction_restriction_display(interact->restrictions, TRUE));
		}
		strcat(save_buffer, "\r\n");
	}
	
	if (count == 0) {
		strcat(save_buffer, " none\r\n");
	}
}


/**
* Gets the display for a set of requirments (e.g. quest tasks).
*
* @param struct req_data *list Pointer to the start of a list of reqs.
* @param char *save_buffer A buffer to store the result to.
*/
void get_requirement_display(struct req_data *list, char *save_buffer) {
	struct req_data *req;
	int count = 0;
	
	*save_buffer = '\0';
	LL_FOREACH(list, req) {
		sprintf(save_buffer + strlen(save_buffer), "%2d. %s: %s\r\n", ++count, requirement_types[req->type], requirement_string(req, TRUE));
	}
	
	// empty list not shown
}


/**
* Gets the resource list for use in an editor or display.
*
* @param struct resource_data *list The list to show.
* @param char *save_buffer A string to write the output to.
*/
void get_resource_display(struct resource_data *list, char *save_buffer) {
	char line[MAX_STRING_LENGTH];
	struct resource_data *res;
	obj_data *obj;
	int num;
	
	*save_buffer = '\0';
	for (res = list, num = 1; res; res = res->next, ++num) {
		// RES_x: resource type determines display
		switch (res->type) {
			case RES_OBJECT: {
				obj = obj_proto(res->vnum);
				sprintf(line, "%dx %s", res->amount, !obj ? "UNKNOWN" : skip_filler(GET_OBJ_SHORT_DESC(obj)));
				sprintf(save_buffer + strlen(save_buffer), " &y%2d&0. [%5d] %-26.26s", num, res->vnum, line);
				break;
			}
			case RES_COMPONENT: {
				sprintf(line, "%dx (%s)", res->amount, component_string(res->vnum, res->misc));
				sprintf(save_buffer + strlen(save_buffer), " &y%2d&0. %-34.34s", num, line);
				break;
			}
			case RES_LIQUID: {
				sprintf(line, "%d units %s", res->amount, get_generic_name_by_vnum(res->vnum));
				sprintf(save_buffer + strlen(save_buffer), " &y%2d&0. [%5d] %-26.26s", num, res->vnum, line);
				break;
			}
			case RES_COINS: {
				sprintf(save_buffer + strlen(save_buffer), " &y%2d&0. %-34.34s", num, money_amount(real_empire(res->vnum), res->amount));
				break;
			}
			case RES_POOL: {
				sprintf(line, "%d %s", res->amount, pool_types[res->vnum]);
				sprintf(save_buffer + strlen(save_buffer), " &y%2d&0. %-34.34s", num, line);
				break;
			}
			case RES_ACTION: {
				sprintf(line, "%dx [%s]", res->amount, get_generic_name_by_vnum(res->vnum));
				sprintf(save_buffer + strlen(save_buffer), " &y%2d&0. [%5d] %-26.26s", num, res->vnum, line);
				break;
			}
			case RES_CURRENCY: {
				sprintf(line, "%dx %s", res->amount, get_generic_string_by_vnum(res->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(res->amount)));
				sprintf(save_buffer + strlen(save_buffer), " &y%2d&0. [%5d] %-26.26s", num, res->vnum, line);
				break;
			}
			default: {
				sprintf(save_buffer + strlen(save_buffer), " &y%2d&0. %-34.34s", num, "???");
			}
		}
		
		if (!(num % 2)) {
			strcat(save_buffer, "\r\n");
		}
	}
	if (!*save_buffer) {
		strcpy(save_buffer, "  none\r\n");
	}
	else if (!(num % 2)) {
		strcat(save_buffer, "\r\n");
	}
}


/**
* Displays the scripts from a given list.
*
* @param struct trig_proto_list *list The list (start) to show.
* @param char *save_buffer A buffer to store the results in.
*/
void get_script_display(struct trig_proto_list *list, char *save_buffer) {
	char lbuf[MAX_STRING_LENGTH];
	struct trig_proto_list *iter;
	trig_data *proto;
	int count = 0;
	
	*save_buffer = '\0';
	
	for (iter = list; iter; iter = iter->next) {
		if ((proto = real_trigger(iter->vnum)) != NULL) {
			strcpy(lbuf, GET_TRIG_NAME(proto));
		}
		else {
			strcpy(lbuf, "UNKNOWN");
		}
		sprintf(save_buffer + strlen(save_buffer), "%2d. [%5d] %s\r\n", ++count, iter->vnum, lbuf);
	}
	
	if (count == 0) {
		strcat(save_buffer, " none\r\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// AUDITORS ////////////////////////////////////////////////////////////////

/**
* Checks for common extra description problems and reports them to ch.
*
* @param any_vnum vnum The vnum of the thing we're reporting on (obj, etc).
* @param struct extra_descr_data *list The list to check.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_extra_descs(any_vnum vnum, struct extra_descr_data *list, char_data *ch) {
	struct extra_descr_data *iter;
	bool problem = FALSE;
	
	for (iter = list; iter; iter = iter->next) {
		if (!iter->keyword || !*skip_filler(iter->keyword)) {
			olc_audit_msg(ch, vnum, "Extra desc: bad keywords");
			problem = TRUE;
		}
		if (!iter->description || !*iter->description || !str_cmp(iter->description, "Nothing.\r\n")) {
			olc_audit_msg(ch, vnum, "Extra desc '%s': bad description", NULLSAFE(iter->keyword));
			problem = TRUE;
		}
		else if (!strn_cmp(iter->description, "Nothing.", 8)) {
			olc_audit_msg(ch, vnum, "Extra desc '%s': description starting with 'Nothing.'", NULLSAFE(iter->keyword));
			problem = TRUE;
		}
	}
	
	return problem;
}


/**
* Checks for common interaction problems and reports them to ch.
*
* @param any_vnum vnum The vnum of the thing we're reporting on (obj, etc).
* @param struct interaction_item *list The list to check.
* @param int attach_type The type to accept (e.g. TYPE_MOB).
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_interactions(any_vnum vnum, struct interaction_item *list, int attach_type, char_data *ch) {
	struct interaction_item *iter;
	bool problem = FALSE;
	int code, type;
	
	struct audint_t {
		int code;
		double percent;
		UT_hash_handle hh;
	};
	struct audint_s {
		int type;
		struct audint_t *set;
		UT_hash_handle hh;
	};
	struct audint_s *as, *next_as, *set = NULL;
	struct audint_t *at, *next_at;
	
	for (iter = list; iter; iter = iter->next) {
		if (interact_attach_types[iter->type] != attach_type) {
			olc_audit_msg(ch, vnum, "Bad interaction: %s", interact_types[iter->type]);
			problem = TRUE;
		}
		
		// track cumulative percent
		if (iter->exclusion_code) {
			type = iter->type;
			HASH_FIND_INT(set, &type, as);
			if (!as) {
				CREATE(as, struct audint_s, 1);
				as->type = type;
				HASH_ADD_INT(set, type, as);
			}
		
			code = iter->exclusion_code;
			HASH_FIND_INT(as->set, &code, at);
			if (!at) {
				CREATE(at, struct audint_t, 1);
				at->code = code;
				HASH_ADD_INT(as->set, code, at);
			}
			at->percent += iter->percent;
		}
	}
	
	HASH_ITER(hh, set, as, next_as) {
		HASH_ITER(hh, as->set, at, next_at) {
			if (at->percent > 100.0) {
				olc_audit_msg(ch, vnum, "Interaction %s exclusion set '%c' totals %.2f%%", interact_types[as->type], (char)at->code, at->percent);
				problem = TRUE;
			}
			HASH_DEL(as->set, at);
			free(at);
		}
		HASH_DEL(set, as);
		free(as);
	}
	
	return problem;
}


/**
* Checks for common spawn list problems and reports them to ch.
*
* @param any_vnum vnum The vnum of the thing we're reporting on (obj, etc).
* @param struct spawn_info *list The list to check.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_spawns(any_vnum vnum, struct spawn_info *list, char_data *ch) {
	struct spawn_info *iter;
	bool problem = FALSE;
	
	for (iter = list; iter; iter = iter->next) {
		if (!mob_proto(iter->vnum)) {
			olc_audit_msg(ch, vnum, "Spawn for mob %d: mob does not exist", iter->vnum);
			problem = TRUE;
		}
		if (IS_SET(iter->flags, SPAWN_NOCTURNAL) && IS_SET(iter->flags, SPAWN_DIURNAL)) {
			olc_audit_msg(ch, vnum, "Spawn for mob %d (%s) has conflicting flags", iter->vnum, get_mob_name_by_proto(iter->vnum));
			problem = TRUE;
		}
		if (IS_SET(iter->flags, SPAWN_CLAIMED) && IS_SET(iter->flags, SPAWN_UNCLAIMED)) {
			olc_audit_msg(ch, vnum, "Spawn for mob %d (%s) has conflicting flags", iter->vnum, get_mob_name_by_proto(iter->vnum));
			problem = TRUE;
		}
		if (IS_SET(iter->flags, SPAWN_CITY) && IS_SET(iter->flags, SPAWN_OUT_OF_CITY)) {
			olc_audit_msg(ch, vnum, "Spawn for mob %d (%s) has conflicting flags", iter->vnum, get_mob_name_by_proto(iter->vnum));
			problem = TRUE;
		}
		if (IS_SET(iter->flags, SPAWN_NORTHERN) && IS_SET(iter->flags, SPAWN_SOUTHERN)) {
			olc_audit_msg(ch, vnum, "Spawn for mob %d (%s) has conflicting flags", iter->vnum, get_mob_name_by_proto(iter->vnum));
			problem = TRUE;
		}
		if (IS_SET(iter->flags, SPAWN_EASTERN) && IS_SET(iter->flags, SPAWN_WESTERN)) {
			olc_audit_msg(ch, vnum, "Spawn for mob %d (%s) has conflicting flags", iter->vnum, get_mob_name_by_proto(iter->vnum));
			problem = TRUE;
		}
	}

	return problem;
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Pre-checks several requirements for opening a new OLC editor.
*
* @param char_data *ch The person trying to edit.
* @param int type Any OLC_ type.
* @param any_vnum vnum The vnum the player wants to edit.
*/
bool can_start_olc_edit(char_data *ch, int type, any_vnum vnum) {
	descriptor_data *desc;
	char typename[42];
	bool found;
	
	if (IS_NPC(ch) || !ch->desc) {
		return FALSE;
	}
	
	sprintbit(GET_OLC_TYPE(ch->desc) != 0 ? GET_OLC_TYPE(ch->desc) : type, olc_type_bits, typename, FALSE);
		
	// check that they're not already editing something
	if (GET_OLC_VNUM(ch->desc) != NOTHING) {
		msg_to_char(ch, "You are currently editing %s %d.\r\n", typename, GET_OLC_VNUM(ch->desc));
		return FALSE;
	}
	
	if (!player_can_olc_edit(ch, type, vnum)) {
		msg_to_char(ch, "You don't have permission to edit that vnum.\r\n");
		return FALSE;
	}
	
	if (type == OLC_ROOM_TEMPLATE && !valid_room_template_vnum(vnum)) {
		msg_to_char(ch, "Invalid room template: may not be outside any adventure zone.\r\n");
		return FALSE;
	}
	
	// make sure nobody else is already editing it
	found = FALSE;
	for (desc = descriptor_list; desc && !found; desc = desc->next) {
		if (GET_OLC_TYPE(desc) == type && GET_OLC_VNUM(desc) == vnum) {
			found = TRUE;
		}
	}
	
	if (found) {
		msg_to_char(ch, "Someone else is already editing that %s.\r\n", typename);
		return FALSE;
	}
	
	return TRUE;
}


/**
* Creates a copy of a set of icons.
*
* @param struct icon_data *input_list The start of the list to copy.
* @return struct icon_data* A pointer to the start of the copied list.
*/
struct icon_data *copy_icon_set(struct icon_data *input_list) {
	struct icon_data *icon, *last_icon, *new, *list;
	
	// copy in order
	list = last_icon = NULL;
	for (icon = input_list; icon; icon = icon->next) {
		CREATE(new, struct icon_data, 1);
		new->type = icon->type;
		new->icon = str_dup(NULLSAFE(icon->icon));
		new->color = str_dup(NULLSAFE(icon->color));
		new->next = NULL;
		
		if (last_icon) {
			last_icon->next = new;
		}
		else {
			list = new;
		}
		
		last_icon = new;
	}
	
	sort_icon_set(&list);
	return list;
}


/**
* Creates a copy of a restriction list.
*
* @param struct interact_restriction *input_list The list to copy.
* @return struct interact_restriction* The copied list.
*/
struct interact_restriction *copy_interaction_restrictions(struct interact_restriction *input_list) {
	struct interact_restriction *iter, *new_res, *list, *last;
	
	// copy in order
	list = last = NULL;
	LL_FOREACH(input_list, iter) {
		CREATE(new_res, struct interact_restriction, 1);
		*new_res = *iter;
		new_res->next = NULL;
		
		if (last) {
			last->next = new_res;
		}
		else {
			list = new_res;
		}
		last = new_res;
	}
	
	return list;
}


/**
* Creates a copy of an interaction list.
*
* @param struct interaction_item *input_list A pointer to the start of the list to copy.
* @return struct interaction_item* A pointer to the start of the copy list.
*/
struct interaction_item *copy_interaction_list(struct interaction_item *input_list) {
	struct interaction_item *interact, *last_interact, *new_interact, *list;
	
	// copy interactions in order
	list = last_interact = NULL;
	for (interact = input_list; interact; interact = interact->next) {
		CREATE(new_interact, struct interaction_item, 1);
		*new_interact = *interact;
		new_interact->restrictions = copy_interaction_restrictions(interact->restrictions);
		new_interact->next = NULL;
		
		// preserve order
		if (last_interact) {
			last_interact->next = new_interact;
		}
		else {
			list = new_interact;
		}
		
		last_interact = new_interact;
	}
			
	// ensure these are sorted
	sort_interactions(&list);
	return list;
}


/**
* Creates a copy of a spawn list.
*
* @param struct spawn_info *input_list A pointer to the start of the list to copy.
* @return struct spawn_info* A pointer to the start of the copy list.
*/
struct spawn_info *copy_spawn_list(struct spawn_info *input_list) {
	struct spawn_info *spawn, *new_spawn, *last, *list;
	
	list = last = NULL;
	for (spawn = input_list; spawn; spawn = spawn->next) {
		CREATE(new_spawn, struct spawn_info, 1);
		*new_spawn = *spawn;
		new_spawn->next = NULL;
		
		if (last) {
			last->next = new_spawn;
		}
		else {
			list = new_spawn;
		}
		last = new_spawn;
	}
	
	return list;
}


/**
* Finds an extra description by number, starting with 1.
*
* @param struct extra_descr_data *list The list to search.
* @param int num Which extra description in the linked list.
* @return struct extra_descr_data* The extra-desc, if any.
*/
struct extra_descr_data *find_extra_desc_by_num(struct extra_descr_data *list, int num) {
	struct extra_descr_data *ex;
	
	ex = list;
	while (--num > 0 && ex) {
		ex = ex->next;
	}
	
	return ex;
}


/**
* @param char *name An olc type name input.
* @return int the OLC_ type (flag) or 
*/
int find_olc_type(char *name) {
	int iter, type = NOBITS;
	for (iter = 0; iter < NUM_OLC_TYPES && !type; ++iter) {
		if (is_abbrev(name, olc_type_bits[iter])) {
			type = BIT(iter);
		}
	}
	return type;
}


/**
* @param char_data *ch The person trying to olc-edit.
* @param int type Any OLC_ mode.
* @param any_vnum vnum The vnum they are trying to edit.
*/
bool player_can_olc_edit(char_data *ch, int type, any_vnum vnum) {
	if (IS_NPC(ch) && !AFF_FLAGGED(ch, AFF_CHARM) && !ch->desc) {
		// scripts
		return TRUE;
	}
	else if (IS_NPC(ch)) {
		return FALSE;
	}
	else if (GET_ACCESS_LEVEL(ch) >= LVL_UNRESTRICTED_BUILDER) {
		// always
		return TRUE;
	}
	else if (vnum == 0 && !OLC_FLAGGED(ch, OLC_FLAG_ALL_VNUMS)) {
		// players cannot normally edit vnum 0 -- this is the "bug" obj/mob
		return FALSE;
	}
	else if (IS_SET(type, OLC_MAP) && !OLC_FLAGGED(ch, OLC_FLAG_MAP_EDIT)) {
		return FALSE;
	}
	else if (IS_SET(type, OLC_BOOK) && book_proto(vnum) && book_proto(vnum)->author == GET_IDNUM(ch)) {
		// own book
		return TRUE;
	}
	else if (IS_SET(type, OLC_ADVENTURE)) {
		if (OLC_FLAGGED(ch, OLC_FLAG_NO_ADVENTURE)) {
			return FALSE;
		}
		else if (OLC_FLAGGED(ch, OLC_FLAG_ALL_VNUMS)) {
			return TRUE;
		}
		else {
			// otherwise, can only edit an adventure if its contained vnums are editable
			adv_data *adv = adventure_proto(vnum);
			if (adv && GET_ADV_START_VNUM(adv) >= GET_OLC_MIN_VNUM(ch) && GET_ADV_END_VNUM(adv) <= GET_OLC_MAX_VNUM(ch)) {
				return TRUE;
			}
		}
	}
	else if (OLC_FLAGGED(ch, OLC_FLAG_ALL_VNUMS) || (GET_OLC_MIN_VNUM(ch) <= vnum && GET_OLC_MAX_VNUM(ch) >= vnum)) {
		// OLC_x: olc allows/disallows
		if (IS_SET(type, OLC_ARCHETYPE) && !OLC_FLAGGED(ch, OLC_FLAG_NO_ARCHETYPE)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_AUGMENT) && !OLC_FLAGGED(ch, OLC_FLAG_NO_AUGMENT)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_BUILDING) && !OLC_FLAGGED(ch, OLC_FLAG_NO_BUILDING)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_CRAFT) && !OLC_FLAGGED(ch, OLC_FLAG_NO_CRAFT)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_CROP) && !OLC_FLAGGED(ch, OLC_FLAG_NO_CROP)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_EVENT) && !OLC_FLAGGED(ch, OLC_FLAG_NO_EVENTS)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_FACTION) && !OLC_FLAGGED(ch, OLC_FLAG_NO_FACTIONS)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_GENERIC) && !OLC_FLAGGED(ch, OLC_FLAG_NO_GENERICS)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_GLOBAL) && !OLC_FLAGGED(ch, OLC_FLAG_NO_GLOBAL)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_MOBILE) && !OLC_FLAGGED(ch, OLC_FLAG_NO_MOBILE)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_MORPH) && !OLC_FLAGGED(ch, OLC_FLAG_NO_MORPHS)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_OBJECT) && !OLC_FLAGGED(ch, OLC_FLAG_NO_OBJECT)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_PROGRESS) && !OLC_FLAGGED(ch, OLC_FLAG_NO_PROGRESS)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_QUEST) && !OLC_FLAGGED(ch, OLC_FLAG_NO_QUESTS)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_ROOM_TEMPLATE) && !OLC_FLAGGED(ch, OLC_FLAG_NO_ROOM_TEMPLATE)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_SHOP) && !OLC_FLAGGED(ch, OLC_FLAG_NO_SHOPS)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_SOCIAL) && !OLC_FLAGGED(ch, OLC_FLAG_NO_SOCIALS)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_TRIGGER) && !OLC_FLAGGED(ch, OLC_FLAG_NO_TRIGGER)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_VEHICLE) && !OLC_FLAGGED(ch, OLC_FLAG_NO_VEHICLES)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_SECTOR) && !OLC_FLAGGED(ch, OLC_FLAG_NO_SECTORS)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_ABILITY) && !OLC_FLAGGED(ch, OLC_FLAG_NO_ABILITIES)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_CLASS) && !OLC_FLAGGED(ch, OLC_FLAG_NO_CLASSES)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_SKILL) && !OLC_FLAGGED(ch, OLC_FLAG_NO_SKILLS)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_BOOK)) {
			// no special permissions for books
			return TRUE;
		}
	}

	// nope	
	return FALSE;
}


/**
* For immortals with an open editor, a prompt tag.
*
* @param char_data *ch The player.
* @return char* A prompt tag like [o123] or empty string.
*/
char *prompt_olc_info(char_data *ch) {
	static char output[MAX_PROMPT_LENGTH];
	char typename[256];
	
	*output = '\0';

	if (!ch || !ch->desc || !IS_IMMORTAL(ch) || !GET_OLC_TYPE(ch->desc)) {
		return output;
	}
	
	sprintbit(GET_OLC_TYPE(ch->desc), olc_type_bits, typename, FALSE);
	
	snprintf(output, sizeof(output), "%c%d", LOWER(typename[0]), GET_OLC_VNUM(ch->desc));
	return output;
}


/**
* Generic processor for doubles/floats/percents in olc:
* 
* @param char_data *ch The player using OLC.
* @param char *argument The argument the player entered.
* @param char *name The display name of the item, e.g. "decay timer".
* @param char *command The command typed to set this, e.g. "timer". (optional)
* @param double min The minimum legal value.
* @param double max The maximum legal value.
* @param double old_value The previous value of the item (in case of no change).
* @return double The new value to set the item to.
*/
double olc_process_double(char_data *ch, char *argument, char *name, char *command, double min, double max, double old_value) {
	double val = atof(argument);
	
	if (!*argument) {
		msg_to_char(ch, "Set the %s to what?\r\n", name);
	}
	else if (!isdigit(*argument) && ((*argument != '-' && *argument != '.') || !isdigit(argument[1]))) {
		msg_to_char(ch, "Invalid setting. Please choose a decimal number.\r\n");
	}
	else if (val < min || val > max) {
		msg_to_char(ch, "You must choose a value between %.2f and %.2f.\r\n", min, max);
	}
	else {
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "You set the %s to %.2f.\r\n", name, val);
		}
		return val;
	}
	
	// fall-through
	return old_value;
}


/**
* Generic flag processor for olc:
*
* @param char_data *ch The player using OLC.
* @param char *argument The arguments the player entered.
* @param char *name The display name of the flag set, e.g. "flags".
* @param char *command The command typed to toggle these bits, e.g. "affects". (optional)
* @param const char **flag_names The "\n"-terminated array of names for the flags.
* @param bitvector_t existing_bits The previous value of the bitset.
* @return bitvector_t The new value of the whole bitset.
*/
bitvector_t olc_process_flag(char_data *ch, char *argument, char *name, char *command, const char **flag_names, bitvector_t existing_bits) {
	char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH], line[256];
	bool add = FALSE, remove = FALSE, toggle = FALSE, alldigit, found;
	bitvector_t bit;
	int iter;
	
	if (!*argument) {
		*buf = '\0';
		for (iter = 0; *flag_names[iter] != '\n'; ++iter) {
			sprintf(line, "%s%s", flag_names[iter], (IS_SET(existing_bits, BIT(iter)) ? " (on)" : ""));
			sprintf(buf + strlen(buf), "%2d. %s%-30.30s&0%s", (iter + 1), (IS_SET(existing_bits, BIT(iter)) ? "&g" : ""), line, ((iter % 2) ? "\r\n" : ""));
		}
		if ((iter % 2) != 0) {
			strcat(buf, "\r\n");
		}
		
		if (*buf) {
			msg_to_char(ch, "The following %s flags are available:\r\n", name);
			send_to_char(buf, ch);
			if (command && *command) {
				msg_to_char(ch, "Use <&y%s add NAME&0>, <&y%s remove NAME&0>, or <&y%s NAME&0> to change them\r\n", command, command, command);
			}
		}
		else {
			msg_to_char(ch, "There are no %s flags available at this time.\r\n", name);
		}
		return existing_bits;
	}
	
	half_chop(argument, arg, arg2);
	
	if (!str_cmp(arg, "add")) {
		add = TRUE;
	}
	else if (!str_cmp(arg, "rem") || !str_cmp(arg, "remove")) {
		remove = TRUE;
	}
	else {
		toggle = TRUE;
		
		// 1st arg is a bit then ... concat it with the others
		if (*arg) {
			strcat(arg, " ");
		}
		strcat(arg, arg2);
		strcpy(arg2, arg);
	}
	
	while (*arg2) {
		// split off the first flag
		strcpy(arg, arg2);
		half_chop(arg, arg2, arg3);
		
		// did they enter a number? (make sure it's all digits because some flags could start with a number)
		alldigit = TRUE;
		for (iter = 0; iter < strlen(arg2) && alldigit; ++iter) {
			if (!isdigit(arg2[iter])) {
				alldigit = FALSE;
			}
		}
	
		// validate bit
		if (alldigit) {
			bit = atoi(arg2) - 1;	// numbers are shown as id+1 to be 1-based not 0-based
		
			// ensure bit is not past the end of the flag_names, and that the flag is not no-set
			found = FALSE;
			for (iter = 0; *flag_names[iter] != '\n' && !found; ++iter) {
				if (iter == bit) {
					found = TRUE;
				}
			}
		
			if (!found) {
				bit = NOTHING;
			}
		}
		else {
			bit = search_block(arg2, flag_names, FALSE);
		}
	
		// valid bit?
		if (bit == NOTHING) {
			msg_to_char(ch, "Unknown %s flag '%s'\r\n", name, arg2);
		}
		else if (!remove && *flag_names[bit] == '*') {
			msg_to_char(ch, "You cannot set the '%s' %s flag.\r\n", flag_names[bit], name);
		}
		else {
			// convert from position to bit
			bit = BIT(bit);
	
			// check that we got an add or remove
			if (toggle || (!add && !remove)) {
				remove = IS_SET(existing_bits, bit) ? TRUE : FALSE;
				add = IS_SET(existing_bits, bit) ? FALSE : TRUE;
			}
	
			// and process
			if (add) {
				SET_BIT(existing_bits, bit);
			}
			else {
				REMOVE_BIT(existing_bits, bit);
			}
	
			// message			
			if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
				send_config_msg(ch, "ok_string");
			}
			else {
				sprintbit(bit, flag_names, buf, FALSE);
				msg_to_char(ch, "You %s the %s %s flag.\r\n", (add ? "add" : "remove"), buf, name);
			}
		}
		
		// last, copy back any remaining args
		strcpy(arg2, arg3);
	}
	
	// this was modified earlier -- return the result
	return existing_bits;
}


/**
* Generic processor for simple numbers in olc:
* 
* @param char_data *ch The player using OLC.
* @param char *argument The argument the player entered.
* @param char *name The display name of the item, e.g. "decay timer".
* @param char *command The command typed to set this, e.g. "timer". (optional)
* @param int min The minimum legal value.
* @param int max The maximum legal value.
* @param int old_value The previous value of the item (in case of no change).
* @return int The new value to set the item to.
*/
int olc_process_number(char_data *ch, char *argument, char *name, char *command, int min, int max, int old_value) {
	int val = atoi(argument);
	
	if (!*argument) {
		msg_to_char(ch, "Set the %s to what?\r\n", name);
	}
	else if (!isdigit(*argument) && (*argument != '-' || !isdigit(argument[1]))) {
		msg_to_char(ch, "Invalid setting. Please choose a number.\r\n");
	}
	else if (val < min || val > max) {
		msg_to_char(ch, "You must choose a value between %d and %d.\r\n", min, max);
	}
	else {
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "You set the %s to %d.\r\n", name, val);
		}
		return val;
	}
	
	// fall-through
	return old_value;
}


/**
* Sub-processor for requirement args.
*
* @param char_data *ch The player using OLC.
* @param int type REQ_ type.
* @param char *argument The remainder of the player's args.
* @param bool find_amount Whether or not to take the 1st arg as amount, if the type requires it.
* @param int *amount A variable to store the amount to.
* @param any_vnum *vnum A variable to store the vnum to.
* @param bitvector_t *misc A variable to store the misc value to.
* param char *group A variable to store the group arg, if any.
* @return bool TRUE if the arguments were provided correctly, FALSE if an error was sent.
*/
bool olc_parse_requirement_args(char_data *ch, int type, char *argument, bool find_amount, int *amount, any_vnum *vnum, bitvector_t *misc, char *group) {
	extern const char *action_bits[];
	extern const char *diplomacy_flags[];
	extern const char *component_flags[];
	extern const char *component_types[];
	extern const char *function_flags[];
	extern const char *vehicle_flags[];
	
	char arg[MAX_INPUT_LENGTH]; 
	bool need_abil = FALSE, need_bld = FALSE, need_component = FALSE;
	bool need_mob = FALSE, need_obj = FALSE, need_quest = FALSE;
	bool need_rmt = FALSE, need_sect = FALSE, need_skill = FALSE;
	bool need_veh = FALSE, need_mob_flags = FALSE, need_faction = FALSE;
	bool need_currency = FALSE, need_func_flags = FALSE, need_veh_flags = FALSE;
	bool need_dip_flags = FALSE, need_event = FALSE;
	
	*amount = 1;
	*vnum = 0;
	*misc = 0;
	*group = 0;
	
	// REQ_x: determine which args we need
	switch (type) {
		case REQ_COMPLETED_QUEST:
		case REQ_NOT_COMPLETED_QUEST:
		case REQ_NOT_ON_QUEST: {
			need_quest = TRUE;
			break;
		}
		case REQ_GET_COMPONENT:
		case REQ_EMPIRE_PRODUCED_COMPONENT: {
			need_component = TRUE;
			break;
		}
		case REQ_GET_OBJECT:
		case REQ_WEARING:
		case REQ_WEARING_OR_HAS:
		case REQ_EMPIRE_PRODUCED_OBJECT: {
			need_obj = TRUE;
			break;
		}
		case REQ_KILL_MOB: {
			need_mob = TRUE;
			break;
		}
		case REQ_KILL_MOB_FLAGGED: {
			need_mob_flags = TRUE;
			break;
		}
		case REQ_OWN_BUILDING: {
			need_bld = TRUE;
			break;
		}
		case REQ_OWN_BUILDING_FUNCTION: {
			need_func_flags = TRUE;
			break;
		}
		case REQ_OWN_VEHICLE: {
			need_veh = TRUE;
			break;
		}
		case REQ_OWN_VEHICLE_FLAGGED: {
			need_veh_flags = TRUE;
			break;
		}
		case REQ_SKILL_LEVEL_OVER:
		case REQ_SKILL_LEVEL_UNDER:
		case REQ_CAN_GAIN_SKILL: {
			need_skill = TRUE;
			break;
		}
		case REQ_TRIGGERED: {
			break;
		}
		case REQ_VISIT_BUILDING: {
			need_bld = TRUE;
			break;
		}
		case REQ_VISIT_ROOM_TEMPLATE: {
			need_rmt = TRUE;
			break;
		}
		case REQ_VISIT_SECTOR: {
			need_sect = TRUE;
			break;
		}
		case REQ_HAVE_ABILITY: {
			need_abil = TRUE;
			break;
		}
		case REQ_REP_OVER: {
			need_faction = TRUE;
			break;
		}
		case REQ_REP_UNDER: {
			need_faction = TRUE;
			break;
		}
		case REQ_GET_CURRENCY: {
			need_currency = TRUE;
			break;
		}
		case REQ_OWN_SECTOR: {
			need_sect = TRUE;
			break;
		}
		case REQ_DIPLOMACY: {
			need_dip_flags = TRUE;
			break;
		}
		case REQ_EVENT_RUNNING:
		case REQ_EVENT_NOT_RUNNING: {
			need_event = TRUE;
			break;
		}
		case REQ_OWN_HOMES:
		case REQ_CROP_VARIETY:
		case REQ_EMPIRE_WEALTH:
		case REQ_EMPIRE_FAME:
		case REQ_EMPIRE_MILITARY:
		case REQ_EMPIRE_GREATNESS:
		case REQ_GET_COINS: {
			// need nothing?
			break;
		}
	}
	
	// REQ_AMT_x: possible args
	if (requirement_amt_type[type] == REQ_AMT_REPUTATION && find_amount) {
		argument = any_one_arg(argument, arg);
		if (!*arg || (*amount = get_reputation_by_name(arg)) == NOTHING) {
			msg_to_char(ch, "You must provide a reputation.\r\n");
			return FALSE;
		}
	}
	else if (requirement_amt_type[type] != REQ_AMT_NONE && find_amount) {
		argument = any_one_arg(argument, arg);
		if (!*arg || !isdigit(*arg) || (*amount = atoi(arg)) < 0) {
			msg_to_char(ch, "You must provide an amount.\r\n");
			return FALSE;
		}
	}
	
	if (need_abil) {
		argument = any_one_arg(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "You must provide an ability vnum.\r\n");
			return FALSE;
		}
		if (!isdigit(*arg) || (*vnum = atoi(arg)) < 0 || !find_ability_by_vnum(*vnum)) {
			msg_to_char(ch, "Invalid ability vnum '%s'.\r\n", arg);
			return FALSE;
		}
	}
	
	if (need_bld) {
		argument = any_one_arg(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "You must provide a building vnum.\r\n");
			return FALSE;
		}
		if (!isdigit(*arg) || (*vnum = atoi(arg)) < 0 || !building_proto(*vnum)) {
			msg_to_char(ch, "Invalid building vnum '%s'.\r\n", arg);
			return FALSE;
		}
	}
	if (need_component) {
		argument = any_one_arg(argument, arg);
		skip_spaces(&argument);
		if (!*arg) {
			msg_to_char(ch, "You must provide a component type.\r\n");
			return FALSE;
		}
		if ((*vnum = search_block(arg, component_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid component type '%s'.\r\n", arg);
			return FALSE;
		}
		if (*argument) {
			argument = any_one_word(argument, arg);
			*misc = olc_process_flag(ch, arg, "component", "", component_flags, NOBITS);
		}
	}
	if (need_currency) {
		generic_data *gen;
		argument = any_one_word(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "You must provide a generic currency vnum.\r\n");
			return FALSE;
		}
		if (!(gen = find_generic(atoi(arg), GENERIC_CURRENCY))) {
			msg_to_char(ch, "Invalid generic currency '%s'.\r\n", arg);
			return FALSE;
		}
		*vnum = GEN_VNUM(gen);
	}
	if (need_dip_flags) {
		argument = any_one_word(argument, arg);
		*misc = olc_process_flag(ch, arg, "diplomacy", "", diplomacy_flags, NOBITS);
		if (!*misc) {
			msg_to_char(ch, "You must provide diplomacy flags.\r\n");
			return FALSE;
		}
	}
	if (need_event) {
		event_data *event;
		argument = any_one_word(argument, arg);
		if (!*arg || !isdigit(*arg)) {
			msg_to_char(ch, "You must provide an event vnum.\r\n");
			return FALSE;
		}
		if (!(event = find_event_by_vnum(atoi(arg)))) {
			msg_to_char(ch, "Invalid event '%s'.\r\n", arg);
			return FALSE;
		}
		*vnum = EVT_VNUM(event);
	}
	if (need_faction) {
		faction_data *fct;
		argument = any_one_word(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "You must provide a faction.\r\n");
			return FALSE;
		}
		if (!(fct = find_faction(arg))) {
			msg_to_char(ch, "Invalid faction '%s'.\r\n", arg);
			return FALSE;
		}
		*vnum = FCT_VNUM(fct);
	}
	if (need_func_flags) {
		argument = any_one_word(argument, arg);
		*misc = olc_process_flag(ch, arg, "function", "", function_flags, NOBITS);
		if (!*misc) {
			msg_to_char(ch, "You must provide function flags.\r\n");
			return FALSE;
		}
	}
	if (need_mob) {
		argument = any_one_arg(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "You must provide a mob vnum.\r\n");
			return FALSE;
		}
		if (!isdigit(*arg) || (*vnum = atoi(arg)) < 0 || !mob_proto(*vnum)) {
			msg_to_char(ch, "Invalid mobile vnum '%s'.\r\n", arg);
			return FALSE;
		}
	}
	if (need_mob_flags) {
		argument = any_one_word(argument, arg);
		*misc = olc_process_flag(ch, arg, "mob", "", action_bits, NOBITS);
		if (!*misc) {
			msg_to_char(ch, "You must provide mob flags.\r\n");
			return FALSE;
		}
	}
	if (need_obj) {
		argument = any_one_arg(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "You must provide an object vnum.\r\n");
			return FALSE;
		}
		if (!isdigit(*arg) || (*vnum = atoi(arg)) < 0 || !obj_proto(*vnum)) {
			msg_to_char(ch, "Invalid object vnum '%s'.\r\n", arg);
			return FALSE;
		}
	}
	if (need_quest) {
		argument = any_one_arg(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "You must provide a quest vnum.\r\n");
			return FALSE;
		}
		if (!isdigit(*arg) || (*vnum = atoi(arg)) < 0 || !quest_proto(*vnum)) {
			msg_to_char(ch, "Invalid quest vnum '%s'.\r\n", arg);
			return FALSE;
		}
	}
	if (need_rmt) {
		argument = any_one_arg(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "You must provide a room template vnum.\r\n");
			return FALSE;
		}
		if (!isdigit(*arg) || (*vnum = atoi(arg)) < 0 || !room_template_proto(*vnum)) {
			msg_to_char(ch, "Invalid room template vnum '%s'.\r\n", arg);
			return FALSE;
		}
	}
	if (need_sect) {
		argument = any_one_arg(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "You must provide a sector vnum.\r\n");
			return FALSE;
		}
		if (!isdigit(*arg) || (*vnum = atoi(arg)) < 0 || !sector_proto(*vnum)) {
			msg_to_char(ch, "Invalid sector vnum '%s'.\r\n", arg);
			return FALSE;
		}
	}
	if (need_skill) {
		argument = any_one_arg(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "You must provide a skill vnum.\r\n");
			return FALSE;
		}
		if (!isdigit(*arg) || (*vnum = atoi(arg)) < 0 || !find_skill_by_vnum(*vnum)) {
			msg_to_char(ch, "Invalid skill vnum '%s'.\r\n", arg);
			return FALSE;
		}
	}
	if (need_veh) {
		argument = any_one_arg(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "You must provide a vehicle vnum.\r\n");
			return FALSE;
		}
		if (!isdigit(*arg) || (*vnum = atoi(arg)) < 0 || !vehicle_proto(*vnum)) {
			msg_to_char(ch, "Invalid vehicle vnum '%s'.\r\n", arg);
			return FALSE;
		}
	}
	if (need_veh_flags) {
		argument = any_one_word(argument, arg);
		*misc = olc_process_flag(ch, arg, "vehicle", "", vehicle_flags, NOBITS);
		if (!*misc) {
			msg_to_char(ch, "You must provide vehicle flags.\r\n");
			return FALSE;
		}
	}
	
	// anything left for a group letter?
	skip_spaces(&argument);
	if (*argument && str_cmp(argument, "none")) {	// ignore a "none" here, for "no group"
		if (strlen(argument) != 1 || !isalpha(*argument)) {
			msg_to_char(ch, "Group must be a letter (or may be blank).\r\n");
			return FALSE;
		}
		else {
			*group = *argument;
		}
	}
	
	// all good
	return TRUE;
}


/**
* Processing for requirements (e.g. quest tasks and pre-reqs).
*
* @param char_data *ch The player using OLC.
* @param char *argument The full argument after the command.
* @param struct req_data **list A pointer to the list we're adding/changing.
* @param char *command The command used by the player (requirements, tasks, prereqs).
* @param bool allow_tracker_types If TRUE, allows types that will require a quest tracker.
*/
void olc_process_requirements(char_data *ch, char *argument, struct req_data **list, char *command, bool allow_tracker_types) {
	extern const bool requirement_needs_tracker[];
	extern const char *requirement_types[];
	
	char cmd_arg[MAX_INPUT_LENGTH], field_arg[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH];
	char vnum_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	struct req_data *req, *iter, *change, *copyfrom;
	int findtype, num, type;
	bitvector_t misc;
	any_vnum vnum;
	char group;
	bool found;
	
	argument = any_one_arg(argument, cmd_arg);	// add/remove/change/copy
	
	if (is_abbrev(cmd_arg, "copy")) {
		// usage: qedit * copy <from type> <from vnum> <tasks/prereqs>
		argument = any_one_arg(argument, type_arg);	// just "quest" for now; any type with requirements
		argument = any_one_arg(argument, vnum_arg);	// any vnum for that type
		argument = any_one_arg(argument, field_arg);	// tasks/prereqs field to copy
		
		if (!*type_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: %s copy <from type> <from vnum> [tasks | prereqs (quests only)]\r\n", command);
		}
		else if ((findtype = find_olc_type(type_arg)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", type_arg);
		}
		else if (!isdigit(*vnum_arg)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy from which %s?\r\n", buf);
		}
		else if ((vnum = atoi(vnum_arg)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			copyfrom = NULL;
			
			switch (findtype) {
				case OLC_PROGRESS: {
					progress_data *from_prg = real_progress(vnum);
					if (from_prg) {
						copyfrom = PRG_TASKS(from_prg);
					}
					break;
				}
				case OLC_QUEST: {
					// requires tasks/preqeqs
					if (!*field_arg || (!is_abbrev(field_arg, "tasks") && !is_abbrev(field_arg, "prereqs"))) {
						msg_to_char(ch, "Copy from the 'tasks' or 'prereqs' list?\r\n");
						return;
					}
					quest_data *from_qst = quest_proto(vnum);
					if (from_qst) {
						copyfrom = (is_abbrev(field_arg, "tasks") ? QUEST_TASKS(from_qst) : QUEST_PREREQS(from_qst));
					}
					break;
				}
				case OLC_SOCIAL: {
					social_data *from_soc = social_proto(vnum);
					if (from_soc) {
						copyfrom = SOC_REQUIREMENTS(from_soc);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy %s from %ss.\r\n", command, buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, vnum_arg);
			}
			else {
				smart_copy_requirements(list, copyfrom);
				msg_to_char(ch, "Copied %s from %s %d.\r\n", command, buf, vnum);
			}
		}
	}	// end 'copy'
	else if (is_abbrev(cmd_arg, "remove")) {
		// usage: qedit * remove <number | all>
		skip_spaces(&argument);	// only arg is number
		
		if (!*argument) {
			msg_to_char(ch, "Remove which %s (number)?\r\n", command);
		}
		else if (!str_cmp(argument, "all")) {
			free_requirements(*list);
			*list = NULL;
			msg_to_char(ch, "You remove all the %s.\r\n", command);
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid %s number.\r\n", command);
		}
		else {
			found = FALSE;
			LL_FOREACH(*list, iter) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the %s info for: %s\r\n", command, requirement_string(iter, TRUE));
					LL_DELETE(*list, iter);
					free(iter);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid %s number.\r\n", command);
			}
		}
	}	// end 'remove'
	else if (is_abbrev(cmd_arg, "add")) {		
		// usage: qedit * add <type> <vnum>
		argument = any_one_arg(argument, type_arg);
		
		if (!*type_arg) {
			msg_to_char(ch, "Usage: %s add <type> [amount] [vnum] (see HELP REQUIREMENTS)\r\n", command);
		}
		else if ((type = search_block(type_arg, requirement_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", type_arg);
		}
		else if (!olc_parse_requirement_args(ch, type, argument, TRUE, &num, &vnum, &misc, &group)) {
			// sends own error
		}
		else if (!allow_tracker_types && requirement_needs_tracker[type]) {
			msg_to_char(ch, "You can't set that type of requirement on this (it requires a quest tracker).\r\n");
		}
		else {
			// success
			CREATE(req, struct req_data, 1);
			req->type = type;
			req->vnum = vnum;
			req->misc = misc;
			req->group = group;
			req->needed = num;
		
			LL_APPEND(*list, req);
			LL_SORT(*list, sort_requirements_by_group);
			msg_to_char(ch, "You add %s: %s\r\n", command, requirement_string(req, TRUE));
		}
	}	// end 'add'
	else if (is_abbrev(cmd_arg, "change")) {
		// usage: qedit * change <number> vnum <number>
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		skip_spaces(&argument);
		
		if (!*num_arg || !isdigit(*num_arg) || !*field_arg) {
			msg_to_char(ch, "Usage: %s change <number> <amount | vnum> <value>\r\n", command);
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		LL_FOREACH(*list, iter) {
			if (--num == 0) {
				change = iter;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid %s number.\r\n", command);
		}
		else if (is_abbrev(field_arg, "amount") || is_abbrev(field_arg, "value") || is_abbrev(field_arg, "quantity")) {
			if (requirement_amt_type[change->type] == REQ_AMT_REPUTATION && (num = get_reputation_by_name(argument)) == NOTHING) {
				msg_to_char(ch, "Invalid reputation '%s'.\r\n", argument);
				return;
			}
			else if (requirement_amt_type[change->type] != REQ_AMT_REPUTATION && (!isdigit(*argument) || (num = atoi(argument)) < 0)) {
				msg_to_char(ch, "Invalid amount '%s'.\r\n", argument);
				return;
			}
			else {
				change->needed = num;
				msg_to_char(ch, "You change %s %d to: %s\r\n", command, atoi(num_arg), requirement_string(change, TRUE));
			}
		}
		else if (is_abbrev(field_arg, "vnum")) {
			// num is junk here
			if (!olc_parse_requirement_args(ch, change->type, argument, FALSE, &num, &vnum, &misc, &group)) {
				// sends own error
			}
			else {
				change->vnum = vnum;
				change->misc = misc;
				msg_to_char(ch, "Changed %s %d to: %s\r\n", command, atoi(num_arg), requirement_string(change, TRUE));
			}
		}
		else if (is_abbrev(field_arg, "group")) {
			if (!str_cmp(argument, "none")) {
				change->group = 0;
				LL_SORT(*list, sort_requirements_by_group);
				msg_to_char(ch, "Changed %s %d to: %s\r\n", command, atoi(num_arg), requirement_string(change, TRUE));
			}
			else if (strlen(argument) != 1 || !isalpha(*argument)) {
				msg_to_char(ch, "Group must be a letter or 'none'.\r\n");
				return;
			}
			else {
				change->group = *argument;
				LL_SORT(*list, sort_requirements_by_group);
				msg_to_char(ch, "Changed %s %d to: %s\r\n", command, atoi(num_arg), requirement_string(change, TRUE));
			}
		}
		else {
			msg_to_char(ch, "You can only change the amount, group, or vnum.\r\n");
		}
	}	// end 'change'
	else if (is_abbrev(cmd_arg, "move")) {
		struct req_data *to_move, *prev, *a, *b, *a_next, *b_next, iitem;
		bool up;
		
		// usage: <cmd> move <number> <up | down>
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		up = is_abbrev(field_arg, "up");
		
		if (!*num_arg || !*field_arg) {
			msg_to_char(ch, "Usage: %s move <number> <up | down>\r\n", command);
		}
		else if (!isdigit(*num_arg) || (num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid %s number.\r\n", command);
		}
		else if (!is_abbrev(field_arg, "up") && !is_abbrev(field_arg, "down")) {
			msg_to_char(ch, "You must specify whether you're moving it up or down in the list.\r\n");
		}
		else if (up && num == 1) {
			msg_to_char(ch, "You can't move it up; it's already at the top of the list.\r\n");
		}
		else {
			// find the one to move
			to_move = prev = NULL;
			for (req = *list; req && !to_move; req = req->next) {
				if (--num == 0) {
					to_move = req;
				}
				else {
					// store for next iteration
					prev = req;
				}
			}
			
			if (!to_move) {
				msg_to_char(ch, "Invalid %s number.\r\n", command);
			}
			else if (!up && !to_move->next) {
				msg_to_char(ch, "You can't move it down; it's already at the bottom of the list.\r\n");
			}
			else {
				// SUCCESS: "move" them by swapping data
				if (up) {
					a = prev;
					b = to_move;
				}
				else {
					a = to_move;
					b = to_move->next;
				}
				
				// store next pointers
				a_next = a->next;
				b_next = b->next;
				
				// swap data
				iitem = *a;
				*a = *b;
				*b = iitem;
				
				// restore next pointers
				a->next = a_next;
				b->next = b_next;
				
				// message: re-atoi(num_arg) because we destroyed num finding our target
				msg_to_char(ch, "You move %s %d %s.\r\n", command, atoi(num_arg), (up ? "up" : "down"));
			}
		}
	}	// end 'move'
	else {
		msg_to_char(ch, "Usage: %s add <type> <vnum>\r\n", command);
		msg_to_char(ch, "Usage: %s change <number> vnum <value>\r\n", command);
		msg_to_char(ch, "Usage: %s copy <from type> <from vnum> [tasks/prereqs]\r\n", command);
		msg_to_char(ch, "Usage: %s remove <number | all>\r\n", command);
		msg_to_char(ch, "Usage: %s move <number> <up | down>\r\n", command);
	}
}


/**
* Generic simple string processor for olc. This can't be used for multi-line
* strings, which require the string editor.
*
* @param char_data *ch The player using OLC.
* @param char *argument The arguments the player entered.
* @param const char *name The display name of the string we're setting, e.g. "short description".
* @param char **save_point A pointer to the location to save the string.
*/
void olc_process_string(char_data *ch, char *argument, const char *name, char **save_point) {
	if (!*argument) {
		msg_to_char(ch, "Set its %s to what?\r\n", name);
	}
	else {
		delete_doubledollar(argument);
		
		if (*save_point) {
			free(*save_point);
		}
		*save_point = str_dup(argument);
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "You set its %s to '%s&0'.\r\n", name, argument);
		}
	}
}


/**
* Generic type processor for olc:
*
* @param char_data *ch The player using OLC.
* @param char *argument The arguments the player entered.
* @param char *name The display name of the type, e.g. "material".
* @param char *command The command typed to set the type, e.g. "material". (optional)
* @param const char **flag_names The "\n"-terminated array of names for the types.
* @param bitvector_t *bit_set A pointer to the bits to read/set.
* @param int old_value The previous value (in case of no change).
* @return int The new type to set.
*/
int olc_process_type(char_data *ch, char *argument, char *name, char *command, const char **type_names, int old_value) {
	char buf[MAX_STRING_LENGTH], line[256];
	int type, iter;
	bool alldigit, found;
	
	if (!*argument) {
		*buf = '\0';
		for (iter = 0; *type_names[iter] != '\n'; ++iter) {
			sprintf(line, "%s%s", type_names[iter], ((old_value == iter) ? " (current)" : ""));
			sprintf(buf + strlen(buf), "%2d. %s%-30.30s&0%s", (iter + 1), (old_value == iter) ? "&g" : "", line, ((iter % 2) ? "\r\n" : ""));
		}
		if ((iter % 2) != 0) {
			strcat(buf, "\r\n");
		}
		
		if (*buf) {
			msg_to_char(ch, "The following %ss are available:\r\n", name);
			send_to_char(buf, ch);
			if (command && *command) {
				msg_to_char(ch, "Use <&y%s NAME&0> to change it\r\n", command);
			}
		}
		else {
			msg_to_char(ch, "There are no %ss available at this time.\r\n", name);
		}
		return old_value;		
	}
	
	// did they enter a number? (make sure it's all digits because some flags could start with a number)
	alldigit = TRUE;
	for (iter = 0; iter < strlen(argument) && alldigit; ++iter) {
		if (!isdigit(argument[iter])) {
			alldigit = FALSE;
		}
	}
	
	// validate type
	if (alldigit) {
		type = atoi(argument) - 1;	// numbers are shown as id+1 to be 1-based not 0-based
		
		// ensure type is not past the end of the type_names
		found = FALSE;
		for (iter = 0; *type_names[iter] != '\n' && !found; ++iter) {
			if (iter == type) {
				found = TRUE;
			}
		}
		
		if (!found) {
			type = NOTHING;
		}
	}
	else {
		type = search_block(argument, type_names, FALSE);
	}
	
	// did we find one?
	if (type == NOTHING) {
		msg_to_char(ch, "Unknown %s '%s'\r\n", name, argument);
		return old_value;
	}
	
	// is it legit?
	if (*type_names[type] == '*') {
		msg_to_char(ch, "You may not choose the '%s' %s.\r\n", type_names[type], name);
		return old_value;
	}
	
	// message
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		sprinttype(type, type_names, buf);
		msg_to_char(ch, "You set the %s to %s.\r\n", name, buf);
	}
	return type;
}


void olc_process_applies(char_data *ch, char *argument, struct apply_data **list) {
	extern const char *apply_types[];
	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	struct apply_data *apply, *next_apply, *change, *temp;
	int loc, num, iter;
	bool found;
	
	// arg1 arg2 arg3
	half_chop(argument, arg1, buf);
	half_chop(buf, arg2, arg3);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which apply (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			free_apply_list(*list);
			*list = NULL;
			msg_to_char(ch, "You remove all the applies.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid apply number.\r\n");
		}
		else {
			found = FALSE;
			for (apply = *list; apply && !found; apply = next_apply) {
				next_apply = apply->next;
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the %d to %s.\r\n", apply->weight, apply_types[apply->location]);
					REMOVE_FROM_LIST(apply, *list, next);
					free(apply);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid apply number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		num = atoi(arg2);
		
		if (!*arg2 || !*arg3 || (!isdigit(*arg2) && *arg2 != '-') || num == 0) {
			msg_to_char(ch, "Usage: apply add <value> <apply>\r\n");
		}
		else if ((loc = search_block(arg3, apply_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid apply.\r\n");
		}
		else {
			CREATE(apply, struct apply_data, 1);
			apply->location = loc;
			apply->weight = num;
			
			// append to end
			if ((temp = *list)) {
				while (temp->next) {
					temp = temp->next;
				}
				temp->next = apply;
			}
			else {
				*list = apply;
			}
			
			msg_to_char(ch, "You add %d to %s.\r\n", num, apply_types[loc]);
		}
	}
	else if (is_abbrev(arg1, "change")) {
		strcpy(num_arg, arg2);
		half_chop(arg3, type_arg, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg || !*val_arg) {
			msg_to_char(ch, "Usage: apply change <number> <value | apply> <new value>\r\n");
			return;
		}
		
		// find which one to change
		if (!isdigit(*num_arg) || (num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid apply number.\r\n");
			return;
		}
		change = NULL;
		for (apply = *list; apply && !change; apply = apply->next) {
			if (--num == 0) {
				change = apply;
				break;
			}
		}
		if (!change) {
			msg_to_char(ch, "Invalid apply number.\r\n");
		}
		else if (is_abbrev(type_arg, "value") || is_abbrev(type_arg, "amount") || is_abbrev(type_arg, "quantity")) {
			num = atoi(val_arg);
			if ((!isdigit(*val_arg) && *val_arg != '-') || num == 0) {
				msg_to_char(ch, "Invalid value '%s'.\r\n", val_arg);
			}
			else {
				change->weight = num;
				msg_to_char(ch, "Apply %d changed to value %d.\r\n", atoi(num_arg), num);
			}
		}
		else if (is_abbrev(type_arg, "apply")) {
			if ((loc = search_block(val_arg, apply_types, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid apply.\r\n");
			}
			else {
				change->location = loc;
				msg_to_char(ch, "Apply %d changed to %s.\r\n", atoi(num_arg), apply_types[loc]);
			}
		}
		else {
			msg_to_char(ch, "You can only change the value or apply.\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: apply add <value> <apply>\r\n");
		msg_to_char(ch, "Usage: apply change <number> <value | apply> <new value>\r\n");
		msg_to_char(ch, "Usage: apply remove <number | all>\r\n");
		
		msg_to_char(ch, "Available applies:\r\n");
		for (iter = 0; *apply_types[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %-24.24s%s", apply_types[iter], ((iter % 2) ? "\r\n" : ""));
		}
		if ((iter % 2) != 0) {
			msg_to_char(ch, "\r\n");
		}
	}
}


/**
* Processes adding/changing custom messages for various olc types.
*
* @param char_data *ch The person editing.
* @param char *argument Text typed by the player.
* @param struct custom_message **list Pointer to the list of custom messages.
* @param const char **type_names List of names of valid types.
*/
void olc_process_custom_messages(char_data *ch, char *argument, struct custom_message **list, const char **type_names) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], *msgstr;
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	struct custom_message *custm, *change;
	int num, iter, msgtype;
	bool found;
	
	// arg1 arg2
	half_chop(argument, arg1, arg2);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which custom message (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			while ((custm = *list)) {
				*list = custm->next;
				if (custm->msg) {
					free(custm->msg);
				}
				free(custm);
			}
			msg_to_char(ch, "You remove all the custom messages.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid custom message number.\r\n");
		}
		else {
			found = FALSE;
			LL_FOREACH(*list, custm) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove custom message #%d.\r\n", atoi(arg2));
					LL_DELETE(*list, custm);
					free(custm);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid custom message number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		msgstr = any_one_word(arg2, arg);
		skip_spaces(&msgstr);
		
		if (!*arg || !*msgstr) {
			msg_to_char(ch, "Usage: custom add <type> <string>\r\n");
		}
		else if ((msgtype = search_block(arg, type_names, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", arg);
		}
		else {
			delete_doubledollar(msgstr);
			
			CREATE(custm, struct custom_message, 1);

			custm->type = msgtype;
			custm->msg = str_dup(msgstr);
			
			LL_APPEND(*list, custm);
			msg_to_char(ch, "You add a custom '%s' message:\r\n%s\r\n", type_names[custm->type], custm->msg);			
		}
	}
	else if (is_abbrev(arg1, "change")) {
		half_chop(arg2, num_arg, arg1);
		half_chop(arg1, type_arg, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg || !*val_arg) {
			msg_to_char(ch, "Usage: custom change <number> <type | message> <value>\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		LL_FOREACH(*list, custm) {
			if (--num == 0) {
				change = custm;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid custom message number.\r\n");
		}
		else if (is_abbrev(type_arg, "type")) {
			if ((msgtype = search_block(val_arg, type_names, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid type '%s'.\r\n", val_arg);
			}
			else {
				change->type = msgtype;
				msg_to_char(ch, "Custom message %d changed to type %s.\r\n", atoi(num_arg), type_names[msgtype]);
			}
		}
		else if (is_abbrev(type_arg, "message")) {
			if (change->msg) {
				free(change->msg);
			}
			delete_doubledollar(val_arg);
			change->msg = str_dup(val_arg);
			msg_to_char(ch, "Custom message %d changed to: %s\r\n", atoi(num_arg), val_arg);
		}
		else {
			msg_to_char(ch, "You can only change the type or message.\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: custom add <type> <message>\r\n");
		msg_to_char(ch, "Usage: custom change <number> <type | message> <value>\r\n");
		msg_to_char(ch, "Usage: custom remove <number | all>\r\n");
		msg_to_char(ch, "Available types:\r\n");
		for (iter = 0; *type_names[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %s\r\n", type_names[iter]);
		}
	}
}


/**
* OLC processor for extra descs, which are used on objs, room templates, and
* buildings.
*
* @param char_data *ch The player using OLC.
* @param char *argument The arguments the player entered.
* @param struct extra_descr_data **list A pointer to an exdesc list to modify.
*/
void olc_process_extra_desc(char_data *ch, char *argument, struct extra_descr_data **list) {
	void setup_extra_desc_editor(char_data *ch, struct extra_descr_data *ex);
	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	struct extra_descr_data *ex, *change, *find_ex, *temp;
	int num;
	
	half_chop(argument, arg1, arg2);
	num = atoi(arg2);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else if (is_abbrev(arg1, "add")) {
		if (!*arg2) {
			msg_to_char(ch, "You must specify the keywords for the extra description you're adding.\r\n");
		}
		else {
			// create
			CREATE(ex, struct extra_descr_data, 1);
			ex->keyword = str_dup(arg2);
			ex->description = NULL;
			ex->next = NULL;
			
			// insert
			if ((find_ex = *list)) {
				while (find_ex->next) {
					find_ex = find_ex->next;
				}
				find_ex->next = ex;
			}
			else {
				*list = ex;
			}
			
			// and edit
			setup_extra_desc_editor(ch, ex);
		}
	}
	else if (is_abbrev(arg1, "remove")) {
		if (!str_cmp(arg2, "all")) {
			while ((ex = *list)) {
				*list = ex->next;
				if (ex->keyword) {
					free(ex->keyword);
				}
				if (ex->description) {
					free(ex->description);
				}
				free(ex);
			}
			msg_to_char(ch, "You remove all extra descriptions.\r\n");
		}
		else if (!(ex = find_extra_desc_by_num(*list, num))) {
			msg_to_char(ch, "invalid extra desc number.\r\n");
		}
		else {
			if (ex->keyword) {
				free(ex->keyword);
			}
			if (ex->description) {
				free(ex->description);
			}
			REMOVE_FROM_LIST(ex, *list, next);
			free(ex);
			
			msg_to_char(ch, "You remove extra description %d.\r\n", num);
		}
	}
	else if (is_abbrev(arg1, "change")) {
		half_chop(arg2, num_arg, arg1);
		half_chop(arg1, type_arg, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg) {
			msg_to_char(ch, "Usage: extra change <number> <keywords | description> [value]\r\n");
			return;
		}
		
		// find which one to change
		if (!(change = find_extra_desc_by_num(*list, atoi(num_arg)))) {
			msg_to_char(ch, "Invalid extra desc number.\r\n");
		}
		else if (is_abbrev(type_arg, "keywords")) {
			if (!*val_arg) {
				msg_to_char(ch, "Change the keywords to what?\r\n");
			}
			else {
				if (change->keyword) {
					free(change->keyword);
				}
				change->keyword = str_dup(val_arg);
				msg_to_char(ch, "Extra description %d keywords changed to: %s\r\n", atoi(num_arg), val_arg);
			}
		}
		else if (is_abbrev(type_arg, "description")) {
			setup_extra_desc_editor(ch, change);
		}
		else {
			msg_to_char(ch, "You can only change the keywords or description.\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: extra add <keywords>\r\n");
		msg_to_char(ch, "Usage: extra change <number> <keywords | description> [value]\r\n");
		msg_to_char(ch, "Usage: extra remove <number | all>\r\n");
	}
}


/**
* OLC processor for icon set data.
*
* @param char_data *ch The player using OLC.
* @param char *argument The arguments the player entered.
* @param struct icon_data **list A pointer to an icon list to modify.
*/
void olc_process_icons(char_data *ch, char *argument, struct icon_data **list) {
	extern bool check_banner_color_string(char *str);
	void smart_copy_icons(struct icon_data **addto, struct icon_data *input);
	extern const char *icon_types[];
	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], arg4[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	char lbuf[MAX_INPUT_LENGTH];
	struct icon_data *icon, *change, *temp, *copyfrom;
	int iter, loc, num, findtype;
	any_vnum vnum;
	bool found;
	
	half_chop(argument, arg1, arg2);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which icon (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			free_icon_set(list);
			msg_to_char(ch, "You remove all icons.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid icon number.\r\n");
		}
		else {
			found = FALSE;
			for (icon = *list; icon && !found; icon = icon->next) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove icon %d.\r\n", atoi(arg2));
					REMOVE_FROM_LIST(icon, *list, next);
					icon->next = NULL;
					free_icon_set(&icon);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid icon number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		// arg1 == add
		// arg2 contains the rest of the string -- split it up now
		strcpy(buf, arg2);
		half_chop(buf, arg2, buf1);	// arg2: type
		half_chop(buf1, arg3, arg4);	// arg3: color code, arg4: icon
	
		if (!*arg2 || !*arg3 || !*arg4) {
			msg_to_char(ch, "Usage: icons add <type> <color code> <icon>\r\n");
		}
		else if ((loc = search_block(arg2, icon_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type.\r\n");
		}
		else if (strlen(arg3) != 2 || !check_banner_color_string(arg3)) {
			msg_to_char(ch, "You must specify a single color code, starting with \t&.\r\n");
		}
		else if (!validate_icon(arg4)) {
			msg_to_char(ch, "You must specify an icon that is 4 characters long, not counting color codes.\r\n");
		}
		else {
			CREATE(temp, struct icon_data, 1);
			temp->type = loc;
			temp->color = str_dup(arg3);
			temp->icon = str_dup(arg4);
			temp->next = NULL;
			
			if ((icon = *list) != NULL) {
				while (icon->next) {
					icon = icon->next;
				}
				icon->next = temp;
			}
			else {
				*list = temp;
			}

			sort_icon_set(list);
			strcpy(lbuf, show_color_codes(arg4));
			msg_to_char(ch, "You add %s: %s %s%s&0 %s\r\n", icon_types[loc], show_color_codes(arg3), arg3, arg4, lbuf);
		}
	}
	else if (is_abbrev(arg1, "copy")) {
		strcpy(buf, arg2);
		half_chop(buf, arg2, arg3);
		
		if (!*arg2 || !*arg3) {
			msg_to_char(ch, "Usage: icons copy <from type> <from vnum>\r\n");
		}
		else if ((findtype = find_olc_type(arg2)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", arg2);
		}
		else if (!isdigit(*arg3)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy icons from which %s?\r\n", buf);
		}
		else if ((vnum = atoi(arg3)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			copyfrom = NULL;
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			
			// OLC_x: copyable icons
			switch (findtype) {
				case OLC_CROP: {
					crop_data *crop;
					if ((crop = crop_proto(vnum))) {
						copyfrom = GET_CROP_ICONS(crop);
					}
					break;
				}
				case OLC_SECTOR: {
					sector_data *sect;
					if ((sect = sector_proto(vnum))) {
						copyfrom = GET_SECT_ICONS(sect);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy icons from %ss.\r\n", buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, arg3);
			}
			else {
				smart_copy_icons(list, copyfrom);
				msg_to_char(ch, "Icons copied from %s %d.\r\n", buf, vnum);
			}
		}
	}
	else if (is_abbrev(arg1, "change")) {
		half_chop(arg2, num_arg, arg1);
		half_chop(arg1, type_arg, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg || !*val_arg) {
			msg_to_char(ch, "Usage: icons change <number> <type | color | icon> <value>\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		for (icon = *list; icon && !change; icon = icon->next) {
			if (--num == 0) {
				change = icon;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid icon number.\r\n");
		}
		else if (is_abbrev(type_arg, "type")) {
			if ((loc = search_block(val_arg, icon_types, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid type.\r\n");
			}
			else {
				change->type = loc;
				msg_to_char(ch, "Icon %d changed to type %s.\r\n", atoi(num_arg), icon_types[loc]);
			}
		}
		else if (is_abbrev(type_arg, "color")) {
			if (strlen(val_arg) != 2 || !check_banner_color_string(val_arg)) {
				msg_to_char(ch, "You must specify a single color code, starting with \t&.\r\n");
			}
			else {
				if (change->color) {
					free(change->color);
				}
				change->color = str_dup(val_arg);
				msg_to_char(ch, "Icon %d changed to color code %s.\r\n", atoi(num_arg), show_color_codes(val_arg));
			}
		}
		else if (is_abbrev(type_arg, "icon")) {
			if (!validate_icon(val_arg)) {
				msg_to_char(ch, "You must specify an icon that is 4 characters long, not counting color codes.\r\n");
			}
			else {
				if (change->icon) {
					free(change->icon);
				}
				change->icon = str_dup(val_arg);
				msg_to_char(ch, "Icon %d changed to: %s%s&0 %s\r\n", atoi(num_arg), icon->color, val_arg, show_color_codes(val_arg));
			}
		}
		else {
			msg_to_char(ch, "You can only change the type, color, or icon.\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: icons add <type> <color code> <icon>\r\n");
		msg_to_char(ch, "Usage: icons copy <from type> <from vnum>\r\n");
		msg_to_char(ch, "Usage: icons change <number> <type | color | icon> <value>\r\n");
		msg_to_char(ch, "Usage: icons remove <number | all>\r\n");
		msg_to_char(ch, "Available types:\r\n");
		
		for (iter = 0; *icon_types[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %-24.24s%s", icon_types[iter], ((iter % 2) ? "\r\n" : ""));
		}
		if ((iter % 2) != 0) {
			msg_to_char(ch, "\r\n");
		}
	}
}


/**
* Parses out any -ability, -tech, or -ptech flags, as well as any exclusion
* code (a single letter). This is used with '.interaction add' to set
* restrictions.
*
* @param char_data *ch The person using the OLC editor.
* @param char *argument The remaining args from a '.interaction add'.
* @param struct interact_restriction **found_restrictions If we find any restrictions, we will pass them back on this parameter.
* @param char *found_exclusion If we find an exclusion code, we pass it back on this parameter.
* @return bool TRUE normally; FALSE if there was an error (message sent to character).
*/
bool parse_interaction_restrictions(char_data *ch, char *argument, struct interact_restriction **found_restrictions, char *found_exclusion) {
	void free_interaction_restrictions(struct interact_restriction **list);
	
	char arg[MAX_INPUT_LENGTH], *ptr = argument;
	struct interact_restriction *res;
	ability_data *abil;
	bool fail = FALSE;
	int num;
	
	// init
	*found_exclusion = (char)0;
	*found_restrictions = NULL;
	
	while (*ptr && !fail) {
		ptr = any_one_word(ptr, arg);
		
		if (strlen(arg) == 1 && isalpha(*arg)) {	// probably an exclusion code
			if (!*found_exclusion) {
				*found_exclusion = *arg;
			}
			else {	// duplicate
				msg_to_char(ch, "Error: Found two exclusion codes: '%c' and '%c'.\r\n", *found_exclusion, *arg);
				fail = TRUE;
			}
		}
		else if (is_abbrev(arg, "-ability")) {
			ptr = any_one_word(ptr, arg);
			if ((abil = find_ability(arg))) {	// valid restriction
				CREATE(res, struct interact_restriction, 1);
				res->type = INTERACT_RESTRICT_ABILITY;
				res->vnum = ABIL_VNUM(abil);
				LL_APPEND(*found_restrictions, res);
			}
			else {
				msg_to_char(ch, "Invalid ability '%s'.\r\n", arg);
				fail = TRUE;
			}
		}
		else if (is_abbrev(arg, "-technology")) {
			ptr = any_one_word(ptr, arg);
			if ((num = search_block(arg, techs, FALSE)) != NOTHING) {	// valid restriction
				CREATE(res, struct interact_restriction, 1);
				res->type = INTERACT_RESTRICT_TECH;
				res->vnum = num;
				LL_APPEND(*found_restrictions, res);
			}
			else {
				msg_to_char(ch, "Invalid tech '%s'.\r\n", arg);
				fail = TRUE;
			}
		}
		else if (is_abbrev(arg, "-ptech")) {
			ptr = any_one_word(ptr, arg);
			if ((num = search_block(arg, player_tech_types, FALSE)) != NOTHING) {	// valid restriction
				CREATE(res, struct interact_restriction, 1);
				res->type = INTERACT_RESTRICT_PTECH;
				res->vnum = num;
				LL_APPEND(*found_restrictions, res);
			}
			else {
				msg_to_char(ch, "Invalid ptech '%s'.\r\n", arg);
				fail = TRUE;
			}
		}
		else {
			msg_to_char(ch, "Unknown argument '%s'.\r\n", arg);
			fail = TRUE;
		}
	}
	
	// free any already found
	if (fail && *found_restrictions) {
		free_interaction_restrictions(found_restrictions);
		*found_restrictions = NULL;
	}
	
	return !fail;
}


/**
* OLC processor for interaction data, which is used on buildings, crops, mobs, and sectors.
*
* @param char_data *ch The player using OLC.
* @param char *argument The arguments the player entered.
* @param struct interaction_item **list A pointer to an interaction list to modify.
* @param int attach_type TYPE_MOB or TYPE_ROOM for interactions that attach to different types of things.
*/
void olc_process_interactions(char_data *ch, char *argument, struct interaction_item **list, int attach_type) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], arg4[MAX_INPUT_LENGTH], arg5[MAX_INPUT_LENGTH], arg6[MAX_INPUT_LENGTH];
	struct interaction_item *interact, *prev, *to_move, *temp, *a, *b, *a_next, *b_next, *copyfrom = NULL, *change;
	struct interact_restriction *found_restrictions = NULL;
	struct interaction_item iitem;
	int iter, loc, num, count, findtype;
	any_vnum vnum;
	double prc;
	bool found, up;
	char exc;
	
	half_chop(argument, arg1, arg2);
	
	if (is_abbrev(arg1, "copy")) {
		strcpy(buf, arg2);
		half_chop(buf, arg2, arg3);
		
		if (!*arg2 || !*arg3) {
			msg_to_char(ch, "Usage: interaction copy <from type> <from vnum>\r\n");
		}
		else if ((findtype = find_olc_type(arg2)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", arg2);
		}
		else if (!isdigit(*arg3)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy interactions from which %s?\r\n", buf);
		}
		else if ((vnum = atoi(arg3)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			
			// OLC_x: copyable interactions
			switch (findtype) {
				case OLC_BUILDING: {
					bld_data *bld = building_proto(vnum);
					if (bld) {
						copyfrom = GET_BLD_INTERACTIONS(bld);
					}
					break;
				}
				case OLC_CROP: {
					crop_data *crop;
					if ((crop = crop_proto(vnum))) {
						copyfrom = GET_CROP_INTERACTIONS(crop);
					}
					break;
				}
				case OLC_GLOBAL: {
					struct global_data *glb;
					if ((glb = global_proto(vnum))) {
						copyfrom = GET_GLOBAL_INTERACTIONS(glb);
					}
					break;
				}
				case OLC_MOBILE: {
					char_data *mob = mob_proto(vnum);
					if (mob) {
						copyfrom = mob->interactions;
					}
					break;
				}
				case OLC_OBJECT: {
					obj_data *obj = obj_proto(vnum);
					if (obj) {
						copyfrom = obj->interactions;
					}
					break;
				}
				case OLC_ROOM_TEMPLATE: {
					room_template *rmt = room_template_proto(vnum);
					if (rmt) {
						copyfrom = GET_RMT_INTERACTIONS(rmt);
					}
					break;
				}
				case OLC_SECTOR: {
					sector_data *sect;
					if ((sect = sector_proto(vnum))) {
						copyfrom = GET_SECT_INTERACTIONS(sect);
					}
					break;
				}
				case OLC_VEHICLE: {
					vehicle_data *veh;
					if ((veh = vehicle_proto(vnum))) {
						copyfrom = VEH_INTERACTIONS(veh);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy interactions from %ss.\r\n", buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, arg3);
			}
			else {
				smart_copy_interactions(list, copyfrom);
				msg_to_char(ch, "Interactions copied from %s %d.\r\n", buf, vnum);
			}
		}
	}
	else if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which interaction (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			while ((interact = *list)) {
				*list = interact->next;
				free(interact);
			}
			msg_to_char(ch, "You remove all interactions.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid interaction number.\r\n");
		}
		else {
			found = FALSE;
			for (interact = *list; interact && !found; interact = interact->next) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove %s: %dx %s %.2f%%\r\n", interact_types[interact->type], interact->quantity, (interact_vnum_types[interact->type] == TYPE_MOB ? skip_filler(get_mob_name_by_proto(interact->vnum)) : skip_filler(get_obj_name_by_proto(interact->vnum))), interact->percent);
					REMOVE_FROM_LIST(interact, *list, next);
					free(interact);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid interaction number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "move")) {
		strcpy(buf, arg2);
		half_chop(buf, arg2, arg3);
		num = atoi(arg2);
		up = is_abbrev(arg3, "up");
		
		if (!*arg2 || !*arg3) {
			msg_to_char(ch, "Usage: interaction move <number> <up | down>\r\n");
		}
		else if (!isdigit(*arg2) || num < 1) {
			msg_to_char(ch, "Invalid interaction number.\r\n");
		}
		else if (!is_abbrev(arg3, "up") && !is_abbrev(arg3, "down")) {
			msg_to_char(ch, "You must specify whether you're moving it up or down in the list.\r\n");
		}
		else if (up && num == 1) {
			msg_to_char(ch, "You can't move it up; it's already at the top of the list.\r\n");
		}
		else {
			// find the one to move
			to_move = prev = NULL;
			for (interact = *list; interact && !to_move; interact = interact->next) {
				if (--num == 0) {
					to_move = interact;
				}
				else {
					// store for next iteration
					prev = interact;
				}
			}
			
			if (!to_move) {
				msg_to_char(ch, "Invalid interaction number.\r\n");
			}
			else if (!up && !to_move->next) {
				msg_to_char(ch, "You can't move it down; it's already at the bottom of the list.\r\n");
			}
			else {
				// SUCCESS: "move" them by swapping data
				if (up) {
					a = prev;
					b = to_move;
				}
				else {
					a = to_move;
					b = to_move->next;
				}
				
				// store next pointers
				a_next = a->next;
				b_next = b->next;
				
				// swap data
				iitem = *a;
				*a = *b;
				*b = iitem;
				
				// restore next pointers
				a->next = a_next;
				b->next = b_next;
				
				// message: re-atoi(arg2) because we destroyed num finding our target
				msg_to_char(ch, "You move interaction %d %s.\r\n", atoi(arg2), (up ? "up" : "down"));
				
				// re-sort to keep types together
				sort_interactions(list);
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		// arg1 == add
		// arg2 contains the rest of the string -- split it up now
		strcpy(buf, arg2);
		half_chop(buf, arg2, buf1);	// arg2: type
		half_chop(buf1, arg3, buf);	// arg3: quantity
		half_chop(buf, arg4, buf1);	// arg4: vnum
		half_chop(buf1, arg5, arg6);	// arg5: percent, arg6: exclusion/restrictions
	
		num = atoi(arg3);
		vnum = atoi(arg4);
		prc = atof(arg5);
		
		if (!*arg2 || !*arg3 || !*arg4 || !*arg5 || !isdigit(*arg3) || !isdigit(*arg4) || (!isdigit(*arg5) && *arg5 != '.')) {
			msg_to_char(ch, "Usage: interaction add <type> <quantity> <vnum> <percent> [exclusion code | restrictions]\r\n");
		}
		else if ((loc = search_block(arg2, interact_types, FALSE)) == NOTHING || interact_attach_types[loc] != attach_type) {
			msg_to_char(ch, "Invalid type.\r\n");
		}
		else if (interact_vnum_types[loc] == TYPE_MOB && !mob_proto(vnum)) {
			msg_to_char(ch, "Invalid mob vnum %d.\r\n", vnum);
		}
		else if (interact_vnum_types[loc] == TYPE_OBJ && !obj_proto(vnum)) {
			msg_to_char(ch, "Invalid object vnum %d.\r\n", vnum);
		}
		else if (num < 1 || num >= 1000) {
			msg_to_char(ch, "You must choose a quantity between 1 and 1000.\r\n");
		}
		else if (prc < 0.01 || prc > 100.00) {
			msg_to_char(ch, "You must choose a percentage between 0.01 and 100.00.\r\n");
		}
		else if (!parse_interaction_restrictions(ch, arg6, &found_restrictions, &exc)) {
			// sends own message if it fails
		}
		else {
			CREATE(temp, struct interaction_item, 1);
			temp->type = loc;
			temp->vnum = vnum;
			temp->percent = prc;
			temp->quantity = num;
			temp->exclusion_code = exc;
			temp->restrictions = found_restrictions;
			temp->next = NULL;
			
			if ((interact = *list) != NULL) {
				while (interact->next) {
					interact = interact->next;
				}
				interact->next = temp;
			}
			else {
				*list = temp;
			}

			sort_interactions(list);
			msg_to_char(ch, "You add %s: %dx %s %.2f%%", interact_types[loc], num, (interact_vnum_types[loc] == TYPE_MOB ? skip_filler(get_mob_name_by_proto(vnum)) : skip_filler(get_obj_name_by_proto(vnum))), prc);
			if (exc) {
				msg_to_char(ch, " (%c)", exc);
			}
			if (found_restrictions) {
				msg_to_char(ch, " (requires: %s)", get_interaction_restriction_display(found_restrictions, TRUE));
			}
			msg_to_char(ch, "\r\n");
		}
	}
	else if (is_abbrev(arg1, "change")) {
		// arg1 == change
		// arg2 contains the rest of the string -- split it up now
		strcpy(buf, arg2);
		half_chop(buf, arg2, buf1);	// arg2: number
		half_chop(buf1, arg3, arg4);	// arg3: field, arg4: value
		
		// find which one to change
		if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid interaction number.\r\n");
			return;
		}
		change = NULL;
		for (interact = *list; interact && !change; interact = interact->next) {
			if (--num == 0) {
				change = interact;
				break;
			}
		}
		if (!change) {
			msg_to_char(ch, "Invalid interaction number.\r\n");
			return;
		}
		
		// ok now which field to change:
		if (is_abbrev(arg3, "type")) {
			if ((loc = search_block(arg4, interact_types, FALSE)) == NOTHING || interact_attach_types[loc] != attach_type) {
				msg_to_char(ch, "Invalid type.\r\n");
			}
			else {
				change->type = loc;
				msg_to_char(ch, "Interaction %d type changed to %s.\r\n", atoi(arg2), interact_types[loc]);
			}
		}
		else if (is_abbrev(arg3, "quantity") || is_abbrev(arg3, "amount")) {
			if ((num = atoi(arg4)) < 1 || num >= 1000) {
				msg_to_char(ch, "You must choose a quantity between 1 and 1000.\r\n");
			}
			else {
				change->quantity = num;
				msg_to_char(ch, "Interaction %d quantity changed to %d.\r\n", atoi(arg2), num);
			}
		}
		else if (is_abbrev(arg3, "vnum")) {
			vnum = atoi(arg4);
			if (!*arg4) {
				msg_to_char(ch, "Change it to which vnum?\r\n");
			}
			else if (interact_vnum_types[change->type] == TYPE_MOB && !mob_proto(vnum)) {
				msg_to_char(ch, "Invalid mob vnum %d.\r\n", vnum);
			}
			else if (interact_vnum_types[change->type] == TYPE_OBJ && !obj_proto(vnum)) {
				msg_to_char(ch, "Invalid object vnum %d.\r\n", vnum);
			}
			else {
				change->vnum = vnum;
				msg_to_char(ch, "Interaction %d vnum changed to [%d] %s.\r\n", atoi(arg2), vnum, (interact_vnum_types[change->type] == TYPE_MOB) ? get_mob_name_by_proto(vnum) : get_obj_name_by_proto(vnum));
			}
		}
		else if (is_abbrev(arg3, "percent")) {
			prc = atof(arg4);
			if (!*arg4) {
				msg_to_char(ch, "Change the percent to what?\r\n");
			}
			else if (prc < 0.01 || prc > 100.00) {
				msg_to_char(ch, "You must choose a percentage between 0.01 and 100.00.\r\n");
			}
			else {
				change->percent = prc;
				msg_to_char(ch, "Interaction %d percent changed to %.2f.\r\n", atoi(arg2), prc);
			}
		}
		else if (is_abbrev(arg3, "exclusion")) {
			if (!*arg4) {
				msg_to_char(ch, "Change the exclusion code to what (or 'none')?\r\n");
			}
			else if (!str_cmp(arg4, "none")) {
				change->exclusion_code = 0;
				msg_to_char(ch, "Interaction %d exclusion code removed.\r\n", atoi(arg2));
			}
			else if (!isalpha(*arg4)) {
				msg_to_char(ch, "Invalid exclusion code (must be a letter, 'none').\r\n");
			}
			else {
				change->exclusion_code = *arg4;
				msg_to_char(ch, "Interaction %d exclusion code changed to '%c'.\r\n", atoi(arg2), *arg4);
			}
		}
		else {
			msg_to_char(ch, "Invalid field. You can change: type, quantity, vnum, percent, exclusion\r\n");
		}
		
		sort_interactions(list);
	}
	else {
		msg_to_char(ch, "Usage: interaction add <type> <quantity> <vnum> <percent> [exclusion code | restrictions]\r\n");
		msg_to_char(ch, "Usage: interaction change <number> <field> <value>\r\n");
		msg_to_char(ch, "Usage: interaction copy <from type> <from vnum>\r\n");
		msg_to_char(ch, "Usage: interaction remove <number | all>\r\n");
		msg_to_char(ch, "Usage: interaction move <number> <up | down>\r\n");
		msg_to_char(ch, "Available types:\r\n");
		
		for (count = 0, iter = 0; *interact_types[iter] != '\n'; ++iter) {
			if (interact_attach_types[iter] == attach_type) {
				msg_to_char(ch, " %-24.24s%s", interact_types[iter], ((count++ % 2) ? "\r\n" : ""));
			}
		}
		if ((count % 2) != 0) {
			msg_to_char(ch, "\r\n");
		}
	}
}


/**
* OLC processor for resource data, which is used on augments and crafts.
*
* @param char_data *ch The player using OLC.
* @param char *argument The arguments the player entered.
* @param struct resource_data **list A pointer to a resource list to modify.
*/
void olc_process_resources(char_data *ch, char *argument, struct resource_data **list) {	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	char arg4[MAX_INPUT_LENGTH], arg5[MAX_INPUT_LENGTH];
	struct resource_data *res, *next_res, *prev_res, *prev_prev, *change, *temp;
	int num, type, misc;
	any_vnum vnum;
	bool found;
	
	// arg1 arg2 arg3
	half_chop(argument, arg1, buf);
	half_chop(buf, arg2, buf1);
	half_chop(buf1, arg3, arg4);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which resource (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			free_resource_list(*list);
			*list = NULL;
			msg_to_char(ch, "All resources removed.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid resource number.\r\n");
		}
		else {
			found = FALSE;
			for (res = *list; res && !found; res = next_res) {
				next_res = res->next;
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the %s.\r\n", get_resource_name(res));
					REMOVE_FROM_LIST(res, *list, next);
					free(res);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid resource number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		strcpy(buf, arg4);	// split out one more arg
		half_chop(buf, arg4, arg5);
		
		// arg2 is "type"
		num = atoi(arg3);
		vnum = atoi(arg4);	// not necessarily a number though
		// arg5 may be flags
		
		if (!*arg2 || !*arg3 || !isdigit(*arg3)) {
			msg_to_char(ch, "Usage: resource add <type> <quantity> <vnum/name> [flags, for components only]\r\n");
		}
		else if ((type = search_block(arg2, resource_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Unknown resource type '%s'.\r\n", arg2);
		}
		else if (num < 1 || num > 10000) {
			msg_to_char(ch, "You must specify a quantity between 1 and 10000, %d given.\r\n", num);
		}
		else {
			misc = 0;
			
			// RES_x: validate arg4/arg5 based on type
			switch (type) {
				case RES_OBJECT: {
					if (!*arg4 || !isdigit(*arg4)) {
						msg_to_char(ch, "Usage: resource add object <quantity> <vnum>\r\n");
						return;
					}
					if (!obj_proto(vnum)) {
						msg_to_char(ch, "There is no such object vnum %d.\r\n", vnum);
						return;
					}
					break;
				}
				case RES_COMPONENT: {
					if (!*arg4) {
						msg_to_char(ch, "Usage: resource add component <quantity> <type> [flags]\r\n");
						return;
					}
					if ((vnum = search_block(arg4, component_types, FALSE)) == NOTHING) {
						msg_to_char(ch, "Unknown component type '%s'.\r\n", arg4);
						return;
					}
					
					if (*arg5) {
						misc = olc_process_flag(ch, arg5, "component", "resource add component <quantity> <type>", component_flags, NOBITS);
					}
					break;
				}
				case RES_LIQUID: {
					if (!*arg4) {
						msg_to_char(ch, "Usage: resource add liquid <units> <vnum>\r\n");
						return;
					}
					if (!find_generic(vnum, GENERIC_LIQUID)) {
						msg_to_char(ch, "Invalid liquid generic vnum %d.\r\n", vnum);
						return;
					}
					break;
				}
				case RES_COINS: {
					vnum = OTHER_COIN;
					break;
				}
				case RES_POOL: {
					if (!*arg4) {
						msg_to_char(ch, "Usage: resource add pool <amount> <type>\r\n");
						return;
					}
					if ((vnum = search_block(arg4, pool_types, FALSE)) == NOTHING) {
						msg_to_char(ch, "Unknown pool type '%s'.\r\n", arg4);
						return;
					}
					break;
				}
				case RES_ACTION: {
					if (!*arg4 || !isdigit(*arg4)) {
						msg_to_char(ch, "Usage: resource add action <amount> <generic action vnum>\r\n");
						return;
					}
					if (!find_generic(vnum, GENERIC_ACTION)) {
						msg_to_char(ch, "Invalid generic action vnum '%s'.\r\n", arg4);
						return;
					}
					break;
				}
				case RES_CURRENCY: {
					if (!*arg4 || !isdigit(*arg4)) {
						msg_to_char(ch, "Usage: resource add currency <quantity> <vnum>\r\n");
						return;
					}
					if (!find_generic(vnum, GENERIC_CURRENCY)) {
						msg_to_char(ch, "There is no such generic currency vnum %d.\r\n", vnum);
						return;
					}
					break;
				}
			}
			
			CREATE(res, struct resource_data, 1);
			res->type = type;
			res->vnum = vnum;
			res->amount = num;
			res->misc = misc;
			
			// append to end
			LL_APPEND(*list, res);
			
			msg_to_char(ch, "You add %s %s resource requirement.\r\n", AN(get_resource_name(res)), get_resource_name(res));
		}
	}
	else if (is_abbrev(arg1, "move")) {
		bool up;
		
		num = atoi(arg2);
		up = is_abbrev(arg3, "up");
		
		if (!*arg2 || !*arg3) {
			msg_to_char(ch, "Usage: resource move <number> <up | down>\r\n");
		}
		else if (!isdigit(*arg2) || num < 1) {
			msg_to_char(ch, "Invalid resource number.\r\n");
		}
		else if (!is_abbrev(arg3, "up") && !is_abbrev(arg3, "down")) {
			msg_to_char(ch, "You must specify whether you're moving it up or down in the list.\r\n");
		}
		else if (up && num == 1) {
			msg_to_char(ch, "You can't move it up; it's already at the top of the list.\r\n");
		}
		else {
			prev_res = prev_prev = NULL;
			found = FALSE;
			for (res = *list; res && !found && num > 0; res = res->next) {
				if (--num == 0) {
					if (up) {
						found = TRUE;
						if (prev_prev) {
							prev_prev->next = res;
							prev_res->next = res->next;
							res->next = prev_res;
						}
						else {
							// must be head of list
							prev_res->next = res->next;
							res->next = prev_res;
							*list = res;
						}
					}
					else {	// down
						if (res->next) {
							found = TRUE;
							temp = res->next;
							if (prev_res) {
								prev_res->next = temp;
								res->next = temp->next;
								temp->next = res;
							}
							else {
								// was at start
								res->next = temp->next;
								temp->next = res;
								*list = temp;
							}
						}
					}
					
					if (found) {
						msg_to_char(ch, "You move resource %d (%s) %s.\r\n", atoi(arg2), get_resource_name(res), (up ? "up" : "down"));
					}
				}
				
				// rotate back
				prev_prev = prev_res;
				prev_res = res;
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid resource number to move.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "change")) {
		if (!*arg2 || !*arg3 || !*arg4 || !isdigit(*arg2)) {
			msg_to_char(ch, "Usage: resource change <number> <quantity | vnum | name | flags> <value>\r\n");
			return;
		}
		
		vnum = atoi(arg4);	// may be used as quantity or something else instead (if a number)
		
		// verify valid pos
		num = atoi(arg2);
		change = NULL;
		for (res = *list; res && !change; res = res->next) {
			if (--num == 0) {
				change = res;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid resource number.\r\n");
		}
		else if (is_abbrev(arg3, "quantity") || is_abbrev(arg3, "amount")) {
			if (vnum < 1 || vnum > 10000) {
				msg_to_char(ch, "You must specify a quantity between 1 and 10000.\r\n");
			}
			else {
				change->amount = vnum;
				msg_to_char(ch, "You change resource %d's quantity to %d.\r\n", atoi(arg2), vnum);
			}
		}
		else if (is_abbrev(arg3, "vnum")) {
			// RES_x: ones that support change vnum
			switch (change->type) {
				case RES_OBJECT: {
					if (!obj_proto(vnum)) {
						msg_to_char(ch, "There is no such object vnum %d.\r\n", vnum);
						return;
					}
					break;
				}
				case RES_LIQUID: {
					if (!find_generic(vnum, GENERIC_LIQUID)) {
						msg_to_char(ch, "Invalid liquid generic vnum %d.\r\n", vnum);
						return;
					}
					break;
				}
				case RES_ACTION: {
					if (!find_generic(vnum, GENERIC_ACTION)) {
						msg_to_char(ch, "Invalid generic action vnum %d.\r\n", vnum);
						return;
					}
					break;
				}
				case RES_CURRENCY: {
					if (!find_generic(vnum, GENERIC_CURRENCY)) {
						msg_to_char(ch, "Invalid generic currency vnum %d.\r\n", vnum);
						return;
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't change the vnum on a resource of that type.\r\n");
					return;
				}
			}
			
			// ok
			change->vnum = vnum;
			msg_to_char(ch, "You change resource %d's vnum to [%d] %s.\r\n", atoi(arg2), vnum, get_resource_name(change));
		}
		else if (is_abbrev(arg3, "name") || is_abbrev(arg3, "component") || is_abbrev(arg3, "pool")) {
			// RES_x: some resource types support "change name"
			switch (change->type) {
				case RES_COMPONENT: {
					if ((vnum = search_block(arg4, component_types, FALSE)) == NOTHING) {
						msg_to_char(ch, "Unknown component type '%s'.\r\n", arg4);
						return;
					}
					
					change->vnum = vnum;
					
					msg_to_char(ch, "You change resource %d's component to %s.\r\n", atoi(arg2), component_string(vnum, change->misc));
					break;
				}
				case RES_POOL: {
					if ((vnum = search_block(arg4, pool_types, FALSE)) == NOTHING) {
						msg_to_char(ch, "Unknown pool type '%s'.\r\n", arg4);
						return;
					}
					
					change->vnum = vnum;
					msg_to_char(ch, "You change resource %d's pool to %s.\r\n", atoi(arg2), pool_types[vnum]);
					break;
				}
				default: {
					msg_to_char(ch, "You can't change the name on a %s resource.\r\n", resource_types[change->type]);
					return;
				}
			}
		}
		else if (is_abbrev(arg3, "flags")) {
			if (change->type != RES_COMPONENT) {
				msg_to_char(ch, "You can't change the vnum on a resource that isn't a component.\r\n");
			}
			else {
				misc = olc_process_flag(ch, arg4, "component", "resource change <number> flags <value>", component_flags, change->misc);
				
				// if there are no changes, they should have gotten an error message
				if (misc != change->misc) {
					change->misc = misc;
					msg_to_char(ch, "You change resource %d's component to %s.\r\n", atoi(arg2), component_string(change->vnum, misc));
				}
			}
		}
		else {
			msg_to_char(ch, "Usage: resource change <number> <quantity | vnum | name | flags> <value>\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: resource add <type> <quantity> <vnum/name> [flags, for component only]\r\n");
		msg_to_char(ch, "Usage: resource change <number> <quantity | vnum> <value>\r\n");
		msg_to_char(ch, "Usage: resource remove <number | all>\r\n");
		msg_to_char(ch, "Usage: resource move <number> <up | down>\r\n");
	}
}


/**
* OLC processor for spawn data, which is used on buildings, crops, and sectors.
*
* @param char_data *ch The player using OLC.
* @param char *argument The arguments the player entered.
* @param struct spawn_info **list A pointer to a spawn info list to modify.
*/
void olc_process_spawns(char_data *ch, char *argument, struct spawn_info **list) {
	extern const char *spawn_flags[];

	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	char *flagarg, *tmp;
	int loc, num, iter, count, findtype;
	struct spawn_info *spawn, *change, *temp, *copyfrom = NULL;
	vehicle_data *veh;
	sector_data *sect;
	any_vnum vnum;
	double prc;
	bool found;
	
	// arg1 arg2
	half_chop(argument, arg1, arg2);
	
	if (is_abbrev(arg1, "copy")) {
		strcpy(buf, arg2);
		half_chop(buf, arg2, arg3);
		
		if (!*arg2 || !*arg3) {
			msg_to_char(ch, "Usage: spawn copy <from type> <from vnum>\r\n");
		}
		else if ((findtype = find_olc_type(arg2)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", arg2);
		}
		else if (!isdigit(*arg3)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy spawns from which %s?\r\n", buf);
		}
		else if ((vnum = atoi(arg3)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			
			switch (findtype) {
				case OLC_BUILDING: {
					bld_data *bld = building_proto(vnum);
					if (bld) {
						copyfrom = GET_BLD_SPAWNS(bld);
					}
					break;
				}
				case OLC_CROP: {
					crop_data *crop = crop_proto(vnum);
					if (crop) {
						copyfrom = GET_CROP_SPAWNS(crop);
					}
					break;
				}
				case OLC_SECTOR: {
					if ((sect = sector_proto(vnum))) {
						copyfrom = GET_SECT_SPAWNS(sect);
					}
					break;
				}
				case OLC_VEHICLE: {
					if ((veh = vehicle_proto(vnum))) {
						copyfrom = VEH_SPAWNS(veh);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy spawns from %ss.\r\n", buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, arg3);
			}
			else {
				smart_copy_spawns(list, copyfrom);
				msg_to_char(ch, "Spawns copied from %s %d.\r\n", buf, vnum);
			}
		}
	}
	else if (is_abbrev(arg1, "change")) {
		half_chop(arg2, num_arg, arg1);
		half_chop(arg1, type_arg, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg || !*val_arg) {
			msg_to_char(ch, "Usage: spawn change <number> <vnum | percent | flags> <value>\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		for (spawn = *list; spawn && !change; spawn = spawn->next) {
			if (--num == 0) {
				change = spawn;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid spawn number.\r\n");
		}
		else if (is_abbrev(type_arg, "vnum")) {
			if (!isdigit(*val_arg) || (vnum = atoi(val_arg)) < 0) {
				msg_to_char(ch, "Invalid vnum '%s'.\r\n", val_arg);
			}
			else if (!mob_proto(vnum)) {
				msg_to_char(ch, "Invalid mobile vnum '%s'.\r\n", val_arg);
			}
			else {
				change->vnum = vnum;
				msg_to_char(ch, "Spawn %d changed to vnum %d (%s).\r\n", atoi(num_arg), vnum, get_mob_name_by_proto(vnum));
			}
		}
		else if (is_abbrev(type_arg, "percent")) {
			prc = atof(val_arg);
			
			if (prc < .01 || prc > 100.00) {
				msg_to_char(ch, "Percentage must be between .01 and 100; '%s' given.\r\n", val_arg);
			}
			else {
				change->percent = prc;
				msg_to_char(ch, "Spawn %d changed to %.2f percent.\r\n", atoi(num_arg), prc);
			}
		}
		else if (is_abbrev(type_arg, "flags")) {
			spawn->flags = olc_process_flag(ch, val_arg, "spawn", "spawn change flags", spawn_flags, spawn->flags);
		}
		else {
			msg_to_char(ch, "You can only change the vnum, percent, or flags.\r\n");
		}
	}
	else if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which spawn (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			// remove all spawns
			while ((spawn = *list)) {
				*list = spawn->next;
				free(spawn);
			}
			msg_to_char(ch, "You remove all spawns.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid spawn number.\r\n");
		}
		else {
			found = FALSE;
			for (spawn = *list; spawn && !found; spawn = spawn->next) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the spawn info for %s.\r\n", get_mob_name_by_proto(spawn->vnum));
					
					REMOVE_FROM_LIST(spawn, *list, next);
					free(spawn);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid spawn number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		tmp = any_one_arg(arg2, arg);	// arg = vnum
		flagarg = any_one_arg(tmp, arg3);	// arg3 = percent, flagarg = flags
		
		// flagarg MAY contain space-separated flags
		
		if (!*arg || !*arg3) {
			msg_to_char(ch, "Usage: spawn add <vnum> <percent> [flags]\r\n");
		}
		else if (!isdigit(*arg) || (vnum = atoi(arg)) < 0 || !mob_proto(vnum)) {
			msg_to_char(ch, "Invalid mob vnum '%s'.\r\n", arg);
		}
		else if ((prc = atof(arg3)) < .01 || prc > 100.00) {
			msg_to_char(ch, "Percentage must be between .01 and 100; '%s' given.\r\n", arg3);
		}
		else {
			CREATE(spawn, struct spawn_info, 1);
			
			spawn->vnum = vnum;
			spawn->percent = prc;
			spawn->flags = 0;
			
			spawn->next = NULL;
			
			// append to end
			if ((temp = *list) != NULL) {
				while (temp->next) {
					temp = temp->next;
				}
				temp->next = spawn;
			}
			else {
				*list = spawn;
			}
			
			// process flags as space-separated string
			while (*flagarg) {
				flagarg = any_one_arg(flagarg, arg);
				
				if ((loc = search_block(arg, spawn_flags, FALSE)) != NOTHING) {
					spawn->flags |= BIT(loc);
				}
			}
			
			sprintbit(spawn->flags, spawn_flags, buf1, TRUE);
			msg_to_char(ch, "You add spawn for %s (%d) at %.2f%% with flags: %s\r\n", get_mob_name_by_proto(vnum), vnum, prc, buf1);
		}
	}
	else if (is_abbrev(arg1, "list")) {
		count = 0;
		*buf = '\0';
		for (spawn = *list; spawn; spawn = spawn->next) {
			sprintbit(spawn->flags, spawn_flags, buf1, TRUE);
			sprintf(buf + strlen(buf), " %d. %s (%d) %.2f%% %s\r\n", ++count, skip_filler(get_mob_name_by_proto(spawn->vnum)), spawn->vnum, spawn->percent, buf1);
		}
		
		page_string(ch->desc, buf, TRUE);
	}
	else {
		msg_to_char(ch, "Usage: spawn add <vnum> <percent> [flags]\r\n");
		msg_to_char(ch, "Usage: spawn copy <from type> <from vnum>\r\n");
		msg_to_char(ch, "Usage: spawn change <number> <vnum | percent | flags> <value>\r\n");
		msg_to_char(ch, "Usage: spawn remove <number | all>\r\n");
		msg_to_char(ch, "Usage: spawn list\r\n");
		msg_to_char(ch, "Available flags:\r\n");
		
		for (iter = 0; *spawn_flags[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %-24.24s%s", spawn_flags[iter], ((iter % 2) ? "\r\n" : ""));
		}
		if ((iter % 2) != 0) {
			msg_to_char(ch, "\r\n");
		}
	}
}


/**
* OLC processor for script data, which is used on mobs and objs.
*
* @param char_data *ch The player using OLC.
* @param char *argument The arguments the player entered.
* @param struct trig_proto_list **list A pointer to a proto list to modify.
* @param int trigger_attach x_TRIGGER const (e.g. MOB_TRIGGER) to prevent wrong-type
*/
void olc_process_script(char_data *ch, char *argument, struct trig_proto_list **list, int trigger_attach) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], *tmp;
	struct trig_proto_list *tpl, *temp, *last, *copyfrom = NULL;
	char lbuf[MAX_STRING_LENGTH];
	int num, pos, findtype;
	any_vnum vnum;
	trig_data *trig;
	bool found;
	
	// arg1 arg2
	half_chop(argument, arg1, arg2);
	
	if (is_abbrev(arg1, "copy")) {
		strcpy(buf, arg2);
		half_chop(buf, arg2, arg3);
		
		if (!*arg2 || !*arg3) {
			msg_to_char(ch, "Usage: script copy <from type> <from vnum>\r\n");
		}
		else if ((findtype = find_olc_type(arg2)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", arg2);
		}
		else if (!isdigit(*arg3)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy scripts from which %s?\r\n", buf);
		}
		else if ((vnum = atoi(arg3)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			
			switch (findtype) {
				case OLC_ADVENTURE: {
					adv_data *adv = adventure_proto(vnum);
					if (adv && trigger_attach == WLD_TRIGGER) {
						copyfrom = GET_ADV_SCRIPTS(adv);
					}
					break;
				}
				case OLC_MOBILE: {
					char_data *mob = mob_proto(vnum);
					if (mob && trigger_attach == MOB_TRIGGER) {
						copyfrom = mob->proto_script;
					}
					break;
				}
				case OLC_OBJECT: {
					obj_data *obj = obj_proto(vnum);
					if (obj && trigger_attach == OBJ_TRIGGER) {
						copyfrom = obj->proto_script;
					}
					break;
				}
				case OLC_ROOM_TEMPLATE: {
					room_template *rmt = room_template_proto(vnum);
					if (rmt && trigger_attach == WLD_TRIGGER) {
						copyfrom = GET_RMT_SCRIPTS(rmt);
					}
					break;
				}
				case OLC_VEHICLE: {
					vehicle_data *veh = vehicle_proto(vnum);
					if (veh && trigger_attach == VEH_TRIGGER) {
						copyfrom = veh->proto_script;
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy scripts from %ss.\r\n", buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, arg3);
			}
			else {
				smart_copy_scripts(list, copyfrom);
				msg_to_char(ch, "Scripts copied from %s %d.\r\n", buf, vnum);
			}
		}
	}
	else if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which script (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			while ((tpl = *list)) {
				*list = tpl->next;
				free(tpl);
			}
			msg_to_char(ch, "You remove all scripts.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid script number.\r\n");
		}
		else {
			found = FALSE;
			for (tpl = *list; tpl && !found; tpl = tpl->next) {
				if (--num == 0) {
					found = TRUE;
					
					if ((trig = real_trigger(tpl->vnum))) {
						strcpy(lbuf, GET_TRIG_NAME(trig));
					}
					else {
						strcpy(lbuf, "UNKNOWN");
					}
					msg_to_char(ch, "You remove script [%d] %s.\r\n", tpl->vnum, lbuf);
					
					REMOVE_FROM_LIST(tpl, *list, next);
					free(tpl);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid script number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		tmp = any_one_arg(arg2, arg);	// arg = position (or vnum)
		any_one_arg(tmp, arg3);	// arg3 == vnum (unless empty)
		
		if (*arg3) {
			vnum = atoi(arg3);
			pos = atoi(arg);
		}
		else {
			vnum = atoi(arg);
			pos = -1;
		}
		
		if (!*arg || !isdigit(*arg) || (*arg3 && !isdigit(*arg3))) {
			msg_to_char(ch, "Usage: script add [position] <vnum>\r\n");
		}
		else if (!real_trigger(vnum)) {
			msg_to_char(ch, "Invalid trigger vnum '%s'.\r\n", (*arg3 ? arg3 : arg));
		}
		else if (real_trigger(vnum)->attach_type != trigger_attach) {
			msg_to_char(ch, "You may not attach that type of trigger.\r\n");
		}
		else {
			CREATE(tpl, struct trig_proto_list, 1);
			tpl->vnum = vnum;
			tpl->next = NULL;
			
			// find where to insert:
			if (pos == -1) {
				// end
				if ((temp = *list)) {
					while (temp->next) {
						temp = temp->next;
					}
					temp->next = tpl;
				}
				else {
					*list = tpl;
				}
			}
			else if (pos <= 1 || !*list) {
				// start
				tpl->next = *list;
				*list = tpl;
			}
			else {
				// find pos
				found = FALSE;
				last = NULL;
				for (temp = *list; temp && !found; temp = temp->next) {
					if (--pos == 1) {
						tpl->next = temp->next;
						temp->next = tpl;
						found = TRUE;
						break;
					}
					last = temp;
				}
				
				if (!found) {
					if (last) {
						last->next = tpl;
					}
					else {
						*list = tpl;
					}
				}
			}
			
			msg_to_char(ch, "You add script [%d] %s\r\n", vnum, GET_TRIG_NAME(real_trigger(vnum)));
		}
	}
	else {
		msg_to_char(ch, "Usage: script add [position] <vnum>\r\n");
		msg_to_char(ch, "Usage: script copy <from type> <from vnum>\r\n");
		msg_to_char(ch, "Usage: script remove <number | all>\r\n");
	}
}


/**
* Creates a copy of an icon list.
*
* @param struct icon_data **addto A place to smart-copy items to.
* @param struct icon_data *input A pointer to the start of the list to copy.
*/
void smart_copy_icons(struct icon_data **addto, struct icon_data *input) {
	struct icon_data *icon, *find, *new_icon, *end;
	bool found;
	
	// find end of new list (end may be NULL)
	if ((end = *addto)) {
		while (end->next) {
			end = end->next;
		}
	}
	
	// copy icons in order
	LL_FOREACH(input, icon) {
		// see if an identical icon is in the addto list
		found = FALSE;
		for (find = *addto; find && !found; find = find->next) {
			if (find->type == icon->type && !strcmp(find->color, icon->color) && !strcmp(find->icon, icon->icon)) {
				found = TRUE;
			}
		}
		
		// don't duplicate
		if (found) {
			continue;
		}
		
		CREATE(new_icon, struct icon_data, 1);
		*new_icon = *icon;
		new_icon->icon = str_dup(NULLSAFE(icon->icon));
		new_icon->color = str_dup(NULLSAFE(icon->color));
		new_icon->next = NULL;
		
		LL_APPEND(*addto, new_icon);
	}
	
	sort_icon_set(addto);
}


/**
* Creates a copy of an interaction list.
*
* @param struct interaction_item **addto A place to smart-copy items to.
* @param struct interaction_item *input A pointer to the start of the list to copy.
*/
void smart_copy_interactions(struct interaction_item **addto, struct interaction_item *input) {
	struct interaction_item *interact, *find, *new_interact, *end;
	bool found;
	
	// find end of new list (end may be NULL)
	if ((end = *addto)) {
		while (end->next) {
			end = end->next;
		}
	}
	
	// copy interactions in order
	for (interact = input; interact; interact = interact->next) {
		// see if an identical interaction is in the addto list
		found = FALSE;
		for (find = *addto; find && !found; find = find->next) {
			if (find->type == interact->type && find->vnum == interact->vnum && find->percent == interact->percent && find->quantity == interact->quantity && find->exclusion_code == interact->exclusion_code) {
				found = TRUE;
			}
		}
		
		// don't duplicate
		if (found) {
			continue;
		}
		
		CREATE(new_interact, struct interaction_item, 1);
		*new_interact = *interact;
		new_interact->restrictions = copy_interaction_restrictions(interact->restrictions);
		new_interact->next = NULL;
		
		// preserve order
		if (end) {
			end->next = new_interact;
		}
		else {
			*addto = new_interact;
		}
		
		end = new_interact;
	}
			
	// ensure these are sorted
	sort_interactions(addto);
}


/**
* Copies entries from one list into another, only if they are not already in
* the to_list.
*
* @param struct req_data **to_list A pointer to the destination list.
* @param struct req_data *from_list The list to copy from.
*/
void smart_copy_requirements(struct req_data **to_list, struct req_data *from_list) {
	struct req_data *iter, *search, *req;
	bool found;
	
	LL_FOREACH(from_list, iter) {
		// ensure not already in list
		found = FALSE;
		LL_FOREACH(*to_list, search) {
			if (search->type == iter->type && search->vnum == iter->vnum && search->misc == iter->misc && search->group == iter->group && search->needed == iter->needed) {
				found = TRUE;
				break;
			}
		}
		
		// add it
		if (!found) {
			CREATE(req, struct req_data, 1);
			*req = *iter;
			req->next = NULL;
			LL_APPEND(*to_list, req);
		}
	}
}


/**
* Creates a copy of a script list.
*
* @param struct trig_proto_list **addto A place to smart-copy items to.
* @param struct trig_proto_list *input A pointer to the start of the list to copy.
*/
void smart_copy_scripts(struct trig_proto_list **addto, struct trig_proto_list *input) {
	struct trig_proto_list *script, *find, *new_script, *end;
	bool found;
	
	// find end of new list (end may be NULL)
	if ((end = *addto)) {
		while (end->next) {
			end = end->next;
		}
	}
	
	// copy scripts in order
	for (script = input; script; script = script->next) {
		// see if an identical script is in the addto list
		found = FALSE;
		for (find = *addto; find && !found; find = find->next) {
			if (find->vnum == script->vnum) {
				found = TRUE;
			}
		}
		
		// don't duplicate
		if (found) {
			continue;
		}
		
		CREATE(new_script, struct trig_proto_list, 1);
		*new_script = *script;
		new_script->next = NULL;
		
		// preserve order
		if (end) {
			end->next = new_script;
		}
		else {
			*addto = new_script;
		}
		
		end = new_script;
	}
}


/**
* Creates a copy of a spawn list.
*
* @param struct spawn_info **addto A place to smart-copy items to.
* @param struct spawn_info *input A pointer to the start of the list to copy.
*/
void smart_copy_spawns(struct spawn_info **addto, struct spawn_info *input) {
	struct spawn_info *spawn, *find, *new_spawn, *end;
	bool found;
	
	// find end of new list (end may be NULL)
	if ((end = *addto)) {
		while (end->next) {
			end = end->next;
		}
	}
	
	// copy spawns in order
	for (spawn = input; spawn; spawn = spawn->next) {
		// see if an identical spawn is in the addto list
		found = FALSE;
		for (find = *addto; find && !found; find = find->next) {
			if (find->vnum == spawn->vnum && find->percent == spawn->percent && find->flags == spawn->flags) {
				found = TRUE;
			}
		}
		
		// don't duplicate
		if (found) {
			continue;
		}
		
		CREATE(new_spawn, struct spawn_info, 1);
		*new_spawn = *spawn;
		new_spawn->next = NULL;
		
		// preserve order
		if (end) {
			end->next = new_spawn;
		}
		else {
			*addto = new_spawn;
		}
		
		end = new_spawn;
	}
}


/**
* Creates a copy of a spawn list.
*
* @param struct adventure_spawn **addto A place to smart-copy items to.
* @param struct adventure_spawn *input A pointer to the start of the list to copy.
*/
void smart_copy_template_spawns(struct adventure_spawn **addto, struct adventure_spawn *input) {
	struct adventure_spawn *spawn, *find, *new_spawn, *end;
	bool found;
	
	// find end of new list (end may be NULL)
	if ((end = *addto)) {
		while (end->next) {
			end = end->next;
		}
	}
	
	// copy spawns in order
	for (spawn = input; spawn; spawn = spawn->next) {
		// see if an identical spawn is in the addto list
		found = FALSE;
		for (find = *addto; find && !found; find = find->next) {
			if (find->type == spawn->type && find->vnum == spawn->vnum && find->percent == spawn->percent && find->limit == spawn->limit) {
				found = TRUE;
			}
		}
		
		// don't duplicate
		if (found) {
			continue;
		}
		
		CREATE(new_spawn, struct adventure_spawn, 1);
		*new_spawn = *spawn;
		new_spawn->next = NULL;
		
		// preserve order
		if (end) {
			end->next = new_spawn;
		}
		else {
			*addto = new_spawn;
		}
		
		end = new_spawn;
	}
}


/**
* Validates an icon (length).
*
* @param char *icon The input icon.
* @return bool TRUE if the icon is ok; FALSE if not.
*/
bool validate_icon(char *icon) {
	if (!*icon) {
		return FALSE;
	}
	else if ((strlen(icon) - count_icon_codes(icon) - color_code_length(icon)) != 4) {
		return FALSE;
	}
	else {
		return TRUE;
	}
}
