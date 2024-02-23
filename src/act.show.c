/* ************************************************************************
*   File: act.show.c                                      EmpireMUD 2.0b5 *
*  Usage: The "show" command and its many functions                       *
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
#include "skills.h"
#include "olc.h"
#include "dg_scripts.h"
#include "constants.h"

/**
* Contents:
*   Helpers
*   Show Islands
*   Show Resource
*   Simple Show Functions
*   Command Data
*   Commands
*/

// external functions
void Crash_listrent(char_data *ch, char *name);
void update_account_stats();

// external variables
extern struct stored_data_type stored_data_info[];
extern int buf_switches, buf_largecount, buf_overflows, top_of_helpt;
extern int total_accounts, active_accounts, active_accounts_week;

// function declaration for the show command/functions
#define SHOW(name)	void (name)(char_data *ch, char *argument)


 //////////////////////////////////////////////////////////////////////////////
//// SHOW ISLANDS ////////////////////////////////////////////////////////////

struct show_island_data {
	int island;
	int items;
	int population;
	int territory;
	char *techs;
	struct show_island_data *next;
};


// helper to get/create an island entry in a list
struct show_island_data *find_or_make_show_island(int island, struct show_island_data **list) {
	struct show_island_data *sid, *cur = NULL;
	
	// find an island entry
	for (sid = *list; sid && !cur; sid = sid->next) {
		if (sid->island == island) {
			cur = sid;
			break;
		}
	}

	// need to make an entry?
	if (!cur) {
		CREATE(cur, struct show_island_data, 1);
		cur->island = island;
		LL_PREPEND(*list, cur);
	}
	
	return cur;
}


SHOW(show_islands) {
	struct empire_unique_storage *uniq;
	struct shipping_data *shipd;
	struct empire_storage_data *store, *next_store;
	struct empire_island *eisle, *next_eisle;
	char tech_str[MAX_STRING_LENGTH];
	struct island_info *isle;
	empire_data *emp;
	int iter, tid;
	
	struct show_island_data *list = NULL, *sid, *cur = NULL;
	
	one_word(argument, arg);
	if (!*arg) {
		msg_to_char(ch, "Show islands: Check which empire?\r\n");
	}
	else if (!(emp = get_empire_by_name(arg))) {
		msg_to_char(ch, "Show islands: Unknown empire '%s'.\r\n", arg);
	}
	else {
		msg_to_char(ch, "Island counts for %s%s&0:\r\n", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		
		// collate storage info and territory
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), eisle, next_eisle) {
			cur = find_or_make_show_island(eisle->island, &list);
			SAFE_ADD(cur->population, eisle->population, 0, INT_MAX, FALSE);
			SAFE_ADD(cur->territory, eisle->territory[TER_TOTAL], 0, INT_MAX, FALSE);
			
			HASH_ITER(hh, eisle->store, store, next_store) {
				SAFE_ADD(cur->items, store->amount, 0, INT_MAX, FALSE);
			}
			
			// techs?
			*tech_str = '\0';
			for (tid = 0; tid < NUM_TECHS; ++tid) {
				if (eisle->tech[tid] > 0 && search_block_int(tid, techs_requiring_same_island) != NOTHING) {
					snprintf(tech_str + strlen(tech_str), sizeof(tech_str) - strlen(tech_str), "%s%s", (*tech_str ? ", " : ""), empire_tech_types[tid]);
				}
			}
			if (*tech_str) {
				if (cur->techs) {
					free(cur->techs);
				}
				cur->techs = strdup(tech_str);
			}
		}
		DL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), uniq) {
			if (!cur || cur->island != uniq->island) {
				cur = find_or_make_show_island(uniq->island, &list);
			}
			SAFE_ADD(cur->items, uniq->amount, 0, INT_MAX, FALSE);
		}
		DL_FOREACH(EMPIRE_SHIPPING_LIST(emp), shipd) {
			if (!cur || cur->island != shipd->from_island) {
				cur = find_or_make_show_island(shipd->from_island, &list);
			}
			SAFE_ADD(cur->items, shipd->amount, 0, INT_MAX, FALSE);
		}
		
		if (!list) {
			msg_to_char(ch, " no storage\r\n");
		}
		
		// islands are sequentially numbered so this should be pretty safe -- iterate as long as anything remains in list; remove as we go
		for (iter = MIN(0, NO_ISLAND); list; ++iter) {
			// do we have this number in the list?
			cur = NULL;
			for (sid = list; sid && !cur; sid = sid->next) {
				if (sid->island == iter) {
					cur = sid;
					break;
				}
			}
			
			if (!cur) {
				continue;
			}
			
			// only show when there's something to show
			if (cur->items > 0 || cur->population > 0 || cur->territory > 0 || cur->techs) {
				isle = get_island(cur->island, TRUE);
				msg_to_char(ch, "%2d. %s: %d items, %d population, %d territory%s%s\r\n", cur->island, get_island_name_for(isle->id, ch), cur->items, cur->population, cur->territory, cur->techs ? ", " : "", NULLSAFE(cur->techs));
			}
			
			// pull it out of the list to prevent unlimited iteration
			LL_DELETE(list, cur);
			if (cur->techs) {
				free(cur->techs);
			}
			free(cur);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// SHOW RESOURCE ///////////////////////////////////////////////////////////

struct show_res_t {
	long long amount;
	struct show_res_t *next;
};


// show resource median sorter
int compare_show_res(struct show_res_t *a, struct show_res_t *b) {
	return a->amount - b->amount;
}


SHOW(show_resource) {
	obj_data *obj_iter, *next_obj, *proto = NULL;
	struct empire_island *eisle, *next_eisle;
	struct empire_storage_data *store;
	struct shipping_data *shipd;
	empire_data *emp, *next_emp;
	long long med_amt = 0;
	int median, pos;
	any_vnum vnum;
	
	// tracker data: uses long longs because empires can store max-int per island
	int total_emps = 0, active_emps = 0, emps_storing = 0, active_storing = 0;
	long long amt, total = 0, active_total = 0;	// track both total-total and active empires
	empire_data *highest_emp = NULL;	// empire with the most
	long long highest_amt = 0;	// how much they have
	
	// data storage for medians
	struct show_res_t *el, *next_el, *list = NULL;
	
	// attempt to figure out which resource
	if (!*argument) {
		msg_to_char(ch, "Usage: show resource <vnum | name>\r\n");
		return;
	}
	else if (isdigit(*argument)) {
		proto = obj_proto(atoi(argument));
		// checked later
	}
	else {	// look up by name
		HASH_ITER(hh, object_table, obj_iter, next_obj) {
			if (GET_OBJ_STORAGE(obj_iter) && multi_isname(argument, GET_OBJ_KEYWORDS(obj_iter))) {
				proto = obj_iter;
				break;
			}
		}
	}
	
	// verify
	if (!proto) {
		msg_to_char(ch, "Show resource: Unknown storable object '%s'.\r\n", argument);
		return;
	}
	if (!GET_OBJ_STORAGE(proto)) {
		msg_to_char(ch, "Show resource: You can only use this on storable objects.\r\n");
		return;
	}
	vnum = GET_OBJ_VNUM(proto);
	
	// ok now build the data
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_IMM_ONLY(emp)) {
			continue;	// skip imms
		}
		
		amt = 0;
		
		// scan islands
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), eisle, next_eisle) {
			HASH_FIND_INT(eisle->store, &vnum, store);
			if (store) {
				SAFE_ADD(amt, store->amount, 0, LLONG_MAX, FALSE);
			}
		}
		// scan shipping
		DL_FOREACH(EMPIRE_SHIPPING_LIST(emp), shipd) {
			if (shipd->vnum == vnum) {
				SAFE_ADD(amt, shipd->amount, 0, LLONG_MAX, FALSE);
			}
		}
		
		// count it
		++total_emps;
		SAFE_ADD(total, amt, 0, LLONG_MAX, FALSE);
		
		if (amt > 0) {
			++emps_storing;
		}
		
		// active-only
		if (!EMPIRE_IS_TIMED_OUT(emp)) {
			++active_emps;
			SAFE_ADD(active_total, amt, 0, LLONG_MAX, FALSE);
			
			CREATE(el, struct show_res_t, 1);
			el->amount = amt;
			LL_INSERT_INORDER(list, el, compare_show_res);
			
			if (amt > highest_amt) {
				highest_emp = emp;
				highest_amt = amt;
			}
			
			if (amt > 0) {
				++active_storing;
			}
		}
	}
	
	// determine medians and free list
	median = active_emps / 2 + 1;
	median = MIN(active_emps, median);
	pos = 0;
	LL_FOREACH_SAFE(list, el, next_el) {
		if (pos++ == median) {
			med_amt = el->amount;
		}
		free(el);
	}
	
	// and output
	msg_to_char(ch, "Resource storage analysis for [%d] %s:\r\n", GET_OBJ_VNUM(proto), GET_OBJ_SHORT_DESC(proto));
	msg_to_char(ch, "%d active empire%s: %lld stored, %lld mean, %lld median, %d empires have any\r\n", active_emps, PLURAL(active_emps), active_total, (active_total / MAX(1, active_emps)), med_amt, active_storing);
	if (highest_emp) {
		msg_to_char(ch, "Highest active empire: %s (%lld stored)\r\n", EMPIRE_NAME(highest_emp), highest_amt);
	}
	msg_to_char(ch, "%d total empire%s: %lld stored, %lld mean, %d empires have any\r\n", total_emps, PLURAL(total_emps), total, (total / MAX(1, total_emps)), emps_storing);
}


 //////////////////////////////////////////////////////////////////////////////
//// SIMPLE SHOW FUNCTIONS ///////////////////////////////////////////////////

SHOW(show_account) {
	player_index_data *plr_index = NULL, *index, *next_index;
	bool file = FALSE, loaded_file = FALSE;
	char skills[MAX_STRING_LENGTH], ago_buf[256], *ago_ptr, color;
	char_data *plr = NULL, *loaded;
	int acc_id = NOTHING;
	time_t last_online = -1;	// -1 here will indicate no data, -2 will indicate online now
	account_data *acc_ptr;
	
	#define ONLINE_NOW  -2
	
	if (!*argument) {
		msg_to_char(ch, "Usage: show account <player>\r\n");
		return;
	}
	
	// target by...
	if (isdigit(*argument)) {
		acc_id = atoi(argument);
	}
	else if (!(plr = find_or_load_player(argument, &file))) {
		send_to_char("Show account: There is no such player.\r\n", ch);
		return;
	}
	
	// look up index if applicable
	if (plr && !(plr_index = find_player_index_by_idnum(GET_IDNUM(plr)))) {
		msg_to_char(ch, "Show account: Unknown error: player not in index.\r\n");
		if (file) {
			free_char(plr);
		}
		return;
	}
	
	// look up id if needed
	if (acc_id == NOTHING && plr_index) {
		acc_id = plr_index->account_id;
	}
	
	if (!(acc_ptr = find_account(acc_id))) {
		msg_to_char(ch, "Show account: Unknown account: %d\r\n", acc_id);
		if (plr && file) {
			free_char(plr);
		}
		return;
	}
	
	// display:
	msg_to_char(ch, "Account characters:\r\n");
	
	HASH_ITER(name_hh, player_table_by_name, index, next_index) {
		if (index->account_id != acc_id && (!plr_index || strcmp(index->last_host, plr_index->last_host))) {
			continue;
		}
		if (!(loaded = find_or_load_player(index->name, &loaded_file))) {
			continue;
		}
		
		// skills name used by all 3
		get_player_skill_string(loaded, skills, TRUE);
		
		if (GET_ACCOUNT(loaded)->id == acc_id) {
			if (!loaded_file) {
				msg_to_char(ch, " &c[%d %s] %s  (online)&0\r\n", GET_COMPUTED_LEVEL(loaded), skills, GET_PC_NAME(loaded));
				last_online = ONLINE_NOW;
			}
			else {
				// not playing but same account
				ago_ptr = strcpy(ago_buf, simple_time_since(loaded->prev_logon));
				skip_spaces(&ago_ptr);
				
				if (strchr(ago_ptr, 'y')) {
					color = 'R';
				}
				else if (strchr(ago_ptr, 'w')) {
					color = 'Y';
				}
				else if (strchr(ago_ptr, 'd')) {
					color = 'B';
				}
				else {
					color = 'G';
				}
				
				msg_to_char(ch, " [%d %s] %s - \t%c%s ago\t0\r\n", GET_LAST_KNOWN_LEVEL(loaded), skills, GET_PC_NAME(loaded), color, ago_ptr);
				if (last_online != ONLINE_NOW) {
					last_online = MAX(last_online, loaded->prev_logon);
				}
			}
		}
		else if (ACCOUNT_FLAGGED(loaded, ACCT_MULTI_IP) || IS_SET(acc_ptr->flags, ACCT_MULTI_IP)) {
			msg_to_char(ch, " &r[%d %s] %s  (separate account %d)&0%s\r\n", loaded_file ? GET_LAST_KNOWN_LEVEL(loaded) : GET_COMPUTED_LEVEL(loaded), skills, GET_PC_NAME(loaded), GET_ACCOUNT(loaded)->id, !loaded_file ? " &c(online)&0" : "");
		}
		else {
			msg_to_char(ch, " &r[%d %s] %s  (same IP, account %d)&0%s\r\n", loaded_file ? GET_LAST_KNOWN_LEVEL(loaded) : GET_COMPUTED_LEVEL(loaded), skills, GET_PC_NAME(loaded), GET_ACCOUNT(loaded)->id, !loaded_file ? " &c(online)&0" : "");
		}
		
		if (loaded_file) {
			// leave them open to be cleaned up later; also loaded may equal plr
			// free_char(loaded);
		}
	}
	
	if (last_online > 0) {
		ago_ptr = strcpy(ago_buf, simple_time_since(last_online));
		skip_spaces(&ago_ptr);
		msg_to_char(ch, " (last online: %-24.24s, %s ago)\r\n", ctime(&last_online), ago_ptr);
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_ammotypes) {
	obj_data *obj, *next_obj;
	int total;
	
	add_page_display(ch, "You find the following ammo types:");
	
	total = 0;
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (IS_MISSILE_WEAPON(obj) || IS_AMMO(obj)) {
			add_page_display_col(ch, 2, FALSE, " %c: [%5d] %s", 'A' + GET_OBJ_VAL(obj, IS_AMMO(obj) ? VAL_AMMO_TYPE : VAL_MISSILE_WEAPON_AMMO_TYPE), GET_OBJ_VNUM(obj), skip_filler(GET_OBJ_SHORT_DESC(obj)));
			++total;
		}
	}
	
	if (total == 0) {
		add_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


SHOW(show_author) {
	int idnum;
	size_t count;
	book_data *book, *next_book;
	player_index_data *index;
	
	one_word(argument, arg);
	
	if (!*arg || !isdigit(*arg)) {
		msg_to_char(ch, "Usage: show author <author idnum>\r\n");
	}
	else if ((idnum = atoi(arg)) < 0) {
		msg_to_char(ch, "Show author: Invalid author idnum.\r\n");
	}
	else {
		add_page_display(ch, "Books authored by [%d] %s:", idnum, (index = find_player_index_by_idnum(idnum)) ? index->fullname : "nobody");
		
		count = 0;
		HASH_ITER(hh, book_table, book, next_book) {
			if (BOOK_AUTHOR(book) != idnum) {
				continue;
			}
			
			add_page_display(ch, "[%7d] %s", BOOK_VNUM(book), BOOK_TITLE(book));
			++count;
		}
		
		if (!count) {
			add_page_display_str(ch, "  none");
		}
		
		send_page_display(ch);
	}
}


SHOW(show_buildings) {
	char part[256];
	struct sector_index_type *idx;
	bld_data *bld, *next_bld;
	int count, total, this;
	struct map_data *map;
	sector_data *sect;
	room_data *room;
	bool any;
	
	// fresh numbers
	update_world_count();
	
	if (!*argument) {	// no-arg: show summary
		// output
		total = count = 0;
	
		HASH_ITER(hh, building_table, bld, next_bld) {
			if (IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM)) {
				continue;
			}
			
			this = stats_get_building_count(bld);
			add_page_display_col(ch, 2, FALSE, " %6d &Z%s", this, GET_BLD_NAME(bld));
			total += this;
		}
	
		add_page_display(ch, " Total: %d", total);
		send_page_display(ch);
	}
	// argument usage: show building <vnum | name>
	else if (!(isdigit(*argument) && (bld = building_proto(atoi(argument)))) && !(bld = get_building_by_name(argument, FALSE))) {
		msg_to_char(ch, "Show buildings: Unknown building '%s'.\r\n", argument);
	}
	else if (IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM)) {
		msg_to_char(ch, "Show buildings: This function only works on map buildings, not interior rooms.\r\n");
	}
	else if (!(sect = sector_proto(config_get_int("default_building_sect"))) || !(idx = find_sector_index(GET_SECT_VNUM(sect)))) {
		msg_to_char(ch, "Show buildings: Error looking up buildings: default sector not configured.\r\n");
	}
	else {
		add_page_display(ch, "[%d] %s (%d in world):", GET_BLD_VNUM(bld), GET_BLD_NAME(bld), stats_get_building_count(bld));
		
		any = FALSE;
		LL_FOREACH2(idx->sect_rooms, map, next_in_sect) {
			// building rooms are always instantiated so it's safe to do a real_room without wasting RAM
			room = real_room(map->vnum);
			if (BUILDING_VNUM(room) != GET_BLD_VNUM(bld)) {
				continue;
			}
			
			// found
			if (ROOM_OWNER(room)) {
				snprintf(part, sizeof(part), " - %s%s\t0", EMPIRE_BANNER(ROOM_OWNER(room)), EMPIRE_ADJECTIVE(ROOM_OWNER(room)));
			}
			else {
				*part = '\0';
			}
			add_page_display(ch, "(%*d, %*d) %s%s", X_PRECISION, MAP_X_COORD(map->vnum), Y_PRECISION, MAP_Y_COORD(map->vnum), get_room_name(room, FALSE), part);
			any = TRUE;
		}
		
		if (!any) {
			add_page_display_str(ch, " no matching tiles");
		}
		
		send_page_display(ch);
	}
}


