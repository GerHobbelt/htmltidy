/* attrs.c -- recognize HTML attributes

  (c) 1998-2003 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.
  
  CVS Info :

    $Author$ 
    $Date$ 
    $Revision$ 

*/

#include "tidy-int.h"
#include "attrs.h"
#include "message.h"
#include "tmbstr.h"
#include "utf8.h"

static const Attribute attribute_defs [] =
{
  { TidyAttr_UNKNOWN,           "unknown!",          VERS_PROPRIETARY,  NULL,       no }, 
  { TidyAttr_ABBR,              "abbr",              VERS_HTML40,       PCDATA,     no }, 
  { TidyAttr_ACCEPT,            "accept",            VERS_ALL,          TYPE,       no }, 
  { TidyAttr_ACCEPT_CHARSET,    "accept-charset",    VERS_HTML40,       CHARSET,    no }, 
  { TidyAttr_ACCESSKEY,         "accesskey",         VERS_HTML40,       CHARACTER,  no }, 
  { TidyAttr_ACTION,            "action",            VERS_ALL,          URL,        no }, 
  { TidyAttr_ADD_DATE,          "add_date",          VERS_NETSCAPE,     PCDATA,     no }, /* A */
  { TidyAttr_ALIGN,             "align",             VERS_ALL,          ALIGN,      no }, /* varies by element */
  { TidyAttr_ALINK,             "alink",             VERS_LOOSE,        COLOR,      no }, 
  { TidyAttr_ALT,               "alt",               VERS_ALL,          PCDATA,    yes }, /* nowrap */
  { TidyAttr_ARCHIVE,           "archive",           VERS_HTML40,       URLS,       no }, /* space or comma separated list */
  { TidyAttr_AXIS,              "axis",              VERS_HTML40,       PCDATA,     no }, 
  { TidyAttr_BACKGROUND,        "background",        VERS_LOOSE,        URL,        no }, 
  { TidyAttr_BGCOLOR,           "bgcolor",           VERS_LOOSE,        COLOR,      no }, 
  { TidyAttr_BGPROPERTIES,      "bgproperties",      VERS_PROPRIETARY,  PCDATA,     no }, /* BODY "fixed" fixes background */
  { TidyAttr_BORDER,            "border",            VERS_ALL,          BORDER,     no }, /* like LENGTH + "border" */
  { TidyAttr_BORDERCOLOR,       "bordercolor",       VERS_MICROSOFT,    COLOR,      no }, /* used on TABLE */
  { TidyAttr_BOTTOMMARGIN,      "bottommargin",      VERS_MICROSOFT,    NUMBER,     no }, /* used on BODY */
  { TidyAttr_CELLPADDING,       "cellpadding",       VERS_FROM32,       LENGTH,     no }, /* % or pixel values */
  { TidyAttr_CELLSPACING,       "cellspacing",       VERS_FROM32,       LENGTH,     no }, 
  { TidyAttr_CHAR,              "char",              VERS_HTML40,       CHARACTER,  no }, 
  { TidyAttr_CHAROFF,           "charoff",           VERS_HTML40,       LENGTH,     no }, 
  { TidyAttr_CHARSET,           "charset",           VERS_HTML40,       CHARSET,    no }, 
  { TidyAttr_CHECKED,           "checked",           VERS_ALL,          BOOL,       no }, /* i.e. "checked" or absent */
  { TidyAttr_CITE,              "cite",              VERS_HTML40,       URL,        no }, 
  { TidyAttr_CLASS,             "class",             VERS_HTML40,       PCDATA,     no }, 
  { TidyAttr_CLASSID,           "classid",           VERS_HTML40,       URL,        no }, 
  { TidyAttr_CLEAR,             "clear",             VERS_LOOSE,        CLEAR,      no }, /* BR: left, right, all */
  { TidyAttr_CODE,              "code",              VERS_LOOSE,        PCDATA,     no }, /* APPLET */
  { TidyAttr_CODEBASE,          "codebase",          VERS_HTML40,       URL,        no }, /* OBJECT */
  { TidyAttr_CODETYPE,          "codetype",          VERS_HTML40,       TYPE,       no }, /* OBJECT */
  { TidyAttr_COLOR,             "color",             VERS_LOOSE,        COLOR,      no }, /* BASEFONT, FONT */
  { TidyAttr_COLS,              "cols",              VERS_IFRAME,       COLS,       no }, /* TABLE & FRAMESET */
  { TidyAttr_COLSPAN,           "colspan",           VERS_FROM32,       NUMBER,     no }, 
  { TidyAttr_COMPACT,           "compact",           VERS_ALL,          BOOL,       no }, /* lists */
  { TidyAttr_CONTENT,           "content",           VERS_ALL,          PCDATA,    yes }, 
  { TidyAttr_COORDS,            "coords",            VERS_FROM32,       COORDS,     no }, /* AREA, A */
  { TidyAttr_DATA,              "data",              VERS_HTML40,       URL,        no }, /* OBJECT */
  { TidyAttr_DATAFLD,           "datafld",           VERS_MICROSOFT,    PCDATA,     no }, /* used on DIV, IMG */
  { TidyAttr_DATAFORMATAS,      "dataformatas",      VERS_MICROSOFT,    PCDATA,     no }, /* used on DIV, IMG */
  { TidyAttr_DATAPAGESIZE,      "datapagesize",      VERS_MICROSOFT,    NUMBER,     no }, /* used on DIV, IMG */
  { TidyAttr_DATASRC,           "datasrc",           VERS_MICROSOFT,    URL,        no }, /* used on TABLE */
  { TidyAttr_DATETIME,          "datetime",          VERS_HTML40,       DATE,       no }, /* INS, DEL */
  { TidyAttr_DECLARE,           "declare",           VERS_HTML40,       BOOL,       no }, /* OBJECT */
  { TidyAttr_DEFER,             "defer",             VERS_HTML40,       BOOL,       no }, /* SCRIPT */
  { TidyAttr_DIR,               "dir",               VERS_HTML40,       TEXTDIR,    no }, /* ltr or rtl */
  { TidyAttr_DISABLED,          "disabled",          VERS_HTML40,       BOOL,       no }, /* form fields */
  { TidyAttr_ENCODING,          "encoding",          VERS_XML,          PCDATA,     no }, /* <?xml?> */
  { TidyAttr_ENCTYPE,           "enctype",           VERS_ALL,          TYPE,       no }, /* FORM */
  { TidyAttr_FACE,              "face",              VERS_LOOSE,        PCDATA,     no }, /* BASEFONT, FONT */
  { TidyAttr_FOR,               "for",               VERS_HTML40,       IDREF,      no }, /* LABEL */
  { TidyAttr_FRAME,             "frame",             VERS_HTML40,       TFRAME,     no }, /* TABLE */
  { TidyAttr_FRAMEBORDER,       "frameborder",       VERS_FRAMESET,     FBORDER,    no }, /* 0 or 1 */
  { TidyAttr_FRAMESPACING,      "framespacing",      VERS_PROPRIETARY,  NUMBER,     no }, 
  { TidyAttr_GRIDX,             "gridx",             VERS_PROPRIETARY,  NUMBER,     no }, /* TABLE Adobe golive*/
  { TidyAttr_GRIDY,             "gridy",             VERS_PROPRIETARY,  NUMBER,     no }, /* TABLE Adobe golive */
  { TidyAttr_HEADERS,           "headers",           VERS_HTML40,       IDREFS,     no }, /* table cells */
  { TidyAttr_HEIGHT,            "height",            VERS_ALL,          LENGTH,     no }, /* pixels only for TH/TD */
  { TidyAttr_HREF,              "href",              VERS_ALL,          URL,        no }, /* A, AREA, LINK and BASE */
  { TidyAttr_HREFLANG,          "hreflang",          VERS_HTML40,       LANG,       no }, /* A, LINK */
  { TidyAttr_HSPACE,            "hspace",            VERS_ALL,          NUMBER,     no }, /* APPLET, IMG, OBJECT */
  { TidyAttr_HTTP_EQUIV,        "http-equiv",        VERS_ALL,          PCDATA,     no }, /* META */
  { TidyAttr_ID,                "id",                VERS_HTML40,       IDDEF,      no }, 
  { TidyAttr_ISMAP,             "ismap",             VERS_ALL,          BOOL,       no }, /* IMG */
  { TidyAttr_LABEL,             "label",             VERS_HTML40,       PCDATA,     no }, /* OPT, OPTGROUP */
  { TidyAttr_LANG,              "lang",              VERS_HTML40,       LANG,       no }, 
  { TidyAttr_LANGUAGE,          "language",          VERS_LOOSE,        PCDATA,     no }, /* SCRIPT */
  { TidyAttr_LAST_MODIFIED,     "last_modified",     VERS_NETSCAPE,     PCDATA,     no }, /* A */
  { TidyAttr_LAST_VISIT,        "last_visit",        VERS_NETSCAPE,     PCDATA,     no }, /* A */
  { TidyAttr_LEFTMARGIN,        "leftmargin",        VERS_MICROSOFT,    NUMBER,     no }, /* used on BODY */
  { TidyAttr_LINK,              "link",              VERS_LOOSE,        COLOR,      no }, /* BODY */
  { TidyAttr_LONGDESC,          "longdesc",          VERS_HTML40,       URL,        no }, /* IMG */
  { TidyAttr_LOWSRC,            "lowsrc",            VERS_PROPRIETARY,  URL,        no }, /* IMG */
  { TidyAttr_MARGINHEIGHT,      "marginheight",      VERS_IFRAME,       NUMBER,     no }, /* FRAME, IFRAME, BODY */
  { TidyAttr_MARGINWIDTH,       "marginwidth",       VERS_IFRAME,       NUMBER,     no }, /* ditto */
  { TidyAttr_MAXLENGTH,         "maxlength",         VERS_ALL,          NUMBER,     no }, /* INPUT */
  { TidyAttr_MEDIA,             "media",             VERS_HTML40,       MEDIA,      no }, /* STYLE, LINK */
  { TidyAttr_METHOD,            "method",            VERS_ALL,          FSUBMIT,    no }, /* FORM: get or post */
  { TidyAttr_MULTIPLE,          "multiple",          VERS_ALL,          BOOL,       no }, /* SELECT */
  { TidyAttr_NAME,              "name",              VERS_ALL,          NAME,       no }, 
  { TidyAttr_NOHREF,            "nohref",            VERS_FROM32,       BOOL,       no }, /* AREA */
  { TidyAttr_NORESIZE,          "noresize",          VERS_FRAMESET,     BOOL,       no }, /* FRAME */
  { TidyAttr_NOSHADE,           "noshade",           VERS_LOOSE,        BOOL,       no }, /* HR */
  { TidyAttr_NOWRAP,            "nowrap",            VERS_LOOSE,        BOOL,       no }, /* table cells */
  { TidyAttr_OBJECT,            "object",            VERS_HTML40_LOOSE, PCDATA,     no }, /* APPLET */
  { TidyAttr_OnAFTERUPDATE,     "onafterupdate",     VERS_MICROSOFT,    SCRIPT,     no }, 
  { TidyAttr_OnBEFOREUNLOAD,    "onbeforeunload",    VERS_MICROSOFT,    SCRIPT,     no }, 
  { TidyAttr_OnBEFOREUPDATE,    "onbeforeupdate",    VERS_MICROSOFT,    SCRIPT,     no }, 
  { TidyAttr_OnBLUR,            "onblur",            VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnCHANGE,          "onchange",          VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnCLICK,           "onclick",           VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnDATAAVAILABLE,   "ondataavailable",   VERS_MICROSOFT,    SCRIPT,     no }, /* object, applet */
  { TidyAttr_OnDATASETCHANGED,  "ondatasetchanged",  VERS_MICROSOFT,    SCRIPT,     no }, /* object, applet */
  { TidyAttr_OnDATASETCOMPLETE, "ondatasetcomplete", VERS_MICROSOFT,    SCRIPT,     no }, 
  { TidyAttr_OnDBLCLICK,        "ondblclick",        VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnERRORUPDATE,     "onerrorupdate",     VERS_MICROSOFT,    SCRIPT,     no }, /* form fields */
  { TidyAttr_OnFOCUS,           "onfocus",           VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnKEYDOWN,         "onkeydown",         VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnKEYPRESS,        "onkeypress",        VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnKEYUP,           "onkeyup",           VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnLOAD,            "onload",            VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnMOUSEDOWN,       "onmousedown",       VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnMOUSEMOVE,       "onmousemove",       VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnMOUSEOUT,        "onmouseout",        VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnMOUSEOVER,       "onmouseover",       VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnMOUSEUP,         "onmouseup",         VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnRESET,           "onreset",           VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnROWENTER,        "onrowenter",        VERS_MICROSOFT,    SCRIPT,     no }, /* form fields */
  { TidyAttr_OnROWEXIT,         "onrowexit",         VERS_MICROSOFT,    SCRIPT,     no }, /* form fields */
  { TidyAttr_OnSELECT,          "onselect",          VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnSUBMIT,          "onsubmit",          VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_OnUNLOAD,          "onunload",          VERS_EVENTS,       SCRIPT,     no }, /* event */
  { TidyAttr_PROFILE,           "profile",           VERS_HTML40,       URL,        no }, /* HEAD */
  { TidyAttr_PROMPT,            "prompt",            VERS_LOOSE,        PCDATA,     no }, /* ISINDEX */
  { TidyAttr_RBSPAN,            "rbspan",            VERS_XHTML11,      NUMBER,     no }, /* ruby markup */
  { TidyAttr_READONLY,          "readonly",          VERS_HTML40,       BOOL,       no }, /* form fields */
  { TidyAttr_REL,               "rel",               VERS_ALL,          LINKTYPES,  no }, 
  { TidyAttr_REV,               "rev",               VERS_ALL,          LINKTYPES,  no }, 
  { TidyAttr_RIGHTMARGIN,       "rightmargin",       VERS_MICROSOFT,    NUMBER,     no }, /* used on BODY */
  { TidyAttr_ROWS,              "rows",              VERS_ALL,          NUMBER,     no }, /* TEXTAREA */
  { TidyAttr_ROWSPAN,           "rowspan",           VERS_ALL,          NUMBER,     no }, /* table cells */
  { TidyAttr_RULES,             "rules",             VERS_HTML40,       TRULES,     no }, /* TABLE */
  { TidyAttr_SCHEME,            "scheme",            VERS_HTML40,       PCDATA,     no }, /* META */
  { TidyAttr_SCOPE,             "scope",             VERS_HTML40,       SCOPE,      no }, /* table cells */
  { TidyAttr_SCROLLING,         "scrolling",         VERS_IFRAME,       SCROLL,     no }, /* yes, no or auto */
  { TidyAttr_SELECTED,          "selected",          VERS_ALL,          BOOL,       no }, /* OPTION */
  { TidyAttr_SHAPE,             "shape",             VERS_FROM32,       SHAPE,      no }, /* AREA, A */
  { TidyAttr_SHOWGRID,          "showgrid",          VERS_PROPRIETARY,  BOOL,       no }, /* TABLE Adobe golive */
  { TidyAttr_SHOWGRIDX,         "showgridx",         VERS_PROPRIETARY,  BOOL,       no }, /* TABLE Adobe golive*/
  { TidyAttr_SHOWGRIDY,         "showgridy",         VERS_PROPRIETARY,  BOOL,       no }, /* TABLE Adobe golive*/
  { TidyAttr_SIZE,              "size",              VERS_LOOSE,        NUMBER,     no }, /* HR, FONT, BASEFONT, SELECT */
  { TidyAttr_SPAN,              "span",              VERS_HTML40,       NUMBER,     no }, /* COL, COLGROUP */
  { TidyAttr_SRC,               "src",               VERS_ALL,          URL,        no }, /* IMG, FRAME, IFRAME */
  { TidyAttr_STANDBY,           "standby",           VERS_HTML40,       PCDATA,     no }, /* OBJECT */
  { TidyAttr_START,             "start",             VERS_ALL,          NUMBER,     no }, /* OL */
  { TidyAttr_STYLE,             "style",             VERS_HTML40,       PCDATA,     no }, 
  { TidyAttr_SUMMARY,           "summary",           VERS_HTML40,       PCDATA,     no }, /* TABLE */
  { TidyAttr_TABINDEX,          "tabindex",          VERS_HTML40,       NUMBER,     no }, /* fields, OBJECT  and A */
  { TidyAttr_TARGET,            "target",            VERS_HTML40,       TARGET,     no }, /* names a frame/window */
  { TidyAttr_TEXT,              "text",              VERS_LOOSE,        COLOR,      no }, /* BODY */
  { TidyAttr_TITLE,             "title",             VERS_HTML40,       PCDATA,     no }, /* text tool tip */
  { TidyAttr_TOPMARGIN,         "topmargin",         VERS_MICROSOFT,    NUMBER,     no }, /* used on BODY */
  { TidyAttr_TYPE,              "type",              VERS_FROM32,       TYPE,       no }, /* also used by SPACER */
  { TidyAttr_USEMAP,            "usemap",            VERS_ALL,          BOOL,       no }, /* things with images */
  { TidyAttr_VALIGN,            "valign",            VERS_FROM32,       VALIGN,     no }, 
  { TidyAttr_VALUE,             "value",             VERS_ALL,          PCDATA,    yes }, 
  { TidyAttr_VALUETYPE,         "valuetype",         VERS_HTML40,       VTYPE,      no }, /* PARAM: data, ref, object */
  { TidyAttr_VERSION,           "version",           VERS_ALL|VERS_XML, PCDATA,     no }, /* HTML <?xml?> */
  { TidyAttr_VLINK,             "vlink",             VERS_LOOSE,        COLOR,      no }, /* BODY */
  { TidyAttr_VSPACE,            "vspace",            VERS_LOOSE,        NUMBER,     no }, /* IMG, OBJECT, APPLET */
  { TidyAttr_WIDTH,             "width",             VERS_ALL,          LENGTH,     no }, /* pixels only for TD/TH */
  { TidyAttr_WRAP,              "wrap",              VERS_NETSCAPE,     PCDATA,     no }, /* textarea */
  { TidyAttr_XML_LANG,          "xml:lang",          VERS_XML,          LANG,       no }, /* XML language */
  { TidyAttr_XML_SPACE,         "xml:space",         VERS_XML,          PCDATA,     no }, /* XML language */
  { TidyAttr_XMLNS,             "xmlns",             VERS_ALL,          PCDATA,     no }, /* name space */
  { TidyAttr_EVENT,             "event",             VERS_HTML40,       PCDATA,     no }, /* reserved for <script> */
  { TidyAttr_METHODS,           "methods",           VERS_HTML20,       PCDATA,     no }, /* for <a>, never implemented */
  { TidyAttr_N,                 "n",                 VERS_HTML20,       PCDATA,     no }, /* for <nextid> */
  { TidyAttr_SDAFORM,           "sdaform",           VERS_HTML20,       PCDATA,     no }, /* SDATA attribute in HTML 2.0 */
  { TidyAttr_SDAPREF,           "sdapref",           VERS_HTML20,       PCDATA,     no }, /* SDATA attribute in HTML 2.0 */
  { TidyAttr_SDASUFF,           "sdasuff",           VERS_HTML20,       PCDATA,     no }, /* SDATA attribute in HTML 2.0 */
  { TidyAttr_URN,               "urn",               VERS_HTML20,       PCDATA,     no }, /* for <a>, never implemented */

  /* this must be the final entry */
  { N_TIDY_ATTRIBS,             NULL,                VERS_UNKNOWN,      NULL,       no }
};

