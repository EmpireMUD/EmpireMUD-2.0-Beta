/* ************************************************************************
*  file:  delobjs.c                                       EmpireMUD 2.0b2 *
*  Usage: deleting object files for players who are not in the playerfile *
*                                                                         *
*  Written by Jeremy Elson 4/2/93                                         *
*  All Rights Reserved                                                    *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */
/*
 * I recommend you use the script in the lib/plrobjs directory instead of
 * invoking this program directly; however, you can use this program thusly:
 *
 * usage: switch into an obj directory; type: delobjs <plrfile> <obj wildcard>
 */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"

struct name_element {
	char name[20];
	struct name_element *next;
};

struct name_element *name_list = 0;

void do_purge(int argc, char **argv) {
	int x, found;
	struct name_element *tmp;
	char name[1024];

	for (x = 2; x < argc; x++) {
		found = 0;
		strcpy(name, argv[x]);
		*(strchr(name, '.')) = '\0';
		for (tmp = name_list; !found && tmp; tmp = tmp->next)
			if (!strcmp(tmp->name, name))
				found = 1;
			if (!found) {
				remove(argv[x]);
				printf("Deleting %s\n", argv[x]);
			}
	}
}


int main(int argc, char **argv) {
	char *ptr;
	struct char_file_u player;
	int okay;
	struct name_element *tmp;
	FILE *fl;

	if (argc < 3) {
		printf("Usage: %s <playerfile-name> <file1> <file2> ... <filen>\n", argv[0]);
		exit(1);
	}
	if (!(fl = fopen(argv[1], "rb"))) {
		perror("Unable to open playerfile for reading");
		exit(1);
	}
	while (1) {
		fread(&player, sizeof(player), 1, fl);

		if (feof(fl)) {
			fclose(fl);
			do_purge(argc, argv);
			exit(0);
		}
		okay = 1;

		for (ptr = player.name; *ptr; ptr++)
			*ptr = LOWER(*ptr);

		if (player.char_specials_saved.act & PLR_DELETED)
			okay = 0;

		if (okay) {
			tmp = (struct name_element *) malloc(sizeof(struct name_element));
			tmp->next = name_list;
			strcpy(tmp->name, player.name);
			name_list = tmp;
		}
	}
}
