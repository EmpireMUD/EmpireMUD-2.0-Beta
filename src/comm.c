/* ************************************************************************
*   File: comm.c                                          EmpireMUD 2.0b5 *
*  Usage: Communication, socket handling, main(), central game loop       *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __COMM_C__

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "dg_scripts.h"
#include "dg_event.h"
#include "vnums.h"

/**
* Contents:
*   Data
*   Helpers
*   Reboot System
*   Main Game Loop
*   Messaging
*   Prompt
*   Signal Processing
*   Startup
*/

#ifdef HAVE_ARPA_TELNET_H
#include <arpa/telnet.h>
#else
#include "telnet.h"
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

// external vars
extern struct ban_list_element *ban_list;
extern bool data_table_needs_save;
extern int num_invalid;
extern char **intros;
extern int num_intros;
extern const char *version;
extern int wizlock_level;
extern int no_auto_deletes;
extern ush_int DFLT_PORT;
extern const char *DFLT_DIR;
extern char *LOGNAME;
extern int max_playing;
extern char *help;

// external functions
void save_all_players();
extern char *flush_reduced_color_codes(descriptor_data *desc);
void mobile_activity(void);
void show_string(descriptor_data *d, char *input);
int isbanned(char *hostname);
void save_whole_world();
extern bool is_fight_ally(char_data *ch, char_data *frenemy);

// local functions
RETSIGTYPE checkpointing(int sig);
RETSIGTYPE hupsig(int sig);
RETSIGTYPE reap(int sig);
RETSIGTYPE import_evolutions(int sig);
RETSIGTYPE unrestrict_game(int sig);
char *make_prompt(descriptor_data *point);
char *prompt_str(char_data *ch);
char *replace_prompt_codes(char_data *ch, char *str);
int get_from_q(struct txt_q *queue, char *dest, int *aliased);
int get_max_players(void);
static void msdp_update();
int new_descriptor(socket_t s);
int open_logfile(const char *filename, FILE *stderr_fp);
int perform_alias(descriptor_data *d, char *orig);
int perform_subst(descriptor_data *t, char *orig, char *subst);
int process_input(descriptor_data *t);
int set_sendbuf(socket_t s);
socket_t init_socket(ush_int port);
ssize_t perform_socket_read(socket_t desc, char *read_point,size_t space_left);
ssize_t perform_socket_write(socket_t desc, const char *txt,size_t length);
static int process_output(descriptor_data *t);
struct in_addr *get_bind_addr(void);
void empire_sleep(struct timeval *timeout);
void flush_queues(descriptor_data *d);
void game_loop(socket_t mother_desc);
void heartbeat(int heart_pulse);
void init_descriptor(descriptor_data *newd, int desc);
void init_game(ush_int port);
void nonblock(socket_t s);
void perform_act(const char *orig, char_data *ch, const void *obj, const void *vict_obj, const char_data *to, bitvector_t act_flags);
void reboot_recover(void);
void setup_log(const char *filename, int fd);
void signal_setup(void);
void timeadd(struct timeval *sum, struct timeval *a, struct timeval *b);
void timediff(struct timeval *diff, struct timeval *a, struct timeval *b);
#if defined(POSIX)
sigfunc *my_signal(int signo, sigfunc * func);
#endif


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

/* local globals (I majored in oxymoronism) */
descriptor_data *descriptor_list = NULL;/* master desc list					*/
struct txt_block *bufpool = 0;			/* pool of large output buffers		*/
int buf_largecount = 0;					/* # of large buffers which exist	*/
int buf_overflows = 0;					/* # of overflows of output			*/
int buf_switches = 0;					/* # of switches from small to large buf*/
int empire_shutdown = 0;				/* clean shutdown					*/
int max_players = 0;					/* max descriptors available		*/
int tics_passed = 0;					/* for extern checkpointing			*/
int scheck = 0;							/* for syntax checking mode			*/
struct timeval null_time;				/* zero-valued time structure		*/
FILE *logfile = NULL;					/* Where to send the log messages	*/
bool gain_cond_message = FALSE;		/* gain cond send messages			*/
int dg_act_check;	/* toggle for act_trigger */
unsigned long pulse = 0;	/* number of pulses since game start */
static bool reboot_recovery = FALSE;
int mother_desc;
ush_int port;
bool do_evo_import = FALSE;	// triggered by SIGUSR1 to import evolutions

// vars to prevent running multiple cycles during a missed-pulse catch-up cycle
bool catch_up_combat = FALSE;	// frequent_combat()
bool catch_up_actions = FALSE;	// update_actions()
bool catch_up_mobs = FALSE;	// mobile_activity()

// vars for detecting slow IPs and preventing repeat-lag
char **detected_slow_ips = NULL;
int num_slow_ips = 0;

/* Reboot data (default to a normal reboot once per week) */
struct reboot_control_data reboot_control = { SCMD_REBOOT, 7.5 * (24 * 60), SHUTDOWN_NORMAL, FALSE };


#ifdef __CXREF__
#undef FD_ZERO
#undef FD_SET
#undef FD_ISSET
#undef FD_CLR
#define FD_ZERO(x)
#define FD_SET(x, y) 0
#define FD_ISSET(x, y) 0
#define FD_CLR(x, y)
#endif

#define plant_magic(x)	do { (x)[sizeof(x) - 1] = MAGIC_NUMBER; } while (0)
#define test_magic(x)	((x)[sizeof(x) - 1])


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Adds an IP address to the list of slow IPs to not look up. This persists
* until reboot.
*
* @param char *ip The IP to add.
*/
void add_slow_ip(char *ip) {
	if (!ip || !*ip) {
		return;	// no work
	}
	
	if (num_slow_ips > 0 && detected_slow_ips) {
		RECREATE(detected_slow_ips, char*, num_slow_ips+1);
	}
	else {
		CREATE(detected_slow_ips, char*, num_slow_ips+1);
	}
	
	detected_slow_ips[num_slow_ips++] = str_dup(ip);
}


// wipes the last act message on the descriptor
void clear_last_act_message(descriptor_data *desc) {
	if (desc->last_act_message) {
		free(desc->last_act_message);
		desc->last_act_message = NULL;
	}
}


/**
* Determines if a descriptor number is already in use. These numbers loop
* at 1000 and, in rare cases, we come back up on a number in use.
*
* @param int num The number to check.
* @return bool TRUE if num is in use; FALSE if not.
*/
bool desc_num_in_use(int num) {
	descriptor_data *desc;
	
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (desc->desc_num == num) {
			return TRUE;
		}
	}
	
	// clear
	return FALSE;
}


/*
 * This may not be pretty but it keeps game_loop() neater than if it was inline.
 */
inline void empire_sleep(struct timeval *timeout) {
	if (select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, timeout) < 0) {
		if (errno != EINTR) {
			perror("SYSERR: Select sleep");
			exit(1);
		}
	}
}


/**
* Updates MSDP data for a player's location, to be called when they move.
*
* @param char_data *ch The player to update (no effect if no descriptor).
*/
void msdp_update_room(char_data *ch) {
	extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom, bool allow_fake_loc);
	extern char *get_room_name(room_data *room, bool color);
	extern const char *alt_dirs[];
	
	char buf[MAX_STRING_LENGTH], area_name[128], exits[256];
	struct empire_city_data *city;
	struct instance_data *inst;
	struct island_info *island;
	size_t buf_size, ex_size;
	descriptor_data *desc;
	
	// no work
	if (!ch || !(desc = ch->desc)) {
		return;
	}

	// determine area name: we'll use it twice
	if ((inst = find_instance_by_room(IN_ROOM(ch), FALSE, FALSE))) {
		snprintf(area_name, sizeof(area_name), "%s", GET_ADV_NAME(INST_ADVENTURE(inst)));
	}
	else if ((city = find_city(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch)))) {
		snprintf(area_name, sizeof(area_name), "%s", city->name);
	}
	else if ((island = GET_ISLAND(IN_ROOM(ch)))) {
		snprintf(area_name, sizeof(area_name), "%s", island->name);
	}
	else {
		snprintf(area_name, sizeof(area_name), "Unknown");
	}
	MSDPSetString(desc, eMSDP_AREA_NAME, area_name);
	
	// room var: table
	buf_size = snprintf(buf, sizeof(buf), "%cVNUM%c%d", (char)MSDP_VAR, (char)MSDP_VAL, IS_IMMORTAL(ch) ? GET_ROOM_VNUM(IN_ROOM(ch)) : 0);
	buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cNAME%c%s", (char)MSDP_VAR, (char)MSDP_VAL, get_room_name(IN_ROOM(ch), FALSE));
	buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cAREA%c%s", (char)MSDP_VAR, (char)MSDP_VAL, area_name);
	
	buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cCOORDS%c%c", (char)MSDP_VAR, (char)MSDP_VAL, (char)MSDP_TABLE_OPEN);
	if (HAS_NAVIGATION(ch) && !NO_LOCATION(IN_ROOM(ch))) {
		buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cX%c%d", (char)MSDP_VAR, (char)MSDP_VAL, X_COORD(IN_ROOM(ch)));
		buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cY%c%d", (char)MSDP_VAR, (char)MSDP_VAL, Y_COORD(IN_ROOM(ch)));
	}
	else {
		buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cX%c0", (char)MSDP_VAR, (char)MSDP_VAL);
		buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cY%c0", (char)MSDP_VAR, (char)MSDP_VAL);
	}
	buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cZ%c0%c", (char)MSDP_VAR, (char)MSDP_VAL, (char)MSDP_TABLE_CLOSE);
	buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cTERRAIN%c%s", (char)MSDP_VAR, (char)MSDP_VAL, GET_SECT_NAME(SECT(IN_ROOM(ch))));

	buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cEXITS%c%c", (char)MSDP_VAR, (char)MSDP_VAL, (char)MSDP_TABLE_OPEN);
	*exits = '\0';
	ex_size = 0;
	if (COMPLEX_DATA(IN_ROOM(ch)) && ROOM_IS_CLOSED(IN_ROOM(ch))) {
		struct room_direction_data *ex;
		for (ex = COMPLEX_DATA(IN_ROOM(ch))->exits; ex; ex = ex->next) {
			if (ex->room_ptr && !EXIT_FLAGGED(ex, EX_CLOSED)) {
				ex_size += snprintf(exits + ex_size, sizeof(exits) - ex_size, "%c%s%c%d", (char)MSDP_VAR, alt_dirs[ex->dir], (char)MSDP_VAL, IS_IMMORTAL(ch) ? GET_ROOM_VNUM(ex->room_ptr) : 0);
			}
		}
	}
	buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%s%c", exits, (char)MSDP_TABLE_CLOSE);
	MSDPSetTable(desc, eMSDP_ROOM, buf);
	
	// simple room data
	MSDPSetNumber(desc, eMSDP_ROOM_VNUM, IS_IMMORTAL(ch) ? GET_ROOM_VNUM(IN_ROOM(ch)) : 0);
	MSDPSetString(desc, eMSDP_ROOM_NAME, get_room_name(IN_ROOM(ch), FALSE));
	MSDPSetTable(desc, eMSDP_ROOM_EXITS, exits);
	
	MSDPUpdate(desc);
}


