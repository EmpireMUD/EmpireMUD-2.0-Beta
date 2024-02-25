/* ************************************************************************
*   File: friends.c                                       EmpireMUD 2.0b5 *
*  Usage: Functions and commands related to the friends list              *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#include "constants.h"

/**
* Contents:
*   Data
*   Commands
*/

// local prototypes
bool remove_account_friend(account_data *acct, int acct_id);


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

/**
* Finds the friendship status between one character and another.
*
* @param char_data *ch One character (may be an NPC).
* @param char_data *vict Another character (may be an NPC).
* @return int A FRIEND_ const: FRIEND_NONE if they are not friends (including if either is an NPC), FRIEND_FRIENDSHIP if they are friends, and another status if they're somewhere in the middle.
*/
int account_friend_status(char_data *ch, char_data *vict) {
	struct friend_data *friend;
	
	if (!ch || !vict) {
		return FRIEND_NONE;
	}
	else if (IS_NPC(ch) || IS_NPC(vict)) {
		return FRIEND_NONE;
	}
	else if (!GET_ACCOUNT(ch) || !GET_ACCOUNT(vict)) {
		return FRIEND_NONE;
	}
	else if (!(friend = find_account_friend(GET_ACCOUNT(ch), GET_ACCOUNT_ID(vict)))) {
		return FRIEND_NONE;
	}
	else {
		return friend->status;
	}
}


/**
* This adds or updates a friend. It does not save the account file; you should
* save it yourself if this return TRUE, unless you're calling this during
* account loading.
*
* @param account_data *acct The account to add/update the friend to.
* @param int acct_id The account id of the friend.
* @param int status the FRIEND_ status to update to, or NOTHING to not update it and keep the current status (or FRIEND_NONE).
* @param char *name The name of the friend's character, to update on the account info.
* @return bool TRUE if there were any changes; FALSE if not.
*/
bool add_account_friend_id(account_data *acct, int acct_id, int status, char *name) {
	bool change = FALSE;
	struct friend_data *friend;
	
	if (!acct) {
		return change;
	}
	
	// find or create
	if (!(friend = find_account_friend(acct, acct_id))) {
		CREATE(friend, struct friend_data, 1);
		friend->account_id = acct_id;
		friend->original_name = (name && *name) ? strdup(name) : NULL;
		HASH_ADD_INT(acct->friends, account_id, friend);
		change = TRUE;
	}
	
	// update status
	if (status != NOTHING && friend->status != status) {
		friend->status = status;
		change = TRUE;
	}
	
	// update name
	if (name && *name && (!friend->name || strcmp(name, friend->name))) {
		if (friend->name) {
			free(friend->name);
		}
		friend->name = strdup(name);
		change = TRUE;
	}
	
	return change;
}


/**
* Updates last-seen name on the friends' list when a player logs in.
*
* @param char_data *ch The player to update.
*/
void check_friends(char_data *ch) {
	bool any = FALSE;
	char_data *plr;
	struct friend_data *friend, *next_friend;
	
	if (!ch || IS_NPC(ch) || !GET_ACCOUNT(ch) || !GET_ACCOUNT_FRIENDS(ch)) {
		return;	// no work
	}
	
	// update online names
	DL_FOREACH2(player_character_list, plr, next_plr) {
		if (plr == ch || !GET_ACCOUNT(plr)) {
			continue;	// self or invalid player
		}
		if (PRF_FLAGGED(plr, PRF_NO_FRIENDS)) {
			continue;	// no-friends toggle
		}
		if (GET_INVIS_LEV(plr) > GET_ACCESS_LEVEL(ch)) {
			continue;	// can't see
		}
		if (account_friend_status(ch, plr) != FRIEND_FRIENDSHIP) {
			continue;	// not friends;
		}
		
		// ok? update name
		any |= add_account_friend_id(GET_ACCOUNT(ch), GET_ACCOUNT_ID(plr), NOTHING, GET_NAME(plr));
	}
	
	// clear out stale friends?
	HASH_ITER(hh, GET_ACCOUNT_FRIENDS(ch), friend, next_friend) {
		if (friend->status == FRIEND_NONE || !find_account(friend->account_id)) {
			// gone baby
			any |= remove_account_friend(GET_ACCOUNT(ch), friend->account_id);
		}
	}
	
	if (any) {
		SAVE_ACCOUNT(GET_ACCOUNT(ch));
	}
}


