/* ************************************************************************
*  file:  showplay.c                                      EmpireMUD 2.0b2 *
*  Usage: list a diku playerfile                                          *
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


void show(char *filename) {
	char sexname;
	FILE *fl;
	struct char_file_u player;
	int num = 0;
	long size;

	if (!(fl = fopen(filename, "r+"))) {
		perror("error opening playerfile");
		exit(1);
	}
	fseek(fl, 0L, SEEK_END);
	size = ftell(fl);
	rewind(fl);
	if (size % sizeof(struct char_file_u)) {
		fprintf(stderr, "\aWARNING:  File size does not match structure, recompile showplay.\n");
		fclose(fl);
		exit(1);
	}

	for (;;) {
		fread(&player, sizeof(struct char_file_u), 1, fl);
		if (feof(fl)) {
			fclose(fl);
			exit(0);
		}

		switch (player.sex) {
			case SEX_FEMALE:	sexname = 'F';			break;
			case SEX_MALE:		sexname = 'M';			break;
			case SEX_NEUTRAL:	sexname = 'N';			break;
			default:			sexname = '-';			break;
		}

		printf("%5d. ID: %5d (%c) [%2d] %-16s\n", ++num, player.char_specials_saved.idnum, sexname, player.access_level, player.name);
	}
}


int main(int argc, char **argv) {
	if (argc != 2)
		printf("Usage: %s playerfile-name\n", argv[0]);
	else
		show(argv[1]);

	return (0);
}
