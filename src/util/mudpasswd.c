/* ************************************************************************
*  file:  mudpasswd.c                                     EmpireMUD 2.0b1 *
*  Usage: changing passwords of chars in a Diku playerfile                *
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


int str_eq(char *s, char *t) {
	for (;;) {
		if (*s == 0 && *t == 0)
			return (TRUE);
		if (LOWER(*s) != LOWER(*t))
			return (FALSE);
		s++;
		t++;
	}
}


void pword(char *filename, char *name, char *password) {
	FILE *fl;
	struct char_file_u buf;
	int found = FALSE;
	long size;

	if (!(fl = fopen(filename, "r+"))) {
		perror(filename);
		exit(1);
	}
	fseek(fl, 0L, SEEK_END);
	size = ftell(fl);
	rewind(fl);
	if (size % sizeof(struct char_file_u)) {
		fprintf(stderr, "\aWARNING:  File size does not match structure, recompile mudpasswd.\n");
		fclose(fl);
		exit(1);
	}

	for (;;) {
		fread(&buf, sizeof(buf), 1, fl);
		if (feof(fl))
			break;

		if (str_eq(name, buf.name)) {
			found = TRUE;
			strncpy(buf.pwd, CRYPT(password, PASSWORD_SALT), MAX_PWD_LENGTH);
			if (fseek(fl, -1L * sizeof(buf), SEEK_CUR) != 0)
				perror("fseek");
			if (fwrite(&buf, sizeof(buf), 1, fl) != 1)
				perror("fwrite");
			if (fseek(fl, 0L, SEEK_CUR) != 0)
				perror("fseek");
		}
	}

	if (found)
		printf("%s password is now %s\n", name, password);
	else
		printf("%s not found\n", name);

	fclose(fl);
}


int main(int argc, char **argv) {
	if (argc != 4)
		fprintf(stderr, "Usage: %s playerfile character-name new-password\n", argv[0]);
	else
		pword(argv[1], argv[2], argv[3]);

	return (0);
}