uint AttributeVersions(Node* node, AttVal* attval)
{
    uint i;

    if (!attval || !attval->dict)
        return VERS_UNKNOWN;

    if (!node || !node->tag || !node->tag->attrvers)
        return attval->dict->versions;

    for (i = 0; node->tag->attrvers[i].attribute; ++i)
        if (node->tag->attrvers[i].attribute == (uint)attval->dict->id)
            return node->tag->attrvers[i].versions;

    return attval->dict->versions;

}

/* used by CheckColor() */
struct _colors
{
    ctmbstr name;
    ctmbstr hex;
};

static const struct _colors colors[] =
{
    { "black",   "#000000" },
    { "green",   "#008000" },
    { "silver",  "#C0C0C0" },
    { "lime",    "#00FF00" },
    { "gray",    "#808080" },
    { "olive",   "#808000" },
    { "white",   "#FFFFFF" },
    { "yellow",  "#FFFF00" },
    { "maroon",  "#800000" },
    { "navy",    "#000080" },
    { "red",     "#FF0000" },
    { "blue",    "#0000FF" },
    { "purple",  "#800080" },
    { "teal",    "#008080" },
    { "fuchsia", "#FF00FF" },
    { "aqua",    "#00FFFF" },
    { NULL,      NULL      }
};

