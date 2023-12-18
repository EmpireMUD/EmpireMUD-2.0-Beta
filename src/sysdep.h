/* ************************************************************************
*   File: sysdep.h                                        EmpireMUD 2.0b5 *
*  Usage: machine-specific defs based on values in conf.h (from configure)*
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/*
 * EmpireMUD uses the crypt(3) function to encrypt player passwords in the
 * playerfile so that they are never stored in plaintext form.  However,
 * due to U.S. export restrictions on machine-readable cryptographic
 * software, the crypt() function is not available on some operating
 * systems such as FreeBSD.  By default, the 'configure' script will
 * determine if you have crypt() available and enable or disable password
 * encryption appropriately.  #define NOCRYPT (by uncommenting the line
 * below) if you'd like to explicitly disable password encryption (i.e.,
 * if you have moved your MUD from an OS that does not support encryption
 * to one that does.)
 *
 * See running.doc for details.
 */

/* #define NOCRYPT */

/**************************************************************************/

/*
 * If you are porting EmpireMUD to a new (untested) platform and you find
 * that POSIX-standard non-blocking I/O does *not* work, you can define
 * the constant below to have Empire work around the problem.  Not having
 * non-blocking I/O can cause the MUD to freeze if someone types part of
 * a command while the MUD waits for the remainder of the command.
 *
 * NOTE: **DO** **NOT** use this constant unless you are SURE you understand
 * exactly what non-blocking I/O is, and you are SURE that your operating
 * system does NOT have it!  (The only UNIX system I've ever seen that has
 * broken POSIX non-blocking I/O is AIX 3.2.)  If your MUD is freezing but
 * you're not sure why, do NOT use this constant.  Use this constant ONLY
 * if you're sure that your MUD is freezing because of a non-blocking I/O
 * problem.
 *
 * See running.doc for details.
 */

/* #define POSIX_NONBLOCK_BROKEN */

/**************************************************************************/

/*
 * The Empire code prototypes library functions to avoid compiler warnings.
 * (Operating system header files *should* do this, but sometimes don't.)
 * However, Empire's prototypes cause the compilation to fail under some
 * combinations of operating systems and compilers.
 *
 * If your compiler reports "conflicting types" for functions, you need to
 * define this constant to turn off library function prototyping.  Note,
 * **DO** **NOT** blindly turn on this constant unless you're sure the
 * problem is type conflicts between my header files and the header files
 * of your operating system.  The error message will look something like
 * this:
 *
 * In file included from comm.c:14:
 *    sysdep.h:207: conflicting types for `random'
 * /usr/local/lib/gcc-lib/alpha-dec-osf3.2/2.7.2/include/stdlib.h:253:
 *    previous declaration of `random'
 *
 * See running.doc for details.
 */

/* #define NO_LIBRARY_PROTOTYPES */

/************************************************************************/
/*** Do not change anything below this line *****************************/
/************************************************************************/

/*
 * Set up various machine-specific things based on the values determined
 * from configure and conf.h.
 */

/* Standard C headers  *************************************************/

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif


#if     (defined (STDC_HEADERS) || defined (__GNU_LIBRARY__))
#include <stdlib.h>

#else   /* No standard headers.  */

#ifdef  HAVE_MEMORY_H
#include <memory.h>
#endif

extern char *malloc(), *calloc(), *realloc();
extern void free ();

extern void abort (), exit ();

#endif  /* Standard headers.  */

/* POSIX compliance  *************************************************/

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

/* Now, we #define POSIX if we have a POSIX system. */

#ifdef HAVE_UNISTD_H
/* Ultrix's unistd.h always defines _POSIX_VERSION, but you only get
   POSIX.1 behavior with `cc -YPOSIX', which predefines POSIX itself!  */
#if defined (_POSIX_VERSION) && !defined (ultrix)
#define POSIX
#endif

/* Some systems define _POSIX_VERSION but are not really POSIX.1.  */
#if (defined (butterfly) || defined (__arm) || \
     (defined (__mips) && defined (_SYSTYPE_SVR3)) || \
     (defined (sequent) && defined (i386)))
#undef POSIX
#endif
#endif /* HAVE_UNISTD_H */

#if !defined (POSIX) && defined (_AIX) && defined (_POSIX_SOURCE)
#define POSIX
#endif

#if defined(_AIX)
#define POSIX_NONBLOCK_BROKEN
#endif


/* Header files *******************************************************/

 
/* Header files common to all source files */

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_NET_ERRNO_H
#include <net/errno.h>
#endif

/* Macintosh */
#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_ASSERT_H
#include <assert.h>
#else
#define assert(arg)
#endif


/* Header files only used in comm.c and some of the utils */

#if defined(__COMM_C__) || defined(EMPIRE_UTIL)

#ifndef HAVE_STRUCT_IN_ADDR
struct in_addr {
  unsigned long int s_addr;	/* for inet_addr, etc. */
}
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif

