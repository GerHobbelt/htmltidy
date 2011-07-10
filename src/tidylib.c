/* tidylib.c -- internal library definitions

  (c) 1998-2008 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.

  CVS Info :

    $Author$
    $Date$
    $Revision$

  Defines HTML Tidy API implemented by tidy library.

  Very rough initial cut for discussion purposes.

  Public interface is const-correct and doesn't explicitly depend
  on any globals.  Thus, thread-safety may be introduced w/out
  changing the interface.

  Looking ahead to a C++ wrapper, C functions always pass
  this-equivalent as 1st arg.

  Created 2001-05-20 by Charles Reitzel

*/

#include <errno.h>

#include "tidy-int.h"
#include "parser.h"
#include "clean.h"
#include "config.h"
#include "message.h"
#include "pprint.h"
#include "entities.h"
#include "tmbstr.h"
#include "utf8.h"
#include "mappedio.h"

#ifdef TIDY_WIN32_MLANG_SUPPORT
#include "win32tc.h"
#endif

/* Create/Destroy a Tidy "document" object */
static TidyDocImpl* tidyDocCreate( TidyAllocator *allocator );
static void         tidyDocRelease( TidyDocImpl* impl );

static int          tidyDocStatus( TidyDocImpl* impl );

/* Parse Markup */
static int          tidyDocParseFile( TidyDocImpl* impl, ctmbstr htmlfil );
static int          tidyDocParseStdin( TidyDocImpl* impl );
static int          tidyDocParseString( TidyDocImpl* impl, ctmbstr content );
static int          tidyDocParseBuffer( TidyDocImpl* impl, TidyBuffer* inbuf );
static int          tidyDocParseSource( TidyDocImpl* impl, TidyInputSource* docIn );


/* Execute post-parse diagnostics and cleanup.
** Note, the order is important.  You will get different
** results from the diagnostics depending on if they are run
** pre-or-post repair.
*/
static int          tidyDocRunDiagnostics( TidyDocImpl* doc );
static int          tidyDocCleanAndRepair( TidyDocImpl* doc );


/* Save cleaned up file to file/buffer/sink */
static int          tidyDocSaveFile( TidyDocImpl* impl, ctmbstr htmlfil );
static int          tidyDocSaveStdout( TidyDocImpl* impl );
static int          tidyDocSaveString( TidyDocImpl* impl, tmbstr buffer, size_t* buflen );
static int          tidyDocSaveBuffer( TidyDocImpl* impl, TidyBuffer* outbuf );
static int          tidyDocSaveSink( TidyDocImpl* impl, TidyOutputSink* docOut );
static int          tidyDocSaveStream( TidyDocImpl* impl, StreamOut* out );

#ifdef NEVER
TidyDocImpl* tidyDocToImpl( TidyDoc tdoc )
{
  return (TidyDocImpl*) tdoc;
}
TidyDoc      tidyImplToDoc( TidyDocImpl* impl )
{
  return (TidyDoc) impl;
}

Node*        tidyNodeToImpl( TidyNode tnod )
{
  return (Node*) tnod;
}
TidyNode     tidyImplToNode( Node* node )
{
  return (TidyNode) node;
}

AttVal*      tidyAttrToImpl( TidyAttr tattr )
{
  return (AttVal*) tattr;
}
TidyAttr     tidyImplToAttr( AttVal* attval )
{
  return (TidyAttr) attval;
}

const TidyOptionImpl* tidyOptionToImpl( TidyOption topt )
{
  return (const TidyOptionImpl*) topt;
}
TidyOption   tidyImplToOption( const TidyOptionImpl* option )
{
  return (TidyOption) option;
}
#endif

/* Tidy public interface
**
** Most functions return an integer:
**
** 0    -> SUCCESS
** >0   -> WARNING
** <0   -> ERROR
**
*/

TidyDoc TIDY_CALL       tidyCreate(void)
{
  TidyDocImpl* impl = tidyDocCreate( &TY_(g_default_allocator) );
  return tidyImplToDoc( impl );
}

TidyDoc TIDY_CALL tidyCreateWithAllocator( TidyAllocator *allocator )
{
  TidyDocImpl* impl = tidyDocCreate( allocator );
  return tidyImplToDoc( impl );
}

void TIDY_CALL          tidyRelease( TidyDoc tdoc )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  tidyDocRelease( impl );
}

TidyDocImpl* tidyDocCreate( TidyAllocator *allocator )
{
    TidyDocImpl* doc = (TidyDocImpl*)TidyAlloc( allocator, sizeof(TidyDocImpl) );
    TidyClearMemory( doc, sizeof(*doc) );
    doc->allocator = allocator;

    TY_(InitMap)();
    TY_(InitTags)( doc );
    TY_(InitAttrs)( doc );
    TY_(InitConfig)( doc );
    TY_(InitPrintBuf)( doc );

	tidyBufInit(&doc->access.textBuffer); /* [i_a] */

    /* By default, wire tidy messages to standard error.
    ** Document input will be set by parsing routines.
    ** Document output will be set by pretty print routines.
    ** Config input will be set by config parsing routines.
    ** But we need to start off with a way to report errors.
    */
    doc->errout = TY_(StdErrOutput)();
    return doc;
}

void          tidyDocRelease( TidyDocImpl* doc )
{
    /* doc in/out opened and closed by parse/print routines */
    if ( doc )
    {
        assert( doc->docIn == NULL );
        assert( doc->docOut == NULL );

        TY_(ReleaseStreamOut)( doc, doc->errout );
        doc->errout = NULL;

        TY_(FreePrintBuf)( doc );
        TY_(FreeLexer)( doc );
        TY_(FreeNode)(doc, &doc->root);
        TidyClearMemory(&doc->root, sizeof(Node));

        if (doc->givenDoctype)
            TidyDocFree(doc, doc->givenDoctype);

        TY_(FreeConfig)( doc );
        TY_(FreeAttrTable)( doc );
        TY_(FreeTags)( doc );

		tidyBufFree(&doc->access.textBuffer); /* [i_a] */

		TidyDocFree( doc, doc );
    }
}

/* Let application store a chunk of data w/ each Tidy tdoc instance.
** Useful for callbacks.
*/
void TIDY_CALL        tidySetAppData( TidyDoc tdoc, void* appData )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
    impl->appData = appData;
}
void* TIDY_CALL       tidyGetAppData( TidyDoc tdoc )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
    return impl->appData;
  return NULL;
}

ctmbstr TIDY_CALL     tidyReleaseDate(void)
{
    return TY_(ReleaseDate)();
}


/* Get/set configuration options
*/
Bool TIDY_CALL        tidySetOptionCallback( TidyDoc tdoc, TidyOptCallback pOptCallback )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
  {
    impl->pOptCallback = pOptCallback;
    return yes;
  }
  return no;
}


int TIDY_CALL     tidyLoadConfigBuffer( TidyDoc tdoc, TidyBuffer* buffer ) /* [i_a] */
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(ParseConfigBuffer)( impl, buffer );
    return -EINVAL;
}

int TIDY_CALL     tidyLoadConfigBufferEnc( TidyDoc tdoc, TidyBuffer* buffer, ctmbstr charenc ) /* [i_a] */
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(ParseConfigBufferEnc)( impl, buffer, charenc );
    return -EINVAL;
}

int TIDY_CALL     tidyLoadConfig( TidyDoc tdoc, ctmbstr cfgfil )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(ParseConfigFile)( impl, cfgfil );
    return -EINVAL;
}

int TIDY_CALL     tidyLoadConfigEnc( TidyDoc tdoc, ctmbstr cfgfil, ctmbstr charenc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(ParseConfigFileEnc)( impl, cfgfil, charenc );
    return -EINVAL;
}

int TIDY_CALL         tidySetCharEncoding( TidyDoc tdoc, ctmbstr encnam )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        int enc = TY_(CharEncodingId)( impl, encnam );
        if ( enc >= 0 && TY_(AdjustCharEncoding)(impl, enc) )
            return 0;

        TY_(ReportBadArgument)( impl, "char-encoding" );
    }
    return -EINVAL;
}

