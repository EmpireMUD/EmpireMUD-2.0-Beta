/* ************************************************************************
*   File: boards.h                                        EmpireMUD 2.0b5 *
*  Usage: header file for bulletin boards                                 *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define NUM_OF_BOARDS		2		/* change if needed! */
#define MAX_BOARD_MESSAGES 	60      /* arbitrary -- change if needed */
#define MAX_MESSAGE_LENGTH	4096	/* arbitrary -- change if needed */

#define INDEX_SIZE	   ((NUM_OF_BOARDS*MAX_BOARD_MESSAGES) + 5)

#define BOARD_MAGIC	1048575			/* arbitrary number - see modify.c */

struct board_msginfo {
	int	slot_num;		/* pos of message in "master index"				*/
	char *heading;		/* pointer to message's heading					*/
	int reply_num;		/* slot_num of message this is a reply for		*/
	int	level;			/* level of poster								*/
	int	heading_len;	/* size of header (for file write)				*/
	int	message_len;	/* size of message text (for file write)		*/
};

struct board_info_type {
	obj_vnum vnum;	/* vnum of this board */
	int	read_lvl;	/* min level to read messages on this board */
	int	write_lvl;	/* min level to write messages on this board */
	int	remove_lvl;	/* min level to remove messages from this board */
	char filename[50];	/* file to save this board to */
};

#define BOARD_VNUM(i) (board_info[i].vnum)
#define READ_LVL(i) (board_info[i].read_lvl)
#define WRITE_LVL(i) (board_info[i].write_lvl)
#define REMOVE_LVL(i) (board_info[i].remove_lvl)
#define FILENAME(i) (board_info[i].filename)

#define NEW_MSG_INDEX(i)  (msg_index[i][num_of_msgs[i]])
#define MSG_HEADING(i, j)  (msg_index[i][j].heading)
#define MSG_SLOTNUM(i, j)  (msg_index[i][j].slot_num)
#define MSG_REPLY(i, j)  (msg_index[i][j].reply_num)
#define MSG_LEVEL(i, j)  (msg_index[i][j].level)

int Board_display_msg(int board_type, char_data *ch, char *arg, obj_data *board);
int Board_show_board(int board_type, char_data *ch, char *arg, obj_data *board);
int Board_remove_msg(int board_type, char_data *ch, char *arg, obj_data *board);
int Board_write_message(int board_type, char_data *ch, char *arg, obj_data *board);
int Board_respond_message(int board_type, char_data *ch, char *arg, obj_data *board);
void Board_display_response(int board_type, int slot_num, char *output, obj_data *board, bool reply);
void Board_save_board(int board_type);
void Board_load_board(int board_type);
void Board_reset_board(int board_num);
