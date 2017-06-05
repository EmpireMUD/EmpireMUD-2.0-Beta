/* ************************************************************************
*   File: act.comm.c                                      EmpireMUD 2.0b5 *
*  Usage: Player-level communication commands                             *
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
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "dg_scripts.h"

/**
* Contents:
*   Helper Functions
*   Pub_Comm
*   Slash Channels
*   Communication Commands
*/

// externs
void clear_last_act_message(descriptor_data *desc);

// locals
struct player_slash_channel *find_on_slash_channel(char_data *ch, int id);
void process_add_to_channel_history(struct channel_history_data **history, char *message);


#define MAX_RECENT_CHANNELS		20		/* Number of pub_comm uses to remember */

#define PUB_COMM_OOC  0
#define PUB_COMM_GLOBAL  1
#define PUB_COMM_SHORT_RANGE  2

// data for public channels
struct {
	char *name;	// channel name/verb
	char *color;	// color code, e.g. \ty
	int type;	// PUB_COMM_x
	int min_level;	// absolute minimum
	bitvector_t ignore_flag;	// PRF_x flag for ignoring this channel
	int history;
} pub_comm[NUM_CHANNELS] = {
	{ "shout", "\ty", PUB_COMM_SHORT_RANGE, 0, PRF_DEAF, CHANNEL_HISTORY_SAY },
	{ "GOD", "\tr", PUB_COMM_OOC, LVL_GOD, PRF_NOGODNET, CHANNEL_HISTORY_GOD },
	{ "IMMORTAL", "\tc", PUB_COMM_OOC, LVL_START_IMM, PRF_NOWIZ, CHANNEL_HISTORY_GOD },
};


 //////////////////////////////////////////////////////////////////////////////
//// HELPER FUNCTIONS ////////////////////////////////////////////////////////

/**
* Adds a new entry to ch's ignore list ONLY if there is free space.
*
* @param char_data *ch The person who typed ignore.
* @param int idnum The idnum of the player to ignore.
* @return TRUE if successful, FALSE if the list is full.
*/
bool add_ignore(char_data *ch, int idnum) {
	int iter;
	bool found = FALSE;
	
	for (iter = 0; iter < MAX_IGNORES && !found; ++iter) {
		if (GET_IGNORE_LIST(ch, iter) == 0 || !find_player_index_by_idnum(GET_IGNORE_LIST(ch, iter))) {
			GET_IGNORE_LIST(ch, iter) = idnum;
			found = TRUE;
		}
	}
	
	return found;
}


/**
* @param descriptor_data *desc
* @param int type CHANNEL_HISTORY_x -- structs.h
* @param char *message full string including sender
*/
void add_to_channel_history(char_data *ch, int type, char *message) {
	if (!REAL_NPC(ch)) {
		process_add_to_channel_history(&GET_HISTORY(REAL_CHAR(ch), type), message);
	}
}


/**
* Adds a message to a player's /history for a channel.
*
* @param char_data *ch The player.
* @param struct slash_channel *chan The channel.
* @param char *message The message to add.
*/
void add_to_slash_channel_history(char_data *ch, struct slash_channel *chan, char *message) {
	struct player_slash_history *psh;
	
	if (!ch || IS_NPC(ch) || !chan || !message || !*message) {
		return;	// don't bother
	}
	
	HASH_FIND_STR(GET_SLASH_HISTORY(ch), chan->lc_name, psh);
	if (!psh) {
		CREATE(psh, struct player_slash_history, 1);
		psh->channel = str_dup(chan->lc_name);
		HASH_ADD_STR(GET_SLASH_HISTORY(ch), channel, psh);
	}
	
	process_add_to_channel_history(&(psh->history), message);
}


/**
* @param char_data *ch The player to check.
* @param char_data *victim The person talking (potentially ignored by ch).
* @return bool TRUE if ch is ignoring victim
*/
bool is_ignoring(char_data *ch, char_data *victim) {
	int iter;
	bool found = FALSE;
	
	if (!REAL_NPC(ch) && !REAL_NPC(victim)) {
		// only bother checking if neither is an immortal -- immortals cannot ignore or be ignored
		if (!IS_IMMORTAL(REAL_CHAR(ch)) && !IS_IMMORTAL(REAL_CHAR(victim))) {
			for (iter = 0; iter < MAX_IGNORES && !found; ++iter) {
				if (GET_IGNORE_LIST(REAL_CHAR(ch), iter) == GET_IDNUM(REAL_CHAR(victim))) {
					found = TRUE;
				}
			}
		}
	}
	
	return found;
}


/**
* Verifies the tellability of a person -- sends its own error message if not.
*
* @param char_data *ch The teller
* @param char_data *vict The recipient
* @return TRUE if it's ok to send that person a tell
*/
int is_tell_ok(char_data *ch, char_data *vict) {
	if (ch == vict)
		msg_to_char(ch, "You try to tell yourself something.\r\n");
	else if (!IS_APPROVED(ch) && !IS_IMMORTAL(ch) && !IS_IMMORTAL(vict) && config_get_bool("tell_approval")) {
		// can always tell immortals
		send_config_msg(ch, "need_approval_string");
	}
	else if (PRF_FLAGGED(ch, PRF_NOTELL))
		msg_to_char(ch, "You can't tell other people while you have notell on.\r\n");
	else if (!REAL_NPC(vict) && !vict->desc)        /* linkless */
		act("$E's linkless at the moment.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	else if (PRF_FLAGGED(vict, PRF_NOTELL))
		act("$E can't hear you.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	else if (is_ignoring(ch, vict)) {
		msg_to_char(ch, "You cannot send a tell to someone you're ignoring.\r\n");
	}
	else if (is_ignoring(vict, ch)) {
		act("$E is ignoring you.", FALSE, ch, NULL, vict, TO_CHAR | TO_SLEEP);
	}
	else
		return (TRUE);

	return (FALSE);
}


/**
* Send a tell to a person, sanity checks are done below
*
* @param char_data *ch The teller
* @param char_data *vict The person receiving the tell
* @param char *arg The string to send
*/
void perform_tell(char_data *ch, char_data *vict, char *arg) {
	char lbuf[MAX_STRING_LENGTH];
	char color;

	// for channel history
	if (vict->desc) {
		clear_last_act_message(vict->desc);
	}
	
	color = (!IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_TELL)) ? GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_TELL) : 'r';
	sprintf(buf, "$o tells you, '%s\t%c'\tn", arg, color);
	msg_to_char(vict, "\t%c", color);
	act(buf, FALSE, ch, 0, vict, TO_VICT | TO_SLEEP | TO_NODARK);
	
	// channel history
	if (vict->desc && vict->desc->last_act_message) {
		// the message was sent via act(), we can retrieve it from the desc
		sprintf(lbuf, "\t%c%s", color, vict->desc->last_act_message);
		add_to_channel_history(vict, CHANNEL_HISTORY_TELLS, lbuf);
	}

	if (PRF_FLAGGED(ch, PRF_NOREPEAT))
		send_config_msg(ch, "ok_string");
	else {
		// for channel history
		if (ch->desc) {
			clear_last_act_message(ch->desc);
		}
		
		color = (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_TELL)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_TELL) : 'r';
	
		sprintf(buf, "\t%cYou tell $O, '%s\t%c'\tn", color, arg, color);
		act(buf, FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP | TO_NODARK);	

		if (ch->desc && ch->desc->last_act_message) {
			// the message was sent via act(), we can retrieve it from the desc
			add_to_channel_history(ch, CHANNEL_HISTORY_TELLS, ch->desc->last_act_message);
		}
	}

	if (IS_AFK(vict)) {
		act("$E is AFK and may not receive your message.", FALSE, ch, 0, vict, TO_CHAR);
	}

	if (!REAL_NPC(vict) && !REAL_NPC(ch)) {
		GET_LAST_TELL(vict) = GET_IDNUM(ch);
	}
}


