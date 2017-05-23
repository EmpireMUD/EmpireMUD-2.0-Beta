/* ************************************************************************
*   File: modify.c                                        EmpireMUD 2.0b5 *
*  Usage: Run-time modification of game variables                         *
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
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "comm.h"
#include "boards.h"
#include "olc.h"


/*
 * Action modes for parse_action().
 */
#define PARSE_FORMAT		0
#define PARSE_REPLACE		1
#define PARSE_HELP			2
#define PARSE_DELETE		3
#define PARSE_INSERT		4
#define PARSE_LIST_NORM		5
#define PARSE_LIST_NUM		6
#define PARSE_EDIT			7
#define PARSE_LIST_COLOR	8

/*
 * Defines for the action variable.
 */
#define STRINGADD_OK		0	/* Just keep adding text.				*/
#define STRINGADD_SAVE		1	/* Save current text.					*/
#define STRINGADD_ABORT		2	/* Abort edit, restore old text.		*/
#define STRINGADD_ACTION	4	/* Editor action, don't append \r\n.	*/

#define FORMAT_INDENT	BIT(0)


extern char *show_color_codes(char *string);
void show_string(descriptor_data *d, char *input);

/* local functions */
void smash_tilde(char *str);
char *next_page(char *str, descriptor_data *desc);
int count_pages(char *str, descriptor_data *desc);
void paginate_string(char *str, descriptor_data *d);


/* ************************************************************************
*  modification of malloc'ed strings                                      *
************************************************************************ */
/*
 * Put '#if 1' here to erase ~, or roll your own method.  A common idea
 * is smash/show tilde to convert the tilde to another innocuous character
 * to save and then back to display it. Whatever you do, at least keep the
 * function around because other MUD packages use it, like mudFTP.
 *   -gg 9/9/98
 */
void smash_tilde(char *str) {
#if 0
	/*
	 * Erase any ~'s inserted by people in the editor.  This prevents anyone
	 * using online creation from causing parse errors in the world files.
	 * Derived from an idea by Sammy <samedi@dhc.net> (who happens to like
	 * his tildes thank you very much.), -gg 2/20/98
	 *
	while ((str = strchr(str, '~')) != NULL)
		*str = ' ';
	*/
#endif

	// empiremud version
	char *ptr;
	
	for (ptr = str; *ptr; ++ptr) {
		if (*ptr == '~' && (*(ptr+1) == '\r' || *(ptr+1) == '\n' || *(ptr+1) == '\0')) {
			*ptr = ' ';
		}
	}
}


/**
* API to start editing a string. This no longer takes the player out of the
* game.
* 
* @param descriptor_data *d The user who will be editing the string.
* @param char *prompt The thing being edited ("name", "description for Mobname").
* @param char **writeto The place where the string will be stored.
* @param size_t max_len The maximum string length.
* @param bool allow_null If TRUE, empty strings will be freed and left null.
*/
void start_string_editor(descriptor_data *d, char *prompt, char **writeto, size_t max_len, bool allow_null) {
	if (d->str) {
		log("SYSERR: start_string_editor called for player already using string editor (len:%d)", (int)max_len);
		return;
	}
	
	if (max_len > MAX_STRING_LENGTH) {
		log("SYSERR: start_string_editor called with max length %zu (cap is %d)", max_len, MAX_STRING_LENGTH);
		max_len = MAX_STRING_LENGTH;
	}
	
	d->str = writeto;
	d->max_str = max_len;
	d->str_on_abort = NULL;

	if (*writeto) {
		d->backstr = str_dup(*writeto);
	}
	else {
		d->backstr = NULL;
	}
	
	d->straight_to_editor = TRUE;
	d->mail_to = 0;
	d->notes_id = 0;
	d->save_empire = NOTHING;
	d->file_storage = NULL;
	d->allow_null = allow_null;
	
	if (STATE(d) == CON_PLAYING && !d->straight_to_editor) {
		msg_to_desc(d, "&cEdit %s: (, to add; ,/s to save; ,/h for help)&0\r\n", prompt);
	}
	else {
		msg_to_desc(d, "&cEdit %s: (/s to save; /h for help, /, to toggle to command mode)&0\r\n", prompt);
	}
	
	if (*writeto) {
		if (strlen(*writeto) > 0) {
			msg_to_desc(d, "%s", show_color_codes(*writeto));
		
			// extra crlf if it did not end with one
			if ((*writeto)[strlen(*writeto)-1] != '\n' && (*writeto)[strlen(*writeto)-1] != '\r') {
				msg_to_desc(d, "\r\n");
			}
		}
		else {
			msg_to_desc(d, "(text is currently empty)\r\n");
		}
	}
}