ctmbstr GetColorCode(ctmbstr name)
{
    uint i;

    for (i = 0; colors[i].name; ++i)
        if (tmbstrcasecmp(name, colors[i].name) == 0)
            return colors[i].hex;

    return NULL;
}

ctmbstr GetColorName(ctmbstr code)
{
    uint i;

    for (i = 0; colors[i].name; ++i)
        if (tmbstrcasecmp(code, colors[i].hex) == 0)
            return colors[i].name;

    return NULL;
}

static const struct _colors fancy_colors[] =
{
    { "darkgreen",            "#006400" },
    { "antiquewhite",         "#FAEBD7" },
    { "aqua",                 "#00FFFF" },
    { "aquamarine",           "#7FFFD4" },
    { "azure",                "#F0FFFF" },
    { "beige",                "#F5F5DC" },
    { "bisque",               "#FFE4C4" },
    { "black",                "#000000" },
    { "blanchedalmond",       "#FFEBCD" },
    { "blue",                 "#0000FF" },
    { "blueviolet",           "#8A2BE2" },
    { "brown",                "#A52A2A" },
    { "burlywood",            "#DEB887" },
    { "cadetblue",            "#5F9EA0" },
    { "chartreuse",           "#7FFF00" },
    { "chocolate",            "#D2691E" },
    { "coral",                "#FF7F50" },
    { "cornflowerblue",       "#6495ED" },
    { "cornsilk",             "#FFF8DC" },
    { "crimson",              "#DC143C" },
    { "cyan",                 "#00FFFF" },
    { "darkblue",             "#00008B" },
    { "darkcyan",             "#008B8B" },
    { "darkgoldenrod",        "#B8860B" },
    { "darkgray",             "#A9A9A9" },
    { "darkgreen",            "#006400" },
    { "darkkhaki",            "#BDB76B" },
    { "darkmagenta",          "#8B008B" },
    { "darkolivegreen",       "#556B2F" },
    { "darkorange",           "#FF8C00" },
    { "darkorchid",           "#9932CC" },
    { "darkred",              "#8B0000" },
    { "darksalmon",           "#E9967A" },
    { "darkseagreen",         "#8FBC8F" },
    { "darkslateblue",        "#483D8B" },
    { "darkslategray",        "#2F4F4F" },
    { "darkturquoise",        "#00CED1" },
    { "darkviolet",           "#9400D3" },
    { "deeppink",             "#FF1493" },
    { "deepskyblue",          "#00BFFF" },
    { "dimgray",              "#696969" },
    { "dodgerblue",           "#1E90FF" },
    { "firebrick",            "#B22222" },
    { "floralwhite",          "#FFFAF0" },
    { "forestgreen",          "#228B22" },
    { "fuchsia",              "#FF00FF" },
    { "gainsboro",            "#DCDCDC" },
    { "ghostwhite",           "#F8F8FF" },
    { "gold",                 "#FFD700" },
    { "goldenrod",            "#DAA520" },
    { "gray",                 "#808080" },
    { "green",                "#008000" },
    { "greenyellow",          "#ADFF2F" },
    { "honeydew",             "#F0FFF0" },
    { "hotpink",              "#FF69B4" },
    { "indianred",            "#CD5C5C" },
    { "indigo",               "#4B0082" },
    { "ivory",                "#FFFFF0" },
    { "khaki",                "#F0E68C" },
    { "lavender",             "#E6E6FA" },
    { "lavenderblush",        "#FFF0F5" },
    { "lawngreen",            "#7CFC00" },
    { "lemonchiffon",         "#FFFACD" },
    { "lightblue",            "#ADD8E6" },
    { "lightcoral",           "#F08080" },
    { "lightcyan",            "#E0FFFF" },
    { "lightgoldenrodyellow", "#FAFAD2" },
    { "lightgreen",           "#90EE90" },
    { "lightgrey",            "#D3D3D3" },
    { "lightpink",            "#FFB6C1" },
    { "lightsalmon",          "#FFA07A" },
    { "lightseagreen",        "#20B2AA" },
    { "lightskyblue",         "#87CEFA" },
    { "lightslategray",       "#778899" },
    { "lightsteelblue",       "#B0C4DE" },
    { "lightyellow",          "#FFFFE0" },
    { "lime",                 "#00FF00" },
    { "limegreen",            "#32CD32" },
    { "linen",                "#FAF0E6" },
    { "magenta",              "#FF00FF" },
    { "maroon",               "#800000" },
    { "mediumaquamarine",     "#66CDAA" },
    { "mediumblue",           "#0000CD" },
    { "mediumorchid",         "#BA55D3" },
    { "mediumpurple",         "#9370DB" },
    { "mediumseagreen",       "#3CB371" },
    { "mediumslateblue",      "#7B68EE" },
    { "mediumspringgreen",    "#00FA9A" },
    { "mediumturquoise",      "#48D1CC" },
    { "mediumvioletred",      "#C71585" },
    { "midnightblue",         "#191970" },
    { "mintcream",            "#F5FFFA" },
    { "mistyrose",            "#FFE4E1" },
    { "moccasin",             "#FFE4B5" },
    { "navajowhite",          "#FFDEAD" },
    { "navy",                 "#000080" },
    { "oldlace",              "#FDF5E6" },
    { "olive",                "#808000" },
    { "olivedrab",            "#6B8E23" },
    { "orange",               "#FFA500" },
    { "orangered",            "#FF4500" },
    { "orchid",               "#DA70D6" },
    { "palegoldenrod",        "#EEE8AA" },
    { "palegreen",            "#98FB98" },
    { "paleturquoise",        "#AFEEEE" },
    { "palevioletred",        "#DB7093" },
    { "papayawhip",           "#FFEFD5" },
    { "peachpuff",            "#FFDAB9" },
    { "peru",                 "#CD853F" },
    { "pink",                 "#FFC0CB" },
    { "plum",                 "#DDA0DD" },
    { "powderblue",           "#B0E0E6" },
    { "purple",               "#800080" },
    { "red",                  "#FF0000" },
    { "rosybrown",            "#BC8F8F" },
    { "royalblue",            "#4169E1" },
    { "saddlebrown",          "#8B4513" },
    { "salmon",               "#FA8072" },
    { "sandybrown",           "#F4A460" },
    { "seagreen",             "#2E8B57" },
    { "seashell",             "#FFF5EE" },
    { "sienna",               "#A0522D" },
    { "silver",               "#C0C0C0" },
    { "skyblue",              "#87CEEB" },
    { "slateblue",            "#6A5ACD" },
    { "slategray",            "#708090" },
    { "snow",                 "#FFFAFA" },
    { "springgreen",          "#00FF7F" },
    { "steelblue",            "#4682B4" },
    { "tan",                  "#D2B48C" },
    { "teal",                 "#008080" },
    { "thistle",              "#D8BFD8" },
    { "tomato",               "#FF6347" },
    { "turquoise",            "#40E0D0" },
    { "violet",               "#EE82EE" },
    { "wheat",                "#F5DEB3" },
    { "white",                "#FFFFFF" },
    { "whitesmoke",           "#F5F5F5" },
    { "yellow",               "#FFFF00" },
    { "yellowgreen",          "#9ACD32" },
    { NULL,                   NULL      }
};