SHOW(show_commons) {
	descriptor_data *d, *nd;
	
	*buf = '\0';
	for (d = descriptor_list; d; d = d->next) {
		if (d->character && get_highest_access_level(GET_ACCOUNT(d->character)) > GET_ACCESS_LEVEL(ch)) {
			continue;
		}
		for (nd = descriptor_list; nd; nd = nd->next) {
			if (!str_cmp(d->host, nd->host) && d != nd && (!d->character || !PLR_FLAGGED(d->character, PLR_IPMASK))) {
				sprintf(buf + strlen(buf), "%s\r\n", d->character ? GET_NAME(d->character) : "<No Player>");
				break;
			}
		}
	}
	msg_to_char(ch, "Common sites:\r\n");
	if (*buf)
		send_to_char(buf, ch);
	else
		msg_to_char(ch, "None.\r\n");
}


SHOW(show_companions) {
	char_data *proto = NULL, *plr = NULL;
	struct companion_data *cd, *next_cd;
	bool found, file = FALSE;
	struct companion_mod *cmod;
	ability_data *abil;
	struct page_display *pd;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show companions <player> [keywords]\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("Show companions: There is no such player.\r\n", ch);
	}
	else {
		check_delayed_load(plr);
		setup_ability_companions(plr);
		
		if (*argument) {
			add_page_display(ch, "Companions matching '%s' for %s:", argument, GET_NAME(plr));
		}
		else {
			add_page_display(ch, "Companions for %s:", GET_NAME(plr));
		}
		
		found = FALSE;
		HASH_ITER(hh, GET_COMPANIONS(plr), cd, next_cd) {
			if (cd->from_abil != NOTHING && !has_ability(plr, cd->from_abil)) {
				continue;	// missing ability: don't show
			}
			if (!(proto = mob_proto(cd->vnum))) {
				continue;	// mob missing
			}
			// check requested keywords
			if (*argument) {
				cmod = get_companion_mod_by_type(cd, CMOD_KEYWORDS);
				if (!multi_isname(argument, cmod ? cmod->str : GET_PC_NAME(proto))) {
					continue;	// no kw match
				}
			}
			
			// build display
			cmod = get_companion_mod_by_type(cd, CMOD_SHORT_DESC);
			pd = add_page_display(ch, " [%5d] %s", cd->vnum, skip_filler(cmod ? cmod->str : get_mob_name_by_proto(cd->vnum, TRUE)));
			
			if (cd->from_abil != NOTHING && (abil = find_ability_by_vnum(cd->from_abil)) && ABIL_COST(abil) > 0) {
				append_page_display_line(pd, " (%d %s)", ABIL_COST(abil), pool_types[ABIL_COST_TYPE(abil)]);
			}
			
			found = TRUE;
		}
		
		if (!found) {
			add_page_display_str(ch, " none");
		}
		send_page_display(ch);
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_components) {
	char part[MAX_STRING_LENGTH];
	obj_data *obj, *next_obj;
	generic_data *cmp;
	
	skip_spaces(&argument);	// component name/vnum
	
	if (!*argument) {
		msg_to_char(ch, "Usage: show components <name | vnum>\r\n");
	}
	else if (!(cmp = find_generic_component(argument))) {
		msg_to_char(ch, "Show components: Unknown generic component type '%s'.\r\n", argument);
	}
	else {
		// preamble
		add_page_display(ch, "Components for [%d] (%s):", GEN_VNUM(cmp), GEN_NAME(cmp));
		
		HASH_ITER(hh, object_table, obj, next_obj) {
			if (!is_component(obj, cmp)) {
				continue;	// wrong type
			}
			
			// show component name if it's not an exact match
			if (GET_OBJ_COMPONENT(obj) != GEN_VNUM(cmp)) {
				snprintf(part, sizeof(part), " (%s)", get_generic_name_by_vnum(GET_OBJ_COMPONENT(obj)));
			}
			else {
				*part = '\0';
			}
			add_page_display(ch, "[%5d] %s%s", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj), part);
		}
		
		send_page_display(ch);
	}
}


SHOW(show_crops) {
	char part[256];
	crop_data *crop, *next_crop;
	int count, total, this;
	struct map_data *map;
	bool any;
	
	// fresh numbers
	update_world_count();
	
	if (!*argument) {	// no-arg: show summary
		// output
		total = count = 0;
	
		HASH_ITER(hh, crop_table, crop, next_crop) {
			this = stats_get_crop_count(crop);
			add_page_display_col(ch, 2, FALSE, " %6d &Z%s", this, GET_CROP_NAME(crop));
			total += this;
		}
	
		add_page_display(ch, " Total: %d", total);
		send_page_display(ch);
	}
	// argument usage: show building <vnum | name>
	else if (!(isdigit(*argument) && (crop = crop_proto(atoi(argument)))) && !(crop = get_crop_by_name(argument))) {
		msg_to_char(ch, "Show crops: Unknown crop '%s'.\r\n", argument);
	}
	else {
		add_page_display(ch, "[%d] &Z%s (%d in world):", GET_CROP_VNUM(crop), GET_CROP_NAME(crop), stats_get_crop_count(crop));
		
		any = FALSE;
		LL_FOREACH(land_map, map) {
			if (map->crop_type != crop) {
				continue;
			}
			
			// room info if possible
			if (map->room && ROOM_OWNER(map->room)) {
				snprintf(part, sizeof(part), " - %s%s\t0", EMPIRE_BANNER(ROOM_OWNER(map->room)), EMPIRE_ADJECTIVE(ROOM_OWNER(map->room)));
			}
			else {
				*part = '\0';
			}
			add_page_display(ch, "(%*d, %*d) %s%s", X_PRECISION, MAP_X_COORD(map->vnum), Y_PRECISION, MAP_Y_COORD(map->vnum), map->room ? get_room_name(map->room, FALSE) : GET_CROP_TITLE(crop), part);
			any = TRUE;
		}
		
		if (!any) {
			add_page_display_str(ch, " no matching tiles");
		}
		
		send_page_display(ch);
	}
}


SHOW(show_currency) {
	char line[MAX_STRING_LENGTH];	
	struct player_currency *cur, *next_cur;
	char_data *plr = NULL;
	bool file = FALSE;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show currency <player>\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("Show currency: There is no such player.\r\n", ch);
	}
	else {
		coin_string(GET_PLAYER_COINS(plr), line);
		add_page_display(ch, "%s has %s.", GET_NAME(plr), line);
	
		if (GET_CURRENCIES(plr)) {
			add_page_display_str(ch, "Currencies:");
		
			HASH_ITER(hh, GET_CURRENCIES(plr), cur, next_cur) {
				add_page_display(ch, "[%5d] %3d %s", cur->vnum, cur->amount, get_generic_string_by_vnum(cur->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(cur->amount)));
			}
		}
		
		send_page_display(ch);
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_dailycycle) {
	quest_data *qst, *next_qst;
	int num, count;
	
	one_argument(argument, arg);
	
	if (!*arg || !isdigit(*arg)) {
		msg_to_char(ch, "Usage: show dailycycle <number>\r\n");
	}
	else if ((num = atoi(arg)) < 0) {
		msg_to_char(ch, "Show dailycycle: Invalid cycle number.\r\n");
	}
	else {
		// see if ANY quests have that cycle
		count = 0;
		HASH_ITER(hh, quest_table, qst, next_qst) {
			if (!IS_DAILY_QUEST(qst) || QUEST_DAILY_CYCLE(qst) != num) {
				continue;
			}
			++count;
		}
		// if we didn't find any, try 1 more thing
		if (count == 0 && (qst = quest_proto(num)) && QUEST_DAILY_CYCLE(qst) != NOTHING && QUEST_DAILY_CYCLE(qst) != QUEST_VNUM(qst)) {
			num = QUEST_DAILY_CYCLE(qst);
		}
		
		add_page_display(ch, "Daily quests with cycle id %d:", num);
		HASH_ITER(hh, quest_table, qst, next_qst) {
			if (!IS_DAILY_QUEST(qst) || QUEST_DAILY_CYCLE(qst) != num) {
				continue;
			}
			
			add_page_display(ch, "[%5d] %s%s%s", QUEST_VNUM(qst), QUEST_NAME(qst), QUEST_DAILY_ACTIVE(qst) ? " (active)" : "", IS_EVENT_QUEST(qst) ? " (event)" : "");
		}
		
		send_page_display(ch);
	}
}


SHOW(show_data) {
	struct stored_data *data, *next_data;
	
	add_page_display_str(ch, "Stored data:");
	
	HASH_ITER(hh, data_table, data, next_data) {
		// DATYPE_x:
		switch (data->keytype) {
			case DATYPE_INT: {
				add_page_display(ch, " %s: %d", stored_data_info[data->key].name, data_get_int(data->key));
				break;
			}
			case DATYPE_LONG: {
				add_page_display(ch, " %s: %ld", stored_data_info[data->key].name, data_get_long(data->key));
				break;
			}
			case DATYPE_DOUBLE: {
				add_page_display(ch, " %s: %f", stored_data_info[data->key].name, data_get_double(data->key));
				break;
			}
			default: {
				add_page_display(ch, " %s: UNKNOWN", stored_data_info[data->key].name);
				break;
			}
		}
	}
	
	send_page_display(ch);
}