/* Add user input to the 'current' string (as defined by d->str) */
void string_add(descriptor_data *d, char *str) {
	void check_delayed_load(char_data *ch);
	extern char *stripcr(char *dest, const char *src);
	extern int improved_editor_execute(descriptor_data *d, char *str);
	
	player_index_data *index;
	struct mail_data *mail;
	account_data *acct;
	char_data *recip;
	bool is_file;
	int action;
	FILE *fl;

	delete_doubledollar(str);
	smash_tilde(str);

	if ((action = improved_editor_execute(d, str)) == STRINGADD_ACTION)
		return;

	if (!(*d->str)) {
		if (strlen(str) + 3 > d->max_str) {
			send_to_char("String too long - Truncated.\r\n", d->character);
			CREATE(*d->str, char, d->max_str);
			strncpy(*d->str, str, d->max_str);
			strcpy(*d->str + (d->max_str - 3), "\r\n");
		}
		else {
			CREATE(*d->str, char, strlen(str) + 3);
			strcpy(*d->str, str);
		}
	}
	else {
		if (strlen(str) + strlen(*d->str) + 3 > d->max_str) {
			if (action == STRINGADD_OK) {
				send_to_char("String too long. Last line skipped.\r\n", d->character);
				return;	// No appending \r\n\0, but still let them save.
			}
		}
		else {
			RECREATE(*d->str, char, strlen(*d->str) + strlen(str) + 3);
			strcat(*d->str, str);
		}
	}
	
	if (action == STRINGADD_OK && STATE(d) == CON_PLAYING && !d->straight_to_editor) {
		msg_to_desc(d, "Text added.\r\n");
	}
	
	switch (action) {
		case STRINGADD_ABORT:
			// only if not mailing/board-writing
			if ((d->mail_to <= 0) && STATE(d) == CON_PLAYING) {
				free(*d->str);
				if (d->str_on_abort) {
					*d->str = d->str_on_abort;
					if (d->backstr) {
						free(d->backstr);
					}
				}
				else {
					*d->str = d->backstr;
				}
				d->backstr = NULL;
				d->str = NULL;
				d->str_on_abort = NULL;
			}
			break;
		case STRINGADD_SAVE:
			if (d->str && *d->str && **d->str == '\0') {
				free(*d->str);
				if (d->allow_null) {
					*d->str = NULL;
				}
				else {
					*d->str = str_dup("Nothing.\r\n");
				}
			}
			if (d->backstr)
				free(d->backstr);
			d->backstr = NULL;
			d->str_on_abort = NULL;
			break;
	}

	if (action) {
		if (STATE(d) == CON_PLAYING && PLR_FLAGGED(d->character, PLR_MAILING)) {
			if (action == STRINGADD_SAVE && *d->str) {
				if ((index = find_player_index_by_idnum(d->mail_to)) && (recip = find_or_load_player(index->name, &is_file))) {
					check_delayed_load(recip);	// need to delay-load them to save mail
					
					// create letter
					CREATE(mail, struct mail_data, 1);
					mail->from = GET_IDNUM(d->character);
					mail->timestamp = time(0);
					mail->body = str_dup(*d->str);
					
					// put it on the pile
					mail->next = GET_MAIL_PENDING(recip);
					GET_MAIL_PENDING(recip) = mail;
					
					if (is_file) {
						store_loaded_char(recip);
					}
				}
				
				SEND_TO_Q("You tie your message to a pigeon and it flies away!\r\n", d);
			}
			else {
				SEND_TO_Q("Mail aborted.\r\n", d);
			}
			
			free(*d->str);
			free(d->str);
			d->str = NULL;
		}
		else if (STATE(d) == CON_PLAYING && d->save_empire != NOTHING && action == STRINGADD_SAVE) {
			empire_data *emp = real_empire(d->save_empire);
			if (emp) {
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
				
				if (emp != GET_LOYALTY(d->character)) {
					syslog(SYS_GC, GET_INVIS_LEV(d->character), TRUE, "ABUSE: %s has edited text for %s", GET_NAME(d->character), EMPIRE_NAME(emp));
				}
			}
		}
		else if (STATE(d) == CON_PLAYING && d->mail_to >= BOARD_MAGIC) {
			Board_save_board(d->mail_to - BOARD_MAGIC);
			if (action == STRINGADD_ABORT) {
				SEND_TO_Q("Post not aborted, use REMOVE <post #>.\r\n", d);
			}
			else {
				SEND_TO_Q("Post complete.\r\n", d);
			}
		}
		else if (d->notes_id > 0) {
			if (action != STRINGADD_ABORT) {
				syslog(SYS_GC, GET_INVIS_LEV(d->character), TRUE, "GC: %s has edited notes for account %d", GET_NAME(d->character), d->notes_id);
				SEND_TO_Q("Notes saved.\r\n", d);
			}
			else {
				SEND_TO_Q("Edit aborted.\r\n", d);
			}
			
			// save if possible -- even if aborted (if it was saved while we were editing, we need to overwrite with thet right thing)
			if ((acct = find_account(d->notes_id))) {
				SAVE_ACCOUNT(acct);
			}
			
			act("$n stops editing notes.", TRUE, d->character, FALSE, FALSE, TO_ROOM);
		}
		else if (d->file_storage) {
			if (action != STRINGADD_ABORT) {
				if ((fl = fopen((char *)d->file_storage, "w"))){
					if (*d->str)
						fputs(stripcr(buf1, *d->str), fl);
					fclose(fl);

					syslog(SYS_GC, GET_INVIS_LEV(d->character), TRUE, "GC: %s saves '%s'.", GET_NAME(d->character), d->file_storage);
					SEND_TO_Q("Saved.\r\n", d);
				}
			}
			else {
				SEND_TO_Q("Edit aborted.\r\n", d);
			}

			act("$n stops editing a file.", TRUE, d->character, 0, 0, TO_ROOM);
			if (d->file_storage) {
				free(d->file_storage);
			}
			d->file_storage = NULL;
		}
		else {
			if (action != STRINGADD_ABORT) {
				SEND_TO_Q("Text editor saved.\r\n", d);
			}
			else {
				SEND_TO_Q("Text editor aborted.\r\n", d);
			}
		}

		d->str = NULL;
		d->mail_to = 0;
		d->notes_id = 0;
		d->max_str = 0;
		d->save_empire = NOTHING;
		if (d->file_storage) {
			free(d->file_storage);
		}
		d->file_storage = NULL;
		if (d->character && !IS_NPC(d->character)) {
			REMOVE_BIT(PLR_FLAGS(d->character), PLR_MAILING);
		}
	}
	else if (strlen(*d->str) <= (d->max_str-3))
		strcat(*d->str, "\r\n");
}