#ifdef ATTRIBUTE_HASH_LOOKUP
static uint hash(ctmbstr s)
{
    uint hashval;

    for (hashval = 0; *s != '\0'; s++)
        hashval = *s + 31*hashval;

    return hashval % ATTRIBUTE_HASH_SIZE;
}

static Attribute *install(TidyAttribImpl * attribs, const Attribute* old)
{
    Attribute *np;
    uint hashval;

    np = (Attribute *)MemAlloc(sizeof(*np));

    np->name = tmbstrdup(old->name);

    hashval = hash(np->name);
    np->next = attribs->hashtab[hashval];
    attribs->hashtab[hashval] = np;

    np->id       = old->id;
    np->versions = old->versions;
    np->attrchk  = old->attrchk;
    np->nowrap   = old->nowrap;
    np->literal  = old->literal;

    return np;
}
#endif

static const Attribute* lookup(TidyAttribImpl* attribs, ctmbstr atnam)
{
    const Attribute *np;

    if (!atnam)
        return NULL;

#ifdef ATTRIBUTE_HASH_LOOKUP
    for (np = attribs->hashtab[hash(atnam)]; np != NULL; np = np->next)
        if (tmbstrcmp(atnam, np->name) == 0)
            return np;

    for (np = attribute_defs; np && np->name; ++np)
        if (tmbstrcmp(atnam, np->name) == 0)
            return install(attribs, np);
#else
    for (np = attribute_defs; np && np->name; ++np)
        if (tmbstrcmp(atnam, np->name) == 0)
            return np;
#endif

    return NULL;
}