/**
* Finds an entry for a friend already on the account.
*
* @param account_data *acct The account to check.
* @param int acct_id The friend to look for.
* @return struct friend_data* The friend entry, if it exists, or NULL if not.
*/
struct friend_data *find_account_friend(account_data *acct, int acct_id) {
	struct friend_data *friend = NULL;
	if (acct) {
		HASH_FIND_INT(acct->friends, &acct_id, friend);
	}
	return friend;	// if any
}


/**
* Looks up a friend by the name that was stored in the friends list. This can
* find one when the last name you saw them by no longer exists.
*
* @param char_data *ch The person looking for their friend.
* @param char *name The name as-typed.
* @param bool *is_file A variable to store whether or not this character was loaded from file.
* @param bool *is_secret A variable to store whether or not we had to use a disallowed character (hide the name).
* @return char_data* The found friend, if any (may be an offline player).
*/
char_data *find_friend_player_by_stored_name(char_data *ch, char *name, bool *is_file, bool *is_secret) {
	bool loaded_file = FALSE;
	account_data *acct;
	char_data *loaded, *last_resort = NULL;
	player_index_data *index, *next_index;
	struct friend_data *friend, *next_friend, *found = NULL, *partial = NULL;
	
	if (is_file) {
		*is_file = FALSE;
	}
	if (is_secret) {
		*is_secret = FALSE;
	}
	
	if (!ch || !GET_ACCOUNT(ch) || !*name) {
		return NULL;	// no work
	}
	
	// find best match for name
	HASH_ITER(hh, GET_ACCOUNT_FRIENDS(ch), friend, next_friend) {
		if (!friend->name) {
			continue;
		}
		else if (!str_cmp(name, friend->name)) {
			// exact
			found = friend;
			break;
		}
		else if (is_abbrev(name, friend->name) && !partial) {
			partial = friend;
			// but keep looking for exact match
		}
	}
	
	// did we find anything
	if (!found) {
		found = partial;
	}
	if (!found) {
		return NULL;	// no matches
	}
	if (!(acct = find_account(found->account_id))) {
		// whoops, deleted? clean up now
		if (remove_account_friend(GET_ACCOUNT(ch), found->account_id)) {
			SAVE_ACCOUNT(GET_ACCOUNT(ch));
		}
		return NULL;
	}
	
	// now find a valid character on the account

	HASH_ITER(name_hh, player_table_by_name, index, next_index) {
		if (index->account_id != found->account_id) {
			continue;	// wrong account
		}
		if (!(loaded = find_or_load_player(index->name, &loaded_file))) {
			continue;
		}
		
		// ok we have a character-- validate them for this
		if (PRF_FLAGGED(loaded, PRF_NO_FRIENDS)) {
			last_resort = loaded;
			continue;	// no-friends toggle
		}
		if (!loaded_file && GET_INVIS_LEV(loaded) > GET_ACCESS_LEVEL(ch)) {
			last_resort = loaded;
			continue;	// can't see
		}
		
		// this seems like a good one:
		if (is_file) {
			*is_file = loaded_file;
		}
		return loaded;
		
		// loaded players will be cleaned up automatically later (may have been saved as last_resort)
	}
	
	// if we got here and there are no valid characters, we should clean up this entry
	if (!last_resort) {
		if (remove_account_friend(GET_ACCOUNT(ch), found->account_id)) {
			SAVE_ACCOUNT(GET_ACCOUNT(ch));
		}
		return NULL;
	}
	
	// otherwise, only return our last resort
	if (is_secret) {
		*is_secret = TRUE;
	}
	return last_resort;
}


/**
* Finds which character a friend is online as. This will never find a person
* with no-friends toggled on, or one who's invisible. If more than one
* character is in-game, it returns the first that meets all the conditions
* (i.e. an invisible immortal is skipped in favor of their visible alt).
*
* @param char_data *for_char Optional: Whose friend it is (limits visibility).
* @param int acct_id Which account to look for.
* @return char_data* The found friend, if any, or NULL if not online.
*/
char_data *find_online_friend(char_data *for_char, int acct_id) {
	char_data *ch_iter;
	DL_FOREACH2(player_character_list, ch_iter, next_plr) {
		if (GET_ACCOUNT_ID(ch_iter) != acct_id) {
			continue;	// wrong account
		}
		if (PRF_FLAGGED(ch_iter, PRF_NO_FRIENDS)) {
			continue;	// not allowed
		}
		if (for_char && GET_INVIS_LEV(ch_iter) > GET_ACCESS_LEVEL(for_char)) {
			continue;	// wiz invis
		}
		if (for_char && !CAN_SEE_NO_DARK(for_char, ch_iter)) {
			continue;	// cannot see them
		}
		
		// ok
		return ch_iter;
	}
	// else:
	return NULL;
}


