/*
  tidy.c - HTML parser and pretty printer

  Copyright (c) 1998-2001 World Wide Web Consortium
  (Massachusetts Institute of Technology, Institut National de
  Recherche en Informatique et en Automatique, Keio University).
  All Rights Reserved.

  CVS Info :

    $Author$ 
    $Date$ 
    $Revision$ 

  Contributing Author(s):

     Dave Raggett <dsr@w3.org>

  The contributing author(s) would like to thank all those who
  helped with testing, bug fixes and suggestions for improvements. 
  This wouldn't have been possible without your help.

  COPYRIGHT NOTICE:
 
  This software and documentation is provided "as is," and
  the copyright holders and contributing author(s) make no
  representations or warranties, express or implied, including
  but not limited to, warranties of merchantability or fitness
  for any particular purpose or that the use of the software or
  documentation will not infringe any third party patents,
  copyrights, trademarks or other rights. 

  The copyright holders and contributing author(s) will not be held
  liable for any direct, indirect, special or consequential damages
  arising out of any use of the software or documentation, even if
  advised of the possibility of such damage.

  Permission is hereby granted to use, copy, modify, and distribute
  this source code, or portions hereof, documentation and executables,
  for any purpose, without fee, subject to the following restrictions:

  1. The origin of this source code must not be misrepresented.
  2. Altered versions must be plainly marked as such and must
     not be misrepresented as being the original source.
  3. This Copyright notice may not be removed or altered from any
     source or altered source distribution.
 
  The copyright holders and contributing author(s) specifically
  permit, without fee, and encourage the use of this source code
  as a component for supporting the Hypertext Markup Language in
  commercial products. If you use this source code in a product,
  acknowledgment is not required but would be appreciated.
*/
#include "platform.h"
#include "html.h"

void InitTidy(void);
void DeInitTidy(void);

extern char *release_date;

Bool        debug_flag = no;
Node       *debug_element = null;
Lexer      *debug_lexer = null;
uint       totalerrors = 0;
uint       totalwarnings = 0;
uint       optionerrors = 0;

FILE *errout;  /* set to stderr or stdout */
FILE *input;

#define UNICODE_BOM_BE   0xFEFF   /* this is the big-endian (default) UNICODE BOM */
#define UNICODE_BOM      UNICODE_BOM_BE
#define UNICODE_BOM_LE   0xFFFE   /* this is the little-endian UNICODE BOM */
#define UNICODE_BOM_UTF8 0xEFBBBF /* this is the UTF-8 UNICODE BOM */

/*
   Private unget buffer for the raw bytes read from the input stream.
   Normally this will only be used by the UTF-8 decoder to resynchronize the
   input stream after finding an illegal UTF-8 sequences.
   But it can be used for other purposes when reading bytes in ReadCharFromStream.
*/

static unsigned char rawBytebuf[CHARBUF_SIZE];
static int rawBufpos = 0;
static Bool rawPushed = no;

/* Mapping for Windows Western character set (128-159) to Unicode */
int Win2Unicode[32] =
{
    0x20AC, 0x0000, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000,
    0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0000, 0x017E, 0x0178
};

/* Function for conversion from Windows-1252 to Unicode */
int DecodeWin1252(int c)
{
    return Win2Unicode[c - 128];
}

/*
John Love-Jensen contributed this table for mapping MacRoman
character set to Unicode
*/

int Mac2Unicode[256] = 
{
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,

    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,

    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,

    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,

    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,

    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,

    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,

    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,
    /* x7F = DEL */
    0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1,
    0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,

    0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3,
    0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC,

    0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF,
    0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8,

    0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211,
    0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8,

    0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB,
    0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153,

    0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA,
    0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,

    0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1,
    0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,
    /* xF0 = Apple Logo */
    0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC,
    0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7
};

/* Function to convert from MacRoman to Unicode */
int DecodeMacRoman(uint c)
{
    return Mac2Unicode[c];
}

/*
table to map symbol font characters to Unicode; undefined
characters are mapped to 0x0000 and characters without any
unicode equivalent are mapped to '?'. Is this appropriate?
*/

int Symbol2Unicode[] = 
{
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
    
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
    
    0x0020, 0x0021, 0x2200, 0x0023, 0x2203, 0x0025, 0x0026, 0x220D,
    0x0028, 0x0029, 0x2217, 0x002B, 0x002C, 0x2212, 0x002E, 0x002F,
    
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    
    0x2245, 0x0391, 0x0392, 0x03A7, 0x0394, 0x0395, 0x03A6, 0x0393,
    0x0397, 0x0399, 0x03D1, 0x039A, 0x039B, 0x039C, 0x039D, 0x039F,
    
    0x03A0, 0x0398, 0x03A1, 0x03A3, 0x03A4, 0x03A5, 0x03C2, 0x03A9,
    0x039E, 0x03A8, 0x0396, 0x005B, 0x2234, 0x005D, 0x22A5, 0x005F,
    
    0x00AF, 0x03B1, 0x03B2, 0x03C7, 0x03B4, 0x03B5, 0x03C6, 0x03B3,
    0x03B7, 0x03B9, 0x03D5, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BF,
    
    0x03C0, 0x03B8, 0x03C1, 0x03C3, 0x03C4, 0x03C5, 0x03D6, 0x03C9,
    0x03BE, 0x03C8, 0x03B6, 0x007B, 0x007C, 0x007D, 0x223C, 0x003F,
    
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    
    0x00A0, 0x03D2, 0x2032, 0x2264, 0x2044, 0x221E, 0x0192, 0x2663,
    0x2666, 0x2665, 0x2660, 0x2194, 0x2190, 0x2191, 0x2192, 0x2193,
    
    0x00B0, 0x00B1, 0x2033, 0x2265, 0x00D7, 0x221D, 0x2202, 0x00B7,
    0x00F7, 0x2260, 0x2261, 0x2248, 0x2026, 0x003F, 0x003F, 0x21B5,
    
    0x2135, 0x2111, 0x211C, 0x2118, 0x2297, 0x2295, 0x2205, 0x2229,
    0x222A, 0x2283, 0x2287, 0x2284, 0x2282, 0x2286, 0x2208, 0x2209,
    
    0x2220, 0x2207, 0x00AE, 0x00A9, 0x2122, 0x220F, 0x221A, 0x22C5,
    0x00AC, 0x2227, 0x2228, 0x21D4, 0x21D0, 0x21D1, 0x21D2, 0x21D3,
    
    0x25CA, 0x2329, 0x00AE, 0x00A9, 0x2122, 0x2211, 0x003F, 0x003F,
    0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F,
    
    0x20AC, 0x232A, 0x222B, 0x2320, 0x003F, 0x2321, 0x003F, 0x003F,
    0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F
};

