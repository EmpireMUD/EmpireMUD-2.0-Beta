/* ************************************************************************
*   File: ban.c                                           EmpireMUD 2.0b5 *
*  Usage: banning/unbanning/checking sites and player names               *
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


struct ban_list_element *ban_list = NULL;

const char *ban_types[] = {
	"no",
	"new",
	"select",
	"all",
	"ERROR"
};


void load_banned(void) {
	FILE *fl;
	int i, date;
	char site_name[BANNED_SITE_LENGTH + 1], ban_type[100];
	char name[MAX_NAME_LENGTH + 1];
	struct ban_list_element *next_node;

	ban_list = 0;

	if (!(fl = fopen(BAN_FILE, "r"))) {
		if (errno != ENOENT)
			log("SYSERR: Unable to open banfile '%s': %s", BAN_FILE, strerror(errno));
		else
			log("   Ban file '%s' doesn't exist.", BAN_FILE);
		return;
	}
	while (fscanf(fl, " %s %s %d %s ", ban_type, site_name, &date, name) == 4) {
		CREATE(next_node, struct ban_list_element, 1);
		strncpy(next_node->site, site_name, BANNED_SITE_LENGTH);
		next_node->site[BANNED_SITE_LENGTH] = '\0';
		strncpy(next_node->name, name, MAX_NAME_LENGTH);
		next_node->name[MAX_NAME_LENGTH] = '\0';
		next_node->date = date;

		for (i = BAN_NOT; i <= BAN_ALL; i++)
			if (!str_cmp(ban_type, ban_types[i]))
				next_node->type = i;

		next_node->next = ban_list;
		ban_list = next_node;
	}

	fclose(fl);
}


int isbanned(char *hostname) {
	int i;
	struct ban_list_element *banned_node;
	char *nextchar;

	if (!hostname || !*hostname)
		return (0);

	i = 0;
	for (nextchar = hostname; *nextchar; nextchar++)
		*nextchar = LOWER(*nextchar);

	for (banned_node = ban_list; banned_node; banned_node = banned_node->next)
		if (strstr(hostname, banned_node->site))	/* if hostname is a substring */
			i = MAX(i, banned_node->type);

	return (i);
}


void _write_one_node(FILE * fp, struct ban_list_element * node) {
	if (node) {
		_write_one_node(fp, node->next);
		fprintf(fp, "%s %s %ld %s\n", ban_types[node->type], node->site, (long) node->date, node->name);
	}
}


void write_ban_list(void) {
	FILE *fl;

	if (!(fl = fopen(BAN_FILE, "w"))) {
		perror("SYSERR: Unable to open '" BAN_FILE "' for writing");
		return;
	}
	_write_one_node(fl, ban_list);/* recursively write from end to start */
	fclose(fl);
	return;
}


ACMD(do_ban) {
	char flag[MAX_INPUT_LENGTH], site[MAX_INPUT_LENGTH], format[MAX_INPUT_LENGTH], *nextchar, *timestr;
	int i;
	struct ban_list_element *ban_node;

	*buf = '\0';

	if (!*argument) {
		if (!ban_list) {
			send_to_char("No sites are banned.\r\n", ch);
			return;
		}
		strcpy(format, "%-25s  %-8.8s  %-10.10s  %-16.16s\r\n");
		msg_to_char(ch, format, "Banned Site Name", "Ban Type", "Banned On", "Banned By");
		msg_to_char(ch, format, "---------------------------------", "---------------------------------", "---------------------------------", "---------------------------------");

		for (ban_node = ban_list; ban_node; ban_node = ban_node->next) {
			if (ban_node->date) {
				timestr = asctime(localtime(&(ban_node->date)));
				*(timestr + 10) = 0;
				strcpy(site, timestr);
			}
			else
				strcpy(site, "Unknown");
			msg_to_char(ch, format, ban_node->site, ban_types[ban_node->type], site, ban_node->name);
		}
		return;
	}
	two_arguments(argument, flag, site);
	if (!*site || !*flag) {
		send_to_char("Usage: ban {all | select | new} site_name\r\n", ch);
		return;
	}
	if (!(!str_cmp(flag, "select") || !str_cmp(flag, "all") || !str_cmp(flag, "new"))) {
		send_to_char("Flag must be ALL, SELECT, or NEW.\r\n", ch);
		return;
	}
	for (ban_node = ban_list; ban_node; ban_node = ban_node->next) {
		if (!str_cmp(ban_node->site, site)) {
			send_to_char("That site has already been banned -- unban it to change the ban type.\r\n", ch);
			return;
		}
	}

	CREATE(ban_node, struct ban_list_element, 1);
	strncpy(ban_node->site, site, BANNED_SITE_LENGTH);
	for (nextchar = ban_node->site; *nextchar; nextchar++)
		*nextchar = LOWER(*nextchar);
	ban_node->site[BANNED_SITE_LENGTH] = '\0';
	strncpy(ban_node->name, GET_NAME(ch), MAX_NAME_LENGTH);
	ban_node->name[MAX_NAME_LENGTH] = '\0';
	ban_node->date = time(0);

	for (i = BAN_NEW; i <= BAN_ALL; i++)
		if (!str_cmp(flag, ban_types[i]))
			ban_node->type = i;

	ban_node->next = ban_list;
	ban_list = ban_node;

	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "BAN: %s has banned %s for %s players", GET_NAME(ch), site, ban_types[ban_node->type]);
	send_to_char("Site banned.\r\n", ch);
	write_ban_list();
}


