/* ************************************************************************
*   File: bookedit.c                                      EmpireMUD 2.0b5 *
*  Usage: the book editor functionality                                   *
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
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "vnums.h"
#include "dg_scripts.h"
#include "olc.h"

/**
* Contents:
*   Helpers
*   Displays
*   Edit Modules
*   Bookedit Commands
*/


const char *bookedit_license_display =
"You are using EmpireMUD's book editor under these terms:\r\n"
"- All text you write here must be original and written by you. You may not\r\n"
"  enter copyrighted text or any text written by another person. You assume\r\n"
"  full liability for any text you enter.\r\n"
"- All text you write is the property of you, the author.\r\n"
"- You grant EmpireMUD a license to use this text free-of-charge for the purpose\r\n"
"  of displaying this text in the EmpireMUD game.\r\n"
"- EmpireMUD will never use this text for any other purpose.\r\n";

const char *default_book_title = "Untitled";
const char *default_book_item = "a book";
const char *default_book_desc = "It appears to be a book.\r\n";


// local protos
void remove_book_from_table(book_data *book);

// external consts
extern const char *book_name_list[];

// external funcs
void add_book_author(int idnum);
void save_author_books(int idnum);
void save_author_index();


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Puts a book in the hash table.
*
* @param book_data *book The book to add to the table.
*/
void add_book_to_table(book_data *book) {
	extern int sort_book_table(book_data *a, book_data *b);
	
	book_data *find;
	book_vnum vnum;
	
	if (book) {
		vnum = book->vnum;
		HASH_FIND_INT(book_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(book_table, vnum, book);
			HASH_SORT(book_table, sort_book_table);
		}
	}
}


/**
* Make an obj copy of a book.
*
* @param book_data *book The book data.
* @return obj_data* The object copy.
*/
obj_data *create_book_obj(book_data *book) {
	char buf[MAX_STRING_LENGTH];
	obj_data *obj;
	
	obj = create_obj();
	
	GET_OBJ_SHORT_DESC(obj) = str_dup(NULLSAFE(book->item_name));
	snprintf(buf, sizeof(buf), "book %s", skip_filler(NULLSAFE(book->item_name)));
	GET_OBJ_KEYWORDS(obj) = str_dup(buf);
	snprintf(buf, sizeof(buf), "Someone has left %s here.", NULLSAFE(book->item_name));
	GET_OBJ_LONG_DESC(obj) = str_dup(buf);
	GET_OBJ_ACTION_DESC(obj) = str_dup(NULLSAFE(book->item_description));
	
	GET_OBJ_TYPE(obj) = ITEM_BOOK;
	GET_OBJ_WEAR(obj) = ITEM_WEAR_TAKE;
	GET_OBJ_TIMER(obj) = UNLIMITED;
	GET_OBJ_VAL(obj, VAL_BOOK_ID) = book->vnum;
	
	return obj;
}


/**
* Creates a new book table entry.
* 
* @param book_vnum vnum The number to create.
* @return book_data* The new book.
*/
book_data *create_book_table_entry(book_vnum vnum, int author) {
	book_data *book;
	
	// sanity
	if (book_proto(vnum)) {
		log("SYSERR: Attempting to insert book at existing vnum %d", vnum);
		return book_proto(vnum);
	}
	
	CREATE(book, book_data, 1);
	book->vnum = vnum;
	add_book_to_table(book);
	
	// update authors
	book->author = author;
	add_book_author(author);
	
	// save now
	save_author_books(author);
	save_author_index();
	
	return book;
}


/**
* frees up memory for a book
*
* See also: olc_delete_book
*
* @param book_data *book The book to free.
*/
void free_book(book_data *book) {
	book_data *proto = book_proto(book->vnum);
	struct paragraph_data *para;
	
	if (book->title && (!proto || book->title != proto->title)) {
		free(book->title);
	}
	if (book->byline && (!proto || book->byline != proto->byline)) {
		free(book->byline);
	}
	if (book->item_name && (!proto || book->item_name != proto->item_name)) {
		free(book->item_name);
	}
	if (book->item_description && (!proto || book->item_description != proto->item_description)) {
		free(book->item_description);
	}
	
	if (book->paragraphs && (!proto || book->paragraphs != proto->paragraphs)) {
		while ((para = book->paragraphs)) {
			book->paragraphs = para->next;
			if (para->text) {
				free(para->text);
			}
			free(para);
		}
	}
	
	free(book);
}