/* Function to convert from MacRoman to Unicode */
int DecodeSymbolFont(uint c)
{
    if (c > 255)
        return c;

    /* todo: add some error message */

    return Symbol2Unicode[c];
}

void FatalError(char *msg)
{
    fprintf(stderr, "Fatal error: %s\n", msg);
    DeInitTidy();

    if (input && input != stdin)
        fclose(input);

    /* 2 signifies a serious error */
    exit(2);
}

void *MemAlloc(uint size)
{
    void *p;

    p = malloc(size);

    if (!p)
        FatalError("Out of memory!");

    return p;
}

void *MemRealloc(void *mem, uint newsize)
{
    void *p;

    if (mem == (void *)null)
        return MemAlloc(newsize);

    p = realloc(mem, newsize);

    if (!p)
        FatalError("Out of memory!");

    return p;
}

void MemFree(void *mem)
{
    if (mem != (void *)null)
        free(mem);
}

void ClearMemory(void *mem, uint size)
{
    memset(mem, 0, size);
}


/* UTF-8 encoding/decoding functions */
/* return # of bytes in UTF-8 sequence; result < 0 if illegal sequence */

/*

References :

1) ISO/IEC 10646-1 Amendment 2 - UCS Transformation Format 8 (UTF-8):
<http://anubis.dkuug.dk/JTC1/SC2/WG2/docs/n1335>

Table 4 - Mapping from UCS-4 to UTF-8

2) ISO 10646-1:2000 Annex D :
<http://www.cl.cam.ac.uk/~mgk25/ucs/ISO-10646-UTF-8.html>

3) Unicode standards:
<http://www.unicode.org/unicode/standard/standard.html>

4) Legal UTF-8 byte sequences:
<http://www.unicode.org/unicode/uni2errata/UTF-8_Corrigendum.html>

Code point			1st byte	2nd byte	3rd byte	4th byte
----------			--------	--------	--------	--------
U+0000..U+007F		00..7F
U+0080..U+07FF		C2..DF		80..BF
U+0800..U+0FFF		E0			A0..BF		80..BF
U+1000..U+FFFF		E1..EF		80..BF		80..BF
U+10000..U+3FFFF	F0			90..BF		80..BF		80..BF
U+40000..U+FFFFF	F1..F3		80..BF		80..BF		80..BF
U+100000..U+10FFFF	F4			80..8F		80..BF		80..BF

The definition of UTF-8 in Annex D of ISO/IEC 10646-1:2000 also allows for the use of
five- and six-byte sequences to encode characters that are outside the range of the Unicode
character set; those five- and six-byte sequences are illegal for the use of UTF-8 as a
transformation of Unicode characters. ISO/IEC 10646 does not allow mapping of
unpaired surrogates, nor U+FFFE and U+FFFF (but it does allow other noncharacters).

5) UTF-8 and Unicode FAQ:
<http://www.cl.cam.ac.uk/~mgk25/unicode.html>

6) Markus Kuhn's UTF-8 decoder stress test file:
<http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt>

7) UTF-8 Demo:
<http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-demo.txt>

8) UTF-8 Sampler:
<http://www.columbia.edu/kermit/utf8.html>

1010  A
1011  B
1100  C
1101  D
1110  E
1111  F

*/

#define kNumUTF8Sequences 7
#define kMaxUTF8Bytes 4

#define kLowUTF16Surrogate 0xD800
#define kHighUTF16Surrogate 0xDFFF

#define kUTF8ByteSwapNotAChar 0xFFFE
#define kUTF8NotAChar 0xFFFF

#define kMaxUTF8FromUCS4 0x10FFFF

/* offsets into validUTF8 table below */
static int offsetUTF8Sequences[kMaxUTF8Bytes + 1] =
{
    0, /* 1 byte */
    1, /* 2 bytes */
    2, /* 3 bytes */
    4, /* 4 bytes */
    kNumUTF8Sequences /* must be last */
};

static struct validUTF8Sequence
{
     unsigned int lowChar;
     unsigned int highChar;
     int numBytes;
     unsigned char validBytes[8];
} validUTF8[kNumUTF8Sequences] =
{
/*   low       high   #bytes  byte 1      byte 2      byte 3      byte 4 */
    {0x0000,   0x007F,   1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x0080,   0x07FF,   2, 0xC2, 0xDF, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00},
    {0x0800,   0x0FFF,   3, 0xE0, 0xE0, 0xA0, 0xBF, 0x80, 0xBF, 0x00, 0x00},
    {0x1000,   0xFFFF,   3, 0xE1, 0xEF, 0x80, 0xBF, 0x80, 0xBF, 0x00, 0x00},
    {0x10000,  0x3FFFF,  4, 0xF0, 0xF0, 0x90, 0xBF, 0x80, 0xBF, 0x80, 0xBF},
    {0x40000,  0xFFFFF,  4, 0xF1, 0xF3, 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF},
    {0x100000, 0x10FFFF, 4, 0xF4, 0xF4, 0x80, 0x8F, 0x80, 0xBF, 0x80, 0xBF} 
};

