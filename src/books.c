/* ************************************************************************
*   File: books.c                                         EmpireMUD 2.0b5 *
*  Usage: data and functions for libraries and books                      *
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
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Data
*   Author Tracking
*   Core Functions
*   Library Functions
*   Library Commands
*   Reading
*/


// local prototypes
void add_book_to_library(any_vnum vnum, struct library_info *library);
struct library_book *find_book_in_library(any_vnum vnum, struct library_info *library);


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

book_data *book_table = NULL;	// hash table
struct author_data *author_table = NULL;	// hash table of authors
book_vnum top_book_vnum = 0;	// need a persistent top vnum because re-using a book vnum causes funny issues with obj copies of the book

struct library_info *library_table = NULL;	// global hash (by room vnum) of libraries
bool book_library_file_needs_save = FALSE;		// hint to save the library file

// from bookedit.c
extern const char *default_book_title;


 //////////////////////////////////////////////////////////////////////////////
//// AUTHOR TRACKING /////////////////////////////////////////////////////////

/**
* @param struct author_data *a One element
* @param struct author_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int author_sort(struct author_data *a, struct author_data *b) {
	return a->idnum - b->idnum;
}


/**
* Marks that an author has at least one book, in author_table.
*
* @param int idnum The author's player idnum.
*/
void add_book_author(int idnum) {
	struct author_data *auth;
	
	HASH_FIND_INT(author_table, &idnum, auth);
	if (!auth) {
		CREATE(auth, struct author_data, 1);
		auth->idnum = idnum;
		HASH_ADD_INT(author_table, idnum, auth);
		HASH_SORT(author_table, author_sort);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CORE FUNCTIONS //////////////////////////////////////////////////////////

/**
* @param book_vnum vnum The book to find.
* @return book_data* the book data, or NULL
*/
book_data *book_proto(book_vnum vnum) {
	book_data *book;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(book_table, &vnum, book);
	return book;
}


/**
* Dumps all the books stored here into the room as items. (They remain in the
* library list, too, unless you also call delete_library() on the room.
*
* @param room_data *room The room to dump books in, from its own library list.
*/
void dump_library_to_room(room_data *room) {
	obj_data *obj;
	struct library_book *book, *next;
	struct library_info *library;
	
	if ((library = find_library(GET_ROOM_VNUM(room), FALSE))) {
		HASH_ITER(hh, library->books, book, next) {
			obj = create_book_obj(book_proto(book->vnum));
			obj_to_room(obj, room);
			
			if (load_otrigger(obj) && ROOM_PEOPLE(room)) {
				act("$p falls to the ground.", FALSE, ROOM_PEOPLE(room), obj, NULL, TO_CHAR | TO_ROOM);
			}
		}
	}
}


// parse 1 book file
void parse_book(FILE *fl, book_vnum vnum) {
	book_data *book;
	struct library_info *library;
	struct paragraph_data *para;
	
	room_vnum room;
	int lvar[2];
	char line[256], astr[256];
	
	sprintf(buf2, "book #%d", vnum);
	
	CREATE(book, book_data, 1);
	BOOK_VNUM(book) = vnum;
	add_book_to_table(book);
	
	top_book_vnum = MAX(top_book_vnum, vnum);
	
	// author
	if (!get_line(fl, line)) {
		log("SYSERR: Expecting author id for %s but file ended", buf2);
		exit(1);
	}
	else if (sscanf(line, "%d %s", lvar, astr) != 2) {
		log("SYSERR: Format error in numeric line of %s", buf2);
		exit(1);
	}
	else {
		BOOK_AUTHOR(book) = lvar[0];
		add_book_author(lvar[0]);
		
		BOOK_FLAGS(book) |= asciiflag_conv(astr);
	}

	// several strings in a row
	if ((BOOK_TITLE(book) = fread_string(fl, buf2)) == NULL) {
		BOOK_TITLE(book) = str_dup("Untitled");
	}
	
	if ((BOOK_BYLINE(book) = fread_string(fl, buf2)) == NULL) {
		BOOK_BYLINE(book) = str_dup("Anonymous");
	}
	
	if ((BOOK_ITEM_NAME(book) = fread_string(fl, buf2)) == NULL ) {
		BOOK_ITEM_NAME(book) = str_dup("a book");
	}
	
	if ((BOOK_ITEM_DESC(book) = fread_string(fl, buf2)) == NULL) {
		BOOK_ITEM_DESC(book) = str_dup("It appears to be a book.\r\n");
	}
			
	// extra data
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s (expecting P/L/S)", buf2);
			exit(1);
		}
		switch (*line) {
			case 'P': {	// paragraph
				CREATE(para, struct paragraph_data, 1);
				
				if ((para->text = fread_string(fl, buf2)) != NULL) {
				    LL_APPEND(BOOK_PARAGRAPHS(book), para);
				}
				else {
					// oops
					free(para);
				}
				break;
			}
			case 'L': {	// library -- OLD data deprecated in b5.174 (still read in for backwards-compatibility)
				room = atoi(line+2);
				if (real_room(room) && (library = find_library(room, TRUE))) {
					add_book_to_library(BOOK_VNUM(book), library);
				}
				break;
			}

			case 'S': {	// end!
				return;
			}
			default: {
				log("SYSERR: Format error in %s (expecting P/L/S)", buf2);
				exit(1);
			}
		}
	}
}


