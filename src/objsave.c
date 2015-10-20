/* ************************************************************************
*   File: objsave.c                                       EmpireMUD 2.0b3 *
*  Usage: loading/saving player objects for rent and crash-save           *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */
/**
* EmpireMUD is using a modified version of AutoEQ, which re-equips a player
* on login rather than dropping everything to inventory.
*
* AutoEQ by Burkhard Knopf <burkhard.knopf@informatik.tu-clausthal.de> 
*/

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "interpreter.h"
#include "utils.h"
#include "dg_scripts.h"

/**
* Contents:
*   Basic Object Loading
*   Basic Object Saving
*   Player Object Loading
*   Player Object Saving
*   World Object Saving
*   Object Utils
*/

#define LOC_INVENTORY	0
#define MAX_BAG_ROWS	5


// externs
extern const struct wear_data_type wear_data[NUM_WEARS];


 //////////////////////////////////////////////////////////////////////////////
//// BASIC OBJECT LOADING ////////////////////////////////////////////////////

/**
* This makes sure we never load an object with too little data to exist
* somehow.
*
* @param obj_data *obj The object to check.
*/
void ensure_safe_obj(obj_data *obj) {
	if (!GET_OBJ_KEYWORDS(obj) || !*GET_OBJ_KEYWORDS(obj)) {
		if (GET_OBJ_KEYWORDS(obj)) {
			free(GET_OBJ_KEYWORDS(obj));
		}
		GET_OBJ_KEYWORDS(obj) = str_dup("object item unknown");
	}
	if (!GET_OBJ_SHORT_DESC(obj) || !*GET_OBJ_SHORT_DESC(obj)) {
		if (GET_OBJ_SHORT_DESC(obj)) {
			free(GET_OBJ_SHORT_DESC(obj));
		}
		GET_OBJ_SHORT_DESC(obj) = str_dup("an unknown object");
	}
	if (!GET_OBJ_LONG_DESC(obj) || !*GET_OBJ_LONG_DESC(obj)) {
		if (GET_OBJ_LONG_DESC(obj)) {
			free(GET_OBJ_LONG_DESC(obj));
		}
		GET_OBJ_LONG_DESC(obj) = str_dup("An unknown object is here.");
	}
}


