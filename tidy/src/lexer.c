/*
  lexer.c - Lexer for html parser
  
  (c) 1998-2001 (W3C) MIT, INRIA, Keio University
  See tidy.c for the copyright notice.
  
  CVS Info :

    $Author$ 
    $Date$ 
    $Revision$ 

*/

/*
  Given a file stream fp it returns a sequence of tokens.

     GetToken(fp) gets the next token
     UngetToken(fp) provides one level undo

  The tags include an attribute list:

    - linked list of attribute/value nodes
    - each node has 2 null-terminated strings.
    - entities are replaced in attribute values

  white space is compacted if not in preformatted mode
  If not in preformatted mode then leading white space
  is discarded and subsequent white space sequences
  compacted to single space chars.

  If XmlTags is no then Tag names are folded to upper
  case and attribute names to lower case.

 Not yet done:
    -   Doctype subset and marked sections
*/

#include "platform.h"
#include "html.h"

extern char *release_date;

AttVal *ParseAttrs(Lexer *lexer, Bool *isempty);  /* forward references */
Node *CommentToken(Lexer *lexer);
static char *ParseAttribute(Lexer *lexer, Bool *isempty, Node **asp, Node **php);
static char *ParseValue(Lexer *lexer, char *name, Bool foldCase, Bool *isempty, int *pdelim);

/* used to classify chars for lexical purposes */
#define MAP(c) ((unsigned)c < 128 ? lexmap[(unsigned)c] : 0)
uint lexmap[128];

#define XHTML_NAMESPACE "http://www.w3.org/1999/xhtml"

/* the 3 URIs  for the XHTML 1.0 DTDs */
#define voyager_loose    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"
#define voyager_strict   "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"
#define voyager_frameset "http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd"

/* URI for XHTML 1.1 and XHTML Basic 1.0 */
#define voyager_11       "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd"
#define voyager_basic    "http://www.w3.org/TR/xhtml-basic/xhtml-basic10.dtd"

#define W3C_VERSIONS 10

struct _vers
{
    char *name;
    char *voyager_name;
    char *profile;
    int code;
} W3C_Version[] =
{
    {"HTML 4.01", "XHTML 1.0 Strict", voyager_strict, VERS_HTML40_STRICT},
    {"HTML 4.01 Transitional", "XHTML 1.0 Transitional", voyager_loose, VERS_HTML40_LOOSE},
    {"HTML 4.01 Frameset", "XHTML 1.0 Frameset", voyager_frameset, VERS_FRAMESET},
    {"HTML 4.0", "XHTML 1.0 Strict", voyager_strict, VERS_HTML40_STRICT},
    {"HTML 4.0 Transitional", "XHTML 1.0 Transitional", voyager_loose, VERS_HTML40_LOOSE},
    {"HTML 4.0 Frameset", "XHTML 1.0 Frameset", voyager_frameset, VERS_FRAMESET},
    {"HTML 3.2", "XHTML 1.0 Transitional", voyager_loose, VERS_HTML32},
    {"HTML 3.2 Final", "XHTML 1.0 Transitional", voyager_loose, VERS_HTML32},
    {"HTML 3.2 Draft", "XHTML 1.0 Transitional", voyager_loose, VERS_HTML32},
    {"HTML 2.0", "XHTML 1.0 Strict", voyager_strict, VERS_HTML20}
};

/* everything is allowed in proprietary version of HTML */
/* this is handled here rather than in the tag/attr dicts */
void ConstrainVersion(Lexer *lexer, uint vers)
{
    lexer->versions &= (vers | VERS_PROPRIETARY);
}

Bool IsWhite(uint c)
{
    uint map = MAP(c);

    return (Bool)(map & white);
}

Bool IsDigit(uint c)
{
    uint map;

    map = MAP(c);

    return (Bool)(map & digit);
}

Bool IsLetter(uint c)
{
    uint map;

    map = MAP(c);

    return (Bool)(map & letter);
}

Bool IsNamechar(uint c)
{
    uint map;

    map = MAP(c);

    return (Bool)(map & namechar);
}

Bool IsLower(uint c)
{
    uint map = MAP(c);

    return (Bool)(map & lowercase);
}

Bool IsUpper(uint c)
{
    uint map = MAP(c);

    return (Bool)(map & uppercase);
}

uint ToLower(uint c)
{
    uint map = MAP(c);

    if (map & uppercase)
        c += 'a' - 'A';

    return c;
}

uint ToUpper(uint c)
{
    uint map = MAP(c);

    if (map & lowercase)
        c += 'A' - 'a';

    return c;
}

char FoldCase(char c, Bool tocaps)
{
    if (!XmlTags)
    {
        if (tocaps)
        {
            c = ToUpper(c);
        }
        else /* force to lower case */
        {
            c = ToLower(c);
        }
    }

    return c;
}


/*
 return last char in string
 this is useful when trailing quotemark
 is missing on an attribute
*/
static int LastChar(char *str)
{
    int n;

    if (str != null && *str != '\0')
    {
        n = wstrlen(str);
        return str[n-1];
    }

    return 0;
}

/*
   node->type is one of these:

    #define TextNode    1
    #define StartTag    2
    #define EndTag      3
    #define StartEndTag 4
*/
Lexer *NewLexer(StreamIn *in)
{
    Lexer *lexer;

    lexer = (Lexer *)MemAlloc(sizeof(Lexer));

    if (lexer != null)
    {
        lexer->in = in;
        lexer->lines = 1;
        lexer->columns = 1;
        lexer->state = LEX_CONTENT;
        lexer->badAccess = 0;
        lexer->badLayout = 0;
        lexer->badChars = 0;
        lexer->badForm = 0;
        lexer->warnings = 0;
        lexer->errors = no;
        lexer->waswhite = no;
        lexer->pushed = no;
        lexer->insertspace = no;
        lexer->exiled = no;
        lexer->isvoyager = no;
        lexer->versions = (VERS_ALL|VERS_PROPRIETARY);
        lexer->doctype = VERS_UNKNOWN;
        lexer->bad_doctype = no;
        lexer->txtstart = 0;
        lexer->txtend = 0;
        lexer->token = null;
        lexer->lexbuf =  null;
        lexer->lexlength = 0;
        lexer->lexsize = 0;
        lexer->inode = null;
        lexer->insert = null;
        lexer->istack = null;
        lexer->istacklength = 0;
        lexer->istacksize = 0;
        lexer->istackbase = 0;
        lexer->styles = null;
    }


    return lexer;
}

Bool EndOfInput(Lexer *lexer)
{
    return  (feof(lexer->in->file));
}

void FreeLexer(Lexer *lexer)
{
    if (lexer->pushed)
        FreeNode(lexer->token);

    if (lexer->lexbuf != null)
        MemFree(lexer->lexbuf);

    while (lexer->istacksize > 0)
        PopInline(lexer, null);

    if (lexer->istack)
        MemFree(lexer->istack);

    if (lexer->styles)
        FreeStyles(lexer);

    MemFree(lexer);
}

void AddByte(Lexer *lexer, uint c)
{
    if (lexer->lexsize + 1 >= lexer->lexlength)
    {
        while (lexer->lexsize + 1 >= lexer->lexlength)
        {
            if (lexer->lexlength == 0)
                lexer->lexlength = 8192;
            else
                lexer->lexlength = lexer->lexlength * 2;
        }

        lexer->lexbuf = (char *)MemRealloc(lexer->lexbuf, lexer->lexlength*sizeof(char));
    }

    lexer->lexbuf[lexer->lexsize++] = (char)c;
    lexer->lexbuf[lexer->lexsize] = '\0';  /* debug */
}

static void ChangeChar(Lexer *lexer, char c)
{
    if (lexer->lexsize > 0)
    {
        lexer->lexbuf[lexer->lexsize-1] = c;
    }
}

/* store char c as UTF-8 encoded byte stream */
void AddCharToLexer(Lexer *lexer, uint c)
{
    int i, err, count = 0;
    unsigned char buf[10];
    
    err = EncodeCharToUTF8Bytes(c, buf, null, null, &count);
    if (err)
    {
#if 0
        extern FILE* errout; /* debug */

        tidy_out(errout, "lexer UTF-8 encoding error for U+%x : ", c); /* debug */
#endif
        /* replacement char 0xFFFD encoded as UTF-8 */
        buf[0] = 0xEF;
        buf[1] = 0xBF;
        buf[2] = 0xBD;
        count = 3;
    }
    
    for (i = 0; i < count; i++)
        AddByte(lexer, (uint)buf[i]);
}

static void AddStringToLexer(Lexer *lexer, char *str)
{
    uint c;

    while((c = (unsigned char)*str++))
        AddCharToLexer(lexer, c);
}

/*
  No longer attempts to insert missing ';' for unknown
  enitities unless one was present already, since this
  gives unexpected results.

  For example:   <a href="something.htm?foo&bar&fred">
  was tidied to: <a href="something.htm?foo&amp;bar;&amp;fred;">
  rather than:   <a href="something.htm?foo&amp;bar&amp;fred">

  My thanks for Maurice Buxton for spotting this.

  Also Randy Waki pointed out the following case for the
  04 Aug 00 version (bug #433012):
  
  For example:   <a href="something.htm?id=1&lang=en">
  was tidied to: <a href="something.htm?id=1&lang;=en">
  rather than:   <a href="something.htm?id=1&amp;lang=en">
  
  where "lang" is a known entity (#9001), but browsers would
  misinterpret "&lang;" because it had a value > 256.
  
  So the case of an apparently known entity with a value > 256 and
  missing a semicolon is handled specially.
  
  "ParseEntity" is also a bit of a misnomer - it handles entities and
  numeric character references. Invalid NCR's are now reported.

  */
