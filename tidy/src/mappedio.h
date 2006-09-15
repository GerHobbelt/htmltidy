#ifndef __WIN32TC_H__
#define __WIN32TC_H__
#ifdef TIDY_WIN32_MLANG_SUPPORT

/* Interface to mmap style I/O

   (c) 2006 (W3C) MIT, ERCIM, Keio University
   See tidy.h for the copyright notice.

   $Id$
*/

#if defined(_WIN32)
int TY_(DocParseFileWithMappedFile)( TidyDocImpl* doc, ctmbstr filnam );
#endif

#endif /* TIDY_WIN32_MLANG_SUPPORT */
#endif /* __WIN32TC_H__ */