SHOW(show_dropped_items) {
	struct empire_dropped_item *edi, *next;
	empire_data *emp;
	int count;
	
	skip_spaces(&argument);
	if (!*argument) {
		msg_to_char(ch, "Usage: show dropped <empire>\r\n");
	}
	else if (!(emp = get_empire_by_name(argument))) {
		msg_to_char(ch, "Show dropped: Unknown empire '%s'.\r\n", argument);
	}
	else {
		add_page_display(ch, "Dropped items for %s%s\t0:", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		count = 0;
		HASH_ITER(hh, EMPIRE_DROPPED_ITEMS(emp), edi, next) {
			add_page_display(ch, "(%d) [%d] %s", edi->count, edi->vnum, get_obj_name_by_proto(edi->vnum));
			SAFE_ADD(count, edi->count, 0, INT_MAX, FALSE);
		}
		if (!count) {
			add_page_display_str(ch, " none");	// always room if !count
		}
		else {
			add_page_display(ch, "(%d total)", count);
		}
		
		send_page_display(ch);
	}
}


SHOW(show_editors) {
	bool any;
	char line[256], part[256];
	int count = 0;
	size_t lsize;
	char_data *targ;
	descriptor_data *desc;
	player_index_data *index;
	
	add_page_display_str(ch, "Players using editors:");
	
	LL_FOREACH(descriptor_list, desc) {
		if (STATE(desc) != CON_PLAYING || !(targ = desc->character)) {
			continue;	// not in-game
		}
		if (GET_INVIS_LEV(targ) > GET_ACCESS_LEVEL(ch)) {
			continue;	// can't see
		}
		
		// start line in case
		lsize = snprintf(line, sizeof(line), "%s", GET_NAME(targ));
		any = FALSE;
		
		// olc?
		if (GET_OLC_TYPE(desc)) {
			any = TRUE;
			if (GET_ACCESS_LEVEL(targ) > GET_ACCESS_LEVEL(ch)) {
				// cannot see due to level
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " - olc");
			}
			else {
				sprintbit(GET_OLC_TYPE(desc), olc_type_bits, part, FALSE);
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " - %s %d", part, GET_OLC_VNUM(desc));
			}
		}
		
		// string editor?
		if (desc->str) {
			any = TRUE;
			if (GET_ACCESS_LEVEL(targ) > GET_ACCESS_LEVEL(ch)) {
				// cannot see due to level
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " - editing a string");
			}
			else if (PLR_FLAGGED(targ, PLR_MAILING)) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " - mailing %s", (index = find_player_index_by_idnum(desc->mail_to)) ? index->fullname : "someone");
			}
			else if (desc->notes_id > 0) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " - editing notes for account %d", desc->notes_id);
			}
			else if (desc->island_desc_id != NOTHING) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " - editing description for island %s", get_island(desc->island_desc_id, TRUE) ? get_island(desc->island_desc_id, TRUE)->name : "???");
			}
			else if (desc->file_storage) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " - editing file %s", desc->file_storage);
			}
			else if (desc->save_empire != NOTHING) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " - editing empire text for %s", real_empire(desc->save_empire) ? EMPIRE_NAME(real_empire(desc->save_empire)) : "an empire");
			}
			else {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " - editing a string");
			}
		}
		
		if (any) {
			add_page_display_str(ch, line);
			++count;
		}
	}
	
	if (!count) {
		add_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


SHOW(show_factions) {
	char name[MAX_INPUT_LENGTH];
	char *arg2;
	struct player_faction_data *pfd, *next_pfd;
	faction_data *fct;
	bool file = FALSE;
	char_data *vict;
	int count = 0;
	int idx;
	
	arg2 = any_one_arg(argument, name);
	skip_spaces(&arg2);
	
	if (!(vict = find_or_load_player(name, &file))) {
		msg_to_char(ch, "Show factions: No player by that name.\r\n");
	}
	else {
		check_delayed_load(vict);
		add_page_display(ch, "%s's factions:", GET_NAME(vict));
		HASH_ITER(hh, GET_FACTIONS(vict), pfd, next_pfd) {
			if (!(fct = find_faction_by_vnum(pfd->vnum))) {
				continue;
			}
			if (*arg2 && !multi_isname(arg2, FCT_NAME(fct))) {
				continue;	// filter
			}
			
			++count;
			idx = rep_const_to_index(pfd->rep);
			add_page_display(ch, "[%5d] %s %s(%s / %d)\t0%s", pfd->vnum, FCT_NAME(fct), reputation_levels[idx].color, reputation_levels[idx].name, pfd->value, (FACTION_FLAGGED(fct, FCT_HIDE_IN_LIST) ? " (hidden)" : ""));
		}
		
		if (!count) {
			add_page_display_str(ch, " none");
		}
		
		send_page_display(ch);
	}
	
	if (file) {
		free_char(vict);
	}
}


SHOW(show_fmessages) {
	int count, iter;
	char_data *plr = NULL;
	bool on, screenreader = (PRF_FLAGGED(ch, PRF_SCREEN_READER) ? TRUE : FALSE), file = FALSE;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show fmessages <player>\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("Show fmessages: There is no such player.\r\n", ch);
	}
	else {
		msg_to_char(ch, "Fight message toggles for %s:\r\n", GET_NAME(plr));
		
		count = 0;
		for (iter = 0; *combat_message_types[iter] != '\n'; ++iter) {
			on = (IS_SET(GET_FIGHT_MESSAGES(plr), BIT(iter)) ? TRUE : FALSE);
			if (screenreader) {
				msg_to_char(ch, "%s: %s\r\n", combat_message_types[iter], on ? "on" : "off");
			}
			else {
				msg_to_char(ch, " [%s%3.3s\t0] %-25.25s%s", on ? "\tg" : "\tr", on ? "on" : "off", combat_message_types[iter], (!(++count % 2) ? "\r\n" : ""));
			}
		}
		
		if (count % 2 && !screenreader) {
			send_to_char("\r\n", ch);
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_friends) {
	bool file = FALSE;
	int count;
	char_data *plr = NULL;
	account_data *acct;
	struct friend_data *friend, *next_friend;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Show friends: List friends for whom?\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		msg_to_char(ch, "Show friends: No player by that name.\r\n");
	}
	else if (get_highest_access_level(GET_ACCOUNT(plr)) > GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "Show friends: You can't do that.\r\n");
	}
	else {
		add_page_display(ch, "Friends list for %s%s:", GET_NAME(plr), PRF_FLAGGED(plr, PRF_NO_FRIENDS) ? " (no-friends toggled on)" : "");
		count = 0;
		
		HASH_ITER(hh, GET_ACCOUNT_FRIENDS(plr), friend, next_friend) {
			if (friend->status == FRIEND_NONE) {
				continue;	// not a friend
			}
			if (!(acct = find_account(friend->account_id))) {
				continue;	// no such account?
			}
			
			// actual line
			add_page_display(ch, " %s - %s", (friend->name ? friend->name : "Unknown"), friend_status_types[friend->status]);
			++count;
		}
		
		if (!count) {
			add_page_display_str(ch, " none");
		}
		
		send_page_display(ch);
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_home) {
	char name[MAX_INPUT_LENGTH];
	bool file = FALSE;
	char_data *vict;
	room_data *home;
	
	any_one_arg(argument, name);
	
	if (!(vict = find_or_load_player(name, &file))) {
		msg_to_char(ch, "Show home: No player by that name.\r\n");
	}
	else {
		home = find_home(vict);
		
		if (!home) {
			msg_to_char(ch, "%s has no home set.\r\n", PERS(vict, ch, TRUE));
		}
		else {
			msg_to_char(ch, "%s's home is at: %s%s\r\n", PERS(vict, ch, TRUE), get_room_name(home, FALSE), coord_display_room(ch, home, FALSE));
		}
	}
	
	if (file) {
		free_char(vict);
	}
}


SHOW(show_homeless) {
	char line[MAX_STRING_LENGTH];
	struct empire_homeless_citizen *ehc;
	struct generic_name_data *nameset;
	size_t lsize;
	empire_data *emp;
	char_data *proto;
	int count;
	
	if (!ch->desc) {
		return;	// somehow
	}
	
	one_word(argument, arg);
	if (!*arg) {
		msg_to_char(ch, "Show homeless: View unhoused citizens for which empire?\r\n");
		return;
	}
	if (!(emp = get_empire_by_name(arg))) {
		msg_to_char(ch, "Show homeless: Unknown empire '%s'.\r\n", arg);
		return;
	}
	
	add_page_display(ch, "Homeless citizens for %s%s\t0:", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
	
	count = 0;
	LL_FOREACH(EMPIRE_HOMELESS_CITIZENS(emp), ehc) {
		if (!(proto = mob_proto(ehc->vnum))) {
			continue;	// no mob
		}
		
		++count;
		
		// location / start of string
		if (ehc->loc) {
			lsize = snprintf(line, sizeof(line), " (%*d, %*d) ", X_PRECISION, MAP_X_COORD(ehc->loc->vnum), Y_PRECISION, MAP_Y_COORD(ehc->loc->vnum));
		}
		else {
			lsize = snprintf(line, sizeof(line), " (%*.*s) ", (X_PRECISION + Y_PRECISION + 2), (X_PRECISION + Y_PRECISION + 2), "no loc");
		}
		
		// load name list
		nameset = get_best_name_list(MOB_NAME_SET(proto), ehc->sex);
		
		// mob portion of the display
		lsize += snprintf(line + lsize, sizeof(line) - lsize, "[%5d] %s '%s'\r\n", ehc->vnum, get_mob_name_by_proto(ehc->vnum, FALSE), ehc->name < nameset->size ? nameset->names[ehc->name] : "unknown");
		
		add_page_display_str(ch, line);
	}
	
	if (count == 0) {
		add_page_display(ch, " none");
	}
	
	send_page_display(ch);
}


SHOW(show_ignoring) {
	player_index_data *index;
	bool found, file = FALSE;
	char_data *vict = NULL;
	int iter;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Show ignoring: View the ignore list for whom?\r\n");
	}
	else if (!(vict = find_or_load_player(arg, &file))) {
		msg_to_char(ch, "Show ignoring: There is no such player.\r\n");
	}
	else if (GET_ACCESS_LEVEL(vict) > GET_ACCESS_LEVEL(ch)) {
		msg_to_char(ch, "Show ignoring: You can't do that.\r\n");
	}
	else {
		// just list ignores
		add_page_display(ch, "%s is ignoring:", GET_NAME(vict));
		
		found = FALSE;
		for (iter = 0; iter < MAX_IGNORES; ++iter) {
			if (GET_IGNORE_LIST(vict, iter) != 0 && (index = find_player_index_by_idnum(GET_IGNORE_LIST(vict, iter)))) {
				add_page_display_col(ch, 4, FALSE, " %s", index->fullname);
				found = TRUE;
			}
		}
		
		if (!found) {
			add_page_display(ch, " nobody");
		}
		
		send_page_display(ch);
	}
	
	if (vict && file) {
		free_char(vict);
	}
}


SHOW(show_inventory) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	player_index_data *index, *next_index;
	struct empire_unique_storage *eus;
	unsigned long long timer;
	obj_vnum vnum;
	bool all = FALSE, loaded;
	char_data *load;
	int count, pos, players, empires;
	empire_data *emp, *next_emp;
	
	half_chop(argument, arg1, arg2);
	all = !str_cmp(arg2, "all") || !str_cmp(arg2, "-all");
	
	if (!ch->desc) {
		// don't bother
	}
	else if (!*arg1 || !isdigit(*arg1) || (*arg2 && !all)) {
		msg_to_char(ch, "Usage: show inventory <obj vnum> [all]\r\n");
	}
	else if ((vnum = atoi(arg1)) < 0 || !obj_proto(vnum)) {
		msg_to_char(ch, "Show inventory: Unknown object vnum '%s'.\r\n", arg1);
	}
	else {
		timer = microtime();
		add_page_display(ch, "Searching%s inventories for %d %s:", (all ? " all" : ""), vnum, get_obj_name_by_proto(vnum));
		players = empires = 0;
		
		// players
		HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
			// determine if we should load them
			if (!all && (time(0) - index->last_logon) > 60 * SECS_PER_REAL_DAY) {
				continue;	// skip due to timeout
			}
			if (!(load = find_or_load_player(index->name, &loaded))) {
				continue;	// no player
			}
			
			// determine if they have any
			check_delayed_load(load);
			count = 0;
			for (pos = 0; pos < NUM_WEARS; ++pos) {
				if (GET_EQ(load, pos)) {
					SAFE_ADD(count, count_objs_by_vnum(vnum, GET_EQ(load, pos)), 0, INT_MAX, FALSE);
				}
			}
			if (load->carrying) {
				SAFE_ADD(count, count_objs_by_vnum(vnum, load->carrying), 0, INT_MAX, FALSE);
			}
			
			DL_FOREACH(GET_HOME_STORAGE(load), eus) {
				if (eus->obj && GET_OBJ_VNUM(eus->obj) == vnum) {
					SAFE_ADD(count, eus->amount, 0, INT_MAX, FALSE);
					// does not have contents in home storage
				}
			}
			
			if (loaded) {
				free_char(load);
			}
			if (count <= 0) {
				continue;	// nothing to show
			}
			
			// build text
			add_page_display(ch, "%s: %d", index->fullname, count);
			++players;
		}
		
		// empires
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if (!all && EMPIRE_IS_TIMED_OUT(emp)) {
				continue;	// skip due to timeout
			}
			
			// fresh count: basic storage
			count = get_total_stored_count(emp, vnum, FALSE);
			
			// and unique storage
			DL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
				if (eus->obj && GET_OBJ_VNUM(eus->obj) == vnum) {
					SAFE_ADD(count, eus->amount, 0, INT_MAX, FALSE);
					// does not have contents in warehouse
				}
			}
			
			if (count <= 0) {
				continue;	// nothing to show
			}
			
			// build text
			add_page_display(ch, "%s%s\t0 (empire): %d", EMPIRE_BANNER(emp), EMPIRE_NAME(emp), count);
			++empires;
		}
		
		add_page_display(ch, "(%d player%s, %d empire%s, %.2f seconds for search)", players, PLURAL(players), empires, PLURAL(empires), (microtime() - timer) / 1000000.0);
		
		send_page_display(ch);
	}
}