/*********************************************************************
* New Pagination Code
* Michael Buselli submitted the following code for an enhanced pager
* for CircleMUD.  All functions below are his.  --JE 8 Mar 96
*********************************************************************/

// fallback defaults if these numbers can't be detected
#define PAGE_LENGTH     22
#define PAGE_WIDTH      80

/**
* Traverse down the string until the begining of the next page has been
* reached.  Return NULL if this is the last page of the string.
*
* @param char *str The string to get the next page from.
* @param descriptor_data *desc The descriptor who will receive it (may have their own page size).
* @return char* The next page.
*/
char *next_page(char *str, descriptor_data *desc) {
	int col = 1, line = 1, spec_code = FALSE;
	int length, width;
	
	length = (desc && desc->pProtocol->ScreenHeight > 0) ? (desc->pProtocol->ScreenHeight - 2) : PAGE_LENGTH;
	width = (desc && desc->pProtocol->ScreenWidth > 0) ? desc->pProtocol->ScreenWidth : PAGE_WIDTH;

	for (;; ++str) {
		/* If end of string, return NULL. */
		if (*str == '\0')
			return (NULL);

		/* If we're at the start of the next page, return this fact. */
		else if (line > length)
			return (str);

		/* Check for the begining of an ANSI color code block. */
		else if (*str == '\x1B' && !spec_code)
			spec_code = TRUE;

		/* Check for the end of an ANSI color code block. */
		else if (*str == 'm' && spec_code)
			spec_code = FALSE;
		
		// skip & colorcodes
		else if (*str == '&') {
			++str;
			if (*str == '&') {	// cause it to print a & in case of &&
				--str;
			}
		}

		/* Check for everything else. */
		else if (!spec_code) {
			/* Carriage return puts us in column one. */
			if (*str == '\r')
				col = 1;
			/* Newline puts us on the next line. */
			else if (*str == '\n')
				line++;

			/*
			 * We need to check here and see if we are over the page width,
			 * and if so, compensate by going to the beginning of the next line.
			 */
			else if (col++ > width) {
				col = 1;
				line++;
			}
		}
	}
}


/**
* Function that returns the number of pages in the string.
*
* @param char *str The string to count pages in.
* @param descriptor_data *desc The descriptor who will receive it (may have their own page size).
* @return int The number of pages.
*/
int count_pages(char *str, descriptor_data *desc) {
	int pages;

	for (pages = 1; (str = next_page(str, desc)); pages++);
	return (pages);
}


/*
 * This function assigns all the pointers for showstr_vector for the
 * page_string function, after showstr_vector has been allocated and
 * showstr_count set.
 */
void paginate_string(char *str, descriptor_data *d) {
	int i;

	if (d->showstr_count)
		*(d->showstr_vector) = str;

	for (i = 1; i < d->showstr_count && str; i++)
		str = d->showstr_vector[i] = next_page(str, d);

	d->showstr_page = 0;
}


/* The call that gets the paging ball rolling... */
void page_string(descriptor_data *d, char *str, int keep_internal) {
	int length;
	
	if (!d)
		return;

	if (!str || !*str)
		return;

	// determine if it will be too long after parsing color codes
	length = strlen(str) + (color_code_length(str) * 3);

	if (d->character && PRF_FLAGGED(d->character, PRF_SCROLLING) && length < MAX_STRING_LENGTH) {
		send_to_char(str, d->character);
		return;
	}

	d->showstr_count = count_pages(str, d);
	CREATE(d->showstr_vector, char *, d->showstr_count);

	if (keep_internal) {
		d->showstr_head = str_dup(str);
		paginate_string(d->showstr_head, d);
	}
	else
		paginate_string(str, d);

	show_string(d, "");
}