/**
* From KaVir's protocol snippet (see protocol.c)
*/
static void msdp_update(void) {
	extern int get_block_rating(char_data *ch, bool can_gain_skill);
	extern double get_combat_speed(char_data *ch, int pos);
	extern int get_crafting_level(char_data *ch);
	extern int get_dodge_modifier(char_data *ch, char_data *attacker, bool can_gain_skill);
	void get_player_skill_string(char_data *ch, char *buffer, bool abbrev);
	extern int get_to_hit(char_data *ch, char_data *victim, bool off_hand, bool can_gain_skill);
	extern int health_gain(char_data *ch, bool info_only);
	extern int mana_gain(char_data *ch, bool info_only);
	extern int move_gain(char_data *ch, bool info_only);
	extern int pick_season(room_data *room);
	extern int total_bonus_healing(char_data *ch);
	extern int get_total_score(empire_data *emp);
	extern const char *damage_types[];
	extern const char *genders[];
	extern const double hit_per_dex;
	extern const char *seasons[];
	
	struct player_skill_data *skill, *next_skill;
	struct over_time_effect_type *dot;
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	struct cooldown_data *cool;
	char_data *ch, *pOpponent, *focus;
	bool is_ally;
	struct affected_type *aff;
	descriptor_data *d;
	int hit_points, PlayerCount = 0;
	size_t buf_size;

	for (d = descriptor_list; d; d = d->next) {
		if ((ch = d->character) && !IS_NPC(ch) && STATE(d) == CON_PLAYING) {
			++PlayerCount;
			
			// TODO: Most of this could be moved to set only when it is changed

			MSDPSetString(d, eMSDP_ACCOUNT_NAME, GET_NAME(ch));
			MSDPSetString(d, eMSDP_CHARACTER_NAME, PERS(ch, ch, FALSE));
			
			MSDPSetString(d, eMSDP_GENDER, genders[GET_SEX(ch)]);
			MSDPSetNumber(d, eMSDP_HEALTH, GET_HEALTH(ch));
			MSDPSetNumber(d, eMSDP_HEALTH_MAX, GET_MAX_HEALTH(ch));
			MSDPSetNumber(d, eMSDP_HEALTH_REGEN, health_gain(ch, TRUE));
			MSDPSetNumber(d, eMSDP_MANA, GET_MANA(ch));
			MSDPSetNumber(d, eMSDP_MANA_MAX, GET_MAX_MANA(ch));
			MSDPSetNumber(d, eMSDP_MANA_REGEN, mana_gain(ch, TRUE));
			MSDPSetNumber(d, eMSDP_MOVEMENT, GET_MOVE(ch));
			MSDPSetNumber(d, eMSDP_MOVEMENT_MAX, GET_MAX_MOVE(ch));
			MSDPSetNumber(d, eMSDP_MOVEMENT_REGEN, move_gain(ch, TRUE));
			MSDPSetNumber(d, eMSDP_BLOOD, GET_BLOOD(ch));
			MSDPSetNumber(d, eMSDP_BLOOD_MAX, GET_MAX_BLOOD(ch));
			MSDPSetNumber(d, eMSDP_BLOOD_UPKEEP, MAX(0, GET_BLOOD_UPKEEP(ch)));
			
			// affects
			*buf = '\0';
			buf_size = 0;
			for (aff = ch->affected; aff; aff = aff->next) {
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%c%s%c%ld", (char)MSDP_VAR, get_generic_name_by_vnum(aff->type), (char)MSDP_VAL, (aff->duration == UNLIMITED ? -1 : (aff->duration * SECS_PER_REAL_UPDATE)));
			}
			MSDPSetTable(d, eMSDP_AFFECTS, buf);
			
			// dots
			*buf = '\0';
			buf_size = 0;
			for (dot = ch->over_time_effects; dot; dot = dot->next) {
				// each dot has a sub-table
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%c%s%c%c", (char)MSDP_VAR, get_generic_name_by_vnum(dot->type), (char)MSDP_VAL, (char)MSDP_TABLE_OPEN);
				
				
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cDURATION%c%ld", (char)MSDP_VAR, (char)MSDP_VAL, (dot->duration == UNLIMITED ? -1 : (dot->duration * SECS_PER_REAL_UPDATE)));
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cTYPE%c%s", (char)MSDP_VAR, (char)MSDP_VAL, damage_types[dot->damage_type]);
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cDAMAGE%c%d", (char)MSDP_VAR, (char)MSDP_VAL, dot->damage * dot->stack);
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cSTACKS%c%d", (char)MSDP_VAR, (char)MSDP_VAL, dot->stack);
				
				// end table
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%c", (char)MSDP_TABLE_CLOSE);
			}
			MSDPSetTable(d, eMSDP_DOTS, buf);
			
			// cooldowns
			*buf = '\0';
			buf_size = 0;
			for (cool = ch->cooldowns; cool; cool = cool->next) {
				if (cool->expire_time > time(0)) {
					buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%c%s%c%ld", (char)MSDP_VAR, get_generic_name_by_vnum(cool->type), (char)MSDP_VAL, cool->expire_time - time(0));
				}
			}
			MSDPSetTable(d, eMSDP_COOLDOWNS, buf);
			
			MSDPSetNumber(d, eMSDP_LEVEL, get_approximate_level(ch));
			MSDPSetNumber(d, eMSDP_SKILL_LEVEL, IS_NPC(ch) ? 0 : GET_SKILL_LEVEL(ch));
			MSDPSetNumber(d, eMSDP_GEAR_LEVEL, IS_NPC(ch) ? 0 : GET_GEAR_LEVEL(ch));
			MSDPSetNumber(d, eMSDP_CRAFTING_LEVEL, get_crafting_level(ch));

			get_player_skill_string(ch, part, FALSE);
			snprintf(buf, sizeof(buf), "%s", part);
			MSDPSetString(d, eMSDP_CLASS, buf);
			
			// skills
			*buf = '\0';
			buf_size = 0;
			HASH_ITER(hh, GET_SKILL_HASH(ch), skill, next_skill) {
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%c%s%c%c", (char)MSDP_VAR, SKILL_NAME(skill->ptr), (char)MSDP_VAL, (char)MSDP_TABLE_OPEN);
				
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cLEVEL%c%d", (char)MSDP_VAR, (char)MSDP_VAL, skill->level);
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cEXP%c%.2f", (char)MSDP_VAR, (char)MSDP_VAL, skill->exp);
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cRESETS%c%d", (char)MSDP_VAR, (char)MSDP_VAL, skill->resets);
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%cNOSKILL%c%d", (char)MSDP_VAR, (char)MSDP_VAL, skill->noskill ? 1 : 0);
				
				// end table
				buf_size += snprintf(buf + buf_size, sizeof(buf) - buf_size, "%c", (char)MSDP_TABLE_CLOSE);
			}
			MSDPSetTable(d, eMSDP_SKILLS, buf);

			MSDPSetNumber(d, eMSDP_MONEY, total_coins(ch));
			MSDPSetNumber(d, eMSDP_BONUS_EXP, IS_NPC(ch) ? 0 : GET_DAILY_BONUS_EXPERIENCE(ch));
			MSDPSetNumber(d, eMSDP_INVENTORY, IS_CARRYING_N(ch));
			MSDPSetNumber(d, eMSDP_INVENTORY_MAX, CAN_CARRY_N(ch));
			
			MSDPSetNumber(d, eMSDP_STR, GET_STRENGTH(ch));
			MSDPSetNumber(d, eMSDP_DEX, GET_DEXTERITY(ch));
			MSDPSetNumber(d, eMSDP_CHA, GET_CHARISMA(ch));
			MSDPSetNumber(d, eMSDP_GRT, GET_GREATNESS(ch));
			MSDPSetNumber(d, eMSDP_INT, GET_INTELLIGENCE(ch));
			MSDPSetNumber(d, eMSDP_WIT, GET_WITS(ch));
			MSDPSetNumber(d, eMSDP_STR_PERM, GET_REAL_ATT(ch, STRENGTH));
			MSDPSetNumber(d, eMSDP_DEX_PERM, GET_REAL_ATT(ch, DEXTERITY));
			MSDPSetNumber(d, eMSDP_CHA_PERM, GET_REAL_ATT(ch, CHARISMA));
			MSDPSetNumber(d, eMSDP_GRT_PERM, GET_REAL_ATT(ch, GREATNESS));
			MSDPSetNumber(d, eMSDP_INT_PERM, GET_REAL_ATT(ch, INTELLIGENCE));
			MSDPSetNumber(d, eMSDP_WIT_PERM, GET_REAL_ATT(ch, WITS));
			
			MSDPSetNumber(d, eMSDP_BLOCK, get_block_rating(ch, FALSE));
			MSDPSetNumber(d, eMSDP_DODGE, get_dodge_modifier(ch, NULL, FALSE) - (hit_per_dex * GET_DEXTERITY(ch)));	// same change made to it in score
			MSDPSetNumber(d, eMSDP_TO_HIT, get_to_hit(ch, NULL, FALSE, FALSE) - (hit_per_dex * GET_DEXTERITY(ch)));	// same change as in score
			snprintf(buf, sizeof(buf), "%.2f", get_combat_speed(ch, WEAR_WIELD));
			MSDPSetString(d, eMSDP_SPEED, buf);
			MSDPSetNumber(d, eMSDP_RESIST_PHYSICAL, GET_RESIST_PHYSICAL(ch));
			MSDPSetNumber(d, eMSDP_RESIST_MAGICAL, GET_RESIST_MAGICAL(ch));
			MSDPSetNumber(d, eMSDP_BONUS_PHYSICAL, GET_BONUS_PHYSICAL(ch));
			MSDPSetNumber(d, eMSDP_BONUS_MAGICAL, GET_BONUS_MAGICAL(ch));
			MSDPSetNumber(d, eMSDP_BONUS_HEALING, total_bonus_healing(ch));
			
			// empire
			if (GET_LOYALTY(ch) && !IS_NPC(ch)) {
				MSDPSetString(d, eMSDP_EMPIRE_NAME, EMPIRE_NAME(GET_LOYALTY(ch)));
				MSDPSetString(d, eMSDP_EMPIRE_ADJECTIVE, EMPIRE_ADJECTIVE(GET_LOYALTY(ch)));
				MSDPSetString(d, eMSDP_EMPIRE_RANK, strip_color(EMPIRE_RANK(GET_LOYALTY(ch), GET_RANK(ch)-1)));
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY, EMPIRE_TERRITORY(GET_LOYALTY(ch), TER_TOTAL));
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY_MAX, land_can_claim(GET_LOYALTY(ch), TER_TOTAL));
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY_OUTSIDE, EMPIRE_TERRITORY(GET_LOYALTY(ch), TER_OUTSKIRTS));
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY_OUTSIDE_MAX, land_can_claim(GET_LOYALTY(ch), TER_OUTSKIRTS));
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY_FRONTIER, EMPIRE_TERRITORY(GET_LOYALTY(ch), TER_FRONTIER));
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY_FRONTIER_MAX, land_can_claim(GET_LOYALTY(ch), TER_FRONTIER));
				MSDPSetNumber(d, eMSDP_EMPIRE_WEALTH, GET_TOTAL_WEALTH(GET_LOYALTY(ch)));
				MSDPSetNumber(d, eMSDP_EMPIRE_SCORE, get_total_score(GET_LOYALTY(ch)));
			}
			else {
				MSDPSetString(d, eMSDP_EMPIRE_NAME, "");
				MSDPSetString(d, eMSDP_EMPIRE_ADJECTIVE, "");
				MSDPSetString(d, eMSDP_EMPIRE_RANK, "");
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY, 0);
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY_MAX, 0);
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY_OUTSIDE, 0);
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY_OUTSIDE_MAX, 0);
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY_FRONTIER, 0);
				MSDPSetNumber(d, eMSDP_EMPIRE_TERRITORY_FRONTIER_MAX, 0);
				MSDPSetNumber(d, eMSDP_EMPIRE_WEALTH, 0);
				MSDPSetNumber(d, eMSDP_EMPIRE_SCORE, 0);
			}

			/* This would be better moved elsewhere */
			if ((pOpponent = FIGHTING(ch))) {
				hit_points = (GET_HEALTH(pOpponent) * 100) / GET_MAX_HEALTH(pOpponent);
				MSDPSetNumber(d, eMSDP_OPPONENT_HEALTH, hit_points);
				MSDPSetNumber(d, eMSDP_OPPONENT_HEALTH_MAX, 100);
				MSDPSetNumber(d, eMSDP_OPPONENT_LEVEL, get_approximate_level(pOpponent));
				MSDPSetString(d, eMSDP_OPPONENT_NAME, PERS(pOpponent, ch, FALSE));
				if ((focus = FIGHTING(pOpponent))) {
					is_ally = is_fight_ally(ch, focus);
					hit_points = is_ally ? GET_HEALTH(focus) : (GET_HEALTH(focus) * 100) / MAX(1,GET_MAX_HEALTH(focus));
					MSDPSetNumber(d, eMSDP_OPPONENT_FOCUS_HEALTH, hit_points);
					MSDPSetNumber(d, eMSDP_OPPONENT_FOCUS_HEALTH_MAX, is_ally ? GET_MAX_HEALTH(focus) : 100);
					MSDPSetString(d, eMSDP_OPPONENT_FOCUS_NAME, PERS(focus, ch, FALSE));
				} else {
					MSDPSetString(d, eMSDP_OPPONENT_FOCUS_NAME, "");
					MSDPSetNumber(d, eMSDP_OPPONENT_FOCUS_HEALTH, 0);
					MSDPSetNumber(d, eMSDP_OPPONENT_FOCUS_HEALTH_MAX, 0);
				}
			}
			else { // Clear the values
				MSDPSetNumber(d, eMSDP_OPPONENT_HEALTH, 0);
				MSDPSetNumber(d, eMSDP_OPPONENT_LEVEL, 0);
				MSDPSetString(d, eMSDP_OPPONENT_NAME, "");
				MSDPSetString(d, eMSDP_OPPONENT_FOCUS_NAME, "");
				MSDPSetNumber(d, eMSDP_OPPONENT_FOCUS_HEALTH, 0);
				MSDPSetNumber(d, eMSDP_OPPONENT_FOCUS_HEALTH_MAX, 0);
			}
			
			MSDPSetNumber(d, eMSDP_WORLD_TIME, time_info.hours);
			MSDPSetString(d, eMSDP_WORLD_SEASON, seasons[pick_season(IN_ROOM(ch))]);
			
			// done -- send it
			MSDPUpdate(d);
		}

		/* Ideally this should be called once at startup, and again whenever
		* someone leaves or joins the mud.  But this works, and it keeps the
		* snippet simple.  Optimise as you see fit.
		*/
	MSSPSetPlayers(PlayerCount);
	}
}


/* perform substitution for the '^..^' csh-esque syntax orig is the
 * orig string, i.e. the one being modified.  subst contains the
 * substition string, i.e. "^telm^tell"
 */
int perform_subst(descriptor_data *t, char *orig, char *subst) {
	char newsub[MAX_INPUT_LENGTH + 5];

	char *first, *second, *strpos;

	/*
	 * first is the position of the beginning of the first string (the one
	 * to be replaced
	 */
	first = subst + 1;

	/* now find the second '^' */
	if (!(second = strchr(first, '^'))) {
		SEND_TO_Q("Invalid substitution.\r\n", t);
		return (1);
	}
	/* terminate "first" at the position of the '^' and make 'second' point
	 * to the beginning of the second string
	 */
	*(second++) = '\0';

	/* now, see if the contents of the first string appear in the original */
	if (!(strpos = strstr(orig, first))) {
		SEND_TO_Q("Invalid substitution.\r\n", t);
		return (1);
	}
	/* now, we construct the new string for output. */

	/* first, everything in the original, up to the string to be replaced */
	strncpy(newsub, orig, (strpos - orig));
	newsub[(strpos - orig)] = '\0';

	/* now, the replacement string */
	strncat(newsub, second, (MAX_INPUT_LENGTH - strlen(newsub) - 1));

	/* now, if there's anything left in the original after the string to
	 * replaced, copy that too.
	 */
	if (((strpos - orig) + strlen(first)) < strlen(orig))
		strncat(newsub, strpos + strlen(first), (MAX_INPUT_LENGTH - strlen(newsub) - 1));

	/* terminate the string in case of an overflow from strncat */
	newsub[MAX_INPUT_LENGTH - 1] = '\0';
	strcpy(subst, newsub);

	return (0);
}


/*
 * This function is called every 30 seconds from heartbeat().  It checks
 * the four global buffers in EmpireMUD to ensure that no one has written
 * past their bounds.  If our check digit is not there (and the position
 * doesn't have a NUL which may result from snprintf) then we gripe that
 * someone has overwritten our buffer.  This could cause a false positive
 * if someone uses the buffer as a non-terminated character array but that
 * is not likely. -gg
 */
void sanity_check(void) {
	int ok = TRUE;

	/*
	 * If any line is false, 'ok' will become false also.
	 */
	ok &= (test_magic(buf)  == MAGIC_NUMBER || test_magic(buf)  == '\0');
	ok &= (test_magic(buf1) == MAGIC_NUMBER || test_magic(buf1) == '\0');
	ok &= (test_magic(buf2) == MAGIC_NUMBER || test_magic(buf2) == '\0');
	ok &= (test_magic(arg)  == MAGIC_NUMBER || test_magic(arg)  == '\0');

	/*
	 * This isn't exactly the safest thing to do (referencing known bad memory)
	 * but we're doomed to crash eventually, might as well try to get something
	 * useful before we go down. -gg
	 * However, lets fix the problem so we don't spam the logs. -gg 11/24/98
	 */
	if (!ok) {
		log("SYSERR: *** Buffer overflow! ***\nbuf: %s\nbuf1: %s\nbuf2: %s\narg: %s", buf, buf1, buf2, arg);

		plant_magic(buf);
		plant_magic(buf1);
		plant_magic(buf2);
		plant_magic(arg);
	}

#if 0
	log("Statistics: buf=%d buf1=%d buf2=%d arg=%d", strlen(buf), strlen(buf1), strlen(buf2), strlen(arg));
#endif
}


/**
* Removes any side-protocol telnet junk, e.g. for snooping.
*
* @param const char *str The string to strip.
* @return char* The resulting stripped string.
*/
char *strip_telnet_codes(const char *str) {
	static char output[MAX_STRING_LENGTH];
	size_t space_left;
	const char *iter;
	char *pos;
	int off;
	
	pos = output;
	space_left = MAX_STRING_LENGTH - 1;
	off = 0;
	
	for (iter = str; *iter && space_left > 0; ++iter) {
		if (*iter == (char)IAC && *(iter+1) == (char)SB) {
			++off;	// stop copying
			++iter;
			continue;
		}
		else if (*iter == (char)IAC && *(iter+1) == (char)SE) {
			--off;	// start copying
			++iter;
			continue;
		}
		
		// copy?
		if (off <= 0) {
			*(pos++) = *iter;
			--space_left;
		}
	}
	*pos = '\0';	// terminate here
	
	return output;
}


/*
 * Add 2 time values.
 *
 * Patch sent by "d. hall" <dhall@OOI.NET> to fix 'static' usage.
 */
void timeadd(struct timeval *rslt, struct timeval *a, struct timeval *b) {
	rslt->tv_sec = a->tv_sec + b->tv_sec;
	rslt->tv_usec = a->tv_usec + b->tv_usec;

	while (rslt->tv_usec >= 1000000) {
		rslt->tv_usec -= 1000000;
		rslt->tv_sec++;
	}
}


/*
 *  new code to calculate time differences, which works on systems
 *  for which tv_usec is unsigned (and thus comparisons for something
 *  being < 0 fail).  Based on code submitted by ss@sirocco.cup.hp.com.
 *
 * code to return the time difference between a and b (a-b).
 * always returns a nonnegative value (floors at 0).
 */
void timediff(struct timeval *rslt, struct timeval *a, struct timeval *b) {
	if (a->tv_sec < b->tv_sec)
		*rslt = null_time;
	else if (a->tv_sec == b->tv_sec) {
		if (a->tv_usec < b->tv_usec)
			*rslt = null_time;
		else {
			rslt->tv_sec = 0;
			rslt->tv_usec = a->tv_usec - b->tv_usec;
		}
	}
	else {			/* a->tv_sec > b->tv_sec */
		rslt->tv_sec = a->tv_sec - b->tv_sec;
		if (a->tv_usec < b->tv_usec) {
			rslt->tv_usec = a->tv_usec + 1000000 - b->tv_usec;
			rslt->tv_sec--;
		}
		else
			rslt->tv_usec = a->tv_usec - b->tv_usec;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// REBOOT SYSTEM ///////////////////////////////////////////////////////////

/**
* @return bool TRUE if all players are confirmed for the reboot (or close enough).
*/
bool check_reboot_confirms(void) {
	descriptor_data *desc;
	bool found = FALSE;
	
	// check for unconfirmed folks
	for (desc = descriptor_list; desc && !found; desc = desc->next) {
		if (STATE(desc) != CON_PLAYING) {
			found = TRUE;
		}
		else if (IS_NPC(desc->character)) {
			continue;
		}
		else if (!REBOOT_CONF(desc->character) && (desc->character->char_specials.timer * SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN) < 5) {
			found = TRUE;
		}
	}
	
	return !found;
}


/**
* Perform a reboot/shutdown.
*/
void perform_reboot(void) {
	extern const char *reboot_strings[];
	extern int num_of_reboot_strings;
	
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], group_data[MAX_STRING_LENGTH];
	descriptor_data *desc, *next_desc;
	int gsize = 0;
	FILE *fl = NULL;
	
	*group_data = '\0';

	if (reboot_control.type == SCMD_REBOOT && !(fl = fopen(REBOOT_FILE, "w"))) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Reboot file not writeable, aborting reboot");
		reboot_control.time = -1;
		return;
	}

	// prepare for the end!
	save_all_empires();
	save_whole_world();

	if (reboot_control.type == SCMD_REBOOT) {
		sprintf(buf, "\r\n[0;0;31m *** Rebooting ***[0;0;37m\r\nPlease be patient, this will take a second.\r\n\r\n");
	}
	else if (reboot_control.type == SCMD_SHUTDOWN) {
		sprintf(buf, "\r\n[0;0;31m *** Shutting Down ***[0;0;37m\r\nThe mud is shutting down, please reconnect later.\r\n\r\n");
	}

	for (desc = descriptor_list; desc; desc = next_desc) {
		char_data *och = desc->character;
		next_desc = desc->next;
		
		// people not in-game get trimmed
		if (!och || STATE(desc) != CON_PLAYING) {
			write_to_descriptor(desc->descriptor, buf);
			close_socket(desc);
			continue;
		}

		if (reboot_control.type == SCMD_REBOOT && fl) {
			fprintf(fl, "%d %s %s %s\n", desc->descriptor, GET_NAME(och), desc->host, CopyoverGet(desc));
		
			if (GROUP(och) && GROUP_LEADER(GROUP(och))) {
				gsize += snprintf(group_data + gsize, sizeof(group_data) - gsize, "G %d %d\n", GET_IDNUM(och), GET_IDNUM(GROUP_LEADER(GROUP(och))));
			}
		}
		
		// send output
		write_to_descriptor(desc->descriptor, buf);
		if (reboot_control.type == SCMD_REBOOT) {
			write_to_descriptor(desc->descriptor, reboot_strings[number(0, num_of_reboot_strings - 1)]);
		}
		
		SAVE_CHAR(och);
		
		// extract is not actually necessary since we're rebooting, right?
		// extract_all_items(och);
		// extract_char(och);
	}

	if (reboot_control.type == SCMD_REBOOT && fl) {
		fprintf(fl, "-1 ~ ~\n");
		fprintf(fl, "%s", group_data);
		fprintf(fl, "$\n");
		fclose(fl);
	}

	// If this is a reboot, restart the mud!
	if (reboot_control.type == SCMD_REBOOT) {
		log("Reboot: performing live reboot");
		
		chdir("..");
				
		// rotate logs -- note: you should also update the autorun script
		system("fgrep \"ABUSE:\" syslog >> log/abuse");
		system("fgrep \"BAN:\" syslog >> log/ban");
		system("fgrep \"CONFIG:\" syslog >> log/config");
		system("fgrep \"GC:\" syslog >> log/godcmds");
		system("fgrep \"VALID:\" syslog >> log/validation");
		system("fgrep \"DEATH:\" syslog >> log/rip");
		system("fgrep \"BAD PW:\" syslog >> log/badpw");
		system("fgrep \"DEL:\" syslog >> log/delete");
		system("fgrep \"NEW:\" syslog >> log/newplayers");
		system("fgrep \"SYSERR:\" syslog >> log/syserr");
		system("fgrep \"LVL:\" syslog >> log/levels");
		system("fgrep \"OLC:\" syslog >> log/olc");
		system("fgrep \"DIPL:\" syslog >> log/diplomacy");
		system("fgrep \"SCRIPT ERR:\" syslog >> log/scripterr");
		system("cp syslog log/syslog.old");
		system("echo 'Rebooting EmpireMUD...' > syslog");
		
		sprintf(buf, "%d", port);
		sprintf(buf2, "-C%d", mother_desc);
		
		// TODO: should support more of the extra options we might have started up with
		if (no_auto_deletes) {
			execl("bin/empire", "empire", buf2, "-q", buf, (char *) NULL);
		}
		else {
			execl("bin/empire", "empire", buf2, buf, (char *) NULL);
		}

		// If that failed we're still here?
		perror("reboot: execl");
		exit(1);
	}
	// Otherwise it's a shutdown
	else {
		empire_shutdown = 1;
		switch (reboot_control.level) {
			case SHUTDOWN_DIE: {
				touch(KILLSCRIPT_FILE);
				log("Shutdown die: mud will not reboot");
				break;
			}
			case SHUTDOWN_PAUSE: {
				touch(PAUSE_FILE);
				log("Shutdown pause: mud will not reboot until '%s' is removed", PAUSE_FILE);
				break;
			}
			default: {
				log("Shutting down: the mud will reboot shortly");
				break;
			}
		}
		
		// done!
		exit(0);
	}
}


