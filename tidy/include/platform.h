/* platform.h

  (c) 1998-2001 (W3C) MIT, INRIA, Keio University
  See tidy.c for the copyright notice.

  CVS Info :

    $Author$ 
    $Date$ 
    $Revision$ 

*/

/*
  Uncomment and edit this #define if you want
  to specify the config file at compile-time

#define CONFIG_FILE "/etc/tidy_config.txt"
*/

/*
  Uncomment this if you are on a Unix system supporting
  the call getpwnam() and the HOME environment variable.
  It enables tidy to find config files named ~/.tidyrc
  and ~your/.tidyrc etc if the HTML_TIDY environment
  variable is not set. Contributed by Todd Lewis.

#define SUPPORT_GETPWNAM
*/

#include <ctype.h>
#include <stdio.h>
#include <setjmp.h>  /* for longjmp on error exit */
#include <stdlib.h>
#include <stdarg.h>  /* may need <varargs.h> for Unix V */
#include <string.h>
#include <assert.h>

#ifdef NEEDS_MALLOC_H
#include <malloc.h>
#endif

#ifdef SUPPORT_GETPWNAM
#include <pwd.h>
#endif

#ifdef NEEDS_UNISTD_H
#include <unistd.h>  /* needed for unlink on some Unix systems */
#endif

/*
 Tidy preserves the last modified time for the files it
 cleans up. If your platform doesn't support <sys/utime.h>
 and the futime function, then set PRESERVEFILETIMES to 0
*/
#ifndef PRESERVEFILETIMES
#ifdef sun
#define PRESERVEFILETIMES 0
#else
#define PRESERVEFILETIMES 1
#endif
#endif

#if PRESERVEFILETIMES
#include <sys/types.h> 
#include <sys/stat.h>
#ifdef __linux__
#include <utime.h>
#else
#include <sys/utime.h>
#endif /* __linux__ */

/*
   MS Windows needs _ prefix for Unix file functions
   Tidy uses for preserving the lasted modified time
*/
#ifdef _WIN32
#define futime _futime
#define fstat _fstat
#define utimbuf _utimbuf
#define stat _stat
#endif /* _WIN32 */
#endif /* PRESERVEFILETIMES */

/* hack for gnu sys/types.h file  which defines uint and ulong */
/* you may need to delete the #ifndef and #endif on your system */

#ifndef __USE_MISC
#if defined(sun) ||defined(__FreeBSD__) ||defined(__NetBSD__) ||defined(__OpenBSD__) ||defined(__MACH__)
#include <sys/types.h>
#else
#ifndef _INCLUDE_HPUX_SOURCE
typedef unsigned int uint;
#endif /* _INCLUDE_HPUX_SOURCE */
typedef unsigned long ulong;
#endif /* BSDs */
#endif  /* __USE_MISC */
typedef unsigned char byte;
           
/*
  bool is a reserved word in some but
  not all C++ compilers depending on age
  work around is to avoid bool altogether
  by introducing a new enum called Bool
*/
typedef enum
{
   no,
   yes
} Bool;

/* for null pointers */
#define null 0

/*
  portability hack for deleting files - this is used
  in pprint.c for deleting superfluous slides.

  Win32 defines _unlink as per Unix unlink function.
  Except, MSVC will not recognize unlink() w/ 
  language extensions disable (i.e. pure ANSI mode).

  WINDOWS automatically set by Win16 compilers.
  _WIN32 automatically set by Win32 compilers.
*/

#if  defined(WINDOWS) || defined(_WIN32)
#define unlink _unlink
#define fileno _fileno
#endif

#if defined(DMALLOC)
#include "dmalloc.h"
#endif

/* were defined in html.h - TRT */
void *MemAlloc(uint size);
void *MemRealloc(void *mem, uint newsize);
void MemFree(void *mem);
void ClearMemory(void *, uint size);
