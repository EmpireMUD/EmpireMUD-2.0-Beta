/* ************************************************************************
*   File: bookedit.c                                      EmpireMUD 2.0b1 *
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
#include "books.h"
#include "dg_scripts.h"

/**
* Contents:
*   Helpers
*   Displays
*   Bookedit Input
*   Bookedit Commands
*/


const char *bookedit_help_display =
"Bookedit main menu help:\r\n"
"- exit: leave the editor and return to the game\r\n"
"- save: save your changes\r\n"
"- license: see the license that applies to your text\r\n"
"- The yellow keywords on the menu can be used to make changes:\r\n"
"  - example: &ytitle&0 The Book of Unendiness\r\n"
"  - example: &yitem&0 a dusty, worn-out book\r\n"
"- The &ydescription&0 keyword takes you to a full text editor\r\n"
"- You can use the following paragraphs commands:\r\n"
"  - &yparagraph&0 list [start] [end]: read any/all paragraphs\r\n"
"  - &yparagraph&0 edit <#>: edit a paragraph\r\n"
"  - &yparagraph&0 new [#]: add a new paragraph at the end, or BEFORE paragraph #\r\n"
"  - &yparagraph&0 delete <#>: delete paragraph #\r\n"
"\r\n"
"Press ENTER to get back to the menu > \r\n";


const char *bookedit_license_display =
"You are using EmpireMUD's book editor under these terms:\r\n"
"- All text you write here must be original and written by you. You may not\r\n"
"  enter copyrighted text or any text written by another person. You assume\r\n"
"  full liability for any text you enter.\r\n"
"- All text you write is the property of you, the author.\r\n"
"- You grant EmpireMUD a license to use this text free-of-charge for the purpose\r\n"
"  of displaying this text in the EmpireMUD game.\r\n"
"- EmpireMUD will never use this text for any other purpose.\r\n"
"\r\n"
"Press ENTER to get back to the menu > \r\n";


 ///////////////////////////////////////////////////////////////////////////////
//// HELPERS //////////////////////////////////////////////////////////////////

// switches modes and sends prompt
void set_bookedit_mode(descriptor_data *d, int mode) {
	void bookedit_prompt(descriptor_data *d);
	
	d->bookedit->mode = mode;
	bookedit_prompt(d);
}


// save changes
void save_bookedit(descriptor_data *d) {
	void save_author_books(int idnum);
	void ensure_author_in_list(int idnum, bool save_if_add);
	
	struct book_data *book, *book_ctr;
	struct paragraph_data *para, *copy, *last;
	
	// no saves
	if (!d->bookedit->changed) {
		msg_to_desc(d, "Nothing to save.\r\n");
		return;
	}
	
	// either create or load a live book
	if (d->bookedit->id == NOTHING) {
		// new book
		CREATE(book, struct book_data, 1);
		book->next = NULL;
		book->id = ++top_book_id;
		book->author = GET_IDNUM(d->character);
		
		if (!book_list) {
			book_list = book;
		}
		else {
			// append
			book_ctr = book_list;
			while (book_ctr->next) {
				book_ctr = book_ctr->next;
			}
			book_ctr->next = book;
		}
		
		// STORE FOR LATER:
		d->bookedit->id = book->id;
	}
	else  if (!(book = find_book_by_id(d->bookedit->id))) {
		msg_to_desc(d, "Unable to save changes: Book does not seem to exist.\r\n");
		return;
	}
	
	// save editor back over
	book->bits = d->bookedit->bits;
	
	if (book->title) {
		free(book->title);
	}
	book->title = str_dup(d->bookedit->title);
	
	if (book->byline) {
		free(book->byline);
	}
	book->byline = str_dup(d->bookedit->byline);
	
	if (book->item_name) {
		free(book->item_name);
	}
	book->item_name = str_dup(d->bookedit->item_name);
	
	if (book->item_description) {
		free(book->item_description);
	}
	book->item_description = str_dup(d->bookedit->item_description);

	// free old paragraphs
	while ((para = book->paragraphs)) {
		if (para->text) {
			free(para->text);
		}
		book->paragraphs = para->next;
		free(para);
	}
	
	// copy over paragraphs
	last = NULL;
	for (para = d->bookedit->paragraphs; para; para = para->next) {
		CREATE(copy, struct paragraph_data, 1);
		copy->text = str_dup(para->text);
		copy->next = NULL;
	
		if (last) {
			last->next = copy;
		}
		else {
			book->paragraphs = copy;
		}
	
		last = copy;
	}
	
	// reset this
	d->bookedit->changed = FALSE;
	
	// and save to file
	ensure_author_in_list(book->author, TRUE);
	save_author_books(book->author);
	msg_to_desc(d, "Book saved.\r\n");
}


