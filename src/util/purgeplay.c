/* ************************************************************************
*  file: purgeplay.c                                      EmpireMUD 2.0b2 * 
*  Usage: purge useless chars from playerfile                             *
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


void purge(char *filename) {
	FILE *fl;
	FILE *outfile;
	struct char_file_u player;
	int okay, num = 0;
	long timeout, size;
	char *ptr, reason[80];

	if (!(fl = fopen(filename, "r+"))) {
		printf("Can't open %s.", filename);
		exit(1);
	}
	fseek(fl, 0L, SEEK_END);
	size = ftell(fl);
	rewind(fl);
	if (size % sizeof(struct char_file_u)) {
		fprintf(stderr, "\aWARNING:  File size does not match structure, recompile purgeplay.\n");
		fclose(fl);
		exit(1);
	}

	outfile = fopen("players.new", "w");
	printf("Deleting: \n");

	for (;;) {
		fread(&player, sizeof(struct char_file_u), 1, fl);
		if (feof(fl)) {
			fclose(fl);
			fclose(outfile);
			printf("Done.\n");
			exit(0);
		}
		okay = 1;
		*reason = '\0';

		for (ptr = player.name; *ptr; ptr++) {
			if (*ptr == ' ') {
				okay = 0;
				strcpy(reason, "Invalid name");
			}
			if (player.access_level == 0) {
				okay = 0;
				strcpy(reason, "Never entered game");
			}
			if (player.access_level < 0 || player.access_level > LVL_IMPL) {
				okay = 0;
				strcpy(reason, "Invalid level");
			}
			/* now, check for timeouts.  Immortals do not time out.  */
			timeout = 1000;

			if (okay && player.access_level <= LVL_START_IMM) {
				if (player.access_level == 1)					timeout = 30;
				else if (player.access_level <= LVL_GOD)		timeout = 60;
				else									timeout = 90;
			}
			else
				timeout = 90;

			timeout *= SECS_PER_REAL_DAY;

			if ((time(0) - player.last_logon) > timeout) {
				okay = 0;
				sprintf(reason, "Level %2d idle for %3ld days", player.access_level, ((time(0) - player.last_logon) / SECS_PER_REAL_DAY));
			}
		}
		if (player.char_specials_saved.act & PLR_DELETED) {
			okay = 0;
			sprintf(reason, "Deleted flag set");
		}

		/* Don't delete for *any* of the above reasons if they have NODELETE */
		if (!okay && (player.char_specials_saved.act & PLR_NODELETE)) {
			okay = 2;
			strcat(reason, "; NOT deleted.");
		}
		if (okay)
			fwrite(&player, sizeof(struct char_file_u), 1, outfile);
		else
			printf("%4d. %-20s %s\n", ++num, player.name, reason);

		if (okay == 2)
			fprintf(stderr, "%-20s %s\n", player.name, reason);
	}
}


int main(int argc, char *argv[]) {
	if (argc != 2)
		printf("Usage: %s playerfile-name\n", argv[0]);
	else
		purge(argv[1]);

	return (0);
}