/**
* formerly obj_from_store
*
* @param FILE *fl The open item file.
* @param obj_vnum vnum The vnum of the item being loaded, or NOTHING for non-prototyped item.
* @param int *location A place to bind the current WEAR_x position of the item; also used to track container contents.
* @param char_data *notify Optional: A person to notify if an item is updated (NULL for none).
* @return obj_data* The loaded item, or NULL if it's not available.
*/
obj_data *Obj_load_from_file(FILE *fl, obj_vnum vnum, int *location, char_data *notify) {
	void scale_item_to_level(obj_data *obj, int level);

	char line[MAX_INPUT_LENGTH], error[MAX_STRING_LENGTH], s_in[MAX_INPUT_LENGTH];
	obj_data *proto = obj_proto(vnum);
	struct extra_descr_data *ex;
	obj_data *obj, *new;
	bool end = FALSE;
	int length, i_in[3];
	int l_in;
	bool seek_end = FALSE;
	
	// up-front
	*location = 0;
	
	// load based on vnum or, if NOTHING, create anonymous object
	if (proto) {
		obj = read_object(vnum, FALSE);
	}
	else {
		// what we do here depends on input ... if the vnum was real, but no proto, it's a deleted obj
		if (vnum == NOTHING) {
			obj = create_obj();
		}
		else {
			obj = NULL;
			seek_end = TRUE;	// signal it to skip obj data
		}
	}
	
	// default to version 0
	if (obj) {
		OBJ_VERSION(obj) = 0;
	}
	
	// for fread_string
	sprintf(error, "Obj_load_from_file %d", vnum);
	
	// for more readable if/else chain	
	#define OBJ_FILE_TAG(src, tag, len)  (!strn_cmp((src), (tag), ((len) = strlen(tag))))

	while (!end) {
		if (!get_line(fl, line)) {
			log("SYSERR: Unexpected end of obj file in Obj_load_from_file");
			exit(1);
		}
		
		if (OBJ_FILE_TAG(line, "End", length)) {
			end = TRUE;
		}
		else if (seek_end) {
			// are we looking for the end of the object? ignore this line
			// WARNING: don't put any ifs that require "obj" above seek_end; obj is not guaranteed
			continue;
		}
		else if (OBJ_FILE_TAG(line, "Version:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				OBJ_VERSION(obj) = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Location:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0]) == 1) {
				*location = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Keywords:", length)) {
			if (GET_OBJ_KEYWORDS(obj) && (!proto || GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(proto))) {
				free(GET_OBJ_KEYWORDS(obj));
			}
			GET_OBJ_KEYWORDS(obj) = fread_string(fl, error);
		}
		else if (OBJ_FILE_TAG(line, "Short-desc:", length)) {
			if (GET_OBJ_SHORT_DESC(obj) && (!proto || GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(proto))) {
				free(GET_OBJ_SHORT_DESC(obj));
			}
			GET_OBJ_SHORT_DESC(obj) = fread_string(fl, error);
		}
		else if (OBJ_FILE_TAG(line, "Long-desc:", length)) {
			if (GET_OBJ_LONG_DESC(obj) && (!proto || GET_OBJ_LONG_DESC(obj) != GET_OBJ_LONG_DESC(proto))) {
				free(GET_OBJ_LONG_DESC(obj));
			}
			GET_OBJ_LONG_DESC(obj) = fread_string(fl, error);
		}
		else if (OBJ_FILE_TAG(line, "Action-desc:", length)) {
			if (GET_OBJ_ACTION_DESC(obj) && (!proto || GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(proto))) {
				free(GET_OBJ_ACTION_DESC(obj));
			}
			GET_OBJ_ACTION_DESC(obj) = fread_string(fl, error);
		}
		else if (OBJ_FILE_TAG(line, "Extra-desc:", length)) {
			if (proto && obj->ex_description == proto->ex_description) {
				obj->ex_description = NULL;
			}
			
			CREATE(ex, struct extra_descr_data, 1);
			ex->next = obj->ex_description;
			obj->ex_description = ex;
			
			ex->keyword = fread_string(fl, error);
			ex->description = fread_string(fl, error);
		}
		else if (OBJ_FILE_TAG(line, "Val-0:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				GET_OBJ_VAL(obj, 0) = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Val-1:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				GET_OBJ_VAL(obj, 1) = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Val-2:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				GET_OBJ_VAL(obj, 2) = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Type:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				GET_OBJ_TYPE(obj) = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Wear:", length)) {
			if (sscanf(line + length + 1, "%s", s_in)) {
				GET_OBJ_WEAR(obj) = asciiflag_conv(s_in);
			}
		}
		else if (OBJ_FILE_TAG(line, "Flags:", length)) {
			if (sscanf(line + length + 1, "%s", s_in)) {
				GET_OBJ_EXTRA(obj) = asciiflag_conv(s_in);
			}
		}
		else if (OBJ_FILE_TAG(line, "Affects:", length)) {
			if (sscanf(line + length + 1, "%s", s_in)) {
				obj->obj_flags.bitvector = asciiflag_conv(s_in);
			}
		}
		else if (OBJ_FILE_TAG(line, "Timer:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				GET_OBJ_TIMER(obj) = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Current-scale:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				GET_OBJ_CURRENT_SCALE_LEVEL(obj) = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Min-scale:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				GET_OBJ_MIN_SCALE_LEVEL(obj) = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Max-scale:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				GET_OBJ_MAX_SCALE_LEVEL(obj) = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Material:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				GET_OBJ_MATERIAL(obj) = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Last-empire:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				obj->last_empire_id = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Last-owner:", length)) {
			if (sscanf(line + length + 1, "%d", &l_in)) {
				obj->last_owner_id = l_in;
			}
		}
		else if (OBJ_FILE_TAG(line, "Stolen-timer:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				obj->stolen_timer = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Autostore-timer:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				GET_AUTOSTORE_TIMER(obj) = i_in[0];
			}
		}
		else if (OBJ_FILE_TAG(line, "Apply:", length)) {
			if (sscanf(line + length + 1, "%d %d %d", &i_in[0], &i_in[1], &i_in[2])) {
				obj->affected[i_in[0]].location = i_in[1];
				obj->affected[i_in[0]].modifier = i_in[2];
			}
		}
		else if (OBJ_FILE_TAG(line, "Bound-to:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0])) {
				struct obj_binding *bind;
				CREATE(bind, struct obj_binding, 1);
				bind->idnum = i_in[0];
				bind->next = OBJ_BOUND_TO(obj);
				OBJ_BOUND_TO(obj) = bind;
			}
		}
		else if (OBJ_FILE_TAG(line, "Trigger:", length)) {
			if (sscanf(line + length + 1, "%d", &i_in[0]) && real_trigger(i_in[0])) {
				if (!SCRIPT(obj)) {
					CREATE(SCRIPT(obj), struct script_data, 1);
				}
				add_trigger(SCRIPT(obj), read_trigger(i_in[0]), -1);
			}
		}
		
		// ignore anything else
		else {
			// just discard line as junk
		}
	}
	
	// we were not guaranteed an object
	if (obj) {
		ensure_safe_obj(obj);
	}
	
	// check versioning: load a new version
	if (obj && proto && OBJ_VERSION(obj) < OBJ_VERSION(proto) && config_get_bool("auto_update_items")) {
		new = fresh_copy_obj(obj, GET_OBJ_CURRENT_SCALE_LEVEL(obj));		
		extract_obj(obj);
		obj = new;
		
		if (notify && notify->desc) {
			msg_to_char(notify, "&yItem '%s' updated.&0\r\n", GET_OBJ_SHORT_DESC(obj));
		}
	}
	
	return obj;
}


/**
* Return values:
*  0 - successful load (RENT_RENTED)
*  1 - load failure or load of crash items -- (this is normal)
*
* @param char_data *ch The player whose objects we're loading
* @param bool dolog If TRUE, syslogs the player.
*/
int Objload_char(char_data *ch, int dolog) {
	void auto_equip(char_data *ch, obj_data *obj, int *location);

	char fname[MAX_STRING_LENGTH], line[MAX_INPUT_LENGTH];
	int rent_code = RENT_CRASH, iter;
	struct coin_data *coin, *last_coin = NULL;
	obj_data *obj, *obj2, *cont_row[MAX_BAG_ROWS];
	int location, int_in[2];
	obj_vnum vnum;
	long long_in;
	FILE *fl;

	/* Empty all of the container lists (you never know ...) */
	for (iter = 0; iter < MAX_BAG_ROWS; ++iter) {
		cont_row[iter] = NULL;
	}

	if (!get_filename(GET_NAME(ch), fname, CRASH_FILE)) {
		return (1);
	}
	if (!(fl = fopen(fname, "r"))) {
		if (errno != ENOENT) {	/* if it fails, NOT because of no file */
			log("SYSERR: READING OBJECT FILE %s (5): %s", fname, strerror(errno));
			send_to_char("\r\n********************* NOTICE *********************\r\n"
						 "There was a problem loading your objects from disk.\r\n"
						 "Contact an immortal for assistance.\r\n", ch);
		}
		if (dolog) {
			syslog(SYS_LOGIN, GET_INVIS_LEV(ch), TRUE, "%s entering game with no equipment.", GET_NAME(ch));
			if (GET_INVIS_LEV(ch) == 0) {
				if (config_get_bool("public_logins")) {
					mortlog("%s has entered the game", PERS(ch, ch, TRUE));
				}
				else if (GET_LOYALTY(ch)) {
					log_to_empire(GET_LOYALTY(ch), ELOG_LOGINS, "%s has entered the game", PERS(ch, ch, TRUE));
				}
			}
		}
		return (1);
	}
	
	// iterate over file
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in crash file for %s", GET_NAME(ch));
			fclose(fl);
			return (1);
		}
		
		if (*line == '$') {
			break;
		}
		else if (*line == '#') {
			if (sscanf(line, "#%d", &vnum) < 1) {
				log("SYSERR: Format error in vnum line of crash file for %s", GET_NAME(ch));
				fclose(fl);
				return (1);
			}
			
			if ((obj = Obj_load_from_file(fl, vnum, &location, ch))) {
				// Obj_load_from_file may return a NULL for deleted objs
				
				auto_equip(ch, obj, &location);
				/*
				 * What to do with a new loaded item:
				 *
				 * If there's a list with location less than 1 below this, then its
				 * container has disappeared from the file so we put the list back into
				 * the character's inventory. (Equipped items are 0 here.)
				 *
				 * If there's a list of contents with location of 1 below this, then we
				 * check if it is a container:
				 *   - Yes: Get it from the character, fill it, and give it back so we
				 *          have the correct weight.
				 *   -  No: The container is missing so we put everything back into the
				 *          character's inventory.
				 *
				 * For items with negative location, we check if there is already a list
				 * of contents with the same location.  If so, we put it there and if not,
				 * we start a new list.
				 *
				 * Since location for contents is < 0, the list indices are switched to
				 * non-negative.
				 *
				 * This looks ugly, but it works.
				 */
				if (location > 0) {		/* Equipped */
					for (iter = MAX_BAG_ROWS - 1; iter > 0; --iter) {
						if (cont_row[iter]) {	/* No container, back to inventory. */
							for (; cont_row[iter]; cont_row[iter] = obj2) {
								obj2 = cont_row[iter]->next_content;
								obj_to_char(cont_row[iter], ch);
							}
							cont_row[iter] = NULL;
						}
					}
					if (cont_row[0]) {	/* Content list existing. */
						if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER || GET_OBJ_TYPE(obj) == ITEM_CART || IS_CORPSE(obj)) {
							/* Remove object, fill it, equip again. */
							obj = unequip_char(ch, location - 1);
							obj->contains = NULL;	/* Should be NULL anyway, but just in case. */
							for (; cont_row[0]; cont_row[0] = obj2) {
								obj2 = cont_row[0]->next_content;
								obj_to_obj(cont_row[0], obj);
							}
							equip_char(ch, obj, location - 1);
						}
						else {			/* Object isn't container, empty the list. */
							for (; cont_row[0]; cont_row[0] = obj2) {
								obj2 = cont_row[0]->next_content;
								obj_to_char(cont_row[0], ch);
							}
							cont_row[0] = NULL;
						}
					}
				}
				else {	/* location <= 0 */
					for (iter = MAX_BAG_ROWS - 1; iter > -location; --iter) {
						if (cont_row[iter]) {	/* No container, back to inventory. */
							for (; cont_row[iter]; cont_row[iter] = obj2) {
								obj2 = cont_row[iter]->next_content;
								obj_to_char(cont_row[iter], ch);
							}
							cont_row[iter] = NULL;
						}
					}
					if (iter == -location && cont_row[iter]) {	/* Content list exists. */
						if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER || GET_OBJ_TYPE(obj) == ITEM_CART || IS_CORPSE(obj)) {
							/* Take the item, fill it, and give it back. */
							obj_from_char(obj);
							obj->contains = NULL;
							for (; cont_row[iter]; cont_row[iter] = obj2) {
								obj2 = cont_row[iter]->next_content;
								obj_to_obj(cont_row[iter], obj);
							}
							obj_to_char(obj, ch);	/* Add to inventory first. */
						}
						else {	/* Object isn't container, empty content list. */
							for (; cont_row[iter]; cont_row[iter] = obj2) {
								obj2 = cont_row[iter]->next_content;
								obj_to_char(cont_row[iter], ch);
							}
							cont_row[iter] = NULL;
						}
					}
					if (location < 0 && location >= -MAX_BAG_ROWS) {
						/*
						 * Let the object be part of the content list but put it at the
						 * list's end.  Thus having the items in the same order as before
						 * the character rented.
						 */
						obj_from_char(obj);
						if ((obj2 = cont_row[-location - 1]) != NULL) {
							while (obj2->next_content) {
								obj2 = obj2->next_content;
							}
							obj2->next_content = obj;
						}
						else {
							cont_row[-location - 1] = obj;
						}
					}
				}
			}
			else {
				// No obj returned: it was probably just deleted...
				
				/*
				log("SYSERR: Got back non-object while loading vnum %d for %s", vnum, GET_NAME(ch));
				fclose(fl);
				return (1);
				*/
			}
		}
		else if (!strn_cmp(line, "Rent-time:", 10)) {
			// don't care about the time
		}
		else if (!strn_cmp(line, "Rent-code:", 10)) {
			if (sscanf(line + 11, "%d", &rent_code) < 1) {
				log("SYSERR: Format error in Rent-code line of crash file for %s", GET_NAME(ch));
				fclose(fl);
				return (1);
			}
		}
		else if (!strn_cmp(line, "Coin:", 5)) {
			if (sscanf(line + 6, "%d %d %ld", &int_in[0], &int_in[1], &long_in) != 3) {
				log("SYSERR: Format in Coin line of crash file for %s", GET_NAME(ch));
				fclose(fl);
				return(1);
			}
			
			CREATE(coin, struct coin_data, 1);
			coin->amount = int_in[0];
			coin->empire_id = int_in[1];
			coin->last_acquired = (time_t)long_in;
			coin->next = NULL;
			
			// add to end
			if (last_coin) {
				last_coin->next = coin;
			}
			else {
				GET_PLAYER_COINS(ch) = coin;
			}
			last_coin = coin;
		}
		else {
			log("SYSERR: Format error in crash file for %s: %s", GET_NAME(ch), line);
			fclose(fl);
			return (1);
		}
	}
	
	// syslog
	switch (rent_code) {
		case RENT_RENTED: {
			if (dolog) {
				syslog(SYS_LOGIN, GET_INVIS_LEV(ch), TRUE, "%s un-renting and entering game.", GET_NAME(ch));
			}
			break;
		}
		case RENT_CRASH: {
			if (dolog) {
				syslog(SYS_LOGIN, GET_INVIS_LEV(ch), TRUE, "%s retrieving crash-saved items and entering game.", GET_NAME(ch));
			}
			break;
		}
		default: {
			syslog(SYS_LOGIN, GET_INVIS_LEV(ch), TRUE, "SYSERR: %s entering game with undefined rent code %d.", GET_NAME(ch), rent_code);
			break;
		}
	}
	
	// mortlog
	if (dolog && GET_INVIS_LEV(ch) == 0) {
		if (config_get_bool("public_logins")) {
			mortlog("%s has entered the game", PERS(ch, ch, TRUE));
		}
		else if (GET_LOYALTY(ch)) {
			log_to_empire(GET_LOYALTY(ch), ELOG_LOGINS, "%s has entered the game", PERS(ch, ch, TRUE));
		}
	}

	fclose(fl);
	return (rent_code == RENT_RENTED) ? 0 : 1;
}


 //////////////////////////////////////////////////////////////////////////////
