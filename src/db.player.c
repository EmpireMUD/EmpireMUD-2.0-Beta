/* ************************************************************************
*   File: db.player.c                                     EmpireMUD 2.0b5 *
*  Usage: Database functions related to players and the player table      *
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
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "skills.h"
#include "interpreter.h"
#include "dg_event.h"
#include "dg_scripts.h"
#include "vnums.h"
#include "constants.h"

/**
* Contents:
*   Getters
*   Account DB
*   Core Player DB
*   Delayed Update System
*   Lastname Handlers
*   loaded_player_hash For Offline Players
*   Autowiz Wizlist Generator
*   Helpers
*   Languages
*   Map Memory
*   Playtime Tracking
*   Empire Member/Greatness Tracking
*   Empire Player Management
*   Equipment Sets
*   Promo Codes
*/

#define LOG_BAD_TAG_WARNINGS  TRUE	// triggers syslogs for invalid pfile tags

// external vars
extern const char *anonymous_public_hosts[];
extern const char *default_channels[];
extern int top_account_id;

// external funcs
ACMD(do_slash_channel);
void add_archetype_lore(char_data *ch);
EVENT_CANCEL_FUNC(cancel_affect_expire_event);
void check_minipets_and_companions(char_data *ch);
void check_player_events(char_data *ch);
void clean_lore(char_data *ch);
void clean_player_kills(char_data *ch);
void clear_delayed_empire_refresh(empire_data *only_emp, bitvector_t refresh_flag);
bool should_delete_empire(empire_data *emp);

// local protos
void check_eq_sets(char_data *ch);
void clear_player(char_data *ch);
time_t get_member_timeout_ch(char_data *ch);
time_t get_member_timeout_time(time_t created, time_t last_login, double played_hours);
void purge_bound_items(int idnum);
char_data *read_player_from_file(FILE *fl, char *name, bool normal, char_data *ch);
void remove_loaded_player(char_data *ch);
int sort_accounts(account_data *a, account_data *b);
int sort_players_by_idnum(player_index_data *a, player_index_data *b);
int sort_players_by_name(player_index_data *a, player_index_data *b);
void track_empire_playtime(empire_data *emp, int add_seconds);
void update_played_time(char_data *ch);
void write_map_memory(char_data *ch);
void write_player_delayed_data_to_file(FILE *fl, char_data *ch);
void write_player_primary_data_to_file(FILE *fl, char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// GETTERS /////////////////////////////////////////////////////////////////

/**
* Look up a player's index entry by idnum.
*
* @param int idnum The idnum to look up.
* @return player_index_data* The player, if any (or NULL).
*/
player_index_data *find_player_index_by_idnum(int idnum) {
	player_index_data *plr;
	HASH_FIND(idnum_hh, player_table_by_idnum, &idnum, sizeof(int), plr);
	return plr;
}


/**
* Look up a player's index entry by name.
*
* @param char *name The name to look up.
* @return player_index_data* The player, if any (or NULL).
*/
player_index_data *find_player_index_by_name(char *name) {
	char dupe[MAX_INPUT_LENGTH];	// should be long enough
	player_index_data *plr;
	
	strcpy(dupe, name);
	strtolower(dupe);
	HASH_FIND(name_hh, player_table_by_name, dupe, strlen(dupe), plr);
	return plr;
}


/**
* @param room_data *room The room to search in.
* @param int id A player id.
* @return char_data* if the player is in the room, or NULL otherwise.
*/
char_data *find_player_in_room_by_id(room_data *room, int id) {
	char_data *ch;
	
	DL_FOREACH2(ROOM_PEOPLE(room), ch, next_in_room) {
		if (!IS_NPC(ch) && GET_IDNUM(ch) == id && !EXTRACTED(ch)) {
			return ch;
		}
	}
	return NULL;
}


/**
* Checks if a user is in the anonymous host list -- the list of sites that
* are public and hide a player's real IP address. This is used to prevent
* cheating via unlinked alts or multiplaying.
*
* You can disable this by turning off the 'restrict_anonymous_hosts' config.
*
* @param descriptor_data *desc The user.
* @return bool TRUE if the player is from an anonymous public host, FALSE if not.
*/
bool has_anonymous_host(descriptor_data *desc) {
	int iter;
	
	if (!config_get_bool("restrict_anonymous_hosts")) {
		return FALSE;	// don't bother
	}
	
	for (iter = 0; *anonymous_public_hosts[iter] != '\n'; ++iter) {
		if (!str_cmp(desc->host, anonymous_public_hosts[iter])) {
			return TRUE;	// FOUND!
		}
	}
	
	return FALSE;	// not found
}


/**
* Finds a character who is sitting at a menu, for various functions that update
* all players and check which are in-game vs not. If a person is at a menu,
* then to safely update them you should change both their live data and saved
* data.
*
* @param int id A player id.
* @return char_data* A character in a menu state, or NULL if none.
*/
char_data *is_at_menu(int id) {
	descriptor_data *desc;
	
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (STATE(desc) != CON_PLAYING && desc->character && !IS_NPC(desc->character) && GET_IDNUM(desc->character) == id) {
			return desc->character;
		}
	}
	
	return NULL;
}


/**
* @param int id A player id
* @return char_data* if the player is in the game, or NULL otherwise
*/
char_data *is_playing(int id) {
	char_data *ch;
	
	DL_FOREACH2(player_character_list, ch, next_plr) {
		if (GET_IDNUM(ch) == id && !EXTRACTED(ch)) {
			return ch;
		}
	}
	return NULL;
}


 //////////////////////////////////////////////////////////////////////////////
//// ACCOUNT DB //////////////////////////////////////////////////////////////

/**
* Add an account to the hash table.
*
* @param account_data *acct The account to add.
*/
void add_account_to_table(account_data *acct) {
	account_data *find;
	int id;
	
	if (acct) {
		id = acct->id;
		HASH_FIND_INT(account_table, &id, find);
		if (!find) {
			HASH_ADD_INT(account_table, id, acct);
			HASH_SORT(account_table, sort_accounts);
		}
	}
}


/**
* Attaches a player to an account. This can't be called until a player has
* an idnum and has been added to the player index.
*
* @param char_data *ch The player to add.
* @param account_data *acct The account to add them to.
*/
void add_player_to_account(char_data *ch, account_data *acct) {
	struct account_player *plr, *pos;
	player_index_data *index;
	bool found;
	
	if (!ch || IS_NPC(ch) || !acct) {
		log("SYSERR: add_player_to_account called without %s", acct ? "player" : "account");
		return;
	}
	
	if (!(index = find_player_index_by_idnum(GET_IDNUM(ch)))) {
		log("SYSERR: add_player_to_account called on player not in index");
		return;
	}
	
	CREATE(plr, struct account_player, 1);
	plr->name = str_dup(GET_PC_NAME(ch));
	strtolower(plr->name);
	
	// see if the player's name is already in the account list (sometimes caused by disconnects during creation)
	found = FALSE;
	for (pos = acct->players; pos; pos = pos->next) {
		if (!str_cmp(pos->name, plr->name)) {
			found = TRUE;
			pos->player = index;
			
			// free our temporary one
			free(plr->name);
			free(plr);
			
			plr = pos;
			break;
		}
	}
	
	if (!found) {
		// add to end of account player list
		LL_APPEND(acct->players, plr);
	}
	
	// update this at the end, after we're sure we've found it
	plr->player = index;
	
	save_library_file_for_vnum(DB_BOOT_ACCT, acct->id);
	GET_ACCOUNT(ch) = acct;
}


/**
* Creates a new account and adds a player to it. The account is added to the
* account_table and saved.
*
* @param char_data *ch The player to make a new account for.
* @return account_data* A pointer to the new account.
*/
account_data *create_account_for_player(char_data *ch) {
	account_data *acct;
	
	if (!ch || IS_NPC(ch)) {
		log("SYSERR: create_account_for_player called without player");
		return NULL;
	}
	if (GET_ACCOUNT(ch)) {
		log("SYSERR: create_account_for_player called for player with account");
		return GET_ACCOUNT(ch);
	}
	
	CREATE(acct, account_data, 1);
	acct->id = ++top_account_id;
	acct->last_logon = ch->player.time.logon;
	
	add_account_to_table(acct);
	add_player_to_account(ch, acct);
	
	save_index(DB_BOOT_ACCT);
	save_library_file_for_vnum(DB_BOOT_ACCT, acct->id);
	
	return acct;
}


/**
* @param int id The account to look up.
* @return account_data* The account from account_table, if any (or NULL).
*/
account_data *find_account(int id) {
	account_data *acct;	
	HASH_FIND_INT(account_table, &id, acct);
	return acct;
}


/**
* Frees the memory for an account.
*
* @param account_data *acct An account to free.
*/
void free_account(account_data *acct) {
	struct account_player *plr;
	descriptor_data *desc;
	struct pk_data *pkd;
	
	if (!acct) {
		return;
	}
	
	// ensure no players editing the notes
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (desc->str == &(acct->notes)) {
			desc->str = NULL;
			desc->notes_id = 0;
			msg_to_desc(desc, "The account you were editing notes for has been deleted.\r\n");
		}
	}
	
	if (acct->notes) {
		free(acct->notes);
	}
	
	// free players list
	while ((plr = acct->players)) {
		if (plr->name) {
			free(plr->name);
		}
		acct->players = plr->next;
		free(plr);
	}
	
	// free pk data
	while ((pkd = acct->killed_by)) {
		acct->killed_by = pkd->next;
		free(pkd);
	}
	
	// free unlocks
	free_account_friends(&acct->friends);
	free_unlocked_archetypes(acct);
	
	free(acct);
}


/**
* Reads in one account from a file and adds it to the table.
*
* @param FILE *fl The file to read from.
* @param int nr The id of the account.
*/
void parse_account(FILE *fl, int nr) {
	char err_buf[MAX_STRING_LENGTH], line[256], str_in[256], str_in2[256];
	struct account_player *plr;
	account_data *acct, *find;
	struct pk_data *pk;
	struct unlocked_archetype *unarch;
	int int_in[3];
	long l_in;
	
	// create
	CREATE(acct, account_data, 1);
	acct->id = nr;
	
	HASH_FIND_INT(account_table, &nr, find);
	if (find) {
		log("WARNING: Duplicate account id #%d", nr);
		// but have to load it anyway to advance the file
	}
	add_account_to_table(acct);
	
	snprintf(err_buf, sizeof(err_buf), "account #%d", nr);
	
	// line 1: last login, flags
	if (get_line(fl, line) && sscanf(line, "%ld %s", &l_in, str_in) == 2) {
		acct->last_logon = l_in;
		acct->flags = asciiflag_conv(str_in);
	}
	else {
		log("SYSERR: Format error in line 1 of %s", err_buf);
		exit(1);
	}
	
	// line 2+: notes
	acct->notes = fread_string(fl, err_buf);
	
	// alphabetic flag section
	snprintf(err_buf, sizeof(err_buf), "account #%d, in alphabetic flags", nr);
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s", err_buf);
			exit(1);
		}
		switch (*line) {
			case 'D': { // data
				switch (*(line+1)) {
					case '0': {	// D0: unlocked archetypes
						if ((sscanf(line, "D0 %d", &int_in[0])) != 1) {
							log("SYSERR: Format error in D0 data section of %s", err_buf);
							exit(1);
						}
						
						// add if not present
						HASH_FIND_INT(acct->unlocked_archetypes, &int_in[0], unarch);
						if (!unarch) {
							CREATE(unarch, struct unlocked_archetype, 1);
							unarch->vnum = int_in[0];
							HASH_ADD_INT(acct->unlocked_archetypes, vnum, unarch);
						}
						break;
					}
					default: {
						log("SYSERR: Format error in D%c data section of %s", *(line+1), err_buf);
						exit(1);
					}
				}
				break;
			}
			case 'F': {	// friend
				if (sscanf(line, "F%d %d %s %s", &int_in[0], &int_in[1], str_in, str_in2) != 4) {
					log("SYSERR: Format error in F line of %s", err_buf);
					exit(1);
				}
				
				// runs twice: import original name, import current name
				if (strcmp(str_in, "*")) {
					add_account_friend_id(acct, int_in[1], int_in[0], str_in);
				}
				add_account_friend_id(acct, int_in[1], int_in[0], str_in2);
				break;
			}
			case 'K': {	// killed by
				if (sscanf(line, "K %d %d %d %ld", &int_in[0], &int_in[1], &int_in[2], &l_in) != 4) {
					log("SYSERR: Format error in K section of %s", err_buf);
					exit(1);
				}
				
				CREATE(pk, struct pk_data, 1);
				pk->killed_alt = int_in[0];
				pk->player_id = int_in[1];
				pk->empire = int_in[2];
				pk->last_time = l_in;
				
				// order doesn't matter right?
				LL_PREPEND(acct->killed_by, pk);
				break;
			}
			case 'P': {	// player
				if (sscanf(line, "P %s", str_in) == 1) {
					CREATE(plr, struct account_player, 1);
					plr->name = str_dup(str_in);
					strtolower(plr->name);	// ensure lowercase
					
					// add to end
					LL_APPEND(acct->players, plr);
				}
				else {
					log("SYSERR: Format error in P section of %s", err_buf);
					exit(1);
				}
				break;
			}
			case 'S': {	// end
				return;
			}

			default: {
				log("SYSERR: Format error in %s", err_buf);
				exit(1);
			}
		}
	}
}


/**
* Removes an account from the hash table.
*
* @param account_data *acct The account to remove from the table.
*/
void remove_account_from_table(account_data *acct) {
	HASH_DEL(account_table, acct);
}


/**
* Removes a player from their existing account and deletes it if there are no
* more players on the account.
*
* @param char_data *ch The player to remove from its account.
*/
void remove_player_from_account(char_data *ch) {
	struct account_player *plr, *next_plr;
	player_index_data *index;
	account_data *acct;
	bool has_players;
	
	// sanity checks
	if (!ch || !(acct = GET_ACCOUNT(ch))) {
		return;
	}
	
	// find index entry
	index = find_player_index_by_idnum(GET_IDNUM(ch));
	
	// remove player from list
	has_players = FALSE;
	for (plr = acct->players; plr && index; plr = next_plr) {
		next_plr = plr->next;
		
		if (plr->player == index || (plr->name && !str_cmp(plr->name, index->name))) {
			// remove
			if (plr->name) {
				free(plr->name);
			}
			LL_DELETE(acct->players, plr);
			free(plr);
		}
		else {
			has_players = TRUE;
		}
	}
	
	GET_ACCOUNT(ch) = NULL;
	
	if (!has_players) {
		remove_account_from_table(acct);
		save_index(DB_BOOT_ACCT);
	}

	// save either way
	save_library_file_for_vnum(DB_BOOT_ACCT, acct->id);
	
	if (!has_players) {
		free_account(acct);
	}
}


/**
* @param account_data *a One element
* @param account_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_accounts(account_data *a, account_data *b) {
	return a->id - b->id;
}


/**
* Writes the account index to file.
*
* @param FILE *fl The open index file.
*/
void write_account_index(FILE *fl) {
	account_data *acct, *next_acct;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, account_table, acct, next_acct) {
		// determine "zone number" by vnum
		this = (int)(acct->id / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, ACCT_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one account in the db file format, starting with a #ID and ending
* in an S.
*
* @param FILE *fl The file to write it to.
* @param account_data *acct The account to save.
*/
void write_account_to_file(FILE *fl, account_data *acct) {
	char temp[MAX_STRING_LENGTH];
	struct account_player *plr;
	struct friend_data *friend, *next_friend;
	struct pk_data *pk;
	struct unlocked_archetype *unarch, *next_unarch;
	
	if (!fl || !acct) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_account_to_file called without %s", !fl ? "file" : "account");
		return;
	}
	
	fprintf(fl, "#%d\n", acct->id);
	fprintf(fl, "%ld %s\n", acct->last_logon, bitv_to_alpha(acct->flags));

	strcpy(temp, NULLSAFE(acct->notes));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// D: data
	// D0: unlocked archetypes
	HASH_ITER(hh, acct->unlocked_archetypes, unarch, next_unarch) {
		fprintf(fl, "D0 %d\n", unarch->vnum);
	}
	
	// F: friends
	HASH_ITER(hh, acct->friends, friend, next_friend) {
		fprintf(fl, "F%d %d %s %s\n", friend->status, friend->account_id, (friend->original_name ? friend->original_name : "*"), (friend->name ? friend->name : "Unknown"));
	}
	
	// K: player kills
	LL_FOREACH(acct->killed_by, pk) {
		fprintf(fl, "K %d %d %d %ld\n", pk->killed_alt, pk->player_id, pk->empire, pk->last_time);
	}
	
	// P: player
	for (plr = acct->players; plr; plr = plr->next) {
		if (plr->player || plr->name) {
			fprintf(fl, "P %s\n", plr->player ? plr->player->name : plr->name);
		}
	}
	
	// END
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// CORE PLAYER DB //////////////////////////////////////////////////////////

/**
* Adds a player to the player tables (by_name and by_idnum), and sorts both
* tables.
*
* @param player_index_data *plr The player to add.
* @return bool TRUE if it succeeded or FALSE if it failed because the name or ID is in-use.
*/
bool add_player_to_table(player_index_data *plr) {
	bool bad_id = FALSE, bad_name = FALSE;
	player_index_data *find;
	int idnum = plr->idnum;
	
	// by idnum
	find = NULL;
	HASH_FIND(idnum_hh, player_table_by_idnum, &idnum, sizeof(int), find);
	if (!find) {
		HASH_ADD(idnum_hh, player_table_by_idnum, idnum, sizeof(int), plr);
		HASH_SRT(idnum_hh, player_table_by_idnum, sort_players_by_idnum);
	}
	else {
		bad_id = TRUE;
	}
	
	// by name: ensure name is lowercase
	find = NULL;
	strtolower(plr->name);	// ensure always lower
	HASH_FIND(name_hh, player_table_by_name, plr->name, strlen(plr->name), find);
	if (!find) {
		HASH_ADD(name_hh, player_table_by_name, name[0], strlen(plr->name), plr);
		HASH_SRT(name_hh, player_table_by_name, sort_players_by_name);
	}
	else {
		bad_name = TRUE;
	}
	
	if (bad_id || bad_name) {
		log("SYSERR: add_player_to_table: '%s' (%d) is already in %s", plr->name, plr->idnum, (bad_id && bad_name) ? "both tables" : (bad_id ? "id table" : "name table"));
	}
	
	// return FALSE only if both id and name failed
	return !(bad_id && bad_name);
}


/**
* Creates the player index by loading all players from the accounts. This must
* be run after accounts are loaded, but before the mud boots up.
*
* This also determines:
*   top_idnum / DATA_TOP_IDNUM
*   top_account_id
*/
void build_player_index(void) {
	struct account_player *plr, *next_plr;
	account_data *acct, *next_acct;
	player_index_data *index;
	bool has_players;
	char_data *ch;
	
	int top_idnum = data_get_int(DATA_TOP_IDNUM);
	
	HASH_ITER(hh, account_table, acct, next_acct) {
		acct->last_logon = 0;	// reset
		
		// update top account id
		top_account_id = MAX(top_account_id, acct->id);
		
		has_players = FALSE;
		for (plr = acct->players; plr; plr = next_plr) {
			next_plr = plr->next;
			
			if (!plr->player) {
				// load the character
				ch = NULL;
				if (plr->name && *plr->name) {
					ch = load_player(plr->name, FALSE);
				}
				
				// could not load character for this entry
				if (!ch) {
					log("SYSERR: Unable to index account player '%s'", plr->name ? plr->name : "???");
					LL_DELETE(acct->players, plr);
					if (plr->name) {
						free(plr->name);
					}
					free(plr);
					save_library_file_for_vnum(DB_BOOT_ACCT, acct->id);
					continue;
				}
				
				GET_ACCOUNT(ch) = acct;	// not set by load_player

				CREATE(index, player_index_data, 1);
				update_player_index(index, ch);
				
				// ensure it can add to the index or else back out
				if (!add_player_to_table(index)) {
					free_player_index_data(index);
					log("SYSERR: Deleting account's player entry for '%s' because it's already on another account", plr->name ? plr->name : "???");
					LL_DELETE(acct->players, plr);
					if (plr->name) {
						free(plr->name);
					}
					free(plr);
					save_library_file_for_vnum(DB_BOOT_ACCT, acct->id);
					free_char(ch);
					continue;
				}
				
				// otherwise store the index entry
				plr->player = index;
				
				// detect top idnum
				top_idnum = MAX(top_idnum, GET_IDNUM(ch));
				
				// unload character
				free_char(ch);
				
				// set this for later
				has_players = TRUE;
			}
			
			// update last logon
			acct->last_logon = MAX(acct->last_logon, plr->player->last_logon);
		}
		
		// failed to load any players -- delete it
		if (!has_players) {
			remove_account_from_table(acct);
			save_index(DB_BOOT_ACCT);
			save_library_file_for_vnum(DB_BOOT_ACCT, acct->id);
			free_account(acct);
		}
	}
	
	// update this
	data_set_int(DATA_TOP_IDNUM, top_idnum);
}


/**
* Loads the rest of a player's data from the delayed-load file, if they have
* an open file. If they don't have one, it just returns.
*
* @param char_data *ch The player to finish loading.
*/
void check_delayed_load(char_data *ch) {
	char filename[256];
	FILE *fl;
	
	// no work
	if (!ch || IS_NPC(ch) || !NEEDS_DELAYED_LOAD(ch)) {
		return;
	}
	
	// reset this no matter what
	NEEDS_DELAYED_LOAD(ch) = FALSE;
	
	if (!get_filename(GET_PC_NAME(ch), filename, DELAYED_FILE)) {
		log("SYSERR: check_delayed_load: Unable to get delayed filename for '%s'", GET_PC_NAME(ch));
		return;
	}
	if (!(fl = fopen(filename, "r"))) {
		// non-fatal: delay file does not exist
		return;
	}
	
	ch = read_player_from_file(fl, GET_PC_NAME(ch), FALSE, ch);
	fclose(fl);	
	
	update_reputations(ch);
	
	// definitely safe to save delay file
	DONT_SAVE_DELAY(ch) = FALSE;
}


/* release memory allocated for a char struct */
void free_char(char_data *ch) {
	struct player_automessage *automsg, *next_automsg;
	struct slash_channel *loadslash, *next_loadslash;
	struct player_ability_data *abil, *next_abil;
	struct player_skill_data *skill, *next_skill;
	struct companion_data *compan, *next_compan;
	struct player_faction_data *pfd, *next_pfd;
	struct ability_gain_hook *hook, *next_hook;
	struct mount_data *mount, *next_mount;
	struct channel_history_data *history, *next_hist;
	struct empire_unique_storage *eus, *next_eus;
	struct player_slash_channel *slash;
	struct player_craft_data *pcd, *next_pcd;
	struct player_currency *cur, *next_cur;
	struct player_language *lang, *next_lang;
	struct minipet_data *mini, *next_mini;
	struct player_lastname *lastn;
	struct player_eq_set *eq_set;
	struct pursuit_data *purs;
	struct player_tech *ptech;
	struct offer_data *offer;
	struct lore_data *lore;
	struct coin_data *coin;
	struct mail_data *mail;
	struct alias_data *a;
	char_data *proto;
	obj_data *obj;
	int iter;
	
	pause_affect_total = TRUE;

	// in case somehow?
	if (GROUP(ch)) {
		leave_group(ch);
	}
	
	// clean up gear/items, if any
	extract_all_items(ch);
	
	// ensure they are not queued for re-freeing
	if (!IS_NPC(ch)) {
		remove_loaded_player(ch);
	}

	if (ch->followers || GET_LEADER(ch)) {
		die_follower(ch);
	}
	
	if (IS_NPC(ch)) {
		free_mob_tags(&MOB_TAGGED_BY(ch));
		while ((purs = MOB_PURSUIT(ch))) {
			MOB_PURSUIT(ch) = purs->next;
			free(purs);
		}
	}
	
	// remove all affects
	while (ch->affected) {
		affect_remove(ch, ch->affected);
	}
	while (ch->over_time_effects) {
		dot_remove(ch, ch->over_time_effects);
	}
	
	// remove cooldowns
	while (ch->cooldowns) {
		remove_cooldown(ch, ch->cooldowns);
	}

	// free any assigned scripts and vars
	if (SCRIPT(ch)) {
		extract_script(ch, MOB_TRIGGER);
	}
	
	// free lore
	while ((lore = GET_LORE(ch))) {
		GET_LORE(ch) = lore->next;
		if (lore->text) {
			free(lore->text);
		}
		free(lore);
	}
	
	// alert empire data the mob is despawned
	if (GET_EMPIRE_NPC_DATA(ch)) {
		GET_EMPIRE_NPC_DATA(ch)->mob = NULL;
		GET_EMPIRE_NPC_DATA(ch) = NULL;
	}
	
	// extract objs: aren't these handled by extract_all_items
	while ((obj = ch->carrying)) {
		extract_obj(obj);
	}
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter)) {
			extract_obj(GET_EQ(ch, iter));
		}
	}

	// This is really just players, but I suppose a mob COULD have it ...
	if (ch->player_specials != NULL && ch->player_specials != &dummy_mob) {		
		if (GET_CURRENT_LASTNAME(ch)) {
			free(GET_CURRENT_LASTNAME(ch));
		}
		if (GET_PERSONAL_LASTNAME(ch)) {
			free(GET_PERSONAL_LASTNAME(ch));
		}
		if (GET_TITLE(ch)) {
			free(GET_TITLE(ch));
		}
		if (GET_PROMPT(ch)) {
			free(GET_PROMPT(ch));
		}
		if (GET_FIGHT_PROMPT(ch)) {
			free(GET_FIGHT_PROMPT(ch));
		}
		if (POOFIN(ch)) {
			free(POOFIN(ch));
		}
		if (POOFOUT(ch)) {
			free(POOFOUT(ch));
		}
		if (GET_CREATION_HOST(ch)) {
			free(GET_CREATION_HOST(ch));
		}
		if (GET_REFERRED_BY(ch)) {
			free(GET_REFERRED_BY(ch));
		}
		if (GET_DISGUISED_NAME(ch)) {
			free(GET_DISGUISED_NAME(ch));
		}
		if (GET_ACTION_STRING(ch)) {
			free(GET_ACTION_STRING(ch));
		}
		
		free_resource_list(GET_ACTION_RESOURCES(ch));
		
		HASH_ITER(hh, GET_AUTOMESSAGES(ch), automsg, next_automsg) {
			HASH_DEL(GET_AUTOMESSAGES(ch), automsg);
			free(automsg);
		}
		
		for (loadslash = LOAD_SLASH_CHANNELS(ch); loadslash; loadslash = next_loadslash) {
			next_loadslash = loadslash->next;
			if (loadslash->name) {
				free(loadslash->name);
			}
			if (loadslash->lc_name) {
				free(loadslash->lc_name);
			}
			free(loadslash);
		}
		
		// free channel histories
		for (iter = 0; iter < NUM_CHANNEL_HISTORY_TYPES; ++iter) {
			DL_FOREACH_SAFE(GET_HISTORY(ch, iter), history, next_hist) {
				DL_DELETE(GET_HISTORY(ch, iter), history);
				
				if (history->message) {
					free(history->message);
				}
				free(history);
			}
		}
		
		while ((a = GET_ALIASES(ch)) != NULL) {
			GET_ALIASES(ch) = (GET_ALIASES(ch))->next;
			free_alias(a);
		}
		
		HASH_ITER(hh, GET_FACTIONS(ch), pfd, next_pfd) {
			HASH_DEL(GET_FACTIONS(ch), pfd);
			free(pfd);
		}
		
		while ((lastn = GET_LASTNAME_LIST(ch))) {
			GET_LASTNAME_LIST(ch) = lastn->next;
			if (lastn->name) {
				free(lastn->name);
			}
			free(lastn);
		}
		
		while ((offer = GET_OFFERS(ch))) {
			GET_OFFERS(ch) = offer->next;
			free(offer);
		}
		
		while ((slash = GET_SLASH_CHANNELS(ch))) {
			GET_SLASH_CHANNELS(ch) = slash->next;
			free(slash);
		}
		
		while ((coin = GET_PLAYER_COINS(ch))) {
			GET_PLAYER_COINS(ch) = coin->next;
			free(coin);
		}
		
		HASH_ITER(hh, GET_COMPANIONS(ch), compan, next_compan) {
			HASH_DEL(GET_COMPANIONS(ch), compan);
			free_companion(compan);
		}
		
		while ((mail = GET_MAIL_PENDING(ch))) {
			GET_MAIL_PENDING(ch) = mail->next;
			free_mail(mail);
		}
		
		while (GET_PASSIVE_BUFFS(ch)) {
			remove_passive_buff(ch, GET_PASSIVE_BUFFS(ch));
		}
		
		HASH_ITER(hh, GET_SKILL_HASH(ch), skill, next_skill) {
			HASH_DEL(GET_SKILL_HASH(ch), skill);
			free(skill);
		}
		HASH_ITER(hh, GET_ABILITY_HASH(ch), abil, next_abil) {
			HASH_DEL(GET_ABILITY_HASH(ch), abil);
			free(abil);
		}
		HASH_ITER(hh, GET_ABILITY_GAIN_HOOKS(ch), hook, next_hook) {
			HASH_DEL(GET_ABILITY_GAIN_HOOKS(ch), hook);
			free(hook);
		}
		HASH_ITER(hh, GET_CURRENCIES(ch), cur, next_cur) {
			HASH_DEL(GET_CURRENCIES(ch), cur);
			free(cur);
		}
		HASH_ITER(hh, GET_LANGUAGES(ch), lang, next_lang) {
			HASH_DEL(GET_LANGUAGES(ch), lang);
			free(lang);
		}
		HASH_ITER(hh, GET_LEARNED_CRAFTS(ch), pcd, next_pcd) {
			HASH_DEL(GET_LEARNED_CRAFTS(ch), pcd);
			free(pcd);
		}
		HASH_ITER(hh, GET_MINIPETS(ch), mini, next_mini) {
			HASH_DEL(GET_MINIPETS(ch), mini);
			free(mini);
		}
		HASH_ITER(hh, GET_MOUNT_LIST(ch), mount, next_mount) {
			HASH_DEL(GET_MOUNT_LIST(ch), mount);
			free(mount);
		}
		free_player_event_data(GET_EVENT_DATA(ch));
		free_trait_hooks(&GET_ABILITY_TRAIT_HOOKS(ch));
		
		while ((eq_set = GET_EQ_SETS(ch))) {
			GET_EQ_SETS(ch) = eq_set->next;
			free_player_eq_set(eq_set);
		}
		
		while (GET_MAP_MEMORY(ch)) {
			delete_player_map_memory(GET_MAP_MEMORY(ch), ch);
		}
		
		while ((ptech = GET_TECHS(ch))) {
			GET_TECHS(ch) = ptech->next;
			free(ptech);
		}
		
		// free unique storage
		DL_FOREACH_SAFE(GET_HOME_STORAGE(ch), eus, next_eus) {
			DL_DELETE(GET_HOME_STORAGE(ch), eus);
			
			if (eus->obj) {
				add_to_object_list(eus->obj);
				extract_obj(eus->obj);
			}
			free(eus);
		}
		
		free_player_completed_quests(&GET_COMPLETED_QUESTS(ch));
		free_player_quests(GET_QUESTS(ch));
		
		free(ch->player_specials);
		if (IS_NPC(ch)) {
			log("SYSERR: Mob %s (#%d) had player_specials allocated!", GET_NAME(ch), GET_MOB_VNUM(ch));
		}
	}
	
	// mob prototype checks
	proto = IS_NPC(ch) ? mob_proto(GET_MOB_VNUM(ch)) : NULL;
	
	if (ch->prev_host && (!proto || ch->prev_host != proto->prev_host)) {
		free(ch->prev_host);
	}
	if (ch->player.name && (!proto || ch->player.name != proto->player.name)) {
		free(ch->player.name);
	}
	if (ch->player.passwd && (!proto || ch->player.passwd != proto->player.passwd)) {
		free(ch->player.passwd);
	}
	if (ch->player.short_descr && (!proto || ch->player.short_descr != proto->player.short_descr)) {
		free(ch->player.short_descr);
	}
	if (ch->player.long_descr && (!proto || ch->player.long_descr != proto->player.long_descr)) {
		free(ch->player.long_descr);
	}
	if (ch->player.look_descr && (!proto || ch->player.look_descr != proto->player.look_descr)) {
		free(ch->player.look_descr);
	}
	if (ch->proto_script && (!proto || ch->proto_script != proto->proto_script)) {
		free_proto_scripts(&ch->proto_script);
	}
	if (ch->interactions && (!proto || ch->interactions != proto->interactions)) {
		free_interactions(&ch->interactions);
	}
	if (MOB_CUSTOM_MSGS(ch) && (!proto || MOB_CUSTOM_MSGS(ch) != MOB_CUSTOM_MSGS(proto))) {
		free_custom_messages(MOB_CUSTOM_MSGS(ch));
	}
	
	if (ch->desc) {
		ch->desc->character = NULL;
	}
	
	// clear any pending updates
	cancel_all_stored_events(&GET_STORED_EVENTS(ch));
	clear_delayed_update(ch);

	/* find_char helper */
	if (ch->in_lookup_table) {
		remove_from_lookup_table(ch->script_id);
	}

	free(ch);
	pause_affect_total = FALSE;
}


