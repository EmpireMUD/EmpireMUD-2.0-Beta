/* ************************************************************************
*   File: db.player.c                                     EmpireMUD 2.0b2 *
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
*   Core Player Db
*   Helpers
*   Empire Player Management
*   Lore
*   Promo Codes
*/

// external vars
extern FILE *player_fl;
extern int top_account_id;
extern int top_idnum;
extern int top_of_p_file;

// external funcs
ACMD(do_slash_channel);
void update_class(char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// GETTERS /////////////////////////////////////////////////////////////////

/**
* Locate entry in p_table with entry->name == name. This is NEARLY identical
* to get_ptable_by_name and could be merged.
* TODO
*
* @param char *name Player's login name (case insensitive).
* @return int Position in player_table, or NOTHING if no match.
*/
int find_name(char *name) {
	int i;

	for (i = 0; i <= top_of_p_table; i++) {
		if (!str_cmp((player_table + i)->name, name))
			return (i);
	}

	return (NOTHING);
}


/**
* Finds a player table position by player name.
*
* @param char *name Player's login name (case insensitive).
* @return int Position in player_table, or NOTHING if no match.
*/
int get_ptable_by_name(char *name) {
	char arg[MAX_STRING_LENGTH];
	int i;

	one_argument(name, arg);
	for (i = 0; i <= top_of_p_table; i++)
		if (!str_cmp(player_table[i].name, arg))
			return (i);

	return (NOTHING);
}


/**
* Looks up a player id by name.
*
* @param char *name Player's login name (case insensitive).
* @return int A player id number, or NOTHING if no match.
*/
int get_id_by_name(char *name) {
	char arg[MAX_STRING_LENGTH];
	int i;

	one_argument(name, arg);
	for (i = 0; i <= top_of_p_table; i++)
		if (!str_cmp(player_table[i].name, arg))
			return (player_table[i].id);

	return (NOTHING);
}


/**
* Looks up a player's name by id.
*
* @param int id A player id.
* @return char* Either a pointer to the player's name in the ptable, or NULL if no match.
*/
char *get_name_by_id(int id) {
	int i;

	for (i = 0; i <= top_of_p_table; i++)
		if (player_table[i].id == id)
			return (player_table[i].name);

	return (NULL);
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
		if (!IS_NPC(ch) && GET_IDNUM(ch) == id) {
			return ch;
		}
	}
	return NULL;
}


 //////////////////////////////////////////////////////////////////////////////
//// CORE PLAYER DB //////////////////////////////////////////////////////////

/* generate index table for the player file */
void build_player_index(void) {
	int nr = -1, i;
	int size, recs;
	struct char_file_u dummy;

	if (!(player_fl = fopen(PLAYER_FILE, "r+b"))) {
		if (errno != ENOENT) {
			perror("SYSERR: fatal error opening playerfile");
			exit(1);
		}
		else {
			log("No playerfile. Creating a new one.");
			touch(PLAYER_FILE);
			if (!(player_fl = fopen(PLAYER_FILE, "r+b"))) {
				perror("SYSERR: fatal error opening playerfile");
				exit(1);
			}
		}
	}

	fseek(player_fl, 0L, SEEK_END);
	size = ftell(player_fl);
	rewind(player_fl);
	if (size % sizeof(struct char_file_u))
		log("\aWARNING:  PLAYERFILE IS PROBABLY CORRUPT!");
	recs = size / sizeof(struct char_file_u);
	if (recs) {
		log("   %d players in database.", recs);
		CREATE(player_table, struct player_index_element, recs);
	}
	else {
		player_table = NULL;
		top_of_p_file = top_of_p_table = -1;
		return;
	}

	for (; !feof(player_fl);) {
		fread(&dummy, sizeof(struct char_file_u), 1, player_fl);
		if (!feof(player_fl)) {	/* new record */
			nr++;
			CREATE(player_table[nr].name, char, strlen(dummy.name) + 1);
			for (i = 0; (*(player_table[nr].name + i) = LOWER(*(dummy.name + i))); i++);
			player_table[nr].id = dummy.char_specials_saved.idnum;
			top_idnum = MAX(top_idnum, dummy.char_specials_saved.idnum);
		}
	}

	top_of_p_file = top_of_p_table = nr;
}


