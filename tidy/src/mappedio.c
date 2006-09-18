/* Interface to mmap style I/O

   (c) 2006 (W3C) MIT, ERCIM, Keio University
   See tidy.h for the copyright notice.

   Originally contributed by Cory Nelson and Nuno Lopes

   $Id$
*/

/* keep these here to keep file non-empty */
#include "forward.h"
#include "mappedio.h"

#ifdef SUPPORT_POSIX_MAPPED_FILES

#include "fileio.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

#include <sys/mman.h>


typedef struct
{
    const char *base;
    size_t pos, size;
} MappedFileSource;

static int TIDY_CALL mapped_getByte( void* sourceData )
{
    MappedFileSource* fin = (MappedFileSource*) sourceData;
    return fin->base[fin->pos++];
}

static Bool TIDY_CALL mapped_eof( void* sourceData )
{
    MappedFileSource* fin = (MappedFileSource*) sourceData;
    return (fin->pos+1 >= fin->size);
}

static void TIDY_CALL mapped_ungetByte( void* sourceData, byte bv )
{
    MappedFileSource* fin = (MappedFileSource*) sourceData;
    fin->pos--;
}

int TY_(initFileSource)( TidyInputSource* inp, FILE* fp )
{
    MappedFileSource* fin;
    struct stat sbuf;
    int fd;

    fin = (MappedFileSource*) MemAlloc( sizeof(MappedFileSource) );
    if ( !fin )
        return -1;

    fd = fileno(fp);
    if ( fstat(fd, &sbuf) == -1 ||
         (fin->base = mmap(0, fin->size = sbuf.st_size, PROT_READ, MAP_SHARED,
                           fd, 0)) == MAP_FAILED)
    {
        MemFree( fin );
        return -1;
    }

    fin->pos = 0;
    fclose(fp);

    inp->getByte    = mapped_getByte;
    inp->eof        = mapped_eof;
    inp->ungetByte  = mapped_ungetByte;
    inp->sourceData = fin;

    return 0;
}

void TY_(freeFileSource)( TidyInputSource* inp, Bool closeIt )
{
    MappedFileSource* fin = (MappedFileSource*) inp->sourceData;
    munmap( (void*)fin->base, fin->size );
    MemFree( fin );
}

#endif


#if defined(_WIN32)
#include <windows.h>

typedef struct _fp_input_mapped_source
{
    LONGLONG size, pos;
    HANDLE file, map;
    byte *view, *iter, *end;
    unsigned int gran;
} MappedFileSource;

static int mapped_openView( MappedFileSource *data )
{
    DWORD numb = ( ( data->size - data->pos ) > data->gran ) ?
        data->gran : (DWORD)( data->size - data->pos );

    if ( data->view )
    {
        UnmapViewOfFile( data->view );
        data->view = NULL;
    }

    data->view = MapViewOfFile( data->map, FILE_MAP_READ,
                                (DWORD)( data->pos >> 32 ),
                                (DWORD)data->pos, numb );

    if ( !data->view ) return -1;

    data->iter = data->view;
    data->end = data->iter + numb;

    return 0;
}

static int TIDY_CALL mapped_getByte( void *sourceData )
{
    MappedFileSource *data = sourceData;

    if ( !data->view || data->iter >= data->end )
    {
        data->pos += data->gran;

        if ( data->pos >= data->size || mapped_openView(data) != 0 )
            return EndOfStream;
    }

    return *( data->iter++ );
}

static Bool TIDY_CALL mapped_eof( void *sourceData )
{
    MappedFileSource *data = sourceData;
    return ( data->pos >= data->size );
}

static void TIDY_CALL mapped_ungetByte( void *sourceData, byte bt )
{
    MappedFileSource *data = sourceData;

    if ( data->iter >= data->view )
    {
        --data->iter;
        return;
    }

    if ( data->pos < data->gran )
    {
        assert(0);
        return;
    }

    data->pos -= data->gran;
    mapped_openView( data );
}

static int initMappedFileSource( TidyInputSource* inp, HANDLE fp )
{
    MappedFileSource* fin = NULL;

    inp->getByte    = mapped_getByte;
    inp->eof        = mapped_eof;
    inp->ungetByte  = mapped_ungetByte;

    fin = (MappedFileSource*) MemAlloc( sizeof(MappedFileSource) );
    if ( !fin )
        return -1;
    
    if ( !GetFileSizeEx( fp, (LARGE_INTEGER*)&fin->size ) || fin->size <= 0 )
    {
        MemFree(fin);
        return -1;
    }

    fin->map = CreateFileMapping( fp, NULL, PAGE_READONLY, 0, 0, NULL );

    if ( !fin->map )
    {
        MemFree(fin);
        return -1;
    }

    {
        SYSTEM_INFO info;
        GetSystemInfo( &info );
        fin->gran = info.dwAllocationGranularity;
    }

    fin->pos    = 0;
    fin->view   = NULL;
    fin->iter   = NULL;
    fin->end    = NULL;

    if ( mapped_openView( fin ) != 0 )
    {
        CloseHandle( fin->map );
        MemFree( fin );
        return -1;
    }

    fin->file = fp;
    inp->sourceData = fin;

    return 0;
}

static void freeMappedFileSource( TidyInputSource* inp, Bool closeIt )
{
    MappedFileSource* fin = (MappedFileSource*) inp->sourceData;
    if ( closeIt && fin && fin->file != INVALID_HANDLE_VALUE )
    {
        if ( fin->view )
            UnmapViewOfFile( fin->view );

        CloseHandle( fin->map );
        CloseHandle( fin->file );
    }
    MemFree( fin );
}

StreamIn* MappedFileInput ( TidyDocImpl* doc, HANDLE fp, int encoding )
{
    StreamIn *in = TY_(initStreamIn)( doc, encoding );
    if ( initMappedFileSource( &in->source, fp ) != 0 )
    {
        TY_(freeStreamIn)( in );
        return NULL;
    }
    in->iotype = FileIO;
    return in;
}


int TY_(DocParseFileWithMappedFile)( TidyDocImpl* doc, ctmbstr filnam ) {
    int status = -ENOENT;
    HANDLE fin = CreateFileA( filnam, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, 0, NULL );

#if PRESERVE_FILE_TIMES
    LONGLONG actime, modtime;
    ClearMemory( &doc->filetimes, sizeof(doc->filetimes) );

    if ( fin != INVALID_HANDLE_VALUE && cfgBool(doc,TidyKeepFileTimes) &&
         GetFileTime(fin, NULL, (FILETIME*)&actime, (FILETIME*)&modtime) )
    {
        doc->filetimes.actime =
            (time_t)( ( actime - 116444736000000000LL) / 10000000LL );

        doc->filetimes.modtime =
            (time_t)( ( modtime - 116444736000000000LL) / 10000000LL );
    }
#endif

    if ( fin != INVALID_HANDLE_VALUE )
    {
        StreamIn* in = MappedFileInput( doc, fin,
                                        cfg( doc, TidyInCharEncoding ) );
        if ( !in )
        {
            CloseHandle( fin );
            return -ENOMEM;
        }

        status = tidyDocParseStream( doc, in );
        freeMappedFileSource( &in->source, yes );
        TY_(freeStreamIn)( in );
    }
    else /* Error message! */
        TY_(FileError)( doc, filnam, TidyError );
    return status;
}

#endif


/*
 * local variables:
 * mode: c
 * indent-tabs-mode: nil
 * c-basic-offset: 4
 * eval: (c-set-offset 'substatement-open 0)
 * end:
 */
