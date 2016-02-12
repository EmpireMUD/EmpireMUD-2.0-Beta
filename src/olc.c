/* ************************************************************************
*   File: olc.c                                           EmpireMUD 2.0b3 *
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
OLC_MODULE(olc_free);
OLC_MODULE(olc_list);
OLC_MODULE(olc_removeindev);
OLC_MODULE(olc_save);
OLC_MODULE(olc_search);
OLC_MODULE(olc_set_flags);
OLC_MODULE(olc_set_max_vnum);
OLC_MODULE(olc_set_min_vnum);

// ability modules
OLC_MODULE(abiledit_flags);
OLC_MODULE(abiledit_masteryability);
OLC_MODULE(abiledit_name);

// adventure zone modules
OLC_MODULE(advedit_author);
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
OLC_MODULE(bedit_hitpoints);
OLC_MODULE(bedit_icon);
OLC_MODULE(bedit_interaction);
OLC_MODULE(bedit_military);
OLC_MODULE(bedit_name);
OLC_MODULE(bedit_spawns);
OLC_MODULE(bedit_title);
OLC_MODULE(bedit_upgradesto);

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
OLC_MODULE(medit_attack);
OLC_MODULE(medit_flags);
OLC_MODULE(medit_interaction);
OLC_MODULE(medit_keywords);
OLC_MODULE(medit_longdescription);
OLC_MODULE(medit_maxlevel);
OLC_MODULE(medit_minlevel);
OLC_MODULE(medit_movetype);
OLC_MODULE(medit_nameset);
OLC_MODULE(medit_script);
OLC_MODULE(medit_sex);
OLC_MODULE(medit_short_description);

// map modules
OLC_MODULE(mapedit_build);
OLC_MODULE(mapedit_complete_room);
OLC_MODULE(mapedit_decustomize);
OLC_MODULE(mapedit_delete_exit);
OLC_MODULE(mapedit_delete_room);
OLC_MODULE(mapedit_exits);
OLC_MODULE(mapedit_icon);
OLC_MODULE(mapedit_pass_walls);
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
OLC_MODULE(morphedit_maxlevel);
OLC_MODULE(morphedit_movetype);
OLC_MODULE(morphedit_requiresobject);
OLC_MODULE(morphedit_shortdesc);

// object modules
OLC_MODULE(oedit_action_desc);
OLC_MODULE(oedit_affects);
OLC_MODULE(oedit_apply);
OLC_MODULE(oedit_armortype);
OLC_MODULE(oedit_arrowtype);
OLC_MODULE(oedit_automint);
OLC_MODULE(oedit_book);
OLC_MODULE(oedit_capacity);
OLC_MODULE(oedit_charges);
OLC_MODULE(oedit_coinamount);
OLC_MODULE(oedit_containerflags);
OLC_MODULE(oedit_contents);
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
OLC_MODULE(oedit_minlevel);
OLC_MODULE(oedit_missilespeed);
OLC_MODULE(oedit_plants);
OLC_MODULE(oedit_poison);
OLC_MODULE(oedit_potion);
OLC_MODULE(oedit_potionscale);
OLC_MODULE(oedit_quantity);
OLC_MODULE(oedit_roomvnum);
OLC_MODULE(oedit_script);
OLC_MODULE(oedit_short_description);
OLC_MODULE(oedit_storage);
OLC_MODULE(oedit_timer);
OLC_MODULE(oedit_type);
OLC_MODULE(oedit_value1);
OLC_MODULE(oedit_value2);
OLC_MODULE(oedit_value3);
OLC_MODULE(oedit_wealth);
OLC_MODULE(oedit_weapontype);
OLC_MODULE(oedit_wear);

// room template
OLC_MODULE(rmedit_affects);
OLC_MODULE(rmedit_description);
OLC_MODULE(rmedit_exit);
OLC_MODULE(rmedit_extra_desc);
OLC_MODULE(rmedit_interaction);
OLC_MODULE(rmedit_flags);
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

// skill modules
OLC_MODULE(skilledit_abbrev);
OLC_MODULE(skilledit_description);
OLC_MODULE(skilledit_flags);
OLC_MODULE(skilledit_name);
OLC_MODULE(skilledit_tree);

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
OLC_MODULE(vedit_extrarooms);
OLC_MODULE(vedit_flags);
OLC_MODULE(vedit_hitpoints);
OLC_MODULE(vedit_icon);
OLC_MODULE(vedit_interiorroom);
OLC_MODULE(vedit_keywords);
OLC_MODULE(vedit_longdescription);
OLC_MODULE(vedit_lookdescription);
OLC_MODULE(vedit_maxlevel);
OLC_MODULE(vedit_minlevel);
OLC_MODULE(vedit_movetype);
OLC_MODULE(vedit_resource);
OLC_MODULE(vedit_script);
OLC_MODULE(vedit_shortdescription);


// externs
extern const char *interact_types[];
extern const int interact_attach_types[NUM_INTERACTS];
extern const byte interact_vnum_types[NUM_INTERACTS];
extern const char *olc_flag_bits[];
extern const char *olc_type_bits[NUM_OLC_TYPES+1];

// external functions
void replace_question_color(char *input, char *color, char *output);
extern char *show_color_codes(char *string);
void sort_icon_set(struct icon_data **list);
void sort_interactions(struct interaction_item **list);
extern bool valid_room_template_vnum(rmt_vnum vnum);

// prototypes
void olc_show_ability(char_data *ch);
void olc_show_adventure(char_data *ch);
void olc_show_archetype(char_data *ch);
void olc_show_augment(char_data *ch);
void olc_show_book(char_data *ch);
void olc_show_building(char_data *ch);
void olc_show_class(char_data *ch);
void olc_show_craft(char_data *ch);
void olc_show_crop(char_data *ch);
void olc_show_global(char_data *ch);
void olc_show_mobile(char_data *ch);
void olc_show_morph(char_data *ch);
void olc_show_object(char_data *ch);
void olc_show_room_template(char_data *ch);
void olc_show_sector(char_data *ch);
void olc_show_skill(char_data *ch);
void olc_show_trigger(char_data *ch);
void olc_show_vehicle(char_data *ch);
extern ability_data *setup_olc_ability(ability_data *input);
extern adv_data *setup_olc_adventure(adv_data *input);
extern archetype_data *setup_olc_archetype(archetype_data *input);
extern augment_data *setup_olc_augment(augment_data *input);
extern book_data *setup_olc_book(book_data *input);
extern bld_data *setup_olc_building(bld_data *input);
extern class_data *setup_olc_class(class_data *input);
extern craft_data *setup_olc_craft(craft_data *input);
extern crop_data *setup_olc_crop(crop_data *input);
extern struct global_data *setup_olc_global(struct global_data *input);
extern char_data *setup_olc_mobile(char_data *input);
extern morph_data *setup_olc_morph(morph_data *input);
extern obj_data *setup_olc_object(obj_data *input);
extern room_template *setup_olc_room_template(room_template *input);
extern sector_data *setup_olc_sector(sector_data *input);
extern skill_data *setup_olc_skill(skill_data *input);
extern struct trig_data *setup_olc_trigger(struct trig_data *input, char **cmdlist_storage);
extern vehicle_data *setup_olc_vehicle(vehicle_data *input);
extern bool validate_icon(char *icon);


// master olc command structure
const struct olc_command_data olc_data[] = {
	// OLC_x: main commands
	{ "abort", olc_abort, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_SECTOR | OLC_SKILL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, OLC_CF_EDITOR | OLC_CF_NO_ABBREV },
	{ "audit", olc_audit, OLC_ABILITY | OLC_ADVENTURE | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_ROOM_TEMPLATE | OLC_SKILL | OLC_VEHICLE, NOBITS },
	{ "copy", olc_copy, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_SECTOR | OLC_SKILL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, NOBITS },
	{ "delete", olc_delete, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_SECTOR | OLC_SKILL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, OLC_CF_NO_ABBREV },
	// "display" command uses the shortcut "." or "olc" with no args, and is in the do_olc function
	{ "edit", olc_edit, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_SECTOR | OLC_SKILL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, NOBITS },
	{ "free", olc_free, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_SECTOR | OLC_SKILL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, NOBITS },
	{ "list", olc_list, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_SECTOR | OLC_SKILL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, NOBITS },
	{ "save", olc_save, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BOOK | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_SECTOR | OLC_SKILL | OLC_TRIGGER | OLC_ADVENTURE | OLC_ROOM_TEMPLATE | OLC_VEHICLE, OLC_CF_EDITOR | OLC_CF_NO_ABBREV },
	{ "search", olc_search, OLC_ABILITY | OLC_ARCHETYPE | OLC_AUGMENT | OLC_BUILDING | OLC_CLASS | OLC_CRAFT | OLC_CROP | OLC_GLOBAL | OLC_MOBILE | OLC_MORPH | OLC_OBJECT | OLC_SECTOR | OLC_SKILL | OLC_TRIGGER | OLC_ROOM_TEMPLATE | OLC_VEHICLE, NOBITS },
	
	// admin
	{ "removeindev", olc_removeindev, NOBITS, NOBITS },
	{ "setflags", olc_set_flags, NOBITS, NOBITS },
	{ "setminvnum", olc_set_min_vnum, NOBITS, NOBITS },
	{ "setmaxvnum", olc_set_max_vnum, NOBITS, NOBITS },
	
	// ability commands
	{ "flags", abiledit_flags, OLC_ABILITY, OLC_CF_EDITOR },
	{ "masteryability", abiledit_masteryability, OLC_ABILITY, OLC_CF_EDITOR },
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
	{ "hitpoints", bedit_hitpoints, OLC_BUILDING, OLC_CF_EDITOR },
	{ "icon", bedit_icon, OLC_BUILDING, OLC_CF_EDITOR },
	{ "interaction", bedit_interaction, OLC_BUILDING, OLC_CF_EDITOR },
	{ "military", bedit_military, OLC_BUILDING, OLC_CF_EDITOR },
	{ "name", bedit_name, OLC_BUILDING, OLC_CF_EDITOR },
	{ "rooms", bedit_extrarooms, OLC_BUILDING, OLC_CF_EDITOR },
	{ "spawns", bedit_spawns, OLC_BUILDING, OLC_CF_EDITOR },
	{ "title", bedit_title, OLC_BUILDING, OLC_CF_EDITOR },
	{ "upgradesto", bedit_upgradesto, OLC_BUILDING, OLC_CF_EDITOR },
	
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
	{ "attack", medit_attack, OLC_MOBILE, OLC_CF_EDITOR },
	{ "flags", medit_flags, OLC_MOBILE, OLC_CF_EDITOR },
	{ "interaction", medit_interaction, OLC_MOBILE, OLC_CF_EDITOR },
	{ "keywords", medit_keywords, OLC_MOBILE, OLC_CF_EDITOR },
	{ "longdescription", medit_longdescription, OLC_MOBILE, OLC_CF_EDITOR },
	{ "maxlevel", medit_maxlevel, OLC_MOBILE, OLC_CF_EDITOR },
	{ "minlevel", medit_minlevel, OLC_MOBILE, OLC_CF_EDITOR },
	{ "movetype", medit_movetype, OLC_MOBILE, OLC_CF_EDITOR },
	{ "nameset", medit_nameset, OLC_MOBILE, OLC_CF_EDITOR },
	{ "script", medit_script, OLC_MOBILE, OLC_CF_EDITOR },
	{ "sex", medit_sex, OLC_MOBILE, OLC_CF_EDITOR },
	{ "shortdescription", medit_short_description, OLC_MOBILE, OLC_CF_EDITOR },
	
	// map commands
	{ "build", mapedit_build, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "complete", mapedit_complete_room, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "decustomize", mapedit_decustomize, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "deleteexit", mapedit_delete_exit, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "deleteroom", mapedit_delete_room, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "description", mapedit_room_description, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "exit", mapedit_exits, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "icon", mapedit_icon, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "name", mapedit_room_name, OLC_MAP, OLC_CF_MAP_EDIT },
	{ "passwalls", mapedit_pass_walls, OLC_MAP, OLC_CF_MAP_EDIT },
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
	{ "maxlevel", morphedit_maxlevel, OLC_MORPH, OLC_CF_EDITOR },
	{ "movetype", morphedit_movetype, OLC_MORPH, OLC_CF_EDITOR },
	{ "requiresability", morphedit_ability, OLC_MORPH, OLC_CF_EDITOR },
	{ "requiresobject", morphedit_requiresobject, OLC_MORPH, OLC_CF_EDITOR },
	{ "shortdescription", morphedit_shortdesc, OLC_MORPH, OLC_CF_EDITOR },
	
	// object commands
	{ "affects", oedit_affects, OLC_OBJECT, OLC_CF_EDITOR },
	{ "apply", oedit_apply, OLC_OBJECT, OLC_CF_EDITOR },
	{ "armortype", oedit_armortype, OLC_OBJECT, OLC_CF_EDITOR },
	{ "arrowtype", oedit_arrowtype, OLC_OBJECT, OLC_CF_EDITOR },
	{ "automint", oedit_automint, OLC_OBJECT, OLC_CF_EDITOR },
	{ "book", oedit_book, OLC_OBJECT, OLC_CF_EDITOR },
	{ "capacity", oedit_capacity, OLC_OBJECT, OLC_CF_EDITOR },
	{ "charges", oedit_charges, OLC_OBJECT, OLC_CF_EDITOR },
	{ "coinamount", oedit_coinamount, OLC_OBJECT, OLC_CF_EDITOR },
	{ "containerflags", oedit_containerflags, OLC_OBJECT, OLC_CF_EDITOR },
	{ "contents", oedit_contents, OLC_OBJECT, OLC_CF_EDITOR },
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
	{ "missilespeed", oedit_missilespeed, OLC_OBJECT, OLC_CF_EDITOR },
	{ "plants", oedit_plants, OLC_OBJECT, OLC_CF_EDITOR },
	{ "poison", oedit_poison, OLC_OBJECT, OLC_CF_EDITOR },
	{ "potion", oedit_potion, OLC_OBJECT, OLC_CF_EDITOR },
	{ "potionscale", oedit_potionscale, OLC_OBJECT, OLC_CF_EDITOR },
	{ "quantity", oedit_quantity, OLC_OBJECT, OLC_CF_EDITOR },
	{ "roomvnum", oedit_roomvnum, OLC_OBJECT, OLC_CF_EDITOR },
	{ "script", oedit_script, OLC_OBJECT, OLC_CF_EDITOR },
	{ "shortdescription", oedit_short_description, OLC_OBJECT, OLC_CF_EDITOR },
	{ "storage", oedit_storage, OLC_OBJECT, OLC_CF_EDITOR },
	{ "store", oedit_storage, OLC_OBJECT, OLC_CF_EDITOR },
	{ "timer", oedit_timer, OLC_OBJECT, OLC_CF_EDITOR },
	{ "type", oedit_type, OLC_OBJECT, OLC_CF_EDITOR },
	{ "value1", oedit_value1, OLC_OBJECT, OLC_CF_EDITOR },
	{ "value2", oedit_value2, OLC_OBJECT, OLC_CF_EDITOR },
	{ "value3", oedit_value3, OLC_OBJECT, OLC_CF_EDITOR },
	{ "wealth", oedit_wealth, OLC_OBJECT, OLC_CF_EDITOR },
	{ "weapontype", oedit_weapontype, OLC_OBJECT, OLC_CF_EDITOR },
	{ "wear", oedit_wear, OLC_OBJECT, OLC_CF_EDITOR },

	// room template commands
	{ "affects", rmedit_affects, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "description", rmedit_description, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "exit", rmedit_exit, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "extra", rmedit_extra_desc, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "interaction", rmedit_interaction, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
	{ "flags", rmedit_flags, OLC_ROOM_TEMPLATE, OLC_CF_EDITOR },
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
	
	// skill commands
	{ "abbrev", skilledit_abbrev, OLC_SKILL, OLC_CF_EDITOR },
	{ "description", skilledit_description, OLC_SKILL, OLC_CF_EDITOR },
	{ "flags", skilledit_flags, OLC_SKILL, OLC_CF_EDITOR },
	{ "name", skilledit_name, OLC_SKILL, OLC_CF_EDITOR },
	{ "tree", skilledit_tree, OLC_SKILL, OLC_CF_EDITOR },
	
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
	{ "extrarooms", vedit_extrarooms, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "flags", vedit_flags, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "hitpoints", vedit_hitpoints, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "icon", vedit_icon, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "interiorroom", vedit_interiorroom, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "keywords", vedit_keywords, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "longdescription", vedit_longdescription, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "lookdescription", vedit_lookdescription, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "maxlevel", vedit_maxlevel, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "minlevel", vedit_minlevel, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "movetype", vedit_movetype, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "resource", vedit_resource, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "script", vedit_script, OLC_VEHICLE, OLC_CF_EDITOR },
	{ "shortdescription", vedit_shortdescription, OLC_VEHICLE, OLC_CF_EDITOR },


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
			case OLC_SKILL: {
				free_skill(GET_OLC_SKILL(ch->desc));
				GET_OLC_SKILL(ch->desc) = NULL;
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
				extern bool audit_ability(ability_data *abil, char_data *ch);
				ability_data *abil, *next_abil;
				HASH_ITER(hh, ability_table, abil, next_abil) {
					if (ABIL_VNUM(abil) >= from_vnum && ABIL_VNUM(abil) <= to_vnum) {
						found |= audit_ability(abil, ch);
					}
				}
				break;
			}
			case OLC_ADVENTURE: {
				extern bool audit_adventure(adv_data *adv, char_data *ch, bool only_one);
				adv_data *adv, *next_adv;
				HASH_ITER(hh, adventure_table, adv, next_adv) {
					if (GET_ADV_VNUM(adv) >= from_vnum && GET_ADV_VNUM(adv) <= to_vnum) {
						found |= audit_adventure(adv, ch, (from_vnum == to_vnum));
					}
				}
				break;
			}
			case OLC_ARCHETYPE: {
				extern bool audit_archetype(archetype_data *arch, char_data *ch);
				archetype_data *arch, *next_arch;
				HASH_ITER(hh, archetype_table, arch, next_arch) {
					if (GET_ARCH_VNUM(arch) >= from_vnum && GET_ARCH_VNUM(arch) <= to_vnum) {
						found |= audit_archetype(arch, ch);
					}
				}
				break;
			}
			case OLC_AUGMENT: {
				extern bool audit_augment(augment_data *aug, char_data *ch);
				augment_data *aug, *next_aug;
				HASH_ITER(hh, augment_table, aug, next_aug) {
					if (GET_AUG_VNUM(aug) >= from_vnum && GET_AUG_VNUM(aug) <= to_vnum) {
						found |= audit_augment(aug, ch);
					}
				}
				break;
			}
			case OLC_BUILDING: {
				extern bool audit_building(bld_data *bld, char_data *ch);
				bld_data *bld, *next_bld;
				HASH_ITER(hh, building_table, bld, next_bld) {
					if (GET_BLD_VNUM(bld) >= from_vnum && GET_BLD_VNUM(bld) <= to_vnum) {
						found |= audit_building(bld, ch);
					}
				}
				break;
			}
			case OLC_CLASS: {
				extern bool audit_class(class_data *cls, char_data *ch);
				class_data *cls, *next_cls;
				HASH_ITER(hh, class_table, cls, next_cls) {
					if (CLASS_VNUM(cls) >= from_vnum && CLASS_VNUM(cls) <= to_vnum) {
						found |= audit_class(cls, ch);
					}
				}
				break;
			}
			case OLC_CRAFT: {
				extern bool audit_craft(craft_data *craft, char_data *ch);
				craft_data *craft, *next_craft;
				HASH_ITER(hh, craft_table, craft, next_craft) {
					if (GET_CRAFT_VNUM(craft) >= from_vnum && GET_CRAFT_VNUM(craft) <= to_vnum) {
						found |= audit_craft(craft, ch);
					}
				}
				break;
			}
			/*
			case OLC_CROP: {
				crop_data *crop, *next_crop;
				HASH_ITER(hh, crop_table, crop, next_crop) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_CROP_VNUM(crop) >= from_vnum && GET_CROP_VNUM(crop) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "[%5d] %s\r\n", GET_CROP_VNUM(crop), GET_CROP_NAME(crop));
					}
				}
				break;
			}
			*/
			case OLC_GLOBAL: {
				extern bool audit_global(struct global_data *global, char_data *ch);
				struct global_data *glb, *next_glb;
				HASH_ITER(hh, globals_table, glb, next_glb) {
					if (GET_GLOBAL_VNUM(glb) >= from_vnum && GET_GLOBAL_VNUM(glb) <= to_vnum) {
						found |= audit_global(glb, ch);
					}
				}
				break;
			}
			case OLC_MOBILE: {
				extern bool audit_mobile(char_data *mob, char_data *ch);
				char_data *mob, *next_mob;
				HASH_ITER(hh, mobile_table, mob, next_mob) {
					if (GET_MOB_VNUM(mob) >= from_vnum && GET_MOB_VNUM(mob) <= to_vnum) {
						found |= audit_mobile(mob, ch);
					}
				}
				break;
			}
			case OLC_MORPH: {
				extern bool audit_morph(morph_data *morph, char_data *ch);
				morph_data *morph, *next_morph;
				HASH_ITER(hh, morph_table, morph, next_morph) {
					if (MORPH_VNUM(morph) >= from_vnum && MORPH_VNUM(morph) <= to_vnum) {
						found |= audit_morph(morph, ch);
					}
				}
				break;
			}
			case OLC_OBJECT: {
				extern bool audit_object(obj_data *obj, char_data *ch);
				obj_data *obj, *next_obj;
				HASH_ITER(hh, object_table, obj, next_obj) {
					if (GET_OBJ_VNUM(obj) >= from_vnum && GET_OBJ_VNUM(obj) <= to_vnum) {
						found |= audit_object(obj, ch);
					}
				}
				break;
			}
			case OLC_ROOM_TEMPLATE: {
				extern bool audit_room_template(room_template *rmt, char_data *ch);
				room_template *rmt, *next_rmt;
				HASH_ITER(hh, room_template_table, rmt, next_rmt) {
					if (GET_RMT_VNUM(rmt) >= from_vnum && GET_RMT_VNUM(rmt) <= to_vnum) {
						found |= audit_room_template(rmt, ch);
					}
				}
				break;
			}
			/*
			case OLC_SECTOR: {
				sector_data *sect, *next_sect;
				HASH_ITER(hh, sector_table, sect, next_sect) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_SECT_VNUM(sect) >= from_vnum && GET_SECT_VNUM(sect) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "[%5d] %s\r\n", GET_SECT_VNUM(sect), GET_SECT_NAME(sect));
					}
				}
				break;
			}
			*/
			case OLC_SKILL: {
				extern bool audit_skill(skill_data *skill, char_data *ch);
				skill_data *skill, *next_skill;
				HASH_ITER(hh, skill_table, skill, next_skill) {
					if (SKILL_VNUM(skill) >= from_vnum && SKILL_VNUM(skill) <= to_vnum) {
						found |= audit_skill(skill, ch);
					}
				}
				break;
			}
			/*
			case OLC_TRIGGER: {
				trig_data *trig, *next_trig;
				HASH_ITER(hh, trigger_table, trig, next_trig) {
					if (len >= sizeof(buf)) {
						break;
					}
					if (GET_TRIG_VNUM(trig) >= from_vnum && GET_TRIG_VNUM(trig) <= to_vnum) {
						++count;
						len += snprintf(buf + len, sizeof(buf) - len, "[%5d] %s\r\n", GET_TRIG_VNUM(trig), GET_TRIG_NAME(trig));
					}
				}
				break;
			}
			*/
			case OLC_VEHICLE: {
				extern bool audit_vehicle(vehicle_data *veh, char_data *ch);
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
		case OLC_SKILL: {
			found = (find_skill_by_vnum(vnum) != NULL);
			exists = (find_skill_by_vnum(from_vnum) != NULL);
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
		case OLC_SKILL: {
			GET_OLC_SKILL(ch->desc) = setup_olc_skill(find_skill_by_vnum(from_vnum));
			GET_OLC_SKILL(ch->desc)->vnum = vnum;
			SET_BIT(GET_OLC_SKILL(ch->desc)->flags, SKILLF_IN_DEVELOPMENT);	// ensure flag
			olc_show_skill(ch);
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
		case OLC_ROOM_TEMPLATE: {
			olc_delete_room_template(ch, vnum);
			break;
		}
		case OLC_SECTOR: {
			olc_delete_sector(ch, vnum);
			break;
		}
		case OLC_SKILL: {
			olc_delete_skill(ch, vnum);
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
		case OLC_ROOM_TEMPLATE: {
			olc_show_room_template(ch);
			break;
		}
		case OLC_SECTOR: {
			olc_show_sector(ch);
			break;
		}
		case OLC_SKILL: {
			olc_show_skill(ch);
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
	descriptor_data *desc;
	char typename[42];
	bool found, ok = TRUE;
	any_vnum vnum;
	
	sprintbit(GET_OLC_TYPE(ch->desc) != 0 ? GET_OLC_TYPE(ch->desc) : type, olc_type_bits, typename, FALSE);
		
	// check that they're not already editing something
	if (GET_OLC_VNUM(ch->desc) != NOTHING) {
		if (*argument) {
			msg_to_char(ch, "You are already editing a %s.\r\n", typename);
		}
		else {
			msg_to_char(ch, "You are currently editing %s %d.\r\n", typename, GET_OLC_VNUM(ch->desc));
		}
		return;
	}
	
	if (!*argument) {
		msg_to_char(ch, "Edit which %s (vnum)?\r\n", typename);
		return;
	}
	if (!isdigit(*argument) || (vnum = atoi(argument)) < 0 || vnum > MAX_VNUM) {
		msg_to_char(ch, "You must pick a valid %s vnum between 0 and %d.\r\n", typename, MAX_VNUM);
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
		msg_to_char(ch, "Someone else is already editing that %s.\r\n", typename);
		return;
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
		case OLC_SKILL: {
			// this sets up either new or existing automatically
			GET_OLC_SKILL(ch->desc) = setup_olc_skill(find_skill_by_vnum(vnum));
			GET_OLC_SKILL(ch->desc)->vnum = vnum;
			olc_show_skill(ch);
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
				case OLC_ROOM_TEMPLATE: {
					free = (room_template_proto(iter) == NULL);
					break;
				}
				case OLC_SECTOR: {
					free = (sector_proto(iter) == NULL);
					break;
				}
				case OLC_SKILL: {
					free = (find_skill_by_vnum(iter) == NULL);
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
	skill_data *skill, *next_skill;
	augment_data *aug, *next_aug;
	class_data *cls, *next_cls;
	adv_data *adv = NULL;
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
	void save_olc_ability(descriptor_data *desc);
	void save_olc_adventure(descriptor_data *desc);
	void save_olc_archetype(descriptor_data *desc);
	void save_olc_augment(descriptor_data *desc);
	void save_olc_book(descriptor_data *desc);
	void save_olc_building(descriptor_data *desc);
	void save_olc_class(descriptor_data *desc);
	void save_olc_craft(descriptor_data *desc);
	void save_olc_crop(descriptor_data *desc);
	void save_olc_global(descriptor_data *desc);
	void save_olc_mobile(descriptor_data *desc);
	void save_olc_morph(descriptor_data *desc);
	void save_olc_object(descriptor_data *desc);
	void save_olc_room_template(descriptor_data *desc);
	void save_olc_sector(descriptor_data *desc);	
	void save_olc_skill(descriptor_data *desc);
	void save_olc_trigger(descriptor_data *desc, char *script_text);
	
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
		// OLC_x:
		switch (GET_OLC_TYPE(ch->desc)) {
			case OLC_ABILITY: {
				save_olc_ability(ch->desc);
				free_ability(GET_OLC_ABILITY(ch->desc));
				GET_OLC_ABILITY(ch->desc) = NULL;
				break;
			}
			case OLC_ADVENTURE: {
				save_olc_adventure(ch->desc);
				free_adventure(GET_OLC_ADVENTURE(ch->desc));
				GET_OLC_ADVENTURE(ch->desc) = NULL;
				break;
			}
			case OLC_ARCHETYPE: {
				save_olc_archetype(ch->desc);
				free_archetype(GET_OLC_ARCHETYPE(ch->desc));
				GET_OLC_ARCHETYPE(ch->desc) = NULL;
				break;
			}
			case OLC_AUGMENT: {
				save_olc_augment(ch->desc);
				free_augment(GET_OLC_AUGMENT(ch->desc));
				GET_OLC_AUGMENT(ch->desc) = NULL;
				break;
			}
			case OLC_BOOK: {
				save_olc_book(ch->desc);
				free_book(GET_OLC_BOOK(ch->desc));
				GET_OLC_BOOK(ch->desc) = NULL;
				break;
			}
			case OLC_BUILDING: {
				save_olc_building(ch->desc);
				free_building(GET_OLC_BUILDING(ch->desc));
				GET_OLC_BUILDING(ch->desc) = NULL;
				break;
			}
			case OLC_CLASS: {
				save_olc_class(ch->desc);
				free_class(GET_OLC_CLASS(ch->desc));
				GET_OLC_CLASS(ch->desc) = NULL;
				break;
			}
			case OLC_CRAFT: {
				save_olc_craft(ch->desc);
				free_craft(GET_OLC_CRAFT(ch->desc));
				GET_OLC_CRAFT(ch->desc) = NULL;
				break;
			}
			case OLC_CROP: {
				save_olc_crop(ch->desc);
				free_crop(GET_OLC_CROP(ch->desc));
				GET_OLC_CROP(ch->desc) = NULL;
				break;
			}
			case OLC_GLOBAL: {
				save_olc_global(ch->desc);
				free_global(GET_OLC_GLOBAL(ch->desc));
				GET_OLC_GLOBAL(ch->desc) = NULL;
				break;
			}
			case OLC_MOBILE: {
				save_olc_mobile(ch->desc);
				free_char(GET_OLC_MOBILE(ch->desc));
				GET_OLC_MOBILE(ch->desc) = NULL;
				break;
			}
			case OLC_MORPH: {
				save_olc_morph(ch->desc);
				free_morph(GET_OLC_MORPH(ch->desc));
				GET_OLC_MORPH(ch->desc) = NULL;
				break;
			}
			case OLC_OBJECT: {
				save_olc_object(ch->desc);
				free_obj(GET_OLC_OBJECT(ch->desc));
				GET_OLC_OBJECT(ch->desc) = NULL;
				break;
			}
			case OLC_ROOM_TEMPLATE: {
				save_olc_room_template(ch->desc);
				free_room_template(GET_OLC_ROOM_TEMPLATE(ch->desc));
				GET_OLC_ROOM_TEMPLATE(ch->desc) = NULL;
				break;
			}
			case OLC_SECTOR: {
				save_olc_sector(ch->desc);
				free_sector(GET_OLC_SECTOR(ch->desc));
				GET_OLC_SECTOR(ch->desc) = NULL;
				break;
			}
			case OLC_SKILL: {
				save_olc_skill(ch->desc);
				free_skill(GET_OLC_SKILL(ch->desc));
				GET_OLC_SKILL(ch->desc) = NULL;
				break;
			}
			case OLC_TRIGGER: {
				save_olc_trigger(ch->desc, GET_OLC_STORAGE(ch->desc));
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
		send_config_msg(ch, "ok_string");
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
			case OLC_ROOM_TEMPLATE: {
				olc_search_room_template(ch, vnum);
				break;
			}
			case OLC_SECTOR: {
				olc_search_sector(ch, vnum);
				break;
			}
			case OLC_SKILL: {
				olc_search_skill(ch, vnum);
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
		size = 30 + color_code_length(line);
		sprintf(lbuf, "%%-%d.%ds%%s", size, size);
		sprintf(save_buffer + strlen(save_buffer), lbuf, line, !(count % 2) ? "\r\n" : "");
	}
	
	if (count % 2) {
		strcat(save_buffer, "\r\n");
	}
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
		strcat(save_buffer, "\r\n");
	}
	
	if (count == 0) {
		strcat(save_buffer, " none\r\n");
	}
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
		obj = obj_proto(res->vnum);
		sprintf(line, "%dx %s", res->amount, !obj ? "UNKNOWN" : skip_filler(GET_OBJ_SHORT_DESC(obj)));
		sprintf(save_buffer + strlen(save_buffer), " &y%2d&0. [%5d] %-26.26s%s", num, res->vnum, line, (!(num % 2) ? "\r\n" : ""));
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
			free(at);
		}
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
		else if (IS_SET(type, OLC_ROOM_TEMPLATE) && !OLC_FLAGGED(ch, OLC_FLAG_NO_ROOM_TEMPLATE)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_TRIGGER) && !OLC_FLAGGED(ch, OLC_FLAG_NO_TRIGGER)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_VEHICLE) && !OLC_FLAGGED(ch, OLC_FLAG_NO_VEHICLES)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_SECTOR) && OLC_FLAGGED(ch, OLC_FLAG_SECTORS)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_ABILITY) && OLC_FLAGGED(ch, OLC_FLAG_ABILITIES)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_CLASS) && OLC_FLAGGED(ch, OLC_FLAG_CLASSES)) {
			return TRUE;
		}
		else if (IS_SET(type, OLC_SKILL) && OLC_FLAGGED(ch, OLC_FLAG_SKILLS)) {
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
* Generic simple string processor for olc. This can't be used for multi-line
* strings, which require the string editor.
*
* @param char_data *ch The player using OLC.
* @param char *argument The arguments the player entered.
* @param char *name The display name of the string we're setting, e.g. "short description".
* @param char **save_point A pointer to the location to save the string.
*/
void olc_process_string(char_data *ch, char *argument, char *name, char **save_point) {
	if (!*argument) {
		msg_to_char(ch, "Set its %s to what?\r\n", name);
	}
	else {
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
		else if (is_abbrev(type_arg, "value")) {
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
	extern const char *icon_types[];
	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], arg4[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	char lbuf[MAX_INPUT_LENGTH];
	struct icon_data *icon, *change, *temp;
	int iter, loc, num;
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
		half_chop(buf1, arg5, arg6);	// arg5: percent, arg6: exclusion
	
		num = atoi(arg3);
		vnum = atoi(arg4);
		prc = atof(arg5);
		
		if (!*arg2 || !*arg3 || !*arg4 || !*arg5 || !isdigit(*arg3) || !isdigit(*arg4) || (!isdigit(*arg5) && *arg5 != '.')) {
			msg_to_char(ch, "Usage: interaction add <type> <quantity> <vnum> <percent> [exclusion code]\r\n");
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
		else if (*arg6 && !isalpha(*arg6)) {
			msg_to_char(ch, "Invalid exclusion code (must be a letter, or leave it blank).\r\n");
		}
		else {
			CREATE(temp, struct interaction_item, 1);
			temp->type = loc;
			temp->vnum = vnum;
			temp->percent = prc;
			temp->quantity = num;
			temp->exclusion_code = exc = isalpha(*arg6) ? *arg6 : 0;
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
		else if (is_abbrev(arg3, "quantity")) {
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
		msg_to_char(ch, "Usage: interaction add <type> <quantity> <vnum> <percent> [exclusion code]\r\n");
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
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], arg4[MAX_INPUT_LENGTH];
	struct resource_data *res, *next_res, *prev_res, *prev_prev, *change, *temp;
	obj_vnum vnum;
	bool found;
	int num;
	
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
					
					msg_to_char(ch, "You remove the %dx %s.\r\n", res->amount, skip_filler(get_obj_name_by_proto(res->vnum)));
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
		num = atoi(arg2);
		vnum = atoi(arg3);
		
		if (!*arg2 || !*arg3 || !isdigit(*arg2)) {
			msg_to_char(ch, "Usage: resource add <quantity> <vnum>\r\n");
		}
		else if (!obj_proto(vnum)) {
			msg_to_char(ch, "There is no such object vnum %d.\r\n", vnum);
		}
		else if (num < 1 || num > 1000) {
			msg_to_char(ch, "You must specify a quantity between 1 and 1000.\r\n");
		}
		else {
			found = FALSE;
			
			CREATE(res, struct resource_data, 1);
			res->vnum = vnum;
			res->amount = num;
			
			// append to end
			if ((temp = *list)) {
				while (temp->next) {
					temp = temp->next;
				}
				temp->next = res;
			}
			else {
				*list = res;
			}
			
			msg_to_char(ch, "You add the %dx %s resource requirement.\r\n", num, skip_filler(get_obj_name_by_proto(vnum)));
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
						msg_to_char(ch, "You move %dx %s %s.\r\n", res->amount, skip_filler(get_obj_name_by_proto(res->vnum)), (up ? "up" : "down"));
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
		if (!*arg2 || !*arg3 || !*arg4 || !isdigit(*arg2) || !isdigit(*arg4)) {
			msg_to_char(ch, "Usage: resource change <number> <quantity | vnum> <value>\r\n");
			return;
		}
		
		vnum = atoi(arg4);	// may be used as quantity instead
		
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
		else if (is_abbrev(arg3, "quantity")) {
			if (vnum < 1 || vnum > 1000) {
				msg_to_char(ch, "You must specify a quantity between 1 and 1000.\r\n");
			}
			else {
				change->amount = vnum;
				msg_to_char(ch, "You change resource %d (%s)'s quantity to %d.\r\n", atoi(arg2), get_obj_name_by_proto(change->vnum), vnum);
			}
		}
		else if (is_abbrev(arg3, "vnum")) {
			if (!obj_proto(vnum)) {
				msg_to_char(ch, "There is no such object vnum %d.\r\n", vnum);
			}
			else {
				change->vnum = vnum;
				msg_to_char(ch, "You change resource %d's vnum to [%d] %s.\r\n", atoi(arg2), vnum, get_obj_name_by_proto(vnum));
			}
		}
		else {
			msg_to_char(ch, "Usage: resource change <number> <quantity | vnum> <value>\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: resource add <quantity> <vnum>\r\n");
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
			if (find->type == interact->type && find->vnum == interact->vnum) {
				found = TRUE;
			}
		}
		
		// don't duplicate
		if (found) {
			continue;
		}
		
		CREATE(new_interact, struct interaction_item, 1);
		*new_interact = *interact;
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
			if (find->vnum == spawn->vnum) {
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
			if (find->type == spawn->type && find->vnum == spawn->vnum) {
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