/**
* @param struct channel_history_data **history a pointer to a history storage pointer
* @param char *message the message to store
*/
void process_add_to_channel_history(struct channel_history_data **history, char *message) {
	struct channel_history_data *new, *old, *chd_iter, *last;
	int count;
	
	// setup
	CREATE(new, struct channel_history_data, 1);
	new->message = strdup(message);
	new->timestamp = time(0);
	new->next = NULL;
	
	// find the end
	last = NULL;
	count = 0;
	for (chd_iter = *history; chd_iter; chd_iter = chd_iter->next) {
		++count;
		if (!(chd_iter->next)) {
			last = chd_iter;
		}
	}
	
	// append at end
	++count;
	if (last != NULL) {
		last->next = new;
	}
	else {
		*history = new;
	}
	
	// check limit
	if (count > MAX_RECENT_CHANNELS) {
		// remove the first one
		old = *history;
		*history = old->next;
		if (old->message) {
			free(old->message);
		}
		free(old);
	}
}


/**
* Removes an idnum from a player's ignore list, if it's on there.
*
* @param char_data *ch The person trying to stop ignoring someone.
* @param int idnum The id of the person to un-ignore.
*/
void remove_ignore(char_data *ch, int idnum) {
	int iter;
	
	for (iter = 0; iter < MAX_IGNORES; ++iter) {
		if (GET_IGNORE_LIST(ch, iter) == idnum) {
			GET_IGNORE_LIST(ch, iter) = 0;
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// PUB_COMM ////////////////////////////////////////////////////////////////

/***********************************************************************
 * generalized communication func, originally by Fred C. Merkel (Torg) *
 * This function was adapted and rewritten by Paul Clarke for tbgMUD   *
 * and again for EmpireMUD.                                            *
 **********************************************************************/

// uses subcmd as a position in the pub_comm array
ACMD(do_pub_comm) {
	char msgbuf[MAX_STRING_LENGTH], someonebuf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	char level_string[10], invis_string[10];
	descriptor_data *desc;
	bool emote = FALSE;
	int level = 0;

	skip_spaces(&argument);

	// simple validation
	if (AFF_FLAGGED(ch, AFF_CHARM) && !ch->desc) {
		msg_to_char(ch, "You can't %s.\r\n", pub_comm[subcmd].name);
	}
	else if (!IS_APPROVED(ch) && !IS_IMMORTAL(ch) && config_get_bool("chat_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (ACCOUNT_FLAGGED(ch, ACCT_MUTED)) {
		msg_to_char(ch, "You are muted and can't use the %s channel.\r\n", pub_comm[subcmd].name);
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_SILENT) && pub_comm[subcmd].type != PUB_COMM_OOC) {
		msg_to_char(ch, "You try to %s, but no words come out!\r\n", pub_comm[subcmd].name);
	}
	else if (pub_comm[subcmd].ignore_flag != NOBITS && PRF_FLAGGED(ch, pub_comm[subcmd].ignore_flag)) {
		msg_to_char(ch, "You are currently ignoring the %s channel. Use toggle to turn it back on first.\r\n", pub_comm[subcmd].name);
	}
	else if (!*argument) {
		msg_to_char(ch, "Yes, %s, fine, %s we must, but WHAT???\r\n", pub_comm[subcmd].name, pub_comm[subcmd].name);
	}
	else {
		// look for emote or level
		while (*argument == '*' || *argument == '#') {
			if (*argument == '*') {
				emote = TRUE;
				++argument;
			}
			else if (*argument == '#') {
				// #1 indicates "level 1+"
				argument = any_one_arg(argument+1, arg);
				skip_spaces(&argument);
				level = atoi(arg);
			}
		}
		
		// in case there was an emote or level indicator, skip spaces again
		skip_spaces(&argument);
		level = MAX(level, pub_comm[subcmd].min_level);
		
		if (level > GET_REAL_LEVEL(ch)) {
			msg_to_char(ch, "You can't speak above your own level.\r\n");
			return;
		}

		// string setup
		*level_string = '\0';
		*invis_string = '\0';
		
		if (level > pub_comm[subcmd].min_level) {
			sprintf(level_string, " <%d>", level);
		}
		if (!IS_NPC(ch) && GET_INVIS_LEV(ch) > 0) {
			sprintf(invis_string, " (i%d)", GET_INVIS_LEV(ch));
		}
		
		// message to the player
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			// for channel history
			if (ch->desc) {
				clear_last_act_message(ch->desc);
			}

			// build string into msgbuf
			switch (pub_comm[subcmd].type) {
				case PUB_COMM_GLOBAL:
				case PUB_COMM_SHORT_RANGE: {
					sprintf(msgbuf, "%sYou%s %s%s, '%s%s'\tn", pub_comm[subcmd].color, invis_string, pub_comm[subcmd].name, level_string, argument, pub_comm[subcmd].color);
					break;
				}
				case PUB_COMM_OOC:
				default: {
					if (emote) {
						sprintf(msgbuf, "[%s%s\tn%s%s] $o %s\tn", pub_comm[subcmd].color, pub_comm[subcmd].name, invis_string, level_string, argument);
					}
					else {
						sprintf(msgbuf, "[%s%s\tn $o%s%s]: %s\tn", pub_comm[subcmd].color, pub_comm[subcmd].name, invis_string, level_string, argument);
					}
					break;
				}
			}
	
			// send the message to ch
			act(msgbuf, FALSE, ch, 0, 0, TO_CHAR | TO_SLEEP);
			
			// trap last act() and send to the history
			if (ch->desc && ch->desc->last_act_message && pub_comm[subcmd].history != NO_HISTORY) {
				add_to_channel_history(ch, pub_comm[subcmd].history, ch->desc->last_act_message);
			}
		}
		
		// message to others:		
		switch (pub_comm[subcmd].type) {
			case PUB_COMM_GLOBAL:
			case PUB_COMM_SHORT_RANGE: {
				// leading color code is handled later
				
				if (IS_DISGUISED(ch) || IS_MORPHED(ch)) {
					sprintf(msgbuf, "$n ($o)%s %ss%s, '%s%s'\tn", invis_string, pub_comm[subcmd].name, level_string, argument, pub_comm[subcmd].color);
				}
				else {
					sprintf(msgbuf, "$n%s %ss%s, '%s%s'\tn", invis_string, pub_comm[subcmd].name, level_string, argument, pub_comm[subcmd].color);
				}
				
				// invis version
				sprintf(someonebuf, "Someone %ss%s, '%s%s'\tn", pub_comm[subcmd].name, level_string, argument, pub_comm[subcmd].color);
				break;
			}
			case PUB_COMM_OOC:
			default: {
				if (emote) {
					sprintf(msgbuf, "[%s%s\tn%s%s] $o %s\tn", pub_comm[subcmd].color, pub_comm[subcmd].name, invis_string, level_string, argument);
					sprintf(someonebuf, "[%s%s\tn%s] Someone %s\tn", pub_comm[subcmd].color, pub_comm[subcmd].name, level_string, argument);
				}
				else {
					sprintf(msgbuf, "[%s%s\tn $o%s%s]: %s\tn", pub_comm[subcmd].color, pub_comm[subcmd].name, invis_string, level_string, argument);
					sprintf(someonebuf, "[%s%s\tn Someone%s]: %s\tn", pub_comm[subcmd].color, pub_comm[subcmd].name, level_string, argument);
				}
				break;
			}
		}
		
		for (desc = descriptor_list; desc; desc = desc->next) {
			// basic qualifications
			if (STATE(desc) == CON_PLAYING && desc != ch->desc && desc->character && !is_ignoring(desc->character, ch) && GET_REAL_LEVEL(desc->character) >= level) {
				// can hear the channel?
				if (pub_comm[subcmd].ignore_flag == NOBITS || !PRF_FLAGGED(desc->character, pub_comm[subcmd].ignore_flag)) {
					// distance?
					if (pub_comm[subcmd].type != PUB_COMM_SHORT_RANGE || compute_distance(IN_ROOM(ch), IN_ROOM(desc->character)) <= 50) {
						// quiet room?
						if (pub_comm[subcmd].type == PUB_COMM_OOC || !ROOM_AFF_FLAGGED(IN_ROOM(desc->character), ROOM_AFF_SILENT)) {
							// special color handling for non-ooc channels
							if (pub_comm[subcmd].type != PUB_COMM_OOC) {
								// use act() so that nobody gets the color code that wouldn't get the rest of the string
								act(pub_comm[subcmd].color, FALSE, ch, NULL, desc->character, TO_VICT | TO_SLEEP | TO_NODARK);
							}
							
							// channel history
							clear_last_act_message(desc);
							
							// send message
							if (CAN_SEE_NO_DARK(desc->character, ch)) {
								act(msgbuf, FALSE, ch, NULL, desc->character, TO_VICT | TO_SLEEP | TO_NODARK);
							}
							else {
								act(someonebuf, FALSE, ch, NULL, desc->character, TO_VICT | TO_SLEEP | TO_NODARK);
							}
							
							// get the message from act() and put it in history
							if (desc->last_act_message && pub_comm[subcmd].history != NO_HISTORY) {
								// color handling for history
								if (pub_comm[subcmd].type != PUB_COMM_OOC) {
									sprintf(lbuf, "%s%s", pub_comm[subcmd].color, desc->last_act_message);
								}
								else {
									strcpy(lbuf, desc->last_act_message);
								}
								
								add_to_channel_history(desc->character, pub_comm[subcmd].history, lbuf);
							}
						}
					}
				}
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// SLASH CHANNELS //////////////////////////////////////////////////////////

// this can be turned on to prevent spammy channel joins on reboot
bool global_mute_slash_channel_joins = FALSE;

// local vars
struct slash_channel *slash_channel_list = NULL;	// master list
int top_slash_channel_id = 0;


/**
* Sends channel join/leave messages. This function is blocked by the
* !CHANNEL-JOINS preference (toggle channel-joins).
*
* @param struct slash_channel *chan The slash channel to announce to.
* @param const char *messg... String format of the messages
*/
void announce_to_slash_channel(struct slash_channel *chan, const char *messg, ...) {
	struct player_slash_channel *slash;
	char output[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	descriptor_data *d;
	va_list tArgList;
	
	if (messg) {
		va_start(tArgList, messg);
		vsprintf(output, messg, tArgList);
		sprintf(lbuf, "[\t%c/%s\tn] %s\tn\r\n", chan->color, chan->name, output);

		for (d = descriptor_list; d; d = d->next) {
			if (d->character && STATE(d) == CON_PLAYING && !PRF_FLAGGED(d->character, PRF_NO_CHANNEL_JOINS)) {
				if ((slash = find_on_slash_channel(d->character, chan->id))) {
					SEND_TO_Q(lbuf, d);
					// no longer putting announcements on history
					// process_add_to_channel_history(&(slash->history), lbuf);
				}
			}
		}

		va_end(tArgList);
	}
}


// picks a deterministic color based on name
char compute_slash_channel_color(char *name) {
	char *colors = "rgbymcaLoYjtRvGBlMCAJpOPTV";
	char *ptr;
	int sum = 0;
	
	// this just adds up the character values of the channel name to get an arbitrary sum
	for (ptr = name; *ptr; ++ptr) {
		sum += (int)(*ptr);
	}
	
	// choose a position in the color code list based on the sum
	return colors[sum % strlen(colors)];
}


/**
* Matches a slash channel name. It will take abbreviations but prefers exact
* matches.
*
* @param char *name the channel name
* @param bool exact If TRUE, will not accept abbrevs
* @return struct slash_channel* the channel, or NULL if none
*/
struct slash_channel *find_slash_channel_by_name(char *name, bool exact) {
	struct slash_channel *chan, *found = NULL, *partial = NULL;
	
	for (chan = slash_channel_list; !found && chan; chan = chan->next) {
		if (!str_cmp(name, chan->name)) {
			found = chan;
		}
		else if (!partial && is_abbrev(name, chan->name) && !exact) {
			partial = chan;
		}
	}
	
	return (found ? found : partial);
}


/**
* @param int id the channel id
* @return struct slash_channel* the channel, or NULL if none
*/
struct slash_channel *find_slash_channel_by_id(int id) {
	struct slash_channel *chan, *found = NULL;
	
	for (chan = slash_channel_list; !found && chan; chan = chan->next) {
		if (chan->id == id) {
			found = chan;
		}
	}
	
	return found;
}


/**
* Finds a slash channel by name, of only the ones the player is on.
*
* @param char_data *ch The player.
* @param char *name The typed-in name.
* @return struct player_slash_channel* The channel, if any.
*/
struct slash_channel *find_slash_channel_for_char(char_data *ch, char *name) {
	struct slash_channel *slash, *partial = NULL;
	
	for (slash = slash_channel_list; slash; slash = slash->next) {
		if (find_on_slash_channel(ch, slash->id)) {
			if (!str_cmp(name, slash->name)) {
				return slash;
			}
			else if (!partial && is_abbrev(name, slash->name)) {
				partial = slash;
			}
		}
	}
	
	return partial;	// if any
}


/**
* @param char_data *ch the player
* @param int id slash channel id
* @return bool TRUE if ch is on the slash channel
*/
struct player_slash_channel *find_on_slash_channel(char_data *ch, int id) {
	struct player_slash_channel *slash, *found = NULL;
	
	for (slash = GET_SLASH_CHANNELS(ch); !found && slash; slash = slash->next) {
		if (slash->id == id) {
			found = slash;
		}
	}
	
	return found;
}


/**
* Skips (copies over) a leading slash in the string.
*
* @param char *string The string to MODIFY (deletes the leading slash, if any).
*/
void skip_slash(char *string) {
	int iter;
	
	// short-circuit
	if (*string != '/') {
		return;
	}
	
	// we already know the first letter is a /
	for (iter = 1; iter <= strlen(string); ++iter) {
		string[iter-1] = string[iter];
	}
}


void speak_on_slash_channel(char_data *ch, struct slash_channel *chan, char *argument) {
	struct player_slash_channel *slash;
	char lbuf[MAX_STRING_LENGTH], invis_string[10];
	descriptor_data *desc;
	char_data *vict;
	char color;
	bool emote = FALSE;

	if (ACCOUNT_FLAGGED(ch, ACCT_MUTED)) {
		msg_to_char(ch, "You can't do that while muted.\r\n");
		return;
	}
	if (!IS_APPROVED(ch) && !IS_IMMORTAL(ch) && config_get_bool("chat_approval")) {
		send_config_msg(ch, "need_approval_string");
		return;
	}

	skip_spaces(&argument);

	if (!*argument) {
		msg_to_char(ch, "What do you want to say??\r\n");
		return;
	}

	if (*argument == '*') {
		emote = TRUE;
		argument++;
	}
	skip_spaces(&argument);

	if (GET_INVIS_LEV(ch) > 0) {
		sprintf(invis_string, " (i%d)", GET_INVIS_LEV(ch));
	}
	else {
		*invis_string = '\0';
	}

	// msg to self
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		// for channel history
		if (ch->desc) {
			clear_last_act_message(ch->desc);
		}
		
		color = (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_SLASH)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_SLASH) : '0';

		if (emote) {
			sprintf(lbuf, "\t%c[\t%c/%s\t%c%s] $o %s\tn", color, chan->color, chan->name, color, invis_string, argument);
		}
		else {
			sprintf(lbuf, "\t%c[\t%c/%s\t%c $o%s]: %s\tn", color, chan->color, chan->name, color, invis_string, argument);
		}
		act(lbuf, FALSE, ch, 0, 0, TO_CHAR | TO_SLEEP);
		
		if (ch->desc && ch->desc->last_act_message) {
			add_to_slash_channel_history(ch, chan, ch->desc->last_act_message);
		}
	}

	for (desc = descriptor_list; desc; desc = desc->next) {
		if (STATE(desc) == CON_PLAYING && (vict = desc->character) && !IS_NPC(vict) && vict != ch && !is_ignoring(vict, ch) && (slash = find_on_slash_channel(vict, chan->id))) {
			// for channel history
			clear_last_act_message(desc);

			color = (!IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_SLASH)) ? GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_SLASH) : '0';

			if (emote) {
				sprintf(lbuf, "\t%c[\t%c/%s\t%c%s] %s %s\tn", color, chan->color, chan->name, color, CAN_SEE_NO_DARK(vict, ch) ? invis_string : "", !CAN_SEE_NO_DARK(vict, ch) ? "Someone" : "$o", argument);
			}
			else {
				sprintf(lbuf, "\t%c[\t%c/%s\t%c %s%s]: %s\tn", color, chan->color, chan->name, color, !CAN_SEE_NO_DARK(vict, ch) ? "Someone" : "$o", CAN_SEE_NO_DARK(vict, ch) ? invis_string : "", argument);
			}

			act(lbuf, FALSE, ch, 0, vict, TO_VICT | TO_SLEEP | TO_NODARK);
		
			// the message was sent via act(), we can retrieve it from the desc
			if (desc->last_act_message) {
				add_to_slash_channel_history(vict, chan, desc->last_act_message);
			}
		}
	}
}


ACMD(do_slash_channel) {
	struct slash_channel *chan;
	struct channel_history_data *hist;
	struct player_slash_channel *slash, *temp;
	char arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	descriptor_data *desc;
	char_data *vict;
	int iter;
	bool ok, found;
	
	char *invalid_channel_names[] = { "/", "join", "leave", "who", "hist", "history", "list", "\n" };
	
	half_chop(argument, arg, arg2);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "No NPCs on slash channels.\r\n");
		return;
	}
	
	if (!*arg) {
		msg_to_char(ch, "Valid slash-channel commands are: /join, /leave, /who, /hist, /history, /check, /list, and /<channel>\r\n");
	}
	else if (!str_cmp(arg, "list")) {
		msg_to_char(ch, "You are on the following channels:\r\n");
		found = FALSE;
		for (slash = GET_SLASH_CHANNELS(ch); slash; slash = slash->next) {
			if ((chan = find_slash_channel_by_id(slash->id))) {
				msg_to_char(ch, "  \t%c/%s\tn\r\n", chan->color, chan->name);
				found = TRUE;
			}
		}
		
		if (!found) {
			msg_to_char(ch, "  none\r\n");
		}
	}
	else if (!str_cmp(arg, "check")) {
		// check channels: any player
		one_argument(arg2, arg3);
		
		if (!(vict = get_player_vis(ch, arg3, FIND_CHAR_WORLD | FIND_NO_DARK))) {
			send_config_msg(ch, "no_person");
			return;
		}
		if (vict == ch) {
			msg_to_char(ch, "If you want to check what channels you're on, use /list.\r\n");
			return;
		}
		
		// messaging
		if (IS_IMMORTAL(ch)) {
			msg_to_char(ch, "%s is on the following slash-channels:\r\n", PERS(vict, ch, TRUE));
		}
		else {
			msg_to_char(ch, "%s is on the following channels with you:\r\n", PERS(vict, ch, TRUE));
		}
		
		found = FALSE;
		for (slash = GET_SLASH_CHANNELS(vict); slash; slash = slash->next) {
			if ((chan = find_slash_channel_by_id(slash->id)) && (IS_IMMORTAL(ch) || find_on_slash_channel(ch, slash->id))) {
				msg_to_char(ch, "  \t%c/%s\tn\r\n", chan->color, chan->name);
				found = TRUE;
			}
		}
		
		if (!found) {
			msg_to_char(ch, "  none\r\n");
		}
	}
	else if (!str_cmp(arg, "join")) {
		// join channel: just first word
		skip_slash(arg2);
		any_one_arg(arg2, arg3);
		
		if (!*arg3) {
			msg_to_char(ch, "What channel do you want to join?\r\n");
			return;
		}
		
		// validate name
		ok = TRUE && *arg3;
		for (iter = 0; ok && *invalid_channel_names[iter] != '\n'; ++iter) {
			if (!str_cmp(arg3, invalid_channel_names[iter])) {
				ok = FALSE;
			}
		}
		
		// check alphanumeric
		for (iter = 0; ok && iter < strlen(arg3); ++iter) {
			if (!isalnum(arg3[iter])) {
				ok = FALSE;
			}
		}
		
		if (!ok || strlen(arg3) >= MAX_SLASH_CHANNEL_NAME_LENGTH) {
			msg_to_char(ch, "Invalid channel name.\r\n");
			return;
		}
		
		if (!(chan = find_slash_channel_by_name(arg3, TRUE))) {
			// create a channel
			CREATE(chan, struct slash_channel, 1);
			chan->next = slash_channel_list;
			slash_channel_list = chan;
			chan->id = ++top_slash_channel_id;
			chan->name = str_dup(arg3);
			strcpy(buf, arg3);
			chan->lc_name = str_dup(strtolower(buf));
			chan->color = compute_slash_channel_color(arg3);
		}
		
		if (find_on_slash_channel(ch, chan->id)) {
			msg_to_char(ch, "You're already on that channel.\r\n");
		}
		else {
			CREATE(slash, struct player_slash_channel, 1);
			slash->next = GET_SLASH_CHANNELS(ch);
			GET_SLASH_CHANNELS(ch) = slash;
			slash->id = chan->id;
			
			// announce it (this also messages the player)
			if (!global_mute_slash_channel_joins) {
				// announce to channel members
				if (GET_INVIS_LEV(ch) <= LVL_MORTAL && !PRF_FLAGGED(ch, PRF_INCOGNITO)) {
					announce_to_slash_channel(chan, "%s has joined the channel", PERS(ch, ch, TRUE));
				}
				// if player wouldn't see their own join announce
				if (GET_INVIS_LEV(ch) > LVL_MORTAL || PRF_FLAGGED(ch, PRF_NO_CHANNEL_JOINS) || PRF_FLAGGED(ch, PRF_INCOGNITO)) {
					msg_to_char(ch, "You join \t%c/%s\tn.\r\n", chan->color, chan->name);
				}
			}
		}
	}
	else if (!str_cmp(arg, "leave")) {
		skip_slash(arg2);
		
		// leave channel
		if (!*arg2) {
			msg_to_char(ch, "Usage: /leave <channel>\r\n");
		}
		else if (!(chan = find_slash_channel_for_char(ch, arg2))) {
			msg_to_char(ch, "You're not even on that channel.\r\n");
		}
		else {
			// announce
			msg_to_char(ch, "You leave \t%c/%s\tn.\r\n", chan->color, chan->name);
			
			while ((slash = find_on_slash_channel(ch, chan->id))) {
				REMOVE_FROM_LIST(slash, GET_SLASH_CHANNELS(ch), next);
				free(slash);
			}
			
			if (GET_INVIS_LEV(ch) <= LVL_MORTAL && !PRF_FLAGGED(ch, PRF_INCOGNITO)) {
				announce_to_slash_channel(chan, "%s has left the channel", PERS(ch, ch, TRUE));
			}
		}
	}
	else if (!str_cmp(arg, "who")) {
		skip_slash(arg2);
		
		// list players
		if (!*arg2) {
			msg_to_char(ch, "Usage: /who <channel>\r\n");
		}
		else if (!(chan = find_slash_channel_for_char(ch, arg2)) && (!IS_IMMORTAL(ch) || !(chan = find_slash_channel_by_name(arg2, TRUE)))) {
			msg_to_char(ch, "You're not even on that channel.\r\n");
		}
		else {
			msg_to_char(ch, "Players on \t%c/%s\tn:\r\n", chan->color, chan->name);
			
			Global_ignore_dark = TRUE;	// not darkness-based
			for (desc = descriptor_list; desc; desc = desc->next) {
				if (desc->character && !IS_NPC(desc->character) && STATE(desc) == CON_PLAYING && find_on_slash_channel(desc->character, chan->id) && CAN_SEE_NO_DARK(ch, desc->character) && INCOGNITO_OK(ch, desc->character)) {
					msg_to_char(ch, " %s\r\n", PERS(desc->character, ch, TRUE));
				}
			}
			Global_ignore_dark = FALSE;
		}
	}
	else if (!str_cmp(arg, "history") || !str_cmp(arg, "hist")) {
		skip_slash(arg2);
		
		// list players
		if (!*arg2) {
			msg_to_char(ch, "Usage: /history <channel>\r\n");
		}
		else if (!(chan = find_slash_channel_for_char(ch, arg2)) || !(slash = find_on_slash_channel(ch, chan->id))) {
			msg_to_char(ch, "You're not even on that channel.\r\n");
		}
		else {
			struct player_slash_history *psh;
			
			msg_to_char(ch, "Last %d messages on \t%c/%s\tn:\r\n", MAX_RECENT_CHANNELS, chan->color, chan->name);
			
			HASH_FIND_STR(GET_SLASH_HISTORY(ch), chan->lc_name, psh);
			if (psh) {
				LL_FOREACH(psh->history, hist) {
					send_to_char(hist->message, ch);
			
					// check for newline
					if (hist->message[strlen(hist->message) - 1] != '\n') {
						send_to_char("\r\n", ch);
					}
				}
			}
		}
	}
	else if ((chan = find_slash_channel_for_char(ch, arg))) {
		speak_on_slash_channel(ch, chan, arg2);
	}
	else {
		msg_to_char(ch, "You are not on a channel called '%s'.\r\n", arg);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMUNICATION COMMANDS //////////////////////////////////////////////////

ACMD(do_gsay) {
	descriptor_data *desc;
	char string[MAX_STRING_LENGTH], custom[MAX_STRING_LENGTH], normal[MAX_STRING_LENGTH];
	
	skip_spaces(&argument);

	if (!IS_APPROVED(ch) && !IS_IMMORTAL(ch) && config_get_bool("chat_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!GROUP(ch)) {
		msg_to_char(ch, "But you are not a member of a group!\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Yes, but WHAT do you want to group-say?\r\n");
	}
	else {		// message to ch
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			if (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_GSAY)) {
				sprintf(normal, "\t%c[gsay %s]: %s\tn\r\n", GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_GSAY), PERS(ch, ch, TRUE), argument);
			}
			else {
				sprintf(normal, "[\tGgsay\tn %s]: %s\tn\r\n", PERS(ch, ch, TRUE), argument);
			}
			
			delete_doubledollar(normal);
			send_to_char(normal, ch);
			if (ch->desc) {
				add_to_channel_history(ch, CHANNEL_HISTORY_SAY, normal);
			}
		}

		sprintf(normal, "[\tGgsay\tn $o]: %s\tn", argument);
		sprintf(custom, "\t%%c[gsay $o]: %s\tn", double_percents(argument));	// leaves in a %c

		// message to the party
		for (desc = descriptor_list; desc; desc = desc->next) {
			if (STATE(desc) != CON_PLAYING || !desc->character || desc->character == ch) {
				continue;
			}
			if (!GROUP(desc->character) || GROUP(desc->character) != GROUP(ch)) {
				continue;
			}
			if (is_ignoring(desc->character, ch)) {
				continue;
			}
			
			clear_last_act_message(desc);
			
			if (!IS_NPC(desc->character) && GET_CUSTOM_COLOR(desc->character, CUSTOM_COLOR_GSAY)) {
				sprintf(string, custom, GET_CUSTOM_COLOR(desc->character, CUSTOM_COLOR_GSAY));
			}
			else {
				strcpy(string, normal);
			}
			
			act(string, FALSE, ch, NULL, desc->character, TO_VICT | TO_SLEEP | TO_NODARK);
			add_to_channel_history(desc->character, CHANNEL_HISTORY_SAY, desc->last_act_message);
		}
	}
}


/**
* Uses subcmd as which channel history.
*/
ACMD(do_history) {
	const char *types[NUM_CHANNEL_HISTORY_TYPES] = { "god channels", "tells", "says", "empire chats" };
	struct channel_history_data *chd_iter;
	bool found_crlf;
	int pos;
	
	if (!REAL_NPC(ch)) {
		msg_to_char(ch, "Last %d %s:\r\n", MAX_RECENT_CHANNELS, types[subcmd]);
	
		for (chd_iter = GET_HISTORY(REAL_CHAR(ch), subcmd); chd_iter; chd_iter = chd_iter->next) {
			// verify has newline
			pos = strlen(chd_iter->message) - 1;
			found_crlf = FALSE;
			while (pos > 0 && !found_crlf) {
				if (chd_iter->message[pos] == '\r' || chd_iter->message[pos] == '\n') {	
					found_crlf = TRUE;
				}
				else if (chd_iter->message[pos] == '&' || chd_iter->message[pos-1] == '&') {
					// probably color code
					--pos;
				}
				else {
					// found something not a crlf or a color code
					break;
				}
			}
			
			// send message
			msg_to_char(ch, "%s\tn%s", chd_iter->message, (found_crlf ? "" : "\r\n"));
		}
	}
}


ACMD(do_ignore) {
	int iter, found_pos, total_ignores;
	player_index_data *index;
	char_data *victim;
	bool found;
	
	one_argument(argument, arg);
	
	// basic checks
	found_pos = NOTHING;
	total_ignores = 0;
	for (iter = 0; iter < MAX_IGNORES && found_pos == NOTHING; ++iter) {
		if (GET_IGNORE_LIST(ch, iter) != 0 && find_player_index_by_idnum(GET_IGNORE_LIST(ch, iter))) {
			++total_ignores;
			
			// is this the one they typed?
			if (*arg && (index = find_player_index_by_idnum(GET_IGNORE_LIST(ch, iter))) && !str_cmp(arg, index->name)) {
				found_pos = iter;
			}
		}
	}
	
	if (IS_IMMORTAL(ch)) {
		msg_to_char(ch, "Immortals cannot use ignore.\r\n");
	}
	else if (!*arg) {
		// just list ignores
		msg_to_char(ch, "You are ignoring: \r\n");
		
		found = FALSE;
		for (iter = 0; iter < MAX_IGNORES; ++iter) {
			if (GET_IGNORE_LIST(ch, iter) != 0 && (index = find_player_index_by_idnum(GET_IGNORE_LIST(ch, iter)))) {
				msg_to_char(ch, " %s\r\n", index->fullname);
				found = TRUE;
			}
		}
		
		if (!found) {
			msg_to_char(ch, " nobody\r\n");
		}
	}
	else if (found_pos != NOTHING) {
		// remove it
		msg_to_char(ch, "You are no longer ignoring %s.\r\n", (index = find_player_index_by_idnum(GET_IGNORE_LIST(ch, found_pos))) ? index->fullname : "Someone");
		remove_ignore(ch, GET_IGNORE_LIST(ch, found_pos));
	}
	else if (!(victim = get_player_vis(ch, arg, FIND_CHAR_WORLD | FIND_NO_DARK))) {
		msg_to_char(ch, "No one by that name here.\r\n");
	}
	else if (is_ignoring(ch, victim)) {
		// remove it
		msg_to_char(ch, "You are no longer ignoring %s.\r\n", GET_NAME(victim));
		remove_ignore(ch, GET_IDNUM(victim));
	}
	else if (ch == victim) {
		msg_to_char(ch, "If you ignore yourself, who will you have left to talk to?\r\n");
	}
	else if (IS_IMMORTAL(victim)) {
		msg_to_char(ch, "You cannot ignore an immortal.\r\n");
	}
	else if (total_ignores > MAX_IGNORES) {
		msg_to_char(ch, "Your ignore list is full. You cannot ignore more than %d people.\r\n", MAX_IGNORES);
	}
	else {
		// attempt to add
		if (add_ignore(ch, GET_IDNUM(victim))) {
			msg_to_char(ch, "You are now ignoring %s.\r\n", GET_NAME(victim));
		}
		else {
			msg_to_char(ch, "Your ignore list is full. You cannot ignore more than %d people.\r\n", MAX_IGNORES);
		}
	}
}


ACMD(do_page) {
	descriptor_data *d;
	char_data *vict;

	half_chop(argument, arg, buf2);

	if (REAL_NPC(ch))
		msg_to_char(ch, "Monsters can't page... go away.\r\n");
	else if (!*arg)
		msg_to_char(ch, "Whom do you wish to page?\r\n");
	else {
		sprintf(buf, "\007*$n* %s", buf2);
		if (!str_cmp(arg, "all")) {
			if (GET_ACCESS_LEVEL(ch) >= LVL_IMPL) {
				for (d = descriptor_list; d; d = d->next) {
					if (STATE(d) == CON_PLAYING && d->character) {
						act(buf, FALSE, ch, 0, d->character, TO_VICT);
					}
				}
			}
			else {
				msg_to_char(ch, "You will never be godly enough to do that!\r\n");
			}
			return;
		}
		if ((vict = get_char_vis(ch, arg, FIND_CHAR_WORLD)) != NULL) {
			if (is_ignoring(vict, ch)) {
				act("$E is ignoring you.", FALSE, ch, NULL, vict, TO_CHAR | TO_SLEEP);
			}
			else {
				act(buf, FALSE, ch, 0, vict, TO_VICT);
				if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
					send_config_msg(ch, "ok_string");
				}
				else {
					act(buf, FALSE, ch, 0, vict, TO_CHAR);
				}
			}
		}
		else {
			msg_to_char(ch, "There is no such person in the game!\r\n");
		}
	}
}