int TIDY_CALL           tidySetInCharEncoding( TidyDoc tdoc, ctmbstr encnam )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        int enc = TY_(CharEncodingId)( impl, encnam );
        if ( enc >= 0 && TY_(SetOptionInt)( impl, TidyInCharEncoding, enc ) )
            return 0;

        TY_(ReportBadArgument)( impl, "in-char-encoding" );
    }
    return -EINVAL;
}

int TIDY_CALL           tidySetOutCharEncoding( TidyDoc tdoc, ctmbstr encnam )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        int enc = TY_(CharEncodingId)( impl, encnam );
        if ( enc >= 0 && TY_(SetOptionInt)( impl, TidyOutCharEncoding, enc ) )
            return 0;

        TY_(ReportBadArgument)( impl, "out-char-encoding" );
    }
    return -EINVAL;
}

TidyOptionId TIDY_CALL tidyOptGetIdForName( ctmbstr optnam )
{
    const TidyOptionImpl* option = TY_(lookupOption)( optnam );
    if ( option )
        return option->id;
    return N_TIDY_OPTIONS;  /* Error */
}

TidyIterator TIDY_CALL  tidyGetOptionList( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(getOptionList)( impl );
    return (TidyIterator) (size_t)-1;
}

TidyOption TIDY_CALL    tidyGetNextOption( TidyDoc tdoc, TidyIterator* pos )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    const TidyOptionImpl* option = NULL;
    if ( impl )
        option = TY_(getNextOption)( impl, pos );
    else if ( pos )
        *pos = 0;
    return tidyImplToOption( option );
}


TidyOption TIDY_CALL    tidyGetOption( TidyDoc ARG_UNUSED(tdoc), TidyOptionId optId )
{
    const TidyOptionImpl* option = TY_(getOption)( optId );
    return tidyImplToOption( option );
}
TidyOption TIDY_CALL    tidyGetOptionByName( TidyDoc ARG_UNUSED(doc), ctmbstr optnam )
{
    const TidyOptionImpl* option = TY_(lookupOption)( optnam );
    return tidyImplToOption( option );
}

TidyOptionId TIDY_CALL  tidyOptGetId( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
        return option->id;
    return N_TIDY_OPTIONS;
}
ctmbstr TIDY_CALL       tidyOptGetName( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
        return option->name;
    return NULL;
}
TidyOptionType TIDY_CALL tidyOptGetType( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
        return option->type;
    return (TidyOptionType) -1;
}
TidyConfigCategory TIDY_CALL tidyOptGetCategory( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
        return option->category;
    return (TidyConfigCategory) -1;
}
ctmbstr TIDY_CALL       tidyOptGetDefault( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option && option->type == TidyString )
        return option->pdflt; /* [i_a] fix(?) for string access in envs where pointer size is not integer size */
    return NULL;
}
ulong TIDY_CALL          tidyOptGetDefaultInt( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option && option->type != TidyString )
        return option->dflt;
    return ~0U;
}
Bool TIDY_CALL          tidyOptGetDefaultBool( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option && option->type != TidyString )
        return ( option->dflt ? yes : no );
    return no;
}
Bool TIDY_CALL          tidyOptIsReadOnly( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option  )
        return ( option->parser == NULL );
    return yes;
}


TidyIterator TIDY_CALL  tidyOptGetPickList( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
      return TY_(getOptionPickList)( option );
    return (TidyIterator) (size_t)-1;
}
ctmbstr TIDY_CALL       tidyOptGetNextPick( TidyOption topt, TidyIterator* pos )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
        return TY_(getNextOptionPick)( option, pos );
    return NULL;
}


ctmbstr TIDY_CALL       tidyOptGetValue( TidyDoc tdoc, TidyOptionId optId )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  ctmbstr optval = NULL;
  if ( impl )
    optval = cfgStr( impl, optId );
  return optval;
}
Bool TIDY_CALL        tidyOptSetValue( TidyDoc tdoc, TidyOptionId optId, ctmbstr val )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
    return TY_(ParseConfigValue)( impl, optId, val );
  return no;
}
Bool TIDY_CALL        tidyOptParseValue( TidyDoc tdoc, ctmbstr optnam, ctmbstr val )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
    return TY_(ParseConfigOption)( impl, optnam, val );
  return no;
}

ulong TIDY_CALL        tidyOptGetInt( TidyDoc tdoc, TidyOptionId optId )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    ulong opti = 0;
    if ( impl )
        opti = cfg( impl, optId );
    return opti;
}

Bool TIDY_CALL        tidyOptSetInt( TidyDoc tdoc, TidyOptionId optId, ulong val )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(SetOptionInt)( impl, optId, val );
    return no;
}

Bool TIDY_CALL         tidyOptGetBool( TidyDoc tdoc, TidyOptionId optId )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    Bool optb = no;
    if ( impl )
    {
        const TidyOptionImpl* option = TY_(getOption)( optId );
        if ( option )
        {
            optb = cfgBool( impl, optId );
        }
    }
    return optb;
}

Bool TIDY_CALL        tidyOptSetBool( TidyDoc tdoc, TidyOptionId optId, Bool val )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(SetOptionBool)( impl, optId, val );
    return no;
}

ctmbstr TIDY_CALL       tidyOptGetEncName( TidyDoc tdoc, TidyOptionId optId )
{
  uint enc = tidyOptGetInt( tdoc, optId );
  return TY_(CharEncodingOptName)( enc );
}

ctmbstr TIDY_CALL       tidyOptGetCurrPick( TidyDoc tdoc, TidyOptionId optId )
{
    const TidyOptionImpl* option = TY_(getOption)( optId );
    if ( option && option->pickList )
    {
        uint ix, pick = tidyOptGetInt( tdoc, optId );
        const ctmbstr* pL = option->pickList;
        for ( ix=0; *pL && ix < pick; ++ix )
            ++pL;
        if ( *pL )
            return *pL;
    }
    return NULL;
}


TidyIterator TIDY_CALL tidyOptGetDeclTagList( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    TidyIterator declIter = 0;
    if ( impl )
        declIter = TY_(GetDeclaredTagList)( impl );
    return declIter;
}

ctmbstr TIDY_CALL       tidyOptGetNextDeclTag( TidyDoc tdoc, TidyOptionId optId,
                                     TidyIterator* iter )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    ctmbstr tagnam = NULL;
    if ( impl )
    {
        UserTagType tagtyp = tagtype_null;
        if ( optId == TidyInlineTags )
            tagtyp = tagtype_inline;
        else if ( optId == TidyBlockTags )
            tagtyp = tagtype_block;
        else if ( optId == TidyEmptyTags )
            tagtyp = tagtype_empty;
        else if ( optId == TidyPreTags )
            tagtyp = tagtype_pre;
		else if ( optId == TidyOtherNamespaceTags )
			tagtyp = tagtype_OtherNamespace;
        if ( tagtyp != tagtype_null )
            tagnam = TY_(GetNextDeclaredTag)( impl, tagtyp, iter );
    }
    return tagnam;
}

ctmbstr TIDY_CALL tidyOptGetDoc( TidyDoc ARG_UNUSED(tdoc), TidyOption opt )
{
    const TidyOptionId optId = tidyOptGetId( opt );
    const TidyOptionDoc* docDesc = TY_(OptGetDocDesc)( optId );
    return docDesc ? docDesc->doc : NULL;
}

TidyIterator TIDY_CALL tidyOptGetDocLinksList( TidyDoc ARG_UNUSED(tdoc), TidyOption opt )
{
    const TidyOptionId optId = tidyOptGetId( opt );
    const TidyOptionDoc* docDesc = TY_(OptGetDocDesc)( optId );
    if (docDesc && docDesc->links)
        return (TidyIterator)docDesc->links;
    return (TidyIterator)NULL;
}