/**
* Updates the reboot timer, if set, and executes a reboot / shutdown. This
* should be called every minute.
*/
void update_reboot(void) {
	extern const char *reboot_type[];
	extern int wizlock_level;
	extern char *wizlock_message;
	
	char buf[MAX_STRING_LENGTH];
	
	// neverboot
	if (reboot_control.time < 0) {
		return;
	}
	
	// otherwise...
	reboot_control.time -= 1;

	if (reboot_control.time <= 0 || reboot_control.immediate || (check_reboot_confirms() && reboot_control.time <= 15)) {
		perform_reboot();
		return;
	}
	
	// wizlock last 5 minutes
	if (reboot_control.time <= 5) {
		wizlock_level = 1;	// newbie lock
		sprintf(buf, "This mud is preparing to %s. The %s will happen in about %d minutes.", reboot_type[reboot_control.type], reboot_type[reboot_control.type], reboot_control.time);
		wizlock_message = str_dup(buf);
	}
	
	// alert everyone
	if (reboot_control.time <= 5 || (reboot_control.time <= 15 && (reboot_control.time % 2))) {
		syslog(SYS_SYSTEM, 0, FALSE, "The mud will %s in %d minute%s (type 'confirm' to %s faster)", reboot_type[reboot_control.type], reboot_control.time, PLURAL(reboot_control.time), reboot_type[reboot_control.type]);
		mortlog("The mud will %s in %d minute%s (type 'confirm' to %s faster)", reboot_type[reboot_control.type], reboot_control.time, PLURAL(reboot_control.time), reboot_type[reboot_control.type]);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MAIN GAME LOOP //////////////////////////////////////////////////////////

void heartbeat(int heart_pulse) {
	void check_death_respawn();
	void check_expired_cooldowns();
	void check_idle_passwords();
	void check_newbie_islands();
	void check_progress_refresh();
	void check_wars();
	void chore_update();
	void display_automessages();
	void extract_pending_chars();
	void free_freeable_triggers();
	void frequent_combat(int pulse);
	void generate_adventure_instances();
	void output_map_to_file();
	void point_update();
	void process_import_evolutions();
	void process_imports();
	void process_theft_logs();
	void prune_instances();
	void real_update();
	void reduce_city_overages();
	void reduce_outside_territory();
	void reduce_stale_empires();
	void reset_instances();
	void run_delayed_refresh();
	void run_external_evolutions();
	void run_mob_echoes();
	void sanity_check();
	void save_data_table(bool force);
	void save_marked_empires();
	void update_actions();
	void update_empire_npc_data();
	void update_guard_towers();
	void update_instance_world_size();
	void update_players_online_stats();
	void update_trading_post();
	void weather_and_time(int mode);
	void write_running_events_file();

	static int mins_since_crashsave = 0;
	bool debug_log = FALSE;
	
	#define HEARTBEAT(x)  !(heart_pulse % (int)((x) * PASSES_PER_SEC))
	
	// TODO go through this, arrange it better, combine anything combinable

	// only get a gain condition message on the hour
	if (HEARTBEAT(SECS_PER_MUD_HOUR)) {
		gain_cond_message = TRUE;
	}
	
	dg_event_process();

	// this is meant to be slightly longer than the mobile_activity pulse, and is mentioned in help files
	if (HEARTBEAT(13)) {
		script_trigger_check();
		if (debug_log && HEARTBEAT(15)) { log("debug  2:\t%lld", microtime()); }
	}

	if (HEARTBEAT(0.2)) {
		update_actions();		
		if (debug_log && HEARTBEAT(15)) { log("debug  3:\t%lld", microtime()); }
	}
	if (HEARTBEAT(1)) {
		check_expired_cooldowns();	// descriptor list
		if (debug_log && HEARTBEAT(15)) { log("debug  4:\t%lld", microtime()); }
	}

	if (HEARTBEAT(3)) {
		update_guard_towers();
		if (debug_log && HEARTBEAT(15)) { log("debug  5:\t%lld", microtime()); }
	}
	
	if (HEARTBEAT(30)) {
		sanity_check();
		if (debug_log && HEARTBEAT(15)) { log("debug  6:\t%lld", microtime()); }
	}

	if (HEARTBEAT(15)) {
		check_idle_passwords();
		if (debug_log && HEARTBEAT(15)) { log("debug  7:\t%lld", microtime()); }
		check_death_respawn();
		if (debug_log && HEARTBEAT(15)) { log("debug  7.5:\t%lld", microtime()); }
		run_mob_echoes();
		if (debug_log && HEARTBEAT(15)) { log("debug  7.6:\t%lld", microtime()); }
	}

	if (HEARTBEAT(30)) {
		update_players_online_stats();
		if (debug_log && HEARTBEAT(15)) { log("debug  9:\t%lld", microtime()); }
	}

	if (HEARTBEAT(10)) {
		mobile_activity();
		if (debug_log && HEARTBEAT(15)) { log("debug 10:\t%lld", microtime()); }
	}

	// TODO won't the macro work here?
	if (!(heart_pulse % (int)(0.1 * PASSES_PER_SEC))) {
		frequent_combat(heart_pulse);
		if (debug_log && HEARTBEAT(15)) { log("debug 11:\t%lld", microtime()); }
	}
	
	if (HEARTBEAT(SECS_PER_MUD_HOUR)) {
		point_update();
		if (debug_log && HEARTBEAT(15)) { log("debug 12:\t%lld", microtime()); }
		process_theft_logs();
		if (debug_log && HEARTBEAT(15)) { log("debug 12.1:\t%lld", microtime()); }
	}
	else if (HEARTBEAT(SECS_PER_REAL_UPDATE)) {
		// only call real_update if we didn't also point_update
		real_update();
		if (debug_log && HEARTBEAT(15)) { log("debug 13:\t%lld", microtime()); }
	}

	if (HEARTBEAT(SECS_PER_MUD_HOUR)) {
		weather_and_time(1);
		if (debug_log && HEARTBEAT(15)) { log("debug 14a:\t%lld", microtime()); }
		chore_update();
		if (debug_log && HEARTBEAT(15)) { log("debug 14b:\t%lld", microtime()); }
		
		// save the world at dawn
		if (time_info.hours == 7) {
			save_whole_world();
			if (debug_log && HEARTBEAT(15)) { log("debug 14c:\t%lld", microtime()); }
		}
	}
	
	// slightly off the hour to prevent yet another thing on the tick
	if (HEARTBEAT(SECS_PER_MUD_HOUR+1)) {
		update_empire_npc_data();
		if (debug_log && HEARTBEAT(15)) { log("debug 15:\t%lld", microtime()); }
	}
	
	if (HEARTBEAT(SECS_PER_REAL_MIN)) {
		check_wars();
		if (debug_log && HEARTBEAT(15)) { log("debug 16:\t%lld", microtime()); }
		reset_instances();
		if (debug_log && HEARTBEAT(15)) { log("debug 17:\t%lld", microtime()); }
	}
	
	if (HEARTBEAT(15 * SECS_PER_REAL_MIN)) {
		output_map_to_file();
		if (debug_log && HEARTBEAT(15)) { log("debug 18:\t%lld", microtime()); }
	}

	if (HEARTBEAT(SECS_PER_REAL_MIN)) {
		update_reboot();
		if (debug_log && HEARTBEAT(15)) { log("debug 19a:\t%lld", microtime()); }
		if (++mins_since_crashsave >= 5) {
			mins_since_crashsave = 0;
			save_all_players();
			if (debug_log && HEARTBEAT(15)) { log("debug 19b:\t%lld", microtime()); }
		}
		
		display_automessages();
		if (debug_log && HEARTBEAT(15)) { log("debug 19c:\t%lld", microtime()); }
	}
	
	if (HEARTBEAT(12 * SECS_PER_REAL_HOUR)) {
		reduce_city_overages();
		if (debug_log && HEARTBEAT(15)) { log("debug 20:\t%lld", microtime()); }
		check_newbie_islands();
		if (debug_log && HEARTBEAT(15)) { log("debug 20.5:\t%lld", microtime()); }
	}
	
	if (HEARTBEAT(SECS_PER_REAL_HOUR)) {
		reduce_stale_empires();
		if (debug_log && HEARTBEAT(15)) { log("debug 21:\t%lld", microtime()); }
	}
	
	if (HEARTBEAT(30 * SECS_PER_REAL_MIN)) {
		reduce_outside_territory();
		if (debug_log && HEARTBEAT(15)) { log("debug 22:\t%lld", microtime()); }
	}
	
	if (HEARTBEAT(3 * SECS_PER_REAL_MIN)) {
		generate_adventure_instances();
		if (debug_log && HEARTBEAT(15)) { log("debug 23:\t%lld", microtime()); }
	}
	
	if (HEARTBEAT(5 * SECS_PER_REAL_MIN)) {
		prune_instances();
		if (debug_log && HEARTBEAT(15)) { log("debug 24:\t%lld", microtime()); }
		update_trading_post();
		if (debug_log && HEARTBEAT(15)) { log("debug 24.5:\t%lld", microtime()); }
	}
	
	if (HEARTBEAT(SECS_PER_MUD_HOUR)) {
		if (time_info.hours == 0) {
			run_external_evolutions();
			if (debug_log && HEARTBEAT(15)) { log("debug 25:\t%lld", microtime()); }
		}
		if (time_info.hours == 12) {
			process_imports();
			if (debug_log && HEARTBEAT(15)) { log("debug 25.5:\t%lld", microtime()); }
		}
	}
	
	if (HEARTBEAT(1)) {
		if (data_table_needs_save) {
			save_data_table(FALSE);
			if (debug_log && HEARTBEAT(15)) { log("debug 26:\t%lld", microtime()); }
		}
		if (events_need_save) {
			write_running_events_file();
			if (debug_log && HEARTBEAT(15)) { log("debug 26.5:\t%lld", microtime()); }
		}
		save_marked_empires();
		if (debug_log && HEARTBEAT(15)) { log("debug 27:\t%lld", microtime()); }
	}
	
	if (HEARTBEAT(SECS_PER_REAL_DAY)) {
		clean_empire_offenses();
		if (debug_log && HEARTBEAT(15)) { log("debug 28:\t%lld", microtime()); }
		update_instance_world_size();
		if (debug_log && HEARTBEAT(15)) { log("debug 28.2:\t%lld", microtime()); }
	}
	
	// check if we've been asked to import new evolutions
	if (do_evo_import) {
		do_evo_import = FALSE;
		process_import_evolutions();
		if (debug_log && HEARTBEAT(15)) { log("debug 28.5:\t%lld", microtime()); }
	}
	
	// this goes roughly last -- update MSDP users
	if (HEARTBEAT(1)) {
		msdp_update();
		if (debug_log && HEARTBEAT(15)) { log("debug 29:\t%lld", microtime()); }
		check_progress_refresh();
		if (debug_log && HEARTBEAT(15)) { log("debug 30:\t%lld", microtime()); }
		run_delayed_refresh();
		if (debug_log && HEARTBEAT(15)) { log("debug 31:\t%lld", microtime()); }
	}

	/* Every pulse! Don't want them to stink the place up... */
	extract_pending_chars();
	free_freeable_triggers();

	/* Turn this off */
	gain_cond_message = FALSE;
	
	// prevent accidentally leaving this on
	pause_affect_total = FALSE;
	
	// check for immediate reboot
	if (reboot_control.immediate == TRUE) {
		perform_reboot();
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MESSAGING ///////////////////////////////////////////////////////////////

void act(const char *str, int hide_invisible, char_data *ch, const void *obj, const void *vict_obj, bitvector_t act_flags) {
	extern bool is_ignoring(char_data *ch, char_data *victim);

	char_data *to = NULL;
	bool to_sleeping = FALSE, no_dark = FALSE, is_spammy = FALSE;

	if (!str || !*str) {
		return;
	}
	
	/* If the bit is set, unset dg_act_check, thus the ! below */
	dg_act_check = !IS_SET(act_flags, DG_NO_TRIG);

	if (IS_SET(act_flags, TO_SLEEP)) {
		to_sleeping = TRUE;
	}
	
	if (IS_SET(act_flags, TO_SPAMMY)) {
		is_spammy = TRUE;
	}

	if (IS_SET(act_flags, TO_NODARK)) {
		Global_ignore_dark = no_dark = TRUE;
	}

	/* To the character */
	if (IS_SET(act_flags, TO_CHAR) && ch && SENDOK(ch)) {
		perform_act(str, ch, obj, vict_obj, ch, act_flags);
	}

	/* To the victim */
	if (IS_SET(act_flags, TO_VICT) && (to = (char_data*) vict_obj) != NULL && SENDOK(to) && (!IS_SET(act_flags, TO_NOT_IGNORING) || !is_ignoring(to, ch))) {
		perform_act(str, ch, obj, vict_obj, to, act_flags);
	}

	if (IS_SET(act_flags, TO_NOTVICT | TO_ROOM)) {
		if (ch && IN_ROOM(ch)) {
			to = ROOM_PEOPLE(IN_ROOM(ch));
		}
		else if (!IS_SET(act_flags, ACT_VEHICLE_OBJ) && obj && IN_ROOM((obj_data*)obj)) {
			to = ROOM_PEOPLE(IN_ROOM((obj_data*)obj));
		}
		else if (IS_SET(act_flags, ACT_VEHICLE_OBJ) && obj && IN_ROOM((vehicle_data*)obj)) {
			to = ROOM_PEOPLE(IN_ROOM((vehicle_data*)obj));
		}
		
		if (to) {
			for (; to; to = to->next_in_room) {
				if (!SENDOK(to) || (to == ch))
					continue;
				if (IS_SET(act_flags, TO_NOT_IGNORING) && is_ignoring(to, ch)) {
					continue;
				}
				if (hide_invisible && ch && !CAN_SEE(to, ch))
					continue;
				if (IS_SET(act_flags, TO_NOTVICT) && to == vict_obj)
					continue;
				if (ch && !WIZHIDE_OK(to, ch)) {
					continue;
				}
				perform_act(str, ch, obj, vict_obj, to, act_flags);
			}
		}
	}
	Global_ignore_dark = FALSE;
}


/**
* Identical to msg_to_char except takes a descriptor.
*
* @param descriptor_data *d The player's descriptor.
* @param const char *messg... va_arg format.
*/
void msg_to_desc(descriptor_data *d, const char *messg, ...) {
	char output[MAX_STRING_LENGTH];
	va_list tArgList;
	
	if (!messg || !d) {
		return;
	}
	
	va_start(tArgList, messg);
	vsprintf(output, messg, tArgList);

	SEND_TO_Q(output, d);

	va_end(tArgList);
}


/**
* EmpireMUD implementation of send_to_char with va_args. In CircleMUD 3.1,
* they changed the argument order on send_to_char to match this.
*
* @param char_data *ch The player.
* @param const char *messg... va_arg format.
*/
void msg_to_char(char_data *ch, const char *messg, ...) {
	char output[MAX_STRING_LENGTH];
	va_list tArgList;
	
	if (!messg || !ch->desc)
		return;
	
	va_start(tArgList, messg);
	vsprintf(output, messg, tArgList);

	SEND_TO_Q(output, ch->desc);

	va_end(tArgList);
}


/**
* Sends a message to everyone in/on the vehicle.
*
* @param vehicle_data *veh The vehicle to send to.
* @param bool awake_only If TRUE, only sends to people who aren't sleeping.
* @param const char *messg... va_arg format.
*/
void msg_to_vehicle(vehicle_data *veh, bool awake_only, const char *messg, ...) {
	char output[MAX_STRING_LENGTH];
	descriptor_data *desc;
	va_list tArgList;
	char_data *ch;
	
	if (!messg || !veh) {
		return;
	}
	
	va_start(tArgList, messg);
	vsprintf(output, messg, tArgList);
	
	LL_FOREACH(descriptor_list, desc) {
		if (STATE(desc) != CON_PLAYING || !(ch = desc->character)) {
			continue;
		}
		if (VEH_SITTING_ON(veh) != ch && GET_ROOM_VEHICLE(IN_ROOM(ch)) != veh) {
			continue;
		}
		if (awake_only && GET_POS(ch) <= POS_SLEEPING && GET_POS(ch) != POS_DEAD) {
			continue;
		}
		
		// looks valid
		SEND_TO_Q(output, desc);
	}
	
	va_end(tArgList);
}


/**
* Sends olc audit info to the player.
*
* @param char_data *ch The player.
* @param any_vnum vnum The vnum we're reporting on.
* @param const char *messg... va arg format.
*/
void olc_audit_msg(char_data *ch, any_vnum vnum, const char *messg, ...) {
	char output[MAX_STRING_LENGTH];
	va_list tArgList;
	
	if (!messg || !ch->desc) {
		return;
	}
	
	va_start(tArgList, messg);
	vsprintf(output, messg, tArgList);
	msg_to_desc(ch->desc, "[%5d] %s\r\n", vnum, output);
	va_end(tArgList);
}


void send_to_all(const char *messg, ...) {
	descriptor_data *i;
	char output[MAX_STRING_LENGTH];
	va_list tArgList;

	if (!messg)
		return;

	va_start(tArgList, messg);
	vsprintf(output, messg, tArgList);

	for (i = descriptor_list; i; i = i->next)
		if (STATE(i) == CON_PLAYING)
			SEND_TO_Q(output, i);
	va_end(tArgList);
}


/* higher-level communication: the act() function */
void perform_act(const char *orig, char_data *ch, const void *obj, const void *vict_obj, const char_data *to, bitvector_t act_flags) {
	extern char *get_vehicle_short_desc(vehicle_data *veh, char_data *to);
	extern bool is_fight_ally(char_data *ch, char_data *frenemy);
	
	const char *i = NULL;
	char *buf, lbuf[MAX_STRING_LENGTH], *dg_arg = NULL;
	bool real_ch = FALSE, real_vict = FALSE;
	char_data *dg_victim = NULL;
	obj_data *dg_target = NULL;
	bool show, any;
	int iter;

	const char *ACTNULL = "<NULL>";
	#define CHECK_NULL(pointer, expression)  if ((pointer) == NULL) i = ACTNULL; else i = (expression);
	
	// check fight messages (may exit early)
	if (!IS_NPC(to) && ch != vict_obj && IS_SET(act_flags, TO_COMBAT_HIT | TO_COMBAT_MISS)) {
		show = any = FALSE;
		// hits
		if (IS_SET(act_flags, TO_COMBAT_HIT)) {
			if (!show && to == ch) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_MY_HITS);
			}
			if (!show && to == vict_obj) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_HITS_AGAINST_ME);
			}
			if (!show && to != ch && is_fight_ally((char_data*)to, (char_data*)ch)) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_ALLY_HITS);
			}
			if (!show && to != ch && to != vict_obj && is_fight_ally((char_data*)to, (char_data*)vict_obj)) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_HITS_AGAINST_ALLIES);
			}
			if (!show && to != ch && FIGHTING(to) == vict_obj) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_HITS_AGAINST_TARGET);
			}
			if (!show && to != ch && FIGHTING(to) && FIGHTING(FIGHTING(to)) == vict_obj) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_HITS_AGAINST_TANK);
			}
			if (!show && !any) {
				show |= SHOW_FIGHT_MESSAGES(to, FM_OTHER_HITS);
			}
		}
		// misses
		if (IS_SET(act_flags, TO_COMBAT_MISS)) {
			if (!show && to == ch) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_MY_MISSES);
			}
			if (!show && to == vict_obj) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_MISSES_AGAINST_ME);
			}
			if (!show && to != ch && is_fight_ally((char_data*)to, (char_data*)ch)) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_ALLY_MISSES);
			}
			if (!show && to != ch && to != vict_obj && is_fight_ally((char_data*)to, (char_data*)vict_obj)) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_MISSES_AGAINST_ALLIES);
			}
			if (!show && to != ch && FIGHTING(to) == vict_obj) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_MISSES_AGAINST_TARGET);
			}
			if (!show && to != ch && FIGHTING(to) && FIGHTING(FIGHTING(to)) == vict_obj) {
				any = TRUE;
				show |= SHOW_FIGHT_MESSAGES(to, FM_MISSES_AGAINST_TANK);
			}
			if (!show && !any) {
				show |= SHOW_FIGHT_MESSAGES(to, FM_OTHER_MISSES);
			}
		}
		
		// are we supposed to show it?
		if (!show) {
			return;
		}
	}
	
	// prepare the message
	buf = lbuf;
	for (;;) {
		if (*orig == '$') {
			switch (*(++orig)) {
				case 'n':
					i = PERS(ch, (char_data*)to, FALSE);
					break;
				case 'N':
					CHECK_NULL(vict_obj, PERS((char_data*)vict_obj,(char_data*)to, FALSE));
					dg_victim = (char_data*) vict_obj;
					break;
				case 'o':
					i = PERS(ch, (char_data*)to, TRUE);
					real_ch = TRUE;
					break;
				case 'O':
					CHECK_NULL(vict_obj, PERS((char_data*)vict_obj, (char_data*)to, TRUE));
					dg_victim = (char_data*) vict_obj;
					real_vict = TRUE;
					break;
				case 'm':
					i = real_ch ? REAL_HMHR(ch) : HMHR(ch);
					break;
				case 'M':
					CHECK_NULL(vict_obj, (real_vict ? REAL_HMHR((char_data*) vict_obj) : HMHR((char_data*) vict_obj)));
					dg_victim = (char_data*) vict_obj;
					break;
				case 's':
					i = real_ch ? REAL_HSHR(ch) : HSHR(ch);
					break;
				case 'S':
					CHECK_NULL(vict_obj, (real_vict ? REAL_HSHR((char_data*) vict_obj) : HSHR((char_data*) vict_obj)));
					dg_victim = (char_data*) vict_obj;
					break;
				case 'e':
					i = real_ch ? REAL_HSSH(ch) : HSSH(ch);
					break;
				case 'E':
					CHECK_NULL(vict_obj, (real_vict ? REAL_HSSH((char_data*) vict_obj) : HSSH((char_data*) vict_obj)));
					dg_victim = (char_data*) vict_obj;
					break;
				case 'p':
					CHECK_NULL(obj, OBJS((obj_data*)obj, (char_data*)to));
					break;
				case 'P':
					CHECK_NULL(vict_obj, OBJS((obj_data*) vict_obj, (char_data*)to));
					dg_target = (obj_data*) vict_obj;
					break;
				case 'a':
					CHECK_NULL(obj, SANA((obj_data*)obj));
					break;
				case 'A':
					CHECK_NULL(vict_obj, SANA((const obj_data*) vict_obj));
					dg_target = (obj_data*) vict_obj;
					break;
				case 'T':
					CHECK_NULL(vict_obj, (const char *) vict_obj);
					dg_arg = (char *) vict_obj;
					break;
				case 't':
					CHECK_NULL(obj, (char *) obj);
					break;
				case 'F':
					CHECK_NULL(vict_obj, fname((const char *) vict_obj));
					break;
				case 'v': {	// $v: vehicle -- you need to pass ACT_VEHICLE_OBJ to use this
					CHECK_NULL(obj, get_vehicle_short_desc((vehicle_data*)obj, (char_data*)to));
					break;
				}
				case 'V': {	// $V: vehicle
					CHECK_NULL(vict_obj, get_vehicle_short_desc((vehicle_data*)vict_obj, (char_data*)to));
					break;
				}
				case '$':
					i = "$";
					break;
				default: {
					if (!IS_SET(act_flags, TO_IGNORE_BAD_CODE)) {
						log("SYSERR: Illegal $-code to act(): %c", *orig);
						log("SYSERR: %s", orig);
						i = "";
					}
					else {
						i = "$?";
					}
					break;
				}
			}
			while ((*buf = *(i++)))
				buf++;
			orig++;
			}
		else if (!(*(buf++) = *(orig++)))
			break;
	}

	*(--buf) = '\r';
	*(++buf) = '\n';
	*(++buf) = '\0';
	
	// find the first non-color-code and cap it
	for (iter = 0; iter < strlen(lbuf); ++iter) {
		if (lbuf[iter] == '&') {
			// skip
			++iter;
		}
		else {
			// found one!
			lbuf[iter] = UPPER(lbuf[iter]);
			break;
		}
	}

	if (to->desc) {
		if (to->desc->last_act_message) {
			free(to->desc->last_act_message);
		}
		to->desc->last_act_message = strdup(lbuf);
		
		if (IS_SET(act_flags, TO_QUEUE)) {
			stack_simple_msg_to_desc(to->desc, lbuf);
		}
		else {	// send normally
			SEND_TO_Q(lbuf, to->desc);
		}
	}

	if ((IS_NPC(to) && dg_act_check) && (to != ch)) {
		act_mtrigger(to, lbuf, ch, dg_victim, IS_SET(act_flags, ACT_VEHICLE_OBJ) ? NULL : (obj_data*)obj, dg_target, dg_arg);
	}
}