ACMD(do_recolor) {
	extern const char *custom_color_types[];
	
	char *valid_colors = "rgbymcwajloptvnRGBYMCWAJLOPTV0";
	
	char arg[MAX_INPUT_LENGTH];
	int iter, type;
	
	argument = any_one_arg(argument, arg);
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Usage: recolor <type> <color code>\r\n");
		msg_to_char(ch, "Your custom colors are set to:\r\n");
		for (iter = 0; *custom_color_types[iter] != '\n' && iter < NUM_CUSTOM_COLORS; ++iter) {
			if (GET_CUSTOM_COLOR(ch, iter)) {
				msg_to_char(ch, " %s: \t&%c\r\n", custom_color_types[iter], GET_CUSTOM_COLOR(ch, iter));
			}
			else {
				msg_to_char(ch, " %s: not set\r\n", custom_color_types[iter]);
			}
		}
	}
	else if (!*argument) {
		msg_to_char(ch, "Usage: recolor <type> <color code>\r\n");
	}
	else if ((type = search_block(arg, custom_color_types, FALSE)) == NOTHING) {
		msg_to_char(ch, "Invalid choice '%s'.\r\n", arg);
	}
	else if (!str_cmp(argument, "none") || !str_cmp(argument, "off")) {
		GET_CUSTOM_COLOR(ch, type) = 0;	// none
		msg_to_char(ch, "You no longer have a custom %s color.\r\n", custom_color_types[type]);
	}
	else if (strlen(argument) != 2 || *argument != '&' || !strchr(valid_colors, argument[1])) {
		msg_to_char(ch, "You must specify a single color code (for example, \t&r).\r\n");
	}
	else {
		GET_CUSTOM_COLOR(ch, type) = argument[1];	// store just the color code
		msg_to_char(ch, "Your %s color is now \t%c\t&%c\tn.\r\n", custom_color_types[type], GET_CUSTOM_COLOR(ch, type), GET_CUSTOM_COLOR(ch, type));
	}
}