/* copy vital data from a players char-structure to the file structure */
void char_to_store(char_data *ch, struct char_file_u *st) {
	int i, iter;
	struct affected_type *af;
	struct over_time_effect_type *dot, *last_dot;
	struct cooldown_data *cd;
	obj_data *char_eq[NUM_WEARS];

	/* Unaffect everything a character can be affected by */

	for (i = 0; i < NUM_WEARS; i++) {
		if (GET_EQ(ch, i)) {
			char_eq[i] = unequip_char(ch, i);
			#ifndef NO_EXTRANEOUS_TRIGGERS
				remove_otrigger(char_eq[i], ch);
			#endif
		}
		else {
			char_eq[i] = NULL;
		}
	}

	// affects
	for (af = ch->affected, i = 0; i < MAX_AFFECT; i++) {
		if (af && af->type > ATYPE_RESERVED && af->type < NUM_ATYPES) {
			st->affected[i] = *af;
			st->affected[i].next = NULL;
			af = af->next;
		}
		else {
			st->affected[i].type = 0;	/* Zero signifies not used */
			st->affected[i].cast_by = 0;
			st->affected[i].duration = 0;
			st->affected[i].modifier = 0;
			st->affected[i].location = 0;
			st->affected[i].bitvector = 0;

			st->affected[i].next = 0;
		}
	}

	if ((i >= MAX_AFFECT) && af && af->next) {
		log("SYSERR: WARNING: OUT OF STORE ROOM FOR AFFECTED TYPES!!!");
	}
	
	// over-time effects
	for (dot = ch->over_time_effects, i = 0; i < MAX_AFFECT; ++i) {
		if (dot && dot->type > ATYPE_RESERVED && dot->type < NUM_ATYPES) {
			st->over_time_effects[i] = *dot;
			st->over_time_effects[i].next = NULL;
			dot = dot->next;
		}
		else {
			st->over_time_effects[i].type = 0;
			st->over_time_effects[i].cast_by = 0;
			st->over_time_effects[i].damage = 0;
			st->over_time_effects[i].duration = 0;
			st->over_time_effects[i].damage_type = 0;
			st->over_time_effects[i].stack = 0;
			st->over_time_effects[i].max_stack = 0;
			st->over_time_effects[i].next = NULL;
		}
	}

	if ((i >= MAX_AFFECT) && dot && dot->next) {
		log("SYSERR: WARNING: OUT OF STORE ROOM FOR DOT TYPES!!!");
	}

	/*
	 * remove the affections so that the raw values are stored; otherwise the
	 * effects are doubled when the char logs back in.
	 */

	while (ch->affected)
		affect_remove(ch, ch->affected);
	
	while (ch->over_time_effects) {
		dot_remove(ch, ch->over_time_effects);
	}

	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		ch->aff_attributes[iter] = ch->real_attributes[iter];
	}

	// cooldowns -- store only non-expired cooldowns
	cd = ch->cooldowns;
	while (cd && cd->expire_time < time(0)) {
		cd = cd->next;
	}
	for (cd = ch->cooldowns, iter = 0; iter < MAX_COOLDOWNS; ++iter) {	
		if (cd) {
			// store a cooldown
			st->cooldowns[iter] = *cd;
			st->cooldowns[iter].next = 0;
			
			// find next
			do {
				cd = cd->next;
			} while (cd && cd->expire_time < time(0));
		}
		else {
			// no cooldowns to store; make sure the rest are blank
			st->cooldowns[iter].type = 0;
			st->cooldowns[iter].expire_time = 0;
			st->cooldowns[iter].next = 0;
		}
	}

	st->birth = ch->player.time.birth;
	st->played = ch->player.time.played;
	
	if (!PLR_FLAGGED(ch, PLR_KEEP_LAST_LOGIN_INFO)) {
		st->played += (int) (time(0) - ch->player.time.logon);
		st->last_logon = time(0);

		ch->player.time.played = st->played;
		ch->player.time.logon = time(0);
	}
	else {
		// preserve old logon info
		st->last_logon = ch->prev_logon;
		strncpy(st->host, ch->prev_host, MAX_HOST_LENGTH);
		st->host[MAX_HOST_LENGTH] = '\0';
	}

	st->sex = GET_REAL_SEX(ch);

	st->access_level = GET_ACCESS_LEVEL(ch);
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		st->attributes[iter] = ch->real_attributes[iter];
	}
	st->points = ch->points;
	st->char_specials_saved = ch->char_specials.saved;
	
	// convert empire now
	ch->player_specials->saved.empire = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING;
	
	st->player_specials_saved = ch->player_specials->saved;

	if (GET_TITLE(ch))
		strcpy(st->title, GET_TITLE(ch));
	else
		*st->title = '\0';
		
	if (ch->player.long_descr) {
		strcpy(st->description, ch->player.long_descr);
	}
	else {
		*st->description = '\0';
	}

	if (GET_PROMPT(ch))
		strcpy(st->prompt, GET_PROMPT(ch));
	else
		*st->prompt = '\0';

	if (GET_FIGHT_PROMPT(ch))
		strcpy(st->fight_prompt, GET_FIGHT_PROMPT(ch));
	else
		*st->fight_prompt = '\0';

	if (POOFIN(ch))
		strcpy(st->poofin, POOFIN(ch));
	else
		*st->poofin = '\0';

	if (POOFOUT(ch))
		strcpy(st->poofout, POOFOUT(ch));
	else
		*st->poofout = '\0';

	if (ch->player_specials->lastname && *ch->player_specials->lastname) {
		strcpy(st->lastname, ch->player_specials->lastname);
	}
	else {
		*st->lastname = '\0';
	}

	strcpy(st->name, GET_NAME(ch));
	strcpy(st->pwd, GET_PASSWD(ch));

	/* add spell and eq affections back in now */
	for (i = 0; i < MAX_AFFECT; i++) {
		if (st->affected[i].type) {
			affect_to_char(ch, &st->affected[i]);
		}
	}
	for (i = 0, last_dot = NULL; i < MAX_AFFECT; i++) {
		if (st->over_time_effects[i].type) {
			CREATE(dot, struct over_time_effect_type, 1);
			*dot = st->over_time_effects[i];
			dot->next = NULL;
			
			if (last_dot) {
				last_dot->next = dot;
			}
			else {
				ch->over_time_effects = dot;
			}
			last_dot = dot;
		}
	}

	for (i = 0; i < NUM_WEARS; i++) {
		if (char_eq[i]) {
			#ifndef NO_EXTRANEOUS_TRIGGERS
				if (wear_otrigger(char_eq[i], ch, i)) {
			#endif
					// this line may depend on the above if
					equip_char(ch, char_eq[i], i);
			#ifndef NO_EXTRANEOUS_TRIGGERS
				}
				else {
					obj_to_char(char_eq[i], ch);
				}
			#endif
		}
	}

	/*   affect_total(ch); unnecessary, I think !?! */
}


/*
 * Create a new entry in the in-memory index table for the player file.
 * If the name already exists, by overwriting a deleted character, then
 * we re-use the old position.
 */