TidyOption TIDY_CALL tidyOptGetNextDocLinks( TidyDoc tdoc, TidyIterator* pos )
{
    const TidyOptionId* curr = (const TidyOptionId *)*pos;
    TidyOption opt;

    if (*curr == TidyUnknownOption)
    {
        *pos = (TidyIterator)NULL;
        return (TidyOption)0;
    }
    opt = tidyGetOption(tdoc, *curr);
    curr++;
    *pos = (*curr == TidyUnknownOption ) ?
        (TidyIterator)NULL:(TidyIterator)curr;
    return opt;
}

int TIDY_CALL tidyOptSaveFile( TidyDoc tdoc, ctmbstr cfgfil )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(SaveConfigFile)( impl, cfgfil );
    return -EINVAL;
}

int TIDY_CALL tidyOptSaveSink( TidyDoc tdoc, TidyOutputSink* sink )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(SaveConfigSink)( impl, sink );
    return -EINVAL;
}

int TIDY_CALL tidyOptSaveBuffer( TidyDoc tdoc, TidyBuffer* buffer ) /* [i_a] */
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(SaveConfigBuffer)( impl, buffer );
    return -EINVAL;
}

Bool TIDY_CALL tidyOptSnapshot( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        TY_(TakeConfigSnapshot)( impl );
        return yes;
    }
    return no;
}
Bool TIDY_CALL tidyOptResetToSnapshot( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        TY_(ResetConfigToSnapshot)( impl );
        return yes;
    }
    return no;
}
Bool TIDY_CALL tidyOptResetAllToDefault( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        TY_(ResetConfigToDefault)( impl );
        return yes;
    }
    return no;
}

Bool TIDY_CALL tidyOptResetToDefault( TidyDoc tdoc, TidyOptionId optId )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(ResetOptionToDefault)( impl, optId );
    return no;
}

Bool TIDY_CALL tidyOptDiffThanDefault( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(ConfigDiffThanDefault)( impl );
    return no;
}
Bool TIDY_CALL          tidyOptDiffThanSnapshot( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return TY_(ConfigDiffThanSnapshot)( impl );
    return no;
}

Bool TIDY_CALL tidyOptCopyConfig( TidyDoc to, TidyDoc from )
{
    TidyDocImpl* docTo = tidyDocToImpl( to );
    TidyDocImpl* docFrom = tidyDocToImpl( from );
    if ( docTo && docFrom )
    {
        TY_(CopyConfig)( docTo, docFrom );
        return yes;
    }
    return no;
}

Bool TIDY_CALL tidyOptAdjustConfig( TidyDoc tdoc ) /* [i_a] */
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    if ( doc )
    {
        TY_(AdjustConfig)( doc );
        return yes;
    }
    return no;
}



/* I/O and Message handling interface
**
** By default, Tidy will define, create and use
** tdoc instances of input and output handlers for
** standard C buffered I/O (i.e. FILE* stdin,
** FILE* stdout and FILE* stderr for content
** input, content output and diagnostic output,
** respectively.  A FILE* cfgFile input handler
** will be used for config files.  Command line
** options will just be set directly.
*/

/* Use TidyReportFilter to filter messages by diagnostic level:
** info, warning, etc.  Just set diagnostic output
** handler to redirect all diagnostics output.  Return true
** to proceed with output, false to cancel.
*/
Bool TIDY_CALL        tidySetReportFilter( TidyDoc tdoc, TidyReportFilter filt )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
  {
    impl->mssgFilt = filt;
    return yes;
  }
  return no;
}

#if 0   /* Not yet */
int         tidySetContentOutputSink( TidyDoc tdoc, TidyOutputSink* outp )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
  {
    impl->docOut = outp;
    return 0;
  }
  return -EINVAL;
}
int         tidySetDiagnosticOutputSink( TidyDoc tdoc, TidyOutputSink* outp )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
  {
    impl->msgOut = outp;
    return 0;
  }
  return -EINVAL;
}


/* Library helpers
*/
cmbstr       tidyLookupMessage( TidyDoc tdoc, int errorNo )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  cmbstr mssg = NULL;
  if ( impl )
    mssg = tidyMessage_Lookup( impl->messages, errorNo );
  return mssg;
}
#endif


FILE* TIDY_CALL   tidySetErrorFile( TidyDoc tdoc, ctmbstr errfilnam )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        FILE* errout = fopen( errfilnam, "wb" );
        if ( errout )
        {
            uint outenc = cfg( impl, TidyOutCharEncoding );
            uint nl = cfg( impl, TidyNewline );
            TY_(ReleaseStreamOut)( impl, impl->errout );
            impl->errout = TY_(FileOutput)( impl, errout, outenc, nl );
            return errout;
        }
        else /* Emit message to current error sink */
            TY_(FileError)( impl, errfilnam, TidyError );
    }
    return NULL;
}

int TIDY_CALL    tidySetErrorBuffer( TidyDoc tdoc, TidyBuffer* errbuf )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        uint outenc = cfg( impl, TidyOutCharEncoding );
        uint nl = cfg( impl, TidyNewline );
        TY_(ReleaseStreamOut)( impl, impl->errout );
        impl->errout = TY_(BufferOutput)( impl, errbuf, outenc, nl );
        return ( impl->errout ? 0 : -ENOMEM );
    }
    return -EINVAL;
}

int TIDY_CALL    tidySetErrorSink( TidyDoc tdoc, TidyOutputSink* sink )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        uint outenc = cfg( impl, TidyOutCharEncoding );
        uint nl = cfg( impl, TidyNewline );
        TY_(ReleaseStreamOut)( impl, impl->errout );
        impl->errout = TY_(UserOutput)( impl, sink, outenc, nl );
        return ( impl->errout ? 0 : -ENOMEM );
    }
    return -EINVAL;
}


/* Document info */
int TIDY_CALL        tidyStatus( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    int tidyStat = -EINVAL;
    if ( impl )
        tidyStat = tidyDocStatus( impl );
    return tidyStat;
}
int TIDY_CALL        tidyDetectedHtmlVersion( TidyDoc ARG_UNUSED(tdoc) )
{
/*    TidyDocImpl* impl = tidyDocToImpl( tdoc ); */
    return 0;
}
Bool TIDY_CALL        tidyDetectedXhtml( TidyDoc ARG_UNUSED(tdoc) )
{
/*    TidyDocImpl* impl = tidyDocToImpl( tdoc ); */
    return no;
}
Bool TIDY_CALL        tidyDetectedGenericXml( TidyDoc ARG_UNUSED(tdoc) )
{
/*    TidyDocImpl* impl = tidyDocToImpl( tdoc ); */
    return no;
}

uint TIDY_CALL       tidyErrorCount( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    uint count = 0xFFFFFFFF;
    if ( impl )
        count = impl->errors;
    return count;
}
uint TIDY_CALL       tidyWarningCount( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    uint count = 0xFFFFFFFF;
    if ( impl )
        count = impl->warnings;
    return count;
}
uint TIDY_CALL       tidyAccessWarningCount( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    uint count = 0xFFFFFFFF;
    if ( impl )
        count = impl->accessErrors;
    return count;
}
uint TIDY_CALL       tidyConfigErrorCount( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    uint count = 0xFFFFFFFF;
    if ( impl )
        count = impl->optionErrors;
    return count;
}


/* Error reporting functions
*/
void TIDY_CALL         tidyErrorSummary( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        TY_(ErrorSummary)( impl );
}
void TIDY_CALL         tidyGeneralInfo( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        TY_(GeneralInfo)( impl );
}


/* I/O Functions
**
** Initial version supports only whole-file operations.
** Do not expose Tidy StreamIn or Out data structures - yet.
*/

