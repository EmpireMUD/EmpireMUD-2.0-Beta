/* ************************************************************************
*   File: db.player.c                                     EmpireMUD 2.0b5 *
*  Usage: Database functions related to players and the player table      *
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
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "skills.h"
#include "interpreter.h"
#include "dg_scripts.h"
#include "vnums.h"

/**
* Contents:
*   Getters
*   Account DB
*   Core Player DB
*   Autowiz Wizlist Generator
*   Helpers
*   Empire Player Management
*   Promo Codes
*/

#define LOG_BAD_TAG_WARNINGS  TRUE	// triggers syslogs for invalid pfile tags

// external vars
extern struct attribute_data_type attributes[NUM_ATTRIBUTES];
extern const char *condition_types[];
extern const char *custom_color_types[];
extern const char *extra_attribute_types[];
extern const char *genders[];
extern const struct material_data materials[NUM_MATERIALS];
extern const char *pool_types[];
extern int top_account_id;
extern int top_idnum;

// external funcs
ACMD(do_slash_channel);
void update_class(char_data *ch);

// local protos
void clear_player(char_data *ch);
void delete_player_character(char_data *ch);
static bool member_is_timed_out(time_t created, time_t last_login, double played_hours);
char_data *read_player_from_file(FILE *fl, char *name, bool normal, char_data *ch);
int sort_players_by_idnum(player_index_data *a, player_index_data *b);
int sort_players_by_name(player_index_data *a, player_index_data *b);
void write_player_delayed_data_to_file(FILE *fl, char_data *ch);
void write_player_primary_data_to_file(FILE *fl, char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// GETTERS /////////////////////////////////////////////////////////////////

/**
* This has the same purpose as get_player_vis_or_file, but won't screw anything
* up if the target is online but invisible. You must call store_loaded_char()
* if is_file == TRUE, or the player won't be stored. If you do NOT wish to save
* the character, use free_char() instead.
*
* @param char *name The player name
* @param bool *is_file A place to store whether or not we loaded from file
* @return char_data *ch or NULL
*/
char_data *find_or_load_player(char *name, bool *is_file) {
	char buf[MAX_INPUT_LENGTH+2];
	player_index_data *index;
	char_data *ch = NULL;
	
	*is_file = FALSE;
	
	if ((index = find_player_index_by_name(name))) {
		if (!(ch = is_playing(index->idnum))) {
			if ((ch = load_player(index->name, TRUE))) {
				SET_BIT(PLR_FLAGS(ch), PLR_KEEP_LAST_LOGIN_INFO);
				*is_file = TRUE;
			}
		}
	}
	
	// not able to find -- look for a player partial match?
	if (!ch) {
		sprintf(buf, "0.%s", name);	// add 0. to force player match
		ch = get_char_world(buf);
		*is_file = FALSE;
		if (ch && IS_NPC(ch)) {
			ch = NULL;	// verify player only
		}
	}
	
	return ch;
}


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
	
	LL_FOREACH2(ROOM_PEOPLE(room), ch, next_in_room) {
		if (!IS_NPC(ch) && GET_IDNUM(ch) == id && !EXTRACTED(ch)) {
			return ch;
		}
	}
	return NULL;
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

	for (ch = character_list; ch; ch = ch->next) {
		if (!IS_NPC(ch) && GET_IDNUM(ch) == id && !EXTRACTED(ch)) {
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
	int sort_accounts(account_data *a, account_data *b);
	
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
		if ((pos = acct->players)) {
			while (pos->next) {
				pos = pos->next;
			}
			pos->next = plr;
		}
		else {
			acct->players = plr;
		}
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
	
	free(acct);
}


/**
* Reads in one account from a file and adds it to the table.
*
* @param FILE *fl The file to read from.
* @param int nr The id of the account.
*/
void parse_account(FILE *fl, int nr) {
	char err_buf[MAX_STRING_LENGTH], line[256], str_in[256];
	struct account_player *plr, *last_plr = NULL;
	account_data *acct, *find;
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
			case 'P': {	// player
				if (sscanf(line, "P %s", str_in) == 1) {
					CREATE(plr, struct account_player, 1);
					plr->name = str_dup(str_in);
					strtolower(plr->name);	// ensure lowercase
					
					// add to end
					if (last_plr) {
						last_plr->next = plr;
					}
					else {
						acct->players = plr;
					}
					last_plr = plr;
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
	struct account_player *plr, *next_plr, *temp;
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
			REMOVE_FROM_LIST(plr, acct->players, next);
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
	
	if (!fl || !acct) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_account_to_file called without %s", !fl ? "file" : "account");
		return;
	}
	
	fprintf(fl, "#%d\n", acct->id);
	fprintf(fl, "%ld %s\n", acct->last_logon, bitv_to_alpha(acct->flags));

	strcpy(temp, NULLSAFE(acct->notes));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
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

// for more readable if/else chain
#define PFILE_TAG(src, tag, len)  (!strn_cmp((src), (tag), ((len) = strlen(tag))))
#define BAD_TAG_WARNING(src)  else if (LOG_BAD_TAG_WARNINGS) { log("SYSERR: Bad tag in player '%s': %s", NULLSAFE(GET_PC_NAME(ch)), (src)); }


/**
* Adds a player to the player tables (by_name and by_idnum), and sorts both
* tables.
*
* @param player_index_data *plr The player to add.
*/
void add_player_to_table(player_index_data *plr) {
	player_index_data *find;
	int idnum = plr->idnum;
	
	// by idnum
	find = NULL;
	HASH_FIND(idnum_hh, player_table_by_idnum, &idnum, sizeof(int), find);
	if (!find) {
		HASH_ADD(idnum_hh, player_table_by_idnum, idnum, sizeof(int), plr);
		HASH_SRT(idnum_hh, player_table_by_idnum, sort_players_by_idnum);
	}
	
	// by name: ensure name is lowercase
	find = NULL;
	strtolower(plr->name);	// ensure always lower
	HASH_FIND(name_hh, player_table_by_name, plr->name, strlen(plr->name), find);
	if (!find) {
		HASH_ADD(name_hh, player_table_by_name, name[0], strlen(plr->name), plr);
		HASH_SRT(name_hh, player_table_by_name, sort_players_by_name);
	}
}


/**
* Creates the player index by loading all players from the accounts. This must
* be run after accounts are loaded, but before the mud boots up.
*
* This also determines:
*   top_idnum
*   top_account_id
*/
void build_player_index(void) {
	struct account_player *plr, *next_plr, *temp;
	account_data *acct, *next_acct;
	player_index_data *index;
	bool has_players;
	char_data *ch;
	
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
					REMOVE_FROM_LIST(plr, acct->players, next);
					if (plr->name) {
						free(plr->name);
					}
					free(plr);
					continue;
				}
				
				has_players = TRUE;
				
				GET_ACCOUNT(ch) = acct;	// not set by load_player
				
				CREATE(index, player_index_data, 1);
				update_player_index(index, ch);
				add_player_to_table(index);
				plr->player = index;
				
				// detect top idnum
				top_idnum = MAX(top_idnum, GET_IDNUM(ch));
				
				// unload character
				free_char(ch);
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
}


/**
* Loads the rest of a player's data from the delayed-load file, if they have
* an open file. If they don't have one, it just returns.
*
* @param char_data *ch The player to finish loading.
*/
void check_delayed_load(char_data *ch) {
	void update_reputations(char_data *ch);
	
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
}


/* release memory allocated for a char struct */
void free_char(char_data *ch) {
	void die_follower(char_data *ch);
	void free_alias(struct alias_data *a);
	void free_mail(struct mail_data *mail);
	void free_player_completed_quests(struct player_completed_quest **hash);
	void free_player_quests(struct player_quest *list);

	struct slash_channel *loadslash, *next_loadslash;
	struct player_ability_data *abil, *next_abil;
	struct player_skill_data *skill, *next_skill;
	struct mount_data *mount, *next_mount;
	struct channel_history_data *history;
	struct player_slash_channel *slash;
	struct player_slash_history *slash_hist, *next_slash_hist;
	struct interaction_item *interact;
	struct pursuit_data *purs;
	struct offer_data *offer;
	struct lore_data *lore;
	struct coin_data *coin;
	struct mail_data *mail;
	struct alias_data *a;
	char_data *proto;
	obj_data *obj;
	int iter;

	// in case somehow?
	if (GROUP(ch)) {
		leave_group(ch);
	}
	
	// clean up gear/items, if any
	extract_all_items(ch);

	if (ch->followers || ch->master) {
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
		if (GET_LASTNAME(ch)) {
			free(GET_LASTNAME(ch));
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
		
		free_resource_list(GET_ACTION_RESOURCES(ch));
		
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
			while ((history = GET_HISTORY(ch, iter))) {
				GET_HISTORY(ch, iter) = history->next;
				if (history->message) {
					free(history->message);
				}
				free(history);
			}
		}
		HASH_ITER(hh, GET_SLASH_HISTORY(ch), slash_hist, next_slash_hist) {
			while ((history = slash_hist->history)) {
				slash_hist->history = history->next;
				if (history->message) {
					free(history->message);
				}
				free(history);
			}
			free(slash_hist);
		}
		
		while ((a = GET_ALIASES(ch)) != NULL) {
			GET_ALIASES(ch) = (GET_ALIASES(ch))->next;
			free_alias(a);
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
		
		while ((mail = GET_MAIL_PENDING(ch))) {
			GET_MAIL_PENDING(ch) = mail->next;
			free_mail(mail);
		}
		
		HASH_ITER(hh, GET_SKILL_HASH(ch), skill, next_skill) {
			HASH_DEL(GET_SKILL_HASH(ch), skill);
			free(skill);
		}
		HASH_ITER(hh, GET_ABILITY_HASH(ch), abil, next_abil) {
			HASH_DEL(GET_ABILITY_HASH(ch), abil);
			free(abil);
		}
		HASH_ITER(hh, GET_MOUNT_LIST(ch), mount, next_mount) {
			HASH_DEL(GET_MOUNT_LIST(ch), mount);
			free(mount);
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
	if (ch->proto_script && (!proto || ch->proto_script != proto->proto_script)) {
		free_proto_scripts(&ch->proto_script);
	}
	if (ch->interactions && (!proto || ch->interactions != proto->interactions)) {
		while ((interact = ch->interactions)) {
			ch->interactions = interact->next;
			free(interact);
		}
	}
	if (MOB_CUSTOM_MSGS(ch) && (!proto || MOB_CUSTOM_MSGS(ch) != MOB_CUSTOM_MSGS(proto))) {
		free_custom_messages(MOB_CUSTOM_MSGS(ch));
	}
	
	if (ch->desc) {
		ch->desc->character = NULL;
	}

	/* find_char helper */
	if (ch->script_id > 0) {
		remove_from_lookup_table(ch->script_id);
	}

	free(ch);
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
	void loaded_obj_to_char(obj_data *obj, char_data *ch, int location);
	extern obj_data *Obj_load_from_file(FILE *fl, obj_vnum vnum, int *location, char_data *notify);
	extern struct mail_data *parse_mail(FILE *fl, char *first_line);
	
	char line[MAX_INPUT_LENGTH], error[MAX_STRING_LENGTH], str_in[MAX_INPUT_LENGTH];
	int account_id = NOTHING, ignore_pos = 0, reward_pos = 0;
	struct lore_data *lore, *last_lore = NULL, *new_lore;
	struct over_time_effect_type *dot, *last_dot = NULL;
	struct affected_type *af, *next_af, *af_list = NULL;
	struct player_quest *plrq, *last_plrq = NULL;
	struct offer_data *offer, *last_offer = NULL;
	struct alias_data *alias, *last_alias = NULL;
	struct resource_data *res, *last_res = NULL;
	struct coin_data *coin, *last_coin = NULL;
	struct mail_data *mail, *last_mail = NULL;
	struct player_completed_quest *plrcom;
	struct player_ability_data *abildata;
	struct player_skill_data *skdata;
	int length, i_in[7], iter, num;
	struct slash_channel *slash;
	struct cooldown_data *cool;
	struct req_data *task;
	account_data *acct;
	bitvector_t bit_in;
	bool end = FALSE;
	obj_data *obj;
	double dbl_in;
	long l_in[2];
	char c_in;
	
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
	
	// We want to read in any old lore ahead of any we've appended already.
	// This happens if the player was loaded for an empire merge and new lore
	// was added before delayed data.
	new_lore = GET_LORE(ch);
	GET_LORE(ch) = NULL;
	
	// for fread_string
	sprintf(error, "read_player_from_file: %s", name);
	
	// for more readable if/else chain
	#define BAD_TAG_WARNING(src)  else if (LOG_BAD_TAG_WARNINGS) { log("SYSERR: Bad tag in player '%s': %s", NULLSAFE(GET_PC_NAME(ch)), (src)); }

	while (!end) {
		if (!get_line(fl, line)) {
			log("SYSERR: Unexpected end of player file in read_player_from_file: %s", name);
			exit(1);
		}
		
		// normal tags by letter
		switch (UPPER(*line)) {
			case '#': {	// an item
				sscanf(line, "#%d", &i_in[0]);
				if ((obj = Obj_load_from_file(fl, i_in[0], &i_in[1], ch->desc ? ch : NULL))) {
					loaded_obj_to_char(obj, ch, i_in[1]);
				}
				break;
			}
			case 'A': {
				if (PFILE_TAG(line, "Ability:", length)) {
					if (sscanf(line + length + 1, "%d %d %d %d", &i_in[0], &i_in[1], &i_in[2], &i_in[3]) == 4) {
						// post-b4.5 version
						if ((abildata = get_ability_data(ch, i_in[0], TRUE))) {
							abildata->levels_gained = i_in[1];
							// note: NUM_SKILL_SETS is not used for this, and 2 are assumed
							abildata->purchased[0] = i_in[2] ? TRUE : FALSE;
							abildata->purchased[1] = i_in[3] ? TRUE : FALSE;
						}
					}
					else if (sscanf(line + length + 1, "%d %d %d", &i_in[0], &i_in[1], &i_in[2]) == 3) {
						// backwards-compatibility
						if ((abildata = get_ability_data(ch, i_in[0], TRUE))) {
							abildata->purchased[0] = i_in[1] ? TRUE : FALSE;
							abildata->levels_gained = i_in[2];
						}
					}
				}
				else if (PFILE_TAG(line, "Access Level:", length)) {
					GET_ACCESS_LEVEL(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Account:", length)) {
					account_id = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Action:", length)) {
					if (sscanf(line + length + 1, "%d %d %d %d", &i_in[0], &i_in[1], &i_in[2], &i_in[3]) == 4) {
						GET_ACTION(ch) = i_in[0];
						GET_ACTION_CYCLE(ch) = i_in[1];
						GET_ACTION_TIMER(ch) = i_in[2];
						GET_ACTION_ROOM(ch) = i_in[3];
					}
				}
				else if (PFILE_TAG(line, "Action-res:", length)) {
					if (sscanf(line + length + 1, "%d %d %d %d", &i_in[0], &i_in[1], &i_in[2], &i_in[3]) == 4) {
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
				else if (PFILE_TAG(line, "Action-vnum:", length)) {
					if (sscanf(line + length + 1, "%d %d", &i_in[0], &i_in[1]) == 2) {
						if (i_in[0] < NUM_ACTION_VNUMS) {
							GET_ACTION_VNUM(ch, i_in[0]) = i_in[1];
						}
					}
				}
				else if (PFILE_TAG(line, "Adventure Summon Loc:", length)) {
					GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Adventure Summon Map:", length)) {
					GET_ADVENTURE_SUMMON_RETURN_MAP(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Affect:", length)) {
					sscanf(line + length + 1, "%d %d %d %d %d %s", &i_in[0], &i_in[1], &i_in[2], &i_in[3], &i_in[4], str_in);
					CREATE(af, struct affected_type, 1);
					af->type = i_in[0];
					af->cast_by = i_in[1];
					af->duration = i_in[2];
					af->modifier = i_in[3];
					af->location = i_in[4];
					af->bitvector = asciiflag_conv(str_in);
					
					// store for later
					LL_APPEND(af_list, af);
				}
				else if (PFILE_TAG(line, "Affect Flags:", length)) {
					AFF_FLAGS(ch) = asciiflag_conv(line + length + 1);
				}
				else if (PFILE_TAG(line, "Alias:", length)) {
					sscanf(line + length + 1, "%d %ld %ld", &i_in[0], &l_in[0], &l_in[1]);
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
				else if (PFILE_TAG(line, "Apparent Age:", length)) {
					GET_APPARENT_AGE(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Archetype:", length)) {
					// NOTE: This tag is outdated and these are now stored as 'Creation Archetype'
					CREATION_ARCHETYPE(ch, ARCHT_ORIGIN) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Attribute:", length)) {
					sscanf(line + length + 1, "%s %d", str_in, &i_in[0]);
					for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
						if (!str_cmp(str_in, attributes[iter].name)) {
							GET_REAL_ATT(ch, iter) = i_in[0];
							GET_ATT(ch, iter) = i_in[0];
							break;
						}
					}
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'B': {
				if (PFILE_TAG(line, "Bad passwords:", length)) {
					GET_BAD_PWS(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Birth:", length)) {
					ch->player.time.birth = atol(line + length + 1);
				}
				else if (PFILE_TAG(line, "Bonus Exp:", length)) {
					GET_DAILY_BONUS_EXPERIENCE(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Bonus Traits:", length)) {
					GET_BONUS_TRAITS(ch) = asciiflag_conv(line + length + 1);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'C': {
				if (PFILE_TAG(line, "Can Gain New Skills:", length)) {
					CAN_GAIN_NEW_SKILLS(ch) = atoi(line + length + 1) ? TRUE : FALSE;
				}
				else if (PFILE_TAG(line, "Can Get Bonus Skills:", length)) {
					CAN_GET_BONUS_SKILLS(ch) = atoi(line + length + 1) ? TRUE : FALSE;
				}
				else if (PFILE_TAG(line, "Class:", length)) {
					GET_CLASS(ch) = find_class_by_vnum(atoi(line + length + 1));
				}
				else if (PFILE_TAG(line, "Class Progression:", length)) {
					GET_CLASS_PROGRESSION(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Class Role:", length)) {
					GET_CLASS_ROLE(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Condition:", length)) {
					sscanf(line + length + 1, "%s %d", str_in, &i_in[0]);
					if ((num = search_block(str_in, condition_types, TRUE)) != NOTHING) {
						GET_COND(ch, num) = i_in[0];
					}
				}
				else if (PFILE_TAG(line, "Coin:", length)) {
					sscanf(line + length + 1, "%d %d %ld", &i_in[0], &i_in[1], &l_in[0]);
					
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
				else if (PFILE_TAG(line, "Confused Direction:", length)) {
					GET_CONFUSED_DIR(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Cooldown:", length)) {
					sscanf(line + length + 1, "%d %ld", &i_in[0], &l_in[0]);
					CREATE(cool, struct cooldown_data, 1);
					add_cooldown(ch, i_in[0], l_in[0] - time(0));
				}
				else if (PFILE_TAG(line, "Creation Archetype:", length)) {
					sscanf(line + length + 1, "%d %d", &i_in[0], &i_in[1]);
					if (i_in[0] >= 0 && i_in[0] < NUM_ARCHETYPE_TYPES) {
						CREATION_ARCHETYPE(ch, i_in[0]) = i_in[1];
					}
				}
				else if (PFILE_TAG(line, "Creation Host:", length)) {
					if (GET_CREATION_HOST(ch)) {
						free(GET_CREATION_HOST(ch));
					}
					GET_CREATION_HOST(ch) = str_dup(trim(line + length + 1));
				}
				else if (PFILE_TAG(line, "Current Pool:", length)) {
					sscanf(line + length + 1, "%s %d", str_in, &i_in[0]);
					if ((num = search_block(str_in, pool_types, TRUE)) != NOTHING) {
						GET_CURRENT_POOL(ch, num) = i_in[0];
					}
				}
				else if (PFILE_TAG(line, "Color:", length)) {
					sscanf(line + length + 1, "%s %c", str_in, &c_in);
					if ((num = search_block(str_in, custom_color_types, TRUE)) != NOTHING) {
						GET_CUSTOM_COLOR(ch, num) = c_in;
					}
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'D': {
				if (PFILE_TAG(line, "Daily Cycle:", length)) {
					GET_DAILY_CYCLE(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Daily Quests:", length)) {
					GET_DAILY_QUESTS(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Deficit:", length)) {
					sscanf(line + length + 1, "%s %d", str_in, &i_in[0]);
					if ((num = search_block(str_in, pool_types, TRUE)) != NOTHING) {
						GET_DEFICIT(ch, num) = i_in[0];
					}
				}
				else if (PFILE_TAG(line, "Description:", length)) {
					if (GET_LONG_DESC(ch)) {
						free(GET_LONG_DESC(ch));
					}
					GET_LONG_DESC(ch) = fread_string(fl, error);
				}
				else if (PFILE_TAG(line, "Disguised Name:", length)) {
					if (GET_DISGUISED_NAME(ch)) {
						free(GET_DISGUISED_NAME(ch));
					}
					GET_DISGUISED_NAME(ch) = str_dup(trim(line + length + 1));
				}
				else if (PFILE_TAG(line, "Disguised Sex:", length)) {
					if ((num = search_block(trim(line + length + 1), genders, TRUE)) != NOTHING) {
						GET_DISGUISED_SEX(ch) = num;
					}
				}
				else if (PFILE_TAG(line, "DoT Effect:", length)) {
					sscanf(line + length + 1, "%d %d %d %d %d %d %d", &i_in[0], &i_in[1], &i_in[2], &i_in[3], &i_in[4], &i_in[5], &i_in[6]);
					CREATE(dot, struct over_time_effect_type, 1);
					dot->type = i_in[0];
					dot->cast_by = i_in[1];
					dot->duration = i_in[2];
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
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'E': {
				if (PFILE_TAG(line, "Empire:", length)) {
					GET_LOYALTY(ch) = real_empire(atoi(line + length + 1));
				}
				else if (PFILE_TAG(line, "End Player File", length)) {
					// actual end
					end = TRUE;
				}
				else if (PFILE_TAG(line, "End Primary Data", length)) {
					// this tag is no longer used; ignore it
				}
				else if (PFILE_TAG(line, "Extra Attribute:", length)) {
					sscanf(line + length + 1, "%s %d", str_in, &i_in[0]);
					if ((num = search_block(str_in, extra_attribute_types, TRUE)) != NOTHING) {
						GET_EXTRA_ATT(ch, num) = i_in[0];
					}
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'F': {
				if (PFILE_TAG(line, "Faction:", length)) {
					struct player_faction_data *pfd;
					sscanf(line + length + 1, "%d %d", &i_in[0], &i_in[1]);
					if ((pfd = get_reputation(ch, i_in[0], TRUE))) {
						pfd->value = i_in[1];
					}
				}
				else if (PFILE_TAG(line, "Fight Messages:", length)) {
					GET_FIGHT_MESSAGES(ch) = asciiflag_conv(line + length + 1);
				}
				else if (PFILE_TAG(line, "Fight Prompt:", length)) {
					if (GET_FIGHT_PROMPT(ch)) {
						free(GET_FIGHT_PROMPT(ch));
					}
					GET_FIGHT_PROMPT(ch) = str_dup(line + length + 1);	// do not trim
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'G': {
				if (PFILE_TAG(line, "Grants:", length)) {
					GET_GRANT_FLAGS(ch) = asciiflag_conv(line + length + 1);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'H': {
				if (PFILE_TAG(line, "Highest Known Level:", length)) {
					GET_HIGHEST_KNOWN_LEVEL(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "History:", length)) {
					sscanf(line + length + 1, "%d %ld", &i_in[0], &l_in[1]);
					if (i_in[0] >= 0 && i_in[0] < NUM_CHANNEL_HISTORY_TYPES) {
						struct channel_history_data *hist;
						CREATE(hist, struct channel_history_data, 1);
						hist->timestamp = l_in[1];
						hist->message = fread_string(fl, error);
						LL_APPEND(GET_HISTORY(ch, i_in[0]), hist);
					}
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'I': {
				if (PFILE_TAG(line, "Idnum:", length)) {
					GET_IDNUM(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Ignore:", length)) {
					if (ignore_pos < MAX_IGNORES) {
						GET_IGNORE_LIST(ch, ignore_pos++) = atoi(line + length + 1);
					}
				}
				else if (PFILE_TAG(line, "Immortal Level:", length)) {
					GET_IMMORTAL_LEVEL(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Injuries:", length)) {
					INJURY_FLAGS(ch) = asciiflag_conv(line + length + 1);
				}
				else if (PFILE_TAG(line, "Invis Level:", length)) {
					GET_INVIS_LEV(ch) = atoi(line + length + 1);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'L': {
				if (PFILE_TAG(line, "Largest Inventory:", length)) {
					GET_LARGEST_INVENTORY(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Lastname:", length)) {
					if (GET_LASTNAME(ch)) {
						free(GET_LASTNAME(ch));
					}
					GET_LASTNAME(ch) = str_dup(trim(line + length + 1));
				}
				else if (PFILE_TAG(line, "Last Host:", length)) {
					if (ch->prev_host) {
						free(ch->prev_host);
					}
					ch->prev_host = str_dup(trim(line + length + 1));
				}
				else if (PFILE_TAG(line, "Last Known Level:", length)) {
					GET_LAST_KNOWN_LEVEL(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Last Logon:", length)) {
					ch->prev_logon = atol(line + length + 1);
				}
				else if (PFILE_TAG(line, "Last Tell:", length)) {
					GET_LAST_TELL(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Last Tip:", length)) {
					GET_LAST_TIP(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Last Room:", length)) {
					GET_LAST_ROOM(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Last Direction:", length)) {
					GET_LAST_DIR(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Last Death:", length)) {
					GET_LAST_DEATH_TIME(ch) = atol(line + length + 1);
				}
				else if (PFILE_TAG(line, "Last Corpse Id:", length)) {
					GET_LAST_CORPSE_ID(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Load Room:", length)) {
					GET_LOADROOM(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Load Room Check:", length)) {
					GET_LOAD_ROOM_CHECK(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Lore:", length)) {
					sscanf(line + length + 1, "%d %ld", &i_in[0], &l_in[0]);
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
				if (PFILE_TAG(line, "Mail", length)) {
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
				else if (PFILE_TAG(line, "Map Mark:", length)) {
					GET_MARK_LOCATION(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Mapsize:", length)) {
					GET_MAPSIZE(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Max Pool:", length)) {
					sscanf(line + length + 1, "%s %d", str_in, &i_in[0]);
					if ((num = search_block(str_in, pool_types, TRUE)) != NOTHING) {
						GET_MAX_POOL(ch, num) = i_in[0];
					}
				}
				else if (PFILE_TAG(line, "Morph:", length)) {
					GET_MORPH(ch) = morph_proto(atoi(line + length + 1));
				}
				else if (PFILE_TAG(line, "Mount:", length)) {
					sscanf(line + length + 1, "%d %s", &i_in[0], str_in);
					// only if mob exists
					if (mob_proto(i_in[0])) {
						add_mount(ch, i_in[0], asciiflag_conv(str_in));
					}
				}
				else if (PFILE_TAG(line, "Mount Flags:", length)) {
					GET_MOUNT_FLAGS(ch) = asciiflag_conv(line + length + 1);
				}
				else if (PFILE_TAG(line, "Mount Vnum:", length)) {
					GET_MOUNT_VNUM(ch) = atoi(line + length + 1);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'N': {
				if (PFILE_TAG(line, "Name:", length)) {
					if (GET_PC_NAME(ch)) {
						free(GET_PC_NAME(ch));
					}
					GET_PC_NAME(ch) = str_dup(trim(line + length + 1));
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'O': {
				if (PFILE_TAG(line, "Offer:", length)) {
					sscanf(line + length + 1, "%d %d %d %ld %d", &i_in[0], &i_in[1], &i_in[2], &l_in[0], &i_in[3]);
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
				else if (PFILE_TAG(line, "OLC:", length)) {
					sscanf(line + length + 1, "%d %d %s", &i_in[0], &i_in[1], str_in);
					GET_OLC_MIN_VNUM(ch) = i_in[0];
					GET_OLC_MAX_VNUM(ch) = i_in[1];
					GET_OLC_FLAGS(ch) = asciiflag_conv(str_in);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'P': {
				if (PFILE_TAG(line, "Password:", length)) {
					if (GET_PASSWD(ch)) {
						free(GET_PASSWD(ch));
					}
					GET_PASSWD(ch) = str_dup(line + length + 1);
				}
				else if (PFILE_TAG(line, "Played:", length)) {
					ch->player.time.played = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Player Flags:", length)) {
					PLR_FLAGS(ch) = asciiflag_conv(line + length + 1);
				}
				else if (PFILE_TAG(line, "Pledge Empire:", length)) {
					GET_PLEDGE(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Poofin:", length)) {
					if (POOFIN(ch)) {
						free(POOFIN(ch));
					}
					POOFIN(ch) = str_dup(line + length + 1);	// do not trim
				}
				else if (PFILE_TAG(line, "Poofout:", length)) {
					if (POOFOUT(ch)) {
						free(POOFOUT(ch));
					}
					POOFOUT(ch) = str_dup(line + length + 1);	// do not trim
				}
				else if (PFILE_TAG(line, "Preferences:", length)) {
					PRF_FLAGS(ch) = asciiflag_conv(line + length + 1);
				}
				else if (PFILE_TAG(line, "Promo ID:", length)) {
					GET_PROMO_ID(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Prompt:", length)) {
					if (GET_PROMPT(ch)) {
						free(GET_PROMPT(ch));
					}
					GET_PROMPT(ch) = str_dup(line + length + 1);	// do not trim
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'Q': {
				if (PFILE_TAG(line, "Quest:", length)) {
					if (sscanf(line + length + 1, "%d %d %ld %d %d", &i_in[0], &i_in[1], &l_in[0], &i_in[2], &i_in[3]) == 5) {
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
				else if (PFILE_TAG(line, "Quest-cmp:", length)) {
					if (sscanf(line + length + 1, "%d %ld %d %d", &i_in[0], &l_in[0], &i_in[1], &i_in[2]) == 4) {
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
				else if (PFILE_TAG(line, "Quest-task:", length)) {
					if (last_plrq && sscanf(line + length + 1, "%d %d %lld %d %d %c", &i_in[0], &i_in[1], &bit_in, &i_in[2], &i_in[3], &c_in) == 6) {
						// found group
					}
					else if (last_plrq && sscanf(line + length + 1, "%d %d %lld %d %d", &i_in[0], &i_in[1], &bit_in, &i_in[2], &i_in[3]) == 5) {
						c_in = 0;	// no group given
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
					task->group = c_in;
					
					LL_APPEND(last_plrq->tracker, task);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'R': {
				if (PFILE_TAG(line, "Rank:", length)) {
					GET_RANK(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Recent Deaths:", length)) {
					GET_RECENT_DEATH_COUNT(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Referred by:", length)) {
					if (GET_REFERRED_BY(ch)) {
						free(GET_REFERRED_BY(ch));
					}
					GET_REFERRED_BY(ch) = str_dup(trim(line + length + 1));
				}
				else if (PFILE_TAG(line, "Rent-code:", length)) {
					// old data; ignore
				}
				else if (PFILE_TAG(line, "Rent-time:", length)) {
					// old data; ignore
				}
				else if (PFILE_TAG(line, "Resource:", length)) {
					sscanf(line + length + 1, "%d %s", &i_in[0], str_in);
					for (iter = 0; iter < NUM_MATERIALS; ++iter) {
						if (!str_cmp(str_in, materials[iter].name)) {
							GET_RESOURCE(ch, iter) = i_in[0];
							break;
						}
					}
				}
				else if (PFILE_TAG(line, "Rewarded:", length)) {
					if (reward_pos < MAX_REWARDS_PER_DAY) {
						GET_REWARDED_TODAY(ch, reward_pos++) = atoi(line + length + 1);
					}
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'S': {
				if (PFILE_TAG(line, "Sex:", length)) {
					if ((num = search_block(trim(line + length + 1), genders, TRUE)) != NOTHING) {
						GET_REAL_SEX(ch) = num;
					}
				}
				else if (PFILE_TAG(line, "Skill:", length)) {
					sscanf(line + length + 1, "%d %d %lf %d %d", &i_in[0], &i_in[1], &dbl_in, &i_in[2], &i_in[3]);
					
					if ((skdata = get_skill_data(ch, i_in[0], TRUE))) {
						skdata->level = i_in[1];
						skdata->exp = dbl_in;
						skdata->resets = i_in[2];
						skdata->noskill = i_in[3];
					}
				}
				else if (PFILE_TAG(line, "Skill Level:", length)) {
					GET_SKILL_LEVEL(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Skill Set:", length)) {
					GET_CURRENT_SKILL_SET(ch) = atoi(line + length + 1);
					GET_CURRENT_SKILL_SET(ch) = MAX(0, GET_CURRENT_SKILL_SET(ch));
					GET_CURRENT_SKILL_SET(ch) = MIN(NUM_SKILL_SETS-1, GET_CURRENT_SKILL_SET(ch));
				}
				else if (PFILE_TAG(line, "Slash-channel:", length)) {
					CREATE(slash, struct slash_channel, 1);
					slash->name = str_dup(trim(line + length + 1));
					
					// append to start (it reverses them on-join anyway)
					slash->next = LOAD_SLASH_CHANNELS(ch);
					LOAD_SLASH_CHANNELS(ch) = slash;
				}
				else if (PFILE_TAG(line, "Slash-History:", length)) {
					sscanf(line + length + 1, "%s %ld", str_in, &l_in[1]);
					if (*str_in) {
						struct player_slash_history *psh;
						struct channel_history_data *hist;
						
						HASH_FIND_STR(GET_SLASH_HISTORY(ch), str_in, psh);
						if (!psh) {
							CREATE(psh, struct player_slash_history, 1);
							psh->channel = str_dup(str_in);
							HASH_ADD_STR(GET_SLASH_HISTORY(ch), channel, psh);
						}
						
						CREATE(hist, struct channel_history_data, 1);
						hist->timestamp = l_in[1];
						hist->message = fread_string(fl, error);
						LL_APPEND(psh->history, hist);
					}
				}
				else if (PFILE_TAG(line, "Syslog Flags:", length)) {
					SYSLOG_FLAGS(ch) = asciiflag_conv(line + length + 1);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'T': {
				if (PFILE_TAG(line, "Temporary Account:", length)) {
					GET_TEMPORARY_ACCOUNT_ID(ch) = atoi(line + length + 1);
				}
				else if (PFILE_TAG(line, "Title:", length)) {
					if (GET_TITLE(ch)) {
						free(GET_TITLE(ch));
					}
					GET_TITLE(ch) = str_dup(line + length + 1);	// do not trim
				}
				else if (PFILE_TAG(line, "Tomb Room:", length)) {
					GET_TOMB_ROOM(ch) = atoi(line + length + 1);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'U': {
				if (PFILE_TAG(line, "Using Poison:", length)) {
					USING_POISON(ch) = atoi(line + length + 1);
				}
				BAD_TAG_WARNING(line);
				break;
			}
			case 'V': {
				if (PFILE_TAG(line, "Variable:", length)) {
					if (sscanf(line + length + 1, "%s %ld", str_in, &l_in[0]) != 2 || !get_line(fl, line)) {
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
	
	// apply affects
	LL_FOREACH_SAFE(af_list, af, next_af) {
		affect_to_char(ch, af);
		free(af);
	}
	
	// safety
	REMOVE_BIT(PLR_FLAGS(ch), PLR_EXTRACTED | PLR_DONTSET);
	
	// Players who have been out for 1 hour get a free restore
	RESTORE_ON_LOGIN(ch) = (((int) (time(0) - ch->prev_logon)) >= 1 * SECS_PER_REAL_HOUR);
	if (GET_LOYALTY(ch)) {
		REREAD_EMPIRE_TECH_ON_LOGIN(ch) = (EMPIRE_MEMBERS(GET_LOYALTY(ch)) < 1 || member_is_timed_out(ch->player.time.birth, ch->prev_logon, ((double)ch->player.time.played) / SECS_PER_REAL_HOUR));
	}
	
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
	room_data *map;
	FILE *fl;

	if (IS_NPC(ch)) {
		return;
	}
	
	// update load room if they aren't flagged for a static one
	if (!PLR_FLAGGED(ch, PLR_LOADROOM)) {
		if (load_room) {
			GET_LOADROOM(ch) = GET_ROOM_VNUM(load_room);
			map = get_map_location_for(load_room);
			GET_LOAD_ROOM_CHECK(ch) = (map ? GET_ROOM_VNUM(map) : NOWHERE);
		}
		else {
			GET_LOADROOM(ch) = NOWHERE;
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
	if (!NEEDS_DELAYED_LOAD(ch)) {
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
	
	// update the index in case any of this changed
	index = find_player_index_by_idnum(GET_IDNUM(ch));
	update_player_index(index, ch);
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
	index->birth = ch->player.time.birth;
	index->played = ch->player.time.played;
	index->access_level = GET_ACCESS_LEVEL(ch);
	index->plr_flags = PLR_FLAGS(ch);
	index->loyalty = GET_LOYALTY(ch);
	index->rank = GET_RANK(ch);
	
	if (ch->desc || ch->prev_host) {
		if (index->last_host) {
			free(index->last_host);
		}
		index->last_host = str_dup(ch->desc ? ch->desc->host : ch->prev_host);
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
	void Crash_save(obj_data *obj, FILE *fp, int location);
	extern struct slash_channel *find_slash_channel_by_id(int id);
	void write_mail_to_file(FILE *fl, char_data *ch);
	
	struct affected_type *af, *new_af, *next_af, *af_list;
	struct player_ability_data *abil, *next_abil;
	struct player_skill_data *skill, *next_skill;
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
	
	if (!fl || !ch) {
		log("SYSERR: write_player_primary_data_to_file called without %s", fl ? "character" : "file");
		return;
	}
	if (IS_NPC(ch)) {
		log("SYSERR: write_player_primary_data_to_file called with NPC");
		return;
	}
	
	// save these for later, as they are sometimes changed by removing and re-adding gear
	for (iter = 0; iter < NUM_POOLS; ++iter) {
		deficit[iter] = GET_DEFICIT(ch, iter);
		pool[iter] = GET_CURRENT_POOL(ch, iter);
	}
	
	// unaffect the character to store raw numbers: equipment
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter)) {
			char_eq[iter] = unequip_char(ch, iter);
			#ifndef NO_EXTRANEOUS_TRIGGERS
				remove_otrigger(char_eq[iter], ch);
			#endif
		}
		else {
			char_eq[iter] = NULL;
		}
	}
	
	// unaffect: affects
	af_list = NULL;
	while ((af = ch->affected)) {
		if (af->type > ATYPE_RESERVED && af->type < NUM_ATYPES) {
			CREATE(new_af, struct affected_type, 1);
			*new_af = *af;
			new_af->next = af_list;
			af_list = new_af;
		}
		affect_remove(ch, af);
	}
	
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
		ch->player.time.played += (int)(time(0) - ch->player.time.logon);
		ch->player.time.logon = time(0);	// reset this first
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
		fprintf(fl, "Action: %d %d %d %d\n", GET_ACTION(ch), GET_ACTION_CYCLE(ch), GET_ACTION_TIMER(ch), GET_ACTION_ROOM(ch));
		for (iter = 0; iter < NUM_ACTION_VNUMS; ++iter) {
			fprintf(fl, "Action-vnum: %d %d\n", iter, GET_ACTION_VNUM(ch, iter));
		}
		LL_FOREACH(GET_ACTION_RESOURCES(ch), res) {
			// argument order is for consistency with other resource lists
			fprintf(fl, "Action-res: %d %d %d %d\n", res->vnum, res->amount, res->type, res->misc);
		}
	}
	if (GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch) != NOWHERE) {
		fprintf(fl, "Adventure Summon Loc: %d\n", GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch));
		fprintf(fl, "Adventure Summon Map: %d\n", GET_ADVENTURE_SUMMON_RETURN_MAP(ch));
	}
	for (af = af_list; af; af = af->next) {	// stored earlier
		fprintf(fl, "Affect: %d %d %d %d %d %s\n", af->type, af->cast_by, af->duration, af->modifier, af->location, bitv_to_alpha(af->bitvector));
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
	if (CAN_GET_BONUS_SKILLS(ch)) {
		fprintf(fl, "Can Get Bonus Skills: 1\n");
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
	
	// 'D'
	fprintf(fl, "Daily Cycle: %d\n", GET_DAILY_CYCLE(ch));
	fprintf(fl, "Daily Quests: %d\n", GET_DAILY_QUESTS(ch));
	if (GET_LONG_DESC(ch)) {
		strcpy(temp, NULLSAFE(GET_LONG_DESC(ch)));
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
		fprintf(fl, "DoT Effect: %d %d %d %d %d %d %d\n", dot->type, dot->cast_by, dot->duration, dot->damage_type, dot->damage, dot->stack, dot->max_stack);
	}
	
	// 'E'
	for (iter = 0; iter < NUM_EXTRA_ATTRIBUTES; ++iter) {
		if (GET_EXTRA_ATT(ch, iter)) {
			fprintf(fl, "Extra Attribute: %s %d\n", extra_attribute_types[iter], GET_EXTRA_ATT(ch, iter));
		}
	}
	
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
	fprintf(fl, "Largest Inventory: %d\n", GET_LARGEST_INVENTORY(ch));
	if (GET_LAST_CORPSE_ID(ch) > 0) {
		fprintf(fl, "Last Corpse Id: %d\n", GET_LAST_CORPSE_ID(ch));
	}
	fprintf(fl, "Last Death: %ld\n", GET_LAST_DEATH_TIME(ch));
	fprintf(fl, "Last Direction: %d\n", GET_LAST_DIR(ch));
	fprintf(fl, "Last Known Level: %d\n", GET_LAST_KNOWN_LEVEL(ch));
	fprintf(fl, "Last Room: %d\n", GET_LAST_ROOM(ch));
	if (GET_LAST_TELL(ch) != NOBODY) {
		fprintf(fl, "Last Tell: %d\n", GET_LAST_TELL(ch));
	}
	if (GET_LAST_TIP(ch)) {
		fprintf(fl, "Last Tip: %d\n", GET_LAST_TIP(ch));
	}
	if (GET_LASTNAME(ch)) {
		fprintf(fl, "Lastname: %s\n", GET_LASTNAME(ch));
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
	for (iter = 0; iter < MAX_REWARDS_PER_DAY; ++iter) {
		if (GET_REWARDED_TODAY(ch, iter) > 0) {
			fprintf(fl, "Rewarded: %d\n", GET_REWARDED_TODAY(ch, iter));
		}
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
	if (SYSLOG_FLAGS(ch)) {
		fprintf(fl, "Syslog Flags: %s\n", bitv_to_alpha(SYSLOG_FLAGS(ch)));
	}
	
	// 'T'
	if (GET_TITLE(ch)) {
		fprintf(fl, "Title: %s\n", GET_TITLE(ch));
	}
	if (GET_TOMB_ROOM(ch) != NOWHERE) {
		fprintf(fl, "Tomb Room: %d\n", GET_TOMB_ROOM(ch));
	}
	
	// 'U'
	if (USING_POISON(ch)) {
		fprintf(fl, "Using Poison: %d\n", USING_POISON(ch));
	}
	
	// END PLAYER FILE
	fprintf(fl, "End Player File\n");
	
	// re-apply: affects
	for (af = af_list; af; af = next_af) {
		next_af = af->next;
		affect_to_char(ch, af);
		free(af);
	}
	
	// re-apply: equipment
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (char_eq[iter]) {
			#ifndef NO_EXTRANEOUS_TRIGGERS
				if (wear_otrigger(char_eq[iter], ch, iter)) {
			#endif
					// this line may depend on the above if
					equip_char(ch, char_eq[iter], iter);
			#ifndef NO_EXTRANEOUS_TRIGGERS
				}
				else {
					obj_to_char(char_eq[iter], ch);
				}
			#endif
		}
	}
	
	// restore pools, which may have been modified
	for (iter = 0; iter < NUM_POOLS; ++iter) {
		GET_CURRENT_POOL(ch, iter) = pool[iter];
		GET_DEFICIT(ch, iter) = deficit[iter];
	}
	
	// affect_total(ch); // unnecessary, I think (?)
}


/**
* Writes all the DELAYED data for one player to file. This is saved separately
* in order to reduce loading in many cases.
*
* @param FILE *fl The open file to write to.
* @param char_data *ch The player to write.
*/
void write_player_delayed_data_to_file(FILE *fl, char_data *ch) {
	void Crash_save(obj_data *obj, FILE *fp, int location);
	extern struct slash_channel *find_slash_channel_by_id(int id);
	void write_mail_to_file(FILE *fl, char_data *ch);
	
	struct player_completed_quest *plrcom, *next_plrcom;
	struct player_slash_history *psh, *next_psh;
	struct player_faction_data *pfd, *next_pfd;
	struct channel_history_data *hist;
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
	
	// 'C'
	for (coin = GET_PLAYER_COINS(ch); coin; coin = coin->next) {
		fprintf(fl, "Coin: %d %d %ld\n", coin->amount, coin->empire_id, coin->last_acquired);
	}
	
	// 'F'
	HASH_ITER(hh, GET_FACTIONS(ch), pfd, next_pfd) {
		fprintf(fl, "Faction: %d %d\n", pfd->vnum, pfd->value);
	}
	
	// 'H'
	for (iter = 0; iter < NUM_CHANNEL_HISTORY_TYPES; ++iter) {
		LL_FOREACH(GET_HISTORY(ch, iter), hist) {
			fprintf(fl, "History: %d %ld\n%s~\n", iter, hist->timestamp, NULLSAFE(hist->message));
		}
	}
	
	// 'L'
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
			fprintf(fl, "Quest-task: %d %d %lld %d %d %c\n", task->type, task->vnum, task->misc, task->needed, task->current, task->group);
		}
	}
	HASH_ITER(hh, GET_COMPLETED_QUESTS(ch), plrcom, next_plrcom) {
		fprintf(fl, "Quest-cmp: %d %ld %d %d\n", plrcom->vnum, plrcom->last_completed, plrcom->last_instance_id, plrcom->last_adventure);
	}
	
	// 'S'
	HASH_ITER(hh, GET_SLASH_HISTORY(ch), psh, next_psh) {
		LL_FOREACH(psh->history, hist) {
			fprintf(fl, "Slash-History: %s %ld\n%s~\n", psh->channel, hist->timestamp, NULLSAFE(hist->message));
		}
	}
	
	// 'V'
	if (SCRIPT(ch) && SCRIPT(ch)->global_vars) {
		for (vars = SCRIPT(ch)->global_vars; vars; vars = vars->next) {
			if (*vars->name == '-') { // don't save if it begins with -
				continue;
			}
			
			fprintf(fl, "Variable: %s %ld\n%s\n", vars->name, vars->context, vars->value);
		}
	}
	
	// '#'
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter)) {
			Crash_save(GET_EQ(ch, iter), fl, iter + 1);	// save at iter+1 because 0 == LOC_INVENTORY
		}
	}
	Crash_save(ch->carrying, fl, LOC_INVENTORY);
	
	// END DELAY-LOADED SECTION
	fprintf(fl, "End Player File\n");
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

const char *AUTOWIZ_IMM_LMARG = "   ";
const int AUTOWIZ_IMM_NSIZE = 16;
const int AUTOWIZ_LINE_LEN = 64;
const int AUTOWIZ_MIN_LEVEL = LVL_GOD;

// max level that should be in columns instead of centered
const int AUTOWIZ_COL_LEVEL = LVL_GOD;

const char *autowiz_header =
"*************************************************************************\n"
"* The following people have reached immortality on EmpireMUD.  They are *\n"
"* to be treated with respect and awe.  Occasional prayers to them are   *\n"
"* advisable.  Annoying them is not recommended.  Stealing from them is  *\n"
"* punishable by immediate death.                                        *\n"
"*************************************************************************\n";


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
		tmp->next = autowiz_data;
		autowiz_data = tmp;
	}
}


/**
* Adds a name to autowiz_data, in the correct level.
*
* @param int level The level of the player.
* @param char *name The player's name.
*/
void autowiz_add_name(int level, char *name) {
	struct autowiz_name_rec *tmp, *end;
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
	while (curr_level->params->level > level)
		curr_level = curr_level->next;
	
	// attempt to append at end
	if ((end = curr_level->names)) {
		while (end->next) {
			end = end->next;
		}
		end->next = tmp;
	}
	else {
		curr_level->names = tmp;
	}
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
* @param FILE *out The file open for writing.
* @param int minlev Minimum level to write to this file.
* @param int maxlev Maximum level to write to this file.
*/
void autowiz_write_wizlist(FILE *out, int minlev, int maxlev) {
	char buf[MAX_STRING_LENGTH];
	struct autowiz_level_rec *curr_level;
	struct autowiz_name_rec *curr_name;
	int i, j;
	
	fprintf(out, "%s\n", autowiz_header);
	
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
	extern int file_to_string_alloc(const char *name, char **buf);
	extern char *wizlist, *godlist;	// db.c

	file_to_string_alloc(WIZLIST_FILE, &wizlist);
	file_to_string_alloc(GODLIST_FILE, &godlist);
}


/**
* Builds and generates the wizlist and godlist files, and reloads them.
*/
void run_autowiz(void) {	
	FILE *fl;
	
	syslog(SYS_INFO, LVL_START_IMM, TRUE, "Rebuilding wizlists");
	
	autowiz_initialize();
	autowiz_read_players();
	
	if (!(fl = fopen(WIZLIST_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: run_autowiz: Unable to open file %s for writing\r\n", WIZLIST_FILE TEMP_SUFFIX);
		return;
	}
	autowiz_write_wizlist(fl, LVL_START_IMM, LVL_TOP);
	fclose(fl);
	rename(WIZLIST_FILE TEMP_SUFFIX, WIZLIST_FILE);
	
	if (!(fl = fopen(GODLIST_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: run_autowiz: Unable to open file %s for writing\r\n", GODLIST_FILE TEMP_SUFFIX);
		return;
	}
	autowiz_write_wizlist(fl, LVL_GOD, LVL_START_IMM - 1);
	fclose(fl);
	rename(GODLIST_FILE TEMP_SUFFIX, GODLIST_FILE);
	
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
	
	syslog(SYS_LOGIN, GET_INVIS_LEV(ch), TRUE, "%s has entered the game", GET_NAME(ch));
	
	// auto-notes
	if (GET_ACCOUNT(ch) && GET_ACCOUNT(ch)->notes) {
		LL_FOREACH(descriptor_list, desc) {
			if (STATE(desc) != CON_PLAYING || !desc->character) {
				continue;
			}
			if (!IS_IMMORTAL(desc->character) || !PRF_FLAGGED(desc->character, PRF_AUTONOTES)) {
				continue;
			}
			if (GET_ACCESS_LEVEL(desc->character) < GET_ACCESS_LEVEL(ch)) {
				continue;
			}
			
			snprintf(buf, sizeof(buf), "(%s account notes)", GET_NAME(ch));
			msg_to_char(desc->character, "%s- %s ", GET_ACCOUNT(ch)->notes, buf);
			
			// rest of the divider
			for (iter = 0; iter < 79 - (3 + strlen(buf)); ++iter) {
				buf2[iter] = '-';
			}
			buf2[iter] = '\0';	// terminate
			msg_to_char(desc->character, "%s\r\n", buf2);
		}
	}
	
	// mortlog
	if (GET_INVIS_LEV(ch) == 0) {
		if (config_get_bool("public_logins") && !PLR_FLAGGED(ch, PLR_NEEDS_NEWBIE_SETUP)) {
			mortlog("%s has entered the game", PERS(ch, ch, TRUE));
		}
		else if (GET_LOYALTY(ch)) {
			log_to_empire(GET_LOYALTY(ch), ELOG_LOGINS, "%s has entered the game", PERS(ch, ch, TRUE));
		}
	}
}


/**
* Ensures that all of a player's skills and abilities exist, and updates their
* class. This should be called on login.
*/
void check_skills_and_abilities(char_data *ch) {
	void check_ability_levels(char_data *ch, any_vnum skill);
	
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
	update_class(ch);
	check_ability_levels(ch, NOTHING);
}


/**
* Deletes chat messages that are more than 24 hours old.
*
* @param char_data *ch The person whose history to clean.
*/
void clean_old_history(char_data *ch) {
	struct channel_history_data *hist, *next_hist;
	struct player_slash_history *psh, *next_psh;
	long now = time(0);
	int iter;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	for (iter = 0; iter < NUM_CHANNEL_HISTORY_TYPES; ++iter) {
		LL_FOREACH_SAFE(GET_HISTORY(ch, iter), hist, next_hist) {
			if (hist->timestamp + (60 * 60 * 24) < now) {
				// 24 hours old
				if (hist->message) {
					free(hist->message);
				}
				LL_DELETE(GET_HISTORY(ch, iter), hist);
				free(hist);
			}
		}
	}
	
	HASH_ITER(hh, GET_SLASH_HISTORY(ch), psh, next_psh) {
		LL_FOREACH_SAFE(psh->history, hist, next_hist) {
			if (hist->timestamp + (60 * 60 * 24) < now) {
				// 24 hours old
				if (hist->message) {
					free(hist->message);
				}
				LL_DELETE(psh->history, hist);
				free(hist);
			}
		}
		
		// no more history
		if (!psh->history) {
			if (psh->channel) {
				free(psh->channel);
			}
			HASH_DEL(GET_SLASH_HISTORY(ch), psh);
			free(psh);
		}
	}
}


/**
* Clears certain player data, similar to clear_char() -- but not for NPCS.
*
* @param char_data *ch The player charater to clear.
*/
void clear_player(char_data *ch) {
	int iter;
	
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
	GET_LAST_TELL(ch) = NOBODY;
	GET_TEMPORARY_ACCOUNT_ID(ch) = NOTHING;
	GET_IMMORTAL_LEVEL(ch) = -1;	// Not an immortal
	
	for (iter = 0; iter < MAX_REWARDS_PER_DAY; ++iter) {
		GET_REWARDED_TODAY(ch, iter) = -1;
	}
}


/**
* This runs at startup (if you don't use -q) and deletes players who are
* timed out according to the delete_invalid_players_after and 
* delete_inactive_players_after configs. This can be prevented with the
* NODELETE flag. Immortals are also never deleted this way.
*/
void delete_old_players(void) {
	player_index_data *index, *next_index;
	bool file, will_delete;
	char_data *ch;
	
	int delete_inactive = config_get_int("delete_inactive_players_after");
	int delete_invalid = config_get_int("delete_invalid_players_after");
	
	HASH_ITER(name_hh, player_table_by_name, index, next_index) {
		// imms immune
		if (index->access_level >= LVL_START_IMM && index->access_level <= LVL_TOP) {
			continue;
		}
		// never!
		if (IS_SET(index->plr_flags, PLR_NODELETE)) {
			continue;
		}
		
		will_delete = FALSE;
		
		// delete #1: invalid players
		if (delete_invalid > 0 && (index->access_level <= 0 || index->access_level > LVL_TOP) && (index->last_logon + (delete_invalid * SECS_PER_REAL_DAY)) < time(0)) {
			will_delete = TRUE;
		}
		// delete #2: inactive players
		else if (delete_inactive > 0 && (index->last_logon + (delete_inactive * SECS_PER_REAL_DAY)) < time(0)) {
			will_delete = TRUE;
		}
		
		// attempt the delete
		if (will_delete && (ch = find_or_load_player(index->name, &file))) {
			if (!file) {
				// EXTREMELY unlikely: they may be deletable but are actually in-game, so we skip them
				continue;
			}
			
			delete_player_character(ch);
			free_char(ch);
		}
	}
}


/**
* Function to DELETE A PLAYER.
*
* @param char_data *ch The player to delete.
*/
void delete_player_character(char_data *ch) {
	void clear_private_owner(int id);
	
	player_index_data *index;
	empire_data *emp = NULL;
	char filename[256];
	
	if (IS_NPC(ch)) {
		syslog(SYS_ERROR, 0, TRUE, "SYSERR: delete_player_character called on NPC");
		return;
	}
	
	clear_private_owner(GET_IDNUM(ch));

	// Check the empire
	if ((emp = GET_LOYALTY(ch)) != NULL) {
		GET_LOYALTY(ch) = NULL;
		GET_RANK(ch) = 0;
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
	if (get_filename(GET_NAME(ch), filename, PLR_FILE)) {
		if (remove(filename) < 0 && errno != ENOENT) {
			log("SYSERR: deleting player file %s: %s", filename, strerror(errno));
		}
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
	void assign_class_abilities(char_data *ch, class_data *cls, int role);
	void check_delayed_load(char_data *ch);
	void clean_lore(char_data *ch);
	extern room_data *find_home(char_data *ch);
	extern room_data *find_load_room(char_data *ch);
	void give_level_zero_abilities(char_data *ch);
	void msdp_update_room(char_data *ch);
	void refresh_all_quests(char_data *ch);
	void reset_combat_meters(char_data *ch);
	
	extern bool global_mute_slash_channel_joins;

	struct slash_channel *load_slash, *next_slash, *temp;
	bool stop_action = FALSE, try_home = FALSE;
	room_data *load_room = NULL, *map_loc;
	char_data *ch = d->character, *repl;
	char lbuf[MAX_STRING_LENGTH];
	struct affected_type *af;
	player_index_data *index;
	empire_data *emp;
	int iter, duration;

	reset_char(ch);
	check_delayed_load(ch);	// ensure everything is loaded
	clean_old_history(ch);
	reset_combat_meters(ch);
	GET_COMBAT_METERS(ch).over = TRUE;	// ensure no active meter
	
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
		GET_IDNUM(ch) = ++top_idnum;
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
			map_loc = get_map_location_for(load_room);
			if (GET_LOAD_ROOM_CHECK(ch) == NOWHERE || !map_loc || GET_ROOM_VNUM(map_loc) != GET_LOAD_ROOM_CHECK(ch)) {
				// ensure they are on the same continent they used to be when it finds them a new loadroom
				GET_LAST_ROOM(ch) = GET_LOAD_ROOM_CHECK(ch);
				load_room = NULL;
				stop_action = TRUE;
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
	ch->next = character_list;
	character_list = ch;
	ch->script_id = GET_IDNUM(ch);
	add_to_lookup_table(ch->script_id, (void *)ch);
	
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
	
	SAVE_CHAR(ch);

	// re-join slash-channels
	global_mute_slash_channel_joins = TRUE;
	for (load_slash = LOAD_SLASH_CHANNELS(ch); load_slash; load_slash = next_slash) {
		next_slash = load_slash->next;
		if (load_slash->name && *load_slash->name) {
			sprintf(lbuf, "join %s", load_slash->name);
			do_slash_channel(ch, lbuf, 0, 0);
		}
		
		REMOVE_FROM_LIST(load_slash, LOAD_SLASH_CHANNELS(ch), next);
		if (load_slash->name) {
			free(load_slash->name);
		}
		if (load_slash->lc_name) {
			free(load_slash->lc_name);
		}
		free(load_slash);
	}
	global_mute_slash_channel_joins = FALSE;
	
	// free reset?
	if (RESTORE_ON_LOGIN(ch)) {
		GET_HEALTH(ch) = GET_MAX_HEALTH(ch);
		GET_MOVE(ch) = GET_MAX_MOVE(ch);
		GET_MANA(ch) = GET_MAX_MANA(ch);
		GET_COND(ch, FULL) = MIN(0, GET_COND(ch, FULL));
		GET_COND(ch, THIRST) = MIN(0, GET_COND(ch, THIRST));
		GET_COND(ch, DRUNK) = MIN(0, GET_COND(ch, DRUNK));
		GET_BLOOD(ch) = GET_MAX_BLOOD(ch);
		
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
	}
	else {
		// ensure not dead
		GET_HEALTH(ch) = MAX(1, GET_HEALTH(ch));
		GET_BLOOD(ch) = MAX(1, GET_BLOOD(ch));
	}
	
	// position must be reset
	if (AFF_FLAGGED(ch, AFF_EARTHMELD | AFF_MUMMIFY | AFF_DEATHSHROUD)) {
		GET_POS(ch) = POS_SLEEPING;
	}
	else {
		GET_POS(ch) = POS_STANDING;
	}
	
	// in some cases, we need to re-read tech when the character logs in
	if ((emp = GET_LOYALTY(ch))) {
		if (REREAD_EMPIRE_TECH_ON_LOGIN(ch)) {
			SAVE_CHAR(ch);
			reread_empire_tech(emp);
			REREAD_EMPIRE_TECH_ON_LOGIN(ch) = FALSE;
		}
		else {
			// reread members to adjust greatness
			read_empire_members(emp, FALSE);
		}
	}
	
	// remove stale coins
	cleanup_coins(ch);
	
	// verify abils -- TODO should this remove/re-add abilities for the empire? do class abilities affect that?
	assign_class_abilities(ch, NULL, NOTHING);
	give_level_zero_abilities(ch);
	
	if (!IS_IMMORTAL(ch)) {
		// ensure player has penalty if at war
		if (fresh && GET_LOYALTY(ch) && is_at_war(GET_LOYALTY(ch)) && (duration = config_get_int("war_login_delay") / SECS_PER_REAL_UPDATE) > 0) {
			af = create_flag_aff(ATYPE_WAR_DELAY, duration, AFF_IMMUNE_PHYSICAL | AFF_NO_ATTACK | AFF_STUNNED, ch);
			affect_join(ch, af, ADD_DURATION);
			msg_to_char(ch, "\trYou are stunned for %d second%s because your empire is at war.\r\n", duration, PLURAL(duration));
		}
		else if (fresh && IN_HOSTILE_TERRITORY(ch) && (duration = config_get_int("hostile_login_delay") / SECS_PER_REAL_UPDATE) > 0) {
			af = create_flag_aff(ATYPE_HOSTILE_DELAY, duration, AFF_IMMUNE_PHYSICAL | AFF_NO_ATTACK | AFF_STUNNED, ch);
			affect_join(ch, af, ADD_DURATION);
			msg_to_char(ch, "\trYou are stunned for %d second%s because you logged in in hostile territory.\r\n", duration, PLURAL(duration));
		}
	}

	// script/trigger stuff
	greet_mtrigger(ch, NO_DIR);
	greet_memory_mtrigger(ch);
	greet_vtrigger(ch, NO_DIR);
	
	// update the index in case any of this changed
	index = find_player_index_by_idnum(GET_IDNUM(ch));
	update_player_index(index, ch);
	
	// ensure quests are up-to-date
	refresh_all_quests(ch);
	
	// break last reply if invis
	if (GET_LAST_TELL(ch) && (repl = is_playing(GET_LAST_TELL(ch))) && (GET_INVIS_LEV(repl) > GET_ACCESS_LEVEL(ch) || (!IS_IMMORTAL(ch) && PRF_FLAGGED(repl, PRF_INCOGNITO)))) {
		GET_LAST_TELL(ch) = NOBODY;
	}
	
	msdp_update_room(ch);
	
	// now is a good time to save and be sure we have a good save file
	SAVE_CHAR(ch);
}


/**
* @param account_data *acct Which account to check.
* @return int The highest access level of any character on that account.
*/
int get_highest_access_level(account_data *acct) {
	struct account_player *plr;
	int highest = 0;
	
	LL_FOREACH(acct->players, plr) {
		highest = MAX(highest, plr->player->access_level);
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
	void scale_item_to_level(obj_data *obj, int level);
	extern const struct wear_data_type wear_data[NUM_WEARS];
	
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
}


/**
* This runs one-time setup on the player character, during their initial
* creation.
*
* @param char_data *ch The player to initialize.
*/
void init_player(char_data *ch) {
	extern const int base_player_pools[NUM_POOLS];
	extern const char *syslog_types[];
	
	player_index_data *index;
	int account_id = NOTHING;
	account_data *acct;
	bool first = FALSE;
	int i, iter;

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
	GET_PROMPT(ch) = NULL;
	GET_FIGHT_PROMPT(ch) = NULL;
	POOFIN(ch) = NULL;
	POOFOUT(ch) = NULL;
	
	ch->points.max_pools[BLOOD] = 10;	// not actually used for players
	ch->points.current_pools[BLOOD] = GET_MAX_BLOOD(ch);	// this is a function
	
	// assign idnum
	if (GET_IDNUM(ch) <= 0) {
		GET_IDNUM(ch) = ++top_idnum;
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

	for (i = 0; i < NUM_CONDS; i++) {
		GET_COND(ch, i) = (GET_ACCESS_LEVEL(ch) == LVL_IMPL ? UNLIMITED : 0);
	}
	
	// script allocation
	if (!SCRIPT(ch)) {
		create_script_data(ch, MOB_TRIGGER);
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
	ch->master = NULL;
	IN_ROOM(ch) = NULL;
	ch->next = NULL;
	ch->next_fighting = NULL;
	ch->next_in_room = NULL;
	FIGHTING(ch) = NULL;
	ch->char_specials.position = POS_STANDING;
	
	if (GET_MOVE(ch) <= 0) {
		GET_MOVE(ch) = 1;
	}
}


/**
* This saves all connected players.
*/
void save_all_players(void) {
	descriptor_data *desc;

	for (desc = descriptor_list; desc; desc = desc->next) {
		if ((STATE(desc) == CON_PLAYING) && !IS_NPC(desc->character)) {
			SAVE_CHAR(desc->character);
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


/**
* Some initializations for characters, including initial skills
*
* @param char_data *ch A new player
*/
void start_new_character(char_data *ch) {
	void add_archetype_lore(char_data *ch);
	void apply_bonus_trait(char_data *ch, bitvector_t trait, bool add);
	void make_vampire(char_data *ch, bool lore);
	void set_skill(char_data *ch, any_vnum skill, int level);
	extern const char *default_channels[];
	extern bool global_mute_slash_channel_joins;
	extern const int primary_attributes[];
	extern struct promo_code_list promo_codes[];
	extern int tips_of_the_day_size;
	
	char lbuf[MAX_INPUT_LENGTH];
	struct global_data *glb, *next_glb, *choose_last;
	int arch_iter, cumulative_prc, iter, level;
	bool done_cumulative = FALSE;
	struct archetype_gear *gear;
	struct archetype_skill *sk;
	archetype_data *arch;
	bool found;
	
	// announce to existing players that we have a newbie
	mortlog("%s has joined the game", PERS(ch, ch, TRUE));

	set_title(ch, NULL);

	/* Default Flags */
	SET_BIT(PRF_FLAGS(ch), PRF_MORTLOG);
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
	if (!IS_APPROVED(ch) && config_get_bool("auto_approve")) {
		// only approve the character automatically
		SET_BIT(PLR_FLAGS(ch), PLR_APPROVED);
	}
	
	GET_HEALTH(ch) = GET_MAX_HEALTH(ch);
	GET_MOVE(ch) = GET_MAX_MOVE(ch);
	GET_MANA(ch) = GET_MAX_MANA(ch);
	GET_BLOOD(ch) = GET_MAX_BLOOD(ch);

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
	
	SET_BIT(PRF_FLAGS(ch), PRF_AUTOKILL);
	
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
			level = MIN(level, CLASS_SKILL_CAP);
			set_skill(ch, sk->skill, level);
			
			// special case for vampire
			if (sk->skill == SKILL_VAMPIRE && !IS_VAMPIRE(ch)) {
				make_vampire(ch, TRUE);
				GET_BLOOD(ch) = GET_MAX_BLOOD(ch);
			}
		}
		
		// newbie eq -- don't run load triggers on this set, as ch is possibly not in a room
		for (gear = GET_ARCH_GEAR(arch); gear; gear = gear->next) {
			give_newbie_gear(ch, gear->vnum, gear->wear);
		}
	}
	
	// guarantee minimum of 1 for active attributes
	for (iter = 0; primary_attributes[iter] != NOTHING; ++iter) {
		if (ch->real_attributes[primary_attributes[iter]] < 1) {
			ch->real_attributes[primary_attributes[iter]] = 1;
		}
	}
	
	// global newbie gear -- TODO there must be a better, more generic way to run globals -pc 5/24/2016
	cumulative_prc = number(1, 10000);
	choose_last = NULL;
	found = FALSE;
	HASH_ITER(hh, globals_table, glb, next_glb) {
		if (GET_GLOBAL_TYPE(glb) != GLOBAL_NEWBIE_GEAR || IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT)) {
			continue;
		}
		if (GET_GLOBAL_ABILITY(glb) != NO_ABIL && !has_ability(ch, GET_GLOBAL_ABILITY(glb))) {
			continue;
		}
		
		// percent checks last
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CUMULATIVE_PERCENT)) {
			if (done_cumulative) {
				continue;
			}
			cumulative_prc -= (int)(GET_GLOBAL_PERCENT(glb) * 100);
			if (cumulative_prc <= 0) {
				done_cumulative = TRUE;
			}
			else {
				continue;	// not this time
			}
		}
		else if (number(1, 10000) > (int)(GET_GLOBAL_PERCENT(glb) * 100)) {
			// normal not-cumulative percent
			continue;
		}
	
		// success!
		
		// check choose-last
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CHOOSE_LAST)) {
			if (!choose_last) {
				choose_last = glb;
			}
			continue;
		}
		
		// safe to apply
		for (gear = GET_GLOBAL_GEAR(glb); gear; gear = gear->next) {
			give_newbie_gear(ch, gear->vnum, gear->wear);
		}
		found = TRUE;
	}
	// do the choose-last
	if (choose_last && !found) {
		for (gear = GET_GLOBAL_GEAR(choose_last); gear; gear = gear->next) {
			give_newbie_gear(ch, gear->vnum, gear->wear);
		}
	}
	
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
	update_class(ch);
	
	// prevent a repeat
	REMOVE_BIT(PLR_FLAGS(ch), PLR_NEEDS_NEWBIE_SETUP);
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE PLAYER MANAGEMENT ////////////////////////////////////////////////

// this is used to ensure each account only contributes once to the empire
struct empire_member_reader_data {
	empire_data *empire;
	int account_id;
	int greatness;
	
	struct empire_member_reader_data *next;
};


/**
* Add a given user's data to the account list of accounts on the empire member reader data
*
* @param struct empire_member_reader_data **list A pointer to the existing list.
* @param int empire which empire entry the player is in
* @param int account_id which account id the player has
* @param int greatness the player's greatness
*/
static void add_to_account_list(struct empire_member_reader_data **list, empire_data *empire, int account_id, int greatness) {
	struct empire_member_reader_data *emrd;
	bool found = FALSE;
	
	for (emrd = *list; emrd && !found; emrd = emrd->next) {
		if (emrd->empire == empire && emrd->account_id == account_id) {
			found = TRUE;
			emrd->greatness = MAX(emrd->greatness, greatness);
		}
	}
	
	if (!found) {
		CREATE(emrd, struct empire_member_reader_data, 1);
		emrd->empire = empire;
		emrd->account_id = account_id;
		emrd->greatness = greatness;
		
		emrd->next = *list;
		*list = emrd;
	}
}


/**
* Determines is an empire member is timed out based on his playtime, creation
* time, and last login.
*
* @param time_t created The player character's birth time.
* @param time_t last_login The player's last login time.
* @param double played_hours Number of hours the player has played, total.
* @return bool TRUE if the member has timed out and should not be counted; FALSE if they're ok.
*/
static bool member_is_timed_out(time_t created, time_t last_login, double played_hours) {
	int member_timeout_full = config_get_int("member_timeout_full") * SECS_PER_REAL_DAY;
	int member_timeout_newbie = config_get_int("member_timeout_newbie") * SECS_PER_REAL_DAY;
	int minutes_per_day_full = config_get_int("minutes_per_day_full");
	int minutes_per_day_newbie = config_get_int("minutes_per_day_newbie");

	if (played_hours >= config_get_int("member_timeout_max_threshold")) {
		return (last_login + member_timeout_full) < time(0);
	}
	else {
		double days_played = (double)(time(0) - created) / SECS_PER_REAL_DAY;
		double avg_min_per_day = 60 * (played_hours / days_played);
		double timeout;
		
		// when playtime drops this low, the character is ALWAYS timed out
		if (avg_min_per_day <= 1) {
			return TRUE;
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
		
		return (last_login + timeout) < time(0);
	}
}


/**
* Calls member_is_timed_out() using a player_index_data.
*
* @param player_index_data *index A pointer to the playertable entry.
* @return bool TRUE if the member has timed out and should not be counted; FALSE if they're ok.
*/
bool member_is_timed_out_index(player_index_data *index) {
	return member_is_timed_out(index->birth, index->last_logon, ((double)index->played) / SECS_PER_REAL_HOUR);
}


/**
* Calls member_is_timed_out() using a char_data.
*
* @param char_data *ch A character to compute timeout on.
* @return bool TRUE if the member has timed out and should not be counted; FALSE if they're ok.
*/
bool member_is_timed_out_ch(char_data *ch) {
	return member_is_timed_out(ch->player.time.birth, ch->prev_logon, ((double)ch->player.time.played) / SECS_PER_REAL_HOUR);
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
	void resort_empires(bool force);
	bool should_delete_empire(empire_data *emp);
	
	struct empire_member_reader_data *account_list = NULL, *emrd;
	player_index_data *index, *next_index;
	empire_data *e, *emp, *next_emp;
	char_data *ch;
	time_t logon;
	bool is_file;

	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (!only_empire || emp == only_empire) {
			EMPIRE_TOTAL_MEMBER_COUNT(emp) = 0;
			EMPIRE_MEMBERS(emp) = 0;
			EMPIRE_GREATNESS(emp) = 0;
			EMPIRE_TOTAL_PLAYTIME(emp) = 0;
			EMPIRE_LAST_LOGON(emp) = 0;
			EMPIRE_IMM_ONLY(emp) = 0;
		}
	}
	
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
		if (only_empire && index->loyalty != only_empire) {
			continue;
		}
		
		// new way of loading data
		if ((ch = find_or_load_player(index->name, &is_file))) {
			check_delayed_load(ch);
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
			EMPIRE_TOTAL_MEMBER_COUNT(e) += 1;
			
			// only count players who have logged on in recent history
			if (!is_file || !member_is_timed_out_ch(ch)) {
				add_to_account_list(&account_list, e, GET_ACCOUNT(ch)->id, GET_GREATNESS(ch));
				
				// not account-restricted
				EMPIRE_TOTAL_PLAYTIME(e) += (ch->player.time.played / SECS_PER_REAL_HOUR);

				if (read_techs) {
					adjust_abilities_to_empire(ch, e, TRUE);
				}
			}
		}
		
		if (ch && is_file) {
			free_char(ch);
		}
	}
	
	// now apply the best from each account, and clear out the list
	while ((emrd = account_list)) {
		EMPIRE_MEMBERS(emrd->empire) += 1;
		EMPIRE_GREATNESS(emrd->empire) += emrd->greatness;
		
		account_list = emrd->next;
		free(emrd);
	}
	
	// delete emptypires
	HASH_ITER(hh, empire_table, emp, next_emp) {
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


// 1.5x skills
PROMO_APPLY(promo_skillups) {
	struct player_skill_data *skill, *next_skill;
	
	HASH_ITER(hh, GET_SKILL_HASH(ch), skill, next_skill) {
		if (skill->level > 0) {
			set_skill(ch, skill->vnum, MIN(BASIC_SKILL_CAP, skill->level * 1.5));
		}
	}
}
