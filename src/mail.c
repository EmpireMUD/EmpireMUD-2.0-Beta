/* ************************************************************************
*   File: mail.c                                          EmpireMUD 2.0b5 *
*  Usage: Internal funcs and handlers of mud-mail system                  *
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
#include "db.h"
#include "interpreter.h"
#include "handler.h"

/**
* Contents:
*   Input / Output
*   Mail Commands
*/


 //////////////////////////////////////////////////////////////////////////////
//// INPUT / OUTPUT //////////////////////////////////////////////////////////

/**
* Free one letter.
*
* @param struct mail_data *mail The item to free.
*/
void free_mail(struct mail_data *mail) {
	if (mail->body) {
		free(mail->body);
	}
	free(mail);
}


/**
* Parses a single mail entry from a player file. You must pass the first line
* into this function, as it contains the basic mail data.
*
* @param FILE *fl The open player file.
* @param char *first_line The line that begins with "Mail:".
* @return struct mail_data* The mail entry, or NULL if it couldn't be read.
*/
struct mail_data *parse_mail(FILE *fl, char *first_line) {
	char line[MAX_INPUT_LENGTH], err[256];
	struct mail_data *mail;
	
	if (!fl || !first_line || !*first_line) {
		log("SYSERR: parse_mail called without %s", fl ? "first line" : "file");
		return NULL;
	}
	
	CREATE(mail, struct mail_data, 1);
	
	if (sscanf(first_line, "Mail: %d %ld", &mail->from, &mail->timestamp) != 2) {
		log("SYSERR: parse_mail: invalid data on Mail line");
		free(mail);
		return NULL;
	}
	
	strcpy(err, "parse_mail");
	mail->body = fread_string(fl, err);
	
	// extra flags
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in parse_mail");
			free(mail);
			return NULL;
		}
		switch (*line) {
			case 'E': {
				if (!strn_cmp(line, "End Mail", 8)) {
					// mail complete
					return mail;
				}
				break;
			}
		}
	}
	
	log("SYSERR: parse_mail: reached end without 'End Mail' tag");
	
	free(mail);
	return NULL;
}


/**
* Writes a player's mail to an open playerfile, starting with a "Mail:" tag,
* and ending with an "End Mail" tag.
*
* Mail is part of the delayed data.
*
* @param FILE *fl The open playerfile, for writing.
* @param char_data *ch The player whose mail to save.
*/
void write_mail_to_file(FILE *fl, char_data *ch) {
	char temp[MAX_STRING_LENGTH];
	struct mail_data *mail;
	
	if (!fl || !ch) {
		log("SYSERR: write_mail_to_file called without %s", fl ? "character" : "file");
		return;
	}
	
	for (mail = GET_MAIL_PENDING(ch); mail; mail = mail->next) {
		// empty? don't bother
		if (!mail->body) {
			continue;
		}
		
		fprintf(fl, "Mail: %d %ld\n", mail->from, mail->timestamp);
		
		strcpy(temp, mail->body);
		strip_crlf(temp);
		fprintf(fl, "%s~\n", temp);
		
		// extra flags?
		
		fprintf(fl, "End Mail\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MAIL COMMANDS ///////////////////////////////////////////////////////////

ACMD(do_mail) {
	char mail_buf[MAX_STRING_LENGTH * 2];
	player_index_data *index;
	int amt = -1, count = 0;
	struct mail_data *mail;
	char *tmstr, **write;
	obj_data *obj;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	two_arguments(argument, arg, buf);
	
	if (!IS_APPROVED(ch) && config_get_bool("write_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!*arg) {
		msg_to_char(ch, "Usage: mail send <name>, mail check, mail receive <number>\r\n");
	}
	else if (is_abbrev(arg, "check")) {
		if (GET_MAIL_PENDING(ch)) {
			msg_to_char(ch, "A pigeon is circling overhead with mail for you.\r\n");
		}
		else {
			msg_to_char(ch, "You can't see any mail pigeons following you.\r\n");
		}
	}
	else if (is_abbrev(arg, "receive")) {
		if (*buf) {
			amt = MAX(1, atoi(buf));
		}

		if (!GET_MAIL_PENDING(ch)) {
			msg_to_char(ch, "You don't have any mail waiting.\r\n");
		}
		else {
			while ((mail = GET_MAIL_PENDING(ch)) && amt-- && ++count) {
				obj = create_obj();
				GET_OBJ_KEYWORDS(obj) = str_dup("letter small mail");
				GET_OBJ_SHORT_DESC(obj) = str_dup("a small letter");
				GET_OBJ_LONG_DESC(obj) = str_dup("Someone has left a small letter here.");
				GET_OBJ_TYPE(obj) = ITEM_MAIL;
				GET_OBJ_WEAR(obj) = ITEM_WEAR_TAKE | ITEM_WEAR_HOLD;
				GET_OBJ_TIMER(obj) = UNLIMITED;
				
				index = find_player_index_by_idnum(mail->from);
				tmstr = asctime(localtime(&mail->timestamp));
				*(tmstr + strlen(tmstr) - 1) = '\0';

				snprintf(mail_buf, sizeof(mail_buf),
					" * * * * Empire Mail System * * * *\r\n"
					"Date: %s\r\n"
					"  To: %s\r\n"
					"From: %s\r\n\r\n%s", tmstr, PERS(ch, ch, TRUE),
						index ? index->fullname : "Unknown", NULLSAFE(mail->body));
				GET_OBJ_ACTION_DESC(obj) = str_dup(mail_buf);
				
				obj_to_char(obj, ch);
				
				GET_MAIL_PENDING(ch) = mail->next;
				free_mail(mail);
			}
			msg_to_char(ch, "A pigeon lands and you find %d letter%s attached to its leg.\r\n", count, count != 1 ? "s" : "");
			sprintf(buf1, "A pigeon lands and $n unties %d letter%s from its leg.", count, count != 1 ? "s" : "");
			act(buf1, TRUE, ch, NULL, NULL, TO_ROOM);
		}
	}
	else if (is_abbrev(arg, "send")) {
		if (!IS_IMMORTAL(ch) && (!HAS_FUNCTION(IN_ROOM(ch), FNC_MAIL) || !IS_COMPLETE(IN_ROOM(ch)))) {
			msg_to_char(ch, "You can only send mail from a pigeon post.\r\n");
		}
		else if (!IS_IMMORTAL(ch) && !check_in_city_requirement(IN_ROOM(ch), TRUE)) {
			msg_to_char(ch, "This building must be in a city to use it.\r\n");
		}
		else if (!ch->desc) {
			msg_to_char(ch, "You can't do that.\r\n");
		}
		else if (ch->desc->str) {
			msg_to_char(ch, "You are already editing some text. Type ,/h for help.\r\n");
		}
		else if (!*buf) {
			msg_to_char(ch, "To whom will you be sending a letter?\r\n");
		}
		else if (!(index = find_player_index_by_name(buf))) {
			msg_to_char(ch, "Nobody by that name is registered here!\r\n");
		}
		else {
			act("$n starts to write some mail.", TRUE, ch, 0, 0, TO_ROOM);
			SET_BIT(PLR_FLAGS(ch), PLR_MAILING);
			CREATE(write, char *, 1);
			start_string_editor(ch->desc, "your message", write, MAX_MAIL_SIZE, FALSE);
			ch->desc->mail_to = index->idnum;
		}
	}
	else {
		msg_to_char(ch, "Usage: mail send <name>, mail check, mail receive <number>\r\n");
	}
}
