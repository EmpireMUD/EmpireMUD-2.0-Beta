/* ************************************************************************
*   File: random.c                                        EmpireMUD 2.0b5 *
*  Usage: pseudo-random number generator                                  *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */
// clean

/*
 * I am bothered by the non-portablility of 'rand' and 'random' -- rand
 * is ANSI C, but on some systems such as Suns, rand has seriously tragic
 * spectral properties (the low bit alternates between 0 and 1!).  random
 * is better but isn't supported by all systems.  So, in my quest for Ultimate
 * CircleMUD Portability, I decided to include this code for a simple but
 * relatively effective random number generator.  It's not the best RNG code
 * around, but I like it because it's very short and simple, and for our
 * purposes it's "random enough".
 *               --Jeremy Elson  2/23/95
 *
 * Well, despite autoconf we're back to using this random all the
 * time.  Oh well, there's no harm in changing my mind on this one
 * from release to release...  -JE 10/28/97
 *
 * I've changed the name from circle_random to empire_random to match my
 * new code base, but it's exactly the same.  Nothing new.  -PC 11/06/2k
 */

/***************************************************************************/

/*
 *
 * This program is public domain and was written by William S. England
 * (Oct 1988).  It is based on an article by:
 *
 * Stephen K. Park and Keith W. Miller. RANDOM NUMBER GENERATORS:
 * GOOD ONES ARE HARD TO FIND. Communications of the ACM,
 * New York, NY.,October 1988 p.1192

 The following is a portable c program for generating random numbers.
 The modulus and multipilier have been extensively tested and should
 not be changed except by someone who is a professional Lehmer generator
 writer.  THIS GENERATOR REPRESENTS THE MINIMUM STANDARD AGAINST WHICH
 OTHER GENERATORS SHOULD BE JUDGED. ("Quote from the referenced article's
 authors. WSE" )
*/

#define	m  (unsigned long)2147483647
#define	q  (unsigned long)127773

#define	a (unsigned int)16807
#define	r (unsigned int)2836

/*
** F(z)	= (az)%m
**	= az-m(az/m)
**
** F(z)  = G(z)+mT(z)
** G(z)  = a(z%q)- r(z/q)
** T(z)  = (z/q) - (az/m)
**
** F(z)  = a(z%q)- rz/q+ m((z/q) - a(z/m))
** 	 = a(z%q)- rz/q+ m(z/q) - az
*/

static unsigned long seed;


void empire_srandom(unsigned long initial_seed) {
    seed = initial_seed;
}


unsigned long empire_random(void) {
	register int lo, hi, test;

	hi = seed/q;
	lo = seed%q;

	test = a*lo - r*hi;

	if (test > 0)
		seed = test;
	else
		seed = test+ m;

	return (seed);
}