/* Parse/load Functions
**
** HTML/XHTML version determined from input.
*/
int TIDY_CALL  tidyParseFile( TidyDoc tdoc, ctmbstr filnam )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocParseFile( doc, filnam );
}
int TIDY_CALL  tidyParseStdin( TidyDoc tdoc )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocParseStdin( doc );
}
int TIDY_CALL  tidyParseString( TidyDoc tdoc, ctmbstr content )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocParseString( doc, content );
}
int TIDY_CALL  tidyParseBuffer( TidyDoc tdoc, TidyBuffer* inbuf )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocParseBuffer( doc, inbuf );
}
int TIDY_CALL  tidyParseSource( TidyDoc tdoc, TidyInputSource* source )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocParseSource( doc, source );
}


int   tidyDocParseFile( TidyDocImpl* doc, ctmbstr filnam )
{
#ifdef _WIN32
    return TY_(DocParseFileWithMappedFile)( doc, filnam );
#else
    int status = -ENOENT;
    FILE* fin = fopen( filnam, "rb" );

#if PRESERVE_FILE_TIMES
    struct stat sbuf = {0};
    /* get last modified time */
    TidyClearMemory( &doc->filetimes, sizeof(doc->filetimes) );
    if ( fin && cfgBool(doc,TidyKeepFileTimes) &&
         fstat(fileno(fin), &sbuf) != -1 )
    {
          doc->filetimes.actime  = sbuf.st_atime;
          doc->filetimes.modtime = sbuf.st_mtime;
    }
#endif

    if ( fin )
    {
        StreamIn* in = TY_(FileInput)( doc, fin, cfg( doc, TidyInCharEncoding ));
        if ( !in )
        {
            fclose( fin );
            return status;
        }
        status = TY_(DocParseStream)( doc, in );
        TY_(freeFileSource)(&in->source, yes);
        TY_(freeStreamIn)(in);
    }
    else /* Error message! */
        TY_(FileError)( doc, filnam, TidyError );
    return status;
#endif
}

int   tidyDocParseStdin( TidyDocImpl* doc )
{
    StreamIn* in = TY_(FileInput)( doc, stdin, cfg( doc, TidyInCharEncoding ));
    int status = TY_(DocParseStream)( doc, in );
    TY_(freeStreamIn)(in);
    return status;
}

int   tidyDocParseBuffer( TidyDocImpl* doc, TidyBuffer* inbuf )
{
    int status = -EINVAL;
    if ( inbuf )
    {
        StreamIn* in = TY_(BufferInput)( doc, inbuf, cfg( doc, TidyInCharEncoding ));
        status = TY_(DocParseStream)( doc, in );
        TY_(freeStreamIn)(in);
    }
    return status;
}

int   tidyDocParseString( TidyDocImpl* doc, ctmbstr content )
{
    int status = -EINVAL;
    TidyBuffer inbuf;
    StreamIn* in = NULL;

    if ( content )
    {
        tidyBufInitWithAllocator( &inbuf, doc->allocator );
        tidyBufAttach( &inbuf, (byte*)content, TY_(tmbstrlen)(content)+1 );
        in = TY_(BufferInput)( doc, &inbuf, cfg( doc, TidyInCharEncoding ));
        status = TY_(DocParseStream)( doc, in );
        tidyBufDetach( &inbuf );
        TY_(freeStreamIn)(in);
    }
    return status;
}

int   tidyDocParseSource( TidyDocImpl* doc, TidyInputSource* source )
{
    StreamIn* in = TY_(UserInput)( doc, source, cfg( doc, TidyInCharEncoding ));
    int status = TY_(DocParseStream)( doc, in );
    TY_(freeStreamIn)(in);
    return status;
}


/* Print/save Functions
**
*/
int TIDY_CALL        tidySaveFile( TidyDoc tdoc, ctmbstr filnam )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocSaveFile( doc, filnam );
}
int TIDY_CALL        tidySaveStdout( TidyDoc tdoc )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocSaveStdout( doc );
}
int TIDY_CALL        tidySaveString( TidyDoc tdoc, tmbstr buffer, size_t* buflen )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocSaveString( doc, buffer, buflen );
}
int TIDY_CALL        tidySaveBuffer( TidyDoc tdoc, TidyBuffer* outbuf )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocSaveBuffer( doc, outbuf );
}
int TIDY_CALL        tidySaveSink( TidyDoc tdoc, TidyOutputSink* sink )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocSaveSink( doc, sink );
}

int         tidyDocSaveFile( TidyDocImpl* doc, ctmbstr filnam )
{
    int status = -ENOENT;
    FILE* fout = NULL;

    /* Don't zap input file if no output */
    if ( doc->errors > 0 &&
         cfgBool(doc, TidyWriteBack) && !cfgBool(doc, TidyForceOutput) )
        status = tidyDocStatus( doc );
    else
        fout = fopen( filnam, "wb" );

    if ( fout )
    {
        uint outenc = cfg( doc, TidyOutCharEncoding );
        uint nl = cfg( doc, TidyNewline );
        StreamOut* out = TY_(FileOutput)( doc, fout, outenc, nl );

        status = tidyDocSaveStream( doc, out );

        fclose( fout );
        TidyDocFree( doc, out );

#if PRESERVE_FILE_TIMES
        if ( doc->filetimes.actime )
        {
            /* set file last accessed/modified times to original values */
            utime( filnam, &doc->filetimes );
            TidyClearMemory( &doc->filetimes, sizeof(doc->filetimes) );
        }
#endif /* PRESERVFILETIMES */
    }
    if ( status < 0 ) /* Error message! */
        TY_(FileError)( doc, filnam, TidyError );
    return status;
}



/* Note, _setmode() does NOT work on Win2K Pro w/ VC++ 6.0 SP3.
** The code has been left in in case it works w/ other compilers
** or operating systems.  If stdout is in Text mode, be aware that
** it will garble UTF16 documents.  In text mode, when it encounters
** a single byte of value 10 (0xA), it will insert a single byte
** value 13 (0xD) just before it.  This has the effect of garbling
** the entire document.
*/

#if !defined(NO_SETMODE_SUPPORT)

#if defined(_WIN32) || defined(OS2_OS)
#include <fcntl.h>
#include <io.h>
#endif

#endif

int         tidyDocSaveStdout( TidyDocImpl* doc )
{
#if !defined(NO_SETMODE_SUPPORT)

#if defined(_WIN32) || defined(OS2_OS)
    int oldstdoutmode = -1, oldstderrmode = -1;
#endif

#endif
    int status = 0;
    uint outenc = cfg( doc, TidyOutCharEncoding );
    uint nl = cfg( doc, TidyNewline );
    StreamOut* out = TY_(FileOutput)( doc, stdout, outenc, nl );

#if !defined(NO_SETMODE_SUPPORT)

#if defined(_WIN32) || defined(OS2_OS)
    oldstdoutmode = setmode( fileno(stdout), _O_BINARY );
    oldstderrmode = setmode( fileno(stderr), _O_BINARY );
#endif

#endif

    if ( 0 == status )
      status = tidyDocSaveStream( doc, out );

    fflush(stdout);
    fflush(stderr);

#if !defined(NO_SETMODE_SUPPORT)

#if defined(_WIN32) || defined(OS2_OS)
    if ( oldstdoutmode != -1 )
        oldstdoutmode = setmode( fileno(stdout), oldstdoutmode );
    if ( oldstderrmode != -1 )
        oldstderrmode = setmode( fileno(stderr), oldstderrmode );
#endif

#endif

    TidyDocFree( doc, out );
    return status;
}

int         tidyDocSaveString( TidyDocImpl* doc, tmbstr buffer, size_t* buflen )
{
    uint outenc = cfg( doc, TidyOutCharEncoding );
    uint nl = cfg( doc, TidyNewline );
    TidyBuffer outbuf;
    StreamOut* out;
    int status;

    tidyBufInitWithAllocator( &outbuf, doc->allocator );
    out = TY_(BufferOutput)( doc, &outbuf, outenc, nl );
    status = tidyDocSaveStream( doc, out );

    if ( outbuf.size + 1 > *buflen ) /* #2893644 */
        status = -ENOMEM;
    else
	{
        memcpy( buffer, outbuf.bp, outbuf.size );
		buffer[outbuf.size] = 0; /* #2893644 */
	}

    *buflen = outbuf.size + 1; /* #2893644 */
    tidyBufFree( &outbuf );
    TidyDocFree( doc, out );
    return status;
}