// this is the pre-circle3.1 send_to_char that doesn't have va_args
void send_to_char(const char *messg, char_data *ch) {
	if (ch->desc && messg)
		SEND_TO_Q(messg, ch->desc);
}


/**
* Sends a message to the entire group, except for ch.
* Send 'ch' as NULL, if you want to message to reach
* everyone. -Vatiken
*
* @param char_data *ch Optional: person to skip (e.g. the sender).
* @param struct group_data *group The group to send to.
* @param const char *msg... Message string/formatting.
*/
void send_to_group(char_data *ch, struct group_data *group, const char *msg, ...) {
	char output[MAX_STRING_LENGTH];
	struct group_member_data *mem;
	char_data *tch;
	va_list args;

	if (msg == NULL)
		return;
		
	for (mem = group->members; mem; mem = mem->next) {
		tch = mem->member;
		if (tch != ch && !IS_NPC(tch) && tch->desc && STATE(tch->desc) == CON_PLAYING) {
			va_start(args, msg);
			vsprintf(output, msg, args);
			
			if (!IS_NPC(tch) && GET_CUSTOM_COLOR(tch, CUSTOM_COLOR_GSAY)) {
				msg_to_desc(tch->desc, "\t%c[group] %s\t0\r\n", GET_CUSTOM_COLOR(tch, CUSTOM_COLOR_GSAY), output);
			}
			else {
				msg_to_desc(tch->desc, "[\tGgroup\t0] %s\t0\r\n", output);
			}
			va_end(args);
		}
	}
}


/**
* Sends a message to all outdoor players.
*
* @param bool weather If TRUE, ignores players in !WEATHER rooms.
* @param const char *messg... The string to send.
*/
void send_to_outdoor(bool weather, const char *messg, ...) {
	descriptor_data *i;
	va_list tArgList;
	char output[MAX_STRING_LENGTH];

	if (!messg || !*messg)
		return;

	va_start(tArgList, messg);
	vsprintf(output, messg, tArgList);

	for (i = descriptor_list; i; i = i->next) {
		if (STATE(i) != CON_PLAYING || i->character == NULL)
			continue;
		if (!AWAKE(i->character) || !IS_OUTDOORS(i->character))
			continue;
		if (weather && ROOM_AFF_FLAGGED(IN_ROOM(i->character), ROOM_AFF_NO_WEATHER)) {
			continue;
		}
		SEND_TO_Q(output, i);
	}
	va_end(tArgList);
}


void send_to_room(const char *messg, room_data *room) {
	char_data *i;

	if (messg == NULL)
		return;

	for (i = ROOM_PEOPLE(room); i; i = i->next_in_room)
		if (i->desc)
			SEND_TO_Q(messg, i->desc);
}


/**
* Flushes a descriptor's stacked messages, adding (x2) where needed.
*
* @param descriptor_data *desc The descriptor to send the messages to.
*/
void send_stacked_msgs(descriptor_data *desc) {
	char output[MAX_STRING_LENGTH+24];
	struct stack_msg *iter, *next_iter;
	int len, rem;
	
	if (!desc) {
		return;
	}
	
	LL_FOREACH_SAFE(desc->stack_msg_list, iter, next_iter) {
		if (iter->count > 1) {
			// deconstruct to add the (x2)
			len = strlen(iter->string);
			rem = (len > 1 && ISNEWL(iter->string[len-1])) ? 1 : 0;
			rem += (len > 2 && ISNEWL(iter->string[len-2])) ? 1 : 0;
			// rebuild
			snprintf(output, sizeof(output), "%*.*s (x%d)%s", (len-rem), (len-rem), iter->string, iter->count, (rem > 0 ? "\r\n" : ""));
			SEND_TO_Q(output, desc);
		}
		else {
			SEND_TO_Q(NULLSAFE(iter->string), desc);
		}
		
		// free it up
		LL_DELETE(desc->stack_msg_list, iter);
		if (iter->string) {
			free(iter->string);
		}
		free(iter);
	}
	
	desc->stack_msg_list = NULL;
}


/**
* Similar to msg_to_desc, but the message is put in a queue for stacking and
* then sent on a very short delay. If more than one identical message is sent
* in this time, it stacks with (x2).
*
* @param descriptor_data *desc The player.
* @param const char *messg... va_arg format.
*/
void stack_msg_to_desc(descriptor_data *desc, const char *messg, ...) {
	char output[MAX_STRING_LENGTH];
	va_list tArgList;
	
	if (!messg || !desc) {
		return;
	}
	
	va_start(tArgList, messg);
	vsprintf(output, messg, tArgList);
	va_end(tArgList);
	stack_simple_msg_to_desc(desc, output);
}