SHOW(show_languages) {
	struct player_language *lang, *next_lang;
	generic_data *gen;
	char_data *plr = NULL;
	size_t count;
	bool file = FALSE;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show languages <player> [keywords]\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("Show languages: There is no such player.\r\n", ch);
	}
	else {
		if (*argument) {
			add_page_display(ch, "Languages matching '%s' for %s:", argument, GET_NAME(plr));
		}
		else {
			add_page_display(ch, "Languages for %s:", GET_NAME(plr));
		}
		
		count = 0;
		HASH_ITER(hh, GET_LANGUAGES(plr), lang, next_lang) {
			if (lang->level == LANG_UNKNOWN || !(gen = find_generic(lang->vnum, GENERIC_LANGUAGE))) {
				continue;
			}
			if (*argument && !multi_isname(argument, GEN_NAME(gen))) {
				continue;	// searched
			}
		
			// show it
			add_page_display(ch, " [%5d] %s (%s)%s%s", GEN_VNUM(gen), GEN_NAME(gen), language_types[lang->level], (GET_SPEAKING(plr) == lang->vnum) ? " - \tgcurrently speaking\t0" : "", (GET_LOYALTY(plr) && speaks_language_empire(GET_LOYALTY(plr), lang->vnum) == lang->level) ? ", from empire" : "");
			++count;
		}
	
		if (!count) {
			add_page_display_str(ch, "  none");;
		}
	
		send_page_display(ch);
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_lastnames) {
	struct player_lastname *lastn;
	bool file = FALSE, cur;
	char_data *plr = NULL;
	int count;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show lastnames <player> [keywords]\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("Show lastnames: There is no such player.\r\n", ch);
	}
	else {
		check_delayed_load(plr);
		
		count = 0;
		if (*argument) {
			add_page_display(ch, "Lastnames matching '%s' for %s:", argument, GET_NAME(plr));
		}
		else {
			add_page_display(ch, "Lastnames for %s:", GET_NAME(plr));
		}
		
		if (GET_PERSONAL_LASTNAME(plr)) {
			cur = GET_CURRENT_LASTNAME(plr) && !str_cmp(GET_PERSONAL_LASTNAME(plr), GET_CURRENT_LASTNAME(plr));
			add_page_display(ch, "%s%2d. %s (personal)%s", (cur ? "\tg" : ""), ++count, GET_PERSONAL_LASTNAME(plr), (cur ? " (current)\t0" : ""));
		}
		
		LL_FOREACH(GET_LASTNAME_LIST(plr), lastn) {
			if (*argument && !multi_isname(argument, lastn->name)) {
				continue;	// searched
			}
		
			// show it
			cur = GET_CURRENT_LASTNAME(plr) && !str_cmp(NULLSAFE(lastn->name), GET_CURRENT_LASTNAME(plr));
			add_page_display(ch, "%s%2d. %s%s", (cur ? "\tg" : ""), ++count, NULLSAFE(lastn->name), (cur ? " (current)\t0" : ""));
		}
	
		if (!count) {
			add_page_display_str(ch, " none");
		}
	
		send_page_display(ch);
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_learned) {
	struct player_craft_data *pcd, *next_pcd;
	empire_data *emp = NULL;
	char_data *plr = NULL;
	size_t count;
	craft_data *craft;
	bool file = FALSE;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	// arg parsing
	if (!*arg) {
		msg_to_char(ch, "Usage: show learned <player | empire>\r\n");
		return;
	}
	else if (is_abbrev(arg, "empire") && strlen(arg) >= 3) {
		argument = one_word(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "Show learned: View which empire's learned recipes?\r\n");
			return;
		}
		else if (!(emp = get_empire_by_name(arg))) {
			msg_to_char(ch, "show learned: No such empire '%s'.\r\n", arg);
			return;
		}
	}
	else if (is_abbrev(arg, "player")) {
		argument = one_word(argument, arg);
		if (!*arg) {
			msg_to_char(ch, "Show learned: View which player's learned recipes?\r\n");
			return;
		}
		else if (!(plr = find_or_load_player(arg, &file))) {
			msg_to_char(ch, "Show learned: No such player '%s'.\r\n", arg);
			return;
		}
	}
	else if (!(plr = find_or_load_player(arg, &file)) && !(emp = get_empire_by_name(arg))) {
		send_to_char("Show learned: There is no such player or empire.\r\n", ch);
		return;
	}
	
	// must have plr or emp by now
	if (*argument) {
		add_page_display(ch, "Learned recipes matching '%s' for %s:", argument, plr ? GET_NAME(plr) : EMPIRE_NAME(emp));
	}
	else {
		add_page_display(ch, "Learned recipes for %s:", plr ? GET_NAME(plr) : EMPIRE_NAME(emp));
	}
	
	count = 0;
	HASH_ITER(hh, (plr ? GET_LEARNED_CRAFTS(plr) : EMPIRE_LEARNED_CRAFTS(emp)), pcd, next_pcd) {
		if (!(craft = craft_proto(pcd->vnum))) {
			continue;	// no craft?
		}
		if (CRAFT_FLAGGED(craft, CRAFT_IN_DEVELOPMENT)) {
			continue;	// in-dev
		}
		if (*argument && !multi_isname(argument, GET_CRAFT_NAME(craft))) {
			continue;	// searched
		}
	
		// show it
		add_page_display(ch, " [%5d] %s", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
		++count;
	}

	if (!count) {
		add_page_display_str(ch, "  none");
	}

	send_page_display(ch);
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_libraries) {
	size_t count;
	book_data *book;
	room_data *room;
	struct library_info *libr, *next_libr;
	
	one_word(argument, arg);
	
	if (!*arg || !isdigit(*arg)) {
		msg_to_char(ch, "Usage: show libraries <book vnum>\r\n");
	}
	else if (!(book = book_proto(atoi(arg)))) {
		msg_to_char(ch, "Show libraries: No such book %d.\r\n", atoi(arg));
	}
	else {
		add_page_display(ch, "Library locations for [%d] %s:", BOOK_VNUM(book), BOOK_TITLE(book));
		
		count = 0;
		HASH_ITER(hh, library_table, libr, next_libr) {
			if (!(room = real_room(libr->room))) {
				continue;
			}
			
			add_page_display(ch, "[%7d] %s", GET_ROOM_VNUM(room), get_room_name(room, FALSE));
			++count;
		}
		
		if (!count) {
			add_page_display_str(ch, "  none");
		}
		
		send_page_display(ch);
	}
}


SHOW(show_lost_books) {
	size_t count;
	book_data *book, *next_book;
	player_index_data *index;
	
	add_page_display_str(ch, "Books not in any libraries:");
	
	count = 0;
	HASH_ITER(hh, book_table, book, next_book) {
		if (find_book_in_any_library(BOOK_VNUM(book))) {
			continue;
		}
		
		add_page_display(ch, "[%7d] %s (%s)", BOOK_VNUM(book), BOOK_TITLE(book), (index = find_player_index_by_idnum(BOOK_AUTHOR(book))) ? index->fullname : "???");
		++count;
	}
	
	if (!count) {
		add_page_display_str(ch, "  none");
	}
	
	send_page_display(ch);
}


SHOW(show_minipets) {
	struct minipet_data *mini, *next_mini;
	char_data *mob, *plr = NULL;
	size_t count;
	bool file = FALSE;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show minipets <player> [keywords]\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("Show minipets: There is no such player.\r\n", ch);
	}
	else {
		if (*argument) {
			add_page_display(ch, "Minipets matching '%s' for %s:", argument, GET_NAME(plr));
		}
		else {
			add_page_display(ch, "Minipets for %s:", GET_NAME(plr));
		}
		
		count = 0;
		HASH_ITER(hh, GET_MINIPETS(plr), mini, next_mini) {
			if (!(mob = mob_proto(mini->vnum))) {
				continue;	// no mob?
			}
			if (*argument && !multi_isname(argument, GET_PC_NAME(mob))) {
				continue;	// searched
			}
		
			// show it
			add_page_display_col(ch, 2, FALSE, " [%5d] %s", GET_MOB_VNUM(mob), skip_filler(GET_SHORT_DESC(mob)));
			++count;
		}
	
		if (!count) {
			add_page_display_str(ch, "  none");
		}
		
		send_page_display(ch);
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_moons) {
	generic_data *moon, *next_gen;
	struct time_info_data tinfo;
	moon_phase_t phase;
	moon_pos_t pos;
	int count;
	
	tinfo = get_local_time(IN_ROOM(ch));
	
	add_page_display_str(ch, "Moons:");
	
	count = 0;
	HASH_ITER(hh, generic_table, moon, next_gen) {
		if (GEN_TYPE(moon) != GENERIC_MOON || GET_MOON_CYCLE(moon) < 1) {
			continue;	// not a moon or invalid cycle
		}
		
		// find moon in the sky
		phase = get_moon_phase(GET_MOON_CYCLE_DAYS(moon));
		pos = get_moon_position(phase, tinfo.hours);
		
		// ok: show it
		++count;
		add_page_display(ch, "[%5d] %s: %s, %s (%.2f day%s)%s", GEN_VNUM(moon), GEN_NAME(moon), moon_phases[phase], moon_positions[pos], GET_MOON_CYCLE_DAYS(moon), PLURAL(GET_MOON_CYCLE_DAYS(moon)), GEN_FLAGGED(moon, GEN_IN_DEVELOPMENT) ? " (in-development)" : "");
	}
	
	if (!count) {
		add_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


SHOW(show_mounts) {
	struct mount_data *mount, *next_mount;
	char_data *mob, *plr = NULL;
	size_t count;
	bool file = FALSE;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show mounts <player>\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("Show mounts: There is no such player.\r\n", ch);
	}
	else {
		if (*argument) {
			add_page_display(ch, "Mounts matching '%s' for %s:", argument, GET_NAME(plr));
		}
		else {
			add_page_display(ch, "Mounts for %s:", GET_NAME(plr));
		}
		
		count = 0;
		HASH_ITER(hh, GET_MOUNT_LIST(plr), mount, next_mount) {
			if (!(mob = mob_proto(mount->vnum))) {
				continue;	// no mob?
			}
			if (*argument && !multi_isname(argument, GET_PC_NAME(mob))) {
				continue;	// searched
			}
		
			// show it
			add_page_display_col(ch, 2, FALSE, " [%5d] %s", GET_MOB_VNUM(mob), skip_filler(GET_SHORT_DESC(mob)));
			++count;
		}
	
		if (!count) {
			add_page_display_str(ch, "  none");
		}
		
		send_page_display(ch);
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_notes) {
	char buf[MAX_STRING_LENGTH];
	player_index_data *index = NULL;
	account_data *acct = NULL;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: show notes <player>\r\n");
		return;
	}
	
	// argument parsing
	if (isdigit(*argument)) {
		if (!(acct = find_account(atoi(argument)))) {
			msg_to_char(ch, "Show notes: Unknown account '%s'.\r\n", argument);
			return;
		}
		if (get_highest_access_level(acct) > GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "Show notes: You can't show notes for accounts of that level.\r\n");
			return;
		}
	}
	else {
		if (!(index = find_player_index_by_name(argument))) {
			msg_to_char(ch, "Show notes: There is no such player.\r\n");
			return;
		}
		if (!(acct = find_account(index->account_id))) {
			msg_to_char(ch, "Show notes: There are no notes for that player.\r\n");
			return;
		}
		if (get_highest_access_level(acct) > GET_ACCESS_LEVEL(ch)) {
			msg_to_char(ch, "Show notes: You can't show notes for accounts of that level.\r\n");
			return;
		}
	}
	
	// final checks
	if (!acct) {	// in case somehow
		msg_to_char(ch, "Show notes: Unknown account.\r\n");
	}
	else if (!acct->notes || !*acct->notes) {
		msg_to_char(ch, "There are no notes for that account.\r\n");
	}
	else {
		if (index) {
			strcpy(buf, index->fullname);
		}
		else {
			sprintf(buf, "account %d", acct->id);
		}
		msg_to_char(ch, "Admin notes for %s:\r\n%s", buf, acct->notes);
	}
}


SHOW(show_oceanmobs) {
	char_data *mob;
	int count = 0;
	
	add_page_display_str(ch, "Mobs lost in the ocean:");
	
	DL_FOREACH(character_list, mob) {
		if (!IS_NPC(mob)) {
			continue;	// player
		}
		if (MOB_FLAGGED(mob, MOB_SPAWNED)) {
			continue;	// is spawned
		}
		if (MOB_INSTANCE_ID(mob) != NOTHING) {
			continue;	// adventure mob
		}
		if (!IN_ROOM(mob)) {
			continue;	// somehow not in a room
		}
		if (!SHARED_DATA(IN_ROOM(mob)) || SHARED_DATA(IN_ROOM(mob)) != &ocean_shared_data) {
			continue;	// not in the ocean
		}
		
		// ok show it:
		++count;
		add_page_display(ch, "[%5d] %s - %s", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob), room_log_identifier(IN_ROOM(mob)));
	}
	
	if (!count) {
		add_page_display_str(ch, " none");
	}
	
	// and send
	send_page_display(ch);
}


SHOW(show_olc) {
	char buf[MAX_STRING_LENGTH];
	player_index_data *index, *next_index;
	any_vnum vnum = NOTHING;
	bool file, any = FALSE;
	char_data *pers;
	
	one_argument(argument, arg);
	if (*arg && (!isdigit(*arg) || (vnum = atoi(arg)) < 0)) {
		msg_to_char(ch, "Usage: show olc [optional vnum]\r\n");
		return;
	}
	
	// heading
	if (vnum != NOTHING) {
		msg_to_char(ch, "Players with OLC access for vnum %d:\r\n", vnum);
	}
	else {
		msg_to_char(ch, "Players with OLC access:\r\n");
	}
	
	// check all players
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
		if ((pers = find_or_load_player(index->name, &file))) {
			if (vnum != NOTHING ? ((GET_OLC_MIN_VNUM(pers) <= vnum && GET_OLC_MAX_VNUM(pers) >= vnum) || GET_ACCESS_LEVEL(pers) >= LVL_UNRESTRICTED_BUILDER || OLC_FLAGGED(pers, OLC_FLAG_ALL_VNUMS)) : (GET_ACCESS_LEVEL(pers) >= LVL_UNRESTRICTED_BUILDER || OLC_FLAGGED(pers, OLC_FLAG_ALL_VNUMS) || GET_OLC_MIN_VNUM(pers) > 0 || GET_OLC_MAX_VNUM(pers) > 0)) {
				sprintbit(GET_OLC_FLAGS(pers), olc_flag_bits, buf, TRUE);
				msg_to_char(ch, " %s [&c%d-%d&0] &g%s&0\r\n", GET_PC_NAME(pers), GET_OLC_MIN_VNUM(pers), GET_OLC_MAX_VNUM(pers), buf);
				any = TRUE;
			}
			
			if (file) {
				free_char(pers);
			}
		}
	}
	
	if (!any) {
		msg_to_char(ch, "none\r\n");
	}
}


