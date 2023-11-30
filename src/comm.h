/* ************************************************************************
*   File: comm.h                                          EmpireMUD 2.0b5 *
*  Usage: header file: prototypes of public communication functions       *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define NUM_RESERVED_DESCS	8

#define REBOOT_FILE			"reboot.dat"

/* comm.c */
// TODO: organize these externs and probably move them down
void clear_last_act_message(descriptor_data *desc);
void flush_queues(descriptor_data *d);
void send_to_all(const char *messg, ...) __attribute__((format(printf, 1, 2)));
void send_to_char(const char *messg, char_data *ch);
void msdp_update_room(char_data *ch);
void msg_to_char(char_data *ch, const char *messg, ...) __attribute__((format(printf, 2, 3)));
void msg_to_desc(descriptor_data *d, const char *messg, ...) __attribute__((format(printf, 2, 3)));
void msg_to_vehicle(vehicle_data *veh, bool awake_only, const char *messg, ...) __attribute__((format(printf, 3, 4)));
void olc_audit_msg(char_data *ch, any_vnum vnum, const char *messg, ...);
void send_to_group(char_data *ch, struct group_data *group, const char * msg, ...) __attribute__ ((format (printf, 3, 4)));
void send_to_room(const char *messg, room_data *room);
void send_to_outdoor(bool weather, const char *messg, ...) __attribute__((format(printf, 2, 3)));
void send_stacked_msgs(descriptor_data *desc);
void stack_msg_to_desc(descriptor_data *desc, const char *messg, ...);
void stack_simple_msg_to_desc(descriptor_data *desc, const char *messg);
const char *telnet_go_ahead(descriptor_data *desc);
void perform_to_all(const char *messg, char_data *ch);
char *replace_prompt_codes(char_data *ch, char *str);
char *prompt_color_by_prc(int cur, int max);
void close_socket(descriptor_data *d);
void act(const char *str, int hide_invisible, char_data *ch, const void *obj, const void *vict_obj, bitvector_t act_flags);

// reboot system
extern struct reboot_control_data reboot_control;
extern bool block_all_saves_due_to_shutdown;

bool check_reboot_confirms();
void perform_reboot();
void update_reboot();


// background color codes - not available to players so you have to sprintf/strcpy them in
#define BACKGROUND_RED  "\033[41m"
#define BACKGROUND_GREEN  "\033[42m"
#define BACKGROUND_YELLOW  "\033[43m"
#define BACKGROUND_BLUE  "\033[44m"
#define BACKGROUND_MAGENTA  "\033[45m"
#define BACKGROUND_CYAN  "\033[46m"
#define BACKGROUND_WHITE  "\033[47m"


// act() target flags
#define TO_ROOM		BIT(0)	/* To everyone but ch				*/
#define TO_VICT		BIT(1)	/* To vict_obj						*/
#define TO_NOTVICT	BIT(2)	/* To everyone but ch and vict_obj	*/
#define TO_CHAR		BIT(3)	/* To ch							*/
#define TO_SLEEP	BIT(8)	/* to char, even if sleeping		*/
#define TO_NODARK	BIT(9)	/* ignore darkness for CAN_SEE		*/
#define TO_SPAMMY	BIT(10)	// check PRF_NOSPAM
#define TO_NOT_IGNORING  BIT(11)	// doesn't send to people who are ignoring the main actor
#define TO_IGNORE_BAD_CODE  BIT(12)	// ignores bad $ codes
#define DG_NO_TRIG  BIT(13)	// don't check act trigger
#define ACT_VEHICLE_OBJ  BIT(14)  // the middle/obj param is a vehicle
#define TO_COMBAT_HIT  BIT(15)	// is a hit (fightmessages) -- REQUIRES vict_obj is a char
#define TO_COMBAT_MISS  BIT(16)	// is a miss (fightmessages) -- REQUIRES vict_obj is a char
#define TO_QUEUE  BIT(17)	// message goes into the stackable queue
#define TO_ANIMAL_MOVE  BIT(18)	// allows SM_ANIMAL_MOVEMENT to ignore the message
#define TO_BUFF  BIT(19)	// is a non-violent buff ability for FM_MY_BUFFS_IN_COMBAT, etc -- REQUIRES vict_obj is a char, if provided
#define TO_AFFECT  BIT(20)	// indicates it's an affect apply/wear-off for FM_*_AFFECTS_IN_COMBAT -- requires vict_obj is a char, if provided
#define TO_ABILITY  BIT(21)	// indicates it's an ability, for FM_ ability flags

/* I/O functions */
int write_to_descriptor(socket_t desc, const char *txt);
void write_to_q(const char *txt, struct txt_q *queue, int aliased, bool add_to_head);
void write_to_output(const char *txt, descriptor_data *d);
void page_string(descriptor_data *d, char *str, int keep_internal);
void string_add(descriptor_data *d, char *str);
void start_string_editor(descriptor_data *d, char *prompt, char **writeto, size_t max_len, bool allow_null);

#define SEND_TO_Q(messg, desc)  write_to_output((messg), desc)

#define USING_SMALL(d)	((d)->output == (d)->small_outbuf)
#define USING_LARGE(d)  ((d)->output == (d)->large_outbuf)

typedef RETSIGTYPE sigfunc(int);