ACMD(do_reply) {
	char_data *tch;

	if (REAL_NPC(ch))
		return;

	skip_spaces(&argument);

	if (GET_LAST_TELL(ch) == NOBODY)
		msg_to_char(ch, "You have no-one to reply to!\r\n");
	else if (!*argument)
		msg_to_char(ch, "What is your reply?\r\n");
	else {
		/*
		 * Make sure the person you're replying to is still playing by searching
		 * for them.  Note, now last tell is stored as player IDnum instead of
		 * a pointer, which is much better because it's safer, plus will still
		 * work if someone logs out and back in again.
		 */

		/*
		 * XXX: A descriptor list based search would be faster although
		 *      we could not find link dead people.  Not that they can
		 *      hear tells anyway. :) -gg 2/24/98
		 */
		tch = character_list;
		while (tch != NULL && (REAL_NPC(tch) || GET_IDNUM(tch) != GET_LAST_TELL(ch))) {
			tch = tch->next;
		}

		if (tch == NULL) {
			msg_to_char(ch, "They are no longer playing.\r\n");
		}
		else if (is_tell_ok(ch, tch)) {
			perform_tell(ch, tch, argument);
		}
	}
}


ACMD(do_say) {
	char_data *c;
	char lbuf[MAX_STRING_LENGTH], string[MAX_STRING_LENGTH], buf[MAX_STRING_LENGTH];
	int ctype = (subcmd == SCMD_OOCSAY ? CUSTOM_COLOR_OOCSAY : CUSTOM_COLOR_SAY);
	char color;
	
	skip_spaces(&argument);
	
	if (!*argument)
		msg_to_char(ch, "Yes, but WHAT do you want to say?\r\n");
	else if (subcmd != SCMD_OOCSAY && ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_SILENT))
		msg_to_char(ch, "You speak, but no words come out!\r\n");
	else {
		if (subcmd == SCMD_OOCSAY) {
			strcpy(buf1, " out of character,");
		}
		else {
			*buf1 = '\0';
		}
		
		// this leaves in a "%c" used for a color code
		sprintf(string, "$n says,%s '%s\t%%c'\tn", buf1, double_percents(argument));
		
		for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
			if (REAL_NPC(c) || ch == c || is_ignoring(c, ch))
				continue;
			
			// for channel history
			if (c->desc) {
				clear_last_act_message(c->desc);
			}
			
			color = (!IS_NPC(c) && GET_CUSTOM_COLOR(c, ctype)) ? GET_CUSTOM_COLOR(c, ctype) : '0';
			msg_to_char(c, "\t%c", color);
			
			sprintf(buf, string, color);
			act(buf, FALSE, ch, 0, c, TO_VICT | DG_NO_TRIG);
			
			// channel history
			if (c->desc && c->desc->last_act_message) {
				// the message was sent via act(), we can retrieve it from the desc
				sprintf(lbuf, "\t%c%s", color, c->desc->last_act_message);
				add_to_channel_history(c, CHANNEL_HISTORY_SAY, lbuf);
			}
		}
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT))
			send_config_msg(ch, "ok_string");
		else {
			delete_doubledollar(argument);
			color = (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, ctype)) ? GET_CUSTOM_COLOR(ch, ctype) : '0';
			sprintf(lbuf, "\t%cYou say,%s '%s\t%c'\tn\r\n", color, buf1, argument, color);
			send_to_char(lbuf, ch);

			if (ch->desc) {
				add_to_channel_history(ch, CHANNEL_HISTORY_SAY, lbuf);
			}
		}
		
		/* trigger check */
		if (subcmd != SCMD_OOCSAY) {
			speech_mtrigger(ch, argument);
			speech_wtrigger(ch, argument);
			speech_vtrigger(ch, argument);
		}
	}
}


