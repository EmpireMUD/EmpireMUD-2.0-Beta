/* ************************************************************************
*   File: olc.h                                           EmpireMUD 2.0b5 *
*  Usage: On-Line Creation header file                                    *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
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


// olc controls
#define LVL_BUILDER  LVL_START_IMM	// level at which olc commands are available
#define LVL_UNRESTRICTED_BUILDER  LVL_CIMPL	// level at which there are no vnum restrictions on olc


#define OLC_MODULE(name)	void name(char_data *ch, int type, char *argument)


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
#define NUM_OLC_TYPES  22


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
#define OLC_FLAG_ABILITIES  BIT(15)	// CAN edit abilities
#define OLC_FLAG_CLASSES  BIT(16)	// CAN edit classes
#define OLC_FLAG_SKILLS  BIT(17)	// CAN edit skills
#define OLC_FLAG_NO_VEHICLES  BIT(18)	// cannot edit vehicles
#define OLC_FLAG_NO_MORPHS  BIT(19)	// cannot edit morphs
#define OLC_FLAG_NO_QUESTS  BIT(19)	// cannot edit quests
#define OLC_FLAG_NO_SOCIALS  BIT(20)	// cannot edit socials
#define OLC_FLAG_NO_FACTIONS  BIT(21)	// cannot edit factionss


// for trigger editing
#define TRIG_ARG_PERCENT  BIT(0)
#define TRIG_ARG_COMMAND  BIT(1)
#define TRIG_ARG_PHRASE_OR_WORDLIST  BIT(2)	// 0 = phrase, 1 = wordlist
#define TRIG_ARG_COST  BIT(3)
#define TRIG_ARG_OBJ_WHERE  BIT(4)


// subcommands for olc
struct olc_command_data {
	char *command;
	void (*func)(char_data *ch, int type, char *argument);
	int valid_types;	// OLC_ type const
	int flags;	// OLC_CF_
};


// olc.c helpers
extern struct icon_data *copy_icon_set(struct icon_data *input_list);
extern struct interaction_item *copy_interaction_list(struct interaction_item *input_list);
extern struct spawn_info *copy_spawn_list(struct spawn_info *input_list);
extern int find_olc_type(char *name);
extern bool player_can_olc_edit(char_data *ch, int type, any_vnum vnum);
extern double olc_process_double(char_data *ch, char *argument, char *name, char *command, double min, double max, double old_value);
extern bitvector_t olc_process_flag(char_data *ch, char *argument, char *name, char *command, const char **flag_names, bitvector_t existing_bits);
extern int olc_process_number(char_data *ch, char *argument, char *name, char *command, int min, int max, int old_value);
void olc_process_string(char_data *ch, char *argument, const char *name, char **save_point);
extern int olc_process_type(char_data *ch, char *argument, char *name, char *command, const char **type_names, int old_value);
void olc_process_extra_desc(char_data *ch, char *argument, struct extra_descr_data **list);
void olc_process_icons(char_data *ch, char *argument, struct icon_data **list);
void olc_process_interactions(char_data *ch, char *argument, struct interaction_item **list, int attach_type);
void olc_process_spawns(char_data *ch, char *argument, struct spawn_info **list);
void olc_process_script(char_data *ch, char *argument, struct trig_proto_list **list, int trigger_attach);
void smart_copy_interactions(struct interaction_item **addto, struct interaction_item *input);
void smart_copy_scripts(struct trig_proto_list **addto, struct trig_proto_list *input);
void smart_copy_spawns(struct spawn_info **addto, struct spawn_info *input);
void smart_copy_template_spawns(struct adventure_spawn **addto, struct adventure_spawn *input);