/* Locate attributes by type */
AttVal* AttrGetById( Node* node, TidyAttrId id )
{
   AttVal* av;
   for ( av = node->attributes; av; av = av->next )
   {
     if ( AttrIsId(av, id) )
         return av;
   }
   return NULL;
}

/* public method for finding attribute definition by name */
const Attribute* FindAttribute( TidyDocImpl* doc, AttVal *attval )
{
    if ( attval )
       return lookup( &doc->attribs, attval->attribute );
    return NULL;
}

AttVal* GetAttrByName( Node *node, ctmbstr name )
{
    AttVal *attr;
    for (attr = node->attributes; attr != NULL; attr = attr->next)
    {
        if (attr->attribute && tmbstrcmp(attr->attribute, name) == 0)
            break;
    }
    return attr;
}

AttVal* AddAttribute( TidyDocImpl* doc,
                      Node *node, ctmbstr name, ctmbstr value )
{
    AttVal *av = NewAttribute();
    av->delim = '"';
    av->attribute = tmbstrdup( name );
    av->value = tmbstrdup( value );
    av->dict = lookup( &doc->attribs, name );

    if ( node->attributes == NULL )
        node->attributes = av;
    else /* append to end of attributes */
    {
        AttVal *here = node->attributes;
        while (here->next)
            here = here->next;
        here->next = av;
    }
    return av;
}

AttVal* RepairAttrValue(TidyDocImpl* doc, Node* node, ctmbstr name, ctmbstr value)
{
    AttVal* old = GetAttrByName(node, name);

    if (old)
    {
        if (old->value)
            MemFree(old->value);
        old->value = tmbstrdup(value);
        return old;
    }
    else
        return AddAttribute(doc, node, name, value);
}

static Bool CheckAttrType( TidyDocImpl* doc,
                           ctmbstr attrname, AttrCheck type )
{
    const Attribute* np = lookup( &doc->attribs, attrname );
    return (Bool)( np && np->attrchk == type );
}

Bool IsUrl( TidyDocImpl* doc, ctmbstr attrname )
{
    return CheckAttrType( doc, attrname, URL );
}

Bool IsBool( TidyDocImpl* doc, ctmbstr attrname )
{
    return CheckAttrType( doc, attrname, BOOL );
}

Bool IsScript( TidyDocImpl* doc, ctmbstr attrname )
{
    return CheckAttrType( doc, attrname, SCRIPT );
}

Bool IsLiteralAttribute( TidyDocImpl* doc, ctmbstr attrname )
{
    const Attribute* np = lookup( &doc->attribs, attrname );
    return (Bool)( np && np->literal );
}

/* may id or name serve as anchor? */
Bool IsAnchorElement( TidyDocImpl* doc, Node *node)
{
    TidyTagImpl* tags = &doc->tags;
    TidyTagId tid = TagId( node );
    if ( tid == TidyTag_A      ||
         tid == TidyTag_APPLET ||
         tid == TidyTag_FORM   ||
         tid == TidyTag_FRAME  ||
         tid == TidyTag_IFRAME ||
         tid == TidyTag_IMG    ||
         tid == TidyTag_MAP )
        return yes;

    return no;
}

/*
  In CSS1, selectors can contain only the characters A-Z, 0-9,
  and Unicode characters 161-255, plus dash (-); they cannot start
  with a dash or a digit; they can also contain escaped characters
  and any Unicode character as a numeric code (see next item).

  The backslash followed by at most four hexadecimal digits
  (0..9A..F) stands for the Unicode character with that number.

  Any character except a hexadecimal digit can be escaped to remove
  its special meaning, by putting a backslash in front.

  #508936 - CSS class naming for -clean option
*/
Bool IsCSS1Selector( ctmbstr buf )
{
    Bool valid = yes;
    int esclen = 0;
    byte c;
    int pos;

    for ( pos=0; valid && (c = *buf++); ++pos )
    {
        if ( c == '\\' )
        {
            esclen = 1;  /* ab\555\444 is 4 chars {'a', 'b', \555, \444} */
        }
        else if ( isdigit( c ) )
        {
            /* Digit not 1st, unless escaped (Max length "\112F") */
            if ( esclen > 0 )
                valid = ( ++esclen < 6 );
            if ( valid )
                valid = ( pos>0 || esclen>0 );
        }
        else
        {
            valid = (
                esclen > 0                       /* Escaped? Anything goes. */
                || ( pos>0 && c == '-' )         /* Dash cannot be 1st char */
                || isalpha(c)                    /* a-z, A-Z anywhere */
                || ( c >= 161 && c <= 255 )      /* Unicode 161-255 anywhere */
            );
            esclen = 0;
        }
    }
    return valid;
}

/* free single anchor */
static void FreeAnchor(Anchor *a)
{
    if ( a )
        MemFree( a->name );
    MemFree( a );
}

/* removes anchor for specific node */
void RemoveAnchorByNode( TidyDocImpl* doc, Node *node )
{
    TidyAttribImpl* attribs = &doc->attribs;
    Anchor *delme = NULL, *curr, *prev = NULL;

    for ( curr=attribs->anchor_list; curr!=NULL; curr=curr->next )
    {
        if ( curr->node == node )
        {
            if ( prev )
                prev->next = curr->next;
            else
                attribs->anchor_list = curr->next;
            delme = curr;
            break;
        }
        prev = curr;
    }
    FreeAnchor( delme );
}

/* initialize new anchor */
static Anchor* NewAnchor( ctmbstr name, Node* node )
{
    Anchor *a = (Anchor*) MemAlloc( sizeof(Anchor) );

    a->name = tmbstrdup( name );
    a->name = tmbstrtolower(a->name);
    a->node = node;
    a->next = NULL;

    return a;
}