ACMD(do_spec_comm) {
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
	char_data *vict;
	const char *action_sing, *action_plur, *action_others;
	char color;

	switch (subcmd) {
		case SCMD_WHISPER: {
			action_sing = "whisper to";
			action_plur = "whispers to";
			action_others = "$n whispers something to $N.";
			break;
		}
		case SCMD_ASK: {
			action_sing = "ask";
			action_plur = "asks";
			action_others = "$n asks $N a question.";
			break;
		}
		default: {
			action_sing = "oops";
			action_plur = "oopses";
			action_others = "$n is tongue-tied trying to speak with $N.";
			break;
		}
	}

	half_chop(argument, buf, buf2);

	if (!IS_APPROVED(ch) && !IS_IMMORTAL(ch) && config_get_bool("chat_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!*buf || !*buf2) {
		sprintf(buf, "Whom do you want to %s... and what??\r\n", action_sing);
		msg_to_char(ch, buf);
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_SILENT))
		msg_to_char(ch, "You speak, but no words come out!\r\n");
	else if (!(vict = get_char_vis(ch, buf, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (vict == ch)
		msg_to_char(ch, "You can't get your mouth close enough to your ear...\r\n");
	else if (is_ignoring(vict, ch)) {
		act("$E is ignoring you.", FALSE, ch, NULL, vict, TO_CHAR | TO_SLEEP);
	}
	else {
		// msg to victim
		if (vict->desc) {
			clear_last_act_message(vict->desc);
		}
		color = (!IS_NPC(vict) && GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_SAY)) ? GET_CUSTOM_COLOR(vict, CUSTOM_COLOR_SAY) : '0';
		sprintf(buf, "$n %s you, '%s\t%c'\t0", action_plur, buf2, color);
		if (color != '0') {
			msg_to_char(vict, "\t%c", color);
		}
		act(buf, FALSE, ch, NULL, vict, TO_VICT);
		// channel history
		if (vict->desc && vict->desc->last_act_message) {
			// the message was sent via act(), we can retrieve it from the desc
			sprintf(buf, "\t%c%s", color, vict->desc->last_act_message);
			add_to_channel_history(vict, CHANNEL_HISTORY_SAY, buf);
		}
		
		// msg to char
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			color = (!IS_NPC(ch) && GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_SAY)) ? GET_CUSTOM_COLOR(ch, CUSTOM_COLOR_SAY) : '0';
			sprintf(buf, "\t%cYou %s %s, '%s\t%c'\t0", color, action_sing, PERS(vict, ch, FALSE), buf2, color);
			msg_to_char(ch, "%s\r\n", buf);
			if (ch->desc) {
				add_to_channel_history(ch, CHANNEL_HISTORY_SAY, buf);
			}
		}

		act(action_others, FALSE, ch, NULL, vict, TO_NOTVICT);
	}
}


ACMD(do_tell) {
	char_data *vict = NULL;
	char target[MAX_INPUT_LENGTH], string[MAX_INPUT_LENGTH];

	if (REAL_NPC(ch))
		return;

	half_chop(argument, target, string);

	if (!*target || !*string) {
		msg_to_char(ch, "Who do you wish to tell what??\r\n");
	}
	else if (!(vict = get_player_vis(ch, target, FIND_CHAR_WORLD | FIND_NO_DARK))) {
		send_config_msg(ch, "no_person");
	}
	else if (is_tell_ok(ch, vict)) {
		perform_tell(ch, vict, string);
	}
}