/**
* Save all books for one author to file.
*
* @param int idnum the author's idnum
*/
void save_author_books(int idnum) {
	char filename[MAX_STRING_LENGTH], tempfile[MAX_STRING_LENGTH], temp[MAX_STRING_LENGTH];
	struct paragraph_data *para;
	book_data *book, *next_book;
	FILE *fl;
	
	if (block_all_saves_due_to_shutdown) {
		return;
	}
	
	sprintf(filename, "%s%d%s", BOOK_PREFIX, idnum, BOOK_SUFFIX);
	strcpy(tempfile, filename);
	strcat(tempfile, TEMP_SUFFIX);
	
	if (!(fl = fopen(tempfile, "w"))) {
		log("SYSERR: unable to write file %s", tempfile);
		exit(1);
	}

	HASH_ITER(hh, book_table, book, next_book) {
		if (BOOK_AUTHOR(book) == idnum) {
			strcpy(temp, NULLSAFE(BOOK_ITEM_DESC(book)));
			strip_crlf(temp);
		
			fprintf(fl, "#%d\n"
						"%d %s\n"
						"%s~\n"
						"%s~\n"
						"%s~\n"
						"%s~\n",
						BOOK_VNUM(book), BOOK_AUTHOR(book), bitv_to_alpha(BOOK_FLAGS(book)), BOOK_TITLE(book), BOOK_BYLINE(book), BOOK_ITEM_NAME(book), temp);
			
			// 'P': paragraph
			LL_FOREACH(BOOK_PARAGRAPHS(book), para) {
				strcpy(temp, NULLSAFE(para->text));
				strip_crlf(temp);
				fprintf(fl, "P\n%s~\n", temp);
			}
			
			// 'L': library
			// Deprecated as of b5.174: libraries now have their own file
			
			fprintf(fl, "S\n");
		}
	}
	
	fprintf(fl, "$~\n");
	fclose(fl);
	rename(tempfile, filename);
}


/**
* Picks a book at random from the list of books that either:
* - are by author 0, or
* - are not in any libraries
*
* @return book_data* The book entry, if any, or NULL if not.
*/
book_data *random_lost_book(void) {
	book_data *book, *next_book, *found = NULL;
	int count = 0;
	
	HASH_ITER(hh, book_table, book, next_book) {
		if (BOOK_AUTHOR(book) != 0 && find_book_in_any_library(BOOK_VNUM(book))) {
			continue;	// not lost
		}
		if (!BOOK_PARAGRAPHS(book)) {
			continue;	// skip empty book
		}
		if (!BOOK_TITLE(book) || !strcmp(BOOK_TITLE(book), default_book_title)) {
			continue;	// skip default title
		}
		
		// ok pick at random if we got here
		if (!number(0, count++)) {
			found = book;
		}
	}
	
	return found;	// if any
}


