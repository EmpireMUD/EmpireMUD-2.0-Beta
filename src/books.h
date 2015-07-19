/* ************************************************************************
*   File: books.h                                         EmpireMUD 2.0b2 *
*  Usage: header file for books and libraries                             *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define MAX_BOOK_TITLE  40
#define MAX_BOOK_BYLINE  40
#define MAX_BOOK_ITEM_NAME  20
#define MAX_BOOK_PARAGRAPH  4000


#define LIBRARY_SCMD(name)  void name(char_data *ch, char *argument)
#define DEFAULT_BOOK  0	// in case of errors


 ///////////////////////////////////////////////////////////////////////////////
//// DATA /////////////////////////////////////////////////////////////////////

// BITS ARE UNIMPLEMENTED!!!
#define BOOK_GLOBAL  BIT(0)	// a. available in all libraries


struct library_data {
	room_vnum location;
	struct library_data *next;
};

// data for a book
struct book_data {
	int id;
	
	int author;
	unsigned int bits;
	char *title;
	char *byline;
	char *item_name;
	char *item_description;
	
	struct paragraph_data *paragraphs;	// linked list
	struct library_data *in_libraries;	// places this book is kept
	
	struct book_data *next;
};


// bookedit modes
#define BOOKEDIT_MENU  0
#define BOOKEDIT_SAVE_BEFORE_QUIT  1
#define BOOKEDIT_MENU_HELP  2
#define BOOKEDIT_ITEM_DESCRIPTION  3
#define BOOKEDIT_PARAGRAPH  4
#define BOOKEDIT_MENU_LICENSE  3


 ///////////////////////////////////////////////////////////////////////////////
//// GLOBALS //////////////////////////////////////////////////////////////////

extern struct book_data *book_list;
extern int *author_list;
extern int size_of_author_list;
extern int top_book_id;


// protos
extern struct book_data *find_book_by_id(int id);
extern struct book_data *find_book_by_author(char *argument, int idnum);
extern struct book_data *find_book_in_library(char *argument, room_data *room);