static void ParseEntity(Lexer *lexer, int mode)
{
    uint start;
    Bool first = yes, semicolon = no;
    int c, ch, startcol;

    start = lexer->lexsize - 1;  /* to start at "&" */
    startcol = lexer->in->curcol - 1;

    while ((c = ReadChar(lexer->in)) != EndOfStream)
    {
        if (c == ';')
        {
            semicolon = yes;
            break;
        }

        if (first && c == '#')
        {
#if SUPPORT_ASIAN_ENCODINGS

            /* #431953 - start RJ */
            if ( NCR == no || inCharEncoding == BIG5 || inCharEncoding == SHIFTJIS )
            {
                UngetChar('#', lexer->in);
                return;
            }
            /* #431953 - end RJ */

#endif

            AddCharToLexer(lexer, c);
            first = no;
            continue;
        }

        first = no;

        if (IsNamechar(c))
        {
            AddCharToLexer(lexer, c);
            continue;
        }

        /* otherwise put it back */

        UngetChar(c, lexer->in);
        break;
    }

    /* make sure entity is null terminated */
    lexer->lexbuf[lexer->lexsize] = '\0';

    if ((wstrcmp(lexer->lexbuf+start, "&apos") == 0)
        && !XmlOut
        && !lexer->isvoyager
        && !xHTML)
        ReportEntityError(lexer, APOS_UNDEFINED, lexer->lexbuf+start, 39);

    ch = EntityCode(lexer->lexbuf+start);

    /* deal with unrecognized or invalid entities */
    /* #433012 - fix by Randy Waki 17 Feb 01 */
    /* report invalid NCR's - Terry Teague 01 Sep 01 */
    if (ch <= 0 || (ch >= 128 && ch <= 159) || (ch >= 256 && c != ';'))
    {
        /* set error position just before offending character */
        lexer->lines = lexer->in->curline;
        lexer->columns = startcol;

        if (lexer->lexsize > start + 1)
        {
            if (ch >= 128 && ch <= 159)
            {
                /* invalid numeric character reference */
                
                int c1, replaceMode;
            
                if (ReplacementCharEncoding == WIN1252)
                    c1 = DecodeWin1252(ch);
                else if (ReplacementCharEncoding == MACROMAN)
                    c1 = DecodeMacRoman(ch);

                replaceMode = c1?REPLACED_CHAR:DISCARDED_CHAR;
                
               if (c != ';')    /* issue warning if not terminated by ';' */
                   ReportEntityError(lexer, MISSING_SEMICOLON_NCR, lexer->lexbuf+start, c);
 
                ReportEncodingError(lexer, INVALID_NCR | replaceMode, ch);
                
                if (c1 != 0)
                {
                    /* make the replacement */
                    lexer->lexsize = start;
                    AddCharToLexer(lexer, c1);
                    semicolon = no;
                }
                else
                {
                    /* discard */
                    lexer->lexsize = start;
                    semicolon = no;
               }
               
            }
            else
                ReportEntityError(lexer, UNKNOWN_ENTITY, lexer->lexbuf+start, ch);

            if (semicolon)
                AddCharToLexer(lexer, ';');
        }
        else /* naked & */
            ReportEntityError(lexer, UNESCAPED_AMPERSAND, lexer->lexbuf+start, ch);
    }
    else
    {
        if (c != ';')    /* issue warning if not terminated by ';' */
        {
            /* set error position just before offending chararcter */
            lexer->lines = lexer->in->curline;
            lexer->columns = startcol;
            ReportEntityError(lexer, MISSING_SEMICOLON, lexer->lexbuf+start, c);
        }

        lexer->lexsize = start;

        if (ch == 160 && (mode & Preformatted))
            ch = ' ';

        AddCharToLexer(lexer, ch);

        if (ch == '&' && !QuoteAmpersand)
        {
            AddCharToLexer(lexer, 'a');
            AddCharToLexer(lexer, 'm');
            AddCharToLexer(lexer, 'p');
            AddCharToLexer(lexer, ';');
        }
    }
}

static char ParseTagName(Lexer *lexer)
{
    uint c;

    /* fold case of first char in buffer */

    c = lexer->lexbuf[lexer->txtstart];

    if (!XmlTags && IsUpper(c))
    {
        lexer->lexbuf[lexer->txtstart] = ToLower(c);
    }

    while ((c = ReadChar(lexer->in)) != EndOfStream)
    {
        if (!IsNamechar(c))
            break;

       /* fold case of subsequent chars */

       if (!XmlTags && IsUpper(c))
            c = ToLower(c);

       AddCharToLexer(lexer, c);
    }

    lexer->txtend = lexer->lexsize;
    return c;
}

/*
  Used for elements and text nodes
  element name is null for text nodes
  start and end are offsets into lexbuf
  which contains the textual content of
  all elements in the parse tree.

  parent and content allow traversal
  of the parse tree in any direction.
  attributes are represented as a linked
  list of AttVal nodes which hold the
  strings for attribute/value pairs.
*/
Node *NewNode(void)
{
    Node *node;

    node = (Node *)MemAlloc(sizeof(Node));

    node->parent = null;
    node->prev = null;
    node->next = null;
    node->last = null;
    node->start = 0;
    node->end = 0;
    node->type = TextNode;
    node->closed = no;
    node->implicit = no;
    node->linebreak = no;
    node->tag = null;
    node->was = null;
    node->element = null;
    node->attributes = null;
    node->content = null;
    return node;
}

/* used to clone heading nodes when split by an <HR> */
Node *CloneNode(Lexer *lexer, Node *element)
{
    Node *node;

    node = NewNode();
    node->parent = element->parent;
    node->start = lexer->lexsize;
    node->end = lexer->lexsize;
    node->type = element->type;
    node->closed = element->closed;
    node->implicit = element->implicit;
    node->tag = element->tag;
    node->element = wstrdup(element->element);
    node->attributes = DupAttrs(element->attributes);
    return node;
}

/* free node's attributes */
void FreeAttrs(Node *node)
{
    AttVal *av;

    while (node->attributes)
    {
        av = node->attributes;

        if (av->attribute)
        {
            if ((wstrcasecmp(av->attribute, "id") == 0) ||
               ((wstrcasecmp(av->attribute, "name") == 0) &&
               IsAnchorElement(node)))
                RemoveAnchorByNode(node);

            MemFree(av->attribute);
        }

        if (av->value)
            MemFree(av->value);

        if (av->asp)
            FreeNode(av->asp);

        if (av->php)
            FreeNode(av->php);

        node->attributes = av->next;
        MemFree(av);
    }
}

/* doesn't repair attribute list linkage */
void FreeAttribute(AttVal *av)
{
    if (av->attribute)
        MemFree(av->attribute);
    if (av->value)
        MemFree(av->value);

    MemFree(av);
}

/* remove attribute from node then free it */
void RemoveAttribute(Node *node, AttVal *attr)
{
    AttVal *av, *prev = null, *next;

    for (av = node->attributes; av != null; av = next)
    {
        next = av->next;

        if (av == attr)
        {
            if (prev)
                prev->next = next;
            else
                node->attributes = next;
        }
        else
            prev = av;
    }

    FreeAttribute(attr);
}

/*
  Free document nodes by iterating through peers and recursing
  through children. Set next to null before calling FreeNode()
  to avoid freeing peer nodes. Doesn't patch up prev/next links.
 */
void FreeNode(Node *node)
{
    Node *next;

    while (node)
    {
        if (node->attributes)
            FreeAttrs(node);

        if (node->element)
            MemFree(node->element);

        if (node->content)
            FreeNode(node->content);

        if (node->next)
        {
            next = node->next;
            MemFree(node);
            node = next;
            continue;
        }

        node->element = null;
        node->tag = null;

#if 0
        if (_msize(node) != sizeof (Node)) /* debug */
            fprintf(stderr, 
            "Error in FreeNode() - trying to free corrupted node size %d vs %d\n",
                _msize(node), sizeof(Node));
#endif
        MemFree(node);
        break;
    }
}

Node *TextToken(Lexer *lexer)
{
    Node *node;

    node = NewNode();
    node->start = lexer->txtstart;
    node->end = lexer->txtend;
    return node;
}

/* used for creating preformatted text from Word2000 */
Node *NewLineNode(Lexer *lexer)
{
    Node *node = NewNode();

    node->start = lexer->lexsize;
    AddCharToLexer(lexer, (uint)'\n');
    node->end = lexer->lexsize;
    return node;
}

/* used for adding a &nbsp; for Word2000 */
Node *NewLiteralTextNode(Lexer *lexer, char* txt )
{
    Node *node = NewNode();

    node->start = lexer->lexsize;
    AddStringToLexer(lexer, txt);
    node->end = lexer->lexsize;
    return node;
}

static Node *TagToken(Lexer *lexer, uint type)
{
    Node *node;

    node = NewNode();
    node->type = type;
    node->element = wstrndup(lexer->lexbuf + lexer->txtstart,
                        lexer->txtend - lexer->txtstart);
    node->start = lexer->txtstart;
    node->end = lexer->txtstart;

    if (type == StartTag || type == StartEndTag || type == EndTag)
        FindTag(node);

    return node;
}

Node *CommentToken(Lexer *lexer)
{
    Node *node;

    node = NewNode();
    node->type = CommentTag;
    node->start = lexer->txtstart;
    node->end = lexer->txtend;
    return node;
}


static Node *DocTypeToken(Lexer *lexer)
{
    Node *node;

    node = NewNode();
    node->type = DocTypeTag;
    node->start = lexer->txtstart;
    node->end = lexer->txtend;
    return node;
}


static Node *PIToken(Lexer *lexer)
{
    Node *node;

    node = NewNode();
    node->type = ProcInsTag;
    node->start = lexer->txtstart;
    node->end = lexer->txtend;
    return node;
}

static Node *AspToken(Lexer *lexer)
{
    Node *node;

    node = NewNode();
    node->type = AspTag;
    node->start = lexer->txtstart;
    node->end = lexer->txtend;
    return node;
}

static Node *JsteToken(Lexer *lexer)
{
    Node *node;

    node = NewNode();
    node->type = JsteTag;
    node->start = lexer->txtstart;
    node->end = lexer->txtend;
    return node;
}

/* Added by Baruch Even - handle PHP code too. */
static Node *PhpToken(Lexer *lexer)
{
    Node *node;

    node = NewNode();
    node->type = PhpTag;
    node->start = lexer->txtstart;
    node->end = lexer->txtend;
    return node;
}

/* XML Declaration <?xml version='1.0' ... ?> */
static Node *XmlDeclToken(Lexer *lexer)
{
    Node *node;

    node = NewNode();
    node->type = XmlDecl;
    node->start = lexer->txtstart;
    node->end = lexer->txtend;
    return node;
}