/**
* saves the author index file, e.g. when a new author is added
*/
void save_author_index(void) {
	char filename[MAX_STRING_LENGTH], tempfile[MAX_STRING_LENGTH];
	struct author_data *author, *next_author;
	FILE *fl;
	
	if (block_all_saves_due_to_shutdown) {
		return;
	}
	
	sprintf(filename, "%s%s", BOOK_PREFIX, INDEX_FILE);
	strcpy(tempfile, filename);
	strcat(tempfile, TEMP_SUFFIX);
	
	if (!(fl = fopen(tempfile, "w"))) {
		log("SYSERR: unable to write file %s", tempfile);
		exit(1);
	}
	
	HASH_ITER(hh, author_table, author, next_author) {
		fprintf(fl, "%d%s\n", author->idnum, BOOK_SUFFIX);
	}
	
	fprintf(fl, "$~\n");
	fclose(fl);
	rename(tempfile, filename);
}


/**
* @param char *argument user input
* @param room_data *room the location of the library
* @return book_data* either the book in this library, or NULL
*/
book_data *find_book_in_library_by_name(char *argument, room_data *room) {
	int num = NOTHING;
	book_data *book, *found = NULL, *by_name = NULL;
	struct library_book *libbook, *next;
	struct library_info *library;
	
	if (is_number(argument)) {
		num = atoi(argument);
	}
	
	// find by number or name
	if ((library = find_library(GET_ROOM_VNUM(room), FALSE))) {
		HASH_ITER(hh, library->books, libbook, next) {
			if ((book = book_proto(libbook->vnum))) {
				if (num != NOTHING && --num == 0) {
					// found by number!
					found = book;
					break;	// only needed 1
				}
				else if (is_abbrev(argument, BOOK_TITLE(book))) {
					// title match
					if (num == NOTHING) {
						found = book;
						break;	// perfect match -- done
					}
					else if (by_name == NULL) {
						by_name = book;
					}
				}
			}
		}
	}
	
	// did we find one by-name when looking for one by number?
	if (!found && by_name) {
		found = by_name;
	}
	
	return found;
}