#ifdef HAVE_SIGNAL_H
# ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 2
#  include <signal.h>
#  undef _POSIX_C_SOURCE
# else
#  include <signal.h>	/* GNU libc 6 already defines _POSIX_C_SOURCE. */
# endif
#endif

#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#endif /* __COMM_C__ && EMPIRE_UTIL */


/* Header files that are only used in act.other.c */
#ifdef __ACT_OTHER_C__

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#endif /* __ACT_OTHER_C__ */


/* Basic system dependencies *******************************************/

#if !defined(__GNUC__)
#define __attribute__(x)	/* nothing */
#endif

#if defined(__MWERKS__)
# define isascii(c)	(((c) & ~0x7f) == 0)	/* So easy to have, but ... */
#endif

/* Socket/header miscellany. */
/* SOCKET -- must be after the winsock.h #include. */
# define CLOSE_SOCKET(sock)	close(sock)
  typedef int			socket_t;

#if defined(__cplusplus)	/* C++ */
#define cpp_extern	extern
#else				/* C */
#define cpp_extern	/* Nothing */
#endif

/* Guess if we have the getrlimit()/setrlimit() functions */
#if defined(RLIMIT_NOFILE) || defined (RLIMIT_OFILE)
#define HAS_RLIMIT
#if !defined (RLIMIT_NOFILE)
# define RLIMIT_NOFILE RLIMIT_OFILE
#endif
#endif


/* Make sure we have STDERR_FILENO */
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

/* Make sure we have STDOUT_FILENO too. */
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif


/* Function prototypes ************************************************/

/*
 * For reasons that perplex me, the header files of many OS's do not contain
 * function prototypes for the standard C library functions.  This produces
 * annoying warning messages (sometimes, a huge number of them) on such OS's
 * when compiling with gcc's -Wall.
 *
 * Some versions of CircleMUD prior to 3.0 patchlevel 9 attempted to
 * include prototypes taken from OS man pages for a large number of
 * OS's in the header files.  I now think such an approach is a bad
 * idea: maintaining that list is very difficult and time-consuming,
 * and when new revisions of OS's are released with new header files,
 * Circle can break if the prototypes contained in Circle's .h files
 * differs from the new OS header files; for example, Circle 3.0
 * patchlevel 8 failed with compiler errors under Solaris 2.5 and
 * Linux 1.3.xx whereas under previous revisions of those OS's it had
 * been fine.
 *
 * Thus, to silence the compiler warnings but still maintain some level of
 * portability (albiet at the expense of worse error checking in the code),
 * my solution is to define a "typeless" function prototype for all problem
 * functions that have not already been prototyped by the OS. --JE
 *
 * 20 Mar 96: My quest is not yet over.  These definitions still cause
 * clashes with some compilers.  Therefore, we only use these prototypes
 * if we're using gcc (which makes sense, since they're only here for gcc's
 * -Wall option in the first place), and configure tells gcc to use
 * -fno-strict-prototypes, so that these definitions don't clash with
 * previous prototypes.
 *
 * 4 June 96: The quest continues.  OSF/1 still doesn't like these
 * prototypes, even with gcc and -fno-strict-prototypes.  I've created
 * the constant NO_LIBRARY_PROTOTYPES to allow people to turn off the
 * prototyping.
 *
 * 27 Oct 97: This is driving me crazy but I think I've finally come
 * up with the solution that will work.  I've changed the configure
 * script to detect which prototypes exist already; this header file
 * only prototypes functions that aren't already prototyped by the
 * system headers.  A clash should be impossible.  This should give us
 * our strong type-checking back.  This should be the last word on
 * this issue!
 */

#ifndef NO_LIBRARY_PROTOTYPES

#ifdef NEED_ATOI_PROTO
   int atoi(const char *str);
#endif

#ifdef NEED_ATOL_PROTO
   long atol(const char *str);
#endif

/*
 * bzero is deprecated - use memset() instead.  Not directly used in Empire
 * but the prototype needed for FD_xxx macros on some machines.
 */
#ifdef NEED_BZERO_PROTO
     void bzero(char *b, int length);
#endif
 
#ifdef NEED_CRYPT_PROTO
   char *crypt(const char *key, const char *salt);
#endif

#ifdef NEED_FCLOSE_PROTO
   int fclose(FILE *stream);
#endif

#ifdef NEED_FDOPEN_PROTO
   FILE *fdopen(int fd, const char *mode);
#endif

#ifdef NEED_FFLUSH_PROTO
   int fflush(FILE *stream);
#endif

#ifdef NEED_FPRINTF_PROTO
   int fprintf(FILE *strm, const char *format, /* args */ ... );
#endif

#ifdef NEED_FREAD_PROTO
   size_t fread(void *ptr, size_t size, size_t nitems, FILE *stream);
#endif

#ifdef NEED_FSCANF_PROTO
  int fscanf(FILE *strm, const char *format, ...);
