#ifndef __TIDY_BUFFIO_H__
#define __TIDY_BUFFIO_H__

/** @file buffio.h - Treat buffer as an I/O stream.

  (c) 1998-2007 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.

  CVS Info :

    $Author$
    $Date$
    $Revision$

  Requires buffer to automatically grow as bytes are added.
  Must keep track of current read and write points.

*/

#include "platform.h"
#include "tidy.h"

#ifdef __cplusplus
extern "C" {
#endif

/** TidyBuffer - A chunk of memory */
TIDY_STRUCT
struct _TidyBuffer
{
    TidyAllocator* allocator;  /**< Memory allocator */
    byte* bp;           /**< Pointer to bytes */
    uint  size;         /**< # bytes currently in use */
    uint  allocated;    /**< # bytes allocated */
    uint  next;         /**< Offset of current input position */
};

/** Create (zeroed-out) data structure */
TIDY_EXPORT TidyBuffer* TIDY_CALL tidyBufCreate( TidyAllocator* allocator ); /* [i_a] */

/** Destroy data structure created with tidyBufCreate(). */
TIDY_EXPORT void TIDY_CALL tidyBufDestroy( TidyBuffer* buf ); /* [i_a] */

/** Initialize data structure using the default allocator */
TIDY_EXPORT void TIDY_CALL tidyBufInit( TidyBuffer* buf );

/** Initialize data structure using the given custom allocator */
TIDY_EXPORT void TIDY_CALL tidyBufInitWithAllocator( TidyBuffer* buf, TidyAllocator* allocator );

/** Free current buffer, allocate given amount, reset input pointer,
    use the default allocator */
TIDY_EXPORT void TIDY_CALL tidyBufAlloc( TidyBuffer* buf, uint allocSize );

/** Free current buffer, allocate given amount, reset input pointer,
    use the given custom allocator */
TIDY_EXPORT void TIDY_CALL tidyBufAllocWithAllocator( TidyBuffer* buf,
                                                      TidyAllocator* allocator,
                                                      uint allocSize );

/** Expand buffer to given size.
**  Chunk size is minimum growth. Pass 0 for default of 256 bytes.
*/
TIDY_EXPORT Bool TIDY_CALL tidyBufCheckAlloc( TidyBuffer* buf,
                                              uint allocSize, uint chunkSize );	/* [i_a] */

/** Free current contents and zero out */
TIDY_EXPORT void TIDY_CALL tidyBufFree( TidyBuffer* buf );

/** Set buffer bytes to 0 */
TIDY_EXPORT void TIDY_CALL tidyBufClear( TidyBuffer* buf );

/** Attach to existing buffer */
TIDY_EXPORT void TIDY_CALL tidyBufAttach( TidyBuffer* buf, byte* bp, uint size );

/** Detach from buffer.  Caller must free. */
TIDY_EXPORT void TIDY_CALL tidyBufDetach( TidyBuffer* buf );


/** Append bytes to buffer.  Expand if necessary. */
TIDY_EXPORT void TIDY_CALL tidyBufAppend( TidyBuffer* buf, const void* vp, uint size );

/** Append one byte to buffer.  Expand if necessary. */
TIDY_EXPORT void TIDY_CALL tidyBufPutByte( TidyBuffer* buf, byte bv );

/** Get byte from end of buffer */
TIDY_EXPORT int TIDY_CALL  tidyBufPopByte( TidyBuffer* buf );


/** Get byte from front of buffer.  Increment input offset. */
TIDY_EXPORT int TIDY_CALL  tidyBufGetByte( TidyBuffer* buf );

/** At end of buffer? */
TIDY_EXPORT Bool TIDY_CALL tidyBufEndOfInput( TidyBuffer* buf );

/** Put a byte back into the buffer.  Decrement input offset. */
TIDY_EXPORT void TIDY_CALL tidyBufUngetByte( TidyBuffer* buf, byte bv );

/** Return the number of bytes readable from the buffer. */
TIDY_EXPORT size_t TIDY_CALL tidyBufLength( TidyBuffer* buf );

/** Get byte from end of buffer.  Does NOT modify input offset. */
TIDY_EXPORT int TIDY_CALL  tidyBufPeekLastByte( TidyBuffer* buf );

/** Get byte from front of buffer.  Does NOT modify input offset. */
TIDY_EXPORT int TIDY_CALL  tidyBufPeekByte( TidyBuffer* buf );

/**
Get a string from the front of the buffer. The input offset will be incremented.

@param dst must point to a valid buffer of size @ref dstsize (or larger)

@param dstsize specifies the maximum number of bytes which will be copied
       from @ref buf to @ref dst, minus 1: the C string sentinel '\0' (NUL)

@return Returns the number of bytes copied to @ref dst, without counting the NUL sentinel.
*/
TIDY_EXPORT size_t TIDY_CALL tidyBufGetString( TidyBuffer* buf, tmbstr dst, size_t dstsize );


/**************
   TIDY
**************/

/* Forward declarations
*/

/** Initialize a buffer input source */
TIDY_EXPORT void TIDY_CALL tidyInitInputBuffer( TidyInputSource* inp, TidyBuffer* buf );

/** Initialize a buffer output sink */
TIDY_EXPORT void TIDY_CALL tidyInitOutputBuffer( TidyOutputSink* outp, TidyBuffer* buf );

#ifdef __cplusplus
}
#endif
#endif /* __TIDY_BUFFIO_H__ */

/*
 * local variables:
 * mode: c
 * indent-tabs-mode: nil
 * c-basic-offset: 4
 * eval: (c-set-offset 'substatement-open 0)
 * end:
 */