/**
* For the .list command.
*
* @param book_data *book The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_book(book_data *book, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s\t0 (%s\t0)", book->vnum, book->title, book->byline);
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s\t0", book->vnum, book->title);
	}
	
	return output;
}


/**
* WARNING: This function actually deletes a book. This can be called without
* the 'ch' parameter to delete a book without logging or messaging, e.g. a
* delete by the author.
*
* @param char_data *ch The person doing the deleting -- this is optional, for OLC edits.
* @param book_vnum vnum The vnum to delete.
*/
void olc_delete_book(char_data *ch, book_vnum vnum) {
	book_data *book;
	int author;
	
	if (!(book = book_proto(vnum))) {
		msg_to_char(ch, "There is no such book %d.\r\n", vnum);
		return;
	}
		
	// pull it from the hash FIRST
	remove_book_from_table(book);
	
	// save without it
	author = book->author;
	save_author_books(author);

	if (ch) {	
		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted book %d", GET_NAME(ch), vnum);
		msg_to_char(ch, "Book %d deleted.\r\n", vnum);
	}	
	
	free_book(book);
}


// processes timed action for bookedit_copy
void process_copying_book(char_data *ch) {
	book_data *book;
	obj_data *obj;
	
	GET_ACTION_TIMER(ch) -= 1;
	
	if (GET_ACTION_TIMER(ch) <= 0) {
		GET_ACTION(ch) = ACT_NONE;
		
		if (!(book = book_proto(GET_ACTION_VNUM(ch, 0)))) {
			msg_to_char(ch, "You finish copying the book, but there was an error and it's illegible.\r\n");
		}
		else {
			obj = create_book_obj(book);
			obj_to_char(obj, ch);
			
			msg_to_char(ch, "You finish a fresh copy of %s!\r\n", book->title);
			act("$n finishes copying out $p.", TRUE, ch, obj, NULL, TO_ROOM);
			load_otrigger(obj);
		}
	}
	else if (GET_ACTION_TIMER(ch) == 6 && !PRF_FLAGGED(ch, PRF_NOSPAM)) {
		msg_to_char(ch, "You're halfway through copying out the book...\r\n");
	}
}


/**
* Removes a book from the hash table.
*
* @param book_data *book The book to remove from the table.
*/
void remove_book_from_table(book_data *book) {
	HASH_DEL(book_table, book);
}