/**
* @param char *argument user input
* @param int idnum an author's idnum
* @return book_data* either the book, or NULL
*/
book_data *find_book_by_author(char *argument, int idnum) {
	book_data *book, *next_book, *by_name = NULL, *found = NULL;
	int num = NOTHING;
	
	if (is_number(argument)) {
		num = atoi(argument);
	}
	
	// find by number or name
	HASH_ITER(hh, book_table, book, next_book) {
		if (BOOK_AUTHOR(book) == idnum) {
			if (num != NOTHING && --num == 0) {
				// found by number!
				found = book;
				break;
			}
			else if (is_abbrev(argument, BOOK_TITLE(book))) {
				// title match
				if (num == NOTHING) {
					found = book;
					break;
				}
				else if (by_name == NULL) {
					by_name = book;
				}
			}
		}
	}
	
	// did we find one by-name when looking for one by number?
	if (!found && by_name) {
		found = by_name;
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// LIBRARY FUNCTIONS ///////////////////////////////////////////////////////

/**
* Adds a book to a library, by vnum, if it's not already there.
*
* @param any_vnum vnum The book vnum to add.
* @param struct library_info *library The library to shelve it in.
*/
void add_book_to_library(any_vnum vnum, struct library_info *library) {
	struct library_book *book;
	
	if (vnum == NOTHING || !library || find_book_in_library(vnum, library)) {
		return;	// shortcut
	}
	
	CREATE(book, struct library_book, 1);
	book->vnum = vnum;
	HASH_ADD_INT(library->books, vnum, book);
	book_library_file_needs_save = TRUE;
}


/**
* Deletes the library entry for a room, if it exists.
*
* @param room_data *room The library's location.
*/
void delete_library(room_data *room) {
	struct library_info *library;
	
	if (room && (library = find_library(GET_ROOM_VNUM(room), FALSE))) {
		HASH_DEL(library_table, library);
		free_library_info(library);
		book_library_file_needs_save = TRUE;
	}
}


/**
* Finds the entry for a book in a given library, by vnum, if it exists.
*
* @param any_vnum vnum The book vnum to find.
* @param struct library_info *library The library to find it in.
* @return struct library_book* The book's entry in that library, if it exists, or NULL if not.
*/
struct library_book *find_book_in_library(any_vnum vnum, struct library_info *library) {
	struct library_book *book;
	
	if (vnum == NOTHING || !library) {
		return NULL;	// shortcut
	}
	
	HASH_FIND_INT(library->books, &vnum, book);
	return book;	// if any
}


/**
* Determines if a book (by vnum) is in any libraries.
*
* @param any_vnum vnum Which book vnum.
* @return bool TRUE if the book is in any libraries; FALSE if not.
*/
bool find_book_in_any_library(any_vnum vnum) {
	struct library_info *library, *next;
	
	HASH_ITER(hh, library_table, library, next) {
		if (find_book_in_library(vnum, library)) {
			// only takes 1
			return TRUE;
		}
	}
	
	// nope
	return FALSE;
}


/**
* Finds the library data for a given room location. Can also create it, if
* desired.
*
* @param room_vnum location Which room has the library (should have the LIBRARY function flag).
* @param bool add_if_missing If TRUE, creates the entry if none exists.
* @return struct library_info* The library data for that location, if any. Guaranteed if add_if_missing was TRUE.
*/
struct library_info *find_library(room_vnum location, bool add_if_missing) {
	struct library_info *library;
	
	HASH_FIND_INT(library_table, &location, library);
	
	if (!library && add_if_missing) {
		CREATE(library, struct library_info, 1);
		library->room = location;
		
		HASH_ADD_INT(library_table, room, library);
		book_library_file_needs_save = TRUE;
	}
	
	return library;
}


/**
* Frees a hash of library books.
*
* @param struct library_book *hash The hash of books to free.
*/
void free_library_books(struct library_book *hash) {
	struct library_book *iter, *next;
	HASH_ITER(hh, hash, iter, next) {
		HASH_DEL(hash, iter);
		free(iter);
	}
}


/**
* Frees a library_info and its data (warning: does not remove from the
* library_table first).
*
* @param struct library_info *library The library info to free.
*/
void free_library_info(struct library_info *library) {
	free_library_books(library->books);
	free(library);
}


/**
* Loads the library file (at startup), if it exists.
*/
void load_book_library_file(void) {
	char line[MAX_STRING_LENGTH];
	int room, book;
	FILE *fl;
	struct library_info *library;
	
	// open library file? not fatal if missing -- just no existing libraries
	if (!(fl = fopen(LIBRARY_FILE, "r"))) {
		log("No existing libraries found");
		return;
	}
	
	// load entries
	for (;;) {
		if (!get_line(fl, line)) {
			// premature EOF is unfortunate but not fatal
			log("Warning: Unexpected end if library file %s", LIBRARY_FILE);
			fclose(fl);
			return;
		}
		
		if (*line == '$') {
			// done
			break;
		}
		else if (isdigit(*line)) {
			// book entry
			if (sscanf(line, "%d %d", &room, &book) != 2) {
				log("Warning: Unable to parse line '%s' in library file %s", line, LIBRARY_FILE);
				// not fatal
				continue;
			}
			
			// add the book
			if (real_room(room) && (library = find_library(room, TRUE))) {
				add_book_to_library(book, library);
			}
		}
		else {
			log("Warning: Unexpected line '%s' in library file %s", line, LIBRARY_FILE);
			// but not fatal
			break;
		}
	}
	
	fclose(fl);
	
	// since we just loaded it, consider it correct
	book_library_file_needs_save = FALSE;
}


/**
* Removes a book from a library, by vnum, if it's in there.
*
* @param any_vnum vnum The book vnum to remove.
* @param struct library_info *library The library to remove it from.
* @return bool TRUE if it was removed, FALSE if it wasn't present.
*/
bool remove_book_from_library(any_vnum vnum, struct library_info *library) {
	struct library_book *book;
	
	if (!library || !(book = find_book_in_library(vnum, library))) {
		return FALSE;	// no work
	}
	
	HASH_DEL(library->books, book);
	free(book);
	book_library_file_needs_save = TRUE;
	return TRUE;
}


/**
* Removes a book from any library it is shelved in.
*
* @param any_vnum vnum The book vnum.
*/
void remove_book_from_all_libraries(any_vnum vnum) {
	struct library_info *library, *next;
	
	HASH_ITER(hh, library_table, library, next) {
		remove_book_from_library(vnum, library);
	}
}


/**
* Writes a fresh copy of the library file. This is triggered by setting
* 'book_library_file_needs_save' to be true, or you can call it manually.
*/
void write_book_library_file(void) {
	FILE *fl;
	struct library_book *book, *next_book;
	struct library_info *library, *next_library;
	
	if (block_all_saves_due_to_shutdown) {
		return;
	}
	
	if (!(fl = fopen(LIBRARY_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to write library file '%s'", LIBRARY_FILE TEMP_SUFFIX);
		exit(1);
	}
	
	// for each library
	HASH_ITER(hh, library_table, library, next_library) {
		if (library->room == NOWHERE) {
			continue;	// skip unexpected entries
		}
		
		// for each book in it
		HASH_ITER(hh, library->books, book, next_book) {
			if (book->vnum == NOTHING) {
				continue;	// skip invalid entries
			}
			
			// 1 entry: room vnum
			fprintf(fl, "%d %d\n", library->room, book->vnum);
		}
	}
	
	// done
	fprintf(fl, "$\n");
	fclose(fl);
	rename(LIBRARY_FILE TEMP_SUFFIX, LIBRARY_FILE);
	
	book_library_file_needs_save = FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// LIBRARY COMMANDS ////////////////////////////////////////////////////////

// externs
LIBRARY_SCMD(bookedit_abort);
LIBRARY_SCMD(bookedit_author);
LIBRARY_SCMD(bookedit_byline);
LIBRARY_SCMD(bookedit_copy);
LIBRARY_SCMD(bookedit_delete);
LIBRARY_SCMD(bookedit_item_description);
LIBRARY_SCMD(bookedit_item_name);
LIBRARY_SCMD(bookedit_license);
LIBRARY_SCMD(bookedit_list);
LIBRARY_SCMD(bookedit_paragraphs);
LIBRARY_SCMD(bookedit_save);
LIBRARY_SCMD(bookedit_title);
LIBRARY_SCMD(bookedit_write);


LIBRARY_SCMD(library_browse) {
	book_data *book;
	struct library_book *libbook, *next;
	struct library_info *library;
	int count = 0;

	if (ch->desc) {
		add_page_display(ch, "Books shelved here:");
		
		if ((library = find_library(GET_ROOM_VNUM(IN_ROOM(ch)), FALSE))) {
			HASH_ITER(hh, library->books, libbook, next) {
				if ((book = book_proto(libbook->vnum))) {
					add_page_display(ch, "%d. %s\t0 (%s\t0)", ++count, BOOK_TITLE(book), BOOK_BYLINE(book));
				}
			}
		}
	
		if (count == 0) {
			add_page_display_str(ch, "  none");
		}
		
		send_page_display(ch);
	}	
	else {
		// no desc?
	}
}


LIBRARY_SCMD(library_checkout) {
	obj_data *obj;
	book_data *book;
	
	skip_spaces(&argument);
	
	if (!*argument) {
		msg_to_char(ch, "Check out which book? (Use 'library browse' to see a list.)\r\n");
	}
	else if (!(book = find_book_in_library_by_name(argument, IN_ROOM(ch)))) {
		msg_to_char(ch, "No such book is shelved here.\r\n");
	}
	else {
		obj = create_book_obj(book);
		obj_to_char(obj, ch);
		
		msg_to_char(ch, "You find a copy of %s on the shelf and take it.\r\n", BOOK_TITLE(book));
		act("$n picks up $p off a shelf.", TRUE, ch, obj, NULL, TO_ROOM);
		if (load_otrigger(obj)) {
			get_otrigger(obj, ch, FALSE);
		}
	}
}


// actually shelves a book -- 99% of sanity checks done before this
int perform_shelve(char_data *ch, obj_data *obj) {
	book_data *book;
	
	if (!IS_BOOK(obj) || !(book = book_proto(GET_BOOK_ID(obj)))) {
		act("You can't shelve $p!", FALSE, ch, obj, NULL, TO_CHAR);
		return 0;
	}
	else {
		add_book_to_library(BOOK_VNUM(book), find_library(GET_ROOM_VNUM(IN_ROOM(ch)), TRUE));
		
		act("You shelve $p.", FALSE, ch, obj, NULL, TO_CHAR);
		act("$n shelves $p.", TRUE, ch, obj, NULL, TO_ROOM);
		extract_obj(obj);
		
		return 1;
	}
}


LIBRARY_SCMD(library_shelve) {
	obj_data *obj, *next_obj;
	int dotmode, multi;
	bool found;
	
	one_argument(argument, arg);
	
	if (ROOM_OWNER(IN_ROOM(ch)) && ROOM_OWNER(IN_ROOM(ch)) != GET_LOYALTY(ch) && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You don't have permission to shelve books here.\r\n");
		return;
	}
	if (!*arg) {
		msg_to_char(ch, "Shelve which book?\r\n");
		return;
	}

	if (is_number(arg)) {
		multi = atoi(arg);
		one_argument(argument, arg);
		if (multi <= 0) {
			send_to_char("Yeah, that makes sense.\r\n", ch);
		}
		else if (!*arg) {
			msg_to_char(ch, "What do you want to shelve %d of?\r\n", multi);
		}
		else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
			msg_to_char(ch, "You don't seem to have any %ss.\r\n", arg);
		}
		else {
			do {
				next_obj = get_obj_in_list_vis(ch, arg, NULL, obj->next_content);
				perform_shelve(ch, obj);
				obj = next_obj;
			} while (obj && --multi);
		}
	}
	else {
		dotmode = find_all_dots(arg);

		if (dotmode == FIND_ALL) {
			if (!ch->carrying) {
				send_to_char("You don't seem to be carrying anything.\r\n", ch);
			}
			else {
				DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
					if (!OBJ_FLAGGED(obj, OBJ_KEEP) && IS_BOOK(obj)) {
						perform_shelve(ch, obj);
						found = TRUE;
					}
				}
				
				if (!found) {
					msg_to_char(ch, "You have nothing to shelve.\r\n");
				}
			}
		}
		else if (dotmode == FIND_ALLDOT) {
			if (!*arg) {
				msg_to_char(ch, "What do you want to shelve all of?\r\n");
				return;
			}
			if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
				msg_to_char(ch, "You don't seem to have any %ss.\r\n", arg);
			}
			while (obj) {
				next_obj = get_obj_in_list_vis(ch, arg, NULL, obj->next_content);
				if (!OBJ_FLAGGED(obj, OBJ_KEEP)) {
					perform_shelve(ch, obj);
				}
				obj = next_obj;
			}
		}
		else {
			if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
				msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
			}
			else {
				perform_shelve(ch, obj);
			}
		}
	}	
}


LIBRARY_SCMD(library_burn) {
	book_data *book;
	struct library_info *library;
	
	skip_spaces(&argument);
	
	if (!IS_IMMORTAL(ch) && !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You can't burn books here.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Burn which book? (Use 'library browse' to see a list.)\r\n");
	}
	else if (!(book = find_book_in_library_by_name(argument, IN_ROOM(ch)))) {
		msg_to_char(ch, "No such book is shelved here.\r\n");
	}
	else {
		// actual removal
		if ((library = find_library(GET_ROOM_VNUM(IN_ROOM(ch)), FALSE))) {
			remove_book_from_library(BOOK_VNUM(book), library);
		}
		
		msg_to_char(ch, "You take all the copies of %s from the shelves and burn them behind the library!\r\n", BOOK_TITLE(book));
		sprintf(buf, "$n takes all the copies of %s from the shelves and burn them behind the library!", BOOK_TITLE(book));
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
	}
}



#define LIBR_REQ_LIBRARY  BIT(0)	// must be in library
#define LIBR_REQ_EDITOR  BIT(1)	// only with an open book editor


struct {
	int subcmd;
	char *name;
	int needs_approval;
	bitvector_t flags;
	LIBRARY_SCMD(*func);
} library_command[] = {
	{ SCMD_LIBRARY, "browse", FALSE, LIBR_REQ_LIBRARY, library_browse },
	{ SCMD_LIBRARY, "checkout", FALSE, LIBR_REQ_LIBRARY, library_checkout },
	{ SCMD_LIBRARY, "shelve", TRUE, LIBR_REQ_LIBRARY, library_shelve },
	{ SCMD_LIBRARY, "burn", TRUE, LIBR_REQ_LIBRARY, library_burn },
	
	{ SCMD_BOOKEDIT, "list", TRUE, LIBR_REQ_LIBRARY, bookedit_list },
	{ SCMD_BOOKEDIT, "copy", TRUE, LIBR_REQ_LIBRARY, bookedit_copy },
	{ SCMD_BOOKEDIT, "delete", TRUE, LIBR_REQ_LIBRARY, bookedit_delete },
	{ SCMD_BOOKEDIT, "write", TRUE, LIBR_REQ_LIBRARY, bookedit_write },
	
	{ SCMD_BOOKEDIT, "abort", TRUE, LIBR_REQ_EDITOR, bookedit_abort },
	{ SCMD_BOOKEDIT, "author", TRUE, LIBR_REQ_EDITOR, bookedit_author },
	{ SCMD_BOOKEDIT, "byline", TRUE, LIBR_REQ_EDITOR, bookedit_byline },
	{ SCMD_BOOKEDIT, "description", TRUE, LIBR_REQ_EDITOR, bookedit_item_description },
	{ SCMD_BOOKEDIT, "item", TRUE, LIBR_REQ_EDITOR, bookedit_item_name },
	{ SCMD_BOOKEDIT, "license", TRUE, LIBR_REQ_EDITOR, bookedit_license },
	{ SCMD_BOOKEDIT, "paragraphs", TRUE, LIBR_REQ_EDITOR, bookedit_paragraphs },
	{ SCMD_BOOKEDIT, "save", TRUE, LIBR_REQ_EDITOR, bookedit_save },
	{ SCMD_BOOKEDIT, "title", TRUE, LIBR_REQ_EDITOR, bookedit_title },

	// last!
	{ SCMD_LIBRARY, "\n", FALSE, NOBITS, NULL }
};


// this is both "library" and "bookedit"
ACMD(do_library) {
	char arg2[MAX_INPUT_LENGTH];
	int iter, pos = NOTHING;
	bool comma;
	
	char *types[] = { "library", "bookedit" };

	half_chop(argument, arg, arg2);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "Mobs are illiterate.\r\n");
	}
	else if (!ch->desc) {
		msg_to_char(ch, "You can't do that right now.\r\n");
	}
	else if (!*arg && GET_OLC_BOOK(ch->desc)) {
		olc_show_book(ch);
	}
	else if (!*arg) {
		msg_to_char(ch, "You can use the following %s commands:", types[subcmd]);
		comma = FALSE;
		for (iter = 0; *(library_command[iter].name) != '\n'; ++iter) {
			if (library_command[iter].subcmd != subcmd) {
				continue;
			}
			if (library_command[iter].needs_approval && !IS_APPROVED(ch) && config_get_bool("write_approval")) {
				continue;
			}
			if (IS_SET(library_command[iter].flags, LIBR_REQ_EDITOR) && !GET_OLC_BOOK(ch->desc)) {
				continue;
			}
			
			// ok
			msg_to_char(ch, "%s%s", comma ? ", " : " ", library_command[iter].name);
			comma = TRUE;
		}
		send_to_char("\r\n", ch);
	}
	else {
		// find command
		for (iter = 0; pos == NOTHING && *(library_command[iter].name) != '\n'; ++iter) {
			if (library_command[iter].subcmd != subcmd) {
				continue;
			}
			if (library_command[iter].needs_approval && !IS_APPROVED(ch) && config_get_bool("write_approval")) {
				continue;
			}
			
			// ok
			if (is_abbrev(arg, library_command[iter].name)) {
				pos = iter;
			}
		}
		
		if (pos == NOTHING) {
			msg_to_char(ch, "Invalid %s command.\r\n", types[subcmd]);
		}
		else if (IS_SET(library_command[pos].flags, LIBR_REQ_LIBRARY) && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_LIBRARY)) {
			if (HAS_FUNCTION(IN_ROOM(ch), FNC_LIBRARY) && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
				msg_to_char(ch, "You don't have permission to use this library.\r\n");
			}
			else {
				msg_to_char(ch, "You must be inside a library to do this.\r\n");
			}
		}
		else if (IS_SET(library_command[pos].flags, LIBR_REQ_LIBRARY) && !check_in_city_requirement(IN_ROOM(ch), TRUE)) {
			msg_to_char(ch, "This library must be in a city to do that.\r\n");
		}
		else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
			msg_to_char(ch, "This building must be in a city to use it.\r\n");
		}
		else if (IS_SET(library_command[pos].flags, LIBR_REQ_LIBRARY) && !IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "The library is unfinished and has no books.\r\n");
		}
		else if (IS_SET(library_command[pos].flags, LIBR_REQ_EDITOR) && !GET_OLC_BOOK(ch->desc)) {
			msg_to_char(ch, "You aren't currently editing a book.\r\n");
		}
		else {
			// pass off to specialized function
			(library_command[pos].func)(ch, arg2);
		}
	}
}



 //////////////////////////////////////////////////////////////////////////////