int DecodeUTF8BytesToChar(uint *c, uint firstByte, unsigned char *successorBytes,
                           StreamIn *in, GetBytes getter, int *count)
{
    unsigned char tempbuf[10];
    unsigned char *buf = &tempbuf[0];
    uint ch = 0, n = 0;
    int i, bytes = 0;
    Bool hasError = no;
    
    if (successorBytes)
        buf = successorBytes;
        
    /* special check if we have been passed an EOF char */
    if (/* (in && feof(in->file)) || */ firstByte == (uint)EndOfStream)
    {
        /* at present */
        *c = firstByte;
        *count = 1;
        return 0;
    }

    ch = firstByte; /* first byte is passed in separately */
    
    if (ch <= 0x7F) /* 0XXX XXXX one byte */
    {
        n = ch;
        bytes = 1;
    }
    else if ((ch & 0xE0) == 0xC0)  /* 110X XXXX  two bytes */
    {
        n = ch & 31;
        bytes = 2;
    }
    else if ((ch & 0xF0) == 0xE0)  /* 1110 XXXX  three bytes */
    {
        n = ch & 15;
        bytes = 3;
    }
    else if ((ch & 0xF8) == 0xF0)  /* 1111 0XXX  four bytes */
    {
        n = ch & 7;
        bytes = 4;
    }
    else if ((ch & 0xFC) == 0xF8)  /* 1111 10XX  five bytes */
    {
        n = ch & 3;
        bytes = 5;
        hasError = yes;
    }
    else if ((ch & 0xFE) == 0xFC)  /* 1111 110X  six bytes */
    {
        n = ch & 1;
        bytes = 6;
        hasError = yes;
    }
    else
    {
        /* not a valid first byte of a UTF-8 sequence */
        n = ch;
        bytes = 1;
        hasError = yes;
    }

    for (i = 1; i < bytes; ++i)
    {
        int tempCount; /* no. of additional bytes to get */
        
        /* successor bytes should have the form 10XX XXXX */
        if (getter && (bytes - i > 0))
        {
            tempCount = 1; /* to simplify things, get 1 byte at a time */
            getter(in, (unsigned char *)&buf[i - 1], &tempCount, no);
            if (tempCount <= 0) /* EOF */
            {
                hasError = yes;
                bytes = i;
                break;
            }
        }
        
		if ((buf[i - 1] & 0xC0) != 0x80)
		{
		    /* illegal successor byte value */
		    hasError = yes;
		    bytes = i;
            if (getter)
            {
                tempCount = 1; /* to simplify things, unget 1 byte at a time */
            	getter(in, (unsigned char *)&buf[i - 1], &tempCount, yes); /* Unget the byte */
            }
		    break;
		}
        
        n = (n << 6) | (buf[i - 1] & 0x3F);
    }
    
    if (!hasError && ((n == kUTF8ByteSwapNotAChar) || (n == kUTF8NotAChar)))
        hasError = yes;
        
    if (!hasError && (n > kMaxUTF8FromUCS4))
        hasError = yes;
        
    if (!hasError && (n >= kLowUTF16Surrogate) && (n <= kHighUTF16Surrogate))
        /* unpaired surrogates not allowed */
        hasError = yes;

    if (!hasError)
    {
        int lo, hi;
        
        lo = offsetUTF8Sequences[bytes - 1];
        hi = offsetUTF8Sequences[bytes] - 1;
        
        /* check for overlong sequences */
        if ((n < validUTF8[lo].lowChar) || (n > validUTF8[hi].highChar))
            hasError = yes;
        else
        {
            hasError = yes; /* assume error until proven otherwise */
        
            for (i = lo; i <= hi; i++)
            {
                int tempCount;
                unsigned char theByte;
                
                for (tempCount = 0; tempCount < bytes; tempCount++)
                {
                    if (!tempCount)
                        theByte = firstByte;
                    else
                        theByte = buf[tempCount - 1];
                        
                    if ((theByte >= validUTF8[i].validBytes[(tempCount * 2)]) &&
                        (theByte <= validUTF8[i].validBytes[(tempCount * 2) + 1]))
                        hasError = no;
                    if (hasError)
                        break;
                }
            }
        }
    }
        
    *count = bytes;

    *c = n;
    
    if (hasError)
    {
#if 0
 	    /* debug */
 	    tidy_out(errout, "UTF-8 decoding error of %d bytes : ", bytes);
		tidy_out(errout, "0x%02x ", firstByte);
	    for (i = 1; i < bytes; i++)
		    tidy_out(errout, "0x%02x ", buf[i - 1]);
	    tidy_out(errout, " = U+%04lx\n", n);
#endif

       /* n = 0xFFFD; */ /* replacement char - do this in the caller */
        return -1;
    }

    return 0;
}

int EncodeCharToUTF8Bytes(uint c, unsigned char *encodebuf,
                           Out *out, PutBytes putter, int *count)
{
    unsigned char tempbuf[10];
    unsigned char *buf = &tempbuf[0];
    int i, bytes = 0;
    Bool hasError = no;
    
    if (encodebuf)
        buf = encodebuf;
        
    if (c <= 0x7F)  /* 0XXX XXXX one byte */
    {
        buf[0] = c;
        bytes = 1;
    }
    else if (c <= 0x7FF)  /* 110X XXXX  two bytes */
    {
        buf[0] = (0xC0 | (c >> 6));
        buf[1] = (0x80 | (c & 0x3F));
        bytes = 2;
    }
    else if (c <= 0xFFFF)  /* 1110 XXXX  three bytes */
    {
        buf[0] = (0xE0 | (c >> 12));
        buf[1] = (0x80 | ((c >> 6) & 0x3F));
        buf[2] = (0x80 | (c & 0x3F));
        bytes = 3;
        if ((c == kUTF8ByteSwapNotAChar) || (c == kUTF8NotAChar))
            hasError = yes;
        else if ((c >= kLowUTF16Surrogate) && (c <= kHighUTF16Surrogate))
            /* unpaired surrogates not allowed */
            hasError = yes;
    }
    else if (c <= 0x1FFFFF)  /* 1111 0XXX  four bytes */
    {
        buf[0] = (0xF0 | (c >> 18));
        buf[1] = (0x80 | ((c >> 12) & 0x3F));
        buf[2] = (0x80 | ((c >> 6) & 0x3F));
        buf[3] = (0x80 | (c & 0x3F));
        bytes = 4;
        if (c > kMaxUTF8FromUCS4)
            hasError = yes;
    }
    else if (c <= 0x3FFFFFF)  /* 1111 10XX  five bytes */
    {
        buf[0] = (0xF8 | (c >> 24));
        buf[1] = (0x80 | (c >> 18));
        buf[2] = (0x80 | ((c >> 12) & 0x3F));
        buf[3] = (0x80 | ((c >> 6) & 0x3F));
        buf[4] = (0x80 | (c & 0x3F));
        bytes = 5;
        hasError = yes;
    }
    else if (c <= 0x7FFFFFFF)  /* 1111 110X  six bytes */
    {
        buf[0] = (0xFC | (c >> 30));
        buf[1] = (0x80 | ((c >> 24) & 0x3F));
        buf[2] = (0x80 | ((c >> 18) & 0x3F));
        buf[3] = (0x80 | ((c >> 12) & 0x3F));
        buf[4] = (0x80 | ((c >> 6) & 0x3F));
        buf[5] = (0x80 | (c & 0x3F));
        bytes = 6;
        hasError = yes;
    }
    else
        hasError = yes;
        
    if (hasError)
    {
#if 0
        /* do this in the caller ? */
        /* replacement char 0xFFFD encoded as UTF-8 */
        buf[0] = 0xEF;
        buf[1] = 0xBF;
        buf[2] = 0xBD;
        bytes = 3;
#else
        putter = NULL; /* don't output invalid UTF-8 byte sequence to a stream */
#endif
    }
        
    if (putter)
    {
        int tempCount = bytes;
        
        putter(out, buf, &tempCount);
        if (tempCount < bytes)
            hasError = yes;
    }
    	
    *count = bytes;
    
    if (hasError)
	{
#if 0
	    /* debug */
	    tidy_out(errout, "UTF-8 encoding error for U+%x : ", c);
	    for (i = 0; 0 < bytes; i++)
		    tidy_out(errout, "0x%02x ", buf[i]);
	    tidy_out(errout, "\n");
#endif

        return -1;
    }
    
    return 0;
}

StreamIn *OpenInput(FILE *fp)
{
    StreamIn *in;

    in = (StreamIn *)MemAlloc(sizeof(StreamIn));
    in->file = fp;
    in->pushed = no;
    in->bufpos = 0;
    in->charbuf[0] = '\0';
    in->tabs = 0;
    in->curline = 1;
    in->curcol = 1;
    in->encoding = CharEncoding;
    in->state = FSM_ASCII;

    return in;
}