/* Word2000 uses <![if ... ]> and <![endif]> */
static Node *SectionToken(Lexer *lexer)
{
    Node *node;

    node = NewNode();
    node->type = SectionTag;
    node->start = lexer->txtstart;
    node->end = lexer->txtend;
    return node;
}

/* CDATA uses <![CDATA[ ... ]]> */
static Node *CDATAToken(Lexer *lexer)
{
    Node *node;

    node = NewNode();
    node->type = CDATATag;
    node->start = lexer->txtstart;
    node->end = lexer->txtend;
    return node;
}

void AddStringLiteral(Lexer *lexer, char *str)
{
    unsigned char c;

    while((c = *str++) != '\0')
        AddCharToLexer(lexer, c);
}

void AddStringLiteralLen(Lexer *lexer, char *str, int len )
{
    unsigned char c;
    int ix;

    for ( ix=0; ix < len && (c = *str++) != '\0'; ++ix )
        AddCharToLexer(lexer, c);
}

/* find doctype element */
Node *FindDocType(Node *root)
{
    Node *node;

    for (node = root->content; 
            node && node->type != DocTypeTag; node = node->next);

    return node;
}

/* find html element */
Node *FindHTML(Node *root)
{
    Node *node;

    for (node = root->content; 
            node && node->tag != tag_html; node = node->next);

    return node;
}

Node *FindHEAD(Node *root)
{
    Node *node;

    node = FindHTML(root);

    if (node)
    {
        for (node = node->content;
            node && node->tag != tag_head; node = node->next);
    }

    return node;
}

Node *FindBody(Node *root)
{
    Node *node;

    node = root->content;

    while (node && node->tag != tag_html)
        node = node->next;

    if (node == null)
        return null;

    node = node->content;

    while (node && node->tag != tag_body)
        node = node->next;

    return node;
}

/* add meta element for Tidy */
Bool AddGenerator(Lexer *lexer, Node *root)
{
    AttVal *attval;
    Node *node;
    Node *head = FindHEAD(root);
    char buf[256];
    
    if (head)
    {
#ifdef PLATFORM_NAME
        sprintf(buf, "HTML Tidy for "PLATFORM_NAME" (vers %s), see www.w3.org", release_date);
#else
        sprintf(buf, "HTML Tidy (vers %s), see www.w3.org", release_date);
#endif

        for (node = head->content; node; node = node->next)
        {
            if (node->tag == tag_meta)
            {
                attval = GetAttrByName(node, "name");

                if (attval && attval->value &&
                    wstrcasecmp(attval->value, "generator") == 0)
                {
                    attval = GetAttrByName(node, "content");

                    if (attval && attval->value &&
                        wstrncasecmp(attval->value, "HTML Tidy", 9) == 0)
                    {
                        /* update the existing content to reflect the */
                        /* actual version of Tidy currently being used */
                        
                        MemFree(attval->value);
                        attval->value = wstrdup(buf);
                        
                        return no;
                    }
                }
            }
        }

        node = InferredTag(lexer, "meta");
        AddAttribute(node, "content", buf);
        AddAttribute(node, "name", "generator");
        InsertNodeAtStart(head, node);
        return yes;
    }

    return no;
}

/* examine <!DOCTYPE> to identify version */
static int FindGivenVersion(Lexer *lexer, Node *doctype)
{
    char *p, *s = lexer->lexbuf+doctype->start;
    uint i, j;
    int len;

    /* if root tag for doctype isn't html give up now */
    if (wstrncasecmp(s, "html ", 5) != 0)
        return 0;

    s += 5; /* if all is well s -> SYSTEM or PUBLIC */

    if  (!CheckDocTypeKeyWords(lexer, doctype))
            ReportWarning(lexer, doctype, null, DTYPE_NOT_UPPER_CASE);


    /* give up if all we are given is the system id for the doctype */
    if (wstrncasecmp(s, "SYSTEM ", 7) == 0)
    {
        /* but at least ensure the case is correct */
        if (wstrncmp(s, "SYSTEM", 6) != 0)
            memcpy(s, "SYSTEM", 6);

        return 0;  /* unrecognized */
    }

    if (wstrncasecmp(s, "PUBLIC ", 7) == 0)
    {
        if (wstrncmp(s, "PUBLIC", 6) != 0)
            memcpy(s, "PUBLIC", 6);
    }
    else
        lexer->bad_doctype = yes;

    for (i = doctype->start; i < doctype->end; ++i)
    {
        if (lexer->lexbuf[i] == '"')
        {
            if (wstrncmp(lexer->lexbuf+i+1, "-//W3C//DTD ", 12) == 0)
            {
                p = lexer->lexbuf + i + 13;

                /* compute length of identifier e.g. "HTML 4.0 Transitional" */
                for (j = i + 13; j < doctype->end && lexer->lexbuf[j] != '/'; ++j);
                len = j - i - 13;

                for (j = 1; j < W3C_VERSIONS; ++j)
                {
                    s = W3C_Version[j].name;
                    if (len == wstrlen(s) && wstrncmp(p, s, len) == 0)
                        return W3C_Version[j].code;
                }

                /* else unrecognized version */
            }
            else if (wstrncmp(lexer->lexbuf+i+1, "-//IETF//DTD ", 13) == 0)
            {
                p = lexer->lexbuf + i + 14;

                /* compute length of identifier e.g. "HTML 2.0" */
                for (j = i + 14; j < doctype->end && lexer->lexbuf[j] != '/'; ++j);
                len = j - i - 14;

                s = W3C_Version[0].name;
                if (len == wstrlen(s) && wstrncmp(p, s, len) == 0)
                    return W3C_Version[0].code;

                /* else unrecognized version */
            }
            break;
        }
    }

    return 0;
}

/* return true if substring s is in p and isn't all in upper case */
/* this is used to check the case of SYSTEM, PUBLIC, DTD and EN */
/* len is how many chars to check in p */
static Bool FindBadSubString(char *s, char *p, int len)
{
    int n = wstrlen(s);

    while (n < len)
    {
        if (wstrncasecmp(s, p, n) == 0)
            return (wstrncmp(s, p, n) != 0);

        ++p;
        --len;
    }

    return 0;
}

Bool CheckDocTypeKeyWords(Lexer *lexer, Node *doctype)
{
    char *s = lexer->lexbuf+doctype->start;
    int len = doctype->end - doctype->start;

    return !(
        FindBadSubString("SYSTEM", s, len) ||
        FindBadSubString("PUBLIC", s, len) ||
        FindBadSubString("//DTD", s, len) ||
        FindBadSubString("//W3C", s, len) ||
        FindBadSubString("//EN", s, len)
        );
}

char *HTMLVersionName(Lexer *lexer)
{
    int guessed, j;

    guessed = ApparentVersion(lexer);

    for (j = 0; j < W3C_VERSIONS; ++j)
    {
        if (guessed == W3C_Version[j].code)
        {
            if (lexer->isvoyager)
                return W3C_Version[j].voyager_name;

            return W3C_Version[j].name;
        }
    }

    return null;
}

static void FixHTMLNameSpace(Lexer *lexer, Node *root, char *profile)
{
    Node *node;
    AttVal *prev, *attr;

    for (node = root->content; 
            node && node->tag != tag_html; node = node->next);

    if (node)
    {
        prev = null;

        for (attr = node->attributes; attr; attr = attr->next)
        {
            if (wstrcmp(attr->attribute, "xmlns") == 0)
                break;

            prev = attr;
        }

        if (attr)
        {
            if (wstrcmp(attr->value, profile))
            {
                ReportWarning(lexer, node, null, INCONSISTENT_NAMESPACE);
                MemFree(attr->value);
                attr->value = wstrdup(profile);
            }
        }
        else
        {
            attr = NewAttribute();
            attr->delim = '"';
            attr->attribute = wstrdup("xmlns");
            attr->value = wstrdup(profile);
            attr->dict = FindAttribute(attr);
            attr->next = node->attributes;
            node->attributes = attr;
        }
    }
}


/* Put DOCTYPE declaration between the
** <?xml version "1.0" ... ?> declaration, if any,
** and the <html> tag.  Should also work for any comments, 
** etc. that may precede the <html> tag.
*/

static Node* NewXhtmlDocTypeNode( Node* root )
{
    Node *doctype, *html;

    html = FindHTML( root );
    if ( !html )
        return null;

    doctype = NewNode();
    doctype->type = DocTypeTag;
    doctype->next = html;
    doctype->parent = root;

    if ( html == root->content )
    {
        /* No <?xml ... ?> declaration. */
        root->content->prev = doctype;
        root->content = doctype;
        doctype->prev = null;
    }
    else
    {
        /* we have an <?xml ... ?> declaration. */
        doctype->prev = html->prev;
        doctype->prev->next = doctype;
    }
    html->prev = doctype;
    return doctype;
}