/* The call that displays the next page. */
void show_string(descriptor_data *d, char *input) {
	char buffer[MAX_STRING_LENGTH];
	int diff;

	any_one_arg(input, buf);

	/* Q is for quit. :) */
	if (LOWER(*buf) == 'q') {
		free(d->showstr_vector);
		d->showstr_count = 0;
		if (d->showstr_head) {
			free(d->showstr_head);
			d->showstr_head = NULL;
		}
		return;
	}
	/*
	 * R is for refresh, so back up one page internally so we can display
	 * it again.
	 */
	else if (LOWER(*buf) == 'r')
		d->showstr_page = MAX(0, d->showstr_page - 1);

	/*
	 * B is for back, so back up two pages internally so we can display the
	 * correct page here.
	 */
	else if (LOWER(*buf) == 'b')
		d->showstr_page = MAX(0, d->showstr_page - 2);

	/*
	 * Feature to 'goto' a page.  Just type the number of the page and you
	 * are there!
	 */
	else if (isdigit(*buf))
		d->showstr_page = MAX(0, MIN(atoi(buf) - 1, d->showstr_count - 1));

	else if (*buf) {
		send_to_char("Valid commands while paging are RETURN, Q, R, B, or a numeric value.\r\n", d->character);
		return;
	}
	/*
	 * If we're displaying the last page, just send it to the character, and
	 * then free up the space we used.
	 */
	if (d->showstr_page + 1 >= d->showstr_count) {
		send_to_char(d->showstr_vector[d->showstr_page], d->character);
		free(d->showstr_vector);
		d->showstr_count = 0;
		if (d->showstr_head) {
			free(d->showstr_head);
			d->showstr_head = NULL;
		}
	}
	/* Or if we have more to show.... */
	else {
		diff = d->showstr_vector[d->showstr_page + 1] - d->showstr_vector[d->showstr_page];
		if (diff >= MAX_STRING_LENGTH)
			diff = MAX_STRING_LENGTH - 1;
		strncpy(buffer, d->showstr_vector[d->showstr_page], diff);
		buffer[diff] = '\0';
		send_to_char(buffer, d->character);
		d->showstr_page++;
	}
}


/* End of code by Michael Buselli */


/************************************************************************
* The following modify code was shipped with OasisOLC.  Here are the    *
* credits from the patch I borrowed it from.                            *
* OasisOLC                                                         v2.0 *
* Original author: Levork                                               *
* Copyright 1996 by Harvey Gilpin                                       *
* Copyright 1997-1999 by George Greer (greerga@circlemud.org)           *
************************************************************************/


int improved_editor_execute(descriptor_data *d, char *str) {
	void parse_action(int command, char *string, descriptor_data *d);
	char actions[MAX_INPUT_LENGTH];

	if (*str != '/')
		return STRINGADD_OK;

	strncpy(actions, str + 2, sizeof(actions) - 1);
	actions[sizeof(actions)-1] = '\0';
	*str = '\0';

	switch(str[1]) {
		case 'a':
			return STRINGADD_ABORT;
		case 'c':
			if (d->str && *(d->str)) {
				free(*d->str);
				*(d->str) = NULL;
				SEND_TO_Q("Current buffer cleared.\r\n", d);
			}
			else
				SEND_TO_Q("Current buffer empty.\r\n", d);
			break;
		case 'd':
			parse_action(PARSE_DELETE, actions, d);
			break;
		case 'e':
			parse_action(PARSE_EDIT, actions, d);
			break;
		case 'f':
			if (d->str && *(d->str))
				parse_action(PARSE_FORMAT, actions, d);
			else
				SEND_TO_Q("Current buffer empty.\r\n", d);
			break;
		case 'i':
			if (d->str && *(d->str))
				parse_action(PARSE_INSERT, actions, d);
			else
				SEND_TO_Q("Current buffer empty.\r\n", d);
			break;
		case 'h':
			parse_action(PARSE_HELP, actions, d);
			break;
		case 'l':
			if (d->str && *d->str)
				parse_action(PARSE_LIST_NORM, actions, d);
			else
				SEND_TO_Q("Current buffer empty.\r\n", d);
			break;
		case 'm': {
			if (d->str && *d->str)
				parse_action(PARSE_LIST_COLOR, actions, d);
			else
				SEND_TO_Q("Current buffer empty.\r\n", d);
			break;
		}
		case 'n':
			if (d->str && *d->str)
				parse_action(PARSE_LIST_NUM, actions, d);
			else
				SEND_TO_Q("Current buffer empty.\r\n", d);
			break;
		case 'r':
			parse_action(PARSE_REPLACE, actions, d);
			break;
		case 's':
			return STRINGADD_SAVE;
		case ',': {
			if (d->straight_to_editor) {
				d->straight_to_editor = FALSE;
				SEND_TO_Q("Commands will now go to the game instead of the text editor.\r\n", d);
			}
			else {
				d->straight_to_editor = TRUE;
				SEND_TO_Q("Commands will now go straight to the text editor.\r\n", d);
			}
			break;
		}
		default:
			SEND_TO_Q("Invalid editor option.\r\n", d);
			break;
	}
	return STRINGADD_ACTION;
}


