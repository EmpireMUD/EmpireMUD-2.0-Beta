/* ************************************************************************
*   File: objsave.c                                       EmpireMUD 2.0b5 *
*  Usage: loading/saving player objects for rent and crash-save           *
*                                                                         *
*  Note: ITEM_CART is deprecated but this will still put items inside of  *
*  one so that the 2.0 b3.8 auto-converter will move them to vehicles.    *
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
#include "constants.h"

/**
* Contents:
*   Basic Object Loading
*   Basic Object Saving
*   Player Object Loading
*   World Object Saving
*   Object Utils
*/

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
		set_obj_keywords(obj, "object item unknown");
	}
	if (!GET_OBJ_SHORT_DESC(obj) || !*GET_OBJ_SHORT_DESC(obj)) {
		set_obj_short_desc(obj, "an unknown object");
	}
	if (!GET_OBJ_LONG_DESC(obj) || !*GET_OBJ_LONG_DESC(obj)) {
		set_obj_long_desc(obj, "An unknown object is here.");
	}
}


/**
* formerly obj_from_store
*
* @param FILE *fl The open item file.
* @param obj_vnum vnum The vnum of the item being loaded, or NOTHING for non-prototyped item.
* @param int *location A place to bind the current WEAR_ position of the item; also used to track container contents.
* @param char_data *notify Optional: A person to notify if an item is updated (NULL for none).
* @param char *error_str Used in logging errors.
* @return obj_data* The loaded item, or NULL if it's not available.
*/
obj_data *Obj_load_from_file(FILE *fl, obj_vnum vnum, int *location, char_data *notify, char *error_str) {
	char line[MAX_INPUT_LENGTH], error[MAX_STRING_LENGTH], s_in[MAX_INPUT_LENGTH], *tmp;
	obj_data *proto = obj_proto(vnum);
	struct extra_descr_data *ex;
	struct obj_apply *apply, *last_apply = NULL;
	obj_data *obj, *new;
	bool end = FALSE;
	int i_in[3];
	bool seek_end = FALSE;
	
	#define LOG_BAD_TAG_WARNINGS  TRUE	// triggers syslogs for invalid objfile tags
	#define BAD_TAG_WARNING(src)  else if (LOG_BAD_TAG_WARNINGS) { \
		log("SYSERR: Bad tag in object %d for %s: %s", vnum, NULLSAFE(error_str), (src));	\
	}
	
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
	
	// we always use the applies from the file, not from the proto
	if (obj) {
		free_obj_apply_list(GET_OBJ_APPLIES(obj));
		GET_OBJ_APPLIES(obj) = NULL;
	}
	
	// for fread_string
	sprintf(error, "Obj_load_from_file %d", vnum);

	while (!end) {
		if (!get_line(fl, line)) {
			log("SYSERR: Unexpected end of obj file in Obj_load_from_file for %s", NULLSAFE(error_str));
			exit(1);
		}
		
		if (!str_cmp(line, "End")) {
			// this MUST be before seek_end
			end = TRUE;
			continue;
		}
		if (seek_end) {
			// are we looking for the end of the object? ignore this line
			// WARNING: don't put any ifs that require "obj" above seek_end; obj is not guaranteed
			continue;
		}
		
		// normal tags by letter
		switch (UPPER(*line)) {
			case 'A': {
				if (!strn_cmp(line, "Action-desc:", 12)) {
					if (GET_OBJ_ACTION_DESC(obj) && (!proto || GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(proto))) {
						free(GET_OBJ_ACTION_DESC(obj));
					}
					GET_OBJ_ACTION_DESC(obj) = fread_string(fl, error);
				}
				else if (!strn_cmp(line, "Affects: ", 9)) {
					if (sscanf(line + 9, "%s", s_in)) {
						obj->obj_flags.bitvector = asciiflag_conv(s_in);
					}
				}
				else if (!strn_cmp(line, "Applies: ", 9)) {
					// this is the CURRENT version of the "Apply" tag
					if (sscanf(line + 9, "%d %d %d", &i_in[0], &i_in[1], &i_in[2])) {
						CREATE(apply, struct obj_apply, 1);
						apply->location = i_in[0];
						apply->modifier = i_in[1];
						apply->apply_type = i_in[2];
						
						// append to end
						if (last_apply) {
							last_apply->next = apply;
						}
						else {
							GET_OBJ_APPLIES(obj) = apply;
						}
						last_apply = apply;
					}
				}
				else if (!strn_cmp(line, "Apply: ", 7)) {
					// this is an OLD version of the tag that has a different set of params
					if (sscanf(line + 7, "%d %d %d", &i_in[0], &i_in[1], &i_in[2])) {
						if (i_in[1] != APPLY_NONE && i_in[2] != 0) {
							CREATE(apply, struct obj_apply, 1);
							apply->location = i_in[1];
							apply->modifier = i_in[2];
							apply->apply_type = APPLY_TYPE_NATURAL;
						
							// append to end
							if (last_apply) {
								last_apply->next = apply;
							}
							else {
								GET_OBJ_APPLIES(obj) = apply;
							}
							last_apply = apply;
						}
					}
				}
				else if (!strn_cmp(line, "Autostore-timer: ", 17)) {
					if (sscanf(line + 17, "%d", &i_in[0])) {
						GET_AUTOSTORE_TIMER(obj) = i_in[0];
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'B': {
				if (!strn_cmp(line, "Bound-to: ", 10)) {
					if (sscanf(line + 10, "%d", &i_in[0])) {
						struct obj_binding *bind;
						CREATE(bind, struct obj_binding, 1);
						bind->idnum = i_in[0];
						LL_PREPEND(OBJ_BOUND_TO(obj), bind);
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'C': {
				if (!strn_cmp(line, "Component: ", 11)) {
					if (sscanf(line + 11, "%d %s", &i_in[0], s_in) == 2) {
						// old pre-b5.88 version: ignore
						// GET_OBJ_CMP_TYPE(obj) = i_in[0];
						// GET_OBJ_CMP_FLAGS(obj) = asciiflag_conv(s_in);
					}
					else if (sscanf(line + 11, "%d", &i_in[0]) == 1) {
						// newer version
						if (GET_OBJ_VNUM(obj) == NOTHING) {
							// only allowed for 'anonymous' objs
							obj->proto_data->component = i_in[0];
						}
					}
				}
				else if (!strn_cmp(line, "Current-scale: ", 15)) {
					if (sscanf(line + 15, "%d", &i_in[0])) {
						GET_OBJ_CURRENT_SCALE_LEVEL(obj) = i_in[0];
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'E': {
				if (!strn_cmp(line, "Extra-desc:", 11)) {
					if (GET_OBJ_VNUM(obj) == NOTHING) {
						// only allowed on 'anonymous' objs
						CREATE(ex, struct extra_descr_data, 1);
						LL_PREPEND(obj->proto_data->ex_description, ex);
						
						ex->keyword = fread_string(fl, error);
						ex->description = fread_string(fl, error);
					}
					else {
						// not allowed; read in 2 strings but dump them
						if ((tmp = fread_string(fl, error))) {
							free(tmp);
						}
						if ((tmp = fread_string(fl, error))) {
							free(tmp);
						}
					}
				}
				else if (!strn_cmp(line, "Eq-set: ", 8)) {
					if (sscanf(line + 8, "%d %d", &i_in[0], &i_in[1]) == 2) {
						add_obj_to_eq_set(obj, i_in[0], i_in[1]);
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'F': {
				if (!strn_cmp(line, "Flags: ", 7)) {
					if (sscanf(line + 7, "%s", s_in)) {
						GET_OBJ_EXTRA(obj) = asciiflag_conv(s_in);
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'K': {
				if (!strn_cmp(line, "Keywords:", 9)) {
					if (GET_OBJ_KEYWORDS(obj) && (!proto || GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(proto))) {
						free(GET_OBJ_KEYWORDS(obj));
					}
					GET_OBJ_KEYWORDS(obj) = fread_string(fl, error);
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'L': {
				if (!strn_cmp(line, "Last-empire: ", 13)) {
					if (sscanf(line + 13, "%d", &i_in[0])) {
						obj->last_empire_id = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Last-owner: ", 12)) {
					if (sscanf(line + 12, "%d", &i_in[0])) {
						obj->last_owner_id = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Location: ", 10)) {
					if (sscanf(line + 10, "%d", &i_in[0]) == 1) {
						*location = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Long-desc:", 10)) {
					if (GET_OBJ_LONG_DESC(obj) && (!proto || GET_OBJ_LONG_DESC(obj) != GET_OBJ_LONG_DESC(proto))) {
						free(GET_OBJ_LONG_DESC(obj));
					}
					GET_OBJ_LONG_DESC(obj) = fread_string(fl, error);
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'M': {
				if (!strn_cmp(line, "Max-scale: ", 11)) {
					if (sscanf(line + 11, "%d", &i_in[0])) {
						if (GET_OBJ_VNUM(obj) == NOTHING) {
							// only allowed for 'anonymous' objs
							obj->proto_data->max_scale_level = i_in[0];
						}
					}
				}
				else if (!strn_cmp(line, "Material: ", 10)) {
					if (sscanf(line + 10, "%d", &i_in[0])) {
						if (GET_OBJ_VNUM(obj) == NOTHING) {
							// only allowed for 'anonymous' objs
							obj->proto_data->material = i_in[0];
						}
					}
				}
				else if (!strn_cmp(line, "Min-scale: ", 11)) {
					if (sscanf(line + 11, "%d", &i_in[0])) {
						if (GET_OBJ_VNUM(obj) == NOTHING) {
							// only allowed for 'anonymous' objs
							obj->proto_data->min_scale_level = i_in[0];
						}
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'Q': {
				if (!strn_cmp(line, "Quest: ", 7)) {
					if (sscanf(line + 7, "%d", &i_in[0])) {
						if (GET_OBJ_VNUM(obj) == NOTHING) {
							// only allowed for 'anonymous' objs
							obj->proto_data->requires_quest = i_in[0];
						}
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'R': {
				if (!strn_cmp(line, "Requires-tool: ", 15)) {
					if (sscanf(line + 15, "%s", s_in)) {
						if (GET_OBJ_VNUM(obj) == NOTHING) {
							// only allowed for 'anonymous' objs
							obj->proto_data->requires_tool = asciiflag_conv(s_in);
						}
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'S': {
				if (!strn_cmp(line, "Short-desc:", 11)) {
					if (GET_OBJ_SHORT_DESC(obj) && (!proto || GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(proto))) {
						free(GET_OBJ_SHORT_DESC(obj));
					}
					GET_OBJ_SHORT_DESC(obj) = fread_string(fl, error);
				}
				else if (!strn_cmp(line, "Stolen-from: ", 13)) {
					if (sscanf(line + 13, "%d", &i_in[0])) {
						obj->stolen_from = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Stolen-timer: ", 14)) {
					if (sscanf(line + 14, "%d", &i_in[0])) {
						obj->stolen_timer = i_in[0];
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'T': {
				if (!strn_cmp(line, "Timer: ", 7)) {
					if (sscanf(line + 7, "%d", &i_in[0])) {
						GET_OBJ_TIMER(obj) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Tool: ", 6)) {
					if (sscanf(line + 6, "%s", s_in)) {
						if (GET_OBJ_VNUM(obj) == NOTHING) {
							// only allowed for 'anonymous' objs
							obj->proto_data->tool_flags = asciiflag_conv(s_in);
						}
					}
				}
				else if (!strn_cmp(line, "Trigger: ", 9)) {
					if (sscanf(line + 9, "%d", &i_in[0]) && real_trigger(i_in[0])) {
						if (!SCRIPT(obj)) {
							create_script_data(obj, OBJ_TRIGGER);
						}
						add_trigger(SCRIPT(obj), read_trigger(i_in[0]), -1);
					}
				}
				else if (!strn_cmp(line, "Type: ", 6)) {
					if (sscanf(line + 6, "%d", &i_in[0])) {
						if (GET_OBJ_VNUM(obj) == NOTHING) {
							// only allowed for 'anonymous' objs
							obj->proto_data->type_flag = i_in[0];
						}
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'V': {
				if (!strn_cmp(line, "Val-0: ", 7)) {
					if (sscanf(line + 7, "%d", &i_in[0])) {
						GET_OBJ_VAL(obj, 0) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Val-1: ", 7)) {
					if (sscanf(line + 7, "%d", &i_in[0])) {
						GET_OBJ_VAL(obj, 1) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Val-2: ", 7)) {
					if (sscanf(line + 7, "%d", &i_in[0])) {
						GET_OBJ_VAL(obj, 2) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Variable: ", 10)) {
					if (sscanf(line + 10, "%s %d", s_in, &i_in[0]) != 2 || !get_line(fl, line)) {
						log("SYSERR: Bad variable format in Obj_load_from_file for %s: #%d", NULLSAFE(error_str), GET_OBJ_VNUM(obj));
						exit(1);
					}
					if (!SCRIPT(obj)) {
						create_script_data(obj, OBJ_TRIGGER);
					}
					add_var(&(SCRIPT(obj)->global_vars), s_in, line, i_in[0]);
				}
				else if (!strn_cmp(line, "Version: ", 9)) {
					if (sscanf(line + 9, "%d", &i_in[0])) {
						OBJ_VERSION(obj) = i_in[0];
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'W': {
				if (!strn_cmp(line, "Wear: ", 6)) {
					if (sscanf(line + 6, "%s", s_in)) {
						GET_OBJ_WEAR(obj) = asciiflag_conv(s_in);
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			
			// ignore anything else and move on
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
			stack_msg_to_desc(notify->desc, "&yItem '%s' updated.&0\r\n", GET_OBJ_SHORT_DESC(obj));
		}
	}
	
	return obj;
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
	struct trig_var_data *tvd;
	struct eq_set_obj *eq_set;
	struct obj_binding *bind;
	struct obj_apply *apply;
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

	if (!proto && GET_OBJ_EX_DESCS(obj)) {
		LL_FOREACH(GET_OBJ_EX_DESCS(obj), ex) {
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
	
	if (!proto) {	// only saves if no proto
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
	if (!proto) {	// only saves if no proto
		fprintf(fl, "Material: %d\n", GET_OBJ_MATERIAL(obj));
	}
	if (!proto && GET_OBJ_COMPONENT(obj) != NOTHING) {
		fprintf(fl, "Component: %d\n", GET_OBJ_COMPONENT(obj));
	}
	if (!proto || GET_OBJ_CURRENT_SCALE_LEVEL(obj) != GET_OBJ_CURRENT_SCALE_LEVEL(proto)) {
		fprintf(fl, "Current-scale: %d\n", GET_OBJ_CURRENT_SCALE_LEVEL(obj));
	}
	if (!proto && GET_OBJ_MIN_SCALE_LEVEL(obj) > 0) {
		fprintf(fl, "Min-scale: %d\n", GET_OBJ_MIN_SCALE_LEVEL(obj));
	}
	if (!proto && GET_OBJ_MAX_SCALE_LEVEL(obj) > 0) {
		fprintf(fl, "Max-scale: %d\n", GET_OBJ_MAX_SCALE_LEVEL(obj));
	}
	if (!proto && GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
		fprintf(fl, "Quest: %d\n", GET_OBJ_REQUIRES_QUEST(obj));
	}
	if (!proto && GET_OBJ_TOOL_FLAGS(obj) != NOBITS) {
		fprintf(fl, "Tool: %s\n", bitv_to_alpha(GET_OBJ_TOOL_FLAGS(obj)));
	}
	if (!proto && GET_OBJ_REQUIRES_TOOL(obj) != NOBITS) {
		fprintf(fl, "Requires-tool: %s\n", bitv_to_alpha(GET_OBJ_REQUIRES_TOOL(obj)));
	}

	if (obj->last_empire_id != NOTHING) {
		fprintf(fl, "Last-empire: %d\n", obj->last_empire_id);
	}
	if (obj->last_owner_id != NOBODY) {
		fprintf(fl, "Last-owner: %d\n", obj->last_owner_id);
	}
	if (IS_STOLEN(obj)) {
		fprintf(fl, "Stolen-from: %d\n", obj->stolen_from);
		fprintf(fl, "Stolen-timer: %d\n", (int) obj->stolen_timer);
	}
	if (GET_AUTOSTORE_TIMER(obj) > 0) {
		fprintf(fl, "Autostore-timer: %d\n", (int) GET_AUTOSTORE_TIMER(obj));
	}
	
	// always save applies
	for (apply = GET_OBJ_APPLIES(obj); apply; apply = apply->next) {
		fprintf(fl, "Applies: %d %d %d\n", apply->location, apply->modifier, apply->apply_type);
	}
	
	// who it's bound to
	for (bind = OBJ_BOUND_TO(obj); bind; bind = bind->next) {
		fprintf(fl, "Bound-to: %d\n", bind->idnum);
	}
	
	// any equipment sets it's in
	LL_FOREACH(GET_OBJ_EQ_SETS(obj), eq_set) {
		fprintf(fl, "Eq-set: %d %d\n", eq_set->id, eq_set->pos);
	}
	
	// scripts
	if (SCRIPT(obj)) {
		trig_data *trig;
		
		for (trig = TRIGGERS(SCRIPT(obj)); trig; trig = trig->next) {
			fprintf(fl, "Trigger: %d\n", GET_TRIG_VNUM(trig));
		}
		
		LL_FOREACH (SCRIPT(obj)->global_vars, tvd) {
			if (*tvd->name == '-' || !*tvd->value) { // don't save if it begins with - or is empty
				continue;
			}
			
			fprintf(fl, "Variable: %s %ld\n%s\n", tvd->name, tvd->context, tvd->value);
		}
	}
	
	fprintf(fl, "End\n");
}


/**
* Removes all of a character's items before they leave the game (otherwise,
* extracting the character would drop the items on the ground).
*
* @param char_data *ch The player whose inventory/equipment must go.
*/
void extract_all_items(char_data *ch) {
	int iter;
	
	// it's no longer safe to save the delay file -- saving it after extracting items results in losing all items
	if (!IS_NPC(ch)) {
		DONT_SAVE_DELAY(ch) = TRUE;
	}
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter)) {
			Crash_extract_objs(GET_EQ(ch, iter));
		}
	}
	
	Crash_extract_objs(ch->carrying);
}


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER OBJECT LOADING ///////////////////////////////////////////////////

/**
* This code seeks to autoequip the character with an item from the crash file,
* rather than loading all items into inventory like a stock CircleMUD.
*
* The 'location' parameter is a WEAR_ flag + 1 (the +1 is because 0 is
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


/**
* Applies a loaded object to a character, during login/player-read.
*
* @param obj_data *obj The object.
* @param char_data *ch The person to give/equip it to.
* @param int location Where it should be equipped.
* @param obj_data ***cont_row For putting items back in containers (must be at least MAX_BAG_ROWS long).
*/
void loaded_obj_to_char(obj_data *obj, char_data *ch, int location, obj_data ***cont_row) {
	obj_data *o, *next_o;
	int iter;
	
	if (!obj || !ch) {
		return;
	}
	
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
			DL_FOREACH_SAFE2((*cont_row)[iter], o, next_o, next_content) {
				// No container, back to inventory.
				DL_DELETE2((*cont_row)[iter], o, prev_content, next_content);
				obj_to_char(o, ch);
			}
		}
		if ((*cont_row)[0]) {	/* Content list existing. */
			if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER || GET_OBJ_TYPE(obj) == ITEM_CART || IS_CORPSE(obj)) {
				/* Remove object, fill it, equip again. */
				obj = unequip_char(ch, location - 1);
				obj->contains = NULL;	/* Should be NULL anyway, but just in case. */
				DL_FOREACH_SAFE2((*cont_row)[0], o, next_o, next_content) {
					DL_DELETE2((*cont_row)[iter], o, prev_content, next_content);
					obj_to_obj(o, obj);
				}
				equip_char(ch, obj, location - 1);
			}
			else {			/* Object isn't container, empty the list. */
				DL_FOREACH_SAFE2((*cont_row)[0], o, next_o, next_content) {
					DL_DELETE2((*cont_row)[0], o, prev_content, next_content);
					obj_to_char(o, ch);
				}
			}
		}
	}
	else {	/* location <= 0 */
		for (iter = MAX_BAG_ROWS - 1; iter > -location; --iter) {
			// No container, back to inventory.
			DL_FOREACH_SAFE2((*cont_row)[iter], o, next_o, next_content) {
				DL_DELETE2((*cont_row)[iter], o, prev_content, next_content);
				obj_to_char(o, ch);
			}
		}
		if (iter == -location && (*cont_row)[iter]) {	/* Content list exists. */
			if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER || GET_OBJ_TYPE(obj) == ITEM_CART || IS_CORPSE(obj)) {
				/* Take the item, fill it, and give it back. */
				obj_from_char(obj);
				obj->contains = NULL;
				DL_FOREACH_SAFE2((*cont_row)[iter], o, next_o, next_content) {
					DL_DELETE2((*cont_row)[iter], o, prev_content, next_content);
					obj_to_obj(o, obj);
				}
				obj_to_char(obj, ch);	/* Add to inventory first. */
			}
			else {	/* Object isn't container, empty content list. */
				DL_FOREACH_SAFE2((*cont_row)[iter], o, next_o, next_content) {
					DL_DELETE2((*cont_row)[iter], o, prev_content, next_content);
					obj_to_char(o, ch);
				}
			}
		}
		if (location < 0 && location >= -MAX_BAG_ROWS) {
			/*
			 * Let the object be part of the content list but put it at the
			 * list's end.  Thus having the items in the same order as before
			 * the character rented.
			 */
			obj_from_char(obj);
			DL_APPEND2((*cont_row)[-location - 1], obj, prev_content, next_content);
		}
	}
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
* @return bool TRUE if it saved a pack, FALSE if it did not.
*/
bool objpack_save_room(room_data *room) {
	char filename[MAX_INPUT_LENGTH], tempname[MAX_INPUT_LENGTH];
	FILE *fp;
	
	if (!room) {
		return FALSE;	// shortcut out
	}
	
	get_world_filename(filename, GET_ROOM_VNUM(room), SUF_PACK);
	
	if (!ROOM_NEEDS_PACK_SAVE(room)) {
		if (IS_SET(room->save_info, SAVE_INFO_PACK_SAVED)) {
			REMOVE_BIT(room->save_info, SAVE_INFO_PACK_SAVED);
			request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
		}
		
		if (!block_all_saves_due_to_shutdown && access(filename, F_OK) == 0) {
			unlink(filename);
		}
		
		return FALSE;
	}
	
	// otherwise save the pack file
	
	// temp file
	strcpy(tempname, filename);
	strcat(tempname, TEMP_SUFFIX);

	if (!(fp = fopen(tempname, "w"))) {
		return FALSE;
	}
	
	Crash_save(ROOM_CONTENTS(room), fp, LOC_INVENTORY);
	Crash_save_vehicles(ROOM_VEHICLES(room), fp);

	fprintf(fp, "$\n");

	fclose(fp);
	rename(tempname, filename);
	
	if (!IS_SET(room->save_info, SAVE_INFO_PACK_SAVED)) {
		SET_BIT(room->save_info, SAVE_INFO_PACK_SAVED);
		request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	}

	return TRUE;
}


/**
* Loads an object pack for a room. This is done at startup.
*
* @param room_data *room The room.
* @param bool use_pre_b5_116_dir If TRUE, loads from the old 'packs' directory
*/
void objpack_load_room(room_data *room, bool use_pre_b5_116_dir) {
	obj_data *obj, *o, *next_o, *cont_row[MAX_BAG_ROWS];
	char fname[MAX_STRING_LENGTH], line[MAX_INPUT_LENGTH], err_str[256];
	int iter, location;
	vehicle_data *veh;
	obj_vnum vnum;
	time_t timer;
	FILE *fl;

	// empty container lists
	for (iter = 0; iter < MAX_BAG_ROWS; iter++) {
		cont_row[iter] = NULL;
	}
	
	// determine file location based on version
	if (use_pre_b5_116_dir) {
		// only for backwards-compatibility
		sprintf(fname, "%s%d%s", LIB_OBJPACK, GET_ROOM_VNUM(room), SUF_PACK);
	}
	else {
		get_world_filename(fname, GET_ROOM_VNUM(room), SUF_PACK);
	}

	if (!(fl = fopen(fname, "r"))) {
		if (errno == ENOENT) {
			log("SYSERR: Unable to find expected pack file: %s", fname);
		}
		else {
			log("SYSERR: READING PACK FILE %s (5): %s", fname, strerror(errno));
		}
		return;
	}
	
	sprintf(err_str, "vehicle in pack file for room %d", GET_ROOM_VNUM(room));
	
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
			
			if ((obj = Obj_load_from_file(fl, vnum, &location, NULL, err_str))) {
				// Obj_load_from_file may return a NULL for deleted objs
				suspend_autostore_updates = TRUE;	// see end of block for FALSE
				
				// Not really an inventory, but same idea.
				if (location > 0) {
					location = LOC_INVENTORY;
				}

				// store autostore timer through obj_to_room
				timer = GET_AUTOSTORE_TIMER(obj);
				obj_to_room(obj, room);
				schedule_obj_autostore_check(obj, timer);
				
				for (iter = MAX_BAG_ROWS - 1; iter > -location; --iter) {
					// No container, back to room.
					DL_FOREACH_SAFE2(cont_row[iter], o, next_o, next_content) {
						DL_DELETE2(cont_row[iter], o, prev_content, next_content);
						timer = GET_AUTOSTORE_TIMER(o);
						obj_to_room(o, room);
						schedule_obj_autostore_check(o, timer);
					}
				}
				if (iter == -location && cont_row[iter]) {			/* Content list exists. */
					if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER || GET_OBJ_TYPE(obj) == ITEM_CART || IS_CORPSE(obj)) {
						/* Take the item, fill it, and give it back. */
						obj_from_room(obj);
						obj->contains = NULL;
						DL_FOREACH_SAFE2(cont_row[iter], o, next_o, next_content) {
							DL_DELETE2(cont_row[iter], o, prev_content, next_content);
							obj_to_obj(o, obj);
						}
						timer = GET_AUTOSTORE_TIMER(obj);
						obj_to_room(obj, room);			/* Add to room first. */
						schedule_obj_autostore_check(obj, timer);
					}
					else {				/* Object isn't container, empty content list. */
						DL_FOREACH_SAFE2(cont_row[iter], o, next_o, next_content) {
							DL_DELETE2(cont_row[iter], o, prev_content, next_content);
							timer = GET_AUTOSTORE_TIMER(o);
							obj_to_room(o, room);
							schedule_obj_autostore_check(o, timer);
						}
					}
				}
				if (location < 0 && location >= -MAX_BAG_ROWS) {
					obj_from_room(obj);
					DL_APPEND2(cont_row[-location - 1], obj, prev_content, next_content);
				}
				
				suspend_autostore_updates = FALSE;
			}
			else {
				// no obj returned -- it was probably deleted
				suspend_autostore_updates = TRUE;
				
				// item is missing here -- attempt to dump stuff back to the room if pending
				for (iter = MAX_BAG_ROWS - 1; iter >= 0; --iter) {
					DL_FOREACH_SAFE2(cont_row[iter], o, next_o, next_content) {
						DL_DELETE2(cont_row[iter], o, prev_content, next_content);
						timer = GET_AUTOSTORE_TIMER(o);
						obj_to_room(o, room);
						schedule_obj_autostore_check(o, timer);
					}
				}
				
				suspend_autostore_updates = FALSE;
				
				/*
				log("SYSERR: Got back non-object while loading vnum %d for %s", vnum, fname);
				fclose(fl);
				return;
				*/
			}
		}
		else if (*line == '%') {	// vehicle entry
			if (sscanf(line, "%%%d", &vnum) < 1) {
				log("SYSERR: Format error in vehicle vnum line of pack file %s", fname);
				fclose(fl);
				return;
			}
			
			if ((veh = unstore_vehicle_from_file(fl, vnum, err_str))) {
				vehicle_to_room(veh, room);
			}
		}
		else if (!strn_cmp(line, "Rent-time:", 10)) {
			// no longer used (may still be in file)
		}
		else if (!strn_cmp(line, "Rent-code:", 10)) {
			// no longer used (may still be in file)
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
* This function lists a given character's inventory/gear information to the
* player who requested it. This is mainly used by "show rent".
*
* @param char_data *ch The person who's looking up the information.
* @param char *name The name of the person whose rent file to show.
*/
void Crash_listrent(char_data *ch, char *name) {
	char_data *victim;
	bool file = FALSE;
	int iter;
	
	if (!(victim = find_or_load_player(name, &file))) {
		msg_to_char(ch, "Unable to find player %s.\r\n", name);
		return;
	}
	
	check_delayed_load(victim);
	
	msg_to_char(ch, "%s is using:\r\n", GET_NAME(victim));
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(victim, iter)) {
			msg_to_char(ch, "%s%s", wear_data[iter].eq_prompt, obj_desc_for_char(GET_EQ(victim, iter), ch, OBJ_DESC_EQUIPMENT));
		}
	}
	
	msg_to_char(ch, "Inventory:\r\n");
	list_obj_to_char(victim->carrying, ch, OBJ_DESC_INVENTORY, TRUE);
	
	if (file) {
		free_char(victim);
	}
}