Bool SetXHTMLDocType(Lexer *lexer, Node *root)
{
    char *fpi, *sysid, *dtdsub, *name_space = XHTML_NAMESPACE;
    Node *doctype;
    int dtdlen = 0;

    doctype = FindDocType(root);

    FixHTMLNameSpace(lexer, root, name_space); /* #427839 - fix by Evan Lenz 05 Sep 00 */

    if (doctype_mode == doctype_omit)
    {
        if (doctype)
            DiscardElement(doctype);

        return yes;
    }

    if (doctype_mode == doctype_auto)
    {
        /* see what flavor of XHTML this document matches */
        if (lexer->versions & VERS_HTML40_STRICT)
        {  /* use XHTML strict */
            fpi = "-//W3C//DTD XHTML 1.0 Strict//EN";
            sysid = voyager_strict;
        }
        else if (lexer->versions & VERS_LOOSE)
        {
            fpi = "-//W3C//DTD XHTML 1.0 Transitional//EN";
            sysid = voyager_loose;
        }
        else if (lexer->versions & VERS_FRAMESET)
        {   /* use XHTML frames */
            fpi = "-//W3C//DTD XHTML 1.0 Frameset//EN";
            sysid = voyager_frameset;
        }
        else /* proprietary */
        {
            fpi = null;
            sysid = "";
        
            if (doctype)/* #473490 - fix by Bj�rn H�hrmann 10 Oct 01 */
                DiscardElement(doctype);
        }
    }
    else if (doctype_mode == doctype_strict)
    {
        fpi = "-//W3C//DTD XHTML 1.0 Strict//EN";
        sysid = voyager_strict;
    }
    else if (doctype_mode == doctype_loose)
    {
        fpi = "-//W3C//DTD XHTML 1.0 Transitional//EN";
        sysid = voyager_loose;
    }

    if (doctype_mode == doctype_user && doctype_str)
    {
        fpi = doctype_str;
        sysid = "";
    }

    if (!fpi)
        return no;

    if (doctype)
    {
      /* Look for internal DTD subset */
      if ( xHTML || XmlOut )
      {
        char* start = lexer->lexbuf + doctype->start;
        int len = doctype->end - doctype->start + 1;
        int dtdbeg = wstrnchr( start, len, '[' );
        if ( dtdbeg >= 0 )
        {
          int dtdend = wstrnchr( start+dtdbeg, len-dtdbeg, ']' );
          if ( dtdend >= 0 )
          {
            dtdlen = dtdend + 1;
            dtdsub = start + dtdbeg;
          }
        }
      }
    }
    else
    {
        if ( !(doctype = NewXhtmlDocTypeNode( root )) )
            return no;
    }

    lexer->txtstart = lexer->txtend = lexer->lexsize;

   /* add public identifier */
    AddStringLiteral(lexer, "html PUBLIC ");

    /* check if the fpi is quoted or not */
    if (fpi[0] == '"')
        AddStringLiteral(lexer, fpi);
    else
    {
        AddStringLiteral(lexer, "\"");
        AddStringLiteral(lexer, fpi);
        AddStringLiteral(lexer, "\"");
    }

    if ((unsigned)(wstrlen(sysid) + 6) >= wraplen)
        AddStringLiteral(lexer, "\n\"");
    else
        AddStringLiteral(lexer, "\n    \"");

    /* add system identifier */
    AddStringLiteral(lexer, sysid);
    AddStringLiteral(lexer, "\"");

    if ( dtdlen > 0 && dtdsub )
    {
      AddCharToLexer( lexer, ' ' );
      AddStringLiteralLen( lexer, dtdsub, dtdlen );
    }

    lexer->txtend = lexer->lexsize;

    doctype->start = lexer->txtstart;
    doctype->end = lexer->txtend;

    return no;
}

int ApparentVersion(Lexer *lexer)
{
    switch (lexer->doctype)
    {
    case VERS_UNKNOWN:
        return HTMLVersion(lexer);

    case VERS_HTML20:
        if (lexer->versions & VERS_HTML20)
            return VERS_HTML20;

        break;

    case VERS_HTML32:
        if (lexer->versions & VERS_HTML32)
            return VERS_HTML32;

        break; /* to replace old version by new */

    case VERS_HTML40_STRICT:
        if (lexer->versions & VERS_HTML40_STRICT)
            return VERS_HTML40_STRICT;

        break;

    case VERS_HTML40_LOOSE:
        if (lexer->versions & VERS_HTML40_LOOSE)
            return VERS_HTML40_LOOSE;

        break; /* to replace old version by new */

    case VERS_FRAMESET:
        if (lexer->versions & VERS_FRAMESET)
            return VERS_FRAMESET;

        break;
    }

    /* 
     kludge to avoid error appearing at end of file
     it would be better to note the actual position
     when first encountering the doctype declaration
    */
    lexer->lines = 1;
    lexer->columns = 1;
    ReportWarning(lexer, null, null, INCONSISTENT_VERSION);
    return HTMLVersion(lexer);
}


/* fixup doctype if missing */
Bool FixDocType(Lexer *lexer, Node *root)
{
    Node *doctype;
    int guessed = VERS_HTML40_STRICT, i;

    if (lexer->bad_doctype)
        ReportWarning(lexer, null, null, MALFORMED_DOCTYPE);

    doctype = FindDocType(root);

    if (doctype_mode == doctype_omit)
    {
        if (doctype)
            DiscardElement(doctype);

        return yes;
    }

    if (XmlOut)
        return yes;

    if (doctype_mode == doctype_strict)
    {
        DiscardElement(doctype);
        doctype = null;
        guessed = VERS_HTML40_STRICT;
    }
    else if (doctype_mode == doctype_loose)
    {
        DiscardElement(doctype);
        doctype = null;
        guessed = VERS_HTML40_LOOSE;
    }
    else if (doctype_mode == doctype_auto)
    {
        if (doctype)
        {
            if (lexer->doctype == VERS_UNKNOWN)
                return no;

            switch (lexer->doctype)
            {
            case VERS_UNKNOWN:
                return no;

            case VERS_HTML20:
                if (lexer->versions & VERS_HTML20)
                    return yes;

                break; /* to replace old version by new */

            case VERS_HTML32:
                if (lexer->versions & VERS_HTML32)
                    return yes;

                break; /* to replace old version by new */

            case VERS_HTML40_STRICT:
                if (lexer->versions & VERS_HTML40_STRICT)
                    return yes;

                break; /* to replace old version by new */

            case VERS_HTML40_LOOSE:
                if (lexer->versions & VERS_HTML40_LOOSE)
                    return yes;

                break; /* to replace old version by new */

            case VERS_FRAMESET:
                if (lexer->versions & VERS_FRAMESET)
                    return yes;

                break; /* to replace old version by new */
            }

            /* INCONSISTENT_VERSION warning is now issued by ApparentVersion() */
        }

        /* choose new doctype */
        guessed = HTMLVersion(lexer);
    }

    if (guessed == VERS_UNKNOWN)
        return no;

    /* for XML use the Voyager system identifier */
    if (XmlOut || XmlTags || lexer->isvoyager)
    {
        if (doctype)
            DiscardElement(doctype);

        FixHTMLNameSpace(lexer, root, XHTML_NAMESPACE);

        /*  Namespace is the same for all XHTML variants
        **  Also, don't return yet.  Still need to add
        **  DOCTYPE declaration.
        **  
        **  for (i = 0; i < W3C_VERSIONS; ++i)
        **  {
        **      if (guessed == W3C_Version[i].code)
        **      {
        **          FixHTMLNameSpace(lexer, root, W3C_Version[i].profile);
        **          break;
        **      }
        **  }
        **  
        **  return yes;
        */
    }

    if (!doctype)
    {
        if ( !(doctype = NewXhtmlDocTypeNode( root )) )
            return no;
    }

    lexer->txtstart = lexer->txtend = lexer->lexsize;

   /* use the appropriate public identifier */
    AddStringLiteral(lexer, "html PUBLIC ");

    if (doctype_mode == doctype_user && doctype_str)
    { 
       AddStringLiteral(lexer, "\""); /* #431889 - fix by Dave Bryan 04 Jan 2001 */
       AddStringLiteral(lexer, doctype_str); 
       AddStringLiteral(lexer, "\""); /* #431889 - fix by Dave Bryan 04 Jan 2001 */
    }
    else if (guessed == VERS_HTML20)
        AddStringLiteral(lexer, "\"-//IETF//DTD HTML 2.0//EN\"");
    else
    {
        AddStringLiteral(lexer, "\"-//W3C//DTD ");

        for (i = 0; i < W3C_VERSIONS; ++i)
        {
            if (guessed == W3C_Version[i].code)
            {
                AddStringLiteral(lexer, W3C_Version[i].name);
                break;
            }
        }

        AddStringLiteral(lexer, "//EN\"");
    }

    lexer->txtend = lexer->lexsize;

    doctype->start = lexer->txtstart;
    doctype->end = lexer->txtend;

    return yes;
}

/* ensure XML document starts with <?XML version="1.0"?> */
/* add encoding attribute if not using ASCII or UTF-8 output */
Bool FixXmlDecl(Lexer *lexer, Node *root)
{
    Node *xml;
    AttVal *version, *encoding;

    if (root->content && root->content->type == XmlDecl)
    {
        xml = root->content;
    }
    else
    {
        xml = NewNode();
        xml->type = XmlDecl;
        xml->next = root->content;
        
        if (root->content)
        {
            root->content->prev = xml;
            xml->next = root->content;
        }
        
        root->content = xml;
    }

    version = GetAttrByName(xml, "version");
    encoding = GetAttrByName(xml, "encoding");

    /*
      We need to insert a check if declared encoding 
      and output encoding mismatch and fix the XML
      declaration accordingly!!!
    */

    if (encoding == null && outCharEncoding != UTF8)
    {
        if (outCharEncoding == LATIN1)
            AddAttribute(xml, "encoding", "iso-8859-1");
        else if (outCharEncoding == ISO2022)
            AddAttribute(xml, "encoding", "iso-2022");
    }

    if (version == null)
        AddAttribute(xml, "version", "1.0");

    return yes;
}

Node *InferredTag(Lexer *lexer, char *name)
{
    Node *node;

    node = NewNode();
    node->type = StartTag;
    node->implicit = yes;
    node->element = wstrdup(name);
    node->start = lexer->txtstart;
    node->end = lexer->txtend;
    FindTag(node);
    return node;
}

Bool ExpectsContent(Node *node)
{
    if (node->type != StartTag)
        return no;

    /* unknown element? */
    if (node->tag == null)
        return yes;

    if (node->tag->model & CM_EMPTY)
        return no;

    return yes;
}

