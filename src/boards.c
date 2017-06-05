/* ************************************************************************
*   File: boards.c                                        EmpireMUD 2.0b5 *
*  Usage: handling of multiple bulletin boards                            *
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
#include "boards.h"
#include "interpreter.h"
#include "handler.h"
#include "vnums.h"

/* Board appearance order. */
#define	NEWEST_AT_TOP	TRUE

int board_loaded = 0;


/* FEATURES & INSTALLATION INSTRUCTIONS ***********************************
 *
 * This board code has many improvements over the infamously buggy standard
 * Diku board code.  Features include:
 *
 * - Arbitrary number of boards handled by one set of generalized routines.
 *   Adding a new board is as easy as adding another entry to an array.
 * - Safe removal of messages while other messages are being written.
 * - Does not allow messages to be removed by someone of a level less than
 *   the poster's level.
 * - Messages listed from bottom to top, or top to bottom
 * - Replies to messages
 *
 *
 * TO ADD A NEW BOARD, simply follow our easy 2-step program:
 *
 * 1 - Increase the NUM_OF_BOARDS constant in boards.h
 *
 * 2 - Add a new line to the board_info array below.  The fields, in order, are:
 *   Board's virtual number.
 *   Min level one must be to look at this board or read messages on it.
 *   Min level one must be to post a message to the board.
 *   Min level one must be to remove other people's messages from this
 *     board (but you can always remove your own message).
 *   Filename of this board, in quotes.
 *   Last field must always be 0.
 *
 */


struct board_info_type board_info[NUM_OF_BOARDS] = {
	{	BOARD_MORT,	0,				0,				LVL_ASST,	LIB_BOARD "mort" },
	{	BOARD_IMM,	LVL_START_IMM,	LVL_START_IMM,	LVL_CIMPL,	LIB_BOARD "immort" }
};


char *msg_storage[INDEX_SIZE];
int msg_storage_taken[INDEX_SIZE];
int num_of_msgs[NUM_OF_BOARDS];
struct board_msginfo msg_index[NUM_OF_BOARDS][MAX_BOARD_MESSAGES];


int find_slot(void) {
	int i;

	for (i = 0; i < INDEX_SIZE; i++)
		if (!msg_storage_taken[i]) {
			msg_storage_taken[i] = 1;
			return (i);
		}
	return (-1);
}


/* search the room ch is standing in to find which board he's looking at */
int find_board(char_data *ch) {
	obj_data *obj;
	int i;

	for (obj = ROOM_CONTENTS(IN_ROOM(ch)); obj; obj = obj->next_content)
		for (i = 0; i < NUM_OF_BOARDS; i++)
			if (BOARD_VNUM(i) == GET_OBJ_VNUM(obj))
				return (i);

	return (-1);
}


void init_boards(void) {
	int i, j, fatal_error = 0;

	for (i = 0; i < INDEX_SIZE; i++) {
		msg_storage[i] = 0;
		msg_storage_taken[i] = 0;
	}

	for (i = 0; i < NUM_OF_BOARDS; i++) {
		if (!obj_proto(BOARD_VNUM(i))) {
			log("SYSERR: Fatal board error: board vnum %d does not exist!", BOARD_VNUM(i));
			fatal_error = 1;
		}
		num_of_msgs[i] = 0;
		for (j = 0; j < MAX_BOARD_MESSAGES; j++) {
			memset((char *) &(msg_index[i][j]), 0, sizeof(struct board_msginfo));
			msg_index[i][j].slot_num = -1;
		}
		Board_load_board(i);
	}

	if (fatal_error)
		exit(1);
}