/*
   Read raw bytes from stream, return <= 0 if EOF; or if
   "unget" is true, Unget the bytes to re-synchronize the input stream
   Normally UTF-8 successor bytes are read using this routine.
*/
static void ReadRawBytesFromStream(StreamIn *in, unsigned char *buf, int *count, Bool unget)
{
    uint i;
    
    for (i = 0; i < *count; i++)
    {
        if (unget)
        {
            /* should never get here; testing for 0xFF, a valid char, is not a good idea */
            if ((in && feof(in->file)) /* || buf[i] == (unsigned char)EndOfStream */)
            {
	            /* tidy_out(errout, "Attempt to unget EOF in ReadRawBytesFromStream\n"); */ /* debug */
                *count = -i;
	            return;
            }
    
            rawPushed = yes;

            if (rawBufpos >= CHARBUF_SIZE)
            {
                memcpy(rawBytebuf, rawBytebuf + 1, CHARBUF_SIZE - 1);
                rawBufpos--;
            }
            rawBytebuf[rawBufpos++] = buf[i];

            if (buf[i] == '\n')
                --(in->curline);

            in->curcol = in->lastcol;
        }
        else
        {
            if (rawPushed)
            {
                buf[i] = rawBytebuf[--rawBufpos];
                if (rawBufpos == 0)
                    rawPushed = no;

                if (buf[i] == '\n')
                {
                    in->curcol = 1;
                    in->curline++;
                }
                else
                    in->curcol++;
            }
            else
            {
                int c;
                
                if (feof(in->file))
                {
                    *count = -i;
                    break;
                }

                c = getc(in->file);
                if (c == EOF)
                {
                    *count = -i;
                    break;
                }
                else
                {
                    buf[i] = c;
                    in->curcol++;
                }
            }
        }
    }
}

/* read char from stream */
static int ReadCharFromStream(StreamIn *in)
{
    static Bool lookingForBOM = yes;
    uint c, n;
    unsigned char tempchar;
    int count;
    
    count = 1;
    ReadRawBytesFromStream(in, &tempchar, &count, no);
    if (count <= 0)
        return EndOfStream;
    c = (uint)tempchar;
        
    if (lookingForBOM &&
       (in->encoding == UTF16   ||
        in->encoding == UTF16LE ||
        in->encoding == UTF16BE ||
        in->encoding == UTF8))
    {
        /* check for a Byte Order Mark */
        uint c1, bom;
        
        lookingForBOM = no;
        
        if (feof(in->file))
        {
            lookingForBOM = no;
            return EndOfStream;
        }
        
        count = 1;
        ReadRawBytesFromStream(in, &tempchar, &count, no);
        c1 = (uint)tempchar;
        
        bom = (c << 8) + c1;
        
        if (bom == UNICODE_BOM_BE)
        {
            /* big-endian UTF-16 */
            /* if (in->encoding != UTF16BE)
                tidy_out(errout, "Input is encoded as UTF16BE\n"); */ /* debug */
            in->encoding = UTF16BE;
            CharEncoding = UTF16BE;
            
            return UNICODE_BOM; /* return decoded BOM */
        }
        else if (bom == UNICODE_BOM_LE)
        {
            /* little-endian UTF-16 */
            /* if (in->encoding != UTF16LE)
                tidy_out(errout, "Input is encoded as UTF16LE\n"); */ /* debug */
            in->encoding = UTF16LE;
            CharEncoding = UTF16LE;
            
            return UNICODE_BOM; /* return decoded BOM */
        }
        else
        {
            uint c2;

            count = 1;
            ReadRawBytesFromStream(in, &tempchar, &count, no);
            c2 = (uint)tempchar;
       
            if (((c << 16) + (c1 << 8) + c2) == UNICODE_BOM_UTF8)
            {
                /* UTF-8 */
                /* if (in->encoding != UTF8)
                    tidy_out(errout, "Input is encoded as UTF8\n"); */ /* debug */
                in->encoding = UTF8;
                CharEncoding = UTF8;
                
                return UNICODE_BOM; /* return decoded BOM */
            }
            else
            {
                /* the 2nd and/or 3rd bytes weren't what we were */
                /* expecting, so unget the extra 2 bytes */
                rawPushed = yes;

                if ((rawBufpos + 1) >= CHARBUF_SIZE)
                {
                    memcpy(rawBytebuf, rawBytebuf + 2, CHARBUF_SIZE - 2);
                    rawBufpos -= 2;
                }
                /* make sure the bytes are pushed in the right order */
                rawBytebuf[rawBufpos++] = (unsigned char)c2;
                rawBytebuf[rawBufpos++] = (unsigned char)c1;
                
               /* drop through to code below, with the original char */
           }
        }
    }
    
    lookingForBOM = no;

    /*
       A document in ISO-2022 based encoding uses some ESC sequences
       called "designator" to switch character sets. The designators
       defined and used in ISO-2022-JP are:

        "ESC" + "(" + ?     for ISO646 variants

        "ESC" + "$" + ?     and
        "ESC" + "$" + "(" + ?   for multibyte character sets

       Where ? stands for a single character used to indicate the
       character set for multibyte characters.

       Tidy handles this by preserving the escape sequence and
       setting the top bit of each byte for non-ascii chars. This
       bit is then cleared on output. The input stream keeps track
       of the state to determine when to set/clear the bit.
    */

    if (in->encoding == ISO2022)
    {
        if (c == 0x1b)  /* ESC */
        {
            in->state = FSM_ESC;
            return c;
        }

        switch (in->state)
        {
        case FSM_ESC:
            if (c == '$')
                in->state = FSM_ESCD;
            else if (c == '(')
                in->state = FSM_ESCP;
            else
                in->state = FSM_ASCII;
            break;

        case FSM_ESCD:
            if (c == '(')
                in->state = FSM_ESCDP;
            else
                in->state = FSM_NONASCII;
            break;

        case FSM_ESCDP:
            in->state = FSM_NONASCII;
            break;

        case FSM_ESCP:
            in->state = FSM_ASCII;
            break;

        case FSM_NONASCII:
            c |= 0x80;
            break;
        }

        return c;
    }

    if (in->encoding == UTF16LE)
    {
        uint c1;
        
        count = 1;
        ReadRawBytesFromStream(in, &tempchar, &count, no);
        if (count <= 0)
            return EndOfStream;
        c1 = (uint)tempchar;
        
        n = (c1 << 8) + c;

        return n;
    }

    if ((in->encoding == UTF16) || (in->encoding == UTF16BE)) /* UTF-16 is big-endian by default */
    {
        uint c1;
        
        count = 1;
        ReadRawBytesFromStream(in, &tempchar, &count, no);
        if (count <= 0)
            return EndOfStream;
        c1 = (uint)tempchar;
        
        n = (c << 8) + c1;

        return n;
    }

    if (in->encoding == UTF8)
#if 0
    {
        /* deal with UTF-8 encoded char */

        uint i, count;

        if ((c & 0xE0) == 0xC0)  /* 110X XXXX  two bytes */
        {
            n = c & 31;
            count = 1;
        }
        else if ((c & 0xF0) == 0xE0)  /* 1110 XXXX  three bytes */
        {
            n = c & 15;
            count = 2;
        }
        else if ((c & 0xF8) == 0xF0)  /* 1111 0XXX  four bytes */
        {
            n = c & 7;
            count = 3;
        }
        else if ((c & 0xFC) == 0xF8)  /* 1111 10XX  five bytes */
        {
            n = c & 3;
            count = 4;
        }
        else if ((c & 0xFE) == 0xFC)       /* 1111 110X  six bytes */
        {
            n = c & 1;
            count = 5;
        }
        else  /* 0XXX XXXX one byte */
            return c;

        /* successor bytes should have the form 10XX XXXX */
        for (i = 1; i <= count; ++i)
        {
            if (feof(in->file))
                 return = EndOfStream;

            c = getc(in->file);

            n = (n << 6) | (c & 0x3F);
        }
        
        return n;
    }
#else
    {
        /* deal with UTF-8 encoded char */

        int err, count = 0;
        
        /* first byte "c" is passed in separately */
        err = DecodeUTF8BytesToChar(&n, c, NULL, in, ReadRawBytesFromStream, &count);
        if (!err && (n == (uint)EndOfStream) && (count == 1)) /* EOF */
            return EndOfStream;
        else if (err)
        {
            ReportEncodingError(in->lexer, ILLEGAL_UTF8, n);
            n = 0xFFFD; /* replacement char */
        }
        
        return n;
    }
#endif
    
    /* #431953 - start RJ */ 
    /*
       This section is suitable for any "multibyte" variable-width 
       character encoding in which a one-byte code is less than
       128, and the first byte of a two-byte code is greater or
       equal to 128. Note that Big5 and ShiftJIS fit into this
       kind, even though their second byte may be less than 128
    */
    if ((in->encoding == BIG5) || (in->encoding == SHIFTJIS))
    {
        if (c < 128)
            return c;
	    else
	    {
            uint c1;
		    
            count = 1;
            ReadRawBytesFromStream(in, &tempchar, &count, no);
            if (count <= 0)
                return EndOfStream;
            c1 = (uint)tempchar;
        
		    n = (c << 8) + c1;
        
		    return n;
	    }
    }
    /* #431953 - end RJ */
    else
        n = c;

    return n;
}