/* add new anchor to namespace */
Anchor* AddAnchor( TidyDocImpl* doc, ctmbstr name, Node *node )
{
    TidyAttribImpl* attribs = &doc->attribs;
    Anchor *a = NewAnchor( name, node );

    if ( attribs->anchor_list == NULL)
         attribs->anchor_list = a;
    else
    {
        Anchor *here =  attribs->anchor_list;
        while (here->next)
            here = here->next;
        here->next = a;
    }

    return attribs->anchor_list;
}

/* return node associated with anchor */
Node* GetNodeByAnchor( TidyDocImpl* doc, ctmbstr name )
{
    TidyAttribImpl* attribs = &doc->attribs;
    Anchor *found;
    for ( found = attribs->anchor_list; found != NULL; found = found->next )
    {
        if ( tmbstrcmp(found->name, name) == 0 )
            break;
    }
    
    if ( found )
        return found->node;
    return NULL;
}

/* free all anchors */
void FreeAnchors( TidyDocImpl* doc )
{
    TidyAttribImpl* attribs = &doc->attribs;
    Anchor* a;
    while ( a = attribs->anchor_list )
    {
        attribs->anchor_list = a->next;
        MemFree( a->name );
        MemFree( a );
    }
}

/* public method for inititializing attribute dictionary */
void InitAttrs( TidyDocImpl* doc )
{
    ClearMemory( &doc->attribs, sizeof(TidyAttribImpl) );
#ifdef _DEBUG
    {
      /* Attribute ID is index position in Attribute type lookup table */
      uint ix;
      for ( ix=0; ix < N_TIDY_ATTRIBS; ++ix )
      {
        const Attribute* dict = &attribute_defs[ ix ];
        assert( (uint) dict->id == ix );
      }
    }
#endif
}

/*
Henry Zrepa reports that some folk are
using embed with script attributes where
newlines are signficant. These need to be
declared and handled specially!
*/
static void DeclareAttribute( TidyDocImpl* doc, ctmbstr name,
                              uint versions, Bool nowrap, Bool isliteral )
{
    const Attribute *exist = lookup( &doc->attribs, name );
    if ( exist == NULL )
    {
        TidyAttribImpl* attribs = &doc->attribs;
        Attribute* dict = (Attribute*) MemAlloc( sizeof(Attribute) );
        ClearMemory( dict, sizeof(Attribute) );

        dict->name     = tmbstrdup( name );
        dict->versions = versions;
        dict->nowrap   = nowrap;
        dict->literal  = isliteral;

        dict->next = attribs->declared_attr_list;
        attribs->declared_attr_list = dict;
    }
}

/* free all declared attributes */
static void FreeDeclaredAttributes( TidyDocImpl* doc )
{
    TidyAttribImpl* attribs = &doc->attribs;
    Attribute* dict;
    while ( dict = attribs->declared_attr_list )
    {
        attribs->declared_attr_list = dict->next;
        MemFree( dict->name );
        MemFree( dict );
    }
}

void DeclareLiteralAttrib( TidyDocImpl* doc, ctmbstr name )
{
    DeclareAttribute( doc, name, VERS_PROPRIETARY, no, yes );
}

void FreeAttrTable( TidyDocImpl* doc )
{
#ifdef ATTRIBUTE_HASH_LOOKUP
    Attribute *dict, *next;
    uint i;

    for (i = 0; i < ATTRIBUTE_HASH_SIZE; ++i)
    {
        dict = doc->attribs.hashtab[i];

        while(dict)
        {
            next = dict->next;
            MemFree(dict->name);
            MemFree(dict);
            dict = next;
        }

        doc->attribs.hashtab[i] = NULL;
    }
#endif

    FreeAnchors( doc );
    FreeDeclaredAttributes( doc );
}

/*
 the same attribute name can't be used
 more than once in each element
*/
void RepairDuplicateAttributes( TidyDocImpl* doc, Node *node)
{
    AttVal *first;

    for (first = node->attributes; first != NULL;)
    {
        AttVal *second;

        if (!(first->asp == NULL && first->php == NULL))
        {
            first = first->next;
            continue;
        }

        for (second = first->next; second != NULL;)
        {
            AttVal *temp;

            if (!(second->asp == NULL && second->php == NULL &&
                AttrsHaveSameId(first, second)))
            {
                second = second->next;
                continue;
            }

            /* first and second attribute have same local name */
            /* now determine what to do with this duplicate... */

            if (attrIsCLASS(first) && cfgBool(doc, TidyJoinClasses))
            {
                /* concatenate classes */

                first->value = (tmbstr) MemRealloc(first->value, tmbstrlen(first->value) +
                    tmbstrlen(second->value)  + 2);
                tmbstrcat(first->value, " ");
                tmbstrcat(first->value, second->value);

                temp = second->next;

                ReportAttrError( doc, node, second, JOINING_ATTRIBUTE);
                RemoveAttribute(node, second);

                second = temp;
            }
            else if (attrIsSTYLE(first) && cfgBool(doc, TidyJoinStyles))
            {
                /* concatenate styles */

                /*
                this doesn't handle CSS comments and
                leading/trailing white-space very well
                see http://www.w3.org/TR/css-style-attr
                */

                uint end = tmbstrlen(first->value);

                if (first->value[end - 1] == ';')
                {
                    /* attribute ends with declaration seperator */

                    first->value = (tmbstr) MemRealloc(first->value,
                        end + tmbstrlen(second->value) + 2);

                    tmbstrcat(first->value, " ");
                    tmbstrcat(first->value, second->value);
                }
                else if (first->value[end - 1] == '}')
                {
                    /* attribute ends with rule set */

                    first->value = (tmbstr) MemRealloc(first->value,
                        end + tmbstrlen(second->value) + 6);

                    tmbstrcat(first->value, " { ");
                    tmbstrcat(first->value, second->value);
                    tmbstrcat(first->value, " }");
                }
                else
                {
                    /* attribute ends with property value */

                    first->value = (tmbstr) MemRealloc(first->value,
                        end + tmbstrlen(second->value) + 3);

                    tmbstrcat(first->value, "; ");
                    tmbstrcat(first->value, second->value);
                }

                temp = second->next;

                ReportAttrError( doc, node, second, JOINING_ATTRIBUTE);

                RemoveAttribute(node, second);
                second = temp;

            }
            else if ( cfg(doc, TidyDuplicateAttrs) == TidyKeepLast )
            {
                temp = first->next;
                ReportAttrError( doc, node, first, REPEATED_ATTRIBUTE);
                RemoveAttribute(node, first);
                first = temp;
                second = second->next;
            }
            else /* TidyDuplicateAttrs == TidyKeepFirst */
            {
                temp = second->next;

                ReportAttrError( doc, node, second, REPEATED_ATTRIBUTE);
                RemoveAttribute(node, second);

                second = temp;
            }
        }
        first = first->next;
    }
}