//// BASIC OBJECT SAVING /////////////////////////////////////////////////////


/**
* Extracts an object, its contents, and the next content in whatever list it's
* in.
*
* @param obj_data *obj The object to extract.
*/
void Crash_extract_objs(obj_data *obj) {
	if (obj) {
		Crash_extract_objs(obj->contains);
		Crash_extract_objs(obj->next_content);
		extract_obj(obj);
	}
}


/**
* Saves an object, its contents, and the rest of the content list it's in.
*
* @param obj_data *obj The object to save.
* @param FILE *fp The file to save to (open for writing).
* @param int location The gear or bag position.
*/
void Crash_save(obj_data *obj, FILE *fp, int location) {
	void Crash_save_one_obj_to_file(FILE *fl, obj_data *obj, int location);

	if (obj) {
		Crash_save(obj->next_content, fp, location);
		Crash_save(obj->contains, fp, MIN(LOC_INVENTORY, location) - 1);
		Crash_save_one_obj_to_file(fp, obj, location);
	}
}


/**
* save_one_obj_to_file: Write an object to a tagged save file, using only those
* properties which are different from the prototype.
*
* @param FILE *fl The file to save to (open for writing).
* @param obj_data *obj The object to save.
* @param int location The current wear location, if any.
*/
void Crash_save_one_obj_to_file(FILE *fl, obj_data *obj, int location) {
	char temp[MAX_STRING_LENGTH];
	struct extra_descr_data *ex;
	struct obj_binding *bind;
	obj_data *proto;
	int iter;
	
	if (!fl || !obj) {
		log("SYSERR: obj_save_to_file called without %s", fl ? "object" : "file");
		return;
	}
	
	proto = obj_proto(GET_OBJ_VNUM(obj));
	
	fprintf(fl, "#%d\n", GET_OBJ_VNUM(obj));
	fprintf(fl, "Version: %d\n", OBJ_VERSION(obj));	// for auto-updating
	
	if (location != 0) {
		fprintf(fl, "Location: %d\n", location);
	}
	
	if (!proto || GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(proto)) {
		fprintf(fl, "Keywords:\n%s~\n", NULLSAFE(GET_OBJ_KEYWORDS(obj)));
	}
	if (!proto || GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(proto)) {
		fprintf(fl, "Short-desc:\n%s~\n", NULLSAFE(GET_OBJ_SHORT_DESC(obj)));
	}
	if (!proto || GET_OBJ_LONG_DESC(obj) != GET_OBJ_LONG_DESC(proto)) {
		fprintf(fl, "Long-desc:\n%s~\n", NULLSAFE(GET_OBJ_LONG_DESC(obj)));
	}
	if (!proto || GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(proto)) {
		strcpy(temp, NULLSAFE(GET_OBJ_ACTION_DESC(obj)));
		strip_crlf(temp);
		fprintf(fl, "Action-desc:\n%s~\n", temp);
	}

	if (!proto || obj->ex_description != proto->ex_description) {
		for (ex = obj->ex_description; ex; ex = ex->next) {
			fprintf(fl, "Extra-desc:\n%s~\n", NULLSAFE(ex->keyword));
			strcpy(temp, NULLSAFE(ex->description));
			strip_crlf(temp);
			fprintf(fl, "%s~\n", temp);
		}
	}

	// always save obj vals	
	for (iter = 0; iter < NUM_OBJ_VAL_POSITIONS; ++iter) {
		fprintf(fl, "Val-%d: %d\n", iter, GET_OBJ_VAL(obj, iter));
	}
	
	// always save flags
	fprintf(fl, "Flags: %s\n", bitv_to_alpha(GET_OBJ_EXTRA(obj)));
	
	if (!proto || GET_OBJ_TYPE(obj) != GET_OBJ_TYPE(proto)) {
		fprintf(fl, "Type: %d\n", GET_OBJ_TYPE(obj));
	}
	if (!proto || GET_OBJ_WEAR(obj) != GET_OBJ_WEAR(proto)) {
		fprintf(fl, "Wear: %s\n", bitv_to_alpha(GET_OBJ_WEAR(obj)));
	}
	if (!proto || obj->obj_flags.bitvector != proto->obj_flags.bitvector) {
		fprintf(fl, "Affects: %s\n", bitv_to_alpha(obj->obj_flags.bitvector));
	}
	if (!proto || GET_OBJ_TIMER(obj) != GET_OBJ_TIMER(proto)) {
		fprintf(fl, "Timer: %d\n", GET_OBJ_TIMER(obj));
	}
	if (!proto || GET_OBJ_MATERIAL(obj) != GET_OBJ_MATERIAL(proto)) {
		fprintf(fl, "Material: %d\n", GET_OBJ_MATERIAL(obj));
	}
	if (!proto || GET_OBJ_CURRENT_SCALE_LEVEL(obj) != GET_OBJ_CURRENT_SCALE_LEVEL(proto)) {
		fprintf(fl, "Current-scale: %d\n", GET_OBJ_CURRENT_SCALE_LEVEL(obj));
	}
	if (!proto || GET_OBJ_MIN_SCALE_LEVEL(obj) != GET_OBJ_MIN_SCALE_LEVEL(proto)) {
		fprintf(fl, "Min-scale: %d\n", GET_OBJ_MIN_SCALE_LEVEL(obj));
	}
	if (!proto || GET_OBJ_MAX_SCALE_LEVEL(obj) != GET_OBJ_MAX_SCALE_LEVEL(proto)) {
		fprintf(fl, "Max-scale: %d\n", GET_OBJ_MAX_SCALE_LEVEL(obj));
	}

	if (obj->last_empire_id != NOTHING) {
		fprintf(fl, "Last-empire: %d\n", obj->last_empire_id);
	}
	if (obj->last_owner_id != NOBODY) {
		fprintf(fl, "Last-owner: %d\n", obj->last_owner_id);
	}
	if (IS_STOLEN(obj)) {
		fprintf(fl, "Stolen-timer: %d\n", (int) obj->stolen_timer);
	}
	if (GET_AUTOSTORE_TIMER(obj) > 0) {
		fprintf(fl, "Autostore-timer: %d\n", (int) GET_AUTOSTORE_TIMER(obj));
	}
	
	// always save applies
	for (iter = 0; iter < MAX_OBJ_AFFECT; ++iter) {
		fprintf(fl, "Apply: %d %d %d\n", iter, obj->affected[iter].location, obj->affected[iter].modifier);
	}
	
	// who it's bound to
	for (bind = OBJ_BOUND_TO(obj); bind; bind = bind->next) {
		fprintf(fl, "Bound-to: %d\n", bind->idnum);
	}
	
	// scripts
	if (SCRIPT(obj)) {
		trig_data *trig;
		
		for (trig = TRIGGERS(SCRIPT(obj)); trig; trig = trig->next) {
			fprintf(fl, "Trigger: %d\n", GET_TRIG_VNUM(trig));
		}
		
		// TODO could save SCRIPT(obj)->global_vars here too
	}
	
	fprintf(fl, "End\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER OBJECT LOADING ///////////////////////////////////////////////////

/**
* This code seeks to autoequip the character with an item from the crash file,
* rather than loading all items into inventory like a stock CircleMUD.
*
* The 'location' parameter is a WEAR_x flag + 1 (the +1 is because 0 is
* special-cased in the autoequip code we used).
*
* This function formerly checked if the object was wearable in the position
* where it was equipped, but since we now store wear flags to file, it didn't
* seem necessary.
*
* @param char_data *ch The player we're loading gear for.
* @param obj_data *obj The item to auto-equip.
* @param int *location The preferred equipment slot.
*/
void auto_equip(char_data *ch, obj_data *obj, int *location) {
	int pos = *location - 1;
	
	if (pos >= 0 && pos < NUM_WEARS) {
		if (!CAN_WEAR(obj, wear_data[pos].item_wear)) {
			// drop to inventory -- no longer wearable there
			*location = LOC_INVENTORY;
			obj_to_char(obj, ch);
		}
		else if (!GET_EQ(ch, pos)) {
			equip_char(ch, obj, pos);
		}
		else {
			log("SYSERR: auto_equip: %s already equipped in position %d", GET_NAME(ch), pos);
			*location = LOC_INVENTORY;
			obj_to_char(obj, ch);
		}
	}
	else {
		// to inventory
		obj_to_char(obj, ch);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER OBJECT SAVING ////////////////////////////////////////////////////

/**
* This crash-saves all players in the game.
*/
void Crash_save_all(void) {
	void Objsave_char(char_data *ch, int rent_code);
	void write_aliases(char_data *ch);

	descriptor_data *d;

	for (d = descriptor_list; d; d = d->next) {
		if ((STATE(d) == CON_PLAYING) && !IS_NPC(d->character)) {
			Objsave_char(d->character, RENT_CRASH);
			write_aliases(d->character);
			SAVE_CHAR(d->character);
		}
	}
}


/**
* Objsave_char() saves a player's equipment and inventory to file. It can be
* called with two modes:
*    RENT_RENTED - for players leaving the game, extracts their gear
*    RENT_CRASH - for players still in the game, does not extact gear
*
* @param char_data *ch The player whose inventory we're saving.
* @param int rent_code RENT_RENTED or RENT_CRASH.
*/
void Objsave_char(char_data *ch, int rent_code) {
	char filename[MAX_INPUT_LENGTH], tempfile[MAX_INPUT_LENGTH];
	struct coin_data *coin;
	int iter;
	FILE *fp;

	if (IS_NPC(ch) || !get_filename(GET_NAME(ch), filename, CRASH_FILE)) {
		return;
	}
	
	strcpy(tempfile, filename);
	strcat(tempfile, TEMP_SUFFIX);
	
	if (!(fp = fopen(tempfile, "w"))) {
		log("SYSERR: Unable to save rent file for %s", GET_NAME(ch));
		return;
	}
	
	fprintf(fp, "Rent-time: %d\n", (int) time(0));
	fprintf(fp, "Rent-code: %d\n", rent_code);
	
	for (coin = GET_PLAYER_COINS(ch); coin; coin = coin->next) {
		fprintf(fp, "Coin: %d %d %ld\n", coin->amount, coin->empire_id, coin->last_acquired);
	}

	for (iter = 0; iter < NUM_WEARS; iter++) {
		if (GET_EQ(ch, iter)) {
			// save at iter+1 because 0 == LOC_INVENTORY
			Crash_save(GET_EQ(ch,iter), fp, iter + 1);
			
			if (rent_code == RENT_RENTED) {
				Crash_extract_objs(GET_EQ(ch, iter));
			}
		}
	}
	
	Crash_save(ch->carrying, fp, LOC_INVENTORY);
	if (rent_code == RENT_RENTED) {
		Crash_extract_objs(ch->carrying);
	}
	
	fprintf(fp, "$\n");
	
	fclose(fp);
	rename(tempfile, filename);
}


 //////////////////////////////////////////////////////////////////////////////
//// WORLD OBJECT SAVING /////////////////////////////////////////////////////

/**
* Objpack-saving is based on player item saving, but it saves current objects
* for each world room, including changes to the objects. This is different from
* stock CircleMUDs, which don't save objects dropped in rooms, and where zone
* update commands don't support loading objects other than prototypes.
*
* @param room_data *room The world location whose objects we are saving.
*/
bool objpack_save_room(room_data *room) {
	char filename[MAX_INPUT_LENGTH], tempname[MAX_INPUT_LENGTH];
	FILE *fp;

	if (!ROOM_CONTENTS(room)) {
		return FALSE;
	}

	// TODO split into dirs?
	sprintf(filename, "%s%d.%s", LIB_OBJPACK, GET_ROOM_VNUM(room), SUF_PACK);
	strcpy(tempname, filename);
	strcat(tempname, TEMP_SUFFIX);

	if (!(fp = fopen(tempname, "w"))) {
		return FALSE;
	}

	fprintf(fp, "Rent-time: %d\n", (int) time(0));
	fprintf(fp, "Rent-code: %d\n", RENT_CRASH);

	Crash_save(ROOM_CONTENTS(room), fp, LOC_INVENTORY);

	fprintf(fp, "$\n");

	fclose(fp);
	rename(tempname, filename);

	return TRUE;
}


/**
* Loads an object pack for a room. This is done at startup.
*
* @param room_data *room The room.
*/
void objpack_load_room(room_data *room) {
	obj_data *obj, *obj2, *cont_row[MAX_BAG_ROWS];
	char fname[MAX_STRING_LENGTH], line[MAX_INPUT_LENGTH];
	int iter, location, rent_code = 0;
	obj_vnum vnum;
	time_t timer;
	FILE *fl;

	// empty container lists
	for (iter = 0; iter < MAX_BAG_ROWS; iter++) {
		cont_row[iter] = NULL;
	}

	sprintf(fname, "%s%d.%s", LIB_OBJPACK, GET_ROOM_VNUM(room), SUF_PACK);

	if (!(fl = fopen(fname, "r"))) {
		if (errno != ENOENT) {
			log("SYSERR: READING OBJECT FILE %s (5): %s", fname, strerror(errno));
		}
		return;
	}
	
	// iterate over file
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in pack file %s", fname);
			fclose(fl);
			return;
		}
		
		if (*line == '$') {
			break;
		}
		else if (*line == '#') {
			if (sscanf(line, "#%d", &vnum) < 1) {
				log("SYSERR: Format error in vnum line of pack file %s", fname);
				fclose(fl);
				return;
			}
			
			if ((obj = Obj_load_from_file(fl, vnum, &location, NULL))) {
				// Obj_load_from_file may return a NULL for deleted objs
				
				// Not really an inventory, but same idea.
				if (location > 0) {
					location = LOC_INVENTORY;
				}

				// store autostore timer through obj_to_room
				timer = GET_AUTOSTORE_TIMER(obj);
				obj_to_room(obj, room);
				GET_AUTOSTORE_TIMER(obj) = timer;
				
				for (iter = MAX_BAG_ROWS - 1; iter > -location; --iter) {
					if (cont_row[iter]) {		/* No container, back to room. */
						for (; cont_row[iter]; cont_row[iter] = obj2) {
							obj2 = cont_row[iter]->next_content;
							timer = GET_AUTOSTORE_TIMER(cont_row[iter]);
							obj_to_room(cont_row[iter], room);
							GET_AUTOSTORE_TIMER(cont_row[iter]) = timer;
						}
						cont_row[iter] = NULL;
					}
				}
				if (iter == -location && cont_row[iter]) {			/* Content list exists. */
					if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER || GET_OBJ_TYPE(obj) == ITEM_CART || IS_CORPSE(obj)) {
						/* Take the item, fill it, and give it back. */
						obj_from_room(obj);
						obj->contains = NULL;
						for (; cont_row[iter]; cont_row[iter] = obj2) {
							obj2 = cont_row[iter]->next_content;
							obj_to_obj(cont_row[iter], obj);
						}
						timer = GET_AUTOSTORE_TIMER(obj);
						obj_to_room(obj, room);			/* Add to room first. */
						GET_AUTOSTORE_TIMER(obj) = timer;
					}
					else {				/* Object isn't container, empty content list. */
						for (; cont_row[iter]; cont_row[iter] = obj2) {
							obj2 = cont_row[iter]->next_content;
							timer = GET_AUTOSTORE_TIMER(cont_row[iter]);
							obj_to_room(cont_row[iter], room);
							GET_AUTOSTORE_TIMER(cont_row[iter]) = timer;
						}
						cont_row[iter] = NULL;
					}
				}
				if (location < 0 && location >= -MAX_BAG_ROWS) {
					obj_from_room(obj);
					if ((obj2 = cont_row[-location - 1]) != NULL) {
						while (obj2->next_content) {
							obj2 = obj2->next_content;
						}
						obj2->next_content = obj;
					}
					else {
						cont_row[-location - 1] = obj;
					}
				}
			}
			else {
				// no obj returned -- it was probably deleted
				
				/*
				log("SYSERR: Got back non-object while loading vnum %d for %s", vnum, fname);
				fclose(fl);
				return;
				*/
			}
		}
		else if (!strn_cmp(line, "Rent-time:", 10)) {
			// don't care about the time
		}
		else if (!strn_cmp(line, "Rent-code:", 10)) {
			if (sscanf(line + 11, "%d", &rent_code) < 1) {
				log("SYSERR: Format error in Rent-code line of pack file %s", fname);
				fclose(fl);
				return;
			}
		}
		else {
			log("SYSERR: Format error in pack file %s: %s", fname, line);
			fclose(fl);
			return;
		}
	}

	fclose(fl);
}


 //////////////////////////////////////////////////////////////////////////////
//// OBJECT UTILS ////////////////////////////////////////////////////////////

/**
* This function checks how old a player's crash file is and deletes it if
* necessary. This is called automatically at startup if quickstart is off.
*
* @param char *name The character name to check and delete.
*/
void Crash_clean_file(char *name) {
	void Crash_delete_file(char *name);
	
	char fname[MAX_STRING_LENGTH], line[MAX_INPUT_LENGTH], filetype[20];
	int rent_time, rent_code;
	FILE *fl;

	if (!get_filename(name, fname, CRASH_FILE))
		return;

	if (!(fl = fopen(fname, "r"))) {
		if (errno != ENOENT) {
			/* if it fails, NOT because of no file */
			log("SYSERR: OPENING OBJECT FILE %s (4): %s", fname, strerror(errno));
		}
		return;
	}
	
	// TODO these rely on these coming in order -- should change this to iterate to find them
	if (!get_line(fl, line) || sscanf(line, "Rent-time: %d", &rent_time) < 1) {
		log("SYSERR: Error getting Rent-time from file %s", fname);
		return;
	}
	if (!get_line(fl, line) || sscanf(line, "Rent-code: %d", &rent_code) < 1) {
		log("SYSERR: Error getting Rent-code from file %s", fname);
		return;
	}
	
	// don't need the rest of the file
	fclose(fl);

	if (rent_time < time(0) - (config_get_int("obj_file_timeout") * SECS_PER_REAL_DAY)) {
		Crash_delete_file(name);
		
		switch (rent_code) {
			case RENT_CRASH: {
				strcpy(filetype, "crash");
				break;
			}
			case RENT_RENTED: {
				strcpy(filetype, "rent");
				break;
			}
			default: {
				strcpy(filetype, "UNKNOWN!");
				break;
			}
		}
		log("    Deleting %s's %s file.", name, filetype);
	}
}


/**
* This deletes the inventory "crash" file for a player.
*
* @param char *name The name of the player.
*/
void Crash_delete_file(char *name) {
	char filename[50];
	FILE *fl;

	if (get_filename(name, filename, CRASH_FILE)) {
		if (!(fl = fopen(filename, "r"))) {
			if (errno != ENOENT) {
				/* if it fails but NOT because of no file */
				log("SYSERR: deleting crash file %s (1): %s", filename, strerror(errno));
			}
		}
		else {
			fclose(fl);

			/* if it fails, NOT because of no file */
			if (remove(filename) < 0 && errno != ENOENT) {
				log("SYSERR: deleting crash file %s (2): %s", filename, strerror(errno));
			}
		}
	}
}


/**
* This function lists a given character's rent (inventory/gear) file to the
* player who requested it. This is mainly used by "show rent".
*
* @param char_data *ch The person who's looking up the information.
* @param char *name The name of the person whose rent file to show.
*/
void Crash_listrent(char_data *ch, char *name) {
	FILE *fl;
	char fname[MAX_INPUT_LENGTH], line[MAX_INPUT_LENGTH], error[MAX_INPUT_LENGTH];
	char *tmp;
	obj_vnum last_vnum = NOTHING;
	bool last_sent = TRUE;
	int amount, eid;
	int junk;

	if (!get_filename(name, fname, CRASH_FILE) || !(fl = fopen(fname, "r"))) {
		msg_to_char(ch, "Unable to load rent file for %s.\r\n", name);
		return;
	}
	
	// in case of fread_string error
	sprintf(error, "rent file %s", fname);
	
	// start output queue
	sprintf(buf, "%s\r\n", fname);
	
	// TODO this whole thing needs length-checking to prevent overruns
	while (get_line(fl, line)) {
		if (*line == '$') {
			break;
		}
		else if (*line == '#') {
			if (!last_sent) {
				sprintf(buf + strlen(buf), " [%5d] %s\r\n", last_vnum, get_obj_name_by_proto(last_vnum));
			}
			sscanf(line, "#%d", &last_vnum);
			last_sent = FALSE;
		}
		else if (!strn_cmp(line, "Short-desc:", 11)) {
			if (last_vnum != NOTHING) {
				tmp = fread_string(fl, error);
				sprintf(buf + strlen(buf), " [%5d] %s\r\n", last_vnum, tmp);
				free(tmp);
				last_sent = TRUE;
			}
		}
		else if (!strn_cmp(line, "Coin:", 5)) {
			if (sscanf(line + 6, "%d %d %d", &amount, &eid, &junk) == 3) {
				sprintf(buf + strlen(buf), " [%5d] %s\r\n", NOTHING, money_amount(real_empire(eid), amount));
			}
		}
		else {
			// ignore! we only care about those properties above
		}
	}
	
	if (!last_sent) {
		sprintf(buf + strlen(buf), " [%5d] %s\r\n", last_vnum, get_obj_name_by_proto(last_vnum));
	}
	
	send_to_char(buf, ch);
	fclose(fl);
}


/**
* This is run at startup if the mud is not run in quick-start mode. It runs
* the obj-file cleaner on all players.
*/
void update_obj_file(void) {
	int iter;

	for (iter = 0; iter <= top_of_p_table; ++iter) {
		if (*player_table[iter].name) {
			Crash_clean_file(player_table[iter].name);
		}
	}
}