/**
* Function to save a player's changes to a book (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_book(descriptor_data *desc) {
	book_data *proto, *book = GET_OLC_BOOK(desc);
	book_vnum vnum = GET_OLC_VNUM(desc);
	struct paragraph_data *para;
	struct library_data *libr;
	UT_hash_handle hh;
	int author_change = -1;
	
	// have a place to save it?
	if (!(proto = book_proto(vnum))) {
		proto = create_book_table_entry(vnum, book->author);
	}
	
	// free prototype strings and pointers
	if (proto->title) {
		free(proto->title);
	}
	if (proto->byline) {
		free(proto->byline);
	}
	if (proto->item_name) {
		free(proto->item_name);
	}
	if (proto->item_description) {
		free(proto->item_description);
	}
	while ((para = proto->paragraphs)) {
		if (para->text) {
			free(para->text);
		}
		proto->paragraphs = para->next;
		free(para);
	}
	
	// basic sanitation:
	if (!book->title) {
		book->title = str_dup(default_book_title);
	}
	if (!book->byline) {
		book->byline = str_dup("Unknown");
	}
	if (!book->item_name) {
		book->item_name = str_dup(default_book_item);
	}
	if (!book->item_description) {
		book->item_description = str_dup(default_book_desc);
	}
	for (para = book->paragraphs; para; para = para->next) {
		if (!para->text) {
			para->text = str_dup("Empty paragraph.\r\n");
		}
	}
	
	// may need to save multiple authors
	if (book->author != proto->author) {
		author_change = proto->author;
		add_book_author(book->author);
	}
	
	// save library data and move over
	libr = proto->in_libraries;
	proto->in_libraries = NULL;
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *book;	// copy data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore hash handle
	
	// restore libraries
	proto->in_libraries = libr;
	
	// and save to file
	save_author_books(book->author);
	
	// did we change author ids?
	if (author_change != -1) {
		save_author_index();
		save_author_books(author_change);
	}
}


/**
* Creates a copy of a book, or clears a new one, for editing.
* 
* @param book_data *input The book to copy, or NULL to make a new one.
* @return book_data* The copied book.
*/
book_data *setup_olc_book(book_data *input) {
	struct paragraph_data *para, *copy, *last;
	book_data *new;
	
	CREATE(new, book_data, 1);
	
	if (input) {
		// copy normal data
		*new = *input;
		
		new->title = input->title ? str_dup(input->title) : NULL;
		new->byline = input->byline ? str_dup(input->byline) : NULL;
		new->item_name = input->item_name ? str_dup(input->item_name) : NULL;
		new->item_description = input->item_description ? str_dup(input->item_description) : NULL;
		
		// don't need this
		new->in_libraries = NULL;
		
		last = NULL;
		new->paragraphs = NULL;
		for (para = input->paragraphs; para; para = para->next) {
			CREATE(copy, struct paragraph_data, 1);
			copy->text = str_dup(para->text);
		
			if (last) {
				last->next = copy;
			}
			else {
				new->paragraphs = copy;
			}
			last = copy;
		}
	}
	else {
		// new!
		new->vnum = NOTHING;
		new->title = str_dup(default_book_title);
		new->byline = NULL;	// will set this soon
		new->item_name = str_dup(default_book_item);
		new->item_description = str_dup(default_book_desc);
	}
		
	// done
	return new;	
}


/**
* Simple sorter for the book hash
*
* @param book_data *a One element
* @param book_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_book_table(book_data *a, book_data *b) {
	return a->vnum - b->vnum;
}


 ///////////////////////////////////////////////////////////////////////////////
//// DISPLAYS /////////////////////////////////////////////////////////////////

/**
* This is the main display for the book editor OLC. It displays the user's
* currently-edited book.
*
* @param char_data *ch The person who is editing a book and will see its display.
*/
void olc_show_book(char_data *ch) {
	book_data *book = GET_OLC_BOOK(ch->desc);
	char buf[MAX_STRING_LENGTH];
	struct paragraph_data *para;
	bool imm = IS_IMMORTAL(ch);
	player_index_data *index;
	int count;
	
	if (!book) {
		return;
	}
	
	*buf = '\0';

	if (imm) {
		sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !book_proto(book->vnum) ? "new book" : book_proto(book->vnum)->title);
	}
	else {
		sprintf(buf + strlen(buf), "\tcEmpireMUD Book Editor: %s\t0\r\n", !book_proto(book->vnum) ? "new book" : book_proto(book->vnum)->title);
	}
	
	sprintf(buf + strlen(buf), "<%stitle\t0> %s\r\n", OLC_LABEL_STR(book->title, default_book_title), NULLSAFE(book->title));
	sprintf(buf + strlen(buf), "<%sbyline\t0> %s\r\n", OLC_LABEL_STR(book->byline, ""), NULLSAFE(book->byline));
	sprintf(buf + strlen(buf), "<%sitem\t0> %s\r\n", OLC_LABEL_STR(book->item_name, default_book_item), NULLSAFE(book->item_name));
	sprintf(buf + strlen(buf), "<%sdescription\t0>\r\n%s", OLC_LABEL_STR(book->item_description, default_book_desc), NULLSAFE(book->item_description));
	
	count = 0;
	for (para = book->paragraphs; para; para = para->next) {
		++count;
	}
	sprintf(buf + strlen(buf), "<%sparagraphs\t0> %d (list, edit, new, delete)\r\n", OLC_LABEL_PTR(book->paragraphs), count);
	
	if (imm) {
		sprintf(buf + strlen(buf), "<%sauthor\t0> %s\r\n", OLC_LABEL_VAL(book->author, 0), (book->author != 0 && (index = find_player_index_by_idnum(book->author))) ? index->fullname : "nobody");
	}
	else {
		sprintf(buf + strlen(buf), "<%slicense\t0>, <%ssave\t0>, <%sabort\t0>\r\n", OLC_LABEL_UNCHANGED, OLC_LABEL_UNCHANGED, OLC_LABEL_UNCHANGED);
	}
	
	page_string(ch->desc, buf, TRUE);
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(booked_author) {
	book_data *book = GET_OLC_BOOK(ch->desc);
	player_index_data *index = NULL;
	int id;
	
	if (!IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You can't change the book's author id.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Set the author to who (idnum, name, or nobody)?\r\n");
	}
	else if (!str_cmp(argument, "nobody") || !str_cmp(argument, "none")) {
		book->author = 0;
		msg_to_char(ch, "You set the book's author to nobody.\r\n");
	}
	else if (is_number(argument) && (id = atoi(argument)) >= 0) {
		if (id != 0 && !(index = find_player_index_by_idnum(id))) {
			msg_to_char(ch, "No such player id.\r\n");
		}
		else {
			book->author = id;
			msg_to_char(ch, "You set the book's author id to %d (%s).\r\n", id, (id == 0 || !index) ? "nobody" : index->fullname);
		}
	}
	else if (!(index = find_player_index_by_name(argument))) {
		msg_to_char(ch, "Unable to find character '%s'.\r\n", argument);
	}
	else {
		book->author = index->idnum;
		msg_to_char(ch, "You set the book's author id to %s (%d).\r\n", (index->idnum <= 0 || !index->fullname) ? "nobody" : index->fullname, index->idnum);
	}
}


