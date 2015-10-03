/* ************************************************************************
*   File: books.c                                         EmpireMUD 2.0b2 *
*  Usage: data and functions for libraries and books                      *
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
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Data
*   Author Tracking
*   Core Functions
*   Library
*   Reading
*/

// local protos


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

struct book_data *book_table = NULL;	// hash table
struct author_data *author_table = NULL;	// hash table of authors


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

// parse 1 book file
void parse_book(FILE *fl, book_vnum vnum) {
	void add_book_to_table(struct book_data *book);

	struct book_data *book;
	struct library_data *libr, *libr_ctr;
	struct paragraph_data *para, *para_ctr;
	
	room_vnum room;
	int lvar[2];
	char line[256], astr[256];
	
	sprintf(buf2, "book #%d", vnum);
	
	CREATE(book, struct book_data, 1);
	book->vnum = vnum;
	add_book_to_table(book);
		
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
		book->author = lvar[0];
		add_book_author(lvar[0]);
		
		book->bits |= asciiflag_conv(astr);
	}

	// several strings in a row
	if ((book->title = fread_string(fl, buf2)) == NULL) {
		book->title = str_dup("Untitled");
	}
	
	if ((book->byline = fread_string(fl, buf2)) == NULL) {
		book->byline = str_dup("Anonymous");
	}
	
	if ((book->item_name = fread_string(fl, buf2)) == NULL ) {
		book->item_name = str_dup("a book");
	}
	
	if ((book->item_description = fread_string(fl, buf2)) == NULL) {
		book->item_description = str_dup("It appears to be a book.\r\n");
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
				para->next = NULL;
				
				if ((para->text = fread_string(fl, buf2)) != NULL) {
					if (book->paragraphs) {
						// append to end
						para_ctr = book->paragraphs;
						while (para_ctr->next) {
							para_ctr = para_ctr->next;
						}
						
						para_ctr->next = para;
					}
					else {
						// empty list
						book->paragraphs = para;
					}
				}
				else {
					// oops
					free(para);
				}
				break;
			}
			case 'L': {	// library
				room = atoi(line+2);
				if (real_room(room)) {
					CREATE(libr, struct library_data, 1);
					libr->next = NULL;
					libr->location = room;

					if (book->in_libraries) {
						// append to end
						libr_ctr = book->in_libraries;
						while (libr_ctr->next) {
							libr_ctr = libr_ctr->next;
						}
					
						libr_ctr->next = libr;
					}
					else {
						// empty list
						book->in_libraries = libr;
					}
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
	struct library_data *libr;
	struct book_data *book, *next_book;
	FILE *fl;
	
	sprintf(filename, "%s%d%s", BOOK_PREFIX, idnum, BOOK_SUFFIX);
	strcpy(tempfile, filename);
	strcat(tempfile, TEMP_SUFFIX);
	
	if (!(fl = fopen(tempfile, "w"))) {
		log("SYSERR: unable to write file %s", tempfile);
		exit(1);
	}

	HASH_ITER(hh, book_table, book, next_book) {
		if (book->author == idnum) {
			strcpy(temp, NULLSAFE(book->item_description));
			strip_crlf(temp);
		
			fprintf(fl, "#%d\n"
						"%d %u\n"
						"%s~\n"
						"%s~\n"
						"%s~\n"
						"%s~\n",
						book->vnum, book->author, book->bits, book->title, book->byline, book->item_name, temp);
			
			// 'P': paragraph
			for (para = book->paragraphs; para; para = para->next) {
				strcpy(temp, NULLSAFE(para->text));
				strip_crlf(temp);
				fprintf(fl, "P\n%s~\n", temp);
			}
			
			// 'L': library
			for (libr = book->in_libraries; libr; libr = libr->next) {
				fprintf(fl, "L %d\n", libr->location);
			}
			
			fprintf(fl, "S\n");
		}
	}
	
	fprintf(fl, "$~\n");
	fclose(fl);
	rename(tempfile, filename);
}


/**
* saves the author index file, e.g. when a new author is added
*/
void save_author_index(void) {
	char filename[MAX_STRING_LENGTH], tempfile[MAX_STRING_LENGTH];
	struct author_data *author, *next_author;
	FILE *fl;
	
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
* @param book_vnum vnum The book to find.
* @return struct book_data* the book data, or NULL
*/
struct book_data *find_book_by_vnum(book_vnum vnum) {
	struct book_data *book;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(book_table, &vnum, book);
	return book;
}


/**
* @param char *argument user input
* @param room_data *room the location of the library
* @return struct book_data* either the book in this library, or NULL
*/
struct book_data *find_book_in_library(char *argument, room_data *room) {
	struct book_data *book, *next_book, *by_name = NULL, *found = NULL;
	struct library_data *libr;
	int num = NOTHING;
	
	if (is_number(argument)) {
		num = atoi(argument);
	}
	
	// find by number or name
	HASH_ITER(hh, book_table, book, next_book) {
		for (libr = book->in_libraries; libr && !found; libr = libr->next) {
			if (libr->location == GET_ROOM_VNUM(room)) {
				if (num != NOTHING && --num == 0) {
					// found by number!
					found = book;
				}
				else if (is_abbrev(argument, book->title)) {
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
* @return struct book_data* either the book, or NULL
*/
struct book_data *find_book_by_author(char *argument, int idnum) {
	struct book_data *book, *next_book, *by_name = NULL, *found = NULL;
	int num = NOTHING;
	
	if (is_number(argument)) {
		num = atoi(argument);
	}
	
	// find by number or name
	HASH_ITER(hh, book_table, book, next_book) {
		if (book->author == idnum) {
			if (num != NOTHING && --num == 0) {
				// found by number!
				found = book;
				break;
			}
			else if (is_abbrev(argument, book->title)) {
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


/**
* Finds a free book vnum.
*
* @return book_vnum A new book vnum.
*/
book_vnum find_new_book_vnum(void) {
	struct book_data *book, *next_book;
	descriptor_data *desc;
	int max_id = 0;
	
	// check descriptors
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (GET_OLC_BOOK(desc)) {
			max_id = MAX(max_id, GET_OLC_BOOK(desc)->vnum);
		}
	}
	
	// check existing books
	HASH_ITER(hh, book_table, book, next_book) {
		max_id = MAX(max_id, book->vnum);
	}
	
	return max_id + 1;
}


 //////////////////////////////////////////////////////////////////////////////
//// LIBRARY /////////////////////////////////////////////////////////////////

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
	struct book_data *book, *next_book;
	struct library_data *libr;
	int count = 0;

	if (ch->desc) {
		sprintf(buf, "Books shelved here:\r\n");
	
		HASH_ITER(hh, book_table, book, next_book) {
			for (libr = book->in_libraries; libr; libr = libr->next) {
				if (libr->location == GET_ROOM_VNUM(IN_ROOM(ch))) {
					sprintf(buf + strlen(buf), "%d. %s (%s)\r\n", ++count, book->title, book->byline);
				}
			}
		}
	
		if (count == 0) {
			strcat(buf, "  none\r\n");
		}
		
		page_string(ch->desc, buf, TRUE);
	}	
	else {
		// no desc?
	}
}


LIBRARY_SCMD(library_checkout) {
	obj_data *obj;
	struct book_data *book;
	
	skip_spaces(&argument);
	
	if (!*argument) {
		msg_to_char(ch, "Check out which book? (Use 'library browse' to see a list.)\r\n");
	}
	else if (!(book = find_book_in_library(argument, IN_ROOM(ch)))) {
		msg_to_char(ch, "No such book is shelved here.\r\n");
	}
	else {
		obj = read_object(o_BOOK, TRUE);
		GET_OBJ_VAL(obj, VAL_BOOK_ID) = book->vnum;
		obj_to_char_or_room(obj, ch);
		
		msg_to_char(ch, "You find a copy of %s on the shelf and take it.\r\n", book->title);
		act("$n picks up $p off a shelf.", TRUE, ch, obj, NULL, TO_ROOM);
		load_otrigger(obj);
	}
}


// actually shelves a book -- 99% of sanity checks done before this
int perform_shelve(char_data *ch, obj_data *obj) {
	struct book_data *book;
	struct library_data *libr, *libr_ctr;
	bool found = FALSE;
	
	if (!IS_BOOK(obj) || !(book = find_book_by_vnum(GET_BOOK_ID(obj)))) {
		act("You can't shelve $p!", FALSE, ch, obj, NULL, TO_CHAR);
		return 0;
	}
	else {
		for (libr = book->in_libraries; libr && !found; libr = libr->next) {
			if (libr->location == GET_ROOM_VNUM(IN_ROOM(ch))) {
				found = TRUE;
			}
		}
		
		if (!found) {
			// not already in the library
			CREATE(libr, struct library_data, 1);
			libr->next = NULL;
			libr->location = GET_ROOM_VNUM(IN_ROOM(ch));

			if (book->in_libraries) {
				// append to end
				libr_ctr = book->in_libraries;
				while (libr_ctr->next) {
					libr_ctr = libr_ctr->next;
				}
			
				libr_ctr->next = libr;
			}
			else {
				// empty list
				book->in_libraries = libr;
			}
			
			// save new locations
			save_author_books(book->author);
		}
	
		act("You shelve $p.", FALSE, ch, obj, NULL, TO_CHAR);
		act("$n shelves $p.", TRUE, ch, obj, NULL, TO_ROOM);
		extract_obj(obj);
		
		return 1;
	}
}


LIBRARY_SCMD(library_shelve) {
	obj_data *obj, *next_obj;
	int dotmode, amount = 0, multi;
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
		else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
			msg_to_char(ch, "You don't seem to have any %ss.\r\n", arg);
		}
		else {
			do {
				next_obj = get_obj_in_list_vis(ch, arg, obj->next_content);
				amount += perform_shelve(ch, obj);
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
				for (obj = ch->carrying; obj; obj = next_obj) {
					next_obj = obj->next_content;
					if (!OBJ_FLAGGED(obj, OBJ_KEEP) && IS_BOOK(obj)) {
						amount += perform_shelve(ch, obj);
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
			if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
				msg_to_char(ch, "You don't seem to have any %ss.\r\n", arg);
			}
			while (obj) {
				next_obj = get_obj_in_list_vis(ch, arg, obj->next_content);
				if (!OBJ_FLAGGED(obj, OBJ_KEEP)) {
					amount += perform_shelve(ch, obj);
				}
				obj = next_obj;
			}
		}
		else {
			if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
				msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
			}
			else {
				amount += perform_shelve(ch, obj);
			}
		}
	}	
}


LIBRARY_SCMD(library_burn) {
	struct book_data *book;
	struct library_data *libr, *next_libr, *temp;
	
	skip_spaces(&argument);
	
	if (!IS_IMMORTAL(ch) && !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You can't burn books here.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Burn which book? (Use 'library browse' to see a list.)\r\n");
	}
	else if (!(book = find_book_in_library(argument, IN_ROOM(ch)))) {
		msg_to_char(ch, "No such book is shelved here.\r\n");
	}
	else {
		for (libr = book->in_libraries; libr; libr = next_libr) {
			next_libr = libr->next;
			
			if (libr->location == GET_ROOM_VNUM(IN_ROOM(ch))) {
				REMOVE_FROM_LIST(libr, book->in_libraries, next);
				free(libr);
			}
		}

		save_author_books(book->author);
		
		msg_to_char(ch, "You take all the copies of %s from the shelves and burn them behind the library!\r\n", book->title);
		sprintf(buf, "$n takes all the copies of %s from the shelves and burn them behind the library!", book->title);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
	}
}



#define LIBR_REQ_LIBRARY  BIT(0)	// must be in library
#define LIBR_REQ_EDITOR  BIT(1)	// only with an open book editor


struct {
	int subcmd;
	char *name;
	int level;
	bitvector_t flags;
	void (*func)(char_data *ch, char *argument);
} library_command[] = {
	{ SCMD_LIBRARY, "browse", 0, LIBR_REQ_LIBRARY, library_browse },
	{ SCMD_LIBRARY, "checkout", 0, LIBR_REQ_LIBRARY, library_checkout },
	{ SCMD_LIBRARY, "shelve", LIBR_REQ_LIBRARY, LVL_APPROVED, library_shelve },
	{ SCMD_LIBRARY, "burn", LIBR_REQ_LIBRARY, LVL_APPROVED, library_burn },
	
	{ SCMD_BOOKEDIT, "list", LVL_APPROVED, LIBR_REQ_LIBRARY, bookedit_list },
	{ SCMD_BOOKEDIT, "copy", LVL_APPROVED, LIBR_REQ_LIBRARY, bookedit_copy },
	{ SCMD_BOOKEDIT, "delete", LVL_APPROVED, LIBR_REQ_LIBRARY, bookedit_delete },
	{ SCMD_BOOKEDIT, "write", LVL_APPROVED, LIBR_REQ_LIBRARY, bookedit_write },
	
	{ SCMD_BOOKEDIT, "abort", LVL_APPROVED, LIBR_REQ_EDITOR, bookedit_abort },
	{ SCMD_BOOKEDIT, "author", LVL_APPROVED, LIBR_REQ_EDITOR, bookedit_author },
	{ SCMD_BOOKEDIT, "byline", LVL_APPROVED, LIBR_REQ_EDITOR, bookedit_byline },
	{ SCMD_BOOKEDIT, "description", LVL_APPROVED, LIBR_REQ_EDITOR, bookedit_item_description },
	{ SCMD_BOOKEDIT, "item", LVL_APPROVED, LIBR_REQ_EDITOR, bookedit_item_name },
	{ SCMD_BOOKEDIT, "license", LVL_APPROVED, LIBR_REQ_EDITOR, bookedit_license },
	{ SCMD_BOOKEDIT, "paragraphs", LVL_APPROVED, LIBR_REQ_EDITOR, bookedit_paragraphs },
	{ SCMD_BOOKEDIT, "save", LVL_APPROVED, LIBR_REQ_EDITOR, bookedit_save },
	{ SCMD_BOOKEDIT, "title", LVL_APPROVED, LIBR_REQ_EDITOR, bookedit_title },

	// last!
	{ SCMD_LIBRARY, "\n", 0, NOBITS, NULL }
};


// this is both "library" and "bookedit"
ACMD(do_library) {
	void olc_show_book(char_data *ch);
	
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
			if (library_command[iter].level <= GET_ACCESS_LEVEL(ch) && library_command[iter].subcmd == subcmd && (!IS_SET(library_command[iter].flags, LIBR_REQ_EDITOR) || GET_OLC_BOOK(ch->desc))) {
				msg_to_char(ch, "%s%s", comma ? ", " : " ", library_command[iter].name);
				comma = TRUE;
			}
		}
		send_to_char("\r\n", ch);
	}
	else {
		// find command
		for (iter = 0; pos == NOTHING && *(library_command[iter].name) != '\n'; ++iter) {
			if (library_command[iter].level <= GET_ACCESS_LEVEL(ch) && library_command[iter].subcmd == subcmd && is_abbrev(arg, library_command[iter].name)) {
				pos = iter;
			}
		}
		
		if (pos == NOTHING) {
			msg_to_char(ch, "Invalid %s command.\r\n", types[subcmd]);
		}
		else if (IS_SET(library_command[pos].flags, LIBR_REQ_LIBRARY) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_LIBRARY)) {
			msg_to_char(ch, "You must be inside a library to do this.\r\n");
		}
		else if (IS_SET(library_command[pos].flags, LIBR_REQ_LIBRARY) && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
			msg_to_char(ch, "You don't have permission to use this library.\r\n");
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
	struct book_data *book;

	if (!IS_BOOK(obj)) {
		msg_to_char(ch, "You can't read that!\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're too busy right now.\r\n");
	}
	else if (!(book = find_book_by_vnum(GET_BOOK_ID(obj)))) {
		msg_to_char(ch, "The book is old and badly damaged; you can't read it.\r\n");
	}
	else {
		start_action(ch, ACT_READING, 0);
		GET_ACTION_VNUM(ch, 0) = GET_BOOK_ID(obj);
		
		msg_to_char(ch, "You start to read '%s', by %s.\r\n", book->title, book->byline);
		act("$n begins to read $p.", TRUE, ch, obj, NULL, TO_ROOM);
	}
}


// action ticks for reading
void process_reading(char_data *ch) {
	obj_data *obj;
	bool found = FALSE;
	struct book_data *book;
	struct paragraph_data *para;
	int pos;
	
	for (obj = ch->carrying; obj && !found; obj = obj->next_content) {
		if (IS_BOOK(obj) && GET_BOOK_ID(obj) == GET_ACTION_VNUM(ch, 0)) {
			found = TRUE;
		}
	}
	
	if (!found) {
		msg_to_char(ch, "You seem to have lost your book.\r\n");
		GET_ACTION(ch) = ACT_NONE;
	}
	else if (!(book = find_book_by_vnum(GET_ACTION_VNUM(ch, 0)))) {
		msg_to_char(ch, "The book is too badly damaged to read.\r\n");
		GET_ACTION(ch) = ACT_NONE;
	}
	else {
		GET_ACTION_TIMER(ch) += 1;

		if ((GET_ACTION_TIMER(ch) % 4) != 0) {
			// no message to show on this tick
			return;
		}
	
		pos = (GET_ACTION_TIMER(ch) / 4);

		found = FALSE;
		for (para = book->paragraphs; para && !found; para = para->next) {
			if (--pos == 0) {
				found = TRUE;
				msg_to_char(ch, "You read:\r\n%s", para->text);
			}
		}
		
		if (!found) {
			// book's done
			msg_to_char(ch, "You close the book and put it away.\r\n");
			GET_ACTION(ch) = ACT_NONE;
		}
	}
}
