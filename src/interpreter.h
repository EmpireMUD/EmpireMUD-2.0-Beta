/* ************************************************************************
*   File: interpreter.h                                   EmpireMUD 2.0b5 *
*  Usage: header file: public procs, macro defs, subcommand defines       *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

// command types
#define ACMD(name)		void name(char_data *ch, char *argument, int cmd, int subcmd)
#define LIBRARY_SCMD(name)  void name(char_data *ch, char *argument)


// prototypes
void command_interpreter(char_data *ch, char *argument);
char lower( char c );
void nanny(descriptor_data *d, char *arg);
int find_command(const char *command);
void send_low_pos_msg(char_data *ch);


struct command_info {
	const char *command;
	byte minimum_position;
	void (*command_pointer)	(char_data *ch, char * argument, int cmd, int subcmd);
	sh_int minimum_level;
	bitvector_t grants;
	int	subcmd;
	byte ctype;
	sh_int flags;
	any_vnum ability;
};


// for the command_info structure
#define NO_GRANTS  NOBITS


/* Command types for reference/sorting */
#define CTYPE_MOVE		0	/* A movement command		*/
#define CTYPE_IMMORTAL	1	/* An imm command			*/
#define CTYPE_EMPIRE	2	/* An empire command		*/
#define CTYPE_BUILD		3	/* A building command		*/
#define CTYPE_COMBAT	4	/* A fighting command		*/
#define CTYPE_COMM		5	/* A communications command	*/
#define CTYPE_OLC		6	/* OLC command				*/
#define CTYPE_UTIL		7	/* A utility command/other	*/
#define CTYPE_SKILL		8	// ability-related (if it's not combat, empire or move)

/* Command flags */
#define CMD_STAY_HIDDEN		BIT(0)	/* Doesn't unhide person 				*/
#define CMD_UNHIDE_AFTER	BIT(1)	/* Doesn't unhide person until AFTER	*/
#define CMD_IMM_OR_MOB_ONLY	BIT(2)	// disallows players from seeing/using it
#define CMD_NO_ABBREV		BIT(3)	// command can't be abbreviated
#define CMD_VAMPIRE_ONLY	BIT(4)	/* Must be a vampire					*/
#define CMD_NOT_RP			BIT(5)	/* Restricted to non-rpers				*/
#define CMD_NO_ANIMALS		BIT(6)	// doesn't work in animal morphs


struct alias_data {
	char *alias;
	char *replacement;
	int type;
	struct alias_data *next;
};


#define ALIAS_SIMPLE	0
#define ALIAS_COMPLEX	1

#define ALIAS_SEP_CHAR	';'
#define ALIAS_VAR_CHAR	'$'
#define ALIAS_GLOB_CHAR	'*'


/*
 * SUBCOMMANDS
 *   You can define these however you want to, and the definitions of the
 *   subcommands are independent from function to function.
 */

// to avoid magic-numbering in things that don't use subcmds
#define NO_SCMD  0

// for do_accept
#define SCMD_ACCEPT  0
#define SCMD_REJECT  1

// for do_approve
#define SCMD_APPROVE  0
#define SCMD_UNAPPROVE  1

// do_board
#define SCMD_BOARD  0
#define SCMD_ENTER  1

// do_drive
#define SCMD_DRIVE  0
#define SCMD_SAIL  1
#define SCMD_PILOT  2

/* do_gen_ps */
#define SCMD_INFO		0
#define SCMD_HANDBOOK	1 
#define SCMD_CREDITS	2
#define SCMD_WIZLIST	3
#define SCMD_POLICIES	4
#define SCMD_VERSION	5
#define SCMD_GODLIST	6
#define SCMD_MOTD		7
#define SCMD_IMOTD		8
#define SCMD_CLEAR		9
#define SCMD_NEWS		10

/* do_say */
#define SCMD_SAY			0
#define SCMD_OOCSAY			1

/* do_wizutil */
#define SCMD_NOTITLE    1
#define SCMD_MUTE		2
#define SCMD_FREEZE		3
#define SCMD_THAW		4

/* do_spec_com */
#define SCMD_WHISPER	0
#define SCMD_ASK		1

/* do_pub_com */
#define SCMD_SHOUT		0
#define SCMD_GODNET		1
#define SCMD_WIZNET		2
#define NUM_CHANNELS	3	// must be the total

/* do_quit */
#define SCMD_QUI		0
#define SCMD_QUIT		1

/* do_date */
#define SCMD_DATE		0
#define SCMD_UPTIME		1