int ReadChar(StreamIn *in)
{
    int c;

    if (in->pushed)
    {
        c = in->charbuf[--(in->bufpos)];
        if ((in->bufpos) == 0)
            in->pushed = no;

        if (c == '\n')
        {
            in->curcol = 1;
            in->curline++;
            return c;
        }

        in->curcol++;
        return c;
    }

    in->lastcol = in->curcol;

    if (in->tabs > 0)
    {
        in->curcol++;
        in->tabs--;
        return ' ';
    }
    
    for (;;)
    {
        c = ReadCharFromStream(in);

        if (c < 0)
            return EndOfStream;

        if (c == '\n')
        {
            in->curcol = 1;
            in->curline++;
            break;
        }

        if (c == '\t')
        {
            in->tabs = tabsize - ((in->curcol - 1) % tabsize) - 1;
            in->curcol++;
            c = ' ';
            break;
        }

        /* #427663 - map '\r' to '\n' - Andy Quick 11 Aug 00 */
        if (c == '\r')
        {
            c = ReadCharFromStream(in);
            if (c != '\n')
            {
                if (c == EndOfStream) /* EOF fix by Terry Teague 12 Aug 01 */
                {
                    /* c = EndOfStream; */ /* debug */
                }
                else
                    UngetChar(c, in);
                c = '\n';
            }
            in->curcol = 1;
            in->curline++;
            break;
        }

        /* strip control characters, except for Esc */

        if (c == '\033')
            break;

        if (0 < c && c < 32)
            continue;

        /* watch out for chars that have already been decoded such as */
        /* IS02022, UTF-8 etc, that don't require further decoding */

        if (
            in->encoding == RAW      ||
            in->encoding == ISO2022  ||
            in->encoding == UTF8     ||
            in->encoding == SHIFTJIS || /* #431953 - RJ */
            in->encoding == BIG5        /* #431953 - RJ */
           )
        {
            in->curcol++;
            break;
        }

        if (in->encoding == MACROMAN)
            c = DecodeMacRoman(c);

        /* produced e.g. as a side-effect of smart quotes in Word */

        if (127 < c && c < 160)
        {
            ReportEncodingError(in->lexer, WINDOWS_CHARS, c);

            c = DecodeWin1252(c);

            if (c == 0)
                continue;
        }

        in->curcol++;
        break;
    }

    return c;
}

void UngetChar(int c, StreamIn *in)
{
    if (c == EndOfStream)
    {
	    /* tidy_out(errout, "Attempt to UngetChar EOF\n"); */ /* debug */
    }
    
    in->pushed = yes;

    if (in->bufpos >= CHARBUF_SIZE)
    {
        memcpy(in->charbuf, in->charbuf + 1, CHARBUF_SIZE - 1);
        (in->bufpos)--;
    }
    in->charbuf[(in->bufpos)++] = c;

    if (c == '\n')
        --(in->curline);

    in->curcol = in->lastcol;
}

/* like strdup but using MemAlloc */
char *wstrdup(char *str)
{
    char *s, *p;
    int len;

    if (str == null)
        return null;

    for (len = 0; str[len] != '\0'; ++len);

    s = (char *)MemAlloc(sizeof(char)*(1+len));
    for (p = s; (*p++ = *str++););
    return s;
}

/* like strndup but using MemAlloc */
char *wstrndup(char *str, int len)
{
    char *s, *p;

    if (str == null || len < 0)
        return null;

    s = (char *)MemAlloc(sizeof(char)*(1+len));

    p = s;

    while (len-- > 0 && (*p++ = *str++));

    *p = '\0';
    return s;
}

/* exactly same as strncpy */
void wstrncpy(char *s1, char *s2, int size)
{
    if (s1 != null && s2 != null)
    {
        if (size >= 0)
        {
            while (size--)
                *s1++ = *s2++;
        }
        else
            while ((*s1++ = *s2++));
    }
}

void wstrcpy(char *s1, char *s2)
{
    while ((*s1++ = *s2++));
}

void wstrcat(char *s1, char *s2)
{
    while (*s1)
        ++s1;

    while ((*s1++ = *s2++));
}