// exit the editor (DOES NOT SAVE)
void exit_bookedit(descriptor_data *d) {
	struct paragraph_data *para;

	// free up data
	if (d->bookedit->title) {
		free(d->bookedit->title);
	}
	if (d->bookedit->byline) {
		free(d->bookedit->byline);
	}
	if (d->bookedit->item_name) {
		free(d->bookedit->item_name);
	}
	if (d->bookedit->item_description) {
		free(d->bookedit->item_description);
	}
	while ((para = d->bookedit->paragraphs)) {
		d->bookedit->paragraphs = para->next;
		if (para->text) {
			free(para->text);
		}
		free(para);
	}
	
	// free the whole editor
	free(d->bookedit);
	d->bookedit = NULL;
	
	// send them back to the game
	STATE(d) = CON_PLAYING;
	REMOVE_BIT(PLR_FLAGS(d->character), PLR_WRITING);
	
	msg_to_desc(d, "\r\nYou exit the book editor.\r\n");
	act("$n has finished working on a book.", TRUE, d->character, NULL, NULL, TO_ROOM);
}


// sets up the initial bookedit
void begin_bookedit(descriptor_data *d, struct book_data *book) {
	struct paragraph_data *para, *copy, *last;

	CREATE(d->bookedit, struct bookedit_data, 1);
		d->bookedit->changed = FALSE;
	
	if (book) {
		d->bookedit->id = book->id;
		d->bookedit->bits = book->bits;

		d->bookedit->title = str_dup(book->title);
		d->bookedit->byline = str_dup(book->byline);
		d->bookedit->item_name = str_dup(book->item_name);
		d->bookedit->item_description = str_dup(book->item_description);	

		last = NULL;
		for (para = book->paragraphs; para; para = para->next) {
			CREATE(copy, struct paragraph_data, 1);
			copy->text = str_dup(para->text);
			copy->next = NULL;
		
			if (last) {
				last->next = copy;
			}
			else {
				d->bookedit->paragraphs = copy;
			}
		
			last = copy;
		}
	}
	else {
		// new!
		d->bookedit->id = NOTHING;
		d->bookedit->bits = 0;
		d->bookedit->title = str_dup("Untitled");
		d->bookedit->byline = str_dup(PERS(d->character, d->character, 1));
		d->bookedit->item_name = str_dup("a book");
		d->bookedit->item_description = str_dup("It appears to be a book.\r\n");
		d->bookedit->paragraphs = NULL;
	}
	
	// player info
	SET_BIT(PLR_FLAGS(d->character), PLR_WRITING);
	STATE(d) = CON_BOOKEDIT;
	set_bookedit_mode(d, BOOKEDIT_MENU);
}


 ///////////////////////////////////////////////////////////////////////////////
//// DISPLAYS /////////////////////////////////////////////////////////////////


void send_bookedit_menu(descriptor_data *d) {
	struct paragraph_data *para;
	int count = 0;
	
	for (para = d->bookedit->paragraphs; para; para = para->next) {
		++count;
	}
	
	if (IS_IMMORTAL(d->character)) {
		sprintf(buf1, "Book ID: %d\r\n", d->bookedit->id);
	}
	else {
		*buf1 = '\0';
	}

	msg_to_desc(d, "[ EmpireMUD Book Editor (%s) ]\r\n"
					"%s"
					"&yTitle&0: %s\r\n"
					"&yByline&0: %s\r\n"
					"&yItem&0: %s\r\n"
					"&yDescription&0: \r\n%s"
					"&yParagraphs&0 (&ylist, edit, new, delete&0): %d\r\n"
					"Type 'license' to see the terms for writing text here\r\n"
					"Type any keyword, or 'help' for help, or 'exit' to quit\r\n"
					"> ",
		(d->bookedit->changed ? "&rUNSAVED&0" : "&gSAVED&0"),
		buf1,
		d->bookedit->title, d->bookedit->byline,
		d->bookedit->item_name, d->bookedit->item_description,
		count
	);	
}