ACMD(do_write) {
	obj_data *board;
	int board_type;

	if (!board_loaded) {
		init_boards();
		board_loaded = 1;
	}
	if (!ch->desc)
		return;

	for (board = ROOM_CONTENTS(IN_ROOM(ch)); board; board = board->next_content)
		if (GET_OBJ_TYPE(board) == ITEM_BOARD)
			break;
	if (!board)
		msg_to_char(ch, "There's no board here.\r\n");
	else if ((board_type = find_board(ch)) == -1) {
		log("SYSERR:  degenerate board!  (what the hell...)");
		msg_to_char(ch, "There's no board here.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("write_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (ACCOUNT_FLAGGED(ch, ACCT_MUTED))
		msg_to_char(ch, "You can't write on boards while muted.\r\n");
	else if (!Board_write_message(board_type, ch, argument, board))
		msg_to_char(ch, "There's no board here.\r\n");
}


ACMD(do_read) {
	void show_obj_to_char(obj_data *obj, char_data *ch, int mode);
	void read_book(char_data *ch, obj_data *obj);
	
	obj_data *board, *obj;
	int board_type;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Read what?\r\n");
		return;
	}
	
	// try to find a book or mail in inventory
	if (*argument && (obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		if (IS_BOOK(obj)) {
			read_book(ch, obj);
			return;
		}
		else if (GET_OBJ_TYPE(obj) == ITEM_MAIL) {
			show_obj_to_char(obj, ch, OBJ_DESC_LOOK_AT);
			return;
		}
	}

	if (!board_loaded) {
		init_boards();
		board_loaded = 1;
	}
	if (!ch->desc)
		return;

	for (board = ROOM_CONTENTS(IN_ROOM(ch)); board; board = board->next_content)
		if (GET_OBJ_TYPE(board) == ITEM_BOARD)
			break;
	if (!board)
		msg_to_char(ch, "You don't have anything like that to read.\r\n");
	else if ((board_type = find_board(ch)) == -1) {
		log("SYSERR:  degenerate board!  (what the hell...)");
		msg_to_char(ch, "There's no board here.\r\n");
	}
	else if (!Board_display_msg(board_type, ch, argument, board))
		msg_to_char(ch, "There's no board here.\r\n");
}


ACMD(do_respond) {
	obj_data *board;
	int board_type;

	if (!board_loaded) {
		init_boards();
		board_loaded = 1;
	}
	if (!ch->desc)
		return;

	for (board = ROOM_CONTENTS(IN_ROOM(ch)); board; board = board->next_content)
		if (GET_OBJ_TYPE(board) == ITEM_BOARD)
			break;
	if (!board)
		msg_to_char(ch, "There's no board here.\r\n");
	else if ((board_type = find_board(ch)) == -1) {
		log("SYSERR:  degenerate board!  (what the hell...)");
		msg_to_char(ch, "There's no board here.\r\n");
	}
	else if (ACCOUNT_FLAGGED(ch, ACCT_MUTED))
		msg_to_char(ch, "You can't write on boards while muted.\r\n");
	else if (!Board_respond_message(board_type, ch, argument, board))
		msg_to_char(ch, "There's no board here.\r\n");
}


int Board_write_message(int board_type, char_data *ch, char *arg, obj_data *board) {
	char *tmstr;
	time_t ct;
	char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];

	if (!ch->desc) {
		return 1;
	}
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing some other text.\r\n");
		return 1;
	}
	if (GET_ACCESS_LEVEL(ch) < WRITE_LVL(board_type)) {
		send_to_char("You are not holy enough to write on this board.\r\n", ch);
		return (1);
	}
	if (num_of_msgs[board_type] >= MAX_BOARD_MESSAGES) {
		send_to_char("The board is full.\r\n", ch);
		return (1);
	}
	if ((NEW_MSG_INDEX(board_type).slot_num = find_slot()) == -1) {
		send_to_char("The board is malfunctioning - sorry.\r\n", ch);
		log("SYSERR: Board: failed to find empty slot on write.");
		return (1);
	}
	/* skip blanks */
	skip_spaces(&arg);
	delete_doubledollar(arg);

	/* JE 27 Oct 95 - Truncate headline at 80 chars if it's longer than that */
	arg[80] = '\0';

	if (!*arg) {
		send_to_char("We must have a headline!\r\n", ch);
		return (1);
	}
	ct = time(0);
	tmstr = (char *) asctime(localtime(&ct));
	*(tmstr + strlen(tmstr) - 1) = '\0';

	sprintf(buf2, "(%s%s&0)", GET_ACCESS_LEVEL(ch) == LVL_IMPL ? "&y" : GET_ACCESS_LEVEL(ch) == LVL_GOD ? "&c" : "&0", GET_NAME(ch));
	sprintf(buf, "%6.10s %-14s :: %s", tmstr, buf2, arg);
	NEW_MSG_INDEX(board_type).heading = str_dup(buf);
	NEW_MSG_INDEX(board_type).level = GET_ACCESS_LEVEL(ch);
	NEW_MSG_INDEX(board_type).reply_num = -1;

	act("$n starts to write a message.", TRUE, ch, 0, 0, TO_ROOM);

	start_string_editor(ch->desc, "your message", &(msg_storage[NEW_MSG_INDEX(board_type).slot_num]), MAX_MESSAGE_LENGTH, FALSE);
	ch->desc->mail_to = board_type + BOARD_MAGIC;

	num_of_msgs[board_type]++;
	return (1);
}