/* exactly same as strcmp */
int wstrcmp(char *s1, char *s2)    
{
    int c;

    while ((c = *s1) == *s2)
    {
        if (c == '\0')
            return 0;

        ++s1;
        ++s2;
    }

    return (*s1 > *s2 ? 1 : -1);
}

/* returns byte count, not char count */
int wstrlen(char *str)
{
    int len = 0;

    while(*str++)
        ++len;

    return len;
}

/*
 MS C 4.2 doesn't include strcasecmp.
 Note that tolower and toupper won't
 work on chars > 127
*/
int wstrcasecmp(char *s1, char *s2)    
{
    uint c;

    while (c = (uint)(*s1), ToLower(c) == ToLower((uint)(*s2)))
    {
        if (c == '\0')
            return 0;

        ++s1;
        ++s2;
    }

    return (*s1 > *s2 ? 1 : -1);
}

int wstrncmp(char *s1, char *s2, int n)    
{
    int c;

    while ((c = *s1) == *s2)
    {
        if (c == '\0')
            return 0;

        if (n == 0)
            return 0;

        ++s1;
        ++s2;
        --n;
    }

    if (n == 0)
        return 0;

    return (*s1 > *s2 ? 1 : -1);
}

int wstrncasecmp(char *s1, char *s2, int n)    
{
    int c;

    while (c = *s1, tolower(c) == tolower(*s2))
    {
        if (c == '\0')
            return 0;

        if (n == 0)
            return 0;

        ++s1;
        ++s2;
        --n;
    }

    if (n == 0)
        return 0;

    return (*s1 > *s2 ? 1 : -1);
}

/* return offset of cc from beginning of s1,
** -1 if not found.
*/
int wstrnchr( char *s1, int len1, char cc )
{
    int i;
    char* cp = s1;

    for ( i = 0; i < len1; ++i, ++cp )
    {
        if ( *cp == cc )
            return i;
    }

    return -1;
}

Bool wsubstrn( char *s1, int len1, char *s2 )
{
    int i, len2 = wstrlen(s2);

    for (i = 0; i <= len1 - len2; ++i)
    {
        if (wstrncmp(s1+i, s2, len2) == 0)
            return yes;
    }

    return no;
}

Bool wsubstrncase(char *s1, int len1, char *s2 )
{
    int i, len2 = wstrlen(s2);

    for (i = 0; i <= len1 - len2; ++i)
    {
        if (wstrncasecmp(s1+i, s2, len2) == 0)
            return yes;
    }

    return no;
}

Bool wsubstr(char *s1, char *s2)
{
    int i, len1 = wstrlen(s1), len2 = wstrlen(s2);

    for (i = 0; i <= len1 - len2; ++i)
    {
        if (wstrncasecmp(s1+i, s2, len2) == 0)
            return yes;
    }

    return no;
}

/* transform string to lower case */
char *wstrtolower(char *s)
{
    int i;
    for (i = 0; i < wstrlen(s); ++i)
        s[i] = ToLower(s[i]);

    return s;
}

/* output UTF-8 bytes to output stream */
static void outcUTF8Bytes(Out *out, unsigned char *buf, int *count)
{
    uint i;
    
    for (i = 0; i < *count; i++)
    {
        putc(buf[i], out->fp);
    }
}

/* For mac users, should we map Unicode back to MacRoman? */
void outc(uint c, Out *out)
{
    uint ch;

    if (out->encoding == UTF8)
#if 0
    {
        if (c < 128)
            putc(c, out->fp);
        else if (c <= 0x7FF)
        {
            ch = (0xC0 | (c >> 6)); putc(ch, out->fp);
            ch = (0x80 | (c & 0x3F)); putc(ch, out->fp);
        }
        else if (c <= 0xFFFF)
        {
            ch = (0xE0 | (c >> 12)); putc(ch, out->fp);
            ch = (0x80 | ((c >> 6) & 0x3F)); putc(ch, out->fp);
            ch = (0x80 | (c & 0x3F)); putc(ch, out->fp);
        }
        else if (c <= 0x1FFFFF)
        {
            ch = (0xF0 | (c >> 18)); putc(ch, out->fp);
            ch = (0x80 | ((c >> 12) & 0x3F)); putc(ch, out->fp);
            ch = (0x80 | ((c >> 6) & 0x3F)); putc(ch, out->fp);
            ch = (0x80 | (c & 0x3F)); putc(ch, out->fp);
        }
        else
        {
            ch = (0xF8 | (c >> 24)); putc(ch, out->fp);
            ch = (0x80 | ((c >> 18) & 0x3F)); putc(ch, out->fp);
            ch = (0x80 | ((c >> 12) & 0x3F)); putc(ch, out->fp);
            ch = (0x80 | ((c >> 6) & 0x3F)); putc(ch, out->fp);
            ch = (0x80 | (c & 0x3F)); putc(ch, out->fp);
        }
    }
#else
    {
        int count = 0;
        
        EncodeCharToUTF8Bytes(c, NULL, out, outcUTF8Bytes, &count);
        if (count <= 0)
        {
            /* ReportEncodingError(in->lexer, ILLEGAL_UTF8, c); */
            /* replacement char 0xFFFD encoded as UTF-8 */
            putc(0xEF, out->fp); putc(0xBF, out->fp); putc(0xBF, out->fp);
        }
    }
#endif
    else if (out->encoding == ISO2022)
    {
        if (c == 0x1b)  /* ESC */
            out->state = FSM_ESC;
        else
        {
            switch (out->state)
            {
            case FSM_ESC:
                if (c == '$')
                    out->state = FSM_ESCD;
                else if (c == '(')
                    out->state = FSM_ESCP;
                else
                    out->state = FSM_ASCII;
                break;

            case FSM_ESCD:
                if (c == '(')
                    out->state = FSM_ESCDP;
                else
                    out->state = FSM_NONASCII;
                break;

            case FSM_ESCDP:
                out->state = FSM_NONASCII;
                break;

            case FSM_ESCP:
                out->state = FSM_ASCII;
                break;

            case FSM_NONASCII:
                c &= 0x7F;
                break;
            }
        }

        putc(c, out->fp);
    }
    /* #431953 - start RJ */
    else if (out->encoding == BIG5 || out->encoding == SHIFTJIS)
    {
        if (c < 128)
            putc(c, out->fp);
        else
        {
            ch = (c >> 8) & 0xFF; putc(ch, out->fp); 
            ch = c & 0xFF; putc(ch, out->fp); 
        }
    }
    /* #431953 - end RJ */
    else
        putc(c, out->fp);
}

/*
  first time initialization which should
  precede reading the command line
*/
void InitTidy(void)
{
    InitMap();
    InitAttrs();
    InitTags();
    InitEntities();
    InitConfig();

    totalerrors = totalwarnings = 0;
    XmlTags = XmlOut = HideEndTags = UpperCaseTags =
    MakeClean = writeback = OnlyErrors = no;

    input = null;
    errfile = null;
    errout = stderr;

#ifdef CONFIG_FILE
    ParseConfigFile(CONFIG_FILE);
#endif
}