/*
  create a text node for the contents of
  a CDATA element like style or script
  which ends with </foo> for some foo.
*/
Node *GetCDATA(Lexer *lexer, Node *container)
{
    int c, lastc, start, i, len;
    Bool endtag = no;

    lexer->lines = lexer->in->curline;
    lexer->columns = lexer->in->curcol;
    lexer->waswhite = no;
    lexer->txtstart = lexer->txtend = lexer->lexsize;

    lastc = '\0';
    start = -1;

    while ((c = ReadChar(lexer->in)) != EndOfStream)
    {
        /* treat \r\n as \n and \r as \n */

        if (c == '/' && lastc == '<')
        {
            if (endtag)
            {
                lexer->lines = lexer->in->curline;
                lexer->columns = lexer->in->curcol - 3;

                ReportWarning(lexer, null, null, BAD_CDATA_CONTENT);
            }

            start = lexer->lexsize + 1;  /* to first letter */
            endtag = yes;
        }
        else if (c == '>' && start >= 0)
        {
            if (((len = lexer->lexsize - start) == wstrlen(container->element)) &&
                wstrncasecmp(lexer->lexbuf+start, container->element, len) == 0)
            {
                lexer->txtend = start - 2;
                lexer->lexsize = start - 2; /* #433857 - fix by Huajun Zeng 26 Apr 01 */
                break;
            }

            lexer->lines = lexer->in->curline;
            lexer->columns = lexer->in->curcol - 3;

            ReportWarning(lexer, null, null, BAD_CDATA_CONTENT);

            /* if javascript insert backslash before / */

            if (IsJavaScript(container))
            {
                for (i = lexer->lexsize; i > start-1; --i)
                    lexer->lexbuf[i] = lexer->lexbuf[i-1];

                lexer->lexbuf[start-1] = '\\';
                lexer->lexsize++;
            }

            start = -1;
        }
        /* #427844 - fix by Markus Hoenicka 21 Oct 00 */
        else if (c == '\r')
        {
            if (endtag) 
            { 
                continue; /* discard whitespace in endtag */ 
            } 
            else 
            { 
                c = ReadChar(lexer->in);

                if (c != '\n')
                    UngetChar(c, lexer->in);

                c = '\n';
            }
        } 
        else if ((c == '\n' || c == '\t' || c == ' ') && endtag) 
        { 
            continue; /* discard whitespace in endtag */ 
        }
        
        AddCharToLexer(lexer, (uint)c);
        lexer->txtend = lexer->lexsize;
        lastc = c;
    }

    if (c == EndOfStream)
        ReportWarning(lexer, container, null, MISSING_ENDTAG_FOR);

    if (lexer->txtend > lexer->txtstart)
        return lexer->token = TextToken(lexer);

    return null;
}

void UngetToken(Lexer *lexer)
{
    lexer->pushed = yes;
}

/*
  modes for GetToken()

  MixedContent   -- for elements which don't accept PCDATA
  Preformatted   -- white space preserved as is
  IgnoreMarkup   -- for CDATA elements such as script, style
*/