/**
* Frees the memory from a player index.
*
* @param player_index_data *index The index to free.
*/
void free_player_index_data(player_index_data *index) {
	if (index->name) {
		free(index->name);
	}
	if (index->fullname) {
		free(index->fullname);
	}
	if (index->last_host) {
		free(index->last_host);
	}
	
	free(index);
}


/**
* Loads a character from file. This creates a character but does not add them
* to any lists or perform any checks. It also does not load:
* - no aliases
* - no equipment or inventory
* - no script variables
*
* Note on the 'normal' parameter: If FALSE, it won't try to attach the player
* to an account or index. This is done during initial startup ONLY.
*
* @param char_data *name The player's login name.
* @param bool normal Pass TRUE for this.
* @return char_data* The loaded player, or NULL if none exists.
*/
char_data *load_player(char *name, bool normal) {
	char filename[256];
	char_data *ch;
	FILE *fl;
	
	if (!get_filename(name, filename, PLR_FILE)) {
		log("SYSERR: load_player: Unable to get player filename for '%s'", name);
		return NULL;
	}
	if (!(fl = fopen(filename, "r"))) {
		// no character file exists
		return NULL;
	}
	
	ch = read_player_from_file(fl, name, normal, NULL);
	fclose(fl);
	
	// mark that they are partially-loaded
	NEEDS_DELAYED_LOAD(ch) = TRUE;
	
	return ch;
}