/**
* Frees a hash of account friends.
*
* @param struct friend_data **hash A pointer to the hash to free.
*/
void free_account_friends(struct friend_data **hash) {
	struct friend_data *friend, *next;
	if (hash) {
		HASH_ITER(hh, *hash, friend, next) {
			HASH_DEL(*hash, friend);
			if (friend->name) {
				free(friend->name);
			}
			if (friend->original_name) {
				free(friend->original_name);
			}
			free(friend);
		}
	}
}


/**
* Determines if a player has any friend requests waiting.
*
* @param char_data *ch The player.
* @return int How many friend requests are waiting for the player (may be 0).
*/
int has_pending_friend_requests(char_data *ch) {
	int count = 0;
	struct friend_data *friend, *next_friend;
	
	HASH_ITER(hh, GET_ACCOUNT_FRIENDS(ch), friend, next_friend) {
		if (friend->status == FRIEND_REQUEST_RECEIVED) {
			++count;
		}
	}
	
	return count;	// if any
}


/**
* Mortlogs but only to the person's friends list.
*
* @param char_data *ch The person whose friends will see this.
* @param const char *str... The string and arguments.
*/
void mortlog_friends(char_data *ch, const char *str, ...) {
	char output[MAX_STRING_LENGTH];
	descriptor_data *desc;
	va_list tArgList;

	if (!str || !ch || IS_NPC(ch) || PRF_FLAGGED(ch, PRF_NO_FRIENDS)) {
		return;
	}

	va_start(tArgList, str);
	vsprintf(output, str, tArgList);
	
	LL_FOREACH(descriptor_list, desc) {
		if (STATE(desc) != CON_PLAYING || !desc->character) {
			continue;	// invalid or no mortlog
		}
		if (PRF_FLAGGED(desc->character, PRF_NO_FRIENDS)) {
			continue;	// no friends on this character
		}
		if (GET_INVIS_LEV(ch) > GET_ACCESS_LEVEL(desc->character)) {
			continue;	// can't see
		}
		if (account_friend_status(ch, desc->character) != FRIEND_FRIENDSHIP) {
			continue;	// not valid friends
		}
		
		// update their last-seen name
		if (add_account_friend_id(GET_ACCOUNT(desc->character), GET_ACCOUNT_ID(ch), NOTHING, GET_NAME(ch))) {
			SAVE_ACCOUNT(GET_ACCOUNT(desc->character));
		}
		
		// AFTER updating name: see if we should suppress the message
		if (!SHOW_STATUS_MESSAGES(desc->character, SM_MORTLOG)) {
			continue;	// does not want to see the mortlog
		}
		if (GET_LOYALTY(ch) == GET_LOYALTY(desc->character) && SHOW_STATUS_MESSAGES(desc->character, SM_EMPIRE_LOGS)) {
			continue;	// same empire and should see empire logs
		}
		
		// ok
		msg_to_desc(desc, "&c[ %s ]&0\r\n", output);
	}
	va_end(tArgList);
}


/**
* Removes a friend from the account. This does not save the account itself;
* you should save it if this function returns TRUE.
*
* @param account_data *acct The account who is losing a friend.
* @param int acct_id The account id of the friend being lost.
* @return bool TRUE if a friend was removed; FALSE if there was no matching friend.
*/
bool remove_account_friend(account_data *acct, int acct_id) {
	struct friend_data *friend;
	if (acct && (friend = find_account_friend(acct, acct_id))) {
		HASH_DEL(acct->friends, friend);
		if (friend->name) {
			free(friend->name);
		}
		if (friend->original_name) {
			free(friend->original_name);
		}
		free(friend);
		return TRUE;
	}
	// else:
	return FALSE;
}