Node *GetToken(Lexer *lexer, uint mode)
{
    int c, lastc, badcomment = 0;
    Bool isempty, inDTDSubset = no;
    AttVal *attributes;

    if (lexer->pushed)
    {
        /* duplicate inlines in preference to pushed text nodes when appropriate */
        if (lexer->token->type != TextNode || (!lexer->insert && !lexer->inode))
        {
            lexer->pushed = no;
            return lexer->token;
        }
    }

    /* at start of block elements, unclosed inline
       elements are inserted into the token stream */
     
    if (lexer->insert || lexer->inode)
        return InsertedToken(lexer);

    lexer->lines = lexer->in->curline;
    lexer->columns = lexer->in->curcol;
    lexer->waswhite = no;

    lexer->txtstart = lexer->txtend = lexer->lexsize;

    while ((c = ReadChar(lexer->in)) != EndOfStream)
    {
        if (lexer->insertspace && !(mode & IgnoreWhitespace))
        {
            AddCharToLexer(lexer, ' ');
            lexer->waswhite = yes;
            lexer->insertspace = no;
        }

        /* treat \r\n as \n and \r as \n */

        if (c == '\r')
        {
            c = ReadChar(lexer->in);

            if (c != '\n')
                UngetChar(c, lexer->in);

            c = '\n';
        }

        AddCharToLexer(lexer, (uint)c);

        switch (lexer->state)
        {
            case LEX_CONTENT:  /* element content */

                /*
                 Discard white space if appropriate. Its cheaper
                 to do this here rather than in parser methods
                 for elements that don't have mixed content.
                */
                if (IsWhite(c) && (mode == IgnoreWhitespace) 
                      && lexer->lexsize == lexer->txtstart + 1)
                {
                    --(lexer->lexsize);
                    lexer->waswhite = no;
                    lexer->lines = lexer->in->curline;
                    lexer->columns = lexer->in->curcol;
                    continue;
                }

                if (c == '<')
                {
                    lexer->state = LEX_GT;
                    continue;
                }

                if (IsWhite(c))
                {
                    /* was previous char white? */
                    if (lexer->waswhite)
                    {
                        if (mode != Preformatted && mode != IgnoreMarkup)
                        {
                            --(lexer->lexsize);
                            lexer->lines = lexer->in->curline;
                            lexer->columns = lexer->in->curcol;
                        }
                    }
                    else /* prev char wasn't white */
                    {
                        lexer->waswhite = yes;
                        lastc = c;

                        if (mode != Preformatted && mode != IgnoreMarkup && c != ' ')
                            ChangeChar(lexer, ' ');
                    }

                    continue;
                }
                else if (c == '&' && mode != IgnoreMarkup)
                    ParseEntity(lexer, mode);

                /* this is needed to avoid trimming trailing whitespace */
                if (mode == IgnoreWhitespace)
                    mode = MixedContent;

                lexer->waswhite = no;
                continue;

            case LEX_GT:  /* < */

                /* check for endtag */
                if (c == '/')
                {
                    if ((c = ReadChar(lexer->in)) == EndOfStream)
                    {
                        UngetChar(c, lexer->in);
                        continue;
                    }

                    AddCharToLexer(lexer, c);

                    if (IsLetter(c))
                    {
                        lexer->lexsize -= 3;
                        lexer->txtend = lexer->lexsize;
                        UngetChar(c, lexer->in);
                        lexer->state = LEX_ENDTAG;
                        lexer->lexbuf[lexer->lexsize] = '\0';  /* debug */
                        lexer->in->curcol -= 2;

                        /* if some text before the </ return it now */
                        if (lexer->txtend > lexer->txtstart)
                        {
                            /* trim space char before end tag */
                            if (mode == IgnoreWhitespace && lexer->lexbuf[lexer->lexsize - 1] == ' ')
                            {
                                lexer->lexsize -= 1;
                                lexer->txtend = lexer->lexsize;
                            }

                            return lexer->token = TextToken(lexer);
                        }

                        continue;       /* no text so keep going */
                    }

                    /* otherwise treat as CDATA */
                    lexer->waswhite = no;
                    lexer->state = LEX_CONTENT;
                    continue;
                }

                if (mode == IgnoreMarkup)
                {
                    /* otherwise treat as CDATA */
                    lexer->waswhite = no;
                    lexer->state = LEX_CONTENT;
                    continue;
                }

                /*
                   look out for comments, doctype or marked sections
                   this isn't quite right, but its getting there ...
                */
                if (c == '!')
                {
                    c = ReadChar(lexer->in);

                    if (c == '-')
                    {
                        c = ReadChar(lexer->in);

                        if (c == '-')
                        {
                            lexer->state = LEX_COMMENT;  /* comment */
                            lexer->lexsize -= 2;
                            lexer->txtend = lexer->lexsize;

                            /* if some text before < return it now */
                            if (lexer->txtend > lexer->txtstart)
                                return lexer->token = TextToken(lexer);

                            lexer->txtstart = lexer->lexsize;
                            continue;
                        }

                        ReportWarning(lexer, null, null, MALFORMED_COMMENT);
                    }
                    else if (c == 'd' || c == 'D')
                    {
                        lexer->state = LEX_DOCTYPE; /* doctype */
                        lexer->lexsize -= 2;
                        lexer->txtend = lexer->lexsize;
                        mode = IgnoreWhitespace;

                        /* skip until white space or '>' */

                        for (;;)
                        {
                            c = ReadChar(lexer->in);

                            if (c == EndOfStream || c == '>')
                            {
                                UngetChar(c, lexer->in);
                                break;
                            }


                            if (!IsWhite(c))
                                continue;

                            /* and skip to end of whitespace */

                            for (;;)
                            {
                                c = ReadChar(lexer->in);

                                if (c == EndOfStream || c == '>')
                                {
                                    UngetChar(c, lexer->in);
                                    break;
                                }


                                if (IsWhite(c))
                                    continue;

                                UngetChar(c, lexer->in);
                                    break;
                            }

                            break;
                        }

                        /* if some text before < return it now */
                        if (lexer->txtend > lexer->txtstart)
                            return lexer->token = TextToken(lexer);

                        lexer->txtstart = lexer->lexsize;
                        continue;
                    }
                    else if (c == '[')
                    {
                        /* Word 2000 embeds <![if ...]> ... <![endif]> sequences */
                        lexer->lexsize -= 2;
                        lexer->state = LEX_SECTION;
                        lexer->txtend = lexer->lexsize;

                        /* if some text before < return it now */
                        if (lexer->txtend > lexer->txtstart)
                            return lexer->token = TextToken(lexer);

                        lexer->txtstart = lexer->lexsize;
                        continue;
                    }



                    /* otherwise swallow chars up to and including next '>' */
                    while ((c = ReadChar(lexer->in)) != '>')
                    {
                        if (c == EndOfStream)
                        {
                            UngetChar(c, lexer->in);
                            break;
                        }
                    }

                    lexer->lexsize -= 2;
                    lexer->lexbuf[lexer->lexsize] = '\0';
                    lexer->state = LEX_CONTENT;
                    continue;
                }

                /*
                   processing instructions
                */

                if (c == '?')
                {
                    lexer->lexsize -= 2;
                    lexer->state = LEX_PROCINSTR;
                    lexer->txtend = lexer->lexsize;

                    /* if some text before < return it now */
                    if (lexer->txtend > lexer->txtstart)
                        return lexer->token = TextToken(lexer);

                    lexer->txtstart = lexer->lexsize;
                    continue;
                }

                /* Microsoft ASP's e.g. <% ... server-code ... %> */
                if (c == '%')
                {
                    lexer->lexsize -= 2;
                    lexer->state = LEX_ASP;
                    lexer->txtend = lexer->lexsize;

                    /* if some text before < return it now */
                    if (lexer->txtend > lexer->txtstart)
                        return lexer->token = TextToken(lexer);

                    lexer->txtstart = lexer->lexsize;
                    continue;
                }

                /* Netscapes JSTE e.g. <# ... server-code ... #> */
                if (c == '#')
                {
                    lexer->lexsize -= 2;
                    lexer->state = LEX_JSTE;
                    lexer->txtend = lexer->lexsize;

                    /* if some text before < return it now */
                    if (lexer->txtend > lexer->txtstart)
                        return lexer->token = TextToken(lexer);

                    lexer->txtstart = lexer->lexsize;
                    continue;
                }

                /* check for start tag */
                if (IsLetter(c))
                {
                    UngetChar(c, lexer->in);     /* push back letter */
                    lexer->lexsize -= 2;      /* discard "<" + letter */
                    lexer->txtend = lexer->lexsize;
                    lexer->state = LEX_STARTTAG;         /* ready to read tag name */

                    /* if some text before < return it now */
                    if (lexer->txtend > lexer->txtstart)
                        return lexer->token = TextToken(lexer);

                    continue;       /* no text so keep going */
                }

                /* otherwise treat as CDATA */
                lexer->state = LEX_CONTENT;
                lexer->waswhite = no;
                continue;

            case LEX_ENDTAG:  /* </letter */
                lexer->txtstart = lexer->lexsize - 1;
                lexer->in->curcol += 2;
                c = ParseTagName(lexer);
                lexer->token = TagToken(lexer, EndTag);  /* create endtag token */
                lexer->lexsize = lexer->txtend = lexer->txtstart;

                /* skip to '>' */
                while (c != '>')
                {
                    c = ReadChar(lexer->in);

                    if (c == EndOfStream)
                        break;
                }

                if (c == EndOfStream)
                {
                    UngetChar(c, lexer->in);
                    continue;
                }

                lexer->state = LEX_CONTENT;
                lexer->waswhite = no;
                return lexer->token;  /* the endtag token */

            case LEX_STARTTAG: /* first letter of tagname */
                lexer->txtstart = lexer->lexsize - 1; /* set txtstart to first letter */
                c = ParseTagName(lexer);
                isempty = no;
                attributes = null;
                lexer->token = TagToken(lexer, (isempty ? StartEndTag : StartTag));

                /* parse attributes, consuming closing ">" */
                if (c != '>')
                {
                    if (c == '/')
                        UngetChar(c, lexer->in);

                    attributes = ParseAttrs(lexer, &isempty);
                }

                if (isempty)
                    lexer->token->type = StartEndTag;

                lexer->token->attributes = attributes;
                lexer->lexsize = lexer->txtend = lexer->txtstart;


                /* swallow newline following start tag */
                /* special check needed for CRLF sequence */
                /* this doesn't apply to empty elements */
                /* nor to preformatted content that needs escaping */

                if ((mode != Preformatted || PreContent(lexer->token))
                    && (ExpectsContent(lexer->token) ||
                               lexer->token->tag == tag_br))
                {
                    c = ReadChar(lexer->in);

                    if (c == '\r')
                    {
                        c = ReadChar(lexer->in);

                        if (c != '\n')
                            UngetChar(c, lexer->in);
                    }
                    else if (c != '\n' && c != '\f')
                        UngetChar(c, lexer->in);

                    lexer->waswhite = yes;  /* to swallow leading whitespace */
                }
                else
                    lexer->waswhite = no;

                lexer->state = LEX_CONTENT;
                if (lexer->token->tag == null)
                    ReportError(lexer, null, lexer->token, UNKNOWN_ELEMENT);
                else if (!XmlTags)
                {
                    ConstrainVersion(lexer, lexer->token->tag->versions);
                    
                    if (lexer->token->tag->versions & VERS_PROPRIETARY)
                    {
                        /* #427810 - fix by Gary Deschaines 24 May 00 */
                        if (MakeClean && (lexer->token->tag != tag_nobr &&
                                                lexer->token->tag != tag_wbr))
                            ReportWarning(lexer, null, lexer->token, PROPRIETARY_ELEMENT);
                        else
                        /* #427810 - fix by Terry Teague 2 Jul 01 */
                        if (!MakeClean)
                        /* if (!MakeClean && (lexer->token->tag == tag_nobr ||
                                                lexer->token->tag == tag_wbr)) */
                            ReportWarning(lexer, null, lexer->token, PROPRIETARY_ELEMENT);
                    }

                    if (lexer->token->tag->chkattrs)
                        lexer->token->tag->chkattrs(lexer, lexer->token);
                    else
                        CheckAttributes(lexer, lexer->token);

                    /* should this be called before attribute checks? */
                    RepairDuplicateAttributes(lexer, lexer->token);
                }

                 return lexer->token;  /* return start tag */

            case LEX_COMMENT:  /* seen <!-- so look for --> */

                if (c != '-')
                    continue;

                c = ReadChar(lexer->in);
                AddCharToLexer(lexer, c);

                if (c != '-')
                    continue;

            end_comment:
                c = ReadChar(lexer->in);

                if (c == '>')
                {
                    if (badcomment)
                        ReportWarning(lexer, null, null, MALFORMED_COMMENT);

                    lexer->txtend = lexer->lexsize;
                    lexer->lexbuf[lexer->lexsize] = '\0';
                    lexer->state = LEX_CONTENT;
                    lexer->waswhite = no;
                    lexer->token = CommentToken(lexer);

                    /* now look for a line break */

                    c = ReadChar(lexer->in);

                    if (c == '\r')
                    {
                        c = ReadChar(lexer->in);

                        if (c != '\n')
                            lexer->token->linebreak = yes;
                    }

                    if (c == '\n')
                        lexer->token->linebreak = yes;
                    else
                        UngetChar(c, lexer->in);

                    return lexer->token;
                }

                /* note position of first such error in the comment */
                if (!badcomment)
                {
                    lexer->lines = lexer->in->curline;
                    lexer->columns = lexer->in->curcol - 3;
                }

                badcomment++;

                if (FixComments)
                    lexer->lexbuf[lexer->lexsize - 2] = '=';

                AddCharToLexer(lexer, c);

                /* if '-' then look for '>' to end the comment */
                if (c == '-')
                    goto end_comment;

                /* otherwise continue to look for --> */
                lexer->lexbuf[lexer->lexsize - 2] = '=';
                continue; 

            case LEX_DOCTYPE:  /* seen <!d so look for '>' munging whitespace */

                if (IsWhite(c))
                {
                    if (lexer->waswhite)
                        lexer->lexsize -= 1;

                    lexer->waswhite = yes;
                }
                else
                    lexer->waswhite = no;

                if (inDTDSubset) {
                    if (c == ']')
                        inDTDSubset = no;
                }
                else if (c == '[')
                    inDTDSubset = yes;

                if (inDTDSubset || c != '>')
                    continue;

                lexer->lexsize -= 1;
                lexer->txtend = lexer->lexsize;
                lexer->lexbuf[lexer->lexsize] = '\0';
                lexer->state = LEX_CONTENT;
                lexer->waswhite = no;
                lexer->token = DocTypeToken(lexer);
                /* make a note of the version named by the doctype */
                lexer->doctype = FindGivenVersion(lexer, lexer->token);
                return lexer->token;

            case LEX_PROCINSTR:  /* seen <? so look for '>' */
                /* check for PHP preprocessor instructions <?php ... ?> */

                if  (lexer->lexsize - lexer->txtstart == 3)
                {
                    if (wstrncmp(lexer->lexbuf + lexer->txtstart, "php", 3) == 0)
                    {
                        lexer->state = LEX_PHP;
                        continue;
                    }
                }

                if  (lexer->lexsize - lexer->txtstart == 3)
                {
                    if (wstrncmp(lexer->lexbuf + lexer->txtstart, "xml", 3) == 0)
                    {
                        lexer->state = LEX_XMLDECL;
                        attributes = null;
                        continue;
                    }
                }

                if (XmlPIs)  /* insist on ?> as terminator */
                {
                    if (c != '?')
                        continue;

                    /* now look for '>' */
                    c = ReadChar(lexer->in);

                    if (c == EndOfStream)
                    {
                        ReportWarning(lexer, null, null, UNEXPECTED_END_OF_FILE);
                        UngetChar(c, lexer->in);
                        continue;
                    }

                    AddCharToLexer(lexer, c);
                }


                if (c != '>')
                    continue;

                lexer->lexsize -= 1;
                lexer->txtend = lexer->lexsize;
                lexer->lexbuf[lexer->lexsize] = '\0';
                lexer->state = LEX_CONTENT;
                lexer->waswhite = no;
                return lexer->token = PIToken(lexer);

            case LEX_ASP:  /* seen <% so look for "%>" */
                if (c != '%')
                    continue;

                /* now look for '>' */
                c = ReadChar(lexer->in);


                if (c != '>')
                {
                    UngetChar(c, lexer->in);
                    continue;
                }

                lexer->lexsize -= 1;
                lexer->txtend = lexer->lexsize;
                lexer->lexbuf[lexer->lexsize] = '\0';
                lexer->state = LEX_CONTENT;
                lexer->waswhite = no;
                return lexer->token = AspToken(lexer);

            case LEX_JSTE:  /* seen <# so look for "#>" */
                if (c != '#')
                    continue;

                /* now look for '>' */
                c = ReadChar(lexer->in);


                if (c != '>')
                {
                    UngetChar(c, lexer->in);
                    continue;
                }

                lexer->lexsize -= 1;
                lexer->txtend = lexer->lexsize;
                lexer->lexbuf[lexer->lexsize] = '\0';
                lexer->state = LEX_CONTENT;
                lexer->waswhite = no;
                return lexer->token = JsteToken(lexer);

            case LEX_PHP: /* seen "<?php" so look for "?>" */
                if (c != '?')
                    continue;

                /* now look for '>' */
                c = ReadChar(lexer->in);

                if (c != '>')
                {
                    UngetChar(c, lexer->in);
                    continue;
                }

                lexer->lexsize -= 1;
                lexer->txtend = lexer->lexsize;
                lexer->lexbuf[lexer->lexsize] = '\0';
                lexer->state = LEX_CONTENT;
                lexer->waswhite = no;
                return lexer->token = PhpToken(lexer);

            case LEX_XMLDECL: /* seen "<?xml" so look for "?>" */

                if (IsWhite(c) && c != '?')
                    continue;

                /* get pseudo-attribute */
                if (c != '?')
                {
                    char *name;
                    Node *asp, *php;
                    AttVal *av = NewAttribute();
                    int pdelim;
                    isempty = no;

                    UngetChar(c, lexer->in);

                    name = ParseAttribute(lexer, &isempty, &asp, &php);

                    av->attribute = name;
                    av->value = ParseValue(lexer, name, yes, &isempty, &pdelim);
                    av->delim = pdelim;
                    av->next = attributes;

                    attributes = av;
                    /* continue; */
                }

                /* now look for '>' */
                c = ReadChar(lexer->in);

                if (c != '>')
                {
                    UngetChar(c, lexer->in);
                    continue;
                }
                lexer->lexsize -= 1;
                lexer->txtend = lexer->txtstart;
                lexer->lexbuf[lexer->txtend] = '\0';
                lexer->state = LEX_CONTENT;
                lexer->waswhite = no;
                lexer->token = XmlDeclToken(lexer);
                lexer->token->attributes = attributes;
                return lexer->token;

            case LEX_SECTION: /* seen "<![" so look for "]>" */
                if (c == '[')
                {
                    if (lexer->lexsize == (lexer->txtstart + 6) &&
                        wstrncmp(lexer->lexbuf+lexer->txtstart, "CDATA[", 6) == 0)
                    {
                        lexer->state = LEX_CDATA;
                        lexer->lexsize -= 6;
                        continue;
                    }
                }

                if (c != ']')
                    continue;

                /* now look for '>' */
                c = ReadChar(lexer->in);

                if (c != '>')
                {
                    UngetChar(c, lexer->in);
                    continue;
                }

                lexer->lexsize -= 1;
                lexer->txtend = lexer->lexsize;
                lexer->lexbuf[lexer->lexsize] = '\0';
                lexer->state = LEX_CONTENT;
                lexer->waswhite = no;
                return lexer->token = SectionToken(lexer);

            case LEX_CDATA: /* seen "<![CDATA[" so look for "]]>" */
                if (c != ']')
                    continue;

                /* now look for ']' */
                c = ReadChar(lexer->in);

                if (c != ']')
                {
                    UngetChar(c, lexer->in);
                    continue;
                }

                /* now look for '>' */
                c = ReadChar(lexer->in);

                if (c != '>')
                {
                    UngetChar(c, lexer->in);
                    continue;
                }

                lexer->lexsize -= 1;
                lexer->txtend = lexer->lexsize;
                lexer->lexbuf[lexer->lexsize] = '\0';
                lexer->state = LEX_CONTENT;
                lexer->waswhite = no;
                return lexer->token = CDATAToken(lexer);
        }
    }

    if (lexer->state == LEX_CONTENT)  /* text string */
    {
        lexer->txtend = lexer->lexsize;

        if (lexer->txtend > lexer->txtstart)
        {
            UngetChar(c, lexer->in);

            if (lexer->lexbuf[lexer->lexsize - 1] == ' ')
            {
                lexer->lexsize -= 1;
                lexer->txtend = lexer->lexsize;
            }

            return lexer->token = TextToken(lexer);
        }
    }
    else if (lexer->state == LEX_COMMENT) /* comment */
    {
        if (c == EndOfStream)
            ReportWarning(lexer, null, null, MALFORMED_COMMENT);

        lexer->txtend = lexer->lexsize;
        lexer->lexbuf[lexer->lexsize] = '\0';
        lexer->state = LEX_CONTENT;
        lexer->waswhite = no;
        return lexer->token = CommentToken(lexer);
    }

    return 0;
}