OLC_MODULE(booked_byline) {
	book_data *book = GET_OLC_BOOK(ch->desc);

	if (color_strlen(argument) > MAX_BOOK_BYLINE) {
		msg_to_char(ch, "Book bylines may not be more than %d characters long.\r\n", MAX_BOOK_TITLE);
	}
	else {
		olc_process_string(ch, argument, "byline", &(book->byline));
	}
}


OLC_MODULE(booked_item_description) {
	book_data *book = GET_OLC_BOOK(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		start_string_editor(ch->desc, "book item description", &(book->item_description), MAX_ITEM_DESCRIPTION, TRUE);
	}
}


OLC_MODULE(booked_item_name) {
	book_data *book = GET_OLC_BOOK(ch->desc);

	if (color_code_length(argument) > 0) {
		msg_to_char(ch, "Book item names may not contain color codes.\r\n");
	}
	else if (strchrstr(argument, "%()[]\\")) {
		msg_to_char(ch, "Book item names may not contain the following characters: %%()[]\\\r\n");
	}
	else if (!IS_IMMORTAL(ch) && strlen(argument) > MAX_BOOK_ITEM_NAME) {
		msg_to_char(ch, "Book item names may not be more than %d characters long.\r\n", MAX_BOOK_ITEM_NAME);
	}
	else if (!IS_IMMORTAL(ch) && !has_keyword(argument, book_name_list, TRUE)) {
		msg_to_char(ch, "Book item names must contain a word like 'book' or 'tome'. See HELP BOOKEDIT ITEM.\r\n");
	}
	else {
		olc_process_string(ch, argument, "item name", &(book->item_name));
	}
}


OLC_MODULE(booked_license) {
	msg_to_char(ch, "%s", bookedit_license_display);
}