SHOW(show_piles) {
	char owner[256];
	room_data *room, *next_room;
	int count, max = 100;
	obj_data *obj, *sub;
	vehicle_data *veh;
	bool any;
	
	if (*argument && isdigit(*argument)) {
		max = atoi(argument);
	}
	
	add_page_display(ch, "Piles of %d item%s or more:", max, PLURAL(max));
	
	any = FALSE;
	HASH_ITER(hh, world_table, room, next_room) {
		count = 0;
		DL_FOREACH2(ROOM_CONTENTS(room), obj, next_content) {
			++count;
			
			DL_FOREACH2(obj->contains, sub, next_content) {
				++count;
			}
		}
		DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
			DL_FOREACH2(VEH_CONTAINS(veh), sub, next_content) {
				++count;
			}
		}
		
		if (count >= max) {
			if (ROOM_OWNER(room)) {
				snprintf(owner, sizeof(owner), " (%s%s\t0)", EMPIRE_BANNER(ROOM_OWNER(room)), EMPIRE_ADJECTIVE(ROOM_OWNER(room)));
			}
			else {
				*owner = '\0';
			}
			add_page_display(ch, "[%d] %s: %d item%s%s", GET_ROOM_VNUM(room), get_room_name(room, FALSE), count, PLURAL(count), owner);
			any = TRUE;
		}
	}
	
	if (!any) {
		add_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


SHOW(show_player) {
	char birth[80], lastlog[80];
	double days_played, avg_min_per_day;
	char_data *plr = NULL;
	bool file = FALSE;
	
	if (!*argument) {
		send_to_char("Show player: A name would help.\r\n", ch);
		return;
	}
	
	if (!(plr = find_or_load_player(argument, &file))) {
		send_to_char("Show player: There is no such player.\r\n", ch);
		return;
	}
	sprintf(buf, "Player: %-12s (%s) [%d]\r\n", GET_PC_NAME(plr), genders[(int) GET_REAL_SEX(plr)], GET_ACCESS_LEVEL(plr));
	strcpy(birth, ctime(&plr->player.time.birth));
	strcpy(lastlog, ctime(&plr->prev_logon));
	// Www Mmm dd hh:mm:ss yyyy
	sprintf(buf + strlen(buf), "Started: %-16.16s %4.4s   Last: %-16.16s %4.4s\r\n", birth, birth+20, lastlog, lastlog+20);
	
	if (get_highest_access_level(GET_ACCOUNT(plr)) <= GET_ACCESS_LEVEL(ch) && GET_ACCESS_LEVEL(ch) >= LVL_TO_SEE_ACCOUNTS) {
		sprintf(buf + strlen(buf), "Creation host: %s\r\n", NULLSAFE(GET_CREATION_HOST(plr)));
	}
	
	days_played = (double)(time(0) - plr->player.time.birth) / SECS_PER_REAL_DAY;
	avg_min_per_day = (((double) plr->player.time.played / SECS_PER_REAL_HOUR) / days_played) * SECS_PER_REAL_MIN;
	
	sprintf(buf + strlen(buf), "Played: %3dh %2dm (%d minutes per day)\r\n", (int) (plr->player.time.played / SECS_PER_REAL_HOUR), (int) (plr->player.time.played % SECS_PER_REAL_HOUR) / SECS_PER_REAL_MIN, (int)avg_min_per_day);
	
	send_to_char(buf, ch);
	
	if (file) {
		free_char(plr);
	}
}


SHOW(show_players) {
	char_data *vict;
	descriptor_data *d;
	
	*buf = '\0';

	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) != CON_PLAYING || !(vict = d->character))
			continue;
		if (!CAN_SEE(ch, vict))
			continue;

		sprintf(buf + strlen(buf), " %-20.20s &g%4d&0/&g%4d&0 &y%4d&0/&y%4d&0 &c%4d&0/&c%4d&0", PERS(vict, vict, 1), GET_HEALTH(vict), GET_MAX_HEALTH(vict), GET_MOVE(vict), GET_MAX_MOVE(vict), GET_MANA(vict), GET_MAX_MANA(vict));
		if (IS_VAMPIRE(vict))
			sprintf(buf + strlen(buf), " &r%2d&0/&r%2d&0", GET_BLOOD(vict), GET_MAX_BLOOD(vict));

		strcat(buf, "\r\n");
	}

	msg_to_char(ch, "Players:\r\n %-20.20s %-9.9s %-9.9s %-9.9s %s\r\n %-20.20s %-9.9s %-9.9s %-9.9s %s\r\n%s", "Name", " Health", " Move", " Mana", "Blood", "----", "---------", "---------", "---------", "-----", buf);
}


SHOW(show_produced) {
	struct empire_production_total *egt, *next_egt;
	empire_data *emp = NULL;
	obj_vnum vnum = NOTHING;
	size_t count;
	obj_data *obj;
	
	argument = any_one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show produced <empire>\r\n");
	}
	else if (!(emp = get_empire_by_name(arg))) {
		send_to_char("Show produced: There is no such empire.\r\n", ch);
	}
	else {
		if (*argument) {
			add_page_display(ch, "Produced items for matching '%s' for %s%s\t0:", argument, EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		}
		else {
			add_page_display(ch, "Produced items for %s%s\t0:", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		}
		
		// check if argument is a vnum
		if (isdigit(*argument)) {
			vnum = atoi(argument);
		}
		
		count = 0;
		HASH_SORT(EMPIRE_PRODUCTION_TOTALS(emp), sort_empire_production_totals);
		HASH_ITER(hh, EMPIRE_PRODUCTION_TOTALS(emp), egt, next_egt) {
			if (!(obj = egt->proto)) {
				continue;	// no obj?
			}
			if (*argument && vnum != egt->vnum && !multi_isname(argument, GET_OBJ_KEYWORDS(obj))) {
				continue;	// searched
			}
		
			// show it
			++count;
			if (egt->imported || egt->exported) {
				add_page_display(ch, " [%5d] %s: %d (%d/%d)", GET_OBJ_VNUM(obj), skip_filler(GET_OBJ_SHORT_DESC(obj)), egt->amount, egt->imported, egt->exported);
			}
			else {
				add_page_display(ch, " [%5d] %s: %d", GET_OBJ_VNUM(obj), skip_filler(GET_OBJ_SHORT_DESC(obj)), egt->amount);
			}
		}
	
		if (!count) {
			add_page_display_str(ch, "  none");
		}
	
		send_page_display(ch);
	}
}


SHOW(show_progress) {
	int total = 0, active = 0, active_completed = 0, total_completed = 0;
	empire_data *emp, *next_emp;
	progress_data *prg;
	bool t_o;
	
	if (!*argument) {
		msg_to_char(ch, "Show progress: Which progression goal?\r\n");
	}
	else if ((isdigit(*argument) && !(prg = real_progress(atoi(argument)))) || (!isdigit(*argument) && !(prg = find_progress_goal_by_name(argument)))) {
		msg_to_char(ch, "Show progress: Unknown goal '%s'.\r\n", argument);
	}
	else {
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if (EMPIRE_IMM_ONLY(emp)) {
				continue;	// safe to skip these
			}
			
			++total;
			
			if (!(t_o = EMPIRE_IS_TIMED_OUT(emp))) {
				++active;
			}
			
			if (empire_has_completed_goal(emp, PRG_VNUM(prg))) {
				++total_completed;
				if (!t_o) {
					++active_completed;
				}
			}
		}
		
		if (total == 0) {
			msg_to_char(ch, "No player empires found; nobody has completed that goal.\r\n");
		}
		else {
			msg_to_char(ch, "%d completed that goal out of %d total empire%s (%d%%).\r\n", total_completed, total, PLURAL(total), (total_completed * 100 / total));
		}
		if (active == 0) {
			msg_to_char(ch, "No active empires found.\r\n");
		}
		else {
			msg_to_char(ch, "%d active empire%s completed that goal out of %d total active (%d%%).\r\n", active_completed, PLURAL(active_completed), active, (active_completed * 100 / active));
		}
	}
}


SHOW(show_progression) {
	int goals[NUM_PROGRESS_TYPES], rewards[NUM_PROGRESS_TYPES], cost[NUM_PROGRESS_TYPES], value[NUM_PROGRESS_TYPES];
	progress_data *prg, *next_prg;
	int iter, tot_v, tot_c;
	
	tot_v = tot_c = 0;
	for (iter = 0; iter < NUM_PROGRESS_TYPES; ++iter) {
		// init
		goals[iter] = rewards[iter] = cost[iter] = value[iter] = 0;
	}
	
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT)) {
			continue;
		}
		
		if (PRG_FLAGGED(prg, PRG_PURCHASABLE)) {
			++rewards[PRG_TYPE(prg)];
		}
		else {
			++goals[PRG_TYPE(prg)];
		}
		
		cost[PRG_TYPE(prg)] += PRG_COST(prg);
		tot_c += PRG_COST(prg);
		value[PRG_TYPE(prg)] += PRG_VALUE(prg);
		tot_v += PRG_VALUE(prg);
	}
	
	msg_to_char(ch, "Stats on active progression entries:\r\n");
	for (iter = 0; iter < NUM_PROGRESS_TYPES; ++iter) {
		msg_to_char(ch, "%s: %d goal%s (%d point%s), %d reward%s (%d total cost)\r\n", progress_types[iter], goals[iter], PLURAL(goals[iter]), value[iter], PLURAL(value[iter]), rewards[iter], PLURAL(rewards[iter]), cost[iter]);
	}
	msg_to_char(ch, "Total: %d point%s, %d total cost\r\n", tot_v, PLURAL(tot_v), tot_c);
}


SHOW(show_quests) {
	char name[MAX_INPUT_LENGTH], *arg2, when[256];
	struct player_completed_quest *pcq, *next_pcq;
	bool file = FALSE, found = FALSE;
	struct player_quest *pq;
	int count, total, diff;
	quest_data *qst;
	char_data *vict = NULL;
	
	arg2 = any_one_arg(argument, name);
	skip_spaces(&arg2);
	
	if (!*name) {
		msg_to_char(ch, "Show quests: Which player?\r\n");
	}
	else if (!(vict = find_or_load_player(name, &file))) {
		msg_to_char(ch, "Show quests: No player by that name.\r\n");
	}
	else if (!*arg2 || is_abbrev(arg2, "active")) {
		// active quest list
		check_delayed_load(vict);
		if (IS_NPC(vict) || !GET_QUESTS(vict)) {
			msg_to_char(ch, "%s is not on any quests (%d/%d dailies, %d/%d event dailies).\r\n", GET_NAME(vict), GET_DAILY_QUESTS(vict), config_get_int("dailies_per_day"), GET_EVENT_DAILY_QUESTS(vict), config_get_int("dailies_per_day"));
			if (vict && file) {
				file = FALSE;
				free_char(vict);
			}
			return;
		}
		
		add_page_display(ch, "%s's quests (%d/%d dailies, %d/%d event dailies):", GET_NAME(vict), GET_DAILY_QUESTS(vict), config_get_int("dailies_per_day"), GET_EVENT_DAILY_QUESTS(vict), config_get_int("dailies_per_day"));
		LL_FOREACH(GET_QUESTS(vict), pq) {
			count_quest_tasks(pq->tracker, &count, &total);
			add_page_display(ch, "[%5d] %s (%d/%d tasks)", pq->vnum, get_quest_name_by_proto(pq->vnum), count, total);
		}
		
		send_page_display(ch);
	}	// end "active"
	else if (is_abbrev(arg2, "completed")) {
		// completed quest list
		check_delayed_load(vict);
		if (IS_NPC(vict) || !GET_COMPLETED_QUESTS(vict)) {
			msg_to_char(ch, "%s has not completed any quests.\r\n", GET_NAME(vict));
			if (vict && file) {
				file = FALSE;
				free_char(vict);
			}
			return;
		}
		
		// sort now
		HASH_SORT(GET_COMPLETED_QUESTS(vict), sort_completed_quests_by_timestamp);
		
		add_page_display(ch, "%s's completed quests:", GET_NAME(vict));
		HASH_ITER(hh, GET_COMPLETED_QUESTS(vict), pcq, next_pcq) {
			if (time(0) - pcq->last_completed < SECS_PER_REAL_DAY) {
				diff = (time(0) - pcq->last_completed) / SECS_PER_REAL_HOUR;
				snprintf(when, sizeof(when), "(%d hour%s ago)", diff, PLURAL(diff));
			}
			else {
				diff = (time(0) - pcq->last_completed) / SECS_PER_REAL_DAY;
				snprintf(when, sizeof(when), "(%d day%s ago)", diff, PLURAL(diff));
			}
			
			add_page_display(ch, "[%5d] %s %s", pcq->vnum, get_quest_name_by_proto(pcq->vnum), when);
		}
		
		send_page_display(ch);
	}
	else {
		// show one active quest's tracker
		check_delayed_load(vict);
		LL_FOREACH(GET_QUESTS(vict), pq) {
			if (!(qst = quest_proto(pq->vnum))) {
				continue;
			}
			
			if (multi_isname(arg2, QUEST_NAME(qst))) {
				msg_to_char(ch, "%s ", QUEST_NAME(qst));	// followed by "Quest Tracker:"
				show_quest_tracker(ch, pq);
				found = TRUE;
				break;	// show just one
			}
		}
		
		if (!found) {
			msg_to_char(ch, "%s is not on a quest called '%s'.\r\n", GET_NAME(vict), arg2);
		}
	}
	
	if (file && vict) {
		free_char(vict);
	}
}


SHOW(show_rent) {
	if (!*argument) {
		send_to_char("Show rent: A name would help.\r\n", ch);
		return;
	}

	Crash_listrent(ch, argument);
}