#endif

#ifdef NEED_FSEEK_PROTO
   int fseek(FILE *stream, long offset, int ptrname);
#endif

#ifdef NEED_FWRITE_PROTO
  size_t fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream);
#endif

#ifdef NEED_GETPID_PROTO
   pid_t getpid(void);
#endif

#ifdef NEED_PERROR_PROTO
   void perror(const char *s);
#endif

#ifdef NEED_QSORT_PROTO
   void qsort(void *base, size_t nel, size_t width,
          int (*compar) (const void *, const void *));
#endif

#ifdef NEED_REWIND_PROTO
   void rewind(FILE *stream);
#endif

#ifdef NEED_SPRINTF_PROTO
   int sprintf(char *s, const char *format, /* args */ ... );
#endif

#ifdef NEED_SSCANF_PROTO
   int sscanf(const char *s, const char *format, ...);
#endif

#ifdef NEED_STRERROR_PROTO
   char *strerror(int errnum);
#endif

#ifdef NEED_SYSTEM_PROTO
   int system(const char *string);
#endif

#ifdef NEED_TIME_PROTO
   time_t time(time_t *tloc);
#endif

#ifdef NEED_UNLINK_PROTO
   int unlink(const char *path);
#endif

#ifdef NEED_REMOVE_PROTO
   int remove(const char *path);
#endif

/* Function prototypes that are only used in comm.c and some of the utils */

#if defined(__COMM_C__) || defined(EMPIRE_UTIL)

#ifdef NEED_ACCEPT_PROTO
   int accept(socket_t s, struct sockaddr *addr, int *addrlen);
#endif

#ifdef NEED_BIND_PROTO
   int bind(socket_t s, const struct sockaddr *name, int namelen);
#endif

#ifdef NEED_CHDIR_PROTO
   int chdir(const char *path);
#endif

#ifdef NEED_CLOSE_PROTO
   int close(int fildes);
#endif

#ifdef NEED_FCNTL_PROTO
   int fcntl(int fildes, int cmd, /* arg */ ...);
#endif

#ifdef NEED_FPUTC_PROTO
   int fputc(char c, FILE *stream);
#endif

#ifdef NEED_FPUTS_PROTO
   int fputs(const char *s, FILE *stream);
#endif

#ifdef NEED_GETPEERNAME_PROTO
   int getpeername(socket_t s, struct sockaddr *name, int *namelen);
#endif

#if defined(HAS_RLIMIT) && defined(NEED_GETRLIMIT_PROTO)
   int getrlimit(int resource, struct rlimit *rlp);
#endif

#ifdef NEED_GETSOCKNAME_PROTO
   int getsockname(socket_t s, struct sockaddr *name, int *namelen);
#endif

#ifdef NEED_GETTIMEOFDAY_PROTO
   int gettimeofday(struct timeval *tp, void * );
#endif

#ifdef NEED_HTONL_PROTO
   ulong htonl(u_long hostlong);
#endif

#ifdef NEED_HTONS_PROTO
   u_short htons(u_short hostshort);
#endif

#if defined(HAVE_INET_ADDR) && defined(NEED_INET_ADDR_PROTO)
   unsigned long int inet_addr(const char *cp);
#endif

#if defined(HAVE_INET_ATON) && defined(NEED_INET_ATON_PROTO)
   int inet_aton(const char *cp, struct in_addr *inp);
#endif

#ifdef NEED_INET_NTOA_PROTO
   char *inet_ntoa(const struct in_addr in);
#endif

#ifdef NEED_LISTEN_PROTO
   int listen(socket_t s, int backlog);
#endif

#ifdef NEED_NTOHL_PROTO
   u_long ntohl(u_long netlong);
#endif

#ifdef NEED_PRINTF_PROTO
   int printf(char *format, ...);
#endif

#ifdef NEED_READ_PROTO
   ssize_t read(int fildes, void *buf, size_t nbyte);
#endif

#ifdef NEED_SELECT_PROTO
   int select(int nfds, fd_set *readfds, fd_set *writefds,
          fd_set *exceptfds, struct timeval *timeout);   
#endif

#ifdef NEED_SETITIMER_PROTO
   int setitimer(int which, const struct itimerval *value,
          struct itimerval *ovalue);
#endif

#if defined(HAS_RLIMIT) && defined(NEED_SETRLIMIT_PROTO)
   int setrlimit(int resource, const struct rlimit *rlp);
#endif

#ifdef NEED_SETSOCKOPT_PROTO
   int setsockopt(socket_t s, int level, int optname, const char *optval,
		  int optlen);
#endif

#ifdef NEED_SOCKET_PROTO
   int socket(int domain, int type, int protocol);
#endif

#ifdef NEED_WRITE_PROTO
    ssize_t write(int fildes, const void *buf, size_t nbyte);
#endif

#endif /* __COMM_C__ */


#endif /* NO_LIBRARY_PROTOTYPES */