// master display controller
void bookedit_prompt(descriptor_data *d) {
	// leading crlf
	SEND_TO_Q("\r\n", d);

	switch (d->bookedit->mode) {
		case BOOKEDIT_MENU: {
			send_bookedit_menu(d);
			break;
		}
		case BOOKEDIT_MENU_HELP: {
			msg_to_desc(d, bookedit_help_display);
			break;
		}
		case BOOKEDIT_MENU_LICENSE: {
			msg_to_desc(d, bookedit_license_display);
			break;
		}
		case BOOKEDIT_SAVE_BEFORE_QUIT: {
			msg_to_desc(d, "Save your changes before you quit (yes/no)? ");
			break;
		}
	}
}


 ///////////////////////////////////////////////////////////////////////////////
//// BOOKEDIT INPUT ///////////////////////////////////////////////////////////

#define PARSE_BOOKEDIT(name)  void (name)(descriptor_data *d, char *argument)


PARSE_BOOKEDIT(bookedit_menu_help) {
	set_bookedit_mode(d, BOOKEDIT_MENU_HELP);
}

PARSE_BOOKEDIT(bookedit_menu_license) {
	set_bookedit_mode(d, BOOKEDIT_MENU_LICENSE);
}


PARSE_BOOKEDIT(bookedit_menu_exit) {
	if (d->bookedit->changed) {
		set_bookedit_mode(d, BOOKEDIT_SAVE_BEFORE_QUIT);
	}
	else {
		exit_bookedit(d);
	}
}


PARSE_BOOKEDIT(bookedit_menu_save) {
	save_bookedit(d);
	set_bookedit_mode(d, BOOKEDIT_MENU);
}


PARSE_BOOKEDIT(bookedit_menu_title) {
	if (!*argument) {
		msg_to_desc(d, "\r\nUsage: title <new title>\r\n> ");
	}
	else if (strlen(argument) - (2 * count_color_codes(argument)) > MAX_BOOK_TITLE) {
		msg_to_desc(d, "\r\nBook titles may not be more than %d characters long.\r\n> ", MAX_BOOK_TITLE);
	}
	else {
		if (d->bookedit->title) {
			free(d->bookedit->title);
		}
		d->bookedit->title = str_dup(argument);
		d->bookedit->changed = TRUE;
		set_bookedit_mode(d, BOOKEDIT_MENU);
	}
}


PARSE_BOOKEDIT(bookedit_menu_byline) {
	if (!*argument) {
		msg_to_desc(d, "\r\nUsage: byline <new author name>\r\n> ");
	}
	else if (strlen(argument) - (2 * count_color_codes(argument)) > MAX_BOOK_BYLINE) {
		msg_to_desc(d, "\r\nBook bylines may not be more than %d characters long.\r\n> ", MAX_BOOK_TITLE);
	}
	else {
		if (d->bookedit->byline) {
			free(d->bookedit->byline);
		}
		d->bookedit->byline = str_dup(argument);
		d->bookedit->changed = TRUE;
		set_bookedit_mode(d, BOOKEDIT_MENU);
	}
}


PARSE_BOOKEDIT(bookedit_menu_item) {
	if (!*argument) {
		msg_to_desc(d, "\r\nUsage: item <new item name>\r\n> ");
	}
	else if (strlen(argument) - (2 * count_color_codes(argument)) > MAX_BOOK_ITEM_NAME) {
		msg_to_desc(d, "\r\nBook item names may not be more than %d characters long.\r\n> ", MAX_BOOK_ITEM_NAME);
	}
	else {
		if (d->bookedit->item_name) {
			free(d->bookedit->item_name);
		}
		d->bookedit->item_name = str_dup(argument);
		d->bookedit->changed = TRUE;
		set_bookedit_mode(d, BOOKEDIT_MENU);
	}
}