/**
* Similar to msg_to_desc, but the message is put in a queue for stacking and
* then sent on a very short delay. If more than one identical message is sent
* in this time, it stacks with (x2).
*
* @param descriptor_data *desc The player.
* @param const char *messg A string to send.
*/
void stack_simple_msg_to_desc(descriptor_data *desc, const char *messg) {
	struct stack_msg *iter, *stm;
	bool found = FALSE;
	
	if (!messg || !desc) {
		return;
	}
	
	// look in queue
	LL_FOREACH(desc->stack_msg_list, iter) {
		if (!strcmp(iter->string, messg)) {
			++iter->count;
			found = TRUE;
			break;
		}
	}
	
	// add
	if (!found) {
		CREATE(stm, struct stack_msg, 1);
		stm->string = str_dup(messg);
		stm->count = 1;
		LL_APPEND(desc->stack_msg_list, stm);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// SOCKETS /////////////////////////////////////////////////////////////////


void close_socket(descriptor_data *d) {
	struct stack_msg *stacked;
	descriptor_data *temp;

	REMOVE_FROM_LIST(d, descriptor_list, next);
	CLOSE_SOCKET(d->descriptor);
	flush_queues(d);

	/* Forget snooping */
	if (d->snooping)
		d->snooping->snoop_by = NULL;

	if (d->snoop_by) {
		SEND_TO_Q("Your victim is no longer among us.\r\n", d->snoop_by);
		d->snoop_by->snooping = NULL;
	}

	if (d->character) {
		/*
		 * Plug memory leak, from Eric Green.
		 * Note: only free if it's a type that isn't editing a live string -pc
		 */
		if (!IS_NPC(d->character) && PLR_FLAGGED(d->character, PLR_MAILING) && d->str) {
			if (*(d->str)) {
				free(*(d->str));
			}
			free(d->str);
			d->str = NULL;
		}
		if (STATE(d) == CON_PLAYING || STATE(d) == CON_DISCONNECT) {
			act("$n has lost $s link.", TRUE, d->character, 0, 0, TO_ROOM);
			if (!IS_NPC(d->character)) {
				SAVE_CHAR(d->character);
				syslog(SYS_LOGIN, GET_INVIS_LEV(d->character), TRUE, "Closing link to: %s at %s", GET_NAME(d->character), IN_ROOM(d->character) ? room_log_identifier(IN_ROOM(d->character)) : "an unknown location");
			}
			d->character->desc = NULL;
		}
		else {
			syslog(SYS_LOGIN, 0, TRUE, "Losing player: %s", GET_NAME(d->character) ? GET_NAME(d->character) : "<null>");
			free_char(d->character);
		}
	}
	else {
		if (config_get_bool("log_losing_descriptor_without_char")) {
			syslog(SYS_LOGIN, 0, TRUE, "Losing descriptor without char");
		}
	}

	/* JE 2/22/95 -- part of my unending quest to make switch stable */
	if (d->original && d->original->desc)
		d->original->desc = NULL;

	/* Clear the command history. */
	if (d->history) {
		int cnt;
		for (cnt = 0; cnt < HISTORY_SIZE; cnt++)
			if (d->history[cnt])
				free(d->history[cnt]);
		free(d->history);
	}

	if (d->showstr_head)
		free(d->showstr_head);
	if (d->showstr_count)
		free(d->showstr_vector);
	if (d->backstr) {
		free(d->backstr);
	}
	// do NOT free d->str_on_abort (is a pointer to something else)
	
	// other strings
	if (d->host) {
		free(d->host);
	}
	if (d->last_act_message) {
		free(d->last_act_message);
	}
	if (d->file_storage) {
		free(d->file_storage);
	}
	
	// leftover stacked messages
	while ((stacked = d->stack_msg_list)) {
		d->stack_msg_list = stacked->next;
		
		if (stacked->string) {
			free(stacked->string);
		}
		free(stacked);
	}
	
	ProtocolDestroy(d->pProtocol);

	// OLC_x: olc data
	if (d->olc_storage) {
		free(d->olc_storage);
	}
	if (d->olc_ability) {
		free_ability(d->olc_ability);
	}
	if (d->olc_adventure) {
		free_adventure(d->olc_adventure);
	}
	if (d->olc_archetype) {
		free_archetype(d->olc_archetype);
	}
	if (d->olc_augment) {
		free_augment(d->olc_augment);
	}
	if (d->olc_book) {
		free_book(d->olc_book);
	}
	if (d->olc_craft) {
		free_craft(d->olc_craft);
	}
	if (d->olc_object) {
		free_obj(d->olc_object);
	}
	if (d->olc_mobile) {
		free_char(d->olc_mobile);
	}
	if (d->olc_morph) {
		free_morph(d->olc_morph);
	}
	if (d->olc_progress) {
		free_progress(d->olc_progress);
	}
	if (d->olc_building) {
		free_building(d->olc_building);
	}
	if (d->olc_crop) {
		free_crop(d->olc_crop);
	}
	if (d->olc_event) {
		free_event(d->olc_event);
	}
	if (d->olc_faction) {
		free_faction(d->olc_faction);
	}
	if (d->olc_generic) {
		free_generic(d->olc_generic);
	}
	if (d->olc_global) {
		free_global(d->olc_global);
	}
	if (d->olc_quest) {
		free_quest(d->olc_quest);
	}
	if (d->olc_room_template) {
		free_room_template(d->olc_room_template);
	}
	if (d->olc_sector) {
		free_sector(d->olc_sector);
	}
	if (d->olc_shop) {
		free_shop(d->olc_shop);
	}
	if (d->olc_social) {
		free_social(d->olc_social);
	}
	if (d->olc_trigger) {
		free_trigger(d->olc_trigger);
	}
	if (d->olc_vehicle) {
		free_vehicle(d->olc_vehicle);
	}
	
	free(d);
}


/* Empty the queues before closing connection */
void flush_queues(descriptor_data *d) {
	char buf2[MAX_STRING_LENGTH];
	int dummy;

	if (d->large_outbuf) {
		d->large_outbuf->next = bufpool;
		bufpool = d->large_outbuf;
	}
	while (get_from_q(&d->input, buf2, &dummy));
}


/*
 * get_bind_addr: Return a struct in_addr that should be used in our
 * call to bind().  If the user has specified a desired binding
 * address, we try to bind to it; otherwise, we bind to INADDR_ANY.
 * Note that inet_aton() is preferred over inet_addr() so we use it if
 * we can.  If neither is available, we always bind to INADDR_ANY.
 */
struct in_addr *get_bind_addr() {
	static struct in_addr bind_addr;

	/* Clear the structure */
	memset((char *) &bind_addr, 0, sizeof(bind_addr));

	/* If DLFT_IP is unspecified, use INADDR_ANY */
	bind_addr.s_addr = htonl(INADDR_ANY);

	/* Put the address that we've finally decided on into the logs */
	if (bind_addr.s_addr == htonl(INADDR_ANY))
		log("Binding to all IP interfaces on this host.");
	else
		log("Binding only to IP address %s", inet_ntoa(bind_addr));

	return (&bind_addr);
}


int get_from_q(struct txt_q *queue, char *dest, int *aliased) {
	struct txt_block *tmp;

	/* queue empty? */
	if (!queue->head)
		return (0);

	tmp = queue->head;
	strcpy(dest, queue->head->text);
	*aliased = queue->head->aliased;
	queue->head = queue->head->next;

	free(tmp->text);
	free(tmp);

	return (1);
}


int get_max_players(void) {
	int max_descs = 0;
	const char *method;

	/*
	 * First, we'll try using getrlimit/setrlimit.  This will probably work
	 * on most systems.  HAS_RLIMIT is defined in sysdep.h.
	 */
#ifdef HAS_RLIMIT
	{
		struct rlimit limit;

		/* find the limit of file descs */
		method = "rlimit";
		if (getrlimit(RLIMIT_NOFILE, &limit) < 0) {
			perror("SYSERR: calling getrlimit");
			exit(1);
		}

		/* set the current to the maximum */
		#ifndef OPEN_MAX
			#define OPEN_MAX limit.rlim_max
		#endif
		limit.rlim_cur = MIN(OPEN_MAX, limit.rlim_max);
		if (setrlimit(RLIMIT_NOFILE, &limit) < 0) {
			perror("SYSERR: calling setrlimit");
			exit(1);
		}

#ifdef RLIM_INFINITY
		if (limit.rlim_max == RLIM_INFINITY)
			max_descs = max_playing + NUM_RESERVED_DESCS;
		else
			max_descs = MIN(max_playing + NUM_RESERVED_DESCS, limit.rlim_max);
#else
		max_descs = MIN(max_playing + NUM_RESERVED_DESCS, limit.rlim_max);
#endif
	}

#elif defined (OPEN_MAX) || defined(FOPEN_MAX)
#if !defined(OPEN_MAX)
#define OPEN_MAX FOPEN_MAX
#endif
	method = "OPEN_MAX";
	max_descs = OPEN_MAX;		/* Uh oh.. rlimit didn't work, but we have
				 * OPEN_MAX */
#elif defined (_SC_OPEN_MAX)
	/*
	 * Okay, you don't have getrlimit() and you don't have OPEN_MAX.  Time to
	 * try the POSIX sysconf() function.  (See Stevens' _Advanced Programming
	 * in the UNIX Environment_).
	 */
	method = "POSIX sysconf";
	errno = 0;

	if ((max_descs = sysconf(_SC_OPEN_MAX)) < 0) {
		if (errno == 0)
			max_descs = max_playing + NUM_RESERVED_DESCS;
		else {
			perror("SYSERR: Error calling sysconf");
			exit(1);
		}
	}
#else
	/* if everything has failed, we'll just take a guess */
	method = "random guess";
	max_descs = max_playing + NUM_RESERVED_DESCS;
#endif

	/* now calculate max _players_ based on max descs */
	max_descs = MIN(max_playing, max_descs - NUM_RESERVED_DESCS);

	if (max_descs <= 0) {
		log("SYSERR: Non-positive max player limit!  (Set at %d using %s).", max_descs, method);
		exit(1);
	}
	log("   Setting player limit to %d using %s.", max_descs, method);
	return (max_descs);
}


void init_descriptor(descriptor_data *newd, int desc) {
	static int last_desc = 0;
	int start;

	newd->descriptor = desc;
	newd->connected = CON_GET_NAME;
	newd->idle_tics = 0;
	newd->output = newd->small_outbuf;
	newd->bufspace = SMALL_BUFSIZE - 1;
	newd->next = descriptor_list;
	newd->login_time = time(0);
	*newd->output = '\0';
	newd->bufptr = 0;
	newd->has_prompt = 0;
	
	newd->save_empire = NOTHING;

	CREATE(newd->history, char *, HISTORY_SIZE);
	newd->pProtocol = ProtocolCreate();
	
	newd->olc_type = 0;
	newd->olc_vnum = NOTHING;
	
	// ensure clean data
	*newd->color.last_fg = '\0';
	*newd->color.last_bg = '\0';
	newd->color.is_clean = FALSE;
	newd->color.is_underline = FALSE;
	*newd->color.want_fg = '\0';
	*newd->color.want_bg = '\0';
	newd->color.want_clean = FALSE;
	newd->color.want_underline = FALSE;
	
	// find a free desc num
	start = last_desc;
	do {
		if (++last_desc == 1000) {
			last_desc = 1;
		}
	} while (desc_num_in_use(last_desc) && last_desc != start);	// prevent infinite loop
	
	newd->desc_num = last_desc;
}


/*
 * init_socket sets up the mother descriptor - creates the socket, sets
 * its options up, binds it, and listens.
 */
socket_t init_socket(ush_int port) {
	socket_t s;
	struct sockaddr_in sa;
	int opt;

	/*
	 * Should the first argument to socket() be AF_INET or PF_INET?  I don't
	 * know, take your pick.  PF_INET seems to be more widely adopted, and
	 * Comer (_Internetworking with TCP/IP_) even makes a point to say that
	 * people erroneously use AF_INET with socket() when they should be using
	 * PF_INET.  However, the man pages of some systems indicate that AF_INET
	 * is correct; some such as ConvexOS even say that you can use either one.
	 * All implementations I've seen define AF_INET and PF_INET to be the same
	 * number anyway, so the point is (hopefully) moot.
	 */

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("SYSERR: Error creating socket");
		exit(1);
	}

#if defined(SO_REUSEADDR)
	opt = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
		perror("SYSERR: setsockopt REUSEADDR");
		exit(1);
	}
#endif

	set_sendbuf(s);

	/*
	 * The GUSI sockets library is derived from BSD, so it defines
	 * SO_LINGER, even though setsockopt() is unimplimented.
	 *	(from Dean Takemori <dean@UHHEPH.PHYS.HAWAII.EDU>)
	 */
#if defined(SO_LINGER)
	{
		struct linger ld;

		ld.l_onoff = 0;
		ld.l_linger = 0;
		if (setsockopt(s, SOL_SOCKET, SO_LINGER, (char *) &ld, sizeof(ld)) < 0)
			perror("SYSERR: setsockopt SO_LINGER");	/* Not fatal I suppose. */
	}
#endif

	/* Clear the structure */
	memset((char *)&sa, 0, sizeof(sa));

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr = *(get_bind_addr());

	if (bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("SYSERR: bind");
		CLOSE_SOCKET(s);
		exit(1);
	}
	nonblock(s);
	listen(s, 5);
	return (s);
}