static void MapStr(char *str, uint code)
{
    uint i;

    while (*str)
    {
        i = (uint)(*str++);
        lexmap[i] |= code;
    }
}

void InitMap(void)
{
    MapStr("\r\n\f", newline|white);
    MapStr(" \t", white);
    MapStr("-.:_", namechar);
    MapStr("0123456789", digit|namechar);
    MapStr("abcdefghijklmnopqrstuvwxyz", lowercase|letter|namechar);
    MapStr("ABCDEFGHIJKLMNOPQRSTUVWXYZ", uppercase|letter|namechar);
}

/*
 parser for ASP within start tags

 Some people use ASP for to customize attributes
 Tidy isn't really well suited to dealing with ASP
 This is a workaround for attributes, but won't
 deal with the case where the ASP is used to tailor
 the attribute value. Here is an example of a work
 around for using ASP in attribute values:

  href='<%=rsSchool.Fields("ID").Value%>'

 where the ASP that generates the attribute value
 is masked from Tidy by the quotemarks.

*/

static Node *ParseAsp(Lexer *lexer)
{
    uint c;
    Node *asp = null;

    lexer->txtstart = lexer->lexsize;

    for (;;)
    {
        if ((c = ReadChar(lexer->in)) == EndOfStream)
            break;

        AddCharToLexer(lexer, c);


        if (c != '%')
            continue;

        if ((c = ReadChar(lexer->in)) == EndOfStream)
            break;

        AddCharToLexer(lexer, c);

        if (c == '>')
            break;
    }

    lexer->lexsize -= 2;
    lexer->txtend = lexer->lexsize;

    if (lexer->txtend > lexer->txtstart)
        asp = AspToken(lexer);

    lexer->txtstart = lexer->txtend;
    return asp;
}   
 

/*
 PHP is like ASP but is based upon XML
 processing instructions, e.g. <?php ... ?>
*/
static Node *ParsePhp(Lexer *lexer)
{
    uint c;
    Node *php = null;

    lexer->txtstart = lexer->lexsize;

    for (;;)
    {
        if ((c = ReadChar(lexer->in)) == EndOfStream)
            break;

        AddCharToLexer(lexer, c);


        if (c != '?')
            continue;

        if ((c = ReadChar(lexer->in)) == EndOfStream)
            break;

        AddCharToLexer(lexer, c);

        if (c == '>')
            break;
    }

    lexer->lexsize -= 2;
    lexer->txtend = lexer->lexsize;

    if (lexer->txtend > lexer->txtstart)
        php = PhpToken(lexer);

    lexer->txtstart = lexer->txtend;
    return php;
}   

/* consumes the '>' terminating start tags */
static char  *ParseAttribute(Lexer *lexer, Bool *isempty,
                             Node **asp, Node **php)
{
    int start, len = 0;
    char *attr;
    uint c, lastc;

    *asp = null;  /* clear asp pointer */
    *php = null;  /* clear php pointer */

 /* skip white space before the attribute */

    for (;;)
    {
        c = ReadChar(lexer->in);


        if (c == '/')
        {
            c = ReadChar(lexer->in);

            if (c == '>')
            {
                *isempty = yes;
                return null;
            }

            UngetChar(c, lexer->in);
            c = '/';
            break;
        }

        if (c == '>')
            return null;

        if (c =='<')
        {
            c = ReadChar(lexer->in);

            if (c == '%')
            {
                *asp = ParseAsp(lexer);
                return null;
            }
            else if (c == '?')
            {
                *php = ParsePhp(lexer);
                return null;
            }

            UngetChar(c, lexer->in);
            UngetChar('<', lexer->in);
            ReportAttrError(lexer, lexer->token, null, UNEXPECTED_GT);
            return null;
        }

        if (c == '=')
        {
            ReportAttrError(lexer, lexer->token, null, UNEXPECTED_EQUALSIGN);
            continue;
        }

        if (c == '"' || c == '\'')
        {
            ReportAttrError(lexer, lexer->token, null, UNEXPECTED_QUOTEMARK);
            continue;
        }

        if (c == EndOfStream)
        {
            ReportAttrError(lexer, lexer->token, null, UNEXPECTED_END_OF_FILE);
            UngetChar(c, lexer->in);
            return null;
        }


        if (!IsWhite(c))
           break;
    }

    start = lexer->lexsize;
    lastc = c;

    for (;;)
    {
     /* but push back '=' for parseValue() */
        if (c == '=' || c == '>')
        {
            UngetChar(c, lexer->in);
            break;
        }

        if (c == '<' || c == EndOfStream)
        {
            UngetChar(c, lexer->in);
            break;
        }

        if (lastc == '-' && (c == '"' || c == '\''))
        {
            lexer->lexsize--;
            --len;
            UngetChar(c, lexer->in);
            break;
        }

        if (IsWhite(c))
            break;

     /* what should be done about non-namechar characters? */
     /* currently these are incorporated into the attr name */

        if (!XmlTags && IsUpper(c))
            c = ToLower(c);

        /* ++len; */ /* #427672 - handle attribute names with multibyte chars - fix by Randy Waki - 10 Aug 00 */
        AddCharToLexer(lexer, c);

        lastc = c;
        c = ReadChar(lexer->in);
    }

    len = lexer->lexsize - start; /* #427672 - handle attribute names with multibyte chars - fix by Randy Waki - 10 Aug 00 */
    attr = (len > 0 ? wstrndup(lexer->lexbuf+start, len) : null);
    lexer->lexsize = start;

    return attr;
}