/* ignore unknown attributes for proprietary elements */
const Attribute* CheckAttribute( TidyDocImpl* doc, Node *node, AttVal *attval )
{
    const Attribute* attribute = attval->dict;

    if ( attribute != NULL )
    {
        /* if attribute looks like <foo/> check XML is ok */
        if ( attribute->versions & VERS_XML )
        {
            if ( !(cfgBool(doc, TidyXmlTags) || cfgBool(doc, TidyXmlOut)) )
                ReportAttrError( doc, node, attval, XML_ATTRIBUTE_VALUE);
        }
        else
        {
            ConstrainVersion(doc, AttributeVersions(node, attval));
        }
        
        if (attribute->attrchk)
            attribute->attrchk( doc, node, attval );
        else if ( attval->dict->versions & VERS_PROPRIETARY )
            ReportAttrError( doc, node, attval, PROPRIETARY_ATTRIBUTE);
    }
    else if ( !cfgBool(doc, TidyXmlTags)
              && attval->asp == NULL 
              && node->tag != NULL 
              && !(node->tag->versions & VERS_PROPRIETARY) )
    {
        ReportAttrError( doc, node, attval, UNKNOWN_ATTRIBUTE );
    }

    return attribute;
}

Bool IsBoolAttribute(AttVal *attval)
{
    const Attribute *attribute = ( attval ? attval->dict : NULL );
    if ( attribute && attribute->attrchk == CheckBool )
        return yes;
    return no;
}

Bool attrIsEvent( AttVal* attval )
{
    TidyAttrId atid = AttrId( attval );

    return (atid == TidyAttr_OnAFTERUPDATE     ||
            atid == TidyAttr_OnBEFOREUNLOAD    ||
            atid == TidyAttr_OnBEFOREUPDATE    ||
            atid == TidyAttr_OnBLUR            ||
            atid == TidyAttr_OnCHANGE          ||
            atid == TidyAttr_OnCLICK           ||
            atid == TidyAttr_OnDATAAVAILABLE   ||
            atid == TidyAttr_OnDATASETCHANGED  ||
            atid == TidyAttr_OnDATASETCOMPLETE ||
            atid == TidyAttr_OnDBLCLICK        ||
            atid == TidyAttr_OnERRORUPDATE     ||
            atid == TidyAttr_OnFOCUS           ||
            atid == TidyAttr_OnKEYDOWN         ||
            atid == TidyAttr_OnKEYPRESS        ||
            atid == TidyAttr_OnKEYUP           ||
            atid == TidyAttr_OnLOAD            ||
            atid == TidyAttr_OnMOUSEDOWN       ||
            atid == TidyAttr_OnMOUSEMOVE       ||
            atid == TidyAttr_OnMOUSEOUT        ||
            atid == TidyAttr_OnMOUSEOVER       ||
            atid == TidyAttr_OnMOUSEUP         ||
            atid == TidyAttr_OnRESET           ||
            atid == TidyAttr_OnROWENTER        ||
            atid == TidyAttr_OnROWEXIT         ||
            atid == TidyAttr_OnSELECT          ||
            atid == TidyAttr_OnSUBMIT          ||
            atid == TidyAttr_OnUNLOAD);
}

static void CheckLowerCaseAttrValue( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    tmbstr p;
    Bool hasUpper = no;
    
    if (!AttrHasValue(attval))
        return;

    p = attval->value;
    
    while (*p)
    {
        if (IsUpper(*p)) /* #501230 - fix by Terry Teague - 09 Jan 02 */
        {
            hasUpper = yes;
            break;
        }
        p++;
    }

    if (hasUpper)
    {
        Lexer* lexer = doc->lexer;
        if (lexer->isvoyager)
            ReportAttrError( doc, node, attval, ATTR_VALUE_NOT_LCASE);
  
        if ( lexer->isvoyager || cfgBool(doc, TidyLowerLiterals) )
            attval->value = tmbstrtolower(attval->value);
    }
}

/* methods for checking value of a specific attribute */

void CheckUrl( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    tmbchar c; 
    tmbstr dest, p;
    uint escape_count = 0, backslash_count = 0;
    uint i, pos = 0;
    uint len;
    
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    p = attval->value;
    
    for (i = 0; c = p[i]; ++i)
    {
        if (c == '\\')
        {
            ++backslash_count;
            if ( cfgBool(doc, TidyFixBackslash) )
                p[i] = '/';
        }
        else if ((c > 0x7e) || (c <= 0x20) || (strchr("<>", c)))
            ++escape_count;
    }
    
    if ( cfgBool(doc, TidyFixUri) && escape_count )
    {
        len = tmbstrlen(p) + escape_count * 2 + 1;
        dest = (tmbstr) MemAlloc(len);
        
        for (i = 0; c = p[i]; ++i)
        {
            if ((c > 0x7e) || (c <= 0x20) || (strchr("<>", c)))
                pos += sprintf( dest + pos, "%%%02X", (byte)c );
            else
                dest[pos++] = c;
        }
        dest[pos++] = 0;

        MemFree(attval->value);
        attval->value = dest;
    }
    if ( backslash_count )
    {
        if ( cfgBool(doc, TidyFixBackslash) )
            ReportAttrError( doc, node, attval, FIXED_BACKSLASH );
        else
            ReportAttrError( doc, node, attval, BACKSLASH_IN_URI );
    }
    if ( escape_count )
    {
        if ( cfgBool(doc, TidyFixUri) )
            ReportAttrError( doc, node, attval, ESCAPED_ILLEGAL_URI);
        else
            ReportAttrError( doc, node, attval, ILLEGAL_URI_REFERENCE);

        doc->badChars |= INVALID_URI;
    }
}

void CheckScript( TidyDocImpl* doc, Node *node, AttVal *attval)
{
}

Bool IsValidHTMLID(tmbstr id)
{
    tmbstr s = id;

    if (!s)
        return no;

    if (!IsLetter(*s++))
        return no;

    while (s)
        if (!IsNamechar(*s++))
            return no;

    return yes;

}

Bool IsValidXMLID(tmbstr id)
{
    tmbstr s = id;
    tchar c;

    if (!s)
        return no;

    c = *s++;
    if (c > 0x7F)
        s += GetUTF8(s, &c);

    if (!(IsXMLLetter(c) || c == '_' || c == ':'))
        return no;

    while (*s)
    {
        c = (unsigned char)*s;

        if (c > 0x7F)
            s += GetUTF8(s, &c);

        ++s;

        if (!IsXMLNamechar(c))
            return no;
    }

    return yes;
}

Bool IsValidNMTOKEN(tmbstr name)
{
    tmbstr s = name;
    tchar c;

    if (!s)
        return no;

    while (*s)
    {
        c = (unsigned char)*s;

        if (c > 0x7F)
            s += GetUTF8(s, &c);

        ++s;

        if (!IsXMLNamechar(c))
            return no;
    }

    return yes;
}

void CheckName( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    Node *old;

    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    if ( IsAnchorElement(doc, node) )
    {
        if (doc->lexer->isvoyager && !IsValidNMTOKEN(attval->value))
            ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);

        if ((old = GetNodeByAnchor(doc, attval->value)) &&  old != node)
        {
            ReportAttrError( doc, node, attval, ANCHOR_NOT_UNIQUE);
        }
        else
            AddAnchor( doc, attval->value, node );
    }
}