SHOW(show_shops) {
	struct shop_temp_list *stl, *shop_list = NULL;
	char buf[MAX_STRING_LENGTH];
	
	msg_to_char(ch, "Shops here:\r\n");
	
	shop_list = build_available_shop_list(ch);
	LL_FOREACH(shop_list, stl) {
		// determine shopkeeper
		if (stl->from_mob) {
			snprintf(buf, sizeof(buf), " (%s)", PERS(stl->from_mob, ch, FALSE));
		}
		else if (stl->from_obj) {
			snprintf(buf, sizeof(buf), " (%s)", GET_OBJ_SHORT_DESC(stl->from_obj));
		}
		else if (stl->from_veh) {
			snprintf(buf, sizeof(buf), " (%s)", VEH_SHORT_DESC(stl->from_veh));
		}
		else if (stl->from_room) {
			strcpy(buf, " (room)");
		}
		else {
			*buf = '\0';
		}
		
		msg_to_char(ch, "[%5d] %s%s%s\r\n", SHOP_VNUM(stl->shop), SHOP_NAME(stl->shop), buf, SHOP_FLAGGED(stl->shop, SHOP_IN_DEVELOPMENT) ? " (IN-DEV)" : "");
	}
	
	if (!shop_list) {
		msg_to_char(ch, " none\r\n");
	}
	
	free_shop_temp_list(shop_list);
}


SHOW(show_site) {
	player_index_data *index, *next_index;
	bool any = FALSE;
	
	if (!*argument) {
		msg_to_char(ch, "Show site: Locate players from what site?\r\n");
		return;
	}
	
	add_page_display(ch, "Players from site %s:", argument);
	
	HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
		if (get_highest_access_level(find_account(index->account_id)) > GET_ACCESS_LEVEL(ch)) {
			continue;
		}
		if (str_str(index->last_host, argument)) {
			add_page_display_col(ch, 4, FALSE, " &Z%s", index->name);
			any = TRUE;
		}
	}
	if (!any) {
		add_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


SHOW(show_skills) {
	char exp_part[256];
	struct player_ability_data *plab, *next_plab;
	struct player_skill_data *plsk, *next_plsk;
	ability_data *abil;
	skill_data *skill;
	char_data *vict;
	bool found, is_file = FALSE;
	int set;
	
	argument = one_argument(argument, arg);
	skip_spaces(&argument);
	
	if (!(vict = find_or_load_player(arg, &is_file))) {
		msg_to_char(ch, "Show skills: Which player?\r\n");
		return;
	}
		
	if (REAL_NPC(vict)) {
		msg_to_char(ch, "Show skills: You can't show skills on an NPC.\r\n");
		if (is_file) {
			// unlikely to get here, but playing it safe
			free_char(vict);
		}
		return;
	}
	
	// detect "swap" arg
	set = GET_CURRENT_SKILL_SET(vict);
	if (*argument && is_abbrev(argument, "swap")) {
		// note: this ONLY supports 2 different sets
		set = (!set ? 1 : 0);
	}
	
	msg_to_char(ch, "Skills for %s:\r\n", PERS(vict, ch, TRUE));
	
	HASH_ITER(hh, GET_SKILL_HASH(vict), plsk, next_plsk) {
		skill = plsk->ptr;
		
		if (plsk->noskill) {
			snprintf(exp_part, sizeof(exp_part), "noskill");
		}
		else {
			snprintf(exp_part, sizeof(exp_part), "%.1f%%", get_skill_exp(vict, SKILL_VNUM(skill)));
		}
		
		msg_to_char(ch, "&y%s&0 [%d, %s, %d%s]: ", SKILL_NAME(skill), get_skill_level(vict, SKILL_VNUM(skill)), exp_part, get_ability_points_available_for_char(vict, SKILL_VNUM(skill)), plsk->resets ? "*" : "");
		
		found = FALSE;
		HASH_ITER(hh, GET_ABILITY_HASH(vict), plab, next_plab) {
			abil = plab->ptr;
			
			if (!plab->purchased[set]) {
				continue;
			}
			if (ABIL_ASSIGNED_SKILL(abil) != skill) {
				continue;
			}

			msg_to_char(ch, "%s%s%s&0", (found ? ", " : ""), ability_color(vict, abil), ABIL_NAME(abil));
			found = TRUE;
		}
		
		msg_to_char(ch, "%s\r\n", (found ? "" : "none"));
	}
	
	msg_to_char(ch, "&ySynergy&0: &g");
	found = FALSE;
	HASH_ITER(hh, GET_ABILITY_HASH(vict), plab, next_plab) {
		abil = plab->ptr;
		
		if (!plab->purchased[GET_CURRENT_SKILL_SET(vict)]) {
			continue;	// not in current set
		}
		if (!ABIL_IS_SYNERGY(abil)) {
			continue;	// only looking for synergy abilities
		}

		msg_to_char(ch, "%s%s", (found ? ", " : ""), ABIL_NAME(abil));
		found = TRUE;
	}
	msg_to_char(ch, "&0%s\r\n", (found ? "" : "none"));
	
	msg_to_char(ch, "&yOther&0: &g");
	found = FALSE;
	HASH_ITER(hh, GET_ABILITY_HASH(vict), plab, next_plab) {
		abil = plab->ptr;
		
		if (!plab->purchased[GET_CURRENT_SKILL_SET(vict)]) {
			continue;	// not in current set
		}
		if (!has_ability_data_any(abil, ADL_PARENT)) {
			continue;	// only looking for abilities with parents
		}
		
		msg_to_char(ch, "%s%s", (found ? ", " : ""), ABIL_NAME(abil));
		found = TRUE;
	}
	msg_to_char(ch, "&0%s\r\n", (found ? "" : "none"));
	
	// available summary
	msg_to_char(ch, "Available daily bonus experience points: %d\r\n", GET_DAILY_BONUS_EXPERIENCE(vict));
	
	if (is_file) {
		free_char(vict);
	}
}


SHOW(show_smessages) {
	int count, iter;
	char_data *plr = NULL;
	bool on, screenreader = (PRF_FLAGGED(ch, PRF_SCREEN_READER) ? TRUE : FALSE), file = FALSE;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show smessages <player>\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("Show smessages: There is no such player.\r\n", ch);
	}
	else {
		msg_to_char(ch, "Status message toggles for %s:\r\n", GET_NAME(plr));
		
		count = 0;
		for (iter = 0; *status_message_types[iter] != '\n'; ++iter) {
			on = (IS_SET(GET_STATUS_MESSAGES(plr), BIT(iter)) ? TRUE : FALSE);
			if (screenreader) {
				msg_to_char(ch, "%s: %s\r\n", status_message_types[iter], on ? "on" : "off");
			}
			else {
				msg_to_char(ch, " [%s%3.3s\t0] %-25.25s%s", on ? "\tg" : "\tr", on ? "on" : "off", status_message_types[iter], (!(++count % 2) ? "\r\n" : ""));
			}
		}
		
		if (count % 2 && !screenreader) {
			send_to_char("\r\n", ch);
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_spawns) {
	bool any = FALSE;
	struct spawn_info *sp;
	vehicle_data *veh, *next_veh;
	sector_data *sect, *next_sect;
	crop_data *crop, *next_crop;
	bld_data *bld, *next_bld;
	char_data *mob;
	room_template *rmt, *next_rmt;
	struct adventure_spawn *asp;
	struct global_data *glb, *next_glb;
	mob_vnum vnum;
	
	if (!*argument || !isdigit(*argument) || !(mob = mob_proto((vnum = atoi(argument))))) {
		msg_to_char(ch, "Show spawns: You must specify a valid mob vnum.\r\n");
		return;
	}

	// sectors:
	HASH_ITER(hh, sector_table, sect, next_sect) {
		for (sp = GET_SECT_SPAWNS(sect); sp; sp = sp->next) {
			if (sp->vnum == vnum) {
				sprintbit(sp->flags, spawn_flags, buf2, TRUE);
				add_page_display(ch, "SCT [%5d] %s: %.2f%% %s", GET_SECT_VNUM(sect), GET_SECT_NAME(sect), sp->percent, buf2);
				any = TRUE;
			}
		}
	}
	
	// crops:
	HASH_ITER(hh, crop_table, crop, next_crop) {
		for (sp = GET_CROP_SPAWNS(crop); sp; sp = sp->next) {
			if (sp->vnum == vnum) {
				sprintbit(sp->flags, spawn_flags, buf2, TRUE);
				add_page_display(ch, "CRP [%5d] &Z%s: %.2f%% %s", GET_CROP_VNUM(crop), GET_CROP_NAME(crop), sp->percent, buf2);
				any = TRUE;
			}
		}
	}

	// buildings:
	HASH_ITER(hh, building_table, bld, next_bld) {
		for (sp = GET_BLD_SPAWNS(bld); sp; sp = sp->next) {
			if (sp->vnum == vnum) {
				sprintbit(sp->flags, spawn_flags, buf2, TRUE);
				add_page_display(ch, "BLD [%5d] %s: %.2f%% %s", GET_BLD_VNUM(bld), GET_BLD_NAME(bld), sp->percent, buf2);
				any = TRUE;
			}
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		LL_FOREACH(VEH_SPAWNS(veh), sp) {
			if (sp->vnum == vnum) {
				sprintbit(sp->flags, spawn_flags, buf2, TRUE);
				add_page_display(ch, "VEH [%5d] %s: %.2f%% %s", VEH_VNUM(veh), VEH_SHORT_DESC(veh), sp->percent, buf2);
				any = TRUE;
			}
		}
	}
	
	// globals
	HASH_ITER(hh, globals_table, glb, next_glb) {
		LL_FOREACH(GET_GLOBAL_SPAWNS(glb), sp) {
			if (sp->vnum == vnum) {
				sprintbit(sp->flags, spawn_flags, buf2, TRUE);
				add_page_display(ch, "GLB [%5d] %s: %.2f%% %s", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb), sp->percent, buf2);
				any = TRUE;
			}
		}
	}
	
	// room templates
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		for (asp = GET_RMT_SPAWNS(rmt); asp; asp = asp->next) {
			if (asp->type == ADV_SPAWN_MOB && asp->vnum == vnum) {
				add_page_display(ch, "RMT [%5d] %s: %.2f%%, limit %d\r\n", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt), asp->percent, asp->limit);
				any = TRUE;
				break;
			}
		}
	}
	
	if (any) {
		send_page_display(ch);
	}
	else {
		msg_to_char(ch, "No spawns found for mob %d.\r\n", vnum);
	}
}


SHOW(show_startlocs) {
	room_data *iter, *next_iter;
	
	add_page_display_str(ch, "Starting locations:");
	
	HASH_ITER(hh, world_table, iter, next_iter) {
		if (ROOM_SECT_FLAGGED(iter, SECTF_START_LOCATION)) {
			add_page_display(ch, "%s (%d, %d)&0", get_room_name(iter, TRUE), X_COORD(iter), Y_COORD(iter));
		}
	}
	
	send_page_display(ch);
}


SHOW(show_stats) {
	int num_active_empires = 0, num_objs = 0, num_mobs = 0, num_vehs = 0, num_players = 0, num_descs = 0, menu_count = 0;
	int num_trigs = 0, num_goals = 0, num_rewards = 0, num_mort_helps = 0, num_imm_helps = 0, num_inst = 0;
	progress_data *prg, *next_prg;
	empire_data *emp, *next_emp;
	struct instance_data *inst;
	descriptor_data *desc;
	vehicle_data *veh;
	char_data *vict;
	trig_data *trig;
	obj_data *obj;
	int iter;
	
	// count descriptors at menus
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (STATE(desc) != CON_PLAYING) {
			++menu_count;
		}
	}
	
	// count connections, players, mobs
	DL_FOREACH(character_list, vict) {
		if (IS_NPC(vict)) {
			++num_mobs;
		}
		else if (CAN_SEE(ch, vict)) {
			++num_players;
			if (vict->desc) {
				++num_descs;
			}
		}
	}
	
	// other counts
	DL_COUNT(object_list, obj, num_objs);
	DL_COUNT(vehicle_list, veh, num_vehs);
	DL_COUNT2(trigger_list, trig, num_trigs, next_in_world);
	DL_COUNT(instance_list, inst, num_inst);

	// count active empires
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_MEMBERS(emp) > 0) {
			++num_active_empires;
		}
	}
	
	// count goals
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT)) {
			continue;
		}
		
		if (PRG_FLAGGED(prg, PRG_PURCHASABLE)) {
			++num_rewards;
		}
		else {
			++num_goals;
		}
	}
	
	// count helps
	for (iter = 0; iter <= top_of_helpt; ++iter) {
		if (help_table[iter].duplicate) {
			continue;
		}
		
		if (help_table[iter].level > LVL_MORTAL) {
			++num_imm_helps;
		}
		else {
			++num_mort_helps;
		}
	}
	
	update_account_stats();

	msg_to_char(ch, "Current stats:\r\n");
	msg_to_char(ch, "  %6d players in game  %6d connected\r\n", num_players, num_descs);
	msg_to_char(ch, "  %6d registered       %6d at menus\r\n", HASH_CNT(idnum_hh, player_table_by_idnum), menu_count);
	msg_to_char(ch, "  %6d player accounts  %6d active accounts\r\n", total_accounts, active_accounts);
	msg_to_char(ch, "  %6d accounts logged in this week\r\n", active_accounts_week);
	msg_to_char(ch, "  %6d empires          %6d active\r\n", HASH_COUNT(empire_table), num_active_empires);
	msg_to_char(ch, "  %6d mobiles          %6d prototypes\r\n", num_mobs, HASH_COUNT(mobile_table));
	msg_to_char(ch, "  %6d objects          %6d prototypes\r\n", num_objs, HASH_COUNT(object_table));
	msg_to_char(ch, "  %6d vehicles         %6d prototypes\r\n", num_vehs, HASH_COUNT(vehicle_table));
	msg_to_char(ch, "  %6d adventures       %6d total rooms\r\n", HASH_COUNT(adventure_table), HASH_COUNT(world_table));
	msg_to_char(ch, "  %6d buildings        %6d room templates\r\n", HASH_COUNT(building_table), HASH_COUNT(room_template_table));
	msg_to_char(ch, "  %6d sectors          %6d crops\r\n", HASH_COUNT(sector_table), HASH_COUNT(crop_table));
	msg_to_char(ch, "  %6d triggers         %6d prototypes\r\n", num_trigs, HASH_COUNT(trigger_table));
	msg_to_char(ch, "  %6d craft recipes    %6d quests\r\n", HASH_COUNT(craft_table), HASH_COUNT(quest_table));
	msg_to_char(ch, "  %6d archetypes       %6d books\r\n", HASH_COUNT(archetype_table), HASH_COUNT(book_table));
	msg_to_char(ch, "  %6d classes          %6d skills\r\n", HASH_COUNT(class_table), HASH_COUNT(skill_table));
	msg_to_char(ch, "  %6d abilities        %6d factions\r\n", HASH_COUNT(ability_table), HASH_COUNT(faction_table));
	msg_to_char(ch, "  %6d globals          %6d morphs\r\n", HASH_COUNT(globals_table), HASH_COUNT(morph_table));
	msg_to_char(ch, "  %6d events           %6d adventure instances\r\n", HASH_COUNT(event_table), num_inst);
	msg_to_char(ch, "  %6d socials          %6d generics\r\n", HASH_COUNT(social_table), HASH_COUNT(generic_table));
	msg_to_char(ch, "  %6d progress goals   %6d progress rewards\r\n", num_goals, num_rewards);
	msg_to_char(ch, "  %6d shops\r\n", HASH_COUNT(shop_table));
	msg_to_char(ch, "  %6d mortal helpfiles %6d immortal helpfiles\r\n", num_mort_helps, num_imm_helps);
	msg_to_char(ch, "  %6d large bufs       %6d buf switches\r\n", buf_largecount, buf_switches);
	msg_to_char(ch, "  %6d overflows\r\n", buf_overflows);
}