// quick sorter for friends
int sort_friends_by_last_logon(struct friend_data *a, struct friend_data *b) {
	account_data *a_acc = find_account(a->account_id);
	account_data *b_acc = find_account(b->account_id);
	if (!a_acc || !b_acc) {
		return 1;
	}
	else {
		// dividing by seconds per day helps keep from using list order to detect secret no-friends alts
		return ((time(0) - a_acc->last_logon) / SECS_PER_REAL_HOUR) - ((time(0) - b_acc->last_logon) / SECS_PER_REAL_HOUR);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

// do_friend validates player, account, and descriptor before alls sub-commands

// called through do_friend
ACMD(do_friends_all) {
	bool need_save = FALSE;
	char color[16], name[256], status[256];
	int count;
	size_t nsize, ssize;
	account_data *acct;
	char_data *plr;
	struct friend_data *friend, *next_friend;
	
	HASH_SORT(GET_ACCOUNT(ch)->friends, sort_friends_by_last_logon);
	
	build_page_display(ch, "Your friends list:");
	count = 0;
	
	HASH_ITER(hh, GET_ACCOUNT_FRIENDS(ch), friend, next_friend) {
		if (friend->status == FRIEND_NONE) {
			continue;	// not my friend
		}
		if (!(acct = find_account(friend->account_id))) {
			// no such account-- clean up now
			need_save |= remove_account_friend(GET_ACCOUNT(ch), friend->account_id);
			continue;
		}
		
		// otherwise show it...
		*color = '\0';
		plr = (friend->status == FRIEND_FRIENDSHIP ? find_online_friend(ch, friend->account_id) : NULL);
		if (plr) {
			nsize = snprintf(name, sizeof(name), "%s", GET_NAME(plr));
		}
		else {
			nsize = snprintf(name, sizeof(name), "%s", (friend->name ? friend->name : "Unknown"));
		}
		
		if (friend->original_name && strcmp(name, friend->original_name)) {
			nsize += snprintf(name + nsize, sizeof(name) - nsize, " (%s)", friend->original_name);
		}
		
		ssize = snprintf(status, sizeof(status), " - %s", friend_status_types[friend->status]);
		if (friend->status == FRIEND_FRIENDSHIP) {
			if (plr) {
				ssize += snprintf(status + ssize, sizeof(status) - ssize, " (online)");
				strcpy(color, "\tc");
			}
			else if (time(0) - acct->last_logon < SECS_PER_REAL_HOUR) {
				ssize += snprintf(status + ssize, sizeof(status) - ssize, " (online recently)");
			}
			else if (time(0) - acct->last_logon < 24 * SECS_PER_REAL_HOUR) {
				ssize += snprintf(status + ssize, sizeof(status) - ssize, " (less than 1 day ago)");
			}
			else {
				ssize += snprintf(status + ssize, sizeof(status) - ssize, " (%s ago)", simple_time_since(acct->last_logon));
			}
		}
		
		// actual line
		build_page_display(ch, " %s%s%s%s", color, name, status, *color ? "\t0" : "");
		++count;
	}
	
	if (!count) {
		build_page_display(ch, " none");
	}
	
	send_page_display(ch);
	
	if (need_save) {
		// a friend was removed due to cleanup
		SAVE_ACCOUNT(GET_ACCOUNT(ch));
	}
}


// called through do_friend
ACMD(do_friends_online) {
	int count, status;
	char_data *plr;
	struct friend_data *friend;
	struct page_display *pd;
	
	build_page_display(ch, "Friends online:");
	count = 0;
	
	DL_FOREACH2(player_character_list, plr, next_plr) {
		status = account_friend_status(ch, plr);
		
		if (status != FRIEND_FRIENDSHIP) {
			continue;	// not friends
		}
		if (GET_INVIS_LEV(plr) > GET_ACCESS_LEVEL(ch)) {
			continue;	// invisible immortal
		}
		if (PRF_FLAGGED(plr, PRF_NO_FRIENDS)) {
			continue;	// unidentified alt
		}
		
		// seems ok to show
		++count;
		pd = build_page_display(ch, " %s", GET_NAME(plr));
		
		if ((friend = find_account_friend(GET_ACCOUNT(ch), GET_ACCOUNT_ID(plr))) && friend->original_name && strcmp(GET_NAME(plr), friend->original_name)) {
			append_page_display_line(pd, " (%s)", friend->original_name);
		}
	}
	
	if (!count) {
	    build_page_display(ch, " none");
	}
	
	send_page_display(ch);
}


// called through do_friend
ACMD(do_friend_accept) {
	bool file = FALSE, secret = FALSE;
	int count, status;
	char_data *plr = NULL;
	struct friend_data *friend, *next_friend;
	
	if (PRF_FLAGGED(ch, PRF_NO_FRIENDS)) {
		msg_to_char(ch, "You cannot accept or deny friend requests while you have no-friends toggled on.\r\n");
	}
	else if (!*argument) {
		// no-arg: show open requests
		build_page_display(ch, "Open friend requests you've received:");
		count = 0;
		
		HASH_ITER(hh, GET_ACCOUNT_FRIENDS(ch), friend, next_friend) {
			if (friend->status != FRIEND_REQUEST_RECEIVED) {
				continue;	// wrong status
			}
			
			// otherwise show it
			build_page_display(ch, " %s", friend->original_name ? friend->original_name : "Unknown");
			++count;
		}
		
		if (!count) {
			build_page_display(ch, " none");
		}
		
		send_page_display(ch);
		return;
	}
	else if (!(plr = find_or_load_player(argument, &file)) && !(plr = find_friend_player_by_stored_name(ch, argument, &file, &secret))) {
		msg_to_char(ch, "No such player.\r\n");
	}
	else if ((status = account_friend_status(ch, plr)) != FRIEND_REQUEST_RECEIVED) {
		msg_to_char(ch, "You do not have an open friend request from %s.\r\n", secret ? "them" : GET_NAME(plr));
	}
	else if (PRF_FLAGGED(plr, PRF_NO_FRIENDS)) {
		if ((friend = find_account_friend(GET_ACCOUNT(ch), GET_ACCOUNT_ID(plr))) && friend->original_name && !str_cmp(GET_NAME(plr), friend->original_name)) {
			// only if it's the same alt who sent the offer
			msg_to_char(ch, "That player has toggled no-friends on since sending this offer; you cannot accept it.\r\n");
			
			// cancel it
			if (remove_account_friend(GET_ACCOUNT(ch), GET_ACCOUNT_ID(plr))) {
				SAVE_ACCOUNT(GET_ACCOUNT(ch));
			}
			if (remove_account_friend(GET_ACCOUNT(plr), GET_ACCOUNT_ID(ch))) {
				SAVE_ACCOUNT(GET_ACCOUNT(plr));
			}
		}
		else {
			// lie (they may be friends but we are not allowed to know this alt)
			msg_to_char(ch, "You do not have an open friend request from %s.\r\n", secret ? "them" : GET_NAME(plr));
		}
	}
	else {
		// looks ok
		msg_to_char(ch, "You accept the friend request from %s!\r\n", secret ? "that player" : GET_NAME(plr));
		if (!file) {
			msg_to_char(plr, "%s has accepted your friend request!\r\n", GET_NAME(ch));
		}
		
		// accept my side
		if (add_account_friend_id(GET_ACCOUNT(ch), GET_ACCOUNT_ID(plr), FRIEND_FRIENDSHIP, secret ? NULL : GET_NAME(plr))) {
			SAVE_ACCOUNT(GET_ACCOUNT(ch));
		}
		
		// accpt their side
		if (add_account_friend_id(GET_ACCOUNT(plr), GET_ACCOUNT_ID(ch), FRIEND_FRIENDSHIP, GET_NAME(ch))) {
			SAVE_ACCOUNT(GET_ACCOUNT(plr));
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


// called through do_friend
ACMD(do_friend_cancel) {
	bool file = FALSE, secret = FALSE;
	int status;
	char_data *plr = NULL;
	
	if (PRF_FLAGGED(ch, PRF_NO_FRIENDS)) {
		msg_to_char(ch, "You cannot send or cancel friend requests while you have no-friends toggled on.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Cancel your friend request to whom?\r\n");
	}
	else if (!(plr = find_or_load_player(argument, &file)) && !(plr = find_friend_player_by_stored_name(ch, argument, &file, &secret))) {
		msg_to_char(ch, "No such player.\r\n");
	}
	else if (PRF_FLAGGED(plr, PRF_NO_FRIENDS) || !GET_ACCOUNT(plr)) {
		// lie (they may be friends but we are not allowed to know this alt)
		msg_to_char(ch, "You do not have a friend request open to %s.\r\n", secret ? "them" : GET_NAME(plr));
	}
	else if ((status = account_friend_status(ch, plr)) != FRIEND_REQUEST_SENT) {
		msg_to_char(ch, "You do not have a friend request open to %s.\r\n", secret ? "them" : GET_NAME(plr));
	}
	else {
		// looks ok
		msg_to_char(ch, "You cancel the friend request to %s.\r\n", secret ? "that player" : GET_NAME(plr));
		if (!file) {
			msg_to_char(plr, "%s cancels the friend request.\r\n", GET_NAME(ch));
		}
		
		// cancel my side
		if (remove_account_friend(GET_ACCOUNT(ch), GET_ACCOUNT_ID(plr))) {
			SAVE_ACCOUNT(GET_ACCOUNT(ch));
		}
		
		// cancel their side
		if (remove_account_friend(GET_ACCOUNT(plr), GET_ACCOUNT_ID(ch))) {
			SAVE_ACCOUNT(GET_ACCOUNT(plr));
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


// called through do_friend
ACMD(do_friend_deny) {
	bool file = FALSE, secret = FALSE;
	int count, status;
	char_data *plr = NULL;
	struct friend_data *friend, *next_friend;
	
	if (PRF_FLAGGED(ch, PRF_NO_FRIENDS)) {
		msg_to_char(ch, "You cannot accept or deny friend requests while you have no-friends toggled on.\r\n");
	}
	else if (!*argument) {
		// no-arg: show open requests
		build_page_display(ch, "Open friend requests you've received:");
		count = 0;
		
		HASH_ITER(hh, GET_ACCOUNT_FRIENDS(ch), friend, next_friend) {
			if (friend->status != FRIEND_REQUEST_RECEIVED) {
				continue;	// wrong status
			}
			
			// otherwise show it
			build_page_display(ch, " %s", friend->original_name ? friend->original_name : "Unknown");
			++count;
		}
		
		if (!count) {
			build_page_display(ch, " none");
		}
		
		send_page_display(ch);
		return;
	}
	else if (!(plr = find_or_load_player(argument, &file)) && !(plr = find_friend_player_by_stored_name(ch, argument, &file, &secret))) {
		msg_to_char(ch, "No such player.\r\n");
	}
	else if (PRF_FLAGGED(plr, PRF_NO_FRIENDS) || !GET_ACCOUNT(plr)) {
		// lie (they may be friends but we are not allowed to know this alt)
		msg_to_char(ch, "You do not have an open friend request from %s.\r\n", secret ? "them" : GET_NAME(plr));
	}
	else if ((status = account_friend_status(ch, plr)) != FRIEND_REQUEST_RECEIVED) {
		msg_to_char(ch, "You do not have an open friend request from %s.\r\n", secret ? "them" : GET_NAME(plr));
	}
	else {
		// looks ok
		msg_to_char(ch, "You deny the friend request from %s.\r\n", secret ? "that player" : GET_NAME(plr));
		if (!file) {
			msg_to_char(plr, "%s has denied your friend request.\r\n", GET_NAME(ch));
		}
		
		// cancel my side
		if (remove_account_friend(GET_ACCOUNT(ch), GET_ACCOUNT_ID(plr))) {
			SAVE_ACCOUNT(GET_ACCOUNT(ch));
		}
		
		// cancel their side
		if (remove_account_friend(GET_ACCOUNT(plr), GET_ACCOUNT_ID(ch))) {
			SAVE_ACCOUNT(GET_ACCOUNT(plr));
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


// called through do_friend
ACMD(do_friend_request) {
	int count, status;
	char_data *plr;
	struct friend_data *friend, *next_friend;
	
	if (!*argument) {
		// no-arg: show open requests
		build_page_display(ch, "Open friend requests you've sent:");
		count = 0;
		
		HASH_ITER(hh, GET_ACCOUNT_FRIENDS(ch), friend, next_friend) {
			if (friend->status != FRIEND_REQUEST_SENT) {
				continue;	// wrong status
			}
			
			// otherwise show it
			build_page_display(ch, " %s", friend->original_name ? friend->original_name : "Unknown");
			++count;
		}
		
		if (!count) {
			build_page_display(ch, " none");
		}
		
		send_page_display(ch);
		return;
	}	// end no-arg
	
	// otherwise, try to request someone:
	if (PRF_FLAGGED(ch, PRF_NO_FRIENDS)) {
		msg_to_char(ch, "You cannot send out friend requests while you have no-friends toggled on.\r\n");
	}
	else if (!(plr = get_player_vis(ch, argument, FIND_CHAR_WORLD | FIND_NO_DARK))) {
		send_config_msg(ch, "no_person");
	}
	else if (plr == ch || GET_ACCOUNT(plr) == GET_ACCOUNT(ch)) {
		msg_to_char(ch, "You cannot send yourself a friend request.\r\n");
	}
	else if (is_ignoring(plr, ch) || !GET_ACCOUNT(plr)) {
		msg_to_char(ch, "You cannot send %s a friend request.\r\n", GET_NAME(plr));
	}
	else if ((status = account_friend_status(ch, plr)) == FRIEND_FRIENDSHIP) {
		// already friends, but...
		if (PRF_FLAGGED(plr, PRF_NO_FRIENDS)) {
			msg_to_char(ch, "You send %s a friend request.\r\n", GET_NAME(plr));
			msg_to_char(plr, "%s has sent you a friend request. You are already friends but you have no-friends toggled on.\r\n", GET_NAME(ch));
		}
		else {
			msg_to_char(ch, "You are already friends with %s.\r\n", GET_NAME(plr));
		}
	}
	else if (status == FRIEND_REQUEST_SENT) {
		// already sent -- but don't mention that, in case it's an alt
		msg_to_char(ch, "You send %s a friend request.\r\n", GET_NAME(plr));
		
		// update the name on the request; do not spam the target
		if (add_account_friend_id(GET_ACCOUNT(plr), GET_ACCOUNT_ID(ch), FRIEND_REQUEST_RECEIVED, GET_NAME(ch))) {
			SAVE_ACCOUNT(GET_ACCOUNT(plr));
		}
	}
	else if (status == FRIEND_REQUEST_RECEIVED) {
		// wrong command
		do_friend_accept(ch, argument, 0, 0);
	}
	else {
		// all other statusses: send a request
		msg_to_char(ch, "You send %s a friend request.\r\n", GET_NAME(plr));
		
		if (!PRF_FLAGGED(plr, PRF_NO_FRIENDS)) {
			msg_to_char(plr, "%s has sent you a friend request. Use 'friend accept %s' or 'friend deny %s'.\r\n", GET_NAME(ch), GET_NAME(ch), GET_NAME(ch));
		
			if (add_account_friend_id(GET_ACCOUNT(ch), GET_ACCOUNT_ID(plr), FRIEND_REQUEST_SENT, GET_NAME(plr))) {
				SAVE_ACCOUNT(GET_ACCOUNT(ch));
			}
		
			if (add_account_friend_id(GET_ACCOUNT(plr), GET_ACCOUNT_ID(ch), FRIEND_REQUEST_RECEIVED, GET_NAME(ch))) {
				SAVE_ACCOUNT(GET_ACCOUNT(plr));
			}
		}
		else {
			// auto-deny
			msg_to_char(plr, "%s tried to send you a friend request but you have no-friends toggled on.\r\n", GET_NAME(ch));
		}
	}
}


// called through do_friend
ACMD(do_friend_status) {
	bool file = FALSE;
	int status;
	char_data *plr = NULL;
	struct friend_data *friend;
	
	if (!*argument) {
		// argument is actually guaranteed, but in case
		msg_to_char(ch, "Check friend status on whom?\r\n");
	}
	else if (!(plr = find_or_load_player(argument, &file))) {
		msg_to_char(ch, "No such player.\r\n");
	}
	else if (PRF_FLAGGED(plr, PRF_NO_FRIENDS) || !GET_ACCOUNT(plr)) {
		// lie (they may be friends but we are not allowed to know this alt)
		msg_to_char(ch, "You are not friends with %s.\r\n", GET_NAME(plr));
	}
	else {
		// FRIEND_x:
		status = account_friend_status(ch, plr);
		friend = find_account_friend(GET_ACCOUNT(ch), GET_ACCOUNT_ID(plr));
		
		// ensure they're really friends on this alt
		if (status != FRIEND_FRIENDSHIP && friend) {
			if (strcmp(friend->original_name, GET_NAME(plr))) {
				// offered to someone else?
				status = FRIEND_NONE;
			}
		}
		
		switch (status) {
			case FRIEND_FRIENDSHIP: {
				if (strcmp(friend->original_name, GET_NAME(plr))) {
					msg_to_char(ch, "You are friends with %s (%s).\r\n", GET_NAME(plr), friend->original_name);
				}
				else {
					msg_to_char(ch, "You are friends with %s.\r\n", GET_NAME(plr));
				}
				break;
			}
			case FRIEND_REQUEST_SENT: {
				msg_to_char(ch, "Your friend request to %s is pending.\r\n", GET_NAME(plr));
				break;
			}
			case FRIEND_REQUEST_RECEIVED: {
				msg_to_char(ch, "%s has sent you a friend request. Use 'friend accept %s' or 'friend deny %s'.\r\n", GET_NAME(plr), GET_NAME(plr), GET_NAME(plr));
				break;
			}
			case FRIEND_NONE:
			default: {
				msg_to_char(ch, "You are not friends with %s.\r\n", GET_NAME(plr));
				break;
			}
		}
		
		// and:
		if (PRF_FLAGGED(ch, PRF_NO_FRIENDS)) {
			msg_to_char(ch, "(You have toggled no-friends on for this character.)\r\n");
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


ACMD(do_friend) {
	char arg2[MAX_INPUT_LENGTH];
	
	two_arguments(argument, arg, arg2);
	
	if (IS_NPC(ch) || !GET_ACCOUNT(ch)) {
		msg_to_char(ch, "You don't have any friends.\r\n");
	}
	else if (!ch->desc) {
		// nothing to show if no desc
	}
	else if (!*arg || !str_cmp(arg, "list")) {
		do_friends_online(ch, "", 0, 0);
	}
	else if (!str_cmp(arg, "all") || is_abbrev(arg, "-all")) {
		do_friends_all(ch, "", 0, 0);
	}
	else if ((is_abbrev(arg, "approve") || is_abbrev(arg, "accept")) && strlen(arg) >= 3) {
		do_friend_accept(ch, arg2, 0, 0);
	}
	else if (!str_cmp(arg, "cancel")) {
		do_friend_cancel(ch, arg2, 0, 0);
	}
	else if (!str_cmp(arg, "deny") || !str_cmp(arg, "decline")) {
		do_friend_deny(ch, arg2, 0, 0);
	}
	else if ((is_abbrev(arg, "request") || is_abbrev(arg, "add")) && strlen(arg) >= 3) {
		do_friend_request(ch, arg2, 0, 0);
	}
	else {
		do_friend_status(ch, arg, 0, 0);
	}
}


ACMD(do_unfriend) {
	bool file = FALSE, secret = FALSE;
	int status;
	char_data *plr = NULL;
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch) || !GET_ACCOUNT(ch)) {
		msg_to_char(ch, "You don't have any friends.\r\n");
	}
	else if (PRF_FLAGGED(ch, PRF_NO_FRIENDS)) {
		msg_to_char(ch, "You cannot unfriend anyone while you have the no-friends toggle on.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Unfriend whom?\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file)) && !(plr = find_friend_player_by_stored_name(ch, arg, &file, &secret))) {
		msg_to_char(ch, "No such player.\r\n");
	}
	else if (PRF_FLAGGED(plr, PRF_NO_FRIENDS) || !GET_ACCOUNT(plr)) {
		// lie (they may be friends but we are not allowed to know this alt)
		msg_to_char(ch, "You are not friends with %s.\r\n", secret ? "that player" : GET_NAME(plr));
	}
	else if ((status = account_friend_status(ch, plr)) != FRIEND_FRIENDSHIP) {
		msg_to_char(ch, "You are not friends with %s.\r\n", secret ? "that player" : GET_NAME(plr));
	}
	else {
		// looks ok
		msg_to_char(ch, "You unfriend %s.\r\n", secret ? "that player" : GET_NAME(plr));
		
		// cancel my side
		if (remove_account_friend(GET_ACCOUNT(ch), GET_ACCOUNT_ID(plr))) {
			SAVE_ACCOUNT(GET_ACCOUNT(ch));
		}
		
		// cancel their side
		if (remove_account_friend(GET_ACCOUNT(plr), GET_ACCOUNT_ID(ch))) {
			SAVE_ACCOUNT(GET_ACCOUNT(plr));
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}