/*
 invoked when < is seen in place of attribute value
 but terminates on whitespace if not ASP, PHP or Tango
 this routine recognizes ' and " quoted strings
*/
static int ParseServerInstruction(Lexer *lexer)
{
    int c, delim = '"';
    Bool isrule = no;

    c = ReadChar(lexer->in);
    AddCharToLexer(lexer, c);

    /* check for ASP, PHP or Tango */
    if (c == '%' || c == '?' || c == '@')
        isrule = yes;

    for (;;)
    {
        c = ReadChar(lexer->in);

        if (c == EndOfStream)
            break;

        if (c == '>')
        {
            if (isrule)
                AddCharToLexer(lexer, c);
            else
                UngetChar(c, lexer->in);

            break;
        }

        /* if not recognized as ASP, PHP or Tango */
        /* then also finish value on whitespace */
        if (!isrule)
        {
            if (IsWhite(c))
                break;
        }

        AddCharToLexer(lexer, c);

        if (c == '"')
        {
            do
            {
                c = ReadChar(lexer->in);
                if (c == EndOfStream) /* #427840 - fix by Terry Teague 30 Jun 01 */
                {
                    ReportAttrError(lexer, lexer->token, null, UNEXPECTED_END_OF_FILE);
                    UngetChar(c, lexer->in);
                    return null;
                }
                if (c == '>') /* #427840 - fix by Terry Teague 30 Jun 01 */
                {
                    UngetChar(c, lexer->in);
                    ReportAttrError(lexer, lexer->token, null, UNEXPECTED_GT);
                    return null;
                }
                AddCharToLexer(lexer, c);
            }
            while (c != '"');
            delim = '\'';
            continue;
        }

        if (c == '\'')
        {
            do
            {
                c = ReadChar(lexer->in);
                if (c == EndOfStream) /* #427840 - fix by Terry Teague 30 Jun 01 */
                {
                    ReportAttrError(lexer, lexer->token, null, UNEXPECTED_END_OF_FILE);
                    UngetChar(c, lexer->in);
                    return null;
                }
                if (c == '>') /* #427840 - fix by Terry Teague 30 Jun 01 */
                {
                    UngetChar(c, lexer->in);
                    ReportAttrError(lexer, lexer->token, null, UNEXPECTED_GT);
                    return null;
                }
                AddCharToLexer(lexer, c);
            }
            while (c != '\'');
        }
    }

    return delim;
}

/* values start with "=" or " = " etc. */
/* doesn't consume the ">" at end of start tag */

static char *ParseValue(Lexer *lexer, char *name,
                        Bool foldCase, Bool *isempty, int *pdelim)
{
    int len = 0, start;
    Bool seen_gt = no;
    Bool munge = yes;
    uint c, lastc, delim, quotewarning;
    char *value;

    delim = (char) 0;
    *pdelim = '"';

    /*
     Henry Zrepa reports that some folk are using the
     embed element with script attributes where newlines
     are significant and must be preserved
    */
    if (LiteralAttribs)
        munge = no;

 /* skip white space before the '=' */

    for (;;)
    {
        c = ReadChar(lexer->in);

        if (c == EndOfStream)
        {
            UngetChar(c, lexer->in);
            break;
        }

        if (!IsWhite(c))
           break;
    }

/*
  c should be '=' if there is a value
  other legal possibilities are white
  space, '/' and '>'
*/

    if (c != '=' && c != '"' && c != '\'')
    {
        UngetChar(c, lexer->in);
        return null;
    }

 /* skip white space after '=' */

    for (;;)
    {
        c = ReadChar(lexer->in);

        if (c == EndOfStream)
        {
            UngetChar(c, lexer->in);
            break;
        }

        if (!IsWhite(c))
           break;
    }

 /* check for quote marks */

    if (c == '"' || c == '\'')
        delim = c;
    else if (c == '<')
    {
        start = lexer->lexsize;
        AddCharToLexer(lexer, c);
        *pdelim = ParseServerInstruction(lexer);
        len = lexer->lexsize - start;
        lexer->lexsize = start;
        return (len > 0 ? wstrndup(lexer->lexbuf+start, len) : null);
    }
    else
        UngetChar(c, lexer->in);

 /*
   and read the value string
   check for quote mark if needed
 */

    quotewarning = 0;
    start = lexer->lexsize;
    c = '\0';

    for (;;)
    {
        lastc = c;  /* track last character */
        c = ReadChar(lexer->in);

        if (c == EndOfStream)
        {
            ReportAttrError(lexer, lexer->token, null, UNEXPECTED_END_OF_FILE);
            UngetChar(c, lexer->in);
            break;
        }

        if (delim == (char)0)
        {
            if (c == '>')
            {
                UngetChar(c, lexer->in);
                break;
            }

            if (c == '"' || c == '\'')
            {
                ReportAttrError(lexer, lexer->token, null, UNEXPECTED_QUOTEMARK);
                break;
            }

            if (c == '<')
            {
                UngetChar(c, lexer->in);
                c = '>';
                UngetChar(c, lexer->in);
                ReportAttrError(lexer, lexer->token, null, UNEXPECTED_GT);
                break;
            }

            /*
             For cases like <br clear=all/> need to avoid treating /> as
             part of the attribute value, however care is needed to avoid
             so treating <a href=http://www.acme.com/> in this way, which
             would map the <a> tag to <a href="http://www.acme.com"/>
            */
            if (c == '/')
            {
                /* peek ahead in case of /> */
                c = ReadChar(lexer->in);

                if (c == '>' && !IsUrl(name))
                {
                    *isempty = yes;
                    UngetChar(c, lexer->in);
                    break;
                }

                /* unget peeked char */
                UngetChar(c, lexer->in);
                c = '/';
            }
        }
        else  /* delim is '\'' or '"' */
        {
            if (c == delim)
                break;

            /* treat CRLF, CR and LF as single line break */

            if (c == '\r')
            {
                if ((c = ReadChar(lexer->in)) != '\n')
                    UngetChar(c, lexer->in);

                c = '\n';
            }

            if (c == '\n' || c == '<' || c == '>')
                ++quotewarning;

            if (c == '>')
                seen_gt = yes;
        }

        if (c == '&')
        {
            /* no entities in ID attributes */
            if (wstrcasecmp(name, "id") == 0)
            {
                ReportAttrError(lexer, null, null, ENTITY_IN_ID);
                continue;
            }
            else
            {
                AddCharToLexer(lexer, c);
                ParseEntity(lexer, null);
                continue;
            }
        }

        /*
         kludge for JavaScript attribute values
         with line continuations in string literals
        */
        if (c == '\\')
        {
            c = ReadChar(lexer->in);

            if (c != '\n')
            {
                UngetChar(c, lexer->in);
                c = '\\';
            }
        }

        if (IsWhite(c))
        {
            if (delim == (char)0)
                break;

            if (munge)
            {
                /* discard line breaks in quoted URLs */ 
                /* #438650 - fix by Randy Waki */
                if (c == '\n' && IsUrl(name)) 
                {
                    /* warn that we discard this newline */
                    ReportAttrError(lexer, lexer->token, null, NEWLINE_IN_URI);
                    continue;
                }
                
                c = ' ';

                if (lastc == ' ')
                    continue;
            }
        }
        else if (foldCase && IsUpper(c))
            c = ToLower(c);

        AddCharToLexer(lexer, c);
    }

    if (quotewarning > 10 && seen_gt && munge)
    {
        /*
           there is almost certainly a missing trailing quote mark
           as we have see too many newlines, < or > characters.

           an exception is made for Javascript attributes and the
           javascript URL scheme which may legitimately include < and >,
           and for attributes starting with "<xml " as generated by
           Microsoft Office.
        */
        if (!IsScript(name) &&
            !(IsUrl(name) && wstrncmp(lexer->lexbuf+start, "javascript:", 11) == 0) &&
            !(wstrncmp(lexer->lexbuf+start, "<xml ", 5) == 0)) /* #500236 - fix by Klaus Johannes Rusch 06 Jan 02 */
                ReportError(lexer, null, null, SUSPECTED_MISSING_QUOTE); 
    }

    len = lexer->lexsize - start;
    lexer->lexsize = start;


    if (len > 0 || delim)
        value = wstrndup(lexer->lexbuf+start, len);
    else
        value = null;

    /* note delimiter if given */
    *pdelim = (delim ? delim : '"');

    return value;
}

/* attr must be non-null */
Bool IsValidAttrName( char *attr)
{
    uint c;
    int i;

    /* first character should be a letter */
    c = attr[0];

    if (!IsLetter(c))
        return no;

    /* remaining characters should be namechars */
    for( i = 1; i < wstrlen(attr); i++)
    {
        c = attr[i];

        if (IsNamechar(c))
            continue;

        return no;
    }

    return yes;
}


/* create a new attribute */
AttVal *NewAttribute()
{
    AttVal *av;

    av = (AttVal *)MemAlloc(sizeof(AttVal));
    av->next = null; 
    av->delim = '\0';
    av->asp = null;
    av->php = null;
    av->attribute = null;
    av->value = null;
    av->dict = null;
    return av;
}

/* create a new attribute with given name and value */
AttVal *NewAttributeEx(char *name, char *value)
{
    AttVal *av = NewAttribute();

    av->attribute = wstrdup(name);
    av->value = wstrdup(value);

    return av;
}


/* swallows closing '>' */

AttVal *ParseAttrs(Lexer *lexer, Bool *isempty)
{
    AttVal *av, *list;
    char *attribute, *value;
    int delim;
    Node *asp, *php;

    list = null;

    for (; !EndOfInput(lexer);)
    {
        attribute = ParseAttribute(lexer, isempty, &asp, &php);

        if (attribute == null)
        {
            /* check if attributes are created by ASP markup */
            if (asp)
            {
                av = NewAttribute();
                av->next = list; 
                av->asp = asp;
                list = av;
                continue;
            }

            /* check if attributes are created by PHP markup */
            if (php)
            {
                av = NewAttribute();
                av->next = list; 
                av->php = php;
                list = av;
                continue;
            }

            break;
        }

        value = ParseValue(lexer, attribute, no, isempty, &delim);

        if (attribute && IsValidAttrName(attribute))
        {
            av = NewAttribute();
            av->next = list; 
            av->delim = delim;
            av->attribute = attribute;
            av->value = value;
            av->dict = FindAttribute(av);
            list = av;
        }
        else
        {
            av = NewAttribute();
            av->attribute = attribute;
            av->value = value;
            /* #427664 - fix by Gary Peskin 04 Aug 00; other fixes by Dave Raggett */
            /*
            if (value == null)
                ReportAttrError(lexer, lexer->token, av, MISSING_ATTR_VALUE);
            else
                ReportAttrError(lexer, lexer->token, av, BAD_ATTRIBUTE_VALUE);
            */
            if (value != null)
                ReportAttrError(lexer, lexer->token, av, BAD_ATTRIBUTE_VALUE);
            else if (LastChar(attribute) == '"')
                ReportAttrError(lexer, lexer->token, av, MISSING_QUOTEMARK);
            else
                ReportAttrError(lexer, lexer->token, av, UNKNOWN_ATTRIBUTE);

            FreeAttribute(av);
        }
    }

    return list;
}