// show storage <building | vehicle> <vnum>
SHOW(show_storage) {
	char arg2[MAX_STRING_LENGTH];
	struct obj_storage_type *store;
	vehicle_data *find_veh = NULL;
	bld_data *find_bld = NULL;
	obj_data *obj, *next_obj;
	int count;
	bool ok;
	
	two_arguments(argument, arg, arg2);
	
	if (!*arg || !*arg2 || !is_number(arg2)) {
		msg_to_char(ch, "Usage: show storage <building | vehicle> <vnum>\r\n");
	}
	else if (is_abbrev(arg, "building") && !(find_bld = building_proto(atoi(arg2)))) {
		msg_to_char(ch, "Show storage: Unknown building '%s'.\r\n", arg2);
	}
	else if (is_abbrev(arg, "vehicle") && !(find_veh = vehicle_proto(atoi(arg2)))) {
		msg_to_char(ch, "Show storage: Unknown vehicle '%s'.\r\n", arg2);
	}
	else if (!find_bld && !find_veh) {
		msg_to_char(ch, "Usage: show storage <building | vehicle> <vnum>\r\n");
	}
	else {
		// ok to show:
		if (find_bld) {
			add_page_display(ch, "Objects that can be stored in %s %s:", AN(GET_BLD_NAME(find_bld)), GET_BLD_NAME(find_bld));
		}
		else if (find_veh) {
			add_page_display(ch, "Objects that can be stored in %s:", VEH_SHORT_DESC(find_veh));
		}
		else {
			add_page_display_str(ch, "Objects that can be stored there:");
		}
		
		count = 0;
		HASH_ITER(hh, object_table, obj, next_obj) {
			ok = FALSE;
			
			// check storage
			LL_FOREACH(GET_OBJ_STORAGE(obj), store) {
				if (find_bld && store->type == TYPE_BLD && store->vnum == GET_BLD_VNUM(find_bld)) {
					ok = TRUE;
					break;
				}
				else if (find_veh && store->type == TYPE_VEH && store->vnum == VEH_VNUM(find_veh)) {
					ok = TRUE;
					break;
				}
			}
		
			if (ok) {
				++count;
				add_page_display(ch, "[%5d] %s", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
			}
		}
		
		if (count == 0) {
			add_page_display_str(ch, " none");
		}
		
		send_page_display(ch);
	}
}


SHOW(show_subzone) {
	bool any;
	char buf2[MAX_STRING_LENGTH];
	room_template *iter, *next_iter, *match;
	rmt_vnum find;
	
	if (!*argument || !isdigit(*argument)) {
		msg_to_char(ch, "Usage: show subzone <id/room>\r\n");
		return;
	}
	if ((find = atoi(argument)) < 0) {
		msg_to_char(ch, "Show subzone: Subzone id must be positive.\r\n");
		return;
	}
	
	// first try: exact match
	any = FALSE;
	HASH_ITER(hh, room_template_table, iter, next_iter) {
		if (GET_RMT_SUBZONE(iter) == find) {
			add_page_display(ch, "[%5d] %s", GET_RMT_VNUM(iter), GET_RMT_TITLE(iter));
			any = TRUE;
		}
	}
	if (any) {
		// found exact match(es)
		snprintf(buf2, sizeof(buf2), "Room templates with subzone %d:", find);
		prepend_page_display_str(ch, buf2);
		send_page_display(ch);
		return;
	}
	
	// no matches: try again with the rmt specfied
	if (!(match = room_template_proto(find)) || GET_RMT_SUBZONE(match) == NOWHERE) {
		msg_to_char(ch, "There were no matches for that subzone id.\r\n");
		return;
	}
	
	// if we get here, we have a subzone to look for
	any = FALSE;
	HASH_ITER(hh, room_template_table, iter, next_iter) {
		if (GET_RMT_SUBZONE(iter) == GET_RMT_SUBZONE(match)) {
			add_page_display(ch, "[%5d] %s", GET_RMT_VNUM(iter), GET_RMT_TITLE(iter));
			any = TRUE;
		}
	}
	if (any) {
		// found exact match(es)
		snprintf(buf2, sizeof(buf2), "Room templates with subzone %d, matching room template %d:", GET_RMT_SUBZONE(match), GET_RMT_VNUM(match));
		prepend_page_display_str(ch, buf2);
		send_page_display(ch);
		return;
	}
	
	// otherwise
	msg_to_char(ch, "There were no matches for that subzone id.\r\n");
}


SHOW(show_technology) {
	struct player_tech *ptech;
	int last_tech, count = 0;
	char one[256], line[256];
	char_data *vict = NULL;
	bool is_file = FALSE;
	size_t lsize;
	
	if (!*argument) {
		msg_to_char(ch, "Show technology: For which player?\r\n");
	}
	else if (!(vict = find_or_load_player(argument, &is_file))) {
		msg_to_char(ch, "Show technology: No player by that name.\r\n");
	}
	else {
		// techs (slightly complicated
		msg_to_char(ch, "Techs for %s:\r\n", GET_NAME(vict));
		
		last_tech = NOTHING;
		*line = '\0';
		lsize = 0;
		
		LL_FOREACH(GET_TECHS(vict), ptech) {
			if (ptech->id == last_tech) {
				continue;
			}
			
			snprintf(one, sizeof(one), "\t%c%s%s, ", (++count % 2) ? 'W' : 'w', player_tech_types[ptech->id], ptech->check_solo ? " (synergy)" : "");
			
			if (color_strlen(one) + lsize >= 79) {
				// send line
				msg_to_char(ch, "%s\r\n", line);
				lsize = 0;
				*line = '\0';
			}
			
			strcat(line, one);
			lsize += color_strlen(one);
			last_tech = ptech->id;
		}
		
		if (*line) {
			msg_to_char(ch, "%s\r\n", line);
		}
		msg_to_char(ch, "\t0");
	}
	
	if (vict && is_file) {
		free_char(vict);
	}
}


SHOW(show_terrain) {
	char part[256];
	sector_data *sect, *next_sect;
	int count, total, this;
	struct map_data *map;
	bool any;
	
	// fresh numbers
	update_world_count();
	
	if (!*argument) {	// no-arg: show summary
		// output
		total = count = 0;
	
		HASH_ITER(hh, sector_table, sect, next_sect) {
			this = stats_get_sector_count(sect);
			add_page_display_col(ch, 2, FALSE, " %6d %s", this, GET_SECT_NAME(sect));
			total += this;
		}
	
		add_page_display(ch, " Total: %d", total);
		send_page_display(ch);
	}
	// argument usage: show building <vnum | name>
	else if (!(isdigit(*argument) && (sect = sector_proto(atoi(argument)))) && !(sect = get_sect_by_name(argument))) {
		msg_to_char(ch, "Show sectors: Unknown sector '%s'.\r\n", argument);
	}
	else {
		strcpy(part, GET_SECT_NAME(sect));
		add_page_display(ch, "[%d] %s (%d in world):", GET_SECT_VNUM(sect), CAP(part), stats_get_sector_count(sect));
		
		any = FALSE;
		LL_FOREACH(land_map, map) {
			if (map->sector_type != sect) {
				continue;
			}
			
			// found
			if (map->room && ROOM_OWNER(map->room)) {
				snprintf(part, sizeof(part), " - %s%s\t0", EMPIRE_BANNER(ROOM_OWNER(map->room)), EMPIRE_ADJECTIVE(ROOM_OWNER(map->room)));
			}
			else {
				*part = '\0';
			}
			add_page_display(ch, "(%*d, %*d) %s%s", X_PRECISION, MAP_X_COORD(map->vnum), Y_PRECISION, MAP_Y_COORD(map->vnum), map->room ? get_room_name(map->room, FALSE) : GET_SECT_TITLE(sect), part);
			any = TRUE;
		}
		
		if (!any) {
			add_page_display_str(ch, " no matching tiles");
		}
		
		send_page_display(ch);
	}
}


SHOW(show_tomb) {
	char name[MAX_INPUT_LENGTH];
	bool file = FALSE;
	char_data *vict;
	room_data *tomb;
	
	any_one_arg(argument, name);
	
	if (!(vict = find_or_load_player(name, &file))) {
		msg_to_char(ch, "Show tomb: No player by that name.\r\n");
	}
	else {
		tomb = real_room(GET_TOMB_ROOM(vict));
		
		if (!tomb) {
			msg_to_char(ch, "%s has no tomb set.\r\n", PERS(vict, ch, TRUE));
		}
		else {
			msg_to_char(ch, "%s's tomb is at: %s%s\r\n", PERS(vict, ch, TRUE), get_room_name(tomb, FALSE), coord_display_room(ch, tomb, FALSE));
		}
	}
	
	if (file) {
		free_char(vict);
	}
}


SHOW(show_tools) {
	obj_data *obj, *next_obj;
	int type;
	
	skip_spaces(&argument);
	
	if (!*argument) {
		msg_to_char(ch, "Usage: show tools <type>\r\n");
		msg_to_char(ch, "See: HELP TOOL FLAGS\r\n");
	}
	else if ((type = search_block(argument, tool_flags, FALSE)) == NOTHING) {
		msg_to_char(ch, "Show tools: Unknown tool type '%s' (see HELP TOOL FLAGS).\r\n", argument);
	}
	else {
		// preamble
		add_page_display(ch, "Types of %s:", tool_flags[type]);
		
		HASH_ITER(hh, object_table, obj, next_obj) {
			if (!TOOL_FLAGGED(obj, BIT(type))) {
				continue;	// wrong type
			}
			
			add_page_display(ch, "[%5d] %s", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
		}
		
		send_page_display(ch);
	}
}


SHOW(show_unlearnable) {
	struct progress_perk *perk, *next_perk;
	craft_data *craft, *next_craft;
	progress_data *prg, *next_prg;
	obj_data *obj, *next_obj;
	bool any = FALSE, found;
	
	msg_to_char(ch, "Unlearnable recipes with the LEARNED flag:\r\n");
	
	HASH_ITER(hh, craft_table, craft, next_craft) {
		if (!CRAFT_FLAGGED(craft, CRAFT_LEARNED)) {
			continue;
		}
		
		found = FALSE;
		
		// try to find it in recipes
		HASH_ITER(hh, object_table, obj, next_obj) {
			if (IS_RECIPE(obj) && GET_RECIPE_VNUM(obj) == GET_CRAFT_VNUM(craft)) {
				found = TRUE;
				break;
			}
		}
		if (found) {	// was an item recipe
			continue;
		}
		
		// try to find it in progression
		HASH_ITER(hh, progress_table, prg, next_prg) {
			LL_FOREACH_SAFE(PRG_PERKS(prg), perk, next_perk) {
				if (perk->type == PRG_PERK_CRAFT && perk->value == GET_CRAFT_VNUM(craft)) {
					found = TRUE;
					break;
				}
			}
			if (found) {
				break;
			}
		}
		if (found) {	// was a progression reward
			continue;
		}
		
		// did we get this far?
		any = TRUE;
		msg_to_char(ch, "[%5d] %s (%s)\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft), GET_CRAFT_ABILITY(craft) == NO_ABIL ? "no ability" : get_ability_name_by_vnum(GET_CRAFT_ABILITY(craft)));
	}
	
	if (any) {
		msg_to_char(ch, "(remember, some of these may be added by scripts)\r\n");
	}
	else {
		msg_to_char(ch, " none\r\n");
	}
}


SHOW(show_unlocked_archetypes) {
	archetype_data *arch;
	struct unlocked_archetype *unarch, *next_unarch;
	char_data *plr = NULL;
	size_t count;
	bool file = FALSE;
	
	argument = one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg) {
		msg_to_char(ch, "Usage: show unlockedarchetypes <player> [keywords]\r\n");
	}
	else if (!(plr = find_or_load_player(arg, &file))) {
		send_to_char("Show unlockedarchetypes: There is no such player.\r\n", ch);
	}
	else {
		if (*argument) {
			add_page_display(ch, "Unlocked archetypes matching '%s' for %s:", argument, GET_NAME(plr));
		}
		else {
			add_page_display(ch, "Unlocked archetypes for %s:", GET_NAME(plr));
		}
		
		count = 0;
		HASH_ITER(hh, ACCOUNT_UNLOCKED_ARCHETYPES(plr), unarch, next_unarch) {
			if (!(arch = archetype_proto(unarch->vnum))) {
				continue;	// no archetype?
			}
			if (*argument && !multi_isname(argument, GET_ARCH_NAME(arch))) {
				continue;	// searched
			}
		
			// show it
			add_page_display(ch, " [%5d] %s", GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch));
			++count;
		}
	
		if (!count) {
			add_page_display_str(ch, "  none");
		}
	
		send_page_display(ch);
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_uses) {
	char part[MAX_STRING_LENGTH];
	craft_data *craft, *next_craft;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	augment_data *aug, *next_aug;
	vehicle_data *veh, *next_veh;
	social_data *soc, *next_soc;
	struct resource_data *res;
	bld_data *bld, *next_bld;
	struct req_data *req, *req_list[2] = { NULL, NULL };
	generic_data *cmp;
	int iter;
		
	skip_spaces(&argument);	// optional flags
	
	if (!*argument) {
		msg_to_char(ch, "Usage: show uses <name | vnum>\r\n");
	}
	else if (!(cmp = find_generic_component(argument))) {
		msg_to_char(ch, "Show uses: Unknown component type '%s'.\r\n", argument);
	}
	else {
		// preamble
		add_page_display(ch, "Uses for [%d] (%s):", GEN_VNUM(cmp), GEN_NAME(cmp));
		
		HASH_ITER(hh, augment_table, aug, next_aug) {
			LL_FOREACH(GET_AUG_RESOURCES(aug), res) {
				if (res->type != RES_COMPONENT) {
					continue;
				}
				if (res->vnum != GEN_VNUM(cmp) && !has_generic_relation(GEN_COMPUTED_RELATIONS(cmp), res->vnum)) {
					continue;
				}
				
				// success
				if (res->vnum == GEN_VNUM(cmp)) {
					*part = '\0';
				}
				else {
					snprintf(part, sizeof(part), " (%s)", get_generic_name_by_vnum(res->vnum));
				}
				add_page_display(ch, "AUG [%5d] %s%s", GET_AUG_VNUM(aug), GET_AUG_NAME(aug), part);
			}
		}
		
		HASH_ITER(hh, building_table, bld, next_bld) {
			LL_FOREACH(GET_BLD_REGULAR_MAINTENANCE(bld), res) {
				if (res->type != RES_COMPONENT) {
					continue;
				}
				if (res->vnum != GEN_VNUM(cmp) && !has_generic_relation(GEN_COMPUTED_RELATIONS(cmp), res->vnum)) {
					continue;
				}
				
				// success
				if (res->vnum == GEN_VNUM(cmp)) {
					*part = '\0';
				}
				else {
					snprintf(part, sizeof(part), " (%s)", get_generic_name_by_vnum(res->vnum));
				}
				add_page_display(ch, "BLD [%5d] %s%s", GET_BLD_VNUM(bld), GET_BLD_NAME(bld), part);
			}
		}
		
		HASH_ITER(hh, craft_table, craft, next_craft) {
			LL_FOREACH(GET_CRAFT_RESOURCES(craft), res) {
				if (res->type != RES_COMPONENT) {
					continue;
				}
				if (res->vnum != GEN_VNUM(cmp) && !has_generic_relation(GEN_COMPUTED_RELATIONS(cmp), res->vnum)) {
					continue;
				}
				
				// success
				if (res->vnum == GEN_VNUM(cmp)) {
					*part = '\0';
				}
				else {
					snprintf(part, sizeof(part), " (%s)", get_generic_name_by_vnum(res->vnum));
				}
				add_page_display(ch, "CFT [%5d] %s%s", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft), part);
			}
		}
		
		HASH_ITER(hh, progress_table, prg, next_prg) {
			LL_FOREACH(PRG_TASKS(prg), req) {
				if (req->type != REQ_GET_COMPONENT && req->type != REQ_EMPIRE_PRODUCED_COMPONENT) {
					continue;	// wrong type
				}
				if (req->vnum != GEN_VNUM(cmp) && !has_generic_relation(GEN_COMPUTED_RELATIONS(cmp), req->vnum)) {
					continue;
				}
				
				// success
				if (req->vnum == GEN_VNUM(cmp)) {
					*part = '\0';
				}
				else {
					snprintf(part, sizeof(part), " (%s)", get_generic_name_by_vnum(req->vnum));
				}
				add_page_display(ch, "PRG [%5d] %s%s", PRG_VNUM(prg), PRG_NAME(prg), part);
			}
		}
		
		HASH_ITER(hh, quest_table, quest, next_quest) {
			req_list[0] = QUEST_TASKS(quest);
			req_list[1] = QUEST_PREREQS(quest);
			
			for (iter = 0; iter < 2; ++iter) {
				LL_FOREACH(req_list[iter], req) {
					if (req->type != REQ_GET_COMPONENT && req->type != REQ_EMPIRE_PRODUCED_COMPONENT) {
						continue;	// wrong type
					}
					if (req->vnum != GEN_VNUM(cmp) && !has_generic_relation(GEN_COMPUTED_RELATIONS(cmp), req->vnum)) {
						continue;
					}
				
					// success
					if (req->vnum == GEN_VNUM(cmp)) {
						*part = '\0';
					}
					else {
						snprintf(part, sizeof(part), " (%s)", get_generic_name_by_vnum(req->vnum));
					}
					add_page_display(ch, "QST [%5d] %s", QUEST_VNUM(quest), QUEST_NAME(quest));
				}
			}
		}
		
		HASH_ITER(hh, social_table, soc, next_soc) {
			LL_FOREACH(SOC_REQUIREMENTS(soc), req) {
				if (req->type != REQ_GET_COMPONENT && req->type != REQ_EMPIRE_PRODUCED_COMPONENT) {
					continue;	// wrong type
				}
				if (req->vnum != GEN_VNUM(cmp) && !has_generic_relation(GEN_COMPUTED_RELATIONS(cmp), req->vnum)) {
					continue;
				}
				
				// success
				if (req->vnum == GEN_VNUM(cmp)) {
					*part = '\0';
				}
				else {
					snprintf(part, sizeof(part), " (%s)", get_generic_name_by_vnum(req->vnum));
				}
				add_page_display(ch, "SOC [%5d] %s", SOC_VNUM(soc), SOC_NAME(soc));
			}
		}
		
		HASH_ITER(hh, vehicle_table, veh, next_veh) {
			LL_FOREACH(VEH_REGULAR_MAINTENANCE(veh), res) {
				if (res->type != RES_COMPONENT) {
					continue;
				}
				if (res->vnum != GEN_VNUM(cmp) && !has_generic_relation(GEN_COMPUTED_RELATIONS(cmp), res->vnum)) {
					continue;
				}
				
				// success
				if (res->vnum == GEN_VNUM(cmp)) {
					*part = '\0';
				}
				else {
					snprintf(part, sizeof(part), " (%s)", get_generic_name_by_vnum(res->vnum));
				}
				add_page_display(ch, "VEH [%5d] %s%s", VEH_VNUM(veh), VEH_SHORT_DESC(veh), part);
			}
		}
		
		send_page_display(ch);
	}
}


SHOW(show_variables) {
	char uname[MAX_INPUT_LENGTH];
	struct trig_var_data *tv;
	char_data *plr = NULL;
	bool file = FALSE;
	
	if (!*argument) {
		msg_to_char(ch, "Usage: show variables <player>\r\n");
	}
	else if (!(plr = find_or_load_player(argument, &file))) {
		send_to_char("Show variables: There is no such player.\r\n", ch);
	}
	else {
		msg_to_char(ch, "Global Variables:\r\n");
		check_delayed_load(plr);
		
		if (plr->script && plr->script->global_vars) {
			/* currently, variable context for players is always 0, so it is */
			/* not displayed here. in the future, this might change */
			for (tv = plr->script->global_vars; tv; tv = tv->next) {
				if (*(tv->value) == UID_CHAR) {
					find_uid_name(tv->value, uname, sizeof(uname));
					msg_to_char(ch, " %10s:  [UID]: %s\r\n", tv->name, uname);
				}
				else {
					msg_to_char(ch, " %10s:  %s\r\n", tv->name, tv->value);
				}
			}
		}
	}
	
	if (plr && file) {
		free_char(plr);
	}
}


SHOW(show_workforce) {
	empire_data *emp;
	
	one_word(argument, arg);
	if (!*arg) {
		msg_to_char(ch, "Show workforce: For what empire?\r\n");
	}
	else if (!(emp = get_empire_by_name(arg))) {
		msg_to_char(ch, "Show workforce: Unknown empire '%s'.\r\n", arg);
	}
	else {
		show_workforce_setup_to_char(emp, ch);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMAND DATA ////////////////////////////////////////////////////////////

struct show_struct {
	const char *cmd;
	const char level;
	SHOW(*func);
} show_command_fields[] = {
	{ "nothing",		0,		NULL },		// this is skipped
	
	// privileged options
	{ "account",		LVL_TO_SEE_ACCOUNTS,	show_account },
	{ "data",			LVL_CIMPL,				show_data },
	{ "piles",			LVL_CIMPL,				show_piles },
	{ "stats",			LVL_GOD,			show_stats },
	
	// basic options
	{ "ammotypes",		LVL_START_IMM,		show_ammotypes },
	{ "author",			LVL_START_IMM,		show_author },
	{ "buildings",		LVL_START_IMM,		show_buildings },
	{ "commons",		LVL_START_IMM,		show_commons },
	{ "companions",		LVL_START_IMM,		show_companions },
	{ "components",		LVL_START_IMM,		show_components },
	{ "crops",			LVL_START_IMM,		show_crops },
	{ "currency",		LVL_START_IMM,		show_currency },
	{ "dailycycle",		LVL_START_IMM,		show_dailycycle },
	{ "dropped",		LVL_START_IMM,		show_dropped_items },
	{ "editors",		LVL_START_IMM,		show_editors },
	{ "factions",		LVL_START_IMM,		show_factions },
	{ "fmessages",		LVL_START_IMM,		show_fmessages },
	{ "friends",		LVL_START_IMM,		show_friends },
	{ "home",			LVL_START_IMM,		show_home },
	{ "homeless",		LVL_START_IMM,		show_homeless },
	{ "ignoring",		LVL_START_IMM,		show_ignoring },
	{ "inventory",		LVL_START_IMM,		show_inventory },
	{ "islands",		LVL_START_IMM,		show_islands },
	{ "languages",		LVL_START_IMM,		show_languages },
	{ "lastnames",		LVL_START_IMM,		show_lastnames },
	{ "learned",		LVL_START_IMM,		show_learned },
	{ "libraries",		LVL_START_IMM,		show_libraries },
	{ "lostbooks",		LVL_START_IMM,		show_lost_books },
	{ "minipets",		LVL_START_IMM,		show_minipets },
	{ "moons",			LVL_START_IMM,		show_moons },
	{ "mounts",			LVL_START_IMM,		show_mounts },
	{ "notes",			LVL_START_IMM,		show_notes },
	{ "oceanmobs",		LVL_START_IMM,		show_oceanmobs },
	{ "olc",			LVL_START_IMM,		show_olc },
	{ "player",			LVL_START_IMM,		show_player },
	{ "players",		LVL_START_IMM,		show_players },
	{ "produced",		LVL_START_IMM,		show_produced },
	{ "progress",		LVL_START_IMM,		show_progress },
	{ "progression",	LVL_START_IMM,		show_progression },
	{ "quests",			LVL_START_IMM,		show_quests },
	{ "rent",			LVL_START_IMM,		show_rent },
	{ "resource",		LVL_START_IMM,		show_resource },
	{ "sectors",		LVL_START_IMM,		show_terrain },
	{ "shops",			LVL_START_IMM,		show_shops },
	{ "site",			LVL_START_IMM,		show_site },
	{ "skills",			LVL_START_IMM,		show_skills },
	{ "smessages",		LVL_START_IMM,		show_smessages },
	{ "spawns",			LVL_START_IMM,		show_spawns },
	{ "startlocs",		LVL_START_IMM,		show_startlocs },
	{ "storage",		LVL_START_IMM,		show_storage },
	{ "subzone",		LVL_START_IMM,		show_subzone },
	{ "technology",		LVL_START_IMM,		show_technology },
	{ "terrain",		LVL_START_IMM,		show_terrain },
	{ "tomb",			LVL_START_IMM,		show_tomb },
	{ "tools",			LVL_START_IMM,		show_tools },
	{ "unlearnable",	LVL_START_IMM,		show_unlearnable },
	{ "unlockedarchetypes",	LVL_START_IMM,	show_unlocked_archetypes },
	{ "uses",			LVL_START_IMM,		show_uses },
	{ "variables",		LVL_START_IMM,		show_variables },
	{ "workforce",		LVL_START_IMM,		show_workforce },
	
	// last
	{ "\n",		0,		NULL }
};


 //////////////////////////////////////////////////////////////////////////////
//// SHOW COMMAND ////////////////////////////////////////////////////////////

ACMD(do_show) {
	char field[MAX_INPUT_LENGTH], value[MAX_INPUT_LENGTH];
	int count, pos, iter;
	
	// don't bother -- nobody to show it to
	if (!ch->desc) {
		return;
	}
	
	skip_spaces(&argument);
	
	// no-arg
	if (!*argument) {
		strcpy(buf, "Show options:\r\n");
		for (count = 0, iter = 1; show_command_fields[iter].level; ++iter) {
			if (show_command_fields[iter].level <= GET_ACCESS_LEVEL(ch)) {
				sprintf(buf + strlen(buf), " %-18.18s%s", show_command_fields[iter].cmd, ((!(++count % 4) || PRF_FLAGGED(ch, PRF_SCREEN_READER)) ? "\r\n" : ""));
			}
		}
		if (!PRF_FLAGGED(ch, PRF_SCREEN_READER) && (count % 4) != 0) {
			strcat(buf, "\r\n");
		}
		send_to_char(buf, ch);
		return;
	}

	// find arg
	half_chop(argument, field, value);
	pos = NOTHING;
	for (iter = 0; *(show_command_fields[iter].cmd) != '\n'; ++iter) {
		if (is_abbrev(field, show_command_fields[iter].cmd)) {
			pos = iter;
			break;
		}
	}
	
	if (pos == NOTHING) {
		msg_to_char(ch, "Invalid option.\r\n");
	}
	else if (GET_ACCESS_LEVEL(ch) < show_command_fields[pos].level) {
		send_to_char("You are not godly enough for that!\r\n", ch);
	}
	else if (!show_command_fields[pos].func) {
		msg_to_char(ch, "Show %s: That function is not implemented yet.\r\n", show_command_fields[iter].cmd);
	}
	else {
		// SUCCESS -- pass to next function
		(show_command_fields[pos].func)(ch, value);
	}
}
