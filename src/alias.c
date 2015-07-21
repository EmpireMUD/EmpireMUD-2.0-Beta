/* ************************************************************************
*   File: alias.c                                         EmpireMUD 2.0b2 *
*  Usage: writing/reading player's aliases.                               *
*                                                                         *
*  Code done by Jeremy Hess and Chad Thompson                             *
*  Modifed by George Greer for inclusion into CircleMUD bpl15.            *
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
#include "db.h"


void write_aliases(char_data *ch) {
	FILE *file;
	char fn[MAX_STRING_LENGTH];
	struct alias_data *temp;

	get_filename(GET_NAME(ch), fn, ALIAS_FILE);
	remove(fn);

	if (GET_ALIASES(ch) == NULL)
		return;

	if ((file = fopen(fn, "w")) == NULL) {
		log("SYSERR: Couldn't save aliases for %s in '%s'.", GET_NAME(ch), fn);
		perror("SYSERR: write_aliases");
		return;
	}

	for (temp = GET_ALIASES(ch); temp; temp = temp->next) {
		int aliaslen = strlen(temp->alias);
		int repllen = strlen(temp->replacement) - 1;

		fprintf(file, "%d\n%s\n"	/* Alias */
					  "%d\n%s\n"	/* Replacement */
					  "%d\n",		/* Type */
			aliaslen, temp->alias,
			repllen, temp->replacement + 1,
			temp->type);
	}
  
	fclose(file);
}

void read_aliases(char_data *ch) {   
	FILE *file;
	char xbuf[MAX_STRING_LENGTH];
	struct alias_data *t2;
	int length;

	get_filename(GET_NAME(ch), xbuf, ALIAS_FILE);

	if ((file = fopen(xbuf, "r")) == NULL) {
		if (errno != ENOENT) {
			log("SYSERR: Couldn't open alias file '%s' for %s.", xbuf, GET_NAME(ch));
			perror("SYSERR: read_aliases");
		}
		return;
	}
 
	CREATE(GET_ALIASES(ch), struct alias_data, 1);
	t2 = GET_ALIASES(ch); 

	for (;;) {
		/* Read the aliased command. */
		fscanf(file, "%d\n", &length);
		fgets(xbuf, length + 1, file);
		t2->alias = str_dup(xbuf);

		/* Build the replacement. */
		fscanf(file, "%d\n", &length);
		*xbuf = ' ';		/* Doesn't need terminated, fgets() will. */
		fgets(xbuf + 1, length + 1, file);
		t2->replacement = str_dup(xbuf); 

		/* Figure out the alias type. */
		fscanf(file, "%d\n", &length);
		t2->type = length; 

		if (feof(file))
			break;

		CREATE(t2->next, struct alias_data, 1);
		t2 = t2->next;
	}

	fclose(file);
} 