int create_entry(char *name) {
	int i, pos;

	if (top_of_p_table == -1) {	/* no table */
		CREATE(player_table, struct player_index_element, 1);
		pos = top_of_p_table = 0;
	}
	else if ((pos = get_ptable_by_name(name)) == NOTHING) {	/* new name */
		i = ++top_of_p_table + 1;

		RECREATE(player_table, struct player_index_element, i);
		pos = top_of_p_table;
	}

	CREATE(player_table[pos].name, char, strlen(name) + 1);

	/* copy lowercase equivalent of name to table field */
	for (i = 0; (player_table[pos].name[i] = LOWER(name[i])); i++);

	return (pos);
}


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
	struct char_file_u tmp_store;
	char_data *ch = NULL;
	int id, player_i;
	
	*is_file = FALSE;
	
	if ((id = get_id_by_name(name))) {
		if (!(ch = is_playing(id))) {
			CREATE(ch, char_data, 1);
			clear_char(ch);			
			if ((player_i = load_char(name, &tmp_store)) > NOBODY) {
				store_to_char(&tmp_store, ch);
				GET_PFILEPOS(ch) = player_i;
				SET_BIT(PLR_FLAGS(ch), PLR_KEEP_LAST_LOGIN_INFO);
				*is_file = TRUE;
			}
			else {
				free(ch);
				ch = NULL;
			}
		}
	}

	return ch;
}


/* release memory allocated for a char struct */
void free_char(char_data *ch) {
	void free_alias(struct alias_data *a);

	struct channel_history_data *history;
	struct player_slash_channel *slash;
	struct interaction_item *interact;
	struct offer_data *offer;
	struct lore_data *lore;
	struct coin_data *coin;
	struct alias_data *a;
	char_data *proto;
	obj_data *obj;
	int iter;

	// in case somehow?
	if (GROUP(ch)) {
		leave_group(ch);
	}
	
	proto = IS_NPC(ch) ? mob_proto(GET_MOB_VNUM(ch)) : NULL;

	if (IS_NPC(ch)) {
		free_mob_tags(&MOB_TAGGED_BY(ch));
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
	
		while ((a = GET_ALIASES(ch)) != NULL) {
			GET_ALIASES(ch) = (GET_ALIASES(ch))->next;
			free_alias(a);
		}
		
		while ((offer = GET_OFFERS(ch))) {
			GET_OFFERS(ch) = offer->next;
			free(offer);
		}
		
		while ((slash = GET_SLASH_CHANNELS(ch))) {
			while ((history = slash->history)) {
				slash->history = history->next;
				if (history->message) {
					free(history->message);
				}
				free(history);
			}
			
			GET_SLASH_CHANNELS(ch) = slash->next;
			free(slash);
		}
		
		while ((coin = GET_PLAYER_COINS(ch))) {
			GET_PLAYER_COINS(ch) = coin->next;
			free(coin);
		}
				
		free(ch->player_specials);
		if (IS_NPC(ch)) {
			log("SYSERR: Mob %s (#%d) had player_specials allocated!", GET_NAME(ch), GET_MOB_VNUM(ch));
		}
	}

	if (ch->player.name && (!proto || ch->player.name != proto->player.name)) {
		free(ch->player.name);
	}
	if (ch->player.short_descr && (!proto || ch->player.short_descr != proto->player.short_descr)) {
		free(ch->player.short_descr);
	}
	if (ch->player.long_descr && (!proto || ch->player.long_descr != proto->player.long_descr)) {
		free(ch->player.long_descr);
	}
	if (ch->proto_script && (!proto || ch->proto_script != proto->proto_script)) {
		free_proto_script(ch, MOB_TRIGGER);
	}
	if (ch->interactions && (!proto || ch->interactions != proto->interactions)) {
		while ((interact = ch->interactions)) {
			ch->interactions = interact->next;
			free(interact);
		}
	}
	
	// remove all affects
	while (ch->affected) {
		affect_remove(ch, ch->affected);
	}
	
	// remove cooldowns
	while (ch->cooldowns) {
		remove_cooldown(ch, ch->cooldowns);
	}

	/* free any assigned scripts */
	if (SCRIPT(ch)) {
		extract_script(ch, MOB_TRIGGER);
	}
	
	// TODO what about script memory (freed in extract_char_final for NPCs, but what about PCs?)
	
	// free lore
	while ((lore = GET_LORE(ch))) {
		GET_LORE(ch) = lore->next;
		free(lore);
	}
	
	// alert empire data the mob is despawned
	if (GET_EMPIRE_NPC_DATA(ch)) {
		GET_EMPIRE_NPC_DATA(ch)->mob = NULL;
		GET_EMPIRE_NPC_DATA(ch) = NULL;
	}
	
	// extract objs
	while ((obj = ch->carrying)) {
		extract_obj(obj);
	}
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter)) {
			extract_obj(GET_EQ(ch, iter));
		}
	}

	if (ch->desc)
		ch->desc->character = NULL;

	/* find_char helper */
	remove_from_lookup_table(GET_ID(ch));

	free(ch);
}