int Board_show_board(int board_type, char_data *ch, char *arg, obj_data *board) {
	int i;
	char tmp[MAX_STRING_LENGTH], buf[MAX_STRING_LENGTH];

	if (!ch->desc)
		return (0);

	one_argument(arg, tmp);

	if (!*tmp || !isname(tmp, GET_OBJ_KEYWORDS(board)))
		return (0);

	if (GET_ACCESS_LEVEL(ch) < READ_LVL(board_type)) {
		send_to_char("You try but fail to understand the holy words.\r\n", ch);
		return (1);
	}
	// this results in a double message with "$n looks at $p."
	//act("$n studies the board.", TRUE, ch, 0, 0, TO_ROOM);

	strcpy(buf,
		"This is a bulletin board. Usage: READ/REMOVE <messg #>, WRITE <header>.\r\n"
		"You will need to look at the board to save your message.\r\n");
	if (!num_of_msgs[board_type])
		strcat(buf, "The board is empty.\r\n");
	else {
		sprintf(buf + strlen(buf), "There are %d messages on the board.\r\n", num_of_msgs[board_type]);
#if NEWEST_AT_TOP
		for (i = num_of_msgs[board_type] - 1; i >= 0; i--) {
#else
		for (i = 0; i < num_of_msgs[board_type]; i++) {
#endif
			if (MSG_HEADING(board_type, i)) {
				if (MSG_REPLY(board_type, i) == -1)
					Board_display_response(board_type, i, buf, board, 0);
			}
			else {
				log("SYSERR: The board is fubar'd.");
				send_to_char("Sorry, the board isn't working.\r\n", ch);
				return (1);
			}
		}
	}
	page_string(ch->desc, buf, 1);

	return (1);
}


void Board_display_response(int board_type, int slot_num, char *output, obj_data *board, bool reply) {
	int k;
	char buf[MAX_STRING_LENGTH];

#if NEWEST_AT_TOP
	sprintf(buf, "%-2d : %s\r\n", num_of_msgs[board_type] - slot_num, MSG_HEADING(board_type, slot_num));
#else
	sprintf(buf, "%-2d : %s\r\n", slot_num + 1, MSG_HEADING(board_type, slot_num));
#endif
	strcat(output, buf);

	for (k = slot_num; k < num_of_msgs[board_type]; k++)
		if (MSG_REPLY(board_type, k) == slot_num)
			Board_display_response(board_type, k, output, board, 1);
}


int Board_display_msg(int board_type, char_data *ch, char *arg, obj_data *board) {
	char number[MAX_STRING_LENGTH], buffer[MAX_STRING_LENGTH];
	int msg, ind;

	one_argument(arg, number);
	if (!*number)
		return (0);
	if (isname(number, GET_OBJ_KEYWORDS(board)))	/* so "read board" works */
		return (Board_show_board(board_type, ch, arg, board));
	if (strchr(number, '.'))	/* read 2.mail, look 2.sword */
		return (0);
	if (!isdigit(*number) || (!(msg = atoi(number))))
		return (0);

	if (GET_ACCESS_LEVEL(ch) < READ_LVL(board_type)) {
		send_to_char("You try but fail to understand the holy words.\r\n", ch);
		return (1);
	}
	if (!num_of_msgs[board_type]) {
		send_to_char("The board is empty!\r\n", ch);
		return (1);
	}
	if (msg < 1 || msg > num_of_msgs[board_type]) {
		send_to_char("That message exists only in your imagination.\r\n", ch);
		return (1);
	}
	#if NEWEST_AT_TOP
		ind = num_of_msgs[board_type] - msg;
	#else
		ind = msg - 1;
	#endif
	if (MSG_SLOTNUM(board_type, ind) < 0 || MSG_SLOTNUM(board_type, ind) >= INDEX_SIZE) {
		send_to_char("Sorry, the board is not working.\r\n", ch);
		log("SYSERR: Board is screwed up. (Room #%d)", GET_ROOM_VNUM(IN_ROOM(ch)));
		return (1);
	}
	if (!(MSG_HEADING(board_type, ind))) {
		send_to_char("That message appears to be screwed up.\r\n", ch);
		return (1);
	}
	if (!(msg_storage[MSG_SLOTNUM(board_type, ind)])) {
		send_to_char("That message seems to be empty.\r\n", ch);
		return (1);
	}
	sprintf(buffer, "Message %d : %s\r\n\r\n%s", msg,
	MSG_HEADING(board_type, ind),
	msg_storage[MSG_SLOTNUM(board_type, ind)]);

	page_string(ch->desc, buffer, 1);

	return (1);
}


void perform_remove_message(int board_type, int slot_num, int ind) {
	int k;
	bool found = FALSE;
	descriptor_data *d;

	for (k = 0; k < num_of_msgs[board_type]; k++, found = FALSE) {
		if (MSG_REPLY(board_type, k) > ind)
			MSG_REPLY(board_type, k) -= 1;
		else if (MSG_REPLY(board_type, k) == slot_num) {
			for (d = descriptor_list; d; d = d->next)
				if (STATE(d) == CON_PLAYING && d->str == &(msg_storage[MSG_SLOTNUM(board_type, k)]))
					found = TRUE;
			if (!found)
				perform_remove_message(board_type, MSG_SLOTNUM(board_type, k), k);
		}
	}

	if (msg_storage[slot_num])
		free(msg_storage[slot_num]);
	msg_storage[slot_num] = 0;
	msg_storage_taken[slot_num] = 0;
	if (MSG_HEADING(board_type, ind))
		free(MSG_HEADING(board_type, ind));
	MSG_REPLY(board_type, ind) = -1;

	for (; ind < num_of_msgs[board_type] - 1; ind++) {
		MSG_HEADING(board_type, ind) = MSG_HEADING(board_type, ind + 1);
		MSG_SLOTNUM(board_type, ind) = MSG_SLOTNUM(board_type, ind + 1);
		MSG_LEVEL(board_type, ind) = MSG_LEVEL(board_type, ind + 1);
		MSG_REPLY(board_type, ind) = MSG_REPLY(board_type, ind + 1);
	}
	num_of_msgs[board_type]--;
}


int Board_remove_msg(int board_type, char_data *ch, char *arg, obj_data *board) {
	int ind, msg, slot_num;
	char number[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH];
	descriptor_data *d;

	one_argument(arg, number);

	if (!*number || !isdigit(*number))
		return (0);
	if (!(msg = atoi(number)))
		return (0);

	if (!num_of_msgs[board_type]) {
		send_to_char("The board is empty!\r\n", ch);
		return (1);
	}
	if (msg < 1 || msg > num_of_msgs[board_type]) {
		send_to_char("That message exists only in your imagination.\r\n", ch);
		return (1);
	}
#if NEWEST_AT_TOP
	ind = num_of_msgs[board_type] - msg;
#else
	ind = msg - 1;
#endif
	if (!MSG_HEADING(board_type, ind)) {
		send_to_char("That message appears to be screwed up.\r\n", ch);
		return (1);
	}
	sprintf(buf, "%s", GET_NAME(ch));
	if (GET_ACCESS_LEVEL(ch) < REMOVE_LVL(board_type) && !(strstr(MSG_HEADING(board_type, ind), buf))) {
		send_to_char("You are not holy enough to remove other people's messages.\r\n", ch);
		return (1);
	}
	if (GET_ACCESS_LEVEL(ch) < MSG_LEVEL(board_type, ind)) {
		send_to_char("You can't remove a message holier than yourself.\r\n", ch);
		return (1);
	}
	slot_num = MSG_SLOTNUM(board_type, ind);
	if (slot_num < 0 || slot_num >= INDEX_SIZE) {
		send_to_char("That message is majorly screwed up.\r\n", ch);
		log("SYSERR: The board is seriously screwed up. (Room #%d)", GET_ROOM_VNUM(IN_ROOM(ch)));
		return (1);
	}
	for (d = descriptor_list; d; d = d->next)
		if (STATE(d) == CON_PLAYING && d->str == &(msg_storage[slot_num])) {
			send_to_char("At least wait until the author is finished before removing it!\r\n", ch);
			return (1);
		}
	perform_remove_message(board_type, slot_num, ind);
	send_to_char("Message removed.\r\n", ch);
	sprintf(buf, "$n just removed message %d.", msg);
	act(buf, FALSE, ch, 0, 0, TO_ROOM);
	Board_save_board(board_type);

	return (1);
}


void Board_save_board(int board_type) {
	FILE *fl;
	int i;
	char *tmp1, *tmp2 = NULL;

	if (!num_of_msgs[board_type]) {
		remove(FILENAME(board_type));
		return;
	}
	if (!(fl = fopen(FILENAME(board_type), "wb"))) {
		perror("SYSERR: Error writing board");
		return;
	}
	fwrite(&(num_of_msgs[board_type]), sizeof(int), 1, fl);

	for (i = 0; i < num_of_msgs[board_type]; i++) {
		if ((tmp1 = MSG_HEADING(board_type, i)) != NULL)
			msg_index[board_type][i].heading_len = strlen(tmp1) + 1;
		else
			msg_index[board_type][i].heading_len = 0;

		if (MSG_SLOTNUM(board_type, i) < 0 || MSG_SLOTNUM(board_type, i) >= INDEX_SIZE || (!(tmp2 = msg_storage[MSG_SLOTNUM(board_type, i)])))
			msg_index[board_type][i].message_len = 0;
		else
			msg_index[board_type][i].message_len = strlen(tmp2) + 1;

		fwrite(&(msg_index[board_type][i]), sizeof(struct board_msginfo), 1, fl);
		if (tmp1)
			fwrite(tmp1, sizeof(char), msg_index[board_type][i].heading_len, fl);
		if (tmp2)
			fwrite(tmp2, sizeof(char), msg_index[board_type][i].message_len, fl);
	}

	fclose(fl);
}


void Board_load_board(int board_type) {
	FILE *fl;
	int i, len1, len2;
	char *tmp1, *tmp2;

	if (!(fl = fopen(FILENAME(board_type), "rb"))) {
		if (errno != ENOENT)
			perror("SYSERR: Error reading board");
		return;
	}
	fread(&(num_of_msgs[board_type]), sizeof(int), 1, fl);
	if (num_of_msgs[board_type] < 1 || num_of_msgs[board_type] > MAX_BOARD_MESSAGES) {
		log("SYSERR: Board file %d corrupt. Resetting.", board_type);
		Board_reset_board(board_type);
		return;
	}
	for (i = 0; i < num_of_msgs[board_type]; i++) {
		fread(&(msg_index[board_type][i]), sizeof(struct board_msginfo), 1, fl);
		if ((len1 = msg_index[board_type][i].heading_len) <= 0) {
			log("SYSERR: Board file %d corrupt! Resetting.", board_type);
			Board_reset_board(board_type);
			return;
		}
		CREATE(tmp1, char, len1);
		fread(tmp1, sizeof(char), len1, fl);
		MSG_HEADING(board_type, i) = tmp1;

		if ((MSG_SLOTNUM(board_type, i) = find_slot()) == -1) {
			log("SYSERR: Out of slots booting board %d! Resetting...", board_type);
			Board_reset_board(board_type);
			return;
		}
		if ((len2 = msg_index[board_type][i].message_len) > 0) {
			CREATE(tmp2, char, len2);
			fread(tmp2, sizeof(char), len2, fl);
			msg_storage[MSG_SLOTNUM(board_type, i)] = tmp2;
		}
		else
			msg_storage[MSG_SLOTNUM(board_type, i)] = NULL;
	}

	fclose(fl);
}


void Board_reset_board(int board_type) {
	int i;

	for (i = 0; i < MAX_BOARD_MESSAGES; i++) {
		if (MSG_HEADING(board_type, i))
			free(MSG_HEADING(board_type, i));
		if (msg_storage[MSG_SLOTNUM(board_type, i)])
			free(msg_storage[MSG_SLOTNUM(board_type, i)]);
		msg_storage_taken[MSG_SLOTNUM(board_type, i)] = 0;
		memset((char *)&(msg_index[board_type][i]),0,sizeof(struct board_msginfo));
		msg_index[board_type][i].slot_num = -1;
	}
	num_of_msgs[board_type] = 0;
	remove(FILENAME(board_type));
}


int Board_respond_message(int board_type, char_data *ch, char *arg, obj_data *board) {
	char *tmstr;
	time_t ct;
	int msg, ind;
	char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH], number[MAX_STRING_LENGTH];

	if (!ch->desc) {
		return 1;
	}
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing some other text.\r\n");
		return 1;
	}
	if (GET_ACCESS_LEVEL(ch) < WRITE_LVL(board_type)) {
		send_to_char("You are not holy enough to write on this board.\r\n", ch);
		return (1);
	}
	if (num_of_msgs[board_type] >= MAX_BOARD_MESSAGES) {
		send_to_char("The board is full.\r\n", ch);
		return (1);
	}
	if ((NEW_MSG_INDEX(board_type).slot_num = find_slot()) == -1) {
		send_to_char("The board is malfunctioning - sorry.\r\n", ch);
		log("SYSERR: Board: failed to find empty slot on write.");
		return (1);
	}
	/* skip blanks */
	skip_spaces(&arg);
	delete_doubledollar(arg);

	arg = one_argument(arg, number);

	/* JE 27 Oct 95 - Truncate headline at 80 chars if it's longer than that */
	arg[80] = '\0';

	if (!*number) {
		msg_to_char(ch, "Usage: respond <number> <headline>\r\n");
		return 1;
	}
	if (!*arg) {
		send_to_char("We must have a headline!\r\n", ch);
		return (1);
	}

	if (!isdigit(*number) || (!(msg = atoi(number)))) {
		msg_to_char(ch, "What message are you trying to respond to?\r\n");
		return 1;
	}
	if (msg < 1 || msg > num_of_msgs[board_type]) {
		msg_to_char(ch, "That message exists only in your imagination.\r\n");
		return 1;
	}