/**
* @param char *ip The IP address to check.
* @return bool TRUE if we should skip nameserver lookup on this IP.
*/
bool is_slow_ip(char *ip) {
	extern const char *slow_nameserver_ips[];
	
	int iter;
	
	for (iter = 0; *slow_nameserver_ips[iter] != '\n'; ++iter) {
		if (!strncmp(ip, slow_nameserver_ips[iter], strlen(slow_nameserver_ips[iter]))) {
			return TRUE;
		}
	}
	for (iter = 0; iter < num_slow_ips; ++iter) {
		if (!strncmp(ip, detected_slow_ips[iter], strlen(detected_slow_ips[iter]))) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* Allows a player to manipulate their input queue. The input text is what the
* user typed AFTER the '-' trigger character.
*
* Options:
*  no-arg: show queue
*  -: clear whole queue
*  other: removes items from the queue that start with text
*
* @param descriptor_data *desc The user.
* @param char *input The text typed, after the -.
*/
void manipulate_input_queue(descriptor_data *desc, char *input) {
	struct txt_block *iter, *next_iter, *temp;
	bool clear_all, found;

	skip_spaces(&input);

	// show queue?
	if (!*input) {
		if (desc->input.head) {
			msg_to_desc(desc, "Input queue:\r\n");
			for (iter = desc->input.head; iter; iter = iter->next) {
				msg_to_desc(desc, "%s\r\n", iter->text);
			}
		}
		else {
			msg_to_desc(desc, "Your input queue is empty.\r\n");
		}
		return;
	}
	
	// clearing everything?
	clear_all = (*input == '-');
	
	// remove matches from queue
	found = FALSE;
	for (iter = desc->input.head; iter; iter = next_iter) {
		next_iter = iter->next;
		
		if (clear_all || is_abbrev(input, iter->text)) {
			REMOVE_FROM_LIST(iter, desc->input.head, next);
			
			if (!clear_all) {
				msg_to_desc(desc, "Removed: %s\r\n", iter->text);
			}
			
			free(iter->text);
			free(iter);
			found = TRUE;
		}
	}
	
	// find new tail
	if ((iter = desc->input.head)) {
		while (iter->next) {
			iter = iter->next;
		}
		desc->input.tail = iter;
	}
	else {
		desc->input.tail = NULL;
	}
	
	if (clear_all) {
		msg_to_desc(desc, "Input queue cleared.\r\n");
	}
	else if (!found) {
		msg_to_desc(desc, "No matching items in your input queue.\r\n");
	}
	else {
		// messages were sent by the loop
	}
}



int new_descriptor(int s) {
	socket_t desc;
	int sockets_connected = 0;
	socklen_t i;
	descriptor_data *newd;
	struct sockaddr_in peer;
	struct hostent *from;
	bool slow_ip;
	time_t when;

	/* accept the new connection */
	i = sizeof(peer);
	if ((desc = accept(s, (struct sockaddr *) &peer, &i)) == INVALID_SOCKET) {
		perror("SYSERR: accept");
		return (-1);
	}
	/* keep it from blocking */
	nonblock(desc);

	/* set the send buffer size */
	if (set_sendbuf(desc) < 0) {
		CLOSE_SOCKET(desc);
		return (0);
	}

	/* make sure we have room for it */
	for (newd = descriptor_list; newd; newd = newd->next)
		sockets_connected++;

	if (sockets_connected >= max_players) {
		write_to_descriptor(desc, "Sorry, EmpireMUD is full right now... please try again later!\r\n");
		CLOSE_SOCKET(desc);
		return (0);
	}
	/* create a new descriptor */
	CREATE(newd, descriptor_data, 1);
	memset((char *) newd, 0, sizeof(descriptor_data));

	/* find the sitename */
	slow_ip = config_get_bool("nameserver_is_slow") || is_slow_ip(inet_ntoa(peer.sin_addr));
	when = time(0);
	if (slow_ip || !(from = gethostbyaddr((char *) &peer.sin_addr, sizeof(peer.sin_addr), AF_INET))) {
		/* resolution failed */
		if (!slow_ip) {
			char buf[MAX_STRING_LENGTH];
			snprintf(buf, sizeof(buf), "Warning: gethostbyaddr [%s]", inet_ntoa(peer.sin_addr));
			perror(buf);
			
			// did it take longer than 5 seconds to look up?
			if (when + 5 < time(0)) {
				log("- added %s to slow IP list", inet_ntoa(peer.sin_addr));
				add_slow_ip(inet_ntoa(peer.sin_addr));
			}
		}

		/* find the numeric site address */
		newd->host = str_dup((char *)inet_ntoa(peer.sin_addr));
	}
	else {
		newd->host = str_dup(from->h_name);
	}

	/* determine if the site is banned */
	if (isbanned(newd->host) == BAN_ALL) {
		CLOSE_SOCKET(desc);
		syslog(SYS_LOGIN, 0, FALSE, "Connection attempt denied from [%s]", newd->host);
		free(newd);
		return (0);
	}
#if 0
  /*
   * Log new connections - probably unnecessary, but you may want it.
   * Note that your immortals may wonder if they see a connection from
   * your site, but you are wizinvis upon login.
   */
	syslog(SYS_LOGIN, 0, FALSE, "New connection from [%s]", newd->host);
#endif

	init_descriptor(newd, desc);
	newd->has_prompt = 1;

	/* prepend to list */
	newd->next = descriptor_list;
	descriptor_list = newd;
	
	ProtocolNegotiate(newd);
	SEND_TO_Q(intros[number(0, num_intros-1)], newd);

	return (0);
}


/*
 * I tried to universally convert Circle over to POSIX compliance, but
 * alas, some systems are still straggling behind and don't have all the
 * appropriate defines.  In particular, NeXT 2.x defines O_NDELAY but not
 * O_NONBLOCK.  Krusty old NeXT machines!  (Thanks to Michael Jones for
 * this and various other NeXT fixes.)
 */

#ifndef O_NONBLOCK
#define O_NONBLOCK O_NDELAY
#endif

void nonblock(socket_t s) {
	int flags;

	flags = fcntl(s, F_GETFL, 0);
	flags |= O_NONBLOCK;
	if (fcntl(s, F_SETFL, flags) < 0) {
		perror("SYSERR: Fatal error executing nonblock (comm.c)");
		exit(1);
	}
}


/*
 * Same information about perform_socket_write applies here. I like
 * standards, there are so many of them. -gg 6/30/98
 */
ssize_t perform_socket_read(socket_t desc, char *read_point, size_t space_left) {
	ssize_t ret;

	ret = read(desc, read_point, space_left);

	/* Read was successful. */
	if (ret > 0)
		return (ret);

	/* read() returned 0, meaning we got an EOF. */
	if (ret == 0) {
		log("WARNING: EOF on socket read (connection broken by peer)");
		return (-1);
	}

	/*
	 * read returned a value < 0: there was an error
	 */

#ifdef EINTR		/* Interrupted system call - various platforms */
	if (errno == EINTR)
		return (0);
#endif

#ifdef EAGAIN		/* POSIX */
	if (errno == EAGAIN)
		return (0);
#endif

#ifdef EWOULDBLOCK	/* BSD */
	if (errno == EWOULDBLOCK)
		return (0);
#endif /* EWOULDBLOCK */

#ifdef EDEADLK		/* Macintosh */
	if (errno == EDEADLK)
		return (0);
#endif

	/*
	 * We don't know what happened, cut them off. This qualifies for
	 * a SYSERR because we have no idea what happened at this point.
	 */
	perror("perform_socket_read: about to lose connection");
	return (-1);
}


/*
 * perform_socket_write: takes a descriptor, a pointer to text, and a
 * text length, and tries once to send that text to the OS.  This is
 * where we stuff all the platform-dependent stuff that used to be
 * ugly #ifdef's in write_to_descriptor().
 *
 * This function must return:
 *
 * -1  If a fatal error was encountered in writing to the descriptor.
 *  0  If a transient failure was encountered (e.g. socket buffer full).
 * >0  To indicate the number of bytes successfully written, possibly
 *     fewer than the number the caller requested be written.
 *
 * Right now there are two versions of this function: one for Windows,
 * and one for all other platforms.
 */
/* perform_socket_write for all Non-Windows platforms */
ssize_t perform_socket_write(socket_t desc, const char *txt, size_t length) {
	ssize_t result;

	result = write(desc, txt, length);

	if (result > 0) {
		/* Write was successful. */
		return (result);
	}

	if (result == 0) {
		/* This should never happen! */
		log("SYSERR: Huh??  write() returned 0???  Please report this!");
		return (-1);
	}

	/*
	 * result < 0, so an error was encountered - is it transient?
	 * Unfortunately, different systems use different constants to
	 * indicate this.
	 */

#ifdef EAGAIN		/* POSIX */
	if (errno == EAGAIN)
		return (0);
#endif

#ifdef EWOULDBLOCK	/* BSD */
	if (errno == EWOULDBLOCK)
		return (0);
#endif

#ifdef EDEADLK		/* Macintosh */
	if (errno == EDEADLK)
		return (0);
#endif

	/* Looks like the error was fatal.  Too bad. */
	return (-1);
}


/*
 * ASSUMPTION: There will be no newlines in the raw input buffer when this
 * function is called.  We must maintain that before returning.
 *
 * Ever wonder why 'tmp' had '+8' on it?  The crusty old code could write
 * MAX_INPUT_LENGTH+1 bytes to 'tmp' if there was a '$' as the final
 * character in the input buffer.  This would also cause 'space_left' to
 * drop to -1, which wasn't very happy in an unsigned variable.  Argh.
 * So to fix the above, 'tmp' lost the '+8' since it doesn't need it
 * and the code has been changed to reserve space by accepting one less
 * character. (Do you really need 256 characters on a line?)
 * -gg 1/21/2000
 */
int process_input(descriptor_data *t) {
	static char read_buf[MAX_PROTOCOL_BUFFER];
	int buf_length, do_not_add;
	ssize_t bytes_read;
	size_t space_left;
	char *ptr, *read_point, *write_point, *nl_pos = NULL;
	char tmp[MAX_INPUT_LENGTH], *input;
	bool add_to_head = FALSE;
	
	*read_buf = '\0';

	/* first, find the point where we left off reading data */
	buf_length = strlen(t->inbuf);
	read_point = t->inbuf + buf_length;
	space_left = MAX_RAW_INPUT_LENGTH - buf_length - 1;

	do {
		if (strlen(t->inbuf) > MAX_INPUT_LENGTH || (space_left <= 0 && strlen(t->inbuf) > 0)) {
			char buffer[MAX_STRING_LENGTH];
			
			// truncate to input length
			if (strlen(t->inbuf) >= MAX_INPUT_LENGTH) {
				t->inbuf[MAX_INPUT_LENGTH-2] = '\0';
			}
			
			snprintf(buffer, sizeof(buffer), "Line too long. Truncated to:\r\n%s\r\n", t->inbuf);
			if (write_to_descriptor(t->descriptor, buffer) < 0) {
				return (-1);
			}
			
			nl_pos = read_point;	// need to infer a newline
			
			// flush the rest of the input
			do {
				bytes_read = perform_socket_read(t->descriptor, read_buf, MAX_PROTOCOL_BUFFER);
				if (bytes_read < 0) {
					return -1;
				}
			} while (bytes_read > 0);
			
			// exit the do-while now
			break;
			
			/* formerly:
			log("WARNING: process_input: about to close connection: input overflow");
			return (-1);
			*/
		}

		bytes_read = perform_socket_read(t->descriptor, read_buf, MAX_PROTOCOL_BUFFER);

		if (bytes_read < 0) {	/* Error, disconnect them. */
			return (-1);
		}
		else if (bytes_read >= 0) {
			read_buf[bytes_read] = '\0';
			ProtocolInput(t, read_buf, bytes_read, read_point, space_left+1);
			bytes_read = strlen(read_point);
		}

		if (bytes_read == 0) {	/* Just blocking, no problems. */
			return (0);
		}

		/* at this point, we know we got some data from the read */

		*(read_point + bytes_read) = '\0';	/* terminate the string */

		/* search for a newline in the data we just read */
		for (ptr = read_point; *ptr && !nl_pos; ptr++)
			if (ISNEWL(*ptr))
				nl_pos = ptr;

		read_point += bytes_read;
		space_left -= bytes_read;

		/*
		 * on some systems such as AIX, POSIX-standard nonblocking I/O is broken,
		 * causing the MUD to hang when it encounters input not terminated by a
		 * newline.  This was causing hangs at the Password: prompt, for example.
		 * I attempt to compensate by always returning after the _first_ read, instead
		 * of looping forever until a read returns -1.  This simulates non-blocking
		 * I/O because the result is we never call read unless we know from select()
		 * that data is ready (process_input is only called if select indicates that
		 * this descriptor is in the read set).  JE 2/23/95.
		 */
#if !defined(POSIX_NONBLOCK_BROKEN)
	} while (nl_pos == NULL);
#else
	} while (0);

	if (nl_pos == NULL)
		return (0);
#endif /* POSIX_NONBLOCK_BROKEN */

	/*
	 * okay, at this point we have at least one newline in the string; now we
	 * can copy the formatted data to a new array for further processing.
	 */

	read_point = t->inbuf;

	while (nl_pos != NULL) {
		input = tmp;
		write_point = tmp;
		space_left = MAX_INPUT_LENGTH - 1;

		/* The '> 1' reserves room for a '$ => $$' expansion. */
		for (ptr = read_point; (space_left > 1) && (ptr < nl_pos); ptr++) {
			if (*ptr == '\b' || *ptr == 127) { /* handle backspacing or delete key */
				if (write_point > tmp) {
					if (*(--write_point) == '$') {
						write_point--;
						space_left += 2;
					}
					else
						space_left++;
				}
			}
			else if (isascii(*ptr) && isprint(*ptr)) {
				if ((*(write_point++) = *ptr) == '$') {		/* copy one character */
					*(write_point++) = '$';	/* if it's a $, double it */
					space_left -= 2;
				}
				else
					space_left--;
			}
		}

		*write_point = '\0';
		
		if ((space_left <= 1) && (ptr < nl_pos)) {
			char buffer[MAX_INPUT_LENGTH + 64];

			sprintf(buffer, "Line too long. Truncated to:\r\n%s\r\n", tmp);
			if (write_to_descriptor(t->descriptor, buffer) < 0)
				return (-1);
		}
		if (t->snoop_by && *input) {
			SEND_TO_Q("% ", t->snoop_by);
			SEND_TO_Q(input, t->snoop_by);
			SEND_TO_Q("\r\n", t->snoop_by);
		}
		do_not_add = 0;

		if (*input == '!' && !(*(input + 1))) {	/* Redo last command. */
			strncpy(input, t->last_input, MAX_INPUT_LENGTH-1);
			input[MAX_INPUT_LENGTH-1] = '\0';
		}
		else if (*input == '!' && *(input + 1)) {
			char *commandln = (input + 1);
			int starting_pos = t->history_pos, cnt = (t->history_pos == 0 ? HISTORY_SIZE - 1 : t->history_pos - 1);

			skip_spaces(&commandln);
			for (; cnt != starting_pos; cnt--) {
				if (t->history[cnt] && is_abbrev(commandln, t->history[cnt])) {
					strcpy(input, t->history[cnt]);
					strncpy(t->last_input, input, sizeof(t->last_input)-1);
					t->last_input[sizeof(t->last_input)-1] = '\0';
					SEND_TO_Q(input, t);
					SEND_TO_Q("\r\n", t);
					break;
				}
				if (cnt == 0)	/* At top, loop to bottom. */
					cnt = HISTORY_SIZE;
			}
		}
		else if (*input == '^') {
			if (!(do_not_add = perform_subst(t, t->last_input, input))) {
				strncpy(t->last_input, input, sizeof(t->last_input)-1);
				t->last_input[sizeof(t->last_input)-1] = '\0';
			}
		}
		else if (*input == '+') {	// add to head of queue
			add_to_head = TRUE;
			++input;
		}
		else if (*input == '-' && (!t->str || !t->straight_to_editor)) { // manipulate input queue
			// ^ this doesn't execute if the person is sending text to a text editor
			manipulate_input_queue(t, input+1);
			do_not_add = 1;
		}
		else {
			strncpy(t->last_input, input, sizeof(t->last_input)-1);
			t->last_input[sizeof(t->last_input)-1] = '\0';
			
			if (t->history[t->history_pos])
				free(t->history[t->history_pos]);	/* Clear the old line. */
			t->history[t->history_pos] = str_dup(input);	/* Save the new. */
			if (++t->history_pos >= HISTORY_SIZE)	/* Wrap to top. */
				t->history_pos = 0;
		}

		if (!do_not_add) {
			write_to_q(input, &t->input, 0, add_to_head);
			add_to_head = FALSE;
		}

		/* find the end of this line */
		while (ISNEWL(*nl_pos))
			nl_pos++;

		/* see if there's another newline in the input buffer */
		read_point = ptr = nl_pos;
		for (nl_pos = NULL; *ptr && !nl_pos; ptr++)
			if (ISNEWL(*ptr))
				nl_pos = ptr;
	}

	/* now move the rest of the buffer up to the beginning for the next pass */
	write_point = t->inbuf;
	while (*read_point)
		*(write_point++) = *(read_point++);
	*write_point = '\0';

	return (1);
}


/* Send all of the output that we've accumulated for a player out to the
 * player's descriptor. 32 byte GARBAGE_SPACE in MAX_SOCK_BUF used for:
 *	 2 bytes: prepended \r\n
 *	14 bytes: overflow message
 *	 2 bytes: extra \r\n for non-comapct
 *      14 bytes: unused */
static int process_output(descriptor_data *t) {
	char i[MAX_SOCK_BUF], *osb = i + 2;
	int result;

	/* we may need this \r\n for later -- see below */
	strcpy(i, "\r\n");	/* strcpy: OK (for 'MAX_SOCK_BUF >= 3') */

	/* now, append the 'real' output */
	strcpy(osb, t->output);	/* strcpy: OK (t->output:LARGE_BUFSIZE < osb:MAX_SOCK_BUF-2) */

	/* if we're in the overflow state, notify the user */
	if (t->bufspace == 0)
		strcat(osb, "**OVERFLOW**\r\n");	/* strcpy: OK (osb:MAX_SOCK_BUF-2 reserves space) */

	/* add the extra CRLF if the person isn't in compact mode */
	if (STATE(t) == CON_PLAYING && t->character && !REAL_NPC(t->character) && !PRF_FLAGGED(t->character, PRF_COMPACT) && !t->pProtocol->WriteOOB)
		strcat(osb, "\r\n");	/* strcpy: OK (osb:MAX_SOCK_BUF-2 reserves space) */

	// add prompt
	if (!t->pProtocol->WriteOOB) {
		char prompt[MAX_STRING_LENGTH];
		int wantsize;
		
		strcpy(prompt, make_prompt(t));
		wantsize = strlen(prompt);
		strncpy(prompt, ProtocolOutput(t, prompt, &wantsize), MAX_STRING_LENGTH);
		prompt[MAX_STRING_LENGTH-1] = '\0';
				
		// force a color code flush
		snprintf(prompt + strlen(prompt), sizeof(prompt) - strlen(prompt), "%s", flush_reduced_color_codes(t));

		strncat(i, prompt, MAX_PROMPT_LENGTH);
	}

	/* now, send the output.  If this is an 'interruption', use the prepended
	* CRLF, otherwise send the straight output sans CRLF. */
	if (t->has_prompt && !t->data_left_to_write && !t->pProtocol->WriteOOB) {
		t->has_prompt = FALSE;
		result = write_to_descriptor(t->descriptor, i);
		if (result >= 2)
			result -= 2;
	}
	else {
		t->has_prompt = FALSE;
		result = write_to_descriptor(t->descriptor, osb);
	}

	if (result < 0) {	/* Oops, fatal error. Bye! */
		close_socket(t);
		return (-1);
	}
	else if (result == 0)	/* Socket buffer full. Try later. */
		return (0);

	/* Handle snooping: prepend "% " and send to snooper. */
	if (t->snoop_by && *t->output) {
		char stripped[MAX_STRING_LENGTH];
		
		strncpy(stripped, strip_telnet_codes(t->output), MAX_STRING_LENGTH);
		stripped[MAX_STRING_LENGTH-1] = '\0';
		
		if (*stripped) {
			write_to_output("% ", t->snoop_by);
			write_to_output(stripped, t->snoop_by);
			write_to_output("%%", t->snoop_by);
		}
	}
	
	/* The common case: all saved output was handed off to the kernel buffer. */
	if (result >= t->bufptr) {
		/* If we were using a large buffer, put the large buffer on the buffer pool
		* and switch back to the small one. */
		if (t->large_outbuf) {
			t->large_outbuf->next = bufpool;
			bufpool = t->large_outbuf;
			t->large_outbuf = NULL;
			t->output = t->small_outbuf;
		}
		/* reset total bufspace back to that of a small buffer */
		t->bufspace = SMALL_BUFSIZE - 1;
		t->bufptr = 0;
		*(t->output) = '\0';

		/* If the overflow message or prompt were partially written, try to save
		* them. There will be enough space for them if this is true.  'result'
		* is effectively unsigned here anyway. */
		if ((unsigned int)result < strlen(osb)) {
			size_t savetextlen = strlen(osb + result);

			strcat(t->output, osb + result);
			t->bufptr -= savetextlen;
			t->bufspace += savetextlen;
		}

		t->data_left_to_write = FALSE;
	}
	else {
		/* Not all data in buffer sent.  result < output buffersize. */
		strcpy(t->output, t->output + result);	/* strcpy: OK (overlap) */
		t->bufptr -= result;
		t->bufspace += result;
		
		t->data_left_to_write = TRUE;
	}

	return (result);
}


/* Sets the kernel's send buffer size for the descriptor */
int set_sendbuf(socket_t s) {
#if defined(SO_SNDBUF)
	int opt = MAX_SOCK_BUF;

	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *) &opt, sizeof(opt)) < 0) {
		perror("SYSERR: setsockopt SNDBUF");
		return (-1);
	}

#if 0
	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *) &opt, sizeof(opt)) < 0) {
		perror("SYSERR: setsockopt RCVBUF");
		return (-1);
	}
#endif

#endif

	return (0);
}


/* write_to_descriptor takes a descriptor, and text to write to the descriptor.
 * It keeps calling the system-level write() until all the text has been
 * delivered to the OS, or until an error is encountered. Returns:
 * >=0  If all is well and good.
 *  -1  If an error was encountered, so that the player should be cut off. */
int write_to_descriptor(socket_t desc, const char *txt) {
	ssize_t bytes_written;
	size_t total = strlen(txt), write_total = 0;

	while (total > 0) {
		bytes_written = perform_socket_write(desc, txt, total);

		if (bytes_written < 0) {
			/* Fatal error.  Disconnect the player. */
			perror("SYSERR: Write to socket");
			return (-1);
		}
		else if (bytes_written == 0) {
			/* Temporary failure -- socket buffer full. */
			return (write_total);
		}
		else {
			txt += bytes_written;
			total -= bytes_written;
			write_total += bytes_written;
		}
	}

	return (write_total);
}


/* Add a new string to a player's output queue */
void write_to_output(const char *txt, descriptor_data *t) {
	const char *overflow_txt = "**OVERFLOW**\r\n";
	int size, wantsize;
	char protocol_txt[MAX_STRING_LENGTH];

	/* if we're in the overflow state already, ignore this new output */
	if (t->bufspace == 0)
		return;

	size = wantsize = strlen(txt);
	strncpy(protocol_txt, ProtocolOutput(t, txt, &wantsize), MAX_STRING_LENGTH);
	protocol_txt[MAX_STRING_LENGTH-1] = '\0';
	size = wantsize;
	if (t->pProtocol->WriteOOB > 0) {
		--t->pProtocol->WriteOOB;
	}

	/* If exceeding the size of the buffer, truncate it for the overflow message */
	if (size < 0 || wantsize >= sizeof(protocol_txt)) {
		size = sizeof(protocol_txt) - 1;
		strcpy(protocol_txt + size - strlen(overflow_txt), overflow_txt);	/* strcpy: OK */
	}
	
	// check that text size is going to fit into a large bufffer
	if (size + t->bufptr + 1 > LARGE_BUFSIZE) {
		size = LARGE_BUFSIZE - t->bufptr - 1;
		protocol_txt[size] = '\0';
		++buf_overflows;
	}

	/* if we have enough space, just write to buffer and that's it! */
	if (t->bufspace > size) {
		strcpy(t->output + t->bufptr, protocol_txt);
		t->bufspace -= size;
		t->bufptr += size;
		return;
	}
	
	++buf_switches;

	/* if the pool has a buffer in it, grab it */
	if (bufpool != NULL) {
		t->large_outbuf = bufpool;
		bufpool = bufpool->next;
	}
	else {			/* else create a new one */
   		CREATE(t->large_outbuf, struct txt_block, 1);
		CREATE(t->large_outbuf->text, char, LARGE_BUFSIZE);
		buf_largecount++;
	}

	strcpy(t->large_outbuf->text, t->output);	/* copy to big buffer */
	t->output = t->large_outbuf->text;	/* make big buffer primary */
	strcat(t->output, protocol_txt);

	/* set the pointer for the next write */
	t->bufptr = strlen(t->output);

	/* calculate how much space is left in the buffer */
	t->bufspace = LARGE_BUFSIZE - 1 - t->bufptr;
}