PARSE_BOOKEDIT(bookedit_menu_description) {
	msg_to_desc(d, "\r\nEdit the item description for your book. This is shown when\r\na player looks at the book item. If it's more than one line, you should\r\nformat it with /fi\r\n");
	start_string_editor(d, "description", &(d->bookedit->item_description), MAX_ITEM_DESCRIPTION);
	d->bookedit->changed = TRUE;
	d->bookedit->mode = BOOKEDIT_ITEM_DESCRIPTION;
}


PARSE_BOOKEDIT(bookedit_paragraphs_list) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	int from = NOTHING, to = NOTHING;
	struct paragraph_data *para;
	int count;
	
	half_chop(argument, arg1, arg2);
	
	if (*arg1) {
		from = atoi(arg1);
		if (from == 0) {
			// nope
			from = NOTHING;
		}
	}
	if (*arg2) {
		to = atoi(arg2);
		if (to == 0) {
			// nope
			to = NOTHING;
		}
	}
	
	// start with a crlf
	strcpy(buf, "\r\n");
	
	// only asking for 1?
	if (from != NOTHING && to == NOTHING) {
		to = from;
	}

	// count starts at 1 and matches the paragraph number as the player sees it
	for (para = d->bookedit->paragraphs, count = 1; para; para = para->next, ++count) {
		if (from == NOTHING || from <= count) {
			if (to == NOTHING || to >= count) {
				sprintf(buf + strlen(buf), "&cParagraph %d&0\r\n%s\r\n", count, para->text);
			}
		}
	}
	
	strcat(buf, "> ");
	page_string(d, buf, TRUE);
}


PARSE_BOOKEDIT(bookedit_paragraphs_edit) {
	struct paragraph_data *para;
	int num;
	bool found = FALSE;
	
	if ((num = atoi(argument)) <= 0) {
		msg_to_desc(d, "\r\nInvalid paragraph number.\r\n> ");
		return;
	}
	
	for (para = d->bookedit->paragraphs; !found && para; para = para->next) {
		if (--num == 0) {
			found = TRUE;
			msg_to_desc(d, "\r\nNOTE: Each paragraph should be no more than 4 or 5 lines, formatted with /fi\r\n");
			start_string_editor(d, "paragraph", &(para->text), MAX_BOOK_PARAGRAPH);
	
			d->bookedit->changed = TRUE;
			d->bookedit->mode = BOOKEDIT_PARAGRAPH;
		}
	}
	
	if (!found) {
		msg_to_desc(d, "\r\nInvalid paragraph number.\r\n> ");
	}
}


PARSE_BOOKEDIT(bookedit_paragraphs_new) {
	struct paragraph_data *para, *new_para, *last;
	int pos = NOTHING;
	bool found = FALSE;
	
	if (*argument) {
		if ((pos = atoi(argument)) <= 0) {
			msg_to_desc(d, "\r\nInvalid paragraph position.\r\n> ");
			return;
		}
	}
	
	d->bookedit->changed = TRUE;

	CREATE(new_para, struct paragraph_data, 1);
	new_para->text = str_dup("   This is a new paragraph.\r\n");
	new_para->next = NULL;
	
	if (pos == 1) {
		new_para->next = d->bookedit->paragraphs;
		d->bookedit->paragraphs = new_para;
		found = TRUE;
	}
	else {
		// find the correct insert position or possibly the end of the list
		for (para = d->bookedit->paragraphs, last = para; !found && para; para = para->next) {
			if (pos != NOTHING && --pos == 0) {
				new_para->next = last->next;
				last->next = new_para;
				found = TRUE;
			}
			
			last = para;
		}
		
		if (!found && last) {
			last->next = new_para;
			found = TRUE;
		}
	}
		
	if (found) {
		msg_to_desc(d, "\r\nNOTE: Each paragraph should be no more than 4 or 5 lines, formatted with /fi\r\n");
		start_string_editor(d, "new paragraph", &(new_para->text), MAX_BOOK_PARAGRAPH);

		d->bookedit->mode = BOOKEDIT_PARAGRAPH;
	}
	else {
		msg_to_desc(d, "\r\nInvalid paragraph position.\r\n> ");
	}
}