int         tidyDocSaveBuffer( TidyDocImpl* doc, TidyBuffer* outbuf )
{
    int status = -EINVAL;
    if ( outbuf )
    {
        uint outenc = cfg( doc, TidyOutCharEncoding );
        uint nl = cfg( doc, TidyNewline );
        StreamOut* out = TY_(BufferOutput)( doc, outbuf, outenc, nl );

        status = tidyDocSaveStream( doc, out );
        TidyDocFree( doc, out );
    }
    return status;
}

int         tidyDocSaveSink( TidyDocImpl* doc, TidyOutputSink* sink )
{
    uint outenc = cfg( doc, TidyOutCharEncoding );
    uint nl = cfg( doc, TidyNewline );
    StreamOut* out = TY_(UserOutput)( doc, sink, outenc, nl );
    int status = tidyDocSaveStream( doc, out );
    TidyDocFree( doc, out );
    return status;
}

int         tidyDocStatus( TidyDocImpl* doc )
{
    if ( doc->errors > 0 )
        return 2;
    if ( doc->warnings > 0 || doc->accessErrors > 0 )
        return 1;
    return 0;
}



int TIDY_CALL        tidyCleanAndRepair( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
      return tidyDocCleanAndRepair( impl );
    return -EINVAL;
}

int TIDY_CALL        tidyRunDiagnostics( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
      return tidyDocRunDiagnostics( impl );
    return -EINVAL;
}


/* Workhorse functions.
**
** Parse requires input source, all input config items
** and diagnostic sink to have all been set before calling.
**
** Emit likewise requires that document sink and all
** pretty printing options have been set.
*/
static ctmbstr integrity = "\nPanic - tree has lost its integrity\n";

#define CHARENC_PREFETCH_BUFSIZE   (1024)  /* keep a little spare beyond the required 512 bytes */

int         TY_(DocParseStream)( TidyDocImpl* doc, StreamIn* in )
{
    Bool xmlIn = cfgBool( doc, TidyXmlTags );
    int bomEnc;
	byte charenc_prefetch_buf[CHARENC_PREFETCH_BUFSIZE];
	size_t raw_count_read;
	int rounds;
	size_t orig_startpos;

    assert( doc != NULL && in != NULL );
    assert( doc->docIn == NULL );
    doc->docIn = in;

    TY_(TakeConfigSnapshot)( doc );    /* Save config state */
    TY_(FreeLexer)( doc );
    TY_(FreeAnchors)( doc );

    TY_(FreeNode)(doc, &doc->root);
    TidyClearMemory(&doc->root, sizeof(Node));

    if (doc->givenDoctype)
        TidyDocFree(doc, doc->givenDoctype);

    doc->givenDoctype = NULL;

    doc->lexer = TY_(NewLexer)( doc );
    /* doc->lexer->root = &doc->root; */
    doc->root.line = doc->lexer->lines;
    doc->root.column = doc->lexer->columns;
	doc->inputHadBOM = no;

	/*
	now do something sneaky: fetch a little over 512 RAW bytes
	(W3C says the <meta charset='xyz'> tag should reside in
	the first 512 bytes) and store those; when a charset <meta>
	is found and the lexer rewound (LEX_REWIND state), those
	bytes can again be pushed back into the stream so we didn't
	lose the RAW input -- we cannot use the InStream regular
	Unget feature as that one stores /decoded/ characters and
	we rewind RAW because the character encoding system used up
	to then was incorrect.

	Note: this implies that the <meta> character encoding
	      processing code checks that the <meta> was not found
		  beyond this initial RAW range, or we will loose
		  input bytes on the second round!
    */
	orig_startpos = tidyTell(&doc->docIn->source); /* for assert() only */
	raw_count_read = TY_(ReadRawBytesFromStream)(doc->docIn, charenc_prefetch_buf, sizeof(charenc_prefetch_buf));
	doc->lexer->buf_prefetch_size = tidyTell(&doc->docIn->source); /* see comment below at LEX_REWIND handling */
	/* immediately push back those bytes: they need to be parsed! */
	TY_(UngetRawBytesToStream)(doc->docIn, charenc_prefetch_buf, raw_count_read);

	/*
	as this can unget some bytes in particular conditions,
	do it after we pushed our bulk so that our 'lexer->buf_prefetch_size'
	number stays reliable whatever happens here.
	*/
	bomEnc = TY_(ReadBOMEncoding)(doc->docIn);

	if (bomEnc != -1)
	{
		in->encoding = bomEnc;
		TY_(SetOptionInt)(doc, TidyInCharEncoding, bomEnc);
	}

	/* limit the number of reruns to prevent bugs in the parser to lock up CPU */
	for (rounds = 5; rounds > 0; rounds--)
	{
		/* TODO   snapshot lexer + doc */
		Lexer lexerSnapshot = *doc->lexer;
		TidyDocImpl docSnapshot = *doc;
		StreamIn inputSnapshot = *doc->docIn;



#ifdef TIDY_WIN32_MLANG_SUPPORT
		if (in->encoding > WIN32MLANG)
			TY_(Win32MLangInitInputTranscoder)(in, in->encoding);
#endif /* TIDY_WIN32_MLANG_SUPPORT */

		/* Tidy doesn't alter the doctype for generic XML docs */
		if ( xmlIn )
		{
			TY_(ParseXMLDocument)( doc );
			if ( !TY_(CheckNodeIntegrity)( &doc->root ) )
				TidyPanic( doc->allocator, integrity );
		}
		else
		{
			doc->warnings = 0;
			TY_(ParseDocument)( doc );
			if ( !TY_(CheckNodeIntegrity)( &doc->root ) )
				TidyPanic( doc->allocator, integrity );
		}

#ifdef TIDY_WIN32_MLANG_SUPPORT
	    TY_(Win32MLangUninitInputTranscoder)(in);
#endif /* TIDY_WIN32_MLANG_SUPPORT */

		if (doc->lexer->state == LEX_REWIND)
		{
			int inenc;
			int outenc;
			int encoding;
			size_t dist;
			size_t prefetch_len = doc->lexer->buf_prefetch_size;

			/*
			check whether we're still sitting within the unget buffer.

			We grabbed more than the W3C required 512 bytes to begin with,
			so our grab should leave some room to spare for well-formed docs.

			So we check how much bytes of the previous TY_(UngetRawBytesToStream)()
			invocation above are still in there. And then we rewind to the full
			size again, thus ending up right smack where we started on our first
			tour. :-)
			*/
			dist = tidyTell(&doc->docIn->source);
			if (dist > prefetch_len)
			{
				TY_(ReportFatal)( doc, NULL, NULL, REWIND_IMPOSSIBLE );
				break;
			}

			/* reset states; keep encoding settings though */
			assert(doc->docIn->encoding == (int)cfg(doc, TidyInCharEncoding));
			inenc = cfg(doc, TidyInCharEncoding);
			outenc = cfg(doc, TidyOutCharEncoding);
			encoding = cfg(doc, TidyCharEncoding);

			TY_(ResetConfigToSnapshot)(doc);
			TY_(FreeLexer)( doc );
			TY_(FreeAnchors)( doc );

			TY_(FreeNode)(doc, &doc->root);
			TidyClearMemory(&doc->root, sizeof(Node));

			if (doc->givenDoctype)
				TidyDocFree(doc, doc->givenDoctype);
			doc->givenDoctype = NULL;

			doc->lexer = TY_(NewLexer)( doc );
			doc->root.line = doc->lexer->lines = lexerSnapshot.lines;
			doc->root.column = doc->lexer->columns = lexerSnapshot.columns;
			/* doc->inputHadBOM = no; */

			docSnapshot.root.line = doc->root.line;
			docSnapshot.root.column = doc->root.column;

			docSnapshot.givenDoctype = doc->givenDoctype;
			docSnapshot.lexer = doc->lexer;
			docSnapshot.attribs = doc->attribs;
			docSnapshot.config = doc->config;
			*doc = docSnapshot;

			doc->lexer->buf_prefetch_size = prefetch_len; /* make sure we always remember this value! */

			/* now reset the InStream too! */
			assert(in == doc->docIn);

			inputSnapshot.charbuf = doc->docIn->charbuf;
			inputSnapshot.bufpos = 0;
			inputSnapshot.bufsize = doc->docIn->bufsize;
			inputSnapshot.pushed = no; /* discard ALL pushed encoded characters */

#ifdef TIDY_STORE_ORIGINAL_TEXT
			if (doc->docIn->otextbuf != NULL)
			{
				TidyDocFree(doc, doc->docIn->otextbuf);
			}
			inputSnapshot.otextbuf = NULL;
			inputSnapshot.otextlen = 0;
			inputSnapshot.otextsize = 0;
#endif
			*doc->docIn = inputSnapshot;

			/*
			after resetting the InStream, push back the original RAW data for refetch from source.

			All we have to do is unget the last N bytes of data we grabbed before we started
			this show. That should park us right at the start of the input again, ready
			for another go.

			(Of course, one caveat remains: Win32 processing > 4GB inputs through memory mapped I/O:
			their tidyTell() method clips at maxvalue(size_t)! So we can detect that
			occasion too and barf a hairball.)
			*/
			if (dist == ~((size_t)0))
			{
				TY_(ReportFatal)( doc, NULL, NULL, REWIND_IMPOSSIBLE );
				break;
			}
			TY_(UngetRawBytesToStream)(doc->docIn, charenc_prefetch_buf, dist);
			assert(tidyTell(&doc->docIn->source) == orig_startpos);
			/* and fill it up again! */


			/* and redo the BOM read, but now while minding our possible override. */
			bomEnc = TY_(ReadBOMEncoding)(doc->docIn);
#if 0
			if (bomEnc != -1)
			{
				doc->docIn->encoding = bomEnc;
				TY_(SetOptionInt)(doc, TidyInCharEncoding, bomEnc);
			}
#endif

			/* now set the encoding as we set it up after we parsed that <meta> tag */
			TY_(SetOptionInt)( doc, TidyCharEncoding, encoding );
			TY_(SetOptionInt)( doc, TidyInCharEncoding, inenc );
			TY_(SetOptionInt)( doc, TidyOutCharEncoding, outenc );
			doc->docIn->encoding = inenc;
		}
		else
		{
			/* no need for a rerun */
			break;
		}
	}

	if (rounds <= 0)
	{
		/* that didn't go well; warn about internal errors! */
		TY_(ReportFatal)( doc, NULL, NULL, REWIND_IMPOSSIBLE /* should be 'INTERNAL_CHARENC_ERROR' */);
	}

    doc->docIn = NULL;
    return tidyDocStatus( doc );
}