void parse_action(int command, char *string, descriptor_data *d) {
	void format_text(char **ptr_string, int mode, descriptor_data *d, unsigned int maxlen);
	extern int format_script(descriptor_data *d);
	extern int replace_str(char **string, char *pattern, char *replacement, int rep_all, unsigned int max_size);
	
	char buf[MAX_STRING_LENGTH * 3];	// should be big enough
	int indent = 0, rep_all = 0, flags = 0, replaced, i, line_low, line_high, j = 0;
	unsigned int total_len;
	char *s, *t, temp;

	switch (command) {
		case PARSE_HELP:
			sprintf(buf,
				"Editor command formats: /<letter>\r\n"
				"/a         - aborts editor without saving\r\n"
				"/c         - clears buffer\r\n"
				"/d#        - deletes a line\r\n"
				"/e# <text> - changes line # to <text>\r\n"
				"/f         - formats text\r\n"
				"/fi        - formats text and indents\r\n"
				"/i# <text> - inserts <text> before line #\r\n"
				"/l         - lists the buffer\r\n"
				"/m         - lists the buffer with color\r\n"
				"/n         - lists the buffer with line numbers\r\n"
				"/r 'a' 'b' - replaces 1st occurrence of <a> with <b>\r\n"
				"/ra 'a' 'b'- replaces all occurrences of <a> with <b>\r\n"
				"             usage: /r[a] 'pattern' 'replacement'\r\n"
				"/,         - toggle requiring , before text edits\r\n"
				"/s         - saves the buffer and exits\r\n"
				"%s",
				d->straight_to_editor ? "" : "(if you are in-game, you must use , before commands)\r\n");
			SEND_TO_Q(buf, d);
			break;
		case PARSE_FORMAT:
			if (GET_OLC_TRIGGER(d) && d->str == &GET_OLC_STORAGE(d)) {
				msg_to_desc(d, "Script %sformatted.\r\n", format_script(d) ? "": "not ");
				return;
			}

			while (isalpha(string[j]) && j < 2) {
				if (string[j++] == 'i' && !indent) {
					indent = TRUE;
					flags |= FORMAT_INDENT;
				}
			}
			format_text(d->str, flags, d, d->max_str);
			sprintf(buf, "Text formatted with%s indent.\r\n", indent ? "" : "out");
			SEND_TO_Q(buf, d);
			break;
		case PARSE_REPLACE:
			while (isalpha(string[j]) && j < 2)
				if (string[j++] == 'a' && !indent)
					rep_all = 1;
			if ((s = strtok(string, "'")) == NULL) {
				SEND_TO_Q("Invalid format.\r\n", d);
				return;
			}
			else if ((s = strtok(NULL, "'")) == NULL) {
				SEND_TO_Q("Target string must be enclosed in single quotes.\r\n", d);
				return;
			}
			else if ((t = strtok(NULL, "'")) == NULL) {
				SEND_TO_Q("Replacement string must be enclosed in single quotes.\r\n", d);
				return;
			}
			else if ((t = strtok(NULL, "'")) == NULL) {
				SEND_TO_Q("Replacement string must be enclosed in single quotes.\r\n", d);
				return;
			}
			else if ((total_len = ((strlen(t) - strlen(s)) + strlen(*d->str))) <= d->max_str) {
				if ((replaced = replace_str(d->str, s, t, rep_all, d->max_str)) > 0) {
					char temp[MAX_STRING_LENGTH];
					strcpy(temp, show_color_codes(t));
					sprintf(buf, "Replaced %d occurrence%sof '%s' with '%s'.\r\n", replaced, replaced != 1 ? "s " : " ", show_color_codes(s), temp);
					SEND_TO_Q(buf, d);
				}
				else if (replaced == 0) {
					 sprintf(buf, "String '%s' not found.\r\n", show_color_codes(s));
					 SEND_TO_Q(buf, d);
				 }
				else
					SEND_TO_Q("ERROR: Replacement string causes buffer overflow, aborted replace.\r\n", d);
			}
			else
				SEND_TO_Q("Not enough space left in buffer.\r\n", d);
			break;
		case PARSE_DELETE:
			switch (sscanf(string, " %d - %d ", &line_low, &line_high)) {
				case 0:
					SEND_TO_Q("You must specify a line number or range to delete.\r\n", d);
					return;
				case 1:
					line_high = line_low;
					break;
				case 2:
					if (line_high < line_low) {
						SEND_TO_Q("That range is invalid.\r\n", d);
						return;
					}
					break;
				}
			i = 1;
			total_len = 1;
			if ((s = *d->str) == NULL) {
				SEND_TO_Q("Buffer is empty.\r\n", d);
				return;
			}
			else if (line_low > 0) {
				while (s && i < line_low)
					if ((s = strchr(s, '\n')) != NULL) {
						i++;
						s++;
					}
				if (s == NULL || i < line_low) {
					SEND_TO_Q("Line(s) out of range; not deleting.\r\n", d);
					return;
				}
				t = s;
				while (s && i < line_high)
					if ((s = strchr(s, '\n')) != NULL) {
						i++;
						total_len++;
						s++;
					}
				if (s && (s = strchr(s, '\n')) != NULL) {
					while (*(++s))
						*(t++) = *s;
				}
				else
					total_len--;
				*t = '\0';
				RECREATE(*d->str, char, strlen(*d->str) + 3);

				sprintf(buf, "%d line%sdeleted.\r\n", total_len, total_len != 1 ? "s " : " ");
				SEND_TO_Q(buf, d);
			}
			else {
				SEND_TO_Q("Invalid, line numbers to delete must be higher than 0.\r\n", d);
				return;
			}
			break;
		case PARSE_LIST_NORM:	// pass-thru
		case PARSE_LIST_COLOR: {
			*buf = '\0';
			if (*string)
				switch(sscanf(string, " %d - %d ", &line_low, &line_high)) {
					case 0:
						line_low = 1;
						line_high = 999999;
						break;
					case 1:
						line_high = line_low;
						break;
				}
			else {
				line_low = 1;
				line_high = 999999;
			}

			if (line_low < 1) {
				SEND_TO_Q("Line numbers must be greater than 0.\r\n", d);
				return;
			}
			else if (line_high < line_low) {
				SEND_TO_Q("That range is invalid.\r\n", d);
				return;
			}
			*buf = '\0';
			if (line_high < 999999 || line_low > 1)
				sprintf(buf, "Current buffer range [%d - %d]:\r\n", line_low, line_high);
			i = 1;
			total_len = 0;
			s = *d->str;
			while (s && i < line_low)
				if ((s = strchr(s, '\n')) != NULL) {
					i++;
					s++;
				}
			if (i < line_low || s == NULL) {
				SEND_TO_Q("Line(s) out of range; no buffer listing.\r\n", d);
				return;
			}
			t = s;
			while (s && i <= line_high)
				if ((s = strchr(s, '\n')) != NULL) {
					i++;
					total_len++;
					s++;
				}
			if (strlen(buf) + strlen(t) < sizeof(buf)) {
				if (s) {
					temp = *s;
					*s = '\0';
					strcat(buf, t);
					*s = temp;
				}
				else {
					strcat(buf, t);
				}
			}
			sprintf(buf + strlen(buf), "\r\n%d line%sshown.\r\n", total_len, total_len != 1 ? "s " : " ");
			page_string(d, (command == PARSE_LIST_COLOR) ? buf : show_color_codes(buf), TRUE);
			break;
		}
		case PARSE_LIST_NUM:
			*buf = '\0';
			if (*string)
				switch (scanf(string, " %d - %d ", &line_low, &line_high)) {
					case 0:
						line_low = 1;
						line_high = 999999;
						break;
					case 1:
						line_high = line_low;
						break;
				}
			else {
				line_low = 1;
				line_high = 999999;
			}

			if (line_low < 1) {
				SEND_TO_Q("Line numbers must be greater than 0.\r\n", d);
				return;
			}
			if (line_high < line_low) {
				SEND_TO_Q("That range is invalid.\r\n", d);
				return;
			}
			*buf = '\0';
			i = 1;
			total_len = 0;
			s = *d->str;
			while (s && i < line_low)
				if ((s = strchr(s, '\n')) != NULL) {
					i++;
					s++;
				}
			if (i < line_low || s == NULL) {
				SEND_TO_Q("Line(s) out of range; no buffer listing.\r\n", d);
				return;
			}
			t = s;
			while (s && i <= line_high)
				if ((s = strchr(s, '\n')) != NULL) {
					i++;
					total_len++;
					s++;
					temp = *s;
					*s = '\0';
					if (strlen(buf) + 7 < sizeof(buf)) {
						sprintf(buf + strlen(buf), "%4d:\r\n", (i-1));
					}
					if (strlen(buf) + strlen(t) < sizeof(buf)) {
						strcat(buf, t);
					}
					*s = temp;
					t = s;
				}
			if (strlen(buf) + strlen(t) < sizeof(buf)) {
				if (s && t) {
					temp = *s;
					*s = '\0';
					strcat(buf, t);
					*s = temp;
				}
				else if (t) {
					strcat(buf, t);
				}
			}

			page_string(d, show_color_codes(buf), TRUE);
			break;
		case PARSE_INSERT:
			half_chop(string, buf, buf2);
			if (*buf == '\0') {
				SEND_TO_Q("You must specify a line number before which to insert text.\r\n", d);
				return;
			}
			line_low = atoi(buf);
			strcat(buf2, "\r\n");

			i = 1;
			*buf = '\0';
			if ((s = *d->str) == NULL) {
				SEND_TO_Q("Buffer is empty, nowhere to insert.\r\n", d);
				return;
			}
			if (line_low > 0) {
				while (s && i < line_low) {
					if ((s = strchr(s, '\n')) != NULL) {
						i++;
						s++;
					}
				}
				
				if (!s) {
					msg_to_desc(d, "Invalid insert position.\r\n");
					return;
				}
				
				temp = *s;
				*s = '\0';
				if ((strlen(*d->str) + strlen(buf2) + strlen(s + 1) + 3) > d->max_str) {
					*s = temp;
					SEND_TO_Q("Insert text pushes buffer over maximum size, insert aborted.\r\n", d);
					return;
				}
				if (*d->str && **d->str)
					strcat(buf, *d->str);
				*s = temp;
				strcat(buf, buf2);
				if (s && *s)
					strcat(buf, s);
				RECREATE(*d->str, char, strlen(buf) + 3);

				strcpy(*d->str, buf);
				SEND_TO_Q("Line inserted.\r\n", d);
			}
			else {
				SEND_TO_Q("Line number must be higher than 0.\r\n", d);
				return;
			}
			break;
		case PARSE_EDIT:
			half_chop(string, buf, buf2);
			if (*buf == '\0') {
				SEND_TO_Q("You must specify a line number at which to change text.\r\n", d);
				return;
			}
			line_low = atoi(buf);
			strcat(buf2, "\r\n");

			i = 1;
			*buf = '\0';
			if ((s = *d->str) == NULL) {
				SEND_TO_Q("Buffer is empty, nothing to change.\r\n", d);
				return;
			}
			if (line_low > 0) {
				while (s && i < line_low)
					if ((s = strchr(s, '\n')) != NULL) {
						i++;
						s++;
					}
				if (s == NULL || i < line_low) {
					SEND_TO_Q("Line number out of range; change aborted.\r\n", d);
					return;
				}
				if (s != *d->str) {
					temp = *s;
					*s = '\0';
					strcat(buf, *d->str);
					*s = temp;
				}
				strcat(buf, buf2);
				if ((s = strchr(s, '\n')) != NULL) {
					s++;
					strcat(buf, s);
				}
				if (strlen(buf) > d->max_str) {
					SEND_TO_Q("Change causes new length to exceed buffer maximum size, aborted.\r\n", d);
					return;
				}
				RECREATE(*d->str, char, strlen(buf) + 3);
				strcpy(*d->str, buf);
				SEND_TO_Q("Line changed.\r\n", d);
			}
			else {
				SEND_TO_Q("Line number must be higher than 0.\r\n", d);
				return;
			}
			break;
		default:
			SEND_TO_Q("Invalid option.\r\n", d);
			syslog(SYS_ERROR, 0, TRUE, "SYSERR: invalid command passed to parse_action");
			return;
	}
}