// do_designate
#define SCMD_DESIGNATE  0
#define SCMD_REDESIGNATE  1

/* do_commands */
#define SCMD_COMMANDS	0
#define SCMD_WIZHELP	1

/* do_drop */
#define SCMD_DROP		0
#define SCMD_JUNK		1

/* do_gen_write */
#define SCMD_BUG		0
#define SCMD_TYPO		1
#define SCMD_IDEA		2

// do_light
#define SCMD_LIGHT  0
#define SCMD_BURN  1

/* do_look */
#define SCMD_LOOK		0

/* do_qcomm */
#define SCMD_QECHO		1

/* do_pour */
#define SCMD_POUR		0
#define SCMD_FILL		1

/* do_poof */
#define SCMD_POOFIN		0
#define SCMD_POOFOUT	1

// do_history
#define SCMD_HISTORY  0
#define SCMD_GOD_HISTORY  1
#define SCMD_TELL_HISTORY  2
#define SCMD_SAY_HISTORY  3
#define SCMD_EMPIRE_HISTORY  4
#define SCMD_ROLL_HISTORY  5

/* do_hit */
#define SCMD_HIT		0
#define SCMD_MURDER		1

/* do_eat */
#define SCMD_EAT		0
#define SCMD_TASTE		1
#define SCMD_DRINK		2
#define SCMD_SIP		3

/* do_echo */
#define SCMD_ECHO		0
#define SCMD_EMOTE		1

/* do_gen_door */
#define SCMD_OPEN		0
#define SCMD_CLOSE		1

/* do_goto */
#define SCMD_GOTO		0
#define SCMD_TELEPORT	1

// do_keep
#define SCMD_KEEP  0
#define SCMD_UNKEEP  1

// do_morph
#define SCMD_MORPH  0
#define SCMD_FASTMORPH  1

// do_no_cmd
#define NOCMD_CAST  1
#define NOCMD_GOSSIP  2
#define NOCMD_LEVELS  3
#define NOCMD_PRACTICE  4
#define NOCMD_RENT  5
#define NOCMD_REPORT  6
#define NOCMD_UNGROUP  7
#define NOCMD_WIMPY  8
#define NOCMD_TOGGLE  9

// do_prompt
#define SCMD_PROMPT  0
#define SCMD_FPROMPT  1

/* do_reboot */
#define SCMD_REBOOT			0
#define SCMD_SHUTDOWN		1

// do_reforge
#define SCMD_REFORGE  0
#define SCMD_REFASHION  1

// do_ritual
#define SCMD_RITUAL  0
#define SCMD_CHANT  1

// do_library
#define SCMD_LIBRARY  0
#define SCMD_BOOKEDIT  1

// do_empire_inventory
#define SCMD_EINVENTORY	0
#define SCMD_EIDENTIFY	1

// do_toggle
#define TOG_ONOFF  0
#define TOG_OFFON  1
#define NUM_TOG_TYPES  2


// movement flags
#define MOVE_LEAD  BIT(0)	// leading something
#define MOVE_FOLLOW  BIT(1)	// following someone
#define MOVE_CIRCLE  BIT(2)	// is circling a building
#define MOVE_EARTHMELD  BIT(3)	// is earthmelded (can't be seen)
#define MOVE_SWIM  BIT(4)	// player is swimming
#define MOVE_CLIMB  BIT(5)	// player is climbing
#define MOVE_HERD  BIT(6)	// mob being herded
#define MOVE_WANDER  BIT(7)	// normal mob move
#define MOVE_RUN  BIT(8)	// running

// flags that ignore some move checks
#define MOVE_IGNORE  (MOVE_LEAD | MOVE_FOLLOW | MOVE_HERD | MOVE_WANDER)


// obj desc flags
#define OBJ_DESC_LONG  0	// long desc: in room
#define OBJ_DESC_SHORT  1	// short desc: inventory, misc
#define OBJ_DESC_CONTENTS  2	// short desc: is in an object
#define OBJ_DESC_INVENTORY  3	// short desc: in inventory
	// 4
#define OBJ_DESC_LOOK_AT  5	// when you look AT the object
	// 6
#define OBJ_DESC_EQUIPMENT  7	// short desc: is equipped


// for look_at_room_by_loc -- option flags
#define LRR_SHIP_PARTIAL  BIT(0)	// shows only part of the room, for use on ships.
#define LRR_SHOW_DARK  BIT(1)	// for passing to show_map_to_char
#define LRR_LOOK_OUT  BIT(2)	// show map even indoors