int         tidyDocRunDiagnostics( TidyDocImpl* doc )
{
    Bool quiet = cfgBool( doc, TidyQuiet );
    Bool force = cfgBool( doc, TidyForceOutput );

    if ( !quiet )
    {

        TY_(ReportMarkupVersion)( doc );
        TY_(ReportNumWarnings)( doc );
    }

    if ( doc->errors > 0 && !force )
        TY_(NeedsAuthorIntervention)( doc );

     return tidyDocStatus( doc );
}

int         tidyDocCleanAndRepair( TidyDocImpl* doc )
{
    Bool word2K   = cfgBool( doc, TidyWord2000 );
    Bool logical  = cfgBool( doc, TidyLogicalEmphasis );
    Bool clean    = cfgBool( doc, TidyMakeClean );
    Bool dropFont = cfgBool( doc, TidyDropFontTags );
    Bool htmlOut  = cfgBool( doc, TidyHtmlOut );
    Bool xmlOut   = cfgBool( doc, TidyXmlOut );
    Bool xhtmlOut = cfgBool( doc, TidyXhtmlOut );
    Bool xmlDecl  = cfgBool( doc, TidyXmlDecl );
    Bool tidyMark = cfgBool( doc, TidyMark );
    Bool tidyTitle = cfgBool( doc, TidyTitle );  /* [i_a] */
    Bool tidyXmlTags = cfgBool( doc, TidyXmlTags );
    Bool wantNameAttr = cfgBool( doc, TidyAnchorAsName );
    Node* node;

    if (tidyXmlTags)
       return tidyDocStatus( doc );

    /* simplifies <b><b> ... </b> ...</b> etc. */
    TY_(NestedEmphasis)( doc, &doc->root );

    /* cleans up <dir>indented text</dir> etc. */
    TY_(List2BQ)( doc, &doc->root );
    TY_(BQ2Div)( doc, &doc->root );

    /* replaces i by em and b by strong */
    if ( logical )
        TY_(EmFromI)( doc, &doc->root );

    if ( word2K && TY_(IsWord2000)(doc) )
    {
        /* prune Word2000's <![if ...]> ... <![endif]> */
        TY_(DropSections)( doc, &doc->root );

        /* drop style & class attributes and empty p, span elements */
        TY_(CleanWord2000)( doc, &doc->root );
        TY_(DropEmptyElements)(doc, &doc->root);
    }

    /* replaces presentational markup by style rules */
    if ( clean || dropFont )
        TY_(CleanDocument)( doc );

    /*  Move terminating <br /> tags from out of paragraphs  */
    /*!  Do we want to do this for all block-level elements?  */

    /* This is disabled due to http://tidy.sf.net/bug/681116 */
	if ( cfgBool( doc, TidyFixBrakes ) )
	{
/* #if 0 */
	    TY_(FixBrakes)( doc, TY_(FindBody)( doc ));
/* #endif */
	}

    /*  Reconcile http-equiv meta element with output encoding  */
    if (cfg( doc, TidyOutCharEncoding) != RAW
#ifndef NO_NATIVE_ISO2022_SUPPORT
        && cfg( doc, TidyOutCharEncoding) != ISO2022
#endif
        )
        TY_(VerifyHTTPEquiv)( doc, TY_(FindHEAD)( doc ));

    if ( !TY_(CheckNodeIntegrity)( &doc->root ) )
        TidyPanic( doc->allocator, integrity );

    /* remember given doctype for reporting */
    node = TY_(FindDocType)(doc);
    if (node)
    {
        AttVal* fpi = TY_(GetAttrByName)(node, "PUBLIC");
        if (AttrHasValue(fpi))
        {
            if (doc->givenDoctype)
                TidyDocFree(doc, doc->givenDoctype);
            doc->givenDoctype = TY_(tmbstrdup)(doc->allocator,fpi->value);
        }
    }

    if ( doc->root.content )
    {
        /* If we had XHTML input but want HTML output */
        if ( htmlOut && doc->lexer->isvoyager )
        {
            Node* node = TY_(FindDocType)(doc);
            /* Remove reference, but do not free */
            if (node)
              TY_(RemoveNode)(node);
        }

        if (xhtmlOut && !htmlOut)
        {
            TY_(SetXHTMLDocType)(doc);
            TY_(FixAnchors)(doc, &doc->root, wantNameAttr, yes);
            TY_(FixXhtmlNamespace)(doc, yes);
            TY_(FixLanguageInformation)(doc, &doc->root, yes, yes);
        }
        else
        {
            TY_(FixDocType)(doc);
            TY_(FixAnchors)(doc, &doc->root, wantNameAttr, yes);
            TY_(FixXhtmlNamespace)(doc, no);
            TY_(FixLanguageInformation)(doc, &doc->root, no, yes);
        }

        if (tidyTitle)  /* [i_a] */
		{
            TY_(FixTitle)(doc);
		}

        if (tidyMark)
		{
            TY_(AddGenerator)(doc);
		}
    }

    /* ensure presence of initial <?xml version="1.0"?> */
    if ( xmlOut && xmlDecl )
        TY_(FixXmlDecl)( doc );

    return tidyDocStatus( doc );
}