/**
* Adds an item to an input queue.
*
* @param const char *txt The text to add to the queue.
* @param struct txt_q *queue The queue to add to.
* @param int aliased Whether or not the input was from an alias.
* @param bool add_to_head If TRUE, puts the new item at the start instead of end of the queue.
*/
void write_to_q(const char *txt, struct txt_q *queue, int aliased, bool add_to_head) {
	struct txt_block *newt;

	CREATE(newt, struct txt_block, 1);
	newt->text = str_dup(txt);
	newt->aliased = aliased;

	/* queue empty? */
	if (!queue->head) {
		newt->next = NULL;
		queue->head = queue->tail = newt;
	}
	else if (add_to_head) {
		newt->next = queue->head;
		queue->head = newt;
	}
	else {
		queue->tail->next = newt;
		queue->tail = newt;
		newt->next = NULL;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// PROMPT //////////////////////////////////////////////////////////////////

/**
* Makes the prompt for the character. 
*
* @param descriptor_data *d The descriptor we're making the prompt for
* @return char* A pointer to the prompt string.
*/
char *make_prompt(descriptor_data *d) {
	static char prompt[MAX_STRING_LENGTH];

	/* Note, prompt is truncated at MAX_PROMPT_LENGTH chars (structs.h )*/

	if (d->character && EXTRACTED(d->character)) {
		// no prompt to people in the EXTRACTED state
		*prompt = '\0';
	}
	else if (d->showstr_count)
		sprintf(prompt, "\r\t0[ Return to continue, (q)uit, (r)efresh, (b)ack, or page number (%d/%d) ]", d->showstr_page, d->showstr_count);
	else if (d->str && (STATE(d) != CON_PLAYING || d->straight_to_editor))
		strcpy(prompt, "] ");
	else if (STATE(d) == CON_PLAYING) {
		*prompt = '\0';
		
		// show idle after 10 minutes
		if ((d->character->char_specials.timer * SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN) >= 10) {
			strcat(prompt, "\tr[IDLE]\t0 ");
		}

		// append rendered prompt
		snprintf(prompt + strlen(prompt), sizeof(prompt) - strlen(prompt), "%s", prompt_str(d->character));
	}
	else {
		*prompt = '\0';
	}

	return prompt;
}


/**
* Determines a color based on percentage.
*
* @param int cur The current value of anything.
* @param int max The maximum value of that thing.
* @return char* A color code.
*/
char *prompt_color_by_prc(int cur, int max) {
	int prc = cur * 100 / MAX(1, max);
	
	if (prc >= 90) {
		return "\tg";
	}
	else if (prc >= 25) {
		return "\ty";
	}
	else {
		return "\tr";
	}
}


/**
* Makes the prompt for a character and sends it off for processing.
*
* @param char_data *ch The player
* @return char* The finished prompt
*/
char *prompt_str(char_data *ch) {
	char *str;
	
	if (FIGHTING(ch)) {
		str = GET_FIGHT_PROMPT(REAL_CHAR(ch)) ? GET_FIGHT_PROMPT(REAL_CHAR(ch)) : GET_PROMPT(REAL_CHAR(ch));
	}
	else {
		str = GET_PROMPT(REAL_CHAR(ch));
	}

	if (!ch->desc)
		return (">");

	// default prompts
	if (!str || !*str) {
		if (IS_IMMORTAL(ch)) {
			str = "\t0|\tc%C \tg%X\t0> ";
		}
		else if (IS_MAGE(ch)) {
			if (IS_VAMPIRE(ch)) {
				str = "\t0|%i/%u/%n/%d [\ty%C\t0] [%t]> ";
			}
			else {
				str = "\t0|%i/%u/%n [\ty%C\t0]> ";
			}
		}
		else {
			if (IS_VAMPIRE(ch)) {
				str = "\t0|%i/%u/%d [\ty%C\t0] [%t]> ";
			}
			else {
				str = "\t0|%i/%u [\ty%C\t0]> ";
			}
		}
	}

	if (!strchr(str, '%')) {
		return str;
	}
	else {
		return replace_prompt_codes(ch, str);
	}
}


/**
* Does the actual replacement of codes like %h that are used by the prompt and
* some other commands.
*
* @param char_data *ch The character to base the info off of
* @param char *str The incoming string to translate
* @return char* The processed string
*/
char *replace_prompt_codes(char_data *ch, char *str) {
	extern const char *health_levels[];
	extern const char *move_levels[];
	extern const char *mana_levels[];
	extern const char *blood_levels[];
	extern struct action_data_struct action_data[];
	
	static char pbuf[MAX_STRING_LENGTH];
	char i[MAX_STRING_LENGTH], spare[10];
	char *cp, *tmp;
	char_data *vict;

	cp = pbuf;

	for (;;) {
		if (*str == '%') {
			switch (*(++str)) {
				case 'c': {	// player conditions (words)
					*i = '\0';
					if (PRF_FLAGGED(ch, PRF_AFK)) {
						strcat(i, "\trA");
					}
					if (IS_DRUNK(ch)) {
						strcat(i, "\t0D");
					}
					if (EFFECTIVELY_FLYING(ch)) {
						strcat(i, "\t0F");
					}
					if (IS_HUNGRY(ch)) {
						strcat(i, "\t0H");
					}
					if (IS_MORPHED(ch)) {
						strcat(i, "\t0M");
					}
					if (IS_PVP_FLAGGED(ch)) {
						strcat(i, "\tRP");
					}
					if (IS_RIDING(ch)) {
						strcat(i, "\t0R");
					}
					if (PRF_FLAGGED(ch, PRF_RP)) {
						strcat(i, "\tmR");
					}
					if (IS_BLOOD_STARVED(ch)) {
						strcat(i, "\t0S");
					}
					if (IS_THIRSTY(ch)) {
						strcat(i, "\t0T");
					}
					if (HAS_NEW_OFFENSES(ch)) {
						strcat(i, "\t0O");
					}
					if (!IS_NPC(ch)) {
						if (get_cooldown_time(ch, COOLDOWN_ROGUE_FLAG) > 0) {
							strcat(i, "\tMR");
						}
						else if (get_cooldown_time(ch, COOLDOWN_HOSTILE_FLAG) > 0) {
							strcat(i, "\tmH");
						}
					}
					
					if (!*i) {
						strcpy(i, "\t0-");
					}
					
					strcat(i, "\t0");
					tmp = i;
					break;
				}
				case 'C': {	// player conditions (words)
					*i = '\0';
					if (PRF_FLAGGED(ch, PRF_AFK)) {
						sprintf(i + strlen(i), "%safk", (*i ? " " : ""));
					}
					if (IS_DRUNK(ch)) {
						sprintf(i + strlen(i), "%sdrunk", (*i ? " " : ""));
					}
					if (EFFECTIVELY_FLYING(ch)) {
						sprintf(i + strlen(i), "%sflying", (*i ? " " : ""));
					}
					if (IS_HUNGRY(ch)) {
						sprintf(i + strlen(i), "%shungry", (*i ? " " : ""));
					}
					if (IS_MORPHED(ch)) {
						sprintf(i + strlen(i), "%smorphed", (*i ? " " : ""));
					}
					if (IS_PVP_FLAGGED(ch)) {
						sprintf(i + strlen(i), "%spvp", (*i ? " " : ""));
					}
					if (IS_RIDING(ch)) {
						sprintf(i + strlen(i), "%sriding", (*i ? " " : ""));
					}
					if (PRF_FLAGGED(ch, PRF_RP)) {
						sprintf(i + strlen(i), "%srp", (*i ? " " : ""));
					}
					if (IS_BLOOD_STARVED(ch)) {
						sprintf(i + strlen(i), "%sstarved", (*i ? " " : ""));
					}
					if (IS_THIRSTY(ch)) {
						sprintf(i + strlen(i), "%sthirsty", (*i ? " " : ""));
					}
					if (!IS_NPC(ch)) {
						if (get_cooldown_time(ch, COOLDOWN_ROGUE_FLAG) > 0) {
							sprintf(i + strlen(i), "%srogue", (*i ? " " : ""));
						}
						else if (get_cooldown_time(ch, COOLDOWN_HOSTILE_FLAG) > 0) {
							sprintf(i + strlen(i), "%shostile", (*i ? " " : ""));
						}
					}
					if (HAS_NEW_OFFENSES(ch)) {
						sprintf(i + strlen(i), "%soffenses", (*i ? " " : ""));
					}
					
					if (!*i) {
						strcpy(i, "--");
					}
					
					tmp = i;
					break;
				}
				case 'x': {	// Immortal conditions (abbrevs)
					*i = '\0';
					if (IS_IMMORTAL(ch)) {
						if (PRF_FLAGGED(ch, PRF_INCOGNITO)) {
							strcat(i, "\tbI");
						}
						if (PRF_FLAGGED(ch, PRF_WIZHIDE)) {
							strcat(i, "\tcW");
						}
						if (wizlock_level > 0) {
							sprintf(i + strlen(i), "\tgw%d", wizlock_level);
						}
						if (!IS_NPC(ch) && GET_INVIS_LEV(ch) > 0) {
							sprintf(i + strlen(i), "\tri%d", GET_INVIS_LEV(ch));
						}
						if (ch->desc && GET_OLC_TYPE(ch->desc) != 0) {
							strcat(i, "\tcO");
						}
					}
					
					if (!*i) {
						strcpy(i, "\t0-");
					}
					
					strcat(i, "\t0");
					tmp = i;
					break;
				}
				case 'X': {	// Immortal conditions (words)
					*i = '\0';
					if (IS_IMMORTAL(ch)) {
						if (PRF_FLAGGED(ch, PRF_INCOGNITO)) {
							sprintf(i + strlen(i), "%sincog", (*i ? " " : ""));
						}
						if (PRF_FLAGGED(ch, PRF_WIZHIDE)) {
							sprintf(i + strlen(i), "%swizhide", (*i ? " " : ""));
						}
						if (wizlock_level > 0) {
							sprintf(i + strlen(i), "%swizlock-%d", (*i ? " " : ""), wizlock_level);
						}
						if (!IS_NPC(ch) && GET_INVIS_LEV(ch) > 0) {
							sprintf(i + strlen(i), "%sinvis-%d", (*i ? " " : ""), GET_INVIS_LEV(ch));
						}
						if (ch->desc && GET_OLC_TYPE(ch->desc) != 0) {
							extern char *prompt_olc_info(char_data *ch);
							sprintf(i + strlen(i), "%solc-%s", (*i ? " " : ""), prompt_olc_info(ch));
						}
					}
					
					if (!*i) {
						strcpy(i, "--");
					}
					
					tmp = i;
					break;
				}
				case 'e': {	// %e enemy's health percent
					if ((vict = FIGHTING(ch))) {
						sprintf(i, "%d%%", GET_HEALTH(vict) * 100 / MAX(1, GET_MAX_HEALTH(vict)));
					}
					else {
						*i = '\0';
					}
					tmp = i;
					break;
				}
				case 'f': {	// %f enemy's focus (tank): health percent
					if (FIGHTING(ch) && (vict = FIGHTING(FIGHTING(ch))) && vict != ch) {
						sprintf(i, "%s %d%%", (IS_NPC(vict) ? PERS(vict, vict, FALSE) : GET_NAME(vict)), GET_HEALTH(vict) * 100 / MAX(1, GET_MAX_HEALTH(vict)));
					}
					else {
						*i = '\0';
					}
					tmp = i;
					break;
				
				}
				case 'F': {	// %f enemy's focus (tank): health total
					if (FIGHTING(ch) && (vict = FIGHTING(FIGHTING(ch))) && !IS_NPC(vict) && vict != ch) {
						sprintf(i, "%s %d/%d", (IS_NPC(vict) ? PERS(vict, vict, FALSE) : GET_NAME(vict)), GET_HEALTH(vict), GET_MAX_HEALTH(vict));
					}
					else {
						*i = '\0';
					}
					tmp = i;
					break;
				}
				case 'h':	// %h current health
					sprintf(i, "%s%d\t0", prompt_color_by_prc(GET_HEALTH(ch), GET_MAX_HEALTH(ch)), MAX(0, GET_HEALTH(ch)));
					tmp = i;
					break;
				case 'H':	// %H max health
					sprintf(i, "%d", GET_MAX_HEALTH(ch));
					tmp = i;
					break;
				case 'i':	// %i health as text
					sprintf(i, "%s", health_levels[MIN(10, MAX(0, GET_HEALTH(ch)) * 10 / MAX(1, GET_MAX_HEALTH(ch)))]);
					tmp = i;
					break;
				case 'v':	// %v current move
					sprintf(i, "%s%d\t0", prompt_color_by_prc(GET_MOVE(ch), GET_MAX_MOVE(ch)), MAX(0, GET_MOVE(ch)));
					tmp = i;
					break;
				case 'V':	// %V max move
					sprintf(i, "%d", GET_MAX_MOVE(ch));
					tmp = i;
					break;
				case 'u':	// %u move as text
					sprintf(i, "%s", move_levels[MIN(10, MAX(0, GET_MOVE(ch)) * 10 / MAX(1, GET_MAX_MOVE(ch)))]);
					tmp = i;
					break;
				case 'm':	// %m current mana
					sprintf(i, "%s%d\t0", prompt_color_by_prc(GET_MANA(ch), GET_MAX_MANA(ch)), MAX(0, GET_MANA(ch)));
					tmp = i;
					break;
				case 'M':	// %M max mana
					sprintf(i, "%d", GET_MAX_MANA(ch));
					tmp = i;
					break;
				case 'n':	// %n mana as text
					sprintf(i, "%s", mana_levels[MIN(10, MAX(0, GET_MANA(ch)) * 10 / MAX(1, GET_MAX_MANA(ch)))]);
					tmp = i;
					break;
				case 'b':	// %b current blood
					if (IS_VAMPIRE(ch))
						sprintf(i, "%s%d\t0", prompt_color_by_prc(GET_BLOOD(ch), GET_MAX_BLOOD(ch)), GET_BLOOD(ch));
					else
						sprintf(i, "%%%c", *str);
					tmp = i;
					break;
				case 'B':	// %B max blood
					if (IS_VAMPIRE(ch))
						sprintf(i, "%d", GET_MAX_BLOOD(ch));
					else
						sprintf(i, "%%%c", *str);
					tmp = i;
					break;
				case 'd': {	// %d blood as text
					sprintf(i, "%s", blood_levels[MIN(10, MAX(0, GET_BLOOD(ch)) * 10 / MAX(1, GET_MAX_BLOOD(ch)))]);
					tmp = i;
					break;
				}
				case 's': {	// %s bonus exp available
					sprintf(i, "%d", GET_DAILY_BONUS_EXPERIENCE(ch));
					tmp = i;
					break;
				}
				case 't':	/* Sun timer */
				case 'T':
					if (time_info.hours >= 12)
						strcpy(spare, "pm");
					else
						strcpy(spare, "am");

					if (time_info.hours == 6)
						sprintf(i, "\tC%d%s\t0", time_info.hours > 12 ? time_info.hours - 12 : time_info.hours, spare);
					else if (time_info.hours < 19 && time_info.hours > 6)
						sprintf(i, "\tY%d%s\t0", time_info.hours > 12 ? time_info.hours - 12 : time_info.hours, spare);
					else if (time_info.hours == 19)
						sprintf(i, "\tR%d%s\t0", time_info.hours > 12 ? time_info.hours - 12 : time_info.hours, spare);
					else if (time_info.hours == 0)
						sprintf(i, "\tB12am\t0");
					else
						sprintf(i, "\tB%d%s\t0", time_info.hours > 12 ? time_info.hours - 12 : time_info.hours, spare);
					tmp = i;
					break;
				case 'a': {	// action
					if (!IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE) {
						strcpy(i, action_data[GET_ACTION(ch)].name);
					}
					else if (GET_FEEDING_FROM(ch)) {
						strcpy(i, "feeding");
					}
					else {
						*i = '\0';
					}
					
					tmp = i;
					break;
				}
				case 'q': {	// daily quests left
					int amt = 0;
					if (!IS_NPC(ch)) {
						amt = config_get_int("dailies_per_day") - GET_DAILY_QUESTS(ch);
					}
					sprintf(i, "%d", MAX(0, amt));
					tmp = i;
					break;
				}
				case 'r': {	// room template/building
					if (IS_IMMORTAL(ch)) {
						if (GET_ROOM_TEMPLATE(IN_ROOM(ch))) {
							sprintf(i, "r%d", GET_RMT_VNUM(GET_ROOM_TEMPLATE(IN_ROOM(ch))));
						}
						else if (GET_BUILDING(IN_ROOM(ch))) {
							sprintf(i, "b%d", GET_BLD_VNUM(GET_BUILDING(IN_ROOM(ch))));
						}
						else if (ROOM_CROP(IN_ROOM(ch))) {
							sprintf(i, "c%d/s%d", GET_CROP_VNUM(ROOM_CROP(IN_ROOM(ch))), GET_SECT_VNUM(SECT(IN_ROOM(ch))));
						}
						else {
							sprintf(i, "s%d", GET_SECT_VNUM(SECT(IN_ROOM(ch))));
						}
					}
					else {
						sprintf(i, "%%%c", *str);
					}
					
					tmp = i;
					break;
				}
				case 'I': {	// inventory (capital i)
					sprintf(i, "%d/%d", IS_CARRYING_N(ch), CAN_CARRY_N(ch));
					tmp = i;
					break;
				}
				case '_':
					tmp = "\r\n";
					break;
				case '%':
					*(cp++) = '%';
					str++;
					continue;
					break;
				default :
					*(cp++) = '%';
					str++;
					continue;
					break;
				}

			while ((*cp = *(tmp++))) {
				cp++;
			}
			str++;
		}
		else if (!(*(cp++) = *(str++))) {
			break;
		}
	}

	*cp = '\0';

	strcat(pbuf, "\t0");
	return (pbuf);
}


 //////////////////////////////////////////////////////////////////////////////
//// SIGNAL PROCESSING ///////////////////////////////////////////////////////

// triggers import of evolutions
RETSIGTYPE import_evolutions(int sig) {
	do_evo_import = TRUE;
}

RETSIGTYPE unrestrict_game(int sig) {
	syslog(SYS_INFO, 0, TRUE, "Received SIGUSR2 - completely unrestricting game (emergent)");
	ban_list = NULL;
	wizlock_level = 0;
	num_invalid = 0;
}

/* clean up our zombie kids to avoid defunct processes */
RETSIGTYPE reap(int sig) {
	while (waitpid(-1, NULL, WNOHANG) > 0);

	my_signal(SIGCHLD, reap);
}

RETSIGTYPE checkpointing(int sig) {
	if (!tics_passed) {
		log("SYSERR: CHECKPOINT shutdown: tics not updated. (Infinite loop suspected)");
		abort();
	}
	else {
		tics_passed = 0;
	}
}

RETSIGTYPE hupsig(int sig) {
	log("SYSERR: Received SIGHUP, SIGINT, or SIGTERM. Shutting down...");
	reboot_control.type = SCMD_SHUTDOWN;
	perform_reboot();
	// exit(1);
}

/*
 * This is an implementation of signal() using sigaction() for portability.
 * (sigaction() is POSIX; signal() is not.)  Taken from Stevens' _Advanced
 * Programming in the UNIX Environment_.  We are specifying that all system
 * calls _not_ be automatically restarted for uniformity, because BSD systems
 * do not restart select(), even if SA_RESTART is used.
 *
 * Note that NeXT 2.x is not POSIX and does not have sigaction; therefore,
 * I just define it to be the old signal.  If your system doesn't have
 * sigaction either, you can use the same fix.
 *
 * SunOS Release 4.0.2 (sun386) needs this too, according to Tim Aldric.
 */

#ifndef POSIX
#define my_signal(signo, func) signal(signo, func)
#else
sigfunc *my_signal(int signo, sigfunc * func) {
	struct sigaction act, oact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;	/* SunOS */
#endif

	if (sigaction(signo, &act, &oact) < 0)
		return (SIG_ERR);

	return (oact.sa_handler);
}
#endif				/* POSIX */


void signal_setup(void) {
	struct itimerval itime;
	struct timeval interval;
	
	// user signal 1: indicates we have waiting evos to import
	my_signal(SIGUSR1, import_evolutions);

	/*
	 * user signal 2: unrestrict game.  Used for emergencies if you lock
	 * yourself out of the MUD somehow.  (Duh...)
	 */
	my_signal(SIGUSR2, unrestrict_game);

	/*
	 * set up the deadlock-protection so that the MUD aborts itself if it gets
	 * caught in an infinite loop for more than 3 minutes.
	 */
	interval.tv_sec = 180;
	interval.tv_usec = 0;
	itime.it_interval = interval;
	itime.it_value = interval;
	setitimer(ITIMER_VIRTUAL, &itime, NULL);
	my_signal(SIGVTALRM, checkpointing);

	/* just to be on the safe side: */
	my_signal(SIGHUP, hupsig);
	my_signal(SIGCHLD, reap);
	my_signal(SIGINT, hupsig);
	my_signal(SIGTERM, hupsig);
	my_signal(SIGPIPE, SIG_IGN);
	my_signal(SIGALRM, SIG_IGN);
}


 //////////////////////////////////////////////////////////////////////////////
//// STARTUP /////////////////////////////////////////////////////////////////

/*
 * game_loop contains the main loop which drives the entire MUD.  It
 * cycles once every 0.10 seconds and is responsible for accepting new
 * new connections, polling existing connections for input, dequeueing
 * output and sending it out to players, and calling "heartbeat" functions
 * such as mobile_activity().
 */
void game_loop(socket_t mother_desc) {
	void reset_time(void);

	fd_set input_set, output_set, exc_set, null_set;
	struct timeval last_time, opt_time, process_time, temp_time;
	struct timeval before_sleep, now, timeout;
	char comm[MAX_INPUT_LENGTH];
	descriptor_data *d, *next_d;
	int missed_pulses, maxdesc, aliased;

	/* initialize various time values */
	null_time.tv_sec = 0;
	null_time.tv_usec = 0;
	opt_time.tv_usec = OPT_USEC;
	opt_time.tv_sec = 0;
	FD_ZERO(&null_set);

	gettimeofday(&last_time, (struct timezone *) 0);

	/* The Main Loop.  The Big Cheese.  The Top Dog.  The Head Honcho.  The.. */
	while (!empire_shutdown) {

		/** Sleep if we don't have any connections
		// This is OFF because it blocks map evolutions and workforce
		if (descriptor_list == NULL) {
			log("No connections. Going to sleep.");
			FD_ZERO(&input_set);
			FD_SET(mother_desc, &input_set);
			if (select(mother_desc + 1, &input_set, (fd_set *) 0, (fd_set *) 0, NULL) < 0) {
				if (errno == EINTR)
					log("Waking up to process signal.");
				else
					perror("SYSERR: Select coma");
				}
			else {
				log("New connection. Waking up.");
				reset_time();
			}
			gettimeofday(&last_time, (struct timezone *) 0);
		}
		*/
		
		/* Set up the input, output, and exception sets for select(). */
		FD_ZERO(&input_set);
		FD_ZERO(&output_set);
		FD_ZERO(&exc_set);
		FD_SET(mother_desc, &input_set);

		maxdesc = mother_desc;
		for (d = descriptor_list; d; d = d->next) {
			if (d->descriptor > maxdesc)
				maxdesc = d->descriptor;
			FD_SET(d->descriptor, &input_set);
			FD_SET(d->descriptor, &output_set);
			FD_SET(d->descriptor, &exc_set);
		}

		/*
		 * At this point, we have completed all input, output and heartbeat
		 * activity from the previous iteration, so we have to put ourselves
		 * to sleep until the next 0.1 second tick.  The first step is to
		 * calculate how long we took processing the previous iteration.
		 */

		gettimeofday(&before_sleep, (struct timezone *) 0); /* current time */
		timediff(&process_time, &before_sleep, &last_time);

		/*
		 * If we were asleep for more than one pass, count missed pulses and sleep
		 * until we're resynchronized with the next upcoming pulse.
		 */
		if (process_time.tv_sec == 0 && process_time.tv_usec < OPT_USEC) {
			missed_pulses = 0;
		}
		else {
			missed_pulses = process_time.tv_sec * PASSES_PER_SEC;
			missed_pulses += process_time.tv_usec / OPT_USEC;
			process_time.tv_sec = 0;
			process_time.tv_usec = process_time.tv_usec % OPT_USEC;
		}

		/* Calculate the time we should wake up */
		timediff(&temp_time, &opt_time, &process_time);
		timeadd(&last_time, &before_sleep, &temp_time);

		/* Now keep sleeping until that time has come */
		gettimeofday(&now, (struct timezone *) 0);
		timediff(&timeout, &last_time, &now);

		/* Go to sleep */
		do {
			empire_sleep(&timeout);
			gettimeofday(&now, (struct timezone *) 0);
			timediff(&timeout, &last_time, &now);
		} while (timeout.tv_usec || timeout.tv_sec);

		/* Poll (without blocking) for new input, output, and exceptions */
		if (select(maxdesc + 1, &input_set, &output_set, &exc_set, &null_time) < 0) {
			perror("SYSERR: Select poll");
			return;
		}
		/* If there are new connections waiting, accept them. */
		if (FD_ISSET(mother_desc, &input_set))
			new_descriptor(mother_desc);

		/* Kick out the freaky folks in the exception set and marked for close */
		for (d = descriptor_list; d; d = next_d) {
			next_d = d->next;
			if (FD_ISSET(d->descriptor, &exc_set)) {
				FD_CLR(d->descriptor, &input_set);
				FD_CLR(d->descriptor, &output_set);
				close_socket(d);
			}
		}

		/* Process descriptors with input pending */
		for (d = descriptor_list; d; d = next_d) {
			next_d = d->next;
			if (FD_ISSET(d->descriptor, &input_set))
				if (process_input(d) < 0)
					close_socket(d);
		}

		/* Process commands we just read from process_input */
		for (d = descriptor_list; d; d = next_d) {
			next_d = d->next;

			/*
			 * Not combined to retain --(d->wait) behavior. -gg 2/20/98
			 * If no wait state, no subtraction.  If there is a wait
			 * state then 1 is subtracted. Therefore we don't go less
			 * than 0 ever and don't require an 'if' bracket. -gg 2/27/99
			 */
			if (d->character) {
				GET_WAIT_STATE(d->character) -= (GET_WAIT_STATE(d->character) > 0);

				if (GET_WAIT_STATE(d->character))
					continue;
			}

			if (!get_from_q(&d->input, comm, &aliased))
				continue;

			if (d->character) {
				/* Reset the idle timer & pull char back from void if necessary */
				d->character->char_specials.timer = 0;
				GET_WAIT_STATE(d->character) = 1;
			}
			d->has_prompt = 0;

			if (d->showstr_count) /* Reading something w/ pager */
				show_string(d, comm);
			else if (d->str && (STATE(d) != CON_PLAYING || d->straight_to_editor))		/* Writing boards, mail, etc. */
				string_add(d, comm);
			else if (STATE(d) != CON_PLAYING) /* In creation, etc. */
				nanny(d, comm);
			else {			/* else: we're playing normally. */
				if (aliased)		/* To prevent recursive aliases. */
					d->has_prompt = 1;	/* To get newline before next cmd output. */
				else if (perform_alias(d, comm))    /* Run it through aliasing system */
					get_from_q(&d->input, comm, &aliased);
				
				if (PRF_FLAGGED(d->character, PRF_EXTRA_SPACING)) {
					SEND_TO_Q("\r\n", d);	// for people who don't get a crlf from localecho
				}
				
				command_interpreter(d->character, comm); /* Send it to interpreter */
			}
		}

		/* Send queued output out to the operating system (ultimately to user). */
		for (d = descriptor_list; d; d = next_d) {
			next_d = d->next;
			
			send_stacked_msgs(d);
			
			if (*(d->output) && FD_ISSET(d->descriptor, &output_set)) {
				/* Output for this player is ready */
				if (process_output(d) < 0) {
					// process_output actually kills it itself
					// close_socket(d);
					continue;
				}
				else
					d->has_prompt = 1;
			}
		}

		/* Print prompts for other descriptors who had no other output */
		for (d = descriptor_list; d; d = next_d) {
			next_d = d->next;
			
			if (!d->has_prompt) {
				char prompt[MAX_STRING_LENGTH];
				int wantsize;
				
				strcpy(prompt, make_prompt(d));
				wantsize = strlen(prompt);
				strncpy(prompt, ProtocolOutput(d, prompt, &wantsize), MAX_STRING_LENGTH);
				prompt[MAX_STRING_LENGTH-1] = '\0';
				
				// force a color code flush
				snprintf(prompt + strlen(prompt), sizeof(prompt) - strlen(prompt), "%s", flush_reduced_color_codes(d));
				
				if (write_to_descriptor(d->descriptor, prompt) >= 0) {
					d->has_prompt = 1;
				}
			}
		}

		/* Kick out folks in the CON_CLOSE or CON_DISCONNECT state */
		for (d = descriptor_list; d; d = next_d) {
			next_d = d->next;
			if (STATE(d) == CON_CLOSE || STATE(d) == CON_DISCONNECT)
				close_socket(d);
		}

		/*
		 * Now, we execute as many pulses as necessary--just one if we haven't
		 * missed any pulses, or make up for lost time if we missed a few
		 * pulses by sleeping for too long.
		 */
		missed_pulses++;

		if (missed_pulses <= 0) {
			log("SYSERR: **BAD** MISSED_PULSES NONPOSITIVE (%d), TIME GOING BACKWARDS!!", missed_pulses);
			missed_pulses = 1;
		}

		/* If we missed more than 30 seconds worth of pulses, just do 30 secs */
		if (missed_pulses > (30 * PASSES_PER_SEC)) {
			log("SYSERR: Missed %d seconds worth of pulses.", missed_pulses / PASSES_PER_SEC);
			missed_pulses = 30 * PASSES_PER_SEC;
		}

		/* Now execute the heartbeat functions */
		catch_up_combat = TRUE;
		catch_up_actions = TRUE;
		catch_up_mobs = TRUE;
		while (missed_pulses--) {
			heartbeat(++pulse);
		}

		/* Update tics_passed for deadlock protection */
		++tics_passed;
	}
}


/* Init sockets, run game, and cleanup sockets */
void init_game(ush_int port) {
	void empire_srandom(unsigned long initial_seed);

	empire_srandom(time(0));

	log("Finding player limit.");
	max_players = get_max_players();

	if (!reboot_recovery) {
		log("Opening mother connection.");
		mother_desc = init_socket(port);
	}

	dg_event_init();

	/* set up hash table for find_char() */
	init_lookup_table();

	boot_db();

	log("Signal trapping.");
	signal_setup();

	if (reboot_recovery)
		reboot_recover();

	log("Entering game loop.");
	game_loop(mother_desc);

	save_all_players();

	log("Closing all sockets.");
	while (descriptor_list)
		close_socket(descriptor_list);

	CLOSE_SOCKET(mother_desc);

	log("Normal termination of game.");
}


int main(int argc, char **argv) {
	void boot_world();

	int pos = 1;
	const char *dir;

	/* Initialize these to check for overruns later. */
	plant_magic(buf);
	plant_magic(buf1);
	plant_magic(buf2);
	plant_magic(arg);

	port = DFLT_PORT;
	dir = DFLT_DIR;

	while ((pos < argc) && (*(argv[pos]) == '-')) {
		switch (*(argv[pos] + 1)) {
			case 'C':
				reboot_recovery = TRUE;
				mother_desc = atoi(argv[pos]+2);
				break;
			case 'o':
				if (*(argv[pos] + 2))
					LOGNAME = argv[pos] + 2;
				else if (++pos < argc)
					LOGNAME = argv[pos];
				else {
					puts("SYSERR: File name to log to expected after option -o.");
					exit(1);
				}
				break;
			case 'd':
				if (*(argv[pos] + 2))
					dir = argv[pos] + 2;
				else if (++pos < argc)
					dir = argv[pos];
				else {
					puts("SYSERR: Directory arg expected after option -d.");
					exit(1);
				}
				break;
			case 'c':
				scheck = 1;
				puts("Syntax check mode enabled.");
				break;
			case 'q':
				no_auto_deletes = 1;
				puts("Quick boot mode -- auto-deletes suppressed.");
				break;
			case 'r':
				wizlock_level = 1;
				puts("Restricting game -- no new players allowed.");
				break;
			case 'w':
				wizlock_level = LVL_START_IMM;
				puts("Restricting game.");
				break;
			case 'h':
				/* From: Anil Mahajan <amahajan@proxicom.com> */
				printf("Usage: %s [-c] [-q] [-r] [-o filename] [-d pathname] [port #]\n"
					   "  -c             Enable syntax check mode.\n"
					   "  -d <directory> Specify library directory (defaults to 'lib').\n"
					   "  -h             Print this command line argument help.\n"
					   "  -o <file>      Write log to <file> instead of stderr.\n"
					   "  -q             Quick boot (doesn't auto-delete players)\n"
					   "  -r             Restrict MUD -- no new players allowed.\n",
					argv[0]);
				exit(0);
			default:
				printf("SYSERR: Unknown option -%c in argument string.\n", *(argv[pos] + 1));
				break;
		}
		pos++;
	}

	if (pos < argc) {
		if (!isdigit(*argv[pos])) {
			printf("Usage: %s [-c] [-m] [-q] [-r] [-s] [-d pathname] [port #]\n", argv[0]);
			exit(1);
		}
		else if ((port = atoi(argv[pos])) <= 1024) {
			printf("SYSERR: Illegal port number %d.\n", port);
			exit(1);
		}
	}

	/* All arguments have been parsed, try to open log file. */
	setup_log(LOGNAME, STDERR_FILENO);

	/*
	 * Moved here to distinguish command line options and to show up
	 * in the log if stderr is redirected to a file.
	 */
	log("%s", version);
	log("%s", DG_SCRIPT_VERSION);

	if (chdir(dir) < 0) {
		perror("SYSERR: Fatal error changing to data directory");
		exit(1);
	}
	log("Using %s as data directory.", dir);

	if (scheck) {
		boot_world();
		log("Done.");
	}
	else {
		log("Running game on port %d.", port);
		init_game(port);
	}

	return (0);
}


int open_logfile(const char *filename, FILE *stderr_fp) {
	if (stderr_fp)	/* freopen() the descriptor. */
		logfile = freopen(filename, "w", stderr_fp);
	else
		logfile = fopen(filename, "w");

	if (logfile) {
		printf("Using log file '%s'%s.\n", filename, stderr_fp ? " with redirection" : "");
		return (TRUE);
	}

	printf("SYSERR: Error opening file '%s': %s\n", filename, strerror(errno));
	return (FALSE);
}


/* Prefer the file over the descriptor. */
void setup_log(const char *filename, int fd) {
	FILE *s_fp;

#if defined(__MWERKS__) || defined(__GNUC__)
	s_fp = stderr;
#else
	if ((s_fp = fdopen(STDERR_FILENO, "w")) == NULL) {
		puts("SYSERR: Error opening stderr, trying stdout.");

		if ((s_fp = fdopen(STDOUT_FILENO, "w")) == NULL) {
			puts("SYSERR: Error opening stdout, trying a file.");

			/* If we don't have a file, try a default. */
			if (filename == NULL || *filename == '\0')
				filename = "log/syslog";
			}
		}
#endif

	if (filename == NULL || *filename == '\0') {
		/* No filename, set us up with the descriptor we just opened. */
		logfile = s_fp;
		puts("Using file descriptor for logging.");
		return;
	}

	/* We honor the default filename first. */
	if (open_logfile(filename, s_fp))
		return;

	/* Well, that failed but we want it logged to a file so try a default. */
	if (open_logfile("log/syslog", s_fp))
		return;

	/* Ok, one last shot at a file. */
	if (open_logfile("syslog", s_fp))
		return;

	/* Erp, that didn't work either, just die. */
	puts("SYSERR: Couldn't open anything to log to, giving up.");
	exit(1);
}


void reboot_recover(void) {
	extern void enter_player_game(descriptor_data *d, int dolog, bool fresh);
	extern bool global_mute_slash_channel_joins;

	char buf[MAX_STRING_LENGTH];
	descriptor_data *d;
	char_data *plr, *ldr;
	FILE *fp;
	char host[1024], protocol_info[1024], line[256];
	int desc, plid, leid;
	bool fOld;
	char name[MAX_INPUT_LENGTH];

	log("Reboot is recovering players...");

	if (!(fp = fopen(REBOOT_FILE, "r"))) {
		perror("reboot_recover: fopen");
		log("Reboot file not found. Exiting.\r\n");
		exit(1);
	}

	unlink(REBOOT_FILE);
	
	// no-spam
	global_mute_slash_channel_joins = TRUE;

	// first pass: load players
	for (;;) {
		fOld = TRUE;
		fscanf(fp, "%d %s %s %s\n", &desc, name, host, protocol_info);
		
		// end indicator (endicator?)
		if (desc == -1) {
			break;
		}

		sprintf(buf, "\r\nRestoring %s...\r\n", name);
		if (write_to_descriptor(desc, buf) < 0) {
			close(desc);
			continue;
		}
		log (" %-20.20s - %s", name, host);
		CREATE(d, descriptor_data, 1);
		memset((char *) d, 0, sizeof (descriptor_data));
		init_descriptor(d, desc);

		d->host = str_dup(host);
		LL_APPEND(descriptor_list, d);

		d->connected = CON_CLOSE;
				
		d->character = load_player(name, TRUE);
		if (d->character) {
			REMOVE_BIT(PLR_FLAGS(d->character), PLR_MAILING);
			d->character->desc = d;
		}
		else {
			fOld = FALSE;
		}
		
		// recover protocol settings
		CopyoverSet(d, protocol_info);

		if (!fOld) {
			write_to_descriptor(desc, "\r\nSomehow, your character couldn't be loaded.\r\n");
			close_socket(d);
		}
		else {
			write_to_descriptor(desc, "\033[0mRecovery complete.\r\n\r\n");
			enter_player_game(d, FALSE, FALSE);
			d->connected = CON_PLAYING;
		}
	}
	
	// second pass: rebuild groups
	while (get_line(fp, line)) {
		// end
		if (*line == '$') {
			break;
		}
		else if (*line == 'G' && sscanf(line, "G %d %d", &plid, &leid) == 2) {
			if ((plr = is_playing(plid)) && (ldr = is_playing(leid))) {
				if (!GROUP(ldr)) {
					create_group(ldr);
				}
				if (GROUP(ldr) && !GROUP(plr)) {
					join_group(plr, GROUP(ldr));
				}
			}
		}
		else {
			log("SYSERR: Unknown line '%s' in reboot file", line);
		}
	}
	
	global_mute_slash_channel_joins = FALSE;
	fclose(fp);
}