/*
  call this when you have finished with tidy
  to free the hash tables and other resources
*/
void DeInitTidy(void)
{
    FreeTags();
    FreeAttrTable();
    FreeEntities();
    FreeConfig();
    FreePrintBuf();
}

int main(int argc, char **argv)
{
    char *file, *prog;
    Node *document, *doctype;
    Lexer *lexer;
    char *s, c, *arg, *current_errorfile = "stderr";
    Out out;   /* normal output stream */

#if PRESERVEFILETIMES
    struct utimbuf filetimes;
    struct stat sbuf;
#endif
    Bool haveFileTimes;

    InitTidy();

    /* look for env var "HTML_TIDY" */
    /* then for ~/.tidyrc (on Unix) */

    if ((file = getenv("HTML_TIDY")))
        ParseConfigFile(file);
#ifdef SUPPORT_GETPWNAM
    else
        ParseConfigFile("~/.tidyrc");
#endif /* SUPPORT_GETPWNAM */

    /* read command line */

    prog = argv[0];

    while (argc > 0)
    {
        if (argc > 1 && argv[1][0] == '-')
        {
            /* support -foo and --foo */
            arg = argv[1] + 1;
#if 0
            if (arg[0] == '-')
                ++arg;
#endif
            /* #427667 - fix by Randy Waki 04 Aug 00 */
            /*
            if (strcmp(arg, "indent") == 0)
                IndentContent = yes;
            else */ if (strcmp(arg, "xml") == 0)
                XmlTags = yes;
            else if (strcmp(arg, "asxml") == 0 || strcmp(arg, "asxhtml") == 0)
                xHTML = yes;
            else if (strcmp(arg, "indent") == 0)
            {
                IndentContent = yes;
                SmartIndent = yes;
            }
            else if (strcmp(arg, "omit") == 0)
                HideEndTags = yes;
            else if (strcmp(arg, "upper") == 0)
                UpperCaseTags = yes;
            else if (strcmp(arg, "clean") == 0)
                MakeClean = yes;
            else if (strcmp(arg, "raw") == 0)
                CharEncoding = RAW;
            else if (strcmp(arg, "ascii") == 0)
                CharEncoding = ASCII;
            else if (strcmp(arg, "latin1") == 0)
                CharEncoding = LATIN1;
            else if (strcmp(arg, "utf8") == 0)
                CharEncoding = UTF8;
            else if (strcmp(arg, "iso2022") == 0)
                CharEncoding = ISO2022;
            else if (strcmp(arg, "mac") == 0)
                CharEncoding = MACROMAN;
            else if (strcmp(arg, "utf16le") == 0)
                CharEncoding = UTF16LE;
            else if (strcmp(arg, "utf16be") == 0)
                CharEncoding = UTF16BE;
            else if (strcmp(arg, "utf16") == 0)
                CharEncoding = UTF16;
            else if (strcmp(arg, "win1252") == 0)
                CharEncoding = WIN1252;
            else if (strcmp(argv[1], "-shiftjis") == 0) /* #431953 - RJ */
                CharEncoding = SHIFTJIS; /* #431953 - RJ */
            else if (strcmp(argv[1], "-big5") == 0) /* #431953 - RJ */
                CharEncoding = BIG5; /* #431953 - RJ */
            else if (strcmp(arg, "numeric") == 0)
                NumEntities = yes;
            else if (strcmp(arg, "modify") == 0)
                writeback = yes;
            else if (strcmp(arg, "change") == 0)  /* obsolete */
                writeback = yes;
            else if (strcmp(arg, "update") == 0)  /* obsolete */
                writeback = yes;
            else if (strcmp(arg, "errors") == 0)
                OnlyErrors = yes;
            else if (strcmp(arg, "quiet") == 0)
                Quiet = yes;
            else if (strcmp(arg, "slides") == 0)
                BurstSlides = yes;
            else if (strcmp(arg, "help") == 0 ||
                     argv[1][1] == '?'|| argv[1][1] == 'h')
            {
                HelpText(stdout, prog);
                return 1;
            }
            else if (strcmp(arg, "config") == 0)
            {
                if (argc >= 3)
                {
                    ParseConfigFile(argv[2]);
                    --argc;
                    ++argv;
                }
            }
            /* #431953 - start RJ */
            else if (strcmp(argv[1], "-language") == 0 ||
                     strcmp(argv[1], "-lang") == 0)
            {
                if (argc >= 3)
                {
                    Language = argv[2];
                    --argc;
                    ++argv;
                }
            }
            /* #431953 - end RJ */
            else if (strcmp(argv[1], "-file") == 0 ||
                     strcmp(argv[1], "--file") == 0 ||
                        strcmp(argv[1], "-f") == 0)
            {
                if (argc >= 3)
                {
                    /* create copy that can be freed by FreeConfig() */
                    errfile = wstrdup(argv[2]);
                    --argc;
                    ++argv;
                }
            }
            else if (strcmp(argv[1], "-wrap") == 0 ||
                        strcmp(argv[1], "--wrap") == 0 ||
                        strcmp(argv[1], "-w") == 0)
            {
                if (argc >= 3)
                {
                    sscanf(argv[2], "%d", &wraplen);
                    --argc;
                    ++argv;
                }
            }
            else if (strcmp(argv[1], "-version") == 0 ||
                        strcmp(argv[1], "--version") == 0 ||
                        strcmp(argv[1], "-v") == 0)
            {
                ShowVersion(errout);
                /* called to free hash tables etc. */
                DeInitTidy();
                return 0;

            }
            else if(strncmp(argv[1],"--",2)==0)
            {
                if (ParseConfig(argv[1]+2, argv[2]))
                {
                    ++argv;
                    --argc;
                }
            }
            else
            {
                s = argv[1];

                while ((c = *++s))
                {
                    if (c == 'i')
                    {
                        IndentContent = yes;
                        SmartIndent = yes;
                    }
                    else if (c == 'o')
                        HideEndTags = yes;
                    else if (c == 'u')
                        UpperCaseTags = yes;
                    else if (c == 'c')
                        MakeClean = yes;
                    else if (c == 'n')
                        NumEntities = yes;
                    else if (c == 'm')
                        writeback = yes;
                    else if (c == 'e')
                        OnlyErrors = yes;
                    else if (c == 'q')
                        Quiet = yes;
                    else
                        UnknownOption(stderr, c);
                }
            }

            --argc;
            ++argv;
            continue;
        }

        /* ensure config is self-consistent */
        AdjustConfig();

        /* user specified error file */
        if (errfile)
        {
            FILE *fp;

            /* is it same as the currently opened file? */
            if (wstrcmp(errfile, current_errorfile) != 0)
            {
                /* no so close previous error file */

                if (errout != stderr)
                    fclose(errout);

                /* and try to open the new error file */
                fp = fopen(errfile, "w");

                if (fp != null)
                {
                    errout = fp;
                    current_errorfile = errfile;
                }
                else /* can't be opened so fall back to stderr */
                {
                    errout = stderr;
                    current_errorfile = "stderr";
                }
            }
        }

        haveFileTimes = no;

        if (argc > 1)
        {
            file = argv[1];
            input = fopen(file, "r");

#if PRESERVEFILETIMES
            /* get last modified time */
            if (KeepFileTimes && input && fstat(fileno(input), &sbuf) != -1)
            {
                filetimes.actime = sbuf.st_atime;
                filetimes.modtime = sbuf.st_mtime;
                haveFileTimes = yes;
            }
#endif
        }
        else
        {
            input = stdin;
            file = "stdin";
        }

        if (input != null)
        {
            lexer = NewLexer(OpenInput(input));
            lexer->errout = errout;

            /*
              store pointer to lexer in input stream
              to allow character encoding errors to be
              reported
            */
            lexer->in->lexer = lexer;

            SetFilename(file);	/* #431895 - fix by Dave Bryan 04 Jan 01 */

            /* skip byte order mark */
            if (lexer->in->encoding == UTF8    ||
                lexer->in->encoding == UTF16LE ||
                lexer->in->encoding == UTF16BE ||
                lexer->in->encoding == UTF16)
            {
                uint c = ReadChar(lexer->in);
                
                if (c != UNICODE_BOM)
                    UngetChar(c, lexer->in);
            }
            
            /* Tidy doesn't alter the doctype for generic XML docs */
            if (XmlTags)
                document = ParseXMLDocument(lexer);
            else
            {
                lexer->warnings = 0;

                if (!Quiet)
                    HelloMessage(errout, release_date, file);

                document = ParseDocument(lexer);

                if (!CheckNodeIntegrity(document))
                {
                    fprintf(stderr, "\nPanic - tree has lost its integrity\n");
                    exit(1);
                }

                /* simplifies <b><b> ... </b> ...</b> etc. */
                NestedEmphasis(document);

                /* cleans up <dir>indented text</dir> etc. */
                List2BQ(document);
                BQ2Div(document);

                /* replaces i by em and b by strong */
                if (LogicalEmphasis)
                    EmFromI(document);

                if (Word2000 && IsWord2000(document))
                {
                    /* prune Word2000's <![if ...]> ... <![endif]> */
                    DropSections(lexer, document);

                    /* drop style & class attributes and empty p, span elements */
                    CleanWord2000(lexer, document);
                }

                /* replaces presentational markup by style rules */
                if (MakeClean || DropFontTags)
                    CleanTree(lexer, document);

                if (!CheckNodeIntegrity(document))
                {
                    fprintf(stderr, "\nPanic - tree has lost its integrity\n");
                    exit(1);
                }

                if (document->content)
                {
                    if (xHTML)
                        SetXHTMLDocType(lexer, document);
                    else
                        FixDocType(lexer, document);

                    if (TidyMark)
                        AddGenerator(lexer, document);
                }

                /* ensure presence of initial <?XML version="1.0"?> */
                if (XmlOut && XmlPi)
                    FixXmlDecl(lexer, document);

                totalwarnings += lexer->warnings;
                totalerrors += lexer->errors;

                if (!Quiet && document->content)
                {
                    doctype = FindDocType(document);
                    ReportVersion(errout, lexer, file, doctype);
                    ReportNumWarnings(errout, lexer);
                }
            }

            if (input != stdin)
            {
                fclose(input);
            }

            MemFree(lexer->in);

            if (lexer->errors > 0 && !ForceOutput)
                NeedsAuthorIntervention(errout);

            out.state = FSM_ASCII;
            out.encoding = CharEncoding;

            if (!OnlyErrors && (lexer->errors == 0 || ForceOutput))
            {
                if (BurstSlides)
                {
                    Node *body, *doctype;

                    /*
                       remove doctype to avoid potential clash with
                       markup introduced when bursting into slides
                    */
                    /* discard the document type */
                    doctype = FindDocType(document);

                    if (doctype)
                        DiscardElement(doctype);

                    /* slides use transitional features */
                    lexer->versions |= VERS_HTML40_LOOSE;

                    /* and patch up doctype to match */
                    if (xHTML)
                        SetXHTMLDocType(lexer, document);
                    else
                        FixDocType(lexer, document);


                    /* find the body element which may be implicit */
                    body = FindBody(document);

                    if (body)
                    {
                        ReportNumberOfSlides(errout, CountSlides(body));
                        CreateSlides(lexer, document);
                    }
                    else
                        MissingBody(errout);
                }
                else if (writeback && (input = fopen(file, "w")))
                {
                    out.fp = input;

                    if (XmlOut && !xHTML /*XmlTags*/)	/* #427826 - fix by Dave Raggett 01 Sep 00 */
                        PPrintXMLTree(&out, null, 0, lexer, document);
                    /* Feature request #434940 - fix by Dave Raggett/Ignacio Vazquez-Abrams 21 Jun 01 */
                    else if (BodyOnly)
                        PrintBody(&out, lexer, document);
                    else
                        PPrintTree(&out, null, 0, lexer, document);

                    PFlushLine(&out, 0);

#if PRESERVEFILETIMES
                    /* set file last accessed/modified times to original values */
                    if (haveFileTimes)
                        futime(fileno(input), &filetimes);
#endif
                    fclose(input);
                }
                else
                {
                    out.fp = stdout;

                    if (XmlOut && !xHTML /*XmlTags*/)	/* #427826 - fix by Dave Raggett 01 Sep 00 */
                        PPrintXMLTree(&out, null, 0, lexer, document);
                    /* Feature request #434940 - fix by Dave Raggett/Ignacio Vazquez-Abrams 21 Jun 01 */
                    else if (BodyOnly)
                        PrintBody(&out, lexer, document);
                    else
                        PPrintTree(&out, null, 0, lexer, document);

                    PFlushLine(&out, 0);
                }

            }

            if (!Quiet)
                ErrorSummary(lexer);

            FreeNode(document);
            FreeLexer(lexer);
        }
        else
            UnknownFile(errout, prog, file);

        --argc;
        ++argv;

        if (argc <= 1)
            break;
    }

    if (totalerrors + totalwarnings > 0 && !Quiet)
        GeneralInfo(errout);

    if (errout != stderr)
        fclose(errout);

    /* called to free hash tables etc. */
    DeInitTidy();

    /* return status can be used by scripts */

    if (totalerrors > 0)
        return 2;

    if (totalwarnings > 0)
        return 1;

    /* 0 signifies all is ok */
    return 0;
}