int format_script(struct descriptor_data *d) {
	char nsc[MAX_CMD_LENGTH], *t, line[READ_SIZE];
	char *sc;
	size_t len = 0, nlen = 0, llen = 0;
	int indent = 0, indent_next = FALSE, found_case = FALSE, i, line_num = 0;

	if (!d->str || !*d->str)
		return FALSE;

	sc = strdup(*d->str); /* we work on a copy, because of strtok() */
	t = strtok(sc, "\n\r");
	*nsc = '\0';

	while (t) {
		line_num++;
		skip_spaces(&t);
		if (!strncasecmp(t, "if ", 3) || !strncasecmp(t, "switch ", 7)) {
			indent_next = TRUE;
		}
		else if (!strncasecmp(t, "while ", 6)) {
			found_case = TRUE;  /* so you can 'break' a loop without complains */
			indent_next = TRUE;
		}
		else if (!strncasecmp(t, "end", 3) || !strncasecmp(t, "done", 4)) {
			if (!indent) {
				msg_to_desc(d, "Unmatched 'end' or 'done' (line %d)!\r\n", line_num);
				free(sc);
				return FALSE;
			}
			indent--;
			indent_next = FALSE;
		}
		else if (!strncasecmp(t, "else", 4)) {
			if (!indent) {
				msg_to_desc(d, "Unmatched 'else' (line %d)!\r\n", line_num);
				free(sc);
				return FALSE;
			}
			indent--;
			indent_next = TRUE;
		}
		else if (!strncasecmp(t, "case", 4) || !strncasecmp(t, "default", 7)) {
			if (!indent) {
				msg_to_desc(d, "Case/default outside switch (line %d)!\r\n", line_num);
				free(sc);
				return FALSE;
			}
			indent_next = TRUE;
			found_case = TRUE;
		}
		else if (!strncasecmp(t, "break", 5)) {
			if (!found_case || !indent ) {
				msg_to_desc(d, "Break not in case (line %d)!\r\n", line_num);
				free(sc);
				return FALSE;
			}
			found_case = FALSE;
			indent--;
		}

		*line = '\0';
		for (nlen = 0, i = 0;i<indent;i++) {
			strncat(line, "  ", sizeof(line)-1);
			nlen += 2;
		}
		llen = snprintf(line + nlen, sizeof(line) - nlen, "%s\r\n", t);
		if (llen + nlen + len > d->max_str - 1 ) {
			msg_to_desc(d, "String too long, formatting aborted\r\n");
			free(sc);
			return FALSE;
		}
		len = len + nlen + llen;
		strcat(nsc, line);  /* strcat OK, size checked above */

		if (indent_next) {
			indent++;
			indent_next = FALSE;
		}
		t = strtok(NULL, "\n\r");
	}  

	if (indent) 
		msg_to_desc(d, "Unmatched if, while or switch ignored.\r\n");

	free(*d->str);
	*d->str = strdup(nsc);
	free(sc);

	return TRUE;
}