PARSE_BOOKEDIT(bookedit_paragraphs_delete) {
	struct paragraph_data *para, *temp;
	int pos = NOTHING;
	bool found = FALSE;
	
	if (!*argument) {
		msg_to_desc(d, "\r\nUsage: paragraphs delete <number>\r\n> ");
	}
	else if ((pos = atoi(argument)) <= 0) {
		msg_to_desc(d, "\r\nInvalid paragraph position.\r\n> ");
	}
	else {
		for (para = d->bookedit->paragraphs; !found && para; para = para->next) {
			if (--pos == 0) {
				REMOVE_FROM_LIST(para, d->bookedit->paragraphs, next);
				if (para->text) {
					free(para->text);
				}
				free(para);
				
				found = TRUE;
			}
		}

		if (found) {
			d->bookedit->changed = TRUE;
			msg_to_desc(d, "\r\nDeleted paragraph %d.\r\n> ", atoi(argument));
		}
		else {
			msg_to_desc(d, "\r\nNo such paragraph to delete.\r\n> ");
		}
	}
}


// paragraphs commands
struct {
	char *command;
	void (*func)(descriptor_data *d, char *argument);
} bookedit_paragraphs_option[] = {
	{ "list", bookedit_paragraphs_list },
	{ "edit", bookedit_paragraphs_edit },
	{ "new", bookedit_paragraphs_new },
	{ "delete", bookedit_paragraphs_delete },
	
	{ "\n", NULL }
};


PARSE_BOOKEDIT(bookedit_menu_paragraphs) {
	char arg2[MAX_INPUT_LENGTH];
	int iter, opt;
	
	half_chop(argument, arg, arg2);
	
	opt = NOTHING;
	for (iter = 0; opt == NOTHING && *bookedit_paragraphs_option[iter].command != '\n'; ++iter) {
		if (is_abbrev(arg, bookedit_paragraphs_option[iter].command)) {
			opt = iter;
		}
	}
	
	if (!*arg || opt == NOTHING) {
		msg_to_desc(d, "\r\nUnknown paragraphs option (see 'help' for info) > ");
		// do not change state or re-show menu
	}
	else {
		(bookedit_paragraphs_option[opt].func)(d, arg2);
	}
}


// main menu
struct {
	char *command;
	int level;
	void (*func)(descriptor_data *d, char *argument);
} bookedit_menu_option[] = {
	{ "help", 0, bookedit_menu_help },
	{ "license", 0, bookedit_menu_license },
	{ "exit", 0, bookedit_menu_exit },
	{ "save", 0, bookedit_menu_save },
	{ "title", 0, bookedit_menu_title },
	{ "byline", 0, bookedit_menu_byline },
	{ "item", 0, bookedit_menu_item },
	{ "description", 0, bookedit_menu_description },
	{ "paragraphs", 0, bookedit_menu_paragraphs },

	//	{ "flags", LVL_IMMORTAL, bookedit_menu_flags },
	
	{ "\n", 0, NULL }
};


void parse_bookedit(descriptor_data *d, char *argument) {
	char arg2[MAX_INPUT_LENGTH];
	int iter, opt;

	// safety and sanity
	if (!d->bookedit) {
		STATE(d) = CON_CLOSE;
		msg_to_desc(d, "There was a problem with the bookeditor. Please reconnect and try again.\r\n");
		return;
	}
	
	skip_spaces(&argument);

	switch (d->bookedit->mode) {
		case BOOKEDIT_MENU: {
			half_chop(argument, arg, arg2);
			
			opt = NOTHING;
			for (iter = 0; opt == NOTHING && *bookedit_menu_option[iter].command != '\n'; ++iter) {
				if (GET_ACCESS_LEVEL(d->character) >= bookedit_menu_option[iter].level && is_abbrev(arg, bookedit_menu_option[iter].command)) {
					opt = iter;
				}
			}
			
			if (!*arg) {
				// just re-show menu
				set_bookedit_mode(d, BOOKEDIT_MENU);
			}
			else if (opt == NOTHING) {
				msg_to_desc(d, "\r\nUnknown option (see 'help' for info) > ");
				// do not change state or re-show menu
			}
			else {
				(bookedit_menu_option[opt].func)(d, arg2);
			}
						
			break;
		}	// end BOOKEDIT_MENU
		case BOOKEDIT_SAVE_BEFORE_QUIT: {
			if (is_abbrev(argument, "yes")) {
				save_bookedit(d);
				exit_bookedit(d);
			}
			else if (is_abbrev(argument, "no")) {
				exit_bookedit(d);
			}
			else {
				msg_to_desc(d, "\r\nInvalid response.\r\n");
				set_bookedit_mode(d, BOOKEDIT_SAVE_BEFORE_QUIT);
			}
			break;
		}
		
		// things that just send you back to the menu
		case BOOKEDIT_ITEM_DESCRIPTION:
		case BOOKEDIT_MENU_HELP: {
			set_bookedit_mode(d, BOOKEDIT_MENU);
			break;
		}
	}
}


 ///////////////////////////////////////////////////////////////////////////////