#if NEWEST_AT_TOP
	ind = num_of_msgs[board_type] - msg;
#else
	ind = msg - 1;
#endif

	if (MSG_SLOTNUM(board_type, ind) < 0 || MSG_SLOTNUM(board_type, ind) >= INDEX_SIZE) {
		msg_to_char(ch, "Sorry, the board is not working.\r\n");
		log("SYSERR: Board is screwed up.");
		return 1;
	}
	if (MSG_REPLY(board_type, ind) != -1) {
		msg_to_char(ch, "You can't respond to a response.\r\n");
		return 1;
	}

	ct = time(0);
	tmstr = (char *) asctime(localtime(&ct));
	*(tmstr + strlen(tmstr) - 1) = '\0';

	sprintf(buf2, "(%s%s&0)", GET_ACCESS_LEVEL(ch) == LVL_IMPL ? "&y" : GET_ACCESS_LEVEL(ch) == LVL_GOD ? "&c" : "&0", GET_NAME(ch));
	sprintf(buf, "%6.10s %-14s :: -%s", tmstr, buf2, arg);
	NEW_MSG_INDEX(board_type).heading = str_dup(buf);
	NEW_MSG_INDEX(board_type).level = GET_ACCESS_LEVEL(ch);
	NEW_MSG_INDEX(board_type).reply_num = MSG_SLOTNUM(board_type, ind);
	
	act("$n starts to write a message.", TRUE, ch, 0, 0, TO_ROOM);
	start_string_editor(ch->desc, "your message", &(msg_storage[NEW_MSG_INDEX(board_type).slot_num]), MAX_MESSAGE_LENGTH, FALSE);
	ch->desc->mail_to = board_type + BOARD_MAGIC;

	num_of_msgs[board_type]++;
	return (1);
}