static
Bool showBodyOnly( TidyDocImpl* doc, TidyTriState bodyOnly )
{
    Node* node;

    switch( bodyOnly )
    {
    case TidyNoState:
        return no;
    case TidyYesState:
        return yes;
    default:
        node = TY_(FindBody)( doc );
        if (node && node->implicit )
            return yes;
    }
    return no;
}


int         tidyDocSaveStream( TidyDocImpl* doc, StreamOut* out )
{
    Bool showMarkup  = cfgBool( doc, TidyShowMarkup );
    Bool forceOutput = cfgBool( doc, TidyForceOutput );
#if SUPPORT_UTF16_ENCODINGS
    Bool outputBOM   = ( cfgAutoBool(doc, TidyOutputBOM) == TidyYesState );
    Bool smartBOM    = ( cfgAutoBool(doc, TidyOutputBOM) == TidyAutoState );
#endif
    Bool xmlOut      = cfgBool( doc, TidyXmlOut );
    Bool xhtmlOut    = cfgBool( doc, TidyXhtmlOut );
    TidyTriState bodyOnly    = cfgAutoBool( doc, TidyBodyOnly );

    Bool dropComments = cfgBool(doc, TidyHideComments);
    Bool makeClean    = cfgBool(doc, TidyMakeClean);
    Bool asciiChars   = cfgBool(doc, TidyAsciiChars);
    Bool makeBare     = cfgBool(doc, TidyMakeBare);
    Bool escapeCDATA  = cfgBool(doc, TidyEscapeCdata);
    TidyAttrSortStrategy sortAttrStrat = cfg(doc, TidySortAttributes);

    if (escapeCDATA)
        TY_(ConvertCDATANodes)(doc, &doc->root);

    if (dropComments)
        TY_(DropComments)(doc, &doc->root);

    if (makeClean)
    {
        /* noop */
        TY_(DropFontElements)(doc, &doc->root, NULL);
        TY_(WbrToSpace)(doc, &doc->root);
    }

    if ((makeClean && asciiChars) || makeBare)
        TY_(DowngradeTypography)(doc, &doc->root);

    if (makeBare)
        /* Note: no longer replaces &nbsp; in */
        /* attribute values / non-text tokens */
        TY_(NormalizeSpaces)(doc->lexer, &doc->root);
    else
        TY_(ReplacePreformattedSpaces)(doc, &doc->root);

    if ( sortAttrStrat != TidySortAttrNone )
        TY_(SortAttributes)(&doc->root, sortAttrStrat);

    if ( showMarkup && (doc->errors == 0 || forceOutput) )
    {
#if SUPPORT_UTF16_ENCODINGS
        /* Output a Byte Order Mark if required */
        if ( outputBOM || (doc->inputHadBOM && smartBOM) )
            TY_(outBOM)( out );
#endif

        /* No longer necessary. No DOCTYPE == HTML 3.2,
        ** which gives you only the basic character entities,
        ** which are safe in any browser.
        ** if ( !TY_(FindDocType)(doc) )
        **    TY_(SetOptionBool)( doc, TidyNumEntities, yes );
        */

        doc->docOut = out;
        if ( xmlOut && !xhtmlOut )
            TY_(PPrintXMLTree)( doc, TidyTextFormat_Normal, 0, &doc->root );
        else if ( showBodyOnly( doc, bodyOnly ) )
            TY_(PrintBody)( doc );
        else
            TY_(PPrintTree)( doc, TidyTextFormat_Normal, 0, &doc->root );

        TY_(PFlushLine)( doc, 0 );
        doc->docOut = NULL;
    }

    TY_(ResetConfigToSnapshot)( doc );
    return tidyDocStatus( doc );
}

/* Tree traversal functions
**
** The big issue here is the degree to which we should mimic
** a DOM and/or SAX nodes.
**
** Is it 100% possible (and, if so, how difficult is it) to
** emit SAX events from this API?  If SAX events are possible,
** is that 100% of data needed to build a DOM?
*/

TidyNode TIDY_CALL   tidyGetRoot( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    Node* node = NULL;
    if ( impl )
        node = &impl->root;
    return tidyImplToNode( node );
}

TidyNode TIDY_CALL   tidyGetHtml( TidyDoc tdoc )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  Node* node = NULL;
  if ( impl )
      node = TY_(FindHTML)( impl );
  return tidyImplToNode( node );
}

TidyNode TIDY_CALL    tidyGetHead( TidyDoc tdoc )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  Node* node = NULL;
  if ( impl )
      node = TY_(FindHEAD)( impl );
  return tidyImplToNode( node );
}

TidyNode TIDY_CALL    tidyGetBody( TidyDoc tdoc )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  Node* node = NULL;
  if ( impl )
      node = TY_(FindBody)( impl );
  return tidyImplToNode( node );
}

/* parent / child */
TidyNode TIDY_CALL    tidyGetParent( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  return tidyImplToNode( nimp->parent );
}
TidyNode TIDY_CALL    tidyGetChild( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  return tidyImplToNode( nimp->content );
}

/* siblings */
TidyNode TIDY_CALL    tidyGetNext( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  return tidyImplToNode( nimp->next );
}
TidyNode TIDY_CALL    tidyGetPrev( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  return tidyImplToNode( nimp->prev );
}

/* Node info */
TidyNodeType TIDY_CALL tidyNodeGetType( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  TidyNodeType ntyp = TidyNode_Root;
  if ( nimp )
    ntyp = (TidyNodeType) nimp->type;
  return ntyp;
}

uint TIDY_CALL tidyNodeLine( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  uint line = 0;
  if ( nimp )
    line = nimp->line;
  return line;
}
uint TIDY_CALL tidyNodeColumn( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  uint col = 0;
  if ( nimp )
    col = nimp->column;
  return col;
}

ctmbstr TIDY_CALL        tidyNodeGetName( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  ctmbstr nnam = NULL;
  if ( nimp )
    nnam = nimp->element;
  return nnam;
}


Bool TIDY_CALL  tidyNodeHasText( TidyDoc tdoc, TidyNode tnod )
{
  TidyDocImpl* doc = tidyDocToImpl( tdoc );
  if ( doc )
      return TY_(nodeHasText)( doc, tidyNodeToImpl(tnod) );
  return no;
}


Bool TIDY_CALL tidyNodeGetText( TidyDoc tdoc, TidyNode tnod, TidyBuffer* outbuf )
{
	return tidyNodeGetFormattedText(tdoc, tnod, TidyTextFormat_Normal, 0, outbuf); /* [i_a] */
}

Bool TIDY_CALL tidyNodeGetRawText( TidyDoc tdoc, TidyNode tnod, TidyTextFormat mode, int indent, TidyBuffer* outbuf ) /* [i_a] */
{
	Bool ret;
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
	Node* nimp = tidyNodeToImpl( tnod );
    Node* alt_node = TY_(CloneNode)( doc, nimp );
	alt_node->start = nimp->start;
	alt_node->end = nimp->end;
	alt_node->line = nimp->line;
	alt_node->column = nimp->column;
	alt_node->linebreak = nimp->linebreak;
	alt_node->type = TextNode;

	ret = tidyNodeGetFormattedText( tdoc, tidyImplToNode(alt_node), mode, indent, outbuf );

	TY_(FreeNode)( doc, alt_node );

	return ret;
}