void init_player(char_data *ch) {
	extern const int base_player_pools[NUM_POOLS];
	extern const char *syslog_types[];

	char fn[MAX_INPUT_LENGTH];
	int i, iter;

	/* create a player_special structure */
	if (ch->player_specials == NULL)
		CREATE(ch->player_specials, struct player_special_data, 1);

	GET_IMMORTAL_LEVEL(ch) = -1;				/* Not an immortal */

	/* *** if this is our first player --- he be God *** */
	if (top_of_p_table == 0) {
		GET_ACCESS_LEVEL(ch) = LVL_TOP;
		GET_IMMORTAL_LEVEL(ch) = 0;			/* Initialize it to 0, meaning Implementor */

		ch->real_attributes[STRENGTH] = att_max(ch);
		ch->real_attributes[DEXTERITY] = att_max(ch);

		ch->real_attributes[CHARISMA] = att_max(ch);
		ch->real_attributes[GREATNESS] = att_max(ch);

		ch->real_attributes[INTELLIGENCE] = att_max(ch);
		ch->real_attributes[WITS] = att_max(ch);
		
		SET_BIT(PRF_FLAGS(ch), PRF_HOLYLIGHT | PRF_ROOMFLAGS | PRF_NOHASSLE);
		
		// turn on all syslogs
		for (iter = 0; *syslog_types[iter] != '\n'; ++iter) {
			SYSLOG_FLAGS(ch) |= BIT(iter);
		}
	}

	get_filename(GET_NAME(ch), fn, LORE_FILE);
	sprintf(buf, "rm -f %s", fn);
	system(buf);

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

	ch->player.time.birth = time(0);
	ch->player.time.played = 0;
	ch->player.time.logon = time(0);

	ch->points.max_pools[BLOOD] = 10;	// not actually used for players
	ch->points.current_pools[BLOOD] = GET_MAX_BLOOD(ch);	// this is a function

	if ((i = get_ptable_by_name(GET_NAME(ch))) != NOTHING)
		player_table[i].id = GET_IDNUM(ch) = ++top_idnum;
	else
		log("SYSERR: init_player: Character '%s' not found in player table.", GET_NAME(ch));

	ch->char_specials.saved.affected_by = 0;

	for (i = 0; i < NUM_CONDS; i++)
		GET_COND(ch, i) = (GET_ACCESS_LEVEL(ch) == LVL_IMPL ? UNLIMITED : 0);

	// some nowheres/nothings
	GET_LOADROOM(ch) = NOWHERE;
	GET_MOUNT_VNUM(ch) = NOTHING;
	GET_EMPIRE_VNUM(ch) = NOTHING;
	GET_PLEDGE(ch) = NOTHING;
	GET_TOMB_ROOM(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_RETURN_LOCATION(ch) = NOWHERE;
	GET_ADVENTURE_SUMMON_RETURN_MAP(ch) = NOWHERE;
	
	// spares
	ch->player_specials->saved.spare30 = NOTHING;
	ch->player_specials->saved.spare31 = NOTHING;
	ch->player_specials->saved.spare32 = NOTHING;
	ch->player_specials->saved.spare33 = NOTHING;
	ch->player_specials->saved.spare34 = NOTHING;
}


/* Load a char, TRUE if loaded, FALSE if not */
int load_char(char *name, struct char_file_u * char_element) {
	int player_i;

	if ((player_i = find_name(name)) >= 0) {
		fseek(player_fl, (int) (player_i * sizeof(struct char_file_u)), SEEK_SET);
		fread(char_element, sizeof(struct char_file_u), 1, player_fl);
		return (player_i);
	}
	else
		return (NOTHING);
}


/**
* This function runs at startup to determine data that has to come from
* player files. You can add more things here, because this loop takes longer
* with a larger number of players, and it seems more efficient to load players
* only once.
*/
void load_player_data_at_startup(void) {
	int iter;
	struct char_file_u cfu;
	
	for (iter = 0; iter <= top_of_p_table; iter++) {
		load_char(player_table[iter].name, &cfu);
		
		// parts that apply even if the character is deleted (could be undeleted)
		top_account_id = MAX(top_account_id, cfu.player_specials_saved.account_id);
		
		// parts that only apply to non-deleted characters
		if (!IS_SET(cfu.char_specials_saved.act, PLR_DELETED)) {
			// add things here; load a whole character if absolutely necessary
		}
	}
}


/* clear some of the the working variables of a char */
void reset_char(char_data *ch) {
	int i;

	for (i = 0; i < NUM_WEARS; i++)
		GET_EQ(ch, i) = NULL;

	ch->followers = NULL;
	ch->master = NULL;
	IN_ROOM(ch) = NULL;
	ch->carrying = NULL;
	ch->next = NULL;
	ch->next_fighting = NULL;
	ch->next_in_room = NULL;
	ON_CHAIR(ch) = NULL;
	FIGHTING(ch) = NULL;
	ch->char_specials.position = POS_STANDING;
	ch->char_specials.carry_items = 0;

	if (GET_MOVE(ch) <= 0)
		GET_MOVE(ch) = 1;

	GET_LAST_TELL(ch) = NOBODY;
}


/*
 * write the vital data of a player to the player file -- this will not save
 * players who are disconnected.
 *
 * @param char_data *ch The player to save.
 * @param room_data *load_room (Optional) The location that the player will reappear on reconnect.
 */
void save_char(char_data *ch, room_data *load_room) {
	void save_char_vars(char_data *ch);
	
	struct char_file_u st;
	room_data *loc;

	if (IS_NPC(ch) || !ch->desc || GET_PFILEPOS(ch) < 0)
		return;

	char_to_store(ch, &st);

	if (!PLR_FLAGGED(ch, PLR_KEEP_LAST_LOGIN_INFO)) {
		strncpy(st.host, ch->desc->host, MAX_HOST_LENGTH);
		st.host[MAX_HOST_LENGTH] = '\0';
	}
	else {
		strncpy(st.host, ch->prev_host, MAX_HOST_LENGTH);
		st.host[MAX_HOST_LENGTH] = '\0';
	}

	if (!PLR_FLAGGED(ch, PLR_LOADROOM)) {
		if (load_room) {
			st.player_specials_saved.load_room = GET_ROOM_VNUM(load_room);
			loc = get_map_location_for(load_room);
			st.player_specials_saved.load_room_check = (loc ? GET_ROOM_VNUM(loc) : NOWHERE);
		}
		else {
			st.player_specials_saved.load_room = NOWHERE;
		}
	}

	GET_LOADROOM(ch) = st.player_specials_saved.load_room;
	GET_LOAD_ROOM_CHECK(ch) = st.player_specials_saved.load_room_check;

	fseek(player_fl, GET_PFILEPOS(ch) * sizeof(struct char_file_u), SEEK_SET);
	fwrite(&st, sizeof(struct char_file_u), 1, player_fl);
	
	save_char_vars(ch);
}


void save_char_file_u(struct char_file_u *st) {
	int player_i;
	
	if ((player_i = find_name(st->name)) >= 0) {
		fseek(player_fl, player_i * sizeof(struct char_file_u), SEEK_SET);
		fwrite(st, sizeof(struct char_file_u), 1, player_fl);
	}
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
	extern FILE *player_fl;
	struct char_file_u store;
	
	char_to_store(ch, &store);

	save_char_file_u(&store);
	free_char(ch);
}


/* copy data from the file structure to a char struct */
void store_to_char(struct char_file_u *st, char_data *ch) {
	extern bool member_is_timed_out_cfu(struct char_file_u *chdata);
	
	struct over_time_effect_type *dot;
	int i, iter;

	/* to save memory, only PC's -- not MOB's -- have player_specials */
	if (ch->player_specials == NULL)
		CREATE(ch->player_specials, struct player_special_data, 1);

	GET_REAL_SEX(ch) = st->sex;
	GET_ACCESS_LEVEL(ch) = st->access_level;

	ch->player.short_descr = NULL;

	GET_TITLE(ch) = str_dup(st->title);
	if (*GET_TITLE(ch) == '\0') {
		free(GET_TITLE(ch));
		GET_TITLE(ch) = NULL;
	}
		
	ch->player.long_descr = str_dup(st->description);
	if (ch->player.long_descr && *ch->player.long_descr == '\0') {
		ch->player.long_descr = NULL;
	}
	
	GET_PROMPT(ch) = str_dup(st->prompt);
	if (*GET_PROMPT(ch) == '\0') {
		free(GET_PROMPT(ch));
		GET_PROMPT(ch) = NULL;
	}
	
	GET_FIGHT_PROMPT(ch) = str_dup(st->fight_prompt);
	if (*GET_FIGHT_PROMPT(ch) == '\0') {
		free(GET_FIGHT_PROMPT(ch));
		GET_FIGHT_PROMPT(ch) = NULL;
	}

	POOFIN(ch) = str_dup(st->poofin);
	if (*POOFIN(ch) == '\0') {
		free(POOFIN(ch));
		POOFIN(ch) = NULL;
	}

	POOFOUT(ch) = str_dup(st->poofout);
	if (*POOFOUT(ch) == '\0') {
		free(POOFOUT(ch));
		POOFOUT(ch) = NULL;
	}

	GET_LASTNAME(ch) = str_dup(st->lastname);
	if (*GET_LASTNAME(ch) == '\0') {
		free(GET_LASTNAME(ch));
		GET_LASTNAME(ch) = NULL;
	}

	/* Get last logon info */
	strcpy(ch->prev_host, st->host);
	ch->prev_logon = st->last_logon;

	ch->player.time.birth = st->birth;
	ch->player.time.played = st->played;
	ch->player.time.logon = time(0);

	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		ch->real_attributes[iter] = st->attributes[iter];
		ch->aff_attributes[iter] = st->attributes[iter];
	}
	ch->points = st->points;
	ch->char_specials.saved = st->char_specials_saved;
	ch->player_specials->saved = st->player_specials_saved;
	GET_LAST_TELL(ch) = NOBODY;

	ch->char_specials.carry_items = 0;

	if (ch->player.name == NULL)
		CREATE(ch->player.name, char, strlen(st->name) + 1);
	strcpy(ch->player.name, st->name);
	strcpy(ch->player.passwd, st->pwd);

	/* Add all spell effects */
	for (i = 0; i < MAX_AFFECT; i++)
		if (st->affected[i].type > ATYPE_RESERVED && st->affected[i].type < NUM_ATYPES)
			affect_to_char(ch, &st->affected[i]);
	
	// dot effects
	for (i = 0; i < MAX_AFFECT; ++i) {
		if (st->over_time_effects[i].type > ATYPE_RESERVED && st->over_time_effects[i].type < NUM_ATYPES) {
			CREATE(dot, struct over_time_effect_type, 1);
			*dot = st->over_time_effects[i];
			dot->next = ch->over_time_effects;
			ch->over_time_effects = dot;
		}
	}
	
	// read in cooldowns
	for (iter = 0; iter < MAX_COOLDOWNS; ++iter) {
		if (st->cooldowns[iter].type > 0) {
			add_cooldown(ch, st->cooldowns[iter].type, st->cooldowns[iter].expire_time - time(0));
		}
	}
	
	// convert empire
	GET_LOYALTY(ch) = real_empire(GET_EMPIRE_VNUM(ch));
	
	// ensure this is not set
	REMOVE_BIT(PLR_FLAGS(ch), PLR_EXTRACTED);

	/* Players who have been out for 1 hour get a free restore */
	RESTORE_ON_LOGIN(ch) = (((int) (time(0) - st->last_logon)) >= 1 * SECS_PER_REAL_HOUR);
	REREAD_EMPIRE_TECH_ON_LOGIN(ch) = member_is_timed_out_cfu(st);
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Function to DELETE A PLAYER.
*
* @param char_data *ch The player to delete.
*/
void delete_player_character(char_data *ch) {
	void clear_private_owner(int id);
	void Crash_delete_file(char *name);
	
	empire_data *emp = NULL;
	char filename[256];
	
	if (IS_NPC(ch)) {
		syslog(SYS_ERROR, 0, TRUE, "SYSERR: delete_player_character called on NPC");
		return;
	}
	
	SET_BIT(PLR_FLAGS(ch), PLR_DELETED);
	clear_private_owner(GET_IDNUM(ch));

	// Check the empire
	if ((emp = GET_LOYALTY(ch)) != NULL) {
		GET_LOYALTY(ch) = NULL;
		GET_EMPIRE_VNUM(ch) = NOTHING;
		GET_RANK(ch) = 0;
	}

	SAVE_CHAR(ch);
	
	// various file deletes
	Crash_delete_file(GET_NAME(ch));
	delete_variables(GET_NAME(ch));
	if (get_filename(GET_NAME(ch), filename, ALIAS_FILE)) {
		if (remove(filename) < 0 && errno != ENOENT) {
			log("SYSERR: deleting alias file %s: %s", filename, strerror(errno));
		}
	}
	if (get_filename(GET_NAME(ch), filename, LORE_FILE)) {
		if (remove(filename) < 0 && errno != ENOENT) {
			log("SYSERR: deleting lore file %s: %s", filename, strerror(errno));
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
* @param int dolog Whether or not to log the login (passed to Objload_char).
* @param bool fresh If FALSE, player was already in the game, not logging in fresh.
* @return int 1 = rent-saved, 0 = crash-saved
*/
int enter_player_game(descriptor_data *d, int dolog, bool fresh) {
	void assign_class_abilities(char_data *ch, int class, int role);
	void clean_lore(char_data *ch);
	extern int Objload_char(char_data *ch, int dolog);
	void read_aliases(char_data *ch);
	void read_lore(char_data *ch);
	extern room_data *find_home(char_data *ch);
	extern room_data *find_load_room(char_data *ch);
	extern int get_ptable_by_name(char *name);
	void read_saved_vars(char_data *ch);
	
	extern bool global_mute_slash_channel_joins;
	extern int top_idnum;

	char lbuf[MAX_STRING_LENGTH];
	int i, iter;
	empire_data *emp;
	room_data *load_room = NULL, *map_loc;
	char_data *ch = d->character;
	int load_result;
	bool try_home = FALSE;

	reset_char(ch);
	read_aliases(ch);
	read_lore(ch);
	
	// remove this now
	REMOVE_BIT(PLR_FLAGS(ch), PLR_KEEP_LAST_LOGIN_INFO);
	
	// ensure they have this
	if (!*GET_CREATION_HOST(ch)) {
		strncpy(GET_CREATION_HOST(ch), d->host, MAX_HOST_LENGTH);
		GET_CREATION_HOST(ch)[MAX_HOST_LENGTH] = '\0';
	}

	if (GET_IDNUM(ch) == 0) {
		if ((i = get_ptable_by_name(GET_NAME(ch))) != NOTHING) {
			player_table[i].id = GET_IDNUM(ch) = ++top_idnum;
		}
	}

	if (GET_IMMORTAL_LEVEL(ch) > -1) {
		GET_ACCESS_LEVEL(ch) = LVL_TOP - GET_IMMORTAL_LEVEL(ch);
	}
	
	if (PLR_FLAGGED(ch, PLR_INVSTART))
		GET_INVIS_LEV(ch) = GET_ACCESS_LEVEL(ch);

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
			}
		}
	}
	
	// cancel detected loadroom?
	if (load_room && RESTORE_ON_LOGIN(ch) && PRF_FLAGGED(ch, PRF_AUTORECALL)) {
		load_room = NULL;
		try_home = TRUE;
	}
		
	// long logout and in somewhere hostile
	if (load_room && RESTORE_ON_LOGIN(ch) && ROOM_OWNER(load_room) && empire_is_hostile(ROOM_OWNER(load_room), GET_LOYALTY(ch), load_room)) {
		load_room = NULL;	// re-detect
		try_home = TRUE;
	}
	
	// on request, try to send them home
	if (try_home) {
		load_room = find_home(ch);
	}

	// nowhere found? must detect load room
	if (!load_room) {
		load_room = find_load_room(d->character);
	}

	// absolute failsafe
	if (!load_room) {
		load_room = real_room(0);
	}


	/* fail-safe */
	if (IS_VAMPIRE(ch) && GET_APPARENT_AGE(ch) <= 0)
		GET_APPARENT_AGE(ch) = 25;

	ch->next = character_list;
	character_list = ch;
	char_to_room(ch, load_room);
	load_result = Objload_char(ch, dolog);

	affect_total(ch);
	SAVE_CHAR(ch);
		
	// verify class and skill/gear levels are up-to-date
	update_class(ch);
	determine_gear_level(ch);
	
	// clear some player special data
	GET_MARK_LOCATION(ch) = NOWHERE;

	// re-join slash-channels
	global_mute_slash_channel_joins = TRUE;
	for (iter = 0; iter < MAX_SLASH_CHANNELS; ++iter) {
		if (*GET_STORED_SLASH_CHANNEL(ch, iter)) {
			sprintf(lbuf, "join %s", GET_STORED_SLASH_CHANNEL(ch, iter));
			do_slash_channel(ch, lbuf, 0, 0);
		}
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
		affect_from_char(ch, ATYPE_DEATH_PENALTY);
		
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
	assign_class_abilities(ch, NOTHING, NOTHING);
	
	// ensure player has penalty if at war
	if (fresh && GET_LOYALTY(ch) && is_at_war(GET_LOYALTY(ch))) {
		int duration = config_get_int("war_login_delay") / SECS_PER_REAL_UPDATE;
		struct affected_type *af = create_flag_aff(ATYPE_WAR_DELAY, duration, AFF_IMMUNE_PHYSICAL | AFF_NO_ATTACK | AFF_STUNNED, ch);
		affect_join(ch, af, ADD_DURATION);
	}

	// script/trigger stuff
	GET_ID(ch) = GET_IDNUM(ch);
	read_saved_vars(ch);
	greet_mtrigger(ch, NO_DIR);
	greet_memory_mtrigger(ch);
	add_to_lookup_table(GET_ID(ch), (void *)ch);
	
	// ensure echo is on -- no, this could cause a duplicate echo-on and lead to an echo loop
	// ProtocolNoEcho(d, false);
	
	return load_result;
}


/**
* @return int a new unique account id
*/
int new_account_id(void) {
	return ++top_account_id;
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

	if (GET_TITLE(ch) != NULL)
		free(GET_TITLE(ch));

	if (*title != ':' && *title != ',' && *title != '-' && *title != ';' && *title != '~')
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
	void apply_bonus_trait(char_data *ch, bitvector_t trait, bool add);
	extern const struct archetype_type archetype[];
	void make_vampire(char_data *ch, bool lore);
	void scale_item_to_level(obj_data *obj, int level);
	void set_skill(char_data *ch, int skill, int level);
	extern const char *default_channels[];
	extern bool global_mute_slash_channel_joins;
	extern struct promo_code_list promo_codes[];
	extern int tips_of_the_day_size;

	char lbuf[MAX_INPUT_LENGTH];
	int type, iter;
	obj_data *obj;
	
	// announce to existing players that we have a newbie
	mortlog("%s has joined the game", PERS(ch, ch, TRUE));

	set_title(ch, NULL);

	/* Default Flags */
	SET_BIT(PRF_FLAGS(ch), PRF_MORTLOG);
	if (config_get_bool("siteok_everyone")) {
		SET_BIT(PLR_FLAGS(ch), PLR_SITEOK);
	}
	
	// not sure how they could not have this...
	if (ch->desc) {
		strncpy(GET_CREATION_HOST(ch), ch->desc->host, MAX_HOST_LENGTH);
		GET_CREATION_HOST(ch)[MAX_HOST_LENGTH] = '\0';
	}
	else {
		*GET_CREATION_HOST(ch) = '\0';
	}

	// level
	if (GET_ACCESS_LEVEL(ch) < LVL_APPROVED && !config_get_bool("require_auth")) {
		GET_ACCESS_LEVEL(ch) = LVL_APPROVED;
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
	for (iter = 0; iter < MAX_CUSTOM_COLORS; ++iter) {
		GET_CUSTOM_COLOR(ch, iter) = 0;
	}
	
	// add the default slash channels
	global_mute_slash_channel_joins = TRUE;
	for (iter = 0; *default_channels[iter] != '\n'; ++iter) {
		sprintf(lbuf, "join %s", default_channels[iter]);
		do_slash_channel(ch, lbuf, 0, 0);
	}
	global_mute_slash_channel_joins = FALSE;

	/* Give EQ, if applicable */
	if (CREATION_ARCHETYPE(ch) != 0) {
		type = CREATION_ARCHETYPE(ch);
		
		// attributes
		for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
			ch->real_attributes[iter] = archetype[type].attributes[iter];
		}
	
		// skills
		if (archetype[type].primary_skill != NO_SKILL && GET_SKILL(ch, archetype[type].primary_skill) < archetype[type].primary_skill_level) {
			set_skill(ch, archetype[type].primary_skill, archetype[type].primary_skill_level);
		}
		if (archetype[type].secondary_skill != NO_SKILL && GET_SKILL(ch, archetype[type].secondary_skill) < archetype[type].secondary_skill_level) {
			set_skill(ch, archetype[type].secondary_skill, archetype[type].secondary_skill_level);
		}
		
		// vampire?
		if (!IS_VAMPIRE(ch) && (archetype[type].primary_skill == SKILL_VAMPIRE || archetype[type].secondary_skill == SKILL_VAMPIRE)) {
			make_vampire(ch, TRUE);
			GET_BLOOD(ch) = GET_MAX_BLOOD(ch);
		}
		
		// newbie eq -- don't run load triggers on this set, as ch is possibly not in a room
		for (iter = 0; archetype[type].gear[iter].vnum != NOTHING; ++iter) {
			// skip slots that are somehow full
			if (archetype[type].gear[iter].wear != NOWHERE && GET_EQ(ch, archetype[type].gear[iter].wear) != NULL) {
				continue;
			}
			
			obj = read_object(archetype[type].gear[iter].vnum, TRUE);
			scale_item_to_level(obj, 1);	// lowest possible scale
			
			if (archetype[type].gear[iter].wear == NOWHERE) {
				obj_to_char(obj, ch);
			}
			else {
				equip_char(ch, obj, archetype[type].gear[iter].wear);
			}
		}

		// misc items
		obj = read_object(o_GRAVE_MARKER, TRUE);
		scale_item_to_level(obj, 1);	// lowest possible scale
		obj_to_char(obj, ch);
		
		for (iter = 0; iter < 2; ++iter) {
			// 2 bread
			obj = read_object(o_BREAD, TRUE);
			scale_item_to_level(obj, 1);	// lowest possible scale
			obj_to_char(obj, ch);
			
			// 2 trinket of conveyance
			obj = read_object(o_TRINKET_OF_CONVEYANCE, TRUE);
			scale_item_to_level(obj, 1);	// lowest possible scale
			obj_to_char(obj, ch);
		}
		
		obj = read_object(o_BOWL, TRUE);
		scale_item_to_level(obj, 1);	// lowest possible scale
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = GET_DRINK_CONTAINER_CAPACITY(obj);
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
		obj_to_char(obj, ch);
		determine_gear_level(ch);
	}
	
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
* Calls member_is_timed_out() using a char_file_u.
*
* @param struct char_file_u *chdata A cfu pointer (character loaded from file but not char_data).
* @return bool TRUE if the member has timed out and should not be counted; FALSE if they're ok.
*/
bool member_is_timed_out_cfu(struct char_file_u *chdata) {
	return member_is_timed_out(chdata->birth, chdata->last_logon, ((double)chdata->played) / SECS_PER_REAL_HOUR);
}


/**
* Calls member_is_timed_out() using a char_data.
*
* @param char_data *ch A character to compute timeout on.
* @return bool TRUE if the member has timed out and should not be counted; FALSE if they're ok.
*/
bool member_is_timed_out_ch(char_data *ch) {
	return member_is_timed_out(ch->player.time.birth, ch->player.time.logon, ((double)ch->player.time.played) / SECS_PER_REAL_HOUR);
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
	void Objsave_char(char_data *ch, int rent_code);
	extern int Objload_char(char_data *ch, int dolog);
	void resort_empires();
	bool should_delete_empire(empire_data *emp);
	
	struct empire_member_reader_data *account_list = NULL, *emrd;
	struct char_file_u chdata;
	int pos;
	empire_data *e, *emp, *next_emp;
	char_data *ch;
	bool is_file;

	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (!only_empire || emp == only_empire) {
			EMPIRE_TOTAL_MEMBER_COUNT(emp) = 0;
			EMPIRE_MEMBERS(emp) = 0;
			EMPIRE_GREATNESS(emp) = 0;
			EMPIRE_TOTAL_PLAYTIME(emp) = 0;
			EMPIRE_LAST_LOGON(emp) = 0;
		}
	}

	for (pos = 0; pos <= top_of_p_table; pos++) {
		// need chdata either way; check deleted here
		if (load_char(player_table[pos].name, &chdata) <= NOBODY || IS_SET(chdata.char_specials_saved.act, PLR_DELETED)) {
			continue;
		}
		
		if (only_empire && chdata.player_specials_saved.empire != EMPIRE_VNUM(only_empire)) {
			continue;
		}
		
		// new way of loading data
		if ((ch = find_or_load_player(player_table[pos].name, &is_file))) {
			affect_total(ch);
			
			if (is_file) {
				Objload_char(ch, FALSE);
			}
		}
		
		// check ch for empire traits
		if (ch && (e = GET_LOYALTY(ch))) {
			// record last-logon whether or not timed out
			if (chdata.last_logon > EMPIRE_LAST_LOGON(e)) {
				EMPIRE_LAST_LOGON(e) = chdata.last_logon;
			}

			if (GET_ACCESS_LEVEL(ch) >= LVL_GOD) {
				EMPIRE_IMM_ONLY(e) = 1;
			}
			
			// always
			EMPIRE_TOTAL_MEMBER_COUNT(e) += 1;
			
			// only count players who have logged on in recent history
			if (!member_is_timed_out_cfu(&chdata)) {
				if (GET_ACCOUNT_ID(ch) == 0) {
					EMPIRE_MEMBERS(e) += 1;
					EMPIRE_GREATNESS(e) += GET_GREATNESS(ch);
				}
				else {
					add_to_account_list(&account_list, e, GET_ACCOUNT_ID(ch), GET_GREATNESS(ch));
				}
				
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
		resort_empires();
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// LORE ////////////////////////////////////////////////////////////////////

void read_lore(char_data *ch) {
	FILE *file;
	char fn[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH];
	struct lore_data *temp, *last = NULL;
	int a, b;
	long c;

	if (!ch || IS_NPC(ch))
		return;

	get_filename(GET_NAME(ch), fn, LORE_FILE);

	if (!(file = fopen(fn, "r"))) {
		if (errno != ENOENT) {
			log("SYSERR: Couldn't load lore for %s from '%s'.", GET_NAME(ch), fn);
			perror("SYSERR: read_lore");
		}
		return;
	}

	for (;;) {
		get_line(file, line);

		if (!*line) {
			log("SYSERR: Unexpected EOF in '%s'.", fn);
			break;
		}

		if (*line == '$')
			break;

		if (sscanf(line, "%d %d %ld", &a, &b, &c) != 3) {
			log("SYSERR: Error reading data from '%s'.", fn);
			break;
		}
		CREATE(temp, struct lore_data, 1);
		temp->type = a;
		temp->value = b;
		temp->date = c;
		if (last)
			last->next = temp;
		else
			GET_LORE(ch) = temp;
		temp->next = NULL;
		last = temp;
	}
	fclose(file);
}


void write_lore(char_data *ch) {
	FILE *file;
	char fn[MAX_STRING_LENGTH], tempname[MAX_STRING_LENGTH];
	struct lore_data *temp;

	if (!ch || IS_NPC(ch))
		return;

	get_filename(GET_NAME(ch), fn, LORE_FILE);
	strcpy(tempname, fn);
	strcat(tempname, TEMP_SUFFIX);

	if (GET_LORE(ch) == NULL)
		return;

	if (!(file = fopen(tempname, "w"))) {
		log("SYSERR: Couldn't save lore for %s in '%s'.", GET_NAME(ch), tempname);
		perror("SYSERR: write_lore");
		return;
	}

	for (temp = GET_LORE(ch); temp; temp = temp->next)
		fprintf(file, "%d %d %ld\n", temp->type, temp->value, temp->date);

	fprintf(file, "$\n");
	fclose(file);
	rename(tempname, fn);
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
	int iter;
	
	for (iter = 0; iter < NUM_SKILLS; ++iter) {
		if (GET_SKILL(ch, iter) > 0) {
			set_skill(ch, iter, MIN(BASIC_SKILL_CAP, GET_SKILL(ch, iter) * 1.5));
		}
	}
}