/**
* Parse the data from a player file -- this is used on both the normal player
* file and the delayed player file, as the same data could appear in either.
*
* @param FILE *fl The open file, for reading.
* @param char *name The name of the player we are trying to load (for error messages).
* @param bool normal Always pass TRUE except during startup and delayed data loads (won't link index/account if FALSE).
* @param char_data *ch Optional: If loading delayed data, pass in the existing character. Otherwise, NULL.
* @return char_data* The loaded character.
*/
char_data *read_player_from_file(FILE *fl, char *name, bool normal, char_data *ch) {
	char line[MAX_INPUT_LENGTH], error[MAX_STRING_LENGTH], str_in[MAX_INPUT_LENGTH], *read, *tmp;
	int account_id = NOTHING, ignore_pos = 0, junk;
	struct lore_data *lore, *last_lore = NULL, *new_lore;
	struct over_time_effect_type *dot, *last_dot = NULL;
	struct affected_type *af, *next_af, *af_list = NULL;
	struct empire_unique_storage *eus;
	struct player_quest *plrq, *last_plrq = NULL;
	struct offer_data *offer, *last_offer = NULL;
	struct alias_data *alias, *last_alias = NULL;
	struct companion_data *last_companion = NULL;
	struct resource_data *res, *last_res = NULL;
	struct coin_data *coin, *last_coin = NULL;
	struct mail_data *mail, *last_mail = NULL;
	struct player_completed_quest *plrcom;
	struct player_ability_data *abildata;
	struct player_automessage *automsg;
	struct player_skill_data *skdata;
	int i_in[7], iter, num, val;
	struct player_event_data *ped;
	struct player_lastname *lastn;
	struct slash_channel *slash;
	obj_data *obj, *o, *next_o;
	struct req_data *task;
	obj_data **cont_row;
	account_data *acct;
	bitvector_t bit_in;
	bool end = FALSE;
	trig_data *trig;
	double dbl_in;
	long l_in[3], stored;
	char c_in[2];
	
	// allocate player if we didn't receive one
	if (!ch) {
		CREATE(ch, char_data, 1);
		clear_char(ch);
		init_player_specials(ch);
		clear_player(ch);
	
		// this is now
		ch->player.time.logon = time(0);
	}
	
	// ensure script allocation
	if (!SCRIPT(ch)) {
		create_script_data(ch, MOB_TRIGGER);
	}
	
	// some parts may already be added, so find the end of lists:
	if ((last_alias = GET_ALIASES(ch))) {
		while (last_alias->next) {
			last_alias = last_alias->next;
		}
	}
	if ((last_mail = GET_MAIL_PENDING(ch))) {
		while (last_mail->next) {
			last_mail = last_mail->next;
		}
	}
	if ((last_res = GET_ACTION_RESOURCES(ch))) {
		while (last_res->next) {
			last_res = last_res->next;
		}
	}
	if ((last_plrq = GET_QUESTS(ch))) {
		while (last_plrq->next) {
			last_plrq = last_plrq->next;
		}
	}
	
	// prepare contaienrs for item load
	CREATE(cont_row, obj_data*, MAX_BAG_ROWS);
	for (iter = 0; iter < MAX_BAG_ROWS; ++iter) {
		cont_row[iter] = NULL;
	}
	
	// We want to read in any old lore ahead of any we've appended already.
	// This happens if the player was loaded for an empire merge and new lore
	// was added before delayed data.
	new_lore = GET_LORE(ch);
	GET_LORE(ch) = NULL;
	
	// for fread_string
	sprintf(error, "read_player_from_file: %s", name);
	
	// for more readable if/else chain
	#define BAD_TAG_WARNING(src)  else if (LOG_BAD_TAG_WARNINGS) { \
		log("SYSERR: Bad tag in player '%s': %s", NULLSAFE(name), (src));	\
	}

	while (!end) {
		if (!get_line(fl, line)) {
			log("SYSERR: Unexpected end of player file in read_player_from_file: %s", name);
			exit(1);
		}
		
		// normal tags by letter
		switch (UPPER(*line)) {
			case '#': {	// an item
				sscanf(line, "#%d", &i_in[0]);
				if ((obj = Obj_load_from_file(fl, i_in[0], &i_in[1], ch->desc ? ch : NULL, error))) {
					loaded_obj_to_char(obj, ch, i_in[1], &cont_row);
				}
				break;
			}
			case 'A': {
				if (!strn_cmp(line, "Ability: ", 9)) {
					if (sscanf(line + 9, "%d %d %d %d", &i_in[0], &i_in[1], &i_in[2], &i_in[3]) == 4) {
						// post-b4.5 version
						if ((abildata = get_ability_data(ch, i_in[0], TRUE))) {
							abildata->levels_gained = i_in[1];
							// note: NUM_SKILL_SETS is not used for this, and 2 are assumed
							abildata->purchased[0] = i_in[2] ? TRUE : FALSE;
							abildata->purchased[1] = i_in[3] ? TRUE : FALSE;
						}
					}
					else if (sscanf(line + 9, "%d %d %d", &i_in[0], &i_in[1], &i_in[2]) == 3) {
						// backwards-compatibility
						if ((abildata = get_ability_data(ch, i_in[0], TRUE))) {
							abildata->purchased[0] = i_in[1] ? TRUE : FALSE;
							abildata->levels_gained = i_in[2];
						}
					}
				}
				else if (!strn_cmp(line, "Access Level: ", 14)) {
					GET_ACCESS_LEVEL(ch) = atoi(line + 14);
				}
				else if (!strn_cmp(line, "Account: ", 9)) {
					account_id = atoi(line + 9);
				}
				else if (!strn_cmp(line, "Action: ", 8)) {
					if (sscanf(line + 8, "%d %lf %d %d", &i_in[0], &dbl_in, &i_in[2], &i_in[3]) == 4) {
						GET_ACTION(ch) = i_in[0];
						GET_ACTION_CYCLE(ch) = dbl_in;
						GET_ACTION_TIMER(ch) = i_in[2];
						GET_ACTION_ROOM(ch) = i_in[3];
					}
				}
				else if (!strn_cmp(line, "Actstr: ", 8)) {
					if (GET_ACTION_STRING(ch)) {
						free(GET_ACTION_STRING(ch));
					}
					GET_ACTION_STRING(ch) = str_dup(trim(line + 10));
				}
				else if (!strn_cmp(line, "Action-res: ", 12)) {
					if (sscanf(line + 12, "%d %d %d %d", &i_in[0], &i_in[1], &i_in[2], &i_in[3]) == 4) {
						// argument order is for consistency with other resource lists
						CREATE(res, struct resource_data, 1);
						res->vnum = i_in[0];
						res->amount = i_in[1];
						res->type = i_in[2];
						res->misc = i_in[3];
						
						if (last_res) {
							last_res->next = res;
						}
						else {
							GET_ACTION_RESOURCES(ch) = res;
						}
						last_res = res;
					}
				}
				else if (!strn_cmp(line, "Action-targets: ", 16)) {
					if (sscanf(line + 16, "%d %lld %d %d", &i_in[0], &bit_in, &i_in[2], &i_in[3]) == 4) {
						// character and vehicle must be converted once in the game
						GET_ACTION_TEMPORARY_CHAR_ID(ch) = i_in[0];
						GET_ACTION_MULTI_TARG(ch) = bit_in;
						GET_ACTION_TEMPORARY_VEH_ID(ch) = i_in[2];
						GET_ACTION_ROOM_TARG(ch) = i_in[3];
						
						// object targets are lost in this process
					}
				}
				else if (!strn_cmp(line, "Action-vnum: ", 13)) {
					if (sscanf(line + 13, "%d %d", &i_in[0], &i_in[1]) == 2) {
						if (i_in[0] < NUM_ACTION_VNUMS) {
							GET_ACTION_VNUM(ch, i_in[0]) = i_in[1];
						}
					}
				}
				else if (!strn_cmp(line, "Adventure Summon Instance: ", 27)) {
					GET_ADVENTURE_SUMMON_INSTANCE_ID(ch) = atoi(line + 27);
				}
				else if (!strn_cmp(line, "Adventure Summon Loc: ", 22)) {
					GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch) = atoi(line + 22);
				}
				else if (!strn_cmp(line, "Adventure Summon Map: ", 22)) {
					GET_ADVENTURE_SUMMON_RETURN_MAP(ch) = atoi(line + 22);
				}
				else if (!strn_cmp(line, "Affect: ", 8)) {
					sscanf(line + 8, "%d %d %ld %d %d %s", &i_in[0], &i_in[1], &l_in[2], &i_in[3], &i_in[4], str_in);
					CREATE(af, struct affected_type, 1);
					af->type = i_in[0];
					af->cast_by = i_in[1];
					af->expire_time = l_in[2];	// this is TEMPORARILY a number of seconds (or UNLIMITED)
					af->modifier = i_in[3];
					af->location = i_in[4];
					af->bitvector = asciiflag_conv(str_in);
					
					// store for later
					LL_APPEND(af_list, af);
				}
				else if (!strn_cmp(line, "Affect Flags: ", 14)) {
					AFF_FLAGS(ch) = asciiflag_conv(line + 14);
				}
				else if (!strn_cmp(line, "Alias: ", 7)) {
					sscanf(line + 7, "%d %ld %ld", &i_in[0], &l_in[0], &l_in[1]);
					CREATE(alias, struct alias_data, 1);
					alias->type = i_in[0];
					
					fgets(line, l_in[0] + 2, fl);
					line[l_in[0]] = '\0';	// trailing \n
					alias->alias = str_dup(line);
					
					*line = ' ';	// Doesn't need terminated, fgets() will
					fgets(line + 1, l_in[1] + 2, fl);
					line[l_in[1] + 1] = '\0';	// trailing \n
					alias->replacement = str_dup(line);
					
					// append to end
					if (last_alias) {
						last_alias->next = alias;
					}
					else {
						GET_ALIASES(ch) = alias;
					}
					last_alias = alias;
				}
				else if (!strn_cmp(line, "Apparent Age: ", 14)) {
					GET_APPARENT_AGE(ch) = atoi(line + 14);
				}
				else if (!strn_cmp(line, "Archetype: ", 11)) {
					// NOTE: This tag is outdated and these are now stored as 'Creation Archetype'
					CREATION_ARCHETYPE(ch, ARCHT_ORIGIN) = atoi(line + 11);
				}
				else if (!strn_cmp(line, "Attribute: ", 11)) {
					sscanf(line + 11, "%s %d", str_in, &i_in[0]);
					for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
						if (!str_cmp(str_in, attributes[iter].name)) {
							GET_REAL_ATT(ch, iter) = i_in[0];
							GET_ATT(ch, iter) = i_in[0];
							break;
						}
					}
				}
				else if (!strn_cmp(line, "Automessage: ", 13)) {
					sscanf(line + 13, "%d %ld", &i_in[0], &l_in[0]);
					CREATE(automsg, struct player_automessage, 1);
					automsg->id = i_in[0];
					automsg->timestamp = l_in[0];
					HASH_ADD_INT(GET_AUTOMESSAGES(ch), id, automsg);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'B': {
				if (!strn_cmp(line, "Bad passwords: ", 15)) {
					GET_BAD_PWS(ch) = atoi(line + 15);
				}
				else if (!strn_cmp(line, "Birth: ", 7)) {
					ch->player.time.birth = atol(line + 7);
				}
				else if (!strn_cmp(line, "Bonus Exp: ", 11)) {
					GET_DAILY_BONUS_EXPERIENCE(ch) = atoi(line + 11);
				}
				else if (!strn_cmp(line, "Bonus Traits: ", 14)) {
					GET_BONUS_TRAITS(ch) = asciiflag_conv(line + 14);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'C': {
				if (!strn_cmp(line, "Can Gain New Skills: ", 21)) {
					CAN_GAIN_NEW_SKILLS(ch) = atoi(line + 21) ? TRUE : FALSE;
				}
				else if (!strn_cmp(line, "Can Get Bonus Skills: ", 22)) {
					// this property is no longer used
					// CAN_GET_BONUS_SKILLS(ch) = atoi(line + 22) ? TRUE : FALSE;
				}
				else if (!strn_cmp(line, "Class: ", 7)) {
					GET_CLASS(ch) = find_class_by_vnum(atoi(line + 7));
				}
				else if (!strn_cmp(line, "Class Progression: ", 19)) {
					GET_CLASS_PROGRESSION(ch) = atoi(line + 19);
				}
				else if (!strn_cmp(line, "Class Role: ", 12)) {
					GET_CLASS_ROLE(ch) = atoi(line + 12);
				}
				else if (!strn_cmp(line, "Condition: ", 11)) {
					sscanf(line + 11, "%s %d", str_in, &i_in[0]);
					if ((num = search_block(str_in, condition_types, TRUE)) != NOTHING) {
						GET_COND(ch, num) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Coin: ", 6)) {
					sscanf(line + 6, "%d %d %ld", &i_in[0], &i_in[1], &l_in[0]);
					
					CREATE(coin, struct coin_data, 1);
					coin->amount = i_in[0];
					coin->empire_id = i_in[1];
					coin->last_acquired = (time_t)l_in[0];
					
					// add to end
					if (last_coin) {
						last_coin->next = coin;
					}
					else {
						GET_PLAYER_COINS(ch) = coin;
					}
					last_coin = coin;
				}
				else if (!strn_cmp(line, "Companion: ", 11)) {
					sscanf(line + 11, "%d %d %d", &i_in[0], &i_in[1], &i_in[2]);
					last_companion = add_companion(ch, i_in[0], i_in[1]);
					last_companion->instantiated = i_in[2] ? TRUE : FALSE;
				}
				else if (!strn_cmp(line, "Compan-mod: ", 12)) {
					sscanf(line + 12, "%d %d", &i_in[0], &i_in[1]);
					read = fread_string(fl, error);
					
					if (last_companion) {
						struct companion_mod *cmod;
						CREATE(cmod, struct companion_mod, 1);
						cmod->type = i_in[0];
						cmod->num = i_in[1];
						cmod->str = read;
						LL_PREPEND(last_companion->mods, cmod);
					}
					else if (read) {
						// something went wrong?
						free(read);
					}
				}
				else if (!strn_cmp(line, "Compan-trg: ", 12)) {
					sscanf(line + 12, "%d", &i_in[0]);
					if (last_companion) {
						struct trig_proto_list *tpl;
						CREATE(tpl, struct trig_proto_list, 1);
						tpl->vnum = i_in[0];
						LL_APPEND(last_companion->scripts, tpl);
					}
				}
				else if (!strn_cmp(line, "Compan-var: ", 12)) {
					if (sscanf(line + 12, "%s %ld", str_in, &l_in[0]) != 2 || !get_line(fl, line)) {
						log("SYSERR: Bad compan-var format in read_player_delayed_data: %s", GET_NAME(ch));
						exit(1);
					}
					if (last_companion) {
						add_var(&(last_companion->vars), str_in, line, l_in[0]);
					}
				}
				else if (!strn_cmp(line, "Confused Direction: ", 20)) {
					GET_CONFUSED_DIR(ch) = atoi(line + 20);
				}
				else if (!strn_cmp(line, "Cooldown: ", 10)) {
					sscanf(line + 10, "%d %ld", &i_in[0], &l_in[0]);
					add_cooldown(ch, i_in[0], l_in[0] - time(0));
				}
				else if (!strn_cmp(line, "Creation Archetype: ", 20)) {
					sscanf(line + 20, "%d %d", &i_in[0], &i_in[1]);
					if (i_in[0] >= 0 && i_in[0] < NUM_ARCHETYPE_TYPES) {
						CREATION_ARCHETYPE(ch, i_in[0]) = i_in[1];
					}
				}
				else if (!strn_cmp(line, "Creation Host: ", 15)) {
					if (GET_CREATION_HOST(ch)) {
						free(GET_CREATION_HOST(ch));
					}
					GET_CREATION_HOST(ch) = str_dup(trim(line + 15));
				}
				else if (!strn_cmp(line, "Current Lastname: ", 18)) {
					if (GET_CURRENT_LASTNAME(ch)) {
						free(GET_CURRENT_LASTNAME(ch));
					}
					GET_CURRENT_LASTNAME(ch) = str_dup(trim(line + 18));
				}
				else if (!strn_cmp(line, "Current Pool: ", 14)) {
					sscanf(line + 14, "%s %d", str_in, &i_in[0]);
					if ((num = search_block(str_in, pool_types, TRUE)) != NOTHING) {
						GET_CURRENT_POOL(ch, num) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Color: ", 7)) {
					sscanf(line + 7, "%s %c", str_in, &c_in[0]);
					if ((num = search_block(str_in, custom_color_types, TRUE)) != NOTHING) {
						GET_CUSTOM_COLOR(ch, num) = c_in[0];
					}
				}
				else if (!strn_cmp(line, "Currency: ", 10)) {
					struct player_currency *cur;
					sscanf(line + 10, "%d %d", &i_in[0], &i_in[1]);
					
					// ensure no duplicates
					HASH_FIND_INT(GET_CURRENCIES(ch), &i_in[0], cur);
					if (!cur) {
						CREATE(cur, struct player_currency, 1);
						cur->vnum = i_in[0];
						HASH_ADD_INT(GET_CURRENCIES(ch), vnum, cur);
					}
					cur->amount = i_in[1];
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'D': {
				if (!strn_cmp(line, "Daily Cycle: ", 13)) {
					GET_DAILY_CYCLE(ch) = atoi(line + 13);
				}
				else if (!strn_cmp(line, "Daily Quests: ", 14)) {
					GET_DAILY_QUESTS(ch) = atoi(line + 14);
				}
				else if (!strn_cmp(line, "Deficit: ", 9)) {
					sscanf(line + 9, "%s %d", str_in, &i_in[0]);
					if ((num = search_block(str_in, pool_types, TRUE)) != NOTHING) {
						GET_DEFICIT(ch, num) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Description:", 12)) {
					// note no trailing space here
					if (GET_LOOK_DESC(ch)) {
						free(GET_LOOK_DESC(ch));
					}
					GET_LOOK_DESC(ch) = fread_string(fl, error);
				}
				else if (!strn_cmp(line, "Disguised Name: ", 16)) {
					if (GET_DISGUISED_NAME(ch)) {
						free(GET_DISGUISED_NAME(ch));
					}
					GET_DISGUISED_NAME(ch) = str_dup(trim(line + 16));
				}
				else if (!strn_cmp(line, "Disguised Sex: ", 15)) {
					if ((num = search_block(trim(line + 15), genders, TRUE)) != NOTHING) {
						GET_DISGUISED_SEX(ch) = num;
					}
				}
				else if (!strn_cmp(line, "DoT Effect: ", 12)) {
					sscanf(line + 12, "%d %d %d %d %d %d %d", &i_in[0], &i_in[1], &i_in[2], &i_in[3], &i_in[4], &i_in[5], &i_in[6]);
					CREATE(dot, struct over_time_effect_type, 1);
					dot->type = i_in[0];
					dot->cast_by = i_in[1];
					dot->time_remaining = i_in[2];
					dot->damage_type = i_in[3];
					dot->damage = i_in[4];
					dot->stack = i_in[5];
					dot->max_stack = i_in[6];
					
					// append to end
					if (last_dot) {
						last_dot->next = dot;
					}
					else {
						ch->over_time_effects = dot;
					}
					last_dot = dot;
					
					// do not schedule the DOT until they enter the game
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'E': {
				if (!strn_cmp(line, "Empire: ", 8)) {
					GET_LOYALTY(ch) = real_empire(atoi(line + 8));
				}
				else if (!strn_cmp(line, "End Player File", 15)) {
					// actual end
					end = TRUE;
				}
				else if (!strn_cmp(line, "End Primary Data", 16)) {
					// this tag is no longer used; ignore it
				}
				else if (!strn_cmp(line, "Eq-set: ", 8)) {
					if (sscanf(line + 8, "%d", &i_in[0]) == 1) {
						add_eq_set_to_char(ch, i_in[0], fread_string(fl, error));
					}
				}
				else if (!strn_cmp(line, "Event: ", 7)) {
					// last arg (level) is optional, for backwards-compatibility
					if ((val = sscanf(line + 7, "%d %d %ld %d %d %d %d %d", &i_in[0], &i_in[1], &l_in[0], &i_in[2], &i_in[3], &i_in[4], &i_in[5], &i_in[6])) >= 7) {
						if ((ped = create_event_data(ch, i_in[0], i_in[1]))) {
							ped->timestamp = l_in[0];
							ped->points = i_in[2];
							ped->collected_points = i_in[3];
							ped->rank = i_in[4];
							ped->status = i_in[5];
							ped->level = (val == 8) ? i_in[6] : 0;	// backwards-compatibility
						}
					}
				}
				else if (!strn_cmp(line, "Event Dailies: ", 15)) {
					GET_EVENT_DAILY_QUESTS(ch) = atoi(line + 14);
				}
				else if (!strn_cmp(line, "Extra Attribute: ", 17)) {
					sscanf(line + 17, "%s %d", str_in, &i_in[0]);
					if ((num = search_block(str_in, extra_attribute_types, TRUE)) != NOTHING) {
						// these are no longer saved/read
						// GET_EXTRA_ATT(ch, num) = i_in[0];
					}
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'F': {
				if (!strn_cmp(line, "Faction: ", 9)) {
					struct player_faction_data *pfd;
					sscanf(line + 9, "%d %d", &i_in[0], &i_in[1]);
					if ((pfd = get_reputation(ch, i_in[0], TRUE))) {
						pfd->value = i_in[1];
					}
				}
				else if (!strn_cmp(line, "Fight Messages: ", 16)) {
					GET_FIGHT_MESSAGES(ch) = asciiflag_conv(line + 16);
				}
				else if (!strn_cmp(line, "Fight Prompt: ", 14)) {
					if (GET_FIGHT_PROMPT(ch)) {
						free(GET_FIGHT_PROMPT(ch));
					}
					GET_FIGHT_PROMPT(ch) = str_dup(line + 14);	// do not trim
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'G': {
				if (!strn_cmp(line, "Grants: ", 8)) {
					GET_GRANT_FLAGS(ch) = asciiflag_conv(line + 8);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'H': {
				if (!strn_cmp(line, "Highest Known Level: ", 21)) {
					GET_HIGHEST_KNOWN_LEVEL(ch) = atoi(line + 21);
				}
				else if (!strn_cmp(line, "History: ", 9)) {
					if (sscanf(line + 9, "%d %ld %d %d %d %d", &i_in[0], &l_in[1], &i_in[2], &i_in[3], &i_in[4], &i_in[5]) != 6) {
						// backwards-compatible
						i_in[2] = 0;	// access_level
						i_in[3] = 0;	// rank
						i_in[4] = NOTHING;	// language
						i_in[5] = 0;	// is_disguised
						if (sscanf(line + 9, "%d %ld", &i_in[0], &l_in[1]) != 2) {
							// discard unknown message
							tmp = fread_string(fl, error);
							free(tmp);
							break;
						}
					}
					if (i_in[0] >= 0 && i_in[0] < NUM_CHANNEL_HISTORY_TYPES) {
						struct channel_history_data *hist;
						CREATE(hist, struct channel_history_data, 1);
						hist->timestamp = l_in[1];
						hist->access_level = i_in[2];
						hist->rank = i_in[3];
						hist->language = i_in[4];
						hist->is_disguised = i_in[5] ? TRUE : FALSE;
						hist->message = fread_string(fl, error);
						DL_APPEND(GET_HISTORY(ch, i_in[0]), hist);
					}
				}
				else if (!strn_cmp(line, "Home Storage: ", 14)) {
					sscanf(line + 14, "%d %d %d", &i_in[0], &i_in[1], &i_in[2]);
					if (!get_line(fl, line) || sscanf(line, "#%d", &i_in[3]) != 1) {
						log("SYSERR: Invalid Home Storage section of player %s: no obj", name);
						// possibly just not fatal, if next line gives no problems
						continue;
					}
					
					obj = Obj_load_from_file(fl, i_in[3], &junk, NULL, error);
					if (obj) {
						remove_from_object_list(obj);	// doesn't really go here right now
					
						CREATE(eus, struct empire_unique_storage, 1);
						eus->island = i_in[0];
						eus->amount = i_in[1];
						eus->flags = i_in[2];
						eus->obj = obj;
						
						DL_APPEND(GET_HOME_STORAGE(ch), eus);
					}
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'I': {
				if (!strn_cmp(line, "Idnum: ", 7)) {
					GET_IDNUM(ch) = atoi(line + 7);
					
					// assign this immediately
					ch->script_id = GET_IDNUM(ch);
				}
				else if (!strn_cmp(line, "Ignore: ", 8)) {
					if (ignore_pos < MAX_IGNORES) {
						GET_IGNORE_LIST(ch, ignore_pos++) = atoi(line + 8);
					}
				}
				else if (!strn_cmp(line, "Immortal Level: ", 16)) {
					GET_IMMORTAL_LEVEL(ch) = atoi(line + 16);
				}
				else if (!strn_cmp(line, "Informative: ", 13)) {
					GET_INFORMATIVE_FLAGS(ch) = asciiflag_conv(line + 13);
				}
				else if (!strn_cmp(line, "Injuries: ", 10)) {
					INJURY_FLAGS(ch) = asciiflag_conv(line + 10);
				}
				else if (!strn_cmp(line, "Invis Level: ", 13)) {
					GET_INVIS_LEV(ch) = atoi(line + 13);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'L': {
				if (!strn_cmp(line, "Language: ", 10)) {
					if (sscanf(line + 10, "%d %d", &i_in[0], &i_in[1]) == 2) {
						add_language(ch, i_in[0], i_in[1]);
					}
				}
				else if (!strn_cmp(line, "Largest Inventory: ", 19)) {
					GET_LARGEST_INVENTORY(ch) = atoi(line + 19);
				}
				else if (!strn_cmp(line, "Lastname: ", 10)) {
					if (GET_PERSONAL_LASTNAME(ch)) {
						free(GET_PERSONAL_LASTNAME(ch));
					}
					GET_PERSONAL_LASTNAME(ch) = str_dup(trim(line + 10));
				}
				else if (!strn_cmp(line, "Last Host: ", 11)) {
					if (ch->prev_host) {
						free(ch->prev_host);
					}
					ch->prev_host = str_dup(trim(line + 11));
				}
				else if (!strn_cmp(line, "Last Known Level: ", 18)) {
					GET_LAST_KNOWN_LEVEL(ch) = atoi(line + 18);
				}
				else if (!strn_cmp(line, "Last Logon: ", 12)) {
					ch->prev_logon = atol(line + 12);
				}
				else if (!strn_cmp(line, "Last Tell: ", 11)) {
					GET_LAST_TELL(ch) = atoi(line + 11);
				}
				else if (!strn_cmp(line, "Last Tip: ", 10)) {
					GET_LAST_TIP(ch) = atoi(line + 10);
				}
				else if (!strn_cmp(line, "Last Vehicle: ", 14)) {
					GET_LAST_VEHICLE(ch) = atoi(line + 14);
				}
				else if (!strn_cmp(line, "Last Room: ", 11)) {
					GET_LAST_ROOM(ch) = atoi(line + 11);
				}
				else if (!strn_cmp(line, "Last Companion: ", 16)) {
					GET_LAST_COMPANION(ch) = atoi(line + 16);
				}
				else if (!strn_cmp(line, "Last Direction: ", 16)) {
					GET_LAST_DIR(ch) = atoi(line + 16);
				}
				else if (!strn_cmp(line, "Last Death: ", 12)) {
					GET_LAST_DEATH_TIME(ch) = atol(line + 12);
				}
				else if (!strn_cmp(line, "Last Corpse Id: ", 16)) {
					GET_LAST_CORPSE_ID(ch) = atoi(line + 16);
				}
				else if (!strn_cmp(line, "Last Goal Check: ", 17)) {
					GET_LAST_GOAL_CHECK(ch) = atol(line + 17);
				}
				else if (!strn_cmp(line, "Last Home Set: ", 15)) {
					GET_LAST_HOME_SET_TIME(ch) = atol(line + 15);
				}
				else if (!strn_cmp(line, "Last Offense: ", 14)) {
					GET_LAST_OFFENSE_SEEN(ch) = atol(line + 14);
				}
				else if (!strn_cmp(line, "Lastname List: ", 15)) {
					if (*(line + 15)) {
						CREATE(lastn, struct player_lastname, 1);
						lastn->name = str_dup(line + 15);
						LL_APPEND(GET_LASTNAME_LIST(ch), lastn);
					}
				}
				else if (!strn_cmp(line, "Learned Craft: ", 15)) {
					if (sscanf(line + 15, "%d", &i_in[0]) == 1) {
						add_learned_craft(ch, i_in[0]);
					}
				}
				else if (!strn_cmp(line, "Load Room: ", 11)) {
					GET_LOADROOM(ch) = atoi(line + 11);
				}
				else if (!strn_cmp(line, "Load Room Check: ", 17)) {
					GET_LOAD_ROOM_CHECK(ch) = atoi(line + 17);
				}
				else if (!strn_cmp(line, "Lore: ", 6)) {
					sscanf(line + 6, "%d %ld", &i_in[0], &l_in[0]);
					CREATE(lore, struct lore_data, 1);
					lore->type = i_in[0];
					lore->date = l_in[0];
					
					// text on next line
					if (get_line(fl, line)) {
						lore->text = str_dup(line);
					}
					
					// append to end
					if (last_lore) {
						last_lore->next = lore;
					}
					else {
						GET_LORE(ch) = lore;
					}
					last_lore = lore;
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'M': {
				if (!strn_cmp(line, "Mail: ", 6)) {
					if ((mail = parse_mail(fl, line))) {
						if (last_mail) {
							last_mail->next = mail;
						}
						else {
							GET_MAIL_PENDING(ch) = mail;
						}
						last_mail = mail;
					}
				}
				else if (!strn_cmp(line, "Map Mark: ", 10)) {
					GET_MARK_LOCATION(ch) = atoi(line + 10);
				}
				else if (!strn_cmp(line, "Mapsize: ", 9)) {
					GET_MAPSIZE(ch) = atoi(line + 9);
				}
				else if (!strn_cmp(line, "Max Pool: ", 10)) {
					sscanf(line + 10, "%s %d", str_in, &i_in[0]);
					if ((num = search_block(str_in, pool_types, TRUE)) != NOTHING) {
						GET_MAX_POOL(ch, num) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Mini-pet: ", 10)) {
					if (sscanf(line + 10, "%d", &i_in[0]) == 1) {
						add_minipet(ch, i_in[0]);
					}
				}
				else if (!strn_cmp(line, "Morph: ", 7)) {
					GET_MORPH(ch) = morph_proto(atoi(line + 7));
				}
				else if (!strn_cmp(line, "Mount: ", 7)) {
					sscanf(line + 7, "%d %s", &i_in[0], str_in);
					// only if mob exists
					if (mob_proto(i_in[0])) {
						add_mount(ch, i_in[0], asciiflag_conv(str_in));
					}
				}
				else if (!strn_cmp(line, "Mount Flags: ", 13)) {
					GET_MOUNT_FLAGS(ch) = asciiflag_conv(line + 13);
				}
				else if (!strn_cmp(line, "Mount Vnum: ", 12)) {
					GET_MOUNT_VNUM(ch) = atoi(line + 12);
				}
				else if (!strn_cmp(line, "Mvstring: ", 10)) {
					// old version of "Actstr:"
					if (GET_ACTION_STRING(ch)) {
						free(GET_ACTION_STRING(ch));
					}
					GET_ACTION_STRING(ch) = str_dup(trim(line + 10));
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'N': {
				if (!strn_cmp(line, "Name: ", 6)) {
					if (GET_PC_NAME(ch)) {
						free(GET_PC_NAME(ch));
					}
					GET_PC_NAME(ch) = str_dup(trim(line + 6));
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'O': {
				if (!strn_cmp(line, "Offer: ", 7)) {
					sscanf(line + 7, "%d %d %d %ld %d", &i_in[0], &i_in[1], &i_in[2], &l_in[0], &i_in[3]);
					CREATE(offer, struct offer_data, 1);
					offer->from = i_in[0];
					offer->type = i_in[1];
					offer->location = i_in[2];
					offer->time = l_in[0];
					offer->data = i_in[3];
					
					// append to end
					if (last_offer) {
						last_offer->next = offer;
					}
					else {
						GET_OFFERS(ch) = offer;
					}
					last_offer = offer;
				}
				else if (!strn_cmp(line, "OLC: ", 5)) {
					sscanf(line + 5, "%d %d %s", &i_in[0], &i_in[1], str_in);
					GET_OLC_MIN_VNUM(ch) = i_in[0];
					GET_OLC_MAX_VNUM(ch) = i_in[1];
					GET_OLC_FLAGS(ch) = asciiflag_conv(str_in);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'P': {
				if (!strn_cmp(line, "Password: ", 10)) {
					if (GET_PASSWD(ch)) {
						free(GET_PASSWD(ch));
					}
					GET_PASSWD(ch) = str_dup(line + 10);
				}
				else if (!strn_cmp(line, "Played: ", 8)) {
					ch->player.time.played = atoi(line + 8);
				}
				else if (!strn_cmp(line, "Player Flags: ", 14)) {
					PLR_FLAGS(ch) = asciiflag_conv(line + 14);
				}
				else if (!strn_cmp(line, "Pledge Empire: ", 15)) {
					GET_PLEDGE(ch) = atoi(line + 15);
				}
				else if (!strn_cmp(line, "Poofin: ", 8)) {
					if (POOFIN(ch)) {
						free(POOFIN(ch));
					}
					POOFIN(ch) = str_dup(line + 8);	// do not trim
				}
				else if (!strn_cmp(line, "Poofout: ", 9)) {
					if (POOFOUT(ch)) {
						free(POOFOUT(ch));
					}
					POOFOUT(ch) = str_dup(line + 9);	// do not trim
				}
				else if (!strn_cmp(line, "Preferences: ", 13)) {
					PRF_FLAGS(ch) = asciiflag_conv(line + 13);
				}
				else if (!strn_cmp(line, "Promo ID: ", 10)) {
					GET_PROMO_ID(ch) = atoi(line + 10);
				}
				else if (!strn_cmp(line, "Prompt: ", 8)) {
					if (GET_PROMPT(ch)) {
						free(GET_PROMPT(ch));
					}
					GET_PROMPT(ch) = str_dup(line + 8);	// do not trim
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'Q': {
				if (!strn_cmp(line, "Quest: ", 7)) {
					if (sscanf(line + 7, "%d %d %ld %d %d", &i_in[0], &i_in[1], &l_in[0], &i_in[2], &i_in[3]) == 5) {
						CREATE(plrq, struct player_quest, 1);
						plrq->vnum = i_in[0];
						plrq->version = i_in[1];
						plrq->start_time = l_in[0];
						plrq->instance_id = i_in[2];
						plrq->adventure = i_in[3];
						
						if (last_plrq) {
							last_plrq->next = plrq;
						}
						else {
							GET_QUESTS(ch) = plrq;
						}
						last_plrq = plrq;
					}
				}
				else if (!strn_cmp(line, "Quest-cmp: ", 11)) {
					if (sscanf(line + 11, "%d %ld %d %d", &i_in[0], &l_in[0], &i_in[1], &i_in[2]) == 4) {
						HASH_FIND_INT(GET_COMPLETED_QUESTS(ch), &i_in[0], plrcom);
						// ensure not already in table
						if (!plrcom) {
							CREATE(plrcom, struct player_completed_quest, 1);
							plrcom->vnum = i_in[0];
							plrcom->last_completed = l_in[0];
							plrcom->last_instance_id = i_in[1];
							plrcom->last_adventure = i_in[2];
						
							HASH_ADD_INT(GET_COMPLETED_QUESTS(ch), vnum, plrcom);
						}
					}
				}
				else if (!strn_cmp(line, "Quest-task: ", 12)) {
					if (last_plrq && sscanf(line + 12, "%d %d %lld %d %d %c %c", &i_in[0], &i_in[1], &bit_in, &i_in[2], &i_in[3], &c_in[0], &c_in[1]) == 7) {
						// found everything
					}
					else if (last_plrq && sscanf(line + 12, "%d %d %lld %d %d %c", &i_in[0], &i_in[1], &bit_in, &i_in[2], &i_in[3], &c_in[0]) == 6) {
						// found group but no custom
						c_in[1] = 0;
					}
					else if (last_plrq && sscanf(line + 12, "%d %d %lld %d %d", &i_in[0], &i_in[1], &bit_in, &i_in[2], &i_in[3]) == 5) {
						c_in[0] = 0;	// no group given
						c_in[1] = 0;	// no custom info given
					}
					else {
						// bad format
						break;
					}
					
					CREATE(task, struct req_data, 1);
					task->type = i_in[0];
					task->vnum = i_in[1];
					task->misc = bit_in;
					task->needed = i_in[2];
					task->current = i_in[3];
					task->group = isalpha(c_in[0]) ? c_in[0] : 0;
					
					if (c_in[1] == '+') {
						task->custom = fread_string(fl, error);
					}
					
					LL_APPEND(last_plrq->tracker, task);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'R': {
				if (!strn_cmp(line, "Rank: ", 6)) {
					GET_RANK(ch) = atoi(line + 6);
				}
				else if (!strn_cmp(line, "Recent Deaths: ", 15)) {
					GET_RECENT_DEATH_COUNT(ch) = atoi(line + 15);
				}
				else if (!strn_cmp(line, "Referred by: ", 13)) {
					if (GET_REFERRED_BY(ch)) {
						free(GET_REFERRED_BY(ch));
					}
					GET_REFERRED_BY(ch) = str_dup(trim(line + 13));
				}
				else if (!strn_cmp(line, "Rent-code: ", 11)) {
					// old data; ignore
				}
				else if (!strn_cmp(line, "Rent-time: ", 11)) {
					// old data; ignore
				}
				else if (!strn_cmp(line, "Resource: ", 10)) {
					sscanf(line + 10, "%d %s", &i_in[0], str_in);
					for (iter = 0; iter < NUM_MATERIALS; ++iter) {
						if (!str_cmp(str_in, materials[iter].name)) {
							GET_RESOURCE(ch, iter) = i_in[0];
							break;
						}
					}
				}
				else if (!strn_cmp(line, "Rewarded: ", 10)) {
					// old data; ignore
				}
				else if (!strn_cmp(line, "Rope vnum: ", 11)) {
					GET_ROPE_VNUM(ch) = atoi(line + 11);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'S': {
				if (!strn_cmp(line, "Sex: ", 5)) {
					if ((num = search_block(trim(line + 5), genders, TRUE)) != NOTHING) {
						GET_REAL_SEX(ch) = num;
					}
				}
				else if (!strn_cmp(line, "Skill: ", 7)) {
					sscanf(line + 7, "%d %d %lf %d %d", &i_in[0], &i_in[1], &dbl_in, &i_in[2], &i_in[3]);
					
					if ((skdata = get_skill_data(ch, i_in[0], TRUE))) {
						skdata->level = i_in[1];
						skdata->exp = dbl_in;
						skdata->resets = i_in[2];
						skdata->noskill = i_in[3];
					}
				}
				else if (!strn_cmp(line, "Skill Level: ", 13)) {
					GET_SKILL_LEVEL(ch) = atoi(line + 13);
				}
				else if (!strn_cmp(line, "Skill Set: ", 11)) {
					GET_CURRENT_SKILL_SET(ch) = atoi(line + 11);
					GET_CURRENT_SKILL_SET(ch) = MAX(0, GET_CURRENT_SKILL_SET(ch));
					GET_CURRENT_SKILL_SET(ch) = MIN(NUM_SKILL_SETS-1, GET_CURRENT_SKILL_SET(ch));
				}
				else if (!strn_cmp(line, "Slash-channel: ", 15)) {
					CREATE(slash, struct slash_channel, 1);
					slash->name = str_dup(trim(line + 15));
					
					// append to start (it reverses them on-join anyway)
					LL_PREPEND(LOAD_SLASH_CHANNELS(ch), slash);
				}
				else if (!strn_cmp(line, "Slash-History: ", 15)) {
					// this line is ignored after b5.88 -- slash histories are now global
					char *junk = fread_string(fl, error);
					if (junk) {
						free(junk);
					}
				}
				else if (!strn_cmp(line, "Speaking: ", 10)) {
					GET_SPEAKING(ch) = atoi(line + 10);
				}
				else if (!strn_cmp(line, "Status Messages: ", 17)) {
					GET_STATUS_MESSAGES(ch) = asciiflag_conv(line + 17);
				}
				else if (!strn_cmp(line, "Syslog Flags: ", 14)) {
					SYSLOG_FLAGS(ch) = asciiflag_conv(line + 14);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'T': {
				if (!strn_cmp(line, "Temperature: ", 13)) {
					GET_TEMPERATURE(ch) = atoi(line + 13);
				}
				else if (!strn_cmp(line, "Temporary Account: ", 19)) {
					GET_TEMPORARY_ACCOUNT_ID(ch) = atoi(line + 19);
				}
				else if (!strn_cmp(line, "Title: ", 7)) {
					if (GET_TITLE(ch)) {
						free(GET_TITLE(ch));
					}
					GET_TITLE(ch) = str_dup(line + 7);	// do not trim
				}
				else if (!strn_cmp(line, "Tomb Room: ", 11)) {
					GET_TOMB_ROOM(ch) = atoi(line + 11);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'U': {
				if (!strn_cmp(line, "Using Ammo: ", 12)) {
					USING_AMMO(ch) = atoi(line + 12);
				}
				else if (!strn_cmp(line, "Using Poison: ", 14)) {
					USING_POISON(ch) = atoi(line + 14);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'V': {
				if (!strn_cmp(line, "Variable: ", 10)) {
					if (sscanf(line + 10, "%s %ld", str_in, &l_in[0]) != 2 || !get_line(fl, line)) {
						log("SYSERR: Bad variable format in read_player_delayed_data: %s", GET_NAME(ch));
						exit(1);
					}
					add_var(&(SCRIPT(ch)->global_vars), str_in, line, l_in[0]);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			
			default: {
				// ignore anything else and move on
				if (FALSE) { /* for BAD_TAG_WARNING */ }
				BAD_TAG_WARNING(line);
				break;
			}
		}
	}
	
	// quick safety checks
	if (!GET_PC_NAME(ch) || !*GET_PC_NAME(ch)) {
		log("SYSERR: Finished loading playerfile '%s' but did not find name", name);
		// hopefully we came in with a good name
		GET_PC_NAME(ch) = str_dup((name && *name) ? name : "Unknown");
		CAP(GET_PC_NAME(ch));
	}
	if (GET_IDNUM(ch) <= 0) {
		log("SYSERR: Finished loading playerfile '%s' but did not find idnum", GET_PC_NAME(ch));
		// TODO assign new idnum?
	}
	if (!GET_PASSWD(ch) || !*GET_PASSWD(ch)) {
		log("SYSERR: Finished loading playerfile '%s' but did not find password", GET_PC_NAME(ch));
	}
	
	// have account?
	if (normal && !GET_ACCOUNT(ch)) {
		if (!(acct = find_account(account_id))) {
			acct = create_account_for_player(ch);
		}
		GET_ACCOUNT(ch) = acct;
	}
	
	// fix lore order
	if (new_lore) {
		if (last_lore) {
			last_lore->next = new_lore;
		}
		else {
			GET_LORE(ch) = new_lore;
		}
	}
	
	// apply affects: need to manage the timers because they are generally WRONG at this point
	AFFECTS_CONVERTED(ch) = FALSE;
	LL_FOREACH_SAFE(af_list, af, next_af) {
		stored = af->expire_time;
		affect_to_char_silent(ch, af);
		
		// detect aff copied by affect_to_char_silent and fix timer
		if (ch->affected && ch->affected->type == af->type) {
			if (ch->affected->expire_event) {
				dg_event_cancel(ch->affected->expire_event, cancel_affect_expire_event);
				ch->affected->expire_event = NULL;
			}
			ch->affected->expire_time = stored;
		}
		free(af);
	}
	
	// safety
	REMOVE_BIT(PLR_FLAGS(ch), PLR_EXTRACTED | PLR_DONTSET);
	
	// Players who have been out for 1 hour get a free restore
	RESTORE_ON_LOGIN(ch) = (((int) (time(0) - ch->prev_logon)) >= 1 * SECS_PER_REAL_HOUR);
	if (GET_LOYALTY(ch)) {
		REREAD_EMPIRE_TECH_ON_LOGIN(ch) = (EMPIRE_MEMBERS(GET_LOYALTY(ch)) < 1 || get_member_timeout_time(ch->player.time.birth, ch->prev_logon, ((double)ch->player.time.played) / SECS_PER_REAL_HOUR) <= time(0));
	}
	
	// ensure random triggers are shut off on home storage
	DL_FOREACH(GET_HOME_STORAGE(ch), eus) {
		if (eus->obj && SCRIPT(eus->obj)) {
			LL_FOREACH(TRIGGERS(SCRIPT(eus->obj)), trig) {
				remove_trigger_from_global_lists(trig, TRUE);
			}
		}
	}
	
	// delivery anything left in cont_row
	for (iter = MAX_BAG_ROWS - 1; iter >= 0; --iter) {
		DL_FOREACH_SAFE2(cont_row[iter], o, next_o, next_content) {
			DL_DELETE2(cont_row[iter], o, prev_content, next_content);
			obj_to_char(o, ch);
		}
	}
	free(cont_row);
	
	return ch;
}


/**
* Removes a player from the player tables.
*
* @param player_index_data *plr The player to remove.
*/
void remove_player_from_table(player_index_data *plr) {
	HASH_DELETE(idnum_hh, player_table_by_idnum, plr);
	HASH_DELETE(name_hh, player_table_by_name, plr);
}


/*
 * write the vital data of a player to the player file -- this will not save
 * players who are disconnected.
 *
 * @param char_data *ch The player to save.
 * @param room_data *load_room (Optional) The location that the player will reappear on reconnect.
 */
void save_char(char_data *ch, room_data *load_room) {
	char filename[256], tempname[256];
	player_index_data *index;
	struct map_data *map;
	FILE *fl;

	if (IS_NPC(ch) || block_all_saves_due_to_shutdown) {
		return;
	}
	
	// update load room if they aren't flagged for a static one
	if (!PLR_FLAGGED(ch, PLR_LOADROOM)) {
		if (!load_room) {
			GET_LOADROOM(ch) = NOWHERE;
		}
		else if (IN_ROOM(ch) || GET_LOADROOM(ch) != GET_ROOM_VNUM(load_room)) {
			GET_LOADROOM(ch) = GET_ROOM_VNUM(load_room);
			map = GET_MAP_LOC(load_room);
			GET_LOAD_ROOM_CHECK(ch) = (map ? map->vnum : NOWHERE);
		}
		else {
			// preserve old loadroom/check data
		}
	}
	
	// PRIMARY data
	if (!get_filename(GET_PC_NAME(ch), filename, PLR_FILE)) {
		log("SYSERR: save_char: Unable to get player filename for '%s'", GET_PC_NAME(ch));
		return;
	}
	
	// store to a temp name in order to avoid problems from crashes during save
	snprintf(tempname, sizeof(tempname), "%s%s", filename, TEMP_SUFFIX);
	if (!(fl = fopen(tempname, "w"))) {
		log("SYSERR: save_char: Unable to open '%s' for writing", tempname);
		return;
	}
	
	// write it
	write_player_primary_data_to_file(fl, ch);
	
	fclose(fl);
	rename(tempname, filename);
	
	// delayed data?
	if (!NEEDS_DELAYED_LOAD(ch) && !DONT_SAVE_DELAY(ch)) {
		if (!get_filename(GET_PC_NAME(ch), filename, DELAYED_FILE)) {
			log("SYSERR: save_char: Unable to get delayed filename for '%s'", GET_PC_NAME(ch));
			return;
		}
	
		// store to a temp name in order to avoid problems from crashes during save
		snprintf(tempname, sizeof(tempname), "%s%s", filename, TEMP_SUFFIX);
		if (!(fl = fopen(tempname, "w"))) {
			log("SYSERR: save_char: Unable to open '%s' for writing", tempname);
			return;
		}
	
		// write it
		write_player_delayed_data_to_file(fl, ch);
	
		fclose(fl);
		rename(tempname, filename);
	}
	
	// check map memory
	write_map_memory(ch);
	
	// update the index in case any of this changed
	index = find_player_index_by_idnum(GET_IDNUM(ch));
	update_player_index(index, ch);
	
	// update empire logon time if the player is in-game
	if (IN_ROOM(ch) && !PLR_FLAGGED(ch, PLR_KEEP_LAST_LOGIN_INFO) && GET_LOYALTY(ch)) {
		EMPIRE_LAST_LOGON(GET_LOYALTY(ch)) = time(0);
		EMPIRE_NEEDS_SAVE(GET_LOYALTY(ch)) = TRUE;
	}
}


/**
* @param player_index_data *a One element
* @param player_index_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_players_by_idnum(player_index_data *a, player_index_data *b) {
	return a->idnum - b->idnum;
}


/**
* @param player_index_data *a One element
* @param player_index_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_players_by_name(player_index_data *a, player_index_data *b) {
	return strcmp(a->name, b->name);
}


/**
* For commands which load chars from file: this handles writing the output
* and frees the character. This should be used on a character loaded via
* find_or_load_player().
*
* ONLY use this if the character was loaded from file for a command like "set"
*
* @param char_data *ch the loaded char -- will be freed
*/
void store_loaded_char(char_data *ch) {
	save_char(ch, real_room(GET_LOADROOM(ch)));
	extract_all_items(ch);
	free_char(ch);
}


/**
* Updates the player index entry for the character. You must look the index
* up first, as this can be used before it's added to the player table.
*
* @param plar_index_data *index The index entry to update.
* @param char_data *ch The player character to update it with.
*/
void update_player_index(player_index_data *index, char_data *ch) {
	if (!index || !ch) {
		return;
	}
	
	index->idnum = GET_IDNUM(ch);
	
	// only copy the name once (it's used as a key)
	if (!index->name) {
		index->name = str_dup(GET_PC_NAME(ch));
		strtolower(index->name);
	}
	
	if (index->fullname) {
		free(index->fullname);
	}
	index->fullname = str_dup(PERS(ch, ch, TRUE));
	
	index->account_id = GET_ACCOUNT(ch) ? GET_ACCOUNT(ch)->id : 0;	// may not be set yet
	index->last_logon = ch->prev_logon;
	if (GET_ACCOUNT(ch)) {
		GET_ACCOUNT(ch)->last_logon = MAX(GET_ACCOUNT(ch)->last_logon, ch->prev_logon);
	}
	index->birth = ch->player.time.birth;
	index->played = ch->player.time.played;
	index->access_level = GET_ACCESS_LEVEL(ch);
	index->plr_flags = PLR_FLAGS(ch);
	index->loyalty = GET_LOYALTY(ch);
	index->rank = GET_RANK(ch);
	index->highest_known_level = GET_HIGHEST_KNOWN_LEVEL(ch);
	
	if (ch->desc || ch->prev_host) {
		if (index->last_host) {
			free(index->last_host);
		}
		index->last_host = str_dup((ch->desc && !PLR_FLAGGED(ch, PLR_KEEP_LAST_LOGIN_INFO)) ? ch->desc->host : ch->prev_host);
	}
}


/**
* Writes all the tagged data for one player to file. This is only the primary
* data -- data saved in the main file. See write_player_delayed_data_to_file().
*
* @param FILE *fl The open file to write to.
* @param char_data *ch The player to write.
*/
void write_player_primary_data_to_file(FILE *fl, char_data *ch) {
	struct affected_type *af, *new_af, *next_af, *af_list;
	struct player_ability_data *abil, *next_abil;
	struct player_skill_data *skill, *next_skill;
	struct player_craft_data *pcd, *next_pcd;
	struct player_currency *cur, *next_cur;
	struct player_language *lang, *next_lang;
	struct minipet_data *mini, *next_mini;
	struct mount_data *mount, *next_mount;
	struct player_slash_channel *slash;
	struct over_time_effect_type *dot;
	struct slash_channel *loadslash;
	struct slash_channel *channel;
	obj_data *char_eq[NUM_WEARS];
	char temp[MAX_STRING_LENGTH];
	struct cooldown_data *cool;
	struct resource_data *res;
	int iter, deficit[NUM_POOLS], pool[NUM_POOLS];
	long timer;
	
	if (!fl || !ch) {
		log("SYSERR: write_player_primary_data_to_file called without %s", fl ? "character" : "file");
		return;
	}
	if (IS_NPC(ch)) {
		log("SYSERR: write_player_primary_data_to_file called with NPC");
		return;
	}
	
	// prevent MANY additional affect_totals
	pause_affect_total = TRUE;
	
	// save these for later, as they are sometimes changed by removing and re-adding gear
	for (iter = 0; iter < NUM_POOLS; ++iter) {
		deficit[iter] = GET_DEFICIT(ch, iter);
		pool[iter] = GET_CURRENT_POOL(ch, iter);
	}
	
	// unaffect the character to store raw numbers: equipment
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter)) {
			char_eq[iter] = unequip_char(ch, iter);
			/* this is almopst certainly an error here as this is called on every save:
			#ifndef NO_EXTRANEOUS_TRIGGERS
				remove_otrigger(char_eq[iter], ch);
			#endif
			*/
		}
		else {
			char_eq[iter] = NULL;
		}
	}
	
	// unaffect: passives
	LL_FOREACH(GET_PASSIVE_BUFFS(ch), af) {
		affect_modify(ch, af->location, af->modifier, af->bitvector, FALSE);
	}
	
	// unaffect: affects
	af_list = NULL;
	while ((af = ch->affected)) {
		CREATE(new_af, struct affected_type, 1);
		*new_af = *af;
		new_af->expire_event = NULL;
		LL_PREPEND(af_list, new_af);
		affect_remove(ch, af);
	}
	
	// this is almost certainly ignored due to pause_affect_total
	affect_total(ch);
	
	// reset attributes
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		ch->aff_attributes[iter] = ch->real_attributes[iter];
	}
	
	// BEGIN TAGS
	
	// Player info
	fprintf(fl, "Name: %s\n", GET_PC_NAME(ch));
	fprintf(fl, "Password: %s\n", GET_PASSWD(ch));
	fprintf(fl, "Idnum: %d\n", GET_IDNUM(ch));
	if (GET_ACCOUNT(ch)) {
		fprintf(fl, "Account: %d\n", GET_ACCOUNT(ch)->id);
	}
	if (GET_TEMPORARY_ACCOUNT_ID(ch) != NOTHING) {
		fprintf(fl, "Temporary Account: %d\n", GET_TEMPORARY_ACCOUNT_ID(ch));
	}
	
	// Empire info
	if (GET_LOYALTY(ch)) {
		fprintf(fl, "Empire: %d\n", EMPIRE_VNUM(GET_LOYALTY(ch)));
		fprintf(fl, "Rank: %d\n", GET_RANK(ch));
	}
	if (GET_PLEDGE(ch) != NOTHING) {
		fprintf(fl, "Pledge Empire: %d\n", GET_PLEDGE(ch));
	}
	
	// Last login info
	if (PLR_FLAGGED(ch, PLR_KEEP_LAST_LOGIN_INFO)) {
		// player was loaded from file
		fprintf(fl, "Last Host: %s\n", NULLSAFE(ch->prev_host));
		fprintf(fl, "Last Logon: %ld\n", ch->prev_logon);
	}
	else {
		update_played_time(ch);
		fprintf(fl, "Last Host: %s\n", ch->desc ? ch->desc->host : NULLSAFE(ch->prev_host));
		fprintf(fl, "Last Logon: %ld\n", ch->player.time.logon);
	}
	
	// Pools
	for (iter = 0; iter < NUM_POOLS; ++iter) {
		fprintf(fl, "Current Pool: %s %d\n", pool_types[iter], GET_CURRENT_POOL(ch, iter));
		fprintf(fl, "Max Pool: %s %d\n", pool_types[iter], GET_MAX_POOL(ch, iter));
		if (GET_DEFICIT(ch, iter)) {
			fprintf(fl, "Deficit: %s %d\n", pool_types[iter], GET_DEFICIT(ch, iter));
		}
	}
	
	// The rest is alphabetical:
	
	// 'A'
	HASH_ITER(hh, GET_ABILITY_HASH(ch), abil, next_abil) {
		// Note: NUM_SKILL_SETS isn't used here but last 2 args are sets 0 and 1
		fprintf(fl, "Ability: %d %d %d %d\n", abil->vnum, abil->levels_gained, abil->purchased[0] ? 1 : 0, abil->purchased[1] ? 1 : 0);
	}
	fprintf(fl, "Access Level: %d\n", GET_ACCESS_LEVEL(ch));
	if (GET_ACTION(ch) != ACT_NONE) {
		fprintf(fl, "Action: %d %.1f %d %d\n", GET_ACTION(ch), GET_ACTION_CYCLE(ch), GET_ACTION_TIMER(ch), GET_ACTION_ROOM(ch));
		for (iter = 0; iter < NUM_ACTION_VNUMS; ++iter) {
			fprintf(fl, "Action-vnum: %d %d\n", iter, GET_ACTION_VNUM(ch, iter));
		}
		LL_FOREACH(GET_ACTION_RESOURCES(ch), res) {
			// argument order is for consistency with other resource lists
			fprintf(fl, "Action-res: %d %d %d %d\n", res->vnum, res->amount, res->type, res->misc);
		}
		if (GET_ACTION_CHAR_TARG(ch) || GET_ACTION_MULTI_TARG(ch) || GET_ACTION_VEH_TARG(ch) || GET_ACTION_ROOM_TARG(ch) != NOWHERE || GET_ACTION_TEMPORARY_CHAR_ID(ch) != 0 || GET_ACTION_TEMPORARY_VEH_ID(ch) != NOTHING) {
			// object targets are not quit-safe and are not saved
			fprintf(fl, "Action-targets: %d %lld %d %d\n", GET_ACTION_CHAR_TARG(ch) ? CAST_BY_ID(GET_ACTION_CHAR_TARG(ch)) : GET_ACTION_TEMPORARY_CHAR_ID(ch), GET_ACTION_MULTI_TARG(ch), GET_ACTION_VEH_TARG(ch) ? VEH_VNUM(GET_ACTION_VEH_TARG(ch)) : GET_ACTION_TEMPORARY_VEH_ID(ch), GET_ACTION_ROOM_TARG(ch));
		}
	}
	if (GET_ACTION_STRING(ch)) {
		strcpy(temp, GET_ACTION_STRING(ch));
		temp[MAX_ACTION_STRING] = '\0';	// ensure not longer than this (we would not be able to load it)
		fprintf(fl, "Actstr: %s\n", temp);
	}
	if (GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch) != NOWHERE) {
		fprintf(fl, "Adventure Summon Instance: %d\n", GET_ADVENTURE_SUMMON_INSTANCE_ID(ch));
		fprintf(fl, "Adventure Summon Loc: %d\n", GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch));
		fprintf(fl, "Adventure Summon Map: %d\n", GET_ADVENTURE_SUMMON_RETURN_MAP(ch));
	}
	for (af = af_list; af; af = af->next) {	// stored earlier
		if (af->expire_time == UNLIMITED) {
			timer = UNLIMITED;
		}
		else if (AFFECTS_CONVERTED(ch)) {
			timer = af->expire_time - time(0);
		}
		else {
			// still in seconds
			timer = af->expire_time;
		}
		fprintf(fl, "Affect: %d %d %ld %d %d %s\n", af->type, af->cast_by, timer, af->modifier, af->location, bitv_to_alpha(af->bitvector));
	}
	fprintf(fl, "Affect Flags: %s\n", bitv_to_alpha(AFF_FLAGS(ch)));
	if (GET_APPARENT_AGE(ch)) {
		fprintf(fl, "Apparent Age: %d\n", GET_APPARENT_AGE(ch));
	}
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		fprintf(fl, "Attribute: %s %d\n", attributes[iter].name, GET_REAL_ATT(ch, iter));
	}
	
	// 'B'
	if (GET_BAD_PWS(ch)) {
		fprintf(fl, "Bad passwords: %d\n", GET_BAD_PWS(ch));
	}
	fprintf(fl, "Birth: %ld\n", ch->player.time.birth);
	fprintf(fl, "Bonus Exp: %d\n", GET_DAILY_BONUS_EXPERIENCE(ch));
	fprintf(fl, "Bonus Traits: %s\n", bitv_to_alpha(GET_BONUS_TRAITS(ch)));
	
	// 'C'
	if (CAN_GAIN_NEW_SKILLS(ch)) {
		fprintf(fl, "Can Gain New Skills: 1\n");
	}
	fprintf(fl, "Class: %d\n", GET_CLASS(ch) ? CLASS_VNUM(GET_CLASS(ch)) : NOTHING);
	fprintf(fl, "Class Progression: %d\n", GET_CLASS_PROGRESSION(ch));
	fprintf(fl, "Class Role: %d\n", GET_CLASS_ROLE(ch));
	for (iter = 0; iter < NUM_CUSTOM_COLORS; ++iter) {
		if (GET_CUSTOM_COLOR(ch, iter) != 0) {
			fprintf(fl, "Color: %s %c\n", custom_color_types[iter], GET_CUSTOM_COLOR(ch, iter));
		}
	}
	for (iter = 0; iter < NUM_CONDS; ++iter) {
		if (GET_COND(ch, iter) != 0) {
			fprintf(fl, "Condition: %s %d\n", condition_types[iter], GET_COND(ch, iter));
		}
	}
	if (GET_CONFUSED_DIR(ch)) {
		fprintf(fl, "Confused Direction: %d\n", GET_CONFUSED_DIR(ch));
	}
	for (cool = ch->cooldowns; cool; cool = cool->next) {
		fprintf(fl, "Cooldown: %d %ld\n", cool->type, cool->expire_time);
	}
	for (iter = 0; iter < NUM_ARCHETYPE_TYPES; ++iter) {
		fprintf(fl, "Creation Archetype: %d %d\n", iter, CREATION_ARCHETYPE(ch, iter));
	}
	if (GET_CREATION_HOST(ch)) {
		fprintf(fl, "Creation Host: %s\n", GET_CREATION_HOST(ch));
	}
	if (GET_CURRENT_LASTNAME(ch)) {
		fprintf(fl, "Current Lastname: %s\n", GET_CURRENT_LASTNAME(ch));
	}
	HASH_ITER(hh, GET_CURRENCIES(ch), cur, next_cur) {
		fprintf(fl, "Currency: %d %d\n", cur->vnum, cur->amount);
	}
	
	// 'D'
	fprintf(fl, "Daily Cycle: %d\n", GET_DAILY_CYCLE(ch));
	fprintf(fl, "Daily Quests: %d\n", GET_DAILY_QUESTS(ch));
	if (GET_LOOK_DESC(ch)) {
		strcpy(temp, NULLSAFE(GET_LOOK_DESC(ch)));
		strip_crlf(temp);
		fprintf(fl, "Description:\n%s~\n", temp);
	}
	if (GET_DISGUISED_NAME(ch)) {
		fprintf(fl, "Disguised Name: %s\n", GET_DISGUISED_NAME(ch));
	}
	if (GET_DISGUISED_SEX(ch)) {
		fprintf(fl, "Disguised Sex: %s\n", genders[(int) GET_DISGUISED_SEX(ch)]);
	}
	for (dot = ch->over_time_effects; dot; dot = dot->next) {
		fprintf(fl, "DoT Effect: %d %d %d %d %d %d %d\n", dot->type, dot->cast_by, dot->time_remaining, dot->damage_type, dot->damage, dot->stack, dot->max_stack);
	}
	
	// 'E'
	fprintf(fl, "Event Dailies: %d\n", GET_EVENT_DAILY_QUESTS(ch));
	/* No longer writing extra attributes: these come from abilities/gear and should not be saved/loaded
		note that if you reenable this, you must also enable the part that loads it
	for (iter = 0; iter < NUM_EXTRA_ATTRIBUTES; ++iter) {
		if (GET_EXTRA_ATT(ch, iter)) {
			fprintf(fl, "Extra Attribute: %s %d\n", extra_attribute_types[iter], GET_EXTRA_ATT(ch, iter));
		}
	}
	*/
	
	// 'F'
	fprintf(fl, "Fight Messages: %s\n", bitv_to_alpha(GET_FIGHT_MESSAGES(ch)));
	if (GET_FIGHT_PROMPT(ch)) {
		fprintf(fl, "Fight Prompt: %s\n", GET_FIGHT_PROMPT(ch));
	}
	
	// 'G'
	if (GET_GRANT_FLAGS(ch)) {
		fprintf(fl, "Grants: %s\n", bitv_to_alpha(GET_GRANT_FLAGS(ch)));
	}
	
	// 'H'
	fprintf(fl, "Highest Known Level: %d\n", GET_HIGHEST_KNOWN_LEVEL(ch));
	
	// 'I'
	for (iter = 0; iter < MAX_IGNORES; ++iter) {
		if (GET_IGNORE_LIST(ch, iter) > 0) {
			fprintf(fl, "Ignore: %d\n", GET_IGNORE_LIST(ch, iter));
		}
	}
	if (GET_IMMORTAL_LEVEL(ch) != -1) {
		fprintf(fl, "Immortal Level: %d\n", GET_IMMORTAL_LEVEL(ch));
	}
	fprintf(fl, "Injuries: %s\n", bitv_to_alpha(INJURY_FLAGS(ch)));
	if (GET_INVIS_LEV(ch)) {
		fprintf(fl, "Invis Level: %d\n", GET_INVIS_LEV(ch));
	}
	
	// 'L'
	HASH_ITER(hh, GET_LANGUAGES(ch), lang, next_lang) {
		fprintf(fl, "Language: %d %d\n", lang->vnum, lang->level);
	}
	fprintf(fl, "Largest Inventory: %d\n", GET_LARGEST_INVENTORY(ch));
	if (GET_LAST_CORPSE_ID(ch) > 0) {
		fprintf(fl, "Last Corpse Id: %d\n", GET_LAST_CORPSE_ID(ch));
	}
	fprintf(fl, "Last Death: %ld\n", GET_LAST_DEATH_TIME(ch));
	fprintf(fl, "Last Direction: %d\n", GET_LAST_DIR(ch));
	fprintf(fl, "Last Goal Check: %ld\n", GET_LAST_GOAL_CHECK(ch));
	fprintf(fl, "Last Known Level: %d\n", GET_LAST_KNOWN_LEVEL(ch));
	fprintf(fl, "Last Room: %d\n", GET_LAST_ROOM(ch));
	if (GET_LAST_TELL(ch) != NOBODY) {
		fprintf(fl, "Last Tell: %d\n", GET_LAST_TELL(ch));
	}
	if (GET_LAST_TIP(ch)) {
		fprintf(fl, "Last Tip: %d\n", GET_LAST_TIP(ch));
	}
	if ((IN_ROOM(ch) && GET_SITTING_ON(ch)) || (!IN_ROOM(ch) && GET_LAST_VEHICLE(ch) != NOTHING)) {
		fprintf(fl, "Last Vehicle: %d\n", IN_ROOM(ch) ? VEH_VNUM(GET_SITTING_ON(ch)) : GET_LAST_VEHICLE(ch));
	}
	fprintf(fl, "Last Offense: %ld\n", GET_LAST_OFFENSE_SEEN(ch));
	if (GET_PERSONAL_LASTNAME(ch)) {
		fprintf(fl, "Lastname: %s\n", GET_PERSONAL_LASTNAME(ch));
	}
	HASH_ITER(hh, GET_LEARNED_CRAFTS(ch), pcd, next_pcd) {
		fprintf(fl, "Learned Craft: %d\n", pcd->vnum);
	}
	fprintf(fl, "Load Room: %d\n", GET_LOADROOM(ch));
	fprintf(fl, "Load Room Check: %d\n", GET_LOAD_ROOM_CHECK(ch));
	
	// 'M'
	if (GET_MARK_LOCATION(ch) != NOWHERE) {
		fprintf(fl, "Map Mark: %d\n", GET_MARK_LOCATION(ch));
	}
	if (GET_MAPSIZE(ch)) {
		fprintf(fl, "Mapsize: %d\n", GET_MAPSIZE(ch));
	}
	HASH_ITER(hh, GET_MINIPETS(ch), mini, next_mini) {
		fprintf(fl, "Mini-pet: %d\n", mini->vnum);
	}
	if (IS_MORPHED(ch)) {
		fprintf(fl, "Morph: %d\n", MORPH_VNUM(GET_MORPH(ch)));
	}
	HASH_ITER(hh, GET_MOUNT_LIST(ch), mount, next_mount) {
		fprintf(fl, "Mount: %d %s\n", mount->vnum, bitv_to_alpha(mount->flags));
	}
	if (GET_MOUNT_FLAGS(ch) != NOBITS) {
		fprintf(fl, "Mount Flags: %s\n", bitv_to_alpha(GET_MOUNT_FLAGS(ch)));
	}
	if (GET_MOUNT_VNUM(ch) != NOTHING) {
		fprintf(fl, "Mount Vnum: %d\n", GET_MOUNT_VNUM(ch));
	}
	
	// 'O'
	if (GET_OLC_MAX_VNUM(ch) > 0 || GET_OLC_MIN_VNUM(ch) > 0 || GET_OLC_FLAGS(ch) != NOBITS) {
		fprintf(fl, "OLC: %d %d %s\n", GET_OLC_MIN_VNUM(ch), GET_OLC_MAX_VNUM(ch), bitv_to_alpha(GET_OLC_FLAGS(ch)));
	}
	
	// 'P'
	fprintf(fl, "Played: %d\n", ch->player.time.played);
	fprintf(fl, "Player Flags: %s\n", bitv_to_alpha(PLR_FLAGS(ch)));
	if (POOFIN(ch)) {
		fprintf(fl, "Poofin: %s\n", POOFIN(ch));
	}
	if (POOFOUT(ch)) {
		fprintf(fl, "Poofout: %s\n", POOFOUT(ch));
	}
	if (PRF_FLAGS(ch)) {
		fprintf(fl, "Preferences: %s\n", bitv_to_alpha(PRF_FLAGS(ch)));
	}
	if (GET_PROMO_ID(ch)) {
		fprintf(fl, "Promo ID: %d\n", GET_PROMO_ID(ch));
	}
	if (GET_PROMPT(ch)) {
		fprintf(fl, "Prompt: %s\n", GET_PROMPT(ch));
	}
	
	// 'R'
	if (GET_RECENT_DEATH_COUNT(ch)) {
		fprintf(fl, "Recent Deaths: %d\n", GET_RECENT_DEATH_COUNT(ch));
	}
	if (GET_REFERRED_BY(ch)) {
		fprintf(fl, "Referred by: %s\n", GET_REFERRED_BY(ch));
	}
	for (iter = 0; iter < NUM_MATERIALS; ++iter) {
		if (GET_RESOURCE(ch, iter) != 0) {
			fprintf(fl, "Resource: %d %s\n", GET_RESOURCE(ch, iter), materials[iter].name);
		}
	}
	if (GET_ROPE_VNUM(ch) != NOTHING) {
		fprintf(fl, "Rope vnum: %d\n", GET_ROPE_VNUM(ch));
	}
	
	// 'S'
	fprintf(fl, "Sex: %s\n", genders[(int) GET_REAL_SEX(ch)]);
	HASH_ITER(hh, GET_SKILL_HASH(ch), skill, next_skill) {
		// don't bother writing ones with no data
		if (skill->level > 0 || skill->exp > 0 || skill->resets > 0 || skill->noskill > 0) {
			fprintf(fl, "Skill: %d %d %.2f %d %d\n", skill->vnum, skill->level, skill->exp, skill->resets, skill->noskill ? 1 : 0);
		}
	}
	fprintf(fl, "Skill Level: %d\n", GET_SKILL_LEVEL(ch));
	fprintf(fl, "Skill Set: %d\n", GET_CURRENT_SKILL_SET(ch));
	for (slash = GET_SLASH_CHANNELS(ch); slash; slash = slash->next) {
		if ((channel = find_slash_channel_by_id(slash->id))) {
			fprintf(fl, "Slash-channel: %s\n", channel->name);
		}
	}
	for (loadslash = LOAD_SLASH_CHANNELS(ch); loadslash; loadslash = loadslash->next) {
		if (loadslash->name) {
			// these are half-loaded slash channels and save in the same way
			fprintf(fl, "Slash-channel: %s\n", loadslash->name);
		}
	}
	if (GET_SPEAKING(ch) != NOTHING) {
		fprintf(fl, "Speaking: %d\n", GET_SPEAKING(ch));
	}
	fprintf(fl, "Status Messages: %s\n", bitv_to_alpha(GET_STATUS_MESSAGES(ch)));
	if (SYSLOG_FLAGS(ch)) {
		fprintf(fl, "Syslog Flags: %s\n", bitv_to_alpha(SYSLOG_FLAGS(ch)));
	}
	
	// 'T'
	if (GET_TEMPERATURE(ch)) {
		fprintf(fl, "Temperature: %d\n", GET_TEMPERATURE(ch));
	}
	if (GET_TITLE(ch)) {
		fprintf(fl, "Title: %s\n", GET_TITLE(ch));
	}
	if (GET_TOMB_ROOM(ch) != NOWHERE) {
		fprintf(fl, "Tomb Room: %d\n", GET_TOMB_ROOM(ch));
	}
	
	// 'U'
	if (USING_AMMO(ch)) {
		fprintf(fl, "Using Ammo: %d\n", USING_AMMO(ch));
	}
	if (USING_POISON(ch) != NOTHING) {
		fprintf(fl, "Using Poison: %d\n", USING_POISON(ch));
	}
	
	// # save equipment
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (char_eq[iter]) {
			Crash_save(char_eq[iter], fl, iter + 1);	// save at iter+1 because 0 == LOC_INVENTORY
		}
	}
	
	// END PLAYER FILE
	fprintf(fl, "End Player File\n");
	
	// re-affect: passives
	LL_FOREACH(GET_PASSIVE_BUFFS(ch), af) {
		affect_modify(ch, af->location, af->modifier, af->bitvector, TRUE);
	}
	
	// re-apply: affects
	for (af = af_list; af; af = next_af) {
		next_af = af->next;
		affect_to_char_silent(ch, af);
		free(af);
	}
	
	// re-apply: equipment
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (char_eq[iter]) {
			/* this is almost certainly an error since this is called on every save:
			#ifndef NO_EXTRANEOUS_TRIGGERS
				if (wear_otrigger(char_eq[iter], ch, iter)) {
			#endif
			*/
					// this line may depend on the above if NO_EXTRANEOUS_TRIGGERS is off
					equip_char(ch, char_eq[iter], iter);
			/* probably an error here (see above):
			#ifndef NO_EXTRANEOUS_TRIGGERS
				}
				else {
					obj_to_char(char_eq[iter], ch);
				}
			#endif
			*/
		}
	}
	
	// restore pools, which may have been modified
	for (iter = 0; iter < NUM_POOLS; ++iter) {
		GET_CURRENT_POOL(ch, iter) = pool[iter];	// set ok: character cannot have taken damage here
		GET_DEFICIT(ch, iter) = deficit[iter];
	}
	
	// resume affect totals and run it
	pause_affect_total = FALSE;
	affect_total(ch);	// not 100% sure this function needs this, but at least now it only does it once -pc 4/22/18
}


/**
* Writes all the DELAYED data for one player to file. This is saved separately
* in order to reduce loading in many cases.
*
* @param FILE *fl The open file to write to.
* @param char_data *ch The player to write.
*/
void write_player_delayed_data_to_file(FILE *fl, char_data *ch) {
	char temp[MAX_STRING_LENGTH];
	struct player_completed_quest *plrcom, *next_plrcom;
	struct player_automessage *automsg, *next_automsg;
	struct companion_data *compan, *next_compan;
	struct player_faction_data *pfd, *next_pfd;
	struct player_event_data *ped, *next_ped;
	struct empire_unique_storage *eus;
	struct channel_history_data *hist;
	struct player_lastname *lastn;
	struct trig_proto_list *tpro;
	struct player_eq_set *eq_set;
	struct companion_mod *cmod;
	struct trig_var_data *vars;
	struct player_quest *plrq;
	struct alias_data *alias;
	struct offer_data *offer;
	struct req_data *task;
	struct coin_data *coin;
	struct lore_data *lore;
	int iter;
	
	if (!fl || !ch) {
		log("SYSERR: write_player_delayed_data_to_file called without %s", fl ? "character" : "file");
		return;
	}
	if (IS_NPC(ch)) {
		log("SYSERR: write_player_delayed_data_to_file called with NPC");
		return;
	}
	
	// BEGIN TAGS
	
	// 'A'
	for (alias = GET_ALIASES(ch); alias; alias = alias->next) {
		fprintf(fl, "Alias: %d %ld %ld\n%s\n%s\n", alias->type, strlen(alias->alias), strlen(alias->replacement)-1, alias->alias, alias->replacement + 1);
	}
	HASH_ITER(hh, GET_AUTOMESSAGES(ch), automsg, next_automsg) {
		fprintf(fl, "Automessage: %d %ld\n", automsg->id, automsg->timestamp);
	}
	
	// 'C'
	for (coin = GET_PLAYER_COINS(ch); coin; coin = coin->next) {
		fprintf(fl, "Coin: %d %d %ld\n", coin->amount, coin->empire_id, coin->last_acquired);
	}
	HASH_ITER(hh, GET_COMPANIONS(ch), compan, next_compan) {
		fprintf(fl, "Companion: %d %d %d\n", compan->vnum, compan->from_abil, compan->instantiated);
		LL_FOREACH(compan->mods, cmod) {
			if (cmod->str && *cmod->str) {
				strcpy(temp, cmod->str);
				strip_crlf(temp);
			}
			else {
				*temp = '\0';
			}
			fprintf(fl, "Compan-mod: %d %d\n%s~\n", cmod->type, cmod->num, temp);
		}
		LL_FOREACH(compan->scripts, tpro) {
			fprintf(fl, "Compan-trg: %d\n", tpro->vnum);
		}
		LL_FOREACH(compan->vars, vars) {
			if (*vars->name == '-' || !*vars->value) { // don't save if it begins with - or is empty
				continue;
			}
			
			fprintf(fl, "Compan-var: %s %ld\n%s\n", vars->name, vars->context, vars->value);
		}
	}
	
	// 'E'
	LL_FOREACH(GET_EQ_SETS(ch), eq_set) {
		fprintf(fl, "Eq-set: %d\n%s~\n", eq_set->id, eq_set->name);
	}
	HASH_ITER(hh, GET_EVENT_DATA(ch), ped, next_ped) {
		if (ped->event) {
			fprintf(fl, "Event: %d %d %ld %d %d %d %d %d\n", ped->id, EVT_VNUM(ped->event), ped->timestamp, ped->points, ped->collected_points, ped->rank, ped->status, ped->level);
		}
	}
	
	// 'F'
	HASH_ITER(hh, GET_FACTIONS(ch), pfd, next_pfd) {
		fprintf(fl, "Faction: %d %d\n", pfd->vnum, pfd->value);
	}
	
	// 'H'
	for (iter = 0; iter < NUM_CHANNEL_HISTORY_TYPES; ++iter) {
		DL_FOREACH(GET_HISTORY(ch, iter), hist) {
			fprintf(fl, "History: %d %ld %d %d %d %d\n%s~\n", iter, hist->timestamp, hist->access_level, hist->rank, hist->language, hist->is_disguised ? 1 : 0, NULLSAFE(hist->message));
		}
	}
	DL_FOREACH(GET_HOME_STORAGE(ch), eus) {
		if (eus->amount <= 0 || !eus->obj) {
			continue;	// ??
		}
		fprintf(fl, "Home Storage: %d %d %d\n", eus->island, eus->amount, eus->flags);
		Crash_save_one_obj_to_file(fl, eus->obj, 0);
	}
	
	// 'I'
	if (GET_INFORMATIVE_FLAGS(ch)) {
		fprintf(fl, "Informative: %s\n", bitv_to_alpha(GET_INFORMATIVE_FLAGS(ch)));
	}
	
	// 'L'
	if (GET_LAST_COMPANION(ch) != NOTHING) {
		fprintf(fl, "Last Companion: %d\n", GET_LAST_COMPANION(ch));
	}
	if (GET_LAST_HOME_SET_TIME(ch)) {
		fprintf(fl, "Last Home Set: %ld\n", GET_LAST_HOME_SET_TIME(ch));
	}
	LL_FOREACH(GET_LASTNAME_LIST(ch), lastn) {
		if (lastn->name) {
			fprintf(fl, "Lastname List: %s\n", lastn->name);
		}
	}
	for (lore = GET_LORE(ch); lore; lore = lore->next) {
		if (lore->text && *lore->text) {
			fprintf(fl, "Lore: %d %ld\n%s\n", lore->type, lore->date, lore->text);
		}
	}
	
	// 'M'
	write_mail_to_file(fl, ch);
	
	// 'O'
	for (offer = GET_OFFERS(ch); offer; offer = offer->next) {
		fprintf(fl, "Offer: %d %d %d %ld %d\n", offer->from, offer->type, offer->location, offer->time, offer->data);
	}
	
	// 'Q'
	LL_FOREACH(GET_QUESTS(ch), plrq) {
		fprintf(fl, "Quest: %d %d %ld %d %d\n", plrq->vnum, plrq->version, plrq->start_time, plrq->instance_id, plrq->adventure);
		LL_FOREACH(plrq->tracker, task) {
			fprintf(fl, "Quest-task: %d %d %lld %d %d %c %s\n", task->type, task->vnum, task->misc, task->needed, task->current, task->group ? task->group : '-', (task->custom && *task->custom) ? "+" : "");
			if (task->custom && *task->custom) {
				fprintf(fl, "%s~\n", task->custom);
			}
		}
	}
	HASH_ITER(hh, GET_COMPLETED_QUESTS(ch), plrcom, next_plrcom) {
		fprintf(fl, "Quest-cmp: %d %ld %d %d\n", plrcom->vnum, plrcom->last_completed, plrcom->last_instance_id, plrcom->last_adventure);
	}
	
	// 'S'
	
	// 'V'
	if (SCRIPT(ch) && SCRIPT(ch)->global_vars) {
		for (vars = SCRIPT(ch)->global_vars; vars; vars = vars->next) {
			if (*vars->name == '-' || !*vars->value) { // don't save if it begins with - or is empty
				continue;
			}
			
			fprintf(fl, "Variable: %s %ld\n%s\n", vars->name, vars->context, vars->value);
		}
	}
	
	// '#'
	Crash_save(ch->carrying, fl, LOC_INVENTORY);
	
	// END DELAY-LOADED SECTION
	fprintf(fl, "End Player File\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// DELAYED UPDATE SYSTEM ///////////////////////////////////////////////////

struct char_delayed_update *char_delayed_update_list = NULL;	// characters who need updating


/**
* Removes a queued char update, if any exists. This is generally called while
* extracting a character.
*
* @param char_data *ch The character to delete update entries for.
*/
void clear_delayed_update(char_data *ch) {
	struct char_delayed_update *cdu;
	int id;
	
	// use ch->script_id instead of char_script_id() because we don't want to assign one if it doesn't have one
	if (ch && (id = ch->script_id) > 0) {
		HASH_FIND_INT(char_delayed_update_list, &id, cdu);
		if (cdu) {
			HASH_DEL(char_delayed_update_list, cdu);
			free(cdu);
		}
	}
}


/**
* Queues a some kind of delayed update for a character.
*
* @param char_data *ch The player.
* @param bitvector_t type The CDU_ flag(s) to queue.
*/
void queue_delayed_update(char_data *ch, bitvector_t type) {
	struct char_delayed_update *cdu;
	int id;
	
	if (ch && !EXTRACTED(ch) && (id = char_script_id(ch)) > 0) {
		HASH_FIND_INT(char_delayed_update_list, &id, cdu);
		if (!cdu) {
			CREATE(cdu, struct char_delayed_update, 1);
			cdu->id = id;
			cdu->ch = ch;
			HASH_ADD_INT(char_delayed_update_list, id, cdu);
		}
		cdu->type |= type;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// LASTNAME HANDLERS ///////////////////////////////////////////////////////

/**
* Grants a lastname to a player's list.
*
* @param char_data *ch The player.
* @param char *name The lastname to add to the list.
*/
void add_lastname(char_data *ch, char *name) {
	struct player_lastname *lastn;
	
	if (!ch || IS_NPC(ch) || !name || !*name) {
		return;	// no work
	}
	if (has_lastname(ch, name)) {
		return;	// already has it
	}
	
	CREATE(lastn, struct player_lastname, 1);
	lastn->name = str_dup(name);
	LL_PREPEND(GET_LASTNAME_LIST(ch), lastn);
	queue_delayed_update(ch, CDU_SAVE);
}


/**
* Changes a player's personal lastname. If it's also their current lastname,
* that is updated as well.
*
* @param char_data *ch The player.
* @param char *name The name to set it to (may be NULL).
*/
void change_personal_lastname(char_data *ch, char *name) {
	bool also_current = FALSE;
	
	if (!ch || IS_NPC(ch)) {
		return;	// no player no work
	}
	
	// free old name
	if (GET_PERSONAL_LASTNAME(ch)) {
		if (GET_CURRENT_LASTNAME(ch) && !str_cmp(GET_PERSONAL_LASTNAME(ch), GET_CURRENT_LASTNAME(ch))) {
			free(GET_CURRENT_LASTNAME(ch));
			also_current = TRUE;
		}
		free(GET_PERSONAL_LASTNAME(ch));
	}
	
	GET_PERSONAL_LASTNAME(ch) = name ? str_dup(name) : NULL;
	if (also_current) {
		GET_CURRENT_LASTNAME(ch) = name ? str_dup(name) : NULL;
	}
	
	queue_delayed_update(ch, CDU_SAVE);
	
	// update msdp
	update_MSDP_name(ch, UPDATE_SOON);
}


/**
* Check the player's lastname list for a name.
*
* @param char_data *ch A player.
* @param char *name The name to look for (not case sensitive).
* @return bool TRUE if the player has the name, FALSE if not.
*/
bool has_lastname(char_data *ch, char *name) {
	struct player_lastname *lastn;
	
	if (!ch || IS_NPC(ch) || !name) {
		return FALSE;	// safety first
	}
	
	LL_FOREACH(GET_LASTNAME_LIST(ch), lastn) {
		if (lastn->name && !str_cmp(lastn->name, name)) {
			return TRUE;
		}
	}
	
	return FALSE;	// if not
}


/**
* Removes a name from a player's lastname list, if present.
*
* @param char_data *ch A player.
* @param char *name The name to remove (not case sensitive).
*/
void remove_lastname(char_data *ch, char *name) {
	struct player_lastname *lastn, *next;
	
	if (!ch || IS_NPC(ch) || !name) {
		return;	// safety first
	}
	
	// shut off current lastname if it matches
	if (GET_CURRENT_LASTNAME(ch) && !str_cmp(GET_CURRENT_LASTNAME(ch), name)) {
		free(GET_CURRENT_LASTNAME(ch));
		GET_CURRENT_LASTNAME(ch) = NULL;
	}
	
	LL_FOREACH_SAFE(GET_LASTNAME_LIST(ch), lastn, next) {
		if (!lastn->name || !str_cmp(lastn->name, name)) {
			if (lastn->name) {
				free(lastn->name);
			}
			LL_DELETE(GET_LASTNAME_LIST(ch), lastn);
			free(lastn);
		}
	}
	
	queue_delayed_update(ch, CDU_SAVE);
}


 //////////////////////////////////////////////////////////////////////////////
//// loaded_player_hash FOR OFFLINE PLAYERS //////////////////////////////////

// for find_or_load_player(), tracks players that are kept in a 'loaded' state
// and are freed up automatically
struct loaded_player_data {
	int id;
	char_data *ch;
	UT_hash_handle hh;
};

struct loaded_player_data *loaded_player_hash = NULL;


/**
* Sets a player as 'loaded from file' in the loaded_player_hash, triggering an
* un-load up to 1 second later.
*
* @param char_data *ch The player to add to the loaded_player_hash.
*/
void add_loaded_player(char_data *ch) {
	struct loaded_player_data *lpd;
	int id;
	
	if (!IS_NPC(ch)) {
		id = GET_IDNUM(ch);
		HASH_FIND_INT(loaded_player_hash, &id, lpd);
		if (!lpd) {
			CREATE(lpd, struct loaded_player_data, 1);
			lpd->id = id;
			HASH_ADD_INT(loaded_player_hash, id, lpd);
		}
	
		if (lpd->ch && lpd->ch != ch) {
			log("SYSERR: add_loaded_player called when a different copy of the same player is already loaded");
		}
	
		lpd->ch = ch;
	}
}


/**
* This has the same purpose as get_player_vis_or_file, but won't screw anything
* up if the target is online but invisible. You can call SAVE_CHAR(ch) like
* normal. You should call store_loaded_char() if is_file == TRUE, or the player
* won't be stored. If you do NOT wish to save the character, you can use
* free_char() instead.
*
* If you do not free the character yourself, it will automatically be freed
* within 1 second. Leaving characters in this state can be advantageous if you
* expect several functions to load them in sequence.
*
* @param char *name The player name
* @param bool *is_file A place to store whether or not we loaded from file
* @return char_data *ch or NULL
*/
char_data *find_or_load_player(char *name, bool *is_file) {
	struct loaded_player_data *lpd;
	char buf[MAX_INPUT_LENGTH+2];
	player_index_data *index;
	char_data *ch = NULL;
	int id;
	
	*is_file = FALSE;
	
	if ((index = find_player_index_by_name(name))) {
		if (!(ch = is_playing(index->idnum))) {
			id = index->idnum;
			HASH_FIND_INT(loaded_player_hash, &id, lpd);
			if (lpd) {	// look in the loaded player hash first
				*is_file = TRUE;
				ch = lpd->ch;
			}
			else if ((ch = load_player(index->name, TRUE))) {
				SET_BIT(PLR_FLAGS(ch), PLR_KEEP_LAST_LOGIN_INFO);
				*is_file = TRUE;
				add_loaded_player(ch);
			}
		}
	}
	
	// not able to find -- look for a player partial match?
	if (!ch) {
		sprintf(buf, "0.%s", name);	// add 0. to force player match
		ch = get_player_world(buf, NULL);
		*is_file = FALSE;
		if (ch && IS_NPC(ch)) {
			ch = NULL;	// verify player only
		}
	}
	
	return ch;
}


/**
* Frees up players who have been left in a 'loaded' state by
* find_or_load_player().
*/
void free_loaded_players(void) {
	struct loaded_player_data *iter, *next;
	HASH_ITER(hh, loaded_player_hash, iter, next) {
		HASH_DEL(loaded_player_hash, iter);
		free_char(iter->ch);
		free(iter);
	}
}


/**
* Removes a player from the loaded_player_hash without freeing them. Use this
* if a previously-loaded player logs into the game, or similar.
*
* @param char_data *ch The player to remove from the loaded_player_hash.
*/
void remove_loaded_player(char_data *ch) {
	struct loaded_player_data *lpd;
	int id;
	
	if (!IS_NPC(ch)) {
		id = GET_IDNUM(ch);
		HASH_FIND_INT(loaded_player_hash, &id, lpd);
		if (lpd) {
			HASH_DEL(loaded_player_hash, lpd);
			free(lpd);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// AUTOWIZ WIZLIST GENERATOR ///////////////////////////////////////////////

/**
* This is adapted from the autowiz.c utility written by Jeremy Elson (All
* Right Reserved) and included with CircleMUD. In that code, written in the
* '90s, tools that iterate over the playerfile were run separate from the MUD
* itself because they were time-consuming and resource-intensive. This is no
* longer the case and it's much simpler to generate this list from within the
* mud itself. The following warning is included as-is:
*
*  WARNING:  THIS CODE IS A HACK.  WE CAN NOT AND WILL NOT BE RESPONSIBLE
*  FOR ANY NASUEA, DIZZINESS, VOMITING, OR SHORTNESS OF BREATH RESULTING
*  FROM READING THIS CODE.  PREGNANT WOMEN AND INDIVIDUALS WITH BACK
*  INJURIES, HEART CONDITIONS, OR ARE UNDER THE CARE OF A PHYSICIAN SHOULD
*  NOT READ THIS CODE.
*
*  -- The Management
*/

// Note: to set the headers of the wiz/godlists, use "config game wizlist_header", "config game godlist_header" in-game

const char *AUTOWIZ_IMM_LMARG = "   ";
const int AUTOWIZ_IMM_NSIZE = 16;
const int AUTOWIZ_LINE_LEN = 64;
const int AUTOWIZ_MIN_LEVEL = LVL_GOD;

// max level that should be in columns instead of centered
const int AUTOWIZ_COL_LEVEL = LVL_GOD;

// AUTOWIZ_MODE_x: which version is being generated
#define AUTOWIZ_MODE_WIZLIST  0
#define AUTOWIZ_MODE_GODLIST  1


struct autowiz_name_rec {
	char *name;
	struct autowiz_name_rec *next;	// linked list
};

struct autowiz_control_rec {
	int level;
	char *level_name;
};

struct autowiz_level_rec {
	struct autowiz_control_rec *params;
	struct autowiz_name_rec *names;
	struct autowiz_level_rec *next;	// linked list
};

struct autowiz_control_rec level_params[] = {
	{ LVL_GOD,	"Gods" },
#if (LVL_START_IMM < LVL_IMPL)
	{ LVL_START_IMM, "Immortal" },
#endif
#if (LVL_ASST < LVL_IMPL && LVL_ASST != LVL_START_IMM)
	{ LVL_ASST, "Assistant" },
#endif
#if (LVL_CIMPL < LVL_IMPL && LVL_CIMPL < LVL_CIMPL != LVL_ASST)
	{ LVL_CIMPL, "Co-Implementor" },
#endif
	{ LVL_IMPL,	"Implementors" },
	{0, ""}
};

struct autowiz_level_rec *autowiz_data = NULL;


/**
* Sets up the autowiz_data, which is shared by these functions.
*/
void autowiz_initialize(void) {
	struct autowiz_level_rec *tmp;
	int i = 0;
	
	while (level_params[i].level > 0) {
		CREATE(tmp, struct autowiz_level_rec, 1);
		tmp->params = &(level_params[i++]);
		LL_PREPEND(autowiz_data, tmp);
	}
}


/**
* Adds a name to autowiz_data, in the correct level.
*
* @param int level The level of the player.
* @param char *name The player's name.
*/
void autowiz_add_name(int level, char *name) {
	struct autowiz_name_rec *tmp;
	struct autowiz_level_rec *curr_level;

	if (!*name)
		return;
	
	/* this used to require all-alpha names bu that shouldn't be necessary
	for (ptr = name; *ptr; ptr++)
		if (!isalpha(*ptr))
			return;
	*/
	
	CREATE(tmp, struct autowiz_name_rec, 1);
	tmp->name = str_dup(name);
	
	curr_level = autowiz_data;
	while (curr_level->params->level > level) {
		curr_level = curr_level->next;
	}
	
	LL_APPEND(curr_level->names, tmp);
}


/**
* Frees memory allocated by the autowiz system.
*/
void autowiz_cleanup(void) {
	struct autowiz_level_rec *lev;
	struct autowiz_name_rec *name;
	
	while ((lev = autowiz_data)) {
		autowiz_data = lev->next;
		
		// do not free lev->params -- it's not allocated
		while ((name = lev->names)) {
			lev->names = name->next;
			
			if (name->name) {
				free(name->name);
			}
			free(name);
		}
		
		free(lev);
	}
	autowiz_data = NULL;
}


/**
* Iterates over the player index and adds anyone of the correct level to
* the autowiz_data.
*/
void autowiz_read_players(void) {
	player_index_data *index, *next_index;
	account_data *acct;
	
	HASH_ITER(name_hh, player_table_by_name, index, next_index) {
		if (IS_SET(index->plr_flags, PLR_NOWIZLIST)) {
			continue;
		}
		if ((acct = find_account(index->account_id)) && IS_SET(acct->flags, ACCT_FROZEN)) {
			continue;
		}
		if (index->access_level < AUTOWIZ_MIN_LEVEL) {
			continue;
		}
		
		// valid
		autowiz_add_name(index->access_level, index->fullname);
	}
}


/**
* Writes a wizlist (or godlist) file.
*
* @param int mode AUTOWIZ_MODE_WIZLIST or AUTOWIZ_MODE_GODLIST
* @param FILE *out The file open for writing.
* @param int minlev Minimum level to write to this file.
* @param int maxlev Maximum level to write to this file.
*/
void autowiz_write_wizlist(int mode, FILE *out, int minlev, int maxlev) {
	char buf[MAX_STRING_LENGTH];
	const char *header;
	struct autowiz_level_rec *curr_level;
	struct autowiz_name_rec *curr_name;
	int i, j;
	
	if ((header = config_get_string(mode == AUTOWIZ_MODE_GODLIST ? "godlist_header" : "wizlist_header"))) {
		fprintf(out, "%s\n", header);
	}
	
	for (curr_level = autowiz_data; curr_level; curr_level = curr_level->next) {
		if (curr_level->params->level < minlev || curr_level->params->level > maxlev) {
			continue;
		}
		
		i = 39 - (strlen(curr_level->params->level_name) / 2);
		for (j = 1; j <= i; ++j)
			fputc(' ', out);
		fprintf(out, "%s\n", curr_level->params->level_name);
		for (j = 1; j <= i; ++j)
			fputc(' ', out);
		for (j = 1; j <= strlen(curr_level->params->level_name); ++j)
			fputc('~', out);
		fprintf(out, "\n");

		strcpy(buf, "");
		curr_name = curr_level->names;
		while (curr_name) {
			strcat(buf, curr_name->name);
			if (strlen(buf) > AUTOWIZ_LINE_LEN) {
				if (curr_level->params->level <= AUTOWIZ_COL_LEVEL)
					fprintf(out, "%s", AUTOWIZ_IMM_LMARG);
				else {
					i = 40 - (strlen(buf) / 2);
					for (j = 1; j <= i; ++j)
						fputc(' ', out);
				}
				fprintf(out, "%s\n", buf);
				strcpy(buf, "");
			}
			else {
				if (curr_level->params->level <= AUTOWIZ_COL_LEVEL) {
					for (j = 1; j <= (AUTOWIZ_IMM_NSIZE - strlen(curr_name->name));++ j)
						strcat(buf, " ");
				}
				if (curr_level->params->level > AUTOWIZ_COL_LEVEL)
					strcat(buf, "   ");
			}
			curr_name = curr_name->next;
		}

		if (*buf) {
			if (curr_level->params->level <= AUTOWIZ_COL_LEVEL)
				fprintf(out, "%s%s\n", AUTOWIZ_IMM_LMARG, buf);
			else {
				i = 40 - (strlen(buf) / 2);
				for (j = 1; j <= i; ++j)
					fputc(' ', out);
				fprintf(out, "%s\n", buf);
			}
		}
		fprintf(out, "\n");
	}
}


/**
* Reloads the wizlist and godlist files.
*/
void reload_wizlists(void) {
	reload_text_string(TEXT_FILE_WIZLIST);
	reload_text_string(TEXT_FILE_GODLIST);
}


/**
* Builds and generates the wizlist and godlist files, and reloads them.
*/
void run_autowiz(void) {
	char basename[256], tempname[256];
	FILE *fl;
	
	syslog(SYS_INFO, LVL_START_IMM, TRUE, "Rebuilding wizlists");
	
	autowiz_initialize();
	autowiz_read_players();
	
	// wizlist
	strcpy(basename, text_file_data[TEXT_FILE_WIZLIST].filename);
	strcpy(tempname, basename);
	strcat(tempname, TEMP_SUFFIX);
	
	if (!(fl = fopen(tempname, "w"))) {
		log("SYSERR: run_autowiz: Unable to open file %s for writing\r\n", tempname);
		return;
	}
	autowiz_write_wizlist(AUTOWIZ_MODE_WIZLIST, fl, LVL_START_IMM, LVL_TOP);
	fclose(fl);
	rename(tempname, basename);
	
	// godlist
	strcpy(basename, text_file_data[TEXT_FILE_GODLIST].filename);
	strcpy(tempname, basename);
	strcat(tempname, TEMP_SUFFIX);
	
	if (!(fl = fopen(tempname, "w"))) {
		log("SYSERR: run_autowiz: Unable to open file %s for writing\r\n", tempname);
		return;
	}
	autowiz_write_wizlist(AUTOWIZ_MODE_GODLIST, fl, LVL_GOD, LVL_START_IMM - 1);
	fclose(fl);
	rename(tempname, basename);
	
	autowiz_cleanup();
	reload_wizlists();
}


/**
* This checks if a player is high enough level, and generates the wizlist. It
* also checks the use_autowiz config. This will re-write the godlist and
* wizlist files and commands.
*
* @param char_data *ch The player to check. Only runs autowiz if the player is of a certain level.
*/
void check_autowiz(char_data *ch) {
	if (GET_ACCESS_LEVEL(ch) >= LVL_GOD && config_get_bool("use_autowiz")) {
		run_autowiz();
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Logs the player's login and announces it to the room.
*
* @param char_data *ch The person logging in.
*/
void announce_login(char_data *ch) {
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
	descriptor_data *desc;
	int iter;
	
	syslog(SYS_LOGIN, GET_INVIS_LEV(ch), TRUE, "%s has entered the game at %s", GET_NAME(ch), IN_ROOM(ch) ? room_log_identifier(IN_ROOM(ch)) : "an unknown location");
	
	// auto-notes
	if (GET_ACCOUNT(ch) && GET_ACCOUNT(ch)->notes) {
		LL_FOREACH(descriptor_list, desc) {
			if (STATE(desc) != CON_PLAYING || !desc->character) {
				continue;
			}
			if (!IS_IMMORTAL(desc->character) || !PRF_FLAGGED(desc->character, PRF_AUTONOTES)) {
				continue;
			}
			if (GET_ACCESS_LEVEL(desc->character) < get_highest_access_level(GET_ACCOUNT(ch))) {
				continue;
			}
			
			// header divider
			snprintf(buf, sizeof(buf), "(%s account notes)", GET_NAME(ch));
			msg_to_char(desc->character, "- %s ", buf);
			
			// rest of the divider
			for (iter = 0; iter < 79 - (3 + strlen(buf)); ++iter) {
				buf2[iter] = '-';
			}
			buf2[iter] = '\0';	// terminate
			msg_to_char(desc->character, "%s\r\n%s", buf2, GET_ACCOUNT(ch)->notes);
		}
	}
	
	// mortlog
	if (GET_INVIS_LEV(ch) == 0 && !PLR_FLAGGED(ch, PLR_NEEDS_NEWBIE_SETUP)) {
		if (config_get_bool("public_logins")) {
			mortlog("%s has entered the game", PERS(ch, ch, TRUE));
		}
		else {
			mortlog_friends(ch, "%s has entered the game", PERS(ch, ch, TRUE));
			if (GET_LOYALTY(ch)) {
				log_to_empire(GET_LOYALTY(ch), ELOG_LOGINS, "%s has entered the game", PERS(ch, ch, TRUE));
			}
		}
	}
}


/**
* Checks if a player needs their traits reset, and does it if so. This will
* display a message to the player informing them of what ws reset.
*
* @param char_data *ch The player to check (and possibly reset).
* @return bool TRUE if it reset the traits and showed a message; FALSE if not.
*/
bool check_bonus_trait_reset(char_data *ch) {
	char buf[MAX_STRING_LENGTH];
	int iter, hours;
	struct time_info_data t;
	
	if (IS_NPC(ch) || !ch->desc) {
		return FALSE;
	}
	
	// compute playtime
	t = *real_time_passed((time(0) - ch->player.time.logon) + ch->player.time.played, 0);
	hours = t.day * 24 + t.hours;
	
	if (PLR_FLAGGED(ch, PLR_TRAITS_RESET)) {
		return FALSE;	// already reset
	}
	if (!config_get_int("hours_to_bonus_trait_reset") || hours < config_get_int("hours_to_bonus_trait_reset")) {
		return FALSE;	// no need to reset
	}
	
	// OK: show explainer
	msg_to_char(ch, "%d-hour Bonus Trait Reset:\r\n%s", config_get_int("hours_to_bonus_trait_reset"), config_get_string("bonus_trait_reset_message"));
	
	// OK: show old traits
	if (GET_BONUS_TRAITS(ch)) {
		prettier_sprintbit(GET_BONUS_TRAITS(ch), bonus_bit_descriptions, buf);
		msg_to_char(ch, "Your previous bonus trait%s: %s\r\n", count_bits(GET_BONUS_TRAITS(ch)) == 1 ? " was" : "s were", buf);
	}
	
	// reset all traits
	for (iter = 0; iter < NUM_BONUS_TRAITS; ++iter) {
		if (HAS_BONUS_TRAIT(ch, BIT(iter))) {
			apply_bonus_trait(ch, BIT(iter), FALSE);
		}
	}
	GET_BONUS_TRAITS(ch) = NOBITS;
	
	// mark as having reset them
	SET_BIT(PLR_FLAGS(ch), PLR_TRAITS_RESET);
	return TRUE;
}


/**
* Checks that all a player's learned crafts are valid.
*
* @param char_data *ch The player to check.
*/
void check_learned_crafts(char_data *ch) {
	struct player_craft_data *pcd, *next_pcd;
	craft_data *craft;
	
	if (IS_NPC(ch)) {
		return;
	}

	HASH_ITER(hh, GET_LEARNED_CRAFTS(ch), pcd, next_pcd) {
		if (!(craft = craft_proto(pcd->vnum)) || !CRAFT_FLAGGED(craft, CRAFT_LEARNED)) {
			remove_learned_craft(ch, pcd->vnum);
		}
	}
}


/**
* Checks that all empires' learned crafts are valid, and removes bad entries.
*/
void check_learned_empire_crafts(void) {
	struct player_craft_data *pcd, *next_pcd;
	empire_data *emp, *next_emp;
	craft_data *craft;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		HASH_ITER(hh, EMPIRE_LEARNED_CRAFTS(emp), pcd, next_pcd) {
			if (!(craft = craft_proto(pcd->vnum)) || !CRAFT_FLAGGED(craft, CRAFT_LEARNED)) {
				remove_learned_craft_empire(emp, pcd->vnum, TRUE);
			}
		}
	}
}


/**
* Checks that all a player's currencies are valid.
*
* @param char_data *ch The player to check.
*/
void check_currencies(char_data *ch) {
	struct player_currency *cur, *next_cur;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	HASH_ITER(hh, GET_CURRENCIES(ch), cur, next_cur) {
		if (!find_generic(cur->vnum, GENERIC_CURRENCY)) {
			HASH_DEL(GET_CURRENCIES(ch), cur);
			free(cur);
		}
	}
}


/**
* Ensures that all of a player's skills and abilities exist, and updates their
* class. This should be called on login.
*/
void check_skills_and_abilities(char_data *ch) {
	struct player_ability_data *plab, *next_plab;
	struct player_skill_data *plsk, *next_plsk;
	
	if (!ch || IS_NPC(ch)) {
		return;
	}
	
	HASH_ITER(hh, GET_ABILITY_HASH(ch), plab, next_plab) {
		if (!plab->ptr) {
			HASH_DEL(GET_ABILITY_HASH(ch), plab);
			free(plab);
		}
	}
	HASH_ITER(hh, GET_SKILL_HASH(ch), plsk, next_plsk) {
		if (!plsk->ptr) {
			HASH_DEL(GET_SKILL_HASH(ch), plsk);
			free(plsk);
			continue;
		}
		
		// check skill level cap
		if (plsk->level > SKILL_MAX_LEVEL(plsk->ptr)) {
			plsk->level = SKILL_MAX_LEVEL(plsk->ptr);
		}
	}
	update_class_and_abilities(ch);
	check_ability_levels(ch, NOTHING);
}


/**
* Deletes chat messages that are more than 24 hours old.
*
* @param char_data *ch The person whose history to clean.
*/
void clean_old_history(char_data *ch) {
	struct channel_history_data *hist, *next_hist;
	long now = time(0);
	int iter;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	for (iter = 0; iter < NUM_CHANNEL_HISTORY_TYPES; ++iter) {
		DL_FOREACH_SAFE(GET_HISTORY(ch, iter), hist, next_hist) {
			if (hist->timestamp + (60 * 60 * 24) < now) {
				// 24 hours old
				if (hist->message) {
					free(hist->message);
				}
				DL_DELETE(GET_HISTORY(ch, iter), hist);
				free(hist);
			}
		}
	}
}


/**
* Clears certain player data, similar to clear_char() -- but not for NPCS.
*
* @param char_data *ch The player charater to clear.
*/
void clear_player(char_data *ch) {
	if (!ch) {
		return;
	}
	
	ch->player.time.birth = time(0);
	ch->player.time.played = 0;
	ch->player.time.logon = time(0);
	
	
	// some nowheres/nothings
	GET_LOADROOM(ch) = NOWHERE;
	GET_MARK_LOCATION(ch) = NOWHERE;
	GET_MOUNT_VNUM(ch) = NOTHING;
	GET_PLEDGE(ch) = NOTHING;
	GET_TOMB_ROOM(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_RETURN_MAP(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_INSTANCE_ID(ch) = NOTHING;
	GET_LAST_TELL(ch) = NOBODY;
	GET_TEMPORARY_ACCOUNT_ID(ch) = NOTHING;
	GET_IMMORTAL_LEVEL(ch) = -1;	// Not an immortal
	GET_LAST_VEHICLE(ch) = NOTHING;
	GET_SPEAKING(ch) = NOTHING;
	GET_ACTION_ROOM_TARG(ch) = NOWHERE;
}


/**
* Updates the affect timers on players when they first log into the game, and
* schedules the expiration events. This is also safe to call on offline players
* if you need the affect timers to be accurate, e.g. in "stat file".
*
* @param char_data *ch The player.
*/
void convert_and_schedule_player_affects(char_data *ch) {
	struct affected_type *af;
	struct over_time_effect_type *dot;
	
	// convert timers first
	if (!IS_NPC(ch) && !AFFECTS_CONVERTED(ch)) {
		AFFECTS_CONVERTED(ch) = TRUE;
		LL_FOREACH(ch->affected, af) {
			if (af->expire_time != UNLIMITED) {
				// convert from seconds
				af->expire_time += time(0);
			}
		}
	}
	
	// schedule them even if already converted
	LL_FOREACH(ch->affected, af) {
		// schedule it
		schedule_affect_expire(ch, af);
	}
	LL_FOREACH(ch->over_time_effects, dot) {
		schedule_dot_update(ch, dot);
	}
}


/**
* This runs at startup (if you don't use -q) and deletes players who are
* timed out according to the delete_invalid_players_after and 
* delete_inactive_players_after configs. This can be prevented with the
* NODELETE flag. Immortals are also never deleted this way. Only whole accounts
* will be deleted, never individual characters on accounts.
*/
void delete_old_players(void) {
	account_data *acct, *next_acct;
	bool has_imm, has_nodelete, will_delete, file;
	char reason[256];
	char_data *ch;
	int avg_min_per_day, inactive_days;
	int max_access_level, max_char_level, best_mins_per_day;
	struct account_player *plr, *next_plr;
	
	int delete_abandoned = config_get_int("delete_abandoned_players_after");
	int delete_invalid = config_get_int("delete_invalid_players_after");
	int delete_inactive = config_get_int("delete_inactive_players_after");
	
	HASH_ITER(hh, account_table, acct, next_acct) {
		max_char_level = max_access_level = best_mins_per_day = 0;
		has_imm = has_nodelete = will_delete = FALSE;
		*reason = '\0';
		
		// scan players in the account
		LL_FOREACH(acct->players, plr) {
			if (plr->player->access_level > LVL_MORTAL) {
				has_imm = TRUE;
			}
			if (IS_SET(plr->player->plr_flags, PLR_NODELETE)) {
				has_nodelete = TRUE;
			}
			max_access_level = MAX(max_access_level, plr->player->access_level);
			max_char_level = MAX(max_char_level, plr->player->highest_known_level);
			
			// determine avg playtime
			avg_min_per_day = (((double) plr->player->played / SECS_PER_REAL_HOUR) / ((double)(time(0) - plr->player->birth) / SECS_PER_REAL_DAY)) * SECS_PER_REAL_MIN;
			best_mins_per_day = MAX(best_mins_per_day, avg_min_per_day);
		}
		
		// never delete imms or anyone with nodelete on the account
		if (has_imm || has_nodelete) {
			continue;
		}
		
		// how long they've been gone
		inactive_days = (time(0) - acct->last_logon) / SECS_PER_REAL_DAY;
		
		// reasons to delete
		if (delete_invalid > 0 && (max_access_level <= 0 || max_char_level <= 0) && inactive_days >= delete_invalid) {
			will_delete = TRUE;
			snprintf(reason, sizeof(reason), "no characters over level 0");
		}
		else if (delete_inactive > 0 && inactive_days >= delete_inactive) {
			will_delete = TRUE;
			snprintf(reason, sizeof(reason), "inactive too long");
		}
		else if (delete_abandoned > 0 && max_char_level < 100 && best_mins_per_day <= 3 && inactive_days >= delete_abandoned) {
			will_delete = TRUE;	// low-level and long-gone
			snprintf(reason, sizeof(reason), "abandoned low-level account");
		}
		
		if (will_delete) {
			log("DEL: Deleting account %d: %s", acct->id, reason);
			LL_FOREACH_SAFE(acct->players, plr, next_plr) {
				log("DEL: - [%d] %s", plr->player->idnum, plr->player->fullname);
				
				// ensure not at the menu somehow
				if ((ch = is_at_menu(plr->player->idnum)) && ch->desc) {
					close_socket(ch->desc);
				}
				
				// now find/load and delete
				if ((ch = find_or_load_player(plr->player->name, &file))) {
					if (file) {
						delete_player_character(ch);
						free_char(ch);
					}
					else {	// in-game somehow
						extract_all_items(ch);
						delete_player_character(ch);
						extract_char(ch);
					}
				}
			}
		}	// end will_delete: if last player was deleted, the account is already gone, too
	}
}


/**
* Function to DELETE A PLAYER.
*
* @param char_data *ch The player to delete.
*/
void delete_player_character(char_data *ch) {
	player_index_data *index;
	empire_data *emp = NULL;
	char filename[256], delname[256];
	
	if (IS_NPC(ch)) {
		syslog(SYS_ERROR, 0, TRUE, "SYSERR: delete_player_character called on NPC");
		return;
	}
	
	clear_private_owner(GET_IDNUM(ch));
	purge_bound_items(GET_IDNUM(ch));
	delete_player_from_running_events(ch);

	// Check the empire
	if ((emp = GET_LOYALTY(ch)) != NULL) {
		log_to_empire(emp, ELOG_MEMBERS, "%s has left the empire", PERS(ch, ch, TRUE));
		GET_LOYALTY(ch) = NULL;
		GET_RANK(ch) = 0;
		delete_member_data(ch, emp);
	}
	
	// remove account and player index
	if (GET_ACCOUNT(ch)) {
		remove_player_from_account(ch);
	}
	if ((index = find_player_index_by_idnum(GET_IDNUM(ch)))) {
		remove_player_from_table(index);
		free_player_index_data(index);
	}
	
	// various file deletes
	if (get_filename(GET_NAME(ch), filename, PLR_FILE) && get_filename(GET_NAME(ch), delname, DELETED_PLR_FILE)) {
		if (rename(filename, delname) < 0 && errno != ENOENT) {
			log("SYSERR: moving deleted player file %s: %s", filename, strerror(errno));
		}
	}
	if (get_filename(GET_NAME(ch), filename, DELAYED_FILE) && get_filename(GET_NAME(ch), delname, DELETED_DELAYED_FILE)) {
		if (rename(filename, delname) < 0 && errno != ENOENT) {
			log("SYSERR: moving deleted player delay file %s: %s", filename, strerror(errno));
		}
	}
	if (get_filename(GET_NAME(ch), filename, MAP_MEMORY_FILE) && access(filename, F_OK) == 0) {
		// just delete map memory, if present
		unlink(filename);
	}
	
	// cleanup
	if (emp) {
		read_empire_members(emp, FALSE);
	}
}


/**
* Does various checks and puts the player into the game. Both return codes are
* successful results.
*
* @param descriptor_data *d The descriptor for the player.
* @param int dolog Whether or not to log the login.
* @param bool fresh If FALSE, player was already in the game, not logging in fresh.
*/
void enter_player_game(descriptor_data *d, int dolog, bool fresh) {
	struct slash_channel *load_slash, *next_slash;
	bool stop_action = FALSE, try_home = FALSE;
	room_data *load_room = NULL;
	char_data *ch = d->character, *repl;
	struct companion_data *compan;
	char lbuf[MAX_STRING_LENGTH];
	struct affected_type *af;
	player_index_data *index;
	vehicle_data *veh;
	empire_data *emp;
	int iter, duration, top_idnum;
	
	pause_affect_total = TRUE;	// prevent unnecessary totaling

	reset_char(ch);
	check_delayed_load(ch);	// ensure everything is loaded
	clean_old_history(ch);
	reset_combat_meters(ch);
	GET_COMBAT_METERS(ch).over = TRUE;	// ensure no active meter
	remove_loaded_player(ch);	// in case they were recently loaded
	
	// remove this now
	REMOVE_BIT(PLR_FLAGS(ch), PLR_KEEP_LAST_LOGIN_INFO);
	
	// ensure they have this
	if (!GET_CREATION_HOST(ch) || !*GET_CREATION_HOST(ch)) {
		if (GET_CREATION_HOST(ch)) {
			free(GET_CREATION_HOST(ch));
		}
		GET_CREATION_HOST(ch) = str_dup(d->host);
	}
	
	// ensure the player has an idnum and is in the index
	if (GET_IDNUM(ch) <= 0) {
		top_idnum = data_get_int(DATA_TOP_IDNUM) + 1;
		data_set_int(DATA_TOP_IDNUM, top_idnum);
		GET_IDNUM(ch) = top_idnum;
		
		ch->script_id = GET_IDNUM(ch);
	}
	if (!(index = find_player_index_by_idnum(GET_IDNUM(ch)))) {
		CREATE(index, player_index_data, 1);
		update_player_index(index, ch);
		add_player_to_table(index);
	}
	
	if (GET_IMMORTAL_LEVEL(ch) > -1) {
		GET_ACCESS_LEVEL(ch) = LVL_TOP - GET_IMMORTAL_LEVEL(ch);
		GET_ACCESS_LEVEL(ch) = MAX(GET_ACCESS_LEVEL(ch), LVL_GOD);
	}
	else {	// not an imm/god
		GET_ACCESS_LEVEL(ch) = MIN(GET_ACCESS_LEVEL(ch), LVL_MORTAL);
	}
	
	// set (and correct) invis level
	if (PLR_FLAGGED(ch, PLR_INVSTART)) {
		GET_INVIS_LEV(ch) = GET_ACCESS_LEVEL(ch);
	}
	GET_INVIS_LEV(ch) = MIN(GET_INVIS_LEV(ch), GET_ACCESS_LEVEL(ch));

	/*
	 * We have to place the character in a room before equipping them
	 * or equip_char() will gripe about the person in no-where.
	 */
	if (GET_LOADROOM(ch) != NOWHERE) {
		load_room = real_room(GET_LOADROOM(ch));
		
		// this verifies they are still in the same map location as where they logged out
		if (load_room && !PLR_FLAGGED(ch, PLR_LOADROOM)) {
			if (GET_LOAD_ROOM_CHECK(ch) == NOWHERE || !GET_MAP_LOC(load_room) || GET_MAP_LOC(load_room)->vnum != GET_LOAD_ROOM_CHECK(ch)) {
				// ensure they are on the same continent they used to be when it finds them a new loadroom
				GET_LAST_ROOM(ch) = GET_LOAD_ROOM_CHECK(ch);
				load_room = NULL;
				try_home = TRUE;
			}
		}
	}
	
	// cancel detected loadroom?
	if (load_room && RESTORE_ON_LOGIN(ch) && PRF_FLAGGED(ch, PRF_AUTORECALL)) {
		load_room = NULL;
		try_home = TRUE;
	}
		
	// long logout and in somewhere hostile
	if (load_room && RESTORE_ON_LOGIN(ch) && ROOM_OWNER(load_room) && ROOM_OWNER(load_room) != GET_LOYALTY(ch) && (IS_HOSTILE(ch) || empire_is_hostile(ROOM_OWNER(load_room), GET_LOYALTY(ch), load_room))) {
		load_room = NULL;	// re-detect
		try_home = TRUE;
	}
	
	// load room is in a vehicle that's no longer complete? dump outside the vehicle
	if (load_room && GET_ROOM_VEHICLE(load_room) && (!VEH_IS_COMPLETE(GET_ROOM_VEHICLE(load_room)) || VEH_IS_DISMANTLING(GET_ROOM_VEHICLE(load_room)))) {
		load_room = IN_ROOM(GET_ROOM_VEHICLE(load_room));
	}
	
	// load room in a building that's no longer complete? send home
	if (load_room && HOME_ROOM(load_room) != load_room && !IS_COMPLETE(HOME_ROOM(load_room))) {
		load_room = NULL;
		try_home = TRUE;
	}
	
	// on request, try to send them home
	if (try_home) {
		load_room = find_home(ch);
		stop_action = TRUE;
	}

	// nowhere found? must detect load room
	if (!load_room) {
		load_room = find_load_room(d->character);
		stop_action = TRUE;
	}

	// absolute failsafe
	if (!load_room) {
		load_room = real_room(0);
		stop_action = TRUE;
	}


	/* fail-safe */
	if (IS_VAMPIRE(ch) && GET_APPARENT_AGE(ch) <= 0)
		GET_APPARENT_AGE(ch) = 25;

	// add to lists
	DL_PREPEND(character_list, ch);
	DL_PREPEND2(player_character_list, ch, prev_plr, next_plr);
	ch->script_id = GET_IDNUM(ch);	// if not already set
	if (!ch->in_lookup_table) {
		add_to_lookup_table(ch->script_id, (void *)ch, TYPE_MOB);
		ch->in_lookup_table = TRUE;
	}
	
	// place character
	char_to_room(ch, load_room);
	qt_visit_room(ch, IN_ROOM(ch));
	ch->prev_logon = ch->player.time.logon;	// and update prev_logon now
	
	// verify morph stats
	if (!IS_MORPHED(ch)) {
		affect_from_char(ch, ATYPE_MORPH, FALSE);
	}
	
	if (dolog) {
		announce_login(ch);
	}
	
	affect_total(ch);
	
	if (stop_action) {
		cancel_action(ch);
	}
		
	// verify skills, abilities, and class and skill/gear levels are up-to-date
	check_skills_and_abilities(ch);
	determine_gear_level(ch);
	add_all_gain_hooks(ch);
	
	queue_delayed_update(ch, CDU_SAVE);

	// re-join slash-channels
	global_mute_slash_channel_joins = TRUE;
	for (load_slash = LOAD_SLASH_CHANNELS(ch); load_slash; load_slash = next_slash) {
		next_slash = load_slash->next;
		if (load_slash->name && *load_slash->name && str_cmp(load_slash->name, "index")) {
			// for safety reasons, ignores channels named "index" (could never write this name to file)
			sprintf(lbuf, "join %s", load_slash->name);
			do_slash_channel(ch, lbuf, 0, 0);
		}
		
		LL_DELETE(LOAD_SLASH_CHANNELS(ch), load_slash);
		if (load_slash->name) {
			free(load_slash->name);
		}
		if (load_slash->lc_name) {
			free(load_slash->lc_name);
		}
		free(load_slash);
	}
	global_mute_slash_channel_joins = FALSE;
	
	// in some cases, we need to re-read tech when the character logs in
	if ((emp = GET_LOYALTY(ch))) {
		if (REREAD_EMPIRE_TECH_ON_LOGIN(ch)) {
			SAVE_CHAR(ch);
			reread_empire_tech(emp);
			REREAD_EMPIRE_TECH_ON_LOGIN(ch) = FALSE;
		}
		else {
			// reread members to adjust greatness: trigger a delayed refresh because multiple things may trigger it at once
			TRIGGER_DELAYED_REFRESH(emp, DELAY_REFRESH_MEMBERS);
		}
	}
	
	// remove stale coins
	cleanup_coins(ch);
	
	// verify abils}
	assign_class_and_extra_abilities(ch, NULL, NOTHING);
	give_level_zero_abilities(ch);

	// script/trigger stuff
	pre_greet_mtrigger(ch, IN_ROOM(ch), NO_DIR, "login");	// cannot pre-greet for this
	enter_wtrigger(IN_ROOM(ch), ch, NO_DIR, "login");
	greet_mtrigger(ch, NO_DIR, "login");
	greet_memory_mtrigger(ch);
	greet_vtrigger(ch, NO_DIR, "login");
	
	// update the index in case any of this changed
	index = find_player_index_by_idnum(GET_IDNUM(ch));
	update_player_index(index, ch);
	
	// ensure data is up-to-date
	apply_all_ability_techs(ch);
	refresh_all_quests(ch);
	check_learned_crafts(ch);
	check_currencies(ch);
	check_eq_sets(ch);
	check_friends(ch);
	check_languages(ch);
	check_minipets_and_companions(ch);
	check_player_events(ch);
	check_unlocked_archetypes(ch);
	refresh_passive_buffs(ch);
	convert_and_schedule_player_affects(ch);
	schedule_all_obj_timers(ch);
	RESET_LAST_MESSAGED_TEMPERATURE(ch);
	
	// break last reply if invis
	if (GET_LAST_TELL(ch) && (repl = is_playing(GET_LAST_TELL(ch))) && (GET_INVIS_LEV(repl) > GET_ACCESS_LEVEL(ch) || (!IS_IMMORTAL(ch) && PRF_FLAGGED(repl, PRF_INCOGNITO)))) {
		GET_LAST_TELL(ch) = NOBODY;
	}
	
	// check if a building was completed while gone, to avoid a strange message
	if (GET_ACTION(ch) == ACT_BUILDING || GET_ACTION(ch) == ACT_MAINTENANCE) {
		if (!BUILDING_RESOURCES(IN_ROOM(ch)) && BUILDING_DAMAGE(IN_ROOM(ch)) == 0) {
			cancel_action(ch);
		}
	}
	
	msdp_update_room(ch);
	
	// now is a good time to save and be sure we have a good save file
	queue_delayed_update(ch, CDU_SAVE);
	
	pause_affect_total = FALSE;
	affect_total(ch);
	
	// free reset?
	if (RESTORE_ON_LOGIN(ch)) {
		set_health(ch, GET_MAX_HEALTH(ch));
		set_move(ch, GET_MAX_MOVE(ch));
		set_mana(ch, GET_MAX_MANA(ch));
		GET_COND(ch, FULL) = MIN(0, GET_COND(ch, FULL));
		GET_COND(ch, THIRST) = MIN(0, GET_COND(ch, THIRST));
		GET_COND(ch, DRUNK) = MIN(0, GET_COND(ch, DRUNK));
		set_blood(ch, GET_MAX_BLOOD(ch));
		
		// clear deficits
		for (iter = 0; iter < NUM_POOLS; ++iter) {
			GET_DEFICIT(ch, iter) = 0;
		}
		
		// check for confusion!
		GET_CONFUSED_DIR(ch) = number(0, NUM_SIMPLE_DIRS-1);
		
		// clear this for long logout
		GET_RECENT_DEATH_COUNT(ch) = 0;
		affect_from_char(ch, ATYPE_DEATH_PENALTY, FALSE);
		
		RESTORE_ON_LOGIN(ch) = FALSE;
		clean_lore(ch);
		clean_player_kills(ch);
		reset_player_temperature(ch);
		affect_total(ch);	// again, in case things changed
	}
	else {
		// ensure not dead
		set_health(ch, MAX(1, GET_HEALTH(ch)));
		set_blood(ch, MAX(1, GET_BLOOD(ch)));
	}
	
	// position must be reset
	if (AFF_FLAGGED(ch, AFF_EARTHMELDED | AFF_MUMMIFIED | AFF_DEATHSHROUDED)) {
		GET_POS(ch) = POS_SLEEPING;
	}
	else {
		GET_POS(ch) = POS_STANDING;
	}
	
	// attempt to put them back in a vehicle
	if (GET_LAST_VEHICLE(ch) != NOTHING) {
		DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
			if (VEH_VNUM(veh) == GET_LAST_VEHICLE(ch) && VEH_IS_COMPLETE(veh) && !VEH_SITTING_ON(veh) && validate_sit_on_vehicle(ch, veh, POS_SITTING, FALSE)) {
				sit_on_vehicle(ch, veh);
				GET_POS(ch) = POS_SITTING;
				break;	// only need 1
			}
		}
	}
	
	// reload last companion?
	if (GET_LAST_COMPANION(ch) != NOTHING && (compan = has_companion(ch, GET_LAST_COMPANION(ch)))) {
		load_companion_mob(ch, compan);
	}
	// redetect targets afterwards in case the companion was the target
	redetect_ability_targets_on_login(ch);
	
	// ensure these are fresh
	if (GET_LOYALTY(ch)) {
		update_member_data(ch);
		update_empire_members_and_greatness(GET_LOYALTY(ch));
	}
	
	if (!IS_IMMORTAL(ch)) {
		// ensure player has penalty if at war
		if (fresh && GET_LOYALTY(ch) && is_at_war(GET_LOYALTY(ch)) && (duration = config_get_int("war_login_delay")) > 0) {
			af = create_flag_aff(ATYPE_WAR_DELAY, duration, AFF_IMMUNE_PHYSICAL | AFF_NO_ATTACK | AFF_HARD_STUNNED, ch);
			affect_join(ch, af, NOBITS);
			msg_to_char(ch, "\trYou are stunned for %d second%s because your empire is at war.\r\n", duration, PLURAL(duration));
		}
		else if (fresh && IN_HOSTILE_TERRITORY(ch) && (duration = config_get_int("hostile_login_delay")) > 0) {
			af = create_flag_aff(ATYPE_HOSTILE_DELAY, duration, AFF_IMMUNE_PHYSICAL | AFF_NO_ATTACK | AFF_HARD_STUNNED, ch);
			affect_join(ch, af, NOBITS);
			msg_to_char(ch, "\trYou are stunned for %d second%s because you logged in in hostile territory.\r\n", duration, PLURAL(duration));
		}
	}
	
	if (ch->desc) {
		send_initial_MSDP(ch->desc);
	}
}


/**
* Frees the data for 1 player_eq_set.
*
* @param struct player_eq_set *eq_set The eq set to free.
*/
void free_player_eq_set(struct player_eq_set *eq_set) {
	if (eq_set) {
		if (eq_set->name) {
			free(eq_set->name);
		}
		free(eq_set);
	}
}


/**
* @param account_data *acct Which account to check.
* @return int The highest access level of any character on that account.
*/
int get_highest_access_level(account_data *acct) {
	struct account_player *plr;
	int highest = 0;
	
	if (acct) {
		LL_FOREACH(acct->players, plr) {
			highest = MAX(highest, plr->player->access_level);
		}
	}
	
	return highest;
}


/**
* Gives a character a piece of newbie gear, if the slot isn't filled. Auto-
* mathematically cascades to the 2nd neck/finger/etc slot.
*
* @param char_data *ch The person to give the item to.
* @param obj_vnum vnum Which item to give.
* @param int pos Where to equip it, or NO_WEAR for inventory.
*/
void give_newbie_gear(char_data *ch, obj_vnum vnum, int pos) {
	obj_data *obj;
	
	if (!obj_proto(vnum)) {
		return;
	}
	
	// detect cascade
	while (pos != NO_WEAR && GET_EQ(ch, pos) && wear_data[pos].cascade_pos != NO_WEAR) {
		pos = wear_data[pos].cascade_pos;
	}
	
	if (pos != NO_WEAR && GET_EQ(ch, pos)) {
		return;
	}
	
	obj = read_object(vnum, TRUE);
	scale_item_to_level(obj, 1);	// lowest possible scale
	
	if (pos == NO_WEAR) {
		obj_to_char(obj, ch);
	}
	else {
		equip_char(ch, obj, pos);
	}
	
	// always bind immediately if BoE
	if (OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
		bind_obj_to_player(obj, ch);
	}
}


/**
* This runs one-time setup on the player character, during their initial
* creation.
*
* @param char_data *ch The player to initialize.
*/
void init_player(char_data *ch) {
	player_index_data *index;
	int account_id = NOTHING;
	account_data *acct;
	bool first = FALSE;
	int i, iter, top_idnum;

	// create a player_special structure, if needed
	if (ch->player_specials == NULL) {
		init_player_specials(ch);
	}
	
	// store temporary account id (may be overwritten by clear_player)
	if (GET_TEMPORARY_ACCOUNT_ID(ch) != NOTHING) {
		account_id = GET_TEMPORARY_ACCOUNT_ID(ch);
		GET_TEMPORARY_ACCOUNT_ID(ch) = NOTHING;
	}
	
	// some basic player inits
	clear_player(ch);

	/* *** if this is our first player --- he be God *** */
	if (HASH_CNT(idnum_hh, player_table_by_idnum) == 0) {
		first = TRUE;
		
		GET_ACCESS_LEVEL(ch) = LVL_TOP;
		GET_IMMORTAL_LEVEL(ch) = 0;			/* Initialize it to 0, meaning Implementor */

		ch->real_attributes[STRENGTH] = att_max(ch);
		ch->real_attributes[DEXTERITY] = att_max(ch);

		ch->real_attributes[CHARISMA] = att_max(ch);
		ch->real_attributes[GREATNESS] = att_max(ch);

		ch->real_attributes[INTELLIGENCE] = att_max(ch);
		ch->real_attributes[WITS] = att_max(ch);
		
		SET_BIT(PLR_FLAGS(ch), PLR_APPROVED);
		SET_BIT(PRF_FLAGS(ch), PRF_HOLYLIGHT | PRF_ROOMFLAGS | PRF_NOHASSLE);
		
		// turn on all syslogs
		for (iter = 0; *syslog_types[iter] != '\n'; ++iter) {
			SYSLOG_FLAGS(ch) |= BIT(iter);
		}
	}
	
	ch->points.max_pools[HEALTH] = base_player_pools[HEALTH];
	ch->points.current_pools[HEALTH] = GET_MAX_HEALTH(ch);
	
	ch->points.max_pools[MOVE] = base_player_pools[MOVE];
	ch->points.current_pools[MOVE] = GET_MAX_MOVE(ch);
	
	ch->points.max_pools[MANA] = base_player_pools[MANA];
	ch->points.current_pools[MANA] = GET_MAX_MANA(ch);
	
	set_title(ch, NULL);
	ch->player.short_descr = NULL;
	ch->player.long_descr = NULL;
	ch->player.look_descr = NULL;
	GET_PROMPT(ch) = NULL;
	GET_FIGHT_PROMPT(ch) = NULL;
	POOFIN(ch) = NULL;
	POOFOUT(ch) = NULL;
	
	ch->points.max_pools[BLOOD] = 10;	// not actually used for players
	ch->points.current_pools[BLOOD] = GET_MAX_BLOOD(ch);	// this is a function
	
	// assign idnum
	if (GET_IDNUM(ch) <= 0) {
		top_idnum = data_get_int(DATA_TOP_IDNUM) + 1;
		data_set_int(DATA_TOP_IDNUM, top_idnum);
		GET_IDNUM(ch) = top_idnum;
		
		ch->script_id = GET_IDNUM(ch);
	}
	// ensure in index
	if (!(index = find_player_index_by_idnum(GET_IDNUM(ch)))) {
		CREATE(index, player_index_data, 1);
		update_player_index(index, ch);
		add_player_to_table(index);
	}
	// assign account
	if (account_id != NOTHING || !GET_ACCOUNT(ch)) {
		if ((acct = find_account(account_id))) {
			if (GET_ACCOUNT(ch)) {
				remove_player_from_account(ch);
			}
			add_player_to_account(ch, acct);
		}
		else if (!GET_ACCOUNT(ch)) {
			create_account_for_player(ch);
		}
	}
	
	// hold-over from earlier; needed account
	if (first) {
		SET_BIT(GET_ACCOUNT(ch)->flags, ACCT_APPROVED);
		SAVE_ACCOUNT(GET_ACCOUNT(ch));
		update_player_index(index, ch);
	}
	
	ch->char_specials.affected_by = 0;
	GET_FIGHT_MESSAGES(ch) = DEFAULT_FIGHT_MESSAGES;
	GET_STATUS_MESSAGES(ch) = DEFAULT_STATUS_MESSAGES;

	for (i = 0; i < NUM_CONDS; i++) {
		GET_COND(ch, i) = (GET_ACCESS_LEVEL(ch) == LVL_IMPL ? UNLIMITED : 0);
	}
	
	// script allocation
	if (!SCRIPT(ch)) {
		create_script_data(ch, MOB_TRIGGER);
	}
}


/**
* When a character is deleted, also clean up items bound to only them.
*
* @param int idnum The idnum to clear bound objects for.
*/
void purge_bound_items(int idnum) {
	obj_data *obj;
	
	// this uses a global, purge_bound_items_next, to avoid problems where both obj and next-obj are purged from being nested (like a corpse)
	DL_FOREACH_SAFE(object_list, obj, purge_bound_items_next) {
		if (OBJ_BOUND_TO(obj) && OBJ_BOUND_TO(obj)->idnum == idnum && !OBJ_BOUND_TO(obj)->next) {
			// bound to exactly 1 idnum and it's this one
			if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
				act("$p vanishes with a wisp of smoke.", FALSE, NULL, obj, NULL, TO_ROOM);
			}
			
			extract_obj(obj);
		}
	}
}


/**
* clear some of the the working variables of a char.
*
* I'm not sure if most of this is really necessary anymore -pc
*
* @param char_data *ch The character to reset.
*/
void reset_char(char_data *ch) {	
	ch->followers = NULL;
	GET_LEADER(ch) = NULL;
	IN_ROOM(ch) = NULL;
	ch->next = NULL;
	ch->next_fighting = NULL;
	ch->next_in_room = ch->prev_in_room = NULL;
	FIGHTING(ch) = NULL;
	ch->char_specials.position = POS_STANDING;
	
	if (GET_MOVE(ch) <= 0) {
		GET_MOVE(ch) = 1;	// ok to set here: character is definitely not in-game
	}
}


/**
* This saves all connected players.
*
* @param bool delay if TRUE, queues them instead of saving instantly
*/
void save_all_players(bool delay) {
	descriptor_data *desc;
	
	if (block_all_saves_due_to_shutdown) {
		return;
	}

	for (desc = descriptor_list; desc; desc = desc->next) {
		if ((STATE(desc) == CON_PLAYING) && !IS_NPC(desc->character)) {
			if (delay) {
				queue_delayed_update(desc->character, CDU_SAVE);
			}
			else {
				SAVE_CHAR(desc->character);
			}
		}
	}
}


/**
* This handles title-setting to allow some characters (, - ; : ~) to appear
* in the title with no leading space.
*
* @param char_data *ch The player to set.
* @param char *title The title string to set.
*/
void set_title(char_data *ch, char *title) {
	char buf[MAX_STRING_LENGTH];
	
	if (IS_NPC(ch)) {
		return;
	}

	if (title == NULL)
		title = "the newbie";
	else {
		title = trim(title);
	}

	if (GET_TITLE(ch) != NULL)
		free(GET_TITLE(ch));

	if (*title && *title != ':' && *title != ',' && *title != '-' && *title != ';' && *title != '~')
		sprintf(buf, " %s", title);
	else
		strcpy(buf, title);

	if (strlen(title) > MAX_TITLE_LENGTH) {
		buf[MAX_TITLE_LENGTH] = '\0';
	}

	GET_TITLE(ch) = str_dup(buf);
}


// for start_new_character
GLB_FUNCTION(run_global_newbie_gear) {
	struct archetype_gear *gear;
	bool any = FALSE;
	
	LL_FOREACH(GET_GLOBAL_GEAR(glb), gear) {
		give_newbie_gear(ch, gear->vnum, gear->wear);
		any = TRUE;
	}
	
	return any;
}


/**
* Some initializations for characters, including initial skills
*
* @param char_data *ch A new player
*/
void start_new_character(char_data *ch) {
	char lbuf[MAX_INPUT_LENGTH];
	int arch_iter, iter, level;
	struct archetype_gear *gear;
	struct archetype_skill *sk;
	skill_data *skill_data;
	archetype_data *arch;
	
	// announce to existing players that we have a newbie
	mortlog("%s has joined the game", PERS(ch, ch, TRUE));

	set_title(ch, NULL);

	/* Default Flags */
	if (GET_ACCOUNT(ch) && config_get_bool("siteok_everyone")) {
		SET_BIT(GET_ACCOUNT(ch)->flags, ACCT_SITEOK);
	}
	
	// store host if possible
	if (GET_CREATION_HOST(ch)) {
		free(GET_CREATION_HOST(ch));
	}
	GET_CREATION_HOST(ch) = ch->desc ? str_dup(ch->desc->host) : NULL;
	
	// access level: set to LVL_MORTAL
	if (GET_ACCESS_LEVEL(ch) < LVL_MORTAL) {
		GET_ACCESS_LEVEL(ch) = LVL_MORTAL;
	}
	// auto-authorization
	if (!IS_APPROVED(ch) && config_get_bool("auto_approve") && ch->desc && !has_anonymous_host(ch->desc)) {
		// only approve the character automatically
		SET_BIT(PLR_FLAGS(ch), PLR_APPROVED);
	}

	/* Standard conditions */
	GET_COND(ch, THIRST) = 0;
	GET_COND(ch, FULL) = 0;
	GET_COND(ch, DRUNK) = 0;

	// base stats: min 1
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		GET_REAL_ATT(ch, iter) = MAX(1, GET_REAL_ATT(ch, iter));
	}
	
	// randomize first tip
	GET_LAST_TIP(ch) = number(0, tips_of_the_day_size - 1);
	
	// confuuuusing
	GET_CONFUSED_DIR(ch) = number(0, NUM_SIMPLE_DIRS-1);

	/* Start playtime */
	ch->player.time.played = 0;
	ch->player.time.logon = time(0);
	
	// default flags
	SET_BIT(PRF_FLAGS(ch), PRF_AUTOKILL);
	SET_BIT(GET_INFORMATIVE_FLAGS(ch), DEFAULT_INFORMATIVE_BITS);
	
	// ensure custom channel colors default to off
	for (iter = 0; iter < NUM_CUSTOM_COLORS; ++iter) {
		GET_CUSTOM_COLOR(ch, iter) = 0;
	}
	
	// add the default slash channels
	global_mute_slash_channel_joins = TRUE;
	for (iter = 0; *default_channels[iter] != '\n'; ++iter) {
		sprintf(lbuf, "join %s", default_channels[iter]);
		do_slash_channel(ch, lbuf, 0, 0);
	}
	global_mute_slash_channel_joins = FALSE;
	
	// zero out attributes before applying archetypes
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		ch->real_attributes[iter] = 0;
	}

	// archetype setup
	for (arch_iter = 0; arch_iter < NUM_ARCHETYPE_TYPES; ++arch_iter) {
		if (!(arch = archetype_proto(CREATION_ARCHETYPE(ch, arch_iter))) && !(arch = archetype_proto(0))) {
			continue;	// couldn't find an archetype
		}
		if (GET_ARCH_TYPE(arch) != arch_iter) {
			continue;	// found archetype does not match this category
		}
		
		// Assign it:
		
		// attributes
		for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
			ch->real_attributes[iter] += GET_ARCH_ATTRIBUTE(arch, iter);
		}
	
		// skills
		for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
			level = get_skill_level(ch, sk->skill) + sk->level;
			level = MIN(level, MAX_SKILL_CAP);
			set_skill(ch, sk->skill, level);
			
			// special case for vampire
			if ((skill_data = find_skill_by_vnum(sk->skill)) && SKILL_FLAGGED(skill_data, SKILLF_VAMPIRE) && !IS_VAMPIRE(ch)) {
				make_vampire(ch, TRUE, sk->skill);
			}
		}
		
		// newbie eq -- don't run load triggers on this set, as ch is possibly not in a room
		for (gear = GET_ARCH_GEAR(arch); gear; gear = gear->next) {
			give_newbie_gear(ch, gear->vnum, gear->wear);
		}
		
		// language
		if (GET_ARCH_LANGUAGE(arch) && GEN_TYPE(GET_ARCH_LANGUAGE(arch)) == GENERIC_LANGUAGE && !GEN_FLAGGED(GET_ARCH_LANGUAGE(arch), GEN_IN_DEVELOPMENT)) {
			add_language(ch, GEN_VNUM(GET_ARCH_LANGUAGE(arch)), LANG_SPEAK);
		}
	}
	
	// guarantee minimum of 1 for active attributes
	for (iter = 0; primary_attributes[iter] != NOTHING; ++iter) {
		if (ch->real_attributes[primary_attributes[iter]] < 1) {
			ch->real_attributes[primary_attributes[iter]] = 1;
		}
	}
	
	// global newbie gear
	run_globals(GLOBAL_NEWBIE_GEAR, run_global_newbie_gear, TRUE, NOBITS, ch, NULL, 0, NULL, NULL);
	
	// basic updates
	determine_gear_level(ch);
	add_archetype_lore(ch);
	
	// apply any bonus traits that needed it
	apply_bonus_trait(ch, GET_BONUS_TRAITS(ch), TRUE);
	
	// if they have a valid promo code, apply it now
	if (GET_PROMO_ID(ch) >= 0 && promo_codes[GET_PROMO_ID(ch)].apply_func) {
		(promo_codes[GET_PROMO_ID(ch)].apply_func)(ch);
	}
	
	// set up class/level data
	update_class_and_abilities(ch);
	
	// restore pools (last, in case they changed during bonus traits or somewhere)
	set_health(ch, GET_MAX_HEALTH(ch));
	set_move(ch, GET_MAX_MOVE(ch));
	set_mana(ch, GET_MAX_MANA(ch));
	set_blood(ch, GET_MAX_BLOOD(ch));
	
	// starting temperature
	reset_player_temperature(ch);
	
	// prevent a repeat
	REMOVE_BIT(PLR_FLAGS(ch), PLR_NEEDS_NEWBIE_SETUP);
}


/**
* Updates the total playtime on a character, generally called when saving (but
* can safely be called any time.
*
* @param char_data *ch The player.
*/
void update_played_time(char_data *ch) {
	int amt = (int)(time(0) - ch->player.time.logon);
	ch->player.time.played += amt;
	ch->player.time.logon = time(0);	// reset this now
	
	// track to empire
	if (!IS_NPC(ch) && GET_LOYALTY(ch)) {
		track_empire_playtime(GET_LOYALTY(ch), amt);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// LANGUAGES ///////////////////////////////////////////////////////////////

/**
* Adds (or updates) a language for a player character. If the level is
* LANG_UNKNOWN, it will delete the entry (this is the default).
*
* @param char_data *ch The player.
* @param any_vnum vnum The vnum of the language (generic).
* @param byte level Any LANG_ const to indicate how well they know it the language.
*/
void add_language(char_data *ch, any_vnum vnum, byte level) {
	struct player_language *pl;
	
	if (!ch || IS_NPC(ch)) {
		return;	// oops
	}
	
	// find
	HASH_FIND_INT(GET_LANGUAGES(ch), &vnum, pl);
	
	// add if necessary
	if (!pl && level != LANG_UNKNOWN) {
		CREATE(pl, struct player_language, 1);
		pl->vnum = vnum;
		HASH_ADD_INT(GET_LANGUAGES(ch), vnum, pl);
	}
	
	// update
	if (pl) {
		pl->level = level;
	}
	
	// delete if necessary?
	if (pl && pl->level == LANG_UNKNOWN) {
		HASH_DEL(GET_LANGUAGES(ch), pl);
		free(pl);
		pl = NULL;
	}
	
	// ensure character speaks SOMETHING if this is a "speak"
	if (GET_SPEAKING(ch) == NOTHING && pl && pl->level == LANG_SPEAK) {
		GET_SPEAKING(ch) = vnum;
	}
	
	// otherwise ensure they're not speaking it
	if (GET_SPEAKING(ch) == vnum && pl && pl->level != LANG_SPEAK) {
		GET_SPEAKING(ch) = NOTHING;
	}
	
	// update quests and mark for save
	qt_change_language(ch, vnum, level);
	queue_delayed_update(ch, CDU_SAVE);
}


/**
* Adds (or updates) a language for an empires. If the level is LANG_UNKNOWN, it
* will delete the entry (this is the default). Empire languages are only used
* to give languages to members.
*
* @param empire_data *emp The empire.
* @param any_vnum vnum The vnum of the language (generic).
* @param byte level Any LANG_ const to indicate how well they know it the language.
*/
void add_language_empire(empire_data *emp, any_vnum vnum, byte level) {
	struct player_language *pl;
	
	if (!emp) {
		return;	// oops
	}
	
	// find
	HASH_FIND_INT(EMPIRE_LANGUAGES(emp), &vnum, pl);
	
	// add if necessary
	if (!pl && level != LANG_UNKNOWN) {
		CREATE(pl, struct player_language, 1);
		pl->vnum = vnum;
		HASH_ADD_INT(EMPIRE_LANGUAGES(emp), vnum, pl);
	}
	
	// update
	if (pl) {
		pl->level = level;
	}
	
	// delete if necessary?
	if (pl && pl->level == LANG_UNKNOWN) {
		HASH_DEL(EMPIRE_LANGUAGES(emp), pl);
		free(pl);
		pl = NULL;
	}
	
	// update quests and mark for save
	// et_change_language(emp, vnum, level);
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
}


/**
* Grants a player all the languages their empire speaks. This is meant to be
* called from within check_languages(), which immediately validates them all.
*
* @param char_data *ch The player.
*/
static void assign_empire_languages(char_data *ch) {
	struct player_language *pl, *next_pl;
	int level;
	
	if (!ch || IS_NPC(ch) || !GET_LOYALTY(ch)) {
		return;	// no work
	}
	
	HASH_ITER(hh, EMPIRE_LANGUAGES(GET_LOYALTY(ch)), pl, next_pl) {
		level = speaks_language(ch, pl->vnum);
		if (level < pl->level) {
			add_language(ch, pl->vnum, pl->level);
		}
	}
}


/**
* This is called when a character logs in or when a language is saved in OLC
* (under some circumstances) to ensure the character has the correct languages
* in their list.
*
* @param char_data *ch The player.
*/
void check_languages(char_data *ch) {
	struct player_language *lang, *next_lang;
	generic_data *gen, *next_gen;
	bool ok = FALSE, save = FALSE;
	any_vnum backup_speaking = NOTHING, second_backup = NOTHING;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	assign_empire_languages(ch);
	
	// check languages they know
	HASH_ITER(hh, GET_LANGUAGES(ch), lang, next_lang) {
		// audit it
		if (!(gen = find_generic(lang->vnum, GENERIC_LANGUAGE))) {
			ok = FALSE;	// deleted?
		}
		else if (GEN_FLAGGED(gen, GEN_IN_DEVELOPMENT)) {
			ok = FALSE;	// in-dev
		}
		else if (lang->level == LANG_UNKNOWN) {
			ok = FALSE;	// might as well delete these here
		}
		else {
			// seems ok
			ok = TRUE;
		}
		
		// did we make it
		if (ok) {
			// try to store as a backup for later
			if (backup_speaking == NOTHING && GEN_FLAGGED(gen, GEN_BASIC)) {
				backup_speaking = lang->vnum;
			}
			else if (second_backup == NOTHING) {
				// non-basic backup just in case
				second_backup = lang->vnum;
			}
		}
		else {
			// remove
			save = TRUE;
			HASH_DEL(GET_LANGUAGES(ch), lang);
			free(lang);
		}
	}
	
	// check for common languages
	HASH_ITER(hh, generic_table, gen, next_gen) {
		if (GEN_TYPE(gen) != GENERIC_LANGUAGE) {
			continue;	// not a language
		}
		if (GEN_FLAGGED(gen, GEN_IN_DEVELOPMENT)) {
			continue;	// can't be in-dev
		}
		if (!GEN_FLAGGED(gen, GEN_BASIC)) {
			continue;	// must be basic
		}
		
		// ok? ensure the player speaks it
		if (speaks_language(ch, GEN_VNUM(gen)) != LANG_SPEAK) {
			add_language(ch, GEN_VNUM(gen), LANG_SPEAK);
			save = TRUE;
		}
		
		// try to store as a backup for later
		if (backup_speaking == NOTHING) {
			backup_speaking = GEN_VNUM(gen);
		}
	}
	
	// ensure they have a valid 'speaking'
	if (GET_SPEAKING(ch) == NOTHING || speaks_language(ch, GET_SPEAKING(ch)) != LANG_SPEAK) {
		// this MAY still be 'NOTHING' but it should be a best choice either way
		GET_SPEAKING(ch) = (backup_speaking != NOTHING ? backup_speaking : second_backup);
		save = TRUE;
	}
	
	// and save if requested
	if (save) {
		queue_delayed_update(ch, CDU_SAVE);
	}
}


/**
* Ensures all characters in the game have valid languages. This does not need
* to hit players who are at menus rather than in-game because they will be
* checked when they enter the game.
*/
void check_languages_all(void) {
	char_data *ch;
	
	check_languages_all_empires();
	
	DL_FOREACH2(player_character_list, ch, next_plr) {
		check_languages(ch);
	}
}


/**
* Checks languages for all empires.
*/
void check_languages_all_empires(void) {
	empire_data *emp, *next_emp;
	HASH_ITER(hh, empire_table, emp, next_emp) {
		check_languages_empire(emp);
	}
}


/**
* Check languages for an empire to make sure they are all valid.
*
* @param empire_data *emp The empire.
*/
void check_languages_empire(empire_data *emp) {
	generic_data *gen;
	struct player_language *lang, *next_lang;
	bool ok = FALSE, save = FALSE;
	
	if (emp) {
		HASH_ITER(hh, EMPIRE_LANGUAGES(emp), lang, next_lang) {
			// audit it
			if (!(gen = find_generic(lang->vnum, GENERIC_LANGUAGE))) {
				ok = FALSE;	// deleted?
			}
			else if (GEN_FLAGGED(gen, GEN_IN_DEVELOPMENT)) {
				ok = FALSE;	// in-dev
			}
			else if (lang->level == LANG_UNKNOWN) {
				ok = FALSE;	// might as well delete these here
			}
			else {
				// seems ok
				ok = TRUE;
			}
		
			// did we make it
			if (!ok) {
				// remove
				save = TRUE;
				HASH_DEL(EMPIRE_LANGUAGES(emp), lang);
				free(lang);
			}
		}
		
		if (save) {
			EMPIRE_NEEDS_SAVE(emp) = TRUE;
		}
	}
}


/**
* Determines how well a character knows a language. NPCs always speak any
* language.
*
* @param char_data *ch The player.
* @param any_vnum vnum The vnum of the language (generic).
* @return int A LANG_ constant indicating how well the player knows the language.
*/
int speaks_language(char_data *ch, any_vnum vnum) {
	struct player_language *pl;
	
	if (!ch) {
		// oops
		return LANG_UNKNOWN;
	}
	if (IS_NPC(ch)) {
		// npcs always speak a language
		return LANG_SPEAK;
	}
	
	// find
	HASH_FIND_INT(GET_LANGUAGES(ch), &vnum, pl);
	return pl ? pl->level : LANG_UNKNOWN;
}


/**
* Determines if an empire has unlocked a language at the empire level (usually
* from progress goals).
*
* @param empire_data *emp The empire.
* @param any_vnum vnum The vnum of the language (generic).
* @return int A LANG_ constant indicating how well the empire knows the language.
*/
int speaks_language_empire(empire_data *emp, any_vnum vnum) {
	struct player_language *pl;
	
	if (!emp) {
		// oops
		return LANG_UNKNOWN;
	}
	
	// find
	HASH_FIND_INT(EMPIRE_LANGUAGES(emp), &vnum, pl);
	return pl ? pl->level : LANG_UNKNOWN;
}


 //////////////////////////////////////////////////////////////////////////////
//// MAP MEMORY //////////////////////////////////////////////////////////////

/**
* Marks a tile for a player to remember.
*
* @param char_data *ch The player.
* @param room_vnum vnum The room (must be on the map).
* @param char *icon The map icon (Optional: may be NULL for screenreaders).
* @param char *name The room name (Optional: may be NULL if just looking at the ascii map).
* @param time_t use_timestamp When this entry was added (Optional: Just pass 0 to detect here).
*/
void add_player_map_memory(char_data *ch, room_vnum vnum, char *icon, char *name, time_t use_timestamp) {
	struct player_map_memory *map_mem;
	
	if (!ch || IS_NPC(ch) || vnum == NOWHERE || vnum >= MAP_SIZE) {
		return;	// no work
	}
	
	// ensure this is loaded
	load_map_memory(ch);
	
	// find or create entry
	HASH_FIND_INT(GET_MAP_MEMORY(ch), &vnum, map_mem);
	if (!map_mem) {
		CREATE(map_mem, struct player_map_memory, 1);
		map_mem->vnum = vnum;
		HASH_ADD_INT(GET_MAP_MEMORY(ch), vnum, map_mem);
		GET_MAP_MEMORY_NEEDS_SAVE(ch) = TRUE;
		++GET_MAP_MEMORY_COUNT(ch);
	}
	
	// update
	map_mem->timestamp = (use_timestamp > 0) ? use_timestamp : time(0);
	
	if (icon && *icon && (!map_mem->icon || strcmp(icon, map_mem->icon))) {
		if (map_mem->icon) {
			free(map_mem->icon);
		}
		map_mem->icon = str_dup(strip_color(icon));
		GET_MAP_MEMORY_NEEDS_SAVE(ch) = TRUE;
	}
	
	if (name && *name && (!map_mem->name || strcmp(name, map_mem->name))) {
		if (map_mem->name) {
			free(map_mem->name);
		}
		map_mem->name = str_dup(name);
		GET_MAP_MEMORY_NEEDS_SAVE(ch) = TRUE;
	}
}


/**
* Deletes and frees a map memory.
*
* @param struct player_map_memory *memory The memory to free.
* @param char_data *ch Optional: If provided, removes the memory from this player's hash. (NULL to skip this step.)
*/
void delete_player_map_memory(struct player_map_memory *memory, char_data *ch) {
	// ch is optional
	if (ch && !IS_NPC(ch)) {
		GET_MAP_MEMORY_NEEDS_SAVE(ch) = TRUE;
		HASH_DEL(GET_MAP_MEMORY(ch), memory);
		--GET_MAP_MEMORY_COUNT(ch);
	}
	
	// free the memory
	if (memory->icon) {
		free(memory->icon);
	}
	if (memory->name) {
		free(memory->name);
	}
	free(memory);
}


/**
* Gets a player's memory of a map location, if any.
*
* @param char_data *ch The player.
* @param room_vnum vnum Any map location.
* @param int type MAP_MEM_ICON or MAP_MEM_NAME -- which string to fetch.
* @return const char* The icon or name, if available, or NULL if not.
*/
const char *get_player_map_memory(char_data *ch, room_vnum vnum, int type) {
	struct player_map_memory *map_mem;
	
	if (ch && !IS_NPC(ch) && has_player_tech(ch, PTECH_MAP_MEMORY)) {
		// ensure this is loaded
		load_map_memory(ch);
		
		HASH_FIND_INT(GET_MAP_MEMORY(ch), &vnum, map_mem);
		if (map_mem) {
			if (type == MAP_MEM_ICON) {
				return map_mem->icon;	// if any
			}
			else if (type == MAP_MEM_NAME) {
				return map_mem->name;	// if any
			}
		}
	}
	
	return NULL;	// all other cases
}


/**
* Only loads a player's map memory if needed, and if not already loaded.
*
* @param char_data *ch The player.
*/
void load_map_memory(char_data *ch) {
	char filename[256], line[256], icon[256], name[256];
	long timestamp;
	room_vnum vnum;
	FILE *fl;
	
	if (ch && !IS_NPC(ch) && !GET_MAP_MEMORY_LOADED(ch) && has_player_tech(ch, PTECH_MAP_MEMORY)) {
		// this will be true no matter what
		GET_MAP_MEMORY_LOADED(ch) = TRUE;
		
		if (!get_filename(GET_PC_NAME(ch), filename, MAP_MEMORY_FILE)) {
			log("SYSERR: load_map_memory: Unable to get memory filename for '%s'", GET_PC_NAME(ch));
			return;
		}
		if (!(fl = fopen(filename, "r"))) {
			// non-fatal: memory file does not exist
			return;
		}
		
		// load from file
		while (get_line(fl, line)) {
			if (*line == '*' && sscanf(line+2, "%d %ld %s", &vnum, &timestamp, name) == 3) {
				// remove dummy values
				if (!strcmp(name, "~")) {
					*name = '\0';
				}
				// and add
				add_player_map_memory(ch, vnum, "", name, timestamp);
			}
			else if (*line && sscanf(line, "%d %ld %4s %s", &vnum, &timestamp, icon, name) == 4) {
				// remove dummy values
				if (!strcmp(icon, "    ")) {
					*icon = '\0';
				}
				if (!strcmp(name, "~")) {
					*name = '\0';
				}
				// and add
				add_player_map_memory(ch, vnum, double_map_ampersands(icon), name, timestamp);
			}
			else {
				log("Warning: Unknown map memory line for %s: %s", GET_PC_NAME(ch), line);
			}
		}
		
		fclose(fl);	
		
		GET_MAP_MEMORY_NEEDS_SAVE(ch) = FALSE;	// this is set by the loading process
	}
}


// simple sorter to put memories in ascending order of timestamp
int sort_map_memory(struct player_map_memory *a, struct player_map_memory *b) {
	return a->timestamp - b->timestamp;
}


/**
* Writes the map memory file for a player.
*
* @param char_data *ch The player whose map memory to save.
*/
void write_map_memory(char_data *ch) {
	struct player_map_memory *map_mem, *next;
	char filename[256], tempname[256];
	FILE *fl;
	const int limit = config_get_int("map_memory_limit");
	
	if (!ch || IS_NPC(ch) || !GET_MAP_MEMORY_LOADED(ch) || !GET_MAP_MEMORY_NEEDS_SAVE(ch)) {	
		return;	// no work
	}
	
	// cleanup when there are too many
	if (GET_MAP_MEMORY_COUNT(ch) > limit) {
		HASH_SORT(GET_MAP_MEMORY(ch), sort_map_memory);
		HASH_ITER(hh, GET_MAP_MEMORY(ch), map_mem, next) {
			if (GET_MAP_MEMORY_COUNT(ch) > (limit * 0.9)) {
				delete_player_map_memory(map_mem, ch);
			}
		}
	}
	
	// update this no matter what
	GET_MAP_MEMORY_NEEDS_SAVE(ch) = FALSE;
	
	if (!get_filename(GET_PC_NAME(ch), filename, MAP_MEMORY_FILE)) {
		log("SYSERR: write_map_memory: Unable to get memory filename for '%s'", GET_PC_NAME(ch));
		return;
	}
	snprintf(tempname, sizeof(tempname), "%s%s", filename, TEMP_SUFFIX);
	if (!(fl = fopen(tempname, "w"))) {
		log("SYSERR: write_map_memory: Unable to get open file for writing: %s", tempname);
		return;
	}
	
	HASH_ITER(hh, GET_MAP_MEMORY(ch), map_mem, next) {
		if (map_mem->icon) {
			fprintf(fl, "%d %ld %-4.4s %s\n", map_mem->vnum, map_mem->timestamp, undouble_map_ampersands(map_mem->icon), (map_mem->name && *map_mem->name) ? map_mem->name : "~");
		}
		else {	// no icon
			fprintf(fl, "* %d %ld %s\n", map_mem->vnum, map_mem->timestamp, (map_mem->name && *map_mem->name) ? map_mem->name : "~");
		}
	}
	
	fclose(fl);
	rename(tempname, filename);
}


 //////////////////////////////////////////////////////////////////////////////
//// PLAYTIME TRACKING ///////////////////////////////////////////////////////

// sorts newest-first based on DAILY_CYCLE_DAY
int sort_playtime_tracker(struct empire_playtime_tracker *a, struct empire_playtime_tracker *b) {
	return b->cycle - a->cycle;
}


/**
* Called whenever a player's playtime is updated to also update the empire's
* daily playtime.
*
* @param empire_data *emp The empire.
* @param int add_seconds Amount of time to add, in seconds.
*/
void track_empire_playtime(empire_data *emp, int add_seconds) {
	struct empire_playtime_tracker *ept;
	int cycle = DAILY_CYCLE_DAY;
	
	if (emp && add_seconds > 0) {
		HASH_FIND_INT(EMPIRE_PLAYTIME_TRACKER(emp), &cycle, ept);
		if (!ept) {	// create if needed
			CREATE(ept, struct empire_playtime_tracker, 1);
			ept->cycle = cycle;
			HASH_ADD_INT(EMPIRE_PLAYTIME_TRACKER(emp), cycle, ept);
		}
		SAFE_ADD(ept->playtime_secs, add_seconds, 0, INT_MAX, FALSE);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}


/**
* Collates the empire's playtime tracker into total playtime (in seconds) for
* each of the last N weeks, based on PLAYTIME_WEEKS_TO_TRACK. Most recent week
* is first.
*
* The output of this function should be copied if you need to keep it; it is a
* static int array and calling it on another empire will lose your original
* values.
*
* @param empire_data *emp The empire.
* @return int* An array of ints, of size PLAYTIME_WEEKS_TO_TRACK, starting with the current week and moving backwards.
*/
int *summarize_weekly_playtime(empire_data *emp) {
	struct empire_playtime_tracker *ept, *next;
	static int data[PLAYTIME_WEEKS_TO_TRACK];
	int iter, pos, cur;
	
	// prep first
	cur = DAILY_CYCLE_DAY;
	HASH_SORT(EMPIRE_PLAYTIME_TRACKER(emp), sort_playtime_tracker);
	for (iter = 0; iter < PLAYTIME_WEEKS_TO_TRACK; ++iter) {
		data[iter] = 0;
	}
	
	// collect data
	HASH_ITER(hh, EMPIRE_PLAYTIME_TRACKER(emp), ept, next) {
		pos = (int)((cur - ept->cycle) / 7);
		
		if (pos < PLAYTIME_WEEKS_TO_TRACK) {
			SAFE_ADD(data[pos], ept->playtime_secs, 0, INT_MAX, FALSE);
		}
		else {	// otherwise delete old data now
			HASH_DEL(EMPIRE_PLAYTIME_TRACKER(emp), ept);
			free(ept);
			continue;
		}
	}
	
	return data;
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE MEMBER/GREATNESS TRACKING ////////////////////////////////////////

// This system keeps greatness (and other data) organized by account/character

/**
* Find/add a member account in an empire.
*
* @param empire_data *emp The empire.
* @param int id The account id.
* @param bool add_if_missing Will attempt to create the item.
* @return struct empire_member_account* The account entry, if possible. Otherwise NULL.
*/
struct empire_member_account *get_member_account(empire_data *emp, int id, bool add_if_missing) {
	struct empire_member_account *emma;
	
	// safety
	if (!emp || !id) {
		return NULL;
	}
	
	HASH_FIND_INT(EMPIRE_MEMBER_ACCOUNTS(emp), &id, emma);
	if (!emma && add_if_missing) {
		CREATE(emma, struct empire_member_account, 1);
		emma->id = id;
		HASH_ADD_INT(EMPIRE_MEMBER_ACCOUNTS(emp), id, emma);
	}
	
	return emma;	// if any
}


/**
* Find/add a character entry in an empire's account entry.
*
* @param struct empire_member_account *mem_acct The account entry from an empire.
* @param int id The player id.
* @param bool add_if_missing Will attempt to create the item.
* @return struct empire_member_data* The character entry, if possible. Otherwise NULL.
*/
struct empire_member_data *get_member_account_player(struct empire_member_account *mem_acct, int id, bool add_if_missing) {
	struct empire_member_data *emda;
	
	// safety
	if (!mem_acct || !id) {
		return NULL;
	}
	
	HASH_FIND_INT(mem_acct->list, &id, emda);
	if (!emda && add_if_missing) {
		CREATE(emda, struct empire_member_data, 1);
		emda->id = id;
		HASH_ADD_INT(mem_acct->list, id, emda);
	}
	
	return emda;
}


/**
* Deletes the data for a character from an empire's member/account list.
*
* @param char_data *ch The character to remove.
* @param empire_data *from_emp The empire to remove them from.
*/
void delete_member_data(char_data *ch, empire_data *from_emp) {
	struct empire_member_account *acct;
	struct empire_member_data *data;
	
	if (!ch || IS_NPC(ch) || !from_emp || !GET_ACCOUNT(ch)) {
		return;	// safety first
	}
	
	if ((acct = get_member_account(from_emp, GET_ACCOUNT(ch)->id, FALSE))) {
		// delete player
		if ((data = get_member_account_player(acct, GET_IDNUM(ch), FALSE))) {
			HASH_DEL(acct->list, data);
			free(data);
		}
		
		// any players left?
		if (!acct->list) {
			HASH_DEL(EMPIRE_MEMBER_ACCOUNTS(from_emp), acct);
			free(acct);
		}
	}
}


/**
* Frees the EMPIRE_MEMBER_ACCOUNTS.
*
* @param empire_data *emp The empire.
*/
void free_member_data(empire_data *emp) {
	struct empire_member_account *acct, *next_acct;
	struct empire_member_data *data, *next_data;
	
	HASH_ITER(hh, EMPIRE_MEMBER_ACCOUNTS(emp), acct, next_acct) {
		HASH_ITER(hh, acct->list, data, next_data) {
			HASH_DEL(acct->list, data);
			free(data);
		}
		
		HASH_DEL(EMPIRE_MEMBER_ACCOUNTS(emp), acct);
		free(acct);
	}
}


/**
* Updates an empire's members and greatness count.
*
* @param empire_data *emp The empire.
*/
void update_empire_members_and_greatness(empire_data *emp) {
	int best, old, greatness = 0, members = 0, total = 0;
	struct empire_member_account *acct, *next_acct;
	struct empire_member_data *data, *next_data;
	time_t curtime = time(0);
	bool any;
	
	if (!emp) {
		return;	// safety
	}
	
	old = EMPIRE_GREATNESS(emp);
	
	HASH_ITER(hh, EMPIRE_MEMBER_ACCOUNTS(emp), acct, next_acct) {
		best = 0;
		any = FALSE;
		HASH_ITER(hh, acct->list, data, next_data) {
			// always update total
			++total;
			
			// stop here if timed out
			if (data->timeout_at <= curtime) {
				continue;
			}
			
			// ok:
			any = TRUE;
			
			// update greatness
			if (data->greatness > best) {
				best = data->greatness;
			}
		}
		
		if (any || best > 0) {	// found an active member
			greatness += best;
			++members;
		}
	}
	
	EMPIRE_GREATNESS(emp) = greatness;
	EMPIRE_MEMBERS(emp) = members;
	EMPIRE_TOTAL_MEMBER_COUNT(emp) = total;
	
	if (greatness != old) {
		et_change_greatness(emp);
		update_MSDP_empire_data_all(emp, TRUE, TRUE);
	}
}


/**
* Updates the empire member data for a character, to be called if their
* greatness changes (or any trait tracked by this).
*
* @param char_data *ch The character to update.
*/
void update_member_data(char_data *ch) {
	struct empire_member_account *acct;
	struct empire_member_data *data;
	
	if (!ch || IS_NPC(ch) || !GET_LOYALTY(ch) || !GET_ACCOUNT(ch)) {
		return;	// safety first
	}
	
	if ((acct = get_member_account(GET_LOYALTY(ch), GET_ACCOUNT(ch)->id, TRUE))) {
		if ((data = get_member_account_player(acct, GET_IDNUM(ch), TRUE))) {
			data->timeout_at = get_member_timeout_ch(ch);
			data->greatness = GET_GREATNESS(ch);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE PLAYER MANAGEMENT ////////////////////////////////////////////////

/**
* Determines when an empire member is timed out based on his playtime, creation
* time, and last login.
*
* @param time_t created The player character's birth time.
* @param time_t last_login The player's last login time.
* @param double played_hours Number of hours the player has played, total.
* @return time_t The timestamp when the member would time out (may be past, present, or future).
*/
time_t get_member_timeout_time(time_t created, time_t last_login, double played_hours) {
	int member_timeout_full = config_get_int("member_timeout_full") * SECS_PER_REAL_DAY;
	int member_timeout_newbie = config_get_int("member_timeout_newbie") * SECS_PER_REAL_DAY;
	int minutes_per_day_full = config_get_int("minutes_per_day_full");
	int minutes_per_day_newbie = config_get_int("minutes_per_day_newbie");

	if (played_hours >= config_get_int("member_timeout_max_threshold")) {
		return (last_login + member_timeout_full);
	}
	else {
		double days_played = (double)(time(0) - created) / SECS_PER_REAL_DAY;
		double avg_min_per_day = 60 * (played_hours / days_played);
		double timeout;
		
		// when playtime drops this low, the character is ALWAYS timed out
		if (avg_min_per_day <= 1) {
			return last_login;
		}
		
		if (avg_min_per_day >= minutes_per_day_full) {
			timeout = member_timeout_full;
		}
		else if (avg_min_per_day <= minutes_per_day_newbie) {
			timeout = member_timeout_newbie;
		}
		else {
			// somewhere in the middle
			double prc = (avg_min_per_day - minutes_per_day_newbie) / (minutes_per_day_full - minutes_per_day_newbie);
			double scale = member_timeout_full - member_timeout_newbie;
			timeout = member_timeout_newbie + (prc * scale);
		}
		
		return (last_login + timeout);
	}
}


/**
* Calls get_member_timeout_time() using a player_index_data.
*
* @param player_index_data *index A pointer to the playertable entry.
* @return bool TRUE if the member has timed out and should not be counted; FALSE if they're ok.
*/
bool member_is_timed_out_index(player_index_data *index) {
	return get_member_timeout_time(index->birth, index->last_logon, ((double)index->played) / SECS_PER_REAL_HOUR) <= time(0);
}


/**
* Calls get_member_timeout_time() using a char_data.
*
* @param char_data *ch A character to compute timeout on.
* @return time_t When the member would time out (past/present/future).
*/
time_t get_member_timeout_ch(char_data *ch) {
	return get_member_timeout_time(ch->player.time.birth, ch->prev_logon, ((double)ch->player.time.played) / SECS_PER_REAL_HOUR);
}


/**
* Calls get_member_timeout_time() using a char_data.
*
* @param char_data *ch A character to compute timeout on.
* @return bool TRUE if the member has timed out and should not be counted; FALSE if they're ok.
*/
bool member_is_timed_out_ch(char_data *ch) {
	return get_member_timeout_ch(ch) <= time(0);
}


/**
* This function reads and re-sets member-realted aspects of all empires,
* but it does not clear technology flags before adding in new ones -- if you
* need to do that, call reread_empire_tech() instead.
*
* @param int only_empire if not NOTHING, only reads 1 empire
* @param bool read_techs if TRUE, will add techs based on players (usually only during startup)
*/
void read_empire_members(empire_data *only_empire, bool read_techs) {
	player_index_data *index, *next_index;
	empire_data *e, *emp, *next_emp;
	char_data *ch;
	time_t logon, curtime = time(0), timeout;
	bool is_file;
	int level;

	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (!only_empire || emp == only_empire) {
			EMPIRE_TOTAL_MEMBER_COUNT(emp) = 0;
			EMPIRE_MEMBERS(emp) = 0;
			EMPIRE_TOTAL_PLAYTIME(emp) = 0;
			EMPIRE_LAST_LOGON(emp) = 0;
			EMPIRE_IMM_ONLY(emp) = 0;
			EMPIRE_NEXT_TIMEOUT(emp) = 0;
			EMPIRE_MIN_LEVEL(emp) = 0;
			EMPIRE_MAX_LEVEL(emp) = 0;
		}
	}
	
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
		if (only_empire && index->loyalty != only_empire) {
			continue;
		}
		
		// load the player unless they're in-game
		if ((ch = find_or_load_player(index->name, &is_file))) {
			// check_delayed_load(ch);	// no longer need this as equipment is in the main file
			affect_total(ch);
		}
		
		// check ch for empire traits
		if (ch && (e = GET_LOYALTY(ch))) {
			// record last-logon whether or not timed out
			logon = is_file ? ch->prev_logon : time(0);
			if (logon > EMPIRE_LAST_LOGON(e)) {
				EMPIRE_LAST_LOGON(e) = logon;
			}

			if (GET_ACCESS_LEVEL(ch) >= LVL_GOD) {
				EMPIRE_IMM_ONLY(e) = 1;
			}
			
			// always
			update_member_data(ch);
			EMPIRE_TOTAL_MEMBER_COUNT(e) += 1;
			
			// only count players who have logged on in recent history
			timeout = get_member_timeout_ch(ch);
			
			if (!is_file || timeout > curtime) {
				// not account-restricted
				EMPIRE_TOTAL_PLAYTIME(e) += (ch->player.time.played / SECS_PER_REAL_HOUR);
				level = is_file ? GET_HIGHEST_KNOWN_LEVEL(ch) : (int) GET_COMPUTED_LEVEL(ch);
				if (!EMPIRE_MIN_LEVEL(e) || level < EMPIRE_MIN_LEVEL(e)) {
					EMPIRE_MIN_LEVEL(e) = level;
				}
				EMPIRE_MAX_LEVEL(e) = MAX(EMPIRE_MAX_LEVEL(e), level);
			}
			
			// update next timeout check
			if (timeout > curtime) {
				EMPIRE_NEXT_TIMEOUT(e) = MIN(EMPIRE_NEXT_TIMEOUT(e), timeout);
			}
		}
		
		if (ch && is_file && !only_empire) {
			// only free right away if doing all empires -- it lags BADLY otherwise
			free_char(ch);
		}
	}
	
	// do this before risking deleting empires
	clear_delayed_empire_refresh(only_empire, DELAY_REFRESH_MEMBERS);
	
	// final updates
	HASH_ITER(hh, empire_table, emp, next_emp) {
		// refresh first
		if (!only_empire || emp == only_empire) {
			update_empire_members_and_greatness(emp);
		}
		
		// delete emptypires
		if ((!only_empire || emp == only_empire) && should_delete_empire(emp)) {
			delete_empire(emp);
			
			// don't accidentally keep deleting if we were only doing 1 (it's been freed)
			if (only_empire) {
				break;
			}
		}
	}
	
	// re-sort now only if we aren't reading techs (this hints that we're also reading territory)
	if (!read_techs) {
		resort_empires(FALSE);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EQUIPMENT SETS //////////////////////////////////////////////////////////

/**
* Creates an equipment set entry for the player. This will fail if the data is
* invalid. If there's already a set with that id, its name will be overwritten.
*
* @param char_data *ch The player to add the set to.
* @param int set_id The set's idnum, or NOTHING if you want to auto-detect and create a new one.
* @param char *name The name of the equipment set (arbitrary but must be pre-validated). NOTE: This string will be stored as the name, so pass a copy of it.
* @return int The id of the set (in case you requested a new one), or NOTHING if we failed to create one.
*/
int add_eq_set_to_char(char_data *ch, int set_id, char *name) {
	struct player_eq_set *eq_set = NULL;
	bool found = FALSE;
	int max_id = 0;
	
	if (!ch || IS_NPC(ch) || set_id == 0 || !name || !*name) {
		if (name) {
			free(name);
		}
		return NOTHING;	// invalid input
	}
	
	// because eq sets are delay data...
	check_delayed_load(ch);
	
	// look for existing set
	LL_FOREACH(GET_EQ_SETS(ch), eq_set) {
		if (set_id > 0 && eq_set->id == set_id) {
			// found existing
			if (eq_set->name) {
				free(eq_set->name);
			}
			eq_set->name = name;
			found = TRUE;
			return eq_set->id;	// done
		}
		else if (max_id < eq_set->id) {
			max_id = eq_set->id;
		}
	}
	
	if (!found) {
		CREATE(eq_set, struct player_eq_set, 1);
		eq_set->id = (set_id > 0 ? set_id : ++max_id);
		eq_set->name = name;
		LL_PREPEND(GET_EQ_SETS(ch), eq_set);
	}
	
	return eq_set ? eq_set->id : NOTHING;
}


/**
* Adds an object to an equipment set, checking that it's unique in the obj's
* list and overwriting existing data if not. This does NOT validate the set id
* in any way.
*
* @param obj_data *obj Which object to set data on.
* @param int set_id The id of the set.
* @param int pos Which WEAR_ pos to set it to.
*/
void add_obj_to_eq_set(obj_data *obj, int set_id, int pos) {
	struct eq_set_obj *eq_set;
	bool found = FALSE;
	
	if (!obj || set_id <= 0 || pos < 0 || pos >= NUM_WEARS) {
		return;	// invalid data
	}
	
	// look for data to overwrite
	LL_FOREACH(GET_OBJ_EQ_SETS(obj), eq_set) {
		if (eq_set->id == set_id) {
			eq_set->pos = pos;
			found = TRUE;
			break;	// only need 1
		}
	}
	
	// add if needed
	if (!found) {
		CREATE(eq_set, struct eq_set_obj, 1);
		eq_set->id = set_id;
		eq_set->pos = pos;
		LL_PREPEND(GET_OBJ_EQ_SETS(obj), eq_set);
	}
	
	request_obj_save_in_world(obj);
}


/**
* Verifies all equipment sets on a player and their objects when they log in.
* Invalid sets will be removed.
*
* @param char_data *ch The player to check.
*/
void check_eq_sets(char_data *ch) {
	struct player_eq_set *pset, *next_pset;
	struct eq_set_obj *oset, *next_oset;
	obj_data *obj;
	int pos;
	bool any = FALSE;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	// verify set integrity first
	LL_FOREACH_SAFE(GET_EQ_SETS(ch), pset, next_pset) {
		if (pset->id < 1 || !pset->name || !*pset->name) {
			LL_DELETE(GET_EQ_SETS(ch), pset);
			free_player_eq_set(pset);
		}
	}
	
	// make sure all the player's objects (and their objects) are on valid sets
	for (pos = 0; pos < NUM_WEARS; ++pos) {
		if ((obj = GET_EQ(ch, pos))) {
			LL_FOREACH_SAFE(GET_OBJ_EQ_SETS(obj), oset, next_oset) {
				if (!get_eq_set_by_id(ch, oset->id)) {
					// not found
					LL_DELETE(GET_OBJ_EQ_SETS(obj), oset);
					free_obj_eq_set(oset);
					any = TRUE;
				}
			}
		}
	}
	DL_FOREACH2(ch->carrying, obj, next_content) {
		LL_FOREACH_SAFE(GET_OBJ_EQ_SETS(obj), oset, next_oset) {
			if (!get_eq_set_by_id(ch, oset->id)) {
				// not found
				LL_DELETE(GET_OBJ_EQ_SETS(obj), oset);
				free_obj_eq_set(oset);
				any = TRUE;
			}
		}
	}
	
	if (any) {
		request_char_save_in_world(ch);
	}
}


/**
* @param char_data *ch The player.
* @return int The number of eq sets the play has saved, currently.
*/
int count_eq_sets(char_data *ch) {
	struct player_eq_set *eq_set;
	int num = 0;
	
	if (!IS_NPC(ch)) {
		LL_COUNT(GET_EQ_SETS(ch), eq_set, num);
	}
	
	return num;
}


/**
* Look up an equipment set by id.
*
* @param char_data *ch The player to look up a set for.
* @param int id Which set to find.
* @return struct player_eq_set* The found eq set, if any, or NULL if not.
*/
struct player_eq_set *get_eq_set_by_id(char_data *ch, int id) {
	struct player_eq_set *eq_set;
	
	if (IS_NPC(ch)) {
		return NULL;
	}
	
	LL_FOREACH(GET_EQ_SETS(ch), eq_set) {
		if (eq_set->id == id) {
			return eq_set;
		}
	}
	
	return NULL;
}


/**
* Look up an equipment set by name. It prefers an exact match but will also
* allow a multi-keyword match.
*
* @param char_data *ch The player to look up a set for.
* @param char *name The name to find;
* @return struct player_eq_set* The found eq set, if any, or NULL if not.
*/
struct player_eq_set *get_eq_set_by_name(char_data *ch, char *name) {
	struct player_eq_set *eq_set, *partial = NULL;
	
	if (IS_NPC(ch)) {
		return NULL;
	}
	
	LL_FOREACH(GET_EQ_SETS(ch), eq_set) {
		if (!str_cmp(name, eq_set->name)) {
			return eq_set;	// exact match
		}
		else if (!partial && multi_isname(name, eq_set->name)) {
			partial = eq_set;	// save for later
		}
	}
	
	return partial;	// if any
}


/**
* Finds a matching eq set entry on an object, if present.
*
* @param obj_data *obj The object to look at.
* @param int id The equipment set id to find.
* @return struct eq_set_obj* The found set entry, if it exists.
*/
struct eq_set_obj *get_obj_eq_set_by_id(obj_data *obj, int id) {
	struct eq_set_obj *oset;
	
	if (obj) {
		LL_FOREACH(GET_OBJ_EQ_SETS(obj), oset) {
			if (oset->id == id) {
				return oset;
			}
		}
	}
	
	return NULL;
}


/**
* If obj is part of the listed eq set, it removes it.
*
* @param obj_data *obj Which object to set data on.
* @param int set_id The id of the set.
*/
void remove_obj_from_eq_set(obj_data *obj, int set_id) {
	struct eq_set_obj *eq_set, *next_set;
	
	if (!obj) {
		return;	// invalid data
	}
	
	// look for data to remove
	LL_FOREACH_SAFE(GET_OBJ_EQ_SETS(obj), eq_set, next_set) {
		if (eq_set->id == set_id) {
			LL_DELETE(GET_OBJ_EQ_SETS(obj), eq_set);
			free_obj_eq_set(eq_set);
		}
	}
	
	request_obj_save_in_world(obj);
}


 //////////////////////////////////////////////////////////////////////////////
//// PROMO CODES /////////////////////////////////////////////////////////////

// these are configured in config.c


// starting coins
PROMO_APPLY(promo_countdemonet) {
	increase_coins(ch, REAL_OTHER_COIN, 100);
}


// bonus charisma
PROMO_APPLY(promo_facebook) {
	ch->real_attributes[CHARISMA] = MAX(1, MIN(att_max(ch), ch->real_attributes[CHARISMA] + 1));
	affect_total(ch);
}


// +10 to starting skills
PROMO_APPLY(promo_skillups) {
	struct player_skill_data *skill, *next_skill;
	
	HASH_ITER(hh, GET_SKILL_HASH(ch), skill, next_skill) {
		if (skill->level > 0) {
			if (get_skill_level(ch, skill->vnum) < BASIC_SKILL_CAP) {
				set_skill(ch, skill->vnum, MIN(BASIC_SKILL_CAP, skill->level + 10));
			}
			else if (get_skill_level(ch, skill->vnum) < SPECIALTY_SKILL_CAP) {
				set_skill(ch, skill->vnum, MIN(SPECIALTY_SKILL_CAP, skill->level + 10));
			}
			else {
				set_skill(ch, skill->vnum, MIN(MAX_SKILL_CAP, skill->level + 10));
			}
		}
	}
}
