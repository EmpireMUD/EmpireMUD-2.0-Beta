/* ************************************************************************
*  file:  cryptpasswd.c                                   EmpireMUD 2.0b5 *
*  Usage: generates an encrypted password for manually editing pfiles     *
*                                                                         *
*  You can edit player's passwords directly in the playerfiles, but they  *
*  are encrypted and to successfully set a password in the file, you      *
*  must use this utility to generate the encrypted form. Use this syntax  *
*  to generate the password:                                              *
*                                                                         *
*  > ./cryptpasswd <plaintext password>                                   *
*                                                                         *
*  NOTE: Editing a pfile while the player is in-game will have no effect  *
*                                                                         *
*  You can also set passwords in-game using "set file <name> password"    *
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


int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <password>\n", argv[0]);
		return 1;
	}
	
	printf("Password: %s\n", CRYPT(argv[1], PASSWORD_SALT));
	
	return 0;
}