ACMD(do_unban) {
	char site[MAX_INPUT_LENGTH];
	struct ban_list_element *ban_node, *temp;
	int found = 0;

	one_argument(argument, site);
	if (!*site) {
		send_to_char("A site to unban might help.\r\n", ch);
		return;
	}
	ban_node = ban_list;
	while (ban_node && !found) {
		if (!str_cmp(ban_node->site, site))
			found = 1;
		else
			ban_node = ban_node->next;
	}

	if (!found) {
		send_to_char("That site is not currently banned.\r\n", ch);
		return;
	}
	REMOVE_FROM_LIST(ban_node, ban_list, next);
	send_to_char("Site unbanned.\r\n", ch);
	syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "BAN: %s removed the %s-player ban on %s", GET_NAME(ch), ban_types[ban_node->type], ban_node->site);

	free(ban_node);
	write_ban_list();
}


/**************************************************************************
 *  Code to check for invalid names (i.e., profanity, etc.)               *
 *  Written by Sharon P. Goza                                             *
 **************************************************************************/

char *invalid_list[MAX_INVALID_NAMES];
int num_invalid = 0;

int Valid_Name(char *newname) {
	int i;
	descriptor_data *dt;
	char tempname[MAX_INPUT_LENGTH];

	/*
	 * Make sure someone isn't trying to create this same name.  We want to
	 * do a 'str_cmp' so people can't do 'Bob' and 'BoB'.  The creating login
	 * will not have a character name yet and other people sitting at the
	 * prompt won't have characters yet.
	 */
	for (dt = descriptor_list; dt; dt = dt->next)
		if (dt->character && GET_NAME(dt->character) && !str_cmp(GET_NAME(dt->character), newname))
			return (STATE(dt) == CON_PLAYING || STATE(dt) == CON_RMOTD);

	/* return valid if list doesn't exist */
	if (num_invalid < 1)
		return (1);

	/* change to lowercase */
	strcpy(tempname, newname);
	for (i = 0; tempname[i]; i++)
		tempname[i] = LOWER(tempname[i]);

	/* Does the desired name contain a string in the invalid list? */
	for (i = 0; i < num_invalid; i++) {
		if (*invalid_list[i] == '%') {	// leading % means substr
			if (strstr(tempname, invalid_list[i] + 1)) {
				return (0);
			}
		}
		else {	// otherwise exact-match
			if (!str_cmp(tempname, invalid_list[i])) {
				return (0);
			}
		}
	}

	return (1);
}


void Read_Invalid_List(void) {
	FILE *fp;
	char temp[256];

	if (!(fp = fopen(XNAME_FILE, "r"))) {
		perror("SYSERR: Unable to open '" XNAME_FILE "' for reading");
		return;
	}

	num_invalid = 0;
	while (get_line(fp, temp) && num_invalid < MAX_INVALID_NAMES)
		invalid_list[num_invalid++] = str_dup(temp);

	if (num_invalid >= MAX_INVALID_NAMES) {
		log("SYSERR: Too many invalid names; change MAX_INVALID_NAMES in ban.c");
		exit(1);
	}

	fclose(fp);
}