Bool TIDY_CALL tidyNodeGetFormattedText( TidyDoc tdoc, TidyNode tnod, TidyTextFormat mode, int indent, TidyBuffer* outbuf ) /* [i_a] */
{
  TidyDocImpl* doc = tidyDocToImpl( tdoc );
  Node* nimp = tidyNodeToImpl( tnod );
  if ( doc && nimp && outbuf )
  {
      uint outenc     = cfg( doc, TidyOutCharEncoding );
      uint nl         = cfg( doc, TidyNewline );
      StreamOut* out  = TY_(BufferOutput)( doc, outbuf, outenc, nl );
      Bool xmlOut     = cfgBool( doc, TidyXmlOut );
      Bool xhtmlOut   = cfgBool( doc, TidyXhtmlOut );

      doc->docOut = out;
      if ( xmlOut && !xhtmlOut )
          TY_(PPrintXMLTree)( doc, (int)mode, indent, nimp ); /* [i_a] */
      else
          TY_(PPrintTree)( doc, (int)mode, indent, nimp ); /* [i_a] */

      TY_(PFlushLine)( doc, 0 );
      doc->docOut = NULL;

      TidyDocFree( doc, out );
      return yes;
  }
  return no;
}

Bool TIDY_CALL tidyNodeGetValue( TidyDoc tdoc, TidyNode tnod, TidyBuffer* buf )
{
    TidyDocImpl *doc = tidyDocToImpl( tdoc );
    Node *node = tidyNodeToImpl( tnod );
    if ( doc == NULL || node == NULL || buf == NULL )
        return no;

    switch( node->type ) {
    case TextNode:
    case CDATATag:
    case CommentTag:
    case ProcInsTag:
    case SectionTag:
    case AspTag:
    case JsteTag:
    case PhpTag:
    {
        tidyBufClear( buf );
        tidyBufAppend( buf, doc->lexer->lexbuf + node->start,
                       node->end - node->start );
        break;
    }
    default:
        /* The node doesn't have a value */
        return no;
    }

    return yes;
}

Bool TIDY_CALL tidyNodeIsProp( TidyDoc ARG_UNUSED(tdoc), TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  Bool isProprietary = yes;
  if ( nimp )
  {
    switch ( nimp->type )
    {
    case RootNode:
    case DocTypeTag:
    case CommentTag:
    case XmlDecl:
    case ProcInsTag:
    case TextNode:
    case CDATATag:
        isProprietary = no;
        break;

    case SectionTag:
    case AspTag:
    case JsteTag:
    case PhpTag:
        isProprietary = yes;
        break;

    case StartTag:
    case EndTag:
    case StartEndTag:
        isProprietary = ( nimp->tag
                          ? (nimp->tag->versions & VERS_PROPRIETARY) != 0
                          : yes );
        break;
    }
  }
  return isProprietary;
}

TidyTagId TIDY_CALL tidyNodeGetId(TidyNode tnod)
{
    Node* nimp = tidyNodeToImpl(tnod);

    return TagId(nimp); /* [i_a] code replaced by previously already existing macro which was meant for this */
}


/* Null for non-element nodes and all pure HTML
cmbstr       tidyNodeNsLocal( TidyNode tnod )
{
}
cmbstr       tidyNodeNsPrefix( TidyNode tnod )
{
}
cmbstr       tidyNodeNsUri( TidyNode tnod )
{
}
*/

/* Iterate over attribute values */
TidyAttr TIDY_CALL   tidyAttrFirst( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  AttVal* attval = NULL;
  if ( nimp )
    attval = nimp->attributes;
  return tidyImplToAttr( attval );
}
TidyAttr TIDY_CALL    tidyAttrNext( TidyAttr tattr )
{
  AttVal* attval = tidyAttrToImpl( tattr );
  AttVal* nxtval = NULL;
  if ( attval )
    nxtval = attval->next;
  return tidyImplToAttr( nxtval );
}

ctmbstr TIDY_CALL       tidyAttrName( TidyAttr tattr )
{
  AttVal* attval = tidyAttrToImpl( tattr );
  ctmbstr anam = NULL;
  if ( attval )
    anam = attval->attribute;
  return anam;
}
ctmbstr TIDY_CALL       tidyAttrValue( TidyAttr tattr )
{
  AttVal* attval = tidyAttrToImpl( tattr );
  ctmbstr aval = NULL;
  if ( attval )
    aval = attval->value;
  return aval;
}

/* Null for pure HTML
ctmbstr       tidyAttrNsLocal( TidyAttr tattr )
{
}
ctmbstr       tidyAttrNsPrefix( TidyAttr tattr )
{
}
ctmbstr       tidyAttrNsUri( TidyAttr tattr )
{
}
*/

TidyAttrId TIDY_CALL tidyAttrGetId( TidyAttr tattr )
{
  AttVal* attval = tidyAttrToImpl( tattr );
  TidyAttrId attrId = TidyAttr_UNKNOWN;
  if ( attval && attval->dict )
    attrId = attval->dict->id;
  return attrId;
}
Bool TIDY_CALL tidyAttrIsProp( TidyAttr tattr )
{
  AttVal* attval = tidyAttrToImpl( tattr );
  Bool isProprietary = yes;
  if ( attval )
    isProprietary = ( attval->dict
                      ? (attval->dict->versions & VERS_PROPRIETARY) != 0
                      : yes );
  return isProprietary;
}







/* [i_a] TODO: html2db patch; complete/clean it for Win32 build of html2db! */
Bool TIDY_CALL tidyDescendantOf( TidyNode tnod, TidyTagId tid )
{
  Node* node = tidyNodeToImpl( tnod );
  if ( node )
	return TY_(DescendantOf)( node, tid );
  return no;
}

Bool TIDY_CALL tidyShouldIndent( TidyDoc tdoc, TidyNode tnod )
{
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
  Node* node = tidyNodeToImpl( tnod );
  if ( doc && node )
	return TY_(ShouldIndent)( doc, node );
  return no;
}

void TIDY_CALL tidyPCondFlushLine( TidyDoc tdoc, uint indent )
{
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
  if ( doc )
	TY_(PCondFlushLine)( doc, indent );
}

void TIDY_CALL tidyPPrintChar( TidyDoc tdoc, uint c, uint mode )
{
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
  if ( doc )
	TY_(PPrintChar)( doc, c, mode );
}

void TIDY_CALL tidyPPrintText( TidyDoc tdoc, uint mode, uint indent, TidyNode tnod )
{
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
  Node* node = tidyNodeToImpl( tnod );
  if ( doc && node )
	TY_(PPrintText)( doc, mode, indent, node );
}

void TIDY_CALL tidyPPrintString( TidyDoc tdoc, uint indent, ctmbstr str )
{
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
  if ( doc )
	TY_(PPrintString)( doc, indent, str );
}

void TIDY_CALL tidyPPrintComment( TidyDoc tdoc, uint indent, TidyNode tnod )
{
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
  Node* node = tidyNodeToImpl( tnod );
  if ( doc && node )
	TY_(PPrintComment)( doc, indent, node );
}

void TIDY_CALL tidyPPrintPI( TidyDoc tdoc, uint indent, TidyNode tnod )
{
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
  Node* node = tidyNodeToImpl( tnod );
  if ( doc && node )
	TY_(PPrintPI)( doc, indent, node );
}

void TIDY_CALL tidyPPrintXmlDecl( TidyDoc tdoc, uint indent, TidyNode tnod )
{
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
  Node* node = tidyNodeToImpl( tnod );
  if ( doc && node )
	TY_(PPrintXmlDecl)( doc, indent, node );
}

void TIDY_CALL tidyPPrintCDATA( TidyDoc tdoc, uint indent, TidyNode tnod )
{
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
  Node* node = tidyNodeToImpl( tnod );
  if ( doc && node )
	TY_(PPrintCDATA)( doc, indent, node );
}

void TIDY_CALL tidyPPrintSection( TidyDoc tdoc, uint indent, TidyNode tnod )
{
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
  Node* node = tidyNodeToImpl( tnod );
  if ( doc && node )
	TY_(PPrintSection)( doc, indent, node );
}

void TIDY_CALL tidyPFlushLine( TidyDoc tdoc, uint indent )
{
	TidyDocImpl* doc = tidyDocToImpl( tdoc );
  if ( doc )
	TY_(PFlushLine)( doc, indent );
}


/*
 * local variables:
 * mode: c
 * indent-tabs-mode: nil
 * c-basic-offset: 4
 * eval: (c-set-offset 'substatement-open 0)
 * end:
 */
