/* platform.h

  (c) 1998-2001 (W3C) MIT, INRIA, Keio University
  See tidy.c for the copyright notice.

  CVS Info :

    $Author$ 
    $Date$ 
    $Revision$ 

*/

/*
  Uncomment and edit one of the following #defines if you
  want to specify the config file at compile-time.
*/

/* #define CONFIG_FILE "/etc/tidy_config.txt" */ /* original */
/* #define CONFIG_FILE "/etc/tidyrc" */
/* #define CONFIG_FILE "/etc/tidy.conf" */

/*
  Uncomment the following #define if you are on a Unix system
  supporting the call getpwnam() and the HOME environment variable.
  It enables tidy to find config files named ~/.tidyrc and
  ~your/.tidyrc etc if the HTML_TIDY environment
  variable is not set. Contributed by Todd Lewis.
*/

/* #define SUPPORT_GETPWNAM */

/* Enable/disable support for Big5 and Shift_JIS character encodings */
#ifndef SUPPORT_ASIAN_ENCODINGS
#define SUPPORT_ASIAN_ENCODINGS 0
#endif

/* Enable/disable support for UTF-16 character encodings */
#ifndef SUPPORT_UTF16_ENCODINGS
#define SUPPORT_UTF16_ENCODINGS 0
#endif

/* Convenience defines for Mac platform */

#if defined(macintosh)
/* Mac OS 6.x/7.x/8.x/9.x, with or without CarbonLib - MPW or Metrowerks 68K/PPC compilers */
#define MAC_OS_CLASSIC
#endif
#if defined(linux) && defined(powerpc)
/* MkLinux on PPC  - gcc (egcs) compiler */
#define MAC_OS_MKLINUX
#endif
#if defined(__APPLE__) && defined(__MACH__)
/* Mac OS X (client) 10.x (or server 1.x/10.x) - gcc or Metrowerks MachO compilers */
#define MAC_OS_X
#endif
#if defined(MAC_OS_CLASSIC) || defined(MAC_OS_MKLINUX) || defined(MAC_OS_X)
/* Any OS on Mac platform */
#define MAC_OS
#endif

/* Convenience defines for BSD like platforms */
 
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define BSD_BASED_OS
#endif

/* Convenience defines for Windows platforms */
 
#if defined(WINDOWS) || defined(_WIN32)
#define WINDOWS_OS
#endif

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
  cleans up.
*/

/*
  If your platform doesn't support <utime.h> and the
  utime() function, or <sys/futime> and the futime()
  function then set PRESERVE_FILE_TIMES to 0.
  
  If your platform doesn't support <sys/utime.h> and the
  futime() function, then set HAS_FUTIME to 0.
  
  If your platform supports <utime.h> and the
  utime() function requires the file to be
  closed first, then set UTIME_NEEDS_CLOSED_FILE to 1.
*/

/* Keep old PRESERVEFILETIMES define for compatibility */
#ifdef PRESERVEFILETIMES
#undef PRESERVE_FILE_TIMES
#define PRESERVE_FILE_TIMES PRESERVEFILETIMES
#endif

#ifndef PRESERVE_FILE_TIMES
#define PRESERVE_FILE_TIMES 1
#endif

#if PRESERVE_FILE_TIMES

#ifndef HAS_FUTIME
#if defined(sun) || defined(__linux__) || defined(BSD_BASED_OS) || defined(MAC_OS) || defined(__MSL__)
#define HAS_FUTIME 0
#else
#define HAS_FUTIME 1
#endif
#endif

#ifndef UTIME_NEEDS_CLOSED_FILE
#if defined(sun) || defined(__MINT__) || defined(BSD_BASED_OS) || defined(MAC_OS) || defined(__MSL__)
#define UTIME_NEEDS_CLOSED_FILE 1
#else
#define UTIME_NEEDS_CLOSED_FILE 0
#endif
#endif

#if defined(MAC_OS_X) || (!defined(MAC_OS_CLASSIC) && !defined(__MSL__))
#include <sys/types.h> 
#include <sys/stat.h>
#else
#include <stat.h>
#endif

#if HAS_FUTIME
#include <sys/utime.h>
#else
#include <utime.h>
#endif /* HASFUTIME */

/*
  MS Windows needs _ prefix for Unix file functions.
  Not required by Metrowerks Standard Library (MSL).
  
  Tidy uses following for preserving the last modified time.

  WINDOWS automatically set by Win16 compilers.
  _WIN32 automatically set by Win32 compilers.
*/
#if defined(_WIN32) && !defined(__MSL__)
#define futime _futime
#define fstat _fstat
#define utimbuf _utimbuf
#define stat _stat
#define fileno _fileno
#endif /* _WIN32 */

#endif /* PRESERVE_FILE_TIMES */

/* hack for gnu sys/types.h file  which defines uint and ulong */
/* you may need to delete the #ifndef and #endif on your system */

#ifndef __USE_MISC
#if defined(sun) || defined(BSD_BASED_OS) || defined(MAC_OS_X)
#include <sys/types.h>
#else
#ifndef _INCLUDE_HPUX_SOURCE
typedef unsigned int uint;
#endif /* _INCLUDE_HPUX_SOURCE */
typedef unsigned long ulong;
#endif /* BSDs */
#endif /* __USE_MISC */

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
  language extensions disabled (i.e. pure ANSI mode).

  WINDOWS automatically set by Win16 compilers.
  _WIN32 automatically set by Win32 compilers.
*/

#if defined(WINDOWS_OS) && !defined(__MSL__)
#define unlink _unlink
#endif

#if defined(DMALLOC)
#include "dmalloc.h"
#endif

/* were defined in html.h - TRT */
void *MemAlloc(uint size);
void *MemRealloc(void *mem, uint newsize);
void MemFree(void *mem);
void ClearMemory(void *, uint size);