//// BOOKEDIT COMMANDS ////////////////////////////////////////////////////////

LIBRARY_SCMD(bookedit_list) {
	struct book_data *book;
	int count = 0;

	if (ch->desc) {
		sprintf(buf, "Books you have written:\r\n");
	
		for (book = book_list; book; book = book->next) {
			if (book->author == GET_IDNUM(ch)) {
				if (IS_IMMORTAL(ch)) {
					sprintf(buf1, "[%d] ", book->id);
				}
				else {
					*buf1 = '\0';
				}
				sprintf(buf + strlen(buf), "%d. %s%s (%s)\r\n", ++count, buf1, book->title, book->byline);
			}
		}
	
		if (count == 0) {
			strcat(buf, "  none\r\n");
		}
		
		page_string(ch->desc, buf, 1);
	}	
	else {
		// no desc?
	}
}


// processes for bookedit_copy
void process_copying_book(char_data *ch) {
	obj_data *obj;
	struct book_data *book;
	
	GET_ACTION_TIMER(ch) -= 1;
	
	if (GET_ACTION_TIMER(ch) <= 0) {
		GET_ACTION(ch) = ACT_NONE;
		
		if (!(book = find_book_by_id(GET_ACTION_VNUM(ch, 0)))) {
			msg_to_char(ch, "You finish copying the book, but there was an error and it's illegible.\r\n");
		}
		else {
			obj = read_object(o_BOOK);
			GET_OBJ_VAL(obj, VAL_BOOK_ID) = book->id;
			obj_to_char_or_room(obj, ch);
		
			msg_to_char(ch, "You finish a fresh copy of %s!\r\n", book->title);
			act("$n finishes copying out $p.", TRUE, ch, obj, NULL, TO_ROOM);
			load_otrigger(obj);
		}
	}
	else if (GET_ACTION_TIMER(ch) == 6 && !PRF_FLAGGED(ch, PRF_NOSPAM)) {
		msg_to_char(ch, "You're halfway through copying out the book...\r\n");
	}
}

LIBRARY_SCMD(bookedit_copy) {
	struct book_data *book;
	
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
		start_action(ch, ACT_COPYING_BOOK, 12, 0);
		GET_ACTION_VNUM(ch, 0) = book->id;
		
		msg_to_char(ch, "You pick up a blank book and begin to copy out '%s'.\r\n", book->title);
		act("$n begins to write out a copy of a book.", TRUE, ch, NULL, NULL, TO_ROOM);
	}
}


LIBRARY_SCMD(bookedit_write) {
	struct book_data *book = NULL;
	
	skip_spaces(&argument);
	
	if (!*argument) {
		msg_to_char(ch, "Usage: bookedit write <new | book title | book number>\r\nSee 'bookedit list' for a list.\r\n");
		return;
	}
	else if (!ch->desc) {
		// nobody home!
		return;
	}

	// find target (if not "new")
	if (str_cmp(argument, "new") && !(book = find_book_by_author(argument, GET_IDNUM(ch)))) {
		msg_to_char(ch, "No such book.\r\n");
		return;
	}

	// begin editor
	act("$n begins working on a book.", TRUE, ch, 0, 0, TO_ROOM);
	begin_bookedit(ch->desc, book);
}