void CheckId( TidyDocImpl* doc, Node *node, AttVal *attval )
{
    Lexer* lexer = doc->lexer;
    Node *old;

    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    if (!IsValidHTMLID(attval->value))
    {
        if (lexer->isvoyager && IsValidXMLID(attval->value))
            ReportAttrError( doc, node, attval, XML_ID_SYNTAX);
        else
            ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
    }

    if ((old = GetNodeByAnchor(doc, attval->value)) &&  old != node)
    {
        ReportAttrError( doc, node, attval, ANCHOR_NOT_UNIQUE);
    }
    else
        AddAnchor( doc, attval->value, node );
}

void CheckBool( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (!AttrHasValue(attval))
        return;

    CheckLowerCaseAttrValue( doc, node, attval );
}

void CheckAlign( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    /* IMG, OBJECT, APPLET and EMBED use align for vertical position */
    if (node->tag && (node->tag->model & CM_IMG))
    {
        CheckValign( doc, node, attval );
        return;
    }

    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval);

    if (!(AttrValueIs(attval, "left")   ||
          AttrValueIs(attval, "right")  ||
          AttrValueIs(attval, "center") ||
          AttrValueIs(attval, "justify")))
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

void CheckValign( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval );

    if (AttrValueIs(attval, "top")    ||
        AttrValueIs(attval, "middle") ||
        AttrValueIs(attval, "bottom") ||
        AttrValueIs(attval, "baseline"))
    {
            /* all is fine */
    }
    else if (AttrValueIs(attval, "left") ||
             AttrValueIs(attval, "right"))
    {
        if (!(node->tag && (node->tag->model & CM_IMG)))
            ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
    }
    else if (AttrValueIs(attval, "texttop")   ||
             AttrValueIs(attval, "absmiddle") ||
             AttrValueIs(attval, "absbottom") ||
             AttrValueIs(attval, "textbottom"))
    {
        ConstrainVersion( doc, VERS_PROPRIETARY );
        ReportAttrError( doc, node, attval, PROPRIETARY_ATTR_VALUE);
    }
    else
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

void CheckLength( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    tmbstr p;
    
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    /* don't check for <col width=...> and <colgroup width=...> */
    if (attrIsWIDTH(attval) && (nodeIsCOL(node) || nodeIsCOLGROUP(node)))
        return;

    p = attval->value;
    
    if (!IsDigit(*p++))
    {
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
    }
    else
    {
        while (*p)
        {
            if (!IsDigit(*p) && *p != '%')
            {
                ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
                break;
            }
            ++p;
        }
    }
}

void CheckTarget( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    /* target names must begin with A-Za-z ... */
    if (IsLetter(attval->value[0]))
        return;

    /* or be one of _blank, _self, _parent and _top */
    if (!(AttrValueIs(attval, "_blank")  ||
          AttrValueIs(attval, "_self")   ||
          AttrValueIs(attval, "_parent") ||
          AttrValueIs(attval, "_top")))
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

void CheckFsubmit( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval);

    if (!(AttrValueIs(attval, "get") ||
          AttrValueIs(attval, "post")))
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

void CheckClear( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        if (attval->value == NULL)
            attval->value = tmbstrdup( "none" );
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval );
        
    if (!(AttrValueIs(attval, "none")  ||
          AttrValueIs(attval, "left")  ||
          AttrValueIs(attval, "right") ||
          AttrValueIs(attval, "all")))
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

void CheckShape( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval );

    if (!(AttrValueIs(attval, "rect")    ||
          AttrValueIs(attval, "default") ||
          AttrValueIs(attval, "circle")  ||
          AttrValueIs(attval, "poly")))
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

void CheckScope( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval);

    if (!(AttrValueIs(attval, "row")      ||
          AttrValueIs(attval, "rowgroup") ||
          AttrValueIs(attval, "col")      ||
          AttrValueIs(attval, "colgroup")))
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

void CheckNumber( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    tmbstr p;
    
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    /* don't check <frameset cols=... rows=...> */
    if ( nodeIsFRAMESET(node) &&
        (attrIsCOLS(attval) || attrIsROWS(attval)))
     return;

    p  = attval->value;
    
    /* font size may be preceded by + or - */
    if ( nodeIsFONT(node) && (*p == '+' || *p == '-') )
        ++p;

    while (*p)
    {
        if (!IsDigit(*p))
        {
            ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
            break;
        }
        ++p;
    }
}

/* check hexadecimal color value */
Bool IsValidColorCode(ctmbstr color)
{
    uint i;

    if (tmbstrlen(color) != 6)
        return no;

    /* check if valid hex digits and letters */
    for (i = 0; i < 6; i++)
        if (!IsDigit(color[i]) && !strchr("abcdef", ToLower(color[i])))
            return no;

    return yes;
}

/* check color syntax and beautify value by option */
void CheckColor( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    Bool valid = no;
    tmbstr given;

    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    given = attval->value;

    /* 727851 - add hash to hash-less color values */
    if (given[0] != '#' && (valid = IsValidColorCode(given)))
    {
        tmbstr cp;
        tmbstr s = NULL;

        cp = s = (tmbstr) MemAlloc(2 + tmbstrlen (given));
        *cp++ = '#';
        while(*cp++ = *given++);

        ReportAttrError(doc, node, attval, BAD_ATTRIBUTE_VALUE_REPLACED);

        MemFree(attval->value);
        given = attval->value = s;
    }

    if (!valid && given[0] == '#')
        valid = IsValidColorCode(given + 1);

    if (valid && given[0] == '#' && cfgBool(doc, TidyReplaceColor))
    {
        ctmbstr newName = GetColorName(given);

        if (newName)
        {
            MemFree(attval->value);
            given = attval->value = tmbstrdup(newName);
        }
    }

    /* if it is not a valid color code, it is a color name */
    if (!valid)
        valid = GetColorCode(given) != NULL;

    if (valid && given[0] == '#')
        attval->value = tmbstrtoupper(attval->value);
    else if (valid)
        attval->value = tmbstrtolower(attval->value);

    if (!valid)
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

/* check valuetype attribute for element param */
void CheckVType( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval );

    if (!(AttrValueIs(attval, "data")   ||
          AttrValueIs(attval, "object") ||
          AttrValueIs(attval, "ref")))
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

/* checks scrolling attribute */
void CheckScroll( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval );

    if (!(AttrValueIs(attval, "no")   ||
          AttrValueIs(attval, "auto") ||
          AttrValueIs(attval, "yes")))
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

/* checks dir attribute */
void CheckTextDir( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (!AttrHasValue(attval))
    {
        ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval);

    if (!(AttrValueIs(attval, "rtl") ||
          AttrValueIs(attval, "ltr")))
        ReportAttrError( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

/* checks lang and xml:lang attributes */
void CheckLang( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    /* empty xml:lang is allowed through XML 1.0 SE errata */
    if (!AttrHasValue(attval) && !attrIsXML_LANG(attval))
    {
        if ( cfg(doc, TidyAccessibilityCheckLevel) == 0 )
        {
            ReportAttrError( doc, node, attval, MISSING_ATTR_VALUE );
        }
        return;
    }
}