//// READING /////////////////////////////////////////////////////////////////

void read_book(char_data *ch, obj_data *obj) {
	book_data *book;

	if (!IS_BOOK(obj)) {
		msg_to_char(ch, "You can't read that!\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're too busy right now.\r\n");
	}
	else if (!(book = book_proto(GET_BOOK_ID(obj)))) {
		msg_to_char(ch, "The book is old and badly damaged; you can't read it.\r\n");
	}
	else if (!consume_otrigger(obj, ch, OCMD_READ, NULL)) {
		return;
	}
	else {
		start_action(ch, ACT_READING, 0);
		GET_ACTION_VNUM(ch, 0) = GET_BOOK_ID(obj);
		
		msg_to_char(ch, "You start to read '%s', by %s.\r\n", BOOK_TITLE(book), BOOK_BYLINE(book));
		act("$n begins to read $p.", TRUE, ch, obj, NULL, TO_ROOM);
		
		// ensure binding
		if (!IS_NPC(ch) && OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
			bind_obj_to_player(obj, ch);
			reduce_obj_binding(obj, ch);
		}
	}
}


// action ticks for reading
void process_reading(char_data *ch) {
	obj_data *obj, *found_obj = NULL;
	bool found = FALSE;
	book_data *book;
	struct paragraph_data *para;
	int pos;
	
	DL_FOREACH2(ch->carrying, obj, next_content) {
		if (IS_BOOK(obj) && GET_BOOK_ID(obj) == GET_ACTION_VNUM(ch, 0)) {
			found_obj = obj;	// save for later
			found = TRUE;
			break;
		}
	}
	
	if (!found) {
		msg_to_char(ch, "You seem to have lost your book.\r\n");
		end_action(ch);
	}
	else if (!(book = book_proto(GET_ACTION_VNUM(ch, 0)))) {
		msg_to_char(ch, "The book is too badly damaged to read.\r\n");
		end_action(ch);
	}
	else {
		GET_ACTION_TIMER(ch) += 1;

		if ((GET_ACTION_TIMER(ch) % 4) != 0) {
			// no message to show on this tick
			return;
		}
	
		pos = (GET_ACTION_TIMER(ch) / 4);

		found = FALSE;
		LL_FOREACH(BOOK_PARAGRAPHS(book), para) {
			if (--pos == 0) {
				msg_to_char(ch, "You read:\r\n%s", para->text);
				found = TRUE;
				break;
			}
		}
		
		if (!found) {
			// book's done -- fire finish triggers
			if (!found_obj || finish_otrigger(found_obj, ch)) {
				msg_to_char(ch, "You close the book.\r\n");
			}
			end_action(ch);
		}
	}
}