void format_text(char **ptr_string, int mode, descriptor_data *d, unsigned int maxlen) {
	int line_chars, startlen, len, cap_next = TRUE, cap_next_next = FALSE;
	char *flow, *start = NULL, temp;
	char formatted[MAX_STRING_LENGTH];

	if (d->max_str > MAX_STRING_LENGTH) {
		log("SYSERR: format_text: max_str is greater than buffer size.");
		return;
	}
	
	// ensure the original string ends with a \r\n -- the formatter requires it
	if (*ptr_string && (*ptr_string)[strlen(*ptr_string) - 1] != '\n' && strlen(*ptr_string) < maxlen + 2) {
		RECREATE(*ptr_string, char, strlen(*ptr_string) + 2);
		strcat(*ptr_string, "\r\n");
	}

	if ((flow = *ptr_string) == NULL)
		return;

	if (IS_SET(mode, FORMAT_INDENT)) {
		strcpy(formatted, "   ");
		line_chars = 3;
	}
	else {
		*formatted = '\0';
		line_chars = 0;
	}

	while (*flow) {
		while (*flow && strchr("\n\r\f\t\v ", *flow))
			flow++;

		if (*flow) {
			start = flow++;
			while (*flow && !strchr("\n\r\f\t\v .?!", *flow))
				flow++;
			if (cap_next_next) {
				cap_next_next = FALSE;
				cap_next = TRUE;
			}
			while (strchr(".!?", *flow)) {
				cap_next_next = TRUE;
				flow++;
			}
			temp = *flow;
			*flow = '\0';

			startlen = color_strlen(start);
			if (line_chars + startlen + 1 > 79) {
				strcat(formatted, "\r\n");
				line_chars = 0;
			}
			if (!cap_next) {
				if (line_chars > 0) {
					strcat(formatted, " ");
					line_chars++;
				}
			}
			else {
				cap_next = FALSE;
				*start = UPPER(*start);
			}
			line_chars += startlen;
			strcat(formatted, start);
			*flow = temp;
		}
		if (cap_next_next) {
			if (line_chars + 3 > 79) {
				strcat(formatted, "\r\n");
				line_chars = 0;
			}
			else {
				strcat(formatted, " ");
				line_chars += 1;
			}
		}
	}
	
	// prevent trailing double-crlf by removing all trailing space
	len = strlen(formatted);
	while (len > 0 && strchr("\n\r\f\t\v ", formatted[len-1])) {
		formatted[--len] = '\0';
	}
	
	// re-add a crlf
	strcat(formatted, "\r\n");

	if (strlen(formatted) + 1 > maxlen)
		formatted[maxlen-1] = '\0';
	RECREATE(*ptr_string, char, MIN(maxlen, strlen(formatted) + 1));
	strcpy(*ptr_string, formatted);
}