OLC_MODULE(booked_paragraphs) {
	char arg1[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH];
	book_data *book = GET_OLC_BOOK(ch->desc);
	struct paragraph_data *para, *new, *last, *temp;
	size_t size;
	
	argument = any_one_arg(argument, arg1);
	skip_spaces(&argument);
	
	if (is_abbrev(arg1, "list")) {
		char arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
		int count, from = -1, to = -1;
		
		if (!book->paragraphs) {
			msg_to_char(ch, "You have not added any paragraphs.\r\n");
			return;
		}
		
		half_chop(argument, arg2, arg3);	// optional "from" "to"
		if (*arg2) {
			from = atoi(arg2) ? atoi(arg2) : -1;
		}
		if (*arg3) {
			to = atoi(arg3) ? atoi(arg3) : -1;
		}
		
		*buf = '\0';
		size = 0;
		
		for (para = book->paragraphs, count = 1; para; para = para->next, ++count) {
			if ((from == -1 || from <= count) && (to == -1 || to >= count)) {
				snprintf(line, sizeof(line), "\r\n\tcParagraph %d\t0\r\n%s", count, NULLSAFE(para->text));
				
				if (size + strlen(line) < sizeof(buf)) {
					size += snprintf(buf + size, sizeof(buf) - size, "%s", line);
				}
				else {
					// too long!
					size += snprintf(buf + size, sizeof(buf) - size, "\r\n...string too long!\r\n");
					break;
				}
			}
		}
		
		page_string(ch->desc, buf, TRUE);
	}
	else if (is_abbrev(arg1, "edit")) {
		bool found = FALSE;
		int num;
		
		if (ch->desc->str) {
			msg_to_char(ch, "You are already editing a string.\r\n");
		}
		else if (!*argument) {
			msg_to_char(ch, "Edit which paragraph number?\r\n");
		}
		else if ((num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid paragraph number.\r\n");
		}
		else {
			for (para = book->paragraphs; !found && para; para = para->next) {
				if (--num == 0) {
					found = TRUE;
					msg_to_char(ch, "NOTE: Each paragraph should be no more than 4 or 5 lines, formatted with /fi\r\n");
					start_string_editor(ch->desc, "paragraph", &(para->text), MAX_BOOK_PARAGRAPH, FALSE);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid paragraph number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "new") || is_abbrev(arg1, "add")) {
		bool found = FALSE;
		int pos = -1;
		
		if (ch->desc->str) {
			msg_to_char(ch, "You are already editing a string.\r\n");
			return;
		}
		
		// optional position
		if (*argument) {
			if ((pos = atoi(argument)) <= 0) {
				msg_to_char(ch, "Invalid paragraph position.\r\n");
				return;
			}
		}
		
		CREATE(new, struct paragraph_data, 1);
		
		if (pos == 1) {
			new->next = book->paragraphs;
			book->paragraphs = new;
			found = TRUE;
		}
		else {
			// find the correct insert position or possibly the end of the list
			for (para = book->paragraphs, last = para; !found && para; para = para->next) {
				if (pos != -1 && --pos == 0) {
					new->next = last->next;
					last->next = new;
					found = TRUE;
				}
			
				last = para;
			}
		
			if (!found) {
				if (last) {
					last->next = new;
				}
				else {
					book->paragraphs = new;
				}
				found = TRUE;
			}
		}
		
		if (found) {
			msg_to_char(ch, "NOTE: Each paragraph should be no more than 4 or 5 lines, formatted with /fi\r\n");
			start_string_editor(ch->desc, "new paragraph", &(new->text), MAX_BOOK_PARAGRAPH, FALSE);
		}
		else {
			msg_to_char(ch, "Invalid paragraph position.\r\n");
		}
	}
	else if (is_abbrev(arg1, "delete") || is_abbrev(arg1, "remove")) {
		bool found = FALSE;
		int pos;
		
		if (!*argument) {
			msg_to_char(ch, "Usage: .paragraphs delete <number>\r\n");
		}
		else if ((pos = atoi(argument)) <= 0) {
			msg_to_char(ch, "Invalid paragraph position.\r\n");
		}
		else {
			for (para = book->paragraphs; !found && para; para = para->next) {
				if (--pos == 0) {
					REMOVE_FROM_LIST(para, book->paragraphs, next);
					if (para->text) {
						free(para->text);
					}
					free(para);
				
					found = TRUE;
				}
			}

			if (found) {
				msg_to_char(ch, "Deleted paragraph %d.\r\n", atoi(argument));
			}
			else {
				msg_to_char(ch, "No such paragraph to delete.\r\n");
			}
		}
	}
	else {
		msg_to_char(ch, "Usage: .paragraphs <list | edit | new | delete> [number]\r\n");
	}
}


OLC_MODULE(booked_title) {
	book_data *book = GET_OLC_BOOK(ch->desc);
	
	if (color_strlen(argument) > MAX_BOOK_TITLE) {
		msg_to_char(ch, "Book titles may not be more than %d characters long.\r\n", MAX_BOOK_TITLE);
	}
	else {
		olc_process_string(ch, argument, "title", &(book->title));
	}
}


 ///////////////////////////////////////////////////////////////////////////////
//// BOOKEDIT COMMANDS ////////////////////////////////////////////////////////

LIBRARY_SCMD(bookedit_abort) {
	if (GET_OLC_TYPE(ch->desc) != OLC_BOOK || GET_OLC_VNUM(ch->desc) == NOTHING) {
		msg_to_char(ch, "You aren't editing a book.\r\n");
	}
	else if (ch->desc->str) {
		msg_to_char(ch, "Close your text editor (\ty,/h\t0) before aborting the book editor.\r\n");
	}
	else {
		free_book(GET_OLC_BOOK(ch->desc));
		GET_OLC_BOOK(ch->desc) = NULL;
		GET_OLC_TYPE(ch->desc) = 0;
		GET_OLC_VNUM(ch->desc) = NOTHING;
		
		msg_to_char(ch, "You abort your changes and close the book editor.\r\n");
	}
}


LIBRARY_SCMD(bookedit_author) {
	booked_author(ch, OLC_BOOK, argument);
}


LIBRARY_SCMD(bookedit_byline) {
	booked_byline(ch, OLC_BOOK, argument);
}


LIBRARY_SCMD(bookedit_copy) {
	book_data *book;
	
	skip_spaces(&argument);
	
	if (!*argument) {
		msg_to_char(ch, "Write a copy of which book? (Use 'bookedit list' to see a list.)\r\n");
	}
	else if (!(book = find_book_by_author(argument, GET_IDNUM(ch)))) {
		msg_to_char(ch, "No such book.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're a bit busy already.\r\n");
	}
	else {
		start_action(ch, ACT_COPYING_BOOK, 12);
		GET_ACTION_VNUM(ch, 0) = book->vnum;
		
		msg_to_char(ch, "You pick up a blank book and begin to copy out '%s'.\r\n", book->title);
		act("$n begins to write out a copy of a book.", TRUE, ch, NULL, NULL, TO_ROOM);
		
		// rapido!
		if (IS_IMMORTAL(ch)) {
			GET_ACTION_TIMER(ch) = 0;
			process_copying_book(ch);
		}
	}
}


LIBRARY_SCMD(bookedit_delete) {
	book_data *book = NULL;
	descriptor_data *desc;
	bool found;
	
	skip_spaces(&argument);
	
	if (!*argument) {
		msg_to_char(ch, "Usage: bookedit delete <book title | book number>\r\nSee 'bookedit list' for a list.\r\n");
		return;
	}
	if (!(book = find_book_by_author(argument, GET_IDNUM(ch)))) {
		msg_to_char(ch, "No such book.\r\n");
		return;
	}
	if (GET_OLC_BOOK(ch->desc) && GET_OLC_VNUM(ch->desc) == book->vnum) {
		msg_to_char(ch, "You can't delete that book while you're editing it.\r\n");
		return;
	}
	
	// make sure nobody else is editing it
	found = FALSE;
	for (desc = descriptor_list; desc && !found; desc = desc->next) {
		if (GET_OLC_TYPE(desc) == OLC_BOOK && GET_OLC_VNUM(desc) == book->vnum) {
			found = TRUE;
		}
	}
	
	// this should really never happen, but it could
	if (found) {
		msg_to_char(ch, "Someone else is currently editing that book.\r\n");
		return;
	}
	
	// success!
	msg_to_char(ch, "You delete the book %s.\r\n", book->title);
	olc_delete_book(NULL, book->vnum);	// don't pass ch -- prevents OLC messages
}


LIBRARY_SCMD(bookedit_item_description) {
	booked_item_description(ch, OLC_BOOK, argument);
}


LIBRARY_SCMD(bookedit_item_name) {
	booked_item_name(ch, OLC_BOOK, argument);
}


LIBRARY_SCMD(bookedit_license) {
	booked_license(ch, OLC_BOOK, argument);
}


LIBRARY_SCMD(bookedit_list) {
	book_data *book, *next_book;
	char buf[MAX_STRING_LENGTH], buf1[MAX_STRING_LENGTH];
	int count = 0;
	size_t size;

	size = snprintf(buf, sizeof(buf), "Books you have written:\r\n");
	
	HASH_ITER(hh, book_table, book, next_book) {
		if (book->author != GET_IDNUM(ch)) {
			continue;
		}
		
		if (IS_IMMORTAL(ch)) {
			sprintf(buf1, "[%5d] ", book->vnum);
		}
		else {
			*buf1 = '\0';
		}
		size += snprintf(buf + size, sizeof(buf) - size, "%s%d. %s\t0 (%s\t0)\r\n", buf1, ++count, book->title, book->byline);
	}

	if (count == 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "  none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


LIBRARY_SCMD(bookedit_paragraphs) {
	booked_paragraphs(ch, OLC_BOOK, argument);
}


LIBRARY_SCMD(bookedit_save) {
	if (GET_OLC_TYPE(ch->desc) != OLC_BOOK || GET_OLC_VNUM(ch->desc) == NOTHING) {
		msg_to_char(ch, "You aren't editing a book.\r\n");
	}
	else if (ch->desc->str) {
		msg_to_char(ch, "Close your text editor (\ty,/h\t0) before saving the book.\r\n");
	}
	else {
		save_olc_book(ch->desc);
		free_book(GET_OLC_BOOK(ch->desc));
		GET_OLC_BOOK(ch->desc) = NULL;
		GET_OLC_TYPE(ch->desc) = 0;
		GET_OLC_VNUM(ch->desc) = NOTHING;
		
		msg_to_char(ch, "You save the book and close the editor.\r\n");
	}
}


LIBRARY_SCMD(bookedit_title) {
	booked_title(ch, OLC_BOOK, argument);
}


LIBRARY_SCMD(bookedit_write) {
	extern book_vnum top_book_vnum;

	book_data *book = NULL;
	descriptor_data *desc;
	bool found;
	
	skip_spaces(&argument);
	
	if (!*argument) {
		msg_to_char(ch, "Usage: bookedit write <new | book title | book number>\r\nSee 'bookedit list' for a list.\r\n");
		return;
	}
	if (GET_OLC_TYPE(ch->desc)) {
		msg_to_char(ch, "You are already editing something.\r\n");
		return;
	}

	// find target (if not "new")
	if (str_cmp(argument, "new") && !(book = find_book_by_author(argument, GET_IDNUM(ch)))) {
		msg_to_char(ch, "No such book.\r\n");
		return;
	}
	
	// make sure nobody else is already editing it
	if (book) {
		found = FALSE;
		for (desc = descriptor_list; desc && !found; desc = desc->next) {
			if (GET_OLC_TYPE(desc) == OLC_BOOK && GET_OLC_VNUM(desc) == book->vnum) {
				found = TRUE;
			}
		}
		if (found) {
			msg_to_char(ch, "Someone else is already editing that book.\r\n");
			return;
		}
	}

	// begin editor
	act("$n begins working on a book.", TRUE, ch, 0, 0, TO_ROOM);
	
	// this copies an existing book or creates a new one
	GET_OLC_BOOK(ch->desc) = setup_olc_book(book);
	
	// was a new book?
	if (!book) {
		GET_OLC_BOOK(ch->desc)->vnum = ++top_book_vnum;
		GET_OLC_BOOK(ch->desc)->byline = str_dup(PERS(ch, ch, TRUE));
		GET_OLC_BOOK(ch->desc)->author = GET_IDNUM(ch);
	}
	
	// ready!
	GET_OLC_VNUM(ch->desc) = GET_OLC_BOOK(ch->desc)->vnum;
	GET_OLC_TYPE(ch->desc) = OLC_BOOK;

	msg_to_char(ch, "You begin writing a book:\r\n");
	olc_show_book(ch);
}