int replace_str(char **string, char *pattern, char *replacement, int rep_all, unsigned int max_size) {
	char *replace_buffer = NULL;
	char *flow, *jetsam, temp;
	int len, i;

	if ((strlen(*string) - strlen(pattern)) + strlen(replacement) > max_size)
		return -1;

	CREATE(replace_buffer, char, max_size);
	i = 0;
	jetsam = *string;
	flow = *string;
	*replace_buffer = '\0';

	if (rep_all) {
		while ((flow = (char *)strstr(flow, pattern)) != NULL) {
			i++;
			temp = *flow;
			*flow = '\0';
			if ((strlen(replace_buffer) + strlen(jetsam) + strlen(replacement)) > max_size) {
				i = -1;
				break;
			}
			strcat(replace_buffer, jetsam);
			strcat(replace_buffer, replacement);
			*flow = temp;
			flow += strlen(pattern);
			jetsam = flow;
		}
		strcat(replace_buffer, jetsam);
	}
	else {
		if ((flow = (char *)strstr(*string, pattern)) != NULL) {
			i++;
			flow += strlen(pattern);
			len = ((char *)flow - (char *)*string) - strlen(pattern);
			strncpy(replace_buffer, *string, len);
			strcat(replace_buffer, replacement);
			strcat(replace_buffer, flow);
		}
	}

	if (i <= 0)
		return 0;
	else {
		RECREATE(*string, char, strlen(replace_buffer) + 3);
		strcpy(*string, replace_buffer);
	}
	free(replace_buffer);
	return i;
}


ACMD(do_string_editor) {
	if (IS_NPC(ch) || !ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!ch->desc->str) {
		msg_to_char(ch, "You are not currently editing any text.\r\n");
	}
	else {
		string_add(ch->desc, argument);
	}
}
